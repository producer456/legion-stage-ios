#include "IOStation24cDebugView.h"
#include "IOStation24cController.h"

namespace
{
    // Mirror the IDs from the controller for use in the label/sub.  We
    // re-declare them here rather than including the controller's
    // anonymous namespace so the panel is self-contained.
    constexpr uint8_t kBtnArm        = 0x00;
    constexpr uint8_t kBtnBypass     = 0x03;
    constexpr uint8_t kBtnSolo       = 0x08;
    constexpr uint8_t kBtnMute       = 0x10;
    constexpr uint8_t kBtnFaderTouch = 0x68;
    constexpr uint8_t kBtnShift      = 0x46;

    constexpr uint8_t kBtnPrev       = 0x2E;
    constexpr uint8_t kBtnEncPush    = 0x20;
    constexpr uint8_t kBtnNext       = 0x2F;
    constexpr uint8_t kBtnLink       = 0x05;
    constexpr uint8_t kBtnPan        = 0x2A;
    constexpr uint8_t kBtnChannel    = 0x36;
    constexpr uint8_t kBtnScroll     = 0x38;
    constexpr uint8_t kBtnMaster     = 0x3A;
    constexpr uint8_t kBtnClick      = 0x3B;
    constexpr uint8_t kBtnSection    = 0x3C;
    constexpr uint8_t kBtnMarker     = 0x3D;

    constexpr uint8_t kBtnRead       = 0x4A;
    constexpr uint8_t kBtnWrite      = 0x4B;
    constexpr uint8_t kBtnTouchAuto  = 0x4D;

    constexpr uint8_t kBtnLoop       = 0x56;
    constexpr uint8_t kBtnRewind     = 0x5B;
    constexpr uint8_t kBtnFFwd       = 0x5C;
    constexpr uint8_t kBtnStop       = 0x5D;
    constexpr uint8_t kBtnPlay       = 0x5E;
    constexpr uint8_t kBtnRecord     = 0x5F;
    constexpr uint8_t kBtnFootswitch = 0x66;

    constexpr uint8_t kEncoderCC     = 0x3C;

    // px of vertical drag = 1 encoder click.  Keep low so a small
    // gesture still registers as motion in the host.
    constexpr float kPxPerEncoderClick = 4.0f;

    // Build a Note-On message on channel 1 (status 0x90) — that's
    // what the firmware sends + receives for buttons / LEDs.
    inline juce::MidiMessage btnPress(uint8_t id)
    {
        return juce::MidiMessage(0x90, id, (juce::uint8) 0x7F);
    }
    inline juce::MidiMessage btnRelease(uint8_t id)
    {
        return juce::MidiMessage(0x90, id, (juce::uint8) 0x00);
    }
}

IOStation24cDebugView::IOStation24cDebugView(IOStation24cController& c)
    : controller(c)
{
    setOpaque(true);
    startTimerHz(20);
}

IOStation24cDebugView::~IOStation24cDebugView() = default;

void IOStation24cDebugView::timerCallback()
{
    // Track the controller's fader value so the cap follows the
    // physical fader (or the controller's motor writeback during
    // automation playback) when we're not actively dragging.
    if (!fakeTouching)
        faderValue = controller.getFaderValue();
    repaint();
}

void IOStation24cDebugView::resized() { rebuildControls(); }

