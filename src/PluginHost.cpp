#include "PluginHost.h"
#include <cmath>
#include <atomic>
#include "AUScanner.h"
#include "BuiltinJuno60Processor.h"
#include "BuiltinSamplerProcessor.h"
#include "SpectrumComponent.h"
#include "LissajousComponent.h"
#include "WaveTerrainComponent.h"
#include "ShaderToyComponent.h"
#include "AnalyzerComponent.h"
#include "GeissComponent.h"
#include "ProjectMComponent.h"
#include "HeartbeatComponent.h"
#include "FluidSimComponent.h"
#include "RayMarchComponent.h"
#include "BioResonanceComponent.h"


PluginHost::PluginHost()
{
#if JUCE_PLUGINHOST_VST3
    formatManager.addFormat(new juce::VST3PluginFormat());
#endif
#if JUCE_PLUGINHOST_AU
    formatManager.addFormat(new juce::AudioUnitPluginFormat());
#endif

    for (int i = 0; i < NUM_TRACKS; ++i)
    {
        tracks[static_cast<size_t>(i)].index = i;
        tracks[static_cast<size_t>(i)].name = "Track " + juce::String(i + 1);
    }

    setupGraph();

    // Provide AudioPlayHead so plugins get BPM, position, transport state
    setPlayHead(&hostPlayHead);
}

PluginHost::~PluginHost()
{
    clear();
}

void PluginHost::setupGraph()
{
    setPlayConfigDetails(2, 2, storedSampleRate, storedBlockSize); // 2 in (mic), 2 out

    midiInputNode = addNode(
        std::make_unique<AudioProcessorGraph::AudioGraphIOProcessor>(
            AudioProcessorGraph::AudioGraphIOProcessor::midiInputNode));

    audioInputNode = addNode(
        std::make_unique<AudioProcessorGraph::AudioGraphIOProcessor>(
            AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode));

    audioOutputNode = addNode(
        std::make_unique<AudioProcessorGraph::AudioGraphIOProcessor>(
            AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode));

    for (int i = 0; i < NUM_TRACKS; ++i)
    {
        auto& track = tracks[static_cast<size_t>(i)];

        // Gain node
        auto gainProc = std::make_unique<GainProcessor>();
        gainProc->soloCount = &soloCount;
        track.gainProcessor = gainProc.get();
        track.gainNode = addNode(std::move(gainProc));

        // ClipPlayer node
        auto clipProc = std::make_unique<ClipPlayerNode>(engine);
        track.clipPlayer = clipProc.get();
        track.clipPlayerNode = addNode(std::move(clipProc));

        // Gain -> audio output
        for (int ch = 0; ch < 2; ++ch)
        {
            addConnection({ { track.gainNode->nodeID, ch },
                            { audioOutputNode->nodeID, ch } });
        }
    }
}

