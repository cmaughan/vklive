# VkLive Source Review

## Scope And Method

I reviewed the repository directly from disk under `/Users/cmaughan/dev/vklive`, starting with `rg --files` and then reading the source files themselves. I did not use any combined/generated source dump and did not edit repository files.

Areas inspected include `app/`, `src/`, `include/vklive/`, `tests/`, `examples/`, `run_tree/projects/`, `cmake/`, `CMakeLists.txt`, `CMakePresets.json`, `vcpkg.json`, `do.py`, `plans/`, and selected project-owned integration points around vendored libraries.

I also ran:

```sh
python3 do.py test debug -- -N
```

This listed 47 CTest tests. I did not run the full build or full test suite because the request was review-only.

## Executive Summary

VkLive has a strong foundation for a live graphics tool: clear app/core target separation, real Vulkan and Metal backends, a practical `do.py` workflow, useful sample projects, scenegraph diagnostics, shader reflection, render parity tests, and editor integration work that goes beyond a toy prototype.

The largest stability risks are around live reload and resource lifetime. Scene building currently happens on a background thread that calls into the live render device while the UI thread may be rendering, presenting, destroying scenes, or recreating the backend. That creates Vulkan and C++ data races around scene maps, model caches, descriptors, images, and device state. Separately, the scene parser has a concrete out-of-bounds bug for single-value vectors such as `scale: 1`, which is exactly the kind of mistake users will make while live editing.

The main architectural opportunity is to make scene parsing, asset loading, GPU resource creation, and editor/UI ownership more explicit. The app wants a pipeline like: parse into immutable CPU scene description, validate, build or diff GPU resources on the render thread, then atomically publish the new live scene. Today those responsibilities are partially interleaved across `scene_build`, backend `InitScene`, global device state, cached model mutation, and editor callbacks.

## Findings

### 1. Critical: single-value vector parsing can read out of bounds

`getVector` in [src/scene.cpp](/Users/cmaughan/dev/vklive/src/scene.cpp:578) computes the loop bound with:

```cpp
std::max(ret.length(), std::min(1, int(vals.size())))
```

For a `glm::vec3` and one parsed value, this loops three times and reads `vals[1]` and `vals[2]`. The grammar allows one-value vectors in [src/scene.cpp](/Users/cmaughan/dev/vklive/src/scene.cpp:221), and several fields accept one to three values, including surface `size`/`scale`, model `scale`, and geometry `scale`.

Impact: a normal live-edit mistake such as `scale: 1` or `size: 0.5` can crash the application instead of producing a validation message.

Recommendation: use `std::min(ret.length(), vals.size())` for the direct copy path. If scalar broadcast is intended, implement it explicitly and cover it with tests.

### 2. Critical: background scene reload calls the live render device without synchronization

The reload worker in [app/src/main.cpp](/Users/cmaughan/dev/vklive/app/src/main.cpp:319) builds scenes and calls `g_pDevice->InitScene(*spProject->spScene)` from the worker thread at [app/src/main.cpp](/Users/cmaughan/dev/vklive/app/src/main.cpp:343). At the same time, the UI thread can call `ValidateSwapChain`, `Render_3D`, `DestroyScene`, `WaitIdle`, `Imgui_Render`, `Present`, or recreate `g_pDevice`.

`IDevice` exposes no threading contract in [include/vklive/IDevice.h](/Users/cmaughan/dev/vklive/include/vklive/IDevice.h:77). Vulkan scene creation mutates `VulkanContext::mapVulkanScene`, a plain `std::map`, through [src/vulkan/vulkan_scene.cpp](/Users/cmaughan/dev/vklive/src/vulkan/vulkan_scene.cpp:58) while rendering can read it through [src/vulkan/vulkan_scene.cpp](/Users/cmaughan/dev/vklive/src/vulkan/vulkan_scene.cpp:39).

Impact: data races, stale scene lookups, descriptor/image lifetime bugs, and device-loss recovery races. This is the biggest mismatch with a fault-tolerant live editor.

