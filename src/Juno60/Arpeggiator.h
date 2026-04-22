#pragma once

#include <vector>
#include <map>
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
    std::vector<int> heldNotes;
    std::map<int, float> noteVelocities;
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
