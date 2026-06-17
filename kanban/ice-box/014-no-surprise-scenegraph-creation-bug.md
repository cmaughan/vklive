# No Surprise Scenegraph Creation Implementation Plan

> **For agentic workers:** Use `superpowers:executing-plans` or `superpowers:subagent-driven-development` to implement this task step by step. Keep checkboxes updated as work is completed.

**Goal:** Stop `scene_build()` from creating `default.scenegraph` in arbitrary user-selected folders during open/load.

**Agents:** Codex identified this as a destructive UX bug. Claude did not focus on it directly. Gemini had no substantive final finding.

## Files

- Modify: `src/scene.cpp`
- Modify: `app/src/project.cpp` if project open/create modes need to be distinguished
- Modify: tests that assume auto-created scenegraphs, especially `tests/zest_file_tests.cpp` if relevant
- Add or modify scene/project tests under `tests/`

## Implementation Plan

- [ ] Add a failing test that calls `scene_build()` or the project-open path on an empty temporary directory and asserts no `default.scenegraph` file is created.
- [ ] In `scene_find_scenegraph()` or equivalent code in `src/scene.cpp`, remove the auto-write block that writes `# Scenegraph` when no scenegraph exists.
- [ ] Replace auto-creation with a scene error: `No .scenegraph file found in <folder>`.
- [ ] Audit project creation paths. If any current "new project" behavior depends on implicit creation, move that behavior behind an explicit create/template flow rather than open/load.
- [ ] Keep tests that need a scenegraph responsible for creating their fixture file explicitly.
- [ ] Run `python3 do.py build debug`.
- [ ] Run `python3 do.py test debug -- -R "scene|project|zest_file"`.

## Acceptance Criteria

- [ ] Opening an arbitrary directory never writes `default.scenegraph` as a side effect.
- [ ] Missing scenegraph produces a clear diagnostic.
- [ ] Any intended project creation path remains possible through explicit UI/workflow.

## Dependencies

This is paired with `026-new-project-template-flow-feature`, which should provide the positive creation experience.

Consensus reviewer: <gpt-5-codex>
