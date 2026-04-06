cmake_minimum_required(VERSION 3.22)
project(Sequencer VERSION 1.3.2)

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

target_sources(Sequencer PRIVATE
    src/Main.cpp
    src/MainComponent.h
    src/MainComponent.cpp
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
    src/SpectrumComponent.h
    src/LissajousComponent.h
    src/WaveTerrainComponent.h
    src/ShaderToyComponent.h
    src/AnalyzerComponent.h
    src/GeissComponent.h
    src/ProjectMComponent.h
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
)

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
    else()
        find_library(AUDIOUNIT_FRAMEWORK AudioUnit)
        find_library(OPENGL_FRAMEWORK OpenGL)
        target_link_libraries(Sequencer PRIVATE ${AUDIOUNIT_FRAMEWORK} ${OPENGL_FRAMEWORK})
    endif()
endif()
