# Metal On Mac Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make Rezonality use a native Metal rendering backend by default on macOS while keeping Vulkan as the default backend on Windows and Linux.

**Architecture:** Split backend selection from app startup, keep `IDevice` as the app-facing contract, and add a project-owned Metal backend that mirrors the current Vulkan scene/pass/surface/model flow. On macOS, GLSL live-editing continues through `glslangValidator` to SPIR-V, then SPIRV-Cross emits MSL and SPIRV-Reflect provides binding metadata; unsupported pass types produce editor-visible diagnostics instead of falling back to Vulkan silently.

**Tech Stack:** C++20, Objective-C++ `.mm`, SDL2 Metal views, Apple Metal/QuartzCore/CoreGraphics frameworks, Dear ImGui SDL2 platform backend plus a project-owned Metal renderer backend, SPIRV-Reflect, SPIRV-Cross, lodepng, CMake, vcpkg.

---

## Current Architecture Notes

- `app/src/main.cpp` hard-codes Vulkan through `<SDL2/SDL_vulkan.h>`, `SDL_WINDOW_VULKAN`, and `vulkan::create_vulkan_device(...)`.
- `include/vklive/IDevice.h` is already a useful backend boundary for scene init, render, ImGui render, present, idle, and output capture.
- `app/src/window_targets.cpp` breaks that boundary by casting `DeviceContext` to `VulkanContext` and reaching into `VulkanScene`.
- `CMakeLists.txt` always calls `find_package(Vulkan REQUIRED)`, always compiles `VULKAN_SOURCES`, and uses a Vulkan header as the `vklive` precompiled header.
- The current Vulkan backend compiles live GLSL to SPIR-V in `src/vulkan/vulkan_shader.cpp`, reflects descriptor sets with SPIRV-Reflect, and stores backend-specific objects in `VulkanScene`, `VulkanPass`, `VulkanSurface`, and `VulkanModel`.
- The Metal backend should first cover standard raster passes with vertex and fragment shaders. Metal support for Vulkan geometry shaders, Vulkan ray tracing shader stages, and the current Vulkan NanoVG scripted pass should be explicit follow-up work because each needs a separate design.

## File Structure

Modify app/backend boundary files:

- Modify `include/vklive/IDevice.h`: add backend identity and target-view queries.
- Create `include/vklive/render_backend.h`: backend enum and string helpers.
- Create `include/vklive/device_factory.h`: app-facing device creation API.
- Create `src/device_factory.cpp`: backend resolution and Vulkan/Metal factory dispatch.
- Modify `app/src/main.cpp`: remove direct Vulkan creation, use backend-aware SDL window flags and factory creation.
- Modify `app/src/window_targets.cpp`: consume `IDevice::TargetViews(...)` instead of Vulkan internals.
- Modify `app/include/app/command_line.h`, `app/src/command_line.cpp`, `tests/app_command_line_tests.cpp`: add `--renderer auto|vulkan|metal`.
- Modify `app/include/app/config.h`, `app/src/config.cpp`, `app/src/menu.cpp`: persist and display active/default renderer.

Modify build/dependency files:

- Modify `CMakeLists.txt`: make Vulkan optional on Apple, add `METAL_SOURCES`, link Apple frameworks, add SPIRV-Cross packages for Metal.
- Modify `prebuild.sh`: install `spirv-cross` on macOS and stop requiring SDL2 Vulkan support on the Metal-only macOS default.
- Modify `.github/workflows/builds.yml`: stop installing Vulkan SDK for the default macOS job and add an opt-in macOS Vulkan compatibility job if desired.

Create Metal backend files:

- Create `include/vklive/metal/metal_context.h`
- Create `include/vklive/metal/metal_device.h`
- Create `include/vklive/metal/metal_imgui.h`
- Create `include/vklive/metal/metal_shader.h`
- Create `include/vklive/metal/metal_scene.h`
- Create `include/vklive/metal/metal_surface.h`
- Create `include/vklive/metal/metal_pass.h`
- Create `include/vklive/metal/metal_model.h`
- Create `include/vklive/metal/metal_utils.h`
- Create `src/metal/metal_context.mm`
- Create `src/metal/metal_device.mm`
- Create `src/metal/metal_imgui.mm`
- Create `src/metal/metal_shader.mm`
- Create `src/metal/metal_scene.mm`
- Create `src/metal/metal_surface.mm`
- Create `src/metal/metal_pass.mm`
- Create `src/metal/metal_model.mm`
- Create `src/metal/metal_utils.mm`
- Create `src/metal/imgui_impl_metal_1917.h`
- Create `src/metal/imgui_impl_metal_1917.mm`

Create or expand tests:

- Create `tests/render_backend_tests.cpp`: pure C++ tests for renderer parsing/default resolution.
- Create `tests/metal_shader_tests.mm`: macOS-only shader compile/reflection tests.
- Modify `tests/scene_inspect.cpp` only if shared scene constraints need a test for Metal-unsupported stages.

## Scope Boundaries

In scope for this plan:

- macOS default backend is Metal.
- Windows and Linux continue to use Vulkan.
- macOS can optionally build Vulkan with `-DVKLIVE_ENABLE_VULKAN=ON`.
- Standard raster passes using `.vert` and `.frag` work in Metal.
- Render targets, texture sampling, default output, target preview, audio texture upload, model vertex/index buffers, material texture arrays, and PNG recording work in Metal.
- Geometry shader, ray tracing, and scripted NanoVG passes produce precise editor-visible errors on Metal.

Out of scope for this plan:

- Metal ray tracing shader support.
- Geometry shader emulation.
- Porting the Vulkan NanoVG scripted pass to a Metal vector renderer.
- Removing Vulkan support from non-Apple platforms.

---

### Task 1: Add Renderer Selection To CLI And Config

**Files:**
- Create: `include/vklive/render_backend.h`
- Modify: `app/include/app/command_line.h`
- Modify: `app/src/command_line.cpp`
- Modify: `app/include/app/config.h`
- Modify: `app/src/config.cpp`
- Test: `tests/app_command_line_tests.cpp`
- Test: `tests/render_backend_tests.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add backend enum and helpers**

Create `include/vklive/render_backend.h`:

```cpp
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
```

- [ ] **Step 2: Extend command-line options**

Update `app/include/app/command_line.h`:

```cpp
#include <vklive/render_backend.h>

struct AppCommandLineOptions
{
    fs::path projectRoot;
    fs::path sceneGraph;
    RenderBackend renderer = RenderBackend::Auto;
    bool smokeTest = false;
    bool help = false;
};
```

Update `app/src/command_line.cpp` help text:

```cpp
return R"(Usage:
  Rezonality [--project <dir>] [--scenegraph <file>] [--renderer <auto|vulkan|metal>] [--smoke-test]

Options:
  --project <dir>                 Load a project directory for this launch.
  --scenegraph <file>             Override the project's scenegraph for this launch.
  --renderer <auto|vulkan|metal>  Select the graphics backend for this launch.
  --smoke-test                    Start and exit immediately after command-line parsing.
)";
```

Add the parser branch:

```cpp
else if (arg == "--renderer")
{
    const char* value = requireValue(arg);
    if (!value)
    {
        return false;
    }
    if (!render_backend_from_string(value, options.renderer))
    {
        error = fmt::format("Unknown renderer: {}", value);
        return false;
    }
}
```

- [ ] **Step 3: Persist renderer preference**

Update `app/include/app/config.h`:

```cpp
#include <vklive/render_backend.h>

struct AppConfig
{
    bool vim_mode = false;
    fs::path project_root;
    RenderBackend renderer = RenderBackend::Auto;
    bool viewports = false;
    ...
};
```

Update `app/src/config.cpp` load/save:

```cpp
RenderBackend renderer = RenderBackend::Auto;
const auto rendererText = tbl["settings"]["renderer"].value_or("auto");
if (render_backend_from_string(rendererText, renderer))
{
    appConfig.renderer = renderer;
}
```

```cpp
settings.insert_or_assign("renderer", render_backend_to_string(appConfig.renderer));
```

- [ ] **Step 4: Add CLI tests**

Append to `tests/app_command_line_tests.cpp`:

```cpp
const char* rendererArgv[] = {
    "Rezonality.exe",
    "--renderer",
    "metal",
    "--smoke-test"
};
AppCommandLineOptions rendererOptions;
std::string rendererError;
bool rendererOk = app_parse_command_line(static_cast<int>(std::size(rendererArgv)), const_cast<char**>(rendererArgv), rendererOptions, rendererError);
ok &= require(rendererOk, "renderer parser failed: " + rendererError);
ok &= require(rendererOptions.renderer == RenderBackend::Metal, "metal renderer not parsed");
ok &= require(rendererOptions.smokeTest, "smoke-test not parsed after renderer");

const char* badRendererArgv[] = {
    "Rezonality.exe",
    "--renderer",
    "direct3d"
};
AppCommandLineOptions badRendererOptions;
std::string badRendererError;
bool badRendererOk = app_parse_command_line(static_cast<int>(std::size(badRendererArgv)), const_cast<char**>(badRendererArgv), badRendererOptions, badRendererError);
ok &= require(!badRendererOk, "bad renderer should fail");
ok &= require(badRendererError == "Unknown renderer: direct3d", "bad renderer error text changed");
```

Create `tests/render_backend_tests.cpp`:

```cpp
#include <cstdlib>
#include <iostream>
#include <string>

#include <vklive/render_backend.h>

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
}

