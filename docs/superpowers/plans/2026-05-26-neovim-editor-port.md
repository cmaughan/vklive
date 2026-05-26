# Neovim Editor Port Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port the Draxul embedded Neovim GUI host into VkLive as a switchable editor backend that draws inside VkLive's Vulkan/Metal window and opens project edit files in Neovim-managed tabs.

**Architecture:** Keep Zep intact and introduce a small editor-backend boundary so VkLive can switch between Zep and Neovim at runtime. Copy the mature Draxul Neovim host, RPC, grid, font, and renderer-support code into a project-owned static library, trimming terminal hosts, Draxul chrome, host management, and non-Neovim features. VkLive owns project-file discovery, backend selection, ImGui docking/window placement, and live-coding callbacks; Neovim owns buffers, tabs, splits, modes, commands, and editor UI state.

**Tech Stack:** C++20, CMake, SDL2, ImGui, Vulkan, Metal, Neovim `--embed`, msgpack/mpack, FreeType, HarfBuzz, Draxul line-grid UI code copied from `../Draxul`.

---

## Current Context

- VkLive's editor integration is Zep-shaped today:
  - `app/include/app/editor.h` exposes `zep_init`, `zep_show`, `zep_load`, `zep_update_files`, diagnostics, and Zep callback types.
  - `app/src/editor.cpp` owns `ZepWrapper`, ImGui presentation, file loading, dirty-buffer saving, and Zep event callbacks.
  - `app/src/main.cpp` calls the Zep API directly for project loads, shader saves, evaluation, diagnostics, and drawing.
- VkLive already renders custom textures in ImGui for Vulkan and Metal:
  - `include/vklive/IDevice.h` exposes `FontTexture()` via `Zest::IFontTexture`.
  - `src/vulkan/vulkan_imgui_texture.cpp` and `src/metal/metal_imgui_texture.*` upload font/atlas textures for ImGui draw lists.
- Draxul has the relevant code split across focused libraries:
  - `../Draxul/libs/draxul-nvim`: Neovim process, msgpack RPC, UI redraw event handling, input translation.
  - `../Draxul/libs/draxul-grid`: line-grid cell storage and updates.
  - `../Draxul/libs/draxul-font`: FreeType/HarfBuzz shaping and atlas management.
  - `../Draxul/libs/draxul-runtime-support` and `../Draxul/libs/draxul-renderer`: renderer state, atlas upload, cursor blink, resize/startup helpers, and grid rendering pipeline.
  - `../Draxul/libs/draxul-host/src/nvim_host.*`: useful behavior to copy into a VkLive-specific host wrapper, excluding Draxul's multi-host manager and chrome.

## Product Decisions

- Use Neovim-native `:tabedit`, `:split`, `:vsplit`, buffers, windows, and tabline behavior. VkLive will not add editor tabs, split panes, or editor chrome for the Neovim backend.
- On project load, VkLive opens the current edit-file set as Neovim tabs. Users can then rearrange inside Neovim using native commands.
- Live switch is allowed for this slice: changing `Zep` to `Neovim` creates the Neovim host if needed and shows the same project files. Changing back keeps Zep available. Buffer-content migration between editors is not part of the first working port.
- Diagnostics continue to use the existing Zep path until the Neovim backend has a dedicated sign/extmark path. The first Neovim slice focuses on drawing, input, resize, and opening shader files.

## File Structure

### New VkLive Editor Boundary

- `app/include/app/editor_backend.h`: backend enum, string conversion, parse helpers.
- `app/src/editor_backend.cpp`: implementation of backend helpers.
- `tests/editor_backend_tests.cpp`: lightweight tests for backend parsing and stable config strings.
- `app/include/app/editor_host.h`: backend-neutral editor interface used by `main.cpp`.
- `app/src/editor_host.cpp`: owns both concrete backends and handles live switching.
- `app/include/app/editor_zep_host.h`: Zep adapter preserving the current Zep behavior.
- `app/src/editor_zep_host.cpp`: current `ZepWrapper` code moved behind the backend-neutral interface.
- `app/include/app/editor_nvim_host.h`: VkLive adapter for the copied Draxul Neovim code.
- `app/src/editor_nvim_host.cpp`: ImGui window, focus handling, project tab loading, eval/save hooks, and draw-list bridge.

### New Static Library

- `libs/vklive_nvim/CMakeLists.txt`: static library definition and dependency wiring.
- `libs/vklive_nvim/include/vklive_nvim/nvim_host.h`: small public API consumed by `app/src/editor_nvim_host.cpp`.
- `libs/vklive_nvim/include/vklive_nvim/nvim_events.h`: UI event and redraw surface exported to VkLive.
- `libs/vklive_nvim/include/vklive_nvim/nvim_render_model.h`: renderable grid/cursor/atlas data independent of VkLive app state.
- `libs/vklive_nvim/src/nvim/*`: copied and trimmed Draxul Neovim RPC/process/UI files.
- `libs/vklive_nvim/src/grid/*`: copied Draxul grid storage and update logic.
- `libs/vklive_nvim/src/font/*`: copied Draxul FreeType/HarfBuzz font grid and atlas logic.
- `libs/vklive_nvim/src/runtime/*`: copied startup resize, cursor blink, atlas upload queue, and grid rendering state.
- `libs/vklive_nvim/third_party/mpack/*`: copied minimal mpack source snapshot used by Draxul if no package manager target is available.
- `libs/vklive_nvim/shaders/*`: copied Draxul grid shaders for the native Vulkan/Metal renderer path.

### Build and Config

- `CMakeLists.txt`: add `libs/vklive_nvim`, tests, and app sources.
- `vcpkg.json`: add `freetype` and `harfbuzz` if they are not already resolved by existing dependencies.
- `app/include/app/config.h`: add `EditorBackendKind editor_backend = EditorBackendKind::Zep`.
- `app/src/config.cpp`: load/save `editor_backend = "zep" | "neovim"`.
- `app/src/menu.cpp` or the existing editor menu file: add backend selection.
- `app/src/main.cpp`: call the backend-neutral editor host instead of direct Zep functions.

