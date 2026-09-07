#include "qb.h"
#include "yt_cli.h"
#include "yt_config_output.h"
#include "yt_game.h"
#include "yt_names.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool
store_config(struct yt_game *game, struct yt_error *error)
{
	return yt_config_store(&game->database, &game->config, error)
	    && yt_database_flush(&game->database, error);
}

static bool
headquarters_read_record(void *context, size_t basic_record,
    struct yt_record *record, struct yt_error *error)
{
	struct yt_game *game = context;

	return yt_database_read(&game->database, basic_record, record, error);
}

static bool
headquarters_write_record(void *context, size_t basic_record,
    const struct yt_record *record, struct yt_error *error)
{
	struct yt_game *game = context;

	return yt_database_write(&game->database, basic_record, record, error)
	    && (basic_record != 1U
	    || yt_database_flush(&game->database, error));
}

static bool
ytconfig_close_all(struct yt_game *game, struct yt_error *error)
{
	struct yt_close_all_control control = {
		.heap_type = YT_CLOSE_ALL_HEAP_FILE,
		.file_class = 0,
		.method = yt_database_close_all_method,
		.context = &game->database,
	};
	size_t control_count = game->database.file != NULL ? 1U : 0U;

	return yt_close_all_run(control_count != 0U ? &control : NULL,
	    control_count, NULL, NULL, error);
}

static bool
redraw_repairs(struct yt_game *game, float maximum,
    struct yt_error *error)
{
	if (game->config.initial_holds > maximum) {
		game->config.initial_holds = maximum;
		if (!store_config(game, error))
			return false;
	}
	if (!yt_config_load(&game->database, &game->config, error))
		return false;
	if (game->config.headquarters == 0.0f) {
		game->config.headquarters = 85.0f;
		if (!store_config(game, error))
			return false;
	}
	return true;
}

static bool
write_output(const struct yt_config_output_result *output,
    struct yt_error *error)
{
	unsigned beep;

	if (output->output_length != 0U
	    && fwrite(output->output, 1, output->output_length, stdout)
	    != output->output_length) {
		if (error != NULL) {
			error->status = YT_IO_ERROR;
			snprintf(error->operation, sizeof(error->operation),
			    "write configuration screen");
			snprintf(error->path, sizeof(error->path), "stdout");
		}
		return false;
	}
	for (beep = 0U; beep < output->local_beeps; ++beep) {
		if (fputc('\a', stdout) == EOF) {
			if (error != NULL) {
				error->status = YT_IO_ERROR;
				snprintf(error->operation, sizeof(error->operation),
				    "beep configuration console");
				snprintf(error->path, sizeof(error->path), "stdout");
			}
			return false;
		}
	}
	return true;
}

static bool
numeric_edit(struct yt_game *game, char key, float *maximum, float *lottery,
    struct yt_error *error)
{
	struct yt_config_output_result output;
	struct qb_val_result parsed;
	char line[160];
	float value;
	bool blank;
	float *field = NULL;
	enum yt_config_scalar_key scalar = (enum yt_config_scalar_key)key;

	switch (key) {
	case 'A':
		field = &game->config.maximum_holds;
		break;
	case 'B':
		field = &game->config.turns_per_day;
		break;
	case 'C':
		field = &game->config.initial_fighters;
		break;
	case 'D':
		field = &game->config.initial_credits;
		break;
	case 'E':
		field = &game->config.initial_holds;
		break;
	case 'F':
		field = &game->config.retention_days;
		break;
	case 'K':
		field = &game->config.lottery_plays;
		break;
	default:
		return true;
	}
	if (!yt_config_compose_scalar_prompt(scalar, *maximum, 0U, &output)
	    || !write_output(&output, error))
		return false;
	if (!yt_cli_line(line, sizeof(line)))
		return true;
	blank = line[0] == '\0';
	if (blank && yt_config_scalar_blank_unchanged(scalar))
		return true;
	parsed = qb_val(line);
	value = (float)(parsed.valid ? parsed.value : 0.0);
	if (!yt_config_scalar_valid(scalar, value)) {
		if (!yt_config_compose_scalar_rejection(scalar,
		    output.final_column, &output) || !write_output(&output, error))
			return false;
		return true;
	}
	*field = value;
	if (key == 'A')
		*maximum = value;
	else if (key == 'K')
		*lottery = value;
	return store_config(game, error);
}

