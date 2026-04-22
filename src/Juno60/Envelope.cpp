#include "Envelope.h"
#include <cmath>
#include <algorithm>

namespace Juno60 {

Envelope::Envelope() {}
Envelope::~Envelope() {}

void Envelope::prepare (double sr)
{
    sampleRate = sr;
    recalcCoeffs();
    reset();
}

void Envelope::recalcCoeffs()
{
    attackCoeff  = calcCoeff (attackSec * 1000.0f, static_cast<float> (sampleRate));
    decayCoeff   = calcCoeff (decaySec * 1000.0f, static_cast<float> (sampleRate));
    releaseCoeff = calcCoeff (releaseSec * 1000.0f, static_cast<float> (sampleRate));
}

void Envelope::setAttack (float seconds)
{
    attackSec = seconds;
    attackCoeff = calcCoeff (seconds * 1000.0f, static_cast<float> (sampleRate));
}

void Envelope::setDecay (float seconds)
{
    decaySec = seconds;
    decayCoeff = calcCoeff (seconds * 1000.0f, static_cast<float> (sampleRate));
}

void Envelope::setSustain (float level)
{
    sustainLevel = level;
}

void Envelope::setRelease (float seconds)
{
    releaseSec = seconds;
    releaseCoeff = calcCoeff (seconds * 1000.0f, static_cast<float> (sampleRate));
}

void Envelope::noteOn()
{
    output = 0.0f; // Clean retrigger (authentic Juno-60 behaviour)
    state = State::Attack;
}

void Envelope::noteOff()
{
    if (state != State::Idle)
        state = State::Release;
}

float Envelope::process()
{
    switch (state)
    {
        case State::Idle:
            return 0.0f;

        case State::Attack:
            // Exponential rise toward overshoot target (1.3)
            // output approaches kAttackTarget; we clamp and transition at 1.0
            output += attackCoeff * (kAttackTarget - output);
            if (output >= 1.0f)
            {
                output = 1.0f;
                state = State::Decay;
            }
            break;

        case State::Decay:
            // Exponential fall toward sustain level
            output += decayCoeff * (sustainLevel - output);
            // Snap to sustain when close enough
            if (std::abs (output - sustainLevel) < 1.0e-5f)
            {
                output = sustainLevel;
                state = State::Sustain;
            }
            break;

        case State::Sustain:
            output = sustainLevel;
            break;

        case State::Release:
            // Exponential fall toward 0
            output += releaseCoeff * (0.0f - output);
            // output -= releaseCoeff * output  (equivalent)
            if (output < 1.0e-3f)
            {
                output = 0.0f;
                state = State::Idle;
            }
            break;
    }

    return output;
}

bool Envelope::isActive() const
{
    return state != State::Idle;
}

void Envelope::reset()
{
    state = State::Idle;
    output = 0.0f;
}

} // namespace Juno60
