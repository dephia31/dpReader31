---
name: dpreader-s31
description: Develop, test, package, or release dpReader firmware for Metalio 4.7 ESP32-S31, including Full and Love Book, ESP-IDF builds, OTA, and Web Flash.
---

Read the repository AGENTS.md first. Use docs/BOARDS.md for the SDK and build setup, docs/FIRMWARE_EDITIONS.md for edition boundaries, and docs/QUY_TRINH_COMMIT_VA_RELEASE.md for packaging and publication. Follow the latest instructions where historical examples differ.

- Edit maintained sources under firmware/shared, firmware/full, firmware/love_book, and boards/metalio47. Use tools/board_source_view.py --board metalio47 --tests to materialize board-specific host tests.
- Build with tools/build_firmware.py --board metalio47 using the matching ESP-IDF 6.1 SDK. Build both editions by default. Verify the embedded versions, chip, partitions, checksums, and USB launchers. Follow AGENTS.md for the next local RC and the Test Build output paths.
- For OTA changes, exercise the firmware parser against actual candidate manifests for both editions. Verify official HTTPS URLs, version and edition selection, hashes, and fallback behavior. Keep RCs off Web Flash and public OTA. Distinguish host tests, package checks, published downloads, and hardware acceptance.
- Treat main as protected. Inspect active repository rules and use a topic branch and pull request for changes. Verify required checks on the PR and follow the repository merge rules. For an authorized release, tag the verified commit on main after merge. Do not bypass branch protection. Flashing hardware and publication require authorization in scope.

## Independent firmware review

For changes with material effects on firmware behavior, persistent data, hardware, or build and update compatibility, proactively delegate independent review to subagents after implementation and self-review, before the final RC build or release verification. Use only the review axes that apply. Small changes and read-only investigations do not require this handoff.

- **Correctness:** Check behavior, regressions, failure paths, and compatibility with existing user data.
- **Architecture and editions:** Check ownership, shared code, board boundaries, and Full/Love Book behavior.
- **Embedded risks:** Check page-turn latency, memory, task contention, power and sleep, and S31 hardware constraints.
- **Delivery and user-facing changes:** Check settings text, documentation, and affected USB/OTA packaging contracts.

Assign one read-only reviewer per applicable axis. Give each reviewer the user requirements, repo path, comparison base, the same fixed diff including relevant uncommitted changes, and existing test results. Ask for findings with file and line, triggering scenario, impact, and evidence. Reviewers inspect code, diffs, history, and recorded checks. They do not edit files, run tests or builds, publish, or flash.

Run reviewers in parallel within the session's slot limit. If needed, dispatch in batches and keep the candidate unchanged until all reviews return. The main agent validates and deduplicates findings, applies fixes, and runs the affected checks and final build. Use at most one targeted follow-up round when a fix invalidates an earlier review. If subagent tools are unavailable, cover the applicable review axes in the main session and report the limitation.
