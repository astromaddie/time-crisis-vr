#include <assert.h>
#include <stdbool.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

int SDL_main(int argc, char **argv);

static bool game_started;

int access(const char *path, int mode)
{
    (void)mode;
    if (!strcmp(path, "/sdcard/Documents/TimeCrisisVR")) return 0;
    errno = ENOENT;
    return -1;
}

int chdir(const char *path)
{
    assert(!strcmp(path, "/sdcard/Documents/TimeCrisisVR"));
    return 0;
}

int SDL_ShowSimpleMessageBox(unsigned flags, const char *title, const char *message, void *window)
{
    (void)flags; (void)title; (void)message; (void)window;
    assert(!"unexpected startup error dialog");
    return 0;
}

int __android_log_print(int priority, const char *tag, const char *format, ...)
{
    (void)priority; (void)tag; (void)format;
    return 0;
}

int tc_game_main(int argc, char **argv)
{
    assert(argc == 5);
    assert(!strcmp(argv[0], "TimeCrisisVR"));
    assert(!strcmp(argv[1], "roms"));
    assert(!strcmp(argv[2], "--window"));
    assert(!strcmp(argv[3], "--record"));
    assert(!strcmp(argv[4], "last-session.inputs"));
    game_started = true;
    return 0;
}

int main(void)
{
    assert(SDL_main(0, NULL) == 0);
    assert(game_started);
    puts("PASS: normal Frame native startup reaches the arcade game with solo recording enabled");
    return 0;
}
