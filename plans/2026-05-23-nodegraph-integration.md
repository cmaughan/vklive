# Nodegraph Integration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Integrate Rezonality/nodegraph as an in-tree static library and expose a dockable Node Graph window in VkLive with zoom, pan, node manipulation, and theme editing.

**Status:** Implemented and verified in Debug and Release on 2026-05-23.

**Architecture:** Import only the nodegraph library/canvas/widget code into `libs/nodegraph`, not its standalone app, vcpkg checkout, SDL3 host, or renderer setup. Adapt nodegraph to VkLive's existing ImGui context, docking-enabled renderer, Zest fontstash font texture path, and Zest global settings manager. The app owns a small `NodeGraphWindow` adapter that creates demo nodes and draws the canvas inside an ImGui dockable window.

**Tech Stack:** C++20, CMake, SDL2, ImGui docking, Zest/Zing, VkLive `IDevice`, vcpkg, CTest.

---

### Task 1: Add Tests That Define The Integration

**Files:**
- Modify: `D:/dev/vklive/CMakeLists.txt`
- Create: `D:/dev/vklive/tests/nodegraph_canvas_tests.cpp`
- Create: `D:/dev/vklive/tests/nodegraph_theme_tests.cpp`
- Create: `D:/dev/vklive/tests/nodegraph_window_tests.cpp`

- [ ] **Step 1: Add CTest targets for nodegraph**

Add three test executables to `CMakeLists.txt` near the other app/core tests:

```cmake
add_executable(vklive_nodegraph_canvas_tests tests/nodegraph_canvas_tests.cpp)
target_link_libraries(vklive_nodegraph_canvas_tests PRIVATE NodeGraph)
target_include_directories(vklive_nodegraph_canvas_tests PRIVATE ${CMAKE_BINARY_DIR})

add_executable(vklive_nodegraph_theme_tests tests/nodegraph_theme_tests.cpp)
target_link_libraries(vklive_nodegraph_theme_tests PRIVATE RezonalityAppCore)
target_include_directories(vklive_nodegraph_theme_tests PRIVATE ${CMAKE_BINARY_DIR})

add_executable(vklive_nodegraph_window_tests tests/nodegraph_window_tests.cpp)
target_link_libraries(vklive_nodegraph_window_tests PRIVATE RezonalityAppCore)
target_include_directories(vklive_nodegraph_window_tests PRIVATE ${CMAKE_BINARY_DIR})

add_test(NAME vklive_nodegraph_canvas_tests COMMAND vklive_nodegraph_canvas_tests)
add_test(NAME vklive_nodegraph_theme_tests COMMAND vklive_nodegraph_theme_tests)
add_test(NAME vklive_nodegraph_window_tests COMMAND vklive_nodegraph_window_tests)
```

- [ ] **Step 2: Write the canvas input/zoom test**

`tests/nodegraph_canvas_tests.cpp` should create a no-op canvas subclass and fake `Zest::IFontTexture`, set mouse-wheel input, call `HandleMouse()`, and assert that world scale changes while the world point under the cursor remains stable enough for a pan/zoom canvas.

- [ ] **Step 3: Write theme seed tests**

`tests/nodegraph_theme_tests.cpp` should call `nodegraph_seed_default_theme()`, then assert representative values exist in `Zest::GlobalSettingsManager`: `NodeGraph::s_gridLineSize`, `NodeGraph::c_nodeCenterColor`, and `NodeGraph::b_debugShowLayout`.

- [ ] **Step 4: Write window model tests**

`tests/nodegraph_window_tests.cpp` should construct `NodeGraphWindow`, assert it starts uninitialized, call `BuildDemoGraphForTests()`, then assert it has at least two nodes.

- [ ] **Step 5: Verify red**

Run: `python do.py build debug`

Expected: failure because `NodeGraph`, `nodegraph/window_nodegraph.h`, and theme seed APIs do not exist yet.

### Task 2: Import Nodegraph As A Static Library

**Files:**
- Create: `D:/dev/vklive/libs/nodegraph/include/nodegraph/**`
- Create: `D:/dev/vklive/libs/nodegraph/src/**`
- Modify: `D:/dev/vklive/CMakeLists.txt`

- [ ] **Step 1: Copy source**

Copy these upstream folders from `https://github.com/Rezonality/nodegraph` into `libs/nodegraph`: `include/nodegraph`, `src/canvas.cpp`, `src/canvas_imgui.cpp`, `src/widgets/*.cpp`, `LICENSE`, `project.natvis`.

- [ ] **Step 2: Replace nodegraph fonts with a Zest shim**

Replace `libs/nodegraph/include/nodegraph/fonts.h` with aliases and using declarations for `Zest::FontContext`, `Zest::IFontTexture`, and the `Zest::fonts_*` functions. Do not compile upstream `src/fonts.cpp`.

- [ ] **Step 3: Exclude standalone dependencies**

