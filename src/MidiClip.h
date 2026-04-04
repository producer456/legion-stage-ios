#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <memory>

struct MidiClip
{
    juce::MidiMessageSequence events;  // timestamps in beats (relative to clip start)
    double lengthInBeats = 4.0;        // default 1 bar at 4/4
    double timelinePosition = 0.0;     // where this clip sits on the arrangement timeline (in beats)
};

struct AutomationPoint
{
    double beat = 0.0;
    float value = 0.0f;
};

struct AutomationLane
{
    int parameterIndex = -1;
    juce::String parameterName;
    juce::Array<AutomationPoint> points;

    float getValueAtBeat(double beat) const
    {
        if (points.isEmpty()) return -1.0f; // no data

        // Before first point
        if (beat <= points.getFirst().beat) return points.getFirst().value;
        // After last point
        if (beat >= points.getLast().beat) return points.getLast().value;

        // Linear interpolation between points
        for (int i = 0; i < points.size() - 1; ++i)
        {
            if (beat >= points[i].beat && beat < points[i + 1].beat)
            {
                double t = (beat - points[i].beat) / (points[i + 1].beat - points[i].beat);
                return points[i].value + static_cast<float>(t) * (points[i + 1].value - points[i].value);
            }
        }
        return points.getLast().value;
    }
};

// Audio clip — stores recorded audio samples
struct AudioClip
{
    juce::AudioBuffer<float> samples;   // stereo audio data at recording sample rate
    double sampleRate = 44100.0;
    double lengthInBeats = 4.0;
    double timelinePosition = 0.0;
};

struct ClipSlot
{
    enum State { Empty, Stopped, Playing, Recording, Armed };

    std::unique_ptr<MidiClip> clip;
    std::unique_ptr<AudioClip> audioClip;
    std::atomic<State> state { Empty };

    bool hasContent() const
    {
        if (clip != nullptr && clip->events.getNumEvents() > 0) return true;
        if (audioClip != nullptr && audioClip->samples.getNumSamples() > 0) return true;
        return false;
    }
};
