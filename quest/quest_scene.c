#include "quest_scene.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <float.h>
float qvr_units_per_meter=1000.0f;
int qvr_flat_view;

typedef struct { float focal,cx,cy; } Camera;
typedef struct { V3 a,b,c; Camera camera;
#ifdef TCVR_STEAM_FRAME
    uint32_t zsort;int order;
#endif
} Triangle;
static Triangle triangles[65536];
static unsigned count,frame_number;
static float farthest;
/* Time Crisis's main camera, retained on title/menu frames without polygons. */
static Camera flat_camera={772.5625f,320,240};
static struct { Camera camera;unsigned weight; } cameras[64];
static unsigned camera_count;
static struct { sprite_item item;float depth; } sprites[1024];
static int sprite_count;
#ifdef TCVR_STEAM_FRAME
static int sprite_active_count;
typedef struct {
    bool valid, poly_fade_enabled;
    uint8_t background[3], fog_rgb[3], poly_fade[3], screen_fade[3];
    uint8_t screen_fade_factor, mixer_flags;
} SceneMixerSnapshot;
static SceneMixerSnapshot scene_mixer_snapshot;
#endif
/* Cache shot-mark depths once per game frame. The bounded screen grid avoids
 * rescanning every polygon for every animated mark; overflow uses a full scan. */
enum { GRID_W=20,GRID_H=15,GRID_LINKS=131072 };
static int grid[GRID_W*GRID_H];
static struct { unsigned triangle;int next; } links[GRID_LINKS];
static unsigned link_count;
static bool grid_overflow;
#ifdef TCVR_STEAM_FRAME
static bool report_grid_ready;
#endif

static bool camera_valid(Camera c){return isfinite(c.focal)&&c.focal>0&&isfinite(c.cx)&&isfinite(c.cy);}
static Camera quad_camera(const geo_quad *q){return (Camera){q->vr_focal,q->vr_cx,q->vr_cy};}
static V3 unproject(Camera c,float x,float y,float z){return v3((x-c.cx)*z/c.focal,(c.cy-y)*z/c.focal,-z);}
bool qvr_flat_camera(float *cx,float *cy,float *focal){
    Camera c=qvr_flat_view?(Camera){500,320,240}:flat_camera;
    *cx=c.cx;*cy=c.cy;*focal=c.focal;return !qvr_flat_view;
}
void qvr_vertex(const geo_quad *q,float x,float y,float z,float out[4]){
    if(!qvr_flat_view&&!q->direct&&camera_valid(quad_camera(q))&&isfinite(z)&&z>0){
        V3 p=unproject(quad_camera(q),x,y,z/qvr_units_per_meter);
        out[0]=p.x;out[1]=p.y;out[2]=p.z;out[3]=1;
    }else{out[0]=x;out[1]=y;out[2]=0;out[3]=0;}
}
void qvr_scene_begin(void){count=0;camera_count=0;farthest=0;sprite_count=0;frame_number++;
#ifdef TCVR_STEAM_FRAME
    sprite_active_count=0;report_grid_ready=false;scene_mixer_snapshot=(SceneMixerSnapshot){0};
#endif
}
void qvr_scene_quad(const geo_quad *q){
    Camera camera=quad_camera(q);
    if(q->direct||!camera_valid(camera)||q->nrv<3||q->nrv>10)return;
    V3 v[10];
    for(int i=0;i<q->nrv;i++){
        float z=q->rv[i].z/qvr_units_per_meter;
        if(!isfinite(z)||z<=0)return;
        v[i]=unproject(camera,q->rv[i].sx16/16.f,q->rv[i].sy16/16.f,z);
        if(z>farthest)farthest=z;
    }
    unsigned cam;
    for(cam=0;cam<camera_count;cam++)if(!memcmp(&camera,&cameras[cam].camera,sizeof camera))break;
    if(cam==camera_count&&camera_count<64){cameras[cam].camera=camera;cameras[cam].weight=0;camera_count++;}
    if(cam<camera_count)cameras[cam].weight+=q->nrv-2;
    if(qvr_flat_view)return;
    for(int i=1;i+1<q->nrv&&count<65536;i++){
#ifdef TCVR_STEAM_FRAME
        triangles[count++]=(Triangle){v[0],v[i],v[i+1],camera,(uint32_t)q->zsort&0xffffffu,q->order};
#else
        triangles[count++]=(Triangle){v[0],v[i],v[i+1],camera};
#endif
    }
}
static bool impact(const sprite_item *it){return it->tile>=QVR_IMPACT_FIRST&&it->tile<=QVR_IMPACT_LAST&&it->z==0;}
#ifdef TCVR_STEAM_FRAME
/* TS2 Ver.B 0x10E12/0x10EDE project a world actor using camera Z, then
 * 0x10E48/0x10F12 add 0x200000 and store it at actor+0x8C. The VICS emitter
 * (0x10C26/0x10C2A) copies that value to attr[0]. Only that world layer is
 * reconstructed here; other priority layers do not establish camera depth.
 * Time Crisis's upstream HUD configuration reserves low depths <= 0x10.
 * Some effects bias this value for painter order, so it remains an authored
 * depth estimate, not a collision surface or a polygon zsort inverse. */
