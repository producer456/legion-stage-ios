#pragma once

#include <JuceHeader.h>
#include "PluginHost.h"

// Session View — Ableton-style clip launcher grid.
// Each column is a track, each row is a clip slot.
// Tap a cell to launch/stop clips. Tap a scene button to launch entire rows.
class SessionViewComponent : public juce::Component, public juce::Timer
{
public:
    static constexpr int VISIBLE_ROWS = 8;
    static constexpr int NUM_TRACKS = 16;

    SessionViewComponent(PluginHost& host)
        : pluginHost(host)
    {
        startTimerHz(30);
    }

    ~SessionViewComponent() override { stopTimer(); }

    void timerCallback() override { repaint(); }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.fillAll(juce::Colour(0xff0a0e14));

        float headerH = 28.0f;
        float sceneW = 50.0f;
        float trackW = (visibleTracks > 0) ? (bounds.getWidth() - sceneW) / static_cast<float>(visibleTracks) : 1.0f;
        float rowH = (VISIBLE_ROWS > 0) ? (bounds.getHeight() - headerH) / static_cast<float>(VISIBLE_ROWS) : 1.0f;

        // ── Track Headers ──
        for (int t = 0; t < visibleTracks; ++t)
        {
            float x = sceneW + t * trackW;
            auto headerBounds = juce::Rectangle<float>(x, 0, trackW, headerH);

            bool isSelected = (t + trackOffset == selectedTrack);
            g.setColour(isSelected ? juce::Colour(0xff2a4050) : juce::Colour(0xff1a2030));
            g.fillRect(headerBounds.reduced(1, 1));

            g.setColour(juce::Colour(0xffb8d8f0));
            g.setFont(11.0f);
            g.drawText("T" + juce::String(t + trackOffset + 1),
                       headerBounds.reduced(4, 2), juce::Justification::centred);
        }

        // ── Scene Buttons (left column) ──
        for (int r = 0; r < VISIBLE_ROWS; ++r)
        {
            float y = headerH + r * rowH;
            auto sceneBounds = juce::Rectangle<float>(0, y, sceneW, rowH);

            g.setColour(juce::Colour(0xff1a2838));
            g.fillRoundedRectangle(sceneBounds.reduced(2), 4.0f);

            g.setColour(juce::Colour(0xff60a0c0));
            g.setFont(10.0f);
            g.drawText(juce::String(r + rowOffset + 1),
                       sceneBounds, juce::Justification::centred);
        }

        // ── Clip Grid ──
        for (int t = 0; t < visibleTracks; ++t)
        {
            int trackIdx = t + trackOffset;
            auto& track = pluginHost.getTrack(trackIdx);
            auto* cp = track.clipPlayer;
            if (!cp) continue;

            float x = sceneW + t * trackW;

            for (int r = 0; r < VISIBLE_ROWS; ++r)
            {
                int slotIdx = r + rowOffset;
                float y = headerH + r * rowH;
                auto cellBounds = juce::Rectangle<float>(x, y, trackW, rowH).reduced(2);

                // Get slot state
                auto slotState = ClipSlot::Empty;
                bool hasContent = false;
                juce::String clipName;

                if (slotIdx < cp->getNumSlots())
                {
                    auto& slot = cp->getSlot(slotIdx);
                    slotState = slot.state.load();
                    hasContent = slot.hasContent();
                    if (slot.clip)
                        clipName = "Clip " + juce::String(slotIdx + 1);
                    else if (slot.audioClip)
                        clipName = "Audio " + juce::String(slotIdx + 1);
                }

                // Cell background color based on state
                juce::Colour cellColor;
                switch (slotState)
                {
                    case ClipSlot::Playing:
                        cellColor = juce::Colour(0xff2a8040); // green
                        break;
                    case ClipSlot::Recording:
                        cellColor = juce::Colour(0xff903030); // red
                        break;
                    case ClipSlot::Armed:
                        cellColor = juce::Colour(0xff807020); // yellow
                        break;
                    case ClipSlot::Stopped:
                        if (hasContent)
                            cellColor = juce::Colour(0xff2a3848); // blue-gray (has clip)
                        else
                            cellColor = juce::Colour(0xff141820); // dark (empty stopped)
                        break;
                    default: // Empty
                        cellColor = juce::Colour(0xff141820);
                        break;
                }

                g.setColour(cellColor);
                g.fillRoundedRectangle(cellBounds, 3.0f);

                // Border
                g.setColour(juce::Colour(0xff2a3040));
                g.drawRoundedRectangle(cellBounds, 3.0f, 0.5f);

                // Clip name or play indicator
                if (hasContent)
                {
                    g.setColour(juce::Colour(0xffc8e4ff).withAlpha(0.8f));
                    g.setFont(9.0f);
                    g.drawText(clipName, cellBounds.reduced(4, 2), juce::Justification::centredLeft);
                }

                // Playing indicator — animated triangle
                if (slotState == ClipSlot::Playing)
                {
                    float triX = cellBounds.getRight() - 12;
                    float triY = cellBounds.getCentreY();
                    juce::Path tri;
                    tri.addTriangle(triX, triY - 4, triX, triY + 4, triX + 6, triY);
                    g.setColour(juce::Colours::white.withAlpha(0.9f));
                    g.fillPath(tri);
                }

                // Recording indicator — red dot
                if (slotState == ClipSlot::Recording)
                {
                    float dotX = cellBounds.getRight() - 10;
                    float dotY = cellBounds.getCentreY();
                    g.setColour(juce::Colours::red);
                    g.fillEllipse(dotX - 4, dotY - 4, 8, 8);
                }
            }
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        float headerH = 28.0f;
        float sceneW = 50.0f;
        float trackW = (getWidth() - sceneW) / static_cast<float>(visibleTracks);
        float rowH = (getHeight() - headerH) / static_cast<float>(VISIBLE_ROWS);

        float mx = e.position.x;
        float my = e.position.y;

        // Scene button click (left column)
        if (mx < sceneW && my >= headerH)
        {
            int row = static_cast<int>((my - headerH) / rowH) + rowOffset;
            launchScene(row);
            return;
        }

        // Track header click
        if (my < headerH && mx >= sceneW)
        {
            int track = static_cast<int>((mx - sceneW) / trackW) + trackOffset;
            if (track >= 0 && track < NUM_TRACKS)
            {
                selectedTrack = track;
                if (onTrackSelected)
                    onTrackSelected(track);
            }
            return;
        }

        // Clip cell click
        if (mx >= sceneW && my >= headerH)
        {
            int track = static_cast<int>((mx - sceneW) / trackW) + trackOffset;
            int row = static_cast<int>((my - headerH) / rowH) + rowOffset;

            if (track >= 0 && track < NUM_TRACKS)
            {
                toggleClipSlot(track, row);
            }
        }
    }