static bool
edit_genesis(struct yt_game *game, struct yt_error *error)
{
	struct yt_config_output_result output;
	struct qb_val_result parsed;
	char line[160];
	float threshold;

	if (!yt_config_compose_genesis_prompt(NULL, 0U, 0U, &output)
	    || !write_output(&output, error))
		return false;
	if (!yt_cli_line(line, sizeof(line)) || line[0] == '\0')
		return true;
	parsed = qb_val(line);
	threshold = (float)(parsed.valid ? parsed.value : 0.0);
	if (!yt_config_genesis_valid(threshold)) {
		if (!yt_config_compose_local_beep(output.final_column, &output)
		    || !write_output(&output, error))
			return false;
		return true;
	}
	game->config.genesis_ports = threshold;
	return store_config(game, error);
}

static bool
edit_maintenance(struct yt_game *game, struct yt_error *error)
{
	struct yt_config_output_result output;
	char line[80];
	int adjusted;
	int serial;

	for (;;) {
		if (!yt_config_compose_scalar_prompt(YT_CONFIG_SCALAR_MAINTENANCE,
		    0.0f, 0U, &output) || !write_output(&output, error))
			return false;
		if (!yt_cli_line(line, sizeof(line)) || line[0] == '\0')
			return true;
		if (((unsigned char)line[0] & 0xdfU) == 'Y'
		    || ((unsigned char)line[0] & 0xdfU) == 'N')
			break;
	}
	if (!yt_current_date_serial(game->config.epoch_year, &serial,
	    &adjusted, error))
		return false;
	game->config.epoch_year = (float)adjusted;
	if (((unsigned char)line[0] & 0xdfU) == 'Y') {
		static const uint8_t raw_allow[4] = {0x00, 0x00, 0x7d, 0x00};

		game->config.last_maintenance = 0.0f;
		yt_record_set_raw_number(&game->config.record, YT_F81, raw_allow);
	}
	else
		game->config.last_maintenance = (float)serial;
	return store_config(game, error);
}

static bool
edit_scoreboard(struct yt_game *game, uint8_t working_path[41],
    size_t *working_path_length, struct yt_error *error)
{
	struct yt_config_output_result output;
	char line[160];
	const char *stored;
	size_t length;

	if (!yt_config_compose_scoreboard_prompt(0U, &output)
	    || !write_output(&output, error))
		return false;
	if (!yt_cli_line(line, sizeof(line)))
		return true;
	if (strlen(line) > 41U) {
		if (!yt_config_compose_scoreboard_too_long(output.final_column,
		    &output) || !write_output(&output, error))
			return false;
		return true;
	}
	stored = line[0] == '\0' ? "YTSCORE.ASC" : line;
	length = strlen(stored);
	snprintf(game->config.scoreboard, sizeof(game->config.scoreboard), "%s",
	    stored);
	if (!store_config(game, error))
		return false;
	memcpy(working_path, stored, length);
	*working_path_length = length;
	return true;
}

static bool
edit_headquarters(struct yt_game *game, struct yt_error *error)
{
	static const struct yt_config_hq_ops ops = {
		headquarters_read_record,
		headquarters_write_record,
	};
	struct yt_config_output_result output;
	struct yt_config_hq_state state;
	struct qb_val_result parsed;
	char line[160];
	float raw;
	float upper = game->config.port_offset
	    - game->config.sector_offset;

	if (!yt_config_compose_hq_prompt(game->config.headquarters, upper, 0U,
	    &output) || !write_output(&output, error))
		return false;
	if (!yt_cli_line(line, sizeof(line)) || line[0] == '\0')
		return true;
	parsed = qb_val(line);
	raw = (float)(parsed.valid ? parsed.value : 0.0);
	if (!yt_config_hq_in_range(raw, upper)) {
		if (!yt_config_compose_hq_diagnostic(YT_CONFIG_HQ_INVALID,
		    output.final_column, &output) || !write_output(&output, error))
			return false;
		return true;
	}
	if (!yt_config_headquarters_relocate(&state, &game->config, raw, &ops,
	    game, error))
		return false;
	if (state.route == YT_CONFIG_HQ_ROUTE_OCCUPIED) {
		if (!yt_config_compose_hq_diagnostic(YT_CONFIG_HQ_OCCUPIED,
		    output.final_column, &output) || !write_output(&output, error))
			return false;
		return true;
	}
	game->config.record = state.field;
	game->config.headquarters = raw;
	return true;
}

