#pragma once

#include <JuceHeader.h>
#include "DawLookAndFeel.h"

// Waveform Terrain — stacked waveform lines scrolling back in pseudo-3D,
// like the Joy Division "Unknown Pleasures" album cover but with live audio.
class WaveTerrainComponent : public juce::Component, public juce::Timer
{
public:
    static constexpr int numLines = 48;       // number of stacked waveforms
    static constexpr int lineResolution = 128; // samples per line

    WaveTerrainComponent() { startTimerHz(30); }
    ~WaveTerrainComponent() override { stopTimer(); }

    void pushSamples(const float* data, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            ringBuf[ringIndex] = data[i];
            ringIndex = (ringIndex + 1) % ringBufSize;
        }
        samplesSinceLastLine += numSamples;
    }

    void timerCallback() override
    {
        // Capture a new line every ~30ms worth of audio
        if (samplesSinceLastLine >= lineResolution)
        {
            samplesSinceLastLine = 0;

            // Shift lines back
            for (int l = numLines - 1; l > 0; --l)
                std::copy(lines[l - 1], lines[l - 1] + lineResolution, lines[l]);

            // Downsample ring buffer into the front line
            int readStart = (ringIndex - lineResolution * 4 + ringBufSize) % ringBufSize;
            for (int i = 0; i < lineResolution; ++i)
            {
                int idx = (readStart + i * 4) % ringBufSize;
                lines[0][i] = ringBuf[idx];
            }

            repaint();
        }
    }

    void paint(juce::Graphics& g) override
    {
        uint32_t lineColor = 0xffc8e4ff;
        uint32_t bgColor   = 0xff0a0e14;
        if (auto* lnf = dynamic_cast<DawLookAndFeel*>(&getLookAndFeel()))
        {
            lineColor = lnf->getTheme().lcdAmber;
            bgColor   = lnf->getTheme().lcdBg;
        }

        auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colour(bgColor));
        g.fillRoundedRectangle(bounds, 3.0f);

        float w = bounds.getWidth();
        float h = bounds.getHeight();
        float lineSpacing = h / static_cast<float>(numLines + 2);

        // Draw from back to front so nearer lines occlude
        for (int l = numLines - 1; l >= 0; --l)
        {
            float baseY = bounds.getY() + (static_cast<float>(l) + 1.5f) * lineSpacing;
            float alpha = 1.0f - (static_cast<float>(l) / static_cast<float>(numLines)) * 0.7f;
            float amplitude = lineSpacing * 2.5f * (1.0f - static_cast<float>(l) / static_cast<float>(numLines) * 0.5f);

            // Fill below the waveform with bg color to create occlusion
            juce::Path fillPath;
            fillPath.startNewSubPath(bounds.getX(), baseY);
            for (int i = 0; i < lineResolution; ++i)
            {
                float x = bounds.getX() + (static_cast<float>(i) / (lineResolution - 1)) * w;
                float y = baseY - lines[l][i] * amplitude;
                fillPath.lineTo(x, y);
            }
            fillPath.lineTo(bounds.getRight(), baseY);
            fillPath.lineTo(bounds.getRight(), baseY + lineSpacing);
            fillPath.lineTo(bounds.getX(), baseY + lineSpacing);
            fillPath.closeSubPath();
            g.setColour(juce::Colour(bgColor));
            g.fillPath(fillPath);

            // Draw the waveform line
            juce::Path linePath;
            for (int i = 0; i < lineResolution; ++i)
            {
                float x = bounds.getX() + (static_cast<float>(i) / (lineResolution - 1)) * w;
                float y = baseY - lines[l][i] * amplitude;
                if (i == 0) linePath.startNewSubPath(x, y);
                else linePath.lineTo(x, y);
            }

            g.setColour(juce::Colour(lineColor).withAlpha(alpha));
            g.strokePath(linePath, juce::PathStrokeType(1.2f));
        }
    }

private:
    float lines[numLines][lineResolution] = {};

    static constexpr int ringBufSize = 8192;
    float ringBuf[ringBufSize] = {};
    int ringIndex = 0;
    int samplesSinceLastLine = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveTerrainComponent)
};
