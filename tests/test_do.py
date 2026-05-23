from __future__ import annotations

import importlib.util
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


def run_git(command: list[str], cwd: pathlib.Path) -> None:
    subprocess.run(["git", *command], cwd=cwd, check=True, capture_output=True, text=True)


def git_output(command: list[str], cwd: pathlib.Path) -> str:
    return subprocess.run(["git", *command], cwd=cwd, check=True, capture_output=True, text=True).stdout.strip()


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

        with mock.patch.object(do.sys, "platform", "win32"):
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

    def test_sync_command_accepts_latest_alias_and_clean_flag(self):
        do = load_do_module()

        parsed = do.parse_args(["latest", "--clean", "--dry-run"])

        self.assertEqual(parsed.command, "sync")
        self.assertTrue(parsed.clean)
        self.assertTrue(parsed.dry_run)

    def test_push_command_accepts_publish_alias(self):
        do = load_do_module()

        parsed = do.parse_args(["publish", "--dry-run"])

        self.assertEqual(parsed.command, "push")
        self.assertTrue(parsed.dry_run)

    def test_sync_configures_and_updates_submodules(self):
        do = load_do_module()
        calls: list[list[str]] = []

        def fake_run(command, **kwargs):
            calls.append([str(part) for part in command])
            return mock.Mock(returncode=0)

        with mock.patch.object(do.subprocess, "run", side_effect=fake_run):
            rc = do.sync_repo(ROOT, clean=False, dry_run=False)

        self.assertEqual(rc, 0)
        self.assertIn(["git", "config", "--local", "submodule.recurse", "true"], calls)
        self.assertIn(["git", "config", "--local", "fetch.recurseSubmodules", "on-demand"], calls)
        self.assertIn(["git", "fetch", "--recurse-submodules=on-demand", "--prune"], calls)
        self.assertIn(["git", "pull", "--ff-only", "--recurse-submodules=on-demand"], calls)
        self.assertIn(["git", "submodule", "sync", "--recursive"], calls)
        self.assertIn(["git", "submodule", "update", "--init", "--recursive", "--jobs", "8"], calls)
        foreach_calls = [call for call in calls if call[:3] == ["git", "submodule", "foreach"]]
        self.assertTrue(any("git checkout" in call[-1] and "git pull --ff-only" in call[-1] for call in foreach_calls))
        self.assertFalse(any("git clean -fdx" in call[-1] for call in foreach_calls))
        root_update_calls = [index for index, call in enumerate(calls) if call == ["git", "submodule", "update", "--init", "--recursive", "--jobs", "8"]]
        branch_sync_index = next(index for index, call in enumerate(calls) if call[:3] == ["git", "submodule", "foreach"] and "configured_branch" in call[-1])
        child_update_index = next(index for index, call in enumerate(calls) if call[:3] == ["git", "submodule", "foreach"] and "git submodule update --init --recursive --jobs 8" in call[-1])
        branch_sync_indices = [index for index, call in enumerate(calls) if call[:3] == ["git", "submodule", "foreach"] and "configured_branch" in call[-1]]
        self.assertEqual(len(root_update_calls), 1)
        self.assertGreater(child_update_index, branch_sync_index)
        self.assertGreater(branch_sync_indices[-1], child_update_index)

    def test_sync_clean_resets_and_cleans_submodules_explicitly(self):
        do = load_do_module()
        calls: list[list[str]] = []

        def fake_run(command, **kwargs):
            calls.append([str(part) for part in command])
            return mock.Mock(returncode=0)

        with mock.patch.object(do.subprocess, "run", side_effect=fake_run):
            rc = do.sync_repo(ROOT, clean=True, dry_run=False)

        self.assertEqual(rc, 0)
        clean_index = next(index for index, call in enumerate(calls) if call[:3] == ["git", "submodule", "foreach"] and "git reset --hard" in call[-1] and "git clean -fdx" in call[-1])
        pull_index = next(index for index, call in enumerate(calls) if call == ["git", "pull", "--ff-only", "--recurse-submodules=on-demand"])
        root_update_index = next(index for index, call in enumerate(calls) if call == ["git", "submodule", "update", "--init", "--recursive", "--jobs", "8"])
        self.assertLess(clean_index, pull_index)
        self.assertLess(clean_index, root_update_index)

    def test_push_pushes_submodules_before_parent(self):
        do = load_do_module()
        calls: list[list[str]] = []

        def fake_run(command, **kwargs):
            calls.append([str(part) for part in command])
            return mock.Mock(returncode=0)

        with mock.patch.object(do.subprocess, "run", side_effect=fake_run):
            rc = do.push_all(ROOT, dry_run=False)

        self.assertEqual(rc, 0)
        self.assertEqual(calls[0], ["git", "submodule", "status", "--recursive"])
        self.assertEqual(calls[1][:3], ["git", "submodule", "foreach"])
        self.assertIn("git push -u origin", calls[1][-1])
        self.assertEqual(calls[-1], ["git", "push", "--recurse-submodules=check"])

    def test_gitconfig_installs_submodule_defaults(self):
        do = load_do_module()
        calls: list[list[str]] = []

        def fake_run(command, **kwargs):
            calls.append([str(part) for part in command])
            return mock.Mock(returncode=0)

        with mock.patch.object(do.subprocess, "run", side_effect=fake_run):
            rc = do.configure_git_defaults(ROOT, global_scope=True, dry_run=False)

        self.assertEqual(rc, 0)
        self.assertIn(["git", "config", "--global", "push.recurseSubmodules", "check"], calls)

    def test_generated_submodule_shell_snippets_parse(self):
        do = load_do_module()

        for script in (
            do.git_foreach(do.SUBMODULE_BRANCH_SYNC_SCRIPT)[-1],
            do.git_foreach(do.SUBMODULE_PUSH_SCRIPT)[-1],
            do.git_foreach(do.SUBMODULE_STATUS_SCRIPT)[-1],
            do.git_foreach(do.SUBMODULE_CLEAN_SCRIPT)[-1],
            do.git_foreach(do.SUBMODULE_CHILD_UPDATE_SCRIPT)[-1],
        ):
            result = subprocess.run(["sh", "-n"], input=script, text=True, capture_output=True, check=False)
            self.assertEqual(result.returncode, 0, result.stderr)

    def test_branch_sync_falls_back_to_remote_head_when_configured_branch_is_missing(self):
        do = load_do_module()

        with tempfile.TemporaryDirectory() as temp_dir:
            temp_root = pathlib.Path(temp_dir)
            remote = temp_root / "remote.git"
            seed = temp_root / "seed"
            clone = temp_root / "clone"
            superproject = temp_root / "super"

            run_git(["init", "--bare", str(remote)], temp_root)
            run_git(["init", "-b", "main", str(seed)], temp_root)
            run_git(["config", "user.email", "test@example.com"], seed)
            run_git(["config", "user.name", "Test User"], seed)
            (seed / "file.txt").write_text("hello\n", encoding="utf-8")
            run_git(["add", "file.txt"], seed)
            run_git(["commit", "-m", "initial"], seed)
            run_git(["remote", "add", "origin", str(remote)], seed)
            run_git(["push", "-u", "origin", "main"], seed)
            run_git(["clone", str(remote), str(clone)], temp_root)

            superproject.mkdir()
            (superproject / ".gitmodules").write_text(
                "[submodule \"libs/tsf\"]\n"
                "\tpath = libs/tsf\n"
                f"\turl = {remote}\n"
                "\tbranch = master\n",
                encoding="utf-8",
            )

            result = subprocess.run(
                ["sh", "-c", do.SUBMODULE_BRANCH_SYNC_SCRIPT],
                cwd=clone,
                env={"PATH": "/usr/bin:/bin:/opt/homebrew/bin", "toplevel": str(superproject), "name": "libs/tsf"},
                capture_output=True,
                text=True,
                check=False,
            )

            self.assertEqual(result.returncode, 0, result.stderr)
            branch = subprocess.run(["git", "branch", "--show-current"], cwd=clone, capture_output=True, text=True, check=True).stdout.strip()
            self.assertEqual(branch, "main")

    def test_branch_sync_leaves_unconfigured_submodules_at_recorded_commit(self):
        do = load_do_module()

        with tempfile.TemporaryDirectory() as temp_dir:
            temp_root = pathlib.Path(temp_dir)
            remote = temp_root / "remote.git"
            seed = temp_root / "seed"
            clone = temp_root / "clone"
            superproject = temp_root / "super"

            run_git(["init", "--bare", str(remote)], temp_root)
            run_git(["init", "-b", "main", str(seed)], temp_root)
            run_git(["config", "user.email", "test@example.com"], seed)
            run_git(["config", "user.name", "Test User"], seed)
            (seed / "file.txt").write_text("one\n", encoding="utf-8")
            run_git(["add", "file.txt"], seed)
            run_git(["commit", "-m", "initial"], seed)
            first_commit = git_output(["rev-parse", "HEAD"], seed)
            (seed / "file.txt").write_text("two\n", encoding="utf-8")
            run_git(["commit", "-am", "second"], seed)
            run_git(["remote", "add", "origin", str(remote)], seed)
            run_git(["push", "-u", "origin", "main"], seed)
            run_git(["clone", str(remote), str(clone)], temp_root)
            run_git(["checkout", "--detach", first_commit], clone)

            superproject.mkdir()
            (superproject / ".gitmodules").write_text(
                "[submodule \"external/lib\"]\n"
                "\tpath = external/lib\n"
                f"\turl = {remote}\n",
                encoding="utf-8",
            )

            result = subprocess.run(
                ["sh", "-c", do.SUBMODULE_BRANCH_SYNC_SCRIPT],
                cwd=clone,
                env={"PATH": "/usr/bin:/bin:/opt/homebrew/bin", "toplevel": str(superproject), "name": "external/lib"},
                capture_output=True,
                text=True,
                check=False,
            )

            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(git_output(["rev-parse", "HEAD"], clone), first_commit)
            self.assertEqual(git_output(["branch", "--show-current"], clone), "")


if __name__ == "__main__":
    unittest.main()
