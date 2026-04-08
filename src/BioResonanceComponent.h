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

        float fc = static_cast<float>(frameCount);
        float twoPi = juce::MathConstants<float>::twoPi;
        float halfPi = juce::MathConstants<float>::halfPi;

        // ── 1. Background bloom — subtle radial glow that breathes with heart ──
        {
            float breathe = 0.03f + std::exp(-hPhase * 4.0f) * 0.06f;
            juce::ColourGradient bg(heartCol.withAlpha(breathe), cx, cy,
                                    juce::Colours::transparentBlack, cx, cy + maxR * 1.2f, true);
            g.setGradientFill(bg);
            g.fillRect(bounds);
        }

        // ── 2. Heart pulse ripples (expanding shockwaves) ──
        for (int i = 0; i < MAX_RIPPLES; ++i)
        {
            if (rippleAlpha[i] > 0.01f)
            {
                float r = innerR * 0.5f + ripplePhase[i] * maxR * 1.8f;
                float thickness = 2.0f * rippleAlpha[i];
                g.setColour(heartCol.withAlpha(rippleAlpha[i] * 0.25f));
                g.drawEllipse(cx - r, cy - r, r * 2, r * 2, thickness);
                // Second ring slightly offset for depth
                float r2 = r * 1.05f;
                g.setColour(accentCol.withAlpha(rippleAlpha[i] * 0.1f));
                g.drawEllipse(cx - r2, cy - r2, r2 * 2, r2 * 2, thickness * 0.5f);
            }
        }

        // ── 3. Audio waveform ring — radial oscilloscope ──
        {
            juce::Path wavePath, wavePathInner;
            int wp = waveWritePos;
            for (int i = 0; i < WAVE_RING_SIZE; ++i)
            {
                float angle = static_cast<float>(i) / WAVE_RING_SIZE * twoPi - halfPi;
                int idx = (wp + i) % WAVE_RING_SIZE;
                float sample = waveRing[idx];
                float rOuter = outerR + sample * maxR * 0.25f;
                float rInner = innerR + sample * maxR * 0.15f;
                float cosA = std::cos(angle), sinA = std::sin(angle);

                if (i == 0) { wavePath.startNewSubPath(cx + cosA * rOuter, cy + sinA * rOuter);
                              wavePathInner.startNewSubPath(cx + cosA * rInner, cy + sinA * rInner); }
                else { wavePath.lineTo(cx + cosA * rOuter, cy + sinA * rOuter);
                       wavePathInner.lineTo(cx + cosA * rInner, cy + sinA * rInner); }
            }
            wavePath.closeSubPath();
            wavePathInner.closeSubPath();
            g.setColour(baseCol.withAlpha(0.2f + smoothPeak * 0.2f));
            g.strokePath(wavePath, juce::PathStrokeType(1.0f + smoothPeak));
            g.setColour(heartCol.withAlpha(0.12f + smoothPeak * 0.1f));
            g.strokePath(wavePathInner, juce::PathStrokeType(0.8f));
        }

        // ── 4. Frequency band visuals ──
        {
            // Bass: thick pulsing outer glow
            float bassR = outerR * (1.0f + smoothBass * 3.0f);
            float bassAlpha = juce::jmin(0.35f, smoothBass * 4.0f);
            g.setColour(bassCol.withAlpha(bassAlpha));
            g.drawEllipse(cx - bassR, cy - bassR, bassR * 2, bassR * 2, 2.0f + smoothBass * 6.0f);

            // Bass radial spokes on beats
            if (smoothBass > 0.02f)
            {
                int numSpokes = 8;
                for (int i = 0; i < numSpokes; ++i)
                {
                    float angle = static_cast<float>(i) / numSpokes * twoPi + mPhase * twoPi;
                    float r1 = outerR;
                    float r2 = outerR + smoothBass * maxR * 0.4f;
                    g.setColour(bassCol.withAlpha(smoothBass * 2.0f));
                    g.drawLine(cx + std::cos(angle) * r1, cy + std::sin(angle) * r1,
                               cx + std::cos(angle) * r2, cy + std::sin(angle) * r2,
                               1.0f + smoothBass * 3.0f);
                }
            }

            // Mid: golden arc segments that rotate
            if (smoothMid > 0.01f)
            {
                float midRing = midR * (1.0f + smoothMid * 1.5f);
                int numArcs = 6;
                for (int i = 0; i < numArcs; ++i)
                {
                    float startAngle = static_cast<float>(i) / numArcs * twoPi + fc * 0.015f;
                    float arcLen = 0.3f + smoothMid * 0.5f;
                    juce::Path arc;
                    arc.addCentredArc(cx, cy, midRing, midRing, 0, startAngle, startAngle + arcLen, true);
                    g.setColour(midCol.withAlpha(juce::jmin(0.4f, smoothMid * 4.0f)));
                    g.strokePath(arc, juce::PathStrokeType(1.5f + smoothMid * 2.0f));
                }
            }

            // High: constellation of sparkles that scatter outward
            int numSparkles = static_cast<int>(smoothHigh * 100.0f);
            for (int i = 0; i < numSparkles; ++i)
            {
                float angle = static_cast<float>(i) * 2.399f + fc * 0.08f;  // golden angle
                float r = innerR + (outerR - innerR) * (0.2f + 0.6f * static_cast<float>(i) / juce::jmax(1, numSparkles));
                float jitter = smoothHigh * 8.0f * std::sin(fc * 0.2f + i * 1.7f);
                float sx = cx + std::cos(angle) * (r + jitter);
                float sy = cy + std::sin(angle) * (r + jitter);
                float sparkSize = 1.0f + smoothHigh * 3.0f * (1.0f - static_cast<float>(i) / juce::jmax(1, numSparkles));
                g.setColour(highCol.withAlpha(0.3f + smoothHigh * 0.5f));
                g.fillEllipse(sx - sparkSize, sy - sparkSize, sparkSize * 2, sparkSize * 2);
            }
        }

        // ── 5. Outer ring with reactive beat markers ──
        {
            float pulseAlpha = std::exp(-mPhase * 6.0f) * 0.7f;
            g.setColour(baseCol.withAlpha(0.12f + pulseAlpha));
            g.drawEllipse(cx - outerR, cy - outerR, outerR * 2, outerR * 2, 1.5f + pulseAlpha * 2.0f);

            // Beat flash ring
            if (mPhase < 0.1f)
            {
                float flashR = outerR * (1.0f + mPhase * 0.5f);
                g.setColour(accentCol.withAlpha(0.15f * (1.0f - mPhase / 0.1f)));
                g.drawEllipse(cx - flashR, cy - flashR, flashR * 2, flashR * 2, 2.0f);
            }

            for (int i = 0; i < 8; ++i)
            {
                float angle = static_cast<float>(i) / 8.0f * twoPi - halfPi + mPhase * twoPi;
                float dotR = outerR + smoothPeak * 4.0f;
                float mx = cx + std::cos(angle) * dotR;
                float my = cy + std::sin(angle) * dotR;
                float dotSize = (i % 2 == 0) ? 6.0f + smoothPeak * 8.0f : 2.5f + smoothPeak * 3.0f;
                g.setColour(accentCol.withAlpha(0.6f + smoothPeak * 0.3f));
                g.fillEllipse(mx - dotSize / 2, my - dotSize / 2, dotSize, dotSize);
            }
        }

        // ── 6. Inner heart ring with multi-layer glow ──
        {
            float heartPulse = std::exp(-hPhase * 5.0f);
            g.setColour(heartCol.withAlpha(0.15f + heartPulse * 0.5f));
            g.drawEllipse(cx - innerR, cy - innerR, innerR * 2, innerR * 2, 1.5f + heartPulse * 3.0f);

            if (hPhase < 0.25f)
            {
                float intensity = 1.0f - hPhase / 0.25f;
                for (int layer = 0; layer < 5; ++layer)
                {
                    float glowR = innerR * (1.0f + hPhase * (1.5f + layer * 0.8f));
                    g.setColour(heartCol.withAlpha(0.08f * intensity / (layer * 0.5f + 1)));
                    g.drawEllipse(cx - glowR, cy - glowR, glowR * 2, glowR * 2, 1.5f);
                }
            }
        }

        // ── 7. Lissajous geometry (3 layers with different ratios) ──
        {
            float audioMod = 0.3f + smoothRms * 0.7f;

            for (int layer = 0; layer < 3; ++layer)
            {
                int numPoints = 400;
                juce::Path geoPath;
                float layerMul = 1.0f + layer * 0.5f;
                float layerPhase = layer * 1.047f;  // 60 degree offset

                for (int i = 0; i < numPoints; ++i)
                {
                    float t = static_cast<float>(i) / static_cast<float>(numPoints);
                    float mA = t * twoPi * static_cast<float>(nearestDen) * layerMul
                               + static_cast<float>(musicPhase) * twoPi;
                    float hA = t * twoPi * static_cast<float>(nearestNum) * layerMul
                               + static_cast<float>(heartPhase) * twoPi + layerPhase;

                    float bassW = smoothBass * 0.4f * std::sin(hA * 2.0f);
                    float midW = smoothMid * 0.2f * std::sin(mA * 3.0f);
                    float highW = smoothHigh * 0.15f * std::sin(mA * 5.0f + hA);
                    float r = innerR + (outerR - innerR) * (0.5f + 0.5f * std::sin(hA) + bassW + midW + highW) * audioMod;

                    float x = cx + std::cos(mA) * r;
                    float y = cy + std::sin(mA) * r;

                    if (i == 0) geoPath.startNewSubPath(x, y);
                    else geoPath.lineTo(x, y);
                }
                geoPath.closeSubPath();

                float layerAlpha = (layer == 0) ? 0.35f : (layer == 1) ? 0.15f : 0.08f;
                juce::Colour layerCol = (layer == 0) ? baseCol : (layer == 1) ? accentCol : heartCol;
                g.setColour(layerCol.withAlpha(layerAlpha * audioMod));
                g.strokePath(geoPath, juce::PathStrokeType(2.0f - layer * 0.5f + coherence * 2.0f));

                if (coherence > 0.5f && layer == 0)
                {
                    g.setColour(accentCol.withAlpha(0.04f * coherence));
                    g.fillPath(geoPath);
                }
            }
        }

        // ── 8. Resonance particles with longer trails ──
        {
            float resPhase = static_cast<float>(std::fmod(musicPhase * nearestDen - heartPhase * nearestNum, 1.0));
            float resStrength = coherence * (smoothPeak + smoothBass);

            int numParticles = static_cast<int>(16 + coherence * 32 + smoothPeak * 16);
            for (int i = 0; i < numParticles; ++i)
            {
                float pAngle = (static_cast<float>(i) / numParticles + resPhase) * twoPi;
                float pR = innerR + (outerR - innerR) * (0.2f + 0.5f * std::sin(pAngle * nearestNum + fc * 0.025f));
                float px = cx + std::cos(pAngle) * pR;
                float py = cy + std::sin(pAngle) * pR;

                // Longer trailing afterimages
                for (int t = 1; t <= 5; ++t)
                {
                    float tAngle = pAngle - t * 0.025f;
                    float tR = innerR + (outerR - innerR) * (0.2f + 0.5f * std::sin(tAngle * nearestNum + fc * 0.025f));
                    g.setColour(accentCol.withAlpha(resStrength * 0.1f / t));
                    g.fillEllipse(cx + std::cos(tAngle) * tR - 1.5f, cy + std::sin(tAngle) * tR - 1.5f, 3.0f, 3.0f);
                }

                float pSize = 2.0f + resStrength * 6.0f;
                g.setColour(accentCol.withAlpha(0.4f + resStrength * 0.4f));
                g.fillEllipse(px - pSize / 2, py - pSize / 2, pSize, pSize);

                // Bright core
                g.setColour(juce::Colours::white.withAlpha(resStrength * 0.3f));
                g.fillEllipse(px - 1.0f, py - 1.0f, 2.0f, 2.0f);
            }
        }

        // ── 9. Center — subtle heart rate with inner waveform ──
        {
            bool hasHR = heartRate.heartRateBpm.load() >= 30.0;
            float centerGlow = std::exp(-hPhase * 5.0f) * 0.2f;
            g.setColour(heartCol.withAlpha(0.03f + centerGlow));
            g.fillEllipse(cx - innerR * 0.6f, cy - innerR * 0.6f, innerR * 1.2f, innerR * 1.2f);

            if (hasHR)
            {
                // Small subtle BPM text
                g.setColour(juce::Colours::white.withAlpha(0.35f + centerGlow));
                g.setFont(juce::jmax(10.0f, innerR * 0.25f));
                g.drawText(juce::String(static_cast<int>(hrBpm)),
                           static_cast<int>(cx - innerR * 0.4f), static_cast<int>(cy - innerR * 0.12f),
                           static_cast<int>(innerR * 0.8f), static_cast<int>(innerR * 0.25f),
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
