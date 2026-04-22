#pragma once

#include <juce_core/juce_core.h>
#include <cmath>

namespace Juno60 {

class Envelope
{
public:
    Envelope();
    ~Envelope();

    void prepare (double sampleRate);

    void setAttack (float seconds);
    void setDecay (float seconds);
    void setSustain (float level);   // 0..1
    void setRelease (float seconds);

    void noteOn();
    void noteOff();

    float process();
    bool isActive() const;
    void reset();

private:
    // Calculate exponential coefficient from time in milliseconds
    static inline float calcCoeff (float timeMs, float sampleRate)
    {
        if (timeMs <= 0.0f) return 1.0f;
        return 1.0f - std::exp (-1.0f / (timeMs * 0.001f * sampleRate));
    }

    enum class State { Idle, Attack, Decay, Sustain, Release };

    double sampleRate = 44100.0;
    State state = State::Idle;
    float output = 0.0f;

    // Raw parameter values (in seconds)
    float attackSec  = 0.01f;
    float decaySec   = 0.3f;
    float sustainLevel = 0.8f;
    float releaseSec = 0.3f;

    // Exponential coefficients
    float attackCoeff  = 0.0f;
    float decayCoeff   = 0.0f;
    float releaseCoeff = 0.0f;

    // Overshoot target for attack (exponential rise past 1.0)
    static constexpr float kAttackTarget = 1.3f;

    void recalcCoeffs();
};

} // namespace Juno60
