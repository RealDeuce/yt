#include "yt_maint.h"

#include "yt_maint_internal.h"

#include "qb.h"

#include <errno.h>
#include <math.h>

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

static float
xannor_quantum(float first, float second)
{
	float quantum = first > 5000.0f && second > 5000.0f
	    ? 5000.0f : 500.0f;

	if (first < 500.0f || second < 500.0f)
		quantum = 1.0f;
	return quantum;
}

static bool
xannor_player_fighter_phase(struct yt_random *random,
    float *player_fighters, float original_xannor,
    float *player_fighter_losses, float *xannor_fighter_losses,
    struct yt_error *error)
{
	float original_player = *player_fighters;
	float player_losses = 0.0f;
	float xannor_losses = 0.0f;

	while (player_losses < original_player
	    && xannor_losses < original_xannor) {
		float sample;
		float quantum = xannor_quantum(original_player - player_losses,
		    original_xannor - xannor_losses);

		if (!yt_random_next(random, &sample, error))
			return false;
		if (sample > 0.5f)
			xannor_losses = qb_single_add(xannor_losses, quantum);
		else
			player_losses = qb_single_add(player_losses, quantum);
	}
	player_losses = fminf(player_losses, original_player);
	xannor_losses = fminf(xannor_losses, original_xannor);
	*player_fighters = qb_single_subtract(original_player, player_losses);
	*player_fighter_losses = player_losses;
	*xannor_fighter_losses = xannor_losses;
	return true;
}

static bool
xannor_player_shield_phase(struct yt_random *random, float player_fighters,
    float *player_shields, float original_xannor,
    float *xannor_fighter_losses,
    struct yt_error *error)
{
	float xannor_losses = *xannor_fighter_losses;

	while (player_fighters < 1.0f
	    && xannor_losses < original_xannor && *player_shields > 0.0f) {
		float sample;
		float quantum = xannor_quantum(original_xannor - xannor_losses,
		    *player_shields);

		if (!yt_random_next(random, &sample, error))
			return false;
		if (sample >= 0.5f)
			*player_shields = qb_single_subtract(*player_shields, quantum);
		else
			xannor_losses = qb_single_add(xannor_losses, quantum);
	}
	if (*player_shields < 0.0f)
		*player_shields = 0.0f;
	*xannor_fighter_losses = fminf(xannor_losses, original_xannor);
	return true;
}

static bool
xannor_reclaim_and_relocate(struct maint_state *state, float location[21],
    float size[21], double regeneration,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_error *error)
{
	bool original_hostile;

	if (!yt_maintenance_xannor_headquarters_reclaim(&state->game,
	    location, size, line_output, line_context, &original_hostile, error))
		return false;
	return yt_maintenance_xannor_headquarters_relocate(&state->game,
	    location, original_hostile, size[1], regeneration,
	    NULL, 0U,
	    line_output, line_context, error);
}

static bool
consume_revenge_slot(struct maint_state *state, int *live_sector,
    int *cached_target, yt_maintenance_score_line_fn line_output,
    void *line_context, struct yt_error *error)
{
	if (!yt_maintenance_xannor_revenge_slot(&state->game,
	    state->player_sector, (size_t)state->player_count + 2U,
	    NULL, 0U,
	    line_output, line_context, live_sector, cached_target, error))
		return false;
	return true;
}

static bool
xannor_candidate_target(struct maint_state *state, float current_location,
    int revenge_live, int revenge_cached, int *target,
    struct yt_error *error)
{
	bool overflow;
	int32_t current = qb_cint(current_location, &overflow);

	if (overflow) {
		set_error(error, YT_RANGE, "Xannor current sector",
		    "YTDATA.DAT");
		return false;
	}
	if (!yt_maintenance_xannor_candidate_discovery(&state->game,
	    state->player_sector, state->player_cloak,
	    (size_t)state->player_count + 2U, current, revenge_live,
	    revenge_cached, target, error))
		return false;
	return true;
}

static bool
xannor_arrival_emit(yt_maintenance_score_line_fn line_output,
    void *line_context, const uint8_t *line, size_t length,
    struct yt_error *error)
{
	if (!yt_news_append_bytes(line, length, error))
		return false;
	return line_output(line_context, line, length, error);
}

