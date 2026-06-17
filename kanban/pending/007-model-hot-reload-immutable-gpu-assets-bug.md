# Model Hot Reload Immutable GPU Assets Implementation Plan

> **For agentic workers:** Use `superpowers:subagent-driven-development` if this expands beyond a minimal cache fix. Keep checkboxes updated as work is completed.

**Goal:** Stop mutating cached models that may still be referenced by the live scene, and ensure changed model files rebuild GPU buffers/materials.

**Agents:** Codex identified cached model mutation and stale GPU buffers. Claude called out texture/model hot reload as a high-value improvement and flagged related resource lifetime risk. Gemini had no substantive final finding.

## Files

- Modify: `src/vulkan/vulkan_model.cpp`
- Modify: `include/vklive/vulkan/vulkan_model.h`
- Modify: `src/model.cpp` only if CPU model reload metadata needs changes
- Modify: `tests/model_inspect.cpp`
- Add or modify Vulkan model/cache tests under `tests/`

## Implementation Plan

- [ ] Add a test that loads a model, changes load metadata or source timestamp/content, reloads, and verifies CPU data and material descriptors are rebuilt rather than silently reused.
- [ ] In `vulkan_model_load()`, stop calling `model_load(*itr->second, createInfo)` on an existing cached `VulkanModel` that may be used by a current scene.
- [ ] Track cache entries by `ModelCreateInfo` plus file timestamp/content version. If the source file changed, create a new `VulkanModel` cache entry or replace the cache only after no live scene references the old one.
- [ ] In `vulkan_model_stage()`, handle the case where CPU vertex/index data changed but GPU buffers already exist. Destroy and recreate buffers only when it is safe, or prefer the new immutable model object path.
- [ ] Ensure material resources and descriptors are rebuilt when material texture paths or model material data change.
- [ ] Keep old `VulkanModel` resources alive until their owning `VulkanScene` is destroyed through the safe destruction path from `003-vulkan-scene-lifetime-thread-ownership-bug`.
- [ ] Add logging/diagnostics for reload failures so stale render output is not mistaken for success.
- [ ] Run `python3 do.py build debug`.
- [ ] Run `python3 do.py test debug -- -R "model|render_backend"`.

## Acceptance Criteria

- [ ] A model reload never mutates CPU data behind a scene currently being rendered.
- [ ] Changed model data causes new GPU vertex/index buffers and material resources to be created.
- [ ] Old resources remain valid until old scene destruction is safe.
- [ ] Tests cover cache reuse and changed-source reload behavior.

## Dependencies

This should be coordinated with `003-vulkan-scene-lifetime-thread-ownership-bug`. The immutable-object side can be started first, but final GPU destruction safety depends on render-thread ownership.

Consensus reviewer: <gpt-5-codex>
