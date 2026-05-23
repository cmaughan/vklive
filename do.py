#!/usr/bin/env python3
from __future__ import annotations

import os
import pathlib
import platform
import shlex
import shutil
import subprocess
import sys
from types import SimpleNamespace


CONFIGS = {
    "debug": "Debug",
    "release": "Release",
    "relwithdebinfo": "RelWithDebInfo",
    "reldbg": "RelWithDebInfo",
}

VCPKG_REPOSITORY = "https://github.com/microsoft/vcpkg.git"
VCPKG_BASELINE = "38d91be5efb2f21fbef4a3c53295002823747431"


def repo_root() -> pathlib.Path:
    return pathlib.Path(__file__).resolve().parent


def help_text() -> str:
    return """Usage:
  dr doctor
  dr setup
  dr config [debug|release|relwithdebinfo] [-- cmake-args...]
  dr build [debug|release|relwithdebinfo]
  dr test [debug|release|relwithdebinfo] [-- ctest-args...]
  dr run [debug|release|relwithdebinfo] [-- app-args...]
  dr sync
  dr push
  dr clean [debug|release|relwithdebinfo]

Examples:
  dr doctor       Check CMake, Ninja, compiler cache, and vcpkg discovery
  dr setup        Bootstrap an ignored local vcpkg checkout if needed
  dr config       Configure Debug with Ninja and expose compile_commands.json
  dr build        Build Debug without reconfiguring
  dr run release -- --project run_tree/projects/pbr_robot --scenegraph uv_debug.scenegraph
  dr test debug -- -R zep
"""


def parse_args(args: list[str]) -> SimpleNamespace:
    if not args or args[0] in {"-h", "--help", "help"}:
        print(help_text())
        raise SystemExit(0)

    command = args[0].lower()
    rest = args[1:]

    if command == "latest":
        command = "sync"
    elif command == "configure":
        command = "config"
    elif command == "publish":
        command = "push"

    if command in {"doctor", "setup", "sync", "push"}:
        if rest:
            _unexpected(rest[0])
        return SimpleNamespace(command=command)

    if command == "config":
        config, remaining = parse_config(rest)
        cmake_args: list[str] = []
        if remaining:
            if remaining[0] == "--":
                cmake_args = remaining[1:]
            else:
                _unexpected(remaining[0])
        return SimpleNamespace(command=command, config=config, cmake_args=cmake_args)

    if command in {"build", "clean"}:
        config, remaining = parse_config(rest)
        if remaining:
            _unexpected(remaining[0])
        return SimpleNamespace(command=command, config=config)

    if command == "test":
        config, remaining = parse_config(rest)
        ctest_args = []
        if remaining:
            if remaining[0] == "--":
                ctest_args = remaining[1:]
            else:
                _unexpected(remaining[0])
        return SimpleNamespace(command=command, config=config, ctest_args=ctest_args)

    if command == "run":
        config, remaining = parse_config(rest, allow_leading_option=True)
        app_args: list[str] = []
        if remaining:
            if remaining[0] == "--":
                app_args = remaining[1:]
            elif remaining[0].startswith("--"):
                app_args = remaining
            else:
                _unexpected(remaining[0])
        return SimpleNamespace(command=command, config=config, app_args=app_args)

    print(f"Unknown command: {args[0]}\n")
    print(help_text())
    raise SystemExit(2)


def parse_config(args: list[str], allow_leading_option: bool = False) -> tuple[str, list[str]]:
    if not args:
        return "Debug", []

    mode = args[0].lower()
    if mode in CONFIGS:
        return CONFIGS[mode], args[1:]

    if allow_leading_option and args[0].startswith("--"):
        return "Debug", args

    print(f"Unknown build mode: {args[0]}\n")
    print(help_text())
    raise SystemExit(2)


def _unexpected(arg: str) -> None:
    print(f"Unexpected argument: {arg}\n")
    print(help_text())
    raise SystemExit(2)


def preset_name(config: str) -> str:
    for key, value in CONFIGS.items():
        if value == config and key != "reldbg":
            return key
    raise ValueError(f"Unknown config: {config}")


def build_dir(root: pathlib.Path, config: str) -> pathlib.Path:
    return root / "build" / preset_name(config)


def exe_path(root: pathlib.Path, config: str) -> pathlib.Path:
    directory = build_dir(root, config)
    if sys.platform.startswith("win"):
        return directory / "Rezonality.exe"

    bundle_exe = directory / "Rezonality.app" / "Contents" / "MacOS" / "Rezonality"
    if sys.platform == "darwin" and bundle_exe.exists():
        return bundle_exe
    return directory / "Rezonality"


def triplet() -> str:
    machine = platform.machine().lower()
    if sys.platform.startswith("win"):
        return "x64-windows-static-md"
    if sys.platform == "darwin":
        return "arm64-osx" if machine == "arm64" else "x64-osx"
    return "x64-linux"


def vcpkg_roots(root: pathlib.Path) -> list[pathlib.Path]:
    roots: list[pathlib.Path] = []
    if os.environ.get("VCPKG_ROOT"):
        roots.append(pathlib.Path(os.environ["VCPKG_ROOT"]))
    roots.extend(
        [
            root / "vcpkg",
            root / ".cache" / "vcpkg",
        ]
    )
    return roots


def vcpkg_toolchain(root: pathlib.Path) -> pathlib.Path | None:
    for candidate in vcpkg_roots(root):
        toolchain = candidate / "scripts" / "buildsystems" / "vcpkg.cmake"
        if toolchain.exists():
            return toolchain
    return None


