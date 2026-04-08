#include "ClipPlayerNode.h"

ClipPlayerNode::ClipPlayerNode(SequencerEngine& eng)
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      engine(eng)
{
    // Pre-allocate full capacity so audio thread never triggers a resize
    slots.resize(static_cast<size_t>(MAX_SLOTS));
    wasInsideClip.resize(static_cast<size_t>(MAX_SLOTS), false);
    slotCount.store(0);  // start with 0 logical slots — grow as needed
}

void ClipPlayerNode::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;
}

void ClipPlayerNode::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    int numSamples = buffer.getNumSamples();

    // Send all-notes-off if flagged (stop/panic — hard kill)
    if (sendAllNotesOff.exchange(false))
    {
        killActiveNotes(midi, 0, true);
        lastPositionInBeats = -1.0;
        std::fill(wasInsideClip.begin(), wasInsideClip.end(), false);
    }

    // Check if we should start recording (not during count-in)
    if (engine.isPlaying() && engine.isRecording() && !engine.isInCountIn() && armed.load() && atomicRecordingSlot < 0)
    {
        // First check for explicitly armed slots
        int targetSlot = -1;
        for (int i = 0; i < getNumSlots(); ++i)
        {
            if (slots[static_cast<size_t>(i)].state.load() == ClipSlot::Armed)
            {
                targetSlot = i;
                break;
            }
        }

        // If no slot is armed, auto-find an empty/available slot
        if (targetSlot < 0)
        {
            targetSlot = findOrCreateEmptySlot();
        }

        if (targetSlot >= 0)
        {
            auto& slot = slots[static_cast<size_t>(targetSlot)];
            double beatsPerSample = (engine.getBpm() / 60.0) / currentSampleRate;
            double blockStartPos = engine.getPositionInBeats() - (beatsPerSample * numSamples);

            if (audioMode)
            {
                // Audio recording — create AudioClip
                slot.audioClip = std::make_unique<AudioClip>();
                slot.audioClip->sampleRate = currentSampleRate;
                if (engine.isLoopEnabled() && engine.hasLoopRegion())
                {
                    double ls = engine.getLoopStart();
                    slot.audioClip->timelinePosition = ls;
                    slot.audioClip->lengthInBeats = engine.getLoopEnd() - ls;
                    recordStartBeat = ls;
                    // Pre-allocate buffer for the loop length
                    int loopSamples = static_cast<int>((slot.audioClip->lengthInBeats / (engine.getBpm() / 60.0)) * currentSampleRate);
                    slot.audioClip->samples.setSize(2, loopSamples, false, true);
                }
                else
                {
                    slot.audioClip->timelinePosition = blockStartPos;
                    recordStartBeat = blockStartPos;
                    // Start with 10 seconds buffer, will grow if needed
                    slot.audioClip->samples.setSize(2, static_cast<int>(currentSampleRate * 10), false, true);
                }
            }
            else
            {
                // MIDI recording
                if (slot.clip == nullptr)
                    slot.clip = std::make_unique<MidiClip>();

                if (engine.isLoopEnabled() && engine.hasLoopRegion())
                {
                    double ls = engine.getLoopStart();
                    slot.clip->timelinePosition = ls;
                    slot.clip->lengthInBeats = engine.getLoopEnd() - ls;
                    recordStartBeat = ls;
                }
                else
                {
                    slot.clip->timelinePosition = blockStartPos;
                    recordStartBeat = blockStartPos;
                }
            }

            slot.state.store(ClipSlot::Recording);
            atomicRecordingSlot = targetSlot;
        }
    }

    // Handle recording
    if (atomicRecordingSlot >= 0 && engine.isPlaying())
    {
        if (audioMode)
            processAudioRecording(buffer, numSamples);
        else
            processRecording(midi, numSamples);
    }

    // Handle playback for all playing clips (skip during count-in)
    if (engine.isPlaying() && !engine.isInCountIn())
    {
        double currentPos = engine.getPositionInBeats();

        // Detect loop wrap-around or play start (position jumped) — kill all sounding notes
        bool positionJumped = (currentPos < lastPositionInBeats - 0.001);
        if (positionJumped)
        {
            killActiveNotes(midi, 0);
            std::fill(wasInsideClip.begin(), wasInsideClip.end(), false);
        }

        // Run clip playback for all playing and recording slots
        // Recording slots also play back so the user hears first-pass notes during loop recording
        for (int i = 0; i < getNumSlots(); ++i)
        {
            auto slotState = slots[static_cast<size_t>(i)].state.load();
            if (slotState == ClipSlot::Playing || slotState == ClipSlot::Recording)
            {
                if (audioMode && slots[static_cast<size_t>(i)].audioClip != nullptr)
                    processAudioClipPlayback(i, buffer, numSamples);
                else
                    processClipPlayback(i, midi, numSamples);
            }
        }

        lastPositionInBeats = currentPos;
    }

    // Audio passes through unchanged (this node only handles MIDI)
}

