# 16-Track System Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Expand from 1 plugin slot to 16 tracks with per-track volume/pan/mute/solo, all mixing to a master stereo output.

**Architecture:** Each track has a plugin node and a GainProcessor node in the AudioProcessorGraph. All gain nodes sum to the audio output. MIDI routes to the selected track. TrackComponent handles per-track UI.

**Tech Stack:** C++17, JUCE 7.0.12

**Spec:** `docs/superpowers/specs/2026-03-25-track-system-design.md`

---

## File Structure

```
C:/dev/sequencer/
  CMakeLists.txt              — modify: add new source files
  src/
    Main.cpp                  — unchanged
    GainProcessor.h           — new: volume/pan/mute AudioProcessor
    GainProcessor.cpp         — new: processBlock with gain/pan/mute/solo
    TrackComponent.h          — new: single track row UI
    TrackComponent.cpp        — new: volume slider, mute/solo buttons
    PluginHost.h              — rewrite: Track struct array, multi-track graph
    PluginHost.cpp            — rewrite: per-track plugin/gain management
    MainComponent.h           — rewrite: track list panel, selected track
    MainComponent.cpp         — rewrite: track list, route actions to selected track
```

---

### Task 1: Update CMakeLists.txt

**Files:**
- Modify: `C:/dev/sequencer/CMakeLists.txt`

- [ ] **Step 1: Add new source files**

Add to `target_sources`:

```cmake
target_sources(Sequencer PRIVATE
    src/Main.cpp
    src/MainComponent.h
    src/MainComponent.cpp
    src/PluginHost.h
    src/PluginHost.cpp
    src/GainProcessor.h
    src/GainProcessor.cpp
    src/TrackComponent.h
    src/TrackComponent.cpp
)
```

- [ ] **Step 2: Commit**

```bash
cd /c/dev/sequencer
git add CMakeLists.txt
git commit -m "chore: add GainProcessor and TrackComponent to CMake"
```

---

### Task 2: GainProcessor

**Files:**
- Create: `C:/dev/sequencer/src/GainProcessor.h`
- Create: `C:/dev/sequencer/src/GainProcessor.cpp`

- [ ] **Step 1: Write GainProcessor.h**

```cpp
#pragma once

#include <JuceHeader.h>
#include <atomic>

class GainProcessor : public juce::AudioProcessor
{
public:
    GainProcessor();

    const juce::String getName() const override { return "GainProcessor"; }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void releaseResources() override {}

    // Editor
    bool hasEditor() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }

    // State
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

    // Bus layout
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    // Programs
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // Controls — set from UI thread, read from audio thread
    std::atomic<float> volume { 0.8f };
    std::atomic<float> pan { 0.0f };
    std::atomic<bool> muted { false };

    // Solo — shared counter across all tracks, set by PluginHost
    std::atomic<int>* soloCount = nullptr;
    std::atomic<bool> soloed { false };

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GainProcessor)
};
```

- [ ] **Step 2: Write GainProcessor.cpp**

```cpp
#include "GainProcessor.h"
#include <cmath>

GainProcessor::GainProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

void GainProcessor::prepareToPlay(double /*sampleRate*/, int /*samplesPerBlock*/)
{
}

bool GainProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void GainProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midi*/)
{
    // Check mute
    if (muted.load())
    {
        buffer.clear();
        return;
    }

    // Check solo — if any track is soloed and this one isn't, silence
    if (soloCount != nullptr && soloCount->load() > 0 && !soloed.load())
    {
        buffer.clear();
        return;
    }

    float vol = volume.load();
    float p = pan.load(); // -1.0 to 1.0

    // Equal power pan law
    float angle = (p + 1.0f) * 0.25f * juce::MathConstants<float>::pi; // 0 to pi/2
    float leftGain = vol * std::cos(angle);
    float rightGain = vol * std::sin(angle);

    if (buffer.getNumChannels() >= 1)
        buffer.applyGain(0, 0, buffer.getNumSamples(), leftGain);
    if (buffer.getNumChannels() >= 2)
        buffer.applyGain(1, 0, buffer.getNumSamples(), rightGain);
}
```

- [ ] **Step 3: Commit**

