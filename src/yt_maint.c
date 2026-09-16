#include "yt_maint.h"

#include "yt_maint_internal.h"

#include "qb.h"
#include "yt_names.h"
#include "yt_score.h"
#include "yt_text.h"

#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static float xannor_quantum(float first, float second);
static bool maintenance_stdout_line(void *context, const uint8_t *line,
    size_t length, struct yt_error *error);
static bool maintenance_stdout_semi(void *context, const uint8_t *text,
    size_t length, struct yt_error *error);
static bool maintenance_emit_output_row(
    const struct yt_maintenance_output_result *output, uint16_t address,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_error *error);
static void set_error(struct yt_error *error, enum yt_status status,
    const char *operation, const char *path);

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

bool
yt_maintenance_age_player(float cloak, float last_active,
    float killer_status, float today, float retention_days,
    struct yt_maintenance_player_aging_result *result)
{
	static const float cloak_charge = -0.05000000074505806f;
	float working;

	if (result == NULL)
		return false;
	memset(result, 0, sizeof(*result));
	working = cloak;
	if (working < 0.0f)
		working = 1.0f;
	result->cached_cloak = working;
	result->persisted_cloak = working;
	result->cloak_written = working > 0.0f;
	if (result->cloak_written) {
		working = qb_single_add(working, cloak_charge);
		if (working < 0.0f)
			working = 0.0f;
		result->persisted_cloak = working;
		result->cloak_expired = working == 0.0f;
	}
	result->cutoff = qb_single_subtract(today, retention_days);
	result->delete_player = !result->cloak_expired
	    && last_active <= result->cutoff && killer_status != 0.0f;
	return true;
}

bool
yt_maintenance_player_name(const struct yt_player *player, bool *occupied,
    struct yt_maintenance_text *name, struct yt_error *error)
{
	bool overflow;
	int32_t stored_length;
	float raw_length;

	if (player == NULL || occupied == NULL || name == NULL) {
		set_error(error, YT_INVALID, "maintenance player name",
		    "YTDATA.DAT");
		return false;
	}
	name->data = player->record.bytes;
	name->length = 0U;
	raw_length = qb_mbf32_decode(player->record.bytes + YT_F85);
	*occupied = raw_length != 0.0f;
	if (!*occupied)
		return true;
	stored_length = qb_cint_mbf32(player->record.bytes + YT_F85, 0U,
	    &overflow);
	if (overflow || stored_length < 0) {
		set_error(error, YT_RANGE, "maintenance player name",
		    "YTDATA.DAT");
		return false;
	}
	name->length = (size_t)stored_length < YT_TEXT_FIELD_SIZE
	    ? (size_t)stored_length : YT_TEXT_FIELD_SIZE;
	return true;
}

/* RANDOMIZE changes the original runtime state but adds no RND callsite. */
bool
yt_maintenance_random_integer(struct yt_random *random, int range, int *value,
    struct yt_error *error)
{
	return yt_random_integer(random, range, value, error);
}

bool
yt_maintenance_nested_integer(struct yt_random *random, int count, int range,
    int *value, struct yt_error *error)
{
	return yt_random_nested_integer(random, count, range, value, error);
}

bool
yt_maintenance_xannor_roaming_split(struct yt_random *random,
    int group_number, float *group_one, float *group_size,
    float *group_location, float top_score, float headquarters,
    struct yt_maintenance_xannor_split_result *result,
    struct yt_error *error)
{
	struct yt_maintenance_xannor_split_result local;
	uint64_t starting_draws;
	bool overflow;
	int range;
	int split;

	if (random == NULL || group_one == NULL || group_size == NULL
	    || group_location == NULL || result == NULL
	    || group_number < 2 || group_number > 20
	    || *group_one < 0.0f || *group_size < 0.0f
	    || top_score < 0.0f) {
		set_error(error, YT_INVALID, "Xannor roaming split", "");
		return false;
	}
	memset(&local, 0, sizeof(local));
	local.group_one_after = *group_one;
	local.group_size_after = *group_size;
	local.group_location_after = *group_location;
	if (*group_size >= 1.0f && *group_location >= 1.0f) {
		*result = local;
		return true;
	}
	if (*group_one < qb_single_divide(top_score, 2000.0f)) {
		local.skip_group = true;
		*result = local;
		return true;
	}
	local.split = true;
	starting_draws = random->draws;
	if (*group_one == 0.0f) {
		split = 0;
	}
	else {
		range = qb_cint(*group_one, &overflow);
		if (overflow || range < 1
		    || !yt_maintenance_nested_integer(random, 4, range, &split,
		    error)) {
			if (error != NULL && error->status == YT_OK)
				set_error(error, YT_RANGE,
				    "Xannor roaming split range", "");
			return false;
		}
	}
	*group_size = (float)split;
	*group_one = qb_single_subtract(*group_one, *group_size);
	*group_location = headquarters;
	local.group_one_after = *group_one;
	local.group_size_after = *group_size;
	local.group_location_after = *group_location;
	local.draws_consumed = random->draws - starting_draws;
	*result = local;
	return true;
}

bool
yt_maintenance_xannor_candidate_discovery(struct yt_game *game,
    const float *player_sector, const float *player_cloak,
    size_t cache_count, int current_sector, int revenge_live_sector,
    int revenge_cached_target,
    struct yt_maintenance_xannor_discovery_result *result,
    struct yt_error *error)
{
	struct yt_maintenance_xannor_discovery_result local = {0};
	uint64_t starting_draws;
	int sector_count;
	int attempt_limit;
	int attempt;

	if (game == NULL || player_sector == NULL || player_cloak == NULL
	    || result == NULL || cache_count <= 2U
	    || (revenge_live_sector == 0 && revenge_cached_target != 0)) {
		set_error(error, YT_INVALID, "Xannor candidate discovery", "");
		return false;
	}
	sector_count = (int)(game->config.port_offset
	    - game->config.sector_offset);
	if (sector_count < 2 || current_sector < 1
	    || current_sector > sector_count) {
		set_error(error, YT_RANGE, "Xannor candidate discovery", "");
		return false;
	}
	starting_draws = game->random.draws;
	do {
		if (!yt_maintenance_random_integer(&game->random, sector_count,
		    &local.initial_target, error))
			return false;
		++local.initial_draws;
	} while (local.initial_target == current_sector);
	local.discovery_target = revenge_cached_target;
	attempt_limit = revenge_live_sector != 0 ? 25 : 1;
	for (attempt = 0; attempt < attempt_limit; ++attempt) {
		struct yt_sector sector;
		int candidate;
		size_t player;

		if (!yt_maintenance_random_integer(&game->random, sector_count,
		    &candidate, error)
		    || !yt_game_read_sector(game, candidate, &sector, error))
			return false;
		++local.attempts;
		if ((sector.fighters > 1.0f
		    && sector.fighter_owner != -1.0f)
		    || sector.planet > 1.0f)
			local.discovery_target = candidate;
		if (local.discovery_target == 0) {
			for (player = 2U; player < cache_count; ++player) {
				float cloak_draw;

				if (!yt_random_next(&game->random, &cloak_draw, error))
					return false;
				++local.player_draws;
				if (player_sector[player] == (float)candidate
				    && (cloak_draw > player_cloak[player]
				    || revenge_live_sector != 0)) {
					local.discovery_target =
					    (int)player_sector[player];
					local.selected_player_record = (int)player;
					break;
				}
			}
		}
		if (local.discovery_target != 0)
			break;
	}
	local.target_sector = local.initial_target;
	if (local.discovery_target > 7)
		local.target_sector = local.discovery_target;
	local.draws_consumed = game->random.draws - starting_draws;
	*result = local;
	return true;
}

bool
yt_maintenance_xannor_target_override(int group_number,
    int discovered_target, float group_one, float top_score,
    int headquarters, int revenge_live_sector, int top_player_target,
    int *target, struct yt_error *error)
{
	bool top_override;

	if (target == NULL || group_number < 2 || group_number > 20) {
		set_error(error, YT_INVALID, "Xannor target override", "");
		return false;
	}
	top_override = (revenge_live_sector != 0 && group_number > 15
	    && top_player_target != 0) || group_number == 20;
	*target = top_override ? top_player_target : discovered_target;
	if (group_one < qb_single_divide(top_score, 2000.0f))
		*target = headquarters;
	return true;
}

bool
yt_maintenance_xannor_should_retarget(float group_location,
    float group_size)
{
	return group_location > 0.0f && group_location < 8.0f
	    && group_size > 0.0f;
}

bool
yt_maintenance_xannor_route_complete(float group_location,
    int target_sector)
{
	return group_location == (float)target_sector;
}

bool
yt_maintenance_xannor_bypass_initial_arrival(int group_number,
    float group_location, float top_player_target)
{
	return group_number == 20 && group_location == top_player_target;
}

bool
yt_maintenance_xannor_should_attack_hunt_player(int group_number,
    int hunt_player)
{
	return hunt_player != 0 && group_number == 20;
}

bool
yt_maintenance_xannor_advance_group(int group_number, int *next_group)
{
	if (next_group == NULL || group_number < 2 || group_number > 20)
		return false;
	*next_group = group_number + 1;
	return *next_group <= 20;
}

bool
yt_maintenance_xannor_post_planet_exhausted(float group_location,
    float group_size)
{
	return group_size < 1.0f || group_location == 0.0f;
}

bool
yt_maintenance_xannor_player_scan_admit(int group_number,
    float group_location, float player_location, float cached_cloak,
    float cloak_draw)
{
	float threshold = qb_single_subtract(cached_cloak, 0.33000001311302185f);

	return player_location == group_location && group_number != 20
	    && threshold <= cloak_draw;
}

bool
yt_maintenance_xannor_player_scan_continue(int player_record,
    int player_count)
{
	return player_record >= 2 && player_record <= player_count + 1;
}

bool
yt_maintenance_xannor_target(struct yt_random *random, int sector_count,
    int hunt_player, int target_sector,
    struct yt_maintenance_xannor_target_result *result,
    struct yt_error *error)
{
	struct yt_maintenance_xannor_target_result local = {
		hunt_player, target_sector, false, 0U
	};
	uint64_t starting_draws;
	int selected;

	if (random == NULL || result == NULL || sector_count < 8) {
		set_error(error, YT_INVALID, "Xannor target", "YTDATA.DAT");
		return false;
	}
	starting_draws = random->draws;
	if (target_sector < 8 || target_sector > sector_count
	    || hunt_player == 0) {
		if (!yt_maintenance_random_integer(random, sector_count - 7,
		    &selected, error))
			return false;
		local.hunt_player = 0;
		local.target_sector = selected + 7;
		local.replaced = true;
	}
	local.draws_consumed = random->draws - starting_draws;
	*result = local;
	return true;
}

bool
yt_maintenance_xannor_defense(struct yt_random *random, float *group_size,
    float *defense_fighters, float *defense_owner, struct yt_error *error)
{
	float original_xannor;
	float original_defenders;
	float xloss = 0.0f;
	float dloss = 0.0f;

	if (random == NULL || group_size == NULL || defense_fighters == NULL
	    || defense_owner == NULL) {
		set_error(error, YT_INVALID, "Xannor defense combat", "");
		return false;
	}
	if (*group_size <= 0.0f || *defense_fighters < 1.0f
	    || *defense_owner == -1.0f || *defense_owner == 0.0f)
		return true;
	original_xannor = *group_size;
	original_defenders = *defense_fighters;
	while (dloss < original_defenders && xloss < original_xannor) {
		float sample;
		float quantum = xannor_quantum(original_defenders - dloss,
		    original_xannor - xloss);

		if (!yt_random_next(random, &sample, error))
			return false;
		if (sample > 0.5f)
			xloss = qb_single_add(xloss, quantum);
		else
			dloss = qb_single_add(dloss, quantum);
	}
	xloss = fminf(xloss, original_xannor);
	dloss = fminf(dloss, original_defenders);
	*group_size = qb_single_subtract(original_xannor, xloss);
	*defense_fighters = qb_single_subtract(original_defenders, dloss);
	if (*defense_fighters <= 0.0f) {
		*defense_fighters = 0.0f;
		*defense_owner = 0.0f;
	}
	return true;
}

bool
yt_maintenance_mercenary_stays(float planet_link, float draw)
{
	return planet_link > 0.0f && draw < 0.6600000262260437f;
}

bool
yt_maintenance_mercenary_attacks(float defense_owner, float draw)
{
	return defense_owner < 0.0f
	    || draw > 0.949999988079071f;
}

static bool
xannor_player_fighter_phase(struct yt_random *random,
    float *player_fighters, float original_xannor,
    struct yt_maintenance_xannor_player_result *result,
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
	result->player_fighter_losses = player_losses;
	result->xannor_losses = xannor_losses;
	return true;
}

static bool
xannor_player_shield_phase(struct yt_random *random, float player_fighters,
    float *player_shields, float original_xannor,
    struct yt_maintenance_xannor_player_result *result,
    struct yt_error *error)
{
	float xannor_losses = result->xannor_losses;

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
	result->xannor_losses = fminf(xannor_losses, original_xannor);
	return true;
}

bool
yt_maintenance_xannor_player_combat(struct yt_random *random,
    float *player_fighters, float *player_shields, float *xannor_fighters,
    struct yt_maintenance_xannor_player_result *result,
    struct yt_error *error)
{
	float original_xannor;

	if (random == NULL || player_fighters == NULL || player_shields == NULL
	    || xannor_fighters == NULL || result == NULL) {
		set_error(error, YT_INVALID, "Xannor player combat", "");
		return false;
	}
	original_xannor = *xannor_fighters;
	result->player_fighter_losses = 0.0f;
	result->xannor_losses = 0.0f;
	if (!xannor_player_fighter_phase(random, player_fighters,
	    original_xannor, result, error)
	    || !xannor_player_shield_phase(random, *player_fighters,
	    player_shields, original_xannor, result, error))
		return false;
	if (*player_fighters < 0.0f)
		*player_fighters = 0.0f;
	*xannor_fighters = qb_single_subtract(original_xannor, result->xannor_losses);
	return true;
}

bool
yt_maintenance_xannor_player_line_bytes(const uint8_t *player_name,
    size_t player_name_length,
    const struct yt_maintenance_xannor_player_result *result,
    float xannor_fighters, float player_shields, bool player_killed,
    uint8_t *line, size_t line_size, size_t *line_length)
{
	static const uint8_t prefix[] = " *** ";
	static const uint8_t lost[] = ": lost";
	static const uint8_t destroyed[] = ", dstrd";
	static const uint8_t xannor_lost[] = " (Xannor Lost) - Shields:";
	static const uint8_t player_lost[] = " (Player Killed)";
	char player_losses[48];
	char xannor_losses[48];
	char shields[48];
	size_t length = 0U;
	int player_length;
	int xannor_length;
	int shields_length;

	if ((player_name == NULL && player_name_length != 0U)
	    || result == NULL || line == NULL || line_length == NULL)
		return false;
	*line_length = 0U;
	player_length = qb_str_double(player_losses, sizeof(player_losses),
	    (double)result->player_fighter_losses);
	xannor_length = qb_str_double(xannor_losses, sizeof(xannor_losses),
	    (double)result->xannor_losses);
	shields_length = qb_str_double(shields, sizeof(shields),
	    (double)player_shields);
	if (player_length < 0 || xannor_length < 0 || shields_length < 0
	    || !maintenance_copy_part(line, line_size, &length,
	    prefix, sizeof(prefix) - 1U)
	    || !maintenance_copy_part(line, line_size, &length,
	    player_name, player_name_length)
	    || !maintenance_copy_part(line, line_size, &length,
	    lost, sizeof(lost) - 1U)
	    || !maintenance_copy_part(line, line_size, &length,
	    (const uint8_t *)player_losses, (size_t)player_length)
	    || !maintenance_copy_part(line, line_size, &length,
	    destroyed, sizeof(destroyed) - 1U)
	    || !maintenance_copy_part(line, line_size, &length,
	    (const uint8_t *)xannor_losses, (size_t)xannor_length))
		return false;
	if (xannor_fighters < 1.0f) {
		if (!maintenance_copy_part(line, line_size, &length,
		    xannor_lost, sizeof(xannor_lost) - 1U)
		    || !maintenance_copy_part(line, line_size, &length,
		    (const uint8_t *)shields, (size_t)shields_length))
			return false;
	}
	else if (player_killed && !maintenance_copy_part(line, line_size,
	    &length, player_lost, sizeof(player_lost) - 1U))
		return false;
	*line_length = length;
	return true;
}

