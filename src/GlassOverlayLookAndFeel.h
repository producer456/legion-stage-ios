#pragma once

#include "DawLookAndFeel.h"

// "Glass Overlay" — every control floats on frosted glass over the arranger.
//
// The timeline renders full-screen behind all UI chrome.
// Top bar, toolbar, and right panel are frosted dark glass overlays.
// Every button is a glass pill floating on the frosted surface.
// The arranger grid bleeds through everything.
//
class GlassOverlayLookAndFeel : public DawLookAndFeel
{
public:
    GlassOverlayLookAndFeel()
    {
        // Body — near-black (transparency handled by MainComponent glass overlay rendering)
        theme.body        = 0xff080810;
        theme.bodyLight   = 0xff0a0a14;
        theme.bodyDark    = 0xff060610;
        theme.border      = 0x1effffff;
        theme.borderLight = 0x30ffffff;

        // Text — white vibrancy
        theme.textPrimary   = 0xffffffff;
        theme.textSecondary = 0x99ffffff;
        theme.textBright    = 0xffffffff;

        // Accents — pale ice blue
        theme.amber     = 0xffc0dfff;
        theme.amberDark = 0xff6aa0d8;
        theme.green     = 0xff30d158;
        theme.greenDark = 0xff0a3018;
        theme.red       = 0xffff453a;
        theme.redDark   = 0xff3a1010;

        // LCD
        theme.lcdBg    = 0xff000000;
        theme.lcdText  = 0xddffffff;
        theme.lcdAmber = 0xffc0dfff;

        // Buttons — Apple systemFill dark: (120,120,128) at varying alpha
        theme.buttonFace  = 0x5c787880;  // systemFill dark 0.36
        theme.buttonHover = 0x68787880;
        theme.buttonDown  = 0x78787880;

        theme.btnStop       = 0x1effffff;
        theme.btnMetronome  = 0x1effffff;
        theme.btnMetronomeOn = 0x33ffffff;
        theme.btnCountIn    = 0x1effffff;
        theme.btnCountInOn  = 0x33ffffff;
        theme.btnLoop       = 0x1effffff;
        theme.btnLoopOn     = 0x33ffffff;
        theme.btnNewClip    = 0x1effffff;
        theme.btnDuplicate  = 0x1effffff;
        theme.btnSplit      = 0x1effffff;
        theme.btnQuantize   = 0x1effffff;
        theme.btnEditNotes  = 0x1effffff;
        theme.btnNav        = 0x1effffff;
        theme.btnSave       = 0x1effffff;
        theme.btnLoad       = 0x1effffff;
        theme.btnUndoRedo   = 0x1effffff;
        theme.btnMidi2      = 0x1effffff;
        theme.btnMidi2On    = 0x33ffffff;
        theme.btnDeleteClip = 0x1effffff;

        theme.loopRegion = 0x18c0dfff;
        theme.loopBorder = 0xffc0dfff;

        // Timeline — dark with visible grid (this IS the background)
        theme.timelineBg         = 0xff080810;
        theme.timelineAltRow     = 0xff0c0c14;
        theme.timelineSelectedRow = 0x1effffff;
        theme.timelineGridMajor  = 0x28ffffff;
        theme.timelineGridMinor  = 0x12ffffff;
        theme.timelineGridFaint  = 0x08ffffff;
        theme.timelineGridBeat   = 0x18ffffff;

        // Clips — glass
        theme.clipDefault   = 0x28ffffff;
        theme.clipRecording = 0x40ff453a;
        theme.clipQueued    = 0x30ffffff;
        theme.clipPlaying   = 0x30ffffff;
        theme.clipNotePreview = 0xb0ffffff;

        theme.playhead     = 0xffc0dfff;
        theme.playheadGlow = 0x20c0dfff;
        theme.accentStripe = 0xffc0dfff;

        theme.trackSelected = 0x1effffff;
        theme.trackArmed    = 0x28ff453a;
        theme.trackMuteOn   = 0xffff453a;
        theme.trackSoloOn   = 0xffffd60a;
        theme.trackSoloText = 0xff000000;

        applyThemeColors();

        setColour(juce::TextButton::textColourOnId, juce::Colour(0xffffffff));
        setColour(juce::ComboBox::backgroundColourId,   juce::Colour(0x1effffff));
        setColour(juce::ComboBox::textColourId,          juce::Colour(0xffffffff));
        setColour(juce::ComboBox::outlineColourId,       juce::Colour(0x1effffff));
        setColour(juce::PopupMenu::backgroundColourId,   juce::Colour(0xf0101018));
        setColour(juce::PopupMenu::textColourId,         juce::Colour(0xffffffff));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0x40ffffff));
        setColour(juce::PopupMenu::highlightedTextColourId,       juce::Colour(0xffffffff));
        setColour(juce::Slider::thumbColourId,           juce::Colour(0xffc0dfff));
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffc0dfff));
        setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0x1effffff));
    }

    float getButtonRadius() const override { return 12.0f; }
    juce::String getUIFontName() const override { return "DIN Alternate"; }
    juce::String getDisplayFontName() const override { return "DIN Alternate"; }

    // Flag for MainComponent to know this theme needs glass overlay rendering
    bool isGlassOverlay() const { return true; }
    bool isGlassOverlayTheme() const override { return true; }

    // Skip OLED art — clean text
    bool drawOledButtonArt(juce::Image&, const juce::String&,
                           bool, float, juce::Colour, juce::Colour) const override
    { return false; }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool, bool) override
    {
        bool on = button.getToggleState();
        g.setFont(juce::Font(getUIFontName(),
                  juce::jmin(12.0f, button.getHeight() * 0.45f), juce::Font::bold));
        g.setColour(on ? juce::Colour(0xffc0dfff) : juce::Colours::white.withAlpha(0.7f));
        g.drawText(button.getButtonText().toUpperCase(),
                   button.getLocalBounds().reduced(2, 1), juce::Justification::centred);
    }

    // Glass button — gradient fill with specular
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool isHighlighted, bool isDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
        float radius = juce::jmin(bounds.getHeight() / 2.0f, 12.0f);
        bool on = button.getToggleState();

        // OLED buttons — solid black
        if (isOledAnimatedButton(button.getButtonText()))
        {
            g.setColour(juce::Colours::black);
            g.fillRoundedRectangle(bounds, radius);
            g.setColour(juce::Colours::white.withAlpha(on ? 0.15f : 0.06f));
            g.drawRoundedRectangle(bounds, radius, 0.5f);
            return;
        }

        // Apple systemFill: neutral gray at varying opacities
        float fillAlpha = isDown ? 0.50f : (isHighlighted ? 0.42f : 0.36f);
        if (on) fillAlpha = 0.44f;
        g.setColour(juce::Colour(120, 120, 128).withAlpha(fillAlpha));
        g.fillRoundedRectangle(bounds, radius);

        // Ice blue tint on active — low saturation (10-15%)
        if (isDown || on)
        {
            g.setColour(juce::Colour(0xffc0dfff).withAlpha(0.10f));
            g.fillRoundedRectangle(bounds, radius);
        }

        // Inner highlight — 1px top edge
        if (!isDown)
        {
            g.setColour(juce::Colours::white.withAlpha(0.12f));
            g.fillRoundedRectangle(bounds.getX() + radius * 0.3f, bounds.getY() + 0.5f,
                                   bounds.getWidth() * 0.5f, 1.0f, 0.5f);
        }

        // Border — Apple separator dark
        g.setColour(juce::Colour(84, 84, 88).withAlpha(on ? 0.50f : 0.30f));
        g.drawRoundedRectangle(bounds, radius, 0.5f);
    }

    // Glass combo box
    void drawComboBox(juce::Graphics& g, int width, int height, bool isDown,
                      int, int, int, int, juce::ComboBox&) override
    {
        auto bounds = juce::Rectangle<float>(0, 0, (float)width, (float)height).reduced(1.0f);
        float radius = juce::jmin(bounds.getHeight() / 2.0f, 10.0f);

        g.setGradientFill(juce::ColourGradient(
            juce::Colours::white.withAlpha(isDown ? 0.20f : 0.14f), 0, 0,
            juce::Colours::white.withAlpha(isDown ? 0.10f : 0.06f), 0, (float)height, false));
        g.fillRoundedRectangle(bounds, radius);

        g.setColour(juce::Colours::white.withAlpha(0.12f));
        g.drawRoundedRectangle(bounds, radius, 0.8f);

        float arrowX = (float)width - 18.0f;
        float arrowY = (float)height * 0.5f;
        juce::Path arrow;
        arrow.addTriangle(arrowX - 4, arrowY - 2, arrowX + 4, arrowY - 2, arrowX, arrowY + 3);
        g.setColour(juce::Colour(theme.textSecondary));
        g.fillPath(arrow);
    }

    // Glass rotary slider
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider&) override
    {
        float radius = juce::jmin(width / 2.0f, height / 2.0f) - 4.0f;
        float centreX = x + width * 0.5f;
        float centreY = y + height * 0.5f;
        float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // Glass body
        juce::ColourGradient grad(
            juce::Colours::white.withAlpha(0.18f), centreX, centreY - radius,
            juce::Colours::white.withAlpha(0.06f), centreX, centreY + radius, false);
        g.setGradientFill(grad);
        g.fillEllipse(centreX - radius, centreY - radius, radius * 2, radius * 2);

        g.setColour(juce::Colours::white.withAlpha(0.15f));
        g.drawEllipse(centreX - radius, centreY - radius, radius * 2, radius * 2, 0.8f);

        // Specular
        float hlR = radius * 0.45f;
        float hlY = centreY - radius * 0.3f;
        g.setColour(juce::Colours::white.withAlpha(0.14f));
        g.fillEllipse(centreX - hlR, hlY - hlR * 0.4f, hlR * 2, hlR * 0.8f);

        // Background arc
        juce::Path bgArc;
        bgArc.addCentredArc(centreX, centreY, radius + 2, radius + 2,
                            0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        g.strokePath(bgArc, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

        // Value arc
        juce::Path arc;
        arc.addCentredArc(centreX, centreY, radius + 2, radius + 2,
                          0.0f, rotaryStartAngle, angle, true);
        g.setColour(juce::Colour(0xffc0dfff).withAlpha(0.7f));
        g.strokePath(arc, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));

        // Pointer dot
        float dotR = juce::jmax(1.5f, radius * 0.08f);
        float dotDist = radius * 0.6f;
        float px = centreX + dotDist * std::cos(angle - juce::MathConstants<float>::halfPi);
        float py = centreY + dotDist * std::sin(angle - juce::MathConstants<float>::halfPi);
        g.setColour(juce::Colours::white.withAlpha(0.6f));
        g.fillEllipse(px - dotR, py - dotR, dotR * 2, dotR * 2);
    }

    // Glass linear slider
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float, float,
                          const juce::Slider::SliderStyle, juce::Slider&) override
    {
        bool isHorizontal = width > height;
        float trackThickness = 2.0f;

        if (isHorizontal)
        {
            float trackY = y + height * 0.5f - trackThickness * 0.5f;
            g.setColour(juce::Colours::white.withAlpha(0.06f));
            g.fillRoundedRectangle((float)x, trackY, (float)width, trackThickness, 1.0f);
            g.setColour(juce::Colour(0xffc0dfff).withAlpha(0.6f));
            g.fillRoundedRectangle((float)x, trackY, sliderPos - x, trackThickness, 1.0f);
            float thumbR = 5.0f;
            g.setColour(juce::Colours::white.withAlpha(0.5f));
            g.fillEllipse(sliderPos - thumbR, trackY + trackThickness * 0.5f - thumbR, thumbR * 2, thumbR * 2);
        }
        else
        {
            float trackX = x + width * 0.5f - trackThickness * 0.5f;
            g.setColour(juce::Colours::white.withAlpha(0.06f));
            g.fillRoundedRectangle(trackX, (float)y, trackThickness, (float)height, 1.0f);
            g.setColour(juce::Colour(0xffc0dfff).withAlpha(0.6f));
            g.fillRoundedRectangle(trackX, sliderPos, trackThickness, (float)(y + height) - sliderPos, 1.0f);
            float thumbR = 5.0f;
            g.setColour(juce::Colours::white.withAlpha(0.5f));
            g.fillEllipse(trackX + trackThickness * 0.5f - thumbR, sliderPos - thumbR, thumbR * 2, thumbR * 2);
        }
    }
};
