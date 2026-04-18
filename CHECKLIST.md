# Release Checklist

This checklist reflects the current Clockwise Paradise release flow using release-please and GitHub Actions.

## Before merge to main

- [ ] Confirm the PR follows the `main` gate in `BRANCH_PROTECTION.md`
- [ ] Native tests pass locally: `make test`
- [ ] Firmware build passes locally: `make build`
- [ ] Version alignment verified: `version.txt` and `firmware/platformio.ini` `CW_FW_VERSION` match
- [ ] README updated for any user-facing behavior/config changes
- [ ] CHANGELOG reviewed (release-please entries look correct)
- [ ] Security review done for any new endpoint/settings touching credentials

## After merge to main

- [ ] `Release Please` PR is created/updated automatically
- [ ] Review and merge release PR
- [ ] Confirm `.github/workflows/release.yml` completed successfully
- [ ] Confirm release assets exist:
  - [ ] `clockwise-paradise.<tag>.bin`
  - [ ] `bootloader.bin`
  - [ ] `partition-table.bin`
- [ ] Flash-test the released app binary on a real ESP32 device

## Optional post-release validation

- [ ] OTA update from previous version succeeds via `/ota/update`
- [ ] Manual OTA upload via `/ota/upload` succeeds
- [ ] Home Assistant MQTT discovery still creates expected entities
