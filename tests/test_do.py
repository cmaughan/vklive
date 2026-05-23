from __future__ import annotations

import importlib.util
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

            with mock.patch.object(do.subprocess, "run", side_effect=fake_run):
                rc = do.configure(root, "Debug")

        self.assertEqual(rc, 0)
        self.assertEqual(calls[0][0:3], ["cmake", "--preset", "debug"])
        self.assertIn(f"-DCMAKE_TOOLCHAIN_FILE={toolchain}", calls[0])
        self.assertIn("-DCMAKE_EXPORT_COMPILE_COMMANDS=ON", calls[0])

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

        with mock.patch.object(do.subprocess, "run", side_effect=fake_run):
            rc = do.build(ROOT, "Debug")

        self.assertEqual(rc, 0)
        self.assertEqual(calls, [["cmake", "--build", str(ROOT / "build" / "debug"), "--config", "Debug", "--parallel"]])

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


if __name__ == "__main__":
    unittest.main()
