#pragma once

#include <JuceHeader.h>
#include "DawLookAndFeel.h"

// Audio-reactive Shadertoy-style visualizer.
// Uses FFT data to drive procedural graphics — no OpenGL needed,
// renders entirely with JUCE software graphics for maximum compatibility.
// Simulates a GPU shader aesthetic using per-pixel math on a cached image.
class ShaderToyComponent : public juce::Component, public juce::Timer
{
public:
    static constexpr int fftOrder = 10;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int numBands = 16;

    ShaderToyComponent() { startTimerHz(60); }
    ~ShaderToyComponent() override { stopTimer(); }

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
    }

    // Cycle through different shader presets
    void nextPreset() { preset = (preset + 1) % numPresets; }
    void prevPreset() { preset = (preset - 1 + numPresets) % numPresets; }
    int getPreset() const { return preset; }
    int getNumPresets() const { return numPresets; }

    void timerCallback() override
    {
        time += 1.0f / 30.0f;

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

        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();
        int w = bounds.getWidth();
        int h = bounds.getHeight();

        // Render at half resolution for performance
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

        for (int y = 0; y < rh; ++y)
        {
            float v = static_cast<float>(y) / rh;
            for (int x = 0; x < rw; ++x)
            {
                float u = static_cast<float>(x) / rw;
                float r, gr, b;

                switch (preset)
                {
                case 0: // Plasma waves
                {
                    float cx = u - 0.5f, cy = v - 0.5f;
                    float d = std::sqrt(cx * cx + cy * cy);
                    float a = std::atan2(cy, cx);
                    float p1 = std::sin(d * 12.0f - time * 2.0f + bass * 8.0f);
                    float p2 = std::sin(a * 4.0f + time * 1.5f + mid * 6.0f);
                    float p3 = std::sin((u + v) * 8.0f + time + high * 4.0f);
                    r  = juce::jlimit(0.0f, 1.0f, 0.5f + 0.3f * p1 + 0.2f * energy);
                    gr = juce::jlimit(0.0f, 1.0f, 0.3f + 0.3f * p2 + 0.15f * bass);
                    b  = juce::jlimit(0.0f, 1.0f, 0.5f + 0.3f * p3 + 0.2f * high);
                    break;
                }
                case 1: // Radial pulse
                {
                    float cx = u - 0.5f, cy = v - 0.5f;
                    float d = std::sqrt(cx * cx + cy * cy);
                    float a = std::atan2(cy, cx);
                    int bandIdx = static_cast<int>((a + juce::MathConstants<float>::pi)
                        / (2.0f * juce::MathConstants<float>::pi) * numBands) % numBands;
                    float bandVal = bands[bandIdx];
                    float ring = std::sin(d * 20.0f - time * 3.0f) * 0.5f + 0.5f;
                    float pulse = (bandVal > d * 1.5f) ? 1.0f : 0.1f;
                    r  = juce::jlimit(0.0f, 1.0f, ring * pulse * 0.8f + bass * 0.3f);
                    gr = juce::jlimit(0.0f, 1.0f, pulse * 0.6f * (1.0f - d));
                    b  = juce::jlimit(0.0f, 1.0f, ring * 0.7f + high * 0.4f);
                    break;
                }
                case 2: // Electric field
                {
                    float cx = u - 0.5f, cy = v - 0.5f;
                    float f1x = 0.25f * std::sin(time * 0.7f), f1y = 0.25f * std::cos(time * 0.9f);
                    float f2x = -0.25f * std::cos(time * 0.6f), f2y = 0.25f * std::sin(time * 0.8f);
                    float d1 = std::sqrt((cx - f1x) * (cx - f1x) + (cy - f1y) * (cy - f1y));
                    float d2 = std::sqrt((cx - f2x) * (cx - f2x) + (cy - f2y) * (cy - f2y));
                    float field = (bass + 0.3f) / (d1 + 0.05f) + (mid + 0.3f) / (d2 + 0.05f);
                    field = std::fmod(field * 0.15f, 1.0f);
                    r  = juce::jlimit(0.0f, 1.0f, field * 1.2f);
                    gr = juce::jlimit(0.0f, 1.0f, field * 0.6f + energy * 0.3f);
                    b  = juce::jlimit(0.0f, 1.0f, (1.0f - field) * 0.8f + high * 0.5f);
                    break;
                }
                default: // Noise ocean
                {
                    float wave = std::sin(u * 10.0f + time + bass * 5.0f)
                               * std::cos(v * 8.0f + time * 0.7f + mid * 4.0f) * 0.5f + 0.5f;
                    float foam = std::sin(u * 30.0f + v * 20.0f + time * 2.0f) * 0.5f + 0.5f;
                    foam *= high * 2.0f;
                    r  = juce::jlimit(0.0f, 1.0f, wave * 0.2f + foam * 0.5f);
                    gr = juce::jlimit(0.0f, 1.0f, wave * 0.5f + energy * 0.3f);
                    b  = juce::jlimit(0.0f, 1.0f, wave * 0.8f + 0.2f);
                    break;
                }
                }

                bmp.setPixelColour(x, y, juce::Colour::fromFloatRGBA(r, gr, b, 1.0f));
            }
        }

        g.drawImage(renderImage, bounds.toFloat(),
            juce::RectanglePlacement::stretchToFit);
    }

private:
    juce::dsp::FFT fft { fftOrder };
    float fifo[fftSize] = {};
    float fftData[fftSize * 2] = {};
    int fifoIndex = 0;
    std::atomic<bool> fftReady { false };

    float bands[numBands] = {};
    float time = 0.0f;
    int preset = 0;
    static constexpr int numPresets = 4;

    juce::Image renderImage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ShaderToyComponent)
};
