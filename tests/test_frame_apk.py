"""Validate the signed Steam Frame artifact and its reproducible build receipt."""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess
import zipfile
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools'))
import patch_sdl_frame
SDK = Path('/Users/mads/Library/Android/sdk')
BUILD_TOOLS = SDK / 'build-tools/35.0.0'

parser = argparse.ArgumentParser()
parser.add_argument('--apk', type=Path, default=ROOT/'artifacts/TimeCrisisVR-SteamFrame-arm64.apk')
args = parser.parse_args()
apk = args.apk.resolve()
receipt_path = apk.with_name('TimeCrisisVR-SteamFrame-build-info.json')
receipt = json.loads(receipt_path.read_text())

assert receipt['package'] == 'org.timecrisis.frame'
assert receipt['abi'] == 'arm64-v8a' and receipt['roms_bundled'] is False
assert receipt['sha256'] == hashlib.sha256(apk.read_bytes()).hexdigest()
assert 'rom_sha256' not in receipt
assert receipt['upstream'] == '6aaa90b4cbc7a23733e1c9f5f9a5e772a19fe23d'
assert receipt['build_tools']['android_ndk']['Pkg.Revision'] == '27.2.12479018'
assert receipt['build_tools']['android_build_tools']['Pkg.Revision'] == '35.0.0'
assert receipt['build_tools']['jdk']['javac'].startswith('javac 17.')
assert receipt['build_tools']['python']['version']
assert receipt['build_tools']['cmake']['version'].startswith('cmake version ')
assert receipt['build_tools']['ninja']['version']
frame_java = receipt['sdl_frame_java']
assert frame_java['patch_id'] == patch_sdl_frame.PATCH_ID
assert frame_java['patch_script_sha256'] == hashlib.sha256((ROOT/'tools/patch_sdl_frame.py').read_bytes()).hexdigest()
java_view = ROOT/frame_java['generated_source_view']
patched_handler = java_view/patch_sdl_frame.HANDLER_PATH
source_handler = ROOT/'.tools/sdl/SDL2-2.30.11/android-project/app/src/main/java'/patch_sdl_frame.HANDLER_PATH
assert frame_java['source_sha256'] == hashlib.sha256(source_handler.read_bytes()).hexdigest()
assert frame_java['patched_sha256'] == hashlib.sha256(patched_handler.read_bytes()).hexdigest()
assert frame_java['generated_java_tree_sha256'] == patch_sdl_frame.java_tree_sha256(java_view)
assert patch_sdl_frame.PATCH_MARKER not in source_handler.read_text()
deps = receipt['dependencies']
assert deps['sdl']['version'] == '2.30.11' and deps['sdl']['archive_sha256'] == deps['sdl']['sha256']
assert deps['openxr_android_loader']['version'] == '1.1.43'
assert deps['openxr_android_loader']['archive_sha256'] == deps['openxr_android_loader']['sha256']
loader_source = ROOT/'.tools/openxr/prefab/modules/openxr_loader/libs/android.arm64-v8a/libopenxr_loader.so'
assert deps['openxr_android_loader']['arm64_library_sha256'] == hashlib.sha256(loader_source.read_bytes()).hexdigest()

with zipfile.ZipFile(apk) as archive:
    assert archive.testzip() is None
    names = set(archive.namelist())
    assert 'assets/roms.sha256' in names
    rom_manifest = archive.read('assets/roms.sha256').decode().splitlines()
    assert len(rom_manifest) == 31 and all(line.split('  ', 1)[1] != 'c71.bin' for line in rom_manifest)
    assert not any(name.startswith('assets/roms/') or name.lower().endswith('timecris.zip') for name in names)
    assert not any('keystore' in name.lower() or name.endswith('.jks') for name in names)
    assert sorted(name for name in names if name.startswith('lib/')) == [
        'lib/arm64-v8a/libSDL2.so',
        'lib/arm64-v8a/libmain.so',
        'lib/arm64-v8a/libopenxr_loader.so',
    ]
    for name in (name for name in names if name.startswith('lib/')):
        data = archive.read(name)
        assert data[:5] == b'\x7fELF\x02'
        assert int.from_bytes(data[18:20], 'little') == 183, name
    dex = archive.read('classes.dex')
    assert b'org/timecrisis/frame/MainActivity' in dex
    assert b'org/timecrisis/frame/LauncherActivity' in dex

aapt2 = BUILD_TOOLS/'aapt2'
badging = subprocess.check_output([aapt2, 'dump', 'badging', apk], text=True)
assert "package: name='org.timecrisis.frame'" in badging
assert "launchable-activity: name='org.timecrisis.frame.LauncherActivity'" in badging
manifest = subprocess.check_output([aapt2, 'dump', 'xmltree', '--file', 'AndroidManifest.xml', apk], text=True)
assert 'org.khronos.openxr.intent.category.IMMERSIVE_HMD' in manifest
assert 'android.permission.MANAGE_EXTERNAL_STORAGE' in manifest
for forbidden in ('com.oculus', 'quest2', 'quest3', 'android.hardware.vr.headtracking'):
    assert forbidden not in manifest.lower(), forbidden
assert 'android.hardware.touchscreen' in manifest
assert 'hardware.vr.headtracking' not in manifest

signer = ['/opt/homebrew/opt/openjdk@17/bin/java', '-jar', BUILD_TOOLS/'lib/apksigner.jar']
subprocess.run(signer + ['verify', '--verbose', apk], check=True, stdout=subprocess.DEVNULL)
# With minSdk 29 the default verifier selects v3; its v2=false means unused,
# not absent. Verify the v2 signature over the API24-27 range explicitly.
v2_report = subprocess.check_output(signer + ['verify', '--verbose', '--min-sdk-version', '24',
                                              '--max-sdk-version', '27', apk], text=True)
assert 'Verified using v2 scheme (APK Signature Scheme v2): true' in v2_report, v2_report
subprocess.run([BUILD_TOOLS/'zipalign', '-c', '-P', '16', '4', apk], check=True)
print('PASS: signed/aligned Frame package, generic OpenXR manifest, ARM64-only libraries, ROM/key exclusion and complete dependency/toolchain receipt')
