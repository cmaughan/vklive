#include <stdexcept>

#include <SDL2/SDL.h>

#include <vklive/device_factory.h>

namespace vulkan
{
#if defined(VKLIVE_ENABLE_VULKAN)
std::shared_ptr<IDevice> create_vulkan_device(SDL_Window* pWindow, const std::string& settingsPath, bool viewports);
#endif
} // namespace vulkan

namespace metal
{
#if defined(VKLIVE_ENABLE_METAL)
std::shared_ptr<IDevice> create_metal_device(SDL_Window* pWindow, const std::string& settingsPath, bool viewports);
#endif
} // namespace metal

RenderBackend device_resolve_backend(RenderBackend requested)
{
    if (requested != RenderBackend::Auto)
    {
        return requested;
    }
#if defined(__APPLE__) && defined(VKLIVE_ENABLE_METAL)
    return RenderBackend::Metal;
#else
    return RenderBackend::Vulkan;
#endif
}

uint32_t device_sdl_window_flags(RenderBackend backend)
{
    uint32_t flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    switch (backend)
    {
    case RenderBackend::Metal:
#if defined(__APPLE__) && defined(VKLIVE_ENABLE_METAL)
        flags |= SDL_WINDOW_METAL;
        break;
#else
        throw std::runtime_error("Metal renderer was not built");
#endif
    case RenderBackend::Vulkan:
#if defined(VKLIVE_ENABLE_VULKAN)
        flags |= SDL_WINDOW_VULKAN;
        break;
#else
        throw std::runtime_error("Vulkan renderer was not built");
#endif
    case RenderBackend::Auto:
        return device_sdl_window_flags(device_resolve_backend(backend));
    }
    return flags;
}

std::shared_ptr<IDevice> device_create(RenderBackend backend, SDL_Window* pWindow, const std::string& settingsPath, bool viewports)
{
    backend = device_resolve_backend(backend);
    switch (backend)
    {
    case RenderBackend::Metal:
#if defined(VKLIVE_ENABLE_METAL)
        return metal::create_metal_device(pWindow, settingsPath, viewports);
#else
        throw std::runtime_error("Metal renderer was not built");
#endif
    case RenderBackend::Vulkan:
#if defined(VKLIVE_ENABLE_VULKAN)
        return vulkan::create_vulkan_device(pWindow, settingsPath, viewports);
#else
        throw std::runtime_error("Vulkan renderer was not built");
#endif
    case RenderBackend::Auto:
        break;
    }
    throw std::runtime_error("Could not resolve renderer backend");
}
