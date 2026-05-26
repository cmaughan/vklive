#!/usr/bin/env python3
from __future__ import annotations

import json
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

REVIEW_MODES = {
    "all": "All",
    "codex": "Codex",
    "agy": "Agy",
    "gemini": "Agy",
    "claude": "Claude",
}

VCPKG_REPOSITORY = "https://github.com/microsoft/vcpkg.git"
VCPKG_BASELINE = "38d91be5efb2f21fbef4a3c53295002823747431"
CONFIGURE_STAMP_NAME = ".do-configure.json"
CONFIGURE_STAMP_VERSION = 1
_BUILD_ENVIRONMENT: dict[str, str] | None = None


def repo_root() -> pathlib.Path:
    return pathlib.Path(__file__).resolve().parent


def help_text() -> str:
    return """Usage:
  python3 do.py doctor
  python3 do.py setup
  python3 do.py config [debug|release|relwithdebinfo] [-- cmake-args...]
  python3 do.py build [debug|release|relwithdebinfo]
  python3 do.py test [debug|release|relwithdebinfo] [-- ctest-args...]
  python3 do.py run [debug|release|relwithdebinfo] [-- app-args...]
  python3 do.py review [all|codex|agy|gemini|claude] [--agy-timeout seconds] [--dry-run]
  python3 do.py consensus [--dry-run]
  python3 do.py sync
  python3 do.py push
  python3 do.py clean [debug|release|relwithdebinfo]

Examples:
  python3 do.py doctor       Check CMake, Ninja, compiler cache, and vcpkg discovery
  python3 do.py setup        Bootstrap an ignored local vcpkg checkout if needed
  python3 do.py config       Configure Debug with Ninja and expose compile_commands.json
  python3 do.py build        Build Debug without reconfiguring
  python3 do.py run release -- --project run_tree/projects/pbr_robot --scenegraph uv_debug.scenegraph
  python3 do.py test debug -- -R zep
  python3 do.py review codex
  python3 do.py consensus

If your shell aliases `dr` to `python3 do.py`, `dr build`, `dr run`, and the
other short forms are equivalent.
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

    if command == "review":
        return parse_review_args(rest)

    if command == "consensus":
        dry_run = False
        for arg in rest:
            if arg == "--dry-run":
                dry_run = True
            else:
                _unexpected(arg)
        return SimpleNamespace(command=command, dry_run=dry_run)

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


def parse_review_args(args: list[str]) -> SimpleNamespace:
    review_target = "all"
    agy_timeout_seconds = 900
    dry_run = False
    index = 0

    while index < len(args):
        arg = args[index]
        lowered = arg.lower()
        if lowered in REVIEW_MODES:
            if review_target != "all":
                _unexpected(arg)
            review_target = lowered
            index += 1
            continue
        if arg == "--dry-run":
            dry_run = True
            index += 1
            continue
        if arg in {"--agy-timeout", "--agy-timeout-seconds"}:
            if index + 1 >= len(args):
                _unexpected(arg)
            try:
                agy_timeout_seconds = int(args[index + 1])
            except ValueError:
                _unexpected(args[index + 1])
            index += 2
            continue
        for prefix in ("--agy-timeout=", "--agy-timeout-seconds="):
            if arg.startswith(prefix):
                try:
                    agy_timeout_seconds = int(arg[len(prefix) :])
                except ValueError:
                    _unexpected(arg)
                index += 1
                break
        else:
            _unexpected(arg)
            index += 1

    if agy_timeout_seconds <= 0:
        print("agy timeout must be positive\n")
        print(help_text())
        raise SystemExit(2)

    return SimpleNamespace(
        command="review",
        review_target=review_target,
        agy_timeout_seconds=agy_timeout_seconds,
        dry_run=dry_run,
    )


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


def configure_stamp(root: pathlib.Path, config: str) -> pathlib.Path:
    return build_dir(root, config) / CONFIGURE_STAMP_NAME


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


def vcpkg_executable_name() -> str:
    return "vcpkg.exe" if sys.platform.startswith("win") else "vcpkg"


def vcpkg_executable(root: pathlib.Path) -> pathlib.Path | None:
    executable_name = vcpkg_executable_name()
    for candidate in vcpkg_roots(root):
        executable = candidate / executable_name
        if executable.exists():
            return executable
    return None


def vcpkg_toolchain(root: pathlib.Path) -> pathlib.Path | None:
    for candidate in vcpkg_roots(root):
        toolchain = candidate / "scripts" / "buildsystems" / "vcpkg.cmake"
        if toolchain.exists():
            return toolchain
    return None


def vcpkg_root_from_toolchain(toolchain: pathlib.Path) -> pathlib.Path:
    return toolchain.parents[2]


def bootstrap_command(checkout: pathlib.Path) -> list[str]:
    if sys.platform.startswith("win"):
        return ["cmd", "/c", str(checkout / "bootstrap-vcpkg.bat"), "-disableMetrics"]
    return [str(checkout / "bootstrap-vcpkg.sh"), "-disableMetrics"]


def cmake_cache_value(root: pathlib.Path, config: str, name: str) -> str | None:
    cache = build_dir(root, config) / "CMakeCache.txt"
    if not cache.exists():
        return None

    for line in cache.read_text(encoding="utf-8", errors="ignore").splitlines():
        key, separator, value = line.partition("=")
        if separator and key.split(":", 1)[0] == name:
            return value
    return None


def configured_toolchain(root: pathlib.Path, config: str) -> str | None:
    return cmake_cache_value(root, config, "CMAKE_TOOLCHAIN_FILE")


def normalized_path_text(path: str | pathlib.Path | None) -> str | None:
    if path is None:
        return None
    return os.path.normcase(os.path.abspath(str(path)))


def same_path(left: str | None, right: pathlib.Path) -> bool:
    if left is None:
        return False
    return normalized_path_text(left) == normalized_path_text(right)


def configure_stamp_state(root: pathlib.Path, config: str) -> dict[str, str | int | None]:
    return {
        "version": CONFIGURE_STAMP_VERSION,
        "config": config,
        "generator": cmake_cache_value(root, config, "CMAKE_GENERATOR"),
        "triplet": cmake_cache_value(root, config, "VCPKG_TARGET_TRIPLET"),
        "manifest_features": cmake_cache_value(root, config, "VCPKG_MANIFEST_FEATURES"),
        "toolchain": normalized_path_text(configured_toolchain(root, config)),
    }


def read_configure_stamp(root: pathlib.Path, config: str) -> dict[str, object] | None:
    stamp = configure_stamp(root, config)
    if not stamp.exists():
        return None

    try:
        state = json.loads(stamp.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return None

    if isinstance(state, dict):
        return state
    return None


def configure_stamp_matches(root: pathlib.Path, config: str) -> bool:
    state = read_configure_stamp(root, config)
    if state is None:
        return False
    if state.get("version") != CONFIGURE_STAMP_VERSION:
        return False
    if state.get("config") != config:
        return False
    if state.get("generator") != "Ninja":
        return False
    if state.get("triplet") != triplet():
        return False
    if state.get("manifest_features") != default_vcpkg_manifest_features():
        return False

    toolchain = vcpkg_toolchain(root)
    stamped_toolchain = state.get("toolchain")
    if toolchain is not None:
        return isinstance(stamped_toolchain, str) and same_path(stamped_toolchain, toolchain)

    return stamped_toolchain in {None, ""}


def configure_marker_time(root: pathlib.Path, config: str) -> float:
    directory = build_dir(root, config)
    marker_time = 0.0

    build_file = directory / "build.ninja"
    if build_file.exists():
        marker_time = max(marker_time, build_file.stat().st_mtime)

    stamp = configure_stamp(root, config)
    if stamp.exists() and configure_stamp_matches(root, config):
        marker_time = max(marker_time, stamp.stat().st_mtime)

    return marker_time


def write_configure_stamp(root: pathlib.Path, config: str) -> None:
    stamp = configure_stamp(root, config)
    stamp.write_text(json.dumps(configure_stamp_state(root, config), indent=2, sort_keys=True) + "\n", encoding="utf-8")


def configure_inputs(root: pathlib.Path) -> list[pathlib.Path]:
    return [
        root / "CMakeLists.txt",
        root / "CMakePresets.json",
        root / "vcpkg.json",
        root / "vcpkg-configuration.json",
    ]


def needs_configure(root: pathlib.Path, config: str, cmake_args: list[str] | None = None) -> bool:
    if cmake_args:
        return True

    directory = build_dir(root, config)
    cache = directory / "CMakeCache.txt"
    if not cache.exists():
        return True
    if not (directory / "build.ninja").exists():
        return True

    if cmake_cache_value(root, config, "CMAKE_GENERATOR") != "Ninja":
        return True
    if cmake_cache_value(root, config, "VCPKG_TARGET_TRIPLET") != triplet():
        return True
    if cmake_cache_value(root, config, "VCPKG_MANIFEST_FEATURES") != default_vcpkg_manifest_features():
        return True

    toolchain = vcpkg_toolchain(root)
    if toolchain is not None and not same_path(configured_toolchain(root, config), toolchain):
        return True

    marker_time = configure_marker_time(root, config)
    if marker_time <= 0:
        return True

    for input_file in configure_inputs(root):
        if input_file.exists() and input_file.stat().st_mtime > marker_time:
            return True

    return False


def configure_command(root: pathlib.Path, config: str, cmake_args: list[str] | None = None) -> list[str]:
    cmake_args = cmake_args or []
    command = [
        "cmake",
        "--preset",
        preset_name(config),
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        f"-DVCPKG_TARGET_TRIPLET={triplet()}",
    ]
    if not has_cmake_define(cmake_args, "VCPKG_MANIFEST_FEATURES"):
        command.append(f"-DVCPKG_MANIFEST_FEATURES={default_vcpkg_manifest_features()}")
    toolchain = vcpkg_toolchain(root)
    if toolchain is not None and not same_path(configured_toolchain(root, config), toolchain):
        command.append(f"-DCMAKE_TOOLCHAIN_FILE={toolchain}")
    command.extend(cmake_args)
    return command


def default_vcpkg_manifest_features() -> str:
    return "metal" if sys.platform == "darwin" else "vulkan"


def has_cmake_define(args: list[str], name: str) -> bool:
    prefix = f"-D{name}="
    return any(arg == f"-D{name}" or arg.startswith(prefix) for arg in args)


def build_command(root: pathlib.Path, config: str) -> list[str]:
    return ["cmake", "--build", str(build_dir(root, config)), "--config", config, "--parallel"]


def test_command(root: pathlib.Path, config: str, ctest_args: list[str]) -> list[str]:
    return ["ctest", "--test-dir", str(build_dir(root, config)), "--output-on-failure", *ctest_args]


def find_vcvars64() -> pathlib.Path | None:
    if not sys.platform.startswith("win"):
        return None

    program_files_x86 = os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")
    vswhere = pathlib.Path(program_files_x86) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    if vswhere.exists():
        result = subprocess.run(
            [
                str(vswhere),
                "-latest",
                "-products",
                "*",
                "-requires",
                "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                "-property",
                "installationPath",
            ],
            capture_output=True,
            text=True,
            check=False,
        )
        if result.returncode == 0:
            install_path = result.stdout.strip().splitlines()
            if install_path:
                candidate = pathlib.Path(install_path[0]) / "VC" / "Auxiliary" / "Build" / "vcvars64.bat"
                if candidate.exists():
                    return candidate

    program_files = pathlib.Path(os.environ.get("ProgramFiles", r"C:\Program Files"))
    for edition in ("Community", "Professional", "Enterprise", "BuildTools", "Preview"):
        candidate = program_files / "Microsoft Visual Studio" / "2022" / edition / "VC" / "Auxiliary" / "Build" / "vcvars64.bat"
        if candidate.exists():
            return candidate

    return None


def build_environment() -> dict[str, str] | None:
    global _BUILD_ENVIRONMENT

    if not sys.platform.startswith("win"):
        return None
    if _BUILD_ENVIRONMENT is not None:
        return _BUILD_ENVIRONMENT

    vcvars = find_vcvars64()
    if vcvars is None:
        return None

    result = subprocess.run(
        ["cmd", "/d", "/c", "call", str(vcvars), ">nul", "&&", "set"],
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        return None

    environment = os.environ.copy()
    for line in result.stdout.splitlines():
        key, separator, value = line.partition("=")
        if separator:
            environment[key] = value

    _BUILD_ENVIRONMENT = environment
    return _BUILD_ENVIRONMENT


def command_text(command: list[str]) -> str:
    return " ".join(shlex.quote(str(part)) for part in command)


def powershell_quote(value: pathlib.Path | str) -> str:
    text = str(value)
    return "'" + text.replace("'", "''") + "'"


def create_review_plan(root: pathlib.Path, agy_timeout_seconds: int = 900, review_target: str = "all") -> SimpleNamespace:
    if agy_timeout_seconds <= 0:
        raise ValueError("agy_timeout_seconds must be positive")

    normalized_target = review_target.lower()
    if normalized_target not in REVIEW_MODES:
        raise ValueError(f"Unsupported review target: {review_target}")
    mode = REVIEW_MODES[normalized_target]

    reviews_dir = root / "plans" / "reviews"
    review_prompt_path = root / "plans" / "prompts" / "review.md"
    consensus_prompt_path = root / "plans" / "prompts" / "consensus_review.md"
    codex_review_path = reviews_dir / "review-codex.md"
    gemini_review_path = reviews_dir / "review-gemini.md"
    claude_review_path = reviews_dir / "review-claude.md"
    consensus_review_path = reviews_dir / "review-consensus.md"
    review_script_path = root / "scripts" / "Run-Review.ps1"

    repo_root_ps = powershell_quote(root)
    review_script_path_ps = powershell_quote(review_script_path)
    powershell_script = f"""
