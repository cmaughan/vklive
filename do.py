#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import shlex
import subprocess
import sys
from types import SimpleNamespace


CONFIGS = {
    "debug": "Debug",
    "release": "Release",
}

GIT_DEFAULTS = (
    ("submodule.recurse", "true"),
    ("fetch.recurseSubmodules", "on-demand"),
    ("status.submoduleSummary", "true"),
    ("diff.submodule", "log"),
    ("push.recurseSubmodules", "check"),
)

SUBMODULE_BRANCH_SYNC_SCRIPT = r"""
branch="$(git config -f "$toplevel/.gitmodules" "submodule.$name.branch" 2>/dev/null || true)"
if [ -n "$branch" ]; then
    git fetch origin --prune
    git checkout "$branch"
    git pull --ff-only origin "$branch"
fi
""".strip()

SUBMODULE_PUSH_SCRIPT = r"""
branch="$(git symbolic-ref --quiet --short HEAD 2>/dev/null || true)"
url="$(git remote get-url origin 2>/dev/null || true)"
case "$url" in
    *github.com/Rezonality/*|*github.com:Rezonality/*)
        can_push=1
        ;;
    *)
        can_push=0
        ;;
esac
if [ -n "$branch" ] && [ "$can_push" -eq 1 ]; then
    git push -u origin "$branch"
elif [ -n "$branch" ]; then
    echo "Skipping third-party submodule $name on $branch ($url)"
else
    echo "Skipping detached submodule $name at $(git rev-parse --short HEAD)"
fi
""".strip()

SUBMODULE_STATUS_SCRIPT = r"""
git status --short --branch
branch="$(git symbolic-ref --quiet --short HEAD 2>/dev/null || true)"
if [ -n "$branch" ]; then
    upstream="$(git rev-parse --abbrev-ref --symbolic-full-name @{u} 2>/dev/null || true)"
    if [ -n "$upstream" ]; then
        git rev-list --left-right --count HEAD...@{u}
    fi
fi
""".strip()

SUBMODULE_CLEAN_SCRIPT = "git reset --hard && git clean -fdx"


def repo_root() -> pathlib.Path:
    return pathlib.Path(__file__).resolve().parent


def help_text() -> str:
    return """Usage:
  dr run [debug|release] [-- app-args...]
  dr sync [--clean] [--dry-run]
  dr latest [--clean] [--dry-run]
  dr submodules [--dry-run]
  dr gitconfig [--global] [--dry-run]
  dr push [--dry-run]
  dr publish [--dry-run]

Examples:
  dr run          Configure, build, and run Debug
  dr run debug    Configure, build, and run Debug
  dr run release  Configure, build, and run Release
  dr run release -- --project run_tree/projects/pbr_robot --scenegraph uv_debug.scenegraph
  dr run release --project run_tree/projects/pbr_robot --scenegraph uv_debug.scenegraph
  dr sync         Pull the parent repo and recursively update submodules
  dr sync --clean Reset and clean submodule worktrees before pulling branch heads
  dr submodules   Show recursive submodule status and ahead/behind counts
  dr push         Push submodule branches first, then push this repo
"""


def parse_args(args: list[str]) -> SimpleNamespace:
    if not args or args[0] in {"-h", "--help", "help"}:
        print(help_text())
        raise SystemExit(0)

    command = args[0].lower()
    rest = args[1:]

    if command in {"sync", "latest"}:
        clean = False
        dry_run = False
        for arg in rest:
            if arg == "--clean":
                clean = True
            elif arg == "--dry-run":
                dry_run = True
            else:
                print(f"Unexpected argument: {arg}\n")
                print(help_text())
                raise SystemExit(2)
        return SimpleNamespace(command="sync", clean=clean, dry_run=dry_run)

    if command in {"push", "publish"}:
        dry_run = False
        for arg in rest:
            if arg == "--dry-run":
                dry_run = True
            else:
                print(f"Unexpected argument: {arg}\n")
                print(help_text())
                raise SystemExit(2)
        return SimpleNamespace(command="push", dry_run=dry_run)

    if command in {"submodules", "status"}:
        dry_run = False
        for arg in rest:
            if arg == "--dry-run":
                dry_run = True
            else:
                print(f"Unexpected argument: {arg}\n")
                print(help_text())
                raise SystemExit(2)
        return SimpleNamespace(command="submodules", dry_run=dry_run)

    if command == "gitconfig":
        global_scope = False
        dry_run = False
        for arg in rest:
            if arg == "--global":
                global_scope = True
            elif arg == "--dry-run":
                dry_run = True
            else:
                print(f"Unexpected argument: {arg}\n")
                print(help_text())
                raise SystemExit(2)
        return SimpleNamespace(command="gitconfig", global_scope=global_scope, dry_run=dry_run)

    if command != "run":
        print(f"Unknown command: {args[0]}\n")
        print(help_text())
        raise SystemExit(2)

    config = "Debug"
    app_args: list[str] = []
    if rest:
        mode = rest[0].lower()
        if mode in CONFIGS:
            config = CONFIGS[mode]
            rest = rest[1:]
        elif rest[0] != "--":
            if rest[0].startswith("--"):
                app_args = rest
                rest = []
            else:
                print(f"Unknown build mode: {rest[0]}\n")
                print(help_text())
                raise SystemExit(2)

    if rest:
        if rest[0] == "--":
            app_args = rest[1:]
        elif rest[0].startswith("--"):
            app_args = rest
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


