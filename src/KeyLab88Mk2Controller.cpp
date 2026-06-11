#include "KeyLab88Mk2Controller.h"
#include "MainComponent.h"

namespace
{
// Substring match against the device's CoreMIDI display name — Arturia
// names this port "KeyLab mkII 88 DAW" (caps + spelling verified via
// CoreMIDI enumeration on the connected machine).
constexpr auto kDawPortHint = "KeyLab mkII 88 DAW";

// Mapping target: KeyLab 88 MkII with DAW Map = MCU.  This is what the
// device sends out of the box once the DAW chooser is set to MCU /
// Logic / Cubase / Live (any MCU flavour); only Save/Read/Write differ
// across flavours and we don't currently bind those.  Source of truth:
// reverse-engineered from the Cubase MIDI Remote script "Arturia
// Keylab MK2 Custom" (Steinberg forum 847677), which declares the
// exact bindings — faders are pitch-bend per channel, encoders are
// CC 16-23 signed-bit relative, etc.  An older revision of this file
// targeted Analog-Lab-mode CCs, which the device does NOT emit in
// DAW=MCU mode; that's why every channel-strip control was dead.

// ── DAW + Transport notes (channel 1) — standard MCU ─────────────
constexpr uint8_t kNoteRecord   = 0x00;     //  0  per-track Record arm (8 buttons: 0..7)
constexpr uint8_t kNoteSolo     = 0x08;     //  8  per-track Solo (8..15)
constexpr uint8_t kNoteMute     = 0x10;     // 16  per-track Mute (16..23)
constexpr uint8_t kNoteSelect   = 0x18;     // 24  per-track Select (24..31) — buttons UNDER each fader
constexpr uint8_t kNoteTrackDn  = 0x2E;     // 46  Track ←  (single-step)
constexpr uint8_t kNoteTrackUp  = 0x2F;     // 47  Track →
constexpr uint8_t kNoteBankDn   = 0x30;     // 48  Bank   ← (8-track jump)
constexpr uint8_t kNoteBankUp   = 0x31;     // 49  Bank   →
constexpr uint8_t kNoteRead     = 0x4A;     // 74  Read / automation read (Cubase MCU)
constexpr uint8_t kNoteWrite    = 0x4B;     // 75  Write
constexpr uint8_t kNoteSave     = 0x50;     // 80  Save (Cubase) — Live MCU sends 0x4A; not currently distinguished
constexpr uint8_t kNoteUndo     = 0x51;     // 81  Undo
constexpr uint8_t kNoteCycle    = 0x56;     // 86  Loop / cycle
constexpr uint8_t kNotePunchIn  = 0x57;     // 87  Punch In
constexpr uint8_t kNotePunchOut = 0x58;     // 88  Punch Out
constexpr uint8_t kNoteMetro    = 0x59;     // 89  Metronome
constexpr uint8_t kNoteRewind   = 0x5B;     // 91  Rewind
constexpr uint8_t kNoteForward  = 0x5C;     // 92  Fast forward
constexpr uint8_t kNoteStop     = 0x5D;     // 93  Stop
constexpr uint8_t kNotePlay     = 0x5E;     // 94  Play
constexpr uint8_t kNoteRecXport = 0x5F;     // 95  Record (transport row)
constexpr uint8_t kNoteJogPush  = 0x54;     // 84  Big-knob push
constexpr uint8_t kNotePrevPage = 0x62;     // 98  ▲ above the cluster
constexpr uint8_t kNoteNextPage = 0x63;     // 99  ▼

// ── Pads (channel 10) — physical layout ──────────────────────────
// Top row notes 36-39, then 40-43, 44-47, 48-51 (bottom).  This is
// what the JS script binds (`bindToNote(9, 36 + 4*row + col)` —
// channel index 9 is MIDI channel 10).  Matches Arturia's pad numbering.
constexpr int kPadChannel = 10;

// ── CCs (channel 1) — standard MCU ───────────────────────────────
constexpr uint8_t kCcEncoderBase= 16;       // 0x10  encoders 1-8 → CCs 16..23
constexpr uint8_t kCcMasterEnc  = 24;       // 0x18  master/9th encoder
constexpr uint8_t kCcJogRot     = 60;       // 0x3C  big-knob rotation (signed-bit relative)
constexpr uint8_t kCcSustain    = 0x40;     // 64    pedal — forwards to plugins, not consumed

constexpr int padRowColToHostRowCol(int row, int col, int& outRow, int& outCol)
{
    outRow = row;  // (0,0) = top-left, matches host's session-view (0,0)
    outCol = col;
    return 0;
}
}

