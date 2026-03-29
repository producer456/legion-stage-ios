#pragma once

#include "DawLookAndFeel.h"

// Sony Walkman Professional inspired look and feel.
// Dark charcoal body, red accents, orange/amber highlights,
// industrial monospace labels, LCD-style displays.
class WalkmanLookAndFeel : public DawLookAndFeel
{
public:
    // Keep Colors struct for backwards compat with any existing references
    struct Colors {
        static constexpr uint32_t body        = 0xff2a2a2d;
        static constexpr uint32_t bodyLight   = 0xff353538;
        static constexpr uint32_t bodyDark    = 0xff1e1e20;
        static constexpr uint32_t border      = 0xff444448;
        static constexpr uint32_t borderLight = 0xff555558;
        static constexpr uint32_t textPrimary   = 0xffcccccc;
        static constexpr uint32_t textSecondary = 0xff888890;
        static constexpr uint32_t textBright    = 0xffeeeef0;
        static constexpr uint32_t red         = 0xffcc3333;
        static constexpr uint32_t redDark     = 0xff882222;
        static constexpr uint32_t amber       = 0xffddaa33;
        static constexpr uint32_t amberDark   = 0xff886600;
        static constexpr uint32_t green       = 0xff33aa55;
        static constexpr uint32_t greenDark   = 0xff226633;
        static constexpr uint32_t lcdBg       = 0xff1a2a1a;
        static constexpr uint32_t lcdText     = 0xff88cc88;
        static constexpr uint32_t lcdAmber    = 0xffccaa44;
        static constexpr uint32_t buttonFace  = 0xff3a3a3e;
        static constexpr uint32_t buttonHover = 0xff4a4a4e;
        static constexpr uint32_t buttonDown  = 0xff2a2a2e;
    };

    WalkmanLookAndFeel()
    {
        theme.body        = Colors::body;
        theme.bodyLight   = Colors::bodyLight;
        theme.bodyDark    = Colors::bodyDark;
        theme.border      = Colors::border;
        theme.borderLight = Colors::borderLight;

        theme.textPrimary   = Colors::textPrimary;
        theme.textSecondary = Colors::textSecondary;
        theme.textBright    = Colors::textBright;

        theme.red       = Colors::red;
        theme.redDark   = Colors::redDark;
        theme.amber     = Colors::amber;
        theme.amberDark = Colors::amberDark;
        theme.green     = Colors::green;
        theme.greenDark = Colors::greenDark;

        theme.lcdBg    = Colors::lcdBg;
        theme.lcdText  = Colors::lcdText;
        theme.lcdAmber = Colors::lcdAmber;

        theme.buttonFace  = Colors::buttonFace;
        theme.buttonHover = Colors::buttonHover;
        theme.buttonDown  = Colors::buttonDown;

        theme.btnStop       = 0xff444444;
        theme.btnMetronome  = 0xff444444;
        theme.btnMetronomeOn = Colors::amberDark;
        theme.btnCountIn    = 0xff444444;
        theme.btnCountInOn  = 0xff665533;
        theme.btnNewClip    = 0xff336655;
        theme.btnDeleteClip = 0xff553333;
        theme.btnDuplicate  = 0xff335555;
        theme.btnSplit      = 0xff555533;
        theme.btnQuantize   = 0xff555544;
        theme.btnEditNotes  = 0xff335566;
        theme.btnNav        = 0xff444444;
        theme.btnSave       = 0xff336644;
        theme.btnLoad       = 0xff444466;
        theme.btnUndoRedo   = 0xff555544;
        theme.btnMidi2      = 0xff444444;
        theme.btnMidi2On    = 0xff446688;
        theme.btnLoop       = 0xff444444;
        theme.btnLoopOn     = 0xff336688;
        theme.loopRegion    = 0x2244aaff;
        theme.loopBorder    = 0xff4488cc;

        theme.timelineBg         = 0xff1a1a1a;
        theme.timelineAltRow     = 0xff222222;
        theme.timelineSelectedRow = 0xff2a3a4a;
        theme.timelineGridMajor  = 0xffcccccc;
        theme.timelineGridMinor  = 0xff666666;
        theme.timelineGridFaint  = 0xff2d2d2d;
        theme.timelineGridBeat   = 0xff444444;

        theme.clipDefault   = 0xff445566;
        theme.clipRecording = 0xff883333;
        theme.clipQueued    = 0xff884400;
        theme.clipPlaying   = 0xff338844;

        theme.playhead     = 0xdd44dd66;
        theme.playheadGlow = 0x3344dd66;

        theme.accentStripe = Colors::red;

        theme.trackSelected = 0xff3a5a8a;
        theme.trackArmed    = 0xff882222;
        theme.trackMuteOn   = 0xffcc3333;
        theme.trackSoloOn   = 0xffffff00;
        theme.trackSoloText = 0xff000000;

        applyThemeColors();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WalkmanLookAndFeel)
};