bool
yt_maintenance_xannor_player_line(const char *player_name,
    const struct yt_maintenance_xannor_player_result *result,
    float xannor_fighters, float player_shields, bool player_killed,
    char *line, size_t line_size)
{
	size_t length;

	if (player_name == NULL || line == NULL || line_size == 0U
	    || !yt_maintenance_xannor_player_line_bytes(
	    (const uint8_t *)player_name, strlen(player_name), result,
	    xannor_fighters, player_shields, player_killed, (uint8_t *)line,
	    line_size - 1U, &length))
		return false;
	line[length] = '\0';
	return true;
}

static bool
maintenance_write_wanderer_sector(struct yt_game *game, int logical,
    struct yt_sector *sector, struct yt_error *error)
{
	if (!yt_record_set_number(&sector->record, YT_F93, sector->planet)) {
		set_error(error, YT_RANGE, "encode Wanderer sector", "YTDATA.DAT");
		return false;
	}
	return yt_database_write(&game->database,
	    (size_t)yt_sector_basic_record(&game->config, logical),
	    &sector->record, error);
}

static bool
maintenance_write_wanderer_rebuild(struct yt_game *game,
    struct yt_planet *planet, int today, struct yt_error *error)
{
	static const size_t production_offsets[] = {YT_F45, YT_F49, YT_F53};
	static const size_t stock_offsets[] = {YT_F57, YT_F61, YT_F65};
	int index;

	yt_record_set_text(&planet->record,
	    (const uint8_t *)"The Wanderer", 12U);
	if (!yt_record_set_number(&planet->record, YT_F41,
	    (float)(today - 10)))
		goto range;
	for (index = 0; index < 3; ++index) {
		if (!yt_record_set_number(&planet->record,
		    production_offsets[index], 5000.0f)
		    || !yt_record_set_number(&planet->record,
		    stock_offsets[index], 0.0f))
			goto range;
	}
	if (!yt_record_set_number(&planet->record, YT_F69, 0.0f)
	    || !yt_record_set_number(&planet->record, YT_F73, 0.0f)
	    || !yt_record_set_number(&planet->record, YT_F77, 0.0f)
	    || !yt_record_set_number(&planet->record, YT_F85, 12.0f)
	    || !yt_record_set_number(&planet->record, YT_F117, 250000.0f)
	    || !yt_record_set_number(&planet->record, YT_F125, 0.0f))
		goto range;
	return yt_database_write(&game->database,
	    (size_t)yt_planet_basic_record(&game->config, 1),
	    &planet->record, error);

range:
	set_error(error, YT_RANGE, "encode Wanderer rebuild", "YTDATA.DAT");
	return false;
}

static bool
maintenance_write_wanderer_planet(struct yt_game *game,
    struct yt_planet *planet, struct yt_error *error)
{
	if (!yt_record_set_number(&planet->record, YT_F73, planet->owner)
	    || !yt_record_set_number(&planet->record, YT_F117, planet->bank)) {
		set_error(error, YT_RANGE, "encode Wanderer planet", "YTDATA.DAT");
		return false;
	}
	return yt_database_write(&game->database,
	    (size_t)yt_planet_basic_record(&game->config, 1),
	    &planet->record, error);
}

bool
yt_maintenance_maintain_wanderer(struct yt_game *game,
    const uint8_t *blank, size_t blank_length,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_maintenance_wanderer_result *result, struct yt_error *error)
{
	struct yt_maintenance_output_result output;
	struct yt_maintenance_wanderer_result local = {0};
	struct yt_sector sector;
	struct yt_planet planet;
	uint64_t starting_draws;
	int sector_count;
	int logical;
	int today;
	size_t row;

	if (game == NULL || line_output == NULL
	    || (blank == NULL && blank_length != 0U)) {
		set_error(error, YT_INVALID, "maintain Wanderer", "YTDATA.DAT");
		return false;
	}
	sector_count = (int)(game->config.port_offset
	    - game->config.sector_offset);
	if (sector_count < 1
	    || game->config.total_records - game->config.planet_offset < 1.0f
	    || !yt_maintenance_compose_wanderer_phase(blank,
	    blank_length, false, &output)) {
		set_error(error, YT_RANGE, "maintain Wanderer", "YTDATA.DAT");
		return false;
	}
	if (result != NULL)
		memset(result, 0, sizeof(*result));
	for (row = 0U; row < 2U; ++row) {
		if (!line_output(line_context, output.rows[row].data,
		    output.rows[row].length, error))
			return false;
	}
	for (logical = 1; logical <= sector_count; ++logical) {
		if (!yt_game_read_sector(game, logical, &sector, error))
			return false;
		++local.scanned_sectors;
		if (sector.planet == 1.0f) {
			local.removed_sector = logical;
			sector.planet = 0.0f;
			if (!maintenance_write_wanderer_sector(game, logical,
			    &sector, error))
				return false;
			break;
		}
	}
	local.rebuilt = local.removed_sector == 0;
	if (local.rebuilt) {
		if (!yt_current_date_serial(game->config.epoch_year, &today, NULL,
		    error)
		    || !yt_maintenance_compose_wanderer_phase(blank,
		    blank_length, true, &output))
			return false;
		if (!line_output(line_context, output.rows[2].data,
		    output.rows[2].length, error)
		    || !yt_news_append_bytes(output.rows[2].data,
		    output.rows[2].length, error)
		    || !yt_game_read_planet(game, 1, &planet, error)
		    || !maintenance_write_wanderer_rebuild(game, &planet, today,
		    error)
		    || !line_output(line_context, output.rows[3].data,
		    output.rows[3].length, error)
		    || !yt_news_append_bytes(output.rows[3].data,
		    output.rows[3].length, error))
			return false;
	}
	for (row = output.row_count - 2U; row < output.row_count; ++row) {
		if (!line_output(line_context, output.rows[row].data,
		    output.rows[row].length, error))
			return false;
	}
	starting_draws = game->random.draws;
	for (;;) {
		if (!yt_maintenance_random_integer(&game->random, sector_count,
		    &logical, error))
			return false;
		++local.candidate_attempts;
		if (!yt_game_read_sector(game, logical, &sector, error))
			return false;
		if (sector.planet == 0.0f)
			break;
	}
	local.target_sector = logical;
	local.draws_consumed = game->random.draws - starting_draws;
	sector.planet = 1.0f;
	if (!maintenance_write_wanderer_sector(game, logical, &sector, error)
	    || !yt_game_read_planet(game, 1, &planet, error))
		return false;
	planet.owner = 0.0f;
	if (planet.bank == 0.0f)
		planet.bank = 250000.0f;
	local.bank_after = planet.bank;
	if (!maintenance_write_wanderer_planet(game, &planet, error))
		return false;
	if (result != NULL)
		*result = local;
	return true;
}

bool
yt_maintenance_scoreboard(struct yt_game *game,
    yt_maintenance_score_line_fn line_output, void *context,
    struct yt_error *error)
{
	struct yt_text_input input;
	const char *path;
	bool result = false;

	if (game == NULL || line_output == NULL) {
		set_error(error, YT_INVALID, "maintenance scoreboard", "");
		return false;
	}
	if (!yt_score_generate(game, error))
		return false;
	path = strcmp(game->config.scoreboard, "NUL") == 0
	    ? "YTTEMP" : game->config.scoreboard;
	yt_text_input_init(&input);
	if (!yt_text_input_open(&input, path, error))
		goto done;
	for (;;) {
		const uint8_t *line;
		size_t length;
		bool available;
		bool eof;

		if (!yt_text_input_eof(&input, &eof, error))
			goto done;
		if (eof)
			break;
		if (!yt_text_input_read_line(&input, &line, &length, &available,
		    error))
			goto done;
		if (!available)
			break;
		if (!line_output(context, line, length, error))
			goto done;
	}
	result = yt_text_input_close(&input, error);

done:
	yt_text_input_destroy(&input);
	return result;
}

static bool
maintenance_stdout_line(void *context, const uint8_t *line, size_t length,
    struct yt_error *error)
{
	(void)context;
	if ((length > 0 && fwrite(line, 1, length, stdout) != length)
	    || fputc('\n', stdout) == EOF) {
		set_error(error, YT_IO_ERROR, "write maintenance screen", "stdout");
		return false;
	}
	return true;
}

static bool
maintenance_stdout_semi(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	(void)context;
	if (length != 0U && fwrite(text, 1, length, stdout) != length) {
		set_error(error, YT_IO_ERROR, "write maintenance screen", "stdout");
		return false;
	}
	return true;
}

/*
 * Faction maintenance is kept in a separate continuation below.  These
 * declarations make the whole-run order explicit and keep record writes
 * local to the phase that owns them.
 */
static bool maintain_factions(struct maint_state *, struct yt_error *);

static bool
store_config_field(struct maint_state *state, size_t offset, float value,
    struct yt_error *error)
{
	yt_record_set_number_if_changed(&state->game.config.record, offset,
	    value);
	return yt_database_write(&state->game.database, 1,
	    &state->game.config.record, error);
}

bool
yt_maintenance_store_final_marker(struct yt_game *game, float serial,
    struct yt_error *error)
{
	struct yt_record fresh;

	if (game == NULL) {
		set_error(error, YT_INVALID, "maintenance final marker",
		    "YTDATA.DAT");
		return false;
	}
	if (!yt_database_read(&game->database, 1U, &fresh, error))
		return false;
	if (!yt_record_set_number(&fresh, YT_F81, serial)) {
		set_error(error, YT_RANGE, "encode maintenance final marker",
		    "YTDATA.DAT");
		return false;
	}
	if (!yt_database_write(&game->database, 1U, &fresh, error))
		return false;
	game->config.record = fresh;
	game->config.last_maintenance = serial;
	return true;
}

static bool
store_final_marker(struct yt_game *game, struct yt_error *error)
{
	int serial;

	if (!yt_current_date_serial(game->config.epoch_year, &serial, NULL,
	    error))
		return false;
	return yt_maintenance_store_final_marker(game, (float)serial,
	    error);
}

static bool maintenance_close_all(struct yt_game *, struct yt_error *);

static bool
maintenance_finish_impl(struct yt_game *game, int player_count,
    int planet_count, int sector_count,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_error *error)
{
	struct yt_maintenance_lottery_result lottery_result;
	struct yt_maintenance_output_result wrapper_output;

	if (game == NULL || line_output == NULL) {
		set_error(error, YT_INVALID, "maintenance finish", "YTDATA.DAT");
		return false;
	}
	if (!yt_maintenance_super_lottery(game, player_count, planet_count,
	    sector_count, NULL, 0U, line_output, line_context,
	    &lottery_result, error))
		return false;
	if (!store_final_marker(game, error))
		return false;
	if (!yt_maintenance_scoreboard(game, line_output, line_context, error)
	    || !maintenance_close_all(game, error))
		return false;
	if (!yt_maintenance_compose_wrapper(&wrapper_output)
	    || !maintenance_emit_output_row(&wrapper_output, 0x004FU,
	    line_output, line_context, error)
	    || !maintenance_emit_output_row(&wrapper_output, 0x0061U,
	    line_output, line_context, error))
		return false;
	return true;
}

bool
yt_maintenance_finish(struct yt_game *game,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_error *error)
{
	int player_count;
	int planet_count;
	int sector_count;

	if (game == NULL) {
		set_error(error, YT_INVALID, "maintenance finish", "YTDATA.DAT");
		return false;
	}
	player_count = (int)game->config.sector_offset - 1;
	sector_count = (int)(game->config.port_offset
	    - game->config.sector_offset);
	planet_count = (int)(game->config.total_records
	    - game->config.planet_offset);
	return maintenance_finish_impl(game, player_count, planet_count,
	    sector_count, line_output, line_context, error);
}

static bool
maintenance_close_all(struct yt_game *game, struct yt_error *error)
{
	return game->database.file == NULL
	    || yt_database_close_all_single(&game->database, error);
}

bool
yt_maintenance_run(struct yt_error *error)
{
	struct maint_state state;
	struct yt_maintenance_output_result entry_output;
	struct yt_maintenance_output_result compaction_output;
	bool same_day;
	bool result = false;

	memset(&state, 0, sizeof(state));
	if (!yt_game_open(&state.game, YT_OPEN_UPDATE, error))
		return false;
	/* The shipped 0244..0270 branch persists this before later defaults. */
	if (yt_maintenance_default_headquarters(
	    &state.game.config.headquarters)
	    && !store_config_field(&state, YT_F117,
	    state.game.config.headquarters, error))
		goto done;
	yt_config_normalize_maintenance(&state.game.config);
	state.player_count = (int)state.game.config.sector_offset - 1;
	state.sector_count = (int)(state.game.config.port_offset
	    - state.game.config.sector_offset);
	state.port_count = (int)(state.game.config.planet_offset
	    - state.game.config.port_offset);
	state.planet_count = (int)(state.game.config.total_records
	    - state.game.config.planet_offset);
	state.today = state.game.today;
	same_day = yt_maintenance_same_day(state.game.config.last_maintenance,
	    (float)state.today);
	if (state.player_count < 1 || state.sector_count < 7
	    || state.port_count < 1 || state.planet_count < 1) {
		set_error(error, YT_RANGE, "maintenance layout", "YTDATA.DAT");
		goto done;
	}
	state.player_sector = calloc((size_t)state.player_count + 2U,
	    sizeof(*state.player_sector));
	state.player_cloak = calloc((size_t)state.player_count + 2U,
	    sizeof(*state.player_cloak));
	if (state.player_sector == NULL || state.player_cloak == NULL) {
		set_error(error, YT_NO_MEMORY, "maintenance player cache", "");
		goto done;
	}
	if (!yt_maintenance_compose_entry(same_day, &entry_output)
	    || (same_day
	    && (!maintenance_emit_output_row(&entry_output, 0x036DU,
	    maintenance_stdout_line, NULL, error)
	    || !maintenance_emit_output_row(&entry_output, 0x037FU,
	    maintenance_stdout_line, NULL, error)))
	    || !maintenance_emit_output_row(&entry_output, 0x0399U,
	    maintenance_stdout_line, NULL, error)
	    || !maintenance_emit_output_row(&entry_output, 0x03ADU,
	    maintenance_stdout_line, NULL, error)
	    || !maintenance_emit_output_row(&entry_output, 0x03BFU,
	    maintenance_stdout_line, NULL, error)
	    || !maintenance_emit_output_row(&entry_output, 0x03D3U,
	    maintenance_stdout_line, NULL, error)
	    || !maintenance_emit_output_row(&entry_output, 0x03E5U,
	    maintenance_stdout_semi, NULL, error)
	    || !maintenance_emit_output_row(&entry_output, 0x03ECU,
	    maintenance_stdout_line, NULL, error)
	    || !maintenance_emit_output_row(&entry_output, 0x03FDU,
	    maintenance_stdout_line, NULL, error)
	    || !maintenance_emit_output_row(&entry_output, 0x040CU,
	    maintenance_stdout_line, NULL, error)
	    || !yt_maintenance_clear_protected_mines(&state.game, error)
	    || !yt_maintenance_compose_message_compaction(&compaction_output)
	    || !maintenance_emit_output_row(&compaction_output, 0x673AU,
	    maintenance_stdout_line, NULL, error)
	    || !maintenance_emit_output_row(&compaction_output, 0x674CU,
	    maintenance_stdout_line, NULL, error)
	    || !yt_radio_compact(error)
	    || !yt_news_rotate(error)
	    || !yt_maintenance_write_header(error)
	    || !maintenance_emit_output_row(&entry_output, 0x04E8U,
	    maintenance_stdout_line, NULL, error)
	    || !maintenance_emit_output_row(&entry_output, 0x04FCU,
	    maintenance_stdout_line, NULL, error)
	    || !maintenance_emit_output_row(&entry_output, 0x050DU,
	    maintenance_stdout_line, NULL, error)
	    || !yt_maintenance_players_run(&state, maintenance_stdout_line, NULL,
	    error)
	    || !yt_maintenance_maintain_ports(&state.game,
	    NULL, 0U,
	    maintenance_stdout_line, NULL, NULL, error)
	    || !yt_maintenance_maintain_planets(&state.game,
	    NULL, 0U,
	    maintenance_stdout_line, NULL, NULL, error)
	    || !yt_maintenance_maintain_wanderer(&state.game,
	    NULL, 0U,
	    maintenance_stdout_line, NULL, NULL, error)
	    || !maintain_factions(&state, error)
	    || !maintenance_finish_impl(&state.game, state.player_count,
	    state.planet_count, state.sector_count, maintenance_stdout_line,
	    NULL, error))
		goto done;
	result = true;

done:
	free(state.player_sector);
	free(state.player_cloak);
	yt_maintenance_route_cache_free(&state.route_cache);
	yt_game_close(&state.game);
	return result;
}