def configure_command(root: pathlib.Path, config: str) -> list[str]:
    if sys.platform.startswith("win"):
        return ["cmd", "/c", str(root / "config.bat")]
    return ["bash", str(root / "config.sh"), config]


def build_command(root: pathlib.Path, config: str) -> list[str]:
    if sys.platform.startswith("win"):
        return ["cmd", "/c", str(root / "build.bat"), config]
    return ["bash", str(root / "build.sh"), config]


def command_text(command: list[str]) -> str:
    return " ".join(shlex.quote(str(part)) for part in command)


def shell_lines(script: str) -> str:
    return "\n".join(line.rstrip() for line in script.splitlines() if line.strip())


def git_foreach(script: str) -> list[str]:
    return ["git", "submodule", "foreach", "--recursive", shell_lines(script)]


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


def configure_git_defaults(root: pathlib.Path, global_scope: bool = False, dry_run: bool = False) -> int:
    scope = "--global" if global_scope else "--local"
    commands = [["git", "config", scope, key, value] for key, value in GIT_DEFAULTS]
    return run_commands(commands, root, dry_run=dry_run)


def sync_repo(root: pathlib.Path, clean: bool = False, dry_run: bool = False) -> int:
    rc = configure_git_defaults(root, dry_run=dry_run)
    if rc != 0:
        return rc

    commands = [
        ["git", "fetch", "--recurse-submodules=on-demand", "--prune"],
        ["git", "pull", "--ff-only", "--recurse-submodules=on-demand"],
        ["git", "submodule", "sync", "--recursive"],
        ["git", "submodule", "update", "--init", "--recursive", "--jobs", "8"],
    ]
    if clean:
        commands.append(git_foreach(SUBMODULE_CLEAN_SCRIPT))
    commands.extend(
        [
            git_foreach(SUBMODULE_BRANCH_SYNC_SCRIPT),
            ["git", "submodule", "status", "--recursive"],
        ]
    )
    return run_commands(commands, root, dry_run=dry_run)


def show_submodules(root: pathlib.Path, dry_run: bool = False) -> int:
    commands = [
        ["git", "status", "--short", "--branch"],
        ["git", "submodule", "status", "--recursive"],
        git_foreach(SUBMODULE_STATUS_SCRIPT),
    ]
    return run_commands(commands, root, dry_run=dry_run)


def push_all(root: pathlib.Path, dry_run: bool = False) -> int:
    commands = [
        ["git", "submodule", "status", "--recursive"],
        git_foreach(SUBMODULE_PUSH_SCRIPT),
        ["git", "push", "--recurse-submodules=check"],
    ]
    return run_commands(commands, root, dry_run=dry_run)


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
    root = repo_root()
    if parsed.command == "run":
        return run_project(root, parsed.config, parsed.app_args)
    if parsed.command == "sync":
        return sync_repo(root, clean=parsed.clean, dry_run=parsed.dry_run)
    if parsed.command == "submodules":
        return show_submodules(root, dry_run=parsed.dry_run)
    if parsed.command == "gitconfig":
        return configure_git_defaults(root, global_scope=parsed.global_scope, dry_run=parsed.dry_run)
    if parsed.command == "push":
        return push_all(root, dry_run=parsed.dry_run)
    print(f"Unknown command: {parsed.command}", file=sys.stderr)
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
