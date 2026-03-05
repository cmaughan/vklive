# CLAUDE.md — AI Assistant Guide for VkLive

## Project Overview

**VkLive** (branded as **Rezonality**) is a Vulkan-based live coding editor for real-time shader and scene development. Users edit shader files and a custom scene description language (scenegraph DSL) and see the results update immediately. The primary platform is Windows; Mac and Linux build but are less tested.

Key capabilities:
- Live shader editing (GLSL vertex, fragment, geometry, ray tracing shaders)
- Custom scenegraph DSL parsed by MPC for describing render passes, geometry, surfaces, and textures
- Audio input analysis (FFT spectrum) exposed as a shader texture
- Zep-based embedded text editor (modal Vim or Notepad mode)
- Vulkan-backed rendering with full validation layer integration

---

## Repository Layout

```
vklive/
├── app/                        # Application layer (UI, menus, project management)
│   ├── src/                    # main.cpp, editor, menus, windows, config, project
│   ├── include/app/            # App-specific public headers
│   ├── cmake/demo_common.cmake # App CMake helpers
│   └── res/                    # Icons, app.manifest
│
├── src/                        # Core library sources
│   ├── vulkan/                 # Vulkan rendering engine (20+ source files)
│   ├── process/                # External process execution (shader compiler)
│   ├── scene.cpp               # Scenegraph DSL parsing (uses MPC)
│   ├── model.cpp               # 3D model loading (via assimp)
│   ├── camera.cpp              # Camera management
│   ├── validation.cpp          # Vulkan validation layer handling
│   └── python_scripting.cpp    # Python scripting integration (pocketpy)
│
├── include/vklive/             # Public headers for the vklive library
│   ├── vulkan/                 # Vulkan subsystem headers
│   ├── IDevice.h               # Abstract rendering device interface
│   ├── scene.h                 # Core scene data structures
│   ├── camera.h                # Camera struct and functions
│   ├── message.h               # Error/warning messaging types
│   └── threadpool/             # Thread pool utilities
│
├── libs/                       # Vendored third-party submodules
│   ├── zing/                   # Audio capture and FFT analysis
│   ├── clip/                   # Clipboard (platform-specific: Win/Mac/X11)
│   ├── mpc/                    # Parser combinator (C library, for scenegraph DSL)
│   ├── nanovg_vulkan/          # NanoVG vector graphics on Vulkan
│   ├── pocketpy/               # Embedded Python interpreter
│   └── rccp/                   # Runtime Compiled C++
│
├── zep/                        # Zep text editor submodule
├── vcpkg/                      # VCPKG package manager submodule
│
├── run_tree/                   # Runtime assets (must be present alongside executable)
│   ├── projects/               # Example projects (default, shadertoy, ray_tracer, etc.)
│   ├── shaders/                # Shared shader templates
│   ├── models/                 # Bundled 3D models (sphere.gltf, etc.)
│   ├── textures/               # Bundled textures
│   ├── fonts/                  # Font files for ImGui
│   └── docs/                   # In-app help documentation
│
├── cmake/                      # CMake utilities and config templates
├── tests/                      # Minimal test content
├── CMakeLists.txt              # Root build file
├── .clang-format               # Code formatting rules (WebKit-based)
├── build.sh / build.bat        # Build scripts
├── config.sh / config.bat      # CMake configuration scripts
└── prebuild.sh / prebuild.bat  # Bootstrap VCPKG and install dependencies
```

---

## Build System

### Prerequisites
- CMake >= 3.15
- Vulkan SDK (latest version required)
- C++20 compatible compiler (MSVC on Windows, Clang/GCC on Mac/Linux)
- Git (for submodule initialization)

### First-Time Setup
```bash
git submodule update --init        # Pull vcpkg, zep, and lib submodules
./prebuild.sh                      # (or prebuild.bat on Windows) — bootstraps VCPKG, installs all dependencies
./config.sh                        # (or config.bat) — runs CMake, creates build/ directory
./build.sh                         # (or cmake --build build/) — compiles everything
```

