#pragma once

#include <string>
#include <string_view>

enum class RenderBackend
{
    Auto,
    Vulkan,
    Metal
};

inline std::string render_backend_to_string(RenderBackend backend)
{
    switch (backend)
    {
    case RenderBackend::Auto:
        return "auto";
    case RenderBackend::Vulkan:
        return "vulkan";
    case RenderBackend::Metal:
        return "metal";
    }
    return "auto";
}

inline bool render_backend_from_string(std::string_view text, RenderBackend& out)
{
    if (text == "auto")
    {
        out = RenderBackend::Auto;
        return true;
    }
    if (text == "vulkan")
    {
        out = RenderBackend::Vulkan;
        return true;
    }
    if (text == "metal")
    {
        out = RenderBackend::Metal;
        return true;
    }
    return false;
}
