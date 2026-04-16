#include <metal_stdlib>
using namespace metal;

// ─── Shared Vertex Shader ─────────────────────────────────────────

struct VisVertexOut {
    float4 position [[position]];
    float2 uv;
};

vertex VisVertexOut visVertex(uint vid [[vertex_id]]) {
    float2 positions[4] = { {-1, -1}, {1, -1}, {-1, 1}, {1, 1} };
    float2 uvs[4]       = { {0, 1},   {1, 1},  {0, 0},  {1, 0} };
    VisVertexOut out;
    out.position = float4(positions[vid], 0.0, 1.0);
    out.uv = uvs[vid];
    return out;
}

// ═══════════════════════════════════════════════════════════════════
// SHADERTOY — 4 audio-reactive procedural presets
// ═══════════════════════════════════════════════════════════════════

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

fragment float4 shaderToyFragment(VisVertexOut in [[stage_in]],
                                   constant ShaderToyUniforms& u [[buffer(0)]]) {
    float2 uv = in.uv;
    float t = u.time;
    float r, g, b;

    switch (u.preset) {
        case 0: // Plasma waves
        {
            float cx = uv.x - 0.5, cy = uv.y - 0.5;
            float d = sqrt(cx * cx + cy * cy);
            float a = atan2(cy, cx);
            float p1 = sin(d * 12.0 - t * 2.0 + u.bass * 8.0);
            float p2 = sin(a * 4.0 + t * 1.5 + u.mid * 6.0);
            float p3 = sin((uv.x + uv.y) * 8.0 + t + u.high * 4.0);
            r = clamp(0.5 + 0.3 * p1 + 0.2 * u.energy, 0.0, 1.0);
            g = clamp(0.3 + 0.3 * p2 + 0.15 * u.bass, 0.0, 1.0);
            b = clamp(0.5 + 0.3 * p3 + 0.2 * u.high, 0.0, 1.0);
            break;
        }
        case 1: // Radial pulse
        {
            float cx = uv.x - 0.5, cy = uv.y - 0.5;
            float d = sqrt(cx * cx + cy * cy);
            float a = atan2(cy, cx);
            int bandIdx = int((a + M_PI_F) / (2.0 * M_PI_F) * float(u.numBands)) % u.numBands;
            float bandVal = u.bands[bandIdx];
            float ring = sin(d * 20.0 - t * 3.0) * 0.5 + 0.5;
            float pulse = (bandVal > d * 1.5) ? 1.0 : 0.1;
            r = clamp(ring * pulse * 0.8 + u.bass * 0.3, 0.0, 1.0);
            g = clamp(pulse * 0.6 * (1.0 - d), 0.0, 1.0);
            b = clamp(ring * 0.7 + u.high * 0.4, 0.0, 1.0);
            break;
        }
        case 2: // Electric field
        {
            float cx = uv.x - 0.5, cy = uv.y - 0.5;
            float f1x = 0.25 * sin(t * 0.7), f1y = 0.25 * cos(t * 0.9);
            float f2x = -0.25 * cos(t * 0.6), f2y = 0.25 * sin(t * 0.8);
            float d1 = sqrt((cx - f1x) * (cx - f1x) + (cy - f1y) * (cy - f1y));
            float d2 = sqrt((cx - f2x) * (cx - f2x) + (cy - f2y) * (cy - f2y));
            float field = (u.bass + 0.3) / (d1 + 0.05) + (u.mid + 0.3) / (d2 + 0.05);
            field = fmod(field * 0.15, 1.0);
            r = clamp(field * 1.2, 0.0, 1.0);
            g = clamp(field * 0.6 + u.energy * 0.3, 0.0, 1.0);
            b = clamp((1.0 - field) * 0.8 + u.high * 0.5, 0.0, 1.0);
            break;
        }
        default: // Noise ocean
        {
            float wave = sin(uv.x * 10.0 + t + u.bass * 5.0)
                       * cos(uv.y * 8.0 + t * 0.7 + u.mid * 4.0) * 0.5 + 0.5;
            float foam = sin(uv.x * 30.0 + uv.y * 20.0 + t * 2.0) * 0.5 + 0.5;
            foam *= u.high * 2.0;
            r = clamp(wave * 0.2 + foam * 0.5, 0.0, 1.0);
            g = clamp(wave * 0.5 + u.energy * 0.3, 0.0, 1.0);
            b = clamp(wave * 0.8 + 0.2, 0.0, 1.0);
            break;
        }
    }

    return float4(r, g, b, 1.0);
}