static bool
edit_planets(struct yt_game *game, struct yt_error *error)
{
	char names[76][42];
	bool active[76] = {false};
	unsigned active_count = 0U;
	struct yt_config_output_result output;
	int logical;

	memset(names, 0, sizeof(names));
	for (logical = 1; logical <= 75; ++logical) {
		struct yt_planet planet;

		if (!yt_game_read_planet(game, logical, &planet, error))
			return false;
		if (planet.name_length == 0.0f)
			continue;
		active[logical] = true;
		++active_count;
		snprintf(names[logical], sizeof(names[logical]), "%s", planet.name);
	}
	if (!yt_config_compose_planet_entry(active_count, 0U, &output)
	    || !write_output(&output, error))
		return false;
	if (active_count == 0U)
		return true;
	for (;;) {
		int raw_key;
		uint8_t folded;

		if (!yt_config_compose_planet_menu(0U, &output)
		    || !write_output(&output, error))
			return false;
		raw_key = yt_cli_key();
		if (raw_key == EOF)
			return true;
		if (raw_key == '\n')
			raw_key = '\r';
		if (!yt_config_compose_planet_key_echo((uint8_t)raw_key,
		    output.final_column, &folded, &output)
		    || !write_output(&output, error))
			return false;
		if (folded == '\r')
			return true;
		if (folded == 'L') {
			if (!yt_config_compose_planet_list_header(0U, &output)
			    || !write_output(&output, error))
				return false;
			for (logical = 1; logical <= 75; ++logical) {
				if (!active[logical])
					continue;
				if (!yt_config_compose_planet_list_row(logical,
				    (const uint8_t *)names[logical],
				    strlen(names[logical]), 0U, &output)
				    || !write_output(&output, error))
					return false;
				if (yt_config_planet_pause_after(logical,
				    active_count)) {
					if (!yt_config_compose_planet_pause(
					    output.final_column, &output)
					    || !write_output(&output, error))
						return false;
					(void)yt_cli_key();
					if (!yt_config_compose_planet_blank(
					    output.final_column, &output)
					    || !write_output(&output, error))
						return false;
				}
			}
			if (!yt_config_compose_planet_blank(0U, &output)
			    || !write_output(&output, error))
				return false;
			continue;
		}
		if (folded == 'C') {
			struct qb_val_result parsed;
			float raw;
			bool overflow;
			int selected;
			char entered[160];
			char name[160];

			if (!yt_config_compose_planet_number_prompt(0U, &output)
			    || !write_output(&output, error))
				return false;
			if (!yt_cli_line(entered, sizeof(entered)))
				return true;
			parsed = qb_val(entered);
			raw = (float)(parsed.valid ? parsed.value : 0.0);
			if (raw == 0.0f)
				continue;
			if (!yt_config_planet_selection_in_range(raw)) {
				if (!yt_config_compose_planet_invalid(
				    (const uint8_t *)entered, strlen(entered),
				    output.final_column, &output)
				    || !write_output(&output, error))
					return false;
				continue;
			}
			selected = (int)qb_cint(raw, &overflow);
			if (overflow)
				continue;
			if (!active[selected]) {
				if (!yt_config_compose_planet_invalid(
				    (const uint8_t *)entered, strlen(entered),
				    output.final_column, &output)
				    || !write_output(&output, error))
					return false;
				continue;
			}
			if (yt_config_planet_selection_protected(raw)) {
				if (!yt_config_compose_planet_protected(
				    output.final_column, &output)
				    || !write_output(&output, error))
					return false;
				continue;
			}
			for (;;) {
				if (!yt_config_compose_planet_edit(
				    (const uint8_t *)names[selected],
				    strlen(names[selected]), 0U, &output)
				    || !write_output(&output, error))
					return false;
				if (!yt_cli_line(name, sizeof(name)))
					return true;
				name[41] = '\0';
				qb_title_case(name);
				if (name[0] == '\0') {
					if (!yt_config_compose_planet_blank(
					    output.final_column, &output)
					    || !write_output(&output, error))
						return false;
					break;
				}
				for (;;) {
					if (!yt_config_compose_planet_confirmation(
					    (const uint8_t *)name, strlen(name),
					    output.final_column, &output)
					    || !write_output(&output, error))
						return false;
					raw_key = yt_cli_key();
					if (raw_key == EOF)
						return true;
					if (!yt_config_compose_planet_response_echo(
					    (uint8_t)raw_key, output.final_column,
					    &folded, &output)
					    || !write_output(&output, error))
						return false;
					if (folded == 'Y')
						goto save_planet_name;
					if (folded == 'N')
						break;
				}
				if (!yt_config_compose_planet_cancel(
				    (const uint8_t *)name, strlen(name),
				    output.final_column, &output)
				    || !write_output(&output, error))
					return false;
			}
			continue;
save_planet_name:
			{
				struct yt_planet planet;

				if (!yt_game_read_planet(game, selected, &planet, error))
					return false;
				snprintf(planet.name, sizeof(planet.name), "%s", name);
				planet.name_length = (float)strlen(name);
				if (!yt_game_write_planet(game, selected, &planet, error))
					return false;
			}
			snprintf(names[selected], sizeof(names[selected]), "%s", name);
			if (!yt_config_compose_planet_saved(output.final_column, &output)
			    || !write_output(&output, error))
				return false;
			(void)yt_cli_key();
			return true;
		}
	}
}

