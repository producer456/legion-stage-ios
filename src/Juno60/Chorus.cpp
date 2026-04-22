#include "Chorus.h"
#include <cmath>
#include <algorithm>

namespace Juno60 {

// Juno-60 Chorus measured parameters
static constexpr float kChorusRate1    = 0.513f;   // Hz — Chorus I LFO rate
static constexpr float kChorusRate2    = 0.863f;   // Hz — Chorus II LFO rate
static constexpr float kChorusDepth1   = 0.0005f;  // 0.5ms swing — Chorus I
static constexpr float kChorusDepth2   = 0.0015f;  // 1.5ms swing — Chorus II
static constexpr float kCenterDelay    = 0.0045f;  // 4.5ms center delay
static constexpr float kLPFilterCutoff = 12000.0f; // BBD bandwidth limit

Chorus::Chorus() {}
Chorus::~Chorus() {}

void Chorus::prepare (double sr, int /*maxBlockSize*/)
{
    sampleRate = sr;
    // ~10ms worth of samples, clamped to buffer size
    delayLineSize = std::min (static_cast<int> (sr * 0.01) + 4, kMaxDelaySize);

    // One-pole low-pass coefficient for BBD bandwidth (~12kHz)
    float w = 2.0f * juce::MathConstants<float>::pi * kLPFilterCutoff / static_cast<float> (sr);
    lpCoeff = w / (1.0f + w);

    reset();
}

void Chorus::setMode (int m) { mode = m; }

void Chorus::process (juce::AudioBuffer<float>& buffer)
{
    if (mode == 0)
        return;

    int numSamples  = buffer.getNumSamples();
    int numChannels = buffer.getNumChannels();

    // LFO increments per sample
    float phaseInc1 = static_cast<float> (kChorusRate1 / sampleRate);
    float phaseInc2 = static_cast<float> (kChorusRate2 / sampleRate);

    // Determine which LFOs are active and their depths (in samples)
    bool useLFO1 = (mode == 1 || mode == 3);
    bool useLFO2 = (mode == 2 || mode == 3);
    float depthSamples1 = static_cast<float> (kChorusDepth1 * sampleRate);
    float depthSamples2 = static_cast<float> (kChorusDepth2 * sampleRate);
    float centerSamples = static_cast<float> (kCenterDelay * sampleRate);

    for (int i = 0; i < numSamples; ++i)
    {
        // Sum input to mono
        float mono = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            mono += buffer.getSample (ch, i);
        mono /= static_cast<float> (numChannels);

        // Write mono into both delay lines
        delayL[writePos] = mono;
        delayR[writePos] = mono;

        // Compute triangle LFO values
        float lfo1val = useLFO1 ? triangleLFO (lfoPhase1) : 0.0f;
        float lfo2val = useLFO2 ? triangleLFO (lfoPhase2) : 0.0f;

        // Composite modulation signal (sum of active LFOs)
        float modL = 0.0f;
        float modR = 0.0f;

        if (useLFO1)
        {
            modL += lfo1val * depthSamples1;
            modR += (-lfo1val) * depthSamples1;  // 180° out of phase
        }
        if (useLFO2)
        {
            modL += lfo2val * depthSamples2;
            modR += (-lfo2val) * depthSamples2;  // 180° out of phase
        }

        // Delay read positions (center delay +/- modulation)
        float readPosL = static_cast<float> (writePos) - centerSamples - modL;
        float readPosR = static_cast<float> (writePos) - centerSamples - modR;

        // Wrap into valid range
        while (readPosL < 0.0f) readPosL += static_cast<float> (delayLineSize);
        while (readPosR < 0.0f) readPosR += static_cast<float> (delayLineSize);

        // Read with cubic interpolation
        float wetL = cubicInterp (delayL, delayLineSize, readPosL);
        float wetR = cubicInterp (delayR, delayLineSize, readPosR);

        // Low-pass filter on wet signal (BBD bandwidth)
        lpStateL += lpCoeff * (wetL - lpStateL);
        lpStateR += lpCoeff * (wetR - lpStateR);
        wetL = lpStateL;
        wetR = lpStateR;

        // 50/50 wet/dry mix, output stereo
        float outL = 0.5f * mono + 0.5f * wetL;
        float outR = 0.5f * mono + 0.5f * wetR;

        if (numChannels >= 2)
        {
            buffer.setSample (0, i, outL);
            buffer.setSample (1, i, outR);
        }
        else
        {
            buffer.setSample (0, i, 0.5f * mono + 0.5f * (wetL + wetR) * 0.5f);
        }

        // Advance write position
        writePos = (writePos + 1) % delayLineSize;

        // Advance LFO phases
        lfoPhase1 += phaseInc1;
        if (lfoPhase1 >= 1.0f) lfoPhase1 -= 1.0f;
        lfoPhase2 += phaseInc2;
        if (lfoPhase2 >= 1.0f) lfoPhase2 -= 1.0f;
    }
}

void Chorus::reset()
{
    std::fill (std::begin (delayL), std::end (delayL), 0.0f);
    std::fill (std::begin (delayR), std::end (delayR), 0.0f);
    writePos = 0;
    lfoPhase1 = 0.0f;
    lfoPhase2 = 0.25f; // offset so LFO2 starts at different phase
    lpStateL = 0.0f;
    lpStateR = 0.0f;
}

} // namespace Juno60
