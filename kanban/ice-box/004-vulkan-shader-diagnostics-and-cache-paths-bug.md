# Vulkan Shader Diagnostics And Cache Paths Implementation Plan

> **For agentic workers:** Use `superpowers:executing-plans` or `superpowers:subagent-driven-development` to implement this task step by step. Keep checkboxes updated as work is completed.

**Goal:** Make Vulkan shader compiler failures visible as scene diagnostics and prevent SPIR-V temp-output collisions.

**Agents:** Codex identified compiler-launch failures and basename-only temp paths. Claude independently called for better shader-error mapping and binding source lines. Gemini had no substantive final finding.

## Files

- Modify: `src/vulkan/vulkan_shader.cpp`
- Modify: `src/shader_compiler.cpp`
- Modify: `include/vklive/shader_compiler.h` if reflection metadata APIs change
- Add or modify tests under `tests/`

## Implementation Plan

- [ ] Add a test path for a missing or failing `glslangValidator`. The test should assert that `vulkan_shader_create()` or its nearest testable wrapper reports a `scene.errors` entry instead of returning `nullptr` silently.
- [ ] In `src/vulkan/vulkan_shader.cpp`, when `run_process(args, &output)` returns non-zero, call `scene_report_error()` with the shader path and a message that includes the compiler path and process output if present.
- [ ] Mark the scene invalid when any required shader stage cannot be compiled, reflected, or converted into a shader module.
- [ ] Replace `/tmp/vklive/<filename>.spirv` with a collision-resistant path. Include at least the canonical shader path, shader stage or extension, process ID, and a stable hash in the filename.
- [ ] Keep the temp directory under `fs::temp_directory_path() / "vklive"` unless a project cache directory already exists. Create directories with `fs::create_directories`.
- [ ] In `src/shader_compiler.cpp`, stop defaulting binding metadata line numbers to `0` when the source line can be found. Use the already parsed shader source to map reflected binding names to source lines.
- [ ] Add a regression test for two shaders with the same basename in different directories if a testable shader compile harness exists. If not, isolate and test the output-path helper.
- [ ] Run `python3 do.py build debug`.
- [ ] Run the shader/compiler-adjacent CTest subset available on the current platform.

## Acceptance Criteria

- [ ] A compiler launch failure becomes an editor-visible scene error.
- [ ] Failed shader creation invalidates the candidate scene before swap.
- [ ] Temporary SPIR-V paths cannot collide for same-basename shaders in different folders/projects or simultaneous app instances.
- [ ] Binding metadata uses useful line numbers where source mapping is available.

## Notes

This task improves the data that the diagnostics panel and filesystem watcher will later consume.

Consensus reviewer: <gpt-5-codex>