// ═══════════════════════════════════════════════════════════════════
// GEISS / PROJECTM — Warp + Blur Compute Shader
// ═══════════════════════════════════════════════════════════════════

// Compute shader: apply warp map displacement + 3x3 weighted blur
// Reads from source texture, writes to dest texture
// Warp map stored as RG16Float texture (dx, dy displacements)

kernel void warpBlurCompute(texture2d<float, access::read>  src   [[texture(0)]],
                            texture2d<float, access::write> dst   [[texture(1)]],
                            texture2d<float, access::read>  warpMap [[texture(2)]],
                            uint2 gid [[thread_position_in_grid]]) {
    int w = src.get_width();
    int h = src.get_height();
    if (int(gid.x) >= w || int(gid.y) >= h) return;

    // Read displacement from warp map (stored as float2 in RG channels)
    float4 warp = warpMap.read(gid);
    int dx = int(warp.r * 65535.0 - 32768.0);  // decode from [0,1] → [-32768,32767]
    int dy = int(warp.g * 65535.0 - 32768.0);

    int sx = clamp(int(gid.x) + dx, 0, w - 1);
    int sy = clamp(int(gid.y) + dy, 0, h - 1);

    // Edge pixels: simple warp, no blur
    if (sx <= 0 || sx >= w - 1 || sy <= 0 || sy >= h - 1) {
        float val = src.read(uint2(clamp(sx, 0, w-1), clamp(sy, 0, h-1))).r;
        val = max(0.0, val - 1.0/255.0);
        dst.write(float4(val, 0, 0, 1), gid);
        return;
    }

    // 3x3 weighted average: corners=1, edges=2, center=4, total=16
    float sum = src.read(uint2(sx-1, sy-1)).r * 1.0
              + src.read(uint2(sx,   sy-1)).r * 2.0
              + src.read(uint2(sx+1, sy-1)).r * 1.0
              + src.read(uint2(sx-1, sy  )).r * 2.0
              + src.read(uint2(sx,   sy  )).r * 4.0
              + src.read(uint2(sx+1, sy  )).r * 2.0
              + src.read(uint2(sx-1, sy+1)).r * 1.0
              + src.read(uint2(sx,   sy+1)).r * 2.0
              + src.read(uint2(sx+1, sy+1)).r * 1.0;

    float result = max(0.0, sum / 16.0 - 1.0/255.0);
    dst.write(float4(result, 0, 0, 1), gid);
}

// Compute shader: render additive bright dot at a position
// Used for shade bobs, particles, waveform points, etc.
// Uses an effects buffer (list of effect commands) passed as a buffer

struct EffectPoint {
    float x;
    float y;
    float radius;    // 0 = single pixel, >0 = radial falloff blob
    float brightness; // 0-255 range, additive
};

