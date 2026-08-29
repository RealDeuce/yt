#include "qb.h"
#include "yt_cli.h"
#include "yt_config_output.h"
#include "yt_game.h"
#include "yt_names.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool
read_single(const char *prompt, float *value, bool *blank)
{
	char line[160];
	struct qb_val_result parsed;

	fputs(prompt, stdout);
	if (!yt_cli_line(line, sizeof(line)))
		return false;
	*blank = line[0] == '\0';
	parsed = qb_val(line);
	*value = (float)(parsed.valid ? parsed.value : 0.0);
	return true;
}

static bool
confirm(const char *prompt)
{
	fputs(prompt, stdout);
	fflush(stdout);
	for (;;) {
		int key;

		key = yt_cli_key();
		if (key == EOF)
			return false;
		switch ((unsigned char)key & 0xdfU) {
		case 'Y':
			return true;
		case 'N':
			return false;
		default:
			break;
		}
	}
}

static bool
store_config(struct yt_game *game, struct yt_error *error)
{
	return yt_config_store(&game->database, &game->config, error)
	    && yt_database_flush(&game->database, error);
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
	static const uint8_t raw_clear[4] = {0x00, 0x00, 0x80, 0x00};
	struct yt_config_output_result output;
	struct qb_val_result parsed;
	char line[160];
	float raw;
	bool overflow;
	int candidate_number;
	int old_number;
	float upper = game->config.port_offset
	    - game->config.sector_offset;
	struct yt_sector candidate;
	struct yt_sector old;
	struct yt_sector sector_one;
	float merged;

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
	candidate_number = (int)qb_cint(raw, &overflow);
	if (overflow)
		return true;
	if (!yt_game_read_sector(game, candidate_number, &candidate, error))
		return false;
	if (qb_cint(candidate.planet, &overflow) != 0
	    || (qb_cint(candidate.fighters, &overflow) != 0
	    && candidate.fighter_owner != -1.0f)) {
		if (!yt_config_compose_hq_diagnostic(YT_CONFIG_HQ_OCCUPIED,
		    output.final_column, &output) || !write_output(&output, error))
			return false;
		return true;
	}
	old_number = (int)qb_cint(game->config.headquarters, &overflow);
	if (overflow || !yt_game_read_sector(game, old_number, &old, error))
		return false;
	merged = old.fighters + candidate.fighters;
	yt_record_set_raw_number(&old.record, YT_F93, raw_clear);
	yt_record_set_raw_number(&old.record, YT_F85, raw_clear);
	yt_record_set_raw_number(&old.record, YT_F81, raw_clear);
	if (!yt_database_write(&game->database,
	    (size_t)yt_sector_basic_record(&game->config, old_number),
	    &old.record, error)
	    || !yt_game_read_sector(game, candidate_number, &candidate, error))
		return false;
	candidate.fighter_owner = -1.0f;
	candidate.fighters = merged;
	candidate.planet = game->config.total_records
	    - game->config.planet_offset;
	if (!yt_game_write_sector(game, candidate_number, &candidate, error)
	    || !yt_game_read_sector(game, 1, &sector_one, error))
		return false;
	sector_one.metadata = raw;
	if (!yt_game_write_sector(game, 1, &sector_one, error))
		return false;
	game->config.headquarters = raw;
	return store_config(game, error);
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
fixed_contains(const uint8_t field[41], const char *needle)
{
	size_t length = strlen(needle);
	size_t index;

	if (length == 0 || length > 41U)
		return false;
	for (index = 0; index + length <= 41U; ++index) {
		if (memcmp(field + index, needle, length) == 0)
			return true;
	}
	return false;
}

static bool
edit_aliases(struct yt_game *game, struct yt_error *error)
{
	struct yt_name_file names;

	if (!yt_names_load("YTNAME.DAT", &names, error))
		return false;
	for (;;) {
		char choice[80];
		size_t counter = names.count == 0 ? 0 : names.count - 1U;
		size_t index;

		puts("L) List players  C) Change alias");
		fputs("Press enter to quit. Please Select: ", stdout);
		if (!yt_cli_line(choice, sizeof(choice)) || choice[0] == '\0') {
			yt_names_free(&names);
			return true;
		}
		if (((unsigned char)choice[0] & 0xdfU) == 'L') {
			for (index = 1; index <= counter; ++index) {
				printf("%zu] %s %s a.k.a. %s %s\n", index,
				    names.rows[index].real_first,
				    names.rows[index].real_last,
				    names.rows[index].alias_first,
				    names.rows[index].alias_last);
				if (index % 20U == 0 || index == counter) {
					fputs("[ Pause ]", stdout);
					(void)getchar();
					fputc('\n', stdout);
				}
			}
			continue;
		}
		if (((unsigned char)choice[0] & 0xdfU) == 'C') {
			float raw;
			bool blank;
			bool overflow;
			int selected;
			char entered[180];
			char old_alias[90];
			char first[90];
			char last[90];
			int basic;

			if (!read_single("Edit which player number? ", &raw, &blank)
			    || blank || raw == 0.0f) {
				yt_names_free(&names);
				return true;
			}
			if (raw < 1.0f || raw > (float)counter)
				continue;
			selected = (int)qb_cint(raw, &overflow);
			if (overflow || selected < 1
			    || (size_t)selected > counter)
				continue;
			for (;;) {
				char prompt[260];

				printf("%s %s a.k.a. %s %s\n",
				    names.rows[selected].real_first,
				    names.rows[selected].real_last,
				    names.rows[selected].alias_first,
				    names.rows[selected].alias_last);
				fputs("Please enter new Alias. -=> ", stdout);
				if (!yt_cli_line(entered, sizeof(entered)))
					return true;
				entered[41] = '\0';
				for (index = 0; entered[index] != '\0'; ++index) {
					if (entered[index] == ',')
						entered[index] = ' ';
				}
				qb_title_case(entered);
				if (entered[0] == '\0')
					continue;
				snprintf(prompt, sizeof(prompt),
				    "Change player Alias to %s? [Y/N] -=> ",
				    entered);
				if (confirm(prompt))
					goto save_alias;
				puts("Canceled!");
			}
save_alias:
			snprintf(old_alias, sizeof(old_alias), "%s %s",
			    names.rows[selected].alias_first,
			    names.rows[selected].alias_last);
			qb_title_case(old_alias);
			yt_names_split(entered, first, sizeof(first), last,
			    sizeof(last));
			snprintf(names.rows[selected].alias_first,
			    sizeof(names.rows[selected].alias_first), "%s", first);
			snprintf(names.rows[selected].alias_last,
			    sizeof(names.rows[selected].alias_last), "%s", last);
			if (!yt_names_write("YTNAME.DAT", &names, error)) {
				yt_names_free(&names);
				return false;
			}
			for (basic = 2; basic <= 51; ++basic) {
				struct yt_player player;

				if (!yt_game_read_player(game, basic, &player, error)) {
					yt_names_free(&names);
					return false;
				}
				if (fixed_contains(player.record.bytes, old_alias)) {
					snprintf(player.name, sizeof(player.name), "%s",
					    entered);
					player.name_length = (float)strlen(entered);
					if (!yt_game_write_player(game, basic, &player,
					    error)) {
						yt_names_free(&names);
						return false;
					}
				}
			}
			fputs("New alias saved! Press any key.", stdout);
			(void)yt_cli_key();
			fputc('\n', stdout);
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
	size_t file_size;

	yt_error_clear(&error);
	memset(&game, 0, sizeof(game));
	if (!yt_database_open(&game.database, "YTDATA.DAT", YT_OPEN_UPDATE,
	    &error)) {
		if (error.status != YT_NOT_FOUND)
			goto failure;
		yt_error_clear(&error);
		if (!yt_database_open(&game.database, "YTDATA.DAT",
		    YT_OPEN_CREATE, &error))
			goto failure;
	}
	if (!yt_file_size(game.database.path, &file_size, &error))
		goto failure;
	if (file_size == 0) {
		struct yt_config_output_result output;

		if (!yt_config_compose_missing_data(0U, &output)
		    || !write_output(&output, &error))
			goto failure;
		yt_database_close(&game.database);
		if (!yt_file_delete("ytdata.dat", true, &error))
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
	yt_game_close(&game);
	return EXIT_SUCCESS;

failure:
	yt_game_close(&game);
	yt_cli_error("YTCONFIG", &error);
	return EXIT_FAILURE;

failure_closed:
	yt_cli_error("YTCONFIG", &error);
	return EXIT_FAILURE;
}
