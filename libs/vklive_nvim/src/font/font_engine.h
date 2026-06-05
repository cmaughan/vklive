#pragma once

#include <cstdint>
#include <vklive_nvim/font_metrics.h>
#include <vklive_nvim/result.h>
#include <vklive_nvim/types.h>
#include <string>
#include <unordered_map>
#include <vector>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb-ft.h>
#include <hb.h>

namespace vklive_nvim
{

class TextShaper;

class FontManager
{
public:
    FontManager() = default;
    ~FontManager() = default;
    FontManager(const FontManager&) = delete;
    FontManager& operator=(const FontManager&) = delete;
    FontManager(FontManager&& other) noexcept;
    FontManager& operator=(FontManager&& other) noexcept;

    bool initialize(const std::string& font_path, float point_size, float display_ppi);
    bool set_point_size(float point_size);
    void shutdown();

    FT_Face face() const
    {
        return face_;
    }
    hb_font_t* hb_font() const
    {
        return hb_font_;
    }
    const FontMetrics& metrics() const
    {
        return metrics_;
    }
    float point_size() const
    {
        return point_size_;
    }

private:
    void update_metrics();
    void select_best_fixed_size();

    FT_Library ft_lib_ = nullptr;
    FT_Face face_ = nullptr;
    hb_font_t* hb_font_ = nullptr;
    FontMetrics metrics_ = {};
    float point_size_ = 0.0f;
    float display_ppi_ = 96.0f;
};

struct ShapedGlyph
{
    uint32_t glyph_id = 0;
    int x_advance = 0;
    int x_offset = 0;
    int y_offset = 0;
    int cluster = 0;
};

class TextShaper
{
public:
    TextShaper() = default;
    TextShaper(const TextShaper&) = delete;
    TextShaper& operator=(const TextShaper&) = delete;
    TextShaper(TextShaper&& other) noexcept;
    TextShaper& operator=(TextShaper&& other) noexcept;
    ~TextShaper();

    void initialize(hb_font_t* font, bool enable_ligatures = true);
    void shutdown();

    std::vector<ShapedGlyph> shape(const std::string& text);

private:
    hb_font_t* font_ = nullptr;
    hb_buffer_t* buffer_ = nullptr;
    bool enable_ligatures_ = true;
};

class GlyphCache
{
public:
    struct DirtyRect
    {
        glm::ivec2 pos = {};
        glm::ivec2 size = {};
    };

    bool initialize(FT_Face face, int pixel_size, int atlas_size = 2048);
    void reset(FT_Face face, int pixel_size);

    // Generation counter — incremented on every reset/initialize.  Callers
    // can snapshot the value and compare later to detect stale state.
    uint32_t face_generation() const
    {
        return face_generation_;
    }

    const AtlasRegion& get_cluster(const std::string& text, FT_Face face, TextShaper& shaper);

    bool atlas_dirty() const
    {
        return dirty_;
    }
    void clear_dirty()
    {
        dirty_ = false;
        dirty_rect_ = {};
    }

    const uint8_t* atlas_data() const
    {
        return atlas_.data();
    }
    int atlas_width() const
    {
        return atlas_size_;
    }
    int atlas_height() const
    {
        return atlas_size_;
    }
    size_t glyph_count() const
    {
        return cluster_cache_.size();
    }
    size_t used_pixels() const
    {
        return used_pixels_;
    }
    float usage_ratio() const
    {
        const size_t total_pixels = (size_t)atlas_size_ * atlas_size_;
        return total_pixels ? static_cast<float>(used_pixels_) / static_cast<float>(total_pixels) : 0.0f;
    }
    const DirtyRect& dirty_rect() const
    {
        return dirty_rect_;
    }

    bool consume_overflowed()
    {
        bool overflowed = overflowed_;
        overflowed_ = false;
        return overflowed;
    }

private:
    struct ClusterKey
    {
        FT_Face face = nullptr;
        std::string text;

        bool operator==(const ClusterKey&) const = default;
    };

    struct ClusterKeyHash
    {
        size_t operator()(const ClusterKey& key) const;
    };

    // WI 24: returns the rasterized atlas region, or a structured error
    // describing why rasterization failed (FreeType load/render error, atlas
    // overflow, or color-glyph conversion failure). Empty/whitespace clusters
    // produce an ok result wrapping a default-constructed AtlasRegion.
    Result<AtlasRegion, Error> rasterize_cluster(const std::string& text, FT_Face face, TextShaper& shaper);
    bool reserve_region(int w, int h, int& atlas_x, int& atlas_y, const char* label);

    FT_Face face_ = nullptr;
    int pixel_size_ = 0;
    int atlas_size_ = 2048;
    uint32_t face_generation_ = 0;

    std::vector<uint8_t> atlas_;
    std::unordered_map<ClusterKey, AtlasRegion, ClusterKeyHash> cluster_cache_;

    int shelf_x_ = 0;
    int shelf_y_ = 0;
    int shelf_height_ = 0;
    size_t used_pixels_ = 0;

    bool dirty_ = false;
    DirtyRect dirty_rect_ = {};
    bool overflowed_ = false;
    AtlasRegion empty_region_ = {};
};

} // namespace vklive_nvim
