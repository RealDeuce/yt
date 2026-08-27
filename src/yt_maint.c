#include "yt_maint.h"

#include "qb.h"
#include "yt_names.h"
#include "yt_score.h"
#include "yt_text.h"

#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

struct maint_state {
	struct yt_game game;
	float *player_sector;
	float *player_cloak;
	int player_count;
	int sector_count;
	int port_count;
	int planet_count;
	int today;
};

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
sadd(float left, float right)
{
	volatile float result = left + right;
	return result;
}

static float
ssub(float left, float right)
{
	volatile float result = left - right;
	return result;
}

static float
smul(float left, float right)
{
	volatile float result = left * right;
	return result;
}

static float
sdiv(float left, float right)
{
	volatile float result = left / right;
	return result;
}

static float
sint(float value)
{
	volatile float result = floorf(value);
	return result;
}

static bool
rnd(struct maint_state *state, float *value, struct yt_error *error)
{
	return yt_random_next(&state->game.random, value, error);
}

/* RANDOMIZE changes the original runtime state but adds no RND callsite. */
static bool
random_integer(struct maint_state *state, int range, int *value,
    struct yt_error *error)
{
	float selection;

	if (range <= 0) {
		set_error(error, YT_RANGE, "maintenance random range", "");
		return false;
	}
	if (!rnd(state, &selection, error))
		return false;
	*value = (int)floorf(smul(selection, (float)range)) + 1;
	return true;
}

static bool
nested_integer(struct maint_state *state, int count, int range, int *value,
    struct yt_error *error)
{
	int index;
	int current = range;

	if (count < 1 || range < 1) {
		set_error(error, YT_RANGE, "maintenance nested random", "");
		return false;
	}
	for (index = 0; index < count; ++index) {
		float selection;

		if (!rnd(state, &selection, error))
			return false;
		current = (int)floorf(smul(selection, (float)current)) + 1;
	}
	*value = current;
	return true;
}

bool
yt_news_append(const char *text, struct yt_error *error)
{
	return yt_text_append_line("YTNEWS.DAT", (const uint8_t *)text,
	    strlen(text), error);
}

bool
yt_radio_append_maintenance(const char *text, float sender, float recipient,
    struct yt_error *error)
{
	struct yt_radio_record record;
	char resolved[512];
	FILE *file;

	if (!yt_resolve_case_path("YTRMSG.DAT", true, resolved,
	    sizeof(resolved), error))
		return false;
	file = fopen(resolved, "ab");
	if (file == NULL) {
		set_error(error, YT_IO_ERROR, "append radio", resolved);
		return false;
	}
	memset(&record, 0, sizeof(record));
	yt_radio_set_number(&record, 0, recipient == -2.0f ? 20.0f : 1.0f);
	yt_radio_set_number(&record, 4, recipient);
	yt_radio_set_number(&record, 8, sender);
	yt_radio_set_text(&record, (const uint8_t *)text, strlen(text), 72);
	if (fwrite(record.bytes, 1, sizeof(record.bytes), file)
	    != sizeof(record.bytes) || fclose(file) != 0) {
		set_error(error, YT_IO_ERROR, "append radio", resolved);
		return false;
	}
	return true;
}

bool
yt_radio_compact(struct yt_error *error)
{
	char source_path[512];
	FILE *source = NULL;
	FILE *dest = NULL;
	struct yt_radio_record input;
	struct yt_radio_record output;
	bool result = false;

	dest = fopen("temp", "w+b");
	if (dest == NULL) {
		set_error(error, YT_IO_ERROR, "create radio temporary", "temp");
		return false;
	}
	if (!yt_resolve_case_path("YTRMSG.DAT", true, source_path,
	    sizeof(source_path), error))
		goto done;
	source = fopen(source_path, "a+b");
	if (source == NULL) {
		set_error(error, YT_IO_ERROR, "open radio", source_path);
		goto done;
	}
	if (fseek(source, 0, SEEK_SET) != 0) {
		set_error(error, YT_IO_ERROR, "rewind radio", source_path);
		goto done;
	}
	while (fread(input.bytes, 1, sizeof(input.bytes), source)
	    == sizeof(input.bytes)) {
		if (yt_radio_get_number(&input, 0) == 0.0f)
			continue;
		memset(&output, 0, sizeof(output));
		memcpy(output.bytes, input.bytes, 12);
		memcpy(output.bytes + 12, input.bytes + 12, 72);
		if (fwrite(output.bytes, 1, sizeof(output.bytes), dest)
		    != sizeof(output.bytes)) {
			set_error(error, YT_IO_ERROR, "write radio temporary",
			    "temp");
			goto done;
		}
	}
	if (ferror(source)) {
		set_error(error, YT_IO_ERROR, "read radio", source_path);
		goto done;
	}
	if (fclose(source) != 0) {
		source = NULL;
		set_error(error, YT_IO_ERROR, "close radio", source_path);
		goto done;
	}
	source = NULL;
	if (fclose(dest) != 0) {
		dest = NULL;
		set_error(error, YT_IO_ERROR, "close radio temporary", "temp");
		goto done;
	}
	dest = NULL;
	if (!yt_file_delete("YTRMSG.DAT", false, error)
	    || !yt_file_rename("Temp", "ytrmsg.dat", error))
		goto done;
	result = true;

done:
	if (source != NULL)
		fclose(source);
	if (dest != NULL)
		fclose(dest);
	return result;
}

static bool
rotate_news(struct yt_error *error)
{
	FILE *file;

	/* The two empty OPEN/CLOSE operations precede KILL and NAME. */
	file = fopen("YTNEWS.DAT", "ab");
	if (file == NULL) {
		set_error(error, YT_IO_ERROR, "open current news", "YTNEWS.DAT");
		return false;
	}
	fclose(file);
	file = fopen("YTYNEWS.DAT", "ab");
	if (file == NULL) {
		set_error(error, YT_IO_ERROR, "open old news", "YTYNEWS.DAT");
		return false;
	}
	fclose(file);
	if (!yt_file_delete("YTYNEWS.DAT", false, error))
		return false;
	return yt_file_rename("YTNEWS.DAT", "YTYNEWS.DAT", error);
}

static bool
write_maintenance_header(struct yt_error *error)
{
	struct yt_clock_value time_now;
	struct yt_clock_value date_now;
	char date[11];
	char time_text[9];
	char line[160];

	if (!yt_platform_clock(&time_now, error)
	    || !yt_platform_clock(&date_now, error))
		return false;
	yt_format_time(&time_now, time_text);
	yt_format_date(&date_now, date);
	snprintf(line, sizeof(line),
	    "%s %s: Maintenance Program Ran (Revision 03/14/94)",
	    time_text, date);
	return yt_news_append(line, error);
}

static bool
clear_protected_mines(struct maint_state *state, struct yt_error *error)
{
	int sector;

	for (sector = 1; sector <= 7; ++sector) {
		struct yt_sector record;

		if (!yt_game_read_sector(&state->game, sector, &record, error))
			return false;
		record.mines = 0.0f;
		if (!yt_game_write_sector(&state->game, sector, &record, error))
			return false;
	}
	return true;
}

static bool
remove_from_teams(struct maint_state *state, int player_record,
    struct yt_error *error)
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
		if (changed && !yt_database_write(&state->game.database,
		    (size_t)yt_sector_basic_record(&state->game.config, team),
		    &sector.record, error))
			return false;
	}
	return true;
}

