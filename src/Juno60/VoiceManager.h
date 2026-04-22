#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include "JunoVoice.h"

namespace Juno60 {

class VoiceManager
{
public:
    static constexpr int kNumVoices = 6;

    explicit VoiceManager (int numVoices = kNumVoices);
    ~VoiceManager();

    void prepare (double sampleRate);

    void handleMidiEvent (const juce::MidiMessage& message);
    void renderNextBlock (juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
    void reset();

    void setUnison (bool on);

    // Bulk parameter updates from processor
    void setLFORate (float hz);
    void setLFODelay (float seconds);
    void setLFODepth (float depth);

    void setOscSawEnabled (bool on);
    void setOscPulseEnabled (bool on);
    void setOscSubEnabled (bool on);
    void setOscRange (int footage);
    void setPWMAmount (float amount);
    void setPWMSource (int source);

    void setFilterCutoff (float freq);
    void setFilterResonance (float res);
    void setFilterEnvAmount (float amount);
    void setFilterLFOAmount (float amount);
    void setFilterKeyFollow (float amount);

    void setVCAMode (int mode);
    void setVCALevel (float level);

    void setAttack (float val);
    void setDecay (float val);
    void setSustain (float val);
    void setRelease (float val);

    void setHPFMode (int mode);
    void setNoiseLevel (float level);

    void allNotesOff();

private:
    std::array<JunoVoice, kNumVoices> voices;
    int roundRobinIndex = 0;
    bool unisonMode = false;

    // Unison detune spread: +/- 0.1 semitones across 6 voices
    static constexpr float kUnisonDetuneSpread = 0.1f;

    void noteOn (int midiNote, float velocity);
    void noteOff (int midiNote);

    JunoVoice* findVoiceForNote (int midiNote);
    JunoVoice* allocateVoice();
    void applyUnisonDetune();
};

} // namespace Juno60
