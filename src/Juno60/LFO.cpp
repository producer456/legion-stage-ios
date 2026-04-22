#include "LFO.h"
#include <cmath>
#include <algorithm>

namespace Juno60 {

LFO::LFO() {}
LFO::~LFO() {}

void LFO::prepare (double sr)
{
    sampleRate = sr;
    reset();
}

void LFO::setRate (float hz)
{
    rate = hz;
}

void LFO::setDelay (float seconds)
{
    delayTime = seconds;
    delaySamples = static_cast<float> (seconds * sampleRate);
}

void LFO::noteOn()
{
    // Restart the delay fade-in ramp, but do NOT reset phase
    // (Juno-60 LFO is free-running / global)
    delayCounter = 0.0f;
    delayRamp = 0.0f;
}

float LFO::process()
{
    // Advance the delay ramp (fade-in after note-on)
    if (delaySamples > 0.0f && delayCounter < delaySamples)
    {
        delayCounter += 1.0f;
        delayRamp = delayCounter / delaySamples;
        if (delayRamp > 1.0f)
            delayRamp = 1.0f;
    }
    else
    {
        delayRamp = 1.0f;
    }

    // Advance free-running triangle LFO phase
    phase += static_cast<float> (rate / sampleRate);
    if (phase >= 1.0f)
        phase -= 1.0f;

    // Triangle waveform: -1..+1
    float tri = 4.0f * std::abs (phase - 0.5f) - 1.0f;

    return tri * delayRamp;
}

void LFO::reset()
{
    phase = 0.0f;
    delayCounter = 0.0f;
    delayRamp = 0.0f;
}

} // namespace Juno60
