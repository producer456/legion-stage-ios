#pragma once

#include "DawLookAndFeel.h"

// "Blueprint" theme — looks like architectural/engineering design plans.
// Deep blue background with white technical line drawings, grid paper,
// dimension annotations, and dashed construction lines.
class BlueprintLookAndFeel : public DawLookAndFeel
{
public:
    static constexpr uint32_t bpBlue     = 0xff1a2a48;  // deep blueprint blue
    static constexpr uint32_t bpBlueLt   = 0xff223458;  // lighter panel
    static constexpr uint32_t bpBlueDk   = 0xff121e38;  // darker area
    static constexpr uint32_t bpLine     = 0xffc8d8e8;  // white-blue line color
    static constexpr uint32_t bpLineDim  = 0xff4a6080;  // dim construction line
    static constexpr uint32_t bpGrid     = 0xff1e3050;  // subtle grid
    static constexpr uint32_t bpGridMaj  = 0xff2a3c60;  // major grid
    static constexpr uint32_t bpWhite    = 0xffe8f0f8;  // bright annotation
    static constexpr uint32_t bpRed      = 0xffdd6655;  // markup red
    static constexpr uint32_t bpGreen    = 0xff55bb88;  // approval green
    static constexpr uint32_t bpYellow   = 0xffeedd66;  // highlight/caution

