#include "yt_game.h"

#include "qb.h"

bool
yt_main_fighters_sector_overlay(struct yt_sector *sector,
    const uint8_t desired_raw[4], int player_record)
{
	uint8_t owner_raw[4];

	if (sector == NULL || desired_raw == NULL
	    || qb_mbf32_encode((float)player_record, owner_raw)
	    == QB_MBF_OVERFLOW
	    || !yt_record_set_raw_number(&sector->record, YT_F81, desired_raw)
	    || !yt_record_set_raw_number(&sector->record, YT_F85, owner_raw))
		return false;
	sector->fighters = qb_mbf32_decode(desired_raw);
	sector->fighter_owner = qb_mbf32_decode(owner_raw);
	return true;
}

bool
yt_main_fighters_player_overlay(struct yt_player *player, float remaining)
{
	uint8_t remaining_raw[4];

	if (player == NULL
	    || qb_mbf32_encode(remaining, remaining_raw) == QB_MBF_OVERFLOW
	    || !yt_record_set_raw_number(&player->record, YT_F61,
	    remaining_raw))
		return false;
	player->fighters = qb_mbf32_decode(remaining_raw);
	return true;
}
