#include <metal_stdlib>
using namespace metal;

// ─── Shared Uniforms ───────────────────────────────────────────────

struct CausticUniforms {
    float time;
    float tiltX;
    float tiltY;
    float alphaScale;       // 0.18 dark, 0.35 light
    int   lightTheme;       // 0 = dark, 1 = light
    float speedMul;         // 18.0 on Pro, 1.0 on mini
    float viewWidth;
    float viewHeight;
    // Ripple data (up to 6)
    int   rippleCount;
    float rippleX[6];
    float rippleY[6];
    float rippleAge[6];
    float rippleMaxRadius[6];
    float _pad0;            // align to 16-byte boundary for float4
    float4 rippleColor;     // RGBA accent color for ripples
};

// ─── Vertex Shader (fullscreen quad) ───────────────────────────────

struct VertexOut {
    float4 position [[position]];
    float2 uv;
};

vertex VertexOut causticVertex(uint vid [[vertex_id]]) {
    // Fullscreen triangle pair from vertex ID
    float2 positions[4] = { {-1, -1}, {1, -1}, {-1, 1}, {1, 1} };
    float2 uvs[4]       = { {0, 1},   {1, 1},  {0, 0},  {1, 0} };

    VertexOut out;
    out.position = float4(positions[vid], 0.0, 1.0);
    out.uv = uvs[vid];
    return out;
}

// ─── Caustic Fragment Shader ───────────────────────────────────────

fragment float4 causticFragment(VertexOut in [[stage_in]],
                                constant CausticUniforms& u [[buffer(0)]]) {
    float x = in.uv.x * u.viewWidth;
    float y = in.uv.y * u.viewHeight;
    float t = u.time;

    float tiltOffsetX = u.tiltX * 1.5;
    float tiltOffsetY = u.tiltY * 1.5;

    // 5 wandering wave directions (exact port of CPU code)
    float angle1 = sin(t * 0.003) * 1.5 + sin(t * 0.0017 + 1.0) * 0.8
                  + sin(t * 0.0007 + 0.3) * 2.0 + tiltOffsetX;
    float angle2 = sin(t * 0.0025 + 2.0) * 1.3 + sin(t * 0.0013 + 3.0) * 0.9
                  + sin(t * 0.0005 + 1.7) * 1.8 + tiltOffsetY;
    float angle3 = sin(t * 0.002 + 4.5) * 1.6 + sin(t * 0.0011 + 5.0) * 0.7
                  + sin(t * 0.0004 + 4.0) * 2.2 + (tiltOffsetX + tiltOffsetY) * 0.5;
    float angle4 = sin(t * 0.0018 + 1.2) * 1.4 + sin(t * 0.0009 + 2.8) * 1.1
                  + tiltOffsetX * 0.7;
    float angle5 = sin(t * 0.0022 + 3.7) * 1.2 + sin(t * 0.0015 + 0.5) * 0.6
                  + tiltOffsetY * 0.8;

    float cos1 = cos(angle1), sin1 = sin(angle1);
    float cos2 = cos(angle2), sin2 = sin(angle2);
    float cos3 = cos(angle3), sin3 = sin(angle3);
    float cos4 = cos(angle4), sin4 = sin(angle4);
    float cos5 = cos(angle5), sin5 = sin(angle5);

    float sp1 = 0.12 * u.speedMul, sp2 = 0.095 * u.speedMul, sp3 = 0.075 * u.speedMul;
    float sp4 = 0.11 * u.speedMul,  sp5 = 0.065 * u.speedMul;

    // 5-wave interference
    float d1 = (x * cos1 + y * sin1) * 0.018 + t * sp1;
    float d2 = (x * cos2 + y * sin2) * 0.022 + t * sp2;
    float d3 = (x * cos3 + y * sin3) * 0.015 + t * sp3;
    float d4 = (x * cos4 + y * sin4) * 0.025 + t * sp4;
    float d5 = (x * cos5 + y * sin5) * 0.013 + t * sp5;

    float raw = (sin(d1) + sin(d2) + sin(d3) + sin(d4) + sin(d5)) / 5.0;
    raw = raw * 0.5 + 0.5;

    // Cubic power — concentrates brightness into thin caustic lines
    float caustic = raw * raw * raw;

    // Fine detail layer at different scale
    float fine1 = sin((x * cos2 - y * sin1) * 0.035 + t * sp3 * 1.3);
    float fine2 = sin((x * cos4 + y * sin3) * 0.030 + t * sp1 * 0.8);
    float fineDetail = (fine1 + fine2) * 0.25 + 0.5;
    fineDetail = fineDetail * fineDetail;

    float combined = caustic * 0.7 + fineDetail * 0.3;
    float alpha = combined * u.alphaScale;

    // Color palette
    float3 brightAqua, deepBlue, warmCyan;
    if (u.lightTheme) {
        brightAqua = float3(0x18/255.0, 0x88/255.0, 0xcc/255.0);
        deepBlue   = float3(0x08/255.0, 0x58/255.0, 0xa0/255.0);
        warmCyan   = float3(0x20/255.0, 0xa0/255.0, 0xd8/255.0);
    } else {
        brightAqua = float3(0x70/255.0, 0xe0/255.0, 0xd0/255.0);
        deepBlue   = float3(0x30/255.0, 0x90/255.0, 0xc0/255.0);
        warmCyan   = float3(0x50/255.0, 0xc8/255.0, 0xb8/255.0);
    }

    float hueShift = sin(x * 0.003 + y * 0.004 + t * 0.008) * 0.5 + 0.5;
    float brightShift = caustic;
    float3 col = mix(deepBlue, brightAqua, hueShift);
    col = mix(col, warmCyan, brightShift * 0.4);

    // ── Ripple overlay ──
    float4 rippleAccum = float4(0.0);
    for (int i = 0; i < u.rippleCount; ++i) {
        float dist = length(float2(x - u.rippleX[i], y - u.rippleY[i]));
        float progress = u.rippleAge[i] / 1.2;
        float radius = u.rippleMaxRadius[i] * progress;
        float ripAlpha = (1.0 - progress) * 0.3;

        // Outer ring — soft band around the expanding circle
        float ringDist = abs(dist - radius);
        float ring = smoothstep(3.0, 0.0, ringDist) * ripAlpha;

        // Inner ring at 0.6x radius
        float innerR = radius * 0.6;
        float innerDist = abs(dist - innerR);
        float innerRing = smoothstep(2.5, 0.0, innerDist) * ripAlpha * 0.5;

        rippleAccum += float4(u.rippleColor.rgb, 1.0) * (ring + innerRing);
    }

    float4 result = float4(col, clamp(alpha, 0.0, 1.0));
    // Additive blend ripples on top — clamp to prevent overbright whiteout
    result.rgb = clamp(result.rgb + rippleAccum.rgb * rippleAccum.a, 0.0, 1.0);
    result.a = clamp(result.a + rippleAccum.a, 0.0, 1.0);

    return result;
}

