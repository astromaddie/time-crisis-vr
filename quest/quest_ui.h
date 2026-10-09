#pragma once
#include "vr_math.h"
bool qui_init(void);
void qui_draw(const float view[16],const float projection[16],V3 head,Q4 rotation,bool laser,bool physical_crouch,bool left_handed,bool frame_profile_active,int gun_pitch,bool saved);
bool qui_capture_ppm(const char *path);
void qui_shutdown(void);
