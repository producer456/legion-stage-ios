#include "Juno60Editor.h"
#include "../BuiltinJuno60Processor.h"

//==============================================================================
// RadioButtonGroup
//==============================================================================
RadioButtonGroup::RadioButtonGroup (juce::AudioProcessorValueTreeState& apvts,
                                    const juce::String& paramID,
                                    const juce::StringArray& labels,
                                    bool vertical)
    : apvtsRef (apvts), parameterID (paramID), isVertical (vertical)
{
    for (int i = 0; i < labels.size(); ++i)
    {
        auto* btn = buttons.add (new juce::TextButton (labels[i]));
        btn->setClickingTogglesState (false);
        btn->setRadioGroupId (paramID.hashCode());
        addAndMakeVisible (btn);

        btn->onClick = [this, i]
        {
            if (auto* param = apvtsRef.getParameter (parameterID))
            {
                float normVal = param->convertTo0to1 (static_cast<float> (i));
                param->beginChangeGesture();
                param->setValueNotifyingHost (normVal);
                param->endChangeGesture();
            }
        };
    }

    apvtsRef.addParameterListener (parameterID, this);

    // Initial state
    if (auto* param = apvtsRef.getRawParameterValue (parameterID))
        updateToggleState (static_cast<int> (param->load()));
}

RadioButtonGroup::~RadioButtonGroup()
{
    apvtsRef.removeParameterListener (parameterID, this);
}

void RadioButtonGroup::resized()
{
    auto area = getLocalBounds();
    int n = buttons.size();
    if (n == 0) return;

    if (isVertical)
    {
        int h = area.getHeight() / n;
        for (auto* btn : buttons)
            btn->setBounds (area.removeFromTop (h));
    }
    else
    {
        int w = area.getWidth() / n;
        for (auto* btn : buttons)
            btn->setBounds (area.removeFromLeft (w));
    }
}

void RadioButtonGroup::parameterChanged (const juce::String&, float newValue)
{
    int idx = static_cast<int> (std::round (newValue));
    juce::MessageManager::callAsync ([this, idx] { updateToggleState (idx); });
}

void RadioButtonGroup::updateToggleState (int index)
{
    for (int i = 0; i < buttons.size(); ++i)
        buttons[i]->setToggleState (i == index, juce::dontSendNotification);
}

//==============================================================================
// Juno60LookAndFeel
//==============================================================================
Juno60Editor::Juno60LookAndFeel::Juno60LookAndFeel()
{
    setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff4A4A4A));
    setColour (juce::TextButton::buttonOnColourId,  juce::Colour (0xffD4682B));
    setColour (juce::TextButton::textColourOnId,    juce::Colours::white);
    setColour (juce::TextButton::textColourOffId,   juce::Colour (0xff999999));
    setColour (juce::Slider::backgroundColourId,    juce::Colour (0xff2A2A2A));
    setColour (juce::Slider::trackColourId,         juce::Colour (0xff2A2A2A));
    setColour (juce::Slider::thumbColourId,         juce::Colour (0xffD8D8D8));
}

void Juno60Editor::Juno60LookAndFeel::drawLinearSlider (
    juce::Graphics& g, int x, int y, int width, int height,
    float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
    juce::Slider::SliderStyle, juce::Slider&)
{
    auto centreX = (float) x + (float) width * 0.5f;
    float trackW = 4.0f;

    // Shadow behind track
    g.setColour (juce::Colour (0xff000000));
    g.fillRoundedRectangle (centreX - trackW * 0.5f - 0.5f, (float) y - 0.5f,
                            trackW + 1.0f, (float) height + 1.0f, 1.5f);

    // Track groove
    g.setColour (juce::Colour (0xff2A2A2A));
    g.fillRoundedRectangle (centreX - trackW * 0.5f, (float) y,
                            trackW, (float) height, 1.0f);

    // Inner shadow edges on track
    g.setColour (juce::Colour (0x40000000));
    g.drawVerticalLine ((int)(centreX - trackW * 0.5f), (float) y, (float)(y + height));
    g.setColour (juce::Colour (0x20FFFFFF));
    g.drawVerticalLine ((int)(centreX + trackW * 0.5f), (float) y, (float)(y + height));

    // Scale marks: "10" at top, "0" at bottom
    g.setColour (juce::Colour (0xff2C2C2C));
    g.setFont (juce::FontOptions (8.0f));
    g.drawText ("10", (int)(centreX + trackW + 2), y, 18, 12, juce::Justification::centredLeft, false);
    g.drawText ("0",  (int)(centreX + trackW + 2), y + height - 12, 18, 12, juce::Justification::centredLeft, false);

    // Thumb: rectangular white/grey cap
    float thumbW = 34.0f;
    float thumbH = 18.0f;
    float thumbY = sliderPos - thumbH * 0.5f;
    float thumbX = centreX - thumbW * 0.5f;

    // Drop shadow below thumb
    g.setColour (juce::Colour (0x30000000));
    g.fillRoundedRectangle (thumbX + 1.0f, thumbY + 2.0f, thumbW, thumbH, 3.0f);

    // Thumb gradient (warm taupe matching real hardware)
    juce::ColourGradient thumbGrad (juce::Colour (0xffC8BEB4), thumbX, thumbY,
                                    juce::Colour (0xff8A8078), thumbX, thumbY + thumbH, false);
    g.setGradientFill (thumbGrad);
    g.fillRoundedRectangle (thumbX, thumbY, thumbW, thumbH, 3.0f);

    // Top highlight
    g.setColour (juce::Colour (0x55FFFFFF));
    g.fillRoundedRectangle (thumbX + 1.0f, thumbY + 1.0f, thumbW - 2.0f, thumbH * 0.35f, 2.0f);

    // Centre grip line
    g.setColour (juce::Colour (0xff555555));
    float gripY = thumbY + thumbH * 0.5f;
    g.drawHorizontalLine ((int) gripY, thumbX + 6.0f, thumbX + thumbW - 6.0f);

    // Thumb outline
    g.setColour (juce::Colour (0xff888888));
    g.drawRoundedRectangle (thumbX, thumbY, thumbW, thumbH, 3.0f, 0.8f);
}

