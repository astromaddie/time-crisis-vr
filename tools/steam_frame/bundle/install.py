#!/usr/bin/env python3
"""Verify and install the ROM-free Time Crisis VR local test bundle."""
from pathlib import Path
import argparse, hashlib, json, os, re, shlex, subprocess, sys, time, zipfile

TITLE = 'TimeCrisisVR'
BASE = Path(__file__).resolve().parent
APK = BASE / 'payload' / 'TimeCrisisVR.apk'

def fail(message):
    raise SystemExit('TimeCrisisVR installer: ' + message)

def run(argv, *, text_input=None, capture=False):
    result = subprocess.run(argv, input=text_input, text=True, capture_output=capture)
    if result.returncode:
        fail('command failed: ' + shlex.join(str(x) for x in argv))
    return result.stdout if capture else None

def verify():
    sums = BASE / 'SHA256SUMS'
    if not sums.is_file(): fail('SHA256SUMS is missing')
    entries = []
    for line in sums.read_text().splitlines():
        if not line: continue
        digest, sep, name = line.partition('  ')
        if not sep or not re.fullmatch('[0-9a-f]{64}', digest): fail('invalid checksum entry')
        relative = Path(name)
        if relative.is_absolute() or '..' in relative.parts: fail('unsafe checksum path')
        path = BASE / relative
        if path.is_symlink() or not path.is_file(): fail('missing or symlinked file: ' + name)
        if hashlib.sha256(path.read_bytes()).hexdigest() != digest: fail('checksum mismatch: ' + name)
        entries.append(name)
    if len(entries) != len(set(entries)): fail('duplicate checksum entries')
    if 'payload/TimeCrisisVR.apk' not in entries: fail('APK is not checksummed')
    actual = {str(p.relative_to(BASE)) for p in BASE.rglob('*') if p.is_file() and p.name != 'SHA256SUMS'}
    if actual != set(entries): fail('bundle has missing or unexpected files')
    print('Bundle verified (' + str(len(entries)) + ' files).')

