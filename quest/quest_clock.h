#pragma once
#include <stdint.h>
#include <stdbool.h>

/* The arcade CPU needs ~59.906 ticks/sec even if a PC compositor reprojection
 * mode renders at 45 Hz. Additional simulation ticks reuse the latest input
 * sample and do not submit another XR frame. A long interruption resets time. */
static inline bool qclock_due(int64_t *deadline,int64_t display_time){
    if(!*deadline||display_time-*deadline>250000000||*deadline-display_time>250000000)
        *deadline=display_time;
    return display_time>=*deadline;
}
static inline void qclock_advance(int64_t *deadline){*deadline+=16692818;}
/* Co-op keeps a due simulation deadline until its ordered network tick exists. */
static inline bool qclock_advance_if_ready(int64_t *deadline,bool canonical_tick_ready){
    if(!canonical_tick_ready)return false;
    qclock_advance(deadline);return true;
}
static inline bool qclock_take(int64_t *deadline,int64_t display_time){
    if(!qclock_due(deadline,display_time))return false;
    qclock_advance(deadline);return true;
}
