#include "yt_init.h"

#include "qb.h"
#include "yt_text.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define YT_INIT_PLAYERS 50
#define YT_INIT_SECTORS 2004
#define YT_INIT_PORTS 1000
#define YT_INIT_PLANETS 100
#define YT_NAME_TOKENS 908

static const uint8_t raw_zero_residue[4] = {0x00, 0x00, 0xa0, 0x00};

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Woverlength-strings"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverlength-strings"
#endif
static const char port_name_blob[] =
#include "yt_portnames.inc"
;
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

struct world {
	int sectors;
	int ports;
	int (*warps)[6];
	int *port_sectors;
	int *sector_ports;
};

static void
set_error(struct yt_error *error, enum yt_status status,
    const char *operation, const char *path)
{
	if (error == NULL)
		return;
	error->status = status;
	error->system_error = 0;
	snprintf(error->operation, sizeof(error->operation), "%s", operation);
	snprintf(error->path, sizeof(error->path), "%s",
	    path != NULL ? path : "");
}

static float
single_add(float left, float right)
{
	volatile float result = left + right;
	return result;
}

static float
single_mul(float left, float right)
{
	volatile float result = left * right;
	return result;
}

static float
float_bits(uint32_t bits)
{
	float value;

	memcpy(&value, &bits, sizeof(value));
	return value;
}

static bool
draw(struct yt_random *random, float *value, struct yt_error *error)
{
	return yt_random_next(random, value, error);
}

static bool
bounded(struct yt_random *random, int bound, int *value,
    struct yt_error *error)
{
	float sample;

	if (bound <= 0) {
		set_error(error, YT_RANGE, "bounded random", "");
		return false;
	}
	if (!draw(random, &sample, error))
		return false;
	*value = (int)floorf(single_mul(sample, (float)bound)) + 1;
	return true;
}

static bool
pool_pointers(const char *pointers[YT_NAME_TOKENS])
{
	const char *cursor = port_name_blob;
	size_t index;

	for (index = 0; index < YT_NAME_TOKENS; ++index) {
		if (*cursor == '\0')
			return false;
		pointers[index] = cursor;
		cursor += strlen(cursor) + 1U;
	}
	return *cursor == '\0';
}

bool
yt_generate_port_name(struct yt_random *random, char name[42],
    struct yt_error *error)
{
	const char *pool[YT_NAME_TOKENS];
	float first;
	float second;
	int parts;
	int part;
	size_t used = 0;

	if (!pool_pointers(pool)) {
		set_error(error, YT_INVALID, "compiled port-name pool", "");
		return false;
	}
	if (!draw(random, &first, error) || !draw(random, &second, error))
		return false;
	parts = (int)floorf(single_mul(single_mul(first, second), 3.0f)) + 2;
	name[0] = '\0';
	for (part = 0; part < parts; ++part) {
		float sample;
		int selected;
		const char *token;
		size_t length;
		bool leading;

		if (!draw(random, &sample, error))
			return false;
		selected = (int)floorf(single_mul(sample,
		    (float)YT_NAME_TOKENS));
		token = pool[selected];
		length = strlen(token);
		leading = length > 4;
		if (leading && used > 0 && used < 41)
			name[used++] = ' ';
		if (length > 0 && used < 41) {
			name[used++] = leading
			    ? (char)((uint8_t)token[0] & 0xdfU) : token[0];
			++token;
			--length;
		}
		while (length-- > 0 && used < 41)
			name[used++] = *token++;
	}
	name[used] = '\0';
	if (used > 0)
		name[0] = (char)((uint8_t)name[0] & 0xdfU);
	return true;
}

static bool
pair_already_linked(const struct world *world, int source, int destination)
{
	int slot;

	for (slot = 0; slot < 6; ++slot) {
		if (world->warps[source][slot] == destination
		    || world->warps[destination][slot] == source)
			return true;
	}
	return false;
}

