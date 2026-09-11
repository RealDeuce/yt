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
	uint8_t sector_raw[YT_PLAYER_CACHE_RECORDS][4];
	uint8_t cloak_raw[YT_PLAYER_CACHE_RECORDS][4];
};

bool yt_player_cache_contains(int player_record);
float yt_player_cache_value(const struct yt_player_cache *cache,
	int player_record, enum yt_player_cache_kind kind);
void yt_player_cache_raw(const struct yt_player_cache *cache,
	int player_record, enum yt_player_cache_kind kind, uint8_t raw[4]);
bool yt_player_cache_set_raw(struct yt_player_cache *cache,
	int player_record, enum yt_player_cache_kind kind, const uint8_t raw[4]);

#endif
