#pragma once

#include <JuceHeader.h>
#include "DawLookAndFeel.h"
#include "SequencerEngine.h"
#include "HeartRateManager.h"
#include <array>
#include <atomic>
#include <cmath>

// Bio-Resonance Visualizer
// Combines live heart rate from Apple Watch with audio output to create
// geometric patterns that highlight mathematical relationships between
// the user's heartbeat and their music.
//
// When heart BPM and music BPM align in harmonic ratios (1:1, 1:2, 2:3, 3:4),
// the geometry becomes symmetrical and beautiful. When they drift apart,
// the patterns become organic and flowing.
class BioResonanceComponent : public juce::Component, public juce::Timer
{
public:
    BioResonanceComponent(SequencerEngine& eng, HeartRateManager& hrm)
        : engine(eng), heartRate(hrm)
    {
        startTimerHz(30);

        connectButton.setButtonText("Connect Watch");
        connectButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff334455));
        connectButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.8f));
        connectButton.onClick = [this] {
            if (!heartRate.available.load())
            {
                auto* alert = new juce::AlertWindow("HealthKit Not Available",
                    "Health data is not available on this device. "
                    "Make sure you have an Apple Watch paired with your iPhone, "
                    "and that the Health app is set up.",
                    juce::AlertWindow::WarningIcon);
                alert->addButton("OK", 1);
                alert->enterModalState(true, juce::ModalCallbackFunction::create(
                    [alert](int) { delete alert; }), false);
                return;
            }
            heartRate.requestAuthorization();
            connectButton.setButtonText("Connecting...");
        };
        addAndMakeVisible(connectButton);

        infoButton.setButtonText("?");
        infoButton.setColour(juce::TextButton::buttonColourId, juce::Colours::white.withAlpha(0.15f));
        infoButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.7f));
        infoButton.onClick = [] {
            auto* alert = new juce::AlertWindow("BioSync - Heart + Music Resonance",
                "BioSync visualizes the mathematical relationship between your "
                "live heart rate (from Apple Watch) and your music's tempo.\n\n"
                "HOW IT WORKS\n\n"
                "Outer Ring - Musical pulse, synced to your project BPM. "
                "Beat markers orbit at the tempo.\n\n"
                "Inner Ring - Your heartbeat pulse (red). Glows on each heart beat "
                "from Apple Watch data.\n\n"
                "Connecting Geometry - Lissajous-like curves drawn between the rings. "
                "The shape is determined by the ratio of heart BPM to music BPM.\n\n"
                "HARMONIC RATIOS\n\n"
                "When your heart rate and music tempo align in simple mathematical "
                "ratios, the geometry becomes symmetrical:\n\n"
                "1:1 - Heart and music in perfect sync (circle)\n"
                "1:2 - Heart at half the tempo (figure-8)\n"
                "2:3 - Creates triangular patterns\n"
                "3:4 - Square-like geometry\n"
                "5:8 - Near golden ratio (organic spirals)\n\n"
                "COHERENCE\n\n"
                "The percentage shown measures how close you are to a perfect "
                "harmonic ratio. Higher coherence = brighter, warmer colors and "
                "more symmetrical geometry. Low coherence = cooler blue tones "
                "and flowing asymmetric patterns.\n\n"
                "COLORS\n\n"
                "Green - Audio level OK\n"
                "Amber - Audio running hot\n"
                "Red - Clipping detected\n\n"
                "Wear your Apple Watch during a session to see your biological "
                "rhythm interact with your music in real time.",
                juce::AlertWindow::InfoIcon);
            alert->addButton("OK", 1);
            alert->enterModalState(true, juce::ModalCallbackFunction::create(
                [alert](int) { delete alert; }), false);
        };
        addAndMakeVisible(infoButton);
    }

    ~BioResonanceComponent() override { stopTimer(); }

    void resized() override
    {
        infoButton.setBounds(getWidth() - 34, 4, 30, 30);

        // Show Connect button only when no heart rate data
        bool hasHR = heartRate.heartRateBpm.load() >= 30.0;
        connectButton.setVisible(!hasHR);
        if (!hasHR)
            connectButton.setBounds(getWidth() / 2 - 70, getHeight() - 50, 140, 30);
    }

    // Called from audio thread
    void pushSamples(const float* data, int numSamples)
    {
        float peak = 0.0f;
        float rms = 0.0f;
        float bassEnergy = 0.0f, midEnergy = 0.0f, highEnergy = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            float s = std::abs(data[i]);
            if (s > peak) peak = s;
            rms += data[i] * data[i];

            // Store waveform samples into circular buffer
            waveRing[waveWritePos % WAVE_RING_SIZE] = data[i];
            waveWritePos++;
        }

        rms = std::sqrt(rms / static_cast<float>(juce::jmax(1, numSamples)));
        currentPeak.store(peak);
        currentRms.store(rms);

        // Simple band-split energy estimation using sample differences
        for (int i = 1; i < numSamples; ++i)
        {
            float diff = std::abs(data[i] - data[i - 1]);  // high-freq content
            float avg = std::abs(data[i] + data[i - 1]) * 0.5f;  // low-freq content
            bassEnergy += avg * avg;
            highEnergy += diff * diff;
            midEnergy += std::abs(data[i]) * diff;
        }
        int n = juce::jmax(1, numSamples - 1);
        currentBass.store(std::sqrt(bassEnergy / n));
        currentMid.store(std::sqrt(midEnergy / n));
        currentHigh.store(std::sqrt(highEnergy / n));

        // Onset detection for live BPM
        float energy = rms;
        float prevEnergy = onsetPrevEnergy;
        onsetPrevEnergy = energy;

        // Detect onset: energy spike above threshold and cooldown expired
        bool onset = (energy > prevEnergy * 2.5f) && (energy > 0.02f) && (onsetCooldown <= 0);
        if (onset)
        {
            double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
            if (lastOnsetTime > 0.0)
            {
                double interval = now - lastOnsetTime;
                if (interval > 0.2 && interval < 2.0)  // 30-300 BPM range
                {
                    // Store in circular buffer
                    onsetIntervals[onsetWritePos % MAX_ONSET_INTERVALS] = interval;
                    onsetWritePos++;
                    int count = juce::jmin(onsetWritePos, MAX_ONSET_INTERVALS);

                    // Calculate average interval from recent onsets
                    double sum = 0.0;
                    for (int i = 0; i < count; ++i)
                        sum += onsetIntervals[i];
                    double avgInterval = sum / count;

                    if (avgInterval > 0.0)
                        detectedBpm.store(60.0 / avgInterval);
                }
            }
            lastOnsetTime = now;
            onsetCooldown = 4;  // ~4 audio blocks cooldown to avoid double-triggers
        }
        if (onsetCooldown > 0) onsetCooldown--;
    }

    void timerCallback() override
    {
        float peak = currentPeak.load();
        float rms = currentRms.load();
        smoothPeak = smoothPeak * 0.8f + peak * 0.2f;
        smoothRms = smoothRms * 0.8f + rms * 0.2f;
        smoothBass = smoothBass * 0.85f + currentBass.load() * 0.15f;
        smoothMid = smoothMid * 0.85f + currentMid.load() * 0.15f;
        smoothHigh = smoothHigh * 0.85f + currentHigh.load() * 0.15f;

        // Advance phases — use detected BPM when playing live (transport stopped)
        double musicBpm = engine.getBpm();
        double liveBpm = detectedBpm.load();
        if (!engine.isPlaying() && liveBpm > 30.0)
            musicBpm = liveBpm;  // live onset detection overrides when transport is off

        double hrBpm = heartRate.heartRateBpm.load();
        if (hrBpm < 30.0) hrBpm = 72.0;  // fallback resting rate if no data

        // Phase advances per timer tick (30 Hz)
        musicPhase += (musicBpm / 60.0) / 30.0;
        heartPhase += (hrBpm / 60.0) / 30.0;

        // Trigger ripple on heartbeat (phase wraps from ~1.0 to ~0.0)
        float hp = static_cast<float>(std::fmod(heartPhase, 1.0));
        if (hp < prevHeartPhase - 0.5f)
        {
            ripplePhase[rippleIndex % MAX_RIPPLES] = 0.0f;
            rippleAlpha[rippleIndex % MAX_RIPPLES] = 1.0f;
            rippleIndex++;
        }
        prevHeartPhase = hp;

        // Age ripples
        for (int i = 0; i < MAX_RIPPLES; ++i)
        {
            ripplePhase[i] += 0.04f;
            rippleAlpha[i] *= 0.95f;
        }

        // Hide connect button once HR data is available
        bool hasHR = heartRate.heartRateBpm.load() >= 30.0;
        if (hasHR && connectButton.isVisible())
            connectButton.setVisible(false);

        frameCount++;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.fillAll(juce::Colours::black);

        float cx = bounds.getCentreX();
        float cy = bounds.getCentreY();
        float maxR = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.44f;

        double musicBpm = engine.getBpm();
        double liveBpm = detectedBpm.load();
        if (!engine.isPlaying() && liveBpm > 30.0) musicBpm = liveBpm;
        if (musicBpm < 1.0) musicBpm = 120.0;
        double hrBpm = heartRate.heartRateBpm.load();
        if (hrBpm < 30.0) hrBpm = 72.0;

        double ratio = hrBpm / musicBpm;
        auto [nearestNum, nearestDen, coherence] = findNearestHarmonic(ratio);

        float hue = 0.6f - coherence * 0.4f;
        float sat = 0.7f - coherence * 0.3f;
        float bright = 0.5f + coherence * 0.5f;
        juce::Colour baseCol = juce::Colour::fromHSV(hue, sat, bright, 1.0f);
        juce::Colour accentCol = juce::Colour::fromHSV(hue - 0.1f, sat * 0.5f, 1.0f, 1.0f);
        juce::Colour heartCol(0xffff4466);
        juce::Colour bassCol = juce::Colour::fromHSV(0.0f, 0.8f, 0.7f, 1.0f);   // warm red
        juce::Colour midCol = juce::Colour::fromHSV(0.15f, 0.7f, 0.9f, 1.0f);    // gold
        juce::Colour highCol = juce::Colour::fromHSV(0.55f, 0.6f, 0.9f, 1.0f);   // cyan

        float mPhase = static_cast<float>(std::fmod(musicPhase, 1.0));
        float hPhase = static_cast<float>(std::fmod(heartPhase, 1.0));
        float outerR = maxR * (0.88f + smoothPeak * 0.12f);
        float innerR = maxR * 0.30f;
        float midR = (innerR + outerR) * 0.5f;

        // ── 1. Heart pulse ripples (expanding circles from center) ──
        for (int i = 0; i < MAX_RIPPLES; ++i)
        {
            if (rippleAlpha[i] > 0.01f)
            {
                float r = innerR + ripplePhase[i] * (outerR - innerR) * 1.5f;
                g.setColour(heartCol.withAlpha(rippleAlpha[i] * 0.3f));
                g.drawEllipse(cx - r, cy - r, r * 2, r * 2, 1.0f + rippleAlpha[i]);
            }
        }

        // ── 2. Audio waveform ring (radial oscilloscope around outer ring) ──
        {
            juce::Path wavePath;
            int wp = waveWritePos;
            for (int i = 0; i < WAVE_RING_SIZE; ++i)
            {
                float angle = static_cast<float>(i) / WAVE_RING_SIZE * juce::MathConstants<float>::twoPi
                              - juce::MathConstants<float>::halfPi;
                int idx = (wp + i) % WAVE_RING_SIZE;
                float sample = waveRing[idx];
                float r = outerR + sample * maxR * 0.2f;
                float x = cx + std::cos(angle) * r;
                float y = cy + std::sin(angle) * r;
                if (i == 0) wavePath.startNewSubPath(x, y);
                else wavePath.lineTo(x, y);
            }
            wavePath.closeSubPath();
            g.setColour(baseCol.withAlpha(0.25f));
            g.strokePath(wavePath, juce::PathStrokeType(1.0f));
        }

        // ── 3. Frequency band rings (bass=outer, mid=middle, high=inner) ──
        {
            // Bass ring — thick, pulsing
            float bassR = outerR * (0.95f + smoothBass * 2.0f);
            g.setColour(bassCol.withAlpha(juce::jmin(0.3f, smoothBass * 3.0f)));
            g.drawEllipse(cx - bassR, cy - bassR, bassR * 2, bassR * 2, 1.5f + smoothBass * 4.0f);

            // Mid frequency ring
            float midRing = midR * (1.0f + smoothMid * 1.5f);
            g.setColour(midCol.withAlpha(juce::jmin(0.25f, smoothMid * 3.0f)));
            g.drawEllipse(cx - midRing, cy - midRing, midRing * 2, midRing * 2, 1.0f + smoothMid * 2.0f);

            // High frequency sparkles
            int numSparkles = static_cast<int>(smoothHigh * 60.0f);
            for (int i = 0; i < numSparkles; ++i)
            {
                float angle = static_cast<float>(i) / juce::jmax(1, numSparkles) * juce::MathConstants<float>::twoPi
                              + static_cast<float>(frameCount) * 0.1f;
                float r = innerR * 0.8f + smoothHigh * maxR * 0.3f;
                float sx = cx + std::cos(angle) * r;
                float sy = cy + std::sin(angle) * r;
                g.setColour(highCol.withAlpha(0.4f + smoothHigh));
                g.fillEllipse(sx - 1.5f, sy - 1.5f, 3.0f, 3.0f);
            }
        }

        // ── 4. Outer ring with beat markers ──
        {
            float pulseAlpha = std::exp(-mPhase * 8.0f) * 0.6f;
            g.setColour(baseCol.withAlpha(0.15f + pulseAlpha));
            g.drawEllipse(cx - outerR, cy - outerR, outerR * 2, outerR * 2, 1.5f);

            for (int i = 0; i < 4; ++i)
            {
                float angle = static_cast<float>(i) / 4 * juce::MathConstants<float>::twoPi
                              - juce::MathConstants<float>::halfPi + mPhase * juce::MathConstants<float>::twoPi;
                float mx = cx + std::cos(angle) * outerR;
                float my = cy + std::sin(angle) * outerR;
                float dotSize = (i == 0) ? 8.0f + smoothPeak * 6.0f : 3.0f + smoothPeak * 2.0f;
                g.setColour(accentCol.withAlpha(0.8f));
                g.fillEllipse(mx - dotSize / 2, my - dotSize / 2, dotSize, dotSize);
            }
        }

        // ── 5. Inner heart ring with pulse glow ──
        {
            float heartPulseAlpha = std::exp(-hPhase * 6.0f) * 0.8f;
            g.setColour(heartCol.withAlpha(0.2f + heartPulseAlpha));
            g.drawEllipse(cx - innerR, cy - innerR, innerR * 2, innerR * 2, 2.0f);

            // Pulsing glow burst on heartbeat
            if (hPhase < 0.2f)
            {
                float glowIntensity = 1.0f - hPhase / 0.2f;
                for (int layer = 0; layer < 3; ++layer)
                {
                    float glowR = innerR * (1.0f + hPhase * (2.0f + layer));
                    g.setColour(heartCol.withAlpha(0.1f * glowIntensity / (layer + 1)));
                    g.drawEllipse(cx - glowR, cy - glowR, glowR * 2, glowR * 2, 1.5f);
                }
            }
        }

        // ── 6. Lissajous connecting geometry (multiple layers) ──
        {
            float audioMod = 0.3f + smoothRms * 0.7f;

            // Layer 1: main Lissajous curve
            for (int layer = 0; layer < 2; ++layer)
            {
                int numPoints = 300;
                juce::Path geoPath;
                float layerOffset = layer * 0.5f;

                for (int i = 0; i < numPoints; ++i)
                {
                    float t = static_cast<float>(i) / static_cast<float>(numPoints);
                    float musicAngle = t * juce::MathConstants<float>::twoPi * static_cast<float>(nearestDen)
                                       + static_cast<float>(musicPhase) * juce::MathConstants<float>::twoPi;
                    float heartAngle = t * juce::MathConstants<float>::twoPi * static_cast<float>(nearestNum)
                                       + static_cast<float>(heartPhase) * juce::MathConstants<float>::twoPi + layerOffset;

                    // Modulate radius with audio bands
                    float bassWave = smoothBass * 0.3f * std::sin(heartAngle * 2.0f);
                    float highWave = smoothHigh * 0.15f * std::sin(musicAngle * 4.0f);
                    float r = innerR + (outerR - innerR) * (0.5f + 0.5f * std::sin(heartAngle) + bassWave + highWave) * audioMod;

                    float x = cx + std::cos(musicAngle) * r;
                    float y = cy + std::sin(musicAngle) * r;

                    if (i == 0) geoPath.startNewSubPath(x, y);
                    else geoPath.lineTo(x, y);
                }
                geoPath.closeSubPath();

                float layerAlpha = (layer == 0) ? 0.4f : 0.15f;
                g.setColour(baseCol.withAlpha(layerAlpha * audioMod));
                g.strokePath(geoPath, juce::PathStrokeType(1.5f - layer * 0.5f + coherence * 1.5f));

                if (coherence > 0.6f && layer == 0)
                {
                    g.setColour(accentCol.withAlpha(0.03f * coherence));
                    g.fillPath(geoPath);
                }
            }
        }

        // ── 7. Resonance particles with trails ──
        {
            float resonancePhase = static_cast<float>(std::fmod(musicPhase * nearestDen - heartPhase * nearestNum, 1.0));
            float resonanceStrength = coherence * smoothPeak;

            int numParticles = static_cast<int>(12 + coherence * 24);
            for (int i = 0; i < numParticles; ++i)
            {
                float pAngle = (static_cast<float>(i) / numParticles + resonancePhase)
                               * juce::MathConstants<float>::twoPi;
                float pR = innerR + (outerR - innerR) * (0.3f + 0.4f * std::sin(pAngle * nearestNum + frameCount * 0.02f));
                float px = cx + std::cos(pAngle) * pR;
                float py = cy + std::sin(pAngle) * pR;

                // Trail
                for (int t = 1; t <= 3; ++t)
                {
                    float trailAngle = pAngle - t * 0.03f;
                    float trailR = innerR + (outerR - innerR) * (0.3f + 0.4f * std::sin(trailAngle * nearestNum + frameCount * 0.02f));
                    float tx = cx + std::cos(trailAngle) * trailR;
                    float ty = cy + std::sin(trailAngle) * trailR;
                    g.setColour(accentCol.withAlpha(resonanceStrength * 0.15f / t));
                    g.fillEllipse(tx - 1.5f, ty - 1.5f, 3.0f, 3.0f);
                }

                float pSize = 2.5f + resonanceStrength * 5.0f;
                g.setColour(accentCol.withAlpha(0.3f + resonanceStrength * 0.5f));
                g.fillEllipse(px - pSize / 2, py - pSize / 2, pSize, pSize);
            }
        }

        // ── 8. Center heart rate display ──
        {
            bool hasHR = heartRate.heartRateBpm.load() >= 30.0;
            if (hasHR)
            {
                float centerGlow = std::exp(-hPhase * 5.0f) * 0.3f;
                g.setColour(heartCol.withAlpha(0.05f + centerGlow));
                g.fillEllipse(cx - innerR * 0.7f, cy - innerR * 0.7f, innerR * 1.4f, innerR * 1.4f);

                g.setColour(juce::Colours::white.withAlpha(0.8f + centerGlow * 0.2f));
                g.setFont(juce::jmax(14.0f, innerR * 0.4f));
                g.drawText(juce::String(static_cast<int>(hrBpm)),
                           static_cast<int>(cx - innerR * 0.5f), static_cast<int>(cy - innerR * 0.25f),
                           static_cast<int>(innerR), static_cast<int>(innerR * 0.4f),
                           juce::Justification::centred);
                g.setFont(juce::jmax(9.0f, innerR * 0.2f));
                g.setColour(juce::Colours::white.withAlpha(0.5f));
                g.drawText("BPM",
                           static_cast<int>(cx - innerR * 0.5f), static_cast<int>(cy + innerR * 0.05f),
                           static_cast<int>(innerR), static_cast<int>(innerR * 0.25f),
                           juce::Justification::centred);
            }
        }

        // ── Info text at bottom ──
        auto textArea = bounds.removeFromBottom(22.0f).reduced(8, 0);
        g.setColour(juce::Colours::white.withAlpha(0.6f));
        g.setFont(11.0f);

        bool hasHR = heartRate.heartRateBpm.load() >= 30.0;
        juce::String ratioStr = juce::String(nearestNum) + ":" + juce::String(nearestDen);
        juce::String coherenceStr = juce::String(static_cast<int>(coherence * 100)) + "%";

        bool usingLiveBpm = !engine.isPlaying() && detectedBpm.load() > 30.0;
        juce::String bpmSource = usingLiveBpm ? "Live " : "Music ";

        juce::String info;
        if (hasHR)
            info = "HR " + juce::String(static_cast<int>(hrBpm)) + " BPM   "
                   + bpmSource + juce::String(static_cast<int>(musicBpm)) + " BPM   "
                   + "Ratio " + ratioStr + "   Coherence " + coherenceStr;
        else
            info = "No heart rate data - wear Apple Watch   "
                   + bpmSource + juce::String(static_cast<int>(musicBpm)) + " BPM   "
                   + "(using 72 BPM fallback)";

        g.drawText(info, textArea.toNearestInt(), juce::Justification::centred);
    }

