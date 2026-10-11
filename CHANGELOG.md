# Changelog

## 2026-10-11 — Experimental Steam Frame source prep

- Replaced the root landing page with the source build route; preserved the former README in docs/README_PRE_PR0.md.
- Recorded the current clean ARM64 APK build, package checks, host-test outcomes and pinned-submodule source notices in docs/PR0_READINESS.md and docs/PR0_METADATA.json.
- Made the native syntax test compare resolved compiler paths so a clean macOS checkout handles `/tmp` path canonicalization.
- Removed personal machine defaults from the Frame build and install paths; ROM-free importer tests use a synthetic manifest and archive.

## 2026-10-11 — public source preparation

- Clarified the ROM-free Frame source/package boundary and retained the upstream reconstruction notice.

- Fixed startup focus handling so the native lobby does not call the game input backend before it is initialized; added a real-backend sanitizer regression.

- Clarified that multiplayer and the new startup lobby are unfinished development on the same branch; confirmed gameplay notes refer to solo play.
