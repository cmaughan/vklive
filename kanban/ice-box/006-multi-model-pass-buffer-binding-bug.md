# Multi-Model Pass Buffer Binding Implementation Plan

> **For agentic workers:** Use `superpowers:executing-plans` or `superpowers:subagent-driven-development` to implement this task step by step. Keep checkboxes updated as work is completed.

**Goal:** Make storage-buffer and acceleration-structure binding behavior correct and explicit for passes with multiple models.

**Agents:** Claude identified `break`-after-first-model behavior for AS, vertex, and index descriptor writes. Codex identified model/GPU ownership problems that make this area riskier. Gemini had no substantive final finding.

## Files

- Modify: `src/vulkan/vulkan_pass.cpp`
- Modify: `include/vklive/vulkan/vulkan_pass.h` if descriptor data structures need array storage
- Modify: sample or test shaders under `tests/content/` if adding a render smoke case
- Add or modify tests under `tests/`

## Implementation Plan

- [ ] Audit current shader conventions for `vertices`, `indices`, and acceleration-structure bindings. Determine whether shaders expect one model, descriptor arrays, or one pass draw per model.
- [ ] Add a failing test scene with a pass containing two model geometries and a shader layout that makes the intended behavior testable.
- [ ] If multiple models are unsupported for a single storage-buffer binding, add explicit validation in scene/Vulkan pass preparation that reports a scene error when a pass has multiple models and a shader asks for singular `vertices`/`indices`/AS bindings.
- [ ] If multiple models should be supported, change descriptor write storage so each model has a stable `vk::DescriptorBufferInfo` or AS descriptor entry and write a descriptor array with `descriptorCount` matching the shader binding.
- [ ] Remove the silent first-model-only behavior. Do not leave code that succeeds while binding only the first model unless that is the documented single-model validation path.
- [ ] Add diagnostics that name the pass, binding, and affected model paths when model buffer binding cannot be satisfied.
- [ ] Run `python3 do.py build debug`.
- [ ] Run `python3 do.py test debug -- -R "render_backend|scene|vulkan"`.

## Acceptance Criteria

- [ ] Multi-model passes either bind all required model resources correctly or fail with a clear scene diagnostic.
- [ ] The old silent first-model-only behavior is gone.
- [ ] Tests cover the chosen supported/unsupported behavior.

## Notes

This task may intersect with model hot-reload ownership. Coordinate with `007-model-hot-reload-immutable-gpu-assets-bug` before changing shared Vulkan model descriptors.

Consensus reviewer: <gpt-5-codex>