void PluginHost::scanForPlugins()
{
#if JUCE_IOS && JUCE_PLUGINHOST_AU
    // On iOS, use AVAudioUnitComponentManager to find all AUv3 plugins
    // and add them directly to the known plugin list
    {
        auto nativeAUs = AUScanner::scanAllAudioUnits();
        for (const auto& info : nativeAUs)
        {
            auto parts = juce::StringArray::fromTokens(info.identifier, "/", "");
            if (parts.size() != 3) continue;

            juce::String category;
            if (info.isInstrument) category = "Synths";
            else if (info.category == "Effect") category = "Effects";
            else if (info.category == "Generator") category = "Generators";
            else if (info.category == "MIDI") category = "MidiEffects";
            else category = "Effects";

            juce::PluginDescription pd;
            pd.name = info.name;
            pd.pluginFormatName = "AudioUnit";
            pd.category = info.category;
            pd.manufacturerName = info.manufacturer;
            pd.isInstrument = info.isInstrument;
            pd.numInputChannels = info.isInstrument ? 0 : 2;
            pd.numOutputChannels = 2;
            pd.uniqueId = info.uniqueId;
            // JUCE AU identifier format: "AudioUnit:Category/type,subtype,manufacturer"
            pd.fileOrIdentifier = "AudioUnit:" + category + "/" + parts[0] + "," + parts[1] + "," + parts[2];

            knownPluginList.addType(pd);
        }
    }
    return;
#endif

    for (int fi = 0; fi < formatManager.getNumFormats(); ++fi)
    {
        auto* format = formatManager.getFormat(fi);
        if (format == nullptr) continue;

        auto searchPaths = format->getDefaultLocationsToSearch();

#if defined(__APPLE__) && !JUCE_IOS
        // macOS VST3 locations
        juce::StringArray extraPaths = {
            "/Library/Audio/Plug-Ins/VST3",
            "~/Library/Audio/Plug-Ins/VST3"
        };
        auto home = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
        extraPaths.add(home.getChildFile("Library/Audio/Plug-Ins/VST3").getFullPathName());
        for (auto& path : extraPaths)
        {
            juce::File dir(path);
            if (dir.isDirectory())
                searchPaths.add(dir);
        }
#elif defined(_WIN32)
        // Add all common Windows VST3 locations
        juce::StringArray extraPaths = {
            "C:\\Program Files\\Common Files\\VST3",
            "C:\\Program Files (x86)\\Common Files\\VST3",
            "C:\\Program Files\\Steinberg\\Cubase 15\\VST3",
            "C:\\Program Files\\Steinberg\\Cubase 14\\VST3",
            "C:\\Program Files\\Steinberg\\Cubase 13\\VST3",
            "C:\\Program Files\\PreSonus\\Studio One 7\\VST3",
            "C:\\Program Files\\PreSonus\\Studio One 5\\VST3",
            "C:\\Program Files\\VSTPlugins",
            "C:\\Program Files (x86)\\VSTPlugins"
        };
        auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
        auto localAppData = juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory);
        extraPaths.add(appData.getChildFile("VST3").getFullPathName());
        extraPaths.add(localAppData.getChildFile("VST3").getFullPathName());
        for (auto& path : extraPaths)
        {
            juce::File dir(path);
            if (dir.isDirectory())
                searchPaths.add(dir);
        }
        juce::File progFiles("C:\\Program Files");
        if (progFiles.isDirectory())
        {
            for (const auto& entry : juce::RangedDirectoryIterator(progFiles, true, "*.vst3", juce::File::findDirectories))
            {
                auto parent = entry.getFile().getParentDirectory();
                searchPaths.addIfNotAlreadyThere(parent);
            }
        }
#endif

        auto foundFiles = format->searchPathsForPlugins(searchPaths, true, true);
        for (const auto& file : foundFiles)
        {
            juce::OwnedArray<juce::PluginDescription> foundTypes;
            knownPluginList.scanAndAddFile(file, true, foundTypes, *format);
        }
    }
}

void PluginHost::setAudioParams(double sampleRate, int blockSize)
{
    storedSampleRate = sampleRate;
    storedBlockSize = blockSize;
    midiCollector.reset(sampleRate);
}

