#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <set>
#include "PluginHost.h"
#include "PianoRollComponent.h"
#include "TimelineComponent.h"
#include "Midi2Handler.h"
#include "ThemeManager.h"
#include "AUScanner.h"
#include "SpectrumComponent.h"
#include "LissajousComponent.h"
#include "WaveTerrainComponent.h"
#include "ShaderToyComponent.h"
#include "AnalyzerComponent.h"
#include "GeissComponent.h"
#include "ProjectMComponent.h"
#include "TouchPianoComponent.h"
#include "MixerComponent.h"
#include "UpdateDialog.h"
#include "ChordDetector.h"
#include "ArrangerMinimapComponent.h"
#include "HeartbeatComponent.h"
#include "BioResonanceComponent.h"
#include "MetalCausticRenderer.h"
#include "FluidSimComponent.h"
#include "RayMarchComponent.h"
#include "AudioExporter.h"
#include "SessionViewComponent.h"
#include "LaunchkeyMK4Controller.h"

class PluginEditorWindow : public juce::DocumentWindow, public juce::ComponentListener
{
public:
    PluginEditorWindow(const juce::String& name, juce::AudioProcessorEditor* editor,
                       std::function<void()> onClose)
        : DocumentWindow(name, juce::Colours::darkgrey, DocumentWindow::closeButton),
          closeCallback(std::move(onClose)), pluginEditor(editor)
    {
        setUsingNativeTitleBar(true);

        // Enforce minimum usable size for AUv3 plugins that report tiny initial sizes
        int w = juce::jmax(400, editor->getWidth());
        int h = juce::jmax(300, editor->getHeight());
        editor->setSize(w, h);

        setContentNonOwned(editor, true);
        setResizable(true, true);
        centreWithSize(w, h);
        setVisible(true);

        // Listen for editor resizes (AUv3 plugins may resize after async loading)
        editor->addComponentListener(this);
    }

    ~PluginEditorWindow() override
    {
        if (pluginEditor) pluginEditor->removeComponentListener(this);
    }

    void closeButtonPressed() override { if (closeCallback) closeCallback(); }

    void componentMovedOrResized(juce::Component& comp, bool, bool wasResized) override
    {
        if (wasResized && &comp == pluginEditor && pluginEditor != nullptr)
        {
            int w = juce::jmax(400, pluginEditor->getWidth());
            int h = juce::jmax(300, pluginEditor->getHeight());
            setContentNonOwned(pluginEditor, true);
            centreWithSize(w, h);
        }
    }

private:
    std::function<void()> closeCallback;
    juce::AudioProcessorEditor* pluginEditor = nullptr;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditorWindow)
};

class MainComponent : public juce::Component, public juce::Timer, public juce::MidiInputCallback
{
public:
    MainComponent();
    ~MainComponent() override;


    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    bool keyPressed(const juce::KeyPress& key) override;
    bool keyStateChanged(bool isKeyDown) override;

    // Capture event type — public so free-function helpers can access it
    struct CaptureEvent
    {
        enum Type : uint8_t { NoteOn, NoteOff, CC, PitchBend };
        Type type = NoteOn;
        uint8_t channel = 1;
        uint8_t data1 = 0;
        uint8_t data2 = 0;
        int16_t pitchBend = 0;
        double absTime = 0.0;
    };
    static constexpr int CAPTURE_RING_SIZE = 32768;

private:
    ThemeManager themeManager;
    juce::ComboBox themeSelector;
    bool forceIPadLayout = false;
    SpectrumComponent spectrumDisplay;
    LissajousComponent lissajousDisplay;
    WaveTerrainComponent waveTerrainDisplay;
    ShaderToyComponent shaderToyDisplay;
    AnalyzerComponent analyzerDisplay;
    GeissComponent geissDisplay;
    ProjectMComponent projectMDisplay;
    std::unique_ptr<HeartbeatComponent> heartbeatDisplay;
    HeartRateManager heartRateManager;
    std::unique_ptr<BioResonanceComponent> bioResonanceDisplay;
    FluidSimComponent fluidSimDisplay;
    RayMarchComponent rayMarchDisplay;
    juce::AudioDeviceManager deviceManager;
    juce::AudioProcessorPlayer audioPlayer;
    PluginHost pluginHost;

    // Current track
    int selectedTrackIndex = 0;

    // Transport
    double lastSpaceStopTime = 0.0;
    bool wasRecording = false;

    // BPM drag
    bool bpmDragging = false;
    juce::Point<float> bpmDragStart;
    double bpmDragStartValue = 120.0;

