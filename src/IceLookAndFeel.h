#pragma once

#include "DawLookAndFeel.h"

// "Ice" theme — dark charcoal/navy with ice blue controls.
// Inspired by a dark UI kit with soft blue interactive elements.
class IceLookAndFeel : public DawLookAndFeel
{
public:
    IceLookAndFeel()
    {
        // ── Surfaces — dark charcoal with blue tint ──
        theme.body        = 0xff0d1117;
        theme.bodyLight   = 0xff151c24;
        theme.bodyDark    = 0xff080c10;
        theme.border      = 0xff1e2833;
        theme.borderLight = 0xff2a3844;

        // ── Text ──
        theme.textPrimary   = 0xffbbc8d4;
        theme.textSecondary = 0xff5c7080;
        theme.textBright    = 0xffe4edf4;

        // ── Accent — electric white-blue ──
        theme.red       = 0xffdd5555;
        theme.redDark   = 0xff882222;
        theme.amber     = 0xffc8e4ff;  // electric white-blue
        theme.amberDark = 0xff88aad0;
        theme.green     = 0xff55cc88;
        theme.greenDark = 0xff2a8855;

        // ── LCD ──
        theme.lcdBg    = 0xff080c10;
        theme.lcdText  = 0xffc8e4ff;
        theme.lcdAmber = 0xffc8e4ff;

        // ── Buttons — slightly elevated cards ──
        theme.buttonFace  = 0xff141c24;
        theme.buttonHover = 0xff1c2630;
        theme.buttonDown  = 0xff0c1218;

        theme.btnStop       = 0xff141c24;
        theme.btnMetronome  = 0xff141c24;
        theme.btnMetronomeOn = 0xff142830;
        theme.btnCountIn    = 0xff141c24;
        theme.btnCountInOn  = 0xff142830;
        theme.btnNewClip    = 0xff142420;
        theme.btnDeleteClip = 0xff241418;
        theme.btnDuplicate  = 0xff141c28;
        theme.btnSplit      = 0xff1c2014;
        theme.btnQuantize   = 0xff161c24;
        theme.btnEditNotes  = 0xff141c28;
        theme.btnNav        = 0xff141c24;
        theme.btnSave       = 0xff142420;
        theme.btnLoad       = 0xff161c28;
        theme.btnUndoRedo   = 0xff161c24;
        theme.btnMidi2      = 0xff141c24;
        theme.btnMidi2On    = 0xff142838;
        theme.btnLoop       = 0xff141c24;
        theme.btnLoopOn     = 0xff142838;
        theme.loopRegion    = 0x22c8e4ff;
        theme.loopBorder    = 0xff88aadd;

        // ── Timeline ──
        theme.timelineBg         = 0xff0a0e14;
        theme.timelineAltRow     = 0xff0e141a;
        theme.timelineSelectedRow = 0xff142430;
        theme.timelineGridMajor  = 0xff8899aa;
        theme.timelineGridMinor  = 0xff3a4a55;
        theme.timelineGridFaint  = 0xff141c24;
        theme.timelineGridBeat   = 0xff1e2833;

        // ── Clips ──
        theme.clipDefault   = 0xff1a2838;
        theme.clipRecording = 0xff442222;
        theme.clipQueued    = 0xff2a3838;
        theme.clipPlaying   = 0xff1a3828;

        // ── Playhead ──
        theme.playhead     = 0xddc8e4ff;
        theme.playheadGlow = 0x33c8e4ff;

        theme.accentStripe = 0xffc8e4ff;

        theme.trackSelected = 0xff142430;
        theme.trackArmed    = 0xff442222;
        theme.trackMuteOn   = 0xffdd5555;
        theme.trackSoloOn   = 0xffc8e4ff;
        theme.trackSoloText = 0xff080c10;

        applyThemeColors();
    }

    float getButtonRadius() const override { return 8.0f; }

    // Ice: rounded pill-style buttons
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        auto baseColour = backgroundColour;

        if (shouldDrawButtonAsDown)
            baseColour = baseColour.darker(0.2f);
        else if (shouldDrawButtonAsHighlighted)
            baseColour = baseColour.brighter(0.08f);

        g.setColour(baseColour);
        g.fillRoundedRectangle(bounds, 8.0f);

        g.setColour(juce::Colour(theme.border));
        g.drawRoundedRectangle(bounds, 8.0f, 1.0f);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IceLookAndFeel)
};
