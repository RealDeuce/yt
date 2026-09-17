#include "yt_player_cache.h"

bool
yt_player_cache_contains(int player_record)
{
	return player_record >= 0
	    && (size_t)player_record < YT_PLAYER_CACHE_RECORDS;
}

float
yt_player_cache_value(const struct yt_player_cache *cache,
    int player_record, enum yt_player_cache_kind kind)
{
	if (cache == NULL || !yt_player_cache_contains(player_record))
		return 0.0f;
	return kind == YT_PLAYER_CACHE_SECTOR
	    ? cache->sector[player_record]
	    : cache->cloak[player_record];
}

bool
yt_player_cache_set(struct yt_player_cache *cache,
    int player_record, enum yt_player_cache_kind kind, float value)
{
	if (cache == NULL || !yt_player_cache_contains(player_record))
		return false;
	if (kind == YT_PLAYER_CACHE_SECTOR)
		cache->sector[player_record] = value;
	else
		cache->cloak[player_record] = value;
	return true;
}
