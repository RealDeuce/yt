#include "qb.h"
#include "yt_cli.h"
#include "yt_config_output.h"
#include "yt_game.h"
#include "yt_names.h"
#include "yt_text.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool
ytconfig_close_all(struct yt_game *game, struct yt_error *error)
{
	if (game->database.file == NULL)
		return true;
	return yt_database_close_all_single(&game->database, error);
}

static bool
redraw_repairs(struct yt_game *game, float maximum,
    struct yt_error *error)
{
	struct yt_record result;

	if (!yt_config_redraw_repairs(&game->database, &game->config.record,
	    maximum, &result, error))
		return false;
	return yt_config_decode(&game->config, &result, error);
}

static bool
write_output(const struct yt_config_output_result *output,
    struct yt_error *error)
{
	unsigned beep;

	if (output->output_length != 0U) {
		if (fwrite(output->output, 1, output->output_length, stdout)
		    != output->output_length) {
			if (error != NULL) {
				error->status = YT_IO_ERROR;
				snprintf(error->operation, sizeof(error->operation),
				    "write configuration screen");
				snprintf(error->path, sizeof(error->path), "stdout");
			}
			return false;
		}
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
	struct yt_config_overlay overlay;
	struct yt_record updated;
	char line[160];
	uint8_t raw[4];
	float value;
	size_t offset = 0U;
	bool blank;
	bool fresh_before_prompt = false;
	float *field = NULL;
	enum yt_config_scalar_key scalar = (enum yt_config_scalar_key)key;

	switch (key) {
	case 'A':
		field = &game->config.maximum_holds;
		offset = YT_F121;
		fresh_before_prompt = true;
		break;
	case 'B':
		field = &game->config.turns_per_day;
		offset = YT_F49;
		break;
	case 'C':
		field = &game->config.initial_fighters;
		offset = YT_F65;
		break;
	case 'D':
		field = &game->config.initial_credits;
		offset = YT_F69;
		break;
	case 'E':
		field = &game->config.initial_holds;
		offset = YT_F73;
		break;
	case 'F':
		field = &game->config.retention_days;
		offset = YT_F77;
		break;
	case 'K':
		field = &game->config.lottery_plays;
		offset = YT_F101;
		fresh_before_prompt = true;
		break;
	default:
		return true;
	}
	if (fresh_before_prompt) {
		if (!yt_database_read(&game->database, 1U, &updated, error))
			return false;
		game->config.record = updated;
		*field = yt_record_get_number(&updated, offset);
	}
	if (!yt_config_compose_scalar_prompt(scalar, *maximum, 0U, &output))
		return false;
	if (!write_output(&output, error))
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
		    output.final_column, &output))
			return false;
		if (!write_output(&output, error))
			return false;
		return true;
	}
	if (qb_mbf32_encode(value, raw) == QB_MBF_OVERFLOW) {
		if (error != NULL)
			error->status = YT_RANGE;
		return false;
	}
	overlay = (struct yt_config_overlay){offset, raw, sizeof(raw)};
	if (!yt_config_apply_loaded_overlays(&game->database,
	    &game->config.record, &overlay, 1U, &updated, error))
		return false;
	game->config.record = updated;
	*field = value;
	if (key == 'A')
		*maximum = value;
	else if (key == 'K')
		*lottery = value;
	return true;
}

static bool
edit_genesis(struct yt_game *game, struct yt_error *error)
{
	struct yt_config_output_result output;
	struct qb_val_result parsed;
	struct yt_config_overlay overlay;
	struct yt_record updated;
	char line[160];
	uint8_t raw[4];
	float threshold;

	if (!yt_config_compose_genesis_prompt(NULL, 0U, 0U, &output))
		return false;
	if (!write_output(&output, error))
		return false;
	if (!yt_cli_line(line, sizeof(line)))
		return true;
	if (line[0] == '\0')
		return true;
	parsed = qb_val(line);
	threshold = (float)(parsed.valid ? parsed.value : 0.0);
	if (!yt_config_genesis_valid(threshold)) {
		if (!yt_config_compose_local_beep(output.final_column, &output))
			return false;
		if (!write_output(&output, error))
			return false;
		return true;
	}
	if (qb_mbf32_encode(threshold, raw) == QB_MBF_OVERFLOW) {
		if (error != NULL)
			error->status = YT_RANGE;
		return false;
	}
	overlay = (struct yt_config_overlay){YT_F105, raw, sizeof(raw)};
	if (!yt_config_apply_overlays(&game->database, &overlay, 1U,
	    &updated, error))
		return false;
	game->config.record = updated;
	game->config.genesis_ports = threshold;
	return true;
}

