#include "yt_maint.h"

#include "yt_maint_internal.h"

#include "qb.h"

#include <errno.h>
#include <math.h>
#include <stdlib.h>
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

static float
xannor_quantum(float first, float second)
{
	float quantum = first > 5000.0f && second > 5000.0f
	    ? 5000.0f : 500.0f;

	if (first < 500.0f || second < 500.0f)
		quantum = 1.0f;
	return quantum;
}


bool
yt_maintenance_xannor_roaming_split(struct yt_random *random,
    int group_number, float *group_one, float *group_size,
    float *group_location, float top_score, float headquarters,
    bool *skip_group,
    struct yt_error *error)
{
	bool overflow;
	int range;
	int split;

	if (random == NULL || group_one == NULL || group_size == NULL
	    || group_location == NULL || skip_group == NULL
	    || group_number < 2 || group_number > 20
	    || *group_one < 0.0f || *group_size < 0.0f
	    || top_score < 0.0f) {
		set_error(error, YT_INVALID, "Xannor roaming split", "");
		return false;
	}
	*skip_group = false;
	if (*group_size >= 1.0f && *group_location >= 1.0f)
		return true;
	if (*group_one < qb_single_divide(top_score, 2000.0f)) {
		*skip_group = true;
		return true;
	}
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
	return true;
}

bool
yt_maintenance_xannor_candidate_discovery(struct yt_game *game,
    const int *player_sector, const float *player_cloak,
    size_t cache_count, int current_sector, int revenge_live_sector,
    int revenge_cached_target, int *target_sector,
    struct yt_error *error)
{
	int sector_count;
	int attempt_limit;
	int initial_target;
	int discovery_target;
	int attempt;

	if (game == NULL || player_sector == NULL || player_cloak == NULL
	    || target_sector == NULL || cache_count <= 2U
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
	do {
		if (!yt_random_integer(&game->random, sector_count,
		    &initial_target, error))
			return false;
	} while (initial_target == current_sector);
	discovery_target = revenge_cached_target;
	attempt_limit = revenge_live_sector != 0 ? 25 : 1;
	for (attempt = 0; attempt < attempt_limit; ++attempt) {
		struct yt_sector sector;
		int candidate;
		size_t player;

		if (!yt_random_integer(&game->random, sector_count,
		    &candidate, error)
		    || !yt_game_read_sector(game, candidate, &sector, error))
			return false;
		if ((sector.fighters > 1.0f
		    && sector.fighter_owner != -1.0f)
		    || sector.planet > 1)
			discovery_target = candidate;
		if (discovery_target == 0) {
			for (player = 2U; player < cache_count; ++player) {
				float cloak_draw;

				if (!yt_random_next(&game->random, &cloak_draw, error))
					return false;
				if (player_sector[player] == candidate
				    && (cloak_draw > player_cloak[player]
				    || revenge_live_sector != 0)) {
					discovery_target = player_sector[player];
					break;
				}
			}
		}
		if (discovery_target != 0)
			break;
	}
	*target_sector = discovery_target > 7
	    ? discovery_target : initial_target;
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
    float group_location, int player_location, float cached_cloak,
    float cloak_draw)
{
	float threshold = qb_single_subtract(cached_cloak, 0.33000001311302185f);

	return (float)player_location == group_location && group_number != 20
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
    int *hunt_player, int *target_sector,
    struct yt_error *error)
{
	int selected;

