#pragma once

#include <JuceHeader.h>

// Native AudioComponent scanning for iOS/macOS
// Bypasses JUCE's searchPathsForPlugins which may miss AUv3 app extensions
namespace AUScanner
{
    struct AUInfo {
        juce::String name;
        juce::String manufacturer;
        juce::String identifier;  // "type/subtype/manufacturer"
        juce::String category;
        bool isInstrument = false;
        int uniqueId = 0;
    };

    juce::Array<AUInfo> scanAllAudioUnits();
    void pumpRunLoop(int milliseconds);
    bool isIPhone();

    // Device performance tiers based on actual chip capability
    enum class DeviceTier {
        High,           // M1/M2/M3/M4+ chips — full 120Hz, half-res
        Mid,            // A12–A17 chips — 60Hz, half-res
        Low,            // A10X and older — 30Hz, quarter-res
        JamieEdition    // iPad Mini 7 (A17 Pro) — specially tuned
    };

    DeviceTier getDeviceTier();
    bool isJamieEdition();
    juce::String getDeviceModelId();  // e.g. "iPad16,2"
}