---

## Task 1: Add the Editor Backend Enum

**Files:**
- Create: `app/include/app/editor_backend.h`
- Create: `app/src/editor_backend.cpp`
- Create: `tests/editor_backend_tests.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write the failing test**

```cpp
// tests/editor_backend_tests.cpp
#include <app/editor_backend.h>

#include <cassert>
#include <string>

int main()
{
    assert(editor_backend_to_string(EditorBackendKind::Zep) == std::string("zep"));
    assert(editor_backend_to_string(EditorBackendKind::Neovim) == std::string("neovim"));

    EditorBackendKind parsed = EditorBackendKind::Zep;
    assert(editor_backend_from_string("zep", parsed));
    assert(parsed == EditorBackendKind::Zep);
    assert(editor_backend_from_string("Zep", parsed));
    assert(parsed == EditorBackendKind::Zep);
    assert(editor_backend_from_string("nvim", parsed));
    assert(parsed == EditorBackendKind::Neovim);
    assert(editor_backend_from_string("neovim", parsed));
    assert(parsed == EditorBackendKind::Neovim);

    parsed = EditorBackendKind::Neovim;
    assert(!editor_backend_from_string("vim", parsed));
    assert(parsed == EditorBackendKind::Neovim);
}
```

- [x] **Step 2: Wire the test target and verify it fails**

```cmake
add_executable(vklive_editor_backend_tests tests/editor_backend_tests.cpp)
target_link_libraries(vklive_editor_backend_tests PRIVATE RezonalityAppCore)
add_test(NAME vklive_editor_backend_tests COMMAND vklive_editor_backend_tests)
```

Run:

```powershell
python do.py build debug --target vklive_editor_backend_tests
```

Expected: compilation fails because `app/editor_backend.h` does not exist.

- [x] **Step 3: Add the backend helper header**

```cpp
// app/include/app/editor_backend.h
#pragma once

#include <string_view>

enum class EditorBackendKind
{
    Zep,
    Neovim,
};

const char* editor_backend_to_string(EditorBackendKind backend);
bool editor_backend_from_string(std::string_view value, EditorBackendKind& backend);
```

- [x] **Step 4: Add the backend helper implementation**

```cpp
// app/src/editor_backend.cpp
#include <app/editor_backend.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <string>

const char* editor_backend_to_string(EditorBackendKind backend)
{
    switch (backend)
    {
    case EditorBackendKind::Zep:
        return "zep";
    case EditorBackendKind::Neovim:
        return "neovim";
    }

    return "zep";
}

bool editor_backend_from_string(std::string_view value, EditorBackendKind& backend)
{
    std::string normalized(value);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (normalized == "zep")
    {
        backend = EditorBackendKind::Zep;
        return true;
    }

    if (normalized == "nvim" || normalized == "neovim")
    {
        backend = EditorBackendKind::Neovim;
        return true;
    }

    return false;
}
```

- [x] **Step 5: Add the app-core source**

```cmake
set(APP_CORE_SOURCES
    app/src/editor_backend.cpp
    app/src/editor_font.cpp
    app/src/project.cpp
    app/src/window.cpp
    app/src/window_nodegraph.cpp
    app/src/window_render.cpp
    app/src/window_targets.cpp
)
```

- [x] **Step 6: Verify the test passes**

```powershell
python do.py build debug --target vklive_editor_backend_tests
build\debug\tests\vklive_editor_backend_tests.exe
```

Expected: executable exits with code `0`.

---

## Task 2: Persist the Backend Selection

**Files:**
- Modify: `app/include/app/config.h`
- Modify: `app/src/config.cpp`
- Test: add config assertions to an existing app config test if one exists; otherwise create `tests/editor_config_backend_tests.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write the config round-trip test**

```cpp
// tests/editor_config_backend_tests.cpp
#include <app/config.h>
#include <app/editor_backend.h>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

int main()
{
    const auto path = std::filesystem::temp_directory_path() / "vklive_editor_backend_config.toml";
    {
        std::ofstream out(path);
        out << "editor_backend = \"neovim\"\n";
    }

    AppConfig config;
    config_load(path, config);
    assert(config.editor_backend == EditorBackendKind::Neovim);

    config.editor_backend = EditorBackendKind::Zep;
    config_save(path, config);

    std::ifstream in(path);
    const std::string saved((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    assert(saved.find("editor_backend = \"zep\"") != std::string::npos);

    std::filesystem::remove(path);
}
```

- [x] **Step 2: Wire and run the test to verify it fails**

```cmake
add_executable(vklive_editor_config_backend_tests tests/editor_config_backend_tests.cpp)
target_link_libraries(vklive_editor_config_backend_tests PRIVATE RezonalityAppCore)
add_test(NAME vklive_editor_config_backend_tests COMMAND vklive_editor_config_backend_tests)
```

Run:

```powershell
python do.py build debug --target vklive_editor_config_backend_tests
```

Expected: compilation fails because `AppConfig::editor_backend`, `config_load`, or `config_save` are not exposed in the required shape. Adjust the test only to match the existing config function names after reading `app/include/app/config.h`.

- [x] **Step 3: Add `editor_backend` to `AppConfig`**

```cpp
// app/include/app/config.h
#include <app/editor_backend.h>

struct AppConfig
{
    bool vim_mode = false;
    EditorBackendKind editor_backend = EditorBackendKind::Zep;
    // keep the existing fields in their current order after this addition
};
```

- [x] **Step 4: Load and save the backend string**