```bash
cd /c/dev/sequencer
git add src/GainProcessor.h src/GainProcessor.cpp
git commit -m "feat: add GainProcessor with volume/pan/mute/solo"
```

---

### Task 3: TrackComponent

**Files:**
- Create: `C:/dev/sequencer/src/TrackComponent.h`
- Create: `C:/dev/sequencer/src/TrackComponent.cpp`

- [ ] **Step 1: Write TrackComponent.h**

```cpp
#pragma once

#include <JuceHeader.h>
#include <functional>

class TrackComponent : public juce::Component
{
public:
    TrackComponent(int trackIndex);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

    void setSelected(bool selected);
    void setPluginName(const juce::String& name);
    float getVolume() const;
    bool isMuted() const;
    bool isSoloed() const;

    // Callbacks — set by MainComponent
    std::function<void(int)> onSelected;           // track index
    std::function<void(int, float)> onVolumeChanged; // track index, value
    std::function<void(int, bool)> onMuteChanged;    // track index, state
    std::function<void(int, bool)> onSoloChanged;    // track index, state

private:
    int index;
    bool selected = false;

    juce::Label trackLabel;
    juce::Label pluginLabel;
    juce::Slider volumeSlider;
    juce::TextButton muteButton { "M" };
    juce::TextButton soloButton { "S" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackComponent)
};
```

- [ ] **Step 2: Write TrackComponent.cpp**

```cpp
#include "TrackComponent.h"

TrackComponent::TrackComponent(int trackIndex)
    : index(trackIndex)
{
    addAndMakeVisible(trackLabel);
    trackLabel.setText("Track " + juce::String(index + 1), juce::dontSendNotification);
    trackLabel.setFont(juce::Font(13.0f, juce::Font::bold));

    addAndMakeVisible(pluginLabel);
    pluginLabel.setText("----", juce::dontSendNotification);
    pluginLabel.setFont(juce::Font(12.0f));
    pluginLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);

    addAndMakeVisible(volumeSlider);
    volumeSlider.setRange(0.0, 1.0, 0.01);
    volumeSlider.setValue(0.8, juce::dontSendNotification);
    volumeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    volumeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    volumeSlider.onValueChange = [this] {
        if (onVolumeChanged)
            onVolumeChanged(index, static_cast<float>(volumeSlider.getValue()));
    };

    addAndMakeVisible(muteButton);
    muteButton.setClickingTogglesState(true);
    muteButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::red);
    muteButton.onClick = [this] {
        if (onMuteChanged)
            onMuteChanged(index, muteButton.getToggleState());
    };

    addAndMakeVisible(soloButton);
    soloButton.setClickingTogglesState(true);
    soloButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::yellow);
    soloButton.onClick = [this] {
        if (onSoloChanged)
            onSoloChanged(index, soloButton.getToggleState());
    };
}

void TrackComponent::paint(juce::Graphics& g)
{
    if (selected)
        g.fillAll(juce::Colour(0xff3a5a8a));
    else
        g.fillAll(juce::Colour(0xff2a2a2a));

    g.setColour(juce::Colour(0xff444444));
    g.drawLine(0, static_cast<float>(getHeight()), static_cast<float>(getWidth()), static_cast<float>(getHeight()));
}

void TrackComponent::resized()
{
    auto area = getLocalBounds().reduced(4, 2);

    trackLabel.setBounds(area.removeFromLeft(60));
    pluginLabel.setBounds(area.removeFromLeft(100));
    muteButton.setBounds(area.removeFromRight(25));
    area.removeFromRight(4);
    soloButton.setBounds(area.removeFromRight(25));
    area.removeFromRight(4);
    volumeSlider.setBounds(area);
}

void TrackComponent::mouseDown(const juce::MouseEvent& /*e*/)
{
    if (onSelected)
        onSelected(index);
}

void TrackComponent::setSelected(bool sel)
{
    selected = sel;
    repaint();
}

void TrackComponent::setPluginName(const juce::String& name)
{
    pluginLabel.setText(name.isEmpty() ? "----" : name, juce::dontSendNotification);
}

float TrackComponent::getVolume() const
{
    return static_cast<float>(volumeSlider.getValue());
}

bool TrackComponent::isMuted() const
{
    return muteButton.getToggleState();
}

bool TrackComponent::isSoloed() const
{
    return soloButton.getToggleState();
}
```

