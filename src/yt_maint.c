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

static bool maintenance_stdout_semi(void *context, const uint8_t *text,
    size_t length, struct yt_error *error);
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

bool
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
const struct yt_maintenance_output_row *
maintenance_find_output_row(const struct yt_maintenance_output_result *output,
    uint16_t address)
{
	size_t index;

	for (index = 0U; index < output->row_count; ++index)
		if (output->rows[index].address == address)
			return &output->rows[index];
	return NULL;
}

bool
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
maintain_factions(struct maint_state *state, struct yt_error *error)
{
	return yt_maintenance_xannor_run(state, maintenance_stdout_line, NULL,
	    error)
	    && yt_maintenance_mercenaries_run(state, maintenance_stdout_line,
	    NULL, error);
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
