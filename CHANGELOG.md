# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).


## [3.1.0](https://github.com/Starlord-8bit/clockwise-paradise/compare/v3.0.0...v3.1.0) (2026-04-09)


### Features

* **ota:** automatic rollback guard — device cannot be bricked by OTA ([9ddc1ae](https://github.com/Starlord-8bit/clockwise-paradise/commit/9ddc1ae557ff8a44ccd3058c97fe12dc19dc652d))
* **ota:** automatic rollback guard — device cannot be bricked by OTA ([9ddc1ae](https://github.com/Starlord-8bit/clockwise-paradise/commit/9ddc1ae557ff8a44ccd3058c97fe12dc19dc652d))
* **ota:** automatic rollback guard — device cannot be bricked by OTA ([288acb5](https://github.com/Starlord-8bit/clockwise-paradise/commit/288acb590a91df4988618500a0b57ed6a3c3c32f))


### Bug Fixes

* **ci:** release workflow — checkout main before tag exists, create tag after build ([88cda95](https://github.com/Starlord-8bit/clockwise-paradise/commit/88cda952104aee48d7065781272bdefa4cfd81d5))
* define CWDriverRegistry::REGISTRY — null deref bootloop after DMA setup ([#21](https://github.com/Starlord-8bit/clockwise-paradise/issues/21)) ([ad88b44](https://github.com/Starlord-8bit/clockwise-paradise/commit/ad88b44c97ae8688ba9855acddd22bcf2fcdfa60))
* guard DMA begin() failure and cwDateTime access before NTP sync ([d263568](https://github.com/Starlord-8bit/clockwise-paradise/commit/d26356840e4b5ea3a9805f24caa5f53878a47c27))
* overhaul CI/CD — release-please automation, remove legacy workflows ([#20](https://github.com/Starlord-8bit/clockwise-paradise/issues/20)) ([0e36f99](https://github.com/Starlord-8bit/clockwise-paradise/commit/0e36f9900241b1ba7c0fc0c7dd56fc26ca8b887b))
* register dma_display with Locator before first StatusController call ([#23](https://github.com/Starlord-8bit/clockwise-paradise/issues/23)) ([1d0408f](https://github.com/Starlord-8bit/clockwise-paradise/commit/1d0408f5be369717e7c43cf0e24732c398e54559))
* **test:** add missing CMakeLists.txt for native test build ([b033690](https://github.com/Starlord-8bit/clockwise-paradise/commit/b0336903983bd7526c7e8e69f6a156d76e25d20d))

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
