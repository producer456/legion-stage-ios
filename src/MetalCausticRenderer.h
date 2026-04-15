#pragma once

// Uniform structures matching the Metal shader — no JUCE dependency
struct CausticUniforms {
    float time;
    float tiltX;
    float tiltY;
    float alphaScale;
    int   lightTheme;
    float speedMul;
    float viewWidth;
    float viewHeight;
    int   rippleCount;
    float rippleX[6];
    float rippleY[6];
    float rippleAge[6];
    float rippleMaxRadius[6];
    float rippleColor[4]; // RGBA
};

struct BlurUniforms {
    float direction[2];
    float radius;
};

struct ButtonRect {
    float x, y, w, h;
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
    ButtonRect buttons[64];
};

/**
 * MetalCausticRenderer — GPU-accelerated caustic lighting for glass overlay themes.
 *
 * Manages a CAMetalLayer inserted behind JUCE's view. Renders:
 * 1. Fullscreen caustic wave interference pattern
 * 2. Ripple effects from touch events
 * 3. Button caustic glow overlay
 *
 * Call updateUniforms() each frame from timerCallback, then render().
 * The Metal layer composites underneath JUCE's software-rendered content.
 */
class MetalCausticRenderer
{
public:
    MetalCausticRenderer();
    ~MetalCausticRenderer();

    /** Attach to a native UIView handle (void* from ComponentPeer::getNativeHandle()). */
    void attachToView(void* nativeViewHandle);

    /** Detach and remove the Metal layer. */
    void detach();

    /** Returns true if Metal is available and the renderer is attached. */
    bool isAttached() const;

    /** Update caustic uniforms. Call each frame before render(). */
    void setCausticUniforms(const CausticUniforms& uniforms);

    /** Update button glow data. */
    void setButtonUniforms(const ButtonUniforms& uniforms);

    /** Render one frame. Call from timerCallback or a CADisplayLink. */
    void render();

    /** Show/hide the Metal layer (e.g. when switching themes). */
    void setVisible(bool visible);

    /** Update Metal layer frame to match component bounds. */
    void setBounds(int x, int y, int width, int height);

    /** Set the opacity of the caustic layer for dimming (e.g. inside timeline). */
    void setOpacity(float opacity);

private:
    class Impl;
    Impl* pimpl;  // raw pointer — avoid unique_ptr header dependency
};
