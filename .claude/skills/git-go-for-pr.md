---
description: Prepare and create a pull request for the current branch. Verifies all reviewer-approved commits are pushed, summarizes changes vs main, and opens a PR via gh. Use /git-go-for-pr when work on a feature or fix branch is complete and reviewed.
---

Create a pull request for the current branch.

## Steps

1. Confirm current branch and that it is not `main`:
```bash
git branch --show-current
```
If on `main`: stop and ask the user which branch to create the PR from.

2. Confirm all commits are pushed:
```bash
git status
git log origin/$(git branch --show-current)..HEAD --oneline
```
If there are unpushed commits: push them first.
```bash
git push -u origin $(git branch --show-current)
```

3. Summarize the branch vs main:
```bash
git log main..HEAD --oneline
git diff main..HEAD --stat
```

4. Check CI status (if remote branch exists):
```bash
gh pr checks 2>/dev/null || gh run list --branch $(git branch --show-current) --limit 3
```
If CI is failing: warn the user and ask whether to proceed or fix CI first.

5. Create the PR:
```bash
gh pr create \
  --base main \
  --title "<derived from branch name and commits>" \
  --body "$(cat <<'EOF'
## Summary
- [bullet points from commit messages on this branch]

## Test plan
- [ ] Native tests pass (`pio test -e native`)
- [ ] OTA flash successful (`/flash`)
- [ ] Device health verified (`/diagnose`)

🤖 Generated with [Claude Code](https://claude.com/claude-code)
EOF
)"
```

6. Return the PR URL.

## Notes

- PR title follows Conventional Commits format: `feat:`, `fix:`, `ci:`, etc.
- Base branch is always `main` unless the coordinator specifies otherwise.
- Do not create a PR if CI is red unless the user explicitly confirms.
