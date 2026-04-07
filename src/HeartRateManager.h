#pragma once

#include <atomic>
#include <functional>

// Reads live heart rate from Apple Watch via HealthKit.
// The Watch automatically writes HR samples; we observe them.
class HeartRateManager
{
public:
    HeartRateManager();
    ~HeartRateManager();

    // Request HealthKit authorization. Call once at startup.
    void requestAuthorization();

    // Start/stop observing heart rate updates
    void startObserving();
    void stopObserving();

    // Current heart rate in BPM (0 = no data)
    std::atomic<double> heartRateBpm { 0.0 };

    // Whether we have authorization
    std::atomic<bool> authorized { false };
    std::atomic<bool> available { false };

private:
    void* impl = nullptr;  // opaque pointer to Obj-C implementation
};