static bool
sector_nonempty(const struct world *world, int sector)
{
	int slot;

	for (slot = 0; slot < 6; ++slot) {
		if (world->warps[sector][slot] != 0)
			return true;
	}
	return false;
}

static bool
randomize_sector(struct world *world, int sector, struct yt_random *random,
    struct yt_error *error)
{
	const float local_threshold = float_bits(UINT32_C(0x3ecccccd));
	const float long_threshold = float_bits(UINT32_C(0x3f7c28f6));

	do {
		int slot;

		for (slot = 0; slot < 6; ++slot) {
			float probability;

			if (!draw(random, &probability, error))
				return false;
			if (probability <= local_threshold && slot < 5) {
				int distance;
				int destination;

				if (!bounded(random, 10, &distance, error))
					return false;
				if (sector > world->sectors - 10)
					distance = -distance;
				destination = sector + distance;
				if (destination < 1 || destination > world->sectors) {
					set_error(error, YT_RANGE,
					    "initializer local warp", "");
					return false;
				}
				if (world->warps[sector][slot] == 0
				    && world->warps[destination][slot] == 0
				    && !pair_already_linked(world, sector,
				    destination)) {
					world->warps[sector][slot] = destination;
					world->warps[destination][slot] = sector;
				}
			}

			if (slot == 5 && world->warps[sector][slot] == 0) {
				for (;;) {
					int destination;

					if (!draw(random, &probability, error))
						return false;
					if (probability < long_threshold)
						break;
					if (!bounded(random, world->sectors,
					    &destination, error))
						return false;
					if (destination == sector)
						continue;
					if (world->warps[destination][slot] > 0)
						break;
					world->warps[sector][slot] = destination;
					world->warps[destination][slot] = sector;
					break;
				}
			}
		}
	} while (sector != 1 && !sector_nonempty(world, sector));
	return true;
}

static bool
reachable(const struct world *world, int target, bool *result,
    struct yt_error *error)
{
	uint8_t *seen;
	int *queue;
	size_t head = 0;
	size_t tail = 0;

	seen = calloc((size_t)world->sectors + 1U, 1);
	queue = malloc(((size_t)world->sectors + 1U) * sizeof(*queue));
	if (seen == NULL || queue == NULL) {
		free(seen);
		free(queue);
		set_error(error, YT_NO_MEMORY, "initializer BFS", "");
		return false;
	}
	seen[1] = 1;
	queue[tail++] = 1;
	while (head < tail) {
		int current = queue[head++];
		int slot;

		for (slot = 0; slot < 6; ++slot) {
			int neighbor = world->warps[current][slot];

			if (!seen[neighbor]) {
				seen[neighbor] = 1;
				queue[tail++] = neighbor;
			}
		}
	}
	*result = seen[target] != 0;
	free(seen);
	free(queue);
	return true;
}

static bool
build_graph(struct world *world, enum yt_initializer_family family,
    struct yt_random *random, struct yt_error *error)
{
	int sector;
	int slot;
	float position;

	for (slot = 0; slot < 6; ++slot) {
		int destination = family == YT_INITIALIZER_YT ? slot + 1 : slot + 2;

		if (destination > world->sectors) {
			set_error(error, YT_RANGE, "initializer fixed warps", "");
			return false;
		}
		world->warps[1][slot] = destination;
		world->warps[destination][slot] = 1;
	}
	for (sector = 1; sector <= world->sectors; ++sector) {
		if (!randomize_sector(world, sector, random, error))
			return false;
	}
	for (sector = 2; sector <= world->sectors; ++sector) {
		bool found;

		if (!reachable(world, sector, &found, error))
			return false;
		if (!found) {
			int candidate;

			do {
				if (!bounded(random, sector - 1, &candidate, error))
					return false;
			} while (world->warps[candidate][5] != 0);
			/* The stale reciprocal at the target is deliberately kept. */
			world->warps[sector][5] = candidate;
			world->warps[candidate][5] = sector;
		}
	}
	if (!draw(random, &position, error))
		return false;
	position = single_add(single_mul(position, 400.0f), 8.0f);
	while (position < (float)world->sectors) {
		bool overflow;
		int selected = (int)qb_cint(position, &overflow);
		float increment;

		if (overflow || selected < 1 || selected > world->sectors) {
			set_error(error, YT_RANGE, "initializer shortcut", "");
			return false;
		}
		if (world->warps[selected][5] == 0)
			world->warps[selected][5] = 1;
		if (!draw(random, &increment, error))
			return false;
		position = single_add(position, single_mul(increment, 400.0f));
	}
	return true;
}

