#include "IOStation24cController.h"
#include "MainComponent.h"

namespace IoStation
{
    // Endpoint substring — iOS exposes the device as "ioStation 24c".
    constexpr const char* kPortHint = "iostation";

    // Channel-strip controls (Note On ch0, status 0x90)
    constexpr uint8_t kBtnArm        = 0x00;
    constexpr uint8_t kBtnBypass     = 0x03;
    constexpr uint8_t kBtnSolo       = 0x08;
    constexpr uint8_t kBtnMute       = 0x10;
    constexpr uint8_t kBtnFaderTouch = 0x68;
    constexpr uint8_t kBtnShift      = 0x46;

    // Session Navigator
    constexpr uint8_t kBtnPrev       = 0x2E;
    constexpr uint8_t kBtnEncPush    = 0x20;
    constexpr uint8_t kBtnNext       = 0x2F;
    constexpr uint8_t kBtnLink       = 0x05;   // RGB
    constexpr uint8_t kBtnPan        = 0x2A;   // RGB
    constexpr uint8_t kBtnChannel    = 0x36;   // RGB
    constexpr uint8_t kBtnScroll     = 0x38;   // RGB
    constexpr uint8_t kBtnMaster     = 0x3A;
    constexpr uint8_t kBtnClick      = 0x3B;
    constexpr uint8_t kBtnSection    = 0x3C;   // beware: same ID as encoder CC#
    constexpr uint8_t kBtnMarker     = 0x3D;

    // Automation
    constexpr uint8_t kBtnRead       = 0x4A;   // RGB
    constexpr uint8_t kBtnWrite      = 0x4B;   // RGB
    constexpr uint8_t kBtnTouchAuto  = 0x4D;   // RGB

    // Transport (Note On ch0)
    constexpr uint8_t kBtnLoop       = 0x56;
    constexpr uint8_t kBtnRewind     = 0x5B;
    constexpr uint8_t kBtnFFwd       = 0x5C;
    constexpr uint8_t kBtnStop       = 0x5D;
    constexpr uint8_t kBtnPlay       = 0x5E;
    constexpr uint8_t kBtnRecord     = 0x5F;
    constexpr uint8_t kBtnFootswitch = 0x66;

    // Encoder CC: B0 3C xx (CC 0x3C on channel 0). xx bit 7 = direction,
    // bits 0-6 = number of steps.  Same number as the Section button —
    // disambiguated by status byte (0xB0 vs 0x90).
    constexpr uint8_t kEncoderCC    = 0x3C;

    // LED states: off=0x00, on=0x7F.  The firmware also accepts 0x01
    // for "flashing" but we don't currently use that mode.
    constexpr uint8_t kLedOff   = 0x00;
    constexpr uint8_t kLedOn    = 0x7F;
}

IOStation24cController::~IOStation24cController() { detach(); }

void IOStation24cController::attach(MainComponent* h)
{
    host = h;
    tryOpenInput();
    if (!active)
    {
        if (out() != nullptr)
        {
            // Initial LED state — clear everything we know about so the
            // device shows our cached state from a known baseline.
            for (auto& v : ledCache) v = 0xFF;
            for (auto& v : ledCacheRGB) v = 0xFFFFFFFFu;
            clearAllLeds();
            active = true;
        }
    }
}

void IOStation24cController::tryOpenInput()
{
    if (midiInput) return;
    for (auto& dev : juce::MidiInput::getAvailableDevices())
    {
        if (dev.name.containsIgnoreCase(IoStation::kPortHint))
        {
            midiInput = juce::MidiInput::openDevice(dev.identifier, this);
            if (midiInput) midiInput->start();
            return;
        }
    }
}

void IOStation24cController::detach()
{
    if (!active) return;
    if (midiInput) { midiInput->stop(); midiInput.reset(); }
    if (out() != nullptr) clearAllLeds();
    active = false;
}

void IOStation24cController::handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& msg)
{
    appendLog(msg);
    processIncoming(msg);
}

void IOStation24cController::injectMessage(const juce::MidiMessage& msg)
{
    appendLog(msg);
    processIncoming(msg);
}

