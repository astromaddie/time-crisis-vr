# PR0 source readiness

**State: EXPERIMENTAL, personal-use source preparation only.** Repository visibility and release state are unchanged.

- **Baseline source commit before this preparation:** 8cd6738886bac9678cc2f0aa5ce32a9410b3654f.
- **Prior runtime source for the tested Frame APK:** 3e6077444f9a293a2fa76263dbc9267c8955dfe8. Source after that APK changed evidence/delivery documentation only; this prep removes personal-machine defaults from build/test entry points.
- **Pinned reconstruction submodule:** upstream → https://github.com/spacestate1/namco22-decompile, commit 6aaa90b4cbc7a23733e1c9f5f9a5e772a19fe23d.
- **Target:** standalone ARM64 Android app inside Steam Frame Lepton. Existing Quest and Windows paths remain in the repository but are not covered by this Frame prep.
- **Full build prerequisites:** Python 3, JDK 17, Android platform 34, build-tools 35.0.0, NDK 27.2.12479018, CMake, Ninja and network access to fetch pinned SDL/OpenXR inputs. The build also needs the matching user-owned Time Crisis World TS2 Ver.B ROM ZIP explicitly supplied with --rom; the APK omits ROM files.
- **Build route:**

  ~~~sh
  ./Build-Steam-Frame.sh --rom /path/to/your/timecris.zip --jobs 4
  ~~~

  This source route is not a release authorization. License scope and unresolved generated-source rights are described in [NOTICE.md](../NOTICE.md).

The immutable PR0 test source commit, supported targets and final check results are in [PR0_METADATA.json](PR0_METADATA.json).

## Evidence

| Surface | Evidence |
|---|---|
| Prior full build/package | APK built from source 3e60774; existing record covers source, ARM64 build, Java packaging, signature/alignment and package checks. This PR0 pass does not rebuild the full APK. |
| Worn/device | Steam launch, persistent ROM import, eye captures, audio, grip cover/reload and Menu pause/resume have prior evidence. Current full checklist is incomplete. |
| Shutdown | Bounded self-exit reproduced an Android UI surface-callback SIGSEGV; root cause is unresolved. Container stop status 137 is not clean-exit evidence. |
| Fresh isolated ROM-free checks | Passed in a fresh local clone with temporary HOME and Git config disabled. Exact commands are below. The synthetic archive is test data, not a game ROM. |

## Blockers and pending focused acceptance

The pinned reconstruction dependency's history/tree includes large generated C translations for multiple commercial games. Rights to publish those translations or a combined binary have not been established; its upstream MIT notice does not resolve the scope. This is a material source-publication blocker, not covered by the port's MIT license.

The rear-facing yaw 180° view is black; quiet-scene multiview parity does not prove performance. Sustained frame rate and GPU/compositor timing are unproven. Focused headset acceptance remains pending for install, real controller input, tracking, both-eye stereo, menus, gameplay, pause/resume, persistent data across relaunch, clean exit and actual performance. The existing exit crash must be resolved or clearly accepted as a known failure before claiming release readiness. No device action is part of this PR0 preparation.

The ROM-free checks ran from an isolated local clone of the reviewed checkout with a temporary HOME, global/system Git configuration disabled and terminal/SSH prompts disabled. The full Android native build and APK packaging were not rerun; they require the Android SDK/NDK and an explicitly supplied matching ROM. The native syntax test that consumes generated build commands is therefore not claimed as fresh-checkout evidence.

Commands run from the isolated clone:

~~~sh
python3 tests/test_frame_installer.py
python3 tests/test_portable_import.py
python3 -m py_compile tests/test_frame_apk.py tests/test_frame_installer.py \
  tests/test_frame_native_syntax.py tests/test_portable_controls.py \
  tests/test_portable_import.py tests/test_sdl_clipboard_patch.py \
  tests/test_ui_capture.py tools/build.py tools/build_frame.py \
  tools/build_pc.py tools/host_paths.py tools/steam_frame/bundle/install.py
sh Build-Steam-Frame.sh --help
~~~

`python3 tools/build_frame.py` without `--rom` was also checked to fail with the expected explicit-ROM usage error before starting a build. No matching `timecris.zip` was found by the indexed filename check, so a current full APK build was not attempted.

The previous root README is preserved in [README_PRE_PR0.md](README_PRE_PR0.md). Detailed source and device evidence remain in [VR_ARCHITECTURE.md](VR_ARCHITECTURE.md).
