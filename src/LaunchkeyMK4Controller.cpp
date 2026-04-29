#include "LaunchkeyMK4Controller.h"
#include "MainComponent.h"
#include "PluginHost.h"
#include "SequencerEngine.h"
#include "SessionViewComponent.h"

namespace LkMini {
    // SysEx header for Mini SKUs (Mini 25 + Mini 37 share 0x13;
    // full-size LK 25/37/49/61 use 0x14).  The Mini line's firmware
    // does NOT accept host display SysEx (Configure/Set Text are
    // silently dropped) even on the Mini 37 which has the OLED
    // hardware.  The OLED is firmware-locked to the device's own
    // chord/scale/mode info.  Verified empirically 2026-04-28 across
    // three iterations matching Live 12's remote-script bytes.
    constexpr uint8_t kHdr[] = { 0xF0, 0x00, 0x20, 0x29, 0x02, 0x13 };

    // DAW endpoint name substring.  iOS exposes Launchkey ports as
    // "Launch Key" (two words with a space), so we match on just
    // "launch" and disambiguate the DAW port via "daw" elsewhere.
    constexpr const char* kDawPortHint = "launch";

    // Lifecycle
    constexpr uint8_t kEnterDaw[] = { 0x9F, 0x0C, 0x7F };
    constexpr uint8_t kExitDaw[]  = { 0x9F, 0x0C, 0x00 };
    constexpr uint8_t kVegasOff[] = { 0xB6, 0x72, 0x00 };

    // Mode CCs (channel 7 status = 0xB6)
    constexpr uint8_t kCcPadMode = 0x1D;
    constexpr uint8_t kCcEncMode = 0x1E;

    // Session-layout pad notes (channel 1, status 0x90/0x80)
    // Top row 0x60-0x67, bottom row 0x70-0x77.
    inline uint8_t sessionPadNote(int row, int col) {
        return uint8_t(0x60 + row * 0x10 + col);
    }

    // Encoder CCs on channel 16 (status 0xBF), 0x55..0x5C.
    constexpr uint8_t kEncoderCC[8] = { 0x55,0x56,0x57,0x58,0x59,0x5A,0x5B,0x5C };

    // Transport / nav button CCs on channel 16
    constexpr uint8_t kCcPlay = 0x73, kCcStop = 0x74, kCcRecord = 0x75, kCcLoop = 0x76;
    constexpr uint8_t kCcTrackL = 0x66, kCcTrackR = 0x67;
    constexpr uint8_t kCcSceneUp = 0x6A, kCcSceneDn = 0x6B;
    constexpr uint8_t kCcRowRight = 0x68, kCcFunction = 0x69;
    constexpr uint8_t kCcShift = 0x3F;

    // Palette colours (Novation 128-entry palette indices)
    constexpr uint8_t kPalOff       = 0x00;
    constexpr uint8_t kPalDimGreen  = 0x13;
    constexpr uint8_t kPalGreen     = 0x15;
    constexpr uint8_t kPalRed       = 0x05;
    constexpr uint8_t kPalAmber     = 0x09;
    constexpr uint8_t kPalBlue      = 0x29;
    constexpr uint8_t kPalDimBlue   = 0x2D;
}

LaunchkeyMK4Controller::~LaunchkeyMK4Controller() { detach(); }

void LaunchkeyMK4Controller::attach(MainComponent* h)
{
    host = h;
    tryOpenInput();   // may be retried later if it failed first time
    if (!active)
    {
        if (auto* o = out())
        {
            enterDawMode();
            disableVegasMode();
            active = true;
        }
    }
}

void LaunchkeyMK4Controller::tryOpenInput()
{
    if (dawInput) return;
    for (auto& dev : juce::MidiInput::getAvailableDevices())
    {
        if (dev.name.containsIgnoreCase("launch")
            && dev.name.containsIgnoreCase("daw"))
        {
            dawInput = juce::MidiInput::openDevice(dev.identifier, this);
            if (dawInput) { dawInput->start(); }
            return;
        }
    }
}

void LaunchkeyMK4Controller::detach()
{
    if (!active) return;
    if (dawInput) { dawInput->stop(); dawInput.reset(); }
    if (auto* o = out())
    {
        clearAllPads();
        exitDawMode();
    }
    active = false;
}

