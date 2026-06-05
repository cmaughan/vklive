#include "font_engine.h"
#include <array>
#include <vklive_nvim/perf_timing.h>
#include <hb-ft.h>
#include <utility>

namespace vklive_nvim
{

TextShaper::TextShaper(TextShaper&& other) noexcept
{
    PERF_MEASURE();
    *this = std::move(other);
}

TextShaper& TextShaper::operator=(TextShaper&& other) noexcept
{
    PERF_MEASURE();
    if (this == &other)
        return *this;

    shutdown();

    font_ = other.font_;
    buffer_ = other.buffer_;

    other.font_ = nullptr;
    other.buffer_ = nullptr;

    return *this;
}

TextShaper::~TextShaper()
{
    PERF_MEASURE();
    shutdown();
}

void TextShaper::initialize(hb_font_t* font, bool enable_ligatures)
{
    PERF_MEASURE();
    shutdown();
    font_ = font;
    buffer_ = hb_buffer_create();
    enable_ligatures_ = enable_ligatures;
}

void TextShaper::shutdown()
{
    PERF_MEASURE();
    if (buffer_)
    {
        hb_buffer_destroy(buffer_);
        buffer_ = nullptr;
    }
}

std::vector<ShapedGlyph> TextShaper::shape(const std::string& text)
{
    PERF_MEASURE();
    std::vector<ShapedGlyph> result;
    if (!font_ || !buffer_)
        return result;

    hb_buffer_reset(buffer_);
    hb_buffer_add_utf8(buffer_, text.c_str(), (int)text.size(), 0, (int)text.size());
    hb_buffer_guess_segment_properties(buffer_);

    constexpr std::array<hb_feature_t, 4> disabled_ligature_features = { {
        { HB_TAG('l', 'i', 'g', 'a'), 0, HB_FEATURE_GLOBAL_START, HB_FEATURE_GLOBAL_END },
        { HB_TAG('c', 'l', 'i', 'g'), 0, HB_FEATURE_GLOBAL_START, HB_FEATURE_GLOBAL_END },
        { HB_TAG('c', 'a', 'l', 't'), 0, HB_FEATURE_GLOBAL_START, HB_FEATURE_GLOBAL_END },
        { HB_TAG('d', 'l', 'i', 'g'), 0, HB_FEATURE_GLOBAL_START, HB_FEATURE_GLOBAL_END },
    } };
    hb_shape(font_, buffer_,
        enable_ligatures_ ? nullptr : disabled_ligature_features.data(),
        enable_ligatures_ ? 0u : static_cast<unsigned int>(disabled_ligature_features.size()));

    unsigned int glyph_count;
    hb_glyph_info_t* info = hb_buffer_get_glyph_infos(buffer_, &glyph_count);
    hb_glyph_position_t* pos = hb_buffer_get_glyph_positions(buffer_, &glyph_count);

    result.reserve(glyph_count);
    for (unsigned int i = 0; i < glyph_count; i++)
    {
        result.push_back({ info[i].codepoint,
            (int)(pos[i].x_advance >> 6),
            (int)(pos[i].x_offset >> 6),
            (int)(pos[i].y_offset >> 6),
            (int)info[i].cluster });
    }

    return result;
}

} // namespace vklive_nvim
