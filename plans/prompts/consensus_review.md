Go read the latest reviews in `kanban/` and write or replace `kanban/review-consensus.md`, distilling everything into one document. Mark which agents said what, and give some indication of agreements and disagreements. The aim is a combined review, as if all agents were sitting in a room and coming up with an ongoing plan they can all live with.

Once you've made the consensus file, extract a list of work items for everything in the file and make a plan for each feature and how you would implement it in the code base. Don't make work items for things that are already in `kanban/ice-box/` or `kanban/done/`. Don't be afraid to mention when sub agents make sense. The aim should be a file for each item that an agent can read and go do the work.

## Kanban Merge Policy

Do not delete, rename, or renumber existing `kanban/pending/*.md` files. Treat those filenames as stable task IDs.

Read every existing file in `kanban/pending/`, `kanban/ice-box/`, and `kanban/done/` before writing work items.

If an existing pending item is the same idea as a consensus finding, update that existing pending file in place. Preserve its filename and numeric prefix. Update its content only as needed to reflect the new consensus, add missing acceptance criteria, or record new agent agreement.

If a consensus finding is already represented in `kanban/ice-box/` or `kanban/done/`, do not create or update a pending file for it.

If a consensus finding is genuinely new, create a new work item in `kanban/pending/` using the next unused numeric prefix after the highest existing pending prefix. Append `-bug` for bug fixes, `-test` for testing improvements, `-refactor` for refactors, and `-feature` for feature improvements.

Use this conceptual priority order when deciding which new items to add first: bugs, then tests, then refactors as needed, then features. Do not renumber existing pending files to enforce that order. If the old queue order no longer reflects the recommended priority, explain that in `kanban/review-consensus.md` instead of renaming files.

In the markdown leave room for checkboxes; be sure to append your `<model>` to each new or updated pending file so I can see who did this consensus review.

Finally flag any interdependencies between tasks in the consensus review.

If you cannot write files because tool permissions deny writes, do not stop. Return a parseable file bundle in your final response so the host agent can extract it into the repository.

## File Bundle Fallback

Use this exact shape, with one section per file:

## File 1 of N: `kanban/review-consensus.md`

```markdown
<complete file contents>
```

## File 2 of N: `kanban/pending/<next unused numeric prefix> item-name-bug.md`

```markdown
<complete file contents>
```

Keep the file paths repo-relative, include every file that should have been written, and do not include extra prose inside the fenced blocks.
