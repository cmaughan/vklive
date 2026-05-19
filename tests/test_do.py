from __future__ import annotations

import importlib.util
import pathlib
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

    def test_run_rejects_unknown_mode(self):
        do = load_do_module()

        with self.assertRaises(SystemExit) as cm:
            do.parse_args(["run", "profile"])

        self.assertEqual(cm.exception.code, 2)

    def test_run_builds_then_launches_exe(self):
        do = load_do_module()
        calls: list[list[str]] = []

        def fake_run(command, **kwargs):
            calls.append([str(part) for part in command])
            return mock.Mock(returncode=0)

        with mock.patch.object(do.subprocess, "run", side_effect=fake_run):
            with mock.patch.object(pathlib.Path, "exists", return_value=True):
                rc = do.run_project(ROOT, "Release", ["--smoke-test"])

        self.assertEqual(rc, 0)
        self.assertEqual(calls[0], ["cmd", "/c", str(ROOT / "config.bat")])
        self.assertEqual(calls[1], ["cmd", "/c", str(ROOT / "build.bat"), "Release"])
        self.assertEqual(calls[2], [str(ROOT / "build" / "Release" / "Rezonality.exe"), "--smoke-test"])

    def test_run_forwards_project_and_scenegraph_args(self):
        do = load_do_module()

        parsed = do.parse_args(["run", "release", "--", "--project", "run_tree/projects/pbr_robot", "--scenegraph", "uv_debug.scenegraph"])

        self.assertEqual(parsed.config, "Release")
        self.assertEqual(parsed.app_args, ["--project", "run_tree/projects/pbr_robot", "--scenegraph", "uv_debug.scenegraph"])
        self.assertIn("--scenegraph", do.help_text())

    def test_run_forwards_app_args_when_wrapper_strips_separator(self):
        do = load_do_module()

        parsed = do.parse_args(["run", "release", "--project", "run_tree/projects/pbr_robot", "--scenegraph", "uv_debug.scenegraph"])

        self.assertEqual(parsed.config, "Release")
        self.assertEqual(parsed.app_args, ["--project", "run_tree/projects/pbr_robot", "--scenegraph", "uv_debug.scenegraph"])

    def test_run_defaults_to_debug_when_app_args_follow_command(self):
        do = load_do_module()

        parsed = do.parse_args(["run", "--project", "run_tree/projects/pbr_robot", "--scenegraph", "uv_debug.scenegraph"])

        self.assertEqual(parsed.config, "Debug")
        self.assertEqual(parsed.app_args, ["--project", "run_tree/projects/pbr_robot", "--scenegraph", "uv_debug.scenegraph"])


if __name__ == "__main__":
    unittest.main()
