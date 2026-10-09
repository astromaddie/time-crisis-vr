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
4. Launch TimeCrisisVR from the Steam library.

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
A: insert three credits. B: toggle laser. These stay fixed for either weapon hand.
Right Menu: pause/options or resume.
Left View: short press goes back; hold 1 second recenters;
hold 3 seconds recenters and calibrates upright head height.
In options only: X switches grip/physical-duck cover; right stick up/down adjusts
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
Visuals, controls and comfort remain headset-unverified until this wear check.
See evidence/ for the recorded build/package/device verification results.
