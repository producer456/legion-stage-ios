#pragma once

#include <JuceHeader.h>
#include <atomic>

class PluginHost;
class SequencerEngine;

// Offline audio bounce — renders the full mix to a WAV/M4A file.
// Runs on a background thread, drives PluginHost::processBlock in a loop
// without the audio device. Progress is readable from the UI thread.
class AudioExporter : public juce::Thread
{
public:
    enum class Format { WAV, M4A };
    enum class State { Idle, Rendering, Finished, Error };

    AudioExporter(PluginHost& host, SequencerEngine& engine)
        : Thread("AudioExport"), pluginHost(host), engine(engine) {}

    ~AudioExporter() override { stopThread(5000); }

    // Configure and start export. Call from UI thread.
    bool startExport(const juce::File& outputFile,
                     double startBeat, double endBeat,
                     double sampleRate, Format format = Format::WAV,
                     bool includeTail = true, double tailSeconds = 2.0)
    {
        if (isThreadRunning()) return false;

        this->outputFile = outputFile;
        this->startBeat = startBeat;
        this->endBeat = endBeat;
        this->exportSampleRate = sampleRate;
        this->format = format;
        this->includeTail = includeTail;
        this->tailSeconds = tailSeconds;
        this->exportProgress.store(0.0f);
        this->exportState.store(State::Rendering);
        this->errorMessage.clear();

        startThread();
        return true;
    }

    void cancelExport() { signalThreadShouldExit(); }

    float getProgress() const { return exportProgress.load(); }
    State getState() const { return exportState.load(); }
    juce::String getErrorMessage() const { return errorMessage; }
    juce::File getOutputFile() const { return outputFile; }

private:
    PluginHost& pluginHost;
    SequencerEngine& engine;

    juce::File outputFile;
    double startBeat = 0.0;
    double endBeat = 16.0;
    double exportSampleRate = 44100.0;
    Format format = Format::WAV;
    bool includeTail = true;
    double tailSeconds = 2.0;

    std::atomic<float> exportProgress { 0.0f };
    std::atomic<State> exportState { State::Idle };
    juce::String errorMessage;

    static constexpr int BLOCK_SIZE = 512;

    void run() override
    {
        // Calculate total samples
        double bpm = engine.getBpm();
        double beatsPerSecond = bpm / 60.0;
        double durationBeats = endBeat - startBeat;
        double durationSeconds = durationBeats / beatsPerSecond;
        double tailDuration = includeTail ? tailSeconds : 0.0;
        int64_t totalSamples = static_cast<int64_t>((durationSeconds + tailDuration) * exportSampleRate);
        int64_t mainSamples = static_cast<int64_t>(durationSeconds * exportSampleRate);

        // Create output file writer
        std::unique_ptr<juce::AudioFormatWriter> writer;
        {
            auto* fos = new juce::FileOutputStream(outputFile);
            if (fos->failedToOpen())
            {
                delete fos;
                errorMessage = "Could not create output file";
                exportState.store(State::Error);
                return;
            }

            if (format == Format::WAV)
            {
                juce::WavAudioFormat wavFormat;
                writer.reset(wavFormat.createWriterFor(fos, exportSampleRate, 2, 24, {}, 0));
            }
#if JUCE_MAC || JUCE_IOS
            else if (format == Format::M4A)
            {
                juce::CoreAudioFormat coreAudioFormat;
                writer.reset(coreAudioFormat.createWriterFor(fos, exportSampleRate, 2, 16, {}, 0));
            }
#endif

            if (!writer)
            {
                delete fos; // writer takes ownership on success, but not on failure
                errorMessage = "Could not create audio format writer";
                exportState.store(State::Error);
                return;
            }
        }

        // Save transport state
        bool wasPlaying = engine.isPlaying();
        bool wasRecording = engine.isRecording();
        double savedPosition = engine.getPositionInBeats();
        bool savedLoop = engine.isLoopEnabled();

        // Configure engine for offline render
        engine.stop();
        if (wasRecording) engine.toggleRecord();
        engine.setLoopEnabled(false);
        engine.setPosition(startBeat);

        // Prepare the graph for offline rendering
        pluginHost.prepareForOfflineRender(exportSampleRate, BLOCK_SIZE);

        // Render loop
        juce::AudioBuffer<float> buffer(2, BLOCK_SIZE);
        juce::MidiBuffer midiBuf;

        for (int64_t pos = 0; pos < totalSamples && !threadShouldExit(); pos += BLOCK_SIZE)
        {
            int numThisBlock = static_cast<int>(juce::jmin(static_cast<int64_t>(BLOCK_SIZE), totalSamples - pos));
            buffer.clear();
            midiBuf.clear();

            // Drive the engine: advance position and process
            if (pos < mainSamples)
            {
                // Main content — engine plays normally
                engine.play();
            }
            else
            {
                // Tail — engine stopped, but graph still processes (reverb tails etc.)
                engine.stop();
            }

            pluginHost.processBlockOffline(buffer, midiBuf, numThisBlock);

            // Write to file
            writer->writeFromAudioSampleBuffer(buffer, 0, numThisBlock);

            exportProgress.store(static_cast<float>(pos) / static_cast<float>(totalSamples));
        }

        // Finalize
        writer.reset(); // flush and close file

        // Restore transport state
        engine.stop();
        engine.setPosition(savedPosition);
        engine.setLoopEnabled(savedLoop);
        pluginHost.restoreFromOfflineRender();
        if (wasPlaying) engine.play();

        if (threadShouldExit())
        {
            outputFile.deleteFile();
            exportState.store(State::Idle);
        }
        else
        {
            exportProgress.store(1.0f);
            exportState.store(State::Finished);
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioExporter)
};
