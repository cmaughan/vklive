# Review Consensus

Prepared from `kanban/review-claude.md`, `kanban/review-codex.md`, `kanban/review-claude-run.json`, and the available Gemini output files on 2026-06-17.

## Agent Inputs

- Claude produced a full source review focused on scene parsing crashes, Vulkan scene lifetime, cross-thread rendering access, descriptor diagnostics, coarse parser recovery, monolithic modules, and live-coding quality-of-life gaps.
- Codex produced a full source review focused on the same live-reload/resource lifetime fault line, plus model hot reload, shader diagnostics, project copy/open behavior, UI crash guards, validation thread safety, capture reporting, and missing workflow tests.
- Gemini did not leave a substantive final review in the accessible markdown/log files. `review-gemini.md` and `review-gemini-stdio.log` contain activity-trace lines only, and `review-gemini-agy.log` ends with a timed-out Antigravity/Gemini session. Consensus below therefore treats Gemini as non-voting rather than inventing findings.

## Room Consensus

All substantive agents agree that VkLive has a good live-coding foundation: keep-last-good-scene behavior, async scene rebuild intent, shader reflection, editor diagnostics, cross-backend `IDevice`, useful sample projects, and a practical `do.py` workflow. The disagreement is not about direction; it is about where the current implementation violates the live-coding promise.

The shared top priority is stability during rapid edit/reload. Claude and Codex both identified the scalar-vector parser crash as a concrete user-triggerable bug, and both identified GPU resource lifetime/thread ownership as the largest architectural risk. The consensus plan keeps CPU parsing on a worker, but moves all GPU resource creation/destruction and scene publication behind a render-thread-owned model.

There is one useful tension: Claude praised the current async double-buffered rebuild, while both Claude and Codex flagged the current worker-thread `InitScene()` call as unsafe. The consensus is to preserve the product behavior, not the implementation detail: keep the editor responsive by parsing off-thread, then enqueue render-thread GPU work.

Node graph also has a nuance. The older plan in `plans/2026-05-23-nodegraph-integration.md` says the dockable demo integration is implemented. Claude and Codex agree the current user-visible node graph still does not edit passes, surfaces, uniforms, materials, or scripts. The consensus is not to redo the prior integration; it is to either connect it to a real scene domain model or make its experimental status explicit.

## Agreements And Disagreements By Topic

| Topic | Agents | Agreement level | Consensus |
| --- | --- | --- | --- |
| Scalar vector parser crash in `getVector` | Claude, Codex | Strong agreement | Fix immediately, define scalar broadcast semantics, and add scene parser regression tests. |
| Render-thread ownership and Vulkan scene lifetime | Claude, Codex | Strong agreement | Worker should not call `IDevice::InitScene()`. Defer destruction out of render callbacks, hold scenes strongly during frames, and quiesce reload during device reset. |
| Vulkan descriptor diagnostics | Claude | One-agent concrete bug, accepted | Fix `end()` iterator dereferences, missing sampler formatting, and source-line metadata so diagnostics do not crash. |
| Multi-model pass buffer binding | Claude | One-agent concrete bug, accepted | Make descriptor behavior explicit for multi-model passes; either bind arrays correctly or reject unsupported layouts with diagnostics. |
| Model hot reload and cache mutation | Codex, Claude adjacent QoL | Strong enough | Stop mutating cached live models; rebuild CPU/GPU assets into new scene generations. |
| Shader compiler failure and SPIR-V temp collisions | Codex, Claude adjacent diagnostics | Accepted | Promote compiler launch failures to scene errors and use collision-resistant output paths. |
| Parser AST leaks, passOrder raw pointers, cascade assert | Codex, Claude adjacent parser robustness | Accepted | Replace manual AST lifetime with RAII and keep invalid passes out of published state. Remove debug aborts from user-reachable error cascades. |
| Project opening/copying behavior | Codex | Accepted | Opening a folder should not silently create `default.scenegraph`; copying should handle nested dependencies and create destination directories. |
| App/editor crash guards | Codex | Accepted | Harden buffer removal, split-menu actions, and minimized-state persistence. |
| Validation shader context thread safety | Codex, connected to Claude threading | Accepted | Make validation context generation-scoped or mutex-protected. |
| Vulkan frame capture reporting | Codex | Accepted | Match Metal behavior: create output directories and report PNG write failures. |
| Disabled `post_2d` surface | Codex | Accepted | Either implement the path or surface a visible unsupported-feature warning. |
| Missing stability tests | Claude, Codex | Strong agreement | Add malformed-scene, descriptor, lifetime/reload, target-resize, sampler matrix, and sanitizer-oriented coverage. |
| Python tests outside CTest | Codex | Accepted | Wire workflow/template/static Python tests into the normal project test path. |
| Modularization | Claude, Codex | Strong agreement | Split the live-edit pipeline first; split `vulkan_pass.cpp` and parser responsibilities after the worst stability bugs are under control. |
| Camera state across reloads | Claude | Accepted QoL | Finish `scene_copy_state()` so live camera navigation survives recompiles. |
| Filesystem watcher/include dependencies | Claude, Codex | Strong agreement as feature | Add debounced reloads and shader include invalidation once render-thread scene ownership is safe. |
| Diagnostics/project health UI | Codex, Claude adjacent | Accepted feature | Build a first-class diagnostics/project-health panel on top of structured scene errors. |
| Runtime inspectors/profiler | Claude, Codex | Strong agreement as feature cluster | Add target thumbnails, uniform controls, model/material inspection, and per-pass GPU timing. |
| Recording/reset-device UI | Claude | Accepted feature | Add in-app recording controls and a reset-device path after ownership is repaired. |
| Scenegraph schema/autocomplete | Claude, Codex | Strong agreement as feature | Extract typed schema from parser/validation work and use it for snippets/hints. |
| New Project from Template | Codex | Accepted feature | Replace implicit file creation with an explicit project creation flow. |

