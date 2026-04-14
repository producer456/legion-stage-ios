#pragma once

#include <JuceHeader.h>

// C-linkage bridge to ObjC implementation
extern "C" {
    const char* fileAccess_readFile(const char* path);
    bool fileAccess_writeFile(const char* path, const char* content);
}

namespace FileAccessHelper
{
    inline juce::String readFileContent(const juce::File& file)
    {
        // Try JUCE direct read first (works for app-sandbox files)
        auto content = file.loadFileAsString();
        if (content.isNotEmpty())
            return content;

        // Use ObjC helper for security-scoped iOS file picker URLs
        auto* cStr = fileAccess_readFile(file.getFullPathName().toRawUTF8());
        if (cStr != nullptr)
        {
            content = juce::String::fromUTF8(cStr);
            free((void*)cStr);
        }
        return content;
    }

    inline bool writeFileContent(const juce::File& file, const juce::String& content)
    {
        // Try JUCE direct write first
        if (file.replaceWithText(content))
            return true;

        // Use ObjC helper for security-scoped access
        return fileAccess_writeFile(file.getFullPathName().toRawUTF8(),
                                   content.toRawUTF8());
    }
}