bool
yt_maintenance_xannor_sector_arrival(struct yt_game *game,
    int sector_number, float *group_size, struct yt_sector *sector,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_error *error)
{
	static const uint8_t mercenaries[] = "Mercenaries";
	static const uint8_t hit_prefix[] = " ***";
	static const uint8_t hit_middle[] =
	    " Xannor hit sector mines in sector";
	static const uint8_t killed[] = " *** The Xannor were killed!";
	static const uint8_t loss_prefix[] = " *** Lost a total of";
	static const uint8_t loss_suffix[] = " fighters!";
	static const uint8_t defense_prefix[] = " *** ";
	static const uint8_t defense_lost[] = ": lost";
	static const uint8_t defense_destroyed[] = ", dstrd";
	static const uint8_t player_destroyed[] = " (Plyr ftrs dstrd)";
	static const uint8_t xannor_destroyed[] = " (Xannor ftrs dstrd)";
	struct yt_maintenance_text defender = {
		mercenaries, sizeof(mercenaries) - 1U
	};
	struct yt_player player;
	uint8_t defender_name[YT_TEXT_FIELD_SIZE];
	uint8_t line[YT_MAINTENANCE_OUTPUT_ROW_SIZE];
	char first[64];
	char second[64];
	float initial_group;
	float initial_defenders;
	int initial_owner;
	float defense_group;
	float remaining_defenders;
	size_t length;
	int first_length;
	int second_length;

	if (game == NULL || group_size == NULL || sector == NULL
	    || line_output == NULL || sector_number < 0) {
		set_error(error, YT_INVALID, "Xannor sector arrival",
		    "YTDATA.DAT");
		return false;
	}
	if (!yt_game_read_sector(game, sector_number, sector, error))
		return false;
	initial_group = *group_size;

	while (sector->mines > 0.0f && *group_size > 0.0f) {
		uint16_t damage;

		if (!yt_random_integer(&game->random, 1000,
		    &damage, error))
			return false;
		if ((float)damage > *group_size)
			damage = (uint16_t)*group_size;
		*group_size = qb_single_subtract(*group_size, (float)damage);
		sector->mines = qb_single_subtract(sector->mines, 1.0f);
	}
	if (initial_group != *group_size) {
		float remaining_mines = sector->mines;

		if (!yt_game_read_sector(game, sector_number, sector, error))
			return false;
		sector->mines = remaining_mines;
		if (!yt_record_set_number(&sector->record, YT_F129,
		    remaining_mines)) {
			set_error(error, YT_RANGE, "encode Xannor sector mines",
			    "YTDATA.DAT");
			return false;
		}
		if (!yt_game_write_sector(game, sector_number, sector, error))
			return false;
		length = 0U;
		first_length = qb_str_single(first, sizeof(first), initial_group);
		second_length = qb_str_single(second, sizeof(second),
		    (float)sector_number);
		if (first_length < 0 || second_length < 0)
			return false;
		if (!maintenance_copy_part(line, sizeof(line), &length,
		    hit_prefix, sizeof(hit_prefix) - 1U))
			return false;
		if (!maintenance_copy_part(line, sizeof(line), &length,
		    (const uint8_t *)first, (size_t)first_length))
			return false;
		if (!maintenance_copy_part(line, sizeof(line), &length,
		    hit_middle, sizeof(hit_middle) - 1U))
			return false;
		if (!maintenance_copy_part(line, sizeof(line), &length,
		    (const uint8_t *)second, (size_t)second_length))
			return false;
		if (!maintenance_copy_part(line, sizeof(line), &length,
		    (const uint8_t *)"!", 1U))
			return false;
		if (!xannor_arrival_emit(line_output, line_context, line,
		    length, error))
			return false;
		if (*group_size <= 0.0f) {
			if (!xannor_arrival_emit(line_output, line_context, killed,
			    sizeof(killed) - 1U, error))
				return false;
		}
		else {
			length = 0U;
			first_length = qb_str_single(first, sizeof(first),
			    qb_single_subtract(initial_group, *group_size));
			if (first_length < 0)
				return false;
			if (!maintenance_copy_part(line, sizeof(line), &length,
			    loss_prefix, sizeof(loss_prefix) - 1U))
				return false;
			if (!maintenance_copy_part(line, sizeof(line), &length,
			    (const uint8_t *)first, (size_t)first_length))
				return false;
			if (!maintenance_copy_part(line, sizeof(line), &length,
			    loss_suffix, sizeof(loss_suffix) - 1U))
				return false;
			if (!xannor_arrival_emit(line_output, line_context, line,
			    length, error))
				return false;
		}
	}
	if (*group_size <= 0.0f)
		*group_size = 0.0f;
	initial_defenders = sector->fighters;
	initial_owner = sector->fighter_owner;
	defense_group = *group_size;
	if (!yt_maintenance_xannor_defense(&game->random, group_size,
	    &sector->fighters, &sector->fighter_owner, error))
		return false;
	if (initial_defenders == sector->fighters
	    && defense_group == *group_size)
		return true;
	remaining_defenders = sector->fighters;
	if (initial_owner > 0) {
		int record = initial_owner;

		if (!yt_game_read_player(game, record, &player, error)) {
			if (error != NULL && error->status == YT_OK)
				set_error(error, YT_RANGE,
				    "Xannor defense owner", "YTDATA.DAT");
			return false;
		}
		defender.length = yt_player_stored_name(&player, defender_name);
		defender.data = defender_name;
	}
	if (!yt_game_read_sector(game, sector_number, sector, error))
		return false;
	sector->fighters = remaining_defenders;
	if (!yt_record_set_number(&sector->record, YT_F81,
	    remaining_defenders)) {
		set_error(error, YT_RANGE, "encode Xannor sector defense",
		    "YTDATA.DAT");
		return false;
	}
	if (remaining_defenders < 1.0f) {
		sector->fighter_owner = 0;
		if (!yt_record_set_number(&sector->record, YT_F85, 0.0f)) {
			set_error(error, YT_RANGE, "encode Xannor sector owner",
			    "YTDATA.DAT");
			return false;
		}
	}
	if (!yt_game_write_sector(game, sector_number, sector, error))
		return false;
	length = 0U;
	first_length = qb_str_single(first, sizeof(first),
	    qb_single_subtract(initial_defenders, remaining_defenders));
	second_length = qb_str_single(second, sizeof(second),
	    qb_single_subtract(defense_group, *group_size));
	if (first_length < 0 || second_length < 0)
		return false;
	if (!maintenance_copy_part(line, sizeof(line), &length,
	    defense_prefix, sizeof(defense_prefix) - 1U))
		return false;
	if (!maintenance_copy_part(line, sizeof(line), &length,
	    defender.data, defender.length))
		return false;
	if (!maintenance_copy_part(line, sizeof(line), &length,
	    defense_lost, sizeof(defense_lost) - 1U))
		return false;
	if (!maintenance_copy_part(line, sizeof(line), &length,
	    (const uint8_t *)first, (size_t)first_length))
		return false;
	if (!maintenance_copy_part(line, sizeof(line), &length,
	    defense_destroyed, sizeof(defense_destroyed) - 1U))
		return false;
	if (!maintenance_copy_part(line, sizeof(line), &length,
	    (const uint8_t *)second, (size_t)second_length))
		return false;
	if (!maintenance_copy_part(line, sizeof(line), &length,
	    remaining_defenders < 1.0f ? player_destroyed : xannor_destroyed,
	    remaining_defenders < 1.0f ? sizeof(player_destroyed) - 1U
	    : sizeof(xannor_destroyed) - 1U))
		return false;
	if (!xannor_arrival_emit(line_output, line_context, line, length,
	    error))
		return false;
	return true;
}

