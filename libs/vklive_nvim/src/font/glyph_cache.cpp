#include "font_engine.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <algorithm>
#include <climits>
#include <cstring>
#include <vklive_nvim/log.h>
#include <vklive_nvim/perf_timing.h>

namespace vklive_nvim
{

namespace
{

struct RasterizedGlyph
{
    int width = 0;
    int height = 0;
    int left = 0;
    int top = 0;
    bool is_color = false;
    std::vector<uint8_t> pixels;
};

constexpr int ATLAS_PIXEL_SIZE = 4;

int snapped_advance(int pen_x, FT_Face face)
{
    if (pen_x <= 0)
        return 0;

    const auto face_cell_w = face != nullptr ? static_cast<int>(face->size->metrics.max_advance >> 6) : 0;
    if (face_cell_w <= 0)
        return pen_x;

    const int n_cells = std::max(1, (pen_x + face_cell_w - 1) / face_cell_w);
    return n_cells * face_cell_w;
}

const uint8_t* bitmap_row_ptr(const FT_Bitmap& bmp, int row)
{
    if (bmp.pitch >= 0)
        return bmp.buffer + row * bmp.pitch;
    return bmp.buffer + (bmp.rows - 1 - row) * (-bmp.pitch);
}

uint8_t unpremultiply_channel(uint8_t value, uint8_t alpha)
{
    if (alpha == 0)
        return 0;

    int result = (static_cast<int>(value) * 255 + alpha / 2) / alpha;
    return static_cast<uint8_t>(std::clamp(result, 0, 255));
}

bool bitmap_to_rgba(const FT_Bitmap& bmp, std::vector<uint8_t>& out, bool& is_color)
{
    PERF_MEASURE();
    auto width = (int)bmp.width;
    auto height = (int)bmp.rows;
    if (width <= 0 || height <= 0)
    {
        out.clear();
        is_color = false;
        return true;
    }

    out.assign((size_t)width * height * ATLAS_PIXEL_SIZE, 0);
    is_color = false;

    switch (bmp.pixel_mode)
    {
    case FT_PIXEL_MODE_GRAY:
        for (int row = 0; row < height; row++)
        {
            const uint8_t* src = bitmap_row_ptr(bmp, row);
            uint8_t* dst = out.data() + (size_t)row * width * ATLAS_PIXEL_SIZE;
            for (int col = 0; col < width; col++)
            {
                dst[col * 4 + 0] = 255;
                dst[col * 4 + 1] = 255;
                dst[col * 4 + 2] = 255;
                dst[col * 4 + 3] = src[col];
            }
        }
        return true;

    case FT_PIXEL_MODE_MONO:
        for (int row = 0; row < height; row++)
        {
            const uint8_t* src = bitmap_row_ptr(bmp, row);
            uint8_t* dst = out.data() + (size_t)row * width * ATLAS_PIXEL_SIZE;
            for (int col = 0; col < width; col++)
            {
                uint8_t mask = (uint8_t)(0x80 >> (col & 7));
                uint8_t alpha = (src[col >> 3] & mask) ? 255 : 0;
                dst[col * 4 + 0] = 255;
                dst[col * 4 + 1] = 255;
                dst[col * 4 + 2] = 255;
                dst[col * 4 + 3] = alpha;
            }
        }
        return true;

    case FT_PIXEL_MODE_BGRA:
        is_color = true;
        for (int row = 0; row < height; row++)
        {
            const uint8_t* src = bitmap_row_ptr(bmp, row);
            uint8_t* dst = out.data() + (size_t)row * width * ATLAS_PIXEL_SIZE;
            for (int col = 0; col < width; col++)
            {
                const uint8_t b = src[col * 4 + 0];
                const uint8_t g = src[col * 4 + 1];
                const uint8_t r = src[col * 4 + 2];
                const uint8_t a = src[col * 4 + 3];
                dst[col * 4 + 0] = unpremultiply_channel(r, a);
                dst[col * 4 + 1] = unpremultiply_channel(g, a);
                dst[col * 4 + 2] = unpremultiply_channel(b, a);
                dst[col * 4 + 3] = a;
            }
        }
        return true;

    default:
        break;
    }

    DRAXUL_LOG_WARN(LogCategory::Font, "Unsupported FreeType pixel mode: %u", bmp.pixel_mode);
    return false;
}

void expand_dirty_rect(GlyphCache::DirtyRect& dirty_rect, bool dirty, int x, int y, int w, int h)
{
    PERF_MEASURE();
    if (!dirty)
    {
        dirty_rect = { { x, y }, { w, h } };
        return;
    }

    int left = std::min(dirty_rect.pos.x, x);
    int top = std::min(dirty_rect.pos.y, y);
    int right = std::max(dirty_rect.pos.x + dirty_rect.size.x, x + w);
    int bottom = std::max(dirty_rect.pos.y + dirty_rect.size.y, y + h);
    dirty_rect = { { left, top }, { right - left, bottom - top } };
}

} // namespace

bool GlyphCache::initialize(FT_Face face, int pixel_size, int atlas_size)
{
    PERF_MEASURE();
    face_ = face;
    pixel_size_ = pixel_size;
    atlas_size_ = std::max(1, atlas_size);
    atlas_.assign((size_t)atlas_size_ * atlas_size_ * ATLAS_PIXEL_SIZE, 0);
    dirty_ = false;
    dirty_rect_ = {};
    overflowed_ = false;
    shelf_x_ = 0;
    shelf_y_ = 0;
    shelf_height_ = 0;
    used_pixels_ = 0;
    cluster_cache_.clear();
    face_generation_++;
    return true;
}

void GlyphCache::reset(FT_Face face, int pixel_size)
{
    PERF_MEASURE();
    face_ = face;
    pixel_size_ = pixel_size;
    cluster_cache_.clear();
    std::fill(atlas_.begin(), atlas_.end(), (uint8_t)0);
    shelf_x_ = 0;
    shelf_y_ = 0;
    shelf_height_ = 0;
    used_pixels_ = 0;
    dirty_ = true;
    dirty_rect_ = { { 0, 0 }, { atlas_size_, atlas_size_ } };
    overflowed_ = false;
    face_generation_++;
}

size_t GlyphCache::ClusterKeyHash::operator()(const ClusterKey& key) const
{
    size_t h1 = std::hash<FT_Face>()(key.face);
    size_t h2 = std::hash<std::string>()(key.text);
    return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
}

const AtlasRegion& GlyphCache::get_cluster(const std::string& text, FT_Face face, TextShaper& shaper)
{
    PERF_MEASURE();
    if (text.empty() || text == " ")
        return empty_region_;

    ClusterKey key = { face, text };
    auto it = cluster_cache_.find(key);
    if (it != cluster_cache_.end())
        return it->second;

    auto result = rasterize_cluster(text, face, shaper);
    if (result.has_value())
    {
        auto [ins, _] = cluster_cache_.try_emplace(std::move(key), result.value());
        return ins->second;
    }

    DRAXUL_LOG_DEBUG(LogCategory::Font, "rasterize_cluster failed: %s", result.error().message.c_str());
    return empty_region_;
}

bool GlyphCache::reserve_region(int w, int h, int& atlas_x, int& atlas_y, const char* label)
{
    PERF_MEASURE();
    if (w <= 0 || h <= 0)
        return false;

    if (shelf_x_ + w + 1 > atlas_size_)
    {
        shelf_y_ += shelf_height_ + 1;
        shelf_x_ = 0;
        shelf_height_ = 0;
    }

    if (shelf_y_ + h > atlas_size_)
    {
        DRAXUL_LOG_WARN(LogCategory::Font, "Atlas full, cannot fit %s (%dx%d)", label, w, h);
        overflowed_ = true;
        return false;
    }

    atlas_x = shelf_x_;
    atlas_y = shelf_y_;
    shelf_x_ += w + 1;
    shelf_height_ = std::max(shelf_height_, h);
    return true;
}

Result<AtlasRegion, Error> GlyphCache::rasterize_cluster(const std::string& text, FT_Face face, TextShaper& shaper)
{
    PERF_MEASURE();
    auto shaped = shaper.shape(text);
    if (shaped.empty())
        return AtlasRegion{};

    std::vector<RasterizedGlyph> glyphs;
    glyphs.reserve(shaped.size());

    int pen_x = 0;
    int bbox_left = INT_MAX;
    int bbox_right = INT_MIN;
    int bbox_top = INT_MIN;
    int bbox_bottom = INT_MAX;

    for (const auto& shaped_glyph : shaped)
    {
        if (FT_Load_Glyph(face, shaped_glyph.glyph_id, FT_LOAD_DEFAULT | FT_LOAD_COLOR))
            return Result<AtlasRegion, Error>::err(
                Error{ ErrorKind::AtlasOverflow, "FT_Load_Glyph failed for cluster: " + text });
        if (face->glyph->format != FT_GLYPH_FORMAT_BITMAP
            && FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL))
            return Result<AtlasRegion, Error>::err(
                Error{ ErrorKind::AtlasOverflow, "FT_Render_Glyph failed for cluster: " + text });

        FT_Bitmap& bmp = face->glyph->bitmap;
        if (bmp.width > 0 && bmp.rows > 0)
        {
            RasterizedGlyph glyph;
            glyph.width = (int)bmp.width;
            glyph.height = (int)bmp.rows;
            glyph.left = pen_x + shaped_glyph.x_offset + face->glyph->bitmap_left;
            glyph.top = shaped_glyph.y_offset + face->glyph->bitmap_top;
            if (!bitmap_to_rgba(bmp, glyph.pixels, glyph.is_color))
                return Result<AtlasRegion, Error>::err(
                    Error{ ErrorKind::AtlasOverflow, "bitmap_to_rgba failed for cluster: " + text });

            bbox_left = std::min(bbox_left, glyph.left);
            bbox_right = std::max(bbox_right, glyph.left + glyph.width);
            bbox_top = std::max(bbox_top, glyph.top);
            bbox_bottom = std::min(bbox_bottom, glyph.top - glyph.height);
            glyphs.push_back(std::move(glyph));
        }

        pen_x += shaped_glyph.x_advance;
    }

