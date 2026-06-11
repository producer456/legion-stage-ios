#include "KeyLab88DebugView.h"
#include "KeyLab88Mk2Controller.h"

namespace
{
    // Standard MCU bindings — match what the KL88 actually emits in
    // DAW=MCU mode.  Faders are pitch-bend per channel; encoders /
    // big-knob are signed-bit relative on CCs.

    // CCs
    constexpr uint8_t kCcEncoderBase= 16;       // encoders 1-8 → CCs 16..23
    constexpr uint8_t kCcMasterEnc  = 24;       // master/9th encoder
    constexpr uint8_t kCcJogRot     = 60;       // big-knob rotate

    // Notes (channel 1)
    constexpr uint8_t kNoteSelectBase = 24;     // select buttons under faders → 24..31
    constexpr uint8_t kNoteJogPush    = 84;     // big-knob push
    constexpr uint8_t kNoteTrackDn    = 46;
    constexpr uint8_t kNoteTrackUp    = 47;
    constexpr uint8_t kNoteBankDn     = 48;
    constexpr uint8_t kNoteBankUp     = 49;
    constexpr uint8_t kNotePrevPage   = 98;
    constexpr uint8_t kNoteNextPage   = 99;

    constexpr uint8_t kPadNotes[16] = {
        48, 49, 50, 51,   // top row
        44, 45, 46, 47,
        40, 41, 42, 43,
        36, 37, 38, 39    // bottom row
    };

    constexpr int kPxPerEncoderClick = 4;   // ~4 px of vertical drag = 1 click
}

KeyLab88DebugView::KeyLab88DebugView(KeyLab88Mk2Controller& c) : controller(c)
{
    setOpaque(true);
    startTimerHz(15);
}

KeyLab88DebugView::~KeyLab88DebugView() = default;

void KeyLab88DebugView::timerCallback() { repaint(); }

void KeyLab88DebugView::resized() { rebuildControls(); }

