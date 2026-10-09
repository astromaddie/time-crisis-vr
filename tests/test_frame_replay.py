"""Run the real Frame launch selector and EEPROM saver in isolated directories."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
COMPILER = Path('/usr/bin/clang')
if not COMPILER.is_file():
    raise SystemExit(f'host C compiler not found: {COMPILER}')

command = [
    str(COMPILER), '-std=c11', '-O1', '-Wall', '-Wextra', '-DTCVR_STEAM_FRAME',
    '-ffunction-sections', '-fdata-sections',
    '-I' + str(ROOT / 'quest'), '-I' + str(ROOT / 'upstream/engine'),
    '-I' + str(ROOT / '.tools/sdl/SDL2-2.30.11/include'),
    '-I' + str(ROOT / 'build/frame/native/compat'),
    '-I' + str(ROOT / 'upstream/engine/c25'), '-I' + str(ROOT / 'upstream/engine/snd'),
    '-I' + str(ROOT / 'upstream/timecris/gen'), '-I' + str(ROOT / 'upstream/include'),
    str(ROOT / 'tests/frame_replay_fixture.c'), str(ROOT / 'quest/frame_replay.c'),
    str(ROOT / 'upstream/engine/ss22_board.c'), '-Wl,-dead_strip', '-o',
]

with tempfile.TemporaryDirectory(prefix='tcvr-frame-replay-test-') as directory:
    executable = Path(directory) / 'frame-replay-fixture'
    subprocess.run(command + [str(executable)], cwd=ROOT, check=True)
    root = Path(directory) / 'files'
    root.mkdir()

    def write(name, data):
        (root / name).write_bytes(data.encode() if isinstance(data, str) else data)

    write('last-session.inputs', 'latest-session')
    write('previous-session.inputs', 'previous-session')
    subprocess.run([executable, 'normal'], cwd=root, check=True)

    write('last-session.inputs', 'latest-session')
    write('previous-session.inputs', 'previous-session')
    header = b'SS22REC 1 TC ' + b'00' * 0x2000 + b'\n'
    write('vrdiag-replay.inputs', header)
    write('vrdiag-replay.once', 'replay=1\n')
    subprocess.run([executable, 'replay'], cwd=root, check=True)

    write('last-session.inputs', 'latest-session')
    write('previous-session.inputs', 'previous-session')
    write('vrdiag-replay.inputs', 'not a TC replay\n')
    write('vrdiag-replay.once', 'replay=1\n')
    subprocess.run([executable, 'invalid'], cwd=root, check=True)

    write('vrdiag-replay.inputs', 'SS22REC 1 TC 00\n')
    write('vrdiag-replay.once', 'replay=1\n')
    subprocess.run([executable, 'truncated'], cwd=root, check=True)

    supplied = os.environ.get('TCVR_FRAME_REPLAY_TEST_INPUT')
    if supplied:
        source = Path(supplied)
        if not source.is_file():
            raise SystemExit(f'diagnostic replay input not found: {source}')
        write('last-session.inputs', 'latest-session')
        write('previous-session.inputs', 'previous-session')
        shutil.copyfile(source, root / 'vrdiag-replay.inputs')
        write('vrdiag-replay.once', 'replay=1\n')
        subprocess.run([executable, 'replay'], cwd=root, check=True)
        print(f'PASS: supplied replay recording header accepted without changing the source: {source}')

print('PASS: normal, valid replay, invalid replay, and memory-only EEPROM save production paths')
