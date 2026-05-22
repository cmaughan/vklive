#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <limits>
#include <vector>

#include <gli/gli.hpp>

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <zing/audio/audio_analysis.h>

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
    case metal::MetalSurfaceFormat::RGBA8Unorm_sRGB:
        return MTLPixelFormatRGBA8Unorm_sRGB;
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

std::string lowercase_extension(const fs::path& path)
{
    auto extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return extension;
}

void flip_image_rows(void* data, int width, int height, size_t bytesPerPixel)
{
    if (!data || width <= 0 || height <= 1 || bytesPerPixel == 0)
    {
        return;
    }

    const auto rowSize = static_cast<size_t>(width) * bytesPerPixel;
    auto* bytes = static_cast<uint8_t*>(data);
    std::vector<uint8_t> temp(rowSize);
    for (int y = 0; y < height / 2; ++y)
    {
        auto* top = bytes + (static_cast<size_t>(y) * rowSize);
        auto* bottom = bytes + (static_cast<size_t>(height - 1 - y) * rowSize);
        std::memcpy(temp.data(), top, rowSize);
        std::memcpy(top, bottom, rowSize);
        std::memcpy(bottom, temp.data(), rowSize);
    }
}

bool metal_surface_create_texture(metal::MetalContext& ctx, metal::MetalSurface& surface, const glm::uvec2& size, metal::MetalSurfaceFormat format, uint32_t mipLevels)
{
    metal::metal_surface_destroy(ctx, surface);

    if (!metal_surface_size_valid(size) || format == metal::MetalSurfaceFormat::Unknown || mipLevels == 0)
    {
        surface.allocationState = metal::MetalAllocationState::Failed;
        return false;
    }

    auto device = bridge<id<MTLDevice>>(ctx.device);
    if (!device)
    {
        surface.allocationState = metal::MetalAllocationState::Failed;
        return false;
    }

    MTLPixelFormat pixelFormat = metal_pixel_format(format);
    if (pixelFormat == MTLPixelFormatInvalid)
    {
        surface.allocationState = metal::MetalAllocationState::Failed;
        return false;
    }

    MTLTextureDescriptor* descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:pixelFormat
                                                                                           width:size.x
                                                                                          height:size.y
                                                                                       mipmapped:mipLevels > 1 ? YES : NO];
    descriptor.mipmapLevelCount = mipLevels;
    descriptor.storageMode = MTLStorageModeShared;
    descriptor.usage = MTLTextureUsageShaderRead;

    id<MTLTexture> texture = [device newTextureWithDescriptor:descriptor];
    if (!texture)
    {
        surface.allocationState = metal::MetalAllocationState::Failed;
        return false;
    }

    texture.label = ns_string(surface.debugName);
    retain_obj(surface.texture, texture);
    surface.size = size;
    surface.format = format;
    surface.mipLevels = mipLevels;
    surface.generation++;
    return true;
}

bool metal_surface_upload_mip(metal::MetalSurface& surface, const void* data, uint32_t mipLevel, NSUInteger bytesPerRow)
{
    auto texture = bridge<id<MTLTexture>>(surface.texture);
    if (!texture || !data || bytesPerRow == 0 || mipLevel >= surface.mipLevels)
    {
        return false;
    }

    const NSUInteger width = std::max<NSUInteger>(static_cast<NSUInteger>(1), static_cast<NSUInteger>(surface.size.x >> mipLevel));
    const NSUInteger height = std::max<NSUInteger>(static_cast<NSUInteger>(1), static_cast<NSUInteger>(surface.size.y >> mipLevel));
    MTLRegion region = MTLRegionMake2D(0, 0, width, height);
    [texture replaceRegion:region mipmapLevel:mipLevel withBytes:data bytesPerRow:bytesPerRow];
    return true;
}

bool metal_surface_format_from_gli(gli::format format, metal::MetalSurfaceFormat& surfaceFormat, uint32_t& bytesPerPixel)
{
    switch (format)
    {
    case gli::FORMAT_RGBA8_UNORM_PACK8:
        surfaceFormat = metal::MetalSurfaceFormat::RGBA8Unorm;
        bytesPerPixel = 4;
        return true;
    case gli::FORMAT_RGBA8_SRGB_PACK8:
        surfaceFormat = metal::MetalSurfaceFormat::RGBA8Unorm_sRGB;
        bytesPerPixel = 4;
        return true;
    case gli::FORMAT_RGBA32_SFLOAT_PACK32:
        surfaceFormat = metal::MetalSurfaceFormat::RGBA32Float;
        bytesPerPixel = 16;
        return true;
    default:
        surfaceFormat = metal::MetalSurfaceFormat::Unknown;
        bytesPerPixel = 0;
        return false;
    }
}

