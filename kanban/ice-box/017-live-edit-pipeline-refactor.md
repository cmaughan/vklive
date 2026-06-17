# Live Edit Pipeline Refactor Implementation Plan

> **For agentic workers:** Use `superpowers:subagent-driven-development`. This refactor should be executed after the highest-priority bug fixes, with review checkpoints between stages.

**Goal:** Split live editing into explicit parse, validate, render-resource build, and publish stages.

**Agents:** Claude and Codex both converged on this architecture. Claude focused on lifetime/concurrency and monolithic files; Codex described the desired four-stage pipeline directly. Gemini had no substantive final finding.

## Files

- Modify: `include/vklive/scene.h`
- Modify: `src/scene.cpp`
- Modify: `include/vklive/IDevice.h`
- Modify: `app/src/main.cpp`
- Modify: Vulkan and Metal scene/device files as needed
- Add focused tests under `tests/`

## Implementation Plan

- [ ] Document the intended pipeline in a short comment or design note near the handoff code: CPU parse, CPU validation, render-thread GPU build, atomic publish.
- [ ] Introduce an immutable or mostly immutable CPU scene description boundary. Keep this minimal at first: the output of `scene_build()` should be treated as CPU-only and should not allocate GPU resources.
- [ ] Move asset and shader path validation into a structured validation phase that produces `Message` diagnostics without touching backend state.
- [ ] Define a render-thread scene-generation object for backend resources. For Vulkan this maps to `VulkanScene`; for Metal use the analogous backend scene.
- [ ] Publish the new generation only after all required backend resources are created and the existing pre-render check succeeds.
- [ ] Make old generation lifetime explicit. The old generation remains the current scene until the new generation is fully valid, then it is retired through deferred destruction.
- [ ] Add generation IDs to diagnostics/logging so UI panels can show current, last-good, and failed reload generations.
- [ ] Keep a compatibility layer so existing scene parser tests and render backend tests continue to compile during the transition.
- [ ] Run `python3 do.py build debug`.
- [ ] Run `python3 do.py test debug`.

## Acceptance Criteria

- [ ] CPU parsing can run off-thread without calling backend device APIs.
- [ ] Backend resources are owned by a render-thread generation.
- [ ] Scene publication is atomic from the user's perspective.
- [ ] Diagnostics distinguish parse/validation/build/render failures.
- [ ] Existing keep-last-good-scene behavior is preserved.

## Dependencies

This refactor should follow or absorb the core of `003-vulkan-scene-lifetime-thread-ownership-bug`. It also unblocks safer filesystem watching, model hot reload, diagnostics panels, and reset-device behavior.

Consensus reviewer: <gpt-5-codex>
