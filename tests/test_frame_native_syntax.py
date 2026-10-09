"""Parse all Frame-native entry points with the Frame flag on and off."""
from pathlib import Path
import os, shlex, subprocess

ROOT=Path(__file__).resolve().parents[1]
NATIVE=ROOT/'build/frame/native'
NINJA=os.environ.get('NINJA','/opt/homebrew/bin/ninja')
for source in ('quest_host.c','quest_main.c','quest_ui.c'):
    command=subprocess.check_output([NINJA,'-C',str(NATIVE),'-t','commands','CMakeFiles/main.dir/'+source+'.o'],text=True).splitlines()[-1]
    original=shlex.split(command);arguments=[];skip=False
    for argument in original:
        if skip:skip=False;continue
        if argument in ('-o','-MF','-MT'):skip=True;continue
        if argument in ('-MD','-c','-DTCVR_STEAM_FRAME'):continue
        arguments.append(argument)
    for mode in ('Quest','Frame'):
        flags=['-DTCVR_STEAM_FRAME'] if mode=='Frame' else []
        subprocess.run(arguments+flags+['-fsyntax-only','-Werror=implicit-function-declaration'],cwd=NATIVE,check=True)
        print('PASS:',mode,source)
source='quest_main.c'
command=subprocess.check_output([NINJA,'-C',str(NATIVE),'-t','commands','CMakeFiles/main.dir/'+source+'.o'],text=True).splitlines()[-1]
original=shlex.split(command);arguments=[];skip=False
for argument in original:
    if skip:skip=False;continue
    if argument in ('-o','-MF','-MT'):skip=True;continue
    if argument in ('-MD','-c','-DTCVR_STEAM_FRAME'):continue
    arguments.append(argument)
for mode in ('Quest','Frame'):
    flags=['-DTCVR_STEAM_FRAME'] if mode=='Frame' else []
    subprocess.run(arguments+flags+['-fsyntax-only','-Werror=implicit-function-declaration',str(ROOT/'quest/frame_replay.c')],cwd=NATIVE,check=True)
    print('PASS:',mode,'frame_replay.c')
commands=subprocess.check_output([NINJA,'-C',str(NATIVE),'-t','commands'],text=True).splitlines()
engine_run=ROOT/'.tools/upstream-steam-frame/engine/ss22_run.c'
command=next(line for line in commands if str(engine_run) in line and ' -c ' in line)
original=shlex.split(command);arguments=[];skip=False
for argument in original:
    if skip:skip=False;continue
    if argument in ('-o','-MF','-MT'):skip=True;continue
    if argument in ('-MD','-c','-DTCVR_STEAM_FRAME'):continue
    arguments.append(argument)
for mode in ('Quest','Frame'):
    flags=['-DTCVR_STEAM_FRAME'] if mode=='Frame' else []
    subprocess.run(arguments+flags+['-fsyntax-only','-Werror=implicit-function-declaration',str(engine_run)],cwd=NATIVE,check=True)
    print('PASS:',mode,'generated ss22_run.c')
print('PASS: native entry points and generated engine replay path preserve the Quest flag-off build and parse the Frame path.')
