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
        // ── Body — pure black base ──
        theme.body        = 0xff000000;
        theme.bodyLight   = 0xff1c1c1e;
        theme.bodyDark    = 0xff000000;
        // Apple vibrancy: borders/separators are white at low alpha
        theme.border      = 0x1effffff;  // white 12%
        theme.borderLight = 0x30ffffff;  // white 19%

        // ── Text — white vibrancy hierarchy (alpha only, no color) ──
        theme.textPrimary   = 0xffffffff;  // labelColor — full white
        theme.textSecondary = 0x99ffffff;  // secondaryLabel — white 60%
        theme.textBright    = 0xffffffff;

        // ── Accent — ice blue only for active/interactive states ──
        theme.amber     = 0xffc0dfff;  // pale ice blue
        theme.amberDark = 0xff6aa0d8;
        theme.green     = 0xff30d158;
        theme.greenDark = 0xff0a3018;
        theme.red       = 0xffff453a;
        theme.redDark   = 0xff3a1010;

        // ── LCD — white vibrancy readout ──
        theme.lcdBg    = 0xff000000;
        theme.lcdText  = 0xddffffff;   // white 87% (primary label vibrancy)
        theme.lcdAmber = 0xffc0dfff;   // ice blue for active values

        // ── Buttons — Apple fill hierarchy (white at graded alphas) ──
        //   thin/small = systemFill (white ~20%)
        //   medium = secondaryFill (white ~16%)
        //   large = tertiaryFill (white ~12%)
        theme.buttonFace  = 0x1effffff;  // tertiaryFill — white 12%
        theme.buttonHover = 0x28ffffff;  // secondaryFill — white 16%
        theme.buttonDown  = 0x33ffffff;  // systemFill — white 20%

        // All buttons: white fill at tertiaryFill level
        theme.btnStop       = 0x1effffff;
        theme.btnMetronome  = 0x1effffff;
        theme.btnMetronomeOn = 0x33ffffff;  // brighter fill when on
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
        theme.btnDeleteClip = 0x1effffff;  // same fill, red shows in text only

        // ── Loop — ice blue accent (interactive element) ──
        theme.loopRegion = 0x18c0dfff;
        theme.loopBorder = 0xffc0dfff;

        // ── Timeline — pure black, white-alpha grid (vibrancy) ──
        theme.timelineBg         = 0xff000000;
        theme.timelineAltRow     = 0x08ffffff;  // white 3% alternating
        theme.timelineSelectedRow = 0x1effffff;  // white 12% selection
        theme.timelineGridMajor  = 0x1effffff;   // white 12%
        theme.timelineGridMinor  = 0x0fffffff;   // white 6%
        theme.timelineGridFaint  = 0x08ffffff;   // white 3%
        theme.timelineGridBeat   = 0x14ffffff;   // white 8%

        // ── Clips — white glass with color only on state ──
        theme.clipDefault   = 0x28ffffff;  // white glass
        theme.clipRecording = 0x40ff453a;  // red only when recording
        theme.clipQueued    = 0x30ffffff;  // slightly brighter white
        theme.clipPlaying   = 0x30ffffff;  // white, green border handled elsewhere
        theme.clipNotePreview = 0xb0ffffff;

        // ── Playhead — ice blue (active/interactive) ──
        theme.playhead     = 0xffc0dfff;
        theme.playheadGlow = 0x20c0dfff;
        theme.accentStripe = 0xffc0dfff;

        // ── Track controls — white vibrancy fills ──
        theme.trackSelected = 0x1effffff;  // white 12%
        theme.trackArmed    = 0x28ff453a;  // red accent for armed
        theme.trackMuteOn   = 0xffff453a;  // red stays colored (semantic)
        theme.trackSoloOn   = 0xffffd60a;  // yellow stays colored (semantic)
        theme.trackSoloText = 0xff000000;

        applyThemeColors();

        // Fix #1: toggled-on button text must be bright, not bodyDark
        setColour(juce::TextButton::textColourOnId, juce::Colour(0xffffffff));

        // Combos — white vibrancy fill
        setColour(juce::ComboBox::backgroundColourId,   juce::Colour(0x1effffff));
        setColour(juce::ComboBox::textColourId,          juce::Colour(0xffffffff));
        setColour(juce::ComboBox::outlineColourId,       juce::Colour(0x1effffff));
        // Popup — slightly elevated surface
        setColour(juce::PopupMenu::backgroundColourId,   juce::Colour(0xff1c1c1e));
        setColour(juce::PopupMenu::textColourId,         juce::Colour(0xffffffff));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0x40ffffff)); // white 25%
        setColour(juce::PopupMenu::highlightedTextColourId,       juce::Colour(0xffffffff));
        // Sliders — ice blue accent for interactive
        setColour(juce::Slider::thumbColourId,           juce::Colour(0xffc0dfff));
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffc0dfff));
        setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0x1effffff));
    }

    float getButtonRadius() const override { return 12.0f; }

    // Skip OLED pixel art — render clean text/icons for glass theme
    bool drawOledButtonArt(juce::Image&, const juce::String&,
                           bool, float, juce::Colour, juce::Colour) const override
    {
        return false; // fall through to text rendering
    }

    // Override button text for glass — clean, bright, readable
    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool, bool) override
    {
        auto text = button.getButtonText();
        bool on = button.getToggleState();

        auto font = juce::Font(getUIFontName(),
                               juce::jmin(12.0f, button.getHeight() * 0.45f),
                               juce::Font::bold);
        g.setFont(font);

        juce::Colour textCol = on
            ? juce::Colour(0xffc0dfff)   // ice blue when on
            : juce::Colours::white.withAlpha(0.7f);  // dimmed white when off
        g.setColour(textCol);

        g.drawText(text.toUpperCase(),
                   button.getLocalBounds().reduced(2, 1),
                   juce::Justification::centred);
    }

    juce::String getUIFontName() const override { return "DIN Alternate"; }
    juce::String getDisplayFontName() const override { return "DIN Alternate"; }

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
            g.setColour(juce::Colour(0xffc0dfff).withAlpha(0.6f));
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
            g.setColour(juce::Colour(0xffc0dfff).withAlpha(0.6f));
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

        // OLED-animated buttons get solid black background for crisp pixel art
        bool isOled = isOledAnimatedButton(button.getButtonText());
        if (isOled)
        {
            g.setColour(juce::Colours::black);
            g.fillRoundedRectangle(bounds, radius);
            // Subtle border
            g.setColour(juce::Colours::white.withAlpha(on ? 0.15f : 0.06f));
            g.drawRoundedRectangle(bounds, radius, 0.5f);
            return;
        }

        // ── Glass fill — enough opacity for readability ──
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
            g.setColour(juce::Colour(0xffc0dfff).withAlpha(blueAlpha));
            g.fillRoundedRectangle(bounds, radius);
        }

        // ── Tilt-reactive specular ──
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

        // Tilt-reactive specular crescent
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
            g.setColour(juce::Colour(0xffc0dfff).withAlpha(0.7f));
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