static bool
immediate_death_cleanup_impl(struct maint_state *state, int victim_record,
    float killer, struct yt_player *victim, struct yt_error *error)
{
	int logical;

	state->player_sector[victim_record] = 0.0f;
	state->player_cloak[victim_record] = 0.0f;
	victim->killed_by = killer;
	victim->sector = 0.0f;
	victim->ground_forces = 0.0f;
	for (logical = 1; logical <= state->port_count; ++logical) {
		struct yt_port port;

		if (!yt_game_read_port(&state->game, logical, &port, error))
			return false;
		if (port.owner == (float)victim_record) {
			port.owner = 0.0f;
			port.treasury = 0.0f;
			if (!yt_game_write_port(&state->game, logical, &port,
			    error))
				return false;
		}
	}
	for (logical = 1; logical <= state->sector_count; ++logical) {
		struct yt_sector sector;

		if (!yt_game_read_sector(&state->game, logical, &sector, error))
			return false;
		if (sector.fighter_owner == (float)victim_record) {
			sector.fighter_owner = -2.0f;
			if (!yt_game_write_sector(&state->game, logical, &sector,
			    error))
				return false;
		}
	}
	if (!yt_maintenance_remove_player_from_teams(state, victim_record, error))
		return false;
	victim->team = 0.0f;
	if (!yt_record_set_number(&victim->record, YT_F45,
	    victim->killed_by)
	    || !yt_record_set_number(&victim->record, YT_F57, victim->sector)
	    || !yt_record_set_number(&victim->record, YT_F89, victim->team)
	    || !yt_record_set_number(&victim->record, YT_F121,
	    victim->ground_forces)) {
		set_error(error, YT_RANGE, "encode immediate death player",
		    "YTDATA.DAT");
		return false;
	}
	return yt_database_write(&state->game.database,
	    (size_t)victim_record, &victim->record, error);
}

bool
yt_maintenance_immediate_death(struct yt_game *game, float *player_sector,
    float *player_cloak, size_t cache_count, int victim_record, float killer,
    struct yt_player *victim, struct yt_error *error)
{
	struct maint_state state;
	int player_count;

	if (game == NULL || player_sector == NULL || player_cloak == NULL
	    || victim == NULL) {
		set_error(error, YT_INVALID, "immediate death", "");
		return false;
	}
	player_count = (int)game->config.sector_offset - 1;
	if (victim_record < 2 || victim_record > player_count + 1
	    || (size_t)victim_record >= cache_count) {
		set_error(error, YT_RANGE, "immediate death", "YTDATA.DAT");
		return false;
	}
	memset(&state, 0, sizeof(state));
	state.game = *game;
	state.player_count = player_count;
	state.sector_count = (int)(game->config.port_offset
	    - game->config.sector_offset);
	state.port_count = (int)(game->config.planet_offset
	    - game->config.port_offset);
	state.planet_count = (int)(game->config.total_records
	    - game->config.planet_offset);
	state.player_sector = player_sector;
	state.player_cloak = player_cloak;
	return immediate_death_cleanup_impl(&state, victim_record, killer,
	    victim, error);
}

bool
yt_maintenance_xannor_groups_extract(struct yt_game *game,
    float location[21], float size[21], struct yt_error *error)
{
	int group;
	int sector_count;

	if (game == NULL || location == NULL || size == NULL) {
		set_error(error, YT_INVALID, "Xannor group extraction",
		    "YTDATA.DAT");
		return false;
	}
	sector_count = (int)(game->config.port_offset
	    - game->config.sector_offset);
	if (sector_count < 20) {
		set_error(error, YT_RANGE, "Xannor group extraction",
		    "YTDATA.DAT");
		return false;
	}
	memset(location, 0, 21U * sizeof(*location));
	memset(size, 0, 21U * sizeof(*size));
	/* 26D4..276F clears every reserved slot before extracting a host. */
	for (group = 1; group <= 20; ++group) {
		struct yt_sector metadata;

		if (!yt_game_read_sector(game, group, &metadata, error))
			return false;
		location[group] = yt_record_get_number(&metadata.record, YT_F105);
		if (!yt_record_set_number(&metadata.record, YT_F105, 0.0f)
		    || !yt_database_write(&game->database,
		    (size_t)yt_sector_basic_record(&game->config, group),
		    &metadata.record, error)) {
			if (error != NULL && error->status == YT_OK)
				set_error(error, YT_RANGE,
				    "encode Xannor group metadata", "YTDATA.DAT");
			return false;
		}
	}
	/* 2772..2861 performs the separate ascending host extraction pass. */
	for (group = 1; group <= 20; ++group) {
		struct yt_sector host;
		bool overflow;
		int32_t logical;

		if (location[group] <= 0.0f)
			continue;
		logical = qb_cint(location[group], &overflow);
		if (overflow || logical < 1 || logical > sector_count) {
			set_error(error, YT_RANGE, "Xannor group sector",
			    "YTDATA.DAT");
			return false;
		}
		if (!yt_game_read_sector(game, logical, &host, error))
			return false;
		if (host.fighter_owner == -1.0f) {
			size[group] = host.fighters;
			if (!yt_record_set_number(&host.record, YT_F81, 0.0f)
			    || !yt_record_set_number(&host.record, YT_F85, 0.0f)
			    || !yt_database_write(&game->database,
			    (size_t)yt_sector_basic_record(&game->config, logical),
			    &host.record, error)) {
				if (error != NULL && error->status == YT_OK)
					set_error(error, YT_RANGE,
					    "encode Xannor group host", "YTDATA.DAT");
				return false;
			}
		}
	}
	/* 2861..2A0A folds each later co-located slot into the earliest. */
	for (group = 1; group < 20; ++group) {
		int later;

		if (location[group] <= 0.0f)
			continue;
		for (later = group + 1; later <= 20; ++later) {
			if (location[later] == location[group]) {
				size[group] = qb_single_add(size[group], size[later]);
				size[later] = 0.0f;
				location[later] = 0.0f;
			}
		}
	}
	return true;
}

bool
yt_maintenance_xannor_regeneration(float top_score, const float size[21],
    struct yt_maintenance_xannor_regeneration_result *result)
{
	struct yt_maintenance_xannor_regeneration_result local = {0};
	volatile float converted;
	float regeneration_single;
	int group;

	if (size == NULL || result == NULL)
		return false;
	for (group = 1; group <= 20; ++group)
		local.total_before = qb_single_add(local.total_before, size[group]);
	regeneration_single = yt_maintenance_sint(
	    qb_single_divide(top_score, 500.0f));
	local.regeneration = (double)regeneration_single;
	local.ceiling = yt_maintenance_sint(
	    qb_single_divide(top_score, 100.0f));
	if (local.total_before > local.ceiling)
		local.regeneration = 0.0;
	converted = (float)((double)size[1] + local.regeneration);
	local.group_one_after = converted;
	*result = local;
	return true;
}

bool
yt_maintenance_xannor_headquarters_reclaim(struct yt_game *game,
    float location[21], float size[21], yt_maintenance_score_line_fn line_output,
    void *line_context, struct yt_maintenance_xannor_reclaim_result *result,
    struct yt_error *error)
{
	static const uint8_t mercenaries[] = "The Mercenaries";
	struct yt_maintenance_xannor_reclaim_result local = {0};
	struct yt_maintenance_output_result output;
	struct yt_maintenance_text opponent = {
		mercenaries, sizeof(mercenaries) - 1U
	};
	struct yt_sector host;
	struct yt_player player;
	uint64_t starting_draws;
	double defenders;
	bool occupied;
	bool overflow;
	int32_t hq;

	if (game == NULL || location == NULL || size == NULL
	    || line_output == NULL) {
		set_error(error, YT_INVALID, "Xannor headquarters reclaim",
		    "YTDATA.DAT");
		return false;
	}
	hq = qb_cint_mbf32(game->config.record.bytes + YT_F117, 0U,
	    &overflow);
	if (overflow || hq < 1
	    || !yt_game_read_sector(game, hq, &host, error)) {
		if (error != NULL && error->status == YT_OK)
			set_error(error, YT_RANGE, "Xannor headquarters",
			    "YTDATA.DAT");
		return false;
	}
	defenders = (double)host.fighters;
	local.original_hostile = host.fighters > 0.0f
	    && host.fighter_owner != -1.0f;
	local.attempted = local.original_hostile && size[1] > 0.0f;
	if (!local.attempted) {
		local.defenders_after = defenders;
		if (result != NULL)
			*result = local;
		return true;
	}
	if (host.fighter_owner > 0.0f) {
		int32_t record = qb_cint_mbf32(host.record.bytes + YT_F85, 0U,
		    &overflow);

		if (overflow || record < 1
		    || !yt_game_read_player(game, record, &player, error)
		    || !yt_maintenance_player_name(&player, &occupied,
		    &opponent, error)) {
			if (error != NULL && error->status == YT_OK)
				set_error(error, YT_RANGE,
				    "Xannor headquarters defender", "YTDATA.DAT");
			return false;
		}
	}
	if (!yt_maintenance_compose_xannor_reclaim_attempt(&opponent, &output)
	    || !yt_news_append_bytes(output.rows[0].data,
	    output.rows[0].length, error)
	    || !line_output(line_context, output.rows[0].data,
	    output.rows[0].length, error))
		return false;
	starting_draws = game->random.draws;
	while (defenders > 0.0 && size[1] > 0.0f) {
		float sample;
		float quantum = defenders > 250.0 && size[1] > 250.0f
		    ? 250.0f : 1.0f;

		if (!yt_random_next(&game->random, &sample, error))
			return false;
		if (sample <= 0.5f)
			defenders -= (double)quantum;
		else
			size[1] = qb_single_subtract(size[1], quantum);
	}
	local.draws_consumed = game->random.draws - starting_draws;
	local.successful = defenders <= 0.0;
	local.defenders_after = defenders;
	if (!yt_game_read_sector(game, hq, &host, error))
		return false;
	if (local.successful) {
		if (!yt_record_set_number(&host.record, YT_F81, 0.0f)
		    || !yt_record_set_number(&host.record, YT_F85, 0.0f))
			goto encode_error;
	}
	else {
		volatile float remaining = (float)defenders;

		if (!yt_record_set_number(&host.record, YT_F81, remaining))
			goto encode_error;
	}
	if (!yt_database_write(&game->database,
	    (size_t)yt_sector_basic_record(&game->config, hq),
	    &host.record, error)
	    || !yt_maintenance_compose_xannor_reclaim_result(
	    local.successful, &output)
	    || !line_output(line_context, output.rows[0].data,
	    output.rows[0].length, error)
	    || !yt_news_append_bytes(output.rows[0].data,
	    output.rows[0].length, error))
		return false;
	if (result != NULL)
		*result = local;
	return true;

encode_error:
	set_error(error, YT_RANGE, "encode Xannor headquarters", "YTDATA.DAT");
	return false;
}

bool
yt_maintenance_xannor_headquarters_relocate(struct yt_game *game,
    float location[21], bool original_hostile, float group_one,
    double regeneration, const uint8_t *blank,
    size_t blank_length, yt_maintenance_score_line_fn line_output,
    void *line_context, struct yt_maintenance_xannor_relocation_result *result,
    struct yt_error *error)
{
	struct yt_maintenance_xannor_relocation_result local = {0};
	struct yt_maintenance_output_result output;
	struct yt_record config_record;
	struct yt_sector sector;
	uint64_t starting_draws;
	float planet_number;
	bool overflow;
	int32_t old_logical;
	int sector_count;
	int candidate;

	if (game == NULL || location == NULL || line_output == NULL
	    || (blank == NULL && blank_length != 0U)) {
		set_error(error, YT_INVALID, "Xannor headquarters relocation",
		    "YTDATA.DAT");
		return false;
	}
	sector_count = (int)(game->config.port_offset
	    - game->config.sector_offset);
	if (sector_count < 8) {
		set_error(error, YT_RANGE, "Xannor headquarters relocation",
		    "YTDATA.DAT");
		return false;
	}
	local.triggered = (!original_hostile
	    && (double)group_one == regeneration)
	    || (original_hostile && group_one > 0.0f);
	if (result != NULL)
		memset(result, 0, sizeof(*result));
	if (!local.triggered) {
		if (result != NULL)
			*result = local;
		return true;
	}
	starting_draws = game->random.draws;
	for (;;) {
		if (!yt_maintenance_random_integer(&game->random,
		    sector_count - 7, &candidate, error))
			return false;
		candidate += 7;
		++local.attempts;
		if (!yt_game_read_sector(game, candidate, &sector, error))
			return false;
		if (!((sector.fighters > 1.0f
		    && sector.fighter_owner != -1.0f)
		    || sector.planet > 1.0f))
			break;
	}
	local.draws_consumed = game->random.draws - starting_draws;
	old_logical = qb_cint_mbf32(game->config.record.bytes + YT_F117,
	    0U, &overflow);
	local.old_headquarters = overflow ? 0 : old_logical;
	local.target_sector = candidate;
	game->config.headquarters = (float)candidate;
	location[1] = (float)candidate;
	if (!yt_database_read(&game->database, 1U, &config_record, error)
	    || !yt_record_set_number(&config_record, YT_F117,
	    (float)candidate)
	    || !yt_database_write(&game->database, 1U, &config_record, error)) {
		if (error != NULL && error->status == YT_OK)
			set_error(error, YT_RANGE,
			    "encode Xannor headquarters config", "YTDATA.DAT");
		return false;
	}
	game->config.record = config_record;
	if (overflow || old_logical < 1
	    || !yt_game_read_sector(game, old_logical, &sector, error)) {
		if (error != NULL && error->status == YT_OK)
			set_error(error, YT_RANGE, "old Xannor headquarters",
			    "YTDATA.DAT");
		return false;
	}
	planet_number = qb_single_subtract(game->config.total_records,
	    game->config.planet_offset);
	if (sector.planet == planet_number) {
		if (!yt_record_set_number(&sector.record, YT_F93, 0.0f)
		    || !yt_database_write(&game->database,
		    (size_t)yt_sector_basic_record(&game->config, old_logical),
		    &sector.record, error)) {
			if (error != NULL && error->status == YT_OK)
				set_error(error, YT_RANGE,
				    "encode old Xannor headquarters", "YTDATA.DAT");
			return false;
		}
	}
	if (!yt_game_read_sector(game, candidate, &sector, error)
	    || !yt_record_set_number(&sector.record, YT_F93, planet_number)
	    || !yt_database_write(&game->database,
	    (size_t)yt_sector_basic_record(&game->config, candidate),
	    &sector.record, error)) {
		if (error != NULL && error->status == YT_OK)
			set_error(error, YT_RANGE,
			    "encode new Xannor headquarters", "YTDATA.DAT");
		return false;
	}
	if (!yt_maintenance_compose_xannor_relocation(blank,
	    blank_length, &output)
	    || !yt_news_append_bytes(output.rows[0].data,
	    output.rows[0].length, error)
	    || !line_output(line_context, output.rows[0].data,
	    output.rows[0].length, error)
	    || !line_output(line_context, output.rows[1].data,
	    output.rows[1].length, error))
		return false;
	if (result != NULL)
		*result = local;
	return true;
}

