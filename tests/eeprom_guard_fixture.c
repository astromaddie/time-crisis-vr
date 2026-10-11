#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ss22_board.h"

ss22_sys_t g_ss22;
static const ss22_board_cfg board={.tag="TC"};
static void read_image(uint8_t image[SS22_EEPROM_SIZE]){
    FILE *file=fopen("game_eeprom.nv","rb");assert(file);assert(fread(image,1,SS22_EEPROM_SIZE,file)==SS22_EEPROM_SIZE);assert(fclose(file)==0);
}
int main(void){
    ss22_board_use(&board);
    uint8_t local[SS22_EEPROM_SIZE],sync[SS22_EEPROM_SIZE],disk[SS22_EEPROM_SIZE];
    for(size_t i=0;i<sizeof local;i++){local[i]=(uint8_t)(i*13u+7u);sync[i]=(uint8_t)(i*29u+3u);}
    memcpy(g_ss22.eeprom,local,sizeof local);
    ss22_eeprom_set_writable(true);ss22_eeprom_persist("game_eeprom.nv");
    assert(!memcmp(g_ss22.eeprom,local,sizeof local));

    /* A synchronized Join receives host EEPROM but may not write it over its Solo file. */
    memcpy(g_ss22.eeprom,sync,sizeof sync);ss22_eeprom_set_writable(false);g_ss22.eeprom[17]^=0xff;ss22_eeprom_save();
    read_image(disk);assert(!memcmp(disk,local,sizeof local));

    /* A later normal Solo boot resets the guard and writes its own persisted settings. */
    ss22_eeprom_set_writable(true);ss22_eeprom_persist("game_eeprom.nv");assert(!memcmp(g_ss22.eeprom,local,sizeof local));
    g_ss22.eeprom[17]^=0xff;ss22_eeprom_save();read_image(disk);assert(!memcmp(disk,g_ss22.eeprom,sizeof disk));
    puts("PASS: synchronized client EEPROM writes stay disabled and a later Solo boot restores normal EEPROM persistence.");return 0;
}