```cpp
// app/src/config.cpp
EditorBackendKind backend = config.editor_backend;
const auto editorBackendValue = toml_find_string_or(settings, "editor_backend", editor_backend_to_string(config.editor_backend));
if (editor_backend_from_string(editorBackendValue, backend))
{
    config.editor_backend = backend;
}

// save path
settings["editor_backend"] = editor_backend_to_string(config.editor_backend);
```

- [x] **Step 5: Verify persistence**

```powershell
python do.py build debug --target vklive_editor_config_backend_tests
build\debug\tests\vklive_editor_config_backend_tests.exe
```

Expected: executable exits with code `0`.

---

## Task 3: Create the Switchable Editor Host Boundary

**Files:**
- Create: `app/include/app/editor_host.h`
- Create: `app/src/editor_host.cpp`
- Create: `app/include/app/editor_zep_host.h`
- Create: `app/src/editor_zep_host.cpp`
- Modify: `app/src/editor.cpp`
- Modify: `app/include/app/editor.h`
- Modify: `app/src/main.cpp`
- Test: `tests/editor_host_switch_tests.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write a backend-switch unit test using a null host**

```cpp
// tests/editor_host_switch_tests.cpp
#include <app/editor_backend.h>
#include <app/editor_host.h>

#include <cassert>
#include <memory>

class CountingEditorBackend : public IEditorBackend
{
public:
    explicit CountingEditorBackend(EditorBackendKind kind)
        : m_kind(kind)
    {
    }

    EditorBackendKind kind() const override { return m_kind; }
    void set_project_root(const std::filesystem::path&) override { ++setProjectCount; }
    void set_files(const std::vector<std::filesystem::path>& files, bool activate_first) override
    {
        fileCount = files.size();
        activated = activate_first;
    }
    void show(EditorShowContext&) override { ++showCount; }
    void save_dirty_edit_files() override { ++saveCount; }
    bool has_dirty_edit_files() const override { return false; }

    int setProjectCount = 0;
    int showCount = 0;
    int saveCount = 0;
    std::size_t fileCount = 0;
    bool activated = false;

private:
    EditorBackendKind m_kind;
};

int main()
{
    EditorHost host;
    auto zep = std::make_unique<CountingEditorBackend>(EditorBackendKind::Zep);
    auto nvim = std::make_unique<CountingEditorBackend>(EditorBackendKind::Neovim);

    auto* zepPtr = zep.get();
    auto* nvimPtr = nvim.get();

    host.register_backend(std::move(zep));
    host.register_backend(std::move(nvim));

    assert(host.active_backend() == EditorBackendKind::Zep);
    host.set_project_root("D:/project");
    host.set_files({ "a.frag", "b.vert" }, true);

    host.set_active_backend(EditorBackendKind::Neovim);
    assert(host.active_backend() == EditorBackendKind::Neovim);
    assert(nvimPtr->setProjectCount == 1);
    assert(nvimPtr->fileCount == 2);
    assert(nvimPtr->activated);

    host.set_active_backend(EditorBackendKind::Zep);
    assert(zepPtr->setProjectCount == 1);
    assert(zepPtr->fileCount == 2);
}
```

- [x] **Step 2: Run the test to verify it fails**

```powershell
python do.py build debug --target vklive_editor_host_switch_tests
```

Expected: compilation fails because `IEditorBackend` and `EditorHost` do not exist.

- [x] **Step 3: Add the backend-neutral interface**

```cpp
// app/include/app/editor_host.h
#pragma once

#include <app/editor_backend.h>

#include <filesystem>
#include <memory>
#include <unordered_map>
#include <vector>

struct EditorShowContext
{
    bool focus = false;
};

class IEditorBackend
{
public:
    virtual ~IEditorBackend() = default;
    virtual EditorBackendKind kind() const = 0;
    virtual void set_project_root(const std::filesystem::path& root) = 0;
    virtual void set_files(const std::vector<std::filesystem::path>& files, bool activate_first) = 0;
    virtual void show(EditorShowContext& context) = 0;
    virtual void save_dirty_edit_files() = 0;
    virtual bool has_dirty_edit_files() const = 0;
};

class EditorHost
{
public:
    void register_backend(std::unique_ptr<IEditorBackend> backend);
    bool set_active_backend(EditorBackendKind kind);
    EditorBackendKind active_backend() const;
    void set_project_root(const std::filesystem::path& root);
    void set_files(std::vector<std::filesystem::path> files, bool activate_first);
    void show(EditorShowContext& context);
    void save_dirty_edit_files();
    bool has_dirty_edit_files() const;

private:
    IEditorBackend* active();
    const IEditorBackend* active() const;
    void sync_backend(IEditorBackend& backend);

    EditorBackendKind m_activeBackend = EditorBackendKind::Zep;
    std::filesystem::path m_projectRoot;
    std::vector<std::filesystem::path> m_files;
    bool m_activateFirst = false;
    std::unordered_map<EditorBackendKind, std::unique_ptr<IEditorBackend>> m_backends;
};
```

- [x] **Step 4: Implement the host switcher**

```cpp
// app/src/editor_host.cpp
#include <app/editor_host.h>

void EditorHost::register_backend(std::unique_ptr<IEditorBackend> backend)
{
    if (!backend)
    {
        return;
    }

    const auto kind = backend->kind();
    sync_backend(*backend);
    m_backends[kind] = std::move(backend);
}

bool EditorHost::set_active_backend(EditorBackendKind kind)
{
    auto itr = m_backends.find(kind);
    if (itr == m_backends.end())
    {
        return false;
    }

    m_activeBackend = kind;
    sync_backend(*itr->second);
    return true;
}

EditorBackendKind EditorHost::active_backend() const
{
    return m_activeBackend;
}

void EditorHost::set_project_root(const std::filesystem::path& root)
{
    m_projectRoot = root;
    for (auto& [_, backend] : m_backends)
    {
        backend->set_project_root(m_projectRoot);
    }
}

