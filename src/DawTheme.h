#pragma once

#include <cstdint>

// Common color palette shared by all themes.
// Every LookAndFeel exposes one of these so MainComponent, TimelineComponent,
// etc. can paint with theme-aware colors without knowing which theme is active.
struct DawTheme
{
    // ── Surface colors ──
    uint32_t body;          // main background
    uint32_t bodyLight;     // lighter panel (top bar, bottom bar)
    uint32_t bodyDark;      // recessed / toolbar area
    uint32_t border;        // subtle dividers
    uint32_t borderLight;   // raised edges

    // ── Text ──
    uint32_t textPrimary;
    uint32_t textSecondary;
    uint32_t textBright;

    // ── Accent colors ──
    uint32_t red;
    uint32_t redDark;
    uint32_t amber;
    uint32_t amberDark;
    uint32_t green;
    uint32_t greenDark;

    // ── LCD / display ──
    uint32_t lcdBg;
    uint32_t lcdText;
    uint32_t lcdAmber;

    // ── Buttons ──
    uint32_t buttonFace;
    uint32_t buttonHover;
    uint32_t buttonDown;

    // ── Semantic buttons ──
    uint32_t btnStop;
    uint32_t btnMetronome;
    uint32_t btnMetronomeOn;
    uint32_t btnCountIn;
    uint32_t btnCountInOn;
    uint32_t btnNewClip;
    uint32_t btnDeleteClip;
    uint32_t btnDuplicate;
    uint32_t btnSplit;
    uint32_t btnQuantize;
    uint32_t btnEditNotes;
    uint32_t btnNav;         // zoom, scroll, refresh
    uint32_t btnSave;
    uint32_t btnLoad;
    uint32_t btnUndoRedo;
    uint32_t btnMidi2;
    uint32_t btnMidi2On;
    uint32_t btnLoop;
    uint32_t btnLoopOn;

    // ── Loop region ──
    uint32_t loopRegion;      // semi-transparent overlay
    uint32_t loopBorder;      // loop region border

    // ── Timeline ──
    uint32_t timelineBg;
    uint32_t timelineAltRow;
    uint32_t timelineSelectedRow;
    uint32_t timelineGridMajor;
    uint32_t timelineGridMinor;
    uint32_t timelineGridFaint;
    uint32_t timelineGridBeat;

    // ── Clips ──
    uint32_t clipDefault;
    uint32_t clipRecording;
    uint32_t clipQueued;
    uint32_t clipPlaying;

    // ── Playhead ──
    uint32_t playhead;
    uint32_t playheadGlow;

    // ── Accent stripe (top of window) ──
    uint32_t accentStripe;

    // ── Track controls ──
    uint32_t trackSelected;
    uint32_t trackArmed;
    uint32_t trackMuteOn;
    uint32_t trackSoloOn;
    uint32_t trackSoloText;
};