    BlueprintLookAndFeel()
    {
        // ── Surfaces — blueprint blue paper ──
        theme.body        = bpBlue;
        theme.bodyLight   = bpBlueLt;
        theme.bodyDark    = bpBlueDk;
        theme.border      = bpLineDim;
        theme.borderLight = bpLine;

        // ── Text — white technical annotations ──
        theme.textPrimary   = bpLine;
        theme.textSecondary = bpLineDim;
        theme.textBright    = bpWhite;

        // ── Accent ──
        theme.red       = bpRed;
        theme.redDark   = 0xff993322;
        theme.amber     = bpLine;
        theme.amberDark = bpLineDim;
        theme.green     = bpGreen;
        theme.greenDark = 0xff338855;

        // ── LCD — schematic label style ──
        theme.lcdBg    = bpBlueDk;
        theme.lcdText  = bpWhite;
        theme.lcdAmber = bpLine;

        // ── Buttons — outlined boxes like schematic symbols ──
        theme.buttonFace  = 0x10c8d8e8;  // nearly transparent
        theme.buttonHover = 0x20c8d8e8;
        theme.buttonDown  = 0x30c8d8e8;

        theme.btnStop       = 0x10c8d8e8;
        theme.btnMetronome  = 0x10c8d8e8;
        theme.btnMetronomeOn = 0x25c8d8e8;
        theme.btnCountIn    = 0x10c8d8e8;
        theme.btnCountInOn  = 0x25c8d8e8;
        theme.btnNewClip    = 0x10c8d8e8;
        theme.btnDeleteClip = 0x10c8d8e8;
        theme.btnDuplicate  = 0x10c8d8e8;
        theme.btnSplit      = 0x10c8d8e8;
        theme.btnQuantize   = 0x10c8d8e8;
        theme.btnEditNotes  = 0x10c8d8e8;
        theme.btnNav        = 0x10c8d8e8;
        theme.btnSave       = 0x10c8d8e8;
        theme.btnLoad       = 0x10c8d8e8;
        theme.btnUndoRedo   = 0x10c8d8e8;
        theme.btnMidi2      = 0x10c8d8e8;
        theme.btnMidi2On    = 0x25c8d8e8;
        theme.btnLoop       = 0x10c8d8e8;
        theme.btnLoopOn     = 0x25c8d8e8;
        theme.loopRegion    = 0x18c8d8e8;
        theme.loopBorder    = bpLine;

        // ── Timeline — blueprint grid ──
        theme.timelineBg         = bpBlueDk;
        theme.timelineAltRow     = 0xff162640;
        theme.timelineSelectedRow = 0xff1e3458;
        theme.timelineGridMajor  = bpLine;
        theme.timelineGridMinor  = bpLineDim;
        theme.timelineGridFaint  = bpGrid;
        theme.timelineGridBeat   = bpGridMaj;

        // ── Clips — outlined boxes ──
        theme.clipDefault   = 0x20c8d8e8;
        theme.clipRecording = 0x30dd6655;
        theme.clipQueued    = 0x20eedd66;
        theme.clipPlaying   = 0x2055bb88;

        // ── Playhead ──
        theme.playhead     = 0xdde8f0f8;
        theme.playheadGlow = 0x22c8d8e8;

        theme.accentStripe = bpLine;

        theme.trackSelected = 0xff1e3458;
        theme.trackArmed    = 0x30dd6655;
        theme.trackMuteOn   = bpRed;
        theme.trackSoloOn   = bpYellow;
        theme.trackSoloText = bpBlueDk;

        applyThemeColors();

        // Override for blueprint look
        setColour(juce::Slider::textBoxTextColourId,       juce::Colour(bpWhite));
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(bpBlueDk));
        setColour(juce::Slider::textBoxOutlineColourId,    juce::Colour(bpLineDim));
        setColour(juce::ComboBox::textColourId,            juce::Colour(bpLine));
        setColour(juce::PopupMenu::textColourId,           juce::Colour(bpLine));
    }

    float getButtonRadius() const override { return 1.0f; }

    // Blueprint buttons — thin outlined rectangles like schematic symbols
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour&,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);

        // Fill — very subtle
        if (shouldDrawButtonAsDown)
            g.setColour(juce::Colour(bpLine).withAlpha(0.15f));
        else if (shouldDrawButtonAsHighlighted)
            g.setColour(juce::Colour(bpLine).withAlpha(0.08f));
        else
            g.setColour(juce::Colour(bpLine).withAlpha(0.03f));
        g.fillRect(bounds);

        // Outline — thin technical line
        bool toggled = button.getToggleState();
        g.setColour(juce::Colour(toggled ? bpWhite : bpLineDim));
        g.drawRect(bounds, toggled ? 1.5f : 0.8f);

        // Corner dimension marks (tiny L-shapes at corners)
        float markLen = 4.0f;
        g.setColour(juce::Colour(bpLineDim).withAlpha(0.5f));
        // Top-left
        g.drawLine(bounds.getX(), bounds.getY(), bounds.getX() + markLen, bounds.getY(), 0.5f);
        g.drawLine(bounds.getX(), bounds.getY(), bounds.getX(), bounds.getY() + markLen, 0.5f);
        // Bottom-right
        g.drawLine(bounds.getRight(), bounds.getBottom(), bounds.getRight() - markLen, bounds.getBottom(), 0.5f);
        g.drawLine(bounds.getRight(), bounds.getBottom(), bounds.getRight(), bounds.getBottom() - markLen, 0.5f);
    }

    // Combo box — outlined field with annotation arrow
    void drawComboBox(juce::Graphics& g, int width, int height, bool,
                      int, int, int, int, juce::ComboBox&) override
    {
        auto bounds = juce::Rectangle<float>(0, 0, static_cast<float>(width), static_cast<float>(height));

        g.setColour(juce::Colour(bpBlueDk));
        g.fillRect(bounds);
        g.setColour(juce::Colour(bpLineDim));
        g.drawRect(bounds, 0.8f);

        // Arrow
        auto arrowZone = bounds.removeFromRight(18.0f).reduced(4.0f);
        juce::Path arrow;
        arrow.addTriangle(arrowZone.getX(), arrowZone.getCentreY() - 3,
                         arrowZone.getRight(), arrowZone.getCentreY() - 3,
                         arrowZone.getCentreX(), arrowZone.getCentreY() + 3);
        g.setColour(juce::Colour(bpLine));
        g.fillPath(arrow);
    }

    // Side panels — white oak wood cheeks
    int getSidePanelWidth() const override { return 18; }

    void drawSidePanels(juce::Graphics& g, int width, int height) override
    {
        int panelW = getSidePanelWidth();
        auto leftPanel = juce::Rectangle<float>(0, 0, static_cast<float>(panelW), static_cast<float>(height));
        auto rightPanel = juce::Rectangle<float>(static_cast<float>(width - panelW), 0,
                                                  static_cast<float>(panelW), static_cast<float>(height));

        juce::Colour oakBase(0xffc8bda8);
        juce::Colour oakLight(0xffd6ccba);
        juce::Colour oakDark(0xffb0a490);
        juce::Colour oakGrain(0xffa89880);

        g.setColour(oakBase);
        g.fillRect(leftPanel);
        g.fillRect(rightPanel);

        juce::Random rng(42);
        for (int i = 0; i < 40; ++i)
        {
            float x = rng.nextFloat() * panelW;
            float grainWidth = 0.5f + rng.nextFloat() * 1.0f;
            float alpha = 0.08f + rng.nextFloat() * 0.15f;
            bool isLight = rng.nextBool();

            g.setColour((isLight ? oakLight : oakGrain).withAlpha(alpha));

            juce::Path grain;
            grain.startNewSubPath(x, 0);
            for (int y = 0; y < height; y += 20)
            {
                float wobble = std::sin(static_cast<float>(y) * 0.015f + i * 0.7f) * 1.5f;
                grain.lineTo(x + wobble, static_cast<float>(y));
            }
            grain.lineTo(x, static_cast<float>(height));

            g.strokePath(grain, juce::PathStrokeType(grainWidth));

            auto rightGrain = grain;
            rightGrain.applyTransform(juce::AffineTransform::translation(
                static_cast<float>(width - panelW), 0));
            g.strokePath(rightGrain, juce::PathStrokeType(grainWidth));
        }

        // Inner shadow where wood meets blueprint
        g.setColour(juce::Colour(0x30000000));
        g.fillRect(static_cast<float>(panelW - 2), 0.0f, 2.0f, static_cast<float>(height));
        g.fillRect(static_cast<float>(width - panelW), 0.0f, 2.0f, static_cast<float>(height));

        // Outer highlight
        g.setColour(oakLight.withAlpha(0.3f));
        g.fillRect(0.0f, 0.0f, 1.0f, static_cast<float>(height));
        g.fillRect(static_cast<float>(width - 1), 0.0f, 1.0f, static_cast<float>(height));
    }

    // Top bar — blueprint header with title block style
    void drawTopBarBackground(juce::Graphics& g, int x, int y, int width, int height) override
    {
        g.setColour(juce::Colour(bpBlueLt));
        g.fillRect(x, y, width, height);

        // Grid
        g.setColour(juce::Colour(bpGrid));
        for (int gy = y; gy < y + height; gy += 10)
            g.drawHorizontalLine(gy, static_cast<float>(x), static_cast<float>(x + width));
        for (int gx = x; gx < x + width; gx += 10)
            g.drawVerticalLine(gx, static_cast<float>(y), static_cast<float>(y + height));

        // Title block border
        g.setColour(juce::Colour(bpLineDim));
        g.drawRect(x, y, width, height, 1);

        // "LEGION STAGE" annotation in top-right
        g.setColour(juce::Colour(bpLineDim).withAlpha(0.4f));
        g.setFont(juce::Font(8.0f));
        g.drawText("LEGION STAGE  REV 1.1.0", x + width - 200, y + 2, 195, 10,
                   juce::Justification::centredRight);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BlueprintLookAndFeel)
};
