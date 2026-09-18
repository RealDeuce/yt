#include "yt_maint.h"

#include "yt_maint_internal.h"

#include "qb.h"

#include <errno.h>
#include <math.h>
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
mercenary_destination_attack_line(double moving, double defenders,
    const uint8_t *owner, size_t owner_length,
    uint8_t line[YT_MAINTENANCE_OUTPUT_ROW_SIZE], size_t *line_length)
{
	static const uint8_t prefix[] = " ***";
	static const uint8_t attack[] = " Mercenaries attacking";
	static const uint8_t belonging[] = " fighters beloning to ";
	char first[64];
	char second[64];
	int first_length = qb_str_double(first, sizeof(first), moving);
	int second_length = qb_str_double(second, sizeof(second), defenders);

	*line_length = 0U;
	if (first_length < 0 || second_length < 0)
		return false;
	if (!maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, prefix, sizeof(prefix) - 1U))
		return false;
	if (!maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, (const uint8_t *)first, (size_t)first_length))
		return false;
	if (!maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, attack, sizeof(attack) - 1U))
		return false;
	if (!maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, (const uint8_t *)second, (size_t)second_length))
		return false;
	if (!maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, belonging, sizeof(belonging) - 1U))
		return false;
	if (!maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, owner, owner_length))
		return false;
	return maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, (const uint8_t *)"!", 1U);
}

static bool
mercenary_destination_join_lines(double moving, int sector_number,
    const uint8_t *owner, size_t owner_length,
    uint8_t line[YT_MAINTENANCE_OUTPUT_ROW_SIZE], size_t *line_length,
    uint8_t radio[YT_MAINTENANCE_OUTPUT_ROW_SIZE], size_t *radio_length)
{
	static const uint8_t prefix[] = " ***";
	static const uint8_t joined[] = " Mercenaries joined ";
	static const uint8_t defense[] = "'s Defense force in Sector";
	static const uint8_t radio_middle[] =
	    " *** of our boys joined your defense force in Sector";
	char count[64];
	char sector[64];
	int count_length = qb_str_double(count, sizeof(count), moving);
	int sector_length = qb_str_single(sector, sizeof(sector),
	    (float)sector_number);

	*line_length = 0U;
	*radio_length = 0U;
	if (count_length < 0 || sector_length < 0)
		return false;
	if (!maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, prefix, sizeof(prefix) - 1U))
		return false;
	if (!maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, (const uint8_t *)count, (size_t)count_length))
		return false;
	if (!maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, joined, sizeof(joined) - 1U))
		return false;
	if (!maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, owner, owner_length))
		return false;
	if (!maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, defense, sizeof(defense) - 1U))
		return false;
	if (!maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, (const uint8_t *)sector, (size_t)sector_length))
		return false;
	if (!maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, (const uint8_t *)"!", 1U))
		return false;
	if (!maintenance_copy_part(radio, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    radio_length, (const uint8_t *)count, (size_t)count_length))
		return false;
	if (!maintenance_copy_part(radio, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    radio_length, radio_middle, sizeof(radio_middle) - 1U))
		return false;
	if (!maintenance_copy_part(radio, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    radio_length, (const uint8_t *)sector, (size_t)sector_length))
		return false;
	return maintenance_copy_part(radio, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    radio_length, (const uint8_t *)"!", 1U);
}

