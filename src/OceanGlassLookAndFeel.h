#pragma once

#include "DawLookAndFeel.h"

// "Ocean Glass" — Apple-style translucent glass with teal accent.
//
// Uses Apple's system fill opacities and neutral gray tints.
// Teal jade accent color at low saturation for glass tinting.
//
class OceanGlassLookAndFeel : public DawLookAndFeel
{
public:
    OceanGlassLookAndFeel()
    {
        // Body — pure black void
        theme.body        = 0xff000000;
        theme.bodyLight   = 0xff000000;
        theme.bodyDark    = 0xff000000;
        theme.border      = 0x30787880;
        theme.borderLight = 0x40787880;

        // Text — crisp white like sunlight on water
        theme.textPrimary   = 0xffffffff;
        theme.textSecondary = 0x9980f0e0;
        theme.textBright    = 0xffffffff;

        // Accents — OLED ice blue
        theme.amber     = 0xff50b8d0;  // ice blue
        theme.amberDark = 0xff3090a8;
        theme.green     = 0xff30d158;
        theme.greenDark = 0xff0a3018;
        theme.red       = 0xffff6961;
        theme.redDark   = 0xff3a1818;

        // LCD — ice blue glow
        theme.lcdBg    = 0xff000000;
        theme.lcdText  = 0xff50b8d0;
        theme.lcdAmber = 0xff50b8d0;

        // Buttons — Apple systemFill dark mode: (120,120,128) at 0.36
        uint32_t sysFill    = 0x5c787880;  // systemFill dark
        uint32_t sysFillSec = 0x527878a0;  // secondarySystemFill
        uint32_t sysFillOn  = 0x6e787880;  // slightly brighter for on state

        theme.buttonFace  = sysFill;
        theme.buttonHover = sysFillSec;
        theme.buttonDown  = sysFillOn;

        theme.btnStop       = sysFill;
        theme.btnMetronome  = sysFill;
        theme.btnMetronomeOn = sysFillOn;
        theme.btnCountIn    = sysFill;
        theme.btnCountInOn  = sysFillOn;
        theme.btnLoop       = sysFill;
        theme.btnLoopOn     = sysFillOn;
        theme.btnNewClip    = sysFill;
        theme.btnDuplicate  = sysFill;
        theme.btnSplit      = sysFill;
        theme.btnQuantize   = sysFill;
        theme.btnEditNotes  = sysFill;
        theme.btnNav        = sysFill;
        theme.btnSave       = sysFill;
        theme.btnLoad       = sysFill;
        theme.btnUndoRedo   = sysFill;
        theme.btnMidi2      = sysFill;
        theme.btnMidi2On    = sysFillOn;
        theme.btnDeleteClip = sysFill;

        theme.loopRegion = 0x18b8d8f0;
        theme.loopBorder = 0xff50b8d0;

        // Timeline — black void
        theme.timelineBg         = 0xff000000;
        theme.timelineAltRow     = 0xff060606;
        theme.timelineSelectedRow = 0x20b8d8f0;
        theme.timelineGridMajor  = 0x28b8d8f0;
        theme.timelineGridMinor  = 0x12b8d8f0;
        theme.timelineGridFaint  = 0x08b8d8f0;
        theme.timelineGridBeat   = 0x1ab8d8f0;

        // Clips
        theme.clipDefault   = 0x38b8d8f0;
        theme.clipRecording = 0x40ff8878;
        theme.clipQueued    = 0x28b8d8f0;
        theme.clipPlaying   = 0x38b8d8f0;
        theme.clipNotePreview = 0xb0c0e8ff;

        theme.playhead     = 0xff50b8d0;
        theme.playheadGlow = 0x20b8d8f0;
        theme.accentStripe = 0xff50b8d0;

        theme.trackSelected = 0x20b8d8f0;
        theme.trackArmed    = 0x28ff8878;
        theme.trackMuteOn   = 0xff888888;
        theme.trackSoloOn   = 0xffaaaaaa;
        theme.trackSoloText = 0xff000000;

        applyThemeColors();

        setColour(juce::TextButton::textColourOnId, juce::Colour(0xffffffff));
        setColour(juce::ComboBox::backgroundColourId,   juce::Colour(sysFill));
        setColour(juce::ComboBox::textColourId,          juce::Colour(0xffffffff));
        setColour(juce::ComboBox::outlineColourId,       juce::Colour(sysFill));
        setColour(juce::PopupMenu::backgroundColourId,   juce::Colour(0xf0080808));
        setColour(juce::PopupMenu::textColourId,         juce::Colour(0xffffffff));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0x4080f0e0));
        setColour(juce::PopupMenu::highlightedTextColourId,       juce::Colour(0xffffffff));
        setColour(juce::Slider::thumbColourId,           juce::Colour(0xff50b8d0));
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff50b8d0));
        setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(sysFill));
    }

    float getButtonRadius() const override { return 12.0f; }
    juce::String getUIFontName() const override { return "DIN Alternate"; }
    juce::String getDisplayFontName() const override { return "DIN Alternate"; }

    bool isGlassOverlay() const { return true; }
    bool isGlassOverlayTheme() const override { return true; }

    bool drawOledButtonArt(juce::Image&, const juce::String&,
                           bool, float, juce::Colour, juce::Colour) const override
    { return false; }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool, bool) override
    {
        bool on = button.getToggleState();
        g.setFont(juce::Font(getUIFontName(),
                  juce::jmin(12.0f, button.getHeight() * 0.45f), juce::Font::bold));
        g.setColour(on ? juce::Colour(0xff50b8d0) : juce::Colours::white);
        g.drawText(button.getButtonText().toUpperCase(),
                   button.getLocalBounds().reduced(2, 1), juce::Justification::centred);
    }

    // Apple-style glass button: neutral gray fill, subtle highlight
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool isHighlighted, bool isDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
        float radius = juce::jmin(bounds.getHeight() / 2.0f, 12.0f);
        bool on = button.getToggleState();

        if (isOledAnimatedButton(button.getButtonText()))
        {
            g.setColour(juce::Colours::black);
            g.fillRoundedRectangle(bounds, radius);
            g.setColour(juce::Colour(0xff50b8d0).withAlpha(on ? 0.4f : 0.15f));
            g.drawRoundedRectangle(bounds, radius, 0.5f);
            return;
        }

        // Apple systemFill: neutral gray (120,120,128) at varying opacities
        float fillAlpha = isDown ? 0.50f : (isHighlighted ? 0.42f : 0.36f);
        if (on) fillAlpha = 0.44f;
        g.setColour(juce::Colour(120, 120, 128).withAlpha(fillAlpha));
        g.fillRoundedRectangle(bounds, radius);

        // Teal tint on active — low saturation per Apple guidelines (10-20%)
        if (isDown || on)
        {
            g.setColour(juce::Colour(0xff50b8d0).withAlpha(0.12f));
            g.fillRoundedRectangle(bounds, radius);
        }

        // Inner highlight — 1px top edge at 10-15% (Apple spec)
        if (!isDown)
        {
            g.setColour(juce::Colours::white.withAlpha(0.12f));
            g.fillRoundedRectangle(bounds.getX() + radius * 0.3f, bounds.getY() + 0.5f,
                                   bounds.getWidth() * 0.5f, 1.0f, 0.5f);
        }

        // Border — Apple separator color dark: (84,84,88, 0.6)
        g.setColour(juce::Colour(84, 84, 88).withAlpha(on ? 0.50f : 0.30f));
        g.drawRoundedRectangle(bounds, radius, 0.5f);
    }

    void drawComboBox(juce::Graphics& g, int width, int height, bool isDown,
                      int, int, int, int, juce::ComboBox&) override
    {
        auto bounds = juce::Rectangle<float>(0, 0, (float)width, (float)height).reduced(1.0f);
        float radius = juce::jmin(bounds.getHeight() / 2.0f, 10.0f);

        float fillAlpha = isDown ? 0.44f : 0.36f;
        g.setColour(juce::Colour(120, 120, 128).withAlpha(fillAlpha));
        g.fillRoundedRectangle(bounds, radius);

        g.setColour(juce::Colour(84, 84, 88).withAlpha(0.30f));
        g.drawRoundedRectangle(bounds, radius, 0.5f);

        float arrowX = (float)width - 18.0f;
        float arrowY = (float)height * 0.5f;
        juce::Path arrow;
        arrow.addTriangle(arrowX - 4, arrowY - 2, arrowX + 4, arrowY - 2, arrowX, arrowY + 3);
        g.setColour(juce::Colour(0x99ebebf5));
        g.fillPath(arrow);
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider&) override
    {
        float radius = juce::jmin(width / 2.0f, height / 2.0f) - 4.0f;
        float centreX = x + width * 0.5f;
        float centreY = y + height * 0.5f;
        float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // Neutral gray glass body
        g.setColour(juce::Colour(120, 120, 128).withAlpha(0.36f));
        g.fillEllipse(centreX - radius, centreY - radius, radius * 2, radius * 2);

        g.setColour(juce::Colour(84, 84, 88).withAlpha(0.30f));
        g.drawEllipse(centreX - radius, centreY - radius, radius * 2, radius * 2, 0.5f);

        // Inner highlight
        float hlR = radius * 0.45f;
        float hlY = centreY - radius * 0.3f;
        g.setColour(juce::Colours::white.withAlpha(0.10f));
        g.fillEllipse(centreX - hlR, hlY - hlR * 0.4f, hlR * 2, hlR * 0.8f);

        // Background arc
        juce::Path bgArc;
        bgArc.addCentredArc(centreX, centreY, radius + 2, radius + 2,
                            0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(120, 120, 128).withAlpha(0.18f));
        g.strokePath(bgArc, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

        // Value arc — teal
        juce::Path arc;
        arc.addCentredArc(centreX, centreY, radius + 2, radius + 2,
                          0.0f, rotaryStartAngle, angle, true);
        g.setColour(juce::Colour(0xff50b8d0).withAlpha(0.85f));
        g.strokePath(arc, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));

        // Pointer dot
        float dotR = juce::jmax(1.5f, radius * 0.08f);
        float dotDist = radius * 0.6f;
        float px = centreX + dotDist * std::cos(angle - juce::MathConstants<float>::halfPi);
        float py = centreY + dotDist * std::sin(angle - juce::MathConstants<float>::halfPi);
        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.fillEllipse(px - dotR, py - dotR, dotR * 2, dotR * 2);
    }

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float, float,
                          const juce::Slider::SliderStyle, juce::Slider&) override
    {
        bool isHorizontal = width > height;
        float trackThickness = 2.0f;

        if (isHorizontal)
        {
            float trackY = y + height * 0.5f - trackThickness * 0.5f;
            g.setColour(juce::Colour(120, 120, 128).withAlpha(0.18f));
            g.fillRoundedRectangle((float)x, trackY, (float)width, trackThickness, 1.0f);
            g.setColour(juce::Colour(0xff50b8d0).withAlpha(0.7f));
            g.fillRoundedRectangle((float)x, trackY, sliderPos - x, trackThickness, 1.0f);
            float thumbR = 5.0f;
            g.setColour(juce::Colours::white.withAlpha(0.6f));
            g.fillEllipse(sliderPos - thumbR, trackY + trackThickness * 0.5f - thumbR, thumbR * 2, thumbR * 2);
        }
        else
        {
            float trackX = x + width * 0.5f - trackThickness * 0.5f;
            g.setColour(juce::Colour(120, 120, 128).withAlpha(0.18f));
            g.fillRoundedRectangle(trackX, (float)y, trackThickness, (float)height, 1.0f);
            g.setColour(juce::Colour(0xff50b8d0).withAlpha(0.7f));
            g.fillRoundedRectangle(trackX, sliderPos, trackThickness, (float)(y + height) - sliderPos, 1.0f);
            float thumbR = 5.0f;
            g.setColour(juce::Colours::white.withAlpha(0.6f));
            g.fillEllipse(trackX + trackThickness * 0.5f - thumbR, sliderPos - thumbR, thumbR * 2, thumbR * 2);
        }
    }
};
