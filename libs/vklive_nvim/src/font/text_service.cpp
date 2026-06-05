#include <vklive_nvim/text_service.h>

#include "font_resolver.h"
#include "font_selector.h"
#include "glyph_atlas_manager.h"
#include "ligature_analyser.h"

#include <algorithm>
#include <vklive_nvim/perf_timing.h>

namespace vklive_nvim
{

struct TextService::Impl
{
    TextServiceConfig config;

    FontResolver resolver;
    FontSelector selector;
    GlyphAtlasManager atlas_manager;
    LigatureAnalyser ligature_analyser;

    bool initialize(float point_size, float ppi)
    {
        if (!resolver.initialize(config, point_size, ppi))
            return false;

        selector.reset_cache();
        selector.set_cache_limit(config.font_choice_cache_limit);
        ligature_analyser.reset_cache();
        ligature_analyser.set_cache_limit(config.font_choice_cache_limit);

        if (!atlas_manager.initialize(resolver.primary().face(), static_cast<int>(resolver.primary().point_size())))
            return false;

        return true;
    }

    void shutdown()
    {
        resolver.shutdown();
        selector.reset_cache();
        ligature_analyser.reset_cache();
    }

    bool set_point_size(float point_size)
    {
        point_size = std::clamp(point_size, TextService::MIN_POINT_SIZE, TextService::MAX_POINT_SIZE);
        if (point_size == resolver.primary().point_size())
            return true;

        if (!resolver.set_point_size(point_size))
            return false;

        atlas_manager.reset_atlas(resolver.primary().face(), static_cast<int>(resolver.primary().point_size()));
        selector.reset_cache();
        ligature_analyser.reset_cache();
        return true;
    }

    AtlasRegion resolve_cluster(const std::string& text, bool is_bold = false, bool is_italic = false)
    {
        return atlas_manager.resolve_cluster(text, selector, resolver, is_bold, is_italic);
    }

    int ligature_cell_span(const std::string& text, bool is_bold = false, bool is_italic = false)
    {
        return ligature_analyser.ligature_cell_span(text, config.enable_ligatures, selector, resolver, is_bold, is_italic);
    }
};

TextService::TextService()
    : impl_(std::make_unique<Impl>())
{
}

TextService::~TextService() = default;

TextService::TextService(TextService&& other) noexcept = default;

TextService& TextService::operator=(TextService&& other) noexcept = default;

bool TextService::initialize(float point_size, float display_ppi)
{
    return initialize(TextServiceConfig{}, point_size, display_ppi);
}

bool TextService::initialize(const TextServiceConfig& config, float point_size, float display_ppi)
{
    PERF_MEASURE();
    impl_->config = config;
    return impl_->initialize(point_size, display_ppi);
}

void TextService::shutdown()
{
    PERF_MEASURE();
    impl_->shutdown();
}

bool TextService::set_point_size(float point_size)
{
    PERF_MEASURE();
    return impl_->set_point_size(point_size);
}

float TextService::point_size() const
{
    return impl_->resolver.primary().point_size();
}

const FontMetrics& TextService::metrics() const
{
    return impl_->resolver.primary().metrics();
}

const std::string& TextService::primary_font_path() const
{
    return impl_->resolver.font_path();
}

AtlasRegion TextService::resolve_cluster(const std::string& text, bool is_bold, bool is_italic)
{
    return impl_->resolve_cluster(text, is_bold, is_italic);
}

int TextService::ligature_cell_span(const std::string& text, bool is_bold, bool is_italic)
{
    return impl_->ligature_cell_span(text, is_bold, is_italic);
}

bool TextService::atlas_dirty() const
{
    return impl_->atlas_manager.cache().atlas_dirty();
}

void TextService::clear_atlas_dirty()
{
    impl_->atlas_manager.cache().clear_dirty();
}

const uint8_t* TextService::atlas_data() const
{
    return impl_->atlas_manager.cache().atlas_data();
}

int TextService::atlas_width() const
{
    return impl_->atlas_manager.cache().atlas_width();
}

int TextService::atlas_height() const
{
    return impl_->atlas_manager.cache().atlas_height();
}

AtlasDirtyRect TextService::atlas_dirty_rect() const
{
    const auto& dirty = impl_->atlas_manager.cache().dirty_rect();
    return { dirty.pos, dirty.size };
}

float TextService::atlas_usage_ratio() const
{
    return impl_->atlas_manager.cache().usage_ratio();
}

size_t TextService::atlas_glyph_count() const
{
    return impl_->atlas_manager.cache().glyph_count();
}

int TextService::atlas_reset_count() const
{
    return impl_->atlas_manager.reset_count();
}

size_t TextService::font_choice_cache_size() const
{
    return impl_->selector.cache_size();
}

std::vector<std::string> TextService::take_font_warnings()
{
    return impl_->resolver.take_warnings();
}

bool TextService::consume_atlas_reset()
{
    return impl_->atlas_manager.consume_atlas_reset();
}

} // namespace vklive_nvim