bool
yt_maintenance_xannor_planet_arrival(struct yt_game *game,
    float *group_location, float *group_size, struct yt_sector *sector,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_error *error)
{
	static const uint8_t attack_prefix[] = " ***";
	static const uint8_t attack_middle[] =
	    " Xannor attacked the planet \"";
	static const uint8_t quote[] = "\"";
	static const uint8_t fighters_destroyed[] =
	    " *** Xannor fighters destroyed!";
	static const uint8_t planet_prefix[] = " *** Planet \"";
	static const uint8_t planet_suffix[] = "\" destroyed!";
	struct yt_planet planet;
	struct yt_planet mutated;
	uint8_t stored_name[YT_TEXT_FIELD_SIZE];
	uint8_t line[YT_MAINTENANCE_OUTPUT_ROW_SIZE];
	size_t stored_name_length;
	size_t line_length;
	bool destroyed = false;
	int arrival_sector_number;
	int planet_number;
	int index;
	char number[64];
	int number_length;

	if (game == NULL || group_location == NULL || group_size == NULL
	    || sector == NULL || line_output == NULL) {
		set_error(error, YT_INVALID, "Xannor planet arrival", "");
		return false;
	}
	if (sector->planet <= 0)
		return true;
	arrival_sector_number = (int)*group_location;
	planet_number = sector->planet;
	if (!yt_game_read_planet(game, planet_number, &planet, error))
		return false;
	if (planet.name_length == 0U
	    || planet.owner == -1)
		return true;
	if (!yt_game_read_planet(game, planet_number, &planet, error))
		return false;
	stored_name_length = yt_planet_stored_name(&planet, stored_name);
	number_length = qb_str_single(number, sizeof(number), *group_size);
	line_length = 0U;
	if (number_length < 0)
		return false;
	if (!maintenance_copy_part(line, sizeof(line), &line_length,
	    attack_prefix, sizeof(attack_prefix) - 1U))
		return false;
	if (!maintenance_copy_part(line, sizeof(line), &line_length,
	    (const uint8_t *)number, (size_t)number_length))
		return false;
	if (!maintenance_copy_part(line, sizeof(line), &line_length,
	    attack_middle, sizeof(attack_middle) - 1U))
		return false;
	if (!maintenance_copy_part(line, sizeof(line), &line_length,
	    stored_name, stored_name_length))
		return false;
	if (!maintenance_copy_part(line, sizeof(line), &line_length,
	    quote, sizeof(quote) - 1U))
		return false;
	if (!xannor_arrival_emit(line_output, line_context, line,
	    line_length, error))
		return false;
	while (planet.ground_forces > 0.0f && *group_size > 0.0f) {
		float sample;

		if (!yt_random_next(&game->random, &sample, error))
			return false;
		*group_size = qb_single_subtract(*group_size, 1.0f);
		planet.ground_forces = qb_single_subtract(planet.ground_forces,
		    floorf(qb_single_multiply(sample, 1000.0f)));
	}
	if (planet.ground_forces < 0.0f)
		planet.ground_forces = 0.0f;
	while (*group_size > 0.0f
	    && (planet.production[0] > 0.0f
	    || planet.production[1] > 0.0f
	    || planet.production[2] > 0.0f)) {
		float gate;
		float quantum = (planet.production[0] > 500.0f
		    || planet.production[1] > 500.0f
		    || planet.production[2] > 500.0f) && *group_location != 0.0f
		    ? 450.0f : 1.0f;

		if (!yt_random_next(&game->random, &gate, error))
			return false;
		if (gate < 0.5f) {
			for (index = 0; index < 3; ++index) {
				float sample;

				if (!yt_random_next(&game->random, &sample, error))
					return false;
				planet.production[index] = qb_single_subtract(
				    planet.production[index],
				    qb_single_divide(qb_single_multiply(sample, quantum), 3.0f));
				if (planet.production[index] < 0.0f)
					planet.production[index] = 0.0f;
			}
		}
		else
			*group_size = qb_single_subtract(*group_size, quantum);
	}
	for (index = 0; index < 3; ++index) {
		float cap = qb_single_multiply(planet.production[index], 10.0f);

		if (planet.stock[index] > cap)
			planet.stock[index] = cap;
	}
	if (planet.ground_forces <= 0.0f)
		planet.owner = 0;
	destroyed = planet.production[0] == 0.0f
	    && planet.production[1] == 0.0f
	    && planet.production[2] == 0.0f;
	if (destroyed) {
		sector->planet = 0;
		planet.name_length = 0U;
	}
	if (*group_size <= 0.0f) {
		*group_size = 0.0f;
		*group_location = 0.0f;
	}
	mutated = planet;
	if (!yt_game_read_planet(game, planet_number, &planet, error))
		return false;
	for (index = 0; index < 3; ++index) {
		planet.production[index] = mutated.production[index];
		planet.stock[index] = mutated.stock[index];
		if (!yt_record_set_number(&planet.record,
		    YT_F45 + (size_t)index * 4U, planet.production[index]))
			goto encode_error;
		if (!yt_record_set_number(&planet.record,
		    YT_F57 + (size_t)index * 4U, planet.stock[index]))
			goto encode_error;
	}
	planet.owner = mutated.owner;
	planet.ground_forces = mutated.ground_forces;
	if (!yt_record_set_number(&planet.record, YT_F73,
	    (float)planet.owner))
		goto encode_error;
	if (!yt_record_set_number(&planet.record, YT_F77,
	    planet.ground_forces))
		goto encode_error;
	if (!yt_database_write(&game->database,
	    (size_t)yt_planet_basic_record(&game->config, planet_number),
	    &planet.record, error))
		return false;
	if (destroyed) {
		if (!yt_game_read_sector(game, arrival_sector_number, sector,
		    error))
			return false;
		sector->planet = 0;
		if (!yt_record_set_number(&sector->record, YT_F93, 0.0f)) {
			set_error(error, YT_RANGE, "encode destroyed Xannor link",
			    "YTDATA.DAT");
			return false;
		}
		if (!yt_game_write_sector(game, arrival_sector_number, sector,
		    error))
			return false;
		if (!yt_game_read_planet(game, planet_number, &planet, error))
			return false;
		planet.name_length = 0U;
		if (!yt_record_set_number(&planet.record, YT_F85, 0.0f)) {
			set_error(error, YT_RANGE,
			    "encode destroyed Xannor planet", "YTDATA.DAT");
			return false;
		}
		if (!yt_database_write(&game->database,
		    (size_t)yt_planet_basic_record(&game->config, planet_number),
		    &planet.record, error))
			return false;
	}
	if (*group_size <= 0.0f) {
		if (!xannor_arrival_emit(line_output, line_context,
		    fighters_destroyed, sizeof(fighters_destroyed) - 1U, error))
			return false;
	}
	else {
		line_length = 0U;
		if (!maintenance_copy_part(line, sizeof(line), &line_length,
		    planet_prefix, sizeof(planet_prefix) - 1U))
			return false;
		if (!maintenance_copy_part(line, sizeof(line), &line_length,
		    stored_name, stored_name_length))
			return false;
		if (!maintenance_copy_part(line, sizeof(line), &line_length,
		    planet_suffix, sizeof(planet_suffix) - 1U))
			return false;
		if (!xannor_arrival_emit(line_output, line_context, line,
		    line_length, error))
			return false;
	}
	return true;

encode_error:
	set_error(error, YT_RANGE, "encode Xannor planet arrival",
	    "YTDATA.DAT");
	return false;
}