## Recommended Queue

There were no existing numbered files in `kanban/pending/`, and `kanban/ice-box/` and `kanban/done/` only contained `.gitkeep`, so no existing work item was updated or skipped.

Bug work:

- `kanban/pending/001-scene-vector-parser-crash-bug.md`
- `kanban/pending/002-scene-parser-lifetime-and-pass-order-bug.md`
- `kanban/pending/003-vulkan-scene-lifetime-thread-ownership-bug.md`
- `kanban/pending/004-vulkan-shader-diagnostics-and-cache-paths-bug.md`
- `kanban/pending/005-vulkan-descriptor-diagnostic-crashes-bug.md`
- `kanban/pending/006-multi-model-pass-buffer-binding-bug.md`
- `kanban/pending/007-model-hot-reload-immutable-gpu-assets-bug.md`
- `kanban/pending/008-model-loader-validation-bug.md`
- `kanban/pending/009-project-copy-nested-dependencies-bug.md`
- `kanban/pending/010-editor-menu-null-buffer-guards-bug.md`
- `kanban/pending/011-validation-context-thread-safety-bug.md`
- `kanban/pending/012-vulkan-frame-capture-reporting-bug.md`
- `kanban/pending/013-post-2d-disabled-feature-surface-bug.md`
- `kanban/pending/014-no-surprise-scenegraph-creation-bug.md`

Test work:

- `kanban/pending/015-failure-mode-stability-suite-test.md`
- `kanban/pending/016-wire-python-tests-into-workflow-test.md`

Refactor work:

- `kanban/pending/017-live-edit-pipeline-refactor.md`
- `kanban/pending/018-split-vulkan-pass-and-scene-parser-refactor.md`

Feature work:

- `kanban/pending/019-preserve-camera-state-feature.md`
- `kanban/pending/020-filesystem-watcher-and-include-dependencies-feature.md`
- `kanban/pending/021-diagnostics-and-project-health-panel-feature.md`
- `kanban/pending/022-runtime-inspectors-and-profiler-feature.md`
- `kanban/pending/023-recording-and-device-reset-ui-feature.md`
- `kanban/pending/024-scenegraph-schema-autocomplete-feature.md`
- `kanban/pending/025-node-graph-domain-integration-feature.md`
- `kanban/pending/026-new-project-template-flow-feature.md`

## Interdependencies

- `003-vulkan-scene-lifetime-thread-ownership-bug` is the backbone for `007-model-hot-reload-immutable-gpu-assets-bug`, `011-validation-context-thread-safety-bug`, `015-failure-mode-stability-suite-test`, `017-live-edit-pipeline-refactor`, `020-filesystem-watcher-and-include-dependencies-feature`, and `023-recording-and-device-reset-ui-feature`.
- `001-scene-vector-parser-crash-bug` and `002-scene-parser-lifetime-and-pass-order-bug` should land before `024-scenegraph-schema-autocomplete-feature`; the schema work should not encode unstable parser behavior.
- `004-vulkan-shader-diagnostics-and-cache-paths-bug` should land before the shader-include portion of `020-filesystem-watcher-and-include-dependencies-feature` and before the shader section of `021-diagnostics-and-project-health-panel-feature`.
- `005-vulkan-descriptor-diagnostic-crashes-bug` should land before broad descriptor/lifetime tests in `015-failure-mode-stability-suite-test`.
- `006-multi-model-pass-buffer-binding-bug` should either land before the multi-model test case in `015` is turned on, or that test should be added as an expected failure only in a temporary branch.
- `009-project-copy-nested-dependencies-bug`, `014-no-surprise-scenegraph-creation-bug`, and `026-new-project-template-flow-feature` are linked. The bug fixes can land independently, but the final user experience should be designed together.
- `019-preserve-camera-state-feature` is mostly independent and is a good small task for a single agent.
- `022-runtime-inspectors-and-profiler-feature` depends on stable target/model/uniform metadata but can be split across subagents after the data contracts are defined.
- `025-node-graph-domain-integration-feature` depends on the live-edit pipeline and schema work if the graph is meant to edit real scene data rather than just visualize it.

## Subagent Guidance

Subagents make sense for independent slices once this queue is being executed:

- One worker can own scene parser hardening: `001`, `002`, and the parser portions of `015`.
- One worker can own Vulkan lifetime/thread ownership: `003`, with a separate reviewer or worker for focused stress tests.
- One worker can own shader/descriptor diagnostics: `004` and `005`.
- One worker can own project/editor app-shell issues: `009`, `010`, `014`, and later `026`.
- Feature work should wait until the safety backbone is in place, except `019`, which is small and isolated.
