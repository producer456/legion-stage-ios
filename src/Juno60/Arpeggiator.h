#pragma once

#include <vector>
#include <array>
#include <algorithm>

namespace Juno60 {

class Arpeggiator
{
public:
    enum Mode { Up, Down, UpDown };

    struct ArpEvent
    {
        int noteOn = -1;
        int noteOff = -1;
        float velocity = 0.8f;
    };

    void setSampleRate (double sr);
    void setEnabled (bool on);
    void setMode (Mode m);
    void setOctaveRange (int octaves); // 1, 2, or 3
    void setRate (float hz);           // notes per second
    void setHold (bool on);

    void noteOn (int midiNote, float velocity = 0.8f);
    void noteOff (int midiNote);
    void allNotesOff();

    // Call per-sample. Returns noteOn/noteOff events, or -1 if no change.
    ArpEvent process();

    void reset();

private:
    // These are mutated from the audio thread (noteOn/noteOff/process), so they must
    // not allocate: heldNotes/sequence are reserved to max capacity in setSampleRate,
    // and per-note velocity is a fixed array indexed by MIDI note (was a std::map,
    // which allocated/freed a tree node on every note — on the realtime callback).
    std::vector<int> heldNotes;
    std::array<float, 128> noteVelocities {};
    std::vector<int> sequence;
    int currentIndex = 0;
    int currentNote = -1;
    double phase = 0.0;
    double phaseIncrement = 0.0;
    double sampleRate = 44100.0;
    float rateHz = 5.0f;
    bool enabled = false;
    bool holdMode = false;
    bool goingUp = true;
    Mode mode = Up;
    int octaveRange = 1;

    void rebuildSequence();
    int advanceIndex();
};

} // namespace Juno60