bool PluginHost::loadPlugin(int trackIndex, const juce::PluginDescription& desc, juce::String& errorMsg)
{
    if (trackIndex < 0 || trackIndex >= NUM_TRACKS) return false;

    unloadPlugin(trackIndex);

    // Built-in Juno-60 synth — no external plugin loading needed
    if (desc.fileOrIdentifier == "legion-stage-builtin-juno60")
    {
        auto& track = tracks[static_cast<size_t>(trackIndex)];
        auto juno = std::make_unique<BuiltinJuno60Processor>();
        juno->prepareToPlay(storedSampleRate, storedBlockSize);
        track.pluginNode = addNode(std::move(juno));
        if (track.pluginNode != nullptr)
            track.plugin = track.pluginNode->getProcessor();
        else {
            track.plugin = nullptr;
            errorMsg = "Failed to add Juno-60 to audio graph";
            return false;
        }
        rewireTrack(trackIndex);
        return true;
    }

    // Built-in drum-kit sampler — identifier is
    // "legion-stage-builtin-sampler:<kit-name>".  Loads the named
    // kit folder from resources/drumkits/.
    if (desc.fileOrIdentifier.startsWith("legion-stage-builtin-sampler:"))
    {
        const auto kitName = desc.fileOrIdentifier
                             .fromFirstOccurrenceOf(":", false, false);
        auto& track = tracks[static_cast<size_t>(trackIndex)];
        auto sampler = std::make_unique<BuiltinSamplerProcessor>();
        sampler->prepareToPlay(storedSampleRate, storedBlockSize);
        sampler->loadDrumKit(kitName);
        track.pluginNode = addNode(std::move(sampler));
        if (track.pluginNode != nullptr)
            track.plugin = track.pluginNode->getProcessor();
        else {
            track.plugin = nullptr;
            errorMsg = "Failed to add drum kit to audio graph";
            return false;
        }
        rewireTrack(trackIndex);
        return true;
    }

#if JUCE_IOS
    // AUv3 plugins on iOS REQUIRE async instantiation
    std::unique_ptr<juce::AudioPluginInstance> instance;
    std::atomic<bool> finished{false};

    formatManager.createPluginInstanceAsync(desc, storedSampleRate, storedBlockSize,
        [&](std::unique_ptr<juce::AudioPluginInstance> result, const juce::String& err)
        {
            instance = std::move(result);
            if (err.isNotEmpty()) errorMsg = err;
            finished.store(true);
        });

    // Wait for async completion — pump the run loop on iOS
    auto deadline = juce::Time::getMillisecondCounter() + 15000;
    while (!finished.load() && juce::Time::getMillisecondCounter() < deadline)
    {
        AUScanner::pumpRunLoop(100);
    }

    if (!finished.load())
    {
        errorMsg = "Plugin instantiation timed out";
        auto& track = tracks[static_cast<size_t>(trackIndex)];
        track.plugin = nullptr;
        track.pluginNode = nullptr;
        return false;
    }
    if (instance == nullptr)
    {
        if (errorMsg.isEmpty()) errorMsg = "Plugin instance is null after async creation";
        return false;
    }
#else
    auto instance = formatManager.createPluginInstance(desc, storedSampleRate, storedBlockSize, errorMsg);
    if (instance == nullptr)
        return false;
#endif

    auto& track = tracks[static_cast<size_t>(trackIndex)];
    track.pluginNode = addNode(std::move(instance));

    if (track.pluginNode != nullptr)
        track.plugin = track.pluginNode->getProcessor();
    else
    {
        track.plugin = nullptr;
        errorMsg = "Failed to add plugin to audio graph";
        return false;
    }

    connectTrackAudio(trackIndex);
    updateMidiRouting();
    prepareToPlay(storedSampleRate, storedBlockSize);

    return true;
}

void PluginHost::unloadPlugin(int trackIndex)
{
    if (trackIndex < 0 || trackIndex >= NUM_TRACKS) return;

    auto& track = tracks[static_cast<size_t>(trackIndex)];
    if (track.pluginNode == nullptr) return;

    // Send all-notes-off to prevent stuck notes
    if (track.clipPlayer != nullptr)
        track.clipPlayer->sendAllNotesOff.store(true);

    // Unload all FX first
    for (int fx = 0; fx < Track::NUM_FX_SLOTS; ++fx)
        unloadFx(trackIndex, fx);

    auto connections = getConnections();
    for (auto& conn : connections)
    {
        if (conn.source.nodeID == track.pluginNode->nodeID ||
            conn.destination.nodeID == track.pluginNode->nodeID)
        {
            removeConnection(conn);
        }
    }

    removeNode(track.pluginNode->nodeID);
    track.pluginNode = nullptr;
    track.plugin = nullptr;
}

void PluginHost::setTrackType(int trackIndex, TrackType type)
{
    if (trackIndex < 0 || trackIndex >= NUM_TRACKS) return;
    auto& track = tracks[static_cast<size_t>(trackIndex)];

    if (track.type == type) return;
    track.type = type;

    // Set audio mode on the clip player
    if (track.clipPlayer)
        track.clipPlayer->audioMode = (type == TrackType::Audio);

    // If switching to audio, unload any instrument plugin
    if (type == TrackType::Audio && track.pluginNode != nullptr)
        unloadPlugin(trackIndex);

    // Rewire the track for the new type
    rewireTrack(trackIndex);
}