- [ ] **Step 3: Commit**

```bash
cd /c/dev/sequencer
git add src/TrackComponent.h src/TrackComponent.cpp
git commit -m "feat: add TrackComponent UI for per-track controls"
```

---

### Task 4: Rewrite PluginHost for multi-track

**Files:**
- Modify: `C:/dev/sequencer/src/PluginHost.h`
- Modify: `C:/dev/sequencer/src/PluginHost.cpp`

- [ ] **Step 1: Write PluginHost.h**

```cpp
#pragma once

#include <JuceHeader.h>
#include "GainProcessor.h"
#include <atomic>
#include <array>

struct Track {
    int index = 0;
    juce::String name;
    juce::AudioProcessorGraph::Node::Ptr pluginNode;
    juce::AudioProcessorGraph::Node::Ptr gainNode;
    GainProcessor* gainProcessor = nullptr;  // raw ptr — graph owns it
    juce::AudioProcessor* plugin = nullptr;  // raw ptr — graph owns it
};

class PluginHost : public juce::AudioProcessorGraph
{
public:
    static constexpr int NUM_TRACKS = 16;

    PluginHost();
    ~PluginHost() override;

    // Plugin scanning
    void scanForPlugins();
    const juce::KnownPluginList& getPluginList() const { return knownPluginList; }

    // Per-track plugin loading — caller must suspend audio before calling
    bool loadPlugin(int trackIndex, const juce::PluginDescription& desc, juce::String& errorMsg);
    void unloadPlugin(int trackIndex);

    // Track access
    Track& getTrack(int index) { return tracks[static_cast<size_t>(index)]; }
    const Track& getTrack(int index) const { return tracks[static_cast<size_t>(index)]; }

    // Selected track for MIDI routing
    void setSelectedTrack(int index);
    int getSelectedTrack() const { return selectedTrack; }

    // MIDI
    juce::MidiMessageCollector& getMidiCollector() { return midiCollector; }
    void sendTestNoteOn(int noteNumber = 60, float velocity = 0.78f);
    void sendTestNoteOff(int noteNumber = 60);

    // Override processBlock to pull MIDI from collector
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    void setAudioParams(double sampleRate, int blockSize);

    // Solo management
    std::atomic<int> soloCount { 0 };

private:
    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPluginList;
    juce::MidiMessageCollector midiCollector;

    std::array<Track, NUM_TRACKS> tracks;

    Node::Ptr midiInputNode;
    Node::Ptr audioOutputNode;

    int selectedTrack = 0;

    double storedSampleRate = 44100.0;
    int storedBlockSize = 512;

    void setupGraph();
    void connectTrackAudio(int trackIndex);
    void updateMidiRouting();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginHost)
};
```

- [ ] **Step 2: Write PluginHost.cpp**