bool
yt_maintenance_xannor_player_arrival(struct yt_game *game,
    int *player_sector, float *player_cloak, size_t cache_count,
    int player_record, float *xannor_fighters,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_error *error)
{
	struct yt_player player;
	float original_xannor;
	float original_shields;
	float remaining_fighters;
	float remaining_shields;
	float player_fighter_losses;
	float xannor_fighter_losses;
	uint8_t stored_name[YT_TEXT_FIELD_SIZE];
	uint8_t line[420];
	char radio_line[420];
	size_t stored_name_length;
	size_t line_length;
	bool killed;
	int player_count;

	if (game == NULL || player_sector == NULL || player_cloak == NULL
	    || xannor_fighters == NULL || line_output == NULL) {
		set_error(error, YT_INVALID, "Xannor player arrival", "");
		return false;
	}
	player_count = (int)game->config.sector_offset - 1;
	if (player_record < 2 || player_record > player_count + 1
	    || (size_t)player_record >= cache_count) {
		set_error(error, YT_RANGE, "Xannor player arrival", "YTDATA.DAT");
		return false;
	}
	if (!yt_game_read_player(game, player_record, &player, error))
		return false;
	if (player.name_length == 0U
	    || player.killed_by != 0)
		return true;
	player_sector[player_record] = player.sector;
	player_cloak[player_record] = player.cloak;
	original_xannor = *xannor_fighters;
	original_shields = player.shields;
	remaining_shields = original_shields;
	if (!xannor_player_fighter_phase(&game->random, &player.fighters,
	    original_xannor, &player_fighter_losses, &xannor_fighter_losses,
	    error))
		return false;
	remaining_fighters = player.fighters;
	if (player_fighter_losses > 0.0f) {
		char losses[48];

		qb_str_double(losses, sizeof(losses),
		    (double)player_fighter_losses);
		snprintf(radio_line, sizeof(radio_line),
		    "Ha! We kilt%s of yoor fyterz hoo-man slyme!", losses);
		if (!yt_radio_append_maintenance(radio_line, -1,
		    (int8_t)player_record, error))
			return false;
	}
	if (!yt_game_read_player(game, player_record, &player, error))
		return false;
	player.fighters = remaining_fighters;
	if (!yt_record_set_number(&player.record, YT_F61, player.fighters))
		return false;
	if (!yt_database_write(&game->database, (size_t)player_record,
	    &player.record, error))
		return false;
	if (!xannor_player_shield_phase(&game->random, player.fighters,
	    &remaining_shields, original_xannor, &xannor_fighter_losses,
	    error))
		return false;
	if (original_shields - remaining_shields > 0.0f) {
		char losses[48];

		qb_str_double(losses, sizeof(losses),
		    (double)(original_shields - remaining_shields));
		snprintf(radio_line, sizeof(radio_line),
		    "Peh! Whee maik yoor wheak sheeldz%s unitz!", losses);
		if (!yt_radio_append_maintenance(radio_line, -1,
		    (int8_t)player_record, error))
			return false;
	}
	if (!yt_game_read_player(game, player_record, &player, error))
		return false;
	player.shields = remaining_shields;
	if (!yt_record_set_number(&player.record, YT_F53, player.shields))
		return false;
	if (!yt_database_write(&game->database, (size_t)player_record,
	    &player.record, error))
		return false;
	*xannor_fighters = qb_single_subtract(original_xannor,
	    xannor_fighter_losses);
	killed = player.shields < 1.0f;
	if (killed) {
		if (!yt_game_read_player(game, player_record, &player, error))
			return false;
		if (!yt_maintenance_immediate_death(game, player_sector,
		    player_cloak, cache_count, player_record, -1, &player,
		    error))
			return false;
	}
	if (*xannor_fighters <= 0.0f)
		*xannor_fighters = 0.0f;
	if (!yt_game_read_player(game, player_record, &player, error))
		return false;
	stored_name_length = yt_player_stored_name(&player, stored_name);
	if (!yt_maintenance_xannor_player_line_bytes(stored_name,
	    stored_name_length, player_fighter_losses, xannor_fighter_losses,
	    *xannor_fighters, player.shields,
	    killed, line, sizeof(line), &line_length))
		return false;
	if (!yt_news_append_bytes(line, line_length, error))
		return false;
	if (!line_output(line_context, line, line_length, error))
		return false;
	if (killed) {
		if (!yt_game_read_player(game, player_record, &player, error))
			return false;
		if (!yt_radio_append_maintenance(
		    "HA! We kilt yoo yoo hoo-man slyme bull!", -1,
		    (int8_t)player_record, error))
			return false;
	}
	return true;
}

