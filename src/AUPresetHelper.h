#pragma once

#include <JuceHeader.h>

// Helper to query AUv3 factory preset names directly from the AudioUnit,
// bypassing JUCE's getProgramName() which sometimes returns empty strings
// for newer AUv3 plugins (e.g., AudioKit).

namespace AUPresetHelper
{
    // Returns factory preset names from an AudioPluginInstance.
    // Falls back to JUCE's getProgramName() if direct AU query fails.
    juce::StringArray getPresetNames(juce::AudioProcessor* plugin);
}