void Juno60Editor::Juno60LookAndFeel::drawButtonBackground (
    juce::Graphics& g, juce::Button& button,
    const juce::Colour&, bool highlighted, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    bool isOn = button.getToggleState();
    auto baseColour = isOn
                          ? button.findColour (juce::TextButton::buttonOnColourId)
                          : button.findColour (juce::TextButton::buttonColourId);
    if (highlighted || down)
        baseColour = baseColour.brighter (0.12f);

    // Button body
    g.setColour (baseColour);
    g.fillRoundedRectangle (bounds, 3.0f);

    // Subtle bevel
    g.setColour (juce::Colour (0x20FFFFFF));
    g.drawHorizontalLine ((int)(bounds.getY() + 1.5f), bounds.getX() + 2.0f, bounds.getRight() - 2.0f);

    g.setColour (juce::Colour (0x30000000));
    g.drawRoundedRectangle (bounds, 3.0f, 0.8f);

    // LED dot above button (inside the button area, top portion)
    float ledRadius = 3.5f;
    float ledX = bounds.getCentreX();
    float ledY = bounds.getY() + 6.0f;

    if (isOn)
    {
        // Subtle pulse based on time
        float pulse = 0.85f + 0.15f * std::sin ((float) (juce::Time::getMillisecondCounterHiRes() * 0.003));

        // Glow (modulated by pulse)
        auto glowAlpha = (juce::uint8) (0x40 * pulse);
        g.setColour (juce::Colour (0x00FF6030).withAlpha (glowAlpha));
        g.fillEllipse (ledX - ledRadius * 2.0f, ledY - ledRadius * 2.0f,
                        ledRadius * 4.0f, ledRadius * 4.0f);
        // LED
        g.setColour (juce::Colour (0xffFF4422));
        g.fillEllipse (ledX - ledRadius, ledY - ledRadius,
                        ledRadius * 2.0f, ledRadius * 2.0f);
        // Specular
        g.setColour (juce::Colour (0x80FFFFFF));
        g.fillEllipse (ledX - ledRadius * 0.4f, ledY - ledRadius * 0.6f,
                        ledRadius * 0.8f, ledRadius * 0.6f);
    }
    else
    {
        g.setColour (juce::Colour (0xff551515));
        g.fillEllipse (ledX - ledRadius, ledY - ledRadius,
                        ledRadius * 2.0f, ledRadius * 2.0f);
    }
}

void Juno60Editor::Juno60LookAndFeel::drawButtonText (
    juce::Graphics& g, juce::TextButton& button,
    bool /*highlighted*/, bool /*down*/)
{
    auto textColour = button.getToggleState()
                          ? button.findColour (juce::TextButton::textColourOnId)
                          : button.findColour (juce::TextButton::textColourOffId);

    auto text = button.getButtonText();
    auto area = button.getLocalBounds().reduced (4);
    area.removeFromTop (10); // below LED

    // Draw waveform icons for oscillator buttons
    if (text == "SAW")
    {
        juce::Path saw;
        float w = area.getWidth() * 0.6f, h = area.getHeight() * 0.5f;
        float cx = (float) area.getCentreX(), cy = (float) area.getCentreY();
        saw.startNewSubPath (cx - w / 2, cy + h / 2);
        saw.lineTo (cx + w / 2, cy - h / 2);
        saw.lineTo (cx + w / 2, cy + h / 2);
        g.setColour (textColour);
        g.strokePath (saw, juce::PathStrokeType (1.5f));
        return;
    }

    if (text == "PULSE")
    {
        juce::Path pulse;
        float w = area.getWidth() * 0.6f, h = area.getHeight() * 0.5f;
        float cx = (float) area.getCentreX(), cy = (float) area.getCentreY();
        float left = cx - w / 2, right = cx + w / 2;
        float top = cy - h / 2, bottom = cy + h / 2;
        pulse.startNewSubPath (left, bottom);
        pulse.lineTo (left, top);
        pulse.lineTo (cx, top);
        pulse.lineTo (cx, bottom);
        pulse.lineTo (right, bottom);
        g.setColour (textColour);
        g.strokePath (pulse, juce::PathStrokeType (1.5f));
        return;
    }

    if (text == "SUB")
    {
        juce::Path sub;
        float w = area.getWidth() * 0.7f, h = area.getHeight() * 0.5f;
        float cx = (float) area.getCentreX(), cy = (float) area.getCentreY();
        float left = cx - w / 2, right = cx + w / 2;
        float top = cy - h / 2, bottom = cy + h / 2;
        // Wider square wave (one octave lower)
        sub.startNewSubPath (left, bottom);
        sub.lineTo (left, top);
        sub.lineTo (right, top);
        sub.lineTo (right, bottom);
        sub.lineTo (left + w, bottom);
        g.setColour (textColour);
        g.strokePath (sub, juce::PathStrokeType (1.5f));
        return;
    }

    // Default: draw text
    g.setColour (textColour);
    g.setFont (juce::FontOptions (9.0f).withStyle ("Bold"));
    g.drawFittedText (text, area, juce::Justification::centred, 1);
}

