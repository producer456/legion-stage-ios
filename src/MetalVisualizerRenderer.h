#pragma once
#include <cstdint>

/**
 * MetalVisualizerRenderer — GPU-accelerated visualizer rendering.
 *
 * Manages Metal pipelines for each visualizer type:
 * - ShaderToy: fullscreen procedural fragment shader
 * - Geiss/ProjectM: warp+blur compute shader + palette lookup
 * - Spectrum: GPU bar rendering
 * - WaveTerrain: GPU stacked waveform rendering
 * - Lissajous: GPU dot field rendering
 * - Analyzer: GPU spectrum curve rendering
 *
 * Each visualizer component calls the appropriate render method
 * and the output goes directly to a CAMetalLayer.
 */

// ── ShaderToy Uniforms ──
struct ShaderToyUniforms {
    float time;
    float bass;
    float mid;
    float high;
    float energy;
    float bands[16];
    int   preset;
    int   numBands;
};

// ── Spectrum Uniforms ──
struct SpectrumGPUUniforms {
    float barLevels[64];
    int   numBars;
    float barColorR, barColorG, barColorB;
    float bgColorR, bgColorG, bgColorB;
};

// ── WaveTerrain Uniforms ──
struct WaveTerrainGPUUniforms {
    float lines[48 * 128];
    int   numLines;
    int   lineResolution;
    float lineColorR, lineColorG, lineColorB;
    float bgColorR, bgColorG, bgColorB;
};

// ── Lissajous Uniforms ──
struct LissajousGPUUniforms {
    float dotsX[1024];
    float dotsY[1024];
    float dotsAlpha[1024];
    int   numDots;
    float dotColorR, dotColorG, dotColorB;
    float bgColorR, bgColorG, bgColorB;
    float dotRadius;
};

// ── Analyzer Uniforms ──
struct AnalyzerGPUUniforms {
    float levels[256];
    float peakLevels[256];
    int   numPoints;
    int   peakHold;
    float accentR, accentG, accentB;
    float bgR, bgG, bgB;
    float gridR, gridG, gridB;
    float dbMin, dbMax;
};

// ── Fluid Sim Uniforms ──
struct FluidSimGPUUniforms {
    float density[128 * 128];
    float vx[128 * 128];
    float vy[128 * 128];
    int   gridW;
    int   gridH;
    int   colorMode;
    float energy;
    float time;
};

// ── Ray March Uniforms ──
struct RayMarchGPUUniforms {
    float time;
    float bass;
    float mid;
    float high;
    float energy;
    float beatIntensity;
    int   preset;
    float camPosX, camPosY, camPosZ;
    float camRotX, camRotY;
};

// ── Palette Uniforms ──
struct PaletteGPUUniforms {
    int   paletteRotation;
    int   energyOffset;
    float brightMult;
};

// ── Effect Point (for Geiss/ProjectM compute shader) ──
struct EffectPoint {
    float x;
    float y;
    float radius;
    float brightness;
};

class MetalVisualizerRenderer
{
public:
    MetalVisualizerRenderer();
    ~MetalVisualizerRenderer();

    /** Attach to a native UIView. */
    void attachToView(void* nativeViewHandle);
    void detach();
    bool isAttached() const;
    bool isAvailable() const;

    /** Update layer bounds. */
    void setBounds(int x, int y, int width, int height);

    // ── Render methods ──

    /** ShaderToy: fullscreen procedural shader */
    void renderShaderToy(const ShaderToyUniforms& uniforms);

    /** Spectrum: frequency bars */
    void renderSpectrum(const SpectrumGPUUniforms& uniforms);

    /** WaveTerrain: stacked waveform lines */
    void renderWaveTerrain(const WaveTerrainGPUUniforms& uniforms);

    /** Lissajous: stereo field dots */
    void renderLissajous(const LissajousGPUUniforms& uniforms);

    /** Analyzer: smooth spectrum curve */
    void renderAnalyzer(const AnalyzerGPUUniforms& uniforms);

    /** Fluid Simulation: density field visualization */
    void renderFluidSim(const FluidSimGPUUniforms& uniforms);

    /** Ray March: 3D SDF rendering */
    void renderRayMarch(const RayMarchGPUUniforms& uniforms);

    /**
     * Geiss/ProjectM warp pipeline:
     * 1. Upload warp map (displacement texture)
     * 2. Execute warp+blur compute shader
     * 3. Upload effect points and execute effects compute
     * 4. Render with palette lookup fragment shader
     */
    void initWarpBuffers(int width, int height);
    void uploadWarpMap(const int* mapDx, const int* mapDy, int w, int h);
    void executeWarpBlur();
    void uploadEffectPoints(const EffectPoint* points, int count);
    void executeEffects();
    void swapWarpBuffers();
    void renderPalette(const uint32_t* palette256, const PaletteGPUUniforms& uniforms);

private:
    class Impl;
    Impl* pimpl;
};