static bool
assign_ports(struct world *world, struct yt_random *random,
    struct yt_error *error)
{
	uint8_t *occupied;
	int port;

	if (world->ports < 4 || world->sectors < 7) {
		set_error(error, YT_RANGE, "initializer port layout", "");
		return false;
	}
	occupied = calloc((size_t)world->sectors + 1U, 1);
	if (occupied == NULL) {
		set_error(error, YT_NO_MEMORY, "initializer ports", "");
		return false;
	}
	world->port_sectors[1] = 1;
	world->port_sectors[2] = 3;
	world->port_sectors[3] = 5;
	world->port_sectors[4] = 7;
	/* Compiled bug: mark sector indices 1..4, not 1,3,5,7. */
	for (port = 1; port <= 4; ++port)
		occupied[port] = 1;
	for (port = 5; port <= world->ports; ++port) {
		int sector;

		do {
			if (!bounded(random, world->sectors - 1, &sector, error)) {
				free(occupied);
				return false;
			}
			++sector;
		} while (occupied[sector]);
		world->port_sectors[port] = sector;
		occupied[sector] = 1;
	}
	for (port = 1; port <= world->ports; ++port)
		world->sector_ports[world->port_sectors[port]] = port;
	free(occupied);
	return true;
}

static void
make_config_record(struct yt_config *config, float stored_scoreboard_length)
{
	size_t length = strlen(config->scoreboard);

	yt_record_clear(&config->record);
	yt_record_set_text(&config->record,
	    (const uint8_t *)config->scoreboard, length);
	yt_record_set_number(&config->record, YT_F41, stored_scoreboard_length);
	yt_record_set_number(&config->record, YT_F45, config->epoch_year);
	yt_record_set_number(&config->record, YT_F49, config->turns_per_day);
	yt_record_set_number(&config->record, YT_F53, config->sector_offset);
	yt_record_set_number(&config->record, YT_F57, config->port_offset);
	yt_record_set_number(&config->record, YT_F61, config->planet_offset);
	yt_record_set_number(&config->record, YT_F65, config->initial_fighters);
	yt_record_set_number(&config->record, YT_F69, config->initial_credits);
	yt_record_set_number(&config->record, YT_F73, config->initial_holds);
	yt_record_set_number(&config->record, YT_F77, config->retention_days);
	yt_record_set_number(&config->record, YT_F81,
	    config->last_maintenance);
	yt_record_set_number(&config->record, YT_F85, config->local_screen);
	yt_record_set_number(&config->record, YT_F93, config->total_records);
	yt_record_set_number(&config->record, YT_F101, config->lottery_plays);
	yt_record_set_number(&config->record, YT_F105, config->genesis_ports);
	yt_record_set_number(&config->record, YT_F117, config->headquarters);
	yt_record_set_number(&config->record, YT_F121, config->maximum_holds);
	yt_record_set_number(&config->record, YT_F125, config->marker);
	yt_record_set_number(&config->record, YT_F129,
	    config->maximum_planets);
	config->scoreboard_length = stored_scoreboard_length;
}

