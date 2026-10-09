"""Run production input routing tests on macOS without a headset or Windows toolchain."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SDK = Path('/Users/mads/Library/Android/sdk')
NDK = SDK/'ndk/27.2.12479018/toolchains/llvm/prebuilt/darwin-x86_64'
COMPILER = Path('/usr/bin/clang')
if not COMPILER.is_file():
    raise SystemExit(f'Android NDK compiler not found: {COMPILER}')

includes = [
    'quest', 'quest/include', 'upstream/engine', 'upstream/engine/c25',
    'upstream/engine/snd', 'upstream/include', 'build/frame/native/compat',
    '.tools/openxr/prefab/modules/headers/include',
]
base = [str(COMPILER), '-std=c11', '-O1', '-g', '-Wall', '-Wextra',
        '-Wno-unused-function', '-Wno-unused-variable', '-Wno-missing-field-initializers',
        '-ffunction-sections', '-fdata-sections', '-DTCVR', '-DSDL_MAIN_HANDLED',
        '-D__MACOSX__', '-Dgettid=getpid', '-include', 'stddef.h',
        '-DXR_USE_PLATFORM_ANDROID', '-DXR_USE_GRAPHICS_API_OPENGL_ES',
        *['-I'+str(ROOT/path) if not Path(path).is_absolute() else '-I'+path for path in includes],
        '-idirafter', str(NDK/'sysroot/usr/include/aarch64-linux-android'),
        '-idirafter', str(NDK/'sysroot/usr/include'),
        str(ROOT/'tests/handedness_fixture.c'), str(ROOT/'quest/quest_options.c'),
        str(ROOT/'quest/quest_cover.c'), '-Wl,-dead_strip', '-lm']

with tempfile.TemporaryDirectory(prefix='tcvr-controls-') as directory:
    modes = [
        ('quest', []),
        ('frame', ['-DTCVR_STEAM_FRAME']),
        ('frame-touch-fallback', ['-DTCVR_STEAM_FRAME', '-DTCVR_FRAME_PROFILE_UNSUPPORTED']),
    ]
    for name, flags in modes:
        output = Path(directory)/(name+'-controls')
        command = base.copy()
        command += flags
        command += ['-o', str(output)]
        subprocess.run(command, cwd=ROOT, check=True)
        subprocess.run([output], cwd=directory, check=True, timeout=20)
print('PASS: Quest, Frame extension profile and Touch fallback production input paths')