void IOStation24cDebugView::rebuildControls()
{
    controls.clear();
    auto area = getLocalBounds().reduced(16);

    // Reserve top strip for header — handled in paint(), not interactive.
    area.removeFromTop(36);
    area.removeFromTop(8);

    // Right-hand vertical fader column.
    auto faderColumn = area.removeFromRight(110);
    area.removeFromRight(12);
    {
        // Fader takes ~80% of column height; "TOUCH" tag below.
        const auto trackTop = faderColumn.getY() + 4;
        const auto trackBottom = faderColumn.getBottom() - 36;
        juce::Rectangle<int> faderRect(faderColumn.getX() + 28, trackTop,
                                       54, trackBottom - trackTop);
        controls.push_back({ faderRect, "FADER", "PB ch0",
                             Style::Fader, 0, {} });
        juce::Rectangle<int> touchRect(faderColumn.getX() + 6,
                                       trackBottom + 6, 96, 24);
        controls.push_back({ touchRect, fakeTouching ? "TOUCH ON" : "TOUCH",
                             "id 0x68", Style::Touch, kBtnFaderTouch, {} });
    }

    // Helper that adds a simple press-and-release Note-On button.
    auto addBtn = [&](juce::Rectangle<int> r, juce::String label,
                      uint8_t id, bool rgb = false)
    {
        const auto subStr = "id 0x" + juce::String::toHexString(id).paddedLeft('0', 2).toUpperCase();
        controls.push_back({ r, std::move(label), subStr,
                             rgb ? Style::RGBButton : Style::Button,
                             id,
                             [id](juce::Point<int>) { return btnPress(id); } });
    };

    // ── Channel Strip row ──
    {
        auto row = area.removeFromTop(46);
        area.removeFromTop(10);
        const struct { const char* l; uint8_t id; bool rgb; } cs[] = {
            { "ARM",   kBtnArm,    false }, { "BYPASS", kBtnBypass, false },
            { "SOLO",  kBtnSolo,   false }, { "MUTE",   kBtnMute,   false },
            { "SHIFT", kBtnShift,  false }
        };
        const int w = 86;
        for (auto& b : cs)
        {
            auto r = row.removeFromLeft(w).reduced(3);
            addBtn(r, b.l, b.id, b.rgb);
        }
    }

    // ── Session Navigator: mode row + Prev / Encoder / Next row ──
    {
        auto row = area.removeFromTop(46);
        area.removeFromTop(8);
        const struct { const char* l; uint8_t id; bool rgb; } nav[] = {
            { "MASTER",  kBtnMaster,  false }, { "PAN",   kBtnPan,     true  },
            { "CHANNEL", kBtnChannel, true  }, { "SCROLL",kBtnScroll,  true  },
            { "SECTION", kBtnSection, false }, { "MARKER",kBtnMarker,  false },
            { "CLICK",   kBtnClick,   false }, { "LINK",  kBtnLink,    true  }
        };
        const int w = juce::jmax(40, row.getWidth() / (int)std::size(nav));
        for (auto& b : nav)
        {
            auto r = row.removeFromLeft(w).reduced(3);
            addBtn(r, b.l, b.id, b.rgb);
        }
    }
    {
        auto row = area.removeFromTop(72);
        area.removeFromTop(12);
        auto prev = row.removeFromLeft(80).reduced(3);
        addBtn(prev, "PREV", kBtnPrev);
        auto enc  = row.removeFromLeft(110).reduced(3);
        controls.push_back({ enc, "ENC", "CC 0x3C", Style::Encoder, kEncoderCC, {} });
        auto push = row.removeFromLeft(80).reduced(3);
        controls.push_back({ push, "PUSH", "id 0x20", Style::EncoderPush,
                             kBtnEncPush,
                             [](juce::Point<int>) { return btnPress(kBtnEncPush); } });
        auto next = row.removeFromLeft(80).reduced(3);
        addBtn(next, "NEXT", kBtnNext);
    }

    // ── Automation row ──
    {
        auto row = area.removeFromTop(46);
        area.removeFromTop(10);
        const struct { const char* l; uint8_t id; bool rgb; } au[] = {
            { "READ",  kBtnRead,      true },
            { "WRITE", kBtnWrite,     true },
            { "TOUCH", kBtnTouchAuto, true }
        };
        const int w = 96;
        for (auto& b : au)
        {
            auto r = row.removeFromLeft(w).reduced(3);
            addBtn(r, b.l, b.id, b.rgb);
        }
    }

    // ── Transport row ──
    {
        auto row = area.removeFromTop(54);
        area.removeFromTop(8);
        const struct { const char* l; uint8_t id; } tp[] = {
            { "REW",  kBtnRewind }, { "FF",   kBtnFFwd   },
            { "STOP", kBtnStop   }, { "PLAY", kBtnPlay   },
            { "REC",  kBtnRecord }, { "LOOP", kBtnLoop   },
            { "FSW",  kBtnFootswitch }
        };
        const int w = 70;
        for (auto& b : tp)
        {
            auto r = row.removeFromLeft(w).reduced(3);
            addBtn(r, b.l, b.id);
        }
    }

    // Whatever's left at the bottom is the MIDI log — drawn in paint().
}

