"""Render exact production shaders and guide vertices in host Chrome/WebGL2.

This is host evidence only: no headset, game-data or device access. Pixel checks
compare the mask against analytic eye rays at stereo, yaw and near-plane poses.
"""
from pathlib import Path
import hashlib, json, re, shutil, subprocess, tempfile, os, sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from host_paths import executable
source=(ROOT/'quest/quest_gl.c').read_text()
compiler=str(executable('cc'))
chrome=os.environ.get('CHROME') or shutil.which('google-chrome') or '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'
if not Path(chrome).is_file(): raise SystemExit('Chrome unavailable; host GLES rendering not verified')
out=ROOT/'build/play-area-render';out.mkdir(parents=True,exist_ok=True)
# Compile the actual conditional C declarations: no hand-built shader variant.
start=source.index('#ifdef TCVR_STEAM_FRAME\nstatic const char *world_vertex=')
end=source.index('static const char *uniform_names[]=',start)
extract='#include <stdio.h>\n'+source[start:end]+'\nint main(void){fputs(world_vertex,stdout);fputc(0,stdout);fputs(world_fragment,stdout);}'
(out/'shaders.c').write_text(extract)
# SHA256 of exact pre-change Quest strings at 1cbab47123122e658e02a786dec970f7897de2c1.
quest_hashes=['16be240a9b26d71804eaffc26a742e96b16401900620a50d68383376e6fa3fc0', '112a413a60981d351f2e459ce87a02b102700e3002665e712a88576f3f14d725']
for frame in (False,True):
    flags=['-DTCVR_STEAM_FRAME'] if frame else []
    subprocess.run([compiler,'-std=c11',*flags,str(out/'shaders.c'),'-o',str(out/'shaders')],check=True)
    strings=subprocess.check_output([out/'shaders']).split(b'\0')
    assert len(strings)==2
    if not frame:
        assert [hashlib.sha256(x).hexdigest() for x in strings]==quest_hashes
        assert all(b'outsideMask' not in x and b'maskRay' not in x and b'inverse(' not in x for x in strings)
        print('PASS: Quest flag-off shader strings byte-identical to pre-change baseline')
    else:
        shaders=[x.decode() for x in strings]