static bool
write_config_and_players(struct yt_database *database,
    const struct yt_config *config, struct yt_error *error)
{
	struct yt_record record;
	int logical;

	if (!yt_database_write(database, 1, &config->record, error))
		return false;

	yt_record_blank(&record);
	for (logical = 1; logical <= (int)config->sector_offset - 1;
	    ++logical) {
		if (!yt_database_write(database, (size_t)logical + 1U,
		    &record, error))
			return false;
	}
	return yt_database_flush(database, error);
}

static bool
write_world_database(struct yt_database *database,
    const struct yt_initializer_options *options, struct yt_config *config,
    const struct world *world, struct yt_random *random, int today,
    struct yt_error *error)
{
	struct yt_record record;
	int logical;

	for (logical = 1; logical <= world->sectors; ++logical) {
		int slot;

		yt_record_blank(&record);
		for (slot = 0; slot < 6; ++slot)
			yt_record_set_number(&record, YT_F41 + (size_t)slot * 4U,
			    (float)world->warps[logical][slot]);
		yt_record_set_number(&record, YT_F65,
		    (float)world->sector_ports[logical]);
		if (!yt_database_write(database,
		    (size_t)yt_sector_basic_record(config, logical),
		    &record, error))
			return false;
	}

	for (logical = 1; logical <= world->ports; ++logical) {
		char name[42];
		float sample;
		int commodity;
		int index;

		yt_record_clear(&record);
		for (index = 0; index < 3; ++index) {
			if (!draw(random, &sample, error))
				return false;
			yt_record_set_number(&record, YT_F61 + (size_t)index * 4U,
			    (float)((int)floorf(single_mul(sample, 31767.0f))
			    + 1000));
		}
		for (index = 0; index < 3; ++index) {
			if (!draw(random, &sample, error))
				return false;
			yt_record_set_number(&record, YT_F73 + (size_t)index * 4U,
			    (float)(-(int)floorf(single_mul(sample, 100.0f))
			    - 1));
		}
		if (logical == 1)
			strcpy(name, "Earth");
		else if (!yt_generate_port_name(random, name, error))
			return false;
		if (!draw(random, &sample, error))
			return false;
		commodity = (int)floorf(single_mul(sample, 3.0f)) + 1;
		yt_record_set_text(&record, (const uint8_t *)name, strlen(name));
		yt_record_set_number(&record, YT_F41, (float)commodity);
		yt_record_set_number(&record, YT_F45, (float)(today - 10));
		for (index = 0; index < 3; ++index)
			yt_record_set_raw_number(&record,
			    YT_F49 + (size_t)index * 4U, raw_zero_residue);
		yt_record_set_number(&record,
		    YT_F73 + (size_t)(3 - commodity) * 4U,
		    -yt_record_get_number(&record,
		    YT_F73 + (size_t)(3 - commodity) * 4U));
		yt_record_set_number(&record, YT_F85, (float)strlen(name));
		yt_record_set_raw_number(&record, YT_F89, raw_zero_residue);
		yt_record_set_number(&record, YT_F93,
		    (float)world->port_sectors[logical]);
		yt_record_set_raw_number(&record, YT_F97, raw_zero_residue);
		yt_record_set_raw_number(&record, YT_F101, raw_zero_residue);
		yt_record_set_raw_number(&record, YT_F105, raw_zero_residue);
		yt_record_set_raw_number(&record, YT_F109, raw_zero_residue);
		yt_record_set_raw_number(&record, YT_F129, raw_zero_residue);
		if (options->family == YT_INITIALIZER_YT && logical == 1) {
			yt_record_set_number(&record, YT_F101,
			    config->lottery_plays);
			yt_record_set_number(&record, YT_F117,
			    config->headquarters);
			yt_record_set_number(&record, YT_F121,
			    config->maximum_holds);
			yt_record_set_number(&record, YT_F125, config->marker);
		}
		if (!yt_database_write(database,
		    (size_t)yt_port_basic_record(config, logical),
		    &record, error))
			return false;
	}