static bool world_sprite(const sprite_item *it){
    uint32_t depth=it->z&0x1fffffu;
    return (it->z>>21)==1&&depth>0x10u&&depth<0x1fffffu;
}
static float world_sprite_depth(const sprite_item *it){
    if(qvr_flat_view||!count||!world_sprite(it)||!isfinite(qvr_units_per_meter)||qvr_units_per_meter<=0)return 0;
    float depth=(it->z&0x1fffffu)/qvr_units_per_meter;
    return isfinite(depth)&&depth>0?depth:0;
}
#endif
static void build_grid(void){
    for(int i=0;i<GRID_W*GRID_H;i++)grid[i]=-1;
    link_count=0;grid_overflow=false;
    for(unsigned i=0;i<count;i++){
        V3 v[3]={triangles[i].a,triangles[i].b,triangles[i].c};
        float x0=FLT_MAX,y0=FLT_MAX,x1=-FLT_MAX,y1=-FLT_MAX;
        for(int j=0;j<3;j++){
            float x=flat_camera.cx+flat_camera.focal*v[j].x/-v[j].z;
            float y=flat_camera.cy-flat_camera.focal*v[j].y/-v[j].z;
            x0=fminf(x0,x);x1=fmaxf(x1,x);y0=fminf(y0,y);y1=fmaxf(y1,y);
        }
        if(x1<0||y1<0||x0>=640||y0>=480)continue;
        int left=(int)(fmaxf(0,x0)/32),right=(int)(fminf(639,x1)/32);
        int top=(int)(fmaxf(0,y0)/32),bottom=(int)(fminf(479,y1)/32);
        for(int y=top;y<=bottom;y++)for(int x=left;x<=right;x++){
            if(link_count==GRID_LINKS){grid_overflow=true;
#ifdef TCVR_STEAM_FRAME
                report_grid_ready=true;
#endif
                return;}
            int cell=y*GRID_W+x;links[link_count].triangle=i;links[link_count].next=grid[cell];grid[cell]=(int)link_count++;
        }
    }
#ifdef TCVR_STEAM_FRAME
    report_grid_ready=true;
#endif
}
#ifdef TCVR_STEAM_FRAME
static bool surface_hit(float x,float y,float *depth,uint32_t *zsort,int *order){
    V3 ray=unproject(flat_camera,x,y,1);float nearest=FLT_MAX;int best=-1;
    if(!grid_overflow&&x>=0&&x<640&&y>=0&&y<480){
        for(int n=grid[(int)(y/32)*GRID_W+(int)(x/32)];n>=0;n=links[n].next){
            unsigned index=links[n].triangle;Triangle *t=&triangles[index];float before=nearest;
            if(ray_triangle(v3(0,0,0),ray,t->a,t->b,t->c,&nearest)&&nearest<before)best=(int)index;
        }
    }else for(unsigned i=0;i<count;i++){Triangle *t=&triangles[i];float before=nearest;if(ray_triangle(v3(0,0,0),ray,t->a,t->b,t->c,&nearest)&&nearest<before)best=(int)i;}
    if(best<0)return false;
    if(depth)*depth=nearest;if(zsort)*zsort=triangles[best].zsort;if(order)*order=triangles[best].order;
    return true;
}
static float surface_depth(float x,float y){float depth=0;return surface_hit(x,y,&depth,NULL,NULL)?depth*.98f:0;}
#else
static float surface_depth(float x,float y){
    V3 ray=unproject(flat_camera,x,y,1);float nearest=FLT_MAX;
    if(!grid_overflow&&x>=0&&x<640&&y>=0&&y<480){
        for(int n=grid[(int)(y/32)*GRID_W+(int)(x/32)];n>=0;n=links[n].next){
            Triangle *t=&triangles[links[n].triangle];ray_triangle(v3(0,0,0),ray,t->a,t->b,t->c,&nearest);
        }
    }else for(unsigned i=0;i<count;i++){Triangle *t=&triangles[i];ray_triangle(v3(0,0,0),ray,t->a,t->b,t->c,&nearest);}
    return nearest==FLT_MAX?0:nearest*.98f;
}
#endif
void qvr_sprites_prepare(const sprite_item *items,int n){
    if(camera_count){unsigned best=0;for(unsigned i=1;i<camera_count;i++)if(cameras[i].weight>cameras[best].weight)best=i;flat_camera=cameras[best].camera;}
    bool diagnostic=getenv("TCVR_GEOMETRY_DIAGNOSTICS")!=NULL;
    if(diagnostic){
        fprintf(stderr,"[GEOMETRY] frame %u; cameras %u; main %.6f %.3f %.3f; farthest %.1f m; triangles %u\n",frame_number,camera_count,flat_camera.focal,flat_camera.cx,flat_camera.cy,farthest,count);
        for(unsigned i=0;i<camera_count;i++)fprintf(stderr,"[CAMERA] %.6f %.3f %.3f weight %u\n",cameras[i].camera.focal,cameras[i].camera.cx,cameras[i].camera.cy,cameras[i].weight);
    }
    int active_count=n<0?0:n;sprite_count=active_count>1024?1024:active_count;
#ifdef TCVR_STEAM_FRAME
    sprite_active_count=active_count;
#endif
    bool grid_built=false;
    for(int i=0;i<sprite_count;i++){
        sprites[i].item=items[i];sprites[i].depth=0;
#ifdef TCVR_STEAM_FRAME
        sprites[i].depth=world_sprite_depth(&items[i]);
#endif
        if(impact(&items[i])){
            if(!qvr_flat_view){
                if(!grid_built){build_grid();grid_built=true;}
                sprites[i].depth=surface_depth(items[i].x0+items[i].w*.5f,items[i].y0+items[i].h*.5f);
            }
            if(diagnostic)fprintf(stderr,"[IMPACT] frame %u tile %d key %u xy %d %d size %d %d depth %.4f\n",frame_number,items[i].tile,items[i].z,items[i].x0,items[i].y0,items[i].w,items[i].h,sprites[i].depth);
        }
    }
}
#ifdef TCVR_STEAM_FRAME
static bool sprite_report_armed,sprite_report_done;
unsigned qvr_scene_frame_number(void){return frame_number;}
void qvr_sprite_report_arm(bool enabled){sprite_report_armed=enabled;sprite_report_done=false;}
void qvr_scene_report_mixer(bool valid,const uint8_t background[3],const uint8_t fog_rgb[3],
                            bool poly_fade_enabled,const uint8_t poly_fade[3],
                            const uint8_t screen_fade[3],uint8_t screen_fade_factor,uint8_t mixer_flags){
    if(!sprite_report_armed||sprite_report_done)return;
    if(!valid||!background||!fog_rgb||!poly_fade||!screen_fade){scene_mixer_snapshot.valid=false;return;}
    scene_mixer_snapshot.valid=true;
    memcpy(scene_mixer_snapshot.background,background,3);
    memcpy(scene_mixer_snapshot.fog_rgb,fog_rgb,3);
    memcpy(scene_mixer_snapshot.poly_fade,poly_fade,3);
    memcpy(scene_mixer_snapshot.screen_fade,screen_fade,3);
    scene_mixer_snapshot.poly_fade_enabled=poly_fade_enabled;
    scene_mixer_snapshot.screen_fade_factor=screen_fade_factor;
    scene_mixer_snapshot.mixer_flags=mixer_flags;
}
static const char *sprite_classification(const sprite_item *item,float projected_depth){
    if(world_sprite(item)){
        if(qvr_flat_view)return "world_candidate_flat_view";
        return projected_depth>0?"world_authored_depth":"world_candidate_flat_fallback";
    }
    if(!impact(item))return "flat_2d";
    if(qvr_flat_view)return "impact_candidate_flat_view";
    return projected_depth>0?"impact_projected":"impact_candidate_flat_fallback";
}
bool qvr_sprite_report_emit(FILE *out,uint64_t simulation_tick,bool ready){
    if(!sprite_report_armed||sprite_report_done||!ready||!out)return false;
    sprite_report_done=true;sprite_report_armed=false;
    if(!report_grid_ready)build_grid();
    const bool fade_active=scene_mixer_snapshot.valid&&(scene_mixer_snapshot.mixer_flags&1u)&&scene_mixer_snapshot.screen_fade_factor;
    fprintf(out,"[vr-sprite] snapshot scene_frame=%u simulation_tick=%llu active_count=%d reported_count=%d truncated=%d triangles=%u camera=(%.3f,%.3f,%.3f) flat_view=%d mixer_valid=%d bg=%02X%02X%02X fog_rgb=%02X%02X%02X poly_fade_enabled=%d poly_fade_rgb=%02X%02X%02X screen_fade_active=%d screen_fade_rgb=%02X%02X%02X screen_fade_factor=0x%02X mixer_flags=0x%02X\n",
        frame_number,(unsigned long long)simulation_tick,sprite_active_count,sprite_count,sprite_active_count>sprite_count,count,flat_camera.focal,flat_camera.cx,flat_camera.cy,qvr_flat_view,
        scene_mixer_snapshot.valid,scene_mixer_snapshot.background[0],scene_mixer_snapshot.background[1],scene_mixer_snapshot.background[2],
        scene_mixer_snapshot.fog_rgb[0],scene_mixer_snapshot.fog_rgb[1],scene_mixer_snapshot.fog_rgb[2],scene_mixer_snapshot.poly_fade_enabled,
        scene_mixer_snapshot.poly_fade[0],scene_mixer_snapshot.poly_fade[1],scene_mixer_snapshot.poly_fade[2],fade_active,
        scene_mixer_snapshot.screen_fade[0],scene_mixer_snapshot.screen_fade[1],scene_mixer_snapshot.screen_fade[2],
        scene_mixer_snapshot.screen_fade_factor,scene_mixer_snapshot.mixer_flags);
    for(int i=0;i<sprite_count;i++){
        const sprite_item *item=&sprites[i].item;uint32_t world_sort=0;int world_order=0;float world_depth=0;
        bool has_world=surface_hit(item->x0+item->w*.5f,item->y0+item->h*.5f,&world_depth,&world_sort,&world_order);
        fprintf(out,"[vr-sprite] item=%d idx=%d tile=%d z=0x%06X layer=%u sort_depth=%u bounds=(%d,%d,%d,%d) prioverchar=%d classification=%s impact_depth_m=%.4f ",
            i,item->idx,item->tile,(unsigned)item->z&0xffffffu,(unsigned)item->z>>21,(unsigned)item->z&0x1fffffu,item->x0,item->y0,item->w,item->h,item->prioverchar,
            sprite_classification(item,sprites[i].depth),impact(item)?sprites[i].depth:0);
        fprintf(out,"projected_depth_m=%.4f ",sprites[i].depth);
        if(has_world)fprintf(out,"nearest_world_depth_m=%.4f world_zsort=0x%06X world_quad_order=%d\n",world_depth,world_sort,world_order);
        else fprintf(out,"nearest_world_depth_m=none world_zsort=none world_quad_order=none\n");
    }
    return true;
}
#endif
bool qvr_sprite_corners(const sprite_item *it,int slot,float out[4][4]){
    if(qvr_flat_view||slot<0||slot>=sprite_count||sprites[slot].depth<=0)return false;
    const sprite_item *saved=&sprites[slot].item;
    if(saved->tile!=it->tile||saved->idx!=it->idx||saved->x0!=it->x0||saved->y0!=it->y0||saved->w!=it->w||saved->h!=it->h||saved->z!=it->z)return false;
    const int corners[4][2]={{0,0},{1,0},{1,1},{0,1}};
    for(int j=0;j<4;j++){
        V3 p=unproject(flat_camera,(float)(it->x0+corners[j][0]*it->w),(float)(it->y0+corners[j][1]*it->h),sprites[slot].depth);
        out[j][0]=p.x;out[j][1]=p.y;out[j][2]=p.z;out[j][3]=1;
    }
    return true;
}
bool qvr_aim(V3 o,V3 d,float *nx,float *ny,V3 *hit){
    float nearest=FLT_MAX;int best=-1;
    if(!qvr_flat_view)for(unsigned i=0;i<count;i++)if(ray_triangle(o,d,triangles[i].a,triangles[i].b,triangles[i].c,&nearest))best=(int)i;
    Camera camera;
    if(best>=0){camera=triangles[best].camera;*hit=add(o,mul(d,nearest));if(hit->z>=-.001f)return false;}
    else{
        if(d.z>=-.001f)return false;
        float t=(-2.5f-o.z)/d.z;if(t<0)return false;
        *hit=add(o,mul(d,t));camera=qvr_flat_view?(Camera){500,320,240}:flat_camera;
    }
    *nx=(camera.cx+camera.focal*hit->x/-hit->z)/640;
    *ny=(camera.cy-camera.focal*hit->y/-hit->z)/480;
    return *nx>0&&*nx<1&&*ny>0&&*ny<1;
}
