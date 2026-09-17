#include "yt_game.h"
#include "yt_game_internal.h"

#include "qb.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

bool
yt_game_load_startup_configuration(struct yt_game *game, const char *path,
    bool local_mode, struct yt_player_cache *player_cache,
    int disruption_sectors[2], bool *local_screen, struct yt_error *error)
{
	struct yt_config *config;
	int basic;
	size_t index;

	if (game == NULL || path == NULL || player_cache == NULL
	    || disruption_sectors == NULL || local_screen == NULL)
		return false;
	config = &game->config;
	if (!yt_database_random_close(&game->database, error)
	    || !yt_database_open(&game->database, path,
	    YT_OPEN_UPDATE_CREATE, error)
	    || !yt_config_load(&game->database, config, error))
		return false;
	*local_screen = config->local_screen;
	qb_compat_upper_n((uint8_t *)config->scoreboard,
	    config->scoreboard_length);

	if (config->headquarters == 0.0f) {
		static const uint8_t headquarters_default[4] = {
			0x00, 0x40, 0x37, 0x8a
		};

		if (!yt_record_set_raw_number(&config->record, YT_F117,
		    headquarters_default)
		    || !yt_database_write_durable(&game->database, 1U,
		    &config->record, error))
			return false;
		config->headquarters = 733.0f;
	}
	if (config->genesis_ports < 20.0f) {
		config->genesis_ports = 200.0f;
	}
	if (config->scoreboard_length == 0U) {
		static const char default_path[] = "YTSCORE.ASC";

		memcpy(config->scoreboard, default_path, sizeof(default_path));
		config->scoreboard_length = sizeof(default_path) - 1U;
	}
	if (local_mode) {
		config->local_screen = true;
		*local_screen = true;
	}
	if (config->lottery_plays < 0.0f || config->lottery_plays > 9.0f) {
		config->lottery_plays = 3.0f;
	}
	if (config->maximum_planets == 0.0f) {
		config->maximum_planets = 100.0f;
	}
	if (config->maximum_holds < 5.0f || config->maximum_holds > 1000.0f) {
		config->maximum_holds = 1000.0f;
	}
	if (config->turns_per_day < 100.0f
	    || config->turns_per_day > 2500.0f) {
		config->turns_per_day = 500.0f;
	}

	for (basic = 2; basic <= (int)config->sector_offset; ++basic) {
		struct yt_player player;

		if (!yt_player_cache_contains(basic))
			return yt_game_error(error, YT_RANGE,
			    "startup player-cache index");
		if (!yt_game_read_player(game, basic, &player, error))
			return false;
		(void)yt_player_cache_set_sector(player_cache, basic,
		    player.sector);
		(void)yt_player_cache_set_cloak(player_cache, basic,
		    player.cloak);
		if (player.cloak < 0.0f || player.cloak > 1.0f) {
			player.cloak = 1.0f;
			(void)yt_player_cache_set_cloak(player_cache, basic,
			    player.cloak);
			if (!yt_record_set_number(&player.record, YT_F125, 1.0f)
			    || !yt_database_write_durable(&game->database,
			    (size_t)basic, &player.record, error))
				return false;
		}
	}
	for (index = 0U; index < 2U; ++index) {
		float draw;
		float difference;
		float span;
		float product;
		float integral;

		if (!yt_random_next(&game->random, &draw, error))
			return false;
		difference = qb_single_subtract(config->port_offset,
		    config->sector_offset);
		span = qb_single_subtract(difference, 2.0f);
		product = qb_single_multiply(draw, span);
		integral = floorf(product);
		disruption_sectors[index] =
		    (int)qb_single_add(integral, 2.0f);
	}
	return true;
}




enum yt_sector_force_route
yt_sector_force_route(float fighters, int owner, int current_player_record,
    int *owner_record)
{
	if (owner_record != NULL)
		*owner_record = 0;
	if (fighters == 0.0f || owner == current_player_record)
		return YT_SECTOR_FORCE_FRIENDLY;
	if (owner <= 0)
		return YT_SECTOR_FORCE_HOSTILE;
	if (owner_record != NULL)
		*owner_record = owner;
	return YT_SECTOR_FORCE_OWNER_GET;
}

bool
yt_sector_mines_admitted(float mines, float suppression)
{
	return mines > 0.0f && suppression == 0.0f;
}

bool
yt_sector_force_same_team(int current_team, int owner_team)
{
	return current_team != 0 && owner_team == current_team;
}

enum yt_port_owner_kind
yt_port_owner_classify(int owner, int current_player_record,
    int *owner_record)
{
	if (owner_record != NULL)
		*owner_record = 0;
	if (owner <= 1)
		return YT_PORT_OWNER_SILENT;
	if (owner == current_player_record)
		return YT_PORT_OWNER_SELF;
	if (owner_record != NULL)
		*owner_record = owner;
	return YT_PORT_OWNER_OTHER;
}

bool
yt_port_owner_compose(enum yt_port_owner_kind kind, float treasury,
    const uint8_t *owner_name, size_t owner_name_length,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "This port is owned by: ";
	static const uint8_t self[] = "YOU, Credits:";
	char treasury_text[80];
	const uint8_t *suffix;
	size_t suffix_length;
	size_t needed;
	int formatted_length;

	if (length == NULL)
		return false;
	*length = 0U;
	if (kind == YT_PORT_OWNER_SILENT)
		return true;
	if (kind == YT_PORT_OWNER_SELF) {
		formatted_length = qb_str_double(treasury_text,
		    sizeof(treasury_text), (double)treasury);
		if (formatted_length < 0)
			return false;
		needed = sizeof(prefix) - 1U + sizeof(self) - 1U
		    + (size_t)formatted_length;
		if (needed > capacity || (needed != 0U && row == NULL))
			return false;
		memcpy(row, prefix, sizeof(prefix) - 1U);
		memcpy(row + sizeof(prefix) - 1U, self, sizeof(self) - 1U);
		memcpy(row + sizeof(prefix) - 1U + sizeof(self) - 1U,
		    treasury_text, (size_t)formatted_length);
		*length = needed;
		return true;
	}
	if (kind != YT_PORT_OWNER_OTHER
	    || (owner_name == NULL && owner_name_length != 0U))
		return false;
	suffix = owner_name;
	suffix_length = owner_name_length;
	needed = sizeof(prefix) - 1U + suffix_length;
	if (needed > capacity || (needed != 0U && row == NULL))
		return false;
	memcpy(row, prefix, sizeof(prefix) - 1U);
	if (suffix_length != 0U)
		memcpy(row + sizeof(prefix) - 1U, suffix, suffix_length);
	*length = needed;
	return true;
}