```cpp
#include "PluginHost.h"

PluginHost::PluginHost()
{
    formatManager.addFormat(new juce::VST3PluginFormat());

    // Init track structs
    for (int i = 0; i < NUM_TRACKS; ++i)
    {
        tracks[static_cast<size_t>(i)].index = i;
        tracks[static_cast<size_t>(i)].name = "Track " + juce::String(i + 1);
    }

    setupGraph();
}

PluginHost::~PluginHost()
{
    clear();
}

void PluginHost::setupGraph()
{
    setPlayConfigDetails(0, 2, storedSampleRate, storedBlockSize);

    midiInputNode = addNode(
        std::make_unique<AudioProcessorGraph::AudioGraphIOProcessor>(
            AudioProcessorGraph::AudioGraphIOProcessor::midiInputNode));

    audioOutputNode = addNode(
        std::make_unique<AudioProcessorGraph::AudioGraphIOProcessor>(
            AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode));

    // Create gain nodes for all 16 tracks
    for (int i = 0; i < NUM_TRACKS; ++i)
    {
        auto gainProc = std::make_unique<GainProcessor>();
        gainProc->soloCount = &soloCount;
        tracks[static_cast<size_t>(i)].gainProcessor = gainProc.get();
        tracks[static_cast<size_t>(i)].gainNode = addNode(std::move(gainProc));

        // Connect gain node outputs to audio output (summing)
        for (int ch = 0; ch < 2; ++ch)
        {
            addConnection({ { tracks[static_cast<size_t>(i)].gainNode->nodeID, ch },
                            { audioOutputNode->nodeID, ch } });
        }
    }
}

void PluginHost::scanForPlugins()
{
    auto* format = formatManager.getFormat(0);
    if (format == nullptr) return;

    auto searchPaths = format->getDefaultLocationsToSearch();
    auto foundFiles = format->searchPathsForPlugins(searchPaths, true, false);

    for (const auto& file : foundFiles)
    {
        juce::OwnedArray<juce::PluginDescription> foundTypes;
        knownPluginList.scanAndAddFile(file, true, foundTypes, *format);
    }
}

void PluginHost::setAudioParams(double sampleRate, int blockSize)
{
    storedSampleRate = sampleRate;
    storedBlockSize = blockSize;
    midiCollector.reset(sampleRate);
}

bool PluginHost::loadPlugin(int trackIndex, const juce::PluginDescription& desc, juce::String& errorMsg)
{
    if (trackIndex < 0 || trackIndex >= NUM_TRACKS) return false;

    unloadPlugin(trackIndex);

    auto instance = formatManager.createPluginInstance(desc, storedSampleRate, storedBlockSize, errorMsg);
    if (instance == nullptr)
        return false;

    auto& track = tracks[static_cast<size_t>(trackIndex)];
    track.plugin = instance.get();
    track.pluginNode = addNode(std::move(instance));

    if (track.pluginNode == nullptr)
    {
        track.plugin = nullptr;
        errorMsg = "Failed to add plugin to graph";
        return false;
    }

    connectTrackAudio(trackIndex);
    updateMidiRouting();
    prepareToPlay(storedSampleRate, storedBlockSize);

    return true;
}

void PluginHost::unloadPlugin(int trackIndex)
{
    if (trackIndex < 0 || trackIndex >= NUM_TRACKS) return;

    auto& track = tracks[static_cast<size_t>(trackIndex)];
    if (track.pluginNode == nullptr) return;

    // Remove connections to/from plugin node
    auto connections = getConnections();
    for (auto& conn : connections)
    {
        if (conn.source.nodeID == track.pluginNode->nodeID ||
            conn.destination.nodeID == track.pluginNode->nodeID)
        {
            removeConnection(conn);
        }
    }

    removeNode(track.pluginNode->nodeID);
    track.pluginNode = nullptr;
    track.plugin = nullptr;
}

void PluginHost::connectTrackAudio(int trackIndex)
{
    auto& track = tracks[static_cast<size_t>(trackIndex)];
    if (track.pluginNode == nullptr || track.gainNode == nullptr) return;

    // Plugin stereo out -> gain node stereo in
    for (int ch = 0; ch < 2; ++ch)
    {
        addConnection({ { track.pluginNode->nodeID, ch },
                        { track.gainNode->nodeID, ch } });
    }
}

void PluginHost::setSelectedTrack(int index)
{
    if (index < 0 || index >= NUM_TRACKS) return;
    selectedTrack = index;
    updateMidiRouting();
}

void PluginHost::updateMidiRouting()
{
    // Remove all MIDI connections from the MIDI input node
    auto connections = getConnections();
    for (auto& conn : connections)
    {
        if (conn.source.nodeID == midiInputNode->nodeID &&
            conn.source.channelIndex == AudioProcessorGraph::midiChannelIndex)
        {
            removeConnection(conn);
        }
    }

    // Connect MIDI to selected track's plugin (if it has one)
    auto& track = tracks[static_cast<size_t>(selectedTrack)];
    if (track.pluginNode != nullptr)
    {
        addConnection({ { midiInputNode->nodeID, AudioProcessorGraph::midiChannelIndex },
                        { track.pluginNode->nodeID, AudioProcessorGraph::midiChannelIndex } });
    }
}

void PluginHost::sendTestNoteOn(int noteNumber, float velocity)
{
    auto msg = juce::MidiMessage::noteOn(1, noteNumber, velocity);
    msg.setTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001);
    midiCollector.addMessageToQueue(msg);
}

void PluginHost::sendTestNoteOff(int noteNumber)
{
    auto msg = juce::MidiMessage::noteOff(1, noteNumber);
    msg.setTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001);
    midiCollector.addMessageToQueue(msg);
}

void PluginHost::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    midiCollector.removeNextBlockOfMessages(midiMessages, buffer.getNumSamples());
    AudioProcessorGraph::processBlock(buffer, midiMessages);
}
```

