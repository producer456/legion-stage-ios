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

        if (points.size() == 1 || beat <= points.getFirst().beat) return points.getFirst().value;
        if (beat >= points.getLast().beat) return points.getLast().value;

        // Binary search for the interval containing beat
        int lo = 0, hi = points.size() - 2;
        while (lo < hi) {
            int mid = (lo + hi + 1) / 2;
            if (points[mid].beat <= beat) lo = mid;
            else hi = mid - 1;
        }

        float denom = (float)(points[lo + 1].beat - points[lo].beat);
        if (denom < 0.0001f) return points[lo].value;  // Same beat — return exact value
        float t = (float)(beat - points[lo].beat) / denom;
        return points[lo].value + (points[lo + 1].value - points[lo].value) * t;
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

    ClipSlot() = default;
    ClipSlot(ClipSlot&& other) noexcept
        : clip(std::move(other.clip)),
          audioClip(std::move(other.audioClip)),
          state(other.state.load()) {}
    ClipSlot& operator=(ClipSlot&& other) noexcept
    {
        clip = std::move(other.clip);
        audioClip = std::move(other.audioClip);
        state.store(other.state.load());
        return *this;
    }

    bool hasContent() const
    {
        if (clip != nullptr && clip->events.getNumEvents() > 0) return true;
        if (audioClip != nullptr && audioClip->samples.getNumSamples() > 0) return true;
        return false;
    }
};
