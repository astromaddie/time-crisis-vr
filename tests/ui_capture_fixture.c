/* Exercise the real CPU-backed HUD PPM writer without a GL context. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../quest/quest_ui.c"

int main(int argc,char **argv){
    assert(argc==2||argc==3);
    memset(pixels,0,sizeof pixels);
    pixels[0]=11;pixels[1]=22;pixels[2]=33;
    size_t bottom=(size_t)(HEIGHT-1)*WIDTH*4;
    size_t bottom_right=bottom+(WIDTH-1)*4;
    pixels[bottom_right]=44;pixels[bottom_right+1]=55;pixels[bottom_right+2]=66;
    assert(qui_capture_ppm(argv[1]));
    FILE *file=fopen(argv[1],"rb");assert(file);
    char line[32];assert(fgets(line,sizeof line,file)&&!strcmp(line,"P6\n"));
    assert(fgets(line,sizeof line,file)&&!strcmp(line,"640 480\n"));
    assert(fgets(line,sizeof line,file)&&!strcmp(line,"255\n"));
    unsigned char pixel[3];assert(fread(pixel,1,3,file)==3);
    assert(pixel[0]==11&&pixel[1]==22&&pixel[2]==33); /* Top row is first in PPM. */
    assert(fseek(file,-3,SEEK_END)==0&&fread(pixel,1,3,file)==3);
    assert(pixel[0]==44&&pixel[1]==55&&pixel[2]==66); /* Bottom row remains last. */
    fclose(file);
    if(argc==3){
        const char *lower="Shared game stopped. Close both games and start a new session.";
        const char *upper="SHARED GAME STOPPED. CLOSE BOTH GAMES AND START A NEW SESSION.";
        panel(true,true,false,true,0,true,lower,true);
        uint64_t lower_hash=1469598103934665603ULL;
        for(size_t i=0;i<sizeof pixels;i++){lower_hash^=pixels[i];lower_hash*=1099511628211ULL;}
        panel(true,true,false,true,0,true,upper,true);
        uint64_t upper_hash=1469598103934665603ULL;
        for(size_t i=0;i<sizeof pixels;i++){upper_hash^=pixels[i];upper_hash*=1099511628211ULL;}
        assert(lower_hash==upper_hash); /* lowercase backend messages remain readable in the uppercase-only arcade font */
        panel(true,true,false,true,0,true,lower,true);
        bool legacy_help=false,notice_text=false;
        for(int y=344;y<434;y++)for(int x=24;x<620;x++){
            const uint8_t *p=pixels+(y*WIDTH+x)*4;
            if(p[2]>60&&p[0]<p[1]&&p[1]<p[2])legacy_help=true;
            if(p[2]>60&&p[0]>p[1]&&p[1]>=p[2])notice_text=true;
        }
        assert(!legacy_help); /* LAN notice owns the lower help rows; old controls must not show through. */
        assert(notice_text);
        assert(qui_capture_ppm(argv[2]));
    }
    return 0;
}
