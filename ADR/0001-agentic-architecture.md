# ADR 0001 — Agentic harness shape

- Status: accepted
- Date: 2026-04-17

## Context

Earlier attempts at agent-driven firmware development on this repo produced spaghetti C++, frequent bootloops, and flaky connectivity. Root cause was not "dumb agents" — it was a general coding agent without embedded constraints producing code a senior firmware dev would reject. Fixing the agent is not a prompt tweak; the shared knowledge layer needs to be first-class context.

## Decision

Adopt a three-layer architecture:

1. **Entry-point orchestrator (`pm`)** — the only agent the user talks to by default. Never writes code. Responsibilities: clarify intent, triage (feature / bug / test feedback / chore), write a structured task contract with acceptance criteria, dispatch to the right specialist.

2. **Specialist implementers** — narrow-scope agents with their own system prompts and domain skills:
   - `coder` — generic firmware changes (default)
   - `firmware-rtos` — boot, memory, tasks, ISR, watchdog
   - `display-render` — HUB75, DMA, framebuffer, brightness
   - `widget-author` — widget lifecycle and manager
   - `connectivity` — Wi-Fi, MQTT, HA discovery, OTA
   - `frontend` — HTML/JS/CSS inside `SettingsWebPage.h` / `CWWebUI.h`
   - `reviewer` (router) + `reviewer-firmware` + `reviewer-frontend`
   - `github-specialist` — all git/GitHub ops

3. **Shared knowledge layer** — `CLAUDE.md` (ground truth), `CONSTRAINTS.md` (hard rules), `.claude/skills/*` (domain skills the reviewer checks against), `ADR/*` (why).

## Consequences

- User has one consistent entry point; does not need to remember which agent to summon.
- Specialists can be prompt-tuned independently. A bug in MQTT discovery does not degrade the display pipeline's prompt.
- Reviewer enforces `CONSTRAINTS.md` — all rules are in one auditable file, not scattered across agent prompts.
- Every hard-won lesson (bootloop, DMA glitch, MQTT disconnect) becomes a line in `CONSTRAINTS.md` or a skill entry. Reliability is cumulative.
- Cost: more agent files to maintain. Mitigation: the PM only invokes specialists when the domain checklist matters; routine firmware work goes straight to `coder`.

## Alternatives considered

- **Single generic coder + long CLAUDE.md.** Tried earlier. CLAUDE.md bloats, agents skim, rules drift. Rejected.
- **One agent per file / tight scope.** Overkill; specialists map to problem domains, not folders. Rejected.
- **No orchestrator, user picks agent.** Works for power users, but breaks "talk to the project in plain English" UX. Rejected as default.