$ErrorActionPreference = "Stop"
$repoRoot = {repo_root_ps}
$reviewScriptPath = {review_script_path_ps}

if (-not (Test-Path -LiteralPath $reviewScriptPath)) {{
    throw "Review script not found: $reviewScriptPath"
}}

& $reviewScriptPath -RepoRoot $repoRoot -AgyTimeoutSeconds {agy_timeout_seconds} -Mode {mode}
exit $LASTEXITCODE
""".strip()

    return SimpleNamespace(
        repo_root=root,
        reviews_dir=reviews_dir,
        review_prompt_path=review_prompt_path,
        consensus_prompt_path=consensus_prompt_path,
        codex_review_path=codex_review_path,
        gemini_review_path=gemini_review_path,
        claude_review_path=claude_review_path,
        consensus_review_path=consensus_review_path,
        review_script_path=review_script_path,
        mode=mode,
        agy_timeout_seconds=agy_timeout_seconds,
        powershell_script=powershell_script,
    )


def create_consensus_plan(root: pathlib.Path) -> SimpleNamespace:
    plan = create_review_plan(root)
    repo_root_ps = powershell_quote(root)
    review_script_path_ps = powershell_quote(plan.review_script_path)
    powershell_script = f"""
$ErrorActionPreference = "Stop"
$repoRoot = {repo_root_ps}
$reviewScriptPath = {review_script_path_ps}