static bool
edit_maintenance(struct yt_game *game, struct yt_error *error)
{
	static const uint8_t raw_allow[4] = {0x00, 0x00, 0x7d, 0x00};
	struct yt_config_output_result output;
	struct yt_config_overlay overlays[2];
	struct yt_record updated;
	char line[80];
	uint8_t raw_marker[4];
	uint8_t raw_epoch[4];
	int adjusted;
	int serial;

	for (;;) {
		if (!yt_config_compose_scalar_prompt(YT_CONFIG_SCALAR_MAINTENANCE,
		    0.0f, 0U, &output))
			return false;
		if (!write_output(&output, error))
			return false;
		if (!yt_cli_line(line, sizeof(line)))
			return true;
		if (line[0] == '\0')
			return true;
		if (((unsigned char)line[0] & 0xdfU) == 'Y'
		    || ((unsigned char)line[0] & 0xdfU) == 'N')
			break;
	}
	if (!yt_current_date_serial(&game->clock, game->config.epoch_year,
	    &serial,
	    &adjusted, error))
		return false;
	if (((unsigned char)line[0] & 0xdfU) == 'Y') {
		memcpy(raw_marker, raw_allow, sizeof(raw_marker));
	}
	else if (qb_mbf32_encode((float)serial, raw_marker)
	    == QB_MBF_OVERFLOW) {
		if (error != NULL)
			error->status = YT_RANGE;
		return false;
	}
	if (qb_mbf32_encode((float)adjusted, raw_epoch) == QB_MBF_OVERFLOW) {
		if (error != NULL)
			error->status = YT_RANGE;
		return false;
	}
	overlays[0] = (struct yt_config_overlay){YT_F81, raw_marker,
	    sizeof(raw_marker)};
	overlays[1] = (struct yt_config_overlay){YT_F45, raw_epoch,
	    sizeof(raw_epoch)};
	if (!yt_config_apply_loaded_overlays(&game->database,
	    &game->config.record, overlays, YT_ARRAY_LEN(overlays), &updated,
	    error))
		return false;
	game->config.record = updated;
	game->config.epoch_year = (float)adjusted;
	game->config.last_maintenance =
	    ((unsigned char)line[0] & 0xdfU) == 'Y' ? 0.0f : (float)serial;
	return true;
}

static bool
edit_scoreboard(struct yt_game *game, uint8_t working_path[41],
    size_t *working_path_length, struct yt_error *error)
{
	struct yt_config_output_result output;
	struct yt_config_overlay overlays[2];
	struct yt_record updated;
	char line[160];
	uint8_t fixed[41];
	uint8_t raw_length[4];
	const char *stored;
	size_t length;

	if (!yt_config_compose_scoreboard_prompt(0U, &output))
		return false;
	if (!write_output(&output, error))
		return false;
	if (!yt_cli_line(line, sizeof(line)))
		return true;
	if (strlen(line) > 41U) {
		if (!yt_config_compose_scoreboard_too_long(output.final_column,
		    &output))
			return false;
		if (!write_output(&output, error))
			return false;
		return true;
	}
	stored = line[0] == '\0' ? "YTSCORE.ASC" : line;
	length = strlen(stored);
	memset(fixed, ' ', sizeof(fixed));
	memcpy(fixed, stored, length);
	if (qb_mbf32_encode((float)length, raw_length) == QB_MBF_OVERFLOW) {
		if (error != NULL)
			error->status = YT_RANGE;
		return false;
	}
	overlays[0] = (struct yt_config_overlay){0U, fixed, sizeof(fixed)};
	overlays[1] = (struct yt_config_overlay){YT_F41, raw_length,
	    sizeof(raw_length)};
	if (!yt_config_apply_overlays(&game->database, overlays,
	    YT_ARRAY_LEN(overlays), &updated, error))
		return false;
	game->config.record = updated;
	snprintf(game->config.scoreboard, sizeof(game->config.scoreboard), "%s",
	    stored);
	game->config.scoreboard_length = length;
	memcpy(working_path, stored, length);
	*working_path_length = length;
	return true;
}

