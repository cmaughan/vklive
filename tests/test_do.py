from __future__ import annotations

import importlib.util
import json
import os
import pathlib
import subprocess
import tempfile
import unittest
from unittest import mock


ROOT = pathlib.Path(__file__).resolve().parents[1]


def load_do_module():
    spec = importlib.util.spec_from_file_location("do", ROOT / "do.py")
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


class DoScriptTests(unittest.TestCase):
    def test_run_defaults_to_debug(self):
        do = load_do_module()

        parsed = do.parse_args(["run"])

        self.assertEqual(parsed.command, "run")
        self.assertEqual(parsed.config, "Debug")
        self.assertEqual(parsed.app_args, [])

    def test_run_accepts_release(self):
        do = load_do_module()

        parsed = do.parse_args(["run", "release"])

        self.assertEqual(parsed.command, "run")
        self.assertEqual(parsed.config, "Release")

    def test_build_accepts_relwithdebinfo(self):
        do = load_do_module()

        parsed = do.parse_args(["build", "relwithdebinfo"])

        self.assertEqual(parsed.command, "build")
        self.assertEqual(parsed.config, "RelWithDebInfo")

    def test_unknown_command_is_rejected(self):
        do = load_do_module()

        with self.assertRaises(SystemExit) as cm:
            do.parse_args(["submodules"])

        self.assertEqual(cm.exception.code, 2)

    def test_run_forwards_project_and_scenegraph_args(self):
        do = load_do_module()

        parsed = do.parse_args(["run", "release", "--", "--project", "run_tree/projects/pbr_robot", "--scenegraph", "uv_debug.scenegraph"])

        self.assertEqual(parsed.config, "Release")
        self.assertEqual(parsed.app_args, ["--project", "run_tree/projects/pbr_robot", "--scenegraph", "uv_debug.scenegraph"])
        self.assertIn("compile_commands.json", do.help_text())

    def test_run_forwards_app_args_when_wrapper_strips_separator(self):
        do = load_do_module()

        parsed = do.parse_args(["run", "release", "--project", "run_tree/projects/pbr_robot", "--scenegraph", "uv_debug.scenegraph"])

        self.assertEqual(parsed.config, "Release")
        self.assertEqual(parsed.app_args, ["--project", "run_tree/projects/pbr_robot", "--scenegraph", "uv_debug.scenegraph"])

    def test_config_uses_cmake_preset_and_toolchain(self):
        do = load_do_module()
        calls: list[list[str]] = []

        def fake_run(command, **kwargs):
            calls.append([str(part) for part in command])
            return mock.Mock(returncode=0)

        with tempfile.TemporaryDirectory() as temp_dir:
            root = pathlib.Path(temp_dir)
            vcpkg = root / "vcpkg"
            toolchain = vcpkg / "scripts" / "buildsystems" / "vcpkg.cmake"
            toolchain.parent.mkdir(parents=True)
            toolchain.write_text("# test toolchain\n", encoding="utf-8")
            build = root / "build" / "debug"
            build.mkdir(parents=True)
            (build / "compile_commands.json").write_text("[]\n", encoding="utf-8")

            with mock.patch.object(do, "build_environment", return_value=None):
                with mock.patch.object(do.subprocess, "run", side_effect=fake_run):
                    rc = do.configure(root, "Debug")

        self.assertEqual(rc, 0)
        self.assertEqual(calls[0][0:3], ["cmake", "--preset", "debug"])
        self.assertIn(f"-DCMAKE_TOOLCHAIN_FILE={toolchain}", calls[0])
        self.assertIn("-DCMAKE_EXPORT_COMPILE_COMMANDS=ON", calls[0])

    def test_config_skips_toolchain_when_build_tree_already_has_it(self):
        do = load_do_module()

        with tempfile.TemporaryDirectory() as temp_dir:
            root = pathlib.Path(temp_dir)
            toolchain = root / ".cache" / "vcpkg" / "scripts" / "buildsystems" / "vcpkg.cmake"
            toolchain.parent.mkdir(parents=True)
            toolchain.write_text("# test toolchain\n", encoding="utf-8")
            build = root / "build" / "debug"
            build.mkdir(parents=True)
            (build / "CMakeCache.txt").write_text(f"CMAKE_TOOLCHAIN_FILE:FILEPATH={toolchain}\n", encoding="utf-8")

            command = do.configure_command(root, "Debug")

        self.assertNotIn(f"-DCMAKE_TOOLCHAIN_FILE={toolchain}", command)

    def test_config_selects_metal_manifest_feature_on_mac(self):
        do = load_do_module()

        with mock.patch.object(do.sys, "platform", "darwin"):
            command = do.configure_command(ROOT, "Debug")

        self.assertIn("-DVCPKG_MANIFEST_FEATURES=metal", command)

    def test_config_selects_vulkan_manifest_feature_off_mac(self):
        do = load_do_module()

        with mock.patch.object(do.sys, "platform", "linux"):
            command = do.configure_command(ROOT, "Debug")

        self.assertIn("-DVCPKG_MANIFEST_FEATURES=vulkan", command)

    def test_config_respects_explicit_manifest_features(self):
        do = load_do_module()

        with mock.patch.object(do.sys, "platform", "darwin"):
            command = do.configure_command(ROOT, "Debug", ["-DVCPKG_MANIFEST_FEATURES=vulkan"])

        self.assertIn("-DVCPKG_MANIFEST_FEATURES=vulkan", command)
        self.assertNotIn("-DVCPKG_MANIFEST_FEATURES=metal", command)

    def test_config_forwards_explicit_cmake_args_after_separator(self):
        do = load_do_module()

        parsed = do.parse_args(["config", "debug", "--", "-DVKLIVE_ENABLE_METAL=ON", "-DVKLIVE_ENABLE_VULKAN=OFF"])

        self.assertEqual(parsed.command, "config")
        self.assertEqual(parsed.config, "Debug")
        self.assertEqual(parsed.cmake_args, ["-DVKLIVE_ENABLE_METAL=ON", "-DVKLIVE_ENABLE_VULKAN=OFF"])

    def test_config_exposes_compile_commands_at_repo_root(self):
        do = load_do_module()

        with tempfile.TemporaryDirectory() as temp_dir:
            root = pathlib.Path(temp_dir)
            build = root / "build" / "debug"
            build.mkdir(parents=True)
            (build / "compile_commands.json").write_text("[]\n", encoding="utf-8")

            do.expose_compile_commands(root, build)

            exposed = root / "compile_commands.json"
            self.assertTrue(exposed.exists())
            self.assertEqual(exposed.read_text(encoding="utf-8"), "[]\n")

    def test_build_uses_existing_configured_directory(self):
        do = load_do_module()
        calls: list[list[str]] = []

        def fake_run(command, **kwargs):
            calls.append([str(part) for part in command])
            return mock.Mock(returncode=0)

        with mock.patch.object(do, "build_environment", return_value=None):
            with mock.patch.object(do.subprocess, "run", side_effect=fake_run):
                rc = do.build(ROOT, "Debug")

        self.assertEqual(rc, 0)
        self.assertEqual(calls, [["cmake", "--build", str(ROOT / "build" / "debug"), "--config", "Debug", "--parallel"]])

    def test_config_and_build_use_msvc_environment_on_windows(self):
        do = load_do_module()
        env = {"PATH": "msvc-tools"}
        calls: list[tuple[list[str], dict[str, str] | None]] = []

        def fake_run(command, **kwargs):
            calls.append(([str(part) for part in command], kwargs.get("env")))
            return mock.Mock(returncode=0)

        with tempfile.TemporaryDirectory() as temp_dir:
            root = pathlib.Path(temp_dir)
            build = root / "build" / "debug"
            build.mkdir(parents=True)

            with mock.patch.object(do, "build_environment", return_value=env):
                with mock.patch.object(do.subprocess, "run", side_effect=fake_run):
                    config_rc = do.configure(root, "Debug")
                    build_rc = do.build(root, "Debug")

        self.assertEqual(config_rc, 0)
        self.assertEqual(build_rc, 0)
        self.assertEqual(calls[0][1], env)
        self.assertEqual(calls[1][1], env)

    def test_build_environment_loads_vcvars_on_windows(self):
        do = load_do_module()
        vcvars = ROOT / "VC Vars" / "vcvars64.bat"
        calls: list[list[str]] = []

        def fake_run(command, **kwargs):
            calls.append([str(part) for part in command])
            return mock.Mock(returncode=0, stdout="PATH=C:\\msvc\nINCLUDE=C:\\include\n")

        with mock.patch.object(do.sys, "platform", "win32"):
            with mock.patch.object(do, "find_vcvars64", return_value=vcvars):
                with mock.patch.object(do.subprocess, "run", side_effect=fake_run):
                    with mock.patch.dict(do.os.environ, {"PATH": "original"}, clear=True):
                        env = do.build_environment()

        self.assertIsNotNone(env)
        self.assertEqual(env["PATH"], "C:\\msvc")
        self.assertEqual(env["INCLUDE"], "C:\\include")
        self.assertEqual(calls, [["cmd", "/d", "/c", "call", str(vcvars), ">nul", "&&", "set"]])

    def test_run_configures_builds_then_launches_exe(self):
        do = load_do_module()
        calls: list[list[str]] = []

        def fake_run(command, **kwargs):
            calls.append([str(part) for part in command])
            return mock.Mock(returncode=0)

        with mock.patch.object(do, "configure", return_value=0):
            with mock.patch.object(do, "build", return_value=0):
                with mock.patch.object(do.sys, "platform", "linux"):
                    with mock.patch.object(do.subprocess, "run", side_effect=fake_run):
                        with mock.patch.object(pathlib.Path, "exists", return_value=True):
                            rc = do.run_project(ROOT, "Release", ["--smoke-test"])

        self.assertEqual(rc, 0)
        self.assertEqual(calls, [[str(ROOT / "build" / "release" / "Rezonality"), "--smoke-test"]])

    def test_run_skips_configure_when_build_tree_is_current(self):
        do = load_do_module()
        build_calls: list[str] = []
        launch_calls: list[list[str]] = []

        def fake_launch(command, **kwargs):
            launch_calls.append([str(part) for part in command])
            return mock.Mock(returncode=0)

        with tempfile.TemporaryDirectory() as temp_dir:
            root = pathlib.Path(temp_dir)
            toolchain = root / ".cache" / "vcpkg" / "scripts" / "buildsystems" / "vcpkg.cmake"
            toolchain.parent.mkdir(parents=True)
            toolchain.write_text("# test toolchain\n", encoding="utf-8")
            build = root / "build" / "release"
            build.mkdir(parents=True)
            exe = build / "Rezonality.exe"
            exe.write_text("", encoding="utf-8")
            (build / "build.ninja").write_text("# ninja\n", encoding="utf-8")
            (build / "CMakeCache.txt").write_text(
                "\n".join(
                    [
                        "CMAKE_GENERATOR:INTERNAL=Ninja",
                        f"CMAKE_TOOLCHAIN_FILE:FILEPATH={toolchain}",
                        f"VCPKG_TARGET_TRIPLET:STRING={do.triplet()}",
                        f"VCPKG_MANIFEST_FEATURES:UNINITIALIZED={do.default_vcpkg_manifest_features()}",
                        "",
                    ]
                ),
                encoding="utf-8",
            )

            with mock.patch.object(do, "configure", side_effect=AssertionError("configure should not run")):
                with mock.patch.object(do, "build", side_effect=lambda _root, _config: build_calls.append(_config) or 0):
                    with mock.patch.object(do.subprocess, "run", side_effect=fake_launch):
                        rc = do.run_project(root, "Release", ["--smoke-test"])

        self.assertEqual(rc, 0)
        self.assertEqual(build_calls, ["Release"])
        self.assertEqual(launch_calls, [[str(exe), "--smoke-test"]])

    def test_run_configures_when_vcpkg_manifest_is_newer_than_cache(self):
        do = load_do_module()

        with tempfile.TemporaryDirectory() as temp_dir:
            root = pathlib.Path(temp_dir)
            toolchain = root / ".cache" / "vcpkg" / "scripts" / "buildsystems" / "vcpkg.cmake"
            toolchain.parent.mkdir(parents=True)
            toolchain.write_text("# test toolchain\n", encoding="utf-8")
            build = root / "build" / "release"
            build.mkdir(parents=True)
            cache = build / "CMakeCache.txt"
            cache.write_text(
                "\n".join(
                    [
                        "CMAKE_GENERATOR:INTERNAL=Ninja",
                        f"CMAKE_TOOLCHAIN_FILE:FILEPATH={toolchain}",
                        f"VCPKG_TARGET_TRIPLET:STRING={do.triplet()}",
                        f"VCPKG_MANIFEST_FEATURES:UNINITIALIZED={do.default_vcpkg_manifest_features()}",
                        "",
                    ]
                ),
                encoding="utf-8",
            )
            (build / "build.ninja").write_text("# ninja\n", encoding="utf-8")
            manifest = root / "vcpkg.json"
            manifest.write_text("{}\n", encoding="utf-8")
            os.utime(cache, (1, 1))
            os.utime(build / "build.ninja", (1, 1))
            os.utime(manifest, (2, 2))

            self.assertTrue(do.needs_configure(root, "Release"))

    def test_run_skips_configure_when_configure_stamp_is_current(self):
        do = load_do_module()

        with tempfile.TemporaryDirectory() as temp_dir:
            root = pathlib.Path(temp_dir)
            toolchain = root / ".cache" / "vcpkg" / "scripts" / "buildsystems" / "vcpkg.cmake"
            toolchain.parent.mkdir(parents=True)
            toolchain.write_text("# test toolchain\n", encoding="utf-8")
            build = root / "build" / "release"
            build.mkdir(parents=True)
            cache = build / "CMakeCache.txt"
            cache.write_text(
                "\n".join(
                    [
                        "CMAKE_GENERATOR:INTERNAL=Ninja",
                        f"CMAKE_TOOLCHAIN_FILE:FILEPATH={toolchain}",
                        f"VCPKG_TARGET_TRIPLET:STRING={do.triplet()}",
                        f"VCPKG_MANIFEST_FEATURES:UNINITIALIZED={do.default_vcpkg_manifest_features()}",
                        "",
                    ]
                ),
                encoding="utf-8",
            )
            (build / "build.ninja").write_text("# ninja\n", encoding="utf-8")
            (build / ".do-configure.json").write_text(
                json.dumps(
                    {
                        "version": 1,
                        "config": "Release",
                        "generator": "Ninja",
                        "triplet": do.triplet(),
                        "manifest_features": do.default_vcpkg_manifest_features(),
                        "toolchain": str(toolchain),
                    }
                )
                + "\n",
                encoding="utf-8",
            )
            manifest = root / "vcpkg.json"
            manifest.write_text("{}\n", encoding="utf-8")
            os.utime(cache, (1, 1))
            os.utime(build / "build.ninja", (1, 1))
            os.utime(manifest, (2, 2))
            os.utime(build / ".do-configure.json", (3, 3))

            self.assertFalse(do.needs_configure(root, "Release"))

    def test_run_skips_configure_when_build_file_is_newer_than_cache(self):
        do = load_do_module()

        with tempfile.TemporaryDirectory() as temp_dir:
            root = pathlib.Path(temp_dir)
            toolchain = root / ".cache" / "vcpkg" / "scripts" / "buildsystems" / "vcpkg.cmake"
            toolchain.parent.mkdir(parents=True)
            toolchain.write_text("# test toolchain\n", encoding="utf-8")
            build = root / "build" / "release"
            build.mkdir(parents=True)
            cache = build / "CMakeCache.txt"
            cache.write_text(
                "\n".join(
                    [
                        "CMAKE_GENERATOR:INTERNAL=Ninja",
                        f"CMAKE_TOOLCHAIN_FILE:FILEPATH={toolchain}",
                        f"VCPKG_TARGET_TRIPLET:STRING={do.triplet()}",
                        f"VCPKG_MANIFEST_FEATURES:UNINITIALIZED={do.default_vcpkg_manifest_features()}",
                        "",
                    ]
                ),
                encoding="utf-8",
            )
            build_file = build / "build.ninja"
            build_file.write_text("# ninja\n", encoding="utf-8")
            manifest = root / "vcpkg.json"
            manifest.write_text("{}\n", encoding="utf-8")
            os.utime(cache, (1, 1))
            os.utime(manifest, (2, 2))
            os.utime(build_file, (3, 3))

            self.assertFalse(do.needs_configure(root, "Release"))

    def test_config_writes_configure_stamp_on_success(self):
        do = load_do_module()

        def fake_run(command, **kwargs):
            return mock.Mock(returncode=0)

        with tempfile.TemporaryDirectory() as temp_dir:
            root = pathlib.Path(temp_dir)
            build = root / "build" / "debug"
            build.mkdir(parents=True)
            (build / "CMakeCache.txt").write_text(
                "\n".join(
                    [
                        "CMAKE_GENERATOR:INTERNAL=Ninja",
                        f"VCPKG_TARGET_TRIPLET:STRING={do.triplet()}",
                        f"VCPKG_MANIFEST_FEATURES:UNINITIALIZED={do.default_vcpkg_manifest_features()}",
                        "",
                    ]
                ),
                encoding="utf-8",
            )
            (build / "build.ninja").write_text("# ninja\n", encoding="utf-8")

            with mock.patch.object(do, "build_environment", return_value=None):
                with mock.patch.object(do.subprocess, "run", side_effect=fake_run):
                    rc = do.configure(root, "Debug")

            self.assertEqual(rc, 0)
            stamp = build / ".do-configure.json"
            self.assertTrue(stamp.exists())
            state = json.loads(stamp.read_text(encoding="utf-8"))
            self.assertEqual(state["generator"], "Ninja")
            self.assertEqual(state["triplet"], do.triplet())

    def test_sync_is_root_repo_only(self):
        do = load_do_module()
        calls: list[list[str]] = []

        def fake_run(command, **kwargs):
            calls.append([str(part) for part in command])
            return mock.Mock(returncode=0)

        with mock.patch.object(do.subprocess, "run", side_effect=fake_run):
            rc = do.sync_repo(ROOT)

        self.assertEqual(rc, 0)
        self.assertEqual(calls, [["git", "fetch", "--prune"], ["git", "pull", "--ff-only"]])

    def test_doctor_checks_required_tools(self):
        do = load_do_module()
        calls: list[list[str]] = []

        def fake_run(command, **kwargs):
            calls.append([str(part) for part in command])
            return mock.Mock(returncode=0)

        with mock.patch.object(do.subprocess, "run", side_effect=fake_run):
            rc = do.doctor(ROOT)

        self.assertEqual(rc, 0)
        self.assertIn(["cmake", "--version"], calls)
        self.assertIn(["ninja", "--version"], calls)

    def test_setup_bootstraps_vcpkg_through_cmd_on_windows(self):
        do = load_do_module()
        calls: list[list[str]] = []

        def fake_run(command, **kwargs):
            calls.append([str(part) for part in command])
            return mock.Mock(returncode=0)

        with tempfile.TemporaryDirectory() as temp_dir:
            root = pathlib.Path(temp_dir)
            checkout = root / ".cache" / "vcpkg"

            with mock.patch.object(do.sys, "platform", "win32"):
                with mock.patch.object(do.subprocess, "run", side_effect=fake_run):
                    rc = do.setup(root)

        self.assertEqual(rc, 0)
        self.assertEqual(calls[0], ["git", "clone", do.VCPKG_REPOSITORY, str(checkout)])
        self.assertEqual(calls[1], ["git", "checkout", do.VCPKG_BASELINE])
        self.assertEqual(calls[2], ["cmd", "/c", str(checkout / "bootstrap-vcpkg.bat"), "-disableMetrics"])

    def test_setup_bootstraps_existing_checkout_without_vcpkg_exe(self):
        do = load_do_module()
        calls: list[list[str]] = []

        def fake_run(command, **kwargs):
            calls.append([str(part) for part in command])
            return mock.Mock(returncode=0)

        with tempfile.TemporaryDirectory() as temp_dir:
            root = pathlib.Path(temp_dir)
            checkout = root / ".cache" / "vcpkg"
            toolchain = checkout / "scripts" / "buildsystems" / "vcpkg.cmake"
            toolchain.parent.mkdir(parents=True)
            toolchain.write_text("# test toolchain\n", encoding="utf-8")

            with mock.patch.object(do.sys, "platform", "win32"):
                with mock.patch.object(do.subprocess, "run", side_effect=fake_run):
                    rc = do.setup(root)

        self.assertEqual(rc, 0)
        self.assertEqual(calls, [["cmd", "/c", str(checkout / "bootstrap-vcpkg.bat"), "-disableMetrics"]])


if __name__ == "__main__":
    unittest.main()
