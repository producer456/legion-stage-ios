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
}
