#include "frame_replay.h"

#ifdef TCVR_STEAM_FRAME
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "ss22_board.h"

static bool replay_header_valid(void)
{
    static const char header[] = "SS22REC 1 TC ";
    FILE *file = fopen(TCVR_FRAME_REPLAY_INPUT, "rb");
    if (!file) return false;
    errno = 0;

    bool valid = true;
    for (size_t i = 0; i < sizeof(header) - 1; ++i) {
        if (fgetc(file) != (unsigned char)header[i]) { valid = false; break; }
    }
    for (size_t i = 0; valid && i < SS22_EEPROM_SIZE * 2u; ++i) {
        int c = fgetc(file);
        if (c == EOF || !isxdigit((unsigned char)c)) valid = false;
    }
    if (valid) {
        int end = fgetc(file);
        if (end == '\r') end = fgetc(file);
        valid = end == '\n';
    }
    if (fclose(file) != 0) valid = false;
    if (!valid && errno == 0) errno = EINVAL;
    return valid;
}

int tcvr_frame_select_launch(char **argv, int *argc)
{
    if (!argv || !argc) { errno = EINVAL; return -1; }

    if (access(TCVR_FRAME_REPLAY_MARKER, F_OK) == 0) {
        if (!replay_header_valid()) return -1;
        if (unlink(TCVR_FRAME_REPLAY_MARKER) != 0) return -1;
        argv[0] = "TimeCrisisVR";
        argv[1] = "roms";
        argv[2] = "--window";
        argv[3] = "--replay";
        argv[4] = TCVR_FRAME_REPLAY_INPUT;
        argv[5] = NULL;
        *argc = 5;
        return 1;
    }
    if (errno != ENOENT) return -1;

    remove("previous-session.inputs");
    if (rename("last-session.inputs", "previous-session.inputs") != 0 && errno != ENOENT) return -1;
    argv[0] = "TimeCrisisVR";
    argv[1] = "roms";
    argv[2] = "--window";
    argv[3] = "--record";
    argv[4] = "last-session.inputs";
    argv[5] = NULL;
    *argc = 5;
    return 0;
}
#endif
