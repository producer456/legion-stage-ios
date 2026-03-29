#pragma once

#include "DawLookAndFeel.h"

// "Indigo" theme — deep indigo/purple background with violet/purple accents.
// Inspired by a futuristic dashboard with circular gauges and purple charts.
class IndigoLookAndFeel : public DawLookAndFeel
{
public:
    IndigoLookAndFeel()
    {
        // ── Surfaces — deep indigo ──
        theme.body        = 0xff0a0a2e;
        theme.bodyLight   = 0xff12123a;
        theme.bodyDark    = 0xff060620;
        theme.border      = 0xff2a2a5a;
        theme.borderLight = 0xff3a3a6a;

        // ── Text ──
        theme.textPrimary   = 0xffccccee;
        theme.textSecondary = 0xff7777aa;
        theme.textBright    = 0xffeeeeff;

        // ── Accent — violet/purple ──
        theme.red       = 0xffee5566;
        theme.redDark   = 0xff882244;
        theme.amber     = 0xff8866dd;  // violet
        theme.amberDark = 0xff5544aa;
        theme.green     = 0xff55cc88;
        theme.greenDark = 0xff2a8855;

        // ── LCD ──
        theme.lcdBg    = 0xff060620;
        theme.lcdText  = 0xff9988ee;
        theme.lcdAmber = 0xff8866dd;

        // ── Buttons ──
        theme.buttonFace  = 0xff101038;
        theme.buttonHover = 0xff181848;
        theme.buttonDown  = 0xff0a0a28;

        theme.btnStop       = 0xff101038;
        theme.btnMetronome  = 0xff101038;
        theme.btnMetronomeOn = 0xff1a1850;
        theme.btnCountIn    = 0xff101038;
        theme.btnCountInOn  = 0xff1a1850;
        theme.btnNewClip    = 0xff102038;
        theme.btnDeleteClip = 0xff281020;
        theme.btnDuplicate  = 0xff101838;
        theme.btnSplit      = 0xff181810;
        theme.btnQuantize   = 0xff121230;
        theme.btnEditNotes  = 0xff101838;
        theme.btnNav        = 0xff101038;
        theme.btnSave       = 0xff102028;
        theme.btnLoad       = 0xff121238;
        theme.btnUndoRedo   = 0xff121230;
        theme.btnMidi2      = 0xff101038;
        theme.btnMidi2On    = 0xff1a1a58;
        theme.btnLoop       = 0xff101038;
        theme.btnLoopOn     = 0xff1a1a58;
        theme.loopRegion    = 0x228866dd;
        theme.loopBorder    = 0xff7755cc;

        // ── Timeline ──
        theme.timelineBg         = 0xff080824;
        theme.timelineAltRow     = 0xff0c0c2c;
        theme.timelineSelectedRow = 0xff181848;
        theme.timelineGridMajor  = 0xff8888bb;
        theme.timelineGridMinor  = 0xff3a3a66;
        theme.timelineGridFaint  = 0xff141438;
        theme.timelineGridBeat   = 0xff2a2a55;

        // ── Clips ──
        theme.clipDefault   = 0xff22224a;
        theme.clipRecording = 0xff552244;
        theme.clipQueued    = 0xff2a2255;
        theme.clipPlaying   = 0xff224a33;

        // ── Playhead ──
        theme.playhead     = 0xdd44dd66;
        theme.playheadGlow = 0x3344dd66;

        theme.accentStripe = 0xff8866dd;

        theme.trackSelected = 0xff181848;
        theme.trackArmed    = 0xff442244;
        theme.trackMuteOn   = 0xffee5566;
        theme.trackSoloOn   = 0xff8866dd;
        theme.trackSoloText = 0xff060620;

        applyThemeColors();
    }

    float getButtonRadius() const override { return 4.0f; }

    // Indigo: buttons with subtle purple glow on hover
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
            baseColour = baseColour.brighter(0.1f);

        g.setColour(baseColour);
        g.fillRoundedRectangle(bounds, 4.0f);

        // Purple glow on hover
        if (shouldDrawButtonAsHighlighted && !shouldDrawButtonAsDown)
        {
            g.setColour(juce::Colour(0x108866dd));
            g.drawRoundedRectangle(bounds.expanded(1.0f), 5.0f, 2.0f);
        }

        g.setColour(juce::Colour(theme.border));
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IndigoLookAndFeel)
};
