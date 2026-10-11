"""Exercise the native lobby against the real LAN service and capture its UI."""
from pathlib import Path
import os, subprocess, sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from host_paths import android_sdk_root
sdk=android_sdk_root()
headers=sdk/'ndk/27.2.12479018/toolchains/llvm/prebuilt/darwin-x86_64/sysroot/usr/include'
out=Path(os.environ.get('TCVR_LOBBY_EVIDENCE','/tmp/tcvr-frame-lobby'));out.mkdir(parents=True,exist_ok=True)
command=['clang','-std=c11','-O1','-g','-DTCVR_STEAM_FRAME','-ffunction-sections','-fdata-sections',
 '-I'+str(ROOT/'quest'),'-I'+str(ROOT/'upstream/include'),'-idirafter',str(headers/'aarch64-linux-android'),'-idirafter',str(headers),
 str(ROOT/'tests/frame_lobby_fixture.c'),str(ROOT/'quest/frame_lobby.c'),str(ROOT/'quest/quest_coop.c'),'-pthread','-Wl,-dead_strip','-o',str(out/'lobby-test')]
subprocess.run(command,check=True)
subprocess.run([str(out/'lobby-test'),str(out)],check=True,timeout=30)
launcher=(ROOT/'quest/java/org/timecrisis/frame/LauncherActivity.java').read_text()
assert 'CoopSessionController' not in launcher and 'launchGame();' in launcher
entry=(ROOT/'quest/quest_main.c').read_text()
assert entry.index('qframe_lobby_run()')<entry.index('bool cooperative=qcoop_requested()')<entry.index('tc_game_main(game_argc,args)')
print('PASS: normal Frame launcher enters native activity; session selection precedes game/recording mode selection')
print('Production CPU UI captures:',out)
