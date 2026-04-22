#include "VoiceManager.h"

namespace Juno60 {

VoiceManager::VoiceManager (int /*numVoices*/)
{
}

VoiceManager::~VoiceManager() {}

void VoiceManager::prepare (double sampleRate)
{
    for (auto& voice : voices)
        voice.prepare (sampleRate);
}

void VoiceManager::handleMidiEvent (const juce::MidiMessage& message)
{
    if (message.isNoteOn())
    {
        if (message.getVelocity() == 0)
            noteOff (message.getNoteNumber()); // MIDI spec: velocity 0 = note-off
        else
            noteOn (message.getNoteNumber(), message.getFloatVelocity());
    }
    else if (message.isNoteOff())
    {
        noteOff (message.getNoteNumber());
    }
    else if (message.isAllNotesOff() || message.isAllSoundOff())
    {
        allNotesOff();
    }
}

void VoiceManager::noteOn (int midiNote, float velocity)
{
    if (unisonMode)
    {
        // All 6 voices play the same note with slight detune
        for (auto& voice : voices)
            voice.noteOn (midiNote, velocity);
        applyUnisonDetune();
    }
    else
    {
        // Check if this note is already playing; retrigger it
        auto* existing = findVoiceForNote (midiNote);
        if (existing != nullptr)
        {
            existing->noteOn (midiNote, velocity);
            return;
        }

        auto* voice = allocateVoice();
        if (voice != nullptr)
            voice->noteOn (midiNote, velocity);
    }
}

void VoiceManager::noteOff (int midiNote)
{
    if (unisonMode)
    {
        // Release all voices (they all play the same note)
        for (auto& voice : voices)
        {
            if (voice.getCurrentNote() == midiNote || voice.getCurrentNote() == -1)
                voice.noteOff();
        }
    }
    else
    {
        // Release all voices playing this note (should be at most one)
        for (auto& voice : voices)
        {
            if (voice.getCurrentNote() == midiNote)
                voice.noteOff();
        }
    }
}

void VoiceManager::allNotesOff()
{
    for (auto& voice : voices)
        voice.reset();
}

void VoiceManager::renderNextBlock (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    for (int i = startSample; i < startSample + numSamples; ++i)
    {
        float sample = 0.0f;

        for (auto& voice : voices)
        {
            if (voice.isActive())
                sample += voice.process();
        }

        // Scale down to avoid clipping with multiple voices
        sample *= 0.25f;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.addSample (ch, i, sample);
    }
}

void VoiceManager::reset()
{
    for (auto& voice : voices)
        voice.reset();
    roundRobinIndex = 0;
}

void VoiceManager::setUnison (bool on)
{
    if (unisonMode != on)
    {
        unisonMode = on;
        if (!on)
        {
            // Clear detune when leaving unison
            for (auto& voice : voices)
                voice.setDetune (0.0f);
        }
    }
}

JunoVoice* VoiceManager::findVoiceForNote (int midiNote)
{
    for (auto& voice : voices)
        if (voice.getCurrentNote() == midiNote)
            return &voice;
    return nullptr;
}

JunoVoice* VoiceManager::allocateVoice()
{
    // 1. Try to find a free (inactive) voice using round-robin
    for (int i = 0; i < kNumVoices; ++i)
    {
        int idx = (roundRobinIndex + i) % kNumVoices;
        if (!voices[static_cast<size_t> (idx)].isActive())
        {
            roundRobinIndex = (idx + 1) % kNumVoices;
            return &voices[static_cast<size_t> (idx)];
        }
    }

    // 2. No free voice — prefer stealing a releasing voice (oldest first)
    uint64_t oldestReleasingAge = UINT64_MAX;
    int oldestReleasingIdx = -1;
    for (int i = 0; i < kNumVoices; ++i)
    {
        if (voices[static_cast<size_t> (i)].isReleasing()
            && voices[static_cast<size_t> (i)].getNoteAge() < oldestReleasingAge)
        {
            oldestReleasingAge = voices[static_cast<size_t> (i)].getNoteAge();
            oldestReleasingIdx = i;
        }
    }

    if (oldestReleasingIdx >= 0)
    {
        roundRobinIndex = (oldestReleasingIdx + 1) % kNumVoices;
        return &voices[static_cast<size_t> (oldestReleasingIdx)];
    }

    // 3. All voices still sustaining — steal the oldest one
    uint64_t oldestAge = UINT64_MAX;
    int oldestIdx = 0;
    for (int i = 0; i < kNumVoices; ++i)
    {
        if (voices[static_cast<size_t> (i)].getNoteAge() < oldestAge)
        {
            oldestAge = voices[static_cast<size_t> (i)].getNoteAge();
            oldestIdx = i;
        }
    }

    roundRobinIndex = (oldestIdx + 1) % kNumVoices;
    return &voices[static_cast<size_t> (oldestIdx)];
}

void VoiceManager::applyUnisonDetune()
{
    // Spread voices evenly from -kUnisonDetuneSpread to +kUnisonDetuneSpread
    for (int i = 0; i < kNumVoices; ++i)
    {
        float t = (kNumVoices > 1)
                      ? (static_cast<float> (i) / static_cast<float> (kNumVoices - 1)) * 2.0f - 1.0f
                      : 0.0f;
        voices[static_cast<size_t> (i)].setDetune (t * kUnisonDetuneSpread);
    }
}

// Bulk parameter updates
void VoiceManager::setLFORate (float hz) { for (auto& v : voices) v.setLFORate (hz); }
void VoiceManager::setLFODelay (float s) { for (auto& v : voices) v.setLFODelay (s); }
void VoiceManager::setLFODepth (float d) { for (auto& v : voices) v.setLFODepth (d); }

void VoiceManager::setOscSawEnabled (bool on) { for (auto& v : voices) v.setOscSawEnabled (on); }
void VoiceManager::setOscPulseEnabled (bool on) { for (auto& v : voices) v.setOscPulseEnabled (on); }
void VoiceManager::setOscSubEnabled (bool on) { for (auto& v : voices) v.setOscSubEnabled (on); }
void VoiceManager::setOscRange (int f) { for (auto& v : voices) v.setOscRange (f); }
void VoiceManager::setPWMAmount (float a) { for (auto& v : voices) v.setPWMAmount (a); }
void VoiceManager::setPWMSource (int s) { for (auto& v : voices) v.setPWMSource (s); }

void VoiceManager::setFilterCutoff (float f) { for (auto& v : voices) v.setFilterCutoff (f); }
void VoiceManager::setFilterResonance (float r) { for (auto& v : voices) v.setFilterResonance (r); }
void VoiceManager::setFilterEnvAmount (float a) { for (auto& v : voices) v.setFilterEnvAmount (a); }
void VoiceManager::setFilterLFOAmount (float a) { for (auto& v : voices) v.setFilterLFOAmount (a); }
void VoiceManager::setFilterKeyFollow (float a) { for (auto& v : voices) v.setFilterKeyFollow (a); }

void VoiceManager::setVCAMode (int m) { for (auto& v : voices) v.setVCAMode (m); }
void VoiceManager::setVCALevel (float l) { for (auto& v : voices) v.setVCALevel (l); }

void VoiceManager::setAttack (float val) { for (auto& v : voices) v.setAttack (val); }
void VoiceManager::setDecay (float val) { for (auto& v : voices) v.setDecay (val); }
void VoiceManager::setSustain (float val) { for (auto& v : voices) v.setSustain (val); }
void VoiceManager::setRelease (float val) { for (auto& v : voices) v.setRelease (val); }

void VoiceManager::setHPFMode (int m) { for (auto& v : voices) v.setHPFMode (m); }
void VoiceManager::setNoiseLevel (float l) { for (auto& v : voices) v.setNoiseLevel (l); }

} // namespace Juno60
