#pragma once

#include <JuceHeader.h>
#include <map>

// Built-in sampler instrument — loads audio files and plays them back
// on MIDI note triggers with pitch shifting, velocity sensitivity, and ADSR.
// Supports drum kit mode where different MIDI notes trigger different samples.
// Registers as an internal AudioProcessor in the PluginHost graph.
class BuiltinSamplerProcessor : public juce::AudioProcessor
{
public:
    // Holds a single drum sample buffer + its file sample rate
    struct DrumSample
    {
        juce::AudioBuffer<float> buffer;
        double sampleRate = 44100.0;
    };

    BuiltinSamplerProcessor()
        : AudioProcessor(BusesProperties()
            .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    {
        // 16 polyphonic voices
        for (int i = 0; i < 16; ++i)
            synth.addVoice(new SamplerVoice(*this));
    }

    ~BuiltinSamplerProcessor() override = default;

    // ── Drum Kit Names ──

    static juce::StringArray getDrumKitNames()
    {
        return { "CR-78", "TR-505", "LM-2" };
    }

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
        // Exit drum kit mode when loading a single sample
        drumKitMode.store(false);
        drumKitName = "";
        drumKitSamples.clear();

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

    // ── Drum Kit Loading ──

    void loadDrumKit(const juce::String& kitName)
    {
        // Find the drumkits directory in the app bundle
        auto bundlePath = juce::File::getSpecialLocation(juce::File::currentApplicationFile);
        auto kitDir = bundlePath.getChildFile("drumkits").getChildFile(kitName);

        if (!kitDir.isDirectory())
        {
            DBG("Drum kit directory not found: " + kitDir.getFullPathName());
            return;
        }

        juce::AudioFormatManager fmgr;
        fmgr.registerBasicFormats();

        std::map<int, DrumSample> newSamples;
        auto wavFiles = kitDir.findChildFiles(juce::File::findFiles, false, "*.wav");

        for (auto& file : wavFiles)
        {
            std::unique_ptr<juce::AudioFormatReader> reader(fmgr.createReaderFor(file));
            if (!reader) continue;

            int midiNote = mapFilenameToMidiNote(file.getFileNameWithoutExtension());
            if (midiNote < 0) continue;

            DrumSample ds;
            ds.buffer = juce::AudioBuffer<float>(static_cast<int>(reader->numChannels),
                                                  static_cast<int>(reader->lengthInSamples));
            reader->read(&ds.buffer, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);
            ds.sampleRate = reader->sampleRate;

            // If note is already mapped, skip (first match wins)
            if (newSamples.find(midiNote) == newSamples.end())
                newSamples[midiNote] = std::move(ds);
        }

        if (newSamples.empty())
        {
            DBG("No drum samples loaded from kit: " + kitName);
            return;
        }

        // Activate drum kit mode
        synth.clearSounds();
        drumKitSamples = std::move(newSamples);
        drumKitName = kitName;
        drumKitMode.store(true);
        sampleLoaded.store(true);
        sampleName = "DrumKit: " + kitName;

        // Set drum-appropriate ADSR and one-shot mode
        oneShot.store(true);
        attack.store(1.0f);
        decay.store(100.0f);
        sustain.store(0.0f);
        release.store(50.0f);
        updateADSR();

        // Clear single-sample voice references (not used in drum kit mode)
        for (int i = 0; i < synth.getNumVoices(); ++i)
        {
            if (auto* v = dynamic_cast<SamplerVoice*>(synth.getVoice(i)))
                v->setSampleData(nullptr, 44100.0);
        }

        // Add a dummy sound so the Synthesiser allows note triggers
        synth.addSound(new DummySound());
    }

    bool isDrumKitMode() const { return drumKitMode.load(); }
    juce::String getDrumKitName() const { return drumKitName; }

    // Look up a drum sample for a given MIDI note (used by voices)
    const DrumSample* getDrumSample(int midiNote) const
    {
        auto it = drumKitSamples.find(midiNote);
        if (it != drumKitSamples.end())
            return &it->second;
        return nullptr;
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
        // Save sample path + ADSR + root note + drum kit state
        juce::ValueTree state("SamplerState");
        state.setProperty("sampleName", sampleName, nullptr);
        state.setProperty("rootNote", rootNote.load(), nullptr);
        state.setProperty("oneShot", oneShot.load(), nullptr);
        state.setProperty("attack", attack.load(), nullptr);
        state.setProperty("decay", decay.load(), nullptr);
        state.setProperty("sustain", sustain.load(), nullptr);
        state.setProperty("release", release.load(), nullptr);
        state.setProperty("drumKitMode", drumKitMode.load(), nullptr);
        state.setProperty("drumKitName", drumKitName, nullptr);
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

            // Restore drum kit if applicable
            bool wasDrumKit = static_cast<bool>(state.getProperty("drumKitMode", false));
            if (wasDrumKit)
            {
                juce::String savedKitName = state.getProperty("drumKitName", "").toString();
                if (savedKitName.isNotEmpty())
                    loadDrumKit(savedKitName);
            }
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

    // Drum kit state
    std::atomic<bool> drumKitMode { false };
    juce::String drumKitName;
    std::map<int, DrumSample> drumKitSamples;

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

    // ── Filename to MIDI Note Mapping ──

    static int mapFilenameToMidiNote(const juce::String& filename)
    {
        auto name = filename.toLowerCase();

        // Order matters: check more specific patterns before generic ones

        // Hi-hat variants
        if (name.contains("hihat-closed") || name.contains("hihat closed") || name.contains("-ch"))
            return 42;
        if (name.contains("hihat-open") || name.contains("hihat open") || name.contains("-oh") || name.contains("oh."))
            return 46;
        if (name.contains("hihat-pedal") || name.contains("hihat pedal"))
            return 44;
        // Accent/metal hi-hats map to closed hi-hat
        if (name.contains("hihat-accent") || name.contains("hihat-metal"))
            return 42;
        // Generic hihat -> closed
        if (name.contains("hihat") || name.contains("hi-hat"))
            return 42;

        // Tom variants (check specific before generic)
        if (name.contains("tom-hh") || name.contains("tom hh"))
            return 50; // Tom High alt
        if (name.contains("tom-h") || name.contains("tom-high") || name.contains("tom high"))
            return 48;
        if (name.contains("tom-ll") || name.contains("tom ll"))
            return 43; // Tom Low alt
        if (name.contains("tom-l") || name.contains("tom-low") || name.contains("tom low"))
            return 41;
        if (name.contains("tom-m") || name.contains("tom-mid") || name.contains("tom mid"))
            return 45;

        // Conga variants
        if (name.contains("conga-hh"))
            return 63; // Open Hi Conga alt
        if (name.contains("conga-h"))
            return 62;
        if (name.contains("conga-lll"))
            return 65; // Low Conga alt
        if (name.contains("conga-ll"))
            return 64; // Low Conga alt
        if (name.contains("conga-l"))
            return 64;
        if (name.contains("conga-m"))
            return 63; // Open Hi Conga
        if (name.contains("conga"))
            return 62;

        // Bongo
        if (name.contains("bongo-h"))
            return 60;
        if (name.contains("bongo-l"))
            return 61;
        if (name.contains("bongo"))
            return 60;

        // Snare variants
        if (name.contains("snare-accent") || name.contains("snare-h"))
            return 38;
        if (name.contains("snare-l") || name.contains("snare-m"))
            return 40; // Snare alt (electric snare)
        if (name.contains("snare"))
            return 38;

        // Kick variants
        if (name.contains("kick-accent") || name.contains("kick-alt"))
            return 36; // same note, or could use 35 for alt
        if (name.contains("kick"))
            return 36;

        // Stick variants
        if (name.contains("stick-h"))
            return 37; // Rim/Sidestick
        if (name.contains("stick-l"))
            return 75; // Claves
        if (name.contains("stick-m"))
            return 76; // Hi Wood Block
        if (name.contains("stick"))
            return 37;

        // Clap
        if (name.contains("clap"))
            return 39;

        // Rim
        if (name.contains("rim"))
            return 37;

        // Crash
        if (name.contains("crash"))
            return 49;

        // Ride
        if (name.contains("ride"))
            return 51;

        // Cymbal (generic) -> crash
        if (name.contains("cymbal"))
            return 49;

        // Cowbell
        if (name.contains("cowb"))
            return 56;

        // Tambourine
        if (name.contains("tamb"))
            return 54;

        // Cabasa
        if (name.contains("cabasa"))
            return 69;

        // Claves
        if (name.contains("claves"))
            return 75;

        // Guiro
        if (name.contains("guiro-short"))
            return 73; // Short Guiro
        if (name.contains("guiro-long") || name.contains("guiro"))
            return 74; // Long Guiro

        // Timbal -> high timbale
        if (name.contains("timbal"))
            return 65;

        return -1; // unknown
    }

    // Dummy sound — just tells Synthesiser to accept all notes
    class DummySound : public juce::SynthesiserSound
    {
    public:
        bool appliesToNote(int) override { return true; }
        bool appliesToChannel(int) override { return true; }
    };

    // Custom voice with pitch shifting, ADSR, one-shot support, and drum kit mode
    class SamplerVoice : public juce::SynthesiserVoice
    {
    public:
        SamplerVoice(BuiltinSamplerProcessor& proc) : processor(proc) {}

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
            if (processor.isDrumKitMode())
            {
                // Drum kit mode: look up the sample for this MIDI note
                activeDrumSample = processor.getDrumSample(midiNote);
                if (!activeDrumSample || activeDrumSample->buffer.getNumSamples() == 0)
                {
                    clearCurrentNote();
                    return;
                }

                // Play at original pitch (no transposition for drums)
                pitchRatio = activeDrumSample->sampleRate / getSampleRate();
                sample = nullptr; // not using single-sample mode
            }
            else
            {
                // Normal mode: single sample with pitch shifting
                activeDrumSample = nullptr;
                if (!sample || sample->getNumSamples() == 0) return;

                double semitones = midiNote - root;
                pitchRatio = std::pow(2.0, semitones / 12.0) * (fileSampleRate / getSampleRate());
            }

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
                activeDrumSample = nullptr;
                clearCurrentNote();
            }
        }

        void renderNextBlock(juce::AudioBuffer<float>& output, int startSample, int numSamples) override
        {
            if (!playing) return;

            // Determine which buffer to read from
            const juce::AudioBuffer<float>* activeBuffer = nullptr;
            if (activeDrumSample)
                activeBuffer = &activeDrumSample->buffer;
            else
                activeBuffer = sample;

            if (!activeBuffer || activeBuffer->getNumSamples() == 0)
            {
                playing = false;
                activeDrumSample = nullptr;
                clearCurrentNote();
                return;
            }

            int numChannels = output.getNumChannels();
            int sampleLen = activeBuffer->getNumSamples();
            int sampleChans = activeBuffer->getNumChannels();

            for (int i = 0; i < numSamples; ++i)
            {
                int pos0 = static_cast<int>(position);
                if (pos0 >= sampleLen)
                {
                    // Sample finished
                    playing = false;
                    adsr.reset();
                    activeDrumSample = nullptr;
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
                    activeDrumSample = nullptr;
                    clearCurrentNote();
                    break;
                }

                for (int ch = 0; ch < numChannels; ++ch)
                {
                    int srcCh = juce::jmin(ch, sampleChans - 1);
                    float s0 = activeBuffer->getSample(srcCh, pos0);
                    float s1 = activeBuffer->getSample(srcCh, pos1);
                    float val = (s0 + (s1 - s0) * frac) * gain * envGain;
                    output.addSample(ch, startSample + i, val);
                }

                position += pitchRatio;
            }
        }

        void pitchWheelMoved(int) override {}
        void controllerMoved(int, int) override {}

    private:
        BuiltinSamplerProcessor& processor;
        const juce::AudioBuffer<float>* sample = nullptr;
        const DrumSample* activeDrumSample = nullptr;
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
