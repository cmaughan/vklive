#pragma once

#include "font_metrics.h"
#include <vklive_nvim/glyph_atlas.h>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace vklive_nvim
{

struct TextServiceConfig
{
    static constexpr size_t DEFAULT_FONT_CHOICE_CACHE_LIMIT = 4096;

    std::string font_path;
    std::string bold_font_path;
    std::string italic_font_path;
    std::string bold_italic_font_path;
    std::vector<std::string> fallback_paths;
    size_t font_choice_cache_limit = DEFAULT_FONT_CHOICE_CACHE_LIMIT;
    bool enable_ligatures = true;
};

class TextService : public IGlyphAtlas
{
public:
    static constexpr float DEFAULT_POINT_SIZE = 11.0f;
    static constexpr float MIN_POINT_SIZE = 6.0f;
    static constexpr float MAX_POINT_SIZE = 72.0f;

    TextService();
    ~TextService() override;
    TextService(const TextService&) = delete;
    TextService& operator=(const TextService&) = delete;
    TextService(TextService&& other) noexcept;
    TextService& operator=(TextService&& other) noexcept;

    bool initialize(float point_size, float display_ppi);
    bool initialize(const TextServiceConfig& config, float point_size, float display_ppi);
    void shutdown();

    bool set_point_size(float point_size);
    float point_size() const;
    const FontMetrics& metrics() const;
    const std::string& primary_font_path() const;

    AtlasRegion resolve_cluster(const std::string& text, bool is_bold, bool is_italic) override;
    AtlasRegion resolve_cluster(const std::string& text)
    {
        return resolve_cluster(text, false, false);
    }
    int ligature_cell_span(const std::string& text, bool is_bold, bool is_italic) override;
    int ligature_cell_span(const std::string& text)
    {
        return ligature_cell_span(text, false, false);
    }

    bool atlas_dirty() const override;
    bool consume_atlas_reset() override;
    void clear_atlas_dirty() override;
    const uint8_t* atlas_data() const override;
    int atlas_width() const override;
    int atlas_height() const override;
    AtlasDirtyRect atlas_dirty_rect() const override;

    float atlas_usage_ratio() const;
    size_t atlas_glyph_count() const;
    int atlas_reset_count() const;
    size_t font_choice_cache_size() const;

    // Drains warnings collected during the most recent initialize() — typically
    // missing bold/italic variants. Caller should surface these to the user
    // (e.g. as toast notifications). Subsequent calls return an empty vector
    // until the next initialization.
    std::vector<std::string> take_font_warnings();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace vklive_nvim
