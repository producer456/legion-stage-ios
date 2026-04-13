#pragma once

#include "DawLookAndFeel.h"

// "Liquid Glass Light" — Apple-style light mode glass for Legion Stage.
//
// White/cream surfaces with translucent dark fills for controls.
// Apple system colors (light mode variants).
// Same glass rendering approach as dark Liquid Glass but inverted.
//
class LiquidGlassLightLookAndFeel : public DawLookAndFeel
{
public:
    LiquidGlassLightLookAndFeel()
    {
        // ── Body — Apple light mode backgrounds ──
        theme.body        = 0xffffffff;  // systemBackgroundColor (light)
        theme.bodyLight   = 0xfff2f2f7;  // secondarySystemBackgroundColor (light)
        theme.bodyDark    = 0xffe5e5ea;  // tertiarySystemBackgroundColor (light)
        theme.border      = 0x4d3c3c43;  // separatorColor (light)
        theme.borderLight = 0x2e3c3c43;  // opaqueSeparatorColor (light)

        // ── Text — Apple label hierarchy (light) ──
        theme.textPrimary   = 0xff000000;  // labelColor (light)
        theme.textSecondary = 0x993c3c43;  // secondaryLabelColor (light)
        theme.textBright    = 0xff000000;

        // ── Accents — Apple system colors (light mode) ──
        theme.amber     = 0xff007aff;  // systemBlue (light)
        theme.amberDark = 0xff0055cc;
        theme.green     = 0xff34c759;  // systemGreen (light)
        theme.greenDark = 0xff1a8a3a;
        theme.red       = 0xffff3b30;  // systemRed (light)
        theme.redDark   = 0xffcc2f26;

        // ── LCD — light mode OLED ──
        theme.lcdBg    = 0xfff2f2f7;
        theme.lcdText  = 0xff000000;
        theme.lcdAmber = 0xff007aff;

        // ── Buttons — dark fills on light (Apple tertiarySystemFill light) ──
        theme.buttonFace  = 0x1e787880;  // tertiarySystemFillColor (light)
        theme.buttonHover = 0x28787880;
        theme.buttonDown  = 0x33007aff;

        theme.btnStop       = 0x1e787880;
        theme.btnMetronome  = 0x1e787880;
        theme.btnMetronomeOn = 0x33007aff;
        theme.btnCountIn    = 0x1e787880;
        theme.btnCountInOn  = 0x33007aff;
        theme.btnLoop       = 0x1e787880;
        theme.btnLoopOn     = 0x33007aff;
        theme.btnNewClip    = 0x1e787880;
        theme.btnDuplicate  = 0x1e787880;
        theme.btnSplit      = 0x1e787880;
        theme.btnQuantize   = 0x1e787880;
        theme.btnEditNotes  = 0x1e787880;
        theme.btnNav        = 0x1e787880;
        theme.btnSave       = 0x1e787880;
        theme.btnLoad       = 0x1e787880;
        theme.btnUndoRedo   = 0x1e787880;
        theme.btnMidi2      = 0x1e787880;
        theme.btnMidi2On    = 0x33007aff;
        theme.btnDeleteClip = 0x1eff3b30;

        theme.loopRegion = 0x18007aff;
        theme.loopBorder = 0xff007aff;

        // ── Timeline — light with subtle grid ──
        theme.timelineBg         = 0xffffffff;
        theme.timelineAltRow     = 0xfff8f8fa;
        theme.timelineSelectedRow = 0x20007aff;
        theme.timelineGridMajor  = 0x1e3c3c43;
        theme.timelineGridMinor  = 0x0f3c3c43;
        theme.timelineGridFaint  = 0x083c3c43;
        theme.timelineGridBeat   = 0x143c3c43;

        // ── Clips ──
        theme.clipDefault   = 0x30007aff;
        theme.clipRecording = 0x40ff3b30;
        theme.clipQueued    = 0x30ff9500;
        theme.clipPlaying   = 0x3034c759;
        theme.clipNotePreview = 0x90000000;

        theme.playhead     = 0xff007aff;
        theme.playheadGlow = 0x20007aff;
        theme.accentStripe = 0xff007aff;

        theme.trackSelected = 0x20007aff;
        theme.trackArmed    = 0x20ff3b30;
        theme.trackMuteOn   = 0xffff3b30;
        theme.trackSoloOn   = 0xffffcc00;
        theme.trackSoloText = 0xff000000;

        applyThemeColors();

        // Override text on for light bg
        setColour(juce::TextButton::textColourOnId, juce::Colour(0xff007aff));

        setColour(juce::ComboBox::backgroundColourId,   juce::Colour(0x1e787880));
        setColour(juce::ComboBox::textColourId,          juce::Colour(0xff000000));
        setColour(juce::ComboBox::outlineColourId,       juce::Colour(0x1e3c3c43));
        setColour(juce::PopupMenu::backgroundColourId,   juce::Colour(0xfff2f2f7));
        setColour(juce::PopupMenu::textColourId,         juce::Colour(0xff000000));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff007aff));
        setColour(juce::PopupMenu::highlightedTextColourId,       juce::Colour(0xffffffff));
        setColour(juce::Slider::thumbColourId,           juce::Colour(0xff007aff));
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff007aff));
        setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0x1e3c3c43));
    }

    float getButtonRadius() const override { return 12.0f; }
    juce::String getUIFontName() const override { return "DIN Alternate"; }
    juce::String getDisplayFontName() const override { return "DIN Alternate"; }

    // Skip OLED art — clean text for light theme
    bool drawOledButtonArt(juce::Image&, const juce::String&,
                           bool, float, juce::Colour, juce::Colour) const override
    { return false; }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool, bool) override
    {
        bool on = button.getToggleState();
        auto font = juce::Font(getUIFontName(),
                               juce::jmin(12.0f, button.getHeight() * 0.45f),
                               juce::Font::bold);
        g.setFont(font);
        g.setColour(on ? juce::Colour(0xff007aff) : juce::Colour(0xff000000).withAlpha(0.7f));
        g.drawText(button.getButtonText().toUpperCase(),
                   button.getLocalBounds().reduced(2, 1),
                   juce::Justification::centred);
    }

    // ── Glass button (dark on light) ──
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool isHighlighted, bool isDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
        float radius = juce::jmin(bounds.getHeight() / 2.0f, 12.0f);
        bool on = button.getToggleState();

        // OLED buttons get white background
        if (isOledAnimatedButton(button.getButtonText()))
        {
            g.setColour(juce::Colours::white);
            g.fillRoundedRectangle(bounds, radius);
            g.setColour(juce::Colour(0x183c3c43));
            g.drawRoundedRectangle(bounds, radius, 0.5f);
            return;
        }

        // Glass fill — dark on light with gradient
        {
            float topAlpha = isDown ? 0.22f : (isHighlighted ? 0.16f : 0.10f);
            float botAlpha = topAlpha * 0.4f;
            if (on) { topAlpha = 0.15f; botAlpha = 0.06f; }
            g.setGradientFill(juce::ColourGradient(
                juce::Colour(0xff3c3c43).withAlpha(topAlpha), bounds.getX(), bounds.getY(),
                juce::Colour(0xff3c3c43).withAlpha(botAlpha), bounds.getX(), bounds.getBottom(), false));
            g.fillRoundedRectangle(bounds, radius);
        }

        // Blue tint on active
        if (isDown || on)
        {
            g.setColour(juce::Colour(0xff007aff).withAlpha(isDown ? 0.12f : 0.06f));
            g.fillRoundedRectangle(bounds, radius);
        }

        // Bottom shadow edge
        if (!isDown)
        {
            g.setColour(juce::Colours::black.withAlpha(0.04f));
            g.fillRoundedRectangle(bounds.getX(), bounds.getBottom() - 1.5f,
                                   bounds.getWidth(), 1.5f, 0.5f);
        }

        // Border
        g.setColour(juce::Colour(0xff3c3c43).withAlpha(on ? 0.18f : 0.10f));
        g.drawRoundedRectangle(bounds, radius, 0.8f);
    }

    // ── Glass combo box ──
    void drawComboBox(juce::Graphics& g, int width, int height, bool isDown,
                      int, int, int, int, juce::ComboBox&) override
    {
        auto bounds = juce::Rectangle<float>(0, 0, (float)width, (float)height).reduced(1.0f);
        float radius = juce::jmin(bounds.getHeight() / 2.0f, 10.0f);

        g.setGradientFill(juce::ColourGradient(
            juce::Colour(0xff3c3c43).withAlpha(isDown ? 0.16f : 0.08f), 0, 0,
            juce::Colour(0xff3c3c43).withAlpha(isDown ? 0.08f : 0.03f), 0, (float)height, false));
        g.fillRoundedRectangle(bounds, radius);

        g.setColour(juce::Colour(0xff3c3c43).withAlpha(0.10f));
        g.drawRoundedRectangle(bounds, radius, 0.8f);

        float arrowX = (float)width - 18.0f;
        float arrowY = (float)height * 0.5f;
        juce::Path arrow;
        arrow.addTriangle(arrowX - 4, arrowY - 2, arrowX + 4, arrowY - 2, arrowX, arrowY + 3);
        g.setColour(juce::Colour(theme.textSecondary));
        g.fillPath(arrow);
    }

    // ── Glass rotary slider ──
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider&) override
    {
        float radius = juce::jmin(width / 2.0f, height / 2.0f) - 4.0f;
        float centreX = x + width * 0.5f;
        float centreY = y + height * 0.5f;
        float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // Knob body
        {
            juce::ColourGradient grad(
                juce::Colours::white, centreX, centreY - radius,
                juce::Colour(0xffe8e8ed), centreX, centreY + radius, false);
            g.setGradientFill(grad);
            g.fillEllipse(centreX - radius, centreY - radius, radius * 2, radius * 2);
        }

        // Border
        g.setColour(juce::Colour(0xff3c3c43).withAlpha(0.15f));
        g.drawEllipse(centreX - radius, centreY - radius, radius * 2, radius * 2, 0.8f);

        // Subtle shadow below
        g.setColour(juce::Colours::black.withAlpha(0.06f));
        g.drawEllipse(centreX - radius, centreY - radius + 1.0f, radius * 2, radius * 2, 1.0f);

        // Background arc
        juce::Path bgArc;
        bgArc.addCentredArc(centreX, centreY, radius + 2, radius + 2,
                            0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(0xff3c3c43).withAlpha(0.08f));
        g.strokePath(bgArc, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

        // Value arc
        juce::Path arc;
        arc.addCentredArc(centreX, centreY, radius + 2, radius + 2,
                          0.0f, rotaryStartAngle, angle, true);
        g.setColour(juce::Colour(0xff007aff));
        g.strokePath(arc, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));

        // Pointer dot
        float dotR = juce::jmax(1.5f, radius * 0.08f);
        float dotDist = radius * 0.6f;
        float px = centreX + dotDist * std::cos(angle - juce::MathConstants<float>::halfPi);
        float py = centreY + dotDist * std::sin(angle - juce::MathConstants<float>::halfPi);
        g.setColour(juce::Colour(0xff007aff));
        g.fillEllipse(px - dotR, py - dotR, dotR * 2, dotR * 2);
    }

    // ── Glass linear slider ──
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float, float,
                          const juce::Slider::SliderStyle, juce::Slider&) override
    {
        bool isHorizontal = width > height;
        float trackThickness = 2.0f;

        if (isHorizontal)
        {
            float trackY = y + height * 0.5f - trackThickness * 0.5f;
            g.setColour(juce::Colour(0xff3c3c43).withAlpha(0.08f));
            g.fillRoundedRectangle((float)x, trackY, (float)width, trackThickness, 1.0f);
            g.setColour(juce::Colour(0xff007aff));
            g.fillRoundedRectangle((float)x, trackY, sliderPos - x, trackThickness, 1.0f);
            g.setColour(juce::Colours::white);
            g.fillEllipse(sliderPos - 5, trackY - 3.5f, 10, 10);
            g.setColour(juce::Colour(0xff3c3c43).withAlpha(0.15f));
            g.drawEllipse(sliderPos - 5, trackY - 3.5f, 10, 10, 0.5f);
        }
        else
        {
            float trackX = x + width * 0.5f - trackThickness * 0.5f;
            g.setColour(juce::Colour(0xff3c3c43).withAlpha(0.08f));
            g.fillRoundedRectangle(trackX, (float)y, trackThickness, (float)height, 1.0f);
            g.setColour(juce::Colour(0xff007aff));
            g.fillRoundedRectangle(trackX, sliderPos, trackThickness, (float)(y + height) - sliderPos, 1.0f);
            g.setColour(juce::Colours::white);
            g.fillEllipse(trackX - 3.5f, sliderPos - 5, 10, 10);
            g.setColour(juce::Colour(0xff3c3c43).withAlpha(0.15f));
            g.drawEllipse(trackX - 3.5f, sliderPos - 5, 10, 10, 0.5f);
        }
    }
};
