#pragma once

#include <JuceHeader.h>
#include <cmath>
#include <array>
#include <vector>
#include <algorithm>
#if JUCE_IOS
#include "MetalVisualizerRenderer.h"
#endif

// MilkDrop-inspired software visualizer.
// Per-pixel warp field with multiple layered effects, audio-reactive
// motion blur, and smooth palette morphing. No OpenGL required.
class ProjectMComponent : public juce::Component, public juce::Timer
{
public:
    static constexpr int WAVE_SIZE = 576;
    static constexpr int NUM_SCENES = 48;
    static constexpr int MAP_RECOMPUTE_FRAMES = 45;

    ProjectMComponent()
    {
        waveBuffer.fill(0.0f);
        buildPalette();
        startTimerHz(60);
    }

    ~ProjectMComponent() override { stopTimer(); }

    // ── Public controls ──
    void nextScene()
    {
        sceneIndex = (sceneIndex + 1) % NUM_SCENES;
        mapFrameCounter = 0;
    }
    void prevScene()
    {
        sceneIndex = (sceneIndex - 1 + NUM_SCENES) % NUM_SCENES;
        mapFrameCounter = 0;
    }
    void randomScene()
    {
        juce::Random& rng = juce::Random::getSystemRandom();
        sceneIndex = rng.nextInt(NUM_SCENES);
        mapFrameCounter = 0;
        paletteA = rng.nextInt(NUM_PALETTES);
        paletteB = (paletteA + 1 + rng.nextInt(NUM_PALETTES - 1)) % NUM_PALETTES;
        paletteMorph = 0.0f;
        buildPalette();
    }
    int getSceneIndex() const { return sceneIndex; }

    void toggleLock() { locked = !locked; }
    bool isLocked() const { return locked; }

    void setBlackBg(bool on) { blackBg = on; buildPalette(); }
    bool isBlackBg() const { return blackBg; }

    void pushSamples(const float* data, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            waveBuffer[writePos] = data[i];
            writePos = (writePos + 1) % WAVE_SIZE;
        }

        float sum = 0.0f;
        for (int i = 0; i < numSamples; ++i)
            sum += data[i] * data[i];
        float rms = std::sqrt(sum / static_cast<float>(juce::jmax(1, numSamples)));
        smoothedRms.store(smoothedRms.load() * 0.85f + rms * 0.15f);

        float cur = smoothedRms.load();
        if (cur > avgRms.load() * 1.8f && cur > 0.02f)
            beatHit.store(true);
        avgRms.store(avgRms.load() * 0.95f + cur * 0.05f);
    }

    void timerCallback() override
    {
        phase += 0.015;
        effectPhase += 0.025;

        float energy = juce::jmin(1.0f, smoothedRms.load() * 5.0f);
        bool beat = beatHit.load();

        // Morph palette continuously
        paletteMorph += 0.003f + energy * 0.01f;
        if (paletteMorph >= 1.0f)
        {
            paletteMorph = 0.0f;
            paletteA = paletteB;
            paletteB = (paletteB + 1) % NUM_PALETTES;
        }
        buildPalette();

        // Rotate palette with energy
        paletteRotation += 0.5f + energy * 3.0f;

        // Beat: chance to change scene
        if (beat && !locked)
        {
            beatCounter++;
            if (beatCounter >= 16)
            {
                beatCounter = 0;
                sceneIndex = (sceneIndex + 1) % NUM_SCENES;
                mapFrameCounter = 0;
            }
        }

        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        int w = getWidth();
        int h = getHeight();
        if (w < 4 || h < 4) return;

        if (w != bufW || h != bufH)
        {
            bufW = w;
            bufH = h;
            vs1.assign(static_cast<size_t>(w * h), 0);
            vs2.assign(static_cast<size_t>(w * h), 0);
            mapDx.assign(static_cast<size_t>(w * h), 0);
            mapDy.assign(static_cast<size_t>(w * h), 0);
            mapFrameCounter = 0;
#if JUCE_IOS
            if (metalRenderer && metalRenderer->isAvailable())
                metalRenderer->initWarpBuffers(w, h);
#endif
        }

        if (mapFrameCounter <= 0)
        {
            computeWarpMap();
            mapFrameCounter = MAP_RECOMPUTE_FRAMES;
#if JUCE_IOS
            if (metalRenderer && metalRenderer->isAvailable())
                metalRenderer->uploadWarpMap(mapDx.data(), mapDy.data(), bufW, bufH);
#endif
        }
        mapFrameCounter--;

        float energy = juce::jmin(1.0f, smoothedRms.load() * 5.0f);
        bool beat = beatHit.exchange(false);

#if JUCE_IOS
        if (tryMetalRender(energy, beat)) return;
#endif

        // CPU fallback
        applyWarpBlur();
        renderWarpedWave(energy);
        renderPulsars(energy, beat);
        renderNebulaField(energy);
        renderFlowParticles(energy, beat);
        std::swap(vs1, vs2);

        int rot = static_cast<int>(paletteRotation) % 256;
        int energyShift = static_cast<int>(energy * 25.0f);

        if (renderImage.getWidth() != w || renderImage.getHeight() != h)
            renderImage = juce::Image(juce::Image::ARGB, w, h, false);
        {
            juce::Image::BitmapData bmp(renderImage, juce::Image::BitmapData::writeOnly);
            for (int y = 0; y < h; ++y)
            {
                for (int x = 0; x < w; ++x)
                {
                    int idx = vs1[static_cast<size_t>(y * w + x)];
                    idx = (juce::jlimit(0, 255, idx) + rot + energyShift) % 256;
                    uint32_t col = palette[static_cast<size_t>(idx)];
                    auto* pixel = bmp.getPixelPointer(x, y);
                    pixel[0] = static_cast<uint8_t>(col & 0xFF);
                    pixel[1] = static_cast<uint8_t>((col >> 8) & 0xFF);
                    pixel[2] = static_cast<uint8_t>((col >> 16) & 0xFF);
                    pixel[3] = 0xFF;
                }
            }
        }
        g.drawImageAt(renderImage, 0, 0);
    }

