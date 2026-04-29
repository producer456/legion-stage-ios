#pragma once

#include "DawLookAndFeel.h"

// "Launchkey Dark" theme — Mini 37 MK4 in black.  Matte black chassis,
// pads still glow pastel but pop harder against the dark surface,
// teal OLED accent shared with the white sibling theme.
class LaunchkeyDarkLookAndFeel : public DawLookAndFeel
{
public:
    LaunchkeyDarkLookAndFeel()
    {
        // ── Surfaces — true black; everything else is wireframe ──
        // OLED-cyan brightness scale (~190° hue):
        //   bright   : 0xffd2e4e8   text / selected / strong UI
        //   accent   : 0xff78b0c4   active borders / accent stripe
        //   mid      : 0xff5891a3   regular outlines
        //   dim      : 0xff3a5a64   subtle dividers
        //   faint    : 0xff1a3038   barely-visible grid
        theme.body        = 0xff000000;
        theme.bodyLight   = 0xff000000;
        theme.bodyDark    = 0xff000000;
        theme.border      = 0xff5891a3;   // mid OLED-cyan
        theme.borderLight = 0xff78b0c4;   // accent OLED-cyan
        theme.wireframe   = true;

        // ── Text — all OLED-cyan, brightness conveys hierarchy ──
        theme.textPrimary   = 0xffd2e4e8;
        theme.textSecondary = 0xff78b0c4;
        theme.textBright    = 0xffe2f0f4;

        // ── Accents — collapsed to OLED-cyan brightness levels ──
        theme.red       = 0xffd2e4e8;
        theme.redDark   = 0xff1a3038;
        theme.amber     = 0xff78b0c4;
        theme.amberDark = 0xff5891a3;
        theme.green     = 0xffd2e4e8;
        theme.greenDark = 0xff1a3038;

        // ── LCD / OLED — true black with bright cool text ──
        theme.lcdBg    = 0xff050a0c;
        theme.lcdText  = 0xffd2e4e8;
        theme.lcdAmber = 0xffe2f0f4;

        // ── Buttons — dark gray cap, like the device's button caps in black ──
        theme.buttonFace  = 0xff2a2a2a;
        theme.buttonHover = 0xff333333;
        theme.buttonDown  = 0xff404040;

        theme.btnStop        = 0xff2a2a2a;
        theme.btnMetronome   = 0xff2a2a2a;
        theme.btnMetronomeOn = 0xff2a4042;
        theme.btnCountIn     = 0xff2a2a2a;
        theme.btnCountInOn   = 0xff2a4042;
        theme.btnNewClip     = 0xff2a2a2a;
        theme.btnDeleteClip  = 0xff3a1c1c;
        theme.btnDuplicate   = 0xff2a2a2a;
        theme.btnSplit       = 0xff2a2a2a;
        theme.btnQuantize    = 0xff2a2a2a;
        theme.btnEditNotes   = 0xff2a2a2a;
        theme.btnNav         = 0xff2a2a2a;
        theme.btnSave        = 0xff2a2a2a;
        theme.btnLoad        = 0xff2a2a2a;
        theme.btnUndoRedo    = 0xff2a2a2a;
        theme.btnMidi2       = 0xff2a2a2a;
        theme.btnMidi2On     = 0xff2a4042;
        theme.btnLoop        = 0xff2a2a2a;
        theme.btnLoopOn      = 0xff2a4042;
        theme.loopRegion     = 0x2878b0c4;
        theme.loopBorder     = 0xff78b0c4;

        // ── Timeline — true black, OLED-cyan grid at the dim end ──
        theme.timelineBg          = 0xff000000;
        theme.timelineAltRow      = 0xff000000;
        theme.timelineSelectedRow = 0xff1a3038;
        theme.timelineGridMajor   = 0xff5891a3;
        theme.timelineGridMinor   = 0xff3a5a64;
        theme.timelineGridFaint   = 0xff1a3038;
        theme.timelineGridBeat    = 0xff3a5a64;

        // ── Clips — OLED-cyan brightness levels for state ──
        theme.clipDefault     = 0xff3a5a64;   // dim cyan — idle
        theme.clipRecording   = 0xffd2e4e8;   // brightest cyan — recording
        theme.clipQueued      = 0xff5891a3;   // mid cyan — queued
        theme.clipPlaying     = 0xff78b0c4;   // accent cyan — playing
        theme.clipNotePreview = 0xccd2e4e8;

        // ── Playhead — bright OLED cyan needle ──
        theme.playhead     = 0xccd2e4e8;
        theme.playheadGlow = 0x30d2e4e8;

        theme.accentStripe = 0xff78b0c4;

        theme.trackSelected = 0xff1a3038;
        theme.trackArmed    = 0xffd2e4e8;     // bright cyan when armed
        theme.trackMuteOn   = 0xff78b0c4;
        theme.trackSoloOn   = 0xffd2e4e8;     // bright cyan when soloed
        theme.trackSoloText = 0xff000000;

        applyThemeColors();

        setColour(juce::ComboBox::textColourId,                   juce::Colour(0xffd2e4e8));
        setColour(juce::PopupMenu::backgroundColourId,            juce::Colour(0xff050a0c));
        setColour(juce::PopupMenu::textColourId,                  juce::Colour(0xffd2e4e8));
        setColour(juce::PopupMenu::highlightedTextColourId,       juce::Colour(0xff050a0c));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xffd2e4e8));

        setColour(juce::Slider::textBoxTextColourId,       juce::Colour(0xffd2e4e8));
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff050a0c));
        setColour(juce::Slider::textBoxOutlineColourId,    juce::Colour(0x00000000));

        setColour(juce::TextButton::buttonOnColourId,      juce::Colour(0xff1c2630));
        setColour(juce::TextButton::textColourOnId,        juce::Colour(0xffd2e4e8));
    }

    float getButtonRadius() const override { return 7.0f; }
    juce::String getUIFontName() const override { return "Plus Jakarta Sans"; }
    int getSidePanelWidth() const override { return 6; }

    // No side-panel fill — let the body's pure black show through.
    void drawSidePanels(juce::Graphics&, int, int) override {}

    void invalidateCaches() override { topBarCacheW = 0; topBarCacheH = 0; }

    // No inner-strip fill — pure black body shows through.
    void drawInnerStrip(juce::Graphics&, int, int, int, int) override {}

    // No top-bar fill — just a thin accent rule along the bottom so
    // the topbar is still visually separated from the content area.
    void drawTopBarBackground(juce::Graphics& g, int x, int y, int width, int height) override
    {
        g.setColour(juce::Colour(0xff78b0c4));
        g.fillRect(x, y + height - 1, width, 1);
    }

    mutable juce::Image topBarCache;
    mutable int topBarCacheW = 0, topBarCacheH = 0;

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour&,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        auto text = button.getButtonText();

        // Launchkey toolbar-pad button — radial "lit-from-within"
        // glow that mimics the device's LED-backed pads: bright
        // center, soft falloff, much darker edge, dark rim for the
        // pad housing.  Higher contrast than the light theme so it
        // reads as a glowing surface on the dark background.
        if (button.getProperties().contains("lkColor"))
        {
            auto base = juce::Colour((juce::uint32) (int) button.getProperties()["lkColor"]);
            if (shouldDrawButtonAsDown)             base = base.darker(0.25f);
            else if (shouldDrawButtonAsHighlighted) base = base.brighter(0.15f);
            const auto core = base.brighter(0.40f);
            const auto edge = base.darker(0.40f);
            const float cx = bounds.getCentreX();
            const float cy = bounds.getY() + bounds.getHeight() * 0.42f;
            const float r  = juce::jmax(bounds.getWidth(), bounds.getHeight()) * 0.55f;
            juce::ColourGradient grad(core, cx, cy, edge, cx + r, cy, true);
            grad.addColour(0.55, base);
            g.setGradientFill(grad);
            g.fillRoundedRectangle(bounds, 4.0f);
            g.setColour(base.darker(0.6f));
            g.drawRoundedRectangle(bounds, 4.0f, 0.7f);
            return;
        }

        const bool useOled = isOledAnimatedButton(text) || text == "REC" || text == "PANIC"
                          || text == "M2" || text == "..." || text == "PHN" || text == "E";

        if (useOled)
        {
            g.setColour(juce::Colour(0xff333333));
            g.fillRoundedRectangle(bounds, 5.0f);
            auto inset = bounds.reduced(1.5f);
            g.setColour(juce::Colour(0xff111111));
            g.drawRoundedRectangle(inset, 4.0f, 1.0f);

            auto screen = bounds.reduced(2.5f);
            g.setColour(juce::Colour(theme.lcdBg));
            g.fillRoundedRectangle(screen, 3.0f);

            if (shouldDrawButtonAsDown)        g.setColour(juce::Colour(0xff78b0c4).withAlpha(0.5f));
            else if (shouldDrawButtonAsHighlighted) g.setColour(juce::Colour(0xff2a3a3c));
            else                                g.setColour(juce::Colour(0xff1a2628));
            g.drawRoundedRectangle(screen, 3.0f, 0.5f);
        }
        else
        {
            // Outlined wireframe — OLED-cyan border, no fill.  Tier
            // by brightness: dim → mid → accent → bright with state.
            const float radius = getButtonRadius();
            if (shouldDrawButtonAsDown)
            {
                g.setColour(juce::Colour(0x2078b0c4));
                g.fillRoundedRectangle(bounds, radius);
                g.setColour(juce::Colour(0xffd2e4e8));
                g.drawRoundedRectangle(bounds, radius, 1.2f);
            }
            else if (shouldDrawButtonAsHighlighted)
            {
                g.setColour(juce::Colour(0xff78b0c4));
                g.drawRoundedRectangle(bounds, radius, 1.2f);
            }
            else
            {
                g.setColour(juce::Colour(0xff5891a3));
                g.drawRoundedRectangle(bounds, radius, 0.9f);
            }
        }
    }

    bool drawOledButtonArt(juce::Image& oled, const juce::String& text,
                           bool on, float t, juce::Colour, juce::Colour) const override
    {
        juce::Colour bright(0xffd2e4e8);
        juce::Colour dim(0xff4a5a64);

        if (text == "REC")
        {
            juce::Colour col = on ? juce::Colour(0xffe04848) : dim;
            float pulse = on ? (0.5f + 0.5f * std::sin(t * 8.0f)) : 1.0f;
            int icx = OLED_W / 2;
            int icy = OLED_H / 2;
            oledCircle(oled, icx, icy, 3, col.withAlpha(pulse), true);
            if (on)
            {
                int ringR = 4 + (static_cast<int>(t * 6.0f) % 3);
                oledCircle(oled, icx, icy, ringR, juce::Colour(0xffe04848).withAlpha(0.3f));
            }
            return true;
        }

        return DawLookAndFeel::drawOledButtonArt(oled, text, on, t, bright, dim);
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool) override
    {
        auto text = button.getButtonText();
        bool on = button.getToggleState();

        juce::Colour bright(0xffd2e4e8);
        juce::Colour dim(0xff4a5a64);

        // Launchkey toolbar-pad buttons get white-on-color text.
        if (button.getProperties().contains("lkColor"))
        {
            auto dispBounds = button.getLocalBounds().toFloat().reduced(2.0f);
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(getUIFontName(),
                      juce::jmin(12.0f, dispBounds.getHeight() * 0.5f), juce::Font::bold));
            g.drawText(formatButtonText(text), button.getLocalBounds().reduced(2),
                       juce::Justification::centred);
            return;
        }

        if (isOledAnimatedButton(text) || text == "REC" || text == "PANIC")
        {
            auto dispBounds = button.getLocalBounds().toFloat().reduced(2.0f);
            float t = static_cast<float>(juce::Time::getMillisecondCounterHiRes() * 0.001);
            juce::Image oled(juce::Image::ARGB, OLED_W, OLED_H, true);
            if (drawOledButtonArt(oled, text, on, t, bright, dim))
            {
                drawOledImage(g, oled, dispBounds);
                return;
            }
        }

        const bool useOled = isOledAnimatedButton(text) || text == "REC" || text == "PANIC"
                          || text == "M2" || text == "..." || text == "PHN" || text == "E";
        auto dispBounds = button.getLocalBounds().toFloat().reduced(2.0f);
        if (useOled)
            g.setColour(on ? bright : bright.withAlpha(0.7f));
        else
            g.setColour(on ? juce::Colour(0xffd2e4e8) : juce::Colour(0xff78b0c4));
        g.setFont(juce::Font(getUIFontName(),
                  juce::jmin(12.0f, dispBounds.getHeight() * 0.5f), juce::Font::bold));
        g.drawText(formatButtonText(text), button.getLocalBounds().reduced(2),
                   juce::Justification::centred);
    }

    void drawComboBox(juce::Graphics& g, int width, int height, bool,
                      int, int, int, int, juce::ComboBox&) override
    {
        auto bounds = juce::Rectangle<float>(0, 0, (float) width, (float) height);
        g.setColour(juce::Colour(0xff333333));
        g.fillRoundedRectangle(bounds, 5.0f);
        auto inset = bounds.reduced(1.5f);
        g.setColour(juce::Colour(0xff111111));
        g.drawRoundedRectangle(inset, 4.0f, 1.0f);
        auto screen = bounds.reduced(2.5f);
        g.setColour(juce::Colour(0xff050a0c));
        g.fillRoundedRectangle(screen, 3.0f);
        g.setColour(juce::Colour(0xff1a2024));
        g.drawRoundedRectangle(screen, 3.0f, 0.5f);

        auto arrowZone = screen.removeFromRight(18.0f).reduced(4.0f);
        juce::Path arrow;
        arrow.addTriangle(arrowZone.getX(),       arrowZone.getCentreY() - 3,
                          arrowZone.getRight(),   arrowZone.getCentreY() - 3,
                          arrowZone.getCentreX(), arrowZone.getCentreY() + 3);
        g.setColour(juce::Colour(0xffd2e4e8));
        g.fillPath(arrow);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LaunchkeyDarkLookAndFeel)
};