Recommendation: parse and validate on the worker, but enqueue GPU resource creation/destruction onto the render thread. Publish completed scenes through a single owner with explicit generation IDs.

### 3. Critical: model hot reload mutates cached models and does not refresh existing GPU buffers

The Vulkan model cache in [src/vulkan/vulkan_model.cpp](/Users/cmaughan/dev/vklive/src/vulkan/vulkan_model.cpp:145) reuses a cached `ModelPtr`. If the file timestamp changes, it calls `model_load(*itr->second, createInfo)` on the existing object. That object may still be referenced by the currently rendered scene.

After mutation, GPU upload only happens when `!model.indices.buffer` in [src/vulkan/vulkan_model.cpp](/Users/cmaughan/dev/vklive/src/vulkan/vulkan_model.cpp:161). Changed vertices, indices, bounds, or material data will not reliably reach GPU memory after the first load.

Impact: model edits can produce stale rendering, partially mutated CPU/GPU state, or races with the current render.

Recommendation: separate CPU asset cache from GPU resources. Load changed models into a new immutable CPU asset, create new GPU buffers, then swap scene ownership after successful upload.

### 4. High: Vulkan shader compiler launch failure is not reported into scene errors

In [src/vulkan/vulkan_shader.cpp](/Users/cmaughan/dev/vklive/src/vulkan/vulkan_shader.cpp:105), failure to run `glslangConvertor` logs to stdout and returns `nullptr`, but does not call `scene_report_error`. The Metal path does report equivalent compiler-launch failures in [src/metal/metal_shader.mm](/Users/cmaughan/dev/vklive/src/metal/metal_shader.mm:325).

`vulkan_scene_create` rejects scenes based on `scene.errors` and `scene.valid` in [src/vulkan/vulkan_scene.cpp](/Users/cmaughan/dev/vklive/src/vulkan/vulkan_scene.cpp:69), so this failure can leave the scene looking valid while shader stages or pipelines are missing.

Impact: blank output or null-pipeline behavior instead of actionable editor diagnostics.

Recommendation: report compiler launch failures as scene errors with file context. Treat failed shader stage creation as a failed scene build.

### 5. High: shader SPIR-V output path can collide across projects and shader directories

Vulkan shader compilation writes to `/tmp/vklive/<filename>.spirv` in [src/vulkan/vulkan_shader.cpp](/Users/cmaughan/dev/vklive/src/vulkan/vulkan_shader.cpp:41).

Impact: two shaders with the same basename in different directories or projects can overwrite each other. Multiple VkLive instances can also collide.

Recommendation: include a hash of the absolute source path, project path, shader stage, and process ID in the output path, or use a per-project cache directory.

### 6. High: scene parser leaks AST memory on exceptions after parse succeeds

`scene_build` stores the parser output at [src/scene.cpp](/Users/cmaughan/dev/vklive/src/scene.cpp:491) and manually deletes it near [src/scene.cpp](/Users/cmaughan/dev/vklive/src/scene.cpp:1099). Many processing paths can throw before that delete, including `getChild`, `std::stof`, filesystem operations, and project validation. The catch blocks at [src/scene.cpp](/Users/cmaughan/dev/vklive/src/scene.cpp:1119) do not clean up `r.output`.

Impact: repeated live edits with malformed scene files can leak memory.

Recommendation: wrap parser output in RAII immediately after parse succeeds.

### 7. High: scene building creates `default.scenegraph` in user-selected folders

If a scenegraph file does not exist, [src/scene.cpp](/Users/cmaughan/dev/vklive/src/scene.cpp:340) creates one with `# Scenegraph`.

Impact: opening the wrong directory can mutate user files without consent. For a creative desktop tool, project creation should be explicit.

Recommendation: distinguish “open existing project” from “create project”. Missing scenegraph should produce a non-destructive error with a clear create action.

### 8. High: device-loss recovery races with reload and is marked as not working

