# Changelog

## 2026-10-11 — Solo Steam Frame branch restored

- Removed the incomplete LAN backend and multiplayer launch lobby from `steam-frame`; multiplayer remains on `steam-frame-multiplayer`.
- Restored direct solo startup while retaining the accepted Frame rendering, controls, storage and build behavior.
- Recreated Java and dex output directories on packaging so stale lobby classes cannot enter an incremental solo APK.
- Retained the compiler-path resolution fix for macOS `/tmp` canonicalization.

## 2026-10-11 — public source preparation

- Replaced the root landing page with the source build route; preserved the former README in docs/README_PRE_PR0.md.
- Recorded source publication, package-boundary and pinned-submodule notices in docs/PR0_READINESS.md and docs/PR0_METADATA.json.
- Clarified the ROM-free Frame source/package boundary and retained the upstream reconstruction notice.