bool metal_surface_create_from_gli(metal::MetalContext& ctx, metal::MetalSurface& surface, const fs::path& sourceName, const char* data, size_t dataSize)
{
    (void)sourceName;
    auto texture = gli::load(data, dataSize);
    if (texture.empty())
    {
        surface.allocationState = metal::MetalAllocationState::Failed;
        return false;
    }

    gli::texture2d texture2D(texture);
    if (texture2D.empty())
    {
        surface.allocationState = metal::MetalAllocationState::Failed;
        return false;
    }

    metal::MetalSurfaceFormat format = metal::MetalSurfaceFormat::Unknown;
    uint32_t bytesPerPixel = 0;
    if (!metal_surface_format_from_gli(texture2D.format(), format, bytesPerPixel))
    {
        surface.allocationState = metal::MetalAllocationState::Failed;
        return false;
    }

    const auto extent = texture2D.extent();
    const auto mipLevels = static_cast<uint32_t>(texture2D.levels());
    if (!metal_surface_create_texture(ctx, surface, glm::uvec2(extent.x, extent.y), format, mipLevels))
    {
        return false;
    }

    for (uint32_t mip = 0; mip < mipLevels; ++mip)
    {
        const auto mipExtent = texture2D[mip].extent();
        const auto bytesPerRow = static_cast<NSUInteger>(mipExtent.x) * bytesPerPixel;
        if (!metal_surface_upload_mip(surface, texture2D[mip].data(), mip, bytesPerRow))
        {
            surface.allocationState = metal::MetalAllocationState::Failed;
            return false;
        }
    }

    metal::metal_surface_create_sampler(ctx, surface);
    surface.allocationState = metal::MetalAllocationState::Loaded;
    return true;
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
    , pingPongIndex(0)
{
    // Render target writes stay on a stable key until Metal sampler ping-pong is implemented.
    if (sampling)
    {
        pingPongIndex = 1 - frame_to_pingpong(frameCount);
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
    release_obj(surface.stagingBuffer);
    release_obj(surface.sampler);
    release_obj(surface.texture);
    surface.size = glm::uvec2(0);
    surface.format = MetalSurfaceFormat::Unknown;
    surface.mipLevels = 1;
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
    surface.allocationState = MetalAllocationState::Loaded;
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
    descriptor.mipFilter = surface.mipLevels > 1 ? MTLSamplerMipFilterLinear : MTLSamplerMipFilterNotMipmapped;
    descriptor.sAddressMode = MTLSamplerAddressModeRepeat;
    descriptor.tAddressMode = MTLSamplerAddressModeRepeat;
    descriptor.rAddressMode = MTLSamplerAddressModeRepeat;
    descriptor.label = ns_string(surface.debugName + ":Sampler");

    id<MTLSamplerState> sampler = [device newSamplerStateWithDescriptor:descriptor];
    retain_obj(surface.sampler, sampler);
}

bool metal_surface_create_from_memory(MetalContext& ctx, MetalSurface& surface, const fs::path& sourceName, const char* data, size_t dataSize, bool flipY, MetalSurfaceFormat ldrFormat)
{
    metal_surface_destroy(ctx, surface);

    if (!data || dataSize == 0 || dataSize > static_cast<size_t>(std::numeric_limits<int>::max()))
    {
        surface.allocationState = MetalAllocationState::Failed;
        return false;
    }

    const auto extension = lowercase_extension(sourceName);
    if (extension == ".dds" || extension == ".ktx")
    {
        return metal_surface_create_from_gli(ctx, surface, sourceName, data, dataSize);
    }

    const auto* imageData = reinterpret_cast<const stbi_uc*>(data);
    const auto imageDataSize = static_cast<int>(dataSize);
    const auto isHdr = stbi_is_hdr_from_memory(imageData, imageDataSize) != 0;

    int width = 0;
    int height = 0;
    int components = 0;
    void* loaded = nullptr;
    size_t bytesPerPixel = 0;
    MetalSurfaceFormat format = MetalSurfaceFormat::RGBA8Unorm;

    if (isHdr)
    {
        auto* loadedHdr = stbi_loadf_from_memory(imageData, imageDataSize, &width, &height, &components, STBI_rgb_alpha);
        if (!loadedHdr || width <= 0 || height <= 0)
        {
            stbi_image_free(loadedHdr);
            surface.allocationState = MetalAllocationState::Failed;
            return false;
        }

        loaded = loadedHdr;
        bytesPerPixel = 4 * sizeof(float);
        format = MetalSurfaceFormat::RGBA32Float;
    }
    else
    {
        auto* loadedLdr = stbi_load_from_memory(imageData, imageDataSize, &width, &height, &components, STBI_rgb_alpha);
        if (!loadedLdr || width <= 0 || height <= 0)
        {
            stbi_image_free(loadedLdr);
            surface.allocationState = MetalAllocationState::Failed;
            return false;
        }

        loaded = loadedLdr;
        bytesPerPixel = 4;
        format = ldrFormat == MetalSurfaceFormat::RGBA8Unorm_sRGB ? MetalSurfaceFormat::RGBA8Unorm_sRGB : MetalSurfaceFormat::RGBA8Unorm;
    }

    if (flipY)
    {
        flip_image_rows(loaded, width, height, bytesPerPixel);
    }

    const bool created = metal_surface_create_texture(ctx, surface, glm::uvec2(static_cast<uint32_t>(width), static_cast<uint32_t>(height)), format, 1);
    if (created)
    {
        const auto bytesPerRow = static_cast<NSUInteger>(width) * bytesPerPixel;
        if (!metal_surface_upload_mip(surface, loaded, 0, bytesPerRow))
        {
            surface.allocationState = MetalAllocationState::Failed;
            stbi_image_free(loaded);
            return false;
        }
    }

    stbi_image_free(loaded);
    if (!created)
    {
        return false;
    }

    metal_surface_create_sampler(ctx, surface);
    surface.allocationState = MetalAllocationState::Loaded;
    return true;
}

bool metal_surface_create_from_file(MetalContext& ctx, MetalSurface& surface, const fs::path& path, bool flipY, MetalSurfaceFormat ldrFormat)
{
    if (!fs::exists(path))
    {
        surface.allocationState = MetalAllocationState::Failed;
        return false;
    }

    auto data = Zest::file_read(path);
    if (data.empty())
    {
        surface.allocationState = MetalAllocationState::Failed;
        return false;
    }

    return metal_surface_create_from_memory(ctx, surface, path, data.c_str(), data.size(), flipY, ldrFormat);
}

bool metal_surface_update_from_audio(MetalContext& ctx, MetalSurface& surface)
{
    auto& audioCtx = Zing::GetAudioContext();

    size_t bufferWidth = 512;
    size_t channels = std::max(audioCtx.analysisChannels.size(), size_t(1));
    const auto bufferTypes = size_t(2);

    for (auto [Id, pAnalysis] : audioCtx.analysisChannels)
    {
        (void)Id;
        std::shared_ptr<Zing::AudioAnalysisData> spNewData;
        while (pAnalysis->analysisData.try_dequeue(spNewData))
        {
            if (pAnalysis->uiDataCache)
            {
                pAnalysis->analysisDataCache.enqueue(pAnalysis->uiDataCache);
            }
            pAnalysis->uiDataCache = spNewData;
        }

        channels = std::max(channels, static_cast<size_t>(pAnalysis->thisChannel.second) + 1);
        if (pAnalysis->uiDataCache && !pAnalysis->uiDataCache->spectrumBuckets.empty())
        {
            bufferWidth = std::max(bufferWidth, pAnalysis->uiDataCache->spectrumBuckets.size());
        }
    }

    const auto bufferHeight = channels * bufferTypes;
    static std::vector<float> uploadCache;
    uploadCache.assign(bufferWidth * bufferHeight * 4, 0.0f);

    for (auto [Id, pAnalysis] : audioCtx.analysisChannels)
    {
        (void)Id;
        if (!pAnalysis->uiDataCache)
        {
            continue;
        }

        const auto& spectrumBuckets = pAnalysis->uiDataCache->spectrumBuckets;
        const auto& audio = pAnalysis->uiDataCache->audio;
        if (spectrumBuckets.empty())
        {
            continue;
        }

        const auto channel = pAnalysis->thisChannel.second;
        if (channel >= channels)
        {
            continue;
        }

        const auto spectrumCount = std::min(spectrumBuckets.size(), bufferWidth);
        for (size_t i = 0; i < spectrumCount; ++i)
        {
            uploadCache[((channel * bufferWidth) + i) * 4] = spectrumBuckets[i];
        }

        const auto audioCount = std::min(audio.size(), bufferWidth);
        if (audioCount != 0)
        {
            const auto audioRow = channel + channels;
            for (size_t i = 0; i < audioCount; ++i)
            {
                uploadCache[((audioRow * bufferWidth) + i) * 4] = audio[i];
            }
        }
    }

    const auto size = glm::uvec2(static_cast<uint32_t>(bufferWidth), static_cast<uint32_t>(bufferHeight));
    if (surface.size != size || surface.format != MetalSurfaceFormat::RGBA32Float || !surface.texture)
    {
        if (!metal_surface_create_texture(ctx, surface, size, MetalSurfaceFormat::RGBA32Float, 1))
        {
            surface.allocationState = MetalAllocationState::Failed;
            return false;
        }
    }

    if (!metal_surface_upload_mip(surface, uploadCache.data(), 0, static_cast<NSUInteger>(bufferWidth * 4 * sizeof(float))))
    {
        surface.allocationState = MetalAllocationState::Failed;
        return false;
    }

    surface.isAudioSurface = true;
    metal_surface_create_sampler(ctx, surface);
    surface.allocationState = MetalAllocationState::Loaded;
    return true;
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
