#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include <fmt/format.h>

#include <zest/file/runtree.h>
#include <zest/ui/nanovg.h>

#include <vklive/metal/metal_nanovg.h>
#include <vklive/scene.h>
#include <vklive/validation.h>

namespace
{

constexpr size_t kMetalNanoVGMaxColorAttachments = 8;

template <typename T>
T bridge(void* object)
{
    return (__bridge T)object;
}

NSString* ns_string(const std::string& string)
{
    return [NSString stringWithUTF8String:string.c_str()];
}

std::string ns_string(NSString* string)
{
    return string ? std::string([string UTF8String]) : std::string();
}

void report_nanovg_error(metal::MetalPass& pass, const std::string& text)
{
    scene_report_error(pass.pass.scene, MessageSeverity::Error, text, pass.pass.scene.sceneGraphPath, pass.pass.scriptPassLine);
    validation_error(text);
}

struct MetalNanoVGVertex
{
    float x = 0.0f;
    float y = 0.0f;
    float u = 0.0f;
    float v = 0.0f;
};

struct MetalNanoVGTexture
{
    int handle = 0;
    int type = NVG_TEXTURE_RGBA;
    int width = 0;
    int height = 0;
    int flags = 0;
    __strong id<MTLTexture> texture = nil;
};

struct MetalNanoVGBackend
{
    metal::MetalContext* ctx = nullptr;
    __strong id<MTLDevice> device = nil;
    __strong id<MTLLibrary> library = nil;
    __strong id<MTLFunction> vertexFunction = nil;
    __strong id<MTLFunction> fragmentFunction = nil;
    __strong id<MTLRenderPipelineState> pipelineState = nil;
    __strong id<MTLSamplerState> sampler = nil;
    __strong id<MTLTexture> dummyTexture = nil;
    __strong id<MTLCommandBuffer> commandBuffer = nil;
    __strong id<MTLRenderCommandEncoder> encoder = nil;
    __strong NSMutableArray* liveBuffers = nil;
    std::vector<MetalNanoVGTexture> textures;
    std::vector<uint32_t> colorFormats;
    uint32_t depthFormat = 0;
    int nextTextureId = 1;
    float viewSize[2] = { 1.0f, 1.0f };
};

MetalNanoVGBackend* backend_from(metal::MetalContext& ctx)
{
    return static_cast<MetalNanoVGBackend*>(ctx.metalNanovg);
}

MetalNanoVGTexture* find_texture(MetalNanoVGBackend& backend, int id)
{
    auto itr = std::find_if(backend.textures.begin(), backend.textures.end(), [&](const auto& texture) {
        return texture.handle == id;
    });
    return itr == backend.textures.end() ? nullptr : &*itr;
}

NVGcolor solid_color(NVGpaint* paint)
{
    auto color = paint->innerColor;
    if (color.a == 0.0f && paint->outerColor.a != 0.0f)
    {
        color = paint->outerColor;
    }
    return color;
}

uint32_t texture_mode(MetalNanoVGBackend& backend, int image)
{
    auto* texture = find_texture(backend, image);
    if (!texture)
    {
        return 0;
    }
    return texture->type == NVG_TEXTURE_ALPHA ? 1 : 2;
}

id<MTLTexture> draw_texture(MetalNanoVGBackend& backend, int image)
{
    auto* texture = find_texture(backend, image);
    if (texture && texture->texture)
    {
        return texture->texture;
    }
    return backend.dummyTexture;
}

void set_draw_state(MetalNanoVGBackend& backend, NVGpaint* paint)
{
    auto color = solid_color(paint);
    float colorData[4] = { color.r, color.g, color.b, color.a };
    uint32_t useTexture = texture_mode(backend, paint->image);

    [backend.encoder setFragmentBytes:colorData length:sizeof(colorData) atIndex:0];
    [backend.encoder setFragmentBytes:&useTexture length:sizeof(useTexture) atIndex:1];
    [backend.encoder setFragmentTexture:draw_texture(backend, paint->image) atIndex:0];
    [backend.encoder setFragmentSamplerState:backend.sampler atIndex:0];
}

void draw_vertices(MetalNanoVGBackend& backend, MTLPrimitiveType primitive, const MetalNanoVGVertex* vertices, size_t count, NVGpaint* paint)
{
    if (!backend.encoder || !backend.liveBuffers || !vertices || count == 0)
    {
        return;
    }

    set_draw_state(backend, paint);
    const auto vertexBytes = sizeof(MetalNanoVGVertex) * count;
    id<MTLBuffer> vertexBuffer = [backend.device newBufferWithBytes:vertices length:vertexBytes options:MTLResourceStorageModeShared];
    if (!vertexBuffer)
    {
        return;
    }

    [backend.liveBuffers addObject:vertexBuffer];
    [backend.encoder setVertexBuffer:vertexBuffer offset:0 atIndex:0];
    [backend.encoder setVertexBytes:backend.viewSize length:sizeof(backend.viewSize) atIndex:1];
    [backend.encoder drawPrimitives:primitive vertexStart:0 vertexCount:count];
}

void draw_vertices(MetalNanoVGBackend& backend, MTLPrimitiveType primitive, const NVGvertex* vertices, int count, NVGpaint* paint)
{
    if (count <= 0)
    {
        return;
    }

    std::vector<MetalNanoVGVertex> converted;
    converted.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i)
    {
        converted.push_back({ vertices[i].x, vertices[i].y, vertices[i].u, vertices[i].v });
    }
    draw_vertices(backend, primitive, converted.data(), converted.size(), paint);
}

