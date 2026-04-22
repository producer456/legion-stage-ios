#pragma once

#include <juce_core/juce_core.h>
#include <cmath>

namespace Juno60 {

class LFO
{
public:
    LFO();
    ~LFO();

    void prepare (double sampleRate);
    void setRate (float hz);
    void setDelay (float seconds);

    // Called on note-on to restart delay fade-in ramp
    // Phase is NOT reset (free-running, global LFO)
    void noteOn();

    // Returns -1..+1 triangle, scaled by delay ramp
    float process();
    void reset();

private:
    double sampleRate = 44100.0;
    float rate = 1.0f;          // Hz
    float phase = 0.0f;         // 0..1 free-running phase
    float delayTime = 0.0f;     // delay in seconds
    float delaySamples = 0.0f;  // delay in samples
    float delayCounter = 0.0f;  // current position in delay ramp
    float delayRamp = 0.0f;     // 0..1 fade-in multiplier
};

} // namespace Juno60