The device-loss path in [app/src/main.cpp](/Users/cmaughan/dev/vklive/app/src/main.cpp:416) says recovery does not work, then attempts to destroy and recreate the device at [app/src/main.cpp](/Users/cmaughan/dev/vklive/app/src/main.cpp:422). The reload worker can still call into `g_pDevice` during this process.

Impact: device-lost handling can compound the original fault with use-after-free or backend state races.

Recommendation: stop or quiesce reload work before device destruction. Rebuild the renderer through a single render-owner state machine.

### 9. Medium: `Scene::passOrder` can hold dangling raw pointers

`Scene::passOrder` stores raw `Pass*` values in [include/vklive/scene.h](/Users/cmaughan/dev/vklive/include/vklive/scene.h:237). `scene_build` pushes `spPass.get()` at [src/scene.cpp](/Users/cmaughan/dev/vklive/src/scene.cpp:1075), even when the pass was not added to `spScene->passes`.

Current backends mostly iterate `scene.passes`, so this is partly latent, but it is dangerous state for future work.

Impact: future pass-order features can accidentally dereference freed pass objects.

Recommendation: store pass names, indices, or owning smart pointers consistently. Keep invalid passes out of published scene state.

### 10. Medium: project copy does not create destination subdirectories

`project_copy` copies files to `destPath / relative` in [app/src/project.cpp](/Users/cmaughan/dev/vklive/app/src/project.cpp:210), but does not create each destination parent directory. The test in [tests/project_copy_tests.cpp](/Users/cmaughan/dev/vklive/tests/project_copy_tests.cpp:124) uses a flat project, so nested assets are not covered.

Impact: copying projects with `models/`, `textures/`, or nested shader folders can fail.

Recommendation: call `fs::create_directories(dest.parent_path())` before each copy and add a nested asset test using a PBR-style project.

### 11. Medium: project copy scans by extension rather than dependency graph

`project_copy` gathers files by extension in [app/src/project.cpp](/Users/cmaughan/dev/vklive/app/src/project.cpp:127), then recursively scans the project folder.

Impact: it can copy too much, miss model-referenced external textures, and does not explain unresolved dependencies.

Recommendation: build a dependency graph from the scenegraph, shader includes, model files, material texture paths, and scripts. Use extension scanning only as an optional “include extras” mode.

### 12. Medium: editor project switching may invalidate buffer iteration

`ZepEditor::InitWithProject` iterates `GetBuffers()` and removes buffers in the same loop in [app/src/editor.cpp](/Users/cmaughan/dev/vklive/app/src/editor.cpp:421).

Impact: if `RemoveBuffer` mutates the underlying container, project switching can crash or skip buffers.

Recommendation: copy raw buffer pointers into a temporary vector, then remove them.

### 13. Medium: menu split actions dereference nullable editor tab state

In [app/src/menu.cpp](/Users/cmaughan/dev/vklive/app/src/menu.cpp:358), split actions fetch `pTabWindow` and immediately call `pTabWindow->GetActiveWindow()`.

Impact: a menu action can crash if no active editor tab exists or if the backend state changes.

Recommendation: disable split menu items unless an active editor tab/window exists, and keep defensive null checks in the action.

### 14. Medium: validation shader context is globally mutable and not thread-safe

`ValidationCurrentShaders` is a global vector in [src/validation.cpp](/Users/cmaughan/dev/vklive/src/validation.cpp:11). `validation_set_shaders` mutates it in [src/validation.cpp](/Users/cmaughan/dev/vklive/src/validation.cpp:31), while Vulkan debug callbacks can call into `validation_error` through [src/vulkan/vulkan_debug.cpp](/Users/cmaughan/dev/vklive/src/vulkan/vulkan_debug.cpp:95).

Impact: concurrent shader updates and Vulkan validation callbacks can race.

Recommendation: guard validation context with a mutex or make validation context immutable per scene/build generation.

### 15. Medium: model loader assumes normals and material pointers are present

`model_load` dereferences `paiMesh->mNormals[j]` and `pScene->mMaterials[paiMesh->mMaterialIndex]` in [src/model.cpp](/Users/cmaughan/dev/vklive/src/model.cpp:248).

