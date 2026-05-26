Explore the repository source files directly. Use `rg --files` and read the actual files as they exist on disk; do not rely on any pre-generated combined file.

This repository is VkLive, a C++20 Vulkan live-coding editor for shader and scene editing. The app executable is `Rezonality`; the core library target is `vklive`.

Review the project as a desktop graphics tool and live shader editor. Pay attention to:

- `app/`: SDL, ImGui docking, menus, windows, project handling, node graph integration, and editor integration.
- `src/` and `include/vklive/`: core engine, Vulkan rendering, scene parsing, shader/model loading, validation, and fault-tolerant runtime behavior.
- `examples/`, `Templates/` if present, `tests/content/`, and `run_tree/`: sample projects and content workflows.
- `tests/`: unit and smoke-test coverage for scene parsing, model loading, command-line handling, node graph behavior, and render backend lifetime.
- `cmake/`, `CMakePresets.json`, `vcpkg.json`, and `do.py`: local build ergonomics, vcpkg behavior, compiler cache use, and fast no-change loops.
- `libs/`, `zep/`, and other vendored/dependency folders: treat these as read-mostly and focus review on project-owned integration code.
- `plans/` and `kanban/`: implementation plans, reviews, and work queue.

Look for bugs, crash risks, Vulkan lifetime hazards, stale descriptor/image ownership, shader hot-reload failure modes, scenegraph parse failures, missing validation, test gaps, and code structure that will make it harder for multiple agents to work in parallel. This is meant to be fault tolerant: users should be able to dynamically edit scene files and shaders without the app falling over when they make a mistake.

Do a thorough review. Look for opportunities to separate concerns and keep the application modular, especially around rendering resource lifetime, scene/model/material loading, editor windows, node graph integration, and project/run-tree handling.

At the end of the report give:

- the top 10 good things about the application
- the top 10 bad things about the application
- the best 10 quality-of-life features to improve the tool
- the best 10 tests that could improve stability
- the worst 10 features or design choices currently in the application

Return the entire output as markdown so a script can save it as a complete report.
