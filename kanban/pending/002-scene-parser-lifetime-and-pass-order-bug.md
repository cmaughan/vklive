# Scene Parser Lifetime And Pass Order Implementation Plan

> **For agentic workers:** Use `superpowers:executing-plans` or `superpowers:subagent-driven-development` to implement this task step by step. Keep checkboxes updated as work is completed.

**Goal:** Remove parser AST leaks on exceptions, avoid dangling `Scene::passOrder` pointers, and stop user-reachable parse cascades from aborting debug builds.

**Agents:** Codex identified the AST leak and dangling pass order. Claude identified coarse parser failure and the debug assert in `scene_report_error`. Gemini had no substantive final finding.

## Files

- Modify: `src/scene.cpp`
- Modify: `include/vklive/scene.h` if `passOrder` changes from raw pointers to indices or names
- Modify: `tests/scene_inspect.cpp`
- Optional create: `tests/content/scene_parser_recovery/default.scenegraph`

## Implementation Plan

- [ ] Add parser regression tests that repeatedly call `scene_build()` on malformed scenegraphs and assert no crash, `scene->valid == false`, and at least one diagnostic.
- [ ] In `scene_build()`, wrap successful `mpc_parse_contents()` output immediately in a local RAII owner, for example a `std::unique_ptr<mpc_ast_t, Deleter>` whose deleter calls `mpc_ast_delete`.
- [ ] Remove the manual `mpc_ast_delete((mpc_ast_t*)r.output)` call after parsing, because the RAII owner should clean up on every early return and exception path.
- [ ] Change pass-order publication so invalid passes are not pushed into `Scene::passOrder`. The minimal fix is to move `spScene->passOrder.push_back(spPass.get())` into the same branch that pushes `spScene->passes`.
- [ ] Prefer replacing `Scene::passOrder` with stable pass names or indices if any current consumer needs order later. If no consumer needs it today, document that and keep the minimal invalid-pointer fix.
- [ ] In `scene_report_error()`, replace the `assert(!"Check this is the right thing?...")` branch with a non-fatal cap. After 100 errors, return without asserting or add one final warning that additional messages were suppressed.
- [ ] Run `python3 do.py build debug`.
- [ ] Run `python3 do.py test debug -- -R vklive_scene_tests`.

## Acceptance Criteria

- [ ] Parser AST output is cleaned up on success, parse-processing exceptions, and validation exceptions.
- [ ] Invalid passes cannot leave dangling pointers in published scene state.
- [ ] More than 100 reported errors do not abort a debug build.
- [ ] Tests cover malformed scenegraph paths that previously depended on manual cleanup.

## Notes

This is a prerequisite for deeper parser recovery and later scenegraph schema work. Keep the patch focused; do not split `scene.cpp` in this task.

Consensus reviewer: <gpt-5-codex>