void LaunchkeyMK4Controller::handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& msg)
{
    // Append to the rolling on-screen MIDI inspector log.
    {
        juce::String line;
        for (int i = 0; i < msg.getRawDataSize(); ++i)
            line << juce::String::toHexString(msg.getRawData()[i]).paddedLeft('0', 2).toUpperCase() << " ";
        const juce::ScopedLock lk(logLock);
        recentLog.insert(0, line.trim());
        while (recentLog.size() > 8) recentLog.remove(8);
    }
    processIncoming(msg);
}

juce::MidiOutput* LaunchkeyMK4Controller::out()
{
    if (host == nullptr) return nullptr;
    return host->outputForDevice(LkMini::kDawPortHint);
}

void LaunchkeyMK4Controller::enterDawMode()
{
    if (auto* o = out())
        o->sendMessageNow(juce::MidiMessage(LkMini::kEnterDaw[0], LkMini::kEnterDaw[1], LkMini::kEnterDaw[2]));
}
void LaunchkeyMK4Controller::exitDawMode()
{
    if (auto* o = out())
        o->sendMessageNow(juce::MidiMessage(LkMini::kExitDaw[0], LkMini::kExitDaw[1], LkMini::kExitDaw[2]));
}
void LaunchkeyMK4Controller::disableVegasMode()
{
    if (auto* o = out())
        o->sendMessageNow(juce::MidiMessage(LkMini::kVegasOff[0], LkMini::kVegasOff[1], LkMini::kVegasOff[2]));
}

bool LaunchkeyMK4Controller::processIncoming(const juce::MidiMessage& msg)
{
    if (!active) return false;

    const auto* raw = msg.getRawData();
    const int rawLen = msg.getRawDataSize();
    if (rawLen < 1) return false;
    const uint8_t status = raw[0];

    // Mode-follow on channel 7 (status 0xB6) — must subscribe or
    // pad/encoder behaviour silently desyncs from what the user sees.
    if (status == 0xB6 && rawLen >= 3)
    {
        if (raw[1] == LkMini::kCcPadMode)      { onPadModeReport(raw[2]); return true; }
        if (raw[1] == LkMini::kCcEncMode)      { onEncoderModeReport(raw[2]); return true; }
    }

    // Session pads (channel 1)
    if ((status == 0x90 || status == 0x80) && rawLen >= 3)
    {
        const uint8_t note = raw[1];
        if (note >= 0x60 && note <= 0x77)
        {
            if (status == 0x90 && raw[2] > 0) handlePadPress(note, raw[2]);
            else                              handlePadRelease(note);
            return true;
        }
    }

    // CC on ANY channel covers encoders + transport/nav buttons.
    // Mk4 firmware sends transport on ch1 (status B0), older Mk3
    // docs say ch16 — accept either.  Encoder CC numbers also vary
    // by mode (0x15-0x1C mixer / 0x37-0x3E plugin / 0x55-0x5C alt).
    if ((status & 0xF0) == 0xB0 && rawLen >= 3)
    {
        const uint8_t cc = raw[1], val = raw[2];
        auto encIdx = [&]() -> int {
            if (cc >= 0x15 && cc <= 0x1C) return cc - 0x15;
            if (cc >= 0x37 && cc <= 0x3E) return cc - 0x37;
            if (cc >= 0x55 && cc <= 0x5C) return cc - 0x55;
            return -1;
        }();
        if (encIdx >= 0)
        {
            handleEncoder(encIdx, val);
            return true;
        }
        handleTransportCC(cc, val);
        return true;
    }

    return false;
}

void LaunchkeyMK4Controller::onPadModeReport(int mode)     { currentPadMode = mode; }
void LaunchkeyMK4Controller::onEncoderModeReport(int mode) { currentEncoderMode = mode; }

void LaunchkeyMK4Controller::setDevicePadMode(uint8_t mode)
{
    if (currentPadMode == mode) return;
    if (auto* o = out())
        o->sendMessageNow(juce::MidiMessage(0xB6, LkMini::kCcPadMode, mode));
    currentPadMode = mode;
}

void LaunchkeyMK4Controller::setDeviceEncoderMode(uint8_t mode)
{
    if (currentEncoderMode == mode) return;
    if (auto* o = out())
        o->sendMessageNow(juce::MidiMessage(0xB6, LkMini::kCcEncMode, mode));
    currentEncoderMode = mode;
}

