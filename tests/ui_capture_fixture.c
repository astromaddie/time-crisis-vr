/* Exercise the real CPU-backed HUD PPM writer without a GL context. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../quest/quest_ui.c"

int main(int argc,char **argv){
    assert(argc==2);
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
    fclose(file);return 0;
}