kernel void effectsCompute(texture2d<float, access::read_write> buf [[texture(0)]],
                           constant EffectPoint* points [[buffer(0)]],
                           constant int& numPoints [[buffer(1)]],
                           uint2 gid [[thread_position_in_grid]]) {
    int w = buf.get_width();
    int h = buf.get_height();
    if (int(gid.x) >= w || int(gid.y) >= h) return;

    float px = float(gid.x);
    float py = float(gid.y);
    float existing = buf.read(gid).r;
    float addVal = 0.0;

    for (int i = 0; i < numPoints; ++i) {
        EffectPoint pt = points[i];
        if (pt.radius <= 0.0) {
            // Single pixel
            if (int(pt.x) == int(gid.x) && int(pt.y) == int(gid.y))
                addVal += pt.brightness / 255.0;
        } else {
            float dx = px - pt.x;
            float dy = py - pt.y;
            float dist = sqrt(dx * dx + dy * dy);
            if (dist <= pt.radius) {
                float intensity = (1.0 - dist / pt.radius);
                intensity *= intensity; // quadratic falloff
                addVal += intensity * pt.brightness / 255.0;
            }
        }
    }

    float result = min(1.0, existing + addVal);
    buf.write(float4(result, 0, 0, 1), gid);
}

// Fragment shader: palette lookup for indexed-color visualizers (Geiss/ProjectM)
// Reads the R channel as a 0-255 index, looks up in palette texture

struct PaletteUniforms {
    int   paletteRotation;  // 0-255 rotation offset
    int   energyOffset;     // energy-based shift
    float brightMult;       // 0-1 blackout fade multiplier
};

fragment float4 paletteLookupFragment(VisVertexOut in [[stage_in]],
                                       constant PaletteUniforms& u [[buffer(0)]],
                                       texture2d<float, access::read> indexTex [[texture(0)]],
                                       texture1d<float> paletteTex [[texture(1)]]) {
    int w = indexTex.get_width();
    int h = indexTex.get_height();
    int px = int(in.uv.x * float(w));
    int py = int(in.uv.y * float(h));
    px = clamp(px, 0, w - 1);
    py = clamp(py, 0, h - 1);

    float indexVal = indexTex.read(uint2(px, py)).r;
    int idx = clamp(int(indexVal * 255.0), 0, 255);
    idx = (idx + u.paletteRotation + u.energyOffset) % 256;

    constexpr sampler s(mag_filter::nearest, min_filter::nearest);
    float4 col = paletteTex.sample(s, (float(idx) + 0.5) / 256.0);

    col.rgb *= u.brightMult;
    col.a = 1.0;
    return col;
}

// ═══════════════════════════════════════════════════════════════════
// SPECTRUM — GPU-rendered frequency bars
// ═══════════════════════════════════════════════════════════════════

struct SpectrumUniforms {
    float barLevels[64];  // up to 64 bars
    int   numBars;
    float barColorR, barColorG, barColorB;
    float bgColorR, bgColorG, bgColorB;
};

fragment float4 spectrumFragment(VisVertexOut in [[stage_in]],
                                  constant SpectrumUniforms& u [[buffer(0)]]) {
    float x = in.uv.x;
    float y = 1.0 - in.uv.y;  // flip Y: 0 at bottom

    int barIdx = clamp(int(x * float(u.numBars)), 0, u.numBars - 1);
    float level = u.barLevels[barIdx];

    // Bar boundaries with 1px gap
    float barWidth = 1.0 / float(u.numBars);
    float barLocal = fmod(x, barWidth) / barWidth;
    bool inGap = barLocal < 0.02 || barLocal > 0.98;

    if (y <= level && !inGap) {
        float brightness = 0.4 + level * 0.6;
        return float4(u.barColorR * brightness, u.barColorG * brightness,
                      u.barColorB * brightness, 1.0);
    }
    return float4(u.bgColorR, u.bgColorG, u.bgColorB, 1.0);
}

// ═══════════════════════════════════════════════════════════════════
// WAVE TERRAIN — stacked waveform lines (Unknown Pleasures style)
// ═══════════════════════════════════════════════════════════════════

struct WaveTerrainUniforms {
    float lines[48 * 128];   // numLines * lineResolution
    int   numLines;           // 48
    int   lineResolution;     // 128
    float lineColorR, lineColorG, lineColorB;
    float bgColorR, bgColorG, bgColorB;
};

