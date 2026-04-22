#pragma once

#include <JuceHeader.h>
#include "Juno60/Juno60Editor.h"
#include "Juno60/VoiceManager.h"
#include "Juno60/Chorus.h"
#include "Juno60/Arpeggiator.h"
#include "Juno60/Presets.h"

// Built-in Juno-60 synthesizer instrument for Legion Stage.
// Wraps the Juno-60 DSP engine as an AudioProcessor that can be loaded
// into the PluginHost graph like any external plugin.
class BuiltinJuno60Processor : public juce::AudioProcessor
{
public:
    BuiltinJuno60Processor()
        : AudioProcessor(BusesProperties()
            .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
          apvts(*this, nullptr, "Parameters", createParameterLayout())
    {
    }

    ~BuiltinJuno60Processor() override = default;

    // ── AudioProcessor Interface ──

    const juce::String getName() const override { return "Juno-60"; }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override
    {
        voiceManager.prepare(sampleRate);
        chorus.prepare(sampleRate, samplesPerBlock);
        arpeggiator.setSampleRate(sampleRate);
    }

    void releaseResources() override {}

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override
    {
        juce::ScopedNoDenormals noDenormals;
        buffer.clear();

        // Update voice parameters from APVTS
        voiceManager.setLFORate(apvts.getRawParameterValue("lfoRate")->load());
        voiceManager.setLFODelay(apvts.getRawParameterValue("lfoDelay")->load());
        voiceManager.setLFODepth(apvts.getRawParameterValue("dcoLfoDepth")->load());

        voiceManager.setPWMAmount(apvts.getRawParameterValue("dcoPwmAmount")->load());
        voiceManager.setPWMSource(static_cast<int>(apvts.getRawParameterValue("dcoPwmSource")->load()));

        int rangeIndex = static_cast<int>(apvts.getRawParameterValue("dcoRange")->load());
        int footage = (rangeIndex == 0) ? 16 : (rangeIndex == 1) ? 8 : 4;
        voiceManager.setOscRange(footage);

        voiceManager.setOscSawEnabled(apvts.getRawParameterValue("dcoSaw")->load() > 0.5f);
        voiceManager.setOscPulseEnabled(apvts.getRawParameterValue("dcoPulse")->load() > 0.5f);
        voiceManager.setOscSubEnabled(apvts.getRawParameterValue("dcoSub")->load() > 0.5f);

        voiceManager.setHPFMode(static_cast<int>(apvts.getRawParameterValue("hpfMode")->load()));

        voiceManager.setFilterCutoff(apvts.getRawParameterValue("vcfFreq")->load());
        voiceManager.setFilterResonance(apvts.getRawParameterValue("vcfRes")->load());
        voiceManager.setFilterEnvAmount(apvts.getRawParameterValue("vcfEnvAmount")->load());
        voiceManager.setFilterLFOAmount(apvts.getRawParameterValue("vcfLfoAmount")->load());

        int keyFollowIndex = static_cast<int>(apvts.getRawParameterValue("vcfKeyFollow")->load());
        float keyFollowVal = (keyFollowIndex == 0) ? 0.0f : (keyFollowIndex == 1) ? 0.5f : 1.0f;
        voiceManager.setFilterKeyFollow(keyFollowVal);

        voiceManager.setVCAMode(static_cast<int>(apvts.getRawParameterValue("vcaMode")->load()));
        voiceManager.setVCALevel(apvts.getRawParameterValue("vcaLevel")->load());

        voiceManager.setAttack(apvts.getRawParameterValue("envAttack")->load());
        voiceManager.setDecay(apvts.getRawParameterValue("envDecay")->load());
        voiceManager.setSustain(apvts.getRawParameterValue("envSustain")->load());
        voiceManager.setRelease(apvts.getRawParameterValue("envRelease")->load());

        voiceManager.setNoiseLevel(apvts.getRawParameterValue("noiseLevel")->load());

        chorus.setMode(static_cast<int>(apvts.getRawParameterValue("chorusMode")->load()));

        // Update arpeggiator parameters
        bool arpEnabled = apvts.getRawParameterValue("arpEnabled")->load() > 0.5f;
        arpeggiator.setEnabled(arpEnabled);
        arpeggiator.setMode(static_cast<Juno60::Arpeggiator::Mode>(
            static_cast<int>(apvts.getRawParameterValue("arpMode")->load())));
        arpeggiator.setRate(apvts.getRawParameterValue("arpRate")->load());
        arpeggiator.setOctaveRange(static_cast<int>(apvts.getRawParameterValue("arpOctave")->load()) + 1);
        arpeggiator.setHold(apvts.getRawParameterValue("arpHold")->load() > 0.5f);

        // Handle all-notes-off / CC 123
        for (const auto metadata : midiMessages)
        {
            auto msg = metadata.getMessage();
            if (msg.isAllNotesOff() || (msg.isController() && msg.getControllerNumber() == 123))
            {
                voiceManager.allNotesOff();
                arpeggiator.allNotesOff();
            }
        }

        if (!arpEnabled)
        {
            for (const auto metadata : midiMessages)
                voiceManager.handleMidiEvent(metadata.getMessage());

            voiceManager.renderNextBlock(buffer, 0, buffer.getNumSamples());
        }
        else
        {
            int numSamples = buffer.getNumSamples();

            for (const auto metadata : midiMessages)
            {
                auto msg = metadata.getMessage();
                if (msg.isNoteOn())
                    arpeggiator.noteOn(msg.getNoteNumber(), msg.getFloatVelocity());
                else if (msg.isNoteOff())
                    arpeggiator.noteOff(msg.getNoteNumber());
                else if (!msg.isAllNotesOff() && !msg.isAllSoundOff())
                    voiceManager.handleMidiEvent(msg);
            }

            for (int i = 0; i < numSamples; ++i)
            {
                auto arpEvent = arpeggiator.process();
                if (arpEvent.noteOff >= 0)
                {
                    auto offMsg = juce::MidiMessage::noteOff(1, arpEvent.noteOff);
                    voiceManager.handleMidiEvent(offMsg);
                }
                if (arpEvent.noteOn >= 0)
                {
                    auto onMsg = juce::MidiMessage::noteOn(1, arpEvent.noteOn, arpEvent.velocity);
                    voiceManager.handleMidiEvent(onMsg);
                }

                voiceManager.renderNextBlock(buffer, i, 1);
            }
        }

        // Apply chorus
        chorus.process(buffer);
    }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    juce::AudioProcessorEditor* createEditor() override
    {
        return new Juno60Editor(*this);
    }
    bool hasEditor() const override { return true; }

    int getNumPrograms() override { return getNumPresets(); }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram(int index) override
    {
        if (index >= 0 && index < getNumPresets())
        {
            currentProgram = index;
            loadPreset(index);
        }
    }
    const juce::String getProgramName(int index) override { return getPresetName(index); }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override
    {
        auto state = apvts.copyState();
        std::unique_ptr<juce::XmlElement> xml(state.createXml());
        copyXmlToBinary(*xml, destData);
    }

    void setStateInformation(const void* data, int sizeInBytes) override
    {
        std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
        if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override
    {
        if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
            && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
            return false;
        return true;
    }

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

    void loadPreset(int index)
    {
        const auto& presets = Juno60::getFactoryPresets();
        if (index < 0 || index >= static_cast<int>(presets.size()))
            return;

        const auto& p = presets[static_cast<size_t>(index)];

        auto setFloat = [this](const char* id, float value) {
            if (auto* param = apvts.getParameter(id))
                param->setValueNotifyingHost(param->convertTo0to1(value));
        };
        auto setChoice = [this](const char* id, int value) {
            if (auto* param = apvts.getParameter(id))
                param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(value)));
        };
        auto setBool = [this](const char* id, bool value) {
            if (auto* param = apvts.getParameter(id))
                param->setValueNotifyingHost(value ? 1.0f : 0.0f);
        };

        setFloat("lfoRate",        p.lfoRate);
        setFloat("lfoDelay",       p.lfoDelay);
        setFloat("dcoLfoDepth",    p.dcoLfo);
        setFloat("dcoPwmAmount",   p.pwm);
        setChoice("dcoPwmSource",  p.pwmLfo ? 0 : 1);
        setChoice("dcoRange",      p.range);
        setBool ("dcoSaw",         p.sawOn);
        setBool ("dcoPulse",       p.pulseOn);
        setBool ("dcoSub",         p.subOn);
        setFloat("noiseLevel",     p.noise);
        setChoice("hpfMode",       p.hpf);
        setFloat("vcfFreq",        p.vcfFreq);
        setFloat("vcfRes",         p.vcfRes);
        setFloat("vcfEnvAmount",   p.vcfEnv);
        setFloat("vcfLfoAmount",   p.vcfLfo);
        setChoice("vcfKeyFollow",  p.keyFollow);
        setChoice("vcaMode",       p.vcaGate ? 0 : 1);
        setFloat("vcaLevel",       p.vcaLevel);
        setFloat("envAttack",      p.attack);
        setFloat("envDecay",       p.decay);
        setFloat("envSustain",     p.sustain);
        setFloat("envRelease",     p.release);
        setChoice("chorusMode",    p.chorus);

        currentProgram = index;
    }

