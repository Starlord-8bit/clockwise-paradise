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

5. Derive the PR title using Conventional Commits format.

   **This is critical** — release-please reads the squash-merge commit title (which
   defaults to the PR title) to determine the next version and build the changelog.
   Getting the type wrong silently misclassifies the change.

   | Branch prefix | Commit type | Version impact |
   |---|---|---|
   | `feature/` or `feat/` | `feat:` | minor bump |
   | `fix/` | `fix:` | patch bump |
   | `ci/` | `ci:` | no release |
   | `refactor/` | `refactor:` | no release |
   | `docs/` | `docs:` | no release |
   | `chore/` | `chore:` | no release |

   Format: `<type>(<optional scope>): <short description in imperative mood>`

   Examples:
   - `feat(ota): add rollback confirmation dialog`
   - `fix(webui): brightness slider not persisting after reboot`
   - `ci: add release-please workflow`

   For breaking changes append `!` after the type: `feat!: replace clockface API`

6. Create the PR:
```bash
gh pr create \
  --base main \
  --title "<conventional commit title derived above>" \
  --body "$(cat <<'EOF'
## Summary
- [bullet points from commit messages on this branch]

## Test plan
- [ ] Native tests pass
- [ ] OTA flash successful on device
- [ ] Device health verified post-flash

🤖 Generated with [Claude Code](https://claude.com/claude-code)
EOF
)"
```

7. Return the PR URL.

## Notes

- Base branch is always `main` unless the coordinator specifies otherwise.
- Do not create a PR if CI is red unless the user explicitly confirms.
- The PR template in `.github/PULL_REQUEST_TEMPLATE.md` shows contributors the same
  format — keep it in sync with this skill if the convention changes.