fragment float4 waveTerrainFragment(VisVertexOut in [[stage_in]],
                                     constant WaveTerrainUniforms& u [[buffer(0)]]) {
    float px = in.uv.x;
    float py = in.uv.y;

    float lineSpacing = 1.0 / float(u.numLines + 2);

    // Check each line from front to back — front lines occlude back ones
    for (int l = 0; l < u.numLines; ++l) {
        float baseY = (float(l) + 1.5) * lineSpacing;
        float alpha = 1.0 - float(l) / float(u.numLines) * 0.7;
        float amplitude = lineSpacing * 2.5 * (1.0 - float(l) / float(u.numLines) * 0.5);

        // Sample waveform at this x position
        int sampleIdx = clamp(int(px * float(u.lineResolution - 1)), 0, u.lineResolution - 1);
        int nextIdx = min(sampleIdx + 1, u.lineResolution - 1);
        float frac = px * float(u.lineResolution - 1) - float(sampleIdx);

        float sample = mix(u.lines[l * u.lineResolution + sampleIdx],
                           u.lines[l * u.lineResolution + nextIdx], frac);

        float waveY = baseY - sample * amplitude;

        // If pixel is below the wave line for this layer → it's occluded (bg color)
        if (py >= waveY && py < baseY + lineSpacing * 0.5) {
            return float4(u.bgColorR, u.bgColorG, u.bgColorB, 1.0);
        }

        // Near the wave line → draw it
        float dist = abs(py - waveY);
        if (dist < 0.003) {
            return float4(u.lineColorR * alpha, u.lineColorG * alpha,
                          u.lineColorB * alpha, 1.0);
        }
    }

    return float4(u.bgColorR, u.bgColorG, u.bgColorB, 1.0);
}

// ═══════════════════════════════════════════════════════════════════
// LISSAJOUS — stereo field XY scope with dot trails
// ═══════════════════════════════════════════════════════════════════

struct LissajousUniforms {
    float dotsX[1024];     // pre-computed dot positions (normalized 0-1)
    float dotsY[1024];
    float dotsAlpha[1024]; // per-dot alpha
    int   numDots;
    float dotColorR, dotColorG, dotColorB;
    float bgColorR, bgColorG, bgColorB;
    float dotRadius;       // in UV space
};

fragment float4 lissajousFragment(VisVertexOut in [[stage_in]],
                                   constant LissajousUniforms& u [[buffer(0)]]) {
    float2 p = in.uv;
    float maxAlpha = 0.0;

    // Check all dots — accumulate closest/brightest
    for (int i = 0; i < u.numDots; ++i) {
        float2 dot = float2(u.dotsX[i], u.dotsY[i]);
        float dist = length(p - dot);
        if (dist < u.dotRadius) {
            float falloff = 1.0 - dist / u.dotRadius;
            maxAlpha = max(maxAlpha, falloff * u.dotsAlpha[i]);
        }
    }

    float3 bg = float3(u.bgColorR, u.bgColorG, u.bgColorB);
    float3 dot = float3(u.dotColorR, u.dotColorG, u.dotColorB);
    float3 col = mix(bg, dot, clamp(maxAlpha, 0.0, 1.0));
    return float4(col, 1.0);
}

// ═══════════════════════════════════════════════════════════════════
// ANALYZER — smooth spectrum curve with filled gradient
// ═══════════════════════════════════════════════════════════════════

struct AnalyzerUniforms {
    float levels[256];     // smoothed dB levels (numPoints)
    float peakLevels[256]; // peak hold levels
    int   numPoints;       // 256
    int   peakHold;        // 0 or 1
    float accentR, accentG, accentB;
    float bgR, bgG, bgB;
    float gridR, gridG, gridB;
    float dbMin, dbMax;    // -90, 6
};

