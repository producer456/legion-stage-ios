#pragma once

#include "DawLookAndFeel.h"

// "Oxide" — workflow-first theme by Claude.
//
// Design philosophy: every visual choice serves the workflow.
// - Warm dark grey body: studio-grade, reduces eye strain in dim rooms
// - Three functional color channels:
//     AMBER  = transport / active states (play, loop, metronome)
//     CYAN   = selection / editing / navigation
//     RED    = recording / destructive actions / warnings
// - Menlo monospace for precise data reading (BPM, beat position, params)
// - No decorative panels — every pixel is content
// - High contrast where attention is needed, muted everywhere else
// - Inspired by the face of a Neve console at 2am
//
class OxideLookAndFeel : public DawLookAndFeel
{
public:
    OxideLookAndFeel()
    {
        // ── Body — warm dark grey (like brushed steel rack gear) ──
        theme.body        = 0xff1c1c1e;
        theme.bodyLight   = 0xff262628;
        theme.bodyDark    = 0xff121214;
        theme.border      = 0xff3a3a3e;
        theme.borderLight = 0xff4a4a50;

        // ── Text — warm off-white, never pure white (reduces glare) ──
        theme.textPrimary   = 0xffd8d4cc;
        theme.textSecondary = 0xff7a7874;
        theme.textBright    = 0xfff0ece4;

        // ── Functional color channels ──
        // Amber = transport / active
        theme.amber     = 0xffe8a030;  // warm amber (VU meter needle)
        theme.amberDark = 0xff8a6020;

        // Cyan = selection / editing
        theme.green     = 0xff48b8c8;  // teal-cyan for selection
        theme.greenDark = 0xff103038;

        // Red = recording / destructive
        theme.red       = 0xffdc3838;
        theme.redDark   = 0xff2a1014;

        // ── LCD — dark with amber readout (like vintage gear) ──
        theme.lcdBg    = 0xff000000;
        theme.lcdText  = 0xffe8a030;   // amber — the numbers you watch
        theme.lcdAmber = 0xfff0b840;

        // ── Buttons — functional grouping by color ──
        theme.buttonFace  = 0xff1e1e20;
        theme.buttonHover = 0xff282828;
        theme.buttonDown  = 0xff141416;

        // Transport buttons — amber when active
        theme.btnStop       = 0xff1e1e20;
        theme.btnMetronome  = 0xff1e1e20;
        theme.btnMetronomeOn = 0xff302010;  // warm amber tint
        theme.btnCountIn    = 0xff1e1e20;
        theme.btnCountInOn  = 0xff302010;
        theme.btnLoop       = 0xff1e1e20;
        theme.btnLoopOn     = 0xff302010;

        // Editing buttons — cyan-tinted when relevant
        theme.btnNewClip    = 0xff1e1e20;
        theme.btnDuplicate  = 0xff1e1e20;
        theme.btnSplit      = 0xff1e1e20;
        theme.btnQuantize   = 0xff1e1e20;
        theme.btnEditNotes  = 0xff1e1e20;
        theme.btnNav        = 0xff1e1e20;
        theme.btnSave       = 0xff1e1e20;
        theme.btnLoad       = 0xff1e1e20;
        theme.btnUndoRedo   = 0xff1e1e20;
        theme.btnMidi2      = 0xff1e1e20;
        theme.btnMidi2On    = 0xff103038;  // cyan tint

        // Destructive — red-tinted
        theme.btnDeleteClip = 0xff241418;

        theme.loopRegion = 0x20e8a030;  // amber translucent
        theme.loopBorder = 0xffe8a030;

        // ── Timeline — high contrast for navigation ──
        theme.timelineBg         = 0xff141416;
        theme.timelineAltRow     = 0xff1a1a1c;
        theme.timelineSelectedRow = 0xff182830;  // cyan selection
        theme.timelineGridMajor  = 0xff3a3a40;   // visible but not distracting
        theme.timelineGridMinor  = 0xff262628;
        theme.timelineGridFaint  = 0xff1e1e20;
        theme.timelineGridBeat   = 0xff303034;

        // ── Clips — color-coded by state ──
        theme.clipDefault   = 0xff283038;  // neutral steel
        theme.clipRecording = 0xff3a1818;  // red = recording
        theme.clipQueued    = 0xff302810;  // amber = queued
        theme.clipPlaying   = 0xff207050;  // visible cyan-green

        // ── Playhead — bright amber needle ──
        theme.playhead     = 0xeee8a030;
        theme.playheadGlow = 0x28e8a030;

        theme.accentStripe = 0xffe8a030;

        // ── Track controls — functional colors ──
        theme.trackSelected = 0xff182830;  // cyan selection
        theme.trackArmed    = 0xff2a1014;  // red armed
        theme.trackMuteOn   = 0xffdc3838;  // red mute = danger, audio gone
        theme.trackSoloOn   = 0xffe8a030;  // amber solo = listen to this
        theme.trackSoloText = 0xff121214;

        applyThemeColors();

        // Amber text in displays
        setColour(juce::ComboBox::textColourId,                   juce::Colour(0xffd8d4cc));
        setColour(juce::PopupMenu::backgroundColourId,            juce::Colour(0xff1a1a1c));
        setColour(juce::PopupMenu::textColourId,                  juce::Colour(0xffd8d4cc));
        setColour(juce::PopupMenu::highlightedTextColourId,       juce::Colour(0xff121214));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xffe8a030));

        setColour(juce::Slider::textBoxTextColourId,       juce::Colour(0xffe8a030));
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff000000));
        setColour(juce::Slider::textBoxOutlineColourId,    juce::Colour(0x00000000));

        setColour(juce::TextButton::buttonOnColourId,      juce::Colour(0xff302010));
        setColour(juce::TextButton::textColourOnId,        juce::Colour(0xffe8a030));
    }

    // Compact radius — efficient, no wasted space
    float getButtonRadius() const override { return 2.0f; }

    // Monospace for precision
    juce::String getUIFontName() const override { return "Menlo"; }

    // No decorative panels — all content
    int getSidePanelWidth() const override { return 0; }

    // VU meter gradient top bar
    void drawTopBarBackground(juce::Graphics& g, int x, int y, int width, int height) override
    {
        if (width != topBarCacheW || height != topBarCacheH)
        {
            topBarCache = juce::Image(juce::Image::RGB, width, height, false);
            juce::Graphics tg(topBarCache);

            // Subtle warm gradient — darker at top, slightly lighter at bottom
            tg.setGradientFill(juce::ColourGradient(
                juce::Colour(0xff1a1a1c), 0, 0,
                juce::Colour(0xff222224), 0, static_cast<float>(height), false));
            tg.fillAll();

            // Subtle horizontal rule at bottom — like a console channel strip divider
            tg.setColour(juce::Colour(0xff3a3a3e));
            tg.fillRect(0, height - 1, width, 1);

            // Very faint amber warmth at bottom edge — like VU backlight bleed
            tg.setColour(juce::Colour(0xffe8a030).withAlpha(0.03f));
            tg.fillRect(0, height - 3, width, 3);

            topBarCacheW = width;
            topBarCacheH = height;
        }
        g.drawImageAt(topBarCache, x, y);
    }

    void invalidateCaches() override { topBarCacheW = 0; topBarCacheH = 0; }

    mutable juce::Image topBarCache;
    mutable int topBarCacheW = 0, topBarCacheH = 0;

    // Clean functional buttons — thin border, no decoration
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour&,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);

        // Flat dark fill
        g.setColour(juce::Colour(0xff1a1a1c));
        g.fillRoundedRectangle(bounds, 2.0f);

        // Thin functional border
        if (shouldDrawButtonAsDown)
            g.setColour(juce::Colour(0xffe8a030).withAlpha(0.6f));  // amber press
        else if (shouldDrawButtonAsHighlighted)
            g.setColour(juce::Colour(0xff4a4a50));
        else
            g.setColour(juce::Colour(0xff303034));
        g.drawRoundedRectangle(bounds, 2.0f, 0.8f);
    }

    // Amber OLED art
    bool drawOledButtonArt(juce::Image& oled, const juce::String& text,
                           bool on, float t, juce::Colour, juce::Colour) const override
    {
        juce::Colour bright(0xffe8a030);  // amber
        juce::Colour dim(0xff504020);

        if (text == "REC")
        {
            juce::Colour col = on ? juce::Colour(0xffdc3838) : dim;
            float pulse = on ? (0.5f + 0.5f * std::sin(t * 8.0f)) : 1.0f;
            int icx = OLED_W / 2, icy = OLED_H / 2;
            oledCircle(oled, icx, icy, 3, col.withAlpha(pulse), true);
            if (on)
            {
                int r = 4 + (static_cast<int>(t * 6.0f) % 3);
                oledCircle(oled, icx, icy, r, juce::Colour(0xffdc3838).withAlpha(0.3f));
            }
            return true;
        }

        return DawLookAndFeel::drawOledButtonArt(oled, text, on, t, bright, dim);
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool) override
    {
        auto text = button.getButtonText();
        bool on = button.getToggleState();

        juce::Colour bright(0xffe8a030);
        juce::Colour dim(0xff504020);

        if (isOledAnimatedButton(text) || text == "REC" || text == "PANIC")
        {
            auto dispBounds = button.getLocalBounds().toFloat().reduced(2.0f);
            float t = static_cast<float>(juce::Time::getMillisecondCounterHiRes() * 0.001);
            juce::Image oled(juce::Image::ARGB, OLED_W, OLED_H, true);
            if (drawOledButtonArt(oled, text, on, t, bright, dim))
            {
                drawOledImage(g, oled, dispBounds);
                return;
            }
        }

        // Default text — warm off-white, not amber (amber is for active/data)
        auto dispBounds = button.getLocalBounds().toFloat().reduced(2.0f);
        g.setColour(on ? juce::Colour(0xffe8a030) : juce::Colour(0xffd8d4cc).withAlpha(0.8f));
        g.setFont(juce::Font(getUIFontName(),
                   juce::jmin(11.0f, dispBounds.getHeight() * 0.45f), juce::Font::bold));
        g.drawText(formatButtonText(text), button.getLocalBounds().reduced(2),
                   juce::Justification::centred);
    }

    // Clean combo box
    void drawComboBox(juce::Graphics& g, int width, int height, bool,
                      int, int, int, int, juce::ComboBox&) override
    {
        auto bounds = juce::Rectangle<float>(0, 0, static_cast<float>(width), static_cast<float>(height));

        g.setColour(juce::Colour(0xff141416));
        g.fillRoundedRectangle(bounds, 2.0f);
        g.setColour(juce::Colour(0xff303034));
        g.drawRoundedRectangle(bounds, 2.0f, 0.8f);

        auto arrowZone = bounds.removeFromRight(18.0f).reduced(4.0f);
        juce::Path arrow;
        arrow.addTriangle(arrowZone.getX(), arrowZone.getCentreY() - 3,
                         arrowZone.getRight(), arrowZone.getCentreY() - 3,
                         arrowZone.getCentreX(), arrowZone.getCentreY() + 3);
        g.setColour(juce::Colour(0xffd8d4cc));
        g.fillPath(arrow);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OxideLookAndFeel)
};
