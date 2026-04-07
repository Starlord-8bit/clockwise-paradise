---
description: Walk through the full release checklist: version bump, CHANGELOG update, tag creation, and GitHub release. Use /git-go-for-release when main is in a releasable state and a new version needs to be published.
---

Cut a new Clockwise Paradise release.

## Steps

### 1 — Confirm you are on main and it is clean
```bash
git checkout main && git pull origin main
git status
```
If not clean: stop. All changes must be committed and merged before releasing.

### 2 — Determine the new version

Ask the user for the version number if not provided. Use semantic versioning:
- **patch** (x.y.Z): bug fixes only
- **minor** (x.Y.0): new features, backwards compatible
- **major** (X.0.0): breaking changes

Current version is in `firmware/platformio.ini` under `build_flags`:
```bash
grep "CW_FW_VERSION" firmware/platformio.ini
```

### 3 — Update version strings

Update in `firmware/platformio.ini`:
```
-D CW_FW_VERSION="<old>"
+D CW_FW_VERSION="<new>"
```

Verify no other hardcoded version strings need updating:
```bash
grep -rn "CW_FW_VERSION\|v[0-9]\+\.[0-9]\+\.[0-9]\+" --include="*.ini" --include="*.cmake" --include="CMakeLists.txt" .
```

### 4 — Update CHANGELOG.md

Add a new section at the top of `CHANGELOG.md`:
```markdown
## [vX.Y.Z] — YYYY-MM-DD

### Added
- ...

### Fixed
- ...

### Changed
- ...
```

Pull the entries from `git log <previous-tag>..HEAD --oneline`.

### 5 — Commit the release prep
```bash
git add firmware/platformio.ini CHANGELOG.md
git commit -m "chore: bump version to vX.Y.Z

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>"
git push origin main
```

### 6 — Confirm CI is green on main
```bash
gh run list --branch main --limit 3
```
Wait for the build workflow to pass before tagging.

### 7 — Create and push the tag
```bash
git tag -a vX.Y.Z -m "Release vX.Y.Z"
git push origin vX.Y.Z
```

### 8 — Create GitHub Release
```bash
gh release create vX.Y.Z \
  --title "Clockwise Paradise vX.Y.Z" \
  --notes "$(sed -n '/## \[vX.Y.Z\]/,/## \[/p' CHANGELOG.md | head -n -1)"
```

The CI `clockwise-ci.yml` workflow will trigger on the tag and build clockface artifacts
for the release. Verify it completes:
```bash
gh run list --limit 5
```

### 9 — Report

Return to the user:
- Tag: `vX.Y.Z`
- GitHub Release URL
- CI run URL for the artifact build

## Notes

- Never tag a commit that CI has not validated.
- If the tag push triggers a failing workflow: do not delete the tag silently. Report to
  the user and diagnose the failure.
- Patch releases should not introduce new NVS keys (would require migration for existing devices).
