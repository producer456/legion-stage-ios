#pragma once

#include "DawLookAndFeel.h"

// "Linen" theme — inspired by FARMform.
// LIGHT theme. Warm cream background, white content panels,
// near-black text. Serif-like display font, uppercase tracked
// sans-serif labels. Warm gray borders, editorial luxury.
class LinenLookAndFeel : public DawLookAndFeel
{
public:
    LinenLookAndFeel()
    {
        // ── Surfaces — warm cream and white ──
        theme.body        = 0xffede8e0;  // warm cream
        theme.bodyLight   = 0xffffffff;  // white panels
        theme.bodyDark    = 0xffe2ddd4;  // slightly darker cream
        theme.border      = 0xffd5d0c8;  // warm gray border
        theme.borderLight = 0xffe0dbd3;  // lighter border

        // ── Text — near-black, warm grays ──
        theme.textPrimary   = 0xff1a1a18;
        theme.textSecondary = 0xff7a756f;
        theme.textBright    = 0xff000000;

        // ── Accent — muted earth tones ──
        theme.red       = 0xffbb4444;
        theme.redDark   = 0xff884433;
        theme.amber     = 0xff8a7550;  // muted gold/brown
        theme.amberDark = 0xff6b5a3d;
        theme.green     = 0xff5a7a55;
        theme.greenDark = 0xff4a6645;

        // ── LCD — cream with dark text ──
        theme.lcdBg    = 0xfff5f0e8;
        theme.lcdText  = 0xff2a2820;
        theme.lcdAmber = 0xff6b5a3d;

        // ── Buttons — white with subtle border ──
        theme.buttonFace  = 0xffffffff;
        theme.buttonHover = 0xfff5f0e8;
        theme.buttonDown  = 0xffe8e3da;

        theme.btnStop       = 0xfff0ebe3;
        theme.btnMetronome  = 0xfff0ebe3;
        theme.btnMetronomeOn = 0xffddd0b8;
        theme.btnCountIn    = 0xfff0ebe3;
        theme.btnCountInOn  = 0xffe0d5c0;
        theme.btnNewClip    = 0xffe0eee0;
        theme.btnDeleteClip = 0xffeee0e0;
        theme.btnDuplicate  = 0xffe0e8ee;
        theme.btnSplit      = 0xffeeeae0;
        theme.btnQuantize   = 0xfff0ebe3;
        theme.btnEditNotes  = 0xffe0e8ee;
        theme.btnNav        = 0xfff0ebe3;
        theme.btnSave       = 0xffe0eee0;
        theme.btnLoad       = 0xffe5e5ee;
        theme.btnUndoRedo   = 0xfff0ebe3;
        theme.btnMidi2      = 0xfff0ebe3;
        theme.btnMidi2On    = 0xffd8e0ee;
        theme.btnLoop       = 0xfff0ebe3;
        theme.btnLoopOn     = 0xffd8e0ee;
        theme.loopRegion    = 0x224488cc;
        theme.loopBorder    = 0xff7799bb;

        // ── Timeline — light ──
        theme.timelineBg         = 0xffffffff;
        theme.timelineAltRow     = 0xfff8f5f0;
        theme.timelineSelectedRow = 0xffe8e0d5;
        theme.timelineGridMajor  = 0xff555550;
        theme.timelineGridMinor  = 0xffaaa8a0;
        theme.timelineGridFaint  = 0xffeeeae4;
        theme.timelineGridBeat   = 0xffd0ccc5;

        // ── Clips — muted pastels ──
        theme.clipDefault   = 0xffc8c0b5;
        theme.clipRecording = 0xffccaa99;
        theme.clipQueued    = 0xffd4c4a8;
        theme.clipPlaying   = 0xffa8c4a0;

        // ── Playhead — dark brown ──
        theme.playhead     = 0xdd339955;
        theme.playheadGlow = 0x33339955;

        theme.accentStripe = 0xff8a7550;

        theme.trackSelected = 0xffe8e0d5;
        theme.trackArmed    = 0xffddccbb;
        theme.trackMuteOn   = 0xffbb4444;
        theme.trackSoloOn   = 0xff8a7550;
        theme.trackSoloText = 0xffffffff;

        applyThemeColors();

        // Override some JUCE colours for light mode
        setColour(juce::TextButton::textColourOnId, juce::Colour(0xffffffff));
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colour(0xffffffff));
    }

    // Linen uses Georgia for a serif editorial feel
    juce::String getDisplayFontName() const override { return "Georgia"; }

    float getButtonRadius() const override { return 4.0f; }

    // Linen: soft outlined buttons
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        auto baseColour = backgroundColour;

        if (shouldDrawButtonAsDown)
            baseColour = baseColour.darker(0.06f);
        else if (shouldDrawButtonAsHighlighted)
            baseColour = baseColour.darker(0.03f);

        g.setColour(baseColour);
        g.fillRoundedRectangle(bounds, 4.0f);

        g.setColour(juce::Colour(theme.border));
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
    }

    // Linen: editorial mixed case
    juce::String formatButtonText(const juce::String& text) const override
    {
        return text;
    }

    // Linen: warm combo box
    void drawComboBox(juce::Graphics& g, int width, int height, bool,
                      int, int, int, int, juce::ComboBox&) override
    {
        auto bounds = juce::Rectangle<float>(0, 0, static_cast<float>(width), static_cast<float>(height));
        g.setColour(juce::Colour(theme.bodyLight));
        g.fillRoundedRectangle(bounds, 4.0f);
        g.setColour(juce::Colour(theme.border));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.0f);

        auto arrowZone = bounds.removeFromRight(20.0f).reduced(5.0f);
        juce::Path arrow;
        arrow.addTriangle(arrowZone.getX(), arrowZone.getY(),
                          arrowZone.getRight(), arrowZone.getY(),
                          arrowZone.getCentreX(), arrowZone.getBottom());
        g.setColour(juce::Colour(theme.textSecondary));
        g.fillPath(arrow);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LinenLookAndFeel)
};
