"""Generated Quest integration patches are exact, idempotent and leave the submodule clean."""
from pathlib import Path
import importlib.util, os, subprocess, tempfile
ROOT=Path(__file__).resolve().parents[1]
GENERATED_VIEW=Path(os.environ.get('TCVR_UPSTREAM_SOURCE',ROOT/'.tools/upstream-steam-frame'))
spec=importlib.util.spec_from_file_location('patch_upstream',ROOT/'tools/patch_upstream.py')
patch=importlib.util.module_from_spec(spec);spec.loader.exec_module(patch)
names=tuple('engine/'+n for n in ('geo_hw.h','geo_hw.c','ss22_gl.c','ss22_run.c','ss22_board.c','ss22_board.h','quad_gl.c','tex_bake.c','sprite_hw.c','text_hw.c'))+('timecris/gen/tc_lifted_tab.c',)
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
    run_source=once['engine/ss22_run.c']
    assert 'ss22_frame_replay_tick(rr_frame);' in run_source
    assert 'if (win_scale && tcvr_frame_replay_eeprom_persist_allowed(rep_path != NULL)) ss22_eeprom_persist(nvfile);' in run_source
    assert 'static void in_update(void) { if (!start_on && !autoplay && !rep_f) ss22_input_update(); }' in run_source
    assert 'qcoop_launch_arguments_supported(argc,argv)' in run_source
    assert 'qcoop_frame_diagnostics_pending()' in run_source
    assert 'getenv("TCVR_FRAME_REPLAY")!=NULL' not in run_source
    assert 'qcoop_engine_boot(g_ss22.eeprom,SS22_EEPROM_SIZE,rep_path!=NULL,diagnostics_pending)' in run_source
    assert 'ss22_eeprom_set_writable(true);' in run_source
    assert 'coop_boot>0 && qcoop_is_client()) ss22_eeprom_set_writable(false);' in run_source
    assert '&& !qcoop_requested()) { snprintf(last_rec' in run_source
    assert '#else\n    if (win_scale && !rec_path && !rep_path) { snprintf(last_rec' in run_source
    boot=run_source.index('int coop_boot=qcoop_engine_boot')
    recorder=run_source.index('static char last_rec[80]')
    assert boot < recorder
    assert run_source.rfind('#endif',0,boot)>run_source.index('TCVR_FRAME_REPLAY_SAFE')
    assert 'if (!nv_on || !nv_writable || !memcmp(nv_saved, g_ss22.eeprom, SS22_EEPROM_SIZE)) return;' in once['engine/ss22_board.c']
    gl_source=once['engine/ss22_gl.c']
    mixer_report='qvr_scene_report_mixer(g_fog_valid, g_fog.bg, g_fog.fog_rgb, g_fog.poly_fade_enabled, g_fog.poly_fade, g_fog.screen_fade, g_fog.screen_fade_factor, g_fog.mixer_flags);'
    assert mixer_report in gl_source
    assert gl_source.index('gun_flash_filter();') < gl_source.index(mixer_report), 'snapshot must report the values the renderer will draw after flash filtering'
status=subprocess.check_output(['git','status','--porcelain'],cwd=ROOT/'upstream',text=True)
assert not status,('canonical upstream worktree is dirty',status)
print('PASS: Frame build-view patches change the pinned sources once, preserve replay and co-op tick/EEPROM guards, reproduce idempotently and leave canonical upstream clean.')
