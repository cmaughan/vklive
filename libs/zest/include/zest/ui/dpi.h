#pragma once

#include <zest/math/math_utils.h>

namespace Zest
{

struct Dpi
{
    float scaleFactor = 1.0;
    glm::vec2 scaleFactorXY = glm::vec2(1.0f);
};
extern Dpi dpi;

void check_dpi();
void set_dpi(const glm::vec2& val);
float dpi_pixel_height_from_point_size(float pointSize, float pixelScaleY);

#define MDPI_VEC2(value) (value * dpi.scaleFactorXY)
#define MDPI_Y(value) (value * dpi.scaleFactorXY.y)
#define MDPI_X(value) (value * dpi.scaleFactorXY.x)
#define MDPI_RECT(value) (value * dpi.scaleFactorXY)

} // Zest
