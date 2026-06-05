#include "font_resolver.h"

#include <vklive_nvim/log.h>
#include <vklive_nvim/text_service.h>

#include <filesystem>
#include <initializer_list>
#include <utility>

namespace vklive_nvim
{

namespace detail
{

std::vector<std::string> default_fallback_font_candidates()
{
#ifdef _WIN32
    const char* windir = std::getenv("WINDIR");
    std::string windows_dir = windir ? windir : "C:\\Windows";
    return {
        windows_dir + "\\Fonts\\seguiemj.ttf",
        windows_dir + "\\Fonts\\seguisym.ttf",
        windows_dir + "\\Fonts\\YuGothR.ttc",
        windows_dir + "\\Fonts\\YuGothM.ttc",
        windows_dir + "\\Fonts\\meiryo.ttc",
        windows_dir + "\\Fonts\\msgothic.ttc",
        windows_dir + "\\Fonts\\msyh.ttc",
        windows_dir + "\\Fonts\\simsun.ttc",
    };
#elif defined(__APPLE__)
    return {
        "/System/Library/Fonts/Apple Color Emoji.ttc",
        "/System/Library/Fonts/Apple Symbols.ttf",
        "/System/Library/Fonts/Apple Braille.ttf",
        "/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
        "/System/Library/Fonts/Hiragino Sans GB.ttc",
        "/System/Library/Fonts/Supplemental/Songti.ttc",
    };
#else
    return {
        "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf",
        "/usr/share/fonts/truetype/noto/NotoEmoji-Regular.ttf",
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    };
#endif
}

std::vector<std::string> default_primary_font_candidates()
{
#ifdef _WIN32
    const char* windir = std::getenv("WINDIR");
    const char* local_app_data = std::getenv("LOCALAPPDATA");
    std::string windows_dir = windir ? windir : "C:\\Windows";
    std::string local_fonts = local_app_data
        ? (std::string(local_app_data) + "\\Microsoft\\Windows\\Fonts\\")
        : "";

    std::vector<std::string> candidates;
    if (!local_fonts.empty())
    {
        candidates.push_back(local_fonts + "JetBrainsMonoNerdFont-Regular.ttf");
        candidates.push_back(local_fonts + "JetBrainsMonoNerdFontMono-Regular.ttf");
    }
    candidates.push_back(windows_dir + "\\Fonts\\JetBrainsMonoNerdFont-Regular.ttf");
    candidates.push_back(windows_dir + "\\Fonts\\JetBrainsMonoNerdFontMono-Regular.ttf");
    candidates.push_back(
        windows_dir + "\\Fonts\\JetBrains Mono Regular Nerd Font Complete Windows Compatible.ttf");
    candidates.push_back(
        windows_dir + "\\Fonts\\JetBrains Mono Regular Nerd Font Complete Mono Windows Compatible.ttf");
    candidates.push_back("fonts/JetBrainsMonoNerdFont-Regular.ttf");
    return candidates;
#else
    return { "fonts/JetBrainsMonoNerdFont-Regular.ttf" };
#endif
}

std::string first_existing_path(const std::vector<std::string>& candidates)
{
    for (const auto& path : candidates)
    {
        if (std::filesystem::exists(path))
            return path;
    }
    return {};
}

std::string auto_detect_bold_path(const std::string& regular_path)
{
    for (const auto& [from, to] : std::initializer_list<std::pair<std::string, std::string>>{
             { "-Regular.", "-Bold." },
             { "_Regular.", "_Bold." },
             { "-Regular-", "-Bold-" },
         })
    {
        auto pos = regular_path.rfind(from);
        if (pos != std::string::npos)
        {
            std::string candidate = regular_path.substr(0, pos) + to + regular_path.substr(pos + from.size());
            if (std::filesystem::exists(candidate))
                return candidate;
        }
    }
    return {};
}

std::string auto_detect_italic_path(const std::string& regular_path)
{
    for (const auto& [from, to] : std::initializer_list<std::pair<std::string, std::string>>{
             { "-Regular.", "-Italic." },
             { "_Regular.", "_Italic." },
             { "-Regular-", "-Italic-" },
         })
    {
        auto pos = regular_path.rfind(from);
        if (pos != std::string::npos)
        {
            std::string candidate = regular_path.substr(0, pos) + to + regular_path.substr(pos + from.size());
            if (std::filesystem::exists(candidate))
                return candidate;
        }
    }
    return {};
}

std::string auto_detect_bold_italic_path(const std::string& regular_path)
{
    for (const auto& [from, to] : std::initializer_list<std::pair<std::string, std::string>>{
             { "-Regular.", "-BoldItalic." },
             { "_Regular.", "_BoldItalic." },
             { "-Regular-", "-BoldItalic-" },
         })
    {
        auto pos = regular_path.rfind(from);
        if (pos != std::string::npos)
        {
            std::string candidate = regular_path.substr(0, pos) + to + regular_path.substr(pos + from.size());
            if (std::filesystem::exists(candidate))
                return candidate;
        }
    }
    return {};
}

} // namespace detail

bool FontResolver::initialize(const TextServiceConfig& config, float point_size, float display_ppi)
{
    config_ = &config;
    display_ppi_ = display_ppi;
    warnings_.clear();
    if (config.font_path.empty())
    {
        font_path_ = detail::first_existing_path(detail::default_primary_font_candidates());
    }
    else
    {
        font_path_ = config.font_path;
        if (!std::filesystem::exists(font_path_))
        {
            DRAXUL_LOG_WARN(LogCategory::Font, "Configured font path does not exist: '%s'", font_path_.c_str());
            warnings_.push_back("Configured font path does not exist: " + font_path_);
        }
    }
    if (font_path_.empty())
        font_path_ = "fonts/JetBrainsMonoNerdFont-Regular.ttf";

    if (!primary_.initialize(font_path_, point_size, display_ppi))
        return false;

    primary_shaper_.initialize(primary_.hb_font(), config.enable_ligatures);
    primary_unligated_shaper_.initialize(primary_.hb_font(), false);

    // Determine bold font path
    std::string bold_path = config.bold_font_path;
    if (bold_path.empty())
        bold_path = detail::auto_detect_bold_path(font_path_);
    if (!bold_path.empty() && bold_path != font_path_)
    {
        if (bold_.initialize(bold_path, point_size, display_ppi))
        {
            bold_shaper_.initialize(bold_.hb_font(), config.enable_ligatures);
            bold_unligated_shaper_.initialize(bold_.hb_font(), false);
            bold_loaded_ = true;
            bold_font_path_ = bold_path;
            DRAXUL_LOG_DEBUG(LogCategory::Font, "Bold font loaded: %s", bold_path.c_str());
        }
        else
        {
            DRAXUL_LOG_WARN(LogCategory::Font, "Bold font not found: %s", bold_path.c_str());
            warnings_.push_back("Bold font not found: " + bold_path);
        }
    }
    else if (!bold_path.empty())
    {
        DRAXUL_LOG_DEBUG(LogCategory::Font, "Bold font path same as regular, skipping");
    }
    else
    {
        DRAXUL_LOG_WARN(LogCategory::Font, "No bold font found for: %s", font_path_.c_str());
        warnings_.push_back("No bold font variant found; using regular");
    }

    // Determine italic font path
    std::string italic_path = config.italic_font_path;
    if (italic_path.empty())
        italic_path = detail::auto_detect_italic_path(font_path_);
    if (!italic_path.empty() && italic_path != font_path_)
    {
        if (italic_.initialize(italic_path, point_size, display_ppi))
        {
            italic_shaper_.initialize(italic_.hb_font(), config.enable_ligatures);
            italic_unligated_shaper_.initialize(italic_.hb_font(), false);
            italic_loaded_ = true;
            italic_font_path_ = italic_path;
            DRAXUL_LOG_DEBUG(LogCategory::Font, "Italic font loaded: %s", italic_path.c_str());
        }
        else
        {
            DRAXUL_LOG_WARN(LogCategory::Font, "Italic font not found: %s", italic_path.c_str());
            warnings_.push_back("Italic font not found: " + italic_path);
        }
    }
    else if (!italic_path.empty())
    {
        DRAXUL_LOG_DEBUG(LogCategory::Font, "Italic font path same as regular, skipping");
    }
    else
    {
        DRAXUL_LOG_WARN(LogCategory::Font, "No italic font found for: %s", font_path_.c_str());
        warnings_.push_back("No italic font variant found; using regular");
    }

    // Determine bold-italic font path
    std::string bold_italic_path = config.bold_italic_font_path;
    if (bold_italic_path.empty())
        bold_italic_path = detail::auto_detect_bold_italic_path(font_path_);
    if (!bold_italic_path.empty() && bold_italic_path != font_path_)
    {
        if (bold_italic_.initialize(bold_italic_path, point_size, display_ppi))
        {
            bold_italic_shaper_.initialize(bold_italic_.hb_font(), config.enable_ligatures);
            bold_italic_unligated_shaper_.initialize(bold_italic_.hb_font(), false);
            bold_italic_loaded_ = true;
            bold_italic_font_path_ = bold_italic_path;
            DRAXUL_LOG_DEBUG(LogCategory::Font, "Bold-italic font loaded: %s", bold_italic_path.c_str());
        }
        else
        {
            DRAXUL_LOG_WARN(LogCategory::Font, "Bold-italic font not found: %s", bold_italic_path.c_str());
            warnings_.push_back("Bold-italic font not found: " + bold_italic_path);
        }
    }
    else if (!bold_italic_path.empty())
    {
        DRAXUL_LOG_DEBUG(LogCategory::Font, "Bold-italic font path same as regular, skipping");
    }
    else
    {
        DRAXUL_LOG_WARN(LogCategory::Font, "No bold-italic font found for: %s", font_path_.c_str());
        warnings_.push_back("No bold-italic font variant found; using regular");
    }

    load_fallback_fonts();
    return true;
}

void FontResolver::shutdown()
{
    primary_shaper_.shutdown();
    primary_unligated_shaper_.shutdown();
    for (auto& fallback : fallbacks_)
    {
        if (!fallback.loaded)
            continue;
        fallback.shaper.shutdown();
        fallback.unligated_shaper.shutdown();
        fallback.font.shutdown();
    }
    fallbacks_.clear();
    primary_.shutdown();
    if (bold_loaded_)
    {
        bold_shaper_.shutdown();
        bold_unligated_shaper_.shutdown();
        bold_.shutdown();
        bold_loaded_ = false;
    }
    if (italic_loaded_)
    {
        italic_shaper_.shutdown();
        italic_unligated_shaper_.shutdown();
        italic_.shutdown();
        italic_loaded_ = false;
    }
    if (bold_italic_loaded_)
    {
        bold_italic_shaper_.shutdown();
        bold_italic_unligated_shaper_.shutdown();
        bold_italic_.shutdown();
        bold_italic_loaded_ = false;
    }
}

bool FontResolver::set_point_size(float point_size)
{
    if (!primary_.set_point_size(point_size))
        return false;

    primary_shaper_.initialize(primary_.hb_font(), config_->enable_ligatures);
    primary_unligated_shaper_.initialize(primary_.hb_font(), false);

    for (auto& fallback : fallbacks_)
    {
        if (!fallback.loaded)
            continue;
        fallback.font.set_point_size(point_size);
        fallback.shaper.initialize(fallback.font.hb_font(), config_->enable_ligatures);
        fallback.unligated_shaper.initialize(fallback.font.hb_font(), false);
    }
    if (bold_loaded_)
    {
        bold_.set_point_size(point_size);
        bold_shaper_.shutdown();
        bold_unligated_shaper_.shutdown();
        bold_shaper_.initialize(bold_.hb_font(), config_->enable_ligatures);
        bold_unligated_shaper_.initialize(bold_.hb_font(), false);
    }
    if (italic_loaded_)
    {
        italic_.set_point_size(point_size);
        italic_shaper_.shutdown();
        italic_unligated_shaper_.shutdown();
        italic_shaper_.initialize(italic_.hb_font(), config_->enable_ligatures);
        italic_unligated_shaper_.initialize(italic_.hb_font(), false);
    }
    if (bold_italic_loaded_)
    {
        bold_italic_.set_point_size(point_size);
        bold_italic_shaper_.shutdown();
        bold_italic_unligated_shaper_.shutdown();
        bold_italic_shaper_.initialize(bold_italic_.hb_font(), config_->enable_ligatures);
        bold_italic_unligated_shaper_.initialize(bold_italic_.hb_font(), false);
    }
    return true;
}

bool FontResolver::ensure_loaded(size_t index)
{
    auto& fb = fallbacks_[index];
    if (fb.loaded)
        return true;
    if (fb.failed)
        return false;

    if (!fb.font.initialize(fb.path, primary_.point_size(), display_ppi_))
    {
        DRAXUL_LOG_WARN(LogCategory::Font, "Fallback font failed to load: %s", fb.path.c_str());
        fb.failed = true;
        return false;
    }

    fb.shaper.initialize(fb.font.hb_font(), config_->enable_ligatures);
    fb.unligated_shaper.initialize(fb.font.hb_font(), false);
    fb.loaded = true;
    DRAXUL_LOG_DEBUG(LogCategory::Font, "Fallback font loaded (on demand): %s", fb.path.c_str());
    return true;
}

void FontResolver::load_fallback_fonts()
{
    fallbacks_.clear();

    std::vector<std::string> candidates = config_->fallback_paths.empty()
        ? detail::default_fallback_font_candidates()
        : config_->fallback_paths;

    fallbacks_.reserve(candidates.size());
    for (const auto& path : candidates)
    {
        if (path == font_path_)
        {
            DRAXUL_LOG_DEBUG(LogCategory::Font, "Fallback candidate skipped (same as primary): %s", path.c_str());
            continue;
        }
        if (!std::filesystem::exists(path))
        {
            DRAXUL_LOG_DEBUG(LogCategory::Font, "Fallback candidate not found, skipping: %s", path.c_str());
            continue;
        }

        fallbacks_.emplace_back();
        fallbacks_.back().path = path;
        DRAXUL_LOG_DEBUG(LogCategory::Font, "Fallback font registered (deferred load): %s", path.c_str());
    }
}

} // namespace vklive_nvim