if (-not (Test-Path -LiteralPath $reviewScriptPath)) {{
    throw "Review script not found: $reviewScriptPath"
}}

& $reviewScriptPath -RepoRoot $repoRoot -Mode Consensus
exit $LASTEXITCODE
""".strip()

    return SimpleNamespace(
        repo_root=plan.repo_root,
        reviews_dir=plan.reviews_dir,
        review_prompt_path=plan.review_prompt_path,
        consensus_prompt_path=plan.consensus_prompt_path,
        codex_review_path=plan.codex_review_path,
        gemini_review_path=plan.gemini_review_path,
        claude_review_path=plan.claude_review_path,
        consensus_review_path=plan.consensus_review_path,
        review_script_path=plan.review_script_path,
        mode="Consensus",
        agy_timeout_seconds=plan.agy_timeout_seconds,
        powershell_script=powershell_script,
    )


def run(command: list[str], cwd: pathlib.Path, dry_run: bool = False, env: dict[str, str] | None = None) -> int:
    print("> " + command_text(command))
    if dry_run:
        return 0
    return subprocess.run(command, cwd=cwd, check=False, env=env).returncode


def run_commands(commands: list[list[str]], cwd: pathlib.Path, dry_run: bool = False) -> int:
    for command in commands:
        rc = run(command, cwd, dry_run=dry_run)
        if rc != 0:
            return rc
    return 0


def run_powershell_script(root: pathlib.Path, powershell_script: str, dry_run: bool = False) -> int:
    powershell = shutil.which("pwsh") or shutil.which("powershell")
    if powershell is None:
        print("Could not find pwsh or powershell on PATH.", file=sys.stderr)
        return 1

    command = [
        powershell,
        "-NoProfile",
        "-ExecutionPolicy",
        "Bypass",
        "-Command",
        powershell_script,
    ]
    print("> " + command_text(command))
    if dry_run:
        return 0
    return subprocess.run(command, cwd=root, check=False).returncode


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
    rc = run(configure_command(root, config, cmake_args), root, env=build_environment())
    if rc == 0:
        write_configure_stamp(root, config)
        expose_compile_commands(root, build_dir(root, config))
    return rc


def build(root: pathlib.Path, config: str) -> int:
    return run(build_command(root, config), root, env=build_environment())


def test(root: pathlib.Path, config: str, ctest_args: list[str]) -> int:
    return run(test_command(root, config, ctest_args), root, env=build_environment())


def run_review(plan: SimpleNamespace, dry_run: bool) -> int:
    return run_powershell_script(plan.repo_root, plan.powershell_script, dry_run)


def run_project(root: pathlib.Path, config: str, app_args: list[str]) -> int:
    if needs_configure(root, config):
        rc = configure(root, config)
        if rc != 0:
            return rc
    else:
        expose_compile_commands(root, build_dir(root, config))

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
    executable = vcpkg_executable(root)
    if toolchain:
        print(f"Using vcpkg toolchain: {toolchain}")
        if not executable:
            print("vcpkg checkout is present but not bootstrapped. Run `python3 do.py setup`.", file=sys.stderr)
            return 1
    else:
        print("No vcpkg toolchain found. Run `python3 do.py setup` or set VCPKG_ROOT.", file=sys.stderr)
        return 1

    return 0


def setup(root: pathlib.Path) -> int:
    toolchain = vcpkg_toolchain(root)
    executable = vcpkg_executable(root)
    if toolchain is not None and executable is not None:
        print(f"vcpkg already available: {toolchain}")
        return 0

    if toolchain is not None:
        checkout = vcpkg_root_from_toolchain(toolchain)
        print(f"Bootstrapping existing vcpkg checkout: {checkout}")
        return run(bootstrap_command(checkout), checkout)

    checkout = root / ".cache" / "vcpkg"
    checkout.parent.mkdir(parents=True, exist_ok=True)
    rc = run(["git", "clone", VCPKG_REPOSITORY, str(checkout)], root)
    if rc != 0:
        return rc

    rc = run(["git", "checkout", VCPKG_BASELINE], checkout)
    if rc != 0:
        return rc

    return run(bootstrap_command(checkout), checkout)


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


def main(argv: list[str] | None = None) -> int:
    parsed = parse_args(sys.argv[1:] if argv is None else argv)
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
    if parsed.command == "review":
        return run_review(create_review_plan(root, parsed.agy_timeout_seconds, parsed.review_target), parsed.dry_run)
    if parsed.command == "consensus":
        return run_review(create_consensus_plan(root), parsed.dry_run)
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
