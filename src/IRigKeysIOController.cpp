#include "IRigKeysIOController.h"
#include "MainComponent.h"

namespace IRig
{
    // Endpoint substring — iOS exposes the device as something like
    // "iRig Keys I/O 49".  Match loose so capitalization variants work.
    constexpr const char* kPortHint = "irig";

    // ── Transport CCs (channel 1) ─────────────────────────────────
    // The device sends DIFFERENT CCs depending on its LED state machine.
    // We hook all six and route to logical actions.
    constexpr uint8_t kCcReset       = 111;   // 0x6F  Stop pressed when both LEDs off
    constexpr uint8_t kCcFastForward = 114;   // 0x72  Alt + Stop
    constexpr uint8_t kCcRecordOff   = 116;   // 0x74  Record toggled OFF (was on)
    constexpr uint8_t kCcStop        = 117;   // 0x75  Stop pressed when a transport LED is lit
    constexpr uint8_t kCcPlay        = 118;   // 0x76  Play pressed
    constexpr uint8_t kCcRecordOn    = 119;   // 0x77  Record toggled ON

    // ── Encoders (channel 1) — relative 2's complement ────────────
    constexpr uint8_t kCcDataEncoder = 22;    // 0x16  clicky data wheel
    constexpr uint8_t kCcDataButton  = 23;    // 0x17  data wheel push (CC, not note!)
    constexpr uint8_t kCcEncoderBase = 12;    // 0x0C  encoders 1-8 → CCs 12..19

    // ── Pads — configurable channel; we expect channel 10 ─────────
    // Non-sequential note layout.  Index → note:
    constexpr uint8_t kPadNotes[8]   = { 36, 38, 40, 42, 46, 43, 47, 49 };
    constexpr int     kPadChannel    = 10;

    // Decode 7-bit two's-complement relative delta:
    //   0..63  → +0..+63
    //   64..127→ -64..-1
    inline int8_t decodeRel2c(uint8_t v)
    {
        return (v < 64) ? static_cast<int8_t>(v)
                        : static_cast<int8_t>(static_cast<int>(v) - 128);
    }
}

IRigKeysIOController::~IRigKeysIOController() { detach(); }

void IRigKeysIOController::attach(MainComponent* h)
{
    host = h;
    tryOpenInput();
    if (midiInput) active = true;
}

void IRigKeysIOController::tryOpenInput()
{
    if (midiInput) return;
    for (auto& dev : juce::MidiInput::getAvailableDevices())
    {
        if (dev.name.containsIgnoreCase(IRig::kPortHint))
        {
            midiInput = juce::MidiInput::openDevice(dev.identifier, this);
            if (midiInput) midiInput->start();
            return;
        }
    }
}

void IRigKeysIOController::detach()
{
    if (!active) return;
    if (midiInput) { midiInput->stop(); midiInput.reset(); }
    active = false;
}

void IRigKeysIOController::handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& msg)
{
    appendLog(msg);
    processIncoming(msg);
}

void IRigKeysIOController::injectMessage(const juce::MidiMessage& msg)
{
    appendLog(msg);
    processIncoming(msg);
}

void IRigKeysIOController::appendLog(const juce::MidiMessage& msg)
{
    juce::String line;
    for (int i = 0; i < msg.getRawDataSize(); ++i)
        line << juce::String::toHexString(msg.getRawData()[i])
                .paddedLeft('0', 2).toUpperCase() << " ";
    const juce::ScopedLock lk(logLock);
    recentLog.insert(0, line.trim());
    while (recentLog.size() > 12) recentLog.remove(12);
}

juce::String IRigKeysIOController::getLastMessages() const
{
    const juce::ScopedLock lk(logLock);
    return recentLog.joinIntoString("\n");
}

