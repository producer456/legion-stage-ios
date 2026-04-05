#pragma once

#include <JuceHeader.h>
#include "MidiClip.h"
#include "SequencerEngine.h"

class PianoRollComponent : public juce::Component, public juce::Timer
{
public:
    PianoRollComponent(MidiClip& clip, SequencerEngine& engine);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;
    void timerCallback() override;

private:
    MidiClip& clip;
    SequencerEngine& engine;
    bool followPlayhead = true;

    // View state
    double scrollX = 0.0;     // beats offset
    int scrollY = 40;         // note offset (lowest visible note)
    double pixelsPerBeat = 80.0;
    int noteHeight = 14;
    int pianoKeyWidth = 40;

    // Note range
    static constexpr int MIN_NOTE = 0;
    static constexpr int MAX_NOTE = 127;
    int visibleNotes() const { return (getHeight() / noteHeight) + 1; }

    // Interaction state
    enum DragMode { None, MoveNote, ResizeNote, SelectArea, AdjustVelocity };
    DragMode dragMode = None;

    struct NoteEvent {
        int noteNumber = 60;
        double startBeat = 0.0;
        double lengthBeats = 0.25;
        int velocity = 100;
    };

    int selectedNoteIndex = -1;   // index into noteEvents
    double dragStartBeat = 0.0;
    int dragStartNote = 0;
    double noteOrigStart = 0.0;
    int noteOrigNote = 0;
    double noteOrigLength = 0.0;

    // Cached note list (rebuilt from clip.events)
    juce::Array<NoteEvent> noteEvents;
    void rebuildNoteList();
    void applyNoteListToClip();

    // Coordinate conversion
    double xToBeat(float x) const;
    float beatToX(double beat) const;
    int yToNote(float y) const;
    float noteToY(int note) const;
    juce::Rectangle<float> getNoteRect(const NoteEvent& n) const;

    // Hit testing
    int hitTestNote(float x, float y) const;
    bool isOnNoteRightEdge(float x, const NoteEvent& n) const;

    // Drawing
    void drawPianoKeys(juce::Graphics& g);
    void drawGrid(juce::Graphics& g);
    void drawNotes(juce::Graphics& g);
    void drawPlayhead(juce::Graphics& g);

    static bool isBlackKey(int note);
    static juce::String noteName(int note);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PianoRollComponent)
};

class PianoRollWindow : public juce::DocumentWindow
{
public:
    PianoRollWindow(const juce::String& name, MidiClip& clip, SequencerEngine& engine)
        : DocumentWindow(name, juce::Colour(0xff222222), DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(new PianoRollComponent(clip, engine), false);
        setSize(800, 500);
        setResizable(true, true);
        centreWithSize(800, 500);
        setVisible(true);
    }

    void closeButtonPressed() override { delete this; }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PianoRollWindow)
};
