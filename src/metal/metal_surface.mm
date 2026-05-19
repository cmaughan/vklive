#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <vklive/metal/metal_context.h>
#include <vklive/metal/metal_scene.h>
#include <vklive/metal/metal_surface.h>
#include <vklive/scene.h>

namespace
{

template <typename T>
T bridge(void* object)
{
    return (__bridge T)object;
}

void release_obj(void*& object)
{
    if (object)
    {
        id releasedObject = CFBridgingRelease(object);
        releasedObject = nil;
        object = nullptr;
    }
}

void retain_obj(void*& storage, id object)
{
    release_obj(storage);
    storage = object ? (__bridge_retained void*)object : nullptr;
}

uint64_t frame_to_pingpong(uint64_t frame)
{
    return frame % 2;
}

metal::MetalSurfaceFormat metal_surface_format_from_scene_format(Format format)
{
    switch (format)
    {
    case Format::default_depth_format:
    case Format::d32:
        return metal::MetalSurfaceFormat::Depth32Float;
    case Format::r16g16b16a16_sfloat:
        return metal::MetalSurfaceFormat::RGBA16Float;
    case Format::r32g32b32a32_sfloat:
        return metal::MetalSurfaceFormat::RGBA32Float;
    case Format::default_format:
    case Format::r8g8b8a8_unorm:
        return metal::MetalSurfaceFormat::RGBA8Unorm;
    }
    return metal::MetalSurfaceFormat::RGBA8Unorm;
}

MTLPixelFormat metal_pixel_format(metal::MetalSurfaceFormat format)
{
    switch (format)
    {
    case metal::MetalSurfaceFormat::Depth32Float:
        return MTLPixelFormatDepth32Float;
    case metal::MetalSurfaceFormat::RGBA16Float:
        return MTLPixelFormatRGBA16Float;
    case metal::MetalSurfaceFormat::RGBA32Float:
        return MTLPixelFormatRGBA32Float;
    case metal::MetalSurfaceFormat::RGBA8Unorm:
        return MTLPixelFormatRGBA8Unorm;
    default:
        return MTLPixelFormatInvalid;
    }
}

bool metal_surface_format_is_depth(metal::MetalSurfaceFormat format)
{
    return format == metal::MetalSurfaceFormat::Depth32Float;
}

bool metal_surface_size_valid(const glm::uvec2& size)
{
    return size.x != 0 && size.y != 0;
}

NSString* ns_string(const std::string& string)
{
    return [NSString stringWithUTF8String:string.c_str()];
}

glm::uvec2 metal_surface_target_size(metal::MetalScene& scene, Surface& surface, const glm::uvec2& renderSize)
{
    auto size = surface.size;
    if (size == glm::uvec2(0, 0))
    {
        auto baseSize = renderSize;
        if (scene.pScene)
        {
            auto itrDefaultColor = scene.pScene->surfaces.find("default_color");
            if (itrDefaultColor != scene.pScene->surfaces.end() && itrDefaultColor->second && itrDefaultColor->second->size != glm::uvec2(0, 0))
            {
                baseSize = itrDefaultColor->second->size;
            }
        }
        size = glm::uvec2(surface.scale.x * baseSize.x, surface.scale.y * baseSize.y);
    }
    return size;
}

} // namespace

