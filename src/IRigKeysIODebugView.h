#pragma once

#include <JuceHeader.h>
#include <vector>
#include <functional>

class IRigKeysIOController;

// Virtual on-screen iRig Keys I/O 49 for mapping debug.  Same shape as
// the ioStation panel: tap controls to inject MIDI through the
// controller, plus a live MIDI-in tail at the bottom so you can see
// what the real hardware is sending.
class IRigKeysIODebugView : public juce::Component,
                            private juce::Timer
{
public:
    explicit IRigKeysIODebugView(IRigKeysIOController& controller);
    ~IRigKeysIODebugView() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void rebuildControls();

    enum class Style { Button, Encoder, EncoderPush, Pad };

    struct Control
    {
        juce::Rectangle<int> bounds;
        juce::String         label;
        juce::String         sub;
        Style                style = Style::Button;
        uint8_t              id = 0;        // CC# (encoders) or note (pads)
        std::function<juce::MidiMessage(juce::Point<int>)> makeMessage;
    };

    void fireButton(const Control& c);

    IRigKeysIOController& controller;
    std::vector<Control>  controls;

    int          dragControlIdx = -1;
    juce::Point<int> lastDragPos;
    int          lastFiredIdx = -1;
    juce::int64  lastFiredAt  = 0;

    // Encoder visual state — accumulates so the pointer rotates as the
    // user drags.  Same as ioStation panel.
    float encoderAccum[8] = {0,0,0,0,0,0,0,0};
    float dataEncoderAccum = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IRigKeysIODebugView)
};
