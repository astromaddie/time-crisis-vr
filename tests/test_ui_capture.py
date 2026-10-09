"""Test the production CPU HUD capture's top-to-bottom PPM row order."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SDK = Path('/Users/mads/Library/Android/sdk')
NDK_HEADERS = SDK/'ndk/27.2.12479018/toolchains/llvm/prebuilt/darwin-x86_64/sysroot/usr/include'
COMPILER = Path('/usr/bin/clang')

with tempfile.TemporaryDirectory(prefix='tcvr-ui-capture-') as directory:
    output = Path(directory)/'hud-capture-test'
    image = Path(directory)/'hud.ppm'
    command = [str(COMPILER), '-std=c11', '-O1', '-ffunction-sections', '-fdata-sections',
               '-I'+str(ROOT/'quest'), '-I'+str(ROOT/'quest/include'), '-I'+str(ROOT/'upstream/include'),
               '-idirafter', str(NDK_HEADERS/'aarch64-linux-android'),
               '-idirafter', str(NDK_HEADERS), str(ROOT/'tests/ui_capture_fixture.c'),
               '-Wl,-dead_strip', '-o', str(output)]
    subprocess.run(command, cwd=ROOT, check=True)
    subprocess.run([output, image], check=True)
print('PASS: production HUD PPM capture writes the CPU top row first and bottom row last')
