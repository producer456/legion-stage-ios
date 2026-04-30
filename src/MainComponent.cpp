#include "MainComponent.h"
#include "AUPresetHelper.h"
#include "FileAccessHelper.h"
#include "DeviceMotion.h"
#include "BuiltinSamplerProcessor.h"
#if JUCE_IOS
#include "AUScanner.h"
#endif
#if JUCE_IOS || JUCE_MAC
#include <mach/mach.h>
#endif

MainComponent::MainComponent()
{
#if JUCE_IOS
    deviceTier = AUScanner::getDeviceTier();
#else
    deviceTier = AUScanner::DeviceTier::High;
#endif

    // Register bundled fonts so look-and-feels can request them by
    // name.  Plus Jakarta Sans is the closest free match to Novation's
    // brand typeface (Brockmann is licensed and can't ship).
    {
        auto fontsDir = juce::File::getSpecialLocation(juce::File::currentApplicationFile)
                          .getChildFile("Contents/Resources/fonts");
        if (!fontsDir.isDirectory())
            fontsDir = juce::File::getSpecialLocation(juce::File::currentApplicationFile)
                          .getChildFile("fonts");
        if (fontsDir.isDirectory())
        {
            for (const auto& f : juce::RangedDirectoryIterator(fontsDir, false, "*.ttf"))
            {
                juce::MemoryBlock data;
                if (f.getFile().loadFileAsData(data))
                    juce::Typeface::createSystemTypefaceFor(data.getData(), data.getSize());
            }
        }
    }

    themeManager.setTheme(ThemeManager::Ioniq, this);

    auto result = deviceManager.initialiseWithDefaultDevices(2, 2);  // 2 in, 2 out for mic recording
    if (result.isNotEmpty())
        DBG("Audio device init error: " + result);

    audioPlayer.setProcessor(&pluginHost);
    deviceManager.addAudioCallback(&audioPlayer);

    addAndMakeVisible(spectrumDisplay);
    pluginHost.spectrumDisplay = &spectrumDisplay;

    addAndMakeVisible(lissajousDisplay);

    addAndMakeVisible(waveTerrainDisplay);
    waveTerrainDisplay.setVisible(false);
    pluginHost.waveTerrainDisplay = &waveTerrainDisplay;

    addAndMakeVisible(shaderToyDisplay);
    shaderToyDisplay.setVisible(false);
    pluginHost.shaderToyDisplay = &shaderToyDisplay;

    addAndMakeVisible(analyzerDisplay);
    analyzerDisplay.setVisible(false);
    pluginHost.analyzerDisplay = &analyzerDisplay;

    addAndMakeVisible(geissDisplay);
    geissDisplay.setVisible(false);
    pluginHost.geissDisplay = &geissDisplay;

    addAndMakeVisible(projectMDisplay);
    projectMDisplay.setVisible(false);
    pluginHost.projectMDisplay = &projectMDisplay;

    heartbeatDisplay = std::make_unique<HeartbeatComponent>(pluginHost.getEngine());
    addAndMakeVisible(*heartbeatDisplay);
    heartbeatDisplay->setVisible(false);
    pluginHost.heartbeatDisplay = heartbeatDisplay.get();

    bioResonanceDisplay = std::make_unique<BioResonanceComponent>(pluginHost.getEngine(), heartRateManager);
    addAndMakeVisible(*bioResonanceDisplay);
    bioResonanceDisplay->setVisible(false);
    pluginHost.bioResonanceDisplay = bioResonanceDisplay.get();

    addAndMakeVisible(fluidSimDisplay);
    fluidSimDisplay.setVisible(false);
    pluginHost.fluidSimDisplay = &fluidSimDisplay;

    addAndMakeVisible(rayMarchDisplay);
    rayMarchDisplay.setVisible(false);
    pluginHost.rayMarchDisplay = &rayMarchDisplay;

    // HealthKit authorization is triggered by the "Connect Watch" button
    // in the BioSync visualizer — no auto-prompt on startup

    // Tap on small visualizer to go fullscreen
    spectrumDisplay.addMouseListener(this, false);
    lissajousDisplay.addMouseListener(this, false);
    waveTerrainDisplay.addMouseListener(this, false);
    shaderToyDisplay.addMouseListener(this, false);
    analyzerDisplay.addMouseListener(this, false);
    geissDisplay.addMouseListener(this, false);
    projectMDisplay.addMouseListener(this, false);
    heartbeatDisplay->addMouseListener(this, false);
    bioResonanceDisplay->addMouseListener(this, false);
    fluidSimDisplay.addMouseListener(this, false);
    rayMarchDisplay.addMouseListener(this, false);

    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        pluginHost.setAudioParams(device->getCurrentSampleRate(),
                                  device->getCurrentBufferSizeSamples());
        pluginHost.prepareToPlay(device->getCurrentSampleRate(),
                                 device->getCurrentBufferSizeSamples());
    }

    // ── Top Bar: Transport + Track Select ──
    addAndMakeVisible(midiLearnButton);
    midiLearnButton.setClickingTogglesState(true);
    midiLearnButton.onClick = [this] {
        midiLearnActive = midiLearnButton.getToggleState();
        if (midiLearnActive)
            statusLabel.setText("MIDI Learn: click a control, then move a CC", juce::dontSendNotification);
        else
        {
            midiLearnTarget = MidiTarget::None;
            statusLabel.setText("MIDI Learn off", juce::dontSendNotification);
        }
    };

    addAndMakeVisible(trackNameLabel);
    trackNameLabel.setJustificationType(juce::Justification::centred);
    trackNameLabel.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 16.0f, juce::Font::bold)));
#if JUCE_IOS
    trackNameLabel.setColour(juce::Label::textColourId, juce::Colour(themeManager.getColors().textSecondary));
#else
    trackNameLabel.setColour(juce::Label::textColourId, juce::Colour(themeManager.getColors().amber));
#endif

    addAndMakeVisible(recordButton);
    recordButton.setClickingTogglesState(true);
    recordButton.onClick = [this] {
        if (midiLearnActive) { startMidiLearn(MidiTarget::Record); return; }
        pluginHost.getEngine().toggleRecord();
    };

    addAndMakeVisible(playButton);
    playButton.onClick = [this] {
        if (midiLearnActive) { startMidiLearn(MidiTarget::Play); return; }
        auto& eng = pluginHost.getEngine();

        const double now = juce::Time::getMillisecondCounterHiRes();
        const bool isDoubleTap = (now - lastPlayTapMs < 1200.0);
        lastPlayTapMs = now;

        if (isDoubleTap)
        {
            // Double-tap — stop playback and jump back to the start.
            // Does NOT resume play; user can tap once more to play
            // from the top.
            eng.stop();
            for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
            {
                auto* cp = pluginHost.getTrack(t).clipPlayer;
                if (cp)
                {
                    cp->stopAllSlots();
                    cp->sendAllNotesOff.store(true);
                }
            }
            eng.resetPosition();
            if (timelineComponent) timelineComponent->repaint();
            return;
        }

        // Single tap — toggle play/stop.
        if (eng.isPlaying())
        {
            eng.stop();
            for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
            {
                auto* cp = pluginHost.getTrack(t).clipPlayer;
                if (cp)
                {
                    cp->stopAllSlots();
                    cp->sendAllNotesOff.store(true);
                }
            }
            if (timelineComponent) timelineComponent->repaint();
        }
        else
        {
            eng.play();
        }
    };

    addAndMakeVisible(stopButton);
    stopButton.onClick = [this] {
        if (midiLearnActive) { startMidiLearn(MidiTarget::Stop); return; }
        auto& eng = pluginHost.getEngine();
        if (!eng.isPlaying())
        {
            eng.resetPosition();
            for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
            {
                auto* cp = pluginHost.getTrack(t).clipPlayer;
                if (cp) cp->stopAllSlots();
            }
        }
        else
        {
            eng.stop();
            for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
            {
                auto* cp = pluginHost.getTrack(t).clipPlayer;
                if (cp)
                {
                    cp->stopAllSlots();
                    cp->sendAllNotesOff.store(true);
                }
            }
        }
        for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
            if (auto* cp = pluginHost.getTrack(t).clipPlayer)
                cp->getArpeggiator().reset();
        // Disable recording when stop is pressed
        if (eng.isRecording())
        {
            eng.toggleRecord();
            recordButton.setToggleState(false, juce::dontSendNotification);
        }
        if (timelineComponent) timelineComponent->repaint();
    };

    addAndMakeVisible(metronomeButton);
    metronomeButton.setClickingTogglesState(true);
    metronomeButton.onClick = [this] {
        if (midiLearnActive) { startMidiLearn(MidiTarget::Metronome); return; }
        pluginHost.getEngine().toggleMetronome();
    };

    addAndMakeVisible(bpmDownButton);
    bpmDownButton.setVisible(false);
    bpmUpButton.setVisible(false);

    addAndMakeVisible(bpmLabel);
    bpmLabel.setText("120 BPM", juce::dontSendNotification);
    bpmLabel.setJustificationType(juce::Justification::centred);
    bpmLabel.addMouseListener(this, false);

    addAndMakeVisible(beatLabel);

    addAndMakeVisible(tapTempoButton);
    tapTempoButton.onClick = [this] {
        double now = juce::Time::getMillisecondCounterHiRes();
        // Reset if last tap was more than 2 seconds ago
        if (tapTimes.size() > 0 && (now - tapTimes.getLast()) > 2000.0)
            tapTimes.clear();
        tapTimes.add(now);
        if (tapTimes.size() > maxTaps)
            tapTimes.remove(0);
        if (tapTimes.size() >= 2)
        {
            double totalInterval = tapTimes.getLast() - tapTimes.getFirst();
            double avgInterval = totalInterval / (tapTimes.size() - 1);
            double bpm = juce::jlimit(20.0, 300.0, 60000.0 / avgInterval);
            pluginHost.getEngine().setBpm(bpm);
            bpmLabel.setText(juce::String(static_cast<int>(bpm)) + " BPM", juce::dontSendNotification);
        }
    };

    // ── Edit Toolbar ──
    addAndMakeVisible(newClipButton);
    newClipButton.onClick = [this] {
        takeSnapshot();
        if (timelineComponent) timelineComponent->createClipAtPlayhead();
    };

    addAndMakeVisible(deleteClipButton);
    deleteClipButton.onClick = [this] {
        takeSnapshot();
        if (timelineComponent) timelineComponent->deleteSelected();
    };

    addAndMakeVisible(duplicateClipButton);
    duplicateClipButton.onClick = [this] {
        takeSnapshot();
        if (timelineComponent) timelineComponent->duplicateSelected();
    };

    addAndMakeVisible(splitClipButton);
    splitClipButton.onClick = [this] {
        takeSnapshot();
        if (timelineComponent) timelineComponent->splitSelected();
    };

    addAndMakeVisible(quantizeButton);
    quantizeButton.onClick = [this] {
        takeSnapshot();
        if (timelineComponent) timelineComponent->quantizeSelectedClip();
    };

    // Clear-Automation lives inside the note editor (PianoRollWindow)
    // now — the toolbar button stays for backwards-compat layout but
    // is permanently hidden.  See editClipButton.onClick below for
    // the callback wiring.
    clearAutoButton.setVisible(false);

    addChildComponent(gridSelector);    // state holder; never visible
    gridSelector.addItem("1/4", 1);
    gridSelector.addItem("1/8", 2);
    gridSelector.addItem("1/16", 3);
    gridSelector.addItem("1/32", 4);
    gridSelector.setSelectedId(3, juce::dontSendNotification); // default 1/16
    gridSelector.onChange = [this] {
        if (timelineComponent)
        {
            double res = 1.0;
            switch (gridSelector.getSelectedId())
            {
                case 1: res = 1.0; break;    // 1/4
                case 2: res = 0.5; break;    // 1/8
                case 3: res = 0.25; break;   // 1/16
                case 4: res = 0.125; break;  // 1/32
            }
            timelineComponent->setGridResolution(res);
        }
        // Mirror the current value to the visible toolbar button.
        gridButton.setButtonText(gridSelector.getText());
    };

    addAndMakeVisible(gridButton);
    gridButton.setButtonText(gridSelector.getText());
    gridButton.onClick = [this] {
        const int n = gridSelector.getNumItems();
        if (n <= 0) return;
        const int next = (gridSelector.getSelectedItemIndex() + 1) % n;
        gridSelector.setSelectedItemIndex(next, juce::sendNotificationSync);
    };

    // OLED color cycler — only visible in the Launchkey OLED theme.
    addChildComponent(oledColorButton);
    oledColorButton.onClick = [this] {
        if (auto* oled = dynamic_cast<LaunchkeyOledLookAndFeel*>(themeManager.getLookAndFeel()))
        {
            oled->cycleOledColour();
            oledColorButton.setButtonText(oled->getOledColourName());
            applyThemeToControls();
            // Re-tint the on-screen toolbar pads so they match the
            // new hue alongside the hardware (which refreshes via
            // controllerToolbarPadColorRGB on the next tick).
            applyLaunchkeyToolbarColors();
            repaint();
        }
    };

    addAndMakeVisible(countInButton);
    countInButton.setClickingTogglesState(true);
    countInButton.onClick = [this] { pluginHost.getEngine().toggleCountIn(); };

    addAndMakeVisible(loopButton);
    loopButton.setClickingTogglesState(true);
    loopButton.onClick = [this] {
        if (midiLearnActive) { startMidiLearn(MidiTarget::Loop); return; }
        pluginHost.getEngine().toggleLoop();
    };

    addAndMakeVisible(panicButton);
    panicButton.onClick = [this] {
        // Flag all tracks for hard note-off on the audio thread (thread-safe)
        for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
        {
            auto& track = pluginHost.getTrack(t);
            if (track.clipPlayer)
            {
                track.clipPlayer->panicKill.store(true);
                track.clipPlayer->sendAllNotesOff.store(true);
            }
        }
        statusLabel.setText("MIDI Panic - all notes off", juce::dontSendNotification);
        panicAnimEndTime = juce::Time::getMillisecondCounterHiRes() * 0.001 + 3.0;
    };

    // ── Glass Animation Toggle ──
    addAndMakeVisible(glassAnimButton);
    glassAnimButton.setClickingTogglesState(true);
    glassAnimButton.setToggleState(true, juce::dontSendNotification);
    glassAnimButton.onClick = [this] {
        glassAnimEnabled = glassAnimButton.getToggleState();
        repaint();
    };

    // ── Capture Button ──
    addAndMakeVisible(captureButton);
    captureButton.onClick = [this] { performCapture(); };

    // ── Export Button ──
    addAndMakeVisible(exportButton);
    exportButton.onClick = [this] {
        auto& eng = pluginHost.getEngine();

        // Determine export range
        double startBeat = 0.0;
        double endBeat = 16.0;
        if (eng.isLoopEnabled() && eng.hasLoopRegion())
        {
            startBeat = eng.getLoopStart();
            endBeat = eng.getLoopEnd();
        }
        else
        {
            // Find the end of the last clip across all tracks
            double maxEnd = 16.0;
            for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
            {
                auto* cp = pluginHost.getTrack(t).clipPlayer;
                if (!cp) continue;
                for (int s = 0; s < cp->getNumSlots(); ++s)
                {
                    auto& slot = cp->getSlot(s);
                    if (slot.clip && slot.hasContent())
                    {
                        double clipEnd = slot.clip->timelinePosition + slot.clip->lengthInBeats;
                        if (clipEnd > maxEnd) maxEnd = clipEnd;
                    }
                    if (slot.audioClip)
                    {
                        double clipEnd = slot.audioClip->timelinePosition + slot.audioClip->lengthInBeats;
                        if (clipEnd > maxEnd) maxEnd = clipEnd;
                    }
                }
            }
            endBeat = maxEnd;
        }

        // Stop playback during export
        eng.stop();

        // Generate filename with timestamp
        auto now = juce::Time::getCurrentTime();
        auto filename = "LegionStage_" + now.formatted("%Y%m%d_%H%M%S") + ".wav";
        auto docsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
        auto outputFile = docsDir.getChildFile(filename);

        statusLabel.setText("Exporting audio...", juce::dontSendNotification);

        double sampleRate = 44100.0;
        if (auto* device = deviceManager.getCurrentAudioDevice())
            sampleRate = device->getCurrentSampleRate();

        // Disconnect audio device during export to avoid conflicts
        audioPlayer.setProcessor(nullptr);

        audioExporter = std::make_unique<AudioExporter>(pluginHost, eng);
        audioExporter->startExport(outputFile, startBeat, endBeat, sampleRate);

        // Poll for completion via timer (checked in timerCallback)
    };

    // ── Arpeggiator Controls ──
    addAndMakeVisible(arpButton);
    arpButton.setClickingTogglesState(true);
    arpButton.onClick = [this] {
        auto* cp = pluginHost.getTrack(selectedTrackIndex).clipPlayer;
        if (cp) cp->getArpeggiator().toggleEnabled();
    };

    addAndMakeVisible(arpModeButton);
    arpModeButton.onClick = [this] {
        auto* cp = pluginHost.getTrack(selectedTrackIndex).clipPlayer;
        if (cp)
        {
            cp->getArpeggiator().cycleMode();
            arpModeButton.setButtonText(cp->getArpeggiator().getModeName());
        }
    };

    addAndMakeVisible(arpRateButton);
    arpRateButton.onClick = [this] {
        auto* cp = pluginHost.getTrack(selectedTrackIndex).clipPlayer;
        if (cp)
        {
            cp->getArpeggiator().cycleRate();
            arpRateButton.setButtonText(cp->getArpeggiator().getRateName());
        }
    };

    addAndMakeVisible(arpOctButton);
    arpOctButton.onClick = [this] {
        auto* cp = pluginHost.getTrack(selectedTrackIndex).clipPlayer;
        if (cp)
        {
            auto& arp = cp->getArpeggiator();
            int oct = arp.getOctaveRange() % 4 + 1;
            arp.setOctaveRange(oct);
            arpOctButton.setButtonText("Oct " + juce::String(oct));
        }
    };

    addAndMakeVisible(zoomInButton);
    zoomInButton.onClick = [this] { if (timelineComponent) timelineComponent->zoomIn(); };

    addAndMakeVisible(zoomOutButton);
    zoomOutButton.onClick = [this] { if (timelineComponent) timelineComponent->zoomOut(); };

    addAndMakeVisible(scrollLeftButton);
    scrollLeftButton.onClick = [this] { if (timelineComponent) timelineComponent->scrollLeft(); };

    addAndMakeVisible(scrollRightButton);
    scrollRightButton.onClick = [this] { if (timelineComponent) timelineComponent->scrollRight(); };

    addAndMakeVisible(editClipButton);
    editClipButton.onClick = [this] {
        if (timelineComponent)
        {
            auto* clip = timelineComponent->getSelectedClip();
            if (clip != nullptr)
            {
                // Wire the in-window CLR AUTO button to clear lanes
                // for the focused track.  Snapshot first so undo works.
                juce::Component::SafePointer<MainComponent> safe(this);
                auto onClearAuto = [safe]
                {
                    auto* self = safe.getComponent();
                    if (!self) return;
                    self->takeSnapshot();
                    auto& trk = self->pluginHost.getTrack(self->selectedTrackIndex);
                    {
                        const juce::SpinLock::ScopedLockType lock(trk.automationLock);
                        trk.automationLanes.clear();
                    }
                    self->statusLabel.setText("Automation cleared", juce::dontSendNotification);
                    if (self->timelineComponent) self->timelineComponent->repaint();
                };
                new PianoRollWindow("Piano Roll", *clip, pluginHost.getEngine(),
                                    nullptr, std::move(onClearAuto));
            }
        }
    };

    // ── Right Panel ──
    addAndMakeVisible(pluginSelector);
    pluginSelector.onChange = [this] { loadSelectedPlugin(); };

    addAndMakeVisible(openEditorButton);
    openEditorButton.onClick = [this] { openPluginEditor(); };
    openEditorButton.setEnabled(false);

    // Preset browser
    addAndMakeVisible(presetSelector);
    presetSelector.addItem("-- Preset --", 1);
    presetSelector.setSelectedId(1, juce::dontSendNotification);
    presetSelector.onChange = [this] {
        int id = presetSelector.getSelectedId();
        if (id > 1) loadPreset(id - 2);
    };
    addAndMakeVisible(presetPrevButton);
    presetPrevButton.onClick = [this] {
        auto& track = pluginHost.getTrack(selectedTrackIndex);
        if (track.plugin == nullptr) return;
        int cur = presetSelector.getSelectedId() - 2;  // use selector as source of truth
        if (cur > 0) loadPreset(cur - 1);
    };
    addAndMakeVisible(presetNextButton);
    presetNextButton.onClick = [this] {
        auto& track = pluginHost.getTrack(selectedTrackIndex);
        if (track.plugin == nullptr) return;
        int cur = presetSelector.getSelectedId() - 2;  // use selector as source of truth
        if (cur < track.plugin->getNumPrograms() - 1) loadPreset(cur + 1);
    };

    addAndMakeVisible(presetUpButton);
    presetUpButton.setComponentID("pill");
    presetUpButton.onClick = [this] {
        auto& track = pluginHost.getTrack(selectedTrackIndex);
        if (track.plugin == nullptr) return;
        int cur = presetSelector.getSelectedId() - 2;  // use selector as source of truth
        if (cur < track.plugin->getNumPrograms() - 1)
        {
            loadPreset(cur + 1);
            juce::String name = track.plugin->getProgramName(cur + 1);
            int num = cur + 2;
            int total = track.plugin->getNumPrograms();
            statusLabel.setText(juce::String(num) + "/" + juce::String(total) + " " + name, juce::dontSendNotification);
            chordLabel.setText("Preset", juce::dontSendNotification);
            juce::Component::SafePointer<MainComponent> safeThis(this);
            juce::Timer::callAfterDelay(2000, [safeThis] {
                if (auto* self = safeThis.getComponent())
                {
                    self->statusLabel.setText("", juce::dontSendNotification);
                    self->chordLabel.setText("---", juce::dontSendNotification);
                }
            });
        }
    };
    addAndMakeVisible(presetDownButton);
    presetDownButton.setComponentID("pill");
    presetDownButton.onClick = [this] {
        auto& track = pluginHost.getTrack(selectedTrackIndex);
        if (track.plugin == nullptr) return;
        int cur = presetSelector.getSelectedId() - 2;  // use selector as source of truth
        if (cur > 0)
        {
            loadPreset(cur - 1);
            juce::String name = track.plugin->getProgramName(cur - 1);
            int num = cur;
            int total = track.plugin->getNumPrograms();
            statusLabel.setText(juce::String(num) + "/" + juce::String(total) + " " + name, juce::dontSendNotification);
            chordLabel.setText("Preset", juce::dontSendNotification);
            juce::Component::SafePointer<MainComponent> safeThis(this);
            juce::Timer::callAfterDelay(2000, [safeThis] {
                if (auto* self = safeThis.getComponent())
                {
                    self->statusLabel.setText("", juce::dontSendNotification);
                    self->chordLabel.setText("---", juce::dontSendNotification);
                }
            });
        }
    };

    addAndMakeVisible(midiInputSelector);
    midiInputSelector.onChange = [this] { selectMidiDevice(); };

    addAndMakeVisible(midiRefreshButton);
    midiRefreshButton.onClick = [this] { scanMidiDevices(); };

    // Param page navigation
    addAndMakeVisible(paramPageLeft);
    addAndMakeVisible(paramPageRight);
    addAndMakeVisible(paramPageLabel);
    paramPageLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(paramPageNameLabel);
    paramPageNameLabel.setJustificationType(juce::Justification::centredLeft);
    paramPageNameLabel.setFont(juce::Font(13.0f));
    paramPageLeft.onClick = [this] {
        if (!paramSmartPage && paramPageOffset == 0)
        {
            paramSmartPage = true;
        }
        else
        {
            paramPageOffset = juce::jmax(0, paramPageOffset - activeParamCount());
        }
        updateParamSliders();
    };
    paramPageRight.onClick = [this] {
        auto& track = pluginHost.getTrack(selectedTrackIndex);
        if (track.plugin != nullptr)
        {
            int total = track.plugin->getParameters().size();
            if (paramSmartPage)
            {
                paramSmartPage = false;
                paramPageOffset = 0;
            }
            else if (paramPageOffset + activeParamCount() < total)
                paramPageOffset += activeParamCount();
            updateParamSliders();
        }
    };

    // FX insert slots
    for (int i = 0; i < NUM_FX_SLOTS; ++i)
    {
        auto* selector = new juce::ComboBox();
        selector->addItem("FX " + juce::String(i + 1) + ": Empty", 1);
        selector->setSelectedId(1, juce::dontSendNotification);
        int slotIdx = i;
        selector->onChange = [this, slotIdx] { loadFxPlugin(slotIdx); };
        addAndMakeVisible(selector);
        fxSelectors.add(selector);

        auto* edBtn = new juce::TextButton("E");
        edBtn->onClick = [this, slotIdx] { openFxEditor(slotIdx); };
        addAndMakeVisible(edBtn);
        fxEditorButtons.add(edBtn);
    }

    addAndMakeVisible(audioSettingsButton);
    audioSettingsButton.onClick = [this] {
        juce::PopupMenu menu;
#if !JUCE_IOS
        menu.addItem(1, "Audio Settings...");
        menu.addItem(2, "Check for Updates...");
#else
        menu.addItem(1, "Audio Info");
#endif
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&audioSettingsButton),
            [this](int result) {
                if (result == 1) showAudioSettings();
#if !JUCE_IOS
                else if (result == 2) showSettingsMenu();
#endif
            });
    };

    addAndMakeVisible(fullscreenButton);
    fullscreenButton.setClickingTogglesState(true);
    fullscreenButton.onClick = [this] {
        visualizerFullScreen = fullscreenButton.getToggleState();
        projectorMode = visualizerFullScreen;
        if (visualizerFullScreen)
            startTimerHz(5);  // Minimal rate — visualizer has its own 60Hz timer
        else
            startTimerHz(themeManager.isGlassOverlay() ? getGlassTimerHz() : getBaseTimerHz());
        resized();
        repaint();
    };

    addAndMakeVisible(visSelector);
    visSelector.addItem("Spectrum", 1);
    visSelector.addItem("Lissajous", 2);
    visSelector.addItem("Terrain", 3);
    visSelector.addItem("Geiss", 4);
    visSelector.addItem("MilkDrop", 5);
    visSelector.addItem("Analyzer", 6);
    visSelector.addItem("Heartbeat", 7);
    visSelector.addItem("BioSync", 8);
    visSelector.addItem("Fluid", 9);
    visSelector.addItem("RayMarch", 10);
    visSelector.setSelectedId(2, juce::dontSendNotification);  // Lissajous
    currentVisMode = 1;
    updateVisualizerTimers();  // Stop all visualizer timers except the active one
    visSelector.onChange = [this] {
        currentVisMode = visSelector.getSelectedId() - 1;
        updateVisualizerTimers();
        resized();
        repaint();
    };

    addAndMakeVisible(visExitButton);
    visExitButton.setVisible(false);
    visExitButton.onClick = [this] {
        visualizerFullScreen = false;
        projectorMode = false;
        fullscreenButton.setToggleState(false, juce::dontSendNotification);
        projectorButton.setToggleState(false, juce::dontSendNotification);
        // Restore normal timer rate now that main UI needs repainting again
        startTimerHz(themeManager.isGlassOverlay() ? getGlassTimerHz() : getBaseTimerHz());
        // Re-assert MIDI routing after exiting fullscreen
        pluginHost.setSelectedTrack(selectedTrackIndex);
        resized();
        repaint();
        grabKeyboardFocus();
    };

    addChildComponent(projectorButton);  // hidden — merged with fullscreen
    projectorButton.setClickingTogglesState(true);
    projectorButton.onClick = [this] {
        projectorMode = projectorButton.getToggleState();
        if (projectorMode)
        {
            visualizerFullScreen = true;
            startTimerHz(5);  // Minimal rate — visualizer has its own 60Hz timer
        }
        else
        {
            visualizerFullScreen = false;
            startTimerHz(themeManager.isGlassOverlay() ? getGlassTimerHz() : getBaseTimerHz());
        }
        resized();
        repaint();
    };

    // ── Geiss control buttons ──
    addAndMakeVisible(geissWaveBtn);
    geissWaveBtn.onClick = [this] {
        if (midiLearnActive) { startMidiLearn(MidiTarget::GeissWaveform); return; }
        geissDisplay.cycleWaveform();
    };

    addAndMakeVisible(geissPaletteBtn);
    geissPaletteBtn.onClick = [this] {
        if (midiLearnActive) { startMidiLearn(MidiTarget::GeissPalette); return; }
        geissDisplay.cyclePalette();
    };

    addAndMakeVisible(geissSceneBtn);
    geissSceneBtn.onClick = [this] {
        if (midiLearnActive) { startMidiLearn(MidiTarget::GeissScene); return; }
        geissDisplay.newRandomScene();
    };

    addAndMakeVisible(geissWaveUpBtn);
    geissWaveUpBtn.onClick = [this] {
        if (midiLearnActive) { startMidiLearn(MidiTarget::GeissWaveScale); return; }
        geissDisplay.waveScaleUp();
    };

    addAndMakeVisible(geissWaveDownBtn);
    geissWaveDownBtn.onClick = [this] {
        if (midiLearnActive) { startMidiLearn(MidiTarget::GeissWaveScale); return; }
        geissDisplay.waveScaleDown();
    };

    addAndMakeVisible(geissWarpLockBtn);
    geissWarpLockBtn.setClickingTogglesState(true);
    geissWarpLockBtn.onClick = [this] {
        if (midiLearnActive) { startMidiLearn(MidiTarget::GeissWarpLock); return; }
        geissDisplay.toggleWarpLock();
    };

    addAndMakeVisible(geissPalLockBtn);
    geissPalLockBtn.setClickingTogglesState(true);
    geissPalLockBtn.onClick = [this] {
        if (midiLearnActive) { startMidiLearn(MidiTarget::GeissPaletteLock); return; }
        geissDisplay.togglePaletteLock();
    };

    addAndMakeVisible(geissSpeedSelector);
    geissSpeedSelector.addItem("0.25x", 1);
    geissSpeedSelector.addItem("0.5x", 2);
    geissSpeedSelector.addItem("1x", 3);
    geissSpeedSelector.addItem("2x", 4);
    geissSpeedSelector.addItem("4x", 5);
    geissSpeedSelector.setSelectedId(3, juce::dontSendNotification);
    geissSpeedSelector.onChange = [this] {
        float speeds[] = { 0.25f, 0.5f, 1.0f, 2.0f, 4.0f };
        int idx = geissSpeedSelector.getSelectedId() - 1;
        if (idx >= 0 && idx < 5) geissDisplay.setSpeed(speeds[idx]);
    };

    addAndMakeVisible(geissAutoPilotBtn);
    geissAutoPilotBtn.setClickingTogglesState(true);
    geissAutoPilotBtn.onClick = [this] {
        geissDisplay.toggleAutoPilot();
    };

    addAndMakeVisible(geissBgBtn);
    geissBgBtn.setClickingTogglesState(true);
    geissBgBtn.onClick = [this] { geissDisplay.setBlackBg(geissBgBtn.getToggleState()); };

    // ── ProjectM (MilkDrop) control buttons ──
    addAndMakeVisible(pmNextBtn);
    pmNextBtn.onClick = [this] { projectMDisplay.nextScene(); };

    addAndMakeVisible(pmPrevBtn);
    pmPrevBtn.onClick = [this] { projectMDisplay.prevScene(); };

    addAndMakeVisible(pmRandBtn);
    pmRandBtn.onClick = [this] { projectMDisplay.randomScene(); };

    addAndMakeVisible(pmLockBtn);
    pmLockBtn.setClickingTogglesState(true);
    pmLockBtn.onClick = [this] { projectMDisplay.toggleLock(); };

    addAndMakeVisible(pmBgBtn);
    pmBgBtn.setClickingTogglesState(true);
    pmBgBtn.onClick = [this] { projectMDisplay.setBlackBg(pmBgBtn.getToggleState()); };

    // ── Terrain/Shader control buttons (reuse G-Force button slots) ──
    addAndMakeVisible(gfRibbonUpBtn);
    gfRibbonUpBtn.setButtonText("Next");
    gfRibbonUpBtn.onClick = [this] { shaderToyDisplay.nextPreset(); };
    addAndMakeVisible(gfRibbonDownBtn);
    gfRibbonDownBtn.setButtonText("Prev");
    gfRibbonDownBtn.onClick = [this] { shaderToyDisplay.prevPreset(); };
    addAndMakeVisible(gfTrailBtn);
    gfTrailBtn.setVisible(false);
    addAndMakeVisible(gfSpeedSelector);
    gfSpeedSelector.setVisible(false);

    // ── Spectrum control buttons ──
    addAndMakeVisible(specDecayBtn);
    specDecayBtn.onClick = [this] {
        if (midiLearnActive) { startMidiLearn(MidiTarget::SpecDecay); return; }
        spectrumDisplay.cycleDecay();
    };
    addAndMakeVisible(specSensUpBtn);
    specSensUpBtn.onClick = [this] {
        if (midiLearnActive) { startMidiLearn(MidiTarget::SpecSensitivity); return; }
        spectrumDisplay.sensitivityUp();
    };
    addAndMakeVisible(specSensDownBtn);
    specSensDownBtn.onClick = [this] {
        if (midiLearnActive) { startMidiLearn(MidiTarget::SpecSensitivity); return; }
        spectrumDisplay.sensitivityDown();
    };

    // ── Lissajous control buttons ──
    addAndMakeVisible(lissZoomInBtn);
    lissZoomInBtn.onClick = [this] {
        if (midiLearnActive) { startMidiLearn(MidiTarget::LissZoom); return; }
        lissajousDisplay.zoomIn();
    };
    addAndMakeVisible(lissZoomOutBtn);
    lissZoomOutBtn.onClick = [this] {
        if (midiLearnActive) { startMidiLearn(MidiTarget::LissZoom); return; }
        lissajousDisplay.zoomOut();
    };
    addAndMakeVisible(lissDotsBtn);
    lissDotsBtn.onClick = [this] {
        if (midiLearnActive) { startMidiLearn(MidiTarget::LissDots); return; }
        lissajousDisplay.cycleDots();
    };

    // ── FluidSim control buttons ──
    addAndMakeVisible(fluidColorBtn);
    fluidColorBtn.onClick = [this] { fluidSimDisplay.cycleColorMode(); };

    addAndMakeVisible(fluidViscUpBtn);
    fluidViscUpBtn.onClick = [this] {
        fluidSimDisplay.setViscosity(fluidSimDisplay.getViscosity() * 1.5f);
    };

    addAndMakeVisible(fluidViscDownBtn);
    fluidViscDownBtn.onClick = [this] {
        fluidSimDisplay.setViscosity(fluidSimDisplay.getViscosity() / 1.5f);
    };

    addAndMakeVisible(fluidVortBtn);
    fluidVortBtn.setClickingTogglesState(true);
    fluidVortBtn.onClick = [this] { fluidSimDisplay.toggleVorticity(); };

    // ── RayMarch control buttons ──
    addAndMakeVisible(rmPrevBtn);
    rmPrevBtn.onClick = [this] { rayMarchDisplay.prevPreset(); };

    addAndMakeVisible(rmNextBtn);
    rmNextBtn.onClick = [this] { rayMarchDisplay.nextPreset(); };

    setVisControlsVisible();

    addAndMakeVisible(midi2Button);
    midi2Button.setClickingTogglesState(true);
    midi2Button.onClick = [this] {
        midi2Enabled = midi2Button.getToggleState();
        if (midi2Enabled)
        {
            auto& track = pluginHost.getTrack(selectedTrackIndex);
            midi2Handler.setPlugin(track.plugin);

            // Find matching MIDI output for the selected input
            auto midiOutputs = juce::MidiOutput::getAvailableDevices();
            juce::String outputId;

            // Try to find output with matching name
            for (auto& out : midiOutputs)
            {
                for (auto& in : midiDevices)
                {
                    if (in.identifier == currentMidiDeviceId && out.name == in.name)
                    {
                        outputId = out.identifier;
                        break;
                    }
                }
                if (outputId.isNotEmpty()) break;
            }

            // Fallback: try partial name match
            if (outputId.isEmpty())
            {
                for (auto& in : midiDevices)
                {
                    if (in.identifier == currentMidiDeviceId)
                    {
                        for (auto& out : midiOutputs)
                        {
                            if (out.name.containsIgnoreCase("keystage") ||
                                in.name.containsIgnoreCase(out.name.substring(0, 8)))
                            {
                                outputId = out.identifier;
                                break;
                            }
                        }
                        break;
                    }
                }
            }

            // Send Discovery broadcast
            midi2Handler.sendDiscovery();

            // Open MIDI output and keep it open
            auto useId = outputId.isNotEmpty() ? outputId : currentMidiDeviceId;
            midiOutput = juce::MidiOutput::openDevice(useId);
            midiOutputId = useId;

            auto& outgoing = midi2Handler.getOutgoing();
            if (!outgoing.isEmpty() && midiOutput)
            {
                for (const auto metadata : outgoing)
                    midiOutput->sendMessageNow(metadata.getMessage());

                statusLabel.setText("MIDI 2.0: Discovery sent via " + midiOutput->getName(),
                    juce::dontSendNotification);
            }
            else if (!midiOutput)
            {
                statusLabel.setText("MIDI 2.0: No MIDI output found!", juce::dontSendNotification);
            }
            midi2Handler.clearOutgoing();
        }
        else
        {
            midiOutput = nullptr;
            statusLabel.setText("MIDI 2.0 disabled", juce::dontSendNotification);
        }
    };

    testNoteButton.setVisible(false);

    // ── CPU Label ──
    addAndMakeVisible(cpuLabel);
    cpuLabel.setText("CPU: 0%", juce::dontSendNotification);
    cpuLabel.setJustificationType(juce::Justification::centredLeft);
    cpuLabel.addMouseListener(this, false);

    // ── Chord Detector Label ──
    addAndMakeVisible(chordLabel);
    chordLabel.setJustificationType(juce::Justification::centred);

    // ── iPhone Menu Button ──
    addAndMakeVisible(phoneMenuButton);
    phoneMenuButton.setVisible(false);
    phoneMenuButton.onClick = [this] { showPhoneMenu(); };

    // ── Touch Piano ──
#if JUCE_IOS
    addChildComponent(touchPiano);  // hidden by default on iOS too
    touchPianoVisible = false;
#else
    addChildComponent(touchPiano);  // hidden by default on desktop
#endif
    touchPiano.onNote = [this](int note, bool isOn) {
        if (isOn) sendNoteOn(note);
        else      sendNoteOff(note);
    };

    // ── Mixer ──
    mixerComponent = std::make_unique<MixerComponent>(pluginHost);
    mixerComponent->onTrackSelected = [this](int track) { selectTrack(track); };
    addChildComponent(*mixerComponent);  // hidden by default

    addAndMakeVisible(trackInputSelector);
    trackInputSelector.onChange = [this] { applyTrackInput(trackInputSelector.getSelectedId()); };

    addAndMakeVisible(mixerButton);
    mixerButton.setClickingTogglesState(true);
    mixerButton.onClick = [this] {
        mixerVisible = mixerButton.getToggleState();
        mixerComponent->setVisible(mixerVisible);
        resized();
        repaint();
    };

    // ── Session View ──
    sessionViewComponent = std::make_unique<SessionViewComponent>(pluginHost);
    addAndMakeVisible(*sessionViewComponent);
    sessionViewComponent->setVisible(false);
    sessionViewComponent->stopTimer();  // don't run 30Hz timer when hidden
    sessionViewComponent->onTrackSelected = [this](int track) { selectTrack(track); };

    addAndMakeVisible(sessionViewButton);
    sessionViewButton.setClickingTogglesState(true);
    sessionViewButton.onClick = [this] {
        showingSessionView = sessionViewButton.getToggleState();
        if (showingSessionView)
        {
            sessionViewComponent->startTimerHz(30);
            sessionViewComponent->setVisible(true);
            if (timelineComponent) timelineComponent->setVisible(false);
        }
        else
        {
            sessionViewComponent->stopTimer();
            sessionViewComponent->setVisible(false);
            if (timelineComponent) timelineComponent->setVisible(true);
        }
        resized();
    };

    // ── Loop Set Mode ──
    addAndMakeVisible(loopSetButton);
    loopSetButton.setClickingTogglesState(true);
    loopSetButton.onClick = [this] {
        if (timelineComponent)
        {
            bool active = loopSetButton.getToggleState();
            timelineComponent->loopSetMode = active;
            timelineComponent->loopSetTapCount = 0;
            if (active)
                statusLabel.setText("Tap loop start point...", juce::dontSendNotification);
            else
                updateStatusLabel();
            timelineComponent->repaint();
        }
    };

    // ── Right Panel Toggle ──
    addAndMakeVisible(panelToggleButton);
    panelToggleButton.onClick = [this] { toggleRightPanel(); };

    addAndMakeVisible(pianoToggleButton);
    pianoToggleButton.setClickingTogglesState(true);
    pianoToggleButton.onClick = [this] {
        touchPianoVisible = pianoToggleButton.getToggleState();
        touchPiano.setVisible(touchPianoVisible);
        resized();
        repaint();
    };

    addAndMakeVisible(pianoOctUpButton);
    pianoOctUpButton.onClick = [this] { touchPiano.octaveUp(); };

    addAndMakeVisible(pianoOctDownButton);
    pianoOctDownButton.onClick = [this] { touchPiano.octaveDown(); };

    // ── Bottom Bar: Mix Controls ──
    addAndMakeVisible(volumeSlider);
    volumeSlider.setRange(0.0, 1.0, 0.01);
    volumeSlider.setValue(0.8, juce::dontSendNotification);
    volumeSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    volumeSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 18);
    volumeSlider.onValueChange = [this] {
        if (midiLearnActive) { startMidiLearn(MidiTarget::Volume); return; }
        auto& track = pluginHost.getTrack(selectedTrackIndex);
        if (track.gainProcessor) track.gainProcessor->volume.store(static_cast<float>(volumeSlider.getValue()));
    };

    addAndMakeVisible(volumeLabel);
    volumeLabel.setJustificationType(juce::Justification::centred);
    volumeLabel.setFont(juce::Font(12.0f));

    addAndMakeVisible(panSlider);
    panSlider.setRange(-1.0, 1.0, 0.01);
    panSlider.setValue(0.0, juce::dontSendNotification);
    panSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    panSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 18);
    panSlider.onValueChange = [this] {
        if (midiLearnActive) { startMidiLearn(MidiTarget::Pan); return; }
        auto& track = pluginHost.getTrack(selectedTrackIndex);
        if (track.gainProcessor) track.gainProcessor->pan.store(static_cast<float>(panSlider.getValue()));
    };

    addAndMakeVisible(panLabel);
    panLabel.setJustificationType(juce::Justification::centred);
    panLabel.setFont(juce::Font(12.0f));

    addAndMakeVisible(saveButton);
    saveButton.onClick = [this] {
        if (currentProjectFile == juce::File())
        {
            saveProject(); // No file yet — go straight to Save As
            return;
        }
        juce::PopupMenu menu;
        menu.addItem(1, "Save (" + currentProjectFile.getFileNameWithoutExtension() + ")");
        menu.addItem(2, "Save As...");
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(saveButton),
            [this](int result) {
                if (result == 1) saveProjectQuick();
                else if (result == 2) saveProject();
            });
    };

    addAndMakeVisible(loadButton);
    loadButton.onClick = [this] { loadProject(); };

    addAndMakeVisible(undoButton);
    undoButton.onClick = [this] {
        if (undoIndex > 0)
        {
            undoIndex--;
            restoreSnapshot(undoHistory[undoIndex]);
            int clips = undoHistory[undoIndex].clips.size();
            statusLabel.setText("Undo (" + juce::String(clips) + " clips)", juce::dontSendNotification);
            chordLabel.setText("Step " + juce::String(undoIndex + 1) + "/" + juce::String(undoHistory.size()), juce::dontSendNotification);
            juce::Component::SafePointer<MainComponent> safeThis(this);
            juce::Timer::callAfterDelay(2000, [safeThis] {
                if (auto* self = safeThis.getComponent())
                {
                    self->statusLabel.setText("", juce::dontSendNotification);
                    self->chordLabel.setText("---", juce::dontSendNotification);
                }
            });
        }
    };

    addAndMakeVisible(redoButton);
    redoButton.onClick = [this] {
        if (undoIndex < undoHistory.size() - 1)
        {
            undoIndex++;
            restoreSnapshot(undoHistory[undoIndex]);
            int clips = undoHistory[undoIndex].clips.size();
            statusLabel.setText("Redo (" + juce::String(clips) + " clips)", juce::dontSendNotification);
            chordLabel.setText("Step " + juce::String(undoIndex + 1) + "/" + juce::String(undoHistory.size()), juce::dontSendNotification);
            juce::Component::SafePointer<MainComponent> safeThis(this);
            juce::Timer::callAfterDelay(2000, [safeThis] {
                if (auto* self = safeThis.getComponent())
                {
                    self->statusLabel.setText("", juce::dontSendNotification);
                    self->chordLabel.setText("---", juce::dontSendNotification);
                }
            });
        }
    };

    // ── Theme Selector ──
    addAndMakeVisible(themeSelector);
    for (int i = 0; i < ThemeManager::NumThemes; ++i)
    {
        if (i == ThemeManager::IoniqForest) continue;  // hidden
        themeSelector.addItem(ThemeManager::getThemeName(static_cast<ThemeManager::Theme>(i)), i + 1);
    }
    themeSelector.setSelectedId(ThemeManager::Ioniq + 1, juce::dontSendNotification);
    themeSelector.onChange = [this] {
        auto idx = themeSelector.getSelectedId() - 1;
        if (idx >= 0 && idx < ThemeManager::NumThemes)
        {
            themeManager.setTheme(static_cast<ThemeManager::Theme>(idx), this);
            applyThemeToControls();
            applyLaunchkeyToolbarColors();
            // Active param-knob count is theme-dependent (Launchkey
            // themes show 6) — re-populate + re-layout so the
            // visible knobs match the new theme immediately.
            updateParamSliders();
            resized();
            // Glass Light defaults to animation off
            bool animDefault = (idx != ThemeManager::LiquidGlassLight);
            glassAnimEnabled = animDefault;
            glassAnimButton.setToggleState(animDefault, juce::dontSendNotification);
            // Start/stop accelerometer and boost timer for glass themes
            if (timelineComponent)
                timelineComponent->setOpaque(!themeManager.isGlassOverlay());
            if (themeManager.isGlassOverlay())
            {
                DeviceMotion::getInstance().start();
                startTimerHz(getGlassTimerHz());
            }
            else
            {
                DeviceMotion::getInstance().stop();
                startTimerHz(getBaseTimerHz());
            }
#if JUCE_IOS
            if (metalRenderer && metalRendererAttached)
                metalRenderer->setVisible(themeManager.isGlassOverlay() && glassAnimEnabled);
#endif
            panelBlurImage = juce::Image();
            panelBlurUpdateCounter = 8;
            resized();
        }
    };

    trackInfoLabel.setVisible(false);

    // Plugin parameter sliders
    for (int i = 0; i < NUM_PARAM_SLIDERS; ++i)
    {
        auto* slider = new juce::Slider();
        slider->setRange(0.0, 1.0, 0.001);
        slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider->setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        slider->setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider->setEnabled(false);

        int paramIdx = i;
        slider->onValueChange = [this, slider, paramIdx] {
            if (midiLearnActive) {
                startMidiLearn(static_cast<MidiTarget>(static_cast<int>(MidiTarget::Param0) + paramIdx));
                return;
            }
            auto& track = pluginHost.getTrack(selectedTrackIndex);
            if (track.plugin == nullptr) return;

            int realIdx = static_cast<int>(slider->getProperties().getWithDefault("paramIndex", -1));
            auto& params = track.plugin->getParameters();
            if (realIdx < 0 || realIdx >= params.size()) return;

            params[realIdx]->setValue(static_cast<float>(slider->getValue()));

            // Mark this param as "touched" so automation playback won't fight the user
            track.touchedParamIndex.store(realIdx);
            track.touchedParamTime.store(static_cast<int64_t>(juce::Time::getMillisecondCounter()));

            // Show full param name and highlight this knob
            paramPageNameLabel.setText(params[realIdx]->getName(50), juce::dontSendNotification);
            highlightParamKnob(paramIdx);

            // Record automation if transport is playing + recording
            auto& eng = pluginHost.getEngine();
            if (eng.isPlaying() && eng.isRecording() && !eng.isInCountIn())
            {
                const juce::SpinLock::ScopedLockType lock(track.automationLock);

                AutomationLane* lane = nullptr;
                for (auto* l : track.automationLanes)
                {
                    if (l->parameterIndex == realIdx) { lane = l; break; }
                }
                if (lane == nullptr)
                {
                    lane = new AutomationLane();
                    lane->parameterIndex = realIdx;
                    lane->parameterName = params[realIdx]->getName(20);
                    track.automationLanes.add(lane);
                }

                AutomationPoint pt;
                pt.beat = eng.getPositionInBeats();
                pt.value = static_cast<float>(slider->getValue());

                // Remove nearby existing points to prevent accumulation
                for (int j = lane->points.size() - 1; j >= 0; --j)
                {
                    if (std::abs(lane->points[j].beat - pt.beat) < 0.01)
                        lane->points.remove(j);
                }
                lane->points.add(pt);

                std::sort(lane->points.begin(), lane->points.end(),
                    [](const AutomationPoint& a, const AutomationPoint& b) { return a.beat < b.beat; });
            }
        };

        addAndMakeVisible(slider);
        paramSliders.add(slider);

        auto* label = new juce::Label();
        label->setJustificationType(juce::Justification::centred);
        label->setFont(juce::Font(9.0f));
        label->setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));
        addAndMakeVisible(label);
        paramLabels.add(label);
    }

    addAndMakeVisible(statusLabel);
    statusLabel.setJustificationType(juce::Justification::centred);
    statusLabel.setFont(juce::Font(12.0f));

    // ── Timeline (arrangement view — always visible) ──
    timelineComponent = std::make_unique<TimelineComponent>(pluginHost);
    timelineComponent->onBeforeEdit = [this] { takeSnapshot(); };
    timelineComponent->onLoopSetProgress = [this](int tapCount) {
        if (tapCount == 1)
            statusLabel.setText("Tap loop end point...", juce::dontSendNotification);
        else
        {
            loopSetButton.setToggleState(false, juce::dontSendNotification);
            updateStatusLabel();
        }
    };
    addAndMakeVisible(*timelineComponent);

    arrangerMinimap = std::make_unique<ArrangerMinimapComponent>(pluginHost);
    arrangerMinimap->onScroll = [this](double newScrollX) {
        if (timelineComponent)
            timelineComponent->setScrollX(newScrollX);
    };
    addChildComponent(*arrangerMinimap);  // hidden by default — minimap disabled

    setSize(1280, 800);
    setWantsKeyboardFocus(true);

    scanPlugins();
    scanMidiDevices();
    selectTrack(0);

#if JUCE_IOS
    {
        juce::Component::SafePointer<MainComponent> safeThis(this);
        juce::Timer::callAfterDelay(3000, [safeThis] {
            if (auto* self = safeThis.getComponent())
                self->scanPlugins();
        });
        // Force layout refresh after orientation settles
        juce::Timer::callAfterDelay(500, [safeThis] {
            if (auto* self = safeThis.getComponent())
            {
                self->resized();
                self->repaint();
            }
        });
    }
#endif
    updateStatusLabel();

    // Initial undo snapshot
    takeSnapshot();

    // Apply initial theme colors to all controls
    applyThemeToControls();

    // Force layout after all controls are set up (handles side panels etc.)
    auto safeThis = juce::Component::SafePointer<MainComponent>(this);
    juce::MessageManager::callAsync([safeThis] { if (safeThis) { safeThis->resized(); safeThis->repaint(); } });

#if JUCE_IOS
    metalRenderer = std::make_unique<MetalCausticRenderer>();
#endif

    startTimerHz(themeManager.isGlassOverlay() ? getGlassTimerHz() : getBaseTimerHz());

    // Bring the Launchkey MK4 online if it's already plugged in.
    // attach() is a no-op if no MK4 DAW endpoint is found, so it's
    // safe to call unconditionally from the ctor.
    launchkey.attach(this);

    // On-screen MIDI inspector for the Launchkey DAW port — shows
    // the last 8 raw MIDI messages so we can decode what each
    // button/encoder actually sends without guessing pad colours.
    addAndMakeVisible(launchkeyMidiInspector);
    launchkeyMidiInspector.setColour(juce::Label::textColourId, juce::Colours::limegreen);
    launchkeyMidiInspector.setColour(juce::Label::backgroundColourId, juce::Colours::black.withAlpha(0.85f));
    launchkeyMidiInspector.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 22.f, juce::Font::bold));
    launchkeyMidiInspector.setJustificationType(juce::Justification::topLeft);
    launchkeyMidiInspector.setText("LK MIDI:\n(plug in device + press buttons)", juce::dontSendNotification);
    launchkeyMidiInspector.setBounds(8, 8, 460, 280);
    launchkeyMidiInspector.setVisible(false);   // off by default — opt-in via the LK pill
    launchkeyMidiInspector.toFront(false);

    // Tiny always-visible pill — tap to toggle the MIDI inspector overlay.
    addAndMakeVisible(launchkeyInspectorToggle);
    launchkeyInspectorToggle.setColour(juce::TextButton::buttonColourId, juce::Colours::black.withAlpha(0.55f));
    launchkeyInspectorToggle.setColour(juce::TextButton::buttonOnColourId, juce::Colours::limegreen.withAlpha(0.85f));
    launchkeyInspectorToggle.setColour(juce::TextButton::textColourOffId, juce::Colours::limegreen);
    launchkeyInspectorToggle.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
    launchkeyInspectorToggle.setClickingTogglesState(true);
    launchkeyInspectorToggle.setToggleState(false, juce::dontSendNotification);
    launchkeyInspectorToggle.onClick = [this] {
        const bool show = launchkeyInspectorToggle.getToggleState();
        launchkeyMidiInspector.setVisible(show);
        if (show) launchkeyMidiInspector.toFront(false);
    };
    launchkeyInspectorToggle.setAlwaysOnTop(true);
    launchkeyInspectorToggle.toFront(false);
    // Initial position; resized() repositions to bottom-left so it
    // always lives below the top bar's transport / BPM controls.
    launchkeyInspectorToggle.setBounds(8, 8, 32, 24);
    launchkeyMidiInspector.setAlwaysOnTop(true);
}

MainComponent::~MainComponent()
{
    pluginHost.spectrumDisplay = nullptr;
    pluginHost.waveTerrainDisplay = nullptr;
    pluginHost.geissDisplay = nullptr;
    pluginHost.projectMDisplay = nullptr;
    pluginHost.shaderToyDisplay = nullptr;
    pluginHost.analyzerDisplay = nullptr;
    pluginHost.heartbeatDisplay = nullptr;
    pluginHost.bioResonanceDisplay = nullptr;
    pluginHost.fluidSimDisplay = nullptr;
    pluginHost.rayMarchDisplay = nullptr;
    // Clear Lissajous pointer from all tracks
    for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
    {
        auto& track = pluginHost.getTrack(t);
        if (track.gainProcessor) track.gainProcessor->lissajousDisplay = nullptr;
    }
#if JUCE_IOS
    if (metalRenderer) metalRenderer->detach();
    metalRenderer.reset();
#endif
    setLookAndFeel(nullptr);  // clear before ThemeManager destructs
    stopTimer();
    disableCurrentMidiDevice();
    closePluginEditor();
    audioPlayer.setProcessor(nullptr);
    deviceManager.removeAudioCallback(&audioPlayer);
}

// ── Timer ────────────────────────────────────────────────────────────────────

void MainComponent::timerCallback()
{
    // Hot-attach: if the device wasn't plugged in at app launch but
    // is plugged in now, attach() will succeed; otherwise no-op.
    if (!launchkey.isActive()) launchkey.attach(this);
    launchkey.tick();

    // iPad-side toolbar boot wave (only fires when no Launchkey is
    // connected — otherwise the controller's tick mirrors the device
    // boot to the iPad already).
    updateIpadToolbarBootWave();

    // Auto-apply the Launchkey theme once per detection — fires on
    // first successful attach (boot or hot-plug).  Don't re-apply on
    // every tick, otherwise the user can't switch away with the
    // theme picker.
    if (launchkey.isActive() && !launchkeyThemeApplied)
    {
        launchkeyThemeApplied = true;
        themeManager.setTheme(ThemeManager::LaunchkeyDark, this);
        themeSelector.setSelectedId(ThemeManager::LaunchkeyDark + 1, juce::dontSendNotification);
        applyLaunchkeyToolbarColors();
        // Launchkey theme uses a different active param-knob count
        // (6 instead of NUM_PARAM_SLIDERS) — re-populate + re-layout.
        updateParamSliders();
        resized();
        // Push device pad/encoder modes to match the focused track.
        syncLaunchkeyDeviceModes();
    }
    if (launchkeyMidiInspector.isVisible())
    {
        auto txt = launchkey.getLastMessages();
        if (txt.isEmpty()) txt = "LK MIDI: (no messages yet)";
        else txt = "LK MIDI:\n" + txt;
        if (launchkeyMidiInspector.getText() != txt)
            launchkeyMidiInspector.setText(txt, juce::dontSendNotification);
    }

    // ── Skip main UI repainting when visualizer is fullscreen ──
    // The visualizer's own 60Hz timer handles all rendering; avoid competing repaints.
    if (visualizerFullScreen)
    {
#if JUCE_IOS
        // Hide the caustic Metal layer so it doesn't compete with the visualizer's Metal layer
        if (metalRendererAttached)
            metalRenderer->setVisible(false);
#endif
        // Still poll CPU/RAM so visualizers that use it (Heartbeat, BioSync) stay accurate
        float totalCpu = static_cast<float>(deviceManager.getCpuUsage() * 100.0);
        int ramMB = 0;
#if JUCE_IOS || JUCE_MAC
        struct mach_task_basic_info info;
        mach_msg_type_number_t size = MACH_TASK_BASIC_INFO_COUNT;
        if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &size) == KERN_SUCCESS)
            ramMB = static_cast<int>(info.resident_size / (1024 * 1024));
#endif
        currentCpuPercent = totalCpu;
        currentRamMB = ramMB;
        return;
    }

    // ── Right Panel slide animation (time-based, independent of timer rate) ──
    if (panelAnimating)
    {
        double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
        double elapsed = now - panelAnimStartTime;
        double duration = 0.35; // 350ms total animation
        float t = (float)juce::jlimit(0.0, 1.0, elapsed / duration);
        // Smooth ease-out curve
        float eased = 1.0f - (1.0f - t) * (1.0f - t);

        panelSlideProgress = panelAnimStartValue + (panelSlideTarget - panelAnimStartValue) * eased;

        if (t >= 1.0f)
        {
            panelSlideProgress = panelSlideTarget;
            panelAnimating = false;
            startTimerHz(themeManager.isGlassOverlay() ? getGlassTimerHz() : getBaseTimerHz());
        }
        resized();
        repaint();
    }

    // When running at 60Hz for animation, only run the rest at ~15Hz to keep
    // flash counters, CPU polling, etc. at their normal rate
    if (panelAnimating)
    {
        int panelSkip = isHighTier() ? 8 : (isLowTier() ? 3 : 4);  // keep EKG/CPU at ~15Hz
        if (++panelAnimFrameSkip < panelSkip) return;
        panelAnimFrameSkip = 0;
    }

    // Glass themes run timer faster — skip non-glass code to keep EKG/CPU at ~15Hz
    bool glassHighRate = themeManager.isGlassOverlay() && glassAnimEnabled && !panelAnimating;
    if (glassHighRate)
    {
        int skipCount = getGlassTimerHz() / 15;  // 60/15=4 on Pro, 30/15=2 on mini
        if (++panelAnimFrameSkip < skipCount)
        {
            // Still update glass animation and repaint, but skip CPU/flash
            glassAnimTime = juce::Time::getMillisecondCounterHiRes() * 0.001;
            auto rawTilt = DeviceMotion::getInstance().getTilt();
            float lerpRate = isHighTier() ? 0.04f : (isLowTier() ? 0.15f : 0.08f);
            smoothTiltX += (rawTilt.x - smoothTiltX) * lerpRate;
            smoothTiltY += (rawTilt.y - smoothTiltY) * lerpRate;

            // Age ripples at full rate for smooth expansion
            for (int ri = 0; ri < rippleCount; )
            {
                ripples[static_cast<size_t>(ri)].age += 1.0f / (float)getGlassTimerHz();
                if (ripples[static_cast<size_t>(ri)].age > 1.2f)
                {
                    for (int rj = ri; rj < rippleCount - 1; ++rj)
                        ripples[static_cast<size_t>(rj)] = ripples[static_cast<size_t>(rj + 1)];
                    --rippleCount;
                }
                else ++ri;
            }

            // Generate 1 EKG sample per frame for smooth sweep at any timer rate
            {
                float cpu = currentCpuPercent / 100.0f;
                double beatsPerSec = (63.0 + static_cast<double>(cpu) * 37.0) / 60.0;
                double phaseStep = beatsPerSec / static_cast<double>(getGlassTimerHz());

                ekgPhase += phaseStep;
                float p = static_cast<float>(std::fmod(ekgPhase, 1.0));
                float v = 0.0f;
                if (p < 0.12f) { float tt = (p - 0.06f) / 0.04f; v = 0.12f * std::exp(-tt * tt); }
                else if (p < 0.18f) { v = 0.0f; }
                else if (p < 0.22f) { float tt = (p - 0.20f) / 0.015f; v = -0.1f * std::exp(-tt * tt); }
                else if (p < 0.28f) { float w2 = 0.018f + cpu * 0.01f; float tt = (p - 0.25f) / w2; v = (0.7f + cpu * 0.3f) * std::exp(-tt * tt); }
                else if (p < 0.34f) { float tt = (p - 0.31f) / 0.02f; v = -(0.15f + cpu * 0.1f) * std::exp(-tt * tt); }
                else if (p < 0.48f) { v = cpu * 0.15f; }
                else if (p < 0.64f) { float tt = (p - 0.56f) / 0.05f; float tA = 0.2f + cpu * 0.1f; if (cpu > 0.85f) tA = -tA * 0.5f; v = tA * std::exp(-tt * tt); }

                ekgBuffer[static_cast<size_t>(ekgWritePos)] = v;
                ekgWritePos = (ekgWritePos + 1) % EKG_BUFFER_SIZE;
            }

#if JUCE_IOS
            if (metalRendererAttached)
                updateMetalCaustics();
            else
#endif
                repaint();
            return;
        }
        panelAnimFrameSkip = 0;
    }

    // ── Glass/Liquid animation tick ──
    if (themeManager.isGlassOverlay() && glassAnimEnabled)
    {
        glassAnimTime = juce::Time::getMillisecondCounterHiRes() * 0.001;

        // Smooth the accelerometer input — lerp toward raw tilt for fluid motion
        auto rawTilt = DeviceMotion::getInstance().getTilt();
        float lerpRate = isHighTier() ? 0.04f : (isLowTier() ? 0.15f : 0.08f);  // lower at 60Hz for same feel
        smoothTiltX += (rawTilt.x - smoothTiltX) * lerpRate;
        smoothTiltY += (rawTilt.y - smoothTiltY) * lerpRate;

        // Age ripples
        for (int i = 0; i < rippleCount; )
        {
            float timerRate = themeManager.isGlassOverlay() ? (float)getGlassTimerHz() : 15.0f;
            ripples[static_cast<size_t>(i)].age += 1.0f / timerRate;
            if (ripples[static_cast<size_t>(i)].age > 1.2f)
            {
                // Remove expired ripple
                for (int j = i; j < rippleCount - 1; ++j)
                    ripples[static_cast<size_t>(j)] = ripples[static_cast<size_t>(j + 1)];
                --rippleCount;
            }
            else
                ++i;
        }

#if JUCE_IOS
        // Use Metal GPU renderer if attached — skip CPU repaint for caustics
        if (metalRendererAttached)
            updateMetalCaustics();
        else
#endif
            repaint();
    }

    // Update panel frosted glass blur
    if (panelSlideProgress > 0.1f)
    {
        // During animation: update every frame for live glass effect
        // Otherwise: every ~500ms to save CPU
        int updateInterval = panelAnimating ? 1 : 8;
        if (++panelBlurUpdateCounter >= updateInterval)
        {
            panelBlurUpdateCounter = 0;
            if (themeManager.getCurrentTheme() == ThemeManager::LiquidGlass)
                updatePanelBlur();
        }
    }


    auto& eng = pluginHost.getEngine();

    // Refresh param knob positions during playback (shows automation in real-time)
    if (eng.isPlaying() && !eng.isInCountIn())
    {
        auto& track = pluginHost.getTrack(selectedTrackIndex);
        if (track.plugin != nullptr && !track.automationLanes.isEmpty())
        {
            auto& params = track.plugin->getParameters();
            for (int i = 0; i < NUM_PARAM_SLIDERS; ++i)
            {
                int realIdx = static_cast<int>(paramSliders[i]->getProperties().getWithDefault("paramIndex", -1));
                if (realIdx >= 0 && realIdx < params.size())
                    paramSliders[i]->setValue(params[realIdx]->getValue(), juce::dontSendNotification);
            }
        }
    }

    // Update BPM label to include beat position
    {
        int bpm = static_cast<int>(eng.getBpm());
        juce::String beatText;
        if (eng.isInCountIn())
        {
            int barsLeft = static_cast<int>(std::ceil(eng.getCountInBeatsRemaining() / 4.0));
            beatText = "Count: -" + juce::String(barsLeft);
        }
        else
        {
            double beatPos = eng.getPositionInBeats();
            int bar = static_cast<int>(beatPos / 4.0) + 1;
            int beatInBar = (static_cast<int>(beatPos) % 4) + 1;
            beatText = juce::String(bar) + "." + juce::String(beatInBar);
        }
        bpmLabel.setText(juce::String(bpm) + " BPM  " + beatText, juce::dontSendNotification);
    }

    // Update chord detector display
    auto chordName = chordDetector.getChordName();
    chordLabel.setText(chordName.isEmpty() ? "---" : chordName, juce::dontSendNotification);

    // Animate active param knob glow ring
    if (activeParamIndex >= 0 && activeParamIndex < NUM_PARAM_SLIDERS)
    {
        if (paramHighlightFadingIn)
        {
            paramHighlightAlpha += 0.06f;
            if (paramHighlightAlpha >= 1.0f) { paramHighlightAlpha = 1.0f; paramHighlightFadingIn = false; }
        }
        else
        {
            paramHighlightAlpha -= 0.03f;
            if (paramHighlightAlpha <= 0.3f) { paramHighlightAlpha = 0.3f; paramHighlightFadingIn = true; }
        }
        if (paramSliders[activeParamIndex]->isVisible())
            repaint(paramSliders[activeParamIndex]->getBounds().expanded(6));
    }

    // Animate play button green border — always visible, flashes when playing
    {
        auto& eng = pluginHost.getEngine();
        if (eng.isPlaying())
        {
            if (++playFlashCounter >= 8) // ~500ms at 15Hz
            {
                playHighlightOn = !playHighlightOn;
                playFlashCounter = 0;
            }
            playHighlightAlpha = playHighlightOn ? 1.0f : 0.15f;
            repaint(playButton.getBounds().expanded(6));
        }
        else if (playHighlightAlpha != 0.4f)
        {
            playHighlightAlpha = 0.4f;
            playHighlightOn = false;
            playFlashCounter = 0;
            repaint(playButton.getBounds().expanded(6));
        }

        // Animate record button red border — always visible, flashes when recording
        if (eng.isRecording())
        {
            if (++recFlashCounter >= 8) // ~500ms at 15Hz
            {
                recHighlightOn = !recHighlightOn;
                recFlashCounter = 0;
            }
            recHighlightAlpha = recHighlightOn ? 1.0f : 0.15f;
            repaint(recordButton.getBounds().expanded(6));
        }
        else if (recHighlightAlpha != 0.4f)
        {
            recHighlightAlpha = 0.4f;
            recHighlightOn = false;
            recFlashCounter = 0;
            repaint(recordButton.getBounds().expanded(6));
        }

        // Animate loop button blue border when loop is on
        if (loopButton.getToggleState())
        {
            if (++loopFlashCounter >= 8)
            {
                loopHighlightOn = !loopHighlightOn;
                loopFlashCounter = 0;
            }
            loopHighlightAlpha = loopHighlightOn ? 1.0f : 0.15f;
            repaint(loopButton.getBounds().expanded(6));
        }
        else if (loopHighlightAlpha != 0.4f)
        {
            loopHighlightAlpha = 0.4f;
            loopHighlightOn = false;
            loopFlashCounter = 0;
            repaint(loopButton.getBounds().expanded(6));
        }
    }

    // CPU + RAM syscalls — throttle to ~1Hz, those are the only
    // expensive bits here.  Everything else (EKG sample, repaint)
    // runs every frame so the sweep stays smooth.
    static int cpuPollCounter = 0;
    if (++cpuPollCounter >= 30) // ~once per second at 30Hz timer
    {
        cpuPollCounter = 0;
        float totalCpu = static_cast<float>(deviceManager.getCpuUsage() * 100.0);
        int ramMB = 0;
#if JUCE_IOS || JUCE_MAC
        struct mach_task_basic_info info;
        mach_msg_type_number_t size = MACH_TASK_BASIC_INFO_COUNT;
        if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &size) == KERN_SUCCESS)
            ramMB = static_cast<int>(info.resident_size / (1024 * 1024));
#endif
        currentCpuPercent = totalCpu;
        currentRamMB = ramMB;
        cpuLabel.setText("", juce::dontSendNotification);  // drawn custom

        cpuHistory.add(totalCpu);
        if (cpuHistory.size() > 60)
            cpuHistory.remove(0);
    }

    // EKG sweep — advance one sample per timer tick so the trace
    // moves smoothly.  Heart rate mirrors current CPU load.
    if (!glassHighRate)
    {
        float cpu = currentCpuPercent / 100.0f;
        double beatsPerSec = (63.0 + static_cast<double>(cpu) * 37.0) / 60.0;
        int currentHz = themeManager.isGlassOverlay() ? getGlassTimerHz() : getBaseTimerHz();
        double phaseStep = beatsPerSec / static_cast<double>(currentHz);

        ekgPhase += phaseStep;
        float p = static_cast<float>(std::fmod(ekgPhase, 1.0));

        float v = 0.0f;
        if (p < 0.12f) {
            float t = (p - 0.06f) / 0.04f;
            v = 0.12f * std::exp(-t * t);
        } else if (p < 0.18f) {
            v = 0.0f;
        } else if (p < 0.22f) {
            float t = (p - 0.20f) / 0.015f;
            v = -0.1f * std::exp(-t * t);
        } else if (p < 0.28f) {
            float width = 0.018f + cpu * 0.01f;
            float t = (p - 0.25f) / width;
            v = (0.7f + cpu * 0.3f) * std::exp(-t * t);
        } else if (p < 0.34f) {
            float t = (p - 0.31f) / 0.02f;
            v = -(0.15f + cpu * 0.1f) * std::exp(-t * t);
        } else if (p < 0.48f) {
            v = cpu * 0.15f;
        } else if (p < 0.64f) {
            float t = (p - 0.56f) / 0.05f;
            float tAmp = 0.2f + cpu * 0.1f;
            if (cpu > 0.85f) tAmp = -tAmp * 0.5f;
            v = tAmp * std::exp(-t * t) + cpu * 0.15f * (1.0f - (p - 0.48f) / 0.16f);
        }

        // Arrhythmia jitter at high CPU
        if (cpu > 0.75f)
            v += (cpu - 0.7f) * 0.1f * std::sin(static_cast<float>(ekgWritePos) * 3.1f);

        ekgBuffer[static_cast<size_t>(ekgWritePos)] = v;
        ekgWritePos = (ekgWritePos + 1) % EKG_BUFFER_SIZE;
    }

    if (cpuLabel.isVisible())
        repaint(cpuLabel.getBounds().expanded(5));

    // Heart rate observation is started automatically by the auth completion handler
    // No need to poll here

    // Update arranger minimap
    if (arrangerMinimap && arrangerMinimap->isVisible() && timelineComponent && timelineComponent->isVisible())
    {
        double totalBeats = 0.0;
        for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
        {
            auto& track = pluginHost.getTrack(t);
            if (track.clipPlayer)
                for (int s = 0; s < track.clipPlayer->getNumSlots(); ++s)
                {
                    auto& slot = track.clipPlayer->getSlot(s);
                    if (slot.clip)
                        totalBeats = juce::jmax(totalBeats, slot.clip->timelinePosition + slot.clip->lengthInBeats);
                    if (slot.audioClip)
                        totalBeats = juce::jmax(totalBeats, slot.audioClip->timelinePosition + slot.audioClip->lengthInBeats);
                }
        }
        arrangerMinimap->setViewRange(timelineComponent->getScrollX(),
                                       timelineComponent->getVisibleBeats(),
                                       totalBeats);
    }

    // Feed peak level to volume slider for VU ring
    {
        auto& track = pluginHost.getTrack(selectedTrackIndex);
        if (track.gainProcessor)
        {
            float peakL = track.gainProcessor->peakLevelL.load();
            float peakR = track.gainProcessor->peakLevelR.load();
            float peak = juce::jmax(peakL, peakR);
            volumeSlider.getProperties().set("vuLevel", peak);
            volumeSlider.repaint();
        }
    }

    // Flash record button hazard orange when armed
    if (recordButton.getToggleState())
    {
        bool flash = (juce::Time::currentTimeMillis() / 400) % 2 == 0;
        auto flashColor = flash ? juce::Colour(0xffdd6600) : juce::Colour(themeManager.getColors().redDark);
        recordButton.setColour(juce::TextButton::buttonColourId, flashColor);
        recordButton.setColour(juce::TextButton::buttonOnColourId, flashColor);
        recordButton.repaint();
    }
    else
    {
        recordButton.setColour(juce::TextButton::buttonColourId, juce::Colour(themeManager.getColors().redDark));
        recordButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(themeManager.getColors().red));
    }

    // Scrolling text on piano toggle button when active
    if (touchPianoVisible)
    {
        juce::String scrollText = "  ON SCREEN KEYBOARD  ";
        int offset = (static_cast<int>(juce::Time::currentTimeMillis() / 200)) % scrollText.length();
        juce::String visible = scrollText.substring(offset) + scrollText.substring(0, offset);
        pianoToggleButton.setButtonText(visible.substring(0, 6));
    }
    else
    {
        pianoToggleButton.setButtonText("KEYS");
    }

    // Sync transport button toggle states for animated OLED icons
    playButton.setToggleState(eng.isPlaying(), juce::dontSendNotification);
    metronomeButton.setToggleState(eng.isMetronomeOn(), juce::dontSendNotification);
    loopButton.setToggleState(eng.isLoopEnabled(), juce::dontSendNotification);
    countInButton.setToggleState(eng.isCountInEnabled(), juce::dontSendNotification);

    // Arpeggiator button state
    if (auto* cp = pluginHost.getTrack(selectedTrackIndex).clipPlayer)
    {
        auto& arp = cp->getArpeggiator();
        arpButton.setToggleState(arp.isEnabled(), juce::dontSendNotification);
        arpButton.setButtonText(arp.isEnabled() ? "ARP ON" : "ARP");
        arpModeButton.setButtonText(arp.getModeName());
        arpRateButton.setButtonText(arp.getRateName());
        arpOctButton.setButtonText("Oct " + juce::String(arp.getOctaveRange()));
    }

    // Check export progress
    if (audioExporter)
    {
        auto state = audioExporter->getState();
        if (state == AudioExporter::State::Rendering)
        {
            int pct = static_cast<int>(audioExporter->getProgress() * 100);
            statusLabel.setText("Exporting... " + juce::String(pct) + "%", juce::dontSendNotification);
        }
        else if (state == AudioExporter::State::Finished)
        {
            auto file = audioExporter->getOutputFile();
            statusLabel.setText("Exported: " + file.getFileName(), juce::dontSendNotification);

            // Reconnect audio device
            audioPlayer.setProcessor(&pluginHost);

            // Offer to share on iOS
            // File is in Documents folder — accessible via Files app on iOS
            audioExporter.reset();
        }
        else if (state == AudioExporter::State::Error)
        {
            statusLabel.setText("Export failed: " + audioExporter->getErrorMessage(), juce::dontSendNotification);
            audioPlayer.setProcessor(&pluginHost);
            audioExporter.reset();
        }
    }

    // Pulse animation for active toggle buttons
    {
        float pulse = 0.7f + 0.3f * std::sin(static_cast<float>(juce::Time::currentTimeMillis()) * 0.004f);
        auto& c = themeManager.getColors();

        auto pulseButton = [&](juce::TextButton& btn, juce::Colour onColour) {
            if (btn.getToggleState())
            {
                btn.setColour(juce::TextButton::buttonOnColourId, onColour.withMultipliedBrightness(pulse));
                btn.repaint();
            }
            else
            {
                btn.setColour(juce::TextButton::buttonOnColourId, onColour);
            }
        };

        pulseButton(loopButton, juce::Colour(c.btnLoopOn));
        pulseButton(countInButton, juce::Colour(c.btnCountInOn));
        pulseButton(midiLearnButton, juce::Colour(c.amber));
        pulseButton(pianoToggleButton, juce::Colour(c.lcdText));
        pulseButton(mixerButton, juce::Colour(c.lcdText));
        pulseButton(metronomeButton, juce::Colour(c.btnMetronomeOn));
    }

    // Repaint OLED buttons during playback (animations active)
    if (eng.isPlaying())
    {
        playButton.repaint();
        stopButton.repaint();
    }
    {
        double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
        panicButton.setToggleState(now < panicAnimEndTime, juce::dontSendNotification);
    }
    panicButton.repaint();

    // Auto-snapshot when recording stops (detect transition)
    // wasRecording is now a member variable (not static)
    bool isRec = false;
    for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
    {
        auto* cp = pluginHost.getTrack(t).clipPlayer;
        if (cp != nullptr)
            for (int s = 0; s < cp->getNumSlots(); ++s)
                if (cp->getSlot(s).state.load() == ClipSlot::Recording)
                    isRec = true;
    }
    if (wasRecording && !isRec)
        takeSnapshot();
    wasRecording = isRec;

    // Sync if timeline changed the selected track or arm state
    int currentSelected = pluginHost.getSelectedTrack();
    if (currentSelected != selectedTrackIndex)
    {
        selectedTrackIndex = currentSelected;
        closePluginEditor();
        updateTrackDisplay();
        updateStatusLabel();

        // Update plugin selector to show current track's plugin
        auto& track = pluginHost.getTrack(selectedTrackIndex);
        if (track.plugin != nullptr)
        {
            juce::String pluginName = track.plugin->getName();
            bool found = false;
            for (int i = 0; i < pluginDescriptions.size(); ++i)
            {
                if (pluginDescriptions[i].name == pluginName)
                {
                    pluginSelector.setSelectedId(i + 2, juce::dontSendNotification);
                    found = true;
                    break;
                }
            }
            if (!found)
                pluginSelector.setSelectedId(1, juce::dontSendNotification);
        }
        else
        {
            pluginSelector.setSelectedId(1, juce::dontSendNotification);
        }
    }

}

// ── Track Selection ──────────────────────────────────────────────────────────

void MainComponent::selectTrack(int index)
{
    selectedTrackIndex = juce::jlimit(0, PluginHost::NUM_TRACKS - 1, index);
    pluginHost.setSelectedTrack(selectedTrackIndex);
    closePluginEditor();
    updateTrackDisplay();
    updateStatusLabel();
    syncLaunchkeyDeviceModes();

    // Update arpeggiator button labels for the new track
    if (auto* cp = pluginHost.getTrack(selectedTrackIndex).clipPlayer)
    {
        auto& arp = cp->getArpeggiator();
        arpButton.setToggleState(arp.isEnabled(), juce::dontSendNotification);
        arpButton.setButtonText(arp.isEnabled() ? "ARP ON" : "ARP");
        arpModeButton.setButtonText(arp.getModeName());
        arpRateButton.setButtonText(arp.getRateName());
        arpOctButton.setButtonText("Oct " + juce::String(arp.getOctaveRange()));
    }

    // Sync session view track selection
    if (sessionViewComponent)
        sessionViewComponent->setSelectedTrack(selectedTrackIndex);

    // Update track input selector
    updateTrackInputSelector();

    // Show the currently loaded plugin in the selector, or reset to default
    auto& track = pluginHost.getTrack(selectedTrackIndex);
    if (track.plugin != nullptr)
    {
        juce::String pluginName = track.plugin->getName();
        bool found = false;
        for (int i = 0; i < pluginDescriptions.size(); ++i)
        {
            if (pluginDescriptions[i].name == pluginName)
            {
                pluginSelector.setSelectedId(i + 2, juce::dontSendNotification);
                found = true;
                break;
            }
        }
        if (!found)
            pluginSelector.setSelectedId(1, juce::dontSendNotification);
    }
    else
    {
        pluginSelector.setSelectedId(1, juce::dontSendNotification);
    }
}

void MainComponent::updateTrackDisplay()
{
    auto& track = pluginHost.getTrack(selectedTrackIndex);

    juce::String name = "Track " + juce::String(selectedTrackIndex + 1);
    if (track.plugin != nullptr)
        name += ": " + track.plugin->getName();
    trackNameLabel.setText(name, juce::dontSendNotification);

    openEditorButton.setEnabled(track.plugin != nullptr);
    updateFxDisplay();

    // Point Lissajous at the selected track's gain processor
    for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
    {
        auto& tr = pluginHost.getTrack(t);
        if (tr.gainProcessor)
            tr.gainProcessor->lissajousDisplay = (t == selectedTrackIndex) ? &lissajousDisplay : nullptr;
    }

    if (track.gainProcessor)
    {
        volumeSlider.setValue(track.gainProcessor->volume.load(), juce::dontSendNotification);
        panSlider.setValue(track.gainProcessor->pan.load(), juce::dontSendNotification);
    }

    // Track info
    juce::String info;
    if (track.plugin != nullptr)
        info += "Plugin: " + track.plugin->getName() + "\n";
    else
        info += "Plugin: (none)\n";

    if (track.clipPlayer != nullptr)
    {
        int clipCount = 0;
        int totalNotes = 0;
        for (int s = 0; s < track.clipPlayer->getNumSlots(); ++s)
        {
            auto& slot = track.clipPlayer->getSlot(s);
            if (slot.hasContent() && slot.clip != nullptr)
            {
                clipCount++;
                totalNotes += slot.clip->events.getNumEvents() / 2; // note on+off pairs
            }
        }
        info += "Clips: " + juce::String(clipCount) + "\n";
        info += "Notes: " + juce::String(totalNotes) + "\n";
    }

    info += "Armed: " + juce::String(track.clipPlayer && track.clipPlayer->armed.load() ? "Yes" : "No");
    trackInfoLabel.setText(info, juce::dontSendNotification);

    paramPageOffset = 0;
    paramSmartPage = true;
    updateParamSliders();
    updatePresetList();

    // Update MIDI 2.0 handler
    if (midi2Enabled)
        midi2Handler.setPlugin(track.plugin);

}

// ── Plugin ───────────────────────────────────────────────────────────────────

void MainComponent::scanPlugins()
{
    statusLabel.setText("Scanning plugins...", juce::dontSendNotification);
    repaint();

    pluginHost.scanForPlugins();

    pluginSelector.clear(juce::dontSendNotification);
    pluginDescriptions.clear();
    fxDescriptions.clear();
    pluginSelector.addItem("-- Plugin --", 1);

#if JUCE_IOS
    // Get native AU instrument identifiers for definitive categorization
    auto nativeAUs = AUScanner::scanAllAudioUnits();
    juce::StringArray instrumentIdentifiers;
    for (const auto& info : nativeAUs)
        if (info.isInstrument)
            instrumentIdentifiers.add(info.identifier);
#endif

    int id = 2; // 1 = "-- Plugin --"

    // Built-in Juno-60 synth
    {
        juce::PluginDescription junoDesc;
        junoDesc.name = "Juno-60";
        junoDesc.pluginFormatName = "Internal";
        junoDesc.category = "Instrument";
        junoDesc.manufacturerName = "Legion Stage";
        junoDesc.isInstrument = true;
        junoDesc.numInputChannels = 0;
        junoDesc.numOutputChannels = 2;
        junoDesc.fileOrIdentifier = "legion-stage-builtin-juno60";
        pluginDescriptions.add(junoDesc);
        pluginSelector.addItem("Juno-60", id++);
    }

    for (const auto& desc : pluginHost.getPluginList().getTypes())
    {
        bool instrument = desc.isInstrument;

        // Check category string
        if (!instrument)
        {
            auto cat = desc.category.toLowerCase();
            if (cat.contains("instrument") || cat.contains("synth")
                || cat.contains("generator") || cat.contains("music"))
                instrument = true;
        }

        // Check AU type code in identifier
        if (!instrument && desc.pluginFormatName == "AudioUnit")
        {
            if (desc.fileOrIdentifier.contains("aumu"))
                instrument = true;
        }

#if JUCE_IOS
        // Cross-reference with native AU scanner using identifier matching
        if (!instrument)
        {
            for (const auto& instId : instrumentIdentifiers)
            {
                // Check if the JUCE identifier contains the native AU type/subtype/manu
                if (desc.fileOrIdentifier.containsIgnoreCase(instId)
                    || instId.containsIgnoreCase(desc.fileOrIdentifier))
                {
                    instrument = true;
                    break;
                }
            }
        }
#endif

#if JUCE_IOS
        // On iOS, add ALL plugins to both instrument and FX lists
        // AUv3 synths often don't report isInstrument correctly
        pluginSelector.addItem(desc.name + (instrument ? "" : " [AU]"), id);
        pluginDescriptions.add(desc);
        id++;
        if (!instrument)
            fxDescriptions.add(desc);
#else
        if (instrument)
        {
            pluginSelector.addItem(desc.name, id);
            pluginDescriptions.add(desc);
            id++;
        }
        else
        {
            fxDescriptions.add(desc);
        }
#endif
    }
    pluginSelector.setSelectedId(1, juce::dontSendNotification);

#if JUCE_IOS
    auto nativeScanResults = AUScanner::scanAllAudioUnits();
    int nativeInst = 0, nativeFx = 0;
    for (const auto& a : nativeScanResults)
        if (a.isInstrument) nativeInst++; else nativeFx++;
    statusLabel.setText("Plugins: " + juce::String(pluginDescriptions.size()) + " inst, "
                        + juce::String(fxDescriptions.size()) + " fx (native: "
                        + juce::String(nativeInst) + " inst, " + juce::String(nativeFx) + " fx)",
#else
    statusLabel.setText("Found " + juce::String(pluginDescriptions.size()) + " instruments, "
                        + juce::String(fxDescriptions.size()) + " effects",
#endif
                        juce::dontSendNotification);
}

void MainComponent::loadSelectedPlugin()
{
    int idx = pluginSelector.getSelectedId() - 2;
    if (idx < 0 || idx >= pluginDescriptions.size()) return;

    closePluginEditor();
    audioPlayer.setProcessor(nullptr);

    statusLabel.setText("Loading plugin...", juce::dontSendNotification);

    juce::String err;
    bool ok = pluginHost.loadPlugin(selectedTrackIndex, pluginDescriptions[idx], err);

    audioPlayer.setProcessor(&pluginHost);

    if (ok)
    {
        updateTrackDisplay();
#if JUCE_IOS
        // Show plugin info in the beat OLED briefly
        {
            juce::String plugName = pluginDescriptions[idx].name;
            beatLabel.setText("Loaded:", juce::dontSendNotification);
            statusLabel.setText(plugName, juce::dontSendNotification);
            chordLabel.setText("OK", juce::dontSendNotification);
            // Restore normal display after 3 seconds
            juce::Component::SafePointer<MainComponent> safeThis(this);
            juce::Timer::callAfterDelay(3000, [safeThis] {
                if (auto* self = safeThis.getComponent())
                {
                    // Beat label will auto-update from timerCallback
                    self->statusLabel.setText("", juce::dontSendNotification);
                    self->chordLabel.setText("---", juce::dontSendNotification);
                }
            });
        }
        {
            juce::Component::SafePointer<MainComponent> safeThis(this);
            juce::Timer::callAfterDelay(500, [safeThis] {
                if (auto* self = safeThis.getComponent())
                {
                    self->paramPageOffset = 0;
                    self->paramSmartPage = true;
                    self->updateParamSliders();
                    self->updateFxDisplay();
                    self->updatePresetList();
                }
            });
        }
#endif
    }
    else
    {
        beatLabel.setText("FAILED:", juce::dontSendNotification);
        statusLabel.setText(err, juce::dontSendNotification);
        chordLabel.setText("ERR", juce::dontSendNotification);
        juce::Component::SafePointer<MainComponent> safeThis(this);
        juce::Timer::callAfterDelay(3000, [safeThis] {
            if (auto* self = safeThis.getComponent())
            {
                self->statusLabel.setText("", juce::dontSendNotification);
                self->chordLabel.setText("---", juce::dontSendNotification);
            }
        });
    }

    updateStatusLabel();
}

void MainComponent::openPluginEditor()
{
    auto& track = pluginHost.getTrack(selectedTrackIndex);
    if (track.plugin == nullptr) return;
    closePluginEditor();

    // Always create a fresh editor — createEditorIfNeeded can return stale cached editors
    currentEditor.reset(track.plugin->createEditor());
    if (currentEditor == nullptr) return;

#if JUCE_IOS
    // On iOS, embed the editor as an overlay with a close button
    {
        auto* overlay = new juce::Component();
        overlay->setName("EditorOverlay");

        auto* closeBtn = new juce::TextButton("CLOSE");
        closeBtn->setColour(juce::TextButton::buttonColourId, juce::Colours::red.withAlpha(0.9f));
        closeBtn->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        closeBtn->onClick = [this] { closePluginEditor(); };
        overlay->addAndMakeVisible(closeBtn);
        overlay->addAndMakeVisible(*currentEditor);

        // Use the editor's preferred size
        int edW = currentEditor->getWidth();
        int edH = currentEditor->getHeight();

        // Check if the plugin has size constraints
        if (auto* constrainer = currentEditor->getConstrainer())
        {
            if (constrainer->getMinimumWidth() > 0)
                edW = juce::jmax(edW, constrainer->getMinimumWidth());
            if (constrainer->getMinimumHeight() > 0)
                edH = juce::jmax(edH, constrainer->getMinimumHeight());
        }

        // Enforce minimum usable size — some AUv3 plugins report tiny initial sizes
        int minW = 400;
        int minH = 300;
        if (edW < minW) edW = minW;
        if (edH < minH) edH = minH;

        int closeBarH = 44;
        int maxW = getWidth() - 20;
        int maxH = getHeight() - closeBarH - 40;

        // Fit to available space
        int ew = juce::jlimit(minW, maxW, edW);
        int eh = juce::jlimit(minH, maxH, edH);

        int ox = (getWidth() - ew) / 2;
        int oy = (getHeight() - eh - closeBarH) / 2;

        overlay->setBounds(ox, oy, ew, eh + closeBarH);
        closeBtn->setBounds(0, 0, ew, closeBarH);
        currentEditor->setBounds(0, closeBarH, ew, eh);

        addAndMakeVisible(overlay);
        overlay->toFront(true);
    }
#else
    editorWindow = std::make_unique<PluginEditorWindow>(track.plugin->getName(), currentEditor.get(),
        [this] { closePluginEditor(); });
#endif
}

void MainComponent::closePluginEditor()
{
#if JUCE_IOS
    // Remove the overlay component that contains the editor and close button
    for (int i = getNumChildComponents() - 1; i >= 0; --i)
    {
        if (auto* child = getChildComponent(i))
        {
            if (child->getName() == "EditorOverlay")
            {
                // Delete overlay's children (e.g. closeBtn) that aren't owned by unique_ptr
                for (int j = child->getNumChildComponents() - 1; j >= 0; --j)
                {
                    auto* sub = child->getChildComponent(j);
                    if (sub != currentEditor.get())
                    {
                        child->removeChildComponent(j);
                        delete sub;
                    }
                }
                removeChildComponent(i);
                delete child;
                break;
            }
        }
    }
#else
    // Destroy window first (removes editor from component tree), then release editor
    editorWindow = nullptr;
#endif
    currentEditor.reset();
}

void MainComponent::playTestNote()
{
    pluginHost.sendTestNoteOn(60, 0.78f);
    juce::Component::SafePointer<MainComponent> safeThis(this);
    juce::Timer::callAfterDelay(500, [safeThis] {
        if (auto* self = safeThis.getComponent())
            self->pluginHost.sendTestNoteOff(60);
    });
}

// ── MIDI ─────────────────────────────────────────────────────────────────────

void MainComponent::scanMidiDevices()
{
    midiInputSelector.clear(juce::dontSendNotification);
    midiDevices = juce::MidiInput::getAvailableDevices();
    midiInputSelector.addItem("-- No MIDI --", 1);
#if !JUCE_IOS
    midiInputSelector.addItem("Computer Keyboard", 2);
#endif
    int id = 3;
    for (const auto& d : midiDevices) midiInputSelector.addItem(d.name, id++);
    midiInputSelector.setSelectedId(1, juce::dontSendNotification);
}

void MainComponent::selectMidiDevice()
{
    disableCurrentMidiDevice();
    useComputerKeyboard = false;

    int selectedId = midiInputSelector.getSelectedId();

    if (selectedId == 2)
    {
        useComputerKeyboard = true;
        setWantsKeyboardFocus(true);
        grabKeyboardFocus();
        updateStatusLabel();
        return;
    }

    int idx = selectedId - 3;
    if (idx < 0 || idx >= midiDevices.size()) { updateStatusLabel(); return; }
    auto d = midiDevices[idx];
    // Don't try to enable a port the Launchkey controller already
    // owns — the double-open crashes CoreMIDI on iPad.  The user
    // doesn't need to pick the DAW port manually anyway; the
    // controller opens it automatically.
    if (d.name.containsIgnoreCase("launch") && d.name.containsIgnoreCase("daw"))
    {
        updateStatusLabel();
        return;
    }
    deviceManager.setMidiInputDeviceEnabled(d.identifier, true);
    deviceManager.addMidiInputDeviceCallback(d.identifier, this);
    currentMidiDeviceId = d.identifier;
    updateStatusLabel();
}

void MainComponent::handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& msg)
{
    // Launchkey DAW-port traffic gets routed to the controller and
    // is consumed there — never forwarded to the plugin host or chord
    // detector (would double-trigger transport / play notes on synth).
    // Match loosely: any source name containing "Launchkey" + "DAW"
    // since iOS may rename the device between OS versions.
    if (source != nullptr
        && source->getName().containsIgnoreCase("launch")
        && source->getName().containsIgnoreCase("daw"))
    {
        DBG("Launchkey DAW port input from: " << source->getName());
        if (launchkey.processIncoming(msg)) return;
    }

    // Chord detection for incoming MIDI notes
    if (msg.isNoteOn())
        chordDetector.noteOn(msg.getNoteNumber());
    else if (msg.isNoteOff())
        chordDetector.noteOff(msg.getNoteNumber());

    // MIDI Learn — capture CC mapping
    if (midiLearnActive && midiLearnTarget != MidiTarget::None && msg.isController())
    {
        int ch = msg.getChannel();
        int cc = msg.getControllerNumber();
        int val = msg.getControllerValue();
        auto safeThis = juce::Component::SafePointer<MainComponent>(this);
        juce::MessageManager::callAsync([safeThis, ch, cc, val] {
            if (safeThis == nullptr) return;
            safeThis->processMidiLearnCC(ch, cc, val);
        });
        return;
    }

    // Apply learned MIDI mappings
    if (msg.isController())
    {
        int ch = msg.getChannel();
        int cc = msg.getControllerNumber();
        int val = msg.getControllerValue();

        bool mapped = false;
        for (auto& mapping : midiMappings)
        {
            if (mapping.channel == ch && mapping.ccNumber == cc)
            {
                auto safeThis = juce::Component::SafePointer<MainComponent>(this);
                juce::MessageManager::callAsync([safeThis, mapping, val] {
                    if (safeThis == nullptr) return;
                    safeThis->applyMidiCC(mapping, val);
                });
                mapped = true;
            }
        }
        if (mapped)
            return; // Don't forward learned CCs to the synth plugin
    }

    if (midi2Enabled)
    {
        // Route CI SysEx to the handler
        if (midi2Handler.processIncoming(msg))
        {
            // Count and send CI responses back to the Keystage
            auto& outgoing = midi2Handler.getOutgoing();
            int outCount = outgoing.getNumEvents();

            if (!outgoing.isEmpty() && midiOutput)
            {
                for (const auto metadata : outgoing)
                    midiOutput->sendMessageNow(metadata.getMessage());
                midi2Handler.clearOutgoing();
            }

            // Show what CI message was received and how many responses we sent
            juce::String ciInfo;
            {
                auto sdata = msg.getSysExData();
                int ssize = msg.getSysExDataSize();
                int subId = (ssize > 3) ? sdata[3] : 0;
                ciInfo = "CI:0x" + juce::String::toHexString(subId);

                if (subId == 0x34 && ssize > 16)
                {
                    int hdrLen = sdata[14] | (sdata[15] << 7);
                    juce::String hdr;
                    for (int i = 0; i < hdrLen && (16 + i) < ssize; ++i)
                        hdr += juce::String::charToString(static_cast<char>(sdata[16 + i]));
                    ciInfo += " " + hdr;
                }
            }

            auto safeThis = juce::Component::SafePointer<MainComponent>(this);
            juce::MessageManager::callAsync([safeThis, ciInfo, outCount] {
                if (safeThis == nullptr) return;
                safeThis->trackNameLabel.setText(ciInfo + " sent:" + juce::String(outCount),
                    juce::dontSendNotification);
            });

            return; // Don't forward CI SysEx to the audio engine
        }

        // Handle CCs from Keystage knobs (24-31)
        if (msg.isController())
        {
            int cc = msg.getControllerNumber();
            int val = msg.getControllerValue();

            if (cc >= 0 && cc <= 7)
            {
                midi2Handler.handleCC(cc, val);

                // Send OLED updates
                auto& ciOut = midi2Handler.getOutgoing();
                if (!ciOut.isEmpty() && midiOutput)
                {
                    for (const auto metadata : ciOut)
                        midiOutput->sendMessageNow(metadata.getMessage());
                    midi2Handler.clearOutgoing();
                }
            }
        }

        // Handle Keystage transport/nav buttons
        // Log ALL CCs for debugging
        if (msg.isController())
        {
            int tcc = msg.getControllerNumber();
            int tval = msg.getControllerValue();
            int tch = msg.getChannel();

            juce::MessageManager::callAsync([this, tcc, tval, tch] {
                statusLabel.setText("CC" + juce::String(tcc) + "=" + juce::String(tval) + " ch" + juce::String(tch),
                    juce::dontSendNotification);
            });
        }

        // Transport/nav buttons — trigger on any non-zero value (button press)
        if (msg.isController() && msg.getControllerValue() > 0)
        {
            int tcc = msg.getControllerNumber();
            if (tcc == 0x29 || tcc == 41)      // PLAY
            {
                pluginHost.getEngine().play();
                juce::MessageManager::callAsync([this] { playButton.setToggleState(true, juce::dontSendNotification); });
            }
            else if (tcc == 0x2A || tcc == 42) // STOP
            {
                auto& eng = pluginHost.getEngine();
                if (!eng.isPlaying())
                {
                    eng.resetPosition();
                    for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
                    {
                        auto* cp = pluginHost.getTrack(t).clipPlayer;
                        if (cp) cp->stopAllSlots();
                    }
                }
                else
                {
                    eng.stop();
                    for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
                    {
                        auto* cp = pluginHost.getTrack(t).clipPlayer;
                        if (cp) cp->sendAllNotesOff.store(true);
                    }
                }
                juce::MessageManager::callAsync([this] {
                    if (timelineComponent) timelineComponent->repaint();
                });
            }
            else if (tcc == 0x2D || tcc == 45) // REC
            {
                pluginHost.getEngine().toggleRecord();
                juce::MessageManager::callAsync([this] {
                    recordButton.setToggleState(pluginHost.getEngine().isRecording(), juce::dontSendNotification);
                });
            }
            else if (tcc == 0x2B || tcc == 43) // REW / SHIFT+VALUE LEFT → move playhead back
            {
                auto& eng = pluginHost.getEngine();
                double pos = eng.getPositionInBeats();
                double grid = timelineComponent ? timelineComponent->getGridResolution() : 1.0;
                eng.setPosition(juce::jmax(0.0, pos - grid));
                if (timelineComponent) timelineComponent->repaint();
            }
            else if (tcc == 0x2C || tcc == 44) // FF / SHIFT+VALUE RIGHT → move playhead forward
            {
                auto& eng = pluginHost.getEngine();
                double pos = eng.getPositionInBeats();
                double grid = timelineComponent ? timelineComponent->getGridResolution() : 1.0;
                eng.setPosition(pos + grid);
                if (timelineComponent) timelineComponent->repaint();
            }
            else if (tcc == 0x2E || tcc == 46) // LOOP → (reserved for future)
            {
                // Could toggle loop mode
            }
            else if (tcc == 0x2F || tcc == 47) // TEMPO — toggle metronome
            {
                pluginHost.getEngine().toggleMetronome();
                juce::MessageManager::callAsync([this] {
                    metronomeButton.setToggleState(pluginHost.getEngine().isMetronomeOn(), juce::dontSendNotification);
                });
            }
            else if (tcc == 58 || tcc == 0x3A) // NEXT TRACK
                selectTrack(juce::jmin(PluginHost::NUM_TRACKS - 1, selectedTrackIndex + 1));
            else if (tcc == 59 || tcc == 0x3B) // PREV TRACK
                selectTrack(juce::jmax(0, selectedTrackIndex - 1));
            else if (tcc == 32) // CC32 — Page/Value button → cycle parameter page
            {
                midi2Handler.nextPage();
                auto& ciOut = midi2Handler.getOutgoing();
                if (!ciOut.isEmpty() && midiOutput) { for (const auto metadata : ciOut) midiOutput->sendMessageNow(metadata.getMessage()); midi2Handler.clearOutgoing(); }
                juce::MessageManager::callAsync([this] {
                    trackNameLabel.setText("Page " + juce::String(midi2Handler.getCurrentPage() + 1)
                        + "/" + juce::String(midi2Handler.getNumPages()), juce::dontSendNotification);
                    updateParamSliders();
                });
            }
            else if (tcc == 60 || tcc == 0x3C) // VALUE DOWN → prev preset
            {
                midi2Handler.prevPreset();
                juce::MessageManager::callAsync([this] {
                    auto& trk = pluginHost.getTrack(selectedTrackIndex);
                    if (trk.plugin) trackNameLabel.setText("Preset: " + trk.plugin->getProgramName(trk.plugin->getCurrentProgram()), juce::dontSendNotification);
                });
            }
            else if (tcc == 61 || tcc == 0x3D) // VALUE UP → next preset
            {
                midi2Handler.nextPreset();
                juce::MessageManager::callAsync([this] {
                    auto& trk = pluginHost.getTrack(selectedTrackIndex);
                    if (trk.plugin) trackNameLabel.setText("Preset: " + trk.plugin->getProgramName(trk.plugin->getCurrentProgram()), juce::dontSendNotification);
                });
            }
            else if (tcc == 62 || tcc == 0x3E) // VALUE KNOB LEFT → prev page
            {
                midi2Handler.prevPage();
                auto& ciOut = midi2Handler.getOutgoing();
                if (!ciOut.isEmpty() && midiOutput) { for (const auto metadata : ciOut) midiOutput->sendMessageNow(metadata.getMessage()); midi2Handler.clearOutgoing(); }
                juce::MessageManager::callAsync([this] { trackNameLabel.setText("Page " + juce::String(midi2Handler.getCurrentPage() + 1), juce::dontSendNotification); });
            }
            else if (tcc == 63 || tcc == 0x3F) // VALUE KNOB RIGHT → next page
            {
                midi2Handler.nextPage();
                auto& ciOut = midi2Handler.getOutgoing();
                if (!ciOut.isEmpty() && midiOutput) { for (const auto metadata : ciOut) midiOutput->sendMessageNow(metadata.getMessage()); midi2Handler.clearOutgoing(); }
                juce::MessageManager::callAsync([this] { trackNameLabel.setText("Page " + juce::String(midi2Handler.getCurrentPage() + 1), juce::dontSendNotification); });
            }
        }

        // Auto-reconnect if connection was lost
        if (!midi2Handler.isConnected() && msg.isController())
        {
            midi2Handler.sendDiscovery();
            auto& ciOut = midi2Handler.getOutgoing();
            if (!ciOut.isEmpty() && midiOutput)
            {
                for (const auto metadata : ciOut)
                    midiOutput->sendMessageNow(metadata.getMessage());
                midi2Handler.clearOutgoing();
            }
        }
    }

    // Buffer MIDI for capture — always-on ring buffer (note on/off, CC, pitch bend)
    // Ignores Active Sensing, Clock, and SysEx. Skips when actively recording.
    if (!pluginHost.getEngine().isRecording())
    {
        CaptureEvent evt;
        evt.absTime = juce::Time::getMillisecondCounterHiRes() * 0.001;
        evt.channel = static_cast<uint8_t>(msg.getChannel());
        evt.pitchBend = 0;
        bool store = false;

        if (msg.isNoteOn())
        {
            evt.type = CaptureEvent::NoteOn;
            evt.data1 = static_cast<uint8_t>(msg.getNoteNumber());
            evt.data2 = static_cast<uint8_t>(msg.getVelocity());
            store = true;
        }
        else if (msg.isNoteOff())
        {
            evt.type = CaptureEvent::NoteOff;
            evt.data1 = static_cast<uint8_t>(msg.getNoteNumber());
            evt.data2 = 0;
            store = true;
        }
        else if (msg.isController())
        {
            evt.type = CaptureEvent::CC;
            evt.data1 = static_cast<uint8_t>(msg.getControllerNumber());
            evt.data2 = static_cast<uint8_t>(msg.getControllerValue());
            store = true;
        }
        else if (msg.isPitchWheel())
        {
            evt.type = CaptureEvent::PitchBend;
            evt.data1 = 0;
            evt.data2 = 0;
            evt.pitchBend = static_cast<int16_t>(msg.getPitchWheelValue() - 8192);
            store = true;
        }

        if (store)
        {
            int pos = captureWritePos.load(std::memory_order_relaxed);
            captureRing[static_cast<size_t>(pos)] = evt;
            captureWritePos.store((pos + 1) % CAPTURE_RING_SIZE, std::memory_order_release);
            int cnt = captureCount.load(std::memory_order_relaxed);
            if (cnt < CAPTURE_RING_SIZE)
                captureCount.store(cnt + 1, std::memory_order_relaxed);
        }
    }

    // Forward all MIDI to the collector for audio processing
    pluginHost.getMidiCollector().addMessageToQueue(msg);
}

void MainComponent::disableCurrentMidiDevice()
{
    if (currentMidiDeviceId.isNotEmpty())
    {
        // Send all-notes-off to prevent stuck notes
        for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
        {
            auto* cp = pluginHost.getTrack(t).clipPlayer;
            if (cp != nullptr)
                cp->sendAllNotesOff.store(true);
        }

        deviceManager.removeMidiInputDeviceCallback(currentMidiDeviceId, this);
        deviceManager.setMidiInputDeviceEnabled(currentMidiDeviceId, false);
        currentMidiDeviceId.clear();
    }
}

void MainComponent::updatePanelBlur()
{
    if (!timelineComponent || !timelineComponent->isVisible()) return;

    // Fix #2: Capture from timeline, not self (avoids feedback loop)
    // Convert panel bounds to timeline's local coordinates
    auto panelArea = panelBoundsCache;
    if (panelArea.isEmpty()) return;

    auto tlBounds = timelineComponent->getBounds();
    auto captureArea = panelArea.translated(-tlBounds.getX(), -tlBounds.getY())
                                .getIntersection(timelineComponent->getLocalBounds());
    if (captureArea.isEmpty()) return;

    // Capture timeline content behind panel at 0.75x for less pixelation (fix #7)
    auto snapshot = timelineComponent->createComponentSnapshot(captureArea, false, 0.75f);
    if (snapshot.isNull()) return;

    // Gaussian blur with larger radius to hide upscale artifacts
    juce::ImageConvolutionKernel kernel(9);
    kernel.createGaussianBlur(9.0f);
    kernel.applyToImage(snapshot, snapshot, snapshot.getBounds());

    panelBlurImage = snapshot;
}

void MainComponent::toggleRightPanel()
{
    if (panelAnimating) return;

    rightPanelVisible = !rightPanelVisible;
    panelSlideTarget = rightPanelVisible ? 1.0f : 0.0f;
    panelAnimStartValue = panelSlideProgress;
    panelAnimStartTime = juce::Time::getMillisecondCounterHiRes() * 0.001;
    panelAnimating = true;
    startTimerHz(isHighTier() ? 120 : 60); // boost frame rate for smooth animation
    panelToggleButton.setButtonText(rightPanelVisible
        ? juce::String::charToString(0x25C0)   // ◀ (hide)
        : juce::String::charToString(0x25B6));  // ▶ (show)
}

void MainComponent::showAudioSettings()
{
#if JUCE_IOS
    // On iOS, audio device is system-controlled — show info only
    juce::String info = "Audio Device: CoreAudio\n";
    if (auto* dev = deviceManager.getCurrentAudioDevice())
    {
        info += "Sample Rate: " + juce::String(dev->getCurrentSampleRate(), 0) + " Hz\n";
        info += "Buffer Size: " + juce::String(dev->getCurrentBufferSizeSamples()) + " samples\n";
        info += "Output Channels: " + juce::String(dev->getActiveOutputChannels().countNumberOfSetBits());
    }
    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "Audio Info", info, "OK");
#else
    auto* sel = new juce::AudioDeviceSelectorComponent(deviceManager, 0, 0, 1, 2, false, false, false, false);
    sel->setSize(500, 400);
    auto* wrapped = new WireframeContentWrapper(sel);
    juce::DialogWindow::LaunchOptions opt;
    opt.content.setOwned(wrapped);
    opt.dialogTitle = "Audio Settings";
    opt.componentToCentreAround = this;
    {
        auto bg = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);
        if (auto* lf = dynamic_cast<DawLookAndFeel*>(&getLookAndFeel()))
            if (lf->getTheme().wireframe) bg = juce::Colour(lf->getTheme().body);
        opt.dialogBackgroundColour = bg;
    }
    opt.escapeKeyTriggersCloseButton = true;
    opt.useNativeTitleBar = true;
    opt.resizable = false;
    opt.launchAsync();
    juce::Component::SafePointer<MainComponent> safeThis(this);
    juce::Timer::callAfterDelay(500, [safeThis] {
        if (auto* self = safeThis.getComponent())
        {
            if (auto* dev = self->deviceManager.getCurrentAudioDevice())
                self->pluginHost.setAudioParams(dev->getCurrentSampleRate(), dev->getCurrentBufferSizeSamples());
            self->updateStatusLabel();
        }
    });
#endif
}

void MainComponent::showSettingsMenu()
{
    juce::PopupMenu menu;
    menu.addItem(1, "Check for Updates...");

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&settingsButton),
        [this](int result)
        {
            if (result == 1)
            {
                auto* dialog = new UpdateDialog();
                dialog->setSize(550, 420);
                auto* wrappedDlg = new WireframeContentWrapper(dialog);
                juce::DialogWindow::LaunchOptions opts;
                opts.content.setOwned(wrappedDlg);
                opts.dialogTitle = "Software Update";
                opts.componentToCentreAround = this;
                {
                    auto bg = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);
                    if (auto* lf = dynamic_cast<DawLookAndFeel*>(&getLookAndFeel()))
                        if (lf->getTheme().wireframe) bg = juce::Colour(lf->getTheme().body);
                    opts.dialogBackgroundColour = bg;
                }
                opts.escapeKeyTriggersCloseButton = true;
                opts.useNativeTitleBar = true;
                opts.resizable = false;
                opts.launchAsync();
            }
        });
}

void MainComponent::updateStatusLabel()
{
    juce::String text;
    if (useComputerKeyboard)
        text += "KB Oct " + juce::String(computerKeyboardOctave) + " | ";
    else if (currentMidiDeviceId.isNotEmpty())
        for (const auto& d : midiDevices)
            if (d.identifier == currentMidiDeviceId) { text += d.name + " | "; break; }

    if (auto* dev = deviceManager.getCurrentAudioDevice())
        text += dev->getName() + " | " + juce::String(dev->getCurrentSampleRate(), 0) + " Hz";
    statusLabel.setText(text, juce::dontSendNotification);
}

// ── Plugin Parameters ─────────────────────────────────────────────────────────

int MainComponent::activeParamCount() const
{
    // Launchkey themes show only 6 visible param knobs (matching the
    // device's encoder count after volume); other themes use the
    // platform default.  Centralized here so paging math, slider
    // population, and layout iteration all agree.
    const auto t = themeManager.getCurrentTheme();
    if (t == ThemeManager::Launchkey || t == ThemeManager::LaunchkeyDark || t == ThemeManager::LaunchkeyOled) return 6;
    return NUM_PARAM_SLIDERS;
}

void MainComponent::highlightParamKnob(int index)
{
    // Reset all knobs to default color
    auto& c = themeManager.getColors();
    for (int i = 0; i < NUM_PARAM_SLIDERS; ++i)
        paramSliders[i]->setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(c.amber));

    activeParamIndex = index;
    paramHighlightAlpha = 1.0f;
    paramHighlightFadingIn = false;
    repaint(); // trigger glow ring repaint
}

void MainComponent::updateParamSliders()
{
    const int N = activeParamCount();
    auto& track = pluginHost.getTrack(selectedTrackIndex);

    if (track.plugin == nullptr)
    {
        for (int i = 0; i < NUM_PARAM_SLIDERS; ++i)
        {
            paramSliders[i]->setEnabled(false);
            paramSliders[i]->setValue(0.0, juce::dontSendNotification);
            paramLabels[i]->setText("", juce::dontSendNotification);
        }
        paramPageLabel.setText("", juce::dontSendNotification);
        paramPageNameLabel.setText("", juce::dontSendNotification);
        return;
    }

    auto& allParams = track.plugin->getParameters();
    int total = allParams.size();
    juce::String pluginName = track.plugin->getName().toLowerCase();

    // Clamp page offset
    if (paramPageOffset >= total) paramPageOffset = juce::jmax(0, total - N);
    if (paramPageOffset < 0) paramPageOffset = 0;

    // Update page label (page 0 = smart, then sequential pages add 1 more)
    int seqPages = juce::jmax(1, (total + N - 1) / N);
    int totalPages = 1 + seqPages; // smart page + sequential pages
    int page = paramSmartPage ? 1 : (paramPageOffset / N) + 2;
    paramPageLabel.setText(juce::String(page) + "/" + juce::String(totalPages), juce::dontSendNotification);

    // Show plugin name in the page name label
    juce::String plugName = track.plugin->getName();
    paramPageNameLabel.setText(plugName, juce::dontSendNotification);

    // ── Page 0: smart selection (plugin-specific + macros + common names) ──
    if (paramSmartPage)
    {
        juce::Array<juce::AudioProcessorParameter*> selectedParams;

        // Plugin-specific mappings
        if (pluginName.contains("diva"))
        {
            juce::StringArray wanted = { "cutoff", "resonance", "hpf", "vco mix", "env2 att", "env2 dec" };
            for (auto& w : wanted)
                for (auto* param : allParams)
                    if (param->getName(30).toLowerCase().contains(w))
                    { selectedParams.add(param); break; }
        }
        else if (pluginName.contains("hive"))
        {
            juce::StringArray wanted = { "macro 1", "macro 2", "macro 3", "macro 4", "cutoff", "resonance" };
            for (auto& w : wanted)
                for (auto* param : allParams)
                    if (param->getName(30).toLowerCase().contains(w))
                    { selectedParams.add(param); break; }
        }
        else if (pluginName.contains("pigments"))
        {
            juce::StringArray wanted = { "macro 1", "macro 2", "macro 3", "macro 4", "macro 5", "macro 6" };
            for (auto& w : wanted)
                for (auto* param : allParams)
                    if (param->getName(30).toLowerCase().contains(w))
                    { selectedParams.add(param); break; }
        }
        else if (pluginName.contains("analog lab") || pluginName.contains("arturia") ||
                 pluginName.contains("jun-6") || pluginName.contains("jup-8") ||
                 pluginName.contains("mini v") || pluginName.contains("cs-80"))
        {
            for (auto* param : allParams)
            {
                juce::String name = param->getName(30).toLowerCase();
                if (name.contains("macro") || name.contains("mcr") || name.contains("assign"))
                    selectedParams.add(param);
                if (selectedParams.size() >= N) break;
            }
        }

        // Generic: macros
        if (selectedParams.isEmpty())
            for (auto* param : allParams)
            {
                juce::String name = param->getName(30).toLowerCase();
                if (name.contains("macro") || name.contains("mcr") || name.contains("assign"))
                    selectedParams.add(param);
                if (selectedParams.size() >= N) break;
            }

        // Generic: common synth params
        if (selectedParams.isEmpty())
        {
            juce::StringArray commonNames = { "cutoff", "filter", "resonance",
                "attack", "decay", "sustain", "release",
                "drive", "mix", "volume", "level", "gain", "osc", "detune", "lfo" };
            for (auto& cn : commonNames)
                for (auto* param : allParams)
                    if (param->getName(30).toLowerCase().contains(cn))
                    { if (!selectedParams.contains(param)) selectedParams.add(param); break; }
        }

        // Fill remaining with first available
        for (int i = 0; i < allParams.size() && selectedParams.size() < N; ++i)
            if (!selectedParams.contains(allParams[i]))
                selectedParams.add(allParams[i]);

        for (int i = 0; i < NUM_PARAM_SLIDERS; ++i)
        {
            if (i < selectedParams.size() && i < N)
            {
                auto* param = selectedParams[i];
                paramSliders[i]->setEnabled(true);
                paramSliders[i]->setValue(param->getValue(), juce::dontSendNotification);
                paramLabels[i]->setText(param->getName(30), juce::dontSendNotification);
                paramSliders[i]->getProperties().set("paramIndex", allParams.indexOf(param));
            }
            else
            {
                paramSliders[i]->setEnabled(false);
                paramSliders[i]->setValue(0.0, juce::dontSendNotification);
                paramLabels[i]->setText("", juce::dontSendNotification);
                paramSliders[i]->getProperties().set("paramIndex", -1);
            }
        }
        return;
    }

    // ── Page 2+: sequential params from offset ──
    for (int i = 0; i < NUM_PARAM_SLIDERS; ++i)
    {
        int paramIdx = paramPageOffset + i;
        if (i < N && paramIdx < total)
        {
            auto* param = allParams[paramIdx];
            paramSliders[i]->setEnabled(true);
            paramSliders[i]->setValue(param->getValue(), juce::dontSendNotification);
            paramLabels[i]->setText(param->getName(30), juce::dontSendNotification);
            paramSliders[i]->getProperties().set("paramIndex", paramIdx);
        }
        else
        {
            paramSliders[i]->setEnabled(false);
            paramSliders[i]->setValue(0.0, juce::dontSendNotification);
            paramLabels[i]->setText("", juce::dontSendNotification);
            paramSliders[i]->getProperties().set("paramIndex", -1);
        }
    }
}

// ── Save/Load/Undo ───────────────────────────────────────────────────────────

// ── Capture helpers ──

// Extract a linear snapshot from the ring buffer (newest events last)
static juce::Array<MainComponent::CaptureEvent> extractRingBuffer(
    const std::array<MainComponent::CaptureEvent, MainComponent::CAPTURE_RING_SIZE>& ring,
    int writePos, int count)
{
    juce::Array<MainComponent::CaptureEvent> result;
    result.ensureStorageAllocated(count);
    int start = (writePos - count + MainComponent::CAPTURE_RING_SIZE) % MainComponent::CAPTURE_RING_SIZE;
    for (int i = 0; i < count; ++i)
        result.add(ring[static_cast<size_t>((start + i) % MainComponent::CAPTURE_RING_SIZE)]);
    return result;
}

// ── Phrase Segmentation ──
// Scans backward from the end to find the musical phrase start.
// Uses silence thresholding (>2s gap) and density clustering.
static int findPhraseStart(const juce::Array<MainComponent::CaptureEvent>& events)
{
    if (events.isEmpty()) return 0;

    constexpr double silenceThreshold = 2.0;  // seconds of silence = phrase boundary

    // Collect note-on indices and times
    juce::Array<int> noteOnIndices;
    juce::Array<double> noteOnTimes;
    for (int i = 0; i < events.size(); ++i)
    {
        if (events[i].type == MainComponent::CaptureEvent::NoteOn)
        {
            noteOnIndices.add(i);
            noteOnTimes.add(events[i].absTime);
        }
    }

    if (noteOnTimes.size() < 2) return 0;

    // Scan backward through note onsets looking for a gap > silenceThreshold
    int phraseFirstOnsetIdx = 0;
    for (int i = noteOnTimes.size() - 1; i > 0; --i)
    {
        double gap = noteOnTimes[i] - noteOnTimes[i - 1];
        if (gap > silenceThreshold)
        {
            phraseFirstOnsetIdx = i;
            break;
        }
    }

    // The phrase starts at (or slightly before) the first onset after the gap.
    // Include any events (CC, pitch bend) that precede the first note-on of the phrase
    // by up to 0.1 seconds (pre-note expression).
    int firstOnsetRingIdx = noteOnIndices[phraseFirstOnsetIdx];
    double phraseTime = events[firstOnsetRingIdx].absTime;

    for (int i = firstOnsetRingIdx - 1; i >= 0; --i)
    {
        if (phraseTime - events[i].absTime > 0.1)
            return i + 1;
    }
    return 0;
}

// ── Tempo Induction via IOI Histogram ──
// Analyzes inter-onset intervals, builds histogram, finds common denominator (tatum).
// Returns BPM clamped to 80-160.
static double detectTempoIOI(const juce::Array<double>& onsetTimesSeconds)
{
    if (onsetTimesSeconds.size() < 3) return 120.0;

    // Collect all inter-onset intervals
    juce::Array<double> intervals;
    for (int i = 1; i < onsetTimesSeconds.size(); ++i)
    {
        double dt = onsetTimesSeconds[i] - onsetTimesSeconds[i - 1];
        if (dt > 0.05 && dt < 2.0)
            intervals.add(dt);
    }
    if (intervals.isEmpty()) return 120.0;

    // Build histogram: try BPM candidates 80-160 and score by how well intervals
    // align to multiples of the beat duration
    double bestBpm = 120.0;
    double bestScore = 0.0;

    for (int candidateBpm = 80; candidateBpm <= 160; ++candidateBpm)
    {
        double beatDur = 60.0 / candidateBpm;
        double score = 0.0;

        for (auto& interval : intervals)
        {
            // Score based on proximity to any integer multiple of beat duration
            double beats = interval / beatDur;
            double nearest = std::round(beats);
            if (nearest < 1.0) nearest = 1.0;
            double error = std::abs(beats - nearest) / nearest;
            score += std::exp(-error * 10.0);
        }

        if (score > bestScore)
        {
            bestScore = score;
            bestBpm = static_cast<double>(candidateBpm);
        }
    }

    // Clamp final result to 80-160 BPM range
    return juce::jlimit(80.0, 160.0, bestBpm);
}

// ── Downbeat Estimation ──
// Adjusts the phrase start time to account for pickup notes (anacrusis).
// If the first note is short and weak compared to the following chord/note,
// treat the strong event as beat 1 and shift the downbeat back.
// Returns the time offset (in seconds) from the first event to the downbeat.
static double estimateDownbeatOffset(const juce::Array<MainComponent::CaptureEvent>& phrase)
{
    // Find first two note-on events
    int firstIdx = -1, secondIdx = -1;
    for (int i = 0; i < phrase.size(); ++i)
    {
        if (phrase[i].type != MainComponent::CaptureEvent::NoteOn) continue;
        if (firstIdx < 0) { firstIdx = i; continue; }
        secondIdx = i;
        break;
    }

    if (firstIdx < 0 || secondIdx < 0) return 0.0;

    const auto& first = phrase[firstIdx];
    const auto& second = phrase[secondIdx];
    double gap = second.absTime - first.absTime;

    // Heuristic: if the first note is significantly weaker than the second,
    // and the gap is short (< 0.5 beat equivalent, roughly < 0.3s at 120 BPM),
    // treat the first note as a pickup.
    bool isPickup = (first.data2 < second.data2 * 0.7) && (gap < 0.4);

    // Also check if second event is a chord (multiple note-ons within 30ms)
    if (!isPickup && gap < 0.4)
    {
        int chordCount = 0;
        for (int i = secondIdx; i < phrase.size(); ++i)
        {
            if (phrase[i].type != MainComponent::CaptureEvent::NoteOn) continue;
            if (phrase[i].absTime - second.absTime < 0.03) chordCount++;
            else break;
        }
        if (chordCount >= 2 && first.data2 < second.data2)
            isPickup = true;
    }

    if (isPickup)
    {
        // The downbeat is at the second note; return negative offset so the
        // first note becomes a pickup before beat 1
        return -(second.absTime - first.absTime);
    }

    return 0.0;
}

// ── Loop Detection ──
// Finds shortest repeating pattern in beat-quantized onsets
static double detectLoopLength(const juce::MidiMessageSequence& events, double totalBeats)
{
    juce::Array<double> onsets;
    for (int i = 0; i < events.getNumEvents(); ++i)
        if (events.getEventPointer(i)->message.isNoteOn())
            onsets.add(events.getEventPointer(i)->message.getTimeStamp());

    if (onsets.size() < 4) return totalBeats;

    double candidates[] = { 8.0, 16.0, 32.0 };  // 2, 4, 8 bars

    for (auto loopLen : candidates)
    {
        if (loopLen >= totalBeats) continue;
        if (totalBeats / loopLen < 1.8) continue;

        int matches = 0;
        int totalNotes = 0;

        for (auto onset : onsets)
        {
            if (onset >= loopLen) break;
            totalNotes++;

            bool found = false;
            for (double cycle = loopLen; cycle < totalBeats; cycle += loopLen)
            {
                for (auto other : onsets)
                {
                    if (std::abs((other - cycle) - onset) < 0.25)
                    {
                        found = true;
                        break;
                    }
                }
                if (!found) break;
            }
            if (found) matches++;
        }

        if (totalNotes > 0 && static_cast<double>(matches) / totalNotes > 0.7)
            return loopLen;
    }

    return totalBeats;
}

// ── Soft Quantize (80% strength to 1/16 grid) ──
static void softQuantize(juce::MidiMessageSequence& events, double gridSize = 0.25)
{
    constexpr double strength = 0.8;

    // Quantize all events, then ensure note-offs stay after their note-ons
    for (int i = 0; i < events.getNumEvents(); ++i)
    {
        auto& msg = events.getEventPointer(i)->message;
        double t = msg.getTimeStamp();
        double nearest = std::round(t / gridSize) * gridSize;
        msg.setTimeStamp(t + (nearest - t) * strength);
    }

    // Fix any inverted note pairs (note-off before note-on)
    events.updateMatchedPairs();
    for (int i = 0; i < events.getNumEvents(); ++i)
    {
        auto* evt = events.getEventPointer(i);
        if (evt->message.isNoteOn() && evt->noteOffObject != nullptr)
        {
            double onTime = evt->message.getTimeStamp();
            double offTime = evt->noteOffObject->message.getTimeStamp();
            if (offTime <= onTime)
                evt->noteOffObject->message.setTimeStamp(onTime + 0.05);  // minimum 1/20 beat
        }
    }
}

// ── Check if project has any existing clips ──
static bool projectHasClips(PluginHost& host)
{
    for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
    {
        auto& track = host.getTrack(t);
        if (track.clipPlayer == nullptr) continue;
        for (int s = 0; s < track.clipPlayer->getNumSlots(); ++s)
            if (track.clipPlayer->getSlot(s).hasContent())
                return true;
    }
    return false;
}

void MainComponent::performCapture()
{
    int count = captureCount.load();
    if (count < 2)
    {
        statusLabel.setText("Nothing to capture", juce::dontSendNotification);
        juce::Component::SafePointer<MainComponent> safeThis(this);
        juce::Timer::callAfterDelay(2000, [safeThis] {
            if (auto* self = safeThis.getComponent())
                self->statusLabel.setText("", juce::dontSendNotification);
        });
        return;
    }

    // ── 1. Snapshot the ring buffer ──
    // Read writePos first, then use min(count, writePos-distance) to avoid
    // reading slots the MIDI thread might be actively writing to.
    int wp = captureWritePos.load(std::memory_order_acquire);
    int safeCount = juce::jmin(count, CAPTURE_RING_SIZE - 1);  // leave 1 slot margin
    if (safeCount <= 0) {
        statusLabel.setText("Nothing to capture", juce::dontSendNotification);
        return;
    }
    auto allEvents = extractRingBuffer(captureRing, wp, safeCount);

    // ── 2. Phrase Segmentation — find the musical idea ──
    int phraseStart = findPhraseStart(allEvents);
    juce::Array<CaptureEvent> phrase;
    for (int i = phraseStart; i < allEvents.size(); ++i)
        phrase.add(allEvents[i]);

    if (phrase.isEmpty())
    {
        statusLabel.setText("Nothing to capture", juce::dontSendNotification);
        return;
    }

    // Collect note-on times (in seconds) for tempo analysis
    juce::Array<double> noteOnTimes;
    for (auto& e : phrase)
        if (e.type == CaptureEvent::NoteOn)
            noteOnTimes.add(e.absTime);

    if (noteOnTimes.size() < 2)
    {
        statusLabel.setText("Not enough notes", juce::dontSendNotification);
        return;
    }

    auto& eng = pluginHost.getEngine();
    bool transportWasPlaying = eng.isPlaying();

    // ── 3. Tempo Induction ──
    double bpm = eng.getBpm();
    bool emptyProject = !projectHasClips(pluginHost);

    if (!transportWasPlaying && emptyProject)
    {
        // Case 1: Empty project — detect tempo, set global BPM
        if (noteOnTimes.size() >= 3)
        {
            bpm = detectTempoIOI(noteOnTimes);
            eng.setBpm(bpm);
        }
    }
    // Case 2: Existing project or transport playing — use existing BPM (warp to grid)

    double beatsPerSecond = bpm / 60.0;

    // ── 4. Downbeat Estimation ──
    double downbeatOffset = estimateDownbeatOffset(phrase);
    // Clamp offset to prevent pathological values (max 1 beat pickup)
    double maxPickupSeconds = 60.0 / bpm;  // 1 beat in seconds
    downbeatOffset = juce::jlimit(-maxPickupSeconds, 0.0, downbeatOffset);

    double phraseStartTime = phrase.getFirst().absTime;
    double phraseEndTime = phrase.getLast().absTime;
    // Downbeat reference: where beat 1.1.1 falls in absolute time
    double downbeatTime = phraseStartTime - downbeatOffset;

    // ── 5. Convert absolute time to beat positions ──
    // Find the last note-off/note-on time for accurate duration (ignore trailing CC/PB)
    double lastNoteTime = phraseStartTime;
    for (int i = phrase.size() - 1; i >= 0; --i)
    {
        if (phrase[i].type == CaptureEvent::NoteOn || phrase[i].type == CaptureEvent::NoteOff)
        {
            lastNoteTime = phrase[i].absTime;
            break;
        }
    }

    juce::MidiMessageSequence beatEvents;
    for (auto& e : phrase)
    {
        double beatPos = (e.absTime - downbeatTime) * beatsPerSecond;

        // Pickup notes land at negative beat positions — wrap them to the end
        // of the previous bar (e.g., -0.5 beats → 3.5 in a 4-beat bar).
        // We'll shift them after we know the clip length.

        juce::MidiMessage msg;
        switch (e.type)
        {
            case CaptureEvent::NoteOn:
                msg = juce::MidiMessage::noteOn(e.channel, (int)e.data1, (juce::uint8)e.data2);
                break;
            case CaptureEvent::NoteOff:
                msg = juce::MidiMessage::noteOff(e.channel, (int)e.data1);
                break;
            case CaptureEvent::CC:
                msg = juce::MidiMessage::controllerEvent(e.channel, (int)e.data1, (int)e.data2);
                break;
            case CaptureEvent::PitchBend:
                msg = juce::MidiMessage::pitchWheel(e.channel, e.pitchBend + 8192);
                break;
        }
        msg.setTimeStamp(beatPos);
        beatEvents.addEvent(msg);
    }
    beatEvents.updateMatchedPairs();

    // ── 6. Calculate clip length — round to nearest musical unit (2, 4, 8 bars) ──
    double durationBeats = (lastNoteTime - downbeatTime) * beatsPerSecond;
    double lengthInBeats;
    if (durationBeats <= 8.0)
        lengthInBeats = 8.0;   // 2 bars
    else if (durationBeats <= 16.0)
        lengthInBeats = 16.0;  // 4 bars
    else if (durationBeats <= 32.0)
        lengthInBeats = 32.0;  // 8 bars
    else
        lengthInBeats = std::ceil(durationBeats / 16.0) * 16.0;  // round to 4-bar blocks
    if (lengthInBeats < 8.0) lengthInBeats = 8.0;

    // ── 7. Wrap negative beat positions (pickup notes) to end of clip ──
    beatEvents.updateMatchedPairs();
    for (int i = 0; i < beatEvents.getNumEvents(); ++i)
    {
        auto* evt = beatEvents.getEventPointer(i);
        if (evt->message.getTimeStamp() < 0.0)
        {
            double shift = lengthInBeats;
            evt->message.setTimeStamp(evt->message.getTimeStamp() + shift);
            // Also wrap the paired note-off, but only if it's also negative.
            // If note-off is already positive (e.g. note-on at -0.5, note-off at 0.5),
            // shifting it would push it beyond the clip end.
            if (evt->message.isNoteOn() && evt->noteOffObject != nullptr)
            {
                if (evt->noteOffObject->message.getTimeStamp() < 0.0)
                    evt->noteOffObject->message.setTimeStamp(
                        evt->noteOffObject->message.getTimeStamp() + shift);
            }
        }
    }
    beatEvents.updateMatchedPairs();

    // ── 8. Soft Quantize (80% strength, 1/16 grid) ──
    softQuantize(beatEvents, 0.25);
    beatEvents.updateMatchedPairs();

    // ── 8. Loop Detection — trim to shortest repeating cycle ──
    double loopLen = detectLoopLength(beatEvents, lengthInBeats);
    if (loopLen < lengthInBeats)
    {
        juce::MidiMessageSequence trimmed;
        std::set<int> openNotes;

        for (int i = 0; i < beatEvents.getNumEvents(); ++i)
        {
            auto& msg = beatEvents.getEventPointer(i)->message;
            if (msg.getTimeStamp() >= loopLen) break;

            trimmed.addEvent(msg);
            if (msg.isNoteOn())
                openNotes.insert(msg.getNoteNumber());
            else if (msg.isNoteOff())
                openNotes.erase(msg.getNoteNumber());
        }

        // Close any notes still open at the loop boundary
        for (int note : openNotes)
        {
            auto noteOff = juce::MidiMessage::noteOff(1, note);
            noteOff.setTimeStamp(loopLen - 0.01);
            trimmed.addEvent(noteOff);
        }

        trimmed.updateMatchedPairs();
        beatEvents = trimmed;
        lengthInBeats = loopLen;
    }

    // ── 9. Close any unclosed notes at clip boundary ──
    {
        beatEvents.updateMatchedPairs();
        std::set<int> openNotes;
        for (int i = 0; i < beatEvents.getNumEvents(); ++i)
        {
            auto& msg = beatEvents.getEventPointer(i)->message;
            if (msg.isNoteOn()) openNotes.insert(msg.getNoteNumber());
            else if (msg.isNoteOff()) openNotes.erase(msg.getNoteNumber());
        }
        for (int note : openNotes)
        {
            auto noteOff = juce::MidiMessage::noteOff(1, note);
            noteOff.setTimeStamp(lengthInBeats - 0.01);
            beatEvents.addEvent(noteOff);
        }
        if (!openNotes.empty())
            beatEvents.updateMatchedPairs();
    }

    // ── 10. Create clip ──
    auto& track = pluginHost.getTrack(selectedTrackIndex);
    if (track.clipPlayer == nullptr) return;

    int emptySlot = track.clipPlayer->findOrCreateEmptySlot();

    takeSnapshot();

    auto newClip = std::make_unique<MidiClip>();
    newClip->lengthInBeats = lengthInBeats;
    newClip->events = beatEvents;

    // ── 10. Transport-aware placement ──
    if (transportWasPlaying)
    {
        // Warp: place relative to current playback position, snapped to bar
        double nowTime = juce::Time::getMillisecondCounterHiRes() * 0.001;
        double currentPosBeats = eng.getPositionInBeats();
        double captureStartBeats = currentPosBeats - (nowTime - downbeatTime) * beatsPerSecond;
        captureStartBeats = std::floor(captureStartBeats / 4.0) * 4.0;
        if (captureStartBeats < 0.0) captureStartBeats = 0.0;
        newClip->timelinePosition = captureStartBeats;
    }
    else
    {
        newClip->timelinePosition = 0.0;
    }

    double clipStartBeats = transportWasPlaying ? newClip->timelinePosition : 0.0;
    double clipEndBeats = clipStartBeats + lengthInBeats;

    track.clipPlayer->getSlot(emptySlot).clip = std::move(newClip);
    track.clipPlayer->getSlot(emptySlot).state.store(ClipSlot::Playing);

    // Set loop region around the captured clip and enable looping
    eng.setLoopRegion(clipStartBeats, clipEndBeats);
    if (!eng.isLoopEnabled())
        eng.toggleLoop();
    loopButton.setToggleState(true, juce::dontSendNotification);

    // Reset playhead to clip start, flush state, and start transport
    eng.setPosition(clipStartBeats);
    track.clipPlayer->sendAllNotesOff.store(true);
    if (!eng.isPlaying())
        eng.play();

    // Clear ring buffer
    captureCount.store(0);
    captureWritePos.store(0);

    // Count notes for display
    int numNotes = 0;
    for (int i = 0; i < track.clipPlayer->getSlot(emptySlot).clip->events.getNumEvents(); ++i)
        if (track.clipPlayer->getSlot(emptySlot).clip->events.getEventPointer(i)->message.isNoteOn())
            numNotes++;

    int bars = static_cast<int>(lengthInBeats / 4);
    juce::String info = "Captured " + juce::String(numNotes) + " notes";
    if (!transportWasPlaying && emptyProject)
        info += " @ " + juce::String(static_cast<int>(bpm)) + " BPM";

    statusLabel.setText(info, juce::dontSendNotification);
    chordLabel.setText(juce::String(bars) + " bar" + (bars != 1 ? "s" : ""), juce::dontSendNotification);
    juce::Component::SafePointer<MainComponent> safeThis(this);
    juce::Timer::callAfterDelay(2000, [safeThis] {
        if (auto* self = safeThis.getComponent())
        {
            self->statusLabel.setText("", juce::dontSendNotification);
            self->chordLabel.setText("---", juce::dontSendNotification);
        }
    });

    if (timelineComponent) timelineComponent->repaint();
}

void MainComponent::takeSnapshot()
{
    // Trim future history if we undid something
    while (undoHistory.size() > undoIndex + 1)
        undoHistory.removeLast();

    ProjectSnapshot snap;
    snap.bpm = pluginHost.getEngine().getBpm();
    snap.loopEnabled = pluginHost.getEngine().isLoopEnabled();
    snap.loopStart = pluginHost.getEngine().getLoopStart();
    snap.loopEnd = pluginHost.getEngine().getLoopEnd();

    for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
    {
        auto* cp = pluginHost.getTrack(t).clipPlayer;
        if (cp == nullptr) continue;

        for (int s = 0; s < cp->getNumSlots(); ++s)
        {
            auto& slot = cp->getSlot(s);
            if (slot.clip != nullptr && slot.hasContent())
            {
                ProjectSnapshot::ClipData cd;
                cd.trackIndex = t;
                cd.slotIndex = s;
                cd.lengthInBeats = slot.clip->lengthInBeats;
                cd.timelinePosition = slot.clip->timelinePosition;

                for (int e = 0; e < slot.clip->events.getNumEvents(); ++e)
                    cd.events.addEvent(slot.clip->events.getEventPointer(e)->message);
                cd.events.updateMatchedPairs();

                snap.clips.add(std::move(cd));
            }
            if (slot.audioClip != nullptr && slot.hasContent())
            {
                ProjectSnapshot::ClipData cd;
                cd.isAudio = true;
                cd.audioSamples = slot.audioClip->samples;  // deep copy
                cd.audioSampleRate = slot.audioClip->sampleRate;
                cd.lengthInBeats = slot.audioClip->lengthInBeats;
                cd.timelinePosition = slot.audioClip->timelinePosition;
                cd.trackIndex = t;
                cd.slotIndex = s;
                snap.clips.add(std::move(cd));
            }
        }
    }

    // Capture automation lanes
    for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
    {
        auto& track = pluginHost.getTrack(t);
        juce::SpinLock::ScopedLockType lock(track.automationLock);
        for (auto* lane : track.automationLanes)
        {
            ProjectSnapshot::AutomationLaneData ad;
            ad.trackIndex = t;
            ad.parameterIndex = lane->parameterIndex;
            ad.parameterName = lane->parameterName;
            ad.points = lane->points;
            snap.automationData.add(std::move(ad));
        }
    }

    undoHistory.add(std::move(snap));
    undoIndex = undoHistory.size() - 1;

    // Limit history
    if (undoHistory.size() > 50)
    {
        undoHistory.remove(0);
        undoIndex--;
    }

    trimUndoHistoryByMemory();
}

void MainComponent::trimUndoHistoryByMemory()
{
    constexpr size_t maxBytes = 100 * 1024 * 1024;  // 100 MB limit

    auto estimateSize = [](const ProjectSnapshot& snap) -> size_t {
        size_t bytes = 0;
        for (auto& cd : snap.clips)
        {
            bytes += cd.events.getNumEvents() * 16;  // rough MIDI estimate
            if (cd.isAudio)
                bytes += static_cast<size_t>(cd.audioSamples.getNumChannels())
                       * static_cast<size_t>(cd.audioSamples.getNumSamples()) * sizeof(float);
        }
        return bytes;
    };

    size_t totalBytes = 0;
    for (auto& snap : undoHistory)
        totalBytes += estimateSize(snap);

    while (totalBytes > maxBytes && undoHistory.size() > 2)
    {
        totalBytes -= estimateSize(undoHistory[0]);
        undoHistory.remove(0);
        undoIndex = juce::jmax(0, undoIndex - 1);
    }
}

void MainComponent::restoreSnapshot(const ProjectSnapshot& snap)
{
    // Clear all clips
    for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
    {
        auto* cp = pluginHost.getTrack(t).clipPlayer;
        if (cp == nullptr) continue;

        for (int s = 0; s < cp->getNumSlots(); ++s)
        {
            auto& slot = cp->getSlot(s);
            slot.clip = nullptr;
            slot.audioClip = nullptr;
            slot.state.store(ClipSlot::Empty);
        }
    }

    pluginHost.getEngine().setBpm(snap.bpm);
    bpmLabel.setText(juce::String(static_cast<int>(snap.bpm)) + " BPM", juce::dontSendNotification);

    // Restore loop state
    if (snap.loopEnabled)
    {
        pluginHost.getEngine().setLoopRegion(snap.loopStart, snap.loopEnd);
        if (!pluginHost.getEngine().isLoopEnabled())
            pluginHost.getEngine().toggleLoop();
    }
    else
    {
        if (pluginHost.getEngine().isLoopEnabled())
            pluginHost.getEngine().toggleLoop();
        pluginHost.getEngine().clearLoopRegion();
    }
    loopButton.setToggleState(snap.loopEnabled, juce::dontSendNotification);

    // Restore clips
    for (auto& cd : snap.clips)
    {
        auto* cp = pluginHost.getTrack(cd.trackIndex).clipPlayer;
        if (cp == nullptr) continue;

        auto& slot = cp->getSlot(cd.slotIndex);

        if (cd.isAudio)
        {
            slot.audioClip = std::make_unique<AudioClip>();
            slot.audioClip->samples = cd.audioSamples;
            slot.audioClip->sampleRate = cd.audioSampleRate;
            slot.audioClip->lengthInBeats = cd.lengthInBeats;
            slot.audioClip->timelinePosition = cd.timelinePosition;
            slot.clip = nullptr;  // ensure only one type
        }
        else
        {
            slot.clip = std::make_unique<MidiClip>();
            slot.clip->lengthInBeats = cd.lengthInBeats;
            slot.clip->timelinePosition = cd.timelinePosition;

            for (int e = 0; e < cd.events.getNumEvents(); ++e)
                slot.clip->events.addEvent(cd.events.getEventPointer(e)->message);
            slot.clip->events.updateMatchedPairs();
            slot.audioClip = nullptr;  // ensure only one type
        }

        slot.state.store(ClipSlot::Playing);
    }

    // Restore automation lanes
    for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
    {
        auto& track = pluginHost.getTrack(t);
        juce::SpinLock::ScopedLockType lock(track.automationLock);
        track.automationLanes.clear();
    }
    for (auto& ad : snap.automationData)
    {
        auto& track = pluginHost.getTrack(ad.trackIndex);
        juce::SpinLock::ScopedLockType lock(track.automationLock);
        auto* lane = new AutomationLane();
        lane->parameterIndex = ad.parameterIndex;
        lane->parameterName = ad.parameterName;
        lane->points = ad.points;
        track.automationLanes.add(lane);
    }

    updateTrackDisplay();
    if (timelineComponent) timelineComponent->repaint();
}

void MainComponent::saveProjectQuick()
{
    if (currentProjectFile != juce::File())
    {
        saveProjectToFile(currentProjectFile);
    }
    else
    {
        saveProject(); // No previous save — fall through to Save As
    }
}

void MainComponent::saveProject()
{
    auto chooser = std::make_shared<juce::FileChooser>("Save Project",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.seqproj");

    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file == juce::File()) return;

        auto fileName = file.getFileNameWithoutExtension() + ".seqproj";
        auto saveFile = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile(fileName);

        saveProjectToFile(saveFile);
    });
}

void MainComponent::saveProjectToFile(const juce::File& saveFile)
{
        // Stop recording to prevent data race on clip events
        auto& eng = pluginHost.getEngine();
        if (eng.isRecording()) eng.toggleRecord();

        auto xml = std::make_unique<juce::XmlElement>("SequencerProject");
        xml->setAttribute("version", 2);
        xml->setAttribute("bpm", eng.getBpm());
        xml->setAttribute("loopEnabled", eng.isLoopEnabled());
        xml->setAttribute("loopStart", eng.getLoopStart());
        xml->setAttribute("loopEnd", eng.getLoopEnd());
        xml->setAttribute("metronome", eng.isMetronomeOn());
        xml->setAttribute("countIn", countInButton.getToggleState());
        xml->setAttribute("gridSelector", gridSelector.getSelectedId());
        xml->setAttribute("position", eng.getPositionInBeats());
        xml->setAttribute("selectedTrack", selectedTrackIndex);
        xml->setAttribute("theme", static_cast<int>(themeManager.getCurrentTheme()));

        for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
        {
            auto& track = pluginHost.getTrack(t);

            auto* trackXml = xml->createNewChildElement("Track");
            trackXml->setAttribute("index", t);
            trackXml->setAttribute("type", track.type == TrackType::Audio ? "audio" : "midi");

            if (track.gainProcessor)
            {
                trackXml->setAttribute("volume", static_cast<double>(track.gainProcessor->volume.load()));
                trackXml->setAttribute("pan", static_cast<double>(track.gainProcessor->pan.load()));
                trackXml->setAttribute("muted", track.gainProcessor->muted.load());
                trackXml->setAttribute("soloed", track.gainProcessor->soloed.load());
            }

            // Save arpeggiator settings
            if (auto* cp = pluginHost.getTrack(t).clipPlayer)
            {
                auto& arp = cp->getArpeggiator();
                trackXml->setAttribute("arpEnabled", arp.isEnabled());
                trackXml->setAttribute("arpMode", static_cast<int>(arp.getMode()));
                trackXml->setAttribute("arpRate", arp.getRate());
                trackXml->setAttribute("arpOctave", arp.getOctaveRange());
                trackXml->setAttribute("arpGate", static_cast<double>(arp.getGate()));
                trackXml->setAttribute("arpSwing", static_cast<double>(arp.getSwing()));
            }

            // Save plugin description and state
            if (track.plugin)
            {
                auto* pluginXml = trackXml->createNewChildElement("Plugin");
                // Get description directly from the loaded plugin instance
                auto* instance = dynamic_cast<juce::AudioPluginInstance*>(track.plugin);
                if (instance != nullptr)
                {
                    auto desc = instance->getPluginDescription();
                    pluginXml->addChildElement(desc.createXml().release());
                }
                else
                {
                    // Fallback: search known list by name
                    for (const auto& desc : pluginHost.getPluginList().getTypes())
                    {
                        if (desc.name == track.plugin->getName())
                        {
                            pluginXml->addChildElement(desc.createXml().release());
                            break;
                        }
                    }
                }
                // Save plugin state (presets, parameters)
                juce::MemoryBlock state;
                track.plugin->getStateInformation(state);
                pluginXml->setAttribute("state", state.toBase64Encoding());
            }

            // Save FX chains
            for (int fx = 0; fx < Track::NUM_FX_SLOTS; ++fx)
            {
                if (track.fxSlots[fx].processor != nullptr)
                {
                    auto* fxXml = trackXml->createNewChildElement("FX");
                    fxXml->setAttribute("slot", fx);
                    fxXml->setAttribute("bypassed", track.fxSlots[fx].bypassed);
                    // Get description directly from the loaded FX instance
                    auto* fxInstance = dynamic_cast<juce::AudioPluginInstance*>(track.fxSlots[fx].processor);
                    if (fxInstance != nullptr)
                    {
                        auto desc = fxInstance->getPluginDescription();
                        fxXml->addChildElement(desc.createXml().release());
                    }
                    else
                    {
                        for (const auto& desc : pluginHost.getPluginList().getTypes())
                        {
                            if (desc.name == track.fxSlots[fx].processor->getName())
                            {
                                fxXml->addChildElement(desc.createXml().release());
                                break;
                            }
                        }
                    }
                    juce::MemoryBlock fxState;
                    track.fxSlots[fx].processor->getStateInformation(fxState);
                    fxXml->setAttribute("state", fxState.toBase64Encoding());
                }
            }

            // Save automation lanes
            for (auto* lane : track.automationLanes)
            {
                if (lane->points.size() < 1) continue;
                auto* autoXml = trackXml->createNewChildElement("Automation");
                autoXml->setAttribute("paramIndex", lane->parameterIndex);
                autoXml->setAttribute("paramName", lane->parameterName);
                for (auto& pt : lane->points)
                {
                    auto* ptXml = autoXml->createNewChildElement("Point");
                    ptXml->setAttribute("beat", pt.beat);
                    ptXml->setAttribute("value", static_cast<double>(pt.value));
                }
            }

            auto* cp = track.clipPlayer;
            if (cp == nullptr) continue;

            for (int s = 0; s < cp->getNumSlots(); ++s)
            {
                auto& slot = cp->getSlot(s);
                if (!slot.hasContent()) continue;

                // Save MIDI clip
                if (slot.clip != nullptr)
                {
                    auto* clipXml = trackXml->createNewChildElement("Clip");
                    clipXml->setAttribute("slot", s);
                    clipXml->setAttribute("length", slot.clip->lengthInBeats);
                    clipXml->setAttribute("position", slot.clip->timelinePosition);

                    for (int e = 0; e < slot.clip->events.getNumEvents(); ++e)
                    {
                        auto* event = slot.clip->events.getEventPointer(e);
                        auto* noteXml = clipXml->createNewChildElement("Event");
                        noteXml->setAttribute("time", event->message.getTimeStamp());
                        noteXml->setAttribute("data", juce::String::toHexString(
                            event->message.getRawData(), event->message.getRawDataSize()));
                    }
                }

                // Save AudioClip
                if (slot.audioClip != nullptr && slot.audioClip->samples.getNumSamples() > 0)
                {
                    auto* audioXml = trackXml->createNewChildElement("AudioClip");
                    audioXml->setAttribute("slot", s);
                    audioXml->setAttribute("length", slot.audioClip->lengthInBeats);
                    audioXml->setAttribute("position", slot.audioClip->timelinePosition);
                    audioXml->setAttribute("sampleRate", slot.audioClip->sampleRate);
                    int numCh = slot.audioClip->samples.getNumChannels();
                    int numSamp = slot.audioClip->samples.getNumSamples();
                    audioXml->setAttribute("channels", numCh);

                    // Interleave and base64-encode float samples
                    juce::MemoryBlock audioData;
                    audioData.setSize(static_cast<size_t>(numSamp) * numCh * sizeof(float));
                    float* ptr = static_cast<float*>(audioData.getData());
                    for (int si = 0; si < numSamp; ++si)
                        for (int ch = 0; ch < numCh; ++ch)
                            *ptr++ = slot.audioClip->samples.getSample(ch, si);

                    audioXml->setAttribute("data", audioData.toBase64Encoding());
                }
            }
        }

        saveFile.getParentDirectory().createDirectory();

        auto xmlString = xml->toString();

        if (saveFile.replaceWithText(xmlString))
        {
            currentProjectFile = saveFile;
            statusLabel.setText("Saved: " + saveFile.getFileNameWithoutExtension(), juce::dontSendNotification);
        }
        else
            statusLabel.setText("SAVE FAILED", juce::dontSendNotification);
}

void MainComponent::loadProject()
{
    auto chooser = std::make_shared<juce::FileChooser>("Load Project",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.seqproj");

    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file == juce::File()) return;

        currentProjectFile = file;

        // Stop transport and disconnect audio BEFORE unloading plugins
        auto& eng = pluginHost.getEngine();
        if (eng.isPlaying()) eng.stop();
        if (eng.isRecording()) eng.toggleRecord();
        audioPlayer.setProcessor(nullptr);  // disconnect audio thread from graph

        // Read file — try app documents first (where we save), then original path
        juce::String fileContent;

        // Try app documents directory first (where save always writes)
        auto appDoc = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile(file.getFileName());
        if (appDoc.existsAsFile())
            fileContent = appDoc.loadFileAsString();

        // Fallback: try the file picker path directly
        if (fileContent.isEmpty())
            fileContent = file.loadFileAsString();

        // Fallback: ObjC security-scoped read
        if (fileContent.isEmpty())
            fileContent = FileAccessHelper::readFileContent(file);

        if (fileContent.isEmpty())
        {
            statusLabel.setText("Cannot read file: " + file.getFileName(), juce::dontSendNotification);
            audioPlayer.setProcessor(&pluginHost);
            return;
        }

        auto xml = juce::parseXML(fileContent);

        if (xml == nullptr || !xml->hasTagName("SequencerProject"))
        {
            // Show more detail about what went wrong
            juce::String reason = (xml == nullptr) ? "XML parse failed" : ("wrong root tag: " + xml->getTagName());
            statusLabel.setText("Invalid project: " + reason, juce::dontSendNotification);
            audioPlayer.setProcessor(&pluginHost);
            return;
        }

        // Clear all tracks — safe now that audio thread is disconnected
        for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
        {
            pluginHost.unloadPlugin(t);
            midi2Handler.setPlugin(nullptr); // Clear dangling pointer
            for (int fx = 0; fx < Track::NUM_FX_SLOTS; ++fx)
                pluginHost.unloadFx(t, fx);
            auto* cp = pluginHost.getTrack(t).clipPlayer;
            if (cp == nullptr) continue;
            for (int s = 0; s < cp->getNumSlots(); ++s)
            {
                cp->getSlot(s).clip = nullptr;
                cp->getSlot(s).audioClip = nullptr;
                cp->getSlot(s).state.store(ClipSlot::Empty);
            }
        }

        // Clear undo history before loading new project
        undoHistory.clear();
        undoIndex = -1;

        double bpm = xml->getDoubleAttribute("bpm", 120.0);
        eng.setBpm(bpm);
        bpmLabel.setText(juce::String(static_cast<int>(bpm)) + " BPM", juce::dontSendNotification);

        // Restore loop region
        if (xml->hasAttribute("loopEnabled"))
        {
            bool loopOn = xml->getBoolAttribute("loopEnabled");
            double ls = xml->getDoubleAttribute("loopStart", 0.0);
            double le = xml->getDoubleAttribute("loopEnd", 0.0);
            if (loopOn && le > ls)
                eng.setLoopRegion(ls, le);
            if (loopOn != eng.isLoopEnabled())
                eng.toggleLoop();
            loopButton.setToggleState(loopOn, juce::dontSendNotification);
        }

        // Restore metronome state
        if (xml->hasAttribute("metronome"))
        {
            bool metOn = xml->getBoolAttribute("metronome");
            if (metOn != eng.isMetronomeOn())
                eng.toggleMetronome();
            metronomeButton.setToggleState(metOn, juce::dontSendNotification);
        }

        // Restore count-in state
        countInButton.setToggleState(xml->getBoolAttribute("countIn", false), juce::dontSendNotification);

        // Restore grid selector
        if (xml->hasAttribute("gridSelector"))
            gridSelector.setSelectedId(xml->getIntAttribute("gridSelector", 1), juce::dontSendNotification);

        // Restore transport position
        if (xml->hasAttribute("position"))
            eng.setPosition(xml->getDoubleAttribute("position", 0.0));

        // Restore selected track
        if (xml->hasAttribute("selectedTrack"))
            selectTrack(xml->getIntAttribute("selectedTrack", 0));

        // Restore theme
        if (xml->hasAttribute("theme"))
        {
            int themeIdx = xml->getIntAttribute("theme", 0);
            if (themeIdx >= 0 && themeIdx < ThemeManager::NumThemes)
            {
                themeManager.setTheme(static_cast<ThemeManager::Theme>(themeIdx), this);
                themeSelector.setSelectedId(themeIdx + 1, juce::dontSendNotification);
                applyThemeToControls();
                applyLaunchkeyToolbarColors();
                // Active param-knob count is theme-dependent —
                // re-populate so the visible knob count matches.
                updateParamSliders();
                // Start DeviceMotion/Metal for glass themes (same as theme selector onChange)
                if (timelineComponent)
                    timelineComponent->setOpaque(!themeManager.isGlassOverlay());
                if (themeManager.isGlassOverlay())
                {
                    DeviceMotion::getInstance().start();
                    startTimerHz(getGlassTimerHz());
                    glassAnimEnabled = true;
                }
                else
                {
                    DeviceMotion::getInstance().stop();
                    startTimerHz(getBaseTimerHz());
                }
#if JUCE_IOS
                if (metalRenderer && metalRendererAttached)
                    metalRenderer->setVisible(themeManager.isGlassOverlay() && glassAnimEnabled);
#endif
            }
        }

        // Track load errors for reporting
        juce::StringArray loadErrors;

        // First pass: restore mixer settings and clips only (no plugin loading)
        for (auto* trackXml : xml->getChildWithTagNameIterator("Track"))
        {
            int t = trackXml->getIntAttribute("index", -1);
            if (t < 0 || t >= PluginHost::NUM_TRACKS) continue;

            auto& track = pluginHost.getTrack(t);

            if (trackXml->getStringAttribute("type") == "audio")
                pluginHost.setTrackType(t, TrackType::Audio);
            else
                pluginHost.setTrackType(t, TrackType::MIDI);

            if (track.gainProcessor)
            {
                track.gainProcessor->volume.store(static_cast<float>(trackXml->getDoubleAttribute("volume", 0.8)));
                track.gainProcessor->pan.store(static_cast<float>(trackXml->getDoubleAttribute("pan", 0.0)));
                track.gainProcessor->muted.store(trackXml->getBoolAttribute("muted", false));
                track.gainProcessor->soloed.store(trackXml->getBoolAttribute("soloed", false));
            }

            // Load arpeggiator settings
            if (auto* cp = pluginHost.getTrack(t).clipPlayer)
            {
                auto& arp = cp->getArpeggiator();
                arp.setEnabled(trackXml->getBoolAttribute("arpEnabled", false));
                arp.setMode(static_cast<Arpeggiator::Mode>(trackXml->getIntAttribute("arpMode", 0)));
                arp.setRate(trackXml->getIntAttribute("arpRate", 3));
                arp.setOctaveRange(trackXml->getIntAttribute("arpOctave", 1));
                arp.setGate(static_cast<float>(trackXml->getDoubleAttribute("arpGate", 0.8)));
                arp.setSwing(static_cast<float>(trackXml->getDoubleAttribute("arpSwing", 0.0)));
            }

#if !JUCE_IOS
            // Desktop: load plugins synchronously in-line
            auto* pluginXml = trackXml->getChildByName("Plugin");
            if (pluginXml != nullptr)
            {
                bool pluginLoaded = false;
                for (auto* descXml : pluginXml->getChildIterator())
                {
                    juce::PluginDescription desc;
                    if (desc.loadFromXml(*descXml))
                    {
                        juce::String err;
                        if (pluginHost.loadPlugin(t, desc, err))
                        {
                            auto stateStr = pluginXml->getStringAttribute("state");
                            if (stateStr.isNotEmpty())
                            {
                                juce::MemoryBlock state;
                                state.fromBase64Encoding(stateStr);
                                auto* plug = pluginHost.getTrack(t).plugin;
                                if (plug != nullptr && state.getSize() > 0)
                                    plug->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
                            }
                            pluginLoaded = true;
                        }
                        else
                            loadErrors.add("Track " + juce::String(t + 1) + ": " + desc.name + " — " + err);
                        break;
                    }
                }
                if (!pluginLoaded && pluginXml->getNumChildElements() > 0)
                    loadErrors.add("Track " + juce::String(t + 1) + ": plugin not found");
            }
            for (auto* fxXml : trackXml->getChildWithTagNameIterator("FX"))
            {
                int fxSlot = fxXml->getIntAttribute("slot", -1);
                if (fxSlot < 0 || fxSlot >= Track::NUM_FX_SLOTS) continue;
                bool fxLoaded = false;
                for (auto* descXml : fxXml->getChildIterator())
                {
                    juce::PluginDescription desc;
                    if (desc.loadFromXml(*descXml))
                    {
                        juce::String err;
                        if (pluginHost.loadFx(t, fxSlot, desc, err))
                        {
                            auto stateStr = fxXml->getStringAttribute("state");
                            if (stateStr.isNotEmpty())
                            {
                                juce::MemoryBlock state;
                                state.fromBase64Encoding(stateStr);
                                auto* proc = pluginHost.getTrack(t).fxSlots[fxSlot].processor;
                                if (proc != nullptr && state.getSize() > 0)
                                    proc->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
                            }
                            pluginHost.setFxBypassed(t, fxSlot, fxXml->getBoolAttribute("bypassed", false));
                            fxLoaded = true;
                        }
                        else
                            loadErrors.add("Track " + juce::String(t + 1) + " FX" + juce::String(fxSlot + 1) + ": " + desc.name + " — " + err);
                        break;
                    }
                }
                if (!fxLoaded && fxXml->getNumChildElements() > 0)
                    loadErrors.add("Track " + juce::String(t + 1) + " FX" + juce::String(fxSlot + 1) + ": effect not found");
            }
#endif

            auto* cp = track.clipPlayer;
            if (cp == nullptr) continue;

            for (auto* clipXml : trackXml->getChildWithTagNameIterator("Clip"))
            {
                int s = clipXml->getIntAttribute("slot", -1);
                if (s < 0) continue;
                cp->ensureSlots(s + 1);

                auto& slot = cp->getSlot(s);
                slot.clip = std::make_unique<MidiClip>();
                slot.clip->lengthInBeats = clipXml->getDoubleAttribute("length", 4.0);
                slot.clip->timelinePosition = clipXml->getDoubleAttribute("position", 0.0);

                for (auto* noteXml : clipXml->getChildWithTagNameIterator("Event"))
                {
                    double time = noteXml->getDoubleAttribute("time", 0.0);
                    auto hexData = noteXml->getStringAttribute("data");

                    juce::MemoryBlock mb;
                    mb.loadFromHexString(hexData);

                    if (mb.getSize() > 0)
                    {
                        auto msg = juce::MidiMessage(mb.getData(), static_cast<int>(mb.getSize()));
                        msg.setTimeStamp(time);
                        slot.clip->events.addEvent(msg);
                    }
                }

                slot.clip->events.updateMatchedPairs();
                slot.state.store(ClipSlot::Playing);
            }

            // Load AudioClips
            for (auto* audioXml : trackXml->getChildWithTagNameIterator("AudioClip"))
            {
                int s = audioXml->getIntAttribute("slot", -1);
                if (s < 0) continue;
                cp->ensureSlots(s + 1);

                auto& slot = cp->getSlot(s);
                slot.audioClip = std::make_unique<AudioClip>();
                slot.audioClip->lengthInBeats = audioXml->getDoubleAttribute("length", 4.0);
                slot.audioClip->timelinePosition = audioXml->getDoubleAttribute("position", 0.0);
                slot.audioClip->sampleRate = audioXml->getDoubleAttribute("sampleRate", 44100.0);

                int numCh = audioXml->getIntAttribute("channels", 2);
                auto dataStr = audioXml->getStringAttribute("data");

                juce::MemoryBlock audioData;
                audioData.fromBase64Encoding(dataStr);

                // Validate: data size must be a multiple of (channels * sizeof(float))
                size_t bytesPerFrame = static_cast<size_t>(numCh) * sizeof(float);
                if (bytesPerFrame > 0 && audioData.getSize() >= bytesPerFrame)
                {
                    int numSamp = static_cast<int>(audioData.getSize() / bytesPerFrame);
                    slot.audioClip->samples.setSize(numCh, numSamp);
                    const float* ptr = static_cast<const float*>(audioData.getData());
                    for (int si = 0; si < numSamp; ++si)
                        for (int ch = 0; ch < numCh; ++ch)
                            slot.audioClip->samples.setSample(ch, si, *ptr++);
                }

                slot.state.store(ClipSlot::Playing);
            }

            // Load automation lanes
            {
                const juce::SpinLock::ScopedLockType lock(track.automationLock);
                track.automationLanes.clear();
                for (auto* autoXml : trackXml->getChildWithTagNameIterator("Automation"))
                {
                    auto* lane = new AutomationLane();
                    lane->parameterIndex = autoXml->getIntAttribute("paramIndex", -1);
                    lane->parameterName = autoXml->getStringAttribute("paramName");
                    for (auto* ptXml : autoXml->getChildWithTagNameIterator("Point"))
                    {
                        AutomationPoint pt;
                        pt.beat = ptXml->getDoubleAttribute("beat", 0.0);
                        pt.value = static_cast<float>(ptXml->getDoubleAttribute("value", 0.0));
                        lane->points.add(pt);
                    }
                    track.automationLanes.add(lane);
                }
            }
        }

        // Recalculate solo count from restored track states
        {
            int soloTotal = 0;
            for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
            {
                auto& trk = pluginHost.getTrack(t);
                if (trk.gainProcessor && trk.gainProcessor->soloed.load())
                    soloTotal++;
            }
            pluginHost.soloCount.store(soloTotal);
        }

#if JUCE_IOS
        // On iOS, load plugins asynchronously after the file callback returns
        // to avoid run loop reentrancy crashes during AUv3 async instantiation
        auto xmlCopy = std::make_shared<juce::XmlElement>(*xml);
        auto fileName = file.getFileName();
        juce::MessageManager::callAsync([this, xmlCopy, fileName] {
            audioPlayer.setProcessor(nullptr);
            juce::StringArray loadErrors;
            for (auto* trackXml : xmlCopy->getChildWithTagNameIterator("Track"))
            {
                int t = trackXml->getIntAttribute("index", -1);
                if (t < 0 || t >= PluginHost::NUM_TRACKS) continue;

                auto* pluginXml = trackXml->getChildByName("Plugin");
                if (pluginXml != nullptr)
                {
                    int numChildren = 0;
                    for (auto* descXml : pluginXml->getChildIterator())
                    {
                        numChildren++;
                        juce::PluginDescription desc;
                        if (desc.loadFromXml(*descXml))
                        {
                            juce::String err;
                            if (pluginHost.loadPlugin(t, desc, err))
                            {
                                auto stateStr = pluginXml->getStringAttribute("state");
                                if (stateStr.isNotEmpty())
                                {
                                    juce::MemoryBlock state;
                                    state.fromBase64Encoding(stateStr);
                                    auto* plugin = pluginHost.getTrack(t).plugin;
                                    if (plugin != nullptr && state.getSize() > 0)
                                        plugin->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
                                }
                            }
                            else
                                loadErrors.add("Track " + juce::String(t + 1) + ": " + desc.name + " LOAD FAILED: " + err);
                            break;
                        }
                    }
                }

                // Restore FX
                for (auto* fxXml : trackXml->getChildWithTagNameIterator("FX"))
                {
                    int fxSlot = fxXml->getIntAttribute("slot", -1);
                    if (fxSlot < 0 || fxSlot >= Track::NUM_FX_SLOTS) continue;
                    for (auto* descXml : fxXml->getChildIterator())
                    {
                        juce::PluginDescription fxDesc;
                        if (fxDesc.loadFromXml(*descXml))
                        {
                            juce::String err;
                            if (pluginHost.loadFx(t, fxSlot, fxDesc, err))
                            {
                                auto stateStr = fxXml->getStringAttribute("state");
                                if (stateStr.isNotEmpty())
                                {
                                    juce::MemoryBlock state;
                                    state.fromBase64Encoding(stateStr);
                                    auto* proc = pluginHost.getTrack(t).fxSlots[fxSlot].processor;
                                    if (proc != nullptr && state.getSize() > 0)
                                        proc->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
                                }
                                pluginHost.setFxBypassed(t, fxSlot, fxXml->getBoolAttribute("bypassed", false));
                            }
                            else
                                loadErrors.add("Track " + juce::String(t + 1) + " FX" + juce::String(fxSlot + 1) + ": " + fxDesc.name + " — " + err);
                            break;
                        }
                    }
                }
            }
            audioPlayer.setProcessor(&pluginHost);
            selectTrack(selectedTrackIndex);  // refresh plugin selector, presets, UI
            updatePresetList();
            updateParamSliders();
            if (timelineComponent) timelineComponent->repaint();

            if (loadErrors.isEmpty())
                statusLabel.setText("Loaded: " + fileName, juce::dontSendNotification);
            else
                statusLabel.setText("Loaded with " + juce::String(loadErrors.size()) + " error(s): " + loadErrors.joinIntoString("; "), juce::dontSendNotification);
            takeSnapshot();
        });
#else
        // Desktop: plugins loaded synchronously above — reconnect audio and show status
        audioPlayer.setProcessor(&pluginHost);
        updateTrackDisplay();
        if (timelineComponent) timelineComponent->repaint();
        if (loadErrors.isEmpty())
            statusLabel.setText("Loaded: " + file.getFileName(), juce::dontSendNotification);
        else
            statusLabel.setText("Loaded with " + juce::String(loadErrors.size()) + " error(s): " + loadErrors.joinIntoString("; "), juce::dontSendNotification);
#endif

#if JUCE_IOS
        // iOS: show loading status while async plugin load happens
        updateTrackDisplay();
        if (timelineComponent) timelineComponent->repaint();
        statusLabel.setText("Loading plugins...", juce::dontSendNotification);
#endif

        // Take snapshot for undo
        takeSnapshot();
    });
}

// ── Layout ───────────────────────────────────────────────────────────────────

void MainComponent::mouseDown(const juce::MouseEvent& e)
{
    grabKeyboardFocus();

    // Glass overlay: spawn ripple on any tap
    if (themeManager.isGlassOverlay())
        addRipple(static_cast<float>(e.getPosition().x), static_cast<float>(e.getPosition().y));

    // Tap on small visualizer to go fullscreen
    if (!visualizerFullScreen)
    {
        auto* src = e.eventComponent;
        if (src == &spectrumDisplay || src == &lissajousDisplay || src == &waveTerrainDisplay
            || src == &shaderToyDisplay || src == &analyzerDisplay || src == &geissDisplay
            || src == &projectMDisplay || src == heartbeatDisplay.get()
            || src == bioResonanceDisplay.get()
            || src == &fluidSimDisplay || src == &rayMarchDisplay)
        {
            visualizerFullScreen = true;
            projectorMode = true;
            fullscreenButton.setToggleState(true, juce::dontSendNotification);
            startTimerHz(5);  // Minimal rate — visualizer has its own 60Hz timer
            resized();
            repaint();
            return;
        }
    }

    // BPM drag start
    if (e.eventComponent == &bpmLabel)
    {
        bpmDragging = true;
        bpmDragStart = e.getScreenPosition().toFloat();
        bpmDragStartValue = pluginHost.getEngine().getBpm();
        return;
    }

    // Tap on CPU OLED to open expanded EKG window
    if (e.eventComponent == &cpuLabel)
    {
        showExpandedEkg();
        return;
    }

#if JUCE_IOS
    bool isPhone = AUScanner::isIPhone() && !forceIPadLayout;
    if (isPhone)
    {
        swipeStartPos = e.position;
        swipeActive = true;
    }
    else
    {
        // iPad: detect swipe near the panel edge or right screen edge
        // When panel is open, zone starts at the panel's left edge
        // When panel is closed, zone is the rightmost 80px of screen
        int panelW = (int)((float)getPanelWidth() * panelSlideProgress);
        int edgeStart = getWidth() - panelW - 80;
        if (e.position.x > edgeStart)
        {
            swipeStartPos = e.position;
            swipeActive = true;
        }
    }
#endif
}

void MainComponent::mouseDrag(const juce::MouseEvent& e)
{
    // BPM drag — up/right increases, down/left decreases
    if (bpmDragging && e.eventComponent == &bpmLabel)
    {
        auto current = e.getScreenPosition().toFloat();
        float dx = current.x - bpmDragStart.x;
        float dy = -(current.y - bpmDragStart.y);  // invert Y so up = positive
        float delta = (dx + dy) * 0.3f;  // combine both axes
        double newBpm = juce::jlimit(20.0, 300.0, bpmDragStartValue + static_cast<double>(delta));
        pluginHost.getEngine().setBpm(newBpm);
    }
}

void MainComponent::mouseUp(const juce::MouseEvent& e)
{
    if (bpmDragging)
    {
        bpmDragging = false;
        return;
    }

#if JUCE_IOS
    if (swipeActive)
    {
        swipeActive = false;
        auto delta = e.position - swipeStartPos;
        float absX = std::abs(delta.x);
        float absY = std::abs(delta.y);

        bool isPhone = AUScanner::isIPhone() && !forceIPadLayout;
        if (isPhone)
        {
            // iPhone: horizontal swipe switches tracks
            if (absX > 80.0f && absX > absY * 2.0f)
            {
                if (delta.x < 0)
                    selectTrack(juce::jmin(PluginHost::NUM_TRACKS - 1, selectedTrackIndex + 1));
                else
                    selectTrack(juce::jmax(0, selectedTrackIndex - 1));
            }
        }
        else
        {
            // iPad: swipe left from right edge = hide panel, swipe right = show panel
            if (absX > 50.0f && absX > absY * 1.5f)
            {
                if (delta.x < 0 && rightPanelVisible)
                    toggleRightPanel();
                else if (delta.x > 0 && !rightPanelVisible)
                    toggleRightPanel();
            }
        }
    }
#else
    (void)e;
#endif
}

void MainComponent::mouseDoubleClick(const juce::MouseEvent& e)
{
    // Double-tap BPM label to type a value
    if (e.eventComponent == &bpmLabel)
    {
        auto callback = juce::ModalCallbackFunction::create([this](int result)
        {
            (void)result;
        });

        auto* alert = new juce::AlertWindow("Set BPM", "", juce::AlertWindow::NoIcon);
        alert->addTextEditor("bpm", juce::String(static_cast<int>(pluginHost.getEngine().getBpm())), "BPM (20-300):");
        alert->addButton("OK", 1);
        alert->addButton("Cancel", 0);

        alert->enterModalState(true, juce::ModalCallbackFunction::create([this, alert](int result)
        {
            if (result == 1)
            {
                auto text = alert->getTextEditorContents("bpm");
                int bpm = text.getIntValue();
                if (bpm >= 20 && bpm <= 300)
                    pluginHost.getEngine().setBpm(static_cast<double>(bpm));
            }
            delete alert;
        }), false);
        return;
    }
}

void MainComponent::showExpandedEkg()
{
    // Create an overlay with a large EKG display
    auto* overlay = new juce::Component();
    overlay->setName("EkgOverlay");

    auto* closeBtn = new juce::TextButton("CLOSE");
    closeBtn->setColour(juce::TextButton::buttonColourId, juce::Colours::red.withAlpha(0.9f));
    closeBtn->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    closeBtn->onClick = [this] {
        for (int i = getNumChildComponents() - 1; i >= 0; --i)
            if (auto* child = getChildComponent(i))
                if (child->getName() == "EkgOverlay")
                {
                    removeChildComponent(i);
                    delete child;
                    break;
                }
    };

    // EKG display component — captures cpu data by reference
    struct EkgDisplay : public juce::Component, public juce::Timer
    {
        juce::Component::SafePointer<MainComponent> owner;
        EkgDisplay(MainComponent& o) : owner(&o) { startTimerHz(15); }
        void timerCallback() override { if (owner) repaint(); else stopTimer(); }

        void paint(juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat();

            // Get theme colors
            uint32_t lcdBg = 0xff000000, lcdText = 0xffb8d8f0, red = 0xffff4444, amber = 0xffffaa44;
            if (auto* lnf = dynamic_cast<DawLookAndFeel*>(&getLookAndFeel()))
            {
                auto& t = lnf->getTheme();
                lcdBg = t.lcdBg; lcdText = t.lcdText; red = t.red; amber = t.amber;
            }

            // OLED background
            g.setColour(juce::Colour(lcdBg));
            g.fillRoundedRectangle(bounds, 6.0f);
            g.setColour(juce::Colour(0xff333333));
            g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

            auto inner = bounds.reduced(10.0f, 8.0f);
            if (!owner) return;
            float cpu = owner->currentCpuPercent / 100.0f;

            juce::Colour waveCol = cpu > 0.75f ? juce::Colour(red) :
                                   cpu > 0.33f ? juce::Colour(amber) :
                                   juce::Colour(lcdText);

            // Grid lines
            g.setColour(juce::Colour(lcdText).withAlpha(0.08f));
            int gridX = static_cast<int>(inner.getWidth() / 20.0f);
            int gridY = static_cast<int>(inner.getHeight() / 10.0f);
            for (int i = 0; i <= 20; ++i)
                g.drawVerticalLine(static_cast<int>(inner.getX() + i * gridX), inner.getY(), inner.getBottom());
            for (int i = 0; i <= 10; ++i)
                g.drawHorizontalLine(static_cast<int>(inner.getY() + i * gridY), inner.getX(), inner.getRight());

            // EKG sweep trace from circular buffer (same as small OLED but bigger)
            float waveH = inner.getHeight() * 0.4f;
            float baselineY = inner.getCentreY();
            float waveW = inner.getWidth();
            int bufSize = MainComponent::EKG_BUFFER_SIZE;
            int writePos = owner->ekgWritePos;
            int gapSize = 8;

            juce::Path wavePath;
            bool pathStarted = false;

            for (int i = 0; i < bufSize; ++i)
            {
                int bufIdx = (writePos + i) % bufSize;
                int distFromCursor = (bufSize - i) % bufSize;
                if (distFromCursor > 0 && distFromCursor <= gapSize)
                {
                    pathStarted = false;
                    continue;
                }

                float x = inner.getX() + (static_cast<float>(i) / static_cast<float>(bufSize - 1)) * waveW;
                float v = owner->ekgBuffer[static_cast<size_t>(bufIdx)];
                float y = baselineY - v * waveH;

                if (!pathStarted) { wavePath.startNewSubPath(x, y); pathStarted = true; }
                else wavePath.lineTo(x, y);
            }

            g.setColour(waveCol.withAlpha(0.9f));
            g.strokePath(wavePath, juce::PathStrokeType(2.0f));

            // Glow dot at cursor
            int cursorIdx = (writePos - 1 + bufSize) % bufSize;
            float cursorFrac = static_cast<float>((cursorIdx - writePos + bufSize) % bufSize) / static_cast<float>(bufSize - 1);
            float cursorX = inner.getX() + cursorFrac * waveW;
            float cursorV = owner->ekgBuffer[static_cast<size_t>(cursorIdx)];
            float cursorY = baselineY - cursorV * waveH;
            g.setColour(waveCol.withAlpha(0.7f));
            g.fillEllipse(cursorX - 4, cursorY - 4, 8, 8);

            // Stats text
            auto textArea = inner.removeFromBottom(20.0f);
            g.setColour(waveCol.withAlpha(0.9f));
            g.setFont(14.0f);

            int bpm = static_cast<int>(60.0f + cpu * 120.0f);
            juce::String status = cpu > 0.75f ? "CRITICAL" : cpu > 0.33f ? "TACHYCARDIA" : "NORMAL";
            g.drawText("CPU " + juce::String(static_cast<int>(owner->currentCpuPercent)) + "%   "
                       + "RAM " + juce::String(owner->currentRamMB) + "MB   "
                       + "HR " + juce::String(bpm) + " BPM   " + status,
                       textArea.toNearestInt(), juce::Justification::centred);
        }
    };

    auto* infoBtn = new juce::TextButton("?");
    infoBtn->setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    infoBtn->setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.7f));
    infoBtn->onClick = [] {
        auto* alert = new juce::AlertWindow("CPU Health Monitor",
            "This EKG visualizes your device's CPU load as a cardiac rhythm, "
            "using the same PQRST waveform morphology as a real electrocardiogram.\n\n"
            "WAVEFORM COMPONENTS\n\n"
            "P Wave - Small bump before the main spike. Represents baseline system "
            "overhead (OS, background tasks).\n\n"
            "QRS Complex - The sharp spike showing audio processing load. "
            "The R wave grows with CPU usage and widens under heavy load.\n\n"
            "ST Segment - Flat normally, elevates with sustained high CPU "
            "(like cardiac ischemia - no headroom between processing cycles).\n\n"
            "T Wave - Recovery phase. Inverts above 85% CPU (danger sign).\n\n"
            "HEART RATE\n\n"
            "0-33% CPU = 60-99 BPM (normal sinus rhythm, green)\n"
            "33-75% CPU = 100-150 BPM (sinus tachycardia, amber)\n"
            "75%+ CPU = 150-180 BPM (critical tachycardia, red)\n\n"
            "WHAT TO DO\n\n"
            "If the EKG turns red:\n"
            "- Reduce plugin count\n"
            "- Increase audio buffer size\n"
            "- Close background apps",
            juce::AlertWindow::InfoIcon);
        alert->addButton("OK", 1);
        alert->enterModalState(true, juce::ModalCallbackFunction::create(
            [alert](int) { delete alert; }), false);
    };

    auto* ekgDisplay = new EkgDisplay(*this);
    overlay->addAndMakeVisible(closeBtn);
    overlay->addAndMakeVisible(infoBtn);
    overlay->addAndMakeVisible(ekgDisplay);

    int closeBarH = 40;
    int w = juce::jmin(500, getWidth() - 40);
    int h = 250;
    int ox = (getWidth() - w) / 2;
    int oy = (getHeight() - h - closeBarH) / 2;

    overlay->setBounds(ox, oy, w, h + closeBarH);
    closeBtn->setBounds(0, 0, w - 44, closeBarH);
    infoBtn->setBounds(w - 44, 0, 44, closeBarH);
    ekgDisplay->setBounds(0, closeBarH, w, h);

    addAndMakeVisible(overlay);
    overlay->toFront(true);
}

void MainComponent::showPhoneMenu()
{
    juce::PopupMenu menu;

    // Track info at top
    auto& track = pluginHost.getTrack(selectedTrackIndex);
    juce::String trackName = "Track " + juce::String(selectedTrackIndex + 1);
    if (track.plugin != nullptr)
        trackName += " - " + track.plugin->getName();
    menu.addSectionHeader(trackName);
    menu.addSeparator();

    // Track navigation submenu
    juce::PopupMenu trackMenu;
    for (int i = 0; i < PluginHost::NUM_TRACKS; ++i)
    {
        auto& t = pluginHost.getTrack(i);
        juce::String label = "Track " + juce::String(i + 1);
        if (t.plugin != nullptr)
            label += " - " + t.plugin->getName();
        trackMenu.addItem(200 + i, label, true, i == selectedTrackIndex);
    }
    menu.addSubMenu("Select Track", trackMenu);
    menu.addSeparator();

    // Project
    menu.addItem(1, "Save Project");
    menu.addItem(2, "Load Project");
    menu.addItem(3, "Undo");
    menu.addItem(4, "Redo");
    menu.addSeparator();

    // Audio
    menu.addItem(5, "Audio Info");
    menu.addItem(6, "Test Note");
    menu.addItem(7, juce::String("MIDI Learn ") + (midiLearnActive ? "(Active)" : ""), true, midiLearnActive);
    menu.addItem(8, juce::String("Count-In ") + (countInButton.getToggleState() ? "ON" : "OFF"), true, countInButton.getToggleState());
    menu.addSeparator();

    // Visualizer submenu
    juce::PopupMenu visMenu;
    visMenu.addItem(100, "Spectrum", true, currentVisMode == 0);
    visMenu.addItem(101, "Lissajous", true, currentVisMode == 1);
    visMenu.addItem(102, "Terrain", true, currentVisMode == 2);
    visMenu.addItem(103, "Geiss", true, currentVisMode == 3);
    visMenu.addItem(104, "MilkDrop", true, currentVisMode == 4);
    visMenu.addItem(105, "Analyzer", true, currentVisMode == 5);
    visMenu.addItem(106, "Heartbeat", true, currentVisMode == 6);
    visMenu.addItem(107, "BioSync", true, currentVisMode == 7);
    visMenu.addItem(108, "Fluid", true, currentVisMode == 8);
    visMenu.addItem(109, "RayMarch", true, currentVisMode == 9);
    menu.addSubMenu("Visualizer", visMenu);
    menu.addItem(10, "Fullscreen Vis");
    menu.addSeparator();

    // FX slot
    juce::PopupMenu fxMenu;
    fxMenu.addItem(300, "(No FX)", true, true);
    for (int i = 0; i < fxDescriptions.size(); ++i)
        fxMenu.addItem(301 + i, fxDescriptions[i].name);
    menu.addSubMenu("FX Insert", fxMenu);
    menu.addSeparator();

    // Theme submenu
    juce::PopupMenu themeMenu;
    for (int i = 0; i < ThemeManager::NumThemes; ++i)
    {
        if (i == ThemeManager::IoniqForest) continue;
        themeMenu.addItem(50 + i, ThemeManager::getThemeName(static_cast<ThemeManager::Theme>(i)),
                         true, themeSelector.getSelectedId() == i + 1);
    }
    menu.addSubMenu("Theme", themeMenu);
    menu.addSeparator();
    menu.addItem(11, "Switch to iPad Layout");

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&phoneMenuButton),
        [this](int result) {
            if (result == 0) return;
            if (result == 1) saveProject();
            else if (result == 2) loadProject();
            else if (result == 3) { if (undoIndex > 0) { undoIndex--; restoreSnapshot(undoHistory[undoIndex]); } }
            else if (result == 4) { if (undoIndex < undoHistory.size() - 1) { undoIndex++; restoreSnapshot(undoHistory[undoIndex]); } }
            else if (result == 5) showAudioSettings();
            else if (result == 6) playTestNote();
            else if (result == 7) {
                midiLearnActive = !midiLearnActive;
                if (midiLearnActive) midiLearnTarget = MidiTarget::None;
            }
            else if (result == 8) countInButton.setToggleState(!countInButton.getToggleState(), juce::sendNotification);
            else if (result == 10) {
                visualizerFullScreen = true;
                projectorMode = true;
                fullscreenButton.setToggleState(true, juce::dontSendNotification);
                startTimerHz(5);  // Minimal rate — visualizer has its own 60Hz timer
                resized();
                repaint();
            }
            else if (result == 11) {
                forceIPadLayout = true;
                resized();
                repaint();
            }
            else if (result >= 100 && result <= 109) {
                currentVisMode = result - 100;
                visSelector.setSelectedId(currentVisMode + 1, juce::dontSendNotification);
                updateVisualizerTimers();
                resized();
                repaint();
            }
            else if (result >= 200 && result < 216) {
                selectTrack(result - 200);
            }
            else if (result >= 50 && result < 50 + ThemeManager::NumThemes) {
                themeSelector.setSelectedId(result - 50 + 1, juce::sendNotification);
            }
            else if (result >= 301) {
                int fxIdx = result - 301;
                if (fxIdx >= 0 && fxIdx < fxDescriptions.size()) {
                    fxSelectors[0]->setSelectedId(fxIdx + 2, juce::sendNotification);
                }
            }
            else if (result == 300) {
                fxSelectors[0]->setSelectedId(1, juce::sendNotification);
            }
        });
}

void MainComponent::paint(juce::Graphics& g)
{
    auto& c = themeManager.getColors();

    // Main body
    bool glassOverlay = themeManager.isGlassOverlay();

    if (glassOverlay)
    {
        // Deep ocean gradient — slightly lighter at top (surface), darker at bottom (deep)
        bool isOceania = (themeManager.getCurrentTheme() == ThemeManager::OceanGlass);
        if (isOceania)
        {
            juce::ColourGradient depth(
                juce::Colour(0xff0a1420), 0, 0,
                juce::Colour(0xff020408), 0, (float)getHeight(), false);
            g.setGradientFill(depth);
            g.fillAll();
        }
        else
        {
            g.fillAll(juce::Colour(c.body));
        }
    }
    else
    {
        g.fillAll(juce::Colour(c.body));
    }

#if JUCE_IOS
    bool paintPhone = AUScanner::isIPhone() && !forceIPadLayout;
    int iosStatusBarH = paintPhone ? 20 : 30;
    int topBarDrawH = iosStatusBarH + (paintPhone ? 44 : 70);
#else
    int iosStatusBarH = 0;
    int topBarDrawH = 80;
    bool paintPhone = false;
#endif

    if (paintPhone)
    {
        if (glassOverlay)
        {
            // Glass overlay on iPhone — ocean gradient or dark body, caustics, no chrome backgrounds
            bool isOceania = (themeManager.getCurrentTheme() == ThemeManager::OceanGlass);
            if (isOceania)
            {
                juce::ColourGradient depth(
                    juce::Colour(0xff0a1420), 0, 0,
                    juce::Colour(0xff020408), 0, (float)getHeight(), false);
                g.setGradientFill(depth);
                g.fillAll();
            }

            // Caustics on phone — skip if Metal GPU renderer is active
            if (glassAnimEnabled)
            {
#if JUCE_IOS
                if (!metalRendererAttached)
#endif
                    drawWaterCaustics(g, getLocalBounds());
            }
        }
        else
        {
            // Normal phone: draw oak/wood top bar if theme supports it, else solid
            if (auto* lnf = dynamic_cast<DawLookAndFeel*>(&getLookAndFeel()))
            {
                if (lnf->getSidePanelWidth() > 0)
                    lnf->drawTopBarBackground(g, 0, 0, getWidth(), topBarDrawH);
                else
                {
                    g.setColour(juce::Colour(c.bodyLight));
                    g.fillRect(0, 0, getWidth(), topBarDrawH);
                }
            }
            else
            {
                g.setColour(juce::Colour(c.bodyLight));
                g.fillRect(0, 0, getWidth(), topBarDrawH);
            }

            // Bottom bar background
            g.setColour(juce::Colour(c.bodyDark));
            g.fillRect(0, getHeight() - 36, getWidth(), 36);
            g.setColour(juce::Colour(c.border));
            g.drawHorizontalLine(getHeight() - 36, 0, static_cast<float>(getWidth()));
        }
        return;
    }

    if (!glassOverlay && !visualizerFullScreen)
    {
        if (auto* lnf = dynamic_cast<DawLookAndFeel*>(&getLookAndFeel()))
        {
            if (lnf->getSidePanelWidth() > 0)
            {
                // Custom top bar (e.g. wood grain) — stop before oak strip
                int sidePW = lnf->getSidePanelWidth();
                int rpW = (int)((float)getPanelWidth() * panelSlideProgress);
                int topBarWidth = getWidth() - sidePW - rpW - sidePW - sidePW;
                lnf->drawTopBarBackground(g, sidePW, 0, topBarWidth, topBarDrawH);
            }
            else
            {
                g.setColour(juce::Colour(c.bodyLight));
                g.fillRect(0, 0, getWidth(), topBarDrawH);
            }
        }
        else
        {
            g.setColour(juce::Colour(c.bodyLight));
            g.fillRect(0, 0, getWidth(), topBarDrawH);
        }
    }

    // Toolbar background
    int oakW = 0;
    if (auto* dlnf = dynamic_cast<DawLookAndFeel*>(&getLookAndFeel()))
        if (dlnf->getSidePanelWidth() > 0)
            oakW = dlnf->getSidePanelWidth();
    int rpW = (int)((float)getPanelWidth() * panelSlideProgress);
    int rightPanelTotal = rpW + oakW + oakW;  // right panel + oak strip + side panel
    int toolbarRight = getWidth() - rightPanelTotal;

    // Paint right panel background — full 180px width, translated to match slide position
    int pw = getPanelWidth();
    int panelSlideOff = (int)((1.0f - panelSlideProgress) * (float)pw);
    int panelPaintLeft = getWidth() - (oakW > 0 ? oakW : 0) - pw + panelSlideOff;
    int panelPaintW = pw;
    if (panelSlideProgress > 0.01f && !glassOverlay && !visualizerFullScreen)
    {
        auto panelRect = juce::Rectangle<int>(panelPaintLeft, 0, panelPaintW, getHeight());
        panelBoundsCache = panelRect;

        // Check if Liquid Glass theme
        bool isGlassTheme = (themeManager.getCurrentTheme() == ThemeManager::LiquidGlass);

        if (isGlassTheme)
        {
            // Dark glass panel with elevated surface feel
            g.setColour(juce::Colour(0xff0e1014));  // slightly lighter than pure black
            g.fillRect(panelRect);

            // Gradient lift on left edge
            juce::ColourGradient edgeGrad(
                juce::Colours::white.withAlpha(0.08f), (float)panelPaintLeft, 0.0f,
                juce::Colours::transparentBlack, (float)(panelPaintLeft + 40), 0.0f, false);
            g.setGradientFill(edgeGrad);
            g.fillRect(panelRect);

            // Bright left edge line
            g.setColour(juce::Colours::white.withAlpha(0.20f));
            g.drawVerticalLine(panelPaintLeft, 0.0f, (float)getHeight());
        }
        else
        {
            g.setColour(juce::Colour(c.body));
            g.fillRect(panelRect);
            // Wireframe themes (LK Dark) get an OLED-cyan outline along
            // the panel edges so it reads as a discrete card rather
            // than dissolving into the black canvas.  Generous corner
            // radius so the rounded curve reads on the visible (left)
            // edge where the panel meets the timeline.
            if (c.wireframe)
            {
                auto pr = panelRect.toFloat().reduced(0.75f);
                g.setColour(juce::Colour(c.borderLight));
                g.drawRoundedRectangle(pr, 12.0f, 1.5f);
            }
        }
    }

    if (!glassOverlay && !visualizerFullScreen)
    {
        // Draw OLED background behind status/chord in top bar (always, regardless of panel state)
        if (statusLabel.isVisible() && chordLabel.isVisible())
        {
            auto oledArea = statusLabel.getBounds().getUnion(chordLabel.getBounds());
            auto borderArea = oledArea.expanded(3, 2);
            g.setColour(juce::Colour(c.borderLight));
            g.fillRoundedRectangle(borderArea.toFloat(), 4.0f);
            g.setColour(juce::Colour(c.lcdBg));
            g.fillRoundedRectangle(oledArea.toFloat(), 3.0f);
        }

        g.setColour(juce::Colour(c.bodyDark));
        g.fillRect(0, topBarDrawH, toolbarRight, 65);

        // Panel dividers (stop at oak strip)
        g.setColour(juce::Colour(c.border));
        g.drawHorizontalLine(topBarDrawH, 0, static_cast<float>(toolbarRight));
        g.drawHorizontalLine(topBarDrawH + 65, 0, static_cast<float>(toolbarRight));
    }

    // OLED background behind track tools section in top bar (Track name -> MIX)
    if (!glassOverlay && trackNameLabel.isVisible() && mixerButton.isVisible())
    {
        auto trackOled = trackNameLabel.getBounds()
            .getUnion(midiLearnButton.getBounds())
            .getUnion(countInButton.getBounds())
            .getUnion(pianoToggleButton.getBounds())
            .getUnion(mixerButton.getBounds())
            .expanded(4, 3);
        g.setColour(juce::Colour(c.lcdBg));
        g.fillRoundedRectangle(trackOled.toFloat(), 4.0f);
        g.setColour(juce::Colour(c.border));
        g.drawRoundedRectangle(trackOled.toFloat(), 4.0f, 1.0f);
    }

    // Accent stripe at top — glass overlay replaces with water surface light
    if (glassOverlay)
    {
        // No accent stripe for glass themes — caustics replace it (or nothing when anim off)
        // On iOS, Metal handles caustics — skip CPU rendering
#if JUCE_IOS
        if (glassAnimEnabled && !metalRendererAttached)
#else
        if (glassAnimEnabled)
#endif
        {
            if (timelineComponent && timelineComponent->isVisible())
            {
                auto tlRect = timelineComponent->getBounds();

                g.saveState();
                g.excludeClipRegion(tlRect);
                drawWaterCaustics(g, getLocalBounds());
                g.restoreState();

                g.saveState();
                g.reduceClipRegion(tlRect);
                g.setOpacity(0.15f);
                drawWaterCaustics(g, getLocalBounds());
                g.setOpacity(1.0f);
                g.restoreState();
            }
            else
            {
                drawWaterCaustics(g, getLocalBounds());
            }
        }
    }
    else
    {
        // Accent stripe removed — clean top edge across all themes
    }

#if JUCE_IOS
    // Draw rectangle around track name label
    if (trackNameLabel.isVisible())
    {
        g.setColour(juce::Colour(c.textSecondary));
        g.drawRect(trackNameLabel.getBounds().expanded(4, 2), 1);
    }
#endif

    // Draw decorative side panels if the theme provides them (skip in fullscreen vis)
    if (!visualizerFullScreen)
    {
        if (auto* lnf = dynamic_cast<DawLookAndFeel*>(&getLookAndFeel()))
        {
            if (lnf->getSidePanelWidth() > 0)
            {
                lnf->drawSidePanels(g, getWidth(), getHeight());

#if JUCE_IOS
                if (!paintPhone)
#endif
                {
                    // Draw decorative strip to the left of the right panel
                    int sidePW = lnf->getSidePanelWidth();
                    int stripW = sidePW;
                    int rpW2 = (int)((float)getPanelWidth() * panelSlideProgress);
                    int stripX = getWidth() - sidePW - rpW2 - stripW;
                    lnf->drawInnerStrip(g, stripX, 0, stripW, getHeight());
                }
            }
        }
    }

    // Draw CPU/RAM heartbeat OLED
    if (cpuLabel.isVisible())
    {
        auto bounds = cpuLabel.getBounds().toFloat().reduced(0.5f);

        // OLED background
        g.setColour(juce::Colour(c.lcdBg));
        g.fillRoundedRectangle(bounds, 4.0f);
        g.setColour(juce::Colour(c.borderLight).withAlpha(0.3f));
        g.drawRoundedRectangle(bounds, 4.0f, 0.8f);

        auto inner = bounds.reduced(3.0f, 2.0f);

        // EKG sweep — reads from circular ekgBuffer, draws like a real monitor
        // Center the waveform in the top half with equal padding above and below text
        float waveH = inner.getHeight() * 0.35f;
        float baselineY = inner.getCentreY() - inner.getHeight() * 0.08f;
        float waveW = inner.getWidth();

        {
            juce::Colour waveCol = currentCpuPercent > 75.0f ? juce::Colour(c.red) :
                                   currentCpuPercent > 33.0f ? juce::Colour(c.amber) :
                                   juce::Colour(c.lcdText);

            // Draw the sweep trace from the circular buffer
            // The write position is the "cursor" — draw everything behind it,
            // leave a gap ahead of it (like a real cardiac monitor)
            int gapSize = 8;  // blank gap ahead of cursor

            juce::Path wavePath;
            bool pathStarted = false;

            for (int i = 0; i < EKG_BUFFER_SIZE; ++i)
            {
                // Read position relative to write cursor
                int bufIdx = (ekgWritePos + i) % EKG_BUFFER_SIZE;

                // Skip the gap zone right ahead of the cursor
                int distFromCursor = (EKG_BUFFER_SIZE - i) % EKG_BUFFER_SIZE;
                if (distFromCursor > 0 && distFromCursor <= gapSize)
                {
                    pathStarted = false;
                    continue;
                }

                float x = inner.getX() + (static_cast<float>(i) / static_cast<float>(EKG_BUFFER_SIZE - 1)) * waveW;
                float v = ekgBuffer[static_cast<size_t>(bufIdx)];
                float y = baselineY - v * waveH;

                // Fade out older samples
                float age = static_cast<float>(EKG_BUFFER_SIZE - distFromCursor) / static_cast<float>(EKG_BUFFER_SIZE);
                (void)age;  // used for future fade effect

                if (!pathStarted) { wavePath.startNewSubPath(x, y); pathStarted = true; }
                else wavePath.lineTo(x, y);
            }

            g.setColour(waveCol.withAlpha(0.85f));
            g.strokePath(wavePath, juce::PathStrokeType(1.3f));

            // Glow dot at the sweep cursor position
            int cursorIdx = (ekgWritePos - 1 + EKG_BUFFER_SIZE) % EKG_BUFFER_SIZE;
            float cursorX = inner.getX() + (static_cast<float>((cursorIdx - (ekgWritePos - EKG_BUFFER_SIZE + EKG_BUFFER_SIZE) % EKG_BUFFER_SIZE + EKG_BUFFER_SIZE) % EKG_BUFFER_SIZE) / static_cast<float>(EKG_BUFFER_SIZE - 1)) * waveW;
            float cursorV = ekgBuffer[static_cast<size_t>(cursorIdx)];
            float cursorY = baselineY - cursorV * waveH;
            g.setColour(waveCol.withAlpha(0.6f));
            g.fillEllipse(cursorX - 2.5f, cursorY - 2.5f, 5.0f, 5.0f);
        }

        // Text — bottom half: "CPU 12%  RAM 201MB"
        auto textArea = inner.removeFromBottom(inner.getHeight() * 0.5f);
        g.setColour(juce::Colour(c.lcdText).withAlpha(0.9f));
        g.setFont(juce::Font(themeManager.getLookAndFeel()->getUIFontName(), 14.0f, juce::Font::bold));
        g.drawText("CPU " + juce::String(static_cast<int>(currentCpuPercent)) + "%  RAM " +
                   juce::String(currentRamMB) + "MB",
                   textArea.toNearestInt(), juce::Justification::centred);
    }

    // Draw pulsing glow ring around active param knob
    if (activeParamIndex >= 0 && activeParamIndex < NUM_PARAM_SLIDERS
        && paramSliders[activeParamIndex]->isVisible())
    {
        auto knobBounds = paramSliders[activeParamIndex]->getBounds().toFloat();
        float cx = knobBounds.getCentreX();
        float cy = knobBounds.getCentreY();
        float r = juce::jmin(knobBounds.getWidth(), knobBounds.getHeight()) / 2.0f + 3.0f;

        auto glowColor = juce::Colour(c.lcdText).withAlpha(paramHighlightAlpha * 0.8f);
        g.setColour(glowColor);
        g.drawEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f, 2.0f);

        // Outer soft glow
        auto outerGlow = juce::Colour(c.lcdText).withAlpha(paramHighlightAlpha * 0.25f);
        g.setColour(outerGlow);
        g.drawEllipse(cx - r - 2, cy - r - 2, (r + 2) * 2.0f, (r + 2) * 2.0f, 3.0f);
    }
}

void MainComponent::paintOverChildren(juce::Graphics& g)
{
    // Wireframe themes (LK Dark) get a rounded OLED-cyan border
    // around the entire app window so the canvas reads as a single
    // self-contained "device screen".  Inset enough that the stroke
    // and the rounded corners stay fully on-screen — drawRounded
    // strokes outward from the path, so we need at least
    // ceil(strokeWidth/2)+1 pixels of breathing room.
    if (auto* lnf = dynamic_cast<DawLookAndFeel*>(&getLookAndFeel()))
    {
        const auto& th = lnf->getTheme();
        if (th.wireframe)
        {
            const float stroke = 2.0f;
            const float inset  = 4.0f;
            auto bounds = getLocalBounds().toFloat().reduced(inset);
            g.setColour(juce::Colour(th.borderLight));
            g.drawRoundedRectangle(bounds, 22.0f, stroke);
        }
    }

    // Get button corner radius from theme
    float radius = 0.0f;
    if (auto* lnf = dynamic_cast<DawLookAndFeel*>(&getLookAndFeel()))
        radius = lnf->getButtonRadius();

    // Glass overlay: thin separator lines only (no wash over controls — prevents washed-out look)
    if (themeManager.isGlassOverlay())
    {
#if JUCE_IOS
        bool isPhone = AUScanner::isIPhone() && !forceIPadLayout;
        int statusBarPad = isPhone ? 20 : 30;
        int topBarH = isPhone ? 80 : 70;
#else
        bool isPhone = false;
        int statusBarPad = 0;
        int topBarH = 80;
#endif
        if (!isPhone)
        {
            int topBarBottom = statusBarPad + topBarH;
            int toolbarBottom = topBarBottom + 65;
            int rpW = (int)((float)getPanelWidth() * panelSlideProgress);

            // ── Glass pane refractive edges ──
            float t3 = static_cast<float>(glassAnimTime);
            if (!glassAnimEnabled) t3 = 0.0f;  // freeze animation

            // Same wave calc as background caustics — with tilt offset
            float etx = smoothTiltX * 1.5f, ety = smoothTiltY * 1.5f;
            float ea1 = std::sin(t3 * 0.003f) * 1.5f + std::sin(t3 * 0.0017f + 1.0f) * 0.8f + std::sin(t3 * 0.0007f + 0.3f) * 2.0f + etx;
            float ea2 = std::sin(t3 * 0.0025f + 2.0f) * 1.3f + std::sin(t3 * 0.0013f + 3.0f) * 0.9f + std::sin(t3 * 0.0005f + 1.7f) * 1.8f + ety;
            float ea3 = std::sin(t3 * 0.002f + 4.5f) * 1.6f + std::sin(t3 * 0.0011f + 5.0f) * 0.7f + std::sin(t3 * 0.0004f + 4.0f) * 2.2f + (etx + ety) * 0.5f;
            float eco1 = std::cos(ea1), esi1 = std::sin(ea1);
            float eco2 = std::cos(ea2), esi2 = std::sin(ea2);
            float eco3 = std::cos(ea3), esi3 = std::sin(ea3);

            auto edgeLight = [&](float ex, float ey) -> float
            {
                float eMul = isHighTier() ? 18.0f : (deviceTier == AUScanner::DeviceTier::JamieEdition ? 12.0f : (isLowTier() ? 1.0f : 8.0f));
                float d1 = (ex * eco1 + ey * esi1) * 0.016f + t3 * 0.12f * eMul;
                float d2 = (ex * eco2 + ey * esi2) * 0.020f + t3 * 0.095f * eMul;
                float d3 = (ex * eco3 + ey * esi3) * 0.013f + t3 * 0.075f * eMul;
                float cv = (std::sin(d1) + std::sin(d2) + std::sin(d3)) / 3.0f;
                cv = cv * 0.5f + 0.5f;
                return cv * cv;
            };

            auto drawGlassPane = [&](juce::Rectangle<float> bounds, float cornerR)
            {
                float edgeW = 8.0f;
                float glowW = 20.0f;
                float cr = cornerR;

                // Clip to rounded rect so edge glow follows the corners
                g.saveState();
                juce::Path clipPath;
                clipPath.addRoundedRectangle(bounds.expanded(edgeW), cr + edgeW);
                g.reduceClipRegion(clipPath);

                // Top edge — skip corners
                for (float x = bounds.getX() + cr; x < bounds.getRight() - cr; x += 6.0f)
                {
                    float intensity = edgeLight(x, bounds.getY());
                    float a = intensity * 0.15f;
                    if (a < 0.01f) continue;
                    float hue = std::sin(x * 0.004f + t3 * 0.008f) * 0.5f + 0.5f;
                    juce::Colour c2 = juce::Colour(0xff60c8c0).interpolatedWith(juce::Colour(0xff4098d0), hue);
                    g.setColour(c2.withAlpha(a));
                    g.fillRect(x, bounds.getY() - 1.0f, 6.0f, edgeW);
                    juce::ColourGradient spread(c2.withAlpha(a * 0.6f), x, bounds.getY(),
                                                c2.withAlpha(0.0f), x, bounds.getY() + glowW, false);
                    g.setGradientFill(spread);
                    g.fillRect(x, bounds.getY(), 6.0f, glowW);
                }

                // Left edge — skip corners
                for (float y = bounds.getY() + cr; y < bounds.getBottom() - cr; y += 6.0f)
                {
                    float intensity = edgeLight(bounds.getX(), y);
                    float a = intensity * 0.12f;
                    if (a < 0.01f) continue;
                    float hue = std::sin(y * 0.005f + t3 * 0.007f) * 0.5f + 0.5f;
                    juce::Colour c2 = juce::Colour(0xff60c8c0).interpolatedWith(juce::Colour(0xff4098d0), hue);
                    g.setColour(c2.withAlpha(a));
                    g.fillRect(bounds.getX() - 1.0f, y, edgeW, 6.0f);
                    juce::ColourGradient spread(c2.withAlpha(a * 0.5f), bounds.getX(), y,
                                                c2.withAlpha(0.0f), bounds.getX() + glowW, y, false);
                    g.setGradientFill(spread);
                    g.fillRect(bounds.getX(), y, glowW, 6.0f);
                }

                // Bottom edge — skip corners
                for (float x = bounds.getX() + cr; x < bounds.getRight() - cr; x += 6.0f)
                {
                    float intensity = edgeLight(x, bounds.getBottom());
                    float a = intensity * 0.10f;
                    if (a < 0.01f) continue;
                    float hue = std::sin(x * 0.004f + t3 * 0.009f + 1.0f) * 0.5f + 0.5f;
                    juce::Colour c2 = juce::Colour(0xff60c8c0).interpolatedWith(juce::Colour(0xff4098d0), hue);
                    juce::ColourGradient spread(c2.withAlpha(a * 0.5f), x, bounds.getBottom(),
                                                c2.withAlpha(0.0f), x, bounds.getBottom() - glowW, false);
                    g.setGradientFill(spread);
                    g.fillRect(x, bounds.getBottom() - glowW, 6.0f, glowW);
                }

                // Right edge — skip corners
                for (float y = bounds.getY() + cr; y < bounds.getBottom() - cr; y += 6.0f)
                {
                    float intensity = edgeLight(bounds.getRight(), y);
                    float a = intensity * 0.10f;
                    if (a < 0.01f) continue;
                    float hue = std::sin(y * 0.005f + t3 * 0.006f + 2.0f) * 0.5f + 0.5f;
                    juce::Colour c2 = juce::Colour(0xff60c8c0).interpolatedWith(juce::Colour(0xff4098d0), hue);
                    juce::ColourGradient spread(c2.withAlpha(a * 0.5f), bounds.getRight(), y,
                                                c2.withAlpha(0.0f), bounds.getRight() - glowW, y, false);
                    g.setGradientFill(spread);
                    g.fillRect(bounds.getRight() - glowW, y, glowW, 6.0f);
                }

                g.restoreState();

                // Glass edge outline
                g.setColour(juce::Colours::white.withAlpha(0.06f));
                g.drawRoundedRectangle(bounds, cornerR, 0.5f);
            };

            // Arranger glass pane
            if (timelineComponent && timelineComponent->isVisible())
                drawGlassPane(timelineComponent->getBounds().toFloat(), 12.0f);

            // Right panel glass pane (skip in fullscreen visualizer)
            if (rpW > 0 && !visualizerFullScreen)
            {
                int panelSlideOff2 = (int)((1.0f - panelSlideProgress) * (float)getPanelWidth());
                int panelX = getWidth() - getPanelWidth() + panelSlideOff2;
                drawGlassPane(juce::Rectangle<float>((float)panelX, 0, (float)getPanelWidth(), (float)getHeight()), 12.0f);
            }
        } // end !isPhone

        // ── These effects apply on both iPhone and iPad ──
        // On iOS with Metal, ripples and button glow are GPU-rendered
#if JUCE_IOS
        if (glassAnimEnabled && !metalRendererAttached)
#else
        if (glassAnimEnabled)
#endif
        {
            // Draw ripple effects
            drawRipples(g);

            // Caustic light dancing over buttons
            float t2 = static_cast<float>(glassAnimTime);

            auto lightOnButton = [&](juce::Component& btn)
            {
                if (!btn.isVisible()) return;
                auto b = btn.getBounds().toFloat();
                float bx = b.getCentreX();
                float by = b.getCentreY();

                float btx = smoothTiltX * 1.5f, bty = smoothTiltY * 1.5f;
                float a1 = std::sin(t2 * 0.003f) * 1.5f + std::sin(t2 * 0.0017f + 1.0f) * 0.8f + std::sin(t2 * 0.0007f + 0.3f) * 2.0f + btx;
                float a2 = std::sin(t2 * 0.0025f + 2.0f) * 1.3f + std::sin(t2 * 0.0013f + 3.0f) * 0.9f + std::sin(t2 * 0.0005f + 1.7f) * 1.8f + bty;
                float a3 = std::sin(t2 * 0.002f + 4.5f) * 1.6f + std::sin(t2 * 0.0011f + 5.0f) * 0.7f + std::sin(t2 * 0.0004f + 4.0f) * 2.2f + (btx + bty) * 0.5f;
                float bSpeedMul = isHighTier() ? 18.0f : (deviceTier == AUScanner::DeviceTier::JamieEdition ? 12.0f : (isLowTier() ? 1.0f : 8.0f));
                float sp1 = 0.12f * bSpeedMul;
                float sp2 = 0.095f * bSpeedMul;
                float sp3 = 0.075f * bSpeedMul;
                float s1 = std::sin((bx * std::cos(a1) + by * std::sin(a1)) * 0.016f + t2 * sp1);
                float s2 = std::sin((bx * std::cos(a2) + by * std::sin(a2)) * 0.020f + t2 * sp2);
                float s3 = std::sin((bx * std::cos(a3) + by * std::sin(a3)) * 0.013f + t2 * sp3);
                float caustic = (s1 + s2 + s3) / 3.0f * 0.5f + 0.5f;
                caustic = caustic * caustic;

                bool lightBtn = (themeManager.getCurrentTheme() == ThemeManager::LiquidGlassLight);
                float alpha = caustic * (lightBtn ? 0.22f : 0.10f);
                if (alpha < 0.01f) return;

                float hueB = std::sin(bx * 0.003f + by * 0.004f + t2 * 0.01f) * 0.5f + 0.5f;
                juce::Colour btnCol = lightBtn
                    ? juce::Colour(0xff2090d0).interpolatedWith(juce::Colour(0xff1060a0), hueB)
                    : juce::Colour(0xff60c8c0).interpolatedWith(juce::Colour(0xff4098d0), hueB);

                juce::ColourGradient topLight(
                    btnCol.withAlpha(alpha), b.getCentreX(), b.getY(),
                    btnCol.withAlpha(alpha * 0.2f), b.getCentreX(), b.getBottom(), false);
                g.setGradientFill(topLight);
                g.fillRoundedRectangle(b, 12.0f);
            };

            for (int ci = 0; ci < getNumChildComponents(); ++ci)
            {
                auto* child = getChildComponent(ci);
                if (child != nullptr && child->isVisible()
                    && dynamic_cast<juce::Button*>(child) != nullptr)
                    lightOnButton(*child);
            }
        }
    }

    // Draw green border on play button (flashes when playing)
    if (playButton.isVisible())
    {
        auto btnBounds = playButton.getBounds().toFloat().expanded(0.5f);
        g.setColour(juce::Colours::green.withAlpha(playHighlightAlpha * 0.9f));
        g.drawRoundedRectangle(btnBounds, radius, 2.0f);
    }

    // Draw red border on record button (flashes when recording)
    if (recordButton.isVisible())
    {
        auto btnBounds = recordButton.getBounds().toFloat().expanded(0.5f);
        g.setColour(juce::Colours::red.withAlpha(recHighlightAlpha * 0.9f));
        g.drawRoundedRectangle(btnBounds, radius, 2.0f);
    }

    // Draw blue border on loop button (flashes when loop is on)
    if (loopButton.isVisible())
    {
        auto btnBounds = loopButton.getBounds().toFloat().expanded(0.5f);
        g.setColour(juce::Colour(0xff4488cc).withAlpha(loopHighlightAlpha * 0.9f));
        g.drawRoundedRectangle(btnBounds, radius, 2.0f);
    }

}

void MainComponent::resized()
{
    auto area = getLocalBounds();

    // Pin the LK inspector pill to the bottom-left corner so it
    // doesn't fight the top-bar transport/BPM/tap-tempo widgets.
    // Inspector overlay sits just above the pill when visible.
    {
        const int pillW = 32, pillH = 24, gap = 6;
        launchkeyInspectorToggle.setBounds(gap, getHeight() - pillH - gap, pillW, pillH);
        const int insW = juce::jmin(460, getWidth() - 2 * gap);
        const int insH = 280;
        launchkeyMidiInspector.setBounds(gap, getHeight() - pillH - gap - insH - 4, insW, insH);
    }

#if JUCE_IOS
    attachMetalRendererIfNeeded();
    if (metalRenderer && metalRendererAttached)
        metalRenderer->setBounds(0, 0, getWidth(), getHeight());

    bool isPhone = AUScanner::isIPhone() && !forceIPadLayout;
#else
    bool isPhone = false;
#endif

    // Inset for decorative side panels (skip on iPhone)
    if (!isPhone)
    {
        if (auto* lnf = dynamic_cast<DawLookAndFeel*>(&getLookAndFeel()))
        {
            int sidePW = lnf->getSidePanelWidth();
            if (sidePW > 0)
            {
                area.removeFromLeft(sidePW);
                area.removeFromRight(sidePW);
            }
        }
    }

#if JUCE_IOS
    int statusBarPad = isPhone ? 20 : 30;
    area.removeFromTop(statusBarPad);
    int topBarH = isPhone ? 80 : 70;
#else
    int topBarH = 80;
#endif
    int bottomBarH = isPhone ? 0 : 45;
    int rightPanelW = isPhone ? 0 : (int)((float)getPanelWidth() * panelSlideProgress);

    // ── Top Bar ──
    auto topBar = area.removeFromTop(topBarH).reduced(4, isPhone ? 2 : 10);
    // Trim top bar — use visible panel width so top bar expands with the arranger
    if (!isPhone)
    {
        int oakTrim = 0;
        if (auto* lnf = dynamic_cast<DawLookAndFeel*>(&getLookAndFeel()))
            if (lnf->getSidePanelWidth() > 0)
                oakTrim = lnf->getSidePanelWidth();
        int topBarTrim = rightPanelW + oakTrim;
        if (topBarTrim > 0)
            topBar.removeFromRight(topBarTrim);
    }

#if JUCE_IOS
    zoomOutButton.setVisible(false);
    zoomInButton.setVisible(false);

    if (isPhone)
    {
        // ── iPhone two-row layout ──
        // Split into top row (transport + utils) and bottom row (clip tools + controls)
        int rowH = topBar.getHeight() / 2;
        auto row1 = topBar.removeFromTop(rowH);
        auto row2 = topBar;
        int bw = 38;
        int gap = 2;

        // ── Row 1: Transport, BPM, utilities ──
        // Right side first so we know remaining width
        phoneMenuButton.setBounds(row1.removeFromRight(34));
        phoneMenuButton.setVisible(true);
        row1.removeFromRight(gap);
        bpmLabel.setBounds(row1.removeFromRight(58));
        row1.removeFromRight(gap);

        // Calculate button width to fill remaining space evenly
        // 10 buttons + scroll arrows in row 1
        int r1w = row1.getWidth();
        int numR1Btns = 12;  // REC, <<, PLAY, >>, STOP, LOOP, MET, PANIC, C-In, Tap, Learn, Audio
        int r1bw = (r1w - (numR1Btns - 1) * gap) / numR1Btns;

        stopButton.setBounds(row1.removeFromLeft(r1bw));
        row1.removeFromLeft(gap);
        scrollLeftButton.setBounds(row1.removeFromLeft(r1bw));
        row1.removeFromLeft(gap);
        playButton.setBounds(row1.removeFromLeft(r1bw));
        row1.removeFromLeft(gap);
        scrollRightButton.setBounds(row1.removeFromLeft(r1bw));
        row1.removeFromLeft(gap);
        recordButton.setBounds(row1.removeFromLeft(r1bw));
        row1.removeFromLeft(gap);
        loopButton.setBounds(row1.removeFromLeft(r1bw));
        row1.removeFromLeft(gap);
        metronomeButton.setBounds(row1.removeFromLeft(r1bw));
        row1.removeFromLeft(gap);
        panicButton.setBounds(row1.removeFromLeft(r1bw));
        row1.removeFromLeft(gap);
        countInButton.setVisible(true);
        countInButton.setButtonText("Count-In");
        countInButton.setBounds(row1.removeFromLeft(r1bw));
        row1.removeFromLeft(gap);
        tapTempoButton.setVisible(true);
        tapTempoButton.setButtonText("TAP");
        tapTempoButton.setBounds(row1.removeFromLeft(r1bw));
        row1.removeFromLeft(gap);
        midiLearnButton.setVisible(true);
        midiLearnButton.setButtonText("LEARN");
        midiLearnButton.setBounds(row1.removeFromLeft(r1bw));
        row1.removeFromLeft(gap);
        audioSettingsButton.setVisible(false);

        settingsButton.setVisible(false);

        // ── Row 2: Clip tools, save/load, vis/piano/mixer ──
        // Right side first
        fullscreenButton.setVisible(false);
        if (themeManager.isGlassOverlay())
        {
            glassAnimButton.setBounds(row2.removeFromRight(30));
            glassAnimButton.setVisible(true);
            row2.removeFromRight(gap);
        }
        pianoToggleButton.setBounds(row2.removeFromRight(42));
        row2.removeFromRight(gap);
        mixerButton.setBounds(row2.removeFromRight(36));
        row2.removeFromRight(gap);

        // Session view button
        sessionViewButton.setBounds(row2.removeFromRight(52));
        sessionViewButton.setVisible(true);
        row2.removeFromRight(gap);

        // Arp buttons at right side of row 2
        arpOctButton.setBounds(row2.removeFromRight(42));
        arpOctButton.setVisible(true);
        row2.removeFromRight(gap);
        arpRateButton.setBounds(row2.removeFromRight(38));
        arpRateButton.setVisible(true);
        row2.removeFromRight(gap);
        arpModeButton.setBounds(row2.removeFromRight(38));
        arpModeButton.setVisible(true);
        row2.removeFromRight(gap);
        arpButton.setBounds(row2.removeFromRight(38));
        arpButton.setVisible(true);
        row2.removeFromRight(gap);

        // Fill remaining with evenly-spaced buttons
        int r2w = row2.getWidth();
        int numR2Btns = 10;  // Save, Load, Undo, Redo, New, Delete, Dupe, Split, Edit, Quant
        int r2bw = (r2w - (numR2Btns - 1) * gap) / numR2Btns;

        saveButton.setVisible(true);
        saveButton.setButtonText("Save");
        saveButton.setBounds(row2.removeFromLeft(r2bw));
        row2.removeFromLeft(gap);
        loadButton.setVisible(true);
        loadButton.setButtonText("Load");
        loadButton.setBounds(row2.removeFromLeft(r2bw));
        row2.removeFromLeft(gap);
        undoButton.setVisible(true);
        undoButton.setButtonText("Undo");
        undoButton.setBounds(row2.removeFromLeft(r2bw));
        row2.removeFromLeft(gap);
        redoButton.setVisible(true);
        redoButton.setButtonText("Redo");
        redoButton.setBounds(row2.removeFromLeft(r2bw));
        row2.removeFromLeft(gap);
        newClipButton.setVisible(true);
        newClipButton.setButtonText("New");
        newClipButton.setBounds(row2.removeFromLeft(r2bw));
        row2.removeFromLeft(gap);
        deleteClipButton.setVisible(true);
        deleteClipButton.setButtonText("Delete");
        deleteClipButton.setBounds(row2.removeFromLeft(r2bw));
        row2.removeFromLeft(gap);
        duplicateClipButton.setVisible(false);
        duplicateClipButton.setButtonText("Dupe");
        duplicateClipButton.setBounds(row2.removeFromLeft(r2bw));
        row2.removeFromLeft(gap);
        splitClipButton.setVisible(true);
        splitClipButton.setButtonText("Split");
        splitClipButton.setBounds(row2.removeFromLeft(r2bw));
        row2.removeFromLeft(gap);
        editClipButton.setVisible(true);
        editClipButton.setButtonText("Edit");
        editClipButton.setBounds(row2.removeFromLeft(r2bw));
        row2.removeFromLeft(gap);
        quantizeButton.setVisible(true);
        quantizeButton.setButtonText("Quant");
        quantizeButton.setBounds(row2.removeFromLeft(row2.getWidth()));  // take the rest

        // Hide BPM +/- buttons on iPhone (use label tap instead)
        bpmDownButton.setVisible(false);
        bpmUpButton.setVisible(false);
        beatLabel.setVisible(false);

        // Hide items that don't fit on iPhone
        trackNameLabel.setVisible(false);
        statusLabel.setVisible(false);
        themeSelector.setVisible(false);
        projectorButton.setVisible(false);
        if (!themeManager.isGlassOverlay())
            glassAnimButton.setVisible(false);
    }
    else
    {
        // ── iPad layout ──
        phoneMenuButton.setVisible(false);
        beatLabel.setVisible(false);  // moved to OLED panel in right panel

        // Right side first: BPM, track tools
        // Show "PHONE" button to switch back if on an iPhone using iPad layout
        if (AUScanner::isIPhone())
        {
            settingsButton.setButtonText("PHN");
            settingsButton.setBounds(topBar.removeFromRight(36));
            settingsButton.setVisible(true);
            settingsButton.onClick = [this] {
                forceIPadLayout = false;
                resized();
                repaint();
            };
            topBar.removeFromRight(4);
        }

        // Left side: BPM (draggable) + tap tempo
        bpmDownButton.setVisible(false);
        bpmUpButton.setVisible(false);
        bpmLabel.setBounds(topBar.removeFromLeft(110));
        topBar.removeFromLeft(3);
        tapTempoButton.setBounds(topBar.removeFromLeft(40));
        topBar.removeFromLeft(12);

        // Right side: track tools
        mixerButton.setBounds(topBar.removeFromRight(38));
        topBar.removeFromRight(3);
        pianoToggleButton.setBounds(topBar.removeFromRight(45));
        topBar.removeFromRight(3);
        countInButton.setBounds(topBar.removeFromRight(70));
        countInButton.setVisible(true);
        topBar.removeFromRight(3);
        midiLearnButton.setBounds(topBar.removeFromRight(55));
        midiLearnButton.setVisible(true);

        // Transport buttons from left
        scrollLeftButton.setVisible(false);
        scrollRightButton.setVisible(false);

        stopButton.setBounds(topBar.removeFromLeft(50));
        topBar.removeFromLeft(3);
        playButton.setBounds(topBar.removeFromLeft(55));
        topBar.removeFromLeft(3);
        recordButton.setBounds(topBar.removeFromLeft(55));
        topBar.removeFromLeft(3);
        loopButton.setBounds(topBar.removeFromLeft(50));
        topBar.removeFromLeft(16);
        metronomeButton.setBounds(topBar.removeFromLeft(45));
        topBar.removeFromLeft(3);
        panicButton.setBounds(topBar.removeFromLeft(55));
        topBar.removeFromLeft(3);
        sessionViewButton.setBounds(topBar.removeFromLeft(55));
        sessionViewButton.setVisible(true);
        topBar.removeFromLeft(3);
        if (themeManager.isGlassOverlay())
        {
            glassAnimButton.setBounds(topBar.removeFromLeft(35));
            glassAnimButton.setVisible(true);
            topBar.removeFromLeft(3);
        }
        else
        {
            glassAnimButton.setVisible(false);
        }

        // OLED info display — centered in remaining space between panic and learn
        {
            auto oledOuter = topBar.reduced(6, 0);
            auto infoArea = oledOuter.reduced(3, 2);
            int rowH = infoArea.getHeight() / 2;
            statusLabel.setBounds(infoArea.removeFromTop(rowH));
            statusLabel.setVisible(true);
            beatLabel.setVisible(false);
            chordLabel.setBounds(infoArea);
            chordLabel.setVisible(true);
        }
    }
#else
    phoneMenuButton.setVisible(false);
    midiLearnButton.setBounds(topBar.removeFromLeft(65));
    topBar.removeFromLeft(4);
    trackNameLabel.setBounds(topBar.removeFromLeft(180));
    topBar.removeFromLeft(6);

    stopButton.setBounds(topBar.removeFromLeft(60));
    topBar.removeFromLeft(3);
    playButton.setBounds(topBar.removeFromLeft(65));
    topBar.removeFromLeft(3);
    recordButton.setBounds(topBar.removeFromLeft(65));
    topBar.removeFromLeft(3);
    metronomeButton.setBounds(topBar.removeFromLeft(50));
    topBar.removeFromLeft(3);
    countInButton.setBounds(topBar.removeFromLeft(80));
    topBar.removeFromLeft(3);
    loopButton.setBounds(topBar.removeFromLeft(60));
    topBar.removeFromLeft(3);
    panicButton.setBounds(topBar.removeFromLeft(65));
    topBar.removeFromLeft(3);
    pianoToggleButton.setBounds(topBar.removeFromLeft(50));
    topBar.removeFromLeft(3);
    mixerButton.setBounds(topBar.removeFromLeft(42));
    topBar.removeFromLeft(4);
    scrollLeftButton.setVisible(false);
    scrollRightButton.setVisible(false);
    zoomOutButton.setBounds(topBar.removeFromLeft(50));
    topBar.removeFromLeft(2);
    zoomInButton.setBounds(topBar.removeFromLeft(50));
    topBar.removeFromLeft(4);
    statusLabel.setBounds(topBar.removeFromLeft(juce::jmin(180, topBar.getWidth() / 2)));
    topBar.removeFromLeft(4);
    sessionViewButton.setBounds(topBar.removeFromLeft(60));
    sessionViewButton.setVisible(true);
    topBar.removeFromLeft(4);
    beatLabel.setBounds(topBar.removeFromRight(100));
#endif

    // ── Fullscreen Visualizer Mode ──
    if (visualizerFullScreen)
    {
        // Hide all top bar controls
        playButton.setVisible(false);
        stopButton.setVisible(false);
        recordButton.setVisible(false);
        metronomeButton.setVisible(false);
        loopButton.setVisible(false);
        bpmDownButton.setVisible(false);
        bpmLabel.setVisible(false);
        bpmUpButton.setVisible(false);
        tapTempoButton.setVisible(false);
        beatLabel.setVisible(false);
        statusLabel.setVisible(false);
        chordLabel.setVisible(false);
        trackNameLabel.setVisible(false);
        midiLearnButton.setVisible(false);
        mixerButton.setVisible(false);
        pianoToggleButton.setVisible(false);
        countInButton.setVisible(false);
        settingsButton.setVisible(false);
        scrollLeftButton.setVisible(false);
        scrollRightButton.setVisible(false);
        zoomInButton.setVisible(false);
        zoomOutButton.setVisible(false);
        panicButton.setVisible(false);
        phoneMenuButton.setVisible(false);
        panelToggleButton.setVisible(false);
        clearAutoButton.setVisible(false);
        presetSelector.setVisible(false);
        presetPrevButton.setVisible(false);
        presetNextButton.setVisible(false);
        presetUpButton.setVisible(false);
        presetDownButton.setVisible(false);
        paramPageLeft.setVisible(false);
        paramPageRight.setVisible(false);
        paramPageLabel.setVisible(false);
        paramPageNameLabel.setVisible(false);
        trackInputSelector.setVisible(false);
        trackInfoLabel.setVisible(false);
        testNoteButton.setVisible(false);
        fullscreenButton.setVisible(false);
        pianoOctUpButton.setVisible(false);
        pianoOctDownButton.setVisible(false);
        touchPiano.setVisible(false);
        arpButton.setVisible(false);
        arpModeButton.setVisible(false);
        arpRateButton.setVisible(false);
        arpOctButton.setVisible(false);
        if (mixerComponent) mixerComponent->setVisible(false);

        {
            // Fullscreen with control bar (always shown)
            auto fsArea = getLocalBounds();
#if JUCE_IOS
            fsArea.removeFromTop(50); // iOS status bar safe area
#endif
            auto controlBar = fsArea.removeFromTop(36).reduced(4, 2);
            visExitButton.setBounds(controlBar.removeFromLeft(55));
            visExitButton.setVisible(true);
            controlBar.removeFromLeft(6);
            visSelector.setBounds(controlBar.removeFromLeft(90));
            visSelector.setVisible(true);
            controlBar.removeFromLeft(6);
            projectorButton.setBounds(controlBar.removeFromLeft(50));
            projectorButton.setVisible(true);

            // Visualizer controls in fullscreen control bar
            controlBar.removeFromLeft(10);
            if (currentVisMode == 0) // Spectrum
            {
                specDecayBtn.setBounds(controlBar.removeFromLeft(50));
                controlBar.removeFromLeft(3);
                specSensDownBtn.setBounds(controlBar.removeFromLeft(30));
                controlBar.removeFromLeft(2);
                specSensUpBtn.setBounds(controlBar.removeFromLeft(30));
            }
            else if (currentVisMode == 1) // Lissajous
            {
                lissZoomOutBtn.setBounds(controlBar.removeFromLeft(30));
                controlBar.removeFromLeft(2);
                lissZoomInBtn.setBounds(controlBar.removeFromLeft(30));
                controlBar.removeFromLeft(3);
                lissDotsBtn.setBounds(controlBar.removeFromLeft(50));
            }
            else if (currentVisMode == 2) // G-Force
            {
                gfRibbonDownBtn.setBounds(controlBar.removeFromLeft(30));
                controlBar.removeFromLeft(2);
                gfRibbonUpBtn.setBounds(controlBar.removeFromLeft(30));
                controlBar.removeFromLeft(3);
                gfTrailBtn.setBounds(controlBar.removeFromLeft(50));
                controlBar.removeFromLeft(3);
                gfSpeedSelector.setBounds(controlBar.removeFromLeft(60));
            }
            else if (currentVisMode == 3) // Geiss
            {
                geissWaveBtn.setBounds(controlBar.removeFromLeft(50));
                controlBar.removeFromLeft(3);
                geissPaletteBtn.setBounds(controlBar.removeFromLeft(50));
                controlBar.removeFromLeft(3);
                geissSceneBtn.setBounds(controlBar.removeFromLeft(55));
                controlBar.removeFromLeft(3);
                geissWaveDownBtn.setBounds(controlBar.removeFromLeft(30));
                controlBar.removeFromLeft(2);
                geissWaveUpBtn.setBounds(controlBar.removeFromLeft(30));
                controlBar.removeFromLeft(3);
                geissWarpLockBtn.setBounds(controlBar.removeFromLeft(50));
                controlBar.removeFromLeft(3);
                geissPalLockBtn.setBounds(controlBar.removeFromLeft(50));
                controlBar.removeFromLeft(3);
                geissSpeedSelector.setBounds(controlBar.removeFromLeft(60));
                controlBar.removeFromLeft(3);
                geissAutoPilotBtn.setBounds(controlBar.removeFromLeft(50));
                controlBar.removeFromLeft(3);
                geissBgBtn.setBounds(controlBar.removeFromLeft(30));
            }
            else if (currentVisMode == 4)
            {
                pmPrevBtn.setBounds(controlBar.removeFromLeft(45));
                controlBar.removeFromLeft(3);
                pmNextBtn.setBounds(controlBar.removeFromLeft(45));
                controlBar.removeFromLeft(3);
                pmRandBtn.setBounds(controlBar.removeFromLeft(45));
                controlBar.removeFromLeft(3);
                pmLockBtn.setBounds(controlBar.removeFromLeft(45));
                controlBar.removeFromLeft(3);
                pmBgBtn.setBounds(controlBar.removeFromLeft(30));
            }
            else if (currentVisMode == 8) // FluidSim
            {
                fluidColorBtn.setBounds(controlBar.removeFromLeft(50));
                controlBar.removeFromLeft(3);
                fluidViscDownBtn.setBounds(controlBar.removeFromLeft(30));
                controlBar.removeFromLeft(2);
                fluidViscUpBtn.setBounds(controlBar.removeFromLeft(30));
                controlBar.removeFromLeft(3);
                fluidVortBtn.setBounds(controlBar.removeFromLeft(50));
            }
            else if (currentVisMode == 9) // RayMarch
            {
                rmPrevBtn.setBounds(controlBar.removeFromLeft(45));
                controlBar.removeFromLeft(3);
                rmNextBtn.setBounds(controlBar.removeFromLeft(45));
            }
            setVisControlsVisible();

            auto visArea = fsArea.reduced(2, 2);

            spectrumDisplay.setVisible(false);
            lissajousDisplay.setVisible(false);
            waveTerrainDisplay.setVisible(false);
            geissDisplay.setVisible(false);
            projectMDisplay.setVisible(false);
            shaderToyDisplay.setVisible(false);
            analyzerDisplay.setVisible(false);
            heartbeatDisplay->setVisible(false);
            bioResonanceDisplay->setVisible(false);
            fluidSimDisplay.setVisible(false);
            rayMarchDisplay.setVisible(false);

            if (currentVisMode == 0) { spectrumDisplay.setBounds(visArea); spectrumDisplay.setAlpha(1.0f); spectrumDisplay.setVisible(true); }
            else if (currentVisMode == 1) { lissajousDisplay.setBounds(visArea); lissajousDisplay.setVisible(true); }
            else if (currentVisMode == 2) { waveTerrainDisplay.setBounds(visArea); waveTerrainDisplay.setVisible(true); }
            else if (currentVisMode == 3) { geissDisplay.setBounds(visArea); geissDisplay.setVisible(true); }
            else if (currentVisMode == 4) { projectMDisplay.setBounds(visArea); projectMDisplay.setVisible(true); }
            else if (currentVisMode == 5) { analyzerDisplay.setBounds(visArea); analyzerDisplay.setVisible(true); }
            else if (currentVisMode == 6) { heartbeatDisplay->setBounds(visArea); heartbeatDisplay->setVisible(true); }
            else if (currentVisMode == 7) { bioResonanceDisplay->setBounds(visArea); bioResonanceDisplay->setVisible(true); }
            else if (currentVisMode == 8) { fluidSimDisplay.setBounds(visArea); fluidSimDisplay.setVisible(true); }
            else if (currentVisMode == 9) { rayMarchDisplay.setBounds(visArea); rayMarchDisplay.setVisible(true); }

            // Bring control bar widgets to front so they're not hidden behind the visualizer
            visExitButton.toFront(false);
            visSelector.toFront(false);
            projectorButton.toFront(false);
            if (currentVisMode == 0) { specDecayBtn.toFront(false); specSensDownBtn.toFront(false); specSensUpBtn.toFront(false); }
            else if (currentVisMode == 1) { lissZoomOutBtn.toFront(false); lissZoomInBtn.toFront(false); lissDotsBtn.toFront(false); }
            else if (currentVisMode == 2) { gfRibbonDownBtn.toFront(false); gfRibbonUpBtn.toFront(false); gfTrailBtn.toFront(false); gfSpeedSelector.toFront(false); }
            else if (currentVisMode == 3) { geissWaveBtn.toFront(false); geissPaletteBtn.toFront(false); geissSceneBtn.toFront(false); geissWaveDownBtn.toFront(false); geissWaveUpBtn.toFront(false); geissWarpLockBtn.toFront(false); geissPalLockBtn.toFront(false); geissSpeedSelector.toFront(false); geissAutoPilotBtn.toFront(false); geissBgBtn.toFront(false); }
            else if (currentVisMode == 4) { pmPrevBtn.toFront(false); pmNextBtn.toFront(false); pmRandBtn.toFront(false); pmLockBtn.toFront(false); pmBgBtn.toFront(false); }
            else if (currentVisMode == 8) { fluidColorBtn.toFront(false); fluidViscDownBtn.toFront(false); fluidViscUpBtn.toFront(false); fluidVortBtn.toFront(false); }
            else if (currentVisMode == 9) { rmPrevBtn.toFront(false); rmNextBtn.toFront(false); }
        }

        // Hide everything else
        newClipButton.setVisible(false);
        deleteClipButton.setVisible(false);
        duplicateClipButton.setVisible(false);
        splitClipButton.setVisible(false);
        editClipButton.setVisible(false);
        quantizeButton.setVisible(false);
        gridSelector.setVisible(false);
        saveButton.setVisible(false);
        loadButton.setVisible(false);
        undoButton.setVisible(false);
        redoButton.setVisible(false);
        themeSelector.setVisible(false);
        audioSettingsButton.setVisible(false);
        midi2Button.setVisible(false);
        pluginSelector.setVisible(false);
        openEditorButton.setVisible(false);
        midiInputSelector.setVisible(false);
        midiRefreshButton.setVisible(false);
        for (int i = 0; i < NUM_FX_SLOTS; ++i)
        {
            fxSelectors[i]->setVisible(false);
            fxEditorButtons[i]->setVisible(false);
        }
        for (int i = 0; i < NUM_PARAM_SLIDERS; ++i)
        {
            paramSliders[i]->setVisible(false);
            paramLabels[i]->setVisible(false);
        }
        volumeSlider.setVisible(false);
        volumeLabel.setVisible(false);
        panSlider.setVisible(false);
        panLabel.setVisible(false);
        if (timelineComponent) timelineComponent->setVisible(false);
        if (arrangerMinimap) arrangerMinimap->setVisible(false);
        return;
    }

    // ── Restore visibility when not in vis mode ──
    {
    // Restore top bar controls
    playButton.setVisible(true);
    stopButton.setVisible(true);
    recordButton.setVisible(true);
    metronomeButton.setVisible(true);
    loopButton.setVisible(true);
    bpmDownButton.setVisible(false);
    bpmLabel.setVisible(true);
    bpmUpButton.setVisible(false);
    tapTempoButton.setVisible(true);
    statusLabel.setVisible(true);
    chordLabel.setVisible(true);
    midiLearnButton.setVisible(true);
    mixerButton.setVisible(true);
    pianoToggleButton.setVisible(true);
    countInButton.setVisible(true);
    scrollLeftButton.setVisible(false);
    scrollRightButton.setVisible(false);
    zoomInButton.setVisible(false);  // not in iPad layout
    zoomOutButton.setVisible(false);  // not in iPad layout
    panicButton.setVisible(true);
    clearAutoButton.setVisible(true);
    fullscreenButton.setVisible(false);
    trackInputSelector.setVisible(true);
    presetSelector.setVisible(true);
    presetPrevButton.setVisible(true);
    presetNextButton.setVisible(true);
    presetUpButton.setVisible(true);
    presetDownButton.setVisible(true);
    paramPageLeft.setVisible(true);
    paramPageRight.setVisible(true);
    paramPageLabel.setVisible(true);
    paramPageNameLabel.setVisible(true);
    trackInfoLabel.setVisible(true);
    testNoteButton.setVisible(false);  // not in iPad layout
    newClipButton.setVisible(true);
    deleteClipButton.setVisible(true);
    duplicateClipButton.setVisible(false);
    splitClipButton.setVisible(true);
    editClipButton.setVisible(true);
    quantizeButton.setVisible(true);
    gridButton.setVisible(true);
    saveButton.setVisible(true);
    loadButton.setVisible(true);
    undoButton.setVisible(true);
    redoButton.setVisible(true);
    themeSelector.setVisible(true);
    audioSettingsButton.setVisible(false);
    midi2Button.setVisible(true);
    pluginSelector.setVisible(true);
    openEditorButton.setVisible(true);
    midiInputSelector.setVisible(true);
    midiRefreshButton.setVisible(true);
    for (int i = 0; i < NUM_FX_SLOTS; ++i)
    {
        fxSelectors[i]->setVisible(true);
        fxEditorButtons[i]->setVisible(true);
    }
    for (int i = 0; i < NUM_PARAM_SLIDERS; ++i)
    {
        paramSliders[i]->setVisible(true);
        paramLabels[i]->setVisible(true);
    }
    volumeSlider.setVisible(true);
    volumeLabel.setVisible(true);
    panSlider.setVisible(true);
    panLabel.setVisible(true);
    if (timelineComponent) timelineComponent->setVisible(true);
    spectrumDisplay.setVisible(currentVisMode == 0);
    lissajousDisplay.setVisible(currentVisMode == 1);
    waveTerrainDisplay.setVisible(currentVisMode == 2);
    geissDisplay.setVisible(currentVisMode == 3);
    projectMDisplay.setVisible(currentVisMode == 4);
    shaderToyDisplay.setVisible(false);
    analyzerDisplay.setVisible(currentVisMode == 5);
    heartbeatDisplay->setVisible(currentVisMode == 6);
    bioResonanceDisplay->setVisible(currentVisMode == 7);
    fluidSimDisplay.setVisible(currentVisMode == 8);
    rayMarchDisplay.setVisible(currentVisMode == 9);
    visExitButton.setVisible(false);
    projectorButton.setVisible(false);
    fullscreenButton.setVisible(false);
    midi2Button.setVisible(true);

    // Hide ALL fullscreen vis controls when not in fullscreen
    specDecayBtn.setVisible(false);
    specSensUpBtn.setVisible(false);
    specSensDownBtn.setVisible(false);
    lissZoomInBtn.setVisible(false);
    lissZoomOutBtn.setVisible(false);
    lissDotsBtn.setVisible(false);
    gfRibbonUpBtn.setVisible(false);
    gfRibbonDownBtn.setVisible(false);
    gfTrailBtn.setVisible(false);
    gfSpeedSelector.setVisible(false);
    geissWaveBtn.setVisible(false);
    geissPaletteBtn.setVisible(false);
    geissSceneBtn.setVisible(false);
    geissWaveUpBtn.setVisible(false);
    geissWaveDownBtn.setVisible(false);
    geissWarpLockBtn.setVisible(false);
    geissPalLockBtn.setVisible(false);
    geissSpeedSelector.setVisible(false);
    geissAutoPilotBtn.setVisible(false);
    geissBgBtn.setVisible(false);
    pmNextBtn.setVisible(false);
    pmPrevBtn.setVisible(false);
    pmRandBtn.setVisible(false);
    pmLockBtn.setVisible(false);
    pmBgBtn.setVisible(false);
    fluidColorBtn.setVisible(false);
    fluidViscUpBtn.setVisible(false);
    fluidViscDownBtn.setVisible(false);
    fluidVortBtn.setVisible(false);
    rmPrevBtn.setVisible(false);
    rmNextBtn.setVisible(false);

    // Reset vis selector alpha (may have been 1.0 in fullscreen)
    visSelector.setAlpha(1.0f);

    // Force all visualizer components behind other UI
    spectrumDisplay.toBack();
    lissajousDisplay.toBack();
    waveTerrainDisplay.toBack();
    geissDisplay.toBack();
    projectMDisplay.toBack();
    analyzerDisplay.toBack();
    heartbeatDisplay->toBack();
    bioResonanceDisplay->toBack();
    fluidSimDisplay.toBack();
    rayMarchDisplay.toBack();

    } // end restore visibility block

#if JUCE_IOS
    if (isPhone)
    {
        // iPhone: hide items not shown in top bar or bottom bar
        gridSelector.setVisible(false);
        openEditorButton.setVisible(false);
        midiInputSelector.setVisible(false);
        midiRefreshButton.setVisible(false);
        midi2Button.setVisible(false);
        visSelector.setVisible(false);
        // fullscreenButton already positioned in top bar for iPhone
        projectorButton.setVisible(false);
        for (int i = 0; i < NUM_FX_SLOTS; ++i)
        {
            if (i > 0) fxSelectors[i]->setVisible(false);  // slot 0 shown in bottom bar
            fxEditorButtons[i]->setVisible(false);
        }
        for (int i = 0; i < NUM_PARAM_SLIDERS; ++i)
        {
            paramSliders[i]->setVisible(false);
            paramLabels[i]->setVisible(false);
        }
        spectrumDisplay.setVisible(false);
        lissajousDisplay.setVisible(false);
        waveTerrainDisplay.setVisible(false);
        geissDisplay.setVisible(false);
        projectMDisplay.setVisible(false);
            shaderToyDisplay.setVisible(false);
            analyzerDisplay.setVisible(false);
            heartbeatDisplay->setVisible(false);
            bioResonanceDisplay->setVisible(false);
            fluidSimDisplay.setVisible(false);
            rayMarchDisplay.setVisible(false);
        chordLabel.setVisible(false);

        // ── Right panel: volume knob on top (full width), then two columns ──
        int fsRightW = rightPanelVisible ? 100 : 0;
        auto rightPanel = area.removeFromRight(fsRightW).reduced(2, 2);

        int knobH = 44;
        int labelH = 12;
        int paramGap = 2;

        // Volume — full panel width, large knob
        volumeSlider.setVisible(true);
        volumeLabel.setVisible(true);
        volumeSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        volumeLabel.setBounds(rightPanel.removeFromTop(labelH));
        auto volArea = rightPanel.removeFromTop(getPanelWidth() > 200 ? 110 : 80);
        int volSz = juce::jmin(volArea.getWidth(), volArea.getHeight());
        volumeSlider.setBounds(volArea.withSizeKeepingCentre(volSz, volSz));
        rightPanel.removeFromTop(paramGap);

        // Split remaining into two columns
        auto knobCol1 = rightPanel.removeFromLeft(rightPanel.getWidth() / 2 - 1);
        rightPanel.removeFromLeft(2);
        auto knobCol2 = rightPanel;

        // Pan — smaller, in col1
        panSlider.setVisible(true);
        panLabel.setVisible(true);
        panSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        panLabel.setBounds(knobCol1.removeFromTop(labelH));
        panSlider.setBounds(knobCol1.removeFromTop(knobH));
        knobCol1.removeFromTop(paramGap);

        // Fill rest of col1 with odd param knobs (0, 2, 4, 6, 8)
        for (int i = 0; i < NUM_PARAM_SLIDERS; i += 2)
        {
            if (knobCol1.getHeight() >= knobH + labelH)
            {
                paramLabels[i]->setVisible(true);
                paramLabels[i]->setBounds(knobCol1.removeFromTop(labelH));
                paramSliders[i]->setVisible(true);
                paramSliders[i]->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
                paramSliders[i]->setBounds(knobCol1.removeFromTop(knobH));
                knobCol1.removeFromTop(paramGap);
            }
            else
            {
                paramSliders[i]->setVisible(false);
                paramLabels[i]->setVisible(false);
            }
        }

        // Column 2: even param knobs (1, 3, 5, 7)
        for (int i = 1; i < NUM_PARAM_SLIDERS; i += 2)
        {
            if (knobCol2.getHeight() >= knobH + labelH)
            {
                paramLabels[i]->setVisible(true);
                paramLabels[i]->setBounds(knobCol2.removeFromTop(labelH));
                paramSliders[i]->setVisible(true);
                paramSliders[i]->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
                paramSliders[i]->setBounds(knobCol2.removeFromTop(knobH));
                knobCol2.removeFromTop(paramGap);
            }
            else
            {
                paramSliders[i]->setVisible(false);
                paramLabels[i]->setVisible(false);
            }
        }

        // ── Bottom bar: track name | plugin selector | FX selector ──
        auto bottomBar = area.removeFromBottom(36).reduced(2, 2);

        trackNameLabel.setVisible(true);
        trackNameLabel.setText("T" + juce::String(selectedTrackIndex + 1), juce::dontSendNotification);
        trackNameLabel.setBounds(bottomBar.removeFromLeft(28));
        bottomBar.removeFromLeft(2);

        pluginSelector.setBounds(bottomBar.removeFromLeft(bottomBar.getWidth() / 2 - 2));
        bottomBar.removeFromLeft(4);

        fxSelectors[0]->setVisible(true);
        fxSelectors[0]->setBounds(bottomBar);

        // Touch piano
        if (touchPianoVisible)
        {
            auto pianoArea = area.removeFromBottom(140);
            touchPiano.setBounds(pianoArea);
        }

        // Arranger fills the rest
        area.reduce(2, 2);
        if (mixerVisible)
        {
            mixerComponent->setBounds(area);
            mixerComponent->setVisible(true);
            if (timelineComponent) timelineComponent->setVisible(false);
        }
        else
        {
            mixerComponent->setVisible(false);
            if (arrangerMinimap)
                arrangerMinimap->setVisible(false);
#if JUCE_IOS
            area.removeFromBottom(20); // home indicator safe area
#endif
            if (timelineComponent)
            {
                timelineComponent->setVisibleTracks(4);
                if (showingSessionView)
                {
                    timelineComponent->setVisible(false);
                    if (sessionViewComponent)
                    {
                        sessionViewComponent->setVisible(true);
                        sessionViewComponent->setBounds(area);
                    }
                }
                else
                {
                    if (sessionViewComponent)
                        sessionViewComponent->setVisible(false);
                    timelineComponent->setVisible(true);
                    timelineComponent->setBounds(area);
                }
            }
        }
        return;
    }
#endif
    // Reset to default for iPad/desktop
    if (timelineComponent)
        timelineComponent->setVisibleTracks(isHighTier() ? 12 : 8);

    // Hide right panel components only when fully off-screen
    bool panelComponentsVisible = !isPhone && panelSlideProgress > 0.02f;

    // ── Right Panel — starts from top of screen (not below top bar) ──
    int oakStripW = 0;
    if (auto* lnf = dynamic_cast<DawLookAndFeel*>(&getLookAndFeel()))
    {
        if (lnf->getSidePanelWidth() > 0)
            oakStripW = lnf->getSidePanelWidth();
    }
    // Build right panel from full bounds so it extends to the top
    auto fullArea = getLocalBounds();
#if JUCE_IOS
    fullArea.removeFromTop(30); // status bar
#endif
    if (auto* lnf = dynamic_cast<DawLookAndFeel*>(&getLookAndFeel()))
    {
        int sidePW = lnf->getSidePanelWidth();
        if (sidePW > 0)
            fullArea.removeFromRight(sidePW);
    }
    // Panel slides off-screen to the right at full 180px width
    // fullArea carves the panel from the right edge, then we translate it off-screen
    int fullPanelW = getPanelWidth();
    int slideOffset = (int)((1.0f - panelSlideProgress) * fullPanelW);

    // Build panel rect from the RIGHT edge of the screen (not from area)
    // This way area doesn't lose the full 180px — only rightPanelW (the visible part)
    auto panelBase = fullArea.withLeft(fullArea.getRight() - fullPanelW);
    auto rightPanel = panelBase.reduced(8, 4).translated(slideOffset, 0);

    // Arranger area only yields the visible portion
    area.removeFromRight(rightPanelW);
    area.removeFromRight(oakStripW);

    // ── Panel Toggle Button — at right edge of arranger area (after panel removed) ──
    if (!isPhone)
    {
        int toggleW = 28;
        int toggleH = 70;
        int togglePad = rightPanelW > 0 ? 4 : 0; // padding when panel is open to avoid overlap
        int toggleX = area.getRight() - toggleW - togglePad;
        int toggleY = area.getY() + (area.getHeight() / 2) - (toggleH / 2);
        panelToggleButton.setBounds(toggleX, toggleY, toggleW, toggleH);
        panelToggleButton.setVisible(true);
        panelToggleButton.toFront(false);
    }
    else
    {
        panelToggleButton.setVisible(false);
    }

    // ── Edit Toolbar ──
    // All buttons positioned L→R in the brightness gradient that
    // matches the LaunchkeyDark theme + the device pad layout:
    //   CPU meter | Arp(faint) → Timing(dim) → Project(mid) → Clip(accent) → I/O(high)
    auto toolbar = area.removeFromTop(65).reduced(4, 4);

    // CPU/RAM EKG meter — anchored at the far left.
    {
        const int cpuW = 150;
        auto cpuArea = toolbar.removeFromLeft(cpuW);
        cpuLabel.setBounds(cpuArea);
        cpuLabel.setVisible(true);
        toolbar.removeFromLeft(18);
    }

    // Arp group (faintest)
    arpButton.setBounds(toolbar.removeFromLeft(48));
    arpButton.setVisible(true);
    toolbar.removeFromLeft(2);
    arpModeButton.setBounds(toolbar.removeFromLeft(42));
    arpModeButton.setVisible(true);
    toolbar.removeFromLeft(2);
    arpRateButton.setBounds(toolbar.removeFromLeft(38));
    arpRateButton.setVisible(true);
    toolbar.removeFromLeft(2);
    arpOctButton.setBounds(toolbar.removeFromLeft(42));
    arpOctButton.setVisible(true);
    toolbar.removeFromLeft(18);

    // Timing group
    gridButton.setBounds(toolbar.removeFromLeft(65));
    gridButton.setVisible(true);
    toolbar.removeFromLeft(2);
    quantizeButton.setBounds(toolbar.removeFromLeft(70));
    toolbar.removeFromLeft(18);

    // Project I/O group — Save + Load
    saveButton.setBounds(toolbar.removeFromLeft(45));
    toolbar.removeFromLeft(2);
    loadButton.setBounds(toolbar.removeFromLeft(45));
    toolbar.removeFromLeft(18);

    // History group — Undo + Redo
    undoButton.setBounds(toolbar.removeFromLeft(42));
    toolbar.removeFromLeft(2);
    redoButton.setBounds(toolbar.removeFromLeft(42));
    toolbar.removeFromLeft(18);

    // Clip group
    newClipButton.setBounds(toolbar.removeFromLeft(80));
    toolbar.removeFromLeft(2);
    deleteClipButton.setBounds(toolbar.removeFromLeft(60));
    toolbar.removeFromLeft(2);
    splitClipButton.setBounds(toolbar.removeFromLeft(50));
    toolbar.removeFromLeft(2);
    editClipButton.setBounds(toolbar.removeFromLeft(60));
    toolbar.removeFromLeft(18);
    // clearAutoButton lives in the PianoRollWindow now — keep
    // hidden + skip layout so it doesn't reserve toolbar space.

    // I/O group (brightest)
    testNoteButton.setVisible(false);
    captureButton.setBounds(toolbar.removeFromLeft(50));
    captureButton.setVisible(true);
    toolbar.removeFromLeft(2);
    exportButton.setBounds(toolbar.removeFromLeft(55));
    exportButton.setVisible(true);
    loopSetButton.setVisible(false);

    // OLED-color cycler — only visible in the LK OLED theme.
    const bool oledTheme = themeManager.getCurrentTheme() == ThemeManager::LaunchkeyOled;
    if (oledTheme)
    {
        toolbar.removeFromLeft(18);
        oledColorButton.setBounds(toolbar.removeFromLeft(70));
        if (auto* lf = dynamic_cast<LaunchkeyOledLookAndFeel*>(themeManager.getLookAndFeel()))
            oledColorButton.setButtonText(lf->getOledColourName());
        oledColorButton.setVisible(true);
    }
    else
    {
        oledColorButton.setVisible(false);
    }
    // CPU label is positioned at the far left now (see top of layout).

    // Pack remaining controls at the right end of the toolbar
#if !JUCE_IOS
    tapTempoButton.setBounds(toolbar.removeFromRight(50));
    toolbar.removeFromRight(3);
    bpmLabel.setBounds(toolbar.removeFromRight(100));
    toolbar.removeFromRight(6);
    bpmDownButton.setVisible(false);
    bpmUpButton.setVisible(false);
#endif
    if (panelComponentsVisible)
    {
        // visSelector positioned in right panel when panel is visible
        visSelector.setVisible(true);
    }
    else
    {
        // Move visSelector into toolbar when panel is hidden
        visSelector.setBounds(toolbar.removeFromRight(72));
        visSelector.setVisible(true);
        toolbar.removeFromRight(2);
    }
    projectorButton.setVisible(false);
    fullscreenButton.setVisible(false);
    audioSettingsButton.setVisible(false);

    // Track input selector + M2 button on same row
    trackNameLabel.setVisible(false);

    // When panel is sliding closed, hide all panel components
    if (!panelComponentsVisible)
    {
        trackInputSelector.setVisible(false);
        midi2Button.setVisible(false);
        themeSelector.setVisible(false);
        pluginSelector.setVisible(false);
        openEditorButton.setVisible(false);
        midiInputSelector.setVisible(false);
        midiRefreshButton.setVisible(false);
        for (int i = 0; i < fxSelectors.size(); ++i)
        {
            fxSelectors[i]->setVisible(false);
            fxEditorButtons[i]->setVisible(false);
        }
        paramPageLeft.setVisible(false);
        paramPageRight.setVisible(false);
        paramPageLabel.setVisible(false);
        paramPageNameLabel.setVisible(false);
        for (int i = 0; i < paramSliders.size(); ++i)
        {
            paramSliders[i]->setVisible(false);
            paramLabels[i]->setVisible(false);
        }
        presetSelector.setVisible(false);
        presetUpButton.setVisible(false);
        presetDownButton.setVisible(false);
        presetPrevButton.setVisible(false);
        presetNextButton.setVisible(false);
        volumeSlider.setVisible(false);
        volumeLabel.setVisible(false);
        panSlider.setVisible(false);
        panLabel.setVisible(false);
        visSelector.setVisible(false);
        // Skip the rest of right panel layout
    }
    else
    {
        // Restore visibility for panel components — layout code below
        // will fine-tune based on track type (audio vs MIDI)
        trackInputSelector.setVisible(true);
        midi2Button.setVisible(true);
        themeSelector.setVisible(true);
        volumeSlider.setVisible(true);
        paramPageLeft.setVisible(true);
        paramPageRight.setVisible(true);
        paramPageLabel.setVisible(true);
        paramPageNameLabel.setVisible(true);
        presetSelector.setVisible(true);
        presetUpButton.setVisible(true);
        presetDownButton.setVisible(true);
        for (int i = 0; i < paramSliders.size(); ++i)
        {
            paramSliders[i]->setVisible(true);
            paramLabels[i]->setVisible(true);
        }
        for (int i = 0; i < fxSelectors.size(); ++i)
        {
            fxSelectors[i]->setVisible(true);
            fxEditorButtons[i]->setVisible(true);
        }
    }

    auto inputRow = rightPanel.removeFromTop(panelComponentsVisible ? 30 : 0);
    midi2Button.setBounds(inputRow.removeFromRight(32));
    inputRow.removeFromRight(4);
    trackInputSelector.setBounds(inputRow);
    rightPanel.removeFromTop(8);
    // Theme selector
    themeSelector.setBounds(rightPanel.removeFromTop(30));
    rightPanel.removeFromTop(8);

    // Visualizer display — compact on Jamie Edition to save space for volume knob
    bool jamieLayout = (deviceTier == AUScanner::DeviceTier::JamieEdition);
    auto visPanelArea = rightPanel.removeFromTop(jamieLayout ? 55 : 70);
    if (currentVisMode == 0)  // Spectrum
    {
        spectrumDisplay.setBounds(visPanelArea);
        spectrumDisplay.setAlpha(1.0f);
        spectrumDisplay.setVisible(true);
    }
    else if (currentVisMode == 1)  // Lissajous
    {
        lissajousDisplay.setBounds(visPanelArea);
        lissajousDisplay.setVisible(true);
    }
    else if (currentVisMode == 2)  // G-Force
    {
        waveTerrainDisplay.setBounds(visPanelArea);
        waveTerrainDisplay.setVisible(true);
    }
    else if (currentVisMode == 3)  // Geiss
    {
        geissDisplay.setBounds(visPanelArea);
        geissDisplay.setVisible(true);
    }
    else if (currentVisMode == 4)  // MilkDrop
    {
        projectMDisplay.setBounds(visPanelArea);
        projectMDisplay.setVisible(true);
    }
    else if (currentVisMode == 5)  // Analyzer
    {
        analyzerDisplay.setBounds(visPanelArea);
        analyzerDisplay.setVisible(true);
    }
    else if (currentVisMode == 6)  // Heartbeat
    {
        heartbeatDisplay->setBounds(visPanelArea);
        heartbeatDisplay->setVisible(true);
    }
    else if (currentVisMode == 7)  // BioSync
    {
        bioResonanceDisplay->setBounds(visPanelArea);
        bioResonanceDisplay->setVisible(true);
    }
    else if (currentVisMode == 8)  // Fluid
    {
        fluidSimDisplay.setBounds(visPanelArea);
        fluidSimDisplay.setVisible(true);
    }
    else if (currentVisMode == 9)  // RayMarch
    {
        rayMarchDisplay.setBounds(visPanelArea);
        rayMarchDisplay.setVisible(true);
    }
    rightPanel.removeFromTop(4);
    // Vis selector overlaid on bottom of visualizer preview
    {
        int selH = 20;
        visSelector.setBounds(visPanelArea.getX(), visPanelArea.getBottom() - selH,
                              visPanelArea.getWidth(), selH);
        visSelector.setVisible(true);
        visSelector.setAlpha(0.6f);
        visSelector.toFront(false);
    }

    // Vis controls only show in fullscreen mode — force all hidden in normal view
    // (setVisControlsVisible is called inside the fullscreen block instead)

    {
        auto& currentTrack = pluginHost.getTrack(selectedTrackIndex);
        bool isAudioTrack = (currentTrack.type == TrackType::Audio);

        int fxRowH = jamieLayout ? 24 : 30;
        int fxGap = jamieLayout ? 1 : 2;

        if (isAudioTrack)
        {
            // Audio track: FX selector + preset + no MIDI/plugin selector
            pluginSelector.setVisible(false);
            openEditorButton.setVisible(false);
            midiInputSelector.setVisible(false);
            midiRefreshButton.setVisible(false);

            // FX slots with preset browser for first FX
            for (int i = 0; i < NUM_FX_SLOTS; ++i)
            {
                auto fxRow = rightPanel.removeFromTop(fxRowH);
                fxEditorButtons[i]->setBounds(fxRow.removeFromRight(jamieLayout ? 26 : 32));
                fxRow.removeFromRight(2);
                fxSelectors[i]->setBounds(fxRow);
                rightPanel.removeFromTop(fxGap);
            }
            rightPanel.removeFromTop(fxGap);

            // Preset row moved below param knobs
            rightPanel.removeFromTop(jamieLayout ? 1 : 3);
        }
        else
        {
            // MIDI track: plugin selector + preset + MIDI input + FX
            pluginSelector.setVisible(true);
            openEditorButton.setVisible(true);
            midiInputSelector.setVisible(false);  // moved to OLED input selector
            midiRefreshButton.setVisible(false);

            {
                auto pluginRow = rightPanel.removeFromTop(jamieLayout ? 26 : 32);
                openEditorButton.setBounds(pluginRow.removeFromRight(jamieLayout ? 26 : 32));
                openEditorButton.setButtonText("E");
                pluginRow.removeFromRight(2);
                pluginSelector.setBounds(pluginRow);
            }
            rightPanel.removeFromTop(fxGap);
            // Preset row moved below param knobs
            rightPanel.removeFromTop(jamieLayout ? 1 : 3);

            // FX slots
            for (int i = 0; i < NUM_FX_SLOTS; ++i)
            {
                auto fxRow = rightPanel.removeFromTop(fxRowH);
                fxEditorButtons[i]->setBounds(fxRow.removeFromRight(jamieLayout ? 26 : 32));
                fxRow.removeFromRight(2);
                fxSelectors[i]->setBounds(fxRow);
                rightPanel.removeFromTop(fxGap);
            }
            rightPanel.removeFromTop(fxGap);
        }
    }

    // Param page navigation — full param name + page number on right
    {
        auto pageRow = rightPanel.removeFromTop(30);
        paramPageLeft.setBounds(pageRow.removeFromLeft(22));
        pageRow.removeFromLeft(2);
        paramPageRight.setBounds(pageRow.removeFromRight(22));
        pageRow.removeFromRight(2);
        // Page number on the right
        paramPageLabel.setBounds(pageRow.removeFromRight(32));
        paramPageLabel.setJustificationType(juce::Justification::centredRight);
        // Param name on the left
        paramPageNameLabel.setBounds(pageRow);
        rightPanel.removeFromTop(2);
    }

    // Plugin parameter knobs — paged grid
    if (paramSliders.size() > 0)
    {
        bool widePanel = getPanelWidth() > 200;
        int knobGap = widePanel ? 12 : 4;
        int knobSize = juce::jmin(widePanel ? 60 : 44, (rightPanel.getWidth() - knobGap * 2) / 3);
        int labelH = widePanel ? 20 : 18;
        int rowGap = widePanel ? 8 : 2;

        if (jamieLayout)
        {
            // Jamie Edition: 4-column grid, fit as many rows as we can
            // while reserving space for preset row + volume knob below
            int jamieCols = 4;
            int minVolSpace = 90;
            int presetAreaH = 24 + 4 + 34 + 4;
            int availForKnobs = rightPanel.getHeight() - presetAreaH - minVolSpace - 8;

            knobGap = 4;
            knobSize = juce::jmin(44, (rightPanel.getWidth() - knobGap * (jamieCols - 1)) / jamieCols);
            labelH = 14;
            rowGap = 4;
            int knobRowH = knobSize + labelH + rowGap;
            int maxRows = juce::jmax(1, availForKnobs / knobRowH);
            const int activeCount = activeParamCount();
            int numRows = juce::jmin((activeCount + jamieCols - 1) / jamieCols, maxRows);
            int knobAreaH = numRows * knobRowH + 4;
            auto knobArea = rightPanel.removeFromTop(knobAreaH);
            rightPanel.removeFromTop(2);

            int gridW = jamieCols * knobSize + (jamieCols - 1) * knobGap;
            int gridOffsetX = (knobArea.getWidth() - gridW) / 2;

            int visibleCount = juce::jmin(activeCount, numRows * jamieCols);
            for (int i = 0; i < NUM_PARAM_SLIDERS; ++i)
            {
                if (i < visibleCount)
                {
                    int col = i % jamieCols;
                    int row = i / jamieCols;
                    int kx = knobArea.getX() + gridOffsetX + col * (knobSize + knobGap);
                    int ky = knobArea.getY() + row * knobRowH;
                    paramLabels[i]->setBounds(kx, ky, knobSize, labelH);
                    paramLabels[i]->setFont(juce::Font(10.0f));
                    paramLabels[i]->setVisible(true);
                    paramSliders[i]->setBounds(kx, ky + labelH, knobSize, knobSize);
                    paramSliders[i]->setVisible(true);
                }
                else
                {
                    paramSliders[i]->setVisible(false);
                    paramLabels[i]->setVisible(false);
                }
            }
        }
        else
        {
        const int activeCount = activeParamCount();
        int numRows = (activeCount + 2) / 3;
        int knobRowH = knobSize + labelH + rowGap;
        int knobAreaH = numRows * knobRowH + 4;
        auto knobArea = rightPanel.removeFromTop(knobAreaH);
        rightPanel.removeFromTop(4);

        int gridW = 3 * knobSize + 2 * knobGap;
        int gridOffsetX = (knobArea.getWidth() - gridW) / 2;

        for (int i = 0; i < NUM_PARAM_SLIDERS; ++i)
        {
            if (i >= activeCount)
            {
                paramSliders[i]->setVisible(false);
                paramLabels[i]->setVisible(false);
                continue;
            }
            paramSliders[i]->setVisible(true);
            paramLabels[i]->setVisible(true);
            int col = i % 3;
            int row = i / 3;
            int kx = knobArea.getX() + gridOffsetX + col * (knobSize + knobGap);
            int ky = knobArea.getY() + row * knobRowH;

            paramLabels[i]->setBounds(kx, ky, knobSize, labelH);
            paramLabels[i]->setFont(juce::Font(11.0f));
            paramSliders[i]->setBounds(kx, ky + labelH, knobSize, knobSize);
        }
        } // end else (non-Jamie layout)
    }

    // (FX slots moved above param knobs)

    // Spectrum ghosted behind volume/pan area (only when not the active visualizer)
    if (currentVisMode != 0)
    {
        spectrumDisplay.setBounds(rightPanel);
        spectrumDisplay.setAlpha(0.3f);
        spectrumDisplay.setVisible(true);
        spectrumDisplay.toBack();
    }
    else
    {
        spectrumDisplay.setAlpha(1.0f);
    }

    // Preset dropdown + big up/down buttons
    presetPrevButton.setVisible(false);
    presetNextButton.setVisible(false);
    presetSelector.setBounds(rightPanel.removeFromTop(jamieLayout ? 24 : 28));
    rightPanel.removeFromTop(jamieLayout ? 4 : 12);
    {
        auto btnArea = rightPanel.removeFromTop(jamieLayout ? 34 : 44);
        int btnH = btnArea.getHeight();
        int btnW = (btnArea.getWidth() - 8) / 2; // two wide pills with gap
        int gap = 8;
        int leftPad = (btnArea.getWidth() - btnW * 2 - gap) / 2;
        presetDownButton.setBounds(btnArea.getX() + leftPad, btnArea.getY(), btnW, btnH);
        presetUpButton.setBounds(btnArea.getX() + leftPad + btnW + gap, btnArea.getY(), btnW, btnH);
    }
    rightPanel.removeFromTop(4);

    // Volume knob — full remaining space, no label or text
    {
        auto mixArea = rightPanel;
        panSlider.setVisible(false);
        panLabel.setVisible(false);
        volumeLabel.setVisible(false);
        volumeSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        volumeSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        int maxVolSz = getPanelWidth() > 200 ? 140 : 110;
        int volSz = juce::jmax(60, juce::jmin(mixArea.getWidth(), mixArea.getHeight(), maxVolSz));
        volumeSlider.setBounds(mixArea.withSizeKeepingCentre(volSz, volSz));
    }

    // ── Touch Piano (bottom of main area, when visible) ──
    if (touchPianoVisible)
    {
        auto pianoBar = area.removeFromBottom(4);  // small gap
        (void)pianoBar;
        auto pianoControlRow = area.removeFromBottom(28);
        pianoOctDownButton.setBounds(pianoControlRow.removeFromLeft(45));
        pianoControlRow.removeFromLeft(3);
        pianoOctUpButton.setBounds(pianoControlRow.removeFromLeft(45));
        pianoOctDownButton.setVisible(true);
        pianoOctUpButton.setVisible(true);

        auto pianoArea = area.removeFromBottom(160);
        touchPiano.setBounds(pianoArea);
    }
    else
    {
        pianoOctDownButton.setVisible(false);
        pianoOctUpButton.setVisible(false);
    }

    // ── Timeline / Mixer fills the rest ──
    area.reduce(2, 2);
    if (mixerVisible)
    {
        mixerComponent->setBounds(area);
        mixerComponent->setVisible(true);
        if (timelineComponent) timelineComponent->setVisible(false);
    }
    else
    {
        mixerComponent->setVisible(false);
        if (timelineComponent)
        {
            if (arrangerMinimap)
                arrangerMinimap->setVisible(false);
#if JUCE_IOS
            area.removeFromBottom(20); // home indicator safe area
#endif
            if (showingSessionView)
            {
                timelineComponent->setVisible(false);
                if (sessionViewComponent)
                {
                    sessionViewComponent->setVisible(true);
                    sessionViewComponent->setBounds(area);
                }
            }
            else
            {
                if (sessionViewComponent)
                    sessionViewComponent->setVisible(false);
                timelineComponent->setVisible(true);
                timelineComponent->setBounds(area);
            }
        }
    }

    // Hide minimap when mixer is visible or timeline is hidden
    if (mixerVisible && arrangerMinimap)
        arrangerMinimap->setVisible(false);
}

// ── Keyboard ─────────────────────────────────────────────────────────────────

int MainComponent::keyToNote(int keyCode) const
{
    switch (keyCode)
    {
        case 'A': return 0;  case 'W': return 1;  case 'S': return 2;  case 'E': return 3;
        case 'D': return 4;  case 'F': return 5;  case 'T': return 6;  case 'G': return 7;
        case 'Y': return 8;  case 'H': return 9;  case 'U': return 10; case 'J': return 11;
        case 'K': return 12; case 'O': return 13; case 'L': return 14; case 'P': return 15;
        default: return -1;
    }
}

void MainComponent::sendNoteOn(int note)
{
    auto msg = juce::MidiMessage::noteOn(1, note, 0.8f);
    msg.setTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001);
    pluginHost.getMidiCollector().addMessageToQueue(msg);
    chordDetector.noteOn(note);

    // Buffer for capture ring (touch piano + computer keyboard)
    if (!pluginHost.getEngine().isRecording())
    {
        CaptureEvent evt;
        evt.absTime = juce::Time::getMillisecondCounterHiRes() * 0.001;
        evt.type = CaptureEvent::NoteOn;
        evt.channel = 1;
        evt.data1 = static_cast<uint8_t>(note);
        evt.data2 = 100;
        evt.pitchBend = 0;
        int pos = captureWritePos.load(std::memory_order_relaxed);
        captureRing[static_cast<size_t>(pos)] = evt;
        captureWritePos.store((pos + 1) % CAPTURE_RING_SIZE, std::memory_order_release);
        int cnt = captureCount.load(std::memory_order_relaxed);
        if (cnt < CAPTURE_RING_SIZE)
            captureCount.store(cnt + 1, std::memory_order_relaxed);
    }
}

void MainComponent::sendNoteOff(int note)
{
    auto msg = juce::MidiMessage::noteOff(1, note);
    msg.setTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001);
    pluginHost.getMidiCollector().addMessageToQueue(msg);
    chordDetector.noteOff(note);

    // Buffer for capture ring
    if (!pluginHost.getEngine().isRecording())
    {
        CaptureEvent evt;
        evt.absTime = juce::Time::getMillisecondCounterHiRes() * 0.001;
        evt.type = CaptureEvent::NoteOff;
        evt.channel = 1;
        evt.data1 = static_cast<uint8_t>(note);
        evt.data2 = 0;
        evt.pitchBend = 0;
        int pos = captureWritePos.load(std::memory_order_relaxed);
        captureRing[static_cast<size_t>(pos)] = evt;
        captureWritePos.store((pos + 1) % CAPTURE_RING_SIZE, std::memory_order_release);
        int cnt = captureCount.load(std::memory_order_relaxed);
        if (cnt < CAPTURE_RING_SIZE)
            captureCount.store(cnt + 1, std::memory_order_relaxed);
    }
}

bool MainComponent::keyPressed(const juce::KeyPress& key)
{
    // ESC = exit fullscreen visualizer
    if (key == juce::KeyPress::escapeKey && visualizerFullScreen)
    {
        visualizerFullScreen = false;
        projectorMode = false;
        fullscreenButton.setToggleState(false, juce::dontSendNotification);
        projectorButton.setToggleState(false, juce::dontSendNotification);
        startTimerHz(themeManager.isGlassOverlay() ? getGlassTimerHz() : getBaseTimerHz());
        resized();
        repaint();
        grabKeyboardFocus();
        return true;
    }

    // Spacebar = play/stop
    if (key == juce::KeyPress::spaceKey)
    {
        auto& eng = pluginHost.getEngine();
        if (eng.isPlaying())
        {
            eng.stop();
            for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
            {
                auto* cp = pluginHost.getTrack(t).clipPlayer;
                if (cp) cp->sendAllNotesOff.store(true);
            }
        }
        else
        {
            double timeSinceLastStop = juce::Time::getMillisecondCounterHiRes() - lastSpaceStopTime;
            if (timeSinceLastStop < 400.0)
            {
                eng.resetPosition();
                for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
                {
                    auto* cp = pluginHost.getTrack(t).clipPlayer;
                    if (cp) cp->stopAllSlots();
                }
                if (timelineComponent) timelineComponent->repaint();
            }
            else
            {
                eng.play();
            }
        }
        lastSpaceStopTime = eng.isPlaying() ? 0.0 : juce::Time::getMillisecondCounterHiRes();
        return true;
    }

    // Arrow keys = switch tracks
    if (key == juce::KeyPress::leftKey) { selectTrack(selectedTrackIndex - 1); return true; }
    if (key == juce::KeyPress::rightKey) { selectTrack(selectedTrackIndex + 1); return true; }

    if (!useComputerKeyboard) return false;

    int keyCode = key.getTextCharacter();
    if (keyCode >= 'a' && keyCode <= 'z') keyCode -= 32;

    if (keyCode == 'Z') {
        // Send note-offs at old octave for all held keys
        for (int k : keysCurrentlyDown) { int s = keyToNote(k); if (s >= 0) { int n = (computerKeyboardOctave * 12) + s; if (n >= 0 && n <= 127) sendNoteOff(n); } }
        computerKeyboardOctave = juce::jmax(0, computerKeyboardOctave - 1);
        // Send note-ons at new octave for all still-held keys
        for (int k : keysCurrentlyDown) { int s = keyToNote(k); if (s >= 0) { int n = (computerKeyboardOctave * 12) + s; if (n >= 0 && n <= 127) sendNoteOn(n); } }
        updateStatusLabel(); return true;
    }
    if (keyCode == 'X') {
        // Send note-offs at old octave for all held keys
        for (int k : keysCurrentlyDown) { int s = keyToNote(k); if (s >= 0) { int n = (computerKeyboardOctave * 12) + s; if (n >= 0 && n <= 127) sendNoteOff(n); } }
        computerKeyboardOctave = juce::jmin(8, computerKeyboardOctave + 1);
        // Send note-ons at new octave for all still-held keys
        for (int k : keysCurrentlyDown) { int s = keyToNote(k); if (s >= 0) { int n = (computerKeyboardOctave * 12) + s; if (n >= 0 && n <= 127) sendNoteOn(n); } }
        updateStatusLabel(); return true;
    }

    return false;
}

bool MainComponent::keyStateChanged(bool /*isKeyDown*/)
{
    if (!useComputerKeyboard) return false;

    const int mappedKeys[] = { 'A','W','S','E','D','F','T','G','Y','H','U','J','K','O','L','P' };

    for (int keyCode : mappedKeys)
    {
        bool isDown = juce::KeyPress::isKeyCurrentlyDown(keyCode);
        int semitone = keyToNote(keyCode);
        if (semitone < 0) continue;

        int midiNote = (computerKeyboardOctave * 12) + semitone;
        if (midiNote < 0 || midiNote > 127) continue;

        bool wasDown = keysCurrentlyDown.count(keyCode) > 0;

        if (isDown && !wasDown) { keysCurrentlyDown.insert(keyCode); sendNoteOn(midiNote); }
        else if (!isDown && wasDown) { keysCurrentlyDown.erase(keyCode); sendNoteOff(midiNote); }
    }
    return true;
}

void MainComponent::setVisControlsVisible()
{
    bool geiss = (currentVisMode == 3);
    geissWaveBtn.setVisible(geiss);
    geissPaletteBtn.setVisible(geiss);
    geissSceneBtn.setVisible(geiss);
    geissWaveUpBtn.setVisible(geiss);
    geissWaveDownBtn.setVisible(geiss);
    geissWarpLockBtn.setVisible(geiss);
    geissPalLockBtn.setVisible(geiss);
    geissSpeedSelector.setVisible(geiss);
    geissAutoPilotBtn.setVisible(geiss);
    geissBgBtn.setVisible(geiss);

    bool pm = (currentVisMode == 4);
    pmNextBtn.setVisible(pm);
    pmPrevBtn.setVisible(pm);
    pmRandBtn.setVisible(pm);
    pmLockBtn.setVisible(pm);
    pmBgBtn.setVisible(pm);

    bool gf = (currentVisMode == 2);
    gfRibbonUpBtn.setVisible(gf);
    gfRibbonDownBtn.setVisible(gf);
    gfTrailBtn.setVisible(gf);
    gfSpeedSelector.setVisible(gf);

    bool spec = (currentVisMode == 0);
    specDecayBtn.setVisible(spec);
    specSensUpBtn.setVisible(spec);
    specSensDownBtn.setVisible(spec);

    bool liss = (currentVisMode == 1);
    lissZoomInBtn.setVisible(liss);
    lissZoomOutBtn.setVisible(liss);
    lissDotsBtn.setVisible(liss);

    bool fluid = (currentVisMode == 8);
    fluidColorBtn.setVisible(fluid);
    fluidViscUpBtn.setVisible(fluid);
    fluidViscDownBtn.setVisible(fluid);
    fluidVortBtn.setVisible(fluid);

    bool rm = (currentVisMode == 9);
    rmPrevBtn.setVisible(rm);
    rmNextBtn.setVisible(rm);
}

void MainComponent::updateVisualizerTimers()
{
    // Stop all visualizer timers to prevent idle GPU/CPU work
    spectrumDisplay.stopTimer();
    lissajousDisplay.stopTimer();
    waveTerrainDisplay.stopTimer();
    shaderToyDisplay.stopTimer();
    geissDisplay.stopTimer();
    projectMDisplay.stopTimer();
    analyzerDisplay.stopTimer();
    heartbeatDisplay->stopTimer();
    bioResonanceDisplay->stopTimer();
    fluidSimDisplay.stopTimer();
    rayMarchDisplay.stopTimer();

    // Start only the active visualizer's timer
    switch (currentVisMode) {
        case 0: spectrumDisplay.startTimerHz(60); break;
        case 1: lissajousDisplay.startTimerHz(60); break;
        case 2: waveTerrainDisplay.startTimerHz(60); break;
        case 3: geissDisplay.startTimerHz(60); break;
        case 4: projectMDisplay.startTimerHz(60); break;
        case 5: analyzerDisplay.startTimerHz(60); break;
        case 6: heartbeatDisplay->startTimerHz(60); break;
        case 7: bioResonanceDisplay->startTimerHz(60); break;
        case 8: fluidSimDisplay.startTimerHz(60); break;
        case 9: rayMarchDisplay.startTimerHz(60); break;
    }
}

// ── Glass/Liquid animation methods ──

void MainComponent::addRipple(float x, float y)
{
    if (rippleCount < MAX_RIPPLES)
    {
        ripples[static_cast<size_t>(rippleCount)] = { x, y, 0.0f, 120.0f };
        ++rippleCount;
    }
}

void MainComponent::drawWaterCaustics(juce::Graphics& g, juce::Rectangle<int> area)
{
    // Caustic network — overlapping sine waves at different angles
    // create the diamond-mesh light pattern seen on shallow ocean floors.
    // Rendered as a grid of soft dots whose brightness is modulated.
    float t = static_cast<float>(glassAnimTime);
    auto& c = themeManager.getColors();
    juce::Colour accent(c.amber);

    float w = static_cast<float>(area.getWidth());
    float h = static_cast<float>(area.getHeight());
    float ax = static_cast<float>(area.getX());
    float ay = static_cast<float>(area.getY());

    // ── Realistic ocean caustics ──
    // 5 wave layers at different angles create the characteristic bright
    // concentrated lines and dark pools of real underwater light.
    // Pro devices render at half-res for detail, others at quarter-res.

    float tiltOffsetX = smoothTiltX * 1.5f;
    float tiltOffsetY = smoothTiltY * 1.5f;

    // 5 wave directions that wander independently
    float angle1 = std::sin(t * 0.003f) * 1.5f + std::sin(t * 0.0017f + 1.0f) * 0.8f
                  + std::sin(t * 0.0007f + 0.3f) * 2.0f + tiltOffsetX;
    float angle2 = std::sin(t * 0.0025f + 2.0f) * 1.3f + std::sin(t * 0.0013f + 3.0f) * 0.9f
                  + std::sin(t * 0.0005f + 1.7f) * 1.8f + tiltOffsetY;
    float angle3 = std::sin(t * 0.002f + 4.5f) * 1.6f + std::sin(t * 0.0011f + 5.0f) * 0.7f
                  + std::sin(t * 0.0004f + 4.0f) * 2.2f + (tiltOffsetX + tiltOffsetY) * 0.5f;
    float angle4 = std::sin(t * 0.0018f + 1.2f) * 1.4f + std::sin(t * 0.0009f + 2.8f) * 1.1f
                  + tiltOffsetX * 0.7f;
    float angle5 = std::sin(t * 0.0022f + 3.7f) * 1.2f + std::sin(t * 0.0015f + 0.5f) * 0.6f
                  + tiltOffsetY * 0.8f;

    float cos1 = std::cos(angle1), sin1 = std::sin(angle1);
    float cos2 = std::cos(angle2), sin2 = std::sin(angle2);
    float cos3 = std::cos(angle3), sin3 = std::sin(angle3);
    float cos4 = std::cos(angle4), sin4 = std::sin(angle4);
    float cos5 = std::cos(angle5), sin5 = std::sin(angle5);

    bool highPerf = isHighTier();
    bool jamie = (deviceTier == AUScanner::DeviceTier::JamieEdition);
    float speedMul = highPerf ? 18.0f : (jamie ? 12.0f : (isLowTier() ? 1.0f : 8.0f));
    float sp1 = 0.12f * speedMul, sp2 = 0.095f * speedMul, sp3 = 0.075f * speedMul;
    float sp4 = 0.11f * speedMul, sp5 = 0.065f * speedMul;

    // High: half-res, Jamie/Mid: third-res, Low: quarter-res
    float resFactor = highPerf ? 2.0f : (jamie ? 2.5f : (isLowTier() ? 4.0f : 3.0f));
    int cw = (int)(w / resFactor);
    int ch = (int)(h / resFactor);
    if (cw < 2 || ch < 2) return;

    static juce::Image causticImg;
    static int causticFrame = 0;

    bool needsUpdate = (causticImg.getWidth() != cw || causticImg.getHeight() != ch);
    if (needsUpdate)
        causticImg = juce::Image(juce::Image::ARGB, cw, ch, true);

    int cacheInterval = highPerf ? 2 : (isLowTier() ? 4 : 3);
    if (needsUpdate || ++causticFrame >= cacheInterval)
    {
        causticFrame = 0;
        causticImg.clear(causticImg.getBounds());

        bool lightTheme = (themeManager.getCurrentTheme() == ThemeManager::LiquidGlassLight);

        // Richer color palette for ocean light
        juce::Colour brightAqua = lightTheme ? juce::Colour(0xff1888cc) : juce::Colour(0xff70e0d0);
        juce::Colour deepBlue   = lightTheme ? juce::Colour(0xff0858a0) : juce::Colour(0xff3090c0);
        juce::Colour warmCyan   = lightTheme ? juce::Colour(0xff20a0d8) : juce::Colour(0xff50c8b8);
        float alphaScale = lightTheme ? 0.35f : 0.18f;

        for (int py = 0; py < ch; ++py)
        {
            float y = ay + (float)py * resFactor;
            for (int px = 0; px < cw; ++px)
            {
                float x = ax + (float)px * resFactor;

                // 5-wave interference — creates realistic caustic network
                float d1 = (x * cos1 + y * sin1) * 0.018f + t * sp1;
                float d2 = (x * cos2 + y * sin2) * 0.022f + t * sp2;
                float d3 = (x * cos3 + y * sin3) * 0.015f + t * sp3;
                float d4 = (x * cos4 + y * sin4) * 0.025f + t * sp4;
                float d5 = (x * cos5 + y * sin5) * 0.013f + t * sp5;

                float s1 = std::sin(d1);
                float s2 = std::sin(d2);
                float s3 = std::sin(d3);
                float s4 = std::sin(d4);
                float s5 = std::sin(d5);

                // Caustic = constructive interference peaks
                // Real caustics: light concentrates into bright thin lines
                float raw = (s1 + s2 + s3 + s4 + s5) / 5.0f;  // -1 to 1
                raw = raw * 0.5f + 0.5f;                        // 0 to 1

                // Sharp peaks — cubic power concentrates brightness into thin lines
                float caustic = raw * raw * raw;

                // Secondary layer at different scale for fine detail
                float fine1 = std::sin((x * cos2 - y * sin1) * 0.035f + t * sp3 * 1.3f);
                float fine2 = std::sin((x * cos4 + y * sin3) * 0.030f + t * sp1 * 0.8f);
                float fineDetail = (fine1 + fine2) * 0.25f + 0.5f;
                fineDetail = fineDetail * fineDetail;

                // Blend coarse caustic network with fine shimmer
                float combined = caustic * 0.7f + fineDetail * 0.3f;

                float alpha = combined * alphaScale;
                if (alpha < 0.01f) continue;

                // Color varies across the pattern — brighter lines get warmer color
                float hueShift = std::sin(x * 0.003f + y * 0.004f + t * 0.008f) * 0.5f + 0.5f;
                float brightShift = caustic;  // bright lines get a different hue
                juce::Colour col = deepBlue.interpolatedWith(brightAqua, hueShift)
                                            .interpolatedWith(warmCyan, brightShift * 0.4f);

                causticImg.setPixelAt(px, py, col.withAlpha(juce::jlimit(0.0f, 1.0f, alpha)));
            }
        }
    }

    // Draw scaled up — bilinear filtering smooths the pixels into organic shapes
    g.setOpacity(1.0f);
    g.drawImage(causticImg, area.toFloat(),
                juce::RectanglePlacement::stretchToFit);
}

void MainComponent::drawRipples(juce::Graphics& g)
{
    auto& c = themeManager.getColors();
    juce::Colour accent(c.amber);

    for (int i = 0; i < rippleCount; ++i)
    {
        auto& rip = ripples[static_cast<size_t>(i)];
        float progress = rip.age / 1.2f;  // 0..1 over 1.2 seconds
        float alpha = (1.0f - progress) * 0.3f;
        float radius = rip.maxRadius * progress;

        if (alpha > 0.01f)
        {
            // Expanding ring
            g.setColour(accent.withAlpha(alpha));
            g.drawEllipse(rip.x - radius, rip.y - radius,
                          radius * 2, radius * 2, 1.5f);

            // Inner ring (slightly behind)
            float innerR = radius * 0.6f;
            float innerA = alpha * 0.5f;
            g.setColour(accent.withAlpha(innerA));
            g.drawEllipse(rip.x - innerR, rip.y - innerR,
                          innerR * 2, innerR * 2, 1.0f);
        }
    }
}

void MainComponent::drawBreathingStripe(juce::Graphics& g)
{
    // Slow drifting orbs — soft glowing pools of light
    auto& c = themeManager.getColors();
    float t = static_cast<float>(glassAnimTime);
    float w = static_cast<float>(getWidth());
    float h = static_cast<float>(getHeight());
    juce::Colour accent(c.amber);

    juce::Colour warmOrb(0xff60c8c0);
    juce::Colour coolOrb(0xff4098d0);

    for (int i = 0; i < 5; ++i)
    {
        float fi = static_cast<float>(i);
        // Each orb has its own wandering path — multiple sines for non-repeating drift
        float cx = w * (0.1f + 0.8f * (0.5f + 0.3f * std::sin(t * 0.008f + fi * 1.3f)
                                             + 0.2f * std::sin(t * 0.013f + fi * 2.7f)));
        float cy = h * (0.1f + 0.8f * (0.5f + 0.3f * std::cos(t * 0.006f + fi * 1.7f)
                                             + 0.2f * std::cos(t * 0.011f + fi * 3.1f)));
        float r = 120.0f + 60.0f * std::sin(t * 0.01f + fi * 2.0f);
        float alpha = 0.12f + 0.06f * std::sin(t * 0.012f + fi * 0.9f);

        float hue = std::sin(t * 0.007f + fi * 1.2f) * 0.5f + 0.5f;
        juce::Colour col = warmOrb.interpolatedWith(coolOrb, hue);

        juce::ColourGradient grad(
            col.withAlpha(alpha), cx, cy,
            col.withAlpha(0.0f), cx + r, cy + r, true);
        g.setGradientFill(grad);
        g.fillEllipse(cx - r, cy - r, r * 2, r * 2);
    }
}

void MainComponent::applyThemeToControls()
{
    auto& c = themeManager.getColors();
    auto fontName = themeManager.getLookAndFeel()->getUIFontName();

    // Labels
#if JUCE_IOS
    trackNameLabel.setColour(juce::Label::textColourId, juce::Colour(c.textSecondary));
#else
    trackNameLabel.setColour(juce::Label::textColourId, juce::Colour(c.lcdText));
#endif
    trackNameLabel.setFont(juce::Font(fontName, 16.0f, juce::Font::bold));
    beatLabel.setFont(juce::Font(fontName, 10.0f, juce::Font::plain));
    beatLabel.setJustificationType(juce::Justification::centred);
    beatLabel.setColour(juce::Label::textColourId, juce::Colour(c.lcdText));
    cpuLabel.setFont(juce::Font(fontName, 12.0f, juce::Font::bold));
    cpuLabel.setColour(juce::Label::textColourId, juce::Colour(c.lcdText));
    cpuLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    beatLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    statusLabel.setFont(juce::Font(fontName, 14.0f, juce::Font::bold));
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(c.lcdText).withAlpha(0.7f));
    statusLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    chordLabel.setFont(juce::Font(fontName, 26.0f, juce::Font::bold));
    chordLabel.setColour(juce::Label::textColourId, juce::Colour(c.lcdText));
    chordLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    chordLabel.setJustificationType(juce::Justification::centred);
    trackInfoLabel.setColour(juce::Label::textColourId, juce::Colour(c.textSecondary));

    // BPM controls
    bpmLabel.setFont(juce::Font(fontName, 14.0f, juce::Font::bold));
    bpmLabel.setColour(juce::Label::textColourId, juce::Colour(c.lcdText));
    bpmLabel.setColour(juce::Label::backgroundColourId, juce::Colour(c.lcdBg));
    bpmDownButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnNav));
    bpmUpButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnNav));

    // Transport
    recordButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.redDark));
    recordButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(c.red));
    playButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.greenDark));
    stopButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnStop));
    metronomeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnMetronome));
    metronomeButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(c.btnMetronomeOn));
    countInButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.lcdBg).brighter(0.15f));
    countInButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(c.lcdText));
    countInButton.setColour(juce::TextButton::textColourOffId, juce::Colour(c.lcdText));
    countInButton.setColour(juce::TextButton::textColourOnId, juce::Colour(c.lcdBg));
    loopButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnLoop));
    loopButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(c.btnLoopOn));
    panicButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.amber));
    midiLearnButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.lcdBg).brighter(0.15f));
    midiLearnButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(c.lcdText));
    midiLearnButton.setColour(juce::TextButton::textColourOffId, juce::Colour(c.lcdText));
    midiLearnButton.setColour(juce::TextButton::textColourOnId, juce::Colour(c.lcdBg));
    pianoToggleButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.lcdBg).brighter(0.15f));
    pianoToggleButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(c.lcdText));
    pianoToggleButton.setColour(juce::TextButton::textColourOffId, juce::Colour(c.lcdText));
    pianoToggleButton.setColour(juce::TextButton::textColourOnId, juce::Colour(c.lcdBg));
    mixerButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.lcdBg).brighter(0.15f));
    mixerButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(c.lcdText));
    mixerButton.setColour(juce::TextButton::textColourOffId, juce::Colour(c.lcdText));
    mixerButton.setColour(juce::TextButton::textColourOnId, juce::Colour(c.lcdBg));
    sessionViewButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.lcdBg).brighter(0.15f));
    sessionViewButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(c.amber));
    sessionViewButton.setColour(juce::TextButton::textColourOffId, juce::Colour(c.lcdText));
    sessionViewButton.setColour(juce::TextButton::textColourOnId, juce::Colour(c.lcdBg));

    // Arpeggiator buttons
    arpButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.lcdBg).brighter(0.15f));
    arpButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(c.amber));
    arpButton.setColour(juce::TextButton::textColourOffId, juce::Colour(c.lcdText));
    arpButton.setColour(juce::TextButton::textColourOnId, juce::Colour(c.lcdBg));
    arpModeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnNav));
    arpModeButton.setColour(juce::TextButton::textColourOffId, juce::Colour(c.lcdText));
    arpRateButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnNav));
    arpRateButton.setColour(juce::TextButton::textColourOffId, juce::Colour(c.lcdText));
    arpOctButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnNav));
    arpOctButton.setColour(juce::TextButton::textColourOffId, juce::Colour(c.lcdText));

    // Edit toolbar
    newClipButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnNewClip));
    deleteClipButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnDeleteClip));
    duplicateClipButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnDuplicate));
    splitClipButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnSplit));
    quantizeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnQuantize));
    editClipButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnEditNotes));

    // Navigation
    zoomInButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnNav));
    zoomOutButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnNav));
    scrollLeftButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnNav));
    scrollRightButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnNav));

    // Right panel
    midiRefreshButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnNav));
    midi2Button.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnMidi2));
    midi2Button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(c.btnMidi2On));
    saveButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnSave));
    loadButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnLoad));
    undoButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnUndoRedo));
    redoButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnUndoRedo));

    // Capture button
    captureButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnNewClip));
    exportButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnNewClip));

    for (int i = 0; i < NUM_FX_SLOTS; ++i)
        fxEditorButtons[i]->setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnNav));

    // Parameter knobs
    for (int i = 0; i < NUM_PARAM_SLIDERS; ++i)
    {
        paramSliders[i]->setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(c.amber));
        paramSliders[i]->setColour(juce::Slider::thumbColourId, juce::Colour(c.lcdText));
        paramLabels[i]->setColour(juce::Label::textColourId, juce::Colour(c.textSecondary));
    }

    repaint();
}

void MainComponent::updateFxDisplay()
{
    for (int i = 0; i < NUM_FX_SLOTS; ++i)
    {
        auto* selector = fxSelectors[i];
        selector->clear(juce::dontSendNotification);
        selector->addItem("FX " + juce::String(i + 1) + ": Empty", 1);

        // Add all effect plugins
        for (int p = 0; p < fxDescriptions.size(); ++p)
            selector->addItem(fxDescriptions[p].name, p + 2);

        // Check if an FX is loaded in this slot
        auto& track = pluginHost.getTrack(selectedTrackIndex);
        if (track.fxSlots[i].processor != nullptr)
        {
            juce::String fxName = track.fxSlots[i].processor->getName();
            // Find the matching item
            bool found = false;
            for (int p = 0; p < fxDescriptions.size(); ++p)
            {
                if (fxDescriptions[p].name == fxName)
                {
                    selector->setSelectedId(p + 2, juce::dontSendNotification);
                    found = true;
                    break;
                }
            }
            if (!found)
                selector->setSelectedId(1, juce::dontSendNotification);

        }
        else
        {
            selector->setSelectedId(1, juce::dontSendNotification);
        }
    }
}

void MainComponent::updateTrackInputSelector()
{
    trackInputSelector.clear(juce::dontSendNotification);

    // Audio option
    trackInputSelector.addItem("Audio: Mic", 1);

    // MIDI options
    auto devices = juce::MidiInput::getAvailableDevices();
    for (int i = 0; i < devices.size(); ++i)
        trackInputSelector.addItem("MIDI: " + devices[i].name, 100 + i);

    if (devices.isEmpty())
        trackInputSelector.addItem("MIDI (no device)", 99);

    // Select current and connect
    auto& track = pluginHost.getTrack(selectedTrackIndex);
    if (track.type == TrackType::Audio)
    {
        trackInputSelector.setSelectedId(1, juce::dontSendNotification);
    }
    else if (devices.size() > 0)
    {
        // Prefer the device the user already had active so switching
        // tracks via the controller doesn't reset their MIDI input
        // back to the first listed device every time.  When no input
        // is yet chosen, prefer the Launchkey's keys port (the one
        // WITHOUT "DAW" in its name — the DAW port is consumed by the
        // controller integration and can't double as a track input).
        // Falls through to devices[0] if no Launchkey port is found.
        int preferIdx = 0;
        if (currentMidiDeviceId.isNotEmpty())
        {
            for (int i = 0; i < devices.size(); ++i)
                if (devices[i].identifier == currentMidiDeviceId) { preferIdx = i; break; }
        }
        else
        {
            for (int i = 0; i < devices.size(); ++i)
                if (devices[i].name.containsIgnoreCase("launch")
                    && !devices[i].name.containsIgnoreCase("daw"))
                { preferIdx = i; break; }
        }
        trackInputSelector.setSelectedId(100 + preferIdx, juce::dontSendNotification);
        disableCurrentMidiDevice();
        deviceManager.setMidiInputDeviceEnabled(devices[preferIdx].identifier, true);
        deviceManager.addMidiInputDeviceCallback(devices[preferIdx].identifier, this);
        currentMidiDeviceId = devices[preferIdx].identifier;
    }
    else
    {
        trackInputSelector.setSelectedId(99, juce::dontSendNotification);
    }
}

void MainComponent::applyTrackInput(int id)
{
    // Check if track has content before allowing type change
    auto& track = pluginHost.getTrack(selectedTrackIndex);
    bool currentIsAudio = (track.type == TrackType::Audio);
    bool newIsAudio = (id == 1);
    if (currentIsAudio != newIsAudio && track.clipPlayer != nullptr)
    {
        for (int s = 0; s < track.clipPlayer->getNumSlots(); ++s)
        {
            if (track.clipPlayer->getSlot(s).hasContent())
            {
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                    "Cannot Change Input",
                    "Clear all clips on this track before changing the input type.");
                // Reset selector to current value
                if (currentIsAudio)
                    trackInputSelector.setSelectedId(1, juce::dontSendNotification);
                else
                    trackInputSelector.setSelectedId(99, juce::dontSendNotification);
                return;
            }
        }
    }

    // Disconnect current MIDI device first
    disableCurrentMidiDevice();

    if (id == 1)
    {
        // Audio: Built-in Mic
        pluginHost.setTrackType(selectedTrackIndex, TrackType::Audio);
    }
    else if (id >= 100)
    {
        // MIDI input — connect the selected device
        pluginHost.setTrackType(selectedTrackIndex, TrackType::MIDI);

        int midiIdx = id - 100;
        auto devices = juce::MidiInput::getAvailableDevices();
        if (midiIdx >= 0 && midiIdx < devices.size())
        {
            const auto& d = devices[midiIdx];
            deviceManager.setMidiInputDeviceEnabled(d.identifier, true);
            deviceManager.addMidiInputDeviceCallback(d.identifier, this);
            currentMidiDeviceId = d.identifier;
        }
    }

    resized();
    repaint();
}

void MainComponent::updatePresetList()
{
    presetSelector.clear(juce::dontSendNotification);

    auto& track = pluginHost.getTrack(selectedTrackIndex);
    if (track.plugin == nullptr)
    {
        presetSelector.addItem("-- No Plugin --", 1);
        presetSelector.setSelectedId(1, juce::dontSendNotification);
        return;
    }

    auto presetNames = AUPresetHelper::getPresetNames(track.plugin);
    int numPresets = presetNames.size();

    // Check if we got real named presets vs just numbered fallbacks
    bool hasNamedPresets = false;
    for (auto& n : presetNames)
        if (!n.startsWith("Preset ")) { hasNamedPresets = true; break; }

    if (numPresets > 1 && hasNamedPresets)
    {
        // Plugin has named presets — show them
        presetSelector.addItem("-- Preset --", 1);
        for (int i = 0; i < numPresets; ++i)
            presetSelector.addItem(presetNames[i], i + 2);

        int current = track.plugin->getCurrentProgram();
        presetSelector.setSelectedId(current + 2, juce::dontSendNotification);
    }
    else if (numPresets > 1)
    {
        // Plugin has presets but no names — show count, use editor for selection
        presetSelector.addItem("-- " + juce::String(numPresets) + " presets (use editor) --", 1);
        presetSelector.setSelectedId(1, juce::dontSendNotification);

        // Still add numbered items so up/down buttons work
        for (int i = 0; i < numPresets; ++i)
            presetSelector.addItem(juce::String(i + 1) + " / " + juce::String(numPresets), i + 2);

        int current = track.plugin->getCurrentProgram();
        presetSelector.setSelectedId(current + 2, juce::dontSendNotification);
    }
    else
    {
        // No presets — plugin manages presets internally
        presetSelector.addItem("-- Use plugin editor --", 1);
        presetSelector.setSelectedId(1, juce::dontSendNotification);

        // Retry in case presets load async
        juce::Component::SafePointer<MainComponent> safeThis(this);
        int trackIdx = selectedTrackIndex;
        juce::Timer::callAfterDelay(2000, [safeThis, trackIdx] {
            if (auto* self = safeThis.getComponent())
            {
                if (self->selectedTrackIndex != trackIdx) return;
                auto& t = self->pluginHost.getTrack(trackIdx);
                if (t.plugin != nullptr && t.plugin->getNumPrograms() > 1)
                    self->updatePresetList();
            }
        });
    }
}

void MainComponent::loadPreset(int index)
{
    auto& track = pluginHost.getTrack(selectedTrackIndex);
    if (track.plugin == nullptr) return;
    if (index < 0 || index >= track.plugin->getNumPrograms()) return;

    track.plugin->setCurrentProgram(index);
    presetSelector.setSelectedId(index + 2, juce::dontSendNotification);

    juce::String name = track.plugin->getProgramName(index);
    statusLabel.setText("Preset: " + name, juce::dontSendNotification);

    // Refresh param knobs with new preset values
    paramPageOffset = 0;
    paramSmartPage = true;
    updateParamSliders();
}

void MainComponent::loadFxPlugin(int slotIndex)
{
    auto* selector = fxSelectors[slotIndex];
    int id = selector->getSelectedId();

    if (id <= 1)
    {
        // "Empty" selected — unload
        pluginHost.unloadFx(selectedTrackIndex, slotIndex);
        return;
    }

    int descIdx = id - 2;
    if (descIdx < 0 || descIdx >= fxDescriptions.size()) return;

    juce::String err;
    bool ok = pluginHost.loadFx(selectedTrackIndex, slotIndex, fxDescriptions[descIdx], err);
    if (!ok)
        statusLabel.setText("FX load error: " + err, juce::dontSendNotification);
    else
        statusLabel.setText("FX " + juce::String(slotIndex + 1) + ": " + fxDescriptions[descIdx].name, juce::dontSendNotification);
}

void MainComponent::openFxEditor(int slotIndex)
{
    auto& track = pluginHost.getTrack(selectedTrackIndex);
    if (slotIndex < 0 || slotIndex >= Track::NUM_FX_SLOTS) return;

    auto* fxProc = track.fxSlots[slotIndex].processor;
    if (fxProc == nullptr || !fxProc->hasEditor()) return;

    auto* editor = fxProc->createEditor();
    if (editor == nullptr) return;

#if JUCE_IOS
    auto* overlay = new juce::Component();
    overlay->setName("EditorOverlay");

    auto* closeBtn = new juce::TextButton("X");
    closeBtn->setColour(juce::TextButton::buttonColourId, juce::Colours::red.withAlpha(0.8f));
    closeBtn->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    closeBtn->onClick = [this] {
        for (int i = getNumChildComponents() - 1; i >= 0; --i)
        {
            if (auto* child = getChildComponent(i))
            {
                if (child->getName() == "EditorOverlay")
                {
                    removeChildComponent(i);
                    delete child;
                    break;
                }
            }
        }
    };
    closeBtn->setColour(juce::TextButton::buttonColourId, juce::Colours::red.withAlpha(0.9f));
    closeBtn->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    closeBtn->setButtonText("CLOSE");
    overlay->addAndMakeVisible(closeBtn);
    overlay->addAndMakeVisible(editor);

    int edW = editor->getWidth();
    int edH = editor->getHeight();
    if (edW < 400) edW = 400;
    if (edH < 300) edH = 300;

    int closeBarH = 44;
    int ew = juce::jmin(edW, getWidth() - 20);
    int eh = juce::jmin(edH, getHeight() - closeBarH - 20);
    int ox = (getWidth() - ew) / 2;
    int oy = (getHeight() - eh - closeBarH) / 2;

    overlay->setBounds(ox, oy, ew, eh + closeBarH);
    closeBtn->setBounds(0, 0, ew, closeBarH);
    editor->setBounds(0, closeBarH, ew, eh);

    addAndMakeVisible(overlay);
    overlay->toFront(true);
#else
    auto name = fxProc->getName() + " (FX " + juce::String(slotIndex + 1) + ")";
    new PluginEditorWindow(name, editor, [editor] { delete editor->getParentComponent(); });
#endif
}

void MainComponent::startMidiLearn(MidiTarget target)
{
    // Check if this target already has a mapping — offer to unlearn
    for (int i = midiMappings.size() - 1; i >= 0; --i)
    {
        if (midiMappings[i].target == target)
        {
            auto mapping = midiMappings[i];
            juce::String info = "CC" + juce::String(mapping.ccNumber) + " ch" + juce::String(mapping.channel);

            juce::AlertWindow::showOkCancelBox(
                juce::MessageBoxIconType::QuestionIcon,
                "MIDI Mapping Exists",
                "This control is mapped to " + info + ".\n\nRemove mapping?",
                "Unlearn", "Keep & Remap",
                nullptr,
                juce::ModalCallbackFunction::create([this, target, i](int result) {
                    if (result == 1)
                    {
                        // Unlearn — remove the mapping
                        midiMappings.remove(i);
                        statusLabel.setText("MIDI mapping removed", juce::dontSendNotification);
                        midiLearnTarget = MidiTarget::None;
                    }
                    else
                    {
                        // Keep & remap — proceed with normal learn
                        midiLearnTarget = target;
                        statusLabel.setText("Waiting for CC...", juce::dontSendNotification);
                    }
                }));
            return;
        }
    }

    midiLearnTarget = target;

    juce::String targetName;
    switch (target)
    {
        case MidiTarget::Volume:    targetName = "Volume"; break;
        case MidiTarget::Pan:       targetName = "Pan"; break;
        case MidiTarget::Bpm:       targetName = "BPM"; break;
        case MidiTarget::Play:      targetName = "Play"; break;
        case MidiTarget::Stop:      targetName = "Stop"; break;
        case MidiTarget::Record:    targetName = "Record"; break;
        case MidiTarget::Metronome: targetName = "Metronome"; break;
        case MidiTarget::Loop:      targetName = "Loop"; break;
        case MidiTarget::TrackNext: targetName = "Track Next"; break;
        case MidiTarget::TrackPrev: targetName = "Track Prev"; break;
        case MidiTarget::Param0:  case MidiTarget::Param1:  case MidiTarget::Param2:
        case MidiTarget::Param3:  case MidiTarget::Param4:  case MidiTarget::Param5:
        case MidiTarget::Param6:  case MidiTarget::Param7:  case MidiTarget::Param8:
        case MidiTarget::Param9:  case MidiTarget::Param10: case MidiTarget::Param11:
        case MidiTarget::Param12: case MidiTarget::Param13: case MidiTarget::Param14:
        case MidiTarget::Param15:
            targetName = "Param " + juce::String(static_cast<int>(target) - static_cast<int>(MidiTarget::Param0) + 1);
            break;
        case MidiTarget::GeissWaveform:   targetName = "Geiss Wave"; break;
        case MidiTarget::GeissPalette:    targetName = "Geiss Palette"; break;
        case MidiTarget::GeissScene:      targetName = "Geiss Scene"; break;
        case MidiTarget::GeissWaveScale:  targetName = "Geiss Wave Scale"; break;
        case MidiTarget::GeissWarpLock:   targetName = "Geiss Warp Lock"; break;
        case MidiTarget::GeissPaletteLock:targetName = "Geiss Palette Lock"; break;
        case MidiTarget::GeissSpeed:      targetName = "Geiss Speed"; break;
        case MidiTarget::GForceRibbons:   targetName = "GF Ribbons"; break;
        case MidiTarget::GForceTrail:     targetName = "GF Trail"; break;
        case MidiTarget::GForceSpeed:     targetName = "GF Speed"; break;
        case MidiTarget::SpecDecay:       targetName = "Spec Decay"; break;
        case MidiTarget::SpecSensitivity: targetName = "Spec Sensitivity"; break;
        case MidiTarget::LissZoom:        targetName = "Liss Zoom"; break;
        case MidiTarget::LissDots:        targetName = "Liss Dots"; break;
        default: targetName = "?"; break;
    }

    statusLabel.setText("Waiting for CC -> " + targetName + "...", juce::dontSendNotification);
}

void MainComponent::processMidiLearnCC(int channel, int cc, int /*value*/)
{
    // Remove any existing mapping for this CC
    for (int i = midiMappings.size() - 1; i >= 0; --i)
    {
        if (midiMappings[i].channel == channel && midiMappings[i].ccNumber == cc)
            midiMappings.remove(i);
    }

    // Also remove any existing mapping for this target
    for (int i = midiMappings.size() - 1; i >= 0; --i)
    {
        if (midiMappings[i].target == midiLearnTarget)
            midiMappings.remove(i);
    }

    MidiMapping mapping;
    mapping.channel = channel;
    mapping.ccNumber = cc;
    mapping.target = midiLearnTarget;
    midiMappings.add(mapping);

    statusLabel.setText("Mapped CC" + juce::String(cc) + " ch" + juce::String(channel)
                        + " -> " + statusLabel.getText().fromFirstOccurrenceOf("-> ", false, false),
                        juce::dontSendNotification);

    midiLearnTarget = MidiTarget::None;
}

void MainComponent::applyMidiCC(const MidiMapping& mapping, int value)
{
    float norm = static_cast<float>(value) / 127.0f;

    switch (mapping.target)
    {
        case MidiTarget::Volume:
            volumeSlider.setValue(norm, juce::sendNotification);
            break;
        case MidiTarget::Pan:
            panSlider.setValue(norm * 2.0 - 1.0, juce::sendNotification);
            break;
        case MidiTarget::Bpm:
        {
            double bpm = 20.0 + norm * 280.0;
            pluginHost.getEngine().setBpm(bpm);
            bpmLabel.setText(juce::String(static_cast<int>(bpm)) + " BPM", juce::dontSendNotification);
            break;
        }
        case MidiTarget::Play:
            if (value > 0) pluginHost.getEngine().play();
            break;
        case MidiTarget::Stop:
            if (value > 0) pluginHost.getEngine().stop();
            break;
        case MidiTarget::Record:
            if (value > 0) pluginHost.getEngine().toggleRecord();
            break;
        case MidiTarget::Metronome:
            if (value > 0) pluginHost.getEngine().toggleMetronome();
            break;
        case MidiTarget::Loop:
            if (value > 0) pluginHost.getEngine().toggleLoop();
            break;
        case MidiTarget::TrackNext:
            if (value > 0) selectTrack(juce::jmin(PluginHost::NUM_TRACKS - 1, selectedTrackIndex + 1));
            break;
        case MidiTarget::TrackPrev:
            if (value > 0) selectTrack(juce::jmax(0, selectedTrackIndex - 1));
            break;
        case MidiTarget::Param0:  case MidiTarget::Param1:  case MidiTarget::Param2:
        case MidiTarget::Param3:  case MidiTarget::Param4:  case MidiTarget::Param5:
        case MidiTarget::Param6:  case MidiTarget::Param7:  case MidiTarget::Param8:
        case MidiTarget::Param9:  case MidiTarget::Param10: case MidiTarget::Param11:
        case MidiTarget::Param12: case MidiTarget::Param13: case MidiTarget::Param14:
        case MidiTarget::Param15:
        {
            int idx = static_cast<int>(mapping.target) - static_cast<int>(MidiTarget::Param0);
            if (idx >= 0 && idx < NUM_PARAM_SLIDERS && paramSliders[idx]->isEnabled())
                paramSliders[idx]->setValue(norm, juce::sendNotification);
            break;
        }
        case MidiTarget::GeissWaveform:
            if (value > 0) geissDisplay.cycleWaveform();
            break;
        case MidiTarget::GeissPalette:
            if (value > 0) geissDisplay.setPaletteStyle(value % GeissComponent::NUM_PALETTE_STYLES);
            break;
        case MidiTarget::GeissScene:
            if (value > 0) geissDisplay.newRandomScene();
            break;
        case MidiTarget::GeissWaveScale:
            geissDisplay.setWaveScale(norm * 3.0f);
            break;
        case MidiTarget::GeissWarpLock:
            if (value > 0) geissDisplay.toggleWarpLock();
            break;
        case MidiTarget::GeissPaletteLock:
            if (value > 0) geissDisplay.togglePaletteLock();
            break;
        case MidiTarget::GeissSpeed:
            geissDisplay.setSpeed(0.25f + norm * 3.75f);
            break;
        case MidiTarget::GForceRibbons:
        case MidiTarget::GForceTrail:
        case MidiTarget::GForceSpeed:
            break; // G-Force removed, these targets are unused
        case MidiTarget::SpecDecay:
            spectrumDisplay.setDecaySpeed(0.5f + norm * 0.49f);
            break;
        case MidiTarget::SpecSensitivity:
            spectrumDisplay.setSensitivity(0.1f + norm * 1.9f);
            break;
        case MidiTarget::LissZoom:
            lissajousDisplay.setZoom(0.5f + norm * 9.5f);
            break;
        case MidiTarget::LissDots:
            lissajousDisplay.setDotCount(64 + static_cast<int>(norm * 960.0f));
            break;
        default: break;
    }
}

// ─── Metal GPU Caustic Renderer Integration ────────────────────────
#if JUCE_IOS

void MainComponent::attachMetalRendererIfNeeded()
{
    if (!metalRenderer || metalRendererAttached) return;
    if (!isShowing()) return;
    auto* peer = getPeer();
    if (!peer) return;

    metalRenderer->attachToView(peer->getNativeHandle());
    if (metalRenderer->isAttached())
    {
        metalRendererAttached = true;
        metalRenderer->setBounds(0, 0, getWidth(), getHeight());
        metalRenderer->setVisible(themeManager.isGlassOverlay() && glassAnimEnabled);
    }
}

void MainComponent::updateMetalCaustics()
{
    if (!metalRenderer || !metalRendererAttached) return;

    bool shouldShow = themeManager.isGlassOverlay() && glassAnimEnabled && !visualizerFullScreen;
    metalRenderer->setVisible(shouldShow);
    if (!shouldShow) return;

    // Update layer bounds if needed
    metalRenderer->setBounds(0, 0, getWidth(), getHeight());

    // ── Caustic uniforms ──
    CausticUniforms cu {};
    cu.time = static_cast<float>(glassAnimTime);
    cu.tiltX = smoothTiltX;
    cu.tiltY = smoothTiltY;
    cu.lightTheme = (themeManager.getCurrentTheme() == ThemeManager::LiquidGlassLight) ? 1 : 0;
    cu.alphaScale = cu.lightTheme ? 0.35f : 0.18f;
    cu.speedMul = isHighTier() ? 18.0f
               : (deviceTier == AUScanner::DeviceTier::JamieEdition ? 12.0f
               : (isLowTier() ? 1.0f : 8.0f));
    cu.viewWidth = static_cast<float>(getWidth());
    cu.viewHeight = static_cast<float>(getHeight());

    // Ripple data
    cu.rippleCount = rippleCount;
    for (int i = 0; i < rippleCount; ++i)
    {
        cu.rippleX[i] = ripples[static_cast<size_t>(i)].x;
        cu.rippleY[i] = ripples[static_cast<size_t>(i)].y;
        cu.rippleAge[i] = ripples[static_cast<size_t>(i)].age;
        cu.rippleMaxRadius[i] = ripples[static_cast<size_t>(i)].maxRadius;
    }
    // Accent color for ripples
    auto& c = themeManager.getColors();
    juce::Colour accent(c.amber);
    cu.rippleColor[0] = accent.getFloatRed();
    cu.rippleColor[1] = accent.getFloatGreen();
    cu.rippleColor[2] = accent.getFloatBlue();
    cu.rippleColor[3] = 1.0f;

    metalRenderer->setCausticUniforms(cu);

    // ── Button glow uniforms ──
    ButtonUniforms bu {};
    bu.time = cu.time;
    bu.tiltX = cu.tiltX;
    bu.tiltY = cu.tiltY;
    bu.speedMul = cu.speedMul;
    bu.viewWidth = cu.viewWidth;
    bu.viewHeight = cu.viewHeight;
    bu.lightTheme = cu.lightTheme;
    bu.buttonCount = 0;

    for (int ci = 0; ci < getNumChildComponents() && bu.buttonCount < MAX_BUTTON_RECTS; ++ci)
    {
        auto* child = getChildComponent(ci);
        if (child && child->isVisible() && dynamic_cast<juce::Button*>(child))
        {
            auto b = child->getBounds();
            bu.buttons[bu.buttonCount] = { static_cast<float>(b.getX()),
                                            static_cast<float>(b.getY()),
                                            static_cast<float>(b.getWidth()),
                                            static_cast<float>(b.getHeight()) };
            bu.buttonCount++;
        }
    }
    metalRenderer->setButtonUniforms(bu);

    // Render the Metal frame
    metalRenderer->render();
}

#endif // JUCE_IOS

// ─── Launchkey MK4 controller bridge ──────────────────────────────
//
// Helper methods that the LaunchkeyMK4Controller calls into.  Kept
// here so the controller class doesn't need to know about every
// internal subsystem layout.

// Controller MIDI callbacks fire on the CoreMIDI thread.  Anything
// that touches the engine, transport, or UI state needs to be
// marshalled onto the message thread or it silently no-ops (or
// trips JUCE thread assertions).  juce::Component::SafePointer
// guards against the user closing/destroying MainComponent before
// the async block runs.
template <typename Fn>
static inline void onMain(juce::Component::SafePointer<MainComponent> safe, Fn&& fn)
{
    juce::MessageManager::callAsync([safe, fn = std::forward<Fn>(fn)] {
        if (safe) fn(safe.getComponent());
    });
}

void MainComponent::controllerPlayToggle()
{
    juce::Component::SafePointer<MainComponent> safe(this);
    juce::MessageManager::callAsync([safe] {
        if (safe) safe->playButton.triggerClick();
    });
}
void MainComponent::controllerStop()
{
    juce::Component::SafePointer<MainComponent> safe(this);
    juce::MessageManager::callAsync([safe] {
        if (safe) safe->stopButton.triggerClick();
    });
}
void MainComponent::controllerRecordToggle()
{
    juce::Component::SafePointer<MainComponent> safe(this);
    juce::MessageManager::callAsync([safe] {
        if (safe) safe->recordButton.triggerClick();
    });
}
void MainComponent::controllerLoopToggle()
{
    juce::Component::SafePointer<MainComponent> safe(this);
    juce::MessageManager::callAsync([safe] {
        if (safe) safe->pluginHost.getEngine().toggleLoop();
    });
}

void MainComponent::controllerSelectTrack(int delta)
{
    juce::Component::SafePointer<MainComponent> safe(this);
    juce::MessageManager::callAsync([safe, delta] {
        if (auto* self = safe.getComponent())
        {
            int next = juce::jlimit(0, PluginHost::NUM_TRACKS - 1,
                                    self->selectedTrackIndex + delta);
            self->selectTrack(next);
            // Force the timeline to redraw — its visible track-highlight
            // is driven by pluginHost.getSelectedTrack() during paint
            // and won't refresh on its own.  Mirrors what Timeline's
            // own mouseDown handler does after setSelectedTrack.
            if (self->timelineComponent) self->timelineComponent->repaint();
        }
    });
}

void MainComponent::controllerScrollScenes(int delta)
{
    onMain(this, [delta](MainComponent* self) {
        if (self->sessionViewComponent) self->sessionViewComponent->scrollScenes(delta);
    });
}

void MainComponent::controllerLaunchScene()
{
    onMain(this, [](MainComponent* self) {
        if (self->sessionViewComponent)
            self->sessionViewComponent->launchSceneAtRow(self->sessionViewComponent->currentSceneRow());
    });
}

// ── Toolbar-pad mode (Launchkey themes) ──────────────────────────
//
// 16 pads → 16 toolbar buttons.  Layout, paired with palette indices
// chosen from the Novation 128-entry palette so each function group
// reads at a glance:
//   Top   : Grid  Quant New   Del   Split Edit  Save  Load
//   Bot   : Undo  Redo  Capt  Expt  Arp   AMode ARate AOct
// Mirroring those exact pad colors onto the iPad-side toolbar
// buttons (see launchkeyButtonColorRGB()) keeps the device + screen
// in visual lock-step.

namespace {
    struct LkPadEntry {
        // Identity
        int row, col;
        // Novation palette index for the pad LED (kept for the light
        // theme's pad colouring; the dark theme overrides via RGB).
        uint8_t pal;
        // Light-theme RGB — vibrant jewel tones, one per function
        uint32_t rgbLight;
        // Dark-theme RGB — collapsed to OLED-cyan brightness levels
        uint32_t rgbDark;
    };
    // ── Jewel tones (light theme) ──
    constexpr uint32_t jRed     = 0xffc26161;
    constexpr uint32_t jAmber   = 0xffc4894e;
    constexpr uint32_t jYellow  = 0xffbd9c4a;
    constexpr uint32_t jLime    = 0xff8ea65a;
    constexpr uint32_t jGreen   = 0xff5e9978;
    constexpr uint32_t jSpring  = 0xff579a8c;
    constexpr uint32_t jCyan    = 0xff5891a3;
    constexpr uint32_t jSky     = 0xff6790c4;
    constexpr uint32_t jBlue    = 0xff6e7ec0;
    constexpr uint32_t jPurple  = 0xff8a72bf;
    constexpr uint32_t jViolet  = 0xff9c72ba;
    constexpr uint32_t jMagenta = 0xffb27590;
    constexpr uint32_t jPink    = 0xffc18097;
    constexpr uint32_t jCoral   = 0xffbd8377;
    constexpr uint32_t jOrange  = 0xffc78b5d;
    // ── OLED-cyan brightness levels (dark theme) ──
    // Floor pre-bumped so the dim/faint levels survive the device's
    // 0-255 → 0-127 SysEx scaling and the LED hardware's non-linear
    // perceived-brightness curve.  Spread is preserved end-to-end so
    // the function-group gradient reads on both iPad + device.
    constexpr uint32_t cFaint  = 0xff446e7c;
    constexpr uint32_t cDim    = 0xff5891a3;
    constexpr uint32_t cMid    = 0xff78b0c4;
    constexpr uint32_t cAccent = 0xff9ac3d4;
    constexpr uint32_t cHigh   = 0xffbcd6e0;
    constexpr uint32_t cBright = 0xffe6f0f4;

    // Pad order matches the iPad toolbar L→R, top→bottom — and the
    // iPad toolbar is itself ordered by FUNCTION-GROUP brightness from
    // faintest to brightest, so reading the panel left→right takes you
    // through the OLED-cyan tiers in order:
    //   Arp     (Arp, Mode, Rate, Oct)   → cFaint   [positions 1..4]
    //   Timing  (Grid, Quant)            → cDim     [positions 5,6]
    //   Project (Save, Load, Undo, Redo) → cMid     [positions 7..10]
    //   Clip    (New, Del, Split, Edit)  → cAccent  [positions 11..14]
    //   I/O     (Capture, Export)        → cHigh    [positions 15,16]
    constexpr LkPadEntry kPadMap[] = {
        //  row col  pal   light       dark
        // Top row — iPad positions 1..8 (Arp → Timing → Project start)
        { 0, 0, 0x39, jMagenta, cFaint  },   // Arp     [Arp]
        { 0, 1, 0x3D, jPink,    cFaint  },   // Mode    [Arp]
        { 0, 2, 0x53, jCoral,   cFaint  },   // Rate    [Arp]
        { 0, 3, 0x57, jOrange,  cFaint  },   // Oct     [Arp]
        { 0, 4, 0x09, jAmber,   cDim    },   // Grid    [Timing]
        { 0, 5, 0x0D, jYellow,  cDim    },   // Quant   [Timing]
        { 0, 6, 0x35, jPurple,  cMid    },   // Save    [Project]
        { 0, 7, 0x37, jViolet,  cMid    },   // Load    [Project]
        // Bottom row — iPad positions 9..16 (Project end → Clip → I/O)
        { 1, 0, 0x21, jSky,     cMid    },   // Undo    [Project]
        { 1, 1, 0x25, jCyan,    cMid    },   // Redo    [Project]
        { 1, 2, 0x15, jGreen,   cAccent },   // New     [Clip]
        { 1, 3, 0x05, jRed,     cAccent },   // Delete  [Clip]
        { 1, 4, 0x57, jOrange,  cAccent },   // Split   [Clip]
        { 1, 5, 0x29, jBlue,    cAccent },   // Edit    [Clip]
        { 1, 6, 0x11, jLime,    cHigh   },   // Capture [I/O]
        { 1, 7, 0x19, jSpring,  cHigh   },   // Export  [I/O]
    };
    constexpr int kPadMapCount = sizeof(kPadMap) / sizeof(kPadMap[0]);

    const LkPadEntry* findPadEntry(int row, int col) {
        for (int i = 0; i < kPadMapCount; ++i)
            if (kPadMap[i].row == row && kPadMap[i].col == col) return &kPadMap[i];
        return nullptr;
    }
}

bool MainComponent::controllerToolbarPadActive() const
{
    const auto t = themeManager.getCurrentTheme();
    return (t == ThemeManager::Launchkey || t == ThemeManager::LaunchkeyDark || t == ThemeManager::LaunchkeyOled);
}

void MainComponent::controllerToolbarPadAction(int row, int col)
{
    if (!controllerToolbarPadActive()) return;
    juce::Component::SafePointer<MainComponent> safe(this);
    juce::MessageManager::callAsync([safe, row, col] {
        auto* self = safe.getComponent();
        if (!self) return;
        // Layout matches the brightness-gradient kPadMap above:
        //   Top : Arp Mode Rate Oct  | Grid Quant | Save Load
        //   Bot : Undo Redo          | New Del Split Edit | Capt Expt
        if (row == 0)
        {
            switch (col)
            {
                case 0: self->arpButton.triggerClick();     break;
                case 1: self->arpModeButton.triggerClick(); break;
                case 2: self->arpRateButton.triggerClick(); break;
                case 3: self->arpOctButton.triggerClick();  break;
                case 4: {   // Grid — advance dropdown to the next item.
                    auto& g = self->gridSelector;
                    const int n = g.getNumItems();
                    if (n <= 0) return;
                    int currentIdx = g.getSelectedItemIndex();
                    int nextIdx = (currentIdx + 1) % n;
                    g.setSelectedItemIndex(nextIdx, juce::sendNotificationSync);
                    break;
                }
                case 5: self->quantizeButton.triggerClick(); break;
                case 6: self->saveButton.triggerClick();     break;
                case 7: self->loadButton.triggerClick();     break;
            }
        }
        else if (row == 1)
        {
            switch (col)
            {
                case 0: self->undoButton.triggerClick();       break;
                case 1: self->redoButton.triggerClick();       break;
                case 2: self->newClipButton.triggerClick();    break;
                case 3: self->deleteClipButton.triggerClick(); break;
                case 4: self->splitClipButton.triggerClick();  break;
                case 5: self->editClipButton.triggerClick();   break;
                case 6: self->captureButton.triggerClick();    break;
                case 7: self->exportButton.triggerClick();     break;
            }
        }
    });
}

uint8_t MainComponent::controllerToolbarPadColor(int row, int col) const
{
    if (!controllerToolbarPadActive()) return 0;
    auto* e = findPadEntry(row, col);
    return e ? e->pal : 0;
}

uint32_t MainComponent::controllerToolbarPadColorRGB(int row, int col) const
{
    if (!controllerToolbarPadActive()) return 0;
    auto* e = findPadEntry(row, col);
    if (!e) return 0;
    // OLED theme: every pad emits the single selected hue so the
    // hardware LEDs match the on-screen monochrome canvas.
    if (themeManager.getCurrentTheme() == ThemeManager::LaunchkeyOled)
    {
        if (auto* oled = dynamic_cast<const LaunchkeyOledLookAndFeel*>(themeManager.getLookAndFeel()))
            return oled->getOledColour() & 0x00FFFFFFu;
    }
    const bool dark = (themeManager.getCurrentTheme() == ThemeManager::LaunchkeyDark);
    // Strip alpha; controller side sends only 24-bit RGB.
    return (dark ? e->rgbDark : e->rgbLight) & 0x00FFFFFFu;
}

void MainComponent::syncLaunchkeyDeviceModes()
{
    if (!launchkey.isActive()) return;

    // Pad mode: DRUM (1) when the focused track hosts a drum-kit
    // sampler, otherwise back to DAW/session (2) so our toolbar
    // shortcuts work.  Encoder mode: PLUGIN (0) when the focused
    // track has a plugin loaded (so the OLED label reads "PLUGIN"
    // when the user touches a knob), otherwise MIXER (1).
    auto& trk = pluginHost.getTrack(selectedTrackIndex);

    bool isDrumKit = false;
    if (trk.plugin != nullptr)
    {
        if (auto* sampler = dynamic_cast<BuiltinSamplerProcessor*>(trk.plugin))
            isDrumKit = sampler->isDrumKitMode();
    }

    const uint8_t padMode = isDrumKit ? 1 : 2;
    const uint8_t encMode = (trk.plugin != nullptr) ? 0 : 1;
    launchkey.setDevicePadMode(padMode);
    launchkey.setDeviceEncoderMode(encMode);
}

void MainComponent::controllerSetToolbarButtonAnimatedColor(int row, int col, uint32_t rgb)
{
    if (!controllerToolbarPadActive()) return;
    juce::Component* target = nullptr;
    // Reverse-lookup table mirroring kPadMap → button bindings in
    // applyLaunchkeyToolbarColors().  Grid is the only ComboBox.
    if (row == 0)
    {
        switch (col) {
            case 0: target = &arpButton;          break;
            case 1: target = &arpModeButton;      break;
            case 2: target = &arpRateButton;      break;
            case 3: target = &arpOctButton;       break;
            case 4: target = &gridButton;         break;
            case 5: target = &quantizeButton;     break;
            case 6: target = &saveButton;         break;
            case 7: target = &loadButton;         break;
        }
    }
    else if (row == 1)
    {
        switch (col) {
            case 0: target = &undoButton;         break;
            case 1: target = &redoButton;         break;
            case 2: target = &newClipButton;      break;
            case 3: target = &deleteClipButton;   break;
            case 4: target = &splitClipButton;    break;
            case 5: target = &editClipButton;     break;
            case 6: target = &captureButton;      break;
            case 7: target = &exportButton;       break;
        }
    }
    if (target == nullptr) return;
    // 0xFF000000 alpha so JUCE renders solid; LaF reads the property.
    const uint32_t packed = 0xFF000000u | (rgb & 0x00FFFFFFu);
    const auto cur = (uint32_t)(int) target->getProperties().getWithDefault("lkColor", 0);
    if (cur == packed) return;
    target->getProperties().set("lkColor", static_cast<int>(packed));
    target->repaint();
}

bool   MainComponent::controllerEngineIsPlaying()    const { return pluginHost.getEngine().isPlaying(); }
bool   MainComponent::controllerEngineIsRecording()  const { return pluginHost.getEngine().isRecording(); }
double MainComponent::controllerEngineBeatPosition() const { return pluginHost.getEngine().getPositionInBeats(); }
double MainComponent::controllerEngineBpm()          const { return pluginHost.getEngine().getBpm(); }

void MainComponent::applyLaunchkeyToolbarColors()
{
    // (button*, kPadMap row, kPadMap col)
    struct Tgt { juce::TextButton* btn; int row, col; };
    const Tgt tgts[] = {
        // Top row — Arp(0..3) | Grid + Quant(4,5) | Save + Load(6,7).
        // Grid is the ComboBox at (0,4), handled separately below.
        { &arpButton,         0, 0 },
        { &arpModeButton,     0, 1 },
        { &arpRateButton,     0, 2 },
        { &arpOctButton,      0, 3 },
        { &gridButton,        0, 4 },
        { &quantizeButton,    0, 5 },
        { &saveButton,        0, 6 },
        { &loadButton,        0, 7 },
        // Bottom row — Undo Redo | New Del Split Edit | Capt Expt
        { &undoButton,        1, 0 },
        { &redoButton,        1, 1 },
        { &newClipButton,     1, 2 },
        { &deleteClipButton,  1, 3 },
        { &splitClipButton,   1, 4 },
        { &editClipButton,    1, 5 },
        { &captureButton,     1, 6 },
        { &exportButton,      1, 7 },
    };

    if (!controllerToolbarPadActive())
    {
        // Clear the LaF hint property so the active theme's
        // defaults take over again.  Repaint to trigger redraw.
        for (const auto& t : tgts)
        {
            t.btn->getProperties().remove("lkColor");
            t.btn->repaint();
        }
        return;
    }

    // Tag each toolbar control with an "lkColor" hint that the
    // Launchkey LaFs read in drawButtonBackground / drawComboBox.
    // setColour() doesn't work because those LaFs don't honor
    // per-button colourId overrides.
    const bool dark = (themeManager.getCurrentTheme() == ThemeManager::LaunchkeyDark
                       || themeManager.getCurrentTheme() == ThemeManager::LaunchkeyOled);
    // OLED theme: collapse every per-button hue to the single
    // currently-selected OLED palette colour so the on-screen
    // toolbar matches the hardware pads.
    uint32_t oledOverride = 0;
    if (themeManager.getCurrentTheme() == ThemeManager::LaunchkeyOled)
        if (auto* oled = dynamic_cast<LaunchkeyOledLookAndFeel*>(themeManager.getLookAndFeel()))
            oledOverride = oled->getOledColour();
    for (const auto& t : tgts)
    {
        if (auto* e = findPadEntry(t.row, t.col))
        {
            const uint32_t c = oledOverride != 0 ? oledOverride
                                                 : (dark ? e->rgbDark : e->rgbLight);
            t.btn->getProperties().set("lkColor", static_cast<int>(c));
            t.btn->repaint();
        }
    }
    // Arm the iPad-side boot wave when the wireframe theme just
    // activated AND no Launchkey is connected to do it for us.
    const bool wfNow = dark;
    if (wfNow && !wireframeWasActiveForBoot && !launchkey.isActive())
        ipadToolbarBootStartMs = juce::Time::currentTimeMillis();
    wireframeWasActiveForBoot = wfNow;
}

// Per-button boot wave on the iPad — 16 steps × 40ms each = 640ms
// total, sweeping L→R across the top row then continuing L→R across
// the bottom row.  Mirrors the device-side boot wave implemented in
// the controller's tick.  Only runs while ipadToolbarBootStartMs is
// set (cleared after the wave finishes).
void MainComponent::updateIpadToolbarBootWave()
{
    if (ipadToolbarBootStartMs == 0) return;

    const auto now = juce::Time::currentTimeMillis();
    const auto since = now - ipadToolbarBootStartMs;
    if (since >= 800)
    {
        // Wave done — restore static base colours and clear arm.
        ipadToolbarBootStartMs = 0;
        applyLaunchkeyToolbarColors();
        return;
    }

    const int waveStrip = static_cast<int>(since / 40);   // 16 steps over ~640ms

    struct Tgt { juce::Component* btn; int row, col; };
    const Tgt tgts[] = {
        { &arpButton,         0, 0 }, { &arpModeButton,     0, 1 },
        { &arpRateButton,     0, 2 }, { &arpOctButton,      0, 3 },
        { &gridSelector,      0, 4 }, { &quantizeButton,    0, 5 },
        { &saveButton,        0, 6 }, { &loadButton,        0, 7 },
        { &undoButton,        1, 0 }, { &redoButton,        1, 1 },
        { &newClipButton,     1, 2 }, { &deleteClipButton,  1, 3 },
        { &splitClipButton,   1, 4 }, { &editClipButton,    1, 5 },
        { &captureButton,     1, 6 }, { &exportButton,      1, 7 },
    };
    // Use the iPad-visual order from the array (sweep follows the
    // L→R toolbar order, not the device-row order — this stays
    // correct even when device rows are remapped).
    for (int i = 0; i < (int) (sizeof(tgts) / sizeof(tgts[0])); ++i)
    {
        const auto& t = tgts[i];
        uint32_t color;
        if (i > waveStrip)        color = 0xff000000u;   // dark — wave hasn't passed yet
        else if (i == waveStrip)  color = 0xffffffffu;   // crest white
        else if (auto* e = findPadEntry(t.row, t.col))   // already passed → base
            color = 0xff000000u | (e->rgbDark & 0x00FFFFFFu);
        else
            continue;
        t.btn->getProperties().set("lkColor", static_cast<int>(color));
        t.btn->repaint();
    }
}

void MainComponent::controllerScrubPlayhead(int delta)
{
    // Step the playhead by `delta` encoder units (signed).  Each unit
    // is a 16th note so a single click feels musical and a fast
    // full-sweep covers ~8 beats.  Engine position is a thread-safe
    // atomic but the timeline repaint needs the message thread.
    onMain(this, [delta](MainComponent* self) {
        auto& eng = self->pluginHost.getEngine();
        const double step = 0.0625;   // beats per encoder unit
        const double next = juce::jmax(0.0, eng.getPositionInBeats() + delta * step);
        eng.setPosition(next);
        if (self->timelineComponent) self->timelineComponent->repaint();
    });
}

void MainComponent::controllerPresetPrev()
{
    juce::Component::SafePointer<MainComponent> safe(this);
    juce::MessageManager::callAsync([safe] {
        if (safe) safe->presetDownButton.triggerClick();
    });
}

void MainComponent::controllerPresetNext()
{
    juce::Component::SafePointer<MainComponent> safe(this);
    juce::MessageManager::callAsync([safe] {
        if (safe) safe->presetUpButton.triggerClick();
    });
}

void MainComponent::controllerToggleMetronomeAndCountIn()
{
    // Shift+Record on the Mini: flip the metronome AND count-in
    // together so they stay locked in step.  Drives the existing
    // toolbar buttons via triggerClick so the engine state, on-
    // screen toggle visuals, and project save state all stay in
    // sync the same way a tap on those buttons would.
    juce::Component::SafePointer<MainComponent> safe(this);
    juce::MessageManager::callAsync([safe] {
        auto* self = safe.getComponent();
        if (!self) return;
        const bool turningOn = !self->metronomeButton.getToggleState();
        if (self->metronomeButton.getToggleState() != turningOn)
            self->metronomeButton.triggerClick();
        if (self->countInButton.getToggleState() != turningOn)
            self->countInButton.triggerClick();
    });
}

void MainComponent::controllerParamPagePrev()
{
    juce::Component::SafePointer<MainComponent> safe(this);
    juce::MessageManager::callAsync([safe] {
        if (safe) safe->paramPageLeft.triggerClick();
    });
}

void MainComponent::controllerParamPageNext()
{
    juce::Component::SafePointer<MainComponent> safe(this);
    juce::MessageManager::callAsync([safe] {
        if (safe) safe->paramPageRight.triggerClick();
    });
}

void MainComponent::setFocusedTrackVolumeFromController(float value)
{
    // Marshal to the message thread so the on-screen volume slider
    // can be re-synced — the audio-side volume atomic could be set
    // from any thread, but JUCE Slider::setValue must be on MT.
    juce::Component::SafePointer<MainComponent> safe(this);
    juce::MessageManager::callAsync([safe, value] {
        auto* self = safe.getComponent();
        if (!self) return;
        const float clamped = juce::jlimit(0.0f, 1.0f, value);
        auto& trk = self->pluginHost.getTrack(self->selectedTrackIndex);
        if (trk.gainProcessor) trk.gainProcessor->volume.store(clamped);
        self->volumeSlider.setValue(clamped, juce::dontSendNotification);
    });
}

void MainComponent::controllerSetParamBySliderIndex(int sliderIdx, float value)
{
    // Routes the encoder to the plugin param mapped to slider position
    // sliderIdx — i.e. the visual order on the iPad, not the plugin's
    // internal param order.  The slider's "paramIndex" property is
    // updated by updateParamSliders() whenever the focused track or
    // param page changes, so this stays in sync automatically.
    juce::Component::SafePointer<MainComponent> safe(this);
    juce::MessageManager::callAsync([safe, sliderIdx, value] {
        auto* self = safe.getComponent();
        if (!self) return;
        if (sliderIdx < 0 || sliderIdx >= self->paramSliders.size()) return;
        auto* slider = self->paramSliders[sliderIdx];
        const int paramIdx = static_cast<int>(slider->getProperties().getWithDefault("paramIndex", -1));
        if (paramIdx < 0) return;   // slider not bound to a plugin param right now

        auto& trk = self->pluginHost.getTrack(self->selectedTrackIndex);
        if (trk.plugin == nullptr) return;
        auto& params = trk.plugin->getParameters();
        if (paramIdx >= params.size()) return;

        const float clamped = juce::jlimit(0.0f, 1.0f, value);
        params[paramIdx]->setValue(clamped);
        trk.touchedParamIndex.store(paramIdx);
        trk.touchedParamTime.store(static_cast<int64_t>(juce::Time::getMillisecondCounter()));
        slider->setValue(clamped, juce::dontSendNotification);
        self->paramPageNameLabel.setText(params[paramIdx]->getName(50), juce::dontSendNotification);
        self->highlightParamKnob(sliderIdx);

        // Mirror the slider's onValueChange automation-recording
        // path so encoder twists are captured to the lane the same
        // way direct touches are.
        auto& eng = self->pluginHost.getEngine();
        if (eng.isPlaying() && eng.isRecording() && !eng.isInCountIn())
        {
            const juce::SpinLock::ScopedLockType lock(trk.automationLock);
            AutomationLane* lane = nullptr;
            for (auto* l : trk.automationLanes)
                if (l->parameterIndex == paramIdx) { lane = l; break; }
            if (lane == nullptr)
            {
                lane = new AutomationLane();
                lane->parameterIndex = paramIdx;
                lane->parameterName = params[paramIdx]->getName(20);
                trk.automationLanes.add(lane);
            }
            AutomationPoint pt;
            pt.beat = eng.getPositionInBeats();
            pt.value = clamped;
            for (int j = lane->points.size() - 1; j >= 0; --j)
                if (std::abs(lane->points[j].beat - pt.beat) < 0.01)
                    lane->points.remove(j);
            lane->points.add(pt);
            std::sort(lane->points.begin(), lane->points.end(),
                [](const AutomationPoint& a, const AutomationPoint& b) { return a.beat < b.beat; });
        }
    });
}

// ─── Per-device MIDI output ───────────────────────────────────────
//
// Substring-matched against installed MIDI output device names.  First
// match wins; result is cached so subsequent calls are cheap.  Used by
// controller integrations (Launchkey MK4, Push, etc.) to send LED /
// SysEx feedback to a SPECIFIC endpoint without colliding with the
// global midiOutput slot reserved for MIDI 2.0 CI replies.
juce::MidiOutput* MainComponent::outputForDevice(const juce::String& nameContains)
{
    auto it = deviceOutputs.find(nameContains);
    if (it != deviceOutputs.end()) return it->second.get();

    auto avail = juce::MidiOutput::getAvailableDevices();
    // Diagnostic — print the full list so we can see what iOS is
    // exposing if our substring match misses.
    juce::String all;
    for (auto& dev : avail) all << "  - " << dev.name << "\n";
    DBG("MIDI outputs (looking for \"" << nameContains << "\"):\n" << all);

    // Prefer a port whose name contains BOTH the requested hint and
    // "DAW" (Launchkey exposes a separate DAW protocol port).  Fall
    // back to the first plain match so other device integrations work.
    for (auto& dev : avail)
        if (dev.name.containsIgnoreCase(nameContains) && dev.name.containsIgnoreCase("DAW"))
        {
            auto out = juce::MidiOutput::openDevice(dev.identifier);
            auto* raw = out.get();
            DBG("Opened DAW port: " << dev.name);
            deviceOutputs.emplace(nameContains, std::move(out));
            return raw;
        }
    for (auto& dev : avail)
        if (dev.name.containsIgnoreCase(nameContains))
        {
            auto out = juce::MidiOutput::openDevice(dev.identifier);
            auto* raw = out.get();
            DBG("Opened plain port: " << dev.name);
            deviceOutputs.emplace(nameContains, std::move(out));
            return raw;
        }
    DBG("No MIDI output matched \"" << nameContains << "\"");
    return nullptr;
}
