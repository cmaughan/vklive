# Vulkan Frame Capture Reporting Implementation Plan

> **For agentic workers:** Use `superpowers:executing-plans` or `superpowers:subagent-driven-development` to implement this task step by step. Keep checkboxes updated as work is completed.

**Goal:** Make Vulkan `WriteToFile` capture create output directories and report PNG write failures instead of failing silently.

**Agents:** Codex identified this Vulkan/Metal parity bug. Claude listed in-app recording as a QoL feature, which depends on reliable capture errors. Gemini had no substantive final finding.

## Files

- Modify: `src/vulkan/vulkan_render.cpp`
- Compare behavior with: `src/metal/metal_scene.mm`
- Add or modify render capture tests under `tests/`

## Implementation Plan

- [ ] Extract a small helper in `src/vulkan/vulkan_render.cpp` for PNG output path creation and `lodepng::encode` error handling.
- [ ] Before enqueuing or writing `Frame_XXXXX.png`, call `fs::create_directories(path)` and handle exceptions.
- [ ] Capture the return value from `lodepng::encode()` and report non-zero errors with `lodepng_error_text()`.
- [ ] Decide where to report failures. Prefer `scene_report_error(scene, MessageSeverity::Error, ...)` if the scene is still alive in the worker lambda; otherwise log an error and add a thread-safe callback path.
- [ ] Avoid capturing references in the thread-pool lambda that may outlive the frame. Copy `fileName`, dimensions, and image data ownership into the lambda.
- [ ] Add a test or harness case for missing output directory and invalid output path if feasible without a Vulkan device. If no headless hook exists, factor and test the helper.
- [ ] Run `python3 do.py build debug`.
- [ ] Run relevant render backend or capture tests.

## Acceptance Criteria

- [ ] Vulkan capture creates the requested output directory.
- [ ] PNG write errors are checked and reported.
- [ ] Capture worker lambda does not depend on dangling scene/path references.
- [ ] Behavior matches Metal capture error reporting as closely as practical.

Consensus reviewer: <gpt-5-codex>
