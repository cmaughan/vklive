#pragma once

#include <glm/glm.hpp>

glm::vec2 window_render_pixel_size(const glm::vec2& logicalSize, const glm::vec2& framebufferScale);
glm::vec2 window_render_logical_surface_size(const glm::uvec2& surfacePixelSize, const glm::vec2& framebufferScale);
glm::vec4 window_render_pixel_viewport(const glm::vec2& topLeft, const glm::vec2& bottomRight, const glm::vec2& maxRect, const glm::vec2& framebufferScale);