bool
yt_maintenance_xannor_revenge_slot(struct yt_game *game,
    const float *player_sector, size_t cache_count,
    const uint8_t *blank, size_t blank_length,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_maintenance_xannor_revenge_result *result,
    struct yt_error *error)
{
	static const uint8_t dirty_zero[4] = {0x00, 0x00, 0x28, 0x00};
	struct yt_maintenance_xannor_revenge_result local = {0};
	struct yt_maintenance_output_result output;
	struct yt_sector metadata;
	float slot_value;
	bool overflow;
	int32_t record;

	if (game == NULL || player_sector == NULL || line_output == NULL
	    || (blank == NULL && blank_length != 0U)) {
		set_error(error, YT_INVALID, "Xannor revenge slot", "YTDATA.DAT");
		return false;
	}
	if (result != NULL)
		memset(result, 0, sizeof(*result));
	if (!yt_game_read_sector(game, 21, &metadata, error))
		return false;
	slot_value = yt_record_get_number(&metadata.record, YT_F105);
	if (slot_value > 0.0f) {
		struct yt_player player;

		record = qb_cint_mbf32(metadata.record.bytes + YT_F105, 0U,
		    &overflow);
		if (overflow || record < 0 || (size_t)record >= cache_count) {
			set_error(error, YT_RANGE, "Xannor revenge player",
			    "YTDATA.DAT");
			return false;
		}
		if (!yt_game_read_player(game, record, &player, error))
			return false;
		if (player.sector > 7.0f) {
			local.live_sector = qb_cint_mbf32(player.record.bytes + YT_F57,
			    0U, &overflow);
			if (overflow) {
				set_error(error, YT_RANGE, "Xannor revenge sector",
				    "YTDATA.DAT");
				return false;
			}
			local.cached_target = qb_cint(player_sector[record],
			    &overflow);
			if (overflow) {
				set_error(error, YT_RANGE, "Xannor revenge cache",
				    "YTDATA.DAT");
				return false;
			}
			local.eligible = true;
			if (!yt_maintenance_compose_xannor_revenge(blank,
			    blank_length, &output)
			    || !line_output(line_context, output.rows[0].data,
			    output.rows[0].length, error)
			    || !yt_news_append_bytes(output.rows[1].data,
			    output.rows[1].length, error)
			    || !line_output(line_context, output.rows[1].data,
			    output.rows[1].length, error)
			    || !line_output(line_context, output.rows[2].data,
			    output.rows[2].length, error))
				return false;
		}
	}
	if (!yt_game_read_sector(game, 21, &metadata, error)
	    || !yt_record_set_raw_number(&metadata.record, YT_F105, dirty_zero)
	    || !yt_database_write(&game->database,
	    (size_t)yt_sector_basic_record(&game->config, 21),
	    &metadata.record, error)) {
		if (error != NULL && error->status == YT_OK)
			set_error(error, YT_RANGE, "clear Xannor revenge slot",
			    "YTDATA.DAT");
		return false;
	}
	if (result != NULL)
		*result = local;
	return true;
}

static bool
maintenance_write_xannor_rebuild(struct yt_game *game, int logical,
    struct yt_planet *planet, int today, float minute,
    struct yt_error *error)
{
	static const size_t production_offsets[] = {YT_F45, YT_F49, YT_F53};
	static const size_t stock_offsets[] = {YT_F57, YT_F61, YT_F65};
	int index;

	yt_record_set_text(&planet->record, (const uint8_t *)"Xannoron", 8U);
	if (!yt_record_set_number(&planet->record, YT_F41, (float)today))
		goto range;
	for (index = 0; index < 3; ++index) {
		if (!yt_record_set_number(&planet->record,
		    production_offsets[index], 100000.0f)
		    || !yt_record_set_number(&planet->record,
		    stock_offsets[index], 0.0f))
			goto range;
	}
	if (!yt_record_set_number(&planet->record, YT_F69, 0.0f)
	    || !yt_record_set_number(&planet->record, YT_F73, -1.0f)
	    || !yt_record_set_number(&planet->record, YT_F77,
	    planet->ground_forces)
	    || !yt_record_set_number(&planet->record, YT_F85, 8.0f)
	    || !yt_record_set_number(&planet->record, YT_F89, minute)
	    || !yt_record_set_number(&planet->record, YT_F117, planet->bank)
	    || !yt_record_set_number(&planet->record, YT_F125, 0.0f))
		goto range;
	return yt_database_write(&game->database,
	    (size_t)yt_planet_basic_record(&game->config, logical),
	    &planet->record, error);

range:
	set_error(error, YT_RANGE, "encode Xannoron rebuild", "YTDATA.DAT");
	return false;
}

static bool
maintenance_write_xannor_daily(struct yt_game *game, int logical,
    struct yt_planet *planet, struct yt_error *error)
{
	if (!yt_record_set_number(&planet->record, YT_F73, planet->owner)
	    || !yt_record_set_number(&planet->record, YT_F77,
	    planet->ground_forces)
	    || !yt_record_set_number(&planet->record, YT_F117, planet->bank)) {
		set_error(error, YT_RANGE, "encode Xannoron daily", "YTDATA.DAT");
		return false;
	}
	return yt_database_write(&game->database,
	    (size_t)yt_planet_basic_record(&game->config, logical),
	    &planet->record, error);
}

static bool
maintenance_write_xannor_sector(struct yt_game *game, int logical,
    struct yt_sector *sector, struct yt_error *error)
{
	if (!yt_record_set_number(&sector->record, YT_F93, sector->planet)) {
		set_error(error, YT_RANGE, "encode Xannor sector", "YTDATA.DAT");
		return false;
	}
	return yt_database_write(&game->database,
	    (size_t)yt_sector_basic_record(&game->config, logical),
	    &sector->record, error);
}

bool
yt_maintenance_maintain_xannor_home(struct yt_game *game,
    const uint8_t *blank, size_t blank_length,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_maintenance_xannor_home_result *result,
    struct yt_error *error)
{
	struct yt_maintenance_xannor_home_result local = {0};
	struct yt_maintenance_output_result output;
	struct yt_sector sector;
	struct yt_planet planet;
	uint64_t starting_draws;
	float sample;
	float minute;
	int sector_count;
	int planet_count;
	int headquarters;
	int today;
	size_t row;

	if (game == NULL || line_output == NULL
	    || (blank == NULL && blank_length != 0U)) {
		set_error(error, YT_INVALID, "maintain Xannoron", "YTDATA.DAT");
		return false;
	}
	sector_count = (int)(game->config.port_offset
	    - game->config.sector_offset);
	planet_count = (int)(game->config.total_records
	    - game->config.planet_offset);
	headquarters = (int)game->config.headquarters;
	if (headquarters < 1 || headquarters > sector_count
	    || planet_count < 1 || planet_count > 100
	    || !yt_maintenance_compose_xannor_home(blank,
	    blank_length, false, &output)) {
		set_error(error, YT_RANGE, "maintain Xannoron", "YTDATA.DAT");
		return false;
	}
	if (result != NULL)
		memset(result, 0, sizeof(*result));
	for (row = 0U; row < output.row_count; ++row) {
		if (!line_output(line_context, output.rows[row].data,
		    output.rows[row].length, error))
			return false;
	}
	if (!yt_game_read_sector(game, headquarters, &sector, error))
		return false;
	local.planet_link_before = sector.planet;
	local.rebuilt = sector.planet == 0.0f;
	starting_draws = game->random.draws;
	if (local.rebuilt) {
		if (!yt_current_date_serial(game->config.epoch_year, &today, NULL,
		    error)
		    || !yt_maintenance_compose_xannor_home(blank,
		    blank_length, true, &output))
			return false;
		for (row = 2U; row < 4U; ++row) {
			if (!line_output(line_context, output.rows[row].data,
			    output.rows[row].length, error))
				return false;
		}
		if (!yt_news_append_bytes(output.rows[3].data,
		    output.rows[3].length, error)
		    || !yt_game_read_planet(game, planet_count, &planet, error))
			return false;
		minute = yt_maintenance_sint(
		    qb_single_divide((float)yt_platform_timer(), 60.0f));
		if (!yt_random_next(&game->random, &sample, error))
			return false;
		planet.ground_forces = yt_maintenance_sint(
		    qb_single_multiply(sample, 250.0f));
		if (!yt_random_next(&game->random, &sample, error))
			return false;
		planet.bank = qb_single_add(100000.0f, qb_single_multiply(sample, 10000000.0f));
		if (!maintenance_write_xannor_rebuild(game, planet_count,
		    &planet, today, minute, error)
		    || !line_output(line_context, output.rows[4].data,
		    output.rows[4].length, error)
		    || !yt_news_append_bytes(output.rows[4].data,
		    output.rows[4].length, error)
		    || !yt_game_read_sector(game, headquarters, &sector, error))
			return false;
		sector.planet = (float)planet_count;
		if (!maintenance_write_xannor_sector(game, headquarters,
		    &sector, error))
			return false;
	}
	if (!yt_game_read_planet(game, planet_count, &planet, error))
		return false;
	local.ground_before_daily_update = planet.ground_forces;
	if (!yt_random_next(&game->random, &sample, error))
		return false;
	planet.ground_forces = qb_single_add(planet.ground_forces,
	    yt_maintenance_sint(qb_single_multiply(sample, 25.0f)));
	planet.owner = -1.0f;
	if (planet.bank == 0.0f)
		planet.bank = 16000000.0f;
	local.ground_after_daily_update = planet.ground_forces;
	local.bank_after = planet.bank;
	local.draws_consumed = game->random.draws - starting_draws;
	if (!maintenance_write_xannor_daily(game, planet_count, &planet,
	    error))
		return false;
	if (result != NULL)
		*result = local;
	return true;
}

bool
yt_maintenance_xannor_hunt(struct yt_game *game,
    const float *player_sector, const float *player_cloak, size_t cache_count,
    const uint8_t *blank, size_t blank_length,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_maintenance_xannor_hunt_result *result,
    struct yt_error *error)
{
	struct yt_maintenance_xannor_hunt_result local = {0};
	struct yt_maintenance_output_result output;
	struct yt_maintenance_text name;
	struct yt_player player;
	uint64_t starting_draws;
	bool overflow;
	float gate;
	float selection;
	int32_t stored_length;
	int player_count;
	int candidate;
	size_t row;

	if (game == NULL || player_sector == NULL || player_cloak == NULL
	    || line_output == NULL || (blank == NULL
	    && blank_length != 0U)) {
		set_error(error, YT_INVALID, "Xannor hunt", "YTDATA.DAT");
		return false;
	}
	player_count = (int)game->config.sector_offset - 1;
	if (player_count < 1 || cache_count < (size_t)player_count + 2U
	    || !yt_maintenance_compose_xannor_hunt(blank,
	    blank_length, NULL, &output)) {
		set_error(error, YT_RANGE, "Xannor hunt", "YTDATA.DAT");
		return false;
	}
	if (result != NULL)
		memset(result, 0, sizeof(*result));
	for (row = 0U; row < output.row_count; ++row) {
		if (!line_output(line_context, output.rows[row].data,
		    output.rows[row].length, error))
			return false;
	}
	for (candidate = 2; candidate <= player_count + 1; ++candidate) {
		if (!yt_game_read_player(game, candidate, &player, error))
			return false;
		if (qb_mbf32_decode(player.record.bytes + YT_F85) != 0.0f
		    && player.score > local.top_score) {
			local.top_record = candidate;
			local.top_score = player.score;
		}
	}
	if (!yt_news_append("  -  Xannor report:", error))
		return false;
	starting_draws = game->random.draws;
	if (local.top_record == 0) {
		if (result != NULL)
			*result = local;
		return true;
	}
	if (!yt_game_read_player(game, local.top_record, &player, error)
	    || !yt_random_next(&game->random, &gate, error))
		return false;
	if (local.top_score < 2500000.0f
	    || qb_single_subtract(player_cloak[local.top_record],
	    0.33000001311302185f) > gate) {
		local.draws_consumed = game->random.draws - starting_draws;
		if (result != NULL)
			*result = local;
		return true;
	}
	stored_length = qb_cint_mbf32(player.record.bytes + YT_F85, 0U,
	    &overflow);
	if (overflow || stored_length < 0) {
		set_error(error, YT_RANGE, "Xannor hunt name", "YTDATA.DAT");
		return false;
	}
	name.data = player.record.bytes;
	name.length = (size_t)stored_length < YT_TEXT_FIELD_SIZE
	    ? (size_t)stored_length : YT_TEXT_FIELD_SIZE;
	if (!yt_maintenance_compose_xannor_hunt(blank,
	    blank_length, &name, &output))
		return false;
	for (row = 4U; row < output.row_count; ++row) {
		if (!line_output(line_context, output.rows[row].data,
		    output.rows[row].length, error))
			return false;
	}
	local.selected = true;
	local.target_sector = (int)player.sector;
	if (!yt_random_next(&game->random, &selection, error))
		return false;
	if (selection > 0.25f) {
		local.used_cached_sector = true;
		local.target_sector = (int)player_sector[local.top_record];
	}
	local.draws_consumed = game->random.draws - starting_draws;
	if (result != NULL)
		*result = local;
	return true;
}

static bool
xannor_reclaim_and_relocate(struct maint_state *state, float location[21],
    float size[21], float regeneration,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_error *error)
{
	struct yt_maintenance_xannor_reclaim_result reclaim;

	if (!yt_maintenance_xannor_headquarters_reclaim(&state->game,
	    location, size, line_output, line_context, &reclaim, error))
		return false;
	return yt_maintenance_xannor_headquarters_relocate(&state->game,
	    location, reclaim.original_hostile, size[1], (double)regeneration,
	    NULL, 0U,
	    line_output, line_context, NULL, error);
}

static bool
consume_revenge_slot(struct maint_state *state, int *live_sector,
    int *cached_target, yt_maintenance_score_line_fn line_output,
    void *line_context, struct yt_error *error)
{
	struct yt_maintenance_xannor_revenge_result revenge;

	if (!yt_maintenance_xannor_revenge_slot(&state->game,
	    state->player_sector, (size_t)state->player_count + 2U,
	    NULL, 0U,
	    line_output, line_context, &revenge, error))
		return false;
	*live_sector = revenge.live_sector;
	*cached_target = revenge.cached_target;
	return true;
}