// ─── Glass Edge Lighting Shader ────────────────────────────────────
// Renders caustic refraction along glass pane edges.
// Input: a texture with glass pane edge masks (white = edge, black = not)
// We compute caustic intensity at each pixel and multiply by the mask.

struct EdgeUniforms {
    float time;
    float tiltX;
    float tiltY;
    float speedMul;
    float viewWidth;
    float viewHeight;
    int   lightTheme;
};

fragment float4 edgeLightFragment(VertexOut in [[stage_in]],
                                  constant EdgeUniforms& u [[buffer(0)]],
                                  texture2d<float> edgeMask [[texture(0)]]) {
    constexpr sampler s(mag_filter::linear, min_filter::linear);
    float4 mask = edgeMask.sample(s, in.uv);
    if (mask.a < 0.01) return float4(0.0);

    float x = in.uv.x * u.viewWidth;
    float y = in.uv.y * u.viewHeight;
    float t = u.time;
    float etx = u.tiltX * 1.5, ety = u.tiltY * 1.5;

    // 3-wave edge lighting (matches CPU paintOverChildren code)
    float ea1 = sin(t * 0.003) * 1.5 + sin(t * 0.0017 + 1.0) * 0.8 + sin(t * 0.0007 + 0.3) * 2.0 + etx;
    float ea2 = sin(t * 0.0025 + 2.0) * 1.3 + sin(t * 0.0013 + 3.0) * 0.9 + sin(t * 0.0005 + 1.7) * 1.8 + ety;
    float ea3 = sin(t * 0.002 + 4.5) * 1.6 + sin(t * 0.0011 + 5.0) * 0.7 + sin(t * 0.0004 + 4.0) * 2.2 + (etx + ety) * 0.5;

    float eco1 = cos(ea1), esi1 = sin(ea1);
    float eco2 = cos(ea2), esi2 = sin(ea2);
    float eco3 = cos(ea3), esi3 = sin(ea3);

    float eMul = u.speedMul;
    float d1 = (x * eco1 + y * esi1) * 0.016 + t * 0.12 * eMul;
    float d2 = (x * eco2 + y * esi2) * 0.020 + t * 0.095 * eMul;
    float d3 = (x * eco3 + y * esi3) * 0.013 + t * 0.075 * eMul;
    float cv = (sin(d1) + sin(d2) + sin(d3)) / 3.0;
    cv = cv * 0.5 + 0.5;
    float intensity = cv * cv;

    // Color with hue shift
    float hue = sin(x * 0.004 + t * 0.008) * 0.5 + 0.5;
    float3 c1 = u.lightTheme
        ? float3(0x20/255.0, 0x90/255.0, 0xd0/255.0)
        : float3(0x60/255.0, 0xc8/255.0, 0xc0/255.0);
    float3 c2 = u.lightTheme
        ? float3(0x10/255.0, 0x60/255.0, 0xa0/255.0)
        : float3(0x40/255.0, 0x98/255.0, 0xd0/255.0);
    float3 col = mix(c1, c2, hue);

    // mask.r encodes edge alpha multiplier (e.g. 0.15 for top, 0.12 for left, etc.)
    float edgeAlpha = intensity * mask.r;

    return float4(col, clamp(edgeAlpha, 0.0, 1.0));
}

