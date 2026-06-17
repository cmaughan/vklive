I have now reviewed the core engine, Vulkan rendering and lifetime code, scene parsing, the app shell and threading model, project handling, model loading, the node graph, build tooling, and test coverage. Here is the full report.

---

# VkLive — Desktop Graphics Tool & Live Shader Editor Review

## Scope & method

I read project-owned source directly from disk: `src/` (engine, scene, model, shader compiler, validation), `src/vulkan/` (context, scene, pass, surface, render, device), `app/src/` (main loop, controller, project, node graph window, editor), the core headers in `include/vklive/`, the test suite, `do.py`, `CMakeLists.txt`, and `AGENTS.md`. Vendored trees (`libs/`, `zep/`, `vcpkg/`) were treated as read-mostly. Findings below cite `file:line`.

## Architecture at a glance

- A **worker thread** (`app/src/main.cpp:325`) dequeues `Project` requests, calls `scene_build()` (pure CPU parse) and then `IDevice::InitScene()` (Vulkan resource creation), and hands the finished `Project` back to the UI thread via a second queue.
- The **UI thread** does a "pre-render" of a candidate scene to a tiny target to flush latent errors, and only swaps it in if it stays valid (`main.cpp:528-558`). The previous good scene is retained when an edit is broken — this is the heart of the fault-tolerance story and it is a genuinely nice design.
- Rendering is **modern dynamic-rendering Vulkan** (`vk::RenderingInfo`, no renderpass objects), with SPIR-V reflection (`src/shader_compiler.cpp`) driving descriptor-set layouts so shaders self-describe their bindings.
- An `IDevice` abstraction (`include/vklive/IDevice.h`) backs both Vulkan and a Metal port.

The skeleton is sound and the live-coding ergonomics are thoughtful. The weak points are concentrated in **resource lifetime, cross-thread Vulkan access, and a handful of concrete parsing/diagnostic bugs** that undercut the stated "never fall over" goal.

---

## Detailed findings

### 1. Scene parser out-of-bounds — crashes on valid-but-short vectors (high)

`src/scene.cpp:578`:

```cpp
for (int i = 0; i < std::max(ret.length(), std::min(1, int(vals.size()))); i++)
    ret[i] = std::stof(vals[i]->contents);
```

`ret.length()` is the **component count** of the glm vector (3 for `vec3`, 2 for `uvec2`), not the number of parsed floats. The grammar (`vector : '(' <float> … | <float>`) accepts a single scalar, so a user typing `position: 5`, `size: 512`, or `clear: 1` produces `vals.size() == 1`, the loop still runs to `ret.length()`, and `vals[1]`/`vals[2]` index past the vector → undefined behavior / crash. This is exactly the "edit a scene file and the app falls over" case the project is trying to avoid, it has no test, and the fix is to clamp the loop bound to `vals.size()`.

### 2. A scene object frees itself mid-call (high, latent UAF)

`vulkan_scene_destroy()` ends with `ctx.mapVulkanScene.erase(vulkanScene.pScene)` (`src/vulkan/vulkan_scene.cpp:162`). The map's `shared_ptr` is the **sole owner** (the render path holds only a raw pointer — `render()` calls `vulkan_scene_get()` which returns `itr->second.get()`, `src/vulkan/vulkan_render.cpp:70`). `vulkan_scene_destroy` is invoked from *inside* operations on that very object — `vulkan_pass_draw()` (`vulkan_pass.cpp:1348` and `:1365`) and `vulkan_scene_render()`'s catch block (`vulkan_scene.cpp:331`). So the object is destroyed while a method referencing it is on the stack, and `vulkan_pass_draw` continues to touch `vulkanPass` (a member of the freed scene) afterward. It currently survives only because the remaining statements happen not to dereference freed memory and the range-for is short-circuited by `return`. This is fragile UB-adjacent design; any future statement after the destroy point becomes a real use-after-free. Destruction should be deferred (mark-and-sweep after the render loop unwinds), and the renderer should hold a `shared_ptr` for the duration of the frame.

### 3. Cross-thread Vulkan + unsynchronized `std::map` (high)

