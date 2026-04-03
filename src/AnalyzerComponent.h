#pragma once

#include <JuceHeader.h>
#include "DawLookAndFeel.h"

// Smooth-curve spectrum analyzer (Vengeance Scope style).
// Logarithmic frequency scale, smooth interpolated line, filled gradient,
// dB grid, peak hold.
class AnalyzerComponent : public juce::Component, public juce::Timer
{
public:
    static constexpr int fftOrder = 11;              // 2048-point FFT
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int numPoints = 256;            // curve resolution

    AnalyzerComponent() { startTimerHz(30); }
    ~AnalyzerComponent() override { stopTimer(); }

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

    void setPeakHold(bool on) { peakHold = on; }
    bool getPeakHold() const { return peakHold; }
    void togglePeakHold() { peakHold = !peakHold; if (!peakHold) std::fill(peakLevels, peakLevels + numPoints, -100.0f); }

    void timerCallback() override
    {
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

            // Map FFT bins to curve points using log frequency scale (20Hz - 20kHz)
            float minFreq = 20.0f;
            float maxFreq = 20000.0f;
            float nyquist = 44100.0f * 0.5f; // approximate

            for (int p = 0; p < numPoints; ++p)
            {
                // Log interpolation
                float t = static_cast<float>(p) / (numPoints - 1);
                float freq = minFreq * std::pow(maxFreq / minFreq, t);
                float binFloat = freq / nyquist * (fftSize / 2);
                int bin = static_cast<int>(binFloat);
                float frac = binFloat - bin;
                bin = juce::jlimit(0, fftSize / 2 - 2, bin);

                // Linear interpolation between bins
                float mag = fftData[bin] * (1.0f - frac) + fftData[bin + 1] * frac;

                // Convert to dB
                float dB = (mag > 0.0001f) ? 20.0f * std::log10(mag) : -100.0f;
                dB = juce::jlimit(-90.0f, 6.0f, dB);

                // Smooth falloff
                smoothLevels[p] = juce::jmax(dB, smoothLevels[p] - 1.5f);

                // Peak hold
                if (peakHold && smoothLevels[p] > peakLevels[p])
                    peakLevels[p] = smoothLevels[p];
                else if (peakHold)
                    peakLevels[p] = juce::jmax(peakLevels[p] - 0.15f, smoothLevels[p]);
            }

            repaint();
        }
    }

    void paint(juce::Graphics& g) override
    {
        uint32_t accentColor = 0xff40c8ff;
        uint32_t bgColor     = 0xff0a0e14;
        uint32_t gridColor   = 0xff1a2030;
        if (auto* lnf = dynamic_cast<DawLookAndFeel*>(&getLookAndFeel()))
        {
            accentColor = lnf->getTheme().lcdAmber;
            bgColor     = lnf->getTheme().bodyDark;
        }

        auto bounds = getLocalBounds().toFloat().reduced(1.0f);
        g.setColour(juce::Colour(bgColor));
        g.fillRoundedRectangle(bounds, 3.0f);

        float w = bounds.getWidth();
        float h = bounds.getHeight();
        float x0 = bounds.getX();
        float y0 = bounds.getY();

        // dB range
        float dbMin = -90.0f;
        float dbMax = 6.0f;
        float dbRange = dbMax - dbMin;

        // Grid lines — dB horizontal
        g.setColour(juce::Colour(gridColor));
        float dbLines[] = { -72, -54, -36, -18, 0 };
        for (float db : dbLines)
        {
            float y = y0 + (1.0f - (db - dbMin) / dbRange) * h;
            g.drawHorizontalLine(static_cast<int>(y), x0, x0 + w);

            // Label
            g.setColour(juce::Colour(gridColor).brighter(0.5f));
            g.setFont(9.0f);
            g.drawText(juce::String(static_cast<int>(db)), static_cast<int>(x0 + 2), static_cast<int>(y - 10), 28, 10,
                juce::Justification::left);
            g.setColour(juce::Colour(gridColor));
        }

        // Grid lines — frequency vertical
        float freqLines[] = { 50, 100, 200, 500, 1000, 2000, 5000, 10000 };
        float minFreq = 20.0f, maxFreq = 20000.0f;
        for (float freq : freqLines)
        {
            float t = std::log(freq / minFreq) / std::log(maxFreq / minFreq);
            float x = x0 + t * w;
            g.drawVerticalLine(static_cast<int>(x), y0, y0 + h);

            g.setColour(juce::Colour(gridColor).brighter(0.5f));
            juce::String label = (freq >= 1000) ? juce::String(static_cast<int>(freq / 1000)) + "k"
                                                 : juce::String(static_cast<int>(freq));
            g.setFont(9.0f);
            g.drawText(label, static_cast<int>(x - 12), static_cast<int>(y0 + h - 12), 24, 10,
                juce::Justification::centred);
            g.setColour(juce::Colour(gridColor));
        }

        // Build the spectrum curve path
        juce::Path curvePath;
        juce::Path fillPath;

        for (int p = 0; p < numPoints; ++p)
        {
            float t = static_cast<float>(p) / (numPoints - 1);
            float x = x0 + t * w;
            float y = y0 + (1.0f - (smoothLevels[p] - dbMin) / dbRange) * h;
            y = juce::jlimit(y0, y0 + h, y);

            if (p == 0) { curvePath.startNewSubPath(x, y); fillPath.startNewSubPath(x, y0 + h); fillPath.lineTo(x, y); }
            else { curvePath.lineTo(x, y); fillPath.lineTo(x, y); }
        }

        fillPath.lineTo(x0 + w, y0 + h);
        fillPath.closeSubPath();

        // Filled gradient under curve
        auto fillColor = juce::Colour(accentColor);
        g.setGradientFill(juce::ColourGradient(
            fillColor.withAlpha(0.35f), 0, y0,
            fillColor.withAlpha(0.03f), 0, y0 + h, false));
        g.fillPath(fillPath);

        // Main curve line
        g.setColour(fillColor.withAlpha(0.9f));
        g.strokePath(curvePath, juce::PathStrokeType(1.5f));

        // Peak hold line
        if (peakHold)
        {
            juce::Path peakPath;
            for (int p = 0; p < numPoints; ++p)
            {
                float t = static_cast<float>(p) / (numPoints - 1);
                float x = x0 + t * w;
                float y = y0 + (1.0f - (peakLevels[p] - dbMin) / dbRange) * h;
                y = juce::jlimit(y0, y0 + h, y);
                if (p == 0) peakPath.startNewSubPath(x, y);
                else peakPath.lineTo(x, y);
            }
            g.setColour(fillColor.withAlpha(0.4f));
            g.strokePath(peakPath, juce::PathStrokeType(0.8f));
        }
    }

private:
    juce::dsp::FFT fft { fftOrder };
    float fifo[fftSize] = {};
    float fftData[fftSize * 2] = {};
    int fifoIndex = 0;
    std::atomic<bool> fftReady { false };

    float smoothLevels[numPoints] = {};
    float peakLevels[numPoints] = {};
    bool peakHold = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AnalyzerComponent)
};