void PluginHost::rewireTrack(int trackIndex)
{
    auto& track = tracks[static_cast<size_t>(trackIndex)];

    // Remove all audio connections for this track's nodes
    auto connections = getConnections();
    for (auto& conn : connections)
    {
        // Check if this connection involves any of this track's nodes (except MIDI input/output)
        auto srcID = conn.source.nodeID;
        auto dstID = conn.destination.nodeID;

        bool isTrackNode = (track.pluginNode != nullptr && (srcID == track.pluginNode->nodeID || dstID == track.pluginNode->nodeID))
                        || (srcID == track.gainNode->nodeID || dstID == track.gainNode->nodeID)
                        || (srcID == track.clipPlayerNode->nodeID || dstID == track.clipPlayerNode->nodeID);

        for (int fx = 0; fx < Track::NUM_FX_SLOTS && !isTrackNode; ++fx)
        {
            if (track.fxSlots[fx].node != nullptr &&
                (srcID == track.fxSlots[fx].node->nodeID || dstID == track.fxSlots[fx].node->nodeID))
                isTrackNode = true;
        }

        if (isTrackNode)
            removeConnection(conn);
    }

    // Reconnect: gain -> output is always there
    for (int ch = 0; ch < 2; ++ch)
        addConnection({ { track.gainNode->nodeID, ch }, { audioOutputNode->nodeID, ch } });

    // Reconnect the audio chain
    if (track.type == TrackType::Audio || track.pluginNode != nullptr)
        connectTrackAudio(trackIndex);

    updateMidiRouting();
}

void PluginHost::connectTrackAudio(int trackIndex)
{
    auto& track = tracks[static_cast<size_t>(trackIndex)];

    juce::AudioProcessorGraph::NodeID lastNodeID{0};

    if (track.type == TrackType::Audio)
    {
        // Audio track: audio input → clip player → FX → gain
        // Route audio input to clip player so it can record
        for (int ch = 0; ch < 2; ++ch)
            addConnection({ { audioInputNode->nodeID, ch },
                            { track.clipPlayerNode->nodeID, ch } });
        lastNodeID = track.clipPlayerNode->nodeID;
    }
    else
    {
        // MIDI track: clip player MIDI → plugin → FX → gain
        if (track.pluginNode == nullptr) return;

        addConnection({ { track.clipPlayerNode->nodeID, AudioProcessorGraph::midiChannelIndex },
                        { track.pluginNode->nodeID, AudioProcessorGraph::midiChannelIndex } });
        lastNodeID = track.pluginNode->nodeID;
    }

    // Safety check: lastNodeID must have been assigned a valid value
    if (lastNodeID.uid == 0) return;

    for (int fx = 0; fx < Track::NUM_FX_SLOTS; ++fx)
    {
        if (track.fxSlots[fx].node != nullptr && !track.fxSlots[fx].bypassed)
        {
            for (int ch = 0; ch < 2; ++ch)
                addConnection({ { lastNodeID, ch }, { track.fxSlots[fx].node->nodeID, ch } });
            lastNodeID = track.fxSlots[fx].node->nodeID;
        }
    }

    // Last node -> Gain
    for (int ch = 0; ch < 2; ++ch)
        addConnection({ { lastNodeID, ch }, { track.gainNode->nodeID, ch } });
}

bool PluginHost::loadFx(int trackIndex, int slotIndex, const juce::PluginDescription& desc, juce::String& errorMsg)
{
    if (trackIndex < 0 || trackIndex >= NUM_TRACKS) return false;
    if (slotIndex < 0 || slotIndex >= Track::NUM_FX_SLOTS) return false;

    unloadFx(trackIndex, slotIndex);

#if JUCE_IOS
    // AUv3 plugins on iOS REQUIRE async instantiation
    std::unique_ptr<juce::AudioPluginInstance> instance;
    std::atomic<bool> finished{false};

    formatManager.createPluginInstanceAsync(desc, storedSampleRate, storedBlockSize,
        [&](std::unique_ptr<juce::AudioPluginInstance> result, const juce::String& err)
        {
            instance = std::move(result);
            if (err.isNotEmpty()) errorMsg = err;
            finished.store(true);
        });

    auto deadline = juce::Time::getMillisecondCounter() + 15000;
    while (!finished.load() && juce::Time::getMillisecondCounter() < deadline)
        AUScanner::pumpRunLoop(100);

    if (!finished.load())
    {
        errorMsg = "FX instantiation timed out";
        auto& track = tracks[static_cast<size_t>(trackIndex)];
        track.fxSlots[slotIndex].processor = nullptr;
        track.fxSlots[slotIndex].node = nullptr;
        return false;
    }
    if (instance == nullptr) { if (errorMsg.isEmpty()) errorMsg = "FX instance is null"; return false; }
#else
    auto instance = formatManager.createPluginInstance(desc, storedSampleRate, storedBlockSize, errorMsg);
    if (instance == nullptr) return false;
#endif

    auto& track = tracks[static_cast<size_t>(trackIndex)];
    track.fxSlots[slotIndex].node = addNode(std::move(instance));
    track.fxSlots[slotIndex].bypassed = false;

    if (track.fxSlots[slotIndex].node != nullptr)
        track.fxSlots[slotIndex].processor = track.fxSlots[slotIndex].node->getProcessor();
    else
    {
        track.fxSlots[slotIndex].processor = nullptr;
        errorMsg = "Failed to add FX to audio graph";
        return false;
    }

    // Rewire the entire track chain
    rewireTrack(trackIndex);
    prepareToPlay(storedSampleRate, storedBlockSize);
    return true;
}