void draw_fan(MetalNanoVGBackend& backend, const NVGvertex* vertices, int count, NVGpaint* paint)
{
    if (count < 3)
    {
        return;
    }

    std::vector<MetalNanoVGVertex> triangles;
    triangles.reserve(static_cast<size_t>((count - 2) * 3));
    for (int i = 1; i < count - 1; ++i)
    {
        triangles.push_back({ vertices[0].x, vertices[0].y, vertices[0].u, vertices[0].v });
        triangles.push_back({ vertices[i].x, vertices[i].y, vertices[i].u, vertices[i].v });
        triangles.push_back({ vertices[i + 1].x, vertices[i + 1].y, vertices[i + 1].u, vertices[i + 1].v });
    }
    draw_vertices(backend, MTLPrimitiveTypeTriangle, triangles.data(), triangles.size(), paint);
}

const char* shader_source()
{
    return R"(
#include <metal_stdlib>
using namespace metal;

struct NanoVGVertexIn
{
    float2 position [[attribute(0)]];
    float2 uv [[attribute(1)]];
};

struct NanoVGVertexOut
{
    float4 position [[position]];
    float2 uv;
};

vertex NanoVGVertexOut vklive_nanovg_vertex(NanoVGVertexIn in [[stage_in]], constant float2& viewSize [[buffer(1)]])
{
    NanoVGVertexOut out;
    float2 clip = float2((in.position.x / viewSize.x) * 2.0 - 1.0, 1.0 - (in.position.y / viewSize.y) * 2.0);
    out.position = float4(clip, 0.0, 1.0);
    out.uv = in.uv;
    return out;
}

fragment float4 vklive_nanovg_fragment(NanoVGVertexOut in [[stage_in]],
                                       constant float4& color [[buffer(0)]],
                                       constant uint& useTexture [[buffer(1)]],
                                       texture2d<float> image [[texture(0)]],
                                       sampler imageSampler [[sampler(0)]])
{
    if (useTexture == 1)
    {
        return float4(color.rgb, color.a * image.sample(imageSampler, in.uv).r);
    }
    if (useTexture == 2)
    {
        return color * image.sample(imageSampler, in.uv);
    }
    return color;
}
)";
}

