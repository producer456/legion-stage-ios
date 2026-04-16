#pragma once

#include <JuceHeader.h>
#include <array>
#include <algorithm>

// Audio-thread-safe arpeggiator that processes a MidiBuffer in-place.
// Call process() from ClipPlayerNode::processBlock() to transform held
// notes into rhythmic patterns synced to the transport.
class Arpeggiator
{
public:
    enum class Mode { Up, Down, UpDown, Random, Order, Chord };
    static constexpr int NUM_MODES = 6;

    // ── Controls (set from UI thread, read from audio thread) ──
    void setEnabled(bool on) { enabled.store(on); }
    bool isEnabled() const { return enabled.load(); }
    void toggleEnabled() { enabled.store(!enabled.load()); }

    void setMode(Mode m) { mode.store(m); }
    Mode getMode() const { return mode.load(); }
    void cycleMode()
    {
        int m = static_cast<int>(mode.load());
        mode.store(static_cast<Mode>((m + 1) % NUM_MODES));
    }

    void setRate(int r) { rate.store(juce::jlimit(0, NUM_RATES - 1, r)); }
    int getRate() const { return rate.load(); }
    void cycleRate() { rate.store((rate.load() + 1) % NUM_RATES); }
    const char* getRateName() const
    {
        static const char* names[] = { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32" };
        return names[rate.load()];
    }

    void setOctaveRange(int r) { octaveRange.store(juce::jlimit(1, 4, r)); }
    int getOctaveRange() const { return octaveRange.load(); }
    void octaveUp() { setOctaveRange(octaveRange.load() + 1); }
    void octaveDown() { setOctaveRange(octaveRange.load() - 1); }

    void setGate(float g) { gate.store(juce::jlimit(0.1f, 1.0f, g)); }
    float getGate() const { return gate.load(); }

    void setSwing(float s) { swing.store(juce::jlimit(0.0f, 0.75f, s)); }
    float getSwing() const { return swing.load(); }

    void setVelocityScale(float v) { velocityScale.store(juce::jlimit(0.1f, 1.5f, v)); }
    float getVelocityScale() const { return velocityScale.load(); }

    const char* getModeName() const
    {
        static const char* names[] = { "Up", "Down", "Up/Dn", "Rand", "Order", "Chord" };
        return names[static_cast<int>(mode.load())];
    }

    // ── Audio thread: process MIDI buffer in-place ──
    void process(juce::MidiBuffer& midi, int numSamples, double bpm, double beatPosition, double sampleRate)
    {
        processReset();

        if (!enabled.load() || bpm <= 0 || sampleRate <= 0)
        {
            // When disabled, kill any lingering arp note
            if (currentNoteOn >= 0)
                killCurrentNote(midi, 0);
            return;
        }

        // Rebuild sequence if mode/rate/octave changed since last call
        Mode curMode = mode.load();
        int curOct = octaveRange.load();
        if (curMode != lastMode || curOct != lastOctRange)
        {
            lastMode = curMode;
            lastOctRange = curOct;
            rebuildSequence();
        }

        // Collect held notes from incoming MIDI
        for (const auto meta : midi)
        {
            auto msg = meta.getMessage();
            if (msg.isNoteOn())
            {
                int note = msg.getNoteNumber();
                int vel = msg.getVelocity();
                if (!isHeld(note) && numHeld < MAX_NOTES)
                {
                    int order = numHeld;
                    heldNotes[numHeld] = { note, vel, order };
                    numHeld++;
                    rebuildSequence();
                }
            }
            else if (msg.isNoteOff())
            {
                removeHeld(msg.getNoteNumber());
                rebuildSequence();
            }
        }

        if (numHeld == 0 || seqLen == 0)
        {
            // Kill any sounding arp notes
            killCurrentNote(midi, 0);
            return;
        }

        // Remove original note events — arp replaces them
        juce::MidiBuffer filtered;
        for (const auto meta : midi)
        {
            auto msg = meta.getMessage();
            if (!msg.isNoteOn() && !msg.isNoteOff())
                filtered.addEvent(msg, meta.samplePosition);
        }
        midi.swapWith(filtered);

        // Rate: beats per arp step
        static const float rateBeats[] = { 4.0f, 2.0f, 1.0f, 0.5f, 0.25f, 0.125f };
        float stepBeats = rateBeats[rate.load()];

        float swingAmount = swing.load();
        float gateLen = gate.load();
        double beatsPerSample = bpm / 60.0 / sampleRate;

        for (int s = 0; s < numSamples; ++s)
        {
            double currentBeat = beatPosition + s * beatsPerSample;

            // Compute swung step position
            // Unswung step index for step counting
            int rawStepIndex = static_cast<int>(std::floor(currentBeat / stepBeats));

            // Compute the actual swung start of this step
            double swungStepStart = rawStepIndex * stepBeats;
            if (rawStepIndex % 2 == 1)
                swungStepStart += swingAmount * stepBeats;

            // Detect new step: trigger when we cross the swung boundary
            bool newStep = false;
            if (rawStepIndex != lastStepIndex)
            {
                // Only trigger if we've actually passed the swung start
                if (currentBeat >= swungStepStart)
                {
                    newStep = true;
                    lastStepIndex = rawStepIndex;
                }
            }

            if (newStep)
            {
                // Kill previous note
                killCurrentNote(midi, s);

                // Advance sequence position
                if (curMode == Mode::Random)
                {
                    // Use a hash that feels more random
                    uint32_t hash = static_cast<uint32_t>(rawStepIndex * 2654435761u);
                    seqPos = static_cast<int>(hash % static_cast<uint32_t>(seqLen));
                }
                else
                    seqPos = rawStepIndex % seqLen;

                if (curMode == Mode::UpDown && seqLen > 1)
                {
                    int cycle = (seqLen - 1) * 2;
                    int pos = rawStepIndex % cycle;
                    seqPos = pos < seqLen ? pos : cycle - pos;
                }

                // Play new note
                int note = sequence[seqPos % seqLen];
                int vel = static_cast<int>(baseVelocity[seqPos % seqLen] * velocityScale.load());
                vel = juce::jlimit(1, 127, vel);

                currentNoteOn = note;
                currentNoteChannel = 1;
                midi.addEvent(juce::MidiMessage::noteOn(1, note, static_cast<juce::uint8>(vel)), s);
            }

            // Gate off — use swung timing
            if (currentNoteOn >= 0)
            {
                double noteOnBeat = lastStepIndex * stepBeats;
                if (lastStepIndex % 2 == 1)
                    noteOnBeat += swingAmount * stepBeats;
                double gateEnd = noteOnBeat + stepBeats * gateLen;

                if (currentBeat >= gateEnd)
                    killCurrentNote(midi, s);
            }
        }
    }

    // Request reset from any thread (processed safely on audio thread)
    void reset() { resetRequested.store(true); }

    // Called at start of process() to handle pending reset
    void processReset()
    {
        if (resetRequested.exchange(false))
        {
            numHeld = 0;
            seqLen = 0;
            seqPos = 0;
            lastStepIndex = -1;
            killPending = false;
            currentNoteOn = -1;
            lastMode = Mode::Up;
            lastOctRange = 1;
        }
    }

private:
    static constexpr int MAX_NOTES = 16;
    static constexpr int MAX_SEQ = 64; // max sequence length with octave expansion
    static constexpr int NUM_RATES = 6;

    // Held note info
    struct HeldNote {
        int note = -1;
        int velocity = 100;
        int order = 0; // order in which it was pressed
    };

    HeldNote heldNotes[MAX_NOTES] = {};
    int numHeld = 0;

    // Generated arp sequence (after sorting + octave expansion)
    int sequence[MAX_SEQ] = {};
    int baseVelocity[MAX_SEQ] = {};
    int seqLen = 0;
    int seqPos = 0;
    int lastStepIndex = -1;

    // Currently sounding note
    int currentNoteOn = -1;
    int currentNoteChannel = 1;
    bool killPending = false;

    // Track last mode/octave for change detection
    Mode lastMode = Mode::Up;
    int lastOctRange = 1;

    // Thread-safe reset flag
    std::atomic<bool> resetRequested { false };

    // Atomic controls
    std::atomic<bool> enabled { false };
    std::atomic<Mode> mode { Mode::Up };
    std::atomic<int> rate { 3 }; // default 1/8
    std::atomic<int> octaveRange { 1 };
    std::atomic<float> gate { 0.8f };
    std::atomic<float> swing { 0.0f };
    std::atomic<float> velocityScale { 1.0f };

    bool isHeld(int note) const
    {
        for (int i = 0; i < numHeld; ++i)
            if (heldNotes[i].note == note) return true;
        return false;
    }

    void removeHeld(int note)
    {
        for (int i = 0; i < numHeld; ++i)
        {
            if (heldNotes[i].note == note)
            {
                for (int j = i; j < numHeld - 1; ++j)
                    heldNotes[j] = heldNotes[j + 1];
                numHeld--;
                return;
            }
        }
    }

    void rebuildSequence()
    {
        if (numHeld == 0) { seqLen = 0; return; }

        // Sort notes by pitch for Up/Down modes
        HeldNote sorted[MAX_NOTES];
        for (int i = 0; i < numHeld; ++i) sorted[i] = heldNotes[i];

        Mode curMode = mode.load();
        if (curMode == Mode::Order)
        {
            // Sort by press order
            std::sort(sorted, sorted + numHeld,
                [](const HeldNote& a, const HeldNote& b) { return a.order < b.order; });
        }
        else if (curMode != Mode::Random)
        {
            // Sort by pitch
            std::sort(sorted, sorted + numHeld,
                [](const HeldNote& a, const HeldNote& b) { return a.note < b.note; });
        }

        // Build sequence with octave expansion
        int octaves = octaveRange.load();
        seqLen = 0;

        for (int oct = 0; oct < octaves && seqLen < MAX_SEQ; ++oct)
        {
            for (int i = 0; i < numHeld && seqLen < MAX_SEQ; ++i)
            {
                int note = sorted[i].note + oct * 12;
                if (note > 127) continue;
                sequence[seqLen] = note;
                baseVelocity[seqLen] = sorted[i].velocity;
                seqLen++;
            }
        }

        // For Down mode, reverse the sequence
        if (curMode == Mode::Down)
        {
            for (int i = 0; i < seqLen / 2; ++i)
            {
                std::swap(sequence[i], sequence[seqLen - 1 - i]);
                std::swap(baseVelocity[i], baseVelocity[seqLen - 1 - i]);
            }
        }
    }

    void killCurrentNote(juce::MidiBuffer& midi, int sampleOffset)
    {
        if (currentNoteOn >= 0)
        {
            midi.addEvent(juce::MidiMessage::noteOff(currentNoteChannel, currentNoteOn), sampleOffset);
            currentNoteOn = -1;
        }
    }
};
