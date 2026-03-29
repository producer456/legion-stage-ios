#pragma once

#include "DawLookAndFeel.h"

// "Noir" theme — inspired by Moxey.no.
// Pure black canvas, white text with opacity hierarchy.
// Zero color accents — everything is black/white.
// Bracket-style labels, ultra-minimal, cinematic.
class NoirLookAndFeel : public DawLookAndFeel
{
public:
    NoirLookAndFeel()
    {
        // ── Surfaces — pure black ──
        theme.body        = 0xff000000;
        theme.bodyLight   = 0xff0c0c0c;
        theme.bodyDark    = 0xff000000;
        theme.border      = 0xff222222;
        theme.borderLight = 0xff333333;

        // ── Text — white at different opacities ──
        theme.textPrimary   = 0xffcccccc;
        theme.textSecondary = 0xff666666;
        theme.textBright    = 0xffffffff;

        // ── "Accents" — all monochrome ──
        theme.red       = 0xffdddddd;  // bright white for REC
        theme.redDark   = 0xff555555;
        theme.amber     = 0xffffffff;  // pure white for active
        theme.amberDark = 0xff444444;
        theme.green     = 0xffaaaaaa;  // mid-gray for play
        theme.greenDark = 0xff444444;

        // ── LCD — dark with dim white text ──
        theme.lcdBg    = 0xff080808;
        theme.lcdText  = 0xff999999;
        theme.lcdAmber = 0xffbbbbbb;

        // ── Buttons — near-black ──
        theme.buttonFace  = 0xff151515;
        theme.buttonHover = 0xff222222;
        theme.buttonDown  = 0xff0a0a0a;

        // Noir: all buttons are the same monochrome
        theme.btnStop       = 0xff181818;
        theme.btnMetronome  = 0xff181818;
        theme.btnMetronomeOn = 0xff333333;
        theme.btnCountIn    = 0xff181818;
        theme.btnCountInOn  = 0xff2a2a2a;
        theme.btnNewClip    = 0xff1a1a1a;
        theme.btnDeleteClip = 0xff1a1a1a;
        theme.btnDuplicate  = 0xff1a1a1a;
        theme.btnSplit      = 0xff1a1a1a;
        theme.btnQuantize   = 0xff1a1a1a;
        theme.btnEditNotes  = 0xff1a1a1a;
        theme.btnNav        = 0xff181818;
        theme.btnSave       = 0xff1a1a1a;
        theme.btnLoad       = 0xff1a1a1a;
        theme.btnUndoRedo   = 0xff1a1a1a;
        theme.btnMidi2      = 0xff181818;
        theme.btnMidi2On    = 0xff2a2a2a;
        theme.btnLoop       = 0xff181818;
        theme.btnLoopOn     = 0xff2a2a2a;
        theme.loopRegion    = 0x18ffffff;
        theme.loopBorder    = 0xff555555;

        // ── Timeline ──
        theme.timelineBg         = 0xff000000;
        theme.timelineAltRow     = 0xff080808;
        theme.timelineSelectedRow = 0xff1a1a1a;
        theme.timelineGridMajor  = 0xff888888;
        theme.timelineGridMinor  = 0xff444444;
        theme.timelineGridFaint  = 0xff141414;
        theme.timelineGridBeat   = 0xff282828;

        // ── Clips — grayscale ──
        theme.clipDefault   = 0xff333333;
        theme.clipRecording = 0xff666666;
        theme.clipQueued    = 0xff555555;
        theme.clipPlaying   = 0xff888888;

        // ── Playhead — white ──
        theme.playhead     = 0xdd44dd66;
        theme.playheadGlow = 0x3344dd66;

        theme.accentStripe = 0xffffffff;  // thin white line

        theme.trackSelected = 0xff1a1a1a;
        theme.trackArmed    = 0xff333333;
        theme.trackMuteOn   = 0xffaaaaaa;
        theme.trackSoloOn   = 0xffffffff;
        theme.trackSoloText = 0xff000000;

        applyThemeColors();
    }

    // Noir: bracket-style button text [ PLAY ]
    juce::String formatButtonText(const juce::String& text) const override
    {
        return "[ " + text.toUpperCase() + " ]";
    }

    // Noir: ultra-flat buttons, no bevel
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat();
        auto baseColour = backgroundColour;

        if (shouldDrawButtonAsDown)
            baseColour = baseColour.brighter(0.15f);
        else if (shouldDrawButtonAsHighlighted)
            baseColour = baseColour.brighter(0.06f);

        g.setColour(baseColour);
        g.fillRect(bounds);  // sharp corners, no radius

        // Bottom edge only
        g.setColour(juce::Colour(theme.border));
        g.drawLine(bounds.getX(), bounds.getBottom(),
                   bounds.getRight(), bounds.getBottom(), 1.0f);
    }

    float getButtonRadius() const override { return 0.0f; }

    // Noir: minimal combo box — just a line
    void drawComboBox(juce::Graphics& g, int width, int height, bool,
                      int, int, int, int, juce::ComboBox&) override
    {
        auto bounds = juce::Rectangle<float>(0, 0, static_cast<float>(width), static_cast<float>(height));
        g.setColour(juce::Colour(theme.bodyDark));
        g.fillRect(bounds);
        g.setColour(juce::Colour(theme.border));
        g.drawLine(0, static_cast<float>(height) - 1, static_cast<float>(width), static_cast<float>(height) - 1, 1.0f);

        // Arrow — simple downward chevron
        auto arrowX = static_cast<float>(width) - 16.0f;
        auto arrowY = static_cast<float>(height) * 0.5f;
        juce::Path arrow;
        arrow.startNewSubPath(arrowX - 4, arrowY - 2);
        arrow.lineTo(arrowX, arrowY + 2);
        arrow.lineTo(arrowX + 4, arrowY - 2);
        g.setColour(juce::Colour(theme.textSecondary));
        g.strokePath(arrow, juce::PathStrokeType(1.5f));
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NoirLookAndFeel)
};