#if JUCE_IOS
    bool tryMetalRender(float energy, bool beat)
    {
        if (!metalRenderer) metalRenderer = std::make_unique<MetalVisualizerRenderer>();
        if (!metalRenderer->isAvailable()) return false;
        if (!metalRenderer->isAttached())
        {
            if (auto* peer = getPeer())
                metalRenderer->attachToView(peer->getNativeHandle());
        }
        if (!metalRenderer->isAttached()) return false;

        metalRenderer->setBounds(getScreenX() - getTopLevelComponent()->getScreenX(),
                                  getScreenY() - getTopLevelComponent()->getScreenY(),
                                  getWidth(), getHeight());

        metalRenderer->executeWarpBlur();

        // Collect effect points for all effects
        std::vector<EffectPoint> points;
        collectEffectPoints(points, energy, beat);
        if (!points.empty())
        {
            metalRenderer->uploadEffectPoints(points.data(), static_cast<int>(points.size()));
            metalRenderer->executeEffects();
        }

        metalRenderer->swapWarpBuffers();

        PaletteGPUUniforms pu {};
        pu.paletteRotation = static_cast<int>(paletteRotation) % 256;
        pu.energyOffset = static_cast<int>(energy * 25.0f);
        pu.brightMult = 1.0f;
        metalRenderer->renderPalette(palette.data(), pu);
        return true;
    }

    void collectEffectPoints(std::vector<EffectPoint>& points, float energy, bool beat)
    {
        float t = static_cast<float>(effectPhase);
        float cx = bufW * 0.5f, cy = bufH * 0.5f;

        // Nebula blobs
        for (int n = 0; n < 5; ++n)
        {
            EffectPoint ep;
            ep.x = bufW * (0.5f + 0.35f * std::sin(t * 0.4f * (n + 1) + n * 1.2f));
            ep.y = bufH * (0.5f + 0.35f * std::cos(t * 0.3f * (n + 1) + n * 0.8f));
            ep.radius = 6.0f + energy * 10.0f + 4.0f * std::sin(t * (1.0f + n * 0.5f));
            ep.brightness = 40.0f + energy * 80.0f;
            points.push_back(ep);
        }

        // Waveform (simplified for GPU)
        std::array<float, WAVE_SIZE> wave;
        int rp = writePos;
        for (int i = 0; i < WAVE_SIZE; ++i)
            wave[i] = waveBuffer[(rp + i) % WAVE_SIZE];
        int brightness = static_cast<int>(120 + energy * 135);

        for (int i = 1; i < WAVE_SIZE; i += 3)
        {
            float frac = static_cast<float>(i) / WAVE_SIZE;
            float sample = wave[i] * (1.0f + energy * 3.0f);
            float angle = frac * juce::MathConstants<float>::twoPi + t * 0.5f;
            float r = juce::jmin(bufW, bufH) * 0.2f + sample * bufH * 0.2f;
            EffectPoint ep;
            ep.x = cx + std::cos(angle) * r;
            ep.y = cy + std::sin(angle) * r;
            ep.radius = 0.0f;
            ep.brightness = static_cast<float>(brightness);
            points.push_back(ep);
        }

        // Pulsars — expanding ring points on active pulsars
        for (int p = 0; p < MAX_PULSARS; ++p)
        {
            if (pulsarAge[p] > 1.0f) continue;

            float age = pulsarAge[p];
            float radius = age * juce::jmin(bufW, bufH) * 0.4f;
            float fade = 1.0f - age;
            float pBrt = fade * (100.0f + energy * 100.0f);

            float pcx = pulsarX[p];
            float pcy = pulsarY[p];
            int numRingPts = juce::jmax(20, juce::jmin(static_cast<int>(radius * 6.28f), 400));

            for (int i = 0; i < numRingPts; i += 2) // sample every other point for GPU efficiency
            {
                float angle = static_cast<float>(i) / numRingPts * juce::MathConstants<float>::twoPi;
                EffectPoint ep;
                ep.x = pcx + std::cos(angle) * radius;
                ep.y = pcy + std::sin(angle) * radius;
                ep.radius = 0.0f;
                ep.brightness = pBrt;
                points.push_back(ep);
            }
        }

        // Flow particles — random dot placement across the field
        {
            int numParticles = static_cast<int>(30 + energy * 80);
            uint32_t seed = static_cast<uint32_t>(t * 500.0);
            float addBrt = 30.0f + energy * 60.0f;
            if (beat) addBrt += 40.0f;

            for (int i = 0; i < numParticles; ++i)
            {
                seed = seed * 1664525u + 1013904223u;
                float fpx = static_cast<float>(seed % static_cast<uint32_t>(bufW));
                seed = seed * 1664525u + 1013904223u;
                float fpy = static_cast<float>(seed % static_cast<uint32_t>(bufH));

                EffectPoint ep;
                ep.x = fpx;
                ep.y = fpy;
                ep.radius = 0.0f;
                ep.brightness = addBrt;
                points.push_back(ep);
            }
        }
    }
#endif

