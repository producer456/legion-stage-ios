#pragma once

#include <juce_core/juce_core.h>
#include "Oscillator.h"
#include "LadderFilter.h"
#include "Envelope.h"
#include "LFO.h"

namespace Juno60 {

class JunoVoice
{
public:
    JunoVoice();
    ~JunoVoice();

    void prepare (double sampleRate);

    void noteOn (int midiNote, float velocity);
    void noteOff();

    float process();
    bool isActive() const;
    void reset();

    int getCurrentNote() const { return currentNote; }
    uint64_t getNoteAge() const { return noteAge; }
    bool isReleasing() const { return !noteHeld && envelope.isActive(); }

    // Parameter setters (called by VoiceManager)
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

    void setVCAMode (int mode); // 0=gate, 1=env
    void setVCALevel (float level);

    void setAttack (float val);
    void setDecay (float val);
    void setSustain (float val);
    void setRelease (float val);

    void setHPFMode (int mode);
    void setNoiseLevel (float level);

    // Detune in semitones (for unison mode)
    void setDetune (float semitones);

private:
    double sampleRate = 44100.0;
    int currentNote = -1;
    float velocity = 0.0f;
    bool noteHeld = false;
    uint64_t noteAge = 0; // monotonic counter for voice stealing

    Oscillator oscillator;
    LadderFilter filter;
    Envelope envelope; // single ADSR for both filter and VCA
    LFO lfo;

    float lfoDepth = 0.0f;
    int vcaMode = 1;
    float vcaLevel = 1.0f;
    int hpfMode = 0;
    float noiseLevel = 0.0f;
    float detuneSemitones = 0.0f;

    // HPF state (1-pole)
    float hpfState = 0.0f;
    float hpfPrevInput = 0.0f;

    // LFO delay ramp (0 -> 1)
    float lfoDelayRamp = 0.0f;
    float lfoDelayMs = 0.0f;
    float lfoDelayRampIncrement = 0.0f;

    float processHPF (float input, int mode);
    void updateFrequency();

    static uint64_t globalNoteCounter;
};

} // namespace Juno60
