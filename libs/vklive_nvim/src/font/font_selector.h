#pragma once

#include "font_engine.h"
#include "font_resolver.h"

#include <vklive_nvim/unicode.h>

#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace vklive_nvim
{

namespace detail
{

inline bool can_render_cluster(FT_Face face, TextShaper& shaper, const std::string& text)
{
    if (!face)
        return false;

    auto shaped = shaper.shape(text);
    if (shaped.empty())
        return false;

    bool has_glyph = false;
    for (const auto& glyph : shaped)
    {
        if (glyph.glyph_id == 0)
            return false;
        if (FT_Load_Glyph(face, glyph.glyph_id, FT_LOAD_DEFAULT))
            return false;
        has_glyph = true;
    }

    return has_glyph;
}

inline bool cluster_prefers_color_font(std::string_view text)
{
    size_t offset = 0;
    uint32_t base = 0;
    bool have_base = false;
    bool has_vs16 = false;
    bool has_zwj = false;
    bool has_emoji_modifier = false;
    bool has_keycap = false;

    while (offset < text.size())
    {
        uint32_t cp = 0;
        if (!utf8_decode_next(text, offset, cp))
            break;

        if (cp == 0xFE0F)
            has_vs16 = true;
        else if (cp == 0x200D)
            has_zwj = true;
        else if (cp == 0x20E3)
            has_keycap = true;
        else if (is_emoji_modifier(cp))
            has_emoji_modifier = true;

        if (!have_base && !is_width_ignorable(cp) && !is_emoji_modifier(cp))
        {
            base = cp;
            have_base = true;
        }
    }

    if (!have_base)
        return false;

    if (is_default_emoji_presentation(base) || is_regional_indicator(base))
        return true;

    if ((has_vs16 || has_zwj || has_emoji_modifier) && is_emoji_text_presentation_candidate(base))
        return true;

    if (has_keycap && is_ascii_keycap_base(base))
        return true;

    return false;
}

inline bool font_has_color(FT_Face face)
{
    return face && FT_HAS_COLOR(face);
}

struct TransparentStringHash
{
    using is_transparent = void;

    size_t operator()(std::string_view text) const noexcept
    {
        return std::hash<std::string_view>{}(text);
    }
};

struct TransparentStringEqual
{
    using is_transparent = void;

    bool operator()(std::string_view lhs, std::string_view rhs) const noexcept
    {
        return lhs == rhs;
    }
};

} // namespace detail

// Selects the best font face for a given text cluster, with per-cluster caching.
class FontSelector
{
public:
    struct Selection
    {
        FT_Face face = nullptr;
        TextShaper* shaper = nullptr;
    };

    void reset_cache()
    {
        cache_.clear();
        bold_cache_.clear();
        italic_cache_.clear();
        bold_italic_cache_.clear();
    }

    void set_cache_limit(size_t limit)
    {
        cache_limit_ = std::max<size_t>(1, limit);
    }

    // Returns the face+shaper best suited to render `text`.
    Selection select(const std::string& text, FontResolver& resolver, bool is_bold = false, bool is_italic = false)
    {
        if (is_bold && is_italic && resolver.has_bold_italic())
        {
            auto sel = try_variant_selection(text, bold_italic_cache_,
                resolver.bold_italic().face(), resolver.bold_italic_shaper(), resolver,
                [this](const std::string& t, int i) { store_bold_italic(t, i); });
            if (sel.face)
                return sel;
        }

        if (is_bold && resolver.has_bold())
        {
            auto sel = try_variant_selection(text, bold_cache_,
                resolver.bold().face(), resolver.bold_shaper(), resolver,
                [this](const std::string& t, int i) { store_bold(t, i); });
            if (sel.face)
                return sel;
        }

        if (is_italic && resolver.has_italic())
        {
            auto sel = try_variant_selection(text, italic_cache_,
                resolver.italic().face(), resolver.italic_shaper(), resolver,
                [this](const std::string& t, int i) { store_italic(t, i); });
            if (sel.face)
                return sel;
        }

        auto it = cache_.find(text);
        if (it != cache_.end())
        {
            int idx = it->second;
            if (idx < 0)
                return { resolver.primary().face(), &resolver.primary_shaper() };
            auto& fallbacks = resolver.fallbacks();
            if (idx < (int)fallbacks.size())
                return { fallbacks[(size_t)idx].font.face(), &fallbacks[(size_t)idx].shaper };
        }

        if (detail::cluster_prefers_color_font(text))
        {
            if (auto sel = select_color_font(text, resolver))
                return *sel;
        }

        if (detail::can_render_cluster(resolver.primary().face(), resolver.primary_shaper(), text))
        {
            store(text, -1);
            return { resolver.primary().face(), &resolver.primary_shaper() };
        }

        auto fb = search_fallbacks(text, resolver, /*color_only=*/false);
        if (fb.face)
            return fb;

        store(text, -1);
        return { resolver.primary().face(), &resolver.primary_shaper() };
    }

    size_t cache_size() const
    {
        return cache_.size();
    }

private:
    // Search fallback fonts for a renderable cluster. If color_only, only color-capable fonts are tried.
    Selection search_fallbacks(const std::string& text, FontResolver& resolver, bool color_only)
    {
        auto& fallbacks = resolver.fallbacks();
        for (int i = 0; i < (int)fallbacks.size(); i++)
        {
            if (!resolver.ensure_loaded((size_t)i))
                continue;
            auto& fb = fallbacks[(size_t)i];
            if (color_only && !detail::font_has_color(fb.font.face()))
                continue;
            if (detail::can_render_cluster(fb.font.face(), fb.shaper, text))
            {
                store(text, i);
                return { fb.font.face(), &fb.shaper };
            }
        }
        return {};
    }

    // Try primary color font then color fallbacks for emoji/color clusters.
    std::optional<Selection> select_color_font(const std::string& text, FontResolver& resolver)
    {
        if (detail::font_has_color(resolver.primary().face())
            && detail::can_render_cluster(resolver.primary().face(), resolver.primary_shaper(), text))
        {
            store(text, -1);
            return Selection{ resolver.primary().face(), &resolver.primary_shaper() };
        }
        auto fb = search_fallbacks(text, resolver, /*color_only=*/true);
        if (fb.face)
            return fb;
        return std::nullopt;
    }

    // Check cache then try a style-variant face; returns empty Selection on miss.
    template <typename StoreFn>
    static Selection try_variant_selection(const std::string& text,
        std::unordered_map<std::string, int, detail::TransparentStringHash, detail::TransparentStringEqual>& variant_cache,
        FT_Face variant_face, TextShaper& variant_shaper,
        FontResolver& resolver, StoreFn store_fn)
    {
        auto it = variant_cache.find(text);
        if (it != variant_cache.end())
        {
            if (it->second < 0)
                return { variant_face, &variant_shaper };
            auto& fallbacks = resolver.fallbacks();
            int idx = it->second;
            if (idx < (int)fallbacks.size())
                return { fallbacks[(size_t)idx].font.face(), &fallbacks[(size_t)idx].shaper };
        }
        if (detail::can_render_cluster(variant_face, variant_shaper, text))
        {
            store_fn(text, -1);
            return { variant_face, &variant_shaper };
        }
        return {};
    }

    void store(const std::string& text, int idx)
    {
        if (cache_.size() >= cache_limit_)
            cache_.clear();
        cache_[text] = idx;
    }

    void store_bold(const std::string& text, int idx)
    {
        if (bold_cache_.size() >= cache_limit_)
            bold_cache_.clear();
        bold_cache_[text] = idx;
    }

    void store_italic(const std::string& text, int idx)
    {
        if (italic_cache_.size() >= cache_limit_)
            italic_cache_.clear();
        italic_cache_[text] = idx;
    }

    void store_bold_italic(const std::string& text, int idx)
    {
        if (bold_italic_cache_.size() >= cache_limit_)
            bold_italic_cache_.clear();
        bold_italic_cache_[text] = idx;
    }

    std::unordered_map<std::string, int, detail::TransparentStringHash, detail::TransparentStringEqual> cache_;
    std::unordered_map<std::string, int, detail::TransparentStringHash, detail::TransparentStringEqual> bold_cache_;
    std::unordered_map<std::string, int, detail::TransparentStringHash, detail::TransparentStringEqual> italic_cache_;
    std::unordered_map<std::string, int, detail::TransparentStringHash, detail::TransparentStringEqual> bold_italic_cache_;
    size_t cache_limit_ = TextServiceConfig::DEFAULT_FONT_CHOICE_CACHE_LIMIT;
};

} // namespace vklive_nvim