Impact: malformed or partially supported assets can crash the app instead of surfacing an asset error.

Recommendation: validate `HasNormals()`, material index bounds, UV channels, texture counts, and imported scene invariants before dereferencing.

### 16. Medium: Vulkan frame capture does not create output directories or report PNG write failures

Vulkan render capture writes through `lodepng::encode` in [src/vulkan/vulkan_render.cpp](/Users/cmaughan/dev/vklive/src/vulkan/vulkan_render.cpp:216), but does not create the target directory or report encode errors. The Metal path creates directories and reports errors in [src/metal/metal_scene.mm](/Users/cmaughan/dev/vklive/src/metal/metal_scene.mm:514).

Impact: `--record-one-frame` can silently fail on Vulkan if `run_tree/renders` is missing.

Recommendation: match Metal behavior: create directories, check encode return codes, and report failures.

### 17. Medium: `post_2d` scripting is parsed and called but disabled

`post_2d` is parsed in [src/scene.cpp](/Users/cmaughan/dev/vklive/src/scene.cpp:1078), and `window_render` calls `python_run_2d` in [app/src/window_render.cpp](/Users/cmaughan/dev/vklive/app/src/window_render.cpp:95). But `python_run_2d` returns before executing the script in [src/python_scripting.cpp](/Users/cmaughan/dev/vklive/src/python_scripting.cpp:256).

Impact: the scenegraph exposes a feature that appears wired but does nothing.

Recommendation: either remove/hide it until implemented or return a visible warning when `post_2d` is present.

### 18. Medium: node graph is currently demo state, not scene-integrated tooling

`WindowNodeGraph::build_demo_graph` hardcodes demo nodes in [app/src/window_nodegraph.cpp](/Users/cmaughan/dev/vklive/app/src/window_nodegraph.cpp:127). The plan in [plans/2026-05-23-nodegraph-integration.md](/Users/cmaughan/dev/vklive/plans/2026-05-23-nodegraph-integration.md:9) also describes the current node graph as a demo window.

Impact: users may expect node edits to affect scenes, passes, materials, or shader parameters, but the window is not yet connected to the live data model.

Recommendation: introduce a domain graph model that maps scene passes, surfaces, samplers, materials, uniforms, and scripts into editable nodes.

### 19. Low: Python tests are not wired into `ctest`

Useful Python tests exist, including [tests/test_do.py](/Users/cmaughan/dev/vklive/tests/test_do.py:1), [tests/test_pbr_template.py](/Users/cmaughan/dev/vklive/tests/test_pbr_template.py:1), and [tests/test_surface_hdr_static.py](/Users/cmaughan/dev/vklive/tests/test_surface_hdr_static.py:1). They did not appear in the `ctest -N` list driven by `python3 do.py test debug -- -N`.

Impact: workflow, template, and static scene validation tests can be skipped by the normal project test command.

Recommendation: register these with CTest or add a `do.py test-python` step that `do.py test` invokes.

### 20. Low: minimized window state is saved but not restored

`WindowState::Minimized` is recorded in [app/src/main.cpp](/Users/cmaughan/dev/vklive/app/src/main.cpp:141), but `init_sdl_window` only restores maximized state in [app/src/main.cpp](/Users/cmaughan/dev/vklive/app/src/main.cpp:114).

Impact: minor state inconsistency.

Recommendation: either restore minimized state intentionally or stop saving it.

## Architecture Notes

### Application Shell

The SDL/ImGui shell is functional and reasonably contained, but it leans heavily on globals: `g_Controller`, `g_pDevice`, `g_pWindow`, `g_SwapChainRebuild`, and shared project pointers. This makes the app harder to reason about under live reload, device loss, project switching, and future multi-window workflows.

A stronger structure would have one application state owner, one render-thread owner, and an explicit queue between UI/editor events and render-resource mutations.

### Scenegraph And Validation