void KeyLab88DebugView::rebuildControls()
{
    controls.clear();
    auto area = getLocalBounds().reduced(16);

    auto addButton = [&](juce::Rectangle<int> r, juce::String label,
                         juce::String sub,
                         std::function<juce::MidiMessage(juce::Point<int>)> mk)
    {
        controls.push_back({ r, std::move(label), std::move(sub),
                             Style::Button, -1, 0, std::move(mk) });
    };

    // ── Top strip ─────────────────────────────────────────────────
    auto top = area.removeFromTop(56);
    area.removeFromTop(10);

    // Transport
    {
        const struct { const char* label; uint8_t note; } row[] = {
            { "REW",  91 }, { "FF",   92 }, { "STOP", 93 },
            { "PLAY", 94 }, { "REC",  95 }, { "LOOP", 86 }
        };
        const int w = 56;
        for (auto& b : row)
        {
            auto r = top.removeFromLeft(w).reduced(2);
            const uint8_t n = b.note;
            addButton(r, b.label, "n" + juce::String(n),
                      [n](juce::Point<int>) { return juce::MidiMessage::noteOn(1, n, (juce::uint8) 100); });
        }
    }
    top.removeFromLeft(20);

    // DAW cluster
    {
        const struct { const char* label; uint8_t note; } row[] = {
            { "SAVE",  80 }, { "UNDO",  81 }, { "METRO", 89 },
            { "IN",    87 }, { "OUT",   88 }, { "READ",  74 }, { "WRITE", 75 },
            { "SOLO",   8 }, { "MUTE",  16 }, { "REC",    0 }
        };
        const int w = 50;
        for (auto& b : row)
        {
            auto r = top.removeFromLeft(w).reduced(2);
            const uint8_t n = b.note;
            addButton(r, b.label, "n" + juce::String(n),
                      [n](juce::Point<int>) { return juce::MidiMessage::noteOn(1, n, (juce::uint8) 100); });
        }
    }

    // ── Middle strip ──────────────────────────────────────────────
    auto mid = area.removeFromTop(70);
    area.removeFromTop(10);

    // Bank / track navigation row — these are notes in MCU mode, not CCs.
    {
        const struct { const char* label; uint8_t note; } row[] = {
            { "BANK-",  kNoteBankDn  }, { "BANK+",  kNoteBankUp  },
            { "TRK-",   kNoteTrackDn }, { "TRK+",   kNoteTrackUp },
            { "PG-",    kNotePrevPage}, { "PG+",    kNoteNextPage}
        };
        const int w = 60;
        for (auto& b : row)
        {
            auto r = mid.removeFromLeft(w).reduced(2);
            const uint8_t n = b.note;
            addButton(r, b.label, "n" + juce::String(n),
                      [n](juce::Point<int>) { return juce::MidiMessage::noteOn(1, n, (juce::uint8) 100); });
        }
    }
    mid.removeFromLeft(20);

    // Big knob — rotation = CC 60 signed-bit relative; push = note 84.
    {
        auto r = mid.removeFromLeft(70).reduced(2);
        controls.push_back({ r, "JOG", "CC 60", Style::BigKnob, 0, kCcJogRot, {} });
        auto p = mid.removeFromLeft(60).reduced(2);
        addButton(p, "PUSH", "n84",
                  [](juce::Point<int>) { return juce::MidiMessage::noteOn(1, kNoteJogPush, (juce::uint8) 100); });
    }

    // ── Bottom-left: pads ─────────────────────────────────────────
    auto bottom = area;
    auto pads = bottom.removeFromLeft(220);
    bottom.removeFromLeft(20);
    {
        const int colW = pads.getWidth()  / 4;
        const int rowH = pads.getHeight() / 4;
        for (int row = 0; row < 4; ++row)
        {
            for (int col = 0; col < 4; ++col)
            {
                auto r = juce::Rectangle<int>(pads.getX() + col * colW,
                                              pads.getY() + row * rowH,
                                              colW, rowH).reduced(3);
                const uint8_t note = kPadNotes[row * 4 + col];
                controls.push_back({ r, juce::String(row * 4 + col + 1),
                                     "n" + juce::String(note), Style::Pad, -1, 0,
                                     [note](juce::Point<int>) {
                                         return juce::MidiMessage::noteOn(10, note, (juce::uint8) 100);
                                     } });
            }
        }
    }

    // ── Bottom-right: encoder / fader / select per column ─────────
    {
        const int colW = bottom.getWidth() / 9;
        const int encH = 60;
        const int selH = 28;
        const int fadH = bottom.getHeight() - encH - selH - 16;

        for (int i = 0; i < 9; ++i)
        {
            auto col = juce::Rectangle<int>(bottom.getX() + i * colW,
                                            bottom.getY(), colW,
                                            bottom.getHeight()).reduced(3, 0);

            // Encoders 1-8 = CC 16..23.  Index 8 (master) = CC 24.
            const uint8_t encCc = (i < 8) ? (uint8_t)(kCcEncoderBase + i) : kCcMasterEnc;
            auto enc = col.removeFromTop(encH);
            controls.push_back({ enc, "E" + juce::String(i + 1),
                                 "CC " + juce::String(encCc),
                                 Style::Encoder, i, encCc, {} });
            col.removeFromTop(6);

            // Faders 1-8 = pitch-bend on channels 1..8.  Index 8 (master)
            // = pitch-bend channel 9.  No CC# applies; we store kindIndex
            // and the interaction code emits 0xE0 + kindIndex directly.
            auto fad = col.removeFromTop(fadH);
            controls.push_back({ fad, "F" + juce::String(i + 1),
                                 "PB ch" + juce::String(i + 1),
                                 Style::Fader, i, /*cc=*/0, {} });
            col.removeFromTop(6);

            // Select buttons under each fader = notes 24..31.  No 9th
            // select button on the device; index 8 stays empty.
            auto sel = col.removeFromTop(selH);
            if (i < 8)
            {
                const uint8_t selNote = (uint8_t)(kNoteSelectBase + i);
                controls.push_back({ sel, "S" + juce::String(i + 1),
                                     "n" + juce::String(selNote),
                                     Style::SelectBtn, i, selNote,
                                     [selNote](juce::Point<int>) {
                                         return juce::MidiMessage::noteOn(1, selNote, (juce::uint8) 100);
                                     } });
            }
        }
    }
}

