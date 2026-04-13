#include <TargetConditionals.h>
#if TARGET_OS_IOS || TARGET_OS_MAC
#include "AUPresetHelper.h"

namespace AUPresetHelper
{
    juce::StringArray getPresetNames(juce::AudioProcessor* plugin)
    {
        juce::StringArray names;
        if (plugin == nullptr) return names;

        int numPresets = plugin->getNumPrograms();
        if (numPresets <= 0) return names;

        // Check if JUCE's API returns real names
        bool hasRealNames = false;
        for (int i = 0; i < juce::jmin(numPresets, 8); ++i)
        {
            auto name = plugin->getProgramName(i);
            if (name.isNotEmpty())
            {
                hasRealNames = true;
                break;
            }
        }

        if (hasRealNames)
        {
            for (int i = 0; i < numPresets; ++i)
            {
                auto name = plugin->getProgramName(i);
                names.add(name.isEmpty() ? ("Preset " + juce::String(i + 1)) : name);
            }
        }
        else if (numPresets > 1)
        {
            // Plugin has programs but no names — numbered fallback
            for (int i = 0; i < numPresets; ++i)
                names.add("Preset " + juce::String(i + 1));
        }

        return names;
    }
}

#else
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
