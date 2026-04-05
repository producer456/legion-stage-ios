#pragma once

#include <JuceHeader.h>
#include "PluginHost.h"
#include "DawLookAndFeel.h"

// Minimap bar above the timeline — shows the whole project and lets you drag to scroll.
class ArrangerMinimapComponent : public juce::Component
{
public:
    ArrangerMinimapComponent(PluginHost& host) : pluginHost(host) {}

    // Call from timeline's paint or timer to keep in sync
    void setViewRange(double scrollXBeats, double visibleBeats, double totalBeats)
    {
        viewStart = scrollXBeats;
        viewWidth = visibleBeats;
        projectLength = juce::jmax(totalBeats, 16.0);
        repaint();
    }

    // Callback when user drags to a new position (in beats)
    std::function<void(double newScrollX)> onScroll;

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Background
        uint32_t bgColor = 0xff0a0e14;
        uint32_t lcdBlue = 0xffb8d8f0;
        uint32_t viewColor = 0x50ffffff;
        if (auto* lnf = dynamic_cast<DawLookAndFeel*>(&getLookAndFeel()))
        {
            bgColor = lnf->getTheme().lcdBg;
            lcdBlue = lnf->getTheme().lcdText;
        }

        g.setColour(juce::Colour(bgColor));
        g.fillRect(bounds);

        float h = bounds.getHeight();
        float mapX = bounds.getX();
        float mapW = bounds.getWidth();

        // Draw clip blocks in OLED blue
        g.setColour(juce::Colour(lcdBlue).withAlpha(0.7f));
        for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
        {
            auto& track = pluginHost.getTrack(t);
            if (track.clipPlayer == nullptr) continue;

            float trackY = h * static_cast<float>(t) / PluginHost::NUM_TRACKS;
            float trackH = h / PluginHost::NUM_TRACKS;

            for (int s = 0; s < ClipPlayerNode::NUM_SLOTS; ++s)
            {
                auto& slot = track.clipPlayer->getSlot(s);
                if (!slot.hasContent()) continue;

                double clipStart = 0.0, clipLen = 0.0;
                if (slot.clip)
                {
                    clipStart = slot.clip->timelinePosition;
                    clipLen = slot.clip->lengthInBeats;
                }
                else if (slot.audioClip)
                {
                    clipStart = slot.audioClip->timelinePosition;
                    clipLen = slot.audioClip->lengthInBeats;
                }

                float x1 = mapX + static_cast<float>(clipStart / projectLength) * mapW;
                float x2 = mapX + static_cast<float>((clipStart + clipLen) / projectLength) * mapW;
                g.fillRect(x1, trackY + 1, x2 - x1, trackH - 2);
            }
        }

        // Draw viewport rectangle — clipped to minimap bounds
        float vx = mapX + static_cast<float>(viewStart / projectLength) * mapW;
        float vw = static_cast<float>(viewWidth / projectLength) * mapW;
        vw = juce::jmax(4.0f, vw);
        // Clip to bounds
        float vRight = juce::jmin(vx + vw, bounds.getRight());
        vx = juce::jmax(vx, bounds.getX());
        vw = vRight - vx;
        if (vw > 0)
        {
            g.setColour(juce::Colour(viewColor));
            g.fillRect(vx, bounds.getY(), vw, h);
            g.setColour(juce::Colour(lcdBlue).withAlpha(0.8f));
            g.drawRect(vx, bounds.getY(), vw, h, 1.0f);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override { dragToPosition(e); }
    void mouseDrag(const juce::MouseEvent& e) override { dragToPosition(e); }

private:
    PluginHost& pluginHost;
    double viewStart = 0.0;
    double viewWidth = 16.0;
    double projectLength = 64.0;

    void dragToPosition(const juce::MouseEvent& e)
    {
        float mapW = static_cast<float>(getWidth());

        float frac = static_cast<float>(e.x) / mapW;
        frac = juce::jlimit(0.0f, 1.0f, frac);
        double newScroll = frac * projectLength - viewWidth * 0.5;
        newScroll = juce::jmax(0.0, newScroll);

        if (onScroll)
            onScroll(newScroll);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArrangerMinimapComponent)
};
