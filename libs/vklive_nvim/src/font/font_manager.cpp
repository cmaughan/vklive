#include "font_engine.h"
#include <cmath>
#include <vklive_nvim/log.h>
#include <vklive_nvim/perf_timing.h>
#include <utility>

namespace vklive_nvim
{

FontManager::FontManager(FontManager&& other) noexcept
{
    *this = std::move(other);
}

FontManager& FontManager::operator=(FontManager&& other) noexcept
{
    if (this == &other)
        return *this;

    shutdown();

    ft_lib_ = other.ft_lib_;
    face_ = other.face_;
    hb_font_ = other.hb_font_;
    metrics_ = other.metrics_;
    point_size_ = other.point_size_;
    display_ppi_ = other.display_ppi_;

    other.ft_lib_ = nullptr;
    other.face_ = nullptr;
    other.hb_font_ = nullptr;
    other.metrics_ = {};
    other.point_size_ = 0.0f;
    other.display_ppi_ = 96.0f;

    return *this;
}

bool FontManager::initialize(const std::string& font_path, float point_size, float display_ppi)
{
    PERF_MEASURE();
    point_size_ = point_size;
    display_ppi_ = display_ppi;

    if (FT_Init_FreeType(&ft_lib_))
    {
        DRAXUL_LOG_ERROR(LogCategory::Font, "Failed to init FreeType");
        return false;
    }

    if (FT_Error ft_err = FT_New_Face(ft_lib_, font_path.c_str(), 0, &face_))
    {
        const char* err_str = FT_Error_String(ft_err);
        if (err_str)
            DRAXUL_LOG_ERROR(LogCategory::Font, "Failed to load font '%s': FreeType error %d (%s)", font_path.c_str(), ft_err, err_str);
        else
            DRAXUL_LOG_ERROR(LogCategory::Font, "Failed to load font '%s': FreeType error %d", font_path.c_str(), ft_err);
        return false;
    }

    FT_Set_Char_Size(face_, 0, static_cast<FT_F26Dot6>(point_size * 64.0f), (FT_UInt)display_ppi, (FT_UInt)display_ppi);
    select_best_fixed_size();

    update_metrics();

    hb_font_ = hb_ft_font_create(face_, nullptr);
    if (!hb_font_)
    {
        DRAXUL_LOG_ERROR(LogCategory::Font, "Failed to create HarfBuzz font");
        return false;
    }

    return true;
}

bool FontManager::set_point_size(float point_size)
{
    PERF_MEASURE();
    point_size_ = point_size;
    FT_Set_Char_Size(face_, 0, static_cast<FT_F26Dot6>(point_size * 64.0f), (FT_UInt)display_ppi_, (FT_UInt)display_ppi_);
    select_best_fixed_size();

    update_metrics();

    if (hb_font_)
        hb_font_destroy(hb_font_);
    hb_font_ = hb_ft_font_create(face_, nullptr);

    return hb_font_ != nullptr;
}

void FontManager::select_best_fixed_size()
{
    if (!FT_HAS_FIXED_SIZES(face_) || face_->num_fixed_sizes <= 0)
        return;

    auto target_px = (int)std::round(point_size_ * display_ppi_ / 72.0f);
    int best = 0;
    for (int i = 1; i < face_->num_fixed_sizes; i++)
    {
        if (std::abs(face_->available_sizes[i].height - target_px) < std::abs(face_->available_sizes[best].height - target_px))
            best = i;
    }
    FT_Select_Size(face_, best);
}

void FontManager::update_metrics()
{
    metrics_.cell_width = (int)(face_->size->metrics.max_advance >> 6);
    metrics_.cell_height = (int)(face_->size->metrics.height >> 6);
    metrics_.ascender = (int)(face_->size->metrics.ascender >> 6);
    metrics_.descender = (int)(-face_->size->metrics.descender >> 6);

    if (metrics_.cell_width == 0)
    {
        FT_Load_Char(face_, 'M', FT_LOAD_DEFAULT);
        metrics_.cell_width = (int)(face_->glyph->advance.x >> 6);
    }
}

void FontManager::shutdown()
{
    PERF_MEASURE();
    if (hb_font_)
    {
        hb_font_destroy(hb_font_);
        hb_font_ = nullptr;
    }
    if (face_)
    {
        FT_Done_Face(face_);
        face_ = nullptr;
    }
    if (ft_lib_)
    {
        FT_Done_FreeType(ft_lib_);
        ft_lib_ = nullptr;
    }
}

} // namespace vklive_nvim