Do not copy or compile upstream `app/`, `src/vulkan/vulkan_imgui_texture.cpp`, `src/theme.cpp`, `src/fonts.cpp`, SDL3 sources, or Soundpipe demo code.

- [ ] **Step 4: Add the CMake target**

Add `NodeGraph` static library with the imported source files, include directories, and links to `Zing::Zing`, `glm::glm`, and `fmt::fmt-header-only`.

- [ ] **Step 5: Build until the static library compiles**

Run: `python do.py build debug`

Expected: compile failures only from integration mismatch. Fix namespace/includes minimally, preserving upstream structure where possible.

### Task 3: Add Theme Defaults And App-Wide Theme Window

**Files:**
- Create: `D:/dev/vklive/app/include/app/nodegraph_theme.h`
- Create: `D:/dev/vklive/app/src/nodegraph_theme.cpp`
- Modify: `D:/dev/vklive/app/include/app/menu.h`
- Modify: `D:/dev/vklive/app/src/menu.cpp`
- Modify: `D:/dev/vklive/app/src/main.cpp`
- Modify: `D:/dev/vklive/CMakeLists.txt`

- [ ] **Step 1: Implement `nodegraph_seed_default_theme()`**

Seed the current `Zest::GlobalSettingsManager` theme with nodegraph defaults derived from upstream `settings.toml`.

- [ ] **Step 2: Implement `nodegraph_load_theme_file()` and `nodegraph_save_theme_file()`**

Wrap `Zest::GlobalSettingsManager::Load/Save` so invalid files return `false` and never throw.

- [ ] **Step 3: Add a Theme Editor window toggle**

Add `themeEditor` to `WindowEnables`, register it with the layout manager, expose it in the existing Window menu, and draw `Zest::GlobalSettingsManager::Instance().DrawGUI("Theme", &g_WindowEnables.themeEditor)`.

- [ ] **Step 4: Load and save theme settings**

At app startup, seed defaults and load `settings/theme.toml` if present. On shutdown, save the theme file.

- [ ] **Step 5: Verify theme tests**

Run: `python do.py build debug; python do.py test debug -- -R nodegraph_theme`

Expected: `vklive_nodegraph_theme_tests` passes.

### Task 4: Add The Dockable Node Graph Window

**Files:**
- Create: `D:/dev/vklive/app/include/app/window_nodegraph.h`
- Create: `D:/dev/vklive/app/src/window_nodegraph.cpp`
- Modify: `D:/dev/vklive/include/vklive/IDevice.h`
- Modify: Vulkan and Metal device classes to expose their `Zest::IFontTexture`
- Modify: `D:/dev/vklive/app/include/app/menu.h`
- Modify: `D:/dev/vklive/app/src/menu.cpp`
- Modify: `D:/dev/vklive/app/src/main.cpp`
- Modify: `D:/dev/vklive/CMakeLists.txt`

- [ ] **Step 1: Expose the shared font texture**

Add `virtual Zest::IFontTexture* FontTexture() = 0;` to `IDevice` and implement it in `VulkanDevice` and `MetalDevice` by returning `ctx.spFontTexture.get()`.

- [ ] **Step 2: Implement `NodeGraphWindow`**

Create a class that owns `std::unique_ptr<NodeGraph::CanvasImGui>`, builds a demo graph using `Node`, `Socket`, `Slider`, `Knob`, and `TextLabel`, and exposes `Draw(bool* open, IDevice& device)`.

- [ ] **Step 3: Add the window toggle**

Add `nodeGraph` to `WindowEnables`, register it with the layout manager, and draw the window each frame when enabled.

- [ ] **Step 4: Keep input contained**

Pass `forceCanCapture = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)` to `canvas_imgui_update_state` so nodegraph only captures when its window is active/hovered.

- [ ] **Step 5: Verify window model tests**

Run: `python do.py build debug; python do.py test debug -- -R nodegraph_window`

Expected: `vklive_nodegraph_window_tests` passes.

### Task 5: Final Verification

**Files:**
- Review all changed source files

- [ ] **Step 1: Run unit tests**

Run: `python -m unittest tests.test_do`

Expected: all Python wrapper tests pass.

- [ ] **Step 2: Build Debug**

Run: `python do.py build debug`

Expected: build exits 0.

- [ ] **Step 3: Run Debug CTest**

Run: `python do.py test debug`

Expected: all CTest tests pass.

- [ ] **Step 4: Build Release**

Run: `python do.py build release`

Expected: build exits 0.

- [ ] **Step 5: Smoke launch**

Run: `python do.py run release -- --smoke-test`

Expected: no configure pass on a current build tree, executable exits 0.

- [ ] **Step 6: Manual UI check**

Run: `python do.py run debug`, open Window > Node Graph, dock the window, mouse-wheel zoom, right-drag pan, left-drag a node, and open Window > Theme to edit nodegraph colors.