	record = config->record;
	yt_record_set_number(&record, YT_F85, 0.0f);
	for (logical = 1;
	    logical <= (int)config->total_records - (int)config->planet_offset;
	    ++logical) {
		if (!yt_database_write(database,
		    (size_t)yt_planet_basic_record(config, logical),
		    &record, error))
			return false;
	}

	if (!yt_database_read(database,
	    (size_t)yt_sector_basic_record(config,
	    (int)config->headquarters), &record, error))
		return false;
	yt_record_set_number(&record, YT_F81, 10000.0f);
	yt_record_set_number(&record, YT_F85, -1.0f);
	if (!yt_database_write(database,
	    (size_t)yt_sector_basic_record(config,
	    (int)config->headquarters), &record, error))
		return false;
	if (!yt_database_read(database,
	    (size_t)yt_sector_basic_record(config, 1), &record, error))
		return false;
	yt_record_set_number(&record, YT_F105, config->headquarters);
	if (!yt_database_write(database,
	    (size_t)yt_sector_basic_record(config, 1), &record, error)
	    || !yt_database_flush(database, error))
		return false;
	return true;
}

static bool
append_bytes(uint8_t **data, size_t *length, size_t *capacity,
    const void *addition, size_t added, struct yt_error *error)
{
	size_t required = *length + added;

	if (required > *capacity) {
		size_t grown = *capacity == 0 ? 512U : *capacity;
		uint8_t *replacement;

		while (grown < required)
			grown *= 2U;
		replacement = realloc(*data, grown);
		if (replacement == NULL) {
			set_error(error, YT_NO_MEMORY, "initializer text", "");
			return false;
		}
		*data = replacement;
		*capacity = grown;
	}
	memcpy(*data + *length, addition, added);
	*length = required;
	return true;
}

static bool
write_banner(const char *credited_name, bool rmt, struct yt_error *error)
{
	static const char *const decorations[] = {
		"**********************",
		"**********************",
		"**                  **",
		"** Game Initialized **",
		"**                  **",
		"**********************",
		"**********************"
	};
	uint8_t *data = NULL;
	size_t length = 0;
	size_t capacity = 0;
	size_t index;

	if (rmt) {
		char prophecy[180];
		int written = snprintf(prophecy, sizeof(prophecy),
		    "** The Prophesy has been fulfilled by %s!! **\r\n",
		    credited_name != NULL ? credited_name : "");

		if (written < 0 || (size_t)written >= sizeof(prophecy))
			goto range_failure;
		for (index = 0; index < 3; ++index) {
			if (!append_bytes(&data, &length, &capacity, prophecy,
			    (size_t)written, error))
				goto failure;
		}
	}
	for (index = 0; index < YT_ARRAY_LEN(decorations); ++index) {
		struct yt_clock_value time_now;
		struct yt_clock_value date_now;
		char date[11];
		char time[9];
		char line[100];
		int written;

		if (!yt_platform_clock(&time_now, error)
		    || !yt_platform_clock(&date_now, error))
			goto failure;
		yt_format_time(&time_now, time);
		yt_format_date(&date_now, date);
		written = snprintf(line, sizeof(line), "%s %s %s\r\n",
		    time, date, decorations[index]);
		if (written < 0 || (size_t)written >= sizeof(line))
			goto range_failure;
		if (!append_bytes(&data, &length, &capacity, line,
		    (size_t)written, error))
			goto failure;
	}
	if (!yt_text_write("YTNEWS.DAT", data, length, true, error))
		goto failure;
	free(data);
	return true;

range_failure:
	set_error(error, YT_RANGE, "initializer banner", "YTNEWS.DAT");
failure:
	free(data);
	return false;
}

static bool
write_yt_auxiliary(struct yt_error *error)
{
	static const uint8_t dummy[] = "Dummy,Dummy,Dummy,Dummy\r\n";

	if (!write_banner(NULL, false, error)
	    || !yt_text_write("YTNAME.DAT", dummy, sizeof(dummy) - 1U,
	    true, error)
	    || !yt_text_write("YTRMSG.DAT", NULL, 0, false, error))
		return false;
	return yt_file_delete("YTRMSG.DAT", false, error);
}

