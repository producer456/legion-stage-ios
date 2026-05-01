#include "KeyLab88Mk2Controller.h"
#include "MainComponent.h"

namespace
{
// Substring match against the device's CoreMIDI display name — Arturia
// names this port "KeyLab mkII 88 DAW" (caps + spelling verified via
// CoreMIDI enumeration on the connected machine).
constexpr auto kDawPortHint = "KeyLab mkII 88 DAW";

// Mapping target: KeyLab 88 MkII in "Analog Lab" mode with DAW Map = MCU.
// Per-track Solo/Mute/Rec on the strip is MCU-style (channel 1 notes
// 0x00/0x08/0x10 + slot index); the rest of the surface uses the
// Analog Lab MIDI codes.

// ── DAW + Transport notes (channel 1) — Analog Lab block ─────────
constexpr uint8_t kNoteRecord   = 0x00;     //  0  per-track Record (ch 1, +trk)
constexpr uint8_t kNoteSolo     = 0x08;     //  8  per-track Solo
constexpr uint8_t kNoteMute     = 0x10;     // 16  per-track Mute
constexpr uint8_t kNoteRead     = 0x4A;     // 74  Read / Arm-none
constexpr uint8_t kNoteWrite    = 0x4B;     // 75  Write / QuickSnapshot
constexpr uint8_t kNoteSave     = 0x50;     // 80  Save
constexpr uint8_t kNoteUndo     = 0x51;     // 81  Undo
constexpr uint8_t kNoteCycle    = 0x56;     // 86  Loop
constexpr uint8_t kNotePunchIn  = 0x57;     // 87  Punch In
constexpr uint8_t kNotePunchOut = 0x58;     // 88  Punch Out
constexpr uint8_t kNoteMetro    = 0x59;     // 89  Metronome
constexpr uint8_t kNoteRewind   = 0x5B;     // 91  Rewind
constexpr uint8_t kNoteForward  = 0x5C;     // 92  Fast forward
constexpr uint8_t kNoteStop     = 0x5D;     // 93  Stop
constexpr uint8_t kNotePlay     = 0x5E;     // 94  Play
constexpr uint8_t kNoteRecXport = 0x5F;     // 95  Record (transport row)

// ── Pads — Arturia Analog Lab physical layout (channel 10) ───────
// Top row notes 48-51, then 44-47, 40-43, 36-39 (bottom).  We map
// note→(row,col) so (0,0) is the top-left pad.
constexpr int kPadChannel = 10;

// ── Analog Lab cluster + bank cluster CCs (channel 1) ────────────
constexpr uint8_t kCcNextBank   = 0x16;     // 22  Part 1 / Next bank
constexpr uint8_t kCcPrevBank   = 0x17;     // 23  Part 2 / Prev bank
constexpr uint8_t kCcLiveBank   = 0x18;     // 24  Live / Bank toggle
constexpr uint8_t kCcLeftArrow  = 0x1C;     // 28
constexpr uint8_t kCcRightArrow = 0x1D;     // 29
constexpr uint8_t kCcSelectMux  = 0x1E;     // 30  Select toggles (multiplexed)
constexpr uint8_t kCcBigKnobRot = 0x70;     // 112 Big knob rotate (63=back, 65=fwd)
constexpr uint8_t kCcBigKnobPush= 0x71;     // 113 Big knob push (panic)
constexpr uint8_t kCcCategory   = 0x74;     // 116
constexpr uint8_t kCcPreset     = 0x75;     // 117
constexpr uint8_t kCcSustain    = 0x40;     // 64

// ── Encoders + Faders (channel 1) — Analog Lab scattered map ─────
// Index 0..7 = groups 1..8 (track strip).  Index 8 = master/group 9.
constexpr uint8_t kCcEncoder[9] = { 0x4A, 0x47, 0x4C, 0x4D, 0x5D, 0x12, 0x13, 0x10, 0x11 };
constexpr uint8_t kCcFader  [9] = { 0x49, 0x4B, 0x4F, 0x48, 0x50, 0x51, 0x52, 0x53, 0x55 };

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

    // Per-track strip — Solo/Mute/Rec follow the focused track in
    // single-track mode; in bank-of-8 mode the index identifies the
    // group (kept simple here: forward index directly to the host).
    if (note >= kNoteRecord && note < kNoteRecord + 8)
        { host->controllerTrackRecArm(note - kNoteRecord); return; }
    if (note >= kNoteSolo   && note < kNoteSolo + 8)
        { host->controllerTrackSolo  (note - kNoteSolo);   return; }
    if (note >= kNoteMute   && note < kNoteMute + 8)
        { host->controllerTrackMute  (note - kNoteMute);   return; }

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

