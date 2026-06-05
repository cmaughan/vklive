#include <app/editor_nvim_renderer.h>

#include <vklive_nvim/atlas_grid.h>

#include <zest/file/runtree.h>
#include <zest/ui/fonts.h>

#include <algorithm>
#include <cmath>
#include <filesystem>

namespace
{

constexpr float kNvimLogicalDisplayPpi = 96.0f;
constexpr float kNvimPointSize = 15.0f;

ImU32 color_to_imgui(vklive_nvim::Color color)
{
    auto channel = [](float value) {
        return static_cast<int>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
    };
    return IM_COL32(channel(color.r), channel(color.g), channel(color.b), channel(color.a));
}

vklive_nvim::TextServiceConfig nvim_text_service_config()
{
    vklive_nvim::TextServiceConfig config;
    const std::filesystem::path regular = Zest::runtree_find_path("fonts/JetBrainsMonoNerdFont-Regular.ttf");
    const std::filesystem::path bold = Zest::runtree_find_path("fonts/JetBrainsMonoNerdFont-Bold.ttf");
    const std::filesystem::path italic = Zest::runtree_find_path("fonts/JetBrainsMonoNerdFont-Italic.ttf");
    const std::filesystem::path boldItalic = Zest::runtree_find_path("fonts/JetBrainsMonoNerdFont-BoldItalic.ttf");

    config.font_path = regular.string();
    config.bold_font_path = std::filesystem::exists(bold) ? bold.string() : config.font_path;
    config.italic_font_path = std::filesystem::exists(italic) ? italic.string() : config.font_path;
    config.bold_italic_font_path = std::filesystem::exists(boldItalic) ? boldItalic.string() : config.font_path;
    config.enable_ligatures = true;
    return config;
}

} // namespace

NvimGridMetrics nvim_grid_metrics(ImVec2 available, float cell_width, float cell_height)
{
    NvimGridMetrics metrics;
    metrics.cell_width = std::max(1.0f, cell_width);
    metrics.cell_height = std::max(1.0f, cell_height);
    metrics.columns = std::max(1, static_cast<int>(std::floor(std::max(0.0f, available.x) / metrics.cell_width)));
    metrics.rows = std::max(1, static_cast<int>(std::floor(std::max(0.0f, available.y) / metrics.cell_height)));
    return metrics;
}

ImVec2 nvim_glyph_origin(ImVec2 cell_min, const vklive_nvim::FontMetrics& metrics, const vklive_nvim::AtlasRegion& glyph)
{
    return ImVec2(
        cell_min.x + static_cast<float>(glyph.bitmap_bearing.x),
        cell_min.y + static_cast<float>(metrics.ascender - glyph.bitmap_bearing.y));
}

bool NvimImGuiRenderer::ensure_initialized(float display_ppi)
{
    // ImGui draw-list coordinates are logical UI units; backend scaling handles device DPI.
    (void)display_ppi;
    if (m_initialized)
    {
        return true;
    }

    m_initialized = m_textService.initialize(nvim_text_service_config(), kNvimPointSize, kNvimLogicalDisplayPpi);
    return m_initialized;
}

ImVec2 NvimImGuiRenderer::cell_size() const
{
    if (m_initialized)
    {
        const auto& metrics = m_textService.metrics();
        return ImVec2(static_cast<float>(std::max(1, metrics.cell_width)), static_cast<float>(std::max(1, metrics.cell_height)));
    }

    return ImVec2(
        std::max(1.0f, ImGui::CalcTextSize("M").x),
        std::max(1.0f, ImGui::GetTextLineHeightWithSpacing()));
}

