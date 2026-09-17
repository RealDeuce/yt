#ifndef YT_PLAYER_CACHE_H
#define YT_PLAYER_CACHE_H

#include "qb.h"

#define YT_PLAYER_FIRST_RECORD 2
#define YT_PLAYER_LAST_RECORD 51
#define YT_PLAYER_CACHE_RECORDS (YT_PLAYER_LAST_RECORD + 1U)

struct yt_player_cache {
	int sector[YT_PLAYER_CACHE_RECORDS];
	float cloak[YT_PLAYER_CACHE_RECORDS];
};

bool yt_player_cache_contains(int player_record);
int yt_player_cache_sector(const struct yt_player_cache *cache,
	int player_record);
bool yt_player_cache_set_sector(struct yt_player_cache *cache,
	int player_record, int sector);
float yt_player_cache_cloak(const struct yt_player_cache *cache,
	int player_record);
bool yt_player_cache_set_cloak(struct yt_player_cache *cache,
	int player_record, float cloak);

#endif