    // Channel-10 pads → clip launchers (consume).
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

    // Control-surface CCs → consume.  Sustain (CC 64) is intentionally
    // NOT in this set — it forwards to plugins normally.
    if (msg.isController())
    {
        const int cc = msg.getControllerNumber();
        const bool isControlSurface =
               cc == kCcNextBank   || cc == kCcPrevBank   || cc == kCcLiveBank
            || cc == kCcLeftArrow  || cc == kCcRightArrow
            || cc == kCcSelectMux
            || cc == kCcBigKnobRot || cc == kCcBigKnobPush
            || cc == kCcCategory   || cc == kCcPreset;
        bool isEncoderOrFader = false;
        for (int i = 0; i < 9 && !isEncoderOrFader; ++i)
            if (cc == kCcEncoder[i] || cc == kCcFader[i]) isEncoderOrFader = true;

        if (isControlSurface || isEncoderOrFader)
        {
            logMessage(msg);
            handleCC((uint8_t) cc, (uint8_t) msg.getControllerValue());
            return true;
        }
    }
    return false;
}

void KeyLab88Mk2Controller::handleCC(uint8_t cc, uint8_t value)
{
    if (host == nullptr) return;

    // ── Big knob ──
    if (cc == kCcBigKnobRot)
    {
        // Relative: 63=back, 65=fwd; magnitude in the low bits.
        const int delta = (value < 0x40) ? +(value - 0x3f)
                                         : -(value - 0x40);
        if (delta != 0)
        {
            host->controllerScrubPlayhead(delta);
            scrubAutoDir    = delta > 0 ? +1 : -1;
            scrubAutoEndsAt = juce::Time::currentTimeMillis() + 250;
        }
        return;
    }
    if (cc == kCcBigKnobPush) { host->controllerStop(); return; }   // panic

    // ── Bank cluster ──
    if (cc == kCcNextBank) { host->controllerParamPageNext(); return; }
    if (cc == kCcPrevBank) { host->controllerParamPagePrev(); return; }
    if (cc == kCcLiveBank) { /* TODO: bank-toggle / unity-gain */  return; }

    // ── Analog Lab cluster ──
    if (cc == kCcLeftArrow)  { host->controllerSelectTrack(-1); return; }
    if (cc == kCcRightArrow) { host->controllerSelectTrack(+1); return; }
    if (cc == kCcCategory)   { host->controllerPresetPrev();    return; }
    if (cc == kCcPreset)     { host->controllerPresetNext();    return; }

    // ── Sustain pedal ── (already routed via the MIDI port to plugins;
    // logged here for debugging but no host action needed)
    if (cc == kCcSustain) return;

    // ── Select-button row (CC 30 multiplexed) ──
    // Press = odd value, release = even value. Track index = (value - 1) / 2.
    // Each select button focuses the track of the fader directly above it.
    if (cc == kCcSelectMux)
    {
        if (value & 0x01)
        {
            const int trackIdx = (value - 1) / 2;   // 0..8
            host->controllerFocusTrack(trackIdx);
        }
        return;
    }

    // ── Encoders (scattered CCs, relative deltas) ──
    for (int i = 0; i < 9; ++i)
    {
        if (cc != kCcEncoder[i]) continue;
        const int8_t delta = (value & 0x40) ? -(int8_t)(value & 0x3f)
                                            :  (int8_t)(value & 0x3f);
        lastEncoderDelta[i] = delta;
        if (i < 8) host->controllerEncoderDelta(i, delta);
        return;
    }

    // ── Faders (scattered CCs, absolute 0..127) ──
    for (int i = 0; i < 9; ++i)
    {
        if (cc != kCcFader[i]) continue;
        const float norm = juce::jlimit(0.0f, 1.0f, value / 127.0f);
        if (i < 8) host->controllerFaderMove(i, norm);
        // Master fader (i == 8) — no host method yet; ignore.
        return;
    }
}

void KeyLab88Mk2Controller::handlePitchBend(int /*channel*/, int /*value*/)
{
    // Analog Lab mode doesn't use pitch-bend for faders (those live on CCs
    // above).  Pitch-bend from the keyboard is forwarded via the MIDI port
    // directly to plugins — no host action here.
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
