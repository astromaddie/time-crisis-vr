"""Run production quest_main startup selection against isolated persistent Solo history."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[1]
compiler=Path(shutil.which('clang') or '')
if not compiler.is_file():raise SystemExit('clang is required for the Quest startup history regression')
with tempfile.TemporaryDirectory(prefix='tcvr-session-history-') as temporary:
    directory=Path(temporary);stubs=directory/'stubs';(stubs/'SDL2').mkdir(parents=True);(stubs/'android').mkdir()
    (stubs/'SDL2/SDL.h').write_text('''#pragma once
#define SDL_MESSAGEBOX_ERROR 0
const char *SDL_AndroidGetInternalStoragePath(void);
int SDL_ShowSimpleMessageBox(unsigned,const char*,const char*,void*);
''')
    (stubs/'android/log.h').write_text('''#pragma once
#define ANDROID_LOG_ERROR 6
int __android_log_print(int,const char*,const char*,...);
''')
    storage=directory/'storage';storage.mkdir();(storage/'last-session.inputs').write_text('solo-current');(storage/'previous-session.inputs').write_text('solo-prior')
    binary=directory/'quest-main-history'
    command=[str(compiler),'-std=gnu11','-O1','-Wall','-Wextra','-Werror','-I'+str(stubs),'-I'+str(ROOT/'quest'),
             str(ROOT/'tests/quest_main_history_fixture.c'),str(ROOT/'quest/quest_main.c'),'-o',str(binary)]
    subprocess.run(command,cwd=ROOT,check=True);subprocess.run([binary,storage],cwd=ROOT,check=True)
print('PASS: production Quest startup does not rotate Solo recordings during co-op.')