static bool
write_rmt_auxiliary(const char *credited_name, struct yt_error *error)
{
	static const uint8_t dummy[] = "Dummy,Dummy,Dummy,Dummy\r\n";
	static const uint8_t yesterday[] =
	    "NO YESTERDAY'S NEWS TO READ!\r\n"
	    "NO YESTERDAY'S NEWS TO READ!\r\n"
	    "NO YESTERDAY'S NEWS TO READ!\r\n"
	    "NO YESTERDAY'S NEWS TO READ!\r\n"
	    "NO YESTERDAY'S NEWS TO READ!\r\n";
	struct yt_radio_record radio;
	FILE *file;
	char prophecy[180];
	int written;
	int index;

	if (!write_banner(credited_name, true, error)
	    || !yt_text_write("YTYNEWS.DAT", yesterday,
	    sizeof(yesterday) - 1U, true, error)
	    || !yt_text_write("YTNAME.DAT", dummy, sizeof(dummy) - 1U,
	    true, error))
		return false;
	written = snprintf(prophecy, sizeof(prophecy),
	    "** The Prophesy has been fulfilled by %s!! **",
	    credited_name != NULL ? credited_name : "");
	if (written < 0 || (size_t)written >= sizeof(prophecy)) {
		set_error(error, YT_RANGE, "RMT prophecy", "YTRMSG.DAT");
		return false;
	}
	memset(&radio, 0, sizeof(radio));
	yt_radio_set_number(&radio, 0, 50.0f);
	yt_radio_set_number(&radio, 4, -2.0f);
	yt_radio_set_number(&radio, 8, -2.0f);
	yt_radio_set_text(&radio, (const uint8_t *)prophecy,
	    (size_t)written, 72);
	file = fopen("YTRMSG.DAT", "wb");
	if (file == NULL) {
		set_error(error, YT_IO_ERROR, "open output", "YTRMSG.DAT");
		return false;
	}
	for (index = 0; index < 5; ++index) {
		if (fwrite(radio.bytes, 1, sizeof(radio.bytes), file)
		    != sizeof(radio.bytes)) {
			fclose(file);
			set_error(error, YT_IO_ERROR, "write output",
			    "YTRMSG.DAT");
			return false;
		}
	}
	if (fclose(file) != 0) {
		set_error(error, YT_IO_ERROR, "close output", "YTRMSG.DAT");
		return false;
	}
	return true;
}

static void
free_world(struct world *world)
{
	free(world->warps);
	free(world->port_sectors);
	free(world->sector_ports);
	memset(world, 0, sizeof(*world));
}

bool
yt_initialize_world(const struct yt_initializer_options *options,
    struct yt_random *random, struct yt_error *error)
{
	struct yt_clock_value current;
	struct yt_config config;
	struct yt_database database;
	struct world world = {0};
	float sample;
	int today;
	bool result = false;

