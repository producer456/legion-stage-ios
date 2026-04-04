#pragma once

#include "DawLookAndFeel.h"

// "Ioniq" theme — inspired by modern luxury EV interiors.
// Light cream leather seats, soft slate dashboard, warm natural tones,
// clean minimalist Scandinavian design. Feels like sitting in a
// sunlit electric vehicle cabin — calm, airy, and refined.
class IoniqLookAndFeel : public DawLookAndFeel
{
public:
    IoniqLookAndFeel()
    {
        // ── Surfaces — warm cream and soft slate ──
        theme.body        = 0xffe8e2d8;  // warm cream (seat leather)
        theme.bodyLight   = 0xfff0ebe0;  // lighter cream panel
        theme.bodyDark    = 0xffd8d0c4;  // slightly deeper cream
        theme.border      = 0xffc0b8aa;  // warm taupe border
        theme.borderLight = 0xffa8a090;  // muted stone

        // ── Text — dark slate for readability on light bg ──
        theme.textPrimary   = 0xff2a2e34;  // dark slate (dashboard color)
        theme.textSecondary = 0xff6a6e74;  // medium slate grey
        theme.textBright    = 0xff1a1e24;  // near-black for emphasis

        // ── Accent — muted teal from ambient lighting + warm copper ──
        theme.red       = 0xffc04040;  // muted warm red
        theme.redDark   = 0xffe8d0d0;  // light rose background
        theme.amber     = 0xff5a7a8a;  // slate teal (dashboard accent)
        theme.amberDark = 0xff4a6a7a;  // deeper teal
        theme.green     = 0xff4a8a5a;  // natural sage green
        theme.greenDark = 0xffd0e0d4;  // pale sage background

        // ── LCD — slate display: light grey bg, dark teal text ──
        theme.lcdBg    = 0xff2a3038;   // dark slate screen
        theme.lcdText  = 0xffc8dce8;   // cool blue-white readout
        theme.lcdAmber = 0xffd0e4f0;   // bright readout

        // ── Buttons — soft cream with slate border ──
        theme.buttonFace  = 0xffdcd6cc;  // cream button
        theme.buttonHover = 0xffd0cac0;  // slightly darker on hover
        theme.buttonDown  = 0xffc4beb4;  // pressed

        theme.btnStop       = 0xffdcd6cc;
        theme.btnMetronome  = 0xffdcd6cc;
        theme.btnMetronomeOn = 0xffc0d4dc;  // cool blue tint
        theme.btnCountIn    = 0xffdcd6cc;
        theme.btnCountInOn  = 0xffc0d4dc;
        theme.btnNewClip    = 0xffdcd6cc;
        theme.btnDeleteClip = 0xffe0c8c8;  // rose hint
        theme.btnDuplicate  = 0xffdcd6cc;
        theme.btnSplit      = 0xffdcd6cc;
        theme.btnQuantize   = 0xffdcd6cc;
        theme.btnEditNotes  = 0xffdcd6cc;
        theme.btnNav        = 0xffdcd6cc;
        theme.btnSave       = 0xffdcd6cc;
        theme.btnLoad       = 0xffdcd6cc;
        theme.btnUndoRedo   = 0xffdcd6cc;
        theme.btnMidi2      = 0xffdcd6cc;
        theme.btnMidi2On    = 0xffc0d4dc;
        theme.btnLoop       = 0xffdcd6cc;
        theme.btnLoopOn     = 0xffc0d4dc;
        theme.loopRegion    = 0x205a7a8a;  // teal translucent
        theme.loopBorder    = 0xff5a7a8a;  // teal border

        // ── Timeline — light with subtle warm grid ──
        theme.timelineBg         = 0xfff4efe6;  // warm off-white
        theme.timelineAltRow     = 0xffece6dc;  // slightly darker cream
        theme.timelineSelectedRow = 0xffd4dee6;  // cool blue selection
        theme.timelineGridMajor  = 0xffb0a898;  // taupe grid
        theme.timelineGridMinor  = 0xffd0c8bc;  // light taupe
        theme.timelineGridFaint  = 0xffe4ded4;  // very faint
        theme.timelineGridBeat   = 0xffc8c0b4;  // beat grid

        // ── Clips — soft pastels ──
        theme.clipDefault   = 0xffc8d8e0;  // cool blue-grey clip
        theme.clipRecording = 0xffe0c0c0;  // soft rose recording
        theme.clipQueued    = 0xffd8d4c0;  // warm cream queued
        theme.clipPlaying   = 0xffc4dcc8;  // sage green playing

        // ── Playhead — dark slate needle ──
        theme.playhead     = 0xcc2a3038;
        theme.playheadGlow = 0x182a3038;

        theme.accentStripe = 0xff5a7a8a;  // slate teal stripe

        theme.trackSelected = 0xffd4dee6;
        theme.trackArmed    = 0xffc0d4dc;  // cool blue tint
        theme.trackMuteOn   = 0xff5a7a8a;  // slate teal
        theme.trackSoloOn   = 0xffe8a840;  // warm amber solo
        theme.trackSoloText = 0xfffaf6f0;

        applyThemeColors();

        // OLED-style combo box text — light on dark
        setColour(juce::ComboBox::textColourId,                   juce::Colour(0xffc8dce8));
        setColour(juce::PopupMenu::backgroundColourId,            juce::Colour(0xff2a3038));
        setColour(juce::PopupMenu::textColourId,                  juce::Colour(0xffc8dce8));
        setColour(juce::PopupMenu::highlightedTextColourId,       juce::Colour(0xff2a3038));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xffc8dce8));

        // Slider text boxes — OLED style
        setColour(juce::Slider::textBoxTextColourId,       juce::Colour(0xffc8dce8));
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff2a3038));
        setColour(juce::Slider::textBoxOutlineColourId,    juce::Colour(0x00000000));

        // Button on-state — OLED tint
        setColour(juce::TextButton::buttonOnColourId,      juce::Colour(0xff354550));
        setColour(juce::TextButton::textColourOnId,        juce::Colour(0xffc8dce8));
    }

    float getButtonRadius() const override { return 6.0f; }

    // Soft slate side panels
    int getSidePanelWidth() const override { return 8; }

    void drawSidePanels(juce::Graphics& g, int width, int height) override
    {
        if (width != sideCacheKey || height != sideCacheH)
        {
            int panelW = getSidePanelWidth();
            sideCache = juce::Image(juce::Image::ARGB, width, height, true);
            juce::Graphics sg(sideCache);

            // Soft grey gradient — like the slate dashboard trim
            juce::Colour slateBase(0xffb8b0a4);
            juce::Colour slateLight(0xffc8c0b4);
            juce::Colour slateDark(0xffa8a094);

            sg.setGradientFill(juce::ColourGradient(slateLight, 0, 0, slateDark,
                               static_cast<float>(panelW), 0, false));
            sg.fillRect(0, 0, panelW, height);

            sg.setGradientFill(juce::ColourGradient(slateDark, static_cast<float>(width - panelW), 0,
                               slateLight, static_cast<float>(width), 0, false));
            sg.fillRect(width - panelW, 0, panelW, height);

            // Subtle texture — fine horizontal lines like brushed finish
            juce::Random rng(42);
            for (int y = 0; y < height; y += 3)
            {
                float alpha = 0.02f + rng.nextFloat() * 0.04f;
                sg.setColour(slateLight.withAlpha(alpha));
                sg.drawHorizontalLine(y, 0, static_cast<float>(panelW));
                sg.drawHorizontalLine(y, static_cast<float>(width - panelW), static_cast<float>(width));
            }

            // Inner edge shadow
            sg.setColour(juce::Colour(0x20000000));
            sg.fillRect(static_cast<float>(panelW - 1), 0.0f, 1.0f, static_cast<float>(height));
            sg.fillRect(static_cast<float>(width - panelW), 0.0f, 1.0f, static_cast<float>(height));

            sideCacheKey = width;
            sideCacheH = height;
        }
        g.drawImageAt(sideCache, 0, 0);
    }

    mutable juce::Image sideCache;
    mutable int sideCacheKey = 0, sideCacheH = 0;

    // Slate inner strip
    void drawInnerStrip(juce::Graphics& g, int x, int /*y*/, int width, int height) override
    {
        juce::Colour slateBase(0xffb8b0a4);
        juce::Colour slateLight(0xffc8c0b4);

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

    // Cream leather top bar with subtle stitch
    void drawTopBarBackground(juce::Graphics& g, int x, int y, int width, int height) override
    {
        if (width != topBarCacheW || height != topBarCacheH)
        {
            topBarCache = juce::Image(juce::Image::RGB, width, height, false);
            juce::Graphics tg(topBarCache);

            // Warm cream base
            tg.setColour(juce::Colour(0xffdcd6cc));
            tg.fillAll();

            // Very subtle leather grain texture
            juce::Random rng(77);
            for (int py = 0; py < height; py += 2)
            {
                for (int px = 0; px < width; px += 2)
                {
                    float alpha = rng.nextFloat() * 0.03f;
                    tg.setColour(juce::Colour(0xff000000).withAlpha(alpha));
                    tg.fillRect(static_cast<float>(px), static_cast<float>(py), 1.0f, 1.0f);
                }
            }

            // Bottom accent line — slate
            tg.setColour(juce::Colour(0xffc0b8aa));
            tg.fillRect(0, height - 1, width, 1);

            topBarCacheW = width;
            topBarCacheH = height;
        }
        g.drawImageAt(topBarCache, x, y);
    }

    mutable juce::Image topBarCache;
    mutable int topBarCacheW = 0, topBarCacheH = 0;

    // Buttons — OLED for transport/animated, cream for toolbar/edit
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour&,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        auto text = button.getButtonText();

        // OLED style for animated transport buttons
        bool useOled = isOledAnimatedButton(text) || text == "REC" || text == "PANIC"
                       || text == "M2" || text == "..." || text == "PHN";

        if (useOled)
        {
            g.setColour(juce::Colour(0xff2a3038));
            g.fillRoundedRectangle(bounds, 4.0f);

            if (shouldDrawButtonAsDown)
                g.setColour(juce::Colour(0xff5a7a8a).withAlpha(0.5f));
            else if (shouldDrawButtonAsHighlighted)
                g.setColour(juce::Colour(0xff4a5a64));
            else
                g.setColour(juce::Colour(0xff3a4248));
            g.drawRoundedRectangle(bounds, 4.0f, 0.8f);
        }
        else
        {
            // Cream leather style for edit toolbar buttons
            if (shouldDrawButtonAsDown)
                g.setColour(juce::Colour(0xffc4beb4));
            else if (shouldDrawButtonAsHighlighted)
                g.setColour(juce::Colour(0xffd4cec4));
            else
                g.setColour(juce::Colour(0xffdcd6cc));
            g.fillRoundedRectangle(bounds, 6.0f);

            g.setColour(juce::Colour(0xffc0b8aa));
            g.drawRoundedRectangle(bounds, 6.0f, 0.8f);
        }
    }

    // OLED art with slate teal colors
    bool drawOledButtonArt(juce::Image& oled, const juce::String& text,
                           bool on, float t, juce::Colour, juce::Colour) const override
    {
        juce::Colour bright(0xffc8dce8);  // cool blue-white (like LCD)
        juce::Colour dim(0xff4a5a64);     // muted slate

        if (text == "REC")
        {
            juce::Colour col = on ? juce::Colour(0xffc04040) : dim;
            float pulse = on ? (0.5f + 0.5f * std::sin(t * 8.0f)) : 1.0f;
            int icx = OLED_W / 2;
            int icy = OLED_H / 2;
            oledCircle(oled, icx, icy, 3, col.withAlpha(pulse), true);
            if (on)
            {
                int ringR = 4 + (static_cast<int>(t * 6.0f) % 3);
                oledCircle(oled, icx, icy, ringR, juce::Colour(0xffc04040).withAlpha(0.3f));
            }
            return true;
        }

        return DawLookAndFeel::drawOledButtonArt(oled, text, on, t, bright, dim);
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool, bool) override
    {
        auto text = button.getButtonText();
        bool on = button.getToggleState();

        juce::Colour bright(0xffc8dce8);  // cool blue-white
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

        // Check if this is an OLED or cream button
        bool useOled = isOledAnimatedButton(text) || text == "REC" || text == "PANIC"
                       || text == "M2" || text == "..." || text == "PHN";
        auto dispBounds = button.getLocalBounds().toFloat().reduced(2.0f);
        if (useOled)
            g.setColour(on ? bright : bright.withAlpha(0.7f));
        else
            g.setColour(on ? juce::Colour(0xff2a3038) : juce::Colour(0xff4a4e54));
        g.setFont(juce::Font(getUIFontName(),
                   juce::jmin(12.0f, dispBounds.getHeight() * 0.5f), juce::Font::bold));
        g.drawText(formatButtonText(text), button.getLocalBounds().reduced(2),
                   juce::Justification::centred);
    }

    // OLED-style combo box — dark background
    void drawComboBox(juce::Graphics& g, int width, int height, bool,
                      int, int, int, int, juce::ComboBox&) override
    {
        auto bounds = juce::Rectangle<float>(0, 0, static_cast<float>(width), static_cast<float>(height));

        g.setColour(juce::Colour(0xff2a3038));
        g.fillRoundedRectangle(bounds, 4.0f);
        g.setColour(juce::Colour(0xff3a4248));
        g.drawRoundedRectangle(bounds, 4.0f, 0.8f);

        auto arrowZone = bounds.removeFromRight(20.0f).reduced(5.0f);
        juce::Path arrow;
        arrow.addTriangle(arrowZone.getX(), arrowZone.getCentreY() - 3,
                         arrowZone.getRight(), arrowZone.getCentreY() - 3,
                         arrowZone.getCentreX(), arrowZone.getCentreY() + 3);
        g.setColour(juce::Colour(0xffc8dce8));
        g.fillPath(arrow);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IoniqLookAndFeel)
};