static bool
xannor_attack_players(struct maint_state *state, int group,
    float location[21], float size[21],
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_error *error)
{
	int record = 2;

	while (yt_maintenance_xannor_player_scan_continue(record,
	    state->player_count)) {
		float cloak_draw;

		if (!yt_random_next(&state->game.random, &cloak_draw, error))
			return false;
		if (!yt_maintenance_xannor_player_scan_admit(group,
		    location[group], state->player_sector[record],
		    state->player_cloak[record], cloak_draw)) {
			++record;
			continue;
		}
		if (!yt_maintenance_xannor_player_arrival(&state->game,
		    state->player_sector, state->player_cloak,
		    (size_t)state->player_count + 2U, record, &size[group],
		    line_output, line_context, error))
			return false;
		if (size[group] <= 0.0f)
			location[group] = 0.0f;
		++record;
	}
	return true;
}

bool
yt_maintenance_xannor_route_arrivals(struct maint_state *state, int group,
    int target, float top_player_target, float location[21], float size[21],
    yt_maintenance_score_line_fn line_output, void *line_context,
    bool *reached_target,
    struct yt_error *error)
{
	int hops = 0;

	*reached_target = false;

	for (;;) {
		struct yt_sector destination;
		bool overflow;
		int32_t source;
		int next;

		source = qb_cint(location[group], &overflow);
		if (overflow) {
			set_error(error, YT_RANGE, "Xannor route source",
			    "YTDATA.DAT");
			return false;
		}
		if (!yt_maintenance_route_next_hop(&state->game,
		    &state->route_cache, source, target, &next,
		    error))
			return false;
		if (next == 0) {
			if (location[group] != (float)target) {
				struct yt_maintenance_output_result output;

				if (!yt_maintenance_compose_xannor_path_error(
				    (uint16_t)source, (uint16_t)target, &output))
					return false;
				if (!maintenance_emit_output_row(&output,
				    YT_MAINT_ROW_XANNOR_PATH_ERROR, line_output,
				    line_context, error))
					return false;
			}
			break;
		}
		if (size[group] < 1.0f || location[group] < 1.0f) {
			size[group] = 0.0f;
			location[group] = 0.0f;
			break;
		}
		/*
		 * The shipped first-pass 3A5E gate sends group 20 from its
		 * retained top-player sector through the 3C90 completion test
		 * before using the already-built next hop.  With an ordinary
		 * route this either completes or returns to the same arrival
		 * continuation without another externally visible operation.
		 */
		if (hops == 0
		    && yt_maintenance_xannor_bypass_initial_arrival(group,
		    location[group], top_player_target)
		    && yt_maintenance_xannor_route_complete(location[group],
		    target)) {
			*reached_target = true;
			break;
		}
		location[group] = (float)next;
		if (!yt_maintenance_xannor_sector_arrival(&state->game, next,
		    &size[group], &destination, line_output, line_context, error))
			return false;
		if (!yt_maintenance_xannor_planet_arrival(&state->game,
		    &location[group], &size[group], &destination, line_output,
		    line_context, error))
			return false;
		if (yt_maintenance_xannor_post_planet_exhausted(
		    location[group], size[group])) {
			location[group] = 0.0f;
			size[group] = 0.0f;
		}
		else if (!xannor_attack_players(state, group, location, size,
		    line_output, line_context, error))
			return false;
		++hops;
		*reached_target = yt_maintenance_xannor_route_complete(
		    location[group], target);
		if (*reached_target || location[group] <= 0.0f
		    || size[group] <= 0.0f)
			break;
	}
	return true;
}

