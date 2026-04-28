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

void LaunchkeyMK4Controller::handlePadPress(uint8_t padNote, uint8_t /*vel*/)
{
    if (host == nullptr) return;
    // Decode the 2x8 grid: top row 0x60..0x67, bottom row 0x70..0x77
    const int row = (padNote & 0x10) ? 1 : 0;
    const int col = padNote & 0x07;
    if (auto* sv = host->getSessionViewComponent())
        sv->launchPad(row, col);
}

void LaunchkeyMK4Controller::handlePadRelease(uint8_t /*padNote*/) { /* no-op */ }

void LaunchkeyMK4Controller::handleEncoder(int idx, uint8_t value)
{
    if (host == nullptr || idx < 0 || idx > 7) return;
    if (currentEncoderMode != 1) return;   // mixer mode only for now

    const float v = value / 127.0f;
    if (idx == 0)
    {
        // Encoder 1 (leftmost): volume of the focused track.
        host->setFocusedTrackVolumeFromController(v);
    }
    else
    {
        // Encoders 2-8: route to on-screen slider position (idx-1) so
        // the controller layout mirrors the iPad's visual order.
        // Param paging via dedicated page-shift buttons is TODO —
        // needs CC capture from the on-screen MIDI inspector for the
        // right-of-encoder buttons; once wired, paging will scroll
        // the slider page in MainComponent and these encoders will
        // automatically address the new bank.
        host->controllerSetParamBySliderIndex(idx - 1, v);
    }
}

void LaunchkeyMK4Controller::handleTransportCC(uint8_t cc, uint8_t value)
{
    if (host == nullptr) return;
    const bool press = (value >= 64);

    if (cc == LkMini::kCcShift)    { shiftHeld = press; return; }

    // Track ◀▶ on the Mk4: each direction is its own CC (UP=0x6A,
    // DOWN=0x6B) with standard press/release values (0x7F/0x00).
    // Act on press only.
    if (cc == 0x6A) { if (press) host->controllerSelectTrack(-1); return; }
    if (cc == 0x6B) { if (press) host->controllerSelectTrack(+1); return; }

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
        case LkMini::kCcRecord:  host->controllerRecordToggle();  break;
        case LkMini::kCcLoop:    host->controllerLoopToggle();    break;
        // 0x66 / 0x67 arrive only when Shift is held with the Track
        // ◀▶ buttons — the firmware translates "Shift+Track UP/DOWN"
        // into these CCs at the hardware level.  UP feels like "next
        // preset" to the user (scroll forward in the list), DOWN feels
        // like "previous" — so map opposite to numeric direction.
        case LkMini::kCcTrackL:  host->controllerPresetNext();    break;
        case LkMini::kCcTrackR:  host->controllerPresetPrev();    break;
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
    auto* sv = host->getSessionViewComponent();
    if (sv == nullptr) return;
    // Repaint each pad based on its visible clip's state — only push
    // changed colours so we don't flood the wire with redundant SysEx.
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