void EditorHost::set_files(std::vector<std::filesystem::path> files, bool activate_first)
{
    m_files = std::move(files);
    m_activateFirst = activate_first;
    for (auto& [_, backend] : m_backends)
    {
        backend->set_files(m_files, m_activateFirst);
    }
}

void EditorHost::show(EditorShowContext& context)
{
    if (auto* backend = active())
    {
        backend->show(context);
    }
}

void EditorHost::save_dirty_edit_files()
{
    if (auto* backend = active())
    {
        backend->save_dirty_edit_files();
    }
}

bool EditorHost::has_dirty_edit_files() const
{
    if (const auto* backend = active())
    {
        return backend->has_dirty_edit_files();
    }
    return false;
}

IEditorBackend* EditorHost::active()
{
    auto itr = m_backends.find(m_activeBackend);
    return itr == m_backends.end() ? nullptr : itr->second.get();
}

const IEditorBackend* EditorHost::active() const
{
    auto itr = m_backends.find(m_activeBackend);
    return itr == m_backends.end() ? nullptr : itr->second.get();
}

void EditorHost::sync_backend(IEditorBackend& backend)
{
    backend.set_project_root(m_projectRoot);
    backend.set_files(m_files, m_activateFirst);
}
```

- [ ] **Step 5: Move Zep behind `IEditorBackend` without changing behavior**

Move the current `ZepWrapper` implementation from `app/src/editor.cpp` into `app/src/editor_zep_host.cpp`. Keep the old free functions in `app/include/app/editor.h` as forwarding wrappers for one intermediate commit so `main.cpp` can be migrated separately.

```cpp
// app/include/app/editor_zep_host.h
#pragma once

#include <app/editor_host.h>

#include <memory>

class ZepEditorBackend final : public IEditorBackend
{
public:
    ZepEditorBackend();
    ~ZepEditorBackend() override;

