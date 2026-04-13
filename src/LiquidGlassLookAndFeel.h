#pragma once

#include "DawLookAndFeel.h"
#include "DeviceMotion.h"

// "Liquid Glass" — Apple-inspired glassmorphism for Legion Stage.
//
// Approximates iOS 26's Liquid Glass within JUCE's rendering:
// - Frosted translucent surfaces with depth
// - Capsule/pill button shapes (Apple's default glass shape)
// - Specular edge highlights (bright top, soft bottom)
// - Size-adaptive opacity (larger = more opaque)
// - iOS system colors: blue 007AFF, green 30D158, red FF3B30
// - Inner glow on active/pressed states
// - Gradient tint for depth perception
//
class LiquidGlassLookAndFeel : public DawLookAndFeel
{
public:
    LiquidGlassLookAndFeel()
    {
        // Start accelerometer for tilt-reactive glass highlights
        DeviceMotion::getInstance().start();
        // ── Body — Apple dark mode system backgrounds ──
        theme.body        = 0xff000000;  // systemBackgroundColor (dark)
        theme.bodyLight   = 0xff1c1c1e;  // secondarySystemBackgroundColor (dark)
        theme.bodyDark    = 0xff000000;  // systemBackgroundColor (dark)
        theme.border      = 0x3d8e8e93;  // separatorColor (dark) — 24% gray
        theme.borderLight = 0x5c8e8e93;  // opaqueSeparatorColor (dark)

        // ── Text — Apple label hierarchy ──
        theme.textPrimary   = 0xffffffff;  // labelColor (dark)
        theme.textSecondary = 0x99ebebf5;  // secondaryLabelColor (dark)
        theme.textBright    = 0xffffffff;

        // ── Accents — Apple system colors (dark mode variants) ──
        theme.amber     = 0xff0a84ff;  // systemBlue (dark)
        theme.amberDark = 0xff0064d2;
        theme.green     = 0xff30d158;  // systemGreen (dark)
        theme.greenDark = 0xff0a3018;
        theme.red       = 0xffff453a;  // systemRed (dark)
        theme.redDark   = 0xff3a1010;

        // ── LCD — OLED with system blue ──
        theme.lcdBg    = 0xff000000;
        theme.lcdText  = 0xff0a84ff;   // systemBlue (dark)
        theme.lcdAmber = 0xff64d2ff;   // systemCyan (dark)

        // ── Buttons — Apple fill colors (translucent, designed for layering) ──
        theme.buttonFace  = 0x1e767680;  // tertiarySystemFillColor — for buttons
        theme.buttonHover = 0x28787880;  // secondarySystemFillColor
        theme.buttonDown  = 0x330a84ff;  // blue tint on press

        // All buttons use tertiarySystemFillColor
        theme.btnStop       = 0x1e767680;
        theme.btnMetronome  = 0x1e767680;
        theme.btnMetronomeOn = 0x330a84ff;  // blue tint when on
        theme.btnCountIn    = 0x1e767680;
        theme.btnCountInOn  = 0x330a84ff;
        theme.btnLoop       = 0x1e767680;
        theme.btnLoopOn     = 0x330a84ff;
        theme.btnNewClip    = 0x1e767680;
        theme.btnDuplicate  = 0x1e767680;
        theme.btnSplit      = 0x1e767680;
        theme.btnQuantize   = 0x1e767680;
        theme.btnEditNotes  = 0x1e767680;
        theme.btnNav        = 0x1e767680;
        theme.btnSave       = 0x1e767680;
        theme.btnLoad       = 0x1e767680;
        theme.btnUndoRedo   = 0x1e767680;
        theme.btnMidi2      = 0x1e767680;
        theme.btnMidi2On    = 0x330a84ff;
        theme.btnDeleteClip = 0x28ff453a;  // systemRed tint

        theme.loopRegion = 0x180a84ff;  // systemBlue translucent
        theme.loopBorder = 0xff0a84ff;

        // ── Timeline — true black with subtle grid ──
        theme.timelineBg         = 0xff000000;
        theme.timelineAltRow     = 0xff0c0c0e;
        theme.timelineSelectedRow = 0x280a84ff;  // systemBlue selection
        theme.timelineGridMajor  = 0x3d8e8e93;   // separatorColor
        theme.timelineGridMinor  = 0x1e8e8e93;
        theme.timelineGridFaint  = 0x0c8e8e93;
        theme.timelineGridBeat   = 0x288e8e93;

        // ── Clips — system color tinted glass ──
        theme.clipDefault   = 0x400a84ff;  // systemBlue
        theme.clipRecording = 0x50ff453a;  // systemRed
        theme.clipQueued    = 0x40ff9f0a;  // systemOrange
        theme.clipPlaying   = 0x4030d158;  // systemGreen
        theme.clipNotePreview = 0xb0ffffff;

        theme.playhead     = 0xff0a84ff;   // systemBlue
        theme.playheadGlow = 0x280a84ff;
        theme.accentStripe = 0xff0a84ff;

        theme.trackSelected = 0x280a84ff;
        theme.trackArmed    = 0x28ff453a;
        theme.trackMuteOn   = 0xffff453a;   // systemRed
        theme.trackSoloOn   = 0xffffd60a;   // systemYellow
        theme.trackSoloText = 0xff000000;

        applyThemeColors();

        // Fix #1: toggled-on button text must be bright, not bodyDark
        setColour(juce::TextButton::textColourOnId, juce::Colour(0xffffffff));

        setColour(juce::ComboBox::backgroundColourId,   juce::Colour(0x1e767680));  // tertiarySystemFill
        setColour(juce::ComboBox::textColourId,          juce::Colour(0xffffffff));  // labelColor
        setColour(juce::ComboBox::outlineColourId,       juce::Colour(0x3d8e8e93));  // separatorColor
        setColour(juce::PopupMenu::backgroundColourId,   juce::Colour(0xff1c1c1e));  // secondarySystemBackground
        setColour(juce::PopupMenu::textColourId,         juce::Colour(0xffffffff));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff0a84ff));  // systemBlue
        setColour(juce::PopupMenu::highlightedTextColourId,       juce::Colour(0xffffffff));
        setColour(juce::Slider::thumbColourId,           juce::Colour(0xff0a84ff));
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff0a84ff));
        setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0x3d8e8e93));
    }

    float getButtonRadius() const override { return 12.0f; }

    // Fix #9: Use system font with fallback
    juce::String getUIFontName() const override { return "SF Pro Text"; }
    juce::String getDisplayFontName() const override { return "SF Pro Display"; }

    // Fix #5: Glass linear slider
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          const juce::Slider::SliderStyle, juce::Slider& slider) override
    {
        bool isHorizontal = width > height;
        float trackThickness = 2.0f;

        if (isHorizontal)
        {
            float trackY = y + height * 0.5f - trackThickness * 0.5f;
            // Background track
            g.setColour(juce::Colours::white.withAlpha(0.06f));
            g.fillRoundedRectangle((float)x, trackY, (float)width, trackThickness, 1.0f);
            // Value fill
            g.setColour(juce::Colour(0xff0a84ff).withAlpha(0.6f));
            g.fillRoundedRectangle((float)x, trackY, sliderPos - x, trackThickness, 1.0f);
            // Thumb
            float thumbR = 5.0f;
            g.setColour(juce::Colours::white.withAlpha(0.5f));
            g.fillEllipse(sliderPos - thumbR, trackY + trackThickness * 0.5f - thumbR, thumbR * 2, thumbR * 2);
        }
        else
        {
            float trackX = x + width * 0.5f - trackThickness * 0.5f;
            g.setColour(juce::Colours::white.withAlpha(0.06f));
            g.fillRoundedRectangle(trackX, (float)y, trackThickness, (float)height, 1.0f);
            g.setColour(juce::Colour(0xff0a84ff).withAlpha(0.6f));
            g.fillRoundedRectangle(trackX, sliderPos, trackThickness, (float)(y + height) - sliderPos, 1.0f);
            float thumbR = 5.0f;
            g.setColour(juce::Colours::white.withAlpha(0.5f));
            g.fillEllipse(trackX + trackThickness * 0.5f - thumbR, sliderPos - thumbR, thumbR * 2, thumbR * 2);
        }
    }

    // ── Glass button rendering (tilt-reactive) ──
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool isHighlighted, bool isDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
        float radius = juce::jmin(bounds.getHeight() / 2.0f, 12.0f); // capsule

        auto baseColour = backgroundColour;
        bool on = button.getToggleState();
        auto tilt = DeviceMotion::getInstance().getTilt();

        // ── Glass fill — enough opacity for OLED art readability ──
        {
            float alpha = isDown ? 0.22f : (isHighlighted ? 0.18f : 0.14f);
            if (on) alpha = 0.20f;
            g.setColour(juce::Colours::white.withAlpha(alpha));
            g.fillRoundedRectangle(bounds, radius);
        }

        // ── Subtle blue tint on active/pressed ──
        if (isDown || on)
        {
            float blueAlpha = isDown ? 0.15f : 0.08f;
            g.setColour(juce::Colour(0xff0a84ff).withAlpha(blueAlpha));
            g.fillRoundedRectangle(bounds, radius);
        }

        // ── Tilt-reactive specular — very subtle ──
        if (!isDown)
        {
            float hlOffsetX = tilt.x * bounds.getWidth() * 0.1f;
            float hlW = bounds.getWidth() * 0.35f;
            float hlX = bounds.getCentreX() + hlOffsetX - hlW * 0.5f;
            g.setColour(juce::Colours::white.withAlpha(0.08f));
            g.fillRoundedRectangle(hlX, bounds.getY() + 0.5f, hlW, 1.0f, 0.5f);
        }

        // ── Hairline border ──
        g.setColour(juce::Colours::white.withAlpha(on ? 0.18f : 0.08f));
        g.drawRoundedRectangle(bounds, radius, 0.5f);
    }

    // ── Glass combo box ──
    void drawComboBox(juce::Graphics& g, int width, int height, bool isDown,
                      int, int, int, int, juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<float>(0, 0, (float)width, (float)height).reduced(1.0f);
        float radius = juce::jmin(bounds.getHeight() / 2.0f, 10.0f);

        // Minimal glass fill
        g.setColour(juce::Colours::white.withAlpha(isDown ? 0.14f : 0.08f));
        g.fillRoundedRectangle(bounds, radius);

        // Hairline border
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.drawRoundedRectangle(bounds, radius, 0.5f);

        // Arrow
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
                          juce::Slider& slider) override
    {
        float radius = juce::jmin(width / 2.0f, height / 2.0f) - 4.0f;
        float centreX = x + width * 0.5f;
        float centreY = y + height * 0.5f;
        float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // Minimal glass knob body
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.fillEllipse(centreX - radius, centreY - radius, radius * 2, radius * 2);

        // Hairline border
        g.setColour(juce::Colours::white.withAlpha(0.10f));
        g.drawEllipse(centreX - radius, centreY - radius, radius * 2, radius * 2, 0.5f);

        // Tilt-reactive specular — tiny, subtle
        {
            auto tilt = DeviceMotion::getInstance().getTilt();
            float hlR = radius * 0.3f;
            float hlX = centreX + tilt.x * radius * 0.15f;
            float hlY = centreY - radius * 0.35f + tilt.y * radius * 0.1f;
            g.setColour(juce::Colours::white.withAlpha(0.10f));
            g.fillEllipse(hlX - hlR, hlY - hlR * 0.3f, hlR * 2, hlR * 0.6f);
        }

        // Value arc — thin, clean
        {
            float arcThickness = radius < 16.0f ? 1.5f : 2.0f;
            // Background arc (track)
            juce::Path bgArc;
            bgArc.addCentredArc(centreX, centreY, radius + 2, radius + 2,
                                0.0f, rotaryStartAngle, rotaryEndAngle, true);
            g.setColour(juce::Colours::white.withAlpha(0.06f));
            g.strokePath(bgArc, juce::PathStrokeType(arcThickness, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
            // Value arc
            juce::Path arc;
            arc.addCentredArc(centreX, centreY, radius + 2, radius + 2,
                              0.0f, rotaryStartAngle, angle, true);
            g.setColour(juce::Colour(0xff0a84ff).withAlpha(0.7f));
            g.strokePath(arc, juce::PathStrokeType(arcThickness, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
        }

        // Small pointer dot
        {
            float dotR = juce::jmax(1.5f, radius * 0.08f);
            float dotDist = radius * 0.6f;
            float px = centreX + dotDist * std::cos(angle - juce::MathConstants<float>::halfPi);
            float py = centreY + dotDist * std::sin(angle - juce::MathConstants<float>::halfPi);
            g.setColour(juce::Colours::white.withAlpha(0.6f));
            g.fillEllipse(px - dotR, py - dotR, dotR * 2, dotR * 2);
        }
    }
};
