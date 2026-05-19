#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <TargetConditionals.h>

#include <algorithm>
#include <cassert>

#include <vklive/metal/metal_context.h>
#include <vklive/metal/metal_imgui_texture.h>

namespace
{

void release_obj(void*& object)
{
    if (object)
    {
        id releasedObject = CFBridgingRelease(object);
        releasedObject = nil;
        object = nullptr;
    }
}

template <typename T>
T bridge(void* object)
{
    return (__bridge T)object;
}

} // namespace

namespace metal
{

MetalImGuiTexture::MetalImGuiTexture(MetalContext& ctx)
    : m_ctx(ctx)
{
}

MetalImGuiTexture::~MetalImGuiTexture()
{
    for (auto& [id, fontInfo] : m_mapFonts)
    {
        destroy_texture(*fontInfo);
    }
    for (auto& [frame, oldFonts] : m_mapOldFonts)
    {
        for (auto& fontInfo : oldFonts)
        {
            destroy_texture(*fontInfo);
        }
    }
}

void MetalImGuiTexture::destroy_texture(FontInfo& fontInfo)
{
    release_obj(fontInfo.texture);
    fontInfo.width = 0;
    fontInfo.height = 0;
}

int MetalImGuiTexture::CreateTexture(int width, int height, const unsigned char* data)
{
    auto device = bridge<id<MTLDevice>>(m_ctx.device);
    if (!device || width <= 0 || height <= 0)
    {
        return 0;
    }

    MTLTextureDescriptor* descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                          width:(NSUInteger)width
                                                                                         height:(NSUInteger)height
                                                                                      mipmapped:NO];
    descriptor.usage = MTLTextureUsageShaderRead;
#if TARGET_OS_OSX
    descriptor.storageMode = MTLStorageModeManaged;
#else
    descriptor.storageMode = MTLStorageModeShared;
#endif

    id<MTLTexture> texture = [device newTextureWithDescriptor:descriptor];
    if (!texture)
    {
        return 0;
    }

    auto spFontInfo = std::make_shared<FontInfo>();
    spFontInfo->texture = (__bridge_retained void*)texture;
    spFontInfo->width = width;
    spFontInfo->height = height;
    spFontInfo->textureId = m_currentTextureId++;
    m_mapFonts[spFontInfo->textureId] = spFontInfo;

    if (data)
    {
        UpdateTexture(spFontInfo->textureId, 0, 0, width, height, data);
    }

    return spFontInfo->textureId;
}

int MetalImGuiTexture::UpdateTexture(int image, int x, int y, int updateWidth, int updateHeight, const unsigned char* data)
{
    auto itr = m_mapFonts.find(image);
    if (itr == m_mapFonts.end() || !data)
    {
        return 0;
    }

    auto& fontInfo = *itr->second;
    auto texture = bridge<id<MTLTexture>>(fontInfo.texture);
    if (!texture)
    {
        return 0;
    }

    x = std::max(x, 0);
    y = std::max(y, 0);
    updateWidth = std::min(updateWidth, fontInfo.width - x);
    updateHeight = std::min(updateHeight, fontInfo.height - y);
    if (updateWidth <= 0 || updateHeight <= 0)
    {
        return image;
    }

    std::vector<uint32_t> rgba(size_t(updateWidth) * size_t(updateHeight));
    for (int yy = 0; yy < updateHeight; yy++)
    {
        for (int xx = 0; xx < updateWidth; xx++)
        {
            auto value = uint32_t(data[size_t(y + yy) * size_t(fontInfo.width) + size_t(x + xx)]);
            rgba[size_t(yy) * size_t(updateWidth) + size_t(xx)] = value | (value << 8) | (value << 16) | (value << 24);
        }
    }

    [texture replaceRegion:MTLRegionMake2D((NSUInteger)x, (NSUInteger)y, (NSUInteger)updateWidth, (NSUInteger)updateHeight)
               mipmapLevel:0
                 withBytes:rgba.data()
               bytesPerRow:(NSUInteger)updateWidth * sizeof(uint32_t)];

    return image;
}

void MetalImGuiTexture::DeleteTexture(int image)
{
    auto itr = m_mapFonts.find(image);
    if (itr == m_mapFonts.end())
    {
        assert(!"Texture not found?");
        return;
    }

    m_mapOldFonts[m_frameIndex].push_back(itr->second);
    m_mapFonts.erase(itr);
}

void MetalImGuiTexture::GetTextureSize(int image, int* w, int* h)
{
    if (!w || !h)
    {
        assert(!"Must request dimensions");
        return;
    }

    *w = *h = 0;
    auto itr = m_mapFonts.find(image);
    if (itr == m_mapFonts.end())
    {
        return;
    }

    *w = itr->second->width;
    *h = itr->second->height;
}

void* MetalImGuiTexture::GetTexture(int image)
{
    auto itr = m_mapFonts.find(image);
    if (itr == m_mapFonts.end())
    {
        return nullptr;
    }
    return itr->second->texture;
}

void MetalImGuiTexture::BeginFrame()
{
    std::vector<uint64_t> victims;
    for (auto& [frameIndex, fonts] : m_mapOldFonts)
    {
        if ((frameIndex + 3) >= m_frameIndex)
        {
            continue;
        }

        victims.push_back(frameIndex);
        for (auto& fontInfo : fonts)
        {
            destroy_texture(*fontInfo);
        }
    }
    for (auto& victim : victims)
    {
        m_mapOldFonts.erase(victim);
    }
}

void MetalImGuiTexture::EndFrame()
{
    m_frameIndex++;
}

} // namespace metal
