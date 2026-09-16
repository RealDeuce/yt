#include "yt_maint.h"

#include "yt_maint_internal.h"

#include "qb.h"

#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static float xannor_quantum(float first, float second);

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
		    || !yt_random_nested_integer(random, 4, range, &split,
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
		if (!yt_random_integer(&game->random, sector_count,
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

		if (!yt_random_integer(&game->random, sector_count,
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
		if (!yt_random_integer(random, sector_count - 7,
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
		int record = (int)host.fighter_owner;

		if (!yt_game_read_player(game, record, &player, error)) {
			if (error != NULL && error->status == YT_OK)
				set_error(error, YT_RANGE,
				    "Xannor headquarters defender", "YTDATA.DAT");
			return false;
		}
		opponent.data = player.record.bytes;
		opponent.length = player.name_length < YT_TEXT_FIELD_SIZE
		    ? player.name_length : YT_TEXT_FIELD_SIZE;
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
		if (!yt_random_integer(&game->random,
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
		if (!yt_current_date_serial(&game->clock, game->config.epoch_year,
		    &today, NULL,
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
		minute = yt_maintenance_sint(qb_single_divide(
		    (float)yt_clock_timer(&game->clock), 60.0f));
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
	float gate;
	float selection;
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
		if (player.name_length != 0U
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
	name.data = player.record.bytes;
	name.length = player.name_length < YT_TEXT_FIELD_SIZE
	    ? player.name_length : YT_TEXT_FIELD_SIZE;
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

		if (!yt_random_integer(&game->random, 1000,
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
		int record = (int)initial_owner;

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
	if (planet.name_length == 0U
	    || planet.owner == -1.0f)
		return true;
	if (!yt_game_read_planet(game, planet_number, &planet, error))
		return false;
	stored_name_length = yt_planet_stored_name(&planet, stored_name);
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
	if (player.name_length == 0U
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
	if (!yt_game_read_player(game, player_record, &player, error))
		return false;
	stored_name_length = yt_player_stored_name(&player, stored_name);
	if (!yt_maintenance_xannor_player_line_bytes(stored_name,
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

bool
yt_maintenance_xannor_route_arrivals(struct maint_state *state, int group,
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
				    || !maintenance_emit_output_row(&output, YT_MAINT_ROW_XANNOR_PATH_ERROR,
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

bool
yt_maintenance_xannor_roaming_groups(struct maint_state *state, float score,
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
				    YT_MAINT_ROW_XANNOR_GROUP_REPORT, line_output, line_context, error))
					return false;
				if (!yt_maintenance_xannor_route_arrivals(state, group,
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
yt_maintenance_xannor_run(struct maint_state *state,
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

	return yt_maintenance_xannor_roaming_groups(state, score, top_target,
	    hunt_player,
	    revenge_live, revenge_cached, location, size,
	    line_output, line_context, error);
}
