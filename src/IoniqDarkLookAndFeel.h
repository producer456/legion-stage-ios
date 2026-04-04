#pragma once

#include "IoniqLookAndFeel.h"

// "Ioniq Dark" theme — the Ioniq at night.
// Same modern EV design language but inverted: dark charcoal body,
// slate teal accents, OLED displays everywhere. Like driving the
// cabin at night with ambient lighting.
class IoniqDarkLookAndFeel : public IoniqLookAndFeel
{
public:
    IoniqDarkLookAndFeel()
    {
        // ── Surfaces — dark charcoal with cool undertone ──
        theme.body        = 0xff1c1e22;  // dark charcoal
        theme.bodyLight   = 0xff242628;  // slightly lighter panel
        theme.bodyDark    = 0xff121416;  // deep shadow
        theme.border      = 0xff3a3e44;  // cool grey border
        theme.borderLight = 0xff4a4e54;  // lighter border

        // ── Text — light for dark bg ──
        theme.textPrimary   = 0xffe0dcd6;  // warm white
        theme.textSecondary = 0xff8a8e94;  // medium grey
        theme.textBright    = 0xfff4f0ea;  // bright white

        // ── Accent — same teal family, slightly brighter for contrast ──
        theme.red       = 0xffcc4444;  // warm red
        theme.redDark   = 0xff2a1418;  // dark rose
        theme.amber     = 0xff6a8a9a;  // slate teal (brighter for dark bg)
        theme.amberDark = 0xff5a7a8a;
        theme.green     = 0xff5a9a6a;  // sage green
        theme.greenDark = 0xff1a2e20;  // dark sage

        // ── LCD — same OLED style ──
        theme.lcdBg    = 0xff0a0c10;
        theme.lcdText  = 0xffc8dce8;   // cool blue-white
        theme.lcdAmber = 0xffd0e4f0;

        // ── Buttons — dark slate ──
        theme.buttonFace  = 0xff2a2c30;
        theme.buttonHover = 0xff323438;
        theme.buttonDown  = 0xff1e2024;

        theme.btnStop       = 0xff2a2c30;
        theme.btnMetronome  = 0xff2a2c30;
        theme.btnMetronomeOn = 0xff1e3040;  // teal tint
        theme.btnCountIn    = 0xff2a2c30;
        theme.btnCountInOn  = 0xff1e3040;
        theme.btnNewClip    = 0xff2a2c30;
        theme.btnDeleteClip = 0xff301820;  // dark rose
        theme.btnDuplicate  = 0xff2a2c30;
        theme.btnSplit      = 0xff2a2c30;
        theme.btnQuantize   = 0xff2a2c30;
        theme.btnEditNotes  = 0xff2a2c30;
        theme.btnNav        = 0xff2a2c30;
        theme.btnSave       = 0xff2a2c30;
        theme.btnLoad       = 0xff2a2c30;
        theme.btnUndoRedo   = 0xff2a2c30;
        theme.btnMidi2      = 0xff2a2c30;
        theme.btnMidi2On    = 0xff1e3040;
        theme.btnLoop       = 0xff2a2c30;
        theme.btnLoopOn     = 0xff1e3040;
        theme.loopRegion    = 0x206a8a9a;
        theme.loopBorder    = 0xff6a8a9a;

        // ── Timeline — dark with cool grid ──
        theme.timelineBg         = 0xff141618;
        theme.timelineAltRow     = 0xff1a1c1e;
        theme.timelineSelectedRow = 0xff1e2e38;  // cool teal selection
        theme.timelineGridMajor  = 0xff4a4e54;
        theme.timelineGridMinor  = 0xff2e3034;
        theme.timelineGridFaint  = 0xff1e2024;
        theme.timelineGridBeat   = 0xff363a3e;

        // ── Clips — muted dark tones ──
        theme.clipDefault   = 0xff2a3844;  // dark teal clip
        theme.clipRecording = 0xff3a2020;  // dark rose
        theme.clipQueued    = 0xff303428;  // dark warm
        theme.clipPlaying   = 0xff203828;  // dark sage

        // ── Playhead — bright teal ──
        theme.playhead     = 0xcc6a8a9a;
        theme.playheadGlow = 0x226a8a9a;

        theme.accentStripe = 0xff6a8a9a;

        theme.trackSelected = 0xff1e2e38;
        theme.trackArmed    = 0xff1e3040;
        theme.trackMuteOn   = 0xff6a8a9a;
        theme.trackSoloOn   = 0xffe8a840;
        theme.trackSoloText = 0xff121416;

        applyThemeColors();

        // OLED combo/popup text
        setColour(juce::ComboBox::textColourId,                   juce::Colour(0xffc8dce8));
        setColour(juce::PopupMenu::backgroundColourId,            juce::Colour(0xff1a1c1e));
        setColour(juce::PopupMenu::textColourId,                  juce::Colour(0xffc8dce8));
        setColour(juce::PopupMenu::highlightedTextColourId,       juce::Colour(0xff121416));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xffc8dce8));

        setColour(juce::Slider::textBoxTextColourId,       juce::Colour(0xffc8dce8));
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff0a0c10));
        setColour(juce::Slider::textBoxOutlineColourId,    juce::Colour(0x00000000));

        setColour(juce::TextButton::buttonOnColourId,      juce::Colour(0xff1e3040));
        setColour(juce::TextButton::textColourOnId,        juce::Colour(0xffc8dce8));
    }

    // Override button drawing — recessed OLED in dark mode
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour&,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);

        // Outer bezel — subtle lighter rim
        g.setColour(juce::Colour(0xff3a3e44));
        g.fillRoundedRectangle(bounds, 5.0f);

        // Inner shadow
        auto inset = bounds.reduced(1.5f);
        g.setColour(juce::Colour(0xff141618));
        g.drawRoundedRectangle(inset, 4.0f, 1.0f);

        // OLED screen
        auto screen = bounds.reduced(2.5f);
        g.setColour(juce::Colour(0xff0e1014));
        g.fillRoundedRectangle(screen, 3.0f);

        if (shouldDrawButtonAsDown)
            g.setColour(juce::Colour(0xff6a8a9a).withAlpha(0.3f));
        else if (shouldDrawButtonAsHighlighted)
            g.setColour(juce::Colour(0xff2a2e34));
        else
            g.setColour(juce::Colour(0xff1a1e24));
        g.drawRoundedRectangle(screen, 3.0f, 0.5f);
    }

    // All button text is light in dark mode
    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool, bool) override
    {
        auto text = button.getButtonText();
        bool on = button.getToggleState();

        juce::Colour bright(0xffc8dce8);
        juce::Colour dim(0xff4a5a64);

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

        auto dispBounds = button.getLocalBounds().toFloat().reduced(2.0f);
        g.setColour(on ? bright : bright.withAlpha(0.7f));
        g.setFont(juce::Font(getUIFontName(),
                   juce::jmin(12.0f, dispBounds.getHeight() * 0.5f), juce::Font::bold));
        g.drawText(formatButtonText(text), button.getLocalBounds().reduced(2),
                   juce::Justification::centred);
    }

    // Dark side panels
    void drawSidePanels(juce::Graphics& g, int width, int height) override
    {
        if (width != sideCacheKey || height != sideCacheH)
        {
            int panelW = getSidePanelWidth();
            sideCache = juce::Image(juce::Image::ARGB, width, height, true);
            juce::Graphics sg(sideCache);

            juce::Colour slateBase(0xff2a2e34);
            juce::Colour slateLight(0xff343840);
            juce::Colour slateDark(0xff1e2228);

            sg.setGradientFill(juce::ColourGradient(slateLight, 0, 0, slateDark,
                               static_cast<float>(panelW), 0, false));
            sg.fillRect(0, 0, panelW, height);

            sg.setGradientFill(juce::ColourGradient(slateDark, static_cast<float>(width - panelW), 0,
                               slateLight, static_cast<float>(width), 0, false));
            sg.fillRect(width - panelW, 0, panelW, height);

            juce::Random rng(42);
            for (int y = 0; y < height; y += 3)
            {
                float alpha = 0.02f + rng.nextFloat() * 0.04f;
                sg.setColour(slateLight.withAlpha(alpha));
                sg.drawHorizontalLine(y, 0, static_cast<float>(panelW));
                sg.drawHorizontalLine(y, static_cast<float>(width - panelW), static_cast<float>(width));
            }

            sg.setColour(juce::Colour(0x20000000));
            sg.fillRect(static_cast<float>(panelW - 1), 0.0f, 1.0f, static_cast<float>(height));
            sg.fillRect(static_cast<float>(width - panelW), 0.0f, 1.0f, static_cast<float>(height));

            sideCacheKey = width;
            sideCacheH = height;
        }
        g.drawImageAt(sideCache, 0, 0);
    }

    // Dark inner strip
    void drawInnerStrip(juce::Graphics& g, int x, int /*y*/, int width, int height) override
    {
        juce::Colour slateBase(0xff2a2e34);
        juce::Colour slateLight(0xff343840);

        g.setGradientFill(juce::ColourGradient(slateLight, static_cast<float>(x), 0,
                          slateBase, static_cast<float>(x + width), 0, false));
        g.fillRect(x, 0, width, height);

        juce::Random rng(99);
        for (int y = 0; y < height; y += 3)
        {
            float alpha = 0.02f + rng.nextFloat() * 0.04f;
            g.setColour(slateLight.withAlpha(alpha));
            g.drawHorizontalLine(y, static_cast<float>(x), static_cast<float>(x + width));
        }

        g.setColour(juce::Colour(0x20000000));
        g.drawVerticalLine(x, 0, static_cast<float>(height));
        g.drawVerticalLine(x + width - 1, 0, static_cast<float>(height));
    }

    // Dark top bar
    void drawTopBarBackground(juce::Graphics& g, int x, int y, int width, int height) override
    {
        if (width != topBarCacheW || height != topBarCacheH)
        {
            topBarCache = juce::Image(juce::Image::RGB, width, height, false);
            juce::Graphics tg(topBarCache);

            tg.setColour(juce::Colour(0xff222428));
            tg.fillAll();

            juce::Random rng(77);
            for (int py = 0; py < height; py += 2)
            {
                for (int px = 0; px < width; px += 2)
                {
                    float alpha = rng.nextFloat() * 0.02f;
                    tg.setColour(juce::Colour(0xffffffff).withAlpha(alpha));
                    tg.fillRect(static_cast<float>(px), static_cast<float>(py), 1.0f, 1.0f);
                }
            }

            tg.setColour(juce::Colour(0xff3a3e44));
            tg.fillRect(0, height - 1, width, 1);

            topBarCacheW = width;
            topBarCacheH = height;
        }
        g.drawImageAt(topBarCache, x, y);
    }

    // Dark recessed combo box
    void drawComboBox(juce::Graphics& g, int width, int height, bool,
                      int, int, int, int, juce::ComboBox&) override
    {
        auto bounds = juce::Rectangle<float>(0, 0, static_cast<float>(width), static_cast<float>(height));

        // Outer bezel
        g.setColour(juce::Colour(0xff3a3e44));
        g.fillRoundedRectangle(bounds, 5.0f);

        // Inner shadow
        auto inset = bounds.reduced(1.5f);
        g.setColour(juce::Colour(0xff141618));
        g.drawRoundedRectangle(inset, 4.0f, 1.0f);

        // OLED screen
        auto screen = bounds.reduced(2.5f);
        g.setColour(juce::Colour(0xff0e1014));
        g.fillRoundedRectangle(screen, 3.0f);
        g.setColour(juce::Colour(0xff1a1e24));
        g.drawRoundedRectangle(screen, 3.0f, 0.5f);

        auto arrowZone = screen.removeFromRight(18.0f).reduced(4.0f);
        juce::Path arrow;
        arrow.addTriangle(arrowZone.getX(), arrowZone.getCentreY() - 3,
                         arrowZone.getRight(), arrowZone.getCentreY() - 3,
                         arrowZone.getCentreX(), arrowZone.getCentreY() + 3);
        g.setColour(juce::Colour(0xffc8dce8));
        g.fillPath(arrow);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IoniqDarkLookAndFeel)
};