static bool
xannor_candidate_target(struct maint_state *state, float current_location,
    int revenge_live, int revenge_cached, int *target,
    struct yt_error *error)
{
	struct yt_maintenance_xannor_discovery_result discovery;
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
	    revenge_cached, &discovery, error))
		return false;
	*target = discovery.target_sector;
	return true;
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
xannor_arrival_emit(yt_maintenance_score_line_fn line_output,
    void *line_context, const uint8_t *line, size_t length,
    struct yt_error *error)
{
	return yt_news_append_bytes(line, length, error)
	    && line_output(line_context, line, length, error);
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
	float initial_owner;
	float defense_group;
	float remaining_defenders;
	size_t length;
	int first_length;
	int second_length;
	bool overflow;

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
		int damage;

		if (!yt_maintenance_random_integer(&game->random, 1000,
		    &damage, error))
			return false;
		if ((float)damage > *group_size)
			damage = (int)*group_size;
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
		if (first_length < 0 || second_length < 0
		    || !maintenance_copy_part(line, sizeof(line), &length,
		    hit_prefix, sizeof(hit_prefix) - 1U)
		    || !maintenance_copy_part(line, sizeof(line), &length,
		    (const uint8_t *)first, (size_t)first_length)
		    || !maintenance_copy_part(line, sizeof(line), &length,
		    hit_middle, sizeof(hit_middle) - 1U)
		    || !maintenance_copy_part(line, sizeof(line), &length,
		    (const uint8_t *)second, (size_t)second_length)
		    || !maintenance_copy_part(line, sizeof(line), &length,
		    (const uint8_t *)"!", 1U)
		    || !xannor_arrival_emit(line_output, line_context, line,
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
			if (first_length < 0
			    || !maintenance_copy_part(line, sizeof(line), &length,
			    loss_prefix, sizeof(loss_prefix) - 1U)
			    || !maintenance_copy_part(line, sizeof(line), &length,
			    (const uint8_t *)first, (size_t)first_length)
			    || !maintenance_copy_part(line, sizeof(line), &length,
			    loss_suffix, sizeof(loss_suffix) - 1U)
			    || !xannor_arrival_emit(line_output, line_context, line,
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
	if (initial_owner > 0.0f) {
		int32_t record = qb_cint_mbf32(sector->record.bytes + YT_F85,
		    0U, &overflow);

		if (overflow || record < 1
		    || !yt_game_read_player(game, record, &player, error)
		    || !yt_player_stored_name(&player, defender_name,
		    &defender.length, error)) {
			if (error != NULL && error->status == YT_OK)
				set_error(error, YT_RANGE,
				    "Xannor defense owner", "YTDATA.DAT");
			return false;
		}
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
		sector->fighter_owner = 0.0f;
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
	if (first_length < 0 || second_length < 0
	    || !maintenance_copy_part(line, sizeof(line), &length,
	    defense_prefix, sizeof(defense_prefix) - 1U)
	    || !maintenance_copy_part(line, sizeof(line), &length,
	    defender.data, defender.length)
	    || !maintenance_copy_part(line, sizeof(line), &length,
	    defense_lost, sizeof(defense_lost) - 1U)
	    || !maintenance_copy_part(line, sizeof(line), &length,
	    (const uint8_t *)first, (size_t)first_length)
	    || !maintenance_copy_part(line, sizeof(line), &length,
	    defense_destroyed, sizeof(defense_destroyed) - 1U)
	    || !maintenance_copy_part(line, sizeof(line), &length,
	    (const uint8_t *)second, (size_t)second_length)
	    || !maintenance_copy_part(line, sizeof(line), &length,
	    remaining_defenders < 1.0f ? player_destroyed : xannor_destroyed,
	    remaining_defenders < 1.0f ? sizeof(player_destroyed) - 1U
	    : sizeof(xannor_destroyed) - 1U)
	    || !xannor_arrival_emit(line_output, line_context, line, length,
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
	if (sector->planet <= 0.0f)
		return true;
	arrival_sector_number = (int)*group_location;
	planet_number = (int)sector->planet;
	if (!yt_game_read_planet(game, planet_number, &planet, error))
		return false;
	if (qb_mbf32_decode(planet.record.bytes + YT_F85) <= 0.0f
	    || planet.owner == -1.0f)
		return true;
	if (!yt_game_read_planet(game, planet_number, &planet, error))
		return false;
	if (!yt_planet_stored_name(&planet, stored_name,
	    &stored_name_length, error))
		return false;
	number_length = qb_str_single(number, sizeof(number), *group_size);
	line_length = 0U;
	if (number_length < 0
	    || !maintenance_copy_part(line, sizeof(line), &line_length,
	    attack_prefix, sizeof(attack_prefix) - 1U)
	    || !maintenance_copy_part(line, sizeof(line), &line_length,
	    (const uint8_t *)number, (size_t)number_length)
	    || !maintenance_copy_part(line, sizeof(line), &line_length,
	    attack_middle, sizeof(attack_middle) - 1U)
	    || !maintenance_copy_part(line, sizeof(line), &line_length,
	    stored_name, stored_name_length)
	    || !maintenance_copy_part(line, sizeof(line), &line_length,
	    quote, sizeof(quote) - 1U)
	    || !xannor_arrival_emit(line_output, line_context, line,
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
		planet.owner = 0.0f;
	destroyed = planet.production[0] == 0.0f
	    && planet.production[1] == 0.0f
	    && planet.production[2] == 0.0f;
	if (destroyed) {
		sector->planet = 0.0f;
		planet.name_length = 0.0f;
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
		    YT_F45 + (size_t)index * 4U, planet.production[index])
		    || !yt_record_set_number(&planet.record,
		    YT_F57 + (size_t)index * 4U, planet.stock[index]))
			goto encode_error;
	}
	planet.owner = mutated.owner;
	planet.ground_forces = mutated.ground_forces;
	if (!yt_record_set_number(&planet.record, YT_F73, planet.owner)
	    || !yt_record_set_number(&planet.record, YT_F77,
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
		sector->planet = 0.0f;
		if (!yt_record_set_number(&sector->record, YT_F93, 0.0f)) {
			set_error(error, YT_RANGE, "encode destroyed Xannor link",
			    "YTDATA.DAT");
			return false;
		}
		if (!yt_game_write_sector(game, arrival_sector_number, sector,
		    error)
		    || !yt_game_read_planet(game, planet_number, &planet, error))
			return false;
		planet.name_length = 0.0f;
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
		    planet_prefix, sizeof(planet_prefix) - 1U)
		    || !maintenance_copy_part(line, sizeof(line), &line_length,
		    stored_name, stored_name_length)
		    || !maintenance_copy_part(line, sizeof(line), &line_length,
		    planet_suffix, sizeof(planet_suffix) - 1U)
		    || !xannor_arrival_emit(line_output, line_context, line,
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
    float *player_sector, float *player_cloak, size_t cache_count,
    int player_record, float *xannor_fighters,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_error *error)
{
	struct yt_player player;
	float original_xannor;
	float original_shields;
	float remaining_fighters;
	float remaining_shields;
	struct yt_maintenance_xannor_player_result combat = {0};
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
	if (qb_mbf32_decode(player.record.bytes + YT_F85) == 0.0f
	    || player.killed_by != 0.0f)
		return true;
	player_sector[player_record] = player.sector;
	player_cloak[player_record] = player.cloak;
	original_xannor = *xannor_fighters;
	original_shields = player.shields;
	remaining_shields = original_shields;
	if (!xannor_player_fighter_phase(&game->random, &player.fighters,
	    original_xannor, &combat, error))
		return false;
	remaining_fighters = player.fighters;
	if (combat.player_fighter_losses > 0.0f) {
		char losses[48];

		qb_str_double(losses, sizeof(losses),
		    (double)combat.player_fighter_losses);
		snprintf(radio_line, sizeof(radio_line),
		    "Ha! We kilt%s of yoor fyterz hoo-man slyme!", losses);
		if (!yt_radio_append_maintenance(radio_line, -1.0f,
		    (float)player_record, error))
			return false;
	}
	if (!yt_game_read_player(game, player_record, &player, error))
		return false;
	player.fighters = remaining_fighters;
	if (!yt_record_set_number(&player.record, YT_F61, player.fighters)
	    || !yt_database_write(&game->database, (size_t)player_record,
	    &player.record, error)
	    || !xannor_player_shield_phase(&game->random, player.fighters,
	    &remaining_shields, original_xannor, &combat, error))
		return false;
	if (original_shields - remaining_shields > 0.0f) {
		char losses[48];

		qb_str_double(losses, sizeof(losses),
		    (double)(original_shields - remaining_shields));
		snprintf(radio_line, sizeof(radio_line),
		    "Peh! Whee maik yoor wheak sheeldz%s unitz!", losses);
		if (!yt_radio_append_maintenance(radio_line, -1.0f,
		    (float)player_record, error))
			return false;
	}
	if (!yt_game_read_player(game, player_record, &player, error))
		return false;
	player.shields = remaining_shields;
	if (!yt_record_set_number(&player.record, YT_F53, player.shields)
	    || !yt_database_write(&game->database, (size_t)player_record,
	    &player.record, error))
		return false;
	*xannor_fighters = qb_single_subtract(original_xannor, combat.xannor_losses);
	killed = player.shields < 1.0f;
	if (killed) {
		if (!yt_game_read_player(game, player_record, &player, error)
		    || !yt_maintenance_immediate_death(game, player_sector,
		    player_cloak, cache_count, player_record, -1.0f, &player,
		    error))
			return false;
	}
	if (*xannor_fighters <= 0.0f)
		*xannor_fighters = 0.0f;
	if (!yt_game_read_player(game, player_record, &player, error)
	    || !yt_player_stored_name(&player, stored_name,
	    &stored_name_length, error)
	    || !yt_maintenance_xannor_player_line_bytes(stored_name,
	    stored_name_length, &combat, *xannor_fighters, player.shields,
	    killed, line, sizeof(line), &line_length)
	    || !yt_news_append_bytes(line, line_length, error)
	    || !line_output(line_context, line, line_length, error))
		return false;
	if (killed) {
		if (!yt_game_read_player(game, player_record, &player, error)
		    || !yt_radio_append_maintenance(
		    "HA! We kilt yoo yoo hoo-man slyme bull!", -1.0f,
		    (float)player_record, error))
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

static bool
xannor_route_arrivals_impl(struct maint_state *state, int group,
    int target, float top_player_target, float location[21], float size[21],
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_maintenance_xannor_route_result *result,
    struct yt_error *error)
{
	struct yt_maintenance_xannor_route_result local = {0};

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
			local.route_missing = location[group] != (float)target;
			if (local.route_missing) {
				struct yt_maintenance_output_result output;

				if (!yt_maintenance_compose_xannor_path_error(
				    (float)source, (float)target, &output)
				    || !maintenance_emit_output_row(&output, 0x38A8U,
				    line_output, line_context, error))
					return false;
			}
			break;
		}
		if (size[group] < 1.0f || location[group] < 1.0f) {
			size[group] = 0.0f;
			location[group] = 0.0f;
			local.exhausted = true;
			break;
		}
		/*
		 * The shipped first-pass 3A5E gate sends group 20 from its
		 * retained top-player sector through the 3C90 completion test
		 * before using the already-built next hop.  With an ordinary
		 * route this either completes or returns to the same arrival
		 * continuation without another externally visible operation.
		 */
		if (local.hops == 0
		    && yt_maintenance_xannor_bypass_initial_arrival(group,
		    location[group], top_player_target)
		    && yt_maintenance_xannor_route_complete(location[group],
		    target)) {
			local.reached_target = true;
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
		++local.hops;
		local.reached_target = yt_maintenance_xannor_route_complete(
		    location[group], target);
		local.exhausted = location[group] <= 0.0f
		    || size[group] <= 0.0f;
		if (local.reached_target || local.exhausted)
			break;
	}
	*result = local;
	return true;
}

bool
yt_maintenance_xannor_route_arrivals(struct yt_game *game,
    struct yt_maintenance_route_cache *cache, float *player_sector,
    float *player_cloak, size_t cache_count, int group_number,
    int target_sector, float location[21], float size[21],
    struct yt_maintenance_xannor_route_result *result,
    struct yt_error *error)
{
	struct maint_state state;
	bool success;
	int player_count;

	if (game == NULL || cache == NULL || player_sector == NULL
	    || player_cloak == NULL || location == NULL || size == NULL
	    || result == NULL || group_number < 2 || group_number > 20) {
		set_error(error, YT_INVALID, "Xannor route arrivals",
		    "YTDATA.DAT");
		return false;
	}
	player_count = (int)game->config.sector_offset - 1;
	if (player_count < 0 || cache_count < (size_t)player_count + 2U) {
		set_error(error, YT_RANGE, "Xannor route player cache",
		    "YTDATA.DAT");
		return false;
	}
	memset(&state, 0, sizeof(state));
	state.game = *game;
	state.route_cache = *cache;
	state.player_sector = player_sector;
	state.player_cloak = player_cloak;
	state.player_count = player_count;
	state.sector_count = (int)(game->config.port_offset
	    - game->config.sector_offset);
	success = xannor_route_arrivals_impl(&state, group_number,
	    target_sector, (float)target_sector, location, size,
	    maintenance_stdout_line, NULL, result, error);
	game->random = state.game.random;
	*cache = state.route_cache;
	return success;
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
	sector_count = (int)(game->config.port_offset
	    - game->config.sector_offset);
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
		    location[group])
		    || !yt_database_write(&game->database,
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
			host.fighter_owner = -1.0f;
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
	sector_count = (int)(game->config.port_offset
	    - game->config.sector_offset);
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
    float *player_sector, float *player_cloak, size_t cache_count,
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
	    hunt_player)
	    && !yt_maintenance_xannor_player_arrival(game, player_sector,
	    player_cloak, cache_count, hunt_player, group_size, line_output,
	    line_context, error))
		return false;
	if (*group_size <= 0.0f)
		*group_location = 0.0f;
	return yt_maintenance_xannor_group_twenty_finish(game, group_number,
	    *group_location, *group_size, error);
}

static bool
xannor_roaming_groups_impl(struct maint_state *state, float score,
    int top_target, int hunt_player, int revenge_live, int revenge_cached,
    float location[21], float size[21],
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_error *error)
{
	int group = 2;

	for (;;) {
		struct yt_maintenance_xannor_split_result split_result;
		bool retarget;

		if (!yt_maintenance_xannor_roaming_split(
		    &state->game.random, group, &size[1], &size[group],
		    &location[group], score, state->game.config.headquarters,
		    &split_result, error))
			return false;
		if (!split_result.skip_group) {
			do {
				int target;
				struct yt_maintenance_output_result group_output;
				struct yt_maintenance_xannor_route_result route_result;

				if (!xannor_candidate_target(state,
				    location[group], revenge_live, revenge_cached,
				    &target, error)
				    || !yt_maintenance_xannor_target_override(group,
				    target, size[1], score,
				    (int)state->game.config.headquarters,
				    revenge_live, top_target, &target, error)
				    || !yt_maintenance_compose_xannor_group(group,
				    size[group], &group_output)
				    || !maintenance_emit_output_row(&group_output,
				    0x34B2U, line_output, line_context, error))
					return false;
				if (!xannor_route_arrivals_impl(state, group,
				    target, (float)top_target, location, size,
				    line_output, line_context, &route_result, error))
					return false;
				if (!yt_maintenance_xannor_target_finish(&state->game,
				    state->player_sector, state->player_cloak,
				    (size_t)state->player_count + 2U,
				    route_result.reached_target, group, hunt_player,
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
yt_maintenance_xannor_roaming_groups(struct yt_game *game,
    struct yt_maintenance_route_cache *cache, float *player_sector,
    float *player_cloak, size_t cache_count, float top_score,
    int top_target, int hunt_player, int revenge_live, int revenge_cached,
    float location[21], float size[21],
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_error *error)
{
	struct maint_state state;
	int player_count;
	bool success;

	if (game == NULL || cache == NULL || player_sector == NULL
	    || player_cloak == NULL || location == NULL || size == NULL
	    || line_output == NULL) {
		set_error(error, YT_INVALID, "Xannor roaming groups",
		    "YTDATA.DAT");
		return false;
	}
	player_count = (int)game->config.sector_offset - 1;
	if (player_count < 1 || cache_count < (size_t)player_count + 2U) {
		set_error(error, YT_RANGE, "Xannor roaming player cache",
		    "YTDATA.DAT");
		return false;
	}
	memset(&state, 0, sizeof(state));
	state.game = *game;
	state.route_cache = *cache;
	state.player_sector = player_sector;
	state.player_cloak = player_cloak;
	state.player_count = player_count;
	state.sector_count = (int)(game->config.port_offset
	    - game->config.sector_offset);
	state.port_count = (int)(game->config.planet_offset
	    - game->config.port_offset);
	state.planet_count = (int)(game->config.total_records
	    - game->config.planet_offset);
	success = xannor_roaming_groups_impl(&state, top_score, top_target,
	    hunt_player, revenge_live, revenge_cached, location, size,
	    line_output, line_context, error);
	game->random = state.game.random;
	*cache = state.route_cache;
	return success;
}

static bool
maintain_xannor_impl(struct maint_state *state,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_error *error)
{
	struct yt_maintenance_xannor_hunt_result hunt;
	struct yt_maintenance_xannor_target_result target_result;
	struct yt_maintenance_xannor_regeneration_result regen_result;
	struct yt_maintenance_output_result regen_output;
	struct yt_maintenance_output_result roaming_output;
	float location[21];
	float size[21];
	float regeneration;
	float score;
	int top_target;
	int hunt_player;
	int revenge_live;
	int revenge_cached;

	if (!yt_maintenance_maintain_xannor_home(&state->game,
	    NULL, 0U,
	    line_output, line_context, NULL, error)
	    || !yt_maintenance_xannor_hunt(&state->game, state->player_sector,
	    state->player_cloak, (size_t)state->player_count + 2U,
	    NULL, 0U,
	    line_output, line_context, &hunt, error))
		return false;
	score = hunt.top_score;
	if (!yt_maintenance_xannor_target(&state->game.random,
	    state->sector_count, hunt.selected ? hunt.top_record : 0,
	    hunt.target_sector, &target_result, error)
	    || !yt_maintenance_xannor_groups_extract(&state->game, location,
	    size, error))
		return false;
	top_target = target_result.target_sector;
	hunt_player = target_result.hunt_player;
	if (!yt_maintenance_xannor_regeneration(score, size, &regen_result)
	    || !yt_maintenance_compose_xannor_regeneration(
	    NULL, 0U,
	    regen_result.regeneration, &regen_output)
	    || !line_output(line_context, regen_output.rows[0].data,
	    regen_output.rows[0].length, error)
	    || !line_output(line_context, regen_output.rows[1].data,
	    regen_output.rows[1].length, error)
	    || !yt_news_append_bytes(regen_output.rows[1].data,
	    regen_output.rows[1].length, error)
	    || !line_output(line_context, regen_output.rows[2].data,
	    regen_output.rows[2].length, error))
		return false;
	regeneration = (float)regen_result.regeneration;
	size[1] = regen_result.group_one_after;
	location[1] = state->game.config.headquarters;
	if (!xannor_reclaim_and_relocate(state, location, size,
	    regeneration, line_output, line_context, error)
	    || !consume_revenge_slot(state, &revenge_live, &revenge_cached,
	    line_output, line_context, error)
	    || !yt_maintenance_compose_xannor_roaming(
	    NULL, 0U,
	    &roaming_output)
	    || !line_output(line_context, roaming_output.rows[0].data,
	    roaming_output.rows[0].length, error)
	    || !line_output(line_context, roaming_output.rows[1].data,
	    roaming_output.rows[1].length, error))
		return false;

	return xannor_roaming_groups_impl(state, score, top_target, hunt_player,
	    revenge_live, revenge_cached, location, size,
	    line_output, line_context, error);
}

bool
yt_maintenance_maintain_xannor(struct yt_game *game,
    struct yt_maintenance_route_cache *cache, float *player_sector,
    float *player_cloak, size_t cache_count,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_error *error)
{
	struct maint_state state;
	int player_count;
	bool success;

	if (game == NULL || cache == NULL || player_sector == NULL
	    || player_cloak == NULL || line_output == NULL) {
		set_error(error, YT_INVALID, "Xannor maintenance", "YTDATA.DAT");
		return false;
	}
	player_count = (int)game->config.sector_offset - 1;
	if (player_count < 1 || cache_count < (size_t)player_count + 2U) {
		set_error(error, YT_RANGE, "Xannor maintenance player cache",
		    "YTDATA.DAT");
		return false;
	}
	memset(&state, 0, sizeof(state));
	state.game = *game;
	state.route_cache = *cache;
	state.player_sector = player_sector;
	state.player_cloak = player_cloak;
	state.player_count = player_count;
	state.sector_count = (int)(game->config.port_offset
	    - game->config.sector_offset);
	state.port_count = (int)(game->config.planet_offset
	    - game->config.port_offset);
	state.planet_count = (int)(game->config.total_records
	    - game->config.planet_offset);
	success = maintain_xannor_impl(&state, line_output, line_context, error);
	game->random = state.game.random;
	game->config = state.game.config;
	*cache = state.route_cache;
	return success;
}

static const struct yt_maintenance_output_row *
maintenance_find_output_row(const struct yt_maintenance_output_result *output,
    uint16_t address)
{
	size_t index;

	for (index = 0U; index < output->row_count; ++index)
		if (output->rows[index].address == address)
			return &output->rows[index];
	return NULL;
}

static bool
maintenance_emit_output_row(const struct yt_maintenance_output_result *output,
    uint16_t address, yt_maintenance_score_line_fn line_output,
    void *line_context, struct yt_error *error)
{
	const struct yt_maintenance_output_row *row =
	    maintenance_find_output_row(output, address);

	if (row == NULL || line_output == NULL) {
		set_error(error, YT_INVALID, "maintenance output row", "");
		return false;
	}
	return line_output(line_context, row->data, row->length, error);
}

static bool
maintenance_news_output_row(const struct yt_maintenance_output_result *output,
    uint16_t address, struct yt_error *error)
{
	const struct yt_maintenance_output_row *row =
	    maintenance_find_output_row(output, address);
	char line[YT_MAINTENANCE_OUTPUT_ROW_SIZE + 1U];

	if (row == NULL || row->length > YT_MAINTENANCE_OUTPUT_ROW_SIZE) {
		set_error(error, YT_INVALID, "maintenance news row", "");
		return false;
	}
	memcpy(line, row->data, row->length);
	line[row->length] = '\0';
	return yt_news_append(line, error);
}

static bool
maintenance_write_mercenary_rebuild(struct yt_game *game,
    int planet_number, struct yt_planet *planet, struct yt_error *error)
{
	static const size_t production_offsets[] = {YT_F45, YT_F49, YT_F53};
	static const size_t stock_offsets[] = {YT_F57, YT_F61, YT_F65};
	int index;

	yt_record_set_text(&planet->record,
	    (const uint8_t *)"Mercenary Base", 14U);
	if (!yt_record_set_number(&planet->record, YT_F41, planet->last_day))
		goto range;
	for (index = 0; index < 3; ++index) {
		if (!yt_record_set_number(&planet->record,
		    production_offsets[index], planet->production[index])
		    || !yt_record_set_number(&planet->record,
		    stock_offsets[index], planet->stock[index]))
			goto range;
	}
	if (!yt_record_set_number(&planet->record, YT_F69, planet->missiles)
	    || !yt_record_set_number(&planet->record, YT_F73, planet->owner)
	    || !yt_record_set_number(&planet->record, YT_F77,
	    planet->ground_forces)
	    || !yt_record_set_number(&planet->record, YT_F85,
	    planet->name_length)
	    || !yt_record_set_number(&planet->record, YT_F117, planet->bank)
	    || !yt_record_set_number(&planet->record, YT_F125, planet->mines))
		goto range;
	return yt_database_write(&game->database,
	    (size_t)yt_planet_basic_record(&game->config, planet_number),
	    &planet->record, error);

range:
	set_error(error, YT_RANGE, "encode Mercenary Base rebuild",
	    "YTDATA.DAT");
	return false;
}

static bool
maintenance_write_mercenary_daily(struct yt_game *game, int planet_number,
    struct yt_planet *planet, bool ground_changed, bool bank_changed,
    struct yt_error *error)
{
	if (!yt_record_set_number(&planet->record, YT_F73, planet->owner)
	    || (ground_changed && !yt_record_set_number(&planet->record, YT_F77,
	    planet->ground_forces))
	    || (bank_changed && !yt_record_set_number(&planet->record, YT_F117,
	    planet->bank))) {
		set_error(error, YT_RANGE, "encode Mercenary Base daily",
		    "YTDATA.DAT");
		return false;
	}
	return yt_database_write(&game->database,
	    (size_t)yt_planet_basic_record(&game->config, planet_number),
	    &planet->record, error);
}

bool
yt_maintenance_maintain_mercenary_base(struct yt_game *game,
    int sector_count, int planet_number, bool *rebuilt,
    struct yt_error *error)
{
	int linked_sector = 0;
	int sector_number;
	struct yt_planet planet;
	bool ground_changed;
	bool bank_changed;

	if (game == NULL || rebuilt == NULL || sector_count < 1
	    || planet_number < 1) {
		set_error(error, YT_INVALID, "Mercenary Base result", "");
		return false;
	}
	*rebuilt = false;
	for (sector_number = 1; sector_number <= sector_count; ++sector_number) {
		struct yt_sector sector;

		if (!yt_game_read_sector(game, sector_number, &sector, error))
			return false;
		if (sector.planet == (float)planet_number) {
			linked_sector = sector_number;
			break;
		}
	}
	if (linked_sector == 0) {
		struct yt_sector sector;
		int today;

		if (!yt_current_date_serial(game->config.epoch_year, &today, NULL,
		    error)
		    || !yt_game_read_planet(game, planet_number, &planet, error))
			return false;
		yt_record_set_text(&planet.record,
		    (const uint8_t *)"Mercenary Base", 14);
		strcpy(planet.name, "Mercenary Base");
		planet.name_length = 14.0f;
		planet.last_day = (float)(today - 10);
		planet.production[0] = 100000.0f;
		planet.production[1] = 100000.0f;
		planet.production[2] = 100000.0f;
		planet.stock[0] = planet.stock[1] = planet.stock[2] = 0.0f;
		planet.missiles = 0.0f;
		planet.owner = -2.0f;
		planet.ground_forces = 150000.0f;
		planet.bank = 25000000.0f;
		planet.mines = 0.0f;
		if (!maintenance_write_mercenary_rebuild(game, planet_number,
		    &planet, error))
			return false;
		do {
			if (!yt_maintenance_random_integer(&game->random,
			    sector_count, &sector_number, error)
			    || !yt_game_read_sector(game, sector_number, &sector,
			    error))
				return false;
		} while (sector.planet > 0.0f);
		sector.planet = (float)planet_number;
		if (!yt_record_set_number(&sector.record, YT_F93, sector.planet)) {
			set_error(error, YT_RANGE, "encode Mercenary Base link",
			    "YTDATA.DAT");
			return false;
		}
		if (!yt_database_write(&game->database,
		    (size_t)yt_sector_basic_record(&game->config, sector_number),
		    &sector.record, error))
			return false;
		*rebuilt = true;
	}
	if (!yt_game_read_planet(game, planet_number, &planet, error))
		return false;
	planet.owner = -2.0f;
	ground_changed = planet.ground_forces < 1.0f;
	if (ground_changed)
		planet.ground_forces = 150000.0f;
	bank_changed = planet.bank == 0.0f;
	if (bank_changed)
		planet.bank = 25000000.0f;
	return maintenance_write_mercenary_daily(game, planet_number, &planet,
	    ground_changed, bank_changed, error);
}

bool
yt_maintenance_collect_mercenary_tax(struct yt_game *game, int port_count,
    struct yt_maintenance_mercenary_tax_result *result,
    struct yt_error *error)
{
	struct yt_maintenance_mercenary_tax_result local = {0};
	int port_number;

	if (game == NULL || result == NULL || port_count < 0) {
		set_error(error, YT_INVALID, "Mercenary port tax", "");
		return false;
	}
	for (port_number = 1; port_number <= port_count; ++port_number) {
		struct yt_port port;
		float tax;

		if (!yt_game_read_port(game, port_number, &port, error))
			return false;
		if (port.treasury == 0.0f)
			continue;
		tax = yt_maintenance_sint(
		    qb_single_divide(port.treasury, 10.0f));
		local.tax_pool = qb_single_add(local.tax_pool, tax);
		port.treasury = qb_single_subtract(port.treasury,
		    yt_maintenance_sint(
		    qb_single_divide(port.treasury, 10.0f)));
		if (!yt_game_write_port(game, port_number, &port, error))
			return false;
		++local.taxed_ports;
	}
	local.fleet_strength = yt_maintenance_sint(
	    qb_single_divide(local.tax_pool, 10.0f));
	*result = local;
	return true;
}

bool
yt_maintenance_place_mercenary_fleets(struct yt_game *game,
    int sector_count, float strength,
    float *hired_fighters, struct yt_error *error)
{
	int fleet;

	if (game == NULL || hired_fighters == NULL || sector_count < 2) {
		set_error(error, YT_INVALID, "Mercenary funding result", "");
		return false;
	}
	*hired_fighters = 0.0f;
	if (strength <= 0.0f)
		return true;
	for (fleet = 0; fleet < 10; ++fleet) {
		int sector_number;
		struct yt_sector sector;

		do {
			if (!yt_maintenance_random_integer(&game->random,
			    sector_count - 1, &sector_number, error))
				return false;
			++sector_number;
			if (!yt_game_read_sector(game, sector_number,
			    &sector, error))
				return false;
		} while (sector.fighters > 0.0f);
		sector.fighters = strength;
		sector.fighter_owner = -2.0f;
		if (!yt_record_set_number(&sector.record, YT_F81,
		    sector.fighters)
		    || !yt_record_set_number(&sector.record, YT_F85,
		    sector.fighter_owner)) {
			set_error(error, YT_RANGE, "encode Mercenary fleet",
			    "YTDATA.DAT");
			return false;
		}
		if (!yt_database_write(&game->database,
		    (size_t)yt_sector_basic_record(&game->config, sector_number),
		    &sector.record, error))
			return false;
	}
	*hired_fighters = qb_single_multiply(strength, 10.0f);
	return true;
}

bool
yt_maintenance_mercenary_defections(struct yt_game *game, int sector_count,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_maintenance_mercenary_defection_result *result,
    struct yt_error *error)
{
	struct yt_maintenance_mercenary_defection_result local = {0};
	uint64_t starting_draws;
	int logical;

	if (game == NULL || sector_count < 1 || line_output == NULL
	    || result == NULL) {
		set_error(error, YT_INVALID, "Mercenary defections", "YTDATA.DAT");
		return false;
	}
	starting_draws = game->random.draws;
	for (logical = 1; logical <= sector_count; ++logical) {
		struct yt_sector sector;

		if (!yt_game_read_sector(game, logical, &sector, error))
			return false;
		if (sector.fighters > 0.0f) {
			float sample;

			if (!yt_random_next(&game->random, &sample, error))
				return false;
			if (sector.fighter_owner > 1.0f
			    && qb_single_multiply(sample, 100.0f) > sector.fighters) {
				char amount[64];
				char sector_text[64];
				uint8_t line[YT_MAINTENANCE_OUTPUT_ROW_SIZE];
				size_t line_length = 0U;
				size_t name_length;
				int amount_length;
				int sector_length;
				int32_t owner_record;
				int32_t stored_length;
				bool overflow;
				struct yt_player owner;
				static const uint8_t prefix[] = "  - ";
				static const uint8_t fighters[] =
				    " fighters in sector";
				static const uint8_t belonging[] = " belonging to ";
				static const uint8_t suffix[] = " joined the mercs!";

				owner_record = qb_cint_mbf32(sector.record.bytes + YT_F85,
				    0U, &overflow);
				if (overflow || owner_record < 1) {
					set_error(error, YT_RANGE,
					    "Mercenary defection owner", "YTDATA.DAT");
					return false;
				}
				sector.fighter_owner = -2.0f;
				if (!yt_record_set_number(&sector.record, YT_F85,
				    sector.fighter_owner)) {
					set_error(error, YT_RANGE,
					    "encode Mercenary defection", "YTDATA.DAT");
					return false;
				}
				if (!yt_database_write(&game->database,
				    (size_t)yt_sector_basic_record(&game->config, logical),
				    &sector.record, error)
				    || !yt_game_read_player(game, owner_record, &owner,
				    error))
					return false;
				stored_length = qb_cint_mbf32(owner.record.bytes + YT_F85, 0U,
				    &overflow);
				if (overflow || stored_length < 0) {
					set_error(error, YT_RANGE,
					    "Mercenary defection owner name", "YTDATA.DAT");
					return false;
				}
				name_length = (size_t)stored_length < YT_TEXT_FIELD_SIZE
				    ? (size_t)stored_length : YT_TEXT_FIELD_SIZE;
				amount_length = qb_str_single(amount, sizeof(amount),
				    sector.fighters);
				sector_length = qb_str_single(sector_text,
				    sizeof(sector_text), (float)logical);
				if (amount_length < 0 || sector_length < 0
				    || !maintenance_copy_part(line, sizeof(line),
				    &line_length, prefix, sizeof(prefix) - 1U)
				    || !maintenance_copy_part(line, sizeof(line),
				    &line_length, (const uint8_t *)amount,
				    (size_t)amount_length)
				    || !maintenance_copy_part(line, sizeof(line),
				    &line_length, fighters, sizeof(fighters) - 1U)
				    || !maintenance_copy_part(line, sizeof(line),
				    &line_length, (const uint8_t *)sector_text,
				    (size_t)sector_length)
				    || !maintenance_copy_part(line, sizeof(line),
				    &line_length, belonging, sizeof(belonging) - 1U)
				    || !maintenance_copy_part(line, sizeof(line),
				    &line_length, owner.record.bytes, name_length)
				    || !maintenance_copy_part(line, sizeof(line),
				    &line_length, suffix, sizeof(suffix) - 1U)
				    || !line_output(line_context, line, line_length, error)
				    || !yt_news_append_bytes(line, line_length,
				    error))
					return false;
				++local.defections;
			}
		}
	}
	local.draws_consumed = game->random.draws - starting_draws;
	*result = local;
	return true;
}

static bool
mercenary_mine_line(float value, int sector_number, int kind,
    uint8_t line[YT_MAINTENANCE_OUTPUT_ROW_SIZE], size_t *line_length)
{
	static const uint8_t hit_prefix[] = " ***";
	static const uint8_t hit_middle[] =
	    " mercenaries hit sector mines in sector";
	static const uint8_t lost_prefix[] = " *** Lost a total of";
	static const uint8_t lost_suffix[] = " fighters!";
	static const uint8_t killed[] = " *** The mercenariers were killed!";
	char number[64];
	char sector[64];
	int number_length;
	int sector_length;

	*line_length = 0U;
	if (kind == 1)
		return maintenance_copy_part(line,
		    YT_MAINTENANCE_OUTPUT_ROW_SIZE, line_length, killed,
		    sizeof(killed) - 1U);
	number_length = qb_str_single(number, sizeof(number), value);
	if (number_length < 0)
		return false;
	if (kind == 2)
		return maintenance_copy_part(line,
		    YT_MAINTENANCE_OUTPUT_ROW_SIZE, line_length, lost_prefix,
		    sizeof(lost_prefix) - 1U)
		    && maintenance_copy_part(line,
		    YT_MAINTENANCE_OUTPUT_ROW_SIZE, line_length,
		    (const uint8_t *)number, (size_t)number_length)
		    && maintenance_copy_part(line,
		    YT_MAINTENANCE_OUTPUT_ROW_SIZE, line_length, lost_suffix,
		    sizeof(lost_suffix) - 1U);
	sector_length = qb_str_single(sector, sizeof(sector),
	    (float)sector_number);
	return sector_length >= 0
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, hit_prefix, sizeof(hit_prefix) - 1U)
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, (const uint8_t *)number, (size_t)number_length)
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, hit_middle, sizeof(hit_middle) - 1U)
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, (const uint8_t *)sector, (size_t)sector_length)
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, (const uint8_t *)"!", 1U);
}

bool
yt_maintenance_mercenary_mines(struct yt_game *game, int sector_number,
    float moving_fighters, yt_maintenance_score_line_fn line_output,
    void *line_context, struct yt_sector *arrival_sector,
    struct yt_maintenance_mercenary_mine_result *result,
    struct yt_error *error)
{
	struct yt_maintenance_mercenary_mine_result local = {0};
	uint64_t starting_draws;
	uint8_t line[YT_MAINTENANCE_OUTPUT_ROW_SIZE];
	size_t line_length;
	int damage;

	if (game == NULL || sector_number < 1 || line_output == NULL
	    || arrival_sector == NULL || result == NULL) {
		set_error(error, YT_INVALID, "Mercenary mine arrival",
		    "YTDATA.DAT");
		return false;
	}
	local.moving_before = moving_fighters;
	local.survivors = moving_fighters;
	starting_draws = game->random.draws;
	if (!yt_game_read_sector(game, sector_number, arrival_sector, error))
		return false;
	if (arrival_sector->mines <= 0.0f || moving_fighters <= 0.0f) {
		*result = local;
		return true;
	}
	if (!yt_maintenance_nested_integer(&game->random, 2, 10000, &damage,
	    error))
		return false;
	if ((float)damage > moving_fighters)
		damage = (int)moving_fighters;
	local.draws_consumed = game->random.draws - starting_draws;
	if (damage <= 0) {
		*result = local;
		return true;
	}
	local.losses = (float)damage;
	local.survivors = qb_single_subtract(moving_fighters, local.losses);
	local.mine_hit = true;
	local.killed = local.survivors == 0.0f;
	if (!yt_game_read_sector(game, sector_number, arrival_sector, error))
		return false;
	arrival_sector->mines = qb_single_subtract(arrival_sector->mines, 1.0f);
	if (!yt_record_set_number(&arrival_sector->record, YT_F129,
	    arrival_sector->mines)) {
		set_error(error, YT_RANGE, "encode Mercenary mine",
		    "YTDATA.DAT");
		return false;
	}
	if (!yt_database_write(&game->database,
	    (size_t)yt_sector_basic_record(&game->config, sector_number),
	    &arrival_sector->record, error)
	    || !mercenary_mine_line(local.moving_before, sector_number, 0,
	    line, &line_length)
	    || !yt_news_append_bytes(line, line_length, error)
	    || !line_output(line_context, line, line_length, error))
		return false;
	if (!mercenary_mine_line(local.killed ? 0.0f : local.losses,
	    sector_number, local.killed ? 1 : 2, line, &line_length)
	    || !yt_news_append_bytes(line, line_length, error)
	    || !line_output(line_context, line, line_length, error))
		return false;
	*result = local;
	return true;
}

static bool
mercenary_planet_line(double mercenaries, float planet_fighters,
    const uint8_t *name, size_t name_length, bool taking,
    uint8_t line[YT_MAINTENANCE_OUTPUT_ROW_SIZE], size_t *line_length)
{
	static const uint8_t prefix[] = "  - ";
	static const uint8_t captured[] = " Mercenaries captured planet ";
	static const uint8_t taking_text[] = " Mercenaries taking";
	static const uint8_t fighters[] = " fighters from planet ";
	char first[64];
	char second[64];
	int first_length;
	int second_length = 0;

	*line_length = 0U;
	first_length = qb_str_double(first, sizeof(first), mercenaries);
	if (taking)
		second_length = qb_str_single(second, sizeof(second),
		    planet_fighters);
	return first_length >= 0 && second_length >= 0
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, prefix, sizeof(prefix) - 1U)
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, (const uint8_t *)first, (size_t)first_length)
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, taking ? taking_text : captured,
	    taking ? sizeof(taking_text) - 1U : sizeof(captured) - 1U)
	    && (!taking || maintenance_copy_part(line,
	    YT_MAINTENANCE_OUTPUT_ROW_SIZE, line_length,
	    (const uint8_t *)second, (size_t)second_length))
	    && (!taking || maintenance_copy_part(line,
	    YT_MAINTENANCE_OUTPUT_ROW_SIZE, line_length, fighters,
	    sizeof(fighters) - 1U))
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, name, name_length)
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, (const uint8_t *)"!", 1U);
}

bool
yt_maintenance_mercenary_planet_absorption(struct yt_game *game,
    int sector_number, int selected_destination, double moving_fighters,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_sector *arrival_sector,
    struct yt_maintenance_mercenary_planet_result *result,
    struct yt_error *error)
{
	static const uint8_t planet_fighter_zero[4] = {0x00, 0x00, 0x80, 0x00};
	struct yt_maintenance_mercenary_planet_result local = {0};
	struct yt_planet planet;
	uint8_t line[YT_MAINTENANCE_OUTPUT_ROW_SIZE];
	size_t line_length;
	int32_t planet_number;
	int32_t name_length;
	bool overflow;
	double incoming;
	double existing;

	if (game == NULL || sector_number < 1 || line_output == NULL
	    || arrival_sector == NULL || result == NULL) {
		set_error(error, YT_INVALID, "Mercenary planet arrival",
		    "YTDATA.DAT");
		return false;
	}
	planet_number = qb_cint_mbf32(arrival_sector->record.bytes + YT_F93,
	    0U, &overflow);
	if (overflow) {
		set_error(error, YT_RANGE, "Mercenary planet link", "YTDATA.DAT");
		return false;
	}
	if ((arrival_sector->fighter_owner != -2.0f
	    && arrival_sector->fighter_owner != 0.0f)
	    || planet_number == 0) {
		*result = local;
		return true;
	}
	if (!yt_game_read_planet(game, planet_number, &planet, error))
		return false;
	name_length = qb_cint_mbf32(planet.record.bytes + YT_F85, 0U,
	    &overflow);
	if (overflow || name_length < 0 || name_length > 41) {
		set_error(error, YT_RANGE, "Mercenary planet name", "YTDATA.DAT");
		return false;
	}
	local.planet_fighters = (float)qb_int((double)planet.fighters);
	planet.fighters = 0.0f;
	if (!yt_record_set_raw_number(&planet.record, YT_F129,
	    planet_fighter_zero)) {
		set_error(error, YT_RANGE, "encode Mercenary planet fighters",
		    "YTDATA.DAT");
		return false;
	}
	if (!yt_database_write(&game->database,
	    (size_t)yt_planet_basic_record(&game->config, planet_number),
	    &planet.record, error)
	    || !yt_game_read_sector(game, sector_number, arrival_sector, error))
		return false;
	existing = (double)arrival_sector->fighters;
	incoming = (double)local.planet_fighters + moving_fighters;
	local.sector_fighters = (float)(incoming + existing);
	arrival_sector->fighters = local.sector_fighters;
	arrival_sector->fighter_owner = -2.0f;
	if (!yt_record_set_number(&arrival_sector->record, YT_F81,
	    arrival_sector->fighters)
	    || !yt_record_set_number(&arrival_sector->record, YT_F85, -2.0f)) {
		set_error(error, YT_RANGE, "encode Mercenary planet sector",
		    "YTDATA.DAT");
		return false;
	}
	if (!yt_database_write(&game->database,
	    (size_t)yt_sector_basic_record(&game->config, sector_number),
	    &arrival_sector->record, error))
		return false;
	local.absorbed = true;
	local.capture_report = selected_destination != sector_number
	    && moving_fighters != 0.0;
	local.taking_report = local.planet_fighters > 0.0f;
	if (local.capture_report
	    && (!mercenary_planet_line(incoming, local.planet_fighters,
	    planet.record.bytes, (size_t)name_length, false, line, &line_length)
	    || !line_output(line_context, line, line_length, error)
	    || !yt_news_append_bytes(line, line_length, error)))
		return false;
	if (local.taking_report
	    && (!mercenary_planet_line(moving_fighters + existing,
	    local.planet_fighters, planet.record.bytes, (size_t)name_length,
	    true, line, &line_length)
	    || !yt_news_append_bytes(line, line_length, error)
	    || !line_output(line_context, line, line_length, error)))
		return false;
	*result = local;
	return true;
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
	return first_length >= 0 && second_length >= 0
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, prefix, sizeof(prefix) - 1U)
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, (const uint8_t *)first, (size_t)first_length)
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, attack, sizeof(attack) - 1U)
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, (const uint8_t *)second, (size_t)second_length)
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, belonging, sizeof(belonging) - 1U)
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, owner, owner_length)
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
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
	return count_length >= 0 && sector_length >= 0
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, prefix, sizeof(prefix) - 1U)
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, (const uint8_t *)count, (size_t)count_length)
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, joined, sizeof(joined) - 1U)
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, owner, owner_length)
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, defense, sizeof(defense) - 1U)
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, (const uint8_t *)sector, (size_t)sector_length)
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, (const uint8_t *)"!", 1U)
	    && maintenance_copy_part(radio, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    radio_length, (const uint8_t *)count, (size_t)count_length)
	    && maintenance_copy_part(radio, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    radio_length, radio_middle, sizeof(radio_middle) - 1U)
	    && maintenance_copy_part(radio, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    radio_length, (const uint8_t *)sector, (size_t)sector_length)
	    && maintenance_copy_part(radio, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    radio_length, (const uint8_t *)"!", 1U);
}

