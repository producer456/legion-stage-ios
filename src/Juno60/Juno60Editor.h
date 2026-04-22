#pragma once

#include <JuceHeader.h>

class BuiltinJuno60Processor;

//==============================================================================
// A vertical slider that springs back to a default value on mouse-up.
class SpringSlider : public juce::Slider
{
public:
    using Slider::Slider;
    void mouseUp (const juce::MouseEvent& e) override
    {
        Slider::mouseUp (e);
        if (springBack)
            setValue (springValue, juce::sendNotificationSync);
    }
    bool springBack = false;
    double springValue = 0.0;
};

//==============================================================================
// A group of TextButtons that acts as a radio selector for a Choice parameter.
class RadioButtonGroup : public juce::Component,
                         private juce::AudioProcessorValueTreeState::Listener
{
public:
    RadioButtonGroup (juce::AudioProcessorValueTreeState& apvts,
                      const juce::String& paramID,
                      const juce::StringArray& labels,
                      bool vertical = false);
    ~RadioButtonGroup() override;

    void resized() override;
    void parameterChanged (const juce::String& parameterID, float newValue) override;

private:
    void updateToggleState (int index);

    juce::AudioProcessorValueTreeState& apvtsRef;
    juce::String parameterID;
    juce::OwnedArray<juce::TextButton> buttons;
    bool isVertical;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RadioButtonGroup)
};

//==============================================================================
class Juno60Editor : public juce::AudioProcessorEditor
{
public:
    explicit Juno60Editor (BuiltinJuno60Processor&);
    ~Juno60Editor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    BuiltinJuno60Processor& processorRef;
    juce::AudioProcessorValueTreeState& apvts;

    // -- Custom LookAndFeel --------------------------------------------------
    struct Juno60LookAndFeel : public juce::LookAndFeel_V4
    {
        Juno60LookAndFeel();
        void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                               float sliderPos, float minSliderPos, float maxSliderPos,
                               juce::Slider::SliderStyle, juce::Slider&) override;
        void drawButtonBackground (juce::Graphics&, juce::Button&,
                                   const juce::Colour& backgroundColourToUse,
                                   bool shouldDrawButtonAsHighlighted,
                                   bool shouldDrawButtonAsDown) override;
        void drawButtonText (juce::Graphics&, juce::TextButton&,
                             bool shouldDrawButtonAsHighlighted,
                             bool shouldDrawButtonAsDown) override;
    };

    Juno60LookAndFeel junoLnf;

    // Colours
    static constexpr juce::uint32 creamBg        = 0xffE8E0D0;
    static constexpr juce::uint32 orangeStripe    = 0xffC4582A;
    static constexpr juce::uint32 darkBezel       = 0xff2A2018;
    static constexpr juce::uint32 darkCharcoal    = 0xff2C2C2C;
    static constexpr juce::uint32 grooveDark      = 0xff3A3530;
    static constexpr juce::uint32 grooveLight     = 0xffF0EBE0;

    // Background cache
    juce::Image cachedBackground;
    int cachedWidth  = 0;
    int cachedHeight = 0;
    void renderBackground (juce::Image& img, int w, int h);

    // -- Sliders --------------------------------------------------------------
    juce::Slider lfoRate, lfoDelay;
    juce::Slider dcoLfoDepth, dcoPwmAmount, noiseLevel;
    juce::Slider vcfFreq, vcfRes, vcfEnvAmount, vcfLfoAmount;
    juce::Slider vcaLevel;
    juce::Slider envAttack, envDecay, envSustain, envRelease;

    // -- Toggle buttons (Bool params) -----------------------------------------
    juce::TextButton dcoSawBtn  { "SAW" };
    juce::TextButton dcoPulseBtn { "PULSE" };
    juce::TextButton dcoSubBtn  { "SUB" };

    // -- Radio groups (Choice params) -----------------------------------------
    std::unique_ptr<RadioButtonGroup> dcoPwmSourceGroup;
    std::unique_ptr<RadioButtonGroup> dcoRangeGroup;
    std::unique_ptr<RadioButtonGroup> hpfModeGroup;
    std::unique_ptr<RadioButtonGroup> vcfKeyFollowGroup;
    std::unique_ptr<RadioButtonGroup> vcaModeGroup;
    std::unique_ptr<RadioButtonGroup> chorusModeGroup;

    // -- Slider attachments ---------------------------------------------------
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> lfoRateAtt, lfoDelayAtt;
    std::unique_ptr<SliderAttachment> dcoLfoDepthAtt, dcoPwmAmountAtt, noiseLevelAtt;
    std::unique_ptr<SliderAttachment> vcfFreqAtt, vcfResAtt, vcfEnvAmountAtt, vcfLfoAmountAtt;
    std::unique_ptr<SliderAttachment> vcaLevelAtt;
    std::unique_ptr<SliderAttachment> envAttackAtt, envDecayAtt, envSustainAtt, envReleaseAtt;

    // -- Button attachments ---------------------------------------------------
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<ButtonAttachment> dcoSawAtt, dcoPulseAtt, dcoSubAtt;

    // -- Pitch / Mod wheels ---------------------------------------------------
    SpringSlider pitchWheel;
    juce::Slider modWheel;

    // -- MIDI activity LED ----------------------------------------------------
    std::atomic<bool> midiActivity { false };

    // -- Preset selector ------------------------------------------------------
    juce::ComboBox presetSelector;

    // -- Helpers --------------------------------------------------------------
    void setupSlider (juce::Slider& s, const juce::String& suffix = {});
    void drawSectionDivider (juce::Graphics& g, int x, int top, int bottom) const;
    void drawAdsrCurve (juce::Graphics& g, juce::Rectangle<int> area) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Juno60Editor)
};