The scenegraph system has useful diagnostics and enough structure to support good editor markers. The main weakness is that parsing, validation, asset discovery, and partial runtime preparation are mixed together. Manual AST walking with broad tag checks also makes it easy for malformed input to slip into crashes instead of messages.

The scenegraph layer should ideally produce an immutable CPU scene description plus a list of typed diagnostics. GPU preparation should happen later.

### Rendering Resource Lifetime

Vulkan resource lifetime is the riskiest part of the app. Scene pointers are used as backend map keys, model cache entries mix CPU and GPU ownership, descriptor/image transitions are managed ad hoc, and backend initialization can happen off the render thread.

A live shader editor needs boring resource ownership. The most important improvement is a render-thread-owned scene generation model: build new GPU resources, keep the old scene alive until the new one is complete, then swap.

### Shader Hot Reload

Shader compilation, reflection, and descriptor generation are central strengths. The weak spots are failure handling and cache paths. Vulkan shader compiler launch failures should become editor-visible diagnostics, and temporary shader output paths should be collision-resistant.

Include dependency tracking would also improve the live-edit loop because editing an included file should invalidate all dependent shaders.

### Model And Material Loading

The Assimp integration is useful and sample projects exercise it, especially the PBR robot project. However, model loading needs more defensive validation and a clearer boundary between source assets, cached CPU data, and GPU buffers.

Live model editing is particularly sensitive: never mutate an object the current scene may still be rendering.

### Editor Integration

Zep integration, Neovim backend tests, and editor markers are good foundations. The next step is to make diagnostics and project state more visible: parse errors, shader compiler errors, asset errors, and current active scene generation should all be visible as first-class UI state.

### Node Graph

The node graph is visually present but architecturally disconnected. That is fine for an experiment, but it should be labeled internally and planned as either a real scene editing model or removed from the main workflow until it drives actual scene data.

### Build And Test Ergonomics

`do.py` is one of the better parts of the repository. It gives the project a single entrypoint and makes local development easier. CTest coverage is broader than expected, especially around render backend behavior and startup frames.

The biggest test gap is failure-mode testing: malformed scenegraph edits, shader compiler failures, concurrent reload behavior, nested project assets, and malformed models.

## Top 10 Good Things About The Application

1. Clear separation between the `Rezonality` executable, `RezonalityAppCore`, and the `vklive` library target.

2. Practical `do.py` workflow for setup, configure, build, test, and run.

3. Cross-backend rendering architecture with Vulkan and Metal implementations behind `IDevice`.

4. Live-edit fault-tolerance intent is already visible: invalid reloads try to preserve the previous scene, and scene errors are copied into editor markers.

5. Scene diagnostics carry file and range information, which is exactly what a shader/scene editor needs.

6. Shader reflection is used to derive descriptor bindings instead of relying entirely on duplicated manual declarations.

7. Sample projects cover meaningful workflows: default rendering, ray tracing, deferred rendering, PBR robot assets, Shadertoy-style examples, and audio.

8. Test coverage already includes command-line parsing, project copying, editor backend behavior, Neovim session behavior, node graph behavior, render backend lifetime, and startup frame smoke tests.

9. Vulkan object naming and debug callback plumbing are present, which helps graphics debugging.

10. The repository contains implementation plans and review notes, making ongoing work easier to understand and continue.

## Top 10 Bad Things About The Application

1. Background scene reload calls into the live render device without a clear thread-safety model.

2. The scene parser has a concrete out-of-bounds bug for scalar vectors.

3. Model hot reload mutates cached models and does not reliably rebuild GPU buffers.

4. Vulkan shader compiler launch failures are not promoted to scene diagnostics.

5. Scene parser AST lifetime is manual and leaks on exceptions.

6. Opening a folder can create `default.scenegraph` without explicit user consent.

7. Project copying is extension-based and incomplete for nested dependency-heavy projects.

8. Global mutable state makes device loss, project switching, validation, and rendering harder to reason about.

9. The node graph is visible but not connected to real scene/material/shader editing.

