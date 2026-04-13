#include <TargetConditionals.h>
#if TARGET_OS_IOS || TARGET_OS_MAC
// Include JUCE first to avoid namespace conflicts with AudioToolbox
#include "AUPresetHelper.h"

namespace AUPresetHelper
{
    juce::StringArray getPresetNames(juce::AudioProcessor* plugin)
    {
        juce::StringArray names;
        if (plugin == nullptr) return names;

        int numPresets = plugin->getNumPrograms();

        // First try JUCE's standard API
        bool allEmpty = true;
        for (int i = 0; i < juce::jmin(numPresets, 5); ++i)
        {
            auto name = plugin->getProgramName(i);
            if (name.isNotEmpty()) allEmpty = false;
        }

        if (!allEmpty)
        {
            // JUCE API works — use it
            for (int i = 0; i < numPresets; ++i)
            {
                auto name = plugin->getProgramName(i);
                names.add(name.isEmpty() ? ("Preset " + juce::String(i + 1)) : name);
            }
            return names;
        }

        // JUCE API returned empty names — try to get AUAudioUnit directly
        // via the AudioComponentInstance handle
        auto* instance = dynamic_cast<juce::AudioPluginInstance*>(plugin);
        if (instance == nullptr) return names;

        // Get the AudioUnit handle from JUCE
        AudioUnit au = nullptr;
        // JUCE's AudioUnitPluginInstance stores the AU handle — try to extract it
        // The method name varies by JUCE version
        #pragma clang diagnostic push
        #pragma clang diagnostic ignored "-Wundeclared-selector"
        if ([(__bridge id)au respondsToSelector:@selector(getAudioUnitHandle)])
        {
            // Can't reliably get AU handle from JUCE's opaque plugin instance
            // Fall back to numbered names
        }
        #pragma clang diagnostic pop

        // Try getting the underlying AUAudioUnit via AudioComponentInstanceGetComponent
        // This requires the AU to be instantiated
        // Unfortunately, JUCE doesn't expose this cleanly for hosted AUv3 plugins

        // Best fallback: try setting each program and reading the current program name
        // Some plugins only report the name of the ACTIVE program correctly
        int savedProgram = plugin->getCurrentProgram();
        for (int i = 0; i < numPresets; ++i)
        {
            plugin->setCurrentProgram(i);
            auto name = plugin->getProgramName(i);

            // Also try getCurrentProgramName if available
            if (name.isEmpty())
                name = plugin->getProgramName(plugin->getCurrentProgram());

            names.add(name.isEmpty() ? ("Preset " + juce::String(i + 1)) : name);
        }
        // Restore original program
        plugin->setCurrentProgram(savedProgram);

        return names;
    }
}

#else
// Non-Apple platforms
namespace AUPresetHelper
{
    juce::StringArray getPresetNames(juce::AudioProcessor* plugin)
    {
        juce::StringArray names;
        if (plugin == nullptr) return names;
        for (int i = 0; i < plugin->getNumPrograms(); ++i)
        {
            auto name = plugin->getProgramName(i);
            names.add(name.isEmpty() ? ("Preset " + juce::String(i + 1)) : name);
        }
        return names;
    }
}
#endif