static bool
mercenary_destination_impl(struct yt_game *game,
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
	float original_owner;
	int32_t owner_record = 0;
	int32_t stored_length;
	bool overflow;

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
	if (original_owner == -2.0f || original_owner == 0.0f) {
		if (!yt_game_read_sector(game, sector_number, &fresh, error))
			return false;
		fresh.fighters = (float)((double)fresh.fighters + moving);
		fresh.fighter_owner = -2.0f;
		if (!yt_game_write_sector(game, sector_number, &fresh, error))
			return false;
		*arrival_sector = fresh;
		*moving_after = fresh.fighters;
		*continues = true;
		return true;
	}
	if (original_owner == -1.0f) {
		memcpy(owner_name, xannor, sizeof(xannor) - 1U);
		owner_length = sizeof(xannor) - 1U;
	}
	else {
		owner_record = qb_cint_mbf32(arrival_sector->record.bytes + YT_F85,
		    0U, &overflow);
		if (overflow || owner_record < 1
		    || !yt_game_read_player(game, owner_record, &owner_player,
		    error)) {
			if (!overflow && owner_record < 1)
				set_error(error, YT_RANGE,
				    "Mercenary destination owner", "YTDATA.DAT");
			return false;
		}
		stored_length = qb_cint_mbf32(owner_player.record.bytes + YT_F85, 0U,
		    &overflow);
		if (overflow || stored_length < 0) {
			set_error(error, YT_RANGE,
			    "Mercenary destination owner name", "YTDATA.DAT");
			return false;
		}
		owner_length = (size_t)stored_length < YT_TEXT_FIELD_SIZE
		    ? (size_t)stored_length : YT_TEXT_FIELD_SIZE;
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
			if (!line_output(line_context, line, line_length, error)
			    || !yt_news_append_bytes(line, line_length, error)
			    || !yt_radio_append_maintenance_bytes(radio, radio_length,
			    -2.0f, original_owner, error)
			    || !yt_game_read_sector(game, sector_number, &fresh, error))
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
	if (!line_output(line_context, line, line_length, error)
	    || !yt_news_append_bytes(line, line_length, error))
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

		if (!line_output(line_context, result_line, result_length, error)
		    || !yt_news_append_bytes(result_line, result_length, error)
		    || !yt_game_read_sector(game, sector_number, &fresh, error))
			return false;
	}
	if (defenders > 0.0) {
		fresh.fighters = (float)defenders;
		fresh.fighter_owner = original_owner;
	}
	else {
		fresh.fighters = 0.0f;
		fresh.fighter_owner = 0.0f;
	}
	if (!yt_game_write_sector(game, sector_number, &fresh, error))
		return false;
	if (moving > 0.0) {
		if (!yt_game_read_sector(game, sector_number, &fresh, error))
			return false;
		fresh.fighters = (float)((double)fresh.fighters + moving);
		fresh.fighter_owner = -2.0f;
		if (!yt_game_write_sector(game, sector_number, &fresh, error))
			return false;
	}
	*arrival_sector = fresh;
	*moving_after = (float)moving;
	*continues = moving > 0.0;
	return true;
}

