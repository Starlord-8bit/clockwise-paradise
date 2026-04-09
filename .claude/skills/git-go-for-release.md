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

## Manual release path — workflow_dispatch

Use this when you want to cut a release directly from main without going through the
release-please Release PR flow (e.g. urgent hotfix, or release-please not yet set up).

**Do NOT pre-create the tag.** `release.yml` creates and pushes the tag itself after
the build succeeds. If the tag already exists the workflow will fail at the tag step.

```bash
gh workflow run release.yml \
  --repo Starlord-8bit/clockwise-paradise \
  --field tag=vX.Y.Z \
  --field prerelease=false
```

The workflow will:
1. Check out main
2. Run native tests
3. Build firmware inside the ESP-IDF Docker container
4. Create and push the tag `vX.Y.Z` on main
5. Publish the GitHub Release with versioned `.bin` assets

Monitor:
```bash
gh run list --workflow=release.yml --repo Starlord-8bit/clockwise-paradise --limit 3
```

---

## Notes

- Never manually create tags before triggering `release.yml` via workflow_dispatch —
  the workflow creates the tag after a successful build. Pre-creating it will cause
  the workflow to fail.
- On the automated path (push:tags from release-please), the tag already exists and
  the workflow skips tag creation — these are two different code paths.
- Never manually edit `CHANGELOG.md` or version files — release-please manages these.
- If the proposed version bump is wrong, the fix is in the PR titles, not in this skill.
  Conventional Commit types determine the bump: `feat` → minor, `fix` → patch, `feat!` → major.
- Patch releases should not introduce new NVS keys (would require migration on existing devices).
