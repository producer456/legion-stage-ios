#pragma once

#include "DawLookAndFeel.h"

// "KeyLab 88 MkII" — modeled on the white Arturia KeyLab 88 MkII.
// Borrows the Ioniq theme's restrained aesthetic: subtle textures
// (matte chassis dither + walnut grain), recessed OLED bezels for
// transport/animated buttons, and soft cream tactile buttons for the
// toolbar.  Cobalt-blue STN display matches the device's actual LCD.
class KeyLab88LookAndFeel : public DawLookAndFeel
{
public:
    KeyLab88LookAndFeel()
    {
        // ── Surfaces — warm matte white aluminium ──
        theme.body        = 0xfff5f3ee;
        theme.bodyLight   = 0xfffcfaf5;
        theme.bodyDark    = 0xffe8e4dc;
        theme.border      = 0xff5a3818;   // dark walnut end-cap shadow
        theme.borderLight = 0xff7a4e2c;   // walnut grain highlight

        // ── Text — black silkscreen on white panel ──
        theme.textPrimary   = 0xff1a1816;
        theme.textSecondary = 0xff5a5854;
        theme.textBright    = 0xff000000;

        // ── Accents — Arturia transport-LED palette, slightly muted
        //              to fit the Ioniq-style restraint.
        theme.red       = 0xffc04848;
        theme.redDark   = 0xfff0d4d4;
        theme.amber     = 0xffd89030;
        theme.amberDark = 0xfff0e0c4;
        theme.green     = 0xff4a9c52;
        theme.greenDark = 0xffd8eed8;

        // ── LCD — cobalt STN display, off-white text ──
        theme.lcdBg    = 0xff1a3a8a;
        theme.lcdText  = 0xffe8efff;
        theme.lcdAmber = 0xffffffff;

        // ── Buttons — soft cream tactile caps ──
        theme.buttonFace  = 0xffe8e4d8;
        theme.buttonHover = 0xffdcd6c8;
        theme.buttonDown  = 0xffc8c2b4;

        theme.btnStop        = 0xffe8e4d8;
        theme.btnMetronome   = 0xffe8e4d8;
        theme.btnMetronomeOn = 0xfff0dcc0;
        theme.btnCountIn     = 0xffe8e4d8;
        theme.btnCountInOn   = 0xfff0dcc0;
        theme.btnNewClip     = 0xffe8e4d8;
        theme.btnDeleteClip  = 0xfff0d4d4;
        theme.btnDuplicate   = 0xffe8e4d8;
        theme.btnSplit       = 0xffe8e4d8;
        theme.btnQuantize    = 0xffe8e4d8;
        theme.btnEditNotes   = 0xffe8e4d8;
        theme.btnNav         = 0xffe8e4d8;
        theme.btnSave        = 0xffe8e4d8;
        theme.btnLoad        = 0xffe8e4d8;
        theme.btnUndoRedo    = 0xffe8e4d8;
        theme.btnMidi2       = 0xffe8e4d8;
        theme.btnMidi2On     = 0xffd0deef;
        theme.btnLoop        = 0xffe8e4d8;
        theme.btnLoopOn      = 0xffd0deef;
        theme.loopRegion     = 0x281a3a8a;
        theme.loopBorder     = 0xff1a3a8a;

        // ── Timeline — bright cream with subtle warm grid ──
        theme.timelineBg          = 0xfff8f5ee;
        theme.timelineAltRow      = 0xffefebe2;
        theme.timelineSelectedRow = 0xffd8e0ef;
        theme.timelineGridMajor   = 0xffb8b0a0;
        theme.timelineGridMinor   = 0xffd6cebe;
        theme.timelineGridFaint   = 0xffe8e2d8;
        theme.timelineGridBeat    = 0xffc4bcac;

        // ── Clips — soft pastels matching the device pad palette ──
        theme.clipDefault     = 0xffd2d8e8;
        theme.clipRecording   = 0xffd8b8b8;
        theme.clipQueued      = 0xffd8c8a4;
        theme.clipPlaying     = 0xffb8d4be;
        theme.clipNotePreview = 0xcc1a1816;

        // ── Playhead — cobalt needle ──
        theme.playhead     = 0xcc1a3a8a;
        theme.playheadGlow = 0x181a3a8a;

        theme.accentStripe  = 0xff5a3818;        // walnut trim line

        theme.trackSelected = 0xffd8e0ef;
        theme.trackArmed    = 0xfff0d4d4;
        theme.trackMuteOn   = 0xff1a3a8a;
        theme.trackSoloOn   = 0xffe8a840;
        theme.trackSoloText = 0xfffcfaf5;

        applyThemeColors();

        // OLED-style combobox + popup menu (cobalt LCD look)
        setColour(juce::ComboBox::textColourId,                   juce::Colour(0xffe8efff));
        setColour(juce::PopupMenu::backgroundColourId,            juce::Colour(0xff1a3a8a));
        setColour(juce::PopupMenu::textColourId,                  juce::Colour(0xffe8efff));
        setColour(juce::PopupMenu::highlightedTextColourId,       juce::Colour(0xff1a3a8a));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xffe8efff));

        setColour(juce::Slider::textBoxTextColourId,       juce::Colour(0xffe8efff));
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff1a3a8a));
        setColour(juce::Slider::textBoxOutlineColourId,    juce::Colour(0x00000000));

        setColour(juce::TextButton::buttonOnColourId,      juce::Colour(0xff264a9a));
        setColour(juce::TextButton::textColourOnId,        juce::Colour(0xffe8efff));
    }

    float getButtonRadius() const override { return 5.0f; }
    juce::String getUIFontName() const override { return "Avenir Next"; }
    int getSidePanelWidth() const override { return 22; }   // wood end-caps

    void invalidateCaches() override
    {
        sideCacheW = 0; sideCacheH = 0;
        topBarCacheW = 0; topBarCacheH = 0;
    }

    // Walnut end-caps with horizontal grain lines (cached image so the
    // grain isn't re-randomised every frame).
    void drawSidePanels(juce::Graphics& g, int width, int height) override
    {
        if (width != sideCacheW || height != sideCacheH)
        {
            sideCache = juce::Image(juce::Image::ARGB, width, height, true);
            juce::Graphics sg(sideCache);

            const int panelW = getSidePanelWidth();
            juce::Colour edge   (0xff3d2510);   // shadowed outer edge
            juce::Colour mid    (0xff5a3818);   // base walnut
            juce::Colour highlight(0xff7a4e2c); // grain highlight

            // Left cap — gradient from outer shadow to inner highlight
            sg.setGradientFill(juce::ColourGradient(edge, 0.0f, 0.0f,
                                                    highlight, (float) panelW, 0.0f, false));
            sg.fillRect(0, 0, panelW, height);
            // Right cap — mirrored
            sg.setGradientFill(juce::ColourGradient(highlight, (float)(width - panelW), 0.0f,
                                                    edge, (float) width, 0.0f, false));
            sg.fillRect(width - panelW, 0, panelW, height);

            // Horizontal wood-grain striations
            juce::Random rng(31);
            for (int y = 0; y < height; y += 2)
            {
                float a = 0.04f + rng.nextFloat() * 0.10f;
                sg.setColour(mid.withAlpha(a));
                sg.drawHorizontalLine(y, 0.0f, (float) panelW);
                sg.drawHorizontalLine(y, (float)(width - panelW), (float) width);

                if (rng.nextFloat() < 0.18f)
                {
                    sg.setColour(highlight.withAlpha(0.18f));
                    sg.drawHorizontalLine(y, 0.0f, (float) panelW);
                    sg.drawHorizontalLine(y, (float)(width - panelW), (float) width);
                }
            }

            // Inner edge shadow where wood meets chassis
            sg.setColour(juce::Colour(0x60000000));
            sg.fillRect((float)(panelW - 1), 0.0f, 1.0f, (float) height);
            sg.fillRect((float)(width - panelW), 0.0f, 1.0f, (float) height);

            sideCacheW = width; sideCacheH = height;
        }
        g.drawImageAt(sideCache, 0, 0);
    }

    // Recessed inner strip with the warm cream chassis tint.
    void drawInnerStrip(juce::Graphics& g, int x, int /*y*/, int width, int height) override
    {
        juce::Colour base   (0xffe6e2d8);
        juce::Colour deeper (0xffd8d4ca);
        g.setGradientFill(juce::ColourGradient(base,   (float) x,                 0.0f,
                                               deeper, (float) (x + width),       0.0f, false));
        g.fillRect(x, 0, width, height);

        g.setColour(juce::Colour(0x18000000));
        g.drawVerticalLine(x, 0.0f, (float) height);
        g.drawVerticalLine(x + width - 1, 0.0f, (float) height);
    }

    // Matte-white top bar with very faint plastic-grain dither and a
    // walnut accent line along the bottom edge (cached image).
    void drawTopBarBackground(juce::Graphics& g, int x, int y, int width, int height) override
    {
        if (width != topBarCacheW || height != topBarCacheH)
        {
            topBarCache = juce::Image(juce::Image::RGB, width, height, false);
            juce::Graphics tg(topBarCache);

            juce::ColourGradient grad(juce::Colour(0xfffcfaf5), 0.0f, 0.0f,
                                      juce::Colour(0xfff0ede5), 0.0f, (float) height, false);
            tg.setGradientFill(grad);
            tg.fillAll();

            // Subtle matte-plastic dither.
            juce::Random rng(57);
            for (int py = 0; py < height; py += 2)
                for (int px = 0; px < width; px += 2)
                {
                    float a = rng.nextFloat() * 0.025f;
                    tg.setColour(juce::Colour(0xff000000).withAlpha(a));
                    tg.fillRect((float) px, (float) py, 1.0f, 1.0f);
                }

            // Walnut trim accent rule along the bottom edge.
            tg.setColour(juce::Colour(0xff5a3818));
            tg.fillRect(0, height - 1, width, 1);

            topBarCacheW = width; topBarCacheH = height;
        }
        g.drawImageAt(topBarCache, x, y);
    }

    mutable juce::Image sideCache;
    mutable int sideCacheW = 0, sideCacheH = 0;
    mutable juce::Image topBarCache;
    mutable int topBarCacheW = 0, topBarCacheH = 0;

    // Buttons: OLED-style recessed bezel for transport/animated, soft
    // cream tactile cap for everything else.  Mirrors the Ioniq pattern.
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour&,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        const auto text = button.getButtonText();

        const bool useOled = isOledAnimatedButton(text) || text == "REC" || text == "PANIC"
                          || text == "M2" || text == "..." || text == "PHN" || text == "E";

        if (useOled)
        {
            // Outer bezel (light cream surround → raised rim feel)
            g.setColour(juce::Colour(0xffd8d4ca));
            g.fillRoundedRectangle(bounds, 5.0f);
            auto inset = bounds.reduced(1.5f);
            g.setColour(juce::Colour(0xff8a8478));
            g.drawRoundedRectangle(inset, 4.0f, 1.0f);

            // Recessed cobalt screen
            auto screen = bounds.reduced(2.5f);
            g.setColour(juce::Colour(theme.lcdBg));
            g.fillRoundedRectangle(screen, 3.0f);

            if (shouldDrawButtonAsDown)             g.setColour(juce::Colour(0xffe8efff).withAlpha(0.4f));
            else if (shouldDrawButtonAsHighlighted) g.setColour(juce::Colour(0xff264a9a));
            else                                    g.setColour(juce::Colour(0xff14306e));
            g.drawRoundedRectangle(screen, 3.0f, 0.5f);
        }
        else
        {
            if (shouldDrawButtonAsDown)             g.setColour(juce::Colour(0xffc8c2b4));
            else if (shouldDrawButtonAsHighlighted) g.setColour(juce::Colour(0xffdcd6c8));
            else                                    g.setColour(juce::Colour(0xffe8e4d8));
            g.fillRoundedRectangle(bounds, getButtonRadius());

            g.setColour(juce::Colour(0xffc0b8aa));
            g.drawRoundedRectangle(bounds, getButtonRadius(), 0.8f);
        }
    }

    bool drawOledButtonArt(juce::Image& oled, const juce::String& text,
                           bool on, float t, juce::Colour, juce::Colour) const override
    {
        juce::Colour bright(0xffe8efff);   // cobalt off-white
        juce::Colour dim   (0xff5a6f9a);   // muted cobalt

        if (text == "REC")
        {
            juce::Colour col = on ? juce::Colour(0xffc04848) : dim;
            float pulse = on ? (0.5f + 0.5f * std::sin(t * 8.0f)) : 1.0f;
            int icx = OLED_W / 2;
            int icy = OLED_H / 2;
            oledCircle(oled, icx, icy, 3, col.withAlpha(pulse), true);
            if (on)
            {
                int ringR = 4 + (static_cast<int>(t * 6.0f) % 3);
                oledCircle(oled, icx, icy, ringR, juce::Colour(0xffc04848).withAlpha(0.3f));
            }
            return true;
        }

        return DawLookAndFeel::drawOledButtonArt(oled, text, on, t, bright, dim);
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool) override
    {
        const auto text = button.getButtonText();
        const bool on   = button.getToggleState();

        juce::Colour bright(0xffe8efff);
        juce::Colour dim   (0xff5a6f9a);

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

        const bool useOled = isOledAnimatedButton(text) || text == "REC" || text == "PANIC"
                          || text == "M2" || text == "..." || text == "PHN" || text == "E";
        auto dispBounds = button.getLocalBounds().toFloat().reduced(2.0f);
        if (useOled)
            g.setColour(on ? bright : bright.withAlpha(0.7f));
        else
            g.setColour(on ? juce::Colour(0xff1a1816) : juce::Colour(0xff3a3834));
        g.setFont(juce::Font(getUIFontName(),
                  juce::jmin(12.0f, dispBounds.getHeight() * 0.5f), juce::Font::bold));
        g.drawText(formatButtonText(text), button.getLocalBounds().reduced(2),
                   juce::Justification::centred);
    }

    // Combobox — recessed cobalt screen with a chrome-cream surround.
    void drawComboBox(juce::Graphics& g, int width, int height, bool,
                      int, int, int, int, juce::ComboBox&) override
    {
        auto bounds = juce::Rectangle<float>(0, 0, (float) width, (float) height);
        g.setColour(juce::Colour(0xffd8d4ca));
        g.fillRoundedRectangle(bounds, 5.0f);
        auto inset = bounds.reduced(1.5f);
        g.setColour(juce::Colour(0xff8a8478));
        g.drawRoundedRectangle(inset, 4.0f, 1.0f);

        auto screen = bounds.reduced(2.5f);
        g.setColour(juce::Colour(0xff1a3a8a));
        g.fillRoundedRectangle(screen, 3.0f);
        g.setColour(juce::Colour(0xff264a9a));
        g.drawRoundedRectangle(screen, 3.0f, 0.5f);

        auto arrowZone = screen.removeFromRight(18.0f).reduced(4.0f);
        juce::Path arrow;
        arrow.addTriangle(arrowZone.getX(),       arrowZone.getCentreY() - 3,
                          arrowZone.getRight(),   arrowZone.getCentreY() - 3,
                          arrowZone.getCentreX(), arrowZone.getCentreY() + 3);
        g.setColour(juce::Colour(0xffe8efff));
        g.fillPath(arrow);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KeyLab88LookAndFeel)
};
