# Post 2D Disabled Feature Surface Implementation Plan

> **For agentic workers:** Use `superpowers:executing-plans` or `superpowers:subagent-driven-development` to implement this task step by step. Keep checkboxes updated as work is completed.

**Goal:** Make `post_2d` honest: either execute it safely or report that it is unsupported instead of silently doing nothing.

**Agents:** Codex identified that `post_2d` is parsed and called while `python_run_2d()` returns before execution. Claude did not focus on this specific feature. Gemini had no substantive final finding.

## Files

- Modify: `src/python_scripting.cpp`
- Modify: `src/scene.cpp` if unsupported diagnostics should be emitted during parse/build
- Modify: `app/src/window_render.cpp` if runtime warnings belong there
- Add or modify tests under `tests/`

## Implementation Plan

- [ ] Decide the target behavior for this task: implement execution of `draw_2d`, or report an unsupported-feature warning/error when `post_2d` is present. For a small safe bug fix, prefer a visible warning unless Python drawing has known-good tests.
- [ ] If warning: in `scene_build()`, when `post_2d` is present and compiled, add a warning message such as `post_2d is parsed but 2D Python execution is currently disabled`.
- [ ] If execution: remove the early `return true` in `python_run_2d()`, execute `draw_2d` inside the existing `pyMutex`, and route Python exceptions through `python_parse_output()` and scene diagnostics.
- [ ] Ensure `g_pDrawList`, `g_viewPort`, `g_pScene`, and `g_pDevice` are reset or overwritten safely around execution.
- [ ] Add tests that parse a scene with `post_2d` and assert either the unsupported warning or successful execution path.
- [ ] Run `python3 do.py build debug`.
- [ ] Run `python3 do.py test debug -- -R "scene|script|render"`.

## Acceptance Criteria

- [ ] Users are no longer presented with a scenegraph feature that silently does nothing.
- [ ] If implemented, `draw_2d` exceptions become scene/editor diagnostics.
- [ ] If deferred, the warning is visible and clear.

Consensus reviewer: <gpt-5-codex>