void ClipPlayerNode::processClipPlayback(int slotIndex, juce::MidiBuffer& midi, int numSamples)
{
    auto& slot = slots[static_cast<size_t>(slotIndex)];
    if (slot.clip == nullptr) return;

    auto& clip = *slot.clip;
    double bpm = engine.getBpm();
    double beatsPerSample = (bpm / 60.0) / currentSampleRate;
    double beatsThisBlock = beatsPerSample * numSamples;
    double blockStart = engine.getPositionInBeats() - beatsThisBlock;
    if (blockStart < 0.0) blockStart = 0.0;
    double blockEnd = blockStart + beatsThisBlock;

    double clipStart = clip.timelinePosition;
    double clipEnd = clipStart + clip.lengthInBeats;
    if (clip.lengthInBeats <= 0.0) return;

    // Detect clip exit → kill active notes
    bool isInside = (blockEnd > clipStart && blockStart < clipEnd);
    if (wasInsideClip[static_cast<size_t>(slotIndex)] && !isInside)
        killActiveNotes(midi, 0);
    wasInsideClip[static_cast<size_t>(slotIndex)] = isInside;
    if (!isInside) return;

    // Clip-relative time range for this block
    double relStart = juce::jmax(0.0, blockStart - clipStart);
    double relEnd = blockEnd - clipStart;

    // Use JUCE's getNextIndexAtTime to find first event in range
    int startIdx = clip.events.getNextIndexAtTime(relStart);

    for (int e = startIdx; e < clip.events.getNumEvents(); ++e)
    {
        auto* holder = clip.events.getEventPointer(e);
        double eventBeat = holder->message.getTimeStamp();

        if (eventBeat >= relEnd)
            break;

        // Calculate sample offset within block
        double eventAbsBeat = clipStart + eventBeat;
        int samplePos = static_cast<int>((eventAbsBeat - blockStart) / beatsPerSample);
        samplePos = juce::jlimit(0, numSamples - 1, samplePos);

        midi.addEvent(holder->message, samplePos);

        if (holder->message.isNoteOn())
            activePlaybackNotes[holder->message.getNoteNumber() & 0x7F] = true;
        else if (holder->message.isNoteOff())
            activePlaybackNotes[holder->message.getNoteNumber() & 0x7F] = false;
    }
}

void ClipPlayerNode::processRecording(const juce::MidiBuffer& incomingMidi, int numSamples)
{
    if (atomicRecordingSlot < 0 || atomicRecordingSlot >= getNumSlots()) return;

    auto& slot = slots[static_cast<size_t>(atomicRecordingSlot)];
    if (slot.clip == nullptr) { atomicRecordingSlot = -1; return; }

    double bpm = engine.getBpm();
    double beatsPerSample = (bpm / 60.0) / currentSampleRate;
    double beatsThisBlock = beatsPerSample * numSamples;
    // Use start-of-block position (engine already advanced past this block)
    double pos = engine.getPositionInBeats() - beatsThisBlock;

    for (const auto metadata : incomingMidi)
    {
        auto msg = metadata.getMessage();
        if (msg.isNoteOnOrOff() || msg.isController() || msg.isPitchWheel() ||
            msg.isAftertouch() || msg.isChannelPressure())
        {
            double beatTimestamp = (pos - recordStartBeat) + (metadata.samplePosition * beatsPerSample);

            // Don't record notes that arrive before the loop/clip start
            if (beatTimestamp < 0.0) continue;

            // In loop mode the clip length is fixed to the loop region.
            // Skip any notes that fall at or beyond the clip end — they belong
            // to a second pass and would otherwise duplicate already-recorded content.
            bool loopMode = engine.isLoopEnabled() && engine.hasLoopRegion();
            if (loopMode && slot.clip->lengthInBeats > 0.0 && beatTimestamp >= slot.clip->lengthInBeats)
                continue;

            msg.setTimeStamp(beatTimestamp);
            slot.clip->events.addEvent(msg);

            // Only auto-extend clip length when not in loop mode (loop length is already fixed)
            if (!loopMode && beatTimestamp > slot.clip->lengthInBeats - 0.1)
            {
                // Round up to next bar
                slot.clip->lengthInBeats = std::ceil(beatTimestamp / 4.0) * 4.0;
                if (slot.clip->lengthInBeats < 4.0) slot.clip->lengthInBeats = 4.0;
            }
        }
    }
}

