# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).


## [2.9.0](https://github.com/Starlord-8bit/clockwise-paradise/compare/v2.7.0...v2.9.0) (2026-04-11)


### Features

* add workspace coding-guidelines skill for structured firmware implementation
* expand native tests with night-window, URL decode, and OTA version-normalization coverage


### Bug Fixes

* seed random mode to avoid repeat clockface sequences after reboot
* harden `brightMethod` handling and compatibility mapping for `autoBrightEn`
* reduce NVS wear with targeted writes for uptime and clockface index updates


### CI

* add build workflow concurrency cancellation
* extract reusable native test workflow and reuse it in build/release pipelines
* enforce version sync check between `version.txt` and `firmware/platformio.ini`


### Documentation

* update README feature docs and roadmap status
* refresh release checklist to match release-please based flow


## [2.7.0](https://github.com/Starlord-8bit/clockwise-paradise/compare/v2.6.2...v2.7.0) (2026-04-10)


### Features

* live brightness/24h callbacks, version.txt single source of truth ([583470b](https://github.com/Starlord-8bit/clockwise-paradise/commit/583470b1150f814c30472eee3f4f52176cf25608))
* **test:** add hardware integration test skill and runner ([8c43213](https://github.com/Starlord-8bit/clockwise-paradise/commit/8c432130ddb839aaa5e532f98c221d0d42c140eb))


### Bug Fixes

* **build:** drop --user from docker run, handle root-owned build/compile in clean ([2634962](https://github.com/Starlord-8bit/clockwise-paradise/commit/2634962bb4646db530936511acc328cdf8af9809))
* **ci:** wire release-please → release.yml via workflow_call ([283953b](https://github.com/Starlord-8bit/clockwise-paradise/commit/283953b5f6b361d6210a7d348eef0b9e8d348cb3))
* **test:** auto-load .env so script works standalone without sourcing ([ca449ac](https://github.com/Starlord-8bit/clockwise-paradise/commit/ca449acd48931d5eb4b50ad31b0f2ff47e25b0e5))
* **test:** handle non-JSON responses in http_get ([6a045dd](https://github.com/Starlord-8bit/clockwise-paradise/commit/6a045dd766dcb10761d3593f56fe5dc89c307678))
* **test:** read version from version.txt instead of git tag ([a145890](https://github.com/Starlord-8bit/clockwise-paradise/commit/a1458902d5bc09bab86694c947c639255f248162))

## [2.6.2](https://github.com/Starlord-8bit/clockwise-paradise/compare/v2.6.1...v2.6.2) (2026-04-10)


### Bug Fixes

* **ci:** migrate release-please action to non-deprecated googleapis org ([40983dc](https://github.com/Starlord-8bit/clockwise-paradise/commit/40983dc6cb7b600d888429082c5c8de9e6499827))
* register dma_display with Locator before first StatusController call ([#23](https://github.com/Starlord-8bit/clockwise-paradise/issues/23)) ([1d0408f](https://github.com/Starlord-8bit/clockwise-paradise/commit/1d0408f5be369717e7c43cf0e24732c398e54559))

## [Unreleased]

### Changed

- Avoid use extensive of String in CWWebServer.cpp


## [1.4.2] - 2024-04-21

### Added

- New Display Rotation param. Thanks @Xefan.
- Added ntp sync in main loop. Thanks @vandalon
- Add api documentation
- Pacman clockface: Add library.json to import using platformio
- Canvas: Feature/add sprite loop and frame delay. Thanks @robegamesios


## [1.4.1] - 2023-08-27

### Added

- New Manual Posix param to avoid the `timezoned.rop.nl` ezTime's timezone service. Thanks @JeffWDH!

### Changed

- Set `time.google.com` as a default NTP server - `pool.ntp.org` is a slug

### Fixed

- A bug in the Canvas clockface - Commit a216c29c4f15b1b3cadbd89805d150c2f551562b


## [1.4.0] - 2023-07-01

### Added

- Canvas clockface
- Created a method to make use of the ezTime formating string
- Possibility to change Wifi user/pwd via API (must be connected)
- A helper to make HTTP requests

### Changed

- RGB icons used in the startup, it was replaced by one-bit images that reduce used flash


## [1.3.0] - 2023-06-11

### Added
- Configure the NTP Server
- Firmware version displayed on settings page
- LDR GPIO configuration
- Added a link in Settings page to read any pin on ESP32 (located in LDR Pin card)

### Changed

- [ABC] It's possible to turnoff the display if the LDR reading < minBright


## [1.2.0] - 2023-05-14

### Added

- Automatic bright control using LDR
- Restart if offline for 5 minutes

### Fixed

- Clockface 0x06 (Pokedex): show AM PM only when is not using 24h format
- Restart endpoint returns HTTP 204 before restarting


## [1.1.0] - 2023-04-02

### Added

- Implements Improv protocol to configure wifi
- Create a Settings Page where user can set up:
  - swap blue/green pins
  - use 24h format or not
  - timezone
  - display bright

### Removed

- Unused variables in main.cpp
