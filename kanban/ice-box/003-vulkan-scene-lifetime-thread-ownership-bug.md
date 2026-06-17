# Vulkan Scene Lifetime And Thread Ownership Implementation Plan

> **For agentic workers:** Use `superpowers:subagent-driven-development` for this task. It is large enough to split into render-thread handoff, deferred destruction, and tests/review.

**Goal:** Ensure all GPU resource creation/destruction and scene publication happen through one render-thread owner, while preserving async CPU scene parsing.

**Agents:** Claude and Codex both identified this as the largest stability risk. Claude emphasized self-destruction through `ctx.mapVulkanScene.erase()` and inconsistent `waitIdle`; Codex emphasized worker-thread `InitScene()` and device-loss races. Gemini had no substantive final finding.

## Files

- Modify: `app/src/main.cpp`
- Modify: `include/vklive/IDevice.h` if new queue/ownership APIs are needed
- Modify: `include/vklive/vulkan/vulkan_context.h`
- Modify: `include/vklive/vulkan/vulkan_scene.h`
- Modify: `src/vulkan/vulkan_device.cpp`
- Modify: `src/vulkan/vulkan_scene.cpp`
- Modify: `src/vulkan/vulkan_render.cpp`
- Modify: `src/vulkan/vulkan_pass.cpp`
- Add or modify focused tests under `tests/`

## Implementation Plan

- [ ] First make the app worker CPU-only: in `app/src/main.cpp`, remove `g_pDevice->InitScene(*spProject->spScene)` from the update thread. The worker should only call `scene_build()` and enqueue the parsed project.
- [ ] On the UI/render thread, call `g_pDevice->InitScene()` for candidate scenes before the existing pre-render step. Keep the keep-last-good-scene flow: initialize candidate, pre-render candidate, then swap only if still valid.
- [ ] Add a clear comment or helper around this handoff explaining that `IDevice` methods are render-thread-only unless explicitly documented otherwise.
- [ ] Change Vulkan scene lookup to keep a `std::shared_ptr<VulkanScene>` alive for the duration of render/output work. `vulkan_scene_get()` should return `std::shared_ptr<VulkanScene>` or there should be a separate `vulkan_scene_get_shared()` used by render paths.
- [ ] Remove direct calls to `vulkan_scene_destroy()` from inside `vulkan_pass_draw()` and `vulkan_scene_render()` call stacks. Replace them with a deferred invalidation path, such as marking the `Scene` invalid and queueing the scene for destruction after render unwinds.
- [ ] Add a deferred-destruction container to `VulkanContext`, drain it at a safe point on the render thread after frame submission or before the next scene initialization.
- [ ] Audit `ctx.mapVulkanScene` reads/writes. If all GPU work is render-thread-only after the first steps, document that and avoid unnecessary locks. If any cross-thread access remains, protect `mapVulkanScene` and related device state with a mutex.
- [ ] Quiesce reload/device work before recreating a lost device. The device-lost path should stop accepting new candidate GPU work, wait for the update thread handoff to drain, destroy scenes from the render thread, then recreate.
- [ ] Replace scattered wait-idle trial behavior with explicit rules: `WaitIdle()` only at scene swap/destruction boundaries and device reset, not inside target resize hot paths unless no narrower synchronization is available.
- [ ] Add a fake-device or instrumentation test proving `InitScene()` is not called from the update thread. If this is hard in the current app shell, add a small testable helper that performs candidate project promotion and records thread IDs.
- [ ] Run `python3 do.py build debug`.
- [ ] Run `python3 do.py test debug -- -R "render_backend|vulkan_imgui_texture_lifetime|app_command_line"`.

## Acceptance Criteria

- [ ] The background update thread no longer calls any live `IDevice` GPU initialization/destruction method.
- [ ] No Vulkan scene erases itself from `mapVulkanScene` while render/pass code is still using its members.
- [ ] Candidate scene initialization, pre-render, swap, and old-scene teardown all happen from the render thread.
- [ ] Device-loss handling cannot race with worker-thread `InitScene()`.
- [ ] The implementation keeps the previous good scene alive when candidate scene initialization or pre-render fails.

## Subagent Split

- [ ] Subagent A: app queue and promotion flow in `app/src/main.cpp`.
- [ ] Subagent B: Vulkan shared lifetime/deferred destruction changes.
- [ ] Subagent C: focused regression tests and stress-test harness.

Consensus reviewer: <gpt-5-codex>
