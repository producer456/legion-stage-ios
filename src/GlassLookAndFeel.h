#pragma once

#include "DawLookAndFeel.h"

// "Glass" theme — inspired by Ruixen UI.
// Monochromatic dark with frosted-glass panels, subtle gray borders,
// coral/pink accent on active elements. Clean geometric feel.
class GlassLookAndFeel : public DawLookAndFeel
{
public:
    GlassLookAndFeel()
    {
        // ── Surfaces — near-black with slight warmth ──
        theme.body        = 0xff111114;
        theme.bodyLight   = 0xff1c1c20;
        theme.bodyDark    = 0xff0a0a0d;
        theme.border      = 0xff2a2a30;
        theme.borderLight = 0xff3a3a42;

        // ── Text — white hierarchy through opacity ──
        theme.textPrimary   = 0xffe0e0e4;
        theme.textSecondary = 0xff737380;
        theme.textBright    = 0xfffafafc;

        // ── Accent — coral/rose for active, cool blue for subtle ──
        theme.red       = 0xffdd5566;
        theme.redDark   = 0xff993344;
        theme.amber     = 0xffee7766;  // coral instead of amber
        theme.amberDark = 0xffaa4433;
        theme.green     = 0xff55bb88;
        theme.greenDark = 0xff337755;

        // ── LCD — frosted dark with blue-white text ──
        theme.lcdBg    = 0xff0d0d14;
        theme.lcdText  = 0xffaabbcc;
        theme.lcdAmber = 0xffee8877;

        // ── Buttons — glass-like with subtle borders ──
        theme.buttonFace  = 0xff1e1e24;
        theme.buttonHover = 0xff282830;
        theme.buttonDown  = 0xff141418;

        theme.btnStop       = 0xff222230;
        theme.btnMetronome  = 0xff222230;
        theme.btnMetronomeOn = 0xff553344;
        theme.btnCountIn    = 0xff222230;
        theme.btnCountInOn  = 0xff443355;
        theme.btnNewClip    = 0xff1e3533;
        theme.btnDeleteClip = 0xff351e22;
        theme.btnDuplicate  = 0xff1e2e35;
        theme.btnSplit      = 0xff2e301e;
        theme.btnQuantize   = 0xff2a2a30;
        theme.btnEditNotes  = 0xff1e2835;
        theme.btnNav        = 0xff222230;
        theme.btnSave       = 0xff1e3530;
        theme.btnLoad       = 0xff252535;
        theme.btnUndoRedo   = 0xff2a2a30;
        theme.btnMidi2      = 0xff222230;
        theme.btnMidi2On    = 0xff2a3555;
        theme.btnLoop       = 0xff222230;
        theme.btnLoopOn     = 0xff1e2e45;
        theme.loopRegion    = 0x2244aaff;
        theme.loopBorder    = 0xff4488cc;

        // ── Timeline ──
        theme.timelineBg         = 0xff0e0e12;
        theme.timelineAltRow     = 0xff141418;
        theme.timelineSelectedRow = 0xff1a2535;
        theme.timelineGridMajor  = 0xffbbbbcc;
        theme.timelineGridMinor  = 0xff555566;
        theme.timelineGridFaint  = 0xff1e1e24;
        theme.timelineGridBeat   = 0xff333340;

        // ── Clips — muted pastels ──
        theme.clipDefault   = 0xff3a4455;
        theme.clipRecording = 0xff773344;
        theme.clipQueued    = 0xff775533;
        theme.clipPlaying   = 0xff337755;

        // ── Playhead — coral glow ──
        theme.playhead     = 0xdd44dd66;
        theme.playheadGlow = 0x3344dd66;

        theme.accentStripe = 0xffee7766;  // coral stripe

        theme.trackSelected = 0xff2a3a55;
        theme.trackArmed    = 0xff773344;
        theme.trackMuteOn   = 0xffdd5566;
        theme.trackSoloOn   = 0xffee7766;
        theme.trackSoloText = 0xff0a0a0d;

        applyThemeColors();
    }

    float getButtonRadius() const override { return 6.0f; }

    // Glass theme: frosted button with softer bevel
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
            baseColour = baseColour.brighter(0.08f);

        // Frosted fill
        g.setColour(baseColour.withMultipliedAlpha(0.85f));
        g.fillRoundedRectangle(bounds, 6.0f);

        // Inner glow at top
        if (!shouldDrawButtonAsDown)
        {
            g.setColour(juce::Colours::white.withAlpha(0.04f));
            g.fillRoundedRectangle(bounds.removeFromTop(bounds.getHeight() * 0.4f), 6.0f);
        }

        // Thin border
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.drawRoundedRectangle(button.getLocalBounds().toFloat().reduced(0.5f), 6.0f, 1.0f);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlassLookAndFeel)
};