	if (random == NULL || hunt_player == NULL || target_sector == NULL
	    || sector_count < 8) {
		set_error(error, YT_INVALID, "Xannor target", "YTDATA.DAT");
		return false;
	}
	if (*target_sector < 8 || *target_sector > sector_count
	    || *hunt_player == 0) {
		if (!yt_random_integer(random, sector_count - 7,
		    &selected, error))
			return false;
		*hunt_player = 0;
		*target_sector = selected + 7;
	}
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
yt_maintenance_xannor_player_line_bytes(const uint8_t *player_name,
    size_t player_name_length, float player_fighter_losses,
    float xannor_fighter_losses,
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
	    || line == NULL || line_length == NULL)
		return false;
	*line_length = 0U;
	player_length = qb_str_double(player_losses, sizeof(player_losses),
	    (double)player_fighter_losses);
	xannor_length = qb_str_double(xannor_losses, sizeof(xannor_losses),
	    (double)xannor_fighter_losses);
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
yt_maintenance_xannor_regeneration(float top_score, float size[21],
    double *regeneration)
{
	volatile float converted;
	float regeneration_single;
	float total = 0.0f;
	float ceiling;
	int group;

	if (size == NULL || regeneration == NULL)
		return false;
	for (group = 1; group <= 20; ++group)
		total = qb_single_add(total, size[group]);
	regeneration_single = yt_maintenance_sint(
	    qb_single_divide(top_score, 500.0f));
	*regeneration = (double)regeneration_single;
	ceiling = yt_maintenance_sint(
	    qb_single_divide(top_score, 100.0f));
	if (total > ceiling)
		*regeneration = 0.0;
	converted = (float)((double)size[1] + *regeneration);
	size[1] = converted;
	return true;
}

bool
yt_maintenance_xannor_headquarters_reclaim(struct yt_game *game,
    float location[21], float size[21], yt_maintenance_score_line_fn line_output,
    void *line_context, bool *original_hostile,
    struct yt_error *error)
{
	static const uint8_t mercenaries[] = "The Mercenaries";
	struct yt_maintenance_output_result output;
	struct yt_maintenance_text opponent = {
		mercenaries, sizeof(mercenaries) - 1U
	};
	struct yt_sector host;
	struct yt_player player;
	double defenders;
	bool successful;
	int hq;