void ClipPlayerNode::triggerSlot(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= getNumSlots()) return;

    auto& slot = slots[static_cast<size_t>(slotIndex)];
    auto currentState = slot.state.load();

    bool slotIsEmpty = currentState == ClipSlot::Empty ||
                       (slot.clip != nullptr && !slot.hasContent() && currentState == ClipSlot::Stopped);

    if (currentState == ClipSlot::Armed)
    {
        // Click armed slot → disarm it
        slot.state.store(slot.clip != nullptr ? ClipSlot::Stopped : ClipSlot::Empty);
        return;
    }

    if (currentState == ClipSlot::Recording)
    {
        // Click recording slot → stop recording, auto-set to Playing
        atomicRecordingSlot = -1;
        if (slot.clip != nullptr)
        {
            slot.clip->events.sort();
            closeOpenNotes(*slot.clip);
            slot.clip->events.updateMatchedPairs();
        }
        slot.state.store(slot.hasContent() ? ClipSlot::Playing : ClipSlot::Empty);
        return;
    }

    if (currentState == ClipSlot::Playing)
    {
        // Click playing slot → stop playback
        slot.state.store(ClipSlot::Stopped);
        sendAllNotesOff.store(true);
        return;
    }

    if (slotIsEmpty && armed.load())
    {
        if (engine.isRecording() && engine.isPlaying())
        {
            // Transport already running with REC → start recording immediately
            if (slot.clip == nullptr)
                slot.clip = std::make_unique<MidiClip>();

            if (engine.isLoopEnabled() && engine.hasLoopRegion())
            {
                double ls = engine.getLoopStart();
                slot.clip->timelinePosition = ls;
                slot.clip->lengthInBeats = engine.getLoopEnd() - ls;
                recordStartBeat = ls;
            }
            else
            {
                slot.clip->timelinePosition = engine.getPositionInBeats();
                recordStartBeat = engine.getPositionInBeats();
            }

            slot.state.store(ClipSlot::Recording);
            atomicRecordingSlot = slotIndex;
        }
        else
        {
            // Transport not running → arm the slot, recording starts when transport plays
            if (slot.clip == nullptr)
                slot.clip = std::make_unique<MidiClip>();
            slot.state.store(ClipSlot::Armed);
        }
        return;
    }

    if (slot.hasContent())
    {
        // Click a clip with content → start playback
        stopAllSlots();
        slot.state.store(ClipSlot::Playing);
    }
}

void ClipPlayerNode::stopSlot(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= getNumSlots()) return;

    auto& slot = slots[static_cast<size_t>(slotIndex)];
    auto state = slot.state.load();

    // Playing and Armed clips stay in their state — they resume when transport plays again
    if (state == ClipSlot::Armed || state == ClipSlot::Empty || state == ClipSlot::Stopped)
        return;

    if (state == ClipSlot::Playing)
    {
        sendAllNotesOff.store(true);
        // Playing clips stay in Playing state — they resume when transport plays again
        if (!slot.hasContent())
        {
            slot.clip = nullptr;
            slot.state.store(ClipSlot::Empty);
        }
        // else: keep Playing — clip will resume on next transport start
        return;
    }

    if (state == ClipSlot::Recording)
    {
        atomicRecordingSlot = -1;
        if (slot.clip != nullptr)
        {
            slot.clip->events.sort();
            closeOpenNotes(*slot.clip);
            slot.clip->events.updateMatchedPairs();
        }
        // After recording, go to Playing only if we captured notes — otherwise clean up
        if (slot.hasContent())
        {
            slot.state.store(ClipSlot::Playing);
        }
        else
        {
            slot.clip = nullptr;
            slot.state.store(ClipSlot::Empty);
        }
        sendAllNotesOff.store(true);
    }
}

void ClipPlayerNode::stopAllSlots()
{
    for (int i = 0; i < getNumSlots(); ++i)
        stopSlot(i);
}

void ClipPlayerNode::closeOpenNotes(MidiClip& clip)
{
    // Find note-ons without matching note-offs and add note-offs at clip end
    std::set<int> openNotes; // (channel << 8) | noteNumber
    for (int e = 0; e < clip.events.getNumEvents(); ++e)
    {
        auto& msg = clip.events.getEventPointer(e)->message;
        int key = (msg.getChannel() << 8) | msg.getNoteNumber();
        if (msg.isNoteOn())
            openNotes.insert(key);
        else if (msg.isNoteOff())
            openNotes.erase(key);
    }
    for (int key : openNotes)
    {
        int ch = (key >> 8) & 0x1F;
        int note = key & 0x7F;
        auto noteOff = juce::MidiMessage::noteOff(ch, note);
        noteOff.setTimeStamp(clip.lengthInBeats - 0.001);
        clip.events.addEvent(noteOff);
    }
    if (!openNotes.empty())
        clip.events.sort();
}

