#ifndef YT_DOOR_H
#define YT_DOOR_H

#include "OpenDoor.h"
#include "yt_game.h"
#include "yt_patch.h"

struct yt_identity {
	char system[256];
	char sysop_first[128];
	char sysop_last[128];
	char real_first[128];
	char real_last[128];
	char location[256];
	bool ansi;
	bool local;
};

struct yt_door {
	struct yt_identity identity;
	struct yt_game game;
	const struct yt_patch_profile *patch;
	char rmt_handoff_path[1024];
	bool game_open;
	bool open_doors_initialized;
};

#ifndef ODPLAT_WIN32
bool yt_door_start(struct yt_door *door, int argc, char **argv,
    struct yt_error *error);
#else
bool yt_door_start(struct yt_door *door, char *command_line,
    struct yt_error *error);
#endif
void yt_door_cleanup(void);
void yt_door_shutdown_for_replace(void);
void yt_door_finish(int errorlevel);
struct yt_door *yt_door_current(void);

#endif
