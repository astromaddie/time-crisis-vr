# Changelog

## 2026-10-11 — Experimental Steam Frame source prep

- Replaced the root landing page with the source build route; preserved the former README in docs/README_PRE_PR0.md.
- Recorded the current clean ARM64 APK build, package checks, host-test outcomes and exact pinned-submodule rights question in docs/PR0_READINESS.md and docs/PR0_METADATA.json.
- Made the native syntax test compare resolved compiler paths so a clean macOS checkout handles `/tmp` path canonicalization.
- Removed personal machine defaults from the Frame build and install paths; ROM-free importer tests use a synthetic manifest and archive.