void IOStation24cDebugView::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff121518));

    // Header.
    {
        auto h = getLocalBounds().reduced(16).removeFromTop(36);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::Font::getDefaultSansSerifFontName(), 22.f, juce::Font::bold));
        g.drawText("ioSTATION 24c — virtual surface", h.removeFromLeft(420),
                   juce::Justification::centredLeft);

        const bool active = controller.isActive();
        g.setColour(active ? juce::Colours::limegreen : juce::Colours::orangered);
        g.setFont(juce::Font(14.f, juce::Font::bold));
        g.drawText(active ? "● MIDI port: connected" : "○ MIDI port: not detected",
                   h, juce::Justification::centredRight);
    }

    auto drawBase = [&](const Control& c, juce::Colour fill, juce::Colour border)
    {
        g.setColour(fill);
        g.fillRoundedRectangle(c.bounds.toFloat(), 6.f);
        g.setColour(border);
        g.drawRoundedRectangle(c.bounds.toFloat().reduced(0.5f), 6.f, 1.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(13.f, juce::Font::bold));
        g.drawText(c.label, c.bounds.reduced(4).removeFromTop(c.bounds.getHeight() - 14),
                   juce::Justification::centred);
        g.setColour(juce::Colours::white.withAlpha(0.55f));
        g.setFont(juce::Font(10.f));
        g.drawText(c.sub, c.bounds.reduced(4).removeFromBottom(12),
                   juce::Justification::centred);
    };

    const auto now = juce::Time::currentTimeMillis();
    for (int i = 0; i < (int) controls.size(); ++i)
    {
        const auto& c = controls[i];
        const bool justFired = (i == lastFiredIdx) && (now - lastFiredAt < 250);
        const float pulse = justFired
            ? juce::jlimit(0.0f, 1.0f, 1.f - (now - lastFiredAt) / 250.f)
            : 0.0f;

        switch (c.style)
        {
            case Style::Button:
            {
                auto fill = juce::Colour(0xff232a31).interpolatedWith(juce::Colours::white, 0.15f * pulse);
                drawBase(c, fill, juce::Colours::white.withAlpha(0.25f));
                break;
            }
            case Style::RGBButton:
            {
                auto base = juce::Colour::fromHSV(juce::jmin(0.95f, (c.id & 0x3F) / 64.f), 0.55f, 0.45f, 1.f);
                auto fill = base.interpolatedWith(juce::Colours::white, 0.30f * pulse);
                drawBase(c, fill, base.brighter(0.4f));
                break;
            }
            case Style::Touch:
            {
                auto fill = fakeTouching ? juce::Colour(0xff7a1f1f) : juce::Colour(0xff232a31);
                drawBase(c, fill, juce::Colours::white.withAlpha(0.25f));
                break;
            }
            case Style::EncoderPush:
            {
                auto fill = juce::Colour(0xff202830).interpolatedWith(juce::Colours::white, 0.15f * pulse);
                drawBase(c, fill, juce::Colours::white.withAlpha(0.3f));
                break;
            }
            case Style::Encoder:
            {
                drawBase(c, juce::Colour(0xff1d2229), juce::Colours::white.withAlpha(0.35f));
                // Pointer hint that wraps based on the accumulator so
                // the user gets visual feedback on rotation direction.
                const auto centre = c.bounds.toFloat().getCentre();
                const float r = (float) juce::jmin(c.bounds.getWidth(), c.bounds.getHeight()) * 0.34f;
                const float ang = std::fmod(encoderAccum * 0.4f, juce::MathConstants<float>::twoPi);
                g.setColour(juce::Colours::cyan.withAlpha(0.85f));
                juce::Line<float> ptr(centre,
                    centre.translated(std::sin(ang) * r, -std::cos(ang) * r));
                g.drawLine(ptr, 2.5f);
                break;
            }
            case Style::Fader:
            {
                // Track
                g.setColour(juce::Colour(0xff0c0f12));
                g.fillRoundedRectangle(c.bounds.toFloat(), 4.f);
                g.setColour(juce::Colours::white.withAlpha(0.2f));
                g.drawRoundedRectangle(c.bounds.toFloat().reduced(0.5f), 4.f, 1.f);
                // Cap — top-left of bounds is value=1.
                const float v = faderValue;
                const int capH = 22;
                const int capY = c.bounds.getY()
                    + (int) std::round((1.0f - v) * (c.bounds.getHeight() - capH));
                juce::Rectangle<int> cap(c.bounds.getX() - 6, capY,
                                         c.bounds.getWidth() + 12, capH);
                auto capCol = fakeTouching || controller.isFaderTouched()
                    ? juce::Colours::orange : juce::Colour(0xffd0d4d8);
                g.setColour(capCol);
                g.fillRoundedRectangle(cap.toFloat(), 4.f);
                g.setColour(juce::Colours::black);
                g.fillRect(cap.reduced(8, 9));
                // Label below
                g.setColour(juce::Colours::white.withAlpha(0.55f));
                g.setFont(juce::Font(10.f));
                g.drawText("PB ch0  " + juce::String((int) std::round(v * 16383)),
                           c.bounds.withY(c.bounds.getBottom() + 2)
                                   .withHeight(14)
                                   .expanded(20, 0),
                           juce::Justification::centred);
                break;
            }
        }
    }

    // ── Bottom: rolling MIDI log ──
    auto logArea = getLocalBounds().reduced(16);
    logArea.removeFromTop(36 + 8);                       // header
    logArea.removeFromRight(110 + 12);                   // fader column
    logArea.removeFromTop(46 + 10 + 46 + 8 + 72 + 12 + 46 + 10 + 54 + 8);
    if (logArea.getHeight() > 24)
    {
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillRoundedRectangle(logArea.toFloat(), 4.f);
        g.setColour(juce::Colours::limegreen);
        g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 12.f, juce::Font::plain));
        auto txt = controller.getLastMessages();
        if (txt.isEmpty()) txt = "(no MIDI yet — tap a button or move the fader)";
        g.drawMultiLineText(txt, logArea.getX() + 8, logArea.getY() + 16, logArea.getWidth() - 16);
    }
}

