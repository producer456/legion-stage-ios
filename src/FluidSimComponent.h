#pragma once

#include <JuceHeader.h>
#include <cmath>
#include <cstring>
#include <array>
#include <vector>
#if JUCE_IOS
#include "MetalVisualizerRenderer.h"
#endif

// Audio-reactive 2D fluid simulation visualizer.
// Maintains velocity and density fields at quarter resolution.
// Audio drives the simulation: bass creates pressure impulses,
// mids create swirling vortices, highs inject colored dye,
// and beats trigger large splashes. CPU fallback renders the
// density field as colored pixels; the real magic is in Metal
// compute shaders (added separately).
class FluidSimComponent : public juce::Component, public juce::Timer
{
public:
    static constexpr int fftOrder = 10;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int numBands = 16;
    static constexpr int NUM_COLOR_MODES = 5;

    FluidSimComponent() { startTimerHz(60); }
    ~FluidSimComponent() override { stopTimer(); }

    void visibilityChanged() override
    {
        if (isVisible())
            startTimerHz(60);
        else
            stopTimer();
    }

    // ── Public controls ──

    void cycleColorMode() { colorMode = (colorMode + 1) % NUM_COLOR_MODES; }
    int getColorMode() const { return colorMode; }
    void setColorMode(int mode) { colorMode = juce::jlimit(0, NUM_COLOR_MODES - 1, mode); }

    void setViscosity(float v) { viscosity = juce::jlimit(0.0001f, 0.1f, v); }
    float getViscosity() const { return viscosity; }

    void toggleVorticity() { vorticityEnabled = !vorticityEnabled; }
    bool isVorticityEnabled() const { return vorticityEnabled; }

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

        // RMS for beat detection (same pattern as GeissComponent)
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
        time += 1.0f / 60.0f;

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