bool
yt_maintenance_mercenary_destination(struct yt_game *game,
    int sector_number, float moving_fighters,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_sector *arrival_sector, struct yt_error *error)
{
	float moving_after;
	bool continues;

	return mercenary_destination_impl(game, sector_number, moving_fighters,
	    line_output, line_context, arrival_sector, &moving_after, &continues,
	    error);
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
	struct yt_maintenance_mercenary_mine_result mines;
	struct yt_maintenance_mercenary_planet_result planet;

	if (moving == NULL || arrival_result == NULL) {
		set_error(error, YT_INVALID, "Mercenary routed arrival",
		    "YTDATA.DAT");
		return false;
	}
	if (!yt_maintenance_mercenary_mines(game, sector_number,
	    *moving, line_output, line_context, &sector, &mines, error))
		return false;
	*moving = mines.survivors;
	if (mines.killed) {
		*arrival_result = MERCENARY_ARRIVAL_TERMINAL;
		return true;
	}
	if (!yt_maintenance_mercenary_planet_absorption(game,
	    sector_number, selected_destination, (double)*moving,
	    line_output, line_context, &sector, &planet, error))
		return false;
	if (planet.absorbed) {
		*moving = 0.0f;
		*arrival_result = MERCENARY_ARRIVAL_TERMINAL;
		return true;
	}
	if (*moving <= 0.0f || sector.planet != 0.0f) {
		*arrival_result = MERCENARY_ARRIVAL_CONTINUE;
		return true;
	}
	{
		bool continues;

		if (!mercenary_destination_impl(game, sector_number, *moving,
		    line_output, line_context, &sector, moving, &continues, error))
			return false;
		*arrival_result = continues ? MERCENARY_ARRIVAL_CONTINUE
		    : MERCENARY_ARRIVAL_TERMINAL;
	}
	return true;
}

