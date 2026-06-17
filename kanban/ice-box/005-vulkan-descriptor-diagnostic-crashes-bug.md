# Vulkan Descriptor Diagnostic Crash Implementation Plan

> **For agentic workers:** Use `superpowers:executing-plans` or `superpowers:subagent-driven-development` to implement this task step by step. Keep checkboxes updated as work is completed.

**Goal:** Ensure malformed shader reflection or sampler setup reports diagnostics instead of crashing.

**Agents:** Claude identified the `end()` iterator dereferences and dropped sampler format argument. Codex did not list these specific lines but agreed broadly that shader/resource diagnostics need hardening. Gemini had no substantive final finding.

## Files

- Modify: `src/vulkan/vulkan_pass.cpp`
- Add or modify descriptor-focused tests under `tests/`

## Implementation Plan

- [ ] Add a regression test or narrow helper test that exercises a binding index present in `bindingSet.bindings` but absent from `bindingSet.bindingMeta`.
- [ ] In `vulkan_pass_build_descriptors()`, replace the current diagnostic that dereferences `itrMeta->second` after `itrMeta == end()` with a safe fallback diagnostic using `scene.sceneGraphPath` or the pass shader path when available.
- [ ] In `vulkan_pass_set_descriptors()`, make the same missing-metadata branch safe.
- [ ] Fix `fmt::format("Surface not found: ", passSampler.sampler)` so the sampler name appears in the message, for example `fmt::format("Surface not found: {}", passSampler.sampler)`.
- [ ] Make descriptor diagnostic helpers include pass name, set number, binding index, and shader path when known.
- [ ] Verify that descriptor failure marks the scene invalid or returns early in a way that prevents pipeline draw from continuing with incomplete descriptors.
- [ ] Run `python3 do.py build debug`.
- [ ] Run `python3 do.py test debug -- -R "scene|render_backend|vulkan"`.

## Acceptance Criteria

- [ ] Missing binding metadata cannot dereference `end()`.
- [ ] Missing sampler diagnostics include the sampler name.
- [ ] Descriptor setup failures produce actionable scene errors and keep the previous good scene.
- [ ] Regression coverage exists for the missing-metadata branch or an extracted helper that represents it.

Consensus reviewer: <gpt-5-codex>