static bool
edit_headquarters(struct yt_game *game, struct yt_error *error)
{
	struct yt_config_output_result output;
	struct qb_val_result parsed;
	struct yt_record updated;
	enum yt_config_hq_route route;
	char line[160];
	float raw;
	float upper = game->config.port_offset
	    - game->config.sector_offset;

	if (!yt_config_compose_hq_prompt(game->config.headquarters, upper, 0U,
	    &output))
		return false;
	if (!write_output(&output, error))
		return false;
	if (!yt_cli_line(line, sizeof(line)))
		return true;
	if (line[0] == '\0')
		return true;
	parsed = qb_val(line);
	raw = (float)(parsed.valid ? parsed.value : 0.0);
	if (!yt_config_hq_in_range(raw, upper)) {
		if (!yt_config_compose_hq_diagnostic(YT_CONFIG_HQ_INVALID,
		    output.final_column, &output))
			return false;
		if (!write_output(&output, error))
			return false;
		return true;
	}
	if (!yt_config_headquarters_relocate(&game->database, &game->config,
	    raw, &route, &updated, error))
		return false;
	if (route == YT_CONFIG_HQ_ROUTE_OCCUPIED) {
		if (!yt_config_compose_hq_diagnostic(YT_CONFIG_HQ_OCCUPIED,
		    output.final_column, &output))
			return false;
		if (!write_output(&output, error))
			return false;
		return true;
	}
	game->config.record = updated;
	game->config.headquarters = raw;
	return true;
}

static bool
edit_planets(struct yt_game *game, struct yt_error *error)
{
	uint8_t names[76][YT_TEXT_FIELD_SIZE];
	bool active[76] = {false};
	unsigned active_count = 0U;
	struct yt_config_output_result output;
	int logical;

	memset(names, 0, sizeof(names));
	for (logical = 1; logical <= 75; ++logical) {
		struct yt_planet planet;

		if (!yt_game_read_planet(game, logical, &planet, error))
			return false;
		if (planet.name_length == 0U)
			continue;
		active[logical] = true;
		++active_count;
		memcpy(names[logical], planet.record.bytes, YT_TEXT_FIELD_SIZE);
	}
	if (!yt_config_compose_planet_entry(active_count, 0U, &output))
		return false;
	if (!write_output(&output, error))
		return false;
	if (active_count == 0U)
		return true;
	for (;;) {
		int raw_key;
		uint8_t folded;

		if (!yt_config_compose_planet_menu(0U, &output))
			return false;
		if (!write_output(&output, error))
			return false;
		raw_key = yt_cli_key();
		if (raw_key == EOF)
			return true;
		if (raw_key == '\n')
			raw_key = '\r';
		if (!yt_config_compose_planet_key_echo((uint8_t)raw_key,
		    output.final_column, &folded, &output))
			return false;
		if (!write_output(&output, error))
			return false;
		if (folded == '\r')
			return true;
		if (folded == 'L') {
			if (!yt_config_compose_planet_list_header(0U, &output))
				return false;
			if (!write_output(&output, error))
				return false;
			for (logical = 1; logical <= 75; ++logical) {
				if (!active[logical])
					continue;
				if (!yt_config_compose_planet_list_row(logical,
				    names[logical], YT_TEXT_FIELD_SIZE, 0U, &output))
					return false;
				if (!write_output(&output, error))
					return false;
				if (yt_config_planet_pause_after(logical,
				    active_count)) {
					if (!yt_config_compose_planet_pause(
					    output.final_column, &output))
						return false;
					if (!write_output(&output, error))
						return false;
					(void)yt_cli_key();
					if (!yt_config_compose_planet_blank(
					    output.final_column, &output))
						return false;
					if (!write_output(&output, error))
						return false;
				}
			}
			if (!yt_config_compose_planet_blank(0U, &output))
				return false;
			if (!write_output(&output, error))
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

			if (!yt_config_compose_planet_number_prompt(0U, &output))
				return false;
			if (!write_output(&output, error))
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
				    output.final_column, &output))
					return false;
				if (!write_output(&output, error))
					return false;
				continue;
			}
			selected = (int)qb_cint(raw, &overflow);
			if (overflow)
				continue;
			if (!active[selected]) {
				if (!yt_config_compose_planet_invalid(
				    (const uint8_t *)entered, strlen(entered),
				    output.final_column, &output))
					return false;
				if (!write_output(&output, error))
					return false;
				continue;
			}
			if (yt_config_planet_selection_protected(raw)) {
				if (!yt_config_compose_planet_protected(
				    output.final_column, &output))
					return false;
				if (!write_output(&output, error))
					return false;
				continue;
			}
			for (;;) {
				if (!yt_config_compose_planet_edit(
				    names[selected], YT_TEXT_FIELD_SIZE, 0U, &output))
					return false;
				if (!write_output(&output, error))
					return false;
				if (!yt_cli_line(name, sizeof(name)))
					return true;
				name[41] = '\0';
				qb_title_case(name);
				if (name[0] == '\0') {
					if (!yt_config_compose_planet_blank(
					    output.final_column, &output))
						return false;
					if (!write_output(&output, error))
						return false;
					break;
				}
				for (;;) {
					if (!yt_config_compose_planet_confirmation(
					    (const uint8_t *)name, strlen(name),
					    output.final_column, &output))
						return false;
					if (!write_output(&output, error))
						return false;
					raw_key = yt_cli_key();
					if (raw_key == EOF)
						return true;
					if (!yt_config_compose_planet_response_echo(
					    (uint8_t)raw_key, output.final_column,
					    &folded, &output))
						return false;
					if (!write_output(&output, error))
						return false;
					if (folded == 'Y')
						goto save_planet_name;
					if (folded == 'N')
						break;
				}
				if (!yt_config_compose_planet_cancel(
				    (const uint8_t *)name, strlen(name),
				    output.final_column, &output))
					return false;
				if (!write_output(&output, error))
					return false;
			}
			continue;