static bool
move_mercenaries_impl(struct yt_game *game, int sector_count,
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
		if (sector.fighter_owner != -2.0f || sector.fighters <= 0.0f)
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
		sector.fighter_owner = 0.0f;
		if (!yt_game_write_sector(game, origin, &sector, error))
			return false;
		do {
			if (!yt_maintenance_random_integer(&game->random,
			    sector_count, &target, error))
				return false;
		} while (target == origin);
		if (!yt_maintenance_route_next_hop(game,
		    route_cache, origin, target, &next, error)
		    || !yt_maintenance_compose_mercenary_movement((double)moving,
		    (float)origin, &output)
		    || !maintenance_emit_output_row(&output, 0x4911U,
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
					destination.fighter_owner = -2.0f;
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

bool
yt_maintenance_move_mercenaries(struct yt_game *game, int sector_count,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_error *error)
{
	struct yt_maintenance_route_cache route_cache = {0};
	bool result;

	if (game == NULL || sector_count < 1 || line_output == NULL) {
		set_error(error, YT_INVALID, "Mercenary movement", "YTDATA.DAT");
		return false;
	}
	result = move_mercenaries_impl(game, sector_count, &route_cache,
	    line_output, line_context, error);
	yt_maintenance_route_cache_free(&route_cache);
	return result;
}

static bool
maintain_mercenaries_impl(struct maint_state *state,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_error *error)
{
	static const char report[] = "  -  Mercenary Report:";
	struct yt_maintenance_mercenary_tax_result tax;
	struct yt_maintenance_mercenary_defection_result defections;
	struct yt_maintenance_output_result output;
	bool rebuilt;
	float hired;

	if (!yt_maintenance_compose_mercenary_phase(
	    NULL, 0U,
	    0.0f, false, 0.0f, &output)
	    || !maintenance_emit_output_row(&output, 0x3F86U,
	    line_output, line_context, error)
	    || !maintenance_emit_output_row(&output, 0x3F95U,
	    line_output, line_context, error)
	    || !yt_maintenance_collect_mercenary_tax(&state->game,
	    state->port_count, &tax, error)
	    || !yt_maintenance_compose_mercenary_phase(
	    NULL, 0U,
	    tax.tax_pool, false, 0.0f, &output))
		return false;
	if (tax.tax_pool != 0.0f
	    && (!maintenance_emit_output_row(&output, 0x40BCU,
	    line_output, line_context, error)
	    || !maintenance_news_output_row(&output, 0x40BCU, error)))
		return false;
	if (!maintenance_emit_output_row(&output, 0x40DEU,
	    line_output, line_context, error)
	    || !maintenance_emit_output_row(&output, 0x40F2U,
	    line_output, line_context, error)
	    || !maintenance_emit_output_row(&output, 0x4103U,
	    line_output, line_context, error)
	    || !yt_news_append(report, error)
	    || !maintenance_emit_output_row(&output, 0x4130U,
	    line_output, line_context, error)
	    || !yt_maintenance_maintain_mercenary_base(&state->game,
	    state->sector_count, state->planet_count - 1, &rebuilt, error))
		return false;
	if (rebuilt) {
		if (!yt_maintenance_compose_mercenary_phase(
		    NULL, 0U, tax.tax_pool, true, 0.0f,
		    &output)
		    || !maintenance_emit_output_row(&output, 0x41DEU,
		    line_output, line_context, error)
		    || !maintenance_emit_output_row(&output, 0x41FAU,
		    line_output, line_context, error)
		    || !maintenance_news_output_row(&output, 0x41FAU, error))
			return false;
	}
	if (!yt_maintenance_place_mercenary_fleets(&state->game,
	    state->sector_count, tax.fleet_strength, &hired, error))
		return false;
	if (hired != 0.0f
	    && (!yt_maintenance_compose_mercenary_phase(
	    NULL, 0U,
	    tax.tax_pool, rebuilt, hired, &output)
	    || !maintenance_emit_output_row(&output, 0x4631U,
	    line_output, line_context, error)
	    || !maintenance_news_output_row(&output, 0x4631U, error)))
		return false;
	if (!yt_maintenance_mercenary_defections(&state->game,
	    state->sector_count, line_output, line_context, &defections,
	    error)
	    || !move_mercenaries_impl(&state->game, state->sector_count,
	    &state->route_cache, line_output, line_context, error))
		return false;
	return true;
}

bool
yt_maintenance_maintain_mercenaries(struct yt_game *game,
    struct yt_maintenance_route_cache *cache,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_error *error)
{
	struct maint_state state;
	bool success;

	if (game == NULL || cache == NULL || line_output == NULL) {
		set_error(error, YT_INVALID, "Mercenary maintenance",
		    "YTDATA.DAT");
		return false;
	}
	memset(&state, 0, sizeof(state));
	state.game = *game;
	state.route_cache = *cache;
	state.sector_count = (int)(game->config.port_offset
	    - game->config.sector_offset);
	state.port_count = (int)(game->config.planet_offset
	    - game->config.port_offset);
	state.planet_count = (int)(game->config.total_records
	    - game->config.planet_offset);
	if (state.sector_count < 2 || state.port_count < 0
	    || state.planet_count < 2) {
		set_error(error, YT_RANGE, "Mercenary maintenance layout",
		    "YTDATA.DAT");
		return false;
	}
	success = maintain_mercenaries_impl(&state, line_output, line_context,
	    error);
	game->random = state.game.random;
	*cache = state.route_cache;
	return success;
}

static bool
maintain_factions(struct maint_state *state, struct yt_error *error)
{
	return maintain_xannor_impl(state, maintenance_stdout_line, NULL, error)
	    && maintain_mercenaries_impl(state, maintenance_stdout_line, NULL,
	    error);
}

static bool
lottery_fail(yt_maintenance_score_line_fn line_output, void *line_context,
    enum yt_maintenance_lottery_failure failure, uint64_t starting_draws,
    struct yt_game *game, struct yt_maintenance_lottery_result *result,
    struct yt_error *error)
{
	static const uint8_t no_winner[] = "No one won a planet today.";

	result->failure = failure;
	result->draws_consumed = game->random.draws - starting_draws;
	return line_output(line_context, no_winner,
	    sizeof(no_winner) - 1U, error);
}

bool
yt_maintenance_super_lottery(struct yt_game *game, int player_count,
    int planet_count, int sector_count, const uint8_t *blank,
    size_t blank_length, yt_maintenance_score_line_fn line_output,
    void *line_context, struct yt_maintenance_lottery_result *result,
    struct yt_error *error)
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
	struct yt_maintenance_lottery_result local = {0};
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
	uint64_t starting_draws;
	int32_t player_name_length;
	int player_slot;
	int index;
	int sector_length;
	float gate;
	bool overflow;

	if (game == NULL || player_count < 1 || planet_count < 1
	    || sector_count < 1 || (blank == NULL
	    && blank_length != 0U) || line_output == NULL
	    || result == NULL) {
		set_error(error, YT_INVALID, "Super Lottery", "YTDATA.DAT");
		return false;
	}
	memset(result, 0, sizeof(*result));
	starting_draws = game->random.draws;
	if (!line_output(line_context, blank,
	    blank_length, error)
	    || !line_output(line_context, phase,
	    sizeof(phase) - 1U, error)
	    || !yt_random_next(&game->random, &gate, error))
		return false;
	if (gate < 0.5f)
		return lottery_fail(line_output, line_context,
		    YT_MAINTENANCE_LOTTERY_COIN, starting_draws, game, result,
		    error);
	if (!yt_maintenance_random_integer(&game->random, player_count,
	    &player_slot, error))
		return false;
	local.player_record = player_slot + 1;
	if (!yt_game_read_player(game, local.player_record, &player, error))
		return false;
	if (qb_mbf32_decode(player.record.bytes + YT_F85) == 0.0f) {
		local.failure = YT_MAINTENANCE_LOTTERY_BLANK_PLAYER;
		*result = local;
		return lottery_fail(line_output, line_context, local.failure,
		    starting_draws, game, result, error);
	}
	player_name_length = qb_cint_mbf32(player.record.bytes + YT_F85, 0U,
	    &overflow);
	if (overflow || player_name_length < 0 || player_name_length > 41) {
		set_error(error, YT_RANGE, "Super Lottery player name",
		    "YTDATA.DAT");
		return false;
	}
	working_length = (size_t)player_name_length + sizeof(name_suffix) - 1U;
	memcpy(working_name, player.record.bytes, (size_t)player_name_length);
	memcpy(working_name + player_name_length, name_suffix,
	    sizeof(name_suffix) - 1U);
	if (!yt_maintenance_random_integer(&game->random, planet_count,
	    &local.planet_number, error)
	    || !yt_game_read_planet(game, local.planet_number, &planet, error))
		return false;
	if (qb_mbf32_decode(planet.record.bytes + YT_F85) != 0.0f) {
		local.failure = YT_MAINTENANCE_LOTTERY_OCCUPIED_PLANET;
		*result = local;
		return lottery_fail(line_output, line_context, local.failure,
		    starting_draws, game, result, error);
	}
	if (!yt_maintenance_random_integer(&game->random, sector_count,
	    &local.sector_number, error)
	    || !yt_game_read_sector(game, local.sector_number, &sector, error))
		return false;
	if (sector.planet > 0.0f) {
		local.failure = YT_MAINTENANCE_LOTTERY_OCCUPIED_SECTOR;
		*result = local;
		return lottery_fail(line_output, line_context, local.failure,
		    starting_draws, game, result, error);
	}
	/* The constructor performs a second, fresh GET of the selected planet. */
	if (!yt_game_read_planet(game, local.planet_number, &planet, error))
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

		if (!yt_random_next(&game->random, &first, error)
		    || !yt_random_next(&game->random, &second, error))
			return false;
		production = qb_single_multiply(qb_single_multiply(first, second), 3000.0f);
		if (!yt_record_set_number(&planet.record,
		    YT_F45 + (size_t)index * 4U, production)
		    || !yt_record_set_raw_number(&planet.record,
		    YT_F57 + (size_t)index * 4U, dirty_zero))
			return false;
	}
	if (!yt_record_set_raw_number(&planet.record, YT_F69, dirty_zero)
	    || !yt_record_set_number(&planet.record, YT_F73,
	    (float)local.player_record)
	    || !yt_random_next(&game->random, &gate, error)
	    || !yt_record_set_number(&planet.record, YT_F77,
	    yt_maintenance_sint(qb_single_add(
	    qb_single_multiply(gate, 100.0f), 1.0f)))
	    || !yt_random_next(&game->random, &gate, error)
	    || !yt_record_set_number(&planet.record, YT_F117,
	    qb_single_multiply(gate, 16000000.0f))
	    || !yt_record_set_raw_number(&planet.record, YT_F125,
	    canonical_zero))
		return false;
	if (!yt_database_write(&game->database,
	    (size_t)yt_planet_basic_record(&game->config, local.planet_number),
	    &planet.record, error)
	    || !yt_game_read_sector(game, local.sector_number, &sector, error))
		return false;
	sector.planet = (float)local.planet_number;
	if (!yt_record_set_number(&sector.record, YT_F93, sector.planet)
	    || !yt_database_write(&game->database,
	    (size_t)yt_sector_basic_record(&game->config, local.sector_number),
	    &sector.record, error))
		return false;
	line_length = 0U;
	if (!maintenance_copy_part(line, sizeof(line), &line_length,
	    winner_prefix, sizeof(winner_prefix) - 1U)
	    || !maintenance_copy_part(line, sizeof(line), &line_length,
	    player.record.bytes, (size_t)player_name_length)
	    || !maintenance_copy_part(line, sizeof(line), &line_length,
	    winner_suffix, sizeof(winner_suffix) - 1U)
	    || !line_output(line_context, line, line_length, error)
	    || !yt_news_append_bytes(line, line_length, error))
		return false;
	sector_length = qb_str_single(sector_text, sizeof(sector_text),
	    (float)local.sector_number);
	radio_length = 0U;
	if (sector_length < 0
	    || !maintenance_copy_part(radio, sizeof(radio), &radio_length,
	    radio_prefix, sizeof(radio_prefix) - 1U)
	    || !maintenance_copy_part(radio, sizeof(radio), &radio_length,
	    (const uint8_t *)sector_text, (size_t)sector_length)
	    || !maintenance_copy_part(radio, sizeof(radio), &radio_length,
	    (const uint8_t *)"!\a", 2U)
	    || !yt_radio_append_maintenance_bytes(radio, radio_length, -2.0f,
	    (float)local.player_record, error))
		return false;
	local.failure = YT_MAINTENANCE_LOTTERY_SUCCESS;
	local.draws_consumed = game->random.draws - starting_draws;
	*result = local;
	return true;
}