    // ── Top Bar ──
    juce::TextButton midiLearnButton { "LEARN" };
    juce::Label trackNameLabel;
    juce::TextButton recordButton { "REC" };
    juce::TextButton playButton { "PLAY" };
    juce::TextButton stopButton { "STOP" };
    juce::TextButton metronomeButton { "MET" };
    juce::TextButton bpmDownButton { "-" };
    juce::Label bpmLabel;
    juce::TextButton bpmUpButton { "+" };
    juce::Label beatLabel;
    juce::TextButton tapTempoButton { "TAP" };
    juce::Array<double> tapTimes;
    static constexpr int maxTaps = 8;

    // ── Edit Toolbar ──
    juce::TextButton newClipButton { "New Clip" };
    juce::TextButton deleteClipButton { "Delete" };
    juce::TextButton duplicateClipButton { "Duplicate" };
    juce::TextButton splitClipButton { "Split" };
    juce::TextButton editClipButton { "Edit Notes" };
    juce::TextButton quantizeButton { "Quantize" };
    juce::TextButton clearAutoButton { "CLR AUTO" };
    juce::ComboBox gridSelector;
    juce::TextButton countInButton { "Count-In" };
    juce::TextButton loopButton { "LOOP" };
    juce::TextButton panicButton { "PANIC" };
    juce::TextButton arpButton { "ARP" };
    juce::TextButton arpModeButton { "Up" };
    juce::TextButton arpRateButton { "1/8" };
    juce::TextButton arpOctButton { "Oct 1" };
    juce::TextButton glassAnimButton { "FX" };
    bool glassAnimEnabled = true;
    double panicAnimEndTime = 0.0;

    // ── Capture (always-on MIDI ring buffer) ──
    juce::TextButton captureButton { "CAPT" };
    juce::TextButton exportButton { "EXPORT" };
    std::unique_ptr<AudioExporter> audioExporter;
    std::array<CaptureEvent, CAPTURE_RING_SIZE> captureRing;
    std::atomic<int> captureWritePos { 0 };
    std::atomic<int> captureCount { 0 };

    void performCapture();

    // ── Navigation ──
    juce::TextButton zoomInButton { "Zoom +" };
    juce::TextButton zoomOutButton { "Zoom -" };
    juce::TextButton scrollLeftButton { "<<" };
    juce::TextButton scrollRightButton { ">>" };

    // ── Right Panel ──
    juce::ComboBox pluginSelector;
    juce::TextButton openEditorButton { "E" };
    juce::ComboBox midiInputSelector;
    juce::TextButton midiRefreshButton { "Refresh" };
    juce::TextButton audioSettingsButton { "Audio Settings" };
    juce::TextButton settingsButton { "Settings" };
    juce::TextButton fullscreenButton { "VIS" };
    juce::ComboBox visSelector;
    bool visualizerFullScreen = false;
    int currentVisMode = 0;  // 0=Spectrum, 1=Lissajous, 2=G-Force, 3=Geiss
    juce::TextButton visExitButton { "EXIT" };
    juce::TextButton projectorButton { "PROJ" };
    bool projectorMode = false;
    juce::TextButton testNoteButton { "Test Note" };
    juce::TextButton phoneMenuButton { "..." };

    // ── iPhone swipe gesture tracking ──
    juce::Point<float> swipeStartPos;
    bool swipeActive = false;
    void showPhoneMenu();

    // ── Mixer ──
    std::unique_ptr<MixerComponent> mixerComponent;
    juce::TextButton mixerButton { "MIX" };
    juce::ComboBox trackInputSelector;
    void updateTrackInputSelector();
    void applyTrackInput(int id);
    bool mixerVisible = false;

    // ── Touch Piano ──
    TouchPianoComponent touchPiano;
    juce::TextButton pianoToggleButton { "KEYS" };
    juce::TextButton pianoOctUpButton { "Oct+" };
    juce::TextButton pianoOctDownButton { "Oct-" };
    bool touchPianoVisible = false;

