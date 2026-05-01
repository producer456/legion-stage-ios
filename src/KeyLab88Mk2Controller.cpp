#include "KeyLab88Mk2Controller.h"
#include "MainComponent.h"

namespace
{
// Substring match against the device's CoreMIDI display name — Arturia
// names this port "KeyLab mkII 88 DAW" (caps + spelling verified via
// CoreMIDI enumeration on the connected machine).
constexpr auto kDawPortHint = "KeyLab mkII 88 DAW";

// MCU master / transport notes (channel 1).
constexpr uint8_t kNoteSave     = 0x4C;
constexpr uint8_t kNoteUndo     = 0x4D;
constexpr uint8_t kNoteCancel   = 0x4E;
constexpr uint8_t kNoteEnter    = 0x4F;
constexpr uint8_t kNoteCycle    = 0x52;     // Loop
constexpr uint8_t kNoteDrop     = 0x53;     // Punch in / record-arm shortcut
constexpr uint8_t kNoteClick    = 0x55;     // Metronome
constexpr uint8_t kNoteRewind   = 0x5B;
constexpr uint8_t kNoteForward  = 0x5C;
constexpr uint8_t kNoteStop     = 0x5D;
constexpr uint8_t kNotePlay     = 0x5E;
constexpr uint8_t kNoteRecord   = 0x5F;
constexpr uint8_t kNoteUp       = 0x60;
constexpr uint8_t kNoteDown     = 0x61;
constexpr uint8_t kNoteLeft     = 0x62;
constexpr uint8_t kNoteRight    = 0x63;

// Per-channel button rows (notes 0x00..0x1F).
constexpr uint8_t kNoteRecArmBase = 0x00;   // 0x00..0x07
constexpr uint8_t kNoteSoloBase   = 0x08;   // 0x08..0x0F
constexpr uint8_t kNoteMuteBase   = 0x10;   // 0x10..0x17
constexpr uint8_t kNoteSelectBase = 0x18;   // 0x18..0x1F

// Snapshot row — 5 buttons.  KL88 sends these on a contiguous note
// range starting at 0x36 in user-mode (educated guess; the live MIDI
// inspector will confirm and we'll iterate if needed).
constexpr uint8_t kNoteSnapshotBase = 0x36;
constexpr uint8_t kNoteSnapshotCount = 5;

// Tap tempo button.
constexpr uint8_t kNoteTapTempo = 0x65;

// Pad notes — Arturia drum pads sit at C1..D#2 on channel 10.
constexpr uint8_t kPadBaseNote   = 0x24;
constexpr int     kPadCount      = 16;
constexpr int     kPadChannel    = 10;

// MCU encoder CCs — 9 encoders in 0x10..0x18.  Values are
// relative deltas: 0x01..0x07 = forward, 0x41..0x47 = back.
constexpr uint8_t kEncoderBaseCc = 0x10;
constexpr int     kJogEncoderIdx = 8;     // rightmost encoder doubles as jog wheel
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
        // Drum pads arrive on a separate channel — split them out here
        // so they don't shadow the button-row note range (0x00..0x33).
        if (msg.getChannel() == kPadChannel)
        {
            const uint8_t n = (uint8_t) msg.getNoteNumber();
            if (n >= kPadBaseNote && n < kPadBaseNote + kPadCount)
            {
                const int idx = n - kPadBaseNote;
                const int row = idx / 4;     // 0..3
                const int col = idx % 4;     // 0..3
                if (host) host->controllerLaunchClipAt(row, col);
                return;
            }
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

    // Per-channel button rows.
    if (note >= kNoteRecArmBase && note < kNoteRecArmBase + 8)
        { host->controllerTrackRecArm(note - kNoteRecArmBase); return; }
    if (note >= kNoteSoloBase   && note < kNoteSoloBase + 8)
        { host->controllerTrackSolo  (note - kNoteSoloBase);   return; }
    if (note >= kNoteMuteBase   && note < kNoteMuteBase + 8)
        { host->controllerTrackMute  (note - kNoteMuteBase);   return; }
    if (note >= kNoteSelectBase && note < kNoteSelectBase + 8)
        { host->controllerTrackRecArm(note - kNoteSelectBase); return; }
    if (note >= kNoteSnapshotBase && note < kNoteSnapshotBase + kNoteSnapshotCount)
        { host->controllerLoadSnapshot(note - kNoteSnapshotBase); return; }

    // Master row.
    switch (note)
    {
        case kNotePlay:    host->controllerPlayToggle();                 break;
        case kNoteStop:    host->controllerStop();                       break;
        case kNoteRecord:  host->controllerRecordToggle();               break;
        case kNoteCycle:   host->controllerLoopToggle();                 break;
        case kNoteRewind:  host->controllerScrubPlayhead(-4);            break;
        case kNoteForward: host->controllerScrubPlayhead(+4);            break;
        case kNoteSave:    host->controllerSaveProject();                break;
        case kNoteUndo:    host->controllerUndo();                       break;
        case kNoteClick:   host->controllerToggleMetronomeAndCountIn(); break;
        case kNoteTapTempo:host->controllerTapTempo();                   break;
        case kNoteUp:      host->controllerCursorUp();                   break;
        case kNoteDown:    host->controllerCursorDown();                 break;
        case kNoteLeft:    host->controllerCursorLeft();                 break;
        case kNoteRight:   host->controllerCursorRight();                break;
        case kNoteEnter:   host->controllerLaunchScene();                break;
        case kNoteDrop:    host->controllerRecordToggle();               break;
        // Cancel / others — left unmapped intentionally.
        default: break;
    }
}

void KeyLab88Mk2Controller::handleNoteOff(uint8_t /*note*/) {}

void KeyLab88Mk2Controller::handleCC(uint8_t cc, uint8_t value)
{
    if (host == nullptr) return;

    // MCU-style encoders sit at 0x10..0x18.  Values 0x01..0x3f are
    // forward deltas; 0x41..0x7f are reverse (with magnitude in the
    // low 6 bits).
    if (cc >= kEncoderBaseCc && cc < kEncoderBaseCc + 9)
    {
        const int idx = cc - kEncoderBaseCc;
        const int8_t delta = (value & 0x40) ? -(int8_t)(value & 0x3f)
                                            :  (int8_t)(value & 0x3f);
        lastEncoderDelta[idx] = delta;

        if (idx == kJogEncoderIdx)
        {
            // Jog wheel → scrub the playhead.  Auto-extend so the
            // playhead keeps moving for ~250ms after the user lets go,
            // gives a smoother feel than per-click stepping.
            host->controllerScrubPlayhead(delta);
            scrubAutoDir = delta > 0 ? +1 : -1;
            scrubAutoEndsAt = juce::Time::currentTimeMillis() + 250;
        }
        else
        {
            // The first 8 encoders nudge the visible param sliders.
            host->controllerEncoderDelta(idx, delta);
        }
    }
}

void KeyLab88Mk2Controller::handlePitchBend(int channel, int value)
{
    // MCU faders — 9 channels of pitch bend, where channel 1..8 are the
    // mixer faders and channel 9 is the master.  value is 0..16383;
    // map to 0..1 for the host.
    if (host == nullptr) return;
    const int   trackIdx = juce::jlimit(0, 8, channel - 1);
    const float norm     = juce::jlimit(0.0f, 1.0f, value / 16383.0f);
    host->controllerFaderMove(trackIdx, norm);
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