static bool
edit_ports(struct yt_game *game, struct yt_error *error)
{
	struct yt_config_output_result output;
	char search[160];
	char upper[160];
	int logical;
	bool matched = false;

	if (!yt_config_compose_port_search_prompt(0U, &output)
	    || !write_output(&output, error))
		return false;
	if (!yt_cli_line(search, sizeof(search)))
		return true;
	if (!yt_config_compose_port_search_echo((const uint8_t *)search,
	    strlen(search), output.final_column, &output)
	    || !write_output(&output, error))
		return false;
	if (search[0] == '\0')
		return true;
	snprintf(upper, sizeof(upper), "%s", search);
	qb_ascii_upper(upper);
	for (logical = 2; logical <= 300; ++logical) {
		struct yt_port port;
		char candidate[42];
		bool overflow;
		int name_length;

		if (!yt_game_read_port(game, logical, &port, error))
			return false;
		name_length = (int)qb_cint(port.name_length, &overflow);
		if (overflow || name_length < 0 || name_length > 41)
			return false;
		memcpy(candidate, port.name, (size_t)name_length);
		candidate[name_length] = '\0';
		qb_ascii_upper(candidate);
		if (strstr(candidate, upper) == NULL)
			continue;
		matched = true;
		for (;;) {
			int raw_key;
			uint8_t folded;

			if (!yt_config_compose_port_match_prompt(
			    (const uint8_t *)port.name, (size_t)name_length,
			    output.final_column, &output)
			    || !write_output(&output, error))
				return false;
			raw_key = yt_cli_key();
			if (raw_key == EOF)
				return true;
			if (!yt_config_compose_port_response_echo((uint8_t)raw_key,
			    output.final_column, &folded, &output)
			    || !write_output(&output, error))
				return false;
			if (folded == 'N')
				break;
			if (folded == 'Y')
				goto replace_port_name;
		}
		continue;
replace_port_name:
		{
			char name[160];

			if (!yt_config_compose_port_replacement_prompt(
			    output.final_column, &output)
			    || !write_output(&output, error))
				return false;
			if (!yt_cli_line(name, sizeof(name)))
				return true;
			qb_title_case(name);
			name[41] = '\0';
			if (name[0] == '\0')
				return true;
			for (;;) {
				int raw_key;
				uint8_t folded;

				if (!yt_config_compose_port_confirmation(
				    (const uint8_t *)name, strlen(name),
				    output.final_column, &output)
				    || !write_output(&output, error))
					return false;
				raw_key = yt_cli_key();
				if (raw_key == EOF)
					return true;
				if (!yt_config_compose_port_response_echo(
				    (uint8_t)raw_key, output.final_column,
				    &folded, &output)
				    || !write_output(&output, error))
					return false;
				if (folded == 'Y')
					break;
				if (folded == 'N') {
					if (!yt_config_compose_port_cancel(
					    output.final_column, &output)
					    || !write_output(&output, error)
					    || !yt_config_compose_port_wait_prompt(
					    output.final_column, &output)
					    || !write_output(&output, error))
						return false;
					(void)yt_cli_line(search, sizeof(search));
					return true;
				}
			}
			snprintf(port.name, sizeof(port.name), "%s", name);
			port.name_length = (float)strlen(name);
			if (!yt_game_write_port(game, logical, &port, error)
			    || !yt_config_compose_port_saved(output.final_column,
				&output)
			    || !write_output(&output, error)
			    || !yt_config_compose_port_wait_prompt(
				output.final_column, &output)
			    || !write_output(&output, error))
				return false;
			(void)yt_cli_line(search, sizeof(search));
			return true;
		}
	}
	if (matched) {
		if (!yt_config_compose_port_end_list(output.final_column, &output)
		    || !write_output(&output, error)
		    || !yt_config_compose_port_wait_prompt(output.final_column,
			&output)
		    || !write_output(&output, error))
			return false;
		(void)yt_cli_line(search, sizeof(search));
	}
	else if (!yt_config_compose_port_not_found(output.final_column, &output)
	    || !write_output(&output, error))
		return false;
	return true;
}

