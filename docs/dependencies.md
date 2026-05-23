# Dependency And Build Workflow

VkLive is now the integration point for Rezonality's live-coding stack. The owned libraries are normal source directories in this repo, not git submodules:

- `libs/zep`
- `libs/zest`
- `libs/zing`

Small third-party libraries that are awkward to package may live as static source snapshots. Standard package dependencies are installed through vcpkg manifest mode.

## Snapshot Provenance

The first snapshot import used these source commits:

| Library | Snapshot Path | Source Commit |
| --- | --- | --- |
| Zep | `libs/zep` | `7aa7ed940d5c889dc6b27a2529a740c0bb2ad6af` |
| Zing | `libs/zing` | `4c2e7729484335171148c0468d51bda9d7fc0983` |
| Zest | `libs/zest` | `39d1a10b623d3415409d570b42f9efb0cf4f9377` |
| libremidi | `libs/zing/libs/remidi` | `95584d139f19f3128cb95bd279cdfa6191e77a5f` |
| TinySoundFont | `libs/zing/libs/tsf` | `fbc913531b85f5707f49115110bb86b1cd583885` |

`libs/rccp` and `libs/nanovg_vulkan` were removed from the live source tree because the current build does not reference them.

## Daily Commands

Use `do.py` directly, or the `dr` alias if your shell defines it:

```sh
python3 do.py doctor
python3 do.py setup
python3 do.py config debug
python3 do.py build debug
python3 do.py test debug
python3 do.py run debug
```

Short forms default to Debug:

```sh
python3 do.py config
python3 do.py build
python3 do.py run
```

`config` uses CMake presets and Ninja. It also exposes the configured build database at the repo root:

```text
compile_commands.json
```

That root file is generated editor state and is intentionally ignored by git. Vim LSP, clangd, and similar tools should pick it up from there.

## Getting Latest And Pushing

There are no submodules to synchronize. The helper commands are root-repo only:

```sh
python3 do.py sync
python3 do.py push
```

`sync` runs:

```sh
git fetch --prune
git pull --ff-only
```

`push` runs:

```sh
git push
```

## vcpkg

The repo no longer tracks vcpkg as a submodule. Dependency versions are described by `vcpkg.json`.

Discovery order for the vcpkg toolchain is:

1. `VCPKG_ROOT`
2. local ignored `vcpkg/`
3. local ignored `.cache/vcpkg/`

For a fresh checkout, run:

```sh
python3 do.py setup
```

That bootstraps a local ignored vcpkg checkout under `.cache/vcpkg` if no usable vcpkg is already available.

## CMake Presets

The standard build directories are:

```text
build/debug
build/release
build/relwithdebinfo
```

The presets use Ninja and set `CMAKE_EXPORT_COMPILE_COMMANDS=ON`.

Renderer-specific configuration can be passed through `do.py config`:

```sh
python3 do.py config debug -- -DVKLIVE_ENABLE_METAL=ON -DVKLIVE_ENABLE_VULKAN=OFF
python3 do.py config release -- -DVKLIVE_ENABLE_METAL=OFF -DVKLIVE_ENABLE_VULKAN=ON
```

## Legacy Scripts

The shell and batch wrappers remain for muscle memory:

```sh
./prebuild.sh
./config.sh Debug
./build.sh Debug
```

They delegate to `do.py`. Prefer `do.py` for new workflow documentation.
