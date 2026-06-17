# Scene Vector Parser Crash Implementation Plan

> **For agentic workers:** Use `superpowers:executing-plans` or `superpowers:subagent-driven-development` to implement this task step by step. Keep checkboxes updated as work is completed.

**Goal:** Prevent scalar or short scenegraph vectors from reading past parsed values and define the intended scalar-broadcast behavior.

**Agents:** Claude and Codex both called this a critical/high live-edit crash. Gemini had no substantive final finding.

## Files

- Modify: `src/scene.cpp`
- Modify: `tests/scene_inspect.cpp`
- Modify: `CMakeLists.txt` if adding a new scene test mode or fixture file
- Optional create: `tests/content/scene_parser_vectors/default.scenegraph`

## Implementation Plan

- [ ] Add a failing test in `tests/scene_inspect.cpp` for a valid project scene that contains scalar values for fields whose parser allows `min == 1`, such as surface `size: 512`, surface `scale: 0.5`, model `scale: 1`, and geometry `scale: 1`.
- [ ] Add a failing test for malformed short vectors where the field requires exact arity, such as `camera position: 5` or `clear: (1, 0)`, and assert the scene is invalid with an error instead of crashing.
- [ ] In `src/scene.cpp`, replace the `getVector` loop bound at the current `std::max(ret.length(), std::min(1, int(vals.size())))` expression with logic that never indexes beyond `vals.size()` or `ret.length()`.
- [ ] Implement explicit scalar broadcast only when the caller passed `min == 1` and the destination vector has more than one component. For example, `scale: 1` should become all components set to `1`; `size: 512` should become all destination components set to `512`.
- [ ] For non-broadcast short vectors, preserve current defaults for components not supplied and report `Wrong size vector` when outside `[min, max]`.
- [ ] Wrap each `std::stof` assignment in the existing parse error path so a bad token becomes a scene error rather than an uncaught exception.
- [ ] Run `python3 do.py build debug`.
- [ ] Run `python3 do.py test debug -- -R vklive_scene_tests`.

## Acceptance Criteria

- [ ] `scale: 1`, `size: 512`, and equivalent one-value fields no longer crash.
- [ ] Exact-arity fields still report errors for wrong vector sizes.
- [ ] New regression coverage fails on the old code and passes after the fix.
- [ ] Existing scene tests still pass.

## Notes

This is the highest-priority small fix because it protects the normal live-edit path. It also creates a clean parser contract for later schema/autocomplete work.

Consensus reviewer: <gpt-5-codex>
