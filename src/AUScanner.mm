// AUScanner — uses AVAudioUnitComponentManager to discover AUv3 plugins
// Apple headers MUST come before JUCE to avoid Point/AudioBuffer conflicts

#import <AVFAudio/AVAudioUnitComponent.h>
#import <Foundation/Foundation.h>
#if __IPHONE_OS_VERSION_MIN_REQUIRED
#import <UIKit/UIDevice.h>
#endif

// Now include JUCE
#include "AUScanner.h"

#if JUCE_IOS || JUCE_MAC

enum {
    kAUType_MusicDevice    = 'aumu',
    kAUType_Effect         = 'aufx',
    kAUType_MusicEffect    = 'aumf',
    kAUType_Generator      = 'augn',
    kAUType_MIDIProcessor  = 'aumi'
};

static juce::String fourCC(UInt32 val)
{
    char str[5] = {
        static_cast<char>((val >> 24) & 0xFF),
        static_cast<char>((val >> 16) & 0xFF),
        static_cast<char>((val >> 8) & 0xFF),
        static_cast<char>(val & 0xFF), 0
    };
    return juce::String(str);
}

juce::Array<AUScanner::AUInfo> AUScanner::scanAllAudioUnits()
{
    juce::Array<AUInfo> results;

    AVAudioUnitComponentManager* manager = [AVAudioUnitComponentManager sharedAudioUnitComponentManager];

    AudioComponentDescription searchDesc = {};
    NSArray<AVAudioUnitComponent*>* components = [manager componentsMatchingDescription:searchDesc];

    for (AVAudioUnitComponent* comp in components)
    {
        AudioComponentDescription desc = comp.audioComponentDescription;

        if (desc.componentType != kAUType_MusicDevice &&
            desc.componentType != kAUType_Effect &&
            desc.componentType != kAUType_MusicEffect &&
            desc.componentType != kAUType_Generator &&
            desc.componentType != kAUType_MIDIProcessor)
            continue;

        AUInfo info;
        info.name = juce::String::fromUTF8([comp.name UTF8String]);
        info.manufacturer = juce::String::fromUTF8([comp.manufacturerName UTF8String]);
        info.identifier = fourCC(desc.componentType) + "/"
                        + fourCC(desc.componentSubType) + "/"
                        + fourCC(desc.componentManufacturer);
        info.isInstrument = (desc.componentType == kAUType_MusicDevice);
        info.category = (desc.componentType == kAUType_MusicDevice) ? "Instrument" :
                       (desc.componentType == kAUType_Effect ||
                        desc.componentType == kAUType_MusicEffect) ? "Effect" :
                       (desc.componentType == kAUType_Generator) ? "Generator" : "MIDI";
        info.uniqueId = static_cast<int>(desc.componentType ^ desc.componentSubType ^ desc.componentManufacturer);

        results.add(info);
    }

    return results;
}

void AUScanner::pumpRunLoop(int milliseconds)
{
    [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                             beforeDate:[NSDate dateWithTimeIntervalSinceNow:milliseconds / 1000.0]];
}

bool AUScanner::isIPhone()
{
    return [[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPhone;
}

#else

juce::Array<AUScanner::AUInfo> AUScanner::scanAllAudioUnits()
{
    return {};
}

void AUScanner::pumpRunLoop(int milliseconds)
{
    juce::Thread::sleep(milliseconds);
}

bool AUScanner::isIPhone()
{
    return false;
}

#endif
