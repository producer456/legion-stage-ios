// Include AudioToolbox BEFORE JuceHeader to avoid type conflicts
#if __APPLE__
#import <AudioToolbox/AudioToolbox.h>
#import <Foundation/Foundation.h>
#endif

#include "AUScanner.h"

#if JUCE_IOS || JUCE_MAC

juce::Array<AUScanner::AUInfo> AUScanner::scanAllAudioUnits()
{
    juce::Array<AUInfo> results;

    UInt32 types[] = {
        kAudioUnitType_MusicDevice,      // aumu - instruments
        kAudioUnitType_Effect,           // aufx - effects
        kAudioUnitType_MusicEffect,      // aumf - music effects (MIDI-aware FX)
        kAudioUnitType_Generator,        // augn - generators
        kAudioUnitType_MIDIProcessor     // aumi - MIDI processors
    };

    auto fourCC = [](UInt32 val) -> juce::String {
        char str[5] = {
            static_cast<char>((val >> 24) & 0xFF),
            static_cast<char>((val >> 16) & 0xFF),
            static_cast<char>((val >> 8) & 0xFF),
            static_cast<char>(val & 0xFF), 0
        };
        return juce::String(str);
    };

    for (auto type : types)
    {
        AudioComponentDescription desc = {};
        desc.componentType = type;

        AudioComponent comp = nullptr;
        while ((comp = AudioComponentFindNext(comp, &desc)) != nullptr)
        {
            AudioComponentDescription found;
            if (AudioComponentGetDescription(comp, &found) != noErr)
                continue;

            CFStringRef cfName = nullptr;
            AudioComponentCopyName(comp, &cfName);

            AUInfo info;
            info.name = cfName ? juce::String::fromCFString(cfName) : "Unknown";
            if (cfName) CFRelease(cfName);

            info.manufacturer = fourCC(found.componentManufacturer);
            info.identifier = fourCC(found.componentType) + "/"
                            + fourCC(found.componentSubType) + "/"
                            + fourCC(found.componentManufacturer);
            info.isInstrument = (type == kAudioUnitType_MusicDevice);
            info.category = (type == kAudioUnitType_MusicDevice) ? "Instrument" :
                           (type == kAudioUnitType_Effect || type == kAudioUnitType_MusicEffect) ? "Effect" :
                           (type == kAudioUnitType_Generator) ? "Generator" : "MIDI";
            info.uniqueId = static_cast<int>(found.componentType ^ found.componentSubType ^ found.componentManufacturer);

            results.add(info);
        }
    }

    return results;
}

#else

juce::Array<AUScanner::AUInfo> AUScanner::scanAllAudioUnits()
{
    return {};
}

#endif
