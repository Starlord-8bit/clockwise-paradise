# Clockwise Paradise Backlog Plan

This file is the single live planning artifact for the audit backlog.

Use this document as the only current execution board for status, sequencing, and ownership. `.audit/findings.md` remains the supporting evidence log, and `.audit/tasks.md` is superseded as a live task board.

Last reviewed: 2026-04-18

## Current status snapshot

| Item | Status | Owner | Notes |
| --- | --- | --- | --- |
| A1 | Done | coder | Consolidating planning/status into this file. |
| A2 | Done | coder | Added `BRANCH_PROTECTION.md` and linked it from `CHECKLIST.md`; documented required checks and the `paths-ignore` caveat. |
| A3 | Done | coder | Added `REVIEWER_ENFORCEMENT.md` and aligned PM/coder/reviewer docs on docs-only routing, contract validity, and named-rule enforcement. |
| A4 | Done | coder | Added `MIXED_SURFACE_OWNERSHIP.md` and aligned PM, task-contract, and add-setting guidance on mixed UI plus firmware/connectivity ownership and sequencing. |
| C1 | Done | pm | Implemented on PR #32. |
| C2 | Proposed | pm | Depends on C1 and A4. |
| C3 | Proposed | coder | Persistence follow-up after planning alignment. |
| C4 | Proposed | display-render | Night-mode state bug remains open. |
| C5 | Done | connectivity | Added broker-backed MQTT smoke coverage to `scripts/test_hw.py` and documented invocation/prereqs in repo docs. |
| C6 | Proposed | coder | Header/dead-config cleanup still pending. |
| C7 | Proposed | coder | Prefer after C1 and C2. |

## Recommended execution order

1. Close platform workflow gaps first: establish one current audit backlog, document required CI and branch gates, and clarify cross-agent ownership for mixed web/UI plus firmware tasks.
2. Address the two LAN-surface hardening items next: stop sending credentials in query strings, then add optional auth for mutating web endpoints.
3. Reduce persistence risk after security work: implement debounced or targeted NVS writes for bursty settings updates.
4. Tackle correctness and boundary cleanup: night-mode state mutation, MQTT hardware coverage, and header/dead-config cleanup.
5. Finish with documentation that lowers future drift: HTTP API reference and explicit credential-storage limitations.

## 1. Agentic self-assessment / platform alignment

### A1. Consolidate audit tracking into one live planning artifact
- Status: Done
- Priority: P1
- Owner: pm
- Scope summary: Replace the current split between `.audit/findings.md`, `.audit/tasks.md`, and ad hoc status notes with a single maintained planning view that clearly marks active, deferred, and closed items.
- Why it matters: The codebase already has local audit artifacts, but they drift quickly once some items are done and others are deferred. One live backlog reduces workflow drift and prevents stale recommendations from being treated as current work.
- Suggested first step: Reconcile the existing `.audit` files into a single status table with explicit owners and last-reviewed dates.
- Dependencies / sequencing: Start first. This becomes the planning source for the rest of the backlog.

### A2. Document required CI gates and branch protection for release flow
- Status: Done
- Priority: P1
- Owner: coder
- Scope summary: Capture the expected protected-branch settings and required checks for `main`, including build, native tests, version-sync enforcement, and release-please expectations.
- Why it matters: Several build-validation improvements are already in place, but repository settings still appear undocumented. That leaves room for bypassing the intended safety net and reintroducing workflow drift.
- Suggested first step: Compare `.github/workflows/` behavior with `CHECKLIST.md`, then write a short repo-operations note listing required checks and branch rules.
- Dependencies / sequencing: Do after A1 so the artifact points at a current planning source.
- Completion note: Added `BRANCH_PROTECTION.md` as the tracked repo-operations note, linked it from `CHECKLIST.md`, listed the current PR-time checks by job name, documented the squash-merge and release-please contract, and called out that `paths-ignore` prevents those checks from running on docs-only and clockface-only pull requests.

### A3. Align reviewer enforcement with current constraints and handoff contracts
- Status: Done
- Priority: P1
- Owner: coder
- Scope summary: Verify that the reviewer workflow explicitly checks the hard constraints in `CONSTRAINTS.md`, the coder handoff contract, and documentation-only task handling.
- Why it matters: The repo intentionally relies on reviewer enforcement to keep boundaries safe. If the review gate drifts from `CONSTRAINTS.md`, the architecture loses its main protection against regressions and hidden scope creep.
- Suggested first step: Build a simple coverage matrix mapping RT-1 through RT-16 to concrete reviewer checks and note any gaps.
- Dependencies / sequencing: Can run in parallel with A2 after A1.
- Completion note: Added `REVIEWER_ENFORCEMENT.md`, documented that docs-only work bypasses reviewer instead of pretending to be firmware/frontend, required the reviewer router to reject malformed handoffs, and updated specialist reviewer docs to verify every named Task Contract constraint/risk explicitly.

### A4. Clarify ownership for cross-cutting web UI plus firmware tasks
- Status: Done
- Priority: P2
- Owner: coder
- Scope summary: Define how tasks that span `SettingsWebPage.h` and `CWWebServer.h` should be split or coordinated between `frontend`, `coder`, and `connectivity`.
- Why it matters: The next hardening items cross those boundaries. Clear ownership reduces task bounce, partial fixes, and mismatched server/UI behavior.
- Suggested first step: Add a short coordination rule to the task-contract process for mixed UI plus endpoint changes.
- Dependencies / sequencing: Finish before starting C1 or C2.
- Completion note: Added `MIXED_SURFACE_OWNERSHIP.md` at repo root, made PM routing explicit for mixed UI plus firmware/connectivity work, required Task Contracts to split acceptance and file scope by specialist, and aligned the add-setting coordinator prompt with the same sequencing and one-branch/one-PR rule.

