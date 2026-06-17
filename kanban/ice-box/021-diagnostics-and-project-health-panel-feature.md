# Diagnostics And Project Health Panel Implementation Plan

> **For agentic workers:** Use `superpowers:subagent-driven-development` if implementing both backend data and UI in one push. Keep checkboxes updated as work is completed.

**Goal:** Provide a first-class UI for current scene status, reload generations, diagnostics, and project asset health.

**Agents:** Codex proposed a diagnostics panel and project health panel. Claude praised inline validation diagnostics and called for better shader error mapping. Gemini had no substantive final finding.

## Files

- Add or modify app window files under `app/src/` and `app/include/app/`
- Modify: `app/src/menu.cpp`
- Modify: `app/src/main.cpp`
- Modify: `include/vklive/message.h` if diagnostic grouping needs structure
- Use diagnostics from `src/scene.cpp`, `src/vulkan/vulkan_shader.cpp`, `src/model.cpp`, and validation code
- Add tests under `tests/`

## Implementation Plan

- [ ] Define the data model first: current scene valid/invalid, last successful reload generation, last failed reload generation, grouped messages by scenegraph/shader/model/script/validation, and project health findings.
- [ ] Add a lightweight app-state container for diagnostics that the UI can read without reaching into backend internals.
- [ ] Add an ImGui window registered through the existing menu/layout system, likely named `Diagnostics` or `Project Health`.
- [ ] Render grouped errors and warnings with file path, line/range when available, severity, and generation.
- [ ] Add quick actions only where existing app APIs already support them, such as opening a file in the editor. Avoid inventing repair actions before the data is reliable.
- [ ] Include missing files, broken model texture paths, invalid shader paths, unsupported scene fields, and compiler failures.
- [ ] Preserve existing inline editor markers; this panel complements them.
- [ ] Add tests for diagnostic grouping logic if UI rendering is hard to automate.
- [ ] Run `python3 do.py build debug`.
- [ ] Run `python3 do.py test debug -- -R "scene|project|editor"`.

## Acceptance Criteria

- [ ] Users can see whether the current scene is last-good or failed candidate state.
- [ ] Diagnostics are grouped by useful source type and include file/line/range when available.
- [ ] Project health includes missing/broken assets.
- [ ] Existing editor inline diagnostics still work.

## Dependencies

This becomes much stronger after `004-vulkan-shader-diagnostics-and-cache-paths-bug` and `017-live-edit-pipeline-refactor`, but grouping scene/model/shader messages can begin earlier.

Consensus reviewer: <gpt-5-codex>