    static int getNumPresets()
    {
        return static_cast<int>(Juno60::getFactoryPresets().size());
    }

    static juce::String getPresetName(int index)
    {
        const auto& presets = Juno60::getFactoryPresets();
        if (index >= 0 && index < static_cast<int>(presets.size()))
            return juce::String(presets[static_cast<size_t>(index)].name);
        return {};
    }

private:
    int currentProgram = 0;
    juce::AudioProcessorValueTreeState apvts;

    Juno60::VoiceManager voiceManager { 6 };
    Juno60::Chorus chorus;
    Juno60::Arpeggiator arpeggiator;

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

        // LFO
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "lfoRate", 1 }, "LFO Rate",
            juce::NormalisableRange<float>(0.1f, 20.0f, 0.01f, 0.4f), 1.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "lfoDelay", 1 }, "LFO Delay",
            juce::NormalisableRange<float>(0.0f, 5.0f, 0.01f), 0.0f));

        // DCO
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "dcoLfoDepth", 1 }, "DCO LFO Depth",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "dcoPwmAmount", 1 }, "DCO PWM Amount",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { "dcoPwmSource", 1 }, "DCO PWM Source",
            juce::StringArray { "LFO", "Manual" }, 0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { "dcoRange", 1 }, "DCO Range",
            juce::StringArray { "16'", "8'", "4'" }, 1));
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { "dcoSaw", 1 }, "DCO Saw", false));
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { "dcoPulse", 1 }, "DCO Pulse", true));
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { "dcoSub", 1 }, "DCO Sub", false));

        // HPF
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { "hpfMode", 1 }, "HPF Mode",
            juce::StringArray { "0", "1", "2", "3" }, 0));

        // VCF
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "vcfFreq", 1 }, "VCF Frequency",
            juce::NormalisableRange<float>(20.0f, 20000.0f, 0.1f, 0.3f), 8000.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "vcfRes", 1 }, "VCF Resonance",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "vcfEnvAmount", 1 }, "VCF Env Amount",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "vcfLfoAmount", 1 }, "VCF LFO Amount",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { "vcfKeyFollow", 1 }, "VCF Key Follow",
            juce::StringArray { "0%", "50%", "100%" }, 0));

        // VCA
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { "vcaMode", 1 }, "VCA Mode",
            juce::StringArray { "Gate", "Env" }, 1));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "vcaLevel", 1 }, "VCA Level",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.8f));

        // ADSR
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "envAttack", 1 }, "Attack",
            juce::NormalisableRange<float>(0.001f, 5.0f, 0.001f, 0.4f), 0.01f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "envDecay", 1 }, "Decay",
            juce::NormalisableRange<float>(0.001f, 5.0f, 0.001f, 0.4f), 0.3f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "envSustain", 1 }, "Sustain",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.8f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "envRelease", 1 }, "Release",
            juce::NormalisableRange<float>(0.001f, 10.0f, 0.001f, 0.4f), 0.3f));

        // Chorus
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { "chorusMode", 1 }, "Chorus Mode",
            juce::StringArray { "Off", "I", "II", "I+II" }, 0));

        // Noise
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "noiseLevel", 1 }, "Noise Level",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

        // Arpeggiator
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { "arpEnabled", 1 }, "Arp Enabled", false));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { "arpMode", 1 }, "Arp Mode",
            juce::StringArray { "Up", "Down", "UpDown" }, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "arpRate", 1 }, "Arp Rate",
            juce::NormalisableRange<float>(1.0f, 20.0f, 0.1f, 0.5f), 5.0f));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { "arpOctave", 1 }, "Arp Octaves",
            juce::StringArray { "1", "2", "3" }, 0));
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { "arpHold", 1 }, "Arp Hold", false));

        return { params.begin(), params.end() };
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BuiltinJuno60Processor)
};