int render_create(void* uptr)
{
    auto* backend = static_cast<MetalNanoVGBackend*>(uptr);
    if (!backend || !backend->device)
    {
        return 0;
    }

    NSError* error = nil;
    NSString* source = [NSString stringWithUTF8String:shader_source()];
    backend->library = [backend->device newLibraryWithSource:source options:nil error:&error];
    if (!backend->library)
    {
        return 0;
    }

    backend->vertexFunction = [backend->library newFunctionWithName:@"vklive_nanovg_vertex"];
    backend->fragmentFunction = [backend->library newFunctionWithName:@"vklive_nanovg_fragment"];
    if (!backend->vertexFunction || !backend->fragmentFunction)
    {
        return 0;
    }

    MTLSamplerDescriptor* samplerDescriptor = [[MTLSamplerDescriptor alloc] init];
    samplerDescriptor.minFilter = MTLSamplerMinMagFilterLinear;
    samplerDescriptor.magFilter = MTLSamplerMinMagFilterLinear;
    samplerDescriptor.sAddressMode = MTLSamplerAddressModeClampToEdge;
    samplerDescriptor.tAddressMode = MTLSamplerAddressModeClampToEdge;
    backend->sampler = [backend->device newSamplerStateWithDescriptor:samplerDescriptor];
    if (!backend->sampler)
    {
        return 0;
    }

    MTLTextureDescriptor* dummyDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm width:1 height:1 mipmapped:NO];
    dummyDescriptor.usage = MTLTextureUsageShaderRead;
    backend->dummyTexture = [backend->device newTextureWithDescriptor:dummyDescriptor];
    unsigned char white = 255;
    [backend->dummyTexture replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 withBytes:&white bytesPerRow:1];
    return backend->dummyTexture ? 1 : 0;
}

int render_create_texture(void* uptr, int type, int width, int height, int imageFlags, const unsigned char* data)
{
    auto* backend = static_cast<MetalNanoVGBackend*>(uptr);
    if (!backend || !backend->device || width <= 0 || height <= 0)
    {
        return 0;
    }

    const auto pixelFormat = type == NVG_TEXTURE_ALPHA ? MTLPixelFormatR8Unorm : MTLPixelFormatRGBA8Unorm;
    MTLTextureDescriptor* descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:pixelFormat width:static_cast<NSUInteger>(width) height:static_cast<NSUInteger>(height) mipmapped:NO];
    descriptor.usage = MTLTextureUsageShaderRead;
    auto texture = [backend->device newTextureWithDescriptor:descriptor];
    if (!texture)
    {
        return 0;
    }

    const int bytesPerPixel = type == NVG_TEXTURE_ALPHA ? 1 : 4;
    if (data)
    {
        [texture replaceRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0 withBytes:data bytesPerRow:width * bytesPerPixel];
    }

    MetalNanoVGTexture metalTexture;
    metalTexture.handle = backend->nextTextureId++;
    metalTexture.type = type;
    metalTexture.width = width;
    metalTexture.height = height;
    metalTexture.flags = imageFlags;
    metalTexture.texture = texture;
    backend->textures.push_back(metalTexture);
    return metalTexture.handle;
}

int render_delete_texture(void* uptr, int image)
{
    auto* backend = static_cast<MetalNanoVGBackend*>(uptr);
    if (!backend)
    {
        return 0;
    }

    auto oldSize = backend->textures.size();
    backend->textures.erase(std::remove_if(backend->textures.begin(), backend->textures.end(), [&](const auto& texture) {
                                return texture.handle == image;
                            }),
        backend->textures.end());
    return oldSize != backend->textures.size() ? 1 : 0;
}

int render_update_texture(void* uptr, int image, int x, int y, int width, int height, const unsigned char* data)
{
    auto* backend = static_cast<MetalNanoVGBackend*>(uptr);
    auto* texture = backend ? find_texture(*backend, image) : nullptr;
    if (!texture || !texture->texture || !data)
    {
        return 0;
    }

    const int bytesPerPixel = texture->type == NVG_TEXTURE_ALPHA ? 1 : 4;
    const unsigned char* source = data + (y * texture->width + x) * bytesPerPixel;
    [texture->texture replaceRegion:MTLRegionMake2D(x, y, width, height) mipmapLevel:0 withBytes:source bytesPerRow:texture->width * bytesPerPixel];
    return 1;
}

