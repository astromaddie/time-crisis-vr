"""Reproduce focused launch through production events and real SS22 inputs."""
from pathlib import Path
import os, subprocess, sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from host_paths import android_sdk_root
host='darwin-x86_64' if sys.platform=='darwin' else 'linux-x86_64'
headers=android_sdk_root()/'ndk/27.2.12479018/toolchains/llvm/prebuilt'/host/'sysroot/usr/include'
out=Path(os.environ.get('TCVR_FOCUS_EVIDENCE','/tmp/tcvr-frame-lobby-focus'));out.mkdir(parents=True,exist_ok=True)
includes=['quest','quest/include','upstream/engine','upstream/engine/c25','upstream/engine/snd','upstream/include','build/frame/native/compat','.tools/openxr/prefab/modules/headers/include']
command=['clang','-std=c11','-O1','-g','-fsanitize=address,undefined','-ffunction-sections','-fdata-sections',
 '-DTCVR_STEAM_FRAME','-DTCVR','-DSDL_MAIN_HANDLED','-D__MACOSX__','-Dgettid=getpid','-include','stddef.h',
 '-DXR_USE_PLATFORM_ANDROID','-DXR_USE_GRAPHICS_API_OPENGL_ES',*['-I'+str(ROOT/p) for p in includes],
 '-idirafter',str(headers/'aarch64-linux-android'),'-idirafter',str(headers),
 str(ROOT/'tests/frame_lobby_focus_fixture.c'),str(ROOT/'upstream/engine/ss22_snd.c'),
 '-Wl,-dead_strip','-lm','-pthread','-o',str(out/'focus-test')]
subprocess.run(command,check=True)
subprocess.run([str(out/'focus-test')],check=True,timeout=10)
