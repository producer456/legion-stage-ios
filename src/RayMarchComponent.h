#pragma once

#include <JuceHeader.h>
#if JUCE_IOS
#include "MetalVisualizerRenderer.h"
#endif

// Audio-reactive ray marching SDF visualizer.
// Renders 3D signed distance field scenes driven by FFT audio analysis.
// GPU path passes uniforms to a Metal fragment shader for real-time ray marching.
// CPU fallback renders at quarter resolution with a simplified SDF sphere scene.
class RayMarchComponent : public juce::Component, public juce::Timer
{
public:
    static constexpr int fftOrder = 10;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int numBands = 16;

    RayMarchComponent() { startTimerHz(60); }
    ~RayMarchComponent() override { stopTimer(); }

    void visibilityChanged() override
    {
        if (isVisible())
            startTimerHz(60);
        else
            stopTimer();
    }

    void pushSamples(const float* data, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            fifo[fifoIndex] = data[i];
            if (++fifoIndex >= fftSize)
            {
                std::copy(fifo, fifo + fftSize, fftData);
                fftReady.store(true);
                fifoIndex = 0;
            }
        }

        // Beat detection — threshold on RMS jump
        float sum = 0.0f;
        for (int i = 0; i < numSamples; ++i)
            sum += data[i] * data[i];
        float rms = std::sqrt(sum / static_cast<float>(juce::jmax(1, numSamples)));
        smoothedRms.store(smoothedRms.load() * 0.85f + rms * 0.15f);

        float cur = smoothedRms.load();
        if (cur > avgRms.load() * 1.8f && cur > 0.02f)
            beatHit.store(true);
        avgRms.store(avgRms.load() * 0.95f + cur * 0.05f);
    }

    // Cycle through SDF scene presets
    void nextPreset() { preset = (preset + 1) % numPresets; }
    void prevPreset() { preset = (preset - 1 + numPresets) % numPresets; }
    int getPreset() const { return preset; }
    int getNumPresets() const { return numPresets; }

    void timerCallback() override
    {
        time += 1.0f / 60.0f;
        // Wrap time to prevent float precision loss after long sessions
        if (time > 10000.0f) time -= 10000.0f;

        if (fftReady.exchange(false))
        {
            // Hann window
            for (int i = 0; i < fftSize; ++i)
            {
                float w = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::twoPi
                    * static_cast<float>(i) / static_cast<float>(fftSize)));
                fftData[i] *= w;
            }
            std::fill(fftData + fftSize, fftData + fftSize * 2, 0.0f);
            fft.performFrequencyOnlyForwardTransform(fftData);

            // Bin into bands
            for (int b = 0; b < numBands; ++b)
            {
                float startFrac = std::pow(static_cast<float>(b) / numBands, 2.0f);
                float endFrac   = std::pow(static_cast<float>(b + 1) / numBands, 2.0f);
                int startBin = static_cast<int>(startFrac * fftSize * 0.5f);
                int endBin   = juce::jmax(startBin + 1, static_cast<int>(endFrac * fftSize * 0.5f));
                endBin = juce::jmin(endBin, fftSize / 2);

                float sum = 0.0f;
                for (int i = startBin; i < endBin; ++i)
                    sum = juce::jmax(sum, fftData[i]);

                float level = juce::jlimit(0.0f, 1.0f, std::log10(1.0f + sum * 10.0f) * 0.5f);
                bands[b] = bands[b] * 0.7f + level * 0.3f; // smooth
            }
        }

        // Beat intensity decay
        if (beatHit.exchange(false))
            beatIntensity = 1.0f;
        beatIntensity *= 0.92f;

        // Auto-animate camera — bounded for all presets
        camX = std::sin(time * 0.3f) * 2.0f;
        camY = 1.0f + std::sin(time * 0.2f) * 0.5f;
        if (preset == 1) // tunnel: fly forward using fmod to wrap
            camZ = std::fmod(time * 0.8f, 100.0f);
        else // other presets: orbit around the scene
            camZ = 3.0f + std::sin(time * 0.15f) * 1.5f;
        camRotX = std::sin(time * 0.15f) * 0.3f;
        camRotY = std::fmod(time * 0.4f, 6.2832f);

        repaint();
    }

    void paint(juce::Graphics& g) override
    {
#if JUCE_IOS
        if (tryMetalRender()) return;
#endif
        paintCPU(g);
    }

