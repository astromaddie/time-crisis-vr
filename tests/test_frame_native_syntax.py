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
print('PASS: complete native entry points preserve the Quest flag-off build and parse the Frame path.')