def verify_rom(path):
    try:
        with zipfile.ZipFile(APK) as apk:
            manifest = apk.read('assets/roms.sha256').decode('ascii')
        required = {line.split()[0] for line in manifest.splitlines() if line.strip()}
        if len(required) != 31: fail('APK ROM manifest does not contain 31 required chips')
        with zipfile.ZipFile(path) as archive:
            if archive.testzip(): fail('ROM archive has a CRC error')
            supplied = {hashlib.sha256(archive.read(info)).hexdigest() for info in archive.infolist() if not info.is_dir()}
        if required - supplied: fail('ROM chips do not match this build')
    except (zipfile.BadZipFile, KeyError, UnicodeError) as error:
        fail('cannot verify ROM: ' + str(error))
    print('ROM verified (31 required chips; older filenames accepted).')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--verify-only', action='store_true')
    parser.add_argument('--dry-run', action='store_true')
    parser.add_argument('--host', default=os.environ.get('FRAME_HOST', 'steamos@frame.local'))
    parser.add_argument('--rom', type=Path, default=Path.home() / 'Downloads' / 'timecris.zip')
    parser.add_argument('--host-key-alias', default=os.environ.get('FRAME_HOST_KEY_ALIAS', '10.86.200.233'))
    parser.add_argument('--lock-script', type=Path, default=Path.home() / '.codex/skills/steam-frame-vr-port/scripts/frame-lock.sh')
    args = parser.parse_args()
    verify()
    if args.verify_only: return
    if not re.fullmatch(r'[A-Za-z0-9_.@:-]+', args.host) or args.host.startswith('-'): fail('invalid SSH host')
    if not args.rom.is_file(): fail('ROM not found: ' + str(args.rom))
    verify_rom(args.rom)
    if args.host_key_alias and (not re.fullmatch(r'[A-Za-z0-9_.:-]+', args.host_key_alias) or args.host_key_alias.startswith('-')): fail('invalid host key alias')
    if args.dry_run:
        print('Would book Frame, back up existing TimeCrisisVR data/title, copy the APK and your ROM, and register the Steam title on ' + args.host + '.')
        return
    if not args.lock_script.is_file(): fail('shared Frame booking script missing: ' + str(args.lock_script))
    ssh = ['ssh', '-o', 'BatchMode=yes', '-o', 'StrictHostKeyChecking=yes', '-o', 'ConnectTimeout=10']
    if args.host_key_alias: ssh += ['-o', 'HostKeyAlias=' + args.host_key_alias]
    ssh += [args.host]
    def remote(command, capture=False): return run(ssh + [command], capture=capture)
    remote('true')
    holder = 'timecrisisvr-install'
    run([str(args.lock_script), 'claim', holder, 'Time Crisis VR: verified local bundle installation', '10'])
    try:
        busy = remote("podman ps --format '{{.Names}}'", capture=True)
        if any('lepton-steamlaunch-' in name or name == 'lepton-dev' for name in busy.splitlines()):
            fail('a Lepton game/container is active; close it before installation')
        focused = remote("journalctl --user --since '-1 min' --no-pager | grep -E 'session focused|session FOCUSED' || true", capture=True)
        if focused.strip(): fail('a recent focused VR session was observed; wait until the headset is free')
        running = remote("ps -eo args | grep -E 'devkit-game/|OpenXR|openxr' | grep -v -E 'grep|vrserver|vrcompositor|vrmonitor' || true", capture=True)
        if running.strip(): fail('a scene application may be active; close it before installation')
        stamp = time.strftime('%Y%m%dT%H%M%SZ', time.gmtime())
        backup = 'Documents/TimeCrisisVR-backups/' + stamp
        backup_cmd = '''set -eu
backup="$HOME/''' + backup + '''"
if test -L "$HOME/Documents/TimeCrisisVR" || test -L "$HOME/devkit-game/TimeCrisisVR"; then
  echo 'Expected TimeCrisisVR directory is a symlink; installation stopped before copying.' >&2
  exit 1
fi
mkdir -p "$backup"
if test -d "$HOME/Documents/TimeCrisisVR"; then cp -a "$HOME/Documents/TimeCrisisVR" "$backup/data"; else touch "$backup/data-absent"; fi
if test -d "$HOME/devkit-game/TimeCrisisVR"; then cp -a "$HOME/devkit-game/TimeCrisisVR" "$backup/title"; else touch "$backup/title-absent"; fi
python3 - "$backup" <<'VERIFY_BACKUP'
from pathlib import Path
import hashlib,json,os,stat,sys
backup=Path(sys.argv[1]); home=Path.home()
def snapshot(root):
    entries={}
    for path in [root]+sorted(root.rglob('*')):
        info=path.lstat(); item={'mode':stat.S_IMODE(info.st_mode)}
        if path.is_symlink(): item['symlink']=os.readlink(path)
        elif path.is_file(): item.update(bytes=info.st_size,sha256=hashlib.sha256(path.read_bytes()).hexdigest())
        elif path.is_dir(): item['directory']=True
        else: raise SystemExit('Unsupported file in backup: '+str(path))
        entries[str(path.relative_to(root))]=item
    return entries
manifest={}
for label,source in [('data',home/'Documents/TimeCrisisVR'),('title',home/'devkit-game/TimeCrisisVR')]:
    if source.exists():
        before=snapshot(source); copied=snapshot(backup/label)
        if before!=copied: raise SystemExit('Backup does not match '+str(source))
        manifest[label]={'original':str(source),'entries':before}
    else: manifest[label]={'original':str(source),'absent':True}
(backup/'pre-install-manifest.json').write_text(json.dumps(manifest,indent=2)+'\\n')
print('Existing title/data backup verified: '+str(backup))
VERIFY_BACKUP
mkdir -p "$HOME/Documents/TimeCrisisVR" "$HOME/devkit-game/TimeCrisisVR"
chmod 777 "$HOME/Documents/TimeCrisisVR"
'''
        remote(backup_cmd)
        scp = ['scp', '-q', '-o', 'BatchMode=yes', '-o', 'StrictHostKeyChecking=yes']
        if args.host_key_alias: scp += ['-o', 'HostKeyAlias=' + args.host_key_alias]
        staged_apk='devkit-game/TimeCrisisVR/.TimeCrisisVR.apk.new-'+stamp
        staged_rom='Documents/TimeCrisisVR/.timecris.zip.new-'+stamp
        run(scp + [str(APK), args.host + ':'+staged_apk])
        run(scp + [str(args.rom), args.host + ':'+staged_rom])
        apk_sha = hashlib.sha256(APK.read_bytes()).hexdigest()
        rom_sha = hashlib.sha256(args.rom.read_bytes()).hexdigest()
        remote("printf '%s\\n' " + shlex.quote(apk_sha + '  '+staged_apk) + ' ' + shlex.quote(rom_sha + '  '+staged_rom) + ' | (cd "$HOME" && sha256sum -c -)')
        remote('set -eu; cd "$HOME"; chmod 644 '+shlex.quote(staged_rom)+'; mv '+shlex.quote(staged_rom)+' Documents/TimeCrisisVR/timecris.zip; mv '+shlex.quote(staged_apk)+' devkit-game/TimeCrisisVR/TimeCrisisVR.apk')
        parms = {'gameid': TITLE, 'directory': '/home/steamos/devkit-game/' + TITLE, 'force_appid': '', 'argv': [TITLE + '.apk'], 'env': {}, 'settings': {'steam_play': '1', 'compat_tool': 'lepton'}, 'lepton_args': ''}
        reply = remote('cd "$HOME/devkit-utils" && python3 steam-client-create-shortcut --parms ' + shlex.quote(json.dumps(parms)), capture=True)
        try: registered = json.loads(reply)
        except json.JSONDecodeError: fail('Steam registration returned an unreadable response: ' + reply.strip())
        if registered.get('error') or not registered.get('success'):
            fail('Steam title registration failed: ' + str(registered.get('error', registered)))
        print('Steam registration: ' + str(registered['success']))
        print('Installed and registered ' + TITLE + '. Launch it from the Steam library.')
        print('Pre-install backup: /home/steamos/' + backup)
    finally:
        run([str(args.lock_script), 'release', holder])

if __name__ == '__main__': main()