static bool
alias_propagate_read(void *context, int basic_record,
    struct yt_record *record, struct yt_error *error)
{
	struct yt_game *game = context;

	return yt_database_read(&game->database, (size_t)basic_record, record,
	    error);
}

static bool
alias_propagate_write(void *context, int basic_record,
    const struct yt_record *record, struct yt_error *error)
{
	struct yt_game *game = context;

	return yt_database_write(&game->database, (size_t)basic_record, record,
	    error);
}

static bool
edit_aliases(struct yt_game *game, struct yt_error *error)
{
	struct yt_name_file names;
	struct yt_config_output_result output;
	unsigned player_count;

	if (!yt_names_load("YTNAME.DAT", &names, error))
		return false;
	if (names.count == 0U || names.count > 51U) {
		if (error != NULL) {
			error->status = YT_RANGE;
			snprintf(error->operation, sizeof(error->operation),
			    "load YTCONFIG alias table");
			snprintf(error->path, sizeof(error->path), "YTNAME.DAT");
		}
		yt_names_free(&names);
		return false;
	}
	player_count = (unsigned)(names.count - 1U);
	if (!yt_config_compose_alias_entry(player_count, 0U, &output)
	    || !write_output(&output, error)) {
		yt_names_free(&names);
		return false;
	}
	if (player_count == 0U) {
		yt_names_free(&names);
		return true;
	}
	for (;;) {
		int raw_key;
		uint8_t folded;
		size_t index;

		if (!yt_config_compose_alias_menu(0U, &output)
		    || !write_output(&output, error)) {
			yt_names_free(&names);
			return false;
		}
		raw_key = yt_cli_key();
		if (raw_key == EOF) {
			yt_names_free(&names);
			return true;
		}
		if (raw_key == '\n')
			raw_key = '\r';
		if (!yt_config_compose_alias_key_echo((uint8_t)raw_key,
		    output.final_column, &folded, &output)
		    || !write_output(&output, error)) {
			yt_names_free(&names);
			return false;
		}
		if (folded == '\r') {
			yt_names_free(&names);
			return true;
		}
		if (folded == 'L') {
			if (!yt_config_compose_alias_list_header(0U, &output)
			    || !write_output(&output, error)) {
				yt_names_free(&names);
				return false;
			}
			for (index = 1U; index <= player_count; ++index) {
				const struct yt_name_row *row = &names.rows[index];

				if (!yt_config_compose_alias_list_row((int)index,
				    (const uint8_t *)row->real_first,
				    strlen(row->real_first),
				    (const uint8_t *)row->real_last,
				    strlen(row->real_last),
				    (const uint8_t *)row->alias_first,
				    strlen(row->alias_first),
				    (const uint8_t *)row->alias_last,
				    strlen(row->alias_last), 0U, &output)
				    || !write_output(&output, error)) {
					yt_names_free(&names);
					return false;
				}
				if (yt_config_alias_pause_after((int)index,
				    player_count)) {
					if (!yt_config_compose_alias_pause(
					    output.final_column, &output)
					    || !write_output(&output, error)) {
						yt_names_free(&names);
						return false;
					}
					(void)yt_cli_key();
					if (!yt_config_compose_alias_blank(
					    output.final_column, &output)
					    || !write_output(&output, error)) {
						yt_names_free(&names);
						return false;
					}
				}
			}
			if (!yt_config_compose_alias_blank(0U, &output)
			    || !write_output(&output, error)) {
				yt_names_free(&names);
				return false;
			}
			continue;
		}
		if (folded == 'C') {
			struct qb_val_result parsed;
			float raw;
			bool overflow;
			int selected;
			char entered[180];
			char old_alias[90];
			char first[90];
			char last[90];
			struct yt_alias_propagate_state propagation;
			static const struct yt_alias_propagate_ops propagation_ops = {
				alias_propagate_read,
				alias_propagate_write,
			};

			if (!yt_config_compose_alias_number_prompt(0U, &output)
			    || !write_output(&output, error)) {
				yt_names_free(&names);
				return false;
			}
			if (!yt_cli_line(entered, sizeof(entered))) {
				yt_names_free(&names);
				return true;
			}
			parsed = qb_val(entered);
			raw = (float)(parsed.valid ? parsed.value : 0.0);
			if (raw == 0.0f)
				continue;
			if (!yt_config_alias_selection_in_range(raw, player_count)) {
				if (!yt_config_compose_alias_invalid(
				    (const uint8_t *)entered, strlen(entered),
				    output.final_column, &output)
				    || !write_output(&output, error)) {
					yt_names_free(&names);
					return false;
				}
				continue;
			}
			selected = (int)qb_cint(raw, &overflow);
			if (overflow || selected < 1
			    || (unsigned)selected > player_count)
				continue;
			if (!yt_config_compose_alias_blank(output.final_column,
			    &output) || !write_output(&output, error)) {
				yt_names_free(&names);
				return false;
			}
			for (;;) {
				const struct yt_name_row *row = &names.rows[selected];

				if (!yt_config_compose_alias_edit(
				    (const uint8_t *)row->real_first,
				    strlen(row->real_first),
				    (const uint8_t *)row->real_last,
				    strlen(row->real_last),
				    (const uint8_t *)row->alias_first,
				    strlen(row->alias_first),
				    (const uint8_t *)row->alias_last,
				    strlen(row->alias_last), 0U, &output)
				    || !write_output(&output, error)) {
					yt_names_free(&names);
					return false;
				}
				if (!yt_cli_line(entered, sizeof(entered))) {
					yt_names_free(&names);
					return true;
				}
				entered[41] = '\0';
				for (index = 0; entered[index] != '\0'; ++index) {
					if (entered[index] == ',')
						entered[index] = ' ';
				}
				qb_title_case(entered);
				if (entered[0] == '\0') {
					if (!yt_config_compose_alias_blank(
					    output.final_column, &output)
					    || !write_output(&output, error)) {
						yt_names_free(&names);
						return false;
					}
					break;
				}
				for (;;) {
					if (!yt_config_compose_alias_confirmation(
					    (const uint8_t *)entered, strlen(entered),
					    output.final_column, &output)
					    || !write_output(&output, error)) {
						yt_names_free(&names);
						return false;
					}
					raw_key = yt_cli_key();
					if (raw_key == EOF) {
						yt_names_free(&names);
						return true;
					}
					if (!yt_config_compose_alias_response_echo(
					    (uint8_t)raw_key, output.final_column,
					    &folded, &output)
					    || !write_output(&output, error)) {
						yt_names_free(&names);
						return false;
					}
					if (folded != 'Y' && folded != 'N')
						continue;
					if (!yt_config_compose_alias_blank(
					    output.final_column, &output)
					    || !write_output(&output, error)) {
						yt_names_free(&names);
						return false;
					}
					if (folded == 'Y')
						goto save_alias;
					if (!yt_config_compose_alias_cancel(
					    output.final_column, &output)
					    || !write_output(&output, error)) {
						yt_names_free(&names);
						return false;
					}
					break;
				}
			}
			continue;
save_alias:
			snprintf(old_alias, sizeof(old_alias), "%s %s",
			    names.rows[selected].alias_first,
			    names.rows[selected].alias_last);
			qb_title_case(old_alias);
			yt_names_split(entered, first, sizeof(first), last,
			    sizeof(last));
			if (!yt_names_set_alias(&names, (size_t)selected, first, last,
			    error)) {
				yt_names_free(&names);
				return false;
			}
			if (!yt_names_write("YTNAME.DAT", &names, error)) {
				yt_names_free(&names);
				return false;
			}
			if (!yt_names_propagate_alias(&propagation,
			    (const uint8_t *)old_alias, strlen(old_alias),
			    (const uint8_t *)entered, strlen(entered),
			    &propagation_ops, game, error)) {
				yt_names_free(&names);
				return false;
			}
			if (!yt_config_compose_alias_saved(0U, &output)
			    || !write_output(&output, error)) {
				yt_names_free(&names);
				return false;
			}
			(void)yt_cli_key();
			yt_names_free(&names);
			return true;
		}
	}
}

