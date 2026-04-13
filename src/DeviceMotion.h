#pragma once

#include <atomic>

// Lightweight wrapper around CoreMotion for glass theme tilt-reactive effects.
// Returns normalized tilt values (-1..1) for x/y device pitch/roll.
// On non-iOS platforms, returns 0 (no tilt).

struct DeviceTilt
{
    float x = 0.0f;  // -1 (left tilt) to +1 (right tilt)
    float y = 0.0f;  // -1 (tilt toward user) to +1 (tilt away)
};

class DeviceMotion
{
public:
    static DeviceMotion& getInstance();

    void start();
    void stop();
    bool isRunning() const { return running.load(); }

    DeviceTilt getTilt() const
    {
        DeviceTilt t;
        t.x = tiltX.load(std::memory_order_relaxed);
        t.y = tiltY.load(std::memory_order_relaxed);
        return t;
    }

private:
    DeviceMotion() = default;
    ~DeviceMotion();

    std::atomic<bool> running { false };
    std::atomic<float> tiltX { 0.0f };
    std::atomic<float> tiltY { 0.0f };
    void* motionManager = nullptr;
};
