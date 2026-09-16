#include "yt_init.h"
#include "yt_init_internal.h"

#include "qb.h"
#include "yt_portname.h"
#include "yt_startup_model.h"
#include "yt_text.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define YT_INIT_PLAYERS 50
#define YT_INIT_SECTORS 2004
#define YT_INIT_PORTS 1000
#define YT_INIT_PLANETS 100
static const uint8_t raw_zero_residue[4] = {0x00, 0x00, 0xa0, 0x00};

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

void
yt_rmt_normalize_config(struct yt_config *config, bool local_mode)
{
	if (config->scoreboard[0] == '\0')
		strcpy(config->scoreboard, "NUL");
	if (config->local_screen < -1.0f || config->local_screen > 0.0f
	    || local_mode)
		config->local_screen = -1.0f;
	if (config->lottery_plays < 1.0f)
		config->lottery_plays = 1.0f;
	if (config->genesis_ports < 20.0f || config->genesis_ports > 300.0f)
		config->genesis_ports = 200.0f;
	if (config->maximum_holds < 5.0f
	    || config->maximum_holds > 1000.0f)
		config->maximum_holds = 50.0f;
	config->marker = 6324.0f;
	config->maximum_planets = 0.0f;
}

bool
yt_rmt_preprocess_old_database(struct yt_database *database,
    struct yt_config *config, struct yt_error *error)
{
	int basic;

	if (config->headquarters == 0.0f) {
		config->headquarters = 85.0f;
		yt_record_set_number(&config->record, YT_F117, 85.0f);
		if (!yt_database_write(database, 1, &config->record, error))
			return false;
	}
	for (basic = 2; basic <= (int)config->sector_offset; ++basic) {
		struct yt_record record;
		float cloak;

		if (!yt_database_read(database, (size_t)basic, &record, error))
			return false;
		cloak = yt_record_get_number(&record, YT_F125);
		if (cloak > 0.0f) {
			record.bytes[YT_F125 + 2U] ^= 0x80U;
			if (!yt_database_write(database, (size_t)basic, &record,
			    error))
				return false;
		}
	}
	return yt_database_flush(database, error);
}

bool
yt_init_sector_prepass(struct yt_database *database, float sector_offset,
    int sector_count, float *port_offset, struct yt_error *error)
{
	struct yt_record record;
	float computed;

	if (database == NULL || port_offset == NULL || sector_count < 0) {
		set_error(error, YT_INVALID, "YT-INIT sector prepass", "");
		return false;
	}
	computed = qb_single_add(sector_offset, (float)sector_count);
	*port_offset = computed;
	if (!yt_database_read(database, 1U, &record, error))
		return false;
	yt_record_set_number(&record, YT_F57, computed);
	return yt_database_write(database, 1U, &record, error);
}

static float
float_bits(uint32_t bits)
{
	float value;

	memcpy(&value, &bits, sizeof(value));
	return value;
}

bool
yt_initializer_confirm_response(const char *response)
{
	return response != NULL
	    && (response[0] == 'Y' || response[0] == 'y')
	    && response[1] == '\0';
}

void
yt_initializer_layout_yt(struct yt_initializer_preparation *preparation)
{
	if (preparation == NULL)
		return;
	memset(preparation, 0, sizeof(*preparation));
	preparation->config.sector_offset = YT_INIT_PLAYERS + 1.0f;
	preparation->config.port_offset = preparation->config.sector_offset
	    + YT_INIT_SECTORS;
	preparation->config.planet_offset = preparation->config.port_offset
	    + YT_INIT_PORTS;
	preparation->config.total_records = preparation->config.planet_offset
	    + YT_INIT_PLANETS;
}

bool
yt_initializer_prepare_yt(struct yt_random *random,
    struct yt_initializer_preparation *preparation, struct yt_error *error)
{
	struct yt_clock_value epoch_date;
	struct yt_clock_value maintenance_date;
	float sample;

	if (random == NULL || preparation == NULL) {
		set_error(error, YT_INVALID, "YT-INIT preparation", "");
		return false;
	}
	yt_initializer_layout_yt(preparation);
	if (!yt_platform_clock(&epoch_date, error)
	    || !yt_platform_clock(&maintenance_date, error)
	    || !yt_random_next(random, &sample, error))
		return false;
	preparation->config.epoch_year = (float)(epoch_date.year % 100);
	preparation->config.turns_per_day = 500.0f;
	preparation->config.initial_fighters = 25.0f;
	preparation->config.initial_credits = 1005.0f;
	preparation->config.initial_holds = 10.0f;
	preparation->config.retention_days = 14.0f;
	preparation->config.local_screen = -1.0f;
	preparation->config.lottery_plays = 5.0f;
	preparation->config.genesis_ports = 300.0f;
	preparation->config.maximum_holds = 1000.0f;
	preparation->config.marker = 6324.0f;
	preparation->config.maximum_planets = 0.0f;
	preparation->today = yt_date_serial(&maintenance_date,
	    preparation->config.epoch_year, NULL);
	preparation->config.last_maintenance = (float)(preparation->today - 1);
	preparation->config.headquarters = (float)((int)floorf(qb_single_multiply(sample,
	    (float)(YT_INIT_SECTORS - 7))) + 1);
	return true;
}

static bool
yt_initializer_bounded(struct yt_random *random, int bound, int *value,
    struct yt_error *error)
{
	float sample;

