// MetalVisualizerRenderer — GPU-accelerated visualizer rendering via Metal

#include <TargetConditionals.h>

#if TARGET_OS_IOS

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <UIKit/UIKit.h>

#include <vector>
#include <algorithm>
#include <cstdint>
#include "MetalVisualizerRenderer.h"

class MetalVisualizerRenderer::Impl
{
public:
    Impl()
    {
        device = MTLCreateSystemDefaultDevice();
        if (!device) return;

        commandQueue = [device newCommandQueue];

        NSError* error = nil;
        library = [device newDefaultLibrary];
        if (!library)
        {
            NSString* path = [[NSBundle mainBundle] pathForResource:@"default" ofType:@"metallib"];
            if (path)
                library = [device newLibraryWithURL:[NSURL fileURLWithPath:path] error:&error];
        }
        if (!library) { NSLog(@"MetalVisualizer: Failed to load Metal library"); return; }

        setupPipelines();
        metalAvailable = (shaderToyPipeline != nil);
    }

    ~Impl() { detach(); }

    void setupPipelines()
    {
        id<MTLFunction> vertFunc = [library newFunctionWithName:@"visVertex"];
        if (!vertFunc) return;

        auto makePipeline = [&](NSString* fragName) -> id<MTLRenderPipelineState> {
            id<MTLFunction> fragFunc = [library newFunctionWithName:fragName];
            if (!fragFunc) return nil;
            MTLRenderPipelineDescriptor* desc = [[MTLRenderPipelineDescriptor alloc] init];
            desc.vertexFunction = vertFunc;
            desc.fragmentFunction = fragFunc;
            desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
            NSError* error = nil;
            id<MTLRenderPipelineState> ps = [device newRenderPipelineStateWithDescriptor:desc error:&error];
            if (!ps) NSLog(@"MetalVisualizer: Pipeline %@ error: %@", fragName, error);
            return ps;
        };

        shaderToyPipeline   = makePipeline(@"shaderToyFragment");
        spectrumPipeline    = makePipeline(@"spectrumFragment");
        waveTerrainPipeline = makePipeline(@"waveTerrainFragment");
        lissajousPipeline   = makePipeline(@"lissajousFragment");
        analyzerPipeline    = makePipeline(@"analyzerFragment");
        palettePipeline     = makePipeline(@"paletteLookupFragment");

        // Compute pipelines
        auto makeCompute = [&](NSString* name) -> id<MTLComputePipelineState> {
            id<MTLFunction> func = [library newFunctionWithName:name];
            if (!func) return nil;
            NSError* error = nil;
            id<MTLComputePipelineState> ps = [device newComputePipelineStateWithFunction:func error:&error];
            if (!ps) NSLog(@"MetalVisualizer: Compute %@ error: %@", name, error);
            return ps;
        };

        warpBlurCompute  = makeCompute(@"warpBlurCompute");
        effectsCompute   = makeCompute(@"effectsCompute");
    }

    void attachToView(void* nativeViewHandle)
    {
        if (!metalAvailable || !nativeViewHandle) return;
        UIView* view = (__bridge UIView*)nativeViewHandle;
        if (metalLayer) detach();

        metalLayer = [CAMetalLayer layer];
        metalLayer.device = device;
        metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
        metalLayer.framebufferOnly = YES;
        metalLayer.opaque = YES;

        CGFloat scale = view.window.screen.scale;
        if (scale < 1.0) scale = [UIScreen mainScreen].scale;
        metalLayer.contentsScale = scale;

        CGRect frame = view.bounds;
        if (frame.size.width < 1 || frame.size.height < 1) { metalLayer = nil; return; }
        metalLayer.frame = frame;
        metalLayer.drawableSize = CGSizeMake(frame.size.width * scale, frame.size.height * scale);

        [view.layer insertSublayer:metalLayer atIndex:0];
        metalLayer.zPosition = -1;
        attached = true;
    }

