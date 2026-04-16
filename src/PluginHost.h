#pragma once

#include <JuceHeader.h>
#include "GainProcessor.h"
#include "ClipPlayerNode.h"
#include "SequencerEngine.h"
#include <atomic>
#include <array>

class SpectrumComponent;
class LissajousComponent;
class WaveTerrainComponent;
class ShaderToyComponent;
class AnalyzerComponent;
class GeissComponent;
class ProjectMComponent;
class HeartbeatComponent;
class BioResonanceComponent;
class FluidSimComponent;
class RayMarchComponent;

struct FxSlot {
    juce::AudioProcessorGraph::Node::Ptr node;
    juce::AudioProcessor* processor = nullptr;
    bool bypassed = false;
};

enum class TrackType { MIDI, Audio };

struct Track {
    int index = 0;
    juce::String name;
    TrackType type = TrackType::MIDI;

    juce::AudioProcessorGraph::Node::Ptr pluginNode;
    juce::AudioProcessorGraph::Node::Ptr gainNode;
    juce::AudioProcessorGraph::Node::Ptr clipPlayerNode;
    GainProcessor* gainProcessor = nullptr;
    ClipPlayerNode* clipPlayer = nullptr;
    juce::AudioProcessor* plugin = nullptr;
    juce::OwnedArray<AutomationLane> automationLanes;
    juce::SpinLock automationLock;

    // "Param touch" mechanism: while the user is actively touching a knob,
    // automation playback must not overwrite their value.
    std::atomic<int> touchedParamIndex { -1 };
    std::atomic<int64_t> touchedParamTime { 0 };

    static constexpr int NUM_FX_SLOTS = 3;
    FxSlot fxSlots[NUM_FX_SLOTS];
};

// Provides real-time transport/timing info to all hosted plugins.
// Snapshot is captured once per processBlock for consistency.
class HostPlayHead : public juce::AudioPlayHead
{
public:
    HostPlayHead(SequencerEngine& eng) : engine(eng) {}

    // Called once at the start of each processBlock to snapshot state
    void captureState(double sr)
    {
        cachedBpm = engine.getBpm();
        if (cachedBpm <= 0.0) cachedBpm = 120.0; // guard div-by-zero
        cachedPosition = engine.getPositionInBeats();
        cachedPlaying = engine.isPlaying() && !engine.isInCountIn();
        cachedRecording = engine.isRecording();
        cachedLooping = engine.isLoopEnabled() && engine.hasLoopRegion();
        cachedLoopStart = engine.getLoopStart();
        cachedLoopEnd = engine.getLoopEnd();
        cachedSampleRate = sr;
    }

    // Plugins call this from the audio thread — uses cached snapshot
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setBpm(cachedBpm);

        double timeInSec = cachedPosition * 60.0 / cachedBpm;
        info.setTimeInSamples(static_cast<juce::int64>(timeInSec * cachedSampleRate));
        info.setTimeInSeconds(timeInSec);
        info.setPpqPosition(cachedPosition);
        info.setIsPlaying(cachedPlaying);
        info.setIsRecording(cachedRecording);
        info.setIsLooping(cachedLooping);

        if (cachedLooping)
        {
            LoopPoints loop;
            loop.ppqStart = cachedLoopStart;
            loop.ppqEnd = cachedLoopEnd;
            info.setLoopPoints(loop);
        }

        juce::AudioPlayHead::TimeSignature ts;
        ts.numerator = 4;
        ts.denominator = 4;
        info.setTimeSignature(ts);

        int bar = static_cast<int>(cachedPosition / 4.0);
        info.setBarCount(bar);
        info.setPpqPositionOfLastBarStart(bar * 4.0);

        return info;
    }

    // Cached state — consistent within one processBlock call
    bool isPlayingCached() const { return cachedPlaying; }

private:
    SequencerEngine& engine;
    double cachedBpm = 120.0;
    double cachedPosition = 0.0;
    bool cachedPlaying = false;
    bool cachedRecording = false;
    bool cachedLooping = false;
    double cachedLoopStart = 0.0;
    double cachedLoopEnd = 0.0;
    double cachedSampleRate = 44100.0;
};

class PluginHost : public juce::AudioProcessorGraph
{
public:
    static constexpr int NUM_TRACKS = 16;

    PluginHost();
    ~PluginHost() override;

    void scanForPlugins();
    const juce::KnownPluginList& getPluginList() const { return knownPluginList; }

    bool loadPlugin(int trackIndex, const juce::PluginDescription& desc, juce::String& errorMsg);
    void unloadPlugin(int trackIndex);
    void setTrackType(int trackIndex, TrackType type);

    // FX inserts
    bool loadFx(int trackIndex, int slotIndex, const juce::PluginDescription& desc, juce::String& errorMsg);
    void unloadFx(int trackIndex, int slotIndex);
    void setFxBypassed(int trackIndex, int slotIndex, bool bypassed);

    Track& getTrack(int index) { return tracks[static_cast<size_t>(index)]; }
    const Track& getTrack(int index) const { return tracks[static_cast<size_t>(index)]; }

    void setSelectedTrack(int index);
    int getSelectedTrack() const { return selectedTrack; }

    juce::MidiMessageCollector& getMidiCollector() { return midiCollector; }
    void sendTestNoteOn(int noteNumber = 60, float velocity = 0.78f);
    void sendTestNoteOff(int noteNumber = 60);

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    // Offline rendering for audio export
    void prepareForOfflineRender(double sampleRate, int blockSize);
    void processBlockOffline(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi, int numSamples);
    void restoreFromOfflineRender();

    void setAudioParams(double sampleRate, int blockSize);

    // Sequencer engine access
    SequencerEngine& getEngine() { return engine; }

    std::atomic<int> soloCount { 0 };

    // Spectrum analyzer — set from UI, read from audio thread
    std::atomic<SpectrumComponent*> spectrumDisplay { nullptr };
    std::atomic<WaveTerrainComponent*> waveTerrainDisplay { nullptr };
    std::atomic<ShaderToyComponent*> shaderToyDisplay { nullptr };
    std::atomic<AnalyzerComponent*> analyzerDisplay { nullptr };
    std::atomic<GeissComponent*> geissDisplay { nullptr };
    std::atomic<ProjectMComponent*> projectMDisplay { nullptr };
    std::atomic<HeartbeatComponent*> heartbeatDisplay { nullptr };
    std::atomic<BioResonanceComponent*> bioResonanceDisplay { nullptr };
    std::atomic<FluidSimComponent*> fluidSimDisplay { nullptr };
    std::atomic<RayMarchComponent*> rayMarchDisplay { nullptr };

private:
    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPluginList;
    juce::MidiMessageCollector midiCollector;
    SequencerEngine engine;

    std::array<Track, NUM_TRACKS> tracks;

    Node::Ptr midiInputNode;
    Node::Ptr audioInputNode;
    Node::Ptr audioOutputNode;

    int selectedTrack = 0;

    double storedSampleRate = 44100.0;
    int storedBlockSize = 512;

    // Saved state for offline render restore
    double offlineSavedSampleRate = 44100.0;
    int offlineSavedBlockSize = 512;

    // AudioPlayHead — provides BPM, position, transport state to plugins
    HostPlayHead hostPlayHead { engine };

    // MIDI clock state
    bool midiClockWasPlaying = false;
    double midiClockPulseAccum = 0.0;  // fractional pulse accumulator

    void setupGraph();
    void connectTrackAudio(int trackIndex);
    void rewireTrack(int trackIndex);
    void updateMidiRouting();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginHost)
};
