#pragma once

#include <JuceHeader.h>
#include <set>
#include <map>
#include "DawLookAndFeel.h"

// On-screen touch/click piano keyboard.
// Sends note on/off via a callback. Supports multi-touch,
// dragging across keys, and octave shifting.
class TouchPianoComponent : public juce::Component
{
public:
    // Callback: (noteNumber, isNoteOn)
    std::function<void(int, bool)> onNote;

    TouchPianoComponent()
    {
        setWantsKeyboardFocus(false);
        setInterceptsMouseClicks(true, false);
    }

    ~TouchPianoComponent() override
    {
        for (int n : activeNotes)
            if (onNote) onNote(n, false);
        activeNotes.clear();
        touchNotes.clear();
    }

    void setOctave(int oct) { baseOctave = juce::jlimit(0, 7, oct); repaint(); }
    int getOctave() const { return baseOctave; }
    void octaveUp()   { setOctave(baseOctave + 1); }
    void octaveDown() { setOctave(baseOctave - 1); }

    void setNumOctaves(int n) { numOctaves = juce::jlimit(1, 4, n); repaint(); }
    int getNumOctaves() const { return numOctaves; }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        int totalWhite = numOctaves * 7;
        float keyW = bounds.getWidth() / static_cast<float>(totalWhite);
        float keyH = bounds.getHeight();
        float blackH = keyH * 0.6f;
        float blackW = keyW * 0.65f;

        // Get theme colors
        juce::Colour whiteKey(0xFFf0ece6);
        juce::Colour whitePressed(0xFFB0C4FF);
        juce::Colour blackKey(0xFF1a1a1a);
        juce::Colour blackPressed(0xFF4466AA);
        juce::Colour keyBorder(0xFF333333);
        juce::Colour labelCol(0xFF666666);

        if (auto* lnf = dynamic_cast<DawLookAndFeel*>(&getLookAndFeel()))
        {
            auto& theme = lnf->getTheme();
            whiteKey     = juce::Colour(0xfff0ece6);  // always ivory white
            whitePressed = juce::Colour(theme.lcdText).withAlpha(0.5f);
            blackKey     = juce::Colour(0xff1a1a1e);  // always near-black
            blackPressed = juce::Colour(theme.lcdText).withAlpha(0.7f);
            keyBorder    = juce::Colour(0xff404040);
            labelCol     = juce::Colour(theme.textSecondary);
        }

        // Draw white keys
        for (int i = 0; i < totalWhite; ++i)
        {
            float x = bounds.getX() + i * keyW;
            int note = whiteKeyToNote(i);
            bool pressed = activeNotes.count(note) > 0;

            g.setColour(pressed ? whitePressed : whiteKey);
            g.fillRect(x + 0.5f, 0.0f, keyW - 1.0f, keyH);
            g.setColour(keyBorder);
            g.drawRect(x, 0.0f, keyW, keyH, 0.5f);

            // Label C notes
            if (note % 12 == 0)
            {
                g.setColour(labelCol);
                g.setFont(10.0f);
                g.drawText("C" + juce::String(note / 12 - 1),
                           static_cast<int>(x), static_cast<int>(keyH - 16),
                           static_cast<int>(keyW), 14, juce::Justification::centred);
            }
        }

        // Draw black keys on top
        for (int i = 0; i < totalWhite - 1; ++i)
        {
            int noteInOctave = whiteKeyToNote(i) % 12;
            if (noteInOctave == 0 || noteInOctave == 2 || noteInOctave == 5 ||
                noteInOctave == 7 || noteInOctave == 9)
            {
                float x = bounds.getX() + (i + 1) * keyW - blackW * 0.5f;
                int note = whiteKeyToNote(i) + 1;
                bool pressed = activeNotes.count(note) > 0;

                g.setColour(pressed ? blackPressed : blackKey);
                g.fillRect(x, 0.0f, blackW, blackH);
                g.setColour(keyBorder.darker(0.5f));
                g.drawRect(x, 0.0f, blackW, blackH, 0.5f);
            }
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        handleTouch(e.source.getIndex(), e.position, true);
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        handleTouch(e.source.getIndex(), e.position, true);
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        releaseTouch(e.source.getIndex());
    }

private:
    int baseOctave = 3;
    int numOctaves = 2;
    std::set<int> activeNotes;
    std::map<int, int> touchNotes;  // source index → note currently held by that touch

    // Map white key index to MIDI note
    int whiteKeyToNote(int whiteIndex) const
    {
        static const int offsets[] = { 0, 2, 4, 5, 7, 9, 11 };
        int octave = whiteIndex / 7;
        int keyInOctave = whiteIndex % 7;
        return (baseOctave + octave + 1) * 12 + offsets[keyInOctave];
    }

    int noteAtPoint(juce::Point<float> pos) const
    {
        auto bounds = getLocalBounds().toFloat();
        int totalWhite = numOctaves * 7;
        float keyW = bounds.getWidth() / static_cast<float>(totalWhite);
        float blackH = bounds.getHeight() * 0.6f;
        float blackW = keyW * 0.65f;

        // Check black keys first (they're on top)
        if (pos.y < blackH)
        {
            for (int i = 0; i < totalWhite - 1; ++i)
            {
                int noteInOctave = whiteKeyToNote(i) % 12;
                if (noteInOctave == 0 || noteInOctave == 2 || noteInOctave == 5 ||
                    noteInOctave == 7 || noteInOctave == 9)
                {
                    float x = (i + 1) * keyW - blackW * 0.5f;
                    if (pos.x >= x && pos.x < x + blackW)
                        return whiteKeyToNote(i) + 1;
                }
            }
        }

        // White key
        int whiteIndex = static_cast<int>(pos.x / keyW);
        whiteIndex = juce::jlimit(0, totalWhite - 1, whiteIndex);
        return whiteKeyToNote(whiteIndex);
    }

    void handleTouch(int sourceIndex, juce::Point<float> pos, bool down)
    {
        int note = noteAtPoint(pos);
        if (note < 0 || note > 127) return;

        if (!down) return;

        // Check if this touch source already has a note
        auto it = touchNotes.find(sourceIndex);
        int prevNote = (it != touchNotes.end()) ? it->second : -1;

        if (note == prevNote) return;  // same note, nothing to do

        // Release previous note for this touch (drag to new key)
        if (prevNote >= 0)
        {
            // Only release if no other touch is also holding this note
            bool otherTouchHolding = false;
            for (auto& [src, n] : touchNotes)
                if (src != sourceIndex && n == prevNote) { otherTouchHolding = true; break; }

            if (!otherTouchHolding)
            {
                activeNotes.erase(prevNote);
                if (onNote) onNote(prevNote, false);
            }
        }

        // Press new note
        touchNotes[sourceIndex] = note;
        if (!activeNotes.count(note))
        {
            activeNotes.insert(note);
            if (onNote) onNote(note, true);
        }
        repaint();
    }

    void releaseTouch(int sourceIndex)
    {
        auto it = touchNotes.find(sourceIndex);
        if (it == touchNotes.end()) return;

        int note = it->second;
        touchNotes.erase(it);

        // Only send note-off if no other touch is holding the same note
        bool otherTouchHolding = false;
        for (auto& [src, n] : touchNotes)
            if (n == note) { otherTouchHolding = true; break; }

        if (!otherTouchHolding && activeNotes.count(note))
        {
            activeNotes.erase(note);
            if (onNote) onNote(note, false);
        }
        repaint();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TouchPianoComponent)
};