bool
yt_maintenance_xannor_groups_persist(struct yt_game *game,
    const float location[21], const float size[21], struct yt_error *error)
{
	int group;
	int sector_count;

	if (game == NULL || location == NULL || size == NULL) {
		set_error(error, YT_INVALID, "Xannor group persistence",
		    "YTDATA.DAT");
		return false;
	}
	sector_count = (int)game->config.port_offset
	    - (int)game->config.sector_offset;
	if (sector_count < 20) {
		set_error(error, YT_RANGE, "Xannor group persistence",
		    "YTDATA.DAT");
		return false;
	}

	for (group = 1; group <= 20; ++group) {
		struct yt_sector metadata;

		if (!yt_game_read_sector(game, group, &metadata, error))
			return false;
		if (!yt_record_set_number(&metadata.record, YT_F105,
		    location[group])) {
			if (error != NULL && error->status == YT_OK)
				set_error(error, YT_RANGE,
				    "encode Xannor group metadata", "YTDATA.DAT");
			return false;
		}
		if (!yt_database_write(&game->database,
		    (size_t)yt_sector_basic_record(&game->config, group),
		    &metadata.record, error)) {
			if (error != NULL && error->status == YT_OK)
				set_error(error, YT_RANGE,
				    "encode Xannor group metadata", "YTDATA.DAT");
			return false;
		}
		if (location[group] > 0.0f && size[group] > 0.0f) {
			struct yt_sector host;
			bool overflow;
			int32_t logical = qb_cint(location[group], &overflow);

			if (overflow || logical < 1 || logical > sector_count) {
				set_error(error, YT_RANGE, "Xannor group sector",
				    "YTDATA.DAT");
				return false;
			}
			if (!yt_game_read_sector(game, logical, &host, error))
				return false;
			host.fighters = qb_single_add(host.fighters, size[group]);
			host.fighter_owner = -1;
			if (!yt_game_write_sector(game, logical, &host, error))
				return false;
		}
	}
	return true;
}

