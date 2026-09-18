#ifndef YT_PATCH_CLI_H
#define YT_PATCH_CLI_H

#include "OpenDoor.h"
#include "yt_patch.h"

struct yt_patch_selection {
	const struct yt_patch_profile *profile;
	bool seen;
	bool valid;
	char rejected[80];
};

#ifdef ODPLAT_WIN32
bool yt_patch_parse_command_line(char *command_line,
    struct yt_patch_selection *selection, struct yt_error *error);
#else
bool yt_patch_parse_command_line(int argc, char **argv,
    struct yt_patch_selection *selection, struct yt_error *error);
#endif

#endif