void IOStation24cDebugView::mouseDown(const juce::MouseEvent& e)
{
    dragControlIdx = -1;
    for (int i = 0; i < (int) controls.size(); ++i)
    {
        if (! controls[i].bounds.contains(e.getPosition())) continue;
        const auto& c = controls[i];
        switch (c.style)
        {
            case Style::Fader:
            {
                // Begin drag — simulate fader-touch press on the firmware's
                // port and start tracking the cap to the cursor.
                fakeTouching = true;
                controller.injectMessage(btnPress(kBtnFaderTouch));
                dragControlIdx = i;
                lastDragPos = e.getPosition();
                // Snap to click position.
                const float v = juce::jlimit(0.0f, 1.0f,
                    1.0f - (e.y - c.bounds.getY()) / (float) c.bounds.getHeight());
                faderValue = v;
                const int v14 = juce::jlimit(0, 16383, (int) std::round(v * 16383.0f));
                controller.injectMessage(juce::MidiMessage(0xE0,
                    (juce::uint8)(v14 & 0x7F), (juce::uint8)((v14 >> 7) & 0x7F)));
                lastFiredIdx = i; lastFiredAt = juce::Time::currentTimeMillis();
                return;
            }
            case Style::Encoder:
            {
                dragControlIdx = i;
                lastDragPos = e.getPosition();
                return;
            }
            case Style::Touch:
            {
                fakeTouching = !fakeTouching;
                controller.injectMessage(fakeTouching ? btnPress(kBtnFaderTouch)
                                                      : btnRelease(kBtnFaderTouch));
                lastFiredIdx = i; lastFiredAt = juce::Time::currentTimeMillis();
                rebuildControls();   // updates the "TOUCH ON" label
                return;
            }
            default:
                fireButton(c);
                lastFiredIdx = i; lastFiredAt = juce::Time::currentTimeMillis();
                return;
        }
    }
}

