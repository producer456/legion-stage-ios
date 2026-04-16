#pragma once

#include <JuceHeader.h>

// Built-in sampler instrument — loads audio files and plays them back
// on MIDI note triggers with pitch shifting, velocity sensitivity, and ADSR.
// Registers as an internal AudioProcessor in the PluginHost graph.
class BuiltinSamplerProcessor : public juce::AudioProcessor
{
public:
    BuiltinSamplerProcessor()
        : AudioProcessor(BusesProperties()
            .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    {
        // 16 polyphonic voices
        for (int i = 0; i < 16; ++i)
            synth.addVoice(new SamplerVoice());
    }

    ~BuiltinSamplerProcessor() override = default;

    // ── Sample Management ──

    void loadSample(const juce::File& file)
    {
        juce::AudioFormatManager fmgr;
        fmgr.registerBasicFormats();

        std::unique_ptr<juce::AudioFormatReader> reader(fmgr.createReaderFor(file));
        if (!reader) return;

        // Read entire sample into memory
        juce::AudioBuffer<float> buf(static_cast<int>(reader->numChannels),
                                      static_cast<int>(reader->lengthInSamples));
        reader->read(&buf, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);

        loadSampleFromBuffer(buf, reader->sampleRate, file.getFileNameWithoutExtension());
    }

    void loadSampleFromBuffer(const juce::AudioBuffer<float>& buf, double fileSampleRate,
                               const juce::String& name = "Sample")
    {
        // Create a BigInteger range covering all MIDI notes
        juce::BigInteger noteRange;
        noteRange.setRange(0, 128, true);

        // Clear old sounds and add new one
        synth.clearSounds();

        // Store raw buffer for our custom voice
        sampleBuffer = buf;
        sampleRate = fileSampleRate;
        sampleName = name;
        sampleLoaded.store(true);

        // Notify voices about the new sample
        for (int i = 0; i < synth.getNumVoices(); ++i)
        {
            if (auto* v = dynamic_cast<SamplerVoice*>(synth.getVoice(i)))
                v->setSampleData(&sampleBuffer, sampleRate);
        }

        // Add a dummy sound so the Synthesiser allows note triggers
        synth.addSound(new DummySound());
    }

    bool hasSample() const { return sampleLoaded.load(); }
    juce::String getSampleName() const { return sampleName; }
    const juce::AudioBuffer<float>& getSampleBuffer() const { return sampleBuffer; }
    double getSampleFileRate() const { return sampleRate; }

    // ── Controls ──

    void setAttack(float ms)  { attack.store(juce::jlimit(1.0f, 5000.0f, ms)); updateADSR(); }
    void setDecay(float ms)   { decay.store(juce::jlimit(1.0f, 5000.0f, ms)); updateADSR(); }
    void setSustain(float lvl) { sustain.store(juce::jlimit(0.0f, 1.0f, lvl)); updateADSR(); }
    void setRelease(float ms) { release.store(juce::jlimit(1.0f, 10000.0f, ms)); updateADSR(); }

    float getAttack() const  { return attack.load(); }
    float getDecay() const   { return decay.load(); }
    float getSustain() const { return sustain.load(); }
    float getRelease() const { return release.load(); }

    void setOneShot(bool on) { oneShot.store(on); }
    bool isOneShot() const { return oneShot.load(); }

    void setRootNote(int note) { rootNote.store(juce::jlimit(0, 127, note)); }
    int getRootNote() const { return rootNote.load(); }

    // ── AudioProcessor Interface ──

    const juce::String getName() const override { return "Built-in Sampler"; }

    void prepareToPlay(double sr, int blockSize) override
    {
        synth.setCurrentPlaybackSampleRate(sr);
        currentSampleRate = sr;
        updateADSR();
    }

    void releaseResources() override {}

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
    {
        buffer.clear();
        if (sampleLoaded.load())
            synth.renderNextBlock(buffer, midi, 0, buffer.getNumSamples());
    }

    // Required overrides
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return release.load() / 1000.0; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& data) override
    {
        // Save sample path + ADSR + root note
        juce::ValueTree state("SamplerState");
        state.setProperty("sampleName", sampleName, nullptr);
        state.setProperty("rootNote", rootNote.load(), nullptr);
        state.setProperty("oneShot", oneShot.load(), nullptr);
        state.setProperty("attack", attack.load(), nullptr);
        state.setProperty("decay", decay.load(), nullptr);
        state.setProperty("sustain", sustain.load(), nullptr);
        state.setProperty("release", release.load(), nullptr);
        juce::MemoryOutputStream stream(data, false);
        state.writeToStream(stream);
    }
    void setStateInformation(const void* data, int sizeInBytes) override
    {
        auto state = juce::ValueTree::readFromData(data, static_cast<size_t>(sizeInBytes));
        if (state.isValid())
        {
            rootNote.store(state.getProperty("rootNote", 60));
            oneShot.store(static_cast<bool>(state.getProperty("oneShot", false)));
            attack.store(static_cast<float>(state.getProperty("attack", 5.0f)));
            decay.store(static_cast<float>(state.getProperty("decay", 100.0f)));
            sustain.store(static_cast<float>(state.getProperty("sustain", 1.0f)));
            release.store(static_cast<float>(state.getProperty("release", 200.0f)));
            updateADSR();
        }
    }
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override
    {
        return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
            || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
    }

private:
    juce::Synthesiser synth;
    juce::AudioBuffer<float> sampleBuffer;
    double sampleRate = 44100.0;
    double currentSampleRate = 44100.0;
    juce::String sampleName;
    std::atomic<bool> sampleLoaded { false };