private:
    SequencerEngine& engine;
    HeartRateManager& heartRate;

    std::atomic<float> currentPeak { 0.0f };
    std::atomic<float> currentRms { 0.0f };
    std::atomic<float> currentBass { 0.0f };
    std::atomic<float> currentMid { 0.0f };
    std::atomic<float> currentHigh { 0.0f };
    float smoothPeak = 0.0f;
    float smoothRms = 0.0f;
    float smoothBass = 0.0f;
    float smoothMid = 0.0f;
    float smoothHigh = 0.0f;

    // Audio waveform ring buffer for radial display
    static constexpr int WAVE_RING_SIZE = 512;
    float waveRing[WAVE_RING_SIZE] = {};
    int waveWritePos = 0;

    // Heart pulse ripple state
    static constexpr int MAX_RIPPLES = 5;
    float ripplePhase[MAX_RIPPLES] = {};
    float rippleAlpha[MAX_RIPPLES] = {};
    int rippleIndex = 0;
    float prevHeartPhase = 0.0f;

    // Live BPM detection from audio onsets
    std::atomic<double> detectedBpm { 0.0 };
    float onsetPrevEnergy = 0.0f;
    double lastOnsetTime = 0.0;
    int onsetCooldown = 0;
    static constexpr int MAX_ONSET_INTERVALS = 16;
    double onsetIntervals[MAX_ONSET_INTERVALS] = {};
    int onsetWritePos = 0;

    double musicPhase = 0.0;
    double heartPhase = 0.0;
    int frameCount = 0;

    // Find the nearest simple harmonic ratio (a:b) to a given ratio
    // Returns {numerator, denominator, coherence (0-1)}
    struct HarmonicResult { int num; int den; float coherence; };
    static HarmonicResult findNearestHarmonic(double ratio)
    {
        // Check common harmonic ratios
        static const int candidates[][2] = {
            {1,1}, {1,2}, {2,1}, {2,3}, {3,2}, {3,4}, {4,3},
            {1,3}, {3,1}, {1,4}, {4,1}, {2,5}, {5,2}, {3,5}, {5,3},
            {5,4}, {4,5}, {5,8}, {8,5}  // golden ratio approximation
        };

        int bestNum = 1, bestDen = 1;
        double bestDist = 999.0;

        for (auto& c : candidates)
        {
            double target = static_cast<double>(c[0]) / static_cast<double>(c[1]);
            double dist = std::abs(ratio - target);
            if (dist < bestDist)
            {
                bestDist = dist;
                bestNum = c[0];
                bestDen = c[1];
            }
        }

        // Coherence: how close we are to the perfect ratio (exponential falloff)
        float coherence = static_cast<float>(std::exp(-bestDist * 15.0));
        return { bestNum, bestDen, coherence };
    }

    juce::TextButton infoButton;
    juce::TextButton connectButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BioResonanceComponent)
};