KeyLab88Mk2Controller::~KeyLab88Mk2Controller() { detach(); }

void KeyLab88Mk2Controller::attach(MainComponent* h)
{
    host = h;
    tryOpenInput();
}

void KeyLab88Mk2Controller::detach()
{
    if (dawInput) dawInput->stop();
    dawInput.reset();
    active = false;
}

void KeyLab88Mk2Controller::tryOpenInput()
{
    if (active && dawInput) return;
    for (auto& d : juce::MidiInput::getAvailableDevices())
    {
        if (d.name.contains(kDawPortHint))
        {
            auto in = juce::MidiInput::openDevice(d.identifier, this);
            if (in)
            {
                dawInput = std::move(in);
                dawInput->start();
                active = true;
                return;
            }
        }
    }
}

void KeyLab88Mk2Controller::tick()
{
    if (!active) tryOpenInput();
    if (!active) return;

    // Auto-scrub the playhead while the jog wheel is held past its limit.
    if (scrubAutoDir != 0 && host)
    {
        const auto now = juce::Time::currentTimeMillis();
        if (now < scrubAutoEndsAt)
            host->controllerScrubPlayhead(scrubAutoDir);
        else
            scrubAutoDir = 0;
    }
}

juce::String KeyLab88Mk2Controller::getLastMessages() const
{
    const juce::ScopedLock l(logLock);
    return recentLog.joinIntoString("\n");
}

void KeyLab88Mk2Controller::handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& msg)
{
    logMessage(msg);

    if (msg.isNoteOn())
    {
        // Drum pads arrive on channel 10 with the Analog Lab physical
        // layout: top row notes 48-51, then 44-47, 40-43, 36-39 (bottom).
        if (msg.getChannel() == kPadChannel)
        {
            const int n = msg.getNoteNumber();
            int row = -1, col = -1;
            if      (n >= 48 && n <= 51) { row = 0; col = n - 48; }
            else if (n >= 44 && n <= 47) { row = 1; col = n - 44; }
            else if (n >= 40 && n <= 43) { row = 2; col = n - 40; }
            else if (n >= 36 && n <= 39) { row = 3; col = n - 36; }
            if (row >= 0 && host) { host->controllerLaunchClipAt(row, col); return; }
        }
        handleNoteOn((uint8_t) msg.getNoteNumber(), (uint8_t) msg.getVelocity());
    }
    else if (msg.isNoteOff())          handleNoteOff((uint8_t) msg.getNoteNumber());
    else if (msg.isController())       handleCC     ((uint8_t) msg.getControllerNumber(),
                                                     (uint8_t) msg.getControllerValue());
    else if (msg.isPitchWheel())       handlePitchBend(msg.getChannel(), msg.getPitchWheelValue());
}

void KeyLab88Mk2Controller::handleNoteOn(uint8_t note, uint8_t velocity)
{
    if (velocity == 0) { handleNoteOff(note); return; }
    if (host == nullptr) return;

    // Per-track strip — eight buttons of each kind, base note + 0..7.
    if (note >= kNoteRecord && note < kNoteRecord + 8)
        { host->controllerTrackRecArm(note - kNoteRecord); return; }
    if (note >= kNoteSolo   && note < kNoteSolo + 8)
        { host->controllerTrackSolo  (note - kNoteSolo);   return; }
    if (note >= kNoteMute   && note < kNoteMute + 8)
        { host->controllerTrackMute  (note - kNoteMute);   return; }
    // Select buttons UNDER each fader — focus that track absolutely.
    if (note >= kNoteSelect && note < kNoteSelect + 8)
        { host->controllerFocusTrack(note - kNoteSelect);  return; }

    switch (note)
    {
        case kNotePlay:     host->controllerPlayToggle();                 break;
        case kNoteStop:     host->controllerStop();                       break;
        case kNoteRecXport: host->controllerRecordToggle();               break;
        case kNoteCycle:    host->controllerLoopToggle();                 break;
        case kNoteRewind:   host->controllerScrubPlayhead(-4);            break;
        case kNoteForward:  host->controllerScrubPlayhead(+4);            break;
        case kNoteSave:     host->controllerSaveProject();                break;
        case kNoteUndo:     host->controllerUndo();                       break;
        case kNoteMetro:    host->controllerToggleMetronomeAndCountIn(); break;
        case kNotePunchIn:  host->controllerSetLoopIn();                  break;
        case kNotePunchOut: host->controllerSetLoopOut();                 break;
        case kNoteRead:     /* TODO: automation read mode */              break;
        case kNoteWrite:    host->controllerSaveSnapshot(0);              break;
        // ── Bank / Track navigation ──
        // MCU emits these as standalone notes (no modifier required).
        case kNoteTrackDn:  host->controllerSelectTrack(-1);              break;
        case kNoteTrackUp:  host->controllerSelectTrack(+1);              break;
        case kNoteBankDn:   host->controllerSelectTrack(-8);              break;
        case kNoteBankUp:   host->controllerSelectTrack(+8);              break;
        // Big-knob push — stop transport (panic) for now.  Could be
        // repurposed to "Locators to Selection" later if useful.
        case kNoteJogPush:  host->controllerStop();                       break;
        case kNotePrevPage: host->controllerParamPagePrev();              break;
        case kNoteNextPage: host->controllerParamPageNext();              break;
        default: break;
    }
}