bool
yt_maintenance_xannor_group_twenty_finish(struct yt_game *game,
    int group_number, float group_location, float group_size,
    struct yt_error *error)
{
	struct yt_sector sector;
	bool overflow;
	int sector_count;
	int32_t logical;

	if (game == NULL || group_number < 2 || group_number > 20) {
		set_error(error, YT_INVALID, "Xannor group 20 finish",
		    "YTDATA.DAT");
		return false;
	}
	if (group_number != 20 || group_size <= 0.0f
	    || group_location <= 0.0f)
		return true;
	sector_count = (int)game->config.port_offset
	    - (int)game->config.sector_offset;
	logical = qb_cint(group_location, &overflow);
	if (overflow || logical < 1 || logical > sector_count) {
		set_error(error, YT_RANGE, "Xannor group 20 sector",
		    "YTDATA.DAT");
		return false;
	}
	if (!yt_game_read_sector(game, logical, &sector, error))
		return false;
	sector.fighters = qb_single_add(sector.fighters, 1.0f);
	return yt_game_write_sector(game, logical, &sector, error);
}

bool
yt_maintenance_xannor_target_finish(struct yt_game *game,
    int *player_sector, float *player_cloak, size_t cache_count,
    bool reached_target, int group_number, int hunt_player,
    float *group_location, float *group_size,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_error *error)
{
	if (game == NULL || player_sector == NULL || player_cloak == NULL
	    || group_location == NULL || group_size == NULL
	    || line_output == NULL || group_number < 2 || group_number > 20) {
		set_error(error, YT_INVALID, "Xannor target finish", "YTDATA.DAT");
		return false;
	}
	if (!reached_target)
		return true;
	if (yt_maintenance_xannor_should_attack_hunt_player(group_number,
	    hunt_player)) {
		if (!yt_maintenance_xannor_player_arrival(game, player_sector,
		    player_cloak, cache_count, hunt_player, group_size, line_output,
		    line_context, error))
			return false;
	}
	if (*group_size <= 0.0f)
		*group_location = 0.0f;
	return yt_maintenance_xannor_group_twenty_finish(game, group_number,
	    *group_location, *group_size, error);
}

