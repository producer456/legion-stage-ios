// MetalCausticRenderer — GPU caustic rendering via Metal
// This file deliberately does NOT include JuceHeader.h to avoid
// JUCE's juce::Point conflicting with MacTypes.h's Point typedef.

#include <TargetConditionals.h>

#if TARGET_OS_IOS

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <UIKit/UIKit.h>

#include "MetalCausticRenderer.h"

// ─── Pimpl Implementation ──────────────────────────────────────────

class MetalCausticRenderer::Impl
{
public:
    Impl()
    {
        device = MTLCreateSystemDefaultDevice();
        if (!device) return;

        commandQueue = [device newCommandQueue];

        // Load shader library from default bundle
        NSError* error = nil;
        library = [device newDefaultLibrary];
        if (!library)
        {
            NSString* path = [[NSBundle mainBundle] pathForResource:@"default" ofType:@"metallib"];
            if (path)
            {
                NSURL* url = [NSURL fileURLWithPath:path];
                library = [device newLibraryWithURL:url error:&error];
            }
        }

        if (!library)
        {
            NSLog(@"MetalCausticRenderer: Failed to load Metal library: %@", error);
            return;
        }

        setupCausticPipeline();
        setupButtonGlowPipeline();

        // Only mark available if the caustic pipeline actually compiled
        metalAvailable = (causticPipeline != nil);
    }

    ~Impl()
    {
        detach();
    }

    void setupCausticPipeline()
    {
        id<MTLFunction> vertFunc = [library newFunctionWithName:@"causticVertex"];
        id<MTLFunction> fragFunc = [library newFunctionWithName:@"causticFragment"];
        if (!vertFunc || !fragFunc) return;

        MTLRenderPipelineDescriptor* desc = [[MTLRenderPipelineDescriptor alloc] init];
        desc.vertexFunction = vertFunc;
        desc.fragmentFunction = fragFunc;
        desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;

        // Premultiplied alpha blending
        desc.colorAttachments[0].blendingEnabled = YES;
        desc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorOne;
        desc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        desc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
        desc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

        NSError* error = nil;
        causticPipeline = [device newRenderPipelineStateWithDescriptor:desc error:&error];
        if (!causticPipeline)
            NSLog(@"MetalCausticRenderer: Caustic pipeline error: %@", error);
    }

    void setupButtonGlowPipeline()
    {
        id<MTLFunction> vertFunc = [library newFunctionWithName:@"causticVertex"];
        id<MTLFunction> fragFunc = [library newFunctionWithName:@"buttonGlowFragment"];
        if (!vertFunc || !fragFunc) return;

        MTLRenderPipelineDescriptor* desc = [[MTLRenderPipelineDescriptor alloc] init];
        desc.vertexFunction = vertFunc;
        desc.fragmentFunction = fragFunc;
        desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;

        // Additive blending for button glow
        desc.colorAttachments[0].blendingEnabled = YES;
        desc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorOne;
        desc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOne;
        desc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
        desc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

        NSError* error = nil;
        buttonGlowPipeline = [device newRenderPipelineStateWithDescriptor:desc error:&error];
        if (!buttonGlowPipeline)
            NSLog(@"MetalCausticRenderer: Button glow pipeline error: %@", error);
    }

    void attachToView(void* nativeViewHandle)
    {
        if (!metalAvailable || !nativeViewHandle) return;

        UIView* juceView = (__bridge UIView*)nativeViewHandle;

        if (metalLayer) detach();

        metalLayer = [CAMetalLayer layer];
        metalLayer.device = device;
        metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
        metalLayer.framebufferOnly = YES;
        metalLayer.opaque = NO;  // Transparent — composites over app background

        // contentsScale: fall back to mainScreen if view has no window yet
        CGFloat scale = juceView.window.screen.scale;
        if (scale < 1.0) scale = [UIScreen mainScreen].scale;
        metalLayer.contentsScale = scale;

        CGRect frame = juceView.bounds;
        if (frame.size.width < 1 || frame.size.height < 1) { metalLayer = nil; return; }
        metalLayer.frame = frame;
        metalLayer.drawableSize = CGSizeMake(frame.size.width * scale,
                                              frame.size.height * scale);

        // Insert behind all other sublayers and set negative zPosition
        // so JUCE sublayer reordering can't push Metal in front
        [juceView.layer insertSublayer:metalLayer atIndex:0];
        metalLayer.zPosition = -1;

        attached = true;
    }

    void detach()
    {
        if (metalLayer)
        {
            [metalLayer removeFromSuperlayer];
            metalLayer = nil;
        }
        attached = false;
    }

