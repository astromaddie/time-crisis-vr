#ifndef TCVR_FRAME_REPLAY_H
#define TCVR_FRAME_REPLAY_H

#include <stdbool.h>

#define TCVR_FRAME_REPLAY_MARKER "vrdiag-replay.once"
#define TCVR_FRAME_REPLAY_INPUT "vrdiag-replay.inputs"

/* Returns 0 for normal recording, 1 for validated diagnostic replay, -1 on error. */
int tcvr_frame_select_launch(char **argv, int *argc);

/* Replay EEPROM comes from the recording and must remain memory-only. */
static inline bool tcvr_frame_replay_eeprom_persist_allowed(bool replay)
{
    return !replay;
}

#endif
