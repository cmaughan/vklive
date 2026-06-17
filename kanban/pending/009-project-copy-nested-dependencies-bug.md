# Project Copy Nested Dependencies Implementation Plan

> **For agentic workers:** Use `superpowers:executing-plans` or `superpowers:subagent-driven-development` to implement this task step by step. Keep checkboxes updated as work is completed.

**Goal:** Make project copy preserve nested project assets reliably and create destination directories before copying.

**Agents:** Codex identified missing destination parent directories and extension-based dependency gaps. Claude did not focus on project copy but reviewed project handling broadly. Gemini had no substantive final finding.

## Files

- Modify: `app/src/project.cpp`
- Modify: `tests/project_copy_tests.cpp`
- Optional add fixtures under `tests/content/project_copy_nested/`

## Implementation Plan

- [ ] Add a failing test in `tests/project_copy_tests.cpp` that creates or uses a project with nested `models/`, `textures/`, `shaders/`, and material files.
- [ ] Assert the copied project preserves the relative nested paths, not just flat files.
- [ ] In `project_copy()`, before `fs::copy_file(source, dest, ...)`, call `fs::create_directories(dest.parent_path())`.
- [ ] Keep the current guard that avoids copying a source onto itself.
- [ ] Extend dependency collection beyond broad extension scanning where data is already available: scenegraph file, `scene.shaders`, pass Metal kernels, `scene.models`, model material texture paths, and scripts.
- [ ] For unresolved referenced dependencies, append a readable message to `error` but keep copying resolvable files unless the missing dependency makes the copied project unusable.
- [ ] Keep extension scanning as an optional safety net for project-owned extra files; do not copy ignored/generated directories.
- [ ] Run `python3 do.py build debug`.
- [ ] Run `python3 do.py test debug -- -R vklive_project_copy_tests`.

## Acceptance Criteria

- [ ] Copying a project with nested assets succeeds.
- [ ] Destination parent directories are created automatically.
- [ ] Referenced shaders, model files, material libraries/textures, scripts, and scenegraph files are included.
- [ ] Missing dependencies are reported clearly.

## Dependencies

Coordinate final UX with `026-new-project-template-flow-feature`. The bug fix can land first.

Consensus reviewer: <gpt-5-codex>