    std::atomic<float> attack { 5.0f };    // ms
    std::atomic<float> decay { 100.0f };   // ms
    std::atomic<float> sustain { 1.0f };   // 0-1
    std::atomic<float> release { 200.0f }; // ms
    std::atomic<bool> oneShot { false };
    std::atomic<int> rootNote { 60 };      // C4

    void updateADSR()
    {
        for (int i = 0; i < synth.getNumVoices(); ++i)
        {
            if (auto* v = dynamic_cast<SamplerVoice*>(synth.getVoice(i)))
            {
                juce::ADSR::Parameters p;
                p.attack = attack.load() / 1000.0f;
                p.decay = decay.load() / 1000.0f;
                p.sustain = sustain.load();
                p.release = release.load() / 1000.0f;
                v->setADSR(p);
                v->setOneShot(oneShot.load());
                v->setRootNote(rootNote.load());
            }
        }
    }

    // Dummy sound — just tells Synthesiser to accept all notes
    class DummySound : public juce::SynthesiserSound
    {
    public:
        bool appliesToNote(int) override { return true; }
        bool appliesToChannel(int) override { return true; }
    };

    // Custom voice with pitch shifting, ADSR, one-shot support
    class SamplerVoice : public juce::SynthesiserVoice
    {
    public:
        bool canPlaySound(juce::SynthesiserSound* s) override
        {
            return dynamic_cast<DummySound*>(s) != nullptr;
        }

        void setSampleData(const juce::AudioBuffer<float>* buf, double fileRate)
        {
            sample = buf;
            fileSampleRate = fileRate;
        }

        void setADSR(const juce::ADSR::Parameters& p) { adsrParams = p; }
        void setOneShot(bool on) { oneShotMode = on; }
        void setRootNote(int note) { root = note; }

        void startNote(int midiNote, float velocity, juce::SynthesiserSound*, int) override
        {
            if (!sample || sample->getNumSamples() == 0) return;

            // Pitch ratio: transpose relative to root note
            double semitones = midiNote - root;
            pitchRatio = std::pow(2.0, semitones / 12.0) * (fileSampleRate / getSampleRate());

            gain = velocity;
            position = 0.0;
            playing = true;
            adsr.setSampleRate(getSampleRate());
            adsr.setParameters(adsrParams);
            adsr.noteOn();
        }

        void stopNote(float, bool allowTailOff) override
        {
            if (oneShotMode && allowTailOff)
                return; // one-shot ignores note-off, plays to end

            if (allowTailOff)
                adsr.noteOff();
            else
            {
                adsr.reset();
                playing = false;
                clearCurrentNote();
            }
        }

        void renderNextBlock(juce::AudioBuffer<float>& output, int startSample, int numSamples) override
        {
            if (!playing || !sample) return;

            int numChannels = output.getNumChannels();
            int sampleLen = sample->getNumSamples();
            int sampleChans = sample->getNumChannels();

            for (int i = 0; i < numSamples; ++i)
            {
                int pos0 = static_cast<int>(position);
                if (pos0 >= sampleLen)
                {
                    // Sample finished
                    playing = false;
                    adsr.reset();
                    clearCurrentNote();
                    break;
                }

                // Linear interpolation
                int pos1 = juce::jmin(pos0 + 1, sampleLen - 1);
                float frac = static_cast<float>(position - pos0);

                float envGain = adsr.getNextSample();
                if (!adsr.isActive())
                {
                    playing = false;
                    clearCurrentNote();
                    break;
                }

                for (int ch = 0; ch < numChannels; ++ch)
                {
                    int srcCh = juce::jmin(ch, sampleChans - 1);
                    float s0 = sample->getSample(srcCh, pos0);
                    float s1 = sample->getSample(srcCh, pos1);
                    float val = (s0 + (s1 - s0) * frac) * gain * envGain;
                    output.addSample(ch, startSample + i, val);
                }

                position += pitchRatio;
            }
        }

        void pitchWheelMoved(int) override {}
        void controllerMoved(int, int) override {}

    private:
        const juce::AudioBuffer<float>* sample = nullptr;
        double fileSampleRate = 44100.0;
        double position = 0.0;
        double pitchRatio = 1.0;
        float gain = 1.0f;
        bool playing = false;
        bool oneShotMode = false;
        int root = 60;
        juce::ADSR adsr;
        juce::ADSR::Parameters adsrParams;
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BuiltinSamplerProcessor)
};