int render_get_texture_size(void* uptr, int image, int* width, int* height)
{
    auto* backend = static_cast<MetalNanoVGBackend*>(uptr);
    auto* texture = backend ? find_texture(*backend, image) : nullptr;
    if (!texture)
    {
        return 0;
    }
    *width = texture->width;
    *height = texture->height;
    return 1;
}

void render_viewport(void* uptr, float width, float height, float devicePixelRatio)
{
    (void)devicePixelRatio;
    auto* backend = static_cast<MetalNanoVGBackend*>(uptr);
    if (!backend)
    {
        return;
    }
    backend->viewSize[0] = std::max(width, 1.0f);
    backend->viewSize[1] = std::max(height, 1.0f);
}

void render_cancel(void* uptr)
{
    (void)uptr;
}

void render_flush(void* uptr)
{
    (void)uptr;
}

void render_fill(void* uptr, NVGpaint* paint, NVGcompositeOperationState compositeOperation, NVGscissor* scissor, float fringe, const float* bounds, const NVGpath* paths, int npaths)
{
    (void)compositeOperation;
    (void)scissor;
    (void)fringe;
    (void)bounds;
    auto* backend = static_cast<MetalNanoVGBackend*>(uptr);
    if (!backend)
    {
        return;
    }
    for (int i = 0; i < npaths; ++i)
    {
        draw_fan(*backend, paths[i].fill, paths[i].nfill, paint);
        draw_vertices(*backend, MTLPrimitiveTypeTriangleStrip, paths[i].stroke, paths[i].nstroke, paint);
    }
}

void render_stroke(void* uptr, NVGpaint* paint, NVGcompositeOperationState compositeOperation, NVGscissor* scissor, float fringe, float strokeWidth, const NVGpath* paths, int npaths)
{
    (void)compositeOperation;
    (void)scissor;
    (void)fringe;
    (void)strokeWidth;
    auto* backend = static_cast<MetalNanoVGBackend*>(uptr);
    if (!backend)
    {
        return;
    }
    for (int i = 0; i < npaths; ++i)
    {
        draw_vertices(*backend, MTLPrimitiveTypeTriangleStrip, paths[i].stroke, paths[i].nstroke, paint);
    }
}

void render_triangles(void* uptr, NVGpaint* paint, NVGcompositeOperationState compositeOperation, NVGscissor* scissor, const NVGvertex* vertices, int nverts, float fringe)
{
    (void)compositeOperation;
    (void)scissor;
    (void)fringe;
    auto* backend = static_cast<MetalNanoVGBackend*>(uptr);
    if (!backend)
    {
        return;
    }
    draw_vertices(*backend, MTLPrimitiveTypeTriangle, vertices, nverts, paint);
}

void render_delete(void* uptr)
{
    auto* backend = static_cast<MetalNanoVGBackend*>(uptr);
    if (backend)
    {
        backend->textures.clear();
    }
}

