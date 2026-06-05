#pragma once

#include "font_engine.h"
#include "font_resolver.h"
#include "font_selector.h"

#include <cassert>
#include <vklive_nvim/log.h>

namespace vklive_nvim
{

// Manages atlas packing, rasterisation, and atlas-overflow reset policy.
class GlyphAtlasManager
{
public:
    bool initialize(FT_Face primary_face, int point_size)
    {
        atlas_reset_pending_ = false;
        atlas_reset_count_ = 0;
        expected_primary_face_ = primary_face;
        return glyph_cache_.initialize(primary_face, point_size);
    }

    void reset_atlas(FT_Face primary_face, int point_size)
    {
        expected_primary_face_ = primary_face;
        glyph_cache_.reset(primary_face, point_size);
        atlas_reset_pending_ = true;
        atlas_reset_count_++;
    }

    // Resolves a text cluster to an atlas region, retrying once after an overflow reset.
    AtlasRegion resolve_cluster(
        const std::string& text, FontSelector& selector, FontResolver& resolver,
        bool is_bold = false, bool is_italic = false)
    {
        // Safety: verify the primary face hasn't changed without a reset_atlas() call.
        // This catches use-after-free if TextService recreates the FT_Face without
        // resetting the glyph cache.
        assert(expected_primary_face_ == resolver.primary().face()
            && "GlyphAtlasManager: primary FT_Face changed without reset_atlas()");
        if (expected_primary_face_ != resolver.primary().face())
        {
            DRAXUL_LOG_WARN(LogCategory::Font,
                "GlyphAtlasManager: primary face changed without reset; auto-resetting (gen=%u)",
                glyph_cache_.face_generation());
            reset_atlas(resolver.primary().face(), static_cast<int>(resolver.primary().point_size()));
        }

        auto sel = selector.select(text, resolver, is_bold, is_italic);
        AtlasRegion region = glyph_cache_.get_cluster(text, sel.face, *sel.shaper);

        if (region.bitmap_size.x > 0 || region.bitmap_size.y > 0 || !glyph_cache_.consume_overflowed())
            return region;

        // Atlas overflowed — reset and retry once.
        glyph_cache_.reset(resolver.primary().face(), static_cast<int>(resolver.primary().point_size()));
        atlas_reset_pending_ = true;
        atlas_reset_count_++;
        DRAXUL_LOG_WARN(
            LogCategory::Font, "Glyph atlas reset after exhaustion (count=%d)", atlas_reset_count_);

        sel = selector.select(text, resolver, is_bold, is_italic);
        return glyph_cache_.get_cluster(text, sel.face, *sel.shaper);
    }

    GlyphCache& cache()
    {
        return glyph_cache_;
    }
    const GlyphCache& cache() const
    {
        return glyph_cache_;
    }

    bool consume_atlas_reset()
    {
        bool pending = atlas_reset_pending_;
        atlas_reset_pending_ = false;
        return pending;
    }

    int reset_count() const
    {
        return atlas_reset_count_;
    }

private:
    GlyphCache glyph_cache_;
    FT_Face expected_primary_face_ = nullptr;
    bool atlas_reset_pending_ = false;
    int atlas_reset_count_ = 0;
};

} // namespace vklive_nvim
