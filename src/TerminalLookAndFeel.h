#pragma once

#include "DawLookAndFeel.h"

// "Terminal" theme — inspired by Jinsu Park's portfolio.
// Dense dark developer aesthetic. Black base, monospace everything,
// tight spacing. Single electric accent: #FDFF6D (yellow-green).
// Subtle card insets, press-in button effects, data-dashboard density.
class TerminalLookAndFeel : public DawLookAndFeel
{
public:
    TerminalLookAndFeel()
    {
        // ── Surfaces — pure black, very subtle variation ──
        theme.body        = 0xff000000;
        theme.bodyLight   = 0xff0e0e10;
        theme.bodyDark    = 0xff050508;
        theme.border      = 0xff1a1a1e;
        theme.borderLight = 0xff2a2a30;

        // ── Text — cool white, secondary is neutral-500 ──
        theme.textPrimary   = 0xffcccccc;
        theme.textSecondary = 0xff6b6b70;
        theme.textBright    = 0xffeeeef0;

        // ── Accent — electric purple ──
        theme.red       = 0xffff6655;  // warm red
        theme.redDark   = 0xff882222;
        theme.amber     = 0xffbb88ff;  // purple highlight
        theme.amberDark = 0xff6644aa;
        theme.green     = 0xff6dff8d;  // bright green
        theme.greenDark = 0xff228833;

        // ── LCD — black with purple text ──
        theme.lcdBg    = 0xff050508;
        theme.lcdText  = 0xffbb88ff;
        theme.lcdAmber = 0xffbb88ff;

        // ── Buttons — inset card style ──
        theme.buttonFace  = 0xff111114;
        theme.buttonHover = 0xff1a1a1e;
        theme.buttonDown  = 0xff080808;

        theme.btnStop       = 0xff131316;
        theme.btnMetronome  = 0xff131316;
        theme.btnMetronomeOn = 0xff1a1430;
        theme.btnCountIn    = 0xff131316;
        theme.btnCountInOn  = 0xff1a1430;
        theme.btnNewClip    = 0xff0e1a0e;
        theme.btnDeleteClip = 0xff1a0e0e;
        theme.btnDuplicate  = 0xff0e1316;
        theme.btnSplit      = 0xff16160e;
        theme.btnQuantize   = 0xff141414;
        theme.btnEditNotes  = 0xff0e1316;
        theme.btnNav        = 0xff131316;
        theme.btnSave       = 0xff0e1a0e;
        theme.btnLoad       = 0xff12121a;
        theme.btnUndoRedo   = 0xff141414;
        theme.btnMidi2      = 0xff131316;
        theme.btnMidi2On    = 0xff1a1430;
        theme.btnLoop       = 0xff131316;
        theme.btnLoopOn     = 0xff1a1430;
        theme.loopRegion    = 0x22bb88ff;
        theme.loopBorder    = 0xff8866cc;

        // ── Timeline ──
        theme.timelineBg         = 0xff000000;
        theme.timelineAltRow     = 0xff0a0a0c;
        theme.timelineSelectedRow = 0xff120e1e;
        theme.timelineGridMajor  = 0xff888888;
        theme.timelineGridMinor  = 0xff444444;
        theme.timelineGridFaint  = 0xff141414;
        theme.timelineGridBeat   = 0xff222222;

        // ── Clips — muted with yellow-green for playing ──
        theme.clipDefault   = 0xff1a1a22;
        theme.clipRecording = 0xff553322;
        theme.clipQueued    = 0xff2a2040;
        theme.clipPlaying   = 0xff1e2a40;

        // ── Playhead — purple ──
        theme.playhead     = 0xdd44dd66;
        theme.playheadGlow = 0x3344dd66;

        theme.accentStripe = 0xffbb88ff;

        theme.trackSelected = 0xff120e1e;
        theme.trackArmed    = 0xff442211;
        theme.trackMuteOn   = 0xffff6655;
        theme.trackSoloOn   = 0xffbb88ff;
        theme.trackSoloText = 0xff000000;

        applyThemeColors();
    }

    float getButtonRadius() const override { return 3.0f; }

    // Terminal: inset card buttons with press-in effect
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        auto baseColour = backgroundColour;

        if (shouldDrawButtonAsDown)
            baseColour = baseColour.darker(0.3f);
        else if (shouldDrawButtonAsHighlighted)
            baseColour = baseColour.brighter(0.06f);

        // Card fill
        g.setColour(baseColour);
        g.fillRoundedRectangle(bounds, 3.0f);

        // Inset shadow (inner border at low opacity)
        g.setColour(juce::Colours::white.withAlpha(0.04f));
        g.drawRoundedRectangle(bounds.reduced(1.0f), 3.0f, 1.0f);

        // Outer border
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.drawRoundedRectangle(bounds, 3.0f, 1.0f);

        // Left accent on toggle-on
        if (button.getToggleState())
        {
            g.setColour(juce::Colour(theme.amber));
            g.fillRect(bounds.getX(), bounds.getY() + 2, 1.5f, bounds.getHeight() - 4);
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TerminalLookAndFeel)
};
