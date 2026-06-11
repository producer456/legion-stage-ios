// Native integration for the IK Multimedia iRig Keys I/O 49 — combined
// USB-C audio interface + 8-knob / 8-pad / transport control surface.
//
// MIDI map sourced from the kmontag/iRift Ableton Live control script
// (https://github.com/kmontag/iRift) — the only reasonably complete
// reference for the device's protocol.  The audio-interface side is
// class-compliant USB Audio: JUCE's AudioDeviceManager picks it up on
// its own, no work here.
//
// Quirks worth remembering:
//   • Transport buttons send DIFFERENT CCs depending on the device's
//     own LED state machine (Stop = CC 117 if a transport LED is lit,
//     else CC 111 = "reset to zero"; Record = 119 to arm, 116 to
//     disarm).  We just hook all six to logical actions and the
//     state machine becomes invisible.
//   • Encoders use relative 2's-complement (NOT the MCU "signed-bit"
//     encoding the KL88 uses).  Different decoder.
//   • Pads have a non-sequential note layout (36/38/40/42/46/43/47/49)
//     and the user-configurable channel.  We default to channel 10
//     (matching KL88's clip-launch convention); the user sets the
//     device's PRE → PAD → CH to 10 once.
//   • The "data button" (encoder push) is sent as a CC, not a note.
#pragma once
#include <JuceHeader.h>

class MainComponent;

class IRigKeysIOController : private juce::MidiInputCallback
{
public:
    IRigKeysIOController() = default;
    ~IRigKeysIOController();

    void attach(MainComponent* host);
    void detach();

    /// Decode a MIDI message from the iRig MIDI port.  Returns true
    /// when the message was consumed (caller skips its normal dispatch).
    bool processIncoming(const juce::MidiMessage& msg);

    /// Periodic refresh — called ~30Hz from the MainComponent timer.
    /// Currently used only for hot-attach retries; the device drives
    /// its own LEDs and doesn't accept feedback.
    void tick();

    bool isActive() const { return active; }

    juce::String getLastMessages() const;

    /// Push a synthetic message through the same handler as a real one
    /// — used by the on-screen debug surface.
    void injectMessage(const juce::MidiMessage& msg);

private:
    void tryOpenInput();
    void appendLog(const juce::MidiMessage& msg);

    void handleEncoderDelta(int idx, int8_t signedDelta);   // idx 0..7
    void handleDataEncoder(int8_t signedDelta);
    void handleDataButton();
    void handlePad(int padIndex);                           // 0..7
    void handleTransportCC(uint8_t cc);

    /// MidiInputCallback — receives the device's events directly,
    /// bypassing MainComponent's single-input dropdown.
    void handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage&) override;

    MainComponent*                       host  = nullptr;
    std::unique_ptr<juce::MidiInput>     midiInput;
    bool                                 active = false;

    mutable juce::CriticalSection logLock;
    juce::StringArray             recentLog;
};