bool
yt_maintenance_mercenary_destination(struct yt_game *game,
    int sector_number, float moving_fighters,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_sector *arrival_sector, float *moving_after,
    bool *continues, struct yt_error *error)
{
	static const uint8_t xannor[] = "The Xannor";
	static const uint8_t won[] = " *** The Mercenaries Won!";
	static const uint8_t lost[] = " *** The Mercenaries Lost!";
	struct yt_sector fresh;
	struct yt_player owner_player;
	uint8_t owner_name[YT_TEXT_FIELD_SIZE];
	uint8_t line[YT_MAINTENANCE_OUTPUT_ROW_SIZE];
	uint8_t radio[YT_MAINTENANCE_OUTPUT_ROW_SIZE];
	size_t owner_length = 0U;
	size_t line_length;
	size_t radio_length;
	double defenders;
	double moving;
	int original_owner;
	int owner_record = 0;

	if (game == NULL || sector_number < 1 || line_output == NULL
	    || arrival_sector == NULL || moving_after == NULL
	    || continues == NULL) {
		set_error(error, YT_INVALID, "Mercenary destination", "YTDATA.DAT");
		return false;
	}
	*moving_after = moving_fighters;
	*continues = false;
	original_owner = arrival_sector->fighter_owner;
	moving = (double)moving_fighters;
	if (original_owner == -2 || original_owner == 0) {
		if (!yt_game_read_sector(game, sector_number, &fresh, error))
			return false;
		fresh.fighters = (float)((double)fresh.fighters + moving);
		fresh.fighter_owner = -2;
		if (!yt_game_write_sector(game, sector_number, &fresh, error))
			return false;
		*arrival_sector = fresh;
		*moving_after = fresh.fighters;
		*continues = true;
		return true;
	}
	if (original_owner == -1) {
		memcpy(owner_name, xannor, sizeof(xannor) - 1U);
		owner_length = sizeof(xannor) - 1U;
	}
	else {
		owner_record = original_owner;
		if (!yt_game_read_player(game, owner_record, &owner_player,
		    error))
			return false;
		owner_length = owner_player.name_length < YT_TEXT_FIELD_SIZE
		    ? owner_player.name_length : YT_TEXT_FIELD_SIZE;
		memcpy(owner_name, owner_player.record.bytes, owner_length);
	}
	{
		float selector;
		bool attack;

		if (!yt_random_next(&game->random, &selector, error))
			return false;
		attack = yt_maintenance_mercenary_attacks(original_owner, selector);
		if (!attack) {
			if (!mercenary_destination_join_lines(moving, sector_number,
			    owner_name, owner_length, line, &line_length, radio,
			    &radio_length)) {
				set_error(error, YT_RANGE,
				    "compose Mercenary join", "YTDATA.DAT");
				return false;
			}
			if (!line_output(line_context, line, line_length, error))
				return false;
			if (!yt_news_append_bytes(line, line_length, error))
				return false;
			if (!yt_radio_append_maintenance_bytes(radio, radio_length,
			    -2.0f, (float)original_owner, error))
				return false;
			if (!yt_game_read_sector(game, sector_number, &fresh, error))
				return false;
			fresh.fighters = (float)((double)fresh.fighters + moving);
			if (!yt_game_write_sector(game, sector_number, &fresh, error))
				return false;
			*arrival_sector = fresh;
			*moving_after = 0.0f;
			return true;
		}
	}
	defenders = (double)arrival_sector->fighters;
	if (!mercenary_destination_attack_line(moving, defenders, owner_name,
	    owner_length, line, &line_length)) {
		set_error(error, YT_RANGE, "compose Mercenary attack", "YTDATA.DAT");
		return false;
	}
	if (!line_output(line_context, line, line_length, error))
		return false;
	if (!yt_news_append_bytes(line, line_length, error))
		return false;
	while (defenders > 0.0 && moving > 0.0) {
		float sample;
		double quantum = defenders > 200.0 && moving > 200.0
		    ? 150.0 : 1.0;

		if (!yt_random_next(&game->random, &sample, error))
			return false;
		if (sample < 0.5f)
			defenders -= quantum;
		else
			moving -= quantum;
	}
	{
		const uint8_t *result_line = moving > 0.0 ? won : lost;
		size_t result_length = moving > 0.0
		    ? sizeof(won) - 1U : sizeof(lost) - 1U;

		if (!line_output(line_context, result_line, result_length, error))
			return false;
		if (!yt_news_append_bytes(result_line, result_length, error))
			return false;
		if (!yt_game_read_sector(game, sector_number, &fresh, error))
			return false;
	}
	if (defenders > 0.0) {
		fresh.fighters = (float)defenders;
		fresh.fighter_owner = original_owner;
	}
	else {
		fresh.fighters = 0.0f;
		fresh.fighter_owner = 0;
	}
	if (!yt_game_write_sector(game, sector_number, &fresh, error))
		return false;
	if (moving > 0.0) {
		if (!yt_game_read_sector(game, sector_number, &fresh, error))
			return false;
		fresh.fighters = (float)((double)fresh.fighters + moving);
		fresh.fighter_owner = -2;
		if (!yt_game_write_sector(game, sector_number, &fresh, error))
			return false;
	}
	*arrival_sector = fresh;
	*moving_after = (float)moving;
	*continues = moving > 0.0;
	return true;
}

enum mercenary_arrival_result {
	MERCENARY_ARRIVAL_TERMINAL,
	MERCENARY_ARRIVAL_CONTINUE
};

