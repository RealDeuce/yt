#ifndef YT_SESSION_H
#define YT_SESSION_H

#include "yt_door.h"

/*
 * Runs one complete player session.  It returns normally for every
 * application-level exit so that main can give OpenDoors the one and only
 * orderly shutdown call.
 */
bool yt_session_run(struct yt_door *door, const char *executable_path,
    struct yt_error *error);

#endif