def configure_command(root: pathlib.Path, config: str, cmake_args: list[str] | None = None) -> list[str]:
    command = [
        "cmake",
        "--preset",
        preset_name(config),
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        f"-DVCPKG_TARGET_TRIPLET={triplet()}",
    ]
    toolchain = vcpkg_toolchain(root)
    if toolchain is not None:
        command.append(f"-DCMAKE_TOOLCHAIN_FILE={toolchain}")
    if cmake_args:
        command.extend(cmake_args)
    return command


def build_command(root: pathlib.Path, config: str) -> list[str]:
    return ["cmake", "--build", str(build_dir(root, config)), "--config", config, "--parallel"]


def test_command(root: pathlib.Path, config: str, ctest_args: list[str]) -> list[str]:
    return ["ctest", "--test-dir", str(build_dir(root, config)), "--output-on-failure", *ctest_args]


def command_text(command: list[str]) -> str:
    return " ".join(shlex.quote(str(part)) for part in command)


def run(command: list[str], cwd: pathlib.Path, dry_run: bool = False) -> int:
    print("> " + command_text(command))
    if dry_run:
        return 0
    return subprocess.run(command, cwd=cwd, check=False).returncode


def run_commands(commands: list[list[str]], cwd: pathlib.Path, dry_run: bool = False) -> int:
    for command in commands:
        rc = run(command, cwd, dry_run=dry_run)
        if rc != 0:
            return rc
    return 0


def expose_compile_commands(root: pathlib.Path, directory: pathlib.Path) -> None:
    source = directory / "compile_commands.json"
    if not source.exists():
        return

    destination = root / "compile_commands.json"
    if destination.exists() or destination.is_symlink():
        destination.unlink()

    try:
        os.symlink(source, destination)
    except OSError:
        shutil.copy2(source, destination)


def configure(root: pathlib.Path, config: str, cmake_args: list[str] | None = None) -> int:
    rc = run(configure_command(root, config, cmake_args), root)
    if rc == 0:
        expose_compile_commands(root, build_dir(root, config))
    return rc


def build(root: pathlib.Path, config: str) -> int:
    return run(build_command(root, config), root)


def test(root: pathlib.Path, config: str, ctest_args: list[str]) -> int:
    return run(test_command(root, config, ctest_args), root)


def run_project(root: pathlib.Path, config: str, app_args: list[str]) -> int:
    rc = configure(root, config)
    if rc != 0:
        return rc

    rc = build(root, config)
    if rc != 0:
        return rc

    exe = exe_path(root, config)
    if not exe.exists():
        print(f"Missing executable: {exe}", file=sys.stderr)
        return 1

    return run([str(exe), *app_args], root)


def doctor(root: pathlib.Path) -> int:
    commands = [
        ["cmake", "--version"],
        ["ninja", "--version"],
    ]
    rc = run_commands(commands, root)
    if rc != 0:
        return rc

    cache_tool = shutil.which("ccache") or shutil.which("sccache")
    if cache_tool:
        run([cache_tool, "--version"], root)
    else:
        print("No compiler cache found; install ccache or sccache for faster rebuilds.")

    toolchain = vcpkg_toolchain(root)
    if toolchain:
        print(f"Using vcpkg toolchain: {toolchain}")
    else:
        print("No vcpkg toolchain found. Run `python3 do.py setup` or set VCPKG_ROOT.", file=sys.stderr)
        return 1

    return 0


def setup(root: pathlib.Path) -> int:
    if vcpkg_toolchain(root) is not None:
        print(f"vcpkg already available: {vcpkg_toolchain(root)}")
        return 0

    checkout = root / ".cache" / "vcpkg"
    checkout.parent.mkdir(parents=True, exist_ok=True)
    rc = run(["git", "clone", VCPKG_REPOSITORY, str(checkout)], root)
    if rc != 0:
        return rc

    rc = run(["git", "checkout", VCPKG_BASELINE], checkout)
    if rc != 0:
        return rc

    bootstrap = "bootstrap-vcpkg.bat" if sys.platform.startswith("win") else "./bootstrap-vcpkg.sh"
    return run([bootstrap, "-disableMetrics"], checkout)


def sync_repo(root: pathlib.Path) -> int:
    return run_commands(
        [
            ["git", "fetch", "--prune"],
            ["git", "pull", "--ff-only"],
        ],
        root,
    )


def push_repo(root: pathlib.Path) -> int:
    return run(["git", "push"], root)


def clean(root: pathlib.Path, config: str) -> int:
    directory = build_dir(root, config)
    if directory.exists():
        shutil.rmtree(directory)
        print(f"Removed {directory}")
    return 0


def main() -> int:
    parsed = parse_args(sys.argv[1:])
    root = repo_root()
    if parsed.command == "doctor":
        return doctor(root)
    if parsed.command == "setup":
        return setup(root)
    if parsed.command == "config":
        return configure(root, parsed.config, parsed.cmake_args)
    if parsed.command == "build":
        return build(root, parsed.config)
    if parsed.command == "test":
        return test(root, parsed.config, parsed.ctest_args)
    if parsed.command == "run":
        return run_project(root, parsed.config, parsed.app_args)
    if parsed.command == "sync":
        return sync_repo(root)
    if parsed.command == "push":
        return push_repo(root)
    if parsed.command == "clean":
        return clean(root, parsed.config)
    print(f"Unknown command: {parsed.command}", file=sys.stderr)
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