`InitScene()` runs on the worker thread and does `ctx.mapVulkanScene[&scene] = spVulkanScene` plus shader/model creation against `ctx.device` (`vulkan_scene.cpp:79`, `vulkan_device.cpp:77`). The UI thread concurrently calls `vulkan_scene_get()` (`find`) and `vulkan_scene_destroy()` (`erase`) on the same map and submits to the same device. There is **no mutex** anywhere around `mapVulkanScene` or device access (grep confirms only an unrelated debug mutex). Concurrent `insert` + `find/erase` on `std::map` is a data race regardless of differing keys (tree rebalancing), and concurrent `vkCreate*`/queue use needs external synchronization. This will eventually corrupt the map or the driver under fast edit loops. It also makes the codebase hard for multiple agents to reason about because "what runs on which thread" is implicit.

### 4. Inconsistent GPU-idle / teardown strategy (medium–high)

- `vulkan_scene_destroy()` has its `ctx.device.waitIdle()` **commented out** (`vulkan_scene.cpp:119`) and relies on per-pass fences. But target surfaces are also referenced by **ImGui descriptor sets** (`vulkan_scene_target_set_imgui_descriptor`) that the main-window pass consumes; those aren't covered by per-pass fences. The error-path destroys (items 2) therefore tear down images that the in-flight UI submission may still reference.
- Conversely, `get_vulkan_surface()` calls a **full `ctx.device.waitIdle()` on every target resize** (`vulkan_pass.cpp:205`), stalling the whole GPU whenever the window or a `scale:`d target changes size — a visible hitch during live resizing.

The pattern of scattered, commented-in/out `waitIdle` calls (9 live, 3 commented) signals that resource lifetime is being managed by trial and error rather than a coherent ownership model.

### 5. Dereferencing `end()` in diagnostic paths (medium, bug)

In `vulkan_pass_build_descriptors` and `vulkan_pass_set_descriptors`:

```cpp
auto itrMeta = bindingSet.bindingMeta.find(index);
if (itrMeta == bindingSet.bindingMeta.end()) {
    scene_report_error(..., itrMeta->second.shaderPath, itrMeta->second.line, itrMeta->second.range);  // UB
    return false;
}
```

`vulkan_pass.cpp:695` and `:809` dereference the iterator they just confirmed is `end()`. This fires precisely when shader reflection metadata is missing — i.e., when a user's shader is malformed — turning a diagnostic into a crash.

### 6. Other concrete defects

- **Dropped format argument**: `fmt::format("Surface not found: ", passSampler.sampler)` (`vulkan_pass.cpp:535`) — the sampler name is silently discarded; the message is useless.
- **Debug abort on cascading errors**: `scene_report_error` does `assert(!"Check this is the right thing?…")` when `reportedErrorCount > 100` (`scene.cpp:1138`). In a debug build, a scene that generates many errors aborts the app instead of degrading gracefully.
- **Single-geometry assumption**: AS / vertex / index storage-buffer descriptor writes `break` after the first model in a pass (`vulkan_pass.cpp:831-882`), so multi-model passes silently bind only the first model's buffers.
- **`scene_copy_state` is a no-op**: camera carry-over across reloads is stubbed out (`scene.cpp:1237`), so the camera resets on every recompile — a real annoyance for live coding.
- **Binding source lines are always 0**: `meta.line = 0` in reflection (`shader_compiler.cpp:238`), so binding-mismatch diagnostics never point at the offending line.

### 7. Fault tolerance is coarse-grained

Scenegraph parsing throws `std::domain_error` from `getChild` to abort on the first missing child (`scene.cpp:536`), caught at the top of `scene_build`. One malformed statement therefore discards the *entire* parse rather than reporting that statement and recovering the rest. For a tool whose pitch is "edit freely and we keep working," per-statement recovery would be a big robustness win.

### 8. Modularity / separation-of-concerns