- [ ] **Step 3: Commit**

```bash
cd /c/dev/sequencer
git add src/PluginHost.h src/PluginHost.cpp
git commit -m "feat: rewrite PluginHost for 16-track multi-plugin graph"
```

---

### Task 5: Rewrite MainComponent for track list

**Files:**
- Modify: `C:/dev/sequencer/src/MainComponent.h`
- Modify: `C:/dev/sequencer/src/MainComponent.cpp`

- [ ] **Step 1: Write MainComponent.h**

```cpp
#pragma once

#include <JuceHeader.h>
#include "PluginHost.h"
#include "TrackComponent.h"

class PluginEditorWindow : public juce::DocumentWindow
{
public:
    PluginEditorWindow(const juce::String& name, juce::AudioProcessorEditor* editor,
                       std::function<void()> onClose)
        : DocumentWindow(name, juce::Colours::darkgrey, DocumentWindow::closeButton),
          closeCallback(std::move(onClose))
    {
        setUsingNativeTitleBar(true);
        setContentNonOwned(editor, true);
        setVisible(true);
        centreWithSize(getWidth(), getHeight());
    }

    void closeButtonPressed() override
    {
        if (closeCallback) closeCallback();
    }

private:
    std::function<void()> closeCallback;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditorWindow)
};

class MainComponent : public juce::Component
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::AudioDeviceManager deviceManager;
    juce::AudioProcessorPlayer audioPlayer;
    PluginHost pluginHost;

    // UI — top controls
    juce::ComboBox midiInputSelector;
    juce::TextButton midiRefreshButton { "Refresh" };
    juce::ComboBox pluginSelector;
    juce::TextButton openEditorButton   { "Open Editor" };
    juce::TextButton testNoteButton     { "Play Test Note" };
    juce::TextButton audioSettingsButton { "Audio Settings" };
    juce::Label statusLabel;

    // Track list
    juce::OwnedArray<TrackComponent> trackComponents;
    juce::Viewport trackViewport;
    juce::Component trackListContainer;
    int selectedTrackIndex = 0;

    // Plugin editor window
    std::unique_ptr<juce::AudioProcessorEditor> currentEditor;
    std::unique_ptr<PluginEditorWindow> editorWindow;

    // Plugin descriptions
    juce::Array<juce::PluginDescription> pluginDescriptions;

    // MIDI device tracking
    juce::Array<juce::MidiDeviceInfo> midiDevices;
    juce::String currentMidiDeviceId;

    void scanMidiDevices();
    void selectMidiDevice();
    void disableCurrentMidiDevice();

    void scanPlugins();
    void loadSelectedPlugin();
    void openPluginEditor();
    void closePluginEditor();
    void playTestNote();
    void showAudioSettings();
    void updateStatusLabel();

    void selectTrack(int index);
    void onTrackVolumeChanged(int trackIndex, float volume);
    void onTrackMuteChanged(int trackIndex, bool muted);
    void onTrackSoloChanged(int trackIndex, bool soloed);
    void setupTrackList();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
```

- [ ] **Step 2: Write MainComponent.cpp**

