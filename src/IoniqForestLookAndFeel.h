#pragma once

#include "IoniqLookAndFeel.h"

// "Ioniq Forest" theme — Ioniq with white oak wood accents.
// Same cream leather body and OLED displays as Ioniq, but with
// Keystage-style white oak side panels, inner strip, and top bar.
class IoniqForestLookAndFeel : public IoniqLookAndFeel
{
public:
    IoniqForestLookAndFeel() {}

    // Wider panels for oak cheeks
    int getSidePanelWidth() const override { return 10; }

    // White oak side panels — same as Keystage
    void drawSidePanels(juce::Graphics& g, int width, int height) override
    {
        if (width != sideCacheKey || height != sideCacheH)
        {
            int panelW = getSidePanelWidth();
            sideCache = juce::Image(juce::Image::ARGB, width, height, true);
            juce::Graphics sg(sideCache);

            juce::Colour oakBase(0xffc8bda8);
            juce::Colour oakLight(0xffd6ccba);
            juce::Colour oakGrain(0xffa89880);

            sg.setColour(oakBase);
            sg.fillRect(0, 0, panelW, height);
            sg.fillRect(width - panelW, 0, panelW, height);

            juce::Random rng(42);
            for (int i = 0; i < 40; ++i)
            {
                float x = rng.nextFloat() * panelW;
                float grainW = 0.5f + rng.nextFloat() * 1.0f;
                float alpha = 0.08f + rng.nextFloat() * 0.15f;
                sg.setColour((rng.nextBool() ? oakLight : oakGrain).withAlpha(alpha));

                juce::Path grain;
                grain.startNewSubPath(x, 0);
                for (int y = 0; y < height; y += 20)
                    grain.lineTo(x + std::sin(static_cast<float>(y) * 0.015f + i * 0.7f) * 1.5f,
                                 static_cast<float>(y));
                grain.lineTo(x, static_cast<float>(height));
                sg.strokePath(grain, juce::PathStrokeType(grainW));

                auto rg = grain;
                rg.applyTransform(juce::AffineTransform::translation(static_cast<float>(width - panelW), 0));
                sg.strokePath(rg, juce::PathStrokeType(grainW));
            }

            sg.setColour(juce::Colour(0x30000000));
            sg.fillRect(static_cast<float>(panelW - 2), 0.0f, 2.0f, static_cast<float>(height));
            sg.fillRect(static_cast<float>(width - panelW), 0.0f, 2.0f, static_cast<float>(height));
            sg.setColour(oakLight.withAlpha(0.3f));
            sg.fillRect(0.0f, 0.0f, 1.0f, static_cast<float>(height));
            sg.fillRect(static_cast<float>(width - 1), 0.0f, 1.0f, static_cast<float>(height));

            sideCacheKey = width;
            sideCacheH = height;
        }
        g.drawImageAt(sideCache, 0, 0);
    }

    // White oak inner strip
    void drawInnerStrip(juce::Graphics& g, int x, int /*y*/, int width, int height) override
    {
        juce::Colour oakBase(0xffc8bda8);
        juce::Colour oakLight(0xffd6ccba);
        juce::Colour oakGrain(0xffa89880);

        g.setColour(oakBase);
        g.fillRect(x, 0, width, height);

        juce::Random rng(99);
        for (int i = 0; i < 40; ++i)
        {
            float fx = x + rng.nextFloat() * width;
            float yStart = rng.nextFloat() * height * 0.8f;
            float len = 30.0f + rng.nextFloat() * (height * 0.4f);
            float thickness = 0.5f + rng.nextFloat() * 1.5f;
            g.setColour(oakGrain.withAlpha(0.15f + rng.nextFloat() * 0.2f));
            g.drawLine(fx, yStart, fx + rng.nextFloat() * 2.0f - 1.0f, yStart + len, thickness);
        }

        for (int i = 0; i < 12; ++i)
        {
            float fx = x + rng.nextFloat() * width;
            float yStart = rng.nextFloat() * height;
            float len = 10.0f + rng.nextFloat() * 40.0f;
            g.setColour(oakLight.withAlpha(0.1f + rng.nextFloat() * 0.15f));
            g.drawLine(fx, yStart, fx, yStart + len, 1.0f);
        }

        g.setColour(juce::Colour(0x30000000));
        g.drawVerticalLine(x, 0, static_cast<float>(height));
        g.drawVerticalLine(x + width - 1, 0, static_cast<float>(height));
    }

    // White oak top bar with wood grain
    void drawTopBarBackground(juce::Graphics& g, int x, int y, int width, int height) override
    {
        if (width != topBarCacheW || height != topBarCacheH)
        {
            topBarCache = juce::Image(juce::Image::RGB, width, height, false);
            juce::Graphics wg(topBarCache);

            juce::Colour oakBase(0xffc0b49e);
            juce::Colour oakLight(0xffcec2ae);
            juce::Colour oakGrain(0xffa89882);

            wg.setColour(oakBase);
            wg.fillAll();

            juce::Random rng(77);
            for (int i = 0; i < 50; ++i)
            {
                float gy = rng.nextFloat() * height;
                float grainH = 0.5f + rng.nextFloat() * 1.2f;
                float alpha = 0.06f + rng.nextFloat() * 0.12f;

                wg.setColour((rng.nextBool() ? oakLight : oakGrain).withAlpha(alpha));

                juce::Path grain;
                grain.startNewSubPath(0, gy);
                for (int gx = 0; gx < width; gx += 15)
                {
                    float wobble = std::sin(static_cast<float>(gx) * 0.01f + i * 0.9f) * 1.2f;
                    grain.lineTo(static_cast<float>(gx), gy + wobble);
                }
                grain.lineTo(static_cast<float>(width), gy);
                wg.strokePath(grain, juce::PathStrokeType(grainH));
            }

            wg.setColour(juce::Colour(0x25000000));
            wg.fillRect(0, height - 2, width, 2);

            topBarCacheW = width;
            topBarCacheH = height;
        }
        g.drawImageAt(topBarCache, x, y);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IoniqForestLookAndFeel)
};
