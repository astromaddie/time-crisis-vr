#include "../quest/quest_clock.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static void test_waiting_preserves_due_deadline(void){
    int64_t deadline=0,display=1000000000;
    assert(qclock_due(&deadline,display));
    int64_t due=deadline;
    assert(!qclock_advance_if_ready(&deadline,false)&&deadline==due);
    assert(qclock_due(&deadline,display+16692818));
    assert(!qclock_advance_if_ready(&deadline,false)&&deadline==due);
    assert(qclock_due(&deadline,display+33385636));
    assert(qclock_advance_if_ready(&deadline,true)&&deadline==due+16692818);
}

static void test_solo_tick_cadence(void){
    const int64_t display_step=8333333;
    int64_t deadline=0,display=2000000000;
    unsigned ticks=0;
    for(unsigned frame=0;frame<=120;frame++,display+=display_step)
        if(qclock_take(&deadline,display))ticks++;
    /* 120 XR frames span one second; 16.692818 ms deadlines yield about 59.906 Hz. */
    assert(ticks==60);
}

int main(void){
    test_waiting_preserves_due_deadline();
    test_solo_tick_cadence();
    puts("PASS: waiting for a canonical LAN tick preserves the due deadline; solo cadence remains 59.906 Hz.");
    return 0;
}
