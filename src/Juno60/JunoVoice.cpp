#include "JunoVoice.h"
#include <cmath>

namespace Juno60 {

uint64_t JunoVoice::globalNoteCounter = 0;

JunoVoice::JunoVoice() {}
JunoVoice::~JunoVoice() {}

void JunoVoice::prepare (double sr)
{
    sampleRate = sr;
    oscillator.prepare (sr);
    filter.prepare (sr);
    envelope.prepare (sr);
    lfo.prepare (sr);
}

void JunoVoice::noteOn (int midiNote, float vel)
{
    currentNote = midiNote;
    velocity = vel;
    noteHeld = true;
    noteAge = ++globalNoteCounter;

    updateFrequency();
    float freq = static_cast<float> (440.0 * std::pow (2.0, (midiNote - 69 + detuneSemitones) / 12.0));
    filter.setNoteFrequency (freq);

    oscillator.noteOn();
    envelope.noteOn();
    lfo.noteOn();

    // Reset LFO delay ramp
    lfoDelayRamp = 0.0f;
    if (lfoDelayMs > 0.0f)
        lfoDelayRampIncrement = 1.0f / (lfoDelayMs * 0.001f * static_cast<float> (sampleRate));
    else
        lfoDelayRampIncrement = 0.0f; // instant (ramp stays at 0, but we handle below)

    // Reset HPF state for new note
    hpfState = 0.0f;
    hpfPrevInput = 0.0f;
}

void JunoVoice::noteOff()
{
    noteHeld = false;
    envelope.noteOff();
    currentNote = -1;
}

float JunoVoice::process()
{
    if (!isActive())
        return 0.0f;

    // --- LFO with delay ramp ---
    float lfoOut = lfo.process();

    // Advance LFO delay ramp
    if (lfoDelayMs > 0.0f && lfoDelayRamp < 1.0f)
    {
        lfoDelayRamp += lfoDelayRampIncrement;
        if (lfoDelayRamp > 1.0f)
            lfoDelayRamp = 1.0f;
    }
    else if (lfoDelayMs <= 0.0f)
    {
        lfoDelayRamp = 1.0f;
    }

    float modulatedLfo = lfoOut * lfoDelayRamp;

    // --- DCO: Oscillator with LFO pitch modulation ---
    oscillator.setLFOModulation (modulatedLfo * lfoDepth);
    float oscOut = oscillator.process();

    // --- Mixer: add noise ---
    if (noiseLevel > 0.0f)
        oscOut += Oscillator::processNoise() * noiseLevel;

    // --- HPF ---
    float hpfOut = processHPF (oscOut, hpfMode);

    // --- Envelope (single ADSR for both filter and VCA) ---
    float envOut = envelope.process();

    // --- VCF: Filter with envelope and LFO modulation ---
    filter.setEnvModulation (envOut);
    filter.setLFOModulation (modulatedLfo);
    float filtered = filter.process (hpfOut);

    // --- VCA ---
    float ampGain = (vcaMode == 0) ? 1.0f : envOut; // gate vs envelope
    float output = filtered * ampGain * vcaLevel * velocity;

    return output;
}

bool JunoVoice::isActive() const
{
    return envelope.isActive();
}

void JunoVoice::reset()
{
    oscillator.reset();
    filter.reset();
    envelope.reset();
    lfo.reset();
    currentNote = -1;
    noteHeld = false;
    hpfState = 0.0f;
    hpfPrevInput = 0.0f;
    lfoDelayRamp = 0.0f;
}

float JunoVoice::processHPF (float input, int mode)
{
    if (mode == 0)
        return input;

    const float cutoffs[] = { 0.0f, 130.0f, 260.0f, 520.0f };
    float rc = 1.0f / (2.0f * static_cast<float> (M_PI) * cutoffs[mode]);
    float dt = 1.0f / static_cast<float> (sampleRate);
    float alpha = rc / (rc + dt);
    float out = alpha * (hpfState + input - hpfPrevInput);
    hpfPrevInput = input;
    hpfState = out;
    return out;
}

void JunoVoice::updateFrequency()
{
    if (currentNote >= 0)
    {
        float freq = static_cast<float> (440.0 * std::pow (2.0, (currentNote - 69 + detuneSemitones) / 12.0));
        oscillator.setFrequency (freq);
    }
}

// Parameter setters
void JunoVoice::setLFORate (float hz) { lfo.setRate (hz); }
void JunoVoice::setLFODelay (float seconds)
{
    lfoDelayMs = seconds * 1000.0f;
    lfo.setDelay (seconds);
}
void JunoVoice::setLFODepth (float depth) { lfoDepth = depth; }

void JunoVoice::setOscSawEnabled (bool on) { oscillator.setSawEnabled (on); }
void JunoVoice::setOscPulseEnabled (bool on) { oscillator.setPulseEnabled (on); }
void JunoVoice::setOscSubEnabled (bool on) { oscillator.setSubEnabled (on); }
void JunoVoice::setOscRange (int footage) { oscillator.setRange (footage); }
void JunoVoice::setPWMAmount (float amount) { oscillator.setPWMAmount (amount); }
void JunoVoice::setPWMSource (int source) { oscillator.setPWMSource (source); }

void JunoVoice::setFilterCutoff (float freq) { filter.setCutoff (freq); }
void JunoVoice::setFilterResonance (float res) { filter.setResonance (res); }
void JunoVoice::setFilterEnvAmount (float amount) { filter.setEnvAmount (amount); }
void JunoVoice::setFilterLFOAmount (float amount) { filter.setLFOAmount (amount); }
void JunoVoice::setFilterKeyFollow (float amount) { filter.setKeyFollow (amount); }

void JunoVoice::setVCAMode (int m) { vcaMode = m; }
void JunoVoice::setVCALevel (float level) { vcaLevel = level; }

void JunoVoice::setAttack (float val) { envelope.setAttack (val); }
void JunoVoice::setDecay (float val) { envelope.setDecay (val); }
void JunoVoice::setSustain (float val) { envelope.setSustain (val); }
void JunoVoice::setRelease (float val) { envelope.setRelease (val); }

void JunoVoice::setHPFMode (int m) { hpfMode = m; }
void JunoVoice::setNoiseLevel (float level) { noiseLevel = level; }

void JunoVoice::setDetune (float semitones)
{
    detuneSemitones = semitones;
    updateFrequency();
}

} // namespace Juno60
