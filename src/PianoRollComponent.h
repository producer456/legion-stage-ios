#pragma once

#include <JuceHeader.h>
#include <set>
#include "MidiClip.h"
#include "SequencerEngine.h"
#include "DawLookAndFeel.h"

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
    void mouseMagnify(const juce::MouseEvent& e, float scaleFactor) override;
    bool keyPressed(const juce::KeyPress& key) override;
    void timerCallback() override;
    void setGridResolution(double res) { gridResolution = res; }

private:
    MidiClip& clip;
    SequencerEngine& engine;
    bool followPlayhead = true;

    // Toolbar
    static constexpr int toolbarHeight = 20;
    juce::ComboBox gridSelector;
    juce::TextButton snapButton { "SNAP" };
    bool snapEnabled = true;

    // View state
    double scrollX = 0.0;     // beats offset
    int scrollY = 40;         // note offset (lowest visible note)
    double pixelsPerBeat = 80.0;
    int noteHeight = 14;
    int pianoKeyWidth = 40;

    // Note range
    static constexpr int MIN_NOTE = 0;
    static constexpr int MAX_NOTE = 127;
    int visibleNotes() const { return ((getHeight() - velLaneHeight - toolbarHeight) / noteHeight) + 1; }

    // Interaction state
    enum DragMode { None, MoveNote, ResizeNote, SelectArea, AdjustVelocity, MarqueeSelect, VelocityLaneDrag };
    DragMode dragMode = None;

    struct NoteEvent {
        int noteNumber = 60;
        double startBeat = 0.0;
        double lengthBeats = 0.25;
        int velocity = 100;
    };

    std::set<int> selectedNotes;   // indices into noteEvents
    int primarySelectedNote = -1;  // the note being directly dragged
    double dragStartBeat = 0.0;
    int dragStartNote = 0;
    double noteOrigStart = 0.0;
    int noteOrigNote = 0;
    double noteOrigLength = 0.0;

    // Multi-select: original positions for all selected notes during move
    struct NoteOrigState { double startBeat; int noteNumber; double lengthBeats; int velocity; };
    std::map<int, NoteOrigState> selectedOrigStates;

    // Marquee selection
    juce::Point<float> marqueeStart;
    juce::Rectangle<float> marqueeRect;

    // Velocity lane
    static constexpr int velLaneHeight = 60;
    int velLaneTop() const { return getHeight() - velLaneHeight; }
    int velocityLaneHitTest(float x) const; // returns note index under x in vel lane

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
    void drawToolbar(juce::Graphics& g);
    void drawPianoKeys(juce::Graphics& g);
    void drawGrid(juce::Graphics& g);
    void drawNotes(juce::Graphics& g);
    void drawPlayhead(juce::Graphics& g);
    void drawVelocityLane(juce::Graphics& g);
    void drawMarquee(juce::Graphics& g);

    double gridResolution = 0.25;

    // Touch scroll state
    bool touchScrolling = false;
    float lastTouchY = 0.0f;
    float lastTouchX = 0.0f;

    static bool isBlackKey(int note);
    static juce::String noteName(int note);

public:
    std::function<void(int note, bool on)> onNotePreview;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PianoRollComponent)
};

class PianoRollWindow : public juce::DocumentWindow
{
public:
    PianoRollWindow(const juce::String& name, MidiClip& clip, SequencerEngine& engine,
                    juce::LookAndFeel* lf = nullptr)
        : DocumentWindow(name,
                         [lf]() -> juce::Colour {
                             if (auto* tc = dynamic_cast<DawLookAndFeel*>(lf))
                                 return juce::Colour(tc->getTheme().body);
                             return juce::Colour(0xff222222);
                         }(),
                         DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(new PianoRollComponent(clip, engine), false);
        setSize(800, 500);
        setResizable(true, true);
        centreWithSize(800, 500);
        setVisible(true);
    }

    void closeButtonPressed() override
    {
        setVisible(false);
        juce::MessageManager::callAsync([this] { delete this; });
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PianoRollWindow)
};
