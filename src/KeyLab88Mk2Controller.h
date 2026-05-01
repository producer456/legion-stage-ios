// Native integration for the Arturia KeyLab 88 MkII.
//
// Owns the device's "DAW" CoreMIDI port and translates between Legion
// Stage's transport / mixer / param state and the device's faders /
// encoders / pads / transport buttons.  The "MIDI" port (88 keys +
// pitch/mod wheels + sustain pedal) is handled separately by the
// existing MIDI-in path.
//
// The KL88 MkII speaks an Arturia-flavoured MCU-ish protocol over its
// DAW port — most of the standard Mackie button-note + encoder-CC
// numbers apply, but the pads + the right-hand encoder/fader bank
// have device-specific assignments that we discover via the on-screen
// MIDI inspector.
#pragma once
#include <JuceHeader.h>

class MainComponent;

class KeyLab88Mk2Controller : private juce::MidiInputCallback
{
public:
    KeyLab88Mk2Controller() = default;
    ~KeyLab88Mk2Controller();

    /// Open the DAW input port if it's plugged in.  Idempotent —
    /// safe to call every tick while we wait for a hot-plug.
    void attach(MainComponent* host);
    void detach();

    /// Periodic LED / fader-feedback refresh.  Call ~30Hz from the
    /// MainComponent timer.
    void tick();

    /// True after the DAW input port has been opened successfully.
    bool isActive() const { return active; }

    /// Recent MIDI log (most-recent first, capped) for the on-screen
    /// inspector.  Same shape as the Launchkey controller's log.
    juce::String getLastMessages() const;

private:
    void tryOpenInput();
    void handleNoteOn(uint8_t note, uint8_t velocity);
    void handleNoteOff(uint8_t note);
    void handleCC(uint8_t cc, uint8_t value);
    void handlePitchBend(int channel, int value);

    /// MidiInputCallback — receives the DAW port's events directly.
    void handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage&) override;

    void logMessage(const juce::MidiMessage& msg);

    MainComponent* host = nullptr;
    std::unique_ptr<juce::MidiInput> dawInput;
    bool active = false;

    // MCU-style relative encoders send 0x40+delta or 0x00+delta on a
    // single CC.  Cache the last delta sign per encoder so we can do
    // velocity-aware param sweeps.
    int8_t lastEncoderDelta[9] = {0};

    // Auto-scrub state for the jog wheel (encoder index 8 in MCU).
    int8_t scrubAutoDir = 0;
    juce::int64 scrubAutoEndsAt = 0;

    mutable juce::CriticalSection logLock;
    juce::StringArray recentLog;
};
