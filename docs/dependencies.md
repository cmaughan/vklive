# Dependency And Submodule Workflow

VkLive uses several submodules, including nested submodules inside Zing and Zep. The preferred personal workflow is to use `do.py` through the `dr` alias, or call `python3 do.py` directly.

## Daily Commands

Get the latest parent repo and submodule branch heads:

```sh
dr latest
```

Equivalent direct form:

```sh
python3 do.py sync
```

Inspect the recursive submodule state:

```sh
dr submodules
```

Push Rezonality-owned submodule branches first, then push VkLive:

```sh
dr publish
```

Equivalent direct form:

```sh
python3 do.py push
```

Install the safer Git defaults globally on a machine:

```sh
dr gitconfig --global
```

`dr sync` also installs these defaults locally in the repo:

```sh
submodule.recurse true
fetch.recurseSubmodules on-demand
status.submoduleSummary true
diff.submodule log
push.recurseSubmodules check
```

## Cleaning Diverged Submodules

If submodules drift because generated files, untracked files, or local experiments are in the way, run:

```sh
dr latest --clean
```

This runs `git reset --hard` and `git clean -fdx` inside submodules before pulling their branch heads. Treat it as a deliberate cleanup command: it removes uncommitted submodule changes and untracked files inside submodules.
The clean pass runs before the recursive pull/update steps, so it can recover from nested submodules that were created by a newer branch head and then block checkout of the parent-recorded commit.

## What Gets Pulled

`dr latest` does this:

1. Sets safer local Git defaults for recursive submodule work.
2. Fetches the parent repo with `--recurse-submodules=on-demand`.
3. Pulls the parent branch with `--ff-only`.
4. Syncs and initializes submodules recursively.
5. Checks out each branch-configured submodule to its configured branch.
6. Pulls each branch-configured submodule with `--ff-only`.
7. Initializes child submodules inside the updated submodule worktrees.
8. Re-runs the branch-head pass so child submodules with branch settings also land on their latest branch heads.
9. Prints recursive submodule status.

The branch configuration comes from each `.gitmodules` file. In practice this keeps `zep`, `libs/zing`, nested `libs/zing/libs/zest`, `vcpkg`, and other branch-configured submodules from silently sitting on stale detached commits.
If a submodule still names an old branch such as `master` but the remote has moved to `main`, the helper falls back to the remote HEAD and prints the branch it used.

## What Gets Pushed

`dr publish` does this:

1. Prints recursive submodule status.
2. Pushes submodules whose `origin` remote is under `github.com/Rezonality`.
3. Skips third-party submodules such as Microsoft/vcpkg and external audio/GL libraries.
4. Pushes VkLive with `--recurse-submodules=check`.

That last check is important: the parent push fails if it points at a submodule commit that is not available from a remote. This avoids publishing a VkLive commit that nobody else can sync.

## Dependency Classes

Use these mental buckets:

- **Primary app repo:** `vklive`
- **Active Rezonality source deps:** `zep`, `libs/zing`, `libs/zing/libs/zest`
- **Mostly external or frozen deps:** `libs/rccp`, `libs/nanovg_vulkan`, nested GL/audio libraries
- **Package tooling:** root `vcpkg` and nested standalone `vcpkg` copies

For normal personal work, update everything with `dr latest`, make changes, commit any submodule work in that submodule, commit the parent pointer in VkLive, then run `dr publish`.