static bool
mercenary_arrival(struct yt_game *game, int sector_number,
    int selected_destination, float *moving,
    yt_maintenance_score_line_fn line_output, void *line_context,
    enum mercenary_arrival_result *arrival_result, struct yt_error *error)
{
	struct yt_sector sector;
	bool absorbed;

	if (moving == NULL || arrival_result == NULL) {
		set_error(error, YT_INVALID, "Mercenary routed arrival",
		    "YTDATA.DAT");
		return false;
	}
	if (!yt_maintenance_mercenary_mines(game, sector_number,
	    moving, line_output, line_context, &sector, error))
		return false;
	if (*moving == 0.0f) {
		*arrival_result = MERCENARY_ARRIVAL_TERMINAL;
		return true;
	}
	if (!yt_maintenance_mercenary_planet_absorption(game,
	    sector_number, selected_destination, (double)*moving,
	    line_output, line_context, &sector, &absorbed, error))
		return false;
	if (absorbed) {
		*moving = 0.0f;
		*arrival_result = MERCENARY_ARRIVAL_TERMINAL;
		return true;
	}
	if (*moving <= 0.0f || sector.planet != 0) {
		*arrival_result = MERCENARY_ARRIVAL_CONTINUE;
		return true;
	}
	{
		bool continues;

		if (!yt_maintenance_mercenary_destination(game, sector_number,
		    *moving,
		    line_output, line_context, &sector, moving, &continues, error))
			return false;
		*arrival_result = continues ? MERCENARY_ARRIVAL_CONTINUE
		    : MERCENARY_ARRIVAL_TERMINAL;
	}
	return true;
}

bool
yt_maintenance_move_mercenaries(struct yt_game *game, int sector_count,
    struct yt_maintenance_route_cache *route_cache,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_error *error)
{
	int origin;

	for (origin = sector_count; origin >= 1; --origin) {
		struct yt_sector sector;
		struct yt_maintenance_output_result output;
		int target;
		int next;
		int cursor;
		int hops;
		float moving;

		if (!yt_game_read_sector(game, origin, &sector, error))
			return false;
		if (sector.fighter_owner != -2 || sector.fighters <= 0.0f)
			continue;
		{
			float hold;

			if (!yt_random_next(&game->random, &hold, error))
				return false;
			if (yt_maintenance_mercenary_stays(sector.planet, hold))
				continue;
		}
		moving = sector.fighters;
		sector.fighters = 0.0f;
		sector.fighter_owner = 0;
		if (!yt_game_write_sector(game, origin, &sector, error))
			return false;
		do {
			if (!yt_random_integer(&game->random,
			    sector_count, &target, error))
				return false;
		} while (target == origin);
		if (!yt_maintenance_route_next_hop(game,
		    route_cache, origin, target, &next, error))
			return false;
		if (!yt_maintenance_compose_mercenary_movement((double)moving,
		    (float)origin, &output))
			return false;
		if (!maintenance_emit_output_row(&output,
		    YT_MAINT_ROW_MERCENARY_MOVEMENT,
		    line_output, line_context, error))
			return false;
		if (route_cache->successors == NULL) {
			set_error(error, YT_INVALID, "Mercenary route workspace",
			    "YTDATA.DAT");
			return false;
		}
		cursor = origin;
		for (hops = 0; hops <= sector_count; ++hops) {
			enum mercenary_arrival_result arrival_result;

			next = route_cache->successors[cursor];
			if (next == 0) {
				if (moving > 0.0f) {
					struct yt_sector destination;

					if (!yt_game_read_sector(game, target,
					    &destination, error))
						return false;
					destination.fighters = moving;
					destination.fighter_owner = -2;
					if (!yt_game_write_sector(game, target,
					    &destination, error))
						return false;
				}
				break;
			}
			if (next < 1 || next > sector_count) {
				set_error(error, YT_RANGE,
				    "Mercenary route successor", "YTDATA.DAT");
				return false;
			}
			cursor = next;
			if (!mercenary_arrival(game, cursor, target, &moving,
			    line_output, line_context, &arrival_result, error))
				return false;
			if (arrival_result == MERCENARY_ARRIVAL_TERMINAL)
				break;
		}
		if (hops > sector_count) {
			set_error(error, YT_RANGE, "Mercenary route cycle",
			    "YTDATA.DAT");
			return false;
		}
	}
	return true;
}
