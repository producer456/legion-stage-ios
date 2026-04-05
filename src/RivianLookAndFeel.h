#pragma once

#include "IoniqLookAndFeel.h"

// "Rivian" theme — inspired by the Rivian R1S interior.
// Adventure-minimalist: Forest Edge green tones, sustainable birch wood,
// clean uncluttered design, nature-inspired palette.
class RivianLookAndFeel : public IoniqLookAndFeel
{
public:
    RivianLookAndFeel()
    {
        // ── Surfaces — Forest Edge dark green-grey ──
        theme.body        = 0xff1e2220;  // dark forest grey
        theme.bodyLight   = 0xff282c28;  // lighter panel
        theme.bodyDark    = 0xff121614;  // deep shadow
        theme.border      = 0xff3a4038;  // mossy border
        theme.borderLight = 0xff4a5048;  // lighter moss

        // ── Text — clean white ──
        theme.textPrimary   = 0xffe4e8e4;  // cool white
        theme.textSecondary = 0xff7a8278;  // sage grey
        theme.textBright    = 0xfff8fcf8;

        // ── Accent — Rivian compass yellow + forest green ──
        theme.red       = 0xffcc4444;
        theme.redDark   = 0xff2a1418;
        theme.amber     = 0xffdabc4a;  // Rivian compass yellow
        theme.amberDark = 0xff8a7a30;
        theme.green     = 0xff58a868;  // adventure green
        theme.greenDark = 0xff1a3018;

        // ── LCD — green-tinted OLED ──
        theme.lcdBg    = 0xff060a08;
        theme.lcdText  = 0xff78c888;   // soft green readout
        theme.lcdAmber = 0xff88d898;

        // ── Buttons ──
        theme.buttonFace  = 0xff222622;
        theme.buttonHover = 0xff2a2e2a;
        theme.buttonDown  = 0xff181c18;

        theme.btnStop = theme.btnMetronome = theme.btnCountIn = theme.btnNewClip = 0xff222622;
        theme.btnDeleteClip = 0xff2a1818;
        theme.btnDuplicate = theme.btnSplit = theme.btnQuantize = theme.btnEditNotes = 0xff222622;
        theme.btnNav = theme.btnSave = theme.btnLoad = theme.btnUndoRedo = theme.btnMidi2 = 0xff222622;
        theme.btnMetronomeOn = theme.btnCountInOn = theme.btnMidi2On = 0xff1e3020;
        theme.btnLoop = 0xff222622;
        theme.btnLoopOn = 0xff1e3020;
        theme.loopRegion = 0x2078c888;
        theme.loopBorder = 0xff78c888;

        // ── Timeline ──
        theme.timelineBg         = 0xff101410;
        theme.timelineAltRow     = 0xff161a16;
        theme.timelineSelectedRow = 0xff1e2e1e;
        theme.timelineGridMajor  = 0xff4a5048;
        theme.timelineGridMinor  = 0xff2a302a;
        theme.timelineGridFaint  = 0xff1a1e1a;
        theme.timelineGridBeat   = 0xff323832;

        // ── Clips ──
        theme.clipDefault   = 0xff283828;
        theme.clipRecording = 0xff3a2020;
        theme.clipQueued    = 0xff303420;
        theme.clipPlaying   = 0xff1e3e1e;

        // ── Playhead — compass yellow ──
        theme.playhead     = 0xdddabc4a;
        theme.playheadGlow = 0x22dabc4a;

        theme.accentStripe = 0xffdabc4a;

        theme.trackSelected = 0xff1e2e1e;
        theme.trackArmed    = 0xff1e3020;
        theme.trackMuteOn   = 0xff78c888;
        theme.trackSoloOn   = 0xffdabc4a;
        theme.trackSoloText = 0xff101410;

        applyThemeColors();

        setColour(juce::ComboBox::textColourId,                   juce::Colour(0xff78c888));
        setColour(juce::PopupMenu::backgroundColourId,            juce::Colour(0xff161a16));
        setColour(juce::PopupMenu::textColourId,                  juce::Colour(0xff78c888));
        setColour(juce::PopupMenu::highlightedTextColourId,       juce::Colour(0xff101410));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff78c888));

        setColour(juce::Slider::textBoxTextColourId,       juce::Colour(0xff78c888));
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff060a08));
        setColour(juce::Slider::textBoxOutlineColourId,    juce::Colour(0x00000000));

        setColour(juce::TextButton::buttonOnColourId,      juce::Colour(0xff1e3020));
        setColour(juce::TextButton::textColourOnId,        juce::Colour(0xff78c888));
    }

    // All OLED buttons
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour&,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);

        g.setColour(juce::Colour(0xff141814));
        g.fillRoundedRectangle(bounds, 4.0f);

        if (shouldDrawButtonAsDown)
            g.setColour(juce::Colour(0xff78c888).withAlpha(0.4f));
        else if (shouldDrawButtonAsHighlighted)
            g.setColour(juce::Colour(0xff3a4238));
        else
            g.setColour(juce::Colour(0xff2a302a));
        g.drawRoundedRectangle(bounds, 4.0f, 0.8f);
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool) override
    {
        auto text = button.getButtonText();
        bool on = button.getToggleState();
        juce::Colour bright(0xff78c888);
        juce::Colour dim(0xff3a5040);

        if (isOledAnimatedButton(text) || text == "REC" || text == "PANIC")
        {
            auto dispBounds = button.getLocalBounds().toFloat().reduced(2.0f);
            float t = static_cast<float>(juce::Time::getMillisecondCounterHiRes() * 0.001);
            juce::Image oled(juce::Image::ARGB, OLED_W, OLED_H, true);
            if (drawOledButtonArt(oled, text, on, t, bright, dim))
            { drawOledImage(g, oled, dispBounds); return; }
        }

        auto dispBounds = button.getLocalBounds().toFloat().reduced(2.0f);
        g.setColour(on ? bright : bright.withAlpha(0.7f));
        g.setFont(juce::Font(getUIFontName(), juce::jmin(12.0f, dispBounds.getHeight() * 0.5f), juce::Font::bold));
        g.drawText(formatButtonText(text), button.getLocalBounds().reduced(2), juce::Justification::centred);
    }

    bool drawOledButtonArt(juce::Image& oled, const juce::String& text,
                           bool on, float t, juce::Colour, juce::Colour) const override
    {
        juce::Colour bright(0xff78c888);
        juce::Colour dim(0xff3a5040);
        if (text == "REC")
        {
            juce::Colour col = on ? juce::Colour(0xffcc4444) : dim;
            float pulse = on ? (0.5f + 0.5f * std::sin(t * 8.0f)) : 1.0f;
            int icx = OLED_W / 2, icy = OLED_H / 2;
            oledCircle(oled, icx, icy, 3, col.withAlpha(pulse), true);
            if (on) { int r = 4 + (static_cast<int>(t * 6.0f) % 3); oledCircle(oled, icx, icy, r, juce::Colour(0xffcc4444).withAlpha(0.3f)); }
            return true;
        }
        return DawLookAndFeel::drawOledButtonArt(oled, text, on, t, bright, dim);
    }

    void drawComboBox(juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox&) override
    {
        auto bounds = juce::Rectangle<float>(0, 0, static_cast<float>(width), static_cast<float>(height));
        g.setColour(juce::Colour(0xff141814));
        g.fillRoundedRectangle(bounds, 4.0f);
        g.setColour(juce::Colour(0xff2a302a));
        g.drawRoundedRectangle(bounds, 4.0f, 0.8f);
        auto arrowZone = bounds.removeFromRight(20.0f).reduced(5.0f);
        juce::Path arrow;
        arrow.addTriangle(arrowZone.getX(), arrowZone.getCentreY() - 3,
                         arrowZone.getRight(), arrowZone.getCentreY() - 3,
                         arrowZone.getCentreX(), arrowZone.getCentreY() + 3);
        g.setColour(juce::Colour(0xff78c888));
        g.fillPath(arrow);
    }

    // Birch wood side panels
    int getSidePanelWidth() const override { return 8; }

    void drawSidePanels(juce::Graphics& g, int width, int height) override
    {
        if (width != sideCacheKey || height != sideCacheH)
        {
            int panelW = getSidePanelWidth();
            sideCache = juce::Image(juce::Image::ARGB, width, height, true);
            juce::Graphics sg(sideCache);
            juce::Colour birch(0xffc8b898);
            juce::Colour birchLight(0xffd8c8a8);
            juce::Colour birchGrain(0xffb0a078);

            sg.setColour(birch);
            sg.fillRect(0, 0, panelW, height);
            sg.fillRect(width - panelW, 0, panelW, height);

            juce::Random rng(42);
            for (int i = 0; i < 30; ++i)
            {
                float x = rng.nextFloat() * panelW;
                float grainW = 0.5f + rng.nextFloat() * 1.0f;
                float alpha = 0.06f + rng.nextFloat() * 0.12f;
                sg.setColour((rng.nextBool() ? birchLight : birchGrain).withAlpha(alpha));
                juce::Path grain;
                grain.startNewSubPath(x, 0);
                for (int y = 0; y < height; y += 20)
                    grain.lineTo(x + std::sin(static_cast<float>(y) * 0.012f + i * 0.6f) * 1.2f, static_cast<float>(y));
                grain.lineTo(x, static_cast<float>(height));
                sg.strokePath(grain, juce::PathStrokeType(grainW));
                auto rg = grain;
                rg.applyTransform(juce::AffineTransform::translation(static_cast<float>(width - panelW), 0));
                sg.strokePath(rg, juce::PathStrokeType(grainW));
            }

            sg.setColour(juce::Colour(0x30000000));
            sg.fillRect(static_cast<float>(panelW - 1), 0.0f, 1.0f, static_cast<float>(height));
            sg.fillRect(static_cast<float>(width - panelW), 0.0f, 1.0f, static_cast<float>(height));
            sideCacheKey = width; sideCacheH = height;
        }
        g.drawImageAt(sideCache, 0, 0);
    }

    void drawInnerStrip(juce::Graphics& g, int x, int, int width, int height) override
    {
        juce::Colour birch(0xffc8b898);
        juce::Colour birchGrain(0xffb0a078);
        g.setColour(birch);
        g.fillRect(x, 0, width, height);
        juce::Random rng(99);
        for (int i = 0; i < 30; ++i)
        {
            float fx = x + rng.nextFloat() * width;
            float yStart = rng.nextFloat() * height * 0.8f;
            float len = 30.0f + rng.nextFloat() * (height * 0.4f);
            g.setColour(birchGrain.withAlpha(0.1f + rng.nextFloat() * 0.15f));
            g.drawLine(fx, yStart, fx, yStart + len, 0.5f + rng.nextFloat() * 1.0f);
        }
        g.setColour(juce::Colour(0x30000000));
        g.drawVerticalLine(x, 0, static_cast<float>(height));
        g.drawVerticalLine(x + width - 1, 0, static_cast<float>(height));
    }

    void drawTopBarBackground(juce::Graphics& g, int x, int y, int width, int height) override
    {
        if (width != topBarCacheW || height != topBarCacheH)
        {
            topBarCache = juce::Image(juce::Image::RGB, width, height, false);
            juce::Graphics tg(topBarCache);
            tg.setColour(juce::Colour(0xff1e2220));
            tg.fillAll();
            tg.setColour(juce::Colour(0xff3a4038));
            tg.fillRect(0, height - 1, width, 1);
            topBarCacheW = width; topBarCacheH = height;
        }
        g.drawImageAt(topBarCache, x, y);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RivianLookAndFeel)
};
