#include "SequencerEngine.h"
#include <cmath>

SequencerEngine::SequencerEngine() {}

void SequencerEngine::play()
{
    // If count-in is enabled and recording is armed, start count-in first
    if (countInEnabled.load() && recording.load() && !playing.load())
    {
        countingIn.store(true);
        countInBeatsRemaining.store(4.0); // 1 bar (4 beats)
        savedPosition = positionInBeats.load();
        countInFirstClick.store(true);
    }

    playFirstClick.store(true);
    playing.store(true);
}

void SequencerEngine::stop()
{
    playing.store(false);
    // Don't clear recording — let the user toggle it manually via the REC button
    countingIn.store(false);
    countInBeatsRemaining.store(0.0);
}

void SequencerEngine::toggleRecord()
{
    recording.store(!recording.load());
}

void SequencerEngine::setBpm(double newBpm)
{
    bpm.store(juce::jlimit(20.0, 300.0, newBpm));
}

void SequencerEngine::toggleCountIn()
{
    countInEnabled.store(!countInEnabled.load());
}

void SequencerEngine::toggleLoop()
{
    loopEnabled.store(!loopEnabled.load());
}

void SequencerEngine::setLoopRegion(double startBeat, double endBeat)
{
    if (!std::isfinite(startBeat) || !std::isfinite(endBeat)) return;
    if (startBeat < 0.0) startBeat = 0.0;
    if (endBeat <= startBeat) endBeat = startBeat + 1.0;

    // Note: the two stores are not jointly atomic. We order them so the
    // audio thread never sees a momentarily inverted (start > end) region.
    if (endBeat > loopEnd.load())
    {
        loopEnd.store(endBeat);      // widen first
        loopStart.store(startBeat);
    }
    else
    {
        loopStart.store(startBeat);  // shrink first
        loopEnd.store(endBeat);
    }
}

void SequencerEngine::clearLoopRegion()
{
    loopStart.store(0.0);
    loopEnd.store(0.0);
}

double SequencerEngine::advancePosition(int numSamples, double sampleRate)
{
    if (!playing.load())
        return 0.0;

    double currentBpm = bpm.load();
    double beatsPerSecond = currentBpm / 60.0;
    double beatsThisBlock = beatsPerSecond * (static_cast<double>(numSamples) / sampleRate);

    // Handle count-in
    if (countingIn.load())
    {
        countInBeatsRemaining.store(countInBeatsRemaining.load() - beatsThisBlock);

        // ALWAYS play metronome clicks during count-in (regardless of metronome toggle)
        // Fire first click immediately
        if (countInFirstClick.load())
        {
            countInFirstClick.store(false);
            clickFrequency.store(1500.0); // downbeat
            clickSamplesRemaining.store(static_cast<int>(sampleRate * 0.03));
            clickPhase.store(0.0);
        }
        else
        {
            double countInPos = 4.0 - countInBeatsRemaining.load();
            double prevPos = countInPos - beatsThisBlock;

            int oldBeat = static_cast<int>(std::floor(prevPos));
            int newBeat = static_cast<int>(std::floor(countInPos));

            if (newBeat > oldBeat)
            {
                bool isDownbeat = (newBeat % 4) == 0;
                clickFrequency.store(isDownbeat ? 1500.0 : 1000.0);
                clickSamplesRemaining.store(static_cast<int>(sampleRate * 0.03));
                clickPhase.store(0.0);
            }
        }

        if (countInBeatsRemaining.load() <= 0.0)
        {
            // Count-in finished — start actual playback/recording from saved position
            countingIn.store(false);
            positionInBeats.store(savedPosition);
        }

        return 0.0; // Don't advance the main position during count-in
    }

    double oldPos = positionInBeats.load();
    double newPos = oldPos + beatsThisBlock;

    // Metronome clicks during normal playback
    if (metronomeEnabled.load())
    {
        // Fire first click immediately when play starts on a beat
        if (playFirstClick.load())
        {
            playFirstClick.store(false);
            if (std::fmod(oldPos, 1.0) < 0.001)
            {
                int beat = static_cast<int>(std::floor(oldPos));
                bool isDownbeat = (beat % 4) == 0;
                clickFrequency.store(isDownbeat ? 1500.0 : 1000.0);
                clickSamplesRemaining.store(static_cast<int>(sampleRate * 0.02));
                clickPhase.store(0.0);
            }
        }

        int oldBeat = static_cast<int>(std::floor(oldPos));
        int newBeat = static_cast<int>(std::floor(newPos));

        if (newBeat > oldBeat)
        {
            bool isDownbeat = (newBeat % 4) == 0;
            clickFrequency.store(isDownbeat ? 1500.0 : 1000.0);
            clickSamplesRemaining.store(static_cast<int>(sampleRate * 0.02));
            clickPhase.store(0.0);
        }
    }
    else
    {
        playFirstClick.store(false);
    }

    // Loop wrap-around
    if (loopEnabled.load())
    {
        double ls = loopStart.load();
        double le = loopEnd.load();
        if (le > ls && newPos >= le)
            newPos = ls + std::fmod(newPos - ls, le - ls);
    }

    positionInBeats.store(newPos);
    return beatsThisBlock;
}

void SequencerEngine::toggleMetronome()
{
    metronomeEnabled.store(!metronomeEnabled.load());
}

void SequencerEngine::renderMetronome(juce::AudioBuffer<float>& buffer, int numSamples, double sampleRate)
{
    int remaining = clickSamplesRemaining.load();
    if (remaining <= 0) return;

    int samplesToRender = juce::jmin(remaining, numSamples);
    double freq = clickFrequency.load();
    double phaseInc = juce::MathConstants<double>::twoPi * freq / sampleRate;
    double phase = clickPhase.load();

    for (int s = 0; s < samplesToRender; ++s)
    {
        double envelope = static_cast<double>(remaining - s) /
                          static_cast<double>(remaining);
        float sample = static_cast<float>(std::sin(phase) * envelope * 0.4);
        phase += phaseInc;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.addSample(ch, s, sample);
    }

    clickPhase.store(phase);
    clickSamplesRemaining.store(remaining - samplesToRender);
}

void SequencerEngine::resetPosition()
{
    positionInBeats.store(0.0);
    clickSamplesRemaining.store(0);
    countingIn.store(false);
}