- **`src/vulkan/vulkan_pass.cpp` (1373 lines)** conflates target allocation, layout transitions, sampler management, descriptor build/set, pipeline (graphics + RT) creation, and submission. It is the single highest-friction file for parallel work.
- **`src/scene.cpp` (1304 lines)** mixes the grammar definition, AST walking, validation, asset resolution, and file-type predicates. The grammar and the AST→model mapping should be separable units.
- **Global mutable statics** (`Scene::GlobalFrameCount`, `Scene::GlobalElapsedSeconds`, `VulkanScene::GlobalGeneration`, the file-scope `parser`) make it impossible to run two scenes/instances and create hidden coupling between modules.
- **Duplicate `g_vertexLayout`** definitions exist in `src/model.cpp:18` (global) and `src/vulkan/vulkan_render.cpp:35` (anonymous namespace) — confusing and a maintenance trap.
- The **node graph window is a hardcoded demo** (`window_nodegraph.cpp` builds fixed Oscillator/Filter/Output nodes, `:127`) with no connection to passes/scene. It ships as a menu feature but does nothing real yet.

### 9. Tests

The suite is **broad on the editor/Neovim/nodegraph subsystems** (~30 executables) but **thin on the engine that actually crashes**: `scene_inspect.cpp` checks a few valid/invalid/ray cases but never exercises malformed vectors, sampler/target edge cases, or fault-tolerant recovery; there is no headless lifetime/stress test for the render path, and nothing runs under TSan to catch the threading race. The very bugs above (items 1, 5) would be caught by modest additions.

---

## Top 10 good things

1. **Keep-last-good-scene fault tolerance**: candidate scenes are pre-rendered and only swapped in if still valid (`main.cpp:528-558`), so a broken edit doesn't blank the viewport.
2. **Async double-buffered rebuild**: scene parse + resource creation happen off the UI thread via lock-free queues, keeping the editor responsive.
3. **Validation-layer errors surface inline in the editor** (`validation.cpp` → `zep_add_file_message`), a genuinely good live-coding feedback loop.
4. **Clean `IDevice` abstraction** enabling real Vulkan and Metal backends behind one interface.
5. **Modern dynamic-rendering Vulkan** (no renderpass/framebuffer objects) — less boilerplate, easier to extend.
6. **Reflection-driven descriptor layouts**: shaders declare their own bindings; the engine adapts (`shader_compiler.cpp`).
7. **Generation counters** drive lazy pipeline/framebuffer rebuilds only when targets/samplers actually change (`vulkan_pass.cpp:274-339`).
8. **Ping-pong targets** allow read-and-write of the same surface, with a helpful `!`-sampler diagnostic (`scene.cpp:419`, `validate_samplers`).
9. **Convenient asset resolution** with project→runtree fallback and subtype dirs (`scene_find_asset`, `scene.cpp:1172`).
10. **Good developer ergonomics**: `do.py` single entrypoint, root `compile_commands.json`, ccache support, CMake presets, and a `--startup-frame-test` smoke harness for CI.

## Top 10 bad things

1. **`getVector` OOB** crashes on single-component vectors typed live (`scene.cpp:578`).
2. **Scene destroys itself by erasing from its owning map mid-call** (`vulkan_scene.cpp:162`) — latent use-after-free.
3. **Cross-thread Vulkan creation + unsynchronized `std::map`** between worker and UI threads (no mutex anywhere).
4. **Inconsistent `waitIdle`/teardown**: disabled idle-wait in scene destroy vs full-stall idle-wait on every resize.
5. **`end()` iterator dereference** in two descriptor-diagnostic paths (`vulkan_pass.cpp:695, 809`).
6. **Coarse parse failure**: one bad statement aborts the whole scenegraph parse (`scene.cpp:536`).
7. **Camera state lost on every recompile** because `scene_copy_state` is stubbed (`scene.cpp:1237`).
8. **Monolithic files** (`vulkan_pass.cpp` 1373 lines, `scene.cpp` 1304 lines) impede parallel work and testing.
9. **Global mutable statics** for frame/time/generation prevent multiple scenes and create hidden coupling.
10. **Node graph is a non-functional demo** presented as a feature.

## Best 10 quality-of-life features to add

