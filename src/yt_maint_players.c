#include "yt_maint.h"

#include "yt_maint_internal.h"

#include "yt_names.h"
#include "yt_text.h"

#include <errno.h>
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

bool
yt_maintenance_clear_protected_mines(struct yt_game *game,
    struct yt_error *error)
{
	struct yt_sector value;
	int sector;

	if (game == NULL) {
		set_error(error, YT_INVALID, "clear protected mines", "");
		return false;
	}
	for (sector = 1; sector <= 7; ++sector) {
		if (!yt_game_read_sector(game, sector, &value, error))
			return false;
		value.mines = 0.0f;
		if (!yt_game_write_sector(game, sector, &value, error))
			return false;
	}
	return true;
}

bool
yt_maintenance_remove_player_from_teams(struct maint_state *state,
    int player_record, struct yt_error *error)
{
	static const size_t roster_offsets[] =
	    {YT_F109, YT_F117, YT_F121, YT_F125};
	int team;

	for (team = 1; team <= 50 && team <= state->sector_count; ++team) {
		struct yt_sector sector;
		bool changed = false;
		size_t slot;

		if (!yt_game_read_sector(&state->game, team, &sector, error))
			return false;
		for (slot = 0; slot < YT_ARRAY_LEN(roster_offsets); ++slot) {
			if (yt_record_get_number(&sector.record,
			    roster_offsets[slot]) == (float)player_record) {
				yt_record_set_number(&sector.record,
				    roster_offsets[slot], 0.0f);
				changed = true;
			}
		}
		if (changed) {
			if (!yt_database_write(&state->game.database,
			    (size_t)yt_sector_basic_record(&state->game.config, team),
			    &sector.record, error))
				return false;
		}
	}
	return true;
}

static bool
invalidate_radio(int player_record, struct yt_error *error)
{
	struct yt_radio_file file;
	struct yt_radio_record record;
	uint64_t length;
	uint64_t record_count;
	uint32_t basic_record;

	yt_radio_file_init(&file);
	if (!yt_radio_file_open(&file, "YTRMSG.DAT", error)) {
		(void)yt_radio_file_close(&file, NULL);
		return false;
	}
	if (!yt_radio_file_get(&file, 1U, &record, NULL, error)) {
		(void)yt_radio_file_close(&file, NULL);
		return false;
	}
	if (!yt_radio_file_size(&file, &length, error)) {
		(void)yt_radio_file_close(&file, NULL);
		return false;
	}
	record_count = length / YT_RADIO_RECORD_SIZE;
	if (record_count > 0xFFFFFFU) {
		set_error(error, YT_RANGE, "radio invalidation bound",
		    file.random.path);
		(void)yt_radio_file_close(&file, NULL);
		return false;
	}
	for (basic_record = 1U; basic_record <= record_count; ++basic_record) {
		if (!yt_radio_file_get(&file, basic_record, &record, NULL, error)) {
			(void)yt_radio_file_close(&file, NULL);
			return false;
		}
		if (yt_radio_get_number(&record, 4) == (float)player_record
		    || yt_radio_get_number(&record, 8) == (float)player_record) {
			yt_radio_set_number(&record, 0, 0.0f);
			if (!yt_radio_file_put(&file, basic_record, &record, error)) {
				(void)yt_radio_file_close(&file, NULL);
				return false;
			}
		}
	}
	if (!yt_radio_file_close(&file, error))
		return false;
	return true;
}