// ── Drawing ──────────────────────────────────────────────────────
void KeyLab88DebugView::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xfff5f3ee));
    g.setColour(juce::Colour(0xff3a3a3a));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(4.0f), 12.0f, 1.5f);

    g.setColour(juce::Colour(0xff1a3a8a));
    g.setFont(juce::Font("Avenir Next", 13.0f, juce::Font::bold));
    g.drawText("KeyLab 88 MkII — virtual debug surface (taps inject MIDI)",
               getLocalBounds().reduced(20, 0).withHeight(20),
               juce::Justification::topLeft);

    const auto now = juce::Time::currentTimeMillis();

    for (size_t i = 0; i < controls.size(); ++i)
    {
        const auto& c = controls[i];
        const float age      = (float) juce::jmin<juce::int64>(400, now - lastFiredAt);
        const bool  flashing = ((int) i == lastFiredIdx) && (now - lastFiredAt < 400);
        const float flashAmt = flashing ? (1.0f - age / 400.0f) : 0.0f;

        juce::Colour fill, stroke, txt;
        switch (c.style)
        {
            case Style::Pad:
                fill   = juce::Colour(0xffe8e4dc).interpolatedWith(juce::Colour(0xff5cd87a), flashAmt);
                stroke = juce::Colour(0xff8a8478);
                txt    = juce::Colour(0xff1a1816);
                break;
            case Style::Button:
                fill   = juce::Colour(0xffe8e4d8).interpolatedWith(juce::Colour(0xff264a9a), flashAmt);
                stroke = juce::Colour(0xff5a5854);
                txt    = juce::Colour(0xff1a1816);
                break;
            case Style::Encoder:
            case Style::BigKnob:
                fill   = juce::Colour(0xffd8d4ca).interpolatedWith(juce::Colour(0xff264a9a), flashAmt);
                stroke = juce::Colour(0xff5a5854);
                txt    = juce::Colour(0xff1a1816);
                break;
            case Style::Fader:
                fill   = juce::Colour(0xffe8e4dc);
                stroke = juce::Colour(0xff8a8478);
                txt    = juce::Colour(0xff1a1816);
                break;
            case Style::SelectBtn:
                fill   = juce::Colour(0xff1a3a8a).interpolatedWith(juce::Colour(0xffe8efff), flashAmt);
                stroke = juce::Colour(0xff264a9a);
                txt    = juce::Colour(0xffe8efff);
                break;
        }

        const auto rf = c.bounds.toFloat();

        if (c.style == Style::Encoder || c.style == Style::BigKnob)
        {
            // Knob body.
            const auto cell = c.bounds;
            const int diam = juce::jmin(cell.getWidth(), cell.getHeight()) - 6;
            auto knob = juce::Rectangle<int>(0, 0, diam, diam).withCentre(cell.getCentre());
            g.setColour(fill);
            g.fillEllipse(knob.toFloat());
            g.setColour(stroke);
            g.drawEllipse(knob.toFloat(), 1.2f);

            // Pointer line — wraps every 30 ticks (≈ a full visible turn).
            const int ticks = (c.style == Style::BigKnob)
                            ? bigKnobTicks
                            : (c.kindIndex >= 0 ? encoderTicks[c.kindIndex] : 0);
            const float angle = juce::MathConstants<float>::twoPi
                              * ((float) (((ticks % 30) + 30) % 30) / 30.0f)
                              - juce::MathConstants<float>::halfPi;
            const float r = diam * 0.4f;
            const auto centre = knob.getCentre().toFloat();
            const auto tip = centre + juce::Point<float>(std::cos(angle) * r, std::sin(angle) * r);
            g.setColour(juce::Colour(0xff1a3a8a));
            g.drawLine(juce::Line<float>(centre, tip), 2.0f);
        }
        else if (c.style == Style::Fader)
        {
            // Slot.
            g.setColour(fill);
            g.fillRoundedRectangle(rf, 4.0f);
            g.setColour(stroke);
            g.drawRoundedRectangle(rf, 4.0f, 0.8f);

            // Cap at current value.
            const float v = c.kindIndex >= 0 ? faderValue[c.kindIndex] : 0.0f;
            const int capH = 14;
            const int y = c.bounds.getBottom() - capH
                        - (int) ((c.bounds.getHeight() - capH) * juce::jlimit(0.0f, 1.0f, v));
            auto cap = juce::Rectangle<int>(c.bounds.getX() + 3, y,
                                            c.bounds.getWidth() - 6, capH);
            g.setColour(juce::Colour(0xff264a9a));
            g.fillRoundedRectangle(cap.toFloat(), 3.0f);
        }
        else
        {
            g.setColour(fill);
            g.fillRoundedRectangle(rf, 4.0f);
            g.setColour(stroke);
            g.drawRoundedRectangle(rf, 4.0f, 0.8f);
        }

        // Labels.
        g.setColour(txt);
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText(c.label, c.bounds.reduced(2),
                   c.style == Style::Fader ? juce::Justification::centredTop
                                           : juce::Justification::centred);
        if (c.sub.isNotEmpty())
        {
            g.setFont(juce::Font(9.0f));
            g.setColour(txt.withAlpha(0.7f));
            g.drawText(c.sub, c.bounds.reduced(2), juce::Justification::centredBottom);
        }
    }

    // ── Live MIDI-in tail (bottom strip) ──────────────────────────
    // Strictly informational: shows the most recent N hex-formatted
    // messages the controller has seen so we can capture what physical
    // buttons emit (e.g. Cat / Preset on this firmware) without needing
    // a separate inspector overlay.
    {
        const int stripH = 88;
        auto strip = getLocalBounds().removeFromBottom(stripH).reduced(20, 4);
        g.setColour(juce::Colours::black.withAlpha(0.85f));
        g.fillRoundedRectangle(strip.toFloat(), 6.0f);
        g.setColour(juce::Colours::limegreen);
        g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));
        auto txt = controller.getLastMessages();
        if (txt.isEmpty())
            txt = "MIDI-IN: (no messages yet — tap a control on the device)";
        else
            txt = "MIDI-IN:\n" + txt;
        g.drawMultiLineText(txt, strip.getX() + 8, strip.getY() + 16, strip.getWidth() - 16);
    }
}

