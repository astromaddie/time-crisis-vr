"""Run the production Java ROM importer with a synthetic 31-chip manifest."""
from pathlib import Path
import hashlib
import subprocess
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools'))
from host_paths import java_tool

JAVAC = java_tool('javac')
JAVA = java_tool('java')

with tempfile.TemporaryDirectory(prefix='tcvr-frame-import-') as temp:
    directory = Path(temp)
    manifest = directory/'roms.sha256'
    rom = directory/'synthetic-rom.zip'
    chips = [f'synthetic test chip {index:02d}'.encode() for index in range(31)]
    manifest.write_text(''.join(hashlib.sha256(data).hexdigest()+f'  synthetic_chip_{index:02d}.bin\n'
                                for index, data in enumerate(chips)), encoding='ascii')
    with zipfile.ZipFile(rom, 'w') as archive:
        for index, data in enumerate(chips):
            archive.writestr(f'legacy-name-{index:02d}.bin', data)
    classes = directory/'classes'
    classes.mkdir()
    subprocess.run([JAVAC, '-d', classes,
                    ROOT/'quest/java/org/timecrisis/quest/RomInstaller.java',
                    ROOT/'quest/java/org/timecrisis/frame/FrameStorage.java',
                    ROOT/'tests/RomInstallerTest.java'], check=True)
    subprocess.run([JAVA, '-cp', classes, 'RomInstallerTest',
                    rom, manifest], check=True, timeout=60)
print('PASS: production FrameStorage first-run importer, persistent root and synthetic 31-chip manifest')