save_planet_name:
			{
				struct yt_planet planet;

				if (!yt_game_read_planet(game, selected, &planet, error))
					return false;
				snprintf(planet.name, sizeof(planet.name), "%s", name);
				planet.name_length = strlen(name);
				if (!yt_game_write_planet(game, selected, &planet, error))
					return false;
			}
			memset(names[selected], ' ', YT_TEXT_FIELD_SIZE);
			memcpy(names[selected], name, strlen(name));
			if (!yt_config_compose_planet_saved(output.final_column, &output))
				return false;
			if (!write_output(&output, error))
				return false;
			(void)yt_cli_key();
			return true;
		}
	}
}

static bool
byte_string_contains(const uint8_t *haystack, size_t haystack_length,
    const uint8_t *needle, size_t needle_length)
{
	size_t offset;

	if (needle_length == 0U)
		return true;
	if (needle_length > haystack_length)
		return false;
	for (offset = 0U; offset + needle_length <= haystack_length; ++offset) {
		if (memcmp(haystack + offset, needle, needle_length) == 0)
			return true;
	}
	return false;
}

static bool
edit_ports(struct yt_game *game, struct yt_error *error)
{
	struct yt_config_output_result output;
	char search[160];
	uint8_t upper[160];
	size_t search_length;
	int logical;
	bool matched = false;

	if (!yt_config_compose_port_search_prompt(0U, &output))
		return false;
	if (!write_output(&output, error))
		return false;
	if (!yt_cli_line(search, sizeof(search)))
		return true;
	if (!yt_config_compose_port_search_echo((const uint8_t *)search,
	    strlen(search), output.final_column, &output))
		return false;
	if (!write_output(&output, error))
		return false;
	if (search[0] == '\0')
		return true;
	search_length = strlen(search);
	memcpy(upper, search, search_length);
	qb_ascii_upper_n(upper, search_length);
	for (logical = 2; logical <= 300; ++logical) {
		struct yt_port port;
		uint8_t candidate[YT_TEXT_FIELD_SIZE];
		size_t name_length;

		if (!yt_game_read_port(game, logical, &port, error))
			return false;
		name_length = port.name_length;
		if (name_length > sizeof(candidate))
			name_length = sizeof(candidate);
		memcpy(candidate, port.record.bytes, name_length);
		qb_ascii_upper_n(candidate, name_length);
		if (!byte_string_contains(candidate, name_length, upper,
		    search_length))
			continue;
		matched = true;
		for (;;) {
			int raw_key;
			uint8_t folded;

			if (!yt_config_compose_port_match_prompt(
			    port.record.bytes, name_length,
			    output.final_column, &output))
				return false;
			if (!write_output(&output, error))
				return false;
			raw_key = yt_cli_key();
			if (raw_key == EOF)
				return true;
			if (!yt_config_compose_port_response_echo((uint8_t)raw_key,
			    output.final_column, &folded, &output))
				return false;
			if (!write_output(&output, error))
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
			    output.final_column, &output))
				return false;
			if (!write_output(&output, error))
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
				    output.final_column, &output))
					return false;
				if (!write_output(&output, error))
					return false;
				raw_key = yt_cli_key();
				if (raw_key == EOF)
					return true;
				if (!yt_config_compose_port_response_echo(
				    (uint8_t)raw_key, output.final_column,
				    &folded, &output))
					return false;
				if (!write_output(&output, error))
					return false;
				if (folded == 'Y')
					break;
				if (folded == 'N') {
					if (!yt_config_compose_port_cancel(
					    output.final_column, &output))
						return false;
					if (!write_output(&output, error))
						return false;
					if (!yt_config_compose_port_wait_prompt(
					    output.final_column, &output))
						return false;
					if (!write_output(&output, error))
						return false;
					(void)yt_cli_line(search, sizeof(search));
					return true;
				}
			}
			snprintf(port.name, sizeof(port.name), "%s", name);
			port.name_length = strlen(name);
			if (!yt_game_write_port(game, logical, &port, error))
				return false;
			if (!yt_config_compose_port_saved(output.final_column,
			    &output))
				return false;
			if (!write_output(&output, error))
				return false;
			if (!yt_config_compose_port_wait_prompt(
			    output.final_column, &output))
				return false;
			if (!write_output(&output, error))
				return false;
			(void)yt_cli_line(search, sizeof(search));
			return true;
		}
	}
	if (matched) {
		if (!yt_config_compose_port_end_list(output.final_column, &output))
			return false;
		if (!write_output(&output, error))
			return false;
		if (!yt_config_compose_port_wait_prompt(output.final_column,
		    &output))
			return false;
		if (!write_output(&output, error))
			return false;
		(void)yt_cli_line(search, sizeof(search));
	}
	else {
		if (!yt_config_compose_port_not_found(output.final_column, &output))
			return false;
		if (!write_output(&output, error))
			return false;
	}
	return true;
}

