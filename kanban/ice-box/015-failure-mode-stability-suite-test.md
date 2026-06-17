# Failure Mode Stability Suite Implementation Plan

> **For agentic workers:** Use `superpowers:subagent-driven-development` for this task. Split by test domain so workers do not overlap files unnecessarily.

**Goal:** Add focused tests for the failure modes the reviews agreed are most likely to break live coding.

**Agents:** Claude and Codex both called for malformed-scene, descriptor, lifetime/reload, target-resize, sampler matrix, and sanitizer-oriented tests. Gemini had no substantive final finding.

## Files

- Modify: `CMakeLists.txt`
- Modify: `tests/scene_inspect.cpp`
- Modify: `tests/vulkan_imgui_texture_lifetime_tests.cpp`
- Optional create: `tests/reload_lifetime_tests.cpp`
- Optional create: `tests/descriptor_diagnostic_tests.cpp`
- Optional create: `tests/content/...` fixtures
- Optional modify: `.github/workflows/builds.yml` if sanitizer jobs are added

## Implementation Plan

- [ ] Add malformed scenegraph cases for scalar vectors, wrong arity vectors, missing children, bad formats, missing target/sampler, and same-surface read/write with and without `!`.
- [ ] Add descriptor diagnostic tests that exercise missing binding metadata and missing image/buffer binding paths without requiring a full visible window.
- [ ] Add a render lifetime smoke test: build a valid scene, render or fake-render several frames, inject a shader compile/validation failure, and assert the old scene is preserved without crash.
- [ ] Extend `vklive_vulkan_imgui_texture_lifetime_tests` or add a related test for target resize while the surface is exposed through ImGui descriptors.
- [ ] Add a multi-model pass test aligned with the expected behavior from `006-multi-model-pass-buffer-binding-bug`.
- [ ] Add a Vulkan ray-tracing smoke test if Vulkan ray tracing is available on the test host; otherwise gate it with CMake feature detection.
- [ ] Add sanitizer-oriented documentation or CI entries for ThreadSanitizer on platforms where the dependencies support it. If CI cannot run TSan yet, add a local command in the test file comments and consensus docs.
- [ ] Register all new C++ tests with CTest in `CMakeLists.txt`.
- [ ] Run `python3 do.py build debug`.
- [ ] Run `python3 do.py test debug`.

## Acceptance Criteria

- [ ] The suite catches the old parser scalar-vector crash.
- [ ] Descriptor diagnostic failure paths are covered.
- [ ] Reload/lifetime behavior has at least one automated stress or smoke test.
- [ ] New tests are registered in CTest and run through `python3 do.py test debug`.
- [ ] Platform-gated tests skip cleanly when Vulkan/Metal capabilities are unavailable.

## Subagent Split

- [ ] Subagent A: scene parser and sampler matrix tests.
- [ ] Subagent B: descriptor and multi-model tests.
- [ ] Subagent C: render lifetime, resize, and sanitizer/CI integration.

Consensus reviewer: <gpt-5-codex>
