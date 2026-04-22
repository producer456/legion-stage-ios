#include "Arpeggiator.h"

namespace Juno60 {

void Arpeggiator::setSampleRate (double sr)
{
    sampleRate = sr;
    phaseIncrement = (double) rateHz / sampleRate;
}

void Arpeggiator::setEnabled (bool on)
{
    if (enabled != on)
    {
        enabled = on;
        if (! on)
        {
            // When disabling, reset arp state but keep held notes
            currentNote = -1;
            currentIndex = 0;
            phase = 0.0;
            goingUp = true;
        }
    }
}

void Arpeggiator::setMode (Mode m)
{
    if (mode != m)
    {
        mode = m;
        goingUp = true;
        rebuildSequence();
    }
}

void Arpeggiator::setOctaveRange (int octaves)
{
    octaves = std::max (1, std::min (3, octaves));
    if (octaveRange != octaves)
    {
        octaveRange = octaves;
        rebuildSequence();
    }
}

void Arpeggiator::setRate (float hz)
{
    rateHz = hz;
    phaseIncrement = (double) rateHz / sampleRate;
}

void Arpeggiator::setHold (bool on)
{
    holdMode = on;
}

void Arpeggiator::noteOn (int midiNote, float velocity)
{
    // Add note if not already held
    auto it = std::lower_bound (heldNotes.begin(), heldNotes.end(), midiNote);
    if (it == heldNotes.end() || *it != midiNote)
        heldNotes.insert (it, midiNote);

    noteVelocities[midiNote] = velocity;

    rebuildSequence();

    // If this is the first note, reset phase so it triggers immediately
    if (heldNotes.size() == 1)
    {
        phase = 1.0; // Will trigger on next process() call
        currentIndex = 0;
        goingUp = true;
    }
}

void Arpeggiator::noteOff (int midiNote)
{
    if (holdMode)
        return;

    auto it = std::lower_bound (heldNotes.begin(), heldNotes.end(), midiNote);
    if (it != heldNotes.end() && *it == midiNote)
        heldNotes.erase (it);

    noteVelocities.erase (midiNote);

    rebuildSequence();

    if (heldNotes.empty())
    {
        currentIndex = 0;
        goingUp = true;
    }
}

void Arpeggiator::allNotesOff()
{
    heldNotes.clear();
    noteVelocities.clear();
    sequence.clear();
    currentIndex = 0;
    currentNote = -1;
    phase = 0.0;
    goingUp = true;
}

Arpeggiator::ArpEvent Arpeggiator::process()
{
    ArpEvent event;

    if (! enabled || sequence.empty())
    {
        // If we had a note playing and now sequence is empty, send noteOff
        if (currentNote != -1 && sequence.empty())
        {
            event.noteOff = currentNote;
            currentNote = -1;
        }
        return event;
    }

    phase += phaseIncrement;

    if (phase >= 1.0)
    {
        phase -= 1.0;

        int nextNote = sequence[(size_t) currentIndex];
        advanceIndex();

        // Look up velocity for the base note (un-transposed by octave)
        int baseNote = nextNote % 12;
        float vel = 0.8f;
        for (auto& [n, v] : noteVelocities)
        {
            if (n % 12 == baseNote)
            {
                vel = v;
                break;
            }
        }

        if (nextNote != currentNote)
        {
            if (currentNote != -1)
                event.noteOff = currentNote;
            event.noteOn = nextNote;
            event.velocity = vel;
            currentNote = nextNote;
        }
        else
        {
            // Retrigger same note: send off then on
            event.noteOff = currentNote;
            event.noteOn = nextNote;
            event.velocity = vel;
            currentNote = nextNote;
        }
    }

    return event;
}

void Arpeggiator::reset()
{
    heldNotes.clear();
    noteVelocities.clear();
    sequence.clear();
    currentIndex = 0;
    currentNote = -1;
    phase = 0.0;
    goingUp = true;
}

void Arpeggiator::rebuildSequence()
{
    sequence.clear();

    if (heldNotes.empty())
        return;

    // Build base sequence (heldNotes is already sorted ascending)
    for (int oct = 0; oct < octaveRange; ++oct)
        for (auto note : heldNotes)
            sequence.push_back (note + oct * 12);

    // Clamp to valid MIDI range
    sequence.erase (
        std::remove_if (sequence.begin(), sequence.end(),
                        [] (int n) { return n > 127; }),
        sequence.end());

    if (sequence.empty())
        return;

    // For Down mode, reverse the sequence
    if (mode == Down)
        std::reverse (sequence.begin(), sequence.end());

    // For UpDown, the sequence stays ascending; direction is handled in advanceIndex

    // Keep currentIndex in bounds
    if (currentIndex >= (int) sequence.size())
        currentIndex = 0;
}

int Arpeggiator::advanceIndex()
{
    if (sequence.empty())
        return 0;

    if (mode == UpDown && sequence.size() > 1)
    {
        if (goingUp)
        {
            currentIndex++;
            if (currentIndex >= (int) sequence.size())
            {
                currentIndex = (int) sequence.size() - 2;
                goingUp = false;
                if (currentIndex < 0)
                    currentIndex = 0;
            }
        }
        else
        {
            currentIndex--;
            if (currentIndex < 0)
            {
                currentIndex = 1;
                goingUp = true;
                if (currentIndex >= (int) sequence.size())
                    currentIndex = 0;
            }
        }
    }
    else
    {
        currentIndex++;
        if (currentIndex >= (int) sequence.size())
            currentIndex = 0;
    }

    return currentIndex;
}

} // namespace Juno60