void NvimImGuiRenderer::upload_atlas(Zest::IFontTexture& texture)
{
    if (!m_initialized || !m_textService.atlas_data())
    {
        return;
    }

    if (m_textureOwner != &texture)
    {
        m_textureOwner = &texture;
        m_atlasTexture = 0;
    }

    const int atlasWidth = m_textService.atlas_width();
    const int atlasHeight = m_textService.atlas_height();
    if (m_atlasTexture == 0)
    {
        m_atlasTexture = texture.CreateTextureRGBA(atlasWidth, atlasHeight, m_textService.atlas_data());
        m_textService.clear_atlas_dirty();
        return;
    }

    const bool reset = m_textService.consume_atlas_reset();
    if (reset)
    {
        texture.UpdateTextureRGBA(m_atlasTexture, 0, 0, atlasWidth, atlasHeight, m_textService.atlas_data());
        m_textService.clear_atlas_dirty();
        return;
    }

    if (!m_textService.atlas_dirty())
    {
        return;
    }

    const auto dirty = m_textService.atlas_dirty_rect();
    if (dirty.size.x > 0 && dirty.size.y > 0)
    {
        texture.UpdateTextureRGBA(m_atlasTexture, dirty.pos.x, dirty.pos.y, dirty.size.x, dirty.size.y, m_textService.atlas_data());
    }
    m_textService.clear_atlas_dirty();
}

void NvimImGuiRenderer::draw(const vklive_nvim::RenderModel& model, const vklive_nvim::HighlightTable& highlights, ImVec2 top_left, const NvimGridMetrics& metrics, Zest::IFontTexture* texture)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    if (!drawList)
    {
        return;
    }

    const ImVec2 bottomRight(
        top_left.x + metrics.columns * metrics.cell_width,
        top_left.y + metrics.rows * metrics.cell_height);
    drawList->AddRectFilled(top_left, bottomRight, color_to_imgui(highlights.default_bg()));

    if (!texture || !m_initialized)
    {
        return;
    }

    const auto cells = vklive_nvim::build_atlas_grid_cells(model, m_textService, highlights);
    upload_atlas(*texture);

    void* atlasTexture = m_atlasTexture != 0 ? texture->GetTexture(m_atlasTexture) : nullptr;
    for (const auto& cell : cells)
    {
        if (cell.column < 0 || cell.column >= metrics.columns || cell.row < 0 || cell.row >= metrics.rows)
        {
            continue;
        }

        const ImVec2 cellMin(
            top_left.x + cell.column * metrics.cell_width,
            top_left.y + cell.row * metrics.cell_height);
        const ImVec2 cellMax(
            cellMin.x + metrics.cell_width,
            cellMin.y + metrics.cell_height);
        drawList->AddRectFilled(cellMin, cellMax, color_to_imgui(cell.background));
    }

    for (const auto& cell : cells)
    {
        if (cell.column < 0 || cell.column >= metrics.columns || cell.row < 0 || cell.row >= metrics.rows)
        {
            continue;
        }
        if (!atlasTexture || cell.glyph.bitmap_size.x <= 0 || cell.glyph.bitmap_size.y <= 0)
        {
            continue;
        }

        const ImVec2 cellMin(
            top_left.x + cell.column * metrics.cell_width,
            top_left.y + cell.row * metrics.cell_height);
        const auto& fontMetrics = m_textService.metrics();
        const ImVec2 glyphMin = nvim_glyph_origin(cellMin, fontMetrics, cell.glyph);
        const ImVec2 glyphMax(
            glyphMin.x + static_cast<float>(cell.glyph.bitmap_size.x),
            glyphMin.y + static_cast<float>(cell.glyph.bitmap_size.y));
        const ImVec2 uvMin(cell.glyph.uv.x, cell.glyph.uv.y);
        const ImVec2 uvMax(cell.glyph.uv.z, cell.glyph.uv.w);
        const bool colorGlyph = (cell.style_flags & vklive_nvim::STYLE_FLAG_COLOR_GLYPH) != 0;
        drawList->AddImage(
            (ImTextureID)atlasTexture,
            glyphMin,
            glyphMax,
            uvMin,
            uvMax,
            colorGlyph ? IM_COL32_WHITE : color_to_imgui(cell.foreground));
    }
}