    // Callbacks
    std::function<void(int)> onTrackSelected;

    void setSelectedTrack(int t) { selectedTrack = t; }
    void setVisibleTracks(int n) { visibleTracks = juce::jlimit(4, 16, n); }

    // ── Native-controller-facing API (Launchkey MK4 etc.) ──
    /// Trigger / stop the clip at the visible (row, col) cell.
    /// Maps the controller's 2x8 pad grid onto the first 2 visible
    /// scene rows and the first 8 visible track columns.
    void launchPad(int row, int col)
    {
        const int track = trackOffset + col;
        const int scene = rowOffset + row;
        if (track < 0 || track >= NUM_TRACKS) return;
        toggleClipSlot(track, scene);
    }

    /// Returns 0=empty 1=stopped-with-content 2=playing 3=recording 4=armed.
    int stateForVisibleSlot(int row, int col) const
    {
        const int track = trackOffset + col;
        const int scene = rowOffset + row;
        if (track < 0 || track >= NUM_TRACKS) return 0;
        auto& trk = pluginHost.getTrack(track);
        auto* cp = trk.clipPlayer;
        if (!cp || scene < 0 || scene >= cp->getNumSlots()) return 0;
        auto& slot = cp->getSlot(scene);
        auto st = slot.state.load();
        if (st == ClipSlot::Playing)   return 2;
        if (st == ClipSlot::Recording) return 3;
        if (st == ClipSlot::Armed)     return 4;
        if (slot.hasContent())         return 1;
        return 0;
    }

    void scrollScenes(int delta) { rowOffset = juce::jmax(0, rowOffset + delta); repaint(); }
    int  currentSceneRow() const { return rowOffset; }
    void launchSceneAtRow(int rowOffsetIdx) { launchScene(rowOffsetIdx); }

private:
    PluginHost& pluginHost;
    int selectedTrack = 0;
    int visibleTracks = 8;
    int trackOffset = 0;
    int rowOffset = 0;

    void toggleClipSlot(int trackIdx, int slotIdx)
    {
        auto& track = pluginHost.getTrack(trackIdx);
        auto* cp = track.clipPlayer;
        if (!cp) return;

        if (slotIdx >= cp->getNumSlots()) return;

        auto& slot = cp->getSlot(slotIdx);
        auto state = slot.state.load();

        if (state == ClipSlot::Playing || state == ClipSlot::Recording)
        {
            cp->stopSlot(slotIdx);
        }
        else if (slot.hasContent())
        {
            // Stop any other playing slot on this track first
            for (int s = 0; s < cp->getNumSlots(); ++s)
            {
                if (s != slotIdx && cp->getSlot(s).state.load() == ClipSlot::Playing)
                    cp->stopSlot(s);
            }
            cp->triggerSlot(slotIdx);
        }
    }

    void launchScene(int sceneIdx)
    {
        // Launch or stop clips across all tracks for this scene
        for (int t = 0; t < NUM_TRACKS; ++t)
        {
            auto& track = pluginHost.getTrack(t);
            auto* cp = track.clipPlayer;
            if (!cp) continue;

            if (sceneIdx >= cp->getNumSlots()) continue;

            auto& slot = cp->getSlot(sceneIdx);
            if (slot.hasContent())
            {
                // Stop other playing slots on this track
                for (int s = 0; s < cp->getNumSlots(); ++s)
                {
                    if (s != sceneIdx && cp->getSlot(s).state.load() == ClipSlot::Playing)
                        cp->stopSlot(s);
                }
                cp->triggerSlot(sceneIdx);
            }
        }

        // If transport isn't playing, start it
        auto& eng = pluginHost.getEngine();
        if (!eng.isPlaying())
            eng.play();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SessionViewComponent)
};
