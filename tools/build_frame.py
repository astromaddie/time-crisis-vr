"""Build the Steam Frame arm64 APK from a local Android toolchain."""
from pathlib import Path
import argparse, os, shutil, subprocess, sys
from host_paths import android_sdk_root, executable, java_home

ROOT = Path(__file__).resolve().parents[1]
PINNED_UPSTREAM = '6aaa90b4cbc7a23733e1c9f5f9a5e772a19fe23d'
MIRROR = ROOT / '.tools/upstream-steam-frame'

def run(command, cwd=ROOT, env=None):
    print('> ' + ' '.join(map(str, command)), flush=True)
    subprocess.run(list(map(str, command)), cwd=cwd, env=env, check=True)

def prepare_mirror():
    revision = subprocess.check_output(['git','rev-parse','HEAD'], cwd=ROOT/'upstream', text=True).strip()
    if revision != PINNED_UPSTREAM:
        raise RuntimeError(f'Expected pinned upstream {PINNED_UPSTREAM}, found {revision}')
    status = subprocess.check_output(['git','status','--porcelain','--untracked-files=all'], cwd=ROOT/'upstream', text=True)
    if status.strip():
        raise RuntimeError('Pinned upstream source must be clean before generating the Frame build view; preserve your changes in another checkout.')
    if MIRROR.exists(): shutil.rmtree(MIRROR)
    MIRROR.parent.mkdir(parents=True, exist_ok=True)
    shutil.copytree(ROOT/'upstream', MIRROR, symlinks=True, ignore=shutil.ignore_patterns('.git'))
    return MIRROR

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--rom', type=Path, required=True, help='user-supplied matching Time Crisis World TS2 Ver.B ROM ZIP')
    parser.add_argument('--jobs', type=int, default=4)
    args=parser.parse_args()
    if not args.rom.is_file(): parser.error('ROM not found: ' + str(args.rom))
    sdk=android_sdk_root()
    java=java_home()
    cmake=executable('cmake')
    ninja=executable('ninja')
    ndk=sdk/'ndk/27.2.12479018'
    for path in (sdk/'platforms/android-34/android.jar',sdk/'build-tools/35.0.0/aapt2',ndk/'build/cmake/android.toolchain.cmake',java/'bin/java'):
        if not path.exists(): raise RuntimeError('Required installed Android tool not found: ' + str(path))
    upstream=prepare_mirror()
    env=os.environ.copy()
    env.update({'ANDROID_SDK_ROOT':str(sdk),'ANDROID_NDK_ROOT':str(ndk),'JAVA_HOME':str(java),
                'CMAKE':str(cmake),'NINJA':str(ninja),
                'TCVR_STEAM_FRAME_BUILD':'1','TCVR_UPSTREAM_SOURCE':str(upstream)})
    run([sys.executable,ROOT/'tools/build.py','--rom',args.rom,'--jobs',args.jobs],env=env)

if __name__=='__main__': main()
