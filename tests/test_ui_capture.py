"""Test the production CPU HUD capture's top-to-bottom PPM row order."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools'))
from host_paths import android_sdk_root

SDK = android_sdk_root()
HOST_TAG = 'darwin-x86_64' if sys.platform == 'darwin' else 'linux-x86_64'
NDK_HEADERS = SDK/'ndk/27.2.12479018/toolchains/llvm/prebuilt'/HOST_TAG/'sysroot/usr/include'
COMPILER = Path(shutil.which('clang') or '')
if not COMPILER.is_file():
    raise SystemExit('clang is required on PATH for this host syntax test')

with tempfile.TemporaryDirectory(prefix='tcvr-ui-capture-') as directory:
    for profile, defines in (('quest', []), ('frame', ['-DTCVR_STEAM_FRAME'])):
        output = Path(directory)/f'hud-capture-{profile}'
        image = Path(directory)/f'hud-{profile}.ppm'
        notice = Path(directory)/f'lan-notice-{profile}.ppm'
        command = [str(COMPILER), '-std=c11', '-O1', '-ffunction-sections', '-fdata-sections',
                   *defines, '-I'+str(ROOT/'quest'), '-I'+str(ROOT/'quest/include'), '-I'+str(ROOT/'upstream/include'),
                   '-idirafter', str(NDK_HEADERS/'aarch64-linux-android'),
                   '-idirafter', str(NDK_HEADERS), str(ROOT/'tests/ui_capture_fixture.c'),
                   '-Wl,-dead_strip', '-o', str(output)]
        subprocess.run(command, cwd=ROOT, check=True)
        subprocess.run([output, image, notice], check=True)
print('PASS: Quest and Frame production HUD captures preserve PPM row order, readable LAN status, and non-overlapping help')
