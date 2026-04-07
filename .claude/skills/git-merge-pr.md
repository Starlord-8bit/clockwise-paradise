---
description: Merge a pull request after CI passes. Verifies all checks are green, squash-merges into main, deletes the branch, and pulls main locally. Use /git-merge-pr <pr-number> when a PR is approved and ready to land.
---

Merge a pull request into main.

## Steps

1. Identify the PR number (from argument or ask):
```bash
gh pr list --state open
```

2. Verify CI checks are all passing:
```bash
gh pr checks <pr-number>
```
If any check is failing or pending: **stop**. Do not merge a red PR. Report the failing
check name and ask the user whether to wait or investigate.

3. Confirm the PR has no unresolved review comments:
```bash
gh pr view <pr-number> --json reviewDecision,reviews
```

4. Squash-merge and delete the branch:
```bash
gh pr merge <pr-number> --squash --delete-branch
```
Squash merge keeps `main` history linear. One PR = one commit on main.

5. Update local main:
```bash
git checkout main && git pull origin main
```

6. Confirm the merge commit appears:
```bash
git log --oneline -3
```

## Notes

- Always squash merge — never create a merge commit on main.
- Branch is deleted remotely after merge. Clean up local branch if present:
  ```bash
  git branch -d <branch-name>
  ```
- If the PR has merge conflicts: do not force. Checkout the branch, rebase onto main,
  resolve conflicts, push, then re-run `/git-merge-pr`.