            // Bin into bands (same as ShaderToyComponent)
            for (int b = 0; b < numBands; ++b)
            {
                float startFrac = std::pow(static_cast<float>(b) / numBands, 2.0f);
                float endFrac   = std::pow(static_cast<float>(b + 1) / numBands, 2.0f);
                int startBin = static_cast<int>(startFrac * fftSize * 0.5f);
                int endBin   = juce::jmax(startBin + 1, static_cast<int>(endFrac * fftSize * 0.5f));
                endBin = juce::jmin(endBin, fftSize / 2);

                float peak = 0.0f;
                for (int i = startBin; i < endBin; ++i)
                    peak = juce::jmax(peak, fftData[i]);

                float level = juce::jlimit(0.0f, 1.0f, std::log10(1.0f + peak * 10.0f) * 0.5f);
                bands[b] = bands[b] * 0.7f + level * 0.3f;
            }
        }

        // Run fluid simulation every frame (before paint, so Metal and CPU both get fresh data)
        stepSimulation();

        repaint();
    }

    void stepSimulation()
    {
        int w = getWidth();
        int h = getHeight();
        if (w < 4 || h < 4) return;

        // Grid at 1/3 resolution, capped to 128 for GPU uniform arrays
        int gw = juce::jmin(128, juce::jmax(8, w / 3));
        int gh = juce::jmin(128, juce::jmax(8, h / 3));

        // Reallocate grids on size change
        if (gw != gridW || gh != gridH)
        {
            gridW = gw;
            gridH = gh;
            size_t n = static_cast<size_t>(gw * gh);
            vx.assign(n, 0.0f);
            vy.assign(n, 0.0f);
            density.assign(n, 0.0f);
            vxPrev.assign(n, 0.0f);
            vyPrev.assign(n, 0.0f);
            densityPrev.assign(n, 0.0f);
        }

        // Extract audio features
        float bass   = (bands[0] + bands[1] + bands[2]) / 3.0f;
        float mid    = (bands[5] + bands[6] + bands[7]) / 3.0f;
        float high   = (bands[11] + bands[12] + bands[13]) / 3.0f;
        float energy = 0.0f;
        for (int b = 0; b < numBands; ++b) energy += bands[b];
        energy /= numBands;
        bool beat = beatHit.exchange(false);

        // ── Audio-driven forces (aggressive for visible results) ──
        int cx = gridW / 2;
        int cy = gridH / 2;

        // Bass: big pressure explosion from center
        if (bass > 0.02f)
        {
            int radius = static_cast<int>(4 + bass * 10);
            float strength = bass * 80.0f;
            for (int dy = -radius; dy <= radius; ++dy)
            {
                for (int dx = -radius; dx <= radius; ++dx)
                {
                    int px = cx + dx;
                    int py = cy + dy;
                    if (px >= 1 && px < gridW - 1 && py >= 1 && py < gridH - 1)
                    {
                        float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy));
                        if (dist < radius)
                        {
                            float falloff = 1.0f - dist / radius;
                            falloff *= falloff;
                            float angle = std::atan2(static_cast<float>(dy), static_cast<float>(dx));
                            vxPrev[IX(px, py)] += std::cos(angle) * strength * falloff;
                            vyPrev[IX(px, py)] += std::sin(angle) * strength * falloff;
                            densityPrev[IX(px, py)] += falloff * 8.0f;
                        }
                    }
                }
            }
        }

        // Mids: two orbiting vortices with large radius
        if (mid > 0.01f)
        {
            for (int v = 0; v < 2; ++v)
            {
                float vAngle = time * 1.5f + v * 3.14159f;
                int vortexX = cx + static_cast<int>(std::cos(vAngle) * gridW * 0.3f);
                int vortexY = cy + static_cast<int>(std::sin(vAngle) * gridH * 0.3f);
                vortexX = juce::jlimit(3, gridW - 4, vortexX);
                vortexY = juce::jlimit(3, gridH - 4, vortexY);
                float vStr = mid * 40.0f;
                int vRad = static_cast<int>(3 + mid * 8);
                for (int dy = -vRad; dy <= vRad; ++dy)
                {
                    for (int dx = -vRad; dx <= vRad; ++dx)
                    {
                        int px = vortexX + dx;
                        int py = vortexY + dy;
                        if (px >= 1 && px < gridW - 1 && py >= 1 && py < gridH - 1)
                        {
                            float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy));
                            if (dist < vRad && dist > 0.5f)
                            {
                                float falloff = 1.0f - dist / vRad;
                                float dir = (v == 0) ? 1.0f : -1.0f;
                                vxPrev[IX(px, py)] += dir * (-dy / dist) * vStr * falloff;
                                vyPrev[IX(px, py)] += dir * (dx / dist) * vStr * falloff;
                                densityPrev[IX(px, py)] += falloff * 3.0f;
                            }
                        }
                    }
                }
            }
        }

        // Highs: spray dye everywhere
        if (high > 0.02f)
        {
            juce::Random rng(static_cast<int>(time * 1000));
            int numDrops = static_cast<int>(5 + high * 30);
            for (int d = 0; d < numDrops; ++d)
            {
                int rx = 2 + rng.nextInt(gridW - 4);
                int ry = 2 + rng.nextInt(gridH - 4);
                float dyeAmount = 5.0f + high * 10.0f;
                for (int sy = -1; sy <= 1; ++sy)
                    for (int sx = -1; sx <= 1; ++sx)
                        if (rx+sx >= 1 && rx+sx < gridW-1 && ry+sy >= 1 && ry+sy < gridH-1)
                            densityPrev[IX(rx+sx, ry+sy)] += dyeAmount * 0.5f;
                // Also add some velocity to push the dye around
                float pushAngle = rng.nextFloat() * 6.28f;
                vxPrev[IX(rx, ry)] += std::cos(pushAngle) * high * 20.0f;
                vyPrev[IX(rx, ry)] += std::sin(pushAngle) * high * 20.0f;
            }
        }

        // Beat: massive explosion
        if (beat)
        {
            int splashR = static_cast<int>(8 + energy * 12);
            for (int dy = -splashR; dy <= splashR; ++dy)
            {
                for (int dx = -splashR; dx <= splashR; ++dx)
                {
                    int px = cx + dx;
                    int py = cy + dy;
                    if (px >= 1 && px < gridW - 1 && py >= 1 && py < gridH - 1)
                    {
                        float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy));
                        if (dist < splashR)
                        {
                            float falloff = 1.0f - dist / splashR;
                            float angle = std::atan2(static_cast<float>(dy), static_cast<float>(dx));
                            vxPrev[IX(px, py)] += std::cos(angle) * 100.0f * falloff;
                            vyPrev[IX(px, py)] += std::sin(angle) * 100.0f * falloff;
                            densityPrev[IX(px, py)] += falloff * 15.0f;
                        }
                    }
                }
            }
        }

        // Ambient motion — multiple injection points with strong flow
        {
            for (int s = 0; s < 3; ++s)
            {
                float ambientAngle = time * (0.4f + s * 0.3f) + s * 2.094f;
                int ax = cx + static_cast<int>(std::cos(ambientAngle) * gridW * 0.3f);
                int ay = cy + static_cast<int>(std::sin(ambientAngle * 0.7f + s) * gridH * 0.3f);
                ax = juce::jlimit(3, gridW - 4, ax);
                ay = juce::jlimit(3, gridH - 4, ay);
                float dyeStr = 4.0f;
                float velStr = 12.0f;
                for (int dy = -2; dy <= 2; ++dy)
                    for (int dx = -2; dx <= 2; ++dx)
                        if (ax+dx >= 1 && ax+dx < gridW-1 && ay+dy >= 1 && ay+dy < gridH-1)
                            densityPrev[IX(ax+dx, ay+dy)] += dyeStr * 0.3f;
                vxPrev[IX(ax, ay)] += std::cos(ambientAngle * 1.3f) * velStr;
                vyPrev[IX(ax, ay)] += std::sin(ambientAngle * 1.1f) * velStr;
            }
        }

        // ── Run Navier-Stokes solver ──
        float dt = 1.0f / 60.0f;
        velocityStep(dt);
        densityStep(dt);
    }

    void paint(juce::Graphics& g) override
    {
#if JUCE_IOS
        if (tryMetalRender()) return;
#endif
        paintCPU(g);
    }