void PluginHost::unloadFx(int trackIndex, int slotIndex)
{
    if (trackIndex < 0 || trackIndex >= NUM_TRACKS) return;
    if (slotIndex < 0 || slotIndex >= Track::NUM_FX_SLOTS) return;

    auto& track = tracks[static_cast<size_t>(trackIndex)];
    auto& slot = track.fxSlots[slotIndex];
    if (slot.node == nullptr) return;

    // Remove all connections to/from this FX node
    auto connections = getConnections();
    for (auto& conn : connections)
    {
        if (conn.source.nodeID == slot.node->nodeID ||
            conn.destination.nodeID == slot.node->nodeID)
            removeConnection(conn);
    }

    removeNode(slot.node->nodeID);
    slot.node = nullptr;
    slot.processor = nullptr;
    slot.bypassed = false;

    rewireTrack(trackIndex);
}

void PluginHost::setFxBypassed(int trackIndex, int slotIndex, bool bypassed)
{
    if (trackIndex < 0 || trackIndex >= NUM_TRACKS) return;
    if (slotIndex < 0 || slotIndex >= Track::NUM_FX_SLOTS) return;

    tracks[static_cast<size_t>(trackIndex)].fxSlots[slotIndex].bypassed = bypassed;
    rewireTrack(trackIndex);
}

void PluginHost::setSelectedTrack(int index)
{
    if (index < 0 || index >= NUM_TRACKS) return;

    // Clean up previous track when switching
    if (selectedTrack != index)
    {
        auto& oldTrack = tracks[static_cast<size_t>(selectedTrack)];
        if (oldTrack.clipPlayer != nullptr)
        {
            if (!oldTrack.clipPlayer->armLocked.load())
                oldTrack.clipPlayer->armed.store(false);
            // Synchronously flush note-offs before MIDI disconnect.
            // The async sendAllNotesOff flag can race with updateMidiRouting()
            // which rebuilds graph topology, so we flush directly instead.
            juce::MidiBuffer noteOffBuf;
            oldTrack.clipPlayer->flushNoteOffs(noteOffBuf);
            // Also set the flag as a safety net for the next processBlock
            oldTrack.clipPlayer->sendAllNotesOff.store(true);
        }
    }

    selectedTrack = index;

    // Auto-arm new track
    auto& newTrack = tracks[static_cast<size_t>(selectedTrack)];
    if (newTrack.clipPlayer != nullptr)
        newTrack.clipPlayer->armed.store(true);

    updateMidiRouting();
}

void PluginHost::updateMidiRouting()
{
    // Remove all MIDI connections from MIDI input node
    auto connections = getConnections();
    for (auto& conn : connections)
    {
        if (conn.source.nodeID == midiInputNode->nodeID &&
            conn.source.channelIndex == AudioProcessorGraph::midiChannelIndex)
        {
            removeConnection(conn);
        }
    }

    // Connect MIDI input to selected track's ClipPlayerNode
    auto& track = tracks[static_cast<size_t>(selectedTrack)];
    addConnection({ { midiInputNode->nodeID, AudioProcessorGraph::midiChannelIndex },
                    { track.clipPlayerNode->nodeID, AudioProcessorGraph::midiChannelIndex } });
}