//==============================================================================
// Editor
//==============================================================================
Juno60Editor::Juno60Editor (BuiltinJuno60Processor& p)
    : AudioProcessorEditor (&p), processorRef (p), apvts (p.getAPVTS())
{
    setLookAndFeel (&junoLnf);

    // -- Setup sliders --------------------------------------------------------
    auto setupSliderFn = [this] (juce::Slider& s)
    {
        s.setSliderStyle (juce::Slider::LinearVertical);
        s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        addAndMakeVisible (s);
    };

    setupSliderFn (lfoRate);
    setupSliderFn (lfoDelay);
    setupSliderFn (dcoLfoDepth);
    setupSliderFn (dcoPwmAmount);
    setupSliderFn (noiseLevel);
    setupSliderFn (vcfFreq);
    setupSliderFn (vcfRes);
    setupSliderFn (vcfEnvAmount);
    setupSliderFn (vcfLfoAmount);
    setupSliderFn (vcaLevel);
    setupSliderFn (envAttack);
    setupSliderFn (envDecay);
    setupSliderFn (envSustain);
    setupSliderFn (envRelease);

    // -- Setup pitch / mod wheels ---------------------------------------------
    pitchWheel.setSliderStyle (juce::Slider::LinearVertical);
    pitchWheel.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    pitchWheel.setRange (-1.0, 1.0, 0.01);
    pitchWheel.setValue (0.0);
    pitchWheel.setDoubleClickReturnValue (true, 0.0);
    pitchWheel.springBack = true;
    pitchWheel.springValue = 0.0;
    addAndMakeVisible (pitchWheel);

    modWheel.setSliderStyle (juce::Slider::LinearVertical);
    modWheel.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    modWheel.setRange (0.0, 1.0, 0.01);
    modWheel.setValue (0.0);
    addAndMakeVisible (modWheel);

    pitchWheel.onValueChange = [this]
    {
        int bendValue = (int) ((pitchWheel.getValue() + 1.0) * 8191.5);
        juce::ignoreUnused (bendValue);
    };

    modWheel.onValueChange = [this]
    {
        int cc1 = (int) (modWheel.getValue() * 127.0);
        juce::ignoreUnused (cc1);
    };

    // -- Setup toggle buttons -------------------------------------------------
    auto setupToggle = [this] (juce::TextButton& btn)
    {
        btn.setClickingTogglesState (true);
        addAndMakeVisible (btn);
    };

    setupToggle (dcoSawBtn);
    setupToggle (dcoPulseBtn);
    setupToggle (dcoSubBtn);

    // -- Radio button groups --------------------------------------------------
    dcoPwmSourceGroup = std::make_unique<RadioButtonGroup> (apvts, "dcoPwmSource",
        juce::StringArray { "LFO", "MAN" });
    addAndMakeVisible (*dcoPwmSourceGroup);

    dcoRangeGroup = std::make_unique<RadioButtonGroup> (apvts, "dcoRange",
        juce::StringArray { "16'", "8'", "4'" });
    addAndMakeVisible (*dcoRangeGroup);

    hpfModeGroup = std::make_unique<RadioButtonGroup> (apvts, "hpfMode",
        juce::StringArray { "0", "1", "2", "3" }, true);
    addAndMakeVisible (*hpfModeGroup);

    vcfKeyFollowGroup = std::make_unique<RadioButtonGroup> (apvts, "vcfKeyFollow",
        juce::StringArray { "0", "50", "100" });
    addAndMakeVisible (*vcfKeyFollowGroup);

    vcaModeGroup = std::make_unique<RadioButtonGroup> (apvts, "vcaMode",
        juce::StringArray { "GATE", "ENV" });
    addAndMakeVisible (*vcaModeGroup);

    chorusModeGroup = std::make_unique<RadioButtonGroup> (apvts, "chorusMode",
        juce::StringArray { "OFF", "I", "II", "I+II" });
    addAndMakeVisible (*chorusModeGroup);

    // -- Attachments (must be created after components) -----------------------
    lfoRateAtt      = std::make_unique<SliderAttachment> (apvts, "lfoRate",      lfoRate);
    lfoDelayAtt     = std::make_unique<SliderAttachment> (apvts, "lfoDelay",     lfoDelay);
    dcoLfoDepthAtt  = std::make_unique<SliderAttachment> (apvts, "dcoLfoDepth",  dcoLfoDepth);
    dcoPwmAmountAtt = std::make_unique<SliderAttachment> (apvts, "dcoPwmAmount", dcoPwmAmount);
    noiseLevelAtt   = std::make_unique<SliderAttachment> (apvts, "noiseLevel",   noiseLevel);
    vcfFreqAtt      = std::make_unique<SliderAttachment> (apvts, "vcfFreq",      vcfFreq);
    vcfResAtt       = std::make_unique<SliderAttachment> (apvts, "vcfRes",       vcfRes);
    vcfEnvAmountAtt = std::make_unique<SliderAttachment> (apvts, "vcfEnvAmount", vcfEnvAmount);
    vcfLfoAmountAtt = std::make_unique<SliderAttachment> (apvts, "vcfLfoAmount", vcfLfoAmount);
    vcaLevelAtt     = std::make_unique<SliderAttachment> (apvts, "vcaLevel",     vcaLevel);
    envAttackAtt    = std::make_unique<SliderAttachment> (apvts, "envAttack",    envAttack);
    envDecayAtt     = std::make_unique<SliderAttachment> (apvts, "envDecay",     envDecay);
    envSustainAtt   = std::make_unique<SliderAttachment> (apvts, "envSustain",   envSustain);
    envReleaseAtt   = std::make_unique<SliderAttachment> (apvts, "envRelease",   envRelease);

    dcoSawAtt   = std::make_unique<ButtonAttachment> (apvts, "dcoSaw",   dcoSawBtn);
    dcoPulseAtt = std::make_unique<ButtonAttachment> (apvts, "dcoPulse", dcoPulseBtn);
    dcoSubAtt   = std::make_unique<ButtonAttachment> (apvts, "dcoSub",   dcoSubBtn);

    // -- Preset selector ------------------------------------------------------
    addAndMakeVisible (presetSelector);
    for (int i = 0; i < BuiltinJuno60Processor::getNumPresets(); ++i)
        presetSelector.addItem (BuiltinJuno60Processor::getPresetName (i), i + 1);
    presetSelector.setSelectedId (1, juce::dontSendNotification);
    presetSelector.onChange = [this] {
        int idx = presetSelector.getSelectedId() - 1;
        if (idx >= 0)
            processorRef.loadPreset (idx);
    };
    presetSelector.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff3A3A3A));
    presetSelector.setColour (juce::ComboBox::textColourId, juce::Colours::white);
    presetSelector.setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff555555));

    // Set size LAST -- triggers resized() which needs all components to exist
    setResizable (true, true);
    setResizeLimits (700, 200, 2400, 800);
    setSize (1000, 360);
}

