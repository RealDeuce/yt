#ifndef YT_RMT_DOOR_H
#define YT_RMT_DOOR_H

#include "yt_init.h"
#include "yt_platform.h"

struct yt_rmt_door {
	struct yt_platform_rmt_serial serial;
	bool initialized;
};

bool yt_rmt_door_prepare(struct yt_rmt_door *door, int port,
    const struct yt_startup_framing *framing, struct yt_error *error);
bool yt_rmt_door_start(struct yt_rmt_door *door, int port,
    struct yt_error *error);
bool yt_rmt_door_write(struct yt_rmt_door *door,
    const uint8_t *data, size_t length);
bool yt_rmt_door_local_write(struct yt_rmt_door *door,
    const uint8_t *data, size_t length);
void yt_rmt_door_finish(struct yt_rmt_door *door, int errorlevel);

#endif