void PluginHost::sendTestNoteOn(int noteNumber, float velocity)
{
    auto msg = juce::MidiMessage::noteOn(1, noteNumber, velocity);
    msg.setTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001);
    midiCollector.addMessageToQueue(msg);
}

void PluginHost::sendTestNoteOff(int noteNumber)
{
    auto msg = juce::MidiMessage::noteOff(1, noteNumber);
    msg.setTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001);
    midiCollector.addMessageToQueue(msg);
}

void PluginHost::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    midiCollector.removeNextBlockOfMessages(midiMessages, buffer.getNumSamples());

    // Snapshot position before advance (for loop wrap detection)
    double posBeforeAdvance = engine.getPositionInBeats();

    // Advance transport
    engine.advancePosition(buffer.getNumSamples(), storedSampleRate);

    // Snapshot PlayHead state once for consistent use by all plugins this block
    hostPlayHead.captureState(storedSampleRate);
    bool isPlaying = hostPlayHead.isPlayingCached();

    // ── MIDI Clock ──
    if (isPlaying && !midiClockWasPlaying)
    {
        // Send Song Position Pointer then Start
        double beatPos = engine.getPositionInBeats();
        int midiBeats = static_cast<int>(beatPos * 6.0); // MIDI beat = 1/16 note = 6 clocks
        midiMessages.addEvent(juce::MidiMessage::songPositionPointer(midiBeats), 0);
        midiMessages.addEvent(juce::MidiMessage(0xFA), 0); // Start
        midiClockPulseAccum = 0.0;
    }
    else if (!isPlaying && midiClockWasPlaying)
    {
        midiMessages.addEvent(juce::MidiMessage(0xFC), 0); // Stop
        midiClockPulseAccum = 0.0;
    }
    // Loop wrap re-sync: if position jumped backward, send SPP to re-sync plugins
    else if (isPlaying && midiClockWasPlaying)
    {
        double posAfter = engine.getPositionInBeats();
        if (posAfter < posBeforeAdvance - 0.5) // wrapped backward (loop)
        {
            int midiBeats = static_cast<int>(posAfter * 6.0);
            midiMessages.addEvent(juce::MidiMessage::songPositionPointer(midiBeats), 0);
            midiMessages.addEvent(juce::MidiMessage(0xFB), 0); // Continue (not Start, to avoid retriggering)
        }
    }
    midiClockWasPlaying = isPlaying;

    // Send clock pulses (0xF8) at 24 PPQN while playing
    if (isPlaying)
    {
        double bpm = engine.getBpm();
        if (bpm <= 0.0) bpm = 120.0;
        double pulsesPerSecond = (bpm / 60.0) * 24.0;
        double pulsesThisBlock = pulsesPerSecond * (static_cast<double>(buffer.getNumSamples()) / storedSampleRate);
        midiClockPulseAccum += pulsesThisBlock;

        int numPulses = static_cast<int>(midiClockPulseAccum);
        if (numPulses > 0)
        {
            midiClockPulseAccum -= numPulses;
            double samplesPerPulse = static_cast<double>(buffer.getNumSamples()) / numPulses;
            for (int p = 0; p < numPulses; ++p)
            {
                int sampleOffset = static_cast<int>(p * samplesPerPulse);
                midiMessages.addEvent(juce::MidiMessage(0xF8), sampleOffset);
            }
        }
    }

    AudioProcessorGraph::processBlock(buffer, midiMessages);

    // Apply automation during playback
    if (engine.isPlaying() && !engine.isInCountIn())
    {
        double beat = engine.getPositionInBeats();

        for (int t = 0; t < NUM_TRACKS; ++t)
        {
            auto& track = tracks[static_cast<size_t>(t)];
            if (track.plugin == nullptr) continue;

            auto& params = track.plugin->getParameters();

            // Check if user is currently touching a param on this track
            int touchedIdx = track.touchedParamIndex.load();
            int64_t touchedTime = track.touchedParamTime.load();
            bool hasFreshTouch = (touchedIdx >= 0)
                && ((static_cast<int64_t>(juce::Time::getMillisecondCounter()) - touchedTime) < 500);

            // If the touch has expired, clear it
            if (touchedIdx >= 0 && !hasFreshTouch)
                track.touchedParamIndex.store(-1);

            {
                const juce::SpinLock::ScopedLockType lock(track.automationLock);
                for (auto* lane : track.automationLanes)
                {
                    if (lane->parameterIndex >= 0 && lane->parameterIndex < params.size()
                        && !lane->points.isEmpty())
                    {
                        // Skip automation playback for the param the user is actively touching
                        if (hasFreshTouch && lane->parameterIndex == touchedIdx)
                            continue;

                        float val = lane->getValueAtBeat(beat);
                        if (val >= 0.0f && val <= 1.0f && std::isfinite(val))
                            params[lane->parameterIndex]->setValue(val);
                    }
                }
            }
        }
    }

    // Render metronome click on top of the output
    engine.renderMetronome(buffer, buffer.getNumSamples(), storedSampleRate);

    // Feed spectrum analyzer (mono mix of L+R)
    auto* spectrum = spectrumDisplay.load();
    if (spectrum != nullptr && buffer.getNumChannels() >= 2)
    {
        int n = buffer.getNumSamples();
        const float* L = buffer.getReadPointer(0);
        const float* R = buffer.getReadPointer(1);

        // Stack-allocate a small mono buffer
        float mono[2048];
        int count = juce::jmin(n, 2048);
        for (int i = 0; i < count; ++i)
            mono[i] = (L[i] + R[i]) * 0.5f;

        spectrum->pushSamples(mono, count);

        if (auto* wt = waveTerrainDisplay.load())
            wt->pushSamples(mono, count);
        if (auto* st = shaderToyDisplay.load())
            st->pushSamples(mono, count);
        if (auto* az = analyzerDisplay.load())
            az->pushSamples(mono, count);
        if (auto* gs = geissDisplay.load())
            gs->pushSamples(mono, count);
        if (auto* pm = projectMDisplay.load())
            pm->pushSamples(mono, count);
        if (auto* hb = heartbeatDisplay.load())
            hb->pushSamples(mono, count);
        if (auto* br = bioResonanceDisplay.load())
            br->pushSamples(mono, count);
        if (auto* fs = fluidSimDisplay.load())
            fs->pushSamples(mono, count);
        if (auto* rm = rayMarchDisplay.load())
            rm->pushSamples(mono, count);
    }

}

