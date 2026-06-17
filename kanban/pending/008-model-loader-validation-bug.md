# Model Loader Validation Implementation Plan

> **For agentic workers:** Use `superpowers:executing-plans` or `superpowers:subagent-driven-development` to implement this task step by step. Keep checkboxes updated as work is completed.

**Goal:** Make malformed or partially supported model assets produce `Model::errors` instead of null dereferences.

**Agents:** Codex identified unchecked Assimp normals and materials. Claude identified model loading as part of the broader live-edit crash surface. Gemini had no substantive final finding.

## Files

- Modify: `src/model.cpp`
- Modify: `include/vklive/model.h` if structured model diagnostics are added
- Modify: `tests/model_inspect.cpp`
- Optional create: small malformed model fixtures under `tests/content/model_loader/`

## Implementation Plan

- [ ] Add tests using minimal OBJ/GLTF fixtures for missing normals, missing or out-of-range material indices, missing UVs, and missing tangents.
- [ ] In `model_load()`, validate `pScene`, `pScene->mMeshes`, `pScene->mMaterials`, and every mesh pointer before iterating.
- [ ] Before reading `paiMesh->mNormals[j]`, check `paiMesh->HasNormals()`. Use a zero or generated fallback normal only if that is acceptable for existing render behavior; otherwise record an error and skip the mesh.
- [ ] Before reading `pScene->mMaterials[paiMesh->mMaterialIndex]`, validate `paiMesh->mMaterialIndex < pScene->mNumMaterials` and the material pointer is non-null.
- [ ] Keep existing fallback behavior for optional UVs/tangents, but add tests that prove missing optional channels do not crash.
- [ ] Accumulate clear `model.errors` text that includes the source filename and mesh name/index.
- [ ] Ensure `model.loaded` remains false when required geometry invariants fail.
- [ ] Run `python3 do.py build debug`.
- [ ] Run `python3 do.py test debug -- -R vklive_model_tests`.

## Acceptance Criteria

- [ ] Missing required Assimp data cannot crash `model_load()`.
- [ ] Required-data failures produce actionable `model.errors`.
- [ ] Optional UV/tangent absence keeps current fallback behavior.
- [ ] Existing PBR robot model tests still pass.

Consensus reviewer: <gpt-5-codex>