    // ── Visualizer Controls ──
    // Geiss
    juce::TextButton geissWaveBtn { "Wave" };
    juce::TextButton geissPaletteBtn { "Color" };
    juce::TextButton geissSceneBtn { "Scene" };
    juce::TextButton geissWaveUpBtn { "W+" };
    juce::TextButton geissWaveDownBtn { "W-" };
    juce::TextButton geissWarpLockBtn { "Warp" };
    juce::TextButton geissPalLockBtn { "PLock" };
    juce::ComboBox geissSpeedSelector;
    juce::TextButton geissAutoPilotBtn { "Auto" };
    juce::TextButton geissBgBtn { "BG" };
    // ProjectM
    juce::TextButton pmNextBtn { "Next" };
    juce::TextButton pmPrevBtn { "Prev" };
    juce::TextButton pmRandBtn { "Rand" };
    juce::TextButton pmLockBtn { "Lock" };
    juce::TextButton pmBgBtn { "BG" };
    // G-Force
    juce::TextButton gfRibbonUpBtn { "R+" };
    juce::TextButton gfRibbonDownBtn { "R-" };
    juce::TextButton gfTrailBtn { "Trail" };
    juce::ComboBox gfSpeedSelector;
    // Spectrum
    juce::TextButton specDecayBtn { "Decay" };
    juce::TextButton specSensUpBtn { "S+" };
    juce::TextButton specSensDownBtn { "S-" };
    // Lissajous
    juce::TextButton lissZoomInBtn { "Z+" };
    juce::TextButton lissZoomOutBtn { "Z-" };
    juce::TextButton lissDotsBtn { "Dots" };
    // FluidSim
    juce::TextButton fluidColorBtn { "Color" };
    juce::TextButton fluidViscUpBtn { "V+" };
    juce::TextButton fluidViscDownBtn { "V-" };
    juce::TextButton fluidVortBtn { "Vort" };
    // RayMarch
    juce::TextButton rmPrevBtn { "Prev" };
    juce::TextButton rmNextBtn { "Next" };

    void setVisControlsVisible();
    void updateVisualizerTimers();

    // MidiInputCallback — intercept SysEx for CI before it goes to collector
    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& msg) override;

    // ── FX Inserts ──
#if JUCE_IOS
    static constexpr int NUM_FX_SLOTS = 1;
#else
    static constexpr int NUM_FX_SLOTS = 2;
#endif
    juce::OwnedArray<juce::ComboBox> fxSelectors;
    juce::OwnedArray<juce::TextButton> fxEditorButtons;
    void updateFxDisplay();
    void loadFxPlugin(int slotIndex);
    void openFxEditor(int slotIndex);

    // ── Right Panel — Save/Load/Undo ──
    juce::TextButton saveButton { "Save" };
    juce::TextButton loadButton { "Load" };
    juce::TextButton undoButton { "Undo" };
    juce::TextButton redoButton { "Redo" };

    // Undo system — stores snapshots of clip data
    struct ProjectSnapshot {
        struct ClipData {
            juce::MidiMessageSequence events;
            double lengthInBeats = 4.0;
            double timelinePosition = 0.0;
            int trackIndex = 0;
            int slotIndex = 0;
            bool isAudio = false;
            juce::AudioBuffer<float> audioSamples;
            double audioSampleRate = 44100.0;
        };
        struct AutomationLaneData {
            int trackIndex = 0;
            int parameterIndex = -1;
            juce::String parameterName;
            juce::Array<AutomationPoint> points;
        };
        juce::Array<ClipData> clips;
        juce::Array<AutomationLaneData> automationData;
        double bpm = 120.0;
        bool loopEnabled = false;
        double loopStart = 0.0;
        double loopEnd = 0.0;
    };
    juce::Array<ProjectSnapshot> undoHistory;
    int undoIndex = -1;
    void takeSnapshot();
    void trimUndoHistoryByMemory();
    void restoreSnapshot(const ProjectSnapshot& snap);
    void saveProject();
    void saveProjectQuick();
    void saveProjectToFile(const juce::File& file);
    void loadProject();
    juce::File currentProjectFile;


    // ── Chord Detector ──
    ChordDetector chordDetector;
    juce::Label chordLabel;

    // ── MIDI 2.0 CI ──
    Midi2Handler midi2Handler;
    juce::TextButton midi2Button { "M2" };
    bool midi2Enabled = false;
    juce::String midiOutputId;
    std::unique_ptr<juce::MidiOutput> midiOutput;  // kept open for CI responses

    // Per-device MIDI output bus.  Controller integrations (Launchkey
    // MK4 etc.) need to push back to a SPECIFIC endpoint identified by
    // its device name (not the single global midiOutput, which is
    // dedicated to MIDI 2.0 CI traffic for Keystage).  Lazy-opened on
    // first request via outputForDevice().
    std::map<juce::String, std::unique_ptr<juce::MidiOutput>> deviceOutputs;

    // Native Launchkey Mini MK4 surface integration.  Owns the device's
    // DAW-port output + decodes its DAW-protocol input.
    LaunchkeyMK4Controller launchkey;
    juce::Label launchkeyMidiInspector;   // on-screen MIDI byte readout

