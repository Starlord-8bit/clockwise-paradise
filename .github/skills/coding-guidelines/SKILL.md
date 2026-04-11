---
name: coding-guidelines
description: "Use when implementing new firmware features, refactors, endpoints, settings, tests, or CI changes in Clockwise Paradise; enforces structured, optimized, and regression-safe implementation rules. Keywords: coding guidelines, optimization, architecture, NVS wear, CI hygiene, version sync, test-first, hardening."
---

# Coding Guidelines (Clockwise Paradise)

Purpose: keep new code structured, performant, and safe while reducing regressions.

Scope: ESP32 firmware C/C++ and repo workflow files.

## 1) Core Principles

- Prefer small, reversible changes over wide refactors.
- Keep one source of truth per behavior.
- Favor deterministic behavior over implicit side effects.
- Optimize for reliability first, then performance.
- Do not optimize blindly: justify with hotspot, wear, latency, or CI-cycle savings.

## 2) Architecture Rules

- Avoid adding global mutable state unless required by hardware lifecycle.
- If adding a setting:
  - Add key/field/load/save in `firmware/lib/cw-commons/CWPreferences.h`.
  - Ensure NVS key length is <= 15 chars.
  - Add runtime handling in `firmware/lib/cw-commons/CWWebServer.h`.
  - Expose in `getCurrentSettings()` where relevant.
- If adding an endpoint:
  - Keep request parsing bounded and validated.
  - No credentials in query strings.
  - Define clear status codes and JSON error paths.
- If adding mode/state flags, avoid dual-representation drift.
  - If two fields represent same concept, enforce sync in one place.

## 3) Performance + Wear Rules

- Avoid full NVS save for single-field updates in periodic paths.
  - Prefer targeted writes for low-cardinality updates (example: uptime, clockface index).
- Avoid heavy `String` concatenation in tight loops and frequent handlers.
- No blocking calls in hot loop sections unless bounded and justified.
- Dynamic allocation in rendering/display loops should be avoided.

## 4) Security Rules

- Never transmit passwords/tokens in URL query parameters.
- Never include secrets in backups unless explicitly encrypted and user-approved.
- Treat LAN trust as limited trust; avoid unauthenticated destructive endpoints by default when feasible.
- Do not commit device-local secrets (`.env`, credentials, API keys).

## 5) CI/CD Rules

- Keep CI jobs DRY (reusable workflows for repeated jobs).
- Use workflow `concurrency` for canceling stale runs on same ref.
- Enforce version synchronization checks:
  - `version.txt`
  - `firmware/platformio.ini` (`CW_FW_VERSION`)
- Keep release flow aligned with release-please workflow ownership.

## 6) Test Rules

- Any pure logic change requires native tests in `firmware/test/test_native/SimpleTests.cpp` (or split file if introduced later).
- Required for new/changed logic in:
  - time-window predicates
  - parsers/decoders
  - version normalization/comparison
  - setting dispatch/validation
- Hardware-only behaviors must include manual verification checklist updates.

## 7) Documentation Rules

When behavior changes, update docs in the same change set:

- `README.md` for user-facing behavior.
- `CHECKLIST.md` for release process changes.
- `CHANGELOG.md` if change is release-relevant.

If docs are intentionally deferred, state why in PR notes.

## 8) Pre-Merge Quality Gate

Before handoff/review, verify all:

- [ ] New/changed settings are fully wired (key + load + save + handler + output).
- [ ] No NVS over-write hot path introduced.
- [ ] Native tests added/updated for logic changes.
- [ ] CI workflow edits are reusable and non-duplicative.
- [ ] Version sync still passes.
- [ ] README/CHECKLIST consistency maintained.
- [ ] No credentials leaked via URLs, logs, or committed files.

## 9) Anti-Patterns (Reject)

- Full-struct `save()` on frequent timers for one-field changes.
- Copy-pasted workflow jobs across multiple YAML files.
- Query-string credential updates.
- "Temporary" behavior flags without cleanup plan.
- Large refactors with no tests and no rollback path.

## 10) Suggested Task Template

Use this plan for new work:

1. Define scope + impacted files.
2. Add/adjust tests first for pure logic.
3. Implement minimal change.
4. Run native tests + static checks.
5. Update docs/checklists.
6. Provide manual verification steps for hardware behavior.

## 11) Repo Anchors

Use these files as implementation anchors:

- Settings/NVS: `firmware/lib/cw-commons/CWPreferences.h`
- HTTP/API: `firmware/lib/cw-commons/CWWebServer.h`
- Runtime loop/modes: `firmware/src/main.cpp`
- Native tests: `firmware/test/test_native/SimpleTests.cpp`
- CI build/release: `.github/workflows/build.yml`, `.github/workflows/release.yml`
- Release process docs: `CHECKLIST.md`, `README.md`, `CHANGELOG.md`
