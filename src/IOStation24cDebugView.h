#pragma once

#include <JuceHeader.h>
#include <vector>
#include <functional>

class IOStation24cController;

// Virtual on-screen ioStation 24c for mapping debug.  Tap any control
// and the same MIDI message a real device would emit gets injected
// through IOStation24cController::injectMessage(), so every binding
// can be exercised without the hardware plugged in.  Also displays
// the rolling MIDI-in log so you can sanity-check what the firmware
// actually sends in each operating mode.
class IOStation24cDebugView : public juce::Component,
                              private juce::Timer
{
public:
    explicit IOStation24cDebugView(IOStation24cController& controller);
    ~IOStation24cDebugView() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp  (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void rebuildControls();

    enum class Style { Button, RGBButton, Encoder, EncoderPush, Fader, Touch };

    struct Control
    {
        juce::Rectangle<int> bounds;
        juce::String         label;
        juce::String         sub;       // "id 0x5E" / "PB ch0" / etc.
        Style                style = Style::Button;
        uint8_t              id = 0;    // button ID or CC# (encoder)
        std::function<juce::MidiMessage(juce::Point<int>)> makeMessage;
    };

    void fireButton(const Control& c);

    IOStation24cController& controller;
    std::vector<Control>    controls;

    int          dragControlIdx = -1;
    juce::Point<int> lastDragPos;
    int          lastFiredIdx = -1;
    juce::int64  lastFiredAt  = 0;

    // Fader cap position (0..1) — driven by drag locally and by the
    // controller's getFaderValue() between drags so it tracks the
    // physical / virtual fader as it moves.
    float faderValue = 0.0f;
    bool  fakeTouching = false;     // true while dragging the on-screen cap

    // Encoder push: ID 0x20.  Encoder rotation: CC 0x3C signed delta.
    // Drag accumulator so a smooth drag translates to discrete clicks.
    float encoderAccum = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IOStation24cDebugView)
};