bool
yt_maintenance_xannor_roaming_groups(struct maint_state *state, float score,
    int top_target, int hunt_player, int revenge_live, int revenge_cached,
    float location[21], float size[21],
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_error *error)
{
	int group = 2;

	for (;;) {
		bool skip_group;
		bool retarget;

		if (!yt_maintenance_xannor_roaming_split(
		    &state->game.random, group, &size[1], &size[group],
		    &location[group], score, state->game.config.headquarters,
		    &skip_group, error))
			return false;
		if (!skip_group) {
				do {
				int target;
				struct yt_maintenance_output_result group_output;
				bool reached_target;

				if (!xannor_candidate_target(state,
				    location[group], revenge_live, revenge_cached,
				    &target, error))
					return false;
				if (!yt_maintenance_xannor_target_override(group,
				    target, size[1], score,
				    (int)state->game.config.headquarters,
				    revenge_live, top_target, &target, error))
					return false;
				if (!yt_maintenance_compose_xannor_group(group,
				    size[group], &group_output))
					return false;
				if (!maintenance_emit_output_row(&group_output,
				    YT_MAINT_ROW_XANNOR_GROUP_REPORT, line_output,
				    line_context, error))
					return false;
				if (!yt_maintenance_xannor_route_arrivals(state, group,
				    target, (float)top_target, location, size,
				    line_output, line_context, &reached_target, error))
					return false;
				if (!yt_maintenance_xannor_target_finish(&state->game,
				    state->player_sector, state->player_cloak,
				    (size_t)state->player_count + 2U,
				    reached_target, group, hunt_player,
				    &location[group], &size[group], line_output,
				    line_context, error))
					return false;
				retarget = yt_maintenance_xannor_should_retarget(
				    location[group], size[group]);
			} while (retarget);
		}
		if (!yt_maintenance_xannor_advance_group(group, &group))
			break;
	}
	return yt_maintenance_xannor_groups_persist(&state->game, location,
	    size, error);
}

bool
yt_maintenance_xannor_run(struct maint_state *state,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_error *error)
{
	struct yt_maintenance_output_result regen_output;
	struct yt_maintenance_output_result roaming_output;
	float location[21];
	float size[21];
	double regeneration;
	float score;
	int top_target;
	int hunt_player;
	int revenge_live;
	int revenge_cached;

	if (!yt_maintenance_maintain_xannor_home(&state->game,
	    NULL, 0U, line_output, line_context, error))
		return false;
	if (!yt_maintenance_xannor_hunt(&state->game, state->player_sector,
	    state->player_cloak, (size_t)state->player_count + 2U,
	    NULL, 0U,
	    line_output, line_context, &hunt_player, &score, &top_target,
	    error))
		return false;
	if (!yt_maintenance_xannor_target(&state->game.random,
	    state->sector_count, &hunt_player, &top_target, error))
		return false;
	if (!yt_maintenance_xannor_groups_extract(&state->game, location,
	    size, error))
		return false;
	if (!yt_maintenance_xannor_regeneration(score, size, &regeneration))
		return false;
	if (!yt_maintenance_compose_xannor_regeneration(
	    NULL, 0U, regeneration, &regen_output))
		return false;
	if (!line_output(line_context, regen_output.rows[0].data,
	    regen_output.rows[0].length, error))
		return false;
	if (!line_output(line_context, regen_output.rows[1].data,
	    regen_output.rows[1].length, error))
		return false;
	if (!yt_news_append_bytes(regen_output.rows[1].data,
	    regen_output.rows[1].length, error))
		return false;
	if (!line_output(line_context, regen_output.rows[2].data,
	    regen_output.rows[2].length, error))
		return false;
	location[1] = state->game.config.headquarters;
	if (!xannor_reclaim_and_relocate(state, location, size,
	    regeneration, line_output, line_context, error))
		return false;
	if (!consume_revenge_slot(state, &revenge_live, &revenge_cached,
	    line_output, line_context, error))
		return false;
	if (!yt_maintenance_compose_xannor_roaming(
	    NULL, 0U, &roaming_output))
		return false;
	if (!line_output(line_context, roaming_output.rows[0].data,
	    roaming_output.rows[0].length, error))
		return false;
	if (!line_output(line_context, roaming_output.rows[1].data,
	    roaming_output.rows[1].length, error))
		return false;

	return yt_maintenance_xannor_roaming_groups(state, score, top_target,
	    hunt_player,
	    revenge_live, revenge_cached, location, size,
	    line_output, line_context, error);
}
