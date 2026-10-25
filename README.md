# kiikoread · Metalio S31

Publicly available kiikoread firmware sources adapted for **Metalio E-INK4 Plus / ESP32-S31**, specifically **V1.5, 34-pin E0470A14-AF** hardware. This is an independent adaptation, not an official Metalio release.

[简体中文](README.zh-CN.md) · [日本語](README.ja-JP.md) · [Web installer](https://kiikoread.com/?device=metalio#flash-action) · [Pico project](https://github.com/wegooo-cell/read-pico-reader)

## Current official release: rc104

- [Web installer and version selection](https://kiikoread.com/?device=metalio&version=rc104#flash-action)
- [rc104 application, TF/USB packages and SHA-256](https://github.com/wegooo-cell/read-pico-reader/releases/tag/metalio-s31-v0.3.3-rc104)
- [Changelog](docs/CHANGELOG.md) · [Board and build documentation](docs/BOARDS.md)

Official binaries remain archived in the shared release repository; this repository maintains Metalio S31 sources. Older test Releases here are previews. Metalio and Pico use separate chip targets, power drivers, waveforms and partitions; their images are not interchangeable.

## Features

- Home, bookshelf, file manager and settings; EPUB/TXT reading and embedded images.
- Imported TTF/OTF system and reading fonts, a128-entry catalogue, weight and antialiasing controls.
- S31 warm/cool frontlight, fine low-brightness adjustment, vibration, AI-key shortcuts and BLE page turners.
- Wi-Fi, hotspot and USB file transfer, browser flashing and online OTA with GitHub Pages preferred and the website as fallback.
- Reading tickets, wallpaper, cover collages and transparent-cover locks, PIN verification and reader resume.
- rc103 provides two shutdown choices: default light sleep followed by deep sleep after ten minutes, or complete power-off. The separate lock-power selector is removed. Overnight battery use and deep-wake behavior still require hardware validation.

## Install and update

Open the [official installer](https://kiikoread.com/?device=metalio#flash-action) in desktop Chrome or Edge and select **Metalio EINK 4 Plus**.

**First installation requires restoring the official Metalio firmware and entering BOOT from that firmware.** A compatible existing kiikoread base can update directly. The installer defaults to preserving settings, offers optional data clearing, retains TF files and creates no pre-flash backup.

Use the Release's `Metalio-S31-update.bin` for a TF update from device settings. An application Bin requires the compatible S31 dual-slot base; never write it directly to `0x0`. Use the website or matching USB package for a complete installation.

## Build

The shared `main/` reader/UI is selected with the hardware backend in `boards/metalio47/`. Use the matching ESP32-S31 SDK, toolchain and locked managed components. See [docs/BOARDS.md](docs/BOARDS.md) for complete commands and the documented waveform/fallback boundaries. The rc104 build configuration is shown in the [Chinese build guide](README.zh-CN.md#构建).

Keep the raw GC48/GL48/DU20 tables and board power sequencing. Validate chip, project, version, partitions and checksums before using an image.

## License and contributions

[LICENSE](LICENSE), [third-party notices](THIRD_PARTY_NOTICES.md) and per-file identifiers retain the same scope as the Pico project: existing Apache-2.0 and third-party rights remain; explicitly marked new original material follows the Personal Study License, with commercial use requiring the relevant rights holder's permission.

Use this repository's Issues and Pull Requests for S31 contributions. Include the firmware version, board revision and reproduction steps. Hardware and vendor references: [CloudZao/Metalio-E-INK4-Plus](https://github.com/CloudZao/Metalio-E-INK4-Plus).
