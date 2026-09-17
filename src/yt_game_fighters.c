#include "yt_game.h"

bool
yt_main_fighters_sector_overlay(struct yt_sector *sector,
    float desired, int player_record)
{
	if (sector == NULL
	    || !yt_record_set_number(&sector->record, YT_F81, desired)
	    || !yt_record_set_number(&sector->record, YT_F85,
	    (float)player_record))
		return false;
	sector->fighters = desired;
	sector->fighter_owner = (float)player_record;
	return true;
}

bool
yt_main_fighters_player_overlay(struct yt_player *player, float remaining)
{
	if (player == NULL
	    || !yt_record_set_number(&player->record, YT_F61, remaining))
		return false;
	player->fighters = remaining;
	return true;
}
