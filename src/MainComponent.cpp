#include "MainComponent.h"
#if JUCE_IOS
#include "AUScanner.h"
#endif
#if JUCE_IOS || JUCE_MAC
#include <mach/mach.h>
#endif

MainComponent::MainComponent()
{
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
        if (eng.isPlaying())
        {
            eng.stop();
            for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
            {
                auto* cp = pluginHost.getTrack(t).clipPlayer;
                if (cp) cp->sendAllNotesOff.store(true);
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

    addAndMakeVisible(clearAutoButton);
    clearAutoButton.onClick = [this] {
        takeSnapshot();
        auto& track = pluginHost.getTrack(selectedTrackIndex);
        track.automationLanes.clear();
        statusLabel.setText("Automation cleared", juce::dontSendNotification);
        if (timelineComponent) timelineComponent->repaint();
    };

    addAndMakeVisible(gridSelector);
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
                track.clipPlayer->sendAllNotesOff.store(true);
        }
        statusLabel.setText("MIDI Panic - all notes off", juce::dontSendNotification);
        panicAnimEndTime = juce::Time::getMillisecondCounterHiRes() * 0.001 + 3.0;
    };

    // ── Capture Button ──
    addAndMakeVisible(captureButton);
    captureButton.onClick = [this] { performCapture(); };

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
                new PianoRollWindow("Piano Roll", *clip, pluginHost.getEngine());
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
        paramPageOffset = juce::jmax(0, paramPageOffset - NUM_PARAM_SLIDERS);
        updateParamSliders();
    };
    paramPageRight.onClick = [this] {
        auto& track = pluginHost.getTrack(selectedTrackIndex);
        if (track.plugin != nullptr)
        {
            int total = track.plugin->getParameters().size();
            if (paramPageOffset + NUM_PARAM_SLIDERS < total)
                paramPageOffset += NUM_PARAM_SLIDERS;
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
        projectorMode = visualizerFullScreen;  // always projector mode
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
    visSelector.setSelectedId(2, juce::dontSendNotification);  // Lissajous
    currentVisMode = 1;
    visSelector.onChange = [this] {
        currentVisMode = visSelector.getSelectedId() - 1;
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
            visualizerFullScreen = true;
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
    saveButton.onClick = [this] { saveProject(); };

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
        themeSelector.addItem(ThemeManager::getThemeName(static_cast<ThemeManager::Theme>(i)), i + 1);
    themeSelector.setSelectedId(ThemeManager::Ioniq + 1, juce::dontSendNotification);
    themeSelector.onChange = [this] {
        auto idx = themeSelector.getSelectedId() - 1;
        if (idx >= 0 && idx < ThemeManager::NumThemes)
        {
            themeManager.setTheme(static_cast<ThemeManager::Theme>(idx), this);
            applyThemeToControls();
            panelBlurImage = juce::Image();
            panelBlurUpdateCounter = 8;
            // Start/stop accelerometer based on theme
            if (idx == ThemeManager::LiquidGlass)
                DeviceMotion::getInstance().start();
            else
                DeviceMotion::getInstance().stop();
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

            // Show full param name and highlight this knob
            paramPageNameLabel.setText(params[realIdx]->getName(50), juce::dontSendNotification);
            highlightParamKnob(paramIdx);

            // Record automation if transport is playing + recording
            auto& eng = pluginHost.getEngine();
            if (eng.isPlaying() && eng.isRecording() && !eng.isInCountIn())
            {
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
                lane->points.add(pt);
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
    addAndMakeVisible(*arrangerMinimap);

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
    juce::MessageManager::callAsync([this] { resized(); repaint(); });

    startTimerHz(15);
}

MainComponent::~MainComponent()
{
    pluginHost.spectrumDisplay = nullptr;
    pluginHost.waveTerrainDisplay = nullptr;
    pluginHost.geissDisplay = nullptr;
    pluginHost.projectMDisplay = nullptr;
    // Clear Lissajous pointer from all tracks
    for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
    {
        auto& track = pluginHost.getTrack(t);
        if (track.gainProcessor) track.gainProcessor->lissajousDisplay = nullptr;
    }
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
            startTimerHz(15);
        }
        resized();
        repaint();
    }

    // When running at 60Hz for animation, only run the rest at ~15Hz to keep
    // flash counters, CPU polling, etc. at their normal rate
    if (panelAnimating)
    {
        if (++panelAnimFrameSkip < 4) return;  // skip 3 of every 4 frames
        panelAnimFrameSkip = 0;
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

    // Liquid Glass: trigger button/slider repaint for tilt-reactive specular
    if (themeManager.getCurrentTheme() == ThemeManager::LiquidGlass)
    {
        auto& dm = DeviceMotion::getInstance();
        if (!dm.isRunning())
            dm.start();  // ensure motion is running

        auto tilt = dm.getTilt();

        // DEBUG: show tilt values in status label so we can verify on device
        statusLabel.setText("TILT x:" + juce::String(tilt.x, 2) +
                           " y:" + juce::String(tilt.y, 2) +
                           (dm.isRunning() ? " [ON]" : " [OFF]"),
                           juce::dontSendNotification);

        // Repaint all visible buttons and sliders
        std::function<void(juce::Component*)> repaintControls = [&](juce::Component* comp) {
            if (comp->isVisible())
            {
                if (dynamic_cast<juce::Button*>(comp) || dynamic_cast<juce::Slider*>(comp))
                    comp->repaint();
                for (int i = 0; i < comp->getNumChildComponents(); ++i)
                    repaintControls(comp->getChildComponent(i));
            }
        };
        repaintControls(this);
    }

    auto& eng = pluginHost.getEngine();

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

    // Update CPU + RAM meter
    {
        // Use audio device CPU usage — measures actual audio callback load
        float totalCpu = static_cast<float>(deviceManager.getCpuUsage() * 100.0);

        // Get RAM usage
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

        // Store CPU history for heartbeat waveform
        cpuHistory.add(totalCpu);
        if (cpuHistory.size() > 60)
            cpuHistory.remove(0);

        // Advance EKG sweep — generate PQRST samples into circular buffer
        {
            float cpu = totalCpu / 100.0f;
            // Heart rate: 60 BPM at 0% CPU → 180 BPM at 100% CPU
            // Timer runs at 15 Hz, 3 sub-samples per tick = 45 samples/sec
            // 60 BPM = 1 cycle/sec = 1/45 phase per sample
            // 180 BPM = 3 cycles/sec = 3/45 phase per sample
            double beatsPerSec = (60.0 + static_cast<double>(cpu) * 120.0) / 60.0;
            double phaseStep = beatsPerSec / 45.0;

            // Generate a few samples per timer tick for smooth sweep
            for (int s = 0; s < 3; ++s)
            {
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
        }

        // Repaint the CPU OLED area
        if (cpuLabel.isVisible())
            repaint(cpuLabel.getBounds().expanded(5));
    }

    // Heart rate observation is started automatically by the auth completion handler
    // No need to poll here

    // Update arranger minimap
    if (arrangerMinimap && timelineComponent && timelineComponent->isVisible())
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

    int id = 2;
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
    deviceManager.setMidiInputDeviceEnabled(d.identifier, true);
    // Route through our callback so we can intercept CI SysEx
    deviceManager.addMidiInputDeviceCallback(d.identifier, this);
    currentMidiDeviceId = d.identifier;
    updateStatusLabel();
}

void MainComponent::handleIncomingMidiMessage(juce::MidiInput* /*source*/, const juce::MidiMessage& msg)
{
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
        juce::MessageManager::callAsync([this, ch, cc, val] {
            processMidiLearnCC(ch, cc, val);
        });
        return;
    }

    // Apply learned MIDI mappings
    if (msg.isController())
    {
        int ch = msg.getChannel();
        int cc = msg.getControllerNumber();
        int val = msg.getControllerValue();

        for (auto& mapping : midiMappings)
        {
            if (mapping.channel == ch && mapping.ccNumber == cc)
            {
                juce::MessageManager::callAsync([this, mapping, val] {
                    applyMidiCC(mapping, val);
                });
            }
        }
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

            juce::MessageManager::callAsync([this, ciInfo, outCount] {
                trackNameLabel.setText(ciInfo + " sent:" + juce::String(outCount),
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
    startTimerHz(60); // boost frame rate for smooth animation
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
    juce::DialogWindow::LaunchOptions opt;
    opt.content.setOwned(sel);
    opt.dialogTitle = "Audio Settings";
    opt.componentToCentreAround = this;
    opt.dialogBackgroundColour = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);
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
                juce::DialogWindow::LaunchOptions opts;
                opts.content.setOwned(dialog);
                opts.dialogTitle = "Software Update";
                opts.componentToCentreAround = this;
                opts.dialogBackgroundColour = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);
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
    if (paramPageOffset >= total) paramPageOffset = juce::jmax(0, total - NUM_PARAM_SLIDERS);
    if (paramPageOffset < 0) paramPageOffset = 0;

    // Update page label
    int totalPages = juce::jmax(1, (total + NUM_PARAM_SLIDERS - 1) / NUM_PARAM_SLIDERS);
    int page = (paramPageOffset / NUM_PARAM_SLIDERS) + 1;
    paramPageLabel.setText(juce::String(page) + "/" + juce::String(totalPages), juce::dontSendNotification);

    // Show plugin name in the page name label
    juce::String plugName = track.plugin->getName();
    paramPageNameLabel.setText(plugName, juce::dontSendNotification);

    // ── Page 1: smart selection (plugin-specific + macros + common names) ──
    if (paramPageOffset == 0)
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
                if (selectedParams.size() >= NUM_PARAM_SLIDERS) break;
            }
        }

        // Generic: macros
        if (selectedParams.isEmpty())
            for (auto* param : allParams)
            {
                juce::String name = param->getName(30).toLowerCase();
                if (name.contains("macro") || name.contains("mcr") || name.contains("assign"))
                    selectedParams.add(param);
                if (selectedParams.size() >= NUM_PARAM_SLIDERS) break;
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
                    { selectedParams.add(param); break; }
        }

        // Fill remaining with first available
        for (int i = 0; i < allParams.size() && selectedParams.size() < NUM_PARAM_SLIDERS; ++i)
            if (!selectedParams.contains(allParams[i]))
                selectedParams.add(allParams[i]);

        for (int i = 0; i < NUM_PARAM_SLIDERS; ++i)
        {
            if (i < selectedParams.size())
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
        if (paramIdx < total)
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
            // Also wrap the paired note-off to keep the pair together
            if (evt->message.isNoteOn() && evt->noteOffObject != nullptr)
                evt->noteOffObject->message.setTimeStamp(
                    evt->noteOffObject->message.getTimeStamp() + shift);
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

    updateTrackDisplay();
    if (timelineComponent) timelineComponent->repaint();
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

        // Stop recording to prevent data race on clip events
        auto& eng = pluginHost.getEngine();
        if (eng.isRecording()) eng.toggleRecord();

        auto xml = std::make_unique<juce::XmlElement>("SequencerProject");
        xml->setAttribute("bpm", eng.getBpm());
        xml->setAttribute("loopEnabled", eng.isLoopEnabled());
        xml->setAttribute("loopStart", eng.getLoopStart());
        xml->setAttribute("loopEnd", eng.getLoopEnd());
        xml->setAttribute("metronome", eng.isMetronomeOn());
        xml->setAttribute("selectedTrack", selectedTrackIndex);
        xml->setAttribute("theme", static_cast<int>(themeManager.getCurrentTheme()));

        for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
        {
            auto& track = pluginHost.getTrack(t);

            auto* trackXml = xml->createNewChildElement("Track");
            trackXml->setAttribute("index", t);

            if (track.gainProcessor)
            {
                trackXml->setAttribute("volume", static_cast<double>(track.gainProcessor->volume.load()));
                trackXml->setAttribute("pan", static_cast<double>(track.gainProcessor->pan.load()));
                trackXml->setAttribute("muted", track.gainProcessor->muted.load());
                trackXml->setAttribute("soloed", track.gainProcessor->soloed.load());
            }

            // Save plugin description and state
            if (track.plugin)
            {
                auto* pluginXml = trackXml->createNewChildElement("Plugin");
                // Find matching description from known list
                for (const auto& desc : pluginHost.getPluginList().getTypes())
                {
                    if (desc.name == track.plugin->getName() && desc.isInstrument)
                    {
                        pluginXml->addChildElement(desc.createXml().release());
                        break;
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
                    for (const auto& desc : pluginHost.getPluginList().getTypes())
                    {
                        if (desc.name == track.fxSlots[fx].processor->getName() && !desc.isInstrument)
                        {
                            fxXml->addChildElement(desc.createXml().release());
                            break;
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

        xml->writeTo(file);
        statusLabel.setText("Saved: " + file.getFileName(), juce::dontSendNotification);
    });
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

        // Stop transport to prevent race with audio thread
        auto& eng = pluginHost.getEngine();
        if (eng.isPlaying()) eng.stop();
        if (eng.isRecording()) eng.toggleRecord();

        auto xml = juce::parseXML(file);

        if (xml == nullptr || !xml->hasTagName("SequencerProject"))
        {
            statusLabel.setText("Invalid project file", juce::dontSendNotification);
            return;
        }

        // Clear all tracks first
        for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
        {
            pluginHost.unloadPlugin(t);
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
            }
        }

        // First pass: restore mixer settings and clips only (no plugin loading)
        for (auto* trackXml : xml->getChildWithTagNameIterator("Track"))
        {
            int t = trackXml->getIntAttribute("index", -1);
            if (t < 0 || t >= PluginHost::NUM_TRACKS) continue;

            auto& track = pluginHost.getTrack(t);

            if (track.gainProcessor)
            {
                track.gainProcessor->volume.store(static_cast<float>(trackXml->getDoubleAttribute("volume", 0.8)));
                track.gainProcessor->pan.store(static_cast<float>(trackXml->getDoubleAttribute("pan", 0.0)));
                track.gainProcessor->muted.store(trackXml->getBoolAttribute("muted", false));
                track.gainProcessor->soloed.store(trackXml->getBoolAttribute("soloed", false));
            }

#if !JUCE_IOS
            // Desktop: load plugins synchronously in-line
            auto* pluginXml = trackXml->getChildByName("Plugin");
            if (pluginXml != nullptr)
            {
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
                                pluginHost.getTrack(t).plugin->setStateInformation(
                                    state.getData(), static_cast<int>(state.getSize()));
                            }
                        }
                        break;
                    }
                }
            }
            for (auto* fxXml : trackXml->getChildWithTagNameIterator("FX"))
            {
                int fxSlot = fxXml->getIntAttribute("slot", -1);
                if (fxSlot < 0 || fxSlot >= Track::NUM_FX_SLOTS) continue;
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
                                pluginHost.getTrack(t).fxSlots[fxSlot].processor->setStateInformation(
                                    state.getData(), static_cast<int>(state.getSize()));
                            }
                            pluginHost.setFxBypassed(t, fxSlot, fxXml->getBoolAttribute("bypassed", false));
                        }
                        break;
                    }
                }
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

                int numSamp = static_cast<int>(audioData.getSize() / (numCh * sizeof(float)));
                if (numSamp > 0)
                {
                    slot.audioClip->samples.setSize(numCh, numSamp);
                    const float* ptr = static_cast<const float*>(audioData.getData());
                    for (int si = 0; si < numSamp; ++si)
                        for (int ch = 0; ch < numCh; ++ch)
                            slot.audioClip->samples.setSample(ch, si, *ptr++);
                }

                slot.state.store(ClipSlot::Playing);
            }

            // Load automation lanes
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

#if JUCE_IOS
        // On iOS, load plugins asynchronously after the file callback returns
        // to avoid run loop reentrancy crashes during AUv3 async instantiation
        auto xmlCopy = std::make_shared<juce::XmlElement>(*xml);
        auto fileName = file.getFileName();
        juce::MessageManager::callAsync([this, xmlCopy, fileName] {
            audioPlayer.setProcessor(nullptr);
            for (auto* trackXml : xmlCopy->getChildWithTagNameIterator("Track"))
            {
                int t = trackXml->getIntAttribute("index", -1);
                if (t < 0 || t >= PluginHost::NUM_TRACKS) continue;

                auto* pluginXml = trackXml->getChildByName("Plugin");
                if (pluginXml != nullptr)
                {
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
                                    auto* plugin = pluginHost.getTrack(t).plugin;
                                    if (plugin != nullptr)
                                        plugin->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
                                }
                            }
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
                                    if (proc != nullptr)
                                        proc->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
                                }
                                pluginHost.setFxBypassed(t, fxSlot, fxXml->getBoolAttribute("bypassed", false));
                            }
                            break;
                        }
                    }
                }
            }
            audioPlayer.setProcessor(&pluginHost);
            updateTrackDisplay();
            if (timelineComponent) timelineComponent->repaint();
            statusLabel.setText("Loaded: " + fileName, juce::dontSendNotification);
            takeSnapshot();
        });