Juno60Editor::~Juno60Editor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void Juno60Editor::drawSectionDivider (juce::Graphics& g, int x, int top, int bottom) const
{
    // Recessed groove: dark line then light line for 3D effect
    g.setColour (juce::Colour (grooveDark));
    g.drawVerticalLine (x, (float) top, (float) bottom);
    g.setColour (juce::Colour (grooveLight));
    g.drawVerticalLine (x + 1, (float) top, (float) bottom);
}

//==============================================================================
void Juno60Editor::renderBackground (juce::Image& img, int w, int h)
{
    juce::Graphics g (img);

    int woodW     = 16;
    int bezelSize = 6;
    int stripeH   = 45;

    // Dark bezel border
    g.fillAll (juce::Colour (darkBezel));

    // === Wood end-cheek panels (left and right edges) ===
    auto leftWood  = juce::Rectangle<int> (0, 0, woodW, h);
    auto rightWood = juce::Rectangle<int> (w - woodW, 0, woodW, h);

    // Wood gradient (vertical grain effect)
    {
        juce::ColourGradient woodGrad (juce::Colour (0xff9B7B3A), 0.0f, 0.0f,
                                       juce::Colour (0xff7A5C2A), 0.0f, (float) h, false);
        g.setGradientFill (woodGrad);
        g.fillRect (leftWood);
        g.fillRect (rightWood);
    }

    // Simulate wood grain with thin horizontal lines
    {
        juce::Random rng (42);
        for (int y = 0; y < h; y += 2)
        {
            float alpha = rng.nextFloat() * 0.12f;
            g.setColour (juce::Colour (0xff5A3A1A).withAlpha (alpha));
            g.drawHorizontalLine (y, (float) leftWood.getX(), (float) leftWood.getRight());
            g.drawHorizontalLine (y, (float) rightWood.getX(), (float) rightWood.getRight());
        }
    }

    // Inner edge shadow on wood panels
    g.setColour (juce::Colour (0x40000000));
    g.drawVerticalLine (woodW, 0.0f, (float) h);
    g.drawVerticalLine (w - woodW - 1, 0.0f, (float) h);

    // === Panel area (inside wood cheeks) ===
    int panelLeft  = woodW + bezelSize;
    int panelRight = w - woodW - bezelSize;
    int panelTop   = bezelSize;
    int panelH     = h - bezelSize * 2;

    auto innerBounds = juce::Rectangle<int> (panelLeft, panelTop,
                                              panelRight - panelLeft, panelH);

    // Cream panel background (inside bezel, between wood cheeks)
    {
        juce::ColourGradient creamGrad (juce::Colour (0xffEDE5D6), 0.0f, (float) innerBounds.getY(),
                                        juce::Colour (0xffE0D8C8), 0.0f, (float) innerBounds.getBottom(), false);
        g.setGradientFill (creamGrad);
        g.fillRect (innerBounds);
    }

    // Orange accent stripe across the top (inside bezel)
    auto stripeArea = juce::Rectangle<int> (panelLeft, panelTop, innerBounds.getWidth(), stripeH);
    {
        juce::ColourGradient orangeGrad (juce::Colour (0xffD06030), 0.0f, (float) stripeArea.getY(),
                                         juce::Colour (0xffB04820), 0.0f, (float) stripeArea.getBottom(), false);
        g.setGradientFill (orangeGrad);
        g.fillRect (stripeArea);
    }

    // Subtle bottom highlight on stripe
    g.setColour (juce::Colour (0x18FFFFFF));
    g.drawHorizontalLine (stripeArea.getY() + 1, (float) stripeArea.getX(), (float) stripeArea.getRight());

    // Logo in stripe - left side
    g.setFont (juce::FontOptions (20.0f).withStyle ("Bold"));
    g.setColour (juce::Colours::white);
    g.drawText ("JUNO-60", stripeArea.withWidth (140).withX (stripeArea.getX() + 10),
                juce::Justification::centredLeft, false);
    // "PROGRAMMABLE POLYPHONIC SYNTHESIZER" subtitle
    g.setFont (juce::FontOptions (7.0f));
    g.setColour (juce::Colour (0xccFFFFFF));
    g.drawText ("PROGRAMMABLE POLYPHONIC SYNTHESIZER",
                stripeArea.withWidth (200).withX (stripeArea.getX() + 10).translated (0, 14),
                juce::Justification::centredLeft, false);

    // MIDI activity LED in the orange stripe area
    {
        float midiLedX = (float) (stripeArea.getRight() - 30);
        float midiLedY = (float) stripeArea.getCentreY();
        float midiLedR = 4.0f;
        bool active = midiActivity.load();
        if (active)
        {
            g.setColour (juce::Colour (0x40FF6030));
            g.fillEllipse (midiLedX - midiLedR * 2.0f, midiLedY - midiLedR * 2.0f,
                            midiLedR * 4.0f, midiLedR * 4.0f);
            g.setColour (juce::Colour (0xff00FF44));
            g.fillEllipse (midiLedX - midiLedR, midiLedY - midiLedR,
                            midiLedR * 2.0f, midiLedR * 2.0f);
        }
        else
        {
            g.setColour (juce::Colour (0xff1A3A1A));
            g.fillEllipse (midiLedX - midiLedR, midiLedY - midiLedR,
                            midiLedR * 2.0f, midiLedR * 2.0f);
        }
        g.setFont (juce::FontOptions (7.0f));
        g.setColour (juce::Colour (0xccFFFFFF));
        g.drawText ("MIDI", (int)(midiLedX - 16), (int)(midiLedY + midiLedR + 1), 32, 10,
                    juce::Justification::centred, false);
    }

    // === Wheels area labels (drawn in the background between left wood and LFO) ===
    {
        int wheelsLeft = woodW + bezelSize;
        int wheelsW = 50;
        int contentTop = stripeArea.getBottom() + 4;
        int contentBottom = innerBounds.getBottom();
        int wheelH = (contentBottom - contentTop) / 2 - 10;

        g.setFont (juce::FontOptions (8.0f).withStyle ("Bold"));
        g.setColour (juce::Colour (darkCharcoal));
        g.drawFittedText ("BEND",
                          juce::Rectangle<int> (wheelsLeft, contentTop + wheelH,
                                                wheelsW, 14),
                          juce::Justification::centred, 1);
        g.drawFittedText ("MOD",
                          juce::Rectangle<int> (wheelsLeft, contentTop + wheelH * 2 + 20,
                                                wheelsW, 14),
                          juce::Justification::centred, 1);
    }

    // Section layout proportions (applied to the area AFTER the wheels strip)
    int wheelsStripW = 50;
    auto sectionArea = juce::Rectangle<int> (panelLeft + wheelsStripW, innerBounds.getY(),
                                              innerBounds.getWidth() - wheelsStripW, innerBounds.getHeight());

    float weights[] = { 8.0f, 25.0f, 5.0f, 20.0f, 8.0f, 20.0f, 14.0f };
    float totalWeight = 0.0f;
    for (auto wt : weights) totalWeight += wt;

    float totalW = (float) sectionArea.getWidth();
    const juce::String sectionLabels[] = { "LFO", "DCO", "HPF", "VCF", "VCA", "ENV", "CHORUS" };

    // Section labels in orange stripe
    g.setFont (juce::FontOptions (11.0f).withStyle ("Bold"));
    g.setColour (juce::Colours::white);

    float xPos = (float) sectionArea.getX();
    int contentTop = stripeArea.getBottom() + 4;
    int contentBottom = innerBounds.getBottom();

    for (int i = 0; i < 7; ++i)
    {
        float secW = totalW * weights[i] / totalWeight;

        // Section label in stripe
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (11.0f).withStyle ("Bold"));
        g.drawFittedText (sectionLabels[i],
                          juce::Rectangle<int> ((int) xPos, stripeArea.getY(), (int) secW, stripeH),
                          juce::Justification::centred, 1);

        // Section divider after each section except last
        if (i < 6)
        {
            float divX = xPos + secW;
            drawSectionDivider (g, (int) divX, contentTop - 2, contentBottom);
        }

        xPos += secW;
    }

    // Parameter labels below sliders
    int labelY = contentBottom - 14;
    int labelH = 13;
    g.setFont (juce::FontOptions (9.0f));
    g.setColour (juce::Colour (darkCharcoal));

    // LFO section
    xPos = (float) sectionArea.getX();
    float lfoW = totalW * weights[0] / totalWeight;
    {
        float sw = 34.0f;
        float gap = (lfoW - sw * 2.0f) / 3.0f;
        g.drawFittedText ("RATE",  juce::Rectangle<int> ((int)(xPos + gap), labelY, (int) sw, labelH), juce::Justification::centred, 1);
        g.drawFittedText ("DELAY", juce::Rectangle<int> ((int)(xPos + gap * 2.0f + sw), labelY, (int) sw, labelH), juce::Justification::centred, 1);
    }

    // DCO section
    float dcoX = xPos + lfoW;
    float dcoW = totalW * weights[1] / totalWeight;
    {
        int pad = 4;
        float sw = 30.0f;
        float dx = dcoX + (float) pad;
        g.drawFittedText ("LFO",   juce::Rectangle<int> ((int) dx, labelY, (int) sw, labelH), juce::Justification::centred, 1);
        dx += sw + (float) pad;
        g.drawFittedText ("PWM",   juce::Rectangle<int> ((int) dx, labelY, (int) sw, labelH), juce::Justification::centred, 1);
        g.drawFittedText ("NOISE", juce::Rectangle<int> ((int)(dcoX + dcoW - sw - (float) pad), labelY, (int) sw, labelH), juce::Justification::centred, 1);
    }

    // HPF section
    float hpfX = dcoX + dcoW;
    float hpfW = totalW * weights[2] / totalWeight;
    g.drawFittedText ("HPF", juce::Rectangle<int> ((int) hpfX, labelY, (int) hpfW, labelH), juce::Justification::centred, 1);

    // VCF section
    float vcfX = hpfX + hpfW;
    float vcfW = totalW * weights[3] / totalWeight;
    {
        int pad = 4;
        float sw = 30.0f;
        float dx = vcfX + (float) pad;
        const juce::String vcfLabels[] = { "FREQ", "RES", "ENV", "LFO" };
        for (int i = 0; i < 4; ++i)
        {
            g.drawFittedText (vcfLabels[i], juce::Rectangle<int> ((int) dx, labelY, (int) sw, labelH), juce::Justification::centred, 1);
            dx += sw + (float) pad;
        }
    }

    // VCA section
    float vcaX = vcfX + vcfW;
    float vcaW = totalW * weights[4] / totalWeight;
    g.drawFittedText ("LEVEL", juce::Rectangle<int> ((int) vcaX, labelY, (int) vcaW, labelH), juce::Justification::centred, 1);

    // ENV section
    float envX = vcaX + vcaW;
    float envW = totalW * weights[5] / totalWeight;
    {
        float sw = 30.0f;
        float gap = (envW - sw * 4.0f) / 5.0f;
        float dx = envX + gap;
        const juce::String envLabels[] = { "A", "D", "S", "R" };
        for (int i = 0; i < 4; ++i)
        {
            g.drawFittedText (envLabels[i], juce::Rectangle<int> ((int) dx, labelY, (int) sw, labelH), juce::Justification::centred, 1);
            dx += sw + gap;
        }
    }

    // Inner bezel shadow (subtle inset effect on the cream area)
    g.setColour (juce::Colour (0x18000000));
    g.drawRect (innerBounds.toFloat(), 1.0f);
}

