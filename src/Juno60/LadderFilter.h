#pragma once

#include <juce_core/juce_core.h>

namespace Juno60 {

class LadderFilter
{
public:
    LadderFilter();
    ~LadderFilter();

    void prepare (double sampleRate);
    void setCutoff (float frequencyHz);
    void setResonance (float res);       // 0..1
    void setEnvAmount (float amount);    // 0..1
    void setLFOAmount (float amount);    // 0..1
    void setKeyFollow (float amount);    // 0, 0.5, or 1.0
    void setEnvModulation (float envValue);
    void setLFOModulation (float lfoValue);
    void setNoteFrequency (float noteFreq);

    float process (float input);
    void reset();

private:
    // Fast tanh approximation — models OTA soft saturation
    inline float fastTanh (float x) const
    {
        float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    double sampleRate = 44100.0;
    float cutoff = 1000.0f;
    float resonance = 0.0f;
    float envAmount = 0.0f;
    float lfoAmount = 0.0f;
    float keyFollow = 0.0f;
    float envValue = 0.0f;
    float lfoValue = 0.0f;
    float noteFreq = 440.0f;

    float stage[4] = {};   // one state variable per pole
    float g = 0.0f;        // cached integrator coefficient
    float k = 0.0f;        // cached resonance (0..4)
};

} // namespace Juno60