### CMake Targets
| Target | Type | Description |
|--------|------|-------------|
| `vklive` | Static library | Core rendering engine + scene system |
| `Rezonality` | Executable | Full application (links against `vklive`) |

### Build Artifacts
- Debug builds append `-debug` suffix to libraries
- RelWithDebInfo builds append `-reldbg`
- Windows Release builds generate PDB symbols for debugging

### Key CMake Definitions
- `ZEP_USE_SDL` — uses SDL2 backend for Zep editor
- `VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1` — enables dynamic Vulkan extension dispatch
- `ZEP_FEATURE_CPP_FILE_SYSTEM` — enables filesystem support in Zep
- `IMGUI_DISABLE_INCLUDE_IMCONFIG_H` — uses bundled ImGui config

### VCPKG Dependencies (managed automatically via prebuild scripts)
```
Vulkan, SDL2, imgui, assimp, reproc++, fmt, range-v3,
tinyfiledialogs, unofficial-nativefiledialog, unofficial-spirv-reflect,
gli, stb, lodepng, unofficial-concurrentqueue, tsl-ordered-map,
clipp, tomlplusplus, unofficial-minizip, kissfft
```

---

## Architecture

### Threading Model
The application uses a two-thread architecture:

1. **Main/UI Thread** — runs the SDL event loop, ImGui rendering, and Vulkan presentation
2. **Compile Thread** — watches for file changes, recompiles the Vulkan pipeline, and atomically swaps in the new scene if compilation succeeds

If compilation fails, the previous valid scene continues rendering. Error messages are reported back to the editor via a thread-safe queue (`unofficial-concurrentqueue`).

### Core Data Flow
```
User edits file → File watcher detects change → scene_build() parses DSL
    → Vulkan pipeline rebuilt on compile thread
    → If success: new VulkanScene swapped atomically
    → If failure: errors sent to Zep editor for display; old scene unchanged
```

### Key Abstractions

**`IDevice`** (`include/vklive/IDevice.h`) — Abstract rendering backend interface. Currently only Vulkan is implemented. Designed to support Metal/DX12 in the future.

**`Scene`** (`include/vklive/scene.h`) — Plain data representation of the scenegraph DSL. Contains:
- `surfaces` — named textures and render targets (`Surface`)
- `passes` — render passes in order (`Pass`)
- `shaders` — shader file references (`Shader`, `ShaderGroup`)
- `models` — geometry references (`Geometry`)
- `cameras` — camera configurations

**`VulkanScene`** (`include/vklive/vulkan/vulkan_scene.h`) — GPU-side realization of a `Scene`. Manages actual Vulkan resources tied to a `Scene` instance.

### Vulkan Subsystem (`src/vulkan/`)
| File | Purpose |
|------|---------|
| `vulkan_context.cpp/h` | Core context; used as precompiled header |
| `vulkan_device.cpp/h` | Physical/logical device selection and creation |
| `vulkan_window.cpp/h` | SDL window, swapchain, framebuffers |
| `vulkan_render.cpp/h` | Per-frame render loop |
| `vulkan_scene.cpp/h` | `Scene` → Vulkan resources binding |
| `vulkan_pipeline.cpp/h` | Graphics pipeline creation and management |
| `vulkan_shader.cpp/h` | GLSL compilation (via external glslc) + SPIR-V loading |
| `vulkan_reflect.cpp/h` | Shader reflection via SPIRV-Reflect (auto-builds descriptors) |
| `vulkan_descriptor.cpp/h` | Descriptor set layout and pool management |
| `vulkan_uniform.cpp/h` | Uniform buffer objects (time, resolution, audio, etc.) |
| `vulkan_pass.cpp/h` | Render pass recording |
| `vulkan_buffer.cpp/h` | Vertex/index buffer allocation |
| `vulkan_command.cpp/h` | Command buffer utilities |
| `vulkan_imgui.cpp/h` | ImGui Vulkan backend integration |
| `vulkan_debug.cpp/h` | Debug labels for Nsight/RenderDoc visibility |
| `vulkan_model.cpp/h` | Mesh loading and GPU upload |
| `vulkan_surface.cpp/h` | Render target creation and management |

