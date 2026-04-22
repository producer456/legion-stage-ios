#include "Oscillator.h"
#include <cmath>

namespace Juno60 {

// Static PRNG state for noise (seeded to nonzero)
uint32_t Oscillator::noiseState = 0x12345678u;

Oscillator::Oscillator() {}
Oscillator::~Oscillator() {}

//==============================================================================
// PolyBLEP antialiasing correction
// t = phase position (0..1), dt = phase increment per sample
//==============================================================================
float Oscillator::polyBLEP (float t, float dt)
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt)
    {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

//==============================================================================
// Setup
//==============================================================================
void Oscillator::prepare (double sr)
{
    sampleRate = sr;
    reset();
}

void Oscillator::noteOn()
{
    subToggle = false;
}

void Oscillator::setSampleRate (double sr)
{
    sampleRate = sr;
    updatePhaseIncrement();
}

void Oscillator::setFrequency (float freq)
{
    frequency = freq;
    updatePhaseIncrement();
}

void Oscillator::setNote (int midiNote, int rangeFootage)
{
    int transpose = 0;
    if (rangeFootage == 16) transpose = -12;
    else if (rangeFootage == 4) transpose = 12;

    frequency = static_cast<float> (440.0 * std::pow (2.0, (midiNote - 69 + transpose) / 12.0));
    range = 8; // transpose already applied via semitones
    updatePhaseIncrement();
}

//==============================================================================
// Waveform enables / parameters
//==============================================================================
void Oscillator::setSawEnabled (bool enabled) { sawEnabled = enabled; }
void Oscillator::setPulseEnabled (bool enabled) { pulseEnabled = enabled; }
void Oscillator::setSubEnabled (bool enabled) { subEnabled = enabled; }

void Oscillator::setRange (int footage)
{
    range = footage;
    updatePhaseIncrement();
}

void Oscillator::setPulseWidth (float pw) { pulseWidth_ = pw; }
void Oscillator::setPWMSource (int source) { pwmSource = source; }
void Oscillator::setPWMAmount (float amount) { pwmAmount = amount; }
void Oscillator::setLFOModulation (float val) { lfoValue = val; }

//==============================================================================
// Individual waveform generators (do NOT advance phase)
//==============================================================================

float Oscillator::processSaw()
{
    float p = static_cast<float> (phase);
    float dt = static_cast<float> (phaseIncrement);

    // Naive saw: ramp from -1 to +1
    float saw = 2.0f * p - 1.0f;

    // PolyBLEP correction at the discontinuity (phase wraps at 1->0)
    saw -= polyBLEP (p, dt);

    return saw;
}

float Oscillator::processPulse (float pw)
{
    // Clamp pulse width
    pw = juce::jlimit (0.05f, 0.95f, pw);

    float p = static_cast<float> (phase);
    float dt = static_cast<float> (phaseIncrement);

    // Pulse = difference of two phase-offset saws
    // Saw1 at phase p, Saw2 at phase (p + pw) mod 1
    float saw1 = 2.0f * p - 1.0f;
    saw1 -= polyBLEP (p, dt);

    float p2 = p + pw;
    if (p2 >= 1.0f) p2 -= 1.0f;

    float saw2 = 2.0f * p2 - 1.0f;
    saw2 -= polyBLEP (p2, dt);

    // Pulse wave: difference of two saws, normalized
    float pulse = saw1 - saw2;

    // DC offset compensation: shift by (1 - 2*pw)
    pulse += (1.0f - 2.0f * pw);

    return pulse;
}

float Oscillator::processSub()
{
    // Sub oscillator: square wave one octave below
    // Driven by a toggle flip-flop that changes state on each saw reset
    return subToggle ? 1.0f : -1.0f;
}

float Oscillator::processNoise()
{
    // xorshift32 PRNG
    uint32_t x = noiseState;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    noiseState = x;

    // Convert to float in range -1..1
    // Use the top 24 bits for better distribution
    return static_cast<float> (static_cast<int32_t> (x)) / static_cast<float> (INT32_MAX);
}

//==============================================================================
// Combined process: mixes enabled waveforms, advances phase
//==============================================================================
float Oscillator::process()
{
    float output = 0.0f;

    // Compute effective pulse width with PWM
    float effectivePW = pulseWidth_;
    if (pwmSource == 0) // LFO
        effectivePW = 0.5f + pwmAmount * lfoValue * 0.5f;
    else // Manual
        effectivePW = 0.5f + (pwmAmount - 0.5f) * 0.5f;

    effectivePW = juce::jlimit (0.05f, 0.95f, effectivePW);

    if (sawEnabled)
        output += processSaw();

    if (pulseEnabled)
        output += processPulse (effectivePW);

    if (subEnabled)
        output += processSub();

    advancePhase();

    return output;
}

//==============================================================================
// Phase management
//==============================================================================
void Oscillator::advancePhase()
{
    phase += phaseIncrement;

    // Detect saw reset (phase wrap) for sub-oscillator toggle
    if (phase >= 1.0)
    {
        phase -= 1.0;
        // Toggle flip-flop for sub-oscillator (divides frequency by 2)
        subToggle = !subToggle;
    }
}

void Oscillator::reset()
{
    phase = 0.0;
    subToggle = false;
    prevSawReset = false;
}

void Oscillator::updatePhaseIncrement()
{
    double rangeMultiplier = 1.0;
    if (range == 16) rangeMultiplier = 0.5;
    else if (range == 4) rangeMultiplier = 2.0;

    phaseIncrement = (static_cast<double> (frequency) * rangeMultiplier) / sampleRate;
}

} // namespace Juno60