void IOStation24cController::appendLog(const juce::MidiMessage& msg)
{
    juce::String line;
    for (int i = 0; i < msg.getRawDataSize(); ++i)
        line << juce::String::toHexString(msg.getRawData()[i]).paddedLeft('0', 2).toUpperCase() << " ";
    const juce::ScopedLock lk(logLock);
    recentLog.insert(0, line.trim());
    while (recentLog.size() > 12) recentLog.remove(12);
}

juce::String IOStation24cController::getLastMessages() const
{
    const juce::ScopedLock lk(logLock);
    return recentLog.joinIntoString("\n");
}

juce::MidiOutput* IOStation24cController::out()
{
    if (host == nullptr) return nullptr;
    return host->outputForDevice(IoStation::kPortHint);
}

bool IOStation24cController::processIncoming(const juce::MidiMessage& msg)
{
    const auto* raw = msg.getRawData();
    const int rawLen = msg.getRawDataSize();
    if (rawLen < 1) return false;
    const uint8_t status = raw[0];

    // Pitch-bend ch0 = motorized fader position (14-bit).
    if (status == 0xE0 && rawLen >= 3)
    {
        const int v14 = (raw[1] & 0x7F) | ((raw[2] & 0x7F) << 7);
        handleFaderPitchBend(v14);
        return true;
    }

    // CC ch0, CC# 0x3C = Session Navigator encoder delta.
    if (status == 0xB0 && rawLen >= 3 && raw[1] == IoStation::kEncoderCC)
    {
        const uint8_t v = raw[2];
        // Bit 7 = direction (1 = negative), bits 0-6 = step count.
        const int magnitude = v & 0x7F;
        const int8_t signedDelta = (v & 0x80) ? static_cast<int8_t>(-magnitude)
                                              : static_cast<int8_t>(+magnitude);
        handleEncoderDelta(signedDelta);
        return true;
    }

    // Note On / Note Off ch0 = button press / release (or LED echo).
    // The device transmits press as 0x7F and release as 0x00; we treat
    // any 0x90 with vel>0 as press.
    if ((status == 0x90 || status == 0x80) && rawLen >= 3)
    {
        const uint8_t id = raw[1];
        const bool pressed = (status == 0x90 && raw[2] > 0);
        handleButton(id, pressed);
        return true;
    }

    return false;
}

// ── Incoming handlers ──────────────────────────────────────────────

void IOStation24cController::handleFaderPitchBend(int value14)
{
    const float norm = juce::jlimit(0.0f, 1.0f, static_cast<float>(value14) / 16383.0f);
    lastFaderNorm = norm;
    if (host != nullptr)
        host->setFocusedTrackVolumeFromController(norm);
    // Keep the writeback cache in sync — the device just told us its
    // physical position, so we shouldn't push the same value back.
    lastSentFader = value14;
}

void IOStation24cController::handleEncoderDelta(int8_t signedDelta)
{
    if (host == nullptr || signedDelta == 0) return;
    switch (navMode)
    {
        case NavMode::Master:
        {
            // Nudge focused-track volume in 1/127 steps per tick.
            const float current = lastFaderNorm;
            const float step    = signedDelta * (1.0f / 127.0f);
            const float next    = juce::jlimit(0.0f, 1.0f, current + step);
            host->setFocusedTrackVolumeFromController(next);
            lastFaderNorm = next;
            // The motor-fader writeback in tick() will push this back
            // to the hardware so the cap follows the encoder.
            break;
        }
        case NavMode::Channel:
            // Bank focused track ±N.
            host->controllerSelectTrack(signedDelta > 0 ? +1 : -1);
            break;
        case NavMode::Scroll:
        case NavMode::Section:
        case NavMode::Marker:
            // Scrub the playhead — same scaling as the LK Mini's enc 8.
            host->controllerScrubPlayhead(signedDelta * 4);
            break;
        case NavMode::Pan:
        case NavMode::Click:
        case NavMode::Link:
        default:
            // Fallback: scrub.  Specific Pan / Click / Link behaviors
            // can be wired once the host exposes the relevant helpers.
            host->controllerScrubPlayhead(signedDelta * 2);
            break;
    }
}

