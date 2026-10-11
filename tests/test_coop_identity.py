"""Exercise the production co-op fingerprint across independent target build views."""
import importlib.util
from pathlib import Path
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools'))
spec = importlib.util.spec_from_file_location('time_crisis_build', ROOT/'tools/build.py')
build = importlib.util.module_from_spec(spec)
spec.loader.exec_module(build)

with tempfile.TemporaryDirectory(prefix='tcvr-identity-') as temporary:
    root = Path(temporary)
    upstream_a = root/'frame-mirror'
    upstream_b = root/'quest-checkout'
    for upstream in (upstream_a, upstream_b):
        for name in build.SIMULATION_FILES:
            path=upstream/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(('source:'+name+'\n').encode())
        lifted=upstream/'timecris/gen/tc_lifted_sample.c';lifted.parent.mkdir(parents=True,exist_ok=True);lifted.write_bytes(b'lifted simulation source\n')
        renderer=upstream/'engine/ss22_gl.c';renderer.parent.mkdir(parents=True,exist_ok=True);renderer.write_bytes(b'target-specific renderer\n')
    for name,data in [('quest/quest_coop.c',b'protocol source\n'),('quest/quest_coop.h',b'protocol header\n'),('build/tc_crate.c',b'generated shared crate\n')]:
        path=root/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
    manifest=''.join(f'{i:064x}  chip-{i:02}.bin\n' for i in range(31))+'0'*64+'  c71.bin\n'
    assets=root/'assets';assets.mkdir();(assets/'roms.sha256').write_text(manifest,encoding='ascii')

    build.ROOT=root;build.BUILD=root/'build/frame';build.UPSTREAM=upstream_a
    frame=build.write_coop_identity(assets)
    frame_header=(build.BUILD/'generated/coop_identity.h').read_text()
    build.UPSTREAM=upstream_b;build.BUILD=root/'build/quest'
    quest=build.write_coop_identity(assets)
    quest_header=(build.BUILD/'generated/coop_identity.h').read_text()
    assert frame['simulation_sha256']==quest['simulation_sha256']
    assert frame['rom_manifest_sha256']==quest['rom_manifest_sha256']
    assert frame_header==quest_header

    shared=upstream_b/'engine/ss22_run.c';shared.write_bytes(shared.read_bytes()+b'changed simulation behavior\n')
    changed=build.write_coop_identity(assets)
    assert changed['simulation_sha256']!=quest['simulation_sha256']
    shared.write_bytes(shared.read_bytes().removesuffix(b'changed simulation behavior\n'))
    (upstream_b/'engine/ss22_gl.c').write_bytes(b'different renderer only\n')
    renderer_only=build.write_coop_identity(assets)
    assert renderer_only['simulation_sha256']==quest['simulation_sha256']

print('PASS: production identity matches separate identical upstream mirrors, changes for shared simulation input, and ignores renderer-only changes.')