```cpp
#include "MainComponent.h"

MainComponent::MainComponent()
{
    auto result = deviceManager.initialiseWithDefaultDevices(0, 2);
    if (result.isNotEmpty())
        DBG("Audio device init error: " + result);

    audioPlayer.setProcessor(&pluginHost);
    deviceManager.addAudioCallback(&audioPlayer);

    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        pluginHost.setAudioParams(device->getCurrentSampleRate(),
                                  device->getCurrentBufferSizeSamples());
        pluginHost.prepareToPlay(device->getCurrentSampleRate(),
                                 device->getCurrentBufferSizeSamples());
    }

    // MIDI UI
    addAndMakeVisible(midiInputSelector);
    midiInputSelector.onChange = [this] { selectMidiDevice(); };
    addAndMakeVisible(midiRefreshButton);
    midiRefreshButton.onClick = [this] { scanMidiDevices(); };

    // Plugin UI
    addAndMakeVisible(pluginSelector);
    pluginSelector.onChange = [this] { loadSelectedPlugin(); };
    addAndMakeVisible(openEditorButton);
    openEditorButton.onClick = [this] { openPluginEditor(); };
    openEditorButton.setEnabled(false);
    addAndMakeVisible(testNoteButton);
    testNoteButton.onClick = [this] { playTestNote(); };
    testNoteButton.setEnabled(false);
    addAndMakeVisible(audioSettingsButton);
    audioSettingsButton.onClick = [this] { showAudioSettings(); };

    addAndMakeVisible(statusLabel);
    statusLabel.setJustificationType(juce::Justification::centred);

    // Track list
    setupTrackList();
    addAndMakeVisible(trackViewport);
    trackViewport.setViewedComponent(&trackListContainer, false);

    setSize(900, 700);

    scanPlugins();
    scanMidiDevices();
    selectTrack(0);
    updateStatusLabel();
}

MainComponent::~MainComponent()
{
    disableCurrentMidiDevice();
    closePluginEditor();
    audioPlayer.setProcessor(nullptr);
    deviceManager.removeAudioCallback(&audioPlayer);
}

void MainComponent::setupTrackList()
{
    trackComponents.clear();

    for (int i = 0; i < PluginHost::NUM_TRACKS; ++i)
    {
        auto* tc = new TrackComponent(i);
        tc->onSelected = [this](int idx) { selectTrack(idx); };
        tc->onVolumeChanged = [this](int idx, float vol) { onTrackVolumeChanged(idx, vol); };
        tc->onMuteChanged = [this](int idx, bool m) { onTrackMuteChanged(idx, m); };
        tc->onSoloChanged = [this](int idx, bool s) { onTrackSoloChanged(idx, s); };
        trackListContainer.addAndMakeVisible(tc);
        trackComponents.add(tc);
    }

    trackListContainer.setSize(350, PluginHost::NUM_TRACKS * 36);
}

void MainComponent::selectTrack(int index)
{
    selectedTrackIndex = index;
    pluginHost.setSelectedTrack(index);

    for (int i = 0; i < trackComponents.size(); ++i)
        trackComponents[i]->setSelected(i == index);

    // Update plugin selector to show the selected track's plugin
    auto& track = pluginHost.getTrack(index);
    if (track.plugin != nullptr)
    {
        openEditorButton.setEnabled(true);
        testNoteButton.setEnabled(true);
    }
    else
    {
        openEditorButton.setEnabled(false);
        testNoteButton.setEnabled(false);
    }

    // Close editor when switching tracks
    closePluginEditor();
    updateStatusLabel();
}

void MainComponent::onTrackVolumeChanged(int trackIndex, float volume)
{
    auto& track = pluginHost.getTrack(trackIndex);
    if (track.gainProcessor != nullptr)
        track.gainProcessor->volume.store(volume);
}

void MainComponent::onTrackMuteChanged(int trackIndex, bool muted)
{
    auto& track = pluginHost.getTrack(trackIndex);
    if (track.gainProcessor != nullptr)
        track.gainProcessor->muted.store(muted);
}

void MainComponent::onTrackSoloChanged(int trackIndex, bool soloed)
{
    auto& track = pluginHost.getTrack(trackIndex);
    if (track.gainProcessor != nullptr)
    {
        bool wasSoloed = track.gainProcessor->soloed.load();
        track.gainProcessor->soloed.store(soloed);

        if (soloed && !wasSoloed)
            pluginHost.soloCount.fetch_add(1);
        else if (!soloed && wasSoloed)
            pluginHost.soloCount.fetch_sub(1);
    }
}

// ── MIDI Device Management ───────────────────────────────────────────────────

void MainComponent::scanMidiDevices()
{
    midiInputSelector.clear(juce::dontSendNotification);
    midiDevices = juce::MidiInput::getAvailableDevices();
    midiInputSelector.addItem("-- No MIDI Input --", 1);
    int itemId = 2;
    for (const auto& device : midiDevices)
        midiInputSelector.addItem(device.name, itemId++);
    midiInputSelector.setSelectedId(1, juce::dontSendNotification);
}

void MainComponent::selectMidiDevice()
{
    disableCurrentMidiDevice();
    int selectedIndex = midiInputSelector.getSelectedId() - 2;
    if (selectedIndex < 0 || selectedIndex >= midiDevices.size())
    {
        updateStatusLabel();
        return;
    }
    auto& device = midiDevices[selectedIndex];
    deviceManager.setMidiInputDeviceEnabled(device.identifier, true);
    deviceManager.addMidiInputDeviceCallback(device.identifier, &pluginHost.getMidiCollector());
    currentMidiDeviceId = device.identifier;
    updateStatusLabel();
}

void MainComponent::disableCurrentMidiDevice()
{
    if (currentMidiDeviceId.isNotEmpty())
    {
        deviceManager.removeMidiInputDeviceCallback(currentMidiDeviceId, &pluginHost.getMidiCollector());
        deviceManager.setMidiInputDeviceEnabled(currentMidiDeviceId, false);
        currentMidiDeviceId.clear();
    }
}

// ── Plugin Management ────────────────────────────────────────────────────────

void MainComponent::scanPlugins()
{
    statusLabel.setText("Scanning plugins...", juce::dontSendNotification);
    repaint();

    pluginHost.scanForPlugins();

    pluginSelector.clear(juce::dontSendNotification);
    pluginDescriptions.clear();
    pluginSelector.addItem("-- Select Plugin --", 1);

    int itemId = 2;
    for (const auto& desc : pluginHost.getPluginList().getTypes())
    {
        if (desc.isInstrument)
        {
            pluginSelector.addItem(desc.name, itemId);
            pluginDescriptions.add(desc);
            itemId++;
        }
    }

    pluginSelector.setSelectedId(1, juce::dontSendNotification);
}

void MainComponent::loadSelectedPlugin()
{
    int selectedIndex = pluginSelector.getSelectedId() - 2;
    if (selectedIndex < 0 || selectedIndex >= pluginDescriptions.size())
    {
        openEditorButton.setEnabled(false);
        testNoteButton.setEnabled(false);
        return;
    }

    closePluginEditor();
    audioPlayer.setProcessor(nullptr);

    juce::String errorMsg;
    bool success = pluginHost.loadPlugin(selectedTrackIndex, pluginDescriptions[selectedIndex], errorMsg);

    audioPlayer.setProcessor(&pluginHost);

    if (success)
    {
        openEditorButton.setEnabled(true);
        testNoteButton.setEnabled(true);
        trackComponents[selectedTrackIndex]->setPluginName(pluginDescriptions[selectedIndex].name);
        updateStatusLabel();
    }
    else
    {
        openEditorButton.setEnabled(false);
        testNoteButton.setEnabled(false);
        statusLabel.setText("Failed: " + errorMsg, juce::dontSendNotification);
    }
}

void MainComponent::openPluginEditor()
{
    auto& track = pluginHost.getTrack(selectedTrackIndex);
    if (track.plugin == nullptr) return;

    closePluginEditor();

    currentEditor.reset(track.plugin->createEditorIfNeeded());
    if (currentEditor == nullptr)
    {
        statusLabel.setText("Plugin has no editor", juce::dontSendNotification);
        return;
    }

    editorWindow = std::make_unique<PluginEditorWindow>(
        track.plugin->getName(), currentEditor.get(),
        [this] { closePluginEditor(); });
}

void MainComponent::closePluginEditor()
{
    // IMPORTANT: destroy window first, then editor
    editorWindow = nullptr;
    currentEditor = nullptr;
}

void MainComponent::playTestNote()
{
    pluginHost.sendTestNoteOn(60, 0.78f);
    juce::Timer::callAfterDelay(500, [this] {
        pluginHost.sendTestNoteOff(60);
    });
}

void MainComponent::showAudioSettings()
{
    auto* selector = new juce::AudioDeviceSelectorComponent(
        deviceManager, 0, 0, 1, 2, false, false, false, false);
    selector->setSize(500, 400);

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(selector);
    options.dialogTitle = "Audio Settings";
    options.componentToCentreAround = this;
    options.dialogBackgroundColour = getLookAndFeel().findColour(
        juce::ResizableWindow::backgroundColourId);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.launchAsync();

    juce::Timer::callAfterDelay(500, [this] {
        if (auto* device = deviceManager.getCurrentAudioDevice())
            pluginHost.setAudioParams(device->getCurrentSampleRate(),
                                      device->getCurrentBufferSizeSamples());
        updateStatusLabel();
    });
}

void MainComponent::updateStatusLabel()
{
    juce::String text;

    text += "Track " + juce::String(selectedTrackIndex + 1) + " | ";

    auto& track = pluginHost.getTrack(selectedTrackIndex);
    if (track.plugin != nullptr)
        text += "Loaded: " + track.plugin->getName() + " | ";
    else
        text += "No plugin | ";

    if (currentMidiDeviceId.isNotEmpty())
    {
        for (const auto& dev : midiDevices)
        {
            if (dev.identifier == currentMidiDeviceId)
            {
                text += "MIDI: " + dev.name + " | ";
                break;
            }
        }
    }

    auto* device = deviceManager.getCurrentAudioDevice();
    if (device != nullptr)
    {
        text += device->getName()
              + " | " + juce::String(device->getCurrentSampleRate(), 0) + " Hz"
              + " | " + juce::String(device->getCurrentBufferSizeSamples()) + " samples";
    }
    else
    {
        text += "No audio device";
    }

    statusLabel.setText(text, juce::dontSendNotification);
}

// ── Component overrides ──────────────────────────────────────────────────────

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced(10);

    // Top controls
    auto topArea = area.removeFromTop(150);

    auto midiRow = topArea.removeFromTop(30);
    midiRefreshButton.setBounds(midiRow.removeFromRight(80));
    midiRow.removeFromRight(5);
    midiInputSelector.setBounds(midiRow);
    topArea.removeFromTop(5);

    pluginSelector.setBounds(topArea.removeFromTop(30));
    topArea.removeFromTop(5);

    auto buttonRow = topArea.removeFromTop(30);
    openEditorButton.setBounds(buttonRow.removeFromLeft(buttonRow.getWidth() / 2));
    testNoteButton.setBounds(buttonRow);
    topArea.removeFromTop(5);

    audioSettingsButton.setBounds(topArea.removeFromTop(30));

    area.removeFromTop(5);

    // Status bar at bottom
    statusLabel.setBounds(area.removeFromBottom(25));
    area.removeFromBottom(5);

    // Track list takes remaining space
    trackViewport.setBounds(area);

    // Layout track components inside the container
    int trackHeight = 36;
    trackListContainer.setSize(trackViewport.getWidth() - trackViewport.getScrollBarThickness(),
                               PluginHost::NUM_TRACKS * trackHeight);
    for (int i = 0; i < trackComponents.size(); ++i)
    {
        trackComponents[i]->setBounds(0, i * trackHeight,
                                       trackListContainer.getWidth(), trackHeight);
    }
}
```

- [ ] **Step 3: Commit**

```bash
cd /c/dev/sequencer
git add src/MainComponent.h src/MainComponent.cpp
git commit -m "feat: rewrite MainComponent with 16-track list UI"
```

---

### Task 6: Build and Test

- [ ] **Step 1: Kill old Sequencer, reconfigure, build**

```bash
taskkill //IM Sequencer.exe //F 2>/dev/null
cd /c/dev/sequencer
cmake -B build -G "Visual Studio 16 2019" -A x64
cmake --build build --config Release
```

- [ ] **Step 2: Run and test**

Expected:
1. 16 tracks visible in scrollable list
2. Click a track to select it (highlights blue)
3. Plugin dropdown loads into the selected track
4. Load different plugins on different tracks
5. MIDI plays the selected track's plugin
6. Volume sliders control per-track volume
7. Mute buttons silence tracks
8. Solo buttons isolate tracks
9. Multiple plugins producing audio simultaneously

- [ ] **Step 3: Final commit**

```bash
cd /c/dev/sequencer
git add src/ CMakeLists.txt
git commit -m "feat: 16-track system complete with volume/pan/mute/solo"
```