void KeyLab88Mk2Controller::handleNoteOff(uint8_t /*note*/) {}

void KeyLab88Mk2Controller::injectMessage(const juce::MidiMessage& msg)
{
    logMessage(msg);
    // Try MIDI-port handler first (CCs + channel-10 pads).
    if (processIncoming(msg)) return;
    // Otherwise treat as DAW-port traffic (notes for transport / DAW commands).
    handleIncomingMidiMessage(nullptr, msg);
}

bool KeyLab88Mk2Controller::processIncoming(const juce::MidiMessage& msg)
{
    if (host == nullptr) return false;

    // Channel-10 pads → clip launchers (consume).  Layout per the
    // Arturia Analog Lab default: top row notes 48-51, bottom row 36-39.
    if (msg.isNoteOn() && msg.getChannel() == kPadChannel)
    {
        const int n = msg.getNoteNumber();
        int row = -1, col = -1;
        if      (n >= 48 && n <= 51) { row = 0; col = n - 48; }
        else if (n >= 44 && n <= 47) { row = 1; col = n - 44; }
        else if (n >= 40 && n <= 43) { row = 2; col = n - 40; }
        else if (n >= 36 && n <= 39) { row = 3; col = n - 36; }
        if (row >= 0) { host->controllerLaunchClipAt(row, col); return true; }
    }
    if (msg.isNoteOff() && msg.getChannel() == kPadChannel)
        return true;   // swallow pad note-offs too

    // Faders 1-8 → pitch-bend channels 1-8.  Channel 9 is the master
    // fader and currently maps to focused-track volume too (no master
    // helper yet).  Both consumed so the keyboard's own pitch-bend
    // (channel 1) doesn't double-route — but the keyboard never sends
    // pitch-bend on the DAW port anyway, so consuming any pitch-bend
    // we see here is safe.
    if (msg.isPitchWheel())
    {
        handlePitchBend(msg.getChannel(), msg.getPitchWheelValue());
        return true;
    }

    // Note-ons that belong to the channel-strip + transport + bank /
    // track / jog-push / page nav set get consumed.  Everything else
    // (e.g. keyboard notes that somehow reached the DAW port) falls
    // through.
    if (msg.isNoteOn() || msg.isNoteOff())
    {
        const int n = msg.getNoteNumber();
        const bool inChannelStrip =
               (n >= kNoteRecord && n <= kNoteRecord + 7)    // rec arm
            || (n >= kNoteSolo   && n <= kNoteSolo   + 7)    // solo
            || (n >= kNoteMute   && n <= kNoteMute   + 7)    // mute
            || (n >= kNoteSelect && n <= kNoteSelect + 7);   // select
        const bool inNav =
               n == kNoteTrackDn || n == kNoteTrackUp
            || n == kNoteBankDn  || n == kNoteBankUp
            || n == kNoteJogPush
            || n == kNotePrevPage|| n == kNoteNextPage;
        const bool inTransport =
               n == kNotePlay   || n == kNoteStop      || n == kNoteRecXport
            || n == kNoteCycle  || n == kNoteRewind    || n == kNoteForward
            || n == kNoteSave   || n == kNoteUndo      || n == kNoteMetro
            || n == kNotePunchIn|| n == kNotePunchOut
            || n == kNoteRead   || n == kNoteWrite;
        if (inChannelStrip || inNav || inTransport)
        {
            if (msg.isNoteOn()) handleNoteOn ((uint8_t) n, (uint8_t) msg.getVelocity());
            else                handleNoteOff((uint8_t) n);
            return true;
        }
        return false;   // keyboard notes — let the plugin host see them
    }

    // CCs — consume only the ones we map (encoders 16-23, master 24,
    // jog rot 60).  Sustain (CC 64) and any unmapped CC fall through.
    // Log every unmapped CC except sustain so the on-screen inspector
    // surfaces what Cat/Preset/etc. actually emit on this firmware.
    if (msg.isController())
    {
        const int cc = msg.getControllerNumber();
        const bool isMapped =
               (cc >= kCcEncoderBase && cc <= kCcEncoderBase + 7)
            || cc == kCcMasterEnc
            || cc == kCcJogRot;
        if (isMapped)
        {
            handleCC((uint8_t) cc, (uint8_t) msg.getControllerValue());
            return true;
        }
        if (cc != kCcSustain)
            logMessage(msg);   // tagged in the log via raw-byte hex
    }
    return false;
}

