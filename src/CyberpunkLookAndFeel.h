#pragma once

#include "DawLookAndFeel.h"

// "Cyberpunk" theme — inspired by Cyberpunk 2077.
// Neon cyan on dark chrome, hot magenta accents, acid yellow warnings,
// angular UI, glitch aesthetics, scan line textures.
// Night City in a DAW.
class CyberpunkLookAndFeel : public DawLookAndFeel
{
public:
    CyberpunkLookAndFeel()
    {
        // ── Surfaces — dark chrome/concrete ──
        theme.body        = 0xff0a0c14;  // near-black with blue tint
        theme.bodyLight   = 0xff141820;  // dark chrome panel
        theme.bodyDark    = 0xff060810;  // deep void
        theme.border      = 0xff1a2030;  // dark blue-chrome border
        theme.borderLight = 0xff2a3448;  // lighter chrome

        // ── Text — neon cyan ──
        theme.textPrimary   = 0xff00e8f0;  // neon cyan
        theme.textSecondary = 0xff406878;  // dimmed cyan
        theme.textBright    = 0xfff0f4f8;  // bright white for emphasis

        // ── Accent — hot magenta + acid yellow ──
        theme.red       = 0xffff003c;  // hot magenta / Samurai red
        theme.redDark   = 0xff300818;
        theme.amber     = 0xfffcee0a;  // acid yellow
        theme.amberDark = 0xff8a7a08;
        theme.green     = 0xff00e8f0;  // cyan (used as green substitute)
        theme.greenDark = 0xff082830;

        // ── LCD — neon cyan on void black ──
        theme.lcdBg    = 0xff000000;
        theme.lcdText  = 0xff00e8f0;   // neon cyan
        theme.lcdAmber = 0xfffcee0a;   // acid yellow for values

        // ── Buttons — dark chrome with neon edge ──
        theme.buttonFace  = 0xff0e1018;
        theme.buttonHover = 0xff161a24;
        theme.buttonDown  = 0xff080a10;

        theme.btnStop = theme.btnMetronome = theme.btnCountIn = theme.btnNewClip = 0xff0e1018;
        theme.btnDeleteClip = 0xff200818;  // magenta hint
        theme.btnDuplicate = theme.btnSplit = theme.btnQuantize = theme.btnEditNotes = 0xff0e1018;
        theme.btnNav = theme.btnSave = theme.btnLoad = theme.btnUndoRedo = theme.btnMidi2 = 0xff0e1018;
        theme.btnMetronomeOn = theme.btnCountInOn = theme.btnMidi2On = 0xff082830;
        theme.btnLoop = 0xff0e1018;
        theme.btnLoopOn = 0xff082830;
        theme.loopRegion = 0x2000e8f0;
        theme.loopBorder = 0xff00e8f0;

        // ── Timeline — void with neon grid ──
        theme.timelineBg         = 0xff060810;
        theme.timelineAltRow     = 0xff0a0e18;
        theme.timelineSelectedRow = 0xff102030;
        theme.timelineGridMajor  = 0xff1a3040;  // cyan-tinted grid
        theme.timelineGridMinor  = 0xff101828;
        theme.timelineGridFaint  = 0xff0a1018;
        theme.timelineGridBeat   = 0xff142030;

        // ── Clips — neon-edged dark ──
        theme.clipDefault   = 0xff102028;  // dark cyan
        theme.clipRecording = 0xff300818;  // magenta
        theme.clipQueued    = 0xff282808;  // acid yellow tint
        theme.clipPlaying   = 0xff083020;  // bright cyan-green

        // ── Playhead — hot neon cyan ──
        theme.playhead     = 0xee00e8f0;
        theme.playheadGlow = 0x3300e8f0;

        theme.accentStripe = 0xff00e8f0;

        theme.trackSelected = 0xff102030;
        theme.trackArmed    = 0xff300818;  // magenta armed
        theme.trackMuteOn   = 0xffff003c;  // hot magenta mute
        theme.trackSoloOn   = 0xfffcee0a;  // acid yellow solo
        theme.trackSoloText = 0xff060810;

        applyThemeColors();

        setColour(juce::ComboBox::textColourId,                   juce::Colour(0xff00e8f0));
        setColour(juce::PopupMenu::backgroundColourId,            juce::Colour(0xff0a0e18));
        setColour(juce::PopupMenu::textColourId,                  juce::Colour(0xff00e8f0));
        setColour(juce::PopupMenu::highlightedTextColourId,       juce::Colour(0xff020408));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff00e8f0));

        setColour(juce::Slider::textBoxTextColourId,       juce::Colour(0xff00e8f0));
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff000000));
        setColour(juce::Slider::textBoxOutlineColourId,    juce::Colour(0x00000000));

        setColour(juce::TextButton::buttonOnColourId,      juce::Colour(0xff082830));
        setColour(juce::TextButton::textColourOnId,        juce::Colour(0xff00e8f0));
    }

    // Sharp angular corners — cyberpunk is NOT rounded
    float getButtonRadius() const override { return 1.0f; }

    // Tech font
    juce::String getUIFontName() const override { return "DIN Alternate"; }

    // No side panels — raw chrome edge
    int getSidePanelWidth() const override { return 0; }

    // Glitch-scan top bar
    void drawTopBarBackground(juce::Graphics& g, int x, int y, int width, int height) override
    {
        if (width != topBarCacheW || height != topBarCacheH)
        {
            topBarCache = juce::Image(juce::Image::RGB, width, height, false);
            juce::Graphics tg(topBarCache);

            tg.setColour(juce::Colour(0xff0a0c14));
            tg.fillAll();

            // Horizontal scan lines
            for (int sy = 0; sy < height; sy += 2)
            {
                tg.setColour(juce::Colour(0xff00e8f0).withAlpha(0.02f));
                tg.drawHorizontalLine(sy, 0, static_cast<float>(width));
            }

            // Bottom neon line
            tg.setColour(juce::Colour(0xff00e8f0).withAlpha(0.4f));
            tg.fillRect(0, height - 1, width, 1);
            tg.setColour(juce::Colour(0xff00e8f0).withAlpha(0.1f));
            tg.fillRect(0, height - 3, width, 2);

            topBarCacheW = width;
            topBarCacheH = height;
        }
        g.drawImageAt(topBarCache, x, y);
    }

    void invalidateCaches() override { topBarCacheW = 0; topBarCacheH = 0; }

    mutable juce::Image topBarCache;
    mutable int topBarCacheW = 0, topBarCacheH = 0;

    // Angular OLED buttons with neon edge
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour&,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);

        // Dark chrome fill
        g.setColour(juce::Colour(0xff0a0c14));
        g.fillRect(bounds);

        // Neon edge
        if (shouldDrawButtonAsDown)
            g.setColour(juce::Colour(0xffff003c).withAlpha(0.8f));  // magenta flash
        else if (shouldDrawButtonAsHighlighted)
            g.setColour(juce::Colour(0xff00e8f0).withAlpha(0.6f));  // bright cyan
        else
            g.setColour(juce::Colour(0xff00e8f0).withAlpha(0.2f));  // dim cyan
        g.drawRect(bounds, 1.0f);

        // Corner glitch accents
        float cs = 3.0f;
        g.setColour(juce::Colour(0xff00e8f0).withAlpha(shouldDrawButtonAsHighlighted ? 0.6f : 0.3f));
        // Top-left corner
        g.drawLine(bounds.getX(), bounds.getY(), bounds.getX() + cs, bounds.getY(), 1.5f);
        g.drawLine(bounds.getX(), bounds.getY(), bounds.getX(), bounds.getY() + cs, 1.5f);
        // Bottom-right corner
        g.drawLine(bounds.getRight() - cs, bounds.getBottom(), bounds.getRight(), bounds.getBottom(), 1.5f);
        g.drawLine(bounds.getRight(), bounds.getBottom() - cs, bounds.getRight(), bounds.getBottom(), 1.5f);
    }

    // Neon cyan OLED art
    bool drawOledButtonArt(juce::Image& oled, const juce::String& text,
                           bool on, float t, juce::Colour, juce::Colour) const override
    {
        juce::Colour bright(0xff00e8f0);   // neon cyan
        juce::Colour dim(0xff204050);

        if (text == "REC")
        {
            // Magenta recording
            juce::Colour col = on ? juce::Colour(0xffff003c) : dim;
            float pulse = on ? (0.5f + 0.5f * std::sin(t * 8.0f)) : 1.0f;
            int icx = OLED_W / 2, icy = OLED_H / 2;
            oledCircle(oled, icx, icy, 3, col.withAlpha(pulse), true);
            if (on)
            {
                int r = 4 + (static_cast<int>(t * 6.0f) % 3);
                oledCircle(oled, icx, icy, r, juce::Colour(0xffff003c).withAlpha(0.3f));
            }
            return true;
        }

        return DawLookAndFeel::drawOledButtonArt(oled, text, on, t, bright, dim);
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool) override
    {
        auto text = button.getButtonText();
        bool on = button.getToggleState();

        juce::Colour bright(0xff00e8f0);
        juce::Colour dim(0xff204050);

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

    // Angular combo box
    void drawComboBox(juce::Graphics& g, int width, int height, bool,
                      int, int, int, int, juce::ComboBox&) override
    {
        auto bounds = juce::Rectangle<float>(0, 0, static_cast<float>(width), static_cast<float>(height));

        g.setColour(juce::Colour(0xff0a0c14));
        g.fillRect(bounds);
        g.setColour(juce::Colour(0xff00e8f0).withAlpha(0.2f));
        g.drawRect(bounds, 1.0f);

        // Corner accents
        float cs = 3.0f;
        g.setColour(juce::Colour(0xff00e8f0).withAlpha(0.4f));
        g.drawLine(bounds.getX(), bounds.getY(), bounds.getX() + cs, bounds.getY(), 1.5f);
        g.drawLine(bounds.getX(), bounds.getY(), bounds.getX(), bounds.getY() + cs, 1.5f);

        auto arrowZone = bounds.removeFromRight(20.0f).reduced(5.0f);
        juce::Path arrow;
        arrow.addTriangle(arrowZone.getX(), arrowZone.getCentreY() - 3,
                         arrowZone.getRight(), arrowZone.getCentreY() - 3,
                         arrowZone.getCentreX(), arrowZone.getCentreY() + 3);
        g.setColour(juce::Colour(0xff00e8f0));
        g.fillPath(arrow);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CyberpunkLookAndFeel)
};
