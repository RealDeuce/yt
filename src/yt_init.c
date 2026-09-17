#include "yt_init.h"
#include "yt_init_internal.h"

#include "qb.h"
#include "yt_portname.h"
#include "yt_startup.h"

#include <math.h>
#include <string.h>

#define YT_INIT_PLAYERS 50
#define YT_INIT_SECTORS 2004
#define YT_INIT_PORTS 1000
#define YT_INIT_PLANETS 100

static const uint8_t raw_zero_residue[4] = {0x00, 0x00, 0xa0, 0x00};

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
yt_init_sector_prepass(struct yt_database *database, int sector_offset,
    int sector_count, float *port_offset, struct yt_error *error)
{
	struct yt_record record;
	float computed;

	if (database == NULL || port_offset == NULL || sector_count < 0) {
		set_error(error, YT_INVALID, "YT-INIT sector prepass", "");
		return false;
	}
	computed = qb_single_add((float)sector_offset, (float)sector_count);
	*port_offset = computed;
	if (!yt_database_read(database, 1U, &record, error))
		return false;
	yt_record_set_number(&record, YT_F57, computed);
	return yt_database_write(database, 1U, &record, error);
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
yt_initializer_prepare_yt(const struct yt_clock *clock,
    struct yt_random *random,
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
	if (!yt_clock_read(clock, &epoch_date, error)
	    || !yt_clock_read(clock, &maintenance_date, error)
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

static void
make_config_record(struct yt_config *config, size_t stored_scoreboard_length)
{
	size_t length = strlen(config->scoreboard);

	yt_record_clear(&config->record);
	yt_record_set_text(&config->record,
	    (const uint8_t *)config->scoreboard, length);
	yt_record_set_number(&config->record, YT_F41,
	    (float)stored_scoreboard_length);
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
	return rmt_present(options, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    && rmt_present_text(options, YT_RMT_OUTPUT_LINE,
	    "          Yankee Trader Remote Initialization Program v2.2", error)
	    && rmt_present_text(options, YT_RMT_OUTPUT_LINE,
	    "                         By Alan Davenport", error)
	    && rmt_present(options, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    && rmt_present(options, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    && rmt_present_text(options, YT_RMT_OUTPUT_LINE,
	    "Re-creating main data file: YTDATA.DAT", error);
}

static bool
rmt_present_before_headquarters(const struct yt_initializer_options *options,
    struct yt_config *config, struct yt_error *error)
{
	struct yt_clock_value maintenance_date;
	int maintenance_serial;

	if (!rmt_present(options, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    || !rmt_present_text(options, YT_RMT_OUTPUT_LINE,
	    "Starting player info:", error)
	    || !rmt_present_number_line(options,
	    "  # of fighters at start:", config->initial_fighters, error)
	    || !rmt_present_number_line(options,
	    "  # of credits at start:", config->initial_credits, error)
	    || !rmt_present_number_line(options,
	    "  # of cargo holds at start:", config->initial_holds, error)
	    || !rmt_present_number_line(options,
	    "  # of days inactivity until an dead player is deleted:",
	    config->retention_days, error))
		return false;
	if (!yt_clock_read(options->clock, &maintenance_date, error))
		return false;
	maintenance_serial = yt_date_serial(&maintenance_date,
	    config->epoch_year, NULL);
	config->last_maintenance = (float)(maintenance_serial - 1);
	return rmt_present_text(options, YT_RMT_OUTPUT_LINE,
	    "  Last day maintenance run: Yesterday", error)
	    && rmt_present_number_line(options, "  # of turns per day:",
	    config->turns_per_day, error)
	    && rmt_present_number_line(options,
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

	if (!rmt_present_number_line(options,
	    "  Xannor Headquarters placed in sector:", config->headquarters,
	    error)
	    || !rmt_present_number_line(options,
	    "  Ports needed to initiate Genesis:", config->genesis_ports,
	    error)
	    || !rmt_present_number_line(options,
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
	return rmt_present(options, YT_RMT_OUTPUT_LINE, payload,
	    prefix_length + scoreboard_length, error)
	    && rmt_present(options, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error);
}

static bool
rmt_present_graph_opening(const struct yt_initializer_options *options,
    struct yt_error *error)
{
	return rmt_present_text(options, YT_RMT_OUTPUT_LINE,
	    "Initializing sectors...", error)
	    && rmt_present(options, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    && rmt_present_text(options, YT_RMT_OUTPUT_LINE,
	    "Generating Randomized Universe... Please be patient...", error)
	    && rmt_present(options, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    && rmt_present_text(options, YT_RMT_OUTPUT_LINE,
	    "Wormholes (Long Warps)..", error)
	    && rmt_present(options, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error);
}

static bool
yt_present_graph_opening(const struct yt_initializer_options *options,
    struct yt_error *error)
{
	return yt_present_text(options, YT_INIT_OUTPUT_LINE, "",
	    error)
	    && yt_present_text(options, YT_INIT_OUTPUT_LINE,
	    "Generating Randomized Universe... Please be patient...", error)
	    && yt_present_text(options, YT_INIT_OUTPUT_LINE, "",
	    error)
	    && yt_present_text(options, YT_INIT_OUTPUT_LINE,
	    "Wormholes (Long Warps)..", error)
	    && yt_present_text(options, YT_INIT_OUTPUT_LINE, "",
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
		    || !yt_present_text(options, YT_INIT_OUTPUT_LINE,
		    "", error)
		    || !yt_database_write(database, 1, &config->record, error)
		    || !yt_present(options, YT_INIT_OUTPUT_LINE,
		    config->record.bytes + YT_F53, 4U, error)
		    || !yt_present_text(options, YT_INIT_OUTPUT_INLINE,
		    "Generating Player Records for", error)
		    || !yt_present_number(options,
		    config->sector_offset - 1.0f, YT_INIT_OUTPUT_INLINE, error)
		    || !yt_present_text(options, YT_INIT_OUTPUT_LINE,
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
		    || !rmt_present(options, YT_RMT_OUTPUT_BLANK, NULL,
		    0U, error)
		    || !rmt_present(options, YT_RMT_OUTPUT_LINE,
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
	    || !yt_present_text(options, YT_INIT_OUTPUT_LINE, "",
	    error)
	    || !yt_present_text(options, YT_INIT_OUTPUT_LINE,
	    "Initializing sectors...", error))
		return false;
	return rmt_present(options, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error);
}

static bool
write_world_database(struct yt_database *database,
    const struct yt_initializer_options *options, struct yt_config *config,
    const struct yt_init_world *world, struct yt_random *random,
    struct yt_error *error)
{
	struct yt_record record;
	struct yt_clock_value port_date;
	int today;
	int logical;

	if (!yt_present_text(options, YT_INIT_OUTPUT_LINE, "", error)
	    || !yt_present_text(options, YT_INIT_OUTPUT_LINE,
	    "Random sector data generated and verified. Writing...", error)
	    || !rmt_present(options, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    || !rmt_present_text(options, YT_RMT_OUTPUT_INLINE,
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
		    && !rmt_present_text(options, YT_RMT_OUTPUT_INLINE, ".", error))
			return false;
	}

	if (!yt_present_text(options, YT_INIT_OUTPUT_LINE, "",
	    error)
	    || !yt_present_text(options, YT_INIT_OUTPUT_LINE,
	    "Initializing ports...", error)
	    || !rmt_present(options, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    || !rmt_present(options, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    || !rmt_present_text(options, YT_RMT_OUTPUT_LINE,
	    "Initializing ports... (Be patient)", error))
		return false;
	if (!yt_clock_read(options->clock, &port_date, error))
		return false;
	today = yt_date_serial(&port_date, config->epoch_year, NULL);
	if (!yt_present_text(options, YT_INIT_OUTPUT_LINE, "", error)
	    || !yt_present_text(options, YT_INIT_OUTPUT_LINE,
	    "   They started producing 10 days ago...", error)
	    || !yt_present_text(options, YT_INIT_OUTPUT_LINE, "",
	    error)
	    || !yt_present(options, YT_INIT_OUTPUT_LOCATE_ROW_25,
	    NULL, 0U, error)
	    || !yt_present_text(options, YT_INIT_OUTPUT_COMMA,
	    "Port #", error)
	    || !yt_present_text(options, YT_INIT_OUTPUT_COMMA,
	    "Prod Ore", error)
	    || !yt_present_text(options, YT_INIT_OUTPUT_COMMA,
	    "Prod Org", error)
	    || !yt_present_text(options, YT_INIT_OUTPUT_COMMA,
	    "Prod Equ", error)
	    || !yt_present_text(options, YT_INIT_OUTPUT_LINE,
	    "Port Name", error)
	    || !rmt_present(options, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    || !rmt_present_text(options, YT_RMT_OUTPUT_INLINE,
	    "   They started producing 10 days ago...", error))
		return false;
	for (logical = 1; logical <= world->ports; ++logical) {
		char name[42];
		float sample;
		int commodity;
		int index;

		if (!yt_present_number(options, (float)logical,
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
		if (!yt_present_text(options, YT_INIT_OUTPUT_LINE,
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
		    && !rmt_present_text(options, YT_RMT_OUTPUT_INLINE, ".", error))
			return false;
	}

	if (!yt_present(options, YT_INIT_OUTPUT_LOCATE_ROW_25,
	    NULL, 0U, error)
	    || !yt_present_text(options, YT_INIT_OUTPUT_LINE,
	    "                                                                               ",
	    error)
	    || !yt_present_text(options, YT_INIT_OUTPUT_LINE, "",
	    error)
	    || !yt_present_text(options, YT_INIT_OUTPUT_LINE,
	    "Initializing planets...", error)
	    || !yt_present_str_number_line(options, "   Maximum number of planets:",
	    config->total_records - config->planet_offset, error)
	    || !rmt_present(options, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    || !rmt_present(options, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    || !rmt_present_text(options, YT_RMT_OUTPUT_LINE,
	    "Initializing planets...", error)
	    || !rmt_present_number_line(options, "   Maximum number of planets:",
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

	if (!yt_present_text(options, YT_INIT_OUTPUT_LINE, "",
	    error)
	    || !yt_present_text(options, YT_INIT_OUTPUT_LINE,
	    "Initializing the Xannor...", error)
	    || !rmt_present(options, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    || !rmt_present_text(options, YT_RMT_OUTPUT_LINE,
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

bool
yt_initialize_world(const struct yt_initializer_options *options,
    struct yt_random *random, struct yt_error *error)
{
	struct yt_clock_value current;
	struct yt_config config;
	struct yt_database owned_database;
	struct yt_database *database;
	struct yt_init_world world = {0};
	float sample;
	bool explicit_close_failed = false;
	bool result = false;

	if (options == NULL || random == NULL) {
		set_error(error, YT_INVALID, "initializer arguments", "");
		return false;
	}
	if ((options->family == YT_INITIALIZER_YT
	    && (options->yt_presenter == NULL
	    || options->yt_presenter->write == NULL))
	    || (options->family == YT_INITIALIZER_RMT
	    && (options->rmt_presenter == NULL
	    || options->rmt_presenter->write == NULL))) {
		set_error(error, YT_INVALID, "initializer output", "");
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
		config.scoreboard_length = strlen(config.scoreboard);
		world.sectors = (int)(config.port_offset - config.sector_offset);
		world.ports = (int)(config.planet_offset - config.port_offset);
		if (world.sectors < 7 || world.ports < 4) {
			set_error(error, YT_RANGE, "initializer configuration", "");
			goto done;
		}
		if (!yt_init_world_allocate(&world, error)
		    || !rmt_present_preopen(options, error)
		    /* 08A1 OUTPUT/CLOSE leaves DOS EOF before 26D7 RANDOM reopen. */
		    || !yt_init_write_sequential_file("YTDATA.DAT", NULL, 0U,
		    error)
		    || !yt_database_random_close(database, error)
		    || !yt_database_open(database, "YTDATA.DAT",
		    YT_OPEN_UPDATE_CREATE, error)
		    || !yt_clock_read(options->clock, &current, error))
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
			config.scoreboard_length = strlen(config.scoreboard);
			world.sectors = (int)(config.port_offset
			    - config.sector_offset);
			world.ports = (int)(config.planet_offset
			    - config.port_offset);
		}
		else if (!yt_clock_read(options->clock, &current, error))
			goto done;
		else if (options->use_existing_config) {
			config = options->config;
			config.scoreboard_length = strlen(config.scoreboard);
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
			config.scoreboard_length = scoreboard_length;
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
			if (!yt_clock_read(options->clock, &current, error))
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
		    || !yt_init_sector_prepass(database, (int)config.sector_offset,
		    world.sectors, &config.port_offset, error)
		    || !yt_init_world_allocate(&world, error))
			goto done;
	}
	if (!yt_present_graph_opening(options, error)
	    || !yt_init_world_build_graph(&world, options->family, random,
	    options, error)
	    || !yt_init_world_assign_ports(&world, random, error)
	    || !write_world_database(database, options, &config, &world,
	    random, error))
		goto done;
	if (options->family == YT_INITIALIZER_YT) {
		if (!yt_present_text(options, YT_INIT_OUTPUT_LINE, "",
		    error)
		    || !yt_present_text(options, YT_INIT_OUTPUT_LINE,
		    "Setting up newspaper file.", error))
			goto done;
	}
	else if (!rmt_present(options, YT_RMT_OUTPUT_BLANK, NULL,
	    0U, error)
	    || !rmt_present_text(options, YT_RMT_OUTPUT_LINE,
	    "Setting up newspaper file.", error))
		goto done;
	if (!yt_database_random_close(database, error)) {
		explicit_close_failed = true;
		goto done;
	}
	if (options->family == YT_INITIALIZER_YT)
		result = yt_init_write_yt_auxiliary(database, options, error);
	else
		result = yt_init_write_rmt_auxiliary(database,
		    options->credited_name,
		    options, error);

done:
	if (!explicit_close_failed)
		yt_database_close(database);
	yt_init_world_free(&world);
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
yt_initialize_bind_yt(struct yt_database *database, struct yt_error *error)
{
	struct yt_record first;
	struct yt_record second;
	struct yt_config loaded;

	if (database == NULL) {
		set_error(error, YT_INVALID, "bind YT initializer", "YTDATA.DAT");
		return false;
	}
	if (!yt_database_open(database, "YTDATA.DAT", YT_OPEN_UPDATE, error)
	    || !yt_database_read(database, 1U, &first, error)
	    || !yt_config_decode(&loaded, &first, error)
	    || !yt_database_read(database, 1U, &second, error)) {
		yt_database_close(database);
		return false;
	}
	return true;
}

bool
yt_initialize_yt_prepared_bound(struct yt_database *database,
    const struct yt_initializer_preparation *preparation,
    const char *scoreboard, const struct yt_clock *clock,
    struct yt_random *random,
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
	options.clock = clock;
	options.config = preparation->config;
	selected = scoreboard != NULL && scoreboard[0] != '\0'
	    ? scoreboard : "YTSCORE.ASC";
	length = strlen(selected);
	retained = length < sizeof(options.config.scoreboard) - 1U
	    ? length : sizeof(options.config.scoreboard) - 1U;
	memcpy(options.config.scoreboard, selected, retained);
	options.config.scoreboard[retained] = '\0';
	options.config.scoreboard_length = length;
	options.use_existing_config = true;
	options.database_already_truncated = true;
	options.yt_presenter = presenter;
	options.prepared_yt = true;
	options.bound_database = database;
	return yt_initialize_world(&options, random, error);
}

bool
yt_initialize_rmt_presented(const struct yt_config *config,
    const char *credited_name, const struct yt_clock *clock,
    struct yt_random *random,
    const struct yt_rmt_presenter *presenter, struct yt_error *error)
{
	if (config == NULL) {
		set_error(error, YT_INVALID, "RMT initializer configuration", "");
		return false;
	}
	struct yt_initializer_options options = {
	    .family = YT_INITIALIZER_RMT,
	    .clock = clock,
	    .config = *config,
	    .use_existing_config = true,
	    .credited_name = credited_name,
	    .rmt_presenter = presenter
	};

	return yt_initialize_world(&options, random, error);
}
