"""Exercise the production normal Frame startup path without Android or a headset."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
COMPILER = shutil.which('clang')
if not COMPILER:
    raise SystemExit('clang is required for the production startup-path test')

manifest = ET.parse(ROOT/'quest/AndroidManifest-SteamFrame.xml').getroot()
android = '{http://schemas.android.com/apk/res/android}'
activities = {activity.get(android+'name'): activity
              for activity in manifest.find('application').findall('activity')}
launcher = activities['.LauncherActivity']
main = activities['.MainActivity']
launcher_categories = {category.get(android+'name')
                       for intent in launcher.findall('intent-filter')
                       for category in intent.findall('category')}
main_categories = {category.get(android+'name')
                   for intent in main.findall('intent-filter')
                   for category in intent.findall('category')}
assert {'android.intent.category.LAUNCHER'} <= launcher_categories
assert 'android.intent.category.LAUNCHER' not in main_categories
assert 'org.khronos.openxr.intent.category.IMMERSIVE_HMD' in main_categories

launcher_source = (ROOT/'quest/java/org/timecrisis/frame/LauncherActivity.java').read_text()
assert launcher_source.index('FrameStorage.prepareRoms') < launcher_source.index(
    'new Intent(this, MainActivity.class)')
assert 'org.khronos.openxr.intent.category.IMMERSIVE_HMD' in launcher_source
assert 'COOP' not in launcher_source and 'Lobby' not in launcher_source

cmake = (ROOT/'quest/CMakeLists.txt').read_text()
assert 'frame_lobby.c' not in cmake and 'quest_coop.c' not in cmake
assert 'qframe_lobby_run' not in (ROOT/'quest/quest_main.c').read_text()

with tempfile.TemporaryDirectory(prefix='tcvr-solo-startup-') as directory:
    work = Path(directory)
    include = work/'include'
    (include/'SDL2').mkdir(parents=True)
    (include/'android').mkdir()
    (include/'SDL2/SDL.h').write_text('''
#pragma once
#define SDL_MESSAGEBOX_ERROR 0x10
int SDL_ShowSimpleMessageBox(unsigned, const char *, const char *, void *);
''')
    (include/'android/log.h').write_text('''
#pragma once
#define ANDROID_LOG_ERROR 6
int __android_log_print(int, const char *, const char *, ...);
''')
    executable = work/'solo-startup'
    subprocess.run([
        COMPILER, '-std=c11', '-Wall', '-Wextra', '-DTCVR_STEAM_FRAME',
        '-I'+str(include), '-I'+str(ROOT/'upstream/engine'),
        '-I'+str(ROOT/'quest'),
        str(ROOT/'tests/solo_startup_fixture.c'),
        str(ROOT/'quest/quest_main.c'),
        str(ROOT/'quest/frame_replay.c'),
        '-o', str(executable)
    ], cwd=ROOT, check=True)
    subprocess.run([executable], cwd=work, check=True, timeout=15)

print('PASS: Frame launcher manifest, ROM-validated handoff, lobby-free CMake/startup, and production native game entry')
