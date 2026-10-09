"""Run the Java ROM importer with the exact manifest shipped in the Frame APK."""
from pathlib import Path
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
APK = ROOT/'artifacts/TimeCrisisVR-SteamFrame-arm64.apk'
JAVA_HOME = Path('/opt/homebrew/opt/openjdk@17')
JAVAC = JAVA_HOME/'bin/javac'
JAVA = JAVA_HOME/'bin/java'
if not JAVAC.is_file() or not JAVA.is_file():
    raise SystemExit(f'OpenJDK 17 is required for this host check: {JAVA_HOME}')

with tempfile.TemporaryDirectory(prefix='tcvr-frame-import-') as temp:
    directory = Path(temp)
    manifest = directory/'roms.sha256'
    with zipfile.ZipFile(APK) as archive:
        manifest.write_bytes(archive.read('assets/roms.sha256'))
    classes = directory/'classes'
    classes.mkdir()
    subprocess.run([JAVAC, '-d', classes,
                    ROOT/'quest/java/org/timecrisis/quest/RomInstaller.java',
                    ROOT/'quest/java/org/timecrisis/frame/FrameStorage.java',
                    ROOT/'tests/RomInstallerTest.java'], check=True)
    subprocess.run([JAVA, '-cp', classes, 'RomInstallerTest',
                    '/Users/mads/Downloads/timecris.zip', manifest], check=True, timeout=60)
print('PASS: production FrameStorage first-run importer, persistent root and shipped 31-chip manifest')
