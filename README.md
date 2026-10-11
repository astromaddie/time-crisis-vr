# Time Crisis VR for Steam Frame

This is my Steam Frame fork of [DR-89's Time Crisis VR](https://github.com/DR-89/time-crisis-vr), built on the [Namco System 22 reconstruction](https://github.com/spacestate1/namco22-decompile). It runs on the headset itself through Lepton and SteamVR OpenXR, as an Android ARM64 app.

**EXPERIMENTAL.** I made this for personal use. Bring your own matching **Time Crisis World TS2 Ver.B** ROM archive (`timecris.zip`). The Frame build makes a ROM-free APK and imports your game files separately.

I've played it on my Frame at **2160×2160 per eye with SteamVR set to 120 Hz**. The gun angle, opening train explosions, audio, grip cover/reload and Menu pause/resume are working. I've also tried the larger gameplay window and darker surround, and I'm happy with how they look.

## What this fork adds

- Standalone Frame support, with its own Android package, OpenXR loader and controller bindings.
- A larger gameplay window: the default is 60% wider and taller than the original port's view, filling more of the lenses without changing the headset's field of view.
- A full cyan frame around the original arcade view, with the scenery outside it darkened by 65%. It makes the intended shooting area easier to see while keeping the game and HUD bright.
- Corrected depth for world effects such as the opening train's crate explosions, so they sit with the scenery instead of floating over it.
- Either-hand gun aiming and firing, grip-operated cover and reload, and physical ducking as an option.
- Adjustable gun angle and default weapon hand. I use **−15°** on my Frame; you can set what suits you in the options.
- View-button recentring and standing-height calibration, plus haptics and a laser aiming aid.
- Persistent ROM files, options and progress under `~/Documents/TimeCrisisVR`, outside Lepton's disposable app storage.

The original VR port supplies the reconstructed stereo world, gun rendering and arcade gameplay. The arcade simulation keeps its original rate of about 60 Hz while the headset follows SteamVR's display rate.

Steam Frame and Quest 3 can also share a fresh local Wi-Fi run. See the [LAN co-op guide](docs/LAN-COOP.md) for lobby behavior, network ports, compatibility, build commands, and current verification limits.

## Controls

| Input | Action |
| --- | --- |
| Either trigger | Use that hand; fire, start or confirm |
| Either grip | Hold to leave cover; release both to hide and reload |
| Right A | Add credits |
| Right B | Toggle laser |
| Right Menu | Pause/options or resume |
| Left View, short press | Back from options |
| Left View, hold 1 second | Recentre |
| Left View, hold 3 seconds | Recentre and calibrate upright height |
| Right X, in options | Switch grip cover / physical ducking |
| Right stick up/down, in options | Adjust gun angle |
| Right stick click, in options | Change default weapon hand |
| Left stick click, in options | Reset gun angle |

A and B keep their roles whichever hand holds the gun. Launch **TimeCrisisVR from the Steam library** so SteamVR supplies the controllers and per-app display settings. Quit through the SteamVR menu.

## Display settings

The default eye target is **2160×2160**. Set the title's refresh rate in SteamVR's per-app settings; the Frame build follows that rate. My play sessions at 120 Hz have felt smooth, but the recorded timings include occasional slow frames.

`frame-display.cfg` in the persistent game folder can change the eye target, window size and outside shading. Settings apply on the next launch:

```ini
eye_resolution=2160
gameplay_scale=1.6
outside_darkening=0.65
```

Use `gameplay_scale=1` for the earlier window size, or `outside_darkening=0` to remove the shading. The cyan border is a guide on the HUD plane; aiming still uses the reconstructed world and the arcade game's original hit tests.

## Build

Build on macOS or Linux with Python 3, JDK 17, CMake, Ninja and the Android SDK. Set `JAVA_HOME` and `ANDROID_SDK_ROOT` to your own tool installations. Install Android platform 34, build-tools 35.0.0 and NDK 27.2.12479018.

```sh
git clone --recurse-submodules --branch steam-frame https://github.com/astromaddie/time-crisis-vr.git
cd time-crisis-vr
export JAVA_HOME="/path/to/jdk-17"
export ANDROID_SDK_ROOT="/path/to/android-sdk"
sdkmanager "platforms;android-34" "build-tools;35.0.0" "ndk;27.2.12479018"
./Build-Steam-Frame.sh --rom "/path/to/your/timecris.zip" --jobs 4
```

The build checks the ROM chips, generates the needed game code, fetches pinned SDL/OpenXR dependencies and produces `artifacts/TimeCrisisVR-SteamFrame-arm64.apk`. The APK excludes the ROM; keep the supplied archive and generated game files private.

For installation, the [local bundle guide](tools/steam_frame/bundle/README.txt) explains the installer and persistent folders. It expects a prepared package with its build receipt and checksums, not just this source template. The installer backs up the existing game folder, copies the APK and ROM separately, and registers the Steam title.

## Current gaps

The Android surface can crash during the diagnostic self-exit path. Normal play and quitting through Steam have worked, but that shutdown bug still needs fixing. The default renderer uses separate eye passes; an optional multiview path exists for development. This fork hasn't added its own foveation controls.

## Credits and licenses

DR-89 and contributors provide the original VR port. The reconstruction dependency is pinned at `6aaa90b4cbc7a23733e1c9f5f9a5e772a19fe23d`. Original notices are retained in [LICENSE](LICENSE), [NOTICE.md](NOTICE.md) and the dependency.

The MIT notice for port code doesn't grant rights to Time Crisis's arcade program, ROM, graphics or sound. The reconstruction includes generated translations of commercial game code, whose distribution rights still need review before a public release. See [the rights record](docs/PR0_READINESS.md). The [architecture notes](docs/VR_ARCHITECTURE.md) hold the implementation and performance history.
