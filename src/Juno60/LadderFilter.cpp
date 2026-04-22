#include "LadderFilter.h"
#include <cmath>
#include <algorithm>

namespace Juno60 {

LadderFilter::LadderFilter() {}
LadderFilter::~LadderFilter() {}

void LadderFilter::prepare (double sr)
{
    sampleRate = sr;
    reset();
}

void LadderFilter::setCutoff (float freq)
{
    cutoff = std::clamp (freq, 20.0f, 20000.0f);
}

void LadderFilter::setResonance (float res)
{
    resonance = std::clamp (res, 0.0f, 1.0f);
    k = resonance * 4.0f;   // map 0..1 → 0..4 (self-oscillation at 4)
}

void LadderFilter::setEnvAmount (float amount)  { envAmount = amount; }
void LadderFilter::setLFOAmount (float amount)  { lfoAmount = amount; }
void LadderFilter::setKeyFollow (float amount)  { keyFollow = amount; }
void LadderFilter::setEnvModulation (float val)  { envValue = val; }
void LadderFilter::setLFOModulation (float val)  { lfoValue = val; }
void LadderFilter::setNoteFrequency (float freq) { noteFreq = freq; }

float LadderFilter::process (float input)
{
    // ---- calculate effective cutoff with modulation ----
    float modCutoff = cutoff;
    modCutoff += envAmount * envValue * 10000.0f;
    modCutoff += lfoAmount * lfoValue * 5000.0f;
    modCutoff += keyFollow * (noteFreq - 261.63f);
    modCutoff = std::clamp (modCutoff, 20.0f, 20000.0f);

    // ---- TPT integrator coefficient (Zavalishin) ----
    g = std::tan (static_cast<float> (juce::MathConstants<double>::pi) * modCutoff
                  / static_cast<float> (sampleRate));

    // ---- resonance feedback (4th stage → input, with tanh saturation) ----
    float feedback = fastTanh (stage[3] * k);
    float u = input - feedback;

    // ---- 4-pole cascade: each stage is a TPT one-pole with OTA tanh ----
    //   y[n] = y[n-1] + g * tanh(x - y[n-1])
    //   This is the Zavalishin/VA topology-preserving one-pole form
    //   with tanh() modelling the OTA transconductance saturation.
    for (int i = 0; i < 4; ++i)
    {
        stage[i] = stage[i] + g * fastTanh (u - stage[i]);
        u = stage[i];
    }

    return stage[3];
}

void LadderFilter::reset()
{
    for (auto& s : stage)
        s = 0.0f;
    g = 0.0f;
}

} // namespace Juno60