#if JUCE_IOS
    bool tryMetalRender()
    {
        if (!metalRenderer) metalRenderer = std::make_unique<MetalVisualizerRenderer>();
        if (!metalRenderer->isAvailable()) return false;

        if (!metalRenderer->isAttached())
        {
            if (auto* peer = getPeer())
                metalRenderer->attachToView(peer->getNativeHandle());
        }
        if (!metalRenderer->isAttached()) return false;

        metalRenderer->setBounds(getScreenX() - getTopLevelComponent()->getScreenX(),
                                  getScreenY() - getTopLevelComponent()->getScreenY(),
                                  getWidth(), getHeight());

        RayMarchGPUUniforms u {};
        u.time = time;
        u.bass = (bands[0] + bands[1] + bands[2]) / 3.0f;
        u.mid = (bands[5] + bands[6] + bands[7]) / 3.0f;
        u.high = (bands[11] + bands[12] + bands[13]) / 3.0f;
        u.energy = 0.0f;
        for (int b = 0; b < numBands; ++b) u.energy += bands[b];
        u.energy /= numBands;
        u.beatIntensity = beatIntensity;
        u.preset = preset;
        u.camPosX = camX;
        u.camPosY = camY;
        u.camPosZ = camZ;
        u.camRotX = camRotX;
        u.camRotY = camRotY;

        metalRenderer->renderRayMarch(u);
        return true;
    }