    EditorBackendKind kind() const override { return EditorBackendKind::Zep; }
    void set_project_root(const std::filesystem::path& root) override;
    void set_files(const std::vector<std::filesystem::path>& files, bool activate_first) override;
    void show(EditorShowContext& context) override;
    void save_dirty_edit_files() override;
    bool has_dirty_edit_files() const override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
```

- [ ] **Step 6: Verify the switcher test passes and Zep still builds**

```powershell
python do.py build debug --target vklive_editor_host_switch_tests
build\debug\tests\vklive_editor_host_switch_tests.exe
python do.py build debug --target Rezonality
```

Expected: test executable exits with code `0`, and `Rezonality` compiles.

---

## Task 4: Copy the Minimal Draxul Neovim Static Library

**Files:**
- Create: `libs/vklive_nvim/CMakeLists.txt`
- Create: `libs/vklive_nvim/include/vklive_nvim/nvim_host.h`
- Copy and trim: `../Draxul/libs/draxul-nvim/include/*` to `libs/vklive_nvim/include/vklive_nvim/draxul_nvim/*`
- Copy and trim: `../Draxul/libs/draxul-nvim/src/*` to `libs/vklive_nvim/src/nvim/*`
- Copy and trim: `../Draxul/libs/draxul-grid/include/*` and `src/*`
- Copy and trim: `../Draxul/libs/draxul-font/include/*` and `src/*`
- Copy and trim: selected files from `../Draxul/libs/draxul-runtime-support`
- Copy: `../Draxul/build/_deps/mpack-src/src/mpack/*` to `libs/vklive_nvim/third_party/mpack/*`
- Modify: root `CMakeLists.txt`
- Test: `tests/nvim_rpc_codec_tests.cpp`, copied from Draxul and adjusted to the new include paths

Progress note: `NvimProcess`, `MpackValue`, the MPack codec, the RPC reader/request/notification path, and the first UI grid-event handler have been copied/adapted into `libs/vklive_nvim` with focused tests. Font atlas work, highlight color resolution, and renderer copy remain open.

- [ ] **Step 1: Copy sources into a VkLive-owned static library directory**

```powershell
New-Item -ItemType Directory -Force libs\vklive_nvim\include\vklive_nvim | Out-Null
New-Item -ItemType Directory -Force libs\vklive_nvim\src\nvim | Out-Null
New-Item -ItemType Directory -Force libs\vklive_nvim\src\grid | Out-Null
New-Item -ItemType Directory -Force libs\vklive_nvim\src\font | Out-Null
New-Item -ItemType Directory -Force libs\vklive_nvim\src\runtime | Out-Null
New-Item -ItemType Directory -Force libs\vklive_nvim\third_party\mpack | Out-Null
Copy-Item ..\Draxul\libs\draxul-nvim\include\* libs\vklive_nvim\include\vklive_nvim\ -Recurse
Copy-Item ..\Draxul\libs\draxul-nvim\src\* libs\vklive_nvim\src\nvim\ -Recurse
Copy-Item ..\Draxul\libs\draxul-grid\include\* libs\vklive_nvim\include\vklive_nvim\ -Recurse
Copy-Item ..\Draxul\libs\draxul-grid\src\* libs\vklive_nvim\src\grid\ -Recurse
Copy-Item ..\Draxul\libs\draxul-font\include\* libs\vklive_nvim\include\vklive_nvim\ -Recurse
Copy-Item ..\Draxul\libs\draxul-font\src\* libs\vklive_nvim\src\font\ -Recurse
Copy-Item ..\Draxul\build\_deps\mpack-src\src\mpack\* libs\vklive_nvim\third_party\mpack\ -Recurse
```

- [ ] **Step 2: Remove non-Neovim dependencies from the copied source**

Use `rg` to find Draxul dependencies:

```powershell
rg "draxul-host|HostManager|Terminal|PowerShell|Bash|Wsl|MegaCity|NanoVG|SDL3|imgui|vulkan|metal" libs\vklive_nvim
```

Keep files that support:

```text
NvimProcess
NvimRpc
UiEventHandler
NvimInput behavior after SDL2 adaptation
Grid
Highlight/style state
Font grid metrics
Glyph atlas data
Startup resize state
Cursor blink state
```

Delete copied files that only support:

```text
terminal hosts
Draxul host chrome
host manager
non-Neovim demos
Draxul application settings
Draxul top-level renderer window ownership
```

- [ ] **Step 3: Add the static library CMake target**

```cmake
add_library(vklive_nvim STATIC
    src/nvim/mpack_codec.cpp
    src/nvim/nvim_process.cpp
    src/nvim/rpc.cpp
    src/nvim/ui_events.cpp
    src/grid/grid.cpp
    src/font/text_service.cpp
    src/font/font_manager.cpp
    src/font/font_resolver.cpp
    src/font/glyph_cache.cpp
    src/font/text_shaper.cpp
    src/runtime/cursor_blinker.cpp
    src/runtime/startup_resize_state.cpp
    third_party/mpack/mpack-common.c
    third_party/mpack/mpack-expect.c
    third_party/mpack/mpack-node.c
    third_party/mpack/mpack-platform.c
    third_party/mpack/mpack-reader.c
    third_party/mpack/mpack-writer.c
)

target_include_directories(vklive_nvim
    PUBLIC include
    PRIVATE third_party
)

target_compile_features(vklive_nvim PUBLIC cxx_std_20)
target_link_libraries(vklive_nvim PUBLIC fmt::fmt)
target_link_libraries(vklive_nvim PRIVATE SDL2::SDL2)
target_link_libraries(vklive_nvim PRIVATE Freetype::Freetype harfbuzz::harfbuzz)
```

- [ ] **Step 4: Wire the root build**

```cmake
add_subdirectory(libs/vklive_nvim)
target_link_libraries(RezonalityAppCore PUBLIC vklive_nvim)
```

- [ ] **Step 5: Copy the Draxul codec tests first**

Copy Draxul's codec tests and only adjust include paths and target name:

```powershell
Copy-Item ..\Draxul\tests\rpc_codec_tests.cpp tests\nvim_rpc_codec_tests.cpp
```

```cmake
add_executable(vklive_nvim_rpc_codec_tests tests/nvim_rpc_codec_tests.cpp)
target_link_libraries(vklive_nvim_rpc_codec_tests PRIVATE vklive_nvim)
add_test(NAME vklive_nvim_rpc_codec_tests COMMAND vklive_nvim_rpc_codec_tests)
```

- [ ] **Step 6: Verify the copied library preserves the Draxul codec behavior**

```powershell
python do.py build debug --target vklive_nvim_rpc_codec_tests
build\debug\tests\vklive_nvim_rpc_codec_tests.exe
```

Expected: codec test executable exits with code `0`.

---

## Task 5: Add the VkLive Neovim Host API

**Files:**
- Create: `libs/vklive_nvim/include/vklive_nvim/nvim_host.h`
- Create: `libs/vklive_nvim/src/nvim_host.cpp`
- Test: `tests/nvim_host_command_tests.cpp`
- Modify: `libs/vklive_nvim/CMakeLists.txt`

- [x] **Step 1: Write the host command test without launching Neovim**

```cpp
// tests/nvim_host_command_tests.cpp
#include <vklive_nvim/nvim_host.h>

#include <cassert>
#include <filesystem>
#include <string>
#include <vector>

int main()
{
    vklive_nvim::NvimProjectFiles files;
    files.project_root = "D:/projects/demo";
    files.files = { "D:/projects/demo/a.frag", "D:/projects/demo/b.vert" };

    const auto commands = vklive_nvim::build_open_project_tab_commands(files);
    assert(commands.size() == 2);
    assert(commands[0].find("tabedit") != std::string::npos);
    assert(commands[0].find("a.frag") != std::string::npos);
    assert(commands[1].find("tabedit") != std::string::npos);
    assert(commands[1].find("b.vert") != std::string::npos);
}
```

- [x] **Step 2: Run the test to verify it fails**

```powershell
python do.py build debug --target vklive_nvim_host_command_tests
```

Expected: compilation fails because `vklive_nvim/nvim_host.h` does not exist.

- [x] **Step 3: Add the public host API**

```cpp
// libs/vklive_nvim/include/vklive_nvim/nvim_host.h
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace vklive_nvim
{

struct NvimProjectFiles
{
    std::filesystem::path project_root;
    std::vector<std::filesystem::path> files;
};

struct NvimHostOptions
{
    std::filesystem::path project_root;
    std::string executable = "nvim";
    int columns = 80;
    int rows = 24;
};

std::vector<std::string> build_open_project_tab_commands(const NvimProjectFiles& project);

class NvimHost
{
public:
    NvimHost();
    ~NvimHost();

    NvimHost(const NvimHost&) = delete;
    NvimHost& operator=(const NvimHost&) = delete;

    bool start(const NvimHostOptions& options);
    void stop();
    bool running() const;
    void resize(int columns, int rows);
    void open_project_files(const NvimProjectFiles& project);
    void pump();
    void send_input(std::string_view input);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace vklive_nvim
```

- [x] **Step 4: Implement project file tab commands**

```cpp
// libs/vklive_nvim/src/nvim_host.cpp
#include <vklive_nvim/nvim_host.h>

#include <sstream>

namespace vklive_nvim
{

namespace
{

std::string escape_vim_path(const std::filesystem::path& path)
{
    std::string value = path.generic_string();
    std::string escaped;
    escaped.reserve(value.size() + 8);
    for (char c : value)
    {
        if (c == ' ' || c == '\\' || c == '|' || c == '"' || c == '%')
        {
            escaped.push_back('\\');
        }
        escaped.push_back(c);
    }
    return escaped;
}

} // namespace

std::vector<std::string> build_open_project_tab_commands(const NvimProjectFiles& project)
{
    std::vector<std::string> commands;
    commands.reserve(project.files.size());
    for (const auto& file : project.files)
    {
        commands.push_back("tabedit " + escape_vim_path(file));
    }
    return commands;
}

} // namespace vklive_nvim
```

- [x] **Step 5: Implement the host by delegating to copied Draxul code**

Use `NvimProcess::spawn("nvim", { "--embed" }, options.project_root)`, create `NvimRpc`, attach UI with:

```cpp
rpc.request("nvim_ui_attach", {
    options.columns,
    options.rows,
    std::map<std::string, bool>{
        { "rgb", true },
        { "ext_linegrid", true },
        { "ext_multigrid", false },
    },
});
```

Set startup options:

```vim
set termguicolors
set noshowmode
set mouse=a
```

Open files with the `tabedit` commands from `build_open_project_tab_commands()`. Do not create external tabs or split panes in VkLive.

Progress note: `NvimHost` now starts over either a real `NvimProcess` or injected `IRpcTransport`, attaches `ext_linegrid`, sends startup `nvim_command` requests, opens project files with native `tabedit` commands, drains `redraw` notifications into `RenderModel`, and sends input through `nvim_input`. This is covered by `vklive_nvim_host_runtime_tests` using an auto-reply embedded-Neovim transport.

- [x] **Step 6: Verify command test and a guarded process test**

```powershell
python do.py build debug --target vklive_nvim_host_command_tests
build\debug\tests\vklive_nvim_host_command_tests.exe
nvim --version
```

Expected: command test exits with code `0`. If `nvim --version` succeeds, add and run a guarded process smoke test that starts `nvim --embed`, attaches the UI, sends `:qall!`, and exits cleanly.

Progress note: `vklive_nvim_host_process_smoke_tests` guards on `nvim --version`, starts `NvimHost` against the real `nvim --embed` process, attaches the UI, pumps briefly, and stops the process cleanly.

---

## Task 6: Adapt Draxul Input to SDL2 and ImGui Focus

**Files:**
- Create: `libs/vklive_nvim/include/vklive_nvim/input.h`
- Create: `libs/vklive_nvim/src/input.cpp`
- Test: `tests/nvim_input_tests.cpp`
- Modify: `libs/vklive_nvim/CMakeLists.txt`
- Modify: `app/src/editor_nvim_host.cpp`

- [x] **Step 1: Copy Draxul input tests and convert SDL3 symbols to SDL2 symbols**

```cpp
// tests/nvim_input_tests.cpp
#include <vklive_nvim/input.h>

#include <SDL.h>
#include <cassert>

int main()
{
    assert(vklive_nvim::sdl_key_to_nvim(SDLK_RETURN, KMOD_NONE) == "<CR>");
    assert(vklive_nvim::sdl_key_to_nvim(SDLK_ESCAPE, KMOD_NONE) == "<Esc>");
    assert(vklive_nvim::sdl_key_to_nvim(SDLK_s, KMOD_CTRL) == "<C-s>");
    assert(vklive_nvim::sdl_key_to_nvim(SDLK_TAB, KMOD_SHIFT) == "<S-Tab>");
}
```

- [x] **Step 2: Run the test to verify it fails**

```powershell
python do.py build debug --target vklive_nvim_input_tests
```

Expected: compilation fails because `vklive_nvim/input.h` does not exist.

- [x] **Step 3: Add the SDL2 input adapter**

```cpp
// libs/vklive_nvim/include/vklive_nvim/input.h
#pragma once

#include <SDL.h>
#include <string>

namespace vklive_nvim
{

std::string sdl_key_to_nvim(SDL_Keycode key, SDL_Keymod mods);

} // namespace vklive_nvim
```

```cpp
// libs/vklive_nvim/src/input.cpp
#include <vklive_nvim/input.h>

namespace vklive_nvim
{

std::string sdl_key_to_nvim(SDL_Keycode key, SDL_Keymod mods)
{
    const bool ctrl = (mods & KMOD_CTRL) != 0;
    const bool shift = (mods & KMOD_SHIFT) != 0;

    switch (key)
    {
    case SDLK_RETURN:
        return "<CR>";
    case SDLK_ESCAPE:
        return "<Esc>";
    case SDLK_BACKSPACE:
        return "<BS>";
    case SDLK_TAB:
        return shift ? "<S-Tab>" : "<Tab>";
    default:
        break;
    }

    if (ctrl && key >= SDLK_a && key <= SDLK_z)
    {
        std::string out = "<C-";
        out.push_back(static_cast<char>(key));
        out.push_back('>');
        return out;
    }

    if (!ctrl && key >= 32 && key <= 126)
    {
        return std::string(1, static_cast<char>(key));
    }

    return {};
}

} // namespace vklive_nvim
```

- [ ] **Step 4: Route input only when the Neovim ImGui child has focus**

In `app/src/editor_nvim_host.cpp`, call `host.send_input(...)` from SDL2 keyboard text and key events only when the Neovim window is focused. Leave global live-coding shortcuts owned by VkLive.

- [x] **Step 5: Verify input test passes**

```powershell
python do.py build debug --target vklive_nvim_input_tests
build\debug\tests\vklive_nvim_input_tests.exe
```

Expected: executable exits with code `0`.

---

## Task 7: Render the Neovim Grid in the VkLive Window

**Files:**
- Create: `libs/vklive_nvim/include/vklive_nvim/render_model.h`
- Create: `libs/vklive_nvim/src/render_model.cpp`
- Create: `libs/vklive_nvim/include/vklive_nvim/nvim_ui.h`
- Create: `libs/vklive_nvim/src/nvim_ui.cpp`
- Create: `app/include/app/editor_nvim_renderer.h`
- Create: `app/src/editor_nvim_renderer.cpp`
- Modify: `app/src/editor_nvim_host.cpp`
- Test: `tests/nvim_render_model_tests.cpp`
- Test: `tests/nvim_ui_events_tests.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write render-model tests before touching GPU code**

```cpp
// tests/nvim_render_model_tests.cpp
#include <vklive_nvim/render_model.h>

#include <cassert>

int main()
{
    vklive_nvim::RenderModel model;
    model.resize(4, 2);
    model.set_cell(1, 0, U"A", 0xff112233, 0xff445566);
    assert(model.columns() == 4);
    assert(model.rows() == 2);
    const auto& cell = model.cell(1, 0);
    assert(cell.text == U"A");
    assert(cell.foreground == 0xff112233);
    assert(cell.background == 0xff445566);
}
```

- [x] **Step 2: Run the render-model test to verify it fails**

```powershell
python do.py build debug --target vklive_nvim_render_model_tests
```

Expected: compilation fails because `vklive_nvim/render_model.h` does not exist.

- [x] **Step 3: Add a renderer-independent model**

```cpp
// libs/vklive_nvim/include/vklive_nvim/render_model.h
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace vklive_nvim
{

struct RenderCell
{
    std::u32string text;
    std::uint32_t foreground = 0xffffffff;
    std::uint32_t background = 0xff000000;
};

class RenderModel
{
public:
    void resize(int columns, int rows);
    int columns() const { return m_columns; }
    int rows() const { return m_rows; }
    void set_cell(int column, int row, std::u32string text, std::uint32_t foreground, std::uint32_t background);
    const RenderCell& cell(int column, int row) const;

private:
    int m_columns = 0;
    int m_rows = 0;
    std::vector<RenderCell> m_cells;
};

} // namespace vklive_nvim
```

```cpp
// libs/vklive_nvim/src/render_model.cpp
#include <vklive_nvim/render_model.h>

#include <cassert>

namespace vklive_nvim
{

void RenderModel::resize(int columns, int rows)
{
    m_columns = columns;
    m_rows = rows;
    m_cells.assign(static_cast<std::size_t>(columns * rows), RenderCell{});
}

void RenderModel::set_cell(int column, int row, std::u32string text, std::uint32_t foreground, std::uint32_t background)
{
    assert(column >= 0 && column < m_columns);
    assert(row >= 0 && row < m_rows);
    auto& cell = m_cells[static_cast<std::size_t>(row * m_columns + column)];
    cell.text = std::move(text);
    cell.foreground = foreground;
    cell.background = background;
}

const RenderCell& RenderModel::cell(int column, int row) const
{
    assert(column >= 0 && column < m_columns);
    assert(row >= 0 && row < m_rows);
    return m_cells[static_cast<std::size_t>(row * m_columns + column)];
}

} // namespace vklive_nvim
```

Progress note: the first `RenderModel` stores UTF-8 cell text and highlight IDs rather than final resolved foreground/background colors, matching Neovim `grid_line` payloads and Draxul's grid shape. `UiEventHandler` now processes `grid_resize`, `grid_line`, `grid_cursor_goto`, `grid_scroll`, `grid_clear`, and `flush` into that model. Highlight color tables and the ImGui/native GPU renderer remain open.

- [ ] **Step 4: Add the first ImGui draw-list renderer**

Use the copied Draxul font atlas for glyph bitmaps and upload the atlas through `IDevice::FontTexture()` so both Vulkan and Metal follow the existing VkLive texture path. The first renderer draws background rectangles and foreground glyph quads into the current ImGui window's draw list. This gets the Neovim GUI inside VkLive's existing Metal/Vulkan wrapper without adding new device render-pass plumbing.

```cpp
// app/include/app/editor_nvim_renderer.h
#pragma once

#include <vklive_nvim/render_model.h>

struct IDevice;

class NvimImGuiRenderer
{
public:
    void draw(IDevice& device, const vklive_nvim::RenderModel& model, float cell_width, float cell_height);
};
```

- [ ] **Step 5: Replace the draw-list renderer with Draxul's native grid pipeline after the host is visible**

Copy Draxul's `grid_rendering_pipeline`, renderer state, and `grid_*` shaders into `libs/vklive_nvim` and add a VkLive device extension:

```cpp
struct NvimGridDrawRequest
{
    const vklive_nvim::RenderModel* model = nullptr;
    ImVec2 top_left;
    ImVec2 size;
    float cell_width = 0.0f;
    float cell_height = 0.0f;
};
```

Add Vulkan and Metal implementations that draw the grid inside the same frame as ImGui. Keep the draw-list renderer available as a fallback behind a compile-time flag named `VKLIVE_NVIM_IMGUI_RENDERER`.

- [ ] **Step 6: Verify the render model test and visual smoke**

```powershell
python do.py build debug --target vklive_nvim_render_model_tests
build\debug\tests\vklive_nvim_render_model_tests.exe
python do.py build debug --target Rezonality
```

Expected: render-model test exits with code `0`, and `Rezonality` compiles. Manual smoke: run `python do.py run debug`, switch to Neovim, and confirm the editor area paints cells, cursor, and text.

---

## Task 8: Integrate Project File Loading as Neovim Tabs

**Files:**
- Modify: `app/src/editor_nvim_host.cpp`
- Modify: `app/src/project.cpp` only if the existing edit-file discovery needs a reusable helper
- Test: `tests/nvim_project_tabs_tests.cpp`

- [ ] **Step 1: Write a project-tab ordering test**

```cpp
// tests/nvim_project_tabs_tests.cpp
#include <vklive_nvim/nvim_host.h>

#include <cassert>

int main()
{
    vklive_nvim::NvimProjectFiles project;
    project.project_root = "D:/demo";
    project.files = { "D:/demo/main.scenegraph", "D:/demo/shaders/a.frag", "D:/demo/shaders/b.vert" };

    const auto commands = vklive_nvim::build_open_project_tab_commands(project);
    assert(commands.size() == 3);
    assert(commands[0].find("main.scenegraph") != std::string::npos);
    assert(commands[1].find("a.frag") != std::string::npos);
    assert(commands[2].find("b.vert") != std::string::npos);
}
```

- [ ] **Step 2: Verify the test passes with the host command builder**

```powershell
python do.py build debug --target vklive_nvim_project_tabs_tests
build\debug\tests\vklive_nvim_project_tabs_tests.exe
```

Expected: executable exits with code `0`.

- [ ] **Step 3: Use existing VkLive file discovery**

In the backend-neutral host, call the same `scene_is_edit_file()` and `Zest::file_gather_files(root)` path currently used by `zep_update_files`. Pass the resulting ordered file list to `NvimHost::open_project_files()`.

- [ ] **Step 4: Load files as Neovim tabs**

In `NvimHost::open_project_files()`, execute:

```vim
silent! tabonly
tabedit D:/project/file1.frag
tabedit D:/project/file2.vert
tabfirst
```

Do not render a VkLive-side tab bar. Users can use Neovim `gt`, `gT`, `:tabnext`, `:split`, and `:vsplit`.

- [ ] **Step 5: Manual smoke**

```powershell
python do.py run debug
```

Expected: selecting the Neovim backend opens the project edit-file set as native Neovim tabs.

---

## Task 9: Add Live Backend Switching in the UI

**Files:**
- Modify: `app/src/menu.cpp`
- Modify: `app/src/main.cpp`
- Modify: `app/src/config.cpp`
- Test: `tests/editor_host_switch_tests.cpp` extended with persistence expectations

- [ ] **Step 1: Extend the switch test**

```cpp
// Add to tests/editor_host_switch_tests.cpp
assert(host.set_active_backend(EditorBackendKind::Neovim));
assert(host.active_backend() == EditorBackendKind::Neovim);
assert(host.set_active_backend(EditorBackendKind::Zep));
assert(host.active_backend() == EditorBackendKind::Zep);
```

- [ ] **Step 2: Add menu actions**

Use radio menu items:

```cpp
if (ImGui::MenuItem("Zep", nullptr, config.editor_backend == EditorBackendKind::Zep))
{
    config.editor_backend = EditorBackendKind::Zep;
    editorHost.set_active_backend(EditorBackendKind::Zep);
}

if (ImGui::MenuItem("Neovim", nullptr, config.editor_backend == EditorBackendKind::Neovim))
{
    config.editor_backend = EditorBackendKind::Neovim;
    editorHost.set_active_backend(EditorBackendKind::Neovim);
}
```

- [ ] **Step 3: Apply config at startup**

After constructing the editor host and registering both backends:

```cpp
if (!editorHost.set_active_backend(appConfig.editor_backend))
{
    editorHost.set_active_backend(EditorBackendKind::Zep);
}
```

- [ ] **Step 4: Verify live switching**

```powershell
python do.py build debug --target Rezonality
python do.py run debug
```

Expected: the menu can switch between Zep and Neovim without restarting. Switching away from Neovim stops input capture for Neovim. Switching back shows the existing Neovim instance if it is still running, or starts it and opens project tabs.

---

## Task 10: Verification Matrix

**Files:**
- Modify tests as needed for changed executable locations in `CMakeLists.txt`
- No product source changes unless verification reveals a defect

- [ ] **Step 1: Run focused unit tests**

```powershell
build\debug\tests\vklive_editor_backend_tests.exe
build\debug\tests\vklive_editor_config_backend_tests.exe
build\debug\tests\vklive_editor_host_switch_tests.exe
build\debug\tests\vklive_nvim_rpc_codec_tests.exe
build\debug\tests\vklive_nvim_host_command_tests.exe
build\debug\tests\vklive_nvim_input_tests.exe
build\debug\tests\vklive_nvim_render_model_tests.exe
build\debug\tests\vklive_nvim_project_tabs_tests.exe
```

Expected: each executable exits with code `0`.

- [ ] **Step 2: Build the app**

```powershell
python do.py build debug --target Rezonality
```

Expected: app target builds.

- [ ] **Step 3: Run guarded Neovim process smoke**

```powershell
nvim --version
python do.py test debug --filter vklive_nvim_process_smoke
```

Expected: if `nvim` is installed, the smoke starts `nvim --embed`, attaches `ext_linegrid`, sends `:qall!`, and exits. If `nvim` is missing, the test reports skipped instead of failed.

- [ ] **Step 4: Manual visual smoke on Vulkan or Metal**

```powershell
python do.py run debug
```

Expected:

```text
Zep remains usable.
Neovim backend starts.
Project shader/edit files appear as Neovim tabs.
Neovim-native splits and tabs work from commands such as :vsplit and :tabnew.
Typing, normal-mode navigation, mouse focus, and resizing work inside the editor window.
VkLive rendering continues outside the editor.
```

---

## Self-Review

- Spec coverage:
  - Switchable alternative to Zep: Tasks 1, 3, 9.
  - New branch: execution setup before this plan.
  - Draxul Neovim GUI host copied, no submodule: Tasks 4, 5.
  - No terminal hosts or Draxul chrome: Product decisions and Task 4 trim list.
  - Existing Metal/Vulkan wrapper: Task 7 uses VkLive's existing ImGui texture path first, then the Draxul native grid pipeline inside VkLive device rendering.
  - Shader files loaded into tabs: Tasks 5 and 8 use Neovim `tabedit`; Neovim supplies tabs and splits.
  - Live coding interfaces and flash code excluded: Product decisions and tasks avoid those paths.
- Red-flag scan:
  - The plan gives concrete files, commands, and expected outcomes for each task.
- Type consistency:
  - `EditorBackendKind`, `EditorHost`, `IEditorBackend`, and `vklive_nvim::NvimHost` names are introduced before use and stay consistent across later tasks.
