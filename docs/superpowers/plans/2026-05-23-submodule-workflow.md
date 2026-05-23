# Submodule Workflow Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make VkLive's submodule-heavy personal workflow simple: get the latest of everything, keep submodules on their tracking branches, detect divergence, and push all touched repos in dependency order.

**Architecture:** Extend `do.py` as the primary workflow entrypoint, with small shell wrappers for muscle memory. Keep operations explicit and inspectable: sync fetches/pulls/updates, status explains drift, push checks for unpushed submodule commits before pushing the parent, and config installs safer Git defaults.

**Tech Stack:** Python 3 standard library, Git CLI, existing `unittest` tests in `tests/test_do.py`, Markdown docs.

---

### Task 1: Add Tested Workflow Commands

**Files:**
- Modify: `do.py`
- Modify: `tests/test_do.py`

- [ ] **Step 1: Write failing parser tests**

Add tests that expect `do.py` to accept `sync`, `push`, `submodules`, and `gitconfig`, including dry-run flags for command sequencing.

- [ ] **Step 2: Run parser tests to verify failure**

Run: `python3 -m unittest tests.test_do`

- [ ] **Step 3: Implement command parsing and command dispatch**

Add parsed command objects for the new workflow commands while preserving the existing `run` behavior.

- [ ] **Step 4: Verify parser tests pass**

Run: `python3 -m unittest tests.test_do`

### Task 2: Implement Submodule Hygiene

**Files:**
- Modify: `do.py`
- Modify: `tests/test_do.py`

- [ ] **Step 1: Write failing tests for command order**

Mock subprocess calls and assert `sync` runs Git config, fetch, fast-forward pull, submodule sync/update, branch checkout/pull for configured tracking branches, status, and drift checks.

- [ ] **Step 2: Run tests to verify failure**

Run: `python3 -m unittest tests.test_do`

- [ ] **Step 3: Implement the sync/status helpers**

Use `.gitmodules` data and `git submodule foreach --recursive` helpers so submodules with branch config are checked out to their branch and fast-forwarded, while nested untracked files can be cleaned with an explicit `--clean` flag.

- [ ] **Step 4: Verify tests pass**

Run: `python3 -m unittest tests.test_do`

### Task 3: Implement Dependency-Order Push

**Files:**
- Modify: `do.py`
- Modify: `tests/test_do.py`

- [ ] **Step 1: Write failing push tests**

Assert `push` checks submodule status recursively, pushes submodules first with `git submodule foreach --recursive`, then pushes the parent branch.

- [ ] **Step 2: Run tests to verify failure**

Run: `python3 -m unittest tests.test_do`

- [ ] **Step 3: Implement push helpers**

Use `push.recurseSubmodules=check` and an explicit recursive submodule push pass before the parent push.

- [ ] **Step 4: Verify tests pass**

Run: `python3 -m unittest tests.test_do`

### Task 4: Add Shell Aliases and Documentation

**Files:**
- Modify: `subs.sh`
- Create: `sync.sh`
- Create: `push-all.sh`
- Create: `docs/dependencies.md`
- Modify: `README.md`

- [ ] **Step 1: Add shell wrappers**

Make `sync.sh` call `python3 do.py sync "$@"`, `push-all.sh` call `python3 do.py push "$@"`, and `subs.sh` delegate to the new sync command.

- [ ] **Step 2: Document daily workflow**

Document `dr sync`, `dr submodules`, `dr push`, and `dr gitconfig`, plus the intended treatment of root, active, frozen, tooling, and nested submodules.

- [ ] **Step 3: Verify docs and scripts**

Run: `python3 -m unittest tests.test_do`, `bash -n sync.sh push-all.sh subs.sh`, and `python3 do.py --help`.

### Task 5: Final Verification and Publish

**Files:**
- All touched files

- [ ] **Step 1: Run focused tests**

Run: `python3 -m unittest tests.test_do`

- [ ] **Step 2: Run build and full test suite**

Run: `cmake --build build --config Debug` and `ctest --test-dir build --output-on-failure`.

- [ ] **Step 3: Commit and push**

Stage the workflow/docs changes, commit with `feat: add submodule workflow helpers`, and push `main`.
