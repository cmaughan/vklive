#pragma once

#include <zest/imgui/imgui.h>

#include <vklive_nvim/highlight.h>
#include <vklive_nvim/render_model.h>
#include <vklive_nvim/text_service.h>

namespace Zest
{
struct IFontTexture;
}

struct NvimGridMetrics
{
    int columns = 1;
    int rows = 1;
    float cell_width = 1.0f;
    float cell_height = 1.0f;
};

NvimGridMetrics nvim_grid_metrics(ImVec2 available, float cell_width, float cell_height);
ImVec2 nvim_glyph_origin(ImVec2 cell_min, const vklive_nvim::FontMetrics& metrics, const vklive_nvim::AtlasRegion& glyph);

class NvimImGuiRenderer
{
public:
    bool ensure_initialized(float display_ppi);
    ImVec2 cell_size() const;
    void draw(const vklive_nvim::RenderModel& model, const vklive_nvim::HighlightTable& highlights, ImVec2 top_left, const NvimGridMetrics& metrics, Zest::IFontTexture* texture);

private:
    void upload_atlas(Zest::IFontTexture& texture);

    vklive_nvim::TextService m_textService;
    Zest::IFontTexture* m_textureOwner = nullptr;
    int m_atlasTexture = 0;
    bool m_initialized = false;
};