bool
yt_maintenance_remove_alias(const char *player_name, struct yt_error *error)
{
	static const uint8_t comma[] = ",";
	static const uint8_t row_end[] = {'\r', '\n'};
	struct yt_text_input input;
	struct yt_text_output output;
	struct yt_name_row row = {0};
	size_t staged_count = 0U;
	char first[128];
	char last[128];
	bool eof;
	bool result = false;

	if (player_name == NULL) {
		if (error != NULL)
			error->status = YT_INVALID;
		return false;
	}
	yt_names_split(player_name, first, sizeof(first), last, sizeof(last));
	yt_text_input_init(&input);
	yt_text_output_init(&output);
	if (!yt_text_output_open(&output, "TEMPWORK", error))
		goto done;
	if (!yt_text_input_open(&input, "YTNAME.DAT", error))
		goto done;
	for (;;) {
		if (!yt_text_input_eof(&input, &eof, error))
			goto done;
		if (eof)
			break;
		if (!yt_names_read_row(&input, &row,
		    &staged_count, error))
			goto done;
		if (strcmp(row.alias_first, first) == 0
		    && strcmp(row.alias_last, last) == 0) {
			yt_name_row_free(&row);
			staged_count = 0U;
			continue;
		}
		if (!yt_text_output_write(&output,
		    (const uint8_t *)row.real_first, strlen(row.real_first), error))
			goto done;
		if (!yt_text_output_write(&output, comma, sizeof(comma) - 1U,
		    error))
			goto done;
		if (!yt_text_output_write(&output,
		    (const uint8_t *)row.real_last, strlen(row.real_last), error))
			goto done;
		if (!yt_text_output_write(&output, comma, sizeof(comma) - 1U,
		    error))
			goto done;
		if (!yt_text_output_write(&output,
		    (const uint8_t *)row.alias_first, strlen(row.alias_first), error))
			goto done;
		if (!yt_text_output_write(&output, comma, sizeof(comma) - 1U,
		    error))
			goto done;
		if (!yt_text_output_write(&output,
		    (const uint8_t *)row.alias_last, strlen(row.alias_last), error))
			goto done;
		if (!yt_text_output_write(&output, row_end, sizeof(row_end), error))
			goto done;
		yt_name_row_free(&row);
		staged_count = 0U;
	}
	if (!yt_text_input_close(&input, error))
		goto done;
	if (!yt_text_output_close(&output, error))
		goto done;
	if (!yt_file_kill("YTNAME.DAT", error))
		goto done;
	if (!yt_file_rename("TEMPWORK", "YTNAME.DAT", error))
		goto done;
	result = true;

done:
	yt_name_row_free(&row);
	yt_text_input_destroy(&input);
	yt_text_output_destroy(&output);
	return result;
}

static bool
expire_player_impl(struct maint_state *state, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	int logical;

	state->player_sector[player_record] = 0;
	state->player_cloak[player_record] = 0.0f;
	player->lottery_plays = 0;
	if (!yt_maintenance_remove_player_from_teams(state, player_record, error))
		return false;
	for (logical = 1; logical <= state->planet_count; ++logical) {
		struct yt_planet planet;

		if (!yt_game_read_planet(&state->game, logical, &planet, error))
			return false;
		if (planet.owner == player_record) {
			planet.owner = 0;
			planet.ground_forces = 0.0f;
			if (!yt_game_write_planet(&state->game, logical, &planet,
			    error))
				return false;
		}
	}
	player->name_length = 0U;
	player->team = 0;
	if (!yt_game_write_player(&state->game, player_record, player, error))
		return false;
	for (logical = 1; logical <= state->sector_count; ++logical) {
		struct yt_sector sector;

		if (!yt_game_read_sector(&state->game, logical, &sector, error))
			return false;
		if (sector.fighter_owner == player_record) {
			sector.fighter_owner = 0;
			sector.fighters = 0.0f;
			if (!yt_game_write_sector(&state->game, logical, &sector,
			    error))
				return false;
		}
	}
	if (!invalidate_radio(player_record, error))
		return false;
	for (logical = 2; logical <= state->player_count + 1; ++logical) {
		struct yt_player other;

		if (!yt_game_read_player(&state->game, logical, &other, error))
			return false;
		if (other.killed_by == player_record) {
			other.killed_by = -98;
			if (!yt_game_write_player(&state->game, logical, &other,
			    error))
				return false;
		}
	}
	return true;
}

