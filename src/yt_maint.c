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
