# Wire Python Tests Into Workflow Implementation Plan

> **For agentic workers:** Use `superpowers:executing-plans` or `superpowers:subagent-driven-development` to implement this task step by step. Keep checkboxes updated as work is completed.

**Goal:** Ensure existing Python workflow/template/static tests run through the normal project test command.

**Agents:** Codex identified that Python tests such as `tests/test_do.py`, `tests/test_pbr_template.py`, and `tests/test_surface_hdr_static.py` were not listed by `python3 do.py test debug -- -N`. Claude praised `do.py` and CTest but agreed stability depends on normal workflow coverage. Gemini had no substantive final finding.

## Files

- Modify: `CMakeLists.txt`
- Modify: `do.py` if a dedicated Python test step is better than CTest registration
- Existing tests: `tests/test_do.py`, `tests/test_pbr_template.py`, `tests/test_surface_hdr_static.py`

## Implementation Plan

- [ ] Run `python3 -m unittest discover tests 'test_*.py'` locally to establish current Python test behavior.
- [ ] Decide whether to register Python tests as CTest entries or add a `do.py test-python` command that `do.py test` invokes. Prefer CTest entries if they are stable and platform-neutral.
- [ ] If using CTest, add `add_test()` entries that invoke `${Python3_EXECUTABLE}` or the configured Python interpreter for each Python test module.
- [ ] If using `do.py`, add a `test-python` subcommand and make `do.py test` call it after CTest unless an explicit skip flag exists.
- [ ] Ensure tests run from the repository root so fixture-relative paths in existing Python tests keep working.
- [ ] Update any help text in `do.py` for the new command or behavior.
- [ ] Run `python3 do.py test debug -- -N` and confirm Python tests are visible if registered with CTest.
- [ ] Run `python3 do.py test debug` or the selected command path.

## Acceptance Criteria

- [ ] Existing Python tests are part of the default verification workflow.
- [ ] A developer can still run them directly with a documented Python command.
- [ ] CI implications are understood; update GitHub Actions only if the default CI path would otherwise skip them.

Consensus reviewer: <gpt-5-codex>