#endif

    // ── CPU fallback: quarter-resolution SDF sphere with basic lighting ──
    void paintCPU(juce::Graphics& g)
    {
        auto bounds = getLocalBounds();
        int w = bounds.getWidth();
        int h = bounds.getHeight();

        // Half resolution for performance (GPU handles full res)
        int rw = juce::jmax(1, w / 2);
        int rh = juce::jmax(1, h / 2);

        if (renderImage.getWidth() != rw || renderImage.getHeight() != rh)
            renderImage = juce::Image(juce::Image::ARGB, rw, rh, false);

        juce::Image::BitmapData bmp(renderImage, juce::Image::BitmapData::writeOnly);

        float bass   = (bands[0] + bands[1] + bands[2]) / 3.0f;
        float mid    = (bands[5] + bands[6] + bands[7]) / 3.0f;
        float high   = (bands[11] + bands[12] + bands[13]) / 3.0f;
        float energy = 0.0f;
        for (int b = 0; b < numBands; ++b) energy += bands[b];
        energy /= numBands;

        // Simple ray march parameters
        float sphereRadius = 0.8f + bass * 0.6f;
        float lightX = std::cos(time * 0.7f) * 2.0f;
        float lightY = 1.5f;
        float lightZ = std::sin(time * 0.7f) * 2.0f;

        for (int y = 0; y < rh; ++y)
        {
            float v = (static_cast<float>(y) / rh) * 2.0f - 1.0f;
            for (int x = 0; x < rw; ++x)
            {
                float u = (static_cast<float>(x) / rw) * 2.0f - 1.0f;
                u *= static_cast<float>(w) / static_cast<float>(h); // aspect correction

                // Ray direction (simple pinhole camera)
                float rdx = u;
                float rdy = v;
                float rdz = -1.5f;
                float rdLen = std::sqrt(rdx * rdx + rdy * rdy + rdz * rdz);
                rdx /= rdLen;
                rdy /= rdLen;
                rdz /= rdLen;

                // Ray origin
                float rox = 0.0f;
                float roy = 0.0f;
                float roz = 3.0f;

                // March the ray
                float totalDist = 0.0f;
                float r = 0.0f, gr = 0.0f, bl = 0.0f;
                bool hit = false;

                for (int step = 0; step < 32; ++step)
                {
                    float px = rox + rdx * totalDist;
                    float py = roy + rdy * totalDist;
                    float pz = roz + rdz * totalDist;

                    // SDF: sphere with sine displacement
                    float dist = std::sqrt(px * px + py * py + pz * pz) - sphereRadius;

                    // Surface distortion from mids
                    if (mid > 0.01f)
                    {
                        float disp = std::sin(px * 5.0f + time) * std::sin(py * 5.0f + time * 0.7f)
                                   * std::sin(pz * 5.0f + time * 0.3f) * mid * 0.3f;
                        dist += disp;
                    }

                    if (dist < 0.01f)
                    {
                        hit = true;

                        // Compute normal via finite differences
                        float eps = 0.02f;
                        float nx = sdfSphere(px + eps, py, pz, sphereRadius, mid)
                                 - sdfSphere(px - eps, py, pz, sphereRadius, mid);
                        float ny = sdfSphere(px, py + eps, pz, sphereRadius, mid)
                                 - sdfSphere(px, py - eps, pz, sphereRadius, mid);
                        float nz = sdfSphere(px, py, pz + eps, sphereRadius, mid)
                                 - sdfSphere(px, py, pz - eps, sphereRadius, mid);
                        float nLen = std::sqrt(nx * nx + ny * ny + nz * nz) + 0.0001f;
                        nx /= nLen;
                        ny /= nLen;
                        nz /= nLen;

                        // Diffuse lighting
                        float lx = lightX - px, ly = lightY - py, lz = lightZ - pz;
                        float lLen = std::sqrt(lx * lx + ly * ly + lz * lz) + 0.0001f;
                        lx /= lLen; ly /= lLen; lz /= lLen;
                        float diff = juce::jmax(0.0f, nx * lx + ny * ly + nz * lz);

                        // Color based on energy
                        float baseR = 0.2f + energy * 0.6f + high * 0.3f;
                        float baseG = 0.1f + bass * 0.4f;
                        float baseB = 0.4f + mid * 0.5f + beatIntensity * 0.3f;

                        r  = juce::jlimit(0.0f, 1.0f, diff * baseR + 0.05f);
                        gr = juce::jlimit(0.0f, 1.0f, diff * baseG + 0.03f);
                        bl = juce::jlimit(0.0f, 1.0f, diff * baseB + 0.08f);
                        break;
                    }

                    totalDist += dist;
                    if (totalDist > 10.0f) break;
                }

                if (!hit)
                {
                    // Background gradient
                    float t = (v + 1.0f) * 0.5f;
                    r  = 0.02f + t * 0.05f + energy * 0.03f;
                    gr = 0.02f + t * 0.03f;
                    bl = 0.05f + t * 0.1f + beatIntensity * 0.05f;
                }

                bmp.setPixelColour(x, y, juce::Colour::fromFloatRGBA(r, gr, bl, 1.0f));
            }
        }

        g.drawImage(renderImage, bounds.toFloat(),
            juce::RectanglePlacement::stretchToFit);
    }

private:
    // SDF helper for normal calculation
    float sdfSphere(float px, float py, float pz, float radius, float midVal) const
    {
        float dist = std::sqrt(px * px + py * py + pz * pz) - radius;
        if (midVal > 0.01f)
        {
            float disp = std::sin(px * 5.0f + time) * std::sin(py * 5.0f + time * 0.7f)
                       * std::sin(pz * 5.0f + time * 0.3f) * midVal * 0.3f;
            dist += disp;
        }
        return dist;
    }

    juce::dsp::FFT fft { fftOrder };
    float fifo[fftSize] = {};
    float fftData[fftSize * 2] = {};
    int fifoIndex = 0;
    std::atomic<bool> fftReady { false };

    float bands[numBands] = {};
    float time = 0.0f;
    int preset = 0;
    static constexpr int numPresets = 4;

    // Beat detection
    std::atomic<float> smoothedRms { 0.0f };
    std::atomic<float> avgRms { 0.0f };
    std::atomic<bool> beatHit { false };
    float beatIntensity = 0.0f;

    // Camera animation state
    float camX = 0.0f, camY = 1.0f, camZ = 0.0f;
    float camRotX = 0.0f, camRotY = 0.0f;

    juce::Image renderImage;

#if JUCE_IOS
    std::unique_ptr<MetalVisualizerRenderer> metalRenderer;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RayMarchComponent)
};
