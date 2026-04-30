#pragma once

#include "LaunchkeyDarkLookAndFeel.h"

// "Launchkey OLED" — the dark theme rendered as if it were on a tiny
// monochrome blue OLED panel (SSD1306-style, ~470nm cyan emission):
//   - one single emission color, no brightness gradient between tiers
//   - hard-pixel rendering: square corners, no anti-aliased curves
//   - 1-bit lit/unlit decisions for state (no smooth highlight tiers)
//   - faint horizontal scan-line texture overlaid on the canvas
class LaunchkeyOledLookAndFeel : public LaunchkeyDarkLookAndFeel
{
public:
    // Common emission colours of cheap monochrome OLED panels — cycled
    // by the user via a button in the toolbar so they can try the look
    // of each variant (blue, white, yellow, green, deep blue).
    static constexpr juce::uint32 kOledPalette[] = {
        0xff5cbcfc,   // sky-cyan-blue (SSD1306 "blue")
        0xffffffff,   // white
        0xffffd247,   // yellow / amber
        0xff58ff58,   // emerald green
        0xff4080ff,   // deeper blue
    };
    static constexpr int kNumOledColors = (int) (sizeof(kOledPalette) / sizeof(kOledPalette[0]));

    LaunchkeyOledLookAndFeel()
    {
        rebuildTheme();
    }

    juce::uint32 getOledColour() const { return kOledPalette[colorIdx]; }
    juce::String getOledColourName() const
    {
        switch (colorIdx) { case 0: return "BLUE"; case 1: return "WHITE";
                            case 2: return "AMBER"; case 3: return "GREEN";
                            case 4: return "DEEP"; default: return "OLED"; }
    }
    void cycleOledColour()
    {
        colorIdx = (colorIdx + 1) % kNumOledColors;
        rebuildTheme();
    }

private:
    int colorIdx = 0;

    void rebuildTheme()
    {
        const juce::uint32 oled = kOledPalette[colorIdx];

        // Pure black for the canvas (off pixels).
        theme.body        = 0xff000000;
        theme.bodyLight   = 0xff000000;
        theme.bodyDark    = 0xff000000;

        // Border/outline colours all collapse to the single OLED hue.
        theme.border      = oled;
        theme.borderLight = oled;
        theme.wireframe   = true;

        theme.textPrimary   = oled;
        theme.textSecondary = oled;
        theme.textBright    = oled;

        theme.red       = oled;
        theme.redDark   = 0xff000000;
        theme.amber     = oled;
        theme.amberDark = 0xff000000;
        theme.green     = oled;
        theme.greenDark = 0xff000000;

        theme.lcdBg    = 0xff000000;
        theme.lcdText  = oled;
        theme.lcdAmber = oled;

        theme.buttonFace  = 0xff000000;
        theme.buttonHover = 0xff000000;
        theme.buttonDown  = 0xff000000;

        theme.btnStop = theme.btnMetronome = theme.btnMetronomeOn
                       = theme.btnCountIn = theme.btnCountInOn
                       = theme.btnNewClip = theme.btnDeleteClip
                       = theme.btnDuplicate = theme.btnSplit
                       = theme.btnQuantize = theme.btnEditNotes
                       = theme.btnNav = theme.btnSave = theme.btnLoad
                       = theme.btnUndoRedo = theme.btnMidi2 = theme.btnMidi2On
                       = theme.btnLoop = theme.btnLoopOn
                       = 0xff000000;

        theme.loopRegion = 0x205cbcfc;
        theme.loopBorder = oled;

        theme.timelineBg          = 0xff000000;
        theme.timelineAltRow      = 0xff000000;
        theme.timelineSelectedRow = 0xff0a1822;
        theme.timelineGridMajor   = oled;
        theme.timelineGridMinor   = 0xff183244;
        theme.timelineGridFaint   = 0xff0a1822;
        theme.timelineGridBeat    = 0xff0a1822;

        theme.clipDefault     = 0xff183244;
        theme.clipRecording   = oled;
        theme.clipQueued      = 0xff2a5a78;
        theme.clipPlaying     = oled;
        theme.clipNotePreview = 0xcc5cbcfc;

        theme.playhead     = oled;
        theme.playheadGlow = 0;

        theme.accentStripe  = oled;
        theme.trackSelected = 0xff0a1822;
        theme.trackArmed    = oled;
        theme.trackMuteOn   = oled;
        theme.trackSoloOn   = oled;
        theme.trackSoloText = 0xff000000;

        applyThemeColors();

        setColour(juce::ComboBox::textColourId,                   juce::Colour(oled));
        setColour(juce::PopupMenu::backgroundColourId,            juce::Colour(0xff000000));
        setColour(juce::PopupMenu::textColourId,                  juce::Colour(oled));
        setColour(juce::PopupMenu::highlightedTextColourId,       juce::Colour(0xff000000));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(oled));