void IOStation24cController::handleButton(uint8_t id, bool pressed)
{
    if (host == nullptr) return;

    using namespace IoStation;

    // Fader-touch is a sensor, not a command.
    if (id == kBtnFaderTouch) { faderTouched = pressed; return; }

    // Modifiers
    if (id == kBtnShift) { shiftHeld = pressed; return; }

    // All other bindings act on press only.
    if (!pressed) return;

    switch (id)
    {
        // Transport
        case kBtnPlay:    host->controllerPlayToggle();     break;
        case kBtnStop:    host->controllerStop();           break;
        case kBtnRecord:
            if (shiftHeld) host->controllerToggleMetronomeAndCountIn();
            else           host->controllerRecordToggle();
            break;
        case kBtnLoop:    host->controllerLoopToggle();     break;
        case kBtnRewind:  host->controllerScrubPlayhead(-8); break;
        case kBtnFFwd:    host->controllerScrubPlayhead(+8); break;

        // Channel-strip — act on the currently focused track.  The
        // host helpers themselves require an absolute track index, so
        // we resolve "focused" here.
        case kBtnArm:     host->controllerTrackRecArm(host->getFocusedTrackIndex()); break;
        case kBtnMute:    host->controllerTrackMute  (host->getFocusedTrackIndex()); break;
        case kBtnSolo:    host->controllerTrackSolo  (host->getFocusedTrackIndex()); break;
        case kBtnBypass:  /* no host helper yet — wire later */ break;

        // Session Navigator: bank track focus by one
        case kBtnPrev:    host->controllerSelectTrack(-1);  break;
        case kBtnNext:    host->controllerSelectTrack(+1);  break;

        // Encoder push: reset focused-track volume to unity (0 dB ≈ 1.0).
        case kBtnEncPush: host->setFocusedTrackVolumeFromController(1.0f); break;

        // Mode buttons — change what the encoder addresses.
        case kBtnMaster:  setNavMode(NavMode::Master);  break;
        case kBtnPan:     setNavMode(NavMode::Pan);     break;
        case kBtnChannel: setNavMode(NavMode::Channel); break;
        case kBtnScroll:  setNavMode(NavMode::Scroll);  break;
        case kBtnSection: setNavMode(NavMode::Section); break;
        case kBtnMarker:  setNavMode(NavMode::Marker);  break;
        case kBtnClick:   setNavMode(NavMode::Click);   break;
        case kBtnLink:    setNavMode(NavMode::Link);    break;

        // Automation buttons — placeholder mappings until automation
        // lanes are exposed by the host.  Read = save snapshot (1),
        // Write = save snapshot (2), Touch = undo, so the buttons at
        // least do *something* useful while we build out the surface.
        case kBtnRead:    host->controllerSaveSnapshot(0); break;
        case kBtnWrite:   host->controllerSaveSnapshot(1); break;
        case kBtnTouchAuto: host->controllerUndo();        break;

        case kBtnFootswitch: host->controllerPlayToggle(); break;

        default: break;
    }
}

void IOStation24cController::setNavMode(NavMode m)
{
    navMode = m;
}

// ── Periodic refresh ───────────────────────────────────────────────

