"""Generated Quest integration patches are exact, idempotent and leave the submodule clean."""
from pathlib import Path
import importlib.util, os, subprocess, tempfile
ROOT=Path(__file__).resolve().parents[1]
GENERATED_VIEW=Path(os.environ.get('TCVR_UPSTREAM_SOURCE',ROOT/'.tools/upstream-steam-frame'))
spec=importlib.util.spec_from_file_location('patch_upstream',ROOT/'tools/patch_upstream.py')
patch=importlib.util.module_from_spec(spec);spec.loader.exec_module(patch)
names=tuple('engine/'+n for n in ('geo_hw.h','geo_hw.c','ss22_gl.c','quad_gl.c','tex_bake.c','sprite_hw.c','text_hw.c'))+('timecris/gen/tc_lifted_tab.c',)
with tempfile.TemporaryDirectory(prefix='tcvr-patch-') as directory:
    patch.ROOT=Path(directory)
    original={}
    for name in names:
        source=subprocess.check_output(['git','show','HEAD:'+name],cwd=ROOT/'upstream')
        original[name]=source
        (patch.ROOT/name).parent.mkdir(parents=True,exist_ok=True)
        (patch.ROOT/name).write_bytes(source)
    patch.main()
    once={name:(patch.ROOT/name).read_text() for name in names}
    patch.main()
    for name in names:
        assert once[name]==(patch.ROOT/name).read_text(),('not idempotent',name)
        assert once[name].encode()==(GENERATED_VIEW/name).read_bytes(),('patch differs from exact generated build view',name)
        assert once[name].encode()!=original[name],('patch did not modify expected source',name)
        assert (ROOT/'upstream'/name).read_bytes()==original[name],('canonical submodule changed',name)
status=subprocess.check_output(['git','status','--porcelain'],cwd=ROOT/'upstream',text=True)
assert not status,('canonical upstream worktree is dirty',status)
print('PASS: 8 Frame build-view patches change the pinned sources once, reproduce idempotently and leave canonical upstream clean.')
