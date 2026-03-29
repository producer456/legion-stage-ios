#pragma once

#include "DawLookAndFeel.h"

// "Journal" theme — inspired by Hemstacks / Antikythera.
// Dark editorial brutalism. Black background, serif headings +
// monospace labels. Per-track accent colors (mauve, coral, amber,
// violet, olive). Glow hovers, backdrop feel. Intellectual vibe.
class JournalLookAndFeel : public DawLookAndFeel
{
public:
    // Per-track accent palette — 8 colors cycling across tracks
    static constexpr uint32_t trackAccents[] = {
        0xffd9add9,  // soft mauve
        0xfff46a6a,  // coral red
        0xfff4dd47,  // bright yellow
        0xffaf98f4,  // soft violet
        0xffa6ad33,  // olive green
        0xfffa8552,  // tangerine
        0xffe68080,  // dusty rose
        0xffd198dd   // lavender
    };

    static constexpr int numTrackAccents = 8;

    JournalLookAndFeel()
    {
        // ── Surfaces — pure black, austere ──
        theme.body        = 0xff000000;
        theme.bodyLight   = 0xff0c0c0c;
        theme.bodyDark    = 0xff000000;
        theme.border      = 0xff262626;
        theme.borderLight = 0xff3a3a3a;

        // ── Text — warm white, gray secondary ──
        theme.textPrimary   = 0xffd0d0d0;
        theme.textSecondary = 0xff707070;
        theme.textBright    = 0xffffffff;

        // ── Accent — warm amber primary, coral for alert ──
        theme.red       = 0xfff46a6a;
        theme.redDark   = 0xff993333;
        theme.amber     = 0xfffdbc00;  // amber from hemstacks
        theme.amberDark = 0xff886600;
        theme.green     = 0xffa6ad33;
        theme.greenDark = 0xff667722;

        // ── LCD — dark with amber text ──
        theme.lcdBg    = 0xff0a0a08;
        theme.lcdText  = 0xfffdbc00;
        theme.lcdAmber = 0xfffdbc00;

        // ── Buttons — minimal, dark ──
        theme.buttonFace  = 0xff141414;
        theme.buttonHover = 0xff1e1e1e;
        theme.buttonDown  = 0xff0a0a0a;

        theme.btnStop       = 0xff161616;
        theme.btnMetronome  = 0xff161616;
        theme.btnMetronomeOn = 0xff2a2010;
        theme.btnCountIn    = 0xff161616;
        theme.btnCountInOn  = 0xff221a10;
        theme.btnNewClip    = 0xff142014;
        theme.btnDeleteClip = 0xff201414;
        theme.btnDuplicate  = 0xff141a20;
        theme.btnSplit      = 0xff1a1a10;
        theme.btnQuantize   = 0xff181818;
        theme.btnEditNotes  = 0xff141a20;
        theme.btnNav        = 0xff161616;
        theme.btnSave       = 0xff142014;
        theme.btnLoad       = 0xff161620;
        theme.btnUndoRedo   = 0xff181818;
        theme.btnMidi2      = 0xff161616;
        theme.btnMidi2On    = 0xff1a2030;
        theme.btnLoop       = 0xff161616;
        theme.btnLoopOn     = 0xff1a2030;
        theme.loopRegion    = 0x22fdbc00;
        theme.loopBorder    = 0xffcc9922;

        // ── Timeline ──
        theme.timelineBg         = 0xff000000;
        theme.timelineAltRow     = 0xff080808;
        theme.timelineSelectedRow = 0xff1a1510;
        theme.timelineGridMajor  = 0xff808080;
        theme.timelineGridMinor  = 0xff404040;
        theme.timelineGridFaint  = 0xff161616;
        theme.timelineGridBeat   = 0xff2a2a2a;

        // ── Clips — warm tones ──
        theme.clipDefault   = 0xff2a2520;
        theme.clipRecording = 0xff553322;
        theme.clipQueued    = 0xff554422;
        theme.clipPlaying   = 0xff334422;

        // ── Playhead — amber ──
        theme.playhead     = 0xdd44dd66;
        theme.playheadGlow = 0x3344dd66;

        theme.accentStripe = 0xfffdbc00;

        theme.trackSelected = 0xff1a1510;
        theme.trackArmed    = 0xff442211;
        theme.trackMuteOn   = 0xfff46a6a;
        theme.trackSoloOn   = 0xfff4dd47;
        theme.trackSoloText = 0xff000000;

        applyThemeColors();
    }

    // Journal uses a serif for display headings
    juce::String getDisplayFontName() const override { return "Georgia"; }

    float getButtonRadius() const override { return 4.0f; }

    // Journal: buttons with subtle glow on hover
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
            baseColour = baseColour.brighter(0.12f);

        g.setColour(baseColour);
        g.fillRoundedRectangle(bounds, 4.0f);

        // Glow border on hover
        if (shouldDrawButtonAsHighlighted && !shouldDrawButtonAsDown)
        {
            g.setColour(juce::Colours::white.withAlpha(0.06f));
            g.drawRoundedRectangle(bounds.expanded(1.0f), 5.0f, 2.0f);
        }

        g.setColour(juce::Colour(theme.border));
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
    }

    // Journal: mixed case, more editorial
    juce::String formatButtonText(const juce::String& text) const override
    {
        return text;  // keep original case
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JournalLookAndFeel)
};
