"""Compile the generated production EEPROM guard and exercise client-to-Solo persistence."""
import importlib.util
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from host_paths import android_sdk_root
spec=importlib.util.spec_from_file_location('patch_upstream',ROOT/'tools/patch_upstream.py')
patch=importlib.util.module_from_spec(spec);spec.loader.exec_module(patch)
names=tuple('engine/'+name for name in ('geo_hw.h','geo_hw.c','ss22_gl.c','ss22_run.c','ss22_board.c','ss22_board.h','quad_gl.c','tex_bake.c','sprite_hw.c','text_hw.c'))+('timecris/gen/tc_lifted_tab.c',)
compiler=Path(shutil.which('clang') or '')
if not compiler.is_file():raise SystemExit('clang is required for the production EEPROM guard test')
with tempfile.TemporaryDirectory(prefix='tcvr-eeprom-guard-') as temporary:
    directory=Path(temporary);patch.ROOT=directory
    for name in names:
        data=subprocess.check_output(['git','show','HEAD:'+name],cwd=ROOT/'upstream')
        path=directory/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
    patch.main()
    binary=directory/'eeprom-guard-test';work=directory/'storage';work.mkdir()
    local=bytes((i*13+7)&255 for i in range(0x2000));(work/'game_eeprom.nv').write_bytes(local)
    sdl=ROOT/'.tools/sdl/SDL2-2.30.11/include';compat=directory/'compat/SDL2';compat.mkdir(parents=True)
    for header in sdl.glob('*.h'):(compat/header.name).symlink_to(header)
    sdk=android_sdk_root();host_tag='darwin-x86_64' if sys.platform=='darwin' else 'linux-x86_64'
    ndk_headers=sdk/'ndk/27.2.12479018/toolchains/llvm/prebuilt'/host_tag/'sysroot/usr/include'
    command=[str(compiler),'-std=gnu11','-O1','-ffunction-sections','-fdata-sections','-I'+str(directory/'engine'),'-I'+str(directory/'compat'),'-I'+str(sdl),
             '-I'+str(ROOT/'upstream/engine'),'-I'+str(ROOT/'upstream/engine/c25'),'-I'+str(ROOT/'upstream/include'),'-I'+str(ROOT/'upstream/timecris/gen'),
             '-idirafter',str(ndk_headers/'aarch64-linux-android'),'-idirafter',str(ndk_headers),
             str(ROOT/'tests/eeprom_guard_fixture.c'),str(directory/'engine/ss22_board.c'),'-Wl,-dead_strip','-o',str(binary)]
    subprocess.run(command,cwd=ROOT,check=True)
    subprocess.run([binary],cwd=work,check=True)
print('PASS: patched production EEPROM guard preserves client and Solo writes.')