static bool
invalidate_radio(int player_record, struct yt_error *error)
{
	char path[512];
	FILE *file;
	struct yt_radio_record record;
	long offset;

	if (!yt_resolve_case_path("YTRMSG.DAT", true, path, sizeof(path),
	    error))
		return false;
	file = fopen(path, "a+b");
	if (file == NULL) {
		set_error(error, YT_IO_ERROR, "open radio", path);
		return false;
	}
	if (fseek(file, 0, SEEK_SET) != 0) {
		fclose(file);
		set_error(error, YT_IO_ERROR, "rewind radio", path);
		return false;
	}
	while ((offset = ftell(file)) >= 0
	    && fread(record.bytes, 1, sizeof(record.bytes), file)
	    == sizeof(record.bytes)) {
		if (yt_radio_get_number(&record, 4) == (float)player_record
		    || yt_radio_get_number(&record, 8) == (float)player_record) {
			yt_radio_set_number(&record, 0, 0.0f);
			if (fseek(file, offset, SEEK_SET) != 0
			    || fwrite(record.bytes, 1, sizeof(record.bytes), file)
			    != sizeof(record.bytes)
			    || fseek(file, offset + (long)sizeof(record.bytes),
			    SEEK_SET) != 0) {
				fclose(file);
				set_error(error, YT_IO_ERROR, "update radio", path);
				return false;
			}
		}
	}
	if (ferror(file) || fclose(file) != 0) {
		set_error(error, YT_IO_ERROR, "scan radio", path);
		return false;
	}
	return true;
}

static bool
remove_alias(const char *player_name, struct yt_error *error)
{
	struct yt_name_file names;
	char first[128];
	char last[128];
	size_t read_index;
	size_t write_index = 0;

	if (!yt_names_load("YTNAME.DAT", &names, error))
		return false;
	yt_names_split(player_name, first, sizeof(first), last, sizeof(last));
	for (read_index = 0; read_index < names.count; ++read_index) {
		if (strcmp(names.rows[read_index].alias_first, first) == 0
		    && strcmp(names.rows[read_index].alias_last, last) == 0)
			continue;
		if (write_index != read_index)
			names.rows[write_index] = names.rows[read_index];
		++write_index;
	}
	names.count = write_index;
	if (!yt_names_write("tempwork", &names, error)) {
		yt_names_free(&names);
		return false;
	}
	yt_names_free(&names);
	if (!yt_file_delete("YTNAME.DAT", false, error)
	    || !yt_file_rename("tempwork", "ytname.dat", error))
		return false;
	return true;
}

