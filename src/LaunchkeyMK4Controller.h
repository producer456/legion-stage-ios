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

class LaunchkeyMK4Controller : private juce::MidiInputCallback
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

    /// Last N messages received on the DAW input, formatted as hex.
    /// MainComponent reads this each tick to drive an on-screen
    /// MIDI inspector.  Thread-safe via a mutex on write/read.
    juce::String getLastMessages() const;

    /// Tell the device to switch its pad / encoder mode.  The mode-CC
    /// is bidirectional — writing it makes the firmware change layout
    /// (and the OLED label updates for free).  Pad-mode bytes:
    /// 2 = DAW/session (default).  Encoder-mode bytes: 1 = Mixer.
    /// Other values discovered empirically via the on-screen MIDI
    /// inspector + the device's Encoder/Pad Mode buttons.
    void setDevicePadMode(uint8_t mode);
    void setDeviceEncoderMode(uint8_t mode);

private:
    // Lifecycle SysEx
    void enterDawMode();
    void exitDawMode();
    void disableVegasMode();

    // Pad / button feedback
    void paintPad(int row, int col, uint8_t paletteColour);
    void paintPadRGB(int row, int col, uint8_t r, uint8_t g, uint8_t b);
    void clearAllPads();

    // Mode-follow — listens for user-driven layout changes on the
    // device (Shift menu).  Without this, internal state silently
    // desyncs from what the user sees.
    void onPadModeReport(int mode);
    void onEncoderModeReport(int mode);
    void tryOpenInput();

    // Surface event handlers
    void handlePadPress(uint8_t padNote, uint8_t velocity);
    void handlePadRelease(uint8_t padNote);
    void handleEncoder(int idx, uint8_t value);
    void handleTransportCC(uint8_t cc, uint8_t value);

    juce::MidiOutput* out();   // resolves to the cached device output

    /// MidiInputCallback — receives the DAW port's incoming events
    /// directly, bypassing MainComponent's single-input dropdown.
    void handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage&) override;

    MainComponent* host = nullptr;
    std::unique_ptr<juce::MidiInput> dawInput;
    bool active = false;
    int currentPadMode = 2;       // 2 = DAW/session
    int currentEncoderMode = 1;   // 1 = mixer
    bool shiftHeld = false;

    // Last value seen per encoder (0..127), -1 = not yet observed.
    // Encoder 8 (idx 7) uses this to derive a direction (±1) for
    // playhead scrubbing — the device sends absolute values, so we
    // delta against the prior reading.
    int8_t lastEncoderValue[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };

    // Auto-scrub state for the playhead encoder.  The Mk4's encoders
    // clamp at 0/127 and stop emitting events at the limit, so when
    // the user pins it past either edge we keep scrubbing on the
    // shared tick until they turn back or the timeout expires.
    int8_t scrubAutoDir = 0;             // 0=off, +1=forward, -1=back
    juce::int64 scrubAutoEndsAt = 0;     // millis (juce::Time::currentTimeMillis())

    // Cache of last-painted pad palette colours so we only push deltas.
    uint8_t lastPaint[16] = {0};
    // Cache of last-painted RGB triples (packed into uint32 0xRRGGBB).
    // 0xffffffff means "not yet painted" so the first frame always
    // pushes.  Used in toolbar-pad mode where pads carry custom RGB.
    uint32_t lastPaintRGB[16] = {
        0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu,
        0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu,
        0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu,
        0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu
    };

    // ── Pad-LED animation state ──
    // Boot/wake wave runs for ~600ms after entering toolbar-pad mode
    // (LK theme detected, hot-plug, theme switch).
    bool wasInToolbarMode = false;
    juce::int64 toolbarModeEntryMillis = 0;
    // Press flash — millis timestamp of the last press per pad (0 = never).
    juce::int64 padPressMillis[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    // Idle detection — bumped on any pad/encoder/transport input.
    juce::int64 lastInputAtMillis = 0;

    mutable juce::CriticalSection logLock;
    juce::StringArray recentLog;   // newest first, capped
};