public:
    /// Open and cache an output endpoint matching the given substring
    /// in any installed device name.  Returns nullptr if no match.
    juce::MidiOutput* outputForDevice(const juce::String& nameContains);

    // ── Launchkey MK4 native controller bridge ──
    SessionViewComponent* getSessionViewComponent() { return sessionViewComponent.get(); }
    void controllerPlayToggle();
    void controllerStop();
    void controllerRecordToggle();
    void controllerLoopToggle();
    void controllerSelectTrack(int delta);
    void controllerScrollScenes(int delta);
    void controllerLaunchScene();
    void setTrackVolumeFromController(int visibleIdx, float value);

private:

    // ── Plugin Parameters ──
#if JUCE_IOS
    static constexpr int NUM_PARAM_SLIDERS = 12;
#else
    static constexpr int NUM_PARAM_SLIDERS = 6;
#endif
    juce::OwnedArray<juce::Slider> paramSliders;
    juce::OwnedArray<juce::Label> paramLabels;

    // ── Preset Browser ──
    juce::ComboBox presetSelector;
    juce::TextButton presetPrevButton { "<" };
    juce::TextButton presetNextButton { ">" };
    juce::TextButton presetUpButton { juce::String::charToString(0x25B2) };
    juce::TextButton presetDownButton { juce::String::charToString(0x25BC) };
    void updatePresetList();
    void loadPreset(int index);
    int paramPageOffset = 0;
    bool paramSmartPage = true;
    juce::TextButton paramPageLeft { "<" };
    juce::TextButton paramPageRight { ">" };
    juce::Label paramPageLabel;
    juce::Label paramPageNameLabel;
    int activeParamIndex = -1;
    float paramHighlightAlpha = 0.0f;
    bool paramHighlightFadingIn = false;
    void updateParamSliders();
    void highlightParamKnob(int index);

    // ── Play / Record pulsing border highlights ──
    float playHighlightAlpha = 0.4f;
    bool playHighlightOn = false;
    int playFlashCounter = 0;
    float recHighlightAlpha = 0.4f;
    bool recHighlightOn = false;
    int recFlashCounter = 0;
    float loopHighlightAlpha = 0.4f;
    bool loopHighlightOn = false;
    int loopFlashCounter = 0;

    // ── Arranger Minimap ──
    std::unique_ptr<ArrangerMinimapComponent> arrangerMinimap;

    // ── Right Panel — Slideable ──
    bool rightPanelVisible = true;
    float panelSlideProgress = 1.0f;   // 1.0 = fully open, 0.0 = fully closed
    float panelSlideTarget = 1.0f;
    int getPanelWidth() const { return getWidth() >= 1100 ? 260 : 180; }
    bool isProDevice() const { return getWidth() >= 1100; }  // layout only — use deviceTier for perf

    // Performance tier — cached from AUScanner::getDeviceTier() on construction
    AUScanner::DeviceTier deviceTier = AUScanner::DeviceTier::Mid;
    bool isHighTier() const { return deviceTier == AUScanner::DeviceTier::High; }
    bool isMidTier() const { return deviceTier == AUScanner::DeviceTier::Mid
                                 || deviceTier == AUScanner::DeviceTier::JamieEdition; }
    bool isLowTier() const { return deviceTier == AUScanner::DeviceTier::Low; }

    int getGlassTimerHz() const {
        switch (deviceTier) {
            case AUScanner::DeviceTier::High:           return 120;
            case AUScanner::DeviceTier::JamieEdition:   return 60;
            case AUScanner::DeviceTier::Mid:             return 60;
            case AUScanner::DeviceTier::Low:             return 30;
        }
        return 30;
    }
    int getBaseTimerHz() const {
        switch (deviceTier) {
            case AUScanner::DeviceTier::High:           return 30;
            case AUScanner::DeviceTier::JamieEdition:   return 30;
            case AUScanner::DeviceTier::Mid:             return 20;
            case AUScanner::DeviceTier::Low:             return 15;
        }
        return 15;
    }
    float panelAnimStartValue = 1.0f;
    double panelAnimStartTime = 0.0;
    bool panelAnimating = false;
    int panelAnimFrameSkip = 0;
    juce::TextButton panelToggleButton { juce::String::charToString(0x25C0) }; // ◀
    void toggleRightPanel();

    // Frosted glass panel background (blurred snapshot of content behind panel)
    juce::Image panelBlurImage;
    int panelBlurUpdateCounter = 0;
    juce::Rectangle<int> panelBoundsCache;
    void updatePanelBlur();

    // ── Loop Set Mode ──
    juce::TextButton loopSetButton { "LOOP SET" };

    // ── Right Panel — Mix + Info ──
    juce::Slider volumeSlider;
    juce::Label volumeLabel { {}, "Vol" };
    juce::Slider panSlider;
    juce::Label panLabel { {}, "Pan" };
    juce::Label trackInfoLabel;
    juce::Label cpuLabel;
    juce::Array<float> cpuHistory;  // rolling CPU % history for heartbeat display
    float currentCpuPercent = 0.0f;
    int currentRamMB = 0;
    void showExpandedEkg();

    // EKG sweep state
    static constexpr int EKG_BUFFER_SIZE = 200;
    std::array<float, EKG_BUFFER_SIZE> ekgBuffer {};  // circular EKG sample buffer
    int ekgWritePos = 0;
    double ekgPhase = 0.0;  // continuous phase through PQRST cycle

    // ── Glass/Liquid animations ──
    double glassAnimTime = 0.0;  // continuous time for caustics & breathing
    float smoothTiltX = 0.0f;   // smoothed accelerometer values
    float smoothTiltY = 0.0f;
    struct Ripple {
        float x, y;        // center position
        float age;          // seconds since tap
        float maxRadius;
    };
    static constexpr int MAX_RIPPLES = 6;
    std::array<Ripple, MAX_RIPPLES> ripples {};
    int rippleCount = 0;
    void addRipple(float x, float y);
    void drawWaterCaustics(juce::Graphics& g, juce::Rectangle<int> area);
    void drawRipples(juce::Graphics& g);
    void drawBreathingStripe(juce::Graphics& g);

