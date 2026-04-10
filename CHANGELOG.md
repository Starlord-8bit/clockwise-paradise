# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).


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
