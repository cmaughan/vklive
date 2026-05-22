#include "app/editor_font.h"

#include <algorithm>
#include <cmath>

int zep_effective_font_pixel_height(float fontSize, float fontGlobalScale)
{
    const auto scale = fontGlobalScale > 0.0f ? fontGlobalScale : 1.0f;
    const auto logicalSize = std::max(fontSize * scale, 1.0f);
    return std::max(1, static_cast<int>(std::round(logicalSize)));
}