    void render()
    {
        if (!attached || !metalLayer || !causticPipeline) return;

        id<CAMetalDrawable> drawable = [metalLayer nextDrawable];
        if (!drawable) return;

        MTLRenderPassDescriptor* passDesc = [MTLRenderPassDescriptor renderPassDescriptor];
        passDesc.colorAttachments[0].texture = drawable.texture;
        passDesc.colorAttachments[0].loadAction = MTLLoadActionClear;
        passDesc.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 0);
        passDesc.colorAttachments[0].storeAction = MTLStoreActionStore;

        id<MTLCommandBuffer> cmdBuf = [commandQueue commandBuffer];
        id<MTLRenderCommandEncoder> encoder = [cmdBuf renderCommandEncoderWithDescriptor:passDesc];

        // ── Pass 1: Caustic background ──
        [encoder setRenderPipelineState:causticPipeline];
        [encoder setFragmentBytes:&causticUniforms length:sizeof(CausticUniforms) atIndex:0];
        [encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];

        // ── Pass 2: Button glow overlay ──
        if (buttonGlowPipeline && buttonUniforms.buttonCount > 0)
        {
            [encoder setRenderPipelineState:buttonGlowPipeline];
            [encoder setFragmentBytes:&buttonUniforms length:sizeof(ButtonUniforms) atIndex:0];
            [encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
        }

        [encoder endEncoding];
        [cmdBuf presentDrawable:drawable];
        [cmdBuf commit];
    }

    void setVisible(bool visible)
    {
        if (metalLayer)
            metalLayer.hidden = !visible;
    }

    void setBounds(int x, int y, int width, int height)
    {
        if (metalLayer && width > 0 && height > 0)
        {
            [CATransaction begin];
            [CATransaction setDisableActions:YES];
            metalLayer.frame = CGRectMake(x, y, width, height);
            metalLayer.drawableSize = CGSizeMake(width * metalLayer.contentsScale,
                                                  height * metalLayer.contentsScale);
            [CATransaction commit];
        }
    }

    void setOpacity(float opacity)
    {
        if (metalLayer)
            metalLayer.opacity = opacity;
    }

    // State
    bool metalAvailable = false;
    bool attached = false;

    id<MTLDevice> device = nil;
    id<MTLCommandQueue> commandQueue = nil;
    id<MTLLibrary> library = nil;
    id<MTLRenderPipelineState> causticPipeline = nil;
    id<MTLRenderPipelineState> buttonGlowPipeline = nil;
    CAMetalLayer* metalLayer = nil;

    CausticUniforms causticUniforms {};
    ButtonUniforms buttonUniforms {};
};

// ─── Public API ────────────────────────────────────────────────────

MetalCausticRenderer::MetalCausticRenderer() : pimpl(new Impl()) {}
MetalCausticRenderer::~MetalCausticRenderer() { delete pimpl; }

void MetalCausticRenderer::attachToView(void* nativeViewHandle) { pimpl->attachToView(nativeViewHandle); }
void MetalCausticRenderer::detach() { pimpl->detach(); }
bool MetalCausticRenderer::isAttached() const { return pimpl->attached; }

void MetalCausticRenderer::setCausticUniforms(const CausticUniforms& u)
{
    pimpl->causticUniforms = u;
}

void MetalCausticRenderer::setButtonUniforms(const ButtonUniforms& u)
{
    pimpl->buttonUniforms = u;
}

void MetalCausticRenderer::render() { pimpl->render(); }
void MetalCausticRenderer::setVisible(bool v) { pimpl->setVisible(v); }
void MetalCausticRenderer::setBounds(int x, int y, int w, int h) { pimpl->setBounds(x, y, w, h); }
void MetalCausticRenderer::setOpacity(float o) { pimpl->setOpacity(o); }

#else
// Non-iOS stubs
MetalCausticRenderer::MetalCausticRenderer() : pimpl(nullptr) {}
MetalCausticRenderer::~MetalCausticRenderer() {}
void MetalCausticRenderer::attachToView(void*) {}
void MetalCausticRenderer::detach() {}
bool MetalCausticRenderer::isAttached() const { return false; }
void MetalCausticRenderer::setCausticUniforms(const CausticUniforms&) {}
void MetalCausticRenderer::setButtonUniforms(const ButtonUniforms&) {}
void MetalCausticRenderer::render() {}
void MetalCausticRenderer::setVisible(bool) {}
void MetalCausticRenderer::setBounds(int, int, int, int) {}
void MetalCausticRenderer::setOpacity(float) {}
#endif // TARGET_OS_IOS
