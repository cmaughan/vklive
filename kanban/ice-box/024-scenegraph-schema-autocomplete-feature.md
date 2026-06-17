# Scenegraph Schema Autocomplete Implementation Plan

> **For agentic workers:** Use `superpowers:subagent-driven-development` if splitting parser schema extraction from editor integration. Keep checkboxes updated as work is completed.

**Goal:** Provide scenegraph autocomplete, snippets, and inline schema hints based on a typed schema rather than duplicated ad hoc strings.

**Agents:** Claude requested scenegraph autocomplete and inline schema hints. Codex requested scenegraph autocomplete/snippets based on a typed schema and called out fragile AST string matching. Gemini had no substantive final finding.

## Files

- Modify: `src/scene.cpp`
- Add schema definitions under `include/vklive/` and `src/` if needed
- Modify editor integration under `app/src/editor.cpp` and related app headers
- Modify Zep syntax/autocomplete tests under `tests/`
- Add tests under `tests/`

## Implementation Plan

- [ ] Wait until `001-scene-vector-parser-crash-bug` and `002-scene-parser-lifetime-and-pass-order-bug` land so schema behavior matches hardened parser behavior.
- [ ] Define a typed schema for top-level entries: `surface`, `environment`, `model`, `camera`, `pass`, ray groups, shader fields, targets, samplers, geometry, clear, and `post_2d`.
- [ ] Include value constraints where known: vector arity, allowed formats, shader file extensions, sampler `!` syntax, pass types, and known model/texture path positions.
- [ ] Use the schema from scene validation so parser rules, diagnostics, and editor hints do not drift.
- [ ] Add editor completion/snippet integration through the existing Zep editor path.
- [ ] Add inline hints or diagnostics for unsupported fields and wrong arity without requiring a full scene build.
- [ ] Add tests for schema entries, vector arity metadata, and editor completion suggestions.
- [ ] Run `python3 do.py build debug`.
- [ ] Run `python3 do.py test debug -- -R "scene|zep|editor"`.

## Acceptance Criteria

- [ ] Scenegraph completions include valid keys for the current context.
- [ ] Snippets produce syntactically valid scenegraph blocks.
- [ ] Hints describe vector arity and known value sets.
- [ ] Parser/validation/schema definitions have one source of truth or a tested synchronization point.

## Dependencies

Depends on parser hardening from `001` and `002`; benefits from diagnostics work in `021`.

Consensus reviewer: <gpt-5-codex>