int
main(void)
{
	struct yt_game game;
	struct yt_error error;
	struct yt_config_menu_working working;
	uint8_t scoreboard[41];
	uint32_t file_size;

	yt_error_clear(&error);
	memset(&game, 0, sizeof(game));
	if (!yt_database_random_close(&game.database, &error)
	    || !yt_database_open(&game.database, "YTDATA.DAT", YT_OPEN_UPDATE,
	    &error)) {
		if (error.status != YT_NOT_FOUND)
			goto failure;
		yt_error_clear(&error);
		if (!yt_database_open(&game.database, "YTDATA.DAT",
		    YT_OPEN_CREATE, &error))
			goto failure;
	}
	if (!yt_database_random_lof(&game.database, &file_size, &error))
		goto failure;
	if (file_size == 0) {
		struct yt_config_output_result output;

		if (!yt_config_compose_missing_data(0U, &output)
		    || !write_output(&output, &error))
			goto failure;
		if (!ytconfig_close_all(&game, &error))
			goto failure;
		if (!yt_file_kill("YTDATA.DAT", NULL, &error))
			goto failure_closed;
		return EXIT_SUCCESS;
	}
	if (!yt_config_load(&game.database, &game.config, &error))
		goto failure;
	if (!yt_config_prepare_menu_working(&game.config, scoreboard, &working))
		goto failure;
	for (;;) {
		struct yt_config_output_result output;
		int today;
		int year;
		int raw_key;
		uint8_t folded;
		char key;

		if (!yt_config_load(&game.database, &game.config, &error)
		    || !yt_current_date_serial(game.config.epoch_year, &today, &year,
		    &error)
		    || !redraw_repairs(&game, working.maximum_holds, &error))
			goto failure;
		if (!yt_config_compose_menu_prompt(&game.config, &working, today,
		    0U, &output) || !write_output(&output, &error))
			goto failure;
		fflush(stdout);
		raw_key = yt_cli_key();
		if (raw_key == EOF)
			break;
		if (!yt_config_compose_command_echo((uint8_t)raw_key,
		    output.final_column, &folded, &output)
		    || !write_output(&output, &error))
			goto failure;
		key = (char)folded;
		if (strchr("ABCDEFK", key) != NULL) {
			if (!numeric_edit(&game, key, &working.maximum_holds,
			    &working.lottery_plays, &error))
				goto failure;
		}
		else if (key == 'G') {
			if (!edit_maintenance(&game, &error))
				goto failure;
		}
		else if (key == 'H') {
			if (!edit_headquarters(&game, &error))
				goto failure;
		}
		else if (key == 'I') {
			if (!edit_scoreboard(&game, scoreboard,
			    &working.scoreboard_path_length, &error))
				goto failure;
		}
		else if (key == 'J') {
			bool overflow;
			int value = (int)qb_cint(working.local_screen,
			    &overflow);

			if (!overflow) {
				working.local_screen = (float)(~value);
				game.config.local_screen = working.local_screen;
				if (!store_config(&game, &error))
					goto failure;
			}
		}
		else if (key == 'L') {
			if (!edit_genesis(&game, &error))
				goto failure;
		}
		else if (key == 'N') {
			if (!edit_aliases(&game, &error))
				goto failure;
		}
		else if (key == 'O') {
			if (!edit_ports(&game, &error))
				goto failure;
		}
		else if (key == 'P') {
			if (!edit_planets(&game, &error))
				goto failure;
		}
		else if (key == 'X') {
			if (!yt_config_compose_exit(output.final_column, &output)
			    || !write_output(&output, &error))
				goto failure;
			break;
		}
	}
	if (!ytconfig_close_all(&game, &error))
		goto failure;
	return EXIT_SUCCESS;

failure:
	yt_game_close(&game);
	yt_cli_error("YTCONFIG", &error);
	return EXIT_FAILURE;

failure_closed:
	yt_cli_error("YTCONFIG", &error);
	return EXIT_FAILURE;
}