10. Python workflow/template tests are not included in the normal CTest path.

## Best 10 Quality-Of-Life Features To Improve The Tool

1. A diagnostics panel showing current scene status, last successful reload, failed reload reason, and grouped errors by scene, shader, model, and script.

2. A debounced filesystem watcher that reloads scenegraph, shaders, includes, scripts, and assets automatically with a visible reload generation.

3. Shader include dependency tracking so editing an include invalidates every dependent shader.

4. A project health panel listing missing files, broken model textures, invalid shader paths, unsupported scene fields, and quick actions.

5. A reflected uniform/parameter inspector with sliders, color pickers, toggles, and saved overrides.

6. A render target inspector with thumbnails, channel views, mip views, freeze/pause, and save-image actions.

7. A model/material inspector showing meshes, bounds, material slots, texture paths, UV availability, normals/tangents, and import warnings.

8. Scenegraph autocomplete and snippets based on a typed schema.

9. A real node graph view that maps passes, surfaces, samplers, uniforms, scripts, and materials to editable graph nodes.

10. An explicit “New Project From Template” flow with recent projects, destination selection, and no surprise file creation.

## Best 10 Tests That Could Improve Stability

1. Scene parser regression tests for scalar vectors: `scale: 1`, `size: 0.5`, malformed vectors, empty vectors, and wrong arity.

2. Scene parser ownership tests that repeatedly parse invalid scenegraphs and assert no leak or crash under sanitizers.

3. Invalid-pass tests ensuring passes without geometry/script do not leave dangling `passOrder` pointers or partially published state.

4. Reload concurrency tests using a fake `IDevice` or thread sanitizer to prove render, destroy, and reload operations are serialized.

5. Model hot-reload tests that change model contents and verify new CPU data and GPU buffers are built without mutating the live scene.

6. Vulkan shader compiler failure tests with a missing or failing compiler path, asserting visible scene diagnostics and no null pipeline bind.

7. Nested project copy tests using a PBR-style project with models, textures, shaders, scripts, and subdirectories.

8. Editor project-switch tests with multiple open buffers to verify buffer removal does not invalidate iteration.

9. Validation callback thread-safety tests that call `validation_set_shaders` while validation messages are reported.

10. Vulkan and Metal render-capture tests where the output directory is missing, asserting directory creation and explicit failure reporting.

## Worst 10 Features Or Design Choices Currently In The Application

1. GPU resource creation and destruction can happen from multiple threads without an explicit render-thread ownership model.

2. Global singleton-style state is used for core app, render, validation, and scene timing behavior.

3. The scene parser manually walks a raw AST and uses broad string matching, making validation fragile.

4. Backend scene resources are keyed by raw `Scene*`, which makes lifetime and generation ownership implicit.

5. `Scene::passOrder` stores raw pass pointers instead of stable names, indices, or owned objects.

6. The model cache mixes source asset caching, mutable CPU model data, and GPU resource ownership.

7. Shader compiler temporary output uses basename-only files in `/tmp/vklive`.

8. Project save/copy behavior is based on directory extension scanning rather than a real project dependency graph.

9. The node graph exists as a user-visible window before it has a real domain model behind it.

10. The `post_2d` scripting path is parsed and called but disabled, creating a misleading feature surface.

## Suggested Modularization Direction

The highest-leverage refactor would be to split the live-edit pipeline into four stages:

1. Parse scene files into an immutable CPU scene description.
2. Validate assets, shader paths, model paths, and script declarations into structured diagnostics.
3. Build GPU resources on the render thread into a new scene generation.
4. Atomically publish the new generation only after every required resource succeeds.

That structure would make shader hot reload, model reload, project switching, device loss, render target inspection, and node graph editing much easier to evolve in parallel.

## Verification Notes

- Repository files were inspected directly from disk using `rg --files` and targeted file reads.
- No repository files were modified.
- `git status --short` was clean during review.
- `python3 do.py test debug -- -N` listed the configured CTest suite; the full build/test suite was not run.