#pragma once

#include <nodegraph/fonts.h>

namespace NodeGraph
{

class FrameNeutralFontTexture : public IFontTexture
{
public:
    explicit FrameNeutralFontTexture(IFontTexture& texture)
        : m_texture(texture)
    {
    }

    int UpdateTexture(int image, int x, int y, int w, int h, const unsigned char* data) override
    {
        return m_texture.UpdateTexture(image, x, y, w, h, data);
    }

    int CreateTexture(int w, int h, const unsigned char* data) override
    {
        return m_texture.CreateTexture(w, h, data);
    }

    void DeleteTexture(int image) override
    {
        m_texture.DeleteTexture(image);
    }

    void GetTextureSize(int image, int* w, int* h) override
    {
        m_texture.GetTextureSize(image, w, h);
    }

    void* GetTexture(int image) override
    {
        return m_texture.GetTexture(image);
    }

    void BeginFrame() override {}
    void EndFrame() override {}

private:
    IFontTexture& m_texture;
};

} // namespace NodeGraph