// ── Interaction ──────────────────────────────────────────────────
void KeyLab88DebugView::mouseDown(const juce::MouseEvent& e)
{
    dragControlIdx = -1;
    for (size_t i = 0; i < controls.size(); ++i)
    {
        if (!controls[i].bounds.contains(e.getPosition())) continue;
        const auto& c = controls[i];
        lastFiredIdx = (int) i;
        lastFiredAt  = juce::Time::currentTimeMillis();

        if (c.style == Style::Fader)
        {
            const float v = juce::jlimit(0.0f, 1.0f,
                (float)(c.bounds.getBottom() - e.getPosition().getY())
                / (float) c.bounds.getHeight());
            if (c.kindIndex >= 0) faderValue[c.kindIndex] = v;
            // Faders are pitch-bend per channel.  kindIndex 0..7 → ch 1..8;
            // kindIndex 8 → ch 9 (master).
            const int channel = c.kindIndex + 1;
            controller.injectMessage(juce::MidiMessage::pitchWheel(
                channel, juce::jlimit(0, 16383, (int)(v * 16383.0f))));
            dragControlIdx = (int) i;
            lastDragPos    = e.getPosition();
        }
        else if (c.style == Style::Encoder || c.style == Style::BigKnob)
        {
            // Initial tap = single click in the direction of the click position.
            const bool fwd = e.getPosition().getX() > c.bounds.getCentreX();
            controller.injectMessage(juce::MidiMessage::controllerEvent(1, c.cc, fwd ? 0x01 : 0x41));
            if (c.style == Style::BigKnob) bigKnobTicks += fwd ? +1 : -1;
            else if (c.kindIndex >= 0)     encoderTicks[c.kindIndex] += fwd ? +1 : -1;
            dragControlIdx = (int) i;
            lastDragPos    = e.getPosition();
        }
        else if (c.makeMessage)
        {
            controller.injectMessage(c.makeMessage(e.getPosition()));
        }
        repaint();
        return;
    }
}

void KeyLab88DebugView::mouseDrag(const juce::MouseEvent& e)
{
    if (dragControlIdx < 0 || dragControlIdx >= (int) controls.size()) return;
    const auto& c = controls[(size_t) dragControlIdx];

    if (c.style == Style::Fader)
    {
        const float v = juce::jlimit(0.0f, 1.0f,
            (float)(c.bounds.getBottom() - e.getPosition().getY())
            / (float) c.bounds.getHeight());
        if (c.kindIndex >= 0) faderValue[c.kindIndex] = v;
        const int channel = c.kindIndex + 1;
        controller.injectMessage(juce::MidiMessage::pitchWheel(
            channel, juce::jlimit(0, 16383, (int)(v * 16383.0f))));
        lastFiredIdx = dragControlIdx;
        lastFiredAt  = juce::Time::currentTimeMillis();
        return;
    }

    if (c.style == Style::Encoder || c.style == Style::BigKnob)
    {
        // Vertical drag: up = forward, down = back. Each kPxPerEncoderClick px = 1 click.
        const int dy     = lastDragPos.getY() - e.getPosition().getY();
        const int clicks = dy / kPxPerEncoderClick;
        if (clicks == 0) return;
        const uint8_t value = clicks > 0 ? 0x01 : 0x41;
        const int n = std::abs(clicks);
        for (int k = 0; k < n; ++k)
            controller.injectMessage(juce::MidiMessage::controllerEvent(1, c.cc, value));

        if (c.style == Style::BigKnob) bigKnobTicks += clicks;
        else if (c.kindIndex >= 0)     encoderTicks[c.kindIndex] += clicks;

        // Anchor lastDragPos by the consumed pixels so leftover doesn't pile up.
        lastDragPos = e.getPosition().translated(0, clicks * kPxPerEncoderClick);
        lastFiredIdx = dragControlIdx;
        lastFiredAt  = juce::Time::currentTimeMillis();
    }
}
