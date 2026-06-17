# Preserve Camera State Implementation Plan

> **For agentic workers:** Use `superpowers:executing-plans` or `superpowers:subagent-driven-development` to implement this task step by step. Keep checkboxes updated as work is completed.

**Goal:** Keep live camera navigation stable across scene reloads when the same camera still exists.

**Agents:** Claude identified `scene_copy_state()` as a no-op and called camera carry-over a meaningful live-coding QoL issue. Codex agreed diagnostics/project state should be more visible but did not call this specific function out. Gemini had no substantive final finding.

## Files

- Modify: `src/scene.cpp`
- Modify: `include/vklive/camera.h` only if helper APIs are useful
- Add or modify tests under `tests/scene_inspect.cpp` or a new scene state test

## Implementation Plan

- [ ] Add a unit test that builds two scenes with the same camera name, mutates the source camera's runtime state, calls `scene_copy_state(dest, source)`, and asserts runtime camera values are preserved in the destination.
- [ ] In `scene_copy_state()`, for matching pass and camera names, copy runtime navigation fields from source camera to destination camera.
- [ ] Preserve user-navigation state: `position`, `focalPoint`, `viewDirection`, `right`, `up`, `orientation`, `orbitDelta`, `positionDelta`, `lastTime`, and camera type where appropriate.
- [ ] Be careful with scene-authored properties. If the new scenegraph changed `fieldOfView` or `nearFar`, prefer the new scenegraph value unless product intent says navigation should override it too.
- [ ] If copying the whole `Camera` object is too broad, add a small helper such as `camera_copy_runtime_state(Camera& dest, const Camera& source)`.
- [ ] Run `python3 do.py build debug`.
- [ ] Run `python3 do.py test debug -- -R "scene|camera"`.

## Acceptance Criteria

- [ ] Camera position/orientation survives a reload when the camera name is unchanged.
- [ ] New scene-authored camera settings are not unintentionally erased.
- [ ] Tests cover matching and non-matching camera names.

Consensus reviewer: <gpt-5-codex>
