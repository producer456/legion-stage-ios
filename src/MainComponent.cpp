#include "MainComponent.h"
#if JUCE_IOS
#include "AUScanner.h"
#endif

MainComponent::MainComponent()
{
    themeManager.setTheme(ThemeManager::Keystage, this);

    auto result = deviceManager.initialiseWithDefaultDevices(0, 2);
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
            analyzerDisplay.setVisible(false);
    pluginHost.shaderToyDisplay = &shaderToyDisplay;

    addAndMakeVisible(analyzerDisplay);
    analyzerDisplay.setVisible(false);
    pluginHost.analyzerDisplay = &analyzerDisplay;

    addAndMakeVisible(geissDisplay);
    geissDisplay.setVisible(false);
    pluginHost.geissDisplay = &geissDisplay;

    addAndMakeVisible(projectMDisplay);
    projectMDisplay.setVisible(false);
            shaderToyDisplay.setVisible(false);
            analyzerDisplay.setVisible(false);
    pluginHost.projectMDisplay = &projectMDisplay;

    // Tap on small visualizer to go fullscreen
    spectrumDisplay.addMouseListener(this, false);
    lissajousDisplay.addMouseListener(this, false);
    waveTerrainDisplay.addMouseListener(this, false);
    shaderToyDisplay.addMouseListener(this, false);
    analyzerDisplay.addMouseListener(this, false);
    geissDisplay.addMouseListener(this, false);
    projectMDisplay.addMouseListener(this, false);

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
    bpmDownButton.onClick = [this] {
        double bpm = juce::jmax(20.0, pluginHost.getEngine().getBpm() - 1.0);
        pluginHost.getEngine().setBpm(bpm);
        bpmLabel.setText(juce::String(static_cast<int>(bpm)) + " BPM", juce::dontSendNotification);
    };

    addAndMakeVisible(bpmLabel);
    bpmLabel.setText("120 BPM", juce::dontSendNotification);
    bpmLabel.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(bpmUpButton);
    bpmUpButton.onClick = [this] {
        double bpm = juce::jmin(300.0, pluginHost.getEngine().getBpm() + 1.0);
        pluginHost.getEngine().setBpm(bpm);
        bpmLabel.setText(juce::String(static_cast<int>(bpm)) + " BPM", juce::dontSendNotification);
    };

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
        statusLabel.setText("MIDI Panic — all notes off", juce::dontSendNotification);
        panicAnimEndTime = juce::Time::getMillisecondCounterHiRes() * 0.001 + 3.0;
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
        int cur = track.plugin->getCurrentProgram();
        if (cur > 0) loadPreset(cur - 1);
    };
    addAndMakeVisible(presetNextButton);
    presetNextButton.onClick = [this] {
        auto& track = pluginHost.getTrack(selectedTrackIndex);
        if (track.plugin == nullptr) return;
        int cur = track.plugin->getCurrentProgram();
        if (cur < track.plugin->getNumPrograms() - 1) loadPreset(cur + 1);
    };

    addAndMakeVisible(presetUpButton);
    presetUpButton.setComponentID("pill");
    presetUpButton.onClick = [this] {
        auto& track = pluginHost.getTrack(selectedTrackIndex);
        if (track.plugin == nullptr) return;
        int cur = track.plugin->getCurrentProgram();
        if (cur < track.plugin->getNumPrograms() - 1)
        {
            loadPreset(cur + 1);
            juce::String name = track.plugin->getProgramName(cur + 1);
            int num = cur + 2;
            int total = track.plugin->getNumPrograms();
            beatLabel.setText(juce::String(num) + "/" + juce::String(total), juce::dontSendNotification);
            statusLabel.setText(name, juce::dontSendNotification);
            chordLabel.setText("Preset", juce::dontSendNotification);
            juce::Timer::callAfterDelay(2000, [this] {
                statusLabel.setText("", juce::dontSendNotification);
                chordLabel.setText("---", juce::dontSendNotification);
            });
        }
    };
    addAndMakeVisible(presetDownButton);
    presetDownButton.setComponentID("pill");
    presetDownButton.onClick = [this] {
        auto& track = pluginHost.getTrack(selectedTrackIndex);
        if (track.plugin == nullptr) return;
        int cur = track.plugin->getCurrentProgram();
        if (cur > 0)
        {
            loadPreset(cur - 1);
            juce::String name = track.plugin->getProgramName(cur - 1);
            int num = cur;
            int total = track.plugin->getNumPrograms();
            beatLabel.setText(juce::String(num) + "/" + juce::String(total), juce::dontSendNotification);
            statusLabel.setText(name, juce::dontSendNotification);
            chordLabel.setText("Preset", juce::dontSendNotification);
            juce::Timer::callAfterDelay(2000, [this] {
                statusLabel.setText("", juce::dontSendNotification);
                chordLabel.setText("---", juce::dontSendNotification);
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
    visSelector.addItem("Shader", 6);
    visSelector.addItem("Analyzer", 7);
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
        }
    };

    addAndMakeVisible(redoButton);
    redoButton.onClick = [this] {
        if (undoIndex < undoHistory.size() - 1)
        {
            undoIndex++;
            restoreSnapshot(undoHistory[undoIndex]);
        }
    };

    // ── Theme Selector ──
    addAndMakeVisible(themeSelector);
    for (int i = 0; i < ThemeManager::NumThemes; ++i)
        themeSelector.addItem(ThemeManager::getThemeName(static_cast<ThemeManager::Theme>(i)), i + 1);
    themeSelector.setSelectedId(ThemeManager::Keystage + 1, juce::dontSendNotification);
    themeSelector.onChange = [this] {
        auto idx = themeSelector.getSelectedId() - 1;
        if (idx >= 0 && idx < ThemeManager::NumThemes)
        {
            themeManager.setTheme(static_cast<ThemeManager::Theme>(idx), this);
            applyThemeToControls();
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
    addAndMakeVisible(*timelineComponent);

    setSize(1280, 800);
    setWantsKeyboardFocus(true);

    scanPlugins();
    scanMidiDevices();
    selectTrack(0);

#if JUCE_IOS
    juce::Timer::callAfterDelay(3000, [this] { scanPlugins(); });
    // Force layout refresh after orientation settles
    juce::Timer::callAfterDelay(500, [this] { resized(); repaint(); });
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
    auto& eng = pluginHost.getEngine();

    if (eng.isInCountIn())
    {
        int barsLeft = static_cast<int>(std::ceil(eng.getCountInBeatsRemaining() / 4.0));
        beatLabel.setText("Count: -" + juce::String(barsLeft), juce::dontSendNotification);
    }
    else
    {
        double beat = eng.getPositionInBeats();
        beatLabel.setText("Beat: " + juce::String(beat, 1), juce::dontSendNotification);
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
    static bool wasRecording = false;
    bool isRec = false;
    for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
    {
        auto* cp = pluginHost.getTrack(t).clipPlayer;
        if (cp != nullptr)
            for (int s = 0; s < ClipPlayerNode::NUM_SLOTS; ++s)
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
        for (int s = 0; s < ClipPlayerNode::NUM_SLOTS; ++s)
        {
            auto& slot = track.clipPlayer->getSlot(s);
            if (slot.hasContent())
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
            juce::Timer::callAfterDelay(3000, [this] {
                // Beat label will auto-update from timerCallback
                statusLabel.setText("", juce::dontSendNotification);
                chordLabel.setText("---", juce::dontSendNotification);
            });
        }
        juce::Timer::callAfterDelay(500, [this] { updateParamSliders(); updateFxDisplay(); updatePresetList(); });
#endif
    }
    else
    {
        beatLabel.setText("FAILED:", juce::dontSendNotification);
        statusLabel.setText(err, juce::dontSendNotification);
        chordLabel.setText("ERR", juce::dontSendNotification);
        juce::Timer::callAfterDelay(3000, [this] {
            statusLabel.setText("", juce::dontSendNotification);
            chordLabel.setText("---", juce::dontSendNotification);
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

        // Use the editor's preferred size, not its current bounds
        int edW = currentEditor->getWidth();
        int edH = currentEditor->getHeight();
        if (edW <= 0) edW = 600;
        if (edH <= 0) edH = 400;

        int closeBarH = 44;
        int ew = juce::jmin(edW, getWidth() - 20);
        int eh = juce::jmin(edH, getHeight() - closeBarH - 20);
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
    juce::Timer::callAfterDelay(500, [this] { pluginHost.sendTestNoteOff(60); });
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
    juce::Timer::callAfterDelay(500, [this] {
        if (auto* dev = deviceManager.getCurrentAudioDevice())
            pluginHost.setAudioParams(dev->getCurrentSampleRate(), dev->getCurrentBufferSizeSamples());
        updateStatusLabel();
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

void MainComponent::takeSnapshot()
{
    // Trim future history if we undid something
    while (undoHistory.size() > undoIndex + 1)
        undoHistory.removeLast();

    ProjectSnapshot snap;
    snap.bpm = pluginHost.getEngine().getBpm();

    for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
    {
        auto* cp = pluginHost.getTrack(t).clipPlayer;
        if (cp == nullptr) continue;

        for (int s = 0; s < ClipPlayerNode::NUM_SLOTS; ++s)
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
}

void MainComponent::restoreSnapshot(const ProjectSnapshot& snap)
{
    // Clear all clips
    for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
    {
        auto* cp = pluginHost.getTrack(t).clipPlayer;
        if (cp == nullptr) continue;

        for (int s = 0; s < ClipPlayerNode::NUM_SLOTS; ++s)
        {
            auto& slot = cp->getSlot(s);
            slot.clip = nullptr;
            slot.state.store(ClipSlot::Empty);
        }
    }

    pluginHost.getEngine().setBpm(snap.bpm);
    bpmLabel.setText(juce::String(static_cast<int>(snap.bpm)) + " BPM", juce::dontSendNotification);

    // Restore clips
    for (auto& cd : snap.clips)
    {
        auto* cp = pluginHost.getTrack(cd.trackIndex).clipPlayer;
        if (cp == nullptr) continue;

        auto& slot = cp->getSlot(cd.slotIndex);
        slot.clip = std::make_unique<MidiClip>();
        slot.clip->lengthInBeats = cd.lengthInBeats;
        slot.clip->timelinePosition = cd.timelinePosition;

        for (int e = 0; e < cd.events.getNumEvents(); ++e)
            slot.clip->events.addEvent(cd.events.getEventPointer(e)->message);
        slot.clip->events.updateMatchedPairs();

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
        auto xml = std::make_unique<juce::XmlElement>("SequencerProject");
        xml->setAttribute("bpm", pluginHost.getEngine().getBpm());

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
                if (lane->points.size() < 2) continue;
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

            for (int s = 0; s < ClipPlayerNode::NUM_SLOTS; ++s)
            {
                auto& slot = cp->getSlot(s);
                if (slot.clip == nullptr || !slot.hasContent()) continue;

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
            for (int s = 0; s < ClipPlayerNode::NUM_SLOTS; ++s)
            {
                cp->getSlot(s).clip = nullptr;
                cp->getSlot(s).state.store(ClipSlot::Empty);
            }
        }

        double bpm = xml->getDoubleAttribute("bpm", 120.0);
        pluginHost.getEngine().setBpm(bpm);
        bpmLabel.setText(juce::String(static_cast<int>(bpm)) + " BPM", juce::dontSendNotification);

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
                if (s < 0 || s >= ClipPlayerNode::NUM_SLOTS) continue;

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
            || src == &projectMDisplay)
        {
            visualizerFullScreen = true;
            projectorMode = true;
            fullscreenButton.setToggleState(true, juce::dontSendNotification);
            resized();
            repaint();
            return;
        }
    }

#if JUCE_IOS
    bool isPhone = AUScanner::isIPhone() && !forceIPadLayout;
    if (isPhone)
    {
        swipeStartPos = e.position;
        swipeActive = true;
    }
#endif
}

void MainComponent::mouseDrag(const juce::MouseEvent& e)
{
    (void)e;
}

void MainComponent::mouseUp(const juce::MouseEvent& e)
{
#if JUCE_IOS
    if (swipeActive)
    {
        swipeActive = false;
        auto delta = e.position - swipeStartPos;
        float absX = std::abs(delta.x);
        float absY = std::abs(delta.y);

        // Require minimum swipe distance and mostly horizontal
        if (absX > 80.0f && absX > absY * 2.0f)
        {
            if (delta.x < 0)
                selectTrack(juce::jmin(PluginHost::NUM_TRACKS - 1, selectedTrackIndex + 1));
            else
                selectTrack(juce::jmax(0, selectedTrackIndex - 1));
        }
    }
#else
    (void)e;
#endif
}

void MainComponent::showPhoneMenu()
{
    juce::PopupMenu menu;

    // Track info at top
    auto& track = pluginHost.getTrack(selectedTrackIndex);
    juce::String trackName = "Track " + juce::String(selectedTrackIndex + 1);
    if (track.plugin != nullptr)
        trackName += " — " + track.plugin->getName();
    menu.addSectionHeader(trackName);
    menu.addSeparator();

    // Track navigation submenu
    juce::PopupMenu trackMenu;
    for (int i = 0; i < PluginHost::NUM_TRACKS; ++i)
    {
        auto& t = pluginHost.getTrack(i);
        juce::String label = "Track " + juce::String(i + 1);
        if (t.plugin != nullptr)
            label += " — " + t.plugin->getName();
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
    visMenu.addItem(105, "Shader", true, currentVisMode == 5);
    visMenu.addItem(106, "Analyzer", true, currentVisMode == 6);
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
            else if (result >= 100 && result <= 104) {
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
            int topBarWidth = getWidth() - sidePW - 180 - sidePW - sidePW;
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
    int rightPanelTotal = 180 + oakW + oakW;  // right panel + oak strip + side panel
    int toolbarRight = getWidth() - rightPanelTotal;

    // Paint over right panel area with body color (clear top bar/toolbar bleed)
    if (oakW > 0)
    {
        int rpLeft = getWidth() - oakW - 180;  // left edge of right panel
        g.setColour(juce::Colour(c.body));
        g.fillRect(rpLeft, 0, 180, getHeight());

        // Draw OLED info panel background — track input selector
        if (trackInputSelector.isVisible())
        {
            auto oledBounds = trackInputSelector.getBounds().expanded(6, 6);
            g.setColour(juce::Colour(c.lcdBg));
            g.fillRoundedRectangle(oledBounds.toFloat(), 5.0f);
            g.setColour(juce::Colour(0xffc8bda8).withAlpha(0.6f));
            g.drawRoundedRectangle(oledBounds.toFloat(), 5.0f, 2.5f);
        }

        // Draw OLED background behind beat/status/chord in top bar with oak border
        if (beatLabel.isVisible())
        {
            auto beatOled = beatLabel.getBounds().getUnion(statusLabel.getBounds()).getUnion(chordLabel.getBounds());
            // Oak border
            auto oakBorder = beatOled.expanded(3, 2);
            g.setColour(juce::Colour(0xffc8bda8));
            g.fillRoundedRectangle(oakBorder.toFloat(), 4.0f);
            // LCD background
            g.setColour(juce::Colour(c.lcdBg));
            g.fillRoundedRectangle(beatOled.toFloat(), 3.0f);
        }
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
                // Draw oak strip to the left of the right panel
                int sidePW = lnf->getSidePanelWidth();
                int stripW = sidePW;
                // Layout order from right: [side panel 18] [right panel 180] [oak strip 18] [arranger]
                int stripX = getWidth() - sidePW - 180 - stripW;

                // Reuse oak grain drawing style from the side panels
                juce::Colour oakBase(0xffc8bda8);
                juce::Colour oakLight(0xffd6ccba);
                juce::Colour oakGrain(0xffa89880);

                g.setColour(oakBase);
                g.fillRect(stripX, 0, stripW, getHeight());

                juce::Random rng(99);
                for (int i = 0; i < 40; ++i)
                {
                    float x = stripX + rng.nextFloat() * stripW;
                    float yStart = rng.nextFloat() * getHeight() * 0.8f;
                    float len = 30.0f + rng.nextFloat() * (getHeight() * 0.4f);
                    float thickness = 0.5f + rng.nextFloat() * 1.5f;
                    g.setColour(oakGrain.withAlpha(0.15f + rng.nextFloat() * 0.2f));
                    g.drawLine(x, yStart, x + rng.nextFloat() * 2.0f - 1.0f, yStart + len, thickness);
                }

                for (int i = 0; i < 12; ++i)
                {
                    float x = stripX + rng.nextFloat() * stripW;
                    float yStart = rng.nextFloat() * getHeight();
                    float len = 10.0f + rng.nextFloat() * 40.0f;
                    g.setColour(oakLight.withAlpha(0.1f + rng.nextFloat() * 0.15f));
                    g.drawLine(x, yStart, x, yStart + len, 1.0f);
                }

                // Subtle border
                g.setColour(juce::Colour(0x30000000));
                g.drawVerticalLine(stripX, 0, static_cast<float>(getHeight()));
                g.drawVerticalLine(stripX + stripW - 1, 0, static_cast<float>(getHeight()));
            }
        }
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
    // Draw green border outside play button (pulses when playing)
    if (playButton.isVisible())
    {
        auto btnBounds = playButton.getBounds().toFloat().expanded(3.0f);
        g.setColour(juce::Colours::green.withAlpha(playHighlightAlpha * 0.8f));
        g.drawRect(btnBounds, 2.0f);
        g.setColour(juce::Colours::green.withAlpha(playHighlightAlpha * 0.25f));
        g.drawRect(btnBounds.expanded(2.0f), 3.0f);
    }

    // Draw red border outside record button (pulses when recording)
    if (recordButton.isVisible())
    {
        auto btnBounds = recordButton.getBounds().toFloat().expanded(3.0f);
        g.setColour(juce::Colours::red.withAlpha(recHighlightAlpha * 0.8f));
        g.drawRect(btnBounds, 2.0f);
        g.setColour(juce::Colours::red.withAlpha(recHighlightAlpha * 0.25f));
        g.drawRect(btnBounds.expanded(2.0f), 3.0f);
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
    int rightPanelW = isPhone ? 0 : 180;

    // ── Top Bar ──
    auto topBar = area.removeFromTop(topBarH).reduced(4, isPhone ? 2 : 10);
    // Trim top bar so it doesn't extend into right panel + oak strip area
    if (!isPhone && rightPanelW > 0)
    {
        int oakTrim = 0;
        if (auto* lnf = dynamic_cast<DawLookAndFeel*>(&getLookAndFeel()))
            if (lnf->getSidePanelWidth() > 0)
                oakTrim = lnf->getSidePanelWidth();
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
        audioSettingsButton.setVisible(true);
        audioSettingsButton.setButtonText("Audio");
        audioSettingsButton.setBounds(row1.removeFromLeft(row1.getWidth()));  // take whatever's left

        settingsButton.setVisible(false);

        // ── Row 2: Clip tools, save/load, vis/piano/mixer ──
        // Right side first
        fullscreenButton.setButtonText("VIS");
        fullscreenButton.setBounds(row2.removeFromRight(36));
        fullscreenButton.setVisible(true);
        row2.removeFromRight(gap);
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
        duplicateClipButton.setVisible(true);
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

        // Left side: BPM group
        bpmDownButton.setBounds(topBar.removeFromLeft(28));
        topBar.removeFromLeft(2);
        bpmLabel.setBounds(topBar.removeFromLeft(65));
        topBar.removeFromLeft(2);
        bpmUpButton.setBounds(topBar.removeFromLeft(28));
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
        topBar.removeFromRight(4);
        {
            // OLED display — wider to fit text, with oak border padding
            auto oledOuter = topBar.removeFromRight(120);
            auto infoArea = oledOuter.reduced(3, 2); // oak border inset
            int rowH = infoArea.getHeight() / 3;
            statusLabel.setBounds(infoArea.removeFromTop(rowH));
            statusLabel.setVisible(true);
            beatLabel.setBounds(infoArea.removeFromTop(rowH));
            beatLabel.setVisible(true);
            chordLabel.setBounds(infoArea);
            chordLabel.setVisible(true);
        }
        topBar.removeFromRight(4);

        // Center: transport group
        int transportW = 50+3+35+2+55+2+35+3+55+3+50+16+45+3+55;
        int leftPad = juce::jmax(0, (topBar.getWidth() - transportW) / 2);
        topBar.removeFromLeft(leftPad);

        stopButton.setBounds(topBar.removeFromLeft(50));
        topBar.removeFromLeft(3);
        scrollLeftButton.setBounds(topBar.removeFromLeft(35));
        topBar.removeFromLeft(2);
        playButton.setBounds(topBar.removeFromLeft(55));
        topBar.removeFromLeft(2);
        scrollRightButton.setBounds(topBar.removeFromLeft(35));
        topBar.removeFromLeft(3);
        recordButton.setBounds(topBar.removeFromLeft(55));
        topBar.removeFromLeft(3);
        loopButton.setBounds(topBar.removeFromLeft(50));
        topBar.removeFromLeft(16);
        metronomeButton.setBounds(topBar.removeFromLeft(45));
        topBar.removeFromLeft(3);
        panicButton.setBounds(topBar.removeFromLeft(55));

        // statusLabel moved to OLED panel in right panel
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
    scrollLeftButton.setBounds(topBar.removeFromLeft(40));
    topBar.removeFromLeft(2);
    scrollRightButton.setBounds(topBar.removeFromLeft(40));
    topBar.removeFromLeft(3);
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

        if (projectorMode)
        {
            // Projector mode — zero UI chrome, just the visualizer
            auto visArea = getLocalBounds();

            spectrumDisplay.setVisible(false);
            lissajousDisplay.setVisible(false);
            waveTerrainDisplay.setVisible(false);
            geissDisplay.setVisible(false);
            projectMDisplay.setVisible(false);
            shaderToyDisplay.setVisible(false);
            analyzerDisplay.setVisible(false);
            visSelector.setVisible(false);
            setVisControlsVisible();
            projectorButton.setVisible(false);

#if JUCE_IOS
            // On iOS, always show EXIT button since there's no Escape key
            visExitButton.setBounds(visArea.getRight() - 60, visArea.getY() + 50, 55, 35);
            visExitButton.setVisible(true);
            visExitButton.toFront(false);
#else
            visExitButton.setVisible(false);
#endif

            if (currentVisMode == 0) { spectrumDisplay.setBounds(visArea); spectrumDisplay.setAlpha(1.0f); spectrumDisplay.setVisible(true); }
            else if (currentVisMode == 1) { lissajousDisplay.setBounds(visArea); lissajousDisplay.setVisible(true); }
            else if (currentVisMode == 2) { waveTerrainDisplay.setBounds(visArea); waveTerrainDisplay.setVisible(true); }
            else if (currentVisMode == 3) { geissDisplay.setBounds(visArea); geissDisplay.setVisible(true); }
            else if (currentVisMode == 4) { projectMDisplay.setBounds(visArea); projectMDisplay.setVisible(true); }
            else if (currentVisMode == 5) { shaderToyDisplay.setBounds(visArea); shaderToyDisplay.setVisible(true); }
            else if (currentVisMode == 6) { analyzerDisplay.setBounds(visArea); analyzerDisplay.setVisible(true); }

#if JUCE_IOS
            visExitButton.toFront(false);
#endif
        }
        else
        {
            // Fullscreen with control bar
            auto fsArea = getLocalBounds();
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

            if (currentVisMode == 0) { spectrumDisplay.setBounds(visArea); spectrumDisplay.setAlpha(1.0f); spectrumDisplay.setVisible(true); }
            else if (currentVisMode == 1) { lissajousDisplay.setBounds(visArea); lissajousDisplay.setVisible(true); }
            else if (currentVisMode == 2) { waveTerrainDisplay.setBounds(visArea); waveTerrainDisplay.setVisible(true); }
            else if (currentVisMode == 3) { geissDisplay.setBounds(visArea); geissDisplay.setVisible(true); }
            else if (currentVisMode == 4) { projectMDisplay.setBounds(visArea); projectMDisplay.setVisible(true); }
            else if (currentVisMode == 5) { shaderToyDisplay.setBounds(visArea); shaderToyDisplay.setVisible(true); }
            else if (currentVisMode == 6) { analyzerDisplay.setBounds(visArea); analyzerDisplay.setVisible(true); }

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
        return;
    }

    // ── Restore visibility when not in vis mode (skip on iPhone) ──
    if (!isPhone)
    {
    // Restore top bar controls
    playButton.setVisible(true);
    stopButton.setVisible(true);
    recordButton.setVisible(true);
    metronomeButton.setVisible(true);
    loopButton.setVisible(true);
    bpmDownButton.setVisible(true);
    bpmLabel.setVisible(true);
    bpmUpButton.setVisible(true);
    tapTempoButton.setVisible(true);
    statusLabel.setVisible(true);
    chordLabel.setVisible(true);
    midiLearnButton.setVisible(true);
    mixerButton.setVisible(true);
    pianoToggleButton.setVisible(true);
    countInButton.setVisible(true);
    scrollLeftButton.setVisible(true);
    scrollRightButton.setVisible(true);
    zoomInButton.setVisible(true);
    zoomOutButton.setVisible(true);
    panicButton.setVisible(true);
    clearAutoButton.setVisible(true);
    fullscreenButton.setVisible(true);
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
    testNoteButton.setVisible(true);
    newClipButton.setVisible(true);
    deleteClipButton.setVisible(true);
    duplicateClipButton.setVisible(true);
    splitClipButton.setVisible(true);
    editClipButton.setVisible(true);
    quantizeButton.setVisible(true);
    gridSelector.setVisible(true);
    saveButton.setVisible(true);
    loadButton.setVisible(true);
    undoButton.setVisible(true);
    redoButton.setVisible(true);
    themeSelector.setVisible(true);
    audioSettingsButton.setVisible(true);
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
    shaderToyDisplay.setVisible(currentVisMode == 5);
    analyzerDisplay.setVisible(currentVisMode == 6);
    visExitButton.setVisible(false);
    projectorButton.setVisible(false);  // merged with fullscreen
    visSelector.setVisible(true);
    fullscreenButton.setVisible(true);
    midi2Button.setVisible(true);
    setVisControlsVisible();

    } // end if (!isPhone) — iPad layout

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
        chordLabel.setVisible(false);

        // ── Right panel: volume knob on top (full width), then two columns ──
        auto rightPanel = area.removeFromRight(100).reduced(2, 2);

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
    auto rightPanel = fullArea.removeFromRight(rightPanelW).reduced(8, 4);
    // Still remove from area so the timeline/toolbar don't overlap
    area.removeFromRight(rightPanelW);
    area.removeFromRight(oakStripW);

    // ── Edit Toolbar ──
    auto toolbar = area.removeFromTop(65).reduced(4, 4);
    newClipButton.setBounds(toolbar.removeFromLeft(95));
    toolbar.removeFromLeft(3);
    deleteClipButton.setBounds(toolbar.removeFromLeft(80));
    toolbar.removeFromLeft(3);
    duplicateClipButton.setBounds(toolbar.removeFromLeft(95));
    toolbar.removeFromLeft(3);
    splitClipButton.setBounds(toolbar.removeFromLeft(55));
    toolbar.removeFromLeft(2);
    editClipButton.setBounds(toolbar.removeFromLeft(75));
    toolbar.removeFromLeft(2);
    quantizeButton.setBounds(toolbar.removeFromLeft(70));
    toolbar.removeFromLeft(2);
    clearAutoButton.setBounds(toolbar.removeFromLeft(75));
    toolbar.removeFromLeft(4);
    gridSelector.setBounds(toolbar.removeFromLeft(65));
    toolbar.removeFromLeft(4);
    saveButton.setBounds(toolbar.removeFromLeft(50));
    toolbar.removeFromLeft(2);
    loadButton.setBounds(toolbar.removeFromLeft(50));
    toolbar.removeFromLeft(2);
    undoButton.setBounds(toolbar.removeFromLeft(50));
    toolbar.removeFromLeft(2);
    redoButton.setBounds(toolbar.removeFromLeft(50));

    // Pack remaining controls at the right end of the toolbar
#if !JUCE_IOS
    tapTempoButton.setBounds(toolbar.removeFromRight(50));
    toolbar.removeFromRight(3);
    bpmUpButton.setBounds(toolbar.removeFromRight(32));
    toolbar.removeFromRight(2);
    bpmLabel.setBounds(toolbar.removeFromRight(70));
    toolbar.removeFromRight(2);
    bpmDownButton.setBounds(toolbar.removeFromRight(32));
    toolbar.removeFromRight(6);
#endif
    midi2Button.setBounds(toolbar.removeFromRight(36));
    toolbar.removeFromRight(2);
    visSelector.setBounds(toolbar.removeFromRight(72));
    toolbar.removeFromRight(2);
    projectorButton.setVisible(false);
    fullscreenButton.setBounds(toolbar.removeFromRight(32));
    toolbar.removeFromRight(2);
    audioSettingsButton.setBounds(toolbar.removeFromRight(80));
    toolbar.removeFromRight(2);
    themeSelector.setBounds(toolbar.removeFromRight(82));

    // Track input selector
    trackNameLabel.setVisible(false);
    trackInputSelector.setBounds(rightPanel.removeFromTop(30));
    rightPanel.removeFromTop(4);

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
    else if (currentVisMode == 5)  // ShaderToy
    {
        shaderToyDisplay.setBounds(visPanelArea);
        shaderToyDisplay.setVisible(true);
    }
    else if (currentVisMode == 6)  // Analyzer
    {
        analyzerDisplay.setBounds(visPanelArea);
        analyzerDisplay.setVisible(true);
    }
    rightPanel.removeFromTop(4);

    // Vis controls only show in fullscreen mode — hide them in right panel
    setVisControlsVisible();

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
            timelineComponent->setVisible(true);
            timelineComponent->setBounds(area);
        }
    }
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
}

void MainComponent::sendNoteOff(int note)
{
    auto msg = juce::MidiMessage::noteOff(1, note);
    msg.setTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001);
    pluginHost.getMidiCollector().addMessageToQueue(msg);
    chordDetector.noteOff(note);
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

    if (keyCode == 'Z') { computerKeyboardOctave = juce::jmax(0, computerKeyboardOctave - 1); updateStatusLabel(); return true; }
    if (keyCode == 'X') { computerKeyboardOctave = juce::jmin(8, computerKeyboardOctave + 1); updateStatusLabel(); return true; }

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
    panicButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffdd6600));
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

    for (int i = 0; i < NUM_FX_SLOTS; ++i)
        fxEditorButtons[i]->setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnNav));

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
    for (int i = 0; i < numPresets; ++i)
    {
        juce::String name = track.plugin->getProgramName(i);
        if (name.isEmpty()) name = "Preset " + juce::String(i + 1);
        presetSelector.addItem(name, i + 2);
    }

    int current = track.plugin->getCurrentProgram();
    presetSelector.setSelectedId(current + 2, juce::dontSendNotification);
}

void MainComponent::loadPreset(int index)
{
    auto& track = pluginHost.getTrack(selectedTrackIndex);
    if (track.plugin == nullptr) return;

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
    if (edW <= 0) edW = 600;
    if (edH <= 0) edH = 400;

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
