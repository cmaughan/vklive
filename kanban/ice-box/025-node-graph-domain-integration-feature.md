# Node Graph Domain Integration Implementation Plan

> **For agentic workers:** Use `superpowers:subagent-driven-development`. This should be split into domain model, UI projection, and scene write-back/testing.

**Goal:** Turn the current demo node graph into scene-integrated tooling, or explicitly hide/label it until it edits real data.

**Agents:** Claude and Codex both identified the node graph as user-visible demo state. The older `plans/2026-05-23-nodegraph-integration.md` says the dockable demo integration is implemented, so this task is specifically the next domain-integration step. Gemini had no substantive final finding.

## Files

- Modify: `app/src/window_nodegraph.cpp`
- Modify: `app/include/app/window_nodegraph.h`
- Modify node graph model code under `libs/nodegraph/` only if the app adapter cannot represent the domain cleanly
- Modify: `include/vklive/scene.h`
- Modify scene serialization/parsing code if write-back is implemented
- Modify tests: `tests/nodegraph_window_tests.cpp`, `tests/nodegraph_canvas_tests.cpp`, and new domain tests

## Implementation Plan

- [ ] Decide first milestone: read-only scene graph visualization or editable scene graph. Prefer read-only visualization first if write-back serialization is not ready.
- [ ] Define domain nodes for passes, surfaces, samplers, models/materials, shaders, uniforms, scripts, and output targets.
- [ ] Build a projection from `Scene` to node graph nodes and edges. For example, pass targets connect to surfaces, pass samplers connect from surfaces, pass shaders connect to shader nodes, and model references connect to model/material nodes.
- [ ] Replace or gate `build_demo_graph()` in normal UI. Keep `BuildDemoGraphForTests()` only if tests still need a simple canvas fixture.
- [ ] Add menu/window labeling that makes experimental/read-only status clear if edits are not implemented yet.
- [ ] If editable, define write-back rules and conflict behavior: node movement is UI-only, while changing a sampler/target/uniform updates scenegraph text or an intermediate scene model.
- [ ] Add tests that build a scene with multiple passes and assert the domain graph contains expected nodes and edges.
- [ ] Add UI smoke tests for opening the Node Graph window without a scene and with a loaded scene.
- [ ] Run `python3 do.py build debug`.
- [ ] Run `python3 do.py test debug -- -R nodegraph`.

## Acceptance Criteria

- [ ] The Node Graph window no longer presents hardcoded Oscillator/Filter/Output nodes as the main experience for VkLive scenes.
- [ ] Loaded scenes produce meaningful pass/surface/shader/model graph nodes.
- [ ] Unsupported edit behavior is clearly gated or labeled.
- [ ] Existing nodegraph canvas/theme/window tests still pass.

## Dependencies

Full editable node graph depends on `017-live-edit-pipeline-refactor` and likely `024-scenegraph-schema-autocomplete-feature`. Read-only visualization can start earlier.

Consensus reviewer: <gpt-5-codex>