### Scenegraph DSL (`src/scene.cpp`)
The `.scenegraph` file format is parsed using the MPC parser combinator library. Example:

```
surface: MyTarget {
    scale: (1, 1, 1)
    format: default_color
    clear: (0.0, 0.0, 0.0, 0.0)
}

surface: Depth {
    scale: (1, 1, 1)
    format: default_depth
}

pass: Pass1 {
    samplers: (NoiseTexture)
    targets: (MyTarget, Depth)
    clear: (0.0, 0.0, 0.0, 1.0)
    geometry: bg {
        path: screen_rect       // built-in fullscreen quad
        vs: screen.vert
        fs: screen.frag
    }
}
```

Key DSL concepts:
- `surface` — declares a named texture (file path) or render target (format + scale)
- `pass` — a render pass with targets, samplers, and geometry
- `geometry` — references a model file or built-in (`screen_rect`) with associated shaders
- `samplers` — textures available to shaders in a pass
- `targets` — render targets written by the pass (first color target is default output)

### Application Layer (`app/src/`)
| File | Purpose |
|------|---------|
| `main.cpp` | SDL event loop, thread management, device init |
| `editor.cpp` | Zep editor integration, error highlighting |
| `project.cpp` | Project load/save, file discovery |
| `config.cpp` | App configuration (TOML format) |
| `controller.cpp` | Global application state (`Controller` struct) |
| `menu.cpp` | ImGui menu bar |
| `window_render.cpp` | Main render output window |
| `window_targets.cpp` | Render target visualization panel |
| `window_sequencer.cpp` | Timeline/sequencer UI |

---

## Code Conventions

### Naming
| Entity | Convention | Example |
|--------|-----------|---------|
| Classes / Structs | PascalCase | `VulkanContext`, `Scene`, `Surface` |
| Functions | snake_case | `vulkan_scene_render`, `scene_build` |
| Variables | snake_case | `frame_buffer_size`, `elapsed_seconds` |
| Raw pointers (params) | `p` prefix | `pDevice`, `pScene` |
| Shared pointers | `sp` prefix | `spScene`, `spDevice` |
| Global shared ptr | `g_p` prefix | `g_pDevice` |
| Namespaces | lowercase | `vulkan`, `Zest` |

### Code Style
Enforced via `.clang-format` (WebKit-based):
- **Indentation**: 4 spaces, no tabs
- **Brace wrapping**: All braces on their own line (functions, classes, structs, if/else, etc.)
- **Column limit**: None (no wrapping enforced)
- **Pointer alignment**: Left-aligned (`int* p`, not `int *p`)
- **Include sorting**: Enabled
- **Short functions**: Only empty functions may be on one line

Run clang-format before committing: `clang-format -i <file>`

### Architecture Patterns
- **C-like structure**: Prefer free functions over methods; structs are plain data, functions operate on them
- **`shared_ptr` for ownership**: Most heap objects are managed with `std::shared_ptr`
- **Atomic scene swap**: Compile thread produces a new `VulkanScene`; main thread swaps atomically
- **Validation first**: Vulkan validation layers are always enabled during development; errors are expected to surface early
- **Debug labels everywhere**: All Vulkan objects (buffers, images, pipelines, passes) are labeled for GPU debugger visibility
- **No exceptions in Vulkan code**: Use return values and error reporting via `Message` vectors

### Include Order (enforced by clang-format)
1. Standard library headers
2. Third-party headers
3. Zest/Zep headers
4. `vklive/` library headers
5. `app/` headers

---

## Example Projects

Located in `run_tree/projects/`. Each project is a folder containing:
- `*.scenegraph` — scene description file (the "entry point")
- `*.vert`, `*.frag`, `*.geom` — GLSL shader files
- `*.gltf` / `*.glb` — optional 3D model files
- `project.toml` — project metadata

| Project | Description |
|---------|-------------|
| `default` | Background quad + sphere, time uniform, noise texture |
| `shadertoy` | ShaderToy-style fullscreen quad examples |
| `ray_tracer` | Vulkan ray tracing extension examples |
| `deferred_shading` | Multi-pass deferred rendering |
| `blend_waves` | Wave simulation |
| `simple` | Minimal single-pass example |

