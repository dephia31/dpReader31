---
name: dpreader-s31
description: Develop, test, package, or release dpReader firmware for Metalio 4.7 ESP32-S31, including Full and Love Book, ESP-IDF builds, OTA, and Web Flash.
---

Read the repository AGENTS.md first. Use docs/BOARDS.md for the SDK and build setup, docs/FIRMWARE_EDITIONS.md for edition boundaries, and docs/QUY_TRINH_COMMIT_VA_RELEASE.md for packaging and publication. Follow the latest instructions where historical examples differ.

- Edit maintained sources under firmware/shared, firmware/full, firmware/love_book, and boards/metalio47. Use tools/board_source_view.py --board metalio47 --tests to materialize board-specific host tests.
- Build with tools/build_firmware.py --board metalio47 using the matching ESP-IDF 6.1 SDK. Build both editions by default. Verify the embedded versions, chip, partitions, checksums, and USB launchers. Follow AGENTS.md for the next local RC and the Test Build output paths.
- For OTA changes, exercise the firmware parser against actual candidate manifests for both editions. Verify official HTTPS URLs, version and edition selection, hashes, and fallback behavior. Keep RCs off Web Flash and public OTA. Distinguish host tests, package checks, published downloads, and hardware acceptance.
- Treat main as protected. Inspect active repository rules and use a topic branch and pull request for changes. Verify required checks on the PR and follow the repository merge rules. For an authorized release, tag the verified commit on main after merge. Do not bypass branch protection. Flashing hardware and publication require authorization in scope.
