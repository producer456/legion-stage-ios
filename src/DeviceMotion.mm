#include "DeviceMotion.h"

// Use __APPLE__ + TARGET_OS_IOS instead of JUCE_IOS (not available without JuceHeader)
#include <TargetConditionals.h>
#if TARGET_OS_IOS
#import <CoreMotion/CoreMotion.h>

DeviceMotion& DeviceMotion::getInstance()
{
    static DeviceMotion instance;
    return instance;
}

DeviceMotion::~DeviceMotion()
{
    stop();
}

void DeviceMotion::start()
{
    if (running.load()) return;

    CMMotionManager* mm = [[CMMotionManager alloc] init];
    motionManager = (__bridge_retained void*)mm;

    if ([mm isDeviceMotionAvailable])
    {
        // Use device motion (gravity vector — best quality)
        mm.deviceMotionUpdateInterval = 1.0 / 30.0;
        [mm startDeviceMotionUpdatesToQueue:[NSOperationQueue mainQueue]
                                withHandler:^(CMDeviceMotion* motion, NSError* error) {
            if (motion && !error)
            {
                float gx = (float)motion.gravity.x;
                float gy = (float)motion.gravity.y;
                this->tiltX.store(fmaxf(-1.0f, fminf(1.0f, gx * 1.5f)), std::memory_order_relaxed);
                this->tiltY.store(fmaxf(-1.0f, fminf(1.0f, gy * 1.5f)), std::memory_order_relaxed);
            }
        }];
        running.store(true);
    }
    else if ([mm isAccelerometerAvailable])
    {
        // Fallback: raw accelerometer (noisier but works on all devices)
        mm.accelerometerUpdateInterval = 1.0 / 30.0;
        [mm startAccelerometerUpdatesToQueue:[NSOperationQueue mainQueue]
                                withHandler:^(CMAccelerometerData* data, NSError* error) {
            if (data && !error)
            {
                float gx = (float)data.acceleration.x;
                float gy = (float)data.acceleration.y;
                this->tiltX.store(fmaxf(-1.0f, fminf(1.0f, gx * 1.5f)), std::memory_order_relaxed);
                this->tiltY.store(fmaxf(-1.0f, fminf(1.0f, gy * 1.5f)), std::memory_order_relaxed);
            }
        }];
        running.store(true);
    }
}

void DeviceMotion::stop()
{
    if (!running.load() || motionManager == nullptr) return;

    CMMotionManager* mm = (__bridge_transfer CMMotionManager*)motionManager;
    [mm stopDeviceMotionUpdates];
    [mm stopAccelerometerUpdates];
    motionManager = nullptr;
    running.store(false);
}

#else
// Non-iOS: no-op implementation
DeviceMotion& DeviceMotion::getInstance()
{
    static DeviceMotion instance;
    return instance;
}

DeviceMotion::~DeviceMotion() {}
void DeviceMotion::start() {}
void DeviceMotion::stop() {}
#endif
