# Runtime Inspectors And Profiler Implementation Plan

> **For agentic workers:** Use `superpowers:subagent-driven-development`; render targets, uniforms, model/materials, and GPU timings can be separate workers after the data contracts are defined.

**Goal:** Add runtime inspection tools for render targets, reflected parameters, model/material assets, and per-pass GPU cost.

**Agents:** Claude requested richer Targets window, texture/model hot reload, per-pass GPU timing, and recording support. Codex requested render target, uniform/parameter, and model/material inspectors. Gemini had no substantive final finding.

## Files

- Add or modify app window files under `app/src/` and `app/include/app/`
- Modify: `app/src/menu.cpp`
- Modify: `include/vklive/IDevice.h` if target/model views need backend APIs
- Modify: `src/vulkan/vulkan_scene.cpp`, `src/vulkan/vulkan_pass.cpp`, and Metal equivalents if exposing target/timing data
- Modify shader reflection data in `src/shader_compiler.cpp` if uniform metadata is incomplete
- Add tests under `tests/`

## Implementation Plan

- [ ] Define backend-neutral view structs for render targets, reflected uniforms/parameters, model/material summaries, and pass timings.
- [ ] Extend `IDevice::TargetViews(Scene&)` or add adjacent APIs so the UI does not reach into Vulkan/Metal internals.
- [ ] Render target inspector: list target name, format, current size, scale, default/depth flags, and thumbnail texture where available.
- [ ] Add target image actions only after safe capture code exists: freeze/pause, save image, channel view, and mip view can be staged.
- [ ] Uniform inspector: use reflection metadata to expose sliders, color pickers, toggles, and saved overrides for supported uniform types.
- [ ] Model/material inspector: list meshes, bounds, material slots, texture paths, UV/normals/tangents availability, and import warnings.
- [ ] GPU timing: add timestamp queries per pass in Vulkan and equivalent Metal timing if available. Display milliseconds per pass and frame.
- [ ] Add tests for backend-neutral data extraction and reflection-to-control mapping.
- [ ] Run `python3 do.py build debug`.
- [ ] Run `python3 do.py test debug -- -R "render_backend|model|scene"`.

## Acceptance Criteria

- [ ] Users can inspect target metadata and thumbnails for a live scene.
- [ ] Reflected parameters appear as editable controls where type information is known.
- [ ] Model/material data and import warnings are visible.
- [ ] Per-pass timings are collected without stalling normal rendering.
- [ ] Inspector APIs are backend-neutral enough for Vulkan and Metal.

## Dependencies

Target thumbnails and save actions depend on stable resource lifetime from `003` and capture fixes from `012`. Uniform inspector quality depends on `004`.

Consensus reviewer: <gpt-5-codex>