fragment float4 analyzerFragment(VisVertexOut in [[stage_in]],
                                  constant AnalyzerUniforms& u [[buffer(0)]]) {
    float px = in.uv.x;
    float py = in.uv.y;
    float dbRange = u.dbMax - u.dbMin;

    // Sample the spectrum curve at this x position
    float curveIdx = px * float(u.numPoints - 1);
    int idx = clamp(int(curveIdx), 0, u.numPoints - 2);
    float frac = curveIdx - float(idx);
    float level = mix(u.levels[idx], u.levels[idx + 1], frac);
    float curveY = (1.0 - (level - u.dbMin) / dbRange);

    float3 bg = float3(u.bgR, u.bgG, u.bgB);
    float3 accent = float3(u.accentR, u.accentG, u.accentB);

    // Grid lines (dB)
    float gridAlpha = 0.0;
    float dbValues[5] = { -72.0, -54.0, -36.0, -18.0, 0.0 };
    for (int i = 0; i < 5; ++i) {
        float gridY = (1.0 - (dbValues[i] - u.dbMin) / dbRange);
        if (abs(py - gridY) < 0.002) gridAlpha = 0.3;
    }

    // Grid lines (frequency) — log scale markers
    float freqMarkers[8] = { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 };
    for (int i = 0; i < 8; ++i) {
        float t = log(freqMarkers[i] / 20.0) / log(20000.0 / 20.0);
        if (abs(px - t) < 0.002) gridAlpha = 0.3;
    }

    // Filled area under curve
    if (py >= curveY) {
        float fillAlpha = mix(0.35, 0.03, (py - curveY) / (1.0 - curveY + 0.001));
        float3 col = mix(bg, accent, fillAlpha);
        // Curve line itself
        if (abs(py - curveY) < 0.004) col = accent * 0.9;
        return float4(col, 1.0);
    }

    // Peak hold line
    if (u.peakHold) {
        float peakLevel = mix(u.peakLevels[idx], u.peakLevels[idx + 1], frac);
        float peakY = (1.0 - (peakLevel - u.dbMin) / dbRange);
        if (abs(py - peakY) < 0.003) {
            return float4(accent * 0.4, 1.0);
        }
    }

    // Background + grid
    float3 col = mix(bg, float3(u.gridR, u.gridG, u.gridB), gridAlpha);
    return float4(col, 1.0);
}

// ═══════════════════════════════════════════════════════════════════
// FLUID SIMULATION — density field visualization with velocity tinting
// ═══════════════════════════════════════════════════════════════════

struct FluidSimUniforms {
    float density[128 * 128];   // density field (quarter res)
    float vx[128 * 128];        // velocity X
    float vy[128 * 128];        // velocity Y
    int   gridW;                // grid width (128)
    int   gridH;                // grid height (128)
    int   colorMode;            // 0-4 palette
    float energy;               // overall energy
    float time;
};

