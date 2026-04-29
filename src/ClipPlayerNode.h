#pragma once

#include <JuceHeader.h>
#include "MidiClip.h"
#include "SequencerEngine.h"
#include "Arpeggiator.h"
#include <vector>
#include <cstring>

class ClipPlayerNode : public juce::AudioProcessor
{
public:
    // Legacy constant kept for backward compatibility — no longer a hard limit
    static constexpr int NUM_SLOTS = 4;

    ClipPlayerNode(SequencerEngine& engine);

    const juce::String getName() const override { return "ClipPlayerNode"; }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void releaseResources() override {}

    bool hasEditor() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override { return true; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }

    // Clip slot access — pre-allocated, effectively unlimited
    // Uses a fixed-capacity vector to avoid audio-thread resize crashes.
    // slotCount tracks how many slots are logically in use.
    int getNumSlots() const { return slotCount.load(); }
    ClipSlot& getSlot(int index) { return slots[static_cast<size_t>(index)]; }

    // Find first empty slot, or grow if none available (UI thread only)
    int findOrCreateEmptySlot()
    {
        int n = slotCount.load();
        for (int i = 0; i < n; ++i)
            if (!slots[static_cast<size_t>(i)].hasContent() && slots[static_cast<size_t>(i)].clip == nullptr)
                return i;
        // Grow by one — safe because we pre-allocated MAX_SLOTS capacity
        if (n < MAX_SLOTS)
        {
            slotCount.store(n + 1);
            return n;
        }
        return -1;  // truly full (256 clips on one track)
    }

    // Ensure at least `count` slots exist (UI thread only, for project load)
    void ensureSlots(int count)
    {
        int c = juce::jmin(count, MAX_SLOTS);
        if (c > slotCount.load())
            slotCount.store(c);
    }

    // Trigger actions (called from UI thread)
    void triggerSlot(int slotIndex);   // play or record
    void stopSlot(int slotIndex);
    void stopAllSlots();

    // Arm for recording
    std::atomic<bool> armed { false };
    std::atomic<bool> armLocked { false };  // stays armed when switching tracks

    // Arpeggiator
    Arpeggiator arpeggiator;
    Arpeggiator& getArpeggiator() { return arpeggiator; }

    // Flag to send all-notes-off on next processBlock
    std::atomic<bool> sendAllNotesOff { false };
    // When true, also sends allSoundOff (CC 120) to kill tails instantly (panic only)
    std::atomic<bool> panicKill { false };

    // Synchronously flush note-offs into a provided MidiBuffer (for use before MIDI disconnect).
    // This avoids the race where the async sendAllNotesOff flag might not be processed
    // if the graph topology changes before the next processBlock call.
    void flushNoteOffs(juce::MidiBuffer& midi)
    {
        killActiveNotes(midi, 0, true);
        arpeggiator.reset();
        lastPositionInBeats = -1.0;
        std::fill(wasInsideClip.begin(), wasInsideClip.end(), false);
    }

    // Audio track mode — set by PluginHost when track type changes
    std::atomic<bool> audioMode { false };

    static constexpr int MAX_SLOTS = 256;

    // ── Step sequencer (Launchkey-pad-driven) ──
    // When enabled, the track plays a drum-machine-style pattern at
    // the engine's tempo: 8 voices × 16 steps, each voice firing its
    // own MIDI note (default GM drum map — kick / snare / hats /
    // toms / cymbal).  16 pads = 16 steps for the selected voice.
    static constexpr int STEP_PATTERN_MAX = 16;
    static constexpr int STEP_NUM_VOICES  = 8;
    std::atomic<bool> stepSeqEnabled { false };

    struct DrumVoice {
        std::atomic<bool>    on[STEP_PATTERN_MAX]   = {};
        std::atomic<uint8_t> vel[STEP_PATTERN_MAX]  = {};
        std::atomic<int>     note    { 36 };       // MIDI note this voice fires
        DrumVoice() {
            for (int i = 0; i < STEP_PATTERN_MAX; ++i) { on[i].store(false); vel[i].store(100); }
        }
    };

    struct DrumPattern {
        DrumVoice            voices[STEP_NUM_VOICES];
        std::atomic<int>     length        { 16 };
        std::atomic<int>     channel       { 0 };
        std::atomic<float>   gate          { 0.5f };
        std::atomic<int>     selectedVoice { 0 };  // which voice the pads edit

        DrumPattern() {
            // GM drum-map defaults — pair well with our built-in
            // drum-kit samplers (CR-78 / TR-505 / LM-2).
            const int defaults[STEP_NUM_VOICES] = { 36, 38, 42, 46, 41, 45, 48, 49 };
            for (int v = 0; v < STEP_NUM_VOICES; ++v) voices[v].note.store(defaults[v]);
        }
    } stepPattern;

    // Most-recent step index (for UI playhead highlight on iPad / pads).
    std::atomic<int> stepSeqCurrentStep { -1 };

private:
    SequencerEngine& engine;
    std::vector<ClipSlot> slots;
    std::atomic<int> slotCount { 0 };

    double currentSampleRate = 44100.0;
    double lastPositionInBeats = 0.0;
    double arpFreeRunBeat = 0.0;  // free-running beat counter for arp when transport is stopped
    std::vector<bool> wasInsideClip;

    // Recording slot — atomic for thread safety
    std::atomic<int> atomicRecordingSlot { -1 };

    // Recording state
    double recordStartBeat = 0.0;
    double recordStartBpm = 120.0;

    // Track active notes so we can send explicit note-offs on loop wrap
    // First dimension is channel (0-15), second is note number (0-127)
    bool activePlaybackNotes[16][128] = {};
    void killActiveNotes(juce::MidiBuffer& midi, int sampleOffset, bool hard = false, bool panicKill = false);

    void processClipPlayback(int slotIndex, juce::MidiBuffer& midi, int numSamples);

    // Step sequencer per-block processing — fires note-on at each
    // step boundary and tracks pending note-offs so the gate can
    // span block boundaries.
    void processStepSequencer(juce::MidiBuffer& midi, int numSamples);
    struct StepPendingOff {
        int note = -1;
        int channel = 1;
        double offBeat = 0.0;
        bool active = false;
    };
    static constexpr int STEP_MAX_PENDING_OFFS = 32;
    StepPendingOff stepPendingOffs[STEP_MAX_PENDING_OFFS] = {};
    void processAudioClipPlayback(int slotIndex, juce::AudioBuffer<float>& buffer, int numSamples);
    void processRecording(const juce::MidiBuffer& incomingMidi, int numSamples);
    void processAudioRecording(const juce::AudioBuffer<float>& inputBuffer, int numSamples);
    void closeOpenNotes(MidiClip& clip);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClipPlayerNode)
};