	if (game == NULL || location == NULL || size == NULL
	    || line_output == NULL || original_hostile == NULL) {
		set_error(error, YT_INVALID, "Xannor headquarters reclaim",
		    "YTDATA.DAT");
		return false;
	}
	hq = (int)game->config.headquarters;
	if (hq < 1
	    || !yt_game_read_sector(game, hq, &host, error)) {
		if (error != NULL && error->status == YT_OK)
			set_error(error, YT_RANGE, "Xannor headquarters",
			    "YTDATA.DAT");
		return false;
	}
	defenders = (double)host.fighters;
	*original_hostile = host.fighters > 0.0f
	    && host.fighter_owner != -1.0f;
	if (!*original_hostile || size[1] <= 0.0f)
		return true;
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
	successful = defenders <= 0.0;
	if (!yt_game_read_sector(game, hq, &host, error))
		return false;
	if (successful) {
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
	    successful, &output)
	    || !line_output(line_context, output.rows[0].data,
	    output.rows[0].length, error)
	    || !yt_news_append_bytes(output.rows[0].data,
	    output.rows[0].length, error))
		return false;
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
    void *line_context, struct yt_error *error)
{
	struct yt_maintenance_output_result output;
	struct yt_record config_record;
	struct yt_sector sector;
	int planet_number;
	int old_logical;
	int sector_count;
	int candidate;
	bool triggered;

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
	triggered = (!original_hostile
	    && (double)group_one == regeneration)
	    || (original_hostile && group_one > 0.0f);
	if (!triggered)
		return true;
	for (;;) {
		if (!yt_random_integer(&game->random,
		    sector_count - 7, &candidate, error))
			return false;
		candidate += 7;
		if (!yt_game_read_sector(game, candidate, &sector, error))
			return false;
		if (!((sector.fighters > 1.0f
		    && sector.fighter_owner != -1.0f)
		    || sector.planet > 1))
			break;
	}
	old_logical = (int)game->config.headquarters;
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
	if (old_logical < 1
	    || !yt_game_read_sector(game, old_logical, &sector, error)) {
		if (error != NULL && error->status == YT_OK)
			set_error(error, YT_RANGE, "old Xannor headquarters",
			    "YTDATA.DAT");
		return false;
	}
	planet_number = (int)qb_single_subtract(game->config.total_records,
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
	    || !yt_record_set_number(&sector.record, YT_F93,
	    (float)planet_number)
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
	return true;
}
bool
yt_maintenance_xannor_revenge_slot(struct yt_game *game,
    const int *player_sector, size_t cache_count,
    const uint8_t *blank, size_t blank_length,
    yt_maintenance_score_line_fn line_output, void *line_context,
    int *live_sector, int *cached_target,
    struct yt_error *error)
{
	static const uint8_t dirty_zero[4] = {0x00, 0x00, 0x28, 0x00};
	struct yt_maintenance_output_result output;
	struct yt_sector metadata;
	float slot_value;
	int record;

	if (game == NULL || player_sector == NULL || line_output == NULL
	    || live_sector == NULL || cached_target == NULL
	    || (blank == NULL && blank_length != 0U)) {
		set_error(error, YT_INVALID, "Xannor revenge slot", "YTDATA.DAT");
		return false;
	}
	*live_sector = 0;
	*cached_target = 0;
	if (!yt_game_read_sector(game, 21, &metadata, error))
		return false;
	slot_value = metadata.metadata;
	if (slot_value > 0.0f) {
		struct yt_player player;

		record = (int)slot_value;
		if (record < 0 || (size_t)record >= cache_count) {
			set_error(error, YT_RANGE, "Xannor revenge player",
			    "YTDATA.DAT");
			return false;
		}
		if (!yt_game_read_player(game, record, &player, error))
			return false;
		if (player.sector > 7) {
			*live_sector = player.sector;
			*cached_target = player_sector[record];
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
	if (!yt_record_set_number(&sector->record, YT_F93,
	    (float)sector->planet)) {
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
    struct yt_error *error)
{
	struct yt_maintenance_output_result output;
	struct yt_sector sector;
	struct yt_planet planet;
	float sample;
	float minute;
	bool rebuilt;
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
	for (row = 0U; row < output.row_count; ++row) {
		if (!line_output(line_context, output.rows[row].data,
		    output.rows[row].length, error))
			return false;
	}
	if (!yt_game_read_sector(game, headquarters, &sector, error))
		return false;
	rebuilt = sector.planet == 0;
	if (rebuilt) {
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
		sector.planet = planet_count;
		if (!maintenance_write_xannor_sector(game, headquarters,
		    &sector, error))
			return false;
	}
	if (!yt_game_read_planet(game, planet_count, &planet, error))
		return false;
	if (!yt_random_next(&game->random, &sample, error))
		return false;
	planet.ground_forces = qb_single_add(planet.ground_forces,
	    yt_maintenance_sint(qb_single_multiply(sample, 25.0f)));
	planet.owner = -1.0f;
	if (planet.bank == 0.0f)
		planet.bank = 16000000.0f;
	if (!maintenance_write_xannor_daily(game, planet_count, &planet,
	    error))
		return false;
	return true;
}

bool
yt_maintenance_xannor_hunt(struct yt_game *game,
    const int *player_sector, const float *player_cloak, size_t cache_count,
    const uint8_t *blank, size_t blank_length,
    yt_maintenance_score_line_fn line_output, void *line_context,
    int *hunt_player, float *top_score, int *target_sector,
    struct yt_error *error)
{
	struct yt_maintenance_output_result output;
	struct yt_maintenance_text name;
	struct yt_player player;
	float gate;
	float selection;
	int player_count;
	int top_record = 0;
	int candidate;
	size_t row;

	if (game == NULL || player_sector == NULL || player_cloak == NULL
	    || line_output == NULL || hunt_player == NULL || top_score == NULL
	    || target_sector == NULL || (blank == NULL
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
	*hunt_player = 0;
	*top_score = 0.0f;
	*target_sector = 0;
	for (row = 0U; row < output.row_count; ++row) {
		if (!line_output(line_context, output.rows[row].data,
		    output.rows[row].length, error))
			return false;
	}
	for (candidate = 2; candidate <= player_count + 1; ++candidate) {
		if (!yt_game_read_player(game, candidate, &player, error))
			return false;
		if (player.name_length != 0U
		    && player.score > *top_score) {
			top_record = candidate;
			*top_score = player.score;
		}
	}
	if (!yt_news_append("  -  Xannor report:", error))
		return false;
	if (top_record == 0)
		return true;
	if (!yt_game_read_player(game, top_record, &player, error)
	    || !yt_random_next(&game->random, &gate, error))
		return false;
	if (*top_score < 2500000.0f
	    || qb_single_subtract(player_cloak[top_record],
	    0.33000001311302185f) > gate)
		return true;
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
	*hunt_player = top_record;
	*target_sector = player.sector;
	if (!yt_random_next(&game->random, &selection, error))
		return false;
	if (selection > 0.25f)
		*target_sector = player_sector[top_record];
	return true;
}
