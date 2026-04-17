# Architecture Decision Records

Short records of **why** we chose something. Anyone (human or agent) asking "why is X done this way" should find the answer here instead of guessing or rewriting.

## Format

One file per decision: `NNNN-short-slug.md`, numbered sequentially. Each record:

```markdown
# ADR NNNN — <decision>

- Status: accepted | superseded by ADR ####
- Date: YYYY-MM-DD

## Context
What problem or constraint forced a decision.

## Decision
What we chose.

## Consequences
What this means — good and bad. What to watch for.

## Alternatives considered
What else was on the table, briefly, and why we didn't pick it.
```

Keep records short. If an ADR is > 300 words it's probably two decisions.

## Index

- [0001](0001-agentic-architecture.md) — Agentic harness shape (PM + specialists + reviewers)
- [0002](0002-single-loop-concurrency.md) — Single Arduino loop task, no custom FreeRTOS tasks