// ── Offline Rendering ──────────────────────────────────────────────

void PluginHost::prepareForOfflineRender(double sampleRate, int blockSize)
{
    offlineSavedSampleRate = storedSampleRate;
    offlineSavedBlockSize = storedBlockSize;
    storedSampleRate = sampleRate;
    storedBlockSize = blockSize;

    // Reconfigure graph for offline
    setPlayConfigDetails(2, 2, sampleRate, blockSize);
    releaseResources();
    prepareToPlay(sampleRate, blockSize);
    setNonRealtime(true);
}

void PluginHost::processBlockOffline(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi, int numSamples)
{
    // Advance transport
    engine.advancePosition(numSamples, storedSampleRate);
    hostPlayHead.captureState(storedSampleRate);

    // Process the graph (all tracks, plugins, FX, gain)
    AudioProcessorGraph::processBlock(buffer, midi);

    // Apply automation
    if (engine.isPlaying() && !engine.isInCountIn())
    {
        double beat = engine.getPositionInBeats();
        for (int t = 0; t < NUM_TRACKS; ++t)
        {
            auto& track = tracks[static_cast<size_t>(t)];
            if (!track.plugin) continue;
            auto params = track.plugin->getParameters();
            const juce::SpinLock::ScopedLockType lock(track.automationLock);
            for (auto* lane : track.automationLanes)
            {
                if (lane->parameterIndex >= 0 && lane->parameterIndex < params.size())
                {
                    float val = lane->getValueAtBeat(static_cast<float>(beat));
                    if (val >= 0.0f && val <= 1.0f && std::isfinite(val))
                        params[lane->parameterIndex]->setValue(val);
                }
            }
        }
    }

    // NO metronome in export
    // NO visualizer feeds in export
}

void PluginHost::restoreFromOfflineRender()
{
    setNonRealtime(false);
    storedSampleRate = offlineSavedSampleRate;
    storedBlockSize = offlineSavedBlockSize;
    setPlayConfigDetails(2, 2, storedSampleRate, storedBlockSize);
    releaseResources();
    prepareToPlay(storedSampleRate, storedBlockSize);
}
