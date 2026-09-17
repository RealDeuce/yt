#include "yt_player_cache.h"

bool
yt_player_cache_contains(int player_record)
{
	return player_record >= 0
	    && (size_t)player_record < YT_PLAYER_CACHE_RECORDS;
}

int
yt_player_cache_sector(const struct yt_player_cache *cache, int player_record)
{
	if (cache == NULL || !yt_player_cache_contains(player_record))
		return 0;
	return cache->sector[player_record];
}

bool
yt_player_cache_set_sector(struct yt_player_cache *cache, int player_record,
    int sector)
{
	if (cache == NULL || !yt_player_cache_contains(player_record))
		return false;
	cache->sector[player_record] = sector;
	return true;
}

float
yt_player_cache_cloak(const struct yt_player_cache *cache, int player_record)
{
	if (cache == NULL || !yt_player_cache_contains(player_record))
		return 0.0f;
	return cache->cloak[player_record];
}

bool
yt_player_cache_set_cloak(struct yt_player_cache *cache, int player_record,
    float cloak)
{
	if (cache == NULL || !yt_player_cache_contains(player_record))
		return false;
	cache->cloak[player_record] = cloak;
	return true;
}
