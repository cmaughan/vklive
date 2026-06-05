#pragma once

#include "font_engine.h"
#include "font_resolver.h"
#include "font_selector.h"

#include <vklive_nvim/unicode.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <unordered_map>

namespace vklive_nvim
{

namespace
{

inline int count_codepoints(std::string_view text)
{
    int count = 0;
    size_t offset = 0;
    while (offset < text.size())
    {
        uint32_t cp = 0;
        if (!utf8_decode_next(text, offset, cp))
            return 0;
        ++count;
    }
    return count;
}

inline bool shaping_differs(
    const std::vector<ShapedGlyph>& lhs, const std::vector<ShapedGlyph>& rhs)
{
    if (lhs.size() != rhs.size())
        return true;

    for (size_t i = 0; i < lhs.size(); ++i)
    {
        if (lhs[i].glyph_id != rhs[i].glyph_id || lhs[i].x_advance != rhs[i].x_advance
            || lhs[i].x_offset != rhs[i].x_offset || lhs[i].y_offset != rhs[i].y_offset
            || lhs[i].cluster != rhs[i].cluster)
        {
            return true;
        }
    }

    return false;
}

} // namespace

// Detects eligible ligature spans in a cluster sequence.
class LigatureAnalyser
{
public:
    void reset_cache()
    {
        cache_.clear();
    }

    void set_cache_limit(size_t limit)
    {
        cache_limit_ = std::max<size_t>(1, limit);
    }

    // Maximum number of cells a ligature can span.
    static constexpr int MAX_LIGATURE_CELLS = 6;

    // Returns the ligature span width in cells (0 = no ligature, 2+ = multi-cell ligature).
    // Requires ligatures enabled; otherwise always returns 0.
    int ligature_cell_span(
        const std::string& text, bool enable_ligatures, FontSelector& selector, FontResolver& resolver,
        bool is_bold = false, bool is_italic = false)
    {
        if (!enable_ligatures)
            return 0;

        auto it = cache_.find(text);
        if (it != cache_.end())
            return it->second;

        const int codepoint_count = count_codepoints(text);
        if (codepoint_count < 2)
        {
            store(text, 0);
            return 0;
        }

        auto sel = selector.select(text, resolver, is_bold, is_italic);
        auto shaped = sel.shaper->shape(text);
        if (shaped.empty())
        {
            store(text, 0);
            return 0;
        }

        // Find the unligated shaper for the chosen face.
        TextShaper* unligated = &resolver.primary_unligated_shaper();
        if (resolver.has_bold_italic() && sel.face == resolver.bold_italic().face())
        {
            unligated = &resolver.bold_italic_unligated_shaper();
        }
        else if (resolver.has_bold() && sel.face == resolver.bold().face())
        {
            unligated = &resolver.bold_unligated_shaper();
        }
        else if (resolver.has_italic() && sel.face == resolver.italic().face())
        {
            unligated = &resolver.italic_unligated_shaper();
        }
        else if (sel.face != resolver.primary().face())
        {
            auto& fallbacks = resolver.fallbacks();
            auto fb_it = std::find_if(
                fallbacks.begin(), fallbacks.end(),
                [&](const FontResolver::FallbackFont& fb) { return fb.font.face() == sel.face; });
            if (fb_it != fallbacks.end())
                unligated = &fb_it->unligated_shaper;
        }

        auto plain = unligated->shape(text);
        if (plain.empty() || !shaping_differs(shaped, plain))
        {
            store(text, 0);
            return 0;
        }

        int advance = 0;
        for (const auto& glyph : shaped)
            advance += std::max(0, glyph.x_advance);

        const int cell_width = std::max(1, resolver.primary().metrics().cell_width);
        const int span = std::max(1, (advance + cell_width - 1) / cell_width);
        const int ligature_span = (span >= 2 && span <= MAX_LIGATURE_CELLS) ? span : 0;
        store(text, ligature_span);
        return ligature_span;
    }

    size_t cache_size() const
    {
        return cache_.size();
    }

private:
    void store(const std::string& text, int span)
    {
        if (cache_.size() >= cache_limit_)
            cache_.clear();
        cache_[text] = span;
    }

    std::unordered_map<std::string, int> cache_;
    size_t cache_limit_ = TextServiceConfig::DEFAULT_FONT_CHOICE_CACHE_LIMIT;
};

} // namespace vklive_nvim