bool ensure_pipeline(MetalNanoVGBackend& backend, metal::MetalPass& pass, const metal::MetalPassTargets& targets)
{
    if (targets.colors.empty() || targets.colorFormats.empty() || targets.colorFormats.size() > kMetalNanoVGMaxColorAttachments)
    {
        report_nanovg_error(pass, fmt::format("Metal scripted pass '{}' has invalid color targets for NanoVG.", pass.pass.name));
        return false;
    }

    if (backend.pipelineState && backend.colorFormats == targets.colorFormats && backend.depthFormat == targets.depthFormat)
    {
        return true;
    }

    MTLVertexDescriptor* vertexDescriptor = [MTLVertexDescriptor vertexDescriptor];
    vertexDescriptor.layouts[0].stride = sizeof(MetalNanoVGVertex);
    vertexDescriptor.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
    vertexDescriptor.attributes[0].format = MTLVertexFormatFloat2;
    vertexDescriptor.attributes[0].offset = 0;
    vertexDescriptor.attributes[0].bufferIndex = 0;
    vertexDescriptor.attributes[1].format = MTLVertexFormatFloat2;
    vertexDescriptor.attributes[1].offset = sizeof(float) * 2;
    vertexDescriptor.attributes[1].bufferIndex = 0;

    MTLRenderPipelineDescriptor* descriptor = [[MTLRenderPipelineDescriptor alloc] init];
    descriptor.label = ns_string(fmt::format("MetalNanoVG:{}", pass.pass.name));
    descriptor.vertexFunction = backend.vertexFunction;
    descriptor.fragmentFunction = backend.fragmentFunction;
    descriptor.vertexDescriptor = vertexDescriptor;
    descriptor.inputPrimitiveTopology = MTLPrimitiveTopologyClassTriangle;
    for (NSUInteger i = 0; i < targets.colorFormats.size(); ++i)
    {
        descriptor.colorAttachments[i].pixelFormat = static_cast<MTLPixelFormat>(targets.colorFormats[i]);
        descriptor.colorAttachments[i].blendingEnabled = YES;
        descriptor.colorAttachments[i].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
        descriptor.colorAttachments[i].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        descriptor.colorAttachments[i].rgbBlendOperation = MTLBlendOperationAdd;
        descriptor.colorAttachments[i].sourceAlphaBlendFactor = MTLBlendFactorOne;
        descriptor.colorAttachments[i].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        descriptor.colorAttachments[i].alphaBlendOperation = MTLBlendOperationAdd;
    }
    if (targets.depth)
    {
        descriptor.depthAttachmentPixelFormat = static_cast<MTLPixelFormat>(targets.depthFormat);
    }

    NSError* error = nil;
    backend.pipelineState = [backend.device newRenderPipelineStateWithDescriptor:descriptor error:&error];
    if (!backend.pipelineState)
    {
        report_nanovg_error(pass, fmt::format("Metal scripted pass '{}' could not create NanoVG pipeline: {}", pass.pass.name, ns_string([error localizedDescription])));
        return false;
    }

    backend.colorFormats = targets.colorFormats;
    backend.depthFormat = targets.depthFormat;
    return true;
}

} // namespace