#if JUCE_IOS
    bool tryMetalRender()
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

        // FluidSimGPUUniforms is ~192KB — heap allocate to avoid stack overflow
        if (!gpuUniforms) gpuUniforms = std::make_unique<FluidSimGPUUniforms>();
        auto& u = *gpuUniforms;
        std::memset(&u, 0, sizeof(FluidSimGPUUniforms));
        int count = juce::jmin(gridW * gridH, 128 * 128);
        for (int i = 0; i < count; ++i)
        {
            u.density[i] = density[i];
            u.vx[i] = vx[i];
            u.vy[i] = vy[i];
        }
        u.gridW = gridW;
        u.gridH = gridH;
        u.colorMode = colorMode;
        float eng = 0.0f;
        for (int b = 0; b < numBands; ++b) eng += bands[b];
        u.energy = eng / numBands;
        u.time = time;

        metalRenderer->renderFluidSim(u);
        return true;
    }
#endif

    void paintCPU(juce::Graphics& g)
    {
        auto bounds = getLocalBounds();
        int w = bounds.getWidth();
        int h = bounds.getHeight();
        if (w < 4 || h < 4 || gridW < 4 || gridH < 4) return;

        // ── Render density field to image (full resolution for quality) ──
        int rw = juce::jmax(1, w);
        int rh = juce::jmax(1, h);

        if (renderImage.getWidth() != rw || renderImage.getHeight() != rh)
            renderImage = juce::Image(juce::Image::ARGB, rw, rh, false);

        juce::Image::BitmapData bmp(renderImage, juce::Image::BitmapData::writeOnly);

        float hueBase = std::fmod(time * 0.05f, 1.0f);
        float energy = 0.0f;
        for (int b = 0; b < numBands; ++b) energy += bands[b];
        energy /= numBands;

        for (int y = 0; y < rh; ++y)
        {
            float gy = static_cast<float>(y) / rh * gridH;
            int gy0 = juce::jlimit(0, gridH - 1, static_cast<int>(gy));

            for (int x = 0; x < rw; ++x)
            {
                float gx = static_cast<float>(x) / rw * gridW;
                int gx0 = juce::jlimit(0, gridW - 1, static_cast<int>(gx));
                size_t idx = static_cast<size_t>(gy0 * gridW + gx0);

                float d = juce::jlimit(0.0f, 1.0f, density[idx]);

                // Also incorporate velocity magnitude for visual interest
                float velMag = std::sqrt(vx[idx] * vx[idx] + vy[idx] * vy[idx]);
                float velNorm = juce::jlimit(0.0f, 1.0f, velMag * 0.1f);

                float r, gr, b;
                densityToColor(d, velNorm, hueBase, energy, r, gr, b);

                bmp.setPixelColour(x, y, juce::Colour::fromFloatRGBA(r, gr, b, 1.0f));
            }
        }

        g.drawImage(renderImage, bounds.toFloat(),
            juce::RectanglePlacement::stretchToFit);
    }