        setColour(juce::Slider::textBoxTextColourId,       juce::Colour(oled));
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff000000));
        setColour(juce::Slider::textBoxOutlineColourId,    juce::Colour(0x00000000));

        setColour(juce::TextButton::buttonOnColourId,      juce::Colour(0xff0a1822));
        setColour(juce::TextButton::textColourOnId,        juce::Colour(oled));
    }

public:
    // Hard-pixel rendering: square corners, no anti-aliased curves.
    float getButtonRadius() const override { return 0.0f; }

    // Buttons render as plain 1px outlined rectangles in the single
    // OLED hue.  "Highlighted" / "down" use a thicker outline rather
    // than a brightness change, since real OLED has no shade tiers.
    // The boot wave sets the lkColor property — interpret its
    // brightness as 1-bit lit/unlit so the wave is still visible in
    // the OLED theme: ~0 → off (no outline), ~1 → fully filled.
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour&,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        const juce::uint32 oled = getOledColour();
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        auto px = juce::Rectangle<int>((int) bounds.getX(), (int) bounds.getY(),
                                       (int) bounds.getWidth(), (int) bounds.getHeight());

        if (button.getProperties().contains("lkColor"))
        {
            const auto lk = juce::Colour((juce::uint32) (int) button.getProperties()["lkColor"]);
            const float bri = lk.getBrightness();
            if (bri < 0.1f) return;                          // off
            g.setColour(juce::Colour(oled));
            if (bri > 0.8f) g.fillRect(px);                  // fully lit (boot crest)
            else            g.drawRect(px, 1);               // outline (post-wave base)
            return;
        }

        g.setColour(juce::Colour(oled));
        if (shouldDrawButtonAsDown)            g.fillRect(px);
        else if (shouldDrawButtonAsHighlighted) g.drawRect(px, 2);
        else                                    g.drawRect(px, 1);
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool /*shouldDrawButtonAsHighlighted*/,
                        bool shouldDrawButtonAsDown) override
    {
        const juce::uint32 oled = getOledColour();
        // When the button is pressed we filled it solid, so the text
        // needs to be the inverse colour (black) to stay readable.
        g.setColour(shouldDrawButtonAsDown ? juce::Colour(0xff000000)
                                           : juce::Colour(oled));
        g.setFont(juce::Font(getUIFontName(),
                  juce::jmin(12.0f, button.getHeight() * 0.5f),
                  juce::Font::bold));
        g.drawText(formatButtonText(button.getButtonText()),
                   button.getLocalBounds().reduced(2),
                   juce::Justification::centred);
    }

    void drawComboBox(juce::Graphics& g, int width, int height, bool,
                      int, int, int, int, juce::ComboBox&) override
    {
        const juce::uint32 oled = getOledColour();
        g.setColour(juce::Colour(oled));
        g.drawRect(0, 0, width, height, 1);
    }

    // No top-bar fill or accent stripe — pure black, just a 1px line.
    void drawTopBarBackground(juce::Graphics& g, int x, int y, int width, int height) override
    {
        g.setColour(juce::Colour(getOledColour()));
        g.fillRect(x, y + height - 1, width, 1);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LaunchkeyOledLookAndFeel)
};
