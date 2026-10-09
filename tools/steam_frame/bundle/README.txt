Time Crisis VR — Steam Frame local test bundle

This is the ARM64 Android/OpenXR port for standalone play through Lepton.
The APK contains the port and public assets. Supply your matching timecris.zip separately.
Source: https://github.com/astromaddie/time-crisis-vr (private)

INSTALL
1. Connect the Frame on USB or Wi-Fi and ensure SSH access works with your existing key.
2. Close running VR games. The installer uses the shared Frame booking.
3. From this directory run:
   ./install.sh --verify-only
   ./install.sh --rom "$HOME/Downloads/timecris.zip"
   For a different address: add --host steamos@<address>.
4. Launch Devkit Game: TimeCrisisVR from the Steam library.

The installer checks the bundle hashes and all 31 required ROM chip hashes,
backs up any existing TimeCrisisVR data and title with a verified manifest,
then copies the APK and your archive separately and registers the Steam title.
The default SSH host is frame.local, checked against this Frame's existing trusted
USB host key. Override --host-key-alias when using another trusted hostname.

Persistent game files/settings/logs: /home/steamos/Documents/TimeCrisisVR
Android path: /sdcard/Documents/TimeCrisisVR
Pre-install backups: /home/steamos/Documents/TimeCrisisVR-backups/<UTC timestamp>

CONTROLS
Either trigger: choose its weapon hand and fire/start/confirm.
Either grip: hold to leave cover; release both to hide/reload.
Right A: insert three credits. Right B: toggle laser. These stay fixed for either weapon hand.
Right Menu: pause/options or resume.
Left View: short press goes back; hold 1 second recenters;
hold 3 seconds recenters and calibrates upright head height.
In options only: right X switches grip/physical-duck cover; right stick up/down adjusts
gun angle, right stick click changes default hand, left stick click resets angle.

15-MINUTE WEAR CHECK
1. Launch through Steam. Check upright stereo, colours, HUD and audio.
2. Press A for credits, then a trigger to start Stage 1.
3. Shoot targets at the centre and both screen edges; check aim versus impacts.
4. Select/fire with each trigger. Check weapon hand and recoil vibration.
5. Use each grip and both together. Release both to hide/reload.
6. Open options with Menu; adjust gun angle, reset it, and change default hand.
   Confirm A/B roles remain fixed and automatic trigger handoff still works.
7. Hold View for 1 second to recenter; for 3 seconds to recalibrate while upright.
8. In options press X for physical ducking, calibrate upright, then duck/reload
   and rise again. Return to grip cover if seated play is more comfortable.
9. Pause/resume, remove and replace the headset, and confirm no accidental shot.
10. Complete Stage 1 if possible; quit through SteamVR and relaunch to check settings.

Report: build ID from build-info.json, scene, aim alignment, world scale,
HUD legibility, cover behavior, controls, comfort, audio and any crash.
Wearer-assisted validation has confirmed the Valve Frame profile, 2160×2160 per eye,
120 Hz, settings reload and subjectively good smoothness. The wearer accepted the
world-effect depth correction on build 3e60774. The wearer also confirmed audio,
grip cover/reload and Menu pause/resume.
Matched quiet-scene, yaw90, rear-facing yaw180 and multiview pairs are inspected.
Yaw180 is black and provides no binocular gameplay evidence. Both hands, physical
ducking, View gestures and full comfort assessment remain pending.
See evidence/ for the recorded build/package/device verification results.

DISPLAY AND CURRENT ACCEPTANCE
The default renderer uses separate eye passes with a 2160-pixel maximum per eye.
frame-display.cfg can set eye_resolution=512..2160 and multiview=1 for comparison.
The Frame build follows the runtime cadence; refresh is controlled by SteamVR.
Startup, ROM persistence and eye-colour probe are verified unattended. The wearer
reached the opening train with the Valve Frame profile at 2160×2160 per eye and 120 Hz,
confirmed settings reload, and described smoothness as good. The barrel had appeared
too high; the wearer accepted saved `gun_pitch=-15` for their device. This is not the
fresh-install default. An early right-eye train capture shows TIME EXTENDED/WAIT;
later matched 2160-square quiet-scene pairs verify the HUD and calibrated shot marks.
On build 3e60774, the wearer reported that the explosions now look great. World
billboards use the game-authored depth estimate, while screen flashes remain overlays.
Audio, grip cover/reload and Menu pause/resume were also confirmed by the wearer.
Yaw90 shows side-facing scenery; both rear-facing yaw180 eyes are entirely black.
The original camera-visible scene does not provide a complete rear environment.
Multiview closely matches the separate-eye quiet scene: fewer than 0.011% of pixels
differ in each eye. Configuration/init/fallback evidence and the source branch
support multiview selection, without per-frame telemetry. This is visual parity,
not a performance or comfort comparison. Separate eyes remain the default.
Matched active-flame images, both weapon hands, physical ducking,
View gestures and full comfort assessment remain pending.

Sustained 120 FPS is not established. The latest 3e60774 wear run produced 68 focused
240-frame reports at 120 Hz/2160-square eyes, with no image or pose capture. Window
means were 4.88–7.25 ms; the largest sample was 88.16 ms, and 3,520 of 16,320 frames
exceeded the render-wall budget. These counters are not compositor dropped frames.
A bounded 60-second diagnostic self-exit reproduced SIGSEGV in Android UI surface
handling under GDB: RefBase::incStrong -> SurfaceControl.Transaction.setWindowCrop
during ViewRootImpl.performTraversals, accessing address 0x4. No app-code frame appears
in the fault stack; app debug symbols were not loaded, so cause remains unresolved.
Normal Steam quit ended with status 137, which does not prove clean native shutdown.
Data was restored and diagnostics/debugger removed before releasing the booking.
Only a curated fault summary is delivered. The grip-pivot diagnostic remains off.

For replaying the saved train session, place the input at
`/sdcard/Documents/TimeCrisisVR/vrdiag-replay.inputs` and create
`vrdiag-replay.once` beside it. Startup validates the recording header before selecting
replay; malformed input preserves the marker, replay source, and normal session files.
Replay does not rotate or create player recordings and keeps its EEPROM image in memory.
The one-shot `vrdiag.once` still controls force-render and capture tick settings.

FOVEATION
This build does not request game-controlled foveation profiles. Valve documents runtime
foveation and eye tracking; current logs show its FDM injection layer loaded through
Zink. Layer loading alone does not prove foveation is active. CPU/GPU measurement
and repeated same scene on/off comparisons are required before recommending a
foveation default. The accepted build keeps its separate-eye path and full eye
resolution.
