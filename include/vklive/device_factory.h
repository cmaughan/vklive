#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <vklive/IDevice.h>
#include <vklive/render_backend.h>

struct SDL_Window;

RenderBackend device_resolve_backend(RenderBackend requested);
uint32_t device_sdl_window_flags(RenderBackend backend);
std::shared_ptr<IDevice> device_create(RenderBackend backend, SDL_Window* pWindow, const std::string& settingsPath, bool viewports);
