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
    void processAudioClipPlayback(int slotIndex, juce::AudioBuffer<float>& buffer, int numSamples);
    void processRecording(const juce::MidiBuffer& incomingMidi, int numSamples);
    void processAudioRecording(const juce::AudioBuffer<float>& inputBuffer, int numSamples);
    void closeOpenNotes(MidiClip& clip);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClipPlayerNode)
};
