#include "GainProcessor.h"
#include "LissajousComponent.h"
#include <cmath>

GainProcessor::GainProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

void GainProcessor::prepareToPlay(double /*sampleRate*/, int /*samplesPerBlock*/)
{
}

bool GainProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void GainProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midi*/)
{
    auto startTime = juce::Time::getHighResolutionTicks();

    float vol = volume.load();
    bool isMuted = muted.load();
    bool soloActive = (soloCount != nullptr && soloCount->load() > 0 && !soloed.load());

    float p = pan.load(); // -1.0 to 1.0

    // Equal power pan law
    float angle = (p + 1.0f) * 0.25f * juce::MathConstants<float>::pi; // 0 to pi/2
    float leftGain = std::cos(angle);
    float rightGain = std::sin(angle);

    float targetVol = vol * (isMuted ? 0.0f : 1.0f) * (soloActive ? 0.0f : 1.0f);
    float targetL = leftGain * targetVol;
    float targetR = rightGain * targetVol;

    // Smooth over the block to avoid clicks on volume/mute/solo changes
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        smoothedPanL += (targetL - smoothedPanL) * 0.01f;
        smoothedPanR += (targetR - smoothedPanR) * 0.01f;
        if (buffer.getNumChannels() >= 1)
            buffer.getWritePointer(0)[i] *= smoothedPanL;
        if (buffer.getNumChannels() >= 2)
            buffer.getWritePointer(1)[i] *= smoothedPanR;
    }

    // Measure peak levels (post-gain)
    float pkL = 0.0f, pkR = 0.0f;
    int numSamples = buffer.getNumSamples();

    if (numSamples > 0)
    {
        if (buffer.getNumChannels() >= 1)
        {
            const float* chanL = buffer.getReadPointer(0);
            for (int i = 0; i < numSamples; ++i)
                pkL = juce::jmax(pkL, std::abs(chanL[i]));
        }
        if (buffer.getNumChannels() >= 2)
        {
            const float* chanR = buffer.getReadPointer(1);
            for (int i = 0; i < numSamples; ++i)
                pkR = juce::jmax(pkR, std::abs(chanR[i]));
        }
    }

    // Smooth falloff — slow decay so UI (15Hz) can see it
    float prevL = peakLevelL.load();
    float prevR = peakLevelR.load();
    peakLevelL.store(juce::jmax(pkL, prevL * 0.995f));
    peakLevelR.store(juce::jmax(pkR, prevR * 0.995f));

    // CPU measurement
    auto endTime = juce::Time::getHighResolutionTicks();
    double seconds = juce::Time::highResolutionTicksToSeconds(endTime - startTime);
    double sr = getSampleRate();
    if (sr > 0.0 && numSamples > 0)
    {
        double blockDuration = static_cast<double>(numSamples) / sr;
        float cpu = static_cast<float>(seconds / blockDuration) * 100.0f;
        float prevCpu = cpuPercent.load();
        cpuPercent.store(prevCpu * 0.8f + cpu * 0.2f);
    }

    // Feed per-track Lissajous
    if (auto* lj = lissajousDisplay.load(); lj != nullptr && buffer.getNumChannels() >= 2)
        lj->pushSamples(buffer.getReadPointer(0), buffer.getReadPointer(1), numSamples);
}