    const int natural_advance_px = std::max(0, pen_x);
    const int snapped_advance_px = snapped_advance(pen_x, face);

    if (glyphs.empty())
    {
        AtlasRegion region{};
        region.advance_px = natural_advance_px;
        return region;
    }

    // If no glyphs had ink (all zero-size bitmaps), reset sentinels to avoid
    // signed overflow in cluster_height = bbox_top - bbox_bottom.
    if (bbox_top == INT_MIN)
    {
        bbox_top = 0;
        bbox_bottom = 0;
    }

    // Extend the bounding box to cover the full advance width and to start
    // at the cell origin. This fills right-side bearing gaps (e.g. the thin
    // strip after Powerline / Nerd Font half-circle glyphs) with transparent
    // pixels that the fragment shader discards, so the glyph covers the full
    // cell without a visible background strip.
    //
    // Use face->size->metrics.max_advance (the same source as cell_width in
    // font_manager) to snap bbox_right to a whole-cell boundary. HarfBuzz
    // x_advance >> 6 can be 1 px less than max_advance >> 6 due to independent
    // 26.6 truncation, which would leave a 1-pixel gap.
    bbox_right = std::max(bbox_right, snapped_advance_px);
    bbox_left = std::min(bbox_left, 0);

    int cluster_width = bbox_right - bbox_left;
    int cluster_height = bbox_top - bbox_bottom;

