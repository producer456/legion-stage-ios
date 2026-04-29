#pragma once

#include "DawLookAndFeel.h"

// "Launchkey" theme — inspired by the Novation Launchkey Mini 37 MK4
// in white.  Clean off-white chassis, soft pastel pad glow, tiny
// green/red LED indicators for play/record, dark-teal OLED screen.
// Auto-applied on boot when a Launchkey is detected.
class LaunchkeyLookAndFeel : public DawLookAndFeel
{
public:
    LaunchkeyLookAndFeel()
    {
        // ── Surfaces — bright white plastic with subtle warm tint ──
        theme.body        = 0xfff6f4f0;  // off-white chassis
        theme.bodyLight   = 0xfffcfbf8;  // brightest white panels
        theme.bodyDark    = 0xffe6e3dd;  // recessed gray panel
        theme.border      = 0xffcecbc4;  // soft mid-gray
        theme.borderLight = 0xffaaa8a2;  // edge shadow

        // ── Text — charcoal like the LAUNCHKEY logo ──
        theme.textPrimary   = 0xff363432;
        theme.textSecondary = 0xff7c7a76;
        theme.textBright    = 0xff1a1816;

        // ── Accents — Launchkey LED palette ──
        theme.red       = 0xffdd3a3a;   // record LED red
        theme.redDark   = 0xfff0d4d4;   // pale rose background
        theme.amber     = 0xff4d8a9c;   // OLED teal accent
        theme.amberDark = 0xff476a6a;
        theme.green     = 0xff42c64a;   // play LED green
        theme.greenDark = 0xffd8eed8;   // pale mint background

        // ── LCD / OLED — deep teal-black with cool white text ──
        theme.lcdBg    = 0xff0a1014;
        theme.lcdText  = 0xffd2e4e8;
        theme.lcdAmber = 0xffe2f0f4;

        // ── Buttons — soft gray, like the device's button caps ──
        theme.buttonFace  = 0xffe2e0da;
        theme.buttonHover = 0xffd6d4ce;
        theme.buttonDown  = 0xffc4c2bc;

        theme.btnStop        = 0xffe2e0da;
        theme.btnMetronome   = 0xffe2e0da;
        theme.btnMetronomeOn = 0xffc4dadc;
        theme.btnCountIn     = 0xffe2e0da;
        theme.btnCountInOn   = 0xffc4dadc;
        theme.btnNewClip     = 0xffe2e0da;
        theme.btnDeleteClip  = 0xffe8d0d0;
        theme.btnDuplicate   = 0xffe2e0da;
        theme.btnSplit       = 0xffe2e0da;
        theme.btnQuantize    = 0xffe2e0da;
        theme.btnEditNotes   = 0xffe2e0da;
        theme.btnNav         = 0xffe2e0da;
        theme.btnSave        = 0xffe2e0da;
        theme.btnLoad        = 0xffe2e0da;
        theme.btnUndoRedo    = 0xffe2e0da;
        theme.btnMidi2       = 0xffe2e0da;
        theme.btnMidi2On     = 0xffc4dadc;
        theme.btnLoop        = 0xffe2e0da;
        theme.btnLoopOn      = 0xffc4dadc;
        theme.loopRegion     = 0x204d8a9c;
        theme.loopBorder     = 0xff4d8a9c;

        // ── Timeline — bright cream with subtle warm grid ──
        theme.timelineBg          = 0xfffbf9f5;
        theme.timelineAltRow      = 0xfff2efe9;
        theme.timelineSelectedRow = 0xffd0e4e8;   // pale OLED-cyan tint
        theme.timelineGridMajor   = 0xffb8b6b0;
        theme.timelineGridMinor   = 0xffd6d4ce;
        theme.timelineGridFaint   = 0xffeae7e2;
        theme.timelineGridBeat    = 0xffcac8c2;

        // ── Clips — soft pastels matching the device's pad LED palette ──
        theme.clipDefault     = 0xffd2d8e8;  // pale indigo (idle)
        theme.clipRecording   = 0xffd8b8b8;  // muted rose (matches kRed)
        theme.clipQueued      = 0xffd8c8a4;  // soft amber (matches kAmber)
        theme.clipPlaying     = 0xffb8d4be;  // sage (matches kGreen)
        theme.clipNotePreview = 0xcc1a1816;

        // ── Playhead — dark slate needle ──
        theme.playhead     = 0xcc2a3038;
        theme.playheadGlow = 0x182a3038;

        theme.accentStripe = 0xff4d8a9c;  // OLED-teal stripe

        theme.trackSelected = 0xffd0e4e8;
        theme.trackArmed    = 0xfff0d4d4;
        theme.trackMuteOn   = 0xff4d8a9c;
        theme.trackSoloOn   = 0xffe8a840;
        theme.trackSoloText = 0xfffcfbf8;

        applyThemeColors();

        // OLED-style combo box text
        setColour(juce::ComboBox::textColourId,                   juce::Colour(0xffd2e4e8));
        setColour(juce::PopupMenu::backgroundColourId,            juce::Colour(0xff0a1014));
        setColour(juce::PopupMenu::textColourId,                  juce::Colour(0xffd2e4e8));
        setColour(juce::PopupMenu::highlightedTextColourId,       juce::Colour(0xff0a1014));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xffd2e4e8));

        setColour(juce::Slider::textBoxTextColourId,       juce::Colour(0xffd2e4e8));
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff0a1014));
        setColour(juce::Slider::textBoxOutlineColourId,    juce::Colour(0x00000000));

        setColour(juce::TextButton::buttonOnColourId,      juce::Colour(0xff1c2630));
        setColour(juce::TextButton::textColourOnId,        juce::Colour(0xffd2e4e8));
    }

    float getButtonRadius() const override { return 7.0f; }
    juce::String getUIFontName() const override { return "Plus Jakarta Sans"; }
    int getSidePanelWidth() const override { return 6; }

    // Smooth white side panels with a soft inner shadow
    void drawSidePanels(juce::Graphics& g, int width, int height) override
    {
        const int panelW = getSidePanelWidth();
        juce::Colour edge(0xffe6e3dd);
        juce::Colour inner(0xfff6f4f0);

        g.setGradientFill(juce::ColourGradient(edge, 0.0f, 0.0f,
                                               inner, (float) panelW, 0.0f, false));
        g.fillRect(0, 0, panelW, height);

        g.setGradientFill(juce::ColourGradient(inner, (float)(width - panelW), 0.0f,
                                               edge,  (float) width, 0.0f, false));
        g.fillRect(width - panelW, 0, panelW, height);

        g.setColour(juce::Colour(0x18000000));
        g.fillRect((float)(panelW - 1), 0.0f, 1.0f, (float) height);
        g.fillRect((float)(width - panelW), 0.0f, 1.0f, (float) height);
    }

    void invalidateCaches() override { topBarCacheW = 0; topBarCacheH = 0; }

    void drawInnerStrip(juce::Graphics& g, int x, int /*y*/, int width, int height) override
    {
        g.setColour(juce::Colour(0xffeae7e2));
        g.fillRect(x, 0, width, height);
        g.setColour(juce::Colour(0x18000000));
        g.drawVerticalLine(x, 0.0f, (float) height);
        g.drawVerticalLine(x + width - 1, 0.0f, (float) height);
    }

    // Bright white top bar with a teal accent line
    void drawTopBarBackground(juce::Graphics& g, int x, int y, int width, int height) override
    {
        if (width != topBarCacheW || height != topBarCacheH)
        {
            topBarCache = juce::Image(juce::Image::RGB, width, height, false);
            juce::Graphics tg(topBarCache);
            tg.setColour(juce::Colour(0xfffcfbf8));
            tg.fillAll();
            tg.setColour(juce::Colour(0xff4d8a9c));
            tg.fillRect(0, height - 1, width, 1);
            topBarCacheW = width;
            topBarCacheH = height;
        }
        g.drawImageAt(topBarCache, x, y);
    }

    mutable juce::Image topBarCache;
    mutable int topBarCacheW = 0, topBarCacheH = 0;

    // Mix of OLED buttons (transport) and soft-gray tactile buttons (toolbar)
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour&,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        auto text = button.getButtonText();

        // Launchkey toolbar-pad button — radial "lit-from-within"
        // glow that mimics the device's LED-backed pads: bright
        // center, soft falloff, slightly darker edge, thin dark rim
        // for the pad housing.  See applyLaunchkeyToolbarColors().
        if (button.getProperties().contains("lkColor"))
        {
            auto base = juce::Colour((juce::uint32) (int) button.getProperties()["lkColor"]);
            if (shouldDrawButtonAsDown)             base = base.darker(0.20f);
            else if (shouldDrawButtonAsHighlighted) base = base.brighter(0.10f);
            const auto core = base.brighter(0.28f);
            const auto edge = base.darker(0.22f);
            // Center the radial glow slightly above-middle for a
            // subtle "light source from above" feel.
            const float cx = bounds.getCentreX();
            const float cy = bounds.getY() + bounds.getHeight() * 0.42f;
            const float r  = juce::jmax(bounds.getWidth(), bounds.getHeight()) * 0.55f;
            juce::ColourGradient grad(core, cx, cy, edge, cx + r, cy, true);
            grad.addColour(0.55, base);
            g.setGradientFill(grad);
            g.fillRoundedRectangle(bounds, 4.0f);
            g.setColour(base.darker(0.45f));
            g.drawRoundedRectangle(bounds, 4.0f, 0.7f);
            return;
        }

        const bool useOled = isOledAnimatedButton(text) || text == "REC" || text == "PANIC"
                          || text == "M2" || text == "..." || text == "PHN" || text == "E";

        if (useOled)
        {
            g.setColour(juce::Colour(0xffd6d4ce));
            g.fillRoundedRectangle(bounds, 5.0f);
            auto inset = bounds.reduced(1.5f);
            g.setColour(juce::Colour(0xff8a8884));
            g.drawRoundedRectangle(inset, 4.0f, 1.0f);

            auto screen = bounds.reduced(2.5f);
            g.setColour(juce::Colour(theme.lcdBg));
            g.fillRoundedRectangle(screen, 3.0f);

            if (shouldDrawButtonAsDown)        g.setColour(juce::Colour(0xff4d8a9c).withAlpha(0.4f));
            else if (shouldDrawButtonAsHighlighted) g.setColour(juce::Colour(0xff2a363a));
            else                                g.setColour(juce::Colour(0xff1a2024));
            g.drawRoundedRectangle(screen, 3.0f, 0.5f);
        }
        else
        {
            // Soft gray tactile button — like the device's button caps
            if (shouldDrawButtonAsDown)        g.setColour(juce::Colour(0xffc4c2bc));
            else if (shouldDrawButtonAsHighlighted) g.setColour(juce::Colour(0xffd6d4ce));
            else                                g.setColour(juce::Colour(0xffe2e0da));
            g.fillRoundedRectangle(bounds, getButtonRadius());
            g.setColour(juce::Colour(0xffcac8c2));
            g.drawRoundedRectangle(bounds, getButtonRadius(), 0.8f);
        }
    }

    bool drawOledButtonArt(juce::Image& oled, const juce::String& text,
                           bool on, float t, juce::Colour, juce::Colour) const override
    {
        juce::Colour bright(0xffd2e4e8);
        juce::Colour dim(0xff4a5a64);

        if (text == "REC")
        {
            juce::Colour col = on ? juce::Colour(0xffdd3a3a) : dim;
            float pulse = on ? (0.5f + 0.5f * std::sin(t * 8.0f)) : 1.0f;
            int icx = OLED_W / 2;
            int icy = OLED_H / 2;
            oledCircle(oled, icx, icy, 3, col.withAlpha(pulse), true);
            if (on)
            {
                int ringR = 4 + (static_cast<int>(t * 6.0f) % 3);
                oledCircle(oled, icx, icy, ringR, juce::Colour(0xffdd3a3a).withAlpha(0.3f));
            }
            return true;
        }

        return DawLookAndFeel::drawOledButtonArt(oled, text, on, t, bright, dim);
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool) override
    {
        auto text = button.getButtonText();
        bool on = button.getToggleState();

        juce::Colour bright(0xffd2e4e8);
        juce::Colour dim(0xff4a5a64);

        // Launchkey toolbar-pad buttons get white-on-color text.
        if (button.getProperties().contains("lkColor"))
        {
            auto dispBounds = button.getLocalBounds().toFloat().reduced(2.0f);
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(getUIFontName(),
                      juce::jmin(12.0f, dispBounds.getHeight() * 0.5f), juce::Font::bold));
            g.drawText(formatButtonText(text), button.getLocalBounds().reduced(2),
                       juce::Justification::centred);
            return;
        }

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
            g.setColour(on ? juce::Colour(0xff1a2024) : juce::Colour(0xff363432));
        g.setFont(juce::Font(getUIFontName(),
                  juce::jmin(12.0f, dispBounds.getHeight() * 0.5f), juce::Font::bold));
        g.drawText(formatButtonText(text), button.getLocalBounds().reduced(2),
                   juce::Justification::centred);
    }

    void drawComboBox(juce::Graphics& g, int width, int height, bool,
                      int, int, int, int, juce::ComboBox&) override
    {
        auto bounds = juce::Rectangle<float>(0, 0, (float) width, (float) height);
        g.setColour(juce::Colour(0xffd6d4ce));
        g.fillRoundedRectangle(bounds, 5.0f);
        auto inset = bounds.reduced(1.5f);
        g.setColour(juce::Colour(0xff8a8884));
        g.drawRoundedRectangle(inset, 4.0f, 1.0f);
        auto screen = bounds.reduced(2.5f);
        g.setColour(juce::Colour(0xff0a1014));
        g.fillRoundedRectangle(screen, 3.0f);
        g.setColour(juce::Colour(0xff1a2024));
        g.drawRoundedRectangle(screen, 3.0f, 0.5f);

        auto arrowZone = screen.removeFromRight(18.0f).reduced(4.0f);
        juce::Path arrow;
        arrow.addTriangle(arrowZone.getX(),       arrowZone.getCentreY() - 3,
                          arrowZone.getRight(),   arrowZone.getCentreY() - 3,
                          arrowZone.getCentreX(), arrowZone.getCentreY() + 3);
        g.setColour(juce::Colour(0xffd2e4e8));
        g.fillPath(arrow);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LaunchkeyLookAndFeel)
};
