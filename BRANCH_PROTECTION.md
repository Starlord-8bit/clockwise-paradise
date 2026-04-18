# Main Branch Protection And CI Gates

This note documents the current expected merge gate for `main` and the release flow that follows it.

## Expected protection for `main`

- Require pull requests before merge.
- Require squash merge for change PRs so the PR title becomes the commit title on `main`.
- Require the current PR-time checks from the `Build & Test` workflow:
  - `Version Sync Check`
  - `Native Tests`
  - `ESP-IDF Build (v4.4.7)`
- Do not allow bypassing required checks for normal release work.

## Current workflow caveat

The `Build & Test` workflow in `.github/workflows/build.yml` uses `paths-ignore` for `**.md` and `clockfaces/**` on both `push` and `pull_request`.

That means GitHub does not start the PR-time checks for docs-only or clockface-only pull requests. Because the workflow does not run for those changes, branch protection cannot enforce those three checks on those pull requests today. This document describes the intended gate for normal code changes and the current limitation for ignored paths.

## PR title and squash-merge contract

The pull request template requires a Conventional Commit title. That matters because the repository relies on squash merges, and the squash commit title becomes the commit message on `main`.

`release-please` reads those merged commit messages to decide whether the next release is a major, minor, patch, or no-release change, and it uses the same commit metadata to build the changelog.

Operationally:

- Use a Conventional Commit PR title before merge.
- Keep the squash commit title aligned with that PR title.
- Treat the squash title as the release input for `release-please`.

## Post-merge release flow

1. A pull request merges to `main`.
2. `Release Please` runs on the push to `main`.
3. For normal feature and fix merges, `release-please` updates or opens the release PR.
4. When the release PR is merged, `release-please` creates the tag and GitHub release.
5. The reusable `Release` workflow runs native tests, builds the firmware, and publishes the release assets.

## Workflow mapping

- PR-time gate: `Build & Test` in `.github/workflows/build.yml`
- Reusable native test workflow: `Native Test Workflow` in `.github/workflows/test-native.yml`
- Post-merge release orchestration: `Release Please` in `.github/workflows/release-please.yml`
- Release asset build and publish: `Release` in `.github/workflows/release.yml`