void LaunchkeyMK4Controller::handlePadPress(uint8_t padNote, uint8_t /*vel*/)
{
    if (host == nullptr) return;
    // Decode the 2x8 grid: top row 0x60..0x67, bottom row 0x70..0x77
    const int row = (padNote & 0x10) ? 1 : 0;
    const int col = padNote & 0x07;
    const int idx = row * 8 + col;
    const auto now = juce::Time::currentTimeMillis();
    padPressMillis[idx] = now;     // press-flash trigger
    lastInputAtMillis   = now;     // wake from idle
    // Launchkey themes repurpose pads as edit-toolbar shortcuts —
    // host owns the mapping.  Other themes fall through to the
    // session-view clip launcher.
    if (host->controllerToolbarPadActive())
    {
        host->controllerToolbarPadAction(row, col);
        return;
    }
    if (auto* sv = host->getSessionViewComponent())
        sv->launchPad(row, col);
}

void LaunchkeyMK4Controller::handlePadRelease(uint8_t /*padNote*/) { /* no-op */ }

void LaunchkeyMK4Controller::handleEncoder(int idx, uint8_t value)
{
    if (host == nullptr || idx < 0 || idx > 7) return;
    lastInputAtMillis = juce::Time::currentTimeMillis();
    // Routing is the same regardless of which mode the device is in
    // — the OLED label changes (Mixer / Plugin / Sends), but our
    // encoder semantics stay constant: enc 1 = focused-track vol,
    // enc 2-7 = on-screen sliders, enc 8 = playhead scrub.

    const float v = value / 127.0f;
    if (idx == 0)
    {
        // Encoder 1 (leftmost): volume of the focused track.
        host->setFocusedTrackVolumeFromController(v);
    }
    else if (idx == 7)
    {
        // Encoder 8 (rightmost): scrub the project playhead.  The
        // device sends absolute 0-127; we derive a signed delta
        // against the prior value.  First reading just seeds the
        // cache.  Wrap detection: if the value changes by more than
        // half the range, the encoder wrapped at 0/127 — adjust by
        // ±128 to take the short path so direction stays correct.
        const int prev = lastEncoderValue[idx];
        lastEncoderValue[idx] = value;
        if (prev >= 0)
        {
            int delta = static_cast<int>(value) - prev;
            if (delta >  64) delta -= 128;
            if (delta < -64) delta += 128;
            if (delta != 0) host->controllerScrubPlayhead(delta);
        }
        // Auto-scrub at the limits: the encoder clamps at 0/127 and
        // stops firing events, so if the user is still trying to
        // scroll past the limit we keep going on the shared tick().
        // The 2s timeout protects against an encoder left pinned
        // unattended (would otherwise scroll forever).
        if (value >= 126)      { scrubAutoDir = +1; scrubAutoEndsAt = juce::Time::currentTimeMillis() + 2000; }
        else if (value <= 1)   { scrubAutoDir = -1; scrubAutoEndsAt = juce::Time::currentTimeMillis() + 2000; }
        else                   { scrubAutoDir = 0; }
    }
    else
    {
        // Encoders 2-7: route to on-screen slider position (idx-1) so
        // the controller layout mirrors the iPad's visual order.
        host->controllerSetParamBySliderIndex(idx - 1, v);
    }
}

void LaunchkeyMK4Controller::handleTransportCC(uint8_t cc, uint8_t value)
{
    if (host == nullptr) return;
    lastInputAtMillis = juce::Time::currentTimeMillis();
    const bool press = (value >= 64);

    if (cc == LkMini::kCcShift)    { shiftHeld = press; return; }

    // Track ▲▼ on the Mk4: each direction is its own CC (UP=0x6A,
    // DOWN=0x6B) with standard press/release values (0x7F/0x00).
    // Plain press changes preset on the focused track (the more
    // common operation while playing); Shift+press changes the
    // focused track itself (CCs 0x66/0x67 — see switch below).
    // UP feels like "next" in the preset list, DOWN like "previous".
    if (cc == 0x6A) { if (press) host->controllerPresetNext(); return; }
    if (cc == 0x6B) { if (press) host->controllerPresetPrev(); return; }

    // Encoder param-page buttons (the two arrows next to the encoder
    // row).  UP=0x33, DOWN=0x34 — empirically captured.  Page the
    // on-screen param sliders so encoders 2-8 address the next bank.
    if (cc == 0x33) { if (press) host->controllerParamPagePrev(); return; }
    if (cc == 0x34) { if (press) host->controllerParamPageNext(); return; }

    if (!press) return;   // every other button: act on press only

    switch (cc)
    {
        case LkMini::kCcPlay:    if (shiftHeld) host->controllerReturnToStart(); else host->controllerPlayToggle(); break;
        case LkMini::kCcStop:    host->controllerStop();          break;
        case LkMini::kCcRecord:  if (shiftHeld) host->controllerToggleMetronomeAndCountIn(); else host->controllerRecordToggle(); break;
        case LkMini::kCcLoop:    host->controllerLoopToggle();    break;
        // 0x66 / 0x67 arrive only when Shift is held with the Track
        // ▲▼ buttons — the firmware translates "Shift+Track UP/DOWN"
        // into these CCs at the hardware level.  Switch the focused
        // track (Shift = the deliberate "change context" gesture).
        case LkMini::kCcTrackL:  host->controllerSelectTrack(-1); break;
        case LkMini::kCcTrackR:  host->controllerSelectTrack(+1); break;
        case LkMini::kCcSceneDn: host->controllerScrollScenes(+1);break;
        case LkMini::kCcRowRight:host->controllerLaunchScene();   break;
        default: break;
    }
}

