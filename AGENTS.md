# Repository Guidelines

If you see improvements in what you have just done, or have ideas for ways it can be done differently, add a short note. This project is also used for learning, so explain relevant connections when they help.

## Project Overview

VkLive is a C++20 Vulkan live-coding editor for shader and scene editing. The app executable is `Rezonality`; the core library target is `vklive`.

Primary code areas:

- `app/`: SDL/ImGui application shell, windows, menus, project handling, and editor integration.
- `src/` and `include/vklive/`: core engine, scene parsing, Vulkan rendering, shaders, models, process helpers, and validation.
- `examples/` and `tests/content/`: sample scenegraph/shader/model projects.
- `libs/`, `vcpkg/`, and `zep/`: vendored or submodule dependencies. Avoid broad edits here unless the task explicitly targets those dependencies.
- `build/`, `build_llvm/`, `out/`, and `run_tree/`: generated/local output. Do not treat these as source.

## Build And Setup

Initialize dependencies before building:

```sh
git submodule update --init --recursive
```

Windows workflow:

```bat
prebuild.bat
config.bat
build.bat
```

`prebuild.bat` installs vcpkg dependencies and is only needed after a fresh checkout or dependency changes. The normal local setup/build loop is:

```bat
config.bat
build.bat
```

`build.bat` defaults to `Debug`; pass another configuration explicitly when needed, for example `build.bat Release`.

The repo also has a Draxul-style Python wrapper:

```bat
python do.py run
python do.py run debug
python do.py run release
```

If your shell aliases `dr` to `python do.py`, the equivalent commands are `dr run`, `dr run debug`, and `dr run release`. The wrapper runs `config.bat`, then `build.bat <Config>`, then launches `build\<Config>\Rezonality.exe`.

Linux/macOS workflow:

```sh
./prebuild.sh
./config.sh Debug
./build.sh Debug
```

Useful direct build command after configuration:

```sh
cmake --build build --config Debug
```

The project expects a Vulkan SDK installation. GitHub Actions currently uses Vulkan SDK `1.3.283.0`; local machines may use newer SDKs, but Vulkan API/extension changes should be checked carefully.

## Testing And Verification

There is no root CTest suite wired up in `CMakeLists.txt` at the moment. For code changes, use the smallest practical verification:

- Compile the touched target with `cmake --build build --config Debug` when dependencies and SDK are available.
- For shader/scenegraph work, use the relevant sample under `examples/` or `tests/content/`.
- For documentation-only changes, inspect the rendered Markdown/text and confirm `git status --short`.

If a build cannot be run because local dependencies are missing or expensive to install, say that explicitly and describe what was checked instead.

## Code Style

- Follow `.clang-format`: 4-space indentation, Allman-style braces, pointer alignment left, sorted includes, and no column limit.
- The codebase is mostly C-like C++: structs and free functions are common. Prefer existing local patterns over introducing new abstractions.
- Keep comments useful and sparse. Many TODOs already mark known technical debt; avoid expanding them unless directly related to the task.
- Preserve platform conditionals (`WIN32`, `__APPLE__`, Linux paths) when editing Vulkan, SDL, or filesystem code.

## Dependency Notes

The build uses the checked-in `vcpkg` submodule as the CMake toolchain and installs packages through `prebuild.*`. Major dependencies include Vulkan, SDL2, ImGui/Zest/Zep, assimp, fmt, range-v3, reproc, native file dialog libraries, SPIR-V reflection, gli, lodepng, and Zing.

When modifying dependency usage:

- Update both Windows and Unix prebuild scripts if package requirements change.
- Check `.github/workflows/builds.yml` for CI matrix implications.
- Prefer changes in project-owned wrappers over editing vendored source directly.

## Git Hygiene

- Do not revert user changes or generated local state unless explicitly asked.
- Keep edits scoped to project-owned files whenever possible.
- Treat large vendored trees as read-mostly context.