void IOStation24cController::tick()
{
    if (host == nullptr) return;
    if (!active) { tryOpenInput(); return; }

    // ── Motor-fader writeback ──
    // Only push when the user isn't actively touching the fader,
    // otherwise we'd fight their finger and the surface judders.
    if (!faderTouched && out() != nullptr)
    {
        // Read the focused track's current volume so the motor follows
        // on-screen slider drags + encoder nudges + automation, not
        // just controller-driven changes.
        const float vol = host->getFocusedTrackVolume();
        lastFaderNorm = vol;

        const int target14 = juce::jlimit(0, 16383,
            static_cast<int>(std::round(vol * 16383.0f)));
        // Throttle: skip if delta < 32 LSB (~0.2%) to reduce bus chatter.
        if (lastSentFader < 0 || std::abs(target14 - lastSentFader) >= 32)
        {
            sendFaderPosition(target14);
            lastSentFader = target14;
        }
    }

    // ── Transport LEDs ──
    using namespace IoStation;
    const bool playing   = host->controllerEngineIsPlaying();
    const bool recording = host->controllerEngineIsRecording();
    sendButton(kBtnPlay,   playing);
    sendButton(kBtnRecord, recording);
    // Stop is "lit when transport is at rest" — subtle UX hint.
    sendButton(kBtnStop,  !playing && !recording);

    // ── Mode-button LEDs (only the active one lit) ──
    auto litFor = [&](NavMode m) { return navMode == m; };
    sendButton(kBtnMaster,  litFor(NavMode::Master));
    sendButton(kBtnClick,   litFor(NavMode::Click));
    sendButton(kBtnSection, litFor(NavMode::Section));
    sendButton(kBtnMarker,  litFor(NavMode::Marker));

    // RGB mode buttons — each gets its own hue when active, off otherwise.
    sendButtonRGB(kBtnPan,     litFor(NavMode::Pan)     ? 0x40 : 0, 0x10, 0);
    sendButtonRGB(kBtnChannel, 0, litFor(NavMode::Channel) ? 0x40 : 0, 0x10);
    sendButtonRGB(kBtnScroll,  litFor(NavMode::Scroll)  ? 0x30 : 0, 0x30, 0);
    sendButtonRGB(kBtnLink,    litFor(NavMode::Link)    ? 0x40 : 0, 0, 0x40);
}

// ── LED output ─────────────────────────────────────────────────────

void IOStation24cController::sendButton(uint8_t id, bool on)
{
    const uint8_t v = on ? IoStation::kLedOn : IoStation::kLedOff;
    if (ledCache[id] == v) return;
    ledCache[id] = v;
    if (auto* o = out())
        o->sendMessageNow(juce::MidiMessage(0x90, id, v));
}

void IOStation24cController::sendButtonRGB(uint8_t id, uint8_t r, uint8_t g, uint8_t b)
{
    const uint32_t packed = (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
    if (ledCacheRGB[id] == packed) return;
    ledCacheRGB[id] = packed;
    auto* o = out(); if (!o) return;
    // Per manual 10.2.4: Red = 0x91 id cc, Green = 0x92 id cc, Blue = 0x93 id cc.
    // Components are 7-bit (0..127) so mask the top bit to avoid SysEx
    // framing collisions / unintended status-byte detection.
    o->sendMessageNow(juce::MidiMessage(0x91, id, r & 0x7F));
    o->sendMessageNow(juce::MidiMessage(0x92, id, g & 0x7F));
    o->sendMessageNow(juce::MidiMessage(0x93, id, b & 0x7F));
}

void IOStation24cController::sendFaderPosition(int value14)
{
    auto* o = out(); if (!o) return;
    const int v = juce::jlimit(0, 16383, value14);
    const uint8_t lo = uint8_t(v & 0x7F);
    const uint8_t hi = uint8_t((v >> 7) & 0x7F);
    o->sendMessageNow(juce::MidiMessage(0xE0, lo, hi));
}

void IOStation24cController::clearAllLeds()
{
    using namespace IoStation;
    static const uint8_t allIds[] = {
        kBtnArm, kBtnBypass, kBtnSolo, kBtnMute, kBtnShift,
        kBtnPrev, kBtnNext, kBtnMaster, kBtnClick, kBtnSection, kBtnMarker,
        kBtnLoop, kBtnRewind, kBtnFFwd, kBtnStop, kBtnPlay, kBtnRecord
    };
    for (auto id : allIds) sendButton(id, false);
    static const uint8_t rgbIds[] = {
        kBtnLink, kBtnPan, kBtnChannel, kBtnScroll,
        kBtnRead, kBtnWrite, kBtnTouchAuto
    };
    for (auto id : rgbIds) sendButtonRGB(id, 0, 0, 0);
}
