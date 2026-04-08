---
description: Trigger or monitor the release pipeline for Clockwise Paradise. The normal path is merging the release-please PR — release-please handles versioning, tagging, and changelog automatically. Use /git-go-for-release to check release readiness or kick off an emergency manual release.
---

Cut a new Clockwise Paradise release.

## How releases work

Releases are fully automated via release-please:

1. PRs are merged to `main` with Conventional Commit titles (`feat:`, `fix:`, etc.)
2. release-please opens/updates a **Release PR** on every push to main, accumulating
   changes and proposing the next version bump
3. When the Release PR is merged → release-please creates the git tag on main
4. The tag push triggers `release.yml` → native tests → ESP-IDF build → GitHub Release published

**The only human action required is merging the Release PR.**

---

## Normal path — merge the Release PR

### 1 — Check for an open release-please PR
```bash
gh pr list --label "autorelease: pending" --base main
```
If none exists: there are no releasable changes on main since the last release.
Report this to the user and stop.

### 2 — Review the proposed release
```bash
gh pr view <pr-number>
```
Confirm the proposed version bump and changelog look correct.
If the version bump seems wrong, check whether the PR titles on recent commits used the
right Conventional Commit types.

### 3 — Confirm CI is green on main
```bash
gh run list --branch main --limit 5
```
If any build or test run is failing: do not merge. Report to the user.

### 4 — Merge the Release PR
```bash
gh pr merge <pr-number> --squash --delete-branch
```

### 5 — Monitor the release pipeline
```bash
gh run list --branch main --limit 5
```
Wait for `release.yml` to complete. Verify the GitHub Release was created:
```bash
gh release list --limit 3
```

### 6 — Report back
Return to the user:
- Version released
- GitHub Release URL (`gh release view <tag> --web`)

---

## Emergency path — manual release from an existing tag

Only use this if the automated pipeline failed after a tag was already created and you
need to re-run the release job without re-tagging.

```bash
gh workflow run release.yml \
  --field tag=<existing-tag> \
  --field prerelease=false
```

Monitor:
```bash
gh run list --workflow=release.yml --limit 3
```

---

## Notes

- Never manually create tags or push them — release-please owns tagging.
- Never manually edit `CHANGELOG.md` or version files — release-please manages these.
- If the proposed version bump is wrong, the fix is in the PR titles, not in this skill.
  Conventional Commit types determine the bump: `feat` → minor, `fix` → patch, `feat!` → major.
- Patch releases should not introduce new NVS keys (would require migration on existing devices).
