#include <cstdlib>
#include <iostream>
#include <string>

#include <app/window_render_sizing.h>

namespace
{

bool require(bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << message << "\n";
        return false;
    }
    return true;
}

bool require_vec2(const glm::vec2& actual, const glm::vec2& expected, const std::string& message)
{
    return require(actual == expected,
        message + ": expected=(" + std::to_string(expected.x) + ", " + std::to_string(expected.y) + ") actual=(" + std::to_string(actual.x) + ", " + std::to_string(actual.y) + ")");
}

bool require_vec4(const glm::vec4& actual, const glm::vec4& expected, const std::string& message)
{
    return require(actual == expected,
        message + ": expected=(" + std::to_string(expected.x) + ", " + std::to_string(expected.y) + ", " + std::to_string(expected.z) + ", " + std::to_string(expected.w) + ") actual=(" + std::to_string(actual.x) + ", " + std::to_string(actual.y) + ", " + std::to_string(actual.z) + ", " + std::to_string(actual.w) + ")");
}

} // namespace

int main()
{
    bool ok = true;

    ok &= require_vec2(window_render_pixel_size(glm::vec2(400.0f, 300.0f), glm::vec2(2.0f, 2.0f)),
        glm::vec2(800.0f, 600.0f),
        "retina render target should use framebuffer pixels");

    ok &= require_vec2(window_render_logical_surface_size(glm::uvec2(800, 600), glm::vec2(2.0f, 2.0f)),
        glm::vec2(400.0f, 300.0f),
        "retina surface should draw at logical ImGui size");

    ok &= require_vec4(window_render_pixel_viewport(glm::vec2(10.0f, 20.0f), glm::vec2(410.0f, 320.0f), glm::vec2(400.0f, 300.0f), glm::vec2(2.0f, 2.0f)),
        glm::vec4(20.0f, 40.0f, 800.0f, 600.0f),
        "target viewport should be clamped then converted to framebuffer pixels");

    ok &= require_vec2(window_render_pixel_size(glm::vec2(400.25f, 300.25f), glm::vec2(2.0f, 2.0f)),
        glm::vec2(801.0f, 601.0f),
        "fractional logical sizes should round up to whole pixels");

    ok &= require_vec2(window_render_pixel_size(glm::vec2(400.0f, 300.0f), glm::vec2(0.0f, -1.0f)),
        glm::vec2(400.0f, 300.0f),
        "invalid framebuffer scales should fall back to 1x");

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
