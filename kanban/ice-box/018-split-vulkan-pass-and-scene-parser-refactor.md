# Split Vulkan Pass And Scene Parser Implementation Plan

> **For agentic workers:** Use `superpowers:subagent-driven-development`; split parser and Vulkan pass work into separate non-overlapping file ownership.

**Goal:** Reduce the highest-friction monolithic files and remove confusing global state after the safety bugs are addressed.

**Agents:** Claude called out `src/vulkan/vulkan_pass.cpp`, `src/scene.cpp`, duplicate `g_vertexLayout`, and global statics. Codex agreed the parser/validation/backend boundaries should be clearer and global state makes device loss/project switching hard. Gemini had no substantive final finding.

## Files

- Modify/split: `src/vulkan/vulkan_pass.cpp`
- Modify/split: `src/scene.cpp`
- Modify: `include/vklive/scene.h`
- Modify: `src/model.cpp`
- Modify: `src/vulkan/vulkan_render.cpp`
- Add new files under `src/scene/` or `src/vulkan/` only if CMake/source layout supports it cleanly
- Modify: `CMakeLists.txt`

## Implementation Plan

- [ ] Do not start by moving code. First add comments or small internal helper boundaries inside `vulkan_pass.cpp`: target/surface allocation, descriptor build/set, pipeline creation, submission, and validation/error handling.
- [ ] Extract the lowest-risk Vulkan pass helpers into new files, one category at a time, updating `CMakeLists.txt` after each extraction and running a focused build.
- [ ] Keep public interfaces stable while extracting; avoid changing behavior during mechanical moves.
- [ ] For `src/scene.cpp`, split parser grammar/setup from AST-to-scene mapping only after `001` and `002` land.
- [ ] Move duplicate vertex layout into a single shared definition or function, likely near `model.cpp`/`model.h`, and update `vulkan_render.cpp` to use it.
- [ ] Replace or encapsulate global mutable statics where practical: `Scene::GlobalFrameCount`, `Scene::GlobalElapsedSeconds`, `VulkanScene::GlobalGeneration`, and the file-scope parser should become explicit context or generation state over time.
- [ ] Add compile-only or unit tests after each extraction to ensure no target/source registration drift.
- [ ] Run `python3 do.py build debug` after each extraction stage.
- [ ] Run `python3 do.py test debug` after completing the full refactor.

## Acceptance Criteria

- [ ] `vulkan_pass.cpp` no longer owns unrelated target, descriptor, pipeline, and submission responsibilities in one large file.
- [ ] `scene.cpp` has a clearer parser/AST/validation split.
- [ ] Duplicate vertex-layout definitions are removed.
- [ ] Global mutable state has an explicit migration path, with at least one concrete reduction in this task.
- [ ] Behavior remains unchanged except for bugs already covered by earlier tickets.

## Dependencies

Do this after `001`, `002`, `003`, and `005` so the refactor does not hide urgent stability fixes inside file moves.

Consensus reviewer: <gpt-5-codex>