    int atlas_x = 0;
    int atlas_y = 0;
    if (!reserve_region(cluster_width, cluster_height, atlas_x, atlas_y, "cluster"))
        return Result<AtlasRegion, Error>::err(
            Error{ ErrorKind::AtlasOverflow, "atlas full for cluster: " + text });
    used_pixels_ += (size_t)cluster_width * cluster_height;

    std::vector<uint8_t> composite((size_t)cluster_width * cluster_height * ATLAS_PIXEL_SIZE, 0);
    bool cluster_is_color = false;
    for (const auto& glyph : glyphs)
    {
        cluster_is_color = cluster_is_color || glyph.is_color;
        int dst_x = glyph.left - bbox_left;
        int dst_y = bbox_top - glyph.top;
        for (int row = 0; row < glyph.height; row++)
        {
            for (int col = 0; col < glyph.width; col++)
            {
                uint8_t* dst = composite.data()
                    + (((size_t)(dst_y + row) * cluster_width) + dst_x + col) * ATLAS_PIXEL_SIZE;
                const uint8_t* src = glyph.pixels.data() + (((size_t)row * glyph.width) + col) * ATLAS_PIXEL_SIZE;

                const float src_alpha = src[3] / 255.0f;
                const float dst_alpha = dst[3] / 255.0f;
                const float out_alpha = src_alpha + dst_alpha * (1.0f - src_alpha);

                if (out_alpha <= 0.0f)
                {
                    dst[0] = dst[1] = dst[2] = dst[3] = 0;
                    continue;
                }

                for (int channel = 0; channel < 3; channel++)
                {
                    float src_value = src[channel] / 255.0f;
                    float dst_value = dst[channel] / 255.0f;
                    float out_value = (src_value * src_alpha + dst_value * dst_alpha * (1.0f - src_alpha))
                        / out_alpha;
                    dst[channel] = static_cast<uint8_t>(std::clamp((int)(out_value * 255.0f + 0.5f), 0, 255));
                }
                dst[3] = static_cast<uint8_t>(std::clamp((int)(out_alpha * 255.0f + 0.5f), 0, 255));
            }
        }
    }

    for (int row = 0; row < cluster_height; row++)
    {
        memcpy(atlas_.data() + (((size_t)(atlas_y + row) * atlas_size_) + atlas_x) * ATLAS_PIXEL_SIZE,
            composite.data() + (size_t)row * cluster_width * ATLAS_PIXEL_SIZE,
            (size_t)cluster_width * ATLAS_PIXEL_SIZE);
    }

    AtlasRegion region{};
    float inv_size = 1.0f / static_cast<float>(atlas_size_);
    region.uv = {
        static_cast<float>(atlas_x) * inv_size,
        static_cast<float>(atlas_y) * inv_size,
        static_cast<float>(atlas_x + cluster_width) * inv_size,
        static_cast<float>(atlas_y + cluster_height) * inv_size
    };
    region.bitmap_bearing = { bbox_left, bbox_top };
    region.bitmap_size = { cluster_width, cluster_height };
    region.advance_px = natural_advance_px;
    region.is_color = cluster_is_color;

    expand_dirty_rect(dirty_rect_, dirty_, atlas_x, atlas_y, cluster_width, cluster_height);
    dirty_ = true;
    return region;
}

} // namespace vklive_nvim