namespace metal
{

MetalSurfaceKey::MetalSurfaceKey(const std::string& name, uint64_t frameCount, bool sampling)
    : targetName(name)
    , pingPongIndex(frame_to_pingpong(frameCount))
{
    if (sampling)
    {
        pingPongIndex = 1 - pingPongIndex;
    }
}

std::string MetalSurfaceKey::DebugName() const
{
    return targetName + ":P" + std::to_string(pingPongIndex);
}

bool MetalSurfaceKey::operator<(const MetalSurfaceKey& rhs) const
{
    if (targetName < rhs.targetName)
    {
        return true;
    }
    if (targetName > rhs.targetName)
    {
        return false;
    }
    return pingPongIndex < rhs.pingPongIndex;
}

std::shared_ptr<MetalSurface> metal_surface_create(MetalContext& ctx, MetalScene& scene, Surface& surface)
{
    (void)ctx;
    (void)scene;
    auto spSurface = std::make_shared<MetalSurface>(&surface);
    spSurface->key.targetName = surface.name;
    spSurface->debugName = surface.name;
    return spSurface;
}

void metal_surface_destroy(MetalContext& ctx, MetalSurface& surface)
{
    (void)ctx;
    release_obj(surface.sampler);
    release_obj(surface.texture);
    surface.size = glm::uvec2(0);
    surface.format = MetalSurfaceFormat::Unknown;
}

void metal_surface_create_target(MetalContext& ctx, MetalSurface& surface, const glm::uvec2& size, MetalSurfaceFormat format)
{
    metal_surface_destroy(ctx, surface);

    if (!metal_surface_size_valid(size) || format == MetalSurfaceFormat::Unknown)
    {
        return;
    }

    auto device = bridge<id<MTLDevice>>(ctx.device);
    if (!device)
    {
        return;
    }

    MTLPixelFormat pixelFormat = metal_pixel_format(format);
    if (pixelFormat == MTLPixelFormatInvalid)
    {
        return;
    }

    MTLTextureDescriptor* descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:pixelFormat
                                                                                           width:size.x
                                                                                          height:size.y
                                                                                       mipmapped:NO];
    descriptor.storageMode = MTLStorageModePrivate;
    MTLTextureUsage usage = MTLTextureUsageRenderTarget;
    if (!metal_surface_format_is_depth(format))
    {
        usage |= MTLTextureUsageShaderRead;
        if (surface.pSurface && surface.pSurface->isRayTarget)
        {
            usage |= MTLTextureUsageShaderWrite;
        }
    }
    descriptor.usage = usage;

    id<MTLTexture> texture = [device newTextureWithDescriptor:descriptor];
    if (!texture)
    {
        return;
    }

    texture.label = ns_string(surface.debugName);
    retain_obj(surface.texture, texture);
    surface.size = size;
    surface.format = format;
    surface.generation++;
}

void metal_surface_create_sampler(MetalContext& ctx, MetalSurface& surface)
{
    if (surface.sampler)
    {
        return;
    }

    auto device = bridge<id<MTLDevice>>(ctx.device);
    if (!device)
    {
        return;
    }

    MTLSamplerDescriptor* descriptor = [[MTLSamplerDescriptor alloc] init];
    descriptor.minFilter = MTLSamplerMinMagFilterLinear;
    descriptor.magFilter = MTLSamplerMinMagFilterLinear;
    descriptor.mipFilter = MTLSamplerMipFilterLinear;
    descriptor.sAddressMode = MTLSamplerAddressModeRepeat;
    descriptor.tAddressMode = MTLSamplerAddressModeRepeat;
    descriptor.rAddressMode = MTLSamplerAddressModeRepeat;
    descriptor.label = ns_string(surface.debugName + ":Sampler");

    id<MTLSamplerState> sampler = [device newSamplerStateWithDescriptor:descriptor];
    retain_obj(surface.sampler, sampler);
}

bool metal_surface_ensure_target(MetalContext& ctx, MetalScene& scene, MetalSurface& surface, const glm::uvec2& renderSize)
{
    if (!surface.pSurface)
    {
        return false;
    }

    const auto size = metal_surface_target_size(scene, *surface.pSurface, renderSize);
    const auto format = metal_surface_format_from_scene_format(surface.pSurface->format);
    if (size == surface.pSurface->currentSize && size == surface.size && format == surface.format)
    {
        return surface.texture != nullptr;
    }

    context_wait_idle(ctx);
    metal_surface_destroy(ctx, surface);

    surface.pSurface->currentSize = size;
    if (metal_surface_size_valid(size))
    {
        metal_surface_create_target(ctx, surface, size, format);
    }

    surface.pSurface->rendered = false;
    return surface.texture != nullptr;
}

} // namespace metal