	if (bound <= 0) {
		set_error(error, YT_RANGE, "bounded random", "");
		return false;
	}
	if (!yt_random_next(random, &sample, error))
		return false;
	*value = (int)floorf(qb_single_multiply(sample, (float)bound)) + 1;
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
    const struct yt_initializer_options *options, struct yt_error *error)
{
	const float local_threshold = float_bits(UINT32_C(0x3ecccccd));
	const float long_threshold = float_bits(UINT32_C(0x3f7c28f6));

	do {
		int slot;

		for (slot = 0; slot < 6; ++slot) {
			float probability;

			if (!yt_random_next(random, &probability, error))
				return false;
			if (probability <= local_threshold && slot < 5) {
				int distance;
				int destination;

				if (!yt_initializer_bounded(random, 10, &distance, error))
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

					if (!yt_random_next(random, &probability, error))
						return false;
					if (probability < long_threshold)
						break;
					if (!yt_initializer_bounded(random, world->sectors,
					    &destination, error))
						return false;
					if (destination == sector)
						continue;
					if (world->warps[destination][slot] > 0)
						break;
					world->warps[sector][slot] = destination;
					world->warps[destination][slot] = sector;
					{
						uint8_t payload[48];
						char source_text[16];
						char destination_text[16];
						int source_length = qb_str_single(source_text,
						    sizeof(source_text), (float)sector);
						int destination_length = qb_str_single(
						    destination_text, sizeof(destination_text),
						    (float)destination);
						size_t length;

						if (source_length < 0 || destination_length < 0)
							return false;
						length = (size_t)source_length + 2U
						    + (size_t)destination_length;
						memcpy(payload, source_text,
						    (size_t)source_length);
						memcpy(payload + source_length, " -", 2U);
						memcpy(payload + source_length + 2U,
						    destination_text,
						    (size_t)destination_length);
						if (!yt_present_number(options, 0x10f8U,
						    (float)sector, YT_INIT_OUTPUT_INLINE,
						    error)
						    || !yt_present_text(options, 0x1102U,
						    YT_INIT_OUTPUT_INLINE, "-", error)
						    || !yt_present_number(options, 0x110aU,
						    (float)destination, YT_INIT_OUTPUT_LINE,
						    error)
						    || !rmt_present(options, 0x10f1U,
						    YT_RMT_OUTPUT_COMMA_SERIAL_FIRST,
						    payload, length, error))
							return false;
					}
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
    struct yt_random *random, const struct yt_initializer_options *options,
    struct yt_error *error)
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
		if (!randomize_sector(world, sector, random, options, error))
			return false;
	}
	if (!yt_present_text(options, 0x1245U, YT_INIT_OUTPUT_LINE, "", error)
	    || !yt_present_text(options, 0x1254U, YT_INIT_OUTPUT_LINE, "",
	    error)
	    || !yt_present_text(options, 0x1268U, YT_INIT_OUTPUT_LINE,
	    "Verifying warps.. linking isolated sectors.", error)
	    || !rmt_present(options, 0x1279U, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    || !rmt_present(options, 0x127cU, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    || !rmt_present_text(options, 0x128aU, YT_RMT_OUTPUT_LINE,
	    "Verifying warps.. linking isolated sectors.", error)
	    || !rmt_present(options, 0x128dU, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error))
		return false;
	for (sector = 2; sector <= world->sectors; ++sector) {
		bool found;

		if (!yt_present(options, 0x12acU,
		    YT_INIT_OUTPUT_LOCATE_COLUMN_ONE, NULL, 0U, error)
		    || !yt_present_text(options, 0x12b9U,
		    YT_INIT_OUTPUT_INLINE, "Verifying warp to sector", error)
		    || !yt_present_number(options, 0x12c1U, (float)sector,
		    YT_INIT_OUTPUT_INLINE, error)
		    || !reachable(world, sector, &found, error))
			return false;
		if (!found) {
			int candidate;
			uint8_t payload[96];
			char target_text[16];
			char candidate_text[16];
			int target_length;
			int candidate_length;
			size_t length;

			do {
				if (!yt_initializer_bounded(random, sector - 1, &candidate,
				    error))
					return false;
			} while (world->warps[candidate][5] != 0);
			target_length = qb_str_single(target_text,
			    sizeof(target_text), (float)sector);
			candidate_length = qb_str_single(candidate_text,
			    sizeof(candidate_text), (float)candidate);
			if (target_length < 0 || candidate_length < 0)
				return false;
			length = sizeof("*** Error - No Path to sector") - 1U
			    + (size_t)target_length + 2U;
			memcpy(payload, "*** Error - No Path to sector",
			    sizeof("*** Error - No Path to sector") - 1U);
			memcpy(payload + sizeof("*** Error - No Path to sector") - 1U,
			    target_text, (size_t)target_length);
			memcpy(payload + length - 2U, "!!", 2U);
			if (!yt_present(options, 0x1501U,
			    YT_INIT_OUTPUT_LOCATE_COLUMN_ONE, NULL, 0U, error)
			    || !yt_present_text(options, 0x150eU,
			    YT_INIT_OUTPUT_INLINE,
			    "*** Error - No Path to sector", error)
			    || !yt_present(options, 0x151bU,
			    YT_INIT_OUTPUT_INLINE, (const uint8_t *)target_text,
			    (size_t)target_length, error)
			    || !yt_present_text(options, 0x1523U,
			    YT_INIT_OUTPUT_LINE, "!!", error)
			    || !rmt_present(options, 0x2f48U, YT_RMT_OUTPUT_LINE,
			    payload, length, error))
				return false;
			length = sizeof("Sector") - 1U + (size_t)target_length
			    + sizeof(" has been linked to sector") - 1U
			    + (size_t)candidate_length;
			memcpy(payload, "Sector", sizeof("Sector") - 1U);
			memcpy(payload + sizeof("Sector") - 1U, target_text,
			    (size_t)target_length);
			memcpy(payload + sizeof("Sector") - 1U
			    + (size_t)target_length, " has been linked to sector",
			    sizeof(" has been linked to sector") - 1U);
			memcpy(payload + length - (size_t)candidate_length,
			    candidate_text, (size_t)candidate_length);
			if (!yt_present_text(options, 0x15c8U,
			    YT_INIT_OUTPUT_INLINE, "Sector", error)
			    || !yt_present_number(options, 0x15cfU,
			    (float)sector, YT_INIT_OUTPUT_INLINE, error)
			    || !yt_present_text(options, 0x15d7U,
			    YT_INIT_OUTPUT_INLINE, "has been linked to sector", error)
			    || !yt_present_number(options, 0x15deU,
			    (float)candidate, YT_INIT_OUTPUT_LINE, error)
			    || !rmt_present(options, 0x302cU, YT_RMT_OUTPUT_LINE,
			    payload, length, error))
				return false;
			/* The stale reciprocal at the target is deliberately kept. */
			world->warps[sector][5] = candidate;
			world->warps[candidate][5] = sector;
		}
	}
	if (!yt_present_text(options, 0x1319U, YT_INIT_OUTPUT_LINE, "", error)
	    || !yt_present_text(options, 0x1328U, YT_INIT_OUTPUT_LINE, "",
	    error)
	    || !yt_present_text(options, 0x133cU, YT_INIT_OUTPUT_LINE,
	    " ** Warp verification complete!! **", error)
	    || !yt_present_text(options, 0x134dU, YT_INIT_OUTPUT_LINE, "",
	    error)
	    || !yt_present_text(options, 0x135fU, YT_INIT_OUTPUT_LINE,
	    " ** Building shortcuts back to sector 1", error)
	    || !rmt_present(options, 0x12f9U, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    || !rmt_present_text(options, 0x1307U, YT_RMT_OUTPUT_LINE,
	    " ** Warp verification complete!! **", error)
	    || !rmt_present(options, 0x130aU, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    || !rmt_present_text(options, 0x1318U, YT_RMT_OUTPUT_LINE,
	    " ** Building shortcuts back to sector 1", error))
		return false;
	if (!yt_random_next(random, &position, error))
		return false;
	position = qb_single_add(qb_single_multiply(position, 400.0f), 8.0f);
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
		if (!yt_random_next(random, &increment, error))
			return false;
		position = qb_single_add(position, qb_single_multiply(increment, 400.0f));
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
			if (!yt_initializer_bounded(random, world->sectors - 1,
			    &sector, error)) {
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
rmt_present_preopen(const struct yt_initializer_options *options,
    struct yt_error *error)
{
	return rmt_present(options, 0x0863U, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    && rmt_present_text(options, 0x0871U, YT_RMT_OUTPUT_LINE,
	    "          Yankee Trader Remote Initialization Program v2.2", error)
	    && rmt_present_text(options, 0x087fU, YT_RMT_OUTPUT_LINE,
	    "                         By Alan Davenport", error)
	    && rmt_present(options, 0x0882U, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    && rmt_present(options, 0x0885U, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    && rmt_present_text(options, 0x089bU, YT_RMT_OUTPUT_LINE,
	    "Re-creating main data file: YTDATA.DAT", error);
}

static bool
rmt_present_before_headquarters(const struct yt_initializer_options *options,
    struct yt_config *config, struct yt_error *error)
{
	struct yt_clock_value maintenance_date;
	int maintenance_serial;

	if (!rmt_present(options, 0x0907U, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    || !rmt_present_text(options, 0x0915U, YT_RMT_OUTPUT_LINE,
	    "Starting player info:", error)
	    || !rmt_present_number_line(options, 0x0930U,
	    "  # of fighters at start:", config->initial_fighters, error)
	    || !rmt_present_number_line(options, 0x096eU,
	    "  # of credits at start:", config->initial_credits, error)
	    || !rmt_present_number_line(options, 0x099cU,
	    "  # of cargo holds at start:", config->initial_holds, error)
	    || !rmt_present_number_line(options, 0x09caU,
	    "  # of days inactivity until an dead player is deleted:",
	    config->retention_days, error))
		return false;
	if (!yt_platform_clock(&maintenance_date, error))
		return false;
	maintenance_serial = yt_date_serial(&maintenance_date,
	    config->epoch_year, NULL);
	config->last_maintenance = (float)(maintenance_serial - 1);
	return rmt_present_text(options, 0x09fdU, YT_RMT_OUTPUT_LINE,
	    "  Last day maintenance run: Yesterday", error)
	    && rmt_present_number_line(options, 0x0a2bU,
	    "  # of turns per day:", config->turns_per_day, error)
	    && rmt_present_number_line(options, 0x0a59U,
	    "  # of times per day a user may play the lottery:",
	    config->lottery_plays, error);
}

static bool
rmt_present_after_headquarters(const struct yt_initializer_options *options,
    const struct yt_config *config, struct yt_error *error)
{
	uint8_t payload[192];
	size_t prefix_length;
	size_t scoreboard_length;

	if (!rmt_present_number_line(options, 0x0ab6U,
	    "  Xannor Headquarters placed in sector:", config->headquarters,
	    error)
	    || !rmt_present_number_line(options, 0x0ad1U,
	    "  Ports needed to initiate Genesis:", config->genesis_ports, error)
	    || !rmt_present_number_line(options, 0x0b0fU,
	    "  Maximum Cargo holds set to:", config->maximum_holds, error))
		return false;
	prefix_length = sizeof("  Scoreboard bulletin name and path is: ") - 1U;
	scoreboard_length = strlen(config->scoreboard);
	if (prefix_length + scoreboard_length > sizeof(payload)) {
		set_error(error, YT_RANGE, "compose RMT scoreboard row", "");
		return false;
	}
	memcpy(payload, "  Scoreboard bulletin name and path is: ",
	    prefix_length);
	memcpy(payload + prefix_length, config->scoreboard, scoreboard_length);
	return rmt_present(options, 0x0b35U, YT_RMT_OUTPUT_LINE, payload,
	    prefix_length + scoreboard_length, error)
	    && rmt_present(options, 0x0b5dU, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error);
}

static bool
rmt_present_graph_opening(const struct yt_initializer_options *options,
    struct yt_error *error)
{
	return rmt_present_text(options, 0x0d75U, YT_RMT_OUTPUT_LINE,
	    "Initializing sectors...", error)
	    && rmt_present(options, 0x0da0U, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    && rmt_present_text(options, 0x0daeU, YT_RMT_OUTPUT_LINE,
	    "Generating Randomized Universe... Please be patient...", error)
	    && rmt_present(options, 0x0e22U, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    && rmt_present_text(options, 0x0e30U, YT_RMT_OUTPUT_LINE,
	    "Wormholes (Long Warps)..", error)
	    && rmt_present(options, 0x0e33U, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error);
}

static bool
yt_present_graph_opening(const struct yt_initializer_options *options,
    struct yt_error *error)
{
	return yt_present_text(options, 0x0defU, YT_INIT_OUTPUT_LINE, "",
	    error)
	    && yt_present_text(options, 0x0e01U, YT_INIT_OUTPUT_LINE,
	    "Generating Randomized Universe... Please be patient...", error)
	    && yt_present_text(options, 0x0e79U, YT_INIT_OUTPUT_LINE, "",
	    error)
	    && yt_present_text(options, 0x0e8dU, YT_INIT_OUTPUT_LINE,
	    "Wormholes (Long Warps)..", error)
	    && yt_present_text(options, 0x0e9eU, YT_INIT_OUTPUT_LINE, "",
	    error);
}

static bool
write_config_and_players(struct yt_database *database,
    const struct yt_config *config,
    const struct yt_initializer_options *options, struct yt_error *error)
{
	struct yt_record record;
	uint8_t payload[96];
	char players_text[32];
	int players_length;
	size_t prefix_length;
	int logical;

	if (options->family == YT_INITIALIZER_YT) {
		players_length = qb_str_single(players_text, sizeof(players_text),
		    config->sector_offset - 1.0f);
		if (players_length < 0
		    || !yt_present_text(options, 0x0abbU, YT_INIT_OUTPUT_LINE,
		    "", error)
		    || !yt_database_write(database, 1, &config->record, error)
		    || !yt_present(options, 0x0b09U, YT_INIT_OUTPUT_LINE,
		    config->record.bytes + YT_F53, 4U, error)
		    || !yt_present_text(options, 0x0b1bU,
		    YT_INIT_OUTPUT_INLINE, "Generating Player Records for", error)
		    || !yt_present_number(options, 0x0b22U,
		    config->sector_offset - 1.0f, YT_INIT_OUTPUT_INLINE, error)
		    || !yt_present_text(options, 0x0b2aU, YT_INIT_OUTPUT_LINE,
		    "Players.", error))
			return false;
	}
	if (options->family == YT_INITIALIZER_RMT) {
		players_length = qb_str_single(players_text, sizeof(players_text),
		    config->sector_offset - 1.0f);
		prefix_length = sizeof("Generating Player Records for") - 1U;
		if (players_length < 0
		    || prefix_length + (size_t)players_length
		    + sizeof(" Players.") - 1U > sizeof(payload)) {
			set_error(error, YT_RANGE, "compose RMT player row", "");
			return false;
		}
		memcpy(payload, "Generating Player Records for", prefix_length);
		memcpy(payload + prefix_length, players_text,
		    (size_t)players_length);
		memcpy(payload + prefix_length + (size_t)players_length,
		    " Players.", sizeof(" Players.") - 1U);
		if (!yt_database_write(database, 1, &config->record, error)
		    || !rmt_present(options, 0x0b7bU, YT_RMT_OUTPUT_BLANK, NULL,
		    0U, error)
		    || !rmt_present(options, 0x0babU, YT_RMT_OUTPUT_LINE,
		    payload, prefix_length + (size_t)players_length
		    + sizeof(" Players.") - 1U, error))
			return false;
	}

	yt_record_blank(&record);
	for (logical = 1; logical <= (int)config->sector_offset - 1;
	    ++logical) {
		if (!yt_database_write(database, (size_t)logical + 1U,
		    &record, error))
			return false;
	}
	if (!yt_database_flush(database, error)
	    || !yt_present_text(options, 0x0cf5U, YT_INIT_OUTPUT_LINE, "",
	    error)
	    || !yt_present_text(options, 0x0d07U, YT_INIT_OUTPUT_LINE,
	    "Initializing sectors...", error))
		return false;
	return rmt_present(options, 0x0d67U, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error);
}

static bool
write_world_database(struct yt_database *database,
    const struct yt_initializer_options *options, struct yt_config *config,
    const struct world *world, struct yt_random *random,
    struct yt_error *error)
{
	struct yt_record record;
	struct yt_clock_value port_date;
	int today;
	int logical;

	if (!yt_present_text(options, 0x163fU, YT_INIT_OUTPUT_LINE, "", error)
	    || !yt_present_text(options, 0x1651U, YT_INIT_OUTPUT_LINE,
	    "Random sector data generated and verified. Writing...", error)
	    || !rmt_present(options, 0x138eU, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    || !rmt_present_text(options, 0x139cU, YT_RMT_OUTPUT_INLINE,
	    "Random sector data generated and verified. Writing...", error))
		return false;
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
		if (logical % 50 == 0
		    && !rmt_present_text(options, 0x13e9U,
		    YT_RMT_OUTPUT_INLINE, ".", error))
			return false;
	}

	if (!yt_present_text(options, 0x18f6U, YT_INIT_OUTPUT_LINE, "",
	    error)
	    || !yt_present_text(options, 0x1908U, YT_INIT_OUTPUT_LINE,
	    "Initializing ports...", error)
	    || !rmt_present(options, 0x1678U, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    || !rmt_present(options, 0x167bU, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    || !rmt_present_text(options, 0x1689U, YT_RMT_OUTPUT_LINE,
	    "Initializing ports... (Be patient)", error))
		return false;
	if (!yt_platform_clock(&port_date, error))
		return false;
	today = yt_date_serial(&port_date, config->epoch_year, NULL);
	if (!yt_present_text(options, 0x1a55U, YT_INIT_OUTPUT_LINE, "", error)
	    || !yt_present_text(options, 0x1a69U, YT_INIT_OUTPUT_LINE,
	    "   They started producing 10 days ago...", error)
	    || !yt_present_text(options, 0x1a7aU, YT_INIT_OUTPUT_LINE, "",
	    error)
	    || !yt_present(options, 0x1acaU, YT_INIT_OUTPUT_LOCATE_ROW_25,
	    NULL, 0U, error)
	    || !yt_present_text(options, 0x1ad7U, YT_INIT_OUTPUT_COMMA,
	    "Port #", error)
	    || !yt_present_text(options, 0x1adfU, YT_INIT_OUTPUT_COMMA,
	    "Prod Ore", error)
	    || !yt_present_text(options, 0x1ae7U, YT_INIT_OUTPUT_COMMA,
	    "Prod Org", error)
	    || !yt_present_text(options, 0x1aefU, YT_INIT_OUTPUT_COMMA,
	    "Prod Equ", error)
	    || !yt_present_text(options, 0x1af7U, YT_INIT_OUTPUT_LINE,
	    "Port Name", error)
	    || !rmt_present(options, 0x17f4U, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    || !rmt_present_text(options, 0x1802U, YT_RMT_OUTPUT_INLINE,
	    "   They started producing 10 days ago...", error))
		return false;
	for (logical = 1; logical <= world->ports; ++logical) {
		char name[42];
		float sample;
		int commodity;
		int index;

		if (!yt_present_number(options, 0x1b1eU, (float)logical,
		    YT_INIT_OUTPUT_COMMA, error))
			return false;
		yt_record_clear(&record);
		for (index = 0; index < 3; ++index) {
			if (!yt_random_next(random, &sample, error))
				return false;
			yt_record_set_number(&record, YT_F61 + (size_t)index * 4U,
			    (float)((int)floorf(qb_single_multiply(sample, 31767.0f))
			    + 1000));
		}
		for (index = 0; index < 3; ++index) {
			if (!yt_random_next(random, &sample, error))
				return false;
			yt_record_set_number(&record, YT_F73 + (size_t)index * 4U,
			    (float)(-(int)floorf(qb_single_multiply(sample, 100.0f))
			    - 1));
		}
		if (logical == 1)
			strcpy(name, "Earth");
		else if (!yt_generate_port_name(random, name, error))
			return false;
		if (!yt_present_text(options, 0x1c59U, YT_INIT_OUTPUT_LINE,
		    name, error))
			return false;
		if (!yt_random_next(random, &sample, error))
			return false;
		commodity = (int)floorf(qb_single_multiply(sample, 3.0f)) + 1;
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
		if (options->family == YT_INITIALIZER_RMT)
			yt_record_set_raw_number(&record, YT_F101,
			    raw_zero_residue);
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
		if (logical % 15 == 0
		    && !rmt_present_text(options, 0x1c46U,
		    YT_RMT_OUTPUT_INLINE, ".", error))
			return false;
	}

	if (!yt_present(options, 0x1ecaU, YT_INIT_OUTPUT_LOCATE_ROW_25,
	    NULL, 0U, error)
	    || !yt_present_text(options, 0x1edeU, YT_INIT_OUTPUT_LINE,
	    "                                                                               ",
	    error)
	    || !yt_present_text(options, 0x1ef0U, YT_INIT_OUTPUT_LINE, "",
	    error)
	    || !yt_present_text(options, 0x1f02U, YT_INIT_OUTPUT_LINE,
	    "Initializing planets...", error)
	    || !yt_present_str_number_line(options, 0x1f2bU,
	    "   Maximum number of planets:",
	    config->total_records - config->planet_offset, error)
	    || !rmt_present(options, 0x1c9dU, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    || !rmt_present(options, 0x1ca0U, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    || !rmt_present_text(options, 0x1caeU, YT_RMT_OUTPUT_LINE,
	    "Initializing planets...", error)
	    || !rmt_present_number_line(options, 0x1cc9U,
	    "   Maximum number of planets:",
	    config->total_records - config->planet_offset, error))
		return false;
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

	if (!yt_present_text(options, 0x1fd1U, YT_INIT_OUTPUT_LINE, "",
	    error)
	    || !yt_present_text(options, 0x1fe5U, YT_INIT_OUTPUT_LINE,
	    "Initializing the Xannor...", error)
	    || !rmt_present(options, 0x1d53U, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    || !rmt_present_text(options, 0x1d61U, YT_RMT_OUTPUT_LINE,
	    "Initializing the Xannor...", error))
		return false;
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

static bool write_sequential_file(const char *path, const uint8_t *data,
    size_t length, struct yt_error *error);

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
	if (!write_sequential_file("YTNEWS.DAT", data, length, error))
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
clear_yt_radio_messages(struct yt_error *error)
{
	struct yt_text_output output;
	bool result = false;

	yt_text_output_init(&output);
	if (!yt_text_output_open(&output, "YTRMSG.DAT", error))
		goto done;
	if (!yt_text_output_close_all(&output, error)
	    || !yt_file_kill("YTRMSG.DAT", error))
		goto done;
	result = true;

done:
	yt_text_output_destroy(&output);
	return result;
}

static bool
write_sequential_file(const char *path, const uint8_t *data,
    size_t length, struct yt_error *error)
{
	struct yt_text_output output;
	bool result = false;

	yt_text_output_init(&output);
	if (!yt_text_output_open(&output, path, error)
	    || !yt_text_output_write(&output, data, length, error)
	    || !yt_text_output_close(&output, error))
		goto done;
	result = true;

done:
	yt_text_output_destroy(&output);
	return result;
}

static bool
write_yt_auxiliary(struct yt_database *database,
    const struct yt_initializer_options *options, struct yt_error *error)
{
	static const uint8_t dummy[] = "Dummy,Dummy,Dummy,Dummy\r\n";
	static const uint8_t play[] = "L64cgaL1p1p1p1";

	if (!write_banner(NULL, false, error)
	    || !yt_present_text(options, 0x224bU, YT_INIT_OUTPUT_LINE, "",
	    error)
	    || !yt_present_text(options, 0x225fU, YT_INIT_OUTPUT_LINE,
	    "Initializing the alias file (Matches real name to alias.)", error)
	    || !yt_database_random_close(database, error)
	    || !write_sequential_file("YTNAME.DAT", dummy,
	    sizeof(dummy) - 1U, error)
	    || !yt_present_text(options, 0x22deU, YT_INIT_OUTPUT_LINE, "",
	    error)
	    || !yt_present_text(options, 0x22f0U, YT_INIT_OUTPUT_LINE,
	    "Clearing YTRMSG.DAT  (Radio message file)", error)
	    || !clear_yt_radio_messages(error)
	    || !yt_present_text(options, 0x2325U, YT_INIT_OUTPUT_LINE, "",
	    error)
	    || !yt_present_text(options, 0x2339U, YT_INIT_OUTPUT_LINE,
	    "Initialization completed sucessfully!", error)
	    || !yt_present_text(options, 0x234aU, YT_INIT_OUTPUT_LINE, "",
	    error)
	    || !yt_present_text(options, 0x235cU, YT_INIT_OUTPUT_LINE,
	    "<YT-INIT Normal Termination>", error)
	    || !yt_present_text(options, 0x236dU, YT_INIT_OUTPUT_LINE, "",
	    error)
	    || !yt_present_text(options, 0x237fU, YT_INIT_OUTPUT_LINE,
	    "Be SURE to run YTMAINT.EXE at LEAST ONCE per day EVERY DAY!", error)
	    || !yt_present_text(options, 0x2390U, YT_INIT_OUTPUT_LINE, "",
	    error)
	    || !yt_present_text(options, 0x23a2U, YT_INIT_OUTPUT_LINE,
	    "Run YTCONFIG and change the default OPTIONS if you wish!", error)
	    || !yt_present_text(options, 0x23b3U, YT_INIT_OUTPUT_LINE, "",
	    error)
	    || !yt_present_text(options, 0x23c5U, YT_INIT_OUTPUT_LINE,
	    "Running initial maintenance...", error)
	    || !yt_present(options, 0x23cfU, YT_INIT_OUTPUT_PLAY, play,
	    sizeof(play) - 1U, error))
		return false;
	return true;
}

static bool
write_rmt_auxiliary(struct yt_database *database, const char *credited_name,
    const struct yt_initializer_options *options, struct yt_error *error)
{
	static const uint8_t dummy[] = "Dummy,Dummy,Dummy,Dummy\r\n";
	static const uint8_t yesterday[] =
	    "NO YESTERDAY'S NEWS TO READ!\r\n"
	    "NO YESTERDAY'S NEWS TO READ!\r\n"
	    "NO YESTERDAY'S NEWS TO READ!\r\n"
	    "NO YESTERDAY'S NEWS TO READ!\r\n"
	    "NO YESTERDAY'S NEWS TO READ!\r\n";
	struct yt_radio_record radio;
	struct yt_radio_file file;
	char prophecy[180];
	int written;
	int index;
	bool valid = false;

	yt_radio_file_init(&file);

	if (!write_banner(credited_name, true, error)
	    || !rmt_present(options, 0x2014U, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    || !rmt_present_text(options, 0x2022U, YT_RMT_OUTPUT_LINE,
	    "Setting up yesterday's newspaper file.", error)
	    || !write_sequential_file("YTYNEWS.DAT", yesterday,
	    sizeof(yesterday) - 1U, error)
	    || !rmt_present(options, 0x20a1U, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    || !rmt_present_text(options, 0x20afU, YT_RMT_OUTPUT_LINE,
	    "Initializing the alias file (Matches real name to alias.)", error)
	    || !yt_database_random_close(database, error)
	    || !write_sequential_file("YTNAME.DAT", dummy,
	    sizeof(dummy) - 1U, error)
	    || !rmt_present(options, 0x20ebU, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    || !rmt_present_text(options, 0x20f9U, YT_RMT_OUTPUT_LINE,
	    "Clearing YTRMSG.DAT  (Radio message file)", error))
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
	if (!write_sequential_file("YTRMSG.DAT", NULL, 0U, error)
	    || !yt_radio_file_open(&file, "YTRMSG.DAT", error))
		goto done;
	for (index = 0; index < 5; ++index) {
		uint32_t record;
		uint64_t size;

		/*
		 * The empty OUTPUT close at 2113 leaves one DOS EOF byte.  The
		 * first LOF/86+1 expression converts that fractional value to
		 * record 1 and overwrites it; the following four LOFs are aligned.
		 */
		if (index == 0) {
			if (!yt_radio_file_size(&file, &size, error)
			    || size != 1U) {
				if (error != NULL && error->status == YT_OK)
					set_error(error, YT_RANGE,
					    "RMT initial radio LOF", "YTRMSG.DAT");
				goto done;
			}
			record = 1U;
		}
		else if (!yt_radio_file_next_record(&file, &record, error))
			goto done;
		if (!yt_radio_file_put(&file, record, &radio, error))
			goto done;
	}
	if (!yt_radio_file_close(&file, error)
	    || !rmt_present(options, 0x227eU, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error))
		goto done;
	valid = true;

done:
	if (file.random.file != NULL)
		(void)yt_radio_file_close(&file, NULL);
	return valid;
}

static void
free_world(struct world *world)
{
	free(world->warps);
	free(world->port_sectors);
	free(world->sector_ports);
	memset(world, 0, sizeof(*world));
}

static bool
allocate_world(struct world *world, struct yt_error *error)
{
	world->warps = calloc((size_t)world->sectors + 1U,
	    sizeof(*world->warps));
	world->port_sectors = calloc((size_t)world->ports + 1U,
	    sizeof(*world->port_sectors));
	world->sector_ports = calloc((size_t)world->sectors + 1U,
	    sizeof(*world->sector_ports));
	if (world->warps == NULL || world->port_sectors == NULL
	    || world->sector_ports == NULL) {
		set_error(error, YT_NO_MEMORY, "initializer world", "");
		return false;
	}
	return true;
}

bool
yt_initialize_world(const struct yt_initializer_options *options,
    struct yt_random *random, struct yt_error *error)
{
	struct yt_clock_value current;
	struct yt_config config;
	struct yt_database owned_database;
	struct yt_database *database;
	struct world world = {0};
	float sample;
	bool explicit_close_failed = false;
	bool result = false;

	if (options == NULL || random == NULL) {
		set_error(error, YT_INVALID, "initializer arguments", "");
		return false;
	}
	memset(&owned_database, 0, sizeof(owned_database));
	database = options->bound_database != NULL
	    ? options->bound_database : &owned_database;
	if (options->family == YT_INITIALIZER_RMT) {
		if (options->bound_database != NULL) {
			set_error(error, YT_INVALID, "bound RMT initializer", "");
			goto done;
		}
		if (!options->use_existing_config) {
			set_error(error, YT_INVALID, "RMT initializer configuration", "");
			goto done;
		}
		config = options->config;
		config.scoreboard_length = (float)strlen(config.scoreboard);
		world.sectors = (int)(config.port_offset - config.sector_offset);
		world.ports = (int)(config.planet_offset - config.port_offset);
		if (world.sectors < 7 || world.ports < 4) {
			set_error(error, YT_RANGE, "initializer configuration", "");
			goto done;
		}
		if (!allocate_world(&world, error)
		    || !rmt_present_preopen(options, error)
		    /* 08A1 OUTPUT/CLOSE leaves DOS EOF before 26D7 RANDOM reopen. */
		    || !write_sequential_file("YTDATA.DAT", NULL, 0U, error)
		    || !yt_database_random_close(database, error)
		    || !yt_database_open(database, "YTDATA.DAT",
		    YT_OPEN_UPDATE_CREATE, error)
		    || !yt_platform_clock(&current, error))
			goto done;
		config.epoch_year = (float)(current.year % 100);
		if (!rmt_present_before_headquarters(options, &config, error)
		    || !yt_random_next(random, &sample, error))
			goto done;
		config.headquarters = (float)((int)floorf(qb_single_multiply(sample,
		    qb_single_add((float)world.sectors, -7.0f))) + 1);
		if (!rmt_present_after_headquarters(options, &config, error))
			goto done;
		make_config_record(&config, config.scoreboard_length);
		if (!write_config_and_players(database, &config, options, error)
		    || !rmt_present_graph_opening(options, error))
			goto done;
	}
	else {
		const char *scoreboard;
		size_t scoreboard_length;

		if (options->bound_database != NULL) {
			if (database->file == NULL) {
				set_error(error, YT_INVALID,
				    "bound YT initializer", "YTDATA.DAT");
				goto done;
			}
		}
		else if (!yt_database_open(database, "YTDATA.DAT",
		    options->database_already_truncated ? YT_OPEN_UPDATE
		    : YT_OPEN_CREATE, error))
			goto done;
		if (options->prepared_yt) {
			if (!options->use_existing_config) {
				set_error(error, YT_INVALID,
				    "prepared YT-INIT configuration", "");
				goto done;
			}
			config = options->config;
			config.scoreboard_length =
			    (float)strlen(config.scoreboard);
			world.sectors = (int)(config.port_offset
			    - config.sector_offset);
			world.ports = (int)(config.planet_offset
			    - config.port_offset);
		}
		else if (!yt_platform_clock(&current, error))
			goto done;
		else if (options->use_existing_config) {
			config = options->config;
			config.scoreboard_length =
			    (float)strlen(config.scoreboard);
			world.sectors = (int)(config.port_offset
			    - config.sector_offset);
			world.ports = (int)(config.planet_offset
			    - config.port_offset);
		}
		else {
			scoreboard = options->scoreboard != NULL
			    && options->scoreboard[0] != '\0'
			    ? options->scoreboard : "YTSCORE.ASC";
			scoreboard_length = strlen(scoreboard);
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
			world.sectors = YT_INIT_SECTORS;
			world.ports = YT_INIT_PORTS;
		}
		if (world.sectors < 7 || world.ports < 4) {
			set_error(error, YT_RANGE, "initializer configuration", "");
			goto done;
		}
		if (!options->prepared_yt) {
			if (!yt_platform_clock(&current, error))
				goto done;
			config.last_maintenance = (float)(yt_date_serial(&current,
			    config.epoch_year, NULL) - 1);
			if (!yt_random_next(random, &sample, error))
				goto done;
			config.headquarters = (float)((int)floorf(qb_single_multiply(sample,
			    qb_single_add((float)world.sectors, -7.0f))) + 1);
		}
		make_config_record(&config, config.scoreboard_length);
		if (!write_config_and_players(database, &config, options, error)
		    || !yt_init_sector_prepass(database, config.sector_offset,
		    world.sectors, &config.port_offset, error)
		    || !allocate_world(&world, error))
			goto done;
	}
	if (!yt_present_graph_opening(options, error)
	    || !build_graph(&world, options->family, random, options, error)
	    || !assign_ports(&world, random, error)
	    || !write_world_database(database, options, &config, &world,
	    random, error))
		goto done;
	if (options->family == YT_INITIALIZER_YT) {
		if (!yt_present_text(options, 0x208cU, YT_INIT_OUTPUT_LINE, "",
		    error)
		    || !yt_present_text(options, 0x209eU, YT_INIT_OUTPUT_LINE,
		    "Setting up newspaper file.", error))
			goto done;
	}
	else if (!rmt_present(options, 0x1dfaU, YT_RMT_OUTPUT_BLANK, NULL,
	    0U, error)
	    || !rmt_present_text(options, 0x1e08U, YT_RMT_OUTPUT_LINE,
	    "Setting up newspaper file.", error))
		goto done;
	if (!yt_database_random_close(database, error)) {
		explicit_close_failed = true;
		goto done;
	}
	if (options->family == YT_INITIALIZER_YT)
		result = write_yt_auxiliary(database, options, error);
	else
		result = write_rmt_auxiliary(database, options->credited_name,
		    options, error);

done:
	if (!explicit_close_failed)
		yt_database_close(database);
	free_world(&world);
	return result;
}

bool
yt_initialize_begin_yt(struct yt_error *error)
{
	struct yt_database database;

	if (!yt_database_open(&database, "YTDATA.DAT", YT_OPEN_CREATE, error))
		return false;
	return yt_database_random_close(&database, error);
}

bool
yt_initialize_bind_yt(struct yt_database *database,
    struct yt_init_binding *binding, struct yt_error *error)
{
	struct yt_record first;

	if (database == NULL || binding == NULL) {
		set_error(error, YT_INVALID, "bind YT initializer", "YTDATA.DAT");
		return false;
	}
	memset(binding, 0, sizeof(*binding));
	if (!yt_database_open(database, "YTDATA.DAT", YT_OPEN_UPDATE, error)
	    || !yt_database_random_get(database, 1U, &first,
	    &binding->first_accepted, error)
	    || !yt_config_decode(&binding->loaded, &first, error)
	    || !yt_database_random_get(database, 1U, &binding->second_record,
	    &binding->second_accepted, error)) {
		yt_database_close(database);
		return false;
	}
	return true;
}

bool
yt_initialize_yt_prepared_bound(struct yt_database *database,
    const struct yt_initializer_preparation *preparation,
    const char *scoreboard, struct yt_random *random,
    const struct yt_init_presenter *presenter, struct yt_error *error)
{
	struct yt_initializer_options options;
	const char *selected;
	size_t length;
	size_t retained;

	if (preparation == NULL || random == NULL) {
		set_error(error, YT_INVALID, "prepared YT-INIT", "");
		return false;
	}
	memset(&options, 0, sizeof(options));
	options.family = YT_INITIALIZER_YT;
	options.config = preparation->config;
	selected = scoreboard != NULL && scoreboard[0] != '\0'
	    ? scoreboard : "YTSCORE.ASC";
	length = strlen(selected);
	retained = length < sizeof(options.config.scoreboard) - 1U
	    ? length : sizeof(options.config.scoreboard) - 1U;
	memcpy(options.config.scoreboard, selected, retained);
	options.config.scoreboard[retained] = '\0';
	options.config.scoreboard_length = (float)length;
	options.use_existing_config = true;
	options.database_already_truncated = true;
	options.yt_presenter = presenter;
	options.prepared_yt = true;
	options.bound_database = database;
	return yt_initialize_world(&options, random, error);
}

bool
yt_initialize_rmt_presented(const struct yt_config *config,
    const char *credited_name, struct yt_random *random,
    const struct yt_rmt_presenter *presenter, struct yt_error *error)
{
	if (config == NULL) {
		set_error(error, YT_INVALID, "RMT initializer configuration", "");
		return false;
	}
	struct yt_initializer_options options = {
	    .family = YT_INITIALIZER_RMT,
	    .config = *config,
	    .use_existing_config = true,
	    .credited_name = credited_name,
	    .rmt_presenter = presenter
	};

	return yt_initialize_world(&options, random, error);
}
