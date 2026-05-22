#include "app/window_render_sizing.h"

#include <algorithm>
#include <cmath>

namespace
{

glm::vec2 sanitize_framebuffer_scale(const glm::vec2& framebufferScale)
{
    return glm::vec2(
        framebufferScale.x > 0.0f ? framebufferScale.x : 1.0f,
        framebufferScale.y > 0.0f ? framebufferScale.y : 1.0f);
}

} // namespace

glm::vec2 window_render_pixel_size(const glm::vec2& logicalSize, const glm::vec2& framebufferScale)
{
    const auto scale = sanitize_framebuffer_scale(framebufferScale);
    return glm::vec2(
        std::ceil(std::max(logicalSize.x, 0.0f) * scale.x),
        std::ceil(std::max(logicalSize.y, 0.0f) * scale.y));
}

glm::vec2 window_render_logical_surface_size(const glm::uvec2& surfacePixelSize, const glm::vec2& framebufferScale)
{
    const auto scale = sanitize_framebuffer_scale(framebufferScale);
    return glm::vec2(
        static_cast<float>(surfacePixelSize.x) / scale.x,
        static_cast<float>(surfacePixelSize.y) / scale.y);
}

glm::vec4 window_render_pixel_viewport(const glm::vec2& topLeft, const glm::vec2& bottomRight, const glm::vec2& maxRect, const glm::vec2& framebufferScale)
{
    const auto scale = sanitize_framebuffer_scale(framebufferScale);
    const auto clampedBottomRight = glm::vec2(std::min(bottomRight.x, maxRect.x), std::min(bottomRight.y, maxRect.y));
    return glm::vec4(
        std::floor(topLeft.x * scale.x),
        std::floor(topLeft.y * scale.y),
        std::floor(clampedBottomRight.x * scale.x),
        std::floor(clampedBottomRight.y * scale.y));
}
