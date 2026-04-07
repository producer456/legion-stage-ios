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
        for (int i = 0; i < numSamples; ++i)
        {
            float s = std::abs(data[i]);
            if (s > peak) peak = s;
            rms += data[i] * data[i];
        }
        rms = std::sqrt(rms / static_cast<float>(juce::jmax(1, numSamples)));
        currentPeak.store(peak);
        currentRms.store(rms);
    }

    void timerCallback() override
    {
        float peak = currentPeak.load();
        float rms = currentRms.load();
        smoothPeak = smoothPeak * 0.8f + peak * 0.2f;
        smoothRms = smoothRms * 0.8f + rms * 0.2f;

        // Advance phases
        double musicBpm = engine.getBpm();
        double hrBpm = heartRate.heartRateBpm.load();
        if (hrBpm < 30.0) hrBpm = 72.0;  // fallback resting rate if no data

        // Phase advances per timer tick (30 Hz)
        musicPhase += (musicBpm / 60.0) / 30.0;
        heartPhase += (hrBpm / 60.0) / 30.0;

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

        // Background
        g.fillAll(juce::Colours::black);

        float cx = bounds.getCentreX();
        float cy = bounds.getCentreY();
        float maxR = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.42f;

        double musicBpm = engine.getBpm();
        if (musicBpm < 1.0) musicBpm = 120.0;
        double hrBpm = heartRate.heartRateBpm.load();
        if (hrBpm < 30.0) hrBpm = 72.0;

        // Calculate the ratio and find nearest harmonic
        double ratio = hrBpm / musicBpm;
        auto [nearestNum, nearestDen, coherence] = findNearestHarmonic(ratio);

        // Colors based on coherence (how close to a perfect harmonic ratio)
        // High coherence = warm gold/white, low = cool blue/purple
        float hue = 0.6f - coherence * 0.4f;  // blue(0.6) → warm(0.2)
        float sat = 0.7f - coherence * 0.3f;   // less saturated when aligned
        float bright = 0.5f + coherence * 0.5f;
        juce::Colour baseCol = juce::Colour::fromHSV(hue, sat, bright, 1.0f);
        juce::Colour accentCol = juce::Colour::fromHSV(hue - 0.1f, sat * 0.5f, 1.0f, 1.0f);

        float mPhase = static_cast<float>(std::fmod(musicPhase, 1.0));
        float hPhase = static_cast<float>(std::fmod(heartPhase, 1.0));

        // ── Outer ring: music pulse ──
        float outerR = maxR * (0.85f + smoothPeak * 0.15f);
        {
            // Draw ring with pulse on beat
            float pulseAlpha = std::exp(-mPhase * 8.0f) * 0.6f;
            g.setColour(baseCol.withAlpha(0.15f + pulseAlpha));
            g.drawEllipse(cx - outerR, cy - outerR, outerR * 2, outerR * 2, 1.5f);

            // Beat markers around the ring
            int numBeats = 4;  // quarter note divisions
            for (int i = 0; i < numBeats; ++i)
            {
                float angle = static_cast<float>(i) / numBeats * juce::MathConstants<float>::twoPi
                              - juce::MathConstants<float>::halfPi + mPhase * juce::MathConstants<float>::twoPi;
                float mx = cx + std::cos(angle) * outerR;
                float my = cy + std::sin(angle) * outerR;
                float dotSize = (i == 0) ? 6.0f : 3.0f;
                g.setColour(accentCol.withAlpha(0.7f));
                g.fillEllipse(mx - dotSize / 2, my - dotSize / 2, dotSize, dotSize);
            }
        }

        // ── Inner ring: heart pulse ──
        float innerR = maxR * 0.35f;
        {
            float heartPulseAlpha = std::exp(-hPhase * 6.0f) * 0.8f;
            juce::Colour heartCol = juce::Colour(0xffff4466).withAlpha(0.2f + heartPulseAlpha);
            g.setColour(heartCol);
            g.drawEllipse(cx - innerR, cy - innerR, innerR * 2, innerR * 2, 2.0f);

            // Heart pulse glow
            if (hPhase < 0.15f)
            {
                float glowR = innerR * (1.0f + hPhase * 2.0f);
                g.setColour(juce::Colour(0xffff4466).withAlpha(0.15f * (1.0f - hPhase / 0.15f)));
                g.drawEllipse(cx - glowR, cy - glowR, glowR * 2, glowR * 2, 1.0f);
            }
        }

        // ── Connecting geometry: Lissajous-like patterns ──
        // The ratio between heart and music BPM determines the symmetry
        {
            int numPoints = 200;
            float audioMod = 0.3f + smoothRms * 0.7f;

            juce::Path geoPath;
            for (int i = 0; i < numPoints; ++i)
            {
                float t = static_cast<float>(i) / static_cast<float>(numPoints);

                // Two frequencies creating the pattern
                float musicAngle = t * juce::MathConstants<float>::twoPi * static_cast<float>(nearestDen)
                                   + static_cast<float>(musicPhase) * juce::MathConstants<float>::twoPi;
                float heartAngle = t * juce::MathConstants<float>::twoPi * static_cast<float>(nearestNum)
                                   + static_cast<float>(heartPhase) * juce::MathConstants<float>::twoPi;

                // Radius oscillates between inner and outer ring
                float r = innerR + (outerR - innerR) * (0.5f + 0.5f * std::sin(heartAngle)) * audioMod;

                // Angular position combines both frequencies
                float angle = musicAngle;

                float x = cx + std::cos(angle) * r;
                float y = cy + std::sin(angle) * r;

                if (i == 0) geoPath.startNewSubPath(x, y);
                else geoPath.lineTo(x, y);
            }
            geoPath.closeSubPath();

            g.setColour(baseCol.withAlpha(0.4f * audioMod));
            g.strokePath(geoPath, juce::PathStrokeType(1.0f + coherence * 1.5f));

            // Fill with very low alpha when coherence is high (sacred geometry glow)
            if (coherence > 0.7f)
            {
                g.setColour(accentCol.withAlpha(0.04f * coherence));
                g.fillPath(geoPath);
            }
        }

        // ── Resonance particles: spawn at intersections of the two rhythms ──
        {
            float resonancePhase = static_cast<float>(std::fmod(musicPhase * static_cast<double>(nearestDen)
                                                                - heartPhase * static_cast<double>(nearestNum), 1.0));
            float resonanceStrength = coherence * smoothPeak;

            int numParticles = static_cast<int>(8 + coherence * 16);
            for (int i = 0; i < numParticles; ++i)
            {
                float pAngle = (static_cast<float>(i) / numParticles + resonancePhase)
                               * juce::MathConstants<float>::twoPi;
                float pR = innerR + (outerR - innerR) * (0.3f + 0.4f * std::sin(pAngle * static_cast<float>(nearestNum) + static_cast<float>(frameCount) * 0.02f));
                float px = cx + std::cos(pAngle) * pR;
                float py = cy + std::sin(pAngle) * pR;

                float pSize = 2.0f + resonanceStrength * 4.0f;
                float pAlpha = 0.2f + resonanceStrength * 0.6f;
                g.setColour(accentCol.withAlpha(pAlpha));
                g.fillEllipse(px - pSize / 2, py - pSize / 2, pSize, pSize);
            }
        }

        // ── Info text at bottom ──
        auto textArea = bounds.removeFromBottom(22.0f).reduced(8, 0);
        g.setColour(juce::Colours::white.withAlpha(0.6f));
        g.setFont(11.0f);

        bool hasHR = heartRate.heartRateBpm.load() >= 30.0;
        juce::String ratioStr = juce::String(nearestNum) + ":" + juce::String(nearestDen);
        juce::String coherenceStr = juce::String(static_cast<int>(coherence * 100)) + "%";

        juce::String info;
        if (hasHR)
            info = "HR " + juce::String(static_cast<int>(hrBpm)) + " BPM   "
                   + "Music " + juce::String(static_cast<int>(musicBpm)) + " BPM   "
                   + "Ratio " + ratioStr + "   Coherence " + coherenceStr;
        else
            info = "No heart rate data - wear Apple Watch   "
                   + juce::String("Music ") + juce::String(static_cast<int>(musicBpm)) + " BPM   "
                   + "(using 72 BPM fallback)";

        g.drawText(info, textArea.toNearestInt(), juce::Justification::centred);
    }

private:
    SequencerEngine& engine;
    HeartRateManager& heartRate;

    std::atomic<float> currentPeak { 0.0f };
    std::atomic<float> currentRms { 0.0f };
    float smoothPeak = 0.0f;
    float smoothRms = 0.0f;

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
