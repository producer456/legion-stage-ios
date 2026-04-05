#pragma once

#include "DawLookAndFeel.h"

// "Alcantara" theme — inspired by the Alcantara fabric and leather trim
// found in luxury car interiors (Porsche, Lamborghini, BMW M).
// Deep charcoal suede body, brushed aluminium accents, amber instrument
// cluster lighting, subtle perforated texture. Feels like sitting in
// a cockpit at night with only the gauges glowing.
class AlcantaraLookAndFeel : public DawLookAndFeel
{
public:
    AlcantaraLookAndFeel()
    {
        // ── Surfaces — deep charcoal suede with warm undertone ──
        theme.body        = 0xff1a1a1e;  // charcoal Alcantara
        theme.bodyLight   = 0xff252528;  // lighter suede panel
        theme.bodyDark    = 0xff101014;  // deep shadow
        theme.border      = 0xff3a3a40;  // brushed aluminium edge
        theme.borderLight = 0xff505058;  // polished trim highlight

        // ── Text — warm white like instrument cluster ──
        theme.textPrimary   = 0xffe8e4de;  // warm parchment white
        theme.textSecondary = 0xff7a7878;  // suede grey
        theme.textBright    = 0xfffaf6f0;  // bright gauge white

        // ── Accent — amber instrument lighting + red sport accents ──
        theme.red       = 0xffc43030;  // sport red (like brake calipers)
        theme.redDark   = 0xff2a1218;  // deep burgundy leather
        theme.amber     = 0xffe8a040;  // amber gauge glow
        theme.amberDark = 0xff8a6028;  // warm leather tan
        theme.green     = 0xff40b868;  // subtle green indicator
        theme.greenDark = 0xff183020;  // dark racing green

        // ── LCD — instrument cluster: black bg, amber readout ──
        theme.lcdBg    = 0xff000000;
        theme.lcdText  = 0xffe8a040;  // amber gauge
        theme.lcdAmber = 0xfff0b850;  // bright amber for values

        // ── Buttons — dark suede with aluminium edge ──
        theme.buttonFace  = 0xff222228;
        theme.buttonHover = 0xff2c2c32;
        theme.buttonDown  = 0xff18181c;

        theme.btnStop       = 0xff222228;
        theme.btnMetronome  = 0xff222228;
        theme.btnMetronomeOn = 0xff302818;  // warm amber tint
        theme.btnCountIn    = 0xff222228;
        theme.btnCountInOn  = 0xff302818;
        theme.btnNewClip    = 0xff222228;
        theme.btnDeleteClip = 0xff2a1218;  // burgundy hint
        theme.btnDuplicate  = 0xff222228;
        theme.btnSplit      = 0xff222228;
        theme.btnQuantize   = 0xff222228;
        theme.btnEditNotes  = 0xff222228;
        theme.btnNav        = 0xff222228;
        theme.btnSave       = 0xff222228;
        theme.btnLoad       = 0xff222228;
        theme.btnUndoRedo   = 0xff222228;
        theme.btnMidi2      = 0xff222228;
        theme.btnMidi2On    = 0xff302818;
        theme.btnLoop       = 0xff222228;
        theme.btnLoopOn     = 0xff302818;
        theme.loopRegion    = 0x20e8a040;  // amber translucent
        theme.loopBorder    = 0xffc08030;  // warm amber border

        // ── Timeline — dark with subtle suede texture grid ──
        theme.timelineBg         = 0xff121216;
        theme.timelineAltRow     = 0xff18181c;
        theme.timelineSelectedRow = 0xff282420;  // warm selection
        theme.timelineGridMajor  = 0xff4a4a50;  // aluminium grid
        theme.timelineGridMinor  = 0xff2a2a30;
        theme.timelineGridFaint  = 0xff1e1e22;
        theme.timelineGridBeat   = 0xff323238;

        // ── Clips — warm dark fills ──
        theme.clipDefault   = 0xff2a2620;  // warm suede clip
        theme.clipRecording = 0xff3a1818;  // red recording
        theme.clipQueued    = 0xff302c18;  // amber queued
        theme.clipPlaying   = 0xff1e2e1a;  // green playing

        // ── Playhead — bright amber gauge needle ──
        theme.playhead     = 0xdde8a040;
        theme.playheadGlow = 0x22e8a040;

        theme.accentStripe = 0xffe8a040;  // amber accent

        theme.trackSelected = 0xff282420;
        theme.trackArmed    = 0xff302818;  // amber tint
        theme.trackMuteOn   = 0xffe8a040;  // amber
        theme.trackSoloOn   = 0xfff0b850;  // bright amber
        theme.trackSoloText = 0xff101014;

        applyThemeColors();

        // Amber instrument text in combo boxes and popups
        setColour(juce::ComboBox::textColourId,                   juce::Colour(0xffe8a040));
        setColour(juce::PopupMenu::textColourId,                  juce::Colour(0xffe8e4de));
        setColour(juce::PopupMenu::highlightedTextColourId,       juce::Colour(0xff101014));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xffe8a040));

        // Slider text boxes — gauge style
        setColour(juce::Slider::textBoxTextColourId,       juce::Colour(0xffe8a040));
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff000000));
        setColour(juce::Slider::textBoxOutlineColourId,    juce::Colour(0x00000000));

        // Button on-state
        setColour(juce::TextButton::buttonOnColourId,      juce::Colour(0xff302818));
        setColour(juce::TextButton::textColourOnId,        juce::Colour(0xffe8a040));
    }

    float getButtonRadius() const override { return 3.0f; }

    // Brushed aluminium side panels
    int getSidePanelWidth() const override { return 8; }

    void drawSidePanels(juce::Graphics& g, int width, int height) override
    {
        if (width != sideCacheKey || height != sideCacheH)
        {
            int panelW = getSidePanelWidth();
            sideCache = juce::Image(juce::Image::ARGB, width, height, true);
            juce::Graphics sg(sideCache);

            // Brushed aluminium gradient
            juce::Colour alumBase(0xff404048);
            juce::Colour alumLight(0xff585860);
            juce::Colour alumDark(0xff2a2a30);

            sg.setGradientFill(juce::ColourGradient(alumLight, 0, 0, alumDark,
                               static_cast<float>(panelW), 0, false));
            sg.fillRect(0, 0, panelW, height);

            sg.setGradientFill(juce::ColourGradient(alumDark, static_cast<float>(width - panelW), 0,
                               alumLight, static_cast<float>(width), 0, false));
            sg.fillRect(width - panelW, 0, panelW, height);

            // Vertical brush lines
            juce::Random rng(42);
            for (int i = 0; i < 30; ++i)
            {
                float x = rng.nextFloat() * panelW;
                float alpha = 0.05f + rng.nextFloat() * 0.1f;
                sg.setColour(alumLight.withAlpha(alpha));
                sg.drawVerticalLine(static_cast<int>(x), 0, static_cast<float>(height));
                sg.drawVerticalLine(width - panelW + static_cast<int>(x), 0, static_cast<float>(height));
            }

            // Inner shadow edge
            sg.setColour(juce::Colour(0x40000000));
            sg.fillRect(static_cast<float>(panelW - 1), 0.0f, 1.0f, static_cast<float>(height));
            sg.fillRect(static_cast<float>(width - panelW), 0.0f, 1.0f, static_cast<float>(height));

            sideCacheKey = width;
            sideCacheH = height;
        }
        g.drawImageAt(sideCache, 0, 0);
    }

    void invalidateCaches() override { sideCacheKey = 0; sideCacheH = 0; topBarCacheW = 0; topBarCacheH = 0; }

    mutable juce::Image sideCache;
    mutable int sideCacheKey = 0, sideCacheH = 0;

    // Brushed aluminium inner strip
    void drawInnerStrip(juce::Graphics& g, int x, int /*y*/, int width, int height) override
    {
        juce::Colour alumBase(0xff404048);
        juce::Colour alumLight(0xff585860);

        g.setGradientFill(juce::ColourGradient(alumLight, static_cast<float>(x), 0,
                          alumBase, static_cast<float>(x + width), 0, false));
        g.fillRect(x, 0, width, height);

        juce::Random rng(99);
        for (int i = 0; i < 20; ++i)
        {
            float fx = x + rng.nextFloat() * width;
            float alpha = 0.05f + rng.nextFloat() * 0.1f;
            g.setColour(alumLight.withAlpha(alpha));
            g.drawVerticalLine(static_cast<int>(fx), 0, static_cast<float>(height));
        }

        g.setColour(juce::Colour(0x40000000));
        g.drawVerticalLine(x, 0, static_cast<float>(height));
        g.drawVerticalLine(x + width - 1, 0, static_cast<float>(height));
    }

    // Perforated leather top bar
    void drawTopBarBackground(juce::Graphics& g, int x, int y, int width, int height) override
    {
        if (width != topBarCacheW || height != topBarCacheH)
        {
            topBarCache = juce::Image(juce::Image::RGB, width, height, false);
            juce::Graphics tg(topBarCache);

            // Dark charcoal base with slight warm tint
            tg.setColour(juce::Colour(0xff1e1e22));
            tg.fillAll();

            // Subtle perforated pattern (like Alcantara perforated leather)
            juce::Random rng(77);
            for (int py = 2; py < height - 2; py += 4)
            {
                for (int px = 2; px < width - 2; px += 4)
                {
                    float alpha = 0.03f + rng.nextFloat() * 0.04f;
                    tg.setColour(juce::Colour(0xff000000).withAlpha(alpha));
                    tg.fillEllipse(static_cast<float>(px), static_cast<float>(py), 1.5f, 1.5f);
                }
            }

            // Bottom stitch line — like real Alcantara stitching
            tg.setColour(juce::Colour(0xff3a3a40));
            for (int sx = 0; sx < width; sx += 6)
                tg.fillRect(static_cast<float>(sx), static_cast<float>(height - 2), 3.0f, 1.0f);

            topBarCacheW = width;
            topBarCacheH = height;
        }
        g.drawImageAt(topBarCache, x, y);
    }

    mutable juce::Image topBarCache;
    mutable int topBarCacheW = 0, topBarCacheH = 0;

    // Custom button with suede feel
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour&,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);

        // Dark suede base
        g.setColour(juce::Colour(0xff1a1a1e));
        g.fillRoundedRectangle(bounds, 3.0f);

        if (shouldDrawButtonAsDown)
            g.setColour(juce::Colour(0xffe8a040).withAlpha(0.4f));
        else if (shouldDrawButtonAsHighlighted)
            g.setColour(juce::Colour(0xff505058));
        else
            g.setColour(juce::Colour(0xff3a3a40));
        g.drawRoundedRectangle(bounds, 3.0f, 0.8f);
    }

    // Amber gauge text for OLED buttons
    bool drawOledButtonArt(juce::Image& oled, const juce::String& text,
                           bool on, float t, juce::Colour, juce::Colour) const override
    {
        juce::Colour amber(0xffe8a040);
        juce::Colour dim(0xff5a4020);

        if (text == "REC")
        {
            juce::Colour col = on ? juce::Colour(0xffc43030) : dim;
            float pulse = on ? (0.5f + 0.5f * std::sin(t * 8.0f)) : 1.0f;
            int icx = OLED_W / 2;
            int icy = OLED_H / 2;
            oledCircle(oled, icx, icy, 3, col.withAlpha(pulse), true);
            if (on)
            {
                int ringR = 4 + (static_cast<int>(t * 6.0f) % 3);
                oledCircle(oled, icx, icy, ringR, juce::Colour(0xffc43030).withAlpha(0.3f));
            }
            return true;
        }

        return DawLookAndFeel::drawOledButtonArt(oled, text, on, t, amber, dim);
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool, bool) override
    {
        auto text = button.getButtonText();
        bool on = button.getToggleState();

        juce::Colour amber(0xffe8a040);
        juce::Colour dim(0xff5a4020);

        if (isOledAnimatedButton(text) || text == "REC" || text == "PANIC")
        {
            auto dispBounds = button.getLocalBounds().toFloat().reduced(2.0f);
            float t = static_cast<float>(juce::Time::getMillisecondCounterHiRes() * 0.001);

            juce::Image oled(juce::Image::ARGB, OLED_W, OLED_H, true);
            if (drawOledButtonArt(oled, text, on, t, amber, dim))
            {
                drawOledImage(g, oled, dispBounds);
                return;
            }
        }

        auto dispBounds = button.getLocalBounds().toFloat().reduced(2.0f);
        g.setColour(on ? amber : amber.withAlpha(0.7f));
        g.setFont(juce::Font(getUIFontName(),
                   juce::jmin(12.0f, dispBounds.getHeight() * 0.5f), juce::Font::bold));
        g.drawText(formatButtonText(text), button.getLocalBounds().reduced(2),
                   juce::Justification::centred);
    }

    // Combo box with brushed aluminium trim
    void drawComboBox(juce::Graphics& g, int width, int height, bool,
                      int, int, int, int, juce::ComboBox&) override
    {
        auto bounds = juce::Rectangle<float>(0, 0, static_cast<float>(width), static_cast<float>(height));

        g.setColour(juce::Colour(0xff1a1a1e));
        g.fillRoundedRectangle(bounds, 3.0f);
        g.setColour(juce::Colour(0xff3a3a40));
        g.drawRoundedRectangle(bounds, 3.0f, 0.8f);

        auto arrowZone = bounds.removeFromRight(20.0f).reduced(5.0f);
        juce::Path arrow;
        arrow.addTriangle(arrowZone.getX(), arrowZone.getCentreY() - 3,
                         arrowZone.getRight(), arrowZone.getCentreY() - 3,
                         arrowZone.getCentreX(), arrowZone.getCentreY() + 3);
        g.setColour(juce::Colour(0xffe8a040));
        g.fillPath(arrow);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AlcantaraLookAndFeel)
};