    void detach()
    {
        if (metalLayer) { [metalLayer removeFromSuperlayer]; metalLayer = nil; }
        attached = false;
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

    // ── Generic render with a pipeline + uniforms ──
    void renderWithPipeline(id<MTLRenderPipelineState> pipeline, const void* uniforms, size_t size)
    {
        if (!attached || !metalLayer || !pipeline) return;
        id<CAMetalDrawable> drawable = [metalLayer nextDrawable];
        if (!drawable) return;

        MTLRenderPassDescriptor* passDesc = [MTLRenderPassDescriptor renderPassDescriptor];
        passDesc.colorAttachments[0].texture = drawable.texture;
        passDesc.colorAttachments[0].loadAction = MTLLoadActionClear;
        passDesc.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);
        passDesc.colorAttachments[0].storeAction = MTLStoreActionStore;

        id<MTLCommandBuffer> cmdBuf = [commandQueue commandBuffer];
        id<MTLRenderCommandEncoder> enc = [cmdBuf renderCommandEncoderWithDescriptor:passDesc];
        [enc setRenderPipelineState:pipeline];
        [enc setFragmentBytes:uniforms length:size atIndex:0];
        [enc drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
        [enc endEncoding];
        [cmdBuf presentDrawable:drawable];
        [cmdBuf commit];
    }

    void renderShaderToy(const ShaderToyUniforms& u) {
        renderWithPipeline(shaderToyPipeline, &u, sizeof(u));
    }
    void renderSpectrum(const SpectrumGPUUniforms& u) {
        renderWithPipeline(spectrumPipeline, &u, sizeof(u));
    }
    void renderWaveTerrain(const WaveTerrainGPUUniforms& u) {
        renderWithPipeline(waveTerrainPipeline, &u, sizeof(u));
    }
    void renderLissajous(const LissajousGPUUniforms& u) {
        renderWithPipeline(lissajousPipeline, &u, sizeof(u));
    }
    void renderAnalyzer(const AnalyzerGPUUniforms& u) {
        renderWithPipeline(analyzerPipeline, &u, sizeof(u));
    }

    // ── Warp pipeline (Geiss/ProjectM) ──

    void initWarpBuffers(int w, int h)
    {
        if (warpW == w && warpH == h) return;
        warpW = w; warpH = h;

        MTLTextureDescriptor* desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR32Float
                                                                                       width:w height:h mipmapped:NO];
        desc.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
        desc.storageMode = MTLStorageModeShared;
        warpBufA = [device newTextureWithDescriptor:desc];
        warpBufB = [device newTextureWithDescriptor:desc];

        // Warp map: RG16Unorm for dx,dy displacements
        MTLTextureDescriptor* mapDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRG16Unorm
                                                                                          width:w height:h mipmapped:NO];
        mapDesc.usage = MTLTextureUsageShaderRead;
        mapDesc.storageMode = MTLStorageModeShared;
        warpMapTex = [device newTextureWithDescriptor:mapDesc];