private:
    juce::Image renderImage;

    std::array<float, WAVE_SIZE> waveBuffer;
    int writePos = 0;
    std::atomic<float> smoothedRms { 0.0f };
    std::atomic<float> avgRms { 0.0f };
    std::atomic<bool> beatHit { false };

    double phase = 0.0;
    double effectPhase = 0.0;
    float paletteRotation = 0.0f;
    int mapFrameCounter = 0;
    int sceneIndex = 0;
    bool locked = false;
    bool blackBg = false;
    int beatCounter = 0;

    int bufW = 0, bufH = 0;
    std::vector<int> vs1, vs2;
    std::vector<int> mapDx, mapDy;

    // Palette morphing between two palette styles
    static constexpr int NUM_PALETTES = 12;
    int paletteA = 0, paletteB = 1;
    float paletteMorph = 0.0f;
    std::array<uint32_t, 256> palette;

    // ── Palette system ──
    static uint32_t packRGB(float r, float g, float b)
    {
        return (0xFFu << 24)
             | (static_cast<uint32_t>(juce::jlimit(0.0f, 255.0f, r * 255.0f)) << 16)
             | (static_cast<uint32_t>(juce::jlimit(0.0f, 255.0f, g * 255.0f)) << 8)
             |  static_cast<uint32_t>(juce::jlimit(0.0f, 255.0f, b * 255.0f));
    }

    static uint32_t lerpRGB(uint32_t a, uint32_t b, float t)
    {
        float ar = static_cast<float>((a >> 16) & 0xFF) / 255.0f;
        float ag = static_cast<float>((a >> 8) & 0xFF) / 255.0f;
        float ab = static_cast<float>(a & 0xFF) / 255.0f;
        float br = static_cast<float>((b >> 16) & 0xFF) / 255.0f;
        float bg = static_cast<float>((b >> 8) & 0xFF) / 255.0f;
        float bb = static_cast<float>(b & 0xFF) / 255.0f;
        return packRGB(ar + (br - ar) * t, ag + (bg - ag) * t, ab + (bb - ab) * t);
    }

    void getPaletteStops(int style, uint32_t* stops, int& count)
    {
        switch (style)
        {
            case 0: // Plasma
                stops[0] = packRGB(0.1f,0,0.2f); stops[1] = packRGB(0.8f,0,0.4f);
                stops[2] = packRGB(1,0.5f,0); stops[3] = packRGB(1,1,0);
                stops[4] = packRGB(0,0.8f,1); stops[5] = packRGB(0.1f,0,0.5f);
                count = 6; break;
            case 1: // Embers
                stops[0] = packRGB(0,0,0); stops[1] = packRGB(0.4f,0,0);
                stops[2] = packRGB(0.8f,0.15f,0); stops[3] = packRGB(1,0.5f,0);
                stops[4] = packRGB(1,0.9f,0.3f); stops[5] = packRGB(1,1,0.8f);
                count = 6; break;
            case 2: // Deep Sea
                stops[0] = packRGB(0,0.02f,0.05f); stops[1] = packRGB(0,0.15f,0.3f);
                stops[2] = packRGB(0,0.4f,0.5f); stops[3] = packRGB(0.2f,0.7f,0.6f);
                stops[4] = packRGB(0.5f,1,0.8f); stops[5] = packRGB(0.9f,1,0.95f);
                count = 6; break;
            case 3: // Neon Ride
                stops[0] = packRGB(0,0,0); stops[1] = packRGB(0,0.2f,0.6f);
                stops[2] = packRGB(0.8f,0,1); stops[3] = packRGB(1,0,0.4f);
                stops[4] = packRGB(1,0.8f,0); stops[5] = packRGB(0,1,0.5f);
                count = 6; break;
            case 4: // Infrared
                stops[0] = packRGB(0,0,0.05f); stops[1] = packRGB(0.3f,0,0.15f);
                stops[2] = packRGB(0.7f,0,0.05f); stops[3] = packRGB(1,0.2f,0);
                stops[4] = packRGB(1,0.6f,0); stops[5] = packRGB(1,1,0.5f);
                count = 6; break;
            case 5: // Aurora
                stops[0] = packRGB(0,0.03f,0.05f); stops[1] = packRGB(0,0.3f,0.2f);
                stops[2] = packRGB(0.1f,0.7f,0.4f); stops[3] = packRGB(0.3f,1,0.7f);
                stops[4] = packRGB(0.6f,0.4f,1); stops[5] = packRGB(0.2f,0.1f,0.5f);
                count = 6; break;
            case 6: // Copper
                stops[0] = packRGB(0.02f,0.01f,0); stops[1] = packRGB(0.25f,0.12f,0.03f);
                stops[2] = packRGB(0.6f,0.3f,0.08f); stops[3] = packRGB(0.85f,0.55f,0.15f);
                stops[4] = packRGB(1,0.8f,0.35f); stops[5] = packRGB(1,0.95f,0.7f);
                count = 6; break;
            case 7: // Rainbow
                stops[0] = packRGB(1,0,0); stops[1] = packRGB(1,0.5f,0);
                stops[2] = packRGB(1,1,0); stops[3] = packRGB(0,1,0);
                stops[4] = packRGB(0,0.5f,1); stops[5] = packRGB(0.5f,0,1);
                count = 6; break;
            case 8: // Toxic
                stops[0] = packRGB(0,0.02f,0); stops[1] = packRGB(0,0.25f,0);
                stops[2] = packRGB(0.15f,0.7f,0); stops[3] = packRGB(0.5f,1,0);
                stops[4] = packRGB(0.85f,1,0.2f); stops[5] = packRGB(1,1,0.7f);
                count = 6; break;
            case 9: // Candy
                stops[0] = packRGB(1,0.2f,0.4f); stops[1] = packRGB(1,0.55f,0.25f);
                stops[2] = packRGB(1,0.85f,0.25f); stops[3] = packRGB(0.25f,1,0.45f);
                stops[4] = packRGB(0.25f,0.7f,1); stops[5] = packRGB(0.7f,0.25f,1);
                count = 6; break;
            case 10: // Electric
                stops[0] = packRGB(0,0,0); stops[1] = packRGB(0.15f,0,0.4f);
                stops[2] = packRGB(0.4f,0,1); stops[3] = packRGB(0.8f,0.4f,1);
                stops[4] = packRGB(1,0.8f,1); stops[5] = packRGB(1,1,1);
                count = 6; break;
            default: // Sunset
                stops[0] = packRGB(0.2f,0,0.05f); stops[1] = packRGB(0.6f,0.05f,0);
                stops[2] = packRGB(1,0.3f,0); stops[3] = packRGB(1,0.6f,0.1f);
                stops[4] = packRGB(1,0.85f,0.3f); stops[5] = packRGB(1,0.95f,0.7f);
                count = 6; break;
        }
    }

    void buildPalette()
    {
        uint32_t stopsA[8], stopsB[8];
        int countA = 0, countB = 0;
        getPaletteStops(paletteA, stopsA, countA);
        getPaletteStops(paletteB, stopsB, countB);

        for (int i = 0; i < 256; ++i)
        {
            // Build color from palette A
            float posA = static_cast<float>(i) / 256.0f * static_cast<float>(countA);
            int idxA = static_cast<int>(posA) % countA;
            int nextA = (idxA + 1) % countA;
            float fracA = posA - std::floor(posA);
            uint32_t colA = lerpRGB(stopsA[idxA], stopsA[nextA], fracA);

            // Build color from palette B
            float posB = static_cast<float>(i) / 256.0f * static_cast<float>(countB);
            int idxB = static_cast<int>(posB) % countB;
            int nextB = (idxB + 1) % countB;
            float fracB = posB - std::floor(posB);
            uint32_t colB = lerpRGB(stopsB[idxB], stopsB[nextB], fracB);

            // Morph between A and B
            palette[static_cast<size_t>(i)] = lerpRGB(colA, colB, paletteMorph);
        }

        // Force low palette values to black for black background mode
        if (blackBg)
        {
            uint32_t black = packRGB(0, 0, 0);
            for (int i = 0; i < 64; ++i)
            {
                float t = static_cast<float>(i) / 64.0f;
                palette[static_cast<size_t>(i)] = lerpRGB(black, palette[static_cast<size_t>(i)], t * t);
            }
        }
    }

    // ── 16 warp scenes — each is a different per-pixel motion field ──
    void computeWarpMap()
    {
        if (bufW < 4 || bufH < 4) return;

        float cx = bufW * 0.5f;
        float cy = bufH * 0.5f;
        float t = static_cast<float>(phase);
        float energy = juce::jmin(1.0f, smoothedRms.load() * 5.0f);

        for (int y = 0; y < bufH; ++y)
        {
            for (int x = 0; x < bufW; ++x)
            {
                float dx = static_cast<float>(x) - cx;
                float dy = static_cast<float>(y) - cy;
                float dist = std::sqrt(dx * dx + dy * dy) + 0.001f;
                float nx = dx, ny = dy;

                switch (sceneIndex)
                {
                    case 0: // Spiral inward
                    {
                        float zoom = 0.96f + 0.02f * std::sin(t * 0.5f);
                        float rot = 0.03f + 0.01f * std::sin(t * 0.3f) + energy * 0.02f;
                        nx = dx * zoom; ny = dy * zoom;
                        float c = std::cos(rot), s = std::sin(rot);
                        float rx = nx * c - ny * s;
                        ny = nx * s + ny * c;
                        nx = rx;
                        break;
                    }
                    case 1: // Zoom pulse
                    {
                        float zoom = 0.95f + 0.04f * std::sin(t * 1.2f) + energy * 0.03f;
                        nx = dx * zoom; ny = dy * zoom;
                        break;
                    }
                    case 2: // Vortex
                    {
                        float twist = (0.04f + energy * 0.03f) * std::sin(t * 0.4f);
                        float angle = twist + 0.0005f * dist;
                        float c = std::cos(angle), s = std::sin(angle);
                        nx = dx * 0.97f; ny = dy * 0.97f;
                        float rx = nx * c - ny * s;
                        ny = nx * s + ny * c;
                        nx = rx;
                        break;
                    }
                    case 3: // Wave distortion
                    {
                        float wave = 3.0f * std::sin(static_cast<float>(y) * 0.02f + t * 2.0f) * (1.0f + energy);
                        nx = dx * 0.97f + wave;
                        ny = dy * 0.97f;
                        break;
                    }
                    case 4: // Kaleidoscope
                    {
                        float angle = std::atan2(dy, dx);
                        float sectors = 6.0f;
                        float sectorAngle = std::fmod(angle + juce::MathConstants<float>::pi, juce::MathConstants<float>::twoPi / sectors);
                        sectorAngle = std::abs(sectorAngle - juce::MathConstants<float>::twoPi / sectors * 0.5f);
                        float newAngle = sectorAngle + 0.02f * std::sin(t * 0.6f);
                        float zoom = 0.97f;
                        nx = std::cos(newAngle) * dist * zoom - dx;
                        ny = std::sin(newAngle) * dist * zoom - dy;
                        nx += dx * 0.97f; ny += dy * 0.97f;
                        break;
                    }
                    case 5: // Ripple
                    {
                        float ripple = 3.0f * std::sin(dist * 0.05f - t * 3.0f) * (1.0f + energy * 2.0f);
                        float invDist = 1.0f / (dist + 1.0f);
                        nx = (dx + dx * invDist * ripple) * 0.97f;
                        ny = (dy + dy * invDist * ripple) * 0.97f;
                        break;
                    }
                    case 6: // Double spiral
                    {
                        float rot = 0.025f * std::sin(t * 0.4f + dist * 0.003f);
                        float zoom = 0.96f + 0.015f * std::cos(t * 0.7f);
                        nx = dx * zoom; ny = dy * zoom;
                        float c = std::cos(rot), s = std::sin(rot);
                        float rx = nx * c - ny * s;
                        ny = nx * s + ny * c;
                        nx = rx;
                        break;
                    }
                    case 7: // Tunnel
                    {
                        float zoom = 0.94f + 0.04f * (dist / (juce::jmin(bufW, bufH) * 0.5f));
                        float rot = 0.02f / (dist * 0.01f + 1.0f);
                        nx = dx * zoom; ny = dy * zoom;
                        float c = std::cos(rot), s = std::sin(rot);
                        float rx = nx * c - ny * s;
                        ny = nx * s + ny * c;
                        nx = rx;
                        break;
                    }
                    case 8: // Pinwheel — inner spins fast, outer slow
                    {
                        float maxDist = juce::jmin(bufW, bufH) * 0.5f;
                        float normDist = juce::jmin(1.0f, dist / maxDist);
                        float rot = (0.08f + energy * 0.04f) * (1.0f - normDist * normDist) * std::sin(t * 0.6f);
                        float zoom = 0.97f;
                        nx = dx * zoom; ny = dy * zoom;
                        float c = std::cos(rot), s = std::sin(rot);
                        float rx = nx * c - ny * s;
                        ny = nx * s + ny * c;
                        nx = rx;
                        break;
                    }
                    case 9: // Breathing — periodic zoom in/out with slight rotation
                    {
                        float breath = 0.96f + 0.03f * std::sin(t * 0.8f) + energy * 0.02f * std::sin(t * 1.6f);
                        float rot = 0.008f * std::sin(t * 0.4f);
                        nx = dx * breath; ny = dy * breath;
                        float c = std::cos(rot), s = std::sin(rot);
                        float rx = nx * c - ny * s;
                        ny = nx * s + ny * c;
                        nx = rx;
                        break;
                    }
                    case 10: // Fractal zoom — recursive zoom with angular distortion
                    {
                        float angle = std::atan2(dy, dx);
                        float zoom = 0.95f + 0.02f * std::sin(t * 0.5f);
                        float angleWarp = 0.03f * std::sin(angle * 3.0f + t) + 0.02f * std::sin(angle * 5.0f - t * 0.7f);
                        float newAngle = angle + angleWarp + energy * 0.015f;
                        nx = std::cos(newAngle) * dist * zoom - cx + cx;
                        ny = std::sin(newAngle) * dist * zoom - cy + cy;
                        nx -= cx; ny -= cy;
                        break;
                    }
                    case 11: // Wave grid — horizontal + vertical wave displacement
                    {
                        float waveH = 3.5f * std::sin(static_cast<float>(y) * 0.025f + t * 2.0f) * (1.0f + energy);
                        float waveV = 3.5f * std::sin(static_cast<float>(x) * 0.025f + t * 1.7f) * (1.0f + energy);
                        nx = dx * 0.97f + waveH;
                        ny = dy * 0.97f + waveV;
                        break;
                    }
                    case 12: // Centrifuge — fast rotation at edges, still at center
                    {
                        float maxDist = juce::jmin(bufW, bufH) * 0.5f;
                        float normDist = juce::jmin(1.0f, dist / maxDist);
                        float rot = normDist * normDist * (0.06f + energy * 0.04f) * std::sin(t * 0.5f);
                        float zoom = 0.96f + 0.02f * normDist;
                        nx = dx * zoom; ny = dy * zoom;
                        float c = std::cos(rot), s = std::sin(rot);
                        float rx = nx * c - ny * s;
                        ny = nx * s + ny * c;
                        nx = rx;
                        break;
                    }
                    case 13: // Diamond — warp toward 4 corner attractors
                    {
                        float zoom = 0.97f;
                        float attractStrength = 0.015f + energy * 0.01f;
                        // Four attractors at diamond positions
                        float ax[4] = { cx, cx + cx * 0.6f, cx, cx - cx * 0.6f };
                        float ay[4] = { cy - cy * 0.6f, cy, cy + cy * 0.6f, cy };
                        float pullX = 0.0f, pullY = 0.0f;
                        for (int a = 0; a < 4; ++a)
                        {
                            float adx = ax[a] - static_cast<float>(x);
                            float ady = ay[a] - static_cast<float>(y);
                            float aDist = std::sqrt(adx * adx + ady * ady) + 1.0f;
                            float weight = std::sin(t * 0.5f + a * 1.57f);
                            weight = weight * weight * attractStrength;
                            pullX += adx / aDist * weight;
                            pullY += ady / aDist * weight;
                        }
                        nx = dx * zoom + pullX;
                        ny = dy * zoom + pullY;
                        break;
                    }
                    case 14: // Elastic — rubber-band stretch and snap back
                    {
                        float stretchPhase = std::sin(t * 1.5f);
                        float stretchX = 0.97f + 0.03f * stretchPhase * (1.0f + energy * 0.5f);
                        float stretchY = 0.97f - 0.03f * stretchPhase * (1.0f + energy * 0.5f);
                        float rot = 0.01f * std::cos(t * 0.6f);
                        nx = dx * stretchX; ny = dy * stretchY;
                        float c = std::cos(rot), s = std::sin(rot);
                        float rx = nx * c - ny * s;
                        ny = nx * s + ny * c;
                        nx = rx;
                        break;
                    }
                    case 15: // Chaos — multiple competing rotation centers
                    {
                        float zoom = 0.96f;
                        float totalRot = 0.0f;
                        for (int rc = 0; rc < 3; ++rc)
                        {
                            float rcx = cx + cx * 0.4f * std::sin(t * (0.3f + rc * 0.2f) + rc * 2.09f);
                            float rcy = cy + cy * 0.4f * std::cos(t * (0.25f + rc * 0.15f) + rc * 2.09f);
                            float rdx = static_cast<float>(x) - rcx;
                            float rdy = static_cast<float>(y) - rcy;
                            float rDist = std::sqrt(rdx * rdx + rdy * rdy) + 1.0f;
                            float influence = 1.0f / (1.0f + rDist * 0.01f);
                            float dir = (rc % 2 == 0) ? 1.0f : -1.0f;
                            totalRot += dir * influence * (0.03f + energy * 0.02f);
                        }
                        nx = dx * zoom; ny = dy * zoom;
                        float c = std::cos(totalRot), s = std::sin(totalRot);
                        float rx = nx * c - ny * s;
                        ny = nx * s + ny * c;
                        nx = rx;
                        break;
                    }
                    case 16: // Whirlpool — strong center rotation with inward pull
                    {
                        float rot = 0.06f + energy * 0.04f;
                        float pull = 0.94f + 0.02f * std::sin(t * 0.5f);
                        nx = dx * pull; ny = dy * pull;
                        float c = std::cos(rot), s = std::sin(rot);
                        float rx = nx * c - ny * s;
                        ny = nx * s + ny * c; nx = rx;
                        break;
                    }
                    case 17: // Heartbeat — rhythmic zoom pulse synced to energy
                    {
                        float pulse = 0.96f + 0.04f * std::sin(t * 4.0f) * (0.5f + energy);
                        nx = dx * pulse; ny = dy * pulse;
                        break;
                    }
                    case 18: // Tornado — fast twist that increases with distance
                    {
                        float normDist = dist / (juce::jmin(bufW, bufH) * 0.5f);
                        float twist = normDist * 0.08f * (1.0f + energy);
                        float zoom = 0.97f;
                        nx = dx * zoom; ny = dy * zoom;
                        float c = std::cos(twist), s = std::sin(twist);
                        float rx = nx * c - ny * s;
                        ny = nx * s + ny * c; nx = rx;
                        break;
                    }
                    case 19: // Lava lamp — slow bulging blobs rising and falling
                    {
                        float bulge = 3.0f * std::sin(static_cast<float>(y) * 0.03f + t) * (1.0f + energy * 0.5f);
                        float rise = 2.0f * std::cos(static_cast<float>(x) * 0.025f + t * 0.6f);
                        nx = dx * 0.97f + bulge;
                        ny = dy * 0.96f - rise;
                        break;
                    }
                    case 20: // Wormhole — zoom into center with heavy rotation
                    {
                        float zoom = 0.92f + 0.03f * std::sin(t * 0.7f);
                        float rot = 0.08f + 0.04f * std::sin(t * 0.3f) + energy * 0.05f;
                        nx = dx * zoom; ny = dy * zoom;
                        float c = std::cos(rot), s = std::sin(rot);
                        float rx = nx * c - ny * s;
                        ny = nx * s + ny * c; nx = rx;
                        break;
                    }
                    case 21: // Starburst — radial rays emanating outward
                    {
                        float angle = std::atan2(dy, dx);
                        float rayPhase = std::sin(angle * 8.0f + t * 2.0f) * 0.02f;
                        float zoom = 0.97f + rayPhase;
                        nx = dx * zoom; ny = dy * zoom;
                        float rot = 0.01f * std::sin(t * 0.4f);
                        float c = std::cos(rot), s = std::sin(rot);
                        float rx = nx * c - ny * s;
                        ny = nx * s + ny * c; nx = rx;
                        break;
                    }
                    case 22: // Liquid mirror — horizontal reflection with wave
                    {
                        float wave = 4.0f * std::sin(static_cast<float>(x) * 0.015f + t * 1.5f) * (1.0f + energy);
                        float mirror = dy > 0 ? -0.02f : 0.02f;
                        nx = dx * 0.97f;
                        ny = dy * 0.96f + wave + mirror * dist * 0.1f;
                        break;
                    }
                    case 23: // Galaxy arms — logarithmic spiral
                    {
                        float angle = std::atan2(dy, dx);
                        float logDist = std::log(dist + 1.0f);
                        float spiralRot = 0.02f + 0.01f * std::sin(logDist * 2.0f - t);
                        float zoom = 0.97f;
                        nx = dx * zoom; ny = dy * zoom;
                        float c = std::cos(spiralRot), s = std::sin(spiralRot);
                        float rx = nx * c - ny * s;
                        ny = nx * s + ny * c; nx = rx;
                        break;
                    }
                    case 24: // Jellyfish — pulsing tentacles from top
                    {
                        float ty = static_cast<float>(y) / bufH;
                        float tentacle = 5.0f * std::sin(static_cast<float>(x) * 0.04f + t * 2.0f + ty * 3.0f) * ty;
                        float pulse = 0.97f + 0.02f * std::sin(t * 3.0f) * (1.0f - ty);
                        nx = dx * pulse + tentacle * (1.0f + energy);
                        ny = dy * 0.97f + 1.0f;
                        break;
                    }
                    case 25: // Siphon — drain from bottom-right corner
                    {
                        float drainX = bufW * 0.8f, drainY = bufH * 0.8f;
                        float ddx = static_cast<float>(x) - drainX;
                        float ddy = static_cast<float>(y) - drainY;
                        float dDist = std::sqrt(ddx * ddx + ddy * ddy) + 1.0f;
                        float pull = 3.0f / dDist;
                        float rot = 0.05f / (dDist * 0.01f + 1.0f);
                        nx = dx * 0.97f - ddx * pull * 0.01f;
                        ny = dy * 0.97f - ddy * pull * 0.01f;
                        float c = std::cos(rot), s = std::sin(rot);
                        float rx = nx * c - ny * s;
                        ny = nx * s + ny * c; nx = rx;
                        break;
                    }
                    case 26: // Northern lights — vertical shimmer
                    {
                        float shimmer = 3.0f * std::sin(static_cast<float>(x) * 0.02f + t + std::sin(t * 0.3f) * 2.0f);
                        float drift = 1.5f * std::cos(static_cast<float>(y) * 0.015f + t * 0.4f);
                        nx = dx * 0.97f + drift * (1.0f + energy * 0.5f);
                        ny = dy * 0.96f + shimmer;
                        break;
                    }
                    case 27: // Pendulum — swinging rotation that reverses
                    {
                        float swing = 0.05f * std::sin(t * 1.2f) * (1.0f + energy);
                        float zoom = 0.97f;
                        nx = dx * zoom; ny = dy * zoom;
                        float c = std::cos(swing), s = std::sin(swing);
                        float rx = nx * c - ny * s;
                        ny = nx * s + ny * c; nx = rx;
                        break;
                    }
                    case 28: // Earthquake — random horizontal displacement
                    {
                        float quake = 4.0f * std::sin(static_cast<float>(y) * 0.08f + t * 5.0f) * energy;
                        float jitter = 2.0f * std::cos(static_cast<float>(x) * 0.06f + t * 4.0f) * energy;
                        nx = dx * 0.97f + quake;
                        ny = dy * 0.97f + jitter;
                        break;
                    }
                    case 29: // Eye of Sauron — concentric rings with pulsing hole
                    {
                        float normDist = dist / (juce::jmin(bufW, bufH) * 0.5f);
                        float ringPulse = std::sin(normDist * 15.0f - t * 3.0f) * 0.01f;
                        float holeZoom = (normDist < 0.1f) ? 0.90f : 0.97f + ringPulse;
                        nx = dx * holeZoom; ny = dy * holeZoom;
                        break;
                    }
                    case 30: // Plasma ball — electric tendrils from center
                    {
                        float angle = std::atan2(dy, dx);
                        float tendril = std::sin(angle * 6.0f + t * 3.0f) * 0.03f * (1.0f + energy * 2.0f);
                        float zoom = 0.96f + tendril;
                        float rot = 0.015f + tendril * 0.5f;
                        nx = dx * zoom; ny = dy * zoom;
                        float c = std::cos(rot), s = std::sin(rot);
                        float rx = nx * c - ny * s;
                        ny = nx * s + ny * c; nx = rx;
                        break;
                    }
                    case 31: // DNA helix — double spiral with offset
                    {
                        float angle = std::atan2(dy, dx);
                        float helix1 = std::sin(angle * 2.0f + dist * 0.02f - t * 2.0f) * 0.02f;
                        float helix2 = std::cos(angle * 2.0f + dist * 0.02f - t * 2.0f) * 0.02f;
                        float zoom = 0.97f;
                        nx = dx * zoom + helix1 * dist * 0.3f;
                        ny = dy * zoom + helix2 * dist * 0.3f;
                        break;
                    }
                    case 32: // Supernova — outward explosion that reverses
                    {
                        float phase = std::sin(t * 0.4f);
                        float zoom = phase > 0 ? (0.94f + phase * 0.04f) : (1.02f + phase * 0.04f);
                        float rot = 0.02f * phase;
                        nx = dx * zoom; ny = dy * zoom;
                        float c = std::cos(rot), s = std::sin(rot);
                        float rx = nx * c - ny * s;
                        ny = nx * s + ny * c; nx = rx;
                        break;
                    }
                    case 33: // Tidal wave — massive horizontal sweep
                    {
                        float wave = 8.0f * std::sin(static_cast<float>(y) * 0.01f + t * 0.8f) * (0.5f + energy);
                        nx = dx * 0.97f + wave;
                        ny = dy * 0.97f;
                        float rot = 0.005f * std::cos(t * 0.5f);
                        float c = std::cos(rot), s = std::sin(rot);
                        float rx = nx * c - ny * s;
                        ny = nx * s + ny * c; nx = rx;
                        break;
                    }
                    case 34: // Flower bloom — petals opening outward
                    {
                        float angle = std::atan2(dy, dx);
                        int petals = 5 + static_cast<int>(energy * 3);
                        float petalPhase = std::sin(angle * petals + t) * 0.03f;
                        float zoom = 0.96f + petalPhase;
                        float rot = 0.01f * std::sin(t * 0.2f);
                        nx = dx * zoom; ny = dy * zoom;
                        float c = std::cos(rot), s = std::sin(rot);
                        float rx = nx * c - ny * s;
                        ny = nx * s + ny * c; nx = rx;
                        break;
                    }
                    case 35: // Black hole — extreme center pull with frame dragging
                    {
                        float normDist = dist / (juce::jmin(bufW, bufH) * 0.5f);
                        float pull = 0.90f + 0.08f * normDist;
                        float frameDrag = 0.1f / (normDist + 0.1f);
                        nx = dx * pull; ny = dy * pull;
                        float c = std::cos(frameDrag), s = std::sin(frameDrag);
                        float rx = nx * c - ny * s;
                        ny = nx * s + ny * c; nx = rx;
                        break;
                    }
                    case 36: // Crosshatch — diagonal wave grid
                    {
                        float d1 = 3.0f * std::sin((static_cast<float>(x) + static_cast<float>(y)) * 0.02f + t * 2.0f);
                        float d2 = 3.0f * std::cos((static_cast<float>(x) - static_cast<float>(y)) * 0.02f + t * 1.5f);
                        nx = dx * 0.97f + d1 * (1.0f + energy);
                        ny = dy * 0.97f + d2 * (1.0f + energy);
                        break;
                    }
                    case 37: // Radar sweep — single rotating beam
                    {
                        float angle = std::atan2(dy, dx);
                        float sweepAngle = std::fmod(t * 1.5f, 6.2832f);
                        float angleDiff = std::abs(angle - sweepAngle);
                        if (angleDiff > 3.14159f) angleDiff = 6.2832f - angleDiff;
                        float sweep = (angleDiff < 0.3f) ? 0.03f * (1.0f - angleDiff / 0.3f) : 0.0f;
                        float zoom = 0.97f + sweep;
                        nx = dx * zoom; ny = dy * zoom;
                        float rot = 0.01f;
                        float c = std::cos(rot), s = std::sin(rot);
                        float rx = nx * c - ny * s;
                        ny = nx * s + ny * c; nx = rx;
                        break;
                    }
                    case 38: // Infinity loop — figure-8 flow
                    {
                        float fx = static_cast<float>(x) / bufW - 0.5f;
                        float fy = static_cast<float>(y) / bufH - 0.5f;
                        float pushX = std::sin(fy * 6.2832f + t) * 3.0f;
                        float pushY = std::sin(fx * 12.566f + t * 0.7f) * 3.0f;
                        nx = dx * 0.97f + pushX * (1.0f + energy * 0.5f);
                        ny = dy * 0.97f + pushY * (1.0f + energy * 0.5f);
                        break;
                    }
                    case 39: // Waterfall — downward cascade with side waves
                    {
                        float cascade = 3.0f + energy * 4.0f;
                        float sideWave = 2.0f * std::sin(static_cast<float>(y) * 0.03f + t * 2.0f);
                        nx = dx * 0.97f + sideWave;
                        ny = dy * 0.96f + cascade;
                        break;
                    }
                    case 40: // Hypnotic — slow deep zoom with gentle multi-fold rotation
                    {
                        float zoom = 0.95f + 0.01f * std::sin(t * 0.2f);
                        float rot = 0.015f + 0.01f * std::sin(t * 0.15f + dist * 0.002f);
                        nx = dx * zoom; ny = dy * zoom;
                        float c = std::cos(rot), s = std::sin(rot);
                        float rx = nx * c - ny * s;
                        ny = nx * s + ny * c; nx = rx;
                        break;
                    }
                    case 41: // Shatter — outward with angular fragmentation
                    {
                        float angle = std::atan2(dy, dx);
                        float sector = std::floor(angle * 3.0f / 3.14159f) * 3.14159f / 3.0f;
                        float sectorOffset = (angle - sector) * 2.0f;
                        float zoom = 0.98f + std::sin(sector * 5.0f + t) * 0.02f;
                        nx = dx * zoom + std::cos(sector) * sectorOffset * energy * 2.0f;
                        ny = dy * zoom + std::sin(sector) * sectorOffset * energy * 2.0f;
                        break;
                    }
                    case 42: // Smoke rings — concentric expanding rings with drift
                    {
                        float normDist = dist / (juce::jmin(bufW, bufH) * 0.5f);
                        float ringPush = std::sin(normDist * 10.0f - t * 2.0f) * 0.02f * (1.0f + energy);
                        float zoom = 0.97f + ringPush;
                        nx = dx * zoom + 1.0f;
                        ny = dy * zoom - 0.5f;
                        break;
                    }
                    case 43: // Moth to flame — attract to a wandering point
                    {
                        float attractX = cx + cx * 0.3f * std::sin(t * 0.5f);
                        float attractY = cy + cy * 0.3f * std::cos(t * 0.4f);
                        float adx = static_cast<float>(x) - attractX;
                        float ady = static_cast<float>(y) - attractY;
                        float aDist = std::sqrt(adx * adx + ady * ady) + 1.0f;
                        float pull = 5.0f / aDist * (1.0f + energy);
                        nx = dx * 0.97f - adx * pull * 0.005f;
                        ny = dy * 0.97f - ady * pull * 0.005f;
                        float rot = 0.02f / (aDist * 0.005f + 1.0f);
                        float c = std::cos(rot), s = std::sin(rot);
                        float rx = nx * c - ny * s;
                        ny = nx * s + ny * c; nx = rx;
                        break;
                    }
                    case 44: // Moiré pattern — interfering zoom fields
                    {
                        float cx2 = cx + cx * 0.3f * std::sin(t * 0.3f);
                        float cy2 = cy + cy * 0.3f * std::cos(t * 0.4f);
                        float dx2 = static_cast<float>(x) - cx2;
                        float dy2 = static_cast<float>(y) - cy2;
                        float dist2 = std::sqrt(dx2 * dx2 + dy2 * dy2) + 1.0f;
                        float zoom1 = 0.97f + std::sin(dist * 0.05f) * 0.01f;
                        float zoom2 = 0.97f + std::sin(dist2 * 0.05f) * 0.01f;
                        float blend = 0.5f + 0.5f * std::sin(t * 0.5f);
                        float zoom = zoom1 * blend + zoom2 * (1.0f - blend);
                        nx = dx * zoom; ny = dy * zoom;
                        break;
                    }
                    case 45: // Solar flare — radial ejections from random angles
                    {
                        float angle = std::atan2(dy, dx);
                        float flare = std::sin(angle * 3.0f + t * 4.0f) + std::sin(angle * 7.0f - t * 3.0f);
                        float ejection = flare > 1.2f ? 0.04f * (1.0f + energy * 2.0f) : 0.0f;
                        float zoom = 0.96f + ejection;
                        float rot = 0.008f + energy * 0.01f;
                        nx = dx * zoom; ny = dy * zoom;
                        float c = std::cos(rot), s = std::sin(rot);
                        float rx = nx * c - ny * s;
                        ny = nx * s + ny * c; nx = rx;
                        break;
                    }
                    case 46: // Zipper — alternating horizontal bands pulling opposite ways
                    {
                        int band = static_cast<int>(static_cast<float>(y) / bufH * 8.0f);
                        float dir = (band % 2 == 0) ? 1.0f : -1.0f;
                        float pull = 3.0f * dir * (1.0f + energy) * std::sin(t * 1.5f + band);
                        nx = dx * 0.97f + pull;
                        ny = dy * 0.97f;
                        float rot = 0.005f * dir;
                        float c = std::cos(rot), s = std::sin(rot);
                        float rx = nx * c - ny * s;
                        ny = nx * s + ny * c; nx = rx;
                        break;
                    }
                    default: // Cosmic drift — gentle everything
                    {
                        float zoom = 0.97f + 0.01f * std::sin(t * 0.3f);
                        float rot = 0.01f * std::sin(t * 0.2f + dist * 0.001f);
                        float wave = 2.0f * std::sin(static_cast<float>(x) * 0.01f + t) * energy;
                        nx = dx * zoom + wave * 0.3f;
                        ny = dy * zoom;
                        float c = std::cos(rot), s = std::sin(rot);
                        float rx = nx * c - ny * s;
                        ny = nx * s + ny * c; nx = rx;
                        break;
                    }
                }

                int sx = static_cast<int>(cx + nx);
                int sy = static_cast<int>(cy + ny);
                size_t idx = static_cast<size_t>(y * bufW + x);
                mapDx[idx] = juce::jlimit(0, bufW - 1, sx) - x;
                mapDy[idx] = juce::jlimit(0, bufH - 1, sy) - y;
            }
        }
    }

    void applyWarpBlur()
    {
        if (bufW < 4 || bufH < 4) return;

        for (int y = 0; y < bufH; ++y)
        {
            for (int x = 0; x < bufW; ++x)
            {
                size_t i = static_cast<size_t>(y * bufW + x);
                int sx = x + mapDx[i];
                int sy = y + mapDy[i];

                if (sx <= 0 || sx >= bufW - 1 || sy <= 0 || sy >= bufH - 1)
                {
                    sx = juce::jlimit(0, bufW - 1, sx);
                    sy = juce::jlimit(0, bufH - 1, sy);
                    vs2[i] = juce::jmax(0, vs1[static_cast<size_t>(sy * bufW + sx)] - 1);
                    continue;
                }

                int sum = vs1[static_cast<size_t>((sy - 1) * bufW + (sx - 1))]
                        + vs1[static_cast<size_t>((sy - 1) * bufW + sx)] * 2
                        + vs1[static_cast<size_t>((sy - 1) * bufW + (sx + 1))]
                        + vs1[static_cast<size_t>(sy * bufW + (sx - 1))] * 2
                        + vs1[static_cast<size_t>(sy * bufW + sx)] * 4
                        + vs1[static_cast<size_t>(sy * bufW + (sx + 1))] * 2
                        + vs1[static_cast<size_t>((sy + 1) * bufW + (sx - 1))]
                        + vs1[static_cast<size_t>((sy + 1) * bufW + sx)] * 2
                        + vs1[static_cast<size_t>((sy + 1) * bufW + (sx + 1))];

                vs2[i] = juce::jmax(0, (sum >> 4) - 1);
            }
        }
    }

    // ── Effects ──

    // Wave drawing mode — changes with scene for maximum variety
    int waveDrawMode = 0;
    static constexpr int NUM_WAVE_MODES = 8;

    void renderWarpedWave(float energy)
    {
        std::array<float, WAVE_SIZE> wave;
        int rp = writePos;
        for (int i = 0; i < WAVE_SIZE; ++i)
            wave[i] = waveBuffer[(rp + i) % WAVE_SIZE];

        float cx = bufW * 0.5f;
        float cy = bufH * 0.5f;
        float t = static_cast<float>(effectPhase);
        int brightness = static_cast<int>(120 + energy * 135);

        // Pick wave mode based on scene for variety (or override)
        int mode = (waveDrawMode + sceneIndex) % NUM_WAVE_MODES;

        switch (mode)
        {
            case 0: // Dual interleaved circles (original)
            {
                for (int w = 0; w < 2; ++w)
                {
                    float wOffset = w * 3.14159f;
                    for (int i = 1; i < WAVE_SIZE; ++i)
                    {
                        float frac = static_cast<float>(i) / WAVE_SIZE;
                        float sample = wave[i] * (1.0f + energy * 3.0f);
                        float angle = frac * juce::MathConstants<float>::twoPi + t * (0.5f + w * 0.3f) + wOffset;
                        float r = juce::jmin(bufW, bufH) * (0.15f + 0.15f * std::sin(t * 0.3f + w)) + sample * bufH * 0.2f;
                        plotThick(static_cast<int>(cx + std::cos(angle) * r),
                                  static_cast<int>(cy + std::sin(angle) * r), brightness);
                    }
                }
                break;
            }
            case 1: // Horizontal mirrored bars
            {
                for (int i = 0; i < WAVE_SIZE; ++i)
                {
                    float frac = static_cast<float>(i) / WAVE_SIZE;
                    float sample = std::abs(wave[i]) * (1.0f + energy * 2.5f);
                    int px = static_cast<int>(frac * bufW);
                    int barH = static_cast<int>(sample * bufH * 0.4f);
                    int brt = static_cast<int>(brightness * (0.5f + sample));
                    for (int dy = 0; dy < barH; dy += 2)
                    {
                        plotPixel(px, static_cast<int>(cy) - dy, brt);
                        plotPixel(px, static_cast<int>(cy) + dy, brt);
                    }
                }
                break;
            }
            case 2: // Star burst — waveform as radial spokes
            {
                int numSpokes = 12 + static_cast<int>(energy * 12);
                for (int s = 0; s < numSpokes; ++s)
                {
                    float spokeAngle = static_cast<float>(s) / numSpokes * juce::MathConstants<float>::twoPi + t * 0.2f;
                    for (int i = 0; i < WAVE_SIZE; i += 3)
                    {
                        float frac = static_cast<float>(i) / WAVE_SIZE;
                        float sample = wave[i] * (1.0f + energy * 2.0f);
                        float r = frac * juce::jmin(bufW, bufH) * 0.45f + sample * 20.0f;
                        plotThick(static_cast<int>(cx + std::cos(spokeAngle) * r),
                                  static_cast<int>(cy + std::sin(spokeAngle) * r), brightness / 2);
                    }
                }
                break;
            }
            case 3: // Lissajous figure-8
            {
                float freqX = 2.0f + std::floor(std::sin(t * 0.2f) * 2.0f);
                float freqY = 3.0f + std::floor(std::cos(t * 0.15f) * 2.0f);
                for (int i = 0; i < WAVE_SIZE; ++i)
                {
                    float frac = static_cast<float>(i) / WAVE_SIZE;
                    float sample = wave[i] * (1.0f + energy * 2.0f);
                    float angle = frac * juce::MathConstants<float>::twoPi;
                    float rx = juce::jmin(bufW, bufH) * 0.3f + sample * 30.0f;
                    plotThick(static_cast<int>(cx + std::sin(angle * freqX + t * 0.5f) * rx),
                              static_cast<int>(cy + std::cos(angle * freqY + t * 0.3f) * rx * 0.7f), brightness);
                }
                break;
            }
            case 4: // Grid waveform — horizontal + vertical cross
            {
                int step = 4;
                for (int i = 0; i < WAVE_SIZE; i += step)
                {
                    float frac = static_cast<float>(i) / WAVE_SIZE;
                    float sample = wave[i] * (1.0f + energy * 2.5f);
                    // Horizontal wave
                    int hx = static_cast<int>(frac * bufW);
                    int hy = static_cast<int>(cy + sample * bufH * 0.35f);
                    plotThick(hx, hy, brightness);
                    // Vertical wave
                    int vy = static_cast<int>(frac * bufH);
                    int vx = static_cast<int>(cx + sample * bufW * 0.35f);
                    plotThick(vx, vy, brightness);
                }
                break;
            }
            case 5: // Spiral outward
            {
                for (int i = 0; i < WAVE_SIZE; ++i)
                {
                    float frac = static_cast<float>(i) / WAVE_SIZE;
                    float sample = wave[i] * (1.0f + energy * 2.0f);
                    float angle = frac * juce::MathConstants<float>::twoPi * 4.0f + t;
                    float r = frac * juce::jmin(bufW, bufH) * 0.4f + sample * 25.0f;
                    plotThick(static_cast<int>(cx + std::cos(angle) * r),
                              static_cast<int>(cy + std::sin(angle) * r), brightness);
                }
                break;
            }
            case 6: // Diamond/square wave
            {
                for (int i = 0; i < WAVE_SIZE; ++i)
                {
                    float frac = static_cast<float>(i) / WAVE_SIZE;
                    float sample = wave[i] * (1.0f + energy * 2.5f);
                    float side = juce::jmin(bufW, bufH) * 0.3f + sample * 30.0f;
                    // Walk around a diamond shape
                    float px, py;
                    float seg = std::fmod(frac * 4.0f, 4.0f);
                    if (seg < 1.0f)      { px = cx + side * seg;         py = cy - side * (1.0f - seg); }
                    else if (seg < 2.0f) { px = cx + side * (2.0f - seg); py = cy + side * (seg - 1.0f); }
                    else if (seg < 3.0f) { px = cx - side * (seg - 2.0f); py = cy + side * (3.0f - seg); }
                    else                 { px = cx - side * (4.0f - seg); py = cy - side * (seg - 3.0f); }
                    // Rotate over time
                    float ca = std::cos(t * 0.3f), sa = std::sin(t * 0.3f);
                    float rx = (px - cx) * ca - (py - cy) * sa + cx;
                    float ry = (px - cx) * sa + (py - cy) * ca + cy;
                    plotThick(static_cast<int>(rx), static_cast<int>(ry), brightness);
                }
                break;
            }
            case 7: // Scatter dots — random positions weighted by wave amplitude
            {
                uint32_t seed = static_cast<uint32_t>(t * 777.0);
                for (int i = 0; i < WAVE_SIZE; i += 2)
                {
                    float sample = std::abs(wave[i]) * (1.0f + energy * 2.0f);
                    seed = seed * 1664525u + 1013904223u;
                    float angle = static_cast<float>(seed & 0xFFFF) / 65536.0f * juce::MathConstants<float>::twoPi;
                    float r = sample * juce::jmin(bufW, bufH) * 0.45f;
                    int px = static_cast<int>(cx + std::cos(angle) * r);
                    int py = static_cast<int>(cy + std::sin(angle) * r);
                    int brt = static_cast<int>(60 + sample * 300.0f);
                    plotPixel(px, py, brt);
                }
                break;
            }
        }
    }

    void plotPixel(int x, int y, int brightness)
    {
        if (x < 0 || x >= bufW || y < 0 || y >= bufH) return;
        size_t idx = static_cast<size_t>(y * bufW + x);
        vs2[idx] = juce::jmin(255, vs2[idx] + brightness);
    }

    void plotThick(int x, int y, int brightness)
    {
        plotPixel(x, y, brightness);
        plotPixel(x + 1, y, brightness / 2);
        plotPixel(x, y + 1, brightness / 2);
        plotPixel(x - 1, y, brightness / 3);
        plotPixel(x, y - 1, brightness / 3);
    }

    // Pulsars — bright expanding rings on beats
    void renderPulsars(float energy, bool beat)
    {
        float t = static_cast<float>(effectPhase);

        if (beat)
        {
            // Place a new pulsar
            pulsarAge[nextPulsar] = 0.0f;
            pulsarX[nextPulsar] = bufW * (0.3f + 0.4f * static_cast<float>(std::sin(t * 1.7)));
            pulsarY[nextPulsar] = bufH * (0.3f + 0.4f * static_cast<float>(std::cos(t * 1.3)));
            nextPulsar = (nextPulsar + 1) % MAX_PULSARS;
        }

        for (int p = 0; p < MAX_PULSARS; ++p)
        {
            if (pulsarAge[p] > 1.0f) continue;

            float age = pulsarAge[p];
            pulsarAge[p] += 0.03f + energy * 0.02f;

            float radius = age * juce::jmin(bufW, bufH) * 0.4f;
            float fade = 1.0f - age;
            int brightness = static_cast<int>(fade * (100 + energy * 100));

            int cx = static_cast<int>(pulsarX[p]);
            int cy = static_cast<int>(pulsarY[p]);
            int numPoints = static_cast<int>(radius * 6.28f);
            numPoints = juce::jmax(20, juce::jmin(numPoints, 400));

            for (int i = 0; i < numPoints; ++i)
            {
                float angle = static_cast<float>(i) / numPoints * juce::MathConstants<float>::twoPi;
                int px = cx + static_cast<int>(std::cos(angle) * radius);
                int py = cy + static_cast<int>(std::sin(angle) * radius);

                if (px >= 0 && px < bufW && py >= 0 && py < bufH)
                {
                    size_t idx = static_cast<size_t>(py * bufW + px);
                    vs2[idx] = juce::jmin(255, vs2[idx] + brightness);
                }
            }
        }
    }

    static constexpr int MAX_PULSARS = 6;
    float pulsarAge[MAX_PULSARS] = { 2, 2, 2, 2, 2, 2 };
    float pulsarX[MAX_PULSARS] = {};
    float pulsarY[MAX_PULSARS] = {};
    int nextPulsar = 0;

    // Nebula field — slowly drifting bright spots
    void renderNebulaField(float energy)
    {
        float t = static_cast<float>(effectPhase);

        for (int n = 0; n < 5; ++n)
        {
            float nx = bufW * (0.5f + 0.35f * std::sin(t * 0.4f * (n + 1) + n * 1.2f));
            float ny = bufH * (0.5f + 0.35f * std::cos(t * 0.3f * (n + 1) + n * 0.8f));
            float radius = 6.0f + energy * 10.0f + 4.0f * std::sin(t * (1.0f + n * 0.5f));

            int ix = static_cast<int>(nx);
            int iy = static_cast<int>(ny);
            int r = static_cast<int>(radius);

            for (int dy = -r; dy <= r; ++dy)
            {
                for (int dx = -r; dx <= r; ++dx)
                {
                    int px = ix + dx;
                    int py = iy + dy;
                    if (px < 0 || px >= bufW || py < 0 || py >= bufH) continue;

                    float d = std::sqrt(static_cast<float>(dx * dx + dy * dy));
                    if (d > radius) continue;

                    float intensity = (1.0f - d / radius);
                    intensity *= intensity;
                    int add = static_cast<int>(intensity * (40.0f + energy * 80.0f));

                    size_t idx = static_cast<size_t>(py * bufW + px);
                    vs2[idx] = juce::jmin(255, vs2[idx] + add);
                }
            }
        }
    }

    // Flow particles — stream of dots following the warp field
    void renderFlowParticles(float energy, bool beat)
    {
        float t = static_cast<float>(effectPhase);
        int numParticles = static_cast<int>(30 + energy * 80);

        uint32_t seed = static_cast<uint32_t>(t * 500.0);
        for (int i = 0; i < numParticles; ++i)
        {
            seed = seed * 1664525u + 1013904223u;
            int px = static_cast<int>(seed % static_cast<uint32_t>(bufW));
            seed = seed * 1664525u + 1013904223u;
            int py = static_cast<int>(seed % static_cast<uint32_t>(bufH));

            if (px >= 0 && px < bufW && py >= 0 && py < bufH)
            {
                int add = static_cast<int>(30 + energy * 60);
                if (beat) add += 40;
                size_t idx = static_cast<size_t>(py * bufW + px);
                vs2[idx] = juce::jmin(255, vs2[idx] + add);
            }
        }
    }


#if JUCE_IOS
    std::unique_ptr<MetalVisualizerRenderer> metalRenderer;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProjectMComponent)
};