void LaunchkeyMK4Controller::tick()
{
    if (!active || host == nullptr) return;
    // Late-arriving input port: try again every tick until it opens.
    if (!dawInput) tryOpenInput();

    // Continuous playhead scrub when the encoder is pinned at an
    // extreme — covered by the timeout so an unattended encoder
    // doesn't keep scrolling forever.
    if (scrubAutoDir != 0)
    {
        if (juce::Time::currentTimeMillis() > scrubAutoEndsAt) scrubAutoDir = 0;
        else host->controllerScrubPlayhead(scrubAutoDir * 4);
    }

    const bool toolbarMode = host->controllerToolbarPadActive();
    auto* sv = toolbarMode ? nullptr : host->getSessionViewComponent();
    if (!toolbarMode && sv == nullptr) return;

    // Toolbar mode: paint each pad with its assigned RGB plus a
    // stack of light animations — boot wave, beat pulse during
    // playback, breathing while idle, hue drift, idle dim, press
    // flash, and a record-state red overlay.
    if (toolbarMode)
    {
        const auto now = juce::Time::currentTimeMillis();

        // Detect entry into toolbar mode → kick the boot wave.
        if (!wasInToolbarMode)
        {
            wasInToolbarMode = true;
            toolbarModeEntryMillis = now;
            if (lastInputAtMillis == 0) lastInputAtMillis = now;
        }

        const juce::int64 sinceEnter = now - toolbarModeEntryMillis;
        const bool inBootAnim = sinceEnter < 640;
        const int  waveCol = static_cast<int>(sinceEnter / 80);   // 0..7 over ~640ms

        const bool isPlaying   = host->controllerEngineIsPlaying();
        const bool isRecording = host->controllerEngineIsRecording();
        const double posBeats  = host->controllerEngineBeatPosition();
        const double bpm       = juce::jmax(60.0, host->controllerEngineBpm());

        const bool isIdle = !inBootAnim && !isPlaying
                            && (now - lastInputAtMillis) > 5000;

        for (int row = 0; row < 2; ++row)
        {
            for (int col = 0; col < 8; ++col)
            {
                const int idx = row * 8 + col;
                const uint32_t baseRgb = host->controllerToolbarPadColorRGB(row, col);
                if (baseRgb == 0)
                {
                    if (lastPaintRGB[idx] != 0u)
                    {
                        lastPaintRGB[idx] = 0u;
                        paintPadRGB(row, col, 0, 0, 0);
                    }
                    continue;
                }

                juce::Colour c(static_cast<juce::uint32>(0xff000000u | baseRgb));

                // Boot wave: pads light only after the wave passes
                // their column; the crest itself flashes white.
                if (inBootAnim)
                {
                    if (col > waveCol)       c = juce::Colours::black;
                    else if (col == waveCol) c = juce::Colours::white;
                }
                else
                {
                    // On-beat lift during playback — single source of
                    // animation on the toolbar pads.  Pads sit at their
                    // static gradient brightness when transport stopped.
                    if (isPlaying)
                    {
                        const float frac = static_cast<float>(posBeats - std::floor(posBeats));
                        const float lift = 0.18f * (1.0f - frac) * (1.0f - frac);
                        c = c.withMultipliedBrightness(1.0f + lift);
                    }
                    // Subtle hue drift while idle for >5s.
                    if (isIdle)
                    {
                        const float drift = 0.025f * static_cast<float>(std::sin(now / 4000.0));
                        c = c.withRotatedHue(drift);
                    }
                }

                // Idle dim — applies regardless of playback state.
                if (isIdle) c = c.withMultipliedBrightness(0.65f);

                // Press flash (additive white pulse, ~220ms decay).
                if (padPressMillis[idx] > 0)
                {
                    const auto sincePress = now - padPressMillis[idx];
                    if (sincePress >= 0 && sincePress < 220)
                    {
                        const float k = 1.0f - static_cast<float>(sincePress) / 220.0f;
                        c = c.interpolatedWith(juce::Colours::white, k * 0.7f);
                    }
                }

                // Record overlay — blend toward a pulsing red while
                // the engine is recording.
                if (isRecording)
                {
                    const float pulse = 0.18f + 0.14f * static_cast<float>(
                        std::sin(now / 220.0));
                    c = c.interpolatedWith(juce::Colour(0xffff3030), pulse);
                }

                const uint32_t finalRgb = c.getARGB() & 0x00FFFFFFu;
                // Always mirror to the iPad button so the on-screen
                // glow tracks the device animation; the host method
                // self-throttles by only repainting on change.
                host->controllerSetToolbarButtonAnimatedColor(row, col, finalRgb);
                if (lastPaintRGB[idx] == finalRgb) continue;
                lastPaintRGB[idx] = finalRgb;
                lastPaint[idx] = 0;   // invalidate palette cache
                const uint8_t r8 = static_cast<uint8_t>((finalRgb >> 16) & 0xFF);
                const uint8_t g8 = static_cast<uint8_t>((finalRgb >>  8) & 0xFF);
                const uint8_t b8 = static_cast<uint8_t>( finalRgb        & 0xFF);
                paintPadRGB(row, col,
                            static_cast<uint8_t>(r8 * 127 / 255),
                            static_cast<uint8_t>(g8 * 127 / 255),
                            static_cast<uint8_t>(b8 * 127 / 255));
            }
        }
        return;
    }
    // Leaving toolbar mode resets so the boot wave plays next time.
    wasInToolbarMode = false;

    for (int row = 0; row < 2; ++row)
    {
        for (int col = 0; col < 8; ++col)
        {
            uint8_t colour = LkMini::kPalOff;
            switch (sv->stateForVisibleSlot(row, col))
            {
                case 1: colour = LkMini::kPalDimBlue; break;  // Stopped (has content)
                case 2: colour = LkMini::kPalGreen;   break;  // Playing
                case 3: colour = LkMini::kPalRed;     break;  // Recording
                case 4: colour = LkMini::kPalAmber;   break;  // Armed
                default: break;
            }
            const int idx = row * 8 + col;
            // Coming out of toolbar mode: invalidate the RGB cache
            // so a future toolbar-mode entry re-pushes.
            lastPaintRGB[idx] = 0xffffffffu;
            if (lastPaint[idx] == colour) continue;
            lastPaint[idx] = colour;
            paintPad(row, col, colour);
        }
    }
}

