#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <vector>

#include <zest/ui/fonts.h>

namespace metal
{

struct MetalContext;

class MetalImGuiTexture : public Zest::IFontTexture
{
public:
    explicit MetalImGuiTexture(MetalContext& ctx);
    ~MetalImGuiTexture();

    virtual int UpdateTexture(int image, int x, int y, int w, int h, const unsigned char* data) override;
    virtual int CreateTexture(int w, int h, const unsigned char* data) override;
    virtual int UpdateTextureRGBA(int image, int x, int y, int w, int h, const unsigned char* data) override;
    virtual int CreateTextureRGBA(int w, int h, const unsigned char* data) override;
    virtual void DeleteTexture(int image) override;
    virtual void GetTextureSize(int image, int* w, int* h) override;
    virtual void* GetTexture(int image) override;
    virtual void BeginFrame() override;
    virtual void EndFrame() override;

private:
    struct FontInfo
    {
        void* texture = nullptr;
        int width = 0;
        int height = 0;
        int textureId = 0;
    };

    void destroy_texture(FontInfo& fontInfo);

    MetalContext& m_ctx;
    int m_currentTextureId = 1;
    uint64_t m_frameIndex = 0;
    std::map<int, std::shared_ptr<FontInfo>> m_mapFonts;
    std::map<uint64_t, std::vector<std::shared_ptr<FontInfo>>> m_mapOldFonts;
};

} // namespace metal
