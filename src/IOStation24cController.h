// Native integration for the PreSonus ioStation 24c — combined 2x2
// USB-C audio interface + single-fader / Session-Navigator production
// controller.
//
// We target the device's "Studio One Native" mode (boot key combo:
// hold NEXT + press SOLO at power-on).  That mode is fully documented
// in the OEM manual section 10.2 — fader on pitch-bend ch0, encoder
// as CC 0x3C delta, buttons as note-on ch0 with an explicit ID table
// for LEDs.  MCU mode would also work but is more ambiguous to drive.
//
// The audio-interface side is class-compliant USB Audio: JUCE's
// AudioDeviceManager picks it up on its own, no work here.
//
// Wire up in MainComponent:
//   1. `iostation.attach(this);` from the ctor / hot-attach tick.
//   2. `iostation.processIncoming(msg)` first in handleIncomingMidiMessage
//      when the source name contains "iostation" (returns true on consume).
//   3. `iostation.tick();` from the existing 30Hz timer for transport-LED
//      and motor-fader writeback.
#pragma once
#include <JuceHeader.h>

class MainComponent;

class IOStation24cController : private juce::MidiInputCallback
{
public:
    IOStation24cController() = default;
    ~IOStation24cController();

    /// Open the ioStation MIDI port (if present) and prime the LED
    /// caches.  Idempotent — safe to call every tick for hot-plug.
    void attach(MainComponent* host);
    void detach();

    /// Decode a MIDI message from the ioStation port.  Returns true
    /// when the message was consumed (caller skips its normal dispatch).
    bool processIncoming(const juce::MidiMessage& msg);

    /// Periodic LED refresh + motor-fader writeback.  Call ~30Hz.
    void tick();

    /// True once the input port has been opened successfully.
    bool isActive() const { return active; }

    /// Last N messages received, formatted as hex.  Drives the
    /// on-screen MIDI inspector in the debug panel.
    juce::String getLastMessages() const;

    /// Push a synthetic message through the same handler as a real one
    /// — used by the on-screen debug surface so every binding can be
    /// exercised without the hardware plugged in.
    void injectMessage(const juce::MidiMessage& msg);

    /// True while the user has a finger on the touch-sensitive fader.
    /// The debug panel reads this to draw a "TOUCH" tag and the
    /// motor-fader writeback gates on it to avoid feedback.
    bool isFaderTouched() const { return faderTouched; }

    /// Last fader value reported by the device, normalized 0..1.  The
    /// debug panel mirrors the cap from this so virtual + physical
    /// faders track each other.
    float getFaderValue() const { return lastFaderNorm; }

private:
    // ── Outgoing helpers ──
    juce::MidiOutput* out();
    void sendButton(uint8_t id, bool on);          // 0x90 ch0 LED state
    void sendButtonRGB(uint8_t id, uint8_t r, uint8_t g, uint8_t b);
    void sendFaderPosition(int value14);           // pitch-bend ch0
    void clearAllLeds();

    // ── Incoming decoders ──
    void handleFaderPitchBend(int value14);
    void handleEncoderDelta(int8_t signedDelta);
    void handleButton(uint8_t id, bool pressed);

    void tryOpenInput();
    void appendLog(const juce::MidiMessage& msg);

    // Mode tracking for the Session Navigator encoder.  The user picks
    // a mode by tapping one of Master/Pan/Channel/Scroll/Section/
    // Marker/Click/Link — we keep the matching LED lit and route the
    // encoder accordingly.  Default to Master so it's useful out of
    // the box.
    enum class NavMode { Master, Pan, Channel, Scroll, Section, Marker, Click, Link };
    void setNavMode(NavMode m);

    /// MidiInputCallback — receives the device's events directly,
    /// bypassing MainComponent's single-input dropdown.
    void handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage&) override;

    MainComponent*                       host     = nullptr;
    std::unique_ptr<juce::MidiInput>     midiInput;
    bool                                 active   = false;

    // Fader state
    bool      faderTouched   = false;
    float     lastFaderNorm  = 0.0f;
    int       lastSentFader  = -1;          // last 14-bit value pushed to motor

    // Modifier
    bool      shiftHeld      = false;

    // Session Navigator
    NavMode   navMode        = NavMode::Master;

    // Per-button LED cache so we only push on change.
    // Key = button ID (we use a flat 128-entry array since all IDs are < 0x80).
    uint8_t   ledCache[128]  = {0};         // 0xFF = "not yet painted"
    uint32_t  ledCacheRGB[128]= {0};        // packed 0xRRGGBB; 0xffffffff = unset

    mutable juce::CriticalSection logLock;
    juce::StringArray             recentLog;   // newest first, capped
};
