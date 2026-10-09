"""Exercise the production scene cache and one-shot Frame sprite report."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
COMPILER = Path('/usr/bin/clang')
GENERATED_ENGINE = ROOT / '.tools/upstream-steam-frame/engine'
if not COMPILER.is_file():
    raise SystemExit(f'host C compiler not found: {COMPILER}')
if not GENERATED_ENGINE.is_dir():
    raise SystemExit(f'generated Frame engine headers not found: {GENERATED_ENGINE}')

with tempfile.TemporaryDirectory(prefix='tcvr-sprite-report-') as directory:
    for name, flags in (('quest', []), ('frame', ['-DTCVR_STEAM_FRAME'])):
        output = Path(directory) / f'{name}-vr'
        command = [
            str(COMPILER), '-std=c11', '-O1', '-Wall', '-Wextra',
            '-I' + str(ROOT / 'quest'), '-I' + str(GENERATED_ENGINE),
            '-I' + str(ROOT / 'upstream/engine'),
            *flags, str(ROOT / 'tests/test_vr.c'),
            str(ROOT / 'quest/quest_scene.c'), '-lm', '-o', str(output),
        ]
        subprocess.run(command, cwd=ROOT, check=True)
        subprocess.run([output], cwd=ROOT, check=True, timeout=20)

print('PASS: Quest flag-off scene behavior and Frame production sprite-report path')