fragment float4 fluidSimFragment(VisVertexOut in [[stage_in]],
                                  constant FluidSimUniforms& u [[buffer(0)]]) {
    float2 uv = in.uv;

    // Map UV to grid coordinates for bilinear sampling
    float gx = uv.x * float(u.gridW - 1);
    float gy = uv.y * float(u.gridH - 1);

    int x0 = clamp(int(gx), 0, u.gridW - 2);
    int y0 = clamp(int(gy), 0, u.gridH - 2);
    int x1 = x0 + 1;
    int y1 = y0 + 1;

    float fx = gx - float(x0);
    float fy = gy - float(y0);

    // Bilinear interpolation of density
    float d00 = u.density[y0 * u.gridW + x0];
    float d10 = u.density[y0 * u.gridW + x1];
    float d01 = u.density[y1 * u.gridW + x0];
    float d11 = u.density[y1 * u.gridW + x1];

    float d = mix(mix(d00, d10, fx), mix(d01, d11, fx), fy);

    // Bilinear interpolation of velocity
    float velX = mix(mix(u.vx[y0 * u.gridW + x0], u.vx[y0 * u.gridW + x1], fx),
                     mix(u.vx[y1 * u.gridW + x0], u.vx[y1 * u.gridW + x1], fx), fy);
    float velY = mix(mix(u.vy[y0 * u.gridW + x0], u.vy[y0 * u.gridW + x1], fx),
                     mix(u.vy[y1 * u.gridW + x0], u.vy[y1 * u.gridW + x1], fx), fy);

    float speed = sqrt(velX * velX + velY * velY);
    float flowAngle = atan2(velY, velX);

    // Velocity-based hue offset
    float hueShift = flowAngle / (2.0 * M_PI_F) + 0.5;

    // Density-to-color palette mapping
    float3 col;
    float ds = clamp(d, 0.0, 1.0);

    switch (u.colorMode) {
        case 0: // Fire
        {
            col.r = clamp(ds * 3.0, 0.0, 1.0);
            col.g = clamp(ds * 3.0 - 1.0, 0.0, 1.0);
            col.b = clamp(ds * 3.0 - 2.0, 0.0, 1.0);
            break;
        }
        case 1: // Ocean
        {
            col.r = clamp(ds * 0.3, 0.0, 1.0);
            col.g = clamp(ds * 0.8, 0.0, 1.0);
            col.b = clamp(0.2 + ds * 0.8, 0.0, 1.0);
            break;
        }
        case 2: // Neon
        {
            col.r = clamp(sin(ds * M_PI_F + hueShift * 2.0) * 0.5 + 0.5, 0.0, 1.0);
            col.g = clamp(sin(ds * M_PI_F + 2.094 + hueShift * 2.0) * 0.5 + 0.5, 0.0, 1.0);
            col.b = clamp(sin(ds * M_PI_F + 4.189 + hueShift * 2.0) * 0.5 + 0.5, 0.0, 1.0);
            break;
        }
        case 3: // Smoke
        {
            float v = clamp(ds * 1.2, 0.0, 1.0);
            col = float3(v * 0.9, v * 0.92, v);
            break;
        }
        default: // Plasma
        {
            col.r = clamp(0.5 + 0.5 * cos(6.2832 * ds + 0.0 + u.time * 0.5), 0.0, 1.0);
            col.g = clamp(0.5 + 0.5 * cos(6.2832 * ds + 2.094 + u.time * 0.3), 0.0, 1.0);
            col.b = clamp(0.5 + 0.5 * cos(6.2832 * ds + 4.189 + u.time * 0.7), 0.0, 1.0);
            break;
        }
    }

    // Velocity tinting: flow direction shifts color
    float tintAmount = clamp(speed * 2.0, 0.0, 0.4);
    float3 flowTint = float3(0.5 + 0.5 * cos(flowAngle),
                              0.5 + 0.5 * cos(flowAngle + 2.094),
                              0.5 + 0.5 * cos(flowAngle + 4.189));
    col = mix(col, flowTint, tintAmount);

    // Subtle glow around high-density areas
    // Sample neighbors for glow approximation
    float glowRadius = 2.0 / float(u.gridW);
    float glowAccum = 0.0;
    for (int gy2 = -2; gy2 <= 2; ++gy2) {
        for (int gx2 = -2; gx2 <= 2; ++gx2) {
            if (gx2 == 0 && gy2 == 0) continue;
            float su = uv.x + float(gx2) * glowRadius;
            float sv = uv.y + float(gy2) * glowRadius;
            int sx = clamp(int(su * float(u.gridW)), 0, u.gridW - 1);
            int sy = clamp(int(sv * float(u.gridH)), 0, u.gridH - 1);
            glowAccum += u.density[sy * u.gridW + sx];
        }
    }
    glowAccum /= 24.0;
    float glowIntensity = clamp(glowAccum * 0.3 * u.energy, 0.0, 0.25);
    col += float3(glowIntensity);

    // Modulate overall brightness by energy
    col *= 0.7 + 0.3 * u.energy;

    return float4(clamp(col, 0.0, 1.0), 1.0);
}

// ═══════════════════════════════════════════════════════════════════
// RAY MARCH — audio-reactive 3D SDF rendering
// ═══════════════════════════════════════════════════════════════════

