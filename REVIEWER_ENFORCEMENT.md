# Reviewer Enforcement

This document is the current reviewer-enforcement coverage matrix for Clockwise Paradise.

It records what the process says today, what is enforced today, and what contributors must do when a task falls outside the current reviewer types.

## Current rule

- Reviewer routing only supports `type: firmware` and `type: frontend` handoff contracts.
- Docs-only and chore-only tasks do not go through `reviewer`.
- Docs-only and chore-only tasks must not be disguised as `type: firmware` just to get through the router.
- The Task Contract remains the source of truth for scope, named constraints, and named reviewer-check risks.
- Reviewer-routed work must include a valid handoff contract. If required fields are missing, the router rejects the contract instead of guessing intent.

## Coverage matrix

| Process surface | Current enforcement | Required expectation | Current gap / note |
| --- | --- | --- | --- |
| Task Contract | Carries `related CONSTRAINTS` and `Risks / rules the reviewer will check` | PM must name every rule the reviewer is expected to verify | Works if the task contract is written carefully; the named rules only matter if reviewers read them explicitly |
| PM intake and routing | PM already classifies docs and chore work separately | Docs-only tasks must route to `coder` without reviewer | Previously implied docs/chore existed, but did not say how they bypass reviewer |
| Coder handoff behavior | Firmware path already produces `type: firmware` handoffs | Docs-only tasks must return a completion note, not a fake firmware handoff | Previously no explicit docs-only completion path |
| Reviewer router | Routes only `type: firmware` and `type: frontend` | Reject malformed handoffs; reject docs-only submissions sent to reviewer | Previously only validated `type`, and silent guessing was possible for incomplete contracts |
| Reviewer specialist checks | Firmware and frontend reviewers run their own checklists | They must also read the task contract's named constraints and named risks and verify each one explicitly | Previously the local checklist was explicit, but the task-contract rule list was only implied context |
| Docs-only route | No reviewer type exists for docs-only work | Keep docs-only work outside reviewer until a real reviewer type is defined | This is intentional for now; do not invent `type: docs` |

## Operational expectations

- Use a full Task Contract for normal docs/process work.
- A genuinely trivial docs-only change may use a one-paragraph brief instead of a full Task Contract.
- That brief is only acceptable if it names the goal, in-scope file or files, out-of-scope boundaries, acceptance expectation, and verification to run.
- For reviewer-routed work, the router checks contract shape before routing, and the specialist reviewer checks both the local checklist and every named task-contract rule.

## Current limitations

- There is no docs reviewer. That is a process limitation, not a license to mislabel docs work as firmware or frontend.
- Named task-contract rules still depend on PM discipline. If a constraint or risk is omitted from the task contract, the reviewer can only enforce what is actually named plus the standing reviewer checklist.