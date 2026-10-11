#include "quest_scene.h"
#include "quest_clock.h"
#include "quest_refresh.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void near(float a,float b){assert(fabsf(a-b)<.0001f);}
#ifdef TCVR_STEAM_FRAME
static size_t read_text(FILE *stream,char *out,size_t capacity){
    fflush(stream);rewind(stream);size_t n=fread(out,1,capacity-1,stream);out[n]=0;return n;
}
#endif
#ifdef TCVR_STEAM_FRAME
static void scaled_camera_tests(void){
    qvr_display_config(NULL);near(qvr_gameplay_scale,1.6f);near(qvr_outside_darkening,.65f);
    const char *configs[]={"eye_resolution=2160\nmultiview=0\ngameplay_scale=nan\noutside_darkening=inf\n", "gameplay_scale=-3\noutside_darkening=-1\n", "gameplay_scale=10\noutside_darkening=2\n", "gameplay_scale=oops\noutside_darkening=oops\n", "gameplay_scale=1.35\noutside_darkening=0.3\n"};
    const float expected[5][2]={{1.6f,.65f},{1,0},{2,1},{1.6f,.65f},{1.35f,.3f}};
    for(int i=0;i<5;i++){FILE *f=tmpfile();assert(f);fputs(configs[i],f);rewind(f);qvr_display_config(f);near(qvr_gameplay_scale,expected[i][0]);near(qvr_outside_darkening,expected[i][1]);rewind(f);char text[256];assert(fread(text,1,strlen(configs[i]),f)==strlen(configs[i]));assert(!memcmp(text,configs[i],strlen(configs[i])));fclose(f);}
    const float scales[]={1,1.35f,1.6f};
    for(int k=0;k<3;k++){
        qvr_gameplay_scale=scales[k];geo_quad q={0};q.vr_focal=772.6f;q.vr_cx=312;q.vr_cy=236;q.nrv=4;
        for(int i=0;i<4;i++){q.rv[i].sx16=(i==1||i==2?500:140)*16;q.rv[i].sy16=(i>=2?370:110)*16;q.rv[i].z=30000;}
        qvr_scene_begin();qvr_scene_quad(&q);
        for(int i=0;i<4;i++)q.rv[i].z=20000;qvr_scene_quad(&q);
        sprite_item items[2]={{.x0=350,.y0=180,.w=33,.h=33,.tile=QVR_IMPACT_FIRST},{.x0=196,.y0=279,.w=61,.h=71,.tile=10131,.z=0x228d13}};
        qvr_sprites_prepare(items,2);float cx,cy,f,x,y,v[4],corners[4][4];V3 hit;
        assert(qvr_flat_camera(&cx,&cy,&f));near(cx,312);near(cy,236);near(f,772.6f/scales[k]);
        qvr_vertex(&q,430,180,20000,v);near(v[0],(430-cx)*20/f);near(v[1],(cy-180)*20/f);near(v[2],-20);
        assert(qvr_aim(v3(0,0,0),v3((430-cx)/f,(cy-180)/f,-1),&x,&y,&hit));near(x,430/640.f);near(y,180/480.f);near(hit.z,-20);
        for(int j=0;j<2;j++){assert(qvr_sprite_corners(&items[j],j,corners));near(corners[0][2],j?-167.187f:-19.6f);near(cx+f*corners[0][0]/-corners[0][2],items[j].x0);near(cy-f*corners[0][1]/-corners[0][2],items[j].y0);}
        // Nonzero controller origin: use known world intersections, independently
        // project those points back with the authored focal and requested scale.
        V3 origin=v3(.25f,-.12f,.3f),direction=v3(.04f,.01f,-1);
        assert(qvr_aim(origin,direction,&x,&y,&hit));
        float t=20.3f;near(hit.z,-20);
        near(x,(312+772.6f*(.25f+.04f*t)/(20*scales[k]))/640);
        near(y,(236-772.6f*(-.12f+.01f*t)/(20*scales[k]))/480);
        // Misses and strict native edges use the same camera as HUD and guide.
        qvr_scene_begin();qvr_sprites_prepare(NULL,0);
        assert(qvr_aim(origin,direction,&x,&y,&hit));
        t=2.8f;near(hit.z,-2.5f);
        near(x,(312+772.6f*(.25f+.04f*t)/(2.5f*scales[k]))/640);
        near(y,(236-772.6f*(-.12f+.01f*t)/(2.5f*scales[k]))/480);
        for(int edge=0;edge<4;edge++)for(int side=-1;side<=1;side++){
            float px=edge==0?side*.1f:edge==1?640-side*.1f:320;
            float py=edge==2?side*.1f:edge==3?480-side*.1f:240;
            bool inside=qvr_aim(v3(0,0,0),v3((px-cx)/f,(cy-py)/f,-1),&x,&y,&hit);
            if(side)assert(inside==(side>0));near(x,px/640);near(y,py/480);near(hit.z,-2.5f);
        }
        qvr_flat_view=1;qvr_flat_camera(&cx,&cy,&f);near(f,500);qvr_vertex(&q,430,180,20000,v);near(v[0],430);near(v[3],0);qvr_flat_view=0;
    }
    qvr_gameplay_scale=1;
}
#endif
int main(void){
#ifdef TCVR_STEAM_FRAME
    // Run scaled tests after baseline assertions to avoid retained-camera coupling.
    qvr_gameplay_scale=1;
#endif
    const float full_rates[]={90,72,120,80},limited_rates[]={72,90,80},compat_rates[]={60,72};
    near(qrefresh_choose(full_rates,4,120),120);near(qrefresh_choose(limited_rates,3,120),90);
    near(qrefresh_choose(full_rates,4,90),90);near(qrefresh_choose(compat_rates,2,120),72);
    near(qrefresh_choose(full_rates,4,NAN),120);near(qrefresh_choose(NULL,0,120),0);
    float cx,cy,focal;assert(qvr_flat_camera(&cx,&cy,&focal));near(focal,772.5625f);
    // Display rates, including compositor half-rate, must preserve arcade time.
    const int rates[]={30,45,60,72,90,120};
    for(unsigned k=0;k<sizeof rates/sizeof rates[0];k++){
        int64_t deadline=0;unsigned ticks=0;
        for(int f=0;f<rates[k]*10;f++){
            int64_t now=1000000000LL+(int64_t)f*1000000000/rates[k];
            while(qclock_take(&deadline,now))ticks++;
        }
        assert(ticks>=598&&ticks<=600);
        assert(qclock_take(&deadline,30000000000LL));
        assert(!qclock_take(&deadline,30000000000LL)); // resume has no large backlog
    }
    float p[16],v[16];projection(p,-.7f,.7f,-.7f,.7f);near(p[8],0);near(p[9],0);
    /* Distant arcade scenery must survive clip-space Z, including >2 km. */
    for(float z=1;z<100000;z*=10){float clip_z=p[10]*-z+p[14],clip_w=z;assert(clip_z>=-clip_w&&clip_z<=clip_w);}
    view_matrix(v,v3(.032f,1,2),(Q4){0,0,0,1});near(v[12],-.032f);near(v[13],-1);near(v[14],-2);
    Q4 turn={0,sinf(.5f),0,cosf(.5f)};V3 back=rotate(conjugate(turn),rotate(turn,v3(1,2,-3)));near(back.x,1);near(back.z,-3);
    float distance=100;assert(ray_triangle(v3(0,0,0),v3(0,0,-1),v3(-1,-1,-3),v3(1,-1,-3),v3(0,1,-3),&distance));near(distance,3);
    distance=100;assert(!ray_triangle(v3(5,0,0),v3(0,0,-1),v3(-1,-1,-3),v3(1,-1,-3),v3(0,1,-3),&distance));
    distance=100;assert(!ray_triangle(v3(0,0,0),v3(0,0,1),v3(-1,-1,-3),v3(1,-1,-3),v3(0,1,-3),&distance));
    geo_quad q={0};q.vr_focal=320;q.vr_cx=320;q.vr_cy=240;q.nrv=4;
    int xy[4][2]={{160,120},{480,120},{480,360},{160,360}};
    for(int i=0;i<4;i++){q.rv[i].sx16=xy[i][0]*16;q.rv[i].sy16=xy[i][1]*16;q.rv[i].z=3000;}
    float vertex[4];qvr_vertex(&q,320,240,3000,vertex);near(vertex[0],0);near(vertex[1],0);near(vertex[2],-3);near(vertex[3],1);
    qvr_scene_begin();qvr_scene_quad(&q);float x,y;V3 hit;
    assert(qvr_aim(v3(0,0,0),v3(0,0,-1),&x,&y,&hit));near(x,.5f);near(y,.5f);near(hit.z,-3);
    assert(qvr_aim(v3(.3f,0,0),v3(0,0,-1),&x,&y,&hit));near(x,.55f);
    qvr_flat_view=1;qvr_vertex(&q,160,120,3000,vertex);near(vertex[0],160);near(vertex[1],120);near(vertex[3],0);qvr_flat_view=0;
    qvr_scene_begin();for(int i=0;i<4;i++)q.rv[i].z=5000000;qvr_scene_quad(&q);
    assert(qvr_aim(v3(0,0,0),v3(0,0,-1),&x,&y,&hit));assert(fabsf(hit.z+5000)<.01f);
    // Nearest surface must win, irrespective of painter order.
    for(int i=0;i<4;i++)q.rv[i].z=1500;qvr_scene_quad(&q);
    assert(qvr_aim(v3(0,0,0),v3(0,0,-1),&x,&y,&hit));near(hit.z,-1.5f);
    q.direct=1;qvr_vertex(&q,10,20,3000,vertex);near(vertex[0],10);near(vertex[1],20);near(vertex[3],0);
    qvr_scene_begin();assert(qvr_aim(v3(0,0,0),v3(0,0,-1),&x,&y,&hit));near(x,.5f);near(y,.5f);near(hit.z,-2.5f);
    assert(!qvr_aim(v3(0,0,0),v3(0,0,1),&x,&y,&hit));
    // IPD yields opposed horizontal disparity and no vertical disparity.
    float left[16],right[16];view_matrix(left,v3(-.032f,0,0),(Q4){0,0,0,1});view_matrix(right,v3(.032f,0,0),(Q4){0,0,0,1});
    assert(left[12]>0&&right[12]<0);near(left[13],right[13]);
    /* Match flat pixels, world pixels and aim at centre AND off-centre. */
    q.direct=0;q.vr_focal=772.6f;q.vr_cx=312;q.vr_cy=236;
    for(int i=0;i<4;i++)q.rv[i].z=30000;
    qvr_scene_begin();qvr_scene_quad(&q);qvr_sprites_prepare(NULL,0);
    assert(qvr_flat_camera(&cx,&cy,&focal));near(cx,312);near(cy,236);near(focal,772.6f);
    for(int px=170;px<=470;px+=30){
        V3 ray=v3((px-cx)/focal,(cy-190)/focal,-1);
        assert(qvr_aim(v3(0,0,0),ray,&x,&y,&hit));near(x,px/640.f);near(y,190/480.f);near(hit.z,-30);
    }
    sprite_item mark={.x0=350,.y0=180,.w=33,.h=33,.tile=QVR_IMPACT_FIRST};
    float corners[4][4];qvr_sprites_prepare(&mark,1);assert(qvr_sprite_corners(&mark,0,corners));near(corners[0][2],-29.4f);
    near(cx+focal*corners[0][0]/-corners[0][2],350);near(cy-focal*corners[0][1]/-corners[0][2],180);
    near(cx+focal*corners[2][0]/-corners[2][2],383);near(cy-focal*corners[2][1]/-corners[2][2],213);
    for(int tile=QVR_IMPACT_FIRST;tile<=QVR_IMPACT_LAST;tile++){mark.tile=tile;qvr_sprites_prepare(&mark,1);assert(qvr_sprite_corners(&mark,0,corners));}
    mark.tile=100;qvr_sprites_prepare(&mark,1);assert(!qvr_sprite_corners(&mark,0,corners));
    mark.tile=QVR_IMPACT_FIRST;mark.z=1;qvr_sprites_prepare(&mark,1);assert(!qvr_sprite_corners(&mark,0,corners));mark.z=0;
    /* Nearest surface wins even if added before distant geometry. */
    qvr_scene_begin();for(int i=0;i<4;i++)q.rv[i].z=20000;qvr_scene_quad(&q);
    for(int i=0;i<4;i++)q.rv[i].z=30000;qvr_scene_quad(&q);qvr_sprites_prepare(&mark,1);
    assert(qvr_sprite_corners(&mark,0,corners));near(corners[0][2],-19.6f);
    /* Clear a scene without losing its camera; misses remain on the HUD plane. */
    qvr_scene_begin();qvr_sprites_prepare(&mark,1);assert(!qvr_sprite_corners(&mark,0,corners));
    assert(qvr_flat_camera(&cx,&cy,&focal));near(focal,772.6f);
    V3 ray=v3((430-cx)/focal,(cy-180)/focal,-1);
    assert(qvr_aim(v3(0,0,0),ray,&x,&y,&hit));near(x,430/640.f);near(y,180/480.f);near(hit.z,-2.5f);
    /* The full-frame guide marks the native viewport, including with an
     * off-centre live camera. Probe immediately inside/outside all four edges. */
    for(int edge=0;edge<4;edge++)for(int inside=0;inside<2;inside++){
        float delta=inside?.1f:-.1f;
        float px=edge==0?delta:edge==1?640-delta:320;
        float py=edge==2?delta:edge==3?480-delta:240;
        V3 edge_ray=v3((px-cx)/focal,(cy-py)/focal,-1);
        assert(qvr_aim(v3(0,0,0),edge_ray,&x,&y,&hit)==(bool)inside);
        near(x,px/640);near(y,py/480);near(hit.z,-2.5f);
    }
    /* A controller offset must still project its actual hit, not its direction. */
    assert(qvr_aim(v3(.25f,-.12f,0),v3(0,0,-1),&x,&y,&hit));near(x,(312+772.6f*.25f/2.5f)/640);
    qvr_flat_view=1;assert(!qvr_flat_camera(&cx,&cy,&focal));near(focal,500);near(cx,320);
    assert(qvr_aim(v3(.25f,0,0),v3(0,0,-1),&x,&y,&hit));near(x,.5f+.25f/3.2f);
    qvr_scene_begin();qvr_scene_quad(&q);qvr_sprites_prepare(&mark,1);assert(!qvr_sprite_corners(&mark,0,corners));qvr_flat_view=0;
    /* Full-screen triangles exhaust the grid's fixed budget: full scan must win. */
    for(int i=0;i<4;i++){q.rv[i].sx16=(i==1||i==2?640:0)*16;q.rv[i].sy16=(i>=2?480:0)*16;}
    qvr_scene_begin();for(int n=0;n<230;n++)qvr_scene_quad(&q);qvr_sprites_prepare(&mark,1);
    assert(qvr_sprite_corners(&mark,0,corners));near(corners[0][2],-29.4f);
    /* The game's world-sprite routines author layer 1 + camera Z. Test the
     * production prepare/corners path in both build modes, with scenery at a
     * different depth so a surface-snapping implementation cannot pass. */
    sprite_item world={.x0=196,.y0=279,.w=61,.h=71,.idx=8,.tile=10131,.z=0x228d13};
    sprite_item original_world=world;
    qvr_scene_begin();qvr_scene_quad(&q);qvr_sprites_prepare(&world,1);
#ifdef TCVR_STEAM_FRAME
    assert(qvr_sprite_corners(&world,0,corners));near(corners[0][2],-167.187f);
    assert(memcmp(&world,&original_world,sizeof world)==0); /* painter key/bounds retained */
    assert(qvr_flat_camera(&cx,&cy,&focal));
    const int corner_xy[4][2]={{196,279},{257,279},{257,350},{196,350}};
    for(int i=0;i<4;i++){
        near(cx+focal*corners[i][0]/-corners[i][2],corner_xy[i][0]);
        near(cy-focal*corners[i][1]/-corners[i][2],corner_xy[i][1]);near(corners[i][3],1);
    }
    /* Its stereo disparity follows world depth, not the inherited 2.5m HUD. */
    float left_px=cx+focal*(corners[0][0]+.032f)/-corners[0][2];
    float right_px=cx+focal*(corners[0][0]-.032f)/-corners[0][2];
    near(left_px-right_px,focal*.064f/167.187f);
    assert(left_px-right_px<focal*.064f/2.5f);
    /* World placement must not replace the polygon-only aim surface. */
    assert(qvr_aim(v3(0,0,0),v3(0,0,-1),&x,&y,&hit));near(hit.z,-30);
    qvr_units_per_meter=2000;qvr_sprites_prepare(&world,1);
    assert(qvr_sprite_corners(&world,0,corners));near(corners[0][2],-83.5935f);
    const float bad_scales[]={0,-1,NAN,INFINITY};
    for(unsigned i=0;i<sizeof bad_scales/sizeof bad_scales[0];i++){
        qvr_units_per_meter=bad_scales[i];qvr_sprites_prepare(&world,1);
        assert(!qvr_sprite_corners(&world,0,corners));
    }
    qvr_units_per_meter=1000;
    /* Near HUD values, other priority bands and saturated keys must retain
     * the flat path, even when their artwork or screen rectangle matches. */
    const uint32_t flat_keys[]={0,1,0x10,0x200000,0x200010,0x3fffff,0x028d13,0x428d13,0xe28d13};
    for(unsigned i=0;i<sizeof flat_keys/sizeof flat_keys[0];i++){
        world.z=flat_keys[i];qvr_sprites_prepare(&world,1);
        assert(!qvr_sprite_corners(&world,0,corners));
    }
    world=original_world;qvr_sprites_prepare(&world,1);
    sprite_item changed=world;changed.z++;assert(!qvr_sprite_corners(&changed,0,corners));
    assert(!qvr_sprite_corners(&world,-1,corners));assert(!qvr_sprite_corners(&world,1,corners));
    qvr_flat_view=1;qvr_sprites_prepare(&world,1);assert(!qvr_sprite_corners(&world,0,corners));qvr_flat_view=0;
    /* A retained gameplay camera must not convert title/menu sprites when no
     * reconstructed world is present, including direct-screen polygon scenes. */
    qvr_scene_begin();qvr_sprites_prepare(&world,1);assert(!qvr_sprite_corners(&world,0,corners));
    q.direct=1;qvr_scene_quad(&q);qvr_sprites_prepare(&world,1);assert(!qvr_sprite_corners(&world,0,corners));q.direct=0;
#else
    assert(!qvr_sprite_corners(&world,0,corners));
    assert(memcmp(&world,&original_world,sizeof world)==0);
#endif
#ifdef TCVR_STEAM_FRAME
    /* Exercise the same cached sprite and reconstructed polygon data consumed
     * by ss22_draw, then gate its one-shot report on the paired dump tick. */
    geo_quad far=q,near_quad=q;far.zsort=0x005678;far.order=77;near_quad.zsort=0x001234;near_quad.order=88;
    for(int i=0;i<4;i++){far.rv[i].sx16=(i==1||i==2?640:0)*16;far.rv[i].sy16=(i>=2?480:0)*16;far.rv[i].z=30000;near_quad.rv[i]=far.rv[i];near_quad.rv[i].z=20000;}
    qvr_scene_begin();qvr_scene_quad(&far);qvr_scene_quad(&near_quad);
    sprite_item report_items[3]={
        {.x0=350,.y0=180,.w=33,.h=33,.idx=17,.prioverchar=1,.tile=QVR_IMPACT_FIRST,.z=0},
        {.x0=100,.y0=120,.w=40,.h=50,.idx=42,.prioverchar=0,.tile=100,.z=0x400123},
        world
    };
    qvr_sprites_prepare(report_items,3);unsigned report_frame=qvr_scene_frame_number();
    float report_corners[4][4];assert(qvr_sprite_corners(&report_items[0],0,report_corners));assert(!qvr_sprite_corners(&report_items[1],1,report_corners));
    FILE *report=tmpfile();assert(report);
    assert(!qvr_sprite_report_emit(report,600,true)); /* static/default state is off */
    qvr_sprite_report_arm(false);assert(!qvr_sprite_report_emit(report,600,true));
    qvr_sprite_report_arm(true);
    const uint8_t bg[3]={0x11,0x22,0x33},fog_rgb[3]={0x44,0x55,0x66},poly_fade[3]={0x70,0x80,0x90},screen_fade[3]={0xff,0xff,0x00};
    qvr_scene_report_mixer(true,bg,fog_rgb,true,poly_fade,screen_fade,0x80,0x01);
    assert(!qvr_sprite_report_emit(report,599,false));
    assert(qvr_sprite_report_emit(report,600,true));assert(!qvr_sprite_report_emit(report,601,true));
    char report_text[8192];assert(read_text(report,report_text,sizeof report_text)>0);fclose(report);
    char expected_header[160];snprintf(expected_header,sizeof expected_header,"[vr-sprite] snapshot scene_frame=%u simulation_tick=600 active_count=3 reported_count=3 truncated=0",report_frame);
    assert(strstr(report_text,expected_header));
    assert(strstr(report_text,"mixer_valid=1 bg=112233 fog_rgb=445566 poly_fade_enabled=1 poly_fade_rgb=708090 screen_fade_active=1 screen_fade_rgb=FFFF00 screen_fade_factor=0x80 mixer_flags=0x01"));
    assert(strstr(report_text,"item=0 idx=17 tile=9368 z=0x000000 layer=0 sort_depth=0 bounds=(350,180,33,33) prioverchar=1 classification=impact_projected impact_depth_m=19.6000"));
    assert(strstr(report_text,"nearest_world_depth_m=20.0000 world_zsort=0x001234 world_quad_order=88"));
    assert(strstr(report_text,"item=1 idx=42 tile=100 z=0x400123 layer=2 sort_depth=291 bounds=(100,120,40,50) prioverchar=0 classification=flat_2d"));
    assert(strstr(report_text,"classification=world_authored_depth impact_depth_m=0.0000 projected_depth_m=167.1870"));

    /* A stored yellow fade color is not active unless the mixer gate is on. */
    qvr_sprite_report_arm(true);qvr_scene_begin();qvr_scene_quad(&near_quad);
    qvr_scene_report_mixer(true,bg,fog_rgb,true,poly_fade,screen_fade,0xff,0x00);
    qvr_sprites_prepare(report_items,2);
    FILE *inactive=tmpfile();assert(inactive);assert(qvr_sprite_report_emit(inactive,601,true));
    char inactive_text[1024];assert(read_text(inactive,inactive_text,sizeof inactive_text)>0);fclose(inactive);
    assert(strstr(inactive_text,"mixer_valid=1 bg=112233 fog_rgb=445566 poly_fade_enabled=1 poly_fade_rgb=708090 screen_fade_active=0 screen_fade_rgb=FFFF00 screen_fade_factor=0xFF mixer_flags=0x00"));

    /* The report is bounded to the renderer's 1024-entry cache and says when
     * the upstream active count exceeded that cache. */
    sprite_item many[1026]={0};
    for(int i=0;i<1026;i++){many[i].idx=i;many[i].tile=100;many[i].x0=i%600;many[i].y0=10;many[i].w=1;many[i].h=1;many[i].z=1;}
    qvr_scene_begin();qvr_sprites_prepare(many,1026);qvr_sprite_report_arm(true);
    FILE *bounded=tmpfile();assert(bounded);assert(qvr_sprite_report_emit(bounded,601,true));
    assert(fflush(bounded)==0&&fseek(bounded,0,SEEK_END)==0);long bounded_size=ftell(bounded);
    assert(bounded_size>0&&bounded_size<1024*1024&&fseek(bounded,0,SEEK_SET)==0);
    char *bounded_text=calloc((size_t)bounded_size+1,1);assert(bounded_text);
    assert(fread(bounded_text,1,(size_t)bounded_size,bounded)==(size_t)bounded_size);fclose(bounded);
    assert(strstr(bounded_text,"active_count=1026 reported_count=1024 truncated=1"));
    assert(strstr(bounded_text,"mixer_valid=0 bg=000000 fog_rgb=000000 poly_fade_enabled=0 poly_fade_rgb=000000 screen_fade_active=0 screen_fade_rgb=000000 screen_fade_factor=0x00 mixer_flags=0x00"));
    assert(strstr(bounded_text,"item=1023 idx=1023"));assert(!strstr(bounded_text,"item=1024 idx=1024"));
    unsigned report_lines=0;for(char *cursor=bounded_text;(cursor=strstr(cursor,"[vr-sprite]"));cursor+=11)report_lines++;
    assert(report_lines==1025);free(bounded_text);
#endif
#ifdef TCVR_STEAM_FRAME
    scaled_camera_tests();
#endif
    puts("PASS: VR timing/math, refresh selection, HUD/aim, surface shot marks, Frame world-sprite depth and exclusions, bounded sprite/mixer report, nearest hit, grid overflow and unchanged desktop projection");return 0;
}
