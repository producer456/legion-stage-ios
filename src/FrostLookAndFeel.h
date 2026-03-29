#pragma once

#include "DawLookAndFeel.h"

// "Frost" theme — frosted glass / glassmorphism.
// Semi-transparent panels, white outlines, airy light feel.
// Inspired by translucent infographic with rounded pill shapes.
class FrostLookAndFeel : public DawLookAndFeel
{
public:
    FrostLookAndFeel()
    {
        // ── Surfaces — translucent blue-gray ──
        theme.body        = 0xff3a4a60;
        theme.bodyLight   = 0xff4a5a72;
        theme.bodyDark    = 0xff2e3e52;
        theme.border      = 0xff8898aa;
        theme.borderLight = 0xffaabbcc;

        // ── Text — white ──
        theme.textPrimary   = 0xffffffff;
        theme.textSecondary = 0xffbbccdd;
        theme.textBright    = 0xffffffff;

        // ── Accent — white / ice blue ──
        theme.red       = 0xffee6666;
        theme.redDark   = 0xffaa4444;
        theme.amber     = 0xffffffff;  // white as main accent
        theme.amberDark = 0xffaabbcc;
        theme.green     = 0xff66dd88;
        theme.greenDark = 0xff44aa66;

        // ── LCD — frosted dark ──
        theme.lcdBg    = 0xff2a3a4e;
        theme.lcdText  = 0xffffffff;
        theme.lcdAmber = 0xffddeeff;

        // ── Buttons — frosted glass ──
        theme.buttonFace  = 0xff485a70;
        theme.buttonHover = 0xff556880;
        theme.buttonDown  = 0xff3a4c60;

        theme.btnStop       = 0xff485a70;
        theme.btnMetronome  = 0xff485a70;
        theme.btnMetronomeOn = 0xff4a6a80;
        theme.btnCountIn    = 0xff485a70;
        theme.btnCountInOn  = 0xff4a6a80;
        theme.btnNewClip    = 0xff486a68;
        theme.btnDeleteClip = 0xff6a4858;
        theme.btnDuplicate  = 0xff485870;
        theme.btnSplit      = 0xff5a6048;
        theme.btnQuantize   = 0xff4e5a6a;
        theme.btnEditNotes  = 0xff485870;
        theme.btnNav        = 0xff485a70;
        theme.btnSave       = 0xff486a60;
        theme.btnLoad       = 0xff4e5878;
        theme.btnUndoRedo   = 0xff4e5a6a;
        theme.btnMidi2      = 0xff485a70;
        theme.btnMidi2On    = 0xff4a6a88;
        theme.btnLoop       = 0xff485a70;
        theme.btnLoopOn     = 0xff4a6a88;
        theme.loopRegion    = 0x22ffffff;
        theme.loopBorder    = 0xffccddee;

        // ── Timeline ──
        theme.timelineBg         = 0xff344858;
        theme.timelineAltRow     = 0xff384e60;
        theme.timelineSelectedRow = 0xff406080;
        theme.timelineGridMajor  = 0xffccddee;
        theme.timelineGridMinor  = 0xff6688aa;
        theme.timelineGridFaint  = 0xff3e5060;
        theme.timelineGridBeat   = 0xff506878;

        // ── Clips ──
        theme.clipDefault   = 0xff506878;
        theme.clipRecording = 0xff885555;
        theme.clipQueued    = 0xff587080;
        theme.clipPlaying   = 0xff508068;

        // ── Playhead ──
        theme.playhead     = 0xdd44dd66;
        theme.playheadGlow = 0x3344dd66;

        theme.accentStripe = 0xffffffff;

        theme.trackSelected = 0xff406080;
        theme.trackArmed    = 0xff804444;
        theme.trackMuteOn   = 0xffee6666;
        theme.trackSoloOn   = 0xffffffff;
        theme.trackSoloText = 0xff2e3e52;

        applyThemeColors();

        // White text on buttons
        setColour(juce::TextButton::textColourOnId, juce::Colour(0xff2e3e52));
    }

    float getButtonRadius() const override { return 12.0f; }

    // Frost: pill-shaped frosted buttons with white outline
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        auto baseColour = backgroundColour;

        if (shouldDrawButtonAsDown)
            baseColour = baseColour.darker(0.15f);
        else if (shouldDrawButtonAsHighlighted)
            baseColour = baseColour.brighter(0.1f);

        // Frosted fill
        g.setColour(baseColour.withMultipliedAlpha(0.7f));
        g.fillRoundedRectangle(bounds, 12.0f);

        // White outline
        g.setColour(juce::Colours::white.withAlpha(shouldDrawButtonAsHighlighted ? 0.6f : 0.3f));
        g.drawRoundedRectangle(bounds, 12.0f, 1.0f);
    }

    // Frost: rounded combo box with white outline
    void drawComboBox(juce::Graphics& g, int width, int height, bool,
                      int, int, int, int, juce::ComboBox&) override
    {
        auto bounds = juce::Rectangle<float>(0, 0, static_cast<float>(width), static_cast<float>(height));
        g.setColour(juce::Colour(theme.bodyDark).withMultipliedAlpha(0.7f));
        g.fillRoundedRectangle(bounds, 10.0f);
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 10.0f, 1.0f);

        auto arrowZone = bounds.removeFromRight(22.0f).reduced(6.0f);
        juce::Path arrow;
        arrow.startNewSubPath(arrowZone.getX(), arrowZone.getCentreY() - 2);
        arrow.lineTo(arrowZone.getCentreX(), arrowZone.getCentreY() + 2);
        arrow.lineTo(arrowZone.getRight(), arrowZone.getCentreY() - 2);
        g.setColour(juce::Colours::white.withAlpha(0.6f));
        g.strokePath(arrow, juce::PathStrokeType(1.5f));
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FrostLookAndFeel)
};