// ─── Gaussian Blur (separable, two-pass) ───────────────────────────
// Pass 1: horizontal, Pass 2: vertical. Use same shader with direction uniform.

struct BlurUniforms {
    float2 direction;  // (1/w, 0) for horizontal, (0, 1/h) for vertical
    float  radius;     // blur radius in pixels
};

fragment float4 gaussianBlurFragment(VertexOut in [[stage_in]],
                                     constant BlurUniforms& u [[buffer(0)]],
                                     texture2d<float> inputTex [[texture(0)]]) {
    constexpr sampler s(mag_filter::linear, min_filter::linear, address::clamp_to_edge);

    // 9-tap Gaussian kernel (sigma ~= radius/3)
    const float weights[9] = {
        0.0162, 0.0540, 0.1216, 0.1942, 0.2280,
        0.1942, 0.1216, 0.0540, 0.0162
    };

    float4 result = float4(0.0);
    for (int i = -4; i <= 4; ++i) {
        float2 offset = u.direction * float(i) * u.radius;
        result += inputTex.sample(s, in.uv + offset) * weights[i + 4];
    }
    return result;
}

// ─── Button Caustic Glow Shader ────────────────────────────────────
// Renders caustic light on button rectangles.
// Uses same wave math as edge lighting but samples at button center
// and fills a rounded rect with gradient.

struct ButtonRect {
    float x, y, w, h;  // button bounds in pixels
};

struct ButtonUniforms {
    float time;
    float tiltX;
    float tiltY;
    float speedMul;
    float viewWidth;
    float viewHeight;
    int   lightTheme;
    int   buttonCount;
    ButtonRect buttons[96];  // up to 96 visible buttons
};

fragment float4 buttonGlowFragment(VertexOut in [[stage_in]],
                                   constant ButtonUniforms& u [[buffer(0)]]) {
    float px = in.uv.x * u.viewWidth;
    float py = in.uv.y * u.viewHeight;
    float t = u.time;

    float4 result = float4(0.0);

    int count = min(u.buttonCount, 96);
    for (int i = 0; i < count; ++i) {
        ButtonRect btn = u.buttons[i];
        if (btn.w < 1.0 || btn.h < 1.0) continue;  // skip zero-size buttons
        // Check if pixel is inside this button (with 12px corner radius)
        float2 center = float2(btn.x + btn.w * 0.5, btn.y + btn.h * 0.5);
        float2 halfSize = float2(btn.w * 0.5, btn.h * 0.5);
        float2 d = abs(float2(px, py) - center) - halfSize + 12.0;
        float dist = length(max(d, 0.0)) - 12.0;
        if (dist > 0.0) continue;  // outside rounded rect

        // Compute caustic at button center
        float bx = center.x, by = center.y;
        float btx = u.tiltX * 1.5, bty = u.tiltY * 1.5;
        float a1 = sin(t * 0.003) * 1.5 + sin(t * 0.0017 + 1.0) * 0.8 + sin(t * 0.0007 + 0.3) * 2.0 + btx;
        float a2 = sin(t * 0.0025 + 2.0) * 1.3 + sin(t * 0.0013 + 3.0) * 0.9 + sin(t * 0.0005 + 1.7) * 1.8 + bty;
        float a3 = sin(t * 0.002 + 4.5) * 1.6 + sin(t * 0.0011 + 5.0) * 0.7 + sin(t * 0.0004 + 4.0) * 2.2 + (btx + bty) * 0.5;
        float bsMul = u.speedMul;
        float s1 = sin((bx * cos(a1) + by * sin(a1)) * 0.016 + t * 0.12 * bsMul);
        float s2 = sin((bx * cos(a2) + by * sin(a2)) * 0.020 + t * 0.095 * bsMul);
        float s3 = sin((bx * cos(a3) + by * sin(a3)) * 0.013 + t * 0.075 * bsMul);
        float caustic = (s1 + s2 + s3) / 3.0 * 0.5 + 0.5;
        caustic = caustic * caustic;

        float alpha = caustic * (u.lightTheme ? 0.22 : 0.10);
        if (alpha < 0.01) continue;

        // Top-to-bottom gradient within button
        float yFrac = (py - btn.y) / max(btn.h, 1.0);
        alpha *= mix(1.0, 0.2, yFrac);

        float hueB = sin(bx * 0.003 + by * 0.004 + t * 0.01) * 0.5 + 0.5;
        float3 col;
        if (u.lightTheme) {
            col = mix(float3(0x20/255.0, 0x90/255.0, 0xd0/255.0),
                      float3(0x10/255.0, 0x60/255.0, 0xa0/255.0), hueB);
        } else {
            col = mix(float3(0x60/255.0, 0xc8/255.0, 0xc0/255.0),
                      float3(0x40/255.0, 0x98/255.0, 0xd0/255.0), hueB);
        }

        result += float4(col * alpha, alpha);
    }

    return float4(result.rgb, clamp(result.a, 0.0, 1.0));
}