bool IRigKeysIOController::processIncoming(const juce::MidiMessage& msg)
{
    if (host == nullptr) return false;

    // Pads → clip launch row.  The device emits these on whichever
    // channel the user has configured PRE→PAD→CH to.  We only consume
    // notes that match the expected pad note set (so a stray keyboard
    // note that happens to be 36 still falls through to plugins on the
    // keyboard channel).
    if (msg.isNoteOn())
    {
        const int n = msg.getNoteNumber();
        for (int i = 0; i < 8; ++i)
        {
            if (IRig::kPadNotes[i] == n)
            {
                handlePad(i);
                return true;
            }
        }
        return false;   // keyboard note — let plugins see it
    }
    if (msg.isNoteOff())
    {
        const int n = msg.getNoteNumber();
        for (auto pn : IRig::kPadNotes)
            if (pn == n) return true;   // swallow pad note-offs
        return false;
    }

    if (msg.isController())
    {
        const int cc = msg.getControllerNumber();
        const int v  = msg.getControllerValue();

        // Encoders 1-8 (CCs 12..19)
        if (cc >= IRig::kCcEncoderBase && cc < IRig::kCcEncoderBase + 8)
        {
            handleEncoderDelta(cc - IRig::kCcEncoderBase,
                               IRig::decodeRel2c(static_cast<uint8_t>(v)));
            return true;
        }
        if (cc == IRig::kCcDataEncoder)
        {
            handleDataEncoder(IRig::decodeRel2c(static_cast<uint8_t>(v)));
            return true;
        }
        if (cc == IRig::kCcDataButton)
        {
            // Press only — the device sends value 127 on press and
            // doesn't always send a release.  Act on any non-zero.
            if (v > 0) handleDataButton();
            return true;
        }
        // Transport CCs — six different ones, all consumed.
        if (cc == IRig::kCcReset       || cc == IRig::kCcFastForward
         || cc == IRig::kCcRecordOff   || cc == IRig::kCcStop
         || cc == IRig::kCcPlay        || cc == IRig::kCcRecordOn)
        {
            // Transport CCs only fire once per press (no separate
            // release).  Some send value 127 on press; treat any
            // non-zero as the trigger.
            if (v > 0) handleTransportCC(static_cast<uint8_t>(cc));
            return true;
        }
    }

    return false;
}

// ── Handlers ───────────────────────────────────────────────────────

void IRigKeysIOController::handleEncoderDelta(int idx, int8_t signedDelta)
{
    if (host == nullptr || idx < 0 || idx > 7 || signedDelta == 0) return;
    // Match the LK Mini paradigm: encoders 0..7 nudge on-screen sliders
    // 0..7.  controllerEncoderDelta accepts a signed step count and the
    // host scales it per parameter.
    host->controllerEncoderDelta(idx, signedDelta);
}

void IRigKeysIOController::handleDataEncoder(int8_t signedDelta)
{
    if (host == nullptr || signedDelta == 0) return;
    // Data encoder = focused-track volume nudge.  1/127 per click feels
    // right; matches the KL88 master encoder.
    const float current = host->getFocusedTrackVolume();
    const float next = juce::jlimit(0.0f, 1.0f,
        current + signedDelta * (1.0f / 127.0f));
    host->setFocusedTrackVolumeFromController(next);
}

void IRigKeysIOController::handleDataButton()
{
    if (host == nullptr) return;
    // Data button push = reset focused-track volume to unity (0 dB ≈ 1.0).
    host->setFocusedTrackVolumeFromController(1.0f);
}

void IRigKeysIOController::handlePad(int padIndex)
{
    if (host == nullptr || padIndex < 0 || padIndex > 7) return;
    // Single row of 8 pads → first row of the session-view clip grid.
    // Matches the KL88 clip-launch convention so muscle memory carries
    // across devices.
    host->controllerLaunchClipAt(0, padIndex);
}

void IRigKeysIOController::handleTransportCC(uint8_t cc)
{
    if (host == nullptr) return;
    using namespace IRig;
    switch (cc)
    {
        case kCcPlay:        host->controllerPlayToggle();       break;
        // Both Stop variants do the same thing — Reset additionally
        // jumps to zero, which is what controllerStop() already does
        // when called twice in a row, so a single call is fine here.
        case kCcStop:        host->controllerStop();             break;
        case kCcReset:       host->controllerStop();             break;
        case kCcRecordOn:    host->controllerRecordToggle();     break;
        case kCcRecordOff:   host->controllerRecordToggle();     break;
        case kCcFastForward: host->controllerScrubPlayhead(+8);  break;
        default: break;
    }
}

void IRigKeysIOController::tick()
{
    if (host == nullptr) return;
    if (!active) tryOpenInput();
}
