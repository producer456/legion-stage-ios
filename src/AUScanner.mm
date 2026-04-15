// AUScanner — uses AVAudioUnitComponentManager to discover AUv3 plugins
// Apple headers MUST come before JUCE to avoid Point/AudioBuffer conflicts

#import <AVFAudio/AVAudioUnitComponent.h>
#import <Foundation/Foundation.h>
#if TARGET_OS_IOS || TARGET_OS_SIMULATOR
#import <UIKit/UIDevice.h>
#endif
#include <sys/utsname.h>

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

juce::String AUScanner::getDeviceModelId()
{
    struct utsname systemInfo;
    uname(&systemInfo);
    return juce::String(systemInfo.machine);
}

AUScanner::DeviceTier AUScanner::getDeviceTier()
{
    static DeviceTier cachedTier = [] {
        juce::String model = getDeviceModelId();

        // iPad Mini 7 = iPad16,1 or iPad16,2 (A17 Pro) — Jamie Edition
        if (model == "iPad16,1" || model == "iPad16,2")
            return DeviceTier::JamieEdition;

        // M-series iPads (High tier)
        // iPad Pro M1: iPad13,4-11  iPad Air M1: iPad13,16-17
        // iPad Pro M2: iPad14,3-6   iPad Air M2: iPad14,8-9
        // iPad Pro M4: iPad16,3-6   iPad Air M3: iPad14,10-11
        // iPad (A16): iPad14,12 (10th gen refresh)
        int majorNum = model.upToFirstOccurrenceOf(",", false, false)
                            .fromFirstOccurrenceOf("iPad", false, false)
                            .getIntValue();

        if (majorNum >= 16 && !model.startsWith("iPad16,1") && !model.startsWith("iPad16,2"))
            return DeviceTier::High;  // M4 Pro and newer
        if (majorNum == 14)
        {
            // iPad14,1-2 = iPad Mini 6 (A15) — Mid tier
            int minorNum = model.fromFirstOccurrenceOf(",", false, false).getIntValue();
            if (minorNum <= 2)
                return DeviceTier::Mid;
            return DeviceTier::High;  // iPad14,3+ = M2 Pro, M1 Air, M2 Air, M3 Air
        }
        if (majorNum == 13)
        {
            // iPad13,16-17 = Air M1, iPad13,4-11 = Pro M1
            // iPad13,1-3 = iPad Air 4 (A14) — Mid tier
            int minorNum = model.fromFirstOccurrenceOf(",", false, false).getIntValue();
            if (minorNum >= 4)
                return DeviceTier::High;
            return DeviceTier::Mid;
        }

        // A12-A17 chips — Mid tier
        // iPad8,x = Pro A12X, iPad11,x = Air A12, iPad12,x = A14
        if (majorNum == 12 || majorNum == 11)
            return DeviceTier::Mid;
        if (majorNum == 8)
            return DeviceTier::Mid;  // A12X Pro — still quite capable

        // Everything older: A10X and below — Low tier
        return DeviceTier::Low;
    }();
    return cachedTier;
}

bool AUScanner::isJamieEdition()
{
    return getDeviceTier() == DeviceTier::JamieEdition;
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

juce::String AUScanner::getDeviceModelId()
{
    return "Desktop";
}

AUScanner::DeviceTier AUScanner::getDeviceTier()
{
    return DeviceTier::High;
}

bool AUScanner::isJamieEdition()
{
    return false;
}

#endif