namespace metal
{

bool metal_nanovg_init(MetalContext& ctx)
{
    auto device = bridge<id<MTLDevice>>(ctx.device);
    if (!device)
    {
        return false;
    }

    auto* backend = new MetalNanoVGBackend();
    backend->ctx = &ctx;
    backend->device = device;

    NVGparams params;
    std::memset(&params, 0, sizeof(params));
    params.userPtr = backend;
    params.edgeAntiAlias = 1;
    params.renderCreate = render_create;
    params.renderCreateTexture = render_create_texture;
    params.renderDeleteTexture = render_delete_texture;
    params.renderUpdateTexture = render_update_texture;
    params.renderGetTextureSize = render_get_texture_size;
    params.renderViewport = render_viewport;
    params.renderCancel = render_cancel;
    params.renderFlushNoContext = render_flush;
    params.renderFill = render_fill;
    params.renderStroke = render_stroke;
    params.renderTriangles = render_triangles;
    params.renderDelete = render_delete;

    ctx.metalNanovg = backend;
    ctx.vg = nvgCreateInternal(&params);
    if (!ctx.vg)
    {
        delete backend;
        ctx.metalNanovg = nullptr;
        return false;
    }

    auto fontPath = Zest::runtree_find_path("fonts/Roboto-Regular.ttf");
    ctx.defaultFont = nvgCreateFont(ctx.vg, "sans", fontPath.string().c_str());
    return true;
}

void metal_nanovg_destroy(MetalContext& ctx)
{
    if (ctx.vg)
    {
        nvgDeleteInternal(ctx.vg);
        ctx.vg = nullptr;
    }
    auto* backend = backend_from(ctx);
    delete backend;
    ctx.metalNanovg = nullptr;
}

bool metal_nanovg_begin(MetalContext& ctx, MetalPass& pass, const MetalPassTargets& targets)
{
    auto* backend = backend_from(ctx);
    auto commandQueue = bridge<id<MTLCommandQueue>>(ctx.commandQueue);
    if (!backend || !ctx.vg || !commandQueue)
    {
        report_nanovg_error(pass, fmt::format("Metal scripted pass '{}' cannot start NanoVG because the Metal NanoVG context is unavailable.", pass.pass.name));
        return false;
    }

    if (!ensure_pipeline(*backend, pass, targets))
    {
        return false;
    }

    MTLRenderPassDescriptor* renderPass = [MTLRenderPassDescriptor renderPassDescriptor];
    for (NSUInteger i = 0; i < targets.colors.size(); ++i)
    {
        renderPass.colorAttachments[i].texture = bridge<id<MTLTexture>>(targets.colors[i]->texture);
        renderPass.colorAttachments[i].loadAction = pass.pass.hasClear ? MTLLoadActionClear : MTLLoadActionLoad;
        renderPass.colorAttachments[i].storeAction = MTLStoreActionStore;
        renderPass.colorAttachments[i].clearColor = MTLClearColorMake(pass.pass.clearColor.x, pass.pass.clearColor.y, pass.pass.clearColor.z, pass.pass.clearColor.w);
    }
    if (targets.depth)
    {
        renderPass.depthAttachment.texture = bridge<id<MTLTexture>>(targets.depth->texture);
        renderPass.depthAttachment.loadAction = MTLLoadActionClear;
        renderPass.depthAttachment.storeAction = MTLStoreActionStore;
        renderPass.depthAttachment.clearDepth = 1.0;
    }

    backend->commandBuffer = [commandQueue commandBuffer];
    backend->commandBuffer.label = ns_string(fmt::format("MetalNanoVG:{}", pass.pass.name));
    backend->liveBuffers = [NSMutableArray array];
    backend->encoder = [backend->commandBuffer renderCommandEncoderWithDescriptor:renderPass];
    if (!backend->encoder)
    {
        backend->liveBuffers = nil;
        report_nanovg_error(pass, fmt::format("Metal scripted pass '{}' could not create a NanoVG render encoder.", pass.pass.name));
        return false;
    }

    backend->encoder.label = ns_string(fmt::format("MetalNanoVG:{}:Encoder", pass.pass.name));
    [backend->encoder setRenderPipelineState:backend->pipelineState];
    [backend->encoder setCullMode:MTLCullModeNone];

    MTLViewport viewport{ 0.0, 0.0, static_cast<double>(targets.size.x), static_cast<double>(targets.size.y), 0.0, 1.0 };
    MTLScissorRect scissor{ 0, 0, static_cast<NSUInteger>(targets.size.x), static_cast<NSUInteger>(targets.size.y) };
    [backend->encoder setViewport:viewport];
    [backend->encoder setScissorRect:scissor];

    nvgBeginFrame(ctx.vg, static_cast<float>(targets.size.x), static_cast<float>(targets.size.y), 1.0f);
    return true;
}

bool metal_nanovg_end(MetalContext& ctx, MetalPass& pass)
{
    auto* backend = backend_from(ctx);
    if (!backend || !backend->encoder || !backend->commandBuffer)
    {
        report_nanovg_error(pass, fmt::format("Metal scripted pass '{}' cannot finish NanoVG because no encoder is active.", pass.pass.name));
        return false;
    }

    nvgEndFrame(ctx.vg);
    [backend->encoder endEncoding];
    backend->encoder = nil;

    [backend->commandBuffer commit];
    [backend->commandBuffer waitUntilCompleted];

    const bool failed = backend->commandBuffer.status == MTLCommandBufferStatusError;
    const auto error = backend->commandBuffer.error;
    backend->commandBuffer = nil;
    backend->liveBuffers = nil;
    if (failed)
    {
        report_nanovg_error(pass, fmt::format("Metal scripted pass '{}' NanoVG command buffer failed: {}", pass.pass.name, ns_string([error localizedDescription])));
        return false;
    }
    return true;
}

} // namespace metal