void ClipPlayerNode::killActiveNotes(juce::MidiBuffer& midi, int sampleOffset, bool hard)
{
    // Send explicit note-offs for every tracked note
    for (int n = 0; n < 128; ++n)
    {
        if (activePlaybackNotes[n])
            midi.addEvent(juce::MidiMessage::noteOff(1, n), sampleOffset);
    }
    std::memset(activePlaybackNotes, 0, sizeof(activePlaybackNotes));

    // Hard kill: send note-off for ALL possible notes on channel 1
    // Some plugins ignore CC 120/123, so brute-force every note
    if (hard)
    {
        for (int note = 0; note < 128; ++note)
            midi.addEvent(juce::MidiMessage::noteOff(1, note), sampleOffset);
        for (int ch = 1; ch <= 16; ++ch)
        {
            midi.addEvent(juce::MidiMessage::allNotesOff(ch), sampleOffset);
            midi.addEvent(juce::MidiMessage::allSoundOff(ch), sampleOffset);
        }
    }
}

// ── Audio clip recording ──
void ClipPlayerNode::processAudioRecording(const juce::AudioBuffer<float>& inputBuffer, int numSamples)
{
    if (atomicRecordingSlot < 0 || atomicRecordingSlot >= getNumSlots()) return;
    auto& slot = slots[static_cast<size_t>(atomicRecordingSlot)];
    if (slot.audioClip == nullptr) { atomicRecordingSlot = -1; return; }

    auto& clip = *slot.audioClip;
    double bpm = engine.getBpm();
    double beatsPerSample = (bpm / 60.0) / currentSampleRate;
    double beatsThisBlock = beatsPerSample * numSamples;
    double pos = engine.getPositionInBeats() - beatsThisBlock;

    double beatOffset = pos - recordStartBeat;
    if (beatOffset < 0.0) beatOffset = 0.0;

    int sampleOffset = static_cast<int>(beatOffset / beatsPerSample);

    // Check if we need to grow the buffer
    int needed = sampleOffset + numSamples;
    if (needed > clip.samples.getNumSamples())
    {
        // In loop mode, don't grow beyond the loop
        bool loopMode = engine.isLoopEnabled() && engine.hasLoopRegion();
        if (loopMode) return; // beyond loop — skip

        int newSize = juce::jmax(needed + static_cast<int>(currentSampleRate), clip.samples.getNumSamples() * 2);
        clip.samples.setSize(2, newSize, true);
    }

    // Copy input audio into the clip buffer
    int numCh = juce::jmin(inputBuffer.getNumChannels(), clip.samples.getNumChannels());
    for (int ch = 0; ch < numCh; ++ch)
    {
        int copyLen = juce::jmin(numSamples, clip.samples.getNumSamples() - sampleOffset);
        if (copyLen > 0)
            clip.samples.copyFrom(ch, sampleOffset, inputBuffer, ch, 0, copyLen);
    }

    // Update clip length
    double endBeat = beatOffset + beatsThisBlock;
    if (endBeat > clip.lengthInBeats)
    {
        bool loopMode = engine.isLoopEnabled() && engine.hasLoopRegion();
        if (!loopMode)
            clip.lengthInBeats = std::ceil(endBeat / 4.0) * 4.0;
    }
}

// ── Audio clip playback ──
void ClipPlayerNode::processAudioClipPlayback(int slotIndex, juce::AudioBuffer<float>& buffer, int numSamples)
{
    auto& slot = slots[static_cast<size_t>(slotIndex)];
    if (slot.audioClip == nullptr) return;

    auto& clip = *slot.audioClip;
    double bpm = engine.getBpm();
    double beatsPerSample = (bpm / 60.0) / currentSampleRate;
    double beatsThisBlock = beatsPerSample * numSamples;
    double blockStart = engine.getPositionInBeats() - beatsThisBlock;
    if (blockStart < 0.0) blockStart = 0.0;

    double clipStart = clip.timelinePosition;
    double clipEnd = clipStart + clip.lengthInBeats;
    if (clip.lengthInBeats <= 0.0) return;

    double blockEnd = blockStart + beatsThisBlock;
    bool isInside = (blockEnd > clipStart && blockStart < clipEnd);
    if (!isInside) return;

    // Calculate sample range in the clip buffer
    double relStart = juce::jmax(0.0, blockStart - clipStart);
    int clipSampleStart = static_cast<int>(relStart / beatsPerSample);
    int clipTotalSamples = clip.samples.getNumSamples();

    int numCh = juce::jmin(buffer.getNumChannels(), clip.samples.getNumChannels());
    for (int s = 0; s < numSamples; ++s)
    {
        int clipSample = clipSampleStart + s;
        if (clipSample >= 0 && clipSample < clipTotalSamples)
        {
            for (int ch = 0; ch < numCh; ++ch)
                buffer.addSample(ch, s, clip.samples.getSample(ch, clipSample));
        }
    }
}
