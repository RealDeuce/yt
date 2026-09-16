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
	return yt_maintenance_xannor_run(state, maintenance_stdout_line, NULL,
	    error)
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
