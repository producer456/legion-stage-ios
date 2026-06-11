#include "IRigKeysIODebugView.h"
#include "IRigKeysIOController.h"

namespace
{
    // Mirror the controller's bindings so the panel injects exactly
    // what the real device emits.

    // Encoders & data wheel
    constexpr uint8_t kCcEncoderBase = 12;
    constexpr uint8_t kCcDataEncoder = 22;
    constexpr uint8_t kCcDataButton  = 23;

    // Transport CCs (the device's state-machine quirks)
    constexpr uint8_t kCcReset       = 111;
    constexpr uint8_t kCcFastForward = 114;
    constexpr uint8_t kCcRecordOff   = 116;
    constexpr uint8_t kCcStop        = 117;
    constexpr uint8_t kCcPlay        = 118;
    constexpr uint8_t kCcRecordOn    = 119;

    // Pads (channel 10, non-sequential note layout)
    constexpr uint8_t kPadNotes[8]   = { 36, 38, 40, 42, 46, 43, 47, 49 };
    constexpr int     kPadChannel    = 10;

    constexpr float kPxPerEncoderClick = 4.0f;

    // Encode an int8 delta as 7-bit two's complement.
    inline uint8_t encRel2c(int delta)
    {
        delta = juce::jlimit(-64, 63, delta);
        return static_cast<uint8_t>((delta < 0) ? (delta + 128) : delta);
    }
}

IRigKeysIODebugView::IRigKeysIODebugView(IRigKeysIOController& c)
    : controller(c)
{
    setOpaque(true);
    startTimerHz(20);
}

IRigKeysIODebugView::~IRigKeysIODebugView() = default;

void IRigKeysIODebugView::timerCallback() { repaint(); }
void IRigKeysIODebugView::resized()       { rebuildControls(); }

void IRigKeysIODebugView::rebuildControls()
{
    controls.clear();
    auto area = getLocalBounds().reduced(16);

    area.removeFromTop(36);   // header (drawn in paint())
    area.removeFromTop(8);

    // Helper for plain CC buttons (transport).
    auto addCcBtn = [&](juce::Rectangle<int> r, juce::String label, uint8_t cc)
    {
        const auto subStr = "CC " + juce::String(cc);
        controls.push_back({ r, std::move(label), subStr, Style::Button, cc,
                             [cc](juce::Point<int>) {
                                 return juce::MidiMessage::controllerEvent(1, cc, 127);
                             } });
    };

    // ── Encoder row (8 numbered + data wheel) ──
    {
        auto row = area.removeFromTop(80);
        area.removeFromTop(10);
        const int cols = 9;          // 8 encoders + 1 data wheel
        const int w    = row.getWidth() / cols;
        for (int i = 0; i < 8; ++i)
        {
            auto r  = row.removeFromLeft(w).reduced(3);
            const uint8_t cc = (uint8_t)(kCcEncoderBase + i);
            controls.push_back({ r, "K" + juce::String(i + 1),
                                 "CC " + juce::String(cc),
                                 Style::Encoder, cc, {} });
        }
        // Data wheel — wider, easier to drag.
        auto dataRect = row.removeFromLeft(w).reduced(3);
        controls.push_back({ dataRect, "DATA",
                             "CC " + juce::String(kCcDataEncoder),
                             Style::Encoder, kCcDataEncoder, {} });
    }

    // ── Data button push (small button under DATA encoder area) ──
    {
        auto row = area.removeFromTop(36);
        area.removeFromTop(10);
        // Tuck the push button to the far right, beneath the data encoder.
        auto right = row.removeFromRight(juce::jmin(120, row.getWidth() / 3))
                          .reduced(3);
        controls.push_back({ right, "DATA PUSH", "CC 23",
                             Style::EncoderPush, kCcDataButton,
                             [](juce::Point<int>) {
                                 return juce::MidiMessage::controllerEvent(1, kCcDataButton, 127);
                             } });
    }

    // ── Pad row (8 pads, single horizontal row) ──
    {
        auto row = area.removeFromTop(96);
        area.removeFromTop(10);
        const int w = row.getWidth() / 8;
        for (int i = 0; i < 8; ++i)
        {
            auto r = row.removeFromLeft(w).reduced(4);
            const uint8_t note = kPadNotes[i];
            controls.push_back({ r, juce::String(i + 1),
                                 "n" + juce::String(note),
                                 Style::Pad, note,
                                 [note](juce::Point<int>) {
                                     return juce::MidiMessage::noteOn(kPadChannel, note, (juce::uint8) 100);
                                 } });
        }
    }

    // ── Transport row ──
    {
        auto row = area.removeFromTop(54);
        area.removeFromTop(8);
        struct { const char* label; uint8_t cc; } tp[] = {
            { "RESET",    kCcReset       },
            { "REW/FF",   kCcFastForward },
            { "REC OFF",  kCcRecordOff   },
            { "STOP",     kCcStop        },
            { "PLAY",     kCcPlay        },
            { "REC ON",   kCcRecordOn    }
        };
        const int w = row.getWidth() / 6;
        for (auto& b : tp)
        {
            auto r = row.removeFromLeft(w).reduced(3);
            addCcBtn(r, b.label, b.cc);
        }
    }

    // Whatever's left is the MIDI log strip — drawn in paint().
}

