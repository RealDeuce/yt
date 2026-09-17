#ifndef YT_PLAYER_CACHE_H
#define YT_PLAYER_CACHE_H

#include "qb.h"

#define YT_PLAYER_FIRST_RECORD 2
#define YT_PLAYER_LAST_RECORD 51
#define YT_PLAYER_CACHE_RECORDS (YT_PLAYER_LAST_RECORD + 1U)

enum yt_player_cache_kind {
	YT_PLAYER_CACHE_SECTOR,
	YT_PLAYER_CACHE_CLOAK,
};

struct yt_player_cache {
	float sector[YT_PLAYER_CACHE_RECORDS];
	float cloak[YT_PLAYER_CACHE_RECORDS];
};

bool yt_player_cache_contains(int player_record);
float yt_player_cache_value(const struct yt_player_cache *cache,
	int player_record, enum yt_player_cache_kind kind);
bool yt_player_cache_set(struct yt_player_cache *cache,
	int player_record, enum yt_player_cache_kind kind, float value);

#endif
