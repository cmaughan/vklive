# Validation Context Thread Safety Implementation Plan

> **For agentic workers:** Use `superpowers:executing-plans` or `superpowers:subagent-driven-development` to implement this task step by step. Keep checkboxes updated as work is completed.

**Goal:** Prevent races between validation shader context updates and Vulkan validation callbacks.

**Agents:** Codex identified `ValidationCurrentShaders` as globally mutable and not thread-safe. Claude identified validation callbacks and render-thread lifetime as part of the broader unsafe concurrency story. Gemini had no substantive final finding.

## Files

- Modify: `src/validation.cpp`
- Modify: `include/vklive/validation.h` if API shape changes
- Modify: `src/vulkan/vulkan_debug.cpp` only if callback context wiring changes
- Add or modify validation tests under `tests/`

## Implementation Plan

- [ ] Add a focused test that calls `validation_set_shaders()` from one thread while another thread calls `validation_error()` repeatedly, then drains the queue and asserts no crash or corrupted paths.
- [ ] Protect `ValidationCurrentShaders` with a `std::mutex`, or replace it with an immutable `std::shared_ptr<const std::vector<fs::path>>` updated atomically if that is cleaner in C++20.
- [ ] In `validation_error()`, take a local snapshot of the shader list before enqueueing messages so queue writes do not hold a lock longer than necessary.
- [ ] Keep `enableValidationMessages` and `validationFoundError` atomic.
- [ ] Consider adding a validation generation ID later, but do not block the basic race fix on the full live-edit pipeline refactor.
- [ ] Run `python3 do.py build debug`.
- [ ] Run `python3 do.py test debug -- -R validation` if a validation-specific target exists; otherwise run `python3 do.py test debug -- -R "render_backend|vulkan|scene"`.

## Acceptance Criteria

- [ ] Concurrent shader-context updates and validation errors are data-race safe.
- [ ] Validation errors still fan out to the currently active shader paths.
- [ ] Existing validation/error marker behavior is preserved.

## Dependencies

This is related to `003-vulkan-scene-lifetime-thread-ownership-bug`, but a local mutex/snapshot fix can land independently.

Consensus reviewer: <gpt-5-codex>