        // Clear both buffers
        std::vector<float> zeros(w * h, 0.0f);
        MTLRegion region = MTLRegionMake2D(0, 0, w, h);
        [warpBufA replaceRegion:region mipmapLevel:0 withBytes:zeros.data() bytesPerRow:w * sizeof(float)];
        [warpBufB replaceRegion:region mipmapLevel:0 withBytes:zeros.data() bytesPerRow:w * sizeof(float)];
    }

    void uploadWarpMap(const int* mapDx, const int* mapDy, int w, int h)
    {
        if (!warpMapTex || warpW != w || warpH != h) return;

        // Encode dx,dy as uint16 values: value + 32768 mapped to [0, 65535]
        std::vector<uint16_t> mapData(w * h * 2);
        for (int i = 0; i < w * h; ++i) {
            mapData[i * 2]     = static_cast<uint16_t>(std::max(0, std::min(65535, mapDx[i] + 32768)));
            mapData[i * 2 + 1] = static_cast<uint16_t>(std::max(0, std::min(65535, mapDy[i] + 32768)));
        }

        MTLRegion region = MTLRegionMake2D(0, 0, w, h);
        [warpMapTex replaceRegion:region mipmapLevel:0 withBytes:mapData.data() bytesPerRow:w * 2 * sizeof(uint16_t)];
    }

    void executeWarpBlur()
    {
        if (!warpBlurCompute || !warpBufA || !warpBufB || !warpMapTex) return;

        id<MTLCommandBuffer> cmdBuf = [commandQueue commandBuffer];
        id<MTLComputeCommandEncoder> enc = [cmdBuf computeCommandEncoder];
        [enc setComputePipelineState:warpBlurCompute];
        [enc setTexture:warpBufA atIndex:0];  // source
        [enc setTexture:warpBufB atIndex:1];  // dest
        [enc setTexture:warpMapTex atIndex:2]; // warp map

        MTLSize gridSize = MTLSizeMake(warpW, warpH, 1);
        MTLSize threadGroup = MTLSizeMake(16, 16, 1);
        [enc dispatchThreads:gridSize threadsPerThreadgroup:threadGroup];
        [enc endEncoding];
        [cmdBuf commit];
        [cmdBuf waitUntilCompleted];
    }

    void uploadEffectPoints(const EffectPoint* points, int count)
    {
        effectPointCount = count;
        if (count <= 0) return;
        size_t size = count * sizeof(EffectPoint);
        if (!effectPointBuffer || [effectPointBuffer length] < size)
            effectPointBuffer = [device newBufferWithLength:std::max(size, (size_t)4096) options:MTLResourceStorageModeShared];
        memcpy([effectPointBuffer contents], points, size);
    }

    void executeEffects()
    {
        if (!effectsCompute || !warpBufB || effectPointCount <= 0) return;

        id<MTLCommandBuffer> cmdBuf = [commandQueue commandBuffer];
        id<MTLComputeCommandEncoder> enc = [cmdBuf computeCommandEncoder];
        [enc setComputePipelineState:effectsCompute];
        [enc setTexture:warpBufB atIndex:0];
        [enc setBuffer:effectPointBuffer offset:0 atIndex:0];
        [enc setBytes:&effectPointCount length:sizeof(int) atIndex:1];

        MTLSize gridSize = MTLSizeMake(warpW, warpH, 1);
        MTLSize threadGroup = MTLSizeMake(16, 16, 1);
        [enc dispatchThreads:gridSize threadsPerThreadgroup:threadGroup];
        [enc endEncoding];
        [cmdBuf commit];
        [cmdBuf waitUntilCompleted];
    }

    void swapWarpBuffers()
    {
        std::swap(warpBufA, warpBufB);
    }

    void renderPalette(const uint32_t* palette256, const PaletteGPUUniforms& uniforms)
    {
        if (!attached || !metalLayer || !palettePipeline || !warpBufA) return;

        // Upload palette as 1D texture
        if (!paletteTex) {
            MTLTextureDescriptor* desc = [MTLTextureDescriptor new];
            desc.textureType = MTLTextureType1D;
            desc.pixelFormat = MTLPixelFormatRGBA8Unorm;
            desc.width = 256;
            desc.usage = MTLTextureUsageShaderRead;
            desc.storageMode = MTLStorageModeShared;
            paletteTex = [device newTextureWithDescriptor:desc];
        }

        // Convert palette from packed ARGB to RGBA bytes
        uint8_t paletteRGBA[256 * 4];
        for (int i = 0; i < 256; ++i) {
            uint32_t col = palette256[i];
            paletteRGBA[i * 4 + 0] = (col >> 16) & 0xFF; // R
            paletteRGBA[i * 4 + 1] = (col >> 8) & 0xFF;  // G
            paletteRGBA[i * 4 + 2] = col & 0xFF;          // B
            paletteRGBA[i * 4 + 3] = 0xFF;                 // A
        }
        MTLRegion region = MTLRegionMake1D(0, 256);
        [paletteTex replaceRegion:region mipmapLevel:0 withBytes:paletteRGBA bytesPerRow:256 * 4];

        id<CAMetalDrawable> drawable = [metalLayer nextDrawable];
        if (!drawable) return;

        MTLRenderPassDescriptor* passDesc = [MTLRenderPassDescriptor renderPassDescriptor];
        passDesc.colorAttachments[0].texture = drawable.texture;
        passDesc.colorAttachments[0].loadAction = MTLLoadActionClear;
        passDesc.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);
        passDesc.colorAttachments[0].storeAction = MTLStoreActionStore;

        id<MTLCommandBuffer> cmdBuf = [commandQueue commandBuffer];
        id<MTLRenderCommandEncoder> enc = [cmdBuf renderCommandEncoderWithDescriptor:passDesc];
        [enc setRenderPipelineState:palettePipeline];
        [enc setFragmentBytes:&uniforms length:sizeof(uniforms) atIndex:0];
        [enc setFragmentTexture:warpBufA atIndex:0];  // indexed buffer (after swap)
        [enc setFragmentTexture:paletteTex atIndex:1]; // palette
        [enc drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
        [enc endEncoding];
        [cmdBuf presentDrawable:drawable];
        [cmdBuf commit];
    }

    // State
    bool metalAvailable = false;
    bool attached = false;

    id<MTLDevice> device = nil;
    id<MTLCommandQueue> commandQueue = nil;
    id<MTLLibrary> library = nil;
    CAMetalLayer* metalLayer = nil;

    // Render pipelines
    id<MTLRenderPipelineState> shaderToyPipeline = nil;
    id<MTLRenderPipelineState> spectrumPipeline = nil;
    id<MTLRenderPipelineState> waveTerrainPipeline = nil;
    id<MTLRenderPipelineState> lissajousPipeline = nil;
    id<MTLRenderPipelineState> analyzerPipeline = nil;
    id<MTLRenderPipelineState> palettePipeline = nil;

    // Compute pipelines
    id<MTLComputePipelineState> warpBlurCompute = nil;
    id<MTLComputePipelineState> effectsCompute = nil;

    // Warp state (Geiss/ProjectM)
    int warpW = 0, warpH = 0;
    id<MTLTexture> warpBufA = nil;
    id<MTLTexture> warpBufB = nil;
    id<MTLTexture> warpMapTex = nil;
    id<MTLTexture> paletteTex = nil;
    id<MTLBuffer> effectPointBuffer = nil;
    int effectPointCount = 0;
};

