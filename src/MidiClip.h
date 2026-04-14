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

    // Cached index for faster sequential lookups (audio thread calls in order)
    mutable int lastSearchIndex = 0;

    float getValueAtBeat(double beat) const
    {
        int n = points.size();
        if (n == 0) return -1.0f;
        if (beat <= points.getFirst().beat) return points.getFirst().value;
        if (beat >= points[n - 1].beat) return points[n - 1].value;

        // Fast path: check cached index first (sequential playback hits this)
        int idx = lastSearchIndex;
        if (idx >= 0 && idx < n - 1 && beat >= points[idx].beat && beat < points[idx + 1].beat)
        {
            // Cache hit — interpolate
        }
        else if (idx + 1 < n - 1 && beat >= points[idx + 1].beat && beat < points[idx + 2].beat)
        {
            // Next segment — advance cache
            idx = idx + 1;
            lastSearchIndex = idx;
        }
        else
        {
            // Binary search fallback
            int lo = 0, hi = n - 2;
            while (lo <= hi)
            {
                int mid = (lo + hi) / 2;
                if (beat < points[mid].beat)
                    hi = mid - 1;
                else if (beat >= points[mid + 1].beat)
                    lo = mid + 1;
                else
                {
                    idx = mid;
                    lastSearchIndex = idx;
                    break;
                }
            }
            if (lo > hi) return points[n - 1].value;
        }

        double t = (beat - points[idx].beat) / (points[idx + 1].beat - points[idx].beat);
        return points[idx].value + static_cast<float>(t) * (points[idx + 1].value - points[idx].value);
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
