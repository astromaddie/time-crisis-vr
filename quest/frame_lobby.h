#pragma once
#include <stdbool.h>
#define QLOBBY_MAX_BUTTONS 16
typedef struct { int x,y,w,h; char label[40]; bool enabled; } QLobbyButton;
typedef struct {
    char title[40],status[3][40];
    QLobbyButton buttons[QLOBBY_MAX_BUTTONS];
    int count,selected;
} QLobbyView;
void qlobby_begin(void);
void qlobby_poll(void);
const QLobbyView *qlobby_view(void);
void qlobby_move(int x,int y);
bool qlobby_hover(int x,int y);
void qlobby_select(void);
void qlobby_back(void);
bool qlobby_done(void);
void qlobby_close(void);
bool qframe_lobby_run(void);