static bool
edit_aliases(struct yt_game *game, struct yt_error *error)
{
	struct yt_name_file names;
	struct yt_config_output_result output;
	unsigned player_count;

	if (!yt_names_load("YTNAME.DAT", &names, error))
		return false;
	if (names.count == 0U) {
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
	if (!yt_config_compose_alias_entry(player_count, 0U, &output)) {
		yt_names_free(&names);
		return false;
	}
	if (!write_output(&output, error)) {
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

		if (!yt_config_compose_alias_menu(0U, &output)) {
			yt_names_free(&names);
			return false;
		}
		if (!write_output(&output, error)) {
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
		    output.final_column, &folded, &output)) {
			yt_names_free(&names);
			return false;
		}
		if (!write_output(&output, error)) {
			yt_names_free(&names);
			return false;
		}
		if (folded == '\r') {
			yt_names_free(&names);
			return true;
		}
		if (folded == 'L') {
			if (!yt_config_compose_alias_list_header(0U, &output)) {
				yt_names_free(&names);
				return false;
			}
			if (!write_output(&output, error)) {
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
				    strlen(row->alias_last), 0U, &output)) {
					yt_names_free(&names);
					return false;
				}
				if (!write_output(&output, error)) {
					yt_names_free(&names);
					return false;
				}
				if (yt_config_alias_pause_after((int)index,
				    player_count)) {
					if (!yt_config_compose_alias_pause(
					    output.final_column, &output)) {
						yt_names_free(&names);
						return false;
					}
					if (!write_output(&output, error)) {
						yt_names_free(&names);
						return false;
					}
					(void)yt_cli_key();
					if (!yt_config_compose_alias_blank(
					    output.final_column, &output)) {
						yt_names_free(&names);
						return false;
					}
					if (!write_output(&output, error)) {
						yt_names_free(&names);
						return false;
					}
				}
			}
			if (!yt_config_compose_alias_blank(0U, &output)) {
				yt_names_free(&names);
				return false;
			}
			if (!write_output(&output, error)) {
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
			if (!yt_config_compose_alias_number_prompt(0U, &output)) {
				yt_names_free(&names);
				return false;
			}
			if (!write_output(&output, error)) {
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
				    output.final_column, &output)) {
					yt_names_free(&names);
					return false;
				}
				if (!write_output(&output, error)) {
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
			    &output)) {
				yt_names_free(&names);
				return false;
			}
			if (!write_output(&output, error)) {
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
				    strlen(row->alias_last), 0U, &output)) {
					yt_names_free(&names);
					return false;
				}
				if (!write_output(&output, error)) {
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
					    output.final_column, &output)) {
						yt_names_free(&names);
						return false;
					}
					if (!write_output(&output, error)) {
						yt_names_free(&names);
						return false;
					}
					break;
				}
				for (;;) {
					if (!yt_config_compose_alias_confirmation(
					    (const uint8_t *)entered, strlen(entered),
					    output.final_column, &output)) {
						yt_names_free(&names);
						return false;
					}
					if (!write_output(&output, error)) {
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
					    &folded, &output)) {
						yt_names_free(&names);
						return false;
					}
					if (!write_output(&output, error)) {
						yt_names_free(&names);
						return false;
					}
					if (folded != 'Y' && folded != 'N')
						continue;
					if (!yt_config_compose_alias_blank(
					    output.final_column, &output)) {
						yt_names_free(&names);
						return false;
					}
					if (!write_output(&output, error)) {
						yt_names_free(&names);
						return false;
					}
					if (folded == 'Y')
						goto save_alias;
					if (!yt_config_compose_alias_cancel(
					    output.final_column, &output)) {
						yt_names_free(&names);
						return false;
					}
					if (!write_output(&output, error)) {
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
			if (!yt_names_propagate_alias(&game->database,
			    (const uint8_t *)old_alias, strlen(old_alias),
			    (const uint8_t *)entered, strlen(entered),
			    error)) {
				yt_names_free(&names);
				return false;
			}
			if (!yt_config_compose_alias_saved(0U, &output)) {
				yt_names_free(&names);
				return false;
			}
			if (!write_output(&output, error)) {
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
	if (!yt_database_random_close(&game.database, &error))
		goto open_failed;
	if (!yt_database_open(&game.database, "YTDATA.DAT", YT_OPEN_UPDATE,
	    &error))
		goto open_failed;
	goto open_complete;

open_failed:
	if (error.status != YT_NOT_FOUND)
		goto failure;
	yt_error_clear(&error);
	if (!yt_database_open(&game.database, "YTDATA.DAT", YT_OPEN_CREATE,
	    &error))
		goto failure;

open_complete:
	if (!yt_database_random_lof(&game.database, &file_size, &error))
		goto failure;
	if (file_size == 0) {
		struct yt_config_output_result output;

		if (!yt_config_compose_missing_data(0U, &output))
			goto failure;
		if (!write_output(&output, &error))
			goto failure;
		if (!ytconfig_close_all(&game, &error))
			goto failure;
		if (!yt_file_kill("YTDATA.DAT", &error))
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

		if (!yt_config_load(&game.database, &game.config, &error))
			goto failure;
		if (!yt_current_date_serial(&game.clock,
		    game.config.epoch_year, &today, &year, &error))
			goto failure;
		if (!redraw_repairs(&game, working.maximum_holds, &error))
			goto failure;
		if (!yt_config_compose_menu_prompt(&game.config, &working, today,
		    0U, &output))
			goto failure;
		if (!write_output(&output, &error))
			goto failure;
		fflush(stdout);
		raw_key = yt_cli_key();
		if (raw_key == EOF)
			break;
		if (!yt_config_compose_command_echo((uint8_t)raw_key,
		    output.final_column, &folded, &output))
			goto failure;
		if (!write_output(&output, &error))
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
			struct yt_record updated;
			bool toggled;

			if (!yt_config_toggle_local_screen(&game.database, &updated,
			    &toggled, &error))
				goto failure;
			game.config.record = updated;
			game.config.local_screen = toggled;
			working.local_screen = toggled;
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
			if (!yt_config_compose_exit(output.final_column, &output))
				goto failure;
			if (!write_output(&output, &error))
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