production=source[source.index('static void play_area_stroke('):source.index('\n#endif',source.index('static void play_area_stroke('))]
c='''#include <stdio.h>
#include <string.h>
typedef unsigned GLenum;typedef unsigned char GLboolean;
typedef struct {float p[4],c[4],uv[4];} Vertex;
enum{GL_TRIANGLES=4,GL_SRC_ALPHA=770,GL_ONE_MINUS_SRC_ALPHA=771};
static int texture_on[2],alpha_on,alpha_replace,blend_on;
static GLenum blend_src,blend_dst;static float rgb_scale;static GLboolean color_mask[4];
static int call;
static void draw(const Vertex *v,int n,GLenum mode){(void)mode;if(call++)printf(",");printf("[");for(int i=0;i<n;i++){if(i)printf(",");printf("[");const float *p=(const float*)&v[i];for(int j=0;j<12;j++)printf("%s%.9g",j?",":"",p[j]);printf("]");}printf("]");}
'''+production+'\nint main(void){printf("[");qgl_play_area(.65f);puts("]");}\n'
(out/'vertices.c').write_text(c)
subprocess.run([compiler,'-std=c11',str(out/'vertices.c'),'-o',str(out/'vertices')],check=True)
vertices=json.loads(subprocess.check_output([out/'vertices'],text=True))
script=r'''
const shaders=SHADERS, vertices=VERTICES;
const cases=[['front left',0,[-.032,0,0]],['front right',0,[.032,0,0]],['yaw45 left',45,[-.032,.15,.2]],['yaw45 right',45,[.032,.15,.2]],['yaw89',89,[0,0,0]],['rear',180,[0,0,0]],['near plane',0,[0,0,-2.49]],['on plane',0,[0,0,-2.5]],['beyond plane',0,[0,0,-2.6]],['rear beyond',180,[0,0,-2.6]],['translated',-25,[1,.4,.5]]];
let checked=0;const surface=document.createElement('canvas');surface.width=256;surface.height=192;const gl=surface.getContext('webgl2',{antialias:false,preserveDrawingBuffer:true});
function assert(x,m){if(!x)throw Error(m)}
for(const scale of [1,1.35,1.6])for(const [label,angle,o] of cases){
const canvas=document.createElement('canvas');canvas.width=256;canvas.height=192;const box=document.createElement('div');box.append(`${label} scale ${scale}`,canvas);document.body.append(box);
assert(gl,'WebGL2 unavailable');
const program=gl.createProgram();for(let i=0;i<2;i++){const shader=gl.createShader(i?gl.FRAGMENT_SHADER:gl.VERTEX_SHADER);gl.shaderSource(shader,shaders[i]);gl.compileShader(shader);assert(gl.getShaderParameter(shader,gl.COMPILE_STATUS),gl.getShaderInfoLog(shader));gl.attachShader(program,shader);}gl.linkProgram(program);assert(gl.getProgramParameter(program,gl.LINK_STATUS),gl.getProgramInfoLog(program));gl.useProgram(program);
const a=angle*Math.PI/180,c=Math.cos(a),s=Math.sin(a),fx=1.1,fy=1.35;
const view=[c,0,s,0, 0,1,0,0, -s,0,c,0, -c*o[0]+s*o[2],-o[1],-s*o[0]-c*o[2],1];
const proj=[fx,0,0,0,0,fy,0,0,0,0,-1.0002,-1,0,0,-.020002,0];
gl.uniformMatrix4fv(gl.getUniformLocation(program,'view[0]'),false,view);gl.uniformMatrix4fv(gl.getUniformLocation(program,'proj[0]'),false,proj);
const pixelScale=2.5/(772.6/scale);gl.uniform4f(gl.getUniformLocation(program,'flatCam'),312,236,pixelScale,2.5);gl.uniform1i(gl.getUniformLocation(program,'tex'),0);gl.uniform1i(gl.getUniformLocation(program,'atlasTex'),2);
gl.clearColor(1,1,1,1);gl.clear(gl.COLOR_BUFFER_BIT);gl.enable(gl.BLEND);gl.blendFunc(gl.SRC_ALPHA,gl.ONE_MINUS_SRC_ALPHA);
const buffer=gl.createBuffer();gl.bindBuffer(gl.ARRAY_BUFFER,buffer);for(let i=0;i<3;i++){gl.enableVertexAttribArray(i);gl.vertexAttribPointer(i,4,gl.FLOAT,false,48,i*16);}
function draw(v){gl.bufferData(gl.ARRAY_BUFFER,new Float32Array(v.flat()),gl.STREAM_DRAW);gl.drawArrays(gl.TRIANGLES,0,v.length);}
draw(vertices[0]);assert(gl.getError()===gl.NO_ERROR,'draw GL error');
const pixels=new Uint8Array(256*192*4);gl.readPixels(0,0,256,192,gl.RGBA,gl.UNSIGNED_BYTE,pixels);
for(let y=0;y<192;y+=3)for(let x=0;x<256;x+=3){
 const dx=(2*(x+.5)/256-1)/fx,dy=(2*(y+.5)/192-1)/fy;
 const d=[c*dx-s,dy,-s*dx-c],t=(-2.5-o[2])/d[2];
 const px=312+(o[0]+t*d[0])/pixelScale,py=236-(o[1]+t*d[1])/pixelScale;
 if(Math.min(Math.abs(px),Math.abs(px-640),Math.abs(py),Math.abs(py-480))<.02)continue;
 const inside=t>0&&px>=0&&px<=640&&py>=0&&py<=480;
 const expected=inside?255:89,actual=pixels[(y*256+x)*4];assert(Math.abs(actual-expected)<=1,`${label} ${scale} pixel ${x},${y}: ${actual} expected ${expected}`);checked++;
}
// Draw exact production cyan/backing rings after mask, preserving their alpha.
draw(vertices[1]);draw(vertices[2]);canvas.getContext('2d').drawImage(surface,0,0);gl.deleteProgram(program);gl.deleteBuffer(buffer);
}
document.title='PASS';document.getElementById('result').textContent=`PASS: ${checked} mask pixel checks; 33 production-shader stereo/head-pose/scale renders`;
'''.replace('SHADERS',json.dumps(shaders)).replace('VERTICES',json.dumps(vertices))
html='<html><head><style>body{background:#222;color:white;font:14px sans-serif;display:flex;flex-wrap:wrap;gap:12px}canvas{display:block}#result{width:100%}</style></head><body><p id="result">RUNNING</p><script>try{'+script+'}catch(e){document.title="FAIL";document.getElementById("result").textContent="FAIL: "+e.stack}</script></body></html>'
(out/'index.html').write_text(html)
with tempfile.TemporaryDirectory(prefix='tcvr-render-browser-') as profile:
    run=subprocess.run([chrome,'--headless','--no-sandbox','--no-first-run','--no-default-browser-check','--disable-background-networking','--use-angle=metal','--enable-unsafe-swiftshader','--user-data-dir='+profile,'--disable-gpu-sandbox','--hide-scrollbars','--window-size=1400,2200','--screenshot='+str(out/'guide-matrix.png'),'--dump-dom',(out/'index.html').as_uri()],capture_output=True,text=True,timeout=60)
    (out/'browser.log').write_text(f'exit={run.returncode}\n'+run.stderr+'\n'+run.stdout)
    result=re.search(r'<p id="result">(.*?)</p>',run.stdout,re.S)
    assert result and result[1].startswith('PASS:'), (result[1] if result else run.stderr[-3000:])
    receipt={'command': ' '.join([sys.executable,*sys.argv]), 'head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(), 'result':result[1], 'source_sha256':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [ROOT/'quest/quest_gl.c',ROOT/'quest/quest_scene.c',Path(__file__).resolve()]}, 'level':'Host Chrome WebGL2/ANGLE Metal only; device and worn pending'}
    (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print(result[1]);print('Host render:',out/'guide-matrix.png')
