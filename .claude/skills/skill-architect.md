---
description: Design and create new Claude Code skills (.claude/skills/) and sub-agents (.claude/agents/) for this repository. Use /skill-architect when you need to add new automation to the agentic harness.
---

Design and create a new skill or sub-agent for the Clockwise Paradise agentic harness.

## When to Create a Skill vs an Agent

| Create a **skill** when... | Create an **agent** when... |
|---------------------------|----------------------------|
| It is a user-invocable action (slash command) | It is a specialist that receives work from the coordinator |
| It runs a defined sequence of steps | It requires domain expertise and judgment |
| It is reusable across different task types | It produces a structured Handoff Contract or verdict |
| It wraps a tool or CLI command | It needs to iterate (retry loop, NOK/OK cycle) |

---

## Skill File Template (`.claude/skills/<name>.md`)

```markdown
---
description: <One sentence: what it does and when to use it. This is shown in /help.>
---

<action verb> the <thing>.

## Steps

1. <first step — include exact bash commands>
2. <second step>
3. <parse output / report result>

## Notes

- <constraint or caveat>
- <when NOT to use this skill>
```

Rules for skills:
- The `description` frontmatter is shown to the user in `/help` — make it scannable.
- Lead with the action, not the context.
- Every bash command must be copy-paste ready (no `<placeholder>` without explanation).
- End with what happens after — what the user should do next, or what the agent does next.

---

## Agent File Template (`.claude/agents/<name>.md`)

```markdown
---
name: <agent-name>
description: <When to invoke this agent. Include trigger conditions. 2-3 sentences.>
---

You are the **<role>** for the Clockwise Paradise project.

[Domain and scope definition]

---

## Skills Available to You

- **`/<skill>`** — [what it does in this agent's context]

---

## [Step sections — numbered, concrete, with exact commands]

---

## Handoff Contract / Verdict Format

[Exact format the agent outputs]

---

## What You Do NOT Do

- [boundary 1]
- [boundary 2]
```

Rules for agents:
- Name the agent's domain in the first sentence. Be specific about what is OUT of scope.
- List skills explicitly — agents should use project skills, not reinvent bash commands.
- The "What You Do NOT Do" section prevents scope bleed between agents.
- Every agent that produces output for another agent needs an exact output format.

---

## Steps

1. Ask the user:
   - Skill or agent?
   - What is the trigger / when is this used?
   - What are the inputs and outputs?
   - What tools/commands does it need?
   - Are there any existing skills it should call?

2. Draft the file using the appropriate template above.

3. Show the draft to the user before writing it.

4. Write to `.claude/skills/<name>.md` or `.claude/agents/<name>.md`.

5. If it is an agent that the coordinator dispatches to: confirm with the user whether
   `CLAUDE.md` needs to be updated to reference it in the workflow diagram.

## Notes

- Skill filenames should be kebab-case, matching the slash command: `my-skill.md` → `/my-skill`
- Agent filenames should be kebab-case: `reviewer-firmware.md`
- Keep skills focused — one skill per concern. A skill that does 5 unrelated things is
  really 5 skills.
- Do not duplicate logic already in an existing skill. If a new skill needs to build
  something, it should call `/build` rather than copy the Docker command.
