# PR0 source readiness

**EXPERIMENTAL.** Public source for the Steam Frame port; no binary release is included.

The `steam-frame` branch is solo-only and starts the game directly after ROM setup. Incomplete multiplayer is on the separate `steam-frame-multiplayer` branch; see [LAN-COOP.md](LAN-COOP.md) for the paired-play result and acceptance requirement.

- **Solo production baseline:** `ae6032f` (the source state restored for this branch).
- **Historical multiplayer build source:** `d662948d098c57122a24a7333126097fcb778a9c`; its APK and checks below do not validate the current solo source.
- **Pinned reconstruction submodule:** `upstream` at `6aaa90b4cbc7a23733e1c9f5f9a5e772a19fe23d`.
- **Build route:** `./Build-Steam-Frame.sh --rom /path/to/your/timecris.zip --jobs 4`.
- **Toolchain:** Python 3.14.6, OpenJDK 17.0.20.1, Android platform 34, build-tools 35.0.0, NDK 27.2.12479018 r27c, CMake 3.22.1 and Ninja 1.10.2. Pinned public inputs were SDL 2.30.11 and Khronos OpenXR loader 1.1.43.

The restored solo source passed the production startup-path test, portable controls, timing/projection/aiming/sprite reporting, guide geometry, replay, synthetic ROM import, and HUD capture. After refreshing the pinned generated upstream view and applying the solo patches, Quest/Frame native syntax and upstream patch reproducibility also passed. The compiler-path fix from `5951e3f` is retained.

## Build and package checks

The prior full Frame build passed from a fresh checkout at the historical multiplayer source commit above. Its APK was SHA-256 `1a5e1a6bd0cc69f9686614adbaf4987fff909716bd1c5f0873e9778ea66506a8`, 5,862,600 bytes, package `org.timecrisis.frame`, version `0.8.4-frame1` (1), ABI `arm64-v8a`. APK Signature Scheme v3 verification, alignment, archive integrity, and ROM/key exclusion checks passed for that artifact. The restored solo source was then built cleanly at `d571dddf8b9be4f643d2da084210fdfcbf42c322`. Its ROM-free APK is SHA-256 `d63fc9a88d9e32dde1e5e66d1a6fd4248afb53f373dba5d8e75e2f44aedcf390`, 5,842,120 bytes. Signature, 16 KiB alignment, ARM64 dependency closure, APK content and production startup checks passed. All nine runtime archive members are byte-identical to the previously wearer-accepted large-window solo APK; signature metadata and license notices were excluded from that comparison.

The current solo checks cover the real Frame controls and fallback profile, arcade timing and projection, aiming, replay behavior, ROM import and rendering paths. The APK validator now rejects stale multiplayer Java classes and identity metadata; it passed against the rebuilt solo package.

An earlier all-tests sweep exposed host-only gaps: the Windows-specific tests require Windows tool paths or PowerShell; Pillow was unavailable for two image tests; the Windows package test needed a separately built Windows artifact; the device test could not find `adb` and did not reach a device; and the headless Chrome play-area render exited `-6`. These checks are not reported as passing.

**Runtime observation supplied by the user:** actual Steam gameplay was confirmed with the larger window at 2160 resolution and 120 Hz. That confirms gameplay on the reported setup; this build check did not measure sustained frame timing. A prior automated shutdown check recorded an Android UI surface-callback SIGSEGV, and an earlier rear-facing view check recorded a black image; neither was re-tested here.

## Source and binary rights

The port code and pinned reconstruction retain their original MIT notices, including the upstream copyright (c) 2026 cmcrann. The dependency contains reconstructed engine and generated C source. The repository has no ROM archives, extracted chip dumps or game-data archives, and the checked Frame APK excluded the ROM. Supply the retail ROM separately; source notices do not grant redistribution rights to the original game files.

No APK was uploaded as a public binary release. The rebuilt solo APK is prepared for local installation; fresh device verification is tracked separately from the build checks. The prior multiplayer-build receipt is retained in [PR0_METADATA.json](PR0_METADATA.json) as historical evidence. No ROM, extracted chip, APK, or signing key is included in this source tree.
