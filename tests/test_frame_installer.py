#!/usr/bin/env python3
"""Exercise bundle verification and the production remote backup command locally."""
from pathlib import Path
import ast, hashlib, importlib.util, json, os, subprocess, sys, tempfile, zipfile

script = Path(__file__).resolve().parents[1] / 'tools/steam_frame/bundle/install.py'
if not script.exists(): script = Path(__file__).resolve().parent / 'delivery/install.py'
spec = importlib.util.spec_from_file_location('frame_installer', script)
m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)

def rejects(function, message):
    try: function()
    except SystemExit as error: assert message in str(error), str(error)
    else: raise AssertionError('Expected rejection: '+message)

assert m.registration_response('{"success":""}') == ''
assert m.registration_response('{"success":"created"}') == 'created'
for reply in ('{"error":"Steam rejected shortcut"}', '{"success":false}', '{"success":null}', '{}', 'not json'):
    rejects(lambda reply=reply: m.registration_response(reply), 'registration')

with tempfile.TemporaryDirectory(prefix='tcvr-installer-test-') as folder:
    root = Path(folder); bundle=root/'bundle'; (bundle/'payload').mkdir(parents=True)
    m.BASE=bundle; m.APK=bundle/'payload/TimeCrisisVR.apk'
    chips=[('chip'+str(i)).encode() for i in range(31)]
    with zipfile.ZipFile(m.APK,'w') as apk:
        apk.writestr('assets/roms.sha256', ''.join(hashlib.sha256(c).hexdigest()+'  expected'+str(i)+'\n' for i,c in enumerate(chips)))
    original=m.APK.read_bytes()
    (bundle/'SHA256SUMS').write_text(hashlib.sha256(original).hexdigest()+'  payload/TimeCrisisVR.apk\n')
    m.verify()
    m.APK.write_bytes(b'tampered'); rejects(m.verify,'checksum mismatch'); m.APK.write_bytes(original)
    (bundle/'unexpected').write_bytes(b'file'); rejects(m.verify,'unexpected'); (bundle/'unexpected').unlink()
    (bundle/'SHA256SUMS').write_text('0'*64+'  ../escape\n'); rejects(m.verify,'unsafe')
    archive=root/'timecris.zip'
    with zipfile.ZipFile(archive,'w') as rom:
        for i,c in enumerate(chips): rom.writestr('older-name-'+str(i),c)
    m.verify_rom(archive)
    with zipfile.ZipFile(archive,'w') as rom:
        for i,c in enumerate(chips[:-1]): rom.writestr('chip-'+str(i),c)
    rejects(lambda:m.verify_rom(archive),'do not match')

    tree=ast.parse(script.read_text())
    expression=next(n.value for n in ast.walk(tree) if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='backup_cmd' for t in n.targets))
    command=eval(compile(ast.Expression(expression),str(script),'eval'),{'backup':'Documents/TimeCrisisVR-backups/test'})
    home=root/'device';home.mkdir()
    # Redirect only this test command's known paths; preserve the real HOME environment.
    command=command.replace('$HOME',str(home)).replace('home=Path.home()', 'home=Path('+repr(str(home))+')')
    data=home/'Documents/TimeCrisisVR';data.mkdir(parents=True)
    settings=data/'options.ini';settings.write_bytes(b'laser=0\ngunAngle=-7\n');settings.chmod(0o640)
    (data/'sidecar').symlink_to('options.ini')
    title=home/'devkit-game/TimeCrisisVR';title.mkdir(parents=True);(title/'TimeCrisisVR.apk').write_bytes(b'old-apk')
    subprocess.run(['sh','-c',command],check=True)
    backup=home/'Documents/TimeCrisisVR-backups/test'
    manifest=json.loads((backup/'pre-install-manifest.json').read_text())
    assert manifest['data']['entries']['options.ini']['mode']==0o640
    assert (backup/'data/options.ini').read_bytes()==settings.read_bytes()
    assert os.readlink(backup/'data/sidecar')=='options.ini'
    absent=root/'absent-device';absent.mkdir()
    subprocess.run(['sh','-c',command.replace(str(home),str(absent))],check=True)
    absent_backup=absent/'Documents/TimeCrisisVR-backups/test'
    assert (absent_backup/'data-absent').exists() and (absent_backup/'title-absent').exists()
    symlink=root/'symlink-device';symlink.mkdir();(symlink/'Documents').mkdir()
    (symlink/'Documents/TimeCrisisVR').symlink_to(data)
    result=subprocess.run(['sh','-c',command.replace(str(home),str(symlink))],capture_output=True,text=True)
    assert result.returncode!=0 and 'symlink' in result.stderr
    assert not (symlink/'Documents/TimeCrisisVR-backups/test').exists()

    # Exercise the real CLI's safe verification-only path and its explicit ROM requirement.
    (bundle/'SHA256SUMS').write_text(hashlib.sha256(original).hexdigest()+'  payload/TimeCrisisVR.apk\n')
    with zipfile.ZipFile(archive,'w') as rom:
        for i,c in enumerate(chips): rom.writestr('older-name-'+str(i),c)
    old_argv=sys.argv
    try:
        sys.argv=[str(script),'--verify-only']
        m.main()  # Does not require a ROM, Frame host, booking script or device access.
        sys.argv=[str(script)]
        rejects(m.main,'supply your matching Time Crisis ROM ZIP with --rom')
        sys.argv=[str(script),'--dry-run','--rom',str(archive)]
        m.main()  # Checks all 31 synthetic chips and stops before SSH/booking.
    finally:
        sys.argv=old_argv
print('PASS: hashes, archive contents, chip aliases/missing chip, verified backup bytes/modes/symlinks/absent roots, symlink-root rejection, Steam response contract and ROM-free verify-only path.')
