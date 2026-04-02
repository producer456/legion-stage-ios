#pragma once

#include <JuceHeader.h>
#include <set>

class ChordDetector
{
public:
    void noteOn(int noteNumber)
    {
        activeNotes.insert(noteNumber);
        updateChord();
    }

    void noteOff(int noteNumber)
    {
        activeNotes.erase(noteNumber);
        updateChord();
    }

    void clear()
    {
        activeNotes.clear();
        currentChord = "";
    }

    juce::String getChordName() const { return currentChord; }
    const std::set<int>& getActiveNotes() const { return activeNotes; }

private:
    std::set<int> activeNotes;
    juce::String currentChord;

    static constexpr const char* noteNames[12] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };

    void updateChord()
    {
        if (activeNotes.empty())
        {
            currentChord = "";
            return;
        }

        if (activeNotes.size() == 1)
        {
            int note = *activeNotes.begin();
            currentChord = juce::String(noteNames[note % 12]) + juce::String(note / 12 - 1);
            return;
        }

        // Get pitch classes (0-11) sorted
        std::set<int> pitchClasses;
        for (int n : activeNotes)
            pitchClasses.insert(n % 12);

        if (pitchClasses.size() < 2)
        {
            // All same pitch class (octaves)
            int note = *activeNotes.begin();
            currentChord = juce::String(noteNames[note % 12]);
            return;
        }

        // Try each pitch class as root and match intervals
        juce::String bestChord;
        int bestScore = 0;

        for (int root : pitchClasses)
        {
            std::vector<int> intervals;
            for (int pc : pitchClasses)
            {
                int interval = (pc - root + 12) % 12;
                if (interval > 0)
                    intervals.push_back(interval);
            }
            std::sort(intervals.begin(), intervals.end());

            juce::String name = noteNames[root];
            int score = 0;

            // Match against known chord patterns
            if (matchIntervals(intervals, {4, 7}))             { score = 10; }                          // Major
            else if (matchIntervals(intervals, {3, 7}))        { score = 10; name += "m"; }             // Minor
            else if (matchIntervals(intervals, {4, 7, 11}))    { score = 12; name += "maj7"; }          // Major 7
            else if (matchIntervals(intervals, {3, 7, 10}))    { score = 12; name += "m7"; }            // Minor 7
            else if (matchIntervals(intervals, {4, 7, 10}))    { score = 12; name += "7"; }             // Dominant 7
            else if (matchIntervals(intervals, {3, 6}))        { score = 9;  name += "dim"; }           // Diminished
            else if (matchIntervals(intervals, {3, 6, 9}))     { score = 11; name += "dim7"; }          // Diminished 7
            else if (matchIntervals(intervals, {3, 6, 10}))    { score = 11; name += "m7b5"; }          // Half-dim
            else if (matchIntervals(intervals, {4, 8}))        { score = 9;  name += "aug"; }           // Augmented
            else if (matchIntervals(intervals, {4, 8, 11}))    { score = 11; name += "aug(maj7)"; }     // Aug maj7
            else if (matchIntervals(intervals, {4, 8, 10}))    { score = 11; name += "aug7"; }          // Aug 7
            else if (matchIntervals(intervals, {5, 7}))        { score = 8;  name += "sus4"; }          // Sus4
            else if (matchIntervals(intervals, {2, 7}))        { score = 8;  name += "sus2"; }          // Sus2
            else if (matchIntervals(intervals, {4, 7, 9}))     { score = 11; name += "6"; }             // Major 6
            else if (matchIntervals(intervals, {3, 7, 9}))     { score = 11; name += "m6"; }            // Minor 6
            else if (matchIntervals(intervals, {4, 7, 10, 2})) { score = 13; name += "9"; }             // Dominant 9
            else if (matchIntervals(intervals, {4, 7, 11, 2})) { score = 13; name += "maj9"; }          // Major 9
            else if (matchIntervals(intervals, {3, 7, 10, 2})) { score = 13; name += "m9"; }            // Minor 9
            else if (matchIntervals(intervals, {4, 7, 10, 5})) { score = 13; name += "11"; }            // Dominant 11
            else if (matchIntervals(intervals, {7}))            { score = 7;  name += "5"; }             // Power chord
            else if (matchIntervals(intervals, {4, 7, 2}))     { score = 11; name += "add9"; }          // Add 9
            else if (matchIntervals(intervals, {3, 7, 2}))     { score = 11; name += "m(add9)"; }       // Minor add 9
            else
                continue;

            if (score > bestScore)
            {
                bestScore = score;
                bestChord = name;
            }
        }

        currentChord = bestChord.isEmpty() ? notesString() : bestChord;
    }

    bool matchIntervals(const std::vector<int>& have, std::initializer_list<int> want) const
    {
        std::set<int> haveSet(have.begin(), have.end());
        for (int w : want)
            if (haveSet.find(w) == haveSet.end())
                return false;
        return true;
    }

    juce::String notesString() const
    {
        juce::String s;
        for (int n : activeNotes)
        {
            if (s.isNotEmpty()) s += " ";
            s += noteNames[n % 12];
        }
        return s;
    }
};