private:
    // ── FFT state (same as ShaderToyComponent) ──
    juce::dsp::FFT fft { fftOrder };
    float fifo[fftSize] = {};
    float fftData[fftSize * 2] = {};
    int fifoIndex = 0;
    std::atomic<bool> fftReady { false };

    float bands[numBands] = {};
    float time = 0.0f;

    // ── Beat detection state (same as GeissComponent) ──
    std::atomic<float> smoothedRms { 0.0f };
    std::atomic<float> avgRms { 0.0f };
    std::atomic<bool> beatHit { false };

    // ── Controls ──
    int colorMode = 0;
    float viscosity = 0.005f;
    bool vorticityEnabled = true;

    // ── Fluid simulation grids (quarter resolution) ──
    int gridW = 0, gridH = 0;
    std::vector<float> vx, vy;           // velocity field
    std::vector<float> vxPrev, vyPrev;    // previous velocity (workspace)
    std::vector<float> density;           // dye/density field
    std::vector<float> densityPrev;       // previous density (workspace)

    juce::Image renderImage;

    // ── Color palette mapping ──
    void densityToColor(float d, float velNorm, float hueBase, float energy,
                        float& r, float& g, float& b) const
    {
        switch (colorMode)
        {
            case 0: // Warm: black → red → orange → yellow → white
            {
                float t = d + velNorm * 0.3f;
                t = juce::jlimit(0.0f, 1.0f, t);
                r = juce::jlimit(0.0f, 1.0f, t * 3.0f);
                g = juce::jlimit(0.0f, 1.0f, (t - 0.33f) * 3.0f);
                b = juce::jlimit(0.0f, 1.0f, (t - 0.66f) * 3.0f);
                break;
            }
            case 1: // Cool: black → blue → cyan → white
            {
                float t = d + velNorm * 0.3f;
                t = juce::jlimit(0.0f, 1.0f, t);
                r = juce::jlimit(0.0f, 1.0f, (t - 0.5f) * 2.0f);
                g = juce::jlimit(0.0f, 1.0f, (t - 0.25f) * 2.0f);
                b = juce::jlimit(0.0f, 1.0f, t * 2.0f);
                break;
            }
            case 2: // Neon: high-contrast cycling hues
            {
                float hue = std::fmod(hueBase + d * 0.5f + velNorm * 0.3f, 1.0f);
                float sat = 0.8f + energy * 0.2f;
                float val = juce::jlimit(0.0f, 1.0f, d * 2.0f + velNorm * 0.5f);
                hsvToRgb(hue, sat, val, r, g, b);
                break;
            }
            case 3: // Smoke: grayscale with subtle blue tint
            {
                float t = juce::jlimit(0.0f, 1.0f, d * 1.5f + velNorm * 0.2f);
                r = t * 0.85f;
                g = t * 0.88f;
                b = t * 1.0f;
                break;
            }
            case 4: // Psychedelic: energy-driven hue cycling
            {
                float hue = std::fmod(hueBase + d * 1.5f + velNorm * 0.5f + energy * 0.3f, 1.0f);
                float sat = 1.0f;
                float val = juce::jlimit(0.0f, 1.0f, d * 3.0f);
                hsvToRgb(hue, sat, val, r, g, b);
                break;
            }
            default:
            {
                r = g = b = juce::jlimit(0.0f, 1.0f, d);
                break;
            }
        }
    }

    static void hsvToRgb(float h, float s, float v, float& r, float& g, float& b)
    {
        int i = static_cast<int>(h * 6.0f);
        float f = h * 6.0f - static_cast<float>(i);
        float p = v * (1.0f - s);
        float q = v * (1.0f - f * s);
        float t = v * (1.0f - (1.0f - f) * s);
        switch (i % 6)
        {
            case 0: r = v; g = t; b = p; break;
            case 1: r = q; g = v; b = p; break;
            case 2: r = p; g = v; b = t; break;
            case 3: r = p; g = q; b = v; break;
            case 4: r = t; g = p; b = v; break;
            case 5: r = v; g = p; b = q; break;
            default: r = g = b = 0.0f; break;
        }
    }

    // ── Fluid simulation helpers ──

    // Index helper with boundary clamping
    size_t IX(int x, int y) const
    {
        x = juce::jlimit(0, gridW - 1, x);
        y = juce::jlimit(0, gridH - 1, y);
        return static_cast<size_t>(y * gridW + x);
    }

    // Set boundary conditions (simple: zero at edges)
    void setBoundary(int bound, std::vector<float>& field)
    {
        for (int i = 1; i < gridH - 1; ++i)
        {
            field[IX(0, i)]          = (bound == 1) ? -field[IX(1, i)] : field[IX(1, i)];
            field[IX(gridW - 1, i)]  = (bound == 1) ? -field[IX(gridW - 2, i)] : field[IX(gridW - 2, i)];
        }
        for (int i = 1; i < gridW - 1; ++i)
        {
            field[IX(i, 0)]          = (bound == 2) ? -field[IX(i, 1)] : field[IX(i, 1)];
            field[IX(i, gridH - 1)]  = (bound == 2) ? -field[IX(i, gridH - 2)] : field[IX(i, gridH - 2)];
        }
        // Corners
        field[IX(0, 0)]                     = 0.5f * (field[IX(1, 0)] + field[IX(0, 1)]);
        field[IX(gridW - 1, 0)]             = 0.5f * (field[IX(gridW - 2, 0)] + field[IX(gridW - 1, 1)]);
        field[IX(0, gridH - 1)]             = 0.5f * (field[IX(1, gridH - 1)] + field[IX(0, gridH - 2)]);
        field[IX(gridW - 1, gridH - 1)]     = 0.5f * (field[IX(gridW - 2, gridH - 1)] + field[IX(gridW - 1, gridH - 2)]);
    }

    // Gauss-Seidel relaxation (few iterations for CPU performance)
    void linearSolve(int bound, std::vector<float>& x, const std::vector<float>& x0,
                     float a, float c)
    {
        float cRecip = 1.0f / c;
        for (int iter = 0; iter < 4; ++iter)
        {
            for (int j = 1; j < gridH - 1; ++j)
            {
                for (int i = 1; i < gridW - 1; ++i)
                {
                    x[IX(i, j)] = (x0[IX(i, j)]
                        + a * (x[IX(i + 1, j)] + x[IX(i - 1, j)]
                             + x[IX(i, j + 1)] + x[IX(i, j - 1)])) * cRecip;
                }
            }
            setBoundary(bound, x);
        }
    }

    // Diffusion step
    void diffuse(int bound, std::vector<float>& x, std::vector<float>& x0, float diff, float dt)
    {
        float a = dt * diff * static_cast<float>((gridW - 2) * (gridH - 2));
        linearSolve(bound, x, x0, a, 1.0f + 4.0f * a);
    }

    // Advection step
    void advect(int bound, std::vector<float>& d, const std::vector<float>& d0,
                const std::vector<float>& velX, const std::vector<float>& velY, float dt)
    {
        float dtx = dt * static_cast<float>(gridW - 2);
        float dty = dt * static_cast<float>(gridH - 2);

        for (int j = 1; j < gridH - 1; ++j)
        {
            for (int i = 1; i < gridW - 1; ++i)
            {
                float x = static_cast<float>(i) - dtx * velX[IX(i, j)];
                float y = static_cast<float>(j) - dty * velY[IX(i, j)];

                x = juce::jlimit(0.5f, static_cast<float>(gridW - 2) + 0.5f, x);
                y = juce::jlimit(0.5f, static_cast<float>(gridH - 2) + 0.5f, y);

                int i0 = static_cast<int>(x);
                int j0 = static_cast<int>(y);
                int i1 = i0 + 1;
                int j1 = j0 + 1;

                float s1 = x - static_cast<float>(i0);
                float s0 = 1.0f - s1;
                float t1 = y - static_cast<float>(j0);
                float t0 = 1.0f - t1;

                d[IX(i, j)] = s0 * (t0 * d0[IX(i0, j0)] + t1 * d0[IX(i0, j1)])
                             + s1 * (t0 * d0[IX(i1, j0)] + t1 * d0[IX(i1, j1)]);
            }
        }
        setBoundary(bound, d);
    }

    // Projection step — enforce incompressibility
    void project(std::vector<float>& velX, std::vector<float>& velY,
                 std::vector<float>& p, std::vector<float>& div)
    {
        float hx = 1.0f / static_cast<float>(gridW - 2);
        float hy = 1.0f / static_cast<float>(gridH - 2);

        for (int j = 1; j < gridH - 1; ++j)
        {
            for (int i = 1; i < gridW - 1; ++i)
            {
                div[IX(i, j)] = -0.5f * (hx * (velX[IX(i + 1, j)] - velX[IX(i - 1, j)])
                                        + hy * (velY[IX(i, j + 1)] - velY[IX(i, j - 1)]));
                p[IX(i, j)] = 0.0f;
            }
        }
        setBoundary(0, div);
        setBoundary(0, p);

        linearSolve(0, p, div, 1.0f, 4.0f);

        for (int j = 1; j < gridH - 1; ++j)
        {
            for (int i = 1; i < gridW - 1; ++i)
            {
                velX[IX(i, j)] -= 0.5f * (p[IX(i + 1, j)] - p[IX(i - 1, j)]) * static_cast<float>(gridW - 2);
                velY[IX(i, j)] -= 0.5f * (p[IX(i, j + 1)] - p[IX(i, j - 1)]) * static_cast<float>(gridH - 2);
            }
        }
        setBoundary(1, velX);
        setBoundary(2, velY);
    }

    // Vorticity confinement — amplifies rotational features
    void applyVorticity()
    {
        if (!vorticityEnabled) return;

        // Compute curl (vorticity)
        std::vector<float> curl(static_cast<size_t>(gridW * gridH), 0.0f);
        for (int j = 1; j < gridH - 1; ++j)
        {
            for (int i = 1; i < gridW - 1; ++i)
            {
                curl[IX(i, j)] = (vy[IX(i + 1, j)] - vy[IX(i - 1, j)])
                                - (vx[IX(i, j + 1)] - vx[IX(i, j - 1)]);
            }
        }

        // Apply confinement force
        float vorticityStrength = 0.1f;
        for (int j = 2; j < gridH - 2; ++j)
        {
            for (int i = 2; i < gridW - 2; ++i)
            {
                float dcdx = std::abs(curl[IX(i + 1, j)]) - std::abs(curl[IX(i - 1, j)]);
                float dcdy = std::abs(curl[IX(i, j + 1)]) - std::abs(curl[IX(i, j - 1)]);
                float len = std::sqrt(dcdx * dcdx + dcdy * dcdy) + 1e-5f;

                float nx = dcdx / len;
                float ny = dcdy / len;
                float c = curl[IX(i, j)];

                vx[IX(i, j)] += vorticityStrength * ny * c;
                vy[IX(i, j)] -= vorticityStrength * nx * c;
            }
        }
    }

    // Full velocity step: diffuse → project → advect → project → vorticity
    void velocityStep(float dt)
    {
        std::swap(vx, vxPrev);
        std::swap(vy, vyPrev);
        diffuse(1, vx, vxPrev, viscosity, dt);
        diffuse(2, vy, vyPrev, viscosity, dt);
        project(vx, vy, vxPrev, vyPrev);

        std::swap(vx, vxPrev);
        std::swap(vy, vyPrev);
        advect(1, vx, vxPrev, vxPrev, vyPrev, dt);
        advect(2, vy, vyPrev, vxPrev, vyPrev, dt);
        project(vx, vy, vxPrev, vyPrev);

        applyVorticity();
    }

    // Full density step: diffuse → advect → decay
    void densityStep(float dt)
    {
        std::swap(density, densityPrev);
        diffuse(0, density, densityPrev, viscosity * 0.5f, dt);

        std::swap(density, densityPrev);
        advect(0, density, densityPrev, vx, vy, dt);

        // Slow decay so trails fade
        for (size_t i = 0; i < density.size(); ++i)
            density[i] *= 0.995f;
    }

#if JUCE_IOS
    std::unique_ptr<MetalVisualizerRenderer> metalRenderer;
    std::unique_ptr<FluidSimGPUUniforms> gpuUniforms;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FluidSimComponent)
};
