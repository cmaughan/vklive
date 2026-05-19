#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import subprocess
import sys
from types import SimpleNamespace


CONFIGS = {
    "debug": "Debug",
    "release": "Release",
}


def repo_root() -> pathlib.Path:
    return pathlib.Path(__file__).resolve().parent


def help_text() -> str:
    return """Usage:
  dr run [debug|release] [-- app-args...]

Examples:
  dr run          Configure, build, and run Debug
  dr run debug    Configure, build, and run Debug
  dr run release  Configure, build, and run Release
"""


def parse_args(args: list[str]) -> SimpleNamespace:
    if not args or args[0] in {"-h", "--help", "help"}:
        print(help_text())
        raise SystemExit(0)

    command = args[0].lower()
    if command != "run":
        print(f"Unknown command: {args[0]}\n")
        print(help_text())
        raise SystemExit(2)

    config = "Debug"
    app_args: list[str] = []
    rest = args[1:]
    if rest:
        mode = rest[0].lower()
        if mode in CONFIGS:
            config = CONFIGS[mode]
            rest = rest[1:]
        elif rest[0] != "--":
            print(f"Unknown build mode: {rest[0]}\n")
            print(help_text())
            raise SystemExit(2)

    if rest:
        if rest[0] == "--":
            app_args = rest[1:]
        else:
            print(f"Unexpected argument: {rest[0]}\n")
            print(help_text())
            raise SystemExit(2)

    return SimpleNamespace(command=command, config=config, app_args=app_args)


def exe_path(root: pathlib.Path, config: str) -> pathlib.Path:
    if sys.platform.startswith("win"):
        return root / "build" / config / "Rezonality.exe"

    bundle_exe = root / "build" / "Rezonality.app" / "Contents" / "MacOS" / "Rezonality"
    if bundle_exe.exists():
        return bundle_exe
    return root / "build" / "Rezonality"


def run(command: list[str], cwd: pathlib.Path) -> int:
    print("> " + " ".join(command))
    return subprocess.run(command, cwd=cwd, check=False).returncode


def configure_command(root: pathlib.Path, config: str) -> list[str]:
    if sys.platform.startswith("win"):
        return ["cmd", "/c", str(root / "config.bat")]
    return ["bash", str(root / "config.sh"), config]


def build_command(root: pathlib.Path, config: str) -> list[str]:
    if sys.platform.startswith("win"):
        return ["cmd", "/c", str(root / "build.bat"), config]
    return ["bash", str(root / "build.sh"), config]


def run_project(root: pathlib.Path, config: str, app_args: list[str]) -> int:
    rc = run(configure_command(root, config), root)
    if rc != 0:
        return rc

    rc = run(build_command(root, config), root)
    if rc != 0:
        return rc

    exe = exe_path(root, config)
    if not exe.exists():
        print(f"Missing executable: {exe}", file=sys.stderr)
        return 1

    return run([str(exe), *app_args], root)


def main() -> int:
    parsed = parse_args(sys.argv[1:])
    return run_project(repo_root(), parsed.config, parsed.app_args)


if __name__ == "__main__":
    raise SystemExit(main())
