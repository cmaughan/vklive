# Filesystem Watcher And Include Dependencies Implementation Plan

> **For agentic workers:** Use `superpowers:subagent-driven-development`. Split watcher infrastructure and shader include dependency tracking into separate workers.

**Goal:** Automatically reload changed scenegraph, shader, include, script, texture, and model files with debouncing and correct dependency invalidation.

**Agents:** Claude requested filesystem watcher hot reload and better shader include mapping. Codex requested debounced watcher, include dependency tracking, and asset hot reload. Gemini had no substantive final finding.

## Files

- Modify: `app/src/main.cpp`
- Modify: app controller/project files under `app/src/` and `app/include/app/`
- Modify: `src/shader_compiler.cpp`
- Modify: `include/vklive/shader_compiler.h`
- Modify: scene/project asset discovery in `src/scene.cpp` and `app/src/project.cpp`
- Add tests under `tests/`

## Implementation Plan

- [ ] Add a platform-neutral file watcher abstraction or use an existing project dependency if already available. Keep the interface small: watch roots, receive changed paths, debounce, enqueue project reload.
- [ ] Start by watching the current project root and filtering to edit/runtime file types: `.scenegraph`, shader extensions, headers/includes, Python scripts, model/texture extensions, and material files.
- [ ] Debounce changes so a save operation that writes multiple files triggers one reload generation.
- [ ] Extend shader compilation to record include dependencies per shader. If parsing compiler output is already available, use that; otherwise scan `#include` lines conservatively.
- [ ] When an included shader file changes, invalidate every shader that depends on it.
- [ ] For model and texture changes, enqueue a reload through the same safe pipeline; avoid direct GPU mutation from watcher callbacks.
- [ ] Show or log the reload generation and changed file list so the diagnostics panel can consume it later.
- [ ] Add tests for debounce behavior using a fake watcher/event source.
- [ ] Add tests for shader include dependency invalidation with two shaders sharing one include.
- [ ] Run `python3 do.py build debug`.
- [ ] Run `python3 do.py test debug -- -R "shader|project|app"`.

## Acceptance Criteria

- [ ] Saving a watched scene/shader/script/model/texture file triggers one debounced reload.
- [ ] Editing a shader include invalidates dependent shaders.
- [ ] Watcher callbacks do not call backend GPU APIs directly.
- [ ] The feature works with keep-last-good-scene behavior.

## Dependencies

This should wait for `003-vulkan-scene-lifetime-thread-ownership-bug` or use its render-thread handoff model from the start.

Consensus reviewer: <gpt-5-codex>
