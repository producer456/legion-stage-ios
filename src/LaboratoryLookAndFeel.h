#pragma once

#include "DawLookAndFeel.h"

// "Laboratory" — Vintage scientific instrument aesthetic.
//
// Inspired by handcrafted modular synthesizers with cream/white panels,
// warm walnut wood frames, teal oscilloscope displays, and silver hardware.
// Clean, tactile, analog feel.
//
class LaboratoryLookAndFeel : public DawLookAndFeel
{
public:
    LaboratoryLookAndFeel()
    {
        // ── Body — warm cream panels with wood-toned darks ──
        theme.body        = 0xffece7df;  // warm cream panel
        theme.bodyLight   = 0xfff4f0ea;  // lighter panel surface
        theme.bodyDark    = 0xffddd6cb;  // recessed area / toolbar
        theme.border      = 0x40705846;  // subtle warm divider
        theme.borderLight = 0x28705846;  // raised edge

        // ── Text — dark warm gray, lab-style ──
        theme.textPrimary   = 0xff2c2a26;  // primary labels
        theme.textSecondary = 0x993c3832;  // secondary / dimmed
        theme.textBright    = 0xff1a1814;  // bright emphasis

        // ── Accents — teal oscilloscope + warm amber ──
        theme.amber     = 0xff3ab89a;  // teal (main accent)
        theme.amberDark = 0xff2a8a72;  // darker teal
        theme.green     = 0xff4ec9a0;  // bright green indicator
        theme.greenDark = 0xff2d9470;
        theme.red       = 0xffcf4a3a;  // warm red (stop/record)
        theme.redDark   = 0xffa33a2d;

        // ── LCD — dark teal oscilloscope display ──
        theme.lcdBg    = 0xff122a24;  // dark scope background
        theme.lcdText  = 0xff4aecc4;  // bright teal text
        theme.lcdAmber = 0xff3ab89a;  // teal accent in LCD

        // ── Buttons — silver/chrome on cream ──
        theme.buttonFace  = 0x1a6b6358;  // warm transparent fill
        theme.buttonHover = 0x286b6358;
        theme.buttonDown  = 0x333ab89a;  // teal tint on press

        theme.btnStop       = 0x1a6b6358;
        theme.btnMetronome  = 0x1a6b6358;
        theme.btnMetronomeOn = 0x333ab89a;
        theme.btnCountIn    = 0x1a6b6358;
        theme.btnCountInOn  = 0x333ab89a;
        theme.btnLoop       = 0x1a6b6358;
        theme.btnLoopOn     = 0x333ab89a;
        theme.btnNewClip    = 0x1a6b6358;
        theme.btnDuplicate  = 0x1a6b6358;
        theme.btnSplit      = 0x1a6b6358;
        theme.btnQuantize   = 0x1a6b6358;
        theme.btnEditNotes  = 0x1a6b6358;
        theme.btnNav        = 0x1a6b6358;
        theme.btnSave       = 0x1a6b6358;
        theme.btnLoad       = 0x1a6b6358;
        theme.btnUndoRedo   = 0x1a6b6358;
        theme.btnMidi2      = 0x1a6b6358;
        theme.btnMidi2On    = 0x333ab89a;
        theme.btnDeleteClip = 0x1acf4a3a;

        theme.loopRegion = 0x183ab89a;
        theme.loopBorder = 0xff3ab89a;

        // ── Timeline — cream with warm grid lines ──
        theme.timelineBg         = 0xfff0ebe3;
        theme.timelineAltRow     = 0xffe8e2d8;
        theme.timelineSelectedRow = 0x203ab89a;
        theme.timelineGridMajor  = 0x226b6358;
        theme.timelineGridMinor  = 0x126b6358;
        theme.timelineGridFaint  = 0x086b6358;
        theme.timelineGridBeat   = 0x186b6358;

        // ── Clips — teal-tinted ──
        theme.clipDefault   = 0x303ab89a;
        theme.clipRecording = 0x40cf4a3a;
        theme.clipQueued    = 0x30c89050;
        theme.clipPlaying   = 0x304ec9a0;
        theme.clipNotePreview = 0x902c2a26;

        theme.playhead     = 0xff3ab89a;
        theme.playheadGlow = 0x203ab89a;
        theme.accentStripe = 0xff7a5c3a;  // walnut wood accent

        theme.trackSelected = 0x203ab89a;
        theme.trackArmed    = 0x20cf4a3a;
        theme.trackMuteOn   = 0xffcf4a3a;
        theme.trackSoloOn   = 0xffc89050;  // warm amber solo
        theme.trackSoloText = 0xff1a1814;

        applyThemeColors();

        // Override JUCE widget colors
        setColour(juce::TextButton::textColourOnId, juce::Colour(0xff3ab89a));

        setColour(juce::ComboBox::backgroundColourId,   juce::Colour(0x1a6b6358));
        setColour(juce::ComboBox::textColourId,          juce::Colour(0xff2c2a26));
        setColour(juce::ComboBox::outlineColourId,       juce::Colour(0x1a6b6358));
        setColour(juce::PopupMenu::backgroundColourId,   juce::Colour(0xfff4f0ea));
        setColour(juce::PopupMenu::textColourId,         juce::Colour(0xff2c2a26));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff3ab89a));
        setColour(juce::PopupMenu::highlightedTextColourId,       juce::Colour(0xffffffff));
        setColour(juce::Slider::thumbColourId,           juce::Colour(0xff3ab89a));
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff3ab89a));
        setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0x1a6b6358));
    }

    float getButtonRadius() const override { return 10.0f; }
    bool isGlassOverlayTheme() const override { return true; }
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
        g.setColour(on ? juce::Colour(0xff3ab89a) : juce::Colour(0xff2c2a26).withAlpha(0.7f));
        g.drawText(button.getButtonText().toUpperCase(),
                   button.getLocalBounds().reduced(2, 1),
                   juce::Justification::centred);
    }

    // ── Buttons — raised cream with subtle shadow (like hardware panel buttons) ──
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour&,
                              bool isHighlighted, bool isDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
        float radius = juce::jmin(bounds.getHeight() / 2.0f, 10.0f);
        bool on = button.getToggleState();

        // OLED buttons — cream background
        if (isOledAnimatedButton(button.getButtonText()))
        {
            g.setColour(juce::Colour(0xfff4f0ea));
            g.fillRoundedRectangle(bounds, radius);
            g.setColour(juce::Colour(0x186b6358));
            g.drawRoundedRectangle(bounds, radius, 0.5f);
            return;
        }

        // Raised panel button
        {
            float topAlpha = isDown ? 0.20f : (isHighlighted ? 0.14f : 0.08f);
            float botAlpha = topAlpha * 0.4f;
            if (on) { topAlpha = 0.14f; botAlpha = 0.05f; }
            g.setGradientFill(juce::ColourGradient(
                juce::Colour(0xff6b6358).withAlpha(topAlpha), bounds.getX(), bounds.getY(),
                juce::Colour(0xff6b6358).withAlpha(botAlpha), bounds.getX(), bounds.getBottom(), false));
            g.fillRoundedRectangle(bounds, radius);
        }

        // Teal tint on active
        if (isDown || on)
        {
            g.setColour(juce::Colour(0xff3ab89a).withAlpha(isDown ? 0.12f : 0.06f));
            g.fillRoundedRectangle(bounds, radius);
        }

        // Bottom shadow (hardware feel)
        if (!isDown)
        {
            g.setColour(juce::Colours::black.withAlpha(0.05f));
            g.fillRoundedRectangle(bounds.getX(), bounds.getBottom() - 1.5f,
                                   bounds.getWidth(), 1.5f, 0.5f);
        }

        // Border — warm
        g.setColour(juce::Colour(0xff6b6358).withAlpha(on ? 0.20f : 0.12f));
        g.drawRoundedRectangle(bounds, radius, 0.8f);
    }

    // ── Combo box ──
    void drawComboBox(juce::Graphics& g, int width, int height, bool isDown,
                      int, int, int, int, juce::ComboBox&) override
    {
        auto bounds = juce::Rectangle<float>(0, 0, (float)width, (float)height).reduced(1.0f);
        float radius = juce::jmin(bounds.getHeight() / 2.0f, 10.0f);

        g.setGradientFill(juce::ColourGradient(
            juce::Colour(0xff6b6358).withAlpha(isDown ? 0.14f : 0.07f), 0, 0,
            juce::Colour(0xff6b6358).withAlpha(isDown ? 0.07f : 0.03f), 0, (float)height, false));
        g.fillRoundedRectangle(bounds, radius);

        g.setColour(juce::Colour(0xff6b6358).withAlpha(0.12f));
        g.drawRoundedRectangle(bounds, radius, 0.8f);

        float arrowX = (float)width - 18.0f;
        float arrowY = (float)height * 0.5f;
        juce::Path arrow;
        arrow.addTriangle(arrowX - 4, arrowY - 2, arrowX + 4, arrowY - 2, arrowX, arrowY + 3);
        g.setColour(juce::Colour(theme.textSecondary));
        g.fillPath(arrow);
    }

    // ── Rotary knob — white knob with teal arc (like the hardware) ──
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider&) override
    {
        float radius = juce::jmin(width / 2.0f, height / 2.0f) - 4.0f;
        float centreX = x + width * 0.5f;
        float centreY = y + height * 0.5f;
        float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // Knob body — cream white gradient
        {
            juce::ColourGradient grad(
                juce::Colour(0xfff8f4ee), centreX, centreY - radius,
                juce::Colour(0xffe0d8cc), centreX, centreY + radius, false);
            g.setGradientFill(grad);
            g.fillEllipse(centreX - radius, centreY - radius, radius * 2, radius * 2);
        }

        // Chrome-like border
        g.setColour(juce::Colour(0xffc0b8a8));
        g.drawEllipse(centreX - radius, centreY - radius, radius * 2, radius * 2, 1.0f);

        // Subtle shadow below
        g.setColour(juce::Colours::black.withAlpha(0.08f));
        g.drawEllipse(centreX - radius, centreY - radius + 1.0f, radius * 2, radius * 2, 1.0f);

        // Background arc
        juce::Path bgArc;
        bgArc.addCentredArc(centreX, centreY, radius + 2, radius + 2,
                            0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(0xff6b6358).withAlpha(0.10f));
        g.strokePath(bgArc, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

        // Value arc — teal
        juce::Path arc;
        arc.addCentredArc(centreX, centreY, radius + 2, radius + 2,
                          0.0f, rotaryStartAngle, angle, true);
        g.setColour(juce::Colour(0xff3ab89a));
        g.strokePath(arc, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));

        // Pointer line (like a hardware knob indicator)
        float pointerLen = radius * 0.55f;
        float px = centreX + pointerLen * std::cos(angle - juce::MathConstants<float>::halfPi);
        float py = centreY + pointerLen * std::sin(angle - juce::MathConstants<float>::halfPi);
        g.setColour(juce::Colour(0xff2c2a26));
        g.drawLine(centreX, centreY, px, py, 2.0f);
    }

    // ── Linear slider ──
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float, float,
                          const juce::Slider::SliderStyle, juce::Slider&) override
    {
        bool isHorizontal = width > height;
        float trackThickness = 2.5f;

        if (isHorizontal)
        {
            float trackY = y + height * 0.5f - trackThickness * 0.5f;
            g.setColour(juce::Colour(0xff6b6358).withAlpha(0.10f));
            g.fillRoundedRectangle((float)x, trackY, (float)width, trackThickness, 1.0f);
            g.setColour(juce::Colour(0xff3ab89a));
            g.fillRoundedRectangle((float)x, trackY, sliderPos - x, trackThickness, 1.0f);
            // Cream knob thumb
            g.setColour(juce::Colour(0xfff4f0ea));
            g.fillEllipse(sliderPos - 5, trackY - 3.5f, 10, 10);
            g.setColour(juce::Colour(0xffc0b8a8));
            g.drawEllipse(sliderPos - 5, trackY - 3.5f, 10, 10, 0.8f);
        }
        else
        {
            float trackX = x + width * 0.5f - trackThickness * 0.5f;
            g.setColour(juce::Colour(0xff6b6358).withAlpha(0.10f));
            g.fillRoundedRectangle(trackX, (float)y, trackThickness, (float)height, 1.0f);
            g.setColour(juce::Colour(0xff3ab89a));
            g.fillRoundedRectangle(trackX, sliderPos, trackThickness, (float)(y + height) - sliderPos, 1.0f);
            g.setColour(juce::Colour(0xfff4f0ea));
            g.fillEllipse(trackX - 3.5f, sliderPos - 5, 10, 10);
            g.setColour(juce::Colour(0xffc0b8a8));
            g.drawEllipse(trackX - 3.5f, sliderPos - 5, 10, 10, 0.8f);
        }
    }
};