void LaunchkeyMK4Controller::paintPad(int row, int col, uint8_t colour)
{
    if (auto* o = out())
    {
        // Note On ch1 with the pad note + palette colour.
        o->sendMessageNow(juce::MidiMessage(0x90, LkMini::sessionPadNote(row, col), colour));
    }
}

void LaunchkeyMK4Controller::paintPadRGB(int row, int col, uint8_t r, uint8_t g, uint8_t b)
{
    auto* o = out(); if (!o) return;
    // Custom-RGB SysEx — Launchkey Mk4 (works on Mini per Live's
    // remote script + the bornacvitanic SDK).  Channels are 7-bit
    // (0-127), high bit clears or the SysEx framing breaks.  Caller
    // is responsible for scaling 0-255 → 0-127.
    const uint8_t bytes[] = {
        0x00, 0x20, 0x29, 0x02, 0x13,
        0x01, 0x43,
        LkMini::sessionPadNote(row, col),
        static_cast<uint8_t>(r & 0x7F),
        static_cast<uint8_t>(g & 0x7F),
        static_cast<uint8_t>(b & 0x7F)
    };
    o->sendMessageNow(juce::MidiMessage::createSysExMessage(bytes, sizeof(bytes)));
}

juce::String LaunchkeyMK4Controller::getLastMessages() const
{
    const juce::ScopedLock lk(logLock);
    return recentLog.joinIntoString("\n");
}

void LaunchkeyMK4Controller::clearAllPads()
{
    for (int r = 0; r < 2; ++r)
        for (int c = 0; c < 8; ++c)
            paintPad(r, c, LkMini::kPalOff);
    for (auto& c : lastPaint) c = 0;
}