#if JUCE_IOS
    // Metal GPU-accelerated caustic renderer
    std::unique_ptr<MetalCausticRenderer> metalRenderer;
    bool metalRendererAttached = false;
    void updateMetalCaustics();
    void attachMetalRendererIfNeeded();
#endif

    // ── Bottom Bar ──
    juce::Label statusLabel;

    // ── Timeline (arrangement view) ──
    std::unique_ptr<TimelineComponent> timelineComponent;

    // ── Session View (clip launcher) ──
    std::unique_ptr<SessionViewComponent> sessionViewComponent;
    juce::TextButton sessionViewButton { "SESSION" };
    bool showingSessionView = false;

    // Plugin editor
    std::unique_ptr<juce::AudioProcessorEditor> currentEditor;
    std::unique_ptr<PluginEditorWindow> editorWindow;

    // Data
    juce::Array<juce::PluginDescription> pluginDescriptions;  // instruments
    juce::Array<juce::PluginDescription> fxDescriptions;      // effects
    juce::Array<juce::MidiDeviceInfo> midiDevices;
    juce::String currentMidiDeviceId;

    // Computer keyboard MIDI
    bool useComputerKeyboard = false;
    int computerKeyboardOctave = 4;
    std::set<int> keysCurrentlyDown;
    int keyToNote(int keyCode) const;
    void sendNoteOn(int note);
    void sendNoteOff(int note);

    // Methods
    void selectTrack(int index);
    void updateTrackDisplay();

    void scanPlugins();
    void loadSelectedPlugin();
    void openPluginEditor();
    void closePluginEditor();
    void playTestNote();

    void scanMidiDevices();
    void selectMidiDevice();
    void disableCurrentMidiDevice();
    void showAudioSettings();
    void showSettingsMenu();
    void updateStatusLabel();
    void applyThemeToControls();

    // ── MIDI Learn ──
    enum class MidiTarget {
        None, Volume, Pan, Bpm,
        Play, Stop, Record, Metronome, Loop,
        Param0, Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8,
        Param9, Param10, Param11, Param12, Param13, Param14, Param15,
        TrackNext, TrackPrev,
        GeissWaveform, GeissPalette, GeissScene,
        GeissWaveScale, GeissWarpLock, GeissPaletteLock, GeissSpeed,
        GForceRibbons, GForceTrail, GForceSpeed,
        SpecDecay, SpecSensitivity,
        LissZoom, LissDots
    };

    struct MidiMapping {
        int channel = -1;
        int ccNumber = -1;
        MidiTarget target = MidiTarget::None;
    };

    bool midiLearnActive = false;
    MidiTarget midiLearnTarget = MidiTarget::None;
    juce::Array<MidiMapping> midiMappings;
    void startMidiLearn(MidiTarget target);
    void processMidiLearnCC(int channel, int cc, int value);
    void applyMidiCC(const MidiMapping& mapping, int value);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
