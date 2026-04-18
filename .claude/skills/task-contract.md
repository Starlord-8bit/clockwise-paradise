---
description: Produce a structured Task Contract from a user request. Called by the PM agent during triage. The Task Contract is the single source of truth dispatched to specialists; nothing in a specialist's Handoff Contract may reference behavior not named here.
---

# Task Contract

A Task Contract turns an informal user request into a spec the specialist can execute and the reviewer can cross-check. Produce one per dispatch except for genuinely trivial docs-only one-liners, where a constrained brief is acceptable.

## Format

```
## Task Contract <short-slug>

- category: [firmware feature | firmware bug | frontend | widget | connectivity |
             rtos | display | chore | docs]
- specialist: [coder | firmware-rtos | display-render | widget-author |
               connectivity | frontend]
- branch: [feature/<slug> | fix/<slug> | ...]
- related ADR: [NNNN-slug or "none"]
- related CONSTRAINTS: [RT-N, RT-M or "none triggered"]

### Goal
[One paragraph. The outcome the user wants, in their language. No implementation detail.]

### Acceptance criteria
- [ ] [Testable condition 1]
- [ ] [Testable condition 2]
- [ ] [Testable condition 3]

### In-scope files (agent may modify)
- [path/to/file.h]
- [path/to/file.cpp]

### Out-of-scope (agent must NOT touch)
- components/**
- firmware/clockfaces/**
- [any file explicitly off-limits]

### Tests required
- Native Unity test in firmware/test/test_native/ covering: [behavior]
- If HW-only: manual verification checklist — [items]

### Hardware verification
- [ ] /test-hw after merge
- Expected boot outcome: [clean boot, rollback window closed within 10 s, widget X visible]

### Risks / rules the reviewer will check
- [e.g., RT-1 (blocking in loop), RT-4 (NVS key length), RT-10 (XSS in UI)]

### Known unknowns
- [Anything the user was vague on that the specialist is authorised to decide,
  with the chosen default in parentheses. Otherwise: "none"]
```

## Rules

- **Goal first, implementation last.** If the Goal section mentions a specific function or file, rewrite it.
- **Acceptance criteria must be testable.** "Works correctly" is not acceptance — "timer at 0 returns to current clockface within 1 s" is.
- **In-scope / out-of-scope are binding.** If the specialist needs a file not listed, they must stop and ask.
- **Name the rules in play.** If the change touches `loop()`, name RT-1. If it adds an NVS key, name RT-4. For reviewer-routed work, the reviewer must verify every named rule explicitly.
- **Known unknowns section is where the PM makes defensible defaults.** Specialists follow the default unless they hit a blocker.

## Docs-only exception

For docs-only or chore-only work:

- route to `coder`
- do not invent a reviewer type that does not exist
- do not label the work as `type: firmware` or `type: frontend` just to fit the reviewer router

A full Task Contract is still preferred. A one-paragraph brief is acceptable only when the task is genuinely trivial and the brief includes:

- goal
- in-scope file or files
- out-of-scope boundary
- acceptance expectation
- verification required

## Anti-patterns

- Task Contract that just restates the user's sentence → expand into goal + acceptance + scope.
- Task Contract that lists code changes line by line → that's the specialist's job.
- Task Contract with no tests — flag it, then either add a test plan or explicitly document HW-only.
- Task Contract that touches more than one specialist's domain without a plan for sequencing — split it.

## After writing the contract

Hand the full contract to the specialist via the dispatch message. For reviewer-routed work, the specialist echoes the contract's slug in their Handoff Contract so the reviewer can cross-check scope. For docs-only work, the specialist returns a completion note instead of a reviewer handoff.