bool
yt_maintenance_expire_player(struct yt_game *game, int *player_sector,
    float *player_cloak, size_t cache_count, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct maint_state state;
	int player_count;

	if (game == NULL || player_sector == NULL || player_cloak == NULL
	    || player == NULL) {
		set_error(error, YT_INVALID, "expire player", "");
		return false;
	}
	player_count = (int)game->config.sector_offset - 1;
	if (player_record < 2 || player_record > player_count + 1
	    || (size_t)player_record >= cache_count) {
		set_error(error, YT_RANGE, "expire player", "YTDATA.DAT");
		return false;
	}
	memset(&state, 0, sizeof(state));
	state.game = *game;
	state.player_count = player_count;
	state.sector_count = (int)game->config.port_offset
	    - (int)game->config.sector_offset;
	state.port_count = (int)game->config.planet_offset
	    - (int)game->config.port_offset;
	state.planet_count = (int)game->config.total_records
	    - (int)game->config.planet_offset;
	state.player_sector = player_sector;
	state.player_cloak = player_cloak;
	return expire_player_impl(&state, player_record, player, error);
}

bool
yt_maintenance_players_run(struct maint_state *state,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_error *error)
{
	int record;

	for (record = 2; record <= state->player_count + 1; ++record) {
		struct yt_player player;
		struct yt_maintenance_player_output_result output;
		struct yt_maintenance_text name;
		struct yt_maintenance_text empty = {NULL, 0U};
		enum yt_maintenance_player_action action;
		float cached_cloak;

		if (!yt_game_read_player(&state->game, record, &player, error))
			return false;
		if (player.name_length == 0U)
			continue;
		name.data = player.record.bytes;
		name.length = player.name_length < YT_TEXT_FIELD_SIZE
		    ? player.name_length : YT_TEXT_FIELD_SIZE;
		if (!yt_maintenance_age_player(&player.cloak, player.last_active,
		    player.killed_by, (uint16_t)state->today,
		    state->game.config.retention_days, &cached_cloak, &action))
			return false;
		state->player_sector[record] = player.sector;
		state->player_cloak[record] = cached_cloak;
		if (cached_cloak > 0.0f) {
			if (!yt_game_write_player(&state->game, record, &player,
			    error))
				return false;
		}
		if (action == YT_MAINTENANCE_PLAYER_CLOAK_EXPIRED) {
			struct yt_clock_value time_now;
			struct yt_clock_value date_now;
			char time_text[9];
			char date_text[11];
			struct yt_maintenance_text time_value;
			struct yt_maintenance_text date_value;

			if (!yt_maintenance_compose_player_aging(&name, &empty,
			    &empty, true, false, &output))
				return false;
			if (!yt_news_append_bytes(output.screen.rows[0].data,
			    output.screen.rows[0].length, error))
				return false;
			if (!line_output(line_context, output.screen.rows[0].data,
			    output.screen.rows[0].length, error))
				return false;
			if (!yt_clock_read(&state->game.clock, &time_now, error))
				return false;
			if (!yt_clock_read(&state->game.clock, &date_now, error))
				return false;
			yt_format_time(&time_now, time_text);
			yt_format_date(&date_now, date_text);
			time_value.data = (const uint8_t *)time_text;
			time_value.length = strlen(time_text);
			date_value.data = (const uint8_t *)date_text;
			date_value.length = strlen(date_text);
			if (!yt_maintenance_compose_player_aging(&name, &time_value,
			    &date_value, true, false, &output))
				return false;
			if (!yt_radio_append_maintenance_bytes(output.radio_message,
			    output.radio_length, -2.0f, (float)record, error))
				return false;
			continue;
		}
		if (action == YT_MAINTENANCE_PLAYER_DELETE) {
			if (!yt_maintenance_compose_player_aging(&name, &empty,
			    &empty, false, true, &output))
				return false;
			if (!yt_news_append_bytes(output.screen.rows[0].data,
			    output.screen.rows[0].length, error))
				return false;
			if (!line_output(line_context, output.screen.rows[0].data,
			    output.screen.rows[0].length, error))
				return false;
			if (!yt_maintenance_expire_player(&state->game,
			    state->player_sector, state->player_cloak,
			    (size_t)state->player_count + 2U, record, &player, error))
				return false;
			if (!yt_maintenance_remove_alias(player.name, error))
				return false;
		}
	}
	return true;
}