---

## Shader Conventions

Shaders are standard GLSL compiled via the external `glslc` compiler (Vulkan SDK). Reflection is performed via SPIRV-Reflect to auto-build descriptor layouts.

### Built-in Uniform Buffer (UBO)
All shaders automatically receive a UBO with standard uniforms:
- `iTime` / `iTimeDelta` — elapsed time in seconds
- `iResolution` — viewport resolution (vec2 or vec3)
- `iMouse` — mouse position (planned: `iMouse.xy`)
- `iFrame` — frame counter
- `iDate` — date/time (planned)

### Built-in Textures
- `AudioAnalysis` — reserved surface name; automatically bound to the FFT audio data texture

### Shader File Extensions
- `.vert` — vertex shader
- `.frag` — fragment shader
- `.geom` — geometry shader
- `.rgen` / `.rchit` / `.rmiss` — ray tracing shaders

---

## Development Workflows

### Adding a New Render Feature
1. Update `Scene` / `Pass` structs in `include/vklive/scene.h` if new DSL syntax is needed
2. Update the MPC parser in `src/scene.cpp` to parse the new syntax
3. Add Vulkan resource management in the appropriate `src/vulkan/vulkan_*.cpp` file
4. Update `vulkan_scene.cpp` to bind new resources when building `VulkanScene`
5. Update `vulkan_pass.cpp` if new commands need to be recorded per pass

### Adding a New Shader Uniform
1. Update the UBO struct in `vulkan_uniform.h`
2. Update the upload logic in `vulkan_uniform.cpp`
3. Document the new uniform name in example shaders

### Adding a New Project Template
1. Create a new folder under `run_tree/projects/`
2. Add a `.scenegraph` file and the required shaders
3. Optionally add a `project.toml` with name and description

### Debugging Rendering Issues
- Enable Vulkan validation layers (always on in debug builds)
- Use NVIDIA Nsight or RenderDoc — all objects are debug-labeled
- Check `Scene::errors` / `Scene::warnings` vectors for parse errors
- Look at editor error highlights — shader compile errors are mapped back to line numbers

---

## Known Limitations and TODOs

See `TODO.md` for the full backlog. Key items:
- UBO is fixed-layout; custom user uniforms not yet supported
- Ray tracing is partially implemented
- Low-DPI and Mac display scaling issues exist
- Auto-indent in the Zep editor is incomplete
- Linux and Mac builds are less tested than Windows

---

## File Watching and Hot Reload

The compile thread monitors the project directory. When any `.vert`, `.frag`, `.geom`, `.scenegraph`, or other editable file changes (or the user presses `Ctrl+Enter`), the full scene is re-parsed and the Vulkan pipeline is rebuilt. The hot-reload is atomic: the old scene stays live until the new one is fully ready.

Files considered editable by the scene system (`src/scene.cpp`):
- `.vert`, `.frag`, `.geom` — shaders
- `.rgen`, `.rchit`, `.rmiss`, `.rint`, `.rahit`, `.rcall` — ray tracing shaders
- `.h`, `.glsl` — shader headers/includes
- `.scenegraph` — scene description
- `.py` — Python scripting

---

## Testing

There is no automated test suite currently. Testing is manual via the example projects in `run_tree/projects/`. The `tests/` directory contains minimal reflection test data.

When making changes, verify by:
1. Building successfully
2. Loading the `default` project and confirming it renders
3. Loading the `shadertoy` project and confirming audio analysis works (if audio changes)
4. Editing shaders live and confirming hot-reload works

---

## Platform Notes

### Windows (Primary)
- Use Visual Studio (load solution from `build/` after `config.bat`)
- VCPKG triplet: `x64-windows-static-md`
- PDB symbols generated for Release builds

### macOS
- Build with CLion or `cmake --build`
- Uses `clip_osx.mm` for clipboard
- Metal renderer is a future goal, not yet implemented

### Linux
- Requires `X11`, `Xext`, `xcb` system libraries
- Uses `clip_x11.cpp` for clipboard
- Tested less frequently; may have platform-specific issues