void KeyLab88Mk2Controller::handleCC(uint8_t cc, uint8_t value)
{
    if (host == nullptr) return;

    // ── Big knob (jog) ──
    // MCU "signed-bit relative": bit 6 (0x40) = sign (1 = negative),
    // bits 0-5 = magnitude.  Same encoding the encoders use.
    if (cc == kCcJogRot)
    {
        const int8_t delta = (value & 0x40) ? -(int8_t)(value & 0x3f)
                                            :  (int8_t)(value & 0x3f);
        if (delta != 0)
        {
            host->controllerScrubPlayhead(delta);
            scrubAutoDir    = delta > 0 ? +1 : -1;
            scrubAutoEndsAt = juce::Time::currentTimeMillis() + 250;
        }
        return;
    }

    // ── Encoders 1-8 (CCs 16-23) ──
    if (cc >= kCcEncoderBase && cc <= kCcEncoderBase + 7)
    {
        const int idx = cc - kCcEncoderBase;
        const int8_t delta = (value & 0x40) ? -(int8_t)(value & 0x3f)
                                            :  (int8_t)(value & 0x3f);
        lastEncoderDelta[idx] = delta;
        host->controllerEncoderDelta(idx, delta);
        return;
    }

    // ── Master encoder (CC 24) — nudge focused-track volume ──
    if (cc == kCcMasterEnc)
    {
        const int8_t delta = (value & 0x40) ? -(int8_t)(value & 0x3f)
                                            :  (int8_t)(value & 0x3f);
        // 1/127 per click is a sensible default; can be tuned later.
        const float current = host->getFocusedTrackVolume();
        const float next = juce::jlimit(0.0f, 1.0f, current + delta * (1.0f / 127.0f));
        host->setFocusedTrackVolumeFromController(next);
        return;
    }
}

void KeyLab88Mk2Controller::handlePitchBend(int channel, int value14)
{
    if (host == nullptr) return;
    // Faders 1-8 → pitch-bend channels 1-8.  Channel index from JUCE
    // is 1-based, matching MCU's user-facing channel numbering.  Index
    // 8 (channel 9) is the master fader; route to focused-track volume
    // until a dedicated master helper exists.
    const int faderIdx = channel - 1;       // 0..7 = strip; 8 = master
    const float norm = juce::jlimit(0.0f, 1.0f,
        static_cast<float>(value14) / 16383.0f);
    if (faderIdx >= 0 && faderIdx <= 7)
        host->controllerFaderMove(faderIdx, norm);
    else if (faderIdx == 8)
        host->setFocusedTrackVolumeFromController(norm);
}

void KeyLab88Mk2Controller::logMessage(const juce::MidiMessage& msg)
{
    juce::String hex;
    const auto* data = msg.getRawData();
    for (int i = 0; i < msg.getRawDataSize(); ++i)
    {
        if (i > 0) hex << ' ';
        hex << juce::String::toHexString(data[i]).paddedLeft('0', 2).toUpperCase();
    }

    const juce::ScopedLock l(logLock);
    recentLog.insert(0, hex);
    if (recentLog.size() > 20) recentLog.remove(recentLog.size() - 1);
}
