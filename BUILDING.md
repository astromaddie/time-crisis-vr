# Build and install on Steam Frame

You can build this port on macOS or an x86_64 Linux computer. On Windows x86_64, use WSL2 Ubuntu with Linux build tools. The output is an Android ARM64 APK that plays standalone on the Frame through Lepton; the computer is only needed to build and transfer it.

The Frame's ARM64 Desktop isn't a native build host for this route. Google's Linux Android SDK/NDK tools are supplied for x86_64 hosts, even when the app they compile targets ARM64. See [the NDK host packages](https://developer.android.com/ndk/downloads).

## Set up the build computer

You need Git, Python 3, JDK 17, CMake, Ninja and Android SDK command-line tools.

- **Windows:** [install WSL2 Ubuntu](https://learn.microsoft.com/en-us/windows/wsl/install), then run the Linux commands in its Ubuntu terminal. Keep the checkout in `~/src`. Install the **Linux** JDK and SDK in WSL; a Windows SDK installation contains the wrong host executables for this route. Windows files are available under `/mnt/c`, so `C:\Games\timecris.zip` becomes `/mnt/c/Games/timecris.zip`.
- **Ubuntu Linux / WSL:** run `sudo apt-get update`, then `sudo apt-get install git python3 openjdk-17-jdk cmake ninja-build openssh-client`.
- **macOS:** install Git/Xcode Command Line Tools, Python 3, JDK 17, CMake and Ninja. For Homebrew users, `brew install python openjdk@17 cmake ninja` supplies the build tools. `JAVA_HOME` must point to the JDK directory containing `bin/java` and `bin/javac`.

Download the SDK command-line tools for your build host from [Android's tools page](https://developer.android.com/studio#command-tools). Place them at `ANDROID_SDK_ROOT/cmdline-tools/latest/`, so `latest/bin/sdkmanager` exists. You can instead use Android Studio's SDK Manager to install the pinned components below.

In a macOS/Linux/WSL terminal:

```sh
export JAVA_HOME="/path/to/your/jdk-17"
export ANDROID_SDK_ROOT="/path/to/your/android-sdk"
export PATH="$JAVA_HOME/bin:$ANDROID_SDK_ROOT/cmdline-tools/latest/bin:$PATH"
sdkmanager "platforms;android-34" "build-tools;35.0.0" "ndk;27.2.12479018"
sdkmanager --licenses
java -version
cmake --version
ninja --version
```

Use your own tool locations. On Ubuntu, `dirname "$(dirname "$(readlink -f "$(command -v javac)")")"` prints the JDK location. This command is Linux-specific; macOS users can use their JDK's documented installation path.

## Build the solo Frame APK

Bring your own matching **Time Crisis World TS2 Ver.B** ROM ZIP. The build validates all required chips; a different game revision won't work.

```sh
mkdir -p "$HOME/src"
cd "$HOME/src"
git clone --recurse-submodules --branch steam-frame https://github.com/astromaddie/time-crisis-vr.git
cd time-crisis-vr
./Build-Steam-Frame.sh --rom "/path/to/your/timecris.zip" --jobs 4
```

The wrapper copies the pinned reconstruction into an ignored build directory, generates the game code, downloads checked SDL/OpenXR inputs and builds the APK. It doesn't need GitHub credentials. Reduce `--jobs` if memory is tight.

Outputs:

- `artifacts/TimeCrisisVR-SteamFrame-arm64.apk` — the ROM-free app.
- `artifacts/TimeCrisisVR-SteamFrame-build-info.json` — source, compiler, dependency and APK identities.

The signing key is created locally under `~/.config/time-crisis-vr/`, outside Git. Keep that key for your own updates. Your ROM and generated game files remain private build inputs. Multiplayer development lives separately on `steam-frame-multiplayer`; these instructions build solo play.

The earlier complete APK checks were on macOS. Linux/WSL host-tool selection now has focused checks; this documentation update doesn't claim a fresh full Linux/Windows game build.

## Install on the Frame

For a first installation, use [Valve's SteamOS Devkit Client](https://partner.steamgames.com/doc/steamhardware/steamframe/loadgames). It handles uploading an APK and making a persistent Steam library entry.

Enable Developer Mode on the Frame and pair your computer as described in that guide. Close any running game before uploading. Put **only** the built APK in a new upload folder, then choose **Title Upload**:

| Field | Value |
| --- | --- |
| Name | `TimeCrisisVR` |
| Local Folder | Your upload folder containing only the APK |
| Start Command | `TimeCrisisVR-SteamFrame-arm64.apk` |
| Runtime | Android |

On Windows, copy the APK out of WSL using `\\wsl.localhost\Ubuntu\home\YOUR_WSL_USER\src\time-crisis-vr\artifacts`, or select that folder in the file picker. Don't upload the source checkout or its generated game files.

Copy your `timecris.zip` to `~/Documents/TimeCrisisVR/timecris.zip` on the Frame. You can use the Frame Desktop file manager, or SSH after pairing:

```sh
export FRAME_HOST="steamos@YOUR_FRAME_ADDRESS"
export TCVR_ROM="/path/to/your/timecris.zip"
ssh "$FRAME_HOST" 'mkdir -p "$HOME/Documents/TimeCrisisVR"; chmod 777 "$HOME/Documents/TimeCrisisVR"'
scp "$TCVR_ROM" "$FRAME_HOST:Documents/TimeCrisisVR/timecris.zip"
```

Inside Lepton that directory is `/sdcard/Documents/TimeCrisisVR`. The app imports and verifies the ROM when it starts. Options and progress remain in this persistent folder when you update the APK.

Launch **TimeCrisisVR from the Frame's Steam library** for real controller input and per-app display settings. Quit through the SteamVR menu. For updates, upload under the same title name and keep your game-data folder.

The [bundle installer](tools/steam_frame/bundle/README.txt) is an optional route for prepared local packages with payloads and checksums. Its shared-device booking script is for that coordinated development workflow; the Devkit Client route above doesn't need it.
