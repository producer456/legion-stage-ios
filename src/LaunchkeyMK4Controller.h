// Native integration for the Novation Launchkey Mini 37 MK4.
//
// Owns the device's "DAW" port input + output and translates between
// Legion Stage's transport / session-view / mixer state and the
// device's pads / encoders / buttons / LEDs.  The "MIDI" port (keys +
// wheels + Modulation) is handled separately by the existing MIDI-in
// path that feeds the focused track's plugin host.
//
// Wire it up by:
//   1. Constructing one in MainComponent: `lkController.attach(this);`
//   2. Calling lkController.processIncoming(midiMessage) from
//      MainComponent::handleIncomingMidiMessage when the source is the
//      Launchkey's "DAW" endpoint.
//   3. Calling lkController.tick() from the existing 30Hz timer so
//      pad LEDs follow live clip state.
#pragma once
#include <JuceHeader.h>

class MainComponent;

class LaunchkeyMK4Controller
{
public:
    LaunchkeyMK4Controller() = default;
    ~LaunchkeyMK4Controller();

    /// Attach to the host app + send the DAW-mode handshake.  Safe to
    /// call repeatedly; idempotent.
    void attach(MainComponent* host);
    void detach();

    /// Process a MIDI message from the Launchkey's "DAW" port.  Returns
    /// true if the controller consumed it (caller should skip its
    /// normal MIDI dispatch path).
    bool processIncoming(const juce::MidiMessage& msg);

    /// Periodic LED refresh — paint pad colours from the current
    /// session-view clip states + transport-button LEDs.  Call ~30Hz.
    void tick();

    /// True after the DAW-mode handshake has been sent (i.e. the
    /// device's DAW endpoint is opened and the mode is engaged).
    bool isActive() const { return active; }

private:
    // Lifecycle SysEx
    void enterDawMode();
    void exitDawMode();
    void disableVegasMode();

    // Pad / button feedback
    void paintPad(int row, int col, uint8_t paletteColour);
    void clearAllPads();

    // Mode-follow — listens for user-driven layout changes on the
    // device (Shift menu).  Without this, internal state silently
    // desyncs from what the user sees.
    void onPadModeReport(int mode);
    void onEncoderModeReport(int mode);

    // Surface event handlers
    void handlePadPress(uint8_t padNote, uint8_t velocity);
    void handlePadRelease(uint8_t padNote);
    void handleEncoder(int idx, uint8_t value);
    void handleTransportCC(uint8_t cc, uint8_t value);

    juce::MidiOutput* out();   // resolves to the cached device output

    MainComponent* host = nullptr;
    bool active = false;
    int currentPadMode = 2;       // 2 = DAW/session
    int currentEncoderMode = 1;   // 1 = mixer
    bool shiftHeld = false;

    // Cache of last-painted pad colours so we only push deltas.
    uint8_t lastPaint[16] = {0};
};