//==============================================================================
void Juno60Editor::drawAdsrCurve (juce::Graphics& g, juce::Rectangle<int> area) const
{
    float a = envAttack.getValue()  / envAttack.getMaximum();
    float d = envDecay.getValue()   / envDecay.getMaximum();
    float s = envSustain.getValue() / envSustain.getMaximum();
    float r = envRelease.getValue() / envRelease.getMaximum();

    float x0 = (float) area.getX();
    float y0 = (float) area.getBottom();
    float w  = (float) area.getWidth();
    float h  = (float) area.getHeight();

    float aW = w * 0.25f * juce::jmax (a, 0.05f);
    float dW = w * 0.25f * juce::jmax (d, 0.05f);
    float sW = w * 0.3f;
    float rW = w - aW - dW - sW;

    juce::Path env;
    env.startNewSubPath (x0, y0);
    env.lineTo (x0 + aW, y0 - h);
    float sustainY = y0 - h * s;
    env.lineTo (x0 + aW + dW, sustainY);
    env.lineTo (x0 + aW + dW + sW, sustainY);
    env.lineTo (x0 + aW + dW + sW + rW, y0);

    g.setColour (juce::Colour (0x20000000));
    g.fillRect (area);

    juce::Path filled (env);
    filled.lineTo (x0 + w, y0);
    filled.closeSubPath();
    g.setColour (juce::Colour (0x18D06030));
    g.fillPath (filled);

    g.setColour (juce::Colour (0xffD06030));
    g.strokePath (env, juce::PathStrokeType (1.5f));
}