#else
        // Desktop: load plugins synchronously (already in the loop above — not reached on iOS)
#endif

        updateTrackDisplay();
        if (timelineComponent) timelineComponent->repaint();
        statusLabel.setText("Loaded: " + file.getFileName(), juce::dontSendNotification);

        // Take snapshot for undo
        takeSnapshot();
    });
}

// ── Layout ───────────────────────────────────────────────────────────────────

void MainComponent::mouseDown(const juce::MouseEvent& e)
{
    grabKeyboardFocus();

    // Tap on small visualizer to go fullscreen
    if (!visualizerFullScreen)
    {
        auto* src = e.eventComponent;
        if (src == &spectrumDisplay || src == &lissajousDisplay || src == &waveTerrainDisplay
            || src == &shaderToyDisplay || src == &analyzerDisplay || src == &geissDisplay
            || src == &projectMDisplay || src == heartbeatDisplay.get()
            || src == bioResonanceDisplay.get())
        {
            visualizerFullScreen = true;
            projectorMode = true;
            fullscreenButton.setToggleState(true, juce::dontSendNotification);
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
        int panelW = (int)(180.0f * panelSlideProgress);
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
        themeMenu.addItem(50 + i, ThemeManager::getThemeName(static_cast<ThemeManager::Theme>(i)),
                         true, themeSelector.getSelectedId() == i + 1);
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
                resized();
                repaint();
            }
            else if (result == 11) {
                forceIPadLayout = true;
                resized();
                repaint();
            }
            else if (result >= 100 && result <= 105) {
                currentVisMode = result - 100;
                visSelector.setSelectedId(currentVisMode + 1, juce::dontSendNotification);
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
    g.fillAll(juce::Colour(c.body));

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
        // iPhone: draw oak/wood top bar if theme supports it, else solid
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

        g.setColour(juce::Colour(c.accentStripe));
        g.fillRect(0, 0, getWidth(), 2);

        // Bottom bar background
        g.setColour(juce::Colour(c.bodyDark));
        g.fillRect(0, getHeight() - 36, getWidth(), 36);
        g.setColour(juce::Colour(c.border));
        g.drawHorizontalLine(getHeight() - 36, 0, static_cast<float>(getWidth()));
        return;
    }

    if (auto* lnf = dynamic_cast<DawLookAndFeel*>(&getLookAndFeel()))
    {
        if (lnf->getSidePanelWidth() > 0)
        {
            // Custom top bar (e.g. wood grain) — stop before oak strip
            int sidePW = lnf->getSidePanelWidth();
            int rpW = (int)(180.0f * panelSlideProgress);
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

    // Toolbar background
    int oakW = 0;
    if (auto* dlnf = dynamic_cast<DawLookAndFeel*>(&getLookAndFeel()))
        if (dlnf->getSidePanelWidth() > 0)
            oakW = dlnf->getSidePanelWidth();
    int rpW = (int)(180.0f * panelSlideProgress);
    int rightPanelTotal = rpW + oakW + oakW;  // right panel + oak strip + side panel
    int toolbarRight = getWidth() - rightPanelTotal;

    // Paint over right panel area — frosted glass blur for Liquid Glass, solid for others
    if (rpW > 0)
    {
        int rpLeft = getWidth() - (oakW > 0 ? oakW : 0) - rpW;
        auto panelRect = juce::Rectangle<int>(rpLeft, 0, rpW, getHeight());
        panelBoundsCache = panelRect;

        // Check if Liquid Glass theme
        bool isGlassTheme = (themeManager.getCurrentTheme() == ThemeManager::LiquidGlass);

        if (isGlassTheme)
        {
            // Frosted glass panel — translucent dark with blur backdrop
            if (!panelBlurImage.isNull())
            {
                g.drawImage(panelBlurImage, panelRect.toFloat(),
                            juce::RectanglePlacement::stretchToFit);
            }

            // Glass tint — semi-transparent so content bleeds through
            // More transparent during slide animation for dramatic reveal
            float tintAlpha = panelAnimating ? 0.55f : 0.70f;
            g.setColour(juce::Colour(0x000000).withAlpha(tintAlpha));
            g.fillRect(panelRect);

            // Frosted overlay — white noise-like fill for glass texture
            g.setColour(juce::Colours::white.withAlpha(0.04f));
            g.fillRect(panelRect);

            // Left edge — bright glass edge highlight
            g.setColour(juce::Colours::white.withAlpha(0.15f));
            g.drawVerticalLine(rpLeft, 0.0f, (float)getHeight());

            // Top edge highlight
            g.setColour(juce::Colours::white.withAlpha(0.08f));
            g.drawHorizontalLine(0, (float)rpLeft, (float)(rpLeft + rpW));

            // Subtle gradient from left edge inward (glass refraction feel)
            {
                juce::ColourGradient grad(
                    juce::Colours::white.withAlpha(0.08f), (float)rpLeft, 0.0f,
                    juce::Colours::transparentBlack, (float)(rpLeft + 30), 0.0f, false);
                g.setGradientFill(grad);
                g.fillRect(panelRect);
            }
        }
        else
        {
            g.setColour(juce::Colour(c.body));
            g.fillRect(panelRect);
        }
    }

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

    // OLED background behind track tools section in top bar (Track name -> MIX)
    if (trackNameLabel.isVisible() && mixerButton.isVisible())
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

    // Accent stripe at top
    g.setColour(juce::Colour(c.accentStripe));
    g.fillRect(0, 0, getWidth(), 2);

#if JUCE_IOS
    // Draw rectangle around track name label
    if (trackNameLabel.isVisible())
    {
        g.setColour(juce::Colour(c.textSecondary));
        g.drawRect(trackNameLabel.getBounds().expanded(4, 2), 1);
    }
#endif

    // Draw decorative side panels if the theme provides them
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
                int rpW2 = (int)(180.0f * panelSlideProgress);
                int stripX = getWidth() - sidePW - rpW2 - stripW;
                lnf->drawInnerStrip(g, stripX, 0, stripW, getHeight());
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
    // Get button corner radius from theme
    float radius = 0.0f;
    if (auto* lnf = dynamic_cast<DawLookAndFeel*>(&getLookAndFeel()))
        radius = lnf->getButtonRadius();

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

#if JUCE_IOS
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
    int rightPanelW = isPhone ? 0 : (int)(180.0f * panelSlideProgress);

    // ── Top Bar ──
    auto topBar = area.removeFromTop(topBarH).reduced(4, isPhone ? 2 : 10);
    // Trim top bar so it doesn't extend into right panel + oak strip area
    if (!isPhone)
    {
        int oakTrim = 0;
        if (auto* lnf = dynamic_cast<DawLookAndFeel*>(&getLookAndFeel()))
            if (lnf->getSidePanelWidth() > 0)
                oakTrim = lnf->getSidePanelWidth();
        if (rightPanelW + oakTrim > 0)
            topBar.removeFromRight(rightPanelW + oakTrim);
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
        tapTempoButton.setButtonText("Tap");
        tapTempoButton.setBounds(row1.removeFromLeft(r1bw));
        row1.removeFromLeft(gap);
        midiLearnButton.setVisible(true);
        midiLearnButton.setButtonText("Learn");
        midiLearnButton.setBounds(row1.removeFromLeft(r1bw));
        row1.removeFromLeft(gap);
        audioSettingsButton.setVisible(false);

        settingsButton.setVisible(false);

        // ── Row 2: Clip tools, save/load, vis/piano/mixer ──
        // Right side first
        fullscreenButton.setVisible(false);
        pianoToggleButton.setBounds(row2.removeFromRight(42));
        row2.removeFromRight(gap);
        mixerButton.setBounds(row2.removeFromRight(36));
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
        // audioSettingsButton shown in top bar row 1
        themeSelector.setVisible(false);
        projectorButton.setVisible(false);
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

            if (currentVisMode == 0) { spectrumDisplay.setBounds(visArea); spectrumDisplay.setAlpha(1.0f); spectrumDisplay.setVisible(true); }
            else if (currentVisMode == 1) { lissajousDisplay.setBounds(visArea); lissajousDisplay.setVisible(true); }
            else if (currentVisMode == 2) { waveTerrainDisplay.setBounds(visArea); waveTerrainDisplay.setVisible(true); }
            else if (currentVisMode == 3) { geissDisplay.setBounds(visArea); geissDisplay.setVisible(true); }
            else if (currentVisMode == 4) { projectMDisplay.setBounds(visArea); projectMDisplay.setVisible(true); }
            else if (currentVisMode == 5) { analyzerDisplay.setBounds(visArea); analyzerDisplay.setVisible(true); }
            else if (currentVisMode == 6) { heartbeatDisplay->setBounds(visArea); heartbeatDisplay->setVisible(true); }
            else if (currentVisMode == 7) { bioResonanceDisplay->setBounds(visArea); bioResonanceDisplay->setVisible(true); }

            // Bring control bar widgets to front so they're not hidden behind the visualizer
            visExitButton.toFront(false);
            visSelector.toFront(false);
            projectorButton.toFront(false);
            if (currentVisMode == 0) { specDecayBtn.toFront(false); specSensDownBtn.toFront(false); specSensUpBtn.toFront(false); }
            else if (currentVisMode == 1) { lissZoomOutBtn.toFront(false); lissZoomInBtn.toFront(false); lissDotsBtn.toFront(false); }
            else if (currentVisMode == 2) { gfRibbonDownBtn.toFront(false); gfRibbonUpBtn.toFront(false); gfTrailBtn.toFront(false); gfSpeedSelector.toFront(false); }
            else if (currentVisMode == 3) { geissWaveBtn.toFront(false); geissPaletteBtn.toFront(false); geissSceneBtn.toFront(false); geissWaveDownBtn.toFront(false); geissWaveUpBtn.toFront(false); geissWarpLockBtn.toFront(false); geissPalLockBtn.toFront(false); geissSpeedSelector.toFront(false); geissAutoPilotBtn.toFront(false); geissBgBtn.toFront(false); }
            else if (currentVisMode == 4) { pmPrevBtn.toFront(false); pmNextBtn.toFront(false); pmRandBtn.toFront(false); pmLockBtn.toFront(false); pmBgBtn.toFront(false); }
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
    gridSelector.setVisible(true);
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
        auto volArea = rightPanel.removeFromTop(80);
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
            {
#if JUCE_IOS
                area.removeFromBottom(20); // home indicator safe area
#endif
                arrangerMinimap->setBounds(area.removeFromBottom(16));
                arrangerMinimap->setVisible(true);
            }
            if (timelineComponent)
            {
                timelineComponent->setVisibleTracks(4);
                timelineComponent->setVisible(true);
                timelineComponent->setBounds(area);
            }
        }
        return;
    }
#endif
    // Reset to default for iPad/desktop
    if (timelineComponent)
        timelineComponent->setVisibleTracks(8);

    // Hide right panel components when panel is too narrow for content
    bool panelComponentsVisible = !isPhone && panelSlideProgress > 0.25f;

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
    // Guard: only create right panel rect if wide enough to reduce safely
    auto rightPanel = (rightPanelW > 20)
        ? fullArea.removeFromRight(rightPanelW).reduced(8, 4)
        : juce::Rectangle<int>();
    // Still remove from area so the timeline/toolbar don't overlap
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
    auto toolbar = area.removeFromTop(65).reduced(4, 4);
    // Grid + Quantize on the far left
    gridSelector.setBounds(toolbar.removeFromLeft(65));
    toolbar.removeFromLeft(2);
    quantizeButton.setBounds(toolbar.removeFromLeft(70));
    toolbar.removeFromLeft(4);
    newClipButton.setBounds(toolbar.removeFromLeft(95));
    toolbar.removeFromLeft(3);
    deleteClipButton.setBounds(toolbar.removeFromLeft(80));
    toolbar.removeFromLeft(3);
    splitClipButton.setBounds(toolbar.removeFromLeft(55));
    toolbar.removeFromLeft(2);
    editClipButton.setBounds(toolbar.removeFromLeft(75));
    toolbar.removeFromLeft(2);
    clearAutoButton.setBounds(toolbar.removeFromLeft(75));
    toolbar.removeFromLeft(4);
    saveButton.setBounds(toolbar.removeFromLeft(50));
    toolbar.removeFromLeft(2);
    loadButton.setBounds(toolbar.removeFromLeft(50));
    toolbar.removeFromLeft(2);
    undoButton.setBounds(toolbar.removeFromLeft(50));
    toolbar.removeFromLeft(2);
    redoButton.setBounds(toolbar.removeFromLeft(50));
    toolbar.removeFromLeft(6);
    testNoteButton.setVisible(false);
    captureButton.setBounds(toolbar.removeFromLeft(55));
    captureButton.setVisible(true);
    toolbar.removeFromLeft(4);
    loopSetButton.setVisible(false);
    // CPU label — takes remaining toolbar space (expands when panel hidden)
    auto cpuArea = toolbar;
    cpuLabel.setBounds(cpuArea.getX(), captureButton.getY(), cpuArea.getWidth(), captureButton.getHeight());
    cpuLabel.setVisible(true);

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

    // Visualizer display
    auto visPanelArea = rightPanel.removeFromTop(70);
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
                auto fxRow = rightPanel.removeFromTop(30);
                fxEditorButtons[i]->setBounds(fxRow.removeFromRight(32));
                fxRow.removeFromRight(2);
                fxSelectors[i]->setBounds(fxRow);
                rightPanel.removeFromTop(2);
            }
            rightPanel.removeFromTop(2);

            // Preset row moved below param knobs
            rightPanel.removeFromTop(3);
        }
        else
        {
            // MIDI track: plugin selector + preset + MIDI input + FX
            pluginSelector.setVisible(true);
            openEditorButton.setVisible(true);
            midiInputSelector.setVisible(false);  // moved to OLED input selector
            midiRefreshButton.setVisible(false);

            {
                auto pluginRow = rightPanel.removeFromTop(32);
                openEditorButton.setBounds(pluginRow.removeFromRight(32));
                openEditorButton.setButtonText("E");
                pluginRow.removeFromRight(2);
                pluginSelector.setBounds(pluginRow);
            }
            rightPanel.removeFromTop(2);
            // Preset row moved below param knobs
            rightPanel.removeFromTop(3);

            // FX slots
            for (int i = 0; i < NUM_FX_SLOTS; ++i)
            {
                auto fxRow = rightPanel.removeFromTop(30);
                fxEditorButtons[i]->setBounds(fxRow.removeFromRight(32));
                fxRow.removeFromRight(2);
                fxSelectors[i]->setBounds(fxRow);
                rightPanel.removeFromTop(2);
            }
            rightPanel.removeFromTop(2);
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
        int knobSize = juce::jmin(44, (rightPanel.getWidth() - 8) / 3);
        int numRows = (NUM_PARAM_SLIDERS + 2) / 3;
        int labelH = 18;
        int knobRowH = knobSize + labelH + 2;  // knob + label + gap
        int knobAreaH = numRows * knobRowH + 4;
        auto knobArea = rightPanel.removeFromTop(knobAreaH);
        rightPanel.removeFromTop(4);

        int gridW = 3 * knobSize + 2 * 4; // 3 knobs + 2 gaps
        int gridOffsetX = (knobArea.getWidth() - gridW) / 2;

        for (int i = 0; i < NUM_PARAM_SLIDERS; ++i)
        {
            int col = i % 3;
            int row = i / 3;
            int kx = knobArea.getX() + gridOffsetX + col * (knobSize + 4);
            int ky = knobArea.getY() + row * knobRowH;

            paramLabels[i]->setBounds(kx, ky, knobSize, labelH);
            paramLabels[i]->setFont(juce::Font(11.0f));
            paramSliders[i]->setBounds(kx, ky + labelH, knobSize, knobSize);
        }
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
    presetSelector.setBounds(rightPanel.removeFromTop(28));
    rightPanel.removeFromTop(12);
    {
        auto btnArea = rightPanel.removeFromTop(44);
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
        int volSz = juce::jmin(mixArea.getWidth(), mixArea.getHeight(), 110);
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
            // Minimap at bottom — with safe area padding on iOS
            if (arrangerMinimap)
            {
#if JUCE_IOS
                area.removeFromBottom(20); // home indicator safe area
#endif
                arrangerMinimap->setBounds(area.removeFromBottom(20));
                arrangerMinimap->setVisible(true);
            }
            // Wider track labels when right panel is hidden — bigger M/S buttons
            // Interpolate with panel slide so it doesn't snap
            int labelW = 140 + (int)(40.0f * (1.0f - panelSlideProgress));
            timelineComponent->setTrackLabelWidth(labelW);
            timelineComponent->setVisible(true);
            timelineComponent->setBounds(area);
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
        fullscreenButton.setToggleState(false, juce::dontSendNotification);
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
        for (int k : keysCurrentlyDown) { int s = keyToNote(k); if (s >= 0) { int n = (computerKeyboardOctave * 12) + s; if (n >= 0 && n <= 127) sendNoteOff(n); } }
        keysCurrentlyDown.clear();
        computerKeyboardOctave = juce::jmax(0, computerKeyboardOctave - 1); updateStatusLabel(); return true;
    }
    if (keyCode == 'X') {
        for (int k : keysCurrentlyDown) { int s = keyToNote(k); if (s >= 0) { int n = (computerKeyboardOctave * 12) + s; if (n >= 0 && n <= 127) sendNoteOff(n); } }
        keysCurrentlyDown.clear();
        computerKeyboardOctave = juce::jmin(8, computerKeyboardOctave + 1); updateStatusLabel(); return true;
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
    captureButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnNav));
    captureButton.setColour(juce::TextButton::textColourOffId, juce::Colour(c.lcdText));

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
        trackInputSelector.setSelectedId(100, juce::dontSendNotification);
        // Auto-connect the first MIDI device
        disableCurrentMidiDevice();
        deviceManager.setMidiInputDeviceEnabled(devices[0].identifier, true);
        deviceManager.addMidiInputDeviceCallback(devices[0].identifier, this);
        currentMidiDeviceId = devices[0].identifier;
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
    presetSelector.addItem("-- Preset --", 1);

    auto& track = pluginHost.getTrack(selectedTrackIndex);
    if (track.plugin == nullptr)
    {
        presetSelector.setSelectedId(1, juce::dontSendNotification);
        return;
    }

    int numPresets = track.plugin->getNumPrograms();

    // Some AUv3 plugins report 1 program with empty name — treat as no presets
    bool hasRealPresets = numPresets > 1 || (numPresets == 1 && track.plugin->getProgramName(0).isNotEmpty());

    if (hasRealPresets)
    {
        for (int i = 0; i < numPresets; ++i)
        {
            juce::String name = track.plugin->getProgramName(i);
            if (name.isEmpty()) name = "Preset " + juce::String(i + 1);
            presetSelector.addItem(name, i + 2);
        }

        int current = track.plugin->getCurrentProgram();
        presetSelector.setSelectedId(current + 2, juce::dontSendNotification);
    }
    else
    {
        // Retry after a delay — some AUv3 plugins populate presets asynchronously
        juce::Component::SafePointer<MainComponent> safeThis(this);
        juce::Timer::callAfterDelay(1500, [safeThis] {
            if (auto* self = safeThis.getComponent())
            {
                auto& t = self->pluginHost.getTrack(self->selectedTrackIndex);
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
        case MidiTarget::Param0: case MidiTarget::Param1: case MidiTarget::Param2:
        case MidiTarget::Param3: case MidiTarget::Param4: case MidiTarget::Param5:
        case MidiTarget::Param6: case MidiTarget::Param7: case MidiTarget::Param8:
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
        case MidiTarget::Param0: case MidiTarget::Param1: case MidiTarget::Param2:
        case MidiTarget::Param3: case MidiTarget::Param4: case MidiTarget::Param5:
        case MidiTarget::Param6: case MidiTarget::Param7: case MidiTarget::Param8:
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
