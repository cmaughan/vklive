#pragma once

#include "font_engine.h"

#include <string>
#include <vector>

namespace vklive_nvim
{

struct TextServiceConfig;

namespace detail
{

std::vector<std::string> default_fallback_font_candidates();
std::vector<std::string> default_primary_font_candidates();
std::string first_existing_path(const std::vector<std::string>& candidates);
std::string auto_detect_bold_path(const std::string& regular_path);
std::string auto_detect_italic_path(const std::string& regular_path);
std::string auto_detect_bold_italic_path(const std::string& regular_path);

} // namespace detail

// Discovers and loads the primary font face plus all fallback faces.
class FontResolver
{
public:
    struct FallbackFont
    {
        FontManager font;
        TextShaper shaper;
        TextShaper unligated_shaper;
        std::string path;
        bool loaded = false;
        bool failed = false;
    };

    // Initializes the primary font and all fallback fonts.
    bool initialize(const TextServiceConfig& config, float point_size, float display_ppi);

    void shutdown();

    // Resizes all fonts; returns false if primary resize fails.
    // Unloaded fallbacks are skipped — they will initialize at the new size
    // when first needed, since ensure_loaded() uses primary_.point_size().
    bool set_point_size(float point_size);

    FontManager& primary()
    {
        return primary_;
    }
    const FontManager& primary() const
    {
        return primary_;
    }
    TextShaper& primary_shaper()
    {
        return primary_shaper_;
    }
    TextShaper& primary_unligated_shaper()
    {
        return primary_unligated_shaper_;
    }

    std::vector<FallbackFont>& fallbacks()
    {
        return fallbacks_;
    }
    const std::vector<FallbackFont>& fallbacks() const
    {
        return fallbacks_;
    }

    const std::string& font_path() const
    {
        return font_path_;
    }

    // Ensure the fallback at `index` is loaded. Returns false if it failed to
    // load (permanently — subsequent calls also return false without retrying).
    bool ensure_loaded(size_t index);

    void load_fallback_fonts();

    FontManager& bold()
    {
        return bold_;
    }
    TextShaper& bold_shaper()
    {
        return bold_shaper_;
    }
    TextShaper& bold_unligated_shaper()
    {
        return bold_unligated_shaper_;
    }
    bool has_bold() const
    {
        return bold_loaded_;
    }
    const std::string& bold_font_path() const
    {
        return bold_font_path_;
    }

    FontManager& italic()
    {
        return italic_;
    }
    TextShaper& italic_shaper()
    {
        return italic_shaper_;
    }
    TextShaper& italic_unligated_shaper()
    {
        return italic_unligated_shaper_;
    }
    bool has_italic() const
    {
        return italic_loaded_;
    }
    const std::string& italic_font_path() const
    {
        return italic_font_path_;
    }

    FontManager& bold_italic()
    {
        return bold_italic_;
    }
    TextShaper& bold_italic_shaper()
    {
        return bold_italic_shaper_;
    }
    TextShaper& bold_italic_unligated_shaper()
    {
        return bold_italic_unligated_shaper_;
    }
    bool has_bold_italic() const
    {
        return bold_italic_loaded_;
    }
    const std::string& bold_italic_font_path() const
    {
        return bold_italic_font_path_;
    }

    // Warnings collected during initialize() — e.g. missing bold/italic/bold-italic
    // variants. The list is filled once per initialize() and consumed by callers
    // (typically TextService → App) which surface them as toast notifications.
    std::vector<std::string> take_warnings()
    {
        return std::move(warnings_);
    }

    const TextServiceConfig* config_ = nullptr;
    float display_ppi_ = 96.0f;
    std::string font_path_;
    FontManager primary_;
    TextShaper primary_shaper_;
    TextShaper primary_unligated_shaper_;
    std::vector<FallbackFont> fallbacks_;
    std::vector<std::string> warnings_;

private:
    FontManager bold_;
    TextShaper bold_shaper_;
    TextShaper bold_unligated_shaper_;
    bool bold_loaded_ = false;
    std::string bold_font_path_;

    FontManager italic_;
    TextShaper italic_shaper_;
    TextShaper italic_unligated_shaper_;
    bool italic_loaded_ = false;
    std::string italic_font_path_;

    FontManager bold_italic_;
    TextShaper bold_italic_shaper_;
    TextShaper bold_italic_unligated_shaper_;
    bool bold_italic_loaded_ = false;
    std::string bold_italic_font_path_;
};

} // namespace vklive_nvim