//==============================================================================
void Juno60Editor::paint (juce::Graphics& g)
{
    int w = getWidth();
    int h = getHeight();

    if (w != cachedWidth || h != cachedHeight)
    {
        cachedBackground = juce::Image (juce::Image::ARGB, w, h, true);
        renderBackground (cachedBackground, w, h);
        cachedWidth  = w;
        cachedHeight = h;
    }

    g.drawImageAt (cachedBackground, 0, 0);

    // Draw dynamic ADSR curve in the ENV section
    {
        int bezelSize = 6;
        int stripeH   = 45;
        int woodW     = 16;
        int wheelsStripW = 50;

        auto bounds = getLocalBounds();
        auto innerBounds = bounds.reduced (bezelSize);
        innerBounds.setLeft (innerBounds.getX() + woodW);
        innerBounds.setRight (innerBounds.getRight() - woodW);

        auto sectionArea = innerBounds.withLeft (innerBounds.getX() + wheelsStripW);

        int contentTop = sectionArea.getY() + stripeH + 6;
        int contentBottom = sectionArea.getBottom() - 16;
        int contentH = contentBottom - contentTop;

        float totalW = (float) sectionArea.getWidth();
        float weights[] = { 8.0f, 25.0f, 5.0f, 20.0f, 8.0f, 20.0f, 14.0f };
        float totalWeight = 0.0f;
        for (auto wt : weights) totalWeight += wt;

        float xPos = (float) sectionArea.getX();
        for (int i = 0; i < 5; ++i)
            xPos += totalW * weights[i] / totalWeight;
        float secW = totalW * weights[5] / totalWeight;

        int curveH = 40;
        auto curveArea = juce::Rectangle<int> ((int) xPos + 8, contentTop + contentH - curveH - 18,
                                                (int) secW - 16, curveH);
        drawAdsrCurve (g, curveArea);
    }
}

