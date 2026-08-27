#ifndef YT_INIT_H
#define YT_INIT_H

#include "yt_game.h"

enum yt_initializer_family {
	YT_INITIALIZER_YT,
	YT_INITIALIZER_RMT
};

struct yt_initializer_options {
	enum yt_initializer_family family;
	const char *scoreboard;
	struct yt_config config;
	bool use_existing_config;
	bool database_already_truncated;
	const char *credited_name;
};

bool yt_generate_port_name(struct yt_random *random, char name[42],
    struct yt_error *error);
bool yt_initialize_begin_yt(struct yt_error *error);
bool yt_initialize_world(const struct yt_initializer_options *options,
    struct yt_random *random, struct yt_error *error);
bool yt_initialize_yt(const char *scoreboard, struct yt_random *random,
    struct yt_error *error);
bool yt_initialize_rmt(const struct yt_config *config,
    const char *credited_name, struct yt_random *random,
    struct yt_error *error);

#endif