int main()
{
    bool ok = true;
    RenderBackend backend = RenderBackend::Auto;
    ok &= require(render_backend_from_string("auto", backend) && backend == RenderBackend::Auto, "auto parse failed");
    ok &= require(render_backend_from_string("vulkan", backend) && backend == RenderBackend::Vulkan, "vulkan parse failed");
    ok &= require(render_backend_from_string("metal", backend) && backend == RenderBackend::Metal, "metal parse failed");
    ok &= require(!render_backend_from_string("Metal", backend), "renderer parsing should be lowercase and explicit");
    ok &= require(render_backend_to_string(RenderBackend::Metal) == "metal", "metal string conversion failed");
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
```

- [ ] **Step 5: Wire the new test target**

Add to `CMakeLists.txt` near the other tests:

```cmake
add_executable(vklive_render_backend_tests tests/render_backend_tests.cpp)
target_link_libraries(vklive_render_backend_tests PRIVATE vklive)
target_include_directories(vklive_render_backend_tests PRIVATE ${CMAKE_BINARY_DIR})

add_test(
    NAME vklive_render_backend_tests
    COMMAND vklive_render_backend_tests
)
```

- [ ] **Step 6: Run tests**

Run:

```bash
cmake --build build --config Debug --target vklive_app_command_line_tests vklive_render_backend_tests
ctest --test-dir build -C Debug -R "vklive_app_command_line_tests|vklive_render_backend_tests" --output-on-failure
```

Expected:

```text
100% tests passed
```

- [ ] **Step 7: Commit**

```bash
git add include/vklive/render_backend.h app/include/app/command_line.h app/src/command_line.cpp app/include/app/config.h app/src/config.cpp tests/app_command_line_tests.cpp tests/render_backend_tests.cpp CMakeLists.txt
git commit -m "feat: add renderer selection"
```

### Task 2: Add Device Factory And Backend-Aware Window Creation

**Files:**
- Create: `include/vklive/device_factory.h`
- Create: `src/device_factory.cpp`
- Modify: `app/src/main.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create the factory interface**

Create `include/vklive/device_factory.h`:

```cpp
#pragma once

#include <memory>
#include <string>

#include <vklive/IDevice.h>
#include <vklive/render_backend.h>

struct SDL_Window;

RenderBackend device_resolve_backend(RenderBackend requested);
uint32_t device_sdl_window_flags(RenderBackend backend);
std::shared_ptr<IDevice> device_create(RenderBackend backend, SDL_Window* pWindow, const std::string& settingsPath, bool viewports);
```

- [ ] **Step 2: Implement backend resolution**

Create `src/device_factory.cpp`:

```cpp
#include <stdexcept>

#include <SDL2/SDL.h>

#include <vklive/device_factory.h>

namespace vulkan
{
#if defined(VKLIVE_ENABLE_VULKAN)
std::shared_ptr<IDevice> create_vulkan_device(SDL_Window* pWindow, const std::string& settingsPath, bool viewports);
#endif
}

namespace metal
{
#if defined(VKLIVE_ENABLE_METAL)
std::shared_ptr<IDevice> create_metal_device(SDL_Window* pWindow, const std::string& settingsPath, bool viewports);
#endif
}

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
#if defined(__APPLE__)
        flags |= SDL_WINDOW_METAL;
        break;
#else
        throw std::runtime_error("Metal renderer is only available on Apple platforms");
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
```

- [ ] **Step 3: Update app startup**

In `app/src/main.cpp`, remove these lines:

```cpp
#include <SDL2/SDL_vulkan.h>
namespace vulkan
{
extern std::shared_ptr<IDevice> create_vulkan_device(SDL_Window* pWindow, const std::string& settingsPath, bool viewports = false);
}
```

Add:

```cpp
#include <vklive/device_factory.h>
```

Change `init_sdl_window()` to accept a backend:

```cpp
SDL_Window* init_sdl_window(RenderBackend backend)
{
    int xPos = (appConfig.main_window_pos.x == 0.0) ? SDL_WINDOWPOS_CENTERED : appConfig.main_window_pos.x;
    int yPos = (appConfig.main_window_pos.y == 0.0) ? SDL_WINDOWPOS_CENTERED : appConfig.main_window_pos.y;
    int xSize = (appConfig.main_window_size.x == 0.0) ? 1024 : appConfig.main_window_size.x;
    int ySize = (appConfig.main_window_size.y == 0.0) ? 768 : appConfig.main_window_size.y;

    auto windowFlags = device_sdl_window_flags(backend);
    if (appConfig.main_window_state == WindowState::Maximized)
    {
        windowFlags |= SDL_WINDOW_MAXIMIZED;
    }

    return SDL_CreateWindow("Rezonality", xPos, yPos, xSize, ySize, windowFlags);
}
```

After command-line config overrides:

```cpp
if (commandLineOptions.renderer != RenderBackend::Auto)
{
    appConfig.renderer = commandLineOptions.renderer;
}
const auto activeBackend = device_resolve_backend(appConfig.renderer);
```

Replace both `vulkan::create_vulkan_device(...)` calls:

```cpp
g_pDevice = device_create(activeBackend, init_sdl_window(activeBackend), imSettingsPath, appConfig.viewports);
```

- [ ] **Step 4: Add factory source to common sources**

In `CMakeLists.txt`, append `src/device_factory.cpp` and `include/vklive/device_factory.h` to `VK_SOURCES`.

- [ ] **Step 5: Build smoke target**

Run:

```bash
cmake --build build --config Debug --target Rezonality
```

Expected:

```text
Built target Rezonality
```

- [ ] **Step 6: Commit**

```bash
git add include/vklive/device_factory.h src/device_factory.cpp app/src/main.cpp CMakeLists.txt
git commit -m "feat: create devices through renderer factory"
```

### Task 3: Decouple Target Preview From Vulkan Internals

**Files:**
- Modify: `include/vklive/IDevice.h`
- Modify: `include/vklive/vulkan/vulkan_device.h`
- Modify: `src/vulkan/vulkan_device.cpp`
- Modify: `app/src/window_targets.cpp`
- Modify: `app/src/menu.cpp`

- [ ] **Step 1: Extend `IDevice` with target previews**

Add to `include/vklive/IDevice.h`:

```cpp
#include <string>

#include <vklive/render_backend.h>

struct RenderTargetView
{
    std::string name;
    ImTextureID textureId = 0;
    glm::uvec2 size = glm::uvec2(0);
};
```

Add virtual methods to `IDevice`:

```cpp
virtual RenderBackend Backend() const = 0;
virtual std::vector<RenderTargetView> TargetViews(Scene& scene) = 0;
```

- [ ] **Step 2: Implement Vulkan target views**

Add declarations to `include/vklive/vulkan/vulkan_device.h`:

```cpp
virtual RenderBackend Backend() const override;
virtual std::vector<RenderTargetView> TargetViews(Scene& scene) override;
```

Add to `src/vulkan/vulkan_device.cpp`:

```cpp
RenderBackend VulkanDevice::Backend() const
{
    return RenderBackend::Vulkan;
}

std::vector<RenderTargetView> VulkanDevice::TargetViews(Scene& scene)
{
    std::vector<RenderTargetView> views;
    auto pVulkanScene = vulkan_scene_get(ctx, scene);
    if (!pVulkanScene)
    {
        return views;
    }

    for (auto& target : pVulkanScene->viewableTargets)
    {
        auto itrTargetData = pVulkanScene->surfaces.find(target);
        if (itrTargetData == pVulkanScene->surfaces.end())
        {
            continue;
        }
        auto pSurf = itrTargetData->second;
        if (!pSurf->ImGuiDescriptorSet)
        {
            continue;
        }
        views.push_back(RenderTargetView{
            pSurf->debugName,
            imgui_texture_id(pSurf->ImGuiDescriptorSet),
            glm::uvec2(pSurf->extent.width, pSurf->extent.height)
        });
    }
    return views;
}
```

- [ ] **Step 3: Rewrite target window against `IDevice`**

Replace the Vulkan-specific body in `app/src/window_targets.cpp` with:

```cpp
#include "app/menu.h"
#include "app/window_targets.h"

#include <algorithm>

#include <zest/imgui/imgui.h>

#include <vklive/IDevice.h>

extern IDevice* GetDevice();

void window_targets(Scene& scene)
{
    if (!g_WindowEnables.targets)
    {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(820, 50), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Targets", &g_WindowEnables.targets))
    {
        auto pDrawList = ImGui::GetWindowDrawList();
        ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
        ImVec2 canvas_size = ImGui::GetContentRegionAvail();
        canvas_size.x = std::max(canvas_size.x, 1.0f);
        canvas_size.y = std::max(canvas_size.y, 1.0f);
        ImGui::InvisibleButton("##dummy", canvas_size);

        auto minRect = pDrawList->GetClipRectMin();
        auto maxRect = pDrawList->GetClipRectMax();
        canvas_pos = minRect;
        canvas_size = ImVec2(maxRect.x - minRect.x, maxRect.y - minRect.y);

        const auto targetViews = scene.valid && GetDevice() ? GetDevice()->TargetViews(scene) : std::vector<RenderTargetView>{};
        if (!targetViews.empty())
        {
            const auto heightPerTile = canvas_size.y / static_cast<float>(targetViews.size());
            const auto fontSize = ImGui::GetFontSize();
            for (const auto& view : targetViews)
            {
                const auto ySize = heightPerTile;
                pDrawList->AddImage(view.textureId,
                    ImVec2(canvas_pos.x, canvas_pos.y),
                    ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + ySize - fontSize));
                pDrawList->AddText(ImVec2(canvas_pos.x, canvas_pos.y + ySize - fontSize), 0xFFFFFFFF, view.name.c_str());
                canvas_pos.y += ySize;
            }
        }
        else
        {
            pDrawList->AddText(ImVec2(canvas_pos.x, canvas_pos.y), 0xFFFFFFFF, "No targets...");
        }
    }
    ImGui::End();
}
```

- [ ] **Step 4: Remove stale Vulkan include from menu**

Remove this include from `app/src/menu.cpp` because that file only uses `IDevice`:

```cpp
#include <vklive/vulkan/vulkan_context.h>
```

- [ ] **Step 5: Build**

Run:

```bash
cmake --build build --config Debug --target Rezonality
```

Expected:

```text
Built target Rezonality
```

- [ ] **Step 6: Commit**

```bash
git add include/vklive/IDevice.h include/vklive/vulkan/vulkan_device.h src/vulkan/vulkan_device.cpp app/src/window_targets.cpp app/src/menu.cpp
git commit -m "refactor: make target previews backend-neutral"
```

### Task 4: Make Vulkan Optional And Add Metal Build Wiring

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `prebuild.sh`
- Modify: `.github/workflows/builds.yml`

- [ ] **Step 1: Add backend CMake options**

Near the top of `CMakeLists.txt`, after `project(...)`:

```cmake
option(VKLIVE_ENABLE_METAL "Build the Metal renderer on Apple platforms" ${APPLE})
if(APPLE)
    option(VKLIVE_ENABLE_VULKAN "Build the Vulkan renderer" OFF)
else()
    option(VKLIVE_ENABLE_VULKAN "Build the Vulkan renderer" ON)
endif()

if(VKLIVE_ENABLE_METAL AND NOT APPLE)
    message(FATAL_ERROR "VKLIVE_ENABLE_METAL requires an Apple platform")
endif()
```

- [ ] **Step 2: Guard Vulkan package and definitions**

Replace:

```cmake
find_package(Vulkan REQUIRED)
```

with:

```cmake
if(VKLIVE_ENABLE_VULKAN)
    find_package(Vulkan REQUIRED)
    add_definitions(-DVULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1)
endif()
```

Remove the existing unconditional `add_definitions(-DVULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1)`.

- [ ] **Step 3: Add SPIRV-Cross and Metal frameworks only for Metal**

Add:

```cmake
if(VKLIVE_ENABLE_METAL)
    enable_language(OBJCXX)
    find_package(spirv_cross_core CONFIG REQUIRED)
    find_package(spirv_cross_msl CONFIG REQUIRED)
    find_package(spirv_cross_cpp CONFIG REQUIRED)
    find_library(APPLE_METAL_FRAMEWORK Metal REQUIRED)
    find_library(APPLE_METALKIT_FRAMEWORK MetalKit REQUIRED)
    find_library(APPLE_QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
    find_library(APPLE_FOUNDATION_FRAMEWORK Foundation REQUIRED)
endif()
```

- [ ] **Step 4: Add Metal source list**

Add after `VULKAN_SOURCES`:

```cmake
set(METAL_SOURCES
    src/metal/metal_context.mm
    src/metal/metal_device.mm
    src/metal/metal_imgui.mm
    src/metal/metal_shader.mm
    src/metal/metal_scene.mm
    src/metal/metal_surface.mm
    src/metal/metal_pass.mm
    src/metal/metal_model.mm
    src/metal/metal_utils.mm
    src/metal/imgui_impl_metal_1917.mm

    include/vklive/metal/metal_context.h
    include/vklive/metal/metal_device.h
    include/vklive/metal/metal_imgui.h
    include/vklive/metal/metal_shader.h
    include/vklive/metal/metal_scene.h
    include/vklive/metal/metal_surface.h
    include/vklive/metal/metal_pass.h
    include/vklive/metal/metal_model.h
    include/vklive/metal/metal_utils.h
    src/metal/imgui_impl_metal_1917.h
)
```

Change `LIB_SOURCES`:

```cmake
set(LIB_SOURCES ${VK_SOURCES})
if(VKLIVE_ENABLE_VULKAN)
    list(APPEND LIB_SOURCES ${VULKAN_SOURCES})
endif()
if(VKLIVE_ENABLE_METAL)
    list(APPEND LIB_SOURCES ${METAL_SOURCES})
endif()
```

- [ ] **Step 5: Guard Vulkan and Metal link libraries**

Change the `target_link_libraries(vklive PUBLIC ...)` block so `Vulkan::Vulkan` is not unconditional:

```cmake
target_link_libraries(vklive
    PUBLIC
        reproc++
        assimp::assimp
        fmt::fmt-header-only
        range-v3
        range-v3-meta
        range-v3::meta
        range-v3-concepts
        Zing::Zing
        lodepng
)

if(VKLIVE_ENABLE_VULKAN)
    target_link_libraries(vklive PUBLIC Vulkan::Vulkan)
    target_compile_definitions(vklive PUBLIC VKLIVE_ENABLE_VULKAN=1)
    target_precompile_headers(vklive PRIVATE include/vklive/vulkan/vulkan_context.h)
endif()

if(VKLIVE_ENABLE_METAL)
    target_link_libraries(vklive
        PUBLIC
            spirv-cross-core
            spirv-cross-msl
            spirv-cross-cpp
            ${APPLE_METAL_FRAMEWORK}
            ${APPLE_METALKIT_FRAMEWORK}
            ${APPLE_QUARTZCORE_FRAMEWORK}
            ${APPLE_FOUNDATION_FRAMEWORK}
    )
    target_compile_definitions(vklive PUBLIC VKLIVE_ENABLE_METAL=1)
    set_source_files_properties(${METAL_SOURCES} PROPERTIES COMPILE_FLAGS "-fobjc-arc")
endif()
```

- [ ] **Step 6: Update Unix prebuild dependencies**

Change the package line in `prebuild.sh` to choose SDL2 features by platform:

```bash
common_packages=(lodepng minizip tsl-ordered-map ableton-link cppcodec range-v3 portaudio stb gli reproc fmt nativefiledialog tinyfiledialogs clipp concurrentqueue assimp glm tinydir spirv-reflect)
if [ "$(uname)" == "Darwin" ]; then
    graphics_packages=(sdl2 spirv-cross)
else
    graphics_packages=(vulkan-memory-allocator sdl2[vulkan])
fi

./vcpkg install "${common_packages[@]}" "${graphics_packages[@]}" --triplet ${triplet[0]} --recurse
```

- [ ] **Step 7: Update macOS CI default**

In `.github/workflows/builds.yml`, change the Vulkan SDK install step condition:

```yaml
if: matrix.os != 'macos-latest'
```

Add this to the macOS configure command through the matrix, or rely on the CMake defaults:

```yaml
- { title: "Mac", os: "macos-latest", cc: "clang-15", cx: "clang++-15", arch: "x64", build_type: "Release", renderer: "metal" }
- { title: "Mac", os: "macos-latest", cc: "clang-15", cx: "clang++-15", arch: "x64", build_type: "Debug", renderer: "metal" }
```

Use the existing configure line with:

```bash
cmake $GITHUB_WORKSPACE -DLIBCXX_ENABLE_INCOMPLETE_FEATURES=ON -DCMAKE_BUILD_TYPE=${{ matrix.build_type }}
```

- [ ] **Step 8: Configure both default platforms**

Run on macOS:

```bash
./prebuild.sh
./config.sh Debug
cmake --build build --config Debug --target vklive_render_backend_tests
```

Expected:

```text
Built target vklive_render_backend_tests
```

Run on a Vulkan platform:

```bash
./config.sh Debug
cmake --build build --config Debug --target Rezonality
```

Expected:

```text
Built target Rezonality
```

- [ ] **Step 9: Commit**

```bash
git add CMakeLists.txt prebuild.sh .github/workflows/builds.yml
git commit -m "build: make mac renderer default to Metal"
```

### Task 5: Add Metal Context, Device, Window, And ImGui Bootstrap

**Files:**
- Create: `include/vklive/metal/metal_context.h`
- Create: `include/vklive/metal/metal_device.h`
- Create: `include/vklive/metal/metal_imgui.h`
- Create: `src/metal/metal_context.mm`
- Create: `src/metal/metal_device.mm`
- Create: `src/metal/metal_imgui.mm`
- Create: `src/metal/imgui_impl_metal_1917.h`
- Create: `src/metal/imgui_impl_metal_1917.mm`

- [ ] **Step 1: Add Metal context structures**

Create `include/vklive/metal/metal_context.h`:

```cpp
#pragma once

#include <memory>
#include <unordered_map>

#include <SDL2/SDL.h>
#include <SDL2/SDL_metal.h>

#include <vklive/IDevice.h>

struct Scene;

namespace metal
{

struct MetalScene;
using ObjCHandle = void*;

struct MetalContext : DeviceContext
{
    SDL_MetalView metalView = nullptr;
    ObjCHandle layer = nullptr;
    ObjCHandle device = nullptr;
    ObjCHandle commandQueue = nullptr;
    ObjCHandle frameCommandBuffer = nullptr;
    ObjCHandle currentDrawable = nullptr;
    glm::uvec2 drawableSize = glm::uvec2(0);
    std::unordered_map<Scene*, std::shared_ptr<MetalScene>> mapMetalScene;
};

bool context_init(MetalContext& ctx);
void context_destroy(MetalContext& ctx);
bool context_begin_frame(MetalContext& ctx);
void context_present(MetalContext& ctx);

} // namespace metal
```

- [ ] **Step 2: Implement Metal context**

Create `src/metal/metal_context.mm`:

```objc
#include <stdexcept>

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <zest/logger/logger.h>

#include <vklive/metal/metal_context.h>

namespace metal
{

bool context_init(MetalContext& ctx)
{
    ctx.metalView = SDL_Metal_CreateView(ctx.window);
    if (!ctx.metalView)
    {
        throw std::runtime_error(SDL_GetError());
    }

    ctx.layer = SDL_Metal_GetLayer(ctx.metalView);
    ctx.device = (__bridge_retained void*)MTLCreateSystemDefaultDevice();
    if (!ctx.device)
    {
        throw std::runtime_error("Metal is not supported on this Mac");
    }

    auto layer = (__bridge CAMetalLayer*)ctx.layer;
    auto device = (__bridge id<MTLDevice>)ctx.device;
    layer.device = device;
    layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly = NO;
    ctx.commandQueue = (__bridge_retained void*)[device newCommandQueue];
    return ctx.commandQueue != nullptr;
}

void context_destroy(MetalContext& ctx)
{
    ctx.currentDrawable = nullptr;
    ctx.frameCommandBuffer = nullptr;
    if (ctx.commandQueue)
    {
        CFRelease(ctx.commandQueue);
        ctx.commandQueue = nullptr;
    }
    if (ctx.device)
    {
        CFRelease(ctx.device);
        ctx.device = nullptr;
    }
    if (ctx.metalView)
    {
        SDL_Metal_DestroyView(ctx.metalView);
        ctx.metalView = nullptr;
    }
}

bool context_begin_frame(MetalContext& ctx)
{
    int drawableWidth = 0;
    int drawableHeight = 0;
    SDL_Metal_GetDrawableSize(ctx.window, &drawableWidth, &drawableHeight);
    ctx.drawableSize = glm::uvec2(std::max(drawableWidth, 0), std::max(drawableHeight, 0));
    ctx.frameBufferSize = ctx.drawableSize;
    if (ctx.drawableSize.x == 0 || ctx.drawableSize.y == 0)
    {
        return false;
    }

    auto layer = (__bridge CAMetalLayer*)ctx.layer;
    layer.drawableSize = CGSizeMake(ctx.drawableSize.x, ctx.drawableSize.y);
    ctx.currentDrawable = (__bridge_retained void*)[layer nextDrawable];
    ctx.frameCommandBuffer = (__bridge_retained void*)[(__bridge id<MTLCommandQueue>)ctx.commandQueue commandBuffer];
    return ctx.currentDrawable && ctx.frameCommandBuffer;
}

void context_present(MetalContext& ctx)
{
    if (!ctx.frameCommandBuffer || !ctx.currentDrawable)
    {
        return;
    }
    [(__bridge id<MTLCommandBuffer>)ctx.frameCommandBuffer presentDrawable:(__bridge id<CAMetalDrawable>)ctx.currentDrawable];
    [(__bridge id<MTLCommandBuffer>)ctx.frameCommandBuffer commit];
    CFRelease(ctx.frameCommandBuffer);
    CFRelease(ctx.currentDrawable);
    ctx.frameCommandBuffer = nullptr;
    ctx.currentDrawable = nullptr;
}

} // namespace metal
```

- [ ] **Step 3: Add Metal device wrapper**

Create `include/vklive/metal/metal_device.h`:

```cpp
#pragma once

#include <vklive/IDevice.h>
#include <vklive/metal/metal_context.h>

struct SDL_Window;

namespace metal
{

struct MetalDevice : public IDevice
{
    MetalDevice(SDL_Window* pWindow, const std::string& iniPath, bool viewports = false);
    ~MetalDevice();

    void InitScene(Scene& scene) override;
    void DestroyScene(Scene& scene) override;
    void ImGui_Render(ImDrawData* pDrawData) override;
    RenderOutput Render_3D(Scene& scene, const glm::vec2& size) override;
    void WriteToFile(Scene& scene, const fs::path& path) override;
    void WaitIdle() override;
    void ValidateSwapChain() override;
    void Present() override;
    std::string GetDeviceString() const override;
    std::set<std::string> ShaderFileExtensions() override;
    DeviceContext& Context() override;
    RenderBackend Backend() const override;
    std::vector<RenderTargetView> TargetViews(Scene& scene) override;

    MetalContext ctx;
};

std::shared_ptr<IDevice> create_metal_device(SDL_Window* pWindow, const std::string& iniPath, bool viewports = false);

} // namespace metal
```

Create `src/metal/metal_device.mm`:

```objc
#import <Metal/Metal.h>

#include <sstream>

#include <zest/imgui/imgui.h>
#include <zest/imgui/imgui_impl_sdl2.h>

#include <vklive/scene.h>
#include <vklive/metal/metal_device.h>
#include <vklive/metal/metal_imgui.h>
#include <vklive/metal/metal_scene.h>

namespace metal
{

std::shared_ptr<IDevice> create_metal_device(SDL_Window* pWindow, const std::string& iniPath, bool viewports)
{
    return std::static_pointer_cast<IDevice>(std::make_shared<MetalDevice>(pWindow, iniPath, viewports));
}

MetalDevice::MetalDevice(SDL_Window* pWindow, const std::string& iniPath, bool viewports)
{
    ctx.window = pWindow;

    float ddpi = 0.0f;
    const auto dpi = SDL_GetDisplayDPI(SDL_GetWindowDisplayIndex(pWindow), &ddpi, &ctx.hdpi, &ctx.vdpi);
    ctx.vdpi = dpi ? 1.0f : ctx.vdpi / 96.0f;

    context_init(ctx);
    imgui_init(ctx, iniPath, viewports);
}

MetalDevice::~MetalDevice()
{
    WaitIdle();
    imgui_shutdown(ctx);
    context_destroy(ctx);
    SDL_DestroyWindow(ctx.window);
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
}

void MetalDevice::InitScene(Scene& scene)
{
    metal_scene_create(ctx, scene);
}

void MetalDevice::DestroyScene(Scene& scene)
{
    if (auto pMetalScene = metal_scene_get(ctx, scene))
    {
        metal_scene_destroy(ctx, *pMetalScene);
    }
}

RenderOutput MetalDevice::Render_3D(Scene& scene, const glm::vec2& size)
{
    metal_scene_render(ctx, scene, glm::uvec2(size));
    return metal_scene_get_output(ctx, scene);
}

void MetalDevice::ImGui_Render(ImDrawData* pDrawData)
{
    if (!context_begin_frame(ctx))
    {
        return;
    }
    imgui_render(ctx, pDrawData);
}

void MetalDevice::WriteToFile(Scene& scene, const fs::path& path)
{
    if ((scene.GlobalFrameCount < scene.maxRecordFrame) && scene.recording)
    {
        metal_scene_write_output(ctx, scene, path);
    }
    else
    {
        scene.recording = false;
    }
}

void MetalDevice::WaitIdle()
{
    if (ctx.frameCommandBuffer)
    {
        [(__bridge id<MTLCommandBuffer>)ctx.frameCommandBuffer waitUntilCompleted];
    }
}

void MetalDevice::ValidateSwapChain()
{
}

void MetalDevice::Present()
{
    context_present(ctx);
}

DeviceContext& MetalDevice::Context()
{
    return ctx;
}

RenderBackend MetalDevice::Backend() const
{
    return RenderBackend::Metal;
}

std::set<std::string> MetalDevice::ShaderFileExtensions()
{
    return { ".fs", ".vs", ".frag", ".vert" };
}

std::string MetalDevice::GetDeviceString() const
{
    std::ostringstream str;
    str << "Renderer: Metal\n";
    str << "Device Name: " << [[(__bridge id<MTLDevice>)ctx.device name] UTF8String] << "\n";
    return str.str();
}

std::vector<RenderTargetView> MetalDevice::TargetViews(Scene& scene)
{
    return metal_scene_target_views(ctx, scene);
}

} // namespace metal
```

- [ ] **Step 4: Add ImGui Metal renderer source**

Copy Dear ImGui's Metal backend files matching the vendored Zest ImGui version:

```bash
curl -L -o src/metal/imgui_impl_metal_1917.h https://raw.githubusercontent.com/ocornut/imgui/v1.91.7-docking/backends/imgui_impl_metal.h
curl -L -o src/metal/imgui_impl_metal_1917.mm https://raw.githubusercontent.com/ocornut/imgui/v1.91.7-docking/backends/imgui_impl_metal.mm
```

Then edit both copied files so their includes use the vendored Zest ImGui headers:

```cpp
#include <zest/imgui/imgui.h>
```

- [ ] **Step 5: Add Metal ImGui wrapper**

Create `include/vklive/metal/metal_imgui.h`:

```cpp
#pragma once

#include <string>

struct ImDrawData;

namespace metal
{
struct MetalContext;

void imgui_init(MetalContext& ctx, const std::string& iniPath, bool viewports);
void imgui_shutdown(MetalContext& ctx);
void imgui_render(MetalContext& ctx, ImDrawData* drawData);

} // namespace metal
```

Create `src/metal/metal_imgui.mm`:

```objc
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <stdexcept>

#include <zest/imgui/imgui.h>
#include <zest/imgui/imgui_impl_sdl2.h>

#include "imgui_impl_metal_1917.h"
#include <vklive/metal/metal_context.h>
#include <vklive/metal/metal_imgui.h>

namespace metal
{

void imgui_init(MetalContext& ctx, const std::string& iniPath, bool viewports)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = iniPath.c_str();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    if (viewports)
    {
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    }

    ImGui_ImplSDL2_InitForMetal(ctx.window);
    ImGui_ImplMetal_Init((__bridge id<MTLDevice>)ctx.device);
}

void imgui_shutdown(MetalContext& ctx)
{
    ImGui_ImplMetal_Shutdown();
}

void imgui_render(MetalContext& ctx, ImDrawData* drawData)
{
    auto drawable = (__bridge id<CAMetalDrawable>)ctx.currentDrawable;
    auto commandBuffer = (__bridge id<MTLCommandBuffer>)ctx.frameCommandBuffer;
    if (!drawable || !commandBuffer)
    {
        return;
    }

    MTLRenderPassDescriptor* passDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
    passDescriptor.colorAttachments[0].texture = drawable.texture;
    passDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
    passDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
    passDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0.45, 0.55, 0.60, 1.00);

    id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:passDescriptor];
    ImGui_ImplMetal_RenderDrawData(drawData, commandBuffer, encoder);
    [encoder endEncoding];
}

} // namespace metal
```

- [ ] **Step 6: Configure and build macOS Metal shell**

Run:

```bash
./config.sh Debug
cmake --build build --config Debug --target Rezonality
```

Expected:

```text
Built target Rezonality
```

- [ ] **Step 7: Run smoke test with Metal**

Run:

```bash
./build/Rezonality.app/Contents/MacOS/Rezonality --renderer metal --smoke-test
```

Expected:

```text
```

The command exits with status `0`.

- [ ] **Step 8: Commit**

```bash
git add include/vklive/metal/metal_context.h include/vklive/metal/metal_device.h include/vklive/metal/metal_imgui.h src/metal/metal_context.mm src/metal/metal_device.mm src/metal/metal_imgui.mm src/metal/imgui_impl_metal_1917.h src/metal/imgui_impl_metal_1917.mm
git commit -m "feat: bootstrap Metal device and ImGui renderer"
```

### Task 6: Add Metal Shader Compilation And Reflection

**Files:**
- Create: `include/vklive/metal/metal_shader.h`
- Create: `src/metal/metal_shader.mm`
- Create: `tests/metal_shader_tests.mm`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Define Metal shader structures**

Create `include/vklive/metal/metal_shader.h`:

```cpp
#pragma once

#include <map>
#include <memory>
#include <string>

#include <vklive/scene.h>
#include <vklive/vulkan/vulkan_bindings.h>

namespace metal
{
struct MetalContext;
struct MetalScene;

enum class MetalShaderStage
{
    Vertex,
    Fragment
};

struct MetalShader
{
    explicit MetalShader(Shader* pS)
        : pShader(pS)
    {
    }

    Shader* pShader = nullptr;
    MetalShaderStage stage = MetalShaderStage::Vertex;
    BindingSets bindingSets;
    std::string mslSource;
    void* library = nullptr;
    void* function = nullptr;
};

std::shared_ptr<MetalShader> metal_shader_create(MetalContext& ctx, MetalScene& scene, Shader& shader);
void metal_shader_destroy(MetalContext& ctx, MetalShader& shader);

} // namespace metal
```

- [ ] **Step 2: Convert GLSL to MSL**

Create the compiler body in `src/metal/metal_shader.mm`:

```objc
#import <Metal/Metal.h>

#include <fmt/format.h>
#include <spirv_cross/spirv_msl.hpp>
#include <spirv-reflect/spirv_reflect.h>

#include <zest/file/file.h>
#include <zest/file/runtree.h>

#include <vklive/process/process.h>
#include <vklive/metal/metal_context.h>
#include <vklive/metal/metal_scene.h>
#include <vklive/metal/metal_shader.h>
#include <vklive/validation.h>

namespace metal
{

namespace
{
bool stage_from_path(const fs::path& path, MetalShaderStage& stage, Scene& scene)
{
    if (path.extension() == ".vert" || path.extension() == ".vs")
    {
        stage = MetalShaderStage::Vertex;
        return true;
    }
    if (path.extension() == ".frag" || path.extension() == ".fs")
    {
        stage = MetalShaderStage::Fragment;
        return true;
    }
    scene_report_error(scene, MessageSeverity::Error,
        fmt::format("Metal supports vertex and fragment shaders in this renderer. Unsupported shader: {}", path.filename().string()),
        path);
    return false;
}

std::vector<uint32_t> read_spirv_words(const fs::path& path)
{
    const auto bytes = Zest::file_read(path);
    std::vector<uint32_t> words(bytes.size() / sizeof(uint32_t));
    std::memcpy(words.data(), bytes.data(), words.size() * sizeof(uint32_t));
    return words;
}

bool reflect_bindings(const std::vector<uint32_t>& spirv, MetalShader& shader)
{
    SpvReflectShaderModule module = {};
    const auto result = spvReflectCreateShaderModule(spirv.size() * sizeof(uint32_t), spirv.data(), &module);
    if (result != SPV_REFLECT_RESULT_SUCCESS)
    {
        return false;
    }

    uint32_t count = 0;
    spvReflectEnumerateDescriptorSets(&module, &count, nullptr);
    std::vector<SpvReflectDescriptorSet*> sets(count);
    spvReflectEnumerateDescriptorSets(&module, &count, sets.data());

    for (auto* set : sets)
    {
        for (uint32_t index = 0; index < set->binding_count; ++index)
        {
            auto& reflected = *set->bindings[index];
            VulkanBindingMeta meta;
            meta.name = reflected.name;
            meta.shaderPath = shader.pShader->path;
            shader.bindingSets[set->set].bindingMeta[reflected.binding] = meta;

            vk::DescriptorSetLayoutBinding layoutBinding;
            layoutBinding.binding = reflected.binding;
            layoutBinding.descriptorType = static_cast<vk::DescriptorType>(reflected.descriptor_type);
            layoutBinding.descriptorCount = 1;
            for (uint32_t dim = 0; dim < reflected.array.dims_count; ++dim)
            {
                layoutBinding.descriptorCount *= reflected.array.dims[dim];
            }
            shader.bindingSets[set->set].bindings[reflected.binding] = layoutBinding;
        }
    }

    spvReflectDestroyShaderModule(&module);
    return true;
}

std::string compile_spirv_to_msl(const std::vector<uint32_t>& spirv)
{
    spirv_cross::CompilerMSL compiler(spirv);
    auto options = compiler.get_msl_options();
    options.platform = spirv_cross::CompilerMSL::Options::macOS;
    options.msl_version = spirv_cross::CompilerMSL::Options::make_msl_version(2, 4);
    options.argument_buffers = false;
    compiler.set_msl_options(options);

    spirv_cross::CompilerMSL::ResourceBinding uniformBinding;
    uniformBinding.desc_set = 0;
    uniformBinding.binding = 0;
    uniformBinding.msl_buffer = 0;
    compiler.add_msl_resource_binding(uniformBinding);

    return compiler.compile();
}
}

std::shared_ptr<MetalShader> metal_shader_create(MetalContext& ctx, MetalScene& metalScene, Shader& shader)
{
    auto spShader = std::make_shared<MetalShader>(&shader);
    if (!stage_from_path(shader.path, spShader->stage, *metalScene.pScene))
    {
        return nullptr;
    }

    auto outPath = fs::temp_directory_path() / "vklive";
    fs::create_directories(outPath);
    outPath = outPath / (shader.path.filename().string() + ".spirv");

#if defined(__APPLE__)
    const auto compilerPath = Zest::runtree_find_path("bin/mac/glslangValidator");
#else
    const auto compilerPath = fs::path();
#endif

    std::vector<std::string> args{
        compilerPath.string(),
        "-V",
        shader.path.string(),
        "-o",
        outPath.string(),
        "-l",
        "-g",
        fmt::format("-I{}", fs::canonical(shader.path.parent_path()).string()),
        fmt::format("-I{}", fs::canonical(Zest::runtree_path() / "shaders/include").string()),
    };

    std::string output;
    const auto ret = run_process(args, &output);
    if (ret)
    {
        scene_report_error(*metalScene.pScene, MessageSeverity::Error, "Could not run glslangValidator for Metal", shader.path);
        return nullptr;
    }
    if (shader_parse_output(output, shader.path, *metalScene.pScene))
    {
        return nullptr;
    }

    const auto spirv = read_spirv_words(outPath);
    if (spirv.empty())
    {
        scene_report_error(*metalScene.pScene, MessageSeverity::Error, fmt::format("Could not get SPIR-V for shader: {}", shader.path.filename().string()), shader.path);
        return nullptr;
    }
    if (!reflect_bindings(spirv, *spShader))
    {
        scene_report_error(*metalScene.pScene, MessageSeverity::Error, fmt::format("Could not reflect shader: {}", shader.path.filename().string()), shader.path);
        return nullptr;
    }

    spShader->mslSource = compile_spirv_to_msl(spirv);
    NSError* error = nil;
    NSString* source = [NSString stringWithUTF8String:spShader->mslSource.c_str()];
    spShader->library = (__bridge_retained void*)[(__bridge id<MTLDevice>)ctx.device newLibraryWithSource:source options:nil error:&error];
    if (!spShader->library)
    {
        scene_report_error(*metalScene.pScene, MessageSeverity::Error, [[error localizedDescription] UTF8String], shader.path);
        return nullptr;
    }
    spShader->function = (__bridge_retained void*)[(__bridge id<MTLLibrary>)spShader->library newFunctionWithName:@"main0"];
    if (!spShader->function)
    {
        scene_report_error(*metalScene.pScene, MessageSeverity::Error, fmt::format("Could not create Metal function for {}", shader.path.filename().string()), shader.path);
        return nullptr;
    }
    metalScene.shaderStages[shader.path] = spShader;
    return spShader;
}

void metal_shader_destroy(MetalContext& ctx, MetalShader& shader)
{
    if (shader.function)
    {
        CFRelease(shader.function);
        shader.function = nullptr;
    }
    if (shader.library)
    {
        CFRelease(shader.library);
        shader.library = nullptr;
    }
}

} // namespace metal
```

Add a non-static declaration for `shader_parse_output` to a shared header in this task by moving it out of `src/vulkan/vulkan_shader.cpp` into `include/vklive/shader_compiler.h` and `src/shader_compiler.cpp`. Both Vulkan and Metal should call the same function so compiler diagnostics stay identical.

- [ ] **Step 3: Add macOS shader test**

Create `tests/metal_shader_tests.mm`:

```objc
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

#include <spirv_cross/spirv_msl.hpp>

#include <config_app.h>
#include <vklive/process/process.h>

namespace fs = std::filesystem;

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

std::vector<uint32_t> read_spirv_words(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    std::vector<char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::vector<uint32_t> words(bytes.size() / sizeof(uint32_t));
    std::memcpy(words.data(), bytes.data(), words.size() * sizeof(uint32_t));
    return words;
}
}

int main()
{
#if defined(__APPLE__) && defined(VKLIVE_ENABLE_METAL)
    bool ok = true;
    const fs::path tempDir = fs::temp_directory_path() / "vklive-metal-shader-test";
    fs::create_directories(tempDir);
    const fs::path shaderPath = tempDir / "test.frag";
    const fs::path spirvPath = tempDir / "test.frag.spirv";

    {
        std::ofstream out(shaderPath);
        out << "#version 450\n"
               "layout(location = 0) out vec4 outColor;\n"
               "void main() { outColor = vec4(1.0, 0.0, 0.0, 1.0); }\n";
    }

    std::string output;
    const auto compilerPath = fs::path(VKLIVE_ROOT) / "run_tree/bin/mac/glslangValidator";
    const std::vector<std::string> args{
        compilerPath.string(),
        "-V",
        shaderPath.string(),
        "-o",
        spirvPath.string()
    };
    ok &= require(run_process(args, &output) == 0, "glslangValidator failed: " + output);

    const auto spirv = read_spirv_words(spirvPath);
    ok &= require(!spirv.empty(), "SPIR-V output was empty");

    spirv_cross::CompilerMSL compiler(spirv);
    auto options = compiler.get_msl_options();
    options.platform = spirv_cross::CompilerMSL::Options::macOS;
    options.msl_version = spirv_cross::CompilerMSL::Options::make_msl_version(2, 4);
    compiler.set_msl_options(options);
    const auto msl = compiler.compile();
    ok &= require(msl.find("fragment") != std::string::npos, "MSL output did not contain a fragment entry");
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
#else
    return EXIT_SUCCESS;
#endif
}
```

This test proves the two moving parts the Metal shader path depends on: the packaged macOS `glslangValidator` can emit SPIR-V, and SPIRV-Cross can emit MSL from that SPIR-V.

- [ ] **Step 4: Wire the test target**

Add to `CMakeLists.txt`:

```cmake
if(VKLIVE_ENABLE_METAL)
    add_executable(vklive_metal_shader_tests tests/metal_shader_tests.mm)
    target_link_libraries(vklive_metal_shader_tests PRIVATE vklive)
    target_include_directories(vklive_metal_shader_tests PRIVATE ${CMAKE_BINARY_DIR})
    add_test(NAME vklive_metal_shader_tests COMMAND vklive_metal_shader_tests)
endif()
```

- [ ] **Step 5: Build**

Run:

```bash
cmake --build build --config Debug --target vklive_metal_shader_tests
ctest --test-dir build -C Debug -R vklive_metal_shader_tests --output-on-failure
```

Expected:

```text
100% tests passed
```

- [ ] **Step 6: Commit**

```bash
git add include/vklive/metal/metal_shader.h src/metal/metal_shader.mm include/vklive/shader_compiler.h src/shader_compiler.cpp src/vulkan/vulkan_shader.cpp tests/metal_shader_tests.mm CMakeLists.txt
git commit -m "feat: compile live GLSL shaders for Metal"
```

### Task 7: Add Metal Scene, Surface, Model, And Pass State

**Files:**
- Create: `include/vklive/metal/metal_scene.h`
- Create: `include/vklive/metal/metal_surface.h`
- Create: `include/vklive/metal/metal_model.h`
- Create: `include/vklive/metal/metal_pass.h`
- Create: `include/vklive/metal/metal_utils.h`
- Create: `src/metal/metal_scene.mm`
- Create: `src/metal/metal_surface.mm`
- Create: `src/metal/metal_model.mm`
- Create: `src/metal/metal_pass.mm`
- Create: `src/metal/metal_utils.mm`

- [ ] **Step 1: Define Metal backend data**

Create `include/vklive/metal/metal_scene.h`:

```cpp
#pragma once

#include <map>
#include <memory>
#include <set>
#include <unordered_map>

#include <vklive/IDevice.h>
#include <vklive/scene.h>

namespace metal
{

struct MetalPass;
struct MetalShader;
struct MetalSurface;
struct MetalModel;
struct MetalContext;

struct SurfaceKey
{
    std::string targetName;
    uint64_t pingPongIndex = 0;

    bool operator==(const SurfaceKey& other) const
    {
        return targetName == other.targetName && pingPongIndex == other.pingPongIndex;
    }

    bool operator<(const SurfaceKey& other) const
    {
        return targetName == other.targetName ? pingPongIndex < other.pingPongIndex : targetName < other.targetName;
    }

    std::string DebugName() const;

    struct HashFunction
    {
        size_t operator()(const SurfaceKey& key) const
        {
            return std::hash<std::string>()(key.targetName) ^ key.pingPongIndex;
        }
    };
};

struct PathHash
{
    size_t operator()(const fs::path& path) const noexcept
    {
        return fs::hash_value(path);
    }
};

struct MetalScene
{
    explicit MetalScene(Scene* pS)
        : pScene(pS)
    {
    }

    Scene* pScene = nullptr;
    std::unordered_map<SurfaceKey, std::shared_ptr<MetalSurface>, SurfaceKey::HashFunction> surfaces;
    std::unordered_map<fs::path, std::shared_ptr<MetalModel>, PathHash> models;
    std::unordered_map<fs::path, std::shared_ptr<MetalShader>, PathHash> shaderStages;
    std::vector<std::shared_ptr<MetalPass>> passes;
    uint64_t audioSurfaceFrameGeneration = 0;
    std::set<SurfaceKey> viewableTargets;
    SurfaceKey defaultTarget;
};

std::shared_ptr<MetalScene> metal_scene_create(MetalContext& ctx, Scene& scene);
MetalScene* metal_scene_get(MetalContext& ctx, Scene& scene);
void metal_scene_destroy(MetalContext& ctx, MetalScene& scene);
void metal_scene_render(MetalContext& ctx, Scene& scene, const glm::uvec2& size);
RenderOutput metal_scene_get_output(MetalContext& ctx, Scene& scene);
std::vector<RenderTargetView> metal_scene_target_views(MetalContext& ctx, Scene& scene);
void metal_scene_write_output(MetalContext& ctx, Scene& scene, const fs::path& path);
MetalSurface* metal_scene_get_or_create_surface(MetalScene& scene, const std::string& surface, uint64_t frameCount = 0, bool sampling = false);

} // namespace metal
```

- [ ] **Step 2: Define Metal surfaces**

Create `include/vklive/metal/metal_surface.h`:

```cpp
#pragma once

#include <glm/glm.hpp>

#include <vklive/scene.h>
#include <vklive/metal/metal_scene.h>

namespace metal
{

enum class MetalAllocationState
{
    Init,
    Loaded,
    Failed
};

struct MetalSurface
{
    explicit MetalSurface(Surface* pS)
        : pSurface(pS)
    {
    }

    Surface* pSurface = nullptr;
    std::string debugName;
    SurfaceKey key;
    MetalAllocationState allocationState = MetalAllocationState::Init;
    void* texture = nullptr;
    void* sampler = nullptr;
    glm::uvec2 size = glm::uvec2(0);
    Format format = Format::default_format;
    uint64_t generation = 0;
};

uint64_t frame_to_pingpong(uint64_t frame);
uint64_t metal_format(Format format);
void metal_surface_destroy(MetalContext& ctx, MetalSurface& surface);
bool metal_surface_create_target(MetalContext& ctx, MetalSurface& surface, const glm::uvec2& size, Format format, bool sampled);
bool metal_surface_create_depth(MetalContext& ctx, MetalSurface& surface, const glm::uvec2& size, Format format);
bool metal_surface_create_from_file(MetalContext& ctx, MetalSurface& surface, const fs::path& filename, Format format = Format::r8g8b8a8_unorm, bool flipY = false);
void metal_surface_ensure_sampler(MetalContext& ctx, MetalSurface& surface);
void metal_surface_update_from_audio(MetalContext& ctx, MetalSurface& surface, bool& surfaceChanged);

} // namespace metal
```

- [ ] **Step 3: Define Metal model and pass state**

Create `include/vklive/metal/metal_model.h`:

```cpp
#pragma once

#include <memory>
#include <unordered_map>

#include <vklive/model.h>

namespace metal
{
struct MetalContext;
struct MetalScene;

struct MetalModel : Model
{
    explicit MetalModel(const ModelCreateInfo& info)
        : createInfo(info)
    {
    }

    ModelCreateInfo createInfo;
    void* vertices = nullptr;
    void* indices = nullptr;
    void* materials = nullptr;
    std::vector<void*> materialTextures;
    bool staged = false;
};

std::shared_ptr<MetalModel> metal_model_create(MetalContext& ctx, MetalScene& scene, const Geometry& geom);
void metal_model_destroy(MetalContext& ctx, MetalModel& model);
void metal_model_stage(MetalContext& ctx, MetalModel& model);

} // namespace metal
```

Create `include/vklive/metal/metal_pass.h`:

```cpp
#pragma once

#include <map>
#include <memory>

#include <glm/glm.hpp>

#include <vklive/scene.h>
#include <vklive/vulkan/vulkan_bindings.h>

namespace metal
{
struct MetalScene;
struct MetalSurface;

struct MetalTargetData
{
    MetalSurface* pMetalSurface = nullptr;
};

struct MetalPassTargets
{
    std::map<std::string, MetalTargetData> mapNameToTargetData;
    std::vector<MetalTargetData*> orderedTargets;
    glm::uvec2 targetSize = glm::uvec2(0);
};

struct MetalPassFrameData
{
    void* uniforms = nullptr;
    void* pipeline = nullptr;
    void* depthStencil = nullptr;
    MetalPassTargets targets;
    BindingSets mergedBindingSets;
};

struct MetalPass
{
    MetalPass(MetalScene& s, Pass& p)
        : metalScene(s)
        , pass(p)
    {
    }

    MetalScene& metalScene;
    Pass& pass;
    MetalPassFrameData frameData;
};

std::shared_ptr<MetalPass> metal_pass_create(MetalScene& scene, Pass& pass);
void metal_pass_destroy(MetalContext& ctx, MetalPass& pass);
bool metal_pass_draw(MetalContext& ctx, MetalPass& pass);

} // namespace metal
```

- [ ] **Step 4: Implement scene create/destroy with unsupported diagnostics**

Create `src/metal/metal_scene.mm` with these required checks:

```objc
#include <fmt/format.h>

#include <zest/time/timer.h>

#include <vklive/validation.h>
#include <vklive/metal/metal_context.h>
#include <vklive/metal/metal_model.h>
#include <vklive/metal/metal_pass.h>
#include <vklive/metal/metal_scene.h>
#include <vklive/metal/metal_shader.h>
#include <vklive/metal/metal_surface.h>

namespace metal
{

std::string SurfaceKey::DebugName() const
{
    return fmt::format("{}:P{}", targetName, pingPongIndex);
}

MetalScene* metal_scene_get(MetalContext& ctx, Scene& scene)
{
    auto itr = ctx.mapMetalScene.find(&scene);
    return itr == ctx.mapMetalScene.end() ? nullptr : itr->second.get();
}

std::shared_ptr<MetalScene> metal_scene_create(MetalContext& ctx, Scene& scene)
{
    if (!scene.errors.empty() || !fs::exists(scene.root) || !scene.valid)
    {
        return nullptr;
    }

    auto spScene = std::make_shared<MetalScene>(&scene);
    ctx.mapMetalScene[&scene] = spScene;

    for (auto& [_, pShader] : scene.shaders)
    {
        metal_shader_create(ctx, *spScene, *pShader);
    }

    for (auto& spPass : scene.passes)
    {
        if (spPass->passType == PassType::RayTracing)
        {
            scene_report_error(scene, MessageSeverity::Error, "Metal renderer does not support VkLive ray tracing passes yet", scene.sceneGraphPath, spPass->scriptPassLine);
            continue;
        }
        if (spPass->passType == PassType::Scripted)
        {
            scene_report_error(scene, MessageSeverity::Error, "Metal renderer does not support scripted NanoVG passes yet", scene.sceneGraphPath, spPass->scriptPassLine);
            continue;
        }
        metal_pass_create(*spScene, *spPass);
    }

    for (auto& [_, pGeom] : scene.models)
    {
        metal_model_create(ctx, *spScene, *pGeom);
    }

    if (!scene.valid)
    {
        metal_scene_destroy(ctx, *spScene);
        return nullptr;
    }
    return spScene;
}

void metal_scene_destroy(MetalContext& ctx, MetalScene& scene)
{
    for (auto& pass : scene.passes)
    {
        metal_pass_destroy(ctx, *pass);
    }
    scene.passes.clear();
    for (auto& [_, surface] : scene.surfaces)
    {
        metal_surface_destroy(ctx, *surface);
    }
    scene.surfaces.clear();
    for (auto& [_, shader] : scene.shaderStages)
    {
        metal_shader_destroy(ctx, *shader);
    }
    scene.shaderStages.clear();
    for (auto& [_, model] : scene.models)
    {
        metal_model_destroy(ctx, *model);
    }
    scene.models.clear();
    ctx.mapMetalScene.erase(scene.pScene);
}

} // namespace metal
```

- [ ] **Step 5: Build**

Run:

```bash
cmake --build build --config Debug --target Rezonality
```

Expected:

```text
Built target Rezonality
```

- [ ] **Step 6: Commit**

```bash
git add include/vklive/metal/metal_scene.h include/vklive/metal/metal_surface.h include/vklive/metal/metal_model.h include/vklive/metal/metal_pass.h include/vklive/metal/metal_utils.h src/metal/metal_scene.mm src/metal/metal_surface.mm src/metal/metal_model.mm src/metal/metal_pass.mm src/metal/metal_utils.mm
git commit -m "feat: add Metal scene resource model"
```

### Task 8: Implement Metal Standard Raster Passes

**Files:**
- Modify: `src/metal/metal_pass.mm`
- Modify: `src/metal/metal_model.mm`
- Modify: `src/metal/metal_surface.mm`
- Modify: `src/metal/metal_scene.mm`
- Modify: `src/metal/metal_utils.mm`

- [ ] **Step 1: Create render targets and sampler inputs**

Implement `metal_scene_get_or_create_surface(...)` with the same key rules as Vulkan:

```cpp
uint64_t frame_to_pingpong(uint64_t frame)
{
    return frame % 2;
}

MetalSurface* metal_scene_get_or_create_surface(MetalScene& scene, const std::string& surfaceName, uint64_t frameCount, bool sampling)
{
    auto pSurface = scene_get_surface(*scene.pScene, surfaceName.c_str());
    if (!pSurface)
    {
        scene_report_error(*scene.pScene, MessageSeverity::Error, fmt::format("Could not find surface: {}", surfaceName));
        return nullptr;
    }

    SurfaceKey key{ surfaceName, 0 };
    if (sampling)
    {
        key.pingPongIndex = 1 - frame_to_pingpong(frameCount);
    }

    auto itr = scene.surfaces.find(key);
    if (itr != scene.surfaces.end())
    {
        return itr->second.get();
    }

    auto spSurface = std::make_shared<MetalSurface>(pSurface);
    spSurface->key = key;
    spSurface->debugName = key.DebugName();
    scene.surfaces[key] = spSurface;
    return spSurface.get();
}
```

- [ ] **Step 2: Map VkLive formats to Metal formats**

In `src/metal/metal_surface.mm`:

```objc
uint64_t metal_format(Format format)
{
    switch (format)
    {
    case Format::default_format:
    case Format::r8g8b8a8_unorm:
        return MTLPixelFormatRGBA8Unorm;
    case Format::r16g16b16a16_sfloat:
        return MTLPixelFormatRGBA16Float;
    case Format::r32g32b32a32_sfloat:
        return MTLPixelFormatRGBA32Float;
    case Format::default_depth_format:
    case Format::d32:
        return MTLPixelFormatDepth32Float;
    }
    return MTLPixelFormatRGBA8Unorm;
}
```

- [ ] **Step 3: Create Metal pipeline state for standard passes**

In `src/metal/metal_pass.mm`, build a `MTLRenderPipelineDescriptor` from the pass shaders and targets:

```objc
MTLRenderPipelineDescriptor* descriptor = [[MTLRenderPipelineDescriptor alloc] init];
descriptor.vertexFunction = (__bridge id<MTLFunction>)vertexShader->function;
descriptor.fragmentFunction = (__bridge id<MTLFunction>)fragmentShader->function;
descriptor.vertexDescriptor = metal_vertex_descriptor();
for (NSUInteger i = 0; i < colorFormats.size(); ++i)
{
    descriptor.colorAttachments[i].pixelFormat = colorFormats[i];
    descriptor.colorAttachments[i].blendingEnabled = YES;
    descriptor.colorAttachments[i].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    descriptor.colorAttachments[i].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    descriptor.colorAttachments[i].rgbBlendOperation = MTLBlendOperationAdd;
    descriptor.colorAttachments[i].sourceAlphaBlendFactor = MTLBlendFactorOne;
    descriptor.colorAttachments[i].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    descriptor.colorAttachments[i].alphaBlendOperation = MTLBlendOperationAdd;
}
descriptor.depthAttachmentPixelFormat = depthFormat;
NSError* error = nil;
frameData.pipeline = (__bridge_retained void*)[device newRenderPipelineStateWithDescriptor:descriptor error:&error];
if (!frameData.pipeline)
{
    scene_report_error(*metalScene.pScene, MessageSeverity::Error, [[error localizedDescription] UTF8String], pass.scene.sceneGraphPath, pass.scriptPassLine);
    return false;
}
```

- [ ] **Step 4: Encode draw calls**

In `metal_pass_draw(...)`, encode only `PassType::Standard`:

```objc
id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
[encoder setRenderPipelineState:(__bridge id<MTLRenderPipelineState>)frameData.pipeline];
[encoder setDepthStencilState:(__bridge id<MTLDepthStencilState>)frameData.depthStencil];
[encoder setVertexBuffer:(__bridge id<MTLBuffer>)frameData.uniforms offset:0 atIndex:0];
[encoder setFragmentBuffer:(__bridge id<MTLBuffer>)frameData.uniforms offset:0 atIndex:0];

for (auto& geom : pass.models)
{
    auto itrModel = metalScene.models.find(geom);
    if (itrModel == metalScene.models.end())
    {
        continue;
    }
    auto& model = *itrModel->second;
    metal_model_stage(ctx, model);
    [encoder setVertexBuffer:(__bridge id<MTLBuffer>)model.vertices offset:0 atIndex:1];
    for (const auto& part : model.parts)
    {
        [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                            indexCount:part.indexCount
                             indexType:MTLIndexTypeUInt32
                           indexBuffer:(__bridge id<MTLBuffer>)model.indices
                     indexBufferOffset:part.indexBase * sizeof(uint32_t)];
    }
}
[encoder endEncoding];
```

- [ ] **Step 5: Update frame timing and output descriptors**

In `metal_scene_render(...)`, mirror the Vulkan frame counter behavior:

```cpp
if (!metalScene->pScene->pause)
{
    Scene::GlobalFrameCount++;
    Scene::GlobalElapsedSeconds = metalScene->pScene->recording ? (metalScene->pScene->GlobalFrameCount * (1.0 / 60.0)) : Zest::timer_get_elapsed_seconds(Zest::globalTimer);
}
else
{
    Scene::GlobalElapsedSeconds = metalScene->pScene->GlobalFrameCount * (1.0 / 60.0);
}
```

After each pass, mark color targets rendered and populate:

```cpp
metalScene->defaultTarget = SurfaceKey{};
metalScene->viewableTargets.clear();
for (auto& [key, surface] : metalScene->surfaces)
{
    if (!surface->pSurface->rendered || format_is_depth(surface->pSurface->format))
    {
        continue;
    }
    if (surface->pSurface->name == "default_color")
    {
        metalScene->defaultTarget = key;
    }
    else
    {
        metalScene->viewableTargets.insert(key);
    }
}
```

- [ ] **Step 6: Verify with sample scenes**

Run:

```bash
./build/Rezonality.app/Contents/MacOS/Rezonality --renderer metal --project run_tree/projects/simple
./build/Rezonality.app/Contents/MacOS/Rezonality --renderer metal --project run_tree/projects/pbr_robot
```

Expected:

```text
The app opens, the render window shows the scene, shader edits still recompile, and no Vulkan SDK is required for the default macOS build.
```

- [ ] **Step 7: Commit**

```bash
git add src/metal/metal_pass.mm src/metal/metal_model.mm src/metal/metal_surface.mm src/metal/metal_scene.mm src/metal/metal_utils.mm
git commit -m "feat: render standard passes with Metal"
```

### Task 9: Implement Metal Target Preview, Output Capture, And Audio Texture

**Files:**
- Modify: `src/metal/metal_scene.mm`
- Modify: `src/metal/metal_surface.mm`
- Modify: `src/metal/metal_device.mm`

- [ ] **Step 1: Return default render output**

Implement `metal_scene_get_output(...)`:

```objc
RenderOutput metal_scene_get_output(MetalContext& ctx, Scene& scene)
{
    RenderOutput out;
    auto pMetalScene = metal_scene_get(ctx, scene);
    if (!pMetalScene)
    {
        return out;
    }

    auto itr = pMetalScene->surfaces.find(pMetalScene->defaultTarget);
    if (itr == pMetalScene->surfaces.end())
    {
        return out;
    }

    auto& surface = *itr->second;
    out.pSurface = surface.pSurface;
    out.textureId = (__bridge ImTextureID)(__bridge id<MTLTexture>)surface.texture;
    return out;
}
```

- [ ] **Step 2: Return target preview textures**

Implement `metal_scene_target_views(...)`:

```objc
std::vector<RenderTargetView> metal_scene_target_views(MetalContext& ctx, Scene& scene)
{
    std::vector<RenderTargetView> views;
    auto pMetalScene = metal_scene_get(ctx, scene);
    if (!pMetalScene)
    {
        return views;
    }
    for (const auto& target : pMetalScene->viewableTargets)
    {
        auto itr = pMetalScene->surfaces.find(target);
        if (itr == pMetalScene->surfaces.end() || !itr->second->texture)
        {
            continue;
        }
        views.push_back(RenderTargetView{
            itr->second->debugName,
            (__bridge ImTextureID)(__bridge id<MTLTexture>)itr->second->texture,
            itr->second->size
        });
    }
    return views;
}
```

- [ ] **Step 3: Upload audio analysis texture**

Implement `metal_surface_update_from_audio(...)` by matching the current Vulkan texture contents:

```objc
void metal_surface_update_from_audio(MetalContext& ctx, MetalSurface& surface, bool& surfaceChanged)
{
    auto& audioCtx = Zing::GetAudioContext();
    const glm::uvec2 size(audioCtx.audioAnalysisSettings.textureWidth, audioCtx.audioAnalysisSettings.textureHeight);
    if (surface.size != size || !surface.texture)
    {
        metal_surface_destroy(ctx, surface);
        metal_surface_create_target(ctx, surface, size, Format::r32g32b32a32_sfloat, true);
        surfaceChanged = true;
    }

    auto pixels = audioCtx.GetAudioTextureData();
    MTLRegion region = MTLRegionMake2D(0, 0, size.x, size.y);
    [(__bridge id<MTLTexture>)surface.texture replaceRegion:region
                                                mipmapLevel:0
                                                  withBytes:pixels.data()
                                                bytesPerRow:size.x * sizeof(glm::vec4)];
}
```

If the exact Zing audio API name differs, use the existing call inside `surface_update_from_audio(...)` in `src/vulkan/vulkan_surface.cpp` and keep the pixel format `RGBA32Float`.

- [ ] **Step 4: Capture PNG output**

Implement `metal_scene_write_output(...)` with a blit to a shared buffer and `lodepng`:

```objc
id<MTLTexture> texture = (__bridge id<MTLTexture>)surface.texture;
const NSUInteger bytesPerPixel = 4;
const NSUInteger bytesPerRow = texture.width * bytesPerPixel;
std::vector<unsigned char> pixels(texture.height * bytesPerRow);
MTLRegion region = MTLRegionMake2D(0, 0, texture.width, texture.height);
[texture getBytes:pixels.data() bytesPerRow:bytesPerRow fromRegion:region mipmapLevel:0];

fs::create_directories(path);
const auto outPath = path / fmt::format("{:05}.png", scene.GlobalFrameCount);
const auto result = lodepng::encode(outPath.string(), pixels, texture.width, texture.height);
if (result != 0)
{
    scene_report_error(scene, MessageSeverity::Error, fmt::format("Could not write Metal render output: {}", lodepng_error_text(result)));
}
```

- [ ] **Step 5: Verify target preview and recording**

Run:

```bash
./build/Rezonality.app/Contents/MacOS/Rezonality --renderer metal --project run_tree/projects/default
```

Expected:

```text
The Targets window lists non-default render targets, the render image uses ImGui texture previews, and enabling recording writes PNG files under run_tree/renders.
```

- [ ] **Step 6: Commit**

```bash
git add src/metal/metal_scene.mm src/metal/metal_surface.mm src/metal/metal_device.mm
git commit -m "feat: show and capture Metal render targets"
```

### Task 10: Add Clear Diagnostics For Metal Unsupported Features

**Files:**
- Modify: `src/scene.cpp`
- Modify: `src/metal/metal_scene.mm`
- Modify: `src/metal/metal_shader.mm`
- Modify: `tests/scene_inspect.cpp`

- [ ] **Step 1: Detect Metal-incompatible shader extensions**

In `src/metal/metal_shader.mm`, before compiling:

```cpp
const auto ext = shader.path.extension().string();
if (ext == ".geom" || ext == ".gs")
{
    scene_report_error(*metalScene.pScene, MessageSeverity::Error,
        "Metal renderer does not support VkLive geometry shaders. Use a vertex/fragment pass for Metal.",
        shader.path);
    return nullptr;
}
if (ext == ".rgen" || ext == ".rchit" || ext == ".rmiss")
{
    scene_report_error(*metalScene.pScene, MessageSeverity::Error,
        "Metal renderer does not support VkLive Vulkan ray tracing shaders yet.",
        shader.path);
    return nullptr;
}
```

- [ ] **Step 2: Add scene-level pass diagnostics**

In `src/metal/metal_scene.mm`, keep the pass-level messages from Task 7 and include the pass name:

```cpp
scene_report_error(scene, MessageSeverity::Error,
    fmt::format("Pass '{}' is a ray tracing pass. The Metal renderer currently supports standard raster passes only.", spPass->name),
    scene.sceneGraphPath,
    spPass->scriptPassLine);
```

```cpp
scene_report_error(scene, MessageSeverity::Error,
    fmt::format("Pass '{}' is a scripted NanoVG pass. The Metal renderer currently supports standard raster passes only.", spPass->name),
    scene.sceneGraphPath,
    spPass->scriptPassLine);
```

- [ ] **Step 3: Add scene inspection regression test**

Extend `tests/scene_inspect.cpp` with a mode named `metal-unsupported`:

```cpp
else if (mode == "metal-unsupported")
{
    ok &= require(scene->valid, "scene parser should accept renderer-specific features before backend init");
    bool hasRayPass = false;
    for (auto& pass : scene->passes)
    {
        hasRayPass |= pass->passType == PassType::RayTracing;
    }
    ok &= require(hasRayPass, "ray tracing sample should still parse as a ray tracing pass");
}
```

Add a CTest entry:

```cmake
add_test(
    NAME vklive_scene_tests_metal_unsupported_raytracer
    COMMAND vklive_scene_tests ${CMAKE_CURRENT_LIST_DIR}/run_tree/projects/ray_tracer default.scenegraph metal-unsupported
)
```

- [ ] **Step 4: Run parser tests**

Run:

```bash
cmake --build build --config Debug --target vklive_scene_tests
ctest --test-dir build -C Debug -R vklive_scene_tests --output-on-failure
```

Expected:

```text
100% tests passed
```

- [ ] **Step 5: Commit**

```bash
git add src/metal/metal_scene.mm src/metal/metal_shader.mm tests/scene_inspect.cpp CMakeLists.txt
git commit -m "feat: report Metal unsupported render features"
```

### Task 11: Documentation And Final Verification

**Files:**
- Modify: `README.md`
- Modify: `TODO.md`
- Modify: `.github/workflows/builds.yml`

- [ ] **Step 1: Update README build text**

Replace the Vulkan-only build note in `README.md` with:

```markdown
On macOS, Rezonality uses the native Metal renderer by default and does not require the Vulkan SDK for the normal build. Windows and Linux use Vulkan by default. Developers can still opt into a macOS Vulkan build with `-DVKLIVE_ENABLE_VULKAN=ON -DVKLIVE_ENABLE_METAL=OFF` when they want to compare behavior.
```

Add renderer CLI documentation:

```markdown
Renderer selection:

```sh
Rezonality --renderer auto
Rezonality --renderer metal
Rezonality --renderer vulkan
```

`auto` selects Metal on macOS and Vulkan elsewhere.
```

- [ ] **Step 2: Update TODO**

Move the `Metal renderer` line from the Mac freezer section to a completed/recent section with this note:

```markdown
- Metal renderer on macOS for standard raster passes. Follow-up work remains for Metal ray tracing, geometry shader emulation, and scripted vector passes.
```

- [ ] **Step 3: Run full available verification**

Run on macOS:

```bash
rm -rf build
./prebuild.sh
./config.sh Debug
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
./build/Rezonality.app/Contents/MacOS/Rezonality --renderer metal --project run_tree/projects/simple --smoke-test
```

Expected:

```text
100% tests passed
```

The app smoke command exits with status `0`.

Run on Windows or Linux:

```bash
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Expected:

```text
100% tests passed
```

- [ ] **Step 4: Manual macOS dogfood**

Run:

```bash
./build/Rezonality.app/Contents/MacOS/Rezonality --renderer metal --project run_tree/projects/pbr_robot
```

Expected manual observations:

```text
The app opens on macOS without Vulkan SDK setup.
The main render window shows the robot scene.
Changing pbr.frag and pressing CTRL+ENTER rebuilds the scene.
Shader compiler errors appear in the editor.
The Targets window shows non-default render targets when the scene has them.
Help/About reports Renderer: Metal and the selected MTLDevice name.
```

- [ ] **Step 5: Commit**

```bash
git add README.md TODO.md .github/workflows/builds.yml
git commit -m "docs: document Metal renderer on macOS"
```

## Self Review

Spec coverage:

- macOS defaults to Metal through `device_resolve_backend`.
- Vulkan remains the default on Windows and Linux.
- The app no longer needs `SDL_WINDOW_VULKAN` on macOS Metal startup.
- `window_targets` no longer depends on Vulkan internals.
- CMake no longer requires Vulkan for the default macOS build.
- Live GLSL editing remains the user-facing shader workflow.
- Unsupported Vulkan-only features get explicit diagnostics.

Red-flag scan:

- The only staged exclusions are explicit out-of-scope systems with diagnostics and follow-up scope.

Type consistency:

- `RenderBackend`, `RenderTargetView`, `MetalContext`, `MetalScene`, `MetalSurface`, `MetalModel`, `MetalPass`, and factory function names are used consistently across tasks.

## Implementation Notes

- The cleanest learning path is to implement this plan in the listed order. Tasks 1-4 preserve behavior while opening the seam for Metal. Tasks 5-9 add Metal incrementally. Tasks 10-11 make the result understandable and shippable.
- One possible improvement after this plan lands is a smaller backend-agnostic descriptor model. Right now the plan reuses `VulkanBindingSet` metadata because it already represents reflected set/binding data, but a future `ShaderBindingSet` name would make the shared intent clearer.
