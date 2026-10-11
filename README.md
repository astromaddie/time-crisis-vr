# Time Crisis VR for Steam Frame — experimental source

**Upstream VR port: [DR-89/time-crisis-vr](https://github.com/DR-89/time-crisis-vr).** This repository is the [astromaddie Steam Frame fork](https://github.com/astromaddie/time-crisis-vr/tree/steam-frame), which builds a standalone ARM64 APK for SteamVR through the Frame's Lepton container, with persistent storage and Frame controller bindings. Its `upstream/` git submodule points to [spacestate1/namco22-decompile](https://github.com/spacestate1/namco22-decompile) at `6aaa90b4cbc7a23733e1c9f5f9a5e772a19fe23d`.

**Status: EXPERIMENTAL, personal use only.** This source-preparation task does not change repository visibility or publish a release. The prior tested runtime source is `3e6077444f9a293a2fa76263dbc9267c8955dfe8`; the source-preparation baseline was `8cd6738886bac9678cc2f0aa5ce32a9410b3654f`, whose intervening changes only updated evidence and delivery documentation.

| Area | Evidence and limit |
|---|---|
| Host build and production-path tests | The tested APK was built from `3e60774`; source/importer, packaging and installer checks are separate from headset acceptance. ROM-free host tests are listed in [PR0 readiness](docs/PR0_READINESS.md). |
| Install, startup, storage and stereo | Earlier candidate install, Steam launch, persistent ROM import, eye captures, audio, grip cover/reload and Menu pause/resume have evidence. These do not cover the entire focused wear checklist. |
| Controls and tracking | Both hands, physical ducking, View gestures, full tracking/recenter behavior and comfort remain pending for focused acceptance. |
| Shutdown | A bounded self-exit reproduced an Android UI thread `SIGSEGV` in surface callback handling; root cause is unresolved. Container exit status 137 does not show graceful shutdown. |
| Stereo and performance | Rear-facing yaw 180° captures are black. A quiet-scene multiview comparison showed visual parity only. Sustained frame rate and actual GPU/compositor performance are unproven. |
| Source rights | The pinned reconstruction submodule contains large generated C translations for multiple commercial games. Rights to publish those translations or a combined binary have not been established; its MIT notice does not resolve this. See [NOTICE.md](NOTICE.md). |

## Build from a fresh checkout

The Frame build needs Python 3, JDK 17, Android SDK platform 34 and build-tools 35.0.0, NDK 27.2.12479018, CMake and Ninja. It downloads the pinned SDL/OpenXR build dependencies. Provide a legally obtained, matching Time Crisis World TS2 Ver.B ROM ZIP explicitly; the APK produced by this route omits ROM files.

```sh
git clone --recurse-submodules --branch steam-frame https://github.com/astromaddie/time-crisis-vr.git
cd time-crisis-vr
./Build-Steam-Frame.sh --rom /path/to/your/timecris.zip --jobs 4
```

The submodule's generated source and rights status are an unresolved source-publication blocker. This repo's MIT license covers only the port code credited there; it does not grant rights to the arcade program, reconstruction, ROM, artwork, audio or generated commercial-game translations. Do not treat this README or the build command as a release authorization.

Focused headset acceptance still needs installation, real controller input, tracking, both-eye stereo, menus, gameplay, pause/resume, persistence across relaunch, clean exit, and actual performance measurement. The existing [architecture and evidence record](docs/VR_ARCHITECTURE.md) distinguishes host checks from worn/device evidence.

The previous root README is preserved in [docs/README_PRE_PR0.md](docs/README_PRE_PR0.md). Exact source identity, ROM-free checks and open gates are in [docs/PR0_READINESS.md](docs/PR0_READINESS.md).
