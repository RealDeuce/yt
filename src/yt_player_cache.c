#include "yt_player_cache.h"

#include <string.h>

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

void
yt_player_cache_raw(const struct yt_player_cache *cache,
    int player_record, enum yt_player_cache_kind kind, uint8_t raw[4])
{
	if (raw == NULL)
		return;
	if (cache == NULL || !yt_player_cache_contains(player_record)) {
		memset(raw, 0, 4U);
		return;
	}
	memcpy(raw, kind == YT_PLAYER_CACHE_SECTOR
	    ? cache->sector_raw[player_record]
	    : cache->cloak_raw[player_record], 4U);
}

bool
yt_player_cache_set_raw(struct yt_player_cache *cache,
    int player_record, enum yt_player_cache_kind kind, const uint8_t raw[4])
{
	if (cache == NULL || raw == NULL
	    || !yt_player_cache_contains(player_record))
		return false;
	if (kind == YT_PLAYER_CACHE_SECTOR) {
		memcpy(cache->sector_raw[player_record], raw, 4U);
		cache->sector[player_record] = qb_mbf32_decode(raw);
	}
	else {
		memcpy(cache->cloak_raw[player_record], raw, 4U);
		cache->cloak[player_record] = qb_mbf32_decode(raw);
	}
	return true;
}
