# Repository Guidelines

If you see improvements in what you have just done, or have ideas for ways it can be done differently, add a short note. This project is also used for learning, so explain relevant connections when they help.

## Project Overview

VkLive is a C++20 Vulkan live-coding editor for shader and scene editing. The app executable is `Rezonality`; the core library target is `vklive`.

Primary code areas:

- `app/`: SDL/ImGui application shell, windows, menus, project handling, and editor integration.
- `src/` and `include/vklive/`: core engine, scene parsing, Vulkan rendering, shaders, models, process helpers, and validation.
- `examples/` and `tests/content/`: sample scenegraph/shader/model projects.
- `libs/`: vendored/static source snapshots and project-owned library code. Avoid broad edits here unless the task explicitly targets those dependencies.
- `vcpkg/` and `.cache/vcpkg/`: ignored local dependency manager checkouts, when present.
- `build/`, `build_llvm/`, `out/`, and `run_tree/`: generated/local output. Do not treat these as source.

## Build And Setup

Use `do.py` as the single project workflow entrypoint. It configures CMake presets, uses Ninja, and exposes `compile_commands.json` at the repo root for clangd/Vim LSP.

```sh
python3 do.py doctor
python3 do.py setup
python3 do.py config debug
python3 do.py build debug
python3 do.py test debug
python3 do.py run debug
```

Short forms default to `Debug`:

```sh
python3 do.py config
python3 do.py build
python3 do.py run
```

On Windows, use `python do.py ...` if that is the available Python launcher. If your shell aliases `dr` to `python3 do.py`, the equivalent commands are `dr run`, `dr run debug`, and `dr run release`.

Useful direct commands after configuration:

```sh
cmake --build build/debug --config Debug --parallel
ctest --test-dir build/debug --output-on-failure
```

Mac defaults to the Metal renderer feature. Windows and Linux default to Vulkan. GitHub Actions currently uses Vulkan SDK `1.3.283.0` for Vulkan jobs; local machines may use newer SDKs, but Vulkan API/extension changes should be checked carefully.

## Testing And Verification

There is no root CTest suite wired up in `CMakeLists.txt` at the moment. For code changes, use the smallest practical verification:

- Compile the touched target with `python3 do.py build debug` when dependencies and SDK are available.
- For shader/scenegraph work, use the relevant sample under `examples/` or `tests/content/`.
- For documentation-only changes, inspect the rendered Markdown/text and confirm `git status --short`.

If a build cannot be run because local dependencies are missing or expensive to install, say that explicitly and describe what was checked instead.

## Code Style

- Follow `.clang-format`: 4-space indentation, Allman-style braces, pointer alignment left, sorted includes, and no column limit.
- The codebase is mostly C-like C++: structs and free functions are common. Prefer existing local patterns over introducing new abstractions.
- Keep comments useful and sparse. Many TODOs already mark known technical debt; avoid expanding them unless directly related to the task.
- Preserve platform conditionals (`WIN32`, `__APPLE__`, Linux paths) when editing Vulkan, SDL, or filesystem code.

## Dependency Notes

Dependency versions are described by `vcpkg.json`. `do.py setup` bootstraps an ignored local vcpkg checkout under `.cache/vcpkg` if `VCPKG_ROOT`, `vcpkg/`, or `.cache/vcpkg/` do not already provide a usable toolchain. Major dependencies include Vulkan or Metal, SDL2, ImGui/Zest/Zep, assimp, fmt, range-v3, reproc, native file dialog libraries, SPIR-V reflection, gli, lodepng, and Zing.

When modifying dependency usage:

- Update `vcpkg.json`, `CMakeLists.txt`, and `do.py` defaults if package requirements or renderer feature defaults change.
- Check `.github/workflows/builds.yml` for CI matrix implications.
- Prefer changes in project-owned wrappers over editing vendored source directly.

## Git Hygiene

- Do not revert user changes or generated local state unless explicitly asked.
- Keep edits scoped to project-owned files whenever possible.
- Treat large vendored trees as read-mostly context.