// ─── Public API ────────────────────────────────────────────────────

MetalVisualizerRenderer::MetalVisualizerRenderer() : pimpl(new Impl()) {}
MetalVisualizerRenderer::~MetalVisualizerRenderer() { delete pimpl; }

void MetalVisualizerRenderer::attachToView(void* v) { pimpl->attachToView(v); }
void MetalVisualizerRenderer::detach() { pimpl->detach(); }
bool MetalVisualizerRenderer::isAttached() const { return pimpl->attached; }
bool MetalVisualizerRenderer::isAvailable() const { return pimpl->metalAvailable; }
void MetalVisualizerRenderer::setBounds(int x, int y, int w, int h) { pimpl->setBounds(x, y, w, h); }

void MetalVisualizerRenderer::renderShaderToy(const ShaderToyUniforms& u) { pimpl->renderShaderToy(u); }
void MetalVisualizerRenderer::renderSpectrum(const SpectrumGPUUniforms& u) { pimpl->renderSpectrum(u); }
void MetalVisualizerRenderer::renderWaveTerrain(const WaveTerrainGPUUniforms& u) { pimpl->renderWaveTerrain(u); }
void MetalVisualizerRenderer::renderLissajous(const LissajousGPUUniforms& u) { pimpl->renderLissajous(u); }
void MetalVisualizerRenderer::renderAnalyzer(const AnalyzerGPUUniforms& u) { pimpl->renderAnalyzer(u); }

void MetalVisualizerRenderer::initWarpBuffers(int w, int h) { pimpl->initWarpBuffers(w, h); }
void MetalVisualizerRenderer::uploadWarpMap(const int* dx, const int* dy, int w, int h) { pimpl->uploadWarpMap(dx, dy, w, h); }
void MetalVisualizerRenderer::executeWarpBlur() { pimpl->executeWarpBlur(); }
void MetalVisualizerRenderer::uploadEffectPoints(const EffectPoint* p, int c) { pimpl->uploadEffectPoints(p, c); }
void MetalVisualizerRenderer::executeEffects() { pimpl->executeEffects(); }
void MetalVisualizerRenderer::swapWarpBuffers() { pimpl->swapWarpBuffers(); }
void MetalVisualizerRenderer::renderPalette(const uint32_t* pal, const PaletteGPUUniforms& u) { pimpl->renderPalette(pal, u); }

#else
// Non-iOS stubs
MetalVisualizerRenderer::MetalVisualizerRenderer() : pimpl(nullptr) {}
MetalVisualizerRenderer::~MetalVisualizerRenderer() {}
void MetalVisualizerRenderer::attachToView(void*) {}
void MetalVisualizerRenderer::detach() {}
bool MetalVisualizerRenderer::isAttached() const { return false; }
bool MetalVisualizerRenderer::isAvailable() const { return false; }
void MetalVisualizerRenderer::setBounds(int, int, int, int) {}
void MetalVisualizerRenderer::renderShaderToy(const ShaderToyUniforms&) {}
void MetalVisualizerRenderer::renderSpectrum(const SpectrumGPUUniforms&) {}
void MetalVisualizerRenderer::renderWaveTerrain(const WaveTerrainGPUUniforms&) {}
void MetalVisualizerRenderer::renderLissajous(const LissajousGPUUniforms&) {}
void MetalVisualizerRenderer::renderAnalyzer(const AnalyzerGPUUniforms&) {}
void MetalVisualizerRenderer::initWarpBuffers(int, int) {}
void MetalVisualizerRenderer::uploadWarpMap(const int*, const int*, int, int) {}
void MetalVisualizerRenderer::executeWarpBlur() {}
void MetalVisualizerRenderer::uploadEffectPoints(const EffectPoint*, int) {}
void MetalVisualizerRenderer::executeEffects() {}
void MetalVisualizerRenderer::swapWarpBuffers() {}
void MetalVisualizerRenderer::renderPalette(const uint32_t*, const PaletteGPUUniforms&) {}
#endif