void IRigKeysIODebugView::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff121518));

    // Header.
    {
        auto h = getLocalBounds().reduced(16).removeFromTop(36);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::Font::getDefaultSansSerifFontName(), 22.f, juce::Font::bold));
        g.drawText("iRIG KEYS I/O 49 — virtual surface", h.removeFromLeft(420),
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
                auto fill = juce::Colour(0xff232a31).interpolatedWith(juce::Colours::white, 0.20f * pulse);
                drawBase(c, fill, juce::Colours::white.withAlpha(0.25f));
                break;
            }
            case Style::EncoderPush:
            {
                auto fill = juce::Colour(0xff202830).interpolatedWith(juce::Colours::white, 0.20f * pulse);
                drawBase(c, fill, juce::Colours::white.withAlpha(0.3f));
                break;
            }
            case Style::Pad:
            {
                auto fill = juce::Colour(0xff20323f).interpolatedWith(juce::Colours::cyan, 0.45f * pulse);
                drawBase(c, fill, juce::Colour(0xff3aa9c2));
                break;
            }
            case Style::Encoder:
            {
                drawBase(c, juce::Colour(0xff1d2229), juce::Colours::white.withAlpha(0.35f));
                const auto centre = c.bounds.toFloat().getCentre();
                const float r = (float) juce::jmin(c.bounds.getWidth(), c.bounds.getHeight()) * 0.34f;
                const float accum = (c.id == kCcDataEncoder)
                    ? dataEncoderAccum
                    : encoderAccum[juce::jlimit(0, 7, (int)(c.id - kCcEncoderBase))];
                const float ang = std::fmod(accum * 0.4f, juce::MathConstants<float>::twoPi);
                g.setColour(juce::Colours::cyan.withAlpha(0.85f));
                juce::Line<float> ptr(centre,
                    centre.translated(std::sin(ang) * r, -std::cos(ang) * r));
                g.drawLine(ptr, 2.5f);
                break;
            }
        }
    }

    // ── Bottom: rolling MIDI log ──
    {
        const int stripH = 96;
        auto strip = getLocalBounds().removeFromBottom(stripH).reduced(16, 8);
        g.setColour(juce::Colours::black.withAlpha(0.85f));
        g.fillRoundedRectangle(strip.toFloat(), 6.0f);
        g.setColour(juce::Colours::limegreen);
        g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 12.f, juce::Font::plain));
        auto txt = controller.getLastMessages();
        if (txt.isEmpty())
            txt = "MIDI-IN: (no messages yet — tap a control on the device)";
        else
            txt = "MIDI-IN:\n" + txt;
        g.drawMultiLineText(txt, strip.getX() + 8, strip.getY() + 16, strip.getWidth() - 16);
    }
}

void IRigKeysIODebugView::mouseDown(const juce::MouseEvent& e)
{
    dragControlIdx = -1;
    for (int i = 0; i < (int) controls.size(); ++i)
    {
        if (! controls[i].bounds.contains(e.getPosition())) continue;
        const auto& c = controls[i];
        if (c.style == Style::Encoder)
        {
            dragControlIdx = i;
            lastDragPos = e.getPosition();
            return;
        }
        fireButton(c);
        lastFiredIdx = i; lastFiredAt = juce::Time::currentTimeMillis();
        return;
    }
}

void IRigKeysIODebugView::mouseDrag(const juce::MouseEvent& e)
{
    if (dragControlIdx < 0 || dragControlIdx >= (int) controls.size()) return;
    const auto& c = controls[dragControlIdx];
    if (c.style != Style::Encoder) return;

    const float dy = (float)(lastDragPos.getY() - e.getPosition().getY());
    lastDragPos = e.getPosition();
    float& accum = (c.id == kCcDataEncoder)
        ? dataEncoderAccum
        : encoderAccum[juce::jlimit(0, 7, (int)(c.id - kCcEncoderBase))];
    accum += dy;

    while (std::abs(accum) >= kPxPerEncoderClick)
    {
        const bool positive = accum > 0;
        accum += positive ? -kPxPerEncoderClick : +kPxPerEncoderClick;
        const uint8_t v = encRel2c(positive ? +1 : -1);
        controller.injectMessage(juce::MidiMessage::controllerEvent(1, c.id, v));
        lastFiredAt = juce::Time::currentTimeMillis();
    }
}

void IRigKeysIODebugView::fireButton(const Control& c)
{
    if (c.makeMessage)
        controller.injectMessage(c.makeMessage(juce::Point<int>{}));
}
