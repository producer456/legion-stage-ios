cmake_minimum_required(VERSION 3.22)
project(Sequencer VERSION 1.9.0)

# ASIO SDK path — JUCE needs this to compile ASIO support (Windows only)
if(WIN32)
    set(JUCE_ASIO_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/asio-sdk" CACHE PATH "ASIO SDK location")
endif()

add_subdirectory(libs/JUCE)

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    juce_add_gui_app(Sequencer
        PRODUCT_NAME "Legion Stage"
        COMPANY_NAME "Dev"
        BUNDLE_ID "com.dev.legionstage"
        IS_AU_PLUGIN_HOST TRUE
        BACKGROUND_AUDIO_ENABLED TRUE
        FILE_SHARING_ENABLED TRUE
        DOCUMENT_BROWSER_ENABLED TRUE
        REQUIRES_FULL_SCREEN TRUE
        MICROPHONE_PERMISSION_ENABLED TRUE
        PLIST_TO_MERGE "${CMAKE_CURRENT_SOURCE_DIR}/resources/LaunchScreen.plist"
        ICON_FILE "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon_1024.png"
        CUSTOM_XCASSETS "${CMAKE_CURRENT_SOURCE_DIR}/resources/Images.xcassets"
        IPHONE_SCREEN_ORIENTATIONS UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight
        IPAD_SCREEN_ORIENTATIONS UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight
    )
    # Skip storyboard compilation (use UILaunchScreen plist key instead)
    # Works around ibtool "iOS Platform Not Installed" issue in Xcode 26 beta
    set_target_properties(Sequencer PROPERTIES JUCE_SHOULD_ADD_STORYBOARD FALSE)
    # HealthKit usage descriptions — inject into JUCE-generated Info.plist
    add_custom_command(TARGET Sequencer POST_BUILD
        COMMAND /usr/libexec/PlistBuddy -c "Add :NSHealthShareUsageDescription string 'Legion Stage uses your heart rate to create bio-resonance visualizations that sync your heartbeat with your music.'" "$<TARGET_BUNDLE_DIR:Sequencer>/Info.plist" 2>/dev/null || true
        COMMAND /usr/libexec/PlistBuddy -c "Add :NSHealthUpdateUsageDescription string 'Legion Stage does not write health data.'" "$<TARGET_BUNDLE_DIR:Sequencer>/Info.plist" 2>/dev/null || true
        COMMAND /usr/libexec/PlistBuddy -c "Add :NSMotionUsageDescription string 'Legion Stage uses device motion for tilt-reactive visual effects in the Liquid Glass theme.'" "$<TARGET_BUNDLE_DIR:Sequencer>/Info.plist" 2>/dev/null || true
    )
    # Add app icon xcassets
    set(XCASSETS "${CMAKE_CURRENT_SOURCE_DIR}/resources/Images.xcassets")
    target_sources(Sequencer PRIVATE "${XCASSETS}")
    set_source_files_properties("${XCASSETS}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_target_properties(Sequencer PROPERTIES
        XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
        XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/Legion Stage.entitlements"
    )
else()
    juce_add_gui_app(Sequencer
        PRODUCT_NAME "Legion Stage"
        COMPANY_NAME "Dev"
    )
endif()

juce_generate_juce_header(Sequencer)

# ── Drum kit WAV resources ──
# Collect all WAV files from each drum kit directory and add them to the bundle
set(DRUMKIT_DIRS "CR-78" "TR-505" "LM-2")
foreach(KIT ${DRUMKIT_DIRS})
    file(GLOB KIT_WAVS "${CMAKE_CURRENT_SOURCE_DIR}/resources/drumkits/${KIT}/*.wav")
    target_sources(Sequencer PRIVATE ${KIT_WAVS})
    set_source_files_properties(${KIT_WAVS} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources/drumkits/${KIT}"
    )
endforeach()

# ── Bundled fonts ──
# Plus Jakarta Sans (variable weight) for the Launchkey themes —
# closest free match to Novation's brand font Brockmann.
file(GLOB FONT_FILES "${CMAKE_CURRENT_SOURCE_DIR}/resources/fonts/*.ttf")
target_sources(Sequencer PRIVATE ${FONT_FILES})
set_source_files_properties(${FONT_FILES} PROPERTIES
    MACOSX_PACKAGE_LOCATION "Resources/fonts"
)

target_sources(Sequencer PRIVATE
    src/Main.cpp
    src/MainComponent.h
    src/MainComponent.cpp
    src/LaunchkeyMK4Controller.h
    src/LaunchkeyMK4Controller.cpp
    src/PluginHost.h
    src/PluginHost.cpp
    src/GainProcessor.h
    src/GainProcessor.cpp
    src/MidiClip.h
    src/ClipPlayerNode.h
    src/ClipPlayerNode.cpp
    src/SequencerEngine.h
    src/SequencerEngine.cpp
    src/PianoRollComponent.h
    src/PianoRollComponent.cpp
    src/Midi2Handler.h
    src/Midi2Handler.cpp
    src/DawTheme.h
    src/DawLookAndFeel.h
    src/KeystageLookAndFeel.h
    src/AlcantaraLookAndFeel.h
    src/IoniqLookAndFeel.h
    src/IoniqDarkLookAndFeel.h
    src/IoniqForestLookAndFeel.h
    src/CyberpunkLookAndFeel.h
    src/OxideLookAndFeel.h
    src/LiquidGlassLookAndFeel.h
    src/LiquidGlassLightLookAndFeel.h
    src/GlassOverlayLookAndFeel.h
    src/OceanGlassLookAndFeel.h
    src/LaboratoryLookAndFeel.h
    src/LaunchkeyLookAndFeel.h
    src/LaunchkeyDarkLookAndFeel.h
    src/DeviceMotion.h
    src/DeviceMotion.mm
    src/AUPresetHelper.h
    src/AUPresetHelper.mm
    src/FileAccessHelper.h
    src/FileAccessHelper.mm
    src/SpectrumComponent.h
    src/LissajousComponent.h
    src/WaveTerrainComponent.h
    src/ShaderToyComponent.h
    src/AnalyzerComponent.h
    src/GeissComponent.h
    src/ProjectMComponent.h
    src/HeartbeatComponent.h
    src/HeartRateManager.h
    src/HeartRateManager.mm
    src/BioResonanceComponent.h
    src/TouchPianoComponent.h
    src/MixerComponent.h
    src/SplashComponent.h
    src/ThemeManager.h
    src/TimelineComponent.h
    src/ArrangerMinimapComponent.h
    src/TimelineComponent.cpp
    src/UpdateChecker.h
    src/UpdateChecker.cpp
    src/UpdateDialog.h
    src/UpdateDialog.cpp
    src/ChordDetector.h
    src/AUScanner.h
    src/AUScanner.mm
    src/MetalCausticRenderer.h
    src/MetalCausticRenderer.mm
    src/CausticShaders.metal
    src/MetalVisualizerRenderer.h
    src/MetalVisualizerRenderer.mm
    src/VisualizerShaders.metal
    src/FluidSimComponent.h
    src/RayMarchComponent.h
    src/Arpeggiator.h
    src/AudioExporter.h
    src/BuiltinSamplerProcessor.h
    src/BuiltinJuno60Processor.h
    src/Juno60/Oscillator.h
    src/Juno60/Oscillator.cpp
    src/Juno60/LadderFilter.h
    src/Juno60/LadderFilter.cpp
    src/Juno60/Envelope.h
    src/Juno60/Envelope.cpp
    src/Juno60/LFO.h
    src/Juno60/LFO.cpp
    src/Juno60/Chorus.h
    src/Juno60/Chorus.cpp
    src/Juno60/JunoVoice.h
    src/Juno60/JunoVoice.cpp
    src/Juno60/VoiceManager.h
    src/Juno60/VoiceManager.cpp
    src/Juno60/Arpeggiator.h
    src/Juno60/Arpeggiator.cpp
    src/Juno60/Presets.h
    src/Juno60/Juno60Editor.h
    src/Juno60/Juno60Editor.cpp
    src/SessionViewComponent.h
)

# Force ARC for Obj-C++ files that use Apple frameworks
set_source_files_properties(src/HeartRateManager.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
set_source_files_properties(src/MetalCausticRenderer.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
set_source_files_properties(src/MetalVisualizerRenderer.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")

target_compile_definitions(Sequencer PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    target_compile_definitions(Sequencer PRIVATE
        JUCE_PLUGINHOST_VST3=0
        JUCE_PLUGINHOST_AU=1
        JUCE_COREAUDIO=1
        JUCE_COREMIDI=1
    )
elseif(WIN32)
    target_compile_definitions(Sequencer PRIVATE
        JUCE_PLUGINHOST_VST3=1
        JUCE_ASIO=1
        JUCE_WASAPI=1
        JUCE_DIRECTSOUND=1
    )
    target_include_directories(Sequencer PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/asio-sdk/common
    )
elseif(APPLE)
    target_compile_definitions(Sequencer PRIVATE
        JUCE_PLUGINHOST_VST3=1
        JUCE_PLUGINHOST_AU=1
        JUCE_COREAUDIO=1
        JUCE_COREMIDI=1
    )
endif()

target_link_libraries(Sequencer PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_devices
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_opengl
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

if(WIN32)
    target_link_libraries(Sequencer PRIVATE opengl32)
elseif(APPLE)
    find_library(COREAUDIO_FRAMEWORK CoreAudio)
    find_library(COREMIDI_FRAMEWORK CoreMIDI)
    find_library(AUDIOTOOLBOX_FRAMEWORK AudioToolbox)
    find_library(AVFOUNDATION_FRAMEWORK AVFoundation)
    target_link_libraries(Sequencer PRIVATE
        ${COREAUDIO_FRAMEWORK}
        ${COREMIDI_FRAMEWORK}
        ${AUDIOTOOLBOX_FRAMEWORK}
        ${AVFOUNDATION_FRAMEWORK}
    )
    if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
        # AudioUnit.framework doesn't exist on iOS — it's part of AudioToolbox
        find_library(HEALTHKIT_FRAMEWORK HealthKit)
        if(HEALTHKIT_FRAMEWORK)
            target_link_libraries(Sequencer PRIVATE ${HEALTHKIT_FRAMEWORK})
        endif()
        find_library(COREMOTION_FRAMEWORK CoreMotion)
        if(COREMOTION_FRAMEWORK)
            target_link_libraries(Sequencer PRIVATE ${COREMOTION_FRAMEWORK})
        endif()
        find_library(METAL_FRAMEWORK Metal)
        find_library(METALKIT_FRAMEWORK MetalKit)
        if(METAL_FRAMEWORK AND METALKIT_FRAMEWORK)
            target_link_libraries(Sequencer PRIVATE ${METAL_FRAMEWORK} ${METALKIT_FRAMEWORK})
        endif()
    else()
        find_library(AUDIOUNIT_FRAMEWORK AudioUnit)
        find_library(OPENGL_FRAMEWORK OpenGL)
        target_link_libraries(Sequencer PRIVATE ${AUDIOUNIT_FRAMEWORK} ${OPENGL_FRAMEWORK})
    endif()
endif()