bool
yt_maintenance_age_player(float *cloak, uint16_t last_active,
    int killer_status, uint16_t today, float retention_days,
    float *cached_cloak, enum yt_maintenance_player_action *action)
{
	static const float cloak_charge = -0.05000000074505806f;
	float cutoff;
	float working;

	if (cloak == NULL || cached_cloak == NULL || action == NULL)
		return false;
	working = *cloak;
	if (working < 0.0f)
		working = 1.0f;
	*cached_cloak = working;
	*action = YT_MAINTENANCE_PLAYER_UNCHANGED;
	if (working > 0.0f) {
		working = qb_single_add(working, cloak_charge);
		if (working < 0.0f)
			working = 0.0f;
		*cloak = working;
		if (working == 0.0f)
			*action = YT_MAINTENANCE_PLAYER_CLOAK_EXPIRED;
	}
	cutoff = qb_single_subtract((float)today, retention_days);
	if (*action != YT_MAINTENANCE_PLAYER_CLOAK_EXPIRED
	    && (float)last_active <= cutoff && killer_status != 0)
		*action = YT_MAINTENANCE_PLAYER_DELETE;
	return true;
}

static bool
immediate_death_cleanup_impl(struct maint_state *state, int victim_record,
    int killer, struct yt_player *victim, struct yt_error *error)
{
	int logical;

	state->player_sector[victim_record] = 0;
	state->player_cloak[victim_record] = 0.0f;
	victim->killed_by = killer;
	victim->sector = 0;
	victim->ground_forces = 0.0f;
	for (logical = 1; logical <= state->port_count; ++logical) {
		struct yt_port port;

		if (!yt_game_read_port(&state->game, logical, &port, error))
			return false;
		if (port.owner == victim_record) {
			port.owner = 0;
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
		if (sector.fighter_owner == victim_record) {
			sector.fighter_owner = -2;
			if (!yt_game_write_sector(&state->game, logical, &sector,
			    error))
				return false;
		}
	}
	if (!yt_maintenance_remove_player_from_teams(state, victim_record, error))
		return false;
	victim->team = 0;
	if (!yt_record_set_number(&victim->record, YT_F45,
	    (float)victim->killed_by)) {
		set_error(error, YT_RANGE, "encode immediate death player",
		    "YTDATA.DAT");
		return false;
	}
	if (!yt_record_set_number(&victim->record, YT_F57,
	    (float)victim->sector)) {
		set_error(error, YT_RANGE, "encode immediate death player",
		    "YTDATA.DAT");
		return false;
	}
	if (!yt_record_set_number(&victim->record, YT_F89,
	    (float)victim->team)) {
		set_error(error, YT_RANGE, "encode immediate death player",
		    "YTDATA.DAT");
		return false;
	}
	if (!yt_record_set_number(&victim->record, YT_F121,
	    victim->ground_forces)) {
		set_error(error, YT_RANGE, "encode immediate death player",
		    "YTDATA.DAT");
		return false;
	}
	return yt_database_write(&state->game.database,
	    (size_t)victim_record, &victim->record, error);
}

bool
yt_maintenance_immediate_death(struct yt_game *game, int *player_sector,
    float *player_cloak, size_t cache_count, int victim_record, int killer,
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
	state.sector_count = (int)game->config.port_offset
	    - (int)game->config.sector_offset;
	state.port_count = (int)game->config.planet_offset
	    - (int)game->config.port_offset;
	state.planet_count = (int)game->config.total_records
	    - (int)game->config.planet_offset;
	state.player_sector = player_sector;
	state.player_cloak = player_cloak;
	return immediate_death_cleanup_impl(&state, victim_record, killer,
	    victim, error);
}