	if (options == NULL || random == NULL) {
		set_error(error, YT_INVALID, "initializer arguments", "");
		return false;
	}
	memset(&database, 0, sizeof(database));
	if (!yt_database_open(&database, "YTDATA.DAT",
	    options->family == YT_INITIALIZER_YT
	    && options->database_already_truncated
	    ? YT_OPEN_UPDATE : YT_OPEN_CREATE, error))
		return false;
	if (!yt_platform_clock(&current, error))
		goto done;
	if (options->use_existing_config) {
		config = options->config;
		if (options->family == YT_INITIALIZER_RMT)
			config.epoch_year = (float)(current.year % 100);
		config.scoreboard_length = (float)strlen(config.scoreboard);
	}
	else {
		const char *scoreboard =
		    options->scoreboard != NULL && options->scoreboard[0] != '\0'
		    ? options->scoreboard : "YTSCORE.ASC";
		size_t scoreboard_length = strlen(scoreboard);

		memset(&config, 0, sizeof(config));
		memcpy(config.scoreboard, scoreboard,
		    scoreboard_length < sizeof(config.scoreboard) - 1U
		    ? scoreboard_length : sizeof(config.scoreboard) - 1U);
		config.scoreboard[scoreboard_length
		    < sizeof(config.scoreboard) - 1U
		    ? scoreboard_length : sizeof(config.scoreboard) - 1U] = '\0';
		config.scoreboard_length = (float)scoreboard_length;
		config.epoch_year = (float)(current.year % 100);
		config.turns_per_day = 500.0f;
		config.sector_offset = YT_INIT_PLAYERS + 1.0f;
		config.port_offset = config.sector_offset + YT_INIT_SECTORS;
		config.planet_offset = config.port_offset + YT_INIT_PORTS;
		config.initial_fighters = 25.0f;
		config.initial_credits = 1005.0f;
		config.initial_holds = 10.0f;
		config.retention_days = 14.0f;
		config.local_screen = -1.0f;
		config.total_records = config.planet_offset + YT_INIT_PLANETS;
		config.lottery_plays = 5.0f;
		config.genesis_ports = 300.0f;
		config.maximum_holds = 1000.0f;
		config.marker = 6324.0f;
		config.maximum_planets = 0.0f;
	}
	world.sectors = (int)(config.port_offset - config.sector_offset);
	world.ports = (int)(config.planet_offset - config.port_offset);
	if (world.sectors < 7 || world.ports < 4) {
		set_error(error, YT_RANGE, "initializer configuration", "");
		goto done;
	}
	today = yt_date_serial(&current, (int)config.epoch_year, NULL);
	config.last_maintenance = (float)(today - 1);
	if (!draw(random, &sample, error))
		goto done;
	config.headquarters = (float)((int)floorf(single_mul(sample,
	    single_add((float)world.sectors, -7.0f))) + 1);
	make_config_record(&config, config.scoreboard_length);
	if (!write_config_and_players(&database, &config, error))
		goto done;

	world.warps = calloc((size_t)world.sectors + 1U,
	    sizeof(*world.warps));
	world.port_sectors = calloc((size_t)world.ports + 1U,
	    sizeof(*world.port_sectors));
	world.sector_ports = calloc((size_t)world.sectors + 1U,
	    sizeof(*world.sector_ports));
	if (world.warps == NULL || world.port_sectors == NULL
	    || world.sector_ports == NULL) {
		set_error(error, YT_NO_MEMORY, "initializer world", "");
		goto done;
	}
	if (!build_graph(&world, options->family, random, error)
	    || !assign_ports(&world, random, error)
	    || !write_world_database(&database, options, &config, &world,
	    random, today, error))
		goto done;
	yt_database_close(&database);
	if (options->family == YT_INITIALIZER_YT)
		result = write_yt_auxiliary(error);
	else
		result = write_rmt_auxiliary(options->credited_name, error);

done:
	yt_database_close(&database);
	free_world(&world);
	return result;
}

bool
yt_initialize_begin_yt(struct yt_error *error)
{
	struct yt_database database;

	if (!yt_database_open(&database, "YTDATA.DAT", YT_OPEN_CREATE, error))
		return false;
	yt_database_close(&database);
	return true;
}

bool
yt_initialize_yt(const char *scoreboard, struct yt_random *random,
    struct yt_error *error)
{
	struct yt_initializer_options options = {
	    .family = YT_INITIALIZER_YT,
	    .scoreboard = scoreboard
	};

	return yt_initialize_world(&options, random, error);
}

bool
yt_initialize_rmt(const struct yt_config *config,
    const char *credited_name, struct yt_random *random,
    struct yt_error *error)
{
	struct yt_initializer_options options = {
	    .family = YT_INITIALIZER_RMT,
	    .config = *config,
	    .use_existing_config = true,
	    .credited_name = credited_name
	};

	return yt_initialize_world(&options, random, error);
}