//==============================================================================
void Juno60Editor::resized()
{
    if (!chorusModeGroup) return;

    cachedWidth  = 0;
    cachedHeight = 0;

    int bezelSize = 6;
    int stripeH   = 45;
    int woodW     = 16;
    int wheelsStripW = 50;

    auto bounds = getLocalBounds();

    auto innerBounds = bounds.reduced (bezelSize);
    innerBounds.setLeft (innerBounds.getX() + woodW);
    innerBounds.setRight (innerBounds.getRight() - woodW);

    // Preset selector in the orange stripe area, right-aligned
    {
        auto stripeArea = juce::Rectangle<int> (innerBounds.getX(), innerBounds.getY(),
                                                 innerBounds.getWidth(), stripeH);
        presetSelector.setBounds (stripeArea.getRight() - 210, stripeArea.getY() + 8,
                                  200, 28);
    }

    int contentTop = innerBounds.getY() + stripeH + 6;
    int contentBottom = innerBounds.getBottom() - 16;
    int contentH = contentBottom - contentTop;

    // === Pitch / Mod Wheels ===
    {
        auto wheelsArea = juce::Rectangle<int> (innerBounds.getX(), contentTop,
                                                 wheelsStripW, contentH);
        int wheelH = wheelsArea.getHeight() / 2 - 10;
        pitchWheel.setBounds (wheelsArea.getX() + 8, wheelsArea.getY(), 34, wheelH);
        modWheel.setBounds   (wheelsArea.getX() + 8, wheelsArea.getY() + wheelH + 20, 34, wheelH);
    }

    auto sectionArea = innerBounds.withLeft (innerBounds.getX() + wheelsStripW);
    float totalW = (float) sectionArea.getWidth();
    float weights[] = { 8.0f, 25.0f, 5.0f, 20.0f, 8.0f, 20.0f, 14.0f };
    float totalWeight = 0.0f;
    for (auto w : weights) totalWeight += w;

    auto sectionBounds = [&] (int index) -> juce::Rectangle<int>
    {
        float xPos = (float) sectionArea.getX();
        for (int i = 0; i < index; ++i)
            xPos += totalW * weights[i] / totalWeight;
        float secW = totalW * weights[index] / totalWeight;
        return juce::Rectangle<int> ((int) xPos + 4, contentTop, (int) secW - 8, contentH);
    };

    int sliderW = 34;
    int btnH = 26;

    // === LFO Section ===
    {
        auto area = sectionBounds (0);
        int sw = juce::jmin (sliderW, area.getWidth() / 2);
        int gap = (area.getWidth() - sw * 2) / 3;
        lfoRate .setBounds (area.getX() + gap,          area.getY(), sw, area.getHeight());
        lfoDelay.setBounds (area.getX() + gap * 2 + sw, area.getY(), sw, area.getHeight());
    }

    // === DCO Section ===
    {
        auto area = sectionBounds (1);
        int sw = juce::jmin (30, area.getWidth() / 6);
        int pad = 4;

        int sliderH = area.getHeight();
        int topY = area.getY();
        int x = area.getX() + pad;

        dcoLfoDepth .setBounds (x, topY, sw, sliderH);  x += sw + pad;
        dcoPwmAmount.setBounds (x, topY, sw, sliderH);  x += sw + pad;

        int grpW = sw * 2 + pad;
        dcoPwmSourceGroup->setBounds (x, topY, grpW, btnH);
        dcoRangeGroup->setBounds (x, topY + btnH + 4, grpW, btnH);

        x += grpW + pad;

        int waveBtnW = juce::jmax (36, (area.getRight() - x - sw - pad * 2) / 3);
        int waveY = topY;
        dcoSawBtn  .setBounds (x, waveY, waveBtnW, btnH);  waveY += btnH + 3;
        dcoPulseBtn.setBounds (x, waveY, waveBtnW, btnH);  waveY += btnH + 3;
        dcoSubBtn  .setBounds (x, waveY, waveBtnW, btnH);

        noiseLevel.setBounds (area.getRight() - sw - pad, topY, sw, sliderH);
    }

    // === HPF Section ===
    {
        auto area = sectionBounds (2);
        int pad = 6;
        hpfModeGroup->setBounds (area.reduced (pad, 0).withHeight (area.getHeight()));
    }

    // === VCF Section ===
    {
        auto area = sectionBounds (3);
        int sw = juce::jmin (30, area.getWidth() / 5);
        int pad = 4;
        int sliderH = area.getHeight() * 70 / 100;
        int x = area.getX() + pad;

        vcfFreq     .setBounds (x, area.getY(), sw, sliderH); x += sw + pad;
        vcfRes      .setBounds (x, area.getY(), sw, sliderH); x += sw + pad;
        vcfEnvAmount.setBounds (x, area.getY(), sw, sliderH); x += sw + pad;
        vcfLfoAmount.setBounds (x, area.getY(), sw, sliderH); x += sw + pad;

        int kfY = area.getY() + sliderH + 6;
        int kfW = area.getWidth() - pad * 2;
        vcfKeyFollowGroup->setBounds (area.getX() + pad, kfY, kfW, btnH);
    }

    // === VCA Section ===
    {
        auto area = sectionBounds (4);
        int sw = juce::jmin (sliderW, area.getWidth() / 2);
        int pad = 4;

        vcaModeGroup->setBounds (area.getX() + pad, area.getY(), area.getWidth() - pad * 2, btnH);
        vcaLevel.setBounds (area.getCentreX() - sw / 2, area.getY() + btnH + 6,
                            sw, area.getHeight() - btnH - 10);
    }

    // === ENV (ADSR) Section ===
    {
        auto area = sectionBounds (5);
        int sw = juce::jmin (30, area.getWidth() / 4 - 4);
        int gap = (area.getWidth() - sw * 4) / 5;
        int x = area.getX() + gap;

        envAttack .setBounds (x, area.getY(), sw, area.getHeight()); x += sw + gap;
        envDecay  .setBounds (x, area.getY(), sw, area.getHeight()); x += sw + gap;
        envSustain.setBounds (x, area.getY(), sw, area.getHeight()); x += sw + gap;
        envRelease.setBounds (x, area.getY(), sw, area.getHeight());
    }

    // === Chorus Section ===
    {
        auto area = sectionBounds (6);
        int pad = 6;
        int grpH = juce::jmin (area.getHeight(), btnH * 4 + 16);
        chorusModeGroup->setBounds (area.getX() + pad, area.getCentreY() - grpH / 2,
                                    area.getWidth() - pad * 2, grpH);
    }
}
