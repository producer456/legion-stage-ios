#pragma once

#include <JuceHeader.h>
#include "DawLookAndFeel.h"
#include "SequencerEngine.h"
#include <array>
#include <atomic>

// Audio-driven EKG visualizer.
// The waveform morphology is a PQRST cardiac rhythm where:
//   Heart rate = project BPM (synced to transport tempo)
//   R wave amplitude = audio peak level
//   ST segment elevation = sustained loudness (compression)
//   T wave = audio decay/release
//   Arrhythmia = audio transients hitting off-beat
class HeartbeatComponent : public juce::Component, public juce::Timer
{
public:
    HeartbeatComponent(SequencerEngine& eng) : engine(eng)
    {
        startTimerHz(60);
    }

    ~HeartbeatComponent() override { stopTimer(); }

    // Called from audio thread — push mono samples to extract level
    void pushSamples(const float* data, int numSamples)
    {
        float peak = 0.0f;
        float rms = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            float s = std::abs(data[i]);
            if (s > peak) peak = s;
            rms += data[i] * data[i];
        }
        rms = std::sqrt(rms / static_cast<float>(juce::jmax(1, numSamples)));

        currentPeak.store(peak);
        currentRms.store(rms);

        // Detect transients (sharp level increase)
        float prevRms = lastRms.load();
        if (rms > prevRms * 3.0f && rms > 0.05f)
            transientDetected.store(true);
        lastRms.store(rms);
    }

    void timerCallback() override
    {
        float peak = currentPeak.load();
        float rms = currentRms.load();

        // Smooth the levels
        smoothPeak = smoothPeak * 0.7f + peak * 0.3f;
        smoothRms = smoothRms * 0.7f + rms * 0.3f;

        // BPM drives the heart rate
        double bpm = engine.getBpm();
        // Convert BPM to phase step per timer tick (30 Hz timer)
        // One cardiac cycle = one beat
        double beatsPerSecond = bpm / 60.0;
        double phaseStepPerTick = beatsPerSecond / 60.0;

        // Generate EKG samples into circular buffer
        for (int s = 0; s < 3; ++s)
        {
            ekgPhase += phaseStepPerTick * 0.33;  // 3 samples per tick
            float p = static_cast<float>(std::fmod(ekgPhase, 1.0));

            float v = generateEkgSample(p, smoothPeak, smoothRms);

            // Add transient jitter (like a PVC / premature beat)
            if (transientDetected.exchange(false))
                v += 0.3f * std::sin(p * 20.0f);

            ekgBuffer[static_cast<size_t>(writePos)] = v;
            writePos = (writePos + 1) % BUFFER_SIZE;
        }

        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Get theme colors
        uint32_t lcdBg = 0xff000000, lcdText = 0xffb8d8f0;
        uint32_t red = 0xffff4444, amber = 0xffffaa44, green = 0xff44cc66;
        if (auto* lnf = dynamic_cast<DawLookAndFeel*>(&getLookAndFeel()))
        {
            auto& t = lnf->getTheme();
            lcdBg = t.lcdBg; lcdText = t.lcdText;
            red = t.red; amber = t.amber; green = t.green;
        }

        // Background
        g.setColour(juce::Colour(lcdBg));
        g.fillRect(bounds);

        auto inner = bounds.reduced(4.0f);

        // Grid lines (like EKG paper)
        g.setColour(juce::Colour(lcdText).withAlpha(0.06f));
        float gridSpacingX = inner.getWidth() / 30.0f;
        float gridSpacingY = inner.getHeight() / 16.0f;
        for (int i = 0; i <= 30; ++i)
            g.drawVerticalLine(static_cast<int>(inner.getX() + i * gridSpacingX),
                               inner.getY(), inner.getBottom());
        for (int i = 0; i <= 16; ++i)
            g.drawHorizontalLine(static_cast<int>(inner.getY() + i * gridSpacingY),
                                 inner.getX(), inner.getRight());

        // Waveform color based on audio level
        juce::Colour waveCol = smoothPeak > 0.9f ? juce::Colour(red) :
                               smoothPeak > 0.5f ? juce::Colour(amber) :
                               juce::Colour(green);

        // Draw sweep trace from circular buffer
        float waveH = inner.getHeight() * 0.42f;
        float baselineY = inner.getCentreY();
        float waveW = inner.getWidth();
        int gapSize = 10;

        juce::Path wavePath;
        bool pathStarted = false;

        for (int i = 0; i < BUFFER_SIZE; ++i)
        {
            int bufIdx = (writePos + i) % BUFFER_SIZE;
            int distFromCursor = (BUFFER_SIZE - i) % BUFFER_SIZE;
            if (distFromCursor > 0 && distFromCursor <= gapSize)
            {
                pathStarted = false;
                continue;
            }

            float x = inner.getX() + (static_cast<float>(i) / static_cast<float>(BUFFER_SIZE - 1)) * waveW;
            float v = ekgBuffer[static_cast<size_t>(bufIdx)];
            float y = baselineY - v * waveH;

            if (!pathStarted) { wavePath.startNewSubPath(x, y); pathStarted = true; }
            else wavePath.lineTo(x, y);
        }

        g.setColour(waveCol.withAlpha(0.9f));
        g.strokePath(wavePath, juce::PathStrokeType(2.0f));

        // Glow dot at cursor
        int cursorIdx = (writePos - 1 + BUFFER_SIZE) % BUFFER_SIZE;
        float cursorFrac = static_cast<float>((cursorIdx - writePos + BUFFER_SIZE) % BUFFER_SIZE)
                           / static_cast<float>(BUFFER_SIZE - 1);
        float cursorX = inner.getX() + cursorFrac * waveW;
        float cursorV = ekgBuffer[static_cast<size_t>(cursorIdx)];
        float cursorY = baselineY - cursorV * waveH;
        g.setColour(waveCol);
        g.fillEllipse(cursorX - 4, cursorY - 4, 8, 8);
        g.setColour(waveCol.withAlpha(0.3f));
        g.fillEllipse(cursorX - 8, cursorY - 8, 16, 16);

        // BPM and level readout at bottom
        auto textArea = inner.removeFromBottom(18.0f);
        g.setColour(juce::Colour(lcdText).withAlpha(0.7f));
        g.setFont(12.0f);
        int bpm = static_cast<int>(engine.getBpm());
        int levelDb = smoothPeak > 0.0001f ? static_cast<int>(20.0f * std::log10(smoothPeak)) : -60;
        g.drawText(juce::String(bpm) + " BPM   "
                   + juce::String(levelDb) + " dB   "
                   + (smoothPeak > 0.9f ? "CLIP" : smoothPeak > 0.5f ? "HOT" : "OK"),
                   textArea.toNearestInt(), juce::Justification::centred);
    }

