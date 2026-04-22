#pragma once

#include <juce_core/juce_core.h>
#include <cstdint>

namespace Juno60 {

class Oscillator
{
public:
    Oscillator();
    ~Oscillator();

    void prepare (double sampleRate);
    void setSampleRate (double sr);
    void setFrequency (float frequencyHz);
    void setNote (int midiNote, int range);

    // Waveform enables
    void setSawEnabled (bool enabled);
    void setPulseEnabled (bool enabled);
    void setSubEnabled (bool enabled);

    // Range: 16', 8', 4'
    void setRange (int footage); // 16, 8, or 4

    // PWM
    void setPulseWidth (float pw);         // 0..1
    void setPWMSource (int source);        // 0 = LFO, 1 = Manual
    void setPWMAmount (float amount);      // 0..1
    void setLFOModulation (float lfoValue); // -1..1

    void noteOn(); // Reset sub-oscillator state for clean start

    // Combined process (respects enabled flags, advances phase once)
    float process();

    // Individual waveform processors (do NOT advance phase)
    float processSaw();
    float processPulse (float pulseWidth);
    float processSub();
    static float processNoise();

    void reset();

private:
    double sampleRate = 44100.0;
    float frequency = 440.0f;
    double phase = 0.0;           // double for precision
    double phaseIncrement = 0.0;

    bool sawEnabled = false;
    bool pulseEnabled = true;
    bool subEnabled = false;
    int range = 8;

    float pulseWidth_ = 0.5f;
    int pwmSource = 0;
    float pwmAmount = 0.0f;
    float lfoValue = 0.0f;

    // Sub-oscillator state: toggle flip-flop
    bool subToggle = false;
    bool prevSawReset = false;

    // PRNG state for noise
    static uint32_t noiseState;

    void updatePhaseIncrement();
    void advancePhase();

    // PolyBLEP correction
    static float polyBLEP (float t, float dt);
};

} // namespace Juno60
