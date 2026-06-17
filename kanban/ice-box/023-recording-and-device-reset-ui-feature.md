# Recording And Device Reset UI Implementation Plan

> **For agentic workers:** Use `superpowers:subagent-driven-development` if implementing recording controls and reset-device flow together. Keep checkboxes updated as work is completed.

**Goal:** Add in-app controls for frame recording and an explicit reset-device affordance after the backend can reset safely.

**Agents:** Claude requested in-app recording UI and a reset-device affordance. Codex identified device-loss recovery races and Vulkan capture reporting gaps. Gemini had no substantive final finding.

## Files

- Modify: `app/src/menu.cpp`
- Modify: `app/src/main.cpp`
- Add or modify app window files under `app/src/` and `app/include/app/`
- Modify: `include/vklive/IDevice.h` if reset/recording APIs need to be explicit
- Modify backend device files as needed
- Add tests under `tests/`

## Implementation Plan

- [ ] Do not implement reset-device UI until `003-vulkan-scene-lifetime-thread-ownership-bug` has removed worker/device races.
- [ ] Add a recording control window or menu section with output directory, start frame, end frame, current frame, and enable/disable recording.
- [ ] Reuse existing scene recording fields: `Scene::recording`, `minRecordFrame`, `maxRecordFrame`, `GlobalFrameCount`, and `configure_record_one_frame()` behavior.
- [ ] Validate output paths and report capture errors using the fixed path from `012-vulkan-frame-capture-reporting-bug`.
- [ ] Add reset-device UI only after there is a safe sequence: stop reload GPU work, wait idle, destroy scenes, recreate backend, rebuild current scene generation, then resume.
- [ ] If reset cannot be made safe yet, expose a disabled menu item with an internal comment explaining the dependency rather than a button that does not work.
- [ ] Add tests for recording state transitions where possible without a visible GPU device.
- [ ] Add a startup-frame smoke path for reset only if the backend can be exercised headlessly.
- [ ] Run `python3 do.py build debug`.
- [ ] Run `python3 do.py test debug -- -R "app_command_line|render_backend"`.

## Acceptance Criteria

- [ ] Recording can be configured in-app without command-line-only flags.
- [ ] Capture failures are visible.
- [ ] Reset-device UI either performs a safe reset or is intentionally gated until safe.
- [ ] No reset path races with reload worker/device ownership.

## Dependencies

Depends on `003-vulkan-scene-lifetime-thread-ownership-bug` and `012-vulkan-frame-capture-reporting-bug`.

Consensus reviewer: <gpt-5-codex>
