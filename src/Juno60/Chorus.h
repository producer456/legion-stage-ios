#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <cmath>

namespace Juno60 {

class Chorus
{
public:
    Chorus();
    ~Chorus();

    void prepare (double sampleRate, int maxBlockSize);

    // mode: 0=off, 1=I, 2=II, 3=I+II
    void setMode (int mode);

    void process (juce::AudioBuffer<float>& buffer);
    void reset();

private:
    // Triangle LFO: returns -1..+1
    static inline float triangleLFO (float phase)
    {
        return 4.0f * std::abs (phase - 0.5f) - 1.0f;
    }

    // Cubic Hermite interpolation for delay line reading
    inline float cubicInterp (const float* buf, int bufSize, float readPos) const
    {
        int i0 = static_cast<int> (readPos);
        float frac = readPos - static_cast<float> (i0);

        int im1 = (i0 - 1 + bufSize) % bufSize;
        int i1  = (i0 + 1) % bufSize;
        int i2  = (i0 + 2) % bufSize;
        i0 = i0 % bufSize;

        float y_1 = buf[im1];
        float y0  = buf[i0];
        float y1  = buf[i1];
        float y2  = buf[i2];

        float c0 = y0;
        float c1 = 0.5f * (y1 - y_1);
        float c2 = y_1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
        float c3 = 0.5f * (y2 - y_1) + 1.5f * (y0 - y1);

        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }

    double sampleRate = 44100.0;
    int mode = 0;

    // Delay buffers — one per channel (L/R)
    static constexpr int kMaxDelaySize = 2048;
    float delayL[kMaxDelaySize] {};
    float delayR[kMaxDelaySize] {};
    int delayLineSize = 0;
    int writePos = 0;

    // Two independent triangle LFO phases (for Chorus I and Chorus II rates)
    float lfoPhase1 = 0.0f;  // Chorus I LFO
    float lfoPhase2 = 0.0f;  // Chorus II LFO

    // One-pole low-pass filter state for BBD bandwidth simulation (~12kHz)
    float lpStateL = 0.0f;
    float lpStateR = 0.0f;
    float lpCoeff  = 0.0f;   // filter coefficient
};

} // namespace Juno60