static bool
expire_player(struct maint_state *state, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	int logical;

	state->player_sector[player_record] = 0.0f;
	state->player_cloak[player_record] = 0.0f;
	player->lottery_plays = 0.0f;
	if (!remove_from_teams(state, player_record, error))
		return false;
	for (logical = 1; logical <= state->planet_count; ++logical) {
		struct yt_planet planet;

		if (!yt_game_read_planet(&state->game, logical, &planet, error))
			return false;
		if (planet.owner == (float)player_record) {
			planet.owner = 0.0f;
			planet.ground_forces = 0.0f;
			if (!yt_game_write_planet(&state->game, logical, &planet,
			    error))
				return false;
		}
	}
	player->name_length = 0.0f;
	player->team = 0.0f;
	if (!yt_game_write_player(&state->game, player_record, player, error))
		return false;
	for (logical = 1; logical <= state->sector_count; ++logical) {
		struct yt_sector sector;

		if (!yt_game_read_sector(&state->game, logical, &sector, error))
			return false;
		if (sector.fighter_owner == (float)player_record) {
			sector.fighter_owner = 0.0f;
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
		if (other.killed_by == (float)player_record) {
			other.killed_by = -98.0f;
			if (!yt_game_write_player(&state->game, logical, &other,
			    error))
				return false;
		}
	}
	return remove_alias(player->name, error);
}

static bool
maintain_players(struct maint_state *state, struct yt_error *error)
{
	const float cloak_charge = -0.05000000074505806f;
	float cutoff = ssub((float)state->today,
	    state->game.config.retention_days);
	int record;

	for (record = 2; record <= state->player_count + 1; ++record) {
		struct yt_player player;
		float cloak;

		if (!yt_game_read_player(&state->game, record, &player, error))
			return false;
		if (player.name_length == 0.0f)
			continue;
		cloak = player.cloak;
		if (cloak < 0.0f)
			cloak = 1.0f;
		state->player_sector[record] = player.sector;
		state->player_cloak[record] = cloak;
		if (cloak > 0.0f) {
			char line[256];

			cloak = sadd(cloak, cloak_charge);
			if (cloak < 0.0f)
				cloak = 0.0f;
			player.cloak = cloak;
			if (!yt_game_write_player(&state->game, record, &player,
			    error))
				return false;
			if (cloak == 0.0f) {
				snprintf(line, sizeof(line),
				    " *** Cloaking Device Energy Expired for %s!",
				    player.name);
				if (!yt_news_append(line, error)
				    || !yt_radio_append_maintenance(
				    "Your Cloaking Device Energy has Expired!",
				    -2.0f, (float)record, error))
					return false;
			}
		}
		if (player.last_active <= cutoff && player.killed_by != 0.0f) {
			char line[256];

			snprintf(line, sizeof(line), " *** %s deleted from game",
			    player.name);
			if (!yt_news_append(line, error)
			    || !expire_player(state, record, &player, error))
				return false;
		}
	}
	return true;
}

static bool
current_day_minute(struct maint_state *state, float *day, float *minute,
    struct yt_error *error)
{
	int serial;

	if (!yt_current_date_serial(state->game.config.epoch_year, &serial,
	    NULL, error))
		return false;
	*day = (float)serial;
	*minute = (float)(yt_platform_timer() / 60.0);
	return true;
}

static float
elapsed_days(float day, float minute, float old_day, float old_minute)
{
	float elapsed = sadd(ssub(day, old_day),
	    sdiv(ssub(minute, old_minute), 1440.0f));

	if (elapsed > 10.0f || elapsed < 0.0f)
		elapsed = 10.0f;
	return elapsed;
}

static bool
maintain_ports(struct maint_state *state, struct yt_error *error)
{
	int plagued = 0;
	int logical;

	for (logical = 1; logical <= state->port_count; ++logical) {
		struct yt_port port;
		float day;
		float minute;
		float elapsed;
		double stock[3];
		int commodity;

		if (!yt_game_read_port(&state->game, logical, &port, error)
		    || !current_day_minute(state, &day, &minute, error))
			return false;
		elapsed = elapsed_days(day, minute, port.last_day,
		    port.last_minute);
		for (commodity = 0; commodity < 3; ++commodity) {
			stock[commodity] = (double)port.stock[commodity]
			    + (double)smul(port.production[commodity], elapsed);
			if (stock[commodity] / 10.0
			    > (double)port.production[commodity])
				port.production[commodity] =
				    (float)(stock[commodity] / 10.0);
		}
		if (sadd(sadd(port.production[0], port.production[1]),
		    port.production[2]) > 16000000.0f) {
			float maximum = 0.0f;
			int selected = 0;

			++plagued;
			for (commodity = 0; commodity < 3; ++commodity) {
				if (port.production[commodity] > 500.0f) {
					float sample;

					if (!rnd(state, &sample, error))
						return false;
					port.production[commodity] = sadd(
					    smul(sample, port.production[commodity]),
					    500.0f);
				}
			}
			for (commodity = 0; commodity < 3; ++commodity) {
				float cap = smul(port.production[commodity], 10.0f);

				if (stock[commodity] > (double)cap)
					stock[commodity] = (double)cap;
				if (stock[commodity] > (double)maximum) {
					maximum = (float)stock[commodity];
					selected = commodity + 1;
				}
				port.factor[commodity] =
				    -fabsf(port.factor[commodity]);
			}
			port.commodity_class = (float)(4 - selected);
			if (selected > 0)
				port.factor[selected - 1] =
				    fabsf(port.factor[selected - 1]);
		}
		for (commodity = 0; commodity < 3; ++commodity)
			port.stock[commodity] = (float)stock[commodity];
		port.last_day = day;
		port.last_minute = minute;
		if (!yt_game_write_port(&state->game, logical, &port, error))
			return false;
	}
	if (plagued > 0) {
		char count[48];
		char line[220];

		qb_str_double(count, sizeof(count), (double)plagued);
		snprintf(line, sizeof(line),
		    " ***%s ports contracted the plague and lost productivity! ***",
		    count);
		if (!yt_news_append(line, error))
			return false;
	}
	return true;
}

static bool
maintain_one_planet(struct maint_state *state, int logical,
    struct yt_planet *planet, struct yt_error *error)
{
	const float one_percent = 0.009999999776482582f;
	const float missile_multiplier = 0.000009999999747378752f;
	const float mine_multiplier = 0.000003999999989900971f;
	float day;
	float minute;
	float elapsed;
	float production[9];
	float quantity[9];
	float contribution[9];
	float sum;
	float old_total;
	float old_ground;
	float old_bank;
	float first;
	float second;
	float third;
	int event = 0;
	int index;

	if (!current_day_minute(state, &day, &minute, error))
		return false;
	for (index = 0; index < 3; ++index) {
		production[index] = planet->production[index];
		quantity[index] = planet->stock[index];
	}
	quantity[3] = planet->fighters;
	quantity[4] = planet->missiles;
	quantity[5] = planet->mines;
	quantity[6] = planet->bank;
	quantity[7] = planet->ground_forces;
	quantity[8] = planet->plasma;
	sum = sadd(sadd(production[0], production[1]), production[2]);
	production[3] = sint(sum);
	production[4] = sint(sdiv(sum, 2500.0f));
	production[5] = sint(sdiv(sum, 25000.0f));
	production[6] = 0.0f;
	production[7] = 0.0f;
	production[8] = sint(smul(sum, missile_multiplier));
	elapsed = elapsed_days(day, minute, planet->last_day,
	    planet->last_minute);
	old_bank = quantity[6];
	contribution[0] = sdiv(old_bank, 10000.0f);
	contribution[1] = sdiv(old_bank, 20000.0f);
	contribution[2] = sdiv(old_bank, 30000.0f);
	contribution[3] = sdiv(old_bank, 500.0f);
	contribution[4] = smul(old_bank, missile_multiplier);
	contribution[5] = smul(old_bank, mine_multiplier);
	contribution[6] = 0.0f;
	contribution[7] = sdiv(old_bank, 10000.0f);
	contribution[8] = (float)((double)old_bank * 0.00000004);

	quantity[6] = sint(sadd(quantity[6],
	    smul(smul(quantity[6], elapsed), one_percent)));
	quantity[7] = sint(sadd(sadd(quantity[7],
	    smul(smul(quantity[7], elapsed), one_percent)),
	    smul(contribution[7], elapsed)));
	for (index = 0; index < 3; ++index)
		production[index] = sadd(production[index],
		    smul(smul(production[index], elapsed), one_percent));
	for (index = 0; index < 6; ++index) {
		quantity[index] = sadd(quantity[index],
		    smul(sadd(production[index], contribution[index]), elapsed));
		if (index < 3
		    && quantity[index] > smul(production[index], 10.0f))
			production[index] = sdiv(quantity[index], 10.0f);
	}
	quantity[8] = sadd(quantity[8],
	    smul(sadd(production[8], contribution[8]), elapsed));
	for (index = 0; index < 3; ++index) {
		if (production[index] < 1.0f)
			production[index] = 1.0f;
	}

	old_total = sadd(sadd(production[0], production[1]), production[2]);
	old_ground = quantity[7];
	if (!rnd(state, &first, error) || !rnd(state, &second, error)
	    || !rnd(state, &third, error))
		return false;
	if (smul(first, old_total)
	    > sadd(smul(second, 16000000.0f), 100000.0f))
		event = 1;
	if (smul(third, old_ground) > 16000000.0f)
		event = 2;
	if (event != 0) {
		char line[300];
		float expense = 0.0f;
		float new_total;

		snprintf(line, sizeof(line),
		    "  -  %s has struck planet %s as a result of overcrowding!",
		    event == 1 ? "A PLAGUE" : "CIVIL WAR", planet->name);
		if (!yt_news_append(line, error))
			return false;
		for (index = 0; index < 3; ++index) {
			float sample;

			if (!rnd(state, &sample, error))
				return false;
			production[index] = smul(sample, production[index]);
		}
		if (quantity[7] > 0.0f) {
			float a;
			float b;

			if (!rnd(state, &a, error) || !rnd(state, &b, error))
				return false;
			quantity[7] = ssub(quantity[7],
			    smul(smul(quantity[7], a), b));
		}
		for (index = 0; index < 3; ++index) {
			float cap = smul(production[index], 10.0f);

			if (quantity[index] > cap)
				quantity[index] = cap;
		}
		if (event == 2) {
			float sample;

			if (!rnd(state, &sample, error))
				return false;
			expense = sint(smul(sample, quantity[6]));
			quantity[6] = (float)((double)quantity[6]
			    - (double)expense);
		}
		new_total = sadd(sadd(production[0], production[1]),
		    production[2]);
		{
			char old_text[48];
			char new_text[48];

			qb_str_double(old_text, sizeof(old_text), old_total);
			qb_str_double(new_text, sizeof(new_text), new_total);
			snprintf(line, sizeof(line),
			    "  -  Productivity reduced from%s units to%s units!",
			    old_text, new_text);
			if (!yt_news_append(line, error))
				return false;
		}
		if (floorf(quantity[7]) != floorf(old_ground)
		    && floorf(quantity[7]) > 0.0f) {
			char old_text[48];
			char new_text[48];

			qb_str_double(old_text, sizeof(old_text), old_ground);
			qb_str_double(new_text, sizeof(new_text), quantity[7]);
			snprintf(line, sizeof(line),
			    "  -  Ground forces reduced from%s to%s units!",
			    old_text, new_text);
			if (!yt_news_append(line, error))
				return false;
		}
		if (event == 2 && expense != 0.0f) {
			char amount[48];

			qb_str_double(amount, sizeof(amount), expense);
			snprintf(line, sizeof(line),
			    "  - %s credits were spent putting down the insurrection!",
			    amount);
			if (!yt_news_append(line, error))
				return false;
		}
	}

	for (index = 0; index < 3; ++index) {
		planet->production[index] = production[index];
		planet->stock[index] = quantity[index];
	}
	planet->fighters = quantity[3];
	planet->missiles = quantity[4];
	planet->mines = quantity[5];
	planet->bank = quantity[6];
	planet->ground_forces = quantity[7];
	planet->plasma = quantity[8];
	planet->last_day = day;
	planet->last_minute = minute;
	return yt_game_write_planet(&state->game, logical, planet, error);
}

static bool
maintain_planets(struct maint_state *state, struct yt_error *error)
{
	int logical;

	for (logical = 1; logical <= state->planet_count; ++logical) {
		struct yt_planet planet;

		if (!yt_game_read_planet(&state->game, logical, &planet, error))
			return false;
		if (planet.name_length > 0.0f
		    && !maintain_one_planet(state, logical, &planet, error))
			return false;
	}
	return true;
}

static bool
find_planet_sector(struct maint_state *state, int planet_number,
    int *sector_number, struct yt_error *error)
{
	int sector;

	*sector_number = 0;
	for (sector = 1; sector <= state->sector_count; ++sector) {
		struct yt_sector record;

		if (!yt_game_read_sector(&state->game, sector, &record, error))
			return false;
		if (record.planet == (float)planet_number) {
			*sector_number = sector;
			return true;
		}
	}
	return true;
}

static bool
choose_free_planet_sector(struct maint_state *state, int *selected,
    struct yt_error *error)
{
	for (;;) {
		struct yt_sector sector;

		if (!random_integer(state, state->sector_count, selected, error)
		    || !yt_game_read_sector(&state->game, *selected, &sector,
		    error))
			return false;
		if (sector.planet <= 0.0f)
			return true;
	}
}

static bool
maintain_wanderer(struct maint_state *state, struct yt_error *error)
{
	int old_sector;
	int new_sector;
	struct yt_planet planet;
	struct yt_sector sector;

	if (!find_planet_sector(state, 1, &old_sector, error)
	    || !yt_game_read_planet(&state->game, 1, &planet, error))
		return false;
	if (old_sector == 0) {
		if (!yt_news_append(
		    "  -  The Wanderer is missing or has been destroyed!",
		    error))
			return false;
		yt_record_set_text(&planet.record,
		    (const uint8_t *)"The Wanderer", 12);
		strcpy(planet.name, "The Wanderer");
		planet.name_length = 12.0f;
		planet.last_day = (float)(state->today - 10);
		planet.production[0] = 5000.0f;
		planet.production[1] = 5000.0f;
		planet.production[2] = 5000.0f;
		planet.stock[0] = planet.stock[1] = planet.stock[2] = 0.0f;
		planet.missiles = 0.0f;
		planet.owner = 0.0f;
		planet.ground_forces = 0.0f;
		planet.bank = 250000.0f;
		planet.mines = 0.0f;
		if (!yt_game_write_planet(&state->game, 1, &planet, error)
		    || !yt_news_append(
		    "  -  The Wanderer regenerated with P.H.O.E.N.I.X. device!",
		    error))
			return false;
	}
	else {
		if (!yt_game_read_sector(&state->game, old_sector, &sector,
		    error))
			return false;
		sector.planet = 0.0f;
		if (!yt_game_write_sector(&state->game, old_sector, &sector,
		    error))
			return false;
	}
	if (!choose_free_planet_sector(state, &new_sector, error)
	    || !yt_game_read_sector(&state->game, new_sector, &sector, error))
		return false;
	sector.planet = 1.0f;
	planet.owner = 0.0f;
	if (planet.bank == 0.0f)
		planet.bank = 250000.0f;
	return yt_game_write_sector(&state->game, new_sector, &sector, error)
	    && yt_game_write_planet(&state->game, 1, &planet, error);
}

static bool
route_next_hop(struct maint_state *state, int source, int target,
    int *next_hop, struct yt_error *error)
{
	int *queue;
	int *previous;
	uint8_t *seen;
	size_t head = 0;
	size_t tail = 0;
	int result = 0;
	bool found = false;

	queue = malloc(((size_t)state->sector_count + 1U) * sizeof(*queue));
	previous = calloc((size_t)state->sector_count + 1U,
	    sizeof(*previous));
	seen = calloc((size_t)state->sector_count + 1U, 1);
	if (queue == NULL || previous == NULL || seen == NULL) {
		free(queue);
		free(previous);
		free(seen);
		set_error(error, YT_NO_MEMORY, "maintenance route", "");
		return false;
	}
	seen[source] = 1;
	queue[tail++] = source;
	while (head < tail && !found) {
		struct yt_sector sector;
		int current = queue[head++];
		int slot;

		if (!yt_game_read_sector(&state->game, current, &sector, error))
			goto done;
		for (slot = 0; slot < 6; ++slot) {
			int neighbor = (int)sector.warps[slot];

			if (neighbor < 1 || neighbor > state->sector_count
			    || seen[neighbor])
				continue;
			seen[neighbor] = 1;
			previous[neighbor] = current;
			queue[tail++] = neighbor;
			if (neighbor == target) {
				found = true;
				break;
			}
		}
	}
	if (found) {
		result = target;
		while (previous[result] != source && previous[result] != 0)
			result = previous[result];
	}
	else {
		char from[48];
		char to[48];
		char line[180];

		qb_str_double(from, sizeof(from), source);
		qb_str_double(to, sizeof(to), target);
		snprintf(line, sizeof(line),
		    "*** Error - Sector path not found - from sector%s to sector %s",
		    from, to);
		if (!yt_news_append(line, error))
			goto done;
	}
	*next_hop = result;
	free(queue);
	free(previous);
	free(seen);
	return true;

done:
	free(queue);
	free(previous);
	free(seen);
	return false;
}

/*
 * Faction maintenance is kept in a separate continuation below.  These
 * declarations make the whole-run order explicit and keep record writes
 * local to the phase that owns them.
 */
static bool maintain_factions(struct maint_state *, struct yt_error *);
static bool super_lottery(struct maint_state *, struct yt_error *);

static bool
store_config_field(struct maint_state *state, size_t offset, float value,
    struct yt_error *error)
{
	yt_record_set_number_if_changed(&state->game.config.record, offset,
	    value);
	return yt_database_write(&state->game.database, 1,
	    &state->game.config.record, error);
}

static bool
store_final_marker(struct maint_state *state, struct yt_error *error)
{
	int serial;

	if (!yt_current_date_serial(state->game.config.epoch_year, &serial,
	    NULL, error))
		return false;
	state->game.config.last_maintenance = (float)serial;
	return store_config_field(state, YT_F81,
	    state->game.config.last_maintenance, error);
}

bool
yt_maintenance_run(struct yt_error *error)
{
	struct maint_state state;
	bool result = false;

	memset(&state, 0, sizeof(state));
	if (!yt_game_open(&state.game, YT_OPEN_UPDATE, error))
		return false;
	yt_config_normalize_maintenance(&state.game.config);
	state.player_count = (int)state.game.config.sector_offset - 1;
	state.sector_count = (int)(state.game.config.port_offset
	    - state.game.config.sector_offset);
	state.port_count = (int)(state.game.config.planet_offset
	    - state.game.config.port_offset);
	state.planet_count = (int)(state.game.config.total_records
	    - state.game.config.planet_offset);
	state.today = state.game.today;
	if (state.game.config.headquarters == 0.0f) {
		state.game.config.headquarters = 85.0f;
		if (!store_config_field(&state, YT_F117,
		    state.game.config.headquarters, error))
			goto done;
	}
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
	if (!clear_protected_mines(&state, error)
	    || !yt_radio_compact(error)
	    || !rotate_news(error)
	    || !write_maintenance_header(error)
	    || !maintain_players(&state, error)
	    || !maintain_ports(&state, error)
	    || !maintain_planets(&state, error)
	    || !maintain_wanderer(&state, error)
	    || !maintain_factions(&state, error)
	    || !super_lottery(&state, error)
	    || !store_final_marker(&state, error)
	    || !yt_score_generate(&state.game, error))
		goto done;
	result = true;

done:
	free(state.player_sector);
	free(state.player_cloak);
	yt_game_close(&state.game);
	return result;
}

static bool
news_number_line(const char *prefix, double first, const char *middle,
    double second, const char *suffix, struct yt_error *error)
{
	char a[64];
	char b[64];
	char line[420];

	qb_str_double(a, sizeof(a), first);
	qb_str_double(b, sizeof(b), second);
	snprintf(line, sizeof(line), "%s%s%s%s%s", prefix, a, middle, b,
	    suffix);
	return yt_news_append(line, error);
}

static bool
news_one_number_line(const char *prefix, double value, const char *suffix,
    struct yt_error *error)
{
	char number[64];
	char line[420];

	qb_str_double(number, sizeof(number), value);
	snprintf(line, sizeof(line), "%s%s%s", prefix, number, suffix);
	return yt_news_append(line, error);
}

static bool
immediate_death_cleanup(struct maint_state *state, int victim_record,
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
	if (!remove_from_teams(state, victim_record, error))
		return false;
	victim->team = 0.0f;
	return yt_game_write_player(&state->game, victim_record, victim, error);
}

static bool
load_xannor_groups(struct maint_state *state, float location[21],
    float size[21], struct yt_error *error)
{
	int group;

	memset(location, 0, 21U * sizeof(*location));
	memset(size, 0, 21U * sizeof(*size));
	for (group = 1; group <= 20; ++group) {
		struct yt_sector metadata;
		int logical;
		int earlier;

		if (!yt_game_read_sector(&state->game, group, &metadata, error))
			return false;
		location[group] = yt_record_get_number(&metadata.record, YT_F105);
		logical = (int)location[group];
		if (logical >= 1 && logical <= state->sector_count) {
			struct yt_sector host;

			if (!yt_game_read_sector(&state->game, logical, &host,
			    error))
				return false;
			if (host.fighter_owner == -1.0f) {
				size[group] = host.fighters;
				host.fighters = 0.0f;
				host.fighter_owner = 0.0f;
				if (!yt_game_write_sector(&state->game, logical,
				    &host, error))
					return false;
			}
		}
		for (earlier = 1; earlier < group; ++earlier) {
			if (location[group] > 0.0f
			    && location[earlier] == location[group]) {
				size[earlier] = sadd(size[earlier], size[group]);
				location[group] = 0.0f;
				size[group] = 0.0f;
				break;
			}
		}
	}
	return true;
}

static bool
maintain_xannoron(struct maint_state *state, struct yt_error *error)
{
	int headquarters = (int)state->game.config.headquarters;
	struct yt_sector sector;
	struct yt_planet planet;
	bool rebuild;
	float sample;

	if (headquarters < 1 || headquarters > state->sector_count) {
		set_error(error, YT_RANGE, "Xannor headquarters", "YTDATA.DAT");
		return false;
	}
	if (!yt_game_read_sector(&state->game, headquarters, &sector, error)
	    || !yt_game_read_planet(&state->game, state->planet_count, &planet,
	    error))
		return false;
	rebuild = sector.planet != (float)state->planet_count;
	if (rebuild) {
		yt_record_set_text(&planet.record, (const uint8_t *)"Xannoron", 8);
		strcpy(planet.name, "Xannoron");
		planet.name_length = 8.0f;
		planet.last_day = (float)state->today;
		planet.last_minute = sint((float)(yt_platform_timer() / 60.0));
		planet.production[0] = 100000.0f;
		planet.production[1] = 100000.0f;
		planet.production[2] = 100000.0f;
		planet.stock[0] = planet.stock[1] = planet.stock[2] = 0.0f;
		planet.missiles = 0.0f;
		planet.owner = -1.0f;
		if (!rnd(state, &sample, error))
			return false;
		planet.ground_forces = sint(smul(sample, 250.0f));
		if (!rnd(state, &sample, error))
			return false;
		planet.bank = sadd(100000.0f, smul(sample, 10000000.0f));
		planet.mines = 0.0f;
		if (!yt_news_append("  -  The Xannor have made a Planet!",
		    error) || !yt_game_write_planet(&state->game,
		    state->planet_count, &planet, error))
			return false;
		sector.planet = (float)state->planet_count;
		if (!yt_game_write_sector(&state->game, headquarters, &sector,
		    error) || !yt_news_append(
		    "The Xannor home base now has a planet!", error))
			return false;
	}
	if (!rnd(state, &sample, error))
		return false;
	planet.ground_forces = sadd(planet.ground_forces,
	    sint(smul(sample, 25.0f)));
	planet.owner = -1.0f;
	if (planet.bank == 0.0f)
		planet.bank = 16000000.0f;
	return yt_game_write_planet(&state->game, state->planet_count, &planet,
	    error);
}

static bool
top_player(struct maint_state *state, int *record, float *score,
    struct yt_error *error)
{
	int candidate;

	*record = 0;
	*score = 0.0f;
	for (candidate = 2; candidate <= state->player_count + 1;
	    ++candidate) {
		struct yt_player player;

		if (!yt_game_read_player(&state->game, candidate, &player, error))
			return false;
		if (player.name_length != 0.0f && player.score > *score) {
			*record = candidate;
			*score = player.score;
		}
	}
	return true;
}

static bool
xannor_hunt(struct maint_state *state, int top_record, float top_score,
    int *top_target, struct yt_error *error)
{
	struct yt_player player;
	float gate;
	float selection;

	*top_target = 0;
	if (top_record == 0)
		return true;
	if (!yt_game_read_player(&state->game, top_record, &player, error)
	    || !rnd(state, &gate, error))
		return false;
	if (top_score < 2500000.0f
	    || ssub(state->player_cloak[top_record],
	    0.33000001311302185f) > gate)
		return true;
	*top_target = (int)player.sector;
	/* This is a screen report in the original, not a newspaper event. */
	if (!rnd(state, &selection, error))
		return false;
	if (selection > 0.25f)
		*top_target = (int)state->player_sector[top_record];
	return true;
}

static bool
relocate_headquarters(struct maint_state *state, float location[21],
    struct yt_error *error)
{
	int candidate;
	struct yt_sector sector;
	int old_planet_sector;

	for (;;) {
		if (!random_integer(state, state->sector_count - 7, &candidate,
		    error))
			return false;
		candidate += 7;
		if (!yt_game_read_sector(&state->game, candidate, &sector, error))
			return false;
		if (!((sector.fighters > 1.0f
		    && sector.fighter_owner != -1.0f)
		    || sector.planet > 1.0f))
			break;
	}
	if (!find_planet_sector(state, state->planet_count, &old_planet_sector,
	    error))
		return false;
	if (old_planet_sector > 0) {
		struct yt_sector old;

		if (!yt_game_read_sector(&state->game, old_planet_sector, &old,
		    error))
			return false;
		old.planet = 0.0f;
		if (!yt_game_write_sector(&state->game, old_planet_sector, &old,
		    error))
			return false;
	}
	sector.planet = (float)state->planet_count;
	state->game.config.headquarters = (float)candidate;
	location[1] = (float)candidate;
	return yt_game_write_sector(&state->game, candidate, &sector, error)
	    && store_config_field(state, YT_F117,
	    state->game.config.headquarters, error)
	    && yt_news_append(
	    " *** The Xannor have MOVED their Headquarters! ***\a", error);
}

static bool
xannor_reclaim_and_relocate(struct maint_state *state, float location[21],
    float size[21], float regeneration, struct yt_error *error)
{
	int hq = (int)state->game.config.headquarters;
	struct yt_sector host;
	bool hostile;

	if (!yt_game_read_sector(&state->game, hq, &host, error))
		return false;
	hostile = host.fighters > 0.0f && host.fighter_owner != -1.0f;
	if (hostile && size[1] > 0.0f) {
		if (host.fighter_owner == -2.0f
		    && !yt_news_append(
		    " *** The Xannor are attempting to reclaim their base from The Mercenaries!",
		    error))
			return false;
		while (host.fighters > 0.0f && size[1] > 0.0f) {
			float sample;
			float quantum = host.fighters > 250.0f
			    && size[1] > 250.0f ? 250.0f : 1.0f;

			if (!rnd(state, &sample, error))
				return false;
			if (sample <= 0.5f)
				host.fighters = ssub(host.fighters, quantum);
			else
				size[1] = ssub(size[1], quantum);
		}
		if (host.fighters <= 0.0f) {
			host.fighters = 0.0f;
			host.fighter_owner = 0.0f;
		}
		if (size[1] <= 0.0f) {
			size[1] = 0.0f;
			location[1] = 0.0f;
		}
		if (!yt_game_write_sector(&state->game, hq, &host, error))
			return false;
		if (host.fighter_owner == -2.0f
		    && !yt_news_append(size[1] > 0.0f
		    ? " *** Successful!" : " *** Failed!", error))
			return false;
	}
	if ((!hostile && size[1] == regeneration)
	    || (hostile && size[1] > 0.0f))
		return relocate_headquarters(state, location, error);
	return true;
}

static bool
consume_revenge_slot(struct maint_state *state, int *live_sector,
    int *cached_target, struct yt_error *error)
{
	struct yt_sector metadata;
	float value;
	int record;

	*live_sector = 0;
	*cached_target = 0;
	if (state->sector_count < 21)
		return true;
	if (!yt_game_read_sector(&state->game, 21, &metadata, error))
		return false;
	value = yt_record_get_number(&metadata.record, YT_F105);
	record = (int)value;
	if (record >= 2 && record <= state->player_count + 1) {
		struct yt_player player;

		if (!yt_game_read_player(&state->game, record, &player, error))
			return false;
		if (player.sector > 7.0f) {
			*live_sector = (int)player.sector;
			*cached_target = (int)state->player_sector[record];
			if (!yt_news_append(" *** Xannor REVENGE! ***\a", error))
				return false;
		}
	}
	yt_record_set_number(&metadata.record, YT_F105, 0.0f);
	return yt_database_write(&state->game.database,
	    (size_t)yt_sector_basic_record(&state->game.config, 21),
	    &metadata.record, error);
}

static bool
xannor_candidate_target(struct maint_state *state, int current,
    int revenge_live, int revenge_cached, int *target,
    struct yt_error *error)
{
	int initial;
	int discovery = revenge_cached;
	int attempt;
	int attempt_limit = revenge_live != 0 ? 25 : 1;

	do {
		if (!random_integer(state, state->sector_count, &initial, error))
			return false;
	} while (initial == current);
	for (attempt = 0; attempt < attempt_limit; ++attempt) {
		struct yt_sector sector;
		int candidate;
		int player;

		if (!random_integer(state, state->sector_count, &candidate,
		    error) || !yt_game_read_sector(&state->game, candidate,
		    &sector, error))
			return false;
		if ((sector.fighters > 1.0f
		    && sector.fighter_owner != -1.0f)
		    || sector.planet > 1.0f)
			discovery = candidate;
		if (discovery != 0)
			break;
		for (player = 2; player <= state->player_count + 1; ++player) {
			float cloak_draw;
			bool same_sector =
			    state->player_sector[player] == (float)candidate;

			if (!rnd(state, &cloak_draw, error))
				return false;
			if (same_sector
			    && (cloak_draw > state->player_cloak[player]
			    || revenge_live != 0)) {
				discovery = (int)state->player_sector[player];
				break;
			}
		}
		if (discovery != 0)
			break;
	}
	*target = discovery > 7 ? discovery : initial;
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
xannor_mines_and_defense(struct maint_state *state, int group,
    int sector_number, float size[21], struct yt_sector *sector,
    struct yt_error *error)
{
	float original = size[group];

	while (sector->mines > 0.0f && size[group] > 0.0f) {
		int damage;

		if (!random_integer(state, 1000, &damage, error))
			return false;
		if ((float)damage > size[group])
			damage = (int)size[group];
		size[group] = ssub(size[group], (float)damage);
		sector->mines = ssub(sector->mines, 1.0f);
	}
	if (original != size[group]) {
		if (!news_number_line(" ***", original,
		    " Xannor hit sector mines in sector", sector_number, "!",
		    error))
			return false;
		if (size[group] <= 0.0f) {
			if (!yt_news_append(" *** The Xannor were killed!", error))
				return false;
		}
		else if (!news_one_number_line(" *** Lost a total of",
		    original - size[group], " fighters!", error))
			return false;
	}
	if (size[group] <= 0.0f) {
		size[group] = 0.0f;
		return true;
	}
	if (sector->fighter_owner == -1.0f
	    || sector->fighter_owner == 0.0f
	    || sector->fighters < 1.0f)
		return true;
	{
		float original_xannor = size[group];
		float original_defenders = sector->fighters;
		float xloss = 0.0f;
		float dloss = 0.0f;

		while (dloss < original_defenders && xloss < original_xannor) {
			float sample;
			float quantum = xannor_quantum(
			    original_defenders - dloss,
			    original_xannor - xloss);

			if (!rnd(state, &sample, error))
				return false;
			if (sample > 0.5f)
				xloss = sadd(xloss, quantum);
			else
				dloss = sadd(dloss, quantum);
		}
		xloss = fminf(xloss, original_xannor);
		dloss = fminf(dloss, original_defenders);
		size[group] = ssub(original_xannor, xloss);
		sector->fighters = ssub(original_defenders, dloss);
		if (sector->fighters <= 0.0f) {
			sector->fighters = 0.0f;
			sector->fighter_owner = 0.0f;
		}
	}
	return true;
}

static bool
xannor_attack_planet(struct maint_state *state, int group, int sector_number,
    float location[21], float size[21], struct yt_sector *sector,
    struct yt_error *error)
{
	struct yt_planet planet;
	bool destroyed = false;
	int planet_number;
	int index;
	char number[64];
	char line[420];

	if (sector->planet <= 0.0f)
		return true;
	planet_number = (int)sector->planet;
	if (!yt_game_read_planet(&state->game, planet_number, &planet,
	    error))
		return false;
	if (planet.name_length <= 0.0f || planet.owner == -1.0f)
		return true;
	qb_str_double(number, sizeof(number), size[group]);
	snprintf(line, sizeof(line),
	    " ***%s Xannor attacked the planet \"%s\"", number, planet.name);
	if (!yt_news_append(line, error))
		return false;
	while (planet.ground_forces > 0.0f && size[group] > 0.0f) {
		float sample;

		if (!rnd(state, &sample, error))
			return false;
		size[group] = ssub(size[group], 1.0f);
		planet.ground_forces = ssub(planet.ground_forces,
		    floorf(smul(sample, 1000.0f)));
	}
	if (planet.ground_forces < 0.0f)
		planet.ground_forces = 0.0f;
	while (size[group] > 0.0f
	    && (planet.production[0] > 0.0f
	    || planet.production[1] > 0.0f
	    || planet.production[2] > 0.0f)) {
		float gate;
		float quantum = (planet.production[0] > 500.0f
		    || planet.production[1] > 500.0f
		    || planet.production[2] > 500.0f) && location[group] != 0.0f
		    ? 450.0f : 1.0f;

		if (!rnd(state, &gate, error))
			return false;
		if (gate < 0.5f) {
			for (index = 0; index < 3; ++index) {
				float sample;

				if (!rnd(state, &sample, error))
					return false;
				planet.production[index] = ssub(
				    planet.production[index],
				    sdiv(smul(sample, quantum), 3.0f));
				if (planet.production[index] < 0.0f)
					planet.production[index] = 0.0f;
			}
		}
		else
			size[group] = ssub(size[group], quantum);
	}
	for (index = 0; index < 3; ++index) {
		float cap = smul(planet.production[index], 10.0f);

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
	if (size[group] <= 0.0f) {
		size[group] = 0.0f;
		location[group] = 0.0f;
	}
	if (size[group] <= 0.0f) {
		if (!yt_news_append(" *** Xannor fighters destroyed!", error))
			return false;
	}
	else {
		snprintf(line, sizeof(line), " *** Planet \"%s\" destroyed!",
		    planet.name);
		if (!yt_news_append(line, error))
			return false;
	}
	(void)sector_number;
	return yt_game_write_planet(&state->game, planet_number, &planet, error);
}

static bool
xannor_attack_players(struct maint_state *state, int group,
    float location[21], float size[21], struct yt_error *error)
{
	int record;

	for (record = 2; record <= state->player_count + 1
	    && size[group] > 0.0f; ++record) {
		struct yt_player player;
		float original_player;
		float original_xannor;
		float player_losses = 0.0f;
		float xannor_losses = 0.0f;

		if (state->player_sector[record] != location[group])
			continue;
		if (!yt_game_read_player(&state->game, record, &player, error))
			return false;
		if (player.name_length == 0.0f || player.killed_by != 0.0f)
			continue;
		original_player = player.fighters;
		original_xannor = size[group];
		while (player_losses < original_player
		    && xannor_losses < original_xannor) {
			float sample;
			float quantum = xannor_quantum(
			    original_player - player_losses,
			    original_xannor - xannor_losses);

			if (!rnd(state, &sample, error))
				return false;
			if (sample > 0.5f)
				xannor_losses = sadd(xannor_losses, quantum);
			else
				player_losses = sadd(player_losses, quantum);
		}
		player_losses = fminf(player_losses, original_player);
		xannor_losses = fminf(xannor_losses, original_xannor);
		player.fighters = ssub(original_player, player_losses);
		while (player.fighters < 1.0f
		    && xannor_losses < original_xannor
		    && player.shields > 0.0f) {
			float sample;
			float quantum = xannor_quantum(
			    original_xannor - xannor_losses, player.shields);

			if (!rnd(state, &sample, error))
				return false;
			if (sample >= 0.5f)
				player.shields = ssub(player.shields, quantum);
			else
				xannor_losses = sadd(xannor_losses, quantum);
		}
		if (player.fighters < 0.0f)
			player.fighters = 0.0f;
		if (player.shields < 0.0f)
			player.shields = 0.0f;
		size[group] = ssub(original_xannor,
		    fminf(xannor_losses, original_xannor));
		if (!yt_game_write_player(&state->game, record, &player, error))
			return false;
		if (player.shields < 1.0f
		    && !immediate_death_cleanup(state, record, -1.0f, &player,
		    error))
			return false;
		if (size[group] <= 0.0f) {
			size[group] = 0.0f;
			location[group] = 0.0f;
		}
	}
	return true;
}

static bool
persist_xannor_groups(struct maint_state *state, float location[21],
    float size[21], struct yt_error *error)
{
	int group;

	for (group = 1; group <= 20; ++group) {
		struct yt_sector metadata;

		if (!yt_game_read_sector(&state->game, group, &metadata, error))
			return false;
		yt_record_set_number(&metadata.record, YT_F105, location[group]);
		if (!yt_database_write(&state->game.database,
		    (size_t)yt_sector_basic_record(&state->game.config, group),
		    &metadata.record, error))
			return false;
		if (location[group] > 0.0f && size[group] > 0.0f) {
			struct yt_sector host;
			int logical = (int)location[group];

			if (!yt_game_read_sector(&state->game, logical, &host,
			    error))
				return false;
			host.fighters = sadd(host.fighters, size[group]);
			host.fighter_owner = -1.0f;
			if (!yt_game_write_sector(&state->game, logical, &host,
			    error))
				return false;
		}
	}
	return true;
}

static bool
maintain_xannor(struct maint_state *state, struct yt_error *error)
{
	float location[21];
	float size[21];
	float total = 0.0f;
	float regeneration;
	float ceiling;
	float score;
	int top_record;
	int top_target;
	int revenge_live;
	int revenge_cached;
	int group;

	if (!load_xannor_groups(state, location, size, error)
	    || !maintain_xannoron(state, error)
	    || !yt_news_append("  -  Xannor report:", error)
	    || !top_player(state, &top_record, &score, error))
		return false;
	for (group = 1; group <= 20; ++group)
		total = sadd(total, size[group]);
	regeneration = floorf(score / 500.0f);
	ceiling = floorf(score / 100.0f);
	if (total > ceiling)
		regeneration = 0.0f;
	size[1] = sadd(size[1], regeneration);
	location[1] = state->game.config.headquarters;
	{
		char value[64];
		char line[180];

		qb_str_double(value, sizeof(value), regeneration);
		snprintf(line, sizeof(line),
		    "Calculated Dynamic Xannor Regeneration is%s fighters.",
		    value);
		if (!yt_news_append(line, error))
			return false;
	}
	if (!xannor_hunt(state, top_record, score, &top_target, error)
	    || !xannor_reclaim_and_relocate(state, location, size,
	    regeneration, error)
	    || !consume_revenge_slot(state, &revenge_live, &revenge_cached,
	    error))
		return false;

	for (group = 2; group <= 20; ++group) {
		int target;
		int next;
		struct yt_sector destination;

		if (size[group] <= 0.0f
		    && size[1] >= score / 2000.0f) {
			int split;

			if (!nested_integer(state, 4, (int)size[1], &split,
			    error))
				return false;
			size[group] = (float)split;
			size[1] = ssub(size[1], size[group]);
			location[group] = state->game.config.headquarters;
		}
		if (size[group] <= 0.0f || location[group] <= 0.0f)
			continue;
		if (!xannor_candidate_target(state, (int)location[group],
		    revenge_live, revenge_cached, &target, error))
			return false;
		if (((revenge_live != 0 && group > 15
		    && top_target != 0) || group == 20))
			target = top_target;
		if (size[1] < score / 2000.0f)
			target = (int)state->game.config.headquarters;
		if (!route_next_hop(state, (int)location[group], target, &next,
		    error) || !yt_game_read_sector(&state->game, next,
		    &destination, error))
			return false;
		location[group] = (float)next;
		if (!xannor_mines_and_defense(state, group, next, size,
		    &destination, error))
			return false;
		if (size[group] <= 0.0f) {
			location[group] = 0.0f;
		}
		else if (!xannor_attack_planet(state, group, next, location,
		    size, &destination, error)
		    || !xannor_attack_players(state, group, location, size,
		    error))
			return false;
		if (!yt_game_write_sector(&state->game, next, &destination,
		    error))
			return false;
	}
	return persist_xannor_groups(state, location, size, error);
}

static bool
maintain_mercenary_base(struct maint_state *state, struct yt_error *error)
{
	int sector_number;
	struct yt_planet planet;

	if (!find_planet_sector(state, state->planet_count - 1, &sector_number,
	    error) || !yt_game_read_planet(&state->game,
	    state->planet_count - 1, &planet, error))
		return false;
	if (sector_number == 0) {
		struct yt_sector sector;

		yt_record_set_text(&planet.record,
		    (const uint8_t *)"Mercenary Base", 14);
		strcpy(planet.name, "Mercenary Base");
		planet.name_length = 14.0f;
		planet.last_day = (float)(state->today - 10);
		planet.production[0] = 100000.0f;
		planet.production[1] = 100000.0f;
		planet.production[2] = 100000.0f;
		planet.stock[0] = planet.stock[1] = planet.stock[2] = 0.0f;
		planet.missiles = 0.0f;
		planet.owner = -2.0f;
		planet.ground_forces = 150000.0f;
		planet.bank = 25000000.0f;
		planet.mines = 0.0f;
		if (!choose_free_planet_sector(state, &sector_number, error)
		    || !yt_game_read_sector(&state->game, sector_number,
		    &sector, error))
			return false;
		sector.planet = (float)(state->planet_count - 1);
		if (!yt_game_write_planet(&state->game,
		    state->planet_count - 1, &planet, error)
		    || !yt_game_write_sector(&state->game, sector_number,
		    &sector, error)
		    || !yt_news_append(
		    "  -  The Mercenaries have built a home base using a captured Genesis Device!",
		    error))
			return false;
	}
	planet.owner = -2.0f;
	if (planet.ground_forces < 1.0f)
		planet.ground_forces = 150000.0f;
	if (planet.bank == 0.0f)
		planet.bank = 25000000.0f;
	return yt_game_write_planet(&state->game, state->planet_count - 1,
	    &planet, error);
}

static bool
collect_tax_and_reinforce(struct maint_state *state,
    struct yt_error *error)
{
	double tax_pool = 0.0;
	int port_number;
	int fleet;
	int strength;

	for (port_number = 1; port_number <= state->port_count; ++port_number) {
		struct yt_port port;

		if (!yt_game_read_port(&state->game, port_number, &port, error))
			return false;
		if (port.treasury != 0.0f) {
			float tax = floorf(port.treasury / 10.0f);

			tax_pool += (double)tax;
			port.treasury = ssub(port.treasury,
			    floorf(port.treasury / 10.0f));
			if (!yt_game_write_port(&state->game, port_number, &port,
			    error))
				return false;
		}
	}
	if (tax_pool > 0.0
	    && !news_one_number_line("  -  The goverment has collected",
	    tax_pool, " credits tax from the ports.", error))
		return false;
	strength = (int)floor(tax_pool / 10.0);
	if (strength <= 0)
		return true;
	for (fleet = 0; fleet < 10; ++fleet) {
		int sector_number;
		struct yt_sector sector;

		do {
			if (!random_integer(state, state->sector_count - 1,
			    &sector_number, error))
				return false;
			++sector_number;
			if (!yt_game_read_sector(&state->game, sector_number,
			    &sector, error))
				return false;
		} while (sector.fighters > 0.0f);
		sector.fighters = (float)strength;
		sector.fighter_owner = -2.0f;
		if (!yt_game_write_sector(&state->game, sector_number, &sector,
		    error))
			return false;
	}
	return news_one_number_line("  -  The government has hired",
	    (double)strength * 10.0,
	    " mercenaries to help Fight the Xannor!", error);
}

static bool
mercenary_defections(struct maint_state *state, struct yt_error *error)
{
	int logical;

	for (logical = 1; logical <= state->sector_count; ++logical) {
		struct yt_sector sector;

		if (!yt_game_read_sector(&state->game, logical, &sector, error))
			return false;
		if (sector.fighters > 0.0f && sector.fighter_owner > 1.0f) {
			float sample;

			if (!rnd(state, &sample, error))
				return false;
			if (smul(sample, 100.0f) > sector.fighters) {
				char amount[64];
				char sector_text[64];
				char owner_name[42] = "";
				char line[360];
				struct yt_player owner;

				if (!yt_game_read_player(&state->game,
				    (int)sector.fighter_owner, &owner, error))
					return false;
				snprintf(owner_name, sizeof(owner_name), "%s",
				    owner.name);
				qb_str_double(amount, sizeof(amount),
				    sector.fighters);
				qb_str_double(sector_text, sizeof(sector_text),
				    logical);
				snprintf(line, sizeof(line),
				    "  - %s fighters in sector%s belonging to %s joined the mercs!",
				    amount, sector_text, owner_name);
				sector.fighter_owner = -2.0f;
				if (!yt_game_write_sector(&state->game, logical,
				    &sector, error)
				    || !yt_news_append(line, error))
					return false;
			}
		}
	}
	return true;
}

static bool
mercenary_arrival(struct maint_state *state, int sector_number,
    float moving, struct yt_error *error)
{
	struct yt_sector sector;

	if (!yt_game_read_sector(&state->game, sector_number, &sector, error))
		return false;
	while (sector.mines > 0.0f && moving > 0.0f) {
		int damage;

		if (!nested_integer(state, 2, 10000, &damage, error))
			return false;
		if ((float)damage > moving)
			damage = (int)moving;
		moving = ssub(moving, (float)damage);
		sector.mines = ssub(sector.mines, 1.0f);
	}
	if (moving <= 0.0f)
		return yt_game_write_sector(&state->game, sector_number, &sector,
		    error);
	if ((sector.fighter_owner == -2.0f
	    || sector.fighter_owner == 0.0f) && sector.planet != 0.0f) {
		struct yt_planet planet;

		if (!yt_game_read_planet(&state->game, (int)sector.planet,
		    &planet, error))
			return false;
		sector.fighters = sadd(sadd(sector.fighters, moving),
		    planet.fighters);
		sector.fighter_owner = -2.0f;
		planet.fighters = 0.0f;
		return yt_game_write_planet(&state->game, (int)sector.planet,
		    &planet, error)
		    && yt_game_write_sector(&state->game, sector_number,
		    &sector, error);
	}
	if (sector.fighter_owner == -2.0f) {
		sector.fighters = sadd(sector.fighters, moving);
		return yt_game_write_sector(&state->game, sector_number, &sector,
		    error);
	}
	if (sector.fighters > 0.0f) {
		bool attack = sector.fighter_owner == -1.0f;
		float original_owner = sector.fighter_owner;

		if (sector.fighter_owner > 0.0f) {
			float sample;

			if (!rnd(state, &sample, error))
				return false;
			attack = sample > 0.949999988079071f;
		}
		if (!attack) {
			sector.fighters = sadd(sector.fighters, moving);
			if (original_owner > 0.0f) {
				struct yt_player owner;
				char message[240];

				if (!yt_game_read_player(&state->game,
				    (int)original_owner, &owner, error))
					return false;
				snprintf(message, sizeof(message),
				    "Mercenaries joined your defense force in sector %d!",
				    sector_number);
				if (!yt_radio_append_maintenance(message, -2.0f,
				    original_owner, error))
					return false;
			}
			return yt_game_write_sector(&state->game, sector_number,
			    &sector, error);
		}
		while (sector.fighters > 0.0f && moving > 0.0f) {
			float sample;
			float quantum = sector.fighters > 200.0f
			    && moving > 200.0f ? 150.0f : 1.0f;

			if (!rnd(state, &sample, error))
				return false;
			if (sample < 0.5f)
				sector.fighters = ssub(sector.fighters, quantum);
			else
				moving = ssub(moving, quantum);
		}
		if (sector.fighters <= 0.0f) {
			sector.fighters = 0.0f;
			sector.fighter_owner = 0.0f;
		}
	}
	if (moving > 0.0f) {
		sector.fighters = sadd(sector.fighters, moving);
		sector.fighter_owner = -2.0f;
	}
	return yt_game_write_sector(&state->game, sector_number, &sector,
	    error);
}

static bool
move_mercenaries(struct maint_state *state, struct yt_error *error)
{
	int origin;

	for (origin = state->sector_count; origin >= 1; --origin) {
		struct yt_sector sector;
		int target;
		int next;
		float moving;

		if (!yt_game_read_sector(&state->game, origin, &sector, error))
			return false;
		if (sector.fighter_owner != -2.0f || sector.fighters <= 0.0f)
			continue;
		if (sector.planet != 0.0f) {
			float hold;

			if (!rnd(state, &hold, error))
				return false;
			if (hold < 0.6600000262260437f)
				continue;
		}
		moving = sector.fighters;
		sector.fighters = 0.0f;
		sector.fighter_owner = 0.0f;
		if (!yt_game_write_sector(&state->game, origin, &sector, error))
			return false;
		do {
			if (!random_integer(state, state->sector_count, &target,
			    error))
				return false;
		} while (target == origin);
		if (!route_next_hop(state, origin, target, &next, error)
		    || !mercenary_arrival(state, next, moving, error))
			return false;
	}
	return true;
}

static bool
maintain_factions(struct maint_state *state, struct yt_error *error)
{
	if (!maintain_xannor(state, error)
	    || !collect_tax_and_reinforce(state, error)
	    || !yt_news_append("  -  Mercenary Report:", error)
	    || !maintain_mercenary_base(state, error)
	    || !mercenary_defections(state, error)
	    || !move_mercenaries(state, error))
		return false;
	return true;
}

static bool
super_lottery(struct maint_state *state, struct yt_error *error)
{
	float gate;
	int player_slot;
	int player_record;
	int planet_number;
	int sector_number;
	struct yt_player player;
	struct yt_planet planet;
	struct yt_sector sector;
	char full_name[160];
	char stored_name[42];
	size_t full_length;
	int index;

	if (!rnd(state, &gate, error))
		return false;
	if (gate < 0.5f)
		return true;
	if (!random_integer(state, state->player_count, &player_slot, error))
		return false;
	player_record = player_slot + 1;
	if (!yt_game_read_player(&state->game, player_record, &player, error))
		return false;
	if (player.name_length == 0.0f)
		return true;
	if (!random_integer(state, state->planet_count, &planet_number, error)
	    || !yt_game_read_planet(&state->game, planet_number, &planet,
	    error))
		return false;
	if (planet.name_length != 0.0f)
		return true;
	if (!random_integer(state, state->sector_count, &sector_number, error)
	    || !yt_game_read_sector(&state->game, sector_number, &sector,
	    error))
		return false;
	if (sector.planet > 0.0f)
		return true;
	snprintf(full_name, sizeof(full_name), "%.*s's Planet",
	    (int)player.name_length, player.name);
	full_length = strlen(full_name);
	snprintf(stored_name, sizeof(stored_name), "%.*s", 41, full_name);
	strcpy(planet.name, stored_name);
	planet.name_length = (float)full_length;
	for (index = 0; index < 3; ++index) {
		float first;
		float second;

		if (!rnd(state, &first, error) || !rnd(state, &second, error))
			return false;
		planet.production[index] = smul(smul(first, second), 3000.0f);
		planet.stock[index] = 0.0f;
	}
	planet.missiles = 0.0f;
	planet.owner = (float)player_record;
	if (!rnd(state, &gate, error))
		return false;
	planet.ground_forces = sint(sadd(smul(gate, 100.0f), 1.0f));
	if (!rnd(state, &gate, error))
		return false;
	planet.bank = smul(gate, 16000000.0f);
	planet.plasma = 0.0f;
	sector.planet = (float)planet_number;
	if (!yt_game_write_planet(&state->game, planet_number, &planet, error)
	    || !yt_game_write_sector(&state->game, sector_number, &sector,
	    error))
		return false;
	{
		char line[260];

		snprintf(line, sizeof(line),
		    " *** %s won a PLANET in the SUPER LOTTERY!!!!!\a",
		    player.name);
		if (!yt_news_append(line, error)
		    || !yt_radio_append_maintenance(
		    "You won a PLANET in the SUPER LOTTERY!!!!!", -2.0f,
		    (float)player_record, error))
			return false;
	}
	return true;
}