void IOStation24cDebugView::mouseDrag(const juce::MouseEvent& e)
{
    if (dragControlIdx < 0 || dragControlIdx >= (int) controls.size()) return;
    const auto& c = controls[dragControlIdx];
    switch (c.style)
    {
        case Style::Fader:
        {
            const float v = juce::jlimit(0.0f, 1.0f,
                1.0f - (e.y - c.bounds.getY()) / (float) c.bounds.getHeight());
            if (std::abs(v - faderValue) < 1e-4f) return;
            faderValue = v;
            const int v14 = juce::jlimit(0, 16383, (int) std::round(v * 16383.0f));
            controller.injectMessage(juce::MidiMessage(0xE0,
                (juce::uint8)(v14 & 0x7F), (juce::uint8)((v14 >> 7) & 0x7F)));
            break;
        }
        case Style::Encoder:
        {
            const float dy = (float)(lastDragPos.getY() - e.getPosition().getY());
            lastDragPos = e.getPosition();
            encoderAccum += dy;
            // Emit one click per kPxPerEncoderClick of accumulated drag.
            while (std::abs(encoderAccum) >= kPxPerEncoderClick)
            {
                const bool positive = encoderAccum > 0;
                encoderAccum += positive ? -kPxPerEncoderClick : +kPxPerEncoderClick;
                // Encoder firmware encoding: bit 7 = direction (1 = neg),
                // bits 0-6 = step count.  We send single-step events.
                const uint8_t v = positive ? 0x01 : (0x80 | 0x01);
                controller.injectMessage(juce::MidiMessage(0xB0,
                    (juce::uint8) kEncoderCC, (juce::uint8) v));
                lastFiredAt = juce::Time::currentTimeMillis();
            }
            break;
        }
        default: break;
    }
}

void IOStation24cDebugView::mouseUp(const juce::MouseEvent&)
{
    if (dragControlIdx >= 0 && dragControlIdx < (int) controls.size())
    {
        const auto& c = controls[dragControlIdx];
        if (c.style == Style::Fader)
        {
            // Release the simulated fader-touch.
            fakeTouching = false;
            controller.injectMessage(btnRelease(kBtnFaderTouch));
            rebuildControls();
        }
    }
    dragControlIdx = -1;
}

void IOStation24cDebugView::fireButton(const Control& c)
{
    if (c.makeMessage)
    {
        controller.injectMessage(c.makeMessage(juce::Point<int>{}));
        // Send a release shortly after — the firmware always pairs
        // press + release, and the controller's button handler acts
        // on press only, but routing release through too keeps the
        // log honest.
        juce::Component::SafePointer<IOStation24cDebugView> safe(this);
        const uint8_t id = c.id;
        juce::Timer::callAfterDelay(60, [safe, id] {
            if (auto* self = safe.getComponent())
                self->controller.injectMessage(btnRelease(id));
        });
    }
}
