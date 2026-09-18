#include "yt_maint.h"

#include "yt_maint_internal.h"

#include "qb.h"

#include <errno.h>
#include <string.h>

static void
set_error(struct yt_error *error, enum yt_status status,
    const char *operation, const char *path)
{
	if (error == NULL)
		return;
	error->status = status;
	error->system_error = errno;
	snprintf(error->operation, sizeof(error->operation), "%s", operation);
	snprintf(error->path, sizeof(error->path), "%s",
	    path != NULL ? path : "");
}

static bool
lottery_fail(yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_error *error)
{
	static const uint8_t no_winner[] = "No one won a planet today.";

	return line_output(line_context, no_winner,
	    sizeof(no_winner) - 1U, error);
}

bool
yt_maintenance_super_lottery(struct yt_game *game, int player_count,
    int planet_count, int sector_count, const uint8_t *blank,
    size_t blank_length, yt_maintenance_score_line_fn line_output,
    void *line_context, struct yt_error *error)
{
	static const uint8_t phase[] = "Running Super Planet Lottery";
	static const uint8_t winner_prefix[] = " *** ";
	static const uint8_t winner_suffix[] =
	    " won a PLANET in the SUPER LOTTERY!!!!!\a";
	static const uint8_t radio_prefix[] =
	    "\aYou won a PLANET in the SUPER-LOTTERY! Look in sector";
	static const uint8_t dirty_zero[4] = {0x00, 0x00, 0x3b, 0x00};
	static const uint8_t canonical_zero[4] = {0};
	static const uint8_t name_suffix[] = "'s Planet";
	struct yt_player player;
	struct yt_planet planet;
	struct yt_sector sector;
	uint8_t working_name[50];
	uint8_t line[YT_MAINTENANCE_OUTPUT_ROW_SIZE];
	uint8_t radio[96];
	char sector_text[64];
	size_t working_length;
	size_t line_length;
	size_t radio_length;
	size_t player_name_length;
	uint16_t player_slot;
	int player_record;
	uint16_t planet_number;
	uint16_t sector_number;
	int index;
	int sector_length;
	float gate;

	if (game == NULL || player_count < 1 || planet_count < 1
	    || sector_count < 1 || (blank == NULL
	    && blank_length != 0U) || line_output == NULL) {
		set_error(error, YT_INVALID, "Super Lottery", "YTDATA.DAT");
		return false;
	}
	if (!line_output(line_context, blank, blank_length, error))
		return false;
	if (!line_output(line_context, phase, sizeof(phase) - 1U, error))
		return false;
	if (!yt_random_next(&game->random, &gate, error))
		return false;
	if (gate < 0.5f)
		return lottery_fail(line_output, line_context, error);
	if (!yt_random_integer(&game->random, player_count,
	    &player_slot, error))
		return false;
	player_record = player_slot + 1;
	if (!yt_game_read_player(game, player_record, &player, error))
		return false;
	if (player.name_length == 0U)
		return lottery_fail(line_output, line_context, error);
	player_name_length = player.name_length;
	working_length = player_name_length + sizeof(name_suffix) - 1U;
	memcpy(working_name, player.record.bytes, player_name_length);
	memcpy(working_name + player_name_length, name_suffix,
	    sizeof(name_suffix) - 1U);
	if (!yt_random_integer(&game->random, planet_count, &planet_number,
	    error))
		return false;
	if (!yt_game_read_planet(game, planet_number, &planet, error))
		return false;
	if (planet.name_length != 0U)
		return lottery_fail(line_output, line_context, error);
	if (!yt_random_integer(&game->random, sector_count, &sector_number,
	    error))
		return false;
	if (!yt_game_read_sector(game, sector_number, &sector, error))
		return false;
	if (sector.planet > 0)
		return lottery_fail(line_output, line_context, error);
	/* The constructor performs a second, fresh GET of the selected planet. */
	if (!yt_game_read_planet(game, planet_number, &planet, error))
		return false;
	memset(planet.record.bytes, ' ', 41U);
	memcpy(planet.record.bytes, working_name,
	    working_length < 41U ? working_length : 41U);
	if (!yt_record_set_number(&planet.record, YT_F85,
	    (float)working_length))
		return false;
	for (index = 0; index < 3; ++index) {
		float first;
		float second;
		float production;

		if (!yt_random_next(&game->random, &first, error))
			return false;
		if (!yt_random_next(&game->random, &second, error))
			return false;
		production = ((first * second) * 3000.0f);
		if (!yt_record_set_number(&planet.record,
		    YT_F45 + (size_t)index * 4U, production))
			return false;
		if (!yt_record_set_raw_number(&planet.record,
		    YT_F57 + (size_t)index * 4U, dirty_zero))
			return false;
	}
	if (!yt_record_set_raw_number(&planet.record, YT_F69, dirty_zero))
		return false;
	if (!yt_record_set_number(&planet.record, YT_F73,
	    (float)player_record))
		return false;
	if (!yt_random_next(&game->random, &gate, error))
		return false;
	if (!yt_record_set_number(&planet.record, YT_F77,
	    yt_maintenance_sint((
	    (gate * 100.0f) + 1.0f))))
		return false;
	if (!yt_random_next(&game->random, &gate, error))
		return false;
	if (!yt_record_set_number(&planet.record, YT_F117,
	    (gate * 16000000.0f)))
		return false;
	if (!yt_record_set_raw_number(&planet.record, YT_F125,
	    canonical_zero))
		return false;
	if (!yt_database_write(&game->database,
	    (size_t)yt_planet_basic_record(&game->config, planet_number),
	    &planet.record, error))
		return false;
	if (!yt_game_read_sector(game, sector_number, &sector, error))
		return false;
	sector.planet = planet_number;
	if (!yt_record_set_number(&sector.record, YT_F93,
	    (float)sector.planet))
		return false;
	if (!yt_database_write(&game->database,
	    (size_t)yt_sector_basic_record(&game->config, sector_number),
	    &sector.record, error))
		return false;
	line_length = 0U;
	if (!maintenance_copy_part(line, sizeof(line), &line_length,
	    winner_prefix, sizeof(winner_prefix) - 1U))
		return false;
	if (!maintenance_copy_part(line, sizeof(line), &line_length,
	    player.record.bytes, player_name_length))
		return false;
	if (!maintenance_copy_part(line, sizeof(line), &line_length,
	    winner_suffix, sizeof(winner_suffix) - 1U))
		return false;
	if (!line_output(line_context, line, line_length, error))
		return false;
	if (!yt_news_append_bytes(line, line_length, error))
		return false;
	sector_length = qb_str_single(sector_text, sizeof(sector_text),
	    (float)sector_number);
	radio_length = 0U;
	if (sector_length < 0)
		return false;
	if (!maintenance_copy_part(radio, sizeof(radio), &radio_length,
	    radio_prefix, sizeof(radio_prefix) - 1U))
		return false;
	if (!maintenance_copy_part(radio, sizeof(radio), &radio_length,
	    (const uint8_t *)sector_text, (size_t)sector_length))
		return false;
	if (!maintenance_copy_part(radio, sizeof(radio), &radio_length,
	    (const uint8_t *)"!\a", 2U))
		return false;
	if (!yt_radio_append_maintenance_bytes(radio, radio_length, -2,
	    (int8_t)player_record, error))
		return false;
	return true;
}