struct RayMarchUniforms {
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

// ─── SDF helpers ─────────────────────────────────────────────────

static float sdSphere(float3 p, float r) {
    return length(p) - r;
}

static float smin(float a, float b, float k) {
    float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
    return mix(b, a, h) - k * h * (1.0 - h);
}

static float hash(float2 p) {
    return fract(sin(dot(p, float2(127.1, 311.7))) * 43758.5453123);
}

static float noise(float2 p) {
    float2 i = floor(p);
    float2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash(i);
    float b = hash(i + float2(1.0, 0.0));
    float c = hash(i + float2(0.0, 1.0));
    float d = hash(i + float2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

static float fbm(float2 p) {
    float val = 0.0;
    float amp = 0.5;
    for (int i = 0; i < 5; ++i) {
        val += amp * noise(p);
        p *= 2.0;
        amp *= 0.5;
    }
    return val;
}

// ─── Scene SDF functions (per preset) ───────────────────────────

static float sceneMorphSphere(float3 p, float time, float bass, float mid) {
    float radius = 1.0 + bass * 0.8;
    float disp = sin(p.x * 5.0 + time * 2.0) * sin(p.y * 5.0 + time * 1.7)
               * sin(p.z * 5.0 + time * 1.3) * mid * 0.5;
    return sdSphere(p, radius) + disp;
}

static float sceneTunnel(float3 p, float time) {
    float angle = atan2(p.y, p.x);
    float r = length(p.xy);
    float rings = cos(p.z * 1.5 + time * 0.5) * 0.3;
    float tunnel = r - (2.0 + rings + sin(angle * 6.0 + p.z * 0.5) * 0.15);
    return -tunnel; // invert so we're inside
}

static float sceneMetaballs(float3 p, float time, float bass) {
    float scale = 1.2 + bass * 0.3;
    float3 p0 = float3(sin(time * 1.1) * scale, cos(time * 0.9) * scale, sin(time * 0.7) * 0.5);
    float3 p1 = float3(cos(time * 0.8) * scale, sin(time * 1.2) * scale, cos(time * 0.6) * 0.5);
    float3 p2 = float3(sin(time * 0.6 + 2.0) * scale, cos(time * 1.0 + 1.0) * scale, sin(time * 0.9) * 0.5);
    float3 p3 = float3(cos(time * 1.3) * 0.8, sin(time * 0.5) * 0.8, cos(time * 1.1) * scale);

    float d0 = sdSphere(p - p0, 0.6 + bass * 0.2);
    float d1 = sdSphere(p - p1, 0.5 + bass * 0.15);
    float d2 = sdSphere(p - p2, 0.55 + bass * 0.18);
    float d3 = sdSphere(p - p3, 0.45 + bass * 0.12);

    float k = 0.8; // smoothness
    float d = smin(d0, d1, k);
    d = smin(d, d2, k);
    d = smin(d, d3, k);
    return d;
}

static float sceneTerrain(float3 p, float time) {
    float h = fbm(p.xz * 0.5 + float2(time * 0.05, 0.0)) * 3.0 - 1.5;
    return p.y - h;
}

// ─── Unified scene dispatcher ───────────────────────────────────

static float scene(float3 p, int preset, float time, float bass, float mid) {
    switch (preset) {
        case 0: return sceneMorphSphere(p, time, bass, mid);
        case 1: return sceneTunnel(p, time);
        case 2: return sceneMetaballs(p, time, bass);
        case 3: return sceneTerrain(p, time);
        default: return sceneMorphSphere(p, time, bass, mid);
    }
}

// ─── Normal via central differences ─────────────────────────────

static float3 calcNormal(float3 p, int preset, float time, float bass, float mid) {
    float2 e = float2(0.002, 0.0);
    return normalize(float3(
        scene(p + e.xyy, preset, time, bass, mid) - scene(p - e.xyy, preset, time, bass, mid),
        scene(p + e.yxy, preset, time, bass, mid) - scene(p - e.yxy, preset, time, bass, mid),
        scene(p + e.yyx, preset, time, bass, mid) - scene(p - e.yyx, preset, time, bass, mid)
    ));
}

// ─── Fragment shader ────────────────────────────────────────────

fragment float4 rayMarchFragment(VisVertexOut in [[stage_in]],
                                  constant RayMarchUniforms& u [[buffer(0)]]) {
    float2 uv = in.uv * 2.0 - 1.0; // remap to [-1, 1]
    uv.x *= 1.777; // approximate 16:9 aspect ratio

    // Camera setup
    float3 camPos = float3(u.camPosX, u.camPosY, u.camPosZ);
    float rotX = u.camRotX;
    float rotY = u.camRotY;

    // Camera rotation matrices
    float cosX = cos(rotX), sinX = sin(rotX);
    float cosY = cos(rotY), sinY = sin(rotY);

    float3 forward = float3(sinY * cosX, sinX, cosY * cosX);
    float3 right   = normalize(cross(forward, float3(0, 1, 0)));
    float3 up      = cross(right, forward);

    // Ray direction
    float3 rd = normalize(uv.x * right + uv.y * up + 1.5 * forward);
    float3 ro = camPos;

    // Ray march loop — 64 steps
    float totalDist = 0.0;
    float minDist = 999.0;
    bool hit = false;

    for (int i = 0; i < 64; ++i) {
        float3 p = ro + rd * totalDist;
        float d = scene(p, u.preset, u.time, u.bass, u.mid);
        minDist = min(minDist, d);
        if (d < 0.001) {
            hit = true;
            break;
        }
        if (totalDist > 50.0) break;
        totalDist += d;
    }

    // Background gradient for missed rays
    float3 bgTop = float3(0.05, 0.05, 0.15);
    float3 bgBot = float3(0.0, 0.0, 0.05);
    float3 bgColor = mix(bgBot, bgTop, in.uv.y);

    // Soft glow for near-misses
    float glow = clamp(0.02 / (minDist + 0.01), 0.0, 1.0) * 0.4;

    if (!hit) {
        float3 col = bgColor + float3(glow * 0.3, glow * 0.2, glow * 0.5);
        // Beat flash
        col += float3(u.beatIntensity * 0.05);
        return float4(clamp(col, 0.0, 1.0), 1.0);
    }

    // Hit point and normal
    float3 hitPos = ro + rd * totalDist;
    float3 norm = calcNormal(hitPos, u.preset, u.time, u.bass, u.mid);

    // Light direction
    float3 lightDir = normalize(float3(0.6, 0.8, -0.5));

    // Phong lighting
    float ambient = 0.15;
    float diffuse = max(dot(norm, lightDir), 0.0);
    float3 halfVec = normalize(lightDir - rd);
    float specular = pow(max(dot(norm, halfVec), 0.0), 32.0);

    float lighting = ambient + diffuse * 0.7 + specular * 0.5;

    // Audio-reactive hue based on energy
    float hue = u.energy * 0.8 + u.time * 0.1;
    float3 baseColor;
    baseColor.r = clamp(0.5 + 0.5 * cos(6.2832 * (hue + 0.0)), 0.0, 1.0);
    baseColor.g = clamp(0.5 + 0.5 * cos(6.2832 * (hue + 0.333)), 0.0, 1.0);
    baseColor.b = clamp(0.5 + 0.5 * cos(6.2832 * (hue + 0.667)), 0.0, 1.0);

    // Brighten with audio bands
    baseColor = mix(baseColor, float3(1.0), u.high * 0.2);

    float3 col = baseColor * lighting;

    // Distance fog
    float fog = exp(-totalDist * 0.06);
    col = mix(bgColor, col, fog);

    // Beat intensity flash — add white
    col += float3(u.beatIntensity * 0.15);

    return float4(clamp(col, 0.0, 1.0), 1.0);
}