## 2. Codebase audit follow-up

### C1. Move credential updates off query strings
- Status: Done
- Priority: P0
- Owner: pm
- Scope summary: Replace `POST /set?key=...&value=...` handling for sensitive fields with POST body payloads, updating both the UI flow and server-side parsing.
- Why it matters: WiFi and MQTT credentials currently travel in URLs, which can leak through browser history, proxies, and logs. This is the clearest security hardening item still open.
- Suggested first step: Split the work into a small endpoint contract change plus UI submission change, then implement the server path before updating the web UI.
- Dependencies / sequencing: Requires A4. Complete before C2 so auth is layered onto the safer request format.
- Completion note: Implemented on PR #32. Sensitive credential updates now use POST body handling in the active UI/backend flow, with native tests added for request parsing and e2e coverage for the active `mqttPass` path.

### C2. Add optional authentication for mutating HTTP endpoints
- Status: Proposed
- Priority: P1
- Owner: pm
- Scope summary: Add opt-in protection for OTA, restart, settings mutation, and other state-changing HTTP routes without breaking the current default setup flow.
- Why it matters: The device is LAN-exposed and currently trusts every client on the network. Optional auth raises the floor without forcing a complex first-boot experience.
- Suggested first step: Define the minimum protected endpoint set and the configuration model before choosing the implementation details.
- Dependencies / sequencing: Do after C1 and after A4. Coordinate `frontend` and `coder` work from one task contract.

### C3. Debounce or target NVS writes for bursty settings changes
- Status: Proposed
- Priority: P1
- Owner: coder
- Scope summary: Stop doing a full preferences save on every setting mutation, especially for bursty UI updates and automation-driven writes.
- Why it matters: Rewriting the full NVS payload for each change increases wear and makes the settings path heavier than it needs to be. This is a concrete durability and stability improvement.
- Suggested first step: Measure which paths still call the broad `save()` path and design either a dirty-flag flush or targeted-save helpers for high-frequency settings.
- Dependencies / sequencing: Independent of C1 and C2, but should start after A3 so reviewer expectations for persistence changes are clear.

### C4. Fix night-mode state mutation that overwrites live canvas settings
- Status: Proposed
- Priority: P1
- Owner: display-render
- Scope summary: Remove the in-memory overwrite pattern where night-mode activation mutates `canvasServer` and `canvasFile` and later reloads preferences, erasing user edits made during night mode.
- Why it matters: This is a real correctness bug with user-visible data loss, even if it is edge-casey. It also reflects unclear ownership of display state versus persisted settings.
- Suggested first step: Trace the exact night-entry and night-exit state flow in `DisplayControl.cpp` and define a temporary-display-state path that does not rewrite user preferences.
- Dependencies / sequencing: Can follow C3, but does not depend on the security work.

### C5. Add automated MQTT hardware coverage for connectivity and discovery
- Status: Done
- Priority: P1
- Owner: connectivity
- Scope summary: Extend hardware validation to cover broker connection, availability, core command topics, and Home Assistant discovery payload publication.
- Why it matters: MQTT is a major advertised integration surface, yet the current automated hardware checks cover HTTP and OTA only. This leaves a large behavioral gap unguarded.
- Suggested first step: Define the smallest broker-backed smoke test that proves connect, publish, subscribe, and discovery basics.
- Dependencies / sequencing: Best done after A2 so the resulting checks can be folded into the documented CI and release gate expectations.
- Completion note: `scripts/test_hw.py` now has an optional `--mqtt-smoke` stage that configures MQTT on-device, restarts it, subscribes before reboot, verifies availability publication plus retained re-delivery, validates a HA discovery payload for a fresh per-run prefix and verifies retained re-delivery there too, and exercises one safe brightness command round-trip while restoring the prior brightness value. `README.md` and `tests/README.md` document prerequisites and exact invocation. No firmware files were changed.

### C6. Clean up header boundaries and verify suspected dead configuration
- Status: Proposed
- Priority: P2
- Owner: coder
- Scope summary: Remove the `WiFiServer server(80)` header-level ODR risk and confirm whether `MQTT_OTA_POLL_INTERVAL_MS` is live configuration or dead code.
- Why it matters: These are small changes, but they directly support safer boundaries and less dead code without chasing stylistic rewrites.
- Suggested first step: Trace both symbols to real call sites and convert the server declaration to a single-definition pattern if still needed.
- Dependencies / sequencing: Can be done after C5 or bundled with any `CWWebServer` cleanup work.

### C7. Publish a concise HTTP API reference and document credential-storage limits
- Status: Proposed
- Priority: P2
- Owner: coder
- Scope summary: Document the supported HTTP endpoints, expected payload shapes, and the known limitation that credentials remain plaintext in NVS.
- Why it matters: The HTTP surface is larger than the README suggests, and the security model is otherwise easy to misunderstand. Good documentation reduces future misuse and support churn.
- Suggested first step: Derive the endpoint list from `CWWebServer.h`, then add a short limitations section rather than a large manual.
- Dependencies / sequencing: Prefer after C1 and C2 so the docs reflect the safer endpoint contract.