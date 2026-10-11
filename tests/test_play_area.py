"""Run the production Frame guide geometry against a recording draw sink.

No GLES context required. The sink checks queued state; production geometry is
compiled verbatim, then measured for winding, coverage, stroke width and alpha.
"""
from pathlib import Path
import subprocess, sys
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools'))
from host_paths import executable
source = (ROOT/'quest/quest_gl.c').read_text()
start = source.index('static void play_area_stroke(')
end = source.index('\n#endif', start)
production = source[start:end]
out = ROOT/'build/play-area-test'
out.mkdir(parents=True, exist_ok=True)
harness = r'''
#include <assert.h>
#include <math.h>
#include <string.h>
typedef unsigned GLenum;
typedef unsigned char GLboolean;
typedef struct {float p[4],c[4],uv[4];} Vertex;
enum {GL_TRIANGLES=4,GL_SRC_ALPHA=770,GL_ONE_MINUS_SRC_ALPHA=771};
static int texture_on[2]={1,1},alpha_on=1,alpha_replace=1,blend_on=0;
static GLenum blend_src=1,blend_dst=0;
static float rgb_scale=2.5f;
static GLboolean color_mask[4]={0,1,0,1};
static Vertex recorded[2][24];static int calls;
static void draw(const Vertex *v,int n,GLenum mode){
    assert(calls<2&&n==24&&mode==GL_TRIANGLES);
    assert(!texture_on[0]&&!texture_on[1]&&!alpha_on&&!alpha_replace);
    assert(blend_on&&blend_src==GL_SRC_ALPHA&&blend_dst==GL_ONE_MINUS_SRC_ALPHA);
    assert(rgb_scale==1);
    for(int i=0;i<4;i++)assert(color_mask[i]==1);
    memcpy(recorded[calls++],v,24*sizeof *v);
}
'''
harness += production
harness += r'''
static void near(float a,float b){assert(fabsf(a-b)<.0002f);}
int main(void){
    qgl_play_area();assert(calls==2);
    assert(texture_on[0]==1&&texture_on[1]==1&&alpha_on==1&&alpha_replace==1);
    assert(!blend_on&&blend_src==1&&blend_dst==0);
    assert(rgb_scale==2.5f);
    assert(!color_mask[0]&&color_mask[1]&&!color_mask[2]&&color_mask[3]);
    for(int layer=0;layer<2;layer++){
        float w=layer?2:5,h=w/2,area=0;
        Vertex *v=recorded[layer];
        for(int i=0;i<24;i++){
            assert(v[i].p[2]==0&&v[i].p[3]==0); // same flatCam shader path as HUD
            near(v[i].c[0],(layer?114:16)/255.f);
            near(v[i].c[1],(layer?213:25)/255.f);
            near(v[i].c[2],(layer?232:27)/255.f);
            near(v[i].c[3],layer?.85f:.8f);
        }
        for(int i=0;i<24;i+=3){
            float cross=(v[i+1].p[0]-v[i].p[0])*(v[i+2].p[1]-v[i].p[1])-
                        (v[i+1].p[1]-v[i].p[1])*(v[i+2].p[0]-v[i].p[0]);
            assert(cross>0);area+=cross/2;
        }
        near(area,2*w*(640+480));
        near(v[0].p[0],-h);near(v[0].p[1],-h);
        near(v[2].p[0],640+h);near(v[2].p[1],h);
        near(v[8].p[1],480+h);
        // The vertical bars meet the horizontal bars, with no overlap/gap.
        near(v[12].p[1],v[2].p[1]);near(v[14].p[1],v[6].p[1]);
        near(v[18].p[0],640-h);near(v[20].p[0],640+h);
    }
    return 0;
}
'''
(out/'fixture.c').write_text(harness)
subprocess.run([str(executable('cc')), '-std=c11', '-Wall', '-Wextra', '-Werror', str(out/'fixture.c'), '-o', str(out/'fixture')], check=True)
subprocess.run([str(out/'fixture')], check=True)
host = (ROOT/'quest/quest_host.c').read_text()
assert '#ifdef TCVR_STEAM_FRAME\n            if(!paused&&!qvr_flat_view)qgl_play_area();\n#endif' in host
assert host.index('if(color_probe){') < host.index('qgl_play_area();')
assert host.index('if(stereo_frame)qgl_stereo_blit(i);else ss22_draw') < host.index('qgl_play_area();') < host.index('qgl_pointer(gun_origin,gun_hit)')
assert 'p=vec3((aPos.x-flatCam.x)*flatCam.z,(flatCam.y-aPos.y)*flatCam.z,-flatCam.w)' in source
assert 'gl_Position=proj[EYE]*view[EYE]*vec4(p,1)' in source
print('PASS: production full-frame rings, color/alpha, flat-camera path, gating, draw order and restored queued state')