1. **Filesystem watcher** for automatic hot-reload (today reload is manual `<C-Return>` / save-driven).
2. **Persist camera (and pause/time) across recompiles** by finishing `scene_copy_state`.
3. **Richer Targets window**: per-surface name, format, size, and live thumbnails to debug multi-pass graphs.
4. **Texture/model hot-reload without a full scene rebuild** (reuse unchanged GPU resources by content hash).
5. **Per-pass GPU timing** in the profiler to find the expensive pass.
6. **In-app recording UI** (frame range + output path) — currently command-line only (`--record-one-frame`).
7. **Scenegraph autocomplete / inline schema hints** in the editor (keys, formats, sampler `!` syntax).
8. **Better shader-error mapping**: include-file attribution and column ranges (the regexes in `shader_compiler.cpp` already try; finish the job).
9. **A "reset device" UI affordance** instead of requiring an `.exe` restart after a lost device (`main.cpp:416-444` admits this is unrecovered).
10. **Wire the node graph to passes** (or hide it until it does something).

## Best 10 tests to improve stability

1. **Malformed-scenegraph fault-tolerance suite**: single-scalar vectors, missing children, bad formats — asserts no crash and scene marked invalid (catches finding #1).
2. **`getVector` regression test** for 1/2/3/4-component inputs into `vec2`/`vec3`/`vec4`.
3. **Headless render-lifetime test**: build → render N frames → inject a shader compile error → assert graceful recovery and no leak/crash.
4. **ThreadSanitizer stress test** running `InitScene` on a worker while the UI renders, hammering reload (catches #3).
5. **Target-resize lifetime test** extending `vulkan_imgui_texture_lifetime_tests` to surfaces resized while sampled by ImGui.
6. **Descriptor meta-not-found path** test exercising the `end()`-deref branches (#5).
7. **Multi-model pass binding test** verifying every model's vertex/index/AS buffers are bound (#6 single-geometry bug).
8. **Sampler/target validation matrix**: same-surface read+write with/without `!`, missing target, missing sampler.
9. **Ray-tracing pass build smoke test** on Vulkan (currently only a Metal-path scene test exists).
10. **Project copy/round-trip with absolute and missing asset paths**, extending `project_copy_tests`.

## Worst 10 features / design choices currently in the app

1. **Self-deleting scene via global-map `erase` from inside its own render/draw** (`vulkan_scene.cpp:162`).
2. **Unsynchronized cross-thread Vulkan resource creation and `std::map` mutation** (worker vs UI thread).
3. **`getVector` loop bounded by component count, not parsed-value count** (`scene.cpp:578`).
4. **Full `device.waitIdle()` on every target/window resize** (`vulkan_pass.cpp:205`) — global GPU stall during live work.
5. **Commented-out idle-wait in scene teardown** relying on incomplete per-pass fences for ImGui-referenced surfaces.
6. **Dereferencing `end()` iterators in error reporting** (`vulkan_pass.cpp:695, 809`).
7. **`assert(!"…")` used as control flow / TODO markers** (`scene.cpp:1138`, callable-shader `vulkan_pass.cpp:989`) — debug aborts on user-reachable paths.
8. **Global mutable frame/time/generation statics** that preclude multiple scenes and obscure data flow.
9. **`break`-after-first-model descriptor binding**, silently wrong for multi-geometry passes.
10. **Shipping a hardcoded demo node graph** as a top-level window with no scene integration.

---

### Bottom line

VkLive's *philosophy* (validate-before-swap, keep last good scene, async rebuild, reflection-driven bindings) is the right one for a live shader editor, and the app/engine/device separation is reasonable. The credibility gap is in **lifetime and concurrency**: a scene that deletes itself through its owning map, Vulkan objects built on one thread and consumed on another without locks, and inconsistent GPU-idle handling. Combined with a small set of concrete defects (the OOB vector parse, the `end()` derefs, coarse parse-abort), these are the things most likely to make the tool "fall over" on exactly the kind of rapid, mistake-prone editing it is designed to encourage. I'd prioritize, in order: (1) the `getVector` OOB, (2) deferring scene destruction out of the render call, (3) putting a lock (or a single-threaded resource-creation handoff) around `mapVulkanScene`/device, and (4) the two `end()`-deref diagnostics — then add the headless lifetime and TSan tests to keep them fixed.