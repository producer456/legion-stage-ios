#pragma once

#include <JuceHeader.h>
#include <vector>
#include <functional>

class KeyLab88Mk2Controller;

// Virtual on-screen KeyLab 88 MkII for mapping debug.  Tap any control
// and the same MIDI message a real device would emit gets injected
// through KeyLab88Mk2Controller::injectMessage(), so every binding can
// be exercised without the hardware plugged in.
class KeyLab88DebugView : public juce::Component,
                          private juce::Timer
{
public:
    explicit KeyLab88DebugView(KeyLab88Mk2Controller& controller);
    ~KeyLab88DebugView() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void rebuildControls();

    enum class Style { Pad, Button, Encoder, BigKnob, Fader, SelectBtn };

    struct Control
    {
        juce::Rectangle<int> bounds;
        juce::String         label;
        juce::String         sub;          // e.g. "CC 22"
        Style                style = Style::Button;
        int                  kindIndex = -1;   // which fader/encoder/etc (0..8)
        uint8_t              cc        = 0;    // for encoders / faders / etc
        // For taps on simple buttons / pads, the message is fixed.
        std::function<juce::MidiMessage(juce::Point<int>)> makeMessage;
    };

    KeyLab88Mk2Controller& controller;
    std::vector<Control>   controls;

    int          lastFiredIdx = -1;
    juce::int64  lastFiredAt  = 0;
    int          dragControlIdx = -1;
    juce::Point<int> lastDragPos;       // for relative encoder/big-knob drag

    // Visible state per control type (for drawing the cap / pointer).
    // Encoder accumulator wraps 0..1 via a counter of clicks; faders are
    // straight 0..1.  Indices match the per-bank order (0..8 for E1..E9 etc).
    float faderValue [9] = { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
    int   encoderTicks[9] = {0,0,0,0,0,0,0,0,0};   // signed cumulative
    int   bigKnobTicks  = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KeyLab88DebugView)
};
