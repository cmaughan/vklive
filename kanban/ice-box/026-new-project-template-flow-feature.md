# New Project Template Flow Implementation Plan

> **For agentic workers:** Use `superpowers:executing-plans` or `superpowers:subagent-driven-development` to implement this task step by step. Keep checkboxes updated as work is completed.

**Goal:** Add an explicit "New Project From Template" flow so project creation is intentional and no longer hidden inside scene loading.

**Agents:** Codex proposed this feature and paired it with the no-surprise file creation bug. Claude did not focus on project creation flow specifically. Gemini had no substantive final finding.

## Files

- Modify: `app/src/menu.cpp`
- Modify: `app/src/controller.cpp`
- Modify: `app/src/project.cpp`
- Modify or add app headers under `app/include/app/`
- Use templates under `run_tree/projects/`
- Add or modify tests under `tests/project_copy_tests.cpp` or new app/project tests

## Implementation Plan

- [ ] Land `014-no-surprise-scenegraph-creation-bug` first so open/load no longer creates files implicitly.
- [ ] Define available templates from `run_tree/projects/`, starting with `default`, `simple`, `pbr_robot`, and `ray_tracer` if those are appropriate.
- [ ] Add a menu action such as `File > New Project From Template`.
- [ ] Prompt for template and destination folder using the existing native file dialog/project controller patterns.
- [ ] Copy the selected template to the destination using the fixed `project_copy()` dependency behavior from `009-project-copy-nested-dependencies-bug` or a dedicated template-copy helper.
- [ ] After successful copy, load the new project and enqueue it through the normal reload path.
- [ ] If destination exists and is non-empty, require explicit overwrite/merge confirmation.
- [ ] Add tests for template copy into an empty destination, destination already exists, and missing template.
- [ ] Run `python3 do.py build debug`.
- [ ] Run `python3 do.py test debug -- -R "project|app_command_line"`.

## Acceptance Criteria

- [ ] Users can create a project intentionally from a known template.
- [ ] Opening an existing folder and creating a new project are distinct flows.
- [ ] Template copy preserves nested assets and reports failures clearly.
- [ ] The newly created project loads through the standard keep-last-good reload path.

## Dependencies

Depends on `014-no-surprise-scenegraph-creation-bug`; strongly benefits from `009-project-copy-nested-dependencies-bug`.

Consensus reviewer: <gpt-5-codex>
