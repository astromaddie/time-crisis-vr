Time Crisis VR for Steam Frame — local package

This is the standalone ARM64 Android/OpenXR port through Lepton. Bring your own
matching Time Crisis World TS2 Ver.B archive (timecris.zip); it is not in the APK.
This guide is for a prepared local package with its APK, checksums and build receipt.
The source template alone is not an installable package.

INSTALL
Enable SSH on your Frame and close the game. Set FRAME_HOST to its own SSH target,
and FRAME_LOCK_SCRIPT to your local Frame booking script. From the prepared package:

  export FRAME_HOST="steamos@YOUR_FRAME_ADDRESS"
  export FRAME_LOCK_SCRIPT="/path/to/frame-lock.sh"
  ./install.sh --verify-only
  ./install.sh --rom "/path/to/your/timecris.zip"

The installer checks the package and all 31 ROM chips, backs up existing game data,
copies the APK and ROM separately, and registers TimeCrisisVR. Use a host-key alias
only if you already have that alias in your trusted SSH configuration.

Launch TimeCrisisVR from Steam for controllers and per-app display settings.
Game files, settings and progress: ~/Documents/TimeCrisisVR on SteamOS,
/sdcard/Documents/TimeCrisisVR inside Lepton.
Backups: ~/Documents/TimeCrisisVR-backups/<UTC timestamp>.
Quit through SteamVR's menu.

CONTROLS
Either trigger: use that hand; fire, start or confirm.
Either grip: hold to leave cover; release both to hide and reload.
Right A: credits. Right B: laser. These keep their roles with either gun hand.
Right Menu: pause/options or resume.
Left View: short press for Back; hold 1 second to recentre;
hold 3 seconds to recentre and calibrate upright height.
In options: right X switches grip cover / physical ducking; right stick up/down
adjusts gun angle; right-stick click changes default hand; left-stick click resets angle.

DISPLAY
The default eye target is 2160x2160. Choose the display rate in SteamVR's per-app
settings. Play sessions at 120 Hz have felt smooth, with occasional slow frames
in the recorded timings. The larger window, darker surround, explosion-depth fix,
audio, grip cover/reload and Menu pause/resume have been confirmed during play.

Persistent frame-display.cfg settings apply on next launch:
  eye_resolution=2160
  gameplay_scale=1.6
  outside_darkening=0.65

The default window is 60% wider/taller. Use scale 1 for its earlier size, or shading 0
to remove the darker surround. The cyan border guides the arcade shooting area;
actual aiming still follows the reconstructed world and the original hit tests.
Gun pitch is adjusted in options; -15 degrees is the setting used on my Frame.

CURRENT GAPS AND LICENSES
The diagnostic self-exit can crash in Android surface handling. Quit through Steam.
The default is separate-eye rendering; optional multiview is a development setting.
No game-controlled foveation controls have been added.
Keep the package's license notices. The port's MIT notice does not cover the commercial
arcade program, graphics, sound or ROM. Reconstruction-source distribution rights
remain under review. Do not redistribute your game files or generated game code.
