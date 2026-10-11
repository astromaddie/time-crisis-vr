# PR0 source readiness

**EXPERIMENTAL.** Public source for the Steam Frame port; no binary release is included.

- **Current host-tested source:** `a3586d7117507085cdf167ca2190e9fee503d3fe`.
- **Fresh full-build source:** `d662948d098c57122a24a7333126097fcb778a9c`.
- **Pinned reconstruction submodule:** `upstream` at `6aaa90b4cbc7a23733e1c9f5f9a5e772a19fe23d`.
- **Build route:** `./Build-Steam-Frame.sh --rom /path/to/your/timecris.zip --jobs 4`.
- **Toolchain:** Python 3.14.6, OpenJDK 17.0.20.1, Android platform 34, build-tools 35.0.0, NDK 27.2.12479018 r27c, CMake 3.22.1 and Ninja 1.10.2. Pinned public inputs were SDL 2.30.11 and Khronos OpenXR loader 1.1.43.

The current source passed `tests/test_frame_lobby_focus.py`, `tests/test_portable_controls.py` and `tests/test_frame_native_syntax.py`. The focus regression uses production XR events and the real SS22 input backend under AddressSanitizer and UndefinedBehaviorSanitizer, covering focus changes before and after the game attaches. Controls, replay neutralization and Quest/Frame syntax checks passed too. The earlier `f1ae4709` lobby/loopback LAN check remains recorded separately. These host checks do not replace the full-build receipt below.

## Build and package checks

The full Frame build passed from a fresh checkout at the recorded full-build commit with the pinned submodule and an explicitly supplied ROM ZIP. The APK is SHA-256 `1a5e1a6bd0cc69f9686614adbaf4987fff909716bd1c5f0873e9778ea66506a8`, 5,862,600 bytes, package `org.timecrisis.frame`, version `0.8.4-frame1` (1), ABI `arm64-v8a`. APK Signature Scheme v3 verification, `zipalign -c -P 16 4`, archive integrity, and ROM/key exclusion checks passed. The receipt includes dependency, source and artifact hashes; it contains no ROM hash. Build output was removed after verification.

ROM-free production checks passed, including the APK validator, installer and import checks, portable controls, replay, rendering policy, sprite report, UI capture, patch idempotency, and the three production co-op clock/LAN/stall C tests. The C LAN tests used loopback only. `test_frame_native_syntax.py` now resolves compiler source paths before matching them, which handles macOS `/tmp` aliases; its Quest and Frame syntax checks passed after the fix.

The all-tests sweep also exposed host-only gaps: the Windows-specific tests require Windows tool paths or PowerShell; Pillow is unavailable for the two image tests; the Windows package test needs a separately built Windows artifact; the device test could not find `adb` and did not reach a device; and the headless Chrome play-area render exited `-6`. These checks are not reported as passing.

**Runtime observation supplied by the user:** actual Steam gameplay was confirmed with the larger window at 2160 resolution and 120 Hz. That confirms gameplay on the reported setup; this build check did not measure sustained frame timing. A prior automated shutdown check recorded an Android UI surface-callback SIGSEGV, and an earlier rear-facing view check recorded a black image; neither was re-tested here.

## Source and binary rights

The port code and pinned reconstruction retain their original MIT notices, including the upstream copyright (c) 2026 cmcrann. The dependency contains reconstructed engine and generated C source. The repository has no ROM archives, extracted chip dumps or game-data archives, and the checked Frame APK excluded the ROM. Supply the retail ROM separately; source notices do not grant redistribution rights to the original game files.

No APK was uploaded, installed, or launched during this check. The current build receipt is in [PR0_METADATA.json](PR0_METADATA.json). The ROM was removed from the temporary checkout after the build; it is not in the repository or the receipt.