private:
    SequencerEngine& engine;

    std::atomic<float> currentPeak { 0.0f };
    std::atomic<float> currentRms { 0.0f };
    std::atomic<float> lastRms { 0.0f };
    std::atomic<bool> transientDetected { false };

    float smoothPeak = 0.0f;
    float smoothRms = 0.0f;

    static constexpr int BUFFER_SIZE = 300;
    std::array<float, BUFFER_SIZE> ekgBuffer {};
    int writePos = 0;
    double ekgPhase = 0.0;

    // Generate one PQRST sample based on audio characteristics
    float generateEkgSample(float phase, float peak, float rms) const
    {
        float v = 0.0f;

        if (phase < 0.12f)
        {
            // P wave — driven by RMS (sustained level = stronger atrial signal)
            float t = (phase - 0.06f) / 0.04f;
            v = (0.08f + rms * 0.15f) * std::exp(-t * t);
        }
        else if (phase < 0.18f)
        {
            // PR segment — flat baseline
            v = 0.0f;
        }
        else if (phase < 0.22f)
        {
            // Q wave — small dip, deeper with louder audio
            float t = (phase - 0.20f) / 0.015f;
            v = -(0.05f + peak * 0.1f) * std::exp(-t * t);
        }
        else if (phase < 0.30f)
        {
            // R wave — main spike, amplitude = audio peak level
            float width = 0.02f - peak * 0.005f;  // sharper at high levels
            width = juce::jmax(0.012f, width);
            float t = (phase - 0.25f) / width;
            float amplitude = 0.2f + peak * 0.8f;  // scales with audio level
            v = amplitude * std::exp(-t * t);
        }
        else if (phase < 0.36f)
        {
            // S wave — negative dip
            float t = (phase - 0.33f) / 0.02f;
            v = -(0.1f + peak * 0.15f) * std::exp(-t * t);
        }
        else if (phase < 0.50f)
        {
            // ST segment — elevated with sustained loudness (compression)
            v = rms * 0.2f;
        }
        else if (phase < 0.68f)
        {
            // T wave — recovery, amplitude tracks audio decay
            float t = (phase - 0.58f) / 0.06f;
            float tAmp = 0.1f + rms * 0.2f;
            // Inversion when clipping (like ischemia)
            if (peak > 0.95f) tAmp = -tAmp * 0.6f;
            v = tAmp * std::exp(-t * t);
        }
        // else: isoelectric baseline (v = 0)

        return v;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HeartbeatComponent)
};
