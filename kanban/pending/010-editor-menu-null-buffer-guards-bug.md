# Editor And Menu Null Buffer Guards Implementation Plan

> **For agentic workers:** Use `superpowers:executing-plans` or `superpowers:subagent-driven-development` to implement this task step by step. Keep checkboxes updated as work is completed.

**Goal:** Remove small app-shell crash hazards around editor buffer removal, split menu actions, and minimized window state.

**Agents:** Codex identified these UI/app-shell bugs. Claude did not focus on these specific items. Gemini had no substantive final finding.

## Files

- Modify: `app/src/editor.cpp`
- Modify: `app/src/menu.cpp`
- Modify: `app/src/main.cpp`
- Add or modify editor/menu tests under `tests/`

## Implementation Plan

- [ ] In `zep_update_files()`, copy buffer raw pointers into a temporary vector before calling `RemoveBuffer()` so the code does not mutate the container being iterated.
- [ ] Add a test or small helper coverage for reset-with-multiple-buffers if the Zep test harness can construct buffers without a full app window.
- [ ] In `menu_show()`, compute whether an active tab window and active editor window exist before enabling split actions.
- [ ] Pass the enabled flag to `ImGui::MenuItem()` for Horizontal Split and Vertical Split, and keep defensive null checks inside the action branch.
- [ ] Decide minimized-state behavior. The minimal fix is to stop saving `WindowState::Minimized` so a minimized exit restores as normal next launch. If restore support is desired, update `init_sdl_window()` to honor it intentionally.
- [ ] Add a test for config/window-state serialization if the app config tests already cover nearby behavior.
- [ ] Run `python3 do.py build debug`.
- [ ] Run `python3 do.py test debug -- -R "editor|app_command_line"`.

## Acceptance Criteria

- [ ] Project switching/resetting editor files cannot invalidate the buffer iteration loop.
- [ ] Split menu actions are disabled or harmless when no active editor tab/window exists.
- [ ] Minimized window state is either intentionally restored or no longer persisted as a misleading state.
- [ ] Existing editor backend tests still pass.

Consensus reviewer: <gpt-5-codex>
