#include "qb.h"
#include "yt_game.h"
#include "yt_maint.h"
#include "yt_platform.h"
#include "yt_score.h"
#include "yt_score_format.h"
#include "yt_text.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define yt_chdir _chdir
#define yt_rmdir _rmdir
#else
#include <unistd.h>
#define yt_chdir chdir
#define yt_rmdir rmdir
#endif

static int
fail(const char *message)
{
	fprintf(stderr, "test_score: %s\n", message);
	return EXIT_FAILURE;
}

struct score_database_read_fault {
	size_t calls;
	size_t fail_at;
};

static bool
score_database_read_with_fault(void *context, FILE *file, uint8_t *data,
    size_t requested, struct yt_database_read_observation *observation)
{
	struct score_database_read_fault *fault = context;
	long position;

	memset(observation, 0, sizeof(*observation));
	++fault->calls;
	if (fault->calls == fault->fail_at) {
		observation->carry = true;
		observation->dos_error = 6U;
		observation->terminal_position = 0x55667788;
		return true;
	}
	observation->accepted = fread(data, 1U, requested, file);
	observation->carry = ferror(file) != 0;
	position = ftell(file);
	observation->terminal_position = position >= 0 ? position : 0;
	if (observation->carry) {
		observation->dos_error = 1U;
		observation->mapped_error = 57U;
	}
	return true;
}

enum startup_configuration_event {
	STARTUP_CONFIGURATION_CLOSE = 1,
	STARTUP_CONFIGURATION_OPEN,
	STARTUP_CONFIGURATION_LOAD,
	STARTUP_CONFIGURATION_STORE_CONFIG,
	STARTUP_CONFIGURATION_READ_PLAYER,
	STARTUP_CONFIGURATION_WRITE_PLAYER,
	STARTUP_CONFIGURATION_RANDOM,
};

struct startup_configuration_tape {
	int events[16];
	int records[16];
	size_t event_count;
	size_t fail_at;
	struct yt_record config_source;
	struct yt_record player_source[8];
	struct yt_config config_write;
	struct yt_player player_write[8];
	bool wrote_config;
	bool wrote_player[8];
	float draws[2];
	size_t draw_position;
};

static bool
startup_configuration_step(struct startup_configuration_tape *tape,
    enum startup_configuration_event event, int record)
{
	if (tape->event_count >= YT_ARRAY_LEN(tape->events))
		return false;
	tape->events[tape->event_count] = (int)event;
	tape->records[tape->event_count] = record;
	++tape->event_count;
	return tape->event_count != tape->fail_at;
}

static bool
startup_configuration_close_test(void *context, struct yt_error *error)
{
	(void)error;
	return startup_configuration_step(context, STARTUP_CONFIGURATION_CLOSE,
	    0);
}

static bool
startup_configuration_open_test(void *context, struct yt_error *error)
{
	(void)error;
	return startup_configuration_step(context, STARTUP_CONFIGURATION_OPEN,
	    0);
}

static bool
startup_configuration_load_test(void *context, struct yt_config *config,
    struct yt_error *error)
{
	struct startup_configuration_tape *tape = context;

	if (!startup_configuration_step(tape, STARTUP_CONFIGURATION_LOAD, 1))
		return false;
	return yt_config_decode(config, &tape->config_source, error);
}

static bool
startup_configuration_store_test(void *context,
    const struct yt_config *config, struct yt_error *error)
{
	struct startup_configuration_tape *tape = context;

	(void)error;
	if (!startup_configuration_step(tape,
	    STARTUP_CONFIGURATION_STORE_CONFIG, 1))
		return false;
	tape->config_write = *config;
	tape->wrote_config = true;
	return true;
}

static bool
startup_configuration_read_test(void *context, int basic,
    struct yt_player *player, struct yt_error *error)
{
	struct startup_configuration_tape *tape = context;

	(void)error;
	if (!startup_configuration_step(tape,
	    STARTUP_CONFIGURATION_READ_PLAYER, basic))
		return false;
	if (basic < 0 || (size_t)basic >= YT_ARRAY_LEN(tape->player_source))
		return false;
	yt_player_decode(player, &tape->player_source[basic]);
	return true;
}

static bool
startup_configuration_write_test(void *context, int basic,
    const struct yt_player *player, struct yt_error *error)
{
	struct startup_configuration_tape *tape = context;

	(void)error;
	if (!startup_configuration_step(tape,
	    STARTUP_CONFIGURATION_WRITE_PLAYER, basic))
		return false;
	if (basic < 0 || (size_t)basic >= YT_ARRAY_LEN(tape->player_write))
		return false;
	tape->player_write[basic] = *player;
	tape->wrote_player[basic] = true;
	return true;
}

static bool
startup_configuration_random_test(void *context, float *value,
    struct yt_error *error)
{
	struct startup_configuration_tape *tape = context;

	(void)error;
	if (!startup_configuration_step(tape, STARTUP_CONFIGURATION_RANDOM, 0))
		return false;
	if (tape->draw_position >= YT_ARRAY_LEN(tape->draws))
		return false;
	*value = tape->draws[tape->draw_position++];
	return true;
}

static bool
startup_configuration_fixture(struct startup_configuration_tape *tape,
    struct yt_startup_configuration_state *state, struct yt_config *config,
    float sector_cache[8], float cloak_cache[8])
{
	size_t index;

	memset(tape, 0, sizeof(*tape));
	memset(state, 0, sizeof(*state));
	memset(config, 0, sizeof(*config));
	for (index = 0U; index < 8U; ++index) {
		sector_cache[index] = -100.0f - (float)index;
		cloak_cache[index] = -200.0f - (float)index;
	}
	tape->fail_at = SIZE_MAX;
	memset(tape->config_source.bytes, 0x5a,
	    sizeof(tape->config_source.bytes));
	memcpy(tape->config_source.bytes, "ab{", 3U);
	if (!yt_record_set_number(&tape->config_source, YT_F41, 3.0f)
	    || !yt_record_set_number(&tape->config_source, YT_F45, 26.0f)
	    || !yt_record_set_number(&tape->config_source, YT_F49, 99.0f)
	    || !yt_record_set_number(&tape->config_source, YT_F53, 4.5f)
	    || !yt_record_set_number(&tape->config_source, YT_F57, 10.75f)
	    || !yt_record_set_number(&tape->config_source, YT_F61, 20.0f)
	    || !yt_record_set_number(&tape->config_source, YT_F85, -0.5f)
	    || !yt_record_set_number(&tape->config_source, YT_F93, 99.0f)
	    || !yt_record_set_number(&tape->config_source, YT_F101, -0.25f)
	    || !yt_record_set_number(&tape->config_source, YT_F105, 19.0f)
	    || !yt_record_set_number(&tape->config_source, YT_F117, 0.0f)
	    || !yt_record_set_number(&tape->config_source, YT_F121, 1001.0f)
	    || !yt_record_set_number(&tape->config_source, YT_F129, 0.0f))
		return false;
	for (index = 2U; index <= 4U; ++index) {
		memset(tape->player_source[index].bytes, (int)(0x20U + index),
		    sizeof(tape->player_source[index].bytes));
		if (!yt_record_set_number(&tape->player_source[index], YT_F57,
		    (float)(index * 10U)))
			return false;
	}
	if (!yt_record_set_number(&tape->player_source[2], YT_F125, -0.25f)
	    || !yt_record_set_number(&tape->player_source[3], YT_F125, 0.5f)
	    || !yt_record_set_number(&tape->player_source[4], YT_F125, 1.25f))
		return false;
	tape->draws[0] = 0.25f;
	tape->draws[1] = 0.75f;
	state->config = config;
	state->local_mode = 0.5f;
	state->sector_cache = sector_cache;
	state->cloak_cache = cloak_cache;
	state->cache_count = 8U;
	return true;
}

static bool
check_startup_configuration_transaction(void)
{
	static const struct yt_startup_configuration_ops ops = {
		startup_configuration_close_test,
		startup_configuration_open_test,
		startup_configuration_load_test,
		startup_configuration_store_test,
		startup_configuration_read_test,
		startup_configuration_write_test,
		startup_configuration_random_test,
	};
	static const int events[] = {
		STARTUP_CONFIGURATION_CLOSE,
		STARTUP_CONFIGURATION_OPEN,
		STARTUP_CONFIGURATION_LOAD,
		STARTUP_CONFIGURATION_STORE_CONFIG,
		STARTUP_CONFIGURATION_READ_PLAYER,
		STARTUP_CONFIGURATION_WRITE_PLAYER,
		STARTUP_CONFIGURATION_READ_PLAYER,
		STARTUP_CONFIGURATION_READ_PLAYER,
		STARTUP_CONFIGURATION_WRITE_PLAYER,
		STARTUP_CONFIGURATION_RANDOM,
		STARTUP_CONFIGURATION_RANDOM,
	};
	static const int records[] = {0, 0, 1, 1, 2, 2, 3, 4, 4, 0, 0};
	struct startup_configuration_tape tape;
	struct yt_startup_configuration_state state;
	struct yt_config config;
	struct yt_record expected;
	struct yt_error error;
	float sector_cache[8];
	float cloak_cache[8];
	size_t failure;

	if (!startup_configuration_fixture(&tape, &state, &config,
	    sector_cache, cloak_cache)
	    || !yt_startup_configuration_run(&state, &ops, &tape, NULL)
	    || !state.handler_installed || state.installed_handler != 0x45F7U
	    || tape.event_count != YT_ARRAY_LEN(events)
	    || memcmp(tape.events, events, sizeof(events)) != 0
	    || memcmp(tape.records, records, sizeof(records)) != 0
	    || state.scoreboard_path_length != 3U
	    || memcmp(config.scoreboard, "AB[", 3U) != 0
	    || config.scoreboard[3] != '\0'
	    || config.headquarters != 85.0f || config.genesis_ports != 200.0f
	    || config.local_screen != -1.0f || config.lottery_plays != 3.0f
	    || config.maximum_planets != 100.0f
	    || config.maximum_holds != 1000.0f
	    || config.turns_per_day != 500.0f || state.cache_guard != 1.0f
	    || sector_cache[2] != 20.0f || sector_cache[3] != 30.0f
	    || sector_cache[4] != 40.0f || sector_cache[1] != -101.0f
	    || cloak_cache[2] != 1.0f || cloak_cache[3] != 0.5f
	    || cloak_cache[4] != 1.0f || cloak_cache[1] != -201.0f
	    || state.black_hole[0] != 3.0f || state.black_hole[1] != 5.0f
	    || tape.draw_position != 2U || !tape.wrote_config
	    || !tape.wrote_player[2] || tape.wrote_player[3]
	    || !tape.wrote_player[4])
		return false;
	expected = tape.config_source;
	if (!yt_record_set_number(&expected, YT_F117, 85.0f)
	    || memcmp(&tape.config_write.record, &expected, sizeof(expected)) != 0)
		return false;
	expected = tape.player_source[2];
	if (!yt_record_set_number(&expected, YT_F125, 1.0f)
	    || memcmp(&tape.player_write[2].record, &expected,
	    sizeof(expected)) != 0)
		return false;
	expected = tape.player_source[4];
	if (!yt_record_set_number(&expected, YT_F125, 1.0f)
	    || memcmp(&tape.player_write[4].record, &expected,
	    sizeof(expected)) != 0)
		return false;

	/* A failed Headquarters PUT retains FIELD bytes but not the later global. */
	if (!startup_configuration_fixture(&tape, &state, &config,
	    sector_cache, cloak_cache))
		return false;
	tape.fail_at = 4U;
	if (yt_startup_configuration_run(&state, &ops, &tape, NULL)
	    || tape.event_count != 4U || config.headquarters != 0.0f
	    || !state.handler_installed || state.installed_handler != 0x45F7U
	    || yt_record_get_number(&config.record, YT_F117) != 85.0f)
		return false;

	/* A nonzero one-shot guard skips all player I/O but not either draw. */
	if (!startup_configuration_fixture(&tape, &state, &config,
	    sector_cache, cloak_cache))
		return false;
	state.cache_guard = -0.25f;
	if (!yt_record_set_number(&tape.config_source, YT_F117, 7.0f)
	    || !yt_startup_configuration_run(&state, &ops, &tape, NULL)
	    || tape.event_count != 5U
	    || tape.events[0] != STARTUP_CONFIGURATION_CLOSE
	    || tape.events[1] != STARTUP_CONFIGURATION_OPEN
	    || tape.events[2] != STARTUP_CONFIGURATION_LOAD
	    || tape.events[3] != STARTUP_CONFIGURATION_RANDOM
	    || tape.events[4] != STARTUP_CONFIGURATION_RANDOM
	    || state.cache_guard != -0.25f || tape.draw_position != 2U)
		return false;

	/* Descriptor length, not an embedded NUL byte, controls path emptiness. */
	if (!startup_configuration_fixture(&tape, &state, &config,
	    sector_cache, cloak_cache))
		return false;
	state.cache_guard = 1.0f;
	tape.config_source.bytes[0] = 'a';
	tape.config_source.bytes[1] = 0U;
	tape.config_source.bytes[2] = 'b';
	if (!yt_record_set_number(&tape.config_source, YT_F117, 7.0f)
	    || !yt_startup_configuration_run(&state, &ops, &tape, NULL)
	    || state.scoreboard_path_length != 3U
	    || config.scoreboard[0] != 'A' || config.scoreboard[1] != 0
	    || config.scoreboard[2] != 'B')
		return false;

	/* A genuinely empty descriptor receives the lowercase compiled default. */
	if (!startup_configuration_fixture(&tape, &state, &config,
	    sector_cache, cloak_cache))
		return false;
	state.cache_guard = 1.0f;
	if (!yt_record_set_number(&tape.config_source, YT_F41, 0.0f)
	    || !yt_record_set_number(&tape.config_source, YT_F117, 7.0f)
	    || !yt_startup_configuration_run(&state, &ops, &tape, NULL)
	    || state.scoreboard_path_length != 11U
	    || memcmp(config.scoreboard, "ytscore.asc", 12U) != 0)
		return false;

	/* The initial FOR test admits no GET when the raw terminal is below two. */
	if (!startup_configuration_fixture(&tape, &state, &config,
	    sector_cache, cloak_cache))
		return false;
	if (!yt_record_set_number(&tape.config_source, YT_F53, 1.75f)
	    || !yt_record_set_number(&tape.config_source, YT_F117, 7.0f)
	    || !yt_startup_configuration_run(&state, &ops, &tape, NULL)
	    || tape.event_count != 5U || state.cache_guard != 1.0f
	    || tape.draw_position != 2U)
		return false;

	/* LEFT$ clamps a positive count to its 41-byte FIELD source. */
	if (!startup_configuration_fixture(&tape, &state, &config,
	    sector_cache, cloak_cache))
		return false;
	state.cache_guard = 1.0f;
	if (!yt_record_set_number(&tape.config_source, YT_F41, 45.0f)
	    || !yt_record_set_number(&tape.config_source, YT_F117, 7.0f)
	    || !yt_startup_configuration_run(&state, &ops, &tape, NULL)
	    || state.scoreboard_path_length != YT_TEXT_FIELD_SIZE)
		return false;

	/* CINT overflow stops after earlier HQ/Genesis/path mutations. */
	if (!startup_configuration_fixture(&tape, &state, &config,
	    sector_cache, cloak_cache))
		return false;
	state.local_mode = 40000.0f;
	yt_error_clear(&error);
	if (yt_startup_configuration_run(&state, &ops, &tape, &error)
	    || error.status != YT_RANGE
	    || strcmp(error.operation, "startup local-mode CINT") != 0
	    || tape.event_count != 4U || !tape.wrote_config
	    || config.headquarters != 85.0f || config.genesis_ports != 200.0f
	    || config.lottery_plays != -0.25f || tape.draw_position != 0U)
		return false;

	for (failure = 1U; failure <= YT_ARRAY_LEN(events); ++failure) {
		if (!startup_configuration_fixture(&tape, &state, &config,
		    sector_cache, cloak_cache))
			return false;
		tape.fail_at = failure;
		if (yt_startup_configuration_run(&state, &ops, &tape, NULL)
		    || tape.event_count != failure
		    || !state.handler_installed
		    || state.installed_handler != 0x45F7U
		    || memcmp(tape.events, events,
		    failure * sizeof(events[0])) != 0)
			return false;
	}
	if (!startup_configuration_fixture(&tape, &state, &config,
	    sector_cache, cloak_cache))
		return false;
	tape.fail_at = 6U;
	if (yt_startup_configuration_run(&state, &ops, &tape, NULL)
	    || sector_cache[2] != 20.0f || cloak_cache[2] != 1.0f
	    || state.cache_guard != 0.0f || tape.draw_position != 0U)
		return false;
	if (!startup_configuration_fixture(&tape, &state, &config,
	    sector_cache, cloak_cache))
		return false;
	tape.fail_at = 11U;
	if (yt_startup_configuration_run(&state, &ops, &tape, NULL)
	    || state.cache_guard != 1.0f || state.black_hole[0] != 3.0f
	    || state.black_hole[1] != 0.0f || tape.draw_position != 1U)
		return false;
	return !yt_startup_configuration_run(NULL, &ops, &tape, NULL)
	    && !yt_startup_configuration_run(&state, NULL, &tape, NULL);
}

struct hydration_tape {
	struct yt_player fresh;
	int requested_record;
	unsigned calls;
	bool succeeds;
};

static bool
hydration_read(void *context, int player_record, struct yt_player *player,
    struct yt_error *error)
{
	struct hydration_tape *tape = context;

	(void)error;
	++tape->calls;
	tape->requested_record = player_record;
	if (!tape->succeeds)
		return false;
	*player = tape->fresh;
	return true;
}

static bool
check_current_player_cache_model(void)
{
	struct yt_current_player_hydration_state state;
	struct hydration_tape tape;
	struct yt_player player;
	struct yt_player before;
	float sector_cache[5] = {-1.0f, -2.0f, -3.0f, -4.0f, -5.0f};
	float cloak_cache[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
	float current_sector = -1.0f;
	struct yt_error error;

	memset(&player, 0, sizeof(player));
	memset(&tape, 0, sizeof(tape));
	(void)snprintf(player.name, sizeof(player.name), "%s", "Cached Name");
	player.name_length = 11.0f;
	player.last_active = 71.0f;
	player.killed_by = 72.0f;
	player.lottery_plays = 73.0f;
	memset(&tape.fresh, 0, sizeof(tape.fresh));
	(void)snprintf(tape.fresh.name, sizeof(tape.fresh.name), "%s", "Field Name");
	tape.fresh.name_length = 10.0f;
	tape.fresh.last_active = 1.0f;
	tape.fresh.killed_by = 2.0f;
	tape.fresh.lottery_plays = 3.0f;
	tape.fresh.turns = 4.0f;
	tape.fresh.shields = 5.0f;
	tape.fresh.sector = 6.0f;
	tape.fresh.fighters = 7.0f;
	tape.fresh.holds = 8.0f;
	tape.fresh.ore = 9.0f;
	tape.fresh.organics = 10.0f;
	tape.fresh.equipment = 11.0f;
	tape.fresh.credits = 12.0f;
	tape.fresh.team = 13.0f;
	tape.fresh.danger_scanner = 14.0f;
	tape.fresh.missiles = 15.0f;
	tape.fresh.score = 16.0f;
	tape.fresh.plasma = 17.0f;
	tape.fresh.ports_owned = 18.0f;
	tape.fresh.ground_forces = 19.0f;
	tape.fresh.cloak = 20.0f;
	tape.fresh.mines = 21.0f;
	yt_player_encode(&tape.fresh);
	tape.fresh.record.bytes[YT_RECORD_TAIL_OFFSET] = 0x7f;
	tape.succeeds = true;
	state.player = &player;
	state.player_record = 2;
	state.last_player_record = 51;
	state.sector_record_offset = 51.0f;
	state.current_sector_record = &current_sector;
	state.sector_cache = sector_cache;
	state.cloak_cache = cloak_cache;
	state.cache_count = YT_ARRAY_LEN(sector_cache);
	state.anti_cloak = false;
	if (!yt_current_player_hydrate_run(&state, hydration_read, &tape, NULL)
	    || tape.calls != 1U || tape.requested_record != 2
	    || strcmp(player.name, "Cached Name") != 0
	    || player.name_length != 11.0f || player.last_active != 71.0f
	    || player.killed_by != 72.0f || player.lottery_plays != 73.0f
	    || player.turns != 4.0f || player.shields != 5.0f
	    || player.sector != 6.0f || player.fighters != 7.0f
	    || player.holds != 8.0f || player.ore != 9.0f
	    || player.organics != 10.0f || player.equipment != 11.0f
	    || player.credits != 12.0f || player.team != 13.0f
	    || player.danger_scanner != 14.0f || player.missiles != 15.0f
	    || player.score != 16.0f || player.plasma != 17.0f
	    || player.ports_owned != 18.0f || player.ground_forces != 19.0f
	    || player.cloak != 20.0f || player.mines != 21.0f
	    || memcmp(&player.record, &tape.fresh.record,
	    sizeof(player.record)) != 0
	    || current_sector != 57.0f || sector_cache[2] != 6.0f
	    || cloak_cache[2] != 20.0f || sector_cache[1] != -2.0f
	    || cloak_cache[1] != 2.0f)
		return false;

	tape.fresh.sector = 22.0f;
	tape.fresh.cloak = 23.0f;
	yt_player_encode(&tape.fresh);
	state.anti_cloak = true;
	if (!yt_current_player_hydrate_run(&state, hydration_read, &tape, NULL)
	    || sector_cache[2] != 22.0f || cloak_cache[2] != 20.0f
	    || current_sector != 73.0f)
		return false;

	before = player;
	tape.succeeds = false;
	current_sector = 99.0f;
	if (yt_current_player_hydrate_run(&state, hydration_read, &tape, NULL)
	    || memcmp(&player, &before, sizeof(player)) != 0
	    || current_sector != 99.0f)
		return false;
	before = player;
	state.player_record = 52;
	tape.calls = 0U;
	yt_error_clear(&error);
	return !yt_current_player_hydrate_run(&state, hydration_read, &tape,
	    &error) && error.status == YT_RANGE && tape.calls == 0U
	    && memcmp(&player, &before, sizeof(player)) == 0;
}

struct friendship_reader_tape {
	int records[2];
	float teams[2];
	size_t count;
	int fail_call;
};

static bool
friendship_reader(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct friendship_reader_tape *tape = context;
	size_t call = tape->count++;

	if (call >= YT_ARRAY_LEN(tape->records))
		return false;
	tape->records[call] = player_record;
	if ((int)call == tape->fail_call) {
		if (error != NULL) {
			error->status = YT_IO_ERROR;
			(void)snprintf(error->operation,
			    sizeof(error->operation), "%s", "friendship GET");
		}
		return false;
	}
	memset(player, 0, sizeof(*player));
	player->team = tape->teams[call];
	return true;
}

static bool
check_friendship_model(void)
{
	struct friendship_reader_tape tape;
	struct yt_error error;
	bool friendly;

	memset(&tape, 0, sizeof(tape));
	tape.fail_call = -1;
	if (!yt_friendship_resolve(1.0f, 2.0f, 51.0f,
	    friendship_reader, &tape, &friendly, &error)
	    || friendly || tape.count != 0U
	    || !yt_friendship_resolve(52.0f, 2.0f, 51.0f,
	    friendship_reader, &tape, &friendly, &error)
	    || friendly || tape.count != 0U
	    || !yt_friendship_resolve(2.0f, 52.0f, 51.0f,
	    friendship_reader, &tape, &friendly, &error)
	    || friendly || tape.count != 0U
	    || !yt_friendship_resolve(2.0f, 2.0f, 51.0f,
	    friendship_reader, &tape, &friendly, &error)
	    || !friendly || tape.count != 0U)
		return false;

	memset(&tape, 0, sizeof(tape));
	tape.fail_call = -1;
	tape.teams[0] = 0.0f;
	if (!yt_friendship_resolve(3.0f, 2.0f, 51.0f,
	    friendship_reader, &tape, &friendly, &error)
	    || friendly || tape.count != 1U || tape.records[0] != 2)
		return false;

	memset(&tape, 0, sizeof(tape));
	tape.fail_call = -1;
	tape.teams[0] = 7.0f;
	tape.teams[1] = 7.0f;
	if (!yt_friendship_resolve(3.75f, 2.25f, 51.0f,
	    friendship_reader, &tape, &friendly, &error)
	    || !friendly || tape.count != 2U
	    || tape.records[0] != 2 || tape.records[1] != 3)
		return false;

	memset(&tape, 0, sizeof(tape));
	tape.fail_call = -1;
	tape.teams[0] = -3.0f;
	tape.teams[1] = -3.0f;
	if (!yt_friendship_resolve(51.0f, 2.0f, 51.0f,
	    friendship_reader, &tape, &friendly, &error)
	    || !friendly || tape.count != 2U
	    || tape.records[0] != 2 || tape.records[1] != 51)
		return false;
	tape.count = 0U;
	tape.teams[1] = 4.0f;
	if (!yt_friendship_resolve(3.0f, 2.0f, 51.0f,
	    friendship_reader, &tape, &friendly, &error)
	    || friendly || tape.count != 2U)
		return false;

	memset(&tape, 0, sizeof(tape));
	tape.fail_call = 0;
	yt_error_clear(&error);
	if (yt_friendship_resolve(3.0f, 2.0f, 51.0f,
	    friendship_reader, &tape, &friendly, &error)
	    || friendly || tape.count != 1U || tape.records[0] != 2
	    || error.status != YT_IO_ERROR)
		return false;

	memset(&tape, 0, sizeof(tape));
	tape.fail_call = 1;
	tape.teams[0] = 1.0f;
	yt_error_clear(&error);
	if (yt_friendship_resolve(3.0f, 2.0f, 51.0f,
	    friendship_reader, &tape, &friendly, &error)
	    || friendly || tape.count != 2U
	    || tape.records[0] != 2 || tape.records[1] != 3
	    || error.status != YT_IO_ERROR)
		return false;
	return true;
}

static bool
check_team_loader_model(void)
{
	static const size_t roster_offsets[4] = {
		YT_F109, YT_F117, YT_F121, YT_F125
	};
	static const uint8_t live_name[] = {'A', 0, 'B'};
	struct yt_team_loader_cache cache;
	struct yt_record overlay;
	struct yt_error error;
	enum yt_team_loader_route route;
	bool needs_overlay;
	size_t index;

	memset(&cache, 0, sizeof(cache));
	memcpy(cache.name, "STALE", 6U);
	cache.name_length = 5U;
	memcpy(cache.password, "OLD!", 5U);
	cache.captain = 17.0f;
	cache.captain_flag = -1.0f;
	for (index = 0; index < YT_ARRAY_LEN(cache.roster); ++index)
		cache.roster[index] = (float)(index + 1U);
	yt_team_loader_begin(0.0f, &cache, &needs_overlay);
	if (needs_overlay || cache.available != -1.0f
	    || cache.counter != 5.0f || cache.name_length != 5U
	    || memcmp(cache.name, "STALE", 6U) != 0
	    || memcmp(cache.password, "OLD!", 5U) != 0
	    || cache.captain != 17.0f || cache.captain_flag != -1.0f)
		return false;
	for (index = 0; index < YT_ARRAY_LEN(cache.roster); ++index)
		if (cache.roster[index] != 0.0f)
			return false;

	yt_record_blank(&overlay);
	yt_record_set_text(&overlay, (const uint8_t *)"DEAD", 4U);
	(void)yt_record_set_number(&overlay, YT_F73, 4.0f);
	(void)yt_record_set_number(&overlay, YT_F77, 99.0f);
	memcpy(overlay.bytes + YT_F113, "NEW!", 4U);
	yt_team_loader_begin(1.0f, &cache, &needs_overlay);
	yt_error_clear(&error);
	if (!needs_overlay
	    || !yt_team_loader_finish(&overlay, 99.0f, 0, &cache,
	    &route, &error)
	    || route != YT_TEAM_LOADER_ROSTER_DEAD
	    || cache.available != -1.0f || cache.name_length != 5U
	    || memcmp(cache.name, "STALE", 6U) != 0
	    || memcmp(cache.password, "OLD!", 5U) != 0
	    || cache.captain != 17.0f || cache.captain_flag != -1.0f)
		return false;

	yt_record_blank(&overlay);
	yt_record_set_text(&overlay, live_name, sizeof(live_name));
	(void)yt_record_set_number(&overlay, YT_F73, 3.0f);
	(void)yt_record_set_number(&overlay, YT_F77, 7.0f);
	memcpy(overlay.bytes + YT_F113, "PASS", 4U);
	(void)yt_record_set_number(&overlay, roster_offsets[0], 2.0f);
	(void)yt_record_set_number(&overlay, roster_offsets[1], 0.0f);
	(void)yt_record_set_number(&overlay, roster_offsets[2], -1.0f);
	(void)yt_record_set_number(&overlay, roster_offsets[3], 4.0f);
	yt_team_loader_begin(50.0f, &cache, &needs_overlay);
	if (!needs_overlay
	    || !yt_team_loader_finish(&overlay, 7.0f, 0, &cache,
	    &route, &error)
	    || route != YT_TEAM_LOADER_LIVE || cache.available != 0.0f
	    || cache.name_length != sizeof(live_name)
	    || memcmp(cache.name, live_name, sizeof(live_name)) != 0
	    || memcmp(cache.password, "PASS", 4U) != 0
	    || cache.captain != 7.0f || cache.captain_flag != -1.0f
	    || cache.roster[0] != 2.0f || cache.roster[1] != 0.0f
	    || cache.roster[2] != -1.0f || cache.roster[3] != 4.0f)
		return false;
	(void)yt_record_set_number(&overlay, YT_F77, 8.0f);
	yt_team_loader_begin(50.0f, &cache, &needs_overlay);
	if (!yt_team_loader_finish(&overlay, 7.0f, 0, &cache,
	    &route, &error) || cache.captain_flag != -1.0f)
		return false;

	cache.captain_flag = 0.0f;
	yt_team_loader_begin(50.0001f, &cache, &needs_overlay);
	if (needs_overlay || cache.captain_flag != 0.0f
	    || cache.name_length != sizeof(live_name))
		return false;

	yt_record_blank(&overlay);
	yt_record_set_text(&overlay, (const uint8_t *)"ABCD", 4U);
	(void)yt_record_set_number(&overlay, YT_F73, 2.5f);
	(void)yt_record_set_number(&overlay, YT_F77, 8.0f);
	(void)yt_record_set_number(&overlay, YT_F109, 1.0f);
	yt_team_loader_begin(1.0f, &cache, &needs_overlay);
	if (!yt_team_loader_finish(&overlay, 7.0f, 0, &cache,
	    &route, &error)
	    || cache.name_length != 3U || memcmp(cache.name, "ABC", 3U) != 0
	    || cache.captain_flag != 0.0f)
		return false;
	yt_team_loader_begin(1.0f, &cache, &needs_overlay);
	if (!yt_team_loader_finish(&overlay, 7.0f, 4, &cache,
	    &route, &error)
	    || cache.name_length != 2U || memcmp(cache.name, "AB", 2U) != 0)
		return false;

	(void)yt_record_set_number(&overlay, YT_F73, -1.0f);
	yt_team_loader_begin(1.0f, &cache, &needs_overlay);
	yt_error_clear(&error);
	if (yt_team_loader_finish(&overlay, 7.0f, 0, &cache,
	    &route, &error)
	    || error.status != YT_RANGE || cache.available != 0.0f
	    || cache.name_length != 2U || memcmp(cache.name, "AB", 2U) != 0)
		return false;
	for (index = 0; index < YT_ARRAY_LEN(cache.roster); ++index)
		if (cache.roster[index] != 0.0f)
			return false;
	return true;
}

struct team_loader_read_tape {
	struct yt_record record;
	uint32_t physical_record;
	size_t calls;
	bool fail;
};

static bool
team_loader_read_test(void *context, uint32_t physical_record,
	struct yt_record *record, struct yt_error *error)
{
	struct team_loader_read_tape *tape = context;

	++tape->calls;
	tape->physical_record = physical_record;
	if (tape->fail) {
		if (error != NULL) {
			error->status = YT_IO_ERROR;
			(void)snprintf(error->operation,
			    sizeof(error->operation), "%s",
			    "team loader injected GET failure");
		}
		return false;
	}
	*record = tape->record;
	return true;
}

static bool
check_team_loader_transaction(void)
{
	struct team_loader_read_tape tape;
	struct yt_team_loader_state state;
	struct yt_team_loader_cache cache;
	struct yt_error error;

	memset(&tape, 0, sizeof(tape));
	yt_record_blank(&tape.record);
	(void)yt_record_set_number(&tape.record, YT_F73, 3.0f);
	(void)yt_record_set_number(&tape.record, YT_F77, 2.0f);
	(void)yt_record_set_number(&tape.record, YT_F109, 3.0f);
	memcpy(tape.record.bytes, "A\0B", 3U);
	memset(&cache, 0, sizeof(cache));
	state = (struct yt_team_loader_state){
		.team_id = 1.5f,
		.current_player_record = 2.0f,
		.sector_record_offset = 55.0f,
		.cache = &cache,
	};
	if (!yt_team_loader_run(&state, team_loader_read_test, &tape, NULL)
	    || tape.calls != 1U || tape.physical_record != 56U
	    || state.physical_record != 56U || !state.overlay_loaded
	    || !state.complete || state.route != YT_TEAM_LOADER_LIVE
	    || cache.available != 0.0f || cache.counter != 5.0f
	    || cache.name_length != 3U || memcmp(cache.name, "A\0B", 3U) != 0
	    || cache.captain != 2.0f || cache.captain_flag != -1.0f
	    || cache.roster[0] != 3.0f)
		return false;

	memset(&cache, 0, sizeof(cache));
	cache.roster[0] = 9.0f;
	state = (struct yt_team_loader_state){
		.team_id = 50.0001f,
		.current_player_record = 2.0f,
		.sector_record_offset = 55.0f,
		.cache = &cache,
		.physical_record = 1234U,
	};
	if (!yt_team_loader_run(&state, team_loader_read_test, &tape, NULL)
	    || tape.calls != 1U || state.physical_record != 1234U
	    || state.overlay_loaded || !state.complete
	    || state.route != YT_TEAM_LOADER_OUT_OF_RANGE
	    || cache.available != -1.0f || cache.counter != 5.0f
	    || cache.roster[0] != 0.0f)
		return false;

	memset(&cache, 0, sizeof(cache));
	cache.roster[0] = 9.0f;
	tape.fail = true;
	state = (struct yt_team_loader_state){
		.team_id = 1.5f,
		.current_player_record = 2.0f,
		.sector_record_offset = 55.0f,
		.cache = &cache,
	};
	yt_error_clear(&error);
	if (yt_team_loader_run(&state, team_loader_read_test, &tape, &error)
	    || tape.calls != 2U || tape.physical_record != 56U
	    || state.overlay_loaded || state.complete
	    || state.route != YT_TEAM_LOADER_OUT_OF_RANGE
	    || error.status != YT_IO_ERROR || cache.available != -1.0f
	    || cache.counter != 5.0f || cache.roster[0] != 0.0f)
		return false;
	return true;
}

enum death_team_event {
	DEATH_TEAM_READ_PLAYER = 1,
	DEATH_TEAM_READ_LOADER,
	DEATH_TEAM_READ_PARENT,
	DEATH_TEAM_WRITE_PARENT,
	DEATH_TEAM_WRITE_PLAYER,
};

struct death_team_tape {
	const enum death_team_event *expected;
	size_t expected_count;
	size_t event_count;
	size_t fail_at;
	uint32_t physical[4];
	struct yt_player player;
	struct yt_record overlay;
	struct yt_record written_overlay;
	bool overlay_written;
	bool player_written;
};

static bool
death_team_step(struct death_team_tape *tape, enum death_team_event event,
	struct yt_error *error)
{
	if (tape->event_count >= tape->expected_count
	    || tape->expected[tape->event_count] != event)
		return false;
	++tape->event_count;
	if (tape->event_count != tape->fail_at)
		return true;
	if (error != NULL) {
		error->status = YT_IO_ERROR;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    "death team injected failure");
	}
	return false;
}

static bool
death_team_read_player_test(void *context, int player_record,
	struct yt_player *player, struct yt_error *error)
{
	struct death_team_tape *tape = context;

	if (player_record != 3
	    || !death_team_step(tape, DEATH_TEAM_READ_PLAYER, error))
		return false;
	*player = tape->player;
	return true;
}

static bool
death_team_write_player_test(void *context, int player_record,
	struct yt_player *player, struct yt_error *error)
{
	struct death_team_tape *tape = context;

	if (player_record != 3
	    || !death_team_step(tape, DEATH_TEAM_WRITE_PLAYER, error))
		return false;
	tape->player = *player;
	tape->player_written = true;
	return true;
}

static bool
death_team_read_record_test(void *context, uint32_t physical_record,
	struct yt_record *record, struct yt_error *error)
{
	struct death_team_tape *tape = context;
	enum death_team_event event;

	if (tape->event_count >= tape->expected_count)
		return false;
	event = tape->expected[tape->event_count];
	if (event != DEATH_TEAM_READ_LOADER
	    && event != DEATH_TEAM_READ_PARENT)
		return false;
	if (tape->event_count < YT_ARRAY_LEN(tape->physical))
		tape->physical[tape->event_count] = physical_record;
	if (!death_team_step(tape, event, error))
		return false;
	*record = tape->overlay;
	return true;
}

static bool
death_team_write_record_test(void *context, uint32_t physical_record,
	const struct yt_record *record, struct yt_error *error)
{
	struct death_team_tape *tape = context;

	if (tape->event_count < YT_ARRAY_LEN(tape->physical))
		tape->physical[tape->event_count] = physical_record;
	if (!death_team_step(tape, DEATH_TEAM_WRITE_PARENT, error))
		return false;
	tape->written_overlay = *record;
	tape->overlay = *record;
	tape->overlay_written = true;
	return true;
}

static const struct yt_death_team_remove_ops death_team_ops = {
	death_team_read_player_test,
	death_team_write_player_test,
	death_team_read_record_test,
	death_team_write_record_test,
};

static void
death_team_reset(struct death_team_tape *tape,
	const enum death_team_event *expected, size_t expected_count,
	float team_id, size_t fail_at)
{
	static const size_t roster_offsets[4] = {
		YT_F109, YT_F117, YT_F121, YT_F125
	};
	struct yt_record raw;
	size_t index;

	memset(tape, 0, sizeof(*tape));
	tape->expected = expected;
	tape->expected_count = expected_count;
	tape->fail_at = fail_at;
	for (index = 0U; index < YT_RECORD_SIZE; ++index)
		raw.bytes[index] = (uint8_t)(index ^ 0x69U);
	(void)yt_record_set_number(&raw, YT_F89, team_id);
	yt_player_decode(&tape->player, &raw);
	for (index = 0U; index < YT_RECORD_SIZE; ++index)
		tape->overlay.bytes[index] = (uint8_t)(index ^ 0xb4U);
	(void)yt_record_set_number(&tape->overlay, YT_F73, 4.0f);
	(void)yt_record_set_number(&tape->overlay, YT_F77, 2.0f);
	for (index = 0U; index < YT_ARRAY_LEN(roster_offsets); ++index)
		(void)yt_record_set_number(&tape->overlay,
		    roster_offsets[index], index == 2U ? 8.0f : 3.0f);
}

static bool
check_death_team_remove_transaction(void)
{
	static const enum death_team_event live_events[] = {
		DEATH_TEAM_READ_PLAYER,
		DEATH_TEAM_READ_LOADER,
		DEATH_TEAM_READ_PARENT,
		DEATH_TEAM_WRITE_PARENT,
		DEATH_TEAM_READ_PLAYER,
		DEATH_TEAM_WRITE_PLAYER,
	};
	static const enum death_team_event rejected_events[] = {
		DEATH_TEAM_READ_PLAYER,
		DEATH_TEAM_READ_PARENT,
		DEATH_TEAM_WRITE_PARENT,
		DEATH_TEAM_READ_PLAYER,
		DEATH_TEAM_WRITE_PLAYER,
	};
	static const enum death_team_event zero_events[] = {
		DEATH_TEAM_READ_PLAYER,
	};
	static const size_t roster_offsets[4] = {
		YT_F109, YT_F117, YT_F121, YT_F125
	};
	struct death_team_tape tape;
	struct yt_death_team_remove_state state;
	struct yt_team_loader_cache cache;
	struct yt_record original_overlay;
	struct yt_error error;
	size_t index;

	memset(&cache, 0, sizeof(cache));
	memcpy(cache.name, "STALE", 6U);
	cache.name_length = 5U;
	death_team_reset(&tape, live_events, YT_ARRAY_LEN(live_events),
	    1.5f, 0U);
	original_overlay = tape.overlay;
	state = (struct yt_death_team_remove_state){
		.victim_record = 3,
		.current_player_record = 2.0f,
		.sector_record_offset = 55.0f,
		.cache = &cache,
	};
	if (!yt_death_team_remove_run(&state, &death_team_ops, &tape, NULL)
	    || tape.event_count != YT_ARRAY_LEN(live_events)
	    || !tape.overlay_written || !tape.player_written || !state.complete
	    || state.raw_team_id != 1.5f
	    || state.loader_route != YT_TEAM_LOADER_LIVE
	    || state.overlay_physical_record != 56U
	    || tape.physical[1] != 56U || tape.physical[2] != 56U
	    || tape.physical[3] != 56U || tape.player.team != 0.0f
	    || yt_record_get_number(&tape.player.record, YT_F89) != 0.0f
	    || cache.roster[0] != 0.0f || cache.roster[1] != 0.0f
	    || cache.roster[2] != 8.0f || cache.roster[3] != 0.0f)
		return false;
	for (index = 0U; index < YT_ARRAY_LEN(roster_offsets); ++index)
		if (yt_record_get_number(&tape.written_overlay,
		    roster_offsets[index]) != cache.roster[index])
			return false;
	for (index = 0U; index < YT_RECORD_SIZE; ++index) {
		bool roster_byte = false;
		size_t roster;

		for (roster = 0U; roster < YT_ARRAY_LEN(roster_offsets); ++roster)
			if (index >= roster_offsets[roster]
			    && index < roster_offsets[roster] + 4U)
				roster_byte = true;
		if (!roster_byte && tape.written_overlay.bytes[index]
		    != original_overlay.bytes[index])
			return false;
	}

	for (index = 1U; index <= YT_ARRAY_LEN(live_events); ++index) {
		memset(&cache, 0, sizeof(cache));
		cache.roster[0] = 41.0f;
		death_team_reset(&tape, live_events, YT_ARRAY_LEN(live_events),
		    1.5f, index);
		state = (struct yt_death_team_remove_state){
			.victim_record = 3,
			.current_player_record = 2.0f,
			.sector_record_offset = 55.0f,
			.cache = &cache,
		};
		yt_error_clear(&error);
		if (yt_death_team_remove_run(&state, &death_team_ops, &tape,
		    &error) || tape.event_count != index || state.complete
		    || error.status != YT_IO_ERROR
		    || (index == 1U && cache.roster[0] != 41.0f)
		    || (index > 1U && cache.roster[0] != 0.0f)
		    || (index <= 4U && tape.overlay_written)
		    || (index > 4U && !tape.overlay_written)
		    || tape.player_written)
			return false;
	}

	memset(&cache, 0, sizeof(cache));
	cache.roster[0] = 9.0f;
	death_team_reset(&tape, rejected_events,
	    YT_ARRAY_LEN(rejected_events), 50.0001f, 0U);
	state = (struct yt_death_team_remove_state){
		.victim_record = 3,
		.current_player_record = 2.0f,
		.sector_record_offset = 55.0f,
		.cache = &cache,
	};
	if (!yt_death_team_remove_run(&state, &death_team_ops, &tape, NULL)
	    || tape.event_count != YT_ARRAY_LEN(rejected_events)
	    || state.loader_route != YT_TEAM_LOADER_OUT_OF_RANGE
	    || state.overlay_physical_record != 105U
	    || tape.physical[1] != 105U || tape.physical[2] != 105U)
		return false;
	for (index = 0U; index < YT_ARRAY_LEN(cache.roster); ++index)
		if (cache.roster[index] != 0.0f)
			return false;

	memset(&cache, 0, sizeof(cache));
	cache.roster[0] = 9.0f;
	death_team_reset(&tape, rejected_events,
	    YT_ARRAY_LEN(rejected_events), 0.5f, 0U);
	state = (struct yt_death_team_remove_state){
		.victim_record = 3,
		.current_player_record = 2.0f,
		.sector_record_offset = 55.0f,
		.cache = &cache,
	};
	if (!yt_death_team_remove_run(&state, &death_team_ops, &tape, NULL)
	    || state.overlay_physical_record != 55U || !tape.overlay_written
	    || !tape.player_written)
		return false;

	memset(&cache, 0, sizeof(cache));
	cache.roster[0] = 9.0f;
	death_team_reset(&tape, zero_events, YT_ARRAY_LEN(zero_events),
	    -0.0f, 0U);
	state = (struct yt_death_team_remove_state){
		.victim_record = 3,
		.current_player_record = 2.0f,
		.sector_record_offset = 55.0f,
		.cache = &cache,
	};
	if (!yt_death_team_remove_run(&state, &death_team_ops, &tape, NULL)
	    || tape.event_count != 1U || !state.complete
	    || tape.overlay_written || tape.player_written
	    || cache.roster[0] != 9.0f)
		return false;
	return true;
}

enum info_team_event {
	INFO_TEAM_READ_PLAYER = 1,
	INFO_TEAM_LOAD,
	INFO_TEAM_READ_OVERLAY,
	INFO_TEAM_WRITE_OVERLAY,
	INFO_TEAM_PRESENT,
};

struct info_team_tape {
	int events[16];
	size_t event_count;
	size_t fail_at;
	struct yt_player players[3];
	float player_records[3];
	size_t player_position;
	struct yt_team team;
	float captain_flag;
	float loaded_team_id;
	float loaded_current_record;
	struct yt_sector overlay;
	struct yt_sector written;
	float overlay_read_team;
	float overlay_write_team;
	uint8_t rows[5][256];
	size_t row_lengths[5];
	size_t row_count;
};

static bool
info_team_step(struct info_team_tape *tape, enum info_team_event event)
{
	if (tape->event_count >= YT_ARRAY_LEN(tape->events))
		return false;
	tape->events[tape->event_count++] = (int)event;
	return tape->event_count != tape->fail_at;
}

static bool
info_team_read_player(void *context, float record, struct yt_player *player,
    struct yt_error *error)
{
	struct info_team_tape *tape = context;
	size_t position = tape->player_position;

	(void)error;
	if (position >= YT_ARRAY_LEN(tape->players)
	    || !info_team_step(tape, INFO_TEAM_READ_PLAYER))
		return false;
	tape->player_records[position] = record;
	*player = tape->players[position];
	tape->player_position++;
	return true;
}

static bool
info_team_load(void *context, float team_id, float current_record,
    float *captain_flag, struct yt_team *team, struct yt_error *error)
{
	struct info_team_tape *tape = context;

	(void)error;
	if (!info_team_step(tape, INFO_TEAM_LOAD))
		return false;
	tape->loaded_team_id = team_id;
	tape->loaded_current_record = current_record;
	*captain_flag = tape->captain_flag;
	*team = tape->team;
	return true;
}

static bool
info_team_read_overlay(void *context, float team_id,
    struct yt_sector *overlay, struct yt_error *error)
{
	struct info_team_tape *tape = context;

	(void)error;
	if (!info_team_step(tape, INFO_TEAM_READ_OVERLAY))
		return false;
	tape->overlay_read_team = team_id;
	*overlay = tape->overlay;
	return true;
}

static bool
info_team_write_overlay(void *context, float team_id,
    const struct yt_sector *overlay, struct yt_error *error)
{
	struct info_team_tape *tape = context;

	(void)error;
	if (!info_team_step(tape, INFO_TEAM_WRITE_OVERLAY))
		return false;
	tape->overlay_write_team = team_id;
	tape->written = *overlay;
	return true;
}

static bool
info_team_present(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	struct info_team_tape *tape = context;
	size_t position = tape->row_count;

	(void)error;
	if (position >= YT_ARRAY_LEN(tape->rows)
	    || length > sizeof(tape->rows[position])
	    || !info_team_step(tape, INFO_TEAM_PRESENT))
		return false;
	if (length != 0U)
		memcpy(tape->rows[position], text, length);
	tape->row_lengths[position] = length;
	tape->row_count++;
	return true;
}

static void
info_team_fixture(struct info_team_tape *tape,
    struct yt_info_team_state *state)
{
	static const uint8_t team_name[] = {'T', 0, 'M'};
	static const uint8_t captain_name[] = {'C', 0, 'P'};

	memset(tape, 0, sizeof(*tape));
	memset(state, 0, sizeof(*state));
	tape->fail_at = SIZE_MAX;
	tape->players[0].team = 3.5f;
	memcpy(tape->players[1].name, captain_name, sizeof(captain_name));
	tape->players[1].name_length = 3.0f;
	tape->players[1].team = 3.5f;
	memcpy(tape->players[2].name, "SECOND", 6U);
	tape->players[2].name_length = 6.0f;
	tape->players[2].team = 3.5f;
	tape->team.id = 3;
	memcpy(tape->team.name, team_name, sizeof(team_name));
	tape->team.name_length = sizeof(team_name);
	tape->team.captain = 2.5f;
	memset(tape->overlay.record.bytes, 0xa5,
	    sizeof(tape->overlay.record.bytes));
	state->current_record = 7.0f;
	state->sector_offset = 52.0f;
}

static bool
check_info_team_resolver_transaction(void)
{
	static const struct yt_info_team_ops ops = {
		info_team_read_player,
		info_team_load,
		info_team_read_overlay,
		info_team_write_overlay,
		info_team_present,
	};
	static const int other_events[] = {
		INFO_TEAM_READ_PLAYER,
		INFO_TEAM_LOAD,
		INFO_TEAM_PRESENT,
		INFO_TEAM_PRESENT,
		INFO_TEAM_READ_PLAYER,
		INFO_TEAM_READ_PLAYER,
		INFO_TEAM_PRESENT,
		INFO_TEAM_PRESENT,
	};
	static const int self_events[] = {
		INFO_TEAM_READ_PLAYER,
		INFO_TEAM_LOAD,
		INFO_TEAM_PRESENT,
		INFO_TEAM_PRESENT,
		INFO_TEAM_PRESENT,
		INFO_TEAM_PRESENT,
	};
	static const int promotion_events[] = {
		INFO_TEAM_READ_PLAYER,
		INFO_TEAM_LOAD,
		INFO_TEAM_PRESENT,
		INFO_TEAM_PRESENT,
		INFO_TEAM_READ_OVERLAY,
		INFO_TEAM_WRITE_OVERLAY,
		INFO_TEAM_PRESENT,
		INFO_TEAM_PRESENT,
		INFO_TEAM_PRESENT,
	};
	static const uint8_t team_row[] =
	    {'T','e','a','m',' ',' ',':',' ','3','.','5',',',' ','T',0,'M'};
	static const uint8_t captain_row[] =
	    {'Y','o','u','r',' ','T','e','a','m',' ','C','a','p','t','a','i','n',
	     ' ','i','s',':',' ','C',0,'P','!'};
	static const uint8_t none[] = "Team  : None";
	static const uint8_t self[] = "You are the Captain of team 3.5!";
	static const uint8_t promoted[] =
	    "Your team has no captain! You've been promoted to Captain!";
	static const uint8_t congratulations[] =
	    "Congratulations Captain! See Team Menu for your new options!";
	struct info_team_tape tape;
	struct yt_info_team_state state;
	float written_captain;
	size_t failure;

	info_team_fixture(&tape, &state);
	if (!yt_info_team_resolver_run(&state, &ops, &tape, NULL)
	    || tape.event_count != YT_ARRAY_LEN(other_events)
	    || memcmp(tape.events, other_events, sizeof(other_events)) != 0
	    || tape.player_position != 3U
	    || tape.player_records[0] != 7.0f
	    || tape.player_records[1] != 2.5f
	    || tape.player_records[2] != 2.5f
	    || tape.loaded_team_id != 3.5f
	    || tape.loaded_current_record != 7.0f
	    || state.team_id != 3.5f || state.captain_record != 2.5f
	    || state.captain_name_length != 3U
	    || memcmp(state.captain_name, "C\0P", 3U) != 0
	    || state.current_is_captain
	    || state.route != YT_INFO_TEAM_OTHER_CAPTAIN
	    || tape.row_count != 4U
	    || tape.row_lengths[0] != sizeof(team_row)
	    || memcmp(tape.rows[0], team_row, sizeof(team_row)) != 0
	    || tape.row_lengths[1] != 0U
	    || tape.row_lengths[2] != sizeof(captain_row)
	    || memcmp(tape.rows[2], captain_row, sizeof(captain_row)) != 0
	    || tape.row_lengths[3] != 0U)
		return false;

	info_team_fixture(&tape, &state);
	tape.captain_flag = -1.0f;
	if (!yt_info_team_resolver_run(&state, &ops, &tape, NULL)
	    || tape.event_count != YT_ARRAY_LEN(self_events)
	    || memcmp(tape.events, self_events, sizeof(self_events)) != 0
	    || !state.current_is_captain
	    || state.route != YT_INFO_TEAM_SELF_CAPTAIN
	    || tape.row_lengths[2] != sizeof(self) - 1U
	    || memcmp(tape.rows[2], self, sizeof(self) - 1U) != 0)
		return false;

	info_team_fixture(&tape, &state);
	tape.players[0].team = 0.0f;
	if (!yt_info_team_resolver_run(&state, &ops, &tape, NULL)
	    || tape.event_count != 3U
	    || tape.events[0] != INFO_TEAM_READ_PLAYER
	    || tape.events[1] != INFO_TEAM_PRESENT
	    || tape.events[2] != INFO_TEAM_PRESENT
	    || tape.row_lengths[0] != sizeof(none) - 1U
	    || memcmp(tape.rows[0], none, sizeof(none) - 1U) != 0
	    || tape.row_lengths[1] != 0U || state.team_id != 0.0f
	    || state.route != YT_INFO_TEAM_NONE)
		return false;

	info_team_fixture(&tape, &state);
	tape.team.captain = 1.5f;
	if (!yt_info_team_resolver_run(&state, &ops, &tape, NULL)
	    || tape.event_count != YT_ARRAY_LEN(promotion_events)
	    || memcmp(tape.events, promotion_events,
	    sizeof(promotion_events)) != 0
	    || tape.player_position != 1U || !state.current_is_captain
	    || state.route != YT_INFO_TEAM_PROMOTED
	    || state.captain_record != 7.0f || state.captain_flag != 1.0f
	    || tape.overlay_read_team != 3.5f
	    || tape.overlay_write_team != 3.5f
	    || tape.row_lengths[2] != sizeof(promoted) - 1U
	    || memcmp(tape.rows[2], promoted, sizeof(promoted) - 1U) != 0
	    || tape.row_lengths[3] != sizeof(congratulations) - 1U
	    || memcmp(tape.rows[3], congratulations,
	    sizeof(congratulations) - 1U) != 0
	    || tape.row_lengths[4] != 0U
	    || (written_captain = yt_record_get_number(&tape.written.record,
	    YT_F77)) != 7.0f)
		return false;

	info_team_fixture(&tape, &state);
	tape.players[1].name_length = 0.0f;
	if (!yt_info_team_resolver_run(&state, &ops, &tape, NULL)
	    || state.route != YT_INFO_TEAM_PROMOTED
	    || tape.player_position != 2U)
		return false;
	info_team_fixture(&tape, &state);
	tape.players[1].team = 9.0f;
	if (!yt_info_team_resolver_run(&state, &ops, &tape, NULL)
	    || state.route != YT_INFO_TEAM_PROMOTED
	    || tape.player_position != 2U)
		return false;
	info_team_fixture(&tape, &state);
	tape.players[1].name_length = 99.0f;
	memset(tape.players[1].name, 'Q', YT_TEXT_FIELD_SIZE);
	if (!yt_info_team_resolver_run(&state, &ops, &tape, NULL)
	    || state.route != YT_INFO_TEAM_OTHER_CAPTAIN
	    || state.captain_name_length != YT_TEXT_FIELD_SIZE)
		return false;
	info_team_fixture(&tape, &state);
	state.sector_offset = 51.0f;
	tape.team.captain = 51.0f;
	if (!yt_info_team_resolver_run(&state, &ops, &tape, NULL)
	    || state.route != YT_INFO_TEAM_OTHER_CAPTAIN
	    || tape.player_position != 3U
	    || tape.player_records[1] != 51.0f
	    || tape.player_records[2] != 51.0f)
		return false;
	info_team_fixture(&tape, &state);
	state.sector_offset = 51.0f;
	tape.team.captain = 51.25f;
	if (!yt_info_team_resolver_run(&state, &ops, &tape, NULL)
	    || state.route != YT_INFO_TEAM_PROMOTED
	    || tape.player_position != 1U)
		return false;

	for (failure = 1U; failure <= YT_ARRAY_LEN(other_events); ++failure) {
		info_team_fixture(&tape, &state);
		tape.fail_at = failure;
		if (yt_info_team_resolver_run(&state, &ops, &tape, NULL)
		    || tape.event_count != failure
		    || memcmp(tape.events, other_events,
		    failure * sizeof(other_events[0])) != 0)
			return false;
	}
	for (failure = 1U; failure <= YT_ARRAY_LEN(promotion_events); ++failure) {
		info_team_fixture(&tape, &state);
		tape.team.captain = 1.5f;
		tape.fail_at = failure;
		if (yt_info_team_resolver_run(&state, &ops, &tape, NULL)
		    || tape.event_count != failure
		    || memcmp(tape.events, promotion_events,
		    failure * sizeof(promotion_events[0])) != 0)
			return false;
	}
	return !yt_info_team_resolver_run(NULL, &ops, &tape, NULL)
	    && !yt_info_team_resolver_run(&state, NULL, &tape, NULL);
}

enum info_panel_event {
	INFO_PANEL_REFRESH = 1,
	INFO_PANEL_TEAM,
	INFO_PANEL_READ_PLAYER,
	INFO_PANEL_PRESENT,
};

struct info_panel_tape {
	int events[40];
	size_t event_count;
	size_t fail_at;
	uint8_t time_text[64];
	size_t time_length;
	struct yt_player final_player;
	enum yt_info_panel_output_kind kinds[35];
	float widths[35];
	float foreground[35];
	float background[35];
	float bold[35];
	size_t present_count;
	size_t line_count;
	size_t fixed_count;
	uint8_t serial[1024];
	size_t serial_length;
	uint8_t cached_name[YT_TEXT_FIELD_SIZE];
};

static bool
info_panel_step(struct info_panel_tape *tape, enum info_panel_event event)
{
	if (tape->event_count >= YT_ARRAY_LEN(tape->events))
		return false;
	tape->events[tape->event_count++] = (int)event;
	return tape->event_count != tape->fail_at;
}

static bool
info_panel_append(struct info_panel_tape *tape, const uint8_t *text,
    size_t length)
{
	if (length > sizeof(tape->serial) - tape->serial_length
	    || (text == NULL && length != 0U))
		return false;
	if (length != 0U)
		memcpy(tape->serial + tape->serial_length, text, length);
	tape->serial_length += length;
	return true;
}

static bool
info_panel_refresh_test(void *context, uint8_t *text, size_t capacity,
    size_t *length, struct yt_error *error)
{
	struct info_panel_tape *tape = context;

	(void)error;
	if (!info_panel_step(tape, INFO_PANEL_REFRESH)
	    || tape->time_length > capacity || length == NULL)
		return false;
	if (tape->time_length != 0U)
		memcpy(text, tape->time_text, tape->time_length);
	*length = tape->time_length;
	return true;
}

static bool
info_panel_team_test(void *context, struct yt_error *error)
{
	static const uint8_t rows[] = "Team  : None\r\n\r\n";
	struct info_panel_tape *tape = context;

	(void)error;
	return info_panel_step(tape, INFO_PANEL_TEAM)
	    && info_panel_append(tape, rows, sizeof(rows) - 1U);
}

static bool
info_panel_read_player_test(void *context, struct yt_player *player,
    struct yt_error *error)
{
	struct info_panel_tape *tape = context;

	(void)error;
	if (!info_panel_step(tape, INFO_PANEL_READ_PLAYER))
		return false;
	*player = tape->final_player;
	return true;
}

static bool
info_panel_present_test(void *context, const uint8_t *text, size_t length,
    enum yt_info_panel_output_kind kind, float width,
    struct yt_info_panel_state *state, struct yt_error *error)
{
	static const uint8_t newline[] = "\r\n";
	static const uint8_t spaces[64] = {
		' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
		' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
		' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
		' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
		' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
		' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
		' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
		' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '
	};
	struct info_panel_tape *tape = context;
	size_t position = tape->present_count;
	size_t rendered;
	size_t fixed_width;

	(void)error;
	if (position >= YT_ARRAY_LEN(tape->kinds))
		return false;
	tape->kinds[position] = kind;
	tape->widths[position] = width;
	tape->foreground[position] = state->foreground;
	tape->background[position] = state->background;
	tape->bold[position] = state->bold;
	tape->present_count++;
	if (!info_panel_step(tape, INFO_PANEL_PRESENT))
		return false;
	if (kind == YT_INFO_PANEL_LINE) {
		tape->line_count++;
		return info_panel_append(tape, text, length)
		    && info_panel_append(tape, newline, sizeof(newline) - 1U);
	}
	if (kind != YT_INFO_PANEL_FIXED || width < 0.0f
	    || width > (float)sizeof(spaces)
	    || width != floorf(width))
		return false;
	tape->fixed_count++;
	fixed_width = (size_t)width;
	rendered = length < fixed_width ? length : fixed_width;
	return info_panel_append(tape, text, rendered)
	    && info_panel_append(tape, spaces, fixed_width - rendered);
}

static void
info_panel_fixture(struct info_panel_tape *tape,
    struct yt_info_panel_state *state)
{
	memset(tape, 0, sizeof(*tape));
	memset(state, 0, sizeof(*state));
	tape->fail_at = SIZE_MAX;
	memcpy(tape->cached_name, "Pilot", 5U);
	memcpy(tape->time_text, " 15:09  ", 8U);
	tape->time_length = 8U;
	tape->final_player.credits = 12345.0f;
	tape->final_player.sector = 733.0f;
	tape->final_player.turns = 42.0f;
	tape->final_player.holds = 20.0f;
	tape->final_player.fighters = 1000.0f;
	tape->final_player.ore = 3.0f;
	tape->final_player.mines = 4.0f;
	tape->final_player.organics = 5.0f;
	tape->final_player.missiles = 6.0f;
	tape->final_player.equipment = 7.0f;
	tape->final_player.danger_scanner = 1.0f;
	tape->final_player.ports_owned = 8.0f;
	tape->final_player.shields = 90.0f;
	tape->final_player.cloak = 0.75f;
	tape->final_player.ground_forces = 9.0f;
	tape->final_player.plasma = 10.0f;
	state->cached_name = tape->cached_name;
	state->cached_name_length = 5U;
	state->foreground = 6.0f;
}

static uint64_t
info_panel_fnv1a64(const uint8_t *data, size_t length)
{
	uint64_t value = UINT64_C(14695981039346656037);
	size_t index;

	for (index = 0U; index < length; ++index) {
		value ^= data[index];
		value *= UINT64_C(1099511628211);
	}
	return value;
}

static bool
info_panel_contains(const uint8_t *data, size_t data_length,
    const uint8_t *needle, size_t needle_length)
{
	size_t index;

	if (needle_length == 0U)
		return true;
	if (needle_length > data_length)
		return false;
	for (index = 0U; index <= data_length - needle_length; ++index)
		if (memcmp(data + index, needle, needle_length) == 0)
			return true;
	return false;
}

static bool
check_info_panel_transaction(void)
{
	static const struct yt_info_panel_ops ops = {
		info_panel_refresh_test,
		info_panel_team_test,
		info_panel_read_player_test,
		info_panel_present_test,
	};
	static const enum yt_info_panel_output_kind expected_kinds[35] = {
		YT_INFO_PANEL_LINE, YT_INFO_PANEL_FIXED,
		YT_INFO_PANEL_LINE, YT_INFO_PANEL_LINE,
		YT_INFO_PANEL_LINE, YT_INFO_PANEL_LINE,
		YT_INFO_PANEL_LINE,
		YT_INFO_PANEL_FIXED, YT_INFO_PANEL_FIXED, YT_INFO_PANEL_LINE,
		YT_INFO_PANEL_FIXED, YT_INFO_PANEL_FIXED, YT_INFO_PANEL_LINE,
		YT_INFO_PANEL_FIXED, YT_INFO_PANEL_FIXED, YT_INFO_PANEL_FIXED,
		YT_INFO_PANEL_LINE,
		YT_INFO_PANEL_FIXED, YT_INFO_PANEL_FIXED, YT_INFO_PANEL_FIXED,
		YT_INFO_PANEL_LINE,
		YT_INFO_PANEL_FIXED, YT_INFO_PANEL_FIXED, YT_INFO_PANEL_FIXED,
		YT_INFO_PANEL_LINE,
		YT_INFO_PANEL_FIXED, YT_INFO_PANEL_FIXED, YT_INFO_PANEL_LINE,
		YT_INFO_PANEL_FIXED, YT_INFO_PANEL_FIXED, YT_INFO_PANEL_LINE,
		YT_INFO_PANEL_FIXED, YT_INFO_PANEL_FIXED, YT_INFO_PANEL_LINE,
		YT_INFO_PANEL_LINE,
	};
	static const float expected_widths[35] = {
		0, 20, 0, 0, 0, 0, 0,
		26, 23, 0, 26, 23, 0,
		26, 17, 6, 0, 26, 17, 6, 0, 26, 17, 6, 0,
		26, 23, 0, 26, 23, 0, 26, 23, 0, 0,
	};
	static const uint8_t heading[] =
	    "\r\n                    [ Info ]\r\n\r\n"
	    "Name  : Pilot\r\nTime  : 15:09  \r\n"
	    "Team  : None\r\n\r\n";
	static const uint8_t binary_name[] =
	    {'N','a','m','e',' ',' ',':',' ','A',0,'B','\r','\n'};
	static const uint8_t cloak_fail[] = " Cloak Energy. : FAIL";
	static const uint8_t cloak_negative[] = " Cloak Energy. :-2%";
	struct info_panel_tape expected;
	struct info_panel_tape tape;
	struct yt_info_panel_state state;
	size_t failure;
	size_t position;
	size_t row;

	info_panel_fixture(&expected, &state);
	if (!yt_info_panel_run(&state, &ops, &expected, NULL)
	    || expected.event_count != 38U
	    || expected.events[0] != INFO_PANEL_REFRESH
	    || expected.events[7] != INFO_PANEL_TEAM
	    || expected.events[8] != INFO_PANEL_READ_PLAYER
	    || expected.present_count != 35U || expected.line_count != 15U
	    || expected.fixed_count != 20U
	    || memcmp(expected.kinds, expected_kinds, sizeof(expected_kinds)) != 0
	    || memcmp(expected.widths, expected_widths,
	    sizeof(expected_widths)) != 0
	    || expected.serial_length != 602U
	    || info_panel_fnv1a64(expected.serial, expected.serial_length)
	    != UINT64_C(0x9b1a7fd0d0c4f1fd)
	    || sizeof(heading) - 1U != 82U
	    || memcmp(expected.serial, heading, sizeof(heading) - 1U) != 0
	    || state.foreground != 6.0f || state.background != 0.0f
	    || state.bold != 1.0f
	    || expected.foreground[15] != 7.0f
	    || expected.background[15] != 4.0f
	    || expected.bold[15] != 1.0f
	    || expected.foreground[16] != 2.0f
	    || expected.background[16] != 0.0f
	    || expected.bold[16] != 1.0f)
		return false;
	for (position = 1U; position < 7U; ++position)
		if (expected.events[position] != INFO_PANEL_PRESENT)
			return false;
	for (position = 9U; position < expected.event_count; ++position)
		if (expected.events[position] != INFO_PANEL_PRESENT)
			return false;
	for (row = 0U; row < 10U; ++row) {
		const uint8_t *line = expected.serial + 82U + row * 52U;

		if (line[50] != '\r' || line[51] != '\n')
			return false;
		if (row > 0U && row < 9U
		    && (line[0] != 0xba || line[26] != 0xba
		    || line[49] != 0xba))
			return false;
	}
	if (expected.serial[82] != 0xc9 || expected.serial[82 + 25] != 0xcd
	    || expected.serial[82 + 26] != 0xcb
	    || expected.serial[82 + 49] != 0xbb
	    || expected.serial[550] != 0xc8
	    || expected.serial[550 + 26] != 0xca
	    || expected.serial[550 + 49] != 0xbc)
		return false;

	for (failure = 1U; failure <= expected.event_count; ++failure) {
		info_panel_fixture(&tape, &state);
		tape.fail_at = failure;
		if (yt_info_panel_run(&state, &ops, &tape, NULL)
		    || tape.event_count != failure
		    || memcmp(tape.events, expected.events,
		    failure * sizeof(tape.events[0])) != 0
		    || tape.serial_length > expected.serial_length
		    || memcmp(tape.serial, expected.serial,
		    tape.serial_length) != 0)
			return false;
		if (failure == 1U
		    && (state.foreground != 6.0f || state.bold != 0.0f))
			return false;
		if (tape.present_count != 0U) {
			position = tape.present_count - 1U;
			if (state.foreground != tape.foreground[position]
			    || state.background != tape.background[position]
			    || state.bold != tape.bold[position])
				return false;
		}
	}

	info_panel_fixture(&tape, &state);
	memcpy(tape.cached_name, "A\0B", 3U);
	state.cached_name_length = 3U;
	state.anti_cloak = -1.0f;
	tape.final_player.ore = 0.0f;
	tape.final_player.organics = 0.0f;
	tape.final_player.equipment = 0.0f;
	tape.final_player.danger_scanner = 0.0f;
	if (!yt_info_panel_run(&state, &ops, &tape, NULL)
	    || !info_panel_contains(tape.serial, tape.serial_length,
	    binary_name, sizeof(binary_name))
	    || !info_panel_contains(tape.serial, tape.serial_length,
	    cloak_fail, sizeof(cloak_fail) - 1U)
	    || !info_panel_contains(tape.serial, tape.serial_length,
	    (const uint8_t *)" Scanner.. : NONE",
	    sizeof(" Scanner.. : NONE") - 1U)
	    || state.bold != 0.0f)
		return false;

	info_panel_fixture(&tape, &state);
	tape.final_player.cloak = -0.015f;
	if (!yt_info_panel_run(&state, &ops, &tape, NULL)
	    || !info_panel_contains(tape.serial, tape.serial_length,
	    cloak_negative, sizeof(cloak_negative) - 1U))
		return false;

	return !yt_info_panel_run(NULL, &ops, &tape, NULL)
	    && !yt_info_panel_run(&state, NULL, &tape, NULL);
}

enum spy_sweep_event {
	SPY_SWEEP_READ_SECTOR = 1,
	SPY_SWEEP_UPDATE_PLANET,
	SPY_SWEEP_READ_PLANET,
	SPY_SWEEP_READ_PLAYER,
	SPY_SWEEP_READ_TEAM,
	SPY_SWEEP_RANDOM,
	SPY_SWEEP_SOUND,
	SPY_SWEEP_PRESENT,
	SPY_SWEEP_PAUSE,
};

struct spy_sweep_tape {
	int events[64];
	float arguments[64];
	size_t event_count;
	size_t fail_at;
	uint8_t serial[1024];
	size_t serial_length;
	struct yt_sector sector;
	struct yt_sector sector_reads[8];
	size_t sector_read_count;
	size_t sector_read_position;
	struct yt_planet planet;
	struct yt_player players[5];
	struct yt_sector team;
	float draws[8];
	size_t draw_position;
};

static bool
spy_sweep_step(struct spy_sweep_tape *tape, enum spy_sweep_event event,
    float argument)
{
	if (tape->event_count >= YT_ARRAY_LEN(tape->events))
		return false;
	tape->events[tape->event_count] = (int)event;
	tape->arguments[tape->event_count] = argument;
	++tape->event_count;
	return tape->event_count != tape->fail_at;
}

static bool
spy_sweep_append(struct spy_sweep_tape *tape, const uint8_t *text,
    size_t length)
{
	if (length > sizeof(tape->serial) - tape->serial_length
	    || (text == NULL && length != 0U))
		return false;
	if (length != 0U)
		memcpy(tape->serial + tape->serial_length, text, length);
	tape->serial_length += length;
	return true;
}

static bool
spy_sweep_read_sector_test(void *context, int sector,
    struct yt_sector *result, struct yt_error *error)
{
	struct spy_sweep_tape *tape = context;

	(void)error;
	if (!spy_sweep_step(tape, SPY_SWEEP_READ_SECTOR, (float)sector))
		return false;
	if (tape->sector_read_position < tape->sector_read_count)
		*result = tape->sector_reads[tape->sector_read_position++];
	else
		*result = tape->sector;
	return true;
}

static bool
spy_sweep_update_planet_test(void *context, float planet,
    struct yt_error *error)
{
	(void)error;
	return spy_sweep_step(context, SPY_SWEEP_UPDATE_PLANET, planet);
}

static bool
spy_sweep_read_planet_test(void *context, float planet,
    struct yt_planet *result, struct yt_error *error)
{
	struct spy_sweep_tape *tape = context;

	(void)error;
	if (!spy_sweep_step(tape, SPY_SWEEP_READ_PLANET, planet))
		return false;
	*result = tape->planet;
	return true;
}

static bool
spy_sweep_read_player_test(void *context, float player,
    struct yt_player *result, struct yt_error *error)
{
	struct spy_sweep_tape *tape = context;
	bool overflow;
	int record;

	(void)error;
	if (!spy_sweep_step(tape, SPY_SWEEP_READ_PLAYER, player))
		return false;
	record = (int)qb_cint(player, &overflow);
	if (overflow || record < 0
	    || (size_t)record >= YT_ARRAY_LEN(tape->players))
		return false;
	*result = tape->players[record];
	return true;
}

static bool
spy_sweep_read_team_test(void *context, float team,
    struct yt_sector *result, struct yt_error *error)
{
	struct spy_sweep_tape *tape = context;

	(void)error;
	if (!spy_sweep_step(tape, SPY_SWEEP_READ_TEAM, team))
		return false;
	*result = tape->team;
	return true;
}

static bool
spy_sweep_random_test(void *context, float *value, struct yt_error *error)
{
	struct spy_sweep_tape *tape = context;

	(void)error;
	if (!spy_sweep_step(tape, SPY_SWEEP_RANDOM, 0.0f)
	    || tape->draw_position >= YT_ARRAY_LEN(tape->draws))
		return false;
	*value = tape->draws[tape->draw_position++];
	return true;
}

static bool
spy_sweep_sound_test(void *context, float selector, struct yt_error *error)
{
	(void)error;
	return spy_sweep_step(context, SPY_SWEEP_SOUND, selector);
}

static bool
spy_sweep_present_test(void *context, const uint8_t *text, size_t length,
    enum yt_spy_output_kind kind, struct yt_spy_sweep_state *state,
    struct yt_error *error)
{
	static const uint8_t newline[] = "\r\n";
	struct spy_sweep_tape *tape = context;

	(void)error;
	if (!spy_sweep_step(tape, SPY_SWEEP_PRESENT, (float)kind))
		return false;
	if (kind == YT_SPY_BOLD_LINE || kind == YT_SPY_BOLD_RAW)
		state->bold = 1.0f;
	else if (kind == YT_SPY_ATTENTION) {
		state->foreground = 3.0f;
		state->background = 1.0f;
		state->blink = 1.0f;
		state->bold = 1.0f;
	}
	if (!spy_sweep_append(tape, text, length))
		return false;
	state->bold = 0.0f;
	state->blink = 0.0f;
	if (kind == YT_SPY_BOLD_RAW)
		return true;
	if (kind == YT_SPY_ATTENTION)
		state->background = 0.0f;
	return spy_sweep_append(tape, newline, sizeof(newline) - 1U);
}

static bool
spy_sweep_pause_test(void *context, struct yt_spy_sweep_state *state,
    struct yt_error *error)
{
	static const uint8_t prompt[] =
	    "*[ Press any Key ]*\r                   \r";
	struct spy_sweep_tape *tape = context;
	float saved = state->foreground;

	(void)error;
	if (!spy_sweep_step(tape, SPY_SWEEP_PAUSE, 0.0f))
		return false;
	state->foreground = 3.0f;
	state->bold = 0.0f;
	if (!spy_sweep_append(tape, prompt, sizeof(prompt) - 1U))
		return false;
	state->foreground = saved;
	return true;
}

static void
spy_sweep_fixture(struct spy_sweep_tape *tape,
    struct yt_spy_sweep_state *state, int sectors[3], int markers[3],
    float sector_cache[52], float cloak_cache[52])
{
	memset(tape, 0, sizeof(*tape));
	memset(state, 0, sizeof(*state));
	memset(sectors, 0, 3U * sizeof(*sectors));
	memset(markers, 0, 3U * sizeof(*markers));
	memset(sector_cache, 0, 52U * sizeof(*sector_cache));
	memset(cloak_cache, 0, 52U * sizeof(*cloak_cache));
	tape->fail_at = SIZE_MAX;
	tape->sector.mines = 5.0f;
	tape->sector.planet = 1.0f;
	tape->sector.fighters = 8.0f;
	tape->sector.fighter_owner = 4.0f;
	tape->sector.warps[1] = 200.0f;
	memcpy(tape->planet.record.bytes, "Gaia", 4U);
	tape->planet.name_length = 4.0f;
	tape->planet.ground_forces = 9.0f;
	memcpy(tape->players[3].record.bytes, "Ada", 3U);
	tape->players[3].name_length = 3.0f;
	tape->players[3].team = 7.0f;
	tape->players[3].fighters = 12.0f;
	tape->players[3].shields = 34.0f;
	memcpy(tape->players[4].record.bytes, "Grace", 5U);
	tape->players[4].name_length = 5.0f;
	tape->players[4].team = 2.0f;
	tape->players[4].fighters = 20.0f;
	tape->players[4].shields = 40.0f;
	memcpy(tape->team.record.bytes, "Union", 5U);
	(void)yt_record_set_number(&tape->team.record, YT_F73, 5.0f);
	tape->draws[0] = 0.75f;
	tape->draws[1] = 0.2f;
	sectors[0] = 100;
	sector_cache[3] = 100.0f;
	cloak_cache[3] = 0.5f;
	state->active_spies = 1.0f;
	state->spy_sectors = sectors;
	state->last_reported_sectors = markers;
	state->spy_capacity = 3U;
	state->current_player_record = 2;
	state->last_player_record = 51.0f;
	state->sector_cache = sector_cache;
	state->cloak_cache = cloak_cache;
	state->cache_count = 52U;
	state->foreground = 5.0f;
}

static bool
check_spy_sweep_transaction(void)
{
	static const struct yt_spy_sweep_ops ops = {
		spy_sweep_read_sector_test,
		spy_sweep_update_planet_test,
		spy_sweep_read_planet_test,
		spy_sweep_read_player_test,
		spy_sweep_read_team_test,
		spy_sweep_random_test,
		spy_sweep_sound_test,
		spy_sweep_present_test,
		spy_sweep_pause_test,
	};
	static const int expected_events[] = {
		SPY_SWEEP_READ_SECTOR,
		SPY_SWEEP_PRESENT, SPY_SWEEP_SOUND, SPY_SWEEP_PRESENT,
		SPY_SWEEP_PRESENT, SPY_SWEEP_PRESENT,
		SPY_SWEEP_UPDATE_PLANET, SPY_SWEEP_READ_PLANET,
		SPY_SWEEP_PRESENT, SPY_SWEEP_PRESENT, SPY_SWEEP_READ_SECTOR,
		SPY_SWEEP_RANDOM, SPY_SWEEP_PRESENT, SPY_SWEEP_PRESENT,
		SPY_SWEEP_SOUND, SPY_SWEEP_PRESENT, SPY_SWEEP_PRESENT,
		SPY_SWEEP_READ_PLAYER, SPY_SWEEP_PRESENT,
		SPY_SWEEP_READ_SECTOR, SPY_SWEEP_READ_SECTOR,
		SPY_SWEEP_PRESENT, SPY_SWEEP_PRESENT, SPY_SWEEP_READ_PLAYER,
		SPY_SWEEP_READ_TEAM, SPY_SWEEP_PRESENT,
		SPY_SWEEP_PRESENT, SPY_SWEEP_PAUSE,
		SPY_SWEEP_READ_SECTOR, SPY_SWEEP_RANDOM,
	};
	static const uint8_t expected_serial[] =
	    "\r\n"
	    "*** RADIO MESSAGE FROM SPY #1! The following was found in sector 100:\r\n"
	    "\r\n"
	    "** WARNING! SECTOR HAS 5 MINES! **\r\n"
	    "\r\n"
	    "Planet: Gaia * Forces: 9\r\n"
	    "\r\n"
	    "The spy detected the shimmering of a cloaking device!\r\n"
	    "\r\n"
	    "Other Ships: \r\n"
	    "    Ada - Team: 7 - Fighters: 12 - Shields: 34\r\n"
	    "\r\n"
	    "Fighters in sector: 8 (Belong to Grace Team [2] [Union])\r\n"
	    "\r\n"
	    "*[ Press any Key ]*\r                   \r";
	struct spy_sweep_tape expected;
	struct spy_sweep_tape tape;
	struct yt_spy_sweep_state state;
	int sectors[3];
	int markers[3];
	float sector_cache[52];
	float cloak_cache[52];
	size_t failure;

	spy_sweep_fixture(&expected, &state, sectors, markers,
	    sector_cache, cloak_cache);
	if (!yt_spy_sweep_run(&state, &ops, &expected, NULL)
	    || expected.event_count != YT_ARRAY_LEN(expected_events)
	    || memcmp(expected.events, expected_events,
	    sizeof(expected_events)) != 0
	    || expected.serial_length != sizeof(expected_serial) - 1U
	    || expected.serial_length != 363U
	    || memcmp(expected.serial, expected_serial,
	    sizeof(expected_serial) - 1U) != 0
	    || info_panel_fnv1a64(expected.serial, expected.serial_length)
	    != UINT64_C(0x94e7f43d9a999917)
	    || sectors[0] != 200 || markers[0] != 100
	    || cloak_cache[3] != 0.0f
	    || state.found_scratch != 1.0f
	    || state.dead_counter_scratch != 2.0f
	    || state.warp_destination_scratch != 200.0f
	    || state.foreground != 0.0f)
		return false;
	for (failure = 1U; failure <= expected.event_count; ++failure) {
		spy_sweep_fixture(&tape, &state, sectors, markers,
		    sector_cache, cloak_cache);
		tape.fail_at = failure;
		if (yt_spy_sweep_run(&state, &ops, &tape, NULL)
		    || tape.event_count != failure
		    || memcmp(tape.events, expected.events,
		    failure * sizeof(tape.events[0])) != 0
		    || tape.serial_length > expected.serial_length
		    || memcmp(tape.serial, expected.serial,
		    tape.serial_length) != 0)
			return false;
	}
	spy_sweep_fixture(&tape, &state, sectors, markers,
	    sector_cache, cloak_cache);
	memset(&tape.sector, 0, sizeof(tape.sector));
	tape.sector.warps[0] = 200.0f;
	tape.draws[0] = 0.0f;
	state.last_player_record = 2.0f;
	if (!yt_spy_sweep_run(&state, &ops, &tape, NULL)
	    || tape.event_count != 4U || tape.serial_length != 0U
	    || tape.events[0] != SPY_SWEEP_READ_SECTOR
	    || tape.events[1] != SPY_SWEEP_READ_SECTOR
	    || tape.events[2] != SPY_SWEEP_READ_SECTOR
	    || tape.events[3] != SPY_SWEEP_RANDOM
	    || state.found_scratch != 0.0f || markers[0] != 0
	    || sectors[0] != 200)
		return false;
	spy_sweep_fixture(&tape, &state, sectors, markers,
	    sector_cache, cloak_cache);
	memset(&tape.sector, 0, sizeof(tape.sector));
	tape.sector.warps[0] = 200.0f;
	tape.draws[0] = 0.25f;
	tape.draws[1] = 0.0f;
	state.last_player_record = 3.0f;
	if (!yt_spy_sweep_run(&state, &ops, &tape, NULL)
	    || tape.serial_length != 0U || tape.draw_position != 2U
	    || cloak_cache[3] != 0.5f || sectors[0] != 200)
		return false;
	spy_sweep_fixture(&tape, &state, sectors, markers,
	    sector_cache, cloak_cache);
	memset(&tape.sector, 0, sizeof(tape.sector));
	tape.sector.warps[0] = 200.0f;
	tape.draws[0] = 0.0f;
	state.last_player_record = 2.0f;
	state.disruption_sectors[0] = 100.0f;
	state.disruption_sectors[1] = 100.0f;
	if (!yt_spy_sweep_run(&state, &ops, &tape, NULL)
	    || !info_panel_contains(tape.serial, tape.serial_length,
	    (const uint8_t *)"** Space-time disruption detected! **",
	    sizeof("** Space-time disruption detected! **") - 1U)
	    || state.dead_counter_scratch != 0.0f)
		return false;
	spy_sweep_fixture(&tape, &state, sectors, markers,
	    sector_cache, cloak_cache);
	memset(tape.sector_reads, 0, sizeof(tape.sector_reads));
	tape.sector_read_count = 4U;
	tape.sector_reads[0].warps[0] = 200.0f;
	tape.sector_reads[1].fighters = 1.0f;
	tape.sector_reads[1].fighter_owner = -1.0f;
	tape.sector_reads[2].fighters = 9.0f;
	tape.sector_reads[2].fighter_owner = 4.0f;
	tape.sector_reads[3].warps[0] = 200.0f;
	tape.draws[0] = 0.0f;
	state.last_player_record = 2.0f;
	if (!yt_spy_sweep_run(&state, &ops, &tape, NULL)
	    || !info_panel_contains(tape.serial, tape.serial_length,
	    (const uint8_t *)
	    "Fighters in sector: 9 (Belong to The Xannor)\r\n",
	    sizeof("Fighters in sector: 9 (Belong to The Xannor)\r\n") - 1U)
	    || state.dead_counter_scratch != 0.0f)
		return false;
	spy_sweep_fixture(&tape, &state, sectors, markers,
	    sector_cache, cloak_cache);
	state.active_spies = -1.0f;
	if (!yt_spy_sweep_run(&state, &ops, &tape, NULL)
	    || tape.event_count != 0U || state.foreground != 0.0f)
		return false;
	spy_sweep_fixture(&tape, &state, sectors, markers,
	    sector_cache, cloak_cache);
	markers[0] = 100;
	state.found_scratch = 1.0f;
	if (!yt_spy_sweep_run(&state, &ops, &tape, NULL)
	    || tape.event_count != 5U
	    || tape.events[0] != SPY_SWEEP_PRESENT
	    || tape.events[1] != SPY_SWEEP_PAUSE
	    || tape.events[2] != SPY_SWEEP_READ_SECTOR
	    || tape.events[3] != SPY_SWEEP_RANDOM
	    || tape.events[4] != SPY_SWEEP_RANDOM
	    || tape.serial_length != 42U
	    || sectors[0] != 200 || state.foreground != 0.0f)
		return false;
	spy_sweep_fixture(&tape, &state, sectors, markers,
	    sector_cache, cloak_cache);
	state.active_spies = 0.0f;
	return yt_spy_sweep_run(&state, &ops, &tape, NULL)
	    && tape.event_count == 0U && state.foreground == 5.0f
	    && !yt_spy_sweep_run(NULL, &ops, &tape, NULL)
	    && !yt_spy_sweep_run(&state, NULL, &tape, NULL);
}

static bool
check_port_owner_row_model(void)
{
	static const uint8_t self_expected[] =
	    "This port is owned by: YOU, Credits: 1234.5";
	static const uint8_t other_name[] = {'O', 't', 0, 'h', 'e', 'r'};
	static const uint8_t other_expected[] =
	    {'T', 'h', 'i', 's', ' ', 'p', 'o', 'r', 't', ' ', 'i', 's', ' ',
	     'o', 'w', 'n', 'e', 'd', ' ', 'b', 'y', ':', ' ',
	     'O', 't', 0, 'h', 'e', 'r'};
	uint8_t row[96];
	size_t length = 99U;
	int owner_record = -1;

	if (yt_port_owner_classify(0.0f, 7, &owner_record)
	    != YT_PORT_OWNER_SILENT || owner_record != 0
	    || yt_port_owner_classify(1.0f, 1, &owner_record)
	    != YT_PORT_OWNER_SILENT
	    || yt_port_owner_classify(-4.0f, 7, &owner_record)
	    != YT_PORT_OWNER_SILENT
	    || yt_port_owner_classify(0.5f, 7, &owner_record)
	    != YT_PORT_OWNER_SILENT
	    || yt_port_owner_classify(7.0f, 7, &owner_record)
	    != YT_PORT_OWNER_SELF
	    || yt_port_owner_classify(8.0f, 7, &owner_record)
	    != YT_PORT_OWNER_OTHER || owner_record != 8
	    || yt_port_owner_classify(1.75f, 7, &owner_record)
	    != YT_PORT_OWNER_OTHER || owner_record != 1
	    || yt_port_owner_classify(16777216.0f, 7, &owner_record)
	    != YT_PORT_OWNER_OTHER || owner_record != 0
	    || yt_port_owner_classify(INFINITY, 7, &owner_record)
	    != YT_PORT_OWNER_INVALID
	    || yt_port_owner_classify(NAN, 7, &owner_record)
	    != YT_PORT_OWNER_INVALID)
		return false;
	if (!yt_port_owner_compose(YT_PORT_OWNER_SILENT, 0.0f,
	    NULL, 0U, NULL, 0U, &length) || length != 0U
	    || !yt_port_owner_compose(YT_PORT_OWNER_SELF, 1234.5f,
	    NULL, 0U, row, sizeof(row), &length)
	    || length != sizeof(self_expected) - 1U
	    || memcmp(row, self_expected, length) != 0
	    || !yt_port_owner_compose(YT_PORT_OWNER_OTHER, 0.0f,
	    other_name, sizeof(other_name), row, sizeof(row), &length)
	    || length != sizeof(other_expected)
	    || memcmp(row, other_expected, length) != 0
	    || yt_port_owner_compose(YT_PORT_OWNER_OTHER, 0.0f,
	    other_name, sizeof(other_name), row, 8U, &length)
	    || yt_port_owner_compose(YT_PORT_OWNER_OTHER, 0.0f,
	    NULL, 1U, row, sizeof(row), &length)
	    || yt_port_owner_compose(YT_PORT_OWNER_INVALID, 0.0f,
	    NULL, 0U, row, sizeof(row), &length))
		return false;
	return true;
}

struct projectile_damage_tape {
	const float *values;
	size_t count;
	size_t position;
	size_t fail_at;
};

static bool
projectile_damage_draw(void *context, float *value, struct yt_error *error)
{
	struct projectile_damage_tape *tape = context;
	size_t call = tape->position++;

	if (call == tape->fail_at || call >= tape->count) {
		if (error != NULL) {
			error->status = YT_IO_ERROR;
			(void)snprintf(error->operation,
			    sizeof(error->operation), "%s", "projectile RND");
		}
		return false;
	}
	*value = tape->values[call];
	return true;
}

static bool
check_projectile_damage_model(void)
{
	static const float lethal_draws[] = {
		0.0f, 0.1f, 0.1f, 0.05f,
		0.0f, 0.2f, 0.2f, 0.1f
	};
	static const float no_shield_draws[] = {0.0f, 0.0f, 1.0f};
	static const float scanner_draws[] = {1.0f, 1.0f, 0.0f, 1.0f};
	struct yt_player target;
	struct yt_projectile_damage_result damage;
	struct yt_error error;
	struct projectile_damage_tape tape;
	float remaining;

	memset(&target, 0, sizeof(target));
	target.fighters = 1000.0f;
	target.shields = 100.0f;
	target.danger_scanner = 2.0f;
	remaining = 2.5f;
	tape.values = lethal_draws;
	tape.count = YT_ARRAY_LEN(lethal_draws);
	tape.position = 0U;
	tape.fail_at = SIZE_MAX;
	if (!yt_projectile_player_damage(&target, &remaining,
	    projectile_damage_draw, &tape, &damage, &error)
	    || tape.position != 8U || damage.iterations != 2U
	    || remaining != 0.5f || damage.fighters != 1000.0
	    || damage.shields != 100.0f || damage.scanner_disabled
	    || target.fighters != 0.0f || target.shields != 0.0f
	    || target.danger_scanner != 2.0f)
		return false;

	memset(&target, 0, sizeof(target));
	target.fighters = 1000.0f;
	target.shields = 100.0f;
	remaining = 1.0f;
	tape.values = no_shield_draws;
	tape.count = YT_ARRAY_LEN(no_shield_draws);
	tape.position = 0U;
	if (!yt_projectile_player_damage(&target, &remaining,
	    projectile_damage_draw, &tape, &damage, &error)
	    || tape.position != 3U || damage.iterations != 1U
	    || damage.fighters != 0.0 || damage.shields != 0.0f
	    || target.fighters != 1000.0f || target.shields != 100.0f)
		return false;

	memset(&target, 0, sizeof(target));
	target.fighters = 1.0f;
	target.shields = 1.0f;
	target.danger_scanner = 7.0f;
	remaining = 101.5f;
	tape.values = scanner_draws;
	tape.count = YT_ARRAY_LEN(scanner_draws);
	tape.position = 0U;
	if (!yt_projectile_player_damage(&target, &remaining,
	    projectile_damage_draw, &tape, &damage, &error)
	    || tape.position != 4U || damage.iterations != 1U
	    || remaining != 100.5f || !damage.scanner_disabled
	    || target.danger_scanner != 0.0f)
		return false;

	memset(&target, 0, sizeof(target));
	target.fighters = 10.0f;
	target.shields = 10.0f;
	target.danger_scanner = 1.0f;
	remaining = 1.0f;
	tape.values = no_shield_draws;
	tape.count = YT_ARRAY_LEN(no_shield_draws);
	tape.position = 0U;
	tape.fail_at = 1U;
	yt_error_clear(&error);
	if (yt_projectile_player_damage(&target, &remaining,
	    projectile_damage_draw, &tape, &damage, &error)
	    || tape.position != 2U || remaining != 0.0f
	    || target.fighters != 10.0f || target.shields != 10.0f
	    || error.status != YT_IO_ERROR
	    || strcmp(error.operation, "projectile RND") != 0)
		return false;

	memset(&target, 0, sizeof(target));
	target.fighters = 10.0f;
	target.shields = 10.0f;
	target.danger_scanner = INFINITY;
	remaining = 1.0f;
	tape.position = 0U;
	tape.fail_at = SIZE_MAX;
	yt_error_clear(&error);
	return !yt_projectile_player_damage(&target, &remaining,
	    projectile_damage_draw, &tape, &damage, &error)
	    && tape.position == 1U && remaining == 0.0f
	    && error.status == YT_RANGE
	    && strcmp(error.operation, "cruise missile scanner CINT") == 0;
}

static bool
check_projectile_persistence_model(void)
{
	static const uint8_t scanner_zero[4] = {
		0x00, 0x00, 0x48, 0x00
	};
	struct yt_player player;
	struct yt_sector sector;
	struct yt_planet planet;
	struct yt_record expected;
	const float production[3] = {1.25f, -2.5f, 3.75f};
	const float stock[3] = {4.5f, 5.5f, 6.5f};
	float saved_mines;
	size_t index;

	memset(&player, 0, sizeof(player));
	for (index = 0U; index < YT_RECORD_SIZE; ++index)
		player.record.bytes[index] = (uint8_t)(index ^ 0xa5U);
	expected = player.record;
	if (!yt_record_set_number(&expected, YT_F53, 12.5f)
	    || !yt_record_set_number(&expected, YT_F61, 7.25f)
	    || !yt_record_set_number(&expected, YT_F93, -0.5f)
	    || !yt_projectile_survivor_overlay(&player, 12.5f, 7.25,
	    -0.5f, false)
	    || player.shields != 12.5f || player.fighters != 7.25f
	    || player.danger_scanner != -0.5f
	    || memcmp(&player.record, &expected, sizeof(expected)) != 0)
		return false;

	memset(&player, 0, sizeof(player));
	for (index = 0U; index < YT_RECORD_SIZE; ++index)
		player.record.bytes[index] = (uint8_t)(index ^ 0x5aU);
	expected = player.record;
	if (!yt_record_set_number(&expected, YT_F53, 1.0f)
	    || !yt_record_set_number(&expected, YT_F61, 2.0f)
	    || !yt_record_set_raw_number(&expected, YT_F93, scanner_zero)
	    || !yt_projectile_survivor_overlay(&player, 1.0f, 2.0,
	    99.0f, true)
	    || player.danger_scanner != 0.0f
	    || memcmp(&player.record, &expected, sizeof(expected)) != 0)
		return false;

	memset(&player, 0, sizeof(player));
	for (index = 0U; index < YT_RECORD_SIZE; ++index)
		player.record.bytes[index] = (uint8_t)(index ^ 0x3cU);
	player.mines = -2.5f;
	expected = player.record;
	if (!yt_record_set_number(&expected, YT_F129, 0.0f)
	    || !yt_projectile_victim_mines_overlay(&player, &saved_mines)
	    || saved_mines != -2.5f || player.mines != 0.0f
	    || memcmp(&player.record, &expected, sizeof(expected)) != 0)
		return false;

	memset(&sector, 0, sizeof(sector));
	for (index = 0U; index < YT_RECORD_SIZE; ++index)
		sector.record.bytes[index] = (uint8_t)(index ^ 0xc3U);
	sector.mines = 1.25f;
	expected = sector.record;
	if (!yt_record_set_number(&expected, YT_F129, 1.75f)
	    || !yt_projectile_sector_mines_overlay(&sector, 0.5f)
	    || sector.mines != 1.75f
	    || memcmp(&sector.record, &expected, sizeof(expected)) != 0
	    || yt_projectile_physical_record(10.75f, 0.5f) != 11U
	    || yt_projectile_physical_record(16777216.0f, 1.0f)
	    != 0U)
		return false;

	memset(&planet, 0, sizeof(planet));
	for (index = 0U; index < YT_RECORD_SIZE; ++index)
		planet.record.bytes[index] = (uint8_t)(index ^ 0x96U);
	expected = planet.record;
	if (!yt_record_set_number(&expected, YT_F77, 8.5f)
	    || !yt_record_set_number(&expected, YT_F73, -2.0f)
	    || !yt_projectile_planet_ground_overlay(&planet, 8.5f, -2.0f)
	    || memcmp(&planet.record, &expected, sizeof(expected)) != 0)
		return false;

	expected = planet.record;
	for (index = 0U; index < 3U; ++index) {
		if (!yt_record_set_number(&expected, YT_F45 + index * 4U,
		    production[index])
		    || !yt_record_set_number(&expected, YT_F57 + index * 4U,
		    stock[index]))
			return false;
	}
	if (!yt_projectile_planet_productivity_overlay(&planet, production,
	    stock)
	    || memcmp(&planet.record, &expected, sizeof(expected)) != 0)
		return false;

	{
		static const uint8_t link_zero[4] = {
			0x00, 0x00, 0x20, 0x00
		};

		expected = planet.record;
		if (!yt_record_set_raw_number(&expected, YT_F85, link_zero)
		    || !yt_projectile_planet_destroy_overlay(&planet)
		    || memcmp(&planet.record, &expected, sizeof(expected)) != 0)
			return false;
		memset(&sector, 0, sizeof(sector));
		for (index = 0U; index < YT_RECORD_SIZE; ++index)
			sector.record.bytes[index] = (uint8_t)(index ^ 0x69U);
		expected = sector.record;
		if (!yt_record_set_raw_number(&expected, YT_F93, link_zero)
		    || !yt_projectile_sector_unlink_overlay(&sector)
		    || memcmp(&sector.record, &expected, sizeof(expected)) != 0)
			return false;
	}
	return true;
}

static bool
check_projectile_planet_damage_model(void)
{
	static const float ground_draws[] = {0.5f, 0.5f};
	static const float productivity_draws[] = {
		0.001f, 0.002f, 0.003f
	};
	static const float clamp_draws[] = {0.01f, 0.01f, 0.01f};
	struct projectile_damage_tape tape;
	struct yt_projectile_ground_result ground;
	struct yt_projectile_productivity_result productivity;
	struct yt_error error;
	float production[3];
	float stock[3];
	float remaining;

	tape.values = ground_draws;
	tape.count = YT_ARRAY_LEN(ground_draws);
	tape.position = 0U;
	tape.fail_at = SIZE_MAX;
	remaining = 2.0f;
	if (!yt_projectile_planet_ground_damage(20.0f, 7.0f, &remaining,
	    projectile_damage_draw, &tape, &ground, &error)
	    || tape.position != 2U || ground.iterations != 2U
	    || ground.ground != 0.0f || ground.owner != 0.0f
	    || remaining != 0.0f)
		return false;
	tape.position = 0U;
	remaining = 3.0f;
	if (!yt_projectile_planet_ground_damage(-2.5f, 7.0f, &remaining,
	    projectile_damage_draw, &tape, &ground, &error)
	    || tape.position != 0U || ground.iterations != 0U
	    || ground.ground != 0.0f || ground.owner != 0.0f
	    || remaining != 3.0f)
		return false;

	memset(production, 0, sizeof(production));
	stock[0] = stock[1] = stock[2] = 5.0f;
	tape.values = productivity_draws;
	tape.count = YT_ARRAY_LEN(productivity_draws);
	tape.position = 0U;
	remaining = 1.0f;
	if (!yt_projectile_planet_productivity_damage(1.0f, production,
	    stock, &remaining, projectile_damage_draw, &tape, &productivity,
	    &error)
	    || tape.position != 3U || productivity.iterations != 1U
	    || productivity.old_total != 0.0f
	    || productivity.new_total != 0.0f || remaining != 0.0f
	    || production[0] != 0.0f || production[1] != 0.0f
	    || production[2] != 0.0f || stock[0] != 0.0f
	    || stock[1] != 0.0f || stock[2] != 0.0f)
		return false;

	production[0] = 0.0f;
	production[1] = 100.0f;
	production[2] = 0.0f;
	stock[0] = 999.0f;
	stock[1] = 800.0f;
	stock[2] = 1.0f;
	tape.values = clamp_draws;
	tape.count = YT_ARRAY_LEN(clamp_draws);
	tape.position = 0U;
	remaining = 1.0f;
	if (!yt_projectile_planet_productivity_damage(0.0f, production,
	    stock, &remaining, projectile_damage_draw, &tape, &productivity,
	    &error)
	    || productivity.old_total != 100.0f
	    || productivity.new_total != 80.0f
	    || stock[0] != 0.0f || stock[1] != 800.0f
	    || stock[2] != 0.0f)
		return false;

	production[0] = 10.0f;
	production[1] = 20.0f;
	production[2] = 30.0f;
	stock[0] = stock[1] = stock[2] = 0.0f;
	tape.position = 0U;
	tape.fail_at = 1U;
	remaining = 1.0f;
	yt_error_clear(&error);
	return !yt_projectile_planet_productivity_damage(0.0f, production,
	    stock, &remaining, projectile_damage_draw, &tape, &productivity,
	    &error)
	    && tape.position == 2U && production[0] == -10.0f
	    && production[1] == 20.0f && production[2] == 30.0f
	    && remaining == 1.0f && error.status == YT_IO_ERROR;
}

enum projectile_opening_event {
	PROJECTILE_OPENING_SOUND = 1,
	PROJECTILE_OPENING_PRESENT,
	PROJECTILE_OPENING_WAIT,
};

struct projectile_opening_tape {
	int events[5];
	float scratch_at_event[5];
	enum yt_projectile_opening_output_kind kinds[4];
	uint8_t text[4][64];
	size_t lengths[4];
	size_t event_count;
	size_t present_count;
	size_t fail_at;
	float *scratch;
	float selector;
};

static bool
projectile_opening_step(struct projectile_opening_tape *tape, int event)
{
	size_t position = tape->event_count++;

	if (position >= YT_ARRAY_LEN(tape->events))
		return false;
	tape->events[position] = event;
	tape->scratch_at_event[position] = *tape->scratch;
	return tape->event_count != tape->fail_at;
}

static bool
projectile_opening_sound(void *context, float selector,
    struct yt_error *error)
{
	struct projectile_opening_tape *tape = context;

	(void)error;
	tape->selector = selector;
	return projectile_opening_step(tape, PROJECTILE_OPENING_SOUND);
}

static bool
projectile_opening_present(void *context, const uint8_t *text, size_t length,
    enum yt_projectile_opening_output_kind kind, struct yt_error *error)
{
	struct projectile_opening_tape *tape = context;
	size_t position = tape->present_count++;

	(void)error;
	if (!projectile_opening_step(tape, PROJECTILE_OPENING_PRESENT)
	    || position >= YT_ARRAY_LEN(tape->text)
	    || length > sizeof(tape->text[position]))
		return false;
	tape->kinds[position] = kind;
	if (length > 0U)
		memcpy(tape->text[position], text, length);
	tape->lengths[position] = length;
	return true;
}

static bool
check_projectile_cruise_opening_transaction(void)
{
	static const struct yt_projectile_cruise_opening_ops ops = {
		projectile_opening_sound,
		projectile_opening_present,
	};
	static const int expected_events[] = {
		PROJECTILE_OPENING_SOUND,
		PROJECTILE_OPENING_PRESENT,
		PROJECTILE_OPENING_PRESENT,
		PROJECTILE_OPENING_PRESENT,
		PROJECTILE_OPENING_PRESENT,
	};
	static const float expected_scratch[] = {
		77.0f, 77.0f, 77.0f, 77.0f, 0.0f,
	};
	static const enum yt_projectile_opening_output_kind expected_kinds[] = {
		YT_PROJECTILE_OPENING_DIRECT_LINE,
		YT_PROJECTILE_OPENING_RAW,
		YT_PROJECTILE_OPENING_DIRECT_LINE,
		YT_PROJECTILE_OPENING_DIRECT_LINE,
	};
	static const uint8_t loading[] =
	    "Loading course into misile targeting computer.";
	static const uint8_t tracking[] = "*** Tracking Report ***";
	struct projectile_opening_tape tape;
	float scratch;
	size_t failure;

	memset(&tape, 0, sizeof(tape));
	scratch = 77.0f;
	tape.scratch = &scratch;
	tape.fail_at = SIZE_MAX;
	if (!yt_projectile_cruise_opening_run(&scratch, &ops, &tape, NULL)
	    || tape.event_count != YT_ARRAY_LEN(expected_events)
	    || memcmp(tape.events, expected_events, sizeof(expected_events)) != 0
	    || memcmp(tape.scratch_at_event, expected_scratch,
	    sizeof(expected_scratch)) != 0
	    || tape.present_count != YT_ARRAY_LEN(expected_kinds)
	    || memcmp(tape.kinds, expected_kinds, sizeof(expected_kinds)) != 0
	    || tape.selector != 4.0f || scratch != 0.0f
	    || tape.lengths[0] != 0U
	    || tape.lengths[1] != sizeof(loading) - 1U
	    || memcmp(tape.text[1], loading, sizeof(loading) - 1U) != 0
	    || tape.lengths[2] != 0U
	    || tape.lengths[3] != sizeof(tracking) - 1U
	    || memcmp(tape.text[3], tracking, sizeof(tracking) - 1U) != 0)
		return false;

	for (failure = 1U; failure <= YT_ARRAY_LEN(expected_events); ++failure) {
		memset(&tape, 0, sizeof(tape));
		scratch = 77.0f;
		tape.scratch = &scratch;
		tape.fail_at = failure;
		if (yt_projectile_cruise_opening_run(&scratch, &ops, &tape, NULL)
		    || tape.event_count != failure
		    || memcmp(tape.events, expected_events,
		    failure * sizeof(expected_events[0])) != 0
		    || scratch != (failure == 5U ? 0.0f : 77.0f))
			return false;
	}
	return true;
}

struct plasma_opening_tape {
	int events[16];
	enum yt_projectile_opening_output_kind kinds[10];
	uint8_t text[10][192];
	size_t lengths[10];
	float selectors[3];
	float waits[2];
	size_t event_count;
	size_t present_count;
	size_t sound_count;
	size_t wait_count;
	size_t fail_at;
};

static bool
plasma_opening_step(struct plasma_opening_tape *tape, int event)
{
	size_t position = tape->event_count++;

	if (position >= YT_ARRAY_LEN(tape->events))
		return false;
	tape->events[position] = event;
	return tape->event_count != tape->fail_at;
}

static bool
plasma_opening_sound(void *context, float selector, struct yt_error *error)
{
	struct plasma_opening_tape *tape = context;
	size_t position = tape->sound_count++;

	(void)error;
	if (position >= YT_ARRAY_LEN(tape->selectors))
		return false;
	tape->selectors[position] = selector;
	return plasma_opening_step(tape, PROJECTILE_OPENING_SOUND);
}

static bool
plasma_opening_present(void *context, const uint8_t *text, size_t length,
    enum yt_projectile_opening_output_kind kind, struct yt_error *error)
{
	struct plasma_opening_tape *tape = context;
	size_t position = tape->present_count++;

	(void)error;
	if (position >= YT_ARRAY_LEN(tape->text)
	    || length > sizeof(tape->text[position]))
		return false;
	tape->kinds[position] = kind;
	if (length != 0U)
		memcpy(tape->text[position], text, length);
	tape->lengths[position] = length;
	return plasma_opening_step(tape, PROJECTILE_OPENING_PRESENT);
}

static bool
plasma_opening_wait(void *context, float duration, struct yt_error *error)
{
	struct plasma_opening_tape *tape = context;
	size_t position = tape->wait_count++;

	(void)error;
	if (position >= YT_ARRAY_LEN(tape->waits))
		return false;
	tape->waits[position] = duration;
	return plasma_opening_step(tape, PROJECTILE_OPENING_WAIT);
}

static bool
check_projectile_plasma_opening_transaction(void)
{
	static const uint8_t oversized_name[YT_PROJECTILE_ATTACKER_CAPACITY + 1U]
	    = {0};
	static const struct yt_projectile_plasma_opening_ops ops = {
		plasma_opening_sound,
		plasma_opening_present,
		plasma_opening_wait,
	};
	static const int expected_events[] = {
		PROJECTILE_OPENING_PRESENT,
		PROJECTILE_OPENING_PRESENT,
		PROJECTILE_OPENING_PRESENT,
		PROJECTILE_OPENING_SOUND,
		PROJECTILE_OPENING_WAIT,
		PROJECTILE_OPENING_PRESENT,
		PROJECTILE_OPENING_PRESENT,
		PROJECTILE_OPENING_WAIT,
		PROJECTILE_OPENING_PRESENT,
		PROJECTILE_OPENING_SOUND,
		PROJECTILE_OPENING_PRESENT,
		PROJECTILE_OPENING_SOUND,
		PROJECTILE_OPENING_PRESENT,
		PROJECTILE_OPENING_PRESENT,
		PROJECTILE_OPENING_PRESENT,
	};
	static const enum yt_projectile_opening_output_kind expected_kinds[] = {
		YT_PROJECTILE_OPENING_DIRECT_LINE,
		YT_PROJECTILE_OPENING_RAW,
		YT_PROJECTILE_OPENING_DIRECT_LINE,
		YT_PROJECTILE_OPENING_DIRECT_LINE,
		YT_PROJECTILE_OPENING_DIRECT_LINE,
		YT_PROJECTILE_OPENING_DIRECT_LINE,
		YT_PROJECTILE_OPENING_DIRECT_LINE,
		YT_PROJECTILE_OPENING_DIRECT_LINE,
		YT_PROJECTILE_OPENING_DIRECT_LINE,
		YT_PROJECTILE_OPENING_DIRECT_LINE,
	};
	static const char *const expected_text[] = {
		"",
		"Loading course into targeting computer.",
		"",
		"Plasma bolts targeted... firing 5000000 megawatts!",
		"",
		"Firing 1!",
		"Firing 2!",
		"",
		"* Tracking Report *",
		"",
	};
	struct yt_projectile_plasma_opening_state state;
	struct plasma_opening_tape tape;
	size_t failure;
	size_t index;

	memset(&state, 0, sizeof(state));
	state.bolts = 2.0f;
	state.player_name = (const uint8_t *)"ACE";
	state.player_name_length = 3U;
	memset(&tape, 0, sizeof(tape));
	tape.fail_at = SIZE_MAX;
	if (!yt_projectile_plasma_opening_run(&state, &ops, &tape, NULL)
	    || tape.event_count != YT_ARRAY_LEN(expected_events)
	    || memcmp(tape.events, expected_events, sizeof(expected_events)) != 0
	    || tape.present_count != YT_ARRAY_LEN(expected_kinds)
	    || memcmp(tape.kinds, expected_kinds, sizeof(expected_kinds)) != 0
	    || tape.sound_count != 3U || tape.selectors[0] != 4.0f
	    || tape.selectors[1] != 7.0f || tape.selectors[2] != 7.0f
	    || tape.wait_count != 2U || tape.waits[0] != 1.0f
	    || tape.waits[1] != 1.0f || state.energy != 5000000.0
	    || state.hop_loss != 100000.0f || state.firing_counter != 3.0f
	    || state.attacker_length != 3U
	    || memcmp(state.attacker, "ACE", 3U) != 0)
		return false;
	for (index = 0U; index < YT_ARRAY_LEN(expected_text); ++index) {
		size_t length = strlen(expected_text[index]);

		if (tape.lengths[index] != length
		    || memcmp(tape.text[index], expected_text[index], length) != 0)
			return false;
	}

	memset(&state, 0, sizeof(state));
	state.special_attacker = -1.0f;
	state.bolts = 1.5f;
	state.player_name = (const uint8_t *)"ignored";
	state.player_name_length = 7U;
	memset(&tape, 0, sizeof(tape));
	tape.fail_at = SIZE_MAX;
	if (!yt_projectile_plasma_opening_run(&state, &ops, &tape, NULL)
	    || state.attacker_length != strlen("The Mercenary")
	    || memcmp(state.attacker, "The Mercenary",
	    strlen("The Mercenary")) != 0
	    || state.energy != 3750000.0 || state.hop_loss != 75000.0f
	    || state.firing_counter != 2.0f || tape.sound_count != 2U
	    || tape.present_count != 9U)
		return false;

	memset(&state, 0, sizeof(state));
	state.bolts = 1.1f;
	state.player_name = (const uint8_t *)"A";
	state.player_name_length = 1U;
	memset(&tape, 0, sizeof(tape));
	tape.fail_at = SIZE_MAX;
	if (!yt_projectile_plasma_opening_run(&state, &ops, &tape, NULL)
	    || state.energy != (double)(float)(2500000.0f * 1.1f)
	    || state.hop_loss != (float)(state.energy / 50.0))
		return false;

	for (failure = 1U; failure <= YT_ARRAY_LEN(expected_events); ++failure) {
		memset(&state, 0, sizeof(state));
		state.bolts = 2.0f;
		state.player_name = (const uint8_t *)"ACE";
		state.player_name_length = 3U;
		memset(&tape, 0, sizeof(tape));
		tape.fail_at = failure;
		if (yt_projectile_plasma_opening_run(&state, &ops, &tape, NULL)
		    || tape.event_count != failure
		    || memcmp(tape.events, expected_events,
		    failure * sizeof(expected_events[0])) != 0
		    || state.energy != 5000000.0
		    || state.hop_loss != 100000.0f)
			return false;
	}

	memset(&state, 0, sizeof(state));
	state.player_name = oversized_name;
	state.player_name_length = YT_PROJECTILE_ATTACKER_CAPACITY + 1U;
	return !yt_projectile_plasma_opening_run(NULL, &ops, &tape, NULL)
	    && !yt_projectile_plasma_opening_run(&state, &ops, &tape, NULL);
}

enum plasma_route_event {
	PLASMA_ROUTE_BUILD = 1,
	PLASMA_ROUTE_LINE,
	PLASMA_ROUTE_WAIT,
	PLASMA_ROUTE_RANDOM,
	PLASMA_ROUTE_ATTENTION,
	PLASMA_ROUTE_IMPACT,
	PLASMA_ROUTE_FOOTER,
};

struct plasma_route_tape {
	int events[16];
	uint8_t lines[4][192];
	size_t line_lengths[4];
	uint8_t attention[192];
	size_t attention_length;
	size_t event_count;
	size_t line_count;
	size_t build_count;
	size_t impact_count;
	size_t fail_at;
	bool empty_route;
	bool impact_footer;
	double impact_energy;
	float draw;
	float wait_duration;
	int impact_hop;
};

static bool
plasma_route_step(struct plasma_route_tape *tape, int event)
{
	size_t position = tape->event_count++;

	if (position >= YT_ARRAY_LEN(tape->events))
		return false;
	tape->events[position] = event;
	return tape->event_count != tape->fail_at;
}

static bool
plasma_route_build(void *context, float origin, float destination,
    int16_t *route, size_t route_capacity, float *status,
    struct yt_error *error)
{
	struct plasma_route_tape *tape = context;
	int from = (int)origin;
	int to = (int)destination;
	bool empty;

	(void)error;
	++tape->build_count;
	if (!plasma_route_step(tape, PLASMA_ROUTE_BUILD))
		return false;
	if (from < 0 || to < 0 || (size_t)from >= route_capacity
	    || (size_t)to >= route_capacity)
		return false;
	memset(route, 0, route_capacity * sizeof(*route));
	empty = tape->empty_route;
	*status = empty ? 1.0f : 0.0f;
	if (!empty) {
		route[from] = (int16_t)to;
		route[to] = 0;
	}
	return true;
}

static bool
plasma_route_line(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	struct plasma_route_tape *tape = context;
	size_t position = tape->line_count++;

	(void)error;
	if (position >= YT_ARRAY_LEN(tape->lines)
	    || length > sizeof(tape->lines[position]))
		return false;
	if (length != 0U)
		memcpy(tape->lines[position], text, length);
	tape->line_lengths[position] = length;
	return plasma_route_step(tape, PLASMA_ROUTE_LINE);
}

static bool
plasma_route_attention(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	struct plasma_route_tape *tape = context;

	(void)error;
	if (length > sizeof(tape->attention))
		return false;
	if (length != 0U)
		memcpy(tape->attention, text, length);
	tape->attention_length = length;
	return plasma_route_step(tape, PLASMA_ROUTE_ATTENTION);
}

static bool
plasma_route_wait(void *context, float duration, struct yt_error *error)
{
	struct plasma_route_tape *tape = context;

	(void)error;
	tape->wait_duration = duration;
	return plasma_route_step(tape, PLASMA_ROUTE_WAIT);
}

static bool
plasma_route_random(void *context, float *value, struct yt_error *error)
{
	struct plasma_route_tape *tape = context;

	(void)error;
	if (!plasma_route_step(tape, PLASMA_ROUTE_RANDOM))
		return false;
	*value = tape->draw;
	return true;
}

static bool
plasma_route_impact(void *context, int hop, double *energy,
    enum yt_projectile_plasma_impact_route *route, struct yt_error *error)
{
	struct plasma_route_tape *tape = context;

	(void)error;
	++tape->impact_count;
	tape->impact_hop = hop;
	tape->impact_energy = *energy;
	if (!plasma_route_step(tape, PLASMA_ROUTE_IMPACT))
		return false;
	if (tape->impact_footer) {
		*energy = 0.0;
		*route = YT_PROJECTILE_PLASMA_FOOTER;
	}
	else
		*route = YT_PROJECTILE_PLASMA_NEXT_HOP;
	return true;
}

static bool
plasma_route_footer(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	(void)text;
	(void)length;
	(void)error;
	return plasma_route_step(context, PLASMA_ROUTE_FOOTER);
}

static void
plasma_route_fixture(struct yt_projectile_plasma_route_state *state,
    struct plasma_route_tape *tape, int16_t *route, float *origin,
    float *destination, double *energy)
{
	memset(tape, 0, sizeof(*tape));
	tape->fail_at = SIZE_MAX;
	tape->draw = 0.5f;
	memset(route, 0, 2048U * sizeof(*route));
	*origin = 7.0f;
	*destination = 8.0f;
	*energy = 1000.0;
	memset(state, 0, sizeof(*state));
	state->origin = origin;
	state->destination = destination;
	state->energy = energy;
	state->hop_loss = 100.0f;
	state->black_hole[0] = 1999.0f;
	state->black_hole[1] = 1998.0f;
	state->sector_record_offset = 51.0f;
	state->port_record_offset = 2055.0f;
	state->route = route;
	state->route_capacity = 2048U;
	state->step_limit = 32U;
}

static bool
check_projectile_plasma_route_transaction(void)
{
	static const struct yt_projectile_plasma_route_ops ops = {
		plasma_route_build,
		plasma_route_line,
		plasma_route_attention,
		plasma_route_wait,
		plasma_route_random,
		plasma_route_impact,
		plasma_route_footer,
	};
	static const int ordinary_events[] = {
		PLASMA_ROUTE_BUILD, PLASMA_ROUTE_LINE, PLASMA_ROUTE_WAIT,
		PLASMA_ROUTE_IMPACT, PLASMA_ROUTE_FOOTER,
	};
	static const int same_events[] = {
		PLASMA_ROUTE_LINE, PLASMA_ROUTE_WAIT, PLASMA_ROUTE_IMPACT,
		PLASMA_ROUTE_FOOTER,
	};
	static const int black_events[] = {
		PLASMA_ROUTE_LINE, PLASMA_ROUTE_WAIT, PLASMA_ROUTE_RANDOM,
		PLASMA_ROUTE_LINE, PLASMA_ROUTE_ATTENTION, PLASMA_ROUTE_LINE,
		PLASMA_ROUTE_BUILD, PLASMA_ROUTE_FOOTER,
	};
	static const uint8_t hop_row[] =
	    "Bolt entering sector 8. 1000 Megawatts remaining.";
	static const uint8_t same_row[] =
	    "Bolt entering sector 7. 1000 Megawatts remaining.";
	static const uint8_t black_row[] =
	    "The plasma bolt is deflected by a black hole in sector 7 to "
	    "sector 1003!";
	struct yt_projectile_plasma_route_state state;
	struct plasma_route_tape tape;
	int16_t route[2048];
	float origin;
	float destination;
	double energy;
	size_t failure;

	plasma_route_fixture(&state, &tape, route, &origin, &destination,
	    &energy);
	if (!yt_projectile_plasma_route_run(&state, &ops, &tape, NULL)
	    || tape.event_count != YT_ARRAY_LEN(ordinary_events)
	    || memcmp(tape.events, ordinary_events, sizeof(ordinary_events)) != 0
	    || state.route_calls != 1U || state.hops != 1U
	    || tape.impact_count != 1U || tape.impact_hop != 8
	    || tape.impact_energy != 1000.0 || energy != 900.0
	    || tape.wait_duration != 0.5f
	    || tape.line_lengths[0] != sizeof(hop_row) - 1U
	    || memcmp(tape.lines[0], hop_row, sizeof(hop_row) - 1U) != 0)
		return false;

	plasma_route_fixture(&state, &tape, route, &origin, &destination,
	    &energy);
	destination = origin;
	if (!yt_projectile_plasma_route_run(&state, &ops, &tape, NULL)
	    || tape.event_count != YT_ARRAY_LEN(same_events)
	    || memcmp(tape.events, same_events, sizeof(same_events)) != 0
	    || origin != 0.0f || route[0] != 7 || route[7] != 0
	    || state.route_calls != 0U || state.hops != 1U || energy != 900.0
	    || tape.line_lengths[0] != sizeof(same_row) - 1U
	    || memcmp(tape.lines[0], same_row, sizeof(same_row) - 1U) != 0)
		return false;

	plasma_route_fixture(&state, &tape, route, &origin, &destination,
	    &energy);
	tape.empty_route = true;
	if (!yt_projectile_plasma_route_run(&state, &ops, &tape, NULL)
	    || tape.event_count != 2U || tape.events[0] != PLASMA_ROUTE_BUILD
	    || tape.events[1] != PLASMA_ROUTE_FOOTER || state.hops != 0U
	    || state.route_status != 1.0f || energy != 1000.0)
		return false;

	plasma_route_fixture(&state, &tape, route, &origin, &destination,
	    &energy);
	destination = origin;
	tape.impact_footer = true;
	if (!yt_projectile_plasma_route_run(&state, &ops, &tape, NULL)
	    || memcmp(tape.events, same_events, sizeof(same_events)) != 0
	    || energy != 0.0)
		return false;

	plasma_route_fixture(&state, &tape, route, &origin, &destination,
	    &energy);
	destination = origin;
	state.black_hole[0] = origin;
	tape.empty_route = true;
	if (!yt_projectile_plasma_route_run(&state, &ops, &tape, NULL)
	    || tape.event_count != YT_ARRAY_LEN(black_events)
	    || memcmp(tape.events, black_events, sizeof(black_events)) != 0
	    || origin != 7.0f || destination != 1003.0f
	    || state.route_calls != 1U || state.hops != 1U || energy != 1000.0
	    || tape.attention_length != sizeof(black_row) - 1U
	    || memcmp(tape.attention, black_row, sizeof(black_row) - 1U) != 0
	    || tape.line_count != 3U || tape.line_lengths[1] != 0U
	    || tape.line_lengths[2] != 0U)
		return false;

	for (failure = 1U; failure <= YT_ARRAY_LEN(ordinary_events); ++failure) {
		plasma_route_fixture(&state, &tape, route, &origin, &destination,
		    &energy);
		tape.fail_at = failure;
		if (yt_projectile_plasma_route_run(&state, &ops, &tape, NULL)
		    || tape.event_count != failure
		    || memcmp(tape.events, ordinary_events,
		    failure * sizeof(ordinary_events[0])) != 0)
			return false;
	}
	for (failure = 1U; failure <= YT_ARRAY_LEN(black_events); ++failure) {
		plasma_route_fixture(&state, &tape, route, &origin, &destination,
		    &energy);
		destination = origin;
		state.black_hole[0] = origin;
		tape.empty_route = true;
		tape.fail_at = failure;
		if (yt_projectile_plasma_route_run(&state, &ops, &tape, NULL)
		    || tape.event_count != failure
		    || memcmp(tape.events, black_events,
		    failure * sizeof(black_events[0])) != 0
		    || (failure == 3U && (origin != 7.0f
		    || destination != 7.0f)))
			return false;
	}

	plasma_route_fixture(&state, &tape, route, &origin, &destination,
	    &energy);
	state.step_limit = 1U;
	return !yt_projectile_plasma_route_run(NULL, &ops, &tape, NULL)
	    && !yt_projectile_plasma_route_run(&state, &ops, &tape, NULL);
}

struct projectile_route_entry_tape {
	struct yt_player player;
	int requested_record;
	size_t reads;
	bool succeeds;
};

static bool
projectile_route_entry_read(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct projectile_route_entry_tape *tape = context;

	(void)error;
	++tape->reads;
	tape->requested_record = player_record;
	if (!tape->succeeds)
		return false;
	*player = tape->player;
	return true;
}

static bool
check_projectile_route_entry_transaction(void)
{
	struct projectile_route_entry_tape tape;
	struct yt_projectile_route_entry_state state;
	static const int skipped_shooters[] = {-1, 2, 52};
	static const float mapped_teams[] = {0.0f, 0.5f, -2.0f};
	size_t index;

	memset(&tape, 0, sizeof(tape));
	tape.succeeds = true;
	tape.player.team = 4.0f;
	state.shooter = 3;
	state.maximum_player_record = 51.0f;
	state.start = 7.0f;
	state.current_hop = -1.0f;
	state.shooter_team = 99.0f;
	if (!yt_projectile_route_entry_run(&state, projectile_route_entry_read,
	    &tape, NULL)
	    || tape.reads != 1U || tape.requested_record != 3
	    || state.current_hop != 7.0f || state.shooter_team != 4.0f)
		return false;

	for (index = 0U; index < YT_ARRAY_LEN(skipped_shooters); ++index) {
		memset(&tape, 0, sizeof(tape));
		tape.succeeds = true;
		state.shooter = skipped_shooters[index];
		state.maximum_player_record = 51.0f;
		state.start = 1.5f;
		state.current_hop = -1.0f;
		state.shooter_team = 99.0f;
		if (!yt_projectile_route_entry_run(&state,
		    projectile_route_entry_read, &tape, NULL)
		    || tape.reads != 0U || state.current_hop != 1.5f
		    || state.shooter_team != -99999.0f)
			return false;
	}

	state.shooter = 3;
	state.maximum_player_record = 3.0f;
	for (index = 0U; index < YT_ARRAY_LEN(mapped_teams); ++index) {
		memset(&tape, 0, sizeof(tape));
		tape.succeeds = true;
		tape.player.team = mapped_teams[index];
		state.start = 9.0f;
		if (!yt_projectile_route_entry_run(&state,
		    projectile_route_entry_read, &tape, NULL)
		    || tape.reads != 1U || tape.requested_record != 3
		    || state.current_hop != 9.0f
		    || state.shooter_team != -99999.0f)
			return false;
	}

	memset(&tape, 0, sizeof(tape));
	tape.succeeds = true;
	tape.player.team = 1.0f;
	state.shooter = 3;
	state.maximum_player_record = 2.999f;
	if (!yt_projectile_route_entry_run(&state, projectile_route_entry_read,
	    &tape, NULL) || tape.reads != 0U
	    || state.shooter_team != -99999.0f)
		return false;

	memset(&tape, 0, sizeof(tape));
	tape.succeeds = true;
	tape.player.team = NAN;
	state.maximum_player_record = 3.0f;
	if (!yt_projectile_route_entry_run(&state, projectile_route_entry_read,
	    &tape, NULL) || tape.reads != 1U || !isnan(state.shooter_team))
		return false;

	memset(&tape, 0, sizeof(tape));
	tape.succeeds = false;
	state.start = 11.0f;
	state.current_hop = -1.0f;
	state.shooter_team = 99.0f;
	return !yt_projectile_route_entry_run(&state,
	    projectile_route_entry_read, &tape, NULL)
	    && tape.reads == 1U && tape.requested_record == 3
	    && state.current_hop == 11.0f && state.shooter_team == 0.0f;
}

enum projectile_reroute_event {
	PROJECTILE_REROUTE_LINE = 1,
	PROJECTILE_REROUTE_ATTENTION,
	PROJECTILE_REROUTE_RANDOM,
};

struct projectile_reroute_tape {
	int events[3];
	float origin_at_event[3];
	float destination_at_event[3];
	uint8_t line[192];
	size_t line_length;
	uint8_t attention[192];
	size_t attention_length;
	size_t event_count;
	size_t fail_at;
	float draw;
	float *origin;
	float *destination;
};

static bool
projectile_reroute_step(struct projectile_reroute_tape *tape, int event)
{
	size_t position = tape->event_count++;

	if (position >= YT_ARRAY_LEN(tape->events))
		return false;
	tape->events[position] = event;
	tape->origin_at_event[position] = *tape->origin;
	tape->destination_at_event[position] = *tape->destination;
	return tape->event_count != tape->fail_at;
}

static bool
projectile_reroute_line(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	struct projectile_reroute_tape *tape = context;

	(void)error;
	if (!projectile_reroute_step(tape, PROJECTILE_REROUTE_LINE)
	    || length > sizeof(tape->line))
		return false;
	if (length > 0U)
		memcpy(tape->line, text, length);
	tape->line_length = length;
	return true;
}

static bool
projectile_reroute_attention(void *context, const uint8_t *text,
    size_t length, struct yt_error *error)
{
	struct projectile_reroute_tape *tape = context;

	(void)error;
	if (!projectile_reroute_step(tape, PROJECTILE_REROUTE_ATTENTION)
	    || length > sizeof(tape->attention))
		return false;
	memcpy(tape->attention, text, length);
	tape->attention_length = length;
	return true;
}

static bool
projectile_reroute_random(void *context, float *value,
    struct yt_error *error)
{
	struct projectile_reroute_tape *tape = context;

	(void)error;
	if (!projectile_reroute_step(tape, PROJECTILE_REROUTE_RANDOM))
		return false;
	*value = tape->draw;
	return true;
}

static void
projectile_reroute_fixture(struct projectile_reroute_tape *tape,
    struct yt_projectile_cruise_reroute_state *state, float *origin,
    float *destination)
{
	memset(tape, 0, sizeof(*tape));
	*origin = 1.0f;
	*destination = 9.0f;
	tape->fail_at = SIZE_MAX;
	tape->draw = 0.5f;
	tape->origin = origin;
	tape->destination = destination;
	state->hop = 3.0f;
	state->sector_record_offset = 51.0f;
	state->port_record_offset = 2055.0f;
	state->origin = origin;
	state->destination = destination;
}

static bool
check_projectile_cruise_reroute_transaction(void)
{
	static const struct yt_projectile_cruise_reroute_ops ops = {
		projectile_reroute_line,
		projectile_reroute_attention,
		projectile_reroute_random,
	};
	static const int expected_events[] = {
		PROJECTILE_REROUTE_LINE,
		PROJECTILE_REROUTE_ATTENTION,
		PROJECTILE_REROUTE_RANDOM,
	};
	static const float expected_origins[] = {1.0f, 1.0f, 3.0f};
	static const float expected_destinations[] = {9.0f, 9.0f, 9.0f};
	static const uint8_t expected_attention[] =
	    "The missiles are deflected by a black hole in sector 3!";
	struct projectile_reroute_tape tape;
	struct yt_projectile_cruise_reroute_state state;
	float origin;
	float destination;
	size_t failure;

	if (!yt_projectile_is_black_hole(3.0f, 3.0f, 4.0f)
	    || !yt_projectile_is_black_hole(4.0f, 3.0f, 4.0f)
	    || yt_projectile_is_black_hole(5.0f, 3.0f, 4.0f)
	    || yt_projectile_is_black_hole(NAN, NAN, 4.0f))
		return false;
	projectile_reroute_fixture(&tape, &state, &origin, &destination);
	if (!yt_projectile_cruise_reroute_run(&state, &ops, &tape, NULL)
	    || tape.event_count != YT_ARRAY_LEN(expected_events)
	    || memcmp(tape.events, expected_events, sizeof(expected_events)) != 0
	    || memcmp(tape.origin_at_event, expected_origins,
	    sizeof(expected_origins)) != 0
	    || memcmp(tape.destination_at_event, expected_destinations,
	    sizeof(expected_destinations)) != 0
	    || tape.line_length != 0U
	    || tape.attention_length != sizeof(expected_attention) - 1U
	    || memcmp(tape.attention, expected_attention,
	    sizeof(expected_attention) - 1U) != 0
	    || origin != 3.0f || destination != 1003.0f)
		return false;

	for (failure = 1U; failure <= YT_ARRAY_LEN(expected_events); ++failure) {
		projectile_reroute_fixture(&tape, &state, &origin, &destination);
		tape.fail_at = failure;
		if (yt_projectile_cruise_reroute_run(&state, &ops, &tape, NULL)
		    || tape.event_count != failure
		    || memcmp(tape.events, expected_events,
		    failure * sizeof(expected_events[0])) != 0
		    || origin != (failure == 3U ? 3.0f : 1.0f)
		    || destination != 9.0f)
			return false;
	}
	return true;
}

struct projectile_union_police_tape {
	uint8_t row[64];
	size_t length;
	size_t calls;
	bool succeeds;
};

static bool
projectile_union_police_present(void *context, const uint8_t *text,
    size_t length, struct yt_error *error)
{
	struct projectile_union_police_tape *tape = context;

	(void)error;
	++tape->calls;
	if (!tape->succeeds || length > sizeof(tape->row))
		return false;
	memcpy(tape->row, text, length);
	tape->length = length;
	return true;
}

static bool
check_projectile_union_police_transaction(void)
{
	static const uint8_t expected[] =
	    "The Union Police have destroyed the Missiles!";
	static const struct {
		float hop;
		float destination;
		int counterattack;
		int xannor_provoker;
		bool admitted;
	} cases[] = {
		{7.999f, 7.5f, 0, 0, true},
		{-1.0f, -2.0f, 0, 0, true},
		{8.0f, 7.0f, 0, 0, false},
		{7.0f, 8.0f, 0, 0, false},
		{NAN, 7.0f, 0, 0, false},
		{7.0f, NAN, 0, 0, false},
		{7.0f, 7.0f, 1, 0, false},
		{7.0f, 7.0f, -1, 0, false},
		{7.0f, 7.0f, 0, 1, false},
		{7.0f, 7.0f, 0, -1, false},
	};
	struct projectile_union_police_tape tape;
	struct yt_projectile_union_police_state state;
	size_t index;

	for (index = 0U; index < YT_ARRAY_LEN(cases); ++index) {
		memset(&tape, 0, sizeof(tape));
		tape.succeeds = true;
		state.hop = cases[index].hop;
		state.destination = cases[index].destination;
		state.counterattack = cases[index].counterattack;
		state.xannor_provoker = cases[index].xannor_provoker;
		state.intercepted = true;
		if (yt_projectile_union_police_admitted(state.hop,
		    state.destination, state.counterattack,
		    state.xannor_provoker) != cases[index].admitted
		    || !yt_projectile_union_police_run(&state,
		    projectile_union_police_present, &tape, NULL)
		    || state.intercepted != cases[index].admitted
		    || tape.calls != (cases[index].admitted ? 1U : 0U))
			return false;
		if (cases[index].admitted
		    && (tape.length != sizeof(expected) - 1U
		    || memcmp(tape.row, expected, sizeof(expected) - 1U) != 0))
			return false;
	}

	memset(&tape, 0, sizeof(tape));
	state.hop = 7.0f;
	state.destination = 7.0f;
	state.counterattack = 0;
	state.xannor_provoker = 0;
	state.intercepted = false;
	return !yt_projectile_union_police_run(&state,
	    projectile_union_police_present, &tape, NULL)
	    && tape.calls == 1U && state.intercepted;
}

static bool
check_projectile_sector_probe_transaction(void)
{
	struct yt_projectile_sector_probe_state state;
	struct yt_sector sector;
	struct yt_error error;
	float sector_cache[8] = {0};
	float cloak_cache[8] = {0};
	float *objects[] = {
		&sector.mines,
		&sector.fighters,
		&sector.port,
		&sector.planet,
	};
	size_t index;

	memset(&sector, 0, sizeof(sector));
	memset(&state, 0, sizeof(state));
	state.sector = &sector;
	state.hop = 17.0f;
	state.player_terminal = 3.5f;
	state.sector_cache = sector_cache;
	state.cloak_cache = cloak_cache;
	state.cache_count = YT_ARRAY_LEN(sector_cache);
	for (index = 0U; index < YT_ARRAY_LEN(objects); ++index) {
		memset(&sector, 0, sizeof(sector));
		*objects[index] = 0.001f;
		if (!yt_projectile_sector_probe_run(&state, NULL)
		    || state.presence != 1.0f || state.matched_player != 0.0f
		    || state.counter != 4.0f)
			return false;
	}

	sector.mines = -1.0f;
	sector.fighters = NAN;
	sector.port = -INFINITY;
	sector.planet = -0.0f;
	state.player_terminal = 1.9f;
	state.xannor_provoker = 0.0f;
	if (!yt_projectile_sector_probe_run(&state, NULL)
	    || state.presence != 0.0f || state.matched_player != 0.0f
	    || state.counter != 2.0f)
		return false;

	memset(&sector, 0, sizeof(sector));
	sector_cache[2] = 17.0f;
	state.player_terminal = 5.0f;
	if (!yt_projectile_sector_probe_run(&state, NULL)
	    || state.presence != 1.0f || state.matched_player != 2.0f
	    || state.counter != 2.0f)
		return false;

	sector_cache[2] = 0.0f;
	sector_cache[3] = 17.0f;
	cloak_cache[3] = -1.0f;
	state.player_terminal = 3.5f;
	if (!yt_projectile_sector_probe_run(&state, NULL)
	    || state.presence != 0.0f || state.matched_player != 0.0f
	    || state.counter != 4.0f)
		return false;
	state.xannor_provoker = 3.0f;
	if (!yt_projectile_sector_probe_run(&state, NULL)
	    || state.presence != 1.0f || state.matched_player != 3.0f
	    || state.counter != 3.0f)
		return false;

	state.player_terminal = NAN;
	state.xannor_provoker = 0.0f;
	if (!yt_projectile_sector_probe_run(&state, NULL)
	    || state.presence != 0.0f || state.counter != 2.0f)
		return false;

	state.player_terminal = 3.0f;
	state.cache_count = 3U;
	yt_error_clear(&error);
	if (yt_projectile_sector_probe_run(&state, &error)
	    || error.status != YT_RANGE
	    || strcmp(error.operation,
	    "projectile sector-probe cache index") != 0
	    || state.presence != 0.0f || state.counter != 3.0f)
		return false;

	sector.mines = 1.0f;
	state.player_terminal = 2.0f;
	state.cache_count = 2U;
	if (yt_projectile_sector_probe_run(&state, NULL)
	    || state.presence != 1.0f || state.counter != 2.0f)
		return false;
	return !yt_projectile_sector_probe_run(NULL, NULL);
}

enum plasma_fighter_event {
	PLASMA_FIGHTER_OWNER = 1,
	PLASMA_FIGHTER_ENCOUNTER,
	PLASMA_FIGHTER_SOUND,
	PLASMA_FIGHTER_RANDOM,
	PLASMA_FIGHTER_DAMAGE,
	PLASMA_FIGHTER_NEWS,
	PLASMA_FIGHTER_READ,
	PLASMA_FIGHTER_WRITE,
	PLASMA_FIGHTER_VICTORY,
};

struct plasma_fighter_tape {
	int events[16];
	size_t event_count;
	size_t fail_at;
	float draws[4];
	size_t draw_position;
	float owner_at_read;
	uint8_t owner_name[YT_TEXT_FIELD_SIZE];
	size_t owner_name_length;
	uint8_t encounter[256];
	size_t encounter_length;
	uint8_t damage[256];
	size_t damage_length;
	uint8_t news[256];
	size_t news_length;
	float selector;
	float *bold;
	float bold_at_sound;
	struct yt_sector source;
	struct yt_sector written;
	float sector_at_read;
	float sector_at_write;
};

static bool
plasma_fighter_step(struct plasma_fighter_tape *tape,
    enum plasma_fighter_event event)
{
	if (tape->event_count >= YT_ARRAY_LEN(tape->events))
		return false;
	tape->events[tape->event_count++] = (int)event;
	return tape->event_count != tape->fail_at;
}

static bool
plasma_fighter_test_owner(void *context, float owner, uint8_t *name,
    size_t *name_length, struct yt_error *error)
{
	struct plasma_fighter_tape *tape = context;

	(void)error;
	tape->owner_at_read = owner;
	if (!plasma_fighter_step(tape, PLASMA_FIGHTER_OWNER))
		return false;
	memcpy(name, tape->owner_name, tape->owner_name_length);
	*name_length = tape->owner_name_length;
	return true;
}

static bool
plasma_fighter_test_present(void *context, const uint8_t *text,
    size_t length, enum yt_projectile_plasma_fighter_output_kind kind,
    struct yt_error *error)
{
	struct plasma_fighter_tape *tape = context;
	uint8_t *target;
	size_t *target_length;
	enum plasma_fighter_event event;

	(void)error;
	if (kind == YT_PROJECTILE_PLASMA_FIGHTER_ENCOUNTER) {
		target = tape->encounter;
		target_length = &tape->encounter_length;
		event = PLASMA_FIGHTER_ENCOUNTER;
	}
	else {
		target = tape->damage;
		target_length = &tape->damage_length;
		event = PLASMA_FIGHTER_DAMAGE;
	}
	if (!plasma_fighter_step(tape, event) || length > 256U)
		return false;
	memcpy(target, text, length);
	*target_length = length;
	return true;
}

static bool
plasma_fighter_test_sound(void *context, float selector,
    struct yt_error *error)
{
	struct plasma_fighter_tape *tape = context;

	(void)error;
	tape->selector = selector;
	tape->bold_at_sound = *tape->bold;
	return plasma_fighter_step(tape, PLASMA_FIGHTER_SOUND);
}

static bool
plasma_fighter_test_random(void *context, float *value,
    struct yt_error *error)
{
	struct plasma_fighter_tape *tape = context;

	(void)error;
	if (!plasma_fighter_step(tape, PLASMA_FIGHTER_RANDOM)
	    || tape->draw_position >= YT_ARRAY_LEN(tape->draws))
		return false;
	*value = tape->draws[tape->draw_position++];
	return true;
}

static bool
plasma_fighter_test_news(void *context, const uint8_t *text,
    size_t length, struct yt_error *error)
{
	struct plasma_fighter_tape *tape = context;

	(void)error;
	if (!plasma_fighter_step(tape, PLASMA_FIGHTER_NEWS)
	    || length > sizeof(tape->news))
		return false;
	memcpy(tape->news, text, length);
	tape->news_length = length;
	return true;
}

static bool
plasma_fighter_test_read(void *context, float sector,
    struct yt_sector *value, struct yt_error *error)
{
	struct plasma_fighter_tape *tape = context;

	(void)error;
	tape->sector_at_read = sector;
	if (!plasma_fighter_step(tape, PLASMA_FIGHTER_READ))
		return false;
	*value = tape->source;
	return true;
}

static bool
plasma_fighter_test_write(void *context, float sector,
    const struct yt_sector *value, struct yt_error *error)
{
	struct plasma_fighter_tape *tape = context;

	(void)error;
	tape->sector_at_write = sector;
	tape->written = *value;
	return plasma_fighter_step(tape, PLASMA_FIGHTER_WRITE);
}

static bool
plasma_fighter_test_victory(void *context, struct yt_error *error)
{
	(void)error;
	return plasma_fighter_step(context, PLASMA_FIGHTER_VICTORY);
}

static void
plasma_fighter_fixture(struct plasma_fighter_tape *tape,
    struct yt_projectile_plasma_fighter_state *state, double *energy,
    float *bold)
{
	static const uint8_t attacker[] = {'A', 0, 'B'};

	memset(tape, 0, sizeof(*tape));
	memset(state, 0, sizeof(*state));
	memset(&tape->source, 0, sizeof(tape->source));
	memset(tape->source.record.bytes, 0xa5,
	    sizeof(tape->source.record.bytes));
	tape->source.fighters = 777.0f;
	tape->source.fighter_owner = -2.0f;
	(void)yt_record_set_number(&tape->source.record, YT_F81,
	    tape->source.fighters);
	(void)yt_record_set_number(&tape->source.record, YT_F85,
	    tape->source.fighter_owner);
	tape->fail_at = SIZE_MAX;
	tape->draws[0] = 0.5f;
	tape->owner_name[0] = 'R';
	tape->owner_name[1] = 0;
	tape->owner_name[2] = 'X';
	tape->owner_name_length = 3U;
	tape->bold = bold;
	*energy = 1.0;
	*bold = 0.0f;
	state->sector = 7.0f;
	state->fighters = 100.0;
	state->owner = -1.0f;
	state->shooter = 2;
	state->headquarters = 999.0f;
	state->attacker = attacker;
	state->attacker_length = sizeof(attacker);
	state->energy = energy;
	state->bold = bold;
}

static bool
check_projectile_plasma_fighter_transaction(void)
{
	static const struct yt_projectile_plasma_fighter_ops ops = {
		plasma_fighter_test_owner,
		plasma_fighter_test_present,
		plasma_fighter_test_sound,
		plasma_fighter_test_random,
		plasma_fighter_test_news,
		plasma_fighter_test_read,
		plasma_fighter_test_write,
		plasma_fighter_test_victory,
	};
	static const int low_energy_events[] = {
		PLASMA_FIGHTER_ENCOUNTER,
		PLASMA_FIGHTER_SOUND,
		PLASMA_FIGHTER_RANDOM,
		PLASMA_FIGHTER_DAMAGE,
		PLASMA_FIGHTER_READ,
		PLASMA_FIGHTER_WRITE,
	};
	static const int victory_events[] = {
		PLASMA_FIGHTER_OWNER,
		PLASMA_FIGHTER_ENCOUNTER,
		PLASMA_FIGHTER_SOUND,
		PLASMA_FIGHTER_RANDOM,
		PLASMA_FIGHTER_DAMAGE,
		PLASMA_FIGHTER_NEWS,
		PLASMA_FIGHTER_READ,
		PLASMA_FIGHTER_WRITE,
		PLASMA_FIGHTER_VICTORY,
	};
	static const uint8_t xannor_row[] =
	    "Sector: 7 defended by The Xannor with 100 fighters.";
	static const uint8_t mercenary_row[] =
	    "Sector: 7 defended by Mercenaries with 100 fighters.";
	static const uint8_t player_row[] =
	    "Sector: 7 defended by R\0X with 100 fighters.";
	static const uint8_t self_row[] =
	    "Sector: 7 defended by YOU with 10 fighters.";
	static const uint8_t damage_row[] =
	    "The plasma bolts destroyed 1 fighters!";
	static const uint8_t news_row[] =
	    "A\0B's plasma bolts destroyed 10 fighters in sector 7!";
	struct plasma_fighter_tape tape;
	struct yt_projectile_plasma_fighter_state state;
	struct yt_record expected;
	double energy;
	float bold;
	size_t failure;

	plasma_fighter_fixture(&tape, &state, &energy, &bold);
	expected = tape.source.record;
	(void)yt_record_set_number(&expected, YT_F81, 99.0f);
	if (!yt_projectile_plasma_fighter_run(&state, &ops, &tape, NULL)
	    || tape.event_count != YT_ARRAY_LEN(low_energy_events)
	    || memcmp(tape.events, low_energy_events,
	    sizeof(low_energy_events)) != 0
	    || state.destroyed != 1.0 || state.remaining_fighters != 99.0
	    || energy != 0.0
	    || state.route != YT_PROJECTILE_PLASMA_FIGHTER_FOOTER
	    || bold != 1.0f || tape.bold_at_sound != 1.0f
	    || tape.selector != 2.0f
	    || tape.encounter_length != sizeof(xannor_row) - 1U
	    || memcmp(tape.encounter, xannor_row, sizeof(xannor_row) - 1U) != 0
	    || tape.damage_length != sizeof(damage_row) - 1U
	    || memcmp(tape.damage, damage_row, sizeof(damage_row) - 1U) != 0
	    || tape.sector_at_read != 7.0f || tape.sector_at_write != 7.0f
	    || tape.written.fighters != 99.0f
	    || tape.written.fighter_owner != -2.0f
	    || memcmp(&tape.written.record, &expected, sizeof(expected)) != 0)
		return false;

	plasma_fighter_fixture(&tape, &state, &energy, &bold);
	state.fighters = 0.0;
	if (!yt_projectile_plasma_fighter_run(&state, &ops, &tape, NULL)
	    || tape.event_count != 0U || state.destroyed != 0.0
	    || state.remaining_fighters != 0.0
	    || state.route != YT_PROJECTILE_PLASMA_FIGHTER_CONTINUE_SECTOR)
		return false;

	plasma_fighter_fixture(&tape, &state, &energy, &bold);
	energy = 0.0;
	if (!yt_projectile_plasma_fighter_run(&state, &ops, &tape, NULL)
	    || tape.event_count != 2U
	    || tape.events[0] != PLASMA_FIGHTER_ENCOUNTER
	    || tape.events[1] != PLASMA_FIGHTER_SOUND
	    || tape.damage_length != 0U || tape.draw_position != 0U
	    || state.route != YT_PROJECTILE_PLASMA_FIGHTER_CONTINUE_SECTOR)
		return false;

	plasma_fighter_fixture(&tape, &state, &energy, &bold);
	state.owner = -2.0f;
	if (!yt_projectile_plasma_fighter_run(&state, &ops, &tape, NULL)
	    || tape.encounter_length != sizeof(mercenary_row) - 1U
	    || memcmp(tape.encounter, mercenary_row,
	    sizeof(mercenary_row) - 1U) != 0)
		return false;

	plasma_fighter_fixture(&tape, &state, &energy, &bold);
	state.owner = 3.0f;
	if (!yt_projectile_plasma_fighter_run(&state, &ops, &tape, NULL)
	    || tape.owner_at_read != 3.0f
	    || tape.encounter_length != sizeof(player_row) - 1U
	    || memcmp(tape.encounter, player_row, sizeof(player_row) - 1U) != 0)
		return false;

	plasma_fighter_fixture(&tape, &state, &energy, &bold);
	energy = 10000.0;
	tape.draws[0] = 0.0f;
	tape.draws[1] = 0.5f;
	if (!yt_projectile_plasma_fighter_run(&state, &ops, &tape, NULL)
	    || tape.draw_position != 2U || state.destroyed != 6.0
	    || state.remaining_fighters != 94.0 || energy != 0.0
	    || tape.event_count != 7U
	    || tape.events[2] != PLASMA_FIGHTER_RANDOM
	    || tape.events[3] != PLASMA_FIGHTER_RANDOM
	    || tape.events[4] != PLASMA_FIGHTER_DAMAGE)
		return false;

	plasma_fighter_fixture(&tape, &state, &energy, &bold);
	state.fighters = 10.0;
	state.owner = 2.0f;
	state.headquarters = 7.0f;
	energy = 100000.0;
	if (!yt_projectile_plasma_fighter_run(&state, &ops, &tape, NULL)
	    || tape.event_count != YT_ARRAY_LEN(victory_events)
	    || memcmp(tape.events, victory_events, sizeof(victory_events)) != 0
	    || tape.encounter_length != sizeof(self_row) - 1U
	    || memcmp(tape.encounter, self_row, sizeof(self_row) - 1U) != 0
	    || state.destroyed != 10.0 || state.remaining_fighters != 0.0
	    || !state.victory_called
	    || state.route != YT_PROJECTILE_PLASMA_FIGHTER_CONTINUE_SECTOR
	    || tape.news_length != sizeof(news_row) - 1U
	    || memcmp(tape.news, news_row, sizeof(news_row) - 1U) != 0
	    || memcmp(tape.written.record.bytes + YT_F81,
	    (const uint8_t[]){0x00, 0x00, 0x10, 0x00}, 4U) != 0
	    || memcmp(tape.written.record.bytes + YT_F85,
	    (const uint8_t[]){0x00, 0x00, 0x10, 0x00}, 4U) != 0)
		return false;

	for (failure = 1U; failure <= YT_ARRAY_LEN(victory_events); ++failure) {
		plasma_fighter_fixture(&tape, &state, &energy, &bold);
		state.fighters = 10.0;
		state.owner = 2.0f;
		state.headquarters = 7.0f;
		energy = 100000.0;
		tape.fail_at = failure;
		if (yt_projectile_plasma_fighter_run(&state, &ops, &tape, NULL)
		    || tape.event_count != failure
		    || memcmp(tape.events, victory_events,
		    failure * sizeof(victory_events[0])) != 0
		    || state.victory_called != (failure == 9U))
			return false;
	}
	return !yt_projectile_plasma_fighter_run(NULL, &ops, &tape, NULL)
	    && !yt_projectile_plasma_fighter_run(&state, NULL, &tape, NULL);
}

enum plasma_mine_event {
	PLASMA_MINE_SOUND = 1,
	PLASMA_MINE_NEWS,
	PLASMA_MINE_RANDOM,
	PLASMA_MINE_PRESENT,
	PLASMA_MINE_READ,
	PLASMA_MINE_WRITE,
};

struct plasma_mine_tape {
	int events[12];
	size_t event_count;
	size_t fail_at;
	float draws[4];
	size_t draw_position;
	uint8_t news[2][256];
	size_t news_length[2];
	size_t news_count;
	uint8_t direct[256];
	size_t direct_length;
	float selector;
	struct yt_sector source;
	struct yt_sector written;
	float sector_at_read;
	float sector_at_write;
};

static bool
plasma_mine_step(struct plasma_mine_tape *tape, enum plasma_mine_event event)
{
	if (tape->event_count >= YT_ARRAY_LEN(tape->events))
		return false;
	tape->events[tape->event_count++] = (int)event;
	return tape->event_count != tape->fail_at;
}

static bool
plasma_mine_test_sound(void *context, float selector,
    struct yt_error *error)
{
	struct plasma_mine_tape *tape = context;

	(void)error;
	tape->selector = selector;
	return plasma_mine_step(tape, PLASMA_MINE_SOUND);
}

static bool
plasma_mine_test_news(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	struct plasma_mine_tape *tape = context;

	(void)error;
	if (!plasma_mine_step(tape, PLASMA_MINE_NEWS)
	    || tape->news_count >= YT_ARRAY_LEN(tape->news)
	    || length > sizeof(tape->news[0]))
		return false;
	memcpy(tape->news[tape->news_count], text, length);
	tape->news_length[tape->news_count++] = length;
	return true;
}

static bool
plasma_mine_test_random(void *context, float *value,
    struct yt_error *error)
{
	struct plasma_mine_tape *tape = context;

	(void)error;
	if (!plasma_mine_step(tape, PLASMA_MINE_RANDOM)
	    || tape->draw_position >= YT_ARRAY_LEN(tape->draws))
		return false;
	*value = tape->draws[tape->draw_position++];
	return true;
}

static bool
plasma_mine_test_present(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	struct plasma_mine_tape *tape = context;

	(void)error;
	if (!plasma_mine_step(tape, PLASMA_MINE_PRESENT)
	    || length > sizeof(tape->direct))
		return false;
	memcpy(tape->direct, text, length);
	tape->direct_length = length;
	return true;
}

static bool
plasma_mine_test_read(void *context, float sector,
    struct yt_sector *value, struct yt_error *error)
{
	struct plasma_mine_tape *tape = context;

	(void)error;
	tape->sector_at_read = sector;
	if (!plasma_mine_step(tape, PLASMA_MINE_READ))
		return false;
	*value = tape->source;
	return true;
}

static bool
plasma_mine_test_write(void *context, float sector,
    const struct yt_sector *value, struct yt_error *error)
{
	struct plasma_mine_tape *tape = context;

	(void)error;
	tape->sector_at_write = sector;
	tape->written = *value;
	return plasma_mine_step(tape, PLASMA_MINE_WRITE);
}

static void
plasma_mine_fixture(struct plasma_mine_tape *tape,
    struct yt_projectile_plasma_mine_state *state, double *energy)
{
	static const uint8_t attacker[] = {'A', 0, 'B'};

	memset(tape, 0, sizeof(*tape));
	memset(state, 0, sizeof(*state));
	memset(tape->source.record.bytes, 0xa5,
	    sizeof(tape->source.record.bytes));
	tape->source.mines = 9.0f;
	(void)yt_record_set_number(&tape->source.record, YT_F129,
	    tape->source.mines);
	tape->fail_at = SIZE_MAX;
	tape->draws[0] = 0.5f;
	*energy = 1.0;
	state->sector = 7.0f;
	state->mines = 3.0;
	state->attacker = attacker;
	state->attacker_length = sizeof(attacker);
	state->energy = energy;
}

static bool
check_projectile_plasma_mine_transaction(void)
{
	static const struct yt_projectile_plasma_mine_ops ops = {
		plasma_mine_test_sound,
		plasma_mine_test_news,
		plasma_mine_test_random,
		plasma_mine_test_present,
		plasma_mine_test_read,
		plasma_mine_test_write,
	};
	static const int ordinary_events[] = {
		PLASMA_MINE_SOUND,
		PLASMA_MINE_NEWS,
		PLASMA_MINE_RANDOM,
		PLASMA_MINE_NEWS,
		PLASMA_MINE_PRESENT,
		PLASMA_MINE_READ,
		PLASMA_MINE_WRITE,
	};
	static const uint8_t entry_news[] =
	    "A\0B's Plasma Bolts hit sector mines in sector 7!";
	static const uint8_t result_news[] =
	    "A\0B's plasma bolts destroyed 1 mines in sector 7!";
	static const uint8_t direct[] =
	    "The plasma bolts destroyed 1 mines in sector 7!";
	struct plasma_mine_tape tape;
	struct yt_projectile_plasma_mine_state state;
	struct yt_record expected;
	double energy;
	size_t failure;

	plasma_mine_fixture(&tape, &state, &energy);
	expected = tape.source.record;
	(void)yt_record_set_number(&expected, YT_F129, 2.0f);
	if (!yt_projectile_plasma_mine_run(&state, &ops, &tape, NULL)
	    || tape.event_count != YT_ARRAY_LEN(ordinary_events)
	    || memcmp(tape.events, ordinary_events, sizeof(ordinary_events)) != 0
	    || tape.selector != 5.0f || tape.draw_position != 1U
	    || state.destroyed != 1.0f || state.remaining_mines != 2.0f
	    || energy != 0.0
	    || state.route != YT_PROJECTILE_PLASMA_MINE_FOOTER
	    || tape.news_count != 2U
	    || tape.news_length[0] != sizeof(entry_news) - 1U
	    || memcmp(tape.news[0], entry_news, sizeof(entry_news) - 1U) != 0
	    || tape.news_length[1] != sizeof(result_news) - 1U
	    || memcmp(tape.news[1], result_news, sizeof(result_news) - 1U) != 0
	    || tape.direct_length != sizeof(direct) - 1U
	    || memcmp(tape.direct, direct, sizeof(direct) - 1U) != 0
	    || tape.sector_at_read != 7.0f || tape.sector_at_write != 7.0f
	    || tape.written.mines != 2.0f
	    || memcmp(&tape.written.record, &expected, sizeof(expected)) != 0)
		return false;

	plasma_mine_fixture(&tape, &state, &energy);
	state.mines = 0.0;
	if (!yt_projectile_plasma_mine_run(&state, &ops, &tape, NULL)
	    || tape.event_count != 0U
	    || state.route != YT_PROJECTILE_PLASMA_MINE_CONTINUE_PLAYERS)
		return false;

	plasma_mine_fixture(&tape, &state, &energy);
	energy = 0.0;
	if (!yt_projectile_plasma_mine_run(&state, &ops, &tape, NULL)
	    || tape.event_count != 6U || tape.draw_position != 0U
	    || state.destroyed != 0.0f || state.remaining_mines != 3.0f
	    || tape.news_count != 2U
	    || state.route != YT_PROJECTILE_PLASMA_MINE_FOOTER)
		return false;

	plasma_mine_fixture(&tape, &state, &energy);
	energy = 100000.0;
	state.mines = 2.0;
	tape.draws[0] = 0.0f;
	tape.draws[1] = 0.5f;
	if (!yt_projectile_plasma_mine_run(&state, &ops, &tape, NULL)
	    || tape.draw_position != 2U || state.destroyed != 2.0f
	    || state.remaining_mines != 0.0f || energy != 87500.0
	    || state.route != YT_PROJECTILE_PLASMA_MINE_CONTINUE_PLAYERS)
		return false;

	for (failure = 1U; failure <= YT_ARRAY_LEN(ordinary_events); ++failure) {
		plasma_mine_fixture(&tape, &state, &energy);
		tape.fail_at = failure;
		if (yt_projectile_plasma_mine_run(&state, &ops, &tape, NULL)
		    || tape.event_count != failure
		    || memcmp(tape.events, ordinary_events,
		    failure * sizeof(ordinary_events[0])) != 0)
			return false;
	}
	return !yt_projectile_plasma_mine_run(NULL, &ops, &tape, NULL)
	    && !yt_projectile_plasma_mine_run(&state, NULL, &tape, NULL);
}

static bool
check_projectile_plasma_dispatch_transaction(void)
{
	float cache[8] = {0.0f};
	struct yt_projectile_plasma_dispatch_state state;
	struct yt_error error;

	memset(&state, 0, sizeof(state));
	state.energy = 100.0;
	state.sector = 7.0f;
	state.player_terminal = 5.0f;
	state.sector_cache = cache;
	state.cache_count = YT_ARRAY_LEN(cache);
	cache[3] = 7.0f;
	cache[4] = 7.0f;
	if (!yt_projectile_plasma_dispatch_run(&state, NULL)
	    || state.route != YT_PROJECTILE_PLASMA_DISPATCH_PLAYER
	    || state.selected_player != 3 || state.counter != 3.0f)
		return false;
	state.resume_after_player = true;
	if (!yt_projectile_plasma_dispatch_run(&state, NULL)
	    || state.route != YT_PROJECTILE_PLASMA_DISPATCH_PLAYER
	    || state.selected_player != 4 || state.counter != 4.0f)
		return false;
	state.energy = 0.5;
	if (!yt_projectile_plasma_dispatch_run(&state, NULL)
	    || state.route != YT_PROJECTILE_PLASMA_DISPATCH_FOOTER
	    || state.counter != 4.0f)
		return false;

	memset(cache, 0, sizeof(cache));
	memset(&state, 0, sizeof(state));
	state.energy = 1.0;
	state.sector = 7.0f;
	state.player_terminal = 3.5f;
	state.sector_cache = cache;
	state.cache_count = YT_ARRAY_LEN(cache);
	if (!yt_projectile_plasma_dispatch_run(&state, NULL)
	    || state.route != YT_PROJECTILE_PLASMA_DISPATCH_NEXT_HOP
	    || state.counter != 4.0f || state.selected_player != 0)
		return false;

	state.planet_link = 0.6f;
	if (!yt_projectile_plasma_dispatch_run(&state, NULL)
	    || state.route != YT_PROJECTILE_PLASMA_DISPATCH_PLANET)
		return false;
	state.planet_link = 0.4f;
	if (!yt_projectile_plasma_dispatch_run(&state, NULL)
	    || state.route != YT_PROJECTILE_PLASMA_DISPATCH_NEXT_HOP)
		return false;
	state.energy = 0.0;
	state.planet_link = 12.0f;
	if (!yt_projectile_plasma_dispatch_run(&state, NULL)
	    || state.route != YT_PROJECTILE_PLASMA_DISPATCH_FOOTER)
		return false;

	state.energy = 1.0;
	state.player_terminal = 1.5f;
	if (!yt_projectile_plasma_dispatch_run(&state, NULL)
	    || state.counter != 2.0f
	    || state.route != YT_PROJECTILE_PLASMA_DISPATCH_PLANET)
		return false;

	state.player_terminal = 2.0f;
	state.cache_count = 2U;
	yt_error_clear(&error);
	if (yt_projectile_plasma_dispatch_run(&state, &error)
	    || error.status != YT_RANGE)
		return false;
	return !yt_projectile_plasma_dispatch_run(NULL, NULL);
}

enum plasma_player_event {
	PLASMA_PLAYER_READ = 1,
	PLASMA_PLAYER_COLOR,
	PLASMA_PLAYER_SOUND,
	PLASMA_PLAYER_RANDOM,
	PLASMA_PLAYER_NEWS,
	PLASMA_PLAYER_PRESENT,
	PLASMA_PLAYER_WRITE,
};

struct plasma_player_tape {
	int events[24];
	size_t event_count;
	size_t fail_at;
	struct yt_player sources[3];
	size_t read_count;
	int read_records[3];
	struct yt_player written;
	int written_record;
	float colors[2];
	size_t color_count;
	float selector;
	float draws[8];
	size_t draw_position;
	uint8_t news[2][256];
	size_t news_lengths[2];
	size_t news_count;
	uint8_t direct[2][256];
	size_t direct_lengths[2];
	enum yt_projectile_plasma_player_output_kind direct_kinds[2];
	size_t direct_count;
};

static bool
plasma_player_step(struct plasma_player_tape *tape,
    enum plasma_player_event event)
{
	if (tape->event_count >= YT_ARRAY_LEN(tape->events))
		return false;
	tape->events[tape->event_count++] = (int)event;
	return tape->event_count != tape->fail_at;
}

static bool
plasma_player_test_read(void *context, int player_record,
    struct yt_player *value, struct yt_error *error)
{
	struct plasma_player_tape *tape = context;

	(void)error;
	if (!plasma_player_step(tape, PLASMA_PLAYER_READ)
	    || tape->read_count >= YT_ARRAY_LEN(tape->sources))
		return false;
	tape->read_records[tape->read_count] = player_record;
	*value = tape->sources[tape->read_count++];
	return true;
}

static bool
plasma_player_test_write(void *context, int player_record,
    const struct yt_player *value, struct yt_error *error)
{
	struct plasma_player_tape *tape = context;

	(void)error;
	if (!plasma_player_step(tape, PLASMA_PLAYER_WRITE))
		return false;
	tape->written_record = player_record;
	tape->written = *value;
	return true;
}

static void
plasma_player_test_color(void *context, float foreground)
{
	struct plasma_player_tape *tape = context;

	(void)plasma_player_step(tape, PLASMA_PLAYER_COLOR);
	if (tape->color_count < YT_ARRAY_LEN(tape->colors))
		tape->colors[tape->color_count++] = foreground;
}

static bool
plasma_player_test_sound(void *context, float selector,
    struct yt_error *error)
{
	struct plasma_player_tape *tape = context;

	(void)error;
	tape->selector = selector;
	return plasma_player_step(tape, PLASMA_PLAYER_SOUND);
}

static bool
plasma_player_test_random(void *context, float *value,
    struct yt_error *error)
{
	struct plasma_player_tape *tape = context;

	(void)error;
	if (!plasma_player_step(tape, PLASMA_PLAYER_RANDOM)
	    || tape->draw_position >= YT_ARRAY_LEN(tape->draws))
		return false;
	*value = tape->draws[tape->draw_position++];
	return true;
}

static bool
plasma_player_test_news(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	struct plasma_player_tape *tape = context;

	(void)error;
	if (!plasma_player_step(tape, PLASMA_PLAYER_NEWS)
	    || tape->news_count >= YT_ARRAY_LEN(tape->news)
	    || length > sizeof(tape->news[0]))
		return false;
	memcpy(tape->news[tape->news_count], text, length);
	tape->news_lengths[tape->news_count++] = length;
	return true;
}

static bool
plasma_player_test_present(void *context, const uint8_t *text, size_t length,
    enum yt_projectile_plasma_player_output_kind kind,
    struct yt_error *error)
{
	struct plasma_player_tape *tape = context;

	(void)error;
	if (!plasma_player_step(tape, PLASMA_PLAYER_PRESENT)
	    || tape->direct_count >= YT_ARRAY_LEN(tape->direct)
	    || length > sizeof(tape->direct[0]))
		return false;
	memcpy(tape->direct[tape->direct_count], text, length);
	tape->direct_lengths[tape->direct_count] = length;
	tape->direct_kinds[tape->direct_count++] = kind;
	return true;
}

static void
plasma_player_fixture(struct plasma_player_tape *tape,
    struct yt_projectile_plasma_player_state *state, double *energy)
{
	static const uint8_t attacker[] = {'A', 0, 'B'};
	struct yt_player *entry;
	struct yt_player *presentation;
	struct yt_player *survivor;

	memset(tape, 0, sizeof(*tape));
	memset(state, 0, sizeof(*state));
	tape->fail_at = SIZE_MAX;
	entry = &tape->sources[0];
	presentation = &tape->sources[1];
	survivor = &tape->sources[2];
	memset(entry->record.bytes, 0xa5, sizeof(entry->record.bytes));
	entry->fighters = 1.0f;
	entry->shields = 10.0f;
	(void)yt_record_set_number(&entry->record, YT_F61, entry->fighters);
	(void)yt_record_set_number(&entry->record, YT_F53, entry->shields);
	memset(presentation->record.bytes, 0xc3,
	    sizeof(presentation->record.bytes));
	presentation->record.bytes[0] = 'C';
	presentation->record.bytes[1] = 0;
	presentation->record.bytes[2] = 'D';
	presentation->name_length = 3.0f;
	(void)yt_record_set_number(&presentation->record, YT_F85,
	    presentation->name_length);
	memset(survivor->record.bytes, 0xb6, sizeof(survivor->record.bytes));
	survivor->fighters = 777.0f;
	survivor->shields = 888.0f;
	(void)yt_record_set_number(&survivor->record, YT_F61,
	    survivor->fighters);
	(void)yt_record_set_number(&survivor->record, YT_F53,
	    survivor->shields);
	tape->draws[0] = 0.0f;
	tape->draws[1] = 0.0f;
	tape->draws[2] = 0.5f;
	*energy = 2.0;
	state->target = 3;
	state->sector = 7.0f;
	state->attacker = attacker;
	state->attacker_length = sizeof(attacker);
	state->energy = energy;
	state->foreground = 3.0f;
}

static bool
check_projectile_plasma_player_transaction(void)
{
	static const struct yt_projectile_plasma_player_ops ops = {
		plasma_player_test_read,
		plasma_player_test_write,
		plasma_player_test_color,
		plasma_player_test_sound,
		plasma_player_test_random,
		plasma_player_test_news,
		plasma_player_test_present,
	};
	static const int ordinary_events[] = {
		PLASMA_PLAYER_READ,
		PLASMA_PLAYER_COLOR,
		PLASMA_PLAYER_SOUND,
		PLASMA_PLAYER_RANDOM,
		PLASMA_PLAYER_RANDOM,
		PLASMA_PLAYER_RANDOM,
		PLASMA_PLAYER_READ,
		PLASMA_PLAYER_NEWS,
		PLASMA_PLAYER_PRESENT,
		PLASMA_PLAYER_NEWS,
		PLASMA_PLAYER_PRESENT,
		PLASMA_PLAYER_COLOR,
		PLASMA_PLAYER_READ,
		PLASMA_PLAYER_WRITE,
	};
	static const size_t failure_positions[] = {
		1U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 13U, 14U,
	};
	static const uint8_t first_news[] =
	    "A\0B's plasma bolts hit C\0D in 7 reducing";
	static const uint8_t first_direct[] =
	    "The plasma bolts hit C\0D in 7 reducing";
	static const uint8_t second_row[] =
	    "shields to 8 units and destroying 1 fighters!";
	struct plasma_player_tape tape;
	struct yt_projectile_plasma_player_state state;
	struct yt_record expected;
	double energy;
	size_t index;

	plasma_player_fixture(&tape, &state, &energy);
	expected = tape.sources[2].record;
	(void)yt_record_set_number(&expected, YT_F53, 8.0f);
	(void)yt_record_set_number(&expected, YT_F61, 0.0f);
	if (!yt_projectile_plasma_player_run(&state, &ops, &tape, NULL)
	    || tape.event_count != YT_ARRAY_LEN(ordinary_events)
	    || memcmp(tape.events, ordinary_events, sizeof(ordinary_events)) != 0
	    || tape.read_count != 3U || tape.read_records[0] != 3
	    || tape.read_records[1] != 3 || tape.read_records[2] != 3
	    || tape.selector != 2.0f || tape.draw_position != 3U
	    || tape.color_count != 2U || tape.colors[0] != 5.0f
	    || tape.colors[1] != 3.0f || state.saved_foreground != 3.0f
	    || state.original_fighters != 1.0 || state.original_shields != 10.0f
	    || state.destroyed_fighters != 1.0
	    || state.destroyed_shields != 2.0f
	    || state.remaining_fighters != 0.0
	    || state.remaining_shields != 8.0f || energy != -12498.0
	    || state.route != YT_PROJECTILE_PLASMA_PLAYER_FOOTER
	    || tape.news_count != 2U || tape.direct_count != 2U
	    || tape.news_lengths[0] != sizeof(first_news) - 1U
	    || memcmp(tape.news[0], first_news, sizeof(first_news) - 1U) != 0
	    || tape.direct_lengths[0] != sizeof(first_direct) - 1U
	    || memcmp(tape.direct[0], first_direct,
	    sizeof(first_direct) - 1U) != 0
	    || tape.news_lengths[1] != sizeof(second_row) - 1U
	    || memcmp(tape.news[1], second_row, sizeof(second_row) - 1U) != 0
	    || tape.direct_lengths[1] != sizeof(second_row) - 1U
	    || memcmp(tape.direct[1], second_row, sizeof(second_row) - 1U) != 0
	    || tape.direct_kinds[0] != YT_PROJECTILE_PLASMA_PLAYER_FIRST_ROW
	    || tape.direct_kinds[1] != YT_PROJECTILE_PLASMA_PLAYER_SECOND_ROW
	    || tape.written_record != 3 || tape.written.shields != 8.0f
	    || tape.written.fighters != 0.0f
	    || memcmp(&tape.written.record, &expected, sizeof(expected)) != 0)
		return false;

	plasma_player_fixture(&tape, &state, &energy);
	tape.sources[0].fighters = 100.0f;
	tape.sources[0].shields = 100.0f;
	energy = 1.0;
	tape.draws[0] = 0.5f;
	if (!yt_projectile_plasma_player_run(&state, &ops, &tape, NULL)
	    || state.route != YT_PROJECTILE_PLASMA_PLAYER_FOOTER
	    || state.destroyed_fighters != 1.0
	    || state.destroyed_shields != 0.0f
	    || state.remaining_fighters != 99.0
	    || state.remaining_shields != 100.0f
	    || energy != -12499.0 || tape.draw_position != 1U
	    || tape.read_count != 3U)
		return false;

	plasma_player_fixture(&tape, &state, &energy);
	tape.sources[0].fighters = 0.0f;
	tape.sources[0].shields = 1.0f;
	energy = 1.0;
	tape.draws[0] = 0.5f;
	if (!yt_projectile_plasma_player_run(&state, &ops, &tape, NULL)
	    || state.route != YT_PROJECTILE_PLASMA_PLAYER_KILLED
	    || state.destroyed_fighters != 0.0
	    || state.destroyed_shields != 1.0f || state.remaining_shields != 0.0f
	    || energy != -12499.0 || tape.read_count != 2U
	    || tape.event_count != 10U || tape.color_count != 2U)
		return false;

	plasma_player_fixture(&tape, &state, &energy);
	tape.sources[0].fighters = 10.0f;
	tape.sources[0].shields = 0.0f;
	energy = 10000.0;
	tape.draws[0] = 0.0f;
	tape.draws[1] = 0.5f;
	if (!yt_projectile_plasma_player_run(&state, &ops, &tape, NULL)
	    || state.route != YT_PROJECTILE_PLASMA_PLAYER_KILLED
	    || state.destroyed_fighters != 6.0 || tape.draw_position != 2U
	    || energy != -2500.0)
		return false;

	plasma_player_fixture(&tape, &state, &energy);
	tape.sources[0].fighters = 10.0f;
	tape.sources[0].shields = 10.0f;
	energy = 1000000.0;
	tape.draws[0] = 0.5f;
	tape.draws[1] = 0.5f;
	if (!yt_projectile_plasma_player_run(&state, &ops, &tape, NULL)
	    || state.route != YT_PROJECTILE_PLASMA_PLAYER_KILLED
	    || state.destroyed_fighters != 10.0
	    || state.destroyed_shields != 10.0f
	    || state.remaining_fighters != 0.0
	    || state.remaining_shields != 0.0f || tape.draw_position != 2U
	    || energy != 975000.0)
		return false;

	for (index = 0U; index < YT_ARRAY_LEN(failure_positions); ++index) {
		plasma_player_fixture(&tape, &state, &energy);
		tape.fail_at = failure_positions[index];
		if (yt_projectile_plasma_player_run(&state, &ops, &tape, NULL)
		    || tape.event_count != failure_positions[index]
		    || memcmp(tape.events, ordinary_events,
		    failure_positions[index] * sizeof(ordinary_events[0])) != 0
		    || (failure_positions[index] == 1U
		    && tape.color_count != 0U)
		    || (failure_positions[index] >= 3U
		    && failure_positions[index] <= 11U
		    && (tape.color_count != 1U || tape.colors[0] != 5.0f))
		    || (failure_positions[index] >= 13U
		    && (tape.color_count != 2U || tape.colors[1] != 3.0f)))
			return false;
	}
	return !yt_projectile_plasma_player_run(NULL, &ops, &tape, NULL)
	    && !yt_projectile_plasma_player_run(&state, NULL, &tape, NULL);
}

enum plasma_killed_event {
	PLASMA_KILLED_READ_PLAYER = 1,
	PLASMA_KILLED_WRITE_PLAYER,
	PLASMA_KILLED_PRESENT,
	PLASMA_KILLED_READ_SECTOR,
	PLASMA_KILLED_WRITE_SECTOR,
	PLASMA_KILLED_DEATH,
	PLASMA_KILLED_SOUND,
	PLASMA_KILLED_SALVAGE,
};

struct plasma_killed_tape {
	int events[16];
	size_t event_count;
	size_t fail_at;
	struct yt_player player_source;
	struct yt_player player_written;
	int player_read_record;
	int player_write_record;
	struct yt_sector sector_source;
	struct yt_sector sector_written;
	int sector_read_record;
	int sector_write_record;
	uint8_t output[2][192];
	size_t output_lengths[2];
	enum yt_projectile_plasma_killed_output_kind output_kinds[2];
	size_t output_count;
	int death_victim;
	int death_shooter;
	int salvage_victim;
	int salvage_shooter;
	float selector;
};

static bool
plasma_killed_step(struct plasma_killed_tape *tape,
    enum plasma_killed_event event)
{
	if (tape->event_count >= YT_ARRAY_LEN(tape->events))
		return false;
	tape->events[tape->event_count++] = (int)event;
	return tape->event_count != tape->fail_at;
}

static bool
plasma_killed_test_read_player(void *context, int player_record,
    struct yt_player *value, struct yt_error *error)
{
	struct plasma_killed_tape *tape = context;

	(void)error;
	tape->player_read_record = player_record;
	if (!plasma_killed_step(tape, PLASMA_KILLED_READ_PLAYER))
		return false;
	*value = tape->player_source;
	return true;
}

static bool
plasma_killed_test_write_player(void *context, int player_record,
    const struct yt_player *value, struct yt_error *error)
{
	struct plasma_killed_tape *tape = context;

	(void)error;
	tape->player_write_record = player_record;
	if (!plasma_killed_step(tape, PLASMA_KILLED_WRITE_PLAYER))
		return false;
	tape->player_written = *value;
	return true;
}

static bool
plasma_killed_test_present(void *context, const uint8_t *text, size_t length,
    enum yt_projectile_plasma_killed_output_kind kind,
    struct yt_error *error)
{
	struct plasma_killed_tape *tape = context;

	(void)error;
	if (!plasma_killed_step(tape, PLASMA_KILLED_PRESENT)
	    || tape->output_count >= YT_ARRAY_LEN(tape->output)
	    || length > sizeof(tape->output[0]))
		return false;
	memcpy(tape->output[tape->output_count], text, length);
	tape->output_lengths[tape->output_count] = length;
	tape->output_kinds[tape->output_count++] = kind;
	return true;
}

static bool
plasma_killed_test_read_sector(void *context, int sector,
    struct yt_sector *value, struct yt_error *error)
{
	struct plasma_killed_tape *tape = context;

	(void)error;
	tape->sector_read_record = sector;
	if (!plasma_killed_step(tape, PLASMA_KILLED_READ_SECTOR))
		return false;
	*value = tape->sector_source;
	return true;
}

static bool
plasma_killed_test_write_sector(void *context, int sector,
    const struct yt_sector *value, struct yt_error *error)
{
	struct plasma_killed_tape *tape = context;

	(void)error;
	tape->sector_write_record = sector;
	if (!plasma_killed_step(tape, PLASMA_KILLED_WRITE_SECTOR))
		return false;
	tape->sector_written = *value;
	return true;
}

static bool
plasma_killed_test_death(void *context, int victim, int shooter,
    struct yt_error *error)
{
	struct plasma_killed_tape *tape = context;

	(void)error;
	tape->death_victim = victim;
	tape->death_shooter = shooter;
	return plasma_killed_step(tape, PLASMA_KILLED_DEATH);
}

static bool
plasma_killed_test_sound(void *context, float selector,
    struct yt_error *error)
{
	struct plasma_killed_tape *tape = context;

	(void)error;
	tape->selector = selector;
	return plasma_killed_step(tape, PLASMA_KILLED_SOUND);
}

static bool
plasma_killed_test_salvage(void *context, int victim, int shooter,
    struct yt_error *error)
{
	struct plasma_killed_tape *tape = context;

	(void)error;
	tape->salvage_victim = victim;
	tape->salvage_shooter = shooter;
	return plasma_killed_step(tape, PLASMA_KILLED_SALVAGE);
}

static void
plasma_killed_fixture(struct plasma_killed_tape *tape,
    struct yt_projectile_plasma_killed_state *state, double *energy,
    float *blink, bool *destroyed, float cache[8])
{
	memset(tape, 0, sizeof(*tape));
	memset(state, 0, sizeof(*state));
	memset(cache, 0, 8U * sizeof(cache[0]));
	tape->fail_at = SIZE_MAX;
	memset(tape->player_source.record.bytes, 0xa5,
	    sizeof(tape->player_source.record.bytes));
	tape->player_source.record.bytes[0] = 'C';
	tape->player_source.record.bytes[1] = 0;
	tape->player_source.record.bytes[2] = 'D';
	tape->player_source.name_length = 3.0f;
	tape->player_source.mines = 3.0f;
	tape->player_source.danger_scanner = 9.0f;
	(void)yt_record_set_number(&tape->player_source.record, YT_F85, 3.0f);
	(void)yt_record_set_number(&tape->player_source.record, YT_F129, 3.0f);
	(void)yt_record_set_number(&tape->player_source.record, YT_F93, 9.0f);
	memset(tape->sector_source.record.bytes, 0xb6,
	    sizeof(tape->sector_source.record.bytes));
	tape->sector_source.mines = 4.0f;
	(void)yt_record_set_number(&tape->sector_source.record, YT_F129, 4.0f);
	*energy = 0.5;
	*blink = 0.0f;
	*destroyed = false;
	cache[2] = 7.0f;
	state->victim = 3;
	state->shooter = 2;
	state->sector = 7;
	state->energy = energy;
	state->blink = blink;
	state->destroyed = destroyed;
	state->sector_cache = cache;
	state->cache_count = 8U;
}

static bool
check_projectile_plasma_killed_transaction(void)
{
	static const struct yt_projectile_plasma_killed_ops ops = {
		plasma_killed_test_read_player,
		plasma_killed_test_write_player,
		plasma_killed_test_present,
		plasma_killed_test_read_sector,
		plasma_killed_test_write_sector,
		plasma_killed_test_death,
		plasma_killed_test_sound,
		plasma_killed_test_salvage,
	};
	static const int ordinary_events[] = {
		PLASMA_KILLED_READ_PLAYER,
		PLASMA_KILLED_WRITE_PLAYER,
		PLASMA_KILLED_PRESENT,
		PLASMA_KILLED_PRESENT,
		PLASMA_KILLED_READ_SECTOR,
		PLASMA_KILLED_WRITE_SECTOR,
		PLASMA_KILLED_DEATH,
		PLASMA_KILLED_SOUND,
		PLASMA_KILLED_SALVAGE,
	};
	static const uint8_t destroyed_row[] = {'C', 0, 'D', ' ', 'w', 'a', 's',
	    ' ', 'd', 'e', 's', 't', 'r', 'o', 'y', 'e', 'd', '!'};
	static const uint8_t warning_row[] = {'*', '*', '*', ' ', 'W', 'A', 'R',
	    'N', 'I', 'N', 'G', ',', ' ', 'C', 0, 'D', ' ', 'h', 'a', 'd', ' ',
	    's', 'e', 'c', 't', 'o', 'r', ' ', 'm', 'i', 'n', 'e', 's', '!'};
	static const uint8_t self_row[] = "YOU were destroyed!";
	struct plasma_killed_tape tape;
	struct yt_projectile_plasma_killed_state state;
	struct yt_record expected_player;
	struct yt_record expected_sector;
	double energy;
	float blink;
	float cache[8];
	bool destroyed;
	size_t failure;

	plasma_killed_fixture(&tape, &state, &energy, &blink, &destroyed, cache);
	expected_player = tape.player_source.record;
	(void)yt_record_set_number(&expected_player, YT_F129, 0.0f);
	(void)yt_record_set_number(&expected_player, YT_F93, 0.0f);
	expected_sector = tape.sector_source.record;
	(void)yt_record_set_number(&expected_sector, YT_F129, 7.0f);
	if (!yt_projectile_plasma_killed_run(&state, &ops, &tape, NULL)
	    || tape.event_count != YT_ARRAY_LEN(ordinary_events)
	    || memcmp(tape.events, ordinary_events, sizeof(ordinary_events)) != 0
	    || state.self_hit || state.saved_mines != 3.0f
	    || state.route != YT_PROJECTILE_PLASMA_KILLED_RELOAD_SECTOR
	    || blink != 1.0f || destroyed || cache[2] != 7.0f
	    || tape.player_read_record != 3 || tape.player_write_record != 3
	    || tape.player_written.mines != 0.0f
	    || tape.player_written.danger_scanner != 0.0f
	    || memcmp(&tape.player_written.record, &expected_player,
	    sizeof(expected_player)) != 0
	    || tape.output_count != 2U
	    || tape.output_lengths[0] != sizeof(destroyed_row)
	    || memcmp(tape.output[0], destroyed_row, sizeof(destroyed_row)) != 0
	    || tape.output_lengths[1] != sizeof(warning_row)
	    || memcmp(tape.output[1], warning_row, sizeof(warning_row)) != 0
	    || tape.output_kinds[0] != YT_PROJECTILE_PLASMA_KILLED_DESTROYED_ROW
	    || tape.output_kinds[1] != YT_PROJECTILE_PLASMA_KILLED_WARNING_ROW
	    || tape.sector_read_record != 7 || tape.sector_write_record != 7
	    || tape.sector_written.mines != 7.0f
	    || memcmp(&tape.sector_written.record, &expected_sector,
	    sizeof(expected_sector)) != 0
	    || tape.death_victim != 3 || tape.death_shooter != 2
	    || tape.selector != 3.0f
	    || tape.salvage_victim != 3 || tape.salvage_shooter != 2)
		return false;

	plasma_killed_fixture(&tape, &state, &energy, &blink, &destroyed, cache);
	tape.player_source.mines = 0.0f;
	(void)yt_record_set_number(&tape.player_source.record, YT_F129, 0.0f);
	if (!yt_projectile_plasma_killed_run(&state, &ops, &tape, NULL)
	    || state.route != YT_PROJECTILE_PLASMA_KILLED_FOOTER
	    || tape.event_count != 6U || tape.output_count != 1U)
		return false;

	plasma_killed_fixture(&tape, &state, &energy, &blink, &destroyed, cache);
	tape.player_source.mines = -2.0f;
	(void)yt_record_set_number(&tape.player_source.record, YT_F129, -2.0f);
	energy = 100.0;
	if (!yt_projectile_plasma_killed_run(&state, &ops, &tape, NULL)
	    || state.route != YT_PROJECTILE_PLASMA_KILLED_CONTINUE_DISPATCH
	    || tape.sector_written.mines != 2.0f || tape.output_count != 2U)
		return false;

	plasma_killed_fixture(&tape, &state, &energy, &blink, &destroyed, cache);
	state.victim = 2;
	tape.player_source.mines = 0.0f;
	tape.player_source.name_length = 40000.0f;
	(void)yt_record_set_number(&tape.player_source.record, YT_F129, 0.0f);
	energy = 1.0;
	if (!yt_projectile_plasma_killed_run(&state, &ops, &tape, NULL)
	    || !state.self_hit || !destroyed || cache[2] != 0.0f
	    || state.route != YT_PROJECTILE_PLASMA_KILLED_CONTINUE_DISPATCH
	    || tape.event_count != 3U || tape.output_count != 1U
	    || tape.output_lengths[0] != sizeof(self_row) - 1U
	    || tape.output_kinds[0] !=
	    YT_PROJECTILE_PLASMA_KILLED_SELF_DESTROYED_ROW
	    || memcmp(tape.output[0], self_row, sizeof(self_row) - 1U) != 0)
		return false;

	plasma_killed_fixture(&tape, &state, &energy, &blink, &destroyed, cache);
	state.victim = 2;
	if (!yt_projectile_plasma_killed_run(&state, &ops, &tape, NULL)
	    || !state.self_hit || !destroyed || cache[2] != 0.0f
	    || state.route != YT_PROJECTILE_PLASMA_KILLED_RELOAD_SECTOR
	    || tape.event_count != 6U || tape.output_count != 2U
	    || tape.sector_written.mines != 7.0f)
		return false;

	plasma_killed_fixture(&tape, &state, &energy, &blink, &destroyed, cache);
	state.victim = 2;
	state.cache_count = 2U;
	tape.player_source.mines = 0.0f;
	(void)yt_record_set_number(&tape.player_source.record, YT_F129, 0.0f);
	if (yt_projectile_plasma_killed_run(&state, &ops, &tape, NULL))
		return false;

	for (failure = 1U; failure <= YT_ARRAY_LEN(ordinary_events); ++failure) {
		plasma_killed_fixture(&tape, &state, &energy, &blink, &destroyed,
		    cache);
		tape.fail_at = failure;
		if (yt_projectile_plasma_killed_run(&state, &ops, &tape, NULL)
		    || tape.event_count != failure
		    || memcmp(tape.events, ordinary_events,
		    failure * sizeof(ordinary_events[0])) != 0)
			return false;
	}
	return !yt_projectile_plasma_killed_run(NULL, &ops, &tape, NULL)
	    && !yt_projectile_plasma_killed_run(&state, NULL, &tape, NULL);
}

enum plasma_planet_event {
	PLASMA_PLANET_UPDATE = 1,
	PLASMA_PLANET_READ,
	PLASMA_PLANET_PRESENT,
	PLASMA_PLANET_NEWS,
	PLASMA_PLANET_SOUND,
	PLASMA_PLANET_RANDOM,
	PLASMA_PLANET_WRITE,
	PLASMA_PLANET_SECTOR_READ,
	PLASMA_PLANET_SECTOR_WRITE,
};

struct plasma_planet_tape {
	int events[32];
	size_t event_count;
	size_t fail_at;
	int logical_planet;
	int sector;
	float stale_ore;
	float draws[4];
	size_t draw_count;
	size_t draw_position;
	struct yt_planet planet_source[3];
	struct yt_planet planet_write[3];
	size_t planet_reads;
	size_t planet_writes;
	struct yt_sector sector_source;
	struct yt_sector sector_write;
	uint8_t output[4][256];
	size_t output_length[4];
	enum yt_projectile_plasma_planet_output_kind output_kind[4];
	size_t output_count;
	uint8_t news[4][256];
	size_t news_length[4];
	size_t news_count;
	float selectors[2];
	size_t sound_count;
};

static bool
plasma_planet_step(struct plasma_planet_tape *tape, int event)
{
	if (tape->event_count >= YT_ARRAY_LEN(tape->events))
		return false;
	tape->events[tape->event_count++] = event;
	return tape->event_count != tape->fail_at;
}

static bool
plasma_planet_update(void *context, int planet, float *stale_ore,
    struct yt_error *error)
{
	struct plasma_planet_tape *tape = context;

	(void)error;
	if (!plasma_planet_step(tape, PLASMA_PLANET_UPDATE)
	    || planet != tape->logical_planet || stale_ore == NULL)
		return false;
	*stale_ore = tape->stale_ore;
	return true;
}

static bool
plasma_planet_read(void *context, int planet, struct yt_planet *value,
    struct yt_error *error)
{
	struct plasma_planet_tape *tape = context;
	size_t position = tape->planet_reads++;

	(void)error;
	if (!plasma_planet_step(tape, PLASMA_PLANET_READ)
	    || planet != tape->logical_planet
	    || position >= YT_ARRAY_LEN(tape->planet_source))
		return false;
	*value = tape->planet_source[position];
	return true;
}

static bool
plasma_planet_write(void *context, int planet,
    const struct yt_planet *value, struct yt_error *error)
{
	struct plasma_planet_tape *tape = context;
	size_t position = tape->planet_writes++;

	(void)error;
	if (!plasma_planet_step(tape, PLASMA_PLANET_WRITE)
	    || planet != tape->logical_planet
	    || position >= YT_ARRAY_LEN(tape->planet_write))
		return false;
	tape->planet_write[position] = *value;
	return true;
}

static bool
plasma_planet_sector_read(void *context, int sector,
    struct yt_sector *value, struct yt_error *error)
{
	struct plasma_planet_tape *tape = context;

	(void)error;
	if (!plasma_planet_step(tape, PLASMA_PLANET_SECTOR_READ)
	    || sector != tape->sector)
		return false;
	*value = tape->sector_source;
	return true;
}

static bool
plasma_planet_sector_write(void *context, int sector,
    const struct yt_sector *value, struct yt_error *error)
{
	struct plasma_planet_tape *tape = context;

	(void)error;
	if (!plasma_planet_step(tape, PLASMA_PLANET_SECTOR_WRITE)
	    || sector != tape->sector)
		return false;
	tape->sector_write = *value;
	return true;
}

static bool
plasma_planet_present(void *context, const uint8_t *text, size_t length,
    enum yt_projectile_plasma_planet_output_kind kind,
    struct yt_error *error)
{
	struct plasma_planet_tape *tape = context;
	size_t position = tape->output_count++;

	(void)error;
	if (!plasma_planet_step(tape, PLASMA_PLANET_PRESENT)
	    || position >= YT_ARRAY_LEN(tape->output)
	    || length > sizeof(tape->output[position]))
		return false;
	memcpy(tape->output[position], text, length);
	tape->output_length[position] = length;
	tape->output_kind[position] = kind;
	return true;
}

static bool
plasma_planet_news(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	struct plasma_planet_tape *tape = context;
	size_t position = tape->news_count++;

	(void)error;
	if (!plasma_planet_step(tape, PLASMA_PLANET_NEWS)
	    || position >= YT_ARRAY_LEN(tape->news)
	    || length > sizeof(tape->news[position]))
		return false;
	memcpy(tape->news[position], text, length);
	tape->news_length[position] = length;
	return true;
}

static bool
plasma_planet_sound(void *context, float selector, struct yt_error *error)
{
	struct plasma_planet_tape *tape = context;
	size_t position = tape->sound_count++;

	(void)error;
	if (!plasma_planet_step(tape, PLASMA_PLANET_SOUND)
	    || position >= YT_ARRAY_LEN(tape->selectors))
		return false;
	tape->selectors[position] = selector;
	return true;
}

static bool
plasma_planet_random(void *context, float *value, struct yt_error *error)
{
	struct plasma_planet_tape *tape = context;
	size_t position = tape->draw_position++;

	(void)error;
	if (!plasma_planet_step(tape, PLASMA_PLANET_RANDOM)
	    || position >= tape->draw_count)
		return false;
	*value = tape->draws[position];
	return true;
}

static bool
plasma_planet_source(struct yt_planet *planet, uint8_t salt,
    const uint8_t *name, size_t name_length, const float production[3],
    const float stock[3], float ground, float owner)
{
	struct yt_record record;
	size_t index;

	yt_record_blank(&record);
	if (name_length > YT_TEXT_FIELD_SIZE)
		return false;
	if (name_length != 0U)
		memcpy(record.bytes, name, name_length);
	record.bytes[YT_RECORD_TAIL_OFFSET] = salt;
	record.bytes[YT_RECORD_TAIL_OFFSET + 1U] = (uint8_t)(salt ^ 0x55U);
	record.bytes[YT_RECORD_TAIL_OFFSET + 2U] = (uint8_t)(salt ^ 0xaaU);
	record.bytes[YT_RECORD_TAIL_OFFSET + 3U] = (uint8_t)~salt;
	if (!yt_record_set_number(&record, YT_F41, (float)salt)
	    || !yt_record_set_number(&record, YT_F73, owner)
	    || !yt_record_set_number(&record, YT_F77, ground)
	    || !yt_record_set_number(&record, YT_F85, (float)name_length))
		return false;
	for (index = 0U; index < 3U; ++index) {
		if (!yt_record_set_number(&record, YT_F45 + index * 4U,
		    production[index])
		    || !yt_record_set_number(&record, YT_F57 + index * 4U,
		    stock[index]))
			return false;
	}
	yt_planet_decode(planet, &record);
	return true;
}

static bool
plasma_planet_fixture(struct plasma_planet_tape *tape,
    struct yt_projectile_plasma_planet_state *state, double *energy)
{
	static const uint8_t attacker[] = {'A', 0, 'B'};
	static const uint8_t planet_name[] = {'P', 0, 'Q'};
	static const float opening_production[3] = {0.25f, 0.25f, 0.25f};
	static const float opening_stock[3] = {5.0f, 5.0f, 5.0f};
	static const float fresh_production[3] = {7.0f, 8.0f, 9.0f};
	static const float fresh_stock[3] = {70.0f, 80.0f, 90.0f};
	struct yt_record sector_record;

	memset(tape, 0, sizeof(*tape));
	memset(state, 0, sizeof(*state));
	tape->fail_at = SIZE_MAX;
	tape->logical_planet = 12;
	tape->sector = 42;
	tape->draws[0] = 0.5f;
	tape->draw_count = 1U;
	if (!plasma_planet_source(&tape->planet_source[0], 0x11,
	    planet_name, sizeof(planet_name), opening_production, opening_stock,
	    2.0f, 7.0f)
	    || !plasma_planet_source(&tape->planet_source[1], 0x22,
	    (const uint8_t *)"Commit", 6U, fresh_production, fresh_stock,
	    88.0f, 13.0f)
	    || !plasma_planet_source(&tape->planet_source[2], 0x33,
	    (const uint8_t *)"Destroy", 7U, fresh_production, fresh_stock,
	    77.0f, 17.0f))
		return false;
	yt_record_blank(&sector_record);
	sector_record.bytes[YT_RECORD_TAIL_OFFSET] = 0x5a;
	if (!yt_record_set_number(&sector_record, YT_F93, 12.0f))
		return false;
	yt_sector_decode(&tape->sector_source, &sector_record);
	*energy = 100000.0;
	state->planet = tape->logical_planet;
	state->sector = tape->sector;
	state->attacker = attacker;
	state->attacker_length = sizeof(attacker);
	state->energy = energy;
	return true;
}

static bool
check_projectile_plasma_planet_transaction(void)
{
	static const struct yt_projectile_plasma_planet_ops ops = {
		plasma_planet_update,
		plasma_planet_read,
		plasma_planet_write,
		plasma_planet_sector_read,
		plasma_planet_sector_write,
		plasma_planet_present,
		plasma_planet_news,
		plasma_planet_sound,
		plasma_planet_random,
	};
	static const int destroy_events[] = {
		PLASMA_PLANET_UPDATE, PLASMA_PLANET_READ,
		PLASMA_PLANET_PRESENT, PLASMA_PLANET_NEWS, PLASMA_PLANET_SOUND,
		PLASMA_PLANET_RANDOM,
		PLASMA_PLANET_PRESENT, PLASMA_PLANET_NEWS,
		PLASMA_PLANET_READ, PLASMA_PLANET_WRITE,
		PLASMA_PLANET_READ, PLASMA_PLANET_WRITE,
		PLASMA_PLANET_SECTOR_READ, PLASMA_PLANET_SECTOR_WRITE,
		PLASMA_PLANET_PRESENT, PLASMA_PLANET_SOUND, PLASMA_PLANET_NEWS,
	};
	static const int survivor_events[] = {
		PLASMA_PLANET_UPDATE, PLASMA_PLANET_READ,
		PLASMA_PLANET_PRESENT, PLASMA_PLANET_NEWS, PLASMA_PLANET_SOUND,
		PLASMA_PLANET_RANDOM, PLASMA_PLANET_RANDOM,
		PLASMA_PLANET_PRESENT, PLASMA_PLANET_NEWS,
		PLASMA_PLANET_READ, PLASMA_PLANET_WRITE,
		PLASMA_PLANET_PRESENT, PLASMA_PLANET_NEWS,
	};
	static const uint8_t hit_row[] =
	    "The plasma bolts hit planet P\0Q in sector 42!";
	static const uint8_t hit_news[] =
	    "A\0B's plasma bolts hit planet P\0Q in sector 42!";
	static const uint8_t productivity_row[] =
	    "Productivity reduced by .28125 units to 59.71875 units!";
	static const uint8_t ground_row[] =
	    "Ground forces reduced by .75 units to 11!";
	static const uint8_t destroyed_row[] = "The planet was destroyed!!";
	static const float survivor_production[3] = {10.0f, 20.0f, 30.0f};
	static const float survivor_stock[3] = {101.0f, 199.0f, 301.0f};
	static const float expected_production[3] = {
		9.90625f, 19.90625f, 29.90625f
	};
	static const float expected_stock[3] = {
		99.0625f, 199.0f, 299.0625f
	};
	struct plasma_planet_tape tape;
	struct yt_projectile_plasma_planet_state state;
	struct yt_record expected_planet;
	struct yt_record expected_sector;
	double energy;
	size_t index;
	size_t failure;

	if (!plasma_planet_fixture(&tape, &state, &energy)
	    || !plasma_planet_source(&tape.planet_source[0], 0x11,
	    (const uint8_t *)"P\0Q", 3U, survivor_production, survivor_stock,
	    11.75f, 7.0f))
		return false;
	energy = 15625.0;
	tape.draws[0] = 0.3125f;
	tape.draws[1] = 0.3125f;
	tape.draw_count = 2U;
	if (!yt_projectile_plasma_planet_run(&state, &ops, &tape, NULL)
	    || tape.event_count != YT_ARRAY_LEN(survivor_events)
	    || memcmp(tape.events, survivor_events, sizeof(survivor_events)) != 0
	    || tape.draw_position != 2U || energy != 0.0
	    || state.route != YT_PROJECTILE_PLASMA_PLANET_FOOTER
	    || state.destroyed || state.stale_ore != 0.0f
	    || state.original_productivity != 60.0f
	    || state.remaining_productivity != 59.71875f
	    || state.original_ground != 11.75f || state.remaining_ground != 11.0f
	    || memcmp(state.production, expected_production,
	    sizeof(expected_production)) != 0
	    || memcmp(state.stock, expected_stock, sizeof(expected_stock)) != 0
	    || tape.output_count != 3U || tape.news_count != 3U
	    || tape.sound_count != 1U || tape.selectors[0] != 2.0f
	    || tape.output_kind[0] != YT_PROJECTILE_PLASMA_PLANET_HIT_ROW
	    || tape.output_kind[1] !=
	    YT_PROJECTILE_PLASMA_PLANET_PRODUCTIVITY_ROW
	    || tape.output_kind[2] != YT_PROJECTILE_PLASMA_PLANET_GROUND_ROW
	    || tape.output_length[0] != sizeof(hit_row) - 1U
	    || memcmp(tape.output[0], hit_row, sizeof(hit_row) - 1U) != 0
	    || tape.news_length[0] != sizeof(hit_news) - 1U
	    || memcmp(tape.news[0], hit_news, sizeof(hit_news) - 1U) != 0
	    || tape.output_length[1] != sizeof(productivity_row) - 1U
	    || memcmp(tape.output[1], productivity_row,
	    sizeof(productivity_row) - 1U) != 0
	    || tape.news_length[1] != sizeof(productivity_row) - 1U
	    || memcmp(tape.news[1], productivity_row,
	    sizeof(productivity_row) - 1U) != 0
	    || tape.output_length[2] != sizeof(ground_row) - 1U
	    || memcmp(tape.output[2], ground_row, sizeof(ground_row) - 1U) != 0
	    || tape.news_length[2] != sizeof(ground_row) - 1U
	    || memcmp(tape.news[2], ground_row, sizeof(ground_row) - 1U) != 0
	    || tape.planet_reads != 2U || tape.planet_writes != 1U)
		return false;
	expected_planet = tape.planet_source[1].record;
	for (index = 0U; index < 3U; ++index) {
		if (!yt_record_set_number(&expected_planet,
		    YT_F45 + index * 4U, expected_production[index])
		    || !yt_record_set_number(&expected_planet,
		    YT_F57 + index * 4U, expected_stock[index]))
			return false;
	}
	if (!yt_record_set_number(&expected_planet, YT_F77, 11.0f)
	    || memcmp(&tape.planet_write[0].record, &expected_planet,
	    sizeof(expected_planet)) != 0)
		return false;

	if (!plasma_planet_fixture(&tape, &state, &energy)
	    || !yt_projectile_plasma_planet_run(&state, &ops, &tape, NULL)
	    || tape.event_count != YT_ARRAY_LEN(destroy_events)
	    || memcmp(tape.events, destroy_events, sizeof(destroy_events)) != 0
	    || tape.draw_position != 1U || energy != 87500.0
	    || !state.destroyed
	    || state.route != YT_PROJECTILE_PLASMA_PLANET_NEXT_HOP
	    || state.remaining_ground != 1.0f
	    || tape.planet_reads != 3U || tape.planet_writes != 2U
	    || tape.output_count != 3U || tape.news_count != 3U
	    || tape.sound_count != 2U || tape.selectors[0] != 2.0f
	    || tape.selectors[1] != 3.0f
	    || tape.output_kind[2] !=
	    YT_PROJECTILE_PLASMA_PLANET_DESTROYED_ROW
	    || tape.output_length[2] != sizeof(destroyed_row) - 1U
	    || memcmp(tape.output[2], destroyed_row,
	    sizeof(destroyed_row) - 1U) != 0
	    || tape.news_length[2] != sizeof(destroyed_row) - 1U
	    || memcmp(tape.news[2], destroyed_row,
	    sizeof(destroyed_row) - 1U) != 0)
		return false;
	expected_planet = tape.planet_source[1].record;
	for (index = 0U; index < 3U; ++index) {
		if (!yt_record_set_number(&expected_planet,
		    YT_F45 + index * 4U, 0.0f)
		    || !yt_record_set_number(&expected_planet,
		    YT_F57 + index * 4U, 0.0f))
			return false;
	}
	if (!yt_record_set_number(&expected_planet, YT_F77, 1.0f)
	    || memcmp(&tape.planet_write[0].record, &expected_planet,
	    sizeof(expected_planet)) != 0)
		return false;
	expected_planet = tape.planet_source[2].record;
	if (!yt_record_set_number(&expected_planet, YT_F85, 0.0f)
	    || memcmp(&tape.planet_write[1].record, &expected_planet,
	    sizeof(expected_planet)) != 0
	    || memcmp(tape.planet_write[1].record.bytes + YT_F85,
	    "\0\0\0\0", 4U) != 0)
		return false;
	expected_sector = tape.sector_source.record;
	if (!yt_record_set_number(&expected_sector, YT_F93, 0.0f)
	    || memcmp(&tape.sector_write.record, &expected_sector,
	    sizeof(expected_sector)) != 0
	    || memcmp(tape.sector_write.record.bytes + YT_F93,
	    "\0\0\0\0", 4U) != 0)
		return false;

	/* The loop must use updater-return P(1), even when the loaded row is zero. */
	if (!plasma_planet_fixture(&tape, &state, &energy))
		return false;
	{
		static const float zero[3] = {0.0f, 0.0f, 0.0f};

		if (!plasma_planet_source(&tape.planet_source[0], 0x11,
		    (const uint8_t *)"P\0Q", 3U, zero, zero, 0.0f, 7.0f))
			return false;
	}
	tape.stale_ore = 3.0f;
	tape.draws[0] = 0.625f;
	tape.draw_count = 1U;
	energy = 15625.0;
	if (!yt_projectile_plasma_planet_run(&state, &ops, &tape, NULL)
	    || tape.draw_position != 1U || state.stale_ore != 3.0f
	    || energy != 0.0
	    || state.route != YT_PROJECTILE_PLASMA_PLANET_FOOTER)
		return false;

	/* Conversely, fresh P(1) alone cannot admit the loop when stale P(1) is zero. */
	if (!plasma_planet_fixture(&tape, &state, &energy))
		return false;
	{
		static const float first_only[3] = {1.0f, 0.0f, 0.0f};
		static const float zero[3] = {0.0f, 0.0f, 0.0f};

		if (!plasma_planet_source(&tape.planet_source[0], 0x11,
		    (const uint8_t *)"P\0Q", 3U, first_only, zero, 2.0f, 7.0f))
			return false;
	}
	energy = 1000.0;
	if (!yt_projectile_plasma_planet_run(&state, &ops, &tape, NULL)
	    || tape.draw_position != 0U || energy != 1000.0
	    || state.production[0] != 1.0f || state.remaining_ground != 2.0f
	    || state.destroyed
	    || state.route != YT_PROJECTILE_PLASMA_PLANET_NEXT_HOP)
		return false;

	for (failure = 1U; failure <= YT_ARRAY_LEN(destroy_events); ++failure) {
		if (!plasma_planet_fixture(&tape, &state, &energy))
			return false;
		tape.fail_at = failure;
		if (yt_projectile_plasma_planet_run(&state, &ops, &tape, NULL)
		    || tape.event_count != failure
		    || memcmp(tape.events, destroy_events,
		    failure * sizeof(destroy_events[0])) != 0)
			return false;
	}
	if (!plasma_planet_fixture(&tape, &state, &energy))
		return false;
	state.attacker = NULL;
	return !yt_projectile_plasma_planet_run(NULL, &ops, &tape, NULL)
	    && !yt_projectile_plasma_planet_run(&state, NULL, &tape, NULL)
	    && !yt_projectile_plasma_planet_run(&state, &ops, &tape, NULL);
}

struct plasma_footer_tape {
	enum yt_projectile_plasma_footer_output_kind kinds[3];
	uint8_t text[3][32];
	size_t lengths[3];
	size_t calls;
	size_t fail_at;
};

static bool
plasma_footer_present(void *context, const uint8_t *text, size_t length,
    enum yt_projectile_plasma_footer_output_kind kind,
    struct yt_error *error)
{
	struct plasma_footer_tape *tape = context;
	size_t position = tape->calls++;

	(void)error;
	if (position >= YT_ARRAY_LEN(tape->kinds)
	    || length > sizeof(tape->text[position])
	    || (length == 0U && text != NULL)
	    || (length != 0U && text == NULL))
		return false;
	tape->kinds[position] = kind;
	tape->lengths[position] = length;
	if (length != 0U)
		memcpy(tape->text[position], text, length);
	return tape->calls != tape->fail_at;
}

static bool
check_projectile_plasma_footer_transaction(void)
{
	static const struct yt_projectile_plasma_footer_ops ops = {
		plasma_footer_present,
	};
	static const uint8_t row[] = "Plasma bolts dissipated.";
	struct plasma_footer_tape tape;
	size_t failure;

	memset(&tape, 0, sizeof(tape));
	tape.fail_at = SIZE_MAX;
	if (!yt_projectile_plasma_footer_run(&ops, &tape, NULL)
	    || tape.calls != 3U
	    || tape.kinds[0] != YT_PROJECTILE_PLASMA_FOOTER_LEADING_BLANK
	    || tape.kinds[1] != YT_PROJECTILE_PLASMA_FOOTER_TEXT
	    || tape.kinds[2] != YT_PROJECTILE_PLASMA_FOOTER_TRAILING_BLANK
	    || tape.lengths[0] != 0U || tape.lengths[2] != 0U
	    || tape.lengths[1] != sizeof(row) - 1U
	    || memcmp(tape.text[1], row, sizeof(row) - 1U) != 0)
		return false;
	for (failure = 1U; failure <= 3U; ++failure) {
		memset(&tape, 0, sizeof(tape));
		tape.fail_at = failure;
		if (yt_projectile_plasma_footer_run(&ops, &tape, NULL)
		    || tape.calls != failure)
			return false;
	}
	return !yt_projectile_plasma_footer_run(NULL, &tape, NULL);
}

enum projectile_defense_front_event {
	PROJECTILE_DEFENSE_OWNER = 1,
	PROJECTILE_DEFENSE_FRIENDSHIP,
	PROJECTILE_DEFENSE_PRESENT,
	PROJECTILE_DEFENSE_SOUND,
};

struct projectile_defense_front_tape {
	int events[4];
	size_t event_count;
	size_t fail_at;
	bool friendly;
	float owner_at_read;
	float owner_at_friendship;
	uint8_t owner_name[YT_TEXT_FIELD_SIZE];
	size_t owner_name_length;
	uint8_t row[256];
	size_t row_length;
	float selector;
};

static bool
projectile_defense_front_step(struct projectile_defense_front_tape *tape,
    enum projectile_defense_front_event event)
{
	if (tape->event_count >= YT_ARRAY_LEN(tape->events))
		return false;
	tape->events[tape->event_count++] = (int)event;
	return tape->event_count != tape->fail_at;
}

static bool
projectile_defense_front_owner(void *context, float owner, uint8_t *name,
    size_t *name_length, struct yt_error *error)
{
	struct projectile_defense_front_tape *tape = context;

	(void)error;
	tape->owner_at_read = owner;
	if (!projectile_defense_front_step(tape, PROJECTILE_DEFENSE_OWNER))
		return false;
	memcpy(name, tape->owner_name, tape->owner_name_length);
	*name_length = tape->owner_name_length;
	return true;
}

static bool
projectile_defense_front_friendship(void *context, float owner,
    bool *friendly, struct yt_error *error)
{
	struct projectile_defense_front_tape *tape = context;

	(void)error;
	tape->owner_at_friendship = owner;
	if (!projectile_defense_front_step(tape,
	    PROJECTILE_DEFENSE_FRIENDSHIP))
		return false;
	*friendly = tape->friendly;
	return true;
}

static bool
projectile_defense_front_present(void *context, const uint8_t *text,
    size_t length, struct yt_error *error)
{
	struct projectile_defense_front_tape *tape = context;

	(void)error;
	if (!projectile_defense_front_step(tape, PROJECTILE_DEFENSE_PRESENT)
	    || length > sizeof(tape->row))
		return false;
	memcpy(tape->row, text, length);
	tape->row_length = length;
	return true;
}

static bool
projectile_defense_front_sound(void *context, float selector,
    struct yt_error *error)
{
	struct projectile_defense_front_tape *tape = context;

	(void)error;
	tape->selector = selector;
	return projectile_defense_front_step(tape, PROJECTILE_DEFENSE_SOUND);
}

static void
projectile_defense_front_fixture(struct projectile_defense_front_tape *tape,
    struct yt_projectile_defense_front_state *state)
{
	memset(tape, 0, sizeof(*tape));
	tape->fail_at = SIZE_MAX;
	tape->owner_name[0] = 'A';
	tape->owner_name[1] = 0;
	tape->owner_name[2] = 'B';
	tape->owner_name_length = 3U;
	state->sector = 7.0f;
	state->fighters = 12.0;
	state->owner = 3.75f;
	state->shooter = 2.0f;
	state->route = YT_PROJECTILE_DEFENSE_NO_DEFENSE;
}

static bool
check_projectile_defense_front_transaction(void)
{
	static const struct yt_projectile_defense_front_ops ops = {
		projectile_defense_front_owner,
		projectile_defense_front_friendship,
		projectile_defense_front_present,
		projectile_defense_front_sound,
	};
	static const int hostile_events[] = {
		PROJECTILE_DEFENSE_OWNER,
		PROJECTILE_DEFENSE_FRIENDSHIP,
		PROJECTILE_DEFENSE_PRESENT,
		PROJECTILE_DEFENSE_SOUND,
	};
	static const uint8_t player_row[] =
	    "Sector: 7 defended by A\0B with 12 fighters.";
	static const uint8_t self_row[] =
	    "Sector: 7 defended by YOU with 12 fighters.";
	static const uint8_t xannor_row[] =
	    "Sector: 7 defended by THEM with 12 fighters.";
	static const uint8_t mercenary_row[] =
	    "Sector: 7 defended by Mercenaries with 12 fighters.";
	struct projectile_defense_front_tape tape;
	struct yt_projectile_defense_front_state state;
	size_t failure;

	projectile_defense_front_fixture(&tape, &state);
	if (!yt_projectile_defense_front_run(&state, &ops, &tape, NULL)
	    || state.route != YT_PROJECTILE_DEFENSE_HOSTILE
	    || tape.event_count != YT_ARRAY_LEN(hostile_events)
	    || memcmp(tape.events, hostile_events, sizeof(hostile_events)) != 0
	    || tape.owner_at_read != 3.75f
	    || tape.owner_at_friendship != 3.75f || tape.selector != 2.0f
	    || tape.row_length != sizeof(player_row) - 1U
	    || memcmp(tape.row, player_row, sizeof(player_row) - 1U) != 0)
		return false;

	projectile_defense_front_fixture(&tape, &state);
	tape.friendly = true;
	if (!yt_projectile_defense_front_run(&state, &ops, &tape, NULL)
	    || state.route != YT_PROJECTILE_DEFENSE_FRIENDLY
	    || tape.event_count != 3U || tape.selector != 0.0f)
		return false;

	projectile_defense_front_fixture(&tape, &state);
	state.owner = (float)state.shooter;
	if (!yt_projectile_defense_front_run(&state, &ops, &tape, NULL)
	    || state.route != YT_PROJECTILE_DEFENSE_FRIENDLY
	    || tape.event_count != 3U
	    || tape.row_length != sizeof(self_row) - 1U
	    || memcmp(tape.row, self_row, sizeof(self_row) - 1U) != 0)
		return false;

	projectile_defense_front_fixture(&tape, &state);
	state.owner = -1.0f;
	state.shooter = -1.0f;
	if (!yt_projectile_defense_front_run(&state, &ops, &tape, NULL)
	    || state.route != YT_PROJECTILE_DEFENSE_FRIENDLY
	    || tape.event_count != 1U
	    || tape.events[0] != PROJECTILE_DEFENSE_PRESENT
	    || tape.row_length != sizeof(xannor_row) - 1U
	    || memcmp(tape.row, xannor_row, sizeof(xannor_row) - 1U) != 0)
		return false;

	projectile_defense_front_fixture(&tape, &state);
	state.owner = -2.0f;
	if (!yt_projectile_defense_front_run(&state, &ops, &tape, NULL)
	    || state.route != YT_PROJECTILE_DEFENSE_HOSTILE
	    || tape.event_count != 2U
	    || tape.events[0] != PROJECTILE_DEFENSE_PRESENT
	    || tape.events[1] != PROJECTILE_DEFENSE_SOUND
	    || tape.row_length != sizeof(mercenary_row) - 1U
	    || memcmp(tape.row, mercenary_row,
	    sizeof(mercenary_row) - 1U) != 0)
		return false;

	projectile_defense_front_fixture(&tape, &state);
	state.fighters = NAN;
	if (!yt_projectile_defense_front_run(&state, &ops, &tape, NULL)
	    || state.route != YT_PROJECTILE_DEFENSE_NO_DEFENSE
	    || tape.event_count != 0U)
		return false;
	state.fighters = -1.0;
	if (!yt_projectile_defense_front_run(&state, &ops, &tape, NULL)
	    || state.route != YT_PROJECTILE_DEFENSE_NO_DEFENSE
	    || tape.event_count != 0U)
		return false;

	for (failure = 1U; failure <= YT_ARRAY_LEN(hostile_events); ++failure) {
		projectile_defense_front_fixture(&tape, &state);
		tape.fail_at = failure;
		if (yt_projectile_defense_front_run(&state, &ops, &tape, NULL)
		    || tape.event_count != failure
		    || memcmp(tape.events, hostile_events,
		    failure * sizeof(hostile_events[0])) != 0
		    || state.route != (failure == 4U
		    ? YT_PROJECTILE_DEFENSE_HOSTILE
		    : YT_PROJECTILE_DEFENSE_NO_DEFENSE))
			return false;
	}
	return !yt_projectile_defense_front_run(NULL, &ops, &tape, NULL)
	    && !yt_projectile_defense_front_run(&state, NULL, &tape, NULL);
}

enum projectile_defense_combat_event {
	PROJECTILE_DEFENSE_RANDOM = 1,
	PROJECTILE_DEFENSE_DAMAGE_PRESENT,
	PROJECTILE_DEFENSE_NEWS,
	PROJECTILE_DEFENSE_READ,
	PROJECTILE_DEFENSE_WRITE,
	PROJECTILE_DEFENSE_VICTORY,
};

struct projectile_defense_combat_tape {
	int events[12];
	size_t event_count;
	size_t fail_at;
	float draws[4];
	size_t draw_position;
	uint8_t direct[256];
	size_t direct_length;
	uint8_t news[256];
	size_t news_length;
	struct yt_sector source;
	struct yt_sector written;
	float sector_at_read;
	float sector_at_write;
	int *provoker;
	int provoker_at_write;
};

static bool
projectile_defense_combat_step(struct projectile_defense_combat_tape *tape,
    enum projectile_defense_combat_event event)
{
	if (tape->event_count >= YT_ARRAY_LEN(tape->events))
		return false;
	tape->events[tape->event_count++] = (int)event;
	return tape->event_count != tape->fail_at;
}

static bool
projectile_defense_combat_random(void *context, float *value,
    struct yt_error *error)
{
	struct projectile_defense_combat_tape *tape = context;

	(void)error;
	if (!projectile_defense_combat_step(tape, PROJECTILE_DEFENSE_RANDOM)
	    || tape->draw_position >= YT_ARRAY_LEN(tape->draws))
		return false;
	*value = tape->draws[tape->draw_position++];
	return true;
}

static bool
projectile_defense_combat_present(void *context, const uint8_t *text,
    size_t length, struct yt_error *error)
{
	struct projectile_defense_combat_tape *tape = context;

	(void)error;
	if (!projectile_defense_combat_step(tape,
	    PROJECTILE_DEFENSE_DAMAGE_PRESENT) || length > sizeof(tape->direct))
		return false;
	memcpy(tape->direct, text, length);
	tape->direct_length = length;
	return true;
}

static bool
projectile_defense_combat_news(void *context, const uint8_t *text,
    size_t length, struct yt_error *error)
{
	struct projectile_defense_combat_tape *tape = context;

	(void)error;
	if (!projectile_defense_combat_step(tape, PROJECTILE_DEFENSE_NEWS)
	    || length > sizeof(tape->news))
		return false;
	memcpy(tape->news, text, length);
	tape->news_length = length;
	return true;
}

static bool
projectile_defense_combat_read(void *context, float sector,
    struct yt_sector *value, struct yt_error *error)
{
	struct projectile_defense_combat_tape *tape = context;

	(void)error;
	tape->sector_at_read = sector;
	if (!projectile_defense_combat_step(tape, PROJECTILE_DEFENSE_READ))
		return false;
	*value = tape->source;
	return true;
}

static bool
projectile_defense_combat_write(void *context, float sector,
    const struct yt_sector *value, struct yt_error *error)
{
	struct projectile_defense_combat_tape *tape = context;

	(void)error;
	tape->sector_at_write = sector;
	tape->written = *value;
	tape->provoker_at_write = *tape->provoker;
	return projectile_defense_combat_step(tape, PROJECTILE_DEFENSE_WRITE);
}

static bool
projectile_defense_combat_victory(void *context, struct yt_error *error)
{
	struct projectile_defense_combat_tape *tape = context;

	(void)error;
	return projectile_defense_combat_step(tape, PROJECTILE_DEFENSE_VICTORY);
}

static void
projectile_defense_combat_fixture(
    struct projectile_defense_combat_tape *tape,
    struct yt_projectile_defense_combat_state *state, float *missiles,
    int *provoker)
{
	static const uint8_t shooter_name[] = {'A', 0, 'B'};

	memset(tape, 0, sizeof(*tape));
	memset(state, 0, sizeof(*state));
	memset(&tape->source, 0, sizeof(tape->source));
	memset(tape->source.record.bytes, 0xa5,
	    sizeof(tape->source.record.bytes));
	tape->source.fighters = 999.0f;
	tape->source.fighter_owner = -1.0f;
	(void)yt_record_set_number(&tape->source.record, YT_F81,
	    tape->source.fighters);
	(void)yt_record_set_number(&tape->source.record, YT_F85,
	    tape->source.fighter_owner);
	tape->fail_at = SIZE_MAX;
	tape->draws[0] = 0.002f;
	tape->draws[1] = 0.003f;
	tape->provoker = provoker;
	*missiles = 2.5f;
	*provoker = 99;
	state->sector = 7.0f;
	state->fighters = 100.0;
	state->owner = -1.0f;
	state->shooter = 2;
	state->headquarters = 85.0f;
	state->shooter_name = shooter_name;
	state->shooter_name_length = sizeof(shooter_name);
	state->missiles = missiles;
	state->xannor_provoker = provoker;
}

static bool
check_projectile_defense_combat_transaction(void)
{
	static const struct yt_projectile_defense_combat_ops ops = {
		projectile_defense_combat_random,
		projectile_defense_combat_present,
		projectile_defense_combat_news,
		projectile_defense_combat_read,
		projectile_defense_combat_write,
		projectile_defense_combat_victory,
	};
	static const int ordinary_events[] = {
		PROJECTILE_DEFENSE_RANDOM,
		PROJECTILE_DEFENSE_RANDOM,
		PROJECTILE_DEFENSE_DAMAGE_PRESENT,
		PROJECTILE_DEFENSE_NEWS,
		PROJECTILE_DEFENSE_READ,
		PROJECTILE_DEFENSE_WRITE,
	};
	static const int victory_events[] = {
		PROJECTILE_DEFENSE_RANDOM,
		PROJECTILE_DEFENSE_DAMAGE_PRESENT,
		PROJECTILE_DEFENSE_NEWS,
		PROJECTILE_DEFENSE_READ,
		PROJECTILE_DEFENSE_WRITE,
		PROJECTILE_DEFENSE_VICTORY,
	};
	static const uint8_t direct[] =
	    "The Missiles destroyed 25 fighters!";
	static const uint8_t news[] =
	    "A\0B's Missiles destroyed 25 fighters in sector 7!";
	static const uint8_t zero_direct[] =
	    "The Missiles destroyed 0 fighters!";
	struct projectile_defense_combat_tape tape;
	struct yt_projectile_defense_combat_state state;
	struct yt_record expected;
	float missiles;
	int provoker;
	size_t failure;

	projectile_defense_combat_fixture(&tape, &state, &missiles, &provoker);
	expected = tape.source.record;
	(void)yt_record_set_number(&expected, YT_F81, 75.0f);
	if (!yt_projectile_defense_combat_run(&state, &ops, &tape, NULL)
	    || tape.event_count != YT_ARRAY_LEN(ordinary_events)
	    || memcmp(tape.events, ordinary_events, sizeof(ordinary_events)) != 0
	    || state.saved_missiles != 2.5f || state.destroyed != 25.0f
	    || state.counter != 3.0f || state.remaining_fighters != 75.0
	    || missiles != 0.5f || provoker != 2
	    || tape.provoker_at_write != 2
	    || state.route != YT_PROJECTILE_DEFENSE_RETURN
	    || state.victory_called || tape.sector_at_read != 7.0f
	    || tape.sector_at_write != 7.0f
	    || tape.direct_length != sizeof(direct) - 1U
	    || memcmp(tape.direct, direct, sizeof(direct) - 1U) != 0
	    || tape.news_length != sizeof(news) - 1U
	    || memcmp(tape.news, news, sizeof(news) - 1U) != 0
	    || tape.written.fighters != 75.0f
	    || tape.written.fighter_owner != -1.0f
	    || memcmp(&tape.written.record, &expected, sizeof(expected)) != 0)
		return false;

	projectile_defense_combat_fixture(&tape, &state, &missiles, &provoker);
	missiles = 0.5f;
	if (!yt_projectile_defense_combat_run(&state, &ops, &tape, NULL)
	    || tape.event_count != 3U
	    || tape.events[0] != PROJECTILE_DEFENSE_DAMAGE_PRESENT
	    || tape.events[1] != PROJECTILE_DEFENSE_READ
	    || tape.events[2] != PROJECTILE_DEFENSE_WRITE
	    || tape.draw_position != 0U || state.destroyed != 0.0f
	    || state.counter != 1.0f || missiles != 0.5f
	    || state.remaining_fighters != 100.0
	    || state.route != YT_PROJECTILE_DEFENSE_RETURN
	    || tape.direct_length != sizeof(zero_direct) - 1U
	    || memcmp(tape.direct, zero_direct, sizeof(zero_direct) - 1U) != 0)
		return false;

	projectile_defense_combat_fixture(&tape, &state, &missiles, &provoker);
	missiles = NAN;
	if (!yt_projectile_defense_combat_run(&state, &ops, &tape, NULL)
	    || tape.event_count != 0U
	    || state.route != YT_PROJECTILE_DEFENSE_CONTINUE_MINES
	    || state.saved_missiles != 0.0f || state.counter != 1.0f)
		return false;
	missiles = -0.1f;
	if (!yt_projectile_defense_combat_run(&state, &ops, &tape, NULL)
	    || tape.event_count != 0U
	    || state.route != YT_PROJECTILE_DEFENSE_CONTINUE_MINES)
		return false;

	projectile_defense_combat_fixture(&tape, &state, &missiles, &provoker);
	state.fighters = 10.0;
	state.headquarters = 7.0f;
	missiles = 1.0f;
	tape.draws[0] = 0.9f;
	if (!yt_projectile_defense_combat_run(&state, &ops, &tape, NULL)
	    || tape.event_count != YT_ARRAY_LEN(victory_events)
	    || memcmp(tape.events, victory_events, sizeof(victory_events)) != 0
	    || state.destroyed != 10.0f || state.remaining_fighters != 0.0
	    || missiles != 0.0f || provoker != 99 || !state.victory_called
	    || state.route != YT_PROJECTILE_DEFENSE_RETURN
	    || memcmp(tape.written.record.bytes + YT_F81,
	    (const uint8_t[]){0x00, 0x00, 0x10, 0x00}, 4U) != 0
	    || memcmp(tape.written.record.bytes + YT_F85,
	    (const uint8_t[]){0x00, 0x00, 0x10, 0x00}, 4U) != 0)
		return false;

	projectile_defense_combat_fixture(&tape, &state, &missiles, &provoker);
	state.shooter = -1;
	missiles = 1.0f;
	tape.draws[0] = 0.0f;
	if (!yt_projectile_defense_combat_run(&state, &ops, &tape, NULL)
	    || provoker != -1 || tape.provoker_at_write != -1
	    || state.route != YT_PROJECTILE_DEFENSE_RETURN)
		return false;

	for (failure = 1U; failure <= YT_ARRAY_LEN(victory_events); ++failure) {
		projectile_defense_combat_fixture(&tape, &state, &missiles,
		    &provoker);
		state.fighters = 10.0;
		state.headquarters = 7.0f;
		missiles = 1.0f;
		tape.draws[0] = 0.9f;
		tape.fail_at = failure;
		if (yt_projectile_defense_combat_run(&state, &ops, &tape, NULL)
		    || tape.event_count != failure
		    || memcmp(tape.events, victory_events,
		    failure * sizeof(victory_events[0])) != 0
		    || state.victory_called != (failure == 6U))
			return false;
	}
	return !yt_projectile_defense_combat_run(NULL, &ops, &tape, NULL)
	    && !yt_projectile_defense_combat_run(&state, NULL, &tape, NULL);
}

enum projectile_sector_mine_event {
	PROJECTILE_SECTOR_MINE_READ = 1,
	PROJECTILE_SECTOR_MINE_PRESENT,
	PROJECTILE_SECTOR_MINE_SOUND,
	PROJECTILE_SECTOR_MINE_NEWS,
	PROJECTILE_SECTOR_MINE_WRITE,
};

struct projectile_sector_mine_tape {
	int events[16];
	size_t event_count;
	size_t fail_at;
	struct yt_sector reads[4];
	size_t read_position;
	uint8_t rows[2][256];
	size_t row_lengths[2];
	size_t row_count;
	uint8_t news[256];
	size_t news_length;
	float selector;
	float sector_at_read[4];
	float sector_at_write;
	struct yt_sector written;
};

static bool
projectile_sector_mine_step(struct projectile_sector_mine_tape *tape,
    enum projectile_sector_mine_event event)
{
	if (tape->event_count >= YT_ARRAY_LEN(tape->events))
		return false;
	tape->events[tape->event_count++] = (int)event;
	return tape->event_count != tape->fail_at;
}

static bool
projectile_sector_mine_read(void *context, float sector,
    struct yt_sector *value, struct yt_error *error)
{
	struct projectile_sector_mine_tape *tape = context;
	size_t position = tape->read_position;

	(void)error;
	if (position >= YT_ARRAY_LEN(tape->reads)
	    || !projectile_sector_mine_step(tape,
	    PROJECTILE_SECTOR_MINE_READ))
		return false;
	tape->sector_at_read[position] = sector;
	*value = tape->reads[position];
	tape->read_position++;
	return true;
}

static bool
projectile_sector_mine_present(void *context, const uint8_t *text,
    size_t length, struct yt_error *error)
{
	struct projectile_sector_mine_tape *tape = context;
	size_t position = tape->row_count;

	(void)error;
	if (position >= YT_ARRAY_LEN(tape->rows)
	    || length > sizeof(tape->rows[position])
	    || !projectile_sector_mine_step(tape,
	    PROJECTILE_SECTOR_MINE_PRESENT))
		return false;
	memcpy(tape->rows[position], text, length);
	tape->row_lengths[position] = length;
	tape->row_count++;
	return true;
}

static bool
projectile_sector_mine_sound(void *context, float selector,
    struct yt_error *error)
{
	struct projectile_sector_mine_tape *tape = context;

	(void)error;
	tape->selector = selector;
	return projectile_sector_mine_step(tape, PROJECTILE_SECTOR_MINE_SOUND);
}

static bool
projectile_sector_mine_news(void *context, const uint8_t *text,
    size_t length, struct yt_error *error)
{
	struct projectile_sector_mine_tape *tape = context;

	(void)error;
	if (length > sizeof(tape->news)
	    || !projectile_sector_mine_step(tape,
	    PROJECTILE_SECTOR_MINE_NEWS))
		return false;
	memcpy(tape->news, text, length);
	tape->news_length = length;
	return true;
}

static bool
projectile_sector_mine_write(void *context, float sector,
    const struct yt_sector *value, struct yt_error *error)
{
	struct projectile_sector_mine_tape *tape = context;

	(void)error;
	if (!projectile_sector_mine_step(tape,
	    PROJECTILE_SECTOR_MINE_WRITE))
		return false;
	tape->sector_at_write = sector;
	tape->written = *value;
	if (tape->read_position < YT_ARRAY_LEN(tape->reads))
		tape->reads[tape->read_position] = *value;
	return true;
}

static void
projectile_sector_mine_fixture(struct projectile_sector_mine_tape *tape,
    struct yt_projectile_sector_mine_state *state, float *missiles,
    float *last_news)
{
	static const uint8_t shooter_name[] = {'A', 0, 'B'};

	memset(tape, 0, sizeof(*tape));
	memset(state, 0, sizeof(*state));
	memset(tape->reads[0].record.bytes, 0xa5,
	    sizeof(tape->reads[0].record.bytes));
	memset(tape->reads[1].record.bytes, 0x5a,
	    sizeof(tape->reads[1].record.bytes));
	tape->reads[0].mines = 5.25f;
	tape->reads[1].mines = 999.0f;
	(void)yt_record_set_number(&tape->reads[0].record, YT_F129,
	    tape->reads[0].mines);
	(void)yt_record_set_number(&tape->reads[1].record, YT_F129,
	    tape->reads[1].mines);
	tape->fail_at = SIZE_MAX;
	*missiles = 2.5f;
	*last_news = -3.5f;
	state->sector = 7.0f;
	state->shooter_name = shooter_name;
	state->shooter_name_length = sizeof(shooter_name);
	state->missiles = missiles;
	state->last_news_sector = last_news;
}

static bool
check_projectile_sector_mine_transaction(void)
{
	static const struct yt_projectile_sector_mine_ops ops = {
		projectile_sector_mine_read,
		projectile_sector_mine_present,
		projectile_sector_mine_sound,
		projectile_sector_mine_news,
		projectile_sector_mine_write,
	};
	static const int ordinary_events[] = {
		PROJECTILE_SECTOR_MINE_READ,
		PROJECTILE_SECTOR_MINE_PRESENT,
		PROJECTILE_SECTOR_MINE_SOUND,
		PROJECTILE_SECTOR_MINE_NEWS,
		PROJECTILE_SECTOR_MINE_PRESENT,
		PROJECTILE_SECTOR_MINE_READ,
		PROJECTILE_SECTOR_MINE_WRITE,
	};
	static const int suppressed_events[] = {
		PROJECTILE_SECTOR_MINE_READ,
		PROJECTILE_SECTOR_MINE_PRESENT,
		PROJECTILE_SECTOR_MINE_SOUND,
		PROJECTILE_SECTOR_MINE_PRESENT,
		PROJECTILE_SECTOR_MINE_READ,
		PROJECTILE_SECTOR_MINE_WRITE,
	};
	static const int reentry_events[] = {
		PROJECTILE_SECTOR_MINE_READ,
		PROJECTILE_SECTOR_MINE_PRESENT,
		PROJECTILE_SECTOR_MINE_SOUND,
		PROJECTILE_SECTOR_MINE_NEWS,
		PROJECTILE_SECTOR_MINE_PRESENT,
		PROJECTILE_SECTOR_MINE_READ,
		PROJECTILE_SECTOR_MINE_WRITE,
		PROJECTILE_SECTOR_MINE_READ,
	};
	static const uint8_t hit[] =
	    "The missiles hit 5.25 SECTOR MINES in sector 7!";
	static const uint8_t news[] =
	    "A\0B's Missiles hit sector mines in sector 7!";
	static const uint8_t destroyed[] =
	    "The missiles destroyed 2.5 mines!";
	static const uint8_t singular[] =
	    "The missile destroyed .5 mine!";
	struct projectile_sector_mine_tape tape;
	struct yt_projectile_sector_mine_state state;
	struct yt_record expected;
	float missiles;
	float last_news;
	size_t failure;

	projectile_sector_mine_fixture(&tape, &state, &missiles, &last_news);
	expected = tape.reads[1].record;
	(void)yt_record_set_number(&expected, YT_F129, 2.75f);
	if (!yt_projectile_sector_mine_run(&state, &ops, &tape, NULL)
	    || tape.event_count != YT_ARRAY_LEN(ordinary_events)
	    || memcmp(tape.events, ordinary_events, sizeof(ordinary_events)) != 0
	    || tape.read_position != 2U || tape.row_count != 2U
	    || state.observed_mines != 5.25 || state.destroyed != 2.5f
	    || missiles != 0.0f || last_news != 7.0f
	    || state.route != YT_PROJECTILE_SECTOR_MINE_RETURN
	    || tape.selector != 5.0f || tape.sector_at_read[0] != 7.0f
	    || tape.sector_at_read[1] != 7.0f
	    || tape.sector_at_write != 7.0f
	    || tape.row_lengths[0] != sizeof(hit) - 1U
	    || memcmp(tape.rows[0], hit, sizeof(hit) - 1U) != 0
	    || tape.news_length != sizeof(news) - 1U
	    || memcmp(tape.news, news, sizeof(news) - 1U) != 0
	    || tape.row_lengths[1] != sizeof(destroyed) - 1U
	    || memcmp(tape.rows[1], destroyed, sizeof(destroyed) - 1U) != 0
	    || tape.written.mines != 2.75f
	    || memcmp(&tape.written.record, &expected, sizeof(expected)) != 0)
		return false;

	projectile_sector_mine_fixture(&tape, &state, &missiles, &last_news);
	last_news = 7.0f;
	if (!yt_projectile_sector_mine_run(&state, &ops, &tape, NULL)
	    || tape.event_count != YT_ARRAY_LEN(suppressed_events)
	    || memcmp(tape.events, suppressed_events,
	    sizeof(suppressed_events)) != 0
	    || tape.news_length != 0U || last_news != 7.0f)
		return false;

	projectile_sector_mine_fixture(&tape, &state, &missiles, &last_news);
	tape.reads[0].mines = 0.5f;
	(void)yt_record_set_number(&tape.reads[0].record, YT_F129, 0.5f);
	missiles = 2.0f;
	if (!yt_projectile_sector_mine_run(&state, &ops, &tape, NULL)
	    || tape.event_count != YT_ARRAY_LEN(reentry_events)
	    || memcmp(tape.events, reentry_events, sizeof(reentry_events)) != 0
	    || tape.read_position != 3U || missiles != 1.5f
	    || state.destroyed != 0.5f || state.observed_mines != 0.0
	    || state.route != YT_PROJECTILE_SECTOR_MINE_CONTINUE_PLAYERS
	    || tape.row_lengths[1] != sizeof(singular) - 1U
	    || memcmp(tape.rows[1], singular, sizeof(singular) - 1U) != 0
	    || tape.written.mines != 0.0f)
		return false;

	projectile_sector_mine_fixture(&tape, &state, &missiles, &last_news);
	tape.reads[0].mines = NAN;
	for (failure = 0U; failure < 3U; ++failure) {
		if (failure == 1U)
			tape.reads[0].mines = 0.0f;
		else if (failure == 2U)
			tape.reads[0].mines = -0.5f;
		if (!yt_projectile_sector_mine_run(&state, &ops, &tape, NULL)
		    || tape.event_count != 1U
		    || tape.events[0] != PROJECTILE_SECTOR_MINE_READ
		    || state.route != YT_PROJECTILE_SECTOR_MINE_CONTINUE_PLAYERS)
			return false;
		projectile_sector_mine_fixture(&tape, &state, &missiles,
		    &last_news);
	}

	for (failure = 1U; failure <= YT_ARRAY_LEN(ordinary_events); ++failure) {
		projectile_sector_mine_fixture(&tape, &state, &missiles,
		    &last_news);
		tape.fail_at = failure;
		if (yt_projectile_sector_mine_run(&state, &ops, &tape, NULL)
		    || tape.event_count != failure
		    || memcmp(tape.events, ordinary_events,
		    failure * sizeof(ordinary_events[0])) != 0)
			return false;
	}
	return !yt_projectile_sector_mine_run(NULL, &ops, &tape, NULL)
	    && !yt_projectile_sector_mine_run(&state, NULL, &tape, NULL);
}

enum projectile_planet_event {
	PROJECTILE_PLANET_RANDOM = 1,
	PROJECTILE_PLANET_READ,
	PROJECTILE_PLANET_WRITE,
	PROJECTILE_PLANET_SECTOR_READ,
	PROJECTILE_PLANET_SECTOR_WRITE,
	PROJECTILE_PLANET_PRESENT,
	PROJECTILE_PLANET_NEWS,
	PROJECTILE_PLANET_SOUND,
};

struct projectile_planet_tape {
	int events[24];
	size_t event_count;
	size_t fail_at;
	const float *draws;
	size_t draw_count;
	size_t draw_position;
	uint32_t physical_planet;
	uint32_t physical_sector;
	struct yt_planet planet_source[3];
	struct yt_planet planet_write[3];
	size_t planet_reads;
	size_t planet_writes;
	struct yt_sector sector_source;
	struct yt_sector sector_write;
	uint8_t present[3][128];
	size_t present_length[3];
	size_t present_count;
	uint8_t news[3][128];
	size_t news_length[3];
	size_t news_count;
	float selector;
};

static bool
projectile_planet_step(struct projectile_planet_tape *tape, int event)
{
	if (tape->event_count >= YT_ARRAY_LEN(tape->events))
		return false;
	tape->events[tape->event_count++] = event;
	return tape->event_count != tape->fail_at;
}

static bool
projectile_planet_random(void *context, float *value,
    struct yt_error *error)
{
	struct projectile_planet_tape *tape = context;
	size_t position = tape->draw_position++;

	(void)error;
	if (!projectile_planet_step(tape, PROJECTILE_PLANET_RANDOM)
	    || position >= tape->draw_count)
		return false;
	*value = tape->draws[position];
	return true;
}

static bool
projectile_planet_read(void *context, uint32_t physical_record,
    struct yt_planet *planet, struct yt_error *error)
{
	struct projectile_planet_tape *tape = context;
	size_t position = tape->planet_reads++;

	(void)error;
	if (!projectile_planet_step(tape, PROJECTILE_PLANET_READ)
	    || physical_record != tape->physical_planet
	    || position >= YT_ARRAY_LEN(tape->planet_source))
		return false;
	*planet = tape->planet_source[position];
	return true;
}

static bool
projectile_planet_write(void *context, uint32_t physical_record,
    struct yt_planet *planet, struct yt_error *error)
{
	struct projectile_planet_tape *tape = context;
	size_t position = tape->planet_writes++;

	(void)error;
	if (!projectile_planet_step(tape, PROJECTILE_PLANET_WRITE)
	    || physical_record != tape->physical_planet
	    || position >= YT_ARRAY_LEN(tape->planet_write))
		return false;
	tape->planet_write[position] = *planet;
	return true;
}

static bool
projectile_planet_sector_read(void *context, uint32_t physical_record,
    struct yt_sector *sector, struct yt_error *error)
{
	struct projectile_planet_tape *tape = context;

	(void)error;
	if (!projectile_planet_step(tape, PROJECTILE_PLANET_SECTOR_READ)
	    || physical_record != tape->physical_sector)
		return false;
	*sector = tape->sector_source;
	return true;
}

static bool
projectile_planet_sector_write(void *context, uint32_t physical_record,
    struct yt_sector *sector, struct yt_error *error)
{
	struct projectile_planet_tape *tape = context;

	(void)error;
	if (!projectile_planet_step(tape, PROJECTILE_PLANET_SECTOR_WRITE)
	    || physical_record != tape->physical_sector)
		return false;
	tape->sector_write = *sector;
	return true;
}

static bool
projectile_planet_present(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	struct projectile_planet_tape *tape = context;
	size_t position = tape->present_count++;

	(void)error;
	if (!projectile_planet_step(tape, PROJECTILE_PLANET_PRESENT)
	    || position >= YT_ARRAY_LEN(tape->present)
	    || length > sizeof(tape->present[position]))
		return false;
	memcpy(tape->present[position], text, length);
	tape->present_length[position] = length;
	return true;
}

static bool
projectile_planet_news(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	struct projectile_planet_tape *tape = context;
	size_t position = tape->news_count++;

	(void)error;
	if (!projectile_planet_step(tape, PROJECTILE_PLANET_NEWS)
	    || position >= YT_ARRAY_LEN(tape->news)
	    || length > sizeof(tape->news[position]))
		return false;
	memcpy(tape->news[position], text, length);
	tape->news_length[position] = length;
	return true;
}

static bool
projectile_planet_sound(void *context, float selector,
    struct yt_error *error)
{
	struct projectile_planet_tape *tape = context;

	(void)error;
	tape->selector = selector;
	return projectile_planet_step(tape, PROJECTILE_PLANET_SOUND);
}

static void
projectile_planet_fixture(struct projectile_planet_tape *tape,
    struct yt_projectile_planet_impact_state *state,
    struct yt_planet *planet, float *remaining)
{
	static const float draws[] = {0.2f, 0.001f, 0.002f, 0.003f};
	size_t record_index;
	size_t byte_index;

	memset(tape, 0, sizeof(*tape));
	memset(planet, 0, sizeof(*planet));
	planet->ground_forces = 5.0f;
	planet->owner = 7.0f;
	*remaining = 2.0f;
	tape->fail_at = SIZE_MAX;
	tape->draws = draws;
	tape->draw_count = YT_ARRAY_LEN(draws);
	tape->physical_planet = 0x01020304U;
	tape->physical_sector = 0x05060708U;
	for (record_index = 0U;
	    record_index < YT_ARRAY_LEN(tape->planet_source);
	    ++record_index) {
		for (byte_index = 0U; byte_index < YT_RECORD_SIZE; ++byte_index)
			tape->planet_source[record_index].record.bytes[byte_index] =
			    (uint8_t)(byte_index ^ (0x31U + record_index * 0x22U));
	}
	for (byte_index = 0U; byte_index < YT_RECORD_SIZE; ++byte_index)
		tape->sector_source.record.bytes[byte_index] =
		    (uint8_t)(byte_index ^ 0xa7U);
	state->planet = planet;
	state->updater_ore = 1.0f;
	state->remaining = remaining;
	state->physical_planet = tape->physical_planet;
	state->physical_sector = tape->physical_sector;
}

static bool
check_projectile_planet_impact_transaction(void)
{
	static const struct yt_projectile_planet_impact_ops ops = {
		projectile_planet_random,
		projectile_planet_read,
		projectile_planet_write,
		projectile_planet_sector_read,
		projectile_planet_sector_write,
		projectile_planet_present,
		projectile_planet_news,
		projectile_planet_sound,
	};
	static const int expected_events[] = {
		PROJECTILE_PLANET_RANDOM,
		PROJECTILE_PLANET_READ,
		PROJECTILE_PLANET_WRITE,
		PROJECTILE_PLANET_PRESENT,
		PROJECTILE_PLANET_NEWS,
		PROJECTILE_PLANET_RANDOM,
		PROJECTILE_PLANET_RANDOM,
		PROJECTILE_PLANET_RANDOM,
		PROJECTILE_PLANET_PRESENT,
		PROJECTILE_PLANET_NEWS,
		PROJECTILE_PLANET_READ,
		PROJECTILE_PLANET_WRITE,
		PROJECTILE_PLANET_READ,
		PROJECTILE_PLANET_WRITE,
		PROJECTILE_PLANET_SECTOR_READ,
		PROJECTILE_PLANET_SECTOR_WRITE,
		PROJECTILE_PLANET_PRESENT,
		PROJECTILE_PLANET_SOUND,
		PROJECTILE_PLANET_NEWS,
	};
	static const uint8_t expected_ground[] =
	    "Ground forces reduced to 0!";
	static const uint8_t expected_productivity[] =
	    "Productivity reduced by 0 units to 0 units!";
	static const uint8_t expected_destroyed[] =
	    "The planet was destroyed!!";
	struct projectile_planet_tape tape;
	struct yt_projectile_planet_impact_state state;
	struct yt_planet planet;
	struct yt_planet expected_planet;
	struct yt_sector expected_sector;
	float remaining;
	size_t failure;

	projectile_planet_fixture(&tape, &state, &planet, &remaining);
	if (!yt_projectile_planet_impact_run(&state, &ops, &tape, NULL)
	    || tape.event_count != YT_ARRAY_LEN(expected_events)
	    || memcmp(tape.events, expected_events, sizeof(expected_events)) != 0
	    || tape.draw_position != 4U || remaining != 0.0f
	    || !state.early_return
	    || planet.ground_forces != 0.0f || planet.owner != 0.0f
	    || planet.production[0] != 0.0f
	    || planet.production[1] != 0.0f
	    || planet.production[2] != 0.0f || tape.planet_reads != 3U
	    || tape.planet_writes != 3U || tape.present_count != 3U
	    || tape.news_count != 3U || tape.selector != 3.0f
	    || tape.present_length[0] != sizeof(expected_ground) - 1U
	    || memcmp(tape.present[0], expected_ground,
	    sizeof(expected_ground) - 1U) != 0
	    || tape.present_length[1] != sizeof(expected_productivity) - 1U
	    || memcmp(tape.present[1], expected_productivity,
	    sizeof(expected_productivity) - 1U) != 0
	    || tape.present_length[2] != sizeof(expected_destroyed) - 1U
	    || memcmp(tape.present[2], expected_destroyed,
	    sizeof(expected_destroyed) - 1U) != 0
	    || tape.news_length[0] != tape.present_length[0]
	    || memcmp(tape.news[0], tape.present[0], tape.present_length[0]) != 0
	    || tape.news_length[1] != tape.present_length[1]
	    || memcmp(tape.news[1], tape.present[1], tape.present_length[1]) != 0
	    || tape.news_length[2] != tape.present_length[2]
	    || memcmp(tape.news[2], tape.present[2], tape.present_length[2]) != 0)
		return false;

	expected_planet = tape.planet_source[0];
	if (!yt_projectile_planet_ground_overlay(&expected_planet, 0.0f, 0.0f)
	    || memcmp(&tape.planet_write[0].record, &expected_planet.record,
	    sizeof(expected_planet.record)) != 0)
		return false;
	expected_planet = tape.planet_source[1];
	if (!yt_projectile_planet_productivity_overlay(&expected_planet,
	    planet.production, planet.stock)
	    || memcmp(&tape.planet_write[1].record, &expected_planet.record,
	    sizeof(expected_planet.record)) != 0)
		return false;
	expected_planet = tape.planet_source[2];
	if (!yt_projectile_planet_destroy_overlay(&expected_planet)
	    || memcmp(&tape.planet_write[2].record, &expected_planet.record,
	    sizeof(expected_planet.record)) != 0)
		return false;
	expected_sector = tape.sector_source;
	if (!yt_projectile_sector_unlink_overlay(&expected_sector)
	    || memcmp(&tape.sector_write.record, &expected_sector.record,
	    sizeof(expected_sector.record)) != 0)
		return false;

	for (failure = 1U; failure <= YT_ARRAY_LEN(expected_events); ++failure) {
		projectile_planet_fixture(&tape, &state, &planet, &remaining);
		tape.fail_at = failure;
		if (yt_projectile_planet_impact_run(&state, &ops, &tape, NULL)
		    || tape.event_count != failure
		    || memcmp(tape.events, expected_events,
		    failure * sizeof(expected_events[0])) != 0)
			return false;
	}

	projectile_planet_fixture(&tape, &state, &planet, &remaining);
	remaining = 1.0f;
	if (!yt_projectile_planet_impact_run(&state, &ops, &tape, NULL)
	    || tape.event_count != 5U
	    || memcmp(tape.events, expected_events,
	    tape.event_count * sizeof(expected_events[0])) != 0
	    || remaining != 0.0f || !state.early_return
	    || tape.present_count != 1U
	    || tape.news_count != 1U || tape.planet_writes != 1U)
		return false;
	return true;
}

static bool
check_projectile_parent_model(void)
{
	static const uint8_t missile[] =
	    "You have 5. Send your cruise missile to what sector? "
	    "[ 1 to 2004 ] ?";
	static const uint8_t plasma[] =
	    "You have 5. Send your plasma bolt to what sector? "
	    "[ 1 to 2004 ] ?";
	uint8_t prompt[192];
	size_t length;
	float target = -1.0f;
	struct yt_player debit;
	struct yt_player missile_debit;
	struct yt_player plasma_debit;
	struct yt_record expected_debit;
	static const uint8_t target_binary[] = {'B', 0, 'B'};
	static const uint8_t saved_binary[] = {'A', 0, 'A'};
	static const uint8_t terminal_expected[] =
	    "B\0B shot back with 3 missiles at you!";
	static const uint8_t news_expected[] =
	    "B\0B shot back with 3 missiles at A\0A!";
	static const uint8_t owner_binary[] = {'A', 0, 'B'};
	static const uint8_t defense_expected[] =
	    "Sector: 7 defended by A\0B with 12 fighters.";
	static const uint8_t attack_news_expected[] =
	    "A\0A's missiles attacked B\0B in 7 reducing";
	static const uint8_t attack_direct_expected[] =
	    "The missiles attacked B\0B in 7 reducing";
	static const uint8_t plasma_news_expected[] =
	    "A\0A's plasma bolts hit B\0B in 7 reducing";
	static const uint8_t plasma_direct_expected[] =
	    "The plasma bolts hit B\0B in 7 reducing";
	static const uint8_t destroyed_expected[] = "B\0B was destroyed!";
	static const uint8_t warning_expected[] =
	    "*** WARNING, B\0B had sector mines!";
	static const uint8_t planet_binary[] = {'P', 0, 'P'};
	static const uint8_t friendly_planet_expected[] =
	    "NOT attacking friendly planet \"P\0P\"!";
	static const uint8_t missile_planet_direct_expected[] =
	    "The Missiles attacked planet P\0P in sector 7!";
	static const uint8_t missile_planet_news_expected[] =
	    "A\0A's Missiles attacked planet P\0P in sector 7!";
	static const uint8_t plasma_planet_direct_expected[] =
	    "The plasma bolts hit planet P\0P in sector 7!";
	static const uint8_t plasma_planet_news_expected[] =
	    "A\0A's plasma bolts hit planet P\0P in sector 7!";
	static const uint8_t footer_expected[] = "*** End of Report ***";
	static const uint8_t route_failure_expected[] =
	    "*** You can't get there without going someplace you dont want to!";
	static const uint8_t self_destruct_expected[] =
	    "Missles self destructed!";
	static const uint8_t victory_winner_expected[] =
	    "Congratulations go to A\0A who defeated the Xannor HQ!!!";
	struct yt_planet planet;
	uint8_t terminal[128];
	uint8_t news[128];
	uint8_t defense[128];
	uint8_t direct[128];
	size_t terminal_length;
	size_t news_length;
	size_t defense_length;

	if (!yt_projectile_target_prompt(false, 5.0f, 2004.0f,
	    prompt, sizeof(prompt), &length)
	    || length != sizeof(missile) - 1U
	    || memcmp(prompt, missile, length) != 0
	    || !yt_projectile_target_prompt(true, 5.0f, 2004.0f,
	    prompt, sizeof(prompt), &length)
	    || length != sizeof(plasma) - 1U
	    || memcmp(prompt, plasma, length) != 0
	    || yt_projectile_target_prompt(false, 5.0f, 2004.0f,
	    prompt, 8U, &length)
	    || yt_projectile_target_response("", 2004.0f, &target)
	    != YT_PROJECTILE_TARGET_CANCEL
	    || target != -1.0f
	    || yt_projectile_target_response("0", 2004.0f, &target)
	    != YT_PROJECTILE_TARGET_RETRY
	    || yt_projectile_target_response("2004.5", 2004.0f, &target)
	    != YT_PROJECTILE_TARGET_RETRY
	    || yt_projectile_target_response("1.5", 2004.0f, &target)
	    != YT_PROJECTILE_TARGET_ACCEPT
	    || target != 1.5f)
		return false;
	memset(&debit, 0, sizeof(debit));
	memset(debit.record.bytes, 0xa5, sizeof(debit.record.bytes));
	debit.missiles = 99.0f;
	yt_counterlaunch_debit_overlay(&debit, 10.0f, 3.0f);
	memset(&missile_debit, 0, sizeof(missile_debit));
	memset(missile_debit.record.bytes, 0xa5,
	    sizeof(missile_debit.record.bytes));
	missile_debit.missiles = 10.0f;
	missile_debit.plasma = 19.0f;
	(void)yt_record_set_number(&missile_debit.record, YT_F97, 10.0f);
	(void)yt_record_set_number(&missile_debit.record, YT_F113, 19.0f);
	expected_debit = missile_debit.record;
	(void)yt_record_set_number(&expected_debit, YT_F97, 7.0f);
	yt_projectile_debit_overlay(&missile_debit, false, 3.0f);
	memset(&plasma_debit, 0, sizeof(plasma_debit));
	memset(plasma_debit.record.bytes, 0x5a,
	    sizeof(plasma_debit.record.bytes));
	plasma_debit.missiles = 23.0f;
	plasma_debit.plasma = 8.0f;
	(void)yt_record_set_number(&plasma_debit.record, YT_F97, 23.0f);
	(void)yt_record_set_number(&plasma_debit.record, YT_F113, 8.0f);
	yt_projectile_debit_overlay(&plasma_debit, true, 2.0f);
	if (!yt_counterlaunch_rows(target_binary, sizeof(target_binary), 3.0f,
	    saved_binary, sizeof(saved_binary), terminal, sizeof(terminal),
	    &terminal_length, news, sizeof(news), &news_length)
	    || terminal_length != sizeof(terminal_expected) - 1U
	    || memcmp(terminal, terminal_expected, terminal_length) != 0
	    || news_length != sizeof(news_expected) - 1U
	    || memcmp(news, news_expected, news_length) != 0
	    || yt_counterlaunch_rows(target_binary, sizeof(target_binary), 3.0f,
	    saved_binary, sizeof(saved_binary), terminal, 4U,
	    &terminal_length, news, sizeof(news), &news_length)
	    || !yt_projectile_defense_row(7.0f, owner_binary,
	    sizeof(owner_binary), 12.0, defense, sizeof(defense),
	    &defense_length)
	    || defense_length != sizeof(defense_expected) - 1U
	    || memcmp(defense, defense_expected, defense_length) != 0
	    || yt_projectile_defense_row(7.0f, owner_binary,
	    sizeof(owner_binary), 12.0, defense, 8U, &defense_length)
	    || !yt_projectile_attack_first_rows(false,
	    saved_binary, sizeof(saved_binary), target_binary,
	    sizeof(target_binary), 7.0f, news, sizeof(news), &news_length,
	    direct, sizeof(direct), &terminal_length)
	    || news_length != sizeof(attack_news_expected) - 1U
	    || memcmp(news, attack_news_expected, news_length) != 0
	    || terminal_length != sizeof(attack_direct_expected) - 1U
	    || memcmp(direct, attack_direct_expected, terminal_length) != 0
	    || !yt_projectile_attack_first_rows(true,
	    saved_binary, sizeof(saved_binary), target_binary,
	    sizeof(target_binary), 7.0f, news, sizeof(news), &news_length,
	    direct, sizeof(direct), &terminal_length)
	    || news_length != sizeof(plasma_news_expected) - 1U
	    || memcmp(news, plasma_news_expected, news_length) != 0
	    || terminal_length != sizeof(plasma_direct_expected) - 1U
	    || memcmp(direct, plasma_direct_expected, terminal_length) != 0
	    || !yt_projectile_destroyed_rows(target_binary,
	    sizeof(target_binary), direct, sizeof(direct), &terminal_length,
	    news, sizeof(news), &news_length)
	    || terminal_length != sizeof(destroyed_expected) - 1U
	    || memcmp(direct, destroyed_expected, terminal_length) != 0
	    || news_length != sizeof(warning_expected) - 1U
	    || memcmp(news, warning_expected, news_length) != 0
	    || !yt_projectile_friendly_planet_row(planet_binary,
	    sizeof(planet_binary), direct, sizeof(direct), &terminal_length)
	    || terminal_length != sizeof(friendly_planet_expected) - 1U
	    || memcmp(direct, friendly_planet_expected, terminal_length) != 0
	    || !yt_projectile_planet_attack_rows(false,
	    saved_binary, sizeof(saved_binary), planet_binary,
	    sizeof(planet_binary), 7.0f, direct, sizeof(direct),
	    &terminal_length, news, sizeof(news), &news_length)
	    || terminal_length != sizeof(missile_planet_direct_expected) - 1U
	    || memcmp(direct, missile_planet_direct_expected,
	    terminal_length) != 0
	    || news_length != sizeof(missile_planet_news_expected) - 1U
	    || memcmp(news, missile_planet_news_expected, news_length) != 0
	    || !yt_projectile_planet_attack_rows(true,
	    saved_binary, sizeof(saved_binary), planet_binary,
	    sizeof(planet_binary), 7.0f, direct, sizeof(direct),
	    &terminal_length, news, sizeof(news), &news_length)
	    || terminal_length != sizeof(plasma_planet_direct_expected) - 1U
	    || memcmp(direct, plasma_planet_direct_expected,
	    terminal_length) != 0
	    || news_length != sizeof(plasma_planet_news_expected) - 1U
	    || memcmp(news, plasma_planet_news_expected, news_length) != 0
	    || !yt_projectile_route_failure_row(false, direct, sizeof(direct),
	    &terminal_length)
	    || terminal_length != sizeof(route_failure_expected) - 1U
	    || memcmp(direct, route_failure_expected, terminal_length) != 0
	    || !yt_projectile_route_failure_row(true, direct, sizeof(direct),
	    &terminal_length)
	    || terminal_length != sizeof(self_destruct_expected) - 1U
	    || memcmp(direct, self_destruct_expected, terminal_length) != 0
	    || yt_projectile_route_failure_row(false, direct, 8U,
	    &terminal_length)
	    || !yt_projectile_footer_row(direct, sizeof(direct),
	    &terminal_length)
	    || terminal_length != sizeof(footer_expected) - 1U
	    || memcmp(direct, footer_expected, terminal_length) != 0
	    || yt_projectile_footer_row(direct, 8U, &terminal_length)
	    || !yt_xannor_victory_winner(saved_binary,
	    sizeof(saved_binary), news, sizeof(news), &news_length)
	    || news_length != sizeof(victory_winner_expected) - 1U
	    || memcmp(news, victory_winner_expected, news_length) != 0
	    || yt_xannor_victory_winner(saved_binary,
	    sizeof(saved_binary), news, 8U, &news_length))
		return false;
	memset(&planet, 0, sizeof(planet));
	memcpy(planet.record.bytes, planet_binary, sizeof(planet_binary));
	planet.name_length = 3.0f;
	if (!yt_planet_stored_name(&planet, defense, &defense_length, NULL)
	    || defense_length != sizeof(planet_binary)
	    || memcmp(defense, planet_binary, defense_length) != 0)
		return false;
	return debit.missiles == 7.0f
	    && yt_record_get_number(&debit.record, YT_F97) == 7.0f
	    && debit.record.bytes[YT_F93] == 0xa5U
	    && missile_debit.missiles == 7.0f
	    && missile_debit.plasma == 19.0f
	    && memcmp(&missile_debit.record, &expected_debit,
	    sizeof(expected_debit)) == 0
	    && plasma_debit.missiles == 23.0f
	    && plasma_debit.plasma == 6.0f
	    && yt_record_get_number(&plasma_debit.record, YT_F97) == 23.0f
	    && yt_record_get_number(&plasma_debit.record, YT_F113) == 6.0f
	    && plasma_debit.record.bytes[YT_F93] == 0x5aU
	    && yt_projectile_quantity_response("2.9") == 2.0f
	    && yt_projectile_quantity_response("-.1") == -1.0f
	    && yt_projectile_quantity_response("E") == 0.0f
	    && yt_projectile_quantity_response(NULL) == 0.0f
	    && yt_projectile_candidate_route(3, 2, 8.0f, 7.0f, 0.0f)
	    == YT_PROJECTILE_CANDIDATE_SKIP
	    && yt_projectile_candidate_route(3, 2, 7.0f, 7.0f, 0.0f)
	    == YT_PROJECTILE_CANDIDATE_TERMINATE
	    && yt_projectile_candidate_route(2, 2, 7.0f, 7.0f, 0.5f)
	    == YT_PROJECTILE_CANDIDATE_SKIP
	    && yt_projectile_candidate_route(3, 2, 7.0f, 7.0f, 0.5f)
	    == YT_PROJECTILE_CANDIDATE_FRIENDSHIP
	    && yt_projectile_candidate_admitted(3, 0.0f, 0)
	    && !yt_projectile_candidate_admitted(3, 1.0f, 0)
	    && yt_projectile_candidate_admitted(3, 1.0f, 3)
	    && !yt_projectile_candidate_admitted(3, 0.0f, 4)
	    && yt_projectile_post_impact_route(1.0f)
	    == YT_PROJECTILE_POST_IMPACT_NEXT_HOP
	    && yt_projectile_post_impact_route(0.0f)
	    == YT_PROJECTILE_POST_IMPACT_FOOTER
	    && yt_projectile_post_impact_route(-1.0f)
	    == YT_PROJECTILE_POST_IMPACT_FOOTER
	    && yt_projectile_post_impact_route(NAN)
	    == YT_PROJECTILE_POST_IMPACT_FOOTER
	    && !yt_projectile_route_has_next(0)
	    && yt_projectile_route_has_next(1)
	    && yt_projectile_route_has_next(-1)
	    && yt_projectile_route_avoid_enabled(false, 0, 2)
	    && yt_projectile_route_avoid_enabled(false, 0, 0)
	    && !yt_projectile_route_avoid_enabled(true, 0, 2)
	    && !yt_projectile_route_avoid_enabled(false, 3, 2)
	    && !yt_projectile_route_avoid_enabled(false, 0, -1)
	    && !yt_projectile_player_survives(0.999f)
	    && yt_projectile_player_survives(1.0f)
	    && yt_projectile_salvage_admitted(0, 0)
	    && !yt_projectile_salvage_admitted(2, 0)
	    && !yt_projectile_salvage_admitted(0, 3)
	    && yt_projectile_death_continuation(0.5f, 7.0f)
	    == YT_PROJECTILE_DEATH_REENTER_MINES
	    && yt_projectile_death_continuation(0.5f, 0.0f)
	    == YT_PROJECTILE_DEATH_RETURN
	    && yt_projectile_death_continuation(0.0f, 7.0f)
	    == YT_PROJECTILE_DEATH_RETURN
	    && yt_projectile_death_continuation(1.0f, 0.0f)
	    == YT_PROJECTILE_DEATH_NEXT_PLAYER
	    && !yt_projectile_survivor_sets_counterattack(-1)
	    && yt_projectile_survivor_sets_counterattack(2)
	    && !yt_projectile_damage_iteration(1.0f, 0.5f)
	    && yt_projectile_damage_iteration(1.0f, 2.5f)
	    && yt_projectile_damage_iteration(2.0f, 2.5f)
	    && !yt_projectile_damage_iteration(3.0f, 2.5f)
	    && yt_counterlaunch_score_count(-1.0, -2.5f) == -2.5f
	    && yt_counterlaunch_score_count(0.0, 4.0f) == 4.0f
	    && yt_counterlaunch_score_count(100000.0, 9.0f) == 1.0f
	    && yt_counterlaunch_score_count(250000.0, 9.0f) == 3.0f
	    && yt_counterlaunch_score_count(2000000.0, 9.0f) == 21.0f;
}

enum xannor_victory_event {
	XANNOR_VICTORY_FOREGROUND = 1,
	XANNOR_VICTORY_FILE,
	XANNOR_VICTORY_PAUSE,
	XANNOR_VICTORY_WAIT,
	XANNOR_VICTORY_BLANK,
	XANNOR_VICTORY_BLINK,
	XANNOR_VICTORY_BONUS,
	XANNOR_VICTORY_CLEAR_QUEUE,
	XANNOR_VICTORY_READ_PLAYER,
	XANNOR_VICTORY_WRITE_PLAYER,
	XANNOR_VICTORY_SOUND,
	XANNOR_VICTORY_NEWS,
	XANNOR_VICTORY_RADIO,
	XANNOR_VICTORY_READ_SECTOR,
	XANNOR_VICTORY_WRITE_SECTOR,
};

struct xannor_victory_tape {
	enum xannor_victory_event events[24];
	size_t event_count;
	size_t calls;
	size_t fail_at;
	char file[32];
	float foreground;
	float blink;
	unsigned clear_count;
	double wait_seconds;
	struct yt_player player_source;
	struct yt_player player_written;
	bool player_write_completed;
	float sounds[3];
	size_t sound_count;
	uint8_t news[3][128];
	size_t news_length[3];
	size_t news_count;
	uint8_t radio[3][128];
	size_t radio_length[3];
	float radio_sender[3];
	float radio_recipient[3];
	size_t radio_count;
	struct yt_sector sector_source;
	struct yt_sector sector_written;
	int sector_read_record;
	int sector_write_record;
	bool sector_write_completed;
	uint8_t presented[3][64];
	size_t presented_length[3];
	enum yt_xannor_victory_output_kind presented_kind[3];
	size_t presented_count;
};

static void
xannor_victory_tape_error(struct yt_error *error, const char *operation)
{
	if (error == NULL)
		return;
	error->status = YT_IO_ERROR;
	error->system_error = 0;
	(void)snprintf(error->operation, sizeof(error->operation), "%s",
	    operation);
	error->path[0] = '\0';
}

static bool
xannor_victory_tape_step(struct xannor_victory_tape *tape,
    enum xannor_victory_event event, bool fallible, const char *operation,
    struct yt_error *error)
{
	if (tape->event_count >= YT_ARRAY_LEN(tape->events))
		return false;
	tape->events[tape->event_count++] = event;
	if (fallible && ++tape->calls == tape->fail_at) {
		xannor_victory_tape_error(error, operation);
		return false;
	}
	return true;
}

static bool
xannor_victory_tape_file(void *context, const char *path,
    struct yt_error *error)
{
	struct xannor_victory_tape *tape = context;

	if (!xannor_victory_tape_step(tape, XANNOR_VICTORY_FILE, true,
	    "victory file", error))
		return false;
	(void)snprintf(tape->file, sizeof(tape->file), "%s", path);
	return true;
}

static bool
xannor_victory_tape_present(void *context, const uint8_t *text,
    size_t length, enum yt_xannor_victory_output_kind kind,
    const char *operation, struct yt_error *error)
{
	struct xannor_victory_tape *tape = context;
	enum xannor_victory_event event = kind == YT_XANNOR_VICTORY_RAW
	    ? XANNOR_VICTORY_PAUSE
	    : kind == YT_XANNOR_VICTORY_BOLD_LINE
	    ? XANNOR_VICTORY_BONUS : XANNOR_VICTORY_BLANK;

	if (!xannor_victory_tape_step(tape, event, true, operation, error))
		return false;
	if (tape->presented_count >= YT_ARRAY_LEN(tape->presented)
	    || length > sizeof(tape->presented[0]))
		return false;
	if (length != 0U)
		memcpy(tape->presented[tape->presented_count], text, length);
	tape->presented_length[tape->presented_count] = length;
	tape->presented_kind[tape->presented_count] = kind;
	++tape->presented_count;
	return true;
}

static bool
xannor_victory_tape_wait(void *context, double seconds,
    const char *operation, struct yt_error *error)
{
	struct xannor_victory_tape *tape = context;

	if (!xannor_victory_tape_step(tape, XANNOR_VICTORY_WAIT, true,
	    operation, error))
		return false;
	tape->wait_seconds = seconds;
	return true;
}

static void
xannor_victory_tape_foreground(void *context, float foreground)
{
	struct xannor_victory_tape *tape = context;

	(void)xannor_victory_tape_step(tape, XANNOR_VICTORY_FOREGROUND, false,
	    "victory foreground", NULL);
	tape->foreground = foreground;
}

static void
xannor_victory_tape_blink(void *context, float blink)
{
	struct xannor_victory_tape *tape = context;

	(void)xannor_victory_tape_step(tape, XANNOR_VICTORY_BLINK, false,
	    "victory blink", NULL);
	tape->blink = blink;
}

static void
xannor_victory_tape_clear(void *context)
{
	struct xannor_victory_tape *tape = context;

	(void)xannor_victory_tape_step(tape, XANNOR_VICTORY_CLEAR_QUEUE,
	    false, "victory clear", NULL);
	++tape->clear_count;
}

static bool
xannor_victory_tape_read_player(struct xannor_victory_tape *tape,
    struct yt_player *player, struct yt_error *error)
{
	if (!xannor_victory_tape_step(tape, XANNOR_VICTORY_READ_PLAYER, true,
	    "victory player GET", error))
		return false;
	*player = tape->player_source;
	return true;
}

static bool
xannor_victory_tape_write_player(struct xannor_victory_tape *tape,
    struct yt_player *player, struct yt_error *error)
{
	if (!xannor_victory_tape_step(tape, XANNOR_VICTORY_WRITE_PLAYER,
	    true, "victory player PUT", error))
		return false;
	tape->player_written = *player;
	tape->player_write_completed = true;
	return true;
}

static bool
xannor_victory_tape_mutate_credits(void *context, float player_record,
    float argument, struct yt_player *player, bool *hydrated,
    struct yt_error *error)
{
	struct xannor_victory_tape *tape = context;

	if (hydrated != NULL)
		*hydrated = false;
	if (player_record != 2.5f || argument != 16000000.0f
	    || !xannor_victory_tape_read_player(tape, player, error))
		return false;
	if (hydrated != NULL)
		*hydrated = true;
	yt_planet_bank_credit_overlay(player, argument);
	if (!yt_record_set_number(&player->record, YT_F81, player->credits))
		return false;
	return xannor_victory_tape_write_player(tape, player, error);
}

static bool
xannor_victory_tape_sound(void *context, float selector,
    const char *operation, struct yt_error *error)
{
	struct xannor_victory_tape *tape = context;

	if (!xannor_victory_tape_step(tape, XANNOR_VICTORY_SOUND, true,
	    operation, error))
		return false;
	if (tape->sound_count >= YT_ARRAY_LEN(tape->sounds))
		return false;
	tape->sounds[tape->sound_count++] = selector;
	return true;
}

static bool
xannor_victory_tape_news(void *context, const uint8_t *text,
    size_t length, struct yt_error *error)
{
	struct xannor_victory_tape *tape = context;

	if (!xannor_victory_tape_step(tape, XANNOR_VICTORY_NEWS, true,
	    "victory news", error))
		return false;
	if (tape->news_count >= YT_ARRAY_LEN(tape->news)
	    || length > sizeof(tape->news[0]))
		return false;
	memcpy(tape->news[tape->news_count], text, length);
	tape->news_length[tape->news_count++] = length;
	return true;
}

static bool
xannor_victory_tape_radio(void *context, const uint8_t *text,
    size_t length, float sender, float recipient, struct yt_error *error)
{
	struct xannor_victory_tape *tape = context;

	if (!xannor_victory_tape_step(tape, XANNOR_VICTORY_RADIO, true,
	    "victory radio", error))
		return false;
	if (tape->radio_count >= YT_ARRAY_LEN(tape->radio)
	    || length > sizeof(tape->radio[0]))
		return false;
	memcpy(tape->radio[tape->radio_count], text, length);
	tape->radio_length[tape->radio_count] = length;
	tape->radio_sender[tape->radio_count] = sender;
	tape->radio_recipient[tape->radio_count] = recipient;
	++tape->radio_count;
	return true;
}

static bool
xannor_victory_tape_read_sector(void *context, int logical_sector,
    struct yt_sector *sector, struct yt_error *error)
{
	struct xannor_victory_tape *tape = context;

	if (!xannor_victory_tape_step(tape, XANNOR_VICTORY_READ_SECTOR, true,
	    "victory sector GET", error))
		return false;
	tape->sector_read_record = logical_sector;
	*sector = tape->sector_source;
	return true;
}

static bool
xannor_victory_tape_write_sector(void *context, int logical_sector,
    struct yt_sector *sector, struct yt_error *error)
{
	struct xannor_victory_tape *tape = context;

	if (!xannor_victory_tape_step(tape, XANNOR_VICTORY_WRITE_SECTOR, true,
	    "victory sector PUT", error))
		return false;
	tape->sector_write_record = logical_sector;
	tape->sector_written = *sector;
	tape->sector_write_completed = true;
	return true;
}

static void
xannor_victory_fixture(struct xannor_victory_tape *tape)
{
	size_t index;

	memset(tape, 0, sizeof(*tape));
	memset(&tape->player_source, 0xa5, sizeof(tape->player_source));
	for (index = 0U; index < sizeof(tape->player_source.record.bytes);
	    ++index)
		tape->player_source.record.bytes[index] = (uint8_t)(index ^ 0x5aU);
	tape->player_source.record.bytes[0] = 'A';
	tape->player_source.record.bytes[1] = 0U;
	tape->player_source.record.bytes[2] = 'B';
	tape->player_source.name_length = 3.0f;
	tape->player_source.credits = 16000001.0f;
	(void)yt_record_set_number(&tape->player_source.record, YT_F81,
	    tape->player_source.credits);
	memset(&tape->sector_source, 0x5a, sizeof(tape->sector_source));
	for (index = 0U; index < sizeof(tape->sector_source.record.bytes);
	    ++index)
		tape->sector_source.record.bytes[index] = (uint8_t)(index ^ 0xa5U);
	tape->sector_source.metadata = -9.0f;
}

static bool
check_xannor_victory_transaction(void)
{
	static const struct yt_xannor_victory_ops ops = {
		xannor_victory_tape_file,
		xannor_victory_tape_present,
		xannor_victory_tape_wait,
		xannor_victory_tape_foreground,
		xannor_victory_tape_blink,
		xannor_victory_tape_clear,
		xannor_victory_tape_mutate_credits,
		xannor_victory_tape_sound,
		xannor_victory_tape_news,
		xannor_victory_tape_radio,
		xannor_victory_tape_read_sector,
		xannor_victory_tape_write_sector,
	};
	static const enum xannor_victory_event expected_events[] = {
		XANNOR_VICTORY_FOREGROUND,
		XANNOR_VICTORY_FILE,
		XANNOR_VICTORY_PAUSE,
		XANNOR_VICTORY_WAIT,
		XANNOR_VICTORY_BLANK,
		XANNOR_VICTORY_BLINK,
		XANNOR_VICTORY_BONUS,
		XANNOR_VICTORY_CLEAR_QUEUE,
		XANNOR_VICTORY_READ_PLAYER,
		XANNOR_VICTORY_WRITE_PLAYER,
		XANNOR_VICTORY_SOUND,
		XANNOR_VICTORY_SOUND,
		XANNOR_VICTORY_SOUND,
		XANNOR_VICTORY_NEWS,
		XANNOR_VICTORY_NEWS,
		XANNOR_VICTORY_NEWS,
		XANNOR_VICTORY_RADIO,
		XANNOR_VICTORY_RADIO,
		XANNOR_VICTORY_RADIO,
		XANNOR_VICTORY_READ_SECTOR,
		XANNOR_VICTORY_WRITE_SECTOR,
	};
	static const uint8_t expected_winner[] =
	    "Congratulations go to A\0B who defeated the Xannor HQ!!!";
	static const uint8_t pause[] = "[PAUSE]";
	static const uint8_t bonus[] =
	    "Collect 16,000,000 credit bonus!";
	struct xannor_victory_tape success;
	struct xannor_victory_tape tape;
	struct yt_xannor_victory_state state;
	struct yt_player expected_player;
	struct yt_sector expected_sector;
	struct yt_error error;
	size_t failure;
	size_t index;
	size_t event_prefix;
	size_t fallible;

	xannor_victory_fixture(&success);
	memset(&state, 0, sizeof(state));
	state.current_player = 2.5f;
	yt_error_clear(&error);
	if (!yt_xannor_victory_run(&state, &ops, &success, &error)
	    || success.event_count != YT_ARRAY_LEN(expected_events)
	    || memcmp(success.events, expected_events,
	    sizeof(expected_events)) != 0 || success.calls != 18U
	    || strcmp(success.file, "XannorHQ.TXT") != 0
	    || success.foreground != 7.0f || success.blink != 1.0f
	    || success.clear_count != 1U || success.wait_seconds != 99.0
	    || success.presented_count != 3U
	    || success.presented_kind[0] != YT_XANNOR_VICTORY_RAW
	    || success.presented_length[0] != sizeof(pause) - 1U
	    || memcmp(success.presented[0], pause, sizeof(pause) - 1U) != 0
	    || success.presented_kind[1] != YT_XANNOR_VICTORY_LINE
	    || success.presented_length[1] != 0U
	    || success.presented_kind[2] != YT_XANNOR_VICTORY_BOLD_LINE
	    || success.presented_length[2] != sizeof(bonus) - 1U
	    || memcmp(success.presented[2], bonus, sizeof(bonus) - 1U) != 0
	    || state.foreground != 7.0f || state.pager_foreground != 7.0f
	    || state.blink != 1.0f || state.awarded_credits != 32000000.0f
	    || !success.player_write_completed || success.sound_count != 3U
	    || state.sounds_completed != 3U || success.news_count != 3U
	    || state.news_completed != 3U || success.radio_count != 3U
	    || state.radio_completed != 3U
	    || state.winner_length != sizeof(expected_winner) - 1U
	    || memcmp(state.winner, expected_winner,
	    sizeof(expected_winner) - 1U) != 0
	    || success.sector_read_record != 21
	    || success.sector_write_record != 21
	    || !success.sector_write_completed)
		return false;
	expected_player = success.player_source;
	expected_player.credits = 32000000.0f;
	(void)yt_record_set_number(&expected_player.record, YT_F81,
	    expected_player.credits);
	if (memcmp(&success.player_written, &expected_player,
	    sizeof(expected_player)) != 0)
		return false;
	expected_sector = success.sector_source;
	expected_sector.metadata = 2.5f;
	if (memcmp(&success.sector_written, &expected_sector,
	    sizeof(expected_sector)) != 0)
		return false;
	for (index = 0U; index < 3U; ++index) {
		const uint8_t *expected = index == 1U ? expected_winner
		    : (const uint8_t *)
		    "*******************************************************************************";
		size_t expected_length = index == 1U
		    ? sizeof(expected_winner) - 1U : 79U;

		if (success.sounds[index] != 2.0f
		    || success.news_length[index] != expected_length
		    || memcmp(success.news[index], expected, expected_length) != 0
		    || success.radio_length[index] != expected_length
		    || memcmp(success.radio[index], expected, expected_length) != 0
		    || success.radio_sender[index] != -2.0f
		    || success.radio_recipient[index] != -2.0f)
			return false;
	}

	/* Every fallible dependency retains the exact successful prefix. */
	for (failure = 1U; failure <= success.calls; ++failure) {
		xannor_victory_fixture(&tape);
		tape.fail_at = failure;
		memset(&state, 0, sizeof(state));
		state.current_player = 2.5f;
		yt_error_clear(&error);
		if (yt_xannor_victory_run(&state, &ops, &tape, &error)
		    || error.status != YT_IO_ERROR || tape.calls != failure)
			return false;
		fallible = 0U;
		event_prefix = 0U;
		for (index = 0U; index < YT_ARRAY_LEN(expected_events); ++index) {
			if (expected_events[index] != XANNOR_VICTORY_FOREGROUND
			    && expected_events[index] != XANNOR_VICTORY_BLINK
			    && expected_events[index]
			    != XANNOR_VICTORY_CLEAR_QUEUE)
				++fallible;
			if (fallible == failure) {
				event_prefix = index + 1U;
				break;
			}
		}
		if (tape.event_count != event_prefix
		    || memcmp(tape.events, expected_events,
		    event_prefix * sizeof(expected_events[0])) != 0)
			return false;
	}

	/* Name conversion faults occur after the durable award and sounds. */
	xannor_victory_fixture(&tape);
	tape.player_source.name_length = -1.0f;
	memset(&state, 0, sizeof(state));
	state.current_player = 2.5f;
	yt_error_clear(&error);
	if (yt_xannor_victory_run(&state, &ops, &tape, &error)
	    || error.status != YT_RANGE
	    || strcmp(error.operation, "player name LEFT$ length") != 0
	    || tape.event_count != 13U || tape.calls != 10U
	    || !tape.player_write_completed || tape.sound_count != 3U
	    || tape.news_count != 0U || tape.radio_count != 0U
	    || tape.sector_read_record != 0 || tape.sector_write_record != 0)
		return false;
	return true;
}

struct projectile_bridge_capture {
	struct yt_game *game;
	int player_record;
	struct yt_record expected;
	bool *destroyed;
	bool succeeds;
	bool valid;
	unsigned calls;
	float origin;
	float target;
	float amount;
	bool plasma;
};

static bool
capture_projectile_bridge(void *context, float *origin, float target,
    float amount, bool plasma, int *counterattack, int *xannor_provoker,
    struct yt_error *error)
{
	struct projectile_bridge_capture *capture = context;
	struct yt_record durable;

	++capture->calls;
	capture->origin = *origin;
	capture->target = target;
	capture->amount = amount;
	capture->plasma = plasma;
	capture->valid = !*capture->destroyed
	    && yt_database_read(&capture->game->database,
	    (size_t)capture->player_record, &durable, error)
	    && memcmp(durable.bytes, capture->expected.bytes,
	    sizeof(durable.bytes)) == 0;
	*origin = 11.0f;
	*counterattack = 4;
	*xannor_provoker = 5;
	return capture->succeeds;
}

static bool
check_projectile_bridge(void)
{
	static const uint8_t dirty_zero[4] = {0, 0, 0x48, 0};
	static const uint8_t tail[YT_RECORD_TAIL_SIZE] = {0xde, 0xad, 0xbe, 0xef};
	struct yt_game game;
	struct yt_player initial;
	struct yt_player player;
	struct projectile_bridge_capture capture;
	struct yt_record durable;
	struct yt_record post_plasma;
	struct yt_error error;
	bool destroyed;
	float origin;
	int counterattack;
	int xannor_provoker;
	bool valid = false;

	memset(&game, 0, sizeof(game));
	memset(&initial, 0, sizeof(initial));
	yt_record_blank(&initial.record);
	(void)snprintf(initial.name, sizeof(initial.name), "%s", "Bridge");
	initial.name_length = 6.0f;
	initial.sector = 7.0f;
	initial.missiles = 10.0f;
	initial.plasma = 8.0f;
	yt_player_encode(&initial);
	if (!yt_record_set_raw_number(&initial.record, YT_F93, dirty_zero))
		return false;
	memcpy(initial.record.bytes + YT_RECORD_TAIL_OFFSET, tail, sizeof(tail));
	yt_error_clear(&error);
	if (!yt_database_open(&game.database, "PROJECTILE-BRIDGE.DAT",
	    YT_OPEN_CREATE, &error)
	    || !yt_database_write(&game.database, 2U, &initial.record, &error)
	    || !yt_database_flush(&game.database, &error))
		goto done;

	player = initial;
	destroyed = true;
	origin = player.sector;
	counterattack = 0;
	xannor_provoker = 0;
	memset(&capture, 0, sizeof(capture));
	capture.game = &game;
	capture.player_record = 2;
	capture.expected = initial.record;
	(void)yt_record_set_number(&capture.expected, YT_F97, 7.0f);
	capture.destroyed = &destroyed;
	capture.succeeds = true;
	if (!yt_projectile_commit(&game, 2, &player, false, &origin, 9.0f,
	    3.0f, &destroyed, &counterattack, &xannor_provoker,
	    capture_projectile_bridge, &capture, &error)
	    || capture.calls != 1U || !capture.valid || capture.plasma
	    || capture.origin != 7.0f || capture.target != 9.0f
	    || capture.amount != 3.0f || origin != 11.0f
	    || counterattack != 4 || xannor_provoker != 5 || destroyed
	    || player.missiles != 7.0f || player.plasma != 8.0f)
		goto done;

	player = initial;
	if (!yt_database_write(&game.database, 2U, &initial.record, &error)
	    || !yt_database_flush(&game.database, &error))
		goto done;
	destroyed = true;
	origin = player.sector;
	counterattack = 0;
	xannor_provoker = 0;
	memset(&capture, 0, sizeof(capture));
	capture.game = &game;
	capture.player_record = 2;
	capture.expected = initial.record;
	(void)yt_record_set_number(&capture.expected, YT_F113, 6.0f);
	post_plasma = capture.expected;
	capture.destroyed = &destroyed;
	capture.succeeds = false;
	if (yt_projectile_commit(&game, 2, &player, true, &origin, 9.0f,
	    2.0f, &destroyed, &counterattack, &xannor_provoker,
	    capture_projectile_bridge, &capture, &error)
	    || capture.calls != 1U || !capture.valid || !capture.plasma
	    || capture.origin != 7.0f || capture.target != 9.0f
	    || capture.amount != 2.0f || origin != 11.0f
	    || counterattack != 4 || xannor_provoker != 5 || destroyed
	    || player.missiles != 10.0f || player.plasma != 6.0f)
		goto done;

	yt_database_close(&game.database);
	if (!yt_database_open(&game.database, "PROJECTILE-BRIDGE.DAT",
	    YT_OPEN_READ, &error))
		goto done;
	player = initial;
	destroyed = true;
	origin = player.sector;
	counterattack = 0;
	xannor_provoker = 0;
	memset(&capture, 0, sizeof(capture));
	capture.game = &game;
	capture.player_record = 2;
	capture.destroyed = &destroyed;
	capture.succeeds = true;
	yt_error_clear(&error);
	if (yt_projectile_commit(&game, 2, &player, false, &origin, 9.0f,
	    3.0f, &destroyed, &counterattack, &xannor_provoker,
	    capture_projectile_bridge, &capture, &error)
	    || error.status != YT_IO_ERROR || capture.calls != 0U || !destroyed
	    || player.missiles != 7.0f)
		goto done;
	yt_database_close(&game.database);
	if (!yt_database_open(&game.database, "PROJECTILE-BRIDGE.DAT",
	    YT_OPEN_READ, &error)
	    || !yt_database_read(&game.database, 2U, &durable, &error)
	    || memcmp(durable.bytes, post_plasma.bytes,
	    sizeof(durable.bytes)) != 0)
		goto done;
	valid = true;

done:
	yt_database_close(&game.database);
	remove("PROJECTILE-BRIDGE.DAT");
	return valid;
}

enum xannor_tape_event {
	XANNOR_READ_SECTOR = 1,
	XANNOR_NESTED_RANDOM,
	XANNOR_BLANK,
	XANNOR_DESTINATION_RANDOM,
	XANNOR_ROW,
	XANNOR_PROJECTILE,
	XANNOR_READ_PLAYER,
	XANNOR_WAIT,
};

struct xannor_tape {
	int events[8];
	size_t event_count;
	int fail_event;
	int sector_record;
	struct yt_sector sector;
	int amount;
	int destination;
	int random_count[2];
	int random_range[2];
	size_t random_calls;
	uint8_t row[192];
	size_t row_length;
	bool row_bold;
	struct yt_player fresh_player;
	int final_player_record;
	double wait_seconds;
	struct yt_player *live_player;
	int *live_record;
	float *live_cloak;
	int *live_provoker;
	float *live_headquarters;
	float projectile_target;
	float projectile_amount;
	float projectile_sector;
	float projectile_cloak;
	bool projectile_actor;
	bool mutate_child;
};

static bool
xannor_tape_step(struct xannor_tape *tape, int event)
{
	if (tape->event_count >= YT_ARRAY_LEN(tape->events))
		return false;
	tape->events[tape->event_count++] = event;
	return tape->fail_event != event;
}

static bool
xannor_read_sector(void *context, int logical_sector,
    struct yt_sector *sector, struct yt_error *error)
{
	struct xannor_tape *tape = context;

	(void)error;
	tape->sector_record = logical_sector;
	if (!xannor_tape_step(tape, XANNOR_READ_SECTOR))
		return false;
	*sector = tape->sector;
	return true;
}

static bool
xannor_random(void *context, int count, int range, int *value,
    struct yt_error *error)
{
	struct xannor_tape *tape = context;
	size_t call = tape->random_calls++;
	int event = count == 3 ? XANNOR_NESTED_RANDOM
	    : XANNOR_DESTINATION_RANDOM;

	(void)error;
	if (call >= YT_ARRAY_LEN(tape->random_count))
		return false;
	tape->random_count[call] = count;
	tape->random_range[call] = range;
	if (!xannor_tape_step(tape, event))
		return false;
	*value = count == 3 ? tape->amount : tape->destination;
	return true;
}

static bool
xannor_present(void *context, const uint8_t *text, size_t length, bool bold,
    struct yt_error *error)
{
	struct xannor_tape *tape = context;
	int event = bold ? XANNOR_ROW : XANNOR_BLANK;

	(void)error;
	if (!xannor_tape_step(tape, event))
		return false;
	if (length > sizeof(tape->row))
		return false;
	if (length != 0U)
		memcpy(tape->row, text, length);
	tape->row_length = length;
	tape->row_bold = bold;
	return true;
}

static bool
xannor_projectile(void *context, float *origin, float target, float amount,
    bool plasma, int *counterattack, int *xannor_provoker,
    struct yt_error *error)
{
	struct xannor_tape *tape = context;

	(void)error;
	tape->projectile_target = target;
	tape->projectile_amount = amount;
	tape->projectile_sector = tape->live_player->sector;
	tape->projectile_cloak = tape->live_cloak[*tape->live_record == -1
	    ? 2 : *tape->live_record];
	tape->projectile_actor = *tape->live_record == -1
	    && strcmp(tape->live_player->name, "The Xannor") == 0
	    && !plasma && *counterattack == 0
	    && xannor_provoker == tape->live_provoker
	    && origin == tape->live_headquarters;
	if (tape->mutate_child) {
		*origin = 9.0f;
		*xannor_provoker = 11;
	}
	return xannor_tape_step(tape, XANNOR_PROJECTILE);
}

static bool
xannor_read_player(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct xannor_tape *tape = context;

	(void)error;
	tape->final_player_record = player_record;
	if (!xannor_tape_step(tape, XANNOR_READ_PLAYER))
		return false;
	*player = tape->fresh_player;
	return true;
}

static bool
xannor_wait(void *context, double seconds, struct yt_error *error)
{
	struct xannor_tape *tape = context;

	(void)error;
	tape->wait_seconds = seconds;
	return xannor_tape_step(tape, XANNOR_WAIT);
}

static void
xannor_fixture(struct xannor_tape *tape,
    struct yt_xannor_retaliation_state *state, struct yt_player *player,
    int *player_record, float sector_cache[6], float cloak_cache[6],
    bool *destroyed, int *provoker, float *headquarters)
{
	memset(tape, 0, sizeof(*tape));
	memset(player, 0, sizeof(*player));
	memset(sector_cache, 0, 6U * sizeof(*sector_cache));
	memset(cloak_cache, 0, 6U * sizeof(*cloak_cache));
	(void)snprintf(player->name, sizeof(player->name), "%s", "Alice");
	player->name_length = 5.0f;
	player->score = 25000000.0f;
	player->sector = 733.0f;
	yt_player_encode(player);
	*player_record = 2;
	sector_cache[2] = 733.0f;
	cloak_cache[2] = 0.75f;
	*destroyed = false;
	*provoker = 0;
	*headquarters = 8.0f;
	tape->sector.fighters = -3.0f;
	tape->sector.fighter_owner = -1.0f;
	tape->amount = 19;
	tape->destination = 733;
	tape->fresh_player = *player;
	tape->live_player = player;
	tape->live_record = player_record;
	tape->live_cloak = cloak_cache;
	tape->live_provoker = provoker;
	tape->live_headquarters = headquarters;
	state->player = player;
	state->player_record = player_record;
	state->sector_cache = sector_cache;
	state->cloak_cache = cloak_cache;
	state->cache_count = 6U;
	state->destroyed = destroyed;
	state->provoker = provoker;
	state->headquarters = headquarters;
	state->sector_count = 2004;
}

static bool
check_xannor_retaliation_model(void)
{
	static const struct yt_xannor_retaliation_ops ops = {
		xannor_read_sector,
		xannor_random,
		xannor_present,
		xannor_projectile,
		xannor_read_player,
		xannor_wait,
	};
	static const int full_events[8] = {
		XANNOR_READ_SECTOR, XANNOR_NESTED_RANDOM, XANNOR_BLANK,
		XANNOR_DESTINATION_RANDOM, XANNOR_ROW, XANNOR_PROJECTILE,
		XANNOR_READ_PLAYER, XANNOR_WAIT,
	};
	static const uint8_t expected_row[] =
	    "The Xannor have launched 19 missiles at sector 733!";
	struct yt_xannor_retaliation_state state;
	struct xannor_tape tape;
	struct yt_player player;
	float sector_cache[6];
	float cloak_cache[6];
	float headquarters;
	int player_record;
	int provoker;
	bool destroyed;

	xannor_fixture(&tape, &state, &player, &player_record, sector_cache,
	    cloak_cache, &destroyed, &provoker, &headquarters);
	player.score = 24999998.0f;
	if (!yt_xannor_retaliation_run(&state, &ops, &tape, NULL)
	    || tape.event_count != 0U || provoker != 0)
		return false;

	xannor_fixture(&tape, &state, &player, &player_record, sector_cache,
	    cloak_cache, &destroyed, &provoker, &headquarters);
	tape.sector.fighters = 0.0f;
	provoker = 7;
	if (!yt_xannor_retaliation_run(&state, &ops, &tape, NULL)
	    || tape.event_count != 1U || tape.events[0] != XANNOR_READ_SECTOR
	    || tape.sector_record != 8 || provoker != 7)
		return false;
	xannor_fixture(&tape, &state, &player, &player_record, sector_cache,
	    cloak_cache, &destroyed, &provoker, &headquarters);
	tape.sector.fighter_owner = 0.0f;
	if (!yt_xannor_retaliation_run(&state, &ops, &tape, NULL)
	    || tape.event_count != 1U)
		return false;

	xannor_fixture(&tape, &state, &player, &player_record, sector_cache,
	    cloak_cache, &destroyed, &provoker, &headquarters);
	tape.mutate_child = true;
	if (!yt_xannor_retaliation_run(&state, &ops, &tape, NULL)
	    || tape.event_count != YT_ARRAY_LEN(full_events)
	    || memcmp(tape.events, full_events, sizeof(full_events)) != 0
	    || tape.random_calls != 2U || tape.random_count[0] != 3
	    || tape.random_range[0] != 100 || tape.random_count[1] != 1
	    || tape.random_range[1] != 2004 || !tape.projectile_actor
	    || tape.projectile_target != 733.0f
	    || tape.projectile_amount != 19.0f
	    || tape.projectile_sector != 733.0f
	    || tape.projectile_cloak != 0.75f
	    || tape.row_length != sizeof(expected_row) - 1U || !tape.row_bold
	    || memcmp(tape.row, expected_row, sizeof(expected_row) - 1U) != 0
	    || player_record != 2 || strcmp(player.name, "Alice") != 0
	    || cloak_cache[2] != 0.75f || sector_cache[2] != 733.0f
	    || destroyed || provoker != 0 || headquarters != 9.0f
	    || tape.final_player_record != 2 || tape.wait_seconds != 4.0)
		return false;

	xannor_fixture(&tape, &state, &player, &player_record, sector_cache,
	    cloak_cache, &destroyed, &provoker, &headquarters);
	provoker = 7;
	tape.destination = 12;
	tape.fresh_player.killed_by = -1.0f;
	yt_player_encode(&tape.fresh_player);
	if (!yt_xannor_retaliation_run(&state, &ops, &tape, NULL)
	    || tape.projectile_target != 733.0f
	    || tape.projectile_cloak != 0.0f || !destroyed
	    || sector_cache[2] != 0.0f || cloak_cache[2] != 0.75f
	    || provoker != 0)
		return false;

	xannor_fixture(&tape, &state, &player, &player_record, sector_cache,
	    cloak_cache, &destroyed, &provoker, &headquarters);
	provoker = 7;
	tape.fail_event = XANNOR_NESTED_RANDOM;
	if (yt_xannor_retaliation_run(&state, &ops, &tape, NULL)
	    || tape.event_count != 2U || player_record != 2
	    || strcmp(player.name, "Alice") != 0
	    || cloak_cache[2] != 0.75f || provoker != 7)
		return false;

	xannor_fixture(&tape, &state, &player, &player_record, sector_cache,
	    cloak_cache, &destroyed, &provoker, &headquarters);
	provoker = 7;
	tape.fail_event = XANNOR_BLANK;
	if (yt_xannor_retaliation_run(&state, &ops, &tape, NULL)
	    || tape.event_count != 3U || player_record != 2
	    || strcmp(player.name, "Alice") != 0
	    || cloak_cache[2] != 0.75f || provoker != 7)
		return false;

	xannor_fixture(&tape, &state, &player, &player_record, sector_cache,
	    cloak_cache, &destroyed, &provoker, &headquarters);
	provoker = 7;
	tape.fail_event = XANNOR_DESTINATION_RANDOM;
	if (yt_xannor_retaliation_run(&state, &ops, &tape, NULL)
	    || tape.event_count != 4U || player_record != -1
	    || strcmp(player.name, "The Xannor") != 0
	    || player.sector != 733.0f || cloak_cache[2] != 0.0f
	    || provoker != 7)
		return false;

	xannor_fixture(&tape, &state, &player, &player_record, sector_cache,
	    cloak_cache, &destroyed, &provoker, &headquarters);
	provoker = 7;
	tape.fail_event = XANNOR_ROW;
	if (yt_xannor_retaliation_run(&state, &ops, &tape, NULL)
	    || tape.event_count != 5U || player_record != -1
	    || strcmp(player.name, "The Xannor") != 0
	    || cloak_cache[2] != 0.0f || provoker != 7)
		return false;

	xannor_fixture(&tape, &state, &player, &player_record, sector_cache,
	    cloak_cache, &destroyed, &provoker, &headquarters);
	provoker = 7;
	tape.mutate_child = true;
	tape.fail_event = XANNOR_PROJECTILE;
	if (yt_xannor_retaliation_run(&state, &ops, &tape, NULL)
	    || player_record != -1 || strcmp(player.name, "The Xannor") != 0
	    || player.sector != 733.0f || cloak_cache[2] != 0.0f
	    || provoker != 11 || headquarters != 9.0f
	    || tape.event_count != 6U)
		return false;

	xannor_fixture(&tape, &state, &player, &player_record, sector_cache,
	    cloak_cache, &destroyed, &provoker, &headquarters);
	provoker = 7;
	tape.fail_event = XANNOR_READ_PLAYER;
	if (yt_xannor_retaliation_run(&state, &ops, &tape, NULL)
	    || player_record != 2 || strcmp(player.name, "Alice") != 0
	    || cloak_cache[2] != 0.75f || provoker != 7
	    || tape.event_count != 7U)
		return false;

	xannor_fixture(&tape, &state, &player, &player_record, sector_cache,
	    cloak_cache, &destroyed, &provoker, &headquarters);
	provoker = 7;
	tape.fail_event = XANNOR_WAIT;
	return !yt_xannor_retaliation_run(&state, &ops, &tape, NULL)
	    && tape.event_count == 8U && provoker == 7
	    && player_record == 2 && cloak_cache[2] == 0.75f;
}

enum counterlaunch_tape_event {
	COUNTERLAUNCH_FIRST_GET = 1,
	COUNTERLAUNCH_RANDOM,
	COUNTERLAUNCH_SECOND_GET,
	COUNTERLAUNCH_WRITE,
	COUNTERLAUNCH_BLANK,
	COUNTERLAUNCH_ROW,
	COUNTERLAUNCH_NEWS,
	COUNTERLAUNCH_PROJECTILE,
	COUNTERLAUNCH_FINAL_GET,
	COUNTERLAUNCH_WAIT,
};

struct counterlaunch_tape {
	int events[10];
	size_t event_count;
	int fail_event;
	size_t read_calls;
	size_t random_calls;
	struct yt_player first_target;
	struct yt_player second_target;
	struct yt_player final_player;
	struct yt_player written_player;
	int written_record;
	float draw;
	uint8_t row[192];
	size_t row_length;
	uint8_t news[192];
	size_t news_length;
	struct yt_player *live_player;
	int *live_record;
	float *live_cloak;
	float *live_retained;
	int *live_counterattacker;
	int *live_xannor;
	float projectile_origin;
	float projectile_target;
	float projectile_amount;
	bool child_valid;
	bool mutate_child;
	double wait_seconds;
};

static bool
counterlaunch_tape_step(struct counterlaunch_tape *tape, int event)
{
	if (tape->event_count >= YT_ARRAY_LEN(tape->events))
		return false;
	tape->events[tape->event_count++] = event;
	return tape->fail_event != event;
}

static bool
counterlaunch_read_player(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct counterlaunch_tape *tape = context;
	int event;
	const struct yt_player *source;

	(void)error;
	if (tape->read_calls == 0U) {
		event = COUNTERLAUNCH_FIRST_GET;
		source = &tape->first_target;
		if (player_record != *tape->live_counterattacker)
			return false;
	}
	else if (tape->read_calls == 1U) {
		event = COUNTERLAUNCH_SECOND_GET;
		source = &tape->second_target;
		if (player_record != *tape->live_counterattacker)
			return false;
	}
	else {
		event = COUNTERLAUNCH_FINAL_GET;
		source = &tape->final_player;
		if (player_record != 2)
			return false;
	}
	++tape->read_calls;
	if (!counterlaunch_tape_step(tape, event))
		return false;
	*player = *source;
	return true;
}

static bool
counterlaunch_random(void *context, float *value, struct yt_error *error)
{
	struct counterlaunch_tape *tape = context;

	(void)error;
	++tape->random_calls;
	if (!counterlaunch_tape_step(tape, COUNTERLAUNCH_RANDOM))
		return false;
	*value = tape->draw;
	return true;
}

static bool
counterlaunch_write_player(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct counterlaunch_tape *tape = context;

	(void)error;
	tape->written_record = player_record;
	tape->written_player = *player;
	return counterlaunch_tape_step(tape, COUNTERLAUNCH_WRITE);
}

static bool
counterlaunch_present(void *context, const uint8_t *text, size_t length,
    bool bold, struct yt_error *error)
{
	struct counterlaunch_tape *tape = context;
	int event = bold ? COUNTERLAUNCH_ROW : COUNTERLAUNCH_BLANK;

	(void)error;
	if (!counterlaunch_tape_step(tape, event))
		return false;
	if ((!bold && (text != NULL || length != 0U))
	    || length > sizeof(tape->row))
		return false;
	if (bold && length != 0U)
		memcpy(tape->row, text, length);
	if (bold)
		tape->row_length = length;
	return true;
}

static bool
counterlaunch_news(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	struct counterlaunch_tape *tape = context;

	(void)error;
	if (!counterlaunch_tape_step(tape, COUNTERLAUNCH_NEWS)
	    || length > sizeof(tape->news))
		return false;
	memcpy(tape->news, text, length);
	tape->news_length = length;
	return true;
}

static bool
counterlaunch_projectile(void *context, float *origin, float target,
    float *amount, bool plasma, int *counterattack, int *xannor_provoker,
    struct yt_error *error)
{
	struct counterlaunch_tape *tape = context;

	(void)error;
	tape->projectile_origin = *origin;
	tape->projectile_target = target;
	tape->projectile_amount = *amount;
	tape->child_valid = !plasma && origin != NULL && amount != NULL
	    && counterattack == tape->live_counterattacker
	    && xannor_provoker == tape->live_xannor
	    && amount == tape->live_retained
	    && *tape->live_record == 3
	    && strcmp(tape->live_player->name, "Bob") == 0
	    && tape->live_player->sector == 733.0f
	    && tape->live_cloak[2] == 0.0f;
	if (tape->mutate_child) {
		*origin = 12.0f;
		*amount = 4.0f;
		*counterattack = 5;
		*xannor_provoker = 11;
	}
	return counterlaunch_tape_step(tape, COUNTERLAUNCH_PROJECTILE);
}

static bool
counterlaunch_wait(void *context, double seconds, struct yt_error *error)
{
	struct counterlaunch_tape *tape = context;

	(void)error;
	tape->wait_seconds = seconds;
	return counterlaunch_tape_step(tape, COUNTERLAUNCH_WAIT);
}

static void
counterlaunch_fixture(struct counterlaunch_tape *tape,
    struct yt_counterlaunch_state *state, struct yt_player *player,
    int *player_record, float sector_cache[6], float cloak_cache[6],
    bool *destroyed, float *retained, int *counterattacker, int *xannor)
{
	memset(tape, 0, sizeof(*tape));
	memset(player, 0, sizeof(*player));
	memset(sector_cache, 0, 6U * sizeof(*sector_cache));
	memset(cloak_cache, 0, 6U * sizeof(*cloak_cache));
	(void)snprintf(player->name, sizeof(player->name), "%s", "Alice");
	player->name_length = 5.0f;
	player->score = 2000000.0f;
	player->sector = 733.0f;
	yt_player_encode(player);
	*player_record = 2;
	sector_cache[2] = 733.0f;
	cloak_cache[2] = 0.75f;
	*destroyed = false;
	*retained = 9.0f;
	*counterattacker = 3;
	*xannor = 8;
	(void)snprintf(tape->first_target.name,
	    sizeof(tape->first_target.name), "%s", "Bob");
	tape->first_target.name_length = 3.0f;
	tape->first_target.missiles = 10.0f;
	tape->first_target.sector = 99.0f;
	yt_player_encode(&tape->first_target);
	tape->second_target = tape->first_target;
	tape->second_target.missiles = 99.0f;
	yt_player_encode(&tape->second_target);
	tape->second_target.record.bytes[YT_RECORD_TAIL_OFFSET] = 0x7f;
	tape->final_player = *player;
	tape->final_player.score = 123.0f;
	yt_player_encode(&tape->final_player);
	tape->draw = 0.25f;
	tape->live_player = player;
	tape->live_record = player_record;
	tape->live_cloak = cloak_cache;
	tape->live_retained = retained;
	tape->live_counterattacker = counterattacker;
	tape->live_xannor = xannor;
	state->player = player;
	state->player_record = player_record;
	state->sector_cache = sector_cache;
	state->cloak_cache = cloak_cache;
	state->cache_count = 6U;
	state->destroyed = destroyed;
	state->retained_count = retained;
	state->counterattacker = counterattacker;
	state->xannor_provoker = xannor;
	state->last_player_record = 51;
}

static bool
check_counterlaunch_model(void)
{
	static const struct yt_counterlaunch_ops ops = {
		counterlaunch_read_player,
		counterlaunch_random,
		counterlaunch_write_player,
		counterlaunch_present,
		counterlaunch_news,
		counterlaunch_projectile,
		counterlaunch_wait,
	};
	static const int full_events[10] = {
		COUNTERLAUNCH_FIRST_GET, COUNTERLAUNCH_RANDOM,
		COUNTERLAUNCH_SECOND_GET, COUNTERLAUNCH_WRITE,
		COUNTERLAUNCH_BLANK, COUNTERLAUNCH_ROW, COUNTERLAUNCH_NEWS,
		COUNTERLAUNCH_PROJECTILE, COUNTERLAUNCH_FINAL_GET,
		COUNTERLAUNCH_WAIT,
	};
	static const uint8_t expected_row[] =
	    "Bob shot back with 3 missiles at you!";
	static const uint8_t expected_news[] =
	    "Bob shot back with 3 missiles at Alice!";
	struct yt_counterlaunch_state state;
	struct counterlaunch_tape tape;
	struct yt_player player;
	struct yt_player original;
	float sector_cache[6];
	float cloak_cache[6];
	float retained;
	int player_record;
	int counterattacker;
	int xannor;
	bool destroyed;
	int gate;
	int failure;

	for (gate = 0; gate < 3; ++gate) {
		counterlaunch_fixture(&tape, &state, &player, &player_record,
		    sector_cache, cloak_cache, &destroyed, &retained,
		    &counterattacker, &xannor);
		counterattacker = gate == 0 ? 1 : gate == 1 ? 52 : 2;
		if (!yt_counterlaunch_run(&state, &ops, &tape, NULL)
		    || tape.event_count != 0U
		    || counterattacker != (gate == 0 ? 1 : gate == 1 ? 52 : 2))
			return false;
	}

	counterlaunch_fixture(&tape, &state, &player, &player_record,
	    sector_cache, cloak_cache, &destroyed, &retained, &counterattacker,
	    &xannor);
	tape.first_target.killed_by = -1.0f;
	yt_player_encode(&tape.first_target);
	if (!yt_counterlaunch_run(&state, &ops, &tape, NULL)
	    || tape.event_count != 1U || counterattacker != 0
	    || player_record != 2 || cloak_cache[2] != 0.75f)
		return false;
	counterlaunch_fixture(&tape, &state, &player, &player_record,
	    sector_cache, cloak_cache, &destroyed, &retained, &counterattacker,
	    &xannor);
	tape.first_target.missiles = 0.5f;
	yt_player_encode(&tape.first_target);
	if (!yt_counterlaunch_run(&state, &ops, &tape, NULL)
	    || tape.event_count != 1U || counterattacker != 0)
		return false;

	counterlaunch_fixture(&tape, &state, &player, &player_record,
	    sector_cache, cloak_cache, &destroyed, &retained, &counterattacker,
	    &xannor);
	original = player;
	tape.final_player.killed_by = -1.0f;
	yt_player_encode(&tape.final_player);
	tape.mutate_child = true;
	if (!yt_counterlaunch_run(&state, &ops, &tape, NULL)
	    || tape.event_count != YT_ARRAY_LEN(full_events)
	    || memcmp(tape.events, full_events, sizeof(full_events)) != 0
	    || tape.random_calls != 1U || !tape.child_valid
	    || tape.projectile_origin != 99.0f
	    || tape.projectile_target != 733.0f
	    || tape.projectile_amount != 3.0f || tape.written_record != 3
	    || tape.written_player.missiles != 7.0f
	    || tape.written_player.record.bytes[YT_RECORD_TAIL_OFFSET] != 0x7f
	    || tape.row_length != sizeof(expected_row) - 1U
	    || memcmp(tape.row, expected_row, sizeof(expected_row) - 1U) != 0
	    || tape.news_length != sizeof(expected_news) - 1U
	    || memcmp(tape.news, expected_news, sizeof(expected_news) - 1U) != 0
	    || player_record != 2 || memcmp(&player, &original, sizeof(player)) != 0
	    || cloak_cache[2] != 0.75f || sector_cache[2] != 0.0f
	    || !destroyed || retained != 4.0f || counterattacker != 0
	    || xannor != 11 || tape.wait_seconds != 4.0)
		return false;

	counterlaunch_fixture(&tape, &state, &player, &player_record,
	    sector_cache, cloak_cache, &destroyed, &retained, &counterattacker,
	    &xannor);
	player.score = -1.0f;
	yt_player_encode(&player);
	retained = -2.5f;
	if (!yt_counterlaunch_run(&state, &ops, &tape, NULL)
	    || tape.random_calls != 0U || tape.projectile_amount != -2.5f
	    || tape.written_player.missiles != 12.5f || retained != -2.5f
	    || counterattacker != 0 || xannor != 8)
		return false;

	for (failure = COUNTERLAUNCH_FIRST_GET;
	    failure <= COUNTERLAUNCH_WAIT; ++failure) {
		counterlaunch_fixture(&tape, &state, &player, &player_record,
		    sector_cache, cloak_cache, &destroyed, &retained,
		    &counterattacker, &xannor);
		tape.fail_event = failure;
		tape.mutate_child = true;
		if (yt_counterlaunch_run(&state, &ops, &tape, NULL)
		    || tape.event_count != (size_t)failure
		    || tape.events[tape.event_count - 1U] != failure)
			return false;
		if (failure == COUNTERLAUNCH_FIRST_GET
		    && (player_record != 2 || counterattacker != 3
		    || cloak_cache[2] != 0.75f))
			return false;
		if (failure == COUNTERLAUNCH_PROJECTILE
		    && (player_record != 3 || strcmp(player.name, "Bob") != 0
		    || player.sector != 733.0f || cloak_cache[2] != 0.0f
		    || retained != 4.0f || counterattacker != 5 || xannor != 11))
			return false;
		if (failure == COUNTERLAUNCH_WAIT
		    && (player_record != 2 || counterattacker != 0
		    || cloak_cache[2] != 0.75f))
			return false;
	}
	return true;
}

struct salvage_draw_tape {
	float range[4];
	float result[4];
	size_t calls;
	size_t fail_call;
};

static bool
salvage_draw(void *context, float range, float *one_based,
    struct yt_error *error)
{
	struct salvage_draw_tape *tape = context;
	size_t call = tape->calls++;

	(void)error;
	if (call >= YT_ARRAY_LEN(tape->range))
		return false;
	tape->range[call] = range;
	if (tape->fail_call != 0U && call + 1U == tape->fail_call)
		return false;
	*one_based = tape->result[call];
	return true;
}

static bool
check_salvage_cargo_sampler(void)
{
	struct yt_salvage_cargo_state state;
	struct salvage_draw_tape tape;
	static const uint8_t salvor[] = {'A', 0, 'B'};
	static const uint8_t victim[] = {'V', 0, 'X'};
	static const uint8_t header_expected[] =
	    " *** A\0B salvaged the following from V\0X's ship:";
	static const uint8_t credit_expected[] = "  -  Credits: 2";
	static const uint8_t mine_expected[] = "  -  Sector Mines:-1";
	static const uint8_t empty_expected[] = "  -  2 empty holds";
	static const uint8_t equipment_expected[] =
	    "  -  3 holds of equipment";
	static const float descending[4] = {4.0f, 3.0f, 2.0f, 1.0f};
	static const float each_one[4] = {1.0f, 1.0f, 1.0f, 1.0f};
	uint8_t row[128];
	size_t length;

	memset(&state, 0, sizeof(state));
	memset(&tape, 0, sizeof(tape));
	state.requested = 4.0f;
	state.stock[0] = 1.0f;
	state.stock[1] = 1.0f;
	state.stock[2] = 1.0f;
	state.remaining = 4.0f;
	memcpy(tape.result, each_one, sizeof(each_one));
	if (!yt_salvage_cargo_sample(&state, salvage_draw, &tape, NULL)
	    || tape.calls != 4U
	    || memcmp(tape.range, descending, sizeof(descending)) != 0
	    || memcmp(state.awards, each_one, sizeof(each_one)) != 0
	    || state.stock[0] != 0.0f || state.stock[1] != 0.0f
	    || state.stock[2] != 0.0f || state.remaining != 0.0f)
		return false;

	memset(&state, 0, sizeof(state));
	memset(&tape, 0, sizeof(tape));
	state.requested = 1.5f;
	state.stock[0] = 0.5f;
	state.remaining = 1.0f;
	tape.result[0] = 1.0f;
	if (!yt_salvage_cargo_sample(&state, salvage_draw, &tape, NULL)
	    || tape.calls != 1U || state.awards[0] != 1.0f
	    || state.stock[0] != -0.5f || state.remaining != 0.0f)
		return false;

	memset(&state, 0, sizeof(state));
	memset(&tape, 0, sizeof(tape));
	state.requested = 1.0f;
	state.remaining = -3.5f;
	tape.result[0] = -1.0f;
	if (!yt_salvage_cargo_sample(&state, salvage_draw, &tape, NULL)
	    || tape.calls != 1U || tape.range[0] != -3.5f
	    || state.awards[0] != 1.0f || state.remaining != -4.5f)
		return false;

	memset(&state, 0, sizeof(state));
	memset(&tape, 0, sizeof(tape));
	state.requested = 1.0f;
	state.remaining = 1.0f;
	tape.result[0] = 5.0f;
	if (!yt_salvage_cargo_sample(&state, salvage_draw, &tape, NULL)
	    || state.awards[3] != 1.0f)
		return false;

	memset(&state, 0, sizeof(state));
	memset(&tape, 0, sizeof(tape));
	state.requested = 3.0f;
	state.stock[0] = 3.0f;
	state.remaining = 3.0f;
	tape.result[0] = 1.0f;
	tape.fail_call = 2U;
	if (yt_salvage_cargo_sample(&state, salvage_draw, &tape, NULL)
	    || tape.calls != 2U || tape.range[0] != 3.0f
	    || tape.range[1] != 2.0f || state.awards[0] != 1.0f
	    || state.stock[0] != 2.0f || state.remaining != 2.0f)
		return false;
	return yt_salvage_header_row(salvor, sizeof(salvor), victim,
	    sizeof(victim), row, sizeof(row), &length)
	    && length == sizeof(header_expected) - 1U
	    && memcmp(row, header_expected, length) == 0
	    && yt_salvage_simple_row(YT_SALVAGE_CREDITS, 2.0f,
	    row, sizeof(row), &length)
	    && length == sizeof(credit_expected) - 1U
	    && memcmp(row, credit_expected, length) == 0
	    && yt_salvage_simple_row(YT_SALVAGE_MINES, -1.0f,
	    row, sizeof(row), &length)
	    && length == sizeof(mine_expected) - 1U
	    && memcmp(row, mine_expected, length) == 0
	    && yt_salvage_cargo_row(YT_SALVAGE_EMPTY_HOLDS, 2.0f,
	    row, sizeof(row), &length)
	    && length == sizeof(empty_expected) - 1U
	    && memcmp(row, empty_expected, length) == 0
	    && yt_salvage_cargo_row(YT_SALVAGE_EQUIPMENT, 3.0f,
	    row, sizeof(row), &length)
	    && length == sizeof(equipment_expected) - 1U
	    && memcmp(row, equipment_expected, length) == 0
	    && !yt_salvage_header_row(salvor, sizeof(salvor), victim,
	    sizeof(victim), row, sizeof(header_expected) - 2U, &length)
	    && !yt_salvage_simple_row((enum yt_salvage_simple_kind)99, 1.0f,
	    row, sizeof(row), &length)
	    && !yt_salvage_cargo_row((enum yt_salvage_cargo_kind)99, 1.0f,
	    row, sizeof(row), &length);
}

enum salvage_event {
	SALVAGE_READ_VICTIM = 1,
	SALVAGE_READ_KILLER,
	SALVAGE_WRITE_KILLER,
	SALVAGE_RANDOM,
	SALVAGE_CARGO_DRAW,
	SALVAGE_WAIT,
	SALVAGE_PRESENT,
	SALVAGE_NEWS,
};

struct salvage_tape {
	const enum salvage_event *expected;
	size_t expected_count;
	size_t event_count;
	size_t fail_at;
	float expected_killer;
	struct yt_player victim;
	struct yt_player killer;
	float draws[6];
	size_t draw_position;
	float cargo_results[8];
	size_t cargo_position;
	float waits[16];
	size_t wait_count;
	size_t present_count;
	size_t news_count;
	size_t write_count;
};

static bool
salvage_step(struct salvage_tape *tape, enum salvage_event event,
	struct yt_error *error)
{
	if (tape->event_count >= tape->expected_count
	    || tape->expected[tape->event_count] != event)
		return false;
	++tape->event_count;
	if (tape->event_count != tape->fail_at)
		return true;
	if (error != NULL) {
		error->status = YT_IO_ERROR;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    "salvage injected failure");
	}
	return false;
}

static bool
salvage_read_victim_test(void *context, int player_record,
	struct yt_player *player, struct yt_error *error)
{
	struct salvage_tape *tape = context;

	if (player_record != 3
	    || !salvage_step(tape, SALVAGE_READ_VICTIM, error))
		return false;
	*player = tape->victim;
	return true;
}

static bool
salvage_read_killer_test(void *context, float player_record,
	struct yt_player *player, struct yt_error *error)
{
	struct salvage_tape *tape = context;

	if (player_record != tape->expected_killer
	    || !salvage_step(tape, SALVAGE_READ_KILLER, error))
		return false;
	*player = tape->killer;
	return true;
}

static bool
salvage_write_killer_test(void *context, float player_record,
	struct yt_player *player, struct yt_error *error)
{
	struct salvage_tape *tape = context;

	if (player_record != tape->expected_killer
	    || !salvage_step(tape, SALVAGE_WRITE_KILLER, error))
		return false;
	tape->killer = *player;
	++tape->write_count;
	return true;
}

static bool
salvage_random_test(void *context, float *value, struct yt_error *error)
{
	struct salvage_tape *tape = context;

	if (tape->draw_position >= YT_ARRAY_LEN(tape->draws)
	    || !salvage_step(tape, SALVAGE_RANDOM, error))
		return false;
	*value = tape->draws[tape->draw_position++];
	return true;
}

static bool
salvage_cargo_draw_test(void *context, float range, float *one_based,
	struct yt_error *error)
{
	struct salvage_tape *tape = context;

	(void)range;
	if (tape->cargo_position >= YT_ARRAY_LEN(tape->cargo_results)
	    || !salvage_step(tape, SALVAGE_CARGO_DRAW, error))
		return false;
	*one_based = tape->cargo_results[tape->cargo_position++];
	return true;
}

static bool
salvage_wait_test(void *context, float duration, struct yt_error *error)
{
	struct salvage_tape *tape = context;

	if (!salvage_step(tape, SALVAGE_WAIT, error))
		return false;
	if (tape->wait_count >= YT_ARRAY_LEN(tape->waits))
		return false;
	tape->waits[tape->wait_count++] = duration;
	return true;
}

static bool
salvage_present_test(void *context, const uint8_t *text, size_t length,
	bool bold, struct yt_error *error)
{
	struct salvage_tape *tape = context;

	(void)text;
	(void)length;
	(void)bold;
	if (!salvage_step(tape, SALVAGE_PRESENT, error))
		return false;
	++tape->present_count;
	return true;
}

static bool
salvage_news_test(void *context, const uint8_t *text, size_t length,
	struct yt_error *error)
{
	struct salvage_tape *tape = context;

	(void)text;
	(void)length;
	if (!salvage_step(tape, SALVAGE_NEWS, error))
		return false;
	++tape->news_count;
	return true;
}

static const struct yt_salvage_ops salvage_ops = {
	salvage_read_victim_test,
	salvage_read_killer_test,
	salvage_write_killer_test,
	salvage_random_test,
	salvage_cargo_draw_test,
	salvage_wait_test,
	salvage_present_test,
	salvage_news_test,
};

static void
salvage_fixture(struct salvage_tape *tape,
	const enum salvage_event *expected, size_t expected_count,
	float killer)
{
	struct yt_record raw;

	memset(tape, 0, sizeof(*tape));
	tape->expected = expected;
	tape->expected_count = expected_count;
	tape->expected_killer = killer;
	yt_record_blank(&raw);
	memcpy(raw.bytes, "V\0X", 3U);
	(void)yt_record_set_number(&raw, YT_F85, 3.0f);
	yt_player_decode(&tape->victim, &raw);
	yt_record_blank(&raw);
	yt_player_decode(&tape->killer, &raw);
}

static bool
check_salvage_transaction(void)
{
	static const enum salvage_event zero_events[] = {
		SALVAGE_READ_VICTIM,
		SALVAGE_PRESENT, SALVAGE_PRESENT, SALVAGE_NEWS, SALVAGE_PRESENT,
		SALVAGE_RANDOM, SALVAGE_RANDOM, SALVAGE_RANDOM,
		SALVAGE_RANDOM, SALVAGE_RANDOM, SALVAGE_RANDOM,
		SALVAGE_WAIT, SALVAGE_READ_KILLER, SALVAGE_WRITE_KILLER,
		SALVAGE_WAIT, SALVAGE_NEWS, SALVAGE_PRESENT, SALVAGE_WAIT,
	};
	static const enum salvage_event cargo_events[] = {
		SALVAGE_READ_VICTIM,
		SALVAGE_PRESENT, SALVAGE_PRESENT, SALVAGE_NEWS, SALVAGE_PRESENT,
		SALVAGE_RANDOM, SALVAGE_RANDOM, SALVAGE_RANDOM,
		SALVAGE_RANDOM, SALVAGE_RANDOM, SALVAGE_RANDOM,
		SALVAGE_WAIT, SALVAGE_READ_KILLER, SALVAGE_WRITE_KILLER,
		SALVAGE_READ_VICTIM, SALVAGE_CARGO_DRAW, SALVAGE_CARGO_DRAW,
		SALVAGE_READ_KILLER, SALVAGE_WRITE_KILLER, SALVAGE_WAIT,
		SALVAGE_WAIT, SALVAGE_NEWS, SALVAGE_PRESENT,
		SALVAGE_WAIT, SALVAGE_NEWS, SALVAGE_PRESENT, SALVAGE_WAIT,
	};
	static const enum salvage_event invalid_events[] = {
		SALVAGE_READ_VICTIM,
	};
	struct salvage_tape tape;
	struct yt_salvage_state state;
	struct yt_error error;
	size_t failure;

	salvage_fixture(&tape, zero_events, YT_ARRAY_LEN(zero_events), 2.5f);
	state = (struct yt_salvage_state){
		.victim_record = 3,
		.killer_record = 2.5f,
		.last_player_record = 51.0f,
		.maximum_holds = 20.0f,
		.current_name = (const uint8_t *)"KILLER",
		.current_name_length = 6U,
	};
	if (!yt_salvage_run(&state, &salvage_ops, &tape, NULL)
	    || tape.event_count != YT_ARRAY_LEN(zero_events)
	    || tape.draw_position != 6U || tape.cargo_position != 0U
	    || tape.write_count != 1U || tape.wait_count != 3U
	    || tape.news_count != 2U || tape.present_count != 4U
	    || tape.waits[0] != 1.0f || tape.waits[1] != 0.5f
	    || tape.waits[2] != 4.0f || !state.admitted || state.emitted
	    || !state.complete)
		return false;

	for (failure = 1U; failure <= YT_ARRAY_LEN(zero_events); ++failure) {
		salvage_fixture(&tape, zero_events, YT_ARRAY_LEN(zero_events),
		    2.5f);
		tape.fail_at = failure;
		state = (struct yt_salvage_state){
			.victim_record = 3,
			.killer_record = 2.5f,
			.last_player_record = 51.0f,
			.maximum_holds = 20.0f,
			.current_name = (const uint8_t *)"KILLER",
			.current_name_length = 6U,
		};
		yt_error_clear(&error);
		if (yt_salvage_run(&state, &salvage_ops, &tape, &error)
		    || tape.event_count != failure || state.complete
		    || error.status != YT_IO_ERROR)
			return false;
	}

	salvage_fixture(&tape, cargo_events, YT_ARRAY_LEN(cargo_events), 2.0f);
	tape.victim.holds = 4.0f;
	tape.victim.ore = 1.0f;
	tape.draws[0] = 0.5f;
	tape.cargo_results[0] = 1.0f;
	tape.cargo_results[1] = 1.0f;
	state = (struct yt_salvage_state){
		.victim_record = 3,
		.killer_record = 2.0f,
		.last_player_record = 51.0f,
		.maximum_holds = 20.0f,
		.current_name = (const uint8_t *)"KILLER",
		.current_name_length = 6U,
	};
	if (!yt_salvage_run(&state, &salvage_ops, &tape, NULL)
	    || tape.event_count != YT_ARRAY_LEN(cargo_events)
	    || tape.write_count != 2U || tape.cargo_position != 2U
	    || tape.news_count != 3U || tape.present_count != 5U
	    || tape.wait_count != 5U || state.requested_holds != 2.0f
	    || state.cargo.awards[0] != 1.0f
	    || state.cargo.awards[3] != 1.0f
	    || tape.killer.holds != 2.0f || tape.killer.ore != 1.0f
	    || !state.emitted || !state.complete)
		return false;

	salvage_fixture(&tape, invalid_events, YT_ARRAY_LEN(invalid_events),
	    1.5f);
	state = (struct yt_salvage_state){
		.victim_record = 3,
		.killer_record = 1.5f,
		.last_player_record = 51.0f,
		.maximum_holds = 20.0f,
	};
	return yt_salvage_run(&state, &salvage_ops, &tape, NULL)
	    && tape.event_count == 1U && !state.admitted && state.complete;
}

struct port_name_tape {
	int events[32];
	size_t calls;
	size_t fail_call;
	const uint8_t *entered[4];
	size_t entered_length[4];
	size_t edit_calls;
	bool accepted[4];
	uint8_t confirmation[4][128];
	size_t confirmation_length[4];
	size_t confirm_calls;
	size_t blank_calls;
	size_t write_calls;
	uint8_t current_row[128];
	size_t current_length;
	int logical_port;
	struct yt_record durable;
};

static bool
port_name_tape_event(struct port_name_tape *tape, int event)
{
	if (tape->calls >= YT_ARRAY_LEN(tape->events))
		return false;
	tape->events[tape->calls++] = event;
	return tape->fail_call == 0U || tape->calls != tape->fail_call;
}

static bool
port_name_tape_row(void *context, enum yt_port_name_row_kind kind,
    const uint8_t *text, size_t length, struct yt_error *error)
{
	struct port_name_tape *tape = context;

	(void)error;
	if (kind == YT_PORT_NAME_CURRENT_ROW) {
		if (length > sizeof(tape->current_row))
			return false;
		memcpy(tape->current_row, text, length);
		tape->current_length = length;
	}
	return port_name_tape_event(tape, 10 + (int)kind);
}

static bool
port_name_tape_prompt(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	static const uint8_t expected[] = "-=> ";

	(void)error;
	if (length != sizeof(expected) - 1U
	    || memcmp(text, expected, length) != 0)
		return false;
	return port_name_tape_event(context, 20);
}

static bool
port_name_tape_edit(void *context, uint8_t *response, size_t capacity,
    size_t *length, struct yt_error *error)
{
	struct port_name_tape *tape = context;
	size_t index = tape->edit_calls++;

	(void)error;
	if (!port_name_tape_event(tape, 30)
	    || index >= YT_ARRAY_LEN(tape->entered)
	    || tape->entered_length[index] > capacity)
		return false;
	memcpy(response, tape->entered[index], tape->entered_length[index]);
	*length = tape->entered_length[index];
	return true;
}

static bool
port_name_tape_blank(void *context, struct yt_error *error)
{
	struct port_name_tape *tape = context;

	(void)error;
	++tape->blank_calls;
	return port_name_tape_event(tape, 40);
}

static bool
port_name_tape_confirm(void *context, const uint8_t *prompt, size_t length,
    bool *accepted, struct yt_error *error)
{
	struct port_name_tape *tape = context;
	size_t index = tape->confirm_calls++;

	(void)error;
	if (!port_name_tape_event(tape, 50)
	    || index >= YT_ARRAY_LEN(tape->accepted)
	    || length > sizeof(tape->confirmation[index]))
		return false;
	memcpy(tape->confirmation[index], prompt, length);
	tape->confirmation_length[index] = length;
	*accepted = tape->accepted[index];
	return true;
}

static bool
port_name_tape_write(void *context, int logical_port,
    const struct yt_record *record, struct yt_error *error)
{
	struct port_name_tape *tape = context;

	(void)error;
	++tape->write_calls;
	tape->logical_port = logical_port;
	if (!port_name_tape_event(tape, 60))
		return false;
	tape->durable = *record;
	return true;
}

static bool
check_port_name_editor_transaction(void)
{
	static const struct yt_port_name_editor_ops ops = {
		.row = port_name_tape_row,
		.prompt = port_name_tape_prompt,
		.edit = port_name_tape_edit,
		.blank = port_name_tape_blank,
		.confirm = port_name_tape_confirm,
		.write = port_name_tape_write,
	};
	static const uint8_t cached[] = {'C', 0, 'P'};
	static const uint8_t spaces[] = "   ";
	static const uint8_t replacement[] = "  new PORT  ";
	static const uint8_t expected_current[] =
	    "This port is called: \"C\0P\".";
	static const uint8_t expected_cached_confirmation[] =
	    "\"C\0P\" Is this OK? [y/N]";
	static const uint8_t expected_new_confirmation[] =
	    "\"New Port\" Is this OK? [y/N]";
	static const int expected_events[] = {
		10, 11, 12, 20, 30, 40, 50,
		10, 11, 12, 20, 30, 40, 50, 60,
	};
	struct yt_port_name_editor_state state;
	struct port_name_tape tape;
	struct yt_port port;
	struct yt_record before;
	uint8_t long_cached[42];
	size_t index;

	memset(&port, 0, sizeof(port));
	for (index = 0U; index < sizeof(port.record.bytes); ++index)
		port.record.bytes[index] = (uint8_t)(index ^ 0x5aU);
	before = port.record;
	memset(&tape, 0, sizeof(tape));
	tape.entered[0] = spaces;
	tape.entered_length[0] = sizeof(spaces) - 1U;
	tape.entered[1] = replacement;
	tape.entered_length[1] = sizeof(replacement) - 1U;
	tape.accepted[0] = false;
	tape.accepted[1] = true;
	state.cached = cached;
	state.cached_length = sizeof(cached);
	state.logical_port = 17;
	state.port = &port;
	if (!yt_port_name_editor_run(&state, &ops, &tape, NULL)
	    || tape.calls != YT_ARRAY_LEN(expected_events)
	    || memcmp(tape.events, expected_events,
	    sizeof(expected_events)) != 0
	    || tape.current_length != sizeof(expected_current) - 1U
	    || memcmp(tape.current_row, expected_current,
	    sizeof(expected_current) - 1U) != 0
	    || tape.confirmation_length[0]
	    != sizeof(expected_cached_confirmation) - 1U
	    || memcmp(tape.confirmation[0], expected_cached_confirmation,
	    sizeof(expected_cached_confirmation) - 1U) != 0
	    || tape.confirmation_length[1]
	    != sizeof(expected_new_confirmation) - 1U
	    || memcmp(tape.confirmation[1], expected_new_confirmation,
	    sizeof(expected_new_confirmation) - 1U) != 0
	    || tape.blank_calls != 2U || tape.confirm_calls != 2U
	    || tape.write_calls != 1U || tape.logical_port != 17
	    || memcmp(port.record.bytes, "New Port", 8U) != 0
	    || yt_record_get_number(&port.record, YT_F85) != 8.0f
	    || memcmp(&tape.durable, &port.record, sizeof(port.record)) != 0
	    || memcmp(port.record.bytes + YT_TEXT_FIELD_SIZE,
	    before.bytes + YT_TEXT_FIELD_SIZE,
	    YT_F85 - YT_TEXT_FIELD_SIZE) != 0
	    || memcmp(port.record.bytes + YT_F89, before.bytes + YT_F89,
	    sizeof(port.record.bytes) - YT_F89) != 0)
		return false;

	memset(&port, 0xa5, sizeof(port));
	before = port.record;
	memset(&tape, 0, sizeof(tape));
	tape.entered[0] = (const uint8_t *)"";
	tape.entered[1] = (const uint8_t *)"";
	tape.fail_call = 6U;
	state.cached = NULL;
	state.cached_length = 0U;
	state.port = &port;
	if (yt_port_name_editor_run(&state, &ops, &tape, NULL)
	    || tape.calls != 6U || tape.events[5] != 10
	    || tape.blank_calls != 0U || tape.confirm_calls != 0U
	    || tape.write_calls != 0U
	    || memcmp(&port.record, &before, sizeof(before)) != 0)
		return false;

	memset(long_cached, 'Z', sizeof(long_cached));
	memset(&port, 0x3c, sizeof(port));
	before = port.record;
	memset(&tape, 0, sizeof(tape));
	tape.entered[0] = (const uint8_t *)"";
	tape.accepted[0] = true;
	state.cached = long_cached;
	state.cached_length = sizeof(long_cached);
	if (!yt_port_name_editor_run(&state, &ops, &tape, NULL)
	    || tape.write_calls != 1U || port.name_length != 42.0f
	    || yt_record_get_number(&port.record, YT_F85) != 42.0f)
		return false;
	for (index = 0U; index < YT_TEXT_FIELD_SIZE; ++index) {
		if (port.record.bytes[index] != 'Z')
			return false;
	}
	if (memcmp(port.record.bytes + YT_TEXT_FIELD_SIZE,
	    before.bytes + YT_TEXT_FIELD_SIZE,
	    YT_F85 - YT_TEXT_FIELD_SIZE) != 0)
		return false;

	for (index = 1U; index <= 8U; ++index) {
		memset(&port, 0x69, sizeof(port));
		before = port.record;
		memset(&tape, 0, sizeof(tape));
		tape.entered[0] = replacement;
		tape.entered_length[0] = sizeof(replacement) - 1U;
		tape.accepted[0] = true;
		tape.fail_call = index;
		state.cached = cached;
		state.cached_length = sizeof(cached);
		if (yt_port_name_editor_run(&state, &ops, &tape, NULL)
		    || tape.calls != index)
			return false;
		if (index < 8U) {
			if (memcmp(&port.record, &before, sizeof(before)) != 0)
				return false;
		}
		else if (memcmp(port.record.bytes, "New Port", 8U) != 0
		    || yt_record_get_number(&port.record, YT_F85) != 8.0f
		    || memcmp(&tape.durable, &(struct yt_record){0},
		    sizeof(tape.durable)) != 0)
			return false;
	}
	return true;
}

static bool
check_port_name_editor_model(void)
{
	static const uint8_t cached[] = {'C', 0, 'P'};
	static const uint8_t expected_display[] =
	    "This port is called: \"C\0P\".";
	static const uint8_t entered[] = "  new PORT  ";
	static const uint8_t expected_candidate[] = "New Port";
	static const uint8_t expected_confirmation[] =
	    "\"New Port\" Is this OK? [y/N]";
	static const uint8_t expected_genesis_prompt[] =
	    "Are you that Trader C\0P [y/N]";
	static const uint8_t expected_genesis_first[] =
	    "You are not up to the challenge. You must own 299.5 ports before "
	    "you are powerful";
	static const uint8_t expected_genesis_second[] =
	    "enough to initiate Genesis. You are .25 short of fulfilling the "
	    "prophesy.";
	static const uint8_t embedded[] = {'N', 0, 'M'};
	struct yt_port port;
	struct yt_record before;
	uint8_t row[128];
	uint8_t second_row[128];
	uint8_t candidate[64];
	size_t length;
	size_t second_length;
	size_t index;
	int logical_port;
	float relative_port;
	static const float price_boundary[3] = {
		16777216.0f, 3.0f, -16777210.0f
	};
	static const float price_negative[3] = {-1.0f, 0.0f, 0.0f};
	uint8_t raw_number[4];

	if (!yt_port_name_display_row(cached, sizeof(cached), row,
	    sizeof(row), &length)
	    || length != sizeof(expected_display) - 1U
	    || memcmp(row, expected_display, length) != 0
	    || yt_port_name_display_row(cached, sizeof(cached), row,
	    4U, &length)
	    || !yt_port_name_prepare_candidate(entered, sizeof(entered) - 1U,
	    cached, sizeof(cached), candidate, sizeof(candidate), &length)
	    || length != sizeof(expected_candidate) - 1U
	    || memcmp(candidate, expected_candidate, length) != 0
	    || !yt_port_name_confirmation_prompt(candidate, length, row,
	    sizeof(row), &length)
	    || length != sizeof(expected_confirmation) - 1U
	    || memcmp(row, expected_confirmation, length) != 0
	    || !yt_genesis_confirmation_prompt(cached, sizeof(cached), row,
	    sizeof(row), &length)
	    || length != sizeof(expected_genesis_prompt) - 1U
	    || memcmp(row, expected_genesis_prompt, length) != 0
	    || !yt_genesis_insufficient_rows(299.5f, 299.25f,
	    row, sizeof(row), &length, second_row, sizeof(second_row),
	    &second_length)
	    || length != sizeof(expected_genesis_first) - 1U
	    || memcmp(row, expected_genesis_first, length) != 0
	    || second_length != sizeof(expected_genesis_second) - 1U
	    || memcmp(second_row, expected_genesis_second, second_length) != 0)
		return false;
	{
		uint8_t long_name[42];

		memset(long_name, 'A', sizeof(long_name));
		if (!yt_port_name_prepare_candidate(long_name,
		    sizeof(long_name), NULL, 0U, candidate, sizeof(candidate),
		    &length) || length != YT_TEXT_FIELD_SIZE
		    || candidate[0] != 'A')
			return false;
		for (index = 1U; index < length; ++index) {
			if (candidate[index] != 'a')
				return false;
		}
	}
	if (!yt_port_name_prepare_candidate((const uint8_t *)"   ", 3U,
	    cached, sizeof(cached), candidate, sizeof(candidate), &length)
	    || length != sizeof(cached)
	    || memcmp(candidate, cached, length) != 0
	    || !yt_port_name_prepare_candidate(NULL, 0U, NULL, 0U,
	    candidate, sizeof(candidate), &length) || length != 0U
	    || yt_port_name_prepare_candidate(entered, sizeof(entered) - 1U,
	    cached, sizeof(cached), candidate, 4U, &length))
		return false;
	memset(&port, 0, sizeof(port));
	for (index = 0U; index < sizeof(port.record.bytes); ++index)
		port.record.bytes[index] = (uint8_t)(index ^ 0xa5U);
	before = port.record;
	if (!yt_port_name_overlay(&port, embedded, sizeof(embedded))
	    || memcmp(port.record.bytes, embedded, sizeof(embedded)) != 0
	    || port.name_length != 3.0f
	    || yt_record_get_number(&port.record, YT_F85) != 3.0f)
		return false;
	for (index = sizeof(embedded); index < YT_TEXT_FIELD_SIZE; ++index) {
		if (port.record.bytes[index] != ' ')
			return false;
	}
	if (memcmp(port.record.bytes + YT_TEXT_FIELD_SIZE,
	    before.bytes + YT_TEXT_FIELD_SIZE,
	    YT_F85 - YT_TEXT_FIELD_SIZE) == 0
	    && memcmp(port.record.bytes + YT_F89, before.bytes + YT_F89,
	    sizeof(port.record.bytes) - YT_F89) == 0
	    && yt_port_rename_record(2055.0f, 3.0f,
	    &logical_port, &relative_port) && logical_port == 3
	    && relative_port == 3.0f
	    && yt_port_rename_record(2055.0f, 2.5f,
	    &logical_port, &relative_port) && logical_port == 2
	    && relative_port == 2.5f
	    && yt_port_rename_record(2055.0f,
	    nextafterf(1.0f, INFINITY), &logical_port, &relative_port)
	    && logical_port == 1 && relative_port == 1.0f
	    && yt_port_purchase_price(price_boundary) == 2.0
	    && yt_port_purchase_price(price_negative) == 0.0
	    && qb_mbf32_encode(yt_port_purchase_seller_credit(16777216.0f,
	    1.0f, 1.0), raw_number) == QB_MBF_OK
	    && memcmp(raw_number, (const uint8_t[]){0x01, 0x00, 0x00, 0x99},
	    4U) == 0
	    && qb_mbf32_encode(yt_port_purchase_seller_credit(
	    72057594037927936.0f, 1.0f, -72057594037927936.0), raw_number)
	    == QB_MBF_OK
	    && memcmp(raw_number, (const uint8_t[]){0x00, 0x00, 0x00, 0x00},
	    4U) == 0
	    && qb_mbf32_encode(yt_port_purchase_buyer_credit(16777218.0f,
	    1.0), raw_number) == QB_MBF_OK
	    && memcmp(raw_number, (const uint8_t[]){0x00, 0x00, 0x00, 0x99},
	    4U) == 0) {
		struct yt_player seller;
		struct yt_player buyer;
		struct yt_port purchase_port;
		struct yt_record expected;

		memset(&seller, 0, sizeof(seller));
		memset(seller.record.bytes, 0xa5, YT_RECORD_SIZE);
		seller.credits = 10.0f;
		seller.ports_owned = 3.0f;
		(void)yt_record_set_number(&seller.record, YT_F81, 10.0f);
		(void)yt_record_set_number(&seller.record, YT_F117, 3.0f);
		expected = seller.record;
		(void)yt_record_set_number(&expected, YT_F81, 16.0f);
		(void)yt_record_set_number(&expected, YT_F117, 2.0f);
		if (!yt_port_purchase_seller_overlay(&seller, 4.0f, 2.0)
		    || seller.credits != 16.0f || seller.ports_owned != 2.0f
		    || memcmp(seller.record.bytes, expected.bytes,
		    YT_RECORD_SIZE) != 0)
			return false;

		memset(&purchase_port, 0, sizeof(purchase_port));
		memset(purchase_port.record.bytes, 0x5a, YT_RECORD_SIZE);
		purchase_port.owner = 7.0f;
		purchase_port.treasury = 0.0f;
		memcpy(purchase_port.record.bytes + YT_F89,
		    (const uint8_t[]){0x12, 0x34, 0x56, 0x00}, 4U);
		(void)yt_record_set_number(&purchase_port.record, YT_F97, 7.0f);
		expected = purchase_port.record;
		(void)yt_record_set_number(&expected, YT_F89, 0.0f);
		(void)yt_record_set_number(&expected, YT_F97, 2.0f);
		if (!yt_port_purchase_title_overlay(&purchase_port, 2)
		    || purchase_port.owner != 2.0f
		    || purchase_port.treasury != 0.0f
		    || memcmp(purchase_port.record.bytes, expected.bytes,
		    YT_RECORD_SIZE) != 0
		    || memcmp(purchase_port.record.bytes + YT_F89,
		    "\0\0\0\0", 4U) != 0)
			return false;

		memset(&buyer, 0, sizeof(buyer));
		memset(buyer.record.bytes, 0xc3, YT_RECORD_SIZE);
		buyer.credits = 20.0f;
		buyer.ports_owned = 1.0f;
		(void)yt_record_set_number(&buyer.record, YT_F81, 20.0f);
		(void)yt_record_set_number(&buyer.record, YT_F117, 1.0f);
		expected = buyer.record;
		(void)yt_record_set_number(&expected, YT_F81, 18.0f);
		(void)yt_record_set_number(&expected, YT_F117, 2.0f);
		if (!yt_port_purchase_buyer_overlay(&buyer, 2.0)
		    || buyer.credits != 18.0f || buyer.ports_owned != 2.0f
		    || memcmp(buyer.record.bytes, expected.bytes,
		    YT_RECORD_SIZE) != 0)
			return false;
		return yt_port_purchase_seller_record(2.75f) == 2
		    && yt_port_purchase_seller_record(16777216.0f) == 0
		    && yt_port_purchase_seller_record(-0.5f) == 16777215
		    && !yt_port_purchase_seller_overlay(NULL, 0.0f, 0.0)
		    && !yt_port_purchase_title_overlay(NULL, 2)
		    && !yt_port_purchase_buyer_overlay(NULL, 0.0);
	}
	return false;
}

static bool
check_planet_garrison_model(void)
{
	static const uint8_t expected_prompt[] =
	    "Drop how many ground force units on the planet? 18 Available ->";
	static const uint8_t expected_success[] =
	    "Ground force strength now at 12 units!";
	static const uint8_t dirty_zero[4] = {0x00, 0x00, 0x40, 0x00};
	struct yt_planet planet;
	struct yt_player player;
	struct yt_record planet_before;
	struct yt_record player_before;
	uint8_t row[128];
	size_t length;
	size_t index;

	if (!yt_planet_garrison_prompt(8.0f, 10.0f, row, sizeof(row),
	    &length) || length != sizeof(expected_prompt) - 1U
	    || memcmp(row, expected_prompt, length) != 0
	    || !yt_planet_garrison_success_row(12.0f, row, sizeof(row),
	    &length) || length != sizeof(expected_success) - 1U
	    || memcmp(row, expected_success, length) != 0
	    || yt_planet_garrison_after(8.0f, 12.0f, 10.0f) != 6.0f)
		return false;
	memset(&planet, 0, sizeof(planet));
	memset(&player, 0, sizeof(player));
	for (index = 0U; index < YT_RECORD_SIZE; ++index) {
		planet.record.bytes[index] = (uint8_t)(index ^ 0x3cU);
		player.record.bytes[index] = (uint8_t)(index ^ 0xc3U);
	}
	planet_before = planet.record;
	player_before = player.record;
	player.ground_forces = 8.0f;
	yt_planet_garrison_overlay(&planet, 12.0f, 0);
	if (memcmp(planet.record.bytes + YT_F73, dirty_zero, 4U) != 0
	    || yt_record_get_number(&planet.record, YT_F77) != 12.0f)
		return false;
	for (index = 0U; index < YT_RECORD_SIZE; ++index) {
		if ((index < YT_F73 || index >= YT_F73 + 4U)
		    && (index < YT_F77 || index >= YT_F77 + 4U)
		    && planet.record.bytes[index] != planet_before.bytes[index])
			return false;
	}
	yt_planet_garrison_overlay(&planet, 12.0f, 2);
	if (yt_record_get_number(&planet.record, YT_F73) != 2.0f)
		return false;
	yt_planet_garrison_player_overlay(&player, 6.75f);
	if (yt_record_get_number(&player.record, YT_F121) != 6.0f
	    || player.ground_forces != 8.0f)
		return false;
	for (index = 0U; index < YT_RECORD_SIZE; ++index) {
		if ((index < YT_F121 || index >= YT_F121 + 4U)
		    && player.record.bytes[index] != player_before.bytes[index])
			return false;
	}
	return true;
}

static bool
planet_permission_fixture_error(struct yt_error *error,
    const char *operation)
{
	if (error != NULL) {
		error->status = YT_IO_ERROR;
		error->system_error = 0;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
		error->path[0] = '\0';
	}
	return false;
}

enum planet_permission_event {
	PLANET_PERMISSION_UPDATE = 1,
	PLANET_PERMISSION_READ_PLANET,
	PLANET_PERMISSION_READ_PLAYER,
	PLANET_PERMISSION_BLANK,
	PLANET_PERMISSION_GOVERNOR,
	PLANET_PERMISSION_SOUND,
	PLANET_PERMISSION_WAIT,
	PLANET_PERMISSION_RANDOM,
	PLANET_PERMISSION_UNREST,
	PLANET_PERMISSION_REDUCTION,
	PLANET_PERMISSION_WRITE_PLANET,
	PLANET_PERMISSION_TRAFFIC,
	PLANET_PERMISSION_PREFIX,
	PLANET_PERMISSION_BLINK,
	PLANET_PERMISSION_FOREGROUND,
	PLANET_PERMISSION_DENIED,
};

struct planet_permission_tape {
	enum planet_permission_event events[24];
	size_t event_count;
	size_t calls;
	size_t fail_at;
	float updater_logical;
	uint32_t planet_read_record[2];
	struct yt_planet planet_source[2];
	size_t planet_read_count;
	int player_read_record[3];
	struct yt_player player_source[3];
	size_t player_read_count;
	float draws[2];
	size_t draw_count;
	float sounds[1];
	size_t sound_count;
	double waits[2];
	size_t wait_count;
	uint8_t presented[7][256];
	size_t presented_length[7];
	enum yt_planet_permission_output_kind presented_kind[7];
	size_t presented_count;
	struct yt_planet planet_written;
	uint32_t planet_write_record;
	bool planet_write_completed;
	float foreground[2];
	size_t foreground_count;
	float blink;
	size_t blink_count;
};

static bool
planet_permission_step(struct planet_permission_tape *tape,
    enum planet_permission_event event, bool fallible,
    const char *operation, struct yt_error *error)
{
	if (tape->event_count >= YT_ARRAY_LEN(tape->events))
		return planet_permission_fixture_error(error,
		    "permission event capacity");
	tape->events[tape->event_count++] = event;
	if (fallible && ++tape->calls == tape->fail_at)
		return planet_permission_fixture_error(error, operation);
	return true;
}

static bool
planet_permission_update_test(void *context, float logical_planet,
    struct yt_error *error)
{
	struct planet_permission_tape *tape = context;

	if (!planet_permission_step(tape, PLANET_PERMISSION_UPDATE, true,
	    "permission update", error))
		return false;
	tape->updater_logical = logical_planet;
	return true;
}

static bool
planet_permission_read_planet_test(void *context, uint32_t physical_record,
    struct yt_planet *planet, struct yt_error *error)
{
	struct planet_permission_tape *tape = context;
	size_t index = tape->planet_read_count;

	if (!planet_permission_step(tape, PLANET_PERMISSION_READ_PLANET, true,
	    "permission planet GET", error))
		return false;
	if (index >= YT_ARRAY_LEN(tape->planet_source))
		return planet_permission_fixture_error(error,
		    "permission planet source");
	tape->planet_read_record[index] = physical_record;
	*planet = tape->planet_source[index];
	++tape->planet_read_count;
	return true;
}

static bool
planet_permission_write_planet_test(void *context, uint32_t physical_record,
    struct yt_planet *planet, struct yt_error *error)
{
	struct planet_permission_tape *tape = context;

	if (!planet_permission_step(tape, PLANET_PERMISSION_WRITE_PLANET, true,
	    "permission planet PUT", error))
		return false;
	tape->planet_write_record = physical_record;
	tape->planet_written = *planet;
	tape->planet_write_completed = true;
	return true;
}

static bool
planet_permission_read_player_test(void *context, int physical_record,
    struct yt_player *player, struct yt_error *error)
{
	struct planet_permission_tape *tape = context;
	size_t index = tape->player_read_count;

	if (!planet_permission_step(tape, PLANET_PERMISSION_READ_PLAYER, true,
	    "permission player GET", error))
		return false;
	if (index >= YT_ARRAY_LEN(tape->player_source))
		return planet_permission_fixture_error(error,
		    "permission player source");
	tape->player_read_record[index] = physical_record;
	*player = tape->player_source[index];
	++tape->player_read_count;
	return true;
}

static enum planet_permission_event
planet_permission_present_event(const char *operation)
{
	if (strcmp(operation, "planet permission late blank") == 0)
		return PLANET_PERMISSION_BLANK;
	if (strcmp(operation, "vacant planet governor row") == 0)
		return PLANET_PERMISSION_GOVERNOR;
	if (strcmp(operation, "vacant planet unrest row") == 0)
		return PLANET_PERMISSION_UNREST;
	if (strcmp(operation, "vacant planet reduction row") == 0)
		return PLANET_PERMISSION_REDUCTION;
	if (strcmp(operation, "planet permission traffic row") == 0)
		return PLANET_PERMISSION_TRAFFIC;
	if (strcmp(operation, "planet permission prefix") == 0)
		return PLANET_PERMISSION_PREFIX;
	return PLANET_PERMISSION_DENIED;
}

static bool
planet_permission_present_test(void *context, const uint8_t *text,
    size_t length, enum yt_planet_permission_output_kind kind,
    const char *operation, struct yt_error *error)
{
	struct planet_permission_tape *tape = context;
	enum planet_permission_event event =
	    planet_permission_present_event(operation);
	size_t index = tape->presented_count;

	if (!planet_permission_step(tape, event, true, operation, error))
		return false;
	if (index >= YT_ARRAY_LEN(tape->presented)
	    || length > sizeof(tape->presented[0]))
		return planet_permission_fixture_error(error,
		    "permission presentation capacity");
	if (length != 0U)
		memcpy(tape->presented[index], text, length);
	tape->presented_length[index] = length;
	tape->presented_kind[index] = kind;
	++tape->presented_count;
	return true;
}

static bool
planet_permission_sound_test(void *context, float selector,
    const char *operation, struct yt_error *error)
{
	struct planet_permission_tape *tape = context;

	if (!planet_permission_step(tape, PLANET_PERMISSION_SOUND, true,
	    operation, error))
		return false;
	if (tape->sound_count >= YT_ARRAY_LEN(tape->sounds))
		return planet_permission_fixture_error(error,
		    "permission sound capacity");
	tape->sounds[tape->sound_count++] = selector;
	return true;
}

static bool
planet_permission_wait_test(void *context, double seconds,
    const char *operation, struct yt_error *error)
{
	struct planet_permission_tape *tape = context;

	if (!planet_permission_step(tape, PLANET_PERMISSION_WAIT, true,
	    operation, error))
		return false;
	if (tape->wait_count >= YT_ARRAY_LEN(tape->waits))
		return planet_permission_fixture_error(error,
		    "permission wait capacity");
	tape->waits[tape->wait_count++] = seconds;
	return true;
}

static bool
planet_permission_random_test(void *context, float *value,
    struct yt_error *error)
{
	struct planet_permission_tape *tape = context;
	size_t index = tape->draw_count;

	if (!planet_permission_step(tape, PLANET_PERMISSION_RANDOM, true,
	    "permission random", error))
		return false;
	if (index >= YT_ARRAY_LEN(tape->draws))
		return planet_permission_fixture_error(error,
		    "permission random capacity");
	*value = tape->draws[index];
	++tape->draw_count;
	return true;
}

static void
planet_permission_foreground_test(void *context, float foreground)
{
	struct planet_permission_tape *tape = context;

	(void)planet_permission_step(tape, PLANET_PERMISSION_FOREGROUND, false,
	    "permission foreground", NULL);
	if (tape->foreground_count < YT_ARRAY_LEN(tape->foreground))
		tape->foreground[tape->foreground_count++] = foreground;
}

static void
planet_permission_blink_test(void *context, float blink)
{
	struct planet_permission_tape *tape = context;

	(void)planet_permission_step(tape, PLANET_PERMISSION_BLINK, false,
	    "permission blink", NULL);
	tape->blink = blink;
	++tape->blink_count;
}

static void
planet_permission_vacancy_fixture(struct planet_permission_tape *tape,
    struct yt_planet_permission_state *state)
{
	size_t index;

	memset(tape, 0, sizeof(*tape));
	memset(state, 0, sizeof(*state));
	state->planet_record_value = 3056.60009765625f;
	state->planet_offset = 3055.0f;
	state->current_player_record = 7;
	state->last_player_record = 51;
	state->foreground = 6.0f;
	memset(&tape->planet_source[0], 0xa5,
	    sizeof(tape->planet_source[0]));
	memset(&tape->planet_source[1], 0x5a,
	    sizeof(tape->planet_source[1]));
	for (index = 0U; index < YT_RECORD_SIZE; ++index) {
		tape->planet_source[0].record.bytes[index] =
		    (uint8_t)(index ^ 0x96U);
		tape->planet_source[1].record.bytes[index] =
		    (uint8_t)(index ^ 0x69U);
	}
	tape->planet_source[0].record.bytes[0] = 'V';
	tape->planet_source[0].record.bytes[1] = 0U;
	tape->planet_source[0].record.bytes[2] = 'X';
	tape->planet_source[0].name_length = 3.0f;
	tape->planet_source[0].owner = 0.0f;
	tape->planet_source[0].ground_forces = 10.0f;
	tape->planet_source[1].name_length = 5.0f;
	tape->planet_source[1].owner = 44.0f;
	tape->planet_source[1].ground_forces = 80.0f;
	tape->draws[0] = 0.5f;
	tape->draws[1] = 0.5f;
}

static void
planet_permission_denial_fixture(struct planet_permission_tape *tape,
    struct yt_planet_permission_state *state)
{
	planet_permission_vacancy_fixture(tape, state);
	tape->planet_source[0].owner = 9.75f;
	tape->player_source[0].team = 4.0f;
	tape->player_source[1].team = 3.0f;
	tape->player_source[2].killed_by = 0.0f;
}

static size_t
planet_permission_failure_prefix(const enum planet_permission_event *events,
    size_t count, size_t failure)
{
	size_t index;
	size_t fallible = 0U;

	for (index = 0U; index < count; ++index) {
		if (events[index] != PLANET_PERMISSION_BLINK
		    && events[index] != PLANET_PERMISSION_FOREGROUND)
			++fallible;
		if (fallible == failure)
			return index + 1U;
	}
	return 0U;
}

static bool
check_planet_permission_transaction(void)
{
	static const struct yt_planet_permission_ops ops = {
		planet_permission_update_test,
		planet_permission_read_planet_test,
		planet_permission_write_planet_test,
		planet_permission_read_player_test,
		planet_permission_present_test,
		planet_permission_sound_test,
		planet_permission_wait_test,
		planet_permission_random_test,
		planet_permission_foreground_test,
		planet_permission_blink_test,
	};
	static const enum planet_permission_event vacancy_events[] = {
		PLANET_PERMISSION_UPDATE,
		PLANET_PERMISSION_READ_PLANET,
		PLANET_PERMISSION_BLANK,
		PLANET_PERMISSION_GOVERNOR,
		PLANET_PERMISSION_SOUND,
		PLANET_PERMISSION_WAIT,
		PLANET_PERMISSION_READ_PLANET,
		PLANET_PERMISSION_RANDOM,
		PLANET_PERMISSION_RANDOM,
		PLANET_PERMISSION_UNREST,
		PLANET_PERMISSION_REDUCTION,
		PLANET_PERMISSION_WRITE_PLANET,
		PLANET_PERMISSION_WAIT,
	};
	static const enum planet_permission_event denial_events[] = {
		PLANET_PERMISSION_UPDATE,
		PLANET_PERMISSION_READ_PLANET,
		PLANET_PERMISSION_READ_PLAYER,
		PLANET_PERMISSION_READ_PLAYER,
		PLANET_PERMISSION_BLANK,
		PLANET_PERMISSION_READ_PLAYER,
		PLANET_PERMISSION_TRAFFIC,
		PLANET_PERMISSION_PREFIX,
		PLANET_PERMISSION_BLINK,
		PLANET_PERMISSION_FOREGROUND,
		PLANET_PERMISSION_DENIED,
		PLANET_PERMISSION_FOREGROUND,
	};
	static const uint8_t governor[] =
	    "This planet has no governor! Hail to the new planetary governor!!";
	static const uint8_t unrest[] =
	    "Due to the unrest caused by the lack of planetary govornment, ";
	static const uint8_t reduction[] =
	    "ground forces have been reduced to 2 from 10!";
	static const uint8_t traffic[] =
	    "This is space traffic control at planet V\0X";
	static const uint8_t prefix[] = "Permission to land is ";
	static const uint8_t denied[] = "DENIED!";
	struct planet_permission_tape success;
	struct planet_permission_tape tape;
	struct yt_planet_permission_state state;
	struct yt_planet expected_planet;
	struct yt_error error;
	size_t failure;
	size_t prefix_length;

	planet_permission_vacancy_fixture(&success, &state);
	yt_error_clear(&error);
	if (!yt_planet_permission_run(&state, &ops, &success, &error)
	    || success.event_count != YT_ARRAY_LEN(vacancy_events)
	    || memcmp(success.events, vacancy_events,
	    sizeof(vacancy_events)) != 0 || success.calls != 13U
	    || state.physical_planet_record != 3056U
	    || state.updater_logical != 1.60009765625f
	    || success.updater_logical != 1.60009765625f
	    || !state.vacant || !state.allowed || state.denied || state.friendly
	    || state.cached_owner != 0.0f
	    || state.cached_ground_forces != 10.0f
	    || state.cached_name_length != 3U
	    || memcmp(state.cached_name, "V\0X", 3U) != 0
	    || state.draws[0] != 0.5f || state.draws[1] != 0.5f
	    || state.reduced_ground_forces != 2.0f
	    || success.planet_read_count != 2U
	    || success.planet_read_record[0] != 3056U
	    || success.planet_read_record[1] != 3056U
	    || success.player_read_count != 0U
	    || success.sound_count != 1U || success.sounds[0] != 1.0f
	    || success.wait_count != 2U || success.waits[0] != 2.0
	    || success.waits[1] != 5.0 || success.draw_count != 2U
	    || success.presented_count != 4U
	    || success.presented_length[0] != 0U
	    || success.presented_kind[0] != YT_PLANET_PERMISSION_LINE
	    || success.presented_length[1] != sizeof(governor) - 1U
	    || memcmp(success.presented[1], governor,
	    sizeof(governor) - 1U) != 0
	    || success.presented_length[2] != sizeof(unrest) - 1U
	    || memcmp(success.presented[2], unrest, sizeof(unrest) - 1U) != 0
	    || success.presented_length[3] != sizeof(reduction) - 1U
	    || memcmp(success.presented[3], reduction,
	    sizeof(reduction) - 1U) != 0
	    || !success.planet_write_completed
	    || success.planet_write_record != 3056U)
		return false;
	expected_planet = success.planet_source[1];
	yt_planet_landing_vacancy_overlay(&expected_planet, 2.0f, 7);
	if (memcmp(&state.planet, &expected_planet, sizeof(expected_planet)) != 0
	    || memcmp(&success.planet_written, &expected_planet,
	    sizeof(expected_planet)) != 0)
		return false;
	for (failure = 1U; failure <= success.calls; ++failure) {
		planet_permission_vacancy_fixture(&tape, &state);
		tape.fail_at = failure;
		yt_error_clear(&error);
		if (yt_planet_permission_run(&state, &ops, &tape, &error)
		    || error.status != YT_IO_ERROR || tape.calls != failure)
			return false;
		prefix_length = planet_permission_failure_prefix(vacancy_events,
		    YT_ARRAY_LEN(vacancy_events), failure);
		if (tape.event_count != prefix_length
		    || memcmp(tape.events, vacancy_events,
		    prefix_length * sizeof(vacancy_events[0])) != 0)
			return false;
		if (failure == 12U
		    && (state.planet.ground_forces != 2.0f
		    || state.planet.owner != 7.0f
		    || tape.planet_write_completed))
			return false;
	}

	planet_permission_denial_fixture(&success, &state);
	yt_error_clear(&error);
	if (!yt_planet_permission_run(&state, &ops, &success, &error)
	    || success.event_count != YT_ARRAY_LEN(denial_events)
	    || memcmp(success.events, denial_events, sizeof(denial_events)) != 0
	    || success.calls != 9U || state.allowed || !state.denied
	    || state.vacant || state.friendly || state.owner_record != 9
	    || success.player_read_count != 3U
	    || success.player_read_record[0] != 7
	    || success.player_read_record[1] != 9
	    || success.player_read_record[2] != 9
	    || success.presented_count != 4U
	    || success.presented_length[0] != 0U
	    || success.presented_length[1] != sizeof(traffic) - 1U
	    || memcmp(success.presented[1], traffic, sizeof(traffic) - 1U) != 0
	    || success.presented_length[2] != sizeof(prefix) - 1U
	    || memcmp(success.presented[2], prefix, sizeof(prefix) - 1U) != 0
	    || success.presented_kind[2] != YT_PLANET_PERMISSION_RAW
	    || success.presented_length[3] != sizeof(denied) - 1U
	    || memcmp(success.presented[3], denied, sizeof(denied) - 1U) != 0
	    || success.presented_kind[3] != YT_PLANET_PERMISSION_BOLD_LINE
	    || success.blink_count != 1U || success.blink != 1.0f
	    || success.foreground_count != 2U
	    || success.foreground[0] != 3.0f
	    || success.foreground[1] != 6.0f
	    || state.foreground != 6.0f || state.blink != 1.0f
	    || success.sound_count != 0U || success.wait_count != 0U
	    || success.draw_count != 0U || success.planet_write_completed)
		return false;
	for (failure = 1U; failure <= success.calls; ++failure) {
		planet_permission_denial_fixture(&tape, &state);
		tape.fail_at = failure;
		yt_error_clear(&error);
		if (yt_planet_permission_run(&state, &ops, &tape, &error)
		    || error.status != YT_IO_ERROR || tape.calls != failure)
			return false;
		prefix_length = planet_permission_failure_prefix(denial_events,
		    YT_ARRAY_LEN(denial_events), failure);
		if (tape.event_count != prefix_length
		    || memcmp(tape.events, denial_events,
		    prefix_length * sizeof(denial_events[0])) != 0)
			return false;
	}

	/* Immediate and teammate admission return without the late blank. */
	planet_permission_vacancy_fixture(&tape, &state);
	tape.planet_source[0].owner = 9.0f;
	tape.planet_source[0].ground_forces = 0.9f;
	if (!yt_planet_permission_run(&state, &ops, &tape, &error)
	    || !state.allowed || tape.calls != 2U || tape.presented_count != 0U)
		return false;
	planet_permission_denial_fixture(&tape, &state);
	tape.player_source[1].team = 4.0f;
	if (!yt_planet_permission_run(&state, &ops, &tape, &error)
	    || !state.allowed || !state.friendly || state.denied
	    || tape.calls != 4U || tape.presented_count != 0U
	    || tape.player_read_count != 2U)
		return false;

	/* A malformed cached name fails after the first planet snapshot. */
	planet_permission_denial_fixture(&tape, &state);
	tape.planet_source[0].name_length = -1.0f;
	yt_error_clear(&error);
	if (yt_planet_permission_run(&state, &ops, &tape, &error)
	    || error.status != YT_RANGE
	    || strcmp(error.operation, "planet name LEFT$ length") != 0
	    || tape.event_count != 2U || tape.calls != 2U
	    || tape.player_read_count != 0U || tape.presented_count != 0U)
		return false;

	/* Both RNG draws occur even when the first guarantees zero. */
	planet_permission_vacancy_fixture(&tape, &state);
	tape.draws[0] = 0.0f;
	tape.draws[1] = 0.999f;
	if (!yt_planet_permission_run(&state, &ops, &tape, &error)
	    || tape.draw_count != 2U || state.reduced_ground_forces != 0.0f
	    || state.planet.owner != 0.0f || state.planet.ground_forces != 0.0f)
		return false;
	return true;
}

static bool
check_planet_landing_model(void)
{
	static const uint8_t traffic[] =
	    "This is space traffic control at planet LOCKED";
	static const uint8_t sensor[] =
	    "Sensors report ground forces of 10 units. You have 5.";
	static const uint8_t amount[] =
	    "Use how many ground forces? You have 5. [0] ";
	static const uint8_t reduction[] =
	    "ground forces have been reduced to 2 from 10!";
	struct yt_planet planet;
	struct yt_record before;
	uint8_t row[160];
	uint32_t physical;
	float updater_logical;
	size_t length;
	size_t index;
	int owner_record;

	if (!yt_planet_landing_record(3055.0f, 12.0f, &physical,
	    &updater_logical) || physical != 3067U || updater_logical != 12.0f
	    || !yt_planet_landing_record(3055.0f, 1.6f, &physical,
	    &updater_logical) || physical != 3056U
	    || updater_logical != 1.60009765625f
	    || !yt_planet_landing_record(3055.0f, -3056.5f, &physical,
	    &updater_logical) || physical != UINT32_C(0x00fffffe)
	    || !yt_planet_landing_immediate_allow(0.9f, 9.0f, 7)
	    || !yt_planet_landing_immediate_allow(10.0f, 7.0f, 7)
	    || yt_planet_landing_immediate_allow(1.0f, 9.0f, 7)
	    || !yt_planet_landing_valid_owner(2.9f, 51, &owner_record)
	    || owner_record != 2
	    || yt_planet_landing_valid_owner(1.0f, 51, &owner_record)
	    || yt_planet_landing_valid_owner(52.0f, 51, &owner_record)
	    || !yt_planet_landing_vacant(0.0f, 0.0f, 51)
	    || !yt_planet_landing_vacant(9.0f, 1.0f, 51)
	    || yt_planet_landing_vacant(9.0f, 0.0f, 51)
	    || yt_planet_landing_vacant(-1.0f, 1.0f, 51)
	    || yt_planet_landing_attrition(0.5f, 0.5f, 10.0f) != 2.0f
	    || !yt_planet_landing_traffic_row((const uint8_t *)"LOCKED", 6U,
	    row, sizeof(row), &length) || length != sizeof(traffic) - 1U
	    || memcmp(row, traffic, length) != 0
	    || !yt_planet_landing_sensor_row(10.9f, 5.0f, row,
	    sizeof(row), &length) || length != sizeof(sensor) - 1U
	    || memcmp(row, sensor, length) != 0
	    || !yt_planet_landing_amount_prompt(5.0f, row, sizeof(row),
	    &length) || length != sizeof(amount) - 1U
	    || memcmp(row, amount, length) != 0
	    || !yt_planet_landing_unrest_row(2.0f, 10.0f, row,
	    sizeof(row), &length) || length != sizeof(reduction) - 1U
	    || memcmp(row, reduction, length) != 0
	    || yt_planet_landing_commitment("5.9") != 5.0f
	    || yt_planet_landing_commitment("") != 0.0f
	    || !yt_planet_landing_commitment_valid(5.0f, 5.0f)
	    || yt_planet_landing_commitment_valid(0.0f, 5.0f)
	    || yt_planet_landing_commitment_valid(6.0f, 5.0f))
		return false;
	memset(&planet, 0, sizeof(planet));
	for (index = 0U; index < YT_RECORD_SIZE; ++index)
		planet.record.bytes[index] = (uint8_t)(index ^ 0x96U);
	before = planet.record;
	yt_planet_landing_vacancy_overlay(&planet, 2.0f, 7);
	if (planet.ground_forces != 2.0f || planet.owner != 7.0f
	    || yt_record_get_number(&planet.record, YT_F77) != 2.0f
	    || yt_record_get_number(&planet.record, YT_F73) != 7.0f)
		return false;
	for (index = 0U; index < YT_RECORD_SIZE; ++index) {
		if ((index < YT_F73 || index >= YT_F73 + 4U)
		    && (index < YT_F77 || index >= YT_F77 + 4U)
		    && planet.record.bytes[index] != before.bytes[index])
			return false;
	}
	yt_planet_landing_vacancy_overlay(&planet, 0.0f, 7);
	return planet.owner == 0.0f
	    && yt_record_get_number(&planet.record, YT_F73) == 0.0f;
}

static bool
check_planet_assault_model(void)
{
	static const uint8_t attack_news[] =
	    " +++ STATIC PILOT attacked planet TEST WORLD with 5 ground forces!";
	static const uint8_t attacker_status[] =
	    "Your forces remaining  : 0!";
	static const uint8_t defender_status[] =
	    "Ground forces remaining: 0!";
	static const uint8_t capture_news[] =
	    " +++ STATIC PILOT captured planet TEST WORLD!";
	static const uint8_t failure_screen[] =
	    "Attack Failed! Ground Forces remaining: 5!";
	static const uint8_t failure_news[] =
	    " +++ Attack Failed! Ground Forces remaining: 5!";
	struct yt_player player;
	struct yt_planet planet;
	struct yt_record player_before;
	struct yt_record planet_before;
	uint8_t row[320];
	size_t length;
	size_t index;
	float attackers;
	float defenders;

	if (!yt_planet_assault_attack_news((const uint8_t *)"STATIC PILOT", 12U,
	    (const uint8_t *)"TEST WORLD", 10U, 5.0f, row, sizeof(row), &length)
	    || length != sizeof(attack_news) - 1U
	    || memcmp(row, attack_news, length) != 0
	    || !yt_planet_assault_status_row(true, 0.0f, row, sizeof(row),
	    &length) || length != sizeof(attacker_status) - 1U
	    || memcmp(row, attacker_status, length) != 0
	    || !yt_planet_assault_status_row(false, 0.0f, row, sizeof(row),
	    &length) || length != sizeof(defender_status) - 1U
	    || memcmp(row, defender_status, length) != 0
	    || !yt_planet_assault_capture_news((const uint8_t *)"STATIC PILOT",
	    12U, (const uint8_t *)"TEST WORLD", 10U, row, sizeof(row), &length)
	    || length != sizeof(capture_news) - 1U
	    || memcmp(row, capture_news, length) != 0
	    || !yt_planet_assault_failure_row(5.0f, false, row, sizeof(row),
	    &length) || length != sizeof(failure_screen) - 1U
	    || memcmp(row, failure_screen, length) != 0
	    || !yt_planet_assault_failure_row(5.0f, true, row, sizeof(row),
	    &length) || length != sizeof(failure_news) - 1U
	    || memcmp(row, failure_news, length) != 0)
		return false;
	attackers = 5.0f;
	defenders = 3.0f;
	yt_planet_assault_round(false, 0.999999f, &attackers, &defenders);
	if (attackers != 5.0f || defenders != 0.0f)
		return false;
	attackers = 1.0f;
	defenders = 5.0f;
	yt_planet_assault_round(true, 0.999999f, &attackers, &defenders);
	if (attackers != 0.0f || defenders != 5.0f)
		return false;
	yt_planet_assault_round(false, 0.0f, &attackers, &defenders);
	if (attackers != 0.0f || defenders != 5.0f)
		return false;
	memset(&player, 0, sizeof(player));
	memset(&planet, 0, sizeof(planet));
	for (index = 0U; index < YT_RECORD_SIZE; ++index) {
		player.record.bytes[index] = (uint8_t)(index ^ 0x5aU);
		planet.record.bytes[index] = (uint8_t)(index ^ 0xa5U);
	}
	player.ground_forces = 20.0f;
	player_before = player.record;
	yt_planet_assault_player_overlay(&player, 5.0f);
	if (player.ground_forces != 15.0f
	    || yt_record_get_number(&player.record, YT_F121) != 15.0f)
		return false;
	for (index = 0U; index < YT_RECORD_SIZE; ++index) {
		if ((index < YT_F121 || index >= YT_F121 + 4U)
		    && player.record.bytes[index] != player_before.bytes[index])
			return false;
	}
	planet_before = planet.record;
	yt_planet_assault_victory_overlay(&planet, 7.0f, 5.9f);
	if (planet.owner != 7.0f || planet.ground_forces != 5.0f
	    || yt_record_get_number(&planet.record, YT_F73) != 7.0f
	    || yt_record_get_number(&planet.record, YT_F77) != 5.0f)
		return false;
	for (index = 0U; index < YT_RECORD_SIZE; ++index) {
		if ((index < YT_F73 || index >= YT_F73 + 4U)
		    && (index < YT_F77 || index >= YT_F77 + 4U)
		    && planet.record.bytes[index] != planet_before.bytes[index])
			return false;
	}
	planet_before = planet.record;
	yt_planet_assault_failure_overlay(&planet, 3.9f);
	if (planet.owner != 7.0f || planet.ground_forces != 3.0f)
		return false;
	for (index = 0U; index < YT_RECORD_SIZE; ++index) {
		if ((index < YT_F77 || index >= YT_F77 + 4U)
		    && planet.record.bytes[index] != planet_before.bytes[index])
			return false;
	}
	return true;
}

static bool
check_planet_creation_model(void)
{
	static const uint8_t credit[] = "You have 30000 credits.";
	static const uint8_t news[] =
	    "  -  Captain Cache made a planet: New Terra";
	static const uint8_t success[] =
	    "Planet \"New Terra\" created with Genesis Device!";
	static const uint8_t dirty_zero[4] = {0x00, 0x00, 0x20, 0x00};
	struct yt_planet planet;
	struct yt_player player;
	struct yt_record before;
	uint8_t row[256];
	size_t length;
	size_t index;

	if (!yt_planet_creation_credit_row(30000.0, row, sizeof(row), &length)
	    || length != sizeof(credit) - 1U || memcmp(row, credit, length) != 0
	    || !yt_planet_creation_news((const uint8_t *)"Captain Cache", 13U,
	    (const uint8_t *)"New Terra", 9U, row, sizeof(row), &length)
	    || length != sizeof(news) - 1U || memcmp(row, news, length) != 0
	    || !yt_planet_creation_success_row((const uint8_t *)"New Terra", 9U,
	    row, sizeof(row), &length) || length != sizeof(success) - 1U
	    || memcmp(row, success, length) != 0)
		return false;
	memset(&planet, 0, sizeof(planet));
	for (index = 0U; index < YT_RECORD_SIZE; ++index)
		planet.record.bytes[index] = (uint8_t)(index ^ 0x69U);
	before = planet.record;
	yt_planet_creation_overlay(&planet, 2);
	if (yt_record_get_number(&planet.record, YT_F45) != 1.0f
	    || yt_record_get_number(&planet.record, YT_F49) != 1.0f
	    || yt_record_get_number(&planet.record, YT_F53) != 1.0f
	    || yt_record_get_number(&planet.record, YT_F57) != 10.0f
	    || yt_record_get_number(&planet.record, YT_F61) != 10.0f
	    || yt_record_get_number(&planet.record, YT_F65) != 10.0f
	    || memcmp(planet.record.bytes + YT_F69, dirty_zero, 4U) != 0
	    || yt_record_get_number(&planet.record, YT_F73) != 2.0f
	    || yt_record_get_number(&planet.record, YT_F77) != 1.0f
	    || yt_record_get_number(&planet.record, YT_F113) != 0.0f
	    || yt_record_get_number(&planet.record, YT_F117) != 0.0f
	    || memcmp(planet.record.bytes + YT_F125, dirty_zero, 4U) != 0
	    || yt_record_get_number(&planet.record, YT_F129) != 30.0f
	    || memcmp(planet.record.bytes + YT_RECORD_TAIL_OFFSET,
	    before.bytes + YT_RECORD_TAIL_OFFSET, YT_RECORD_TAIL_SIZE) != 0)
		return false;
	before = planet.record;
	yt_planet_creation_timestamp_overlay(&planet, 321.0f, 60.0f);
	if (yt_record_get_number(&planet.record, YT_F41) != 321.0f
	    || yt_record_get_number(&planet.record, YT_F89) != 60.0f)
		return false;
	for (index = 0U; index < YT_RECORD_SIZE; ++index) {
		if ((index < YT_F41 || index >= YT_F41 + 4U)
		    && (index < YT_F89 || index >= YT_F89 + 4U)
		    && planet.record.bytes[index] != before.bytes[index])
			return false;
	}
	memset(&player, 0, sizeof(player));
	for (index = 0U; index < YT_RECORD_SIZE; ++index)
		player.record.bytes[index] = (uint8_t)(index ^ 0xc3U);
	player.credits = 80000.75f;
	before = player.record;
	yt_planet_creation_credit_overlay(&player, -25000.0f);
	if (player.credits != 55000.0f
	    || yt_record_get_number(&player.record, YT_F81) != 55000.0f)
		return false;
	for (index = 0U; index < YT_RECORD_SIZE; ++index) {
		if ((index < YT_F81 || index >= YT_F81 + 4U)
		    && player.record.bytes[index] != before.bytes[index])
			return false;
	}
	return true;
}

static bool
check_planet_move_model(void)
{
	static const uint8_t heading[] =
	    "The shortest path from sector 1 to sector 3 is:";
	static const uint8_t summary[] =
	    "Distance is 2 and will take 20 turns.";
	static const uint8_t turns[] = "You have 100 turns left.";
	static const uint8_t explosion[] =
	    "The stress was too much! PLANET Gaia EXPLODED!";
	static const uint8_t explosion_news[] =
	    " *** Planet Gaia EXPLODED while being moved by Pilot!!!";
	static const uint8_t loss[] =
	    "You lost 4 fighters in the explosion!";
	static const uint8_t moved[] =
	    "Gaia moved! (Xannoron Movers, we move anyTHING, anyWHERE!)";
	struct yt_sector sector;
	struct yt_planet planet;
	struct yt_player player;
	struct yt_record before;
	uint8_t row[256];
	size_t length;
	size_t index;

	if (yt_planet_move_destination("3.9") != 3.0f
	    || yt_planet_move_destination("") != 0.0f
	    || yt_planet_move_maximum(53.0f, 51.0f) != 2.0f
	    || yt_planet_move_add_cost(10.0f) != 20.0f
	    || yt_planet_move_fighter_loss(10.0f, 0.5f, 0.5f) != 4.0f
	    || !yt_planet_move_path_heading(1.0f, 3.0f, row,
	    sizeof(row), &length) || length != sizeof(heading) - 1U
	    || memcmp(row, heading, length) != 0
	    || !yt_planet_move_summary(20.0f, row, sizeof(row), &length)
	    || length != sizeof(summary) - 1U
	    || memcmp(row, summary, length) != 0
	    || !yt_planet_move_turns_row(100.0f, row, sizeof(row), &length)
	    || length != sizeof(turns) - 1U || memcmp(row, turns, length) != 0
	    || !yt_planet_move_explosion_row((const uint8_t *)"Gaia", 4U,
	    row, sizeof(row), &length) || length != sizeof(explosion) - 1U
	    || memcmp(row, explosion, length) != 0
	    || !yt_planet_move_explosion_news((const uint8_t *)"Gaia", 4U,
	    (const uint8_t *)"Pilot", 5U, row, sizeof(row), &length)
	    || length != sizeof(explosion_news) - 1U
	    || memcmp(row, explosion_news, length) != 0
	    || !yt_planet_move_loss_row((const uint8_t *)"You", 3U, 4.0f,
	    row, sizeof(row), &length) || length != sizeof(loss) - 1U
	    || memcmp(row, loss, length) != 0
	    || !yt_planet_move_success_row((const uint8_t *)"Gaia", 4U,
	    row, sizeof(row), &length) || length != sizeof(moved) - 1U
	    || memcmp(row, moved, length) != 0)
		return false;
	memset(&sector, 0, sizeof(sector));
	memset(&planet, 0, sizeof(planet));
	memset(&player, 0, sizeof(player));
	for (index = 0U; index < YT_RECORD_SIZE; ++index) {
		sector.record.bytes[index] = (uint8_t)(index ^ 0x31U);
		planet.record.bytes[index] = (uint8_t)(index ^ 0x72U);
		player.record.bytes[index] = (uint8_t)(index ^ 0xb4U);
	}
	before = sector.record;
	yt_planet_move_sector_overlay(&sector, 7.0f);
	if (sector.planet != 7.0f
	    || yt_record_get_number(&sector.record, YT_F93) != 7.0f)
		return false;
	for (index = 0U; index < YT_RECORD_SIZE; ++index)
		if ((index < YT_F93 || index >= YT_F93 + 4U)
		    && sector.record.bytes[index] != before.bytes[index])
			return false;
	before = planet.record;
	yt_planet_move_explosion_overlay(&planet);
	if (planet.name_length != 0.0f || planet.name[0] != '\0'
	    || memcmp(planet.record.bytes, "\0\0\0\0", 4U) != 0)
		return false;
	for (index = 4U; index < YT_TEXT_FIELD_SIZE; ++index)
		if (planet.record.bytes[index] != ' ')
			return false;
	for (index = YT_TEXT_FIELD_SIZE; index < YT_RECORD_SIZE; ++index)
		if ((index < YT_F85 || index >= YT_F85 + 4U)
		    && planet.record.bytes[index] != before.bytes[index])
			return false;
	player.fighters = 10.0f;
	before = player.record;
	yt_planet_move_fighter_overlay(&player, 4.0f);
	if (player.fighters != 6.0f
	    || yt_record_get_number(&player.record, YT_F61) != 6.0f)
		return false;
	for (index = 0U; index < YT_RECORD_SIZE; ++index)
		if ((index < YT_F61 || index >= YT_F61 + 4U)
		    && player.record.bytes[index] != before.bytes[index])
			return false;
	player.turns = 100.0f;
	before = player.record;
	yt_planet_move_success_overlay(&player, 3.0f);
	if (player.turns != 90.0f || player.sector != 3.0f
	    || yt_record_get_number(&player.record, YT_F49) != 90.0f
	    || yt_record_get_number(&player.record, YT_F57) != 3.0f)
		return false;
	for (index = 0U; index < YT_RECORD_SIZE; ++index)
		if ((index < YT_F49 || index >= YT_F49 + 4U)
		    && (index < YT_F57 || index >= YT_F57 + 4U)
		    && player.record.bytes[index] != before.bytes[index])
			return false;
	return true;
}

struct sector_mine_draw_tape {
	float value;
	size_t calls;
	bool fail;
};

static bool
sector_mine_draw(void *context, float *value, struct yt_error *error)
{
	struct sector_mine_draw_tape *tape = context;

	++tape->calls;
	if (tape->fail) {
		if (error != NULL) {
			error->status = YT_IO_ERROR;
			(void)snprintf(error->operation,
			    sizeof(error->operation), "%s", "sector mine RND");
		}
		return false;
	}
	*value = tape->value;
	return true;
}

static bool
check_sector_mine_model(void)
{
	static const uint8_t explosion[] =
	    "There are 3 mines here! 1 EXPLODE!";
	static const uint8_t shields[] = "Shields down to 1 units!";
	static const uint8_t fighters[] = "Lost 1 fighters!";
	static const uint8_t cloak[] = "Lost 50% cloak!";
	static const uint8_t missiles[] = "Lost 1 Missiles!";
	static const uint8_t mines[] = "Lost 1 mines!";
	static const uint8_t ore[] = "Lost 1 holds of ore!";
	static const uint8_t organics[] = "Lost 1 holds of organics!";
	static const uint8_t equipment[] = "Lost 1 holds of equipment!";
	static const uint8_t empty[] = "Lost 1 empty holds!";
	static const uint8_t entry_news[] =
	    "P\0L hit sector mines in sector 42!";
	static const uint8_t final_news[] = "Shields reduced to 0 units!";
	static const uint8_t scanner_zero[4] = {0x00, 0x00, 0x48, 0x00};
	static const struct {
		enum yt_sector_mine_loss_kind kind;
		float value;
		const uint8_t *expected;
		size_t length;
	} losses[] = {
		{YT_SECTOR_MINE_LOSS_FIGHTERS, 1.0f, fighters,
		    sizeof(fighters) - 1U},
		{YT_SECTOR_MINE_LOSS_CLOAK, 50.0f, cloak,
		    sizeof(cloak) - 1U},
		{YT_SECTOR_MINE_LOSS_MISSILES, 1.0f, missiles,
		    sizeof(missiles) - 1U},
		{YT_SECTOR_MINE_LOSS_MINES, 1.0f, mines,
		    sizeof(mines) - 1U},
		{YT_SECTOR_MINE_LOSS_ORE, 1.0f, ore,
		    sizeof(ore) - 1U},
		{YT_SECTOR_MINE_LOSS_ORGANICS, 1.0f, organics,
		    sizeof(organics) - 1U},
		{YT_SECTOR_MINE_LOSS_EQUIPMENT, 1.0f, equipment,
		    sizeof(equipment) - 1U},
		{YT_SECTOR_MINE_LOSS_EMPTY_HOLDS, 1.0f, empty,
		    sizeof(empty) - 1U},
	};
	struct yt_sector sector;
	struct yt_player fresh;
	struct yt_player working;
	struct yt_record before;
	struct yt_record expected;
	struct sector_mine_draw_tape tape;
	struct yt_sector_mine_missile_result missile_result;
	struct yt_error error;
	uint8_t row[256];
	size_t length;
	size_t index;
	unsigned all_fields = YT_SECTOR_MINE_DAMAGE_SHIELDS
	    | YT_SECTOR_MINE_DAMAGE_FIGHTERS
	    | YT_SECTOR_MINE_DAMAGE_HOLDS
	    | YT_SECTOR_MINE_DAMAGE_ORE
	    | YT_SECTOR_MINE_DAMAGE_ORGANICS
	    | YT_SECTOR_MINE_DAMAGE_EQUIPMENT
	    | YT_SECTOR_MINE_DAMAGE_SCANNER
	    | YT_SECTOR_MINE_DAMAGE_MISSILES
	    | YT_SECTOR_MINE_DAMAGE_CLOAK
	    | YT_SECTOR_MINE_DAMAGE_CARRIED_MINES;

	if (yt_sector_mine_batch(19.0f) != 1.0f
	    || yt_sector_mine_batch(20.0f) != 2.0f
	    || yt_sector_mine_batch(0.5f) != 1.0f
	    || yt_sector_mine_shield_result(10.0f, 1.0f, 0.0f) != 10.0f
	    || yt_sector_mine_shield_result(1.0f, 1.0f, 0.999f) != 0.0f
	    || yt_sector_mine_cloak_loss(0.5f, 1.0f, 0.501f) != 0.5f
	    || yt_sector_mine_missile_loss(3.0f, 1.0f, 0.0f) != 1.0f
	    || !yt_sector_mine_explosion_row(3.0f, 1.0f, row,
	    sizeof(row), &length) || length != sizeof(explosion) - 1U
	    || memcmp(row, explosion, length) != 0
	    || !yt_sector_mine_shields_row(1.0f, row, sizeof(row), &length)
	    || length != sizeof(shields) - 1U
	    || memcmp(row, shields, length) != 0
	    || !yt_sector_mine_entry_news(entry_news, 3U, 42.0f, row,
	    sizeof(row), &length) || length != sizeof(entry_news) - 1U
	    || memcmp(row, entry_news, length) != 0
	    || !yt_sector_mine_final_news(0.0f, row, sizeof(row), &length)
	    || length != sizeof(final_news) - 1U
	    || memcmp(row, final_news, length) != 0)
		return false;
	tape = (struct sector_mine_draw_tape){0.5f, 0U, false};
	missile_result = (struct yt_sector_mine_missile_result){-1.0f, -1.0f,
	    true};
	if (!yt_sector_mine_missile_step(0.0f, 1.0f, sector_mine_draw,
	    &tape, &missile_result, NULL) || tape.calls != 0U
	    || missile_result.applied || missile_result.loss != 0.0f
	    || missile_result.remaining != 0.0f)
		return false;
	missile_result = (struct yt_sector_mine_missile_result){0};
	if (!yt_sector_mine_missile_step(3.0f, 1.0f, sector_mine_draw,
	    &tape, &missile_result, NULL) || tape.calls != 1U
	    || !missile_result.applied || missile_result.loss != 2.0f
	    || missile_result.remaining != 1.0f)
		return false;
	tape = (struct sector_mine_draw_tape){0.9f, 0U, false};
	if (!yt_sector_mine_missile_step(3.0f, 2.0f, sector_mine_draw,
	    &tape, &missile_result, NULL) || tape.calls != 1U
	    || missile_result.loss != 3.0f || missile_result.remaining != 0.0f)
		return false;
	tape = (struct sector_mine_draw_tape){0.0f, 0U, true};
	missile_result = (struct yt_sector_mine_missile_result){13.0f, 17.0f,
	    true};
	yt_error_clear(&error);
	if (yt_sector_mine_missile_step(3.0f, 1.0f, sector_mine_draw,
	    &tape, &missile_result, &error) || tape.calls != 1U
	    || missile_result.remaining != 13.0f
	    || missile_result.loss != 17.0f || !missile_result.applied
	    || error.status != YT_IO_ERROR
	    || strcmp(error.operation, "sector mine RND") != 0)
		return false;
	for (index = 0U; index < YT_ARRAY_LEN(losses); ++index)
		if (!yt_sector_mine_loss_row(losses[index].kind,
		    losses[index].value, row, sizeof(row), &length)
		    || length != losses[index].length
		    || memcmp(row, losses[index].expected, length) != 0)
			return false;
	memset(&sector, 0, sizeof(sector));
	memset(&fresh, 0, sizeof(fresh));
	memset(&working, 0, sizeof(working));
	for (index = 0U; index < YT_RECORD_SIZE; ++index) {
		sector.record.bytes[index] = (uint8_t)(index ^ 0x5aU);
		fresh.record.bytes[index] = (uint8_t)(index ^ 0xa5U);
	}
	before = sector.record;
	yt_sector_mine_sector_overlay(&sector, -0.5f);
	if (sector.mines != -0.5f
	    || yt_record_get_number(&sector.record, YT_F129) != -0.5f)
		return false;
	for (index = 0U; index < YT_RECORD_SIZE; ++index)
		if ((index < YT_F129 || index >= YT_F129 + 4U)
		    && sector.record.bytes[index] != before.bytes[index])
			return false;
	working.shields = 1.0f;
	working.fighters = 2.0f;
	working.holds = 3.0f;
	working.ore = 4.0f;
	working.organics = 5.0f;
	working.equipment = 6.0f;
	working.danger_scanner = 0.0f;
	working.missiles = 7.0f;
	working.cloak = 0.5f;
	working.mines = 8.0f;
	expected = fresh.record;
	if (!yt_record_set_number(&expected, YT_F53, working.shields)
	    || !yt_record_set_number(&expected, YT_F61, working.fighters)
	    || !yt_record_set_number(&expected, YT_F65, working.holds)
	    || !yt_record_set_number(&expected, YT_F69, working.ore)
	    || !yt_record_set_number(&expected, YT_F73, working.organics)
	    || !yt_record_set_number(&expected, YT_F77, working.equipment)
	    || !yt_record_set_raw_number(&expected, YT_F93, scanner_zero)
	    || !yt_record_set_number(&expected, YT_F97, working.missiles)
	    || !yt_record_set_number(&expected, YT_F125, working.cloak)
	    || !yt_record_set_number(&expected, YT_F129, working.mines))
		return false;
	yt_sector_mine_player_overlay(&fresh, &working, all_fields);
	if (memcmp(fresh.record.bytes, expected.bytes, YT_RECORD_SIZE) != 0
	    || fresh.shields != working.shields
	    || fresh.fighters != working.fighters
	    || fresh.holds != working.holds || fresh.ore != working.ore
	    || fresh.organics != working.organics
	    || fresh.equipment != working.equipment
	    || fresh.danger_scanner != 0.0f
	    || fresh.missiles != working.missiles
	    || fresh.cloak != working.cloak || fresh.mines != working.mines)
		return false;
	working.holds = 9.0f;
	working.equipment = 1.0f;
	working.organics = 2.0f;
	working.ore = 3.0f;
	return yt_sector_mine_empty_holds(&working) == 3.0f;
}

enum mine_transaction_event {
	MINE_TX_PRESENT = 1,
	MINE_TX_SOUND,
	MINE_TX_READ_CURRENT,
	MINE_TX_NEWS,
	MINE_TX_READ_SECTOR,
	MINE_TX_WRITE_SECTOR,
	MINE_TX_RANDOM,
	MINE_TX_READ_PLAYER,
	MINE_TX_WRITE_PLAYER,
	MINE_TX_SHRINK,
	MINE_TX_WARP,
};

struct mine_transaction_tape {
	const enum mine_transaction_event *expected;
	size_t expected_count;
	size_t event_count;
	size_t fail_at;
	struct yt_player player;
	struct yt_sector sector;
	float draws[16];
	size_t draw_position;
	float shrink_result;
	size_t present_count;
	size_t news_count;
	size_t sound_count;
	size_t sector_writes;
	size_t player_writes;
	bool warped;
	float foreground;
	float background;
	float blink;
	int pager_foreground;
};

static bool
mine_tx_step(struct mine_transaction_tape *tape,
	enum mine_transaction_event event, struct yt_error *error)
{
	if (tape->event_count >= tape->expected_count
	    || tape->expected[tape->event_count] != event)
		return false;
	++tape->event_count;
	if (tape->event_count != tape->fail_at)
		return true;
	if (error != NULL) {
		error->status = YT_IO_ERROR;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    "mine transaction injected failure");
	}
	return false;
}

static bool
mine_tx_read_current(void *context, struct yt_player *player,
	struct yt_error *error)
{
	struct mine_transaction_tape *tape = context;

	if (!mine_tx_step(tape, MINE_TX_READ_CURRENT, error))
		return false;
	*player = tape->player;
	return true;
}

static bool
mine_tx_read_player(void *context, int player_record,
	struct yt_player *player, struct yt_error *error)
{
	struct mine_transaction_tape *tape = context;

	if (player_record != 2
	    || !mine_tx_step(tape, MINE_TX_READ_PLAYER, error))
		return false;
	*player = tape->player;
	return true;
}

static bool
mine_tx_write_player(void *context, int player_record,
	struct yt_player *player, struct yt_error *error)
{
	struct mine_transaction_tape *tape = context;

	if (player_record != 2
	    || !mine_tx_step(tape, MINE_TX_WRITE_PLAYER, error))
		return false;
	tape->player = *player;
	++tape->player_writes;
	return true;
}

static bool
mine_tx_read_sector(void *context, int logical_sector,
	struct yt_sector *sector, struct yt_error *error)
{
	struct mine_transaction_tape *tape = context;

	if (logical_sector != 42
	    || !mine_tx_step(tape, MINE_TX_READ_SECTOR, error))
		return false;
	*sector = tape->sector;
	return true;
}

static bool
mine_tx_write_sector(void *context, int logical_sector,
	struct yt_sector *sector, struct yt_error *error)
{
	struct mine_transaction_tape *tape = context;

	if (logical_sector != 42
	    || !mine_tx_step(tape, MINE_TX_WRITE_SECTOR, error))
		return false;
	tape->sector = *sector;
	++tape->sector_writes;
	return true;
}

static bool
mine_tx_present(void *context, const uint8_t *text, size_t length,
	enum yt_sector_mine_output_kind kind, struct yt_error *error)
{
	struct mine_transaction_tape *tape = context;

	(void)text;
	(void)length;
	(void)kind;
	if (!mine_tx_step(tape, MINE_TX_PRESENT, error))
		return false;
	++tape->present_count;
	return true;
}

static bool
mine_tx_sound(void *context, float selector, struct yt_error *error)
{
	struct mine_transaction_tape *tape = context;

	if ((selector != 5.0f && selector != 2.0f)
	    || !mine_tx_step(tape, MINE_TX_SOUND, error))
		return false;
	++tape->sound_count;
	return true;
}

static bool
mine_tx_news(void *context, const uint8_t *text, size_t length,
	struct yt_error *error)
{
	struct mine_transaction_tape *tape = context;

	(void)text;
	(void)length;
	if (!mine_tx_step(tape, MINE_TX_NEWS, error))
		return false;
	++tape->news_count;
	return true;
}

static bool
mine_tx_random(void *context, float *value, struct yt_error *error)
{
	struct mine_transaction_tape *tape = context;

	if (tape->draw_position >= YT_ARRAY_LEN(tape->draws)
	    || !mine_tx_step(tape, MINE_TX_RANDOM, error))
		return false;
	*value = tape->draws[tape->draw_position++];
	return true;
}

static bool
mine_tx_shrink(void *context, float range, float *result,
	struct yt_error *error)
{
	struct mine_transaction_tape *tape = context;

	(void)range;
	if (!mine_tx_step(tape, MINE_TX_SHRINK, error))
		return false;
	*result = tape->shrink_result;
	return true;
}

static bool
mine_tx_warp(void *context, struct yt_error *error)
{
	struct mine_transaction_tape *tape = context;

	if (!mine_tx_step(tape, MINE_TX_WARP, error))
		return false;
	tape->warped = true;
	return true;
}

static void
mine_tx_set_current(void *context, const struct yt_player *player)
{
	struct mine_transaction_tape *tape = context;

	tape->player = *player;
}

static void
mine_tx_style(void *context, float foreground, float background,
	float blink, int pager_foreground)
{
	struct mine_transaction_tape *tape = context;

	tape->foreground = foreground;
	tape->background = background;
	tape->blink = blink;
	tape->pager_foreground = pager_foreground;
}

static const struct yt_sector_mine_ops mine_tx_ops = {
	mine_tx_read_current,
	mine_tx_read_player,
	mine_tx_write_player,
	mine_tx_read_sector,
	mine_tx_write_sector,
	mine_tx_present,
	mine_tx_sound,
	mine_tx_news,
	mine_tx_random,
	mine_tx_shrink,
	mine_tx_warp,
	mine_tx_set_current,
	mine_tx_style,
};

static void
mine_tx_fixture(struct mine_transaction_tape *tape,
	const enum mine_transaction_event *expected, size_t expected_count,
	float emergency_draw)
{
	struct yt_record raw;

	memset(tape, 0, sizeof(*tape));
	tape->expected = expected;
	tape->expected_count = expected_count;
	yt_record_blank(&raw);
	memcpy(raw.bytes, "P\0L", 3U);
	(void)yt_record_set_number(&raw, YT_F53, 10.0f);
	(void)yt_record_set_number(&raw, YT_F65, 5.0f);
	(void)yt_record_set_number(&raw, YT_F85, 3.0f);
	yt_player_decode(&tape->player, &raw);
	yt_record_blank(&raw);
	(void)yt_record_set_number(&raw, YT_F129, 1.0f);
	yt_sector_decode(&tape->sector, &raw);
	tape->draws[0] = 0.0f;
	tape->draws[1] = 0.0f;
	tape->draws[2] = emergency_draw;
}

static bool
check_sector_mine_transaction(void)
{
	static const enum mine_transaction_event returned[] = {
		MINE_TX_PRESENT, MINE_TX_PRESENT, MINE_TX_SOUND,
		MINE_TX_READ_CURRENT, MINE_TX_NEWS, MINE_TX_READ_SECTOR,
		MINE_TX_WRITE_SECTOR, MINE_TX_PRESENT, MINE_TX_PRESENT,
		MINE_TX_READ_CURRENT, MINE_TX_RANDOM, MINE_TX_PRESENT,
		MINE_TX_RANDOM, MINE_TX_READ_PLAYER, MINE_TX_WRITE_PLAYER,
		MINE_TX_SOUND, MINE_TX_RANDOM, MINE_TX_NEWS,
		MINE_TX_READ_SECTOR,
	};
	static const enum mine_transaction_event warped[] = {
		MINE_TX_PRESENT, MINE_TX_PRESENT, MINE_TX_SOUND,
		MINE_TX_READ_CURRENT, MINE_TX_NEWS, MINE_TX_READ_SECTOR,
		MINE_TX_WRITE_SECTOR, MINE_TX_PRESENT, MINE_TX_PRESENT,
		MINE_TX_READ_CURRENT, MINE_TX_RANDOM, MINE_TX_PRESENT,
		MINE_TX_RANDOM, MINE_TX_READ_PLAYER, MINE_TX_WRITE_PLAYER,
		MINE_TX_SOUND, MINE_TX_RANDOM, MINE_TX_WARP,
	};
	static const enum mine_transaction_event repeated[] = {
		MINE_TX_PRESENT, MINE_TX_PRESENT, MINE_TX_SOUND,
		MINE_TX_READ_CURRENT, MINE_TX_NEWS,
		MINE_TX_READ_SECTOR, MINE_TX_WRITE_SECTOR,
		MINE_TX_PRESENT, MINE_TX_PRESENT, MINE_TX_READ_CURRENT,
		MINE_TX_RANDOM, MINE_TX_PRESENT, MINE_TX_RANDOM,
		MINE_TX_READ_PLAYER, MINE_TX_WRITE_PLAYER, MINE_TX_SOUND,
		MINE_TX_RANDOM,
		MINE_TX_READ_SECTOR, MINE_TX_WRITE_SECTOR,
		MINE_TX_PRESENT, MINE_TX_PRESENT, MINE_TX_READ_CURRENT,
		MINE_TX_RANDOM, MINE_TX_PRESENT, MINE_TX_RANDOM,
		MINE_TX_READ_PLAYER, MINE_TX_WRITE_PLAYER, MINE_TX_SOUND,
		MINE_TX_RANDOM, MINE_TX_NEWS, MINE_TX_READ_SECTOR,
	};
	static const enum mine_transaction_event fatal[] = {
		MINE_TX_PRESENT, MINE_TX_PRESENT, MINE_TX_SOUND,
		MINE_TX_READ_CURRENT, MINE_TX_NEWS, MINE_TX_READ_SECTOR,
		MINE_TX_WRITE_SECTOR, MINE_TX_PRESENT, MINE_TX_PRESENT,
		MINE_TX_READ_CURRENT, MINE_TX_SHRINK, MINE_TX_PRESENT,
		MINE_TX_READ_PLAYER, MINE_TX_WRITE_PLAYER, MINE_TX_SOUND,
		MINE_TX_RANDOM, MINE_TX_NEWS, MINE_TX_READ_SECTOR,
	};
	struct mine_transaction_tape tape;
	struct yt_sector_mine_state state;
	struct yt_error error;
	bool destroyed;
	size_t failure;

	mine_tx_fixture(&tape, returned, YT_ARRAY_LEN(returned), 0.0f);
	destroyed = false;
	state = (struct yt_sector_mine_state){
		.current_player_record = 2,
		.current_sector = 42.0f,
		.foreground = 6.0f,
		.pager_foreground = 6,
		.destroyed = &destroyed,
	};
	if (!yt_sector_mine_run(&state, &mine_tx_ops, &tape, NULL)
	    || tape.event_count != YT_ARRAY_LEN(returned)
	    || tape.draw_position != 3U || tape.sector_writes != 1U
	    || tape.player_writes != 1U || tape.news_count != 2U
	    || tape.sound_count != 2U || tape.present_count != 5U
	    || tape.sector.mines != 0.0f || tape.player.shields != 10.0f
	    || state.batches != 1U || state.terminal || !state.complete
	    || destroyed || tape.foreground != 3.0f
	    || tape.background != 1.0f || tape.blink != 0.0f)
		return false;

	for (failure = 1U; failure <= YT_ARRAY_LEN(returned); ++failure) {
		mine_tx_fixture(&tape, returned, YT_ARRAY_LEN(returned), 0.0f);
		tape.fail_at = failure;
		destroyed = false;
		state = (struct yt_sector_mine_state){
			.current_player_record = 2,
			.current_sector = 42.0f,
			.foreground = 6.0f,
			.pager_foreground = 6,
			.destroyed = &destroyed,
		};
		yt_error_clear(&error);
		if (yt_sector_mine_run(&state, &mine_tx_ops, &tape, &error)
		    || tape.event_count != failure || state.complete
		    || error.status != YT_IO_ERROR)
			return false;
	}

	mine_tx_fixture(&tape, repeated, YT_ARRAY_LEN(repeated), 0.0f);
	tape.sector.mines = 2.0f;
	destroyed = false;
	state = (struct yt_sector_mine_state){
		.current_player_record = 2,
		.current_sector = 42.0f,
		.foreground = 6.0f,
		.pager_foreground = 6,
		.destroyed = &destroyed,
	};
	if (!yt_sector_mine_run(&state, &mine_tx_ops, &tape, NULL)
	    || tape.event_count != YT_ARRAY_LEN(repeated) || state.batches != 2U
	    || tape.sector_writes != 2U || tape.player_writes != 2U
	    || tape.draw_position != 6U || tape.sector.mines != 0.0f
	    || destroyed || state.terminal || !state.complete)
		return false;

	mine_tx_fixture(&tape, fatal, YT_ARRAY_LEN(fatal), 0.0f);
	tape.player.shields = 0.0f;
	tape.player.holds = 1.0f;
	tape.shrink_result = 1.0f;
	destroyed = false;
	state = (struct yt_sector_mine_state){
		.current_player_record = 2,
		.current_sector = 42.0f,
		.foreground = 6.0f,
		.pager_foreground = 6,
		.destroyed = &destroyed,
	};
	if (!yt_sector_mine_run(&state, &mine_tx_ops, &tape, NULL)
	    || tape.event_count != YT_ARRAY_LEN(fatal) || !destroyed
	    || tape.player.holds != 0.0f || state.terminal || !state.complete
	    || tape.draw_position != 1U)
		return false;

	mine_tx_fixture(&tape, warped, YT_ARRAY_LEN(warped), 0.9f);
	destroyed = false;
	state = (struct yt_sector_mine_state){
		.current_player_record = 2,
		.current_sector = 42.0f,
		.foreground = 6.0f,
		.pager_foreground = 6,
		.destroyed = &destroyed,
	};
	return yt_sector_mine_run(&state, &mine_tx_ops, &tape, NULL)
	    && tape.event_count == YT_ARRAY_LEN(warped) && tape.warped
	    && state.terminal && state.complete && tape.news_count == 1U;
}

static bool
check_direct_fighter_kill_model(void)
{
	static const uint8_t name[] = {'V', 0, 'X'};
	static const uint8_t expected[] =
	    "  -  V\0X had sector mines! They EXPLODED!";
	uint8_t row[128];
	size_t length;

	return yt_direct_fighter_mine_warning(name, sizeof(name), row,
	    sizeof(row), &length)
	    && length == sizeof(expected) - 1U
	    && memcmp(row, expected, length) == 0;
}

enum common_fatal_event {
	COMMON_FATAL_SET_FOREGROUND,
	COMMON_FATAL_PRESENT,
	COMMON_FATAL_READ_PLAYER,
	COMMON_FATAL_SOUND,
	COMMON_FATAL_DEATH,
	COMMON_FATAL_WAIT,
	COMMON_FATAL_EVENT_COUNT,
};

enum common_fatal_field {
	COMMON_FATAL_FIELD_ENTRY,
	COMMON_FATAL_FIELD_CURRENT,
	COMMON_FATAL_FIELD_DEATH,
};

struct common_fatal_tape {
	enum common_fatal_event events[COMMON_FATAL_EVENT_COUNT];
	size_t event_count;
	int fail_event;
	struct yt_player player;
	float foreground;
	int pager_foreground;
	enum common_fatal_field field;
	bool notice_visible;
	bool sound_complete;
	bool death_durable;
	bool news_durable;
	bool wait_complete;
	float wait_duration;
	size_t rng_position;
};

static bool
common_fatal_test_step(struct common_fatal_tape *tape,
    enum common_fatal_event event, struct yt_error *error)
{
	if (tape->event_count >= YT_ARRAY_LEN(tape->events)
	    || event != (enum common_fatal_event)tape->event_count)
		return false;
	tape->events[tape->event_count++] = event;
	if ((int)event != tape->fail_event)
		return true;
	if (error != NULL) {
		error->status = YT_IO_ERROR;
		(void)snprintf(error->operation, sizeof(error->operation),
		    "common fatal event %u", (unsigned)event);
	}
	return false;
}

static void
common_fatal_test_set_foreground(void *context, float foreground,
    int pager_foreground)
{
	struct common_fatal_tape *tape = context;

	if (!common_fatal_test_step(tape, COMMON_FATAL_SET_FOREGROUND, NULL))
		return;
	tape->foreground = foreground;
	tape->pager_foreground = pager_foreground;
}

static bool
common_fatal_test_present(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	static const uint8_t expected[] = "Your ship has been destroyed!";
	struct common_fatal_tape *tape = context;

	if (length != sizeof(expected) - 1U
	    || memcmp(text, expected, length) != 0
	    || !common_fatal_test_step(tape, COMMON_FATAL_PRESENT, error))
		return false;
	tape->notice_visible = true;
	return true;
}

static bool
common_fatal_test_read_player(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct common_fatal_tape *tape = context;

	if (player_record != 2 || !common_fatal_test_step(tape,
	    COMMON_FATAL_READ_PLAYER, error))
		return false;
	*player = tape->player;
	tape->field = COMMON_FATAL_FIELD_CURRENT;
	return true;
}

static bool
common_fatal_test_sound(void *context, struct yt_error *error)
{
	struct common_fatal_tape *tape = context;

	if (!common_fatal_test_step(tape, COMMON_FATAL_SOUND, error))
		return false;
	tape->sound_complete = true;
	return true;
}

static bool
common_fatal_test_death(void *context, int victim_record, float killer,
    struct yt_error *error)
{
	struct common_fatal_tape *tape = context;

	if (victim_record != 2 || killer != 2.0f
	    || !common_fatal_test_step(tape, COMMON_FATAL_DEATH, error))
		return false;
	tape->field = COMMON_FATAL_FIELD_DEATH;
	tape->death_durable = true;
	tape->news_durable = true;
	return true;
}

static bool
common_fatal_test_wait(void *context, float duration,
    struct yt_error *error)
{
	struct common_fatal_tape *tape = context;

	tape->wait_duration = duration;
	if (!common_fatal_test_step(tape, COMMON_FATAL_WAIT, error))
		return false;
	tape->wait_complete = true;
	return true;
}

static const struct yt_common_fatal_ops common_fatal_test_ops = {
	common_fatal_test_set_foreground,
	common_fatal_test_present,
	common_fatal_test_read_player,
	common_fatal_test_sound,
	common_fatal_test_death,
	common_fatal_test_wait,
};

static void
common_fatal_test_reset(struct common_fatal_tape *tape,
    const struct yt_player *player, int fail_event)
{
	memset(tape, 0, sizeof(*tape));
	tape->player = *player;
	tape->fail_event = fail_event;
	tape->foreground = 7.0f;
	tape->pager_foreground = 7;
	tape->field = COMMON_FATAL_FIELD_ENTRY;
	tape->rng_position = 13U;
}

static struct yt_common_fatal_state
common_fatal_test_state(const struct yt_player *entry)
{
	struct yt_common_fatal_state state = {
		.current_player_record = 2,
		.foreground = 7.0f,
		.pager_foreground = 7,
		.target_record = 99.0f,
		.field_player = *entry,
		.field_valid = true,
	};

	return state;
}

static bool
check_common_fatal_transaction(void)
{
	struct common_fatal_tape tape;
	struct yt_common_fatal_state state;
	struct yt_player player;
	struct yt_player entry;
	struct yt_record raw;
	struct yt_record entry_raw;
	struct yt_error error;
	size_t index;

	for (index = 0U; index < YT_RECORD_SIZE; ++index)
		raw.bytes[index] = (uint8_t)(index ^ 0xc3U);
	for (index = 0U; index < YT_RECORD_SIZE; ++index)
		entry_raw.bytes[index] = (uint8_t)(index ^ 0x3cU);
	yt_player_decode(&player, &raw);
	yt_player_decode(&entry, &entry_raw);
	common_fatal_test_reset(&tape, &player, -1);
	state = common_fatal_test_state(&entry);
	if (!yt_common_fatal_run(&state, &common_fatal_test_ops, &tape, NULL)
	    || tape.event_count != COMMON_FATAL_EVENT_COUNT
	    || tape.foreground != 3.0f || tape.pager_foreground != 3
	    || !tape.notice_visible || !tape.sound_complete
	    || !tape.death_durable || !tape.news_durable || !tape.wait_complete
	    || tape.wait_duration != 5.0f || tape.rng_position != 13U
	    || tape.field != COMMON_FATAL_FIELD_DEATH
	    || state.foreground != 3.0f || state.pager_foreground != 3
	    || state.target_record != 2.0f || !state.field_valid
	    || memcmp(state.field_player.record.bytes, raw.bytes,
	    YT_RECORD_SIZE) != 0 || !state.wait_complete || !state.normal_exit)
		return false;

	for (index = COMMON_FATAL_PRESENT; index <= COMMON_FATAL_WAIT; ++index) {
		common_fatal_test_reset(&tape, &player, (int)index);
		state = common_fatal_test_state(&entry);
		yt_error_clear(&error);
		if (yt_common_fatal_run(&state, &common_fatal_test_ops, &tape,
		    &error) || tape.event_count != index + 1U
		    || error.status != YT_IO_ERROR
		    || tape.notice_visible != (index > COMMON_FATAL_PRESENT)
		    || tape.sound_complete != (index > COMMON_FATAL_SOUND)
		    || tape.death_durable != (index > COMMON_FATAL_DEATH)
		    || tape.news_durable != (index > COMMON_FATAL_DEATH)
		    || tape.wait_complete || tape.rng_position != 13U
		    || !state.field_valid
		    || memcmp(state.field_player.record.bytes,
		    index > COMMON_FATAL_READ_PLAYER ? raw.bytes : entry_raw.bytes,
		    YT_RECORD_SIZE) != 0
		    || state.target_record != (index > COMMON_FATAL_READ_PLAYER
		    ? 2.0f : 99.0f)
		    || state.wait_complete || state.normal_exit)
			return false;
	}
	return true;
}

enum direct_fighter_kill_event {
	DIRECT_KILL_SOUND,
	DIRECT_KILL_READ_PLAYER,
	DIRECT_KILL_NAME_LENGTH,
	DIRECT_KILL_DEATH,
	DIRECT_KILL_SALVAGE,
	DIRECT_KILL_READ_SECTOR,
	DIRECT_KILL_WRITE_SECTOR,
	DIRECT_KILL_PRESENT,
	DIRECT_KILL_NEWS,
	DIRECT_KILL_MINE,
	DIRECT_KILL_FATAL,
	DIRECT_KILL_EVENT_COUNT,
};

enum direct_fighter_field {
	DIRECT_KILL_FIELD_ENTRY,
	DIRECT_KILL_FIELD_TARGET,
	DIRECT_KILL_FIELD_CURRENT,
	DIRECT_KILL_FIELD_SECTOR,
};

struct direct_fighter_kill_tape {
	enum direct_fighter_kill_event events[DIRECT_KILL_EVENT_COUNT];
	size_t event_count;
	size_t fail_at;
	struct yt_player target;
	struct yt_sector sector;
	struct yt_sector staged_sector;
	struct yt_sector durable_sector;
	bool staged;
	bool durable;
	bool news_durable;
	bool mine_terminal;
	uint8_t mine_destroyed_raw[4];
	enum direct_fighter_field field;
	size_t rng_position;
};

static bool
direct_fighter_kill_step(struct direct_fighter_kill_tape *tape,
    enum direct_fighter_kill_event event, struct yt_error *error)
{
	if (tape->event_count >= YT_ARRAY_LEN(tape->events)
	    || event != (enum direct_fighter_kill_event)tape->event_count)
		return false;
	tape->events[tape->event_count++] = event;
	if (tape->event_count != tape->fail_at)
		return true;
	if (error != NULL) {
		error->status = YT_IO_ERROR;
		(void)snprintf(error->operation, sizeof(error->operation),
		    "direct fighter event %u", (unsigned)event);
	}
	return false;
}

static bool
direct_fighter_kill_test_sound(void *context, struct yt_error *error)
{
	return direct_fighter_kill_step(context, DIRECT_KILL_SOUND, error);
}

static bool
direct_fighter_kill_test_read_player(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct direct_fighter_kill_tape *tape = context;

	if (player_record != 3 || !direct_fighter_kill_step(tape,
	    DIRECT_KILL_READ_PLAYER, error))
		return false;
	*player = tape->target;
	tape->field = DIRECT_KILL_FIELD_TARGET;
	return true;
}

static bool
direct_fighter_kill_test_name_length(void *context, float raw_length,
    size_t *length, struct yt_error *error)
{
	struct direct_fighter_kill_tape *tape = context;

	if (raw_length != 3.0f || !direct_fighter_kill_step(tape,
	    DIRECT_KILL_NAME_LENGTH, error))
		return false;
	*length = 3U;
	return true;
}

static bool
direct_fighter_kill_test_death(void *context, int victim_record,
    float killer, struct yt_error *error)
{
	struct direct_fighter_kill_tape *tape = context;

	if (victim_record != 3 || killer != 2.0f
	    || !direct_fighter_kill_step(tape, DIRECT_KILL_DEATH, error))
		return false;
	tape->field = DIRECT_KILL_FIELD_TARGET;
	return true;
}

static bool
direct_fighter_kill_test_salvage(void *context, int victim_record,
    float killer, struct yt_error *error)
{
	struct direct_fighter_kill_tape *tape = context;

	if (victim_record != 3 || killer != 2.0f
	    || !direct_fighter_kill_step(tape, DIRECT_KILL_SALVAGE, error))
		return false;
	tape->rng_position += 6U;
	tape->field = DIRECT_KILL_FIELD_CURRENT;
	return true;
}

static bool
direct_fighter_kill_test_read_sector(void *context, float logical_sector,
    struct yt_sector *sector, struct yt_error *error)
{
	struct direct_fighter_kill_tape *tape = context;

	if (logical_sector != 42 || !direct_fighter_kill_step(tape,
	    DIRECT_KILL_READ_SECTOR, error))
		return false;
	*sector = tape->sector;
	tape->field = DIRECT_KILL_FIELD_SECTOR;
	return true;
}

static bool
direct_fighter_kill_test_write_sector(void *context, float logical_sector,
    struct yt_sector *sector, struct yt_error *error)
{
	struct direct_fighter_kill_tape *tape = context;

	if (logical_sector != 42)
		return false;
	tape->staged_sector = *sector;
	tape->staged = true;
	tape->field = DIRECT_KILL_FIELD_SECTOR;
	if (!direct_fighter_kill_step(tape, DIRECT_KILL_WRITE_SECTOR, error))
		return false;
	tape->durable_sector = *sector;
	tape->durable = true;
	return true;
}

static bool
direct_fighter_kill_test_warning(struct direct_fighter_kill_tape *tape,
    const uint8_t *text, size_t length)
{
	static const uint8_t expected[] =
	    "  -  V\0X had sector mines! They EXPLODED!";

	(void)tape;
	return length == sizeof(expected) - 1U
	    && memcmp(text, expected, length) == 0;
}

static bool
direct_fighter_kill_test_present(void *context, const uint8_t *text,
    size_t length, struct yt_error *error)
{
	struct direct_fighter_kill_tape *tape = context;

	return direct_fighter_kill_test_warning(tape, text, length)
	    && direct_fighter_kill_step(tape, DIRECT_KILL_PRESENT, error);
}

static bool
direct_fighter_kill_test_news(void *context, const uint8_t *text,
    size_t length, struct yt_error *error)
{
	struct direct_fighter_kill_tape *tape = context;

	if (!direct_fighter_kill_test_warning(tape, text, length)
	    || !direct_fighter_kill_step(tape, DIRECT_KILL_NEWS, error))
		return false;
	tape->news_durable = true;
	return true;
}

static bool
direct_fighter_kill_test_mine(void *context, bool *terminal,
    uint8_t destroyed_raw[4], struct yt_error *error)
{
	struct direct_fighter_kill_tape *tape = context;

	if (!direct_fighter_kill_step(tape, DIRECT_KILL_MINE, error))
		return false;
	tape->rng_position += 1U;
	tape->field = DIRECT_KILL_FIELD_SECTOR;
	*terminal = tape->mine_terminal;
	memcpy(destroyed_raw, tape->mine_destroyed_raw, 4U);
	return true;
}

static bool
direct_fighter_kill_test_fatal(void *context, struct yt_error *error)
{
	return direct_fighter_kill_step(context, DIRECT_KILL_FATAL, error);
}

static const struct yt_direct_fighter_kill_ops direct_fighter_kill_test_ops = {
	direct_fighter_kill_test_sound,
	direct_fighter_kill_test_read_player,
	direct_fighter_kill_test_name_length,
	direct_fighter_kill_test_death,
	direct_fighter_kill_test_salvage,
	direct_fighter_kill_test_read_sector,
	direct_fighter_kill_test_write_sector,
	direct_fighter_kill_test_present,
	direct_fighter_kill_test_news,
	direct_fighter_kill_test_mine,
	direct_fighter_kill_test_fatal,
};

static void
direct_fighter_kill_test_reset(struct direct_fighter_kill_tape *tape,
    const struct yt_player *target, const struct yt_sector *sector,
    size_t fail_at)
{
	memset(tape, 0, sizeof(*tape));
	tape->target = *target;
	tape->sector = *sector;
	tape->fail_at = fail_at;
	tape->field = DIRECT_KILL_FIELD_ENTRY;
	tape->mine_destroyed_raw[3] = 1U;
}

static struct yt_direct_fighter_kill_state
direct_fighter_kill_test_state(float shields)
{
	struct yt_direct_fighter_kill_state state = {
		.target_shields = shields,
		.target_record = 3,
		.current_player_record = 2,
		.current_sector = 42,
	};

	return state;
}

static bool
check_direct_fighter_kill_transaction(void)
{
	struct direct_fighter_kill_tape tape;
	struct yt_direct_fighter_kill_state state;
	struct yt_player target;
	struct yt_sector sector;
	struct yt_record raw;
	struct yt_record expected_sector;
	struct yt_error error;
	struct yt_direct_fighter_kill_ops lazy_ops;
	size_t index;

	memset(&target, 0, sizeof(target));
	memset(&sector, 0, sizeof(sector));
	for (index = 0U; index < YT_RECORD_SIZE; ++index) {
		target.record.bytes[index] = (uint8_t)(index ^ 0x96U);
		sector.record.bytes[index] = (uint8_t)(index ^ 0x69U);
	}
	target.record.bytes[0] = 'V';
	target.record.bytes[1] = 0;
	target.record.bytes[2] = 'X';
	if (!yt_record_set_number(&target.record, YT_F85, 3.0f)
	    || !yt_record_set_number(&target.record, YT_F129, 2.0f)
	    || !yt_record_set_number(&sector.record, YT_F129, 5.0f))
		return false;
	raw = target.record;
	yt_player_decode(&target, &raw);
	raw = sector.record;
	yt_sector_decode(&sector, &raw);
	expected_sector = sector.record;
	if (!yt_record_set_number(&expected_sector, YT_F129, 7.0f))
		return false;

	direct_fighter_kill_test_reset(&tape, &target, &sector, 0U);
	state = direct_fighter_kill_test_state(0.0f);
	if (!yt_direct_fighter_kill_run(&state, &direct_fighter_kill_test_ops,
	    &tape, NULL) || tape.event_count != DIRECT_KILL_EVENT_COUNT
	    || tape.rng_position != 7U || !tape.staged || !tape.durable
	    || !tape.news_durable || tape.field != DIRECT_KILL_FIELD_SECTOR
	    || state.saved_mines != 2.0f || state.saved_name_length != 3U
	    || memcmp(state.saved_name, "V\0X", 3U) != 0
	    || state.route != YT_DIRECT_FIGHTER_COMMON_FATAL
	    || memcmp(tape.durable_sector.record.bytes, expected_sector.bytes,
	    YT_RECORD_SIZE) != 0)
		return false;

	direct_fighter_kill_test_reset(&tape, &target, &sector, 0U);
	state = direct_fighter_kill_test_state(1.0f);
	memset(&lazy_ops, 0, sizeof(lazy_ops));
	if (!yt_direct_fighter_kill_run(&state, &lazy_ops,
	    &tape, NULL) || tape.event_count != 0U
	    || state.route != YT_DIRECT_FIGHTER_NO_KILL)
		return false;

	target.mines = 0.0f;
	if (!yt_record_set_number(&target.record, YT_F129, target.mines))
		return false;
	direct_fighter_kill_test_reset(&tape, &target, &sector, 0U);
	state = direct_fighter_kill_test_state(0.0f);
	lazy_ops = direct_fighter_kill_test_ops;
	lazy_ops.read_sector = NULL;
	lazy_ops.write_sector = NULL;
	lazy_ops.present = NULL;
	lazy_ops.news = NULL;
	lazy_ops.mine = NULL;
	lazy_ops.fatal = NULL;
	if (!yt_direct_fighter_kill_run(&state, &lazy_ops,
	    &tape, NULL) || tape.event_count != 5U || tape.rng_position != 6U
	    || state.route != YT_DIRECT_FIGHTER_FRESH_PROMPT || tape.staged)
		return false;
	target.mines = 2.0f;
	if (!yt_record_set_number(&target.record, YT_F129, target.mines))
		return false;

	direct_fighter_kill_test_reset(&tape, &target, &sector, 0U);
	tape.mine_terminal = true;
	state = direct_fighter_kill_test_state(0.0f);
	lazy_ops = direct_fighter_kill_test_ops;
	lazy_ops.fatal = NULL;
	if (!yt_direct_fighter_kill_run(&state, &lazy_ops,
	    &tape, NULL) || tape.event_count != 10U
	    || state.route != YT_DIRECT_FIGHTER_MINE_TERMINAL)
		return false;
	direct_fighter_kill_test_reset(&tape, &target, &sector, 0U);
	memset(tape.mine_destroyed_raw, 0, 4U);
	state = direct_fighter_kill_test_state(0.0f);
	if (!yt_direct_fighter_kill_run(&state, &lazy_ops,
	    &tape, NULL) || tape.event_count != 10U
	    || state.route != YT_DIRECT_FIGHTER_FRESH_PROMPT)
		return false;

	for (index = 1U; index <= DIRECT_KILL_EVENT_COUNT; ++index) {
		direct_fighter_kill_test_reset(&tape, &target, &sector, index);
		state = direct_fighter_kill_test_state(0.0f);
		yt_error_clear(&error);
		if (yt_direct_fighter_kill_run(&state,
		    &direct_fighter_kill_test_ops, &tape, &error)
		    || tape.event_count != index || error.status != YT_IO_ERROR
		    || tape.staged != (index >= 7U)
		    || tape.durable != (index > 7U)
		    || tape.news_durable != (index > 9U)
		    || tape.rng_position != (index > 10U ? 7U
		    : index > 5U ? 6U : 0U))
			return false;
	}
	return true;
}

enum player_death_event {
	PLAYER_DEATH_CLEAR_CACHE,
	PLAYER_DEATH_READ_PLAYER,
	PLAYER_DEATH_WRITE_PLAYER,
	PLAYER_DEATH_READ_SECTOR,
	PLAYER_DEATH_WRITE_SECTOR,
	PLAYER_DEATH_REMOVE_TEAM,
	PLAYER_DEATH_READ_PORT,
	PLAYER_DEATH_WRITE_PORT,
	PLAYER_DEATH_PRESENT,
	PLAYER_DEATH_NEWS,
	PLAYER_DEATH_SET_CURRENT,
	PLAYER_DEATH_FLUSH,
};

static const enum player_death_event player_death_distinct_events[] = {
	PLAYER_DEATH_CLEAR_CACHE,
	PLAYER_DEATH_READ_PLAYER,
	PLAYER_DEATH_WRITE_PLAYER,
	PLAYER_DEATH_READ_SECTOR,
	PLAYER_DEATH_WRITE_SECTOR,
	PLAYER_DEATH_READ_SECTOR,
	PLAYER_DEATH_REMOVE_TEAM,
	PLAYER_DEATH_READ_PORT,
	PLAYER_DEATH_WRITE_PORT,
	PLAYER_DEATH_READ_PORT,
	PLAYER_DEATH_WRITE_PORT,
	PLAYER_DEATH_PRESENT,
	PLAYER_DEATH_READ_PLAYER,
	PLAYER_DEATH_WRITE_PLAYER,
	PLAYER_DEATH_READ_PLAYER,
	PLAYER_DEATH_NEWS,
	PLAYER_DEATH_NEWS,
	PLAYER_DEATH_FLUSH,
};

static const enum player_death_event player_death_self_events[] = {
	PLAYER_DEATH_CLEAR_CACHE,
	PLAYER_DEATH_READ_PLAYER,
	PLAYER_DEATH_WRITE_PLAYER,
	PLAYER_DEATH_READ_SECTOR,
	PLAYER_DEATH_WRITE_SECTOR,
	PLAYER_DEATH_READ_SECTOR,
	PLAYER_DEATH_REMOVE_TEAM,
	PLAYER_DEATH_READ_PORT,
	PLAYER_DEATH_WRITE_PORT,
	PLAYER_DEATH_READ_PORT,
	PLAYER_DEATH_WRITE_PORT,
	PLAYER_DEATH_NEWS,
	PLAYER_DEATH_SET_CURRENT,
	PLAYER_DEATH_FLUSH,
};

struct player_death_tape {
	const enum player_death_event *expected;
	size_t expected_count;
	size_t event_count;
	size_t fail_at;
	struct yt_player players[4];
	struct yt_sector sectors[3];
	struct yt_port ports[3];
	struct yt_player staged_player;
	struct yt_sector staged_sector;
	struct yt_port staged_port;
	bool cache_cleared;
	bool player_written[4];
	bool sector_written[3];
	bool port_written[3];
	bool title_visible;
	uint8_t news[2][128];
	size_t news_length[2];
	size_t news_count;
	bool current_set;
	bool flushed;
	size_t rng_position;
};

static bool
player_death_test_step(struct player_death_tape *tape,
    enum player_death_event event, struct yt_error *error)
{
	if (tape->event_count >= tape->expected_count
	    || tape->expected[tape->event_count] != event)
		return false;
	++tape->event_count;
	if (tape->event_count != tape->fail_at)
		return true;
	if (error != NULL) {
		error->status = YT_IO_ERROR;
		(void)snprintf(error->operation, sizeof(error->operation),
		    "player death event %u", (unsigned)event);
	}
	return false;
}

static void
player_death_test_clear_cache(void *context, int victim_record)
{
	struct player_death_tape *tape = context;

	if (victim_record == 3
	    && player_death_test_step(tape, PLAYER_DEATH_CLEAR_CACHE, NULL))
		tape->cache_cleared = true;
}

static bool
player_death_test_read_player(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct player_death_tape *tape = context;

	if (player_record < 0 || player_record >= (int)YT_ARRAY_LEN(tape->players)
	    || !player_death_test_step(tape, PLAYER_DEATH_READ_PLAYER, error))
		return false;
	*player = tape->players[player_record];
	return true;
}

static bool
player_death_test_write_player(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct player_death_tape *tape = context;

	if (player_record < 0 || player_record >= (int)YT_ARRAY_LEN(tape->players))
		return false;
	tape->staged_player = *player;
	if (!player_death_test_step(tape, PLAYER_DEATH_WRITE_PLAYER, error))
		return false;
	tape->players[player_record] = *player;
	tape->player_written[player_record] = true;
	return true;
}

static bool
player_death_test_read_sector(void *context, int logical_sector,
    struct yt_sector *sector, struct yt_error *error)
{
	struct player_death_tape *tape = context;

	if (logical_sector < 1 || logical_sector > 2
	    || !player_death_test_step(tape, PLAYER_DEATH_READ_SECTOR, error))
		return false;
	*sector = tape->sectors[logical_sector];
	return true;
}

static bool
player_death_test_write_sector(void *context, int logical_sector,
    struct yt_sector *sector, struct yt_error *error)
{
	struct player_death_tape *tape = context;

	if (logical_sector < 1 || logical_sector > 2)
		return false;
	tape->staged_sector = *sector;
	if (!player_death_test_step(tape, PLAYER_DEATH_WRITE_SECTOR, error))
		return false;
	tape->sectors[logical_sector] = *sector;
	tape->sector_written[logical_sector] = true;
	return true;
}

static bool
player_death_test_remove_team(void *context, int victim_record,
    struct yt_error *error)
{
	struct player_death_tape *tape = context;

	return victim_record == 3 && player_death_test_step(tape,
	    PLAYER_DEATH_REMOVE_TEAM, error);
}

static bool
player_death_test_read_port(void *context, int logical_port,
    struct yt_port *port, struct yt_error *error)
{
	struct player_death_tape *tape = context;

	if (logical_port < 1 || logical_port > 2
	    || !player_death_test_step(tape, PLAYER_DEATH_READ_PORT, error))
		return false;
	*port = tape->ports[logical_port];
	return true;
}

static bool
player_death_test_write_port(void *context, int logical_port,
    struct yt_port *port, struct yt_error *error)
{
	struct player_death_tape *tape = context;

	if (logical_port < 1 || logical_port > 2)
		return false;
	tape->staged_port = *port;
	if (!player_death_test_step(tape, PLAYER_DEATH_WRITE_PORT, error))
		return false;
	tape->ports[logical_port] = *port;
	tape->port_written[logical_port] = true;
	return true;
}

static bool
player_death_test_present(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	static const uint8_t expected[] =
	    "The titles to 2 ports of V\0X's are now yours!";
	struct player_death_tape *tape = context;

	if (length != sizeof(expected) - 1U
	    || memcmp(text, expected, length) != 0
	    || !player_death_test_step(tape, PLAYER_DEATH_PRESENT, error))
		return false;
	tape->title_visible = true;
	return true;
}

static bool
player_death_test_news(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	struct player_death_tape *tape = context;

	if (tape->news_count >= YT_ARRAY_LEN(tape->news)
	    || length > sizeof(tape->news[0])
	    || !player_death_test_step(tape, PLAYER_DEATH_NEWS, error))
		return false;
	memcpy(tape->news[tape->news_count], text, length);
	tape->news_length[tape->news_count] = length;
	++tape->news_count;
	return true;
}

static void
player_death_test_set_current(void *context,
    const struct yt_player *player)
{
	struct player_death_tape *tape = context;

	if (player != NULL && player_death_test_step(tape,
	    PLAYER_DEATH_SET_CURRENT, NULL))
		tape->current_set = true;
}

static bool
player_death_test_flush(void *context, struct yt_error *error)
{
	struct player_death_tape *tape = context;

	if (!player_death_test_step(tape, PLAYER_DEATH_FLUSH, error))
		return false;
	tape->flushed = true;
	return true;
}

static const struct yt_player_death_ops player_death_test_ops = {
	player_death_test_clear_cache,
	player_death_test_read_player,
	player_death_test_write_player,
	player_death_test_read_sector,
	player_death_test_write_sector,
	player_death_test_remove_team,
	player_death_test_read_port,
	player_death_test_write_port,
	player_death_test_present,
	player_death_test_news,
	player_death_test_set_current,
	player_death_test_flush,
};

static void
player_death_test_reset(struct player_death_tape *tape,
    const struct yt_player players[4], const struct yt_sector sectors[3],
    const struct yt_port ports[3], bool self, size_t fail_at)
{
	memset(tape, 0, sizeof(*tape));
	memcpy(tape->players, players, sizeof(tape->players));
	memcpy(tape->sectors, sectors, sizeof(tape->sectors));
	memcpy(tape->ports, ports, sizeof(tape->ports));
	tape->expected = self ? player_death_self_events
	    : player_death_distinct_events;
	tape->expected_count = self ? YT_ARRAY_LEN(player_death_self_events)
	    : YT_ARRAY_LEN(player_death_distinct_events);
	tape->fail_at = fail_at;
	tape->rng_position = 23U;
}

static bool
check_player_death_transaction(void)
{
	static const uint8_t kill_news[] = "  -  CURRENT killed V\0X";
	static const uint8_t port_news[] = "  -  Took 2 ports from V\0X";
	static const uint8_t self_news[] = "  -  CURRENT was killed!";
	static const uint8_t dirty_zero[4] = {0x00, 0x00, 0x7a, 0x00};
	struct player_death_tape tape;
	struct yt_player_death_state state;
	struct yt_player players[4];
	struct yt_sector sectors[3];
	struct yt_port ports[3];
	struct yt_record raw;
	struct yt_error error;
	size_t index;

	memset(players, 0, sizeof(players));
	memset(sectors, 0, sizeof(sectors));
	memset(ports, 0, sizeof(ports));
	for (index = 0U; index < YT_RECORD_SIZE; ++index)
		raw.bytes[index] = (uint8_t)(index ^ 0x5dU);
	raw.bytes[0] = 'V';
	raw.bytes[1] = 0;
	raw.bytes[2] = 'X';
	if (!yt_record_set_number(&raw, YT_F85, 3.0f)
	    || !yt_record_set_number(&raw, YT_F117, 99.0f))
		return false;
	yt_player_decode(&players[3], &raw);
	for (index = 0U; index < YT_RECORD_SIZE; ++index)
		raw.bytes[index] = (uint8_t)(index ^ 0xa7U);
	memcpy(raw.bytes, "RAW", 3U);
	if (!yt_record_set_number(&raw, YT_F85, 3.0f)
	    || !yt_record_set_number(&raw, YT_F117, 5.0f))
		return false;
	yt_player_decode(&players[2], &raw);
	for (index = 1U; index <= 2U; ++index) {
		yt_record_blank(&raw);
		if (!yt_record_set_number(&raw, YT_F81, index == 1U ? 0.0f : 7.0f)
		    || !yt_record_set_number(&raw, YT_F85,
		    index == 1U ? 3.0f : 4.0f))
			return false;
		yt_sector_decode(&sectors[index], &raw);
		yt_record_blank(&raw);
		if (!yt_record_set_number(&raw, YT_F89, 50.0f + (float)index)
		    || !yt_record_set_number(&raw, YT_F97, 3.0f)
		    || !yt_record_set_number(&raw, YT_F101, 70.0f + (float)index))
			return false;
		yt_port_decode(&ports[index], &raw);
	}

	player_death_test_reset(&tape, players, sectors, ports, false, 0U);
	state = (struct yt_player_death_state){
		.victim_record = 3,
		.current_player_record = 2,
		.killer = 2.0f,
		.sector_count = 2,
		.port_count = 2,
		.last_player_record = 51.0f,
		.current_name = (const uint8_t *)"CURRENT",
		.current_name_length = 7U,
	};
	if (!yt_player_death_run(&state, &player_death_test_ops, &tape, NULL)
	    || tape.event_count != tape.expected_count || !tape.cache_cleared
	    || !tape.player_written[3] || !tape.sector_written[1]
	    || tape.sector_written[2] || !tape.port_written[1]
	    || !tape.port_written[2] || !tape.title_visible
	    || !tape.player_written[2] || tape.news_count != 2U
	    || tape.news_length[0] != sizeof(kill_news) - 1U
	    || memcmp(tape.news[0], kill_news, sizeof(kill_news) - 1U) != 0
	    || tape.news_length[1] != sizeof(port_news) - 1U
	    || memcmp(tape.news[1], port_news, sizeof(port_news) - 1U) != 0
	    || !tape.flushed || tape.current_set || tape.rng_position != 23U
	    || !state.complete || state.old_ports_owned != 99.0f
	    || state.matched_ports != 2 || state.victim_name_length != 3U
	    || memcmp(state.victim_name, "V\0X", 3U) != 0
	    || tape.sectors[1].fighter_owner != -2.0f
	    || tape.sectors[1].fighters != 0.0f
	    || tape.ports[1].owner != 2.0f || tape.ports[1].last_minute != 2.0f
	    || tape.ports[1].treasury != 51.0f
	    || tape.players[2].ports_owned != 7.0f
	    || memcmp(tape.players[3].record.bytes + YT_F57, dirty_zero, 4U)
	    != 0 || memcmp(tape.players[3].record.bytes + YT_F117,
	    dirty_zero, 4U) != 0)
		return false;

	for (index = 2U; index <= YT_ARRAY_LEN(player_death_distinct_events);
	    ++index) {
		player_death_test_reset(&tape, players, sectors, ports, false,
		    index);
		state = (struct yt_player_death_state){
			.victim_record = 3,
			.current_player_record = 2,
			.killer = 2.0f,
			.sector_count = 2,
			.port_count = 2,
			.last_player_record = 51.0f,
			.current_name = (const uint8_t *)"CURRENT",
			.current_name_length = 7U,
		};
		yt_error_clear(&error);
		if (yt_player_death_run(&state, &player_death_test_ops, &tape,
		    &error) || tape.event_count != index
		    || !tape.cache_cleared || error.status != YT_IO_ERROR
		    || tape.rng_position != 23U || state.complete)
			return false;
	}

	player_death_test_reset(&tape, players, sectors, ports, true, 0U);
	state = (struct yt_player_death_state){
		.victim_record = 3,
		.current_player_record = 3,
		.killer = 3.0f,
		.sector_count = 2,
		.port_count = 2,
		.last_player_record = 51.0f,
		.current_name = (const uint8_t *)"CURRENT",
		.current_name_length = 7U,
	};
	if (!yt_player_death_run(&state, &player_death_test_ops, &tape, NULL)
	    || tape.event_count != tape.expected_count || tape.title_visible
	    || tape.news_count != 1U
	    || tape.news_length[0] != sizeof(self_news) - 1U
	    || memcmp(tape.news[0], self_news, sizeof(self_news) - 1U) != 0
	    || !tape.current_set || !tape.flushed || !state.complete
	    || tape.ports[1].owner != 0.0f || tape.ports[1].treasury != 0.0f
	    || tape.ports[1].last_minute != 71.0f)
		return false;
	return true;
}

static bool
check_player_death_model(void)
{
	static const uint8_t dirty_zero[4] = {0x00, 0x00, 0x7a, 0x00};
	static const uint8_t victim_name[] = {'V', 0, 'X'};
	static const uint8_t killer_name[] = {'K', 0, 'Y'};
	static const uint8_t title_expected[] =
	    "The titles to 2 ports of V\0X's are now yours!";
	static const uint8_t self_expected[] = "  -  K\0Y was killed!";
	static const uint8_t kill_expected[] = "  -  K\0Y killed V\0X";
	static const uint8_t ports_expected[] = "  -  Took 2 ports from V\0X";
	static const size_t roster_offsets[4] = {
		YT_F109, YT_F117, YT_F121, YT_F125
	};
	struct yt_player player;
	struct yt_sector sector;
	struct yt_port port;
	struct yt_record record;
	struct yt_record team;
	struct yt_record before;
	uint8_t row[128];
	size_t length;
	size_t index;

	yt_record_blank(&record);
	(void)yt_record_set_number(&record, YT_F45, 9.0f);
	(void)yt_record_set_number(&record, YT_F57, 77.0f);
	(void)yt_record_set_number(&record, YT_F61, 123.0f);
	(void)yt_record_set_number(&record, YT_F117, 4.0f);
	yt_player_decode(&player, &record);
	before = player.record;
	yt_death_player_overlay(&player, 3.0f);
	if (player.killed_by != 3.0f || player.sector != 0.0f
	    || player.ports_owned != 0.0f
	    || yt_record_get_number(&player.record, YT_F45) != 3.0f
	    || memcmp(player.record.bytes + YT_F57, dirty_zero, 4U) != 0
	    || memcmp(player.record.bytes + YT_F117, dirty_zero, 4U) != 0
	    || yt_record_get_number(&player.record, YT_F61) != 123.0f)
		return false;
	for (index = 0U; index < YT_RECORD_SIZE; ++index)
		if ((index < YT_F45 || index >= YT_F45 + 4U)
		    && (index < YT_F57 || index >= YT_F57 + 4U)
		    && (index < YT_F117 || index >= YT_F117 + 4U)
		    && player.record.bytes[index] != before.bytes[index])
			return false;

	yt_record_blank(&record);
	(void)yt_record_set_number(&record, YT_F81, 0.0f);
	(void)yt_record_set_number(&record, YT_F85, 2.0f);
	yt_sector_decode(&sector, &record);
	if (!yt_death_sector_overlay(&sector, 2.0f)
	    || sector.fighters != 0.0f || sector.fighter_owner != -2.0f
	    || yt_record_get_number(&sector.record, YT_F85) != -2.0f
	    || yt_death_sector_overlay(&sector, 7.0f))
		return false;

	yt_record_blank(&team);
	(void)yt_record_set_number(&team, YT_F77, 2.0f);
	for (index = 0U; index < 4U; ++index)
		(void)yt_record_set_number(&team, roster_offsets[index],
		    index == 2U ? 9.0f : 2.0f);
	yt_death_team_roster_overlay(&team, 2.0f);
	if (yt_record_get_number(&team, YT_F77) != 2.0f
	    || yt_record_get_number(&team, YT_F109) != 0.0f
	    || yt_record_get_number(&team, YT_F117) != 0.0f
	    || yt_record_get_number(&team, YT_F121) != 9.0f
	    || yt_record_get_number(&team, YT_F125) != 0.0f)
		return false;

	yt_record_blank(&record);
	(void)yt_record_set_number(&record, YT_F89, 77.0f);
	(void)yt_record_set_number(&record, YT_F97, 2.0f);
	(void)yt_record_set_number(&record, YT_F101, 55.0f);
	yt_port_decode(&port, &record);
	if (yt_death_port_overlay(&port, 2.0f, 3.0f, 51.0f)
	    != YT_DEATH_PORT_TRANSFERRED
	    || port.owner != 3.0f || port.last_minute != 3.0f
	    || port.treasury != 77.0f)
		return false;
	if (yt_death_port_overlay(&port, 2.0f, 3.0f, 51.0f)
	    != YT_DEATH_PORT_UNMATCHED)
		return false;
	yt_record_blank(&record);
	(void)yt_record_set_number(&record, YT_F89, 77.0f);
	(void)yt_record_set_number(&record, YT_F97, 2.0f);
	(void)yt_record_set_number(&record, YT_F101, 55.0f);
	yt_port_decode(&port, &record);
	if (yt_death_port_overlay(&port, 2.0f, -1.0f, 51.0f)
	    != YT_DEATH_PORT_CLEARED
	    || port.owner != 0.0f || port.treasury != 0.0f
	    || port.last_minute != 55.0f)
		return false;

	yt_record_blank(&record);
	(void)yt_record_set_number(&record, YT_F117, 4.0f);
	yt_player_decode(&player, &record);
	yt_death_killer_credit_overlay(&player, 2.0f);
	if (player.ports_owned != 6.0f
	    || yt_record_get_number(&player.record, YT_F117) != 6.0f)
		return false;
	return yt_death_title_row(victim_name, sizeof(victim_name), 2.0f,
	    row, sizeof(row), &length)
	    && length == sizeof(title_expected) - 1U
	    && memcmp(row, title_expected, length) == 0
	    && yt_death_kill_news_row(killer_name, sizeof(killer_name),
	    victim_name, sizeof(victim_name), true, row, sizeof(row), &length)
	    && length == sizeof(self_expected) - 1U
	    && memcmp(row, self_expected, length) == 0
	    && yt_death_kill_news_row(killer_name, sizeof(killer_name),
	    victim_name, sizeof(victim_name), false, row, sizeof(row), &length)
	    && length == sizeof(kill_expected) - 1U
	    && memcmp(row, kill_expected, length) == 0
	    && yt_death_port_news_row(victim_name, sizeof(victim_name), 2.0f,
	    row, sizeof(row), &length)
	    && length == sizeof(ports_expected) - 1U
	    && memcmp(row, ports_expected, length) == 0
	    && !yt_death_title_row(victim_name, sizeof(victim_name), 2.0f,
	    row, sizeof(title_expected) - 2U, &length);
}

static bool
check_emergency_warp_model(void)
{
	static const uint8_t result_expected[] =
	    "sector 1003. However, it takes you 3 turns to recharge your engines!";
	static const uint8_t stranded_expected[] =
	    "You are stranded in sector 1003.";
	struct yt_player player;
	struct yt_record before;
	uint8_t row[128];
	size_t length;
	size_t index;

	if (yt_emergency_warp_duration(0.0f, 0.0f) != 0.0f
	    || yt_emergency_warp_duration(0.5f, 0.25f) != 52.5f
	    || yt_emergency_warp_destination(0.5f, 2004.0f) != 1003.0f
	    || yt_emergency_warp_destination(0.0f, 2004.0f) != 1.0f
	    || yt_emergency_warp_cost(0.0f, 0.75f, 77.0f, false) != 3.0f
	    || yt_emergency_warp_cost(20.0f, 0.0f, 77.0f, false) != 77.0f
	    || yt_emergency_warp_cost(31.0f, 0.0f, 77.0f, true) != 77.0f
	    || !yt_emergency_warp_result_row(1003.0f, 3.0f, row,
	    sizeof(row), &length)
	    || length != sizeof(result_expected) - 1U
	    || memcmp(row, result_expected, length) != 0
	    || !yt_emergency_warp_stranded_row(1003.0f, row,
	    sizeof(row), &length)
	    || length != sizeof(stranded_expected) - 1U
	    || memcmp(row, stranded_expected, length) != 0)
		return false;
	memset(&player, 0, sizeof(player));
	for (index = 0U; index < YT_RECORD_SIZE; ++index)
		player.record.bytes[index] = (uint8_t)(index ^ 0x96U);
	player.turns = 77.0f;
	player.sector = 42.0f;
	before = player.record;
	yt_emergency_warp_player_overlay(&player, 1003.0f, 3.0f);
	if (player.turns != 74.0f || player.sector != 1003.0f
	    || yt_record_get_number(&player.record, YT_F49) != 74.0f
	    || yt_record_get_number(&player.record, YT_F57) != 1003.0f)
		return false;
	for (index = 0U; index < YT_RECORD_SIZE; ++index)
		if ((index < YT_F49 || index >= YT_F49 + 4U)
		    && (index < YT_F57 || index >= YT_F57 + 4U)
		    && player.record.bytes[index] != before.bytes[index])
			return false;
	return true;
}

static bool
check_movement_model(void)
{
	static const uint8_t warps_expected[] =
	    "Warps lead to,-2, 9, 12.5, 9";
	static const uint8_t empty_expected[] = "Warps lead to";
	static const uint8_t prompt_expected[] =
	    "Move into sector 42? [y/N] ";
	const float warps[6] = {0.0f, -2.0f, 9.0f, 0.0f, 12.5f, 9.0f};
	const float empty[6] = {0};
	struct yt_player player;
	struct yt_record before;
	uint8_t row[128];
	size_t length;
	size_t index;

	if (!yt_movement_warp_row(warps, row, sizeof(row), &length)
	    || length != sizeof(warps_expected) - 1U
	    || memcmp(row, warps_expected, length) != 0
	    || !yt_movement_warp_row(empty, row, sizeof(row), &length)
	    || length != sizeof(empty_expected) - 1U
	    || memcmp(row, empty_expected, length) != 0
	    || !yt_movement_confirmation_prompt(42.0f, row,
	    sizeof(row), &length)
	    || length != sizeof(prompt_expected) - 1U
	    || memcmp(row, prompt_expected, length) != 0)
		return false;
	memset(&player, 0, sizeof(player));
	for (index = 0U; index < YT_RECORD_SIZE; ++index)
		player.record.bytes[index] = (uint8_t)(index ^ 0x3cU);
	player.sector = 12.0f;
	before = player.record;
	yt_movement_player_overlay(&player, 12.5f);
	if (player.sector != 12.5f
	    || yt_record_get_number(&player.record, YT_F57) != 12.5f)
		return false;
	for (index = 0U; index < YT_RECORD_SIZE; ++index)
		if ((index < YT_F57 || index >= YT_F57 + 4U)
		    && player.record.bytes[index] != before.bytes[index])
			return false;
	return true;
}

static bool
check_maintenance_entry_output(void)
{
	static const uint8_t expected_common[] =
	    "\rYankee Trader Maintenance program\r"
	    "        by Alan Davenport\r\r"
	    "       (Revision 03/14/94)\r\r"
	    "This should be run once per day.\r"
	    "\rLoading players, deleting inactive players and "
	    "subtracting cloak charge.\r\r";
	static const uint8_t expected_same_day[] =
	    "\rMaintenance not needed!\r";
	static const uint8_t expected_wrapper[] =
	    "\rDaily Maintenance Completed OK\r";
	static const uint8_t expected_compaction[] =
	    "\rCompressing Message Base's\r";
	struct yt_maintenance_output_result result;
	size_t prefix_length = sizeof(expected_same_day) - 1U;

	if (!yt_maintenance_same_day(123.0f, 123.0f)
	    || yt_maintenance_same_day(122.0f, 123.0f)
	    || !yt_maintenance_compose_entry(false, &result)
	    || result.row_count != 11U || result.final_column != 0U
	    || result.output_length != sizeof(expected_common) - 1U
	    || memcmp(result.output, expected_common,
	    sizeof(expected_common) - 1U) != 0
	    || result.rows[0].address != 0x0399U
	    || result.rows[4].address != 0x03E5U
	    || result.rows[4].newline
	    || result.rows[10].address != 0x050DU)
		return false;
	if (!yt_maintenance_compose_entry(true, &result)
	    || result.row_count != 13U
	    || result.output_length != prefix_length
	    + sizeof(expected_common) - 1U
	    || memcmp(result.output, expected_same_day, prefix_length) != 0
	    || memcmp(result.output + prefix_length, expected_common,
	    sizeof(expected_common) - 1U) != 0
	    || result.rows[0].address != 0x036DU
	    || result.rows[1].address != 0x037FU)
		return false;
	if (!yt_maintenance_compose_wrapper(&result)
	    || result.row_count != 2U
	    || result.output_length != sizeof(expected_wrapper) - 1U
	    || memcmp(result.output, expected_wrapper,
	    sizeof(expected_wrapper) - 1U) != 0
	    || result.rows[0].address != 0x004FU
	    || result.rows[1].address != 0x0061U)
		return false;
	if (!yt_maintenance_compose_message_compaction(&result)
	    || result.row_count != 2U
	    || result.output_length != sizeof(expected_compaction) - 1U
	    || memcmp(result.output, expected_compaction,
	    sizeof(expected_compaction) - 1U) != 0
	    || result.rows[0].address != 0x673AU
	    || result.rows[1].address != 0x674CU)
		return false;
	return !yt_maintenance_compose_entry(false, NULL)
	    && !yt_maintenance_compose_wrapper(NULL)
	    && !yt_maintenance_compose_message_compaction(NULL);
}

struct score_random_script {
	const uint8_t *data;
	size_t length;
	size_t position;
};

static bool
score_random_fill(void *context, void *buffer, size_t length,
    struct yt_error *error)
{
	struct score_random_script *script = context;

	if (script == NULL || length > script->length - script->position) {
		if (error != NULL)
			error->status = YT_RANDOM_ERROR;
		return false;
	}
	memcpy(buffer, script->data + script->position, length);
	script->position += length;
	return true;
}

static bool
check_maintenance_port_model(void)
{
	static const uint8_t zero_output[] =
	    "\rRunning port maintenance...\r";
	static const uint8_t plague_output[] =
	    "\rRunning port maintenance...\r"
	    "\r"
	    " 3 *** ports contracted the plague and lost productivity! ***\r";
	static const uint8_t draws[] = {
		0x00, 0x00, 0x40,
		0x00, 0x00, 0x80,
		0x00, 0x00, 0xc0
	};
	struct score_random_script script = {draws, sizeof(draws), 0U};
	struct yt_maintenance_output_result output;
	struct yt_maintenance_port_result mutation;
	struct yt_random random;
	struct yt_port port;
	struct yt_error error;

	if (!yt_maintenance_compose_port_phase(NULL,
	    0U, 0, &output) || output.row_count != 2U
	    || output.output_length != sizeof(zero_output) - 1U
	    || memcmp(output.output, zero_output, sizeof(zero_output) - 1U) != 0
	    || output.rows[0].address != 0x07A3U
	    || output.rows[1].address != 0x07B5U
	    || !yt_maintenance_compose_port_phase(
	    NULL, 0U, 3, &output)
	    || output.row_count != 4U
	    || output.output_length != sizeof(plague_output) - 1U
	    || memcmp(output.output, plague_output,
	    sizeof(plague_output) - 1U) != 0
	    || output.rows[2].address != 0x0F62U
	    || output.rows[3].address != 0x0F94U
	    || yt_maintenance_compose_port_phase(NULL, 1U, 0, &output)
	    || yt_maintenance_compose_port_phase(NULL, 0U, -1, &output)
	    || yt_maintenance_compose_port_phase(NULL, 0U, 1001, &output))
		return false;

	memset(&port, 0, sizeof(port));
	port.production[0] = 600.0f;
	port.production[1] = 1000.0f;
	port.production[2] = 2000.0f;
	port.stock[0] = 6000.0f;
	port.stock[1] = 10000.0f;
	port.stock[2] = 20000.0f;
	port.factor[0] = 1.0f;
	port.factor[1] = -1.0f;
	port.factor[2] = -1.0f;
	port.commodity_class = 3.0f;
	port.last_day = 100.0f;
	port.last_minute = 720.0f;
	yt_random_init(&random);
	yt_random_set_provider(&random, score_random_fill, &script);
	yt_error_clear(&error);
	if (!yt_maintenance_update_port(&random, &port, 105.0f, 720.0f,
	    &mutation, &error) || mutation.elapsed != 5.0f
	    || mutation.plagued || mutation.selected_stock_index != 0
	    || mutation.draws_consumed != 0U || random.draws != 0U
	    || port.stock[0] != 9000.0f || port.stock[1] != 15000.0f
	    || port.stock[2] != 30000.0f
	    || port.production[0] != 900.0f
	    || port.production[1] != 1500.0f
	    || port.production[2] != 3000.0f
	    || port.last_day != 105.0f || port.last_minute != 720.0f)
		return false;

	memset(&port, 0, sizeof(port));
	port.production[0] = 10000000.0f;
	port.production[1] = 5000000.0f;
	port.production[2] = 2000000.0f;
	port.stock[0] = 100000000.0f;
	port.stock[1] = 50000000.0f;
	port.stock[2] = 20000000.0f;
	port.factor[0] = -2.0f;
	port.factor[1] = 3.0f;
	port.factor[2] = -4.0f;
	port.commodity_class = 1.0f;
	port.last_day = 1.0f;
	script.position = 0U;
	yt_random_set_provider(&random, score_random_fill, &script);
	if (!yt_maintenance_update_port(&random, &port, 1.0f, 0.0f,
	    &mutation, &error) || !mutation.plagued
	    || mutation.selected_stock_index != 1
	    || mutation.draws_consumed != 3U || random.draws != 3U
	    || script.position != sizeof(draws)
	    || port.production[0] != 2500500.0f
	    || port.production[1] != 2500500.0f
	    || port.production[2] != 1500500.0f
	    || port.stock[0] != 25005000.0f
	    || port.stock[1] != 25005000.0f
	    || port.stock[2] != 15005000.0f
	    || port.commodity_class != 3.0f
	    || port.factor[0] != 2.0f || port.factor[1] != -3.0f
	    || port.factor[2] != -4.0f)
		return false;
	return !yt_maintenance_update_port(NULL, &port, 1.0f, 0.0f,
	    &mutation, &error);
}

static bool
check_maintenance_mercenary_output(void)
{
	static const uint8_t expected[] =
	    "\r\r"
	    "  -  The goverment has collected 100 credits tax from the ports.\r"
	    "\rMercenary Maintenance...\r\r"
	    "Checking for Mercenary Planet.. Rebuild if missing\r"
	    "\r"
	    "  -  The Mercenaries have built a home base using a captured "
	    "Genesis Device!\r"
	    "  -  The government has hired 100 mercenaries to help Fight the "
	    "Xannor!\r";
	static const uint8_t movement[] =
	    "  -  10 Mercenaries moving from sector 8 \r";
	struct yt_maintenance_output_result output;

	if (!yt_maintenance_compose_mercenary_phase(
	    NULL, 0U, 100.0f, true, 100.0f,
	    &output)
	    || output.row_count != 10U
	    || output.output_length != sizeof(expected) - 1U
	    || memcmp(output.output, expected, sizeof(expected) - 1U) != 0
	    || output.final_column != 0U
	    || output.rows[0].address != 0x3F86U
	    || output.rows[1].address != 0x3F95U
	    || output.rows[2].address != 0x40BCU
	    || output.rows[3].address != 0x40DEU
	    || output.rows[4].address != 0x40F2U
	    || output.rows[5].address != 0x4103U
	    || output.rows[6].address != 0x4130U
	    || output.rows[7].address != 0x41DEU
	    || output.rows[8].address != 0x41FAU
	    || output.rows[9].address != 0x4631U)
		return false;
	if (!yt_maintenance_compose_mercenary_phase(NULL, 0U, 0.0f,
	    false, 0.0f, &output) || output.row_count != 6U
	    || output.rows[2].address != 0x40DEU
	    || output.rows[5].address != 0x4130U)
		return false;
	if (!yt_maintenance_compose_mercenary_movement(10.0, 8.0f, &output)
	    || output.row_count != 1U
	    || output.rows[0].address != 0x4911U
	    || output.output_length != sizeof(movement) - 1U
	    || memcmp(output.output, movement, sizeof(movement) - 1U) != 0
	    || output.final_column != 0U
	    || yt_maintenance_compose_mercenary_movement(10.0, 8.0f, NULL))
		return false;
	return !yt_maintenance_compose_mercenary_phase(NULL, 1U, 0.0f,
	    false, 0.0f, &output)
	    && !yt_maintenance_compose_mercenary_phase(NULL, 0U, 0.0f,
	    false, 0.0f, NULL)
	    && yt_maintenance_mercenary_stays(1.0f,
	    0.6600000262260437f - 0.000001f)
	    && !yt_maintenance_mercenary_stays(1.0f,
	    0.6600000262260437f)
	    && !yt_maintenance_mercenary_stays(0.0f, 0.0f)
	    && !yt_maintenance_mercenary_stays(-1.0f, 0.0f)
	    && yt_maintenance_mercenary_attacks(-1.0f, 0.0f)
	    && yt_maintenance_mercenary_attacks(-3.0f, 0.0f)
	    && yt_maintenance_mercenary_attacks(2.0f, 0.950001f)
	    && !yt_maintenance_mercenary_attacks(2.0f,
	    0.949999988079071f);
}

static bool
check_maintenance_planet_model(void)
{
	static const uint8_t heading[] =
	    "\rRunning planet maintenance...\r";
	static const uint8_t civil_output[] =
	    "\rRunning planet maintenance...\r"
	    "\r"
	    "  -  CIVIL WAR has struck planet Xannoron as a result of "
	    "overcrowding!\r"
	    "  -  Productivity reduced from 600 units to 300 units!\r"
	    "  -  Ground forces reduced from 200 to 150 units!\r"
	    "  -  50 credits were spent putting down the insurrection!\r";
	static const uint8_t zero_draws[9] = {0};
	static const uint8_t civil_draws[] = {
		0x00, 0x00, 0x20, 0x00, 0x00, 0x20, 0x00, 0x00, 0xe0,
		0x00, 0x00, 0x80, 0x00, 0x00, 0x80, 0x00, 0x00, 0x80,
		0x00, 0x00, 0x80, 0x00, 0x00, 0x80, 0x00, 0x00, 0x40
	};
	static const uint8_t plague_draws[] = {
		0xff, 0xff, 0xff, 0, 0, 0, 0, 0, 0,
		0x00, 0x00, 0x80, 0x00, 0x00, 0x80, 0x00, 0x00, 0x80
	};
	struct yt_maintenance_planet_result synthetic = {
		.event = YT_MAINTENANCE_PLANET_CIVIL_WAR,
		.old_event_total = 600.0f,
		.new_event_total = 300.0f,
		.old_event_ground = 200.0f,
		.new_event_ground = 150.0f,
		.civil_war_expense = 50.0f,
		.emit_ground_line = true,
		.draws_consumed = 9U
	};
	struct yt_maintenance_text name = {
		(const uint8_t *)"Xannoron", 8U
	};
	struct score_random_script script = {
		zero_draws, sizeof(zero_draws), 0U
	};
	struct yt_maintenance_output_result output;
	struct yt_maintenance_planet_result mutation;
	struct yt_random random;
	struct yt_planet planet;
	struct yt_error error;

	if (!yt_maintenance_compose_planet_phase(
	    NULL, 0U, NULL, NULL, &output)
	    || output.row_count != 2U
	    || output.output_length != sizeof(heading) - 1U
	    || memcmp(output.output, heading, sizeof(heading) - 1U) != 0
	    || !yt_maintenance_compose_planet_phase(
	    NULL, 0U, &name, &synthetic, &output)
	    || output.row_count != 7U
	    || output.output_length != sizeof(civil_output) - 1U
	    || memcmp(output.output, civil_output,
	    sizeof(civil_output) - 1U) != 0
	    || output.rows[2].address != 0x1899U
	    || output.rows[3].address != 0x196AU
	    || output.rows[4].address != 0x19FBU
	    || output.rows[5].address != 0x1AB6U
	    || output.rows[6].address != 0x1B0BU)
		return false;

	memset(&planet, 0, sizeof(planet));
	planet.production[0] = 100.0f;
	planet.production[1] = 200.0f;
	planet.production[2] = 300.0f;
	planet.last_day = 1.0f;
	yt_random_init(&random);
	yt_random_set_provider(&random, score_random_fill, &script);
	yt_error_clear(&error);
	if (!yt_maintenance_update_planet(&random, &planet, 2.0f, 0.0f,
	    &mutation, &error)
	    || mutation.event != YT_MAINTENANCE_PLANET_NO_EVENT
	    || mutation.elapsed != 1.0f || mutation.draws_consumed != 3U
	    || planet.production[0] != 101.0f
	    || planet.production[1] != 202.0f
	    || planet.production[2] != 303.0f
	    || planet.stock[0] != 101.0f || planet.stock[1] != 202.0f
	    || planet.stock[2] != 303.0f || planet.fighters != 600.0f
	    || random.draws != 3U)
		return false;

	memset(&planet, 0, sizeof(planet));
	planet.production[0] = 100.0f;
	planet.production[1] = 200.0f;
	planet.production[2] = 300.0f;
	planet.stock[0] = 1000.0f;
	planet.stock[1] = 2000.0f;
	planet.stock[2] = 3000.0f;
	planet.bank = 1000000.0f;
	planet.ground_forces = 20000000.0f;
	planet.last_day = 1.0f;
	script = (struct score_random_script){
		civil_draws, sizeof(civil_draws), 0U
	};
	yt_random_set_provider(&random, score_random_fill, &script);
	if (!yt_maintenance_update_planet(&random, &planet, 1.0f, 0.0f,
	    &mutation, &error)
	    || mutation.event != YT_MAINTENANCE_PLANET_CIVIL_WAR
	    || mutation.draws_consumed != 9U
	    || mutation.old_event_total != 600.0f
	    || mutation.new_event_total != 300.0f
	    || mutation.old_event_ground != 20000000.0f
	    || mutation.new_event_ground != 15000000.0f
	    || mutation.civil_war_expense != 250000.0f
	    || !mutation.emit_ground_line
	    || planet.production[0] != 50.0f
	    || planet.production[1] != 100.0f
	    || planet.production[2] != 150.0f
	    || planet.stock[0] != 500.0f || planet.stock[1] != 1000.0f
	    || planet.stock[2] != 1500.0f || planet.bank != 750000.0f
	    || planet.ground_forces != 15000000.0f
	    || script.position != sizeof(civil_draws))
		return false;
	memset(&planet, 0, sizeof(planet));
	planet.production[0] = 10000000.0f;
	planet.production[1] = 5000000.0f;
	planet.production[2] = 2000000.0f;
	planet.last_day = 1.0f;
	script = (struct score_random_script){
		plague_draws, sizeof(plague_draws), 0U
	};
	yt_random_set_provider(&random, score_random_fill, &script);
	if (!yt_maintenance_update_planet(&random, &planet, 1.0f, 0.0f,
	    &mutation, &error)
	    || mutation.event != YT_MAINTENANCE_PLANET_PLAGUE
	    || mutation.draws_consumed != 6U || mutation.emit_ground_line
	    || mutation.civil_war_expense != 0.0f
	    || planet.production[0] != 5000000.0f
	    || planet.production[1] != 2500000.0f
	    || planet.production[2] != 1000000.0f
	    || script.position != sizeof(plague_draws)
	    || !yt_maintenance_compose_planet_phase(
	    NULL, 0U, &name, &mutation, &output)
	    || output.row_count != 5U
	    || output.rows[3].length < sizeof("  -  A PLAGUE") - 1U
	    || memcmp(output.rows[3].data, "  -  A PLAGUE",
	    sizeof("  -  A PLAGUE") - 1U) != 0
	    || output.rows[4].address != 0x19FBU)
		return false;
	return !yt_maintenance_update_planet(NULL, &planet, 1.0f, 0.0f,
	    &mutation, &error);
}

static bool
check_maintenance_wanderer_model(void)
{
	static const uint8_t existing[] =
	    "\rMoving The Wanderer (Planet #1)\r"
	    "\rWanderer has successfully warped!\r";
	static const uint8_t rebuilt[] =
	    "\rMoving The Wanderer (Planet #1)\r"
	    "  -  The Wanderer is missing or has been destroyed!\r"
	    "  -  The Wanderer regenerated with P.H.O.E.N.I.X. device!\r"
	    "\rWanderer has successfully warped!\r";
	struct yt_maintenance_output_result output;

	if (!yt_maintenance_compose_wanderer_phase(
	    NULL, 0U, false, &output)
	    || output.row_count != 4U
	    || output.output_length != sizeof(existing) - 1U
	    || memcmp(output.output, existing, sizeof(existing) - 1U) != 0
	    || output.rows[0].address != 0x1CEBU
	    || output.rows[1].address != 0x1CFDU
	    || output.rows[2].address != 0x1F6CU
	    || output.rows[3].address != 0x1F93U
	    || !yt_maintenance_compose_wanderer_phase(
	    NULL, 0U, true, &output)
	    || output.row_count != 6U
	    || output.output_length != sizeof(rebuilt) - 1U
	    || memcmp(output.output, rebuilt, sizeof(rebuilt) - 1U) != 0
	    || output.rows[2].address != 0x1DC4U
	    || output.rows[3].address != 0x1F37U)
		return false;
	return !yt_maintenance_compose_wanderer_phase(NULL, 1U, false,
	    &output)
	    && !yt_maintenance_compose_wanderer_phase(NULL, 0U, false, NULL);
}

static bool
check_maintenance_xannor_home_model(void)
{
	static const uint8_t existing[] =
	    "\rChecking for Planet Xannor, create it if missing.\r";
	static const uint8_t rebuilt[] =
	    "\rChecking for Planet Xannor, create it if missing.\r"
	    "\r  -  The Xannor have made a Planet!\r"
	    "The Xannor home base now has a planet!\r";
	struct yt_maintenance_output_result output;

	if (!yt_maintenance_compose_xannor_home(
	    NULL, 0U, false, &output)
	    || output.row_count != 2U
	    || output.output_length != sizeof(existing) - 1U
	    || memcmp(output.output, existing, sizeof(existing) - 1U) != 0
	    || output.rows[0].address != 0x2073U
	    || output.rows[1].address != 0x2085U
	    || !yt_maintenance_compose_xannor_home(
	    NULL, 0U, true, &output)
	    || output.row_count != 5U
	    || output.output_length != sizeof(rebuilt) - 1U
	    || memcmp(output.output, rebuilt, sizeof(rebuilt) - 1U) != 0
	    || output.rows[2].address != 0x2107U
	    || output.rows[3].address != 0x2123U
	    || output.rows[4].address != 0x22C4U)
		return false;
	return !yt_maintenance_compose_xannor_home(NULL, 1U, false, &output)
	    && !yt_maintenance_compose_xannor_home(NULL, 0U, false, NULL);
}

static bool
check_maintenance_xannor_hunt_model(void)
{
	static const uint8_t ordinary[] =
	    "\rProcessing the Xannor.....\r\r"
	    "Locating Top Player... (For Groups 16 - 20 to Pick on!)\r";
	static const uint8_t selected[] =
	    "\rProcessing the Xannor.....\r\r"
	    "Locating Top Player... (For Groups 16 - 20 to Pick on!)\r"
	    "\rGroup 20 will hunt for Alice\r";
	struct yt_maintenance_text name = {(const uint8_t *)"Alice", 5U};
	struct yt_maintenance_output_result output;

	if (!yt_maintenance_compose_xannor_hunt(
	    NULL, 0U, NULL, &output)
	    || output.row_count != 4U
	    || output.output_length != sizeof(ordinary) - 1U
	    || memcmp(output.output, ordinary, sizeof(ordinary) - 1U) != 0
	    || output.rows[0].address != 0x23EAU
	    || output.rows[1].address != 0x23FEU
	    || output.rows[2].address != 0x240FU
	    || output.rows[3].address != 0x2421U
	    || !yt_maintenance_compose_xannor_hunt(
	    NULL, 0U, &name, &output)
	    || output.row_count != 6U
	    || output.output_length != sizeof(selected) - 1U
	    || memcmp(output.output, selected, sizeof(selected) - 1U) != 0
	    || output.rows[4].address != 0x25D6U
	    || output.rows[5].address != 0x2604U)
		return false;
	return !yt_maintenance_compose_xannor_hunt(NULL, 1U, NULL, &output)
	    && !yt_maintenance_compose_xannor_hunt(NULL, 0U, &name, NULL);
}

static bool
check_maintenance_xannor_target_model(void)
{
	static const uint8_t low_draw[] = {0x00, 0x00, 0x00};
	static const uint8_t high_draw[] = {0xff, 0xff, 0xff};
	struct score_random_script script = {
		low_draw, sizeof(low_draw), 0U
	};
	struct yt_maintenance_xannor_target_result target;
	struct yt_random random;
	struct yt_error error;

	yt_random_init(&random);
	yt_random_set_provider(&random, score_random_fill, &script);
	yt_error_clear(&error);
	if (!yt_maintenance_xannor_target(&random, 2004, 2, 8,
	    &target, &error)
	    || target.hunt_player != 2 || target.target_sector != 8
	    || target.replaced || target.draws_consumed != 0U
	    || random.draws != 0U || script.position != 0U)
		return false;
	if (!yt_maintenance_xannor_target(&random, 2004, 0, 2004,
	    &target, &error)
	    || target.hunt_player != 0 || target.target_sector != 8
	    || !target.replaced || target.draws_consumed != 1U
	    || random.draws != 1U || script.position != sizeof(low_draw))
		return false;
	script = (struct score_random_script){
		high_draw, sizeof(high_draw), 0U
	};
	yt_random_set_provider(&random, score_random_fill, &script);
	if (!yt_maintenance_xannor_target(&random, 2004, 2, 7,
	    &target, &error)
	    || target.hunt_player != 0 || target.target_sector != 2004
	    || !target.replaced || target.draws_consumed != 1U
	    || random.draws != 1U || script.position != sizeof(high_draw))
		return false;
	return !yt_maintenance_xannor_target(NULL, 2004, 2, 8,
	    &target, &error)
	    && !yt_maintenance_xannor_target(&random, 7, 2, 8,
	    &target, &error)
	    && !yt_maintenance_xannor_target(&random, 2004, 2, 8,
	    NULL, &error);
}

static bool
check_maintenance_xannor_regeneration_model(void)
{
	static const uint8_t expected[] =
	    "\rCalculated Dynamic Xannor Regeneration is 200 fighters.\r"
	    "\r";
	struct yt_maintenance_xannor_regeneration_result mutation;
	struct yt_maintenance_output_result output;
	float size[21] = {0};

	size[1] = 1000.0f;
	if (!yt_maintenance_xannor_regeneration(100000.0f, size, &mutation)
	    || mutation.total_before != 1000.0f
	    || mutation.ceiling != 1000.0f
	    || mutation.regeneration != 200.0
	    || mutation.group_one_after != 1200.0f
	    || !yt_maintenance_compose_xannor_regeneration(
	    NULL, 0U, mutation.regeneration, &output)
	    || output.row_count != 3U
	    || output.output_length != sizeof(expected) - 1U
	    || memcmp(output.output, expected, sizeof(expected) - 1U) != 0
	    || output.rows[0].address != 0x2A47U
	    || output.rows[1].address != 0x2A7AU
	    || output.rows[2].address != 0x2A9CU)
		return false;
	size[1] = 1001.0f;
	if (!yt_maintenance_xannor_regeneration(100000.0f, size, &mutation)
	    || mutation.total_before != 1001.0f
	    || mutation.regeneration != 0.0
	    || mutation.group_one_after != 1001.0f)
		return false;
	return !yt_maintenance_xannor_regeneration(100000.0f, NULL,
	    &mutation)
	    && !yt_maintenance_xannor_regeneration(100000.0f, size, NULL)
	    && !yt_maintenance_compose_xannor_regeneration(NULL, 1U, 0.0,
	    &output)
	    && !yt_maintenance_compose_xannor_regeneration(NULL, 0U, 0.0,
	    NULL);
}

static bool
check_maintenance_config_defaults(void)
{
	struct yt_config config;
	struct yt_config before;
	float headquarters;

	memset(&config, 0, sizeof(config));
	memset(config.scoreboard, 0xa5, sizeof(config.scoreboard));
	config.scoreboard[0] = '\0';
	config.scoreboard_length = 0.0f;
	config.epoch_year = 26.0f;
	config.turns_per_day = 777.0f;
	config.sector_offset = 51.0f;
	config.port_offset = 2055.0f;
	config.planet_offset = 3055.0f;
	config.initial_fighters = 25.0f;
	config.initial_credits = 1005.0f;
	config.initial_holds = 10.0f;
	config.retention_days = 14.0f;
	config.last_maintenance = 123.0f;
	config.local_screen = -1.0001f;
	config.total_records = 3155.0f;
	config.lottery_plays = 0.9999f;
	config.genesis_ports = 300.0f;
	config.headquarters = 99.0f;
	config.maximum_holds = 9.9999f;
	config.marker = 6324.0f;
	config.maximum_planets = 100.0f;
	before = config;
	yt_config_normalize_maintenance(&config);
	if (memcmp(config.scoreboard, "NUL\0", 4U) != 0
	    || memcmp(config.scoreboard + 4U, before.scoreboard + 4U,
	    sizeof(config.scoreboard) - 4U) != 0
	    || config.scoreboard_length != before.scoreboard_length
	    || config.local_screen != -1.0f
	    || config.lottery_plays != 1.0f
	    || config.maximum_holds != 250.0f
	    || config.epoch_year != before.epoch_year
	    || config.turns_per_day != before.turns_per_day
	    || config.headquarters != before.headquarters
	    || config.genesis_ports != before.genesis_ports
	    || config.maximum_planets != before.maximum_planets)
		return false;

	strcpy(config.scoreboard, "SCORE.TXT");
	config.local_screen = -1.0f;
	config.lottery_plays = 1.0f;
	config.maximum_holds = 10.0f;
	yt_config_normalize_maintenance(&config);
	if (strcmp(config.scoreboard, "SCORE.TXT") != 0
	    || config.local_screen != -1.0f
	    || config.lottery_plays != 1.0f
	    || config.maximum_holds != 10.0f)
		return false;
	config.local_screen = 0.0f;
	config.lottery_plays = 1234.5f;
	config.maximum_holds = 250.0f;
	yt_config_normalize_maintenance(&config);
	if (config.local_screen != 0.0f
	    || config.lottery_plays != 1234.5f
	    || config.maximum_holds != 250.0f)
		return false;
	config.local_screen = 0.0001f;
	config.maximum_holds = 250.0001f;
	yt_config_normalize_maintenance(&config);
	if (config.local_screen != -1.0f
	    || config.maximum_holds != 250.0f)
		return false;

	headquarters = -0.0f;
	if (!yt_maintenance_default_headquarters(&headquarters)
	    || headquarters != 85.0f)
		return false;
	headquarters = 0.0001f;
	return !yt_maintenance_default_headquarters(&headquarters)
	    && headquarters == 0.0001f
	    && !yt_maintenance_default_headquarters(NULL);
}

static bool
check_maintenance_player_aging(void)
{
	static const uint8_t expiry_screen[] =
	    " *** Cloaking Device Energy Expired for Alice!\r";
	static const uint8_t expiry_radio[] =
	    "Your cloaking energy ran out at 12:34:56 on 07/23/26!";
	static const uint8_t deletion_screen[] =
	    " *** Alice deleted from game\r";
	struct yt_maintenance_text name = {(const uint8_t *)"Alice", 5U};
	struct yt_maintenance_text time_text = {
	    (const uint8_t *)"12:34:56", 8U};
	struct yt_maintenance_text date_text = {
	    (const uint8_t *)"07/23/26", 8U};
	struct yt_maintenance_player_aging_result aging;
	struct yt_maintenance_player_output_result output;
	struct yt_player player;
	struct yt_maintenance_text stored_name;
	struct yt_error error;
	bool occupied;

	memset(&player, 0, sizeof(player));
	memcpy(player.record.bytes, "Alice", 5U);
	yt_error_clear(&error);
	if (!yt_maintenance_player_name(&player, &occupied, &stored_name,
	    &error) || occupied || stored_name.length != 0U)
		return false;
	player.name_length = 0.4f;
	if (!yt_maintenance_player_name(&player, &occupied, &stored_name,
	    &error) || !occupied || stored_name.length != 0U)
		return false;
	player.name_length = 5.0f;
	if (!yt_maintenance_player_name(&player, &occupied, &stored_name,
	    &error) || !occupied || stored_name.length != 5U
	    || memcmp(stored_name.data, "Alice", 5U) != 0)
		return false;
	player.name_length = -1.0f;
	if (yt_maintenance_player_name(&player, &occupied, &stored_name,
	    &error) || error.status != YT_RANGE)
		return false;

	if (!yt_maintenance_age_player(0.0f, 100.0f, 0.0f, 204.0f,
	    14.0f, &aging)
	    || aging.cloak_written || aging.cloak_expired
	    || aging.delete_player || aging.persisted_cloak != 0.0f
	    || aging.cutoff != 190.0f)
		return false;
	if (!yt_maintenance_age_player(0.02f, 190.0f, -1.0f, 204.0f,
	    14.0f, &aging)
	    || !aging.cloak_written || !aging.cloak_expired
	    || aging.delete_player || aging.persisted_cloak != 0.0f)
		return false;
	if (!yt_maintenance_age_player(-4.0f, 191.0f, -1.0f, 204.0f,
	    14.0f, &aging)
	    || aging.cached_cloak != 1.0f
	    || fabsf(aging.persisted_cloak - 0.95f) > 0.000001f
	    || aging.delete_player)
		return false;
	if (!yt_maintenance_age_player(0.0f, 190.0f, -1.0f, 204.0f,
	    14.0f, &aging) || !aging.delete_player
	    || !yt_maintenance_compose_player_aging(&name, &time_text,
	    &date_text, true, true, &output)
	    || output.screen.row_count != 1U
	    || output.screen.rows[0].address != 0x067AU
	    || output.screen.output_length != sizeof(expiry_screen) - 1U
	    || memcmp(output.screen.output, expiry_screen,
	    sizeof(expiry_screen) - 1U) != 0
	    || output.radio_length != sizeof(expiry_radio) - 1U
	    || memcmp(output.radio_message, expiry_radio,
	    sizeof(expiry_radio) - 1U) != 0
	    || output.deletion_reached)
		return false;
	if (!yt_maintenance_compose_player_aging(&name, &time_text,
	    &date_text, false, true, &output)
	    || output.screen.row_count != 1U
	    || output.screen.rows[0].address != 0x0749U
	    || output.screen.output_length != sizeof(deletion_screen) - 1U
	    || memcmp(output.screen.output, deletion_screen,
	    sizeof(deletion_screen) - 1U) != 0
	    || output.radio_length != 0U || !output.deletion_reached)
		return false;
	return yt_maintenance_compose_player_aging(&name, &time_text,
	    &date_text, false, false, &output)
	    && output.screen.row_count == 0U && output.radio_length == 0U
	    && !yt_maintenance_age_player(0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
	    NULL)
	    && !yt_maintenance_compose_player_aging(NULL, &time_text,
	    &date_text, false, false, &output);
}

static bool
check_maintenance_headquarters_write(void)
{
	struct yt_database database;
	struct yt_config config;
	struct yt_error error;
	bool valid = false;

	(void)remove("YTDATA.DAT");
	memset(&database, 0, sizeof(database));
	memset(&config, 0, sizeof(config));
	yt_record_blank(&config.record);
	config.epoch_year = 26.0f;
	config.sector_offset = 1.0f;
	config.port_offset = 1.0f;
	config.planet_offset = 1.0f;
	config.total_records = 1.0f;
	config.local_screen = 9.0f;
	config.lottery_plays = -2.0f;
	config.maximum_holds = 300.0f;
	config.headquarters = 0.0f;
	yt_error_clear(&error);
	if (!yt_database_open(&database, "YTDATA.DAT", YT_OPEN_CREATE,
	    &error) || !yt_config_store(&database, &config, &error))
		goto done;
	yt_database_close(&database);
	memset(&database, 0, sizeof(database));
	yt_error_clear(&error);
	if (yt_maintenance_run(&error) || error.status != YT_RANGE
	    || strcmp(error.operation, "maintenance layout") != 0
	    || !yt_database_open(&database, "YTDATA.DAT", YT_OPEN_READ,
	    &error) || !yt_config_load(&database, &config, &error))
		goto done;
	valid = config.headquarters == 85.0f
	    && config.scoreboard[0] == '\0'
	    && config.local_screen == 9.0f
	    && config.lottery_plays == -2.0f
	    && config.maximum_holds == 300.0f;

done:
	yt_database_close(&database);
	(void)remove("YTDATA.DAT");
	return valid;
}

static bool
check_maintenance_protected_mines(void)
{
	struct yt_game game;
	struct yt_record before[8];
	struct yt_record after;
	struct yt_error error;
	int sector;
	bool valid = false;

	(void)remove("YTDATA.DAT");
	memset(&game, 0, sizeof(game));
	game.config.sector_offset = 1.0f;
	yt_error_clear(&error);
	if (!yt_database_open(&game.database, "YTDATA.DAT", YT_OPEN_CREATE,
	    &error))
		goto done;
	for (sector = 1; sector <= 8; ++sector) {
		memset(before[sector - 1].bytes, 0x40 + sector,
		    sizeof(before[sector - 1].bytes));
		yt_record_set_number(&before[sector - 1], YT_F129,
		    (float)(sector * 11));
		if (!yt_database_write(&game.database,
		    (size_t)yt_sector_basic_record(&game.config, sector),
		    &before[sector - 1], &error))
			goto done;
	}
	if (!yt_maintenance_clear_protected_mines(&game, &error))
		goto done;
	for (sector = 1; sector <= 8; ++sector) {
		if (!yt_database_read(&game.database,
		    (size_t)yt_sector_basic_record(&game.config, sector),
		    &after, &error))
			goto done;
		if (sector <= 7) {
			if (yt_record_get_number(&after, YT_F129) != 0.0f
			    || memcmp(after.bytes, before[sector - 1].bytes,
			    YT_F129) != 0
			    || memcmp(after.bytes + YT_F129 + 4U,
			    before[sector - 1].bytes + YT_F129 + 4U,
			    YT_RECORD_SIZE - YT_F129 - 4U) != 0)
				goto done;
		}
		else if (memcmp(after.bytes, before[sector - 1].bytes,
		    YT_RECORD_SIZE) != 0)
			goto done;
	}
	valid = true;

done:
	yt_game_close(&game);
	(void)remove("YTDATA.DAT");
	return valid && !yt_maintenance_clear_protected_mines(NULL, &error);
}

struct score_clock_script {
	struct yt_clock_value values[8];
	size_t position;
};

struct score_line_tape {
	uint8_t data[2048];
	size_t length;
	unsigned lines;
};

struct score_progress_tape {
	unsigned phases[4];
	size_t count;
};

static bool
score_progress_collect(void *context, unsigned phase,
    struct yt_error *error)
{
	struct score_progress_tape *tape = context;

	if (tape == NULL || tape->count >= YT_ARRAY_LEN(tape->phases)) {
		if (error != NULL)
			error->status = YT_RANGE;
		return false;
	}
	tape->phases[tape->count++] = phase;
	return true;
}

static bool
score_line_collect(void *context, const uint8_t *line, size_t length,
    struct yt_error *error)
{
	struct score_line_tape *tape = context;

	if (tape == NULL || (line == NULL && length != 0U)
	    || length + 1U > sizeof(tape->data) - tape->length) {
		if (error != NULL)
			error->status = YT_RANGE;
		return false;
	}
	memcpy(tape->data + tape->length, line, length);
	tape->length += length;
	tape->data[tape->length++] = '\r';
	++tape->lines;
	return true;
}

static bool
score_clock_read(void *context, struct yt_clock_value *value,
    struct yt_error *error)
{
	struct score_clock_script *script = context;

	if (script->position >= YT_ARRAY_LEN(script->values)) {
		if (error != NULL)
			error->status = YT_IO_ERROR;
		return false;
	}
	*value = script->values[script->position++];
	return true;
}

static bool
check_maintenance_header_writer(void)
{
	static const uint8_t expected[] =
	    "22:47:29 08-23-2031: Maintenance Program Ran "
	    "(Revision 03/14/94)\r\n\x1a";
	struct score_clock_script script = {{
		{2026, 7, 22, 22, 47, 29, 0},
		{2031, 8, 23, 1, 2, 3, 0}
	}, 0};
	struct yt_text_file text = {0};
	struct yt_error error;
	bool valid = false;

	(void)remove("YTNEWS.DAT");
	yt_platform_set_clock_provider(score_clock_read, &script);
	yt_error_clear(&error);
	if (!yt_maintenance_write_header(&error)
	    || script.position != 2U
	    || !yt_text_read("YTNEWS.DAT", &text, &error))
		goto done;
	valid = text.length == sizeof(expected) - 1U
	    && memcmp(text.data, expected, sizeof(expected) - 1U) == 0;

done:
	yt_text_free(&text);
	yt_platform_set_clock_provider(NULL, NULL);
	(void)remove("YTNEWS.DAT");
	return valid;
}

static bool
check_maintenance_player_pass(void)
{
	static const uint8_t expected_line[] =
	    " *** Cloaking Device Energy Expired for Alice!\r";
	static const uint8_t expected_news[] =
	    " *** Cloaking Device Energy Expired for Alice!\r\n\x1a";
	static const uint8_t expected_radio[] =
	    "Your cloaking energy ran out at 12:34:56 on 08-23-2031!";
	struct score_clock_script script = {{
		{2026, 7, 23, 12, 34, 56, 0},
		{2031, 8, 23, 1, 2, 3, 0}
	}, 0};
	struct score_line_tape screen = {0};
	struct yt_game game;
	struct yt_player player;
	struct yt_record seed;
	struct yt_record before[4];
	struct yt_record after;
	struct yt_radio_record radio;
	struct yt_text_file news = {0};
	struct yt_error error;
	float sector_cache[6];
	float cloak_cache[6];
	FILE *file = NULL;
	int record;
	bool valid = false;

	(void)remove("YTDATA.DAT");
	(void)remove("YTNEWS.DAT");
	(void)remove("YTRMSG.DAT");
	memset(&game, 0, sizeof(game));
	game.config.sector_offset = 5.0f;
	game.config.port_offset = 12.0f;
	game.config.planet_offset = 13.0f;
	game.config.total_records = 14.0f;
	game.config.retention_days = 14.0f;
	yt_error_clear(&error);
	if (!yt_database_open(&game.database, "YTDATA.DAT", YT_OPEN_CREATE,
	    &error))
		goto done;
	for (record = 2; record <= 5; ++record) {
		yt_record_blank(&seed);
		yt_player_decode(&player, &seed);
		player.sector = (float)(record * 11);
		player.last_active = record == 4 ? 0.0f : 204.0f;
		player.killed_by = record >= 4 ? -1.0f : 0.0f;
		player.cloak = record == 2 ? 9.0f : record == 3 ? 0.0f
		    : record == 4 ? 0.02f : -4.0f;
		if (record != 2) {
			const char *value = record == 3 ? "Stay"
			    : record == 4 ? "Alice" : "Neg";

			strcpy(player.name, value);
			player.name_length = (float)strlen(value);
		}
		memset(player.record.bytes + YT_RECORD_TAIL_OFFSET,
		    0xa0 + record, YT_RECORD_TAIL_SIZE);
		if (!yt_game_write_player(&game, record, &player, &error)
		    || !yt_database_read(&game.database, (size_t)record,
		    &before[record - 2], &error))
			goto done;
	}
	for (record = 0; record < 6; ++record) {
		sector_cache[record] = -99.0f;
		cloak_cache[record] = -99.0f;
	}
	yt_platform_set_clock_provider(score_clock_read, &script);
	if (!yt_maintenance_maintain_players(&game, sector_cache, cloak_cache,
	    YT_ARRAY_LEN(sector_cache), 204, score_line_collect, &screen,
	    &error) || script.position != 2U
	    || screen.length != sizeof(expected_line) - 1U
	    || memcmp(screen.data, expected_line,
	    sizeof(expected_line) - 1U) != 0
	    || sector_cache[2] != -99.0f || cloak_cache[2] != -99.0f
	    || sector_cache[3] != 33.0f || cloak_cache[3] != 0.0f
	    || sector_cache[4] != 44.0f || cloak_cache[4] != 0.02f
	    || sector_cache[5] != 55.0f || cloak_cache[5] != 1.0f)
		goto done;
	for (record = 2; record <= 5; ++record) {
		if (!yt_database_read(&game.database, (size_t)record, &after,
		    &error))
			goto done;
		if (record <= 3) {
			if (memcmp(after.bytes, before[record - 2].bytes,
			    YT_RECORD_SIZE) != 0)
				goto done;
		}
		else if (memcmp(after.bytes, before[record - 2].bytes,
		    YT_F125) != 0
		    || memcmp(after.bytes + YT_F129,
		    before[record - 2].bytes + YT_F129,
		    YT_RECORD_SIZE - YT_F129) != 0
		    || (record == 4
		    ? yt_record_get_number(&after, YT_F125) != 0.0f
		    : fabsf(yt_record_get_number(&after, YT_F125) - 0.95f)
		    > 0.000001f))
			goto done;
	}
	if (!yt_text_read("YTNEWS.DAT", &news, &error)
	    || news.length != sizeof(expected_news) - 1U
	    || memcmp(news.data, expected_news, sizeof(expected_news) - 1U)
	    != 0)
		goto done;
	file = fopen("YTRMSG.DAT", "rb");
	if (file == NULL || fread(radio.bytes, 1, sizeof(radio.bytes), file)
	    != sizeof(radio.bytes) || fgetc(file) != EOF || ferror(file)
	    || yt_radio_get_number(&radio, 0) != 1.0f
	    || yt_radio_get_number(&radio, 4) != 4.0f
	    || yt_radio_get_number(&radio, 8) != -2.0f
	    || memcmp(radio.bytes + 12U, expected_radio,
	    sizeof(expected_radio) - 1U) != 0)
		goto done;
	valid = true;

done:
	if (file != NULL)
		(void)fclose(file);
	yt_text_free(&news);
	yt_platform_set_clock_provider(NULL, NULL);
	yt_game_close(&game);
	(void)remove("YTDATA.DAT");
	(void)remove("YTNEWS.DAT");
	(void)remove("YTRMSG.DAT");
	return valid;
}

static bool
port_maintenance_owned_byte(size_t offset)
{
	return (offset >= YT_F41 && offset < YT_F85)
	    || (offset >= YT_F101 && offset < YT_F105);
}

static void
seed_maintenance_port(struct yt_record *record, bool plagued)
{
	static const float ordinary_production[3] = {600.0f, 1000.0f, 2000.0f};
	static const float ordinary_stock[3] = {6000.0f, 10000.0f, 20000.0f};
	static const float plague_production[3] = {
		10000000.0f, 5000000.0f, 2000000.0f
	};
	static const float plague_stock[3] = {
		100000000.0f, 50000000.0f, 20000000.0f
	};
	static const float factors[3] = {-2.0f, 3.0f, -4.0f};
	const float *production = plagued ? plague_production
	    : ordinary_production;
	const float *stock = plagued ? plague_stock : ordinary_stock;
	int commodity;

	memset(record->bytes, 0xa5, sizeof(record->bytes));
	record->bytes[3] = 0;
	yt_record_set_number(record, YT_F41, plagued ? 1.0f : 3.0f);
	if (!plagued) {
		record->bytes[YT_F41] = 1U;
		record->bytes[YT_F41 + 1U] = 2U;
		record->bytes[YT_F41 + 2U] = 3U;
		record->bytes[YT_F41 + 3U] = 0U;
	}
	yt_record_set_number(record, YT_F45, plagued ? 234.0f : 233.0f);
	for (commodity = 0; commodity < 3; ++commodity) {
		yt_record_set_number(record, YT_F49 + (size_t)commodity * 4U,
		    stock[commodity]);
		yt_record_set_number(record, YT_F61 + (size_t)commodity * 4U,
		    production[commodity]);
		yt_record_set_number(record, YT_F73 + (size_t)commodity * 4U,
		    factors[commodity]);
	}
	yt_record_set_number(record, YT_F101, plagued ? 720.0f : 780.0f);
}

static bool
check_maintenance_port_pass(void)
{
	static const uint8_t random_bytes[] = {
		0x00, 0x00, 0x40,
		0x00, 0x00, 0x80,
		0x00, 0x00, 0xc0
	};
	static const uint8_t expected_screen[] =
	    "\rRunning port maintenance...\r"
	    "\r"
	    " 1 *** ports contracted the plague and lost productivity! ***\r";
	static const uint8_t expected_news[] =
	    " 1 *** ports contracted the plague and lost productivity! ***\r\n\x1a";
	struct score_random_script random_script = {
		random_bytes, sizeof(random_bytes), 0U
	};
	struct score_clock_script clock_script = {{
		{2026, 8, 22, 0, 0, 0, 0},
		{2026, 8, 22, 12, 0, 0, 0},
		{2026, 8, 22, 0, 0, 0, 0},
		{2026, 8, 22, 13, 0, 0, 0}
	}, 0U};
	struct score_line_tape screen = {0};
	struct yt_text_file news = {0};
	struct yt_record before[2];
	struct yt_record after[2];
	struct yt_game game;
	struct yt_error error;
	int plagued = -1;
	int record;
	size_t offset;
	bool valid = false;

	(void)remove("YTDATA.DAT");
	(void)remove("YTNEWS.DAT");
	memset(&game, 0, sizeof(game));
	game.config.port_offset = 1.0f;
	game.config.planet_offset = 3.0f;
	game.config.epoch_year = 26.0f;
	yt_random_init(&game.random);
	yt_random_set_provider(&game.random, score_random_fill, &random_script);
	yt_error_clear(&error);
	if (!yt_database_open(&game.database, "YTDATA.DAT", YT_OPEN_CREATE,
	    &error))
		goto done;
	for (record = 0; record < 2; ++record) {
		seed_maintenance_port(&before[record], record == 0);
		if (!yt_database_write(&game.database, (size_t)record + 2U,
		    &before[record], &error))
			goto done;
	}
	yt_platform_set_clock_provider(score_clock_read, &clock_script);
	if (!yt_maintenance_maintain_ports(&game,
	    NULL, 0U, score_line_collect, &screen,
	    &plagued, &error) || plagued != 1
	    || clock_script.position != 4U
	    || random_script.position != sizeof(random_bytes)
	    || game.random.draws != 3U
	    || screen.lines != 4U
	    || screen.length != sizeof(expected_screen) - 1U
	    || memcmp(screen.data, expected_screen,
	    sizeof(expected_screen) - 1U) != 0)
		goto done;
	for (record = 0; record < 2; ++record) {
		if (!yt_database_read(&game.database, (size_t)record + 2U,
		    &after[record], &error))
			goto done;
		for (offset = 0; offset < YT_RECORD_SIZE; ++offset) {
			if (!port_maintenance_owned_byte(offset)
			    && after[record].bytes[offset]
			    != before[record].bytes[offset])
				goto done;
		}
	}
	if (yt_record_get_number(&after[0], YT_F41) != 3.0f
	    || yt_record_get_number(&after[0], YT_F45) != 234.0f
	    || yt_record_get_number(&after[0], YT_F49) != 25005000.0f
	    || yt_record_get_number(&after[0], YT_F53) != 25005000.0f
	    || yt_record_get_number(&after[0], YT_F57) != 15005000.0f
	    || yt_record_get_number(&after[0], YT_F61) != 2500500.0f
	    || yt_record_get_number(&after[0], YT_F65) != 2500500.0f
	    || yt_record_get_number(&after[0], YT_F69) != 1500500.0f
	    || yt_record_get_number(&after[0], YT_F73) != 2.0f
	    || yt_record_get_number(&after[0], YT_F77) != -3.0f
	    || yt_record_get_number(&after[0], YT_F81) != -4.0f
	    || yt_record_get_number(&after[0], YT_F101) != 720.0f
	    || yt_record_get_number(&after[1], YT_F45) != 234.0f
	    || yt_record_get_number(&after[1], YT_F41) != 0.0f
	    || after[1].bytes[YT_F41] != 0U
	    || after[1].bytes[YT_F41 + 1U] != 0U
	    || after[1].bytes[YT_F41 + 2U] != 0U
	    || after[1].bytes[YT_F41 + 3U] != 0U
	    || yt_record_get_number(&after[1], YT_F49) != 6600.0f
	    || yt_record_get_number(&after[1], YT_F53) != 11000.0f
	    || yt_record_get_number(&after[1], YT_F57) != 22000.0f
	    || yt_record_get_number(&after[1], YT_F61) != 660.0f
	    || yt_record_get_number(&after[1], YT_F65) != 1100.0f
	    || yt_record_get_number(&after[1], YT_F69) != 2200.0f
	    || yt_record_get_number(&after[1], YT_F101) != 780.0f
	    || !yt_text_read("YTNEWS.DAT", &news, &error)
	    || news.length != sizeof(expected_news) - 1U
	    || memcmp(news.data, expected_news, sizeof(expected_news) - 1U)
	    != 0)
		goto done;
	valid = true;

done:
	yt_text_free(&news);
	yt_platform_set_clock_provider(NULL, NULL);
	yt_game_close(&game);
	(void)remove("YTDATA.DAT");
	(void)remove("YTNEWS.DAT");
	return valid;
}

static bool
check_maintenance_mercenary_tax_pass(void)
{
	static const float treasuries[] = {99.0f, 100.0f, 101.0f};
	static const float expected_treasuries[] = {90.0f, 90.0f, 91.0f};
	static const uint8_t dirty_zero[4] = {0x00, 0x00, 0x20, 0x00};
	struct yt_maintenance_mercenary_tax_result tax;
	struct yt_record before[4];
	struct yt_record after;
	struct yt_record expected;
	struct yt_game game;
	struct yt_error error;
	int record;
	bool valid = false;

	(void)remove("YTDATA.DAT");
	memset(&game, 0, sizeof(game));
	game.config.port_offset = 1.0f;
	yt_error_clear(&error);
	if (!yt_database_open(&game.database, "YTDATA.DAT", YT_OPEN_CREATE,
	    &error))
		goto done;
	for (record = 0; record < 4; ++record) {
		yt_record_blank(&before[record]);
		before[record].bytes[YT_RECORD_TAIL_OFFSET] =
		    (uint8_t)(0x90 + record);
		if ((record < 3
		    && !yt_record_set_number(&before[record], YT_F89,
		    treasuries[record]))
		    || (record == 3
		    && !yt_record_set_raw_number(&before[record], YT_F89,
		    dirty_zero))
		    || !yt_database_write(&game.database, (size_t)record + 2U,
		    &before[record], &error))
			goto done;
	}
	if (!yt_maintenance_collect_mercenary_tax(&game, 4, &tax, &error)
	    || tax.tax_pool != 29.0f || tax.fleet_strength != 2.0f
	    || tax.taxed_ports != 3)
		goto done;
	for (record = 0; record < 4; ++record) {
		if (!yt_database_read(&game.database, (size_t)record + 2U,
		    &after, &error))
			goto done;
		expected = before[record];
		if (record < 3
		    && !yt_record_set_number(&expected, YT_F89,
		    expected_treasuries[record]))
			goto done;
		if (memcmp(after.bytes, expected.bytes, YT_RECORD_SIZE) != 0)
			goto done;
	}
	if (!yt_maintenance_collect_mercenary_tax(&game, 0, &tax, &error)
	    || tax.tax_pool != 0.0f || tax.fleet_strength != 0.0f
	    || tax.taxed_ports != 0
	    || yt_maintenance_collect_mercenary_tax(NULL, 0, &tax, &error)
	    || yt_maintenance_collect_mercenary_tax(&game, -1, &tax, &error)
	    || yt_maintenance_collect_mercenary_tax(&game, 0, NULL, &error))
		goto done;
	valid = true;

done:
	yt_game_close(&game);
	(void)remove("YTDATA.DAT");
	return valid;
}

static bool
check_maintenance_mercenary_base_pass(void)
{
	static const uint8_t random_bytes[] = {
		0x00, 0x00, 0x00,
		0x00, 0x00, 0x80
	};
	static const uint8_t dirty_zero[4] = {0x00, 0x00, 0x20, 0x00};
	struct score_random_script script = {
		random_bytes, sizeof(random_bytes), 0U
	};
	struct score_clock_script clock_script = {{
		{2026, 7, 23, 0, 0, 0, 0}
	}, 0U};
	struct yt_record sectors[3];
	struct yt_record planet_before;
	struct yt_record planet_expected;
	struct yt_record after;
	struct yt_game game;
	struct yt_error error;
	bool rebuilt = false;
	int index;
	bool valid = false;

	(void)remove("YTDATA.DAT");
	memset(&game, 0, sizeof(game));
	game.config.sector_offset = 1.0f;
	game.config.planet_offset = 4.0f;
	game.config.epoch_year = 26.0f;
	yt_random_init(&game.random);
	yt_random_set_provider(&game.random, score_random_fill, &script);
	yt_error_clear(&error);
	if (!yt_database_open(&game.database, "YTDATA.DAT", YT_OPEN_CREATE,
	    &error))
		goto done;
	for (index = 0; index < 3; ++index) {
		yt_record_blank(&sectors[index]);
		sectors[index].bytes[YT_RECORD_TAIL_OFFSET] =
		    (uint8_t)(0x31 + index);
		if (!yt_record_set_number(&sectors[index], YT_F93,
		    index == 0 ? 7.0f : index == 1 ? -3.0f : 0.0f)
		    || !yt_database_write(&game.database, (size_t)index + 2U,
		    &sectors[index], &error))
			goto done;
	}
	yt_record_blank(&planet_before);
	memcpy(planet_before.bytes, "Old Base", 8U);
	planet_before.bytes[YT_RECORD_TAIL_OFFSET] = 0xE7U;
	if (!yt_record_set_number(&planet_before, YT_F41, 12.0f)
	    || !yt_record_set_number(&planet_before, YT_F45, 1.0f)
	    || !yt_record_set_number(&planet_before, YT_F49, 2.0f)
	    || !yt_record_set_number(&planet_before, YT_F53, 3.0f)
	    || !yt_record_set_raw_number(&planet_before, YT_F57, dirty_zero)
	    || !yt_record_set_raw_number(&planet_before, YT_F61, dirty_zero)
	    || !yt_record_set_raw_number(&planet_before, YT_F65, dirty_zero)
	    || !yt_record_set_raw_number(&planet_before, YT_F69, dirty_zero)
	    || !yt_record_set_number(&planet_before, YT_F73, 9.0f)
	    || !yt_record_set_number(&planet_before, YT_F77, 0.5f)
	    || !yt_record_set_number(&planet_before, YT_F85, 8.0f)
	    || !yt_record_set_number(&planet_before, YT_F89, 721.0f)
	    || !yt_record_set_number(&planet_before, YT_F113, 33.0f)
	    || !yt_record_set_raw_number(&planet_before, YT_F117, dirty_zero)
	    || !yt_record_set_raw_number(&planet_before, YT_F125, dirty_zero)
	    || !yt_record_set_number(&planet_before, YT_F129, 44.0f)
	    || !yt_database_write(&game.database, 8U, &planet_before, &error))
		goto done;
	planet_expected = planet_before;
	yt_record_set_text(&planet_expected,
	    (const uint8_t *)"Mercenary Base", 14U);
	if (!yt_record_set_number(&planet_expected, YT_F41, 194.0f)
	    || !yt_record_set_number(&planet_expected, YT_F45, 100000.0f)
	    || !yt_record_set_number(&planet_expected, YT_F49, 100000.0f)
	    || !yt_record_set_number(&planet_expected, YT_F53, 100000.0f)
	    || !yt_record_set_number(&planet_expected, YT_F57, 0.0f)
	    || !yt_record_set_number(&planet_expected, YT_F61, 0.0f)
	    || !yt_record_set_number(&planet_expected, YT_F65, 0.0f)
	    || !yt_record_set_number(&planet_expected, YT_F69, 0.0f)
	    || !yt_record_set_number(&planet_expected, YT_F73, -2.0f)
	    || !yt_record_set_number(&planet_expected, YT_F77, 150000.0f)
	    || !yt_record_set_number(&planet_expected, YT_F85, 14.0f)
	    || !yt_record_set_number(&planet_expected, YT_F117, 25000000.0f)
	    || !yt_record_set_number(&planet_expected, YT_F125, 0.0f))
		goto done;
	yt_platform_set_clock_provider(score_clock_read, &clock_script);
	if (!yt_maintenance_maintain_mercenary_base(&game, 3, 4,
	    &rebuilt, &error) || !rebuilt || game.random.draws != 2U
	    || script.position != sizeof(random_bytes)
	    || clock_script.position != 1U)
		goto done;
	for (index = 0; index < 3; ++index) {
		struct yt_record expected = sectors[index];

		if (index == 1
		    && !yt_record_set_number(&expected, YT_F93, 4.0f))
			goto done;
		if (!yt_database_read(&game.database, (size_t)index + 2U,
		    &after, &error)
		    || memcmp(after.bytes, expected.bytes, YT_RECORD_SIZE) != 0)
			goto done;
	}
	if (!yt_database_read(&game.database, 8U, &after, &error)
	    || memcmp(after.bytes, planet_expected.bytes, YT_RECORD_SIZE) != 0
	    || !yt_maintenance_maintain_mercenary_base(&game, 3, 4,
	    &rebuilt, &error) || rebuilt || game.random.draws != 2U
	    || clock_script.position != 1U
	    || !yt_database_read(&game.database, 8U, &after, &error)
	    || memcmp(after.bytes, planet_expected.bytes, YT_RECORD_SIZE) != 0
	    || yt_maintenance_maintain_mercenary_base(NULL, 3, 4,
	    &rebuilt, &error)
	    || yt_maintenance_maintain_mercenary_base(&game, 0, 4,
	    &rebuilt, &error)
	    || yt_maintenance_maintain_mercenary_base(&game, 3, 0,
	    &rebuilt, &error)
	    || yt_maintenance_maintain_mercenary_base(&game, 3, 4,
	    NULL, &error))
		goto done;
	valid = true;

done:
	yt_platform_set_clock_provider(NULL, NULL);
	yt_game_close(&game);
	(void)remove("YTDATA.DAT");
	return valid;
}

static bool
check_maintenance_mercenary_funding_pass(void)
{
	static const uint8_t random_bytes[] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x18,
		0x00, 0x00, 0x2F, 0x00, 0x00, 0x46,
		0x00, 0x00, 0x5E, 0x00, 0x00, 0x75,
		0x00, 0x00, 0x8C, 0x00, 0x00, 0xA3,
		0x00, 0x00, 0xBB, 0x00, 0x00, 0xD2,
		0x00, 0x00, 0xEA
	};
	static const uint8_t dirty_zero[4] = {0x00, 0x00, 0x20, 0x00};
	struct score_random_script script = {
		random_bytes, sizeof(random_bytes), 0U
	};
	struct yt_record before[12];
	struct yt_record after;
	struct yt_record expected;
	struct yt_game game;
	struct yt_error error;
	float hired = -1.0f;
	int sector;
	bool valid = false;

	(void)remove("YTDATA.DAT");
	memset(&game, 0, sizeof(game));
	game.config.sector_offset = 1.0f;
	yt_random_init(&game.random);
	yt_random_set_provider(&game.random, score_random_fill, &script);
	yt_error_clear(&error);
	if (!yt_database_open(&game.database, "YTDATA.DAT", YT_OPEN_CREATE,
	    &error))
		goto done;
	for (sector = 1; sector <= 12; ++sector) {
		yt_record_blank(&before[sector - 1]);
		before[sector - 1].bytes[YT_RECORD_TAIL_OFFSET] =
		    (uint8_t)(0x40 + sector);
		if (!yt_record_set_raw_number(&before[sector - 1], YT_F81,
		    dirty_zero)
		    || !yt_record_set_raw_number(&before[sector - 1], YT_F85,
		    dirty_zero))
			goto done;
		if (sector == 2
		    && (!yt_record_set_number(&before[sector - 1], YT_F81, 5.0f)
		    || !yt_record_set_number(&before[sector - 1], YT_F85, 7.0f)))
			goto done;
		if (!yt_database_write(&game.database, (size_t)sector + 1U,
		    &before[sector - 1], &error))
			goto done;
	}
	if (!yt_maintenance_place_mercenary_fleets(&game, 12, 2.0f,
	    &hired, &error) || hired != 20.0f || game.random.draws != 11U
	    || script.position != sizeof(random_bytes))
		goto done;
	for (sector = 1; sector <= 12; ++sector) {
		expected = before[sector - 1];
		if (sector >= 3
		    && (!yt_record_set_number(&expected, YT_F81, 2.0f)
		    || !yt_record_set_number(&expected, YT_F85, -2.0f)))
			goto done;
		if (!yt_database_read(&game.database, (size_t)sector + 1U,
		    &after, &error)
		    || memcmp(after.bytes, expected.bytes, YT_RECORD_SIZE) != 0)
			goto done;
	}
	if (!yt_maintenance_place_mercenary_fleets(&game, 12, 0.0f,
	    &hired, &error) || hired != 0.0f || game.random.draws != 11U
	    || yt_maintenance_place_mercenary_fleets(NULL, 12, 1.0f,
	    &hired, &error)
	    || yt_maintenance_place_mercenary_fleets(&game, 1, 1.0f,
	    &hired, &error)
	    || yt_maintenance_place_mercenary_fleets(&game, 12, 1.0f,
	    NULL, &error))
		goto done;
	valid = true;

done:
	yt_game_close(&game);
	(void)remove("YTDATA.DAT");
	return valid;
}

static bool
check_maintenance_mercenary_defection_pass(void)
{
	static const uint8_t random_bytes[] = {
		0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF,
		0x80, 0x80, 0x80,
		0x40, 0x40, 0x40,
		0x00, 0x00, 0x00
	};
	static const uint8_t expected_screen[] =
	    "  -  99 fighters in sector 1 belonging to A\0B joined the mercs!\r";
	static const uint8_t expected_news[] =
	    "  -  99 fighters in sector 1 belonging to A\0B joined the mercs!"
	    "\r\n\x1a";
	struct score_random_script script = {
		random_bytes, sizeof(random_bytes), 0U
	};
	struct score_line_tape screen = {0};
	struct yt_maintenance_mercenary_defection_result result;
	struct yt_text_file news = {0};
	struct yt_record before[5];
	struct yt_record owner;
	struct yt_record expected;
	struct yt_record after;
	struct yt_game game;
	struct yt_error error;
	int sector;
	bool valid = false;

	(void)remove("YTDATA.DAT");
	(void)remove("YTNEWS.DAT");
	memset(&game, 0, sizeof(game));
	game.config.sector_offset = 10.0f;
	yt_random_init(&game.random);
	yt_random_set_provider(&game.random, score_random_fill, &script);
	yt_error_clear(&error);
	if (!yt_database_open(&game.database, "YTDATA.DAT", YT_OPEN_CREATE,
	    &error))
		goto done;
	yt_record_blank(&owner);
	owner.bytes[0] = 'A';
	owner.bytes[1] = 0;
	owner.bytes[2] = 'B';
	owner.bytes[YT_RECORD_TAIL_OFFSET] = 0xD2U;
	if (!yt_record_set_number(&owner, YT_F85, 3.0f)
	    || !yt_database_write(&game.database, 2U, &owner, &error))
		goto done;
	for (sector = 1; sector <= 5; ++sector) {
		float fighters = sector == 1 ? 99.0f
		    : sector == 2 ? 100.0f : 1.0f;
		float fighter_owner = sector == 1 ? 2.3999999f
		    : sector == 2 ? 3.0f : sector == 3 ? -2.0f
		    : sector == 4 ? 1.0f : 2.0f;

		yt_record_blank(&before[sector - 1]);
		before[sector - 1].bytes[YT_RECORD_TAIL_OFFSET] =
		    (uint8_t)(0x60 + sector);
		if (!yt_record_set_number(&before[sector - 1], YT_F81, fighters)
		    || !yt_record_set_number(&before[sector - 1], YT_F85,
		    fighter_owner)
		    || !yt_database_write(&game.database, (size_t)sector + 10U,
		    &before[sector - 1], &error))
			goto done;
	}
	if (!yt_maintenance_mercenary_defections(&game, 5,
	    score_line_collect, &screen, &result, &error)
	    || result.defections != 1 || result.draws_consumed != 5U
	    || game.random.draws != 5U || script.position != sizeof(random_bytes)
	    || screen.lines != 1U
	    || screen.length != sizeof(expected_screen) - 1U
	    || memcmp(screen.data, expected_screen,
	    sizeof(expected_screen) - 1U) != 0)
		goto done;
	for (sector = 1; sector <= 5; ++sector) {
		expected = before[sector - 1];
		if (sector == 1
		    && !yt_record_set_number(&expected, YT_F85, -2.0f))
			goto done;
		if (!yt_database_read(&game.database, (size_t)sector + 10U,
		    &after, &error)
		    || memcmp(after.bytes, expected.bytes, YT_RECORD_SIZE) != 0)
			goto done;
	}
	if (!yt_text_read("YTNEWS.DAT", &news, &error)
	    || news.length != sizeof(expected_news) - 1U
	    || memcmp(news.data, expected_news, sizeof(expected_news) - 1U)
	    != 0
	    || yt_maintenance_mercenary_defections(NULL, 5,
	    score_line_collect, &screen, &result, &error)
	    || yt_maintenance_mercenary_defections(&game, 0,
	    score_line_collect, &screen, &result, &error)
	    || yt_maintenance_mercenary_defections(&game, 5, NULL, &screen,
	    &result, &error)
	    || yt_maintenance_mercenary_defections(&game, 5,
	    score_line_collect, &screen, NULL, &error))
		goto done;
	valid = true;

done:
	yt_text_free(&news);
	yt_game_close(&game);
	(void)remove("YTDATA.DAT");
	(void)remove("YTNEWS.DAT");
	return valid;
}

static bool
check_maintenance_mercenary_movement_pass(void)
{
	static const uint8_t random_bytes[] = {
		0x00, 0x00, 0x80,
		0x00, 0x00, 0xC0,
		0x00, 0x00, 0x00,
		0x00, 0x00, 0x00
	};
	static const uint8_t expected_screen[] =
	    "  -  5 Mercenaries moving from sector 1 \r"
	    " *** 5 Mercenaries attacking 1 fighters beloning to The Xannor!\r"
	    " *** The Mercenaries Won!\r";
	static const uint8_t expected_news[] =
	    " *** 5 Mercenaries attacking 1 fighters beloning to The Xannor!"
	    "\r\n *** The Mercenaries Won!\r\n\x1a";
	static const float warps[4][2] = {
		{2.0f, 0.0f},
		{3.0f, 0.0f},
		{4.0f, 0.0f},
		{0.0f, 0.0f}
	};
	struct score_random_script script = {
		random_bytes, sizeof(random_bytes), 0U
	};
	struct score_line_tape screen = {0};
	struct yt_text_file news = {0};
	struct yt_record before[4];
	struct yt_record expected;
	struct yt_record after;
	struct yt_game game;
	struct yt_error error;
	int sector;
	bool valid = false;

	(void)remove("YTDATA.DAT");
	(void)remove("YTNEWS.DAT");
	memset(&game, 0, sizeof(game));
	game.config.sector_offset = 1.0f;
	game.config.port_offset = 5.0f;
	yt_random_init(&game.random);
	yt_random_set_provider(&game.random, score_random_fill, &script);
	yt_error_clear(&error);
	if (!yt_database_open(&game.database, "YTDATA.DAT", YT_OPEN_CREATE,
	    &error))
		goto done;
	for (sector = 1; sector <= 4; ++sector) {
		yt_record_blank(&before[sector - 1]);
		before[sector - 1].bytes[YT_RECORD_TAIL_OFFSET] =
		    (uint8_t)(0x90 + sector);
		if (!yt_record_set_number(&before[sector - 1], YT_F41,
		    warps[sector - 1][0])
		    || !yt_record_set_number(&before[sector - 1], YT_F45,
		    warps[sector - 1][1])
		    || (sector == 1
		    && (!yt_record_set_number(&before[sector - 1], YT_F81, 5.0f)
		    || !yt_record_set_number(&before[sector - 1], YT_F85, -2.0f)))
		    || (sector == 2
		    && (!yt_record_set_number(&before[sector - 1], YT_F85, 2.0f)
		    || !yt_record_set_number(&before[sector - 1], YT_F93, 1.0f)))
		    || (sector == 3
		    && !yt_record_set_number(&before[sector - 1], YT_F85, -2.0f))
		    || (sector == 4
		    && (!yt_record_set_number(&before[sector - 1], YT_F81, 1.0f)
		    || !yt_record_set_number(&before[sector - 1], YT_F85, -1.0f)))
		    || !yt_database_write(&game.database, (size_t)sector + 1U,
		    &before[sector - 1], &error))
			goto done;
	}
	if (!yt_maintenance_move_mercenaries(&game, 4, score_line_collect,
	    &screen, &error)
	    || game.random.draws != 4U
	    || script.position != sizeof(random_bytes)
	    || screen.lines != 3U
	    || screen.length != sizeof(expected_screen) - 1U
	    || memcmp(screen.data, expected_screen,
	    sizeof(expected_screen) - 1U) != 0
	    || !yt_text_read("YTNEWS.DAT", &news, &error)
	    || news.length != sizeof(expected_news) - 1U
	    || memcmp(news.data, expected_news,
	    sizeof(expected_news) - 1U) != 0)
		goto done;
	for (sector = 1; sector <= 4; ++sector) {
		expected = before[sector - 1];
		if (sector == 1
		    && (!yt_record_set_number(&expected, YT_F81, 0.0f)
		    || !yt_record_set_number(&expected, YT_F85, 0.0f)))
			goto done;
		if ((sector == 3 || sector == 4)
		    && (!yt_record_set_number(&expected, YT_F81, 5.0f)
		    || !yt_record_set_number(&expected, YT_F85, -2.0f)))
			goto done;
		if (!yt_database_read(&game.database, (size_t)sector + 1U,
		    &after, &error)
		    || memcmp(after.bytes, expected.bytes, YT_RECORD_SIZE) != 0)
			goto done;
	}
	if (yt_maintenance_move_mercenaries(NULL, 4, score_line_collect,
	    &screen, &error)
	    || yt_maintenance_move_mercenaries(&game, 0, score_line_collect,
	    &screen, &error)
	    || yt_maintenance_move_mercenaries(&game, 4, NULL, &screen,
	    &error))
		goto done;
	valid = true;

done:
	yt_text_free(&news);
	yt_game_close(&game);
	(void)remove("YTDATA.DAT");
	(void)remove("YTNEWS.DAT");
	return valid;
}

static bool
check_maintenance_mercenary_lower_reentry_pass(void)
{
	static const uint8_t random_bytes[] = {
		0x00, 0x00, 0x80,
		0x00, 0x00, 0x00,
		0x00, 0x00, 0x00,
		0x00, 0x00, 0x80,
		0xFF, 0xFF, 0xFF,
		0x00, 0x00, 0x00,
		0x00, 0x00, 0xC0
	};
	static const uint8_t expected_screen[] =
	    "  -  1 Mercenaries moving from sector 3 \r"
	    " *** 1 Mercenaries joined AB's Defense force in Sector 1!\r"
	    "  -  1 Mercenaries moving from sector 2 \r"
	    " *** 1 Mercenaries attacking 2 fighters beloning to The Xannor!\r"
	    " *** The Mercenaries Lost!\r";
	struct score_random_script script = {
		random_bytes, sizeof(random_bytes), 0U
	};
	struct score_line_tape screen = {0};
	struct yt_record before[4];
	struct yt_record after;
	struct yt_game game;
	struct yt_error error;
	int sector;
	bool valid = false;

	(void)remove("YTDATA.DAT");
	(void)remove("YTNEWS.DAT");
	(void)remove("YTRMSG.DAT");
	memset(&game, 0, sizeof(game));
	game.config.sector_offset = 1.0f;
	game.config.port_offset = 5.0f;
	yt_random_init(&game.random);
	yt_random_set_provider(&game.random, score_random_fill, &script);
	yt_error_clear(&error);
	if (!yt_database_open(&game.database, "YTDATA.DAT", YT_OPEN_CREATE,
	    &error))
		goto done;
	for (sector = 1; sector <= 4; ++sector) {
		yt_record_blank(&before[sector - 1]);
		before[sector - 1].bytes[YT_RECORD_TAIL_OFFSET] =
		    (uint8_t)(0xB0 + sector);
	}
	memcpy(before[0].bytes, "AB", 2U);
	if (!yt_record_set_number(&before[0], YT_F81, 1.0f)
	    || !yt_record_set_number(&before[0], YT_F85, 2.0f)
	    || !yt_record_set_number(&before[1], YT_F41, 1.0f)
	    || !yt_record_set_number(&before[1], YT_F45, 4.0f)
	    || !yt_record_set_number(&before[1], YT_F85, -2.0f)
	    || !yt_record_set_number(&before[2], YT_F41, 2.0f)
	    || !yt_record_set_number(&before[2], YT_F81, 1.0f)
	    || !yt_record_set_number(&before[2], YT_F85, -2.0f)
	    || !yt_record_set_number(&before[3], YT_F81, 2.0f)
	    || !yt_record_set_number(&before[3], YT_F85, -1.0f))
		goto done;
	for (sector = 1; sector <= 4; ++sector) {
		if (!yt_database_write(&game.database, (size_t)sector + 1U,
		    &before[sector - 1], &error))
			goto done;
	}
	if (!yt_maintenance_move_mercenaries(&game, 4, score_line_collect,
	    &screen, &error)
	    || game.random.draws != 7U
	    || script.position != sizeof(random_bytes)
	    || screen.lines != 5U
	    || screen.length != sizeof(expected_screen) - 1U
	    || memcmp(screen.data, expected_screen,
	    sizeof(expected_screen) - 1U) != 0)
		goto done;
	for (sector = 1; sector <= 4; ++sector) {
		float fighters = sector == 1 ? 2.0f : sector == 4 ? 2.0f : 0.0f;
		float owner = sector == 1 ? 2.0f : sector == 4 ? -1.0f : 0.0f;

		if (!yt_database_read(&game.database, (size_t)sector + 1U,
		    &after, &error)
		    || yt_record_get_number(&after, YT_F81) != fighters
		    || yt_record_get_number(&after, YT_F85) != owner
		    || after.bytes[YT_RECORD_TAIL_OFFSET]
		    != before[sector - 1].bytes[YT_RECORD_TAIL_OFFSET])
			goto done;
	}
	valid = true;

done:
	yt_game_close(&game);
	(void)remove("YTDATA.DAT");
	(void)remove("YTNEWS.DAT");
	(void)remove("YTRMSG.DAT");
	return valid;
}

static bool
check_maintenance_mercenary_destination_pass(void)
{
	static const uint8_t random_bytes[] = {
		0x00, 0x00, 0x80,
		0x00, 0x00, 0x80,
		0x00, 0x00, 0x00,
		0x00, 0x00, 0x00,
		0x00, 0x00, 0x80,
		0x00, 0x00, 0xC0
	};
	static const uint8_t large_random_bytes[53U * 3U] = {0};
	static const uint8_t expected_first_screen[] =
	    " *** 6 Mercenaries joined A\0B's Defense force in Sector 2!\r"
	    " *** 5 Mercenaries attacking 2 fighters beloning to The Xannor!\r"
	    " *** The Mercenaries Won!\r"
	    " *** 1 Mercenaries attacking 2 fighters beloning to The Xannor!\r"
	    " *** The Mercenaries Lost!\r";
	static const uint8_t expected_screen[] =
	    " *** 6 Mercenaries joined A\0B's Defense force in Sector 2!\r"
	    " *** 5 Mercenaries attacking 2 fighters beloning to The Xannor!\r"
	    " *** The Mercenaries Won!\r"
	    " *** 1 Mercenaries attacking 2 fighters beloning to The Xannor!\r"
	    " *** The Mercenaries Lost!\r"
	    " *** 201 Mercenaries attacking 201 fighters beloning to The Xannor!\r"
	    " *** The Mercenaries Won!\r";
	static const uint8_t expected_news[] =
	    " *** 6 Mercenaries joined A\0B's Defense force in Sector 2!\r\n"
	    " *** 5 Mercenaries attacking 2 fighters beloning to The Xannor!\r\n"
	    " *** The Mercenaries Won!\r\n"
	    " *** 1 Mercenaries attacking 2 fighters beloning to The Xannor!\r\n"
	    " *** The Mercenaries Lost!\r\n"
	    " *** 201 Mercenaries attacking 201 fighters beloning to The Xannor!\r\n"
	    " *** The Mercenaries Won!\r\n\x1a";
	static const uint8_t expected_radio[] =
	    " 6 *** of our boys joined your defense force in Sector 2!";
	struct score_random_script script = {
		random_bytes, sizeof(random_bytes), 0U
	};
	struct score_random_script large_script = {
		large_random_bytes, sizeof(large_random_bytes), 0U
	};
	struct score_line_tape screen = {0};
	struct yt_radio_record radio;
	struct yt_text_file news = {0};
	struct yt_record player;
	struct yt_record before[5];
	struct yt_record expected;
	struct yt_record after;
	struct yt_sector arrival;
	struct yt_game game;
	struct yt_error error;
	FILE *file = NULL;
	int sector;
	bool valid = false;

	(void)remove("YTDATA.DAT");
	(void)remove("YTNEWS.DAT");
	(void)remove("YTRMSG.DAT");
	memset(&game, 0, sizeof(game));
	game.config.sector_offset = 10.0f;
	yt_random_init(&game.random);
	yt_random_set_provider(&game.random, score_random_fill, &script);
	yt_error_clear(&error);
	if (!yt_database_open(&game.database, "YTDATA.DAT", YT_OPEN_CREATE,
	    &error))
		goto done;
	yt_record_blank(&player);
	memcpy(player.bytes, "A\0B", 3U);
	player.bytes[YT_RECORD_TAIL_OFFSET] = 0xD7U;
	if (!yt_record_set_number(&player, YT_F85, 3.0f)
	    || !yt_database_write(&game.database, 2U, &player, &error))
		goto done;
	for (sector = 1; sector <= 5; ++sector) {
		yt_record_blank(&before[sector - 1]);
		before[sector - 1].bytes[YT_RECORD_TAIL_OFFSET] =
		    (uint8_t)(0xA0 + sector);
		if (!yt_record_set_number(&before[sector - 1], YT_F81,
		    sector == 1 ? 3.0f : sector == 5 ? 201.0f : 2.0f)
		    || !yt_record_set_number(&before[sector - 1], YT_F85,
		    sector == 1 ? -2.0f : sector == 2 ? 2.0f : -1.0f)
		    || !yt_database_write(&game.database, (size_t)sector + 10U,
		    &before[sector - 1], &error))
			goto done;
	}
	if (!yt_game_read_sector(&game, 1, &arrival, &error)
	    || !yt_maintenance_mercenary_destination(&game, 1, 4.0f,
	    score_line_collect, &screen, &arrival, &error)
	    || arrival.fighters != 7.0f || arrival.fighter_owner != -2.0f
	    || game.random.draws != 0U || screen.lines != 0U)
		goto done;
	if (!yt_game_read_sector(&game, 2, &arrival, &error)
	    || !yt_maintenance_mercenary_destination(&game, 2, 6.0f,
	    score_line_collect, &screen, &arrival, &error)
	    || arrival.fighters != 8.0f || arrival.fighter_owner != 2.0f
	    || game.random.draws != 1U)
		goto done;
	if (!yt_game_read_sector(&game, 3, &arrival, &error)
	    || !yt_maintenance_mercenary_destination(&game, 3, 5.0f,
	    score_line_collect, &screen, &arrival, &error)
	    || arrival.fighters != 5.0f || arrival.fighter_owner != -2.0f
	    || game.random.draws != 4U)
		goto done;
	if (!yt_game_read_sector(&game, 4, &arrival, &error)
	    || !yt_maintenance_mercenary_destination(&game, 4, 1.0f,
	    score_line_collect, &screen, &arrival, &error)
	    || arrival.fighters != 2.0f || arrival.fighter_owner != -1.0f
	    || game.random.draws != 6U
	    || script.position != sizeof(random_bytes)
	    || screen.lines != 5U
	    || screen.length != sizeof(expected_first_screen) - 1U
	    || memcmp(screen.data, expected_first_screen,
	    sizeof(expected_first_screen) - 1U) != 0)
		goto done;
	/* Both strengths begin above 200, so the first combat draw removes
	 * 150 defenders; the remaining 51 zero draws use the one-unit lane. */
	yt_random_set_provider(&game.random, score_random_fill, &large_script);
	if (!yt_game_read_sector(&game, 5, &arrival, &error)
	    || !yt_maintenance_mercenary_destination(&game, 5, 201.0f,
	    score_line_collect, &screen, &arrival, &error)
	    || arrival.fighters != 201.0f || arrival.fighter_owner != -2.0f
	    || game.random.draws != 53U
	    || large_script.position != sizeof(large_random_bytes)
	    || screen.lines != 7U
	    || screen.length != sizeof(expected_screen) - 1U
	    || memcmp(screen.data, expected_screen,
	    sizeof(expected_screen) - 1U) != 0)
		goto done;
	for (sector = 1; sector <= 5; ++sector) {
		expected = before[sector - 1];
		if (!yt_record_set_number(&expected, YT_F81,
		    sector == 1 ? 7.0f : sector == 2 ? 8.0f
		    : sector == 3 ? 5.0f : sector == 4 ? 2.0f : 201.0f)
		    || !yt_record_set_number(&expected, YT_F85,
		    sector == 1 ? -2.0f : sector == 2 ? 2.0f
		    : sector == 3 ? -2.0f : sector == 4 ? -1.0f : -2.0f)
		    || !yt_database_read(&game.database, (size_t)sector + 10U,
		    &after, &error)
		    || memcmp(after.bytes, expected.bytes, YT_RECORD_SIZE) != 0)
			goto done;
	}
	if (!yt_text_read("YTNEWS.DAT", &news, &error)
	    || news.length != sizeof(expected_news) - 1U
	    || memcmp(news.data, expected_news, sizeof(expected_news) - 1U) != 0)
		goto done;
	file = fopen("YTRMSG.DAT", "rb");
	if (file == NULL
	    || fread(radio.bytes, 1, sizeof(radio.bytes), file)
	    != sizeof(radio.bytes) || fgetc(file) != EOF || ferror(file)
	    || yt_radio_get_number(&radio, 0U) != 1.0f
	    || yt_radio_get_number(&radio, 4U) != 2.0f
	    || yt_radio_get_number(&radio, 8U) != -2.0f
	    || memcmp(radio.bytes + 12U, expected_radio,
	    sizeof(expected_radio) - 1U) != 0
	    || radio.bytes[12U + sizeof(expected_radio) - 1U] != ' ')
		goto done;
	(void)fclose(file);
	file = NULL;
	if (yt_maintenance_mercenary_destination(NULL, 1, 1.0f,
	    score_line_collect, &screen, &arrival, &error)
	    || yt_maintenance_mercenary_destination(&game, 0, 1.0f,
	    score_line_collect, &screen, &arrival, &error)
	    || yt_maintenance_mercenary_destination(&game, 1, 1.0f,
	    NULL, &screen, &arrival, &error)
	    || yt_maintenance_mercenary_destination(&game, 1, 1.0f,
	    score_line_collect, &screen, NULL, &error))
		goto done;
	valid = true;

done:
	if (file != NULL)
		(void)fclose(file);
	yt_text_free(&news);
	yt_game_close(&game);
	(void)remove("YTDATA.DAT");
	(void)remove("YTNEWS.DAT");
	(void)remove("YTRMSG.DAT");
	return valid;
}

static bool
check_maintenance_mercenary_mine_pass(void)
{
	static const uint8_t random_bytes[18] = {0};
	static const uint8_t expected_screen[] =
	    " *** 10 mercenaries hit sector mines in sector 7!\r"
	    " *** Lost a total of 1 fighters!\r"
	    " *** 1 mercenaries hit sector mines in sector 10!\r"
	    " *** The mercenariers were killed!\r";
	static const uint8_t expected_first_screen[] =
	    " *** 10 mercenaries hit sector mines in sector 7!\r"
	    " *** Lost a total of 1 fighters!\r";
	static const uint8_t expected_news[] =
	    " *** 10 mercenaries hit sector mines in sector 7!\r\n"
	    " *** Lost a total of 1 fighters!\r\n"
	    " *** 1 mercenaries hit sector mines in sector 10!\r\n"
	    " *** The mercenariers were killed!\r\n\x1a";
	static const uint8_t expected_first_news[] =
	    " *** 10 mercenaries hit sector mines in sector 7!\r\n"
	    " *** Lost a total of 1 fighters!\r\n\x1a";
	struct score_random_script script = {
		random_bytes, sizeof(random_bytes), 0U
	};
	struct score_line_tape screen = {0};
	struct yt_maintenance_mercenary_mine_result result;
	struct yt_text_file news = {0};
	struct yt_record before;
	struct yt_record expected;
	struct yt_record after;
	struct yt_record fractional;
	struct yt_sector arrival;
	struct yt_game game;
	struct yt_error error;
	bool valid = false;

	(void)remove("YTDATA.DAT");
	(void)remove("YTNEWS.DAT");
	memset(&game, 0, sizeof(game));
	game.config.sector_offset = 1.0f;
	yt_random_init(&game.random);
	yt_random_set_provider(&game.random, score_random_fill, &script);
	yt_error_clear(&error);
	if (!yt_database_open(&game.database, "YTDATA.DAT", YT_OPEN_CREATE,
	    &error))
		goto done;
	yt_record_blank(&before);
	before.bytes[YT_RECORD_TAIL_OFFSET] = 0xB7U;
	if (!yt_record_set_number(&before, YT_F81, 23.0f)
	    || !yt_record_set_number(&before, YT_F85, 4.0f)
	    || !yt_record_set_number(&before, YT_F93, 9.0f)
	    || !yt_record_set_number(&before, YT_F129, 3.0f)
	    || !yt_database_write(&game.database, 8U, &before, &error))
		goto done;
	if (!yt_maintenance_mercenary_mines(&game, 7, 10.0f,
	    score_line_collect, &screen, &arrival, &result, &error)
	    || !result.mine_hit || result.killed
	    || result.moving_before != 10.0f || result.losses != 1.0f
	    || result.survivors != 9.0f || result.draws_consumed != 2U
	    || game.random.draws != 2U || script.position != 6U
	    || arrival.mines != 2.0f || arrival.fighters != 23.0f
	    || arrival.fighter_owner != 4.0f || arrival.planet != 9.0f
	    || screen.lines != 2U
	    || screen.length != sizeof(expected_first_screen) - 1U
	    || memcmp(screen.data, expected_first_screen,
	    sizeof(expected_first_screen) - 1U) != 0)
		goto done;
	expected = before;
	if (!yt_record_set_number(&expected, YT_F129, 2.0f)
	    || !yt_database_read(&game.database, 8U, &after, &error)
	    || memcmp(after.bytes, expected.bytes, YT_RECORD_SIZE) != 0
	    || !yt_text_read("YTNEWS.DAT", &news, &error)
	    || news.length != sizeof(expected_first_news) - 1U
	    || memcmp(news.data, expected_first_news,
	    sizeof(expected_first_news) - 1U)
	    != 0)
		goto done;
	/* CINT-like clamp to a fractional moving force can produce zero damage. */
	yt_record_blank(&fractional);
	fractional.bytes[YT_RECORD_TAIL_OFFSET] = 0x5CU;
	if (!yt_record_set_number(&fractional, YT_F81, 17.0f)
	    || !yt_record_set_number(&fractional, YT_F129, 3.0f)
	    || !yt_database_write(&game.database, 10U, &fractional, &error)
	    || !yt_maintenance_mercenary_mines(&game, 9, 0.5f,
	    score_line_collect, &screen, &arrival, &result, &error)
	    || result.mine_hit || result.killed || result.losses != 0.0f
	    || result.survivors != 0.5f || result.draws_consumed != 2U
	    || game.random.draws != 4U || script.position != 12U
	    || screen.lines != 2U
	    || !yt_database_read(&game.database, 10U, &after, &error)
	    || memcmp(after.bytes, fractional.bytes, YT_RECORD_SIZE) != 0)
		goto done;
	yt_record_blank(&before);
	before.bytes[YT_RECORD_TAIL_OFFSET] = 0x93U;
	if (!yt_record_set_number(&before, YT_F81, 41.0f)
	    || !yt_record_set_number(&before, YT_F129, 2.0f)
	    || !yt_database_write(&game.database, 11U, &before, &error)
	    || !yt_maintenance_mercenary_mines(&game, 10, 1.0f,
	    score_line_collect, &screen, &arrival, &result, &error)
	    || !result.mine_hit || !result.killed || result.losses != 1.0f
	    || result.survivors != 0.0f || result.draws_consumed != 2U
	    || game.random.draws != 6U || script.position != sizeof(random_bytes)
	    || arrival.mines != 1.0f || arrival.fighters != 41.0f
	    || screen.lines != 4U
	    || screen.length != sizeof(expected_screen) - 1U
	    || memcmp(screen.data, expected_screen,
	    sizeof(expected_screen) - 1U) != 0)
		goto done;
	expected = before;
	if (!yt_record_set_number(&expected, YT_F129, 1.0f)
	    || !yt_database_read(&game.database, 11U, &after, &error)
	    || memcmp(after.bytes, expected.bytes, YT_RECORD_SIZE) != 0
	    || !yt_text_read("YTNEWS.DAT", &news, &error)
	    || news.length != sizeof(expected_news) - 1U
	    || memcmp(news.data, expected_news, sizeof(expected_news) - 1U)
	    != 0)
		goto done;
	yt_record_blank(&before);
	if (!yt_record_set_number(&before, YT_F129, 0.0f)
	    || !yt_database_write(&game.database, 9U, &before, &error)
	    || !yt_maintenance_mercenary_mines(&game, 8, 10.0f,
	    score_line_collect, &screen, &arrival, &result, &error)
	    || result.mine_hit || result.killed || result.survivors != 10.0f
	    || result.draws_consumed != 0U || game.random.draws != 6U
	    || screen.lines != 4U
	    || yt_maintenance_mercenary_mines(NULL, 7, 1.0f,
	    score_line_collect, &screen, &arrival, &result, &error)
	    || yt_maintenance_mercenary_mines(&game, 0, 1.0f,
	    score_line_collect, &screen, &arrival, &result, &error)
	    || yt_maintenance_mercenary_mines(&game, 7, 1.0f, NULL,
	    &screen, &arrival, &result, &error))
		goto done;
	valid = true;

done:
	yt_text_free(&news);
	yt_game_close(&game);
	(void)remove("YTDATA.DAT");
	(void)remove("YTNEWS.DAT");
	return valid;
}

static bool
check_maintenance_mercenary_planet_pass(void)
{
	static const uint8_t planet_fighter_zero[4] = {0x00, 0x00, 0x80, 0x00};
	static const uint8_t expected_screen[] =
	    "  -  13 Mercenaries captured planet E\0den!\r"
	    "  -  12 Mercenaries taking 3 fighters from planet E\0den!\r";
	static const uint8_t expected_news[] =
	    "  -  13 Mercenaries captured planet E\0den!\r\n"
	    "  -  12 Mercenaries taking 3 fighters from planet E\0den!\r\n\x1a";
	struct score_line_tape screen = {0};
	struct yt_maintenance_mercenary_planet_result result;
	struct yt_text_file news = {0};
	struct yt_record sector_raw;
	struct yt_record planet_raw;
	struct yt_record expected;
	struct yt_record after;
	struct yt_sector sector;
	struct yt_game game;
	struct yt_error error;
	bool valid = false;

	(void)remove("YTDATA.DAT");
	(void)remove("YTNEWS.DAT");
	memset(&game, 0, sizeof(game));
	game.config.sector_offset = 1.0f;
	game.config.planet_offset = 20.0f;
	yt_error_clear(&error);
	if (!yt_database_open(&game.database, "YTDATA.DAT", YT_OPEN_CREATE,
	    &error))
		goto done;
	yt_record_blank(&sector_raw);
	sector_raw.bytes[YT_RECORD_TAIL_OFFSET] = 0xA6U;
	if (!yt_record_set_number(&sector_raw, YT_F81, 2.0f)
	    || !yt_record_set_number(&sector_raw, YT_F85, 0.0f)
	    || !yt_record_set_number(&sector_raw, YT_F93, 5.0f)
	    || !yt_record_set_number(&sector_raw, YT_F129, 7.0f)
	    || !yt_database_write(&game.database, 8U, &sector_raw, &error))
		goto done;
	yt_record_blank(&planet_raw);
	memcpy(planet_raw.bytes, "E\0den", 5U);
	planet_raw.bytes[YT_RECORD_TAIL_OFFSET] = 0x6BU;
	if (!yt_record_set_number(&planet_raw, YT_F73, 7.0f)
	    || !yt_record_set_number(&planet_raw, YT_F85, 5.0f)
	    || !yt_record_set_number(&planet_raw, YT_F125, 9.0f)
	    || !yt_record_set_number(&planet_raw, YT_F129, 3.4f)
	    || !yt_database_write(&game.database, 25U, &planet_raw, &error)
	    || !yt_game_read_sector(&game, 7, &sector, &error)
	    || !yt_maintenance_mercenary_planet_absorption(&game, 7, 20,
	    10.0, score_line_collect, &screen, &sector, &result, &error)
	    || !result.absorbed || !result.capture_report
	    || !result.taking_report || result.planet_fighters != 3.0f
	    || result.sector_fighters != 15.0f || sector.fighters != 15.0f
	    || sector.fighter_owner != -2.0f || sector.planet != 5.0f
	    || screen.lines != 2U
	    || screen.length != sizeof(expected_screen) - 1U
	    || memcmp(screen.data, expected_screen,
	    sizeof(expected_screen) - 1U) != 0)
		goto done;
	expected = planet_raw;
	if (!yt_record_set_raw_number(&expected, YT_F129,
	    planet_fighter_zero)
	    || !yt_database_read(&game.database, 25U, &after, &error)
	    || memcmp(after.bytes, expected.bytes, YT_RECORD_SIZE) != 0)
		goto done;
	expected = sector_raw;
	if (!yt_record_set_number(&expected, YT_F81, 15.0f)
	    || !yt_record_set_number(&expected, YT_F85, -2.0f)
	    || !yt_database_read(&game.database, 8U, &after, &error)
	    || memcmp(after.bytes, expected.bytes, YT_RECORD_SIZE) != 0
	    || !yt_text_read("YTNEWS.DAT", &news, &error)
	    || news.length != sizeof(expected_news) - 1U
	    || memcmp(news.data, expected_news, sizeof(expected_news) - 1U)
	    != 0)
		goto done;

	/* Same-sector arrival and an empty planet reserve emit neither report. */
	yt_record_blank(&sector_raw);
	if (!yt_record_set_number(&sector_raw, YT_F81, 4.0f)
	    || !yt_record_set_number(&sector_raw, YT_F85, -2.0f)
	    || !yt_record_set_number(&sector_raw, YT_F93, 6.0f)
	    || !yt_database_write(&game.database, 9U, &sector_raw, &error))
		goto done;
	yt_record_blank(&planet_raw);
	memcpy(planet_raw.bytes, "Void", 4U);
	if (!yt_record_set_number(&planet_raw, YT_F85, 4.0f)
	    || !yt_record_set_number(&planet_raw, YT_F129, 0.0f)
	    || !yt_database_write(&game.database, 26U, &planet_raw, &error)
	    || !yt_game_read_sector(&game, 8, &sector, &error)
	    || !yt_maintenance_mercenary_planet_absorption(&game, 8, 8,
	    1.0, score_line_collect, &screen, &sector, &result, &error)
	    || !result.absorbed || result.capture_report || result.taking_report
	    || result.sector_fighters != 5.0f || screen.lines != 2U)
		goto done;

	/* A player-owned destination bypasses planet I/O and absorption. */
	yt_record_blank(&sector_raw);
	if (!yt_record_set_number(&sector_raw, YT_F81, 6.0f)
	    || !yt_record_set_number(&sector_raw, YT_F85, 7.0f)
	    || !yt_record_set_number(&sector_raw, YT_F93, 6.0f)
	    || !yt_database_write(&game.database, 10U, &sector_raw, &error)
	    || !yt_game_read_sector(&game, 9, &sector, &error)
	    || !yt_maintenance_mercenary_planet_absorption(&game, 9, 20,
	    1.0, score_line_collect, &screen, &sector, &result, &error)
	    || result.absorbed || result.capture_report || result.taking_report
	    || sector.fighters != 6.0f || screen.lines != 2U)
		goto done;
	valid = true;

done:
	yt_text_free(&news);
	yt_game_close(&game);
	(void)remove("YTDATA.DAT");
	(void)remove("YTNEWS.DAT");
	return valid;
}

static bool
check_maintenance_super_lottery_pass(void)
{
	static const uint8_t success_draws[] = {
		0x00, 0x00, 0x80, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x80, 0x00, 0x00, 0x80,
		0x00, 0x00, 0x80, 0x00, 0x00, 0x80,
		0x00, 0x00, 0x80, 0x00, 0x00, 0x80,
		0x00, 0x00, 0x80, 0x00, 0x00, 0x80
	};
	static const uint8_t failure_draws[] = {
		0x00, 0x00, 0x80, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	static const uint8_t coin_draw[] = {0x00, 0x00, 0x00};
	static const uint8_t expected_screen[] =
	    "\rRunning Super Planet Lottery\r"
	    " *** A\0da won a PLANET in the SUPER LOTTERY!!!!!\a\r";
	static const uint8_t expected_failure[] =
	    "\rRunning Super Planet Lottery\r"
	    "No one won a planet today.\r";
	static const uint8_t expected_news[] =
	    " *** A\0da won a PLANET in the SUPER LOTTERY!!!!!\a\r\n\x1a";
	static const uint8_t expected_radio[] =
	    "\aYou won a PLANET in the SUPER-LOTTERY! Look in sector 1!\a";
	static const uint8_t dirty_zero[4] = {0x00, 0x00, 0x3b, 0x00};
	static const size_t production_offsets[] = {YT_F45, YT_F49, YT_F53};
	static const size_t dirty_offsets[] = {YT_F57, YT_F61, YT_F65, YT_F69};
	struct score_random_script script = {
		success_draws, sizeof(success_draws), 0U
	};
	struct score_line_tape screen = {0};
	struct yt_maintenance_lottery_result result;
	struct yt_record player;
	struct yt_record planet_before;
	struct yt_record sector_before;
	struct yt_record expected;
	struct yt_record after;
	struct yt_radio_record radio;
	struct yt_text_file news = {0};
	struct yt_game game;
	struct yt_error error;
	FILE *file = NULL;
	size_t index;
	bool valid = false;

	(void)remove("YTDATA.DAT");
	(void)remove("YTNEWS.DAT");
	(void)remove("YTRMSG.DAT");
	memset(&game, 0, sizeof(game));
	game.config.sector_offset = 10.0f;
	game.config.planet_offset = 30.0f;
	yt_random_init(&game.random);
	yt_random_set_provider(&game.random, score_random_fill, &script);
	yt_error_clear(&error);
	if (!yt_database_open(&game.database, "YTDATA.DAT", YT_OPEN_CREATE,
	    &error))
		goto done;
	yt_record_blank(&player);
	memcpy(player.bytes, "A\0da", 4U);
	player.bytes[YT_RECORD_TAIL_OFFSET] = 0xC2U;
	if (!yt_record_set_number(&player, YT_F85, 4.0f)
	    || !yt_database_write(&game.database, 2U, &player, &error))
		goto done;
	memset(&planet_before, 0xA5, sizeof(planet_before));
	memcpy(planet_before.bytes + YT_F113, "PLAS", 4U);
	memcpy(planet_before.bytes + YT_F129, "FITE", 4U);
	planet_before.bytes[YT_RECORD_TAIL_OFFSET] = 0xE1U;
	if (!yt_record_set_number(&planet_before, YT_F41, 19.0f)
	    || !yt_record_set_number(&planet_before, YT_F85, 0.0f)
	    || !yt_record_set_number(&planet_before, YT_F89, 721.0f)
	    || !yt_record_set_number(&planet_before, YT_F93, 23.0f)
	    || !yt_record_set_number(&planet_before, YT_F97, 29.0f)
	    || !yt_record_set_number(&planet_before, YT_F101, 31.0f)
	    || !yt_record_set_number(&planet_before, YT_F105, 37.0f)
	    || !yt_record_set_number(&planet_before, YT_F109, 41.0f)
	    || !yt_record_set_number(&planet_before, YT_F121, 43.0f)
	    || !yt_database_write(&game.database, 31U, &planet_before, &error))
		goto done;
	memset(&sector_before, 0x6D, sizeof(sector_before));
	sector_before.bytes[YT_RECORD_TAIL_OFFSET] = 0xD4U;
	if (!yt_record_set_number(&sector_before, YT_F81, 17.0f)
	    || !yt_record_set_number(&sector_before, YT_F85, 8.0f)
	    || !yt_record_set_number(&sector_before, YT_F93, -1.0f)
	    || !yt_database_write(&game.database, 11U, &sector_before, &error))
		goto done;
	if (!yt_maintenance_super_lottery(&game, 1, 1, 1,
	    NULL, 0U, score_line_collect, &screen,
	    &result, &error)
	    || result.failure != YT_MAINTENANCE_LOTTERY_SUCCESS
	    || result.player_record != 2 || result.planet_number != 1
	    || result.sector_number != 1 || result.draws_consumed != 12U
	    || game.random.draws != 12U
	    || script.position != sizeof(success_draws)
	    || screen.lines != 3U
	    || screen.length != sizeof(expected_screen) - 1U
	    || memcmp(screen.data, expected_screen,
	    sizeof(expected_screen) - 1U) != 0)
		goto done;
	expected = planet_before;
	memset(expected.bytes, ' ', 41U);
	memcpy(expected.bytes, "A\0da's Planet", 13U);
	if (!yt_record_set_number(&expected, YT_F85, 13.0f))
		goto done;
	for (index = 0U; index < YT_ARRAY_LEN(production_offsets); ++index)
		if (!yt_record_set_number(&expected, production_offsets[index],
		    750.0f))
			goto done;
	for (index = 0U; index < YT_ARRAY_LEN(dirty_offsets); ++index)
		if (!yt_record_set_raw_number(&expected, dirty_offsets[index],
		    dirty_zero))
			goto done;
	if (!yt_record_set_number(&expected, YT_F73, 2.0f)
	    || !yt_record_set_number(&expected, YT_F77, 51.0f)
	    || !yt_record_set_number(&expected, YT_F117, 8000000.0f)
	    || !yt_record_set_number(&expected, YT_F125, 0.0f)
	    || !yt_database_read(&game.database, 31U, &after, &error)
	    || memcmp(after.bytes, expected.bytes, YT_RECORD_SIZE) != 0)
		goto done;
	expected = sector_before;
	if (!yt_record_set_number(&expected, YT_F93, 1.0f)
	    || !yt_database_read(&game.database, 11U, &after, &error)
	    || memcmp(after.bytes, expected.bytes, YT_RECORD_SIZE) != 0
	    || !yt_text_read("YTNEWS.DAT", &news, &error)
	    || news.length != sizeof(expected_news) - 1U
	    || memcmp(news.data, expected_news, sizeof(expected_news) - 1U) != 0)
		goto done;
	file = fopen("YTRMSG.DAT", "rb");
	if (file == NULL
	    || fread(radio.bytes, 1, sizeof(radio.bytes), file)
	    != sizeof(radio.bytes) || fgetc(file) != EOF || ferror(file)
	    || yt_radio_get_number(&radio, 0U) != 1.0f
	    || yt_radio_get_number(&radio, 4U) != 2.0f
	    || yt_radio_get_number(&radio, 8U) != -2.0f
	    || memcmp(radio.bytes + 12U, expected_radio,
	    sizeof(expected_radio) - 1U) != 0)
		goto done;
	(void)fclose(file);
	file = NULL;
	yt_text_free(&news);
	(void)remove("YTNEWS.DAT");
	(void)remove("YTRMSG.DAT");

	/* Each no-winner branch stops at its exact draw prefix. */
	memset(&screen, 0, sizeof(screen));
	script = (struct score_random_script){failure_draws,
	    sizeof(failure_draws), 0U};
	yt_random_init(&game.random);
	yt_random_set_provider(&game.random, score_random_fill, &script);
	if (!yt_record_set_number(&player, YT_F85, 0.0f)
	    || !yt_database_write(&game.database, 2U, &player, &error)
	    || !yt_maintenance_super_lottery(&game, 1, 1, 1,
	    NULL, 0U, score_line_collect, &screen,
	    &result, &error)
	    || result.failure != YT_MAINTENANCE_LOTTERY_BLANK_PLAYER
	    || result.player_record != 2 || result.draws_consumed != 2U
	    || screen.length != sizeof(expected_failure) - 1U
	    || memcmp(screen.data, expected_failure,
	    sizeof(expected_failure) - 1U) != 0)
		goto done;
	memset(&screen, 0, sizeof(screen));
	script.position = 0U;
	yt_random_init(&game.random);
	yt_random_set_provider(&game.random, score_random_fill, &script);
	if (!yt_record_set_number(&player, YT_F85, 4.0f)
	    || !yt_database_write(&game.database, 2U, &player, &error)
	    || !yt_record_set_number(&planet_before, YT_F85, 1.0f)
	    || !yt_database_write(&game.database, 31U, &planet_before, &error)
	    || !yt_maintenance_super_lottery(&game, 1, 1, 1,
	    NULL, 0U, score_line_collect, &screen,
	    &result, &error)
	    || result.failure != YT_MAINTENANCE_LOTTERY_OCCUPIED_PLANET
	    || result.planet_number != 1 || result.draws_consumed != 3U
	    || screen.length != sizeof(expected_failure) - 1U
	    || memcmp(screen.data, expected_failure,
	    sizeof(expected_failure) - 1U) != 0)
		goto done;
	memset(&screen, 0, sizeof(screen));
	script.position = 0U;
	yt_random_init(&game.random);
	yt_random_set_provider(&game.random, score_random_fill, &script);
	if (!yt_record_set_number(&planet_before, YT_F85, 0.0f)
	    || !yt_database_write(&game.database, 31U, &planet_before, &error)
	    || !yt_record_set_number(&sector_before, YT_F93, 1.0f)
	    || !yt_database_write(&game.database, 11U, &sector_before, &error)
	    || !yt_maintenance_super_lottery(&game, 1, 1, 1,
	    NULL, 0U, score_line_collect, &screen,
	    &result, &error)
	    || result.failure != YT_MAINTENANCE_LOTTERY_OCCUPIED_SECTOR
	    || result.sector_number != 1 || result.draws_consumed != 4U
	    || screen.length != sizeof(expected_failure) - 1U
	    || memcmp(screen.data, expected_failure,
	    sizeof(expected_failure) - 1U) != 0)
		goto done;
	memset(&screen, 0, sizeof(screen));
	script = (struct score_random_script){coin_draw, sizeof(coin_draw), 0U};
	yt_random_init(&game.random);
	yt_random_set_provider(&game.random, score_random_fill, &script);
	if (!yt_maintenance_super_lottery(&game, 1, 1, 1,
	    NULL, 0U, score_line_collect, &screen,
	    &result, &error)
	    || result.failure != YT_MAINTENANCE_LOTTERY_COIN
	    || result.draws_consumed != 1U
	    || screen.length != sizeof(expected_failure) - 1U
	    || memcmp(screen.data, expected_failure,
	    sizeof(expected_failure) - 1U) != 0
	    || yt_maintenance_super_lottery(NULL, 1, 1, 1,
	    (const uint8_t *)"", 0U, score_line_collect, &screen, &result,
	    &error)
	    || yt_maintenance_super_lottery(&game, 0, 1, 1,
	    (const uint8_t *)"", 0U, score_line_collect, &screen, &result,
	    &error))
		goto done;
	valid = true;

done:
	if (file != NULL)
		(void)fclose(file);
	yt_text_free(&news);
	yt_game_close(&game);
	(void)remove("YTDATA.DAT");
	(void)remove("YTNEWS.DAT");
	(void)remove("YTRMSG.DAT");
	return valid;
}

static bool
check_maintenance_final_marker_pass(void)
{
	struct yt_record before;
	struct yt_record expected;
	struct yt_record after;
	struct yt_game game;
	struct yt_error error;
	bool valid = false;

	(void)remove("YTDATA.DAT");
	memset(&game, 0, sizeof(game));
	memset(&before, 0x79, sizeof(before));
	memset(&game.config.record, 0x24, sizeof(game.config.record));
	yt_error_clear(&error);
	if (!yt_database_open(&game.database, "YTDATA.DAT", YT_OPEN_CREATE,
	    &error)
	    || !yt_database_write(&game.database, 1U, &before, &error))
		goto done;
	expected = before;
	if (!yt_record_set_number(&expected, YT_F81, 204.0f)
	    || !yt_maintenance_store_final_marker(&game, 204.0f, &error)
	    || game.config.last_maintenance != 204.0f
	    || memcmp(game.config.record.bytes, expected.bytes,
	    YT_RECORD_SIZE) != 0
	    || !yt_database_read(&game.database, 1U, &after, &error)
	    || memcmp(after.bytes, expected.bytes, YT_RECORD_SIZE) != 0
	    || yt_maintenance_store_final_marker(NULL, 204.0f, &error))
		goto done;
	valid = true;

done:
	yt_game_close(&game);
	(void)remove("YTDATA.DAT");
	return valid;
}

static bool
planet_maintenance_owned_byte(size_t offset)
{
	return (offset >= YT_F41 && offset < YT_F73)
	    || (offset >= YT_F77 && offset < YT_F81)
	    || (offset >= YT_F89 && offset < YT_F93)
	    || (offset >= YT_F113 && offset < YT_F121)
	    || (offset >= YT_F125 && offset < YT_RECORD_TAIL_OFFSET);
}

static void
seed_maintenance_planet(struct yt_record *record, int kind)
{
	int index;

	memset(record->bytes, 0xa5, sizeof(record->bytes));
	if (kind != 0) {
		memcpy(record->bytes, kind == 1 ? "Ordinary" : "Xannoron", 8U);
		record->bytes[8] = 0;
	}
	yt_record_set_number(record, YT_F41, 1.0f);
	for (index = 0; index < 3; ++index) {
		float production = kind == 2 ? (float)((index + 1) * 100)
		    : kind == 1 ? (float)((index + 1) * 100) : 9.0f;
		float stock = kind == 2 ? (float)((index + 1) * 1000)
		    : 0.0f;

		yt_record_set_number(record, YT_F45 + (size_t)index * 4U,
		    production);
		yt_record_set_number(record, YT_F57 + (size_t)index * 4U,
		    stock);
	}
	yt_record_set_number(record, YT_F69, 0.0f);
	yt_record_set_number(record, YT_F73, 77.0f);
	yt_record_set_number(record, YT_F77,
	    kind == 2 ? 20000000.0f : 0.0f);
	yt_record_set_number(record, YT_F85, kind == 0 ? 0.0f : 8.0f);
	yt_record_set_number(record, YT_F89, 0.0f);
	yt_record_set_number(record, YT_F113, 0.0f);
	yt_record_set_number(record, YT_F117,
	    kind == 2 ? 1000000.0f : 0.0f);
	yt_record_set_number(record, YT_F125, 0.0f);
	yt_record_set_number(record, YT_F129, 0.0f);
}

static bool
check_maintenance_planet_pass(void)
{
	static const uint8_t random_bytes[] = {
		0, 0, 0, 0, 0, 0, 0, 0, 0,
		0x00, 0x00, 0x20, 0x00, 0x00, 0x20, 0x00, 0x00, 0xe0,
		0x00, 0x00, 0x80, 0x00, 0x00, 0x80, 0x00, 0x00, 0x80,
		0x00, 0x00, 0x80, 0x00, 0x00, 0x80, 0x00, 0x00, 0x40
	};
	static const uint8_t expected_screen[] =
	    "\rRunning planet maintenance...\r"
	    "\r"
	    "  -  CIVIL WAR has struck planet Xannoron as a result of "
	    "overcrowding!\r"
	    "  -  Productivity reduced from 600 units to 300 units!\r"
	    "  -  Ground forces reduced from 2E+07 to 1.5E+07 units!\r"
	    "  -  250000 credits were spent putting down the insurrection!\r";
	static const uint8_t expected_news[] =
	    "  -  CIVIL WAR has struck planet Xannoron as a result of "
	    "overcrowding!\r\n"
	    "  -  Productivity reduced from 600 units to 300 units!\r\n"
	    "  -  Ground forces reduced from 2E+07 to 1.5E+07 units!\r\n"
	    "  -  250000 credits were spent putting down the insurrection!\r\n"
	    "\x1a";
	struct score_random_script random_script = {
		random_bytes, sizeof(random_bytes), 0U
	};
	struct score_clock_script clock_script = {{
		{2026, 1, 2, 0, 0, 0, 0},
		{2026, 1, 2, 0, 0, 0, 0},
		{2026, 1, 1, 0, 0, 0, 0},
		{2026, 1, 1, 0, 0, 0, 0}
	}, 0U};
	struct score_line_tape screen = {0};
	struct yt_text_file news = {0};
	struct yt_record before[3];
	struct yt_record after[3];
	struct yt_game game;
	struct yt_error error;
	int events = -1;
	int record;
	size_t offset;
	bool valid = false;

	(void)remove("YTDATA.DAT");
	(void)remove("YTNEWS.DAT");
	memset(&game, 0, sizeof(game));
	game.config.planet_offset = 1.0f;
	game.config.total_records = 4.0f;
	game.config.epoch_year = 26.0f;
	yt_random_init(&game.random);
	yt_random_set_provider(&game.random, score_random_fill, &random_script);
	yt_error_clear(&error);
	if (!yt_database_open(&game.database, "YTDATA.DAT", YT_OPEN_CREATE,
	    &error))
		goto done;
	for (record = 0; record < 3; ++record) {
		seed_maintenance_planet(&before[record], record);
		if (!yt_database_write(&game.database, (size_t)record + 2U,
		    &before[record], &error))
			goto done;
	}
	yt_platform_set_clock_provider(score_clock_read, &clock_script);
	if (!yt_maintenance_maintain_planets(&game,
	    NULL, 0U, score_line_collect, &screen,
	    &events, &error) || events != 1 || clock_script.position != 4U
	    || random_script.position != sizeof(random_bytes)
	    || game.random.draws != 12U || screen.lines != 7U
	    || screen.length != sizeof(expected_screen) - 1U
	    || memcmp(screen.data, expected_screen,
	    sizeof(expected_screen) - 1U) != 0)
		goto done;
	for (record = 0; record < 3; ++record) {
		if (!yt_database_read(&game.database, (size_t)record + 2U,
		    &after[record], &error))
			goto done;
		if (record == 0) {
			if (memcmp(after[record].bytes, before[record].bytes,
			    YT_RECORD_SIZE) != 0)
				goto done;
			continue;
		}
		for (offset = 0; offset < YT_RECORD_SIZE; ++offset) {
			if (!planet_maintenance_owned_byte(offset)
			    && after[record].bytes[offset]
			    != before[record].bytes[offset])
				goto done;
		}
	}
	if (after[2].bytes[8] != 0 || after[2].bytes[9] != 0xa5
	    || yt_record_get_number(&after[1], YT_F41) != 2.0f
	    || yt_record_get_number(&after[1], YT_F45) != 101.0f
	    || yt_record_get_number(&after[1], YT_F49) != 202.0f
	    || yt_record_get_number(&after[1], YT_F53) != 303.0f
	    || yt_record_get_number(&after[1], YT_F129) != 600.0f
	    || yt_record_get_number(&after[2], YT_F45) != 50.0f
	    || yt_record_get_number(&after[2], YT_F49) != 100.0f
	    || yt_record_get_number(&after[2], YT_F53) != 150.0f
	    || yt_record_get_number(&after[2], YT_F57) != 500.0f
	    || yt_record_get_number(&after[2], YT_F61) != 1000.0f
	    || yt_record_get_number(&after[2], YT_F65) != 1500.0f
	    || yt_record_get_number(&after[2], YT_F77) != 15000000.0f
	    || yt_record_get_number(&after[2], YT_F117) != 750000.0f
	    || !yt_text_read("YTNEWS.DAT", &news, &error)
	    || news.length != sizeof(expected_news) - 1U
	    || memcmp(news.data, expected_news, sizeof(expected_news) - 1U)
	    != 0)
		goto done;
	valid = true;

done:
	yt_text_free(&news);
	yt_platform_set_clock_provider(NULL, NULL);
	yt_game_close(&game);
	(void)remove("YTDATA.DAT");
	(void)remove("YTNEWS.DAT");
	return valid;
}

static void
seed_wanderer_sector(struct yt_record *record, float planet)
{
	memset(record->bytes, 0xa5, sizeof(record->bytes));
	yt_record_set_number(record, YT_F93, planet);
}

static void
seed_wanderer_planet(struct yt_record *record, float owner, float bank)
{
	memset(record->bytes, 0xa5, sizeof(record->bytes));
	yt_record_set_number(record, YT_F73, owner);
	yt_record_set_number(record, YT_F117, bank);
}

static bool
wanderer_rebuild_owned_byte(size_t offset)
{
	return offset < YT_TEXT_FIELD_SIZE
	    || (offset >= YT_F41 && offset < YT_F81)
	    || (offset >= YT_F85 && offset < YT_F89)
	    || (offset >= YT_F117 && offset < YT_F121)
	    || (offset >= YT_F125 && offset < YT_F129);
}

static bool
check_maintenance_wanderer_pass(void)
{
	static const uint8_t existing_draws[] = {
		0x00, 0x00, 0x80, 0x00, 0x00, 0x00
	};
	static const uint8_t missing_draws[] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0xc0
	};
	static const uint8_t expected_existing[] =
	    "\rMoving The Wanderer (Planet #1)\r"
	    "\rWanderer has successfully warped!\r";
	static const uint8_t expected_missing[] =
	    "\rMoving The Wanderer (Planet #1)\r"
	    "  -  The Wanderer is missing or has been destroyed!\r"
	    "  -  The Wanderer regenerated with P.H.O.E.N.I.X. device!\r"
	    "\rWanderer has successfully warped!\r";
	static const uint8_t expected_news[] =
	    "  -  The Wanderer is missing or has been destroyed!\r\n"
	    "  -  The Wanderer regenerated with P.H.O.E.N.I.X. device!\r\n"
	    "\x1a";
	struct score_random_script random_script;
	struct score_clock_script clock_script = {{
		{2026, 1, 2, 0, 0, 0, 0}
	}, 0U};
	struct score_line_tape screen;
	struct yt_maintenance_wanderer_result mutation;
	struct yt_text_file news = {0};
	struct yt_record before[4];
	struct yt_record after[4];
	struct yt_game game;
	struct yt_error error;
	size_t offset;
	int record;
	bool valid = false;

	(void)remove("YTDATA.DAT");
	(void)remove("YTNEWS.DAT");
	memset(&game, 0, sizeof(game));
	game.config.sector_offset = 1.0f;
	game.config.port_offset = 4.0f;
	game.config.planet_offset = 4.0f;
	game.config.total_records = 5.0f;
	game.config.epoch_year = 26.0f;
	random_script = (struct score_random_script){
		existing_draws, sizeof(existing_draws), 0U
	};
	yt_random_init(&game.random);
	yt_random_set_provider(&game.random, score_random_fill, &random_script);
	yt_error_clear(&error);
	if (!yt_database_open(&game.database, "YTDATA.DAT", YT_OPEN_CREATE,
	    &error))
		goto done;
	seed_wanderer_sector(&before[0], 1.0f);
	seed_wanderer_sector(&before[1], 1.0f);
	seed_wanderer_sector(&before[2], 0.0f);
	seed_wanderer_planet(&before[3], 77.0f, 12.0f);
	for (record = 0; record < 4; ++record) {
		if (!yt_database_write(&game.database, (size_t)record + 2U,
		    &before[record], &error))
			goto done;
	}
	memset(&screen, 0, sizeof(screen));
	if (!yt_maintenance_maintain_wanderer(&game,
	    NULL, 0U, score_line_collect, &screen,
	    &mutation, &error)
	    || mutation.scanned_sectors != 1 || mutation.removed_sector != 1
	    || mutation.rebuilt || mutation.candidate_attempts != 2
	    || mutation.target_sector != 1 || mutation.bank_after != 12.0f
	    || mutation.draws_consumed != 2U || game.random.draws != 2U
	    || random_script.position != sizeof(existing_draws)
	    || screen.lines != 4U
	    || screen.length != sizeof(expected_existing) - 1U
	    || memcmp(screen.data, expected_existing,
	    sizeof(expected_existing) - 1U) != 0)
		goto done;
	for (record = 0; record < 4; ++record) {
		if (!yt_database_read(&game.database, (size_t)record + 2U,
		    &after[record], &error))
			goto done;
	}
	if (yt_record_get_number(&after[0], YT_F93) != 1.0f
	    || yt_record_get_number(&after[1], YT_F93) != 1.0f
	    || memcmp(after[2].bytes, before[2].bytes, YT_RECORD_SIZE) != 0
	    || yt_record_get_number(&after[3], YT_F73) != 0.0f
	    || yt_record_get_number(&after[3], YT_F117) != 12.0f)
		goto done;
	for (offset = 0; offset < YT_RECORD_SIZE; ++offset) {
		if (offset != YT_F93 && offset != YT_F93 + 1U
		    && offset != YT_F93 + 2U && offset != YT_F93 + 3U
		    && after[0].bytes[offset] != before[0].bytes[offset])
			goto done;
		if ((offset < YT_F73 || offset >= YT_F73 + 4U)
		    && (offset < YT_F117 || offset >= YT_F117 + 4U)
		    && after[3].bytes[offset] != before[3].bytes[offset])
			goto done;
	}
	yt_game_close(&game);
	memset(&game, 0, sizeof(game));
	game.config.sector_offset = 1.0f;
	game.config.port_offset = 4.0f;
	game.config.planet_offset = 4.0f;
	game.config.total_records = 5.0f;
	game.config.epoch_year = 26.0f;
	random_script = (struct score_random_script){
		missing_draws, sizeof(missing_draws), 0U
	};
	yt_random_init(&game.random);
	yt_random_set_provider(&game.random, score_random_fill, &random_script);
	if (!yt_database_open(&game.database, "YTDATA.DAT", YT_OPEN_CREATE,
	    &error))
		goto done;
	seed_wanderer_sector(&before[0], -1.0f);
	seed_wanderer_sector(&before[1], 3.0f);
	seed_wanderer_sector(&before[2], 0.0f);
	seed_wanderer_planet(&before[3], 77.0f, 999.0f);
	for (record = 0; record < 4; ++record) {
		if (!yt_database_write(&game.database, (size_t)record + 2U,
		    &before[record], &error))
			goto done;
	}
	memset(&screen, 0, sizeof(screen));
	yt_platform_set_clock_provider(score_clock_read, &clock_script);
	if (!yt_maintenance_maintain_wanderer(&game,
	    NULL, 0U, score_line_collect, &screen,
	    &mutation, &error)
	    || mutation.scanned_sectors != 3 || mutation.removed_sector != 0
	    || !mutation.rebuilt || mutation.candidate_attempts != 2
	    || mutation.target_sector != 3 || mutation.bank_after != 250000.0f
	    || mutation.draws_consumed != 2U || game.random.draws != 2U
	    || random_script.position != sizeof(missing_draws)
	    || clock_script.position != 1U || screen.lines != 6U
	    || screen.length != sizeof(expected_missing) - 1U
	    || memcmp(screen.data, expected_missing,
	    sizeof(expected_missing) - 1U) != 0)
		goto done;
	for (record = 0; record < 4; ++record) {
		if (!yt_database_read(&game.database, (size_t)record + 2U,
		    &after[record], &error))
			goto done;
	}
	if (memcmp(after[0].bytes, before[0].bytes, YT_RECORD_SIZE) != 0
	    || memcmp(after[1].bytes, before[1].bytes, YT_RECORD_SIZE) != 0
	    || yt_record_get_number(&after[2], YT_F93) != 1.0f
	    || memcmp(after[3].bytes, "The Wanderer", 12U) != 0
	    || after[3].bytes[12] != ' '
	    || yt_record_get_number(&after[3], YT_F41) != -8.0f
	    || yt_record_get_number(&after[3], YT_F45) != 5000.0f
	    || yt_record_get_number(&after[3], YT_F49) != 5000.0f
	    || yt_record_get_number(&after[3], YT_F53) != 5000.0f
	    || yt_record_get_number(&after[3], YT_F57) != 0.0f
	    || yt_record_get_number(&after[3], YT_F61) != 0.0f
	    || yt_record_get_number(&after[3], YT_F65) != 0.0f
	    || yt_record_get_number(&after[3], YT_F69) != 0.0f
	    || yt_record_get_number(&after[3], YT_F73) != 0.0f
	    || yt_record_get_number(&after[3], YT_F77) != 0.0f
	    || yt_record_get_number(&after[3], YT_F85) != 12.0f
	    || yt_record_get_number(&after[3], YT_F117) != 250000.0f
	    || yt_record_get_number(&after[3], YT_F125) != 0.0f)
		goto done;
	for (offset = 0; offset < YT_RECORD_SIZE; ++offset) {
		if (!wanderer_rebuild_owned_byte(offset)
		    && after[3].bytes[offset] != before[3].bytes[offset])
			goto done;
	}
	if (!yt_text_read("YTNEWS.DAT", &news, &error)
	    || news.length != sizeof(expected_news) - 1U
	    || memcmp(news.data, expected_news, sizeof(expected_news) - 1U)
	    != 0)
		goto done;
	valid = true;

done:
	yt_text_free(&news);
	yt_platform_set_clock_provider(NULL, NULL);
	yt_game_close(&game);
	(void)remove("YTDATA.DAT");
	(void)remove("YTNEWS.DAT");
	return valid;
}

static bool
xannor_rebuild_owned_byte(size_t offset)
{
	return offset < YT_TEXT_FIELD_SIZE
	    || (offset >= YT_F41 && offset < YT_F81)
	    || (offset >= YT_F85 && offset < YT_F93)
	    || (offset >= YT_F117 && offset < YT_F121)
	    || (offset >= YT_F125 && offset < YT_F129);
}

static bool
check_maintenance_xannor_home_pass(void)
{
	static const uint8_t rebuild_draws[] = {
		0x00, 0x00, 0x80, 0x00, 0x00, 0x40,
		0x00, 0x00, 0x80
	};
	static const uint8_t bypass_draw[] = {0x00, 0x00, 0xc0};
	static const uint8_t existing_draw[] = {0x00, 0x00, 0x00};
	static const uint8_t expected_rebuild[] =
	    "\rChecking for Planet Xannor, create it if missing.\r"
	    "\r  -  The Xannor have made a Planet!\r"
	    "The Xannor home base now has a planet!\r";
	static const uint8_t expected_bypass[] =
	    "\rChecking for Planet Xannor, create it if missing.\r";
	static const uint8_t expected_news[] =
	    "  -  The Xannor have made a Planet!\r\n"
	    "The Xannor home base now has a planet!\r\n\x1a";
	struct score_random_script random_script = {
		rebuild_draws, sizeof(rebuild_draws), 0U
	};
	struct score_clock_script clock_script = {{
		{2026, 1, 2, 0, 0, 0, 0},
		{2026, 1, 2, 22, 47, 30, 0}
	}, 0U};
	struct score_line_tape screen = {0};
	struct yt_maintenance_xannor_home_result mutation;
	struct yt_text_file news = {0};
	struct yt_record sector_before;
	struct yt_record sector_after;
	struct yt_record planet_before;
	struct yt_record planet_after;
	struct yt_game game;
	struct yt_error error;
	FILE *unexpected;
	size_t offset;
	bool valid = false;

	(void)remove("YTDATA.DAT");
	(void)remove("YTNEWS.DAT");
	memset(&game, 0, sizeof(game));
	game.config.sector_offset = 1.0f;
	game.config.port_offset = 4.0f;
	game.config.planet_offset = 4.0f;
	game.config.total_records = 7.0f;
	game.config.headquarters = 2.0f;
	game.config.epoch_year = 26.0f;
	yt_random_init(&game.random);
	yt_random_set_provider(&game.random, score_random_fill, &random_script);
	yt_error_clear(&error);
	if (!yt_database_open(&game.database, "YTDATA.DAT", YT_OPEN_CREATE,
	    &error))
		goto done;
	seed_wanderer_sector(&sector_before, 0.0f);
	seed_wanderer_planet(&planet_before, 77.0f, 999.0f);
	if (!yt_database_write(&game.database, 3U, &sector_before, &error)
	    || !yt_database_write(&game.database, 7U, &planet_before, &error))
		goto done;
	yt_platform_set_clock_provider(score_clock_read, &clock_script);
	if (!yt_maintenance_maintain_xannor_home(&game,
	    NULL, 0U, score_line_collect, &screen,
	    &mutation, &error)
	    || mutation.planet_link_before != 0.0f || !mutation.rebuilt
	    || mutation.ground_before_daily_update != 125.0f
	    || mutation.ground_after_daily_update != 137.0f
	    || mutation.bank_after != 2600000.0f
	    || mutation.draws_consumed != 3U || game.random.draws != 3U
	    || random_script.position != sizeof(rebuild_draws)
	    || clock_script.position != 2U || screen.lines != 5U
	    || screen.length != sizeof(expected_rebuild) - 1U
	    || memcmp(screen.data, expected_rebuild,
	    sizeof(expected_rebuild) - 1U) != 0
	    || !yt_database_read(&game.database, 3U, &sector_after, &error)
	    || !yt_database_read(&game.database, 7U, &planet_after, &error))
		goto done;
	if (yt_record_get_number(&sector_after, YT_F93) != 3.0f
	    || memcmp(planet_after.bytes, "Xannoron", 8U) != 0
	    || planet_after.bytes[8] != ' '
	    || yt_record_get_number(&planet_after, YT_F41) != 2.0f
	    || yt_record_get_number(&planet_after, YT_F45) != 100000.0f
	    || yt_record_get_number(&planet_after, YT_F49) != 100000.0f
	    || yt_record_get_number(&planet_after, YT_F53) != 100000.0f
	    || yt_record_get_number(&planet_after, YT_F57) != 0.0f
	    || yt_record_get_number(&planet_after, YT_F61) != 0.0f
	    || yt_record_get_number(&planet_after, YT_F65) != 0.0f
	    || yt_record_get_number(&planet_after, YT_F69) != 0.0f
	    || yt_record_get_number(&planet_after, YT_F73) != -1.0f
	    || yt_record_get_number(&planet_after, YT_F77) != 137.0f
	    || yt_record_get_number(&planet_after, YT_F85) != 8.0f
	    || yt_record_get_number(&planet_after, YT_F89) != 1367.0f
	    || yt_record_get_number(&planet_after, YT_F117) != 2600000.0f
	    || yt_record_get_number(&planet_after, YT_F125) != 0.0f)
		goto done;
	for (offset = 0; offset < YT_RECORD_SIZE; ++offset) {
		if ((offset < YT_F93 || offset >= YT_F93 + 4U)
		    && sector_after.bytes[offset] != sector_before.bytes[offset])
			goto done;
		if (!xannor_rebuild_owned_byte(offset)
		    && planet_after.bytes[offset] != planet_before.bytes[offset])
			goto done;
	}
	if (!yt_text_read("YTNEWS.DAT", &news, &error)
	    || news.length != sizeof(expected_news) - 1U
	    || memcmp(news.data, expected_news, sizeof(expected_news) - 1U)
	    != 0)
		goto done;
	yt_text_free(&news);
	(void)remove("YTNEWS.DAT");
	seed_wanderer_sector(&sector_before, 2.0f);
	seed_wanderer_planet(&planet_before, 42.0f, 0.0f);
	yt_record_set_number(&planet_before, YT_F77, 10.0f);
	if (!yt_database_write(&game.database, 3U, &sector_before, &error)
	    || !yt_database_write(&game.database, 7U, &planet_before, &error))
		goto done;
	random_script = (struct score_random_script){
		bypass_draw, sizeof(bypass_draw), 0U
	};
	yt_random_set_provider(&game.random, score_random_fill, &random_script);
	memset(&screen, 0, sizeof(screen));
	clock_script.position = 0U;
	if (!yt_maintenance_maintain_xannor_home(&game,
	    NULL, 0U, score_line_collect, &screen,
	    &mutation, &error)
	    || mutation.planet_link_before != 2.0f || mutation.rebuilt
	    || mutation.ground_before_daily_update != 10.0f
	    || mutation.ground_after_daily_update != 28.0f
	    || mutation.bank_after != 16000000.0f
	    || mutation.draws_consumed != 1U || game.random.draws != 1U
	    || random_script.position != sizeof(bypass_draw)
	    || clock_script.position != 0U || screen.lines != 2U
	    || screen.length != sizeof(expected_bypass) - 1U
	    || memcmp(screen.data, expected_bypass,
	    sizeof(expected_bypass) - 1U) != 0
	    || !yt_database_read(&game.database, 3U, &sector_after, &error)
	    || !yt_database_read(&game.database, 7U, &planet_after, &error))
		goto done;
	if (memcmp(sector_after.bytes, sector_before.bytes, YT_RECORD_SIZE) != 0
	    || yt_record_get_number(&planet_after, YT_F73) != -1.0f
	    || yt_record_get_number(&planet_after, YT_F77) != 28.0f
	    || yt_record_get_number(&planet_after, YT_F117) != 16000000.0f)
		goto done;
	for (offset = 0; offset < YT_RECORD_SIZE; ++offset) {
		if ((offset < YT_F73 || offset >= YT_F73 + 4U)
		    && (offset < YT_F77 || offset >= YT_F77 + 4U)
		    && (offset < YT_F117 || offset >= YT_F117 + 4U)
		    && planet_after.bytes[offset] != planet_before.bytes[offset])
			goto done;
	}
	seed_wanderer_sector(&sector_before, 3.0f);
	seed_wanderer_planet(&planet_before, 42.0f, 12.0f);
	yt_record_set_number(&planet_before, YT_F77, 10.0f);
	if (!yt_database_write(&game.database, 3U, &sector_before, &error)
	    || !yt_database_write(&game.database, 7U, &planet_before, &error))
		goto done;
	random_script = (struct score_random_script){
		existing_draw, sizeof(existing_draw), 0U
	};
	yt_random_set_provider(&game.random, score_random_fill, &random_script);
	memset(&screen, 0, sizeof(screen));
	clock_script.position = 0U;
	if (!yt_maintenance_maintain_xannor_home(&game,
	    NULL, 0U, score_line_collect, &screen,
	    &mutation, &error)
	    || mutation.planet_link_before != 3.0f || mutation.rebuilt
	    || mutation.ground_before_daily_update != 10.0f
	    || mutation.ground_after_daily_update != 10.0f
	    || mutation.bank_after != 12.0f
	    || mutation.draws_consumed != 1U || game.random.draws != 1U
	    || random_script.position != sizeof(existing_draw)
	    || clock_script.position != 0U || screen.lines != 2U
	    || screen.length != sizeof(expected_bypass) - 1U
	    || memcmp(screen.data, expected_bypass,
	    sizeof(expected_bypass) - 1U) != 0
	    || !yt_database_read(&game.database, 3U, &sector_after, &error)
	    || !yt_database_read(&game.database, 7U, &planet_after, &error)
	    || memcmp(sector_after.bytes, sector_before.bytes,
	    YT_RECORD_SIZE) != 0
	    || yt_record_get_number(&planet_after, YT_F73) != -1.0f
	    || yt_record_get_number(&planet_after, YT_F77) != 10.0f
	    || yt_record_get_number(&planet_after, YT_F117) != 12.0f)
		goto done;
	unexpected = fopen("YTNEWS.DAT", "rb");
	if (unexpected != NULL) {
		fclose(unexpected);
		goto done;
	}
	valid = true;

done:
	yt_text_free(&news);
	yt_platform_set_clock_provider(NULL, NULL);
	yt_game_close(&game);
	(void)remove("YTDATA.DAT");
	(void)remove("YTNEWS.DAT");
	return valid;
}

static void
seed_xannor_hunt_player(struct yt_record *record, const char *name,
    float name_length, float sector, float score)
{
	memset(record->bytes, 0xa5, sizeof(record->bytes));
	if (name != NULL)
		memcpy(record->bytes, name, strlen(name));
	yt_record_set_number(record, YT_F57, sector);
	yt_record_set_number(record, YT_F85, name_length);
	yt_record_set_number(record, YT_F109, score);
}

static bool
check_maintenance_xannor_hunt_pass(void)
{
	static const uint8_t selected_draws[] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x40
	};
	static const uint8_t rejected_draw[] = {0xff, 0xff, 0xe5};
	static const uint8_t selected_screen[] =
	    "\rProcessing the Xannor.....\r\r"
	    "Locating Top Player... (For Groups 16 - 20 to Pick on!)\r"
	    "\rGroup 20 will hunt for Alice\r";
	static const uint8_t rejected_screen[] =
	    "\rProcessing the Xannor.....\r\r"
	    "Locating Top Player... (For Groups 16 - 20 to Pick on!)\r";
	static const uint8_t expected_news[] = "  -  Xannor report:\r\n\x1a";
	struct score_random_script random_script = {
		selected_draws, sizeof(selected_draws), 0U
	};
	struct score_line_tape screen = {0};
	struct yt_maintenance_xannor_hunt_result hunt;
	struct yt_text_file news = {0};
	struct yt_record before[3];
	struct yt_record after;
	struct yt_game game;
	struct yt_error error;
	float sector_cache[5] = {0};
	float cloak_cache[5] = {0};
	int record;
	bool valid = false;

	(void)remove("YTDATA.DAT");
	(void)remove("YTNEWS.DAT");
	memset(&game, 0, sizeof(game));
	game.config.sector_offset = 4.0f;
	yt_random_init(&game.random);
	yt_random_set_provider(&game.random, score_random_fill, &random_script);
	yt_error_clear(&error);
	if (!yt_database_open(&game.database, "YTDATA.DAT", YT_OPEN_CREATE,
	    &error))
		goto done;
	seed_xannor_hunt_player(&before[0], "Alice", 5.0f, 10.0f,
	    2500000.0f);
	seed_xannor_hunt_player(&before[1], "Bob", 3.0f, 12.0f,
	    2500000.0f);
	seed_xannor_hunt_player(&before[2], "Idle", 0.0f, 14.0f,
	    9000000.0f);
	for (record = 0; record < 3; ++record) {
		if (!yt_database_write(&game.database, (size_t)record + 2U,
		    &before[record], &error))
			goto done;
	}
	sector_cache[2] = 11.0f;
	cloak_cache[2] = 0.33000001311302185f;
	if (!yt_maintenance_xannor_hunt(&game, sector_cache, cloak_cache,
	    YT_ARRAY_LEN(sector_cache), NULL, 0U,
	    score_line_collect, &screen, &hunt, &error)
	    || hunt.top_record != 2 || hunt.top_score != 2500000.0f
	    || !hunt.selected || hunt.used_cached_sector
	    || hunt.target_sector != 10 || hunt.draws_consumed != 2U
	    || game.random.draws != 2U
	    || random_script.position != sizeof(selected_draws)
	    || screen.lines != 6U
	    || screen.length != sizeof(selected_screen) - 1U
	    || memcmp(screen.data, selected_screen,
	    sizeof(selected_screen) - 1U) != 0
	    || !yt_text_read("YTNEWS.DAT", &news, &error)
	    || news.length != sizeof(expected_news) - 1U
	    || memcmp(news.data, expected_news, sizeof(expected_news) - 1U)
	    != 0)
		goto done;
	for (record = 0; record < 3; ++record) {
		if (!yt_database_read(&game.database, (size_t)record + 2U,
		    &after, &error)
		    || memcmp(after.bytes, before[record].bytes,
		    YT_RECORD_SIZE) != 0)
			goto done;
	}
	yt_text_free(&news);
	(void)remove("YTNEWS.DAT");
	seed_xannor_hunt_player(&before[0], "Alice", 5.0f, 10.0f,
	    2499999.0f);
	if (!yt_database_write(&game.database, 2U, &before[0], &error))
		goto done;
	/* Make every occupied score sub-threshold: the gate draw still occurs. */
	seed_xannor_hunt_player(&before[1], "Bob", 3.0f, 12.0f,
	    2499998.0f);
	if (!yt_database_write(&game.database, 3U, &before[1], &error))
		goto done;
	random_script = (struct score_random_script){
		rejected_draw, sizeof(rejected_draw), 0U
	};
	yt_random_set_provider(&game.random, score_random_fill, &random_script);
	memset(&screen, 0, sizeof(screen));
	(void)remove("YTNEWS.DAT");
	if (!yt_maintenance_xannor_hunt(&game, sector_cache, cloak_cache,
	    YT_ARRAY_LEN(sector_cache), NULL, 0U,
	    score_line_collect, &screen, &hunt, &error)
	    || hunt.top_record != 2 || hunt.top_score != 2499999.0f
	    || hunt.selected || hunt.used_cached_sector
	    || hunt.target_sector != 0 || hunt.draws_consumed != 1U
	    || game.random.draws != 1U
	    || random_script.position != sizeof(rejected_draw)
	    || screen.lines != 4U
	    || screen.length != sizeof(rejected_screen) - 1U
	    || memcmp(screen.data, rejected_screen,
	    sizeof(rejected_screen) - 1U) != 0)
		goto done;
	valid = true;

done:
	yt_text_free(&news);
	yt_game_close(&game);
	(void)remove("YTDATA.DAT");
	(void)remove("YTNEWS.DAT");
	return valid;
}

static bool
check_maintenance_xannor_group_extraction_pass(void)
{
	struct yt_record before[30];
	struct yt_record after;
	struct yt_game game;
	struct yt_error error;
	float location[21];
	float size[21];
	size_t offset;
	int sector;
	bool valid = false;

	(void)remove("YTDATA.DAT");
	memset(&game, 0, sizeof(game));
	game.config.sector_offset = 1.0f;
	game.config.port_offset = 31.0f;
	yt_error_clear(&error);
	if (!yt_database_open(&game.database, "YTDATA.DAT", YT_OPEN_CREATE,
	    &error))
		goto done;
	for (sector = 1; sector <= 30; ++sector) {
		memset(before[sector - 1].bytes, 0x40 + sector,
		    YT_RECORD_SIZE);
		yt_record_set_number(&before[sector - 1], YT_F105, 0.0f);
		if (!yt_database_write(&game.database,
		    (size_t)yt_sector_basic_record(&game.config, sector),
		    &before[sector - 1], &error))
			goto done;
	}
	yt_record_set_number(&before[0], YT_F105, 21.0f);
	yt_record_set_number(&before[1], YT_F105, 22.0f);
	yt_record_set_number(&before[2], YT_F105, 21.0f);
	yt_record_set_number(&before[3], YT_F105, 23.0f);
	yt_record_set_number(&before[20], YT_F81, 7.0f);
	yt_record_set_number(&before[20], YT_F85, -1.0f);
	yt_record_set_number(&before[21], YT_F81, 3.0f);
	yt_record_set_number(&before[21], YT_F85, 2.0f);
	yt_record_set_number(&before[22], YT_F81, 0.0f);
	yt_record_set_number(&before[22], YT_F85, -1.0f);
	for (sector = 1; sector <= 23; ++sector) {
		if (!yt_database_write(&game.database,
		    (size_t)yt_sector_basic_record(&game.config, sector),
		    &before[sector - 1], &error))
			goto done;
	}
	if (!yt_maintenance_xannor_groups_extract(&game, location, size,
	    &error)
	    || location[1] != 21.0f || size[1] != 7.0f
	    || location[2] != 22.0f || size[2] != 0.0f
	    || location[3] != 0.0f || size[3] != 0.0f
	    || location[4] != 23.0f || size[4] != 0.0f)
		goto done;
	for (sector = 1; sector <= 23; ++sector) {
		if (!yt_database_read(&game.database,
		    (size_t)yt_sector_basic_record(&game.config, sector),
		    &after, &error))
			goto done;
		if (sector <= 20) {
			if (yt_record_get_number(&after, YT_F105) != 0.0f)
				goto done;
			for (offset = 0; offset < YT_RECORD_SIZE; ++offset) {
				if ((offset < YT_F105 || offset >= YT_F105 + 4U)
				    && after.bytes[offset]
				    != before[sector - 1].bytes[offset])
					goto done;
			}
		}
		else if (sector == 21 || sector == 23) {
			if (yt_record_get_number(&after, YT_F81) != 0.0f
			    || yt_record_get_number(&after, YT_F85) != 0.0f)
				goto done;
			for (offset = 0; offset < YT_RECORD_SIZE; ++offset) {
				if ((offset < YT_F81 || offset >= YT_F81 + 4U)
				    && (offset < YT_F85 || offset >= YT_F85 + 4U)
				    && after.bytes[offset]
				    != before[sector - 1].bytes[offset])
					goto done;
			}
		}
		else if (memcmp(after.bytes, before[sector - 1].bytes,
		    YT_RECORD_SIZE) != 0)
			goto done;
	}
	/* A host-range failure occurs only after all 20 metadata PUTs. */
	for (sector = 1; sector <= 20; ++sector) {
		yt_record_set_number(&before[sector - 1], YT_F105,
		    sector == 1 ? 31.0f : (float)(20 + sector % 3));
		if (!yt_database_write(&game.database,
		    (size_t)yt_sector_basic_record(&game.config, sector),
		    &before[sector - 1], &error))
			goto done;
	}
	if (yt_maintenance_xannor_groups_extract(&game, location, size,
	    &error))
		goto done;
	for (sector = 1; sector <= 20; ++sector) {
		yt_error_clear(&error);
		if (!yt_database_read(&game.database,
		    (size_t)yt_sector_basic_record(&game.config, sector),
		    &after, &error)
		    || yt_record_get_number(&after, YT_F105) != 0.0f)
			goto done;
	}
	valid = true;

done:
	yt_game_close(&game);
	(void)remove("YTDATA.DAT");
	return valid;
}

static bool
check_maintenance_xannor_group_persistence_pass(void)
{
	struct yt_record before[30];
	struct yt_record after;
	struct yt_game game;
	struct yt_error error;
	float location[21] = {0};
	float size[21] = {0};
	size_t offset;
	int sector;
	bool valid = false;

	(void)remove("YTDATA.DAT");
	memset(&game, 0, sizeof(game));
	game.config.sector_offset = 1.0f;
	game.config.port_offset = 31.0f;
	yt_error_clear(&error);
	if (!yt_database_open(&game.database, "YTDATA.DAT", YT_OPEN_CREATE,
	    &error))
		goto done;
	for (sector = 1; sector <= 30; ++sector) {
		memset(before[sector - 1].bytes, 0x20 + sector,
		    YT_RECORD_SIZE);
		yt_record_set_number(&before[sector - 1], YT_F81,
		    sector == 21 ? 3.0f : sector == 23 ? 1.0f : 2.0f);
		yt_record_set_number(&before[sector - 1], YT_F85,
		    sector == 21 ? 7.0f : 0.0f);
		yt_record_set_number(&before[sector - 1], YT_F93,
		    sector == 21 ? 9.0f : sector == 23 ? 8.0f : 0.0f);
		yt_record_set_number(&before[sector - 1], YT_F105, 99.0f);
		if (!yt_database_write(&game.database,
		    (size_t)yt_sector_basic_record(&game.config, sector),
		    &before[sector - 1], &error))
			goto done;
	}
	location[1] = 21.0f;
	size[1] = 5.0f;
	location[2] = 21.0f;
	size[2] = 7.0f;
	location[3] = 0.6f;
	size[3] = 2.0f;
	location[4] = 22.0f;
	location[5] = 23.0f;
	size[5] = 4.0f;
	if (!yt_maintenance_xannor_groups_persist(&game, location, size,
	    &error))
		goto done;
	for (sector = 1; sector <= 30; ++sector) {
		if (!yt_database_read(&game.database,
		    (size_t)yt_sector_basic_record(&game.config, sector),
		    &after, &error))
			goto done;
		if (sector <= 20) {
			float expected_location = location[sector];

			if (yt_record_get_number(&after, YT_F105)
			    != expected_location)
				goto done;
			for (offset = 0; offset < YT_RECORD_SIZE; ++offset) {
				bool metadata_lane = offset >= YT_F105
				    && offset < YT_F105 + 4U;
				bool rounded_host_lane = sector == 1
				    && ((offset >= YT_F81
				    && offset < YT_F81 + 4U)
				    || (offset >= YT_F85
				    && offset < YT_F85 + 4U));

				if (!metadata_lane && !rounded_host_lane
				    && after.bytes[offset]
				    != before[sector - 1].bytes[offset])
					goto done;
			}
		}
		else if (sector != 21 && sector != 23
		    && memcmp(after.bytes, before[sector - 1].bytes,
		    YT_RECORD_SIZE) != 0)
			goto done;
	}
	if (!yt_database_read(&game.database,
	    (size_t)yt_sector_basic_record(&game.config, 1), &after, &error)
	    || yt_record_get_number(&after, YT_F81) != 4.0f
	    || yt_record_get_number(&after, YT_F85) != -1.0f
	    || yt_record_get_number(&after, YT_F93) != 0.0f
	    || !yt_database_read(&game.database,
	    (size_t)yt_sector_basic_record(&game.config, 21), &after, &error)
	    || yt_record_get_number(&after, YT_F81) != 15.0f
	    || yt_record_get_number(&after, YT_F85) != -1.0f
	    || yt_record_get_number(&after, YT_F93) != 9.0f
	    || !yt_database_read(&game.database,
	    (size_t)yt_sector_basic_record(&game.config, 22), &after, &error)
	    || memcmp(after.bytes, before[21].bytes, YT_RECORD_SIZE) != 0
	    || !yt_database_read(&game.database,
	    (size_t)yt_sector_basic_record(&game.config, 23), &after, &error)
	    || yt_record_get_number(&after, YT_F81) != 5.0f
	    || yt_record_get_number(&after, YT_F85) != -1.0f
	    || yt_record_get_number(&after, YT_F93) != 8.0f)
		goto done;
	if (!yt_maintenance_xannor_group_twenty_finish(&game, 19, 21.0f,
	    2.0f, &error)
	    || !yt_maintenance_xannor_group_twenty_finish(&game, 20, 21.0f,
	    0.0f, &error)
	    || !yt_maintenance_xannor_group_twenty_finish(&game, 20, 0.0f,
	    2.0f, &error)
	    || !yt_database_read(&game.database,
	    (size_t)yt_sector_basic_record(&game.config, 21), &after, &error)
	    || yt_record_get_number(&after, YT_F81) != 15.0f
	    || !yt_maintenance_xannor_group_twenty_finish(&game, 20, 21.0f,
	    2.0f, &error)
	    || !yt_database_read(&game.database,
	    (size_t)yt_sector_basic_record(&game.config, 21), &after, &error)
	    || yt_record_get_number(&after, YT_F81) != 16.0f
	    || yt_record_get_number(&after, YT_F85) != -1.0f
	    || yt_record_get_number(&after, YT_F93) != 9.0f)
		goto done;

	/* The failing host lookup follows the current group's metadata PUT. */
	memset(location, 0, sizeof(location));
	memset(size, 0, sizeof(size));
	location[1] = 31.0f;
	size[1] = 1.0f;
	yt_error_clear(&error);
	if (yt_maintenance_xannor_groups_persist(&game, location, size,
	    &error)
	    || !yt_database_read(&game.database,
	    (size_t)yt_sector_basic_record(&game.config, 1), &after, &error)
	    || yt_record_get_number(&after, YT_F105) != 31.0f)
		goto done;
	valid = true;

done:
	yt_game_close(&game);
	(void)remove("YTDATA.DAT");
	return valid;
}

static bool
check_maintenance_xannor_route_arrivals_pass(void)
{
	static const uint8_t four_zero_draws[12] = {0};
	struct yt_record before[4];
	struct yt_record after;
	struct yt_maintenance_route_cache cache = {0};
	struct yt_maintenance_xannor_route_result route;
	struct yt_game game;
	struct yt_error error;
	struct score_random_script random_script = {
		four_zero_draws, sizeof(four_zero_draws), 0U
	};
	float player_sector[4] = {0};
	float player_cloak[4] = {0};
	float location[21] = {0};
	float size[21] = {0};
	int sector;
	bool valid = false;

	(void)remove("YTDATA.DAT");
	(void)remove("YTNEWS.DAT");
	memset(&game, 0, sizeof(game));
	game.config.sector_offset = 3.0f;
	game.config.port_offset = 7.0f;
	yt_random_init(&game.random);
	yt_random_set_provider(&game.random, score_random_fill, &random_script);
	yt_error_clear(&error);
	if (!yt_database_open(&game.database, "YTDATA.DAT", YT_OPEN_CREATE,
	    &error))
		goto done;
	for (sector = 1; sector <= 4; ++sector) {
		memset(before[sector - 1].bytes, 0x30 + sector,
		    YT_RECORD_SIZE);
		yt_record_set_number(&before[sector - 1], YT_F41,
		    sector == 1 ? 2.0f : sector == 2 ? 3.0f : 0.0f);
		yt_record_set_number(&before[sector - 1], YT_F45, 0.0f);
		yt_record_set_number(&before[sector - 1], YT_F49, 0.0f);
		yt_record_set_number(&before[sector - 1], YT_F53, 0.0f);
		yt_record_set_number(&before[sector - 1], YT_F57, 0.0f);
		yt_record_set_number(&before[sector - 1], YT_F61, 0.0f);
		yt_record_set_number(&before[sector - 1], YT_F81, 0.0f);
		yt_record_set_number(&before[sector - 1], YT_F85, 0.0f);
		yt_record_set_number(&before[sector - 1], YT_F93, 0.0f);
		yt_record_set_number(&before[sector - 1], YT_F129, 0.0f);
		if (!yt_database_write(&game.database,
		    (size_t)yt_sector_basic_record(&game.config, sector),
		    &before[sector - 1], &error))
			goto done;
	}
	location[2] = 1.0f;
	size[2] = 10.0f;
	if (!yt_maintenance_xannor_route_arrivals(&game, &cache,
	    player_sector, player_cloak, YT_ARRAY_LEN(player_sector), 2, 3,
	    location, size, &route, &error)
	    || route.hops != 2 || !route.reached_target
	    || route.route_missing || route.exhausted
	    || location[2] != 3.0f || size[2] != 10.0f
	    || game.random.draws != 4U
	    || random_script.position != sizeof(four_zero_draws))
		goto done;
	for (sector = 1; sector <= 4; ++sector) {
		if (!yt_database_read(&game.database,
		    (size_t)yt_sector_basic_record(&game.config, sector),
		    &after, &error)
		    || memcmp(after.bytes, before[sector - 1].bytes,
		    YT_RECORD_SIZE) != 0)
			goto done;
	}
	if (!yt_maintenance_xannor_route_arrivals(&game, &cache,
	    player_sector, player_cloak, YT_ARRAY_LEN(player_sector), 2, 4,
	    location, size, &route, &error)
	    || route.hops != 0 || route.reached_target
	    || !route.route_missing || route.exhausted
	    || location[2] != 3.0f || size[2] != 10.0f)
		goto done;
	location[2] = 1.0f;
	size[2] = 0.0f;
	if (!yt_maintenance_xannor_route_arrivals(&game, &cache,
	    player_sector, player_cloak, YT_ARRAY_LEN(player_sector), 2, 3,
	    location, size, &route, &error)
	    || route.hops != 0 || route.reached_target
	    || route.route_missing || !route.exhausted
	    || location[2] != 0.0f || size[2] != 0.0f)
		goto done;
	location[2] = 0.6f;
	size[2] = 10.0f;
	if (!yt_maintenance_xannor_route_arrivals(&game, &cache,
	    player_sector, player_cloak, YT_ARRAY_LEN(player_sector), 2, 3,
	    location, size, &route, &error)
	    || route.hops != 0 || route.reached_target
	    || route.route_missing || !route.exhausted
	    || location[2] != 0.0f || size[2] != 0.0f)
		goto done;
	valid = true;

done:
	yt_maintenance_route_cache_free(&cache);
	yt_game_close(&game);
	(void)remove("YTDATA.DAT");
	(void)remove("YTNEWS.DAT");
	return valid;
}

static bool
check_maintenance_xannor_headquarters_reclaim_pass(void)
{
	static const uint8_t success_draws[] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	static const uint8_t failure_draw[] = {0xff, 0xff, 0xff};
	static const uint8_t success_screen[] =
	    " *** The Xannor are attempting to reclaim their base from "
	    "Al\0i!\r *** Successful!\r";
	static const uint8_t success_news[] =
	    " *** The Xannor are attempting to reclaim their base from "
	    "Al\0i!\r\n *** Successful!\r\n\x1a";
	static const uint8_t failure_screen[] =
	    " *** The Xannor are attempting to reclaim their base from "
	    "The Mercenaries!\r *** Failed!\r";
	struct score_random_script script = {
		success_draws, sizeof(success_draws), 0U
	};
	struct score_line_tape screen = {0};
	struct yt_maintenance_xannor_reclaim_result reclaim;
	struct yt_text_file news = {0};
	struct yt_record player;
	struct yt_record host_before;
	struct yt_record host_after;
	struct yt_game game;
	struct yt_error error;
	float location[21] = {0};
	float size[21] = {0};
	size_t offset;
	bool valid = false;

	(void)remove("YTDATA.DAT");
	(void)remove("YTNEWS.DAT");
	memset(&game, 0, sizeof(game));
	game.config.sector_offset = 3.0f;
	game.config.port_offset = 6.0f;
	game.config.headquarters = 2.0f;
	yt_random_init(&game.random);
	yt_random_set_provider(&game.random, score_random_fill, &script);
	yt_error_clear(&error);
	if (!yt_database_open(&game.database, "YTDATA.DAT", YT_OPEN_CREATE,
	    &error))
		goto done;
	memset(player.bytes, 0xa5, YT_RECORD_SIZE);
	memcpy(player.bytes, "Al\0i", 4U);
	yt_record_set_number(&player, YT_F85, 4.0f);
	memset(host_before.bytes, 0x6d, YT_RECORD_SIZE);
	yt_record_set_number(&host_before, YT_F81, 2.0f);
	yt_record_set_number(&host_before, YT_F85, 2.0f);
	if (!yt_database_write(&game.database, 2U, &player, &error)
	    || !yt_database_write(&game.database,
	    (size_t)yt_sector_basic_record(&game.config, 2),
	    &host_before, &error))
		goto done;
	location[1] = 2.0f;
	size[1] = 2.0f;
	if (!yt_maintenance_xannor_headquarters_reclaim(&game, location, size,
	    score_line_collect, &screen, &reclaim, &error)
	    || !reclaim.original_hostile || !reclaim.attempted
	    || !reclaim.successful || reclaim.defenders_after != 0.0
	    || reclaim.draws_consumed != 2U || size[1] != 2.0f
	    || location[1] != 2.0f || game.random.draws != 2U
	    || script.position != sizeof(success_draws)
	    || screen.length != sizeof(success_screen) - 1U
	    || memcmp(screen.data, success_screen,
	    sizeof(success_screen) - 1U) != 0
	    || !yt_text_read("YTNEWS.DAT", &news, &error)
	    || news.length != sizeof(success_news) - 1U
	    || memcmp(news.data, success_news, sizeof(success_news) - 1U) != 0
	    || !yt_database_read(&game.database,
	    (size_t)yt_sector_basic_record(&game.config, 2),
	    &host_after, &error)
	    || yt_record_get_number(&host_after, YT_F81) != 0.0f
	    || yt_record_get_number(&host_after, YT_F85) != 0.0f)
		goto done;
	for (offset = 0; offset < YT_RECORD_SIZE; ++offset) {
		if ((offset < YT_F81 || offset >= YT_F81 + 4U)
		    && (offset < YT_F85 || offset >= YT_F85 + 4U)
		    && host_after.bytes[offset] != host_before.bytes[offset])
			goto done;
	}
	yt_text_free(&news);
	(void)remove("YTNEWS.DAT");
	memset(&screen, 0, sizeof(screen));
	yt_record_set_number(&host_before, YT_F81, 1.0f);
	yt_record_set_number(&host_before, YT_F85, -2.0f);
	if (!yt_database_write(&game.database,
	    (size_t)yt_sector_basic_record(&game.config, 2),
	    &host_before, &error))
		goto done;
	script = (struct score_random_script){
		failure_draw, sizeof(failure_draw), 0U
	};
	yt_random_set_provider(&game.random, score_random_fill, &script);
	location[1] = 2.0f;
	size[1] = 1.0f;
	if (!yt_maintenance_xannor_headquarters_reclaim(&game, location, size,
	    score_line_collect, &screen, &reclaim, &error)
	    || !reclaim.original_hostile || !reclaim.attempted
	    || reclaim.successful || reclaim.defenders_after != 1.0
	    || reclaim.draws_consumed != 1U || size[1] != 0.0f
	    || location[1] != 2.0f || game.random.draws != 1U
	    || script.position != sizeof(failure_draw)
	    || screen.length != sizeof(failure_screen) - 1U
	    || memcmp(screen.data, failure_screen,
	    sizeof(failure_screen) - 1U) != 0
	    || !yt_database_read(&game.database,
	    (size_t)yt_sector_basic_record(&game.config, 2),
	    &host_after, &error)
	    || yt_record_get_number(&host_after, YT_F81) != 1.0f
	    || yt_record_get_number(&host_after, YT_F85) != -2.0f)
		goto done;
	valid = true;

done:
	yt_text_free(&news);
	yt_game_close(&game);
	(void)remove("YTDATA.DAT");
	(void)remove("YTNEWS.DAT");
	return valid;
}

static bool
check_maintenance_xannor_headquarters_relocation_pass(void)
{
	static const uint8_t success_draws[] = {
		0x00, 0x00, 0x00, 0xff, 0xff, 0xff
	};
	static const uint8_t failure_draw[] = {0xff, 0xff, 0xff};
	static const uint8_t expected_screen[] =
	    " *** The Xannor have MOVED their Headquarters! ***\a\r"
	    "\r";
	static const uint8_t expected_news[] =
	    " *** The Xannor have MOVED their Headquarters! ***\a\r\n\x1a";
	struct score_random_script script = {
		success_draws, sizeof(success_draws), 0U
	};
	struct score_line_tape screen = {0};
	struct yt_maintenance_xannor_relocation_result relocation;
	struct yt_text_file news = {0};
	struct yt_record config_before;
	struct yt_record config_after;
	struct yt_record sector_before[11];
	struct yt_record sector_after;
	struct yt_game game;
	struct yt_error error;
	struct score_database_read_fault read_fault = {0U, 3U};
	float location[21] = {0};
	size_t offset;
	int sector;
	bool valid = false;

	(void)remove("YTDATA.DAT");
	(void)remove("YTNEWS.DAT");
	memset(&game, 0, sizeof(game));
	game.config.sector_offset = 1.0f;
	game.config.port_offset = 12.0f;
	game.config.planet_offset = 12.0f;
	game.config.total_records = 112.0f;
	game.config.headquarters = 8.0f;
	yt_random_init(&game.random);
	yt_random_set_provider(&game.random, score_random_fill, &script);
	yt_error_clear(&error);
	if (!yt_database_open(&game.database, "YTDATA.DAT", YT_OPEN_CREATE,
	    &error))
		goto done;
	memset(config_before.bytes, 0x35, YT_RECORD_SIZE);
	if (!yt_record_set_number(&config_before, YT_F117, 8.0f)
	    || !yt_database_write(&game.database, 1U, &config_before, &error))
		goto done;
	for (sector = 1; sector <= 11; ++sector) {
		memset(sector_before[sector - 1].bytes, 0x50 + sector,
		    YT_RECORD_SIZE);
		if (!yt_record_set_number(&sector_before[sector - 1], YT_F81,
		    0.0f)
		    || !yt_record_set_number(&sector_before[sector - 1], YT_F85,
		    0.0f)
		    || !yt_record_set_number(&sector_before[sector - 1], YT_F93,
		    0.0f))
			goto done;
	}
	if (!yt_record_set_number(&sector_before[7], YT_F81, 2.0f)
	    || !yt_record_set_number(&sector_before[7], YT_F85, 7.0f)
	    || !yt_record_set_number(&sector_before[7], YT_F93, 100.0f))
		goto done;
	for (sector = 1; sector <= 11; ++sector) {
		if (!yt_database_write(&game.database,
		    (size_t)yt_sector_basic_record(&game.config, sector),
		    &sector_before[sector - 1], &error))
			goto done;
	}
	location[1] = 8.0f;
	/* The false predicate is a true no-op, including the RNG stream. */
	if (!yt_maintenance_xannor_headquarters_relocate(&game, location,
	    false, 1.0f, 2.0, NULL, 0U,
	    score_line_collect, &screen, &relocation, &error)
	    || relocation.triggered || relocation.draws_consumed != 0U
	    || game.random.draws != 0U || script.position != 0U
	    || screen.length != 0U || location[1] != 8.0f
	    || game.config.headquarters != 8.0f)
		goto done;
	if (!yt_maintenance_xannor_headquarters_relocate(&game, location,
	    true, 1.0f, 0.0, NULL, 0U,
	    score_line_collect, &screen, &relocation, &error)
	    || !relocation.triggered || relocation.old_headquarters != 8
	    || relocation.target_sector != 11 || relocation.attempts != 2
	    || relocation.draws_consumed != 2U || game.random.draws != 2U
	    || script.position != sizeof(success_draws)
	    || game.config.headquarters != 11.0f || location[1] != 11.0f
	    || screen.length != sizeof(expected_screen) - 1U
	    || memcmp(screen.data, expected_screen,
	    sizeof(expected_screen) - 1U) != 0
	    || !yt_text_read("YTNEWS.DAT", &news, &error)
	    || news.length != sizeof(expected_news) - 1U
	    || memcmp(news.data, expected_news, sizeof(expected_news) - 1U) != 0
	    || !yt_database_read(&game.database, 1U, &config_after, &error)
	    || yt_record_get_number(&config_after, YT_F117) != 11.0f)
		goto done;
	for (offset = 0; offset < YT_RECORD_SIZE; ++offset) {
		if ((offset < YT_F117 || offset >= YT_F117 + 4U)
		    && config_after.bytes[offset] != config_before.bytes[offset])
			goto done;
	}
	if (!yt_database_read(&game.database,
	    (size_t)yt_sector_basic_record(&game.config, 8),
	    &sector_after, &error)
	    || yt_record_get_number(&sector_after, YT_F93) != 0.0f)
		goto done;
	for (offset = 0; offset < YT_RECORD_SIZE; ++offset) {
		if ((offset < YT_F93 || offset >= YT_F93 + 4U)
		    && sector_after.bytes[offset] != sector_before[7].bytes[offset])
			goto done;
	}
	if (!yt_database_read(&game.database,
	    (size_t)yt_sector_basic_record(&game.config, 11),
	    &sector_after, &error)
	    || yt_record_get_number(&sector_after, YT_F93) != 100.0f)
		goto done;
	for (offset = 0; offset < YT_RECORD_SIZE; ++offset) {
		if ((offset < YT_F93 || offset >= YT_F93 + 4U)
		    && sector_after.bytes[offset] != sector_before[10].bytes[offset])
			goto done;
	}

	/* The config PUT precedes the old-sector GET and is not rolled back. */
	yt_text_free(&news);
	(void)remove("YTNEWS.DAT");
	memset(&screen, 0, sizeof(screen));
	config_before = config_after;
	if (!yt_record_set_number(&config_before, YT_F117, 12.0f)
	    || !yt_database_write(&game.database, 1U, &config_before, &error)
	    || !yt_record_set_number(&sector_before[10], YT_F93, 0.0f)
	    || !yt_database_write(&game.database,
	    (size_t)yt_sector_basic_record(&game.config, 11),
	    &sector_before[10], &error))
		goto done;
	script = (struct score_random_script){
		failure_draw, sizeof(failure_draw), 0U
	};
	yt_random_set_provider(&game.random, score_random_fill, &script);
	game.config.headquarters = 12.0f;
	location[1] = 12.0f;
	yt_database_set_read_provider(&game.database,
	    score_database_read_with_fault, &read_fault);
	if (yt_maintenance_xannor_headquarters_relocate(&game, location,
	    true, 1.0f, 0.0, NULL, 0U,
	    score_line_collect, &screen, &relocation, &error)
	    || game.random.draws != 1U
	    || script.position != sizeof(failure_draw)
	    || read_fault.calls != 3U
	    || game.database.last_get.outcome != YT_DATABASE_GET_READ_ERROR
	    || game.database.last_get.basic_error != 57U
	    || game.database.last_get.dos_error != 6U
	    || game.database.last_get.terminal_position != 0x55667788
	    || game.config.headquarters != 11.0f || location[1] != 11.0f
	    || screen.length != 0U)
		goto done;
	yt_database_set_read_provider(&game.database, NULL, NULL);
	yt_error_clear(&error);
	if (!yt_database_read(&game.database, 1U, &config_after, &error)
	    || yt_record_get_number(&config_after, YT_F117) != 11.0f
	    || yt_text_read("YTNEWS.DAT", &news, &error))
		goto done;
	valid = true;

done:
	yt_database_set_read_provider(&game.database, NULL, NULL);
	yt_text_free(&news);
	yt_game_close(&game);
	(void)remove("YTDATA.DAT");
	(void)remove("YTNEWS.DAT");
	return valid;
}

static bool
check_maintenance_xannor_revenge_slot_pass(void)
{
	static const uint8_t expected_screen[] =
	    "\r *** Xannor REVENGE! ***\a\r\r";
	static const uint8_t expected_news[] =
	    " *** Xannor REVENGE! ***\a\r\n\x1a";
	static const uint8_t cleared_raw[4] = {0x00, 0x00, 0x28, 0x00};
	static const uint8_t input_dirty_zero[4] = {0x12, 0x34, 0x56, 0x00};
	struct score_line_tape screen = {0};
	struct yt_maintenance_xannor_revenge_result revenge;
	struct yt_text_file news = {0};
	struct yt_record metadata_before;
	struct yt_record metadata_after;
	struct yt_record player;
	struct yt_game game;
	struct yt_error error;
	float player_sector[8] = {0};
	size_t offset;
	bool valid = false;

	(void)remove("YTDATA.DAT");
	(void)remove("YTNEWS.DAT");
	memset(&game, 0, sizeof(game));
	game.config.sector_offset = 10.0f;
	game.config.port_offset = 41.0f;
	yt_error_clear(&error);
	if (!yt_database_open(&game.database, "YTDATA.DAT", YT_OPEN_CREATE,
	    &error))
		goto done;
	memset(player.bytes, 0xa3, YT_RECORD_SIZE);
	if (!yt_record_set_number(&player, YT_F57, 733.0f)
	    || !yt_database_write(&game.database, 4U, &player, &error))
		goto done;
	memset(metadata_before.bytes, 0x6c, YT_RECORD_SIZE);
	if (!yt_record_set_number(&metadata_before, YT_F105, 4.0f)
	    || !yt_database_write(&game.database,
	    (size_t)yt_sector_basic_record(&game.config, 21),
	    &metadata_before, &error))
		goto done;
	player_sector[4] = 902.0f;
	if (!yt_maintenance_xannor_revenge_slot(&game, player_sector,
	    YT_ARRAY_LEN(player_sector), NULL, 0U,
	    score_line_collect, &screen, &revenge, &error)
	    || !revenge.eligible || revenge.live_sector != 733
	    || revenge.cached_target != 902
	    || screen.length != sizeof(expected_screen) - 1U
	    || memcmp(screen.data, expected_screen,
	    sizeof(expected_screen) - 1U) != 0
	    || !yt_text_read("YTNEWS.DAT", &news, &error)
	    || news.length != sizeof(expected_news) - 1U
	    || memcmp(news.data, expected_news, sizeof(expected_news) - 1U) != 0
	    || !yt_database_read(&game.database,
	    (size_t)yt_sector_basic_record(&game.config, 21),
	    &metadata_after, &error)
	    || memcmp(metadata_after.bytes + YT_F105, cleared_raw,
	    sizeof(cleared_raw)) != 0)
		goto done;
	for (offset = 0; offset < YT_RECORD_SIZE; ++offset) {
		if ((offset < YT_F105 || offset >= YT_F105 + 4U)
		    && metadata_after.bytes[offset] != metadata_before.bytes[offset])
			goto done;
	}

	/* A positive slot with live sector 7 produces no revenge output. */
	yt_text_free(&news);
	(void)remove("YTNEWS.DAT");
	memset(&screen, 0, sizeof(screen));
	if (!yt_record_set_number(&player, YT_F57, 7.0f)
	    || !yt_database_write(&game.database, 4U, &player, &error)
	    || !yt_record_set_number(&metadata_before, YT_F105, 4.0f)
	    || !yt_database_write(&game.database,
	    (size_t)yt_sector_basic_record(&game.config, 21),
	    &metadata_before, &error)
	    || !yt_maintenance_xannor_revenge_slot(&game, player_sector,
	    YT_ARRAY_LEN(player_sector), NULL, 0U,
	    score_line_collect, &screen, &revenge, &error)
	    || revenge.eligible || revenge.live_sector != 0
	    || revenge.cached_target != 0 || screen.length != 0U
	    || !yt_database_read(&game.database,
	    (size_t)yt_sector_basic_record(&game.config, 21),
	    &metadata_after, &error)
	    || memcmp(metadata_after.bytes + YT_F105, cleared_raw,
	    sizeof(cleared_raw)) != 0)
		goto done;

	/* Raw numeric zero skips the player GET but is still rewritten dirty. */
	if (!yt_record_set_raw_number(&metadata_before, YT_F105,
	    input_dirty_zero)
	    || !yt_database_write(&game.database,
	    (size_t)yt_sector_basic_record(&game.config, 21),
	    &metadata_before, &error)
	    || !yt_maintenance_xannor_revenge_slot(&game, player_sector,
	    YT_ARRAY_LEN(player_sector), NULL, 0U,
	    score_line_collect, &screen, &revenge, &error)
	    || revenge.eligible || screen.length != 0U
	    || !yt_database_read(&game.database,
	    (size_t)yt_sector_basic_record(&game.config, 21),
	    &metadata_after, &error)
	    || memcmp(metadata_after.bytes + YT_F105, cleared_raw,
	    sizeof(cleared_raw)) != 0)
		goto done;
	valid = true;

done:
	yt_text_free(&news);
	yt_game_close(&game);
	(void)remove("YTDATA.DAT");
	(void)remove("YTNEWS.DAT");
	return valid;
}

static bool
check_maintenance_xannor_roaming_split(void)
{
	static const uint8_t half_draws[] = {
		0x00, 0x00, 0x80, 0x00, 0x00, 0x80,
		0x00, 0x00, 0x80, 0x00, 0x00, 0x80
	};
	static const uint8_t expected_output[] =
	    "The Xannor are on the prowl...\r\r";
	static const uint8_t expected_group_two[] =
	    "  -  Group: 2               Size: 1 \r";
	static const uint8_t expected_group_sixteen[] =
	    "  -  Group: 16              Size: 12345 \r";
	static const uint8_t expected_path_error[] =
	    "*** Error - Sector path not found - from sector 1 to sector  5\r";
	struct score_random_script script = {
		half_draws, sizeof(half_draws), 0U
	};
	struct yt_maintenance_xannor_split_result split;
	struct yt_maintenance_output_result output;
	struct yt_random random;
	struct yt_error error;
	int next_group;
	float group_one;
	float group_size;
	float location;

	if (!yt_maintenance_compose_xannor_roaming(
	    NULL, 0U, &output)
	    || output.row_count != 2U
	    || output.rows[0].address != 0x30F7U
	    || output.rows[1].address != 0x3109U
	    || output.output_length != sizeof(expected_output) - 1U
	    || memcmp(output.output, expected_output,
	    sizeof(expected_output) - 1U) != 0
	    || yt_maintenance_compose_xannor_roaming(NULL, 1U, &output)
	    || !yt_maintenance_compose_xannor_group(2, 1.0f, &output)
	    || output.row_count != 1U || output.rows[0].address != 0x34B2U
	    || output.output_length != sizeof(expected_group_two) - 1U
	    || output.final_column != 0U
	    || memcmp(output.output, expected_group_two,
	    sizeof(expected_group_two) - 1U) != 0
	    || !yt_maintenance_compose_xannor_group(16, 12345.0f, &output)
	    || output.output_length != sizeof(expected_group_sixteen) - 1U
	    || memcmp(output.output, expected_group_sixteen,
	    sizeof(expected_group_sixteen) - 1U) != 0
	    || yt_maintenance_compose_xannor_group(1, 1.0f, &output)
	    || !yt_maintenance_compose_xannor_path_error(1.0f, 5.0f, &output)
	    || output.row_count != 1U || output.rows[0].address != 0x38A8U
	    || output.output_length != sizeof(expected_path_error) - 1U
	    || output.final_column != 0U
	    || memcmp(output.output, expected_path_error,
	    sizeof(expected_path_error) - 1U) != 0
	    || yt_maintenance_compose_xannor_path_error(1.0f, 5.0f, NULL)
	    || yt_maintenance_xannor_should_retarget(-1.0f, 1.0f)
	    || yt_maintenance_xannor_should_retarget(0.0f, 1.0f)
	    || !yt_maintenance_xannor_should_retarget(0.5f, 1.0f)
	    || !yt_maintenance_xannor_should_retarget(7.999f, 1.0f)
	    || yt_maintenance_xannor_should_retarget(8.0f, 1.0f)
	    || yt_maintenance_xannor_should_retarget(7.0f, 0.0f)
	    || yt_maintenance_xannor_should_retarget(7.0f, -1.0f)
	    || !yt_maintenance_xannor_route_complete(7.0f, 7)
	    || yt_maintenance_xannor_route_complete(6.999f, 7)
	    || yt_maintenance_xannor_route_complete(8.0f, 7)
	    || yt_maintenance_xannor_bypass_initial_arrival(19, 9.0f, 9.0f)
	    || yt_maintenance_xannor_bypass_initial_arrival(20, 8.999f, 9.0f)
	    || !yt_maintenance_xannor_bypass_initial_arrival(20, 9.0f, 9.0f)
	    || !yt_maintenance_xannor_bypass_initial_arrival(20, 9.25f, 9.25f)
	    || yt_maintenance_xannor_should_attack_hunt_player(19, 2)
	    || yt_maintenance_xannor_should_attack_hunt_player(20, 0)
	    || !yt_maintenance_xannor_should_attack_hunt_player(20, 2)
	    || !yt_maintenance_xannor_should_attack_hunt_player(20, -1)
	    || !yt_maintenance_xannor_advance_group(2, &next_group)
	    || next_group != 3
	    || !yt_maintenance_xannor_advance_group(19, &next_group)
	    || next_group != 20
	    || yt_maintenance_xannor_advance_group(20, &next_group)
	    || next_group != 21
	    || !yt_maintenance_xannor_post_planet_exhausted(9.0f, 0.999f)
	    || !yt_maintenance_xannor_post_planet_exhausted(0.0f, 1.0f)
	    || yt_maintenance_xannor_post_planet_exhausted(-1.0f, 1.0f)
	    || yt_maintenance_xannor_post_planet_exhausted(9.0f, 1.0f)
	    || yt_maintenance_xannor_player_scan_admit(20, 9.0f, 9.0f,
	    0.83f, 1.0f)
	    || yt_maintenance_xannor_player_scan_admit(2, 9.0f, 8.0f,
	    0.83f, 1.0f)
	    || yt_maintenance_xannor_player_scan_admit(2, 9.0f, 9.0f,
	    0.83f, 0.499f)
	    || !yt_maintenance_xannor_player_scan_admit(2, 9.0f, 9.0f,
	    0.83f, 0.5f)
	    || !yt_maintenance_xannor_player_scan_admit(2, 9.0f, 9.0f,
	    0.33f, 0.0f)
	    || !yt_maintenance_xannor_player_scan_continue(2, 1)
	    || yt_maintenance_xannor_player_scan_continue(3, 1))
		return false;
	yt_random_init(&random);
	yt_random_set_provider(&random, score_random_fill, &script);
	yt_error_clear(&error);

	/* Exact one in both planes bypasses the split gate. */
	group_one = 100.0f;
	group_size = 1.0f;
	location = 1.0f;
	if (!yt_maintenance_xannor_roaming_split(&random, 2, &group_one,
	    &group_size, &location, 200000.0f, 733.0f, &split, &error)
	    || split.split || split.skip_group || split.draws_consumed != 0U
	    || group_one != 100.0f || group_size != 1.0f || location != 1.0f
	    || random.draws != 0U || script.position != 0U)
		return false;

	/* A zero location admits and overwrites an existing positive size. */
	group_one = 100.0f;
	group_size = 10.0f;
	location = 0.0f;
	if (!yt_maintenance_xannor_roaming_split(&random, 2, &group_one,
	    &group_size, &location, 200000.0f, 733.0f, &split, &error)
	    || !split.split || split.skip_group || split.draws_consumed != 4U
	    || split.group_one_after != 92.0f
	    || split.group_size_after != 8.0f
	    || split.group_location_after != 733.0f
	    || group_one != 92.0f || group_size != 8.0f
	    || location != 733.0f || random.draws != 4U
	    || script.position != sizeof(half_draws))
		return false;

	/* Below-one size also admits, but a failed threshold consumes no draw. */
	script = (struct score_random_script){
		half_draws, sizeof(half_draws), 0U
	};
	yt_random_set_provider(&random, score_random_fill, &script);
	group_one = 99.0f;
	group_size = 0.5f;
	location = 900.0f;
	if (!yt_maintenance_xannor_roaming_split(&random, 19, &group_one,
	    &group_size, &location, 200000.0f, 733.0f, &split, &error)
	    || split.split || !split.skip_group || split.draws_consumed != 0U
	    || group_one != 99.0f || group_size != 0.5f
	    || location != 900.0f || random.draws != 0U
	    || script.position != 0U)
		return false;

	/* At a zero threshold, zero group 1 overwrites without calling RND. */
	group_one = 0.0f;
	group_size = 10.0f;
	location = 0.0f;
	if (!yt_maintenance_xannor_roaming_split(&random, 20, &group_one,
	    &group_size, &location, 0.0f, 733.0f, &split, &error)
	    || !split.split || split.skip_group || split.draws_consumed != 0U
	    || group_one != 0.0f || group_size != 0.0f
	    || location != 733.0f || random.draws != 0U
	    || script.position != 0U)
		return false;
	return true;
}

static bool
check_maintenance_xannor_candidate_discovery(void)
{
	static const uint8_t player_match_draws[] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x20,
		0x00, 0x00, 0xc0, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0x00, 0x00, 0x40, 0x00, 0x00, 0x50
	};
	static const uint8_t immediate_draws[] = {
		0x00, 0x00, 0x20, 0x00, 0x00, 0xc0
	};
	static const uint8_t low_discovery_draws[] = {
		0x00, 0x00, 0x20, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00
	};
	uint8_t ordinary_failure_draws[52U * 3U] = {0};
	uint8_t revenge_failure_draws[1276U * 3U] = {0};
	struct score_random_script script = {
		player_match_draws, sizeof(player_match_draws), 0U
	};
	struct yt_maintenance_xannor_discovery_result discovery;
	struct yt_record before[10];
	struct yt_record after;
	struct yt_game game;
	struct yt_error error;
	float player_sector[52] = {0};
	float player_cloak[52] = {0};
	int target;
	int sector;
	bool valid = false;

	(void)remove("YTDATA.DAT");
	memset(&game, 0, sizeof(game));
	game.config.sector_offset = 60.0f;
	game.config.port_offset = 70.0f;
	yt_random_init(&game.random);
	yt_random_set_provider(&game.random, score_random_fill, &script);
	yt_error_clear(&error);
	if (!yt_database_open(&game.database, "YTDATA.DAT", YT_OPEN_CREATE,
	    &error))
		goto done;
	for (sector = 1; sector <= 10; ++sector) {
		memset(before[sector - 1].bytes, 0x30 + sector, YT_RECORD_SIZE);
		if (!yt_record_set_number(&before[sector - 1], YT_F81, 0.0f)
		    || !yt_record_set_number(&before[sector - 1], YT_F85, 0.0f)
		    || !yt_record_set_number(&before[sector - 1], YT_F93, 0.0f)
		    || !yt_database_write(&game.database,
		    (size_t)yt_sector_basic_record(&game.config, sector),
		    &before[sector - 1], &error))
			goto done;
	}
	player_sector[5] = 8.0f;
	player_sector[6] = 8.0f;
	player_cloak[5] = 0.25f;
	player_cloak[6] = 0.25f;
	if (!yt_maintenance_xannor_candidate_discovery(&game, player_sector,
	    player_cloak, YT_ARRAY_LEN(player_sector), 1, 0, 0,
	    &discovery, &error)
	    || discovery.initial_target != 2 || discovery.initial_draws != 2
	    || discovery.discovery_target != 8 || discovery.target_sector != 8
	    || discovery.attempts != 1 || discovery.player_draws != 5
	    || discovery.selected_player_record != 6
	    || discovery.draws_consumed != 8U || game.random.draws != 8U
	    || script.position != sizeof(player_match_draws))
		goto done;

	/* Immediate sector interest replaces a cached revenge target. */
	if (!yt_record_set_number(&before[7], YT_F81, 2.0f)
	    || !yt_record_set_number(&before[7], YT_F85, 3.0f)
	    || !yt_database_write(&game.database,
	    (size_t)yt_sector_basic_record(&game.config, 8), &before[7],
	    &error))
		goto done;
	script = (struct score_random_script){
		immediate_draws, sizeof(immediate_draws), 0U
	};
	yt_random_set_provider(&game.random, score_random_fill, &script);
	if (!yt_maintenance_xannor_candidate_discovery(&game, player_sector,
	    player_cloak, YT_ARRAY_LEN(player_sector), 1, 22, 9,
	    &discovery, &error)
	    || discovery.discovery_target != 8 || discovery.target_sector != 8
	    || discovery.player_draws != 0 || discovery.draws_consumed != 2U
	    || script.position != sizeof(immediate_draws))
		goto done;

	/* Without immediate interest, cached revenge exits before player draws. */
	if (!yt_record_set_number(&before[7], YT_F81, 0.0f)
	    || !yt_record_set_number(&before[7], YT_F85, 0.0f)
	    || !yt_database_write(&game.database,
	    (size_t)yt_sector_basic_record(&game.config, 8), &before[7],
	    &error))
		goto done;
	script = (struct score_random_script){
		immediate_draws, sizeof(immediate_draws), 0U
	};
	yt_random_set_provider(&game.random, score_random_fill, &script);
	if (!yt_maintenance_xannor_candidate_discovery(&game, player_sector,
	    player_cloak, YT_ARRAY_LEN(player_sector), 1, 22, 9,
	    &discovery, &error)
	    || discovery.discovery_target != 9 || discovery.target_sector != 9
	    || discovery.player_draws != 0 || discovery.draws_consumed != 2U)
		goto done;

	/* Xannor-owned fighters are uninteresting; a planet link still wins. */
	if (!yt_record_set_number(&before[7], YT_F81, 2.0f)
	    || !yt_record_set_number(&before[7], YT_F85, -1.0f)
	    || !yt_database_write(&game.database,
	    (size_t)yt_sector_basic_record(&game.config, 8), &before[7],
	    &error))
		goto done;
	script = (struct score_random_script){
		immediate_draws, sizeof(immediate_draws), 0U
	};
	yt_random_set_provider(&game.random, score_random_fill, &script);
	if (!yt_maintenance_xannor_candidate_discovery(&game, player_sector,
	    player_cloak, YT_ARRAY_LEN(player_sector), 1, 22, 9,
	    &discovery, &error) || discovery.discovery_target != 9)
		goto done;
	if (!yt_record_set_number(&before[7], YT_F81, 0.0f)
	    || !yt_record_set_number(&before[7], YT_F85, 0.0f)
	    || !yt_record_set_number(&before[7], YT_F93, 2.0f)
	    || !yt_database_write(&game.database,
	    (size_t)yt_sector_basic_record(&game.config, 8), &before[7],
	    &error))
		goto done;
	script = (struct score_random_script){
		immediate_draws, sizeof(immediate_draws), 0U
	};
	yt_random_set_provider(&game.random, score_random_fill, &script);
	if (!yt_maintenance_xannor_candidate_discovery(&game, player_sector,
	    player_cloak, YT_ARRAY_LEN(player_sector), 1, 22, 9,
	    &discovery, &error) || discovery.discovery_target != 8)
		goto done;
	if (!yt_record_set_number(&before[7], YT_F93, 0.0f)
	    || !yt_database_write(&game.database,
	    (size_t)yt_sector_basic_record(&game.config, 8), &before[7],
	    &error))
		goto done;

	memset(player_sector, 0, sizeof(player_sector));
	memset(player_cloak, 0, sizeof(player_cloak));
	script = (struct score_random_script){
		ordinary_failure_draws, sizeof(ordinary_failure_draws), 0U
	};
	yt_random_set_provider(&game.random, score_random_fill, &script);
	if (!yt_maintenance_xannor_candidate_discovery(&game, player_sector,
	    player_cloak, YT_ARRAY_LEN(player_sector), 10, 0, 0,
	    &discovery, &error)
	    || discovery.initial_target != 1 || discovery.discovery_target != 0
	    || discovery.target_sector != 1 || discovery.attempts != 1
	    || discovery.player_draws != 50
	    || discovery.draws_consumed != 52U
	    || script.position != sizeof(ordinary_failure_draws))
		goto done;

	script = (struct score_random_script){
		revenge_failure_draws, sizeof(revenge_failure_draws), 0U
	};
	yt_random_set_provider(&game.random, score_random_fill, &script);
	if (!yt_maintenance_xannor_candidate_discovery(&game, player_sector,
	    player_cloak, YT_ARRAY_LEN(player_sector), 10, 22, 0,
	    &discovery, &error)
	    || discovery.attempts != 25 || discovery.player_draws != 1250
	    || discovery.draws_consumed != 1276U
	    || script.position != sizeof(revenge_failure_draws))
		goto done;

	/* A discovered protected sector stops discovery but does not replace. */
	player_sector[2] = 1.0f;
	player_cloak[2] = 999.0f;
	script = (struct score_random_script){
		low_discovery_draws, sizeof(low_discovery_draws), 0U
	};
	yt_random_set_provider(&game.random, score_random_fill, &script);
	if (!yt_maintenance_xannor_candidate_discovery(&game, player_sector,
	    player_cloak, YT_ARRAY_LEN(player_sector), 10, 22, 0,
	    &discovery, &error)
	    || discovery.initial_target != 2 || discovery.discovery_target != 1
	    || discovery.target_sector != 2 || discovery.player_draws != 1
	    || discovery.selected_player_record != 2
	    || discovery.draws_consumed != 3U)
		goto done;

	if (!yt_maintenance_xannor_target_override(2, 23, 100.0f,
	    200000.0f, 733, 22, 24, &target, &error) || target != 23
	    || !yt_maintenance_xannor_target_override(16, 23, 100.0f,
	    200000.0f, 733, 22, 24, &target, &error) || target != 24
	    || !yt_maintenance_xannor_target_override(16, 23, 100.0f,
	    200000.0f, 733, 0, 24, &target, &error) || target != 23
	    || !yt_maintenance_xannor_target_override(20, 23, 100.0f,
	    200000.0f, 733, 0, 0, &target, &error) || target != 0
	    || !yt_maintenance_xannor_target_override(20, 23, 99.0f,
	    200000.0f, 733, 22, 24, &target, &error) || target != 733)
		goto done;
	for (sector = 1; sector <= 10; ++sector) {
		if (!yt_database_read(&game.database,
		    (size_t)yt_sector_basic_record(&game.config, sector),
		    &after, &error)
		    || memcmp(after.bytes, before[sector - 1].bytes,
		    YT_RECORD_SIZE) != 0)
			goto done;
	}
	valid = true;

done:
	yt_game_close(&game);
	(void)remove("YTDATA.DAT");
	return valid;
}

static bool
check_date_serial(void)
{
	struct yt_clock_value date = {2026, 1, 1, 0, 0, 0, 0};
	struct score_clock_script script = {{{0}}, 0};
	static const int month_starts[] =
	    {0, 1, 32, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335};
	struct yt_error error;
	int adjusted;
	int serial;
	int month;

	for (month = 1; month <= 12; ++month) {
		date.month = month;
		if (yt_date_serial(&date, 26, &adjusted) != month_starts[month]
		    || adjusted != 26)
			return false;
	}
	date.month = 7;
	date.day = 22;
	if (yt_date_serial(&date, 26, &adjusted) != 203
	    || adjusted != 26)
		return false;
	date.day = 23;
	if (yt_date_serial(&date, 26, &adjusted) != 204
	    || adjusted != 26)
		return false;
	date.month = 3;
	date.day = 1;
	date.year = 2028;
	if (yt_date_serial(&date, 28, &adjusted) != 61 || adjusted != 28)
		return false;
	if (yt_date_serial(&date, 27, &adjusted) != 426 || adjusted != 28)
		return false;
	if (yt_date_serial(&date, 26, &adjusted) != 426 || adjusted != 28)
		return false;
	date.year = 2030;
	date.month = 1;
	if (yt_date_serial(&date, 26, &adjusted) != 366 || adjusted != 30)
		return false;
	date.month = 3;
	if (yt_date_serial(&date, 26, &adjusted) != 425 || adjusted != 30)
		return false;
	date.month = 1;
	date.year = 2100;
	if (yt_date_serial(&date, 98, &adjusted) != 366 || adjusted != 100)
		return false;
	date.month = 3;
	if (yt_date_serial(&date, 99, &adjusted) != 426 || adjusted != 100)
		return false;
	if (yt_date_serial(&date, 100, &adjusted) != 61 || adjusted != 100)
		return false;
	date.year = 2028;
	script.values[0] = date;
	yt_error_clear(&error);
	yt_platform_set_clock_provider(score_clock_read, &script);
	if (!yt_current_date_serial(27.5f, &serial, &adjusted, &error)
	    || serial != 61 || adjusted != 28 || script.position != 1U) {
		yt_platform_set_clock_provider(NULL, NULL);
		return false;
	}
	yt_platform_set_clock_provider(NULL, NULL);
	return true;
}

static bool
check_maintenance_route_enqueue(void)
{
	int queue[5] = {1, 9, 9, 9, 9};
	int previous[5] = {0, -1, 0, 4, 0};
	size_t tail = 1U;

	if (yt_maintenance_route_enqueue(0, 1, 4, queue, 5U, &tail,
	    previous) != YT_MAINTENANCE_ENQUEUE_SKIPPED || tail != 1U
	    || previous[0] != 0)
		return false;
	if (yt_maintenance_route_enqueue(3, 1, 4, queue, 5U, &tail,
	    previous) != YT_MAINTENANCE_ENQUEUE_SKIPPED || tail != 1U
	    || previous[3] != 4)
		return false;
	if (yt_maintenance_route_enqueue(2, 1, 4, queue, 5U, &tail,
	    previous) != YT_MAINTENANCE_ENQUEUE_ADDED || tail != 2U
	    || queue[1] != 2 || previous[2] != 1)
		return false;
	if (yt_maintenance_route_enqueue(4, 2, 4, queue, 5U, &tail,
	    previous) != YT_MAINTENANCE_ENQUEUE_ADDED || tail != 3U
	    || queue[2] != 4 || previous[4] != 2)
		return false;
	if (yt_maintenance_route_enqueue(5, 2, 4, queue, 5U, &tail,
	    previous) != YT_MAINTENANCE_ENQUEUE_INVALID || tail != 3U)
		return false;
	tail = 5U;
	previous[3] = 0;
	return yt_maintenance_route_enqueue(3, 2, 4, queue, 5U, &tail,
	    previous) == YT_MAINTENANCE_ENQUEUE_INVALID
	    && tail == 5U && previous[3] == 0;
}

static bool
check_player_name_match(void)
{
	struct yt_record record;
	struct yt_player player;
	struct yt_error error;
	uint8_t full_field[YT_TEXT_FIELD_SIZE];
	uint8_t stored_name[YT_TEXT_FIELD_SIZE];
	uint8_t radio_prompt[YT_TEXT_FIELD_SIZE + sizeof(" [Y]? ") - 1U];
	static const uint8_t expected_radio_prompt[] = {
		'A', 0, 'B', 'O', 'B', ' ', '[', 'Y', ']', '?', ' '
	};
	uint8_t killer_row[YT_TEXT_FIELD_SIZE + 21U];
	size_t stored_length;
	size_t radio_prompt_length;
	size_t killer_length;
	bool emit;
	bool matches;

	yt_record_blank(&record);
	yt_record_set_text(&record, (const uint8_t *)"Solo ", 5);
	yt_record_set_number(&record, YT_F85, 5.0f);
	yt_player_decode(&player, &record);
	yt_error_clear(&error);
	if (!yt_player_name_matches(&player, (const uint8_t *)"Solo ", 5,
	    &matches, &error) || !matches)
		return false;

	memset(record.bytes, ' ', YT_TEXT_FIELD_SIZE);
	memcpy(record.bytes, expected_radio_prompt, 5U);
	yt_record_set_number(&record, YT_F85, 5.0f);
	yt_player_decode(&player, &record);
	if (!yt_fixed_text_contains(record.bytes, (const uint8_t *)"BOB", 3U)
	    || yt_fixed_text_contains(record.bytes, (const uint8_t *)"bob", 3U)
	    || !yt_fixed_text_contains(record.bytes, NULL, 0U)
	    || yt_fixed_text_contains(record.bytes, (const uint8_t *)"ignored",
	    YT_TEXT_FIELD_SIZE + 1U)
	    || !yt_radio_player_prompt(&player, radio_prompt,
	    sizeof(radio_prompt), &radio_prompt_length, &error)
	    || radio_prompt_length != sizeof(expected_radio_prompt)
	    || memcmp(radio_prompt, expected_radio_prompt,
	    sizeof(expected_radio_prompt)) != 0)
		return false;

	memset(record.bytes, 'A', YT_TEXT_FIELD_SIZE);
	yt_record_set_number(&record, YT_F85, 42.0f);
	yt_player_decode(&player, &record);
	memset(full_field, 'A', sizeof(full_field));
	if (!yt_player_name_matches(&player, full_field, sizeof(full_field),
	    &matches, &error) || !matches
	    || !yt_player_name_matches(&player, full_field,
	    sizeof(full_field) - 1U, &matches, &error) || matches)
		return false;
	yt_record_set_text(&record, (const uint8_t *)"Star Lord", 9);
	yt_record_set_number(&record, YT_F85, 2.6f);
	yt_player_decode(&player, &record);
	if (!yt_player_stored_name(&player, stored_name, &stored_length, &error)
	    || stored_length != 3U || memcmp(stored_name, "Sta", 3) != 0)
		return false;
	yt_record_set_number(&record, YT_F85, 0.4f);
	yt_player_decode(&player, &record);
	if (!yt_player_stored_name(&player, stored_name, &stored_length, &error)
	    || stored_length != 0U
	    || !yt_player_killer_row(&player, killer_row,
	    sizeof(killer_row), &killer_length, &emit, &error)
	    || !emit || killer_length != strlen(" destroyed your ship!")
	    || memcmp(killer_row, " destroyed your ship!", killer_length) != 0)
		return false;
	yt_record_set_number(&record, YT_F85, 0.0f);
	yt_player_decode(&player, &record);
	if (!yt_player_killer_row(&player, killer_row, sizeof(killer_row),
	    &killer_length, &emit, &error) || emit || killer_length != 0U)
		return false;

	yt_record_set_number(&record, YT_F85, -0.4f);
	yt_player_decode(&player, &record);
	if (!yt_player_name_matches(&player, NULL, 0, &matches, &error)
	    || !matches)
		return false;
	yt_record_set_number(&record, YT_F85, -0.6f);
	yt_player_decode(&player, &record);
	yt_error_clear(&error);
	if (yt_player_name_matches(&player, NULL, 0, &matches, &error)
	    || error.status != YT_RANGE || matches)
		return false;
	return true;
}

enum hostile_surrender_event {
	HOSTILE_SURRENDER_READ,
	HOSTILE_SURRENDER_RADIO,
	HOSTILE_SURRENDER_SOUND_FOUR,
	HOSTILE_SURRENDER_CAPTAIN,
	HOSTILE_SURRENDER_WISH,
	HOSTILE_SURRENDER_BLANK,
	HOSTILE_SURRENDER_PROMPT,
	HOSTILE_SURRENDER_JOINED,
	HOSTILE_SURRENDER_SOUND_ONE,
	HOSTILE_SURRENDER_NEWS,
	HOSTILE_SURRENDER_COUNT,
	HOSTILE_SURRENDER_XANNOR,
	HOSTILE_SURRENDER_MERCENARY,
	HOSTILE_SURRENDER_SOUND_FIVE,
};

struct hostile_surrender_tape {
	struct yt_player player;
	enum hostile_surrender_event events[16];
	size_t calls;
	size_t fail_at;
	enum yt_hostile_surrender_answer answer;
	uint8_t rows[8][384];
	size_t row_lengths[8];
	uint8_t news[384];
	size_t news_length;
};

static bool
hostile_surrender_event(struct hostile_surrender_tape *tape,
    enum hostile_surrender_event event, struct yt_error *error)
{
	size_t call = tape->calls++;

	if (call < YT_ARRAY_LEN(tape->events))
		tape->events[call] = event;
	if (call != tape->fail_at)
		return true;
	if (error != NULL) {
		error->status = YT_IO_ERROR;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    "hostile surrender injected failure");
	}
	return false;
}

static bool
hostile_surrender_read(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct hostile_surrender_tape *tape = context;

	if (player_record != 2
	    || !hostile_surrender_event(tape, HOSTILE_SURRENDER_READ, error))
		return false;
	*player = tape->player;
	return true;
}

static bool
hostile_surrender_present(void *context, const uint8_t *text, size_t length,
    enum yt_hostile_surrender_output_kind kind, struct yt_error *error)
{
	static const enum hostile_surrender_event events[] = {
		HOSTILE_SURRENDER_RADIO,
		HOSTILE_SURRENDER_CAPTAIN,
		HOSTILE_SURRENDER_WISH,
		HOSTILE_SURRENDER_BLANK,
		HOSTILE_SURRENDER_JOINED,
		HOSTILE_SURRENDER_COUNT,
		HOSTILE_SURRENDER_XANNOR,
		HOSTILE_SURRENDER_MERCENARY,
	};
	struct hostile_surrender_tape *tape = context;

	if ((size_t)kind >= YT_ARRAY_LEN(events)
	    || length > sizeof(tape->rows[0])
	    || !hostile_surrender_event(tape, events[kind], error))
		return false;
	if (length != 0U)
		memcpy(tape->rows[kind], text, length);
	tape->row_lengths[kind] = length;
	return true;
}

static bool
hostile_surrender_sound(void *context, float selector,
    struct yt_error *error)
{
	enum hostile_surrender_event event;

	if (selector == 4.0f)
		event = HOSTILE_SURRENDER_SOUND_FOUR;
	else if (selector == 1.0f)
		event = HOSTILE_SURRENDER_SOUND_ONE;
	else if (selector == 5.0f)
		event = HOSTILE_SURRENDER_SOUND_FIVE;
	else
		return false;
	return hostile_surrender_event(context, event, error);
}

static bool
hostile_surrender_prompt(void *context, const uint8_t *prompt, size_t length,
    enum yt_hostile_surrender_answer *answer, struct yt_error *error)
{
	static const uint8_t expected[] =
	    "Will you accept our surrender? [Y]/N -=>";
	struct hostile_surrender_tape *tape = context;

	if (length != sizeof(expected) - 1U
	    || memcmp(prompt, expected, sizeof(expected) - 1U) != 0
	    || !hostile_surrender_event(tape, HOSTILE_SURRENDER_PROMPT, error))
		return false;
	*answer = tape->answer;
	return true;
}

static bool
hostile_surrender_news(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	struct hostile_surrender_tape *tape = context;

	if (length > sizeof(tape->news)
	    || !hostile_surrender_event(tape, HOSTILE_SURRENDER_NEWS, error))
		return false;
	if (length != 0U)
		memcpy(tape->news, text, length);
	tape->news_length = length;
	return true;
}

static const struct yt_hostile_surrender_ops hostile_surrender_ops = {
	hostile_surrender_read,
	hostile_surrender_present,
	hostile_surrender_sound,
	hostile_surrender_prompt,
	hostile_surrender_news,
};

static void
hostile_surrender_fixture(struct hostile_surrender_tape *tape,
    struct yt_hostile_surrender_state *state, float owner,
    enum yt_hostile_surrender_answer answer)
{
	static const uint8_t cached_name[] = {'A', 0, 'B'};

	memset(tape, 0, sizeof(*tape));
	tape->fail_at = (size_t)-1;
	tape->answer = answer;
	memset(tape->player.record.bytes, 0xa5,
	    sizeof(tape->player.record.bytes));
	tape->player.fighters = 11.0f;
	tape->player.sector = 733.0f;
	*state = (struct yt_hostile_surrender_state){
		.current_player_record = 2,
		.old_owner = owner,
		.attacker_loss = 0.0,
		.defender_loss = 0.0,
		.deployed_fighters = 10.0,
		.cached_player_name = cached_name,
		.cached_player_name_length = sizeof(cached_name),
		.real_first_name = (const uint8_t *)"Sysop",
		.real_first_name_length = 5U,
	};
}

static bool
check_hostile_surrender_transaction(void)
{
	static const enum hostile_surrender_event accepted_events[] = {
		HOSTILE_SURRENDER_READ,
		HOSTILE_SURRENDER_RADIO,
		HOSTILE_SURRENDER_SOUND_FOUR,
		HOSTILE_SURRENDER_CAPTAIN,
		HOSTILE_SURRENDER_WISH,
		HOSTILE_SURRENDER_BLANK,
		HOSTILE_SURRENDER_PROMPT,
		HOSTILE_SURRENDER_JOINED,
		HOSTILE_SURRENDER_SOUND_ONE,
		HOSTILE_SURRENDER_NEWS,
		HOSTILE_SURRENDER_COUNT,
	};
	static const uint8_t radio[] = "RADIO MESSAGE COMING IN!";
	static const uint8_t captain[] =
	    "This is the captain of the fighter group in sector 733";
	static const uint8_t news[] =
	    {' ', '1', '0', ' ', 'f', 'i', 'g', 'h', 't', 'e', 'r', 's', ' ',
	    'i', 'n', ' ', 's', 'e', 'c', 't', 'o', 'r', ' ', '7', '3', '3',
	    ' ', 's', 'u', 'r', 'r', 'e', 'n', 'd', 'e', 'r', 'e', 'd', ' ',
	    't', 'o', ' ', 'A', 0, 'B'};
	static const uint8_t count[] = " 10 fighters surrendered!";
	static const uint8_t xannor[] =
	    "Whee fyte to the deeth hoo-man slyme!";
	static const uint8_t mercenary[] =
	    "We'll DIE before joining with a slyme like you Sysop!";
	struct hostile_surrender_tape tape;
	struct yt_hostile_surrender_state state;
	struct yt_error error;
	size_t failure;

	hostile_surrender_fixture(&tape, &state, 2.0f,
	    YT_HOSTILE_SURRENDER_ANSWER_EMPTY);
	if (!yt_hostile_attack_surrender_run(&state, &hostile_surrender_ops,
	    &tape, NULL)
	    || !state.checked || !state.accepted || !state.complete
	    || state.owner_route != YT_HOSTILE_SURRENDER_PLAYER
	    || state.surrendered_fighters != 10.0
	    || state.ship_fighters != 21.0 || state.current.fighters != 21.0f
	    || state.deployed_remaining != 0.0 || state.fighter_owner != 0.0f
	    || tape.calls != YT_ARRAY_LEN(accepted_events)
	    || memcmp(tape.events, accepted_events, sizeof(accepted_events)) != 0
	    || tape.row_lengths[YT_HOSTILE_SURRENDER_RADIO_ROW]
	    != sizeof(radio) - 1U
	    || memcmp(tape.rows[YT_HOSTILE_SURRENDER_RADIO_ROW], radio,
	    sizeof(radio) - 1U) != 0
	    || tape.row_lengths[YT_HOSTILE_SURRENDER_CAPTAIN_ROW]
	    != sizeof(captain) - 1U
	    || memcmp(tape.rows[YT_HOSTILE_SURRENDER_CAPTAIN_ROW], captain,
	    sizeof(captain) - 1U) != 0
	    || tape.news_length != sizeof(news)
	    || memcmp(tape.news, news, sizeof(news)) != 0
	    || tape.row_lengths[YT_HOSTILE_SURRENDER_COUNT_ROW]
	    != sizeof(count) - 1U
	    || memcmp(tape.rows[YT_HOSTILE_SURRENDER_COUNT_ROW], count,
	    sizeof(count) - 1U) != 0
	    || memcmp(state.current.record.bytes, tape.player.record.bytes,
	    sizeof(state.current.record.bytes)) != 0)
		return false;

	for (failure = 0U; failure < YT_ARRAY_LEN(accepted_events); ++failure) {
		hostile_surrender_fixture(&tape, &state, 2.0f,
		    YT_HOSTILE_SURRENDER_ANSWER_YES);
		tape.fail_at = failure;
		yt_error_clear(&error);
		if (yt_hostile_attack_surrender_run(&state,
		    &hostile_surrender_ops, &tape, &error)
		    || error.status != YT_IO_ERROR || tape.calls != failure + 1U
		    || memcmp(tape.events, accepted_events,
		    (failure + 1U) * sizeof(accepted_events[0])) != 0
		    || state.complete
		    || (failure < 7U && (state.checked || state.accepted))
		    || (failure >= 7U && (!state.checked || !state.accepted)))
			return false;
	}

	hostile_surrender_fixture(&tape, &state, 2.0f,
	    YT_HOSTILE_SURRENDER_ANSWER_NO);
	if (!yt_hostile_attack_surrender_run(&state, &hostile_surrender_ops,
	    &tape, NULL) || !state.checked || state.accepted || !state.complete
	    || tape.calls != 7U || tape.news_length != 0U
	    || state.ship_fighters != 11.0
	    || state.deployed_remaining != 10.0 || state.fighter_owner != 2.0f)
		return false;

	hostile_surrender_fixture(&tape, &state, -1.0f,
	    YT_HOSTILE_SURRENDER_ANSWER_YES);
	if (!yt_hostile_attack_surrender_run(&state, &hostile_surrender_ops,
	    &tape, NULL) || state.owner_route != YT_HOSTILE_SURRENDER_XANNOR
	    || state.accepted || tape.calls != 6U
	    || tape.events[4] != HOSTILE_SURRENDER_XANNOR
	    || tape.events[5] != HOSTILE_SURRENDER_SOUND_FIVE
	    || tape.row_lengths[YT_HOSTILE_SURRENDER_XANNOR_REFUSAL_ROW]
	    != sizeof(xannor) - 1U
	    || memcmp(tape.rows[YT_HOSTILE_SURRENDER_XANNOR_REFUSAL_ROW],
	    xannor, sizeof(xannor) - 1U) != 0)
		return false;

	hostile_surrender_fixture(&tape, &state, -2.0f,
	    YT_HOSTILE_SURRENDER_ANSWER_YES);
	if (!yt_hostile_attack_surrender_run(&state, &hostile_surrender_ops,
	    &tape, NULL) || state.owner_route != YT_HOSTILE_SURRENDER_MERCENARY
	    || state.accepted || tape.calls != 6U
	    || tape.events[4] != HOSTILE_SURRENDER_MERCENARY
	    || tape.events[5] != HOSTILE_SURRENDER_SOUND_FIVE
	    || tape.row_lengths[YT_HOSTILE_SURRENDER_MERCENARY_REFUSAL_ROW]
	    != sizeof(mercenary) - 1U
	    || memcmp(tape.rows[YT_HOSTILE_SURRENDER_MERCENARY_REFUSAL_ROW],
	    mercenary, sizeof(mercenary) - 1U) != 0)
		return false;

	hostile_surrender_fixture(&tape, &state, 1.0f,
	    YT_HOSTILE_SURRENDER_ANSWER_YES);
	if (!yt_hostile_attack_surrender_run(&state, &hostile_surrender_ops,
	    &tape, NULL) || state.owner_route != YT_HOSTILE_SURRENDER_QUIET
	    || state.accepted || !state.complete || tape.calls != 4U)
		return false;

	hostile_surrender_fixture(&tape, &state, 2.0f,
	    YT_HOSTILE_SURRENDER_ANSWER_YES);
	return !yt_hostile_attack_surrender_run(NULL, &hostile_surrender_ops,
	    &tape, NULL)
	    && !yt_hostile_attack_surrender_run(&state, NULL, &tape, NULL);
}

enum hostile_persistence_event {
	HOSTILE_PERSISTENCE_PLAYER_READ_ONE,
	HOSTILE_PERSISTENCE_PLAYER_WRITE,
	HOSTILE_PERSISTENCE_SECTOR_READ,
	HOSTILE_PERSISTENCE_SECTOR_WRITE,
	HOSTILE_PERSISTENCE_BLANK,
	HOSTILE_PERSISTENCE_PLAYER_READ_TWO,
	HOSTILE_PERSISTENCE_NEWS,
	HOSTILE_PERSISTENCE_FATAL,
};

struct hostile_persistence_tape {
	struct yt_player players[2];
	struct yt_sector sector;
	struct yt_player written_player;
	struct yt_sector written_sector;
	enum hostile_persistence_event events[12];
	size_t calls;
	size_t fail_at;
	size_t player_reads;
	uint8_t news[384];
	size_t news_length;
};

static bool
hostile_persistence_event(struct hostile_persistence_tape *tape,
    enum hostile_persistence_event event, struct yt_error *error)
{
	size_t call = tape->calls++;

	if (call < YT_ARRAY_LEN(tape->events))
		tape->events[call] = event;
	if (call != tape->fail_at)
		return true;
	if (error != NULL) {
		error->status = YT_IO_ERROR;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    "hostile persistence injected failure");
	}
	return false;
}

static bool
hostile_persistence_read_player(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct hostile_persistence_tape *tape = context;
	size_t read = tape->player_reads++;
	enum hostile_persistence_event event = read == 0U
	    ? HOSTILE_PERSISTENCE_PLAYER_READ_ONE
	    : HOSTILE_PERSISTENCE_PLAYER_READ_TWO;

	if (player_record != 2 || read >= YT_ARRAY_LEN(tape->players)
	    || !hostile_persistence_event(tape, event, error))
		return false;
	*player = tape->players[read];
	return true;
}

static bool
hostile_persistence_write_player(void *context, int player_record,
    const struct yt_player *player, struct yt_error *error)
{
	struct hostile_persistence_tape *tape = context;

	if (player_record != 2 || !hostile_persistence_event(tape,
	    HOSTILE_PERSISTENCE_PLAYER_WRITE, error))
		return false;
	tape->written_player = *player;
	return true;
}

static bool
hostile_persistence_read_sector(void *context, int sector_number,
    struct yt_sector *sector, struct yt_error *error)
{
	struct hostile_persistence_tape *tape = context;

	if (sector_number != 733 || !hostile_persistence_event(tape,
	    HOSTILE_PERSISTENCE_SECTOR_READ, error))
		return false;
	*sector = tape->sector;
	return true;
}

static bool
hostile_persistence_write_sector(void *context, int sector_number,
    const struct yt_sector *sector, struct yt_error *error)
{
	struct hostile_persistence_tape *tape = context;

	if (sector_number != 733 || !hostile_persistence_event(tape,
	    HOSTILE_PERSISTENCE_SECTOR_WRITE, error))
		return false;
	tape->written_sector = *sector;
	return true;
}

static bool
hostile_persistence_blank(void *context, struct yt_error *error)
{
	return hostile_persistence_event(context, HOSTILE_PERSISTENCE_BLANK,
	    error);
}

static bool
hostile_persistence_news(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	struct hostile_persistence_tape *tape = context;

	if (length > sizeof(tape->news) || !hostile_persistence_event(tape,
	    HOSTILE_PERSISTENCE_NEWS, error))
		return false;
	if (length != 0U)
		memcpy(tape->news, text, length);
	tape->news_length = length;
	return true;
}

static bool
hostile_persistence_fatal(void *context, struct yt_error *error)
{
	return hostile_persistence_event(context, HOSTILE_PERSISTENCE_FATAL,
	    error);
}

static const struct yt_hostile_attack_persistence_ops
hostile_persistence_ops = {
	hostile_persistence_read_player,
	hostile_persistence_write_player,
	hostile_persistence_read_sector,
	hostile_persistence_write_sector,
	hostile_persistence_blank,
	hostile_persistence_news,
	hostile_persistence_fatal,
};

static void
hostile_persistence_fixture(struct hostile_persistence_tape *tape,
    struct yt_hostile_attack_persistence_state *state)
{
	static const uint8_t cached_name[] = {'A', 0, 'B'};
	static const uint8_t owner_label[] = {'X', 0, 'Y'};

	memset(tape, 0, sizeof(*tape));
	tape->fail_at = (size_t)-1;
	memset(tape->players[0].record.bytes, 0xa5,
	    sizeof(tape->players[0].record.bytes));
	tape->players[0].fighters = 90.0f;
	tape->players[0].shields = 80.0f;
	memset(tape->players[1].record.bytes, 0x3c,
	    sizeof(tape->players[1].record.bytes));
	tape->players[1].fighters = 9.25f;
	memset(tape->sector.record.bytes, 0x5a,
	    sizeof(tape->sector.record.bytes));
	tape->sector.fighters = 12.0f;
	tape->sector.fighter_owner = 9.0f;
	*state = (struct yt_hostile_attack_persistence_state){
		.current_player_record = 2,
		.current_sector = 733,
		.ship_fighters = 7.5,
		.shields = 6.25f,
		.deployed_fighters = 0.0,
		.defender_loss = 2.0,
		.old_owner = -2.0f,
		.cached_player_name = cached_name,
		.cached_player_name_length = sizeof(cached_name),
		.owner_label = owner_label,
		.owner_label_length = sizeof(owner_label),
	};
}

static bool
check_hostile_attack_persistence_transaction(void)
{
	static const enum hostile_persistence_event expected_events[] = {
		HOSTILE_PERSISTENCE_PLAYER_READ_ONE,
		HOSTILE_PERSISTENCE_PLAYER_WRITE,
		HOSTILE_PERSISTENCE_SECTOR_READ,
		HOSTILE_PERSISTENCE_SECTOR_WRITE,
		HOSTILE_PERSISTENCE_BLANK,
		HOSTILE_PERSISTENCE_PLAYER_READ_TWO,
		HOSTILE_PERSISTENCE_NEWS,
	};
	static const enum hostile_persistence_event fatal_events[] = {
		HOSTILE_PERSISTENCE_PLAYER_READ_ONE,
		HOSTILE_PERSISTENCE_PLAYER_WRITE,
		HOSTILE_PERSISTENCE_SECTOR_READ,
		HOSTILE_PERSISTENCE_SECTOR_WRITE,
		HOSTILE_PERSISTENCE_FATAL,
	};
	static const uint8_t expected_news[] = {
		'A', 0, 'B', ' ', 'd', 'e', 's', 't', 'r', 'o', 'y', 'e', 'd',
		' ', '2', ' ', 'f', 'i', 'g', 'h', 't', 'e', 'r', 's', ' ',
		'b', 'e', 'l', 'o', 'n', 'g', 'i', 'n', 'g', ' ', 't', 'o', ' ',
		'X', 0, 'Y',
	};
	struct hostile_persistence_tape tape;
	struct yt_hostile_attack_persistence_state state;
	struct yt_record expected_player;
	struct yt_record expected_sector;
	struct yt_error error;
	size_t failure;

	hostile_persistence_fixture(&tape, &state);
	expected_player = tape.players[0].record;
	(void)yt_record_set_number(&expected_player, YT_F53, 6.25f);
	(void)yt_record_set_number(&expected_player, YT_F61, 7.5f);
	expected_sector = tape.sector.record;
	(void)yt_record_set_number(&expected_sector, YT_F81, 0.0f);
	(void)yt_record_set_number(&expected_sector, YT_F85, 0.0f);
	if (!yt_hostile_attack_persistence_run(&state,
	    &hostile_persistence_ops, &tape, NULL)
	    || !state.complete
	    || state.route != YT_HOSTILE_ATTACK_PERSISTENCE_NORMAL
	    || !state.player_written || !state.sector_written
	    || !state.post_loss_read || !state.news_written
	    || !state.mercenaries_hurt || state.ship_fighters != 9.25
	    || tape.calls != YT_ARRAY_LEN(expected_events)
	    || memcmp(tape.events, expected_events, sizeof(expected_events)) != 0
	    || memcmp(tape.written_player.record.bytes, expected_player.bytes,
	    sizeof(expected_player.bytes)) != 0
	    || memcmp(tape.written_sector.record.bytes, expected_sector.bytes,
	    sizeof(expected_sector.bytes)) != 0
	    || tape.news_length != sizeof(expected_news)
	    || memcmp(tape.news, expected_news, sizeof(expected_news)) != 0)
		return false;

	for (failure = 0U; failure < YT_ARRAY_LEN(expected_events); ++failure) {
		hostile_persistence_fixture(&tape, &state);
		tape.fail_at = failure;
		yt_error_clear(&error);
		if (yt_hostile_attack_persistence_run(&state,
		    &hostile_persistence_ops, &tape, &error)
		    || error.status != YT_IO_ERROR || state.complete
		    || tape.calls != failure + 1U
		    || memcmp(tape.events, expected_events,
		    (failure + 1U) * sizeof(expected_events[0])) != 0)
			return false;
	}

	hostile_persistence_fixture(&tape, &state);
	state.ship_fighters = 0.0;
	state.shields = 0.0f;
	state.defender_loss = 0.0;
	if (!yt_hostile_attack_persistence_run(&state,
	    &hostile_persistence_ops, &tape, NULL)
	    || !state.complete
	    || state.route != YT_HOSTILE_ATTACK_PERSISTENCE_FATAL
	    || tape.calls != YT_ARRAY_LEN(fatal_events)
	    || memcmp(tape.events, fatal_events, sizeof(fatal_events)) != 0
	    || state.post_loss_read || state.news_written)
		return false;

	hostile_persistence_fixture(&tape, &state);
	state.defender_loss = 0.0;
	if (!yt_hostile_attack_persistence_run(&state,
	    &hostile_persistence_ops, &tape, NULL)
	    || tape.calls != 5U || state.post_loss_read || state.news_written
	    || state.mercenaries_hurt || state.ship_fighters != 7.5)
		return false;

	hostile_persistence_fixture(&tape, &state);
	return !yt_hostile_attack_persistence_run(NULL,
	    &hostile_persistence_ops, &tape, NULL)
	    && !yt_hostile_attack_persistence_run(&state, NULL, &tape, NULL);
}

enum hostile_tail_event {
	HOSTILE_TAIL_PLAYER_READ,
	HOSTILE_TAIL_PLAYER_WRITE,
	HOSTILE_TAIL_REWARD,
	HOSTILE_TAIL_NEWS,
	HOSTILE_TAIL_CLEARANCE,
	HOSTILE_TAIL_RANDOM,
	HOSTILE_TAIL_DEFEATED,
	HOSTILE_TAIL_VICTORY,
};

struct hostile_tail_tape {
	struct yt_player player;
	struct yt_player written_player;
	enum hostile_tail_event events[12];
	size_t calls;
	size_t fail_at;
	float draw;
	uint8_t rows[2][384];
	size_t row_lengths[2];
	uint8_t news[384];
	size_t news_length;
};

static bool
hostile_tail_event(struct hostile_tail_tape *tape,
    enum hostile_tail_event event, struct yt_error *error)
{
	size_t call = tape->calls++;

	if (call < YT_ARRAY_LEN(tape->events))
		tape->events[call] = event;
	if (call != tape->fail_at)
		return true;
	if (error != NULL) {
		error->status = YT_IO_ERROR;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    "hostile tail injected failure");
	}
	return false;
}

static bool
hostile_tail_read(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct hostile_tail_tape *tape = context;

	if (player_record != 2
	    || !hostile_tail_event(tape, HOSTILE_TAIL_PLAYER_READ, error))
		return false;
	*player = tape->player;
	return true;
}

static bool
hostile_tail_write(void *context, int player_record,
    const struct yt_player *player, struct yt_error *error)
{
	struct hostile_tail_tape *tape = context;

	if (player_record != 2
	    || !hostile_tail_event(tape, HOSTILE_TAIL_PLAYER_WRITE, error))
		return false;
	tape->written_player = *player;
	return true;
}

static bool
hostile_tail_present(void *context, const uint8_t *text, size_t length,
    enum yt_hostile_attack_tail_output_kind kind, struct yt_error *error)
{
	static const enum hostile_tail_event events[] = {
		HOSTILE_TAIL_REWARD,
		HOSTILE_TAIL_DEFEATED,
	};
	struct hostile_tail_tape *tape = context;

	if ((size_t)kind >= YT_ARRAY_LEN(events)
	    || length > sizeof(tape->rows[0])
	    || !hostile_tail_event(tape, events[kind], error))
		return false;
	if (length != 0U)
		memcpy(tape->rows[kind], text, length);
	tape->row_lengths[kind] = length;
	return true;
}

static bool
hostile_tail_news(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	struct hostile_tail_tape *tape = context;

	if (length > sizeof(tape->news)
	    || !hostile_tail_event(tape, HOSTILE_TAIL_NEWS, error))
		return false;
	if (length != 0U)
		memcpy(tape->news, text, length);
	tape->news_length = length;
	return true;
}

static bool
hostile_tail_clearance(void *context, struct yt_error *error)
{
	return hostile_tail_event(context, HOSTILE_TAIL_CLEARANCE, error);
}

static bool
hostile_tail_random(void *context, float *value, struct yt_error *error)
{
	struct hostile_tail_tape *tape = context;

	if (!hostile_tail_event(tape, HOSTILE_TAIL_RANDOM, error))
		return false;
	*value = tape->draw;
	return true;
}

static bool
hostile_tail_victory(void *context, struct yt_error *error)
{
	return hostile_tail_event(context, HOSTILE_TAIL_VICTORY, error);
}

static const struct yt_hostile_attack_tail_ops hostile_tail_ops = {
	hostile_tail_read,
	hostile_tail_write,
	hostile_tail_present,
	hostile_tail_news,
	hostile_tail_clearance,
	hostile_tail_random,
	hostile_tail_victory,
};

static void
hostile_tail_fixture(struct hostile_tail_tape *tape,
    struct yt_hostile_attack_tail_state *state)
{
	static const uint8_t cached_name[] = {'A', 0, 'B'};

	memset(tape, 0, sizeof(*tape));
	tape->fail_at = (size_t)-1;
	tape->draw = 0.75f;
	memset(tape->player.record.bytes, 0xa5,
	    sizeof(tape->player.record.bytes));
	tape->player.fighters = 21.0f;
	tape->player.turns = 98.0f;
	tape->player.sector = 7.0f;
	*state = (struct yt_hostile_attack_tail_state){
		.current_player_record = 2,
		.old_owner = -1.0f,
		.defender_loss = 512000.0,
		.deployed_fighters = 0.0,
		.ship_fighters = 19.0,
		.turns_per_day = 100.0f,
		.headquarters = 7.0f,
		.cached_player_name = cached_name,
		.cached_player_name_length = sizeof(cached_name),
		.current = tape->player,
	};
}

static bool
check_hostile_attack_tail_transaction(void)
{
	static const enum hostile_tail_event expected_events[] = {
		HOSTILE_TAIL_PLAYER_READ,
		HOSTILE_TAIL_PLAYER_WRITE,
		HOSTILE_TAIL_REWARD,
		HOSTILE_TAIL_NEWS,
		HOSTILE_TAIL_CLEARANCE,
		HOSTILE_TAIL_RANDOM,
		HOSTILE_TAIL_DEFEATED,
		HOSTILE_TAIL_VICTORY,
	};
	static const uint8_t expected_reward[] =
	    "Collect 2 turns bonus for destroying 512000 Xannor!!";
	static const uint8_t expected_news[] = {
		'A', 0, 'B', ' ', 'c', 'o', 'l', 'l', 'e', 'c', 't', 'e', 'd',
		' ', '2', ' ', 't', 'u', 'r', 'n', 's', ' ', 'b', 'o', 'n', 'u',
		's', ' ', 'f', 'o', 'r', ' ', 'd', 'e', 's', 't', 'r', 'o', 'y',
		'i', 'n', 'g', ' ', '5', '1', '2', '0', '0', '0', ' ', 'X', 'a',
		'n', 'n', 'o', 'r', '!', '!'
	};
	static const uint8_t expected_defeated[] =
	    "You defeated all the fighters and have 21 left.";
	struct hostile_tail_tape tape;
	struct yt_hostile_attack_tail_state state;
	struct yt_record expected_player;
	struct yt_error error;
	size_t failure;

	hostile_tail_fixture(&tape, &state);
	expected_player = tape.player.record;
	(void)yt_record_set_number(&expected_player, YT_F49, 100.0f);
	if (!yt_hostile_attack_tail_run(&state, &hostile_tail_ops, &tape, NULL)
	    || !state.complete || !state.player_read || !state.player_written
	    || !state.reward_presented || !state.reward_news_written
	    || !state.clearance_called || !state.draw_consumed
	    || !state.defeated_presented || !state.victory_called
	    || state.bonus != 2.0f || state.dominated_draw != 0.75f
	    || state.ship_fighters != 21.0 || state.current.turns != 100.0f
	    || tape.calls != YT_ARRAY_LEN(expected_events)
	    || memcmp(tape.events, expected_events, sizeof(expected_events)) != 0
	    || memcmp(tape.written_player.record.bytes, expected_player.bytes,
	    sizeof(expected_player.bytes)) != 0
	    || tape.row_lengths[YT_HOSTILE_ATTACK_TAIL_REWARD_ROW]
	    != sizeof(expected_reward) - 1U
	    || memcmp(tape.rows[YT_HOSTILE_ATTACK_TAIL_REWARD_ROW],
	    expected_reward, sizeof(expected_reward) - 1U) != 0
	    || tape.news_length != sizeof(expected_news)
	    || memcmp(tape.news, expected_news, sizeof(expected_news)) != 0
	    || tape.row_lengths[YT_HOSTILE_ATTACK_TAIL_DEFEATED_ROW]
	    != sizeof(expected_defeated) - 1U
	    || memcmp(tape.rows[YT_HOSTILE_ATTACK_TAIL_DEFEATED_ROW],
	    expected_defeated, sizeof(expected_defeated) - 1U) != 0)
		return false;

	for (failure = 0U; failure < YT_ARRAY_LEN(expected_events); ++failure) {
		hostile_tail_fixture(&tape, &state);
		tape.fail_at = failure;
		yt_error_clear(&error);
		if (yt_hostile_attack_tail_run(&state, &hostile_tail_ops,
		    &tape, &error) || error.status != YT_IO_ERROR
		    || state.complete || tape.calls != failure + 1U
		    || memcmp(tape.events, expected_events,
		    (failure + 1U) * sizeof(expected_events[0])) != 0)
			return false;
	}

	hostile_tail_fixture(&tape, &state);
	state.deployed_fighters = 1.0;
	if (!yt_hostile_attack_tail_run(&state, &hostile_tail_ops, &tape, NULL)
	    || tape.calls != 5U || state.clearance_called
	    || state.defeated_presented || state.victory_called
	    || tape.events[4] != HOSTILE_TAIL_RANDOM)
		return false;

	hostile_tail_fixture(&tape, &state);
	state.defender_loss = 255999.0;
	if (!yt_hostile_attack_tail_run(&state, &hostile_tail_ops, &tape, NULL)
	    || tape.calls != 4U || state.bonus != 0.0f
	    || state.player_written || state.reward_presented
	    || tape.events[0] != HOSTILE_TAIL_PLAYER_READ
	    || tape.events[1] != HOSTILE_TAIL_RANDOM
	    || tape.events[2] != HOSTILE_TAIL_DEFEATED
	    || tape.events[3] != HOSTILE_TAIL_VICTORY)
		return false;

	hostile_tail_fixture(&tape, &state);
	state.old_owner = -2.0f;
	state.deployed_fighters = 1.0;
	if (!yt_hostile_attack_tail_run(&state, &hostile_tail_ops, &tape, NULL)
	    || tape.calls != 1U || tape.events[0] != HOSTILE_TAIL_RANDOM
	    || state.player_read || state.defeated_presented)
		return false;

	hostile_tail_fixture(&tape, &state);
	return !yt_hostile_attack_tail_run(NULL, &hostile_tail_ops, &tape, NULL)
	    && !yt_hostile_attack_tail_run(&state, NULL, &tape, NULL);
}

enum hostile_combat_event {
	HOSTILE_COMBAT_READ_SECTOR,
	HOSTILE_COMBAT_READ_PLAYER,
	HOSTILE_COMBAT_SOUND,
	HOSTILE_COMBAT_RANDOM,
	HOSTILE_COMBAT_SURRENDER,
	HOSTILE_COMBAT_RESULT_BLANK,
	HOSTILE_COMBAT_LOSS_ROW,
	HOSTILE_COMBAT_DESTROYED_ROW,
	HOSTILE_COMBAT_EXPOSED_ROW,
	HOSTILE_COMBAT_SPILL_BLANK,
	HOSTILE_COMBAT_SPILL,
	HOSTILE_COMBAT_PERSISTENCE,
	HOSTILE_COMBAT_TAIL,
};

struct hostile_combat_tape {
	struct yt_player player;
	struct yt_sector opened_sector;
	float draws[8];
	size_t draw_count;
	size_t draw_index;
	enum hostile_combat_event events[32];
	size_t calls;
	size_t fail_at;
	bool surrender_accept;
	bool surrender_fail_after;
	bool spill_fail_after;
	bool persistence_fail_after;
	bool persistence_fatal;
	bool tail_fail_after;
	bool real_children;
	double persistence_ship_output;
	struct hostile_surrender_tape surrender_tape;
	struct hostile_persistence_tape persistence_tape;
	struct hostile_tail_tape tail_tape;
	struct yt_player cached_player;
	struct yt_sector cached_sector;
	size_t player_cache_calls;
	size_t sector_cache_calls;
	struct yt_hostile_attack_persistence_state persistence_input;
	struct yt_hostile_attack_tail_state tail_input;
	uint8_t rows[5][192];
	size_t row_lengths[5];
};

static bool
hostile_combat_event(struct hostile_combat_tape *tape,
    enum hostile_combat_event event, struct yt_error *error)
{
	size_t call = tape->calls++;

	if (call < YT_ARRAY_LEN(tape->events))
		tape->events[call] = event;
	if (call != tape->fail_at)
		return true;
	if (error != NULL) {
		error->status = YT_IO_ERROR;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    "hostile combat injected failure");
	}
	return false;
}

static bool
hostile_combat_fail_after(struct yt_error *error)
{
	if (error != NULL) {
		error->status = YT_IO_ERROR;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    "hostile combat child residue failure");
	}
	return false;
}

static bool
hostile_combat_read_sector(void *context, int sector_number,
    struct yt_sector *sector, struct yt_error *error)
{
	struct hostile_combat_tape *tape = context;

	if (sector_number != 733 || !hostile_combat_event(tape,
	    HOSTILE_COMBAT_READ_SECTOR, error))
		return false;
	*sector = tape->opened_sector;
	return true;
}

static bool
hostile_combat_read_player(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct hostile_combat_tape *tape = context;

	if (player_record != 2 || !hostile_combat_event(tape,
	    HOSTILE_COMBAT_READ_PLAYER, error))
		return false;
	*player = tape->player;
	return true;
}

static bool
hostile_combat_sound(void *context, float selector,
    struct yt_error *error)
{
	return selector == 2.0f && hostile_combat_event(context,
	    HOSTILE_COMBAT_SOUND, error);
}

static bool
hostile_combat_random(void *context, float *value,
    struct yt_error *error)
{
	struct hostile_combat_tape *tape = context;

	if (!hostile_combat_event(tape, HOSTILE_COMBAT_RANDOM, error)
	    || tape->draw_index >= tape->draw_count)
		return false;
	*value = tape->draws[tape->draw_index++];
	return true;
}

static bool
hostile_combat_surrender(void *context,
    struct yt_hostile_surrender_state *state, struct yt_error *error)
{
	struct hostile_combat_tape *tape = context;

	if (!hostile_combat_event(tape, HOSTILE_COMBAT_SURRENDER, error))
		return false;
	if (tape->real_children)
		return yt_hostile_attack_surrender_run(state,
		    &hostile_surrender_ops, &tape->surrender_tape, error);
	state->checked = true;
	state->accepted = tape->surrender_accept;
	state->complete = !tape->surrender_fail_after;
	state->ship_fighters = (double)state->current.fighters;
	state->deployed_remaining = state->deployed_fighters;
	state->fighter_owner = state->old_owner;
	if (state->accepted) {
		state->surrendered_fighters = state->deployed_fighters
		    - state->defender_loss;
		state->ship_fighters = (double)state->current.fighters
		    - state->attacker_loss - state->defender_loss
		    + state->deployed_fighters;
		state->current.fighters = (float)state->ship_fighters;
		state->deployed_remaining = 0.0;
		state->fighter_owner = 0.0f;
	}
	if (tape->surrender_fail_after)
		return hostile_combat_fail_after(error);
	return true;
}

static bool
hostile_combat_present(void *context, const uint8_t *text, size_t length,
    enum yt_hostile_attack_combat_output_kind kind,
    struct yt_error *error)
{
	static const enum hostile_combat_event events[] = {
		HOSTILE_COMBAT_RESULT_BLANK,
		HOSTILE_COMBAT_LOSS_ROW,
		HOSTILE_COMBAT_DESTROYED_ROW,
		HOSTILE_COMBAT_EXPOSED_ROW,
		HOSTILE_COMBAT_SPILL_BLANK,
	};
	struct hostile_combat_tape *tape = context;

	if ((size_t)kind >= YT_ARRAY_LEN(events)
	    || length > sizeof(tape->rows[0])
	    || !hostile_combat_event(tape, events[kind], error))
		return false;
	if (length != 0U)
		memcpy(tape->rows[kind], text, length);
	tape->row_lengths[kind] = length;
	return true;
}

static void
hostile_combat_cache_player(void *context, const struct yt_player *player)
{
	struct hostile_combat_tape *tape = context;

	tape->cached_player = *player;
	++tape->player_cache_calls;
}

static void
hostile_combat_cache_sector(void *context, const struct yt_sector *sector)
{
	struct hostile_combat_tape *tape = context;

	tape->cached_sector = *sector;
	++tape->sector_cache_calls;
}

static bool
hostile_combat_spill(void *context, double *fighters, float *shields,
    struct yt_error *error)
{
	struct hostile_combat_tape *tape = context;

	if (!hostile_combat_event(tape, HOSTILE_COMBAT_SPILL, error))
		return false;
	*fighters -= 1.0;
	*shields = 0.0f;
	if (tape->spill_fail_after)
		return hostile_combat_fail_after(error);
	return true;
}

static bool
hostile_combat_persistence(void *context,
    struct yt_hostile_attack_persistence_state *state,
    struct yt_error *error)
{
	struct hostile_combat_tape *tape = context;

	if (!hostile_combat_event(tape, HOSTILE_COMBAT_PERSISTENCE, error))
		return false;
	tape->persistence_input = *state;
	if (tape->real_children)
		return yt_hostile_attack_persistence_run(state,
		    &hostile_persistence_ops, &tape->persistence_tape, error);
	state->player_written = true;
	state->sector_written = true;
	state->sector.fighters = (float)state->deployed_fighters;
	state->ship_fighters = tape->persistence_ship_output;
	state->current.fighters = (float)state->ship_fighters;
	state->route = tape->persistence_fatal
	    ? YT_HOSTILE_ATTACK_PERSISTENCE_FATAL
	    : YT_HOSTILE_ATTACK_PERSISTENCE_NORMAL;
	state->complete = !tape->persistence_fail_after;
	if (tape->persistence_fail_after)
		return hostile_combat_fail_after(error);
	return true;
}

static bool
hostile_combat_tail(void *context,
    struct yt_hostile_attack_tail_state *state, struct yt_error *error)
{
	struct hostile_combat_tape *tape = context;

	if (!hostile_combat_event(tape, HOSTILE_COMBAT_TAIL, error))
		return false;
	tape->tail_input = *state;
	if (tape->real_children)
		return yt_hostile_attack_tail_run(state, &hostile_tail_ops,
		    &tape->tail_tape, error);
	state->complete = !tape->tail_fail_after;
	if (tape->tail_fail_after)
		return hostile_combat_fail_after(error);
	return true;
}

static const struct yt_hostile_attack_combat_ops hostile_combat_ops = {
	hostile_combat_read_sector,
	hostile_combat_read_player,
	hostile_combat_sound,
	hostile_combat_random,
	hostile_combat_surrender,
	hostile_combat_present,
	hostile_combat_cache_player,
	hostile_combat_cache_sector,
	hostile_combat_spill,
	hostile_combat_persistence,
	hostile_combat_tail,
};

static void
hostile_combat_fixture(struct hostile_combat_tape *tape,
    struct yt_hostile_attack_combat_state *state)
{
	static const uint8_t cached_name[] = {'A', 0, 'B'};
	static const uint8_t owner_label[] = {'X', 0, 'Y'};
	struct yt_hostile_surrender_state surrender;
	struct yt_hostile_attack_persistence_state persistence;
	struct yt_hostile_attack_tail_state tail;

	memset(tape, 0, sizeof(*tape));
	hostile_surrender_fixture(&tape->surrender_tape, &surrender, 2.0f,
	    YT_HOSTILE_SURRENDER_ANSWER_YES);
	hostile_persistence_fixture(&tape->persistence_tape, &persistence);
	hostile_tail_fixture(&tape->tail_tape, &tail);
	tape->fail_at = (size_t)-1;
	tape->persistence_ship_output = 5.5;
	tape->draws[0] = 0.0f;
	tape->draws[1] = 1.0f;
	tape->draws[2] = 1.0f;
	tape->draw_count = 3U;
	memset(tape->player.record.bytes, 0xa5,
	    sizeof(tape->player.record.bytes));
	tape->player.fighters = 3.0f;
	tape->player.shields = 5.0f;
	tape->player.cloak = 0.0f;
	tape->player.sector = 733.0f;
	memset(tape->opened_sector.record.bytes, 0x3c,
	    sizeof(tape->opened_sector.record.bytes));
	tape->opened_sector.fighter_owner = 3.0f;
	*state = (struct yt_hostile_attack_combat_state){
		.current_player_record = 2,
		.current_sector = 733,
		.commitment = 3.0,
		.allow_surrender = true,
		.sector = {
			.fighters = 2.0f,
			.fighter_owner = 3.0f,
		},
		.cached_player_name = cached_name,
		.cached_player_name_length = sizeof(cached_name),
		.real_first_name = (const uint8_t *)"Sysop",
		.real_first_name_length = 5U,
		.owner_label = owner_label,
		.owner_label_length = sizeof(owner_label),
		.turns_per_day = 100.0f,
		.headquarters = 7.0f,
	};
}

static bool
check_hostile_attack_combat_transaction(void)
{
	static const enum hostile_combat_event ordinary_events[] = {
		HOSTILE_COMBAT_READ_SECTOR,
		HOSTILE_COMBAT_READ_PLAYER,
		HOSTILE_COMBAT_SOUND,
		HOSTILE_COMBAT_RANDOM,
		HOSTILE_COMBAT_RANDOM,
		HOSTILE_COMBAT_RANDOM,
		HOSTILE_COMBAT_RESULT_BLANK,
		HOSTILE_COMBAT_LOSS_ROW,
		HOSTILE_COMBAT_DESTROYED_ROW,
		HOSTILE_COMBAT_PERSISTENCE,
		HOSTILE_COMBAT_TAIL,
	};
	static const enum hostile_combat_event surrender_events[] = {
		HOSTILE_COMBAT_READ_SECTOR,
		HOSTILE_COMBAT_READ_PLAYER,
		HOSTILE_COMBAT_SOUND,
		HOSTILE_COMBAT_SURRENDER,
		HOSTILE_COMBAT_RESULT_BLANK,
		HOSTILE_COMBAT_LOSS_ROW,
		HOSTILE_COMBAT_DESTROYED_ROW,
		HOSTILE_COMBAT_PERSISTENCE,
		HOSTILE_COMBAT_TAIL,
	};
	static const uint8_t lost_one[] = " You lost 1 fighter(s)";
	static const uint8_t destroyed_two[] =
	    " You destroyed 2 enemy fighters.";
	struct hostile_combat_tape tape;
	struct yt_hostile_attack_combat_state state;
	struct yt_record joined_player;
	struct yt_record joined_sector;
	struct yt_error error;
	size_t failure;

	hostile_combat_fixture(&tape, &state);
	if (!yt_hostile_attack_combat_run(&state, &hostile_combat_ops,
	    &tape, NULL) || !state.complete
	    || state.route != YT_HOSTILE_ATTACK_COMBAT_NORMAL
	    || state.attacker_loss != 1.0 || state.defender_loss != 2.0
	    || state.ship_fighters != 5.5 || state.deployed_remaining != 0.0
	    || state.iterations != 3U || tape.draw_index != 3U
	    || state.surrender_checked || state.surrendered
	    || tape.calls != YT_ARRAY_LEN(ordinary_events)
	    || memcmp(tape.events, ordinary_events, sizeof(ordinary_events)) != 0
	    || tape.row_lengths[YT_HOSTILE_ATTACK_COMBAT_LOSS_ROW]
	    != sizeof(lost_one) - 1U
	    || memcmp(tape.rows[YT_HOSTILE_ATTACK_COMBAT_LOSS_ROW], lost_one,
	    sizeof(lost_one) - 1U) != 0
	    || tape.row_lengths[YT_HOSTILE_ATTACK_COMBAT_DESTROYED_ROW]
	    != sizeof(destroyed_two) - 1U
	    || memcmp(tape.rows[YT_HOSTILE_ATTACK_COMBAT_DESTROYED_ROW],
	    destroyed_two, sizeof(destroyed_two) - 1U) != 0
	    || tape.persistence_input.ship_fighters != 2.0
	    || tape.persistence_input.deployed_fighters != 0.0
	    || tape.persistence_input.defender_loss != 2.0
	    || tape.tail_input.ship_fighters != 5.5
	    || tape.player_cache_calls != 1U || tape.sector_cache_calls != 2U)
		return false;

	for (failure = 0U; failure < YT_ARRAY_LEN(ordinary_events); ++failure) {
		hostile_combat_fixture(&tape, &state);
		tape.fail_at = failure;
		yt_error_clear(&error);
		if (yt_hostile_attack_combat_run(&state, &hostile_combat_ops,
		    &tape, &error) || error.status != YT_IO_ERROR
		    || state.complete || tape.calls != failure + 1U
		    || memcmp(tape.events, ordinary_events,
		    (failure + 1U) * sizeof(ordinary_events[0])) != 0)
			return false;
	}

	hostile_combat_fixture(&tape, &state);
	tape.player.fighters = 11.0f;
	tape.opened_sector.fighter_owner = 2.0f;
	tape.surrender_accept = true;
	state.commitment = 120.0;
	state.sector.fighters = 10.0f;
	if (!yt_hostile_attack_combat_run(&state, &hostile_combat_ops,
	    &tape, NULL) || !state.surrendered || !state.surrender_checked
	    || state.iterations != 0U || tape.draw_index != 0U
	    || tape.calls != YT_ARRAY_LEN(surrender_events)
	    || memcmp(tape.events, surrender_events,
	    sizeof(surrender_events)) != 0
	    || tape.persistence_input.ship_fighters != 21.0
	    || tape.persistence_input.deployed_fighters != 0.0
	    || tape.persistence_input.old_owner != 2.0f
	    || state.sector.fighter_owner != 0.0f)
		return false;

	/* Join the real surrender, raw persistence and tail transactions. */
	hostile_combat_fixture(&tape, &state);
	tape.real_children = true;
	tape.player.fighters = 11.0f;
	tape.opened_sector.fighter_owner = 2.0f;
	tape.persistence_tape.players[0].fighters = 90.0f;
	tape.persistence_tape.players[0].shields = 80.0f;
	joined_player = tape.persistence_tape.players[0].record;
	(void)yt_record_set_number(&joined_player, YT_F53, 0.0f);
	(void)yt_record_set_number(&joined_player, YT_F61, 21.0f);
	joined_sector = tape.persistence_tape.sector.record;
	(void)yt_record_set_number(&joined_sector, YT_F81, 0.0f);
	(void)yt_record_set_number(&joined_sector, YT_F85, 0.0f);
	state.commitment = 120.0;
	state.sector.fighters = 10.0f;
	if (!yt_hostile_attack_combat_run(&state, &hostile_combat_ops,
	    &tape, NULL) || !state.complete || !state.surrendered
	    || state.route != YT_HOSTILE_ATTACK_COMBAT_NORMAL
	    || state.iterations != 0U || tape.draw_index != 0U
	    || state.ship_fighters != 21.0 || state.deployed_remaining != 0.0
	    || state.sector.fighter_owner != 0.0f
	    || tape.surrender_tape.calls != 11U
	    || tape.persistence_tape.calls != 5U
	    || tape.tail_tape.calls != 2U
	    || tape.tail_tape.events[0] != HOSTILE_TAIL_RANDOM
	    || tape.tail_tape.events[1] != HOSTILE_TAIL_DEFEATED
	    || memcmp(tape.persistence_tape.written_player.record.bytes,
	    joined_player.bytes, sizeof(joined_player.bytes)) != 0
	    || memcmp(tape.persistence_tape.written_sector.record.bytes,
	    joined_sector.bytes, sizeof(joined_sector.bytes)) != 0
	    || tape.surrender_tape.news_length == 0U
	    || tape.persistence_tape.news_length != 0U
	    || tape.tail_tape.news_length != 0U)
		return false;

	hostile_combat_fixture(&tape, &state);
	tape.player.fighters = 1.0f;
	tape.player.shields = 5.0f;
	tape.draws[0] = 0.0f;
	tape.draw_count = 1U;
	tape.persistence_fatal = true;
	state.commitment = 1.0;
	state.sector.fighters = 2.0f;
	if (!yt_hostile_attack_combat_run(&state, &hostile_combat_ops,
	    &tape, NULL) || !state.complete
	    || state.route != YT_HOSTILE_ATTACK_COMBAT_FATAL
	    || !state.spill_called || state.current.shields != 0.0f
	    || state.deployed_remaining != 1.0
	    || tape.events[tape.calls - 1U] != HOSTILE_COMBAT_PERSISTENCE)
		return false;

	hostile_combat_fixture(&tape, &state);
	tape.player.fighters = 11.0f;
	tape.opened_sector.fighter_owner = 2.0f;
	tape.surrender_accept = true;
	tape.surrender_fail_after = true;
	state.commitment = 120.0;
	state.sector.fighters = 10.0f;
	yt_error_clear(&error);
	if (yt_hostile_attack_combat_run(&state, &hostile_combat_ops,
	    &tape, &error) || error.status != YT_IO_ERROR
	    || !state.surrender_checked || !state.surrendered
	    || state.complete || tape.player_cache_calls != 1U)
		return false;

	hostile_combat_fixture(&tape, &state);
	tape.player.fighters = 1.0f;
	tape.draws[0] = 0.0f;
	tape.draw_count = 1U;
	tape.spill_fail_after = true;
	state.commitment = 1.0;
	state.sector.fighters = 2.0f;
	yt_error_clear(&error);
	if (yt_hostile_attack_combat_run(&state, &hostile_combat_ops,
	    &tape, &error) || error.status != YT_IO_ERROR
	    || state.complete || state.current.shields != 0.0f
	    || state.deployed_remaining != 1.0)
		return false;

	hostile_combat_fixture(&tape, &state);
	tape.persistence_fail_after = true;
	yt_error_clear(&error);
	if (yt_hostile_attack_combat_run(&state, &hostile_combat_ops,
	    &tape, &error) || error.status != YT_IO_ERROR || state.complete
	    || !state.persistence.player_written
	    || !state.persistence.sector_written
	    || state.sector.fighters != 0.0f
	    || tape.sector_cache_calls != 2U)
		return false;

	hostile_combat_fixture(&tape, &state);
	tape.tail_fail_after = true;
	yt_error_clear(&error);
	if (yt_hostile_attack_combat_run(&state, &hostile_combat_ops,
	    &tape, &error) || error.status != YT_IO_ERROR || state.complete
	    || state.tail.complete
	    || tape.events[tape.calls - 1U] != HOSTILE_COMBAT_TAIL)
		return false;

	hostile_combat_fixture(&tape, &state);
	return !yt_hostile_attack_combat_run(NULL, &hostile_combat_ops,
	    &tape, NULL)
	    && !yt_hostile_attack_combat_run(&state, NULL, &tape, NULL);
}

enum hostile_bribe_accept_event {
	HOSTILE_BRIBE_ACCEPT_DEAL,
	HOSTILE_BRIBE_ACCEPT_SOUND,
	HOSTILE_BRIBE_ACCEPT_SECTOR_READ,
	HOSTILE_BRIBE_ACCEPT_SECTOR_WRITE,
	HOSTILE_BRIBE_ACCEPT_PLAYER_READ,
	HOSTILE_BRIBE_ACCEPT_PLAYER_WRITE,
};

struct hostile_bribe_accept_tape {
	struct yt_sector sector;
	struct yt_player player;
	struct yt_sector written_sector;
	struct yt_player written_player;
	enum hostile_bribe_accept_event events[8];
	size_t calls;
	size_t fail_at;
	uint8_t deal[64];
	size_t deal_length;
};

static bool
hostile_bribe_accept_event(struct hostile_bribe_accept_tape *tape,
    enum hostile_bribe_accept_event event, struct yt_error *error)
{
	size_t call = tape->calls++;

	if (call < YT_ARRAY_LEN(tape->events))
		tape->events[call] = event;
	if (call != tape->fail_at)
		return true;
	if (error != NULL) {
		error->status = YT_IO_ERROR;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    "hostile Bribe acceptance injected failure");
	}
	return false;
}

static bool
hostile_bribe_accept_present(void *context, const uint8_t *text,
    size_t length, struct yt_error *error)
{
	struct hostile_bribe_accept_tape *tape = context;

	if (length > sizeof(tape->deal) || !hostile_bribe_accept_event(tape,
	    HOSTILE_BRIBE_ACCEPT_DEAL, error))
		return false;
	memcpy(tape->deal, text, length);
	tape->deal_length = length;
	return true;
}

static bool
hostile_bribe_accept_sound(void *context, float selector,
    struct yt_error *error)
{
	return selector == 1.0f && hostile_bribe_accept_event(context,
	    HOSTILE_BRIBE_ACCEPT_SOUND, error);
}

static bool
hostile_bribe_accept_read_sector(void *context, int sector_number,
    struct yt_sector *sector, struct yt_error *error)
{
	struct hostile_bribe_accept_tape *tape = context;

	if (sector_number != 733 || !hostile_bribe_accept_event(tape,
	    HOSTILE_BRIBE_ACCEPT_SECTOR_READ, error))
		return false;
	*sector = tape->sector;
	return true;
}

static bool
hostile_bribe_accept_write_sector(void *context, int sector_number,
    const struct yt_sector *sector, struct yt_error *error)
{
	struct hostile_bribe_accept_tape *tape = context;

	if (sector_number != 733 || !hostile_bribe_accept_event(tape,
	    HOSTILE_BRIBE_ACCEPT_SECTOR_WRITE, error))
		return false;
	tape->written_sector = *sector;
	return true;
}

static bool
hostile_bribe_accept_read_player(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct hostile_bribe_accept_tape *tape = context;

	if (player_record != 2 || !hostile_bribe_accept_event(tape,
	    HOSTILE_BRIBE_ACCEPT_PLAYER_READ, error))
		return false;
	*player = tape->player;
	return true;
}

static bool
hostile_bribe_accept_write_player(void *context, int player_record,
    const struct yt_player *player, struct yt_error *error)
{
	struct hostile_bribe_accept_tape *tape = context;

	if (player_record != 2 || !hostile_bribe_accept_event(tape,
	    HOSTILE_BRIBE_ACCEPT_PLAYER_WRITE, error))
		return false;
	tape->written_player = *player;
	return true;
}

static const struct yt_hostile_bribe_accept_ops hostile_bribe_accept_ops = {
	hostile_bribe_accept_present,
	hostile_bribe_accept_sound,
	hostile_bribe_accept_read_sector,
	hostile_bribe_accept_write_sector,
	hostile_bribe_accept_read_player,
	hostile_bribe_accept_write_player,
};

static void
hostile_bribe_accept_fixture(struct hostile_bribe_accept_tape *tape,
    struct yt_hostile_bribe_accept_state *state)
{
	memset(tape, 0, sizeof(*tape));
	tape->fail_at = (size_t)-1;
	memset(tape->sector.record.bytes, 0x5a,
	    sizeof(tape->sector.record.bytes));
	tape->sector.fighters = 99.0f;
	tape->sector.fighter_owner = -2.0f;
	memset(tape->player.record.bytes, 0xa5,
	    sizeof(tape->player.record.bytes));
	tape->player.fighters = 7.25f;
	tape->player.credits = 100.5f;
	*state = (struct yt_hostile_bribe_accept_state){
		.current_player_record = 2,
		.current_sector = 733,
		.cached_defenders = 10.5f,
		.offer = 30.25f,
	};
}

static bool
check_hostile_bribe_accept_transaction(void)
{
	static const enum hostile_bribe_accept_event expected_events[] = {
		HOSTILE_BRIBE_ACCEPT_DEAL,
		HOSTILE_BRIBE_ACCEPT_SOUND,
		HOSTILE_BRIBE_ACCEPT_SECTOR_READ,
		HOSTILE_BRIBE_ACCEPT_SECTOR_WRITE,
		HOSTILE_BRIBE_ACCEPT_PLAYER_READ,
		HOSTILE_BRIBE_ACCEPT_PLAYER_WRITE,
	};
	static const uint8_t expected_deal[] =
	    "Good Deal! We join up with you!";
	struct hostile_bribe_accept_tape tape;
	struct yt_hostile_bribe_accept_state state;
	struct yt_record expected_sector;
	struct yt_record expected_player;
	struct yt_error error;
	size_t failure;

	hostile_bribe_accept_fixture(&tape, &state);
	expected_sector = tape.sector.record;
	(void)yt_record_set_number(&expected_sector, YT_F85, 0.0f);
	(void)yt_record_set_number(&expected_sector, YT_F81, 0.0f);
	expected_player = tape.player.record;
	(void)yt_record_set_number(&expected_player, YT_F61, 17.75f);
	(void)yt_record_set_number(&expected_player, YT_F81, 70.25f);
	if (!yt_hostile_bribe_accept_run(&state, &hostile_bribe_accept_ops,
	    &tape, NULL) || !state.complete || !state.deal_presented
	    || !state.sound_played || !state.sector_read
	    || !state.sector_written || !state.player_read
	    || !state.player_written || state.persisted_fighters != 17.75f
	    || state.persisted_credits != 70.25f
	    || tape.calls != YT_ARRAY_LEN(expected_events)
	    || memcmp(tape.events, expected_events, sizeof(expected_events)) != 0
	    || tape.deal_length != sizeof(expected_deal) - 1U
	    || memcmp(tape.deal, expected_deal, sizeof(expected_deal) - 1U) != 0
	    || memcmp(tape.written_sector.record.bytes, expected_sector.bytes,
	    sizeof(expected_sector.bytes)) != 0
	    || memcmp(tape.written_player.record.bytes, expected_player.bytes,
	    sizeof(expected_player.bytes)) != 0)
		return false;

	for (failure = 0U; failure < YT_ARRAY_LEN(expected_events); ++failure) {
		hostile_bribe_accept_fixture(&tape, &state);
		tape.fail_at = failure;
		yt_error_clear(&error);
		if (yt_hostile_bribe_accept_run(&state,
		    &hostile_bribe_accept_ops, &tape, &error)
		    || error.status != YT_IO_ERROR || state.complete
		    || tape.calls != failure + 1U
		    || memcmp(tape.events, expected_events,
		    (failure + 1U) * sizeof(expected_events[0])) != 0)
			return false;
	}

	/* A failed sector PUT retains the dirty FIELD and blocks player I/O. */
	hostile_bribe_accept_fixture(&tape, &state);
	tape.fail_at = 3U;
	yt_error_clear(&error);
	if (yt_hostile_bribe_accept_run(&state, &hostile_bribe_accept_ops,
	    &tape, &error) || error.status != YT_IO_ERROR
	    || !state.sector_read || state.sector_written || state.player_read
	    || memcmp(state.sector.record.bytes, expected_sector.bytes,
	    sizeof(expected_sector.bytes)) != 0)
		return false;

	hostile_bribe_accept_fixture(&tape, &state);
	return !yt_hostile_bribe_accept_run(NULL, &hostile_bribe_accept_ops,
	    &tape, NULL)
	    && !yt_hostile_bribe_accept_run(&state, NULL, &tape, NULL);
}

enum hostile_bribe_event {
	HOSTILE_BRIBE_EVENT_RANDOM,
	HOSTILE_BRIBE_EVENT_AMOUNT,
	HOSTILE_BRIBE_EVENT_ACCEPT,
	HOSTILE_BRIBE_EVENT_COMBAT,
	HOSTILE_BRIBE_EVENT_FATAL,
	HOSTILE_BRIBE_EVENT_ORDINARY,
	HOSTILE_BRIBE_EVENT_PLANET,
	HOSTILE_BRIBE_EVENT_LIFE,
	HOSTILE_BRIBE_EVENT_INTRODUCTION,
	HOSTILE_BRIBE_EVENT_PROMPT,
	HOSTILE_BRIBE_EVENT_REJECTED,
};

struct hostile_bribe_tape {
	float draws[3];
	size_t draw_index;
	const char *response;
	enum hostile_bribe_event events[16];
	size_t calls;
	size_t fail_at;
	uint8_t rows[6][512];
	size_t row_lengths[6];
	double combat_commitment;
};

static bool
hostile_bribe_event(struct hostile_bribe_tape *tape,
    enum hostile_bribe_event event, struct yt_error *error)
{
	size_t call = tape->calls++;

	if (call < YT_ARRAY_LEN(tape->events))
		tape->events[call] = event;
	if (call != tape->fail_at)
		return true;
	if (error != NULL) {
		error->status = YT_IO_ERROR;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    "hostile Bribe injected failure");
	}
	return false;
}

static bool
hostile_bribe_present(void *context, const uint8_t *text, size_t length,
    enum yt_hostile_bribe_output_kind kind, struct yt_error *error)
{
	static const enum hostile_bribe_event events[] = {
		HOSTILE_BRIBE_EVENT_ORDINARY,
		HOSTILE_BRIBE_EVENT_PLANET,
		HOSTILE_BRIBE_EVENT_LIFE,
		HOSTILE_BRIBE_EVENT_INTRODUCTION,
		HOSTILE_BRIBE_EVENT_PROMPT,
		HOSTILE_BRIBE_EVENT_REJECTED,
	};
	struct hostile_bribe_tape *tape = context;

	if ((size_t)kind >= YT_ARRAY_LEN(events)
	    || length > sizeof(tape->rows[0])
	    || !hostile_bribe_event(tape, events[kind], error))
		return false;
	memcpy(tape->rows[kind], text, length);
	tape->row_lengths[kind] = length;
	return true;
}

static bool
hostile_bribe_random(void *context, float *value, struct yt_error *error)
{
	struct hostile_bribe_tape *tape = context;

	if (!hostile_bribe_event(tape, HOSTILE_BRIBE_EVENT_RANDOM, error)
	    || tape->draw_index >= YT_ARRAY_LEN(tape->draws))
		return false;
	*value = tape->draws[tape->draw_index++];
	return true;
}

static bool
hostile_bribe_amount(void *context, char *response, size_t capacity,
    struct yt_error *error)
{
	struct hostile_bribe_tape *tape = context;
	size_t length = strlen(tape->response);

	if (length + 1U > capacity || !hostile_bribe_event(tape,
	    HOSTILE_BRIBE_EVENT_AMOUNT, error))
		return false;
	memcpy(response, tape->response, length + 1U);
	return true;
}

static bool
hostile_bribe_accept_child(void *context,
    struct yt_hostile_bribe_accept_state *state, struct yt_error *error)
{
	if (!hostile_bribe_event(context, HOSTILE_BRIBE_EVENT_ACCEPT, error))
		return false;
	state->complete = true;
	return true;
}

static bool
hostile_bribe_combat_child(void *context, double commitment,
    struct yt_error *error)
{
	struct hostile_bribe_tape *tape = context;

	if (!hostile_bribe_event(tape, HOSTILE_BRIBE_EVENT_COMBAT, error))
		return false;
	tape->combat_commitment = commitment;
	return true;
}

static bool
hostile_bribe_fatal_child(void *context, struct yt_error *error)
{
	return hostile_bribe_event(context, HOSTILE_BRIBE_EVENT_FATAL, error);
}

static const struct yt_hostile_bribe_ops hostile_bribe_ops = {
	hostile_bribe_present,
	hostile_bribe_random,
	hostile_bribe_amount,
	hostile_bribe_accept_child,
	hostile_bribe_combat_child,
	hostile_bribe_fatal_child,
};

static void
hostile_bribe_fixture(struct hostile_bribe_tape *tape,
    struct yt_hostile_bribe_state *state)
{
	static const uint8_t name[] = {'A', 0, 'B'};

	memset(tape, 0, sizeof(*tape));
	tape->fail_at = (size_t)-1;
	tape->draws[0] = 0.9f;
	tape->draws[1] = 0.0f;
	tape->draws[2] = 0.0f;
	tape->response = "30";
	*state = (struct yt_hostile_bribe_state){
		.current_player_record = 2,
		.current_sector = 733,
		.owner = -2.0f,
		.cached_defenders = 10.0f,
		.ship_fighters = 20.0,
		.shields = 5.0f,
		.credits = 100.0,
		.real_first_name = name,
		.real_first_name_length = sizeof(name),
	};
}

static bool
check_hostile_bribe_transaction(void)
{
	static const enum hostile_bribe_event accepted_events[] = {
		HOSTILE_BRIBE_EVENT_RANDOM,
		HOSTILE_BRIBE_EVENT_RANDOM,
		HOSTILE_BRIBE_EVENT_INTRODUCTION,
		HOSTILE_BRIBE_EVENT_PROMPT,
		HOSTILE_BRIBE_EVENT_AMOUNT,
		HOSTILE_BRIBE_EVENT_RANDOM,
		HOSTILE_BRIBE_EVENT_ACCEPT,
	};
	static const uint8_t introduction[] = {
		'W','e',' ','M','A','Y',' ','j','o','i','n',' ','u','p',' ','i','f',
		' ','y','o','u',' ','p','a','y',' ','u','s',' ','e','n','o','u','g',
		'h',' ','A',0,'B','!'
	};
	static const uint8_t prompt[] =
	    "You have 100 credits. How much do you offer? -+>";
	struct hostile_bribe_tape tape;
	struct yt_hostile_bribe_state state;
	struct yt_error error;
	size_t failure;

	hostile_bribe_fixture(&tape, &state);
	if (!yt_hostile_bribe_run(&state, &hostile_bribe_ops, &tape, NULL)
	    || !state.complete || state.branch != YT_HOSTILE_BRIBE_ACCEPTED
	    || state.route != YT_HOSTILE_BRIBE_SCANNER
	    || state.draws_consumed != 3U || tape.draw_index != 3U
	    || !state.mercenaries_hurt_converted
	    || state.mercenaries_hurt_cint != 0
	    || !state.offer_stored || state.offer != 30.0f
	    || state.threshold != 10.0 || !state.accepted_called
	    || state.forced_attack || state.commitment_stored
	    || tape.calls != YT_ARRAY_LEN(accepted_events)
	    || memcmp(tape.events, accepted_events, sizeof(accepted_events)) != 0
	    || tape.row_lengths[YT_HOSTILE_BRIBE_INTRODUCTION_ROW]
	    != sizeof(introduction)
	    || memcmp(tape.rows[YT_HOSTILE_BRIBE_INTRODUCTION_ROW], introduction,
	    sizeof(introduction)) != 0
	    || tape.row_lengths[YT_HOSTILE_BRIBE_OFFER_PROMPT]
	    != sizeof(prompt) - 1U
	    || memcmp(tape.rows[YT_HOSTILE_BRIBE_OFFER_PROMPT], prompt,
	    sizeof(prompt) - 1U) != 0)
		return false;

	for (failure = 0U; failure < YT_ARRAY_LEN(accepted_events); ++failure) {
		hostile_bribe_fixture(&tape, &state);
		tape.fail_at = failure;
		yt_error_clear(&error);
		if (yt_hostile_bribe_run(&state, &hostile_bribe_ops, &tape,
		    &error) || error.status != YT_IO_ERROR || state.complete
		    || tape.calls != failure + 1U
		    || memcmp(tape.events, accepted_events,
		    (failure + 1U) * sizeof(accepted_events[0])) != 0)
			return false;
	}

	/* Ordinary quiet, Xannor force and raw planet truth partitions. */
	hostile_bribe_fixture(&tape, &state);
	state.owner = 3.0f;
	if (!yt_hostile_bribe_run(&state, &hostile_bribe_ops, &tape, NULL)
	    || state.route != YT_HOSTILE_BRIBE_SCANNER
	    || state.draws_consumed != 1U || tape.calls != 2U)
		return false;
	hostile_bribe_fixture(&tape, &state);
	state.owner = -1.0f;
	if (!yt_hostile_bribe_run(&state, &hostile_bribe_ops, &tape, NULL)
	    || state.route != YT_HOSTILE_BRIBE_COMBAT
	    || !state.forced_attack || !state.commitment_stored
	    || tape.combat_commitment != 20.0 || tape.calls != 3U)
		return false;
	hostile_bribe_fixture(&tape, &state);
	state.planet_link_raw[0] = 0x12U;
	state.planet_link_raw[1] = 0x34U;
	state.planet_link_raw[2] = 0x56U;
	if (!yt_hostile_bribe_run(&state, &hostile_bribe_ops, &tape, NULL)
	    || state.branch != YT_HOSTILE_BRIBE_ACCEPTED)
		return false;
	hostile_bribe_fixture(&tape, &state);
	state.planet_link_raw[3] = 0x80U;
	if (!yt_hostile_bribe_run(&state, &hostile_bribe_ops, &tape, NULL)
	    || state.branch != YT_HOSTILE_BRIBE_PLANET_REFUSAL
	    || state.draws_consumed != 0U || tape.calls != 1U)
		return false;

	/* Sticky CINT runs after both draws and preserves rounding/faults. */
	hostile_bribe_fixture(&tape, &state);
	(void)qb_mbf32_encode(-1.0f, state.mercenaries_hurt_raw);
	if (!yt_hostile_bribe_run(&state, &hostile_bribe_ops, &tape, NULL)
	    || !state.mercenaries_hurt_converted
	    || state.mercenaries_hurt_cint != -1
	    || state.branch != YT_HOSTILE_BRIBE_LIFE_DEMAND
	    || state.draws_consumed != 2U)
		return false;
	hostile_bribe_fixture(&tape, &state);
	(void)qb_mbf32_encode(0.4f, state.mercenaries_hurt_raw);
	if (!yt_hostile_bribe_run(&state, &hostile_bribe_ops, &tape, NULL)
	    || state.mercenaries_hurt_cint != 0
	    || state.branch != YT_HOSTILE_BRIBE_ACCEPTED)
		return false;
	hostile_bribe_fixture(&tape, &state);
	(void)qb_mbf32_encode(0.6f, state.mercenaries_hurt_raw);
	if (!yt_hostile_bribe_run(&state, &hostile_bribe_ops, &tape, NULL)
	    || state.mercenaries_hurt_cint != 1
	    || state.branch != YT_HOSTILE_BRIBE_LIFE_DEMAND)
		return false;
	hostile_bribe_fixture(&tape, &state);
	(void)qb_mbf32_encode(40000.0f, state.mercenaries_hurt_raw);
	yt_error_clear(&error);
	if (yt_hostile_bribe_run(&state, &hostile_bribe_ops, &tape, &error)
	    || error.status != YT_RANGE
	    || strcmp(error.operation, "bribe:mercenary-sticky-cint") != 0
	    || state.mercenaries_hurt_converted
	    || state.draws_consumed != 2U || tape.calls != 2U)
		return false;

	/* Life demand: combat, fatal and rounded sub-one menu return. */
	hostile_bribe_fixture(&tape, &state);
	tape.draws[0] = 0.01f;
	if (!yt_hostile_bribe_run(&state, &hostile_bribe_ops, &tape, NULL)
	    || state.branch != YT_HOSTILE_BRIBE_LIFE_DEMAND
	    || state.route != YT_HOSTILE_BRIBE_COMBAT
	    || state.draws_consumed != 2U || tape.calls != 4U)
		return false;
	hostile_bribe_fixture(&tape, &state);
	tape.draws[0] = 0.01f;
	state.ship_fighters = 0.0;
	state.shields = 0.0f;
	if (!yt_hostile_bribe_run(&state, &hostile_bribe_ops, &tape, NULL)
	    || state.route != YT_HOSTILE_BRIBE_FATAL || !state.fatal_called)
		return false;
	hostile_bribe_fixture(&tape, &state);
	tape.draws[0] = 0.01f;
	state.ship_fighters = 0.4;
	if (!yt_hostile_bribe_run(&state, &hostile_bribe_ops, &tape, NULL)
	    || state.route != YT_HOSTILE_BRIBE_HOSTILE_MENU
	    || !state.direct_hostile_menu || state.combat_called)
		return false;

	/* Empty and rejected offers preserve their distinct draw counts. */
	hostile_bribe_fixture(&tape, &state);
	tape.response = "";
	if (!yt_hostile_bribe_run(&state, &hostile_bribe_ops, &tape, NULL)
	    || state.branch != YT_HOSTILE_BRIBE_EMPTY_OFFER
	    || state.draws_consumed != 2U || state.offer_stored)
		return false;
	hostile_bribe_fixture(&tape, &state);
	tape.response = "5";
	if (!yt_hostile_bribe_run(&state, &hostile_bribe_ops, &tape, NULL)
	    || state.branch != YT_HOSTILE_BRIBE_REJECTED
	    || state.route != YT_HOSTILE_BRIBE_COMBAT
	    || state.draws_consumed != 3U || !state.commitment_stored)
		return false;
	hostile_bribe_fixture(&tape, &state);
	tape.response = "101";
	if (!yt_hostile_bribe_run(&state, &hostile_bribe_ops, &tape, NULL)
	    || !state.above_credits || state.draws_consumed != 3U
	    || state.branch != YT_HOSTILE_BRIBE_REJECTED)
		return false;

	/* Commitment conversion failure retains the selected forced branch. */
	hostile_bribe_fixture(&tape, &state);
	state.owner = -1.0f;
	state.ship_fighters = HUGE_VAL;
	yt_error_clear(&error);
	if (yt_hostile_bribe_run(&state, &hostile_bribe_ops, &tape, &error)
	    || error.status != YT_RANGE || state.commitment_stored
	    || !state.forced_attack
	    || strcmp(error.operation, "bribe:commitment-csng") != 0)
		return false;

	hostile_bribe_fixture(&tape, &state);
	return !yt_hostile_bribe_run(NULL, &hostile_bribe_ops, &tape, NULL)
	    && !yt_hostile_bribe_run(&state, NULL, &tape, NULL);
}

struct direct_attack_attrition_tape {
	float values[8];
	size_t calls;
	size_t fail_at;
};

static bool
direct_attack_attrition_draw(void *context, float *value,
    struct yt_error *error)
{
	struct direct_attack_attrition_tape *tape = context;
	size_t call = tape->calls++;

	if (call == tape->fail_at) {
		if (error != NULL) {
			error->status = YT_IO_ERROR;
			(void)snprintf(error->operation,
			    sizeof(error->operation), "%s",
			    "direct Attack attrition draw");
		}
		return false;
	}
	if (call >= YT_ARRAY_LEN(tape->values))
		return false;
	*value = tape->values[call];
	return true;
}

static bool
check_direct_attack_attrition_model(void)
{
	struct direct_attack_attrition_tape tape;
	struct yt_direct_attack_attrition_state state;
	struct yt_error error;

	memset(&tape, 0, sizeof(tape));
	tape.fail_at = (size_t)-1;
	state = (struct yt_direct_attack_attrition_state){
		.committed = 0.0,
		.defenders = 4.0,
		.cloak = 0.0f,
	};
	if (!yt_direct_attack_attrition_run(&state,
	    direct_attack_attrition_draw, &tape, NULL)
	    || !state.complete || state.iterations != 0U || tape.calls != 0U
	    || state.attacker_loss != 0.0 || state.defender_loss != 0.0
	    || state.quantum != 0.0f)
		return false;

	memset(&tape, 0, sizeof(tape));
	tape.fail_at = (size_t)-1;
	tape.values[0] = 0.44999998807907104f;
	state = (struct yt_direct_attack_attrition_state){
		.committed = 1.0,
		.defenders = 1.0,
		.cloak = 0.0f,
	};
	if (!yt_direct_attack_attrition_run(&state,
	    direct_attack_attrition_draw, &tape, NULL)
	    || state.attacker_loss != 0.0 || state.defender_loss != 1.0
	    || state.quantum != 1.0f || state.iterations != 1U
	    || tape.calls != 1U)
		return false;

	memset(&tape, 0, sizeof(tape));
	tape.fail_at = (size_t)-1;
	tape.values[0] = 0.0f;
	tape.values[1] = 0.0f;
	state = (struct yt_direct_attack_attrition_state){
		.committed = 1.5,
		.defenders = 1.5,
		.cloak = 0.0f,
	};
	if (!yt_direct_attack_attrition_run(&state,
	    direct_attack_attrition_draw, &tape, NULL)
	    || state.attacker_loss != 2.0 || state.defender_loss != 0.0
	    || state.iterations != 2U || tape.calls != 2U)
		return false;

	memset(&tape, 0, sizeof(tape));
	tape.values[0] = 0.0f;
	tape.fail_at = 1U;
	state = (struct yt_direct_attack_attrition_state){
		.committed = 100.0,
		.defenders = 40.0,
		.cloak = 0.0f,
	};
	yt_error_clear(&error);
	if (yt_direct_attack_attrition_run(&state,
	    direct_attack_attrition_draw, &tape, &error)
	    || state.complete || state.attacker_loss != 2.0
	    || state.defender_loss != 0.0 || state.quantum != 2.0f
	    || state.iterations != 1U || tape.calls != 2U
	    || error.status != YT_IO_ERROR)
		return false;

	memset(&tape, 0, sizeof(tape));
	tape.fail_at = (size_t)-1;
	tape.values[0] = 0.0f;
	tape.values[1] = 1.0f;
	tape.values[2] = 0.0f;
	tape.values[3] = 1.0f;
	tape.values[4] = 0.0f;
	tape.values[5] = 1.0f;
	tape.values[6] = 0.0f;
	state = (struct yt_direct_attack_attrition_state){
		.committed = 4.0,
		.defenders = 4.0,
		.cloak = 0.0f,
	};
	return yt_direct_attack_attrition_run(&state,
	    direct_attack_attrition_draw, &tape, NULL)
	    && state.complete && state.attacker_loss == 4.0
	    && state.defender_loss == 3.0 && state.quantum == 1.0f
	    && state.iterations == 7U && tape.calls == 7U
	    && !yt_direct_attack_attrition_run(NULL,
	    direct_attack_attrition_draw, &tape, NULL)
	    && !yt_direct_attack_attrition_run(&state, NULL, &tape, NULL);
}

enum fighter_shield_spill_event {
	FIGHTER_SHIELD_SPILL_RANDOM,
	FIGHTER_SHIELD_SPILL_FIGHTER_ROW,
	FIGHTER_SHIELD_SPILL_SHIELD_ROW,
};

struct fighter_shield_spill_tape {
	enum fighter_shield_spill_event events[8];
	size_t event_count;
	size_t fail_at;
	float draws[4];
	size_t draw_count;
	size_t draw_position;
	uint8_t rows[2][128];
	size_t row_length[2];
};

static bool
fighter_shield_spill_event(struct fighter_shield_spill_tape *tape,
    enum fighter_shield_spill_event event, struct yt_error *error)
{
	size_t index = tape->event_count;

	if (index >= YT_ARRAY_LEN(tape->events))
		return false;
	tape->events[tape->event_count++] = event;
	if (index != tape->fail_at)
		return true;
	if (error != NULL) {
		error->status = YT_IO_ERROR;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    "fighter shield spill dependency");
	}
	return false;
}

static bool
fighter_shield_spill_draw(void *context, float *value,
    struct yt_error *error)
{
	struct fighter_shield_spill_tape *tape = context;

	if (!fighter_shield_spill_event(tape, FIGHTER_SHIELD_SPILL_RANDOM,
	    error) || tape->draw_position >= tape->draw_count)
		return false;
	*value = tape->draws[tape->draw_position++];
	return true;
}

static bool
fighter_shield_spill_present(void *context, const uint8_t *text,
    size_t length, enum yt_fighter_shield_spill_output_kind kind,
    struct yt_error *error)
{
	struct fighter_shield_spill_tape *tape = context;
	enum fighter_shield_spill_event event =
	    kind == YT_FIGHTER_SHIELD_SPILL_FIGHTER_ROW
	    ? FIGHTER_SHIELD_SPILL_FIGHTER_ROW
	    : FIGHTER_SHIELD_SPILL_SHIELD_ROW;

	if ((size_t)kind >= YT_ARRAY_LEN(tape->rows)
	    || length > sizeof(tape->rows[0])
	    || !fighter_shield_spill_event(tape, event, error))
		return false;
	if (length != 0U)
		memcpy(tape->rows[kind], text, length);
	tape->row_length[kind] = length;
	return true;
}

static const struct yt_fighter_shield_spill_ops fighter_spill_ops = {
	fighter_shield_spill_draw,
	fighter_shield_spill_present,
};

static bool
check_fighter_shield_spill_transaction(void)
{
	static const uint8_t fighter_row[] = "Fighters remaining: 0";
	static const uint8_t shield_row[] = "Shields reduced to: 101";
	static const enum fighter_shield_spill_event expected[] = {
		FIGHTER_SHIELD_SPILL_RANDOM,
		FIGHTER_SHIELD_SPILL_RANDOM,
		FIGHTER_SHIELD_SPILL_FIGHTER_ROW,
		FIGHTER_SHIELD_SPILL_SHIELD_ROW,
	};
	struct fighter_shield_spill_tape tape;
	struct yt_fighter_shield_spill_state state;
	struct yt_error error;
	size_t failure;

	memset(&tape, 0, sizeof(tape));
	tape.fail_at = (size_t)-1;
	tape.draws[0] = 0.999f;
	tape.draws[1] = 0.5f;
	tape.draw_count = 2U;
	state = (struct yt_fighter_shield_spill_state){
		.fighters = 101.0,
		.shields = 101.0f,
	};
	if (!yt_fighter_shield_spill_run(&state, &fighter_spill_ops, &tape,
	    NULL) || !state.complete || state.fighters != 0.0
	    || state.shields != 101.0f || state.iterations != 2U
	    || !state.fighter_row_presented || !state.shield_row_presented
	    || tape.event_count != YT_ARRAY_LEN(expected)
	    || memcmp(tape.events, expected, sizeof(expected)) != 0
	    || tape.row_length[YT_FIGHTER_SHIELD_SPILL_FIGHTER_ROW]
	    != sizeof(fighter_row) - 1U
	    || memcmp(tape.rows[YT_FIGHTER_SHIELD_SPILL_FIGHTER_ROW],
	    fighter_row, sizeof(fighter_row) - 1U) != 0
	    || tape.row_length[YT_FIGHTER_SHIELD_SPILL_SHIELD_ROW]
	    != sizeof(shield_row) - 1U
	    || memcmp(tape.rows[YT_FIGHTER_SHIELD_SPILL_SHIELD_ROW],
	    shield_row, sizeof(shield_row) - 1U) != 0)
		return false;

	for (failure = 0U; failure < YT_ARRAY_LEN(expected); ++failure) {
		memset(&tape, 0, sizeof(tape));
		tape.fail_at = failure;
		tape.draws[0] = 0.999f;
		tape.draws[1] = 0.5f;
		tape.draw_count = 2U;
		state = (struct yt_fighter_shield_spill_state){
			.fighters = 101.0,
			.shields = 101.0f,
		};
		yt_error_clear(&error);
		if (yt_fighter_shield_spill_run(&state, &fighter_spill_ops,
		    &tape, &error) || state.complete
		    || tape.event_count != failure + 1U
		    || memcmp(tape.events, expected,
		    tape.event_count * sizeof(tape.events[0])) != 0
		    || state.iterations != (failure == 0U ? 0U
		    : failure == 1U ? 1U : 2U)
		    || state.fighters != (failure == 0U ? 101.0
		    : failure == 1U ? 1.0 : 0.0)
		    || state.fighter_row_presented != (failure > 2U)
		    || state.shield_row_presented || error.status != YT_IO_ERROR)
			return false;
	}

	memset(&tape, 0, sizeof(tape));
	tape.fail_at = (size_t)-1;
	state = (struct yt_fighter_shield_spill_state){
		.fighters = 0.0,
		.shields = 2.0f,
	};
	if (!yt_fighter_shield_spill_run(&state, &fighter_spill_ops, &tape,
	    NULL) || !state.complete || state.iterations != 0U
	    || tape.event_count != 2U)
		return false;

	memset(&tape, 0, sizeof(tape));
	tape.fail_at = (size_t)-1;
	tape.draws[0] = 0.0f;
	tape.draw_count = 1U;
	state = (struct yt_fighter_shield_spill_state){
		.fighters = 1.0,
		.shields = 0.5f,
	};
	return yt_fighter_shield_spill_run(&state, &fighter_spill_ops, &tape,
	    NULL) && state.fighters == 1.0 && state.shields == -0.5f
	    && state.iterations == 1U
	    && !yt_fighter_shield_spill_run(NULL, &fighter_spill_ops, &tape,
	    NULL) && !yt_fighter_shield_spill_run(&state, NULL, &tape, NULL);
}

enum direct_attack_combat_event {
	DIRECT_COMBAT_READ_TARGET,
	DIRECT_COMBAT_READ_CURRENT,
	DIRECT_COMBAT_WRITE_TARGET,
	DIRECT_COMBAT_WRITE_CURRENT,
	DIRECT_COMBAT_PRESENT_TOO_MANY,
	DIRECT_COMBAT_PRESENT_ATTACKER,
	DIRECT_COMBAT_PRESENT_DEFENDER,
	DIRECT_COMBAT_PRESENT_ELIMINATED,
	DIRECT_COMBAT_SOUND,
	DIRECT_COMBAT_RADIO,
	DIRECT_COMBAT_RANDOM,
	DIRECT_COMBAT_SPILL,
	DIRECT_COMBAT_KILL,
};

struct direct_attack_combat_tape {
	struct yt_player world[4];
	enum direct_attack_combat_event events[32];
	size_t event_count;
	size_t fail_at;
	float draws[8];
	size_t draw_count;
	size_t draw_position;
	double spill_fighters;
	float spill_shields;
	uint8_t output[4][300];
	size_t output_length[4];
	uint8_t radio[160];
	size_t radio_length;
	float radio_recipient;
	size_t kills;
};

static bool
direct_attack_combat_event(struct direct_attack_combat_tape *tape,
    enum direct_attack_combat_event event, struct yt_error *error)
{
	size_t index = tape->event_count;

	if (index >= YT_ARRAY_LEN(tape->events))
		return false;
	tape->events[tape->event_count++] = event;
	if (index != tape->fail_at)
		return true;
	if (error != NULL) {
		error->status = YT_IO_ERROR;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    "direct Attack combat dependency");
	}
	return false;
}

static bool
direct_attack_combat_read(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct direct_attack_combat_tape *tape = context;
	enum direct_attack_combat_event event = player_record == 2
	    ? DIRECT_COMBAT_READ_CURRENT : DIRECT_COMBAT_READ_TARGET;

	if (player_record < 0
	    || (size_t)player_record >= YT_ARRAY_LEN(tape->world)
	    || !direct_attack_combat_event(tape, event, error))
		return false;
	*player = tape->world[player_record];
	return true;
}

static bool
direct_attack_combat_write(void *context, int player_record,
    const struct yt_player *player, struct yt_error *error)
{
	struct direct_attack_combat_tape *tape = context;
	enum direct_attack_combat_event event = player_record == 2
	    ? DIRECT_COMBAT_WRITE_CURRENT : DIRECT_COMBAT_WRITE_TARGET;

	if (player_record < 0
	    || (size_t)player_record >= YT_ARRAY_LEN(tape->world)
	    || !direct_attack_combat_event(tape, event, error))
		return false;
	tape->world[player_record] = *player;
	return true;
}

static bool
direct_attack_combat_present(void *context, const uint8_t *text,
    size_t length, enum yt_direct_attack_combat_output_kind kind,
    struct yt_error *error)
{
	static const enum direct_attack_combat_event events[] = {
		DIRECT_COMBAT_PRESENT_TOO_MANY,
		DIRECT_COMBAT_PRESENT_ATTACKER,
		DIRECT_COMBAT_PRESENT_DEFENDER,
		DIRECT_COMBAT_PRESENT_ELIMINATED,
	};
	struct direct_attack_combat_tape *tape = context;

	if ((size_t)kind >= YT_ARRAY_LEN(events) || length > 300U
	    || !direct_attack_combat_event(tape, events[kind], error))
		return false;
	if (length != 0U)
		memcpy(tape->output[kind], text, length);
	tape->output_length[kind] = length;
	return true;
}

static bool
direct_attack_combat_sound(void *context, float selector,
    struct yt_error *error)
{
	return selector == 2.0f && direct_attack_combat_event(context,
	    DIRECT_COMBAT_SOUND, error);
}

static bool
direct_attack_combat_radio(void *context, const uint8_t *text,
    size_t length, float recipient, struct yt_error *error)
{
	struct direct_attack_combat_tape *tape = context;

	if (length > sizeof(tape->radio)
	    || !direct_attack_combat_event(tape, DIRECT_COMBAT_RADIO, error))
		return false;
	if (length != 0U)
		memcpy(tape->radio, text, length);
	tape->radio_length = length;
	tape->radio_recipient = recipient;
	return true;
}

static bool
direct_attack_combat_random(void *context, float *value,
    struct yt_error *error)
{
	struct direct_attack_combat_tape *tape = context;

	if (!direct_attack_combat_event(tape, DIRECT_COMBAT_RANDOM, error)
	    || tape->draw_position >= tape->draw_count)
		return false;
	*value = tape->draws[tape->draw_position++];
	return true;
}

static bool
direct_attack_combat_spill(void *context, double *fighters,
    float *shields, struct yt_error *error)
{
	struct direct_attack_combat_tape *tape = context;

	if (!direct_attack_combat_event(tape, DIRECT_COMBAT_SPILL, error))
		return false;
	*fighters = tape->spill_fighters;
	*shields = tape->spill_shields;
	return true;
}

static bool
direct_attack_combat_kill(void *context, int target_record,
    int current_player_record, float current_sector, float target_shields,
    struct yt_error *error)
{
	struct direct_attack_combat_tape *tape = context;

	if (target_record != 3 || current_player_record != 2
	    || current_sector != 7.0f || target_shields > 0.0f
	    || !direct_attack_combat_event(tape, DIRECT_COMBAT_KILL, error))
		return false;
	++tape->kills;
	return true;
}

static const struct yt_direct_attack_combat_ops direct_combat_ops = {
	direct_attack_combat_read,
	direct_attack_combat_write,
	direct_attack_combat_present,
	direct_attack_combat_sound,
	direct_attack_combat_radio,
	direct_attack_combat_random,
	direct_attack_combat_spill,
	direct_attack_combat_kill,
};

static void
direct_attack_combat_fixture(struct direct_attack_combat_tape *tape,
    struct yt_direct_attack_combat_state *state, float target_fighters,
    float target_shields, double committed)
{
	struct yt_record current_record;
	struct yt_record target_record;
	size_t index;

	memset(tape, 0, sizeof(*tape));
	tape->fail_at = (size_t)-1;
	tape->spill_fighters = committed;
	tape->spill_shields = target_shields;
	for (index = 0U; index < YT_RECORD_SIZE; ++index) {
		tape->world[2].record.bytes[index] =
		    (uint8_t)(index * 5U + 3U);
		tape->world[3].record.bytes[index] =
		    (uint8_t)(index * 7U + 11U);
	}
	yt_record_set_text(&tape->world[2].record,
	    (const uint8_t *)"Ada", 3U);
	(void)yt_record_set_number(&tape->world[2].record, YT_F53, 0.0f);
	(void)yt_record_set_number(&tape->world[2].record, YT_F57, 7.0f);
	(void)yt_record_set_number(&tape->world[2].record, YT_F61, 5.0f);
	(void)yt_record_set_number(&tape->world[2].record, YT_F85, 3.0f);
	(void)yt_record_set_number(&tape->world[2].record, YT_F125, 0.0f);
	current_record = tape->world[2].record;
	yt_player_decode(&tape->world[2], &current_record);
	yt_record_set_text(&tape->world[3].record,
	    (const uint8_t *)"Victim", 6U);
	(void)yt_record_set_number(&tape->world[3].record, YT_F53,
	    target_shields);
	(void)yt_record_set_number(&tape->world[3].record, YT_F61,
	    target_fighters);
	(void)yt_record_set_number(&tape->world[3].record, YT_F85, 6.0f);
	target_record = tape->world[3].record;
	yt_player_decode(&tape->world[3], &target_record);
	*state = (struct yt_direct_attack_combat_state){
		.current_player_record = 2,
		.target_record = 3,
		.committed = committed,
	};
}

static bool
check_direct_attack_combat_transaction(void)
{
	static const uint8_t attacker[] =
	    "You lost 0 fighter(s), 2 remain.";
	static const uint8_t defender[] =
	    "You destroyed 2 enemy fighters, 0 remain.";
	static const uint8_t eliminated[] =
	    "Fighters eliminated! Attacking the ship!";
	static const uint8_t radio[] =
	    "Ada destroyed 0 of your fighters!";
	static const uint8_t too_many[] = "You only have 5!";
	struct direct_attack_combat_tape tape;
	struct yt_direct_attack_combat_state state;
	enum direct_attack_combat_event expected[32];
	struct yt_record current_before;
	struct yt_record target_before;
	struct yt_error error;
	size_t expected_count;
	size_t failure;
	size_t index;

	direct_attack_combat_fixture(&tape, &state, 2.0f, 0.0f, 3.0);
	current_before = tape.world[2].record;
	target_before = tape.world[3].record;
	tape.draws[0] = 1.0f;
	tape.draws[1] = 1.0f;
	tape.draw_count = 2U;
	if (!yt_direct_attack_combat_run(&state, &direct_combat_ops, &tape,
	    NULL) || !state.complete
	    || state.route != YT_DIRECT_ATTACK_COMBAT_KILL_RETURN
	    || state.defenders != 0.0 || state.attacking != 3.0
	    || state.cached_reserve != 2.0 || state.attrition.iterations != 2U
	    || !state.reserve_written || !state.current_casualty_written
	    || !state.target_casualty_written || !state.target_shield_written
	    || !state.current_final_written || tape.kills != 1U
	    || tape.world[2].fighters != 5.0f
	    || tape.world[3].fighters != 0.0f
	    || tape.output_length[YT_DIRECT_ATTACK_COMBAT_ATTACKER_ROW]
	    != sizeof(attacker) - 1U
	    || memcmp(tape.output[YT_DIRECT_ATTACK_COMBAT_ATTACKER_ROW],
	    attacker, sizeof(attacker) - 1U) != 0
	    || tape.output_length[YT_DIRECT_ATTACK_COMBAT_DEFENDER_ROW]
	    != sizeof(defender) - 1U
	    || memcmp(tape.output[YT_DIRECT_ATTACK_COMBAT_DEFENDER_ROW],
	    defender, sizeof(defender) - 1U) != 0
	    || tape.output_length[YT_DIRECT_ATTACK_COMBAT_ELIMINATED_ROW]
	    != sizeof(eliminated) - 1U
	    || memcmp(tape.output[YT_DIRECT_ATTACK_COMBAT_ELIMINATED_ROW],
	    eliminated, sizeof(eliminated) - 1U) != 0
	    || tape.radio_length != sizeof(radio) - 1U
	    || memcmp(tape.radio, radio, sizeof(radio) - 1U) != 0
	    || tape.radio_recipient != 3.0f)
		return false;
	for (index = 0U; index < YT_RECORD_SIZE; ++index) {
		if ((index < YT_F61 || index >= YT_F61 + 4U)
		    && tape.world[2].record.bytes[index]
		    != current_before.bytes[index])
			return false;
		if ((index < YT_F53 || index >= YT_F53 + 4U)
		    && (index < YT_F61 || index >= YT_F61 + 4U)
		    && tape.world[3].record.bytes[index]
		    != target_before.bytes[index])
			return false;
	}
	expected_count = tape.event_count;
	if (expected_count != 19U)
		return false;
	memcpy(expected, tape.events, expected_count * sizeof(expected[0]));
	for (failure = 0U; failure < expected_count; ++failure) {
		direct_attack_combat_fixture(&tape, &state, 2.0f, 0.0f, 3.0);
		tape.draws[0] = 1.0f;
		tape.draws[1] = 1.0f;
		tape.draw_count = 2U;
		tape.fail_at = failure;
		yt_error_clear(&error);
		if (yt_direct_attack_combat_run(&state, &direct_combat_ops,
		    &tape, &error) || state.complete
		    || tape.event_count != failure + 1U
		    || memcmp(tape.events, expected,
		    tape.event_count * sizeof(tape.events[0])) != 0
		    || error.status != YT_IO_ERROR
		    || state.reserve_written != (failure > 2U)
		    || state.current_casualty_written != (failure > 8U)
		    || state.target_casualty_written != (failure > 10U)
		    || state.target_shield_written != (failure > 15U)
		    || state.current_final_written != (failure > 17U)
		    || tape.world[2].fighters
		    != (failure > 8U ? 5.0f : failure > 2U ? 2.0f : 5.0f)
		    || tape.world[3].fighters
		    != (failure > 10U ? 0.0f : 2.0f))
			return false;
	}

	direct_attack_combat_fixture(&tape, &state, 2.0f, 0.0f, 6.0);
	if (!yt_direct_attack_combat_run(&state, &direct_combat_ops, &tape,
	    NULL) || state.route != YT_DIRECT_ATTACK_COMBAT_TOO_MANY
	    || !state.complete || tape.event_count != 3U
	    || tape.output_length[YT_DIRECT_ATTACK_COMBAT_TOO_MANY_ROW]
	    != sizeof(too_many) - 1U
	    || memcmp(tape.output[YT_DIRECT_ATTACK_COMBAT_TOO_MANY_ROW],
	    too_many, sizeof(too_many) - 1U) != 0)
		return false;

	direct_attack_combat_fixture(&tape, &state, 4.0f, 0.0f, 1.0);
	tape.draws[0] = 0.0f;
	tape.draw_count = 1U;
	if (!yt_direct_attack_combat_run(&state, &direct_combat_ops, &tape,
	    NULL) || state.route != YT_DIRECT_ATTACK_COMBAT_CASUALTY_RETURN
	    || !state.complete || state.defenders != 4.0
	    || state.attacking != 0.0 || tape.kills != 0U)
		return false;

	direct_attack_combat_fixture(&tape, &state, 1.0f, 5.0f, 3.0);
	tape.draws[0] = 1.0f;
	tape.draw_count = 1U;
	tape.spill_fighters = 1.0;
	tape.spill_shields = 2.0f;
	if (!yt_direct_attack_combat_run(&state, &direct_combat_ops, &tape,
	    NULL) || state.route != YT_DIRECT_ATTACK_COMBAT_SHIELD_RETURN
	    || !state.complete || state.attacking != 1.0
	    || state.target_shields != 2.0f || tape.world[2].fighters != 3.0f
	    || tape.world[3].shields != 2.0f || tape.kills != 0U)
		return false;

	direct_attack_combat_fixture(&tape, &state, 1.0f, 5.0f, 3.0);
	tape.draws[0] = 1.0f;
	tape.draw_count = 1U;
	tape.fail_at = 13U;
	yt_error_clear(&error);
	return !yt_direct_attack_combat_run(&state, &direct_combat_ops, &tape,
	    &error) && !state.complete && error.status == YT_IO_ERROR
	    && !yt_direct_attack_combat_run(NULL, &direct_combat_ops, &tape,
	    NULL) && !yt_direct_attack_combat_run(&state, NULL, &tape, NULL);
}

enum direct_attack_event {
	DIRECT_ATTACK_PRESENT_TITLE,
	DIRECT_ATTACK_READ_CURRENT,
	DIRECT_ATTACK_READ_CANDIDATE,
	DIRECT_ATTACK_PRESENT_NO_FIGHTERS,
	DIRECT_ATTACK_PRESENT_TEAM,
	DIRECT_ATTACK_CONFIRM,
	DIRECT_ATTACK_PRESENT_COMMITMENT,
	DIRECT_ATTACK_AMOUNT,
	DIRECT_ATTACK_COMBAT,
	DIRECT_ATTACK_PRESENT_NONE_SELECTED,
	DIRECT_ATTACK_PRESENT_NONE_VISIBLE,
};

struct direct_attack_tape {
	struct yt_player player[6];
	float sector_cache[6];
	float cloak_cache[6];
	enum direct_attack_event events[20];
	size_t event_count;
	size_t fail_at;
	int read_records[8];
	size_t read_count;
	enum yt_direct_attack_confirmation answers[4];
	size_t answer_count;
	size_t answer_position;
	char amount[64];
	uint8_t output[6][300];
	size_t output_length[6];
	uint8_t prompts[4][300];
	size_t prompt_length[4];
	size_t prompt_count;
	int combat_target;
	double combat_committed;
};

static bool
direct_attack_event(struct direct_attack_tape *tape,
    enum direct_attack_event event, struct yt_error *error)
{
	size_t index = tape->event_count;

	if (index >= YT_ARRAY_LEN(tape->events))
		return false;
	tape->events[tape->event_count++] = event;
	if (index != tape->fail_at)
		return true;
	if (error != NULL) {
		error->status = YT_IO_ERROR;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    "direct Attack selector dependency");
	}
	return false;
}

static bool
direct_attack_read(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct direct_attack_tape *tape = context;
	enum direct_attack_event event = player_record == 2
	    ? DIRECT_ATTACK_READ_CURRENT : DIRECT_ATTACK_READ_CANDIDATE;

	if (player_record < 0
	    || (size_t)player_record >= YT_ARRAY_LEN(tape->player)
	    || tape->read_count >= YT_ARRAY_LEN(tape->read_records)
	    || !direct_attack_event(tape, event, error))
		return false;
	tape->read_records[tape->read_count++] = player_record;
	*player = tape->player[player_record];
	return true;
}

static bool
direct_attack_present(void *context, const uint8_t *text, size_t length,
    enum yt_direct_attack_output_kind kind, struct yt_error *error)
{
	static const enum direct_attack_event events[] = {
		DIRECT_ATTACK_PRESENT_TITLE,
		DIRECT_ATTACK_PRESENT_NO_FIGHTERS,
		DIRECT_ATTACK_PRESENT_TEAM,
		DIRECT_ATTACK_PRESENT_COMMITMENT,
		DIRECT_ATTACK_PRESENT_NONE_SELECTED,
		DIRECT_ATTACK_PRESENT_NONE_VISIBLE,
	};
	struct direct_attack_tape *tape = context;

	if ((size_t)kind >= YT_ARRAY_LEN(events) || length > 300U
	    || !direct_attack_event(tape, events[kind], error))
		return false;
	if (length != 0U)
		memcpy(tape->output[kind], text, length);
	tape->output_length[kind] = length;
	return true;
}

static bool
direct_attack_confirm(void *context, const uint8_t *prompt, size_t length,
    enum yt_direct_attack_confirmation *answer, struct yt_error *error)
{
	struct direct_attack_tape *tape = context;

	if (length > 300U || tape->prompt_count >= YT_ARRAY_LEN(tape->prompts)
	    || tape->answer_position >= tape->answer_count
	    || !direct_attack_event(tape, DIRECT_ATTACK_CONFIRM, error))
		return false;
	if (length != 0U)
		memcpy(tape->prompts[tape->prompt_count], prompt, length);
	tape->prompt_length[tape->prompt_count++] = length;
	*answer = tape->answers[tape->answer_position++];
	return true;
}

static bool
direct_attack_amount(void *context, char *response, size_t capacity,
    struct yt_error *error)
{
	struct direct_attack_tape *tape = context;
	size_t length = strlen(tape->amount);

	if (length >= capacity
	    || !direct_attack_event(tape, DIRECT_ATTACK_AMOUNT, error))
		return false;
	memcpy(response, tape->amount, length + 1U);
	return true;
}

static bool
direct_attack_combat_child(void *context, int target_record,
    double committed, struct yt_error *error)
{
	struct direct_attack_tape *tape = context;

	if (!direct_attack_event(tape, DIRECT_ATTACK_COMBAT, error))
		return false;
	tape->combat_target = target_record;
	tape->combat_committed = committed;
	return true;
}

static const struct yt_direct_attack_ops direct_attack_ops = {
	direct_attack_read,
	direct_attack_present,
	direct_attack_confirm,
	direct_attack_amount,
	direct_attack_combat_child,
};

static void
direct_attack_player_fixture(struct yt_player *player, const char *name,
    float fighters, float sector, float team)
{
	struct yt_record record;
	size_t index;
	size_t name_length = strlen(name);

	for (index = 0U; index < YT_RECORD_SIZE; ++index)
		record.bytes[index] = (uint8_t)(index * 11U + name_length);
	yt_record_set_text(&record, (const uint8_t *)name, name_length);
	(void)yt_record_set_number(&record, YT_F57, sector);
	(void)yt_record_set_number(&record, YT_F61, fighters);
	(void)yt_record_set_number(&record, YT_F85, (float)name_length);
	(void)yt_record_set_number(&record, YT_F89, team);
	yt_player_decode(player, &record);
}

static void
direct_attack_fixture(struct direct_attack_tape *tape,
    struct yt_direct_attack_state *state)
{
	size_t index;

	memset(tape, 0, sizeof(*tape));
	tape->fail_at = (size_t)-1;
	for (index = 0U; index < YT_ARRAY_LEN(tape->sector_cache); ++index) {
		tape->sector_cache[index] = 0.0f;
		tape->cloak_cache[index] = 0.0f;
	}
	direct_attack_player_fixture(&tape->player[2], "Ada", 5.0f, 7.0f,
	    1.0f);
	direct_attack_player_fixture(&tape->player[3], "Team", 2.0f, 7.0f,
	    1.0f);
	direct_attack_player_fixture(&tape->player[4], "Decline", 2.0f, 7.0f,
	    0.0f);
	direct_attack_player_fixture(&tape->player[5], "Fight", 2.0f, 7.0f,
	    0.0f);
	for (index = 2U; index < YT_ARRAY_LEN(tape->sector_cache); ++index)
		tape->sector_cache[index] = 7.0f;
	tape->answers[0] = YT_DIRECT_ATTACK_CONFIRM_NO;
	tape->answers[1] = YT_DIRECT_ATTACK_CONFIRM_YES;
	tape->answer_count = 2U;
	(void)snprintf(tape->amount, sizeof(tape->amount), "%s", "3");
	*state = (struct yt_direct_attack_state){
		.current_player_record = 2,
		.last_player_record = 5.0f,
		.conversion_mode = 0,
		.sector_cache = tape->sector_cache,
		.cloak_cache = tape->cloak_cache,
		.cache_count = YT_ARRAY_LEN(tape->sector_cache),
	};
}

static bool
check_direct_attack_transaction(void)
{
	static const uint8_t title[] = "<Attack>";
	static const uint8_t team[] = "NOT attacking team member Team!";
	static const uint8_t first_prompt[] = "Attack Decline (Y/N)[Y]? ";
	static const uint8_t second_prompt[] = "Attack Fight (Y/N)[Y]? ";
	static const uint8_t commitment[] =
	    "You have 5. Use how many fighters? [0] ";
	static const uint8_t no_fighters[] =
	    "You don't have any fighters.";
	static const uint8_t none_visible[] = "There's no one here!";
	static const uint8_t none_selected[] =
	    "There are no other ships in this sector.";
	struct direct_attack_tape tape;
	struct yt_direct_attack_state state;
	enum direct_attack_event expected[20];
	struct yt_error error;
	size_t expected_count;
	size_t failure;

	direct_attack_fixture(&tape, &state);
	if (!yt_direct_attack_run(&state, &direct_attack_ops, &tape, NULL)
	    || !state.complete || state.route != YT_DIRECT_ATTACK_COMBAT_RETURN
	    || state.enter_sector || !state.encountered || state.candidate != 5.0f
	    || state.target_record_cell != 5.0f || state.committed != 3.0
	    || tape.combat_target != 5 || tape.combat_committed != 3.0
	    || tape.read_count != 4U || tape.read_records[0] != 2
	    || tape.read_records[1] != 3 || tape.read_records[2] != 4
	    || tape.read_records[3] != 5
	    || tape.output_length[YT_DIRECT_ATTACK_TITLE_ROW]
	    != sizeof(title) - 1U
	    || memcmp(tape.output[YT_DIRECT_ATTACK_TITLE_ROW], title,
	    sizeof(title) - 1U) != 0
	    || tape.output_length[YT_DIRECT_ATTACK_TEAM_ROW]
	    != sizeof(team) - 1U
	    || memcmp(tape.output[YT_DIRECT_ATTACK_TEAM_ROW], team,
	    sizeof(team) - 1U) != 0
	    || tape.output_length[YT_DIRECT_ATTACK_COMMITMENT_PROMPT]
	    != sizeof(commitment) - 1U
	    || memcmp(tape.output[YT_DIRECT_ATTACK_COMMITMENT_PROMPT],
	    commitment, sizeof(commitment) - 1U) != 0
	    || tape.prompt_count != 2U
	    || tape.prompt_length[0] != sizeof(first_prompt) - 1U
	    || memcmp(tape.prompts[0], first_prompt,
	    sizeof(first_prompt) - 1U) != 0
	    || tape.prompt_length[1] != sizeof(second_prompt) - 1U
	    || memcmp(tape.prompts[1], second_prompt,
	    sizeof(second_prompt) - 1U) != 0)
		return false;
	expected_count = tape.event_count;
	if (expected_count != 11U)
		return false;
	memcpy(expected, tape.events, expected_count * sizeof(expected[0]));
	for (failure = 0U; failure < expected_count; ++failure) {
		direct_attack_fixture(&tape, &state);
		tape.fail_at = failure;
		yt_error_clear(&error);
		if (yt_direct_attack_run(&state, &direct_attack_ops, &tape,
		    &error) || state.complete
		    || tape.event_count != failure + 1U
		    || memcmp(tape.events, expected,
		    tape.event_count * sizeof(tape.events[0])) != 0
		    || error.status != YT_IO_ERROR
		    || state.encountered != (failure >= 4U)
		    || state.target_record_cell != (failure < 2U ? 0.0f
		    : failure < 4U ? 3.0f : failure < 6U ? 4.0f : 5.0f))
			return false;
	}

	direct_attack_fixture(&tape, &state);
	tape.player[2].fighters = 0.5f;
	(void)yt_record_set_number(&tape.player[2].record, YT_F61, 0.5f);
	if (!yt_direct_attack_run(&state, &direct_attack_ops, &tape, NULL)
	    || state.route != YT_DIRECT_ATTACK_NO_FIGHTERS || !state.complete
	    || tape.event_count != 3U
	    || tape.output_length[YT_DIRECT_ATTACK_NO_FIGHTERS_ROW]
	    != sizeof(no_fighters) - 1U
	    || memcmp(tape.output[YT_DIRECT_ATTACK_NO_FIGHTERS_ROW],
	    no_fighters, sizeof(no_fighters) - 1U) != 0)
		return false;

	direct_attack_fixture(&tape, &state);
	tape.sector_cache[3] = 8.0f;
	tape.sector_cache[4] = 8.0f;
	tape.sector_cache[5] = 8.0f;
	if (!yt_direct_attack_run(&state, &direct_attack_ops, &tape, NULL)
	    || state.route != YT_DIRECT_ATTACK_EXHAUSTED || !state.complete
	    || !state.enter_sector || state.encountered
	    || state.target_record_cell != 0.0f
	    || tape.output_length[YT_DIRECT_ATTACK_NONE_VISIBLE_ROW]
	    != sizeof(none_visible) - 1U
	    || memcmp(tape.output[YT_DIRECT_ATTACK_NONE_VISIBLE_ROW],
	    none_visible, sizeof(none_visible) - 1U) != 0)
		return false;

	direct_attack_fixture(&tape, &state);
	state.last_player_record = 3.0f;
	if (!yt_direct_attack_run(&state, &direct_attack_ops, &tape, NULL)
	    || state.route != YT_DIRECT_ATTACK_EXHAUSTED || !state.complete
	    || !state.enter_sector || !state.encountered
	    || tape.output_length[YT_DIRECT_ATTACK_NONE_SELECTED_ROW]
	    != sizeof(none_selected) - 1U
	    || memcmp(tape.output[YT_DIRECT_ATTACK_NONE_SELECTED_ROW],
	    none_selected, sizeof(none_selected) - 1U) != 0)
		return false;

	direct_attack_fixture(&tape, &state);
	tape.player[3].team = 0.0f;
	(void)yt_record_set_number(&tape.player[3].record, YT_F89, 0.0f);
	tape.answers[0] = YT_DIRECT_ATTACK_CONFIRM_EMPTY;
	tape.answer_count = 1U;
	(void)snprintf(tape.amount, sizeof(tape.amount), "%s", "0");
	if (!yt_direct_attack_run(&state, &direct_attack_ops, &tape, NULL)
	    || state.route != YT_DIRECT_ATTACK_CANCELLED || !state.complete
	    || state.target_record_cell != 3.0f || state.committed != 0.0
	    || tape.combat_target != 0)
		return false;

	direct_attack_fixture(&tape, &state);
	state.last_player_record = 6.0f;
	tape.sector_cache[3] = 8.0f;
	tape.sector_cache[4] = 8.0f;
	tape.sector_cache[5] = 8.0f;
	yt_error_clear(&error);
	return !yt_direct_attack_run(&state, &direct_attack_ops, &tape, &error)
	    && !state.complete && state.candidate == 6.0f
	    && error.status == YT_RANGE
	    && !yt_direct_attack_run(NULL, &direct_attack_ops, &tape, NULL)
	    && !yt_direct_attack_run(&state, NULL, &tape, NULL);
}

static bool
check_direct_attack_radio_model(void)
{
	static const uint8_t ordinary[] =
	    "Ada destroyed 0 of your fighters!";
	static const uint8_t alias_boundary[] =
	    "Ada destroyed .5 of your fighters!";
	static const uint8_t team_expected[] =
	    {'N','O','T',' ','a','t','t','a','c','k','i','n','g',' ',
	    't','e','a','m',' ','m','e','m','b','e','r',' ','A',0,'B','!'};
	static const uint8_t candidate_expected[] =
	    {'A','t','t','a','c','k',' ','A',0,'B',' ','(','Y','/','N',')',
	    '[','Y',']','?',' '};
	static const uint8_t commitment_expected[] =
	    "You have 5. Use how many fighters? [0] ";
	static const uint8_t too_many_expected[] = "You only have 5!";
	static const uint8_t attacker_expected[] =
	    "You lost 0 fighter(s), 2 remain.";
	static const uint8_t defender_expected[] =
	    "You destroyed 2 enemy fighters, 0 remain.";
	static const uint8_t rounding_expected[] =
	    "You lost 0 fighter(s), 3.900000095367432 remain.";
	static const uint8_t binary_name[] = {'A', 0, 'B'};
	struct yt_player player;
	struct yt_record before;
	uint8_t text[160];
	uint8_t second[160];
	size_t length;
	size_t second_length;
	size_t index;

	if (!yt_direct_attack_radio_text((const uint8_t *)"Ada", 3U, 1.0,
	    text, sizeof(text), &length)
	    || length != sizeof(ordinary) - 1U
	    || memcmp(text, ordinary, length) != 0)
		return false;
	if (!yt_direct_attack_radio_text((const uint8_t *)"Ada", 3U,
	    16777217.0, text, sizeof(text), &length)
	    || length != sizeof(alias_boundary) - 1U
	    || memcmp(text, alias_boundary, length) != 0)
		return false;
	if (yt_direct_attack_radio_text((const uint8_t *)"Ada", 3U, 1.0,
	    text, sizeof(ordinary) - 2U, &length)
	    || !yt_direct_attack_team_row(binary_name, sizeof(binary_name),
	    text, sizeof(text), &length)
	    || length != sizeof(team_expected)
	    || memcmp(text, team_expected, length) != 0
	    || !yt_direct_attack_candidate_prompt(binary_name,
	    sizeof(binary_name), text, sizeof(text), &length)
	    || length != sizeof(candidate_expected)
	    || memcmp(text, candidate_expected, length) != 0
	    || !yt_direct_attack_commitment_prompt(5.0, text, sizeof(text),
	    &length) || length != sizeof(commitment_expected) - 1U
	    || memcmp(text, commitment_expected, length) != 0
	    || !yt_direct_attack_too_many_row(5.0, text, sizeof(text),
	    &length) || length != sizeof(too_many_expected) - 1U
	    || memcmp(text, too_many_expected, length) != 0
	    || !yt_direct_attack_result_rows(0.0, 2.0, 2.0, 0.0,
	    text, sizeof(text), &length, second, sizeof(second),
	    &second_length)
	    || length != sizeof(attacker_expected) - 1U
	    || memcmp(text, attacker_expected, length) != 0
	    || second_length != sizeof(defender_expected) - 1U
	    || memcmp(second, defender_expected, second_length) != 0
	    || !yt_direct_attack_result_rows(0.0, 3.900000095367432,
	    1.0, 0.0, text, sizeof(text), &length, second,
	    sizeof(second), &second_length)
	    || length != sizeof(rounding_expected) - 1U
	    || memcmp(text, rounding_expected, length) != 0)
		return false;
	memset(&player, 0, sizeof(player));
	for (index = 0U; index < YT_RECORD_SIZE; ++index)
		player.record.bytes[index] = (uint8_t)(index ^ 0xa5U);
	before = player.record;
	yt_direct_attack_fighter_overlay(&player, 3.25f);
	if (player.fighters != 3.25f
	    || yt_record_get_number(&player.record, YT_F61) != 3.25f)
		return false;
	for (index = 0U; index < YT_RECORD_SIZE; ++index)
		if ((index < YT_F61 || index >= YT_F61 + 4U)
		    && player.record.bytes[index] != before.bytes[index])
			return false;
	before = player.record;
	yt_direct_attack_shield_overlay(&player, 7.5f);
	if (player.shields != 7.5f
	    || yt_record_get_number(&player.record, YT_F53) != 7.5f)
		return false;
	for (index = 0U; index < YT_RECORD_SIZE; ++index)
		if ((index < YT_F53 || index >= YT_F53 + 4U)
		    && player.record.bytes[index] != before.bytes[index])
			return false;
	return true;
}

static bool
check_planet_rename_model(void)
{
	struct yt_record source;
	struct yt_record expected;
	struct yt_planet planet;
	char name[300];
	size_t length;
	size_t index;

	if (!yt_planet_rename_protected(101.5f, 100.5f, 400.25f)
	    || !yt_planet_rename_protected(400.25f, 100.5f, 400.25f)
	    || !yt_planet_rename_protected(399.25f, 100.5f, 400.25f)
	    || yt_planet_rename_protected(398.0f, 100.5f, 400.25f))
		return false;

	memset(name, 'A', 256U);
	name[256] = '\0';
	if (yt_planet_rename_prepare_name(name, &length)
	    != YT_PLANET_RENAME_ACCEPTED || length != YT_TEXT_FIELD_SIZE
	    || name[0] != 'A' || name[YT_TEXT_FIELD_SIZE] != '\0')
		return false;
	for (index = 1; index < YT_TEXT_FIELD_SIZE; ++index)
		if (name[index] != 'a')
			return false;

	strcpy(name, "  the WANDERER  ");
	if (yt_planet_rename_prepare_name(name, &length)
	    != YT_PLANET_RENAME_RESERVED || length != 12U
	    || strcmp(name, "The Wanderer") != 0)
		return false;
	strcpy(name, "The Wanderer II");
	if (yt_planet_rename_prepare_name(name, &length)
	    != YT_PLANET_RENAME_ACCEPTED || length != 15U)
		return false;
	strcpy(name, "       ");
	if (yt_planet_rename_prepare_name(name, &length)
	    != YT_PLANET_RENAME_EMPTY || length != 0U || name[0] != '\0')
		return false;

	for (index = 0; index < sizeof(source.bytes); ++index)
		source.bytes[index] = (uint8_t)(index * 37U + 11U);
	yt_planet_decode(&planet, &source);
	yt_planet_rename_overlay(&planet, "Nova", 4U);
	yt_planet_encode(&planet);
	expected = source;
	yt_record_set_text(&expected, (const uint8_t *)"Nova", 4U);
	if (!yt_record_set_number(&expected, YT_F85, 4.0f)
	    || memcmp(&planet.record, &expected, sizeof(expected)) != 0
	    || strcmp(planet.name, "Nova") != 0
	    || planet.name_length != 4.0f)
		return false;
	return true;
}

static bool
check_planet_take_all_overlays(void)
{
	struct yt_player player;
	struct yt_planet planet;
	double quantity[10] = {0};
	double amount[10];
	float commodity;
	static const int take_one_items[7] = {1, 2, 3, 4, 5, 6, 9};
	static const char *const take_one_titles[7] = {
		"<Take Ore>", "<Take Organics)", "<Take Equipment)",
		"<Take Fighters)", "<Take Missiles)", "<Take Mines)",
		"<Take Plasma Bolts>"
	};
	size_t index;

	for (index = 0; index < YT_ARRAY_LEN(take_one_items); ++index)
		if (strcmp(yt_planet_take_one_title(take_one_items[index]),
		    take_one_titles[index]) != 0)
			return false;
	if (yt_planet_take_one_title(0) != NULL
	    || yt_planet_take_one_title(7) != NULL)
		return false;

	memset(&player, 0, sizeof(player));
	memset(&planet, 0, sizeof(planet));
	player.ore = 40.0f;
	planet.stock[0] = 999.0f;
	yt_planet_take_one_player_overlay(&player, 1, 3.0f);
	yt_planet_take_one_planet_overlay(&planet, 1, 101.75, 3.0f);
	if (player.ore != 43.0f || planet.stock[0] != 98.75f)
		return false;
	player.ore = 10.0f;
	planet.stock[0] = 999.0f;
	yt_planet_take_one_player_overlay(&player, 1, 65.0f);
	yt_planet_take_one_planet_overlay(&planet, 1, 101.0, 65.0f);
	if (player.ore != 75.0f || planet.stock[0] != 36.0f)
		return false;
	player.ore = 10.0f;
	planet.stock[0] = 999.0f;
	yt_planet_take_one_player_overlay(&player, 1, 0.0f);
	yt_planet_take_one_planet_overlay(&planet, 1, 101.0, 0.0f);
	if (player.ore != 10.0f || planet.stock[0] != 101.0f)
		return false;
	player.plasma = 4.0f;
	planet.plasma = 999.0f;
	yt_planet_take_one_player_overlay(&player, 9, 3.0f);
	yt_planet_take_one_planet_overlay(&planet, 9, 9.5, 3.0f);
	if (player.plasma != 7.0f || planet.plasma != 6.5f)
		return false;
	memset(&player, 0, sizeof(player));
	memset(&planet, 0, sizeof(planet));
	player.ore = player.organics = player.equipment = 1.0f;
	player.fighters = player.missiles = player.mines = player.plasma = 1.0f;
	yt_planet_take_one_player_overlay(&player, 1, 1.0f);
	yt_planet_take_one_player_overlay(&player, 2, 1.0f);
	yt_planet_take_one_player_overlay(&player, 3, 1.0f);
	yt_planet_take_one_player_overlay(&player, 4, 1.0f);
	yt_planet_take_one_player_overlay(&player, 5, 1.0f);
	yt_planet_take_one_player_overlay(&player, 6, 1.0f);
	yt_planet_take_one_player_overlay(&player, 9, 1.0f);
	yt_planet_take_one_planet_overlay(&planet, 1, 2.5, 1.0f);
	yt_planet_take_one_planet_overlay(&planet, 2, 2.5, 1.0f);
	yt_planet_take_one_planet_overlay(&planet, 3, 2.5, 1.0f);
	yt_planet_take_one_planet_overlay(&planet, 4, 2.5, 1.0f);
	yt_planet_take_one_planet_overlay(&planet, 5, 2.5, 1.0f);
	yt_planet_take_one_planet_overlay(&planet, 6, 2.5, 1.0f);
	yt_planet_take_one_planet_overlay(&planet, 9, 2.5, 1.0f);
	if (player.ore != 2.0f || player.organics != 2.0f
	    || player.equipment != 2.0f || player.fighters != 2.0f
	    || player.missiles != 2.0f || player.mines != 2.0f
	    || player.plasma != 2.0f || planet.stock[0] != 1.5f
	    || planet.stock[1] != 1.5f || planet.stock[2] != 1.5f
	    || planet.fighters != 1.5f || planet.missiles != 1.5f
	    || planet.mines != 1.5f || planet.plasma != 1.5f)
		return false;

	memset(&player, 0, sizeof(player));
	player.holds = 100.0f;
	player.ore = 10.0f;
	player.organics = 20.0f;
	player.equipment = 5.0f;
	player.fighters = 7.0f;
	player.missiles = 2.0f;
	player.mines = 3.0f;
	player.plasma = 4.0f;
	quantity[1] = 101.0;
	quantity[2] = 202.0;
	quantity[3] = 303.0;
	quantity[4] = 404.0;
	quantity[5] = 5.0;
	quantity[6] = 6.0;
	quantity[9] = 9.0;
	yt_planet_take_all_weapon_player_overlay(&player, quantity, amount);
	if (amount[4] != 404.0 || amount[5] != 5.0 || amount[6] != 6.0
	    || amount[9] != 9.0 || player.fighters != 411.0f
	    || player.missiles != 7.0f || player.mines != 9.0f
	    || player.plasma != 13.0f)
		return false;
	memset(&planet, 0, sizeof(planet));
	planet.fighters = planet.missiles = planet.mines = planet.plasma = 999.0f;
	yt_planet_take_all_weapon_planet_overlay(&planet, quantity, amount);
	if (planet.fighters != 0.0f || planet.missiles != 0.0f
	    || planet.mines != 0.0f || planet.plasma != 0.0f)
		return false;
	commodity = yt_planet_take_all_commodity_player_overlay(&player, 3,
	    quantity[3]);
	yt_planet_take_all_commodity_planet_overlay(&planet, 3, quantity[3],
	    commodity);
	if (commodity != 65.0f || player.equipment != 70.0f
	    || planet.stock[2] != 238.0f)
		return false;
	commodity = yt_planet_take_all_commodity_player_overlay(&player, 2,
	    quantity[2]);
	if (commodity != 0.0f || player.organics != 20.0f)
		return false;
	commodity = yt_planet_take_all_commodity_player_overlay(&player, 1,
	    quantity[1]);
	if (commodity != 0.0f || player.ore != 10.0f)
		return false;

	memset(&player, 0, sizeof(player));
	memset(&planet, 0, sizeof(planet));
	player.holds = 10.0f;
	player.ore = player.organics = player.equipment = 8.0f;
	player.fighters = 7.0f;
	player.missiles = 2.0f;
	player.mines = 3.0f;
	player.plasma = 4.0f;
	quantity[3] = 5.0;
	quantity[4] = -2.0;
	quantity[5] = -3.0;
	quantity[6] = -4.0;
	quantity[9] = -5.0;
	yt_planet_take_all_weapon_player_overlay(&player, quantity, amount);
	commodity = yt_planet_take_all_commodity_player_overlay(&player, 3,
	    quantity[3]);
	yt_planet_take_all_commodity_planet_overlay(&planet, 3, quantity[3],
	    commodity);
	if (amount[4] != -2.0 || player.fighters != 5.0f
	    || commodity != -14.0f || player.equipment != -6.0f
	    || planet.stock[2] != 19.0f)
		return false;

	memset(&player, 0, sizeof(player));
	memset(&planet, 0, sizeof(planet));
	player.ore = ldexpf(1.0f, -57);
	quantity[3] = 1.0 + ldexp(1.0, -24);
	commodity = yt_planet_take_all_commodity_player_overlay(&player, 3,
	    quantity[3]);
	yt_planet_take_all_commodity_planet_overlay(&planet, 3, quantity[3],
	    commodity);
	if (planet.stock[2] != 1.0f)
		return false;

	{
		float rate[10] = {0};
		float contribution[10] = {0};
		double cargo_quantity[10] = {0};
		const double held[3] = {3.75, 0.0, -1.0};

		rate[1] = 10.0f;
		rate[2] = 30.0f;
		rate[3] = 40.0f;
		contribution[1] = 1.0f;
		contribution[2] = 2.0f;
		contribution[3] = 3.0f;
		cargo_quantity[1] = 101.25;
		cargo_quantity[2] = 60.0;
		cargo_quantity[3] = 1.0;
		yt_planet_transfer_cargo_cache(rate, cargo_quantity, held);
		if (rate[1] != 11.5f || rate[2] != 30.0f
		    || rate[3] != 40.0f || cargo_quantity[1] != 105.0
		    || cargo_quantity[2] != 60.0 || cargo_quantity[3] != 0.0)
			return false;
		memset(&player, 0, sizeof(player));
		player.ore = 11.0f;
		player.organics = 22.0f;
		player.equipment = 5.0f;
		player.credits = 777.0f;
		yt_planet_transfer_cargo_player_overlay(&player);
		if (player.ore != 0.0f || player.organics != 0.0f
		    || player.equipment != 0.0f || player.credits != 777.0f)
			return false;
		memset(&planet, 0, sizeof(planet));
		planet.owner = 23.0f;
		yt_planet_transfer_cargo_planet_overlay(&planet, rate,
		    cargo_quantity, contribution);
		if (planet.production[0] != 10.5f
		    || planet.production[1] != 28.0f
		    || planet.production[2] != 37.0f
		    || planet.stock[0] != 105.0f || planet.stock[1] != 60.0f
		    || planet.stock[2] != 0.0f || planet.owner != 23.0f)
			return false;
	}

	memset(&player, 0, sizeof(player));
	memset(&planet, 0, sizeof(planet));
	player.plasma = 99.0f;
	player.credits = 777.0f;
	planet.plasma = 100.0f;
	planet.owner = 23.0f;
	yt_planet_transfer_direct_player_overlay(&player, 9);
	yt_planet_transfer_direct_planet_overlay(&planet, 9, 10.25, 1.5f);
	if (player.plasma != 0.0f || player.credits != 777.0f
	    || planet.plasma != 11.75f || planet.owner != 23.0f)
		return false;
	yt_planet_transfer_direct_player_overlay(&player, 5);
	yt_planet_transfer_direct_planet_overlay(&planet, 5, 5.0, 2.0f);
	yt_planet_transfer_direct_player_overlay(&player, 6);
	yt_planet_transfer_direct_planet_overlay(&planet, 6, 6.0, 3.0f);
	if (player.missiles != 0.0f || player.mines != 0.0f
	    || planet.missiles != 7.0f || planet.mines != 9.0f)
		return false;
	player.fighters = 99.0f;
	planet.fighters = 999.0f;
	yt_planet_transfer_fighter_player_overlay(&player, 7.0f, 1.5f);
	yt_planet_transfer_fighter_planet_overlay(&planet, 404.0, 1.5f);
	if (player.fighters != 5.5f || player.credits != 777.0f
	    || planet.fighters != 405.5f || planet.owner != 23.0f)
		return false;
	{
		const double empty[3] = {0.0, -0.0, 0.0};
		const double nonempty[3] = {-1.0, 0.0, 0.0};

		return yt_planet_transfer_selector_position("") == 1
		    && yt_planet_transfer_selector_position("C") == 1
		    && yt_planet_transfer_selector_position("S") == 2
		    && yt_planet_transfer_selector_position("F") == 3
		    && yt_planet_transfer_selector_position("M") == 4
		    && yt_planet_transfer_selector_position("B") == 5
		    && yt_planet_transfer_selector_position("SF") == 2
		    && yt_planet_transfer_selector_position("X") == 0
		    && yt_planet_transfer_selector_position("SCX") == 0
		    && yt_planet_transfer_cargo_empty(empty)
		    && !yt_planet_transfer_cargo_empty(nonempty)
		    && !yt_planet_transfer_fighter_rejected(0.0f, 7.0f)
		    && !yt_planet_transfer_fighter_rejected(0.5f, 7.0f)
		    && !yt_planet_transfer_fighter_rejected(7.0f, 7.0f)
		    && yt_planet_transfer_fighter_rejected(-1.0f, 7.0f)
		    && yt_planet_transfer_fighter_rejected(8.0f, 7.0f);
	}
}

static bool
check_planet_bank_overlays(void)
{
	struct yt_player player;
	struct yt_planet planet;
	float argument;

	if (yt_planet_bank_available(12345.0f, 1000.0f) != 13345.0
	    || yt_planet_bank_remaining(12345.0f, 1000.0f, 1500.0)
	    != 11845.0)
		return false;
	memset(&planet, 0, sizeof(planet));
	planet.bank = 777.0f;
	planet.mines = 91.0f;
	yt_planet_bank_planet_overlay(&planet, 1000.0);
	if (planet.bank != 1000.0f || planet.mines != 91.0f)
		return false;
	argument = yt_planet_bank_credit_argument(2000.0f, 1000.0);
	memset(&player, 0, sizeof(player));
	player.credits = 7000.0f;
	player.mines = 44.0f;
	yt_planet_bank_credit_overlay(&player, argument);
	if (argument != 1000.0f || player.credits != 8000.0f
	    || player.mines != 44.0f)
		return false;
	if (yt_planet_bank_available(16777216.0f, 1.0f) != 16777217.0
	    || yt_planet_bank_remaining(16777216.0f, 1.0f, 16777217.0)
	    != 0.0)
		return false;
	yt_planet_bank_planet_overlay(&planet, 16777217.0);
	argument = yt_planet_bank_credit_argument(1.0f, 16777217.0);
	if (planet.bank != 16777216.0f || argument != -16777216.0f)
		return false;
	player.credits = 16777216.0f;
	yt_planet_bank_credit_overlay(&player, 1.0f);
	return player.credits == 16777216.0f;
}

static bool
check_planet_menu_selector(void)
{
	static const char selector[] = "F!MPC1234569LTAB$";
	char thrusters[sizeof(selector)];
	char movement[sizeof(selector)];
	char port[sizeof(selector)];
	size_t index;

	for (index = 0U; index < sizeof(selector) - 1U; ++index) {
		char command[2] = {selector[index], '\0'};

		if (yt_planet_menu_selector_position(command)
		    != (int)index + 1)
			return false;
	}
	for (index = 1U; index < sizeof(selector) - 1U; ++index) {
		memcpy(thrusters, selector + 1U, index);
		thrusters[index] = '\0';
		if (yt_planet_menu_selector_position(thrusters) != 2)
			return false;
	}
	for (index = 1U; index < sizeof(selector) - 2U; ++index) {
		memcpy(movement, selector + 2U, index);
		movement[index] = '\0';
		if (yt_planet_menu_selector_position(movement) != 3)
			return false;
	}
	for (index = 1U; index < sizeof(selector) - 3U; ++index) {
		memcpy(port, selector + 3U, index);
		port[index] = '\0';
		if (yt_planet_menu_selector_position(port) != 4)
			return false;
	}
	return yt_planet_menu_selector_position("") == 1
	    && yt_planet_menu_selector_position("L") == 13
	    && yt_planet_menu_selector_position("LT") == 13
	    && yt_planet_menu_selector_position("LTA") == 13
	    && yt_planet_menu_selector_position("LTAB") == 13
	    && yt_planet_menu_selector_position("LTAB$") == 13
	    && yt_planet_menu_selector_position("9L") == 12
	    && yt_planet_menu_selector_position("AL") == 0
	    && yt_planet_menu_selector_position("LL") == 0
	    && yt_planet_menu_selector_position("Lx") == 0
	    && yt_planet_menu_selector_position("l") == 0
	    && yt_planet_menu_selector_position(NULL) == 0;
}

enum credit_mutation_event {
	CREDIT_MUTATION_READ = 1,
	CREDIT_MUTATION_WRITE,
};

struct credit_mutation_tape {
	struct yt_player fresh;
	struct yt_record persistent;
	enum credit_mutation_event events[2];
	size_t event_count;
	bool fail_read;
	bool fail_write;
};

static bool
credit_mutation_read(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct credit_mutation_tape *tape = context;

	if (tape->event_count >= YT_ARRAY_LEN(tape->events)
	    || player_record != 2)
		return false;
	tape->events[tape->event_count++] = CREDIT_MUTATION_READ;
	if (tape->fail_read)
		return planet_permission_fixture_error(error,
		    "credit mutation read");
	*player = tape->fresh;
	return true;
}

static bool
credit_mutation_write(void *context, int player_record,
    const struct yt_record *record, struct yt_error *error)
{
	struct credit_mutation_tape *tape = context;

	if (tape->event_count >= YT_ARRAY_LEN(tape->events)
	    || player_record != 2 || record == NULL)
		return false;
	tape->events[tape->event_count++] = CREDIT_MUTATION_WRITE;
	if (tape->fail_write)
		return planet_permission_fixture_error(error,
		    "credit mutation write");
	tape->persistent = *record;
	return true;
}

static void
credit_mutation_initialize(struct yt_credit_mutation_state *state,
    struct yt_player *player, float *current_sector, float sector_cache[52],
    float cloak_cache[52])
{
	memset(state, 0, sizeof(*state));
	state->hydration.player = player;
	state->hydration.player_record = 2;
	state->hydration.last_player_record = 51;
	state->hydration.sector_record_offset = 100.0f;
	state->hydration.current_sector_record = current_sector;
	state->hydration.sector_cache = sector_cache;
	state->hydration.cloak_cache = cloak_cache;
	state->hydration.cache_count = 52U;
	state->argument = -1.25f;
}

static bool
check_credit_mutation_transaction(void)
{
	static const struct yt_credit_mutation_ops ops = {
		credit_mutation_read,
		credit_mutation_write,
	};
	struct yt_credit_mutation_state state;
	struct credit_mutation_tape tape;
	struct yt_player player;
	struct yt_record source_record;
	struct yt_record boundary_record;
	struct yt_record player_before;
	struct yt_error error;
	float sector_cache[52];
	float cloak_cache[52];
	float current_sector;
	uint8_t expected_argument[4];
	uint8_t expected_sum[4];
	uint8_t expected_result[4];
	size_t index;

	memset(&tape, 0, sizeof(tape));
	for (index = 0U; index < YT_RECORD_SIZE; ++index)
		source_record.bytes[index] = (uint8_t)(index * 13U + 7U);
	(void)yt_record_set_number(&source_record, YT_F57, 9.0f);
	(void)yt_record_set_number(&source_record, YT_F81, 100.75f);
	(void)yt_record_set_number(&source_record, YT_F125, 0.75f);
	yt_player_decode(&tape.fresh, &source_record);
	memset(&player, 0xa5, sizeof(player));
	for (index = 0U; index < YT_ARRAY_LEN(sector_cache); ++index) {
		sector_cache[index] = -9.0f;
		cloak_cache[index] = -8.0f;
	}
	current_sector = -7.0f;
	credit_mutation_initialize(&state, &player, &current_sector,
	    sector_cache, cloak_cache);
	yt_error_clear(&error);
	if (!yt_credit_mutation_run(&state, &ops, &tape, &error)
	    || tape.event_count != 2U
	    || tape.events[0] != CREDIT_MUTATION_READ
	    || tape.events[1] != CREDIT_MUTATION_WRITE
	    || !state.hydrated || !state.overlay_applied
	    || !state.write_attempted || !state.written
	    || state.fresh_credits != 100.75f
	    || state.summed_credits != 99.5f
	    || state.result_credits != 99.0f || player.credits != 99.0f
	    || current_sector != 109.0f || sector_cache[2] != 9.0f
	    || cloak_cache[2] != 0.75f
	    || yt_record_get_number(&tape.persistent, YT_F81) != 99.0f
	    || memcmp(&tape.persistent, &player.record,
	    sizeof(tape.persistent)) != 0
	    || qb_mbf32_encode(-1.25f, expected_argument) != QB_MBF_OK
	    || qb_mbf32_encode(99.5f, expected_sum) != QB_MBF_OK
	    || qb_mbf32_encode(99.0f, expected_result) != QB_MBF_OK
	    || memcmp(state.argument_raw, expected_argument, 4U) != 0
	    || memcmp(state.fresh_credits_raw,
	    source_record.bytes + YT_F81, 4U) != 0
	    || memcmp(state.summed_credits_raw, expected_sum, 4U) != 0
	    || memcmp(state.result_credits_raw, expected_result, 4U) != 0)
		return false;
	for (index = 0U; index < YT_RECORD_SIZE; ++index) {
		if ((index < YT_F81 || index >= YT_F81 + 4U)
		    && tape.persistent.bytes[index] != source_record.bytes[index])
			return false;
	}

	/* MBF has only one zero; a negative IEEE zero persists and hydrates as +0. */
	memset(&tape, 0, sizeof(tape));
	boundary_record = source_record;
	(void)yt_record_set_number(&boundary_record, YT_F81, 0.0f);
	yt_player_decode(&tape.fresh, &boundary_record);
	memset(&player, 0, sizeof(player));
	current_sector = -7.0f;
	credit_mutation_initialize(&state, &player, &current_sector,
	    sector_cache, cloak_cache);
	state.argument = -0.0f;
	yt_error_clear(&error);
	if (!yt_credit_mutation_run(&state, &ops, &tape, &error)
	    || tape.event_count != 2U || signbit(player.credits)
	    || memcmp(player.record.bytes + YT_F81,
	    (const uint8_t[4]){0U, 0U, 0U, 0U}, 4U) != 0)
		return false;

	/* A representable MBF argument may overflow the SINGLE sum after GET. */
	memset(&tape, 0, sizeof(tape));
	boundary_record = source_record;
	(void)yt_record_set_raw_number(&boundary_record, YT_F81,
	    (const uint8_t[4]){0xffU, 0xffU, 0x7fU, 0xffU});
	yt_player_decode(&tape.fresh, &boundary_record);
	memset(&player, 0, sizeof(player));
	current_sector = -7.0f;
	credit_mutation_initialize(&state, &player, &current_sector,
	    sector_cache, cloak_cache);
	state.argument = tape.fresh.credits;
	yt_error_clear(&error);
	if (yt_credit_mutation_run(&state, &ops, &tape, &error)
	    || tape.event_count != 1U || !state.hydrated
	    || state.overlay_applied || state.write_attempted || state.written
	    || strcmp(error.operation, "credit mutation result MBF32") != 0)
		return false;

	/* An unrepresentable argument fails before the helper performs its GET. */
	memset(&tape, 0, sizeof(tape));
	memset(&player, 0, sizeof(player));
	current_sector = -7.0f;
	credit_mutation_initialize(&state, &player, &current_sector,
	    sector_cache, cloak_cache);
	state.argument = NAN;
	yt_error_clear(&error);
	if (yt_credit_mutation_run(&state, &ops, &tape, &error)
	    || tape.event_count != 0U || state.hydrated
	    || strcmp(error.operation, "credit mutation argument MBF32") != 0)
		return false;

	memset(&tape, 0, sizeof(tape));
	yt_player_decode(&tape.fresh, &source_record);
	tape.fail_read = true;
	memset(&player, 0x5a, sizeof(player));
	player_before = player.record;
	current_sector = -7.0f;
	sector_cache[2] = -9.0f;
	cloak_cache[2] = -8.0f;
	credit_mutation_initialize(&state, &player, &current_sector,
	    sector_cache, cloak_cache);
	yt_error_clear(&error);
	if (yt_credit_mutation_run(&state, &ops, &tape, &error)
	    || tape.event_count != 1U
	    || tape.events[0] != CREDIT_MUTATION_READ || state.hydrated
	    || state.overlay_applied || state.write_attempted || state.written
	    || memcmp(&player.record, &player_before, sizeof(player_before)) != 0
	    || current_sector != -7.0f || sector_cache[2] != -9.0f
	    || cloak_cache[2] != -8.0f
	    || strcmp(error.operation, "credit mutation read") != 0)
		return false;

	memset(&tape, 0, sizeof(tape));
	yt_player_decode(&tape.fresh, &source_record);
	tape.persistent = source_record;
	tape.fail_write = true;
	memset(&player, 0, sizeof(player));
	current_sector = -7.0f;
	credit_mutation_initialize(&state, &player, &current_sector,
	    sector_cache, cloak_cache);
	yt_error_clear(&error);
	if (yt_credit_mutation_run(&state, &ops, &tape, &error)
	    || tape.event_count != 2U
	    || tape.events[0] != CREDIT_MUTATION_READ
	    || tape.events[1] != CREDIT_MUTATION_WRITE
	    || !state.hydrated || !state.overlay_applied
	    || !state.write_attempted || state.written
	    || player.credits != 99.0f
	    || yt_record_get_number(&player.record, YT_F81) != 99.0f
	    || memcmp(&tape.persistent, &source_record,
	    sizeof(source_record)) != 0
	    || strcmp(error.operation, "credit mutation write") != 0)
		return false;
	return true;
}

static bool
check_planet_productivity_overlays(void)
{
	struct yt_player player;
	struct yt_planet planet;
	float rate[10] = {0};
	float contribution[10] = {0};
	double quantity[10] = {0};
	float delta[4];
	double units;
	float argument;

	units = yt_planet_productivity_units(250.0);
	rate[1] = 100.0f;
	rate[2] = 200.0f;
	rate[3] = 300.0f;
	yt_planet_productivity_cache(rate, units, delta);
	if (units != 1.0 || rate[1] != 101.0f || rate[2] != 201.0f
	    || rate[3] != 301.0f || delta[0] != 3.0f
	    || delta[1] != 0.0f || delta[2] != 0.0f || delta[3] != 0.0f)
		return false;
	rate[1] = rate[2] = rate[3] = 83333.0f;
	yt_planet_productivity_cache(rate, 1.0, delta);
	if (delta[0] != 3.0f || delta[1] != 1.0f
	    || delta[2] != 1.0f || delta[3] != 1.0f)
		return false;
	memset(&planet, 0, sizeof(planet));
	planet.owner = 23.0f;
	contribution[1] = 1.0f;
	contribution[2] = 2.0f;
	contribution[3] = 3.0f;
	quantity[1] = 11.0;
	quantity[2] = 22.0;
	quantity[3] = 33.0;
	yt_planet_productivity_planet_overlay(&planet, rate, quantity,
	    contribution);
	if (planet.production[0] != 83333.0f
	    || planet.production[1] != 83332.0f
	    || planet.production[2] != 83331.0f
	    || planet.stock[0] != 11.0f || planet.stock[1] != 22.0f
	    || planet.stock[2] != 33.0f || planet.owner != 23.0f)
		return false;
	units = yt_planet_productivity_units(16777217.0);
	argument = yt_planet_productivity_credit_argument(units);
	memset(&player, 0, sizeof(player));
	player.credits = 16777218.0f;
	yt_planet_bank_credit_overlay(&player, argument);
	return argument == -16777216.0f && player.credits == 2.0f;
}

static bool
check_clearance_model(void)
{
	float value;

	if (yt_clearance_candidate_needed(0, 0.7900000214576721f,
	    0.0f, true)
	    || !yt_clearance_candidate_needed(0, 0.8f, 0.0f, true)
	    || yt_clearance_candidate_needed(0, 0.8f, 0.0f, false)
	    || yt_clearance_candidate_needed(0, 0.8f, 0.5f, true)
	    || yt_clearance_candidate_needed(4, 1.0f, 0.0f, true))
		return false;
	value = 0.099f;
	if (yt_clearance_normalize(0, &value) || value != 0.0f)
		return false;
	value = 0.10000000149011612f;
	if (!yt_clearance_normalize(0, &value))
		return false;
	value = 0.9509999752044678f;
	if (!yt_clearance_normalize(0, &value))
		return false;
	value = 0.9511f;
	if (yt_clearance_normalize(0, &value) || value != 0.0f)
		return false;
	value = 0.800000011920929f;
	if (!yt_clearance_normalize(2, &value))
		return false;
	return yt_clearance_percentage(0.10000000149011612f) == 10.0f;
}

enum anti_cloak_event {
	ANTI_CLOAK_PRESENT = 1,
	ANTI_CLOAK_SOUND,
	ANTI_CLOAK_READ,
	ANTI_CLOAK_WRITE,
};

struct anti_cloak_tape {
	int events[24];
	size_t event_count;
	size_t fail_at;
	struct yt_player players[4];
	float read_records[4];
	size_t read_position;
	float write_record;
	struct yt_player written;
	uint8_t rows[10][128];
	size_t row_lengths[10];
	float row_foregrounds[10];
	bool row_bold[10];
	size_t row_count;
	float sounds[4];
	size_t sound_count;
};

static bool
anti_cloak_step(struct anti_cloak_tape *tape, enum anti_cloak_event event)
{
	if (tape->event_count >= YT_ARRAY_LEN(tape->events))
		return false;
	tape->events[tape->event_count++] = (int)event;
	return tape->event_count != tape->fail_at;
}

static bool
anti_cloak_read(void *context, float record, struct yt_player *player,
    struct yt_error *error)
{
	struct anti_cloak_tape *tape = context;
	size_t position = tape->read_position;

	(void)error;
	if (position >= YT_ARRAY_LEN(tape->players)
	    || !anti_cloak_step(tape, ANTI_CLOAK_READ))
		return false;
	tape->read_records[position] = record;
	*player = tape->players[position];
	tape->read_position++;
	return true;
}

static bool
anti_cloak_write(struct anti_cloak_tape *tape, float record,
    const struct yt_player *player, struct yt_error *error)
{
	(void)error;
	if (!anti_cloak_step(tape, ANTI_CLOAK_WRITE))
		return false;
	tape->write_record = record;
	tape->written = *player;
	return true;
}

static bool
anti_cloak_mutate_credits(void *context, float record, float argument,
    struct yt_player *player, bool *hydrated, struct yt_error *error)
{
	struct anti_cloak_tape *tape = context;

	if (hydrated != NULL)
		*hydrated = false;
	if (!anti_cloak_read(context, record, player, error))
		return false;
	if (hydrated != NULL)
		*hydrated = true;
	yt_planet_bank_credit_overlay(player, argument);
	if (!yt_record_set_number(&player->record, YT_F81, player->credits))
		return false;
	return anti_cloak_write(tape, record, player, error);
}

static bool
anti_cloak_present(void *context, const uint8_t *text, size_t length,
    float foreground, bool bold, struct yt_error *error)
{
	struct anti_cloak_tape *tape = context;
	size_t position = tape->row_count;

	(void)error;
	if (position >= YT_ARRAY_LEN(tape->rows)
	    || length > sizeof(tape->rows[position])
	    || !anti_cloak_step(tape, ANTI_CLOAK_PRESENT))
		return false;
	if (length != 0U)
		memcpy(tape->rows[position], text, length);
	tape->row_lengths[position] = length;
	tape->row_foregrounds[position] = foreground;
	tape->row_bold[position] = bold;
	tape->row_count++;
	return true;
}

static bool
anti_cloak_sound(void *context, float selector, struct yt_error *error)
{
	struct anti_cloak_tape *tape = context;

	(void)error;
	if (tape->sound_count >= YT_ARRAY_LEN(tape->sounds)
	    || !anti_cloak_step(tape, ANTI_CLOAK_SOUND))
		return false;
	tape->sounds[tape->sound_count++] = selector;
	return true;
}

static void
anti_cloak_fixture(struct anti_cloak_tape *tape,
    struct yt_earth_anti_cloak_state *state, float cache[8])
{
	static const uint8_t target_name[] = {'A', 0, 'B', 'C'};

	memset(tape, 0, sizeof(*tape));
	memset(state, 0, sizeof(*state));
	memset(cache, 0, 8U * sizeof(cache[0]));
	tape->fail_at = SIZE_MAX;
	memcpy(tape->players[0].record.bytes, target_name,
	    sizeof(target_name));
	tape->players[0].name_length = 3.5f;
	tape->players[0].killed_by = 0.0f;
	tape->players[1].killed_by = -1.0f;
	memset(tape->players[2].record.bytes, 0xa5,
	    sizeof(tape->players[2].record.bytes));
	tape->players[2].credits = 100.75f;
	(void)yt_record_set_number(&tape->players[2].record, YT_F81, 100.75f);
	cache[2] = 1.0f;
	cache[3] = -1.0f;
	cache[4] = 0.5f;
	state->price = 10.25f;
	state->current_record = 7.0f;
	state->player_terminal = 4.0f;
	state->conversion_mode = 4U;
	state->cloak_cache = cache;
	state->cloak_cache_count = 8U;
	state->foreground = 4.0f;
}

static bool
check_earth_anti_cloak_transaction(void)
{
	static const struct yt_earth_anti_cloak_ops ops = {
		anti_cloak_read,
		anti_cloak_mutate_credits,
		anti_cloak_present,
		anti_cloak_sound,
	};
	static const int reported_events[] = {
		ANTI_CLOAK_PRESENT, ANTI_CLOAK_PRESENT,
		ANTI_CLOAK_PRESENT, ANTI_CLOAK_PRESENT,
		ANTI_CLOAK_SOUND, ANTI_CLOAK_READ,
		ANTI_CLOAK_PRESENT, ANTI_CLOAK_SOUND,
		ANTI_CLOAK_READ, ANTI_CLOAK_PRESENT,
		ANTI_CLOAK_PRESENT, ANTI_CLOAK_SOUND,
		ANTI_CLOAK_READ, ANTI_CLOAK_WRITE,
	};
	static const int none_events[] = {
		ANTI_CLOAK_PRESENT, ANTI_CLOAK_PRESENT,
		ANTI_CLOAK_PRESENT, ANTI_CLOAK_PRESENT,
		ANTI_CLOAK_SOUND, ANTI_CLOAK_PRESENT,
		ANTI_CLOAK_PRESENT, ANTI_CLOAK_PRESENT,
		ANTI_CLOAK_PRESENT, ANTI_CLOAK_SOUND,
		ANTI_CLOAK_READ, ANTI_CLOAK_WRITE,
	};
	static const uint8_t activation[] =
	    "ti-Cloaking device activated!\xd4" "D";
	static const uint8_t waves[] =
	    "Waves of electromagnetic disruption flood the galaxy..."
	    "\xd4\x0e\x00\x86\xc1" " is uncl";
	static const uint8_t target_row[] =
	    {'A', 0, 'B', ' ', 'i', 's', ' ', 'u', 'n', 'c', 'l', 'o', 'a',
	     'k', 'e', 'd', '!'};
	static const uint8_t none[] = "Too bad noone was cloaked anyhow!";
	static const uint8_t fade[] = "...the effect fades.";
	struct anti_cloak_tape tape;
	struct yt_earth_anti_cloak_state state;
	struct yt_record original;
	float cache[8];
	size_t failure;
	size_t index;

	anti_cloak_fixture(&tape, &state, cache);
	original = tape.players[2].record;
	if (!yt_earth_anti_cloak_run(&state, &ops, &tape, NULL)
	    || tape.event_count != YT_ARRAY_LEN(reported_events)
	    || memcmp(tape.events, reported_events,
	    sizeof(reported_events)) != 0
	    || tape.read_position != 3U || tape.read_records[0] != 2.0f
	    || tape.read_records[1] != 4.0f
	    || tape.read_records[2] != 7.0f || tape.write_record != 7.0f
	    || cache[2] != 0.0f || cache[3] != -1.0f || cache[4] != 0.0f
	    || !state.reported || state.counter != 5.0f
	    || state.foreground != 3.0f || state.field_record != 7.0f
	    || state.credit_argument != -10.25f || !state.credit_loaded
	    || tape.written.credits != 90.0f
	    || yt_record_get_number(&tape.written.record, YT_F81) != 90.0f
	    || tape.row_count != 7U || tape.sound_count != 3U
	    || tape.sounds[0] != 2.0f || tape.sounds[1] != 1.0f
	    || tape.sounds[2] != 5.0f
	    || tape.row_lengths[0] != sizeof(activation) - 1U
	    || memcmp(tape.rows[0], activation, sizeof(activation) - 1U) != 0
	    || tape.row_foregrounds[0] != 4.0f || tape.row_bold[0]
	    || tape.row_lengths[1] != 0U
	    || tape.row_lengths[2] != sizeof(waves) - 1U
	    || memcmp(tape.rows[2], waves, sizeof(waves) - 1U) != 0
	    || tape.row_foregrounds[2] != 2.0f || !tape.row_bold[2]
	    || tape.row_lengths[4] != sizeof(target_row)
	    || memcmp(tape.rows[4], target_row, sizeof(target_row)) != 0
	    || tape.row_foregrounds[4] != 6.0f || !tape.row_bold[4]
	    || tape.row_lengths[5] != 0U
	    || tape.row_lengths[6] != sizeof(fade) - 1U
	    || memcmp(tape.rows[6], fade, sizeof(fade) - 1U) != 0
	    || tape.row_foregrounds[6] != 2.0f || !tape.row_bold[6])
		return false;
	for (index = 0U; index < YT_RECORD_SIZE; ++index) {
		if (index >= YT_F81 && index < YT_F81 + 4U)
			continue;
		if (tape.written.record.bytes[index] != original.bytes[index])
			return false;
	}

	anti_cloak_fixture(&tape, &state, cache);
	state.player_terminal = 1.5f;
	if (!yt_earth_anti_cloak_run(&state, &ops, &tape, NULL)
	    || tape.event_count != YT_ARRAY_LEN(none_events)
	    || memcmp(tape.events, none_events, sizeof(none_events)) != 0
	    || tape.read_position != 1U || tape.read_records[0] != 7.0f
	    || state.reported || state.counter != 2.0f
	    || tape.row_count != 8U
	    || tape.row_lengths[4] != 0U
	    || tape.row_lengths[5] != sizeof(none) - 1U
	    || memcmp(tape.rows[5], none, sizeof(none) - 1U) != 0)
		return false;

	anti_cloak_fixture(&tape, &state, cache);
	state.player_terminal = 2.0f;
	cache[2] = NAN;
	if (!yt_earth_anti_cloak_run(&state, &ops, &tape, NULL)
	    || tape.read_position != 1U || tape.read_records[0] != 7.0f
	    || !isnan(cache[2]) || state.reported)
		return false;

	anti_cloak_fixture(&tape, &state, cache);
	state.player_terminal = 2.5f;
	tape.players[1] = tape.players[2];
	if (!yt_earth_anti_cloak_run(&state, &ops, &tape, NULL)
	    || state.counter != 3.0f || tape.read_position != 2U
	    || tape.read_records[0] != 2.0f || tape.read_records[1] != 7.0f)
		return false;

	anti_cloak_fixture(&tape, &state, cache);
	state.player_terminal = 2.0f;
	tape.players[0].name_length = -1.0f;
	if (yt_earth_anti_cloak_run(&state, &ops, &tape, NULL)
	    || tape.event_count != 6U || tape.events[5] != ANTI_CLOAK_READ
	    || cache[2] != 0.0f || state.field_record != 2.0f
	    || state.counter != 2.0f || state.foreground != 6.0f
	    || state.credit_loaded
	    || memcmp(&state.field_player, &tape.players[0],
	    sizeof(state.field_player)) != 0)
		return false;

	anti_cloak_fixture(&tape, &state, cache);
	tape.fail_at = 9U;
	if (yt_earth_anti_cloak_run(&state, &ops, &tape, NULL)
	    || state.field_record != 2.0f || state.counter != 4.0f
	    || !state.reported || state.credit_loaded
	    || cache[2] != 0.0f || cache[4] != 0.0f
	    || memcmp(&state.field_player, &tape.players[0],
	    sizeof(state.field_player)) != 0)
		return false;

	anti_cloak_fixture(&tape, &state, cache);
	tape.fail_at = 13U;
	if (yt_earth_anti_cloak_run(&state, &ops, &tape, NULL)
	    || state.field_record != 4.0f || state.credit_loaded
	    || state.credit_argument != -10.25f
	    || memcmp(&state.field_player, &tape.players[1],
	    sizeof(state.field_player)) != 0)
		return false;

	anti_cloak_fixture(&tape, &state, cache);
	tape.fail_at = 14U;
	if (yt_earth_anti_cloak_run(&state, &ops, &tape, NULL)
	    || state.field_record != 7.0f || !state.credit_loaded
	    || state.field_player.credits != 90.0f
	    || yt_record_get_number(&state.field_player.record, YT_F81) != 90.0f)
		return false;

	for (failure = 1U; failure <= YT_ARRAY_LEN(reported_events); ++failure) {
		anti_cloak_fixture(&tape, &state, cache);
		tape.fail_at = failure;
		if (yt_earth_anti_cloak_run(&state, &ops, &tape, NULL)
		    || tape.event_count != failure
		    || memcmp(tape.events, reported_events,
		    failure * sizeof(reported_events[0])) != 0)
			return false;
	}
	for (failure = 1U; failure <= YT_ARRAY_LEN(none_events); ++failure) {
		anti_cloak_fixture(&tape, &state, cache);
		state.player_terminal = 1.5f;
		tape.fail_at = failure;
		if (yt_earth_anti_cloak_run(&state, &ops, &tape, NULL)
		    || tape.event_count != failure
		    || memcmp(tape.events, none_events,
		    failure * sizeof(none_events[0])) != 0)
			return false;
	}
	return !yt_earth_anti_cloak_run(NULL, &ops, &tape, NULL)
	    && !yt_earth_anti_cloak_run(&state, NULL, &tape, NULL);
}

static bool
check_earth_report_model(void)
{
	struct yt_player player;
	const int winning[6] = {1, 1, 2, 2, 3, 3};
	const char ticket[6] = {'1', '1', '1', '1', '1', '1'};
	bool matched[6];
	float discount[4] = {0};
	float price[4];

	yt_earth_prices(discount, price);
	if (price[0] != 250.0f || price[1] != 50.0f
	    || price[2] != 50.0f || price[3] != 200.0f)
		return false;
	discount[0] = 0.10000000149011612f;
	discount[1] = 0.10000000149011612f;
	discount[2] = 0.10000000149011612f;
	discount[3] = 0.10000000149011612f;
	yt_earth_prices(discount, price);
	if (price[0] != 225.0f || price[1] != 45.0f
	    || price[2] != 45.0f || price[3] != 180.0f)
		return false;
	discount[0] = 0.5f;
	discount[1] = 0.25f;
	discount[2] = 0.25f;
	discount[3] = 0.5f;
	yt_earth_prices(discount, price);
	if (price[0] != 125.0f || price[1] != 37.0f
	    || price[2] != 37.0f || price[3] != 100.0f)
		return false;
	discount[0] = 0.9509999752044678f;
	discount[1] = 0.9800000190734863f;
	discount[2] = 0.800000011920929f;
	discount[3] = 0.8999999761581421f;
	yt_earth_prices(discount, price);
	if (price[0] != 12.0f || price[1] != 1.0f
	    || price[2] != 9.0f || price[3] != 20.0f)
		return false;
	if (yt_earth_affordable(12345.0f, 1000.0f) != 12.0
	    || yt_earth_affordable(12345.0f, 250.0f) != 49.0
	    || yt_earth_affordable(12345.0f, 50.0f) != 246.0
	    || yt_earth_affordable(12345.0f, 5.0f) != 2469.0
	    || yt_earth_affordable(12345.0f, 500000.0f) != 0.0
	    || yt_earth_affordable(12345.0f, 1000000000.0f) != 0.0
	    || yt_earth_affordable(12345.0f, 200.0f) != 61.0)
		return false;
	if (yt_earth_purchase_quantity(3.9) != 3.0f
	    || yt_earth_purchase_quantity(-0.1) != -1.0f
	    || yt_earth_receipt_amount(0.0f, 2, 250.0f) != 0.0f
	    || yt_earth_receipt_amount(3.0f, 2, 250.0f) != 250.0f
	    || yt_earth_receipt_amount(2.0f, 2, 250.0f) != 2.0f
	    || yt_earth_receipt_amount(2.0f, 2, 50.0f) != 0.0f
	    || yt_earth_cloak_points(0.5f) != 25.0f
	    || yt_earth_cloak_default(25.0f, 12345.0f) != 12.0f
	    || yt_earth_cloak_default(25.0f, 25000.0f) != 25.0f
	    || yt_earth_cloak_overlay(25.0f, 1.0f)
	    != 0.5199999809265137f)
		return false;
	memset(&player, 0, sizeof(player));
	player.fighters = 10.0f;
	player.ground_forces = 5.5f;
	player.shields = 5.5f;
	yt_earth_supply_overlay(&player, 3, 1.0f);
	yt_earth_supply_overlay(&player, 7, 1.0f);
	yt_earth_supply_overlay(&player, 8, 1.0f);
	if (player.fighters != 11.0f || player.ground_forces != 6.0f
	    || player.shields != 6.0f)
		return false;
	if (yt_lottery_match_count(winning, ticket, matched) != 2
	    || !matched[0] || !matched[1] || matched[2] || matched[3]
	    || matched[4] || matched[5]
	    || yt_lottery_award(0) != 0.0f
	    || yt_lottery_award(1) != 100.0f
	    || yt_lottery_award(2) != 1000.0f
	    || yt_lottery_award(3) != 10000.0f
	    || yt_lottery_award(4) != 100000.0f
	    || yt_lottery_award(5) != 1000000.0f
	    || yt_lottery_award(6) != 100000000.0f
	    || yt_lottery_award(7) != 0.0f)
		return false;
	return yt_earth_selector_position("") == 1
	    && yt_earth_selector_position("L") == 1
	    && yt_earth_selector_position("M") == 2
	    && yt_earth_selector_position("0") == 3
	    && yt_earth_selector_position("C") == 4
	    && yt_earth_selector_position("LM") == 1
	    && yt_earth_selector_position("M0") == 2
	    && yt_earth_selector_position("LC") == 0
	    && yt_earth_selector_position("X") == 0
	    && yt_earth_selector_position(NULL) == 0;
}

static bool
check_player_constructor_failures(void)
{
	struct yt_game game;
	struct yt_record config;
	struct yt_record target;
	struct yt_record after;
	struct yt_player player;
	struct yt_error error;
	struct score_database_read_fault read_fault = {0U, 1U};
	bool valid = false;

	remove("CONSTRUCT.DAT");
	memset(&game, 0, sizeof(game));
	yt_error_clear(&error);
	if (!yt_database_open(&game.database, "CONSTRUCT.DAT", YT_OPEN_CREATE,
	    &error))
		return false;
	yt_database_set_read_provider(&game.database,
	    score_database_read_with_fault, &read_fault);
	if (yt_game_construct_player(&game, 2, 77.0f, &player, &error)
	    || error.status != YT_IO_ERROR || read_fault.calls != 1U
	    || game.database.last_get.basic_error != 57U)
		goto close;
	yt_database_set_read_provider(&game.database, NULL, NULL);

	yt_record_blank(&config);
	yt_record_set_number(&config, YT_F49, 123.0f);
	yt_record_set_number(&config, YT_F65, 45.0f);
	yt_record_set_number(&config, YT_F69, 678.0f);
	yt_record_set_number(&config, YT_F73, 9.0f);
	if (!yt_database_write(&game.database, 1, &config, &error))
		goto close;
	read_fault = (struct score_database_read_fault){0U, 2U};
	yt_database_set_read_provider(&game.database,
	    score_database_read_with_fault, &read_fault);
	yt_error_clear(&error);
	if (yt_game_construct_player(&game, 2, 77.0f, &player, &error)
	    || error.status != YT_IO_ERROR || read_fault.calls != 2U
	    || game.database.last_get.basic_error != 57U)
		goto close;
	yt_database_set_read_provider(&game.database, NULL, NULL);

	yt_record_blank(&target);
	yt_record_set_text(&target, (const uint8_t *)"Keep Name", 9);
	yt_record_set_number(&target, YT_F85, 9.0f);
	yt_record_set_number(&target, YT_F89, 4.0f);
	yt_record_set_number(&target, YT_F109, 88.0f);
	memcpy(target.bytes + YT_RECORD_TAIL_OFFSET, "TAIL",
	    YT_RECORD_TAIL_SIZE);
	if (!yt_database_write(&game.database, 2, &target, &error)
	    || !yt_database_flush(&game.database, &error))
		goto close;
	yt_database_close(&game.database);
	if (!yt_database_open(&game.database, "CONSTRUCT.DAT", YT_OPEN_READ,
	    &error))
		goto done;
	yt_error_clear(&error);
	if (yt_game_construct_player(&game, 2, 77.0f, &player, &error)
	    || error.status != YT_IO_ERROR
	    || strcmp(error.operation, "write record") != 0
	    || strcmp(player.name, "Keep Name") != 0
	    || player.name_length != 9.0f || player.score != 88.0f
	    || player.team != 0.0f || player.last_active != 77.0f
	    || player.turns != 123.0f || player.fighters != 45.0f
	    || player.credits != 678.0f || player.holds != 9.0f)
		goto close;
	yt_error_clear(&error);
	if (yt_game_set_player_identity(&game, 2, (const uint8_t *)"New", 3,
	    &player, &error) || error.status != YT_IO_ERROR
	    || strcmp(error.operation, "write record") != 0
	    || strcmp(player.name, "New") != 0 || player.name_length != 3.0f
	    || player.team != 0.0f || player.score != 88.0f)
		goto close;
	yt_database_close(&game.database);
	if (!yt_database_open(&game.database, "CONSTRUCT.DAT", YT_OPEN_READ,
	    &error) || !yt_database_read(&game.database, 2, &after, &error)
	    || memcmp(&after, &target, sizeof(after)) != 0)
		goto close;
	valid = true;

close:
	yt_database_set_read_provider(&game.database, NULL, NULL);
	yt_database_close(&game.database);
done:
	remove("CONSTRUCT.DAT");
	return valid;
}

static bool
check_post_login_repairs(void)
{
	static const uint8_t tail[YT_RECORD_TAIL_SIZE] =
	    {0x10, 0x32, 0x54, 0x76};
	struct yt_game game;
	struct yt_record record;
	struct yt_player player;
	struct yt_player durable;
	struct yt_post_login_repairs repairs;
	struct yt_error error;
	bool valid = false;

	remove("REPAIRS.DAT");
	memset(&game, 0, sizeof(game));
	yt_error_clear(&error);
	if (!yt_database_open(&game.database, "REPAIRS.DAT", YT_OPEN_CREATE,
	    &error))
		return false;
	yt_record_blank(&record);
	yt_record_set_text(&record, (const uint8_t *)"Repair Pilot", 12);
	yt_record_set_number(&record, YT_F49, 0.0f);
	yt_record_set_number(&record, YT_F65, 21.0f);
	yt_record_set_number(&record, YT_F69, 3.0f);
	yt_record_set_number(&record, YT_F73, 4.0f);
	yt_record_set_number(&record, YT_F77, 5.0f);
	yt_record_set_number(&record, YT_F109, 88.0f);
	memcpy(record.bytes + YT_RECORD_TAIL_OFFSET, tail, sizeof(tail));
	if (!yt_database_write(&game.database, 2, &record, &error)
	    || !yt_database_flush(&game.database, &error)
	    || !yt_game_post_login_repairs(&game, 2, 20.0f, &player,
	    &repairs, &error)
	    || !repairs.turns || !repairs.holds || repairs.writes != 2U
	    || player.turns != 1.0f || player.holds != 20.0f
	    || player.ore != 0.0f || player.organics != 0.0f
	    || player.equipment != 20.0f
	    || !yt_game_read_player(&game, 2, &durable, &error)
	    || durable.turns != 1.0f || durable.holds != 20.0f
	    || durable.ore != 0.0f || durable.organics != 0.0f
	    || durable.equipment != 20.0f || durable.score != 88.0f
	    || strcmp(durable.name, "Repair Pilot") != 0
	    || memcmp(durable.record.bytes + YT_RECORD_TAIL_OFFSET, tail,
	    sizeof(tail)) != 0)
		goto close;

	player.turns = 0.99999999f;
	player.holds = 20.0000001f;
	player.ore = 3.00000001f;
	player.organics = 4.00000001f;
	player.equipment = 5.00000001f;
	if (!yt_game_write_player(&game, 2, &player, &error)
	    || !yt_database_flush(&game.database, &error)
	    || !yt_game_post_login_repairs(&game, 2, 20.00000001f, &player,
	    &repairs, &error)
	    || repairs.turns || repairs.holds || repairs.writes != 0U
	    || player.turns != 1.0f || player.holds != 20.0f
	    || player.ore != 3.0f || player.organics != 4.0f
	    || player.equipment != 5.0f)
		goto close;

	player.turns = 0.0f;
	player.holds = 21.0f;
	if (!yt_game_write_player(&game, 2, &player, &error)
	    || !yt_database_flush(&game.database, &error))
		goto close;
	yt_database_close(&game.database);
	if (!yt_database_open(&game.database, "REPAIRS.DAT", YT_OPEN_READ,
	    &error))
		goto done;
	yt_error_clear(&error);
	if (yt_game_post_login_repairs(&game, 2, 20.0f, &player, &repairs,
	    &error) || error.status != YT_IO_ERROR
	    || strcmp(error.operation, "write record") != 0
	    || !repairs.turns || repairs.holds || repairs.writes != 0U
	    || player.turns != 1.0f || player.holds != 21.0f)
		goto close;

	yt_database_close(&game.database);
	if (!yt_database_open(&game.database, "REPAIRS.DAT", YT_OPEN_UPDATE,
	    &error) || !yt_game_read_player(&game, 2, &player, &error))
		goto done;
	player.turns = 1.0f;
	player.holds = 21.0f;
	if (!yt_game_write_player(&game, 2, &player, &error)
	    || !yt_database_flush(&game.database, &error))
		goto close;
	yt_database_close(&game.database);
	if (!yt_database_open(&game.database, "REPAIRS.DAT", YT_OPEN_READ,
	    &error))
		goto done;
	yt_error_clear(&error);
	if (yt_game_post_login_repairs(&game, 2, 20.0f, &player, &repairs,
	    &error) || error.status != YT_IO_ERROR
	    || strcmp(error.operation, "write record") != 0
	    || repairs.turns || !repairs.holds || repairs.writes != 0U
	    || player.turns != 1.0f || player.holds != 20.0f
	    || player.ore != 0.0f || player.organics != 0.0f
	    || player.equipment != 20.0f
	    || !yt_game_read_player(&game, 2, &durable, &error)
	    || durable.holds != 21.0f)
		goto close;
	valid = true;

close:
	yt_database_close(&game.database);
done:
	remove("REPAIRS.DAT");
	return valid;
}

static bool
check_sector_force_routes(void)
{
	struct yt_error error;
	enum yt_sector_force_route route;
	int owner;

	yt_error_clear(&error);
	if (!yt_sector_force_route(0.0f, -1.0f, 2, &route, &owner, &error)
	    || route != YT_SECTOR_FORCE_FRIENDLY || owner != 0
	    || !yt_sector_force_route(-7.0f, 2.0f, 2, &route, &owner,
	    &error) || route != YT_SECTOR_FORCE_FRIENDLY || owner != 0
	    || !yt_sector_force_route(-1.0f, -1.0f, 2, &route, &owner,
	    &error) || route != YT_SECTOR_FORCE_HOSTILE || owner != 0
	    || !yt_sector_force_route(10.0f, 3.0f, 2, &route, &owner,
	    &error) || route != YT_SECTOR_FORCE_OWNER_GET || owner != 3)
		return false;
	yt_error_clear(&error);
	if (yt_sector_force_route(1.0f, 2.5f, 2, &route, &owner, &error)
	    || error.status != YT_RANGE
	    || strcmp(error.operation, "fighter owner record") != 0)
		return false;
	if (!yt_sector_is_black_hole(7.0f, 7.0f, 9.0f)
	    || !yt_sector_is_black_hole(9.0f, 7.0f, 9.0f)
	    || yt_sector_is_black_hole(8.0f, 7.0f, 9.0f)
	    || !yt_sector_mines_admitted(0.4f, 0.0f)
	    || yt_sector_mines_admitted(0.0f, 0.0f)
	    || yt_sector_mines_admitted(-0.4f, 0.0f)
	    || yt_sector_mines_admitted(1.0f, -1.0f)
	    || yt_sector_mines_admitted(1.0f, 1.0f)
	    || !yt_sector_force_same_team(3.0f, 3.0f)
	    || !yt_sector_force_same_team(-3.0f, -3.0f)
	    || yt_sector_force_same_team(0.0f, 0.0f)
	    || yt_sector_force_same_team(3.0f, 4.0f))
		return false;
	return true;
}

static bool
check_hostile_menu_front(void)
{
	uint8_t row[128];
	uint8_t shield_row[128];
	size_t length;
	size_t shield_length;

	if (!yt_hostile_menu_row(1000.0, 1250.0, row, sizeof(row), &length)
	    || length != strlen("Fighters: 1000 / 1250")
	    || memcmp(row, "Fighters: 1000 / 1250", length) != 0)
		return false;
	if (!yt_hostile_menu_row(1.0, -0.5, row, sizeof(row), &length)
	    || length != strlen("Fighters: 1 /-.5")
	    || memcmp(row, "Fighters: 1 /-.5", length) != 0)
		return false;
	if (yt_hostile_menu_row(1.0, 2.0, row, 4, &length)
	    || length != 0)
		return false;
	if (yt_hostile_menu_dispatch("") != YT_HOSTILE_MENU_HELP
	    || yt_hostile_menu_dispatch("?") != YT_HOSTILE_MENU_HELP
	    || yt_hostile_menu_dispatch("S") != YT_HOSTILE_MENU_SECTOR
	    || yt_hostile_menu_dispatch("I") != YT_HOSTILE_MENU_INFO
	    || yt_hostile_menu_dispatch("A") != YT_HOSTILE_MENU_ATTACK
	    || yt_hostile_menu_dispatch("AQ") != YT_HOSTILE_MENU_ATTACK
	    || yt_hostile_menu_dispatch("QBDWT") != YT_HOSTILE_MENU_QUIT
	    || yt_hostile_menu_dispatch("BDW") != YT_HOSTILE_MENU_BRIBE
	    || yt_hostile_menu_dispatch("DWT") != YT_HOSTILE_MENU_MINE
	    || yt_hostile_menu_dispatch("WT") != YT_HOSTILE_MENU_WARP
	    || yt_hostile_menu_dispatch("T") != YT_HOSTILE_MENU_TEAM
	    || yt_hostile_menu_dispatch("YES") != YT_HOSTILE_MENU_INVALID
	    || yt_hostile_menu_dispatch("A ") != YT_HOSTILE_MENU_INVALID
	    || yt_hostile_menu_dispatch("SI") != YT_HOSTILE_MENU_INVALID)
		return false;
	if (yt_main_shell_dispatch(NULL) != YT_MAIN_SHELL_DISPLAY
	    || yt_main_shell_dispatch("") != YT_MAIN_SHELL_DISPLAY
	    || yt_main_shell_dispatch("X") != YT_MAIN_SHELL_SOUND
	    || yt_main_shell_dispatch("XJUNK") != YT_MAIN_SHELL_INVALID
	    || yt_main_shell_dispatch("S") != YT_MAIN_SHELL_SENSORS
	    || yt_main_shell_dispatch("SJUNK") != YT_MAIN_SHELL_INVALID
	    || yt_main_shell_dispatch("WJUNK") != YT_MAIN_SHELL_WARP
	    || yt_main_shell_dispatch(")") != YT_MAIN_SHELL_MISSILE
	    || yt_main_shell_dispatch("+") != YT_MAIN_SHELL_PLASMA
	    || yt_main_shell_dispatch("AJUNK") != YT_MAIN_SHELL_ATTACK
	    || yt_main_shell_dispatch("BJUNK") != YT_MAIN_SHELL_BUY_PORT
	    || yt_main_shell_dispatch("CJUNK") != YT_MAIN_SHELL_COMPUTER
	    || yt_main_shell_dispatch("FJUNK") != YT_MAIN_SHELL_FIGHTERS
	    || yt_main_shell_dispatch("LJUNK") != YT_MAIN_SHELL_LAND
	    || yt_main_shell_dispatch("MJUNK") != YT_MAIN_SHELL_MOVE
	    || yt_main_shell_dispatch("PJUNK") != YT_MAIN_SHELL_TRADE
	    || yt_main_shell_dispatch("QJUNK") != YT_MAIN_SHELL_QUIT
	    || yt_main_shell_dispatch("TJUNK") != YT_MAIN_SHELL_TEAM
	    || yt_main_shell_dispatch("DJUNK") != YT_MAIN_SHELL_MINES
	    || yt_main_shell_dispatch("$JUNK") != YT_MAIN_SHELL_COLLECT
	    || yt_main_shell_dispatch("GJUNK") != YT_MAIN_SHELL_GENESIS
	    || yt_main_shell_dispatch("NJUNK") != YT_MAIN_SHELL_RENAME_PORT
	    || yt_main_shell_dispatch("VJUNK") != YT_MAIN_SHELL_VERSION
	    || yt_main_shell_dispatch("IJUNK") != YT_MAIN_SHELL_INFO
	    || yt_main_shell_dispatch("ZJUNK") != YT_MAIN_SHELL_INSTRUCTIONS
	    || yt_main_shell_dispatch("?JUNK") != YT_MAIN_SHELL_HELP
	    || yt_main_shell_dispatch("!") != YT_MAIN_SHELL_INVALID
	    || yt_main_shell_dispatch("_") != YT_MAIN_SHELL_INVALID)
		return false;
	if (yt_hostile_attack_admit(0.0f, 99.0f)
	    != YT_HOSTILE_ATTACK_NO_FIGHTERS
	    || yt_hostile_attack_admit(12.0f, 13.0f)
	    != YT_HOSTILE_ATTACK_TOO_MANY
	    || yt_hostile_attack_admit(12.0f, 0.0f)
	    != YT_HOSTILE_ATTACK_LESS_THAN_ONE
	    || yt_hostile_attack_admit(12.0f, 0.99999994f)
	    != YT_HOSTILE_ATTACK_LESS_THAN_ONE
	    || yt_hostile_attack_admit(12.0f, 1.0f)
	    != YT_HOSTILE_ATTACK_ADMITTED
	    || yt_hostile_attack_admit(12.0f, 12.0f)
	    != YT_HOSTILE_ATTACK_ADMITTED
	    || yt_hostile_attack_admit(12.0f, 12.00000095f)
	    != YT_HOSTILE_ATTACK_TOO_MANY)
		return false;
	if (yt_hostile_attack_quantum(1.0, 5.0) != 1.0f
	    || yt_hostile_attack_quantum(40.0, 80.0) != 2.0f
	    || yt_hostile_attack_quantum(80.0, 40.0) != 2.0f
	    || !yt_hostile_attack_loses_attacker(0.0f, 0.44f)
	    || yt_hostile_attack_loses_attacker(0.0f,
	    0.44999998807907104f)
	    || yt_hostile_surrender_route(2.0f)
	    != YT_HOSTILE_SURRENDER_PLAYER
	    || yt_hostile_surrender_route(1.0f)
	    != YT_HOSTILE_SURRENDER_QUIET
	    || yt_hostile_surrender_route(0.0f)
	    != YT_HOSTILE_SURRENDER_QUIET
	    || yt_hostile_surrender_route(-1.0f)
	    != YT_HOSTILE_SURRENDER_XANNOR
	    || yt_hostile_surrender_route(-2.0f)
	    != YT_HOSTILE_SURRENDER_MERCENARY
	    || yt_xannor_attack_bonus(512000.0, 98.0f, 100.0f) != 2.0f
	    || yt_xannor_attack_bonus(1280000.0, 99.0f, 100.0f) != 1.0f
	    || yt_xannor_attack_bonus(1280000.0, 100.0f, 100.0f) != 0.0f
	    || yt_xannor_attack_bonus(255999.0, 0.0f, 100.0f) != 0.0f)
		return false;
	{
		double fighters = 101.0;
		float shields = 101.0f;

		if (!yt_fighter_shield_spill_step(&fighters, &shields, 0.999f)
		    || fighters != 1.0 || shields != 101.0f
		    || !yt_fighter_shield_spill_step(&fighters, &shields, 0.5f)
		    || fighters != 0.0 || shields != 101.0f
		    || yt_fighter_shield_spill_step(&fighters, &shields, 0.0f))
			return false;
	}
	{
		double fighters = 1.0;
		float shields = 16777216.0f;

		if (!yt_fighter_shield_spill_step(&fighters, &shields, 0.0f)
		    || fighters != 1.0 || shields != 16777215.0f
		    || !yt_fighter_shield_spill_step(&fighters, &shields, 0.999f)
		    || fighters != 0.0 || shields != 16777215.0f)
			return false;
	}
	if (!yt_fighter_shield_spill_rows(16777217.0, 0.0f,
	    row, sizeof(row), &length, shield_row, sizeof(shield_row),
	    &shield_length)
	    || length != strlen("Fighters remaining: 16777217")
	    || memcmp(row, "Fighters remaining: 16777217", length) != 0
	    || shield_length != strlen("Shields reduced to: 0")
	    || memcmp(shield_row, "Shields reduced to: 0",
	    shield_length) != 0)
		return false;
	if (!yt_hostile_defeated_row(3.0, row, sizeof(row), &length)
	    || length != strlen(
	    "You defeated all the fighters and have 3 left.")
	    || memcmp(row, "You defeated all the fighters and have 3 left.",
	    length) != 0)
		return false;
	{
		double threshold = yt_bribe_offer_threshold(10.0f, 0.5f);

		if (threshold != 20.0
		    || !yt_bribe_offer_accepted(20.0f, 20.0f, threshold)
		    || yt_bribe_offer_accepted(19.0f, 20.0f, threshold)
		    || !yt_bribe_ordinary_forces(-1.0f, 1.0f, 10.0f, 1.0f)
		    || yt_bribe_ordinary_forces(3.0f, 10.0f, 5.0f,
		    0.33000001311302185f)
		    || !yt_bribe_mercenary_forces(10.0f, 20.0f,
		    0.049999997f, 0.0f, false)
		    || yt_bribe_mercenary_forces(10.0f, 20.0f,
		    0.05000000074505806f, 1.0f, false)
		    || !yt_bribe_mercenary_forces(10.0f, 20.0f,
		    1.0f, 0.0f, true)
		    || yt_bribe_forced_admit(0.0, 0.0f, true, 0.0f)
		    != YT_BRIBE_FORCED_FATAL
		    || yt_bribe_forced_admit(0.0, 0.0f, false, 0.0f)
		    != YT_BRIBE_FORCED_LESS_THAN_ONE
		    || yt_bribe_forced_admit(0.0, 1.0f, true, 0.0f)
		    != YT_BRIBE_FORCED_LESS_THAN_ONE
		    || yt_bribe_forced_admit(0.99999999, 1.0f, true, 1.0f)
		    != YT_BRIBE_FORCED_ATTACK
		    || yt_bribe_forced_admit(1.0, 0.0f, true, 1.0f)
		    != YT_BRIBE_FORCED_ATTACK
		    || yt_sector_mine_admit(5.0f, 0.99999994f)
		    != YT_SECTOR_MINE_BELOW_ONE
		    || yt_sector_mine_admit(5.0f, 1.0f)
		    != YT_SECTOR_MINE_ACCEPTED
		    || yt_sector_mine_admit(5.0f, 1.5f)
		    != YT_SECTOR_MINE_ACCEPTED
		    || yt_sector_mine_admit(5.0f, 5.0f)
		    != YT_SECTOR_MINE_ACCEPTED
		    || yt_sector_mine_admit(5.0f, 5.00000048f)
		    != YT_SECTOR_MINE_ABOVE_CARRIED
		    || !yt_no_turn_gate_denied(-1.0f)
		    || !yt_no_turn_gate_denied(0.0f)
		    || yt_no_turn_gate_denied(0.00000001f)
		    || yt_team_choice_rejected(1.0f, 0.0f, 0, 0)
		    || !yt_team_choice_rejected(4.0f, 0.0f, 0, 0)
		    || yt_team_choice_rejected(2.0f, 0.4f, 0, 0)
		    || !yt_team_choice_rejected(2.0f, 0.6f, 0, 1)
		    || yt_team_choice_rejected(7.0f, 7.0f, -1, 7)
		    || !yt_team_choice_rejected(7.0f, 7.0f, 0, 7)
		    || yt_team_choice_rejected(10.0f, 7.0f, -1, 7)
		    || !yt_team_choice_rejected(10.00000095f, 7.0f, -1, 7))
			return false;
	}
	{
		static const uint8_t binary_name[] = {'A', 0, 'B'};
		static const uint8_t expected_display[] =
		    "Collect 2 turns bonus for destroying 512000 Xannor!!";
		static const uint8_t expected_news[] = {
		    'A', 0, 'B', ' ', 'c', 'o', 'l', 'l', 'e', 'c', 't', 'e', 'd',
		    ' ', '2', ' ', 't', 'u', 'r', 'n', 's', ' ', 'b', 'o', 'n', 'u',
		    's', ' ', 'f', 'o', 'r', ' ', 'd', 'e', 's', 't', 'r', 'o', 'y',
		    'i', 'n', 'g', ' ', '5', '1', '2', '0', '0', '0', ' ', 'X', 'a',
		    'n', 'n', 'o', 'r', '!', '!'
		};
		uint8_t display[128];
		uint8_t news[128];
		size_t display_length;
		size_t news_length;

		if (!yt_xannor_attack_reward_rows(binary_name,
		    sizeof(binary_name), 2.0f, 512000.0,
		    display, sizeof(display), &display_length,
		    news, sizeof(news), &news_length)
		    || display_length != sizeof(expected_display) - 1U
		    || memcmp(display, expected_display, display_length) != 0
		    || news_length != sizeof(expected_news)
		    || memcmp(news, expected_news, news_length) != 0
		    || yt_xannor_attack_reward_rows(binary_name,
		    sizeof(binary_name), 2.0f, 512000.0,
		    display, sizeof(expected_display) - 2U, &display_length,
		    news, sizeof(news), &news_length)
		    || display_length != 0U || news_length != 0U)
			return false;
	}
	{
		struct yt_player player = {0};
		struct yt_player expected_player;
		struct yt_sector sector = {0};
		struct yt_sector expected_sector;

		memset(player.record.bytes, 0x5a, sizeof(player.record.bytes));
		player.shields = 90.0f;
		player.fighters = 80.0f;
		player.credits = 70.0f;
		expected_player = player;
		(void)yt_record_set_number(&expected_player.record, YT_F53, 12.0f);
		(void)yt_record_set_number(&expected_player.record, YT_F61, 34.0f);
		yt_deployed_attack_player_overlay(&player, 12.0f, 34.0f);
		if (player.shields != 12.0f || player.fighters != 34.0f
		    || player.credits != 70.0f
		    || memcmp(player.record.bytes, expected_player.record.bytes,
		    YT_RECORD_SIZE) != 0)
			return false;

		memset(sector.record.bytes, 0xa5, sizeof(sector.record.bytes));
		sector.fighters = 20.0f;
		sector.fighter_owner = 7.0f;
		sector.planet = 99.0f;
		expected_sector = sector;
		(void)yt_record_set_number(&expected_sector.record, YT_F81, 3.0f);
		yt_deployed_attack_sector_overlay(&sector, 3.0f);
		if (sector.fighters != 3.0f || sector.fighter_owner != 7.0f
		    || sector.planet != 99.0f
		    || memcmp(sector.record.bytes, expected_sector.record.bytes,
		    YT_RECORD_SIZE) != 0)
			return false;

		expected_sector = sector;
		(void)yt_record_set_number(&expected_sector.record, YT_F81, 0.5f);
		(void)yt_record_set_number(&expected_sector.record, YT_F85, 0.0f);
		yt_deployed_attack_sector_overlay(&sector, 0.5f);
		if (sector.fighters != 0.5f || sector.fighter_owner != 0.0f
		    || sector.planet != 99.0f
		    || memcmp(sector.record.bytes, expected_sector.record.bytes,
		    YT_RECORD_SIZE) != 0)
			return false;

		sector.fighters = 5.0f;
		sector.fighter_owner = -2.0f;
		expected_sector = sector;
		(void)yt_record_set_number(&expected_sector.record, YT_F85, 0.0f);
		(void)yt_record_set_number(&expected_sector.record, YT_F81, 0.0f);
		yt_bribe_sector_overlay(&sector);
		if (sector.fighters != 0.0f || sector.fighter_owner != 0.0f
		    || sector.planet != 99.0f
		    || memcmp(sector.record.bytes, expected_sector.record.bytes,
		    YT_RECORD_SIZE) != 0)
			return false;

		expected_player = player;
		(void)yt_record_set_number(&expected_player.record, YT_F61, 44.0f);
		(void)yt_record_set_number(&expected_player.record, YT_F81, 55.0f);
		yt_bribe_player_overlay(&player, 44.0f, 55.0f);
		if (player.shields != 12.0f || player.fighters != 44.0f
		    || player.credits != 55.0f
		    || memcmp(player.record.bytes, expected_player.record.bytes,
		    YT_RECORD_SIZE) != 0)
			return false;
	}
	{
		struct yt_sector sector = {0};
		struct yt_player player = {0};
		struct yt_player banished = {0};
		struct yt_record roster_record;
		struct yt_record roster_expected;
		struct yt_record name_record;
		struct yt_record name_expected;
		struct yt_record password_record;
		struct yt_record password_expected;
		const float roster[4] = {2.0f, 0.0f, 4.0f, 5.0f};
		static const uint8_t team_name[] = "New Raiders";
		static const uint8_t password[4] = {'P', 'A', 'S', 'S'};
		char prepared_name[64];
		size_t prepared_length;
		size_t index;
		uint8_t sector_record[YT_RECORD_SIZE];
		uint8_t player_record[YT_RECORD_SIZE];
		uint8_t banished_record[YT_RECORD_SIZE];

		memset(sector.record.bytes, 0xa5, sizeof(sector.record.bytes));
		memset(player.record.bytes, 0x5a, sizeof(player.record.bytes));
		memset(banished.record.bytes, 0x3c,
		    sizeof(banished.record.bytes));
		memset(roster_record.bytes, 0xc3, sizeof(roster_record.bytes));
		memset(name_record.bytes, 0x96, sizeof(name_record.bytes));
		memset(password_record.bytes, 0x69,
		    sizeof(password_record.bytes));
		strcpy(prepared_name, "ab");
		if (yt_team_prepare_name(prepared_name, &prepared_length))
			return false;
		strcpy(prepared_name, "   ");
		if (!yt_team_prepare_name(prepared_name, &prepared_length)
		    || prepared_length != 0U || prepared_name[0] != '\0')
			return false;
		strcpy(prepared_name, "  new   RAIDERS  ");
		if (!yt_team_prepare_name(prepared_name, &prepared_length)
		    || prepared_length != 11U
		    || strcmp(prepared_name, "New Raiders") != 0)
			return false;
		memset(prepared_name, 'A', 50U);
		prepared_name[50] = '\0';
		if (!yt_team_prepare_name(prepared_name, &prepared_length)
		    || prepared_length != YT_TEXT_FIELD_SIZE
		    || prepared_name[0] != 'A'
		    || prepared_name[YT_TEXT_FIELD_SIZE] != '\0')
			return false;
		for (index = 1U; index < YT_TEXT_FIELD_SIZE; ++index)
			if (prepared_name[index] != 'a')
				return false;
		roster_expected = roster_record;
		name_expected = name_record;
		password_expected = password_record;
		(void)yt_record_set_number(&roster_expected, YT_F109, roster[0]);
		(void)yt_record_set_number(&roster_expected, YT_F117, roster[1]);
		(void)yt_record_set_number(&roster_expected, YT_F121, roster[2]);
		(void)yt_record_set_number(&roster_expected, YT_F125, roster[3]);
		yt_record_set_text(&name_expected, team_name,
		    sizeof(team_name) - 1U);
		(void)yt_record_set_number(&name_expected, YT_F73,
		    (float)(sizeof(team_name) - 1U));
		memcpy(password_expected.bytes + YT_F113, password,
		    sizeof(password));
		memcpy(sector_record, sector.record.bytes, sizeof(sector_record));
		memcpy(player_record, player.record.bytes, sizeof(player_record));
		memcpy(banished_record, banished.record.bytes,
		    sizeof(banished_record));
		sector.fighters = 99.0f;
		sector.fighter_owner = 44.0f;
		sector.planet = 8.0f;
		player.sector = 99.0f;
		player.fighters = 12.0f;
		player.team = 7.0f;
		banished.team = 7.0f;
		yt_team_transfer_apply_sector(&sector, 10.0, 5.0f);
		yt_team_transfer_apply_player(&player, 5.0f);
		yt_team_banish_apply_player(&banished);
		yt_team_roster_overlay(&roster_record, roster);
		yt_team_name_overlay(&name_record, team_name,
		    sizeof(team_name) - 1U);
		yt_team_password_overlay(&password_record, password);
		if (sector.fighters != 15.0f || sector.fighter_owner != 44.0f
		    || sector.planet != 8.0f || player.fighters != 7.0f
		    || player.sector != 99.0f || player.team != 7.0f
		    || memcmp(sector.record.bytes, sector_record, YT_F81) != 0
		    || memcmp(sector.record.bytes + YT_F85,
		    sector_record + YT_F85, YT_RECORD_SIZE - YT_F85) != 0
		    || yt_record_get_number(&sector.record, YT_F81) != 15.0f
		    || memcmp(player.record.bytes, player_record, YT_F61) != 0
		    || memcmp(player.record.bytes + YT_F65,
		    player_record + YT_F65, YT_RECORD_SIZE - YT_F65) != 0
		    || yt_record_get_number(&player.record, YT_F61) != 7.0f
		    || banished.team != 0.0f
		    || yt_record_get_number(&banished.record, YT_F89) != 0.0f
		    || memcmp(banished.record.bytes, banished_record, YT_F89) != 0
		    || memcmp(banished.record.bytes + YT_F93,
		    banished_record + YT_F93,
		    YT_RECORD_SIZE - YT_F93) != 0
		    || memcmp(roster_record.bytes, roster_expected.bytes,
		    YT_RECORD_SIZE) != 0
		    || memcmp(name_record.bytes, name_expected.bytes,
		    YT_RECORD_SIZE) != 0
		    || memcmp(password_record.bytes, password_expected.bytes,
		    YT_RECORD_SIZE) != 0)
			return false;
	}
	{
		const float factors[3] = {-60.0f, 74.0f, -66.0f};
		const float zero_factors[3] = {0.0f, 0.0f, 0.0f};
		size_t order[3] = {99U, 99U, 99U};

		if (!yt_port_link_missing(0.0f)
		    || yt_port_link_missing(-1.0f)
		    || yt_port_link_missing(0.5f)
		    || yt_port_selected_expression(2055.0f, 2.75f) != 2057.75f
		    || qb_brun_random_record_number(
		    yt_port_selected_expression(2055.0f, 2.75f)) != 2057U
		    || qb_brun_random_record_number(
		    yt_port_selected_expression(0.0f, 0.5f)) != 0U
		    || qb_brun_random_record_number(
		    yt_port_selected_expression(0.0f, 16777216.0f)) != 0U
		    || yt_port_trade_schedule(factors, order) != 3U
		    || order[0] != 0U || order[1] != 2U || order[2] != 1U
		    || yt_port_trade_schedule(zero_factors, order) != 0U
		    || yt_computer_selector_position("+") != 1
		    || yt_computer_selector_position("+!") != 1
		    || yt_computer_selector_position("!L") != 2
		    || yt_computer_selector_position("LM") != 3
		    || yt_computer_selector_position("MP") != 4
		    || yt_computer_selector_position("P?") != 5
		    || yt_computer_selector_position("?1") != 6
		    || yt_computer_selector_position("1") != 7
		    || yt_computer_selector_position("23") != 8
		    || yt_computer_selector_position("34") != 9
		    || yt_computer_selector_position("45") != 10
		    || yt_computer_selector_position("59") != 11
		    || yt_computer_selector_position("9") != 12
		    || yt_computer_selector_position("17") != 0
		    || yt_computer_selector_position("NO") != 0)
			return false;
	}
	{
		static const uint8_t treasury_dirty_zero[4] = {
			0x00, 0x00, 0x20, 0x00
		};
		struct yt_port treasury = {0};
		struct yt_port cleared = {0};
		struct yt_port stock = {0};
		struct yt_player credit = {0};
		struct yt_player holds = {0};
		struct yt_record expected;

		memset(treasury.record.bytes, 0x29,
		    sizeof(treasury.record.bytes));
		treasury.treasury = 100.0f;
		expected = treasury.record;
		(void)yt_record_set_number(&expected, YT_F89, 120.0f);
		yt_trade_treasury_overlay(&treasury, 20.0f);
		if (treasury.treasury != 120.0f
		    || memcmp(treasury.record.bytes, expected.bytes,
		    YT_RECORD_SIZE) != 0)
			return false;
		if (!yt_record_set_raw_number(&cleared.record, YT_F89,
		    treasury_dirty_zero))
			return false;
		cleared.treasury = 0.0f;
		yt_port_encode(&cleared);
		if (memcmp(cleared.record.bytes + YT_F89,
		    treasury_dirty_zero, sizeof(treasury_dirty_zero)) != 0)
			return false;

		memset(credit.record.bytes, 0x42, sizeof(credit.record.bytes));
		credit.credits = 12345.0f;
		expected = credit.record;
		(void)yt_record_set_number(&expected, YT_F81, 12285.0f);
		yt_trade_credit_overlay(&credit, -60.0f);
		if (credit.credits != 12285.0f
		    || memcmp(credit.record.bytes, expected.bytes,
		    YT_RECORD_SIZE) != 0)
			return false;

		memset(holds.record.bytes, 0x63, sizeof(holds.record.bytes));
		holds.ore = 10.0f;
		holds.organics = 20.0f;
		holds.equipment = 30.0f;
		expected = holds.record;
		(void)yt_record_set_number(&expected, YT_F69, 13.0f);
		(void)yt_record_set_number(&expected, YT_F73, 20.0f);
		(void)yt_record_set_number(&expected, YT_F77, 30.0f);
		yt_trade_holds_overlay(&holds, 0U, 3.0f, 1.0f);
		if (holds.ore != 13.0f || holds.organics != 20.0f
		    || holds.equipment != 30.0f
		    || memcmp(holds.record.bytes, expected.bytes,
		    YT_RECORD_SIZE) != 0)
			return false;

		memset(stock.record.bytes, 0x84, sizeof(stock.record.bytes));
		stock.stock[0] = 999.0f;
		expected = stock.record;
		(void)yt_record_set_number(&expected, YT_F49, 97.0f);
		yt_trade_stock_overlay(&stock, 0U, 100.0, 3.0f);
		if (stock.stock[0] != 97.0f
		    || memcmp(stock.record.bytes, expected.bytes,
		    YT_RECORD_SIZE) != 0)
			return false;
	}
	return true;
}

static bool
check_admission_news(void)
{
	static const uint8_t full_expected[] =
	    "***07-24-2026 Star Lord: New player not allowed - game full.\r\n\x1a";
	static const uint8_t new_expected[] =
	    "-=*=- 07-24-2026 Star Lord New Player Entered -=*=-\r\n\x1a";
	static const uint8_t login_expected[] =
	    "-=*=- 12:34:56 Star Lord Logged on -=*=-\r\n\x1a";
	struct yt_text_file text;
	struct yt_error error;
	char long_name[512];
	bool valid = false;

	remove("YTNEWS.DAT");
	yt_error_clear(&error);
	if (!yt_news_append_game_full("07-24-2026", "Star Lord", &error)
	    || !yt_text_read("YTNEWS.DAT", &text, &error))
		goto done;
	if (text.length != sizeof(full_expected) - 1U
	    || memcmp(text.data, full_expected, sizeof(full_expected) - 1U) != 0) {
		yt_text_free(&text);
		goto done;
	}
	yt_text_free(&text);
	remove("YTNEWS.DAT");
	if (!yt_news_append_new_player("07-24-2026", "Star Lord", &error)
	    || !yt_text_read("YTNEWS.DAT", &text, &error))
		goto done;
	if (text.length != sizeof(new_expected) - 1U
	    || memcmp(text.data, new_expected, sizeof(new_expected) - 1U) != 0) {
		yt_text_free(&text);
		goto done;
	}
	yt_text_free(&text);
	remove("YTNEWS.DAT");
	if (!yt_news_append_login("12:34:56", "Star Lord", &error)
	    || !yt_text_read("YTNEWS.DAT", &text, &error))
		goto done;
	if (text.length != sizeof(login_expected) - 1U
	    || memcmp(text.data, login_expected,
	    sizeof(login_expected) - 1U) != 0) {
		yt_text_free(&text);
		goto done;
	}
	yt_text_free(&text);
	memset(long_name, 'X', sizeof(long_name) - 1U);
	long_name[sizeof(long_name) - 1U] = '\0';
	yt_error_clear(&error);
	if (yt_news_append_new_player("07-24-2026", long_name, &error)
	    || error.status != YT_RANGE
	    || strcmp(error.operation, "format news") != 0)
		goto done;
	valid = true;

done:
	remove("YTNEWS.DAT");
	return valid;
}

static bool
check_maintenance_writers(void)
{
	static const uint8_t expected_news[] =
	    "first\r\nsecond\r\nA\0B\r\n\x1a";
	static const uint8_t binary_news[] = {'A', 0, 'B'};
	static const uint8_t personal_header[] = {
		0x00, 0x00, 0x00, 0x81,
		0x00, 0x00, 0x40, 0x82,
		0x00, 0x00, 0x80, 0x82
	};
	static const uint8_t broadcast_header[] = {
		0x00, 0x00, 0x20, 0x85,
		0x00, 0x00, 0x80, 0x82,
		0x00, 0x00, 0x80, 0x82
	};
	struct yt_text_file news;
	struct yt_error error;
	uint8_t records[YT_RADIO_RECORD_SIZE * 2U];
	char long_text[75];
	FILE *file = NULL;
	size_t length;
	size_t index;
	bool valid = false;

	remove("YTNEWS.DAT");
	remove("YTRMSG.DAT");
	yt_error_clear(&error);
	if (!yt_news_append("first", &error)
	    || !yt_news_append("second", &error)
	    || !yt_news_append_bytes(binary_news, sizeof(binary_news), &error)
	    || !yt_text_read("YTNEWS.DAT", &news, &error))
		goto done;
	if (news.length != sizeof(expected_news) - 1U
	    || memcmp(news.data, expected_news, sizeof(expected_news) - 1U) != 0) {
		yt_text_free(&news);
		goto done;
	}
	yt_text_free(&news);
	memset(long_text, 'M', sizeof(long_text) - 1U);
	long_text[sizeof(long_text) - 1U] = '\0';
	if (!yt_radio_append_maintenance(long_text, -2.0f, 3.0f, &error)
	    || !yt_radio_append_maintenance("all", -2.0f, -2.0f, &error))
		goto done;
	file = fopen("YTRMSG.DAT", "rb");
	if (file == NULL)
		goto done;
	length = fread(records, 1, sizeof(records), file);
	if (length != sizeof(records) || fgetc(file) != EOF || ferror(file)
	    || fclose(file) != 0) {
		file = NULL;
		goto done;
	}
	file = NULL;
	if (memcmp(records, personal_header, sizeof(personal_header)) != 0
	    || memcmp(records + YT_RADIO_RECORD_SIZE, broadcast_header,
	    sizeof(broadcast_header)) != 0
	    || memcmp(records + 12U, long_text, 72U) != 0
	    || records[84] != 0 || records[85] != 0
	    || memcmp(records + YT_RADIO_RECORD_SIZE + 12U, "all", 3U) != 0
	    || records[YT_RADIO_RECORD_SIZE + 84U] != 0
	    || records[YT_RADIO_RECORD_SIZE + 85U] != 0)
		goto done;
	for (index = YT_RADIO_RECORD_SIZE + 15U;
	    index < YT_RADIO_RECORD_SIZE + 84U; ++index)
		if (records[index] != ' ')
			goto done;
	valid = true;

done:
	if (file != NULL)
		(void)fclose(file);
	remove("YTNEWS.DAT");
	remove("YTRMSG.DAT");
	return valid;
}

static unsigned
hex_digit(char digit)
{
	if (digit >= '0' && digit <= '9')
		return (unsigned)(digit - '0');
	if (digit >= 'a' && digit <= 'f')
		return (unsigned)(digit - 'a') + 10U;
	return (unsigned)(digit - 'A') + 10U;
}

static uint64_t
hash_text(uint64_t hash, const char *text)
{
	do {
		hash = (hash ^ (uint8_t)*text) * UINT64_C(1099511628211);
	} while (*text++ != '\0');
	return hash;
}

static bool
check_format(const char *hex, enum yt_score_field field,
    const char *expected)
{
	uint8_t raw[8];
	char actual[160];
	size_t hex_length = strlen(hex);
	size_t raw_length = hex_length / 2U;
	size_t index;

	if ((hex_length != 8U && hex_length != 16U)
	    || raw_length > sizeof(raw))
		return false;
	for (index = 0; index < raw_length; ++index)
		raw[index] = (uint8_t)((hex_digit(hex[index * 2U]) << 4)
		    | hex_digit(hex[index * 2U + 1U]));
	return yt_score_format_mbf(actual, sizeof(actual), raw, raw_length,
	    field) && strcmp(actual, expected) == 0;
}

static bool
check_formatter_boundaries(void)
{
	static const struct {
		const char *hex;
		enum yt_score_field field;
		const char *expected;
	} vectors[] = {
		{"ffff4687", YT_SCORE_FIELD_RANK, " 99"},
		{"00004787", YT_SCORE_FIELD_RANK, "%100"},
		{"f7237494", YT_SCORE_FIELD_RANK, "%999999"},
		{"f8237494", YT_SCORE_FIELD_RANK, "%10E+05"},
		{"ffdf798a", YT_SCORE_FIELD_PORTS, " 999"},
		{"00e0798a", YT_SCORE_FIELD_PORTS, "%1000"},
		{"7f961898", YT_SCORE_FIELD_PORTS, "%9999999"},
		{"80961898", YT_SCORE_FIELD_PORTS, "%100E+05"},
		{"3d0ad7a370fd4787", YT_SCORE_FIELD_PERCENT, " 99.99"},
		{"3e0ad7a370fd4787", YT_SCORE_FIELD_PERCENT, "%100.00"},
		{"b81e85ebff237494", YT_SCORE_FIELD_PERCENT, "%999999.99"},
		{"b91e85ebff237494", YT_SCORE_FIELD_PERCENT, "%10.00D+05"},
		{"47e17a14aeff798a", YT_SCORE_FIELD_XANNOR_PERCENT,
		    " 999.99"},
		{"48e17a14aeff798a", YT_SCORE_FIELD_XANNOR_PERCENT,
		    "%1000.00"},
		{"eb51b8fe7f961898", YT_SCORE_FIELD_XANNOR_PERCENT,
		    "%9999999.99"},
		{"ec51b8fe7f961898", YT_SCORE_FIELD_XANNOR_PERCENT,
		    "%100.00D+05"},
		{"fffffb3fb7433aa5", YT_SCORE_FIELD_SCORE,
		    " 99,999,999,999"},
		{"0000fc3fb7433aa5", YT_SCORE_FIELD_SCORE,
		    "%100,000,000,000"},
		{"fffe7ff420e635af", YT_SCORE_FIELD_SCORE,
		    "%99,999,999,999,999"},
		{"00ff7ff420e635af", YT_SCORE_FIELD_SCORE,
		    "%10000000000000D+01"},
		{"b7433aa5", YT_SCORE_FIELD_SCORE, " 99,999,998,000"},
		{"b8433aa5", YT_SCORE_FIELD_SCORE, "%100,000,006,000"},
		{"20e635af", YT_SCORE_FIELD_SCORE,
		    "%99,999,992,000,000"},
		{"21e635af", YT_SCORE_FIELD_SCORE,
		    "%10000000000000E+01"},
		{"82000099", YT_SCORE_FIELD_SCORE, "     16,777,476"},
		{"820000a6", YT_SCORE_FIELD_SCORE, "%137,441,083,000"},
		{"820000b0", YT_SCORE_FIELD_SCORE,
		    "%14073966900000E+01"},
		{"0000c787", YT_SCORE_FIELD_RANK, "%-100"},
		{"00e0f98a", YT_SCORE_FIELD_PORTS, "%-1000"},
		{"3e0ad7a370fdc787", YT_SCORE_FIELD_PERCENT, "%-100.00"},
		{"0000fc3fb743baa5", YT_SCORE_FIELD_SCORE,
		    "%-100,000,000,000"},
		{"f823f494", YT_SCORE_FIELD_RANK, "%-10E+05"},
		{"b91e85ebff23f494", YT_SCORE_FIELD_PERCENT,
		    "%-10.00D+05"},
		{"00ff7ff420e6b5af", YT_SCORE_FIELD_SCORE,
		    "%-10000000000000D+01"},
		{"21e6b5af", YT_SCORE_FIELD_SCORE,
		    "%-10000000000000E+01"}
	};
	size_t index;
	char actual[40];

	for (index = 0; index < sizeof(vectors) / sizeof(vectors[0]);
	    ++index) {
		if (!check_format(vectors[index].hex, vectors[index].field,
		    vectors[index].expected))
			return false;
	}
	return yt_score_format_double(actual, sizeof(actual), -0.0049,
	    YT_SCORE_FIELD_PERCENT)
	    && strcmp(actual, " -0.00") == 0;
}

static bool
check_formatter_sweep(void)
{
	uint64_t hash32 = UINT64_C(14695981039346656037);
	uint64_t hash64 = UINT64_C(14695981039346656037);
	uint32_t state32 = UINT32_C(0x31415926);
	uint64_t state64 = UINT64_C(0x2718281828459045);
	char rendered[160];
	unsigned iteration;

	for (iteration = 0; iteration < 4096U; ++iteration) {
		uint8_t raw[4];
		unsigned field;
		unsigned index;

		state32 ^= state32 << 13;
		state32 ^= state32 >> 17;
		state32 ^= state32 << 5;
		for (index = 0; index < 4U; ++index)
			raw[index] = (uint8_t)(state32 >> (index * 8U));
		for (field = 0; field <= (unsigned)YT_SCORE_FIELD_SCORE; ++field) {
			if (!yt_score_format_mbf(rendered, sizeof(rendered), raw,
			    sizeof(raw), (enum yt_score_field)field))
				return false;
			hash32 = hash_text(hash32, rendered);
		}
	}
	for (iteration = 0; iteration < 4096U; ++iteration) {
		uint8_t raw[8];
		unsigned field;
		unsigned index;

		state64 ^= state64 << 13;
		state64 ^= state64 >> 7;
		state64 ^= state64 << 17;
		for (index = 0; index < 8U; ++index)
			raw[index] = (uint8_t)(state64 >> (index * 8U));
		for (field = 0; field <= (unsigned)YT_SCORE_FIELD_SCORE; ++field) {
			if (!yt_score_format_mbf(rendered, sizeof(rendered), raw,
			    sizeof(raw), (enum yt_score_field)field))
				return false;
			hash64 = hash_text(hash64, rendered);
		}
	}
	return hash32 == UINT64_C(0xe7a35ef075efc336)
	    && hash64 == UINT64_C(0xd994fd9ba5bf8346);
}

static bool
check_datetime_format(void)
{
	static const int month_days[] =
	    {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	char rendered[11];
	unsigned dates = 0;
	int year;
	int hour;

	for (year = 1980; year <= 2099; ++year) {
		int month;

		for (month = 1; month <= 12; ++month) {
			int days = month_days[month]
			    + (month == 2 && year % 4 == 0
			    && (year % 100 != 0 || year % 400 == 0));
			int day;

			for (day = 1; day <= days; ++day) {
				struct yt_clock_value value =
				    {year, month, day, 0, 0, 0, 0};

				yt_format_date(&value, rendered);
				if (rendered[0] != '0' + month / 10
				    || rendered[1] != '0' + month % 10
				    || rendered[2] != '-'
				    || rendered[3] != '0' + day / 10
				    || rendered[4] != '0' + day % 10
				    || rendered[5] != '-'
				    || rendered[6] != '0' + year / 1000
				    || rendered[7] != '0' + year / 100 % 10
				    || rendered[8] != '0' + year / 10 % 10
				    || rendered[9] != '0' + year % 10
				    || rendered[10] != '\0')
					return false;
				++dates;
			}
		}
	}
	if (dates != 43830U)
		return false;
	for (hour = 0; hour < 24; ++hour) {
		int minute;

		for (minute = 0; minute < 60; ++minute) {
			int second;

			for (second = 0; second < 60; ++second) {
				int hundredth;

				for (hundredth = 0; hundredth < 100; ++hundredth) {
					struct yt_clock_value value =
					    {2000, 1, 1, hour, minute, second, hundredth};
					int rounded = hour * 3600 + minute * 60 + second
					    + (hundredth >= 50 ? 1 : 0);

					yt_format_time(&value, rendered);
					if (rendered[0] != '0' + rounded / 36000
					    || rendered[1] != '0' + rounded / 3600 % 10
					    || rendered[2] != ':'
					    || rendered[3] != '0' + rounded / 600 % 6
					    || rendered[4] != '0' + rounded / 60 % 10
					    || rendered[5] != ':'
					    || rendered[6] != '0' + rounded / 10 % 6
					    || rendered[7] != '0' + rounded % 10
					    || rendered[8] != '\0')
						return false;
				}
			}
		}
	}
	return true;
}

struct planet_updater_tape {
	enum yt_planet_updater_stage events[YT_PLANET_UPDATER_STAGE_COUNT];
	size_t event_count;
	size_t fail_at;
	struct yt_record stored;
	uint8_t day_raw[4];
	uint8_t timer_raw[4];
	bool has_record;
};

static bool
planet_updater_test_error(struct yt_error *error, const char *operation)
{
	if (error != NULL) {
		error->status = YT_IO_ERROR;
		error->system_error = 0;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
		error->path[0] = '\0';
	}
	return false;
}

static bool
planet_updater_step(struct planet_updater_tape *tape,
    enum yt_planet_updater_stage stage, struct yt_error *error)
{
	if (tape->event_count >= YT_ARRAY_LEN(tape->events))
		return planet_updater_test_error(error, "updater tape overflow");
	tape->events[tape->event_count++] = stage;
	if (tape->event_count == tape->fail_at)
		return planet_updater_test_error(error,
		    yt_planet_updater_stage_name(stage));
	return true;
}

static bool
planet_updater_date_test(void *context, uint8_t current_day_raw[4],
    struct yt_error *error)
{
	struct planet_updater_tape *tape = context;

	if (!planet_updater_step(tape, YT_PLANET_UPDATER_DATE_HELPER, error))
		return false;
	memcpy(current_day_raw, tape->day_raw, 4U);
	return true;
}

static bool
planet_updater_expression_test(void *context, bool closing,
    struct yt_error *error)
{
	struct planet_updater_tape *tape = context;

	return planet_updater_step(tape, closing
	    ? YT_PLANET_UPDATER_CLOSING_RECORD_EXPRESSION
	    : YT_PLANET_UPDATER_OPENING_RECORD_EXPRESSION, error);
}

static bool
planet_updater_get_test(void *context, uint32_t physical_record,
    struct yt_record *record, struct yt_error *error)
{
	struct planet_updater_tape *tape = context;

	if (!planet_updater_step(tape, YT_PLANET_UPDATER_GET, error))
		return false;
	if (!tape->has_record || physical_record != 2001U)
		return planet_updater_test_error(error, "planet updater GET #1");
	*record = tape->stored;
	return true;
}

static bool
planet_updater_timer_test(void *context, uint8_t timer_seconds_raw[4],
    struct yt_error *error)
{
	struct planet_updater_tape *tape = context;

	if (!planet_updater_step(tape, YT_PLANET_UPDATER_TIMER, error))
		return false;
	memcpy(timer_seconds_raw, tape->timer_raw, 4U);
	return true;
}

static bool
planet_updater_lset_test(void *context,
    enum yt_planet_updater_stage stage, size_t offset,
    const uint8_t raw[4], struct yt_error *error)
{
	struct planet_updater_tape *tape = context;

	(void)offset;
	(void)raw;
	return planet_updater_step(tape, stage, error);
}

static bool
planet_updater_put_test(void *context, uint32_t physical_record,
    const struct yt_record *record, struct yt_error *error)
{
	struct planet_updater_tape *tape = context;

	if (!planet_updater_step(tape, YT_PLANET_UPDATER_PUT, error))
		return false;
	if (physical_record != 2001U)
		return planet_updater_test_error(error, "planet updater PUT #1");
	tape->stored = *record;
	return true;
}

static const struct yt_planet_updater_ops planet_updater_test_ops = {
	planet_updater_date_test,
	planet_updater_expression_test,
	planet_updater_get_test,
	planet_updater_timer_test,
	planet_updater_lset_test,
	planet_updater_put_test,
};

static void
planet_updater_record_fixture(struct yt_record *record)
{
	static const uint8_t dirty_zero[4] = {0x00, 0x00, 0x20, 0x00};
	size_t index;

	for (index = 0U; index < sizeof(record->bytes); ++index)
		record->bytes[index] = (uint8_t)((index * 7U + 11U) & 0xffU);
	memcpy(record->bytes, "Haven", 5U);
	(void)yt_record_set_number(record, YT_F41, 100.0f);
	(void)yt_record_set_number(record, YT_F45, 100.0f);
	(void)yt_record_set_number(record, YT_F49, 200.0f);
	(void)yt_record_set_number(record, YT_F53, 300.0f);
	(void)yt_record_set_number(record, YT_F57, 1000.0f);
	(void)yt_record_set_number(record, YT_F61, 2000.0f);
	(void)yt_record_set_number(record, YT_F65, 3000.0f);
	(void)yt_record_set_raw_number(record, YT_F69, dirty_zero);
	(void)yt_record_set_number(record, YT_F77, 12.75f);
	(void)yt_record_set_number(record, YT_F89, 60.0f);
	(void)yt_record_set_number(record, YT_F113, 2.0f);
	(void)yt_record_set_number(record, YT_F117, 10000.5f);
	(void)yt_record_set_raw_number(record, YT_F125, dirty_zero);
	(void)yt_record_set_number(record, YT_F129, 30.0f);
}

static bool
planet_updater_hex(const char *hex, uint8_t *raw, size_t length)
{
	size_t index;

	for (index = 0U; index < length; ++index) {
		unsigned high;
		unsigned low;
		char first = hex[index * 2U];
		char second = hex[index * 2U + 1U];

		high = first >= '0' && first <= '9' ? (unsigned)(first - '0')
		    : first >= 'a' && first <= 'f'
		    ? (unsigned)(first - 'a' + 10) : 16U;
		low = second >= '0' && second <= '9' ? (unsigned)(second - '0')
		    : second >= 'a' && second <= 'f'
		    ? (unsigned)(second - 'a' + 10) : 16U;
		if (high > 15U || low > 15U)
			return false;
		raw[index] = (uint8_t)((high << 4U) | low);
	}
	return hex[length * 2U] == '\0';
}

static void
planet_updater_fixture(struct planet_updater_tape *tape,
    struct yt_planet_updater_state *state, const struct yt_record *record,
    float day, float timer)
{
	memset(tape, 0, sizeof(*tape));
	tape->stored = *record;
	tape->has_record = true;
	tape->fail_at = SIZE_MAX;
	(void)qb_mbf32_encode(day, tape->day_raw);
	(void)qb_mbf32_encode(timer, tape->timer_raw);
	memset(state, 0, sizeof(*state));
	memset(state->field.bytes, 0x55, sizeof(state->field.bytes));
	(void)qb_mbf32_encode(1.0f, state->logical_planet_raw);
	(void)qb_mbf32_encode(2000.0f, state->planet_offset_raw);
}

static bool
check_planet_updater_transaction(void)
{
	static const enum yt_planet_updater_stage expected_events[] = {
		YT_PLANET_UPDATER_DATE_HELPER,
		YT_PLANET_UPDATER_OPENING_RECORD_EXPRESSION,
		YT_PLANET_UPDATER_GET,
		YT_PLANET_UPDATER_TIMER,
		YT_PLANET_UPDATER_LSET_DAY,
		YT_PLANET_UPDATER_LSET_BASE_ORE,
		YT_PLANET_UPDATER_LSET_BASE_ORGANICS,
		YT_PLANET_UPDATER_LSET_BASE_EQUIPMENT,
		YT_PLANET_UPDATER_LSET_STOCK_ORE,
		YT_PLANET_UPDATER_LSET_STOCK_ORGANICS,
		YT_PLANET_UPDATER_LSET_STOCK_EQUIPMENT,
		YT_PLANET_UPDATER_LSET_MISSILES,
		YT_PLANET_UPDATER_LSET_FORCES,
		YT_PLANET_UPDATER_LSET_MINUTE,
		YT_PLANET_UPDATER_LSET_PLASMA,
		YT_PLANET_UPDATER_LSET_BANK,
		YT_PLANET_UPDATER_LSET_MINES,
		YT_PLANET_UPDATER_LSET_FIGHTERS,
		YT_PLANET_UPDATER_CLOSING_RECORD_EXPRESSION,
		YT_PLANET_UPDATER_PUT,
	};
	static const char expected_hex[] =
	    "486176656e2e353c434a51585f666d747b828990979ea5acb3bac1c8cfd6dde4"
	    "ebf2f900070e151c2300004a8739425d87391a5d881dce258964490a8b64300a"
	    "8ca4414f8c1058557d0a11181f00005084424950575e656c7300007087969da4"
	    "abb2b9c0c7ced5dce3eaf1f8ff060d141bd406008200e01d8e5a61686fd9ac2"
	    "a7c66f5288aaeb5bcc3";
	static const size_t changed_offsets[] = {
		YT_F41, YT_F45, YT_F49, YT_F53, YT_F57, YT_F61, YT_F65,
		YT_F69, YT_F77, YT_F89, YT_F113, YT_F117, YT_F125, YT_F129,
	};
	static const char *const expected_sites[] = {
		"YT-SUB2:0AA5", "YT-SUB2:0AB3", "YT-SUB2:0AC1",
		"YT-SUB2:0BF6", "YT-SUB2:0EBF", "YT-SUB2:0ECB",
		"YT-SUB2:0EE0", "YT-SUB2:0EF5", "YT-SUB2:0F0A",
		"YT-SUB2:0F1C", "YT-SUB2:0F2E", "YT-SUB2:0F40",
		"YT-SUB2:0F52", "YT-SUB2:0F64", "YT-SUB2:0F70",
		"YT-SUB2:0F82", "YT-SUB2:0F94", "YT-SUB2:0FA6",
		"YT-SUB2:0FBE", "YT-SUB2:0FCC",
	};
	struct yt_record original;
	struct yt_record expected;
	struct yt_record partial;
	struct yt_planet_updater_state state;
	struct planet_updater_tape tape;
	struct yt_error error;
	size_t failure;

	planet_updater_record_fixture(&original);
	if (!planet_updater_hex(expected_hex, expected.bytes,
	    sizeof(expected.bytes)))
		return false;
	for (failure = 0U; failure < YT_ARRAY_LEN(expected_events); ++failure) {
		if (strcmp(yt_planet_updater_stage_site(expected_events[failure]),
		    expected_sites[failure]) != 0)
			return false;
	}
	planet_updater_fixture(&tape, &state, &original, 101.0f, 7200.0f);
	yt_error_clear(&error);
	if (!yt_planet_updater_run(&state, &planet_updater_test_ops, &tape,
	    &error)
	    || tape.event_count != YT_ARRAY_LEN(expected_events)
	    || memcmp(tape.events, expected_events, sizeof(expected_events)) != 0
	    || state.effect_count != YT_ARRAY_LEN(expected_events)
	    || state.completed_effects != YT_ARRAY_LEN(expected_events)
	    || state.physical_record != 2001U || !state.field_loaded
	    || state.field_dirty || !state.written
	    || memcmp(state.field.bytes, expected.bytes,
	    sizeof(expected.bytes)) != 0
	    || memcmp(tape.stored.bytes, expected.bytes,
	    sizeof(expected.bytes)) != 0
	    || memcmp(state.field.bytes + YT_RECORD_TAIL_OFFSET,
	    original.bytes + YT_RECORD_TAIL_OFFSET, YT_RECORD_TAIL_SIZE) != 0
	    || state.cache.elapsed != 1.04166662693023681640625f)
		return false;
	for (failure = 1U; failure <= YT_ARRAY_LEN(expected_events); ++failure) {
		size_t completed_lsets;
		size_t write;

		planet_updater_fixture(&tape, &state, &original, 101.0f, 7200.0f);
		tape.fail_at = failure;
		yt_error_clear(&error);
		if (yt_planet_updater_run(&state, &planet_updater_test_ops,
		    &tape, &error) || error.status != YT_IO_ERROR
		    || tape.event_count != failure || state.effect_count != failure
		    || state.completed_effects != failure - 1U
		    || state.stage != expected_events[failure - 1U]
		    || memcmp(tape.stored.bytes, original.bytes,
		    sizeof(original.bytes)) != 0 || state.written
		    || state.field_loaded != (failure >= 4U)
		    || state.field_dirty != (failure >= 6U))
			return false;
		memset(partial.bytes, 0x55, sizeof(partial.bytes));
		if (failure >= 4U)
			partial = original;
		completed_lsets = failure > 5U ? failure - 5U : 0U;
		if (completed_lsets > YT_ARRAY_LEN(changed_offsets))
			completed_lsets = YT_ARRAY_LEN(changed_offsets);
		for (write = 0U; write < completed_lsets; ++write)
			memcpy(partial.bytes + changed_offsets[write],
			    expected.bytes + changed_offsets[write], 4U);
		if (memcmp(state.field.bytes, partial.bytes,
		    sizeof(partial.bytes)) != 0)
			return false;
	}

	planet_updater_fixture(&tape, &state, &original, 101.0f, 7200.0f);
	tape.has_record = false;
	yt_error_clear(&error);
	if (yt_planet_updater_run(&state, &planet_updater_test_ops, &tape,
	    &error) || tape.event_count != 3U
	    || state.stage != YT_PLANET_UPDATER_GET || state.field_loaded
	    || memcmp(tape.stored.bytes, original.bytes,
	    sizeof(original.bytes)) != 0)
		return false;

	planet_updater_record_fixture(&original);
	(void)yt_record_set_number(&original, YT_F41, 100.0f);
	(void)yt_record_set_number(&original, YT_F89, 60.0f);
	(void)yt_record_set_number(&original, YT_F77, 0.6f);
	(void)yt_record_set_number(&original, YT_F117, 0.6f);
	planet_updater_fixture(&tape, &state, &original, 100.0f, 3600.0f);
	yt_error_clear(&error);
	if (!yt_planet_updater_run(&state, &planet_updater_test_ops, &tape,
	    &error)
	    || memcmp(tape.stored.bytes + YT_F77, "\x9a\x99\x19\0", 4U) != 0
	    || memcmp(tape.stored.bytes + YT_F117, "\x9a\x99\x19\0", 4U) != 0)
		return false;
	partial = tape.stored;
	planet_updater_fixture(&tape, &state, &partial, 100.0f, 3600.0f);
	yt_error_clear(&error);
	if (!yt_planet_updater_run(&state, &planet_updater_test_ops, &tape,
	    &error)
	    || memcmp(tape.stored.bytes + YT_F77, "\x9a\x99\x19\0", 4U) != 0
	    || memcmp(tape.stored.bytes + YT_F117, "\x9a\x99\x19\0", 4U) != 0)
		return false;

	planet_updater_record_fixture(&original);
	planet_updater_fixture(&tape, &state, &original, 100.0f, 3600.0f);
	yt_error_clear(&error);
	if (!yt_planet_updater_run(&state, &planet_updater_test_ops, &tape,
	    &error) || state.cache.elapsed != 0.0f
	    || memcmp(tape.stored.bytes + YT_F69, "\0\0\x20\0", 4U) != 0
	    || memcmp(tape.stored.bytes + YT_F125, "\0\0\x20\0", 4U) != 0)
		return false;

	planet_updater_record_fixture(&original);
	(void)yt_record_set_number(&original, YT_F41, 100.0f);
	(void)yt_record_set_number(&original, YT_F45, 0.0f);
	(void)yt_record_set_number(&original, YT_F49, 0.0f);
	(void)yt_record_set_number(&original, YT_F53, 0.0f);
	(void)yt_record_set_number(&original, YT_F57, 0.0f);
	(void)yt_record_set_number(&original, YT_F61, 0.0f);
	(void)yt_record_set_number(&original, YT_F65, 0.0f);
	(void)yt_record_set_number(&original, YT_F77, 0.0f);
	(void)yt_record_set_number(&original, YT_F89, 60.0f);
	(void)yt_record_set_number(&original, YT_F113, 1.1f);
	(void)yt_record_set_number(&original, YT_F117, 10000.0f);
	(void)yt_record_set_number(&original, YT_F129, 0.0f);
	planet_updater_fixture(&tape, &state, &original, 100.0f, 3600.0f);
	yt_error_clear(&error);
	if (!yt_planet_updater_run(&state, &planet_updater_test_ops, &tape,
	    &error)
	    || memcmp(tape.stored.bytes + YT_F45, "\xcd\xcc\x0c\0", 4U) != 0
	    || memcmp(tape.stored.bytes + YT_F49, "\xcd\xcc\x0c\0", 4U) != 0
	    || memcmp(tape.stored.bytes + YT_F53, "\xcd\xcc\x0c\0", 4U) != 0)
		return false;

	planet_updater_record_fixture(&original);
	(void)yt_record_set_number(&original, YT_F41, 100.0f);
	(void)yt_record_set_number(&original, YT_F45, 10.0f);
	(void)yt_record_set_number(&original, YT_F49, 0.0f);
	(void)yt_record_set_number(&original, YT_F53, 0.0f);
	(void)yt_record_set_number(&original, YT_F57, 100.0f);
	(void)yt_record_set_number(&original, YT_F61, 0.0f);
	(void)yt_record_set_number(&original, YT_F65, 0.0f);
	(void)yt_record_set_number(&original, YT_F77, 0.0f);
	(void)yt_record_set_number(&original, YT_F89, 60.0f);
	(void)yt_record_set_number(&original, YT_F113, 0.0f);
	(void)yt_record_set_number(&original, YT_F117, 0.0f);
	(void)yt_record_set_number(&original, YT_F129, 0.0f);
	planet_updater_fixture(&tape, &state, &original, 100.0f, 3600.0f);
	yt_error_clear(&error);
	if (!yt_planet_updater_run(&state, &planet_updater_test_ops, &tape,
	    &error) || yt_record_get_number(&tape.stored, YT_F45) != 10.0f)
		return false;
	(void)yt_record_set_number(&original, YT_F57, 101.0f);
	planet_updater_fixture(&tape, &state, &original, 100.0f, 3600.0f);
	yt_error_clear(&error);
	if (!yt_planet_updater_run(&state, &planet_updater_test_ops, &tape,
	    &error) || yt_record_get_number(&tape.stored, YT_F45) != 10.1f)
		return false;

	planet_updater_record_fixture(&original);
	(void)yt_record_set_number(&original, YT_F41, 100.0f);
	(void)yt_record_set_number(&original, YT_F45, 2499.0f);
	(void)yt_record_set_number(&original, YT_F49, 0.0f);
	(void)yt_record_set_number(&original, YT_F53, 0.0f);
	(void)yt_record_set_number(&original, YT_F57, 0.0f);
	(void)yt_record_set_number(&original, YT_F61, 0.0f);
	(void)yt_record_set_number(&original, YT_F65, 0.0f);
	(void)yt_record_set_number(&original, YT_F77, 0.0f);
	(void)yt_record_set_number(&original, YT_F89, 0.0f);
	(void)yt_record_set_number(&original, YT_F113, 0.0f);
	(void)yt_record_set_number(&original, YT_F117, 0.0f);
	(void)yt_record_set_number(&original, YT_F129, 0.0f);
	planet_updater_fixture(&tape, &state, &original, 110.0f, 0.0f);
	yt_error_clear(&error);
	if (!yt_planet_updater_run(&state, &planet_updater_test_ops, &tape,
	    &error) || state.cache.elapsed != 10.0f
	    || memcmp(tape.stored.bytes + YT_F69, "\0\0\x20\0", 4U) != 0)
		return false;
	planet_updater_fixture(&tape, &state, &original, 110.0f, 60.0f);
	yt_error_clear(&error);
	if (!yt_planet_updater_run(&state, &planet_updater_test_ops, &tape,
	    &error) || state.cache.elapsed != 10.0f)
		return false;
	planet_updater_fixture(&tape, &state, &original, 99.0f, 0.0f);
	yt_error_clear(&error);
	if (!yt_planet_updater_run(&state, &planet_updater_test_ops, &tape,
	    &error) || state.cache.elapsed != 10.0f)
		return false;

	planet_updater_record_fixture(&original);
	memcpy(original.bytes + YT_F45, "\x01\x02\x03\0", 4U);
	planet_updater_fixture(&tape, &state, &original, 101.0f, 7200.0f);
	yt_error_clear(&error);
	if (yt_planet_updater_run(&state, &planet_updater_test_ops, &tape,
	    &error) || error.status != YT_RANGE || tape.event_count != 3U
	    || state.completed_effects != 3U || !state.field_loaded
	    || memcmp(tape.stored.bytes, original.bytes,
	    sizeof(original.bytes)) != 0)
		return false;
	return true;
}

static void
port_market_fixture(struct yt_port_market_state *state, float stored_day,
    const float stock[3], const float production[3])
{
	struct yt_record record;
	static const float factors[3] = {-60.0f, 74.0f, -66.0f};
	size_t index;

	memset(state, 0, sizeof(*state));
	memset(record.bytes, 0x6d, sizeof(record.bytes));
	memset(record.bytes, ' ', YT_TEXT_FIELD_SIZE);
	memcpy(record.bytes, "Argus", 5U);
	(void)yt_record_set_number(&record, YT_F45, stored_day);
	for (index = 0U; index < 3U; ++index) {
		(void)yt_record_set_number(&record, YT_F49 + index * 4U,
		    stock[index]);
		(void)yt_record_set_number(&record, YT_F61 + index * 4U,
		    production[index]);
		(void)yt_record_set_number(&record, YT_F73 + index * 4U,
		    factors[index]);
	}
	(void)yt_record_set_number(&record, YT_F85, 5.0f);
	(void)yt_record_set_number(&record, YT_F89, 1234.5f);
	(void)yt_record_set_number(&record, YT_F93, 7.0f);
	(void)yt_record_set_number(&record, YT_F97, 2.0f);
	(void)yt_record_set_number(&record, YT_F101, 600.0f);
	yt_port_decode(&state->port, &record);
	state->current_day = 1000.0f;
	state->timer_seconds = 36000.0f;
	state->base_price[0] = 20.0f;
	state->base_price[1] = 30.0f;
	state->base_price[2] = 40.0f;
}

static bool
check_port_market_update(void)
{
	static const float stock[3] = {100.0f, 200.0f, 300.0f};
	static const float production[3] = {5.0f, 10.0f, 15.0f};
	static const float expected_capacity[3] = {105.0f, 210.0f, 315.0f};
	static const float expected_production[3] = {10.5f, 21.0f, 31.5f};
	static const float expected_price[3] = {32.0f, 8.0f, 66.0f};
	struct yt_port_market_state state;
	struct yt_record expected;
	struct yt_error error;
	uint8_t exact_capacity[8];
	size_t index;

	port_market_fixture(&state, 999.0f, stock, production);
	expected = state.port.record;
	(void)yt_record_set_number(&expected, YT_F45, 1000.0f);
	(void)yt_record_set_number(&expected, YT_F101, 600.0f);
	for (index = 0U; index < 3U; ++index) {
		(void)yt_record_set_number(&expected, YT_F49 + index * 4U,
		    expected_capacity[index]);
		(void)yt_record_set_number(&expected, YT_F61 + index * 4U,
		    expected_production[index]);
	}
	if (!yt_port_market_update(&state, NULL) || !state.complete
	    || state.completed_items != 3U || state.current_minute != 600.0f
	    || state.elapsed != 1.0f
	    || memcmp(state.port.record.bytes, expected.bytes,
	    sizeof(expected.bytes)) != 0)
		return false;
	for (index = 0U; index < 3U; ++index) {
		if (state.capacity[index] != (double)expected_capacity[index]
		    || state.port.production[index] != expected_production[index]
		    || state.price[index] != expected_price[index]
		    || !state.production_raised[index])
			return false;
	}

	port_market_fixture(&state, 999.0f, stock, production);
	expected = state.port.record;
	(void)yt_record_set_number(&expected, YT_F49, 16777216.0f);
	(void)yt_record_set_number(&expected, YT_F61, 1.0f);
	yt_port_decode(&state.port, &expected);
	if (qb_mbf64_encode(16777217.0, exact_capacity) != QB_MBF_OK
	    || !yt_port_market_update(&state, NULL)
	    || memcmp(state.capacity_raw[0], exact_capacity,
	    sizeof(exact_capacity)) != 0
	    || state.capacity[0] != 16777217.0)
		return false;

	port_market_fixture(&state, 999.0f, stock, production);
	expected = state.port.record;
	(void)yt_record_set_number(&expected, YT_F49, 0.0f);
	(void)yt_record_set_number(&expected, YT_F61, 0.0f);
	yt_port_decode(&state.port, &expected);
	yt_error_clear(&error);
	if (yt_port_market_update(&state, &error) || error.status != YT_RANGE
	    || state.complete || state.completed_items != 0U
	    || memcmp(state.port.record.bytes, expected.bytes,
	    sizeof(expected.bytes)) != 0)
		return false;

	port_market_fixture(&state, 900.0f, stock, production);
	if (!yt_port_market_update(&state, NULL) || state.elapsed != 10.0f)
		return false;
	port_market_fixture(&state, 1001.0f, stock, production);
	return yt_port_market_update(&state, NULL) && state.elapsed == 10.0f;
}

enum commodity_trade_event {
	COMMODITY_TRADE_READ_PLAYER = 1,
	COMMODITY_TRADE_WRITE_PLAYER,
	COMMODITY_TRADE_READ_PORT,
	COMMODITY_TRADE_WRITE_PORT,
};
struct commodity_trade_fragment {
	enum yt_commodity_trade_output_kind kind;
	uint8_t text[256];
	size_t length;
};
struct commodity_trade_tape {
	enum commodity_trade_event events[12];
	size_t event_count;
	size_t dependency_calls;
	size_t fail_at;
	size_t boundary_calls;
	size_t boundary_fail_at;
	struct yt_player player_reads[3];
	size_t player_read_count;
	struct yt_port port_reads[2];
	size_t port_read_count;
	struct yt_player written_players[2];
	size_t player_write_count;
	struct yt_port written_ports[2];
	size_t port_write_count;
	struct commodity_trade_fragment fragments[16];
	size_t fragment_count;
	const char *responses[3];
	size_t response_count;
	size_t response_index;
	bool accepted;
};

static bool
commodity_trade_test_boundary(struct commodity_trade_tape *tape,
    struct yt_error *error)
{
	size_t call = tape->boundary_calls++;

	if (call != tape->boundary_fail_at)
		return true;
	if (error != NULL)
		error->status = YT_IO_ERROR;
	return false;
}

static bool
commodity_trade_test_step(struct commodity_trade_tape *tape,
    enum commodity_trade_event event, struct yt_error *error)
{
	size_t call = tape->dependency_calls++;

	if (tape->event_count < YT_ARRAY_LEN(tape->events))
		tape->events[tape->event_count++] = event;
	if (call != tape->fail_at)
		return true;
	if (error != NULL)
		error->status = YT_IO_ERROR;
	return false;
}

static bool
commodity_trade_test_read_player(void *context, uint32_t physical_record,
    struct yt_player *player, struct yt_error *error)
{
	struct commodity_trade_tape *tape = context;

	if (physical_record != 2U
	    || tape->player_read_count >= YT_ARRAY_LEN(tape->player_reads)
	    || !commodity_trade_test_step(tape,
	    COMMODITY_TRADE_READ_PLAYER, error))
		return false;
	*player = tape->player_reads[tape->player_read_count++];
	return true;
}

static bool
commodity_trade_test_write_player(void *context, uint32_t physical_record,
    const struct yt_player *player, struct yt_error *error)
{
	struct commodity_trade_tape *tape = context;

	if (physical_record != 2U
	    || tape->player_write_count >= YT_ARRAY_LEN(tape->written_players)
	    || !commodity_trade_test_step(tape,
	    COMMODITY_TRADE_WRITE_PLAYER, error))
		return false;
	tape->written_players[tape->player_write_count++] = *player;
	return true;
}

static bool
commodity_trade_test_mutate_credits(void *context, float player_record,
    float argument, struct yt_player *player, bool *hydrated,
    struct yt_error *error)
{
	struct yt_player fresh;

	*hydrated = false;
	if (player_record != 2.0f
	    || !commodity_trade_test_read_player(context, 2U, &fresh, error))
		return false;
	*hydrated = true;
	yt_trade_credit_overlay(&fresh, argument);
	*player = fresh;
	return commodity_trade_test_write_player(context, 2U, &fresh, error);
}

static bool
commodity_trade_test_read_port(void *context, uint32_t physical_record,
    struct yt_port *port, struct yt_error *error)
{
	struct commodity_trade_tape *tape = context;

	if (physical_record != 2057U
	    || tape->port_read_count >= YT_ARRAY_LEN(tape->port_reads)
	    || !commodity_trade_test_step(tape,
	    COMMODITY_TRADE_READ_PORT, error))
		return false;
	*port = tape->port_reads[tape->port_read_count++];
	return true;
}

static bool
commodity_trade_test_write_port(void *context, uint32_t physical_record,
    const struct yt_port *port, struct yt_error *error)
{
	struct commodity_trade_tape *tape = context;

	if (physical_record != 2057U
	    || tape->port_write_count >= YT_ARRAY_LEN(tape->written_ports)
	    || !commodity_trade_test_step(tape,
	    COMMODITY_TRADE_WRITE_PORT, error))
		return false;
	tape->written_ports[tape->port_write_count++] = *port;
	return true;
}

static bool
commodity_trade_test_present(void *context, const uint8_t *text,
    size_t length, enum yt_commodity_trade_output_kind kind,
    struct yt_error *error)
{
	struct commodity_trade_tape *tape = context;
	struct commodity_trade_fragment *fragment;

	if (tape->fragment_count >= YT_ARRAY_LEN(tape->fragments)
	    || length > sizeof(tape->fragments[0].text)
	    || !commodity_trade_test_boundary(tape, error))
		return false;
	fragment = &tape->fragments[tape->fragment_count++];
	fragment->kind = kind;
	fragment->length = length;
	if (length != 0U)
		memcpy(fragment->text, text, length);
	return true;
}

static bool
commodity_trade_test_input(void *context, char *response, size_t capacity,
    struct yt_error *error)
{
	struct commodity_trade_tape *tape = context;
	const char *source;

	if (tape->response_index >= tape->response_count
	    || !commodity_trade_test_boundary(tape, error))
		return false;
	source = tape->responses[tape->response_index++];
	if (strlen(source) >= capacity)
		return false;
	strcpy(response, source);
	return true;
}

static bool
commodity_trade_test_confirm(void *context, const uint8_t *prompt,
    size_t length, bool *accepted, struct yt_error *error)
{
	static const uint8_t expected[] = "Do you agree? [Y/n] ";
	struct commodity_trade_tape *tape = context;

	if (length != sizeof(expected) - 1U
	    || memcmp(prompt, expected, length) != 0
	    || !commodity_trade_test_boundary(tape, error))
		return false;
	*accepted = tape->accepted;
	return true;
}

static const struct yt_commodity_trade_ops commodity_trade_test_ops = {
	commodity_trade_test_read_player,
	commodity_trade_test_write_player,
	commodity_trade_test_mutate_credits,
	commodity_trade_test_read_port,
	commodity_trade_test_write_port,
	commodity_trade_test_present,
	commodity_trade_test_input,
	commodity_trade_test_confirm,
};

static void
commodity_trade_player(struct yt_player *player, uint8_t pattern,
    float credits, float ore)
{
	struct yt_record record;

	memset(record.bytes, pattern, sizeof(record.bytes));
	(void)yt_record_set_number(&record, YT_F65, 100.0f);
	(void)yt_record_set_number(&record, YT_F69, ore);
	(void)yt_record_set_number(&record, YT_F73, 20.0f);
	(void)yt_record_set_number(&record, YT_F77, 5.0f);
	(void)yt_record_set_number(&record, YT_F81, credits);
	yt_player_decode(player, &record);
}

static void
commodity_trade_fixture(struct commodity_trade_tape *tape,
    struct yt_commodity_trade_state *state)
{
	struct yt_record record;
	size_t index;
	static const float stock[3] = {100.0f, 80.0f, 60.0f};
	static const float factors[3] = {66.0f, -74.0f, -60.0f};
	static const float prices[3] = {20.0f, 30.0f, 40.0f};

	memset(tape, 0, sizeof(*tape));
	memset(state, 0, sizeof(*state));
	tape->fail_at = SIZE_MAX;
	tape->boundary_fail_at = SIZE_MAX;
	tape->accepted = true;
	tape->responses[0] = "3";
	tape->response_count = 1U;
	commodity_trade_player(&tape->player_reads[0], 0x11, 12345.0f, 10.0f);
	commodity_trade_player(&tape->player_reads[1], 0x22, 20000.0f, 10.0f);
	commodity_trade_player(&tape->player_reads[2], 0x33, 19940.0f, 40.0f);
	memset(record.bytes, 0x44, sizeof(record.bytes));
	for (index = 0U; index < 3U; ++index) {
		(void)yt_record_set_number(&record, YT_F49 + index * 4U,
		    stock[index]);
		(void)yt_record_set_number(&record, YT_F73 + index * 4U,
		    factors[index]);
	}
	(void)yt_record_set_number(&record, YT_F89, 1000.0f);
	(void)yt_record_set_number(&record, YT_F97, 8.0f);
	yt_port_decode(&state->market.port, &record);
	tape->port_reads[0] = state->market.port;
	tape->port_reads[1] = state->market.port;
	record = tape->port_reads[1].record;
	(void)yt_record_set_number(&record, YT_F89, 1060.0f);
	yt_port_decode(&tape->port_reads[1], &record);
	for (index = 0U; index < 3U; ++index) {
		(void)qb_mbf64_encode((double)stock[index],
		    state->market.capacity_raw[index]);
		state->market.capacity[index] = stock[index];
		state->market.price[index] = prices[index];
	}
	state->current_player_record = 2U;
	state->port_physical_record = 2057U;
	state->commodity = 0U;
}

static bool
commodity_trade_fragment_equal(const struct commodity_trade_fragment *fragment,
    enum yt_commodity_trade_output_kind kind, const char *text)
{
	size_t length = strlen(text);

	return fragment->kind == kind && fragment->length == length
	    && memcmp(fragment->text, text, length) == 0;
}

static bool
check_commodity_trade_transaction(void)
{
	static const enum commodity_trade_event expected[] = {
		COMMODITY_TRADE_READ_PLAYER,
		COMMODITY_TRADE_READ_PORT,
		COMMODITY_TRADE_WRITE_PORT,
		COMMODITY_TRADE_READ_PLAYER,
		COMMODITY_TRADE_WRITE_PLAYER,
		COMMODITY_TRADE_READ_PLAYER,
		COMMODITY_TRADE_WRITE_PLAYER,
		COMMODITY_TRADE_READ_PORT,
		COMMODITY_TRADE_WRITE_PORT,
	};
	static const size_t completed_writes[] = {0U, 0U, 0U, 1U, 1U,
	    2U, 2U, 3U, 3U};
	static const size_t boundary_fragments[] = {0U, 1U, 2U, 3U, 3U,
	    4U, 5U, 5U};
	static const size_t boundary_responses[] = {0U, 0U, 0U, 0U, 1U,
	    1U, 1U, 1U};
	struct commodity_trade_tape tape;
	struct yt_commodity_trade_state state;
	struct yt_error error;
	size_t failure;

	commodity_trade_fixture(&tape, &state);
	if (!yt_commodity_trade_run(&state, &commodity_trade_test_ops, &tape,
	    NULL) || !state.complete
	    || state.route != YT_COMMODITY_TRADE_ACCEPTED
	    || !state.port_sells || state.free_holds != 65.0f
	    || state.maximum != 65.0f || state.quantity != 3.0f
	    || state.total != 60.0f || state.direction != 1.0f
	    || state.credit_delta != -60.0f || !state.prompt_reached
	    || qb_mbf64_decode(state.caller_trade_flag_raw) != 1.0
	    || tape.boundary_calls != 8U
	    || tape.event_count != YT_ARRAY_LEN(expected)
	    || memcmp(tape.events, expected, sizeof(expected)) != 0
	    || tape.fragment_count != 6U
	    || !commodity_trade_fragment_equal(&tape.fragments[0],
	    YT_COMMODITY_TRADE_STATUS,
	    "You have 12345 credits and 65 empty cargo holds.")
	    || !commodity_trade_fragment_equal(&tape.fragments[1],
	    YT_COMMODITY_TRADE_MARKET,
	    "We are selling up to 100.  You have 10 in your holds.")
	    || !commodity_trade_fragment_equal(&tape.fragments[2],
	    YT_COMMODITY_TRADE_QUANTITY_PROMPT,
	    "How many holds of Ore do you want to buy [ 65 ]? ")
	    || !commodity_trade_fragment_equal(&tape.fragments[3],
	    YT_COMMODITY_TRADE_AGREED, "Agreed, 3 units.")
	    || !commodity_trade_fragment_equal(&tape.fragments[4],
	    YT_COMMODITY_TRADE_OFFER,
	    "We'll sell them for 60 credits.")
	    || !commodity_trade_fragment_equal(&tape.fragments[5],
	    YT_COMMODITY_TRADE_SUCCESS, "It's Yours!")
	    || tape.written_ports[0].treasury != 1060.0f
	    || tape.written_players[0].credits != 19940.0f
	    || tape.written_players[1].ore != 43.0f
	    || tape.written_players[1].record.bytes[0] != 0x33
	    || tape.written_ports[1].stock[0] != 97.0f
	    || tape.written_ports[1].treasury != 1060.0f
	    || tape.written_ports[1].record.bytes[0] != 0x44)
		return false;

	for (failure = 0U; failure < YT_ARRAY_LEN(expected); ++failure) {
		size_t writes;

		commodity_trade_fixture(&tape, &state);
		tape.fail_at = failure;
		yt_error_clear(&error);
		if (yt_commodity_trade_run(&state, &commodity_trade_test_ops,
		    &tape, &error) || error.status != YT_IO_ERROR || state.complete
		    || tape.dependency_calls != failure + 1U)
			return false;
		writes = tape.player_write_count + tape.port_write_count;
		if (writes != completed_writes[failure])
			return false;
	}

	for (failure = 0U; failure < YT_ARRAY_LEN(boundary_fragments);
	    ++failure) {
		commodity_trade_fixture(&tape, &state);
		tape.boundary_fail_at = failure;
		yt_error_clear(&error);
		if (yt_commodity_trade_run(&state, &commodity_trade_test_ops,
		    &tape, &error) || error.status != YT_IO_ERROR || state.complete
		    || tape.boundary_calls != failure + 1U
		    || tape.fragment_count != boundary_fragments[failure]
		    || tape.response_index != boundary_responses[failure]
		    || tape.dependency_calls != 1U || tape.player_write_count != 0U
		    || tape.port_write_count != 0U)
			return false;
	}

	commodity_trade_fixture(&tape, &state);
	tape.player_reads[0].holds = 35.0f;
	(void)yt_record_set_number(&tape.player_reads[0].record,
	    YT_F65, 35.0f);
	if (!yt_commodity_trade_run(&state, &commodity_trade_test_ops, &tape,
	    NULL) || state.route != YT_COMMODITY_TRADE_MAXIMUM_ZERO
	    || state.prompt_reached || tape.fragment_count != 0U
	    || tape.dependency_calls != 1U)
		return false;

	commodity_trade_fixture(&tape, &state);
	tape.responses[0] = "12345";
	tape.responses[1] = "2";
	tape.response_count = 2U;
	if (!yt_commodity_trade_run(&state, &commodity_trade_test_ops, &tape,
	    NULL) || state.quantity_attempts != 2U || state.quantity != 2.0f
	    || tape.fragment_count != 7U)
		return false;

	commodity_trade_fixture(&tape, &state);
	(void)qb_mbf64_encode(3.75, state.market.capacity_raw[0]);
	state.market.capacity[0] = 3.75;
	if (!yt_commodity_trade_run(&state, &commodity_trade_test_ops, &tape,
	    NULL) || state.selected_quantity != 3.75
	    || tape.written_ports[1].stock[0] != 0.75f)
		return false;

	/* INT(.5) chooses buying prose while SGN(.5) mutates positively. */
	commodity_trade_fixture(&tape, &state);
	state.market.port.factor[0] = 0.5f;
	(void)yt_record_set_number(&state.market.port.record, YT_F73, 0.5f);
	if (!yt_commodity_trade_run(&state, &commodity_trade_test_ops, &tape,
	    NULL) || state.port_sells || state.direction != 1.0f
	    || state.credit_delta != -60.0f
	    || !commodity_trade_fragment_equal(&tape.fragments[1],
	    YT_COMMODITY_TRADE_MARKET,
	    "We are buying up to 100.  You have 10 in your holds.")
	    || !commodity_trade_fragment_equal(&tape.fragments[5],
	    YT_COMMODITY_TRADE_SUCCESS, "We'll take them!"))
		return false;

	commodity_trade_fixture(&tape, &state);
	tape.responses[0] = "NO";
	if (!yt_commodity_trade_run(&state, &commodity_trade_test_ops, &tape,
	    NULL) || state.route != YT_COMMODITY_TRADE_QUANTITY_CANCEL
	    || state.quantity != 0.0f || tape.fragment_count != 3U
	    || tape.dependency_calls != 1U)
		return false;

	commodity_trade_fixture(&tape, &state);
	tape.responses[0] = "-.1";
	if (!yt_commodity_trade_run(&state, &commodity_trade_test_ops, &tape,
	    NULL) || state.route != YT_COMMODITY_TRADE_QUANTITY_CANCEL
	    || state.quantity != -1.0f || tape.fragment_count != 3U
	    || tape.dependency_calls != 1U)
		return false;

	commodity_trade_fixture(&tape, &state);
	tape.responses[0] = "101";
	if (!yt_commodity_trade_run(&state, &commodity_trade_test_ops, &tape,
	    NULL) || state.route != YT_COMMODITY_TRADE_CAPACITY_REJECTED
	    || tape.fragment_count != 4U
	    || !commodity_trade_fragment_equal(&tape.fragments[3],
	    YT_COMMODITY_TRADE_CAPACITY_ERROR,
	    "We don't have that much!"))
		return false;

	commodity_trade_fixture(&tape, &state);
	state.commodity = 1U;
	tape.responses[0] = "81";
	if (!yt_commodity_trade_run(&state, &commodity_trade_test_ops, &tape,
	    NULL) || state.route != YT_COMMODITY_TRADE_CAPACITY_REJECTED
	    || !commodity_trade_fragment_equal(&tape.fragments[3],
	    YT_COMMODITY_TRADE_CAPACITY_ERROR,
	    "We don't need that much!"))
		return false;

	commodity_trade_fixture(&tape, &state);
	tape.player_reads[0].credits = 100.0f;
	(void)yt_record_set_number(&tape.player_reads[0].record,
	    YT_F81, 100.0f);
	tape.responses[0] = "6";
	if (!yt_commodity_trade_run(&state, &commodity_trade_test_ops, &tape,
	    NULL) || state.maximum != 5.0f
	    || state.route != YT_COMMODITY_TRADE_MAXIMUM_REJECTED
	    || !commodity_trade_fragment_equal(&tape.fragments[3],
	    YT_COMMODITY_TRADE_MAXIMUM_ERROR,
	    "You can't afford that much!"))
		return false;

	commodity_trade_fixture(&tape, &state);
	state.commodity = 1U;
	tape.responses[0] = "21";
	if (!yt_commodity_trade_run(&state, &commodity_trade_test_ops, &tape,
	    NULL) || state.maximum != 20.0f
	    || state.route != YT_COMMODITY_TRADE_MAXIMUM_REJECTED
	    || !commodity_trade_fragment_equal(&tape.fragments[3],
	    YT_COMMODITY_TRADE_MAXIMUM_ERROR,
	    "You don't have that much!"))
		return false;

	/* Corrupt negative price raises maximum above free holds: retry. */
	commodity_trade_fixture(&tape, &state);
	tape.player_reads[0].credits = -100.0f;
	(void)yt_record_set_number(&tape.player_reads[0].record,
	    YT_F81, -100.0f);
	state.market.price[0] = -1.0f;
	tape.responses[0] = "66";
	tape.responses[1] = "2";
	tape.response_count = 2U;
	if (!yt_commodity_trade_run(&state, &commodity_trade_test_ops, &tape,
	    NULL) || state.maximum != 100.0f || state.quantity_attempts != 2U
	    || state.quantity != 2.0f || tape.fragment_count != 9U
	    || tape.fragments[3].kind != YT_COMMODITY_TRADE_FREE_HOLDS_ERROR
	    || tape.fragments[4].kind != YT_COMMODITY_TRADE_FREE_HOLDS_BLANK
	    || tape.fragments[4].length != 0U
	    || tape.fragments[5].kind != YT_COMMODITY_TRADE_QUANTITY_PROMPT)
		return false;

	/* Blank selects buying-port maximum and no treasury mutation occurs. */
	commodity_trade_fixture(&tape, &state);
	state.commodity = 1U;
	tape.responses[0] = "";
	if (!yt_commodity_trade_run(&state, &commodity_trade_test_ops, &tape,
	    NULL) || state.quantity != 20.0f || state.total != 600.0f
	    || state.direction != -1.0f || state.credit_delta != 600.0f
	    || state.treasury_port_read || state.treasury_port_written
	    || tape.player_write_count != 2U || tape.port_write_count != 1U
	    || tape.written_players[1].organics != 0.0f
	    || tape.written_ports[0].stock[1] != 60.0f
	    || !commodity_trade_fragment_equal(&tape.fragments[4],
	    YT_COMMODITY_TRADE_OFFER,
	    "We'll buy them for 600 credits.")
	    || !commodity_trade_fragment_equal(&tape.fragments[5],
	    YT_COMMODITY_TRADE_SUCCESS, "We'll take them!"))
		return false;

	/* Own-port receipt is INT(S(.01 * 100)) = 1. */
	commodity_trade_fixture(&tape, &state);
	state.market.port.owner = 2.0f;
	(void)yt_record_set_number(&state.market.port.record, YT_F97, 2.0f);
	tape.responses[0] = "5";
	if (!yt_commodity_trade_run(&state, &commodity_trade_test_ops, &tape,
	    NULL) || state.total != 100.0f
	    || tape.written_ports[0].treasury != 1001.0f)
		return false;

	/* Unowned selling port skips treasury GET/PUT entirely. */
	commodity_trade_fixture(&tape, &state);
	state.market.port.owner = 0.0f;
	(void)yt_record_set_number(&state.market.port.record, YT_F97, 0.0f);
	if (!yt_commodity_trade_run(&state, &commodity_trade_test_ops, &tape,
	    NULL) || state.treasury_port_read || state.treasury_port_written
	    || tape.port_read_count != 1U || tape.port_write_count != 1U)
		return false;

	/* SGN zero still runs credit/hold/stock persistence with zero deltas. */
	commodity_trade_fixture(&tape, &state);
	state.market.port.factor[0] = 0.0f;
	(void)yt_record_set_number(&state.market.port.record, YT_F73, 0.0f);
	tape.responses[0] = "2";
	if (!yt_commodity_trade_run(&state, &commodity_trade_test_ops, &tape,
	    NULL) || state.port_sells || state.direction != 0.0f
	    || state.credit_delta != 0.0f
	    || tape.written_players[0].credits != 20000.0f
	    || tape.written_players[1].ore != 40.0f
	    || tape.written_ports[0].stock[0] != 98.0f)
		return false;

	commodity_trade_fixture(&tape, &state);
	(void)qb_mbf64_encode(16777217.0,
	    state.market.capacity_raw[0]);
	state.market.capacity[0] = 16777217.0;
	if (!yt_commodity_trade_run(&state, &commodity_trade_test_ops, &tape,
	    NULL) || !commodity_trade_fragment_equal(&tape.fragments[1],
	    YT_COMMODITY_TRADE_MARKET,
	    "We are selling up to 16777217.  You have 10 in your holds."))
		return false;

	commodity_trade_fixture(&tape, &state);
	tape.accepted = false;
	return yt_commodity_trade_run(&state, &commodity_trade_test_ops, &tape,
	    NULL) && state.route == YT_COMMODITY_TRADE_DECLINED_ROUTE
	    && tape.dependency_calls == 1U && tape.fragment_count == 6U
	    && commodity_trade_fragment_equal(&tape.fragments[5],
	    YT_COMMODITY_TRADE_DECLINED, "Never mind!");
}

enum ordinary_commerce_event {
	ORDINARY_COMMERCE_UPDATE = 1,
	ORDINARY_COMMERCE_REPORT,
	ORDINARY_COMMERCE_TRADE_ORE,
	ORDINARY_COMMERCE_TRADE_ORGANICS,
	ORDINARY_COMMERCE_TRADE_EQUIPMENT,
	ORDINARY_COMMERCE_READ_PLAYER,
	ORDINARY_COMMERCE_REFUSAL,
	ORDINARY_COMMERCE_STATUS,
	ORDINARY_COMMERCE_FOREGROUND,
};
struct ordinary_commerce_tape {
	enum ordinary_commerce_event events[12];
	size_t event_count;
	size_t calls;
	size_t fail_at;
	struct yt_port_market_state market;
	struct yt_player player;
	bool reached[3];
	uint8_t rows[2][256];
	size_t row_lengths[2];
	float foreground;
};

static bool
ordinary_commerce_step(struct ordinary_commerce_tape *tape,
    enum ordinary_commerce_event event, struct yt_error *error)
{
	size_t call = tape->calls++;

	if (tape->event_count < YT_ARRAY_LEN(tape->events))
		tape->events[tape->event_count++] = event;
	if (call != tape->fail_at)
		return true;
	if (error != NULL)
		error->status = YT_IO_ERROR;
	return false;
}

static bool
ordinary_commerce_test_update(void *context, int sector_number,
    float sector_record_expression,
    struct yt_port_market_state *market, struct yt_error *error)
{
	struct ordinary_commerce_tape *tape = context;

	if (sector_number != 733 || sector_record_expression != 784.0f
	    || !ordinary_commerce_step(tape, ORDINARY_COMMERCE_UPDATE, error))
		return false;
	*market = tape->market;
	return true;
}

static bool
ordinary_commerce_test_report(void *context,
    const struct yt_port_market_state *market, struct yt_error *error)
{
	struct ordinary_commerce_tape *tape = context;

	if (market->port_physical_record != 2057U)
		return false;
	return ordinary_commerce_step(tape, ORDINARY_COMMERCE_REPORT, error);
}

static bool
ordinary_commerce_test_trade(void *context,
    const struct yt_port_market_state *market, size_t commodity,
    bool *prompt_reached, struct yt_error *error)
{
	struct ordinary_commerce_tape *tape = context;
	enum ordinary_commerce_event event;

	if (market->port_physical_record != 2057U || commodity >= 3U)
		return false;
	event = commodity == 0U ? ORDINARY_COMMERCE_TRADE_ORE
	    : commodity == 1U ? ORDINARY_COMMERCE_TRADE_ORGANICS
	    : ORDINARY_COMMERCE_TRADE_EQUIPMENT;
	if (!ordinary_commerce_step(tape, event, error))
		return false;
	*prompt_reached = tape->reached[commodity];
	return true;
}

static bool
ordinary_commerce_test_read_player(void *context, uint32_t physical_record,
    struct yt_player *player, struct yt_error *error)
{
	struct ordinary_commerce_tape *tape = context;

	if (physical_record != 2U
	    || !ordinary_commerce_step(tape,
	    ORDINARY_COMMERCE_READ_PLAYER, error))
		return false;
	*player = tape->player;
	return true;
}

static bool
ordinary_commerce_test_present(void *context, const uint8_t *text,
    size_t length, enum yt_ordinary_commerce_output_kind kind,
    struct yt_error *error)
{
	struct ordinary_commerce_tape *tape = context;
	enum ordinary_commerce_event event = kind == YT_ORDINARY_COMMERCE_REFUSAL
	    ? ORDINARY_COMMERCE_REFUSAL : ORDINARY_COMMERCE_STATUS;

	if ((size_t)kind >= YT_ARRAY_LEN(tape->rows)
	    || length > sizeof(tape->rows[0])
	    || !ordinary_commerce_step(tape, event, error))
		return false;
	if (length != 0U)
		memcpy(tape->rows[kind], text, length);
	tape->row_lengths[kind] = length;
	return true;
}

static void
ordinary_commerce_test_foreground(void *context, float foreground)
{
	struct ordinary_commerce_tape *tape = context;

	if (tape->event_count < YT_ARRAY_LEN(tape->events))
		tape->events[tape->event_count++] = ORDINARY_COMMERCE_FOREGROUND;
	tape->foreground = foreground;
}

static const struct yt_ordinary_commerce_ops ordinary_commerce_test_ops = {
	ordinary_commerce_test_update,
	ordinary_commerce_test_report,
	ordinary_commerce_test_trade,
	ordinary_commerce_test_read_player,
	ordinary_commerce_test_present,
	ordinary_commerce_test_foreground,
};

static void
ordinary_commerce_fixture(struct ordinary_commerce_tape *tape,
    struct yt_ordinary_commerce_state *state)
{
	struct yt_record record;

	memset(tape, 0, sizeof(*tape));
	memset(state, 0, sizeof(*state));
	tape->fail_at = SIZE_MAX;
	tape->market.port_physical_record = 2057U;
	tape->market.port.factor[0] = -60.0f;
	tape->market.port.factor[1] = 74.0f;
	tape->market.port.factor[2] = -66.0f;
	tape->reached[0] = true;
	tape->reached[1] = true;
	tape->reached[2] = true;
	memset(record.bytes, 0x6b, sizeof(record.bytes));
	(void)yt_record_set_number(&record, YT_F65, 50.0f);
	(void)yt_record_set_number(&record, YT_F69, 10.0f);
	(void)yt_record_set_number(&record, YT_F73, 20.0f);
	(void)yt_record_set_number(&record, YT_F77, 5.0f);
	(void)yt_record_set_number(&record, YT_F81, 777.0f);
	yt_player_decode(&tape->player, &record);
	state->sector_number = 733;
	state->sector_record_expression = 784.0f;
	state->current_player_record = 2U;
	state->first_name = (const uint8_t *)"Pat";
	state->first_name_length = 3U;
}

static bool
ordinary_commerce_row_equal(const struct ordinary_commerce_tape *tape,
    enum yt_ordinary_commerce_output_kind kind, const char *text)
{
	size_t length = strlen(text);

	return tape->row_lengths[kind] == length
	    && memcmp(tape->rows[kind], text, length) == 0;
}

static bool
check_ordinary_commerce_transaction(void)
{
	static const enum ordinary_commerce_event expected[] = {
		ORDINARY_COMMERCE_UPDATE,
		ORDINARY_COMMERCE_REPORT,
		ORDINARY_COMMERCE_TRADE_ORE,
		ORDINARY_COMMERCE_TRADE_EQUIPMENT,
		ORDINARY_COMMERCE_TRADE_ORGANICS,
		ORDINARY_COMMERCE_READ_PLAYER,
		ORDINARY_COMMERCE_STATUS,
	};
	static const enum ordinary_commerce_event refusal_expected[] = {
		ORDINARY_COMMERCE_UPDATE,
		ORDINARY_COMMERCE_REPORT,
		ORDINARY_COMMERCE_FOREGROUND,
		ORDINARY_COMMERCE_REFUSAL,
		ORDINARY_COMMERCE_READ_PLAYER,
		ORDINARY_COMMERCE_STATUS,
	};
	struct ordinary_commerce_tape tape;
	struct yt_ordinary_commerce_state state;
	struct yt_error error;
	size_t failure;

	ordinary_commerce_fixture(&tape, &state);
	if (!yt_ordinary_commerce_run(&state, &ordinary_commerce_test_ops,
	    &tape, NULL) || !state.complete || !state.update_complete
	    || !state.report_complete || state.scheduled_count != 3U
	    || state.completed_trades != 3U || !state.prompt_reached
	    || state.refusal_presented || !state.final_player_read
	    || !state.status_presented
	    || memcmp(state.schedule, (size_t[]){0U, 2U, 1U},
	    sizeof(state.schedule)) != 0
	    || tape.event_count != YT_ARRAY_LEN(expected)
	    || memcmp(tape.events, expected, sizeof(expected)) != 0
	    || !ordinary_commerce_row_equal(&tape,
	    YT_ORDINARY_COMMERCE_STATUS,
	    "You have 777 credits and 15 empty cargo holds."))
		return false;

	for (failure = 0U; failure < YT_ARRAY_LEN(expected); ++failure) {
		ordinary_commerce_fixture(&tape, &state);
		tape.fail_at = failure;
		yt_error_clear(&error);
		if (yt_ordinary_commerce_run(&state,
		    &ordinary_commerce_test_ops, &tape, &error)
		    || error.status != YT_IO_ERROR || state.complete
		    || tape.calls != failure + 1U)
			return false;
	}

	ordinary_commerce_fixture(&tape, &state);
	memset(tape.market.port.factor, 0,
	    sizeof(tape.market.port.factor));
	if (!yt_ordinary_commerce_run(&state, &ordinary_commerce_test_ops,
	    &tape, NULL) || state.scheduled_count != 0U
	    || state.completed_trades != 0U || state.prompt_reached
	    || !state.refusal_presented || tape.foreground != 6.0f
	    || tape.event_count != YT_ARRAY_LEN(refusal_expected)
	    || memcmp(tape.events, refusal_expected,
	    sizeof(refusal_expected)) != 0
	    || !ordinary_commerce_row_equal(&tape,
	    YT_ORDINARY_COMMERCE_REFUSAL,
	    "We don't want your goods and you can't buy ours Pat!"))
		return false;

	/* Scheduled zero-maximum children still complete but do not set flag. */
	ordinary_commerce_fixture(&tape, &state);
	memset(tape.reached, 0, sizeof(tape.reached));
	if (!yt_ordinary_commerce_run(&state, &ordinary_commerce_test_ops,
	    &tape, NULL) || state.completed_trades != 3U
	    || state.prompt_reached || !state.refusal_presented)
		return false;

	/* The refusal output itself is a fallible boundary before final GET. */
	ordinary_commerce_fixture(&tape, &state);
	memset(tape.market.port.factor, 0,
	    sizeof(tape.market.port.factor));
	tape.fail_at = 2U;
	yt_error_clear(&error);
	return !yt_ordinary_commerce_run(&state, &ordinary_commerce_test_ops,
	    &tape, &error) && error.status == YT_IO_ERROR
	    && !state.refusal_presented && !state.final_player_read
	    && tape.foreground == 6.0f;
}

enum port_docking_event {
	PORT_DOCKING_LABEL = 1,
	PORT_DOCKING_FOREGROUND,
	PORT_DOCKING_GATE,
	PORT_DOCKING_READ_SECTOR,
	PORT_DOCKING_NO_PORT,
	PORT_DOCKING_BLANK,
	PORT_DOCKING_PREFIX,
	PORT_DOCKING_FINALIZE,
	PORT_DOCKING_READ_PORT,
	PORT_DOCKING_EARTH,
	PORT_DOCKING_ORDINARY,
};
struct port_docking_tape {
	enum port_docking_event events[16];
	size_t event_count;
	size_t calls;
	size_t fail_at;
	bool denied;
	float gate_sector;
	float gate_sector_record;
	struct yt_sector sector;
	bool finalizer_returned;
	float post_sector;
	float post_sector_record;
	uint32_t sector_record_read;
	uint32_t port_record_read;
	int ordinary_sector;
	float foreground;
};

static bool
port_docking_step(struct port_docking_tape *tape,
    enum port_docking_event event, struct yt_error *error)
{
	size_t call = tape->calls++;

	if (tape->event_count < YT_ARRAY_LEN(tape->events))
		tape->events[tape->event_count++] = event;
	if (call != tape->fail_at)
		return true;
	if (error != NULL)
		error->status = YT_IO_ERROR;
	return false;
}

static bool
port_docking_test_present(void *context, const uint8_t *text, size_t length,
    enum yt_port_docking_output_kind kind, struct yt_error *error)
{
	static const char *const expected[] = {
		"<Port>", "No port here!", "", "Docking, ",
	};
	static const enum port_docking_event event[] = {
		PORT_DOCKING_LABEL, PORT_DOCKING_NO_PORT,
		PORT_DOCKING_BLANK, PORT_DOCKING_PREFIX,
	};
	struct port_docking_tape *tape = context;

	if ((size_t)kind >= YT_ARRAY_LEN(expected)
	    || length != strlen(expected[kind])
	    || (length != 0U && memcmp(text, expected[kind], length) != 0))
		return false;
	return port_docking_step(tape, event[kind], error);
}

static void
port_docking_test_foreground(void *context, float foreground)
{
	struct port_docking_tape *tape = context;

	if (tape->event_count < YT_ARRAY_LEN(tape->events))
		tape->events[tape->event_count++] = PORT_DOCKING_FOREGROUND;
	tape->foreground = foreground;
}

static bool
port_docking_test_gate(void *context, bool *denied, float *current_sector,
    float *sector_record_expression, struct yt_error *error)
{
	struct port_docking_tape *tape = context;

	if (!port_docking_step(tape, PORT_DOCKING_GATE, error))
		return false;
	*denied = tape->denied;
	*current_sector = tape->gate_sector;
	*sector_record_expression = tape->gate_sector_record;
	return true;
}

static bool
port_docking_test_read_sector(void *context, uint32_t physical_record,
    struct yt_sector *sector, struct yt_error *error)
{
	struct port_docking_tape *tape = context;

	if (!port_docking_step(tape, PORT_DOCKING_READ_SECTOR, error))
		return false;
	tape->sector_record_read = physical_record;
	*sector = tape->sector;
	return true;
}

static bool
port_docking_test_finalize(void *context, bool *returned,
    float *current_sector, float *sector_record_expression,
    struct yt_error *error)
{
	struct port_docking_tape *tape = context;

	if (!port_docking_step(tape, PORT_DOCKING_FINALIZE, error))
		return false;
	*returned = tape->finalizer_returned;
	*current_sector = tape->post_sector;
	*sector_record_expression = tape->post_sector_record;
	return true;
}

static bool
port_docking_test_read_port(void *context, uint32_t physical_record,
    struct yt_port *port, struct yt_error *error)
{
	struct port_docking_tape *tape = context;

	if (!port_docking_step(tape, PORT_DOCKING_READ_PORT, error))
		return false;
	tape->port_record_read = physical_record;
	memset(port, 0, sizeof(*port));
	return true;
}

static bool
port_docking_test_earth(void *context, struct yt_error *error)
{
	return port_docking_step(context, PORT_DOCKING_EARTH, error);
}

static bool
port_docking_test_ordinary(void *context, int sector_number,
    float sector_record_expression,
    struct yt_error *error)
{
	struct port_docking_tape *tape = context;

	if (sector_record_expression != tape->post_sector_record
	    || !port_docking_step(tape, PORT_DOCKING_ORDINARY, error))
		return false;
	tape->ordinary_sector = sector_number;
	return true;
}

static const struct yt_port_docking_ops port_docking_test_ops = {
	port_docking_test_present,
	port_docking_test_foreground,
	port_docking_test_gate,
	port_docking_test_read_sector,
	port_docking_test_finalize,
	port_docking_test_read_port,
	port_docking_test_earth,
	port_docking_test_ordinary,
};

static void
port_docking_fixture(struct port_docking_tape *tape,
    struct yt_port_docking_state *state)
{
	memset(tape, 0, sizeof(*tape));
	memset(state, 0, sizeof(*state));
	tape->fail_at = SIZE_MAX;
	tape->gate_sector = 733.0f;
	tape->gate_sector_record = 784.0f;
	tape->sector.port = 2.0f;
	tape->finalizer_returned = true;
	tape->post_sector = 733.0f;
	tape->post_sector_record = 790.0f;
	state->port_offset = 2055.0f;
}

static bool
check_port_docking_transaction(void)
{
	static const enum port_docking_event ordinary_expected[] = {
		PORT_DOCKING_LABEL,
		PORT_DOCKING_FOREGROUND,
		PORT_DOCKING_GATE,
		PORT_DOCKING_READ_SECTOR,
		PORT_DOCKING_BLANK,
		PORT_DOCKING_PREFIX,
		PORT_DOCKING_FINALIZE,
		PORT_DOCKING_READ_PORT,
		PORT_DOCKING_ORDINARY,
	};
	static const enum port_docking_event no_port_expected[] = {
		PORT_DOCKING_LABEL,
		PORT_DOCKING_FOREGROUND,
		PORT_DOCKING_GATE,
		PORT_DOCKING_READ_SECTOR,
		PORT_DOCKING_NO_PORT,
	};
	struct port_docking_tape tape;
	struct yt_port_docking_state state;
	struct yt_error error;
	size_t failure;

	port_docking_fixture(&tape, &state);
	if (!yt_port_docking_run(&state, &port_docking_test_ops, &tape, NULL)
	    || !state.complete || state.route != YT_PORT_DOCKING_ORDINARY
	    || !state.label_presented || !state.foreground_selected
	    || !state.gate_complete || state.gate_denied || !state.sector_read
	    || state.gate_sector_physical_record != 784U
	    || state.logical_port != 2.0f
	    || state.selected_port_expression != 2057.0f
	    || state.selected_port_physical_record != 2057U
	    || state.post_finalizer_sector_record_expression != 790.0f
	    || !state.docking_blank_presented
	    || !state.docking_prefix_presented || !state.finalizer_complete
	    || !state.selected_port_read || !state.child_complete
	    || tape.foreground != 3.0f || tape.sector_record_read != 784U
	    || tape.port_record_read != 2057U || tape.ordinary_sector != 733
	    || tape.event_count != YT_ARRAY_LEN(ordinary_expected)
	    || memcmp(tape.events, ordinary_expected,
	    sizeof(ordinary_expected)) != 0)
		return false;

	/* Every fallible callback in the ordinary route stops its suffix. */
	for (failure = 0U; failure < 8U; ++failure) {
		port_docking_fixture(&tape, &state);
		tape.fail_at = failure;
		yt_error_clear(&error);
		if (yt_port_docking_run(&state, &port_docking_test_ops,
		    &tape, &error) || error.status != YT_IO_ERROR
		    || state.complete || tape.calls != failure + 1U)
			return false;
	}

	port_docking_fixture(&tape, &state);
	tape.denied = true;
	if (!yt_port_docking_run(&state, &port_docking_test_ops, &tape, NULL)
	    || state.route != YT_PORT_DOCKING_GATE_DENIED
	    || !state.complete || state.sector_read || tape.calls != 2U)
		return false;

	port_docking_fixture(&tape, &state);
	tape.sector.port = 0.0f;
	if (!yt_port_docking_run(&state, &port_docking_test_ops, &tape, NULL)
	    || state.route != YT_PORT_DOCKING_NO_PORT_ROUTE
	    || !state.no_port_presented || state.finalizer_complete
	    || tape.event_count != YT_ARRAY_LEN(no_port_expected)
	    || memcmp(tape.events, no_port_expected,
	    sizeof(no_port_expected)) != 0)
		return false;

	port_docking_fixture(&tape, &state);
	tape.finalizer_returned = false;
	if (!yt_port_docking_run(&state, &port_docking_test_ops, &tape, NULL)
	    || state.route != YT_PORT_DOCKING_FINALIZER_TERMINAL
	    || !state.finalizer_complete || state.selected_port_read)
		return false;

	port_docking_fixture(&tape, &state);
	tape.post_sector = 1.0f;
	if (!yt_port_docking_run(&state, &port_docking_test_ops, &tape, NULL)
	    || state.route != YT_PORT_DOCKING_EARTH
	    || tape.events[tape.event_count - 1U] != PORT_DOCKING_EARTH)
		return false;

	/* Conversion remains after the finalizer and before selected GET. */
	port_docking_fixture(&tape, &state);
	state.port_offset = 0.0f;
	tape.sector.port = 0.5f;
	yt_error_clear(&error);
	if (yt_port_docking_run(&state, &port_docking_test_ops, &tape, &error)
	    || error.status != YT_RANGE || !state.finalizer_complete
	    || state.selected_port_read
	    || tape.events[tape.event_count - 1U] != PORT_DOCKING_FINALIZE)
		return false;

	/* The initial sector GET uses the gate hydrator's cached expression. */
	port_docking_fixture(&tape, &state);
	tape.gate_sector_record = 0.5f;
	yt_error_clear(&error);
	return !yt_port_docking_run(&state, &port_docking_test_ops, &tape,
	    &error) && error.status == YT_RANGE && !state.sector_read
	    && tape.events[tape.event_count - 1U] == PORT_DOCKING_GATE;
}

enum port_update_event {
	PORT_UPDATE_READ_SECTOR = 1,
	PORT_UPDATE_DAY,
	PORT_UPDATE_READ_PORT,
	PORT_UPDATE_TIMER,
	PORT_UPDATE_WRITE_PORT,
};
struct port_update_tape {
	enum port_update_event events[5];
	size_t calls;
	size_t fail_at;
	struct yt_sector sector;
	struct yt_port stored_port;
	struct yt_port written_port;
	uint32_t sector_record;
	uint32_t read_record;
	uint32_t written_record;
};

static bool
port_update_test_step(struct port_update_tape *tape,
    enum port_update_event event, struct yt_error *error)
{
	size_t call = tape->calls++;

	if (call < YT_ARRAY_LEN(tape->events))
		tape->events[call] = event;
	if (call != tape->fail_at)
		return true;
	if (error != NULL)
		error->status = YT_IO_ERROR;
	return false;
}

static bool
port_update_test_read_sector(void *context, uint32_t physical_record,
    struct yt_sector *sector, struct yt_error *error)
{
	struct port_update_tape *tape = context;

	if (physical_record != tape->sector_record
	    || !port_update_test_step(tape, PORT_UPDATE_READ_SECTOR, error))
		return false;
	*sector = tape->sector;
	return true;
}

static bool
port_update_test_day(void *context, float *current_day,
    struct yt_error *error)
{
	struct port_update_tape *tape = context;

	if (!port_update_test_step(tape, PORT_UPDATE_DAY, error))
		return false;
	*current_day = 1000.0f;
	return true;
}

static bool
port_update_test_read_port(void *context, uint32_t physical_record,
    struct yt_port *port, struct yt_error *error)
{
	struct port_update_tape *tape = context;

	if (!port_update_test_step(tape, PORT_UPDATE_READ_PORT, error))
		return false;
	tape->read_record = physical_record;
	*port = tape->stored_port;
	return true;
}

static bool
port_update_test_timer(void *context, float *timer_seconds,
    struct yt_error *error)
{
	struct port_update_tape *tape = context;

	if (!port_update_test_step(tape, PORT_UPDATE_TIMER, error))
		return false;
	*timer_seconds = 36000.0f;
	return true;
}

static bool
port_update_test_write_port(void *context, uint32_t physical_record,
    const struct yt_port *port, struct yt_error *error)
{
	struct port_update_tape *tape = context;

	if (!port_update_test_step(tape, PORT_UPDATE_WRITE_PORT, error))
		return false;
	tape->written_record = physical_record;
	tape->written_port = *port;
	return true;
}

static const struct yt_port_update_ops port_update_test_ops = {
	port_update_test_read_sector,
	port_update_test_day,
	port_update_test_read_port,
	port_update_test_timer,
	port_update_test_write_port,
};

static void
port_update_fixture(struct port_update_tape *tape,
    struct yt_port_update_state *state)
{
	static const float stock[3] = {100.0f, 200.0f, 300.0f};
	static const float production[3] = {10.0f, 20.0f, 30.0f};
	struct yt_port_market_state market;

	memset(tape, 0, sizeof(*tape));
	memset(state, 0, sizeof(*state));
	tape->fail_at = SIZE_MAX;
	tape->sector_record = 58U;
	tape->sector.port = 2.0f;
	port_market_fixture(&market, 1000.0f, stock, production);
	tape->stored_port = market.port;
	state->sector_number = 7;
	state->sector_record_offset = 51.0f;
	state->port_offset = 2055.0f;
	state->base_price[0] = 20.0f;
	state->base_price[1] = 30.0f;
	state->base_price[2] = 40.0f;
}

static bool
check_port_update_transaction(void)
{
	static const enum port_update_event expected[] = {
		PORT_UPDATE_READ_SECTOR,
		PORT_UPDATE_DAY,
		PORT_UPDATE_READ_PORT,
		PORT_UPDATE_TIMER,
		PORT_UPDATE_WRITE_PORT,
	};
	static const enum port_update_event preloaded_expected[] = {
		PORT_UPDATE_DAY,
		PORT_UPDATE_READ_PORT,
		PORT_UPDATE_TIMER,
		PORT_UPDATE_WRITE_PORT,
	};
	struct port_update_tape tape;
	struct yt_port_update_state state;
	struct yt_error error;
	size_t failure;

	port_update_fixture(&tape, &state);
	if (!yt_port_update_run(&state, &port_update_test_ops, &tape, NULL)
	    || !state.complete || !state.sector_loaded || !state.sector_read
	    || !state.day_observed || !state.port_read || !state.timer_observed
	    || !state.port_written || tape.calls != YT_ARRAY_LEN(expected)
	    || memcmp(tape.events, expected, sizeof(expected)) != 0
	    || state.sector_record_expression != 58.0f
	    || state.sector_physical_record != 58U
	    || state.market.logical_port != 2.0f
	    || state.market.port_record_expression != 2057.0f
	    || state.market.port_physical_record != 2057U
	    || tape.read_record != 2057U || tape.written_record != 2057U
	    || state.market.price[0] != 32.0f
	    || state.market.price[1] != 8.0f
	    || state.market.price[2] != 66.0f
	    || memcmp(tape.written_port.record.bytes,
	    state.market.port.record.bytes, sizeof(tape.written_port.record.bytes))
	    != 0)
		return false;

	for (failure = 0U; failure < YT_ARRAY_LEN(expected); ++failure) {
		port_update_fixture(&tape, &state);
		tape.fail_at = failure;
		yt_error_clear(&error);
		if (yt_port_update_run(&state, &port_update_test_ops, &tape,
		    &error) || error.status != YT_IO_ERROR || state.complete
		    || tape.calls != failure + 1U
		    || memcmp(tape.events, expected,
		    tape.calls * sizeof(expected[0])) != 0
		    || state.port_written)
			return false;
	}

	port_update_fixture(&tape, &state);
	state.sector = tape.sector;
	state.sector_loaded = true;
	if (!yt_port_update_run(&state, &port_update_test_ops, &tape, NULL)
	    || state.sector_read || tape.calls != YT_ARRAY_LEN(preloaded_expected)
	    || memcmp(tape.events, preloaded_expected,
	    sizeof(preloaded_expected)) != 0)
		return false;

	port_update_fixture(&tape, &state);
	tape.sector.port = 2.75f;
	if (!yt_port_update_run(&state, &port_update_test_ops, &tape, NULL)
	    || state.market.logical_port != 2.75f
	    || state.market.port_record_expression != 2057.75f
	    || state.market.port_physical_record != 2057U)
		return false;

	/* Docking supplies the post-finalizer A41C sector scratch directly. */
	port_update_fixture(&tape, &state);
	state.sector_record_expression = 784.0f;
	state.sector_record_supplied = true;
	tape.sector_record = 784U;
	if (!yt_port_update_run(&state, &port_update_test_ops, &tape, NULL)
	    || state.sector_record_expression != 784.0f
	    || state.sector_physical_record != 784U)
		return false;

	port_update_fixture(&tape, &state);
	state.sector_record_expression = 0.5f;
	state.sector_record_supplied = true;
	yt_error_clear(&error);
	if (yt_port_update_run(&state, &port_update_test_ops, &tape, &error)
	    || error.status != YT_RANGE || tape.calls != 0U
	    || state.sector_read || state.day_observed)
		return false;

	port_update_fixture(&tape, &state);
	state.port_offset = -2.25f;
	tape.sector.port = 2.75f;
	yt_error_clear(&error);
	if (yt_port_update_run(&state, &port_update_test_ops, &tape, &error)
	    || error.status != YT_RANGE || tape.calls != 1U
	    || state.day_observed || state.port_read || state.port_written)
		return false;

	port_update_fixture(&tape, &state);
	(void)yt_record_set_number(&tape.stored_port.record, YT_F49, 0.0f);
	(void)yt_record_set_number(&tape.stored_port.record, YT_F61, 0.0f);
	yt_error_clear(&error);
	return !yt_port_update_run(&state, &port_update_test_ops, &tape, &error)
	    && error.status == YT_RANGE && tape.calls == 4U
	    && !state.port_written
	    && memcmp(tape.written_port.record.bytes, "\0", 1U) == 0;
}

enum port_report_event {
	PORT_REPORT_RESET = 1,
	PORT_REPORT_READ_PLAYER,
	PORT_REPORT_READ_PORT,
	PORT_REPORT_DATE,
	PORT_REPORT_TIME,
	PORT_REPORT_PRESENT,
	PORT_REPORT_BOLD,
	PORT_REPORT_FOREGROUND,
};
struct port_report_fragment {
	enum yt_port_report_output_kind kind;
	size_t item;
	uint8_t text[256];
	size_t length;
};
struct port_report_tape {
	enum port_report_event events[64];
	size_t event_count;
	size_t bool_calls;
	size_t fail_bool_at;
	uint32_t player_records[2];
	size_t player_reads;
	struct yt_player current_player;
	struct yt_player owner_player;
	uint32_t port_record;
	struct yt_port report_port;
	struct port_report_fragment fragments[20];
	size_t fragment_count;
	float bold[2];
	size_t bold_count;
	float foreground[4];
	size_t foreground_count;
	uint8_t pager_raw[4];
};

static bool
port_report_test_step(struct port_report_tape *tape,
    enum port_report_event event, struct yt_error *error)
{
	size_t call = tape->bool_calls++;

	if (tape->event_count < YT_ARRAY_LEN(tape->events))
		tape->events[tape->event_count++] = event;
	if (call != tape->fail_bool_at)
		return true;
	if (error != NULL)
		error->status = YT_IO_ERROR;
	return false;
}

static void
port_report_test_event(struct port_report_tape *tape,
    enum port_report_event event)
{
	if (tape->event_count < YT_ARRAY_LEN(tape->events))
		tape->events[tape->event_count++] = event;
}

static bool
port_report_test_read_player(void *context, uint32_t physical_record,
    struct yt_player *player, struct yt_error *error)
{
	struct port_report_tape *tape = context;

	if (!port_report_test_step(tape, PORT_REPORT_READ_PLAYER, error)
	    || tape->player_reads >= YT_ARRAY_LEN(tape->player_records))
		return false;
	tape->player_records[tape->player_reads++] = physical_record;
	if (physical_record == 2U)
		*player = tape->current_player;
	else if (physical_record == 8U)
		*player = tape->owner_player;
	else
		return false;
	return true;
}

static bool
port_report_test_read_port(void *context, uint32_t physical_record,
    struct yt_port *port, struct yt_error *error)
{
	struct port_report_tape *tape = context;

	if (!port_report_test_step(tape, PORT_REPORT_READ_PORT, error)
	    || physical_record != tape->port_record)
		return false;
	*port = tape->report_port;
	return true;
}

static bool
port_report_test_date(void *context, uint8_t date[10],
    struct yt_error *error)
{
	struct port_report_tape *tape = context;

	if (!port_report_test_step(tape, PORT_REPORT_DATE, error))
		return false;
	memcpy(date, "07-25-2026", 10U);
	return true;
}

static bool
port_report_test_time(void *context, uint8_t time_text[8],
    struct yt_error *error)
{
	struct port_report_tape *tape = context;

	if (!port_report_test_step(tape, PORT_REPORT_TIME, error))
		return false;
	memcpy(time_text, "12:34:56", 8U);
	return true;
}

static bool
port_report_test_present(void *context, const uint8_t *text, size_t length,
    enum yt_port_report_output_kind kind, size_t item,
    struct yt_error *error)
{
	struct port_report_tape *tape = context;
	struct port_report_fragment *fragment;

	if (!port_report_test_step(tape, PORT_REPORT_PRESENT, error)
	    || tape->fragment_count >= YT_ARRAY_LEN(tape->fragments)
	    || length > sizeof(tape->fragments[0].text))
		return false;
	fragment = &tape->fragments[tape->fragment_count++];
	fragment->kind = kind;
	fragment->item = item;
	fragment->length = length;
	if (length != 0U)
		memcpy(fragment->text, text, length);
	return true;
}

static void
port_report_test_reset(void *context, const uint8_t raw[4])
{
	struct port_report_tape *tape = context;

	port_report_test_event(tape, PORT_REPORT_RESET);
	memcpy(tape->pager_raw, raw, sizeof(tape->pager_raw));
}

static void
port_report_test_bold(void *context, float bold)
{
	struct port_report_tape *tape = context;

	port_report_test_event(tape, PORT_REPORT_BOLD);
	if (tape->bold_count < YT_ARRAY_LEN(tape->bold))
		tape->bold[tape->bold_count++] = bold;
}

static void
port_report_test_foreground(void *context, float foreground)
{
	struct port_report_tape *tape = context;

	port_report_test_event(tape, PORT_REPORT_FOREGROUND);
	if (tape->foreground_count < YT_ARRAY_LEN(tape->foreground))
		tape->foreground[tape->foreground_count++] = foreground;
}

static const struct yt_port_report_ops port_report_test_ops = {
	port_report_test_read_player,
	port_report_test_read_port,
	port_report_test_date,
	port_report_test_time,
	port_report_test_present,
	port_report_test_reset,
	port_report_test_bold,
	port_report_test_foreground,
};

static void
port_report_player_fixture(struct yt_player *player, uint8_t pattern,
    const char *name, float ore, float organics, float equipment)
{
	struct yt_record record;
	size_t name_length = strlen(name);

	memset(record.bytes, pattern, sizeof(record.bytes));
	memset(record.bytes, ' ', YT_TEXT_FIELD_SIZE);
	memcpy(record.bytes, name, name_length);
	(void)yt_record_set_number(&record, YT_F69, ore);
	(void)yt_record_set_number(&record, YT_F73, organics);
	(void)yt_record_set_number(&record, YT_F77, equipment);
	(void)yt_record_set_number(&record, YT_F85, (float)name_length);
	yt_player_decode(player, &record);
}

static void
port_report_fixture(struct port_report_tape *tape,
    struct yt_port_report_state *state)
{
	static const float stock[3] = {100.0f, 200.0f, 300.0f};
	static const float production[3] = {10.0f, 20.0f, 30.0f};
	struct yt_record fresh;

	memset(tape, 0, sizeof(*tape));
	tape->fail_bool_at = SIZE_MAX;
	tape->port_record = 2057U;
	port_report_player_fixture(&tape->current_player, 0x91, "Current",
	    5.5f, 6.0f, 7.25f);
	port_report_player_fixture(&tape->owner_player, 0xa2, "Other",
	    1.0f, 2.0f, 3.0f);
	port_market_fixture(&state->market, 1000.0f, stock, production);
	(void)yt_port_market_update(&state->market, NULL);
	fresh = state->market.port.record;
	memset(fresh.bytes, ' ', YT_TEXT_FIELD_SIZE);
	memcpy(fresh.bytes, "Fresh", 5U);
	(void)yt_record_set_number(&fresh, YT_F85, 5.0f);
	yt_port_decode(&tape->report_port, &fresh);
	state->current_player_record = 2;
	state->port_physical_record = tape->port_record;
	state->conversion_mode = 0U;
}

static bool
port_report_fragment_equal(const struct port_report_fragment *fragment,
    enum yt_port_report_output_kind kind, size_t item,
    const void *text, size_t length)
{
	return fragment->kind == kind && fragment->item == item
	    && fragment->length == length
	    && (length == 0U || memcmp(fragment->text, text, length) == 0);
}

static bool
check_port_report_transaction(void)
{
	static const enum port_report_event expected_events[] = {
		PORT_REPORT_RESET,
		PORT_REPORT_PRESENT, PORT_REPORT_PRESENT,
		PORT_REPORT_READ_PLAYER, PORT_REPORT_READ_PORT,
		PORT_REPORT_DATE, PORT_REPORT_TIME,
		PORT_REPORT_PRESENT, PORT_REPORT_PRESENT,
		PORT_REPORT_PRESENT, PORT_REPORT_PRESENT,
		PORT_REPORT_BOLD, PORT_REPORT_PRESENT,
		PORT_REPORT_FOREGROUND,
		PORT_REPORT_PRESENT, PORT_REPORT_PRESENT,
		PORT_REPORT_PRESENT, PORT_REPORT_PRESENT,
		PORT_REPORT_FOREGROUND,
		PORT_REPORT_PRESENT, PORT_REPORT_PRESENT,
		PORT_REPORT_PRESENT, PORT_REPORT_PRESENT,
		PORT_REPORT_FOREGROUND,
		PORT_REPORT_PRESENT, PORT_REPORT_PRESENT,
		PORT_REPORT_PRESENT, PORT_REPORT_PRESENT,
		PORT_REPORT_FOREGROUND,
	};
	static const uint8_t owner[] =
	    "This port is owned by: YOU, Credits: 1234.5";
	static const uint8_t title[] =
	    "Commerce report for Fresh: 07-25-2026 12:34:56";
	static const uint8_t header[] =
	    " Items         Status      # units    in holds   Cost";
	static const uint8_t rule[] =
	    "=======       =========   =========   ========   ====";
	static const uint8_t names[3][23] = {
		"Ore..........  Buying ",
		"Organics.....  Selling",
		"Equipment....  Buying ",
	};
	static const uint8_t capacities[3][13] = {
		"         100", "         200", "         300",
	};
	static const uint8_t holds[3][12] = {
		"        5.5", "          6", "       7.25",
	};
	static const uint8_t prices[3][8] = {
		" 32    ", " 8    ", " 66    ",
	};
	static const size_t price_lengths[3] = {7U, 6U, 7U};
	struct port_report_tape tape;
	struct yt_port_report_state state;
	struct yt_error error;
	size_t index;
	size_t failure;

	memset(&state, 0, sizeof(state));
	port_report_fixture(&tape, &state);
	if (!yt_port_report_run(&state, &port_report_test_ops, &tape, NULL)
	    || !state.complete || !state.pager_reset
	    || !state.current_player_read || !state.report_port_read
	    || state.owner_player_read || !state.date_observed
	    || !state.time_observed || state.output_count != 19U
	    || state.completed_items != 3U || state.foreground != 3.0f
	    || state.bold != 1.0f || state.owner_kind != YT_PORT_OWNER_SELF
	    || state.owner_record != 0 || tape.fragment_count != 19U
	    || tape.event_count != YT_ARRAY_LEN(expected_events)
	    || memcmp(tape.events, expected_events, sizeof(expected_events)) != 0
	    || memcmp(tape.pager_raw, "\0\0\1\0", 4U) != 0
	    || tape.player_reads != 1U || tape.player_records[0] != 2U
	    || tape.bold_count != 1U || tape.bold[0] != 1.0f
	    || tape.foreground_count != 4U
	    || tape.foreground[0] != 3.0f || tape.foreground[1] != 2.0f
	    || tape.foreground[2] != 3.0f || tape.foreground[3] != 3.0f
	    || !port_report_fragment_equal(&tape.fragments[0],
	    YT_PORT_REPORT_OWNER_BLANK, SIZE_MAX, NULL, 0U)
	    || !port_report_fragment_equal(&tape.fragments[1],
	    YT_PORT_REPORT_OWNER_ROW, SIZE_MAX, owner, sizeof(owner) - 1U)
	    || !port_report_fragment_equal(&tape.fragments[2],
	    YT_PORT_REPORT_TITLE_BLANK, SIZE_MAX, NULL, 0U)
	    || !port_report_fragment_equal(&tape.fragments[3],
	    YT_PORT_REPORT_TITLE, SIZE_MAX, title, sizeof(title) - 1U)
	    || !port_report_fragment_equal(&tape.fragments[4],
	    YT_PORT_REPORT_HEADER_BLANK, SIZE_MAX, NULL, 0U)
	    || !port_report_fragment_equal(&tape.fragments[5],
	    YT_PORT_REPORT_HEADER, SIZE_MAX, header, sizeof(header) - 1U)
	    || !port_report_fragment_equal(&tape.fragments[6],
	    YT_PORT_REPORT_RULE, SIZE_MAX, rule, sizeof(rule) - 1U))
		return false;
	for (index = 0U; index < 3U; ++index) {
		size_t fragment = 7U + index * 4U;

		if (!port_report_fragment_equal(&tape.fragments[fragment],
		    YT_PORT_REPORT_ITEM_NAME_STATUS, index, names[index],
		    sizeof(names[index]) - 1U)
		    || !port_report_fragment_equal(&tape.fragments[fragment + 1U],
		    YT_PORT_REPORT_ITEM_CAPACITY, index, capacities[index], 12U)
		    || !port_report_fragment_equal(&tape.fragments[fragment + 2U],
		    YT_PORT_REPORT_ITEM_HOLD, index, holds[index], 11U)
		    || !port_report_fragment_equal(&tape.fragments[fragment + 3U],
		    YT_PORT_REPORT_ITEM_PRICE, index, prices[index],
		    price_lengths[index]))
			return false;
	}

	/* Every provider/output failure retains exactly its completed prefix. */
	for (failure = 0U; failure < 23U; ++failure) {
		memset(&state, 0, sizeof(state));
		port_report_fixture(&tape, &state);
		tape.fail_bool_at = failure;
		yt_error_clear(&error);
		if (yt_port_report_run(&state, &port_report_test_ops, &tape,
		    &error) || error.status != YT_IO_ERROR || state.complete
		    || tape.bool_calls != failure + 1U)
			return false;
	}

	/* Other-owner GET fails before its direct blank. */
	memset(&state, 0, sizeof(state));
	port_report_fixture(&tape, &state);
	state.market.port.owner = 8.0f;
	(void)yt_record_set_number(&state.market.port.record, YT_F97, 8.0f);
	tape.fail_bool_at = 0U;
	yt_error_clear(&error);
	if (yt_port_report_run(&state, &port_report_test_ops, &tape, &error)
	    || error.status != YT_IO_ERROR || tape.fragment_count != 0U
	    || !state.pager_reset || state.owner_kind != YT_PORT_OWNER_OTHER)
		return false;

	/* Exact MBF56 stock survives beyond host SINGLE precision. */
	memset(&state, 0, sizeof(state));
	port_report_fixture(&tape, &state);
	if (qb_mbf64_encode(16777217.0, state.market.capacity_raw[0])
	    != QB_MBF_OK
	    || !yt_port_report_run(&state, &port_report_test_ops, &tape, NULL)
	    || !port_report_fragment_equal(&tape.fragments[8],
	    YT_PORT_REPORT_ITEM_CAPACITY, 0U, "    16777217", 12U))
		return false;

	/* Corrupt negative capacity follows INT_D floor, not truncation. */
	memset(&state, 0, sizeof(state));
	port_report_fixture(&tape, &state);
	return qb_mbf64_encode(-5.5, state.market.capacity_raw[0]) == QB_MBF_OK
	    && yt_port_report_run(&state, &port_report_test_ops, &tape, NULL)
	    && port_report_fragment_equal(&tape.fragments[8],
	    YT_PORT_REPORT_ITEM_CAPACITY, 0U, "          -6", 12U);
}

enum treasury_event {
	TREASURY_PRESENT = 1,
	TREASURY_READ_PLAYER_INITIAL,
	TREASURY_READ_PORT,
	TREASURY_WRITE_PORT,
	TREASURY_READ_PLAYER_FINAL,
	TREASURY_WRITE_PLAYER,
	TREASURY_FLUSH,
	TREASURY_CACHE,
};
struct treasury_tape {
	enum treasury_event events[32];
	size_t calls;
	size_t fail_at;
	struct yt_player players[2];
	size_t player_reads;
	uint32_t player_record;
	struct yt_port ports[3];
	struct yt_port written_port;
	uint32_t written_port_record;
	struct yt_player written_player;
	struct yt_player cached_player;
	uint8_t rows[17][256];
	size_t row_lengths[17];
	bool row_seen[17];
};
static bool
treasury_step(struct treasury_tape *tape, enum treasury_event event,
    struct yt_error *error)
{
	size_t call = tape->calls++;

	if (call < YT_ARRAY_LEN(tape->events))
		tape->events[call] = event;
	if (call != tape->fail_at)
		return true;
	if (error != NULL)
		error->status = YT_IO_ERROR;
	return false;
}
static bool
treasury_read_player_test(void *context, uint32_t physical_record,
    struct yt_player *player, struct yt_error *error)
{
	struct treasury_tape *tape = context;
	enum treasury_event event = tape->player_reads == 0U
	    ? TREASURY_READ_PLAYER_INITIAL : TREASURY_READ_PLAYER_FINAL;

	if (physical_record != tape->player_record || tape->player_reads >= 2U
	    || !treasury_step(tape, event, error))
		return false;
	*player = tape->players[tape->player_reads++];
	return true;
}
static bool
treasury_read_port_test(void *context, uint32_t physical_record,
    struct yt_port *port, struct yt_error *error)
{
	struct treasury_tape *tape = context;

	if (physical_record < 2056U || physical_record > 2058U
	    || !treasury_step(tape, TREASURY_READ_PORT, error))
		return false;
	*port = tape->ports[physical_record - 2056U];
	return true;
}
static bool
treasury_write_port_test(void *context, uint32_t physical_record,
    struct yt_port *port, struct yt_error *error)
{
	struct treasury_tape *tape = context;

	if (!treasury_step(tape, TREASURY_WRITE_PORT, error))
		return false;
	tape->written_port_record = physical_record;
	tape->written_port = *port;
	return true;
}
static bool
treasury_present_test(void *context, const uint8_t *text, size_t length,
    enum yt_treasury_output_kind kind, struct yt_error *error)
{
	struct treasury_tape *tape = context;

	if ((size_t)kind >= YT_ARRAY_LEN(tape->rows)
	    || length > sizeof(tape->rows[0])
	    || !treasury_step(tape, TREASURY_PRESENT, error))
		return false;
	if (length != 0U)
		memcpy(tape->rows[kind], text, length);
	tape->row_lengths[kind] = length;
	tape->row_seen[kind] = true;
	return true;
}
static bool
treasury_write_player_test(void *context, uint32_t physical_record,
    struct yt_player *player, struct yt_error *error)
{
	struct treasury_tape *tape = context;

	if (physical_record != tape->player_record
	    || !treasury_step(tape, TREASURY_WRITE_PLAYER, error))
		return false;
	tape->written_player = *player;
	return true;
}
static bool
treasury_flush_test(void *context, struct yt_error *error)
{
	return treasury_step(context, TREASURY_FLUSH, error);
}
static bool
treasury_cache_test(void *context, const struct yt_player *player,
    struct yt_error *error)
{
	struct treasury_tape *tape = context;

	if (!treasury_step(tape, TREASURY_CACHE, error))
		return false;
	tape->cached_player = *player;
	return true;
}
static const struct yt_treasury_ops treasury_test_ops = {
	treasury_read_player_test,
	treasury_read_port_test,
	treasury_write_port_test,
	treasury_present_test,
	treasury_write_player_test,
	treasury_flush_test,
	treasury_cache_test,
};
static void
treasury_set_port(struct yt_port *port, uint8_t pattern,
    const uint8_t *name, size_t name_length, float treasury, float sector,
    float owner)
{
	memset(port, 0, sizeof(*port));
	memset(port->record.bytes, pattern, sizeof(port->record.bytes));
	memset(port->record.bytes, ' ', YT_TEXT_FIELD_SIZE);
	if (name_length != 0U)
		memcpy(port->record.bytes, name, name_length);
	(void)yt_record_set_number(&port->record, YT_F85,
	    (float)name_length);
	(void)yt_record_set_number(&port->record, YT_F89, treasury);
	(void)yt_record_set_number(&port->record, YT_F93, sector);
	(void)yt_record_set_number(&port->record, YT_F97, owner);
	port->name_length = (float)name_length;
	port->treasury = treasury;
	port->sector = sector;
	port->owner = owner;
}
static void
treasury_fixture(struct treasury_tape *tape,
    struct yt_treasury_state *state, bool collecting)
{
	static const uint8_t true_raw[4] = {0, 0, 0, 0x81};
	static const uint8_t false_raw[4] = {0, 0xae, 3, 0};
	static const uint8_t dirty_zero[4] = {0, 0, 0x20, 0};

	memset(tape, 0, sizeof(*tape));
	memset(state, 0, sizeof(*state));
	tape->fail_at = SIZE_MAX;
	tape->player_record = 2U;
	memset(tape->players[0].record.bytes, 0x91,
	    sizeof(tape->players[0].record.bytes));
	tape->players[0].ports_owned = 2.0f;
	tape->players[0].credits = 1.0f;
	(void)yt_record_set_number(&tape->players[0].record, YT_F117, 2.0f);
	(void)yt_record_set_number(&tape->players[0].record, YT_F81, 1.0f);
	memset(tape->players[1].record.bytes, 0xa2,
	    sizeof(tape->players[1].record.bytes));
	tape->players[1].ports_owned = 99.0f;
	tape->players[1].credits = 100.0f;
	(void)yt_record_set_number(&tape->players[1].record, YT_F117, 99.0f);
	(void)yt_record_set_number(&tape->players[1].record, YT_F81, 100.0f);
	treasury_set_port(&tape->ports[0], 0xb3, (const uint8_t *)"Alpha",
	    5U, 10.0f, 5.0f, 2.75f);
	treasury_set_port(&tape->ports[1], 0xc4, (const uint8_t *)"Beta",
	    4U, 0.0f, 6.0f, 2.75f);
	(void)yt_record_set_raw_number(&tape->ports[1].record, YT_F89,
	    dirty_zero);
	treasury_set_port(&tape->ports[2], 0xd5, (const uint8_t *)"Gamma",
	    5U, 20.0f, 7.0f, 3.0f);
	*state = (struct yt_treasury_state){
		.current_player_record = 2.75f,
		.port_offset = 2055.0f,
		.planet_offset = 2058.0f,
		.conversion_mode = 0,
	};
	memcpy(state->collecting_raw, collecting ? true_raw : false_raw,
	    sizeof(state->collecting_raw));
}
static bool
check_treasury_transaction(void)
{
	static const enum treasury_event collect_events[] = {
		TREASURY_PRESENT,
		TREASURY_READ_PLAYER_INITIAL,
		TREASURY_PRESENT, TREASURY_PRESENT, TREASURY_PRESENT,
		TREASURY_READ_PORT,
		TREASURY_PRESENT, TREASURY_PRESENT, TREASURY_PRESENT,
		TREASURY_PRESENT,
		TREASURY_WRITE_PORT,
		TREASURY_READ_PORT,
		TREASURY_READ_PORT,
		TREASURY_PRESENT, TREASURY_PRESENT, TREASURY_PRESENT,
		TREASURY_PRESENT, TREASURY_PRESENT, TREASURY_PRESENT,
		TREASURY_PRESENT,
		TREASURY_READ_PLAYER_FINAL,
		TREASURY_WRITE_PLAYER,
		TREASURY_FLUSH,
		TREASURY_CACHE,
	};
	static const enum treasury_event report_events[] = {
		TREASURY_PRESENT,
		TREASURY_READ_PLAYER_INITIAL,
		TREASURY_PRESENT, TREASURY_PRESENT, TREASURY_PRESENT,
		TREASURY_READ_PORT,
		TREASURY_PRESENT, TREASURY_PRESENT, TREASURY_PRESENT,
		TREASURY_PRESENT,
		TREASURY_READ_PORT,
		TREASURY_READ_PORT,
		TREASURY_PRESENT, TREASURY_PRESENT, TREASURY_PRESENT,
		TREASURY_PRESENT, TREASURY_PRESENT, TREASURY_PRESENT,
		TREASURY_PRESENT,
	};
	static const uint8_t dirty_zero[4] = {0, 0, 0x20, 0};
	static const uint8_t sector[] = "Sector: 5";
	static const uint8_t credits[] = " Credits: 10";
	static const uint8_t total[] = " Total: 10";
	static const uint8_t collect_result[] =
	    "You collected a total of 10 credits.";
	static const uint8_t report_result[] =
	    "You have 10 credits in your port accounts.";
	struct treasury_tape tape;
	struct yt_treasury_state state;
	struct yt_player expected_player;
	struct yt_error error;
	uint8_t expected_total_raw[8];
	size_t failure;

	treasury_fixture(&tape, &state, true);
	expected_player = tape.players[1];
	(void)yt_record_set_number(&expected_player.record, YT_F117, 2.0f);
	(void)yt_record_set_number(&expected_player.record, YT_F81, 110.0f);
	if (!yt_treasury_run(&state, &treasury_test_ops, &tape, NULL)
	    || !state.complete || state.route != YT_TREASURY_COLLECTION_ROUTE
	    || !state.collecting || !state.initial_player_read
	    || state.current_player_physical_record != 2U
	    || state.loop_bound != 3.0f || state.counter != 4.0f
	    || state.owned != 2.0f || state.credited != 1.0f
	    || state.barren != 1.0f || state.total != 10.0
	    || state.records_read != 3U || state.records_written != 1U
	    || !state.final_player_read || !state.player_written
	    || !state.player_flushed || !state.cache_updated
	    || tape.calls != YT_ARRAY_LEN(collect_events)
	    || memcmp(tape.events, collect_events, sizeof(collect_events)) != 0
	    || tape.written_port_record != 2056U
	    || memcmp(tape.written_port.record.bytes + YT_F89, dirty_zero,
	    sizeof(dirty_zero)) != 0
	    || memcmp(tape.written_player.record.bytes,
	    expected_player.record.bytes, sizeof(expected_player.record.bytes))
	    != 0
	    || memcmp(tape.cached_player.record.bytes,
	    expected_player.record.bytes, sizeof(expected_player.record.bytes))
	    != 0
	    || tape.row_lengths[YT_TREASURY_SECTOR_FIELD]
	    != sizeof(sector) - 1U
	    || memcmp(tape.rows[YT_TREASURY_SECTOR_FIELD], sector,
	    sizeof(sector) - 1U) != 0
	    || tape.row_lengths[YT_TREASURY_NAME_FIELD] != 5U
	    || memcmp(tape.rows[YT_TREASURY_NAME_FIELD], "Alpha", 5U) != 0
	    || tape.row_lengths[YT_TREASURY_CREDIT_FIELD]
	    != sizeof(credits) - 1U
	    || memcmp(tape.rows[YT_TREASURY_CREDIT_FIELD], credits,
	    sizeof(credits) - 1U) != 0
	    || tape.row_lengths[YT_TREASURY_ROW_TOTAL] != sizeof(total) - 1U
	    || memcmp(tape.rows[YT_TREASURY_ROW_TOTAL], total,
	    sizeof(total) - 1U) != 0
	    || tape.row_lengths[YT_TREASURY_COLLECTION_RESULT]
	    != sizeof(collect_result) - 1U
	    || memcmp(tape.rows[YT_TREASURY_COLLECTION_RESULT], collect_result,
	    sizeof(collect_result) - 1U) != 0)
		return false;

	for (failure = 0U; failure < YT_ARRAY_LEN(collect_events); ++failure) {
		treasury_fixture(&tape, &state, true);
		tape.fail_at = failure;
		yt_error_clear(&error);
		if (yt_treasury_run(&state, &treasury_test_ops, &tape, &error)
		    || error.status != YT_IO_ERROR || state.complete
		    || tape.calls != failure + 1U
		    || memcmp(tape.events, collect_events,
		    tape.calls * sizeof(collect_events[0])) != 0)
			return false;
	}

	treasury_fixture(&tape, &state, false);
	if (!yt_treasury_run(&state, &treasury_test_ops, &tape, NULL)
	    || !state.complete || state.route != YT_TREASURY_REPORT_ROUTE
	    || state.collecting || state.records_written != 0U
	    || state.final_player_read || state.player_written
	    || tape.calls != YT_ARRAY_LEN(report_events)
	    || memcmp(tape.events, report_events, sizeof(report_events)) != 0
	    || tape.row_lengths[YT_TREASURY_REPORT_RESULT]
	    != sizeof(report_result) - 1U
	    || memcmp(tape.rows[YT_TREASURY_REPORT_RESULT], report_result,
	    sizeof(report_result) - 1U) != 0)
		return false;

	treasury_fixture(&tape, &state, true);
	tape.players[0].ports_owned = 0.5f;
	if (!yt_treasury_run(&state, &treasury_test_ops, &tape, NULL)
	    || state.route != YT_TREASURY_NO_PORTS_ROUTE || tape.calls != 3U
	    || !tape.row_seen[YT_TREASURY_NO_PORTS]
	    || state.records_read != 0U)
		return false;

	treasury_fixture(&tape, &state, false);
	treasury_set_port(&tape.ports[0], 0xb3, (const uint8_t *)"Large",
	    5U, 16777216.0f, 1.0f, 2.75f);
	treasury_set_port(&tape.ports[1], 0xc4, (const uint8_t *)"Unit",
	    4U, 1.0f, 2.0f, 2.75f);
	if (qb_mbf64_encode(16777217.0, expected_total_raw) != QB_MBF_OK
	    || !yt_treasury_run(&state, &treasury_test_ops, &tape, NULL)
	    || state.total != 16777217.0
	    || memcmp(state.total_raw, expected_total_raw,
	    sizeof(expected_total_raw)) != 0
	    || tape.row_lengths[YT_TREASURY_ROW_TOTAL] != 16U
	    || memcmp(tape.rows[YT_TREASURY_ROW_TOTAL],
	    " Total: 16777217", 16U) != 0)
		return false;

	treasury_fixture(&tape, &state, true);
	if (yt_treasury_run(NULL, &treasury_test_ops, &tape, NULL)
	    || yt_treasury_run(&state, NULL, &tape, NULL))
		return false;

	treasury_fixture(&tape, &state, true);
	state.current_player_record = 0.5f;
	yt_error_clear(&error);
	return !yt_treasury_run(&state, &treasury_test_ops, &tape, &error)
	    && error.status == YT_RANGE && tape.calls == 1U
	    && tape.row_seen[YT_TREASURY_OPENING_BLANK]
	    && state.current_player_physical_record == 0U;
}

enum movement_event {
	MOVEMENT_GATE = 1,
	MOVEMENT_PRESENT,
	MOVEMENT_INPUT,
	MOVEMENT_DANGER,
	MOVEMENT_CLEAR_QUEUE,
	MOVEMENT_CONFIRM,
	MOVEMENT_FINALIZE,
	MOVEMENT_CLEAR_SELF_MINES,
	MOVEMENT_HYDRATE,
	MOVEMENT_WRITE_PLAYER,
	MOVEMENT_FLUSH,
	MOVEMENT_CACHE,
};
struct movement_tape {
	enum movement_event events[24];
	size_t calls;
	size_t fail_at;
	struct yt_player gate_player;
	struct yt_player accepted_player;
	struct yt_player written_player;
	bool denied;
	bool dangerous;
	bool confirmed;
	bool finalizer_result;
	const char *responses[4];
	size_t response_count;
	size_t response_index;
	uint8_t rows[7][256];
	size_t row_lengths[7];
	bool row_seen[7];
	float cached_target;
};
static bool
movement_step(struct movement_tape *tape, enum movement_event event,
    struct yt_error *error)
{
	size_t call = tape->calls++;

	if (call < YT_ARRAY_LEN(tape->events))
		tape->events[call] = event;
	if (call != tape->fail_at)
		return true;
	if (error != NULL)
		error->status = YT_IO_ERROR;
	return false;
}
static void
movement_record(struct movement_tape *tape, enum movement_event event)
{
	if (tape->calls < YT_ARRAY_LEN(tape->events))
		tape->events[tape->calls] = event;
	++tape->calls;
}
static bool
movement_gate_test(void *context, int player_record, struct yt_player *player,
    bool *denied, struct yt_error *error)
{
	struct movement_tape *tape = context;

	if (player_record != 2 || player == NULL || denied == NULL
	    || !movement_step(tape, MOVEMENT_GATE, error))
		return false;
	*player = tape->gate_player;
	*denied = tape->denied;
	return true;
}
static bool
movement_present_test(void *context, const uint8_t *text, size_t length,
    enum yt_movement_output_kind kind, struct yt_error *error)
{
	struct movement_tape *tape = context;

	if ((size_t)kind >= YT_ARRAY_LEN(tape->rows)
	    || length > sizeof(tape->rows[0])
	    || !movement_step(tape, MOVEMENT_PRESENT, error))
		return false;
	if (length != 0U)
		memcpy(tape->rows[kind], text, length);
	tape->row_lengths[kind] = length;
	tape->row_seen[kind] = true;
	return true;
}
static bool
movement_input_test(void *context, char *response, size_t capacity,
    struct yt_error *error)
{
	struct movement_tape *tape = context;
	const char *source;
	size_t length;

	if (tape->response_index >= tape->response_count)
		return false;
	source = tape->responses[tape->response_index++];
	length = strlen(source);
	if (length + 1U > capacity
	    || !movement_step(tape, MOVEMENT_INPUT, error))
		return false;
	memcpy(response, source, length + 1U);
	return true;
}
static bool
movement_danger_test(void *context, float target, bool *dangerous,
    struct yt_error *error)
{
	struct movement_tape *tape = context;

	if (target != 42.0f || dangerous == NULL
	    || !movement_step(tape, MOVEMENT_DANGER, error))
		return false;
	*dangerous = tape->dangerous;
	return true;
}
static void
movement_clear_queue_test(void *context)
{
	movement_record(context, MOVEMENT_CLEAR_QUEUE);
}
static bool
movement_confirm_test(void *context, const uint8_t *prompt, size_t length,
    bool *accepted, struct yt_error *error)
{
	static const uint8_t expected[] = "Move into sector 42? [y/N] ";
	struct movement_tape *tape = context;

	if (accepted == NULL || length != sizeof(expected) - 1U
	    || memcmp(prompt, expected, sizeof(expected) - 1U) != 0
	    || !movement_step(tape, MOVEMENT_CONFIRM, error))
		return false;
	*accepted = tape->confirmed;
	return true;
}
static bool
movement_finalize_test(void *context, struct yt_error *error)
{
	struct movement_tape *tape = context;

	if (!movement_step(tape, MOVEMENT_FINALIZE, error))
		return false;
	return tape->finalizer_result;
}
static void
movement_clear_self_mines_test(void *context)
{
	movement_record(context, MOVEMENT_CLEAR_SELF_MINES);
}
static bool
movement_hydrate_test(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct movement_tape *tape = context;

	if (player_record != 2
	    || !movement_step(tape, MOVEMENT_HYDRATE, error))
		return false;
	*player = tape->accepted_player;
	return true;
}
static bool
movement_write_player_test(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct movement_tape *tape = context;

	if (player_record != 2
	    || !movement_step(tape, MOVEMENT_WRITE_PLAYER, error))
		return false;
	tape->written_player = *player;
	return true;
}
static bool
movement_flush_test(void *context, struct yt_error *error)
{
	return movement_step(context, MOVEMENT_FLUSH, error);
}
static bool
movement_cache_test(void *context, int player_record, float target,
    struct yt_error *error)
{
	struct movement_tape *tape = context;

	if (player_record != 2 || target != 42.0f
	    || !movement_step(tape, MOVEMENT_CACHE, error))
		return false;
	tape->cached_target = target;
	return true;
}
static const struct yt_movement_ops movement_test_ops = {
	movement_gate_test,
	movement_present_test,
	movement_input_test,
	movement_danger_test,
	movement_clear_queue_test,
	movement_confirm_test,
	movement_finalize_test,
	movement_clear_self_mines_test,
	movement_hydrate_test,
	movement_write_player_test,
	movement_flush_test,
	movement_cache_test,
};
static void
movement_fixture(struct movement_tape *tape,
    struct yt_movement_state *state)
{
	memset(tape, 0, sizeof(*tape));
	memset(state, 0, sizeof(*state));
	tape->fail_at = SIZE_MAX;
	tape->confirmed = true;
	tape->finalizer_result = true;
	tape->responses[0] = "42";
	tape->response_count = 1U;
	memset(tape->gate_player.record.bytes, 0xa1,
	    sizeof(tape->gate_player.record.bytes));
	tape->gate_player.sector = 7.0f;
	tape->gate_player.danger_scanner = 0.0f;
	memset(tape->accepted_player.record.bytes, 0xb2,
	    sizeof(tape->accepted_player.record.bytes));
	tape->accepted_player.sector = 99.0f;
	*state = (struct yt_movement_state){
		.current_player_record = 2,
		.port_offset = 2055.0f,
		.sector_offset = 51.0f,
		.warps = {7.0f, 42.0f, 12.5f, 42.0f, 0.0f, 0.0f},
	};
}
static bool
check_movement_transaction(void)
{
	static const enum movement_event success_events[] = {
		MOVEMENT_GATE,
		MOVEMENT_PRESENT,
		MOVEMENT_PRESENT,
		MOVEMENT_PRESENT,
		MOVEMENT_INPUT,
		MOVEMENT_PRESENT,
		MOVEMENT_FINALIZE,
		MOVEMENT_CLEAR_SELF_MINES,
		MOVEMENT_HYDRATE,
		MOVEMENT_WRITE_PLAYER,
		MOVEMENT_FLUSH,
		MOVEMENT_CACHE,
	};
	static const enum movement_event danger_events[] = {
		MOVEMENT_GATE,
		MOVEMENT_PRESENT,
		MOVEMENT_PRESENT,
		MOVEMENT_PRESENT,
		MOVEMENT_INPUT,
		MOVEMENT_PRESENT,
		MOVEMENT_DANGER,
		MOVEMENT_PRESENT,
		MOVEMENT_CLEAR_QUEUE,
		MOVEMENT_CONFIRM,
		MOVEMENT_FINALIZE,
		MOVEMENT_CLEAR_SELF_MINES,
		MOVEMENT_HYDRATE,
		MOVEMENT_WRITE_PLAYER,
		MOVEMENT_FLUSH,
		MOVEMENT_CACHE,
	};
	static const uint8_t warp[] =
	    "Warps lead to, 7, 42, 12.5, 42";
	static const uint8_t destination[] = "Move to which sector? ";
	static const uint8_t same[] =
	    "That was quick! Felt like we didn't even move!";
	static const uint8_t adjacent[] = "You can't get there from here.";
	struct movement_tape tape;
	struct yt_movement_state state;
	struct yt_player expected;
	struct yt_error error;
	size_t failure;

	movement_fixture(&tape, &state);
	expected = tape.accepted_player;
	yt_movement_player_overlay(&expected, 42.0f);
	if (!yt_movement_run(&state, &movement_test_ops, &tape, NULL)
	    || !state.complete || state.route != YT_MOVEMENT_MOVED
	    || state.maximum != 2004.0f || state.target != 42.0f
	    || !state.target_stored || state.attempts != 1U
	    || !state.adjacent || state.matching_warp != 2U
	    || state.danger_called || state.confirmation_read
	    || !state.finalizer_called || !state.self_mine_suppression_cleared
	    || !state.player_hydrated || !state.player_written
	    || !state.player_flushed || !state.cache_updated
	    || tape.cached_target != 42.0f
	    || tape.calls != YT_ARRAY_LEN(success_events)
	    || memcmp(tape.events, success_events, sizeof(success_events)) != 0
	    || tape.row_lengths[YT_MOVEMENT_WARP_ROW] != sizeof(warp) - 1U
	    || memcmp(tape.rows[YT_MOVEMENT_WARP_ROW], warp,
	    sizeof(warp) - 1U) != 0
	    || tape.row_lengths[YT_MOVEMENT_DESTINATION_PROMPT]
	    != sizeof(destination) - 1U
	    || memcmp(tape.rows[YT_MOVEMENT_DESTINATION_PROMPT], destination,
	    sizeof(destination) - 1U) != 0
	    || memcmp(tape.written_player.record.bytes, expected.record.bytes,
	    sizeof(expected.record.bytes)) != 0)
		return false;

	for (failure = 0U; failure < YT_ARRAY_LEN(success_events); ++failure) {
		if (success_events[failure] == MOVEMENT_CLEAR_SELF_MINES)
			continue;
		movement_fixture(&tape, &state);
		tape.fail_at = failure;
		yt_error_clear(&error);
		if (yt_movement_run(&state, &movement_test_ops, &tape, &error)
		    || error.status != YT_IO_ERROR || state.complete
		    || tape.calls != failure + 1U
		    || memcmp(tape.events, success_events,
		    tape.calls * sizeof(success_events[0])) != 0)
			return false;
	}

	movement_fixture(&tape, &state);
	tape.denied = true;
	if (!yt_movement_run(&state, &movement_test_ops, &tape, NULL)
	    || state.route != YT_MOVEMENT_TURN_DENIED || tape.calls != 1U)
		return false;

	movement_fixture(&tape, &state);
	tape.responses[0] = "";
	if (!yt_movement_run(&state, &movement_test_ops, &tape, NULL)
	    || state.route != YT_MOVEMENT_BOUNDS_CANCELLED
	    || !state.target_stored || state.maximum != 2004.0f
	    || tape.calls != 5U)
		return false;

	movement_fixture(&tape, &state);
	tape.gate_player.sector = 42.0f;
	if (!yt_movement_run(&state, &movement_test_ops, &tape, NULL)
	    || state.route != YT_MOVEMENT_SAME_SECTOR_ROUTE
	    || tape.calls != 6U
	    || tape.row_lengths[YT_MOVEMENT_SAME_SECTOR] != sizeof(same) - 1U
	    || memcmp(tape.rows[YT_MOVEMENT_SAME_SECTOR], same,
	    sizeof(same) - 1U) != 0)
		return false;

	movement_fixture(&tape, &state);
	tape.responses[0] = "13";
	if (!yt_movement_run(&state, &movement_test_ops, &tape, NULL)
	    || state.route != YT_MOVEMENT_NOT_ADJACENT_ROUTE
	    || tape.calls != 6U
	    || tape.row_lengths[YT_MOVEMENT_NOT_ADJACENT]
	    != sizeof(adjacent) - 1U
	    || memcmp(tape.rows[YT_MOVEMENT_NOT_ADJACENT], adjacent,
	    sizeof(adjacent) - 1U) != 0)
		return false;

	movement_fixture(&tape, &state);
	tape.responses[0] = "M";
	tape.responses[1] = "42";
	tape.response_count = 2U;
	if (!yt_movement_run(&state, &movement_test_ops, &tape, NULL)
	    || state.route != YT_MOVEMENT_MOVED || state.attempts != 2U
	    || tape.calls != YT_ARRAY_LEN(success_events) + 2U
	    || tape.events[5] != MOVEMENT_PRESENT
	    || tape.events[6] != MOVEMENT_INPUT)
		return false;

	movement_fixture(&tape, &state);
	tape.gate_player.danger_scanner = 1.0f;
	tape.dangerous = true;
	tape.confirmed = false;
	if (!yt_movement_run(&state, &movement_test_ops, &tape, NULL)
	    || state.route != YT_MOVEMENT_DANGER_DECLINED
	    || !state.danger_called || !state.dangerous
	    || !state.confirmation_read || state.finalizer_called
	    || tape.calls != 10U
	    || memcmp(tape.events, danger_events,
	    10U * sizeof(danger_events[0])) != 0)
		return false;

	movement_fixture(&tape, &state);
	tape.gate_player.danger_scanner = 1.0f;
	tape.dangerous = true;
	if (!yt_movement_run(&state, &movement_test_ops, &tape, NULL)
	    || state.route != YT_MOVEMENT_MOVED
	    || tape.calls != YT_ARRAY_LEN(danger_events)
	    || memcmp(tape.events, danger_events, sizeof(danger_events)) != 0)
		return false;
	for (failure = 0U; failure < YT_ARRAY_LEN(danger_events); ++failure) {
		if (danger_events[failure] == MOVEMENT_CLEAR_QUEUE
		    || danger_events[failure] == MOVEMENT_CLEAR_SELF_MINES)
			continue;
		movement_fixture(&tape, &state);
		tape.gate_player.danger_scanner = 1.0f;
		tape.dangerous = true;
		tape.fail_at = failure;
		yt_error_clear(&error);
		if (yt_movement_run(&state, &movement_test_ops, &tape, &error)
		    || error.status != YT_IO_ERROR || state.complete
		    || tape.calls != failure + 1U
		    || memcmp(tape.events, danger_events,
		    tape.calls * sizeof(danger_events[0])) != 0)
			return false;
	}

	movement_fixture(&tape, &state);
	tape.finalizer_result = false;
	yt_error_clear(&error);
	if (!yt_movement_run(&state, &movement_test_ops, &tape, &error)
	    || state.route != YT_MOVEMENT_FINALIZER_TERMINAL
	    || !state.finalizer_called || state.self_mine_suppression_cleared
	    || tape.calls != 7U)
		return false;

	movement_fixture(&tape, &state);
	return !yt_movement_run(NULL, &movement_test_ops, &tape, NULL)
	    && !yt_movement_run(&state, NULL, &tape, NULL);
}

enum main_fighters_event {
	MAIN_FIGHTERS_PRESENT = 1,
	MAIN_FIGHTERS_HYDRATE,
	MAIN_FIGHTERS_READ_SECTOR_FIRST,
	MAIN_FIGHTERS_INPUT,
	MAIN_FIGHTERS_READ_SECTOR_ACCEPTED,
	MAIN_FIGHTERS_WRITE_SECTOR,
	MAIN_FIGHTERS_READ_PLAYER,
	MAIN_FIGHTERS_WRITE_PLAYER,
	MAIN_FIGHTERS_SOUND,
};
struct main_fighters_tape {
	enum main_fighters_event events[16];
	size_t calls;
	size_t fail_at;
	struct yt_player body_player;
	struct yt_sector first_sector;
	struct yt_sector accepted_sector;
	struct yt_player accepted_player;
	struct yt_sector written_sector;
	struct yt_player written_player;
	size_t sector_reads;
	const char *response;
	uint8_t rows[7][192];
	size_t row_lengths[7];
	bool row_seen[7];
};
static bool
main_fighters_step(struct main_fighters_tape *tape,
    enum main_fighters_event event, struct yt_error *error)
{
	size_t call = tape->calls++;

	if (call < YT_ARRAY_LEN(tape->events))
		tape->events[call] = event;
	if (call != tape->fail_at)
		return true;
	if (error != NULL)
		error->status = YT_IO_ERROR;
	return false;
}
static bool
main_fighters_hydrate_test(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct main_fighters_tape *tape = context;

	if (player_record != 2 || !main_fighters_step(tape,
	    MAIN_FIGHTERS_HYDRATE, error))
		return false;
	*player = tape->body_player;
	return true;
}
static bool
main_fighters_read_sector_test(void *context, int sector_number,
    struct yt_sector *sector, struct yt_error *error)
{
	struct main_fighters_tape *tape = context;
	enum main_fighters_event event = tape->sector_reads == 0U
	    ? MAIN_FIGHTERS_READ_SECTOR_FIRST
	    : MAIN_FIGHTERS_READ_SECTOR_ACCEPTED;

	if (sector_number != 8 || !main_fighters_step(tape, event, error))
		return false;
	*sector = tape->sector_reads++ == 0U
	    ? tape->first_sector : tape->accepted_sector;
	return true;
}
static bool
main_fighters_write_sector_test(void *context, int sector_number,
    struct yt_sector *sector, struct yt_error *error)
{
	struct main_fighters_tape *tape = context;

	if (sector_number != 8 || !main_fighters_step(tape,
	    MAIN_FIGHTERS_WRITE_SECTOR, error))
		return false;
	tape->written_sector = *sector;
	return true;
}
static bool
main_fighters_read_player_test(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct main_fighters_tape *tape = context;

	if (player_record != 2 || !main_fighters_step(tape,
	    MAIN_FIGHTERS_READ_PLAYER, error))
		return false;
	*player = tape->accepted_player;
	return true;
}
static bool
main_fighters_write_player_test(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct main_fighters_tape *tape = context;

	if (player_record != 2 || !main_fighters_step(tape,
	    MAIN_FIGHTERS_WRITE_PLAYER, error))
		return false;
	tape->written_player = *player;
	return true;
}
static bool
main_fighters_present_test(void *context, const uint8_t *text, size_t length,
    enum yt_main_fighters_output_kind kind, struct yt_error *error)
{
	struct main_fighters_tape *tape = context;

	if ((size_t)kind >= YT_ARRAY_LEN(tape->rows)
	    || length > sizeof(tape->rows[0])
	    || !main_fighters_step(tape, MAIN_FIGHTERS_PRESENT, error))
		return false;
	if (length != 0U)
		memcpy(tape->rows[kind], text, length);
	tape->row_lengths[kind] = length;
	tape->row_seen[kind] = true;
	return true;
}
static bool
main_fighters_input_test(void *context, char *response, size_t capacity,
    struct yt_error *error)
{
	struct main_fighters_tape *tape = context;
	size_t length = strlen(tape->response);

	if (length + 1U > capacity || !main_fighters_step(tape,
	    MAIN_FIGHTERS_INPUT, error))
		return false;
	memcpy(response, tape->response, length + 1U);
	return true;
}
static bool
main_fighters_sound_test(void *context, float selector,
    struct yt_error *error)
{
	return selector == 4.0f && main_fighters_step(context,
	    MAIN_FIGHTERS_SOUND, error);
}
static const struct yt_main_fighters_ops main_fighters_test_ops = {
	main_fighters_hydrate_test,
	main_fighters_read_sector_test,
	main_fighters_write_sector_test,
	main_fighters_read_player_test,
	main_fighters_write_player_test,
	main_fighters_present_test,
	main_fighters_input_test,
	main_fighters_sound_test,
};
static void
main_fighters_fixture(struct main_fighters_tape *tape,
    struct yt_main_fighters_state *state)
{
	memset(tape, 0, sizeof(*tape));
	memset(state, 0, sizeof(*state));
	tape->fail_at = SIZE_MAX;
	tape->response = "12";
	memset(tape->body_player.record.bytes, 0xa1,
	    sizeof(tape->body_player.record.bytes));
	tape->body_player.sector = 8.5f;
	tape->body_player.fighters = 8.0f;
	memset(tape->first_sector.record.bytes, 0xb2,
	    sizeof(tape->first_sector.record.bytes));
	tape->first_sector.fighters = 10.0f;
	tape->first_sector.fighter_owner = 2.0f;
	memset(tape->accepted_sector.record.bytes, 0xc3,
	    sizeof(tape->accepted_sector.record.bytes));
	tape->accepted_sector.fighters = 99.0f;
	tape->accepted_sector.fighter_owner = -2.0f;
	memset(tape->accepted_player.record.bytes, 0xd4,
	    sizeof(tape->accepted_player.record.bytes));
	tape->accepted_player.fighters = 77.0f;
	state->current_player_record = 2;
}
static bool
check_main_fighters_transaction(void)
{
	static const enum main_fighters_event success_events[] = {
		MAIN_FIGHTERS_PRESENT,
		MAIN_FIGHTERS_HYDRATE,
		MAIN_FIGHTERS_READ_SECTOR_FIRST,
		MAIN_FIGHTERS_PRESENT,
		MAIN_FIGHTERS_PRESENT,
		MAIN_FIGHTERS_INPUT,
		MAIN_FIGHTERS_READ_SECTOR_ACCEPTED,
		MAIN_FIGHTERS_WRITE_SECTOR,
		MAIN_FIGHTERS_READ_PLAYER,
		MAIN_FIGHTERS_WRITE_PLAYER,
		MAIN_FIGHTERS_PRESENT,
		MAIN_FIGHTERS_SOUND,
	};
	static const uint8_t title[] = "<Drop/Take Fighters>";
	static const uint8_t available[] = "You have 18 fighters available.";
	static const uint8_t prompt[] =
	    "Defend this sector with how many? ";
	static const uint8_t success[] = "Done.  You have 6 fighters left.";
	static const uint8_t union_refusal[] =
	    "You can't leave fighters in the Union (sectors 1-7)";
	static const uint8_t foreign_refusal[] =
	    "There are already fighters in this sector!";
	static const uint8_t insufficient[] = "You don't have that many!";
	struct main_fighters_tape tape;
	struct yt_main_fighters_state state;
	struct yt_sector expected_sector;
	struct yt_player expected_player;
	struct yt_error error;
	size_t failure;

	main_fighters_fixture(&tape, &state);
	expected_sector = tape.accepted_sector;
	expected_player = tape.accepted_player;
	if (!yt_main_fighters_sector_overlay(&expected_sector,
	    (const uint8_t[]){0, 0, 0x40, 0x84}, 2)
	    || !yt_main_fighters_player_overlay(&expected_player, 6.0f)
	    || !yt_main_fighters_run(&state, &main_fighters_test_ops, &tape,
	    NULL)
	    || !state.complete || state.route != YT_MAIN_FIGHTERS_ACCEPTED_ROUTE
	    || !state.player_hydrated || !state.first_sector_read
	    || !state.input_read || !state.desired_stored
	    || !state.accepted_sector_read || !state.sector_written
	    || !state.accepted_player_read || !state.player_written
	    || !state.sound_called || state.logical_sector != 8
	    || state.available != 18.0 || state.desired != 12.0f
	    || state.delta != -2.0f || state.remaining != 6.0f
	    || state.player.fighters != 8.0f
	    || tape.calls != YT_ARRAY_LEN(success_events)
	    || memcmp(tape.events, success_events, sizeof(success_events)) != 0
	    || !tape.row_seen[YT_MAIN_FIGHTERS_TITLE]
	    || tape.row_lengths[YT_MAIN_FIGHTERS_TITLE] != sizeof(title) - 1U
	    || memcmp(tape.rows[YT_MAIN_FIGHTERS_TITLE], title,
	    sizeof(title) - 1U) != 0
	    || tape.row_lengths[YT_MAIN_FIGHTERS_AVAILABLE]
	    != sizeof(available) - 1U
	    || memcmp(tape.rows[YT_MAIN_FIGHTERS_AVAILABLE], available,
	    sizeof(available) - 1U) != 0
	    || tape.row_lengths[YT_MAIN_FIGHTERS_PROMPT] != sizeof(prompt) - 1U
	    || memcmp(tape.rows[YT_MAIN_FIGHTERS_PROMPT], prompt,
	    sizeof(prompt) - 1U) != 0
	    || tape.row_lengths[YT_MAIN_FIGHTERS_SUCCESS]
	    != sizeof(success) - 1U
	    || memcmp(tape.rows[YT_MAIN_FIGHTERS_SUCCESS], success,
	    sizeof(success) - 1U) != 0
	    || memcmp(tape.written_sector.record.bytes,
	    expected_sector.record.bytes, sizeof(expected_sector.record.bytes))
	    != 0
	    || memcmp(tape.written_player.record.bytes,
	    expected_player.record.bytes, sizeof(expected_player.record.bytes))
	    != 0)
		return false;

	for (failure = 0U; failure < YT_ARRAY_LEN(success_events); ++failure) {
		main_fighters_fixture(&tape, &state);
		tape.fail_at = failure;
		yt_error_clear(&error);
		if (yt_main_fighters_run(&state, &main_fighters_test_ops, &tape,
		    &error) || error.status != YT_IO_ERROR || state.complete
		    || tape.calls != failure + 1U
		    || memcmp(tape.events, success_events,
		    tape.calls * sizeof(success_events[0])) != 0)
			return false;
	}

	main_fighters_fixture(&tape, &state);
	tape.body_player.sector = 7.5f;
	if (!yt_main_fighters_run(&state, &main_fighters_test_ops, &tape, NULL)
	    || state.route != YT_MAIN_FIGHTERS_UNION_ROUTE || tape.calls != 3U
	    || tape.sector_reads != 0U
	    || tape.row_lengths[YT_MAIN_FIGHTERS_UNION_REFUSAL]
	    != sizeof(union_refusal) - 1U
	    || memcmp(tape.rows[YT_MAIN_FIGHTERS_UNION_REFUSAL], union_refusal,
	    sizeof(union_refusal) - 1U) != 0)
		return false;

	main_fighters_fixture(&tape, &state);
	tape.first_sector.fighter_owner = 3.0f;
	if (!yt_main_fighters_run(&state, &main_fighters_test_ops, &tape, NULL)
	    || state.route != YT_MAIN_FIGHTERS_FOREIGN_ROUTE || tape.calls != 4U
	    || tape.row_lengths[YT_MAIN_FIGHTERS_FOREIGN_REFUSAL]
	    != sizeof(foreign_refusal) - 1U
	    || memcmp(tape.rows[YT_MAIN_FIGHTERS_FOREIGN_REFUSAL],
	    foreign_refusal, sizeof(foreign_refusal) - 1U) != 0)
		return false;

	main_fighters_fixture(&tape, &state);
	tape.response = "";
	tape.first_sector.fighters = 0.0f;
	tape.first_sector.fighter_owner = -2.0f;
	if (!yt_main_fighters_run(&state, &main_fighters_test_ops, &tape, NULL)
	    || state.route != YT_MAIN_FIGHTERS_CANCELLED_ROUTE
	    || tape.calls != 6U || state.desired_stored)
		return false;

	main_fighters_fixture(&tape, &state);
	tape.response = "-.1";
	if (!yt_main_fighters_run(&state, &main_fighters_test_ops, &tape, NULL)
	    || state.route != YT_MAIN_FIGHTERS_CANCELLED_ROUTE
	    || state.desired != -1.0f || !state.desired_stored
	    || tape.calls != 6U)
		return false;

	main_fighters_fixture(&tape, &state);
	tape.response = "20";
	if (!yt_main_fighters_run(&state, &main_fighters_test_ops, &tape, NULL)
	    || state.route != YT_MAIN_FIGHTERS_INSUFFICIENT_ROUTE
	    || state.remaining != -2.0f || tape.calls != 7U
	    || tape.row_lengths[YT_MAIN_FIGHTERS_INSUFFICIENT]
	    != sizeof(insufficient) - 1U
	    || memcmp(tape.rows[YT_MAIN_FIGHTERS_INSUFFICIENT], insufficient,
	    sizeof(insufficient) - 1U) != 0)
		return false;

	/* Zero is accepted and deliberately leaves a nonzero owner. */
	main_fighters_fixture(&tape, &state);
	tape.response = "0";
	if (!yt_main_fighters_run(&state, &main_fighters_test_ops, &tape, NULL)
	    || state.route != YT_MAIN_FIGHTERS_ACCEPTED_ROUTE
	    || state.desired != 0.0f || state.remaining != 18.0f
	    || yt_record_get_number(&tape.written_sector.record, YT_F81) != 0.0f
	    || yt_record_get_number(&tape.written_sector.record, YT_F85) != 2.0f
	    || tape.calls != YT_ARRAY_LEN(success_events))
		return false;

	main_fighters_fixture(&tape, &state);
	return !yt_main_fighters_run(NULL, &main_fighters_test_ops, &tape, NULL)
	    && !yt_main_fighters_run(&state, NULL, &tape, NULL)
	    && !yt_main_fighters_sector_overlay(NULL, state.desired_raw, 2)
	    && !yt_main_fighters_player_overlay(NULL, 1.0f);
}

enum genesis_event {
	GENESIS_HYDRATE = 1,
	GENESIS_PRESENT,
	GENESIS_CONFIRM,
	GENESIS_HANDOFF,
};
struct genesis_tape {
	enum genesis_event events[12];
	size_t calls;
	size_t fail_at;
	struct yt_player player;
	bool answer;
	uint8_t rows[10][256];
	size_t row_lengths[10];
	enum yt_genesis_output_kind kinds[10];
	size_t row_count;
	uint8_t prompt[128];
	size_t prompt_length;
};
static bool
genesis_step(struct genesis_tape *tape, enum genesis_event event,
    struct yt_error *error)
{
	size_t call = tape->calls++;

	if (call < YT_ARRAY_LEN(tape->events))
		tape->events[call] = event;
	if (call != tape->fail_at)
		return true;
	if (error != NULL)
		error->status = YT_IO_ERROR;
	return false;
}
static bool
genesis_hydrate_test(void *context, int record, struct yt_player *player,
    struct yt_error *error)
{
	struct genesis_tape *tape = context;

	if (record != 2 || !genesis_step(tape, GENESIS_HYDRATE, error))
		return false;
	*player = tape->player;
	return true;
}
static bool
genesis_present_test(void *context, const uint8_t *text, size_t length,
    enum yt_genesis_output_kind kind, struct yt_error *error)
{
	struct genesis_tape *tape = context;
	size_t row = tape->row_count;

	if (row >= YT_ARRAY_LEN(tape->rows) || length > sizeof(tape->rows[0])
	    || !genesis_step(tape, GENESIS_PRESENT, error))
		return false;
	if (length != 0U)
		memcpy(tape->rows[row], text, length);
	tape->row_lengths[row] = length;
	tape->kinds[row] = kind;
	tape->row_count++;
	return true;
}
static bool
genesis_confirm_test(void *context, const uint8_t *prompt, size_t length,
    bool *accepted, struct yt_error *error)
{
	struct genesis_tape *tape = context;

	if (accepted == NULL || length > sizeof(tape->prompt)
	    || !genesis_step(tape, GENESIS_CONFIRM, error))
		return false;
	memcpy(tape->prompt, prompt, length);
	tape->prompt_length = length;
	*accepted = tape->answer;
	return true;
}
static bool
genesis_handoff_test(void *context, struct yt_error *error)
{
	return genesis_step(context, GENESIS_HANDOFF, error);
}
static const struct yt_genesis_ops genesis_test_ops = {
	genesis_hydrate_test,
	genesis_present_test,
	genesis_confirm_test,
	genesis_handoff_test,
};
static void
genesis_fixture(struct genesis_tape *tape, struct yt_genesis_state *state)
{
	static const uint8_t trader[] = {'T', 0, 'R'};

	memset(tape, 0, sizeof(*tape));
	memset(state, 0, sizeof(*state));
	tape->fail_at = SIZE_MAX;
	tape->player.ports_owned = 5.0f;
	tape->answer = true;
	*state = (struct yt_genesis_state){
		.current_player_record = 2,
		.required_ports = 5.0f,
		.cached_trader = trader,
		.cached_trader_length = sizeof(trader),
	};
}
static bool
check_genesis_transaction(void)
{
	static const enum genesis_event success_events[] = {
		GENESIS_HYDRATE, GENESIS_PRESENT, GENESIS_PRESENT,
		GENESIS_PRESENT, GENESIS_CONFIRM, GENESIS_PRESENT,
		GENESIS_PRESENT, GENESIS_PRESENT, GENESIS_HANDOFF,
	};
	static const uint8_t prompt[] = {
		'A','r','e',' ','y','o','u',' ','t','h','a','t',' ','T','r','a',
		'd','e','r',' ','T',0,'R',' ','[','y','/','N',']'
	};
	static const uint8_t prophecy_first[] =
	    "It has been written that one day a Trader Baron will rise up";
	static const uint8_t success_second[] =
	    "would wipe away the all of the evil in the universe.....";
	static const uint8_t declined[] =
	    "Alas, today is not the day that the prophesy will be fullfilled.";
	static const uint8_t disabled[] = "*FUNCTION DISABLED*";
	static const uint8_t insufficient_first[] =
	    "You are not up to the challenge. You must own 5 ports before you are powerful";
	static const uint8_t insufficient_second[] =
	    "enough to initiate Genesis. You are .5 short of fulfilling the prophesy.";
	struct genesis_tape tape;
	struct yt_genesis_state state;
	struct yt_error error;
	size_t failure;

	genesis_fixture(&tape, &state);
	if (!yt_genesis_run(&state, &genesis_test_ops, &tape, NULL)
	    || !state.complete || state.route != YT_GENESIS_HANDOFF_ROUTE
	    || !state.player_hydrated || !state.confirmation_read
	    || !state.answer || state.disabled_presented || !state.handoff_called
	    || tape.calls != YT_ARRAY_LEN(success_events)
	    || memcmp(tape.events, success_events, sizeof(success_events)) != 0
	    || tape.prompt_length != sizeof(prompt)
	    || memcmp(tape.prompt, prompt, sizeof(prompt)) != 0
	    || tape.row_count != 6U
	    || tape.kinds[0] != YT_GENESIS_PROPHECY_FIRST
	    || tape.row_lengths[0] != sizeof(prophecy_first) - 1U
	    || memcmp(tape.rows[0], prophecy_first,
	    sizeof(prophecy_first) - 1U) != 0
	    || tape.kinds[3] != YT_GENESIS_SUCCESS_BLANK
	    || tape.row_lengths[3] != 0U
	    || tape.kinds[5] != YT_GENESIS_SUCCESS_SECOND
	    || tape.row_lengths[5] != sizeof(success_second) - 1U
	    || memcmp(tape.rows[5], success_second,
	    sizeof(success_second) - 1U) != 0)
		return false;
	for (failure = 0U; failure < YT_ARRAY_LEN(success_events); ++failure) {
		genesis_fixture(&tape, &state);
		tape.fail_at = failure;
		yt_error_clear(&error);
		if (yt_genesis_run(&state, &genesis_test_ops, &tape, &error)
		    || error.status != YT_IO_ERROR || state.complete
		    || tape.calls != failure + 1U
		    || memcmp(tape.events, success_events,
		    tape.calls * sizeof(success_events[0])) != 0)
			return false;
	}

	genesis_fixture(&tape, &state);
	tape.answer = false;
	if (!yt_genesis_run(&state, &genesis_test_ops, &tape, NULL)
	    || state.route != YT_GENESIS_DECLINED_ROUTE || tape.calls != 6U
	    || tape.row_count != 4U || tape.kinds[3] != YT_GENESIS_DECLINED
	    || tape.row_lengths[3] != sizeof(declined) - 1U
	    || memcmp(tape.rows[3], declined, sizeof(declined) - 1U) != 0
	    || state.handoff_called)
		return false;

	/* The cutoff runs after confirmation and forces even a typed Y to N. */
	genesis_fixture(&tape, &state);
	state.required_ports = 300.5f;
	if (!yt_genesis_run(&state, &genesis_test_ops, &tape, NULL)
	    || state.route != YT_GENESIS_DISABLED_ROUTE || state.answer
	    || !state.disabled_presented || tape.calls != 7U
	    || tape.kinds[3] != YT_GENESIS_DISABLED
	    || tape.row_lengths[3] != sizeof(disabled) - 1U
	    || memcmp(tape.rows[3], disabled, sizeof(disabled) - 1U) != 0
	    || tape.kinds[4] != YT_GENESIS_DECLINED)
		return false;

	genesis_fixture(&tape, &state);
	tape.player.ports_owned = 4.5f;
	if (!yt_genesis_run(&state, &genesis_test_ops, &tape, NULL)
	    || state.route != YT_GENESIS_INSUFFICIENT_ROUTE || tape.calls != 7U
	    || tape.row_count != 5U
	    || tape.kinds[3] != YT_GENESIS_INSUFFICIENT_FIRST
	    || tape.row_lengths[3] != sizeof(insufficient_first) - 1U
	    || memcmp(tape.rows[3], insufficient_first,
	    sizeof(insufficient_first) - 1U) != 0
	    || tape.kinds[4] != YT_GENESIS_INSUFFICIENT_SECOND
	    || tape.row_lengths[4] != sizeof(insufficient_second) - 1U
	    || memcmp(tape.rows[4], insufficient_second,
	    sizeof(insufficient_second) - 1U) != 0)
		return false;

	genesis_fixture(&tape, &state);
	return !yt_genesis_run(NULL, &genesis_test_ops, &tape, NULL)
	    && !yt_genesis_run(&state, NULL, &tape, NULL);
}

enum port_rename_event {
	PORT_RENAME_HYDRATE = 1,
	PORT_RENAME_READ_SECTOR,
	PORT_RENAME_READ_PORT,
	PORT_RENAME_PRESENT,
	PORT_RENAME_EDIT,
};
struct port_rename_tape {
	enum port_rename_event events[8];
	size_t calls;
	size_t fail_at;
	struct yt_player player;
	struct yt_sector sector;
	struct yt_port port;
	int expected_logical;
	uint8_t row[64];
	size_t row_length;
	enum yt_port_rename_output_kind kind;
	uint8_t cached[YT_TEXT_FIELD_SIZE];
	size_t cached_length;
	struct yt_port edited_port;
};
static bool
port_rename_step(struct port_rename_tape *tape,
    enum port_rename_event event, struct yt_error *error)
{
	size_t call = tape->calls++;

	if (call < YT_ARRAY_LEN(tape->events))
		tape->events[call] = event;
	if (call != tape->fail_at)
		return true;
	if (error != NULL)
		error->status = YT_IO_ERROR;
	return false;
}
static bool
port_rename_hydrate_test(void *context, int record,
    struct yt_player *player, struct yt_error *error)
{
	struct port_rename_tape *tape = context;

	if (record != 2
	    || !port_rename_step(tape, PORT_RENAME_HYDRATE, error))
		return false;
	*player = tape->player;
	return true;
}
static bool
port_rename_read_sector_test(void *context, int sector_number,
    struct yt_sector *sector, struct yt_error *error)
{
	struct port_rename_tape *tape = context;

	if (sector_number != 42
	    || !port_rename_step(tape, PORT_RENAME_READ_SECTOR, error))
		return false;
	*sector = tape->sector;
	return true;
}
static bool
port_rename_read_port_test(void *context, int logical_port,
    struct yt_port *port, struct yt_error *error)
{
	struct port_rename_tape *tape = context;

	if (logical_port != tape->expected_logical
	    || !port_rename_step(tape, PORT_RENAME_READ_PORT, error))
		return false;
	*port = tape->port;
	return true;
}
static bool
port_rename_present_test(void *context, const uint8_t *text, size_t length,
    enum yt_port_rename_output_kind kind, struct yt_error *error)
{
	struct port_rename_tape *tape = context;

	if (length > sizeof(tape->row)
	    || !port_rename_step(tape, PORT_RENAME_PRESENT, error))
		return false;
	memcpy(tape->row, text, length);
	tape->row_length = length;
	tape->kind = kind;
	return true;
}
static bool
port_rename_edit_test(void *context, int logical_port,
    const uint8_t *cached, size_t cached_length, struct yt_port *port,
    struct yt_error *error)
{
	struct port_rename_tape *tape = context;

	if (logical_port != tape->expected_logical
	    || cached_length > sizeof(tape->cached)
	    || !port_rename_step(tape, PORT_RENAME_EDIT, error))
		return false;
	memcpy(tape->cached, cached, cached_length);
	tape->cached_length = cached_length;
	tape->edited_port = *port;
	return true;
}
static const struct yt_port_rename_ops port_rename_test_ops = {
	port_rename_hydrate_test,
	port_rename_read_sector_test,
	port_rename_read_port_test,
	port_rename_present_test,
	port_rename_edit_test,
};
static void
port_rename_fixture(struct port_rename_tape *tape,
    struct yt_port_rename_state *state)
{
	static const uint8_t name[] = {'O', 0, 'l', 'd'};

	memset(tape, 0, sizeof(*tape));
	memset(state, 0, sizeof(*state));
	tape->fail_at = SIZE_MAX;
	tape->player.sector = 42.0f;
	tape->sector.port = 3.0f;
	(void)yt_record_set_number(&tape->sector.record, YT_F65, 3.0f);
	memset(tape->port.record.bytes, ' ', YT_TEXT_FIELD_SIZE);
	memcpy(tape->port.record.bytes, name, sizeof(name));
	tape->port.owner = 2.25f;
	tape->port.name_length = 3.6f;
	tape->expected_logical = 3;
	*state = (struct yt_port_rename_state){
		.current_player_record = 2.25f,
		.port_offset = 100.0f,
		.conversion_mode = 4U,
	};
}
static bool
check_port_rename_transaction(void)
{
	static const enum port_rename_event edited_events[] = {
		PORT_RENAME_HYDRATE, PORT_RENAME_READ_SECTOR,
		PORT_RENAME_READ_PORT, PORT_RENAME_EDIT,
	};
	static const uint8_t no_port[] = "No port here!";
	static const uint8_t not_owner[] = "This isn't your port!";
	static const uint8_t earth[] = "Can't rename Earth!";
	struct port_rename_tape tape;
	struct yt_port_rename_state state;
	struct yt_error error;
	size_t failure;

	port_rename_fixture(&tape, &state);
	if (!yt_port_rename_run(&state, &port_rename_test_ops, &tape, NULL)
	    || !state.complete || state.route != YT_PORT_RENAME_EDITED_ROUTE
	    || state.hydration_record != 2 || !state.player_hydrated
	    || !state.sector_read || !state.port_read || !state.editor_called
	    || state.logical_port != 3 || state.relative_port != 3.0f
	    || state.cached_name_length != 3U || tape.cached_length != 3U
	    || memcmp(tape.cached, "O\0l", 3U) != 0
	    || tape.calls != YT_ARRAY_LEN(edited_events)
	    || memcmp(tape.events, edited_events, sizeof(edited_events)) != 0)
		return false;
	for (failure = 0U; failure < YT_ARRAY_LEN(edited_events); ++failure) {
		port_rename_fixture(&tape, &state);
		tape.fail_at = failure;
		yt_error_clear(&error);
		if (yt_port_rename_run(&state, &port_rename_test_ops, &tape,
		    &error) || error.status != YT_IO_ERROR || state.complete
		    || tape.calls != failure + 1U
		    || memcmp(tape.events, edited_events,
		    tape.calls * sizeof(edited_events[0])) != 0)
			return false;
	}

	port_rename_fixture(&tape, &state);
	memcpy(tape.sector.record.bytes + YT_F65, "\x01\x02\x03\0", 4U);
	if (!yt_port_rename_run(&state, &port_rename_test_ops, &tape, NULL)
	    || state.route != YT_PORT_RENAME_NO_PORT_ROUTE || tape.calls != 3U
	    || tape.kind != YT_PORT_RENAME_NO_PORT
	    || tape.row_length != sizeof(no_port) - 1U
	    || memcmp(tape.row, no_port, sizeof(no_port) - 1U) != 0)
		return false;

	/* Ownership is tested against the uncoerced 2.25 cell before Earth. */
	port_rename_fixture(&tape, &state);
	tape.sector.port = 1.0f;
	(void)yt_record_set_number(&tape.sector.record, YT_F65, 1.0f);
	tape.expected_logical = 1;
	tape.port.owner = 2.0f;
	if (!yt_port_rename_run(&state, &port_rename_test_ops, &tape, NULL)
	    || state.route != YT_PORT_RENAME_NOT_OWNER_ROUTE
	    || tape.kind != YT_PORT_RENAME_NOT_OWNER
	    || tape.row_length != sizeof(not_owner) - 1U
	    || memcmp(tape.row, not_owner, sizeof(not_owner) - 1U) != 0)
		return false;
	port_rename_fixture(&tape, &state);
	tape.sector.port = 1.0f;
	(void)yt_record_set_number(&tape.sector.record, YT_F65, 1.0f);
	tape.expected_logical = 1;
	if (!yt_port_rename_run(&state, &port_rename_test_ops, &tape, NULL)
	    || state.route != YT_PORT_RENAME_EARTH_ROUTE
	    || tape.kind != YT_PORT_RENAME_EARTH
	    || tape.row_length != sizeof(earth) - 1U
	    || memcmp(tape.row, earth, sizeof(earth) - 1U) != 0)
		return false;

	port_rename_fixture(&tape, &state);
	state.conversion_mode = 0U;
	if (!yt_port_rename_run(&state, &port_rename_test_ops, &tape, NULL)
	    || state.cached_name_length != 4U || tape.cached_length != 4U
	    || memcmp(tape.cached, "O\0ld", 4U) != 0)
		return false;
	port_rename_fixture(&tape, &state);
	tape.port.name_length = 32767.0f;
	if (!yt_port_rename_run(&state, &port_rename_test_ops, &tape, NULL)
	    || state.cached_name_length != YT_TEXT_FIELD_SIZE
	    || tape.cached_length != YT_TEXT_FIELD_SIZE)
		return false;
	port_rename_fixture(&tape, &state);
	tape.port.name_length = -1.0f;
	yt_error_clear(&error);
	if (yt_port_rename_run(&state, &port_rename_test_ops, &tape, &error)
	    || error.status != YT_RANGE || tape.calls != 3U
	    || strcmp(error.operation, "port name length") != 0
	    || state.editor_called)
		return false;

	port_rename_fixture(&tape, &state);
	return !yt_port_rename_run(NULL, &port_rename_test_ops, &tape, NULL)
	    && !yt_port_rename_run(&state, NULL, &tape, NULL);
}

struct port_rename_cycle_tape {
	int events[2];
	size_t calls;
	size_t fail_at;
};
static bool
port_rename_cycle_step(void *context, int event, struct yt_error *error)
{
	struct port_rename_cycle_tape *tape = context;
	size_t call = tape->calls++;

	if (call < YT_ARRAY_LEN(tape->events))
		tape->events[call] = event;
	if (call != tape->fail_at)
		return true;
	if (error != NULL)
		error->status = YT_IO_ERROR;
	return false;
}
static bool
port_rename_cycle_rename_test(void *context, struct yt_error *error)
{
	return port_rename_cycle_step(context, 1, error);
}
static bool
port_rename_cycle_scanner_test(void *context, struct yt_error *error)
{
	return port_rename_cycle_step(context, 2, error);
}
static bool
check_port_rename_cycle_transaction(void)
{
	static const struct yt_port_rename_cycle_ops ops = {
		port_rename_cycle_rename_test,
		port_rename_cycle_scanner_test,
	};
	struct port_rename_cycle_tape tape;
	struct yt_port_rename_cycle_state state;
	struct yt_error error;
	size_t failure;

	memset(&tape, 0, sizeof(tape));
	tape.fail_at = SIZE_MAX;
	if (!yt_port_rename_cycle_run(&state, &ops, &tape, NULL)
	    || !state.complete || !state.rename_complete
	    || !state.scanner_complete || tape.calls != 2U
	    || tape.events[0] != 1 || tape.events[1] != 2)
		return false;
	for (failure = 0U; failure < 2U; ++failure) {
		memset(&tape, 0, sizeof(tape));
		tape.fail_at = failure;
		yt_error_clear(&error);
		if (yt_port_rename_cycle_run(&state, &ops, &tape, &error)
		    || error.status != YT_IO_ERROR || state.complete
		    || tape.calls != failure + 1U
		    || state.rename_complete != (failure != 0U)
		    || state.scanner_complete)
			return false;
	}
	return !yt_port_rename_cycle_run(NULL, &ops, &tape, NULL)
	    && !yt_port_rename_cycle_run(&state, NULL, &tape, NULL);
}

enum main_prompt_event {
	MAIN_PROMPT_RESET = 1,
	MAIN_PROMPT_HYDRATE,
	MAIN_PROMPT_FOREGROUND,
	MAIN_PROMPT_BLANK,
	MAIN_PROMPT_SCANNER_RESET,
	MAIN_PROMPT_TEXT,
	MAIN_PROMPT_EDIT,
};
struct main_prompt_tape {
	enum main_prompt_event events[8];
	size_t calls;
	size_t fail_at;
	struct yt_player player;
	const char *response;
	bool available;
	uint8_t rows[2][128];
	size_t row_lengths[2];
	enum yt_main_prompt_output_kind kinds[2];
	size_t row_count;
};
static bool
main_prompt_step(struct main_prompt_tape *tape,
    enum main_prompt_event event, struct yt_error *error)
{
	size_t call = tape->calls++;

	if (call < YT_ARRAY_LEN(tape->events))
		tape->events[call] = event;
	if (call != tape->fail_at)
		return true;
	if (error != NULL)
		error->status = YT_IO_ERROR;
	return false;
}
static void
main_prompt_effect_test(void *context, enum yt_main_prompt_effect effect)
{
	static const enum main_prompt_event events[] = {
		MAIN_PROMPT_RESET, MAIN_PROMPT_FOREGROUND,
		MAIN_PROMPT_SCANNER_RESET,
	};
	struct main_prompt_tape *tape = context;

	if ((size_t)effect < YT_ARRAY_LEN(events))
		(void)main_prompt_step(tape, events[effect], NULL);
}
static bool
main_prompt_hydrate_test(void *context, int record,
    struct yt_player *player, struct yt_error *error)
{
	struct main_prompt_tape *tape = context;

	if (record != 2
	    || !main_prompt_step(tape, MAIN_PROMPT_HYDRATE, error))
		return false;
	*player = tape->player;
	return true;
}
static bool
main_prompt_present_test(void *context, const uint8_t *text, size_t length,
    enum yt_main_prompt_output_kind kind, struct yt_error *error)
{
	struct main_prompt_tape *tape = context;
	size_t row = tape->row_count;
	enum main_prompt_event event = kind == YT_MAIN_PROMPT_LEADING_BLANK
	    ? MAIN_PROMPT_BLANK : MAIN_PROMPT_TEXT;

	if (row >= YT_ARRAY_LEN(tape->rows) || length > sizeof(tape->rows[0])
	    || !main_prompt_step(tape, event, error))
		return false;
	if (length != 0U)
		memcpy(tape->rows[row], text, length);
	tape->row_lengths[row] = length;
	tape->kinds[row] = kind;
	tape->row_count++;
	return true;
}
static bool
main_prompt_edit_test(void *context, char *response, size_t capacity,
    size_t *length, bool *available, struct yt_error *error)
{
	struct main_prompt_tape *tape = context;
	size_t response_length = strlen(tape->response);

	if (length == NULL || available == NULL
	    || response_length >= capacity
	    || !main_prompt_step(tape, MAIN_PROMPT_EDIT, error))
		return false;
	memcpy(response, tape->response, response_length + 1U);
	*length = response_length;
	*available = tape->available;
	return true;
}
static const struct yt_main_prompt_ops main_prompt_test_ops = {
	main_prompt_effect_test,
	main_prompt_hydrate_test,
	main_prompt_present_test,
	main_prompt_edit_test,
};
static void
main_prompt_fixture(struct main_prompt_tape *tape,
    struct yt_main_prompt_state *state, char response[32])
{
	static const uint8_t time_text[] = " 14:59  ";

	memset(tape, 0, sizeof(*tape));
	memset(state, 0, sizeof(*state));
	memset(response, 0xa5, 32U);
	tape->fail_at = SIZE_MAX;
	tape->player.credits = 123.0f;
	tape->response = "BJUNK";
	tape->available = true;
	*state = (struct yt_main_prompt_state){
		.current_player_record = 2,
		.time_text = time_text,
		.time_text_length = sizeof(time_text) - 1U,
		.time_text_capacity = sizeof(time_text) - 1U,
		.response = response,
		.response_capacity = 32U,
	};
}
static bool
check_main_prompt_transaction(void)
{
	static const enum main_prompt_event expected[] = {
		MAIN_PROMPT_RESET, MAIN_PROMPT_HYDRATE,
		MAIN_PROMPT_FOREGROUND, MAIN_PROMPT_BLANK,
		MAIN_PROMPT_SCANNER_RESET, MAIN_PROMPT_TEXT,
		MAIN_PROMPT_EDIT,
	};
	static const size_t failable[] = {1U, 3U, 5U, 6U};
	static const uint8_t prompt[] =
	    "Time: 14:59  Main Command (?=Help)? ";
	struct main_prompt_tape tape;
	struct yt_main_prompt_state state;
	struct yt_error error;
	char response[32];
	size_t index;

	main_prompt_fixture(&tape, &state, response);
	if (!yt_main_prompt_run(&state, &main_prompt_test_ops, &tape, NULL)
	    || !state.complete || !state.player_hydrated
	    || !state.prompt_presented || !state.input_available
	    || state.player.credits != 123.0f
	    || state.route != YT_MAIN_SHELL_BUY_PORT
	    || state.response_length != 5U || strcmp(response, "BJUNK") != 0
	    || tape.calls != YT_ARRAY_LEN(expected)
	    || memcmp(tape.events, expected, sizeof(expected)) != 0
	    || tape.row_count != 2U || tape.row_lengths[0] != 0U
	    || tape.kinds[0] != YT_MAIN_PROMPT_LEADING_BLANK
	    || tape.row_lengths[1] != sizeof(prompt) - 1U
	    || tape.kinds[1] != YT_MAIN_PROMPT_TEXT
	    || memcmp(tape.rows[1], prompt, sizeof(prompt) - 1U) != 0)
		return false;
	for (index = 0U; index < YT_ARRAY_LEN(failable); ++index) {
		main_prompt_fixture(&tape, &state, response);
		tape.fail_at = failable[index];
		yt_error_clear(&error);
		if (yt_main_prompt_run(&state, &main_prompt_test_ops, &tape,
		    &error) || error.status != YT_IO_ERROR || state.complete
		    || tape.calls != failable[index] + 1U
		    || memcmp(tape.events, expected,
		    tape.calls * sizeof(expected[0])) != 0)
			return false;
	}
	main_prompt_fixture(&tape, &state, response);
	tape.available = false;
	tape.response = "";
	if (!yt_main_prompt_run(&state, &main_prompt_test_ops, &tape, NULL)
	    || !state.complete || state.input_available
	    || state.response_length != 0U || response[0] != '\0')
		return false;
	main_prompt_fixture(&tape, &state, response);
	state.time_text_length++;
	yt_error_clear(&error);
	if (yt_main_prompt_run(&state, &main_prompt_test_ops, &tape, &error)
	    || error.status != YT_RANGE || tape.calls != 5U
	    || strcmp(error.operation, "main prompt time capacity") != 0)
		return false;
	main_prompt_fixture(&tape, &state, response);
	return !yt_main_prompt_run(NULL, &main_prompt_test_ops, &tape, NULL)
	    && !yt_main_prompt_run(&state, NULL, &tape, NULL);
}

enum port_purchase_accept_event {
	PORT_ACCEPT_PRESENT = 1,
	PORT_ACCEPT_READ_PORT,
	PORT_ACCEPT_READ_PLAYER,
	PORT_ACCEPT_WRITE_PLAYER,
	PORT_ACCEPT_RADIO,
	PORT_ACCEPT_RENAME,
	PORT_ACCEPT_WRITE_PORT,
	PORT_ACCEPT_HYDRATE,
};
struct port_purchase_accept_tape {
	enum port_purchase_accept_event events[24];
	size_t calls;
	size_t fail_at;
	struct yt_port port;
	struct yt_player seller;
	struct yt_player buyer;
	struct yt_player written_seller;
	struct yt_player written_buyer;
	struct yt_port written_port;
	uint8_t rows[6][256];
	size_t row_lengths[6];
	enum yt_port_purchase_accept_output_kind kinds[6];
	size_t row_count;
	uint8_t radio[256];
	size_t radio_length;
	float sender;
	float recipient;
};
static bool
port_accept_step(struct port_purchase_accept_tape *tape,
    enum port_purchase_accept_event event, struct yt_error *error)
{
	size_t call = tape->calls++;

	if (call < YT_ARRAY_LEN(tape->events))
		tape->events[call] = event;
	if (call != tape->fail_at)
		return true;
	if (error != NULL)
		error->status = YT_IO_ERROR;
	return false;
}
static bool
port_accept_present(void *context, const uint8_t *text, size_t length,
    enum yt_port_purchase_accept_output_kind kind, struct yt_error *error)
{
	struct port_purchase_accept_tape *tape = context;
	size_t row = tape->row_count;

	if (row >= YT_ARRAY_LEN(tape->rows) || length > sizeof(tape->rows[0])
	    || !port_accept_step(tape, PORT_ACCEPT_PRESENT, error))
		return false;
	if (length != 0U)
		memcpy(tape->rows[row], text, length);
	tape->row_lengths[row] = length;
	tape->kinds[row] = kind;
	tape->row_count++;
	return true;
}
static bool
port_accept_read_port(void *context, int logical, struct yt_port *port,
    struct yt_error *error)
{
	struct port_purchase_accept_tape *tape = context;

	if (logical != 3
	    || !port_accept_step(tape, PORT_ACCEPT_READ_PORT, error))
		return false;
	*port = tape->port;
	return true;
}
static bool
port_accept_read_player(void *context, int record, struct yt_player *player,
    struct yt_error *error)
{
	struct port_purchase_accept_tape *tape = context;

	if (record != 7
	    || !port_accept_step(tape, PORT_ACCEPT_READ_PLAYER, error))
		return false;
	*player = tape->seller;
	return true;
}
static bool
port_accept_write_player(void *context, int record,
    struct yt_player *player, struct yt_error *error)
{
	struct port_purchase_accept_tape *tape = context;

	if (!port_accept_step(tape, PORT_ACCEPT_WRITE_PLAYER, error))
		return false;
	if (record == 7)
		tape->written_seller = *player;
	else if (record == 2)
		tape->written_buyer = *player;
	else
		return false;
	return true;
}
static bool
port_accept_radio(void *context, const uint8_t *text, size_t length,
    float sender, float recipient, struct yt_error *error)
{
	struct port_purchase_accept_tape *tape = context;

	if (length > sizeof(tape->radio)
	    || !port_accept_step(tape, PORT_ACCEPT_RADIO, error))
		return false;
	memcpy(tape->radio, text, length);
	tape->radio_length = length;
	tape->sender = sender;
	tape->recipient = recipient;
	return true;
}
static bool
port_accept_rename(void *context, int logical, const uint8_t *cached,
    size_t cached_length, struct yt_port *port, struct yt_error *error)
{
	static const uint8_t expected[] = {'P', 0, 'N'};

	(void)port;
	return logical == 3 && cached_length == sizeof(expected)
	    && memcmp(cached, expected, sizeof(expected)) == 0
	    && port_accept_step(context, PORT_ACCEPT_RENAME, error);
}
static bool
port_accept_write_port(void *context, int logical, struct yt_port *port,
    struct yt_error *error)
{
	struct port_purchase_accept_tape *tape = context;

	if (logical != 3
	    || !port_accept_step(tape, PORT_ACCEPT_WRITE_PORT, error))
		return false;
	tape->written_port = *port;
	return true;
}
static bool
port_accept_hydrate(void *context, int record, struct yt_player *player,
    struct yt_error *error)
{
	struct port_purchase_accept_tape *tape = context;

	if (record != 2 || !port_accept_step(tape, PORT_ACCEPT_HYDRATE, error))
		return false;
	*player = tape->buyer;
	return true;
}
static const struct yt_port_purchase_accept_ops port_accept_ops = {
	port_accept_present, port_accept_read_port, port_accept_read_player,
	port_accept_write_player, port_accept_radio, port_accept_rename,
	port_accept_write_port, port_accept_hydrate,
};
static void
port_accept_fixture(struct port_purchase_accept_tape *tape,
    struct yt_port_purchase_accept_state *state)
{
	static const uint8_t trader[] = {'T', 0, 'R'};
	static const uint8_t old_name[] = {'P', 0, 'N'};
	static const uint8_t owner[] = {'O', 0, 'W'};
	static const uint8_t first[] = {'F', 0, 'I'};

	memset(tape, 0, sizeof(*tape));
	memset(state, 0, sizeof(*state));
	tape->fail_at = (size_t)-1;
	memset(tape->port.record.bytes, 0xa5, YT_RECORD_SIZE);
	tape->port.treasury = 4.0f;
	(void)yt_record_set_number(&tape->port.record, YT_F89, 4.0f);
	tape->seller.credits = 10.0f;
	tape->seller.ports_owned = 3.0f;
	memset(tape->seller.record.bytes, 0x5a, YT_RECORD_SIZE);
	(void)yt_record_set_number(&tape->seller.record, YT_F81, 10.0f);
	(void)yt_record_set_number(&tape->seller.record, YT_F117, 3.0f);
	tape->buyer.credits = 20.0f;
	tape->buyer.ports_owned = 1.0f;
	memset(tape->buyer.record.bytes, 0xc3, YT_RECORD_SIZE);
	(void)yt_record_set_number(&tape->buyer.record, YT_F81, 20.0f);
	(void)yt_record_set_number(&tape->buyer.record, YT_F117, 1.0f);
	*state = (struct yt_port_purchase_accept_state){
		.current_player_record = 2, .logical_port = 3,
		.relative_port = 3.0f, .old_owner = 7.0f, .price = 2.0,
		.cached_buyer_sector = 42.0f,
		.cached_trader = trader, .cached_trader_length = sizeof(trader),
		.old_name = old_name, .old_name_length = sizeof(old_name),
		.owner_name = owner, .owner_name_length = sizeof(owner),
		.first_name = first, .first_name_length = sizeof(first),
	};
}
static bool
check_port_purchase_accept_transaction(void)
{
	static const enum port_purchase_accept_event expected[] = {
		PORT_ACCEPT_PRESENT, PORT_ACCEPT_PRESENT, PORT_ACCEPT_READ_PORT,
		PORT_ACCEPT_PRESENT, PORT_ACCEPT_PRESENT, PORT_ACCEPT_READ_PLAYER,
		PORT_ACCEPT_WRITE_PLAYER, PORT_ACCEPT_RADIO, PORT_ACCEPT_READ_PORT,
		PORT_ACCEPT_RENAME, PORT_ACCEPT_READ_PORT, PORT_ACCEPT_WRITE_PORT,
		PORT_ACCEPT_HYDRATE, PORT_ACCEPT_WRITE_PLAYER,
		PORT_ACCEPT_PRESENT, PORT_ACCEPT_PRESENT,
	};
	static const uint8_t transfer[] =
	    {'C','r','e','d','i','t','s',' ','t','r','a','n','s','f','e','r','r',
	     'e','d',' ','t','o',' ','O',0,'W','\'', 's',' ','a','c','c','o','u',
	     'n','t','!'};
	static const uint8_t radio[] =
	    {'T',0,'R',' ','b','o','u','g','h','t',' ','y','o','u','r',' ','p',
	     'o','r','t',' ','"','P',0,'N','"',' ','i','n',' ','4','2',' ','f',
	     'o','r',' ','2',' ','c','r','e','d','i','t','s'};
	struct port_purchase_accept_tape tape;
	struct yt_port_purchase_accept_state state;
	struct yt_record expected_port;
	struct yt_error error;
	size_t failure;

	port_accept_fixture(&tape, &state);
	expected_port = tape.port.record;
	(void)yt_record_set_number(&expected_port, YT_F89, 0.0f);
	(void)yt_record_set_number(&expected_port, YT_F97, 2.0f);
	if (!yt_port_purchase_accept_run(&state, &port_accept_ops, &tape, NULL)
	    || !state.complete || !state.sold_presented || !state.seller_written
	    || !state.radio_written || !state.rename_called
	    || !state.title_written || !state.buyer_written
	    || !state.success_presented || state.seller_record != 7
	    || tape.calls != YT_ARRAY_LEN(expected)
	    || memcmp(tape.events, expected, sizeof(expected)) != 0
	    || tape.written_seller.credits != 16.0f
	    || tape.written_seller.ports_owned != 2.0f
	    || tape.written_buyer.credits != 18.0f
	    || tape.written_buyer.ports_owned != 2.0f
	    || memcmp(tape.written_port.record.bytes, expected_port.bytes,
	    YT_RECORD_SIZE) != 0
	    || tape.row_lengths[3] != sizeof(transfer)
	    || memcmp(tape.rows[3], transfer, sizeof(transfer)) != 0
	    || tape.radio_length != sizeof(radio)
	    || memcmp(tape.radio, radio, sizeof(radio)) != 0
	    || tape.sender != -2.0f || tape.recipient != 7.0f)
		return false;
	for (failure = 0U; failure < YT_ARRAY_LEN(expected); ++failure) {
		port_accept_fixture(&tape, &state);
		tape.fail_at = failure;
		yt_error_clear(&error);
		if (yt_port_purchase_accept_run(&state, &port_accept_ops, &tape,
		    &error) || error.status != YT_IO_ERROR || state.complete
		    || tape.calls != failure + 1U
		    || memcmp(tape.events, expected,
		    tape.calls * sizeof(expected[0])) != 0)
			return false;
	}
	port_accept_fixture(&tape, &state);
	state.old_owner = 0.0f;
	state.relative_port = 1.0f;
	if (!yt_port_purchase_accept_run(&state, &port_accept_ops, &tape, NULL)
	    || tape.calls != 9U || state.seller_read || state.radio_written
	    || state.rename_called || !state.title_written || !state.buyer_written)
		return false;
	return !yt_port_purchase_accept_run(NULL, &port_accept_ops, &tape, NULL)
	    && !yt_port_purchase_accept_run(&state, NULL, &tape, NULL);
}

enum port_purchase_event {
	PORT_PURCHASE_HYDRATE = 1,
	PORT_PURCHASE_READ_SECTOR,
	PORT_PURCHASE_REPORT,
	PORT_PURCHASE_OWNER,
	PORT_PURCHASE_PRESENT,
	PORT_PURCHASE_CONFIRM,
	PORT_PURCHASE_ACCEPT,
};
struct port_purchase_tape {
	enum port_purchase_event events[16];
	size_t calls;
	size_t fail_at;
	struct yt_player buyer;
	struct yt_sector sector;
	float early_owner;
	float terminal_owner;
	float production[3];
	float terminal_name_length;
	bool answer;
	bool expected_earth;
	size_t expected_name_length;
	uint8_t rows[8][256];
	size_t row_lengths[8];
	enum yt_port_purchase_output_kind kinds[8];
	size_t row_count;
	struct yt_port owner_port;
	struct yt_port_purchase_accept_state accepted;
};
static bool
port_purchase_step(struct port_purchase_tape *tape,
    enum port_purchase_event event, struct yt_error *error)
{
	size_t call = tape->calls++;

	if (call < YT_ARRAY_LEN(tape->events))
		tape->events[call] = event;
	if (call != tape->fail_at)
		return true;
	if (error != NULL)
		error->status = YT_IO_ERROR;
	return false;
}
static bool
port_purchase_hydrate_test(void *context, int record,
    struct yt_player *player, struct yt_error *error)
{
	struct port_purchase_tape *tape = context;

	if (record != 2
	    || !port_purchase_step(tape, PORT_PURCHASE_HYDRATE, error))
		return false;
	*player = tape->buyer;
	return true;
}
static bool
port_purchase_read_sector_test(void *context, int sector_number,
    struct yt_sector *sector, struct yt_error *error)
{
	struct port_purchase_tape *tape = context;

	if (sector_number != 42
	    || !port_purchase_step(tape, PORT_PURCHASE_READ_SECTOR, error))
		return false;
	*sector = tape->sector;
	return true;
}
static bool
port_purchase_report_test(void *context, int logical, bool earth,
    struct yt_port *early, struct yt_port *terminal, float production[3],
    struct yt_error *error)
{
	static const uint8_t terminal_name[] = {'P', 0, 'N', 'Q'};
	struct port_purchase_tape *tape = context;

	if (logical != (tape->expected_earth ? 1 : 3)
	    || earth != tape->expected_earth
	    || !port_purchase_step(tape, PORT_PURCHASE_REPORT, error))
		return false;
	memset(early, 0x5a, sizeof(*early));
	memset(terminal, 0xa5, sizeof(*terminal));
	early->owner = tape->early_owner;
	terminal->owner = tape->terminal_owner;
	terminal->name_length = tape->terminal_name_length;
	memcpy(terminal->record.bytes, terminal_name, sizeof(terminal_name));
	memcpy(production, tape->production, sizeof(tape->production));
	return true;
}
static bool
port_purchase_owner_test(void *context, const struct yt_port *port,
    uint8_t *name, size_t capacity, size_t *length, struct yt_error *error)
{
	static const uint8_t owner[] = {'O', 0, 'W'};
	struct port_purchase_tape *tape = context;

	if (capacity < sizeof(owner) || length == NULL || port->owner != 7.0f
	    || port->record.bytes[0] != 'P'
	    || !port_purchase_step(tape, PORT_PURCHASE_OWNER, error))
		return false;
	tape->owner_port = *port;
	memcpy(name, owner, sizeof(owner));
	*length = sizeof(owner);
	return true;
}
static bool
port_purchase_present_test(void *context, const uint8_t *text,
    size_t length, enum yt_port_purchase_output_kind kind,
    struct yt_error *error)
{
	struct port_purchase_tape *tape = context;
	size_t row = tape->row_count;

	if (row >= YT_ARRAY_LEN(tape->rows) || length > sizeof(tape->rows[0])
	    || !port_purchase_step(tape, PORT_PURCHASE_PRESENT, error))
		return false;
	if (length != 0U)
		memcpy(tape->rows[row], text, length);
	tape->row_lengths[row] = length;
	tape->kinds[row] = kind;
	tape->row_count++;
	return true;
}
static bool
port_purchase_confirm_test(void *context, const uint8_t *prompt,
    size_t length, bool *accepted, struct yt_error *error)
{
	static const uint8_t expected[] = "Do you wish to buy it? [y/N]";
	struct port_purchase_tape *tape = context;

	if (accepted == NULL || length != sizeof(expected) - 1U
	    || memcmp(prompt, expected, sizeof(expected) - 1U) != 0
	    || !port_purchase_step(tape, PORT_PURCHASE_CONFIRM, error))
		return false;
	*accepted = tape->answer;
	return true;
}
static bool
port_purchase_accept_test(void *context,
    struct yt_port_purchase_accept_state *state, struct yt_error *error)
{
	static const uint8_t trader[] = {'T', 0, 'R'};
	static const uint8_t ordinary_name[] = {'P', 0, 'N', 'Q'};
	static const uint8_t owner[] = {'O', 0, 'W'};
	static const uint8_t first[] = {'F', 0, 'I'};
	struct port_purchase_tape *tape = context;
	const uint8_t *expected_name = tape->expected_earth
	    ? (const uint8_t *)"Earth" : ordinary_name;

	if (state->current_player_record != 2
	    || state->logical_port != (tape->expected_earth ? 1 : 3)
	    || state->relative_port != (tape->expected_earth ? 1.0f : 3.0f)
	    || state->old_owner != tape->early_owner
	    || state->price != (tape->expected_earth ? 1000000000.0 : 7.0)
	    || state->cached_buyer_sector != 42.0f
	    || state->cached_trader_length != sizeof(trader)
	    || memcmp(state->cached_trader, trader, sizeof(trader)) != 0
	    || state->old_name_length != tape->expected_name_length
	    || memcmp(state->old_name, expected_name,
	    tape->expected_name_length) != 0
	    || state->owner_name_length != (tape->early_owner == 0.0f
	    ? 0U : sizeof(owner))
	    || (state->owner_name_length != 0U
	    && memcmp(state->owner_name, owner, sizeof(owner)) != 0)
	    || state->first_name_length != sizeof(first)
	    || memcmp(state->first_name, first, sizeof(first)) != 0
	    || !port_purchase_step(tape, PORT_PURCHASE_ACCEPT, error))
		return false;
	tape->accepted = *state;
	state->complete = true;
	return true;
}
static const struct yt_port_purchase_ops port_purchase_test_ops = {
	port_purchase_hydrate_test,
	port_purchase_read_sector_test,
	port_purchase_report_test,
	port_purchase_owner_test,
	port_purchase_present_test,
	port_purchase_confirm_test,
	port_purchase_accept_test,
};
static void
port_purchase_fixture(struct port_purchase_tape *tape,
    struct yt_port_purchase_state *state)
{
	static const uint8_t trader[] = {'T', 0, 'R'};
	static const uint8_t first[] = {'F', 0, 'I'};

	memset(tape, 0, sizeof(*tape));
	memset(state, 0, sizeof(*state));
	tape->fail_at = SIZE_MAX;
	tape->buyer.credits = 100.0f;
	tape->buyer.sector = 42.0f;
	tape->buyer.name_length = (float)sizeof(trader);
	memcpy(tape->buyer.record.bytes, trader, sizeof(trader));
	tape->sector.port = 3.0f;
	(void)yt_record_set_number(&tape->sector.record, YT_F65, 3.0f);
	tape->early_owner = 7.0f;
	tape->terminal_owner = 99.0f;
	tape->production[0] = 10.0f;
	tape->production[1] = 20.0f;
	tape->production[2] = 30.0f;
	tape->terminal_name_length = 3.6f;
	tape->answer = true;
	tape->expected_name_length = 3U;
	*state = (struct yt_port_purchase_state){
		.current_player_record = 2,
		.port_offset = 100.0f,
		.conversion_mode = 4U,
		.first_name = first,
		.first_name_length = sizeof(first),
	};
}
static bool
check_port_purchase_transaction(void)
{
	static const enum port_purchase_event accepted_events[] = {
		PORT_PURCHASE_HYDRATE, PORT_PURCHASE_READ_SECTOR,
		PORT_PURCHASE_REPORT, PORT_PURCHASE_PRESENT,
		PORT_PURCHASE_OWNER, PORT_PURCHASE_PRESENT,
		PORT_PURCHASE_PRESENT, PORT_PURCHASE_PRESENT,
		PORT_PURCHASE_CONFIRM, PORT_PURCHASE_ACCEPT,
	};
	static const uint8_t price_row[] =
	    "This port is for sale for 7 credits. You have 100 credits.";
	static const uint8_t offer_row[] = {
		'Y','o','u',' ','m','a','y',' ','b','u','y',' ','i','t',' ',
		'f','r','o','m',' ','O',0,'W',' ','i','f',' ','y','o','u',' ',
		'w','i','s','h','.'
	};
	static const uint8_t already[] = {
		'Y','o','u',' ','a','l','r','e','a','d','y',' ','O','W','N',' ',
		't','h','i','s',' ','p','o','r','t',' ','F',0,'I','!'
	};
	static const uint8_t unaffordable[] =
	    "Come back when you can afford it!";
	static const uint8_t declined[] = "What a shame.. it's a nice port!";
	struct port_purchase_tape tape;
	struct yt_port_purchase_state state;
	struct yt_error error;
	size_t failure;

	port_purchase_fixture(&tape, &state);
	if (!yt_port_purchase_run(&state, &port_purchase_test_ops, &tape, NULL)
	    || !state.complete || state.route != YT_PORT_PURCHASE_ACCEPTED_ROUTE
	    || !state.buyer_hydrated || !state.sector_read
	    || !state.report_complete || !state.owner_displayed
	    || !state.confirmation_read || !state.accepted_called
	    || state.old_owner != 7.0f || state.terminal_port.owner != 99.0f
	    || state.price != 7.0 || state.old_name_length != 3U
	    || tape.owner_port.owner != 7.0f || tape.calls != 10U
	    || memcmp(tape.events, accepted_events,
	    sizeof(accepted_events)) != 0
	    || tape.row_count != 4U
	    || tape.kinds[0] != YT_PORT_PURCHASE_PRICE
	    || tape.row_lengths[0] != sizeof(price_row) - 1U
	    || memcmp(tape.rows[0], price_row, sizeof(price_row) - 1U) != 0
	    || tape.kinds[1] != YT_PORT_PURCHASE_OFFER_LEADING_BLANK
	    || tape.row_lengths[1] != 0U
	    || tape.kinds[2] != YT_PORT_PURCHASE_OFFER_ROW
	    || tape.row_lengths[2] != sizeof(offer_row)
	    || memcmp(tape.rows[2], offer_row, sizeof(offer_row)) != 0
	    || tape.kinds[3] != YT_PORT_PURCHASE_OFFER_TRAILING_BLANK
	    || tape.row_lengths[3] != 0U)
		return false;
	for (failure = 0U; failure < YT_ARRAY_LEN(accepted_events); ++failure) {
		port_purchase_fixture(&tape, &state);
		tape.fail_at = failure;
		yt_error_clear(&error);
		if (yt_port_purchase_run(&state, &port_purchase_test_ops, &tape,
		    &error) || error.status != YT_IO_ERROR || state.complete
		    || tape.calls != failure + 1U
		    || memcmp(tape.events, accepted_events,
		    tape.calls * sizeof(accepted_events[0])) != 0)
			return false;
	}

	/* Raw exponent zero is the only no-port gate, including dirty
	 * mantissa bytes. */
	port_purchase_fixture(&tape, &state);
	memcpy(tape.sector.record.bytes + YT_F65, "\x01\x02\x03\0", 4U);
	if (!yt_port_purchase_run(&state, &port_purchase_test_ops, &tape, NULL)
	    || state.route != YT_PORT_PURCHASE_NO_PORT_ROUTE
	    || tape.calls != 3U || tape.row_count != 1U
	    || tape.kinds[0] != YT_PORT_PURCHASE_NO_PORT)
		return false;

	port_purchase_fixture(&tape, &state);
	tape.early_owner = 2.0f;
	if (!yt_port_purchase_run(&state, &port_purchase_test_ops, &tape, NULL)
	    || state.route != YT_PORT_PURCHASE_ALREADY_OWNER_ROUTE
	    || tape.calls != 4U || tape.row_count != 1U
	    || tape.kinds[0] != YT_PORT_PURCHASE_ALREADY_OWNER
	    || tape.row_lengths[0] != sizeof(already)
	    || memcmp(tape.rows[0], already, sizeof(already)) != 0)
		return false;

	port_purchase_fixture(&tape, &state);
	tape.buyer.credits = 1.0f;
	if (!yt_port_purchase_run(&state, &port_purchase_test_ops, &tape, NULL)
	    || state.route != YT_PORT_PURCHASE_UNAFFORDABLE_ROUTE
	    || tape.calls != 5U || tape.row_count != 2U
	    || tape.kinds[1] != YT_PORT_PURCHASE_UNAFFORDABLE
	    || tape.row_lengths[1] != sizeof(unaffordable) - 1U
	    || memcmp(tape.rows[1], unaffordable,
	    sizeof(unaffordable) - 1U) != 0)
		return false;

	port_purchase_fixture(&tape, &state);
	tape.answer = false;
	if (!yt_port_purchase_run(&state, &port_purchase_test_ops, &tape, NULL)
	    || state.route != YT_PORT_PURCHASE_DECLINED_ROUTE
	    || tape.calls != 10U || tape.row_count != 5U
	    || tape.events[9] != PORT_PURCHASE_PRESENT
	    || tape.kinds[4] != YT_PORT_PURCHASE_DECLINED
	    || tape.row_lengths[4] != sizeof(declined) - 1U
	    || memcmp(tape.rows[4], declined, sizeof(declined) - 1U) != 0
	    || state.accepted_called)
		return false;

	port_purchase_fixture(&tape, &state);
	tape.expected_earth = true;
	tape.expected_name_length = 5U;
	tape.early_owner = 0.0f;
	tape.buyer.credits = 2000000000.0f;
	tape.sector.port = 1.0f;
	(void)yt_record_set_number(&tape.sector.record, YT_F65, 1.0f);
	if (!yt_port_purchase_run(&state, &port_purchase_test_ops, &tape, NULL)
	    || !state.earth || state.price != 1000000000.0
	    || state.route != YT_PORT_PURCHASE_ACCEPTED_ROUTE
	    || tape.calls != 6U || tape.events[3] != PORT_PURCHASE_PRESENT
	    || tape.events[4] != PORT_PURCHASE_CONFIRM
	    || tape.events[5] != PORT_PURCHASE_ACCEPT || state.owner_displayed)
		return false;

	/* Non-floor CINT retains the fourth byte of the terminal report name. */
	port_purchase_fixture(&tape, &state);
	state.conversion_mode = 0U;
	tape.expected_name_length = 4U;
	if (!yt_port_purchase_run(&state, &port_purchase_test_ops, &tape, NULL)
	    || state.old_name_length != 4U || state.old_name[3] != 'Q')
		return false;

	port_purchase_fixture(&tape, &state);
	return !yt_port_purchase_run(NULL, &port_purchase_test_ops, &tape, NULL)
	    && !yt_port_purchase_run(&state, NULL, &tape, NULL);
}

struct port_purchase_cycle_tape {
	int events[2];
	size_t calls;
	size_t fail_at;
};
static bool
port_purchase_cycle_step(void *context, int event, struct yt_error *error)
{
	struct port_purchase_cycle_tape *tape = context;
	size_t call = tape->calls++;

	if (call < YT_ARRAY_LEN(tape->events))
		tape->events[call] = event;
	if (call != tape->fail_at)
		return true;
	if (error != NULL)
		error->status = YT_IO_ERROR;
	return false;
}
static bool
port_purchase_cycle_purchase_test(void *context, struct yt_error *error)
{
	return port_purchase_cycle_step(context, 1, error);
}
static bool
port_purchase_cycle_scanner_test(void *context, struct yt_error *error)
{
	return port_purchase_cycle_step(context, 2, error);
}
static bool
check_port_purchase_cycle_transaction(void)
{
	static const struct yt_port_purchase_cycle_ops ops = {
		port_purchase_cycle_purchase_test,
		port_purchase_cycle_scanner_test,
	};
	struct port_purchase_cycle_tape tape;
	struct yt_port_purchase_cycle_state state;
	struct yt_error error;
	size_t failure;

	memset(&tape, 0, sizeof(tape));
	tape.fail_at = SIZE_MAX;
	if (!yt_port_purchase_cycle_run(&state, &ops, &tape, NULL)
	    || !state.complete || !state.purchase_complete
	    || !state.scanner_complete || tape.calls != 2U
	    || tape.events[0] != 1 || tape.events[1] != 2)
		return false;
	for (failure = 0U; failure < 2U; ++failure) {
		memset(&tape, 0, sizeof(tape));
		tape.fail_at = failure;
		yt_error_clear(&error);
		if (yt_port_purchase_cycle_run(&state, &ops, &tape, &error)
		    || error.status != YT_IO_ERROR || state.complete
		    || tape.calls != failure + 1U
		    || state.purchase_complete != (failure != 0U)
		    || state.scanner_complete)
			return false;
	}
	return !yt_port_purchase_cycle_run(NULL, &ops, &tape, NULL)
	    && !yt_port_purchase_cycle_run(&state, NULL, &tape, NULL);
}

enum projectile_command_event {
	PROJECTILE_COMMAND_PRESENT = 1,
	PROJECTILE_COMMAND_HYDRATE,
	PROJECTILE_COMMAND_INPUT,
	PROJECTILE_COMMAND_FINALIZE,
	PROJECTILE_COMMAND_WRITE,
	PROJECTILE_COMMAND_FLUSH,
	PROJECTILE_COMMAND_RESOLVE,
	PROJECTILE_COMMAND_COUNTER,
	PROJECTILE_COMMAND_XANNOR,
	PROJECTILE_COMMAND_FATAL,
};

struct projectile_command_tape {
	enum projectile_command_event events[40];
	size_t calls;
	size_t fail_at;
	struct yt_player hydrations[4];
	size_t hydration_index;
	const char *responses[4];
	size_t response_index;
	struct yt_player finalizer_player;
	struct yt_player written_player;
	uint8_t rows[12][192];
	size_t row_lengths[12];
	enum yt_projectile_command_output_kind kinds[12];
	size_t row_count;
	bool finalizer_terminal;
	bool destroy_after_xannor;
	bool *destroyed;
	float resolved_origin;
	float resolved_target;
	float resolved_amount;
	bool resolved_plasma;
};

static bool
projectile_command_step(struct projectile_command_tape *tape,
    enum projectile_command_event event, struct yt_error *error)
{
	size_t call = tape->calls++;

	if (call < YT_ARRAY_LEN(tape->events))
		tape->events[call] = event;
	if (call != tape->fail_at)
		return true;
	if (error != NULL) {
		error->status = YT_IO_ERROR;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    "projectile command injected failure");
	}
	return false;
}

static bool
projectile_command_test_hydrate(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct projectile_command_tape *tape = context;

	if (player_record != 2
	    || tape->hydration_index >= YT_ARRAY_LEN(tape->hydrations)
	    || !projectile_command_step(tape, PROJECTILE_COMMAND_HYDRATE,
	    error))
		return false;
	*player = tape->hydrations[tape->hydration_index++];
	return true;
}

static bool
projectile_command_test_present(void *context, const uint8_t *text,
    size_t length, enum yt_projectile_command_output_kind kind,
    struct yt_error *error)
{
	struct projectile_command_tape *tape = context;
	size_t row = tape->row_count;

	if (row >= YT_ARRAY_LEN(tape->rows) || length > sizeof(tape->rows[0])
	    || !projectile_command_step(tape, PROJECTILE_COMMAND_PRESENT,
	    error))
		return false;
	if (length != 0U)
		memcpy(tape->rows[row], text, length);
	tape->row_lengths[row] = length;
	tape->kinds[row] = kind;
	tape->row_count++;
	return true;
}

static bool
projectile_command_test_input(void *context, char *response,
    size_t capacity, struct yt_error *error)
{
	struct projectile_command_tape *tape = context;
	const char *source;
	size_t length;

	if (tape->response_index >= YT_ARRAY_LEN(tape->responses)
	    || tape->responses[tape->response_index] == NULL)
		return false;
	source = tape->responses[tape->response_index++];
	length = strlen(source);
	if (length + 1U > capacity || !projectile_command_step(tape,
	    PROJECTILE_COMMAND_INPUT, error))
		return false;
	memcpy(response, source, length + 1U);
	return true;
}

static bool
projectile_command_test_finalize(void *context, struct yt_player *player,
    struct yt_error *error)
{
	struct projectile_command_tape *tape = context;

	if (!projectile_command_step(tape, PROJECTILE_COMMAND_FINALIZE, error))
		return false;
	if (tape->finalizer_terminal)
		return false;
	*player = tape->finalizer_player;
	return true;
}

static bool
projectile_command_test_write(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct projectile_command_tape *tape = context;

	if (player_record != 2 || !projectile_command_step(tape,
	    PROJECTILE_COMMAND_WRITE, error))
		return false;
	tape->written_player = *player;
	return true;
}

static bool
projectile_command_test_flush(void *context, struct yt_error *error)
{
	return projectile_command_step(context, PROJECTILE_COMMAND_FLUSH,
	    error);
}

static bool
projectile_command_test_resolve(void *context, float *origin, float target,
    float amount, bool plasma, int *counterattack, int *xannor_provoker,
    struct yt_error *error)
{
	struct projectile_command_tape *tape = context;

	if (!projectile_command_step(tape, PROJECTILE_COMMAND_RESOLVE, error))
		return false;
	tape->resolved_origin = *origin;
	tape->resolved_target = target;
	tape->resolved_amount = amount;
	tape->resolved_plasma = plasma;
	*origin = 13.0f;
	*counterattack = 3;
	*xannor_provoker = 4;
	return true;
}

static bool
projectile_command_test_counter(void *context, int *counterattack,
    int *xannor_provoker, struct yt_error *error)
{
	if (*counterattack != 3 || *xannor_provoker != 4)
		return false;
	return projectile_command_step(context, PROJECTILE_COMMAND_COUNTER,
	    error);
}

static bool
projectile_command_test_xannor(void *context, int *xannor_provoker,
    struct yt_error *error)
{
	struct projectile_command_tape *tape = context;

	if (*xannor_provoker != 4 || !projectile_command_step(tape,
	    PROJECTILE_COMMAND_XANNOR, error))
		return false;
	if (tape->destroy_after_xannor)
		*tape->destroyed = true;
	return true;
}

static bool
projectile_command_test_fatal(void *context, struct yt_error *error)
{
	return projectile_command_step(context, PROJECTILE_COMMAND_FATAL,
	    error);
}

static const struct yt_projectile_command_ops projectile_command_ops = {
	projectile_command_test_hydrate,
	projectile_command_test_present,
	projectile_command_test_input,
	projectile_command_test_finalize,
	projectile_command_test_write,
	projectile_command_test_flush,
	projectile_command_test_resolve,
	projectile_command_test_counter,
	projectile_command_test_xannor,
	projectile_command_test_fatal,
};

static void
projectile_command_fixture(struct projectile_command_tape *tape,
    struct yt_projectile_command_state *state, bool *destroyed)
{
	size_t index;

	memset(tape, 0, sizeof(*tape));
	memset(state, 0, sizeof(*state));
	tape->fail_at = (size_t)-1;
	for (index = 0U; index < YT_ARRAY_LEN(tape->hydrations); ++index) {
		tape->hydrations[index].turns = 10.0f;
		tape->hydrations[index].missiles = 5.0f;
		tape->hydrations[index].plasma = 6.0f;
	}
	memset(tape->finalizer_player.record.bytes, 0xa5,
	    sizeof(tape->finalizer_player.record.bytes));
	tape->finalizer_player.sector = 12.0f;
	tape->finalizer_player.missiles = 9.0f;
	tape->finalizer_player.plasma = 8.0f;
	(void)yt_record_set_number(&tape->finalizer_player.record, YT_F57,
	    12.0f);
	(void)yt_record_set_number(&tape->finalizer_player.record, YT_F97,
	    9.0f);
	(void)yt_record_set_number(&tape->finalizer_player.record, YT_F113,
	    8.0f);
	tape->responses[0] = "42";
	tape->responses[1] = "2.9";
	*destroyed = true;
	tape->destroyed = destroyed;
	state->current_player_record = 2;
	state->maximum_sector = 2004.0f;
	state->displayed = 9.0f;
	state->destroyed = destroyed;
}

static bool
check_projectile_command_transaction(void)
{
	static const enum projectile_command_event expected[] = {
		PROJECTILE_COMMAND_PRESENT,
		PROJECTILE_COMMAND_HYDRATE,
		PROJECTILE_COMMAND_HYDRATE,
		PROJECTILE_COMMAND_PRESENT,
		PROJECTILE_COMMAND_INPUT,
		PROJECTILE_COMMAND_PRESENT,
		PROJECTILE_COMMAND_INPUT,
		PROJECTILE_COMMAND_PRESENT,
		PROJECTILE_COMMAND_FINALIZE,
		PROJECTILE_COMMAND_WRITE,
		PROJECTILE_COMMAND_FLUSH,
		PROJECTILE_COMMAND_RESOLVE,
		PROJECTILE_COMMAND_COUNTER,
		PROJECTILE_COMMAND_XANNOR,
	};
	static const uint8_t target_prompt[] =
	    "You have 9. Send your cruise missile to what sector? "
	    "[ 1 to 2004 ] ?";
	struct projectile_command_tape tape;
	struct yt_projectile_command_state state;
	struct yt_record expected_record;
	struct yt_error error;
	bool destroyed;
	size_t failure;

	projectile_command_fixture(&tape, &state, &destroyed);
	expected_record = tape.finalizer_player.record;
	(void)yt_record_set_number(&expected_record, YT_F97, 7.0f);
	if (!yt_projectile_command_run(&state, &projectile_command_ops, &tape,
	    NULL) || !state.complete
	    || state.route != YT_PROJECTILE_COMMAND_RETURNED
	    || state.attempts != 1U || state.hydrations != 2U
	    || state.available != 5.0f || state.target != 42.0f
	    || state.amount != 2.0f || !state.target_stored
	    || !state.amount_stored || !state.finalizer_called
	    || !state.player_written || !state.player_flushed
	    || !state.destruction_cleared || !state.resolver_called
	    || !state.counterlaunch_called || !state.xannor_called
	    || state.fatal_called || destroyed || state.origin != 13.0f
	    || tape.calls != YT_ARRAY_LEN(expected)
	    || memcmp(tape.events, expected, sizeof(expected)) != 0
	    || tape.row_count != 4U
	    || tape.kinds[0] != YT_PROJECTILE_COMMAND_OPENING_BLANK
	    || tape.kinds[1] != YT_PROJECTILE_COMMAND_TARGET_PROMPT
	    || tape.row_lengths[1] != sizeof(target_prompt) - 1U
	    || memcmp(tape.rows[1], target_prompt,
	    sizeof(target_prompt) - 1U) != 0
	    || tape.kinds[2] != YT_PROJECTILE_COMMAND_QUANTITY_PROMPT
	    || tape.kinds[3] != YT_PROJECTILE_COMMAND_ACCEPTED_BLANK
	    || tape.resolved_origin != 12.0f || tape.resolved_target != 42.0f
	    || tape.resolved_amount != 2.0f || tape.resolved_plasma
	    || memcmp(tape.written_player.record.bytes, expected_record.bytes,
	    YT_RECORD_SIZE) != 0)
		return false;

	for (failure = 0U; failure < YT_ARRAY_LEN(expected); ++failure) {
		projectile_command_fixture(&tape, &state, &destroyed);
		tape.fail_at = failure;
		yt_error_clear(&error);
		if (yt_projectile_command_run(&state, &projectile_command_ops,
		    &tape, &error) || error.status != YT_IO_ERROR
		    || state.complete || tape.calls != failure + 1U
		    || memcmp(tape.events, expected,
		    tape.calls * sizeof(expected[0])) != 0
		    || (failure > 10U
		    && (!state.player_written || !state.player_flushed
		    || !state.destruction_cleared)))
			return false;
	}

	/* Retry retains the displayed snapshot but refreshes the live bound. */
	projectile_command_fixture(&tape, &state, &destroyed);
	tape.responses[0] = "9999";
	tape.responses[1] = "42";
	tape.responses[2] = "4";
	tape.hydrations[3].missiles = 3.0f;
	if (!yt_projectile_command_run(&state, &projectile_command_ops, &tape,
	    NULL) || state.route != YT_PROJECTILE_COMMAND_TOO_MANY
	    || state.attempts != 2U || state.hydrations != 4U
	    || state.available != 3.0f || tape.row_count != 7U
	    || tape.kinds[4] != YT_PROJECTILE_COMMAND_TARGET_PROMPT
	    || memcmp(tape.rows[4], target_prompt,
	    sizeof(target_prompt) - 1U) != 0)
		return false;

	projectile_command_fixture(&tape, &state, &destroyed);
	tape.hydrations[1].turns = 0.0f;
	if (!yt_projectile_command_run(&state, &projectile_command_ops, &tape,
	    NULL) || state.route != YT_PROJECTILE_COMMAND_NO_TURNS
	    || tape.calls != 4U)
		return false;
	projectile_command_fixture(&tape, &state, &destroyed);
	tape.hydrations[1].missiles = 0.0f;
	if (!yt_projectile_command_run(&state, &projectile_command_ops, &tape,
	    NULL) || state.route != YT_PROJECTILE_COMMAND_NO_AMMUNITION
	    || tape.calls != 4U)
		return false;
	projectile_command_fixture(&tape, &state, &destroyed);
	tape.responses[0] = "";
	if (!yt_projectile_command_run(&state, &projectile_command_ops, &tape,
	    NULL) || state.route != YT_PROJECTILE_COMMAND_TARGET_CANCELLED
	    || tape.calls != 5U)
		return false;
	projectile_command_fixture(&tape, &state, &destroyed);
	tape.responses[1] = ".9";
	if (!yt_projectile_command_run(&state, &projectile_command_ops, &tape,
	    NULL) || state.route != YT_PROJECTILE_COMMAND_QUANTITY_CANCELLED
	    || state.amount != 0.0f)
		return false;

	projectile_command_fixture(&tape, &state, &destroyed);
	tape.finalizer_terminal = true;
	yt_error_clear(&error);
	if (!yt_projectile_command_run(&state, &projectile_command_ops, &tape,
	    &error)
	    || state.route != YT_PROJECTILE_COMMAND_FINALIZER_TERMINAL
	    || !state.complete || state.player_written)
		return false;
	projectile_command_fixture(&tape, &state, &destroyed);
	tape.destroy_after_xannor = true;
	if (!yt_projectile_command_run(&state, &projectile_command_ops, &tape,
	    NULL) || state.route != YT_PROJECTILE_COMMAND_FATAL
	    || !state.fatal_called || tape.calls != YT_ARRAY_LEN(expected) + 1U)
		return false;

	projectile_command_fixture(&tape, &state, &destroyed);
	tape.responses[0] = "1E400";
	yt_error_clear(&error);
	if (yt_projectile_command_run(&state, &projectile_command_ops, &tape,
	    &error) || error.status != YT_RANGE || state.target_stored)
		return false;

	projectile_command_fixture(&tape, &state, &destroyed);
	return !yt_projectile_command_run(NULL, &projectile_command_ops, &tape,
	    NULL)
	    && !yt_projectile_command_run(&state, NULL, &tape, NULL);
}

enum drop_mines_event {
	DROP_MINES_READ_PLAYER = 1,
	DROP_MINES_WRITE_PLAYER,
	DROP_MINES_FLUSH,
	DROP_MINES_READ_SECTOR,
	DROP_MINES_WRITE_SECTOR,
	DROP_MINES_PRESENT,
	DROP_MINES_AMOUNT,
	DROP_MINES_SUPPRESS,
	DROP_MINES_SOUND,
};

struct drop_mines_tape {
	enum drop_mines_event events[24];
	size_t calls;
	size_t fail_at;
	struct yt_player player;
	struct yt_player written_player;
	struct yt_sector sector;
	struct yt_sector written_sector;
	int player_read_record;
	int player_write_record;
	int sector_read_record;
	int sector_write_record;
	const char *response;
	uint8_t rows[6][192];
	size_t row_lengths[6];
	enum yt_drop_mines_output_kind kinds[6];
	size_t row_count;
	float selector;
	bool suppressed;
};

static bool
drop_mines_step(struct drop_mines_tape *tape, enum drop_mines_event event,
    struct yt_error *error)
{
	size_t call = tape->calls++;

	if (call < YT_ARRAY_LEN(tape->events))
		tape->events[call] = event;
	if (call != tape->fail_at)
		return true;
	if (error != NULL) {
		error->status = YT_IO_ERROR;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    "Drop Mine injected failure");
	}
	return false;
}

static bool
drop_mines_test_read_player(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct drop_mines_tape *tape = context;

	tape->player_read_record = player_record;
	if (!drop_mines_step(tape, DROP_MINES_READ_PLAYER, error))
		return false;
	*player = tape->player;
	return true;
}

static bool
drop_mines_test_write_player(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct drop_mines_tape *tape = context;

	tape->player_write_record = player_record;
	if (!drop_mines_step(tape, DROP_MINES_WRITE_PLAYER, error))
		return false;
	tape->written_player = *player;
	return true;
}

static bool
drop_mines_test_flush(void *context, struct yt_error *error)
{
	return drop_mines_step(context, DROP_MINES_FLUSH, error);
}

static bool
drop_mines_test_read_sector(void *context, int sector_number,
    struct yt_sector *sector, struct yt_error *error)
{
	struct drop_mines_tape *tape = context;

	tape->sector_read_record = sector_number;
	if (!drop_mines_step(tape, DROP_MINES_READ_SECTOR, error))
		return false;
	*sector = tape->sector;
	return true;
}

static bool
drop_mines_test_write_sector(void *context, int sector_number,
    struct yt_sector *sector, struct yt_error *error)
{
	struct drop_mines_tape *tape = context;

	tape->sector_write_record = sector_number;
	if (!drop_mines_step(tape, DROP_MINES_WRITE_SECTOR, error))
		return false;
	tape->written_sector = *sector;
	return true;
}

static bool
drop_mines_test_present(void *context, const uint8_t *text, size_t length,
    enum yt_drop_mines_output_kind kind, struct yt_error *error)
{
	struct drop_mines_tape *tape = context;
	size_t row = tape->row_count;

	if (row >= YT_ARRAY_LEN(tape->rows) || length > sizeof(tape->rows[0])
	    || !drop_mines_step(tape, DROP_MINES_PRESENT, error))
		return false;
	if (length != 0U)
		memcpy(tape->rows[row], text, length);
	tape->row_lengths[row] = length;
	tape->kinds[row] = kind;
	tape->row_count++;
	return true;
}

static bool
drop_mines_test_amount(void *context, char *response, size_t capacity,
    struct yt_error *error)
{
	struct drop_mines_tape *tape = context;
	size_t length = strlen(tape->response);

	if (length + 1U > capacity
	    || !drop_mines_step(tape, DROP_MINES_AMOUNT, error))
		return false;
	memcpy(response, tape->response, length + 1U);
	return true;
}

static void
drop_mines_test_suppress(void *context)
{
	struct drop_mines_tape *tape = context;

	if (tape->calls < YT_ARRAY_LEN(tape->events))
		tape->events[tape->calls] = DROP_MINES_SUPPRESS;
	tape->calls++;
	tape->suppressed = true;
}

static bool
drop_mines_test_sound(void *context, float selector,
    struct yt_error *error)
{
	struct drop_mines_tape *tape = context;

	tape->selector = selector;
	return drop_mines_step(tape, DROP_MINES_SOUND, error);
}

static const struct yt_drop_mines_ops drop_mines_ops = {
	drop_mines_test_read_player,
	drop_mines_test_write_player,
	drop_mines_test_flush,
	drop_mines_test_read_sector,
	drop_mines_test_write_sector,
	drop_mines_test_present,
	drop_mines_test_amount,
	drop_mines_test_suppress,
	drop_mines_test_sound,
};

static void
drop_mines_fixture(struct drop_mines_tape *tape,
    struct yt_drop_mines_state *state)
{
	size_t index;

	memset(tape, 0, sizeof(*tape));
	memset(state, 0, sizeof(*state));
	tape->fail_at = (size_t)-1;
	tape->response = "1.5";
	for (index = 0U; index < YT_RECORD_SIZE; ++index) {
		tape->player.record.bytes[index] = (uint8_t)(index ^ 0xa5U);
		tape->sector.record.bytes[index] = (uint8_t)(index ^ 0x5aU);
	}
	tape->player.sector = 42.0f;
	tape->player.mines = 5.0f;
	(void)yt_record_set_number(&tape->player.record, YT_F57, 42.0f);
	(void)yt_record_set_number(&tape->player.record, YT_F129, 5.0f);
	tape->sector.mines = -2.0f;
	(void)yt_record_set_number(&tape->sector.record, YT_F129, -2.0f);
	state->current_player_record = 2;
}

static bool
check_drop_mines_transaction(void)
{
	static const enum drop_mines_event accepted_events[] = {
		DROP_MINES_READ_PLAYER,
		DROP_MINES_PRESENT,
		DROP_MINES_PRESENT,
		DROP_MINES_AMOUNT,
		DROP_MINES_SUPPRESS,
		DROP_MINES_WRITE_PLAYER,
		DROP_MINES_FLUSH,
		DROP_MINES_READ_SECTOR,
		DROP_MINES_WRITE_SECTOR,
		DROP_MINES_FLUSH,
		DROP_MINES_PRESENT,
		DROP_MINES_PRESENT,
		DROP_MINES_SOUND,
	};
	static const size_t failable_calls[] = {
		0U, 1U, 2U, 3U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 12U,
	};
	static const enum drop_mines_event repair_events[] = {
		DROP_MINES_READ_PLAYER,
		DROP_MINES_WRITE_PLAYER,
		DROP_MINES_FLUSH,
		DROP_MINES_PRESENT,
	};
	static const uint8_t prompt[] =
	    "You have 5 mines. Drop how many? [0] -=>";
	static const uint8_t success[] = "Sector 42 is now mined!";
	static const uint8_t no_mines[] = "You don't HAVE any!";
	static const uint8_t union_refusal[] =
	    "The Union doesnt like the home 7 sectors mined!";
	struct drop_mines_tape tape;
	struct yt_drop_mines_state state;
	struct yt_record expected_player;
	struct yt_record expected_sector;
	struct yt_error error;
	size_t index;

	drop_mines_fixture(&tape, &state);
	expected_player = tape.player.record;
	expected_sector = tape.sector.record;
	(void)yt_record_set_number(&expected_player, YT_F129, 3.5f);
	(void)yt_record_set_number(&expected_sector, YT_F129, -0.5f);
	if (!yt_drop_mines_run(&state, &drop_mines_ops, &tape, NULL)
	    || !state.complete || state.route != YT_DROP_MINES_ACCEPTED
	    || !state.player_read || !state.amount_stored
	    || !state.suppression_set || !state.player_written
	    || !state.player_flushed || !state.sector_read
	    || !state.sector_written || !state.sector_flushed
	    || state.negative_repair || state.current_sector != 42
	    || state.carried != 5.0f || state.amount != 1.5f
	    || state.player_mines_after != 3.5f
	    || state.sector_mines_before != -2.0f
	    || state.sector_mines_after != -0.5f
	    || tape.calls != YT_ARRAY_LEN(accepted_events)
	    || memcmp(tape.events, accepted_events, sizeof(accepted_events)) != 0
	    || tape.player_read_record != 2 || tape.player_write_record != 2
	    || tape.sector_read_record != 42 || tape.sector_write_record != 42
	    || !tape.suppressed || tape.selector != 4.0f
	    || tape.player.mines != 5.0f || tape.sector.mines != -2.0f
	    || memcmp(tape.written_player.record.bytes, expected_player.bytes,
	    YT_RECORD_SIZE) != 0
	    || memcmp(tape.written_sector.record.bytes, expected_sector.bytes,
	    YT_RECORD_SIZE) != 0
	    || tape.row_count != 4U || tape.row_lengths[0] != 0U
	    || tape.kinds[0] != YT_DROP_MINES_PROMPT_BLANK
	    || tape.row_lengths[1] != sizeof(prompt) - 1U
	    || tape.kinds[1] != YT_DROP_MINES_PROMPT
	    || memcmp(tape.rows[1], prompt, sizeof(prompt) - 1U) != 0
	    || tape.row_lengths[2] != 0U
	    || tape.kinds[2] != YT_DROP_MINES_SUCCESS_BLANK
	    || tape.row_lengths[3] != sizeof(success) - 1U
	    || tape.kinds[3] != YT_DROP_MINES_SUCCESS_ROW
	    || memcmp(tape.rows[3], success, sizeof(success) - 1U) != 0)
		return false;

	for (index = 0U; index < YT_ARRAY_LEN(failable_calls); ++index) {
		drop_mines_fixture(&tape, &state);
		tape.fail_at = failable_calls[index];
		yt_error_clear(&error);
		if (yt_drop_mines_run(&state, &drop_mines_ops, &tape, &error)
		    || error.status != YT_IO_ERROR || state.complete
		    || tape.calls != failable_calls[index] + 1U
		    || memcmp(tape.events, accepted_events,
		    tape.calls * sizeof(accepted_events[0])) != 0
		    || (failable_calls[index] > 4U && !state.suppression_set)
		    || (failable_calls[index] > 6U
		    && (!state.player_written || !state.player_flushed))
		    || (failable_calls[index] > 9U
		    && (!state.sector_written || !state.sector_flushed)))
			return false;
	}

	/* A negative carried value is repaired and flushed, but its scratch
	 * value remains negative and selects the ordinary no-mines row. */
	drop_mines_fixture(&tape, &state);
	tape.player.mines = -2.0f;
	(void)yt_record_set_number(&tape.player.record, YT_F129, -2.0f);
	expected_player = tape.player.record;
	(void)yt_record_set_number(&expected_player, YT_F129, 0.0f);
	if (!yt_drop_mines_run(&state, &drop_mines_ops, &tape, NULL)
	    || state.route != YT_DROP_MINES_NO_MINES || !state.complete
	    || !state.negative_repair || !state.repair_written
	    || !state.repair_flushed || state.carried != -2.0f
	    || tape.calls != 4U || tape.events[0] != DROP_MINES_READ_PLAYER
	    || tape.events[1] != DROP_MINES_WRITE_PLAYER
	    || tape.events[2] != DROP_MINES_FLUSH
	    || tape.events[3] != DROP_MINES_PRESENT
	    || tape.row_count != 1U
	    || tape.kinds[0] != YT_DROP_MINES_NO_MINES_ROW
	    || tape.row_lengths[0] != sizeof(no_mines) - 1U
	    || memcmp(tape.rows[0], no_mines, sizeof(no_mines) - 1U) != 0
	    || memcmp(tape.written_player.record.bytes, expected_player.bytes,
	    YT_RECORD_SIZE) != 0)
		return false;
	for (index = 1U; index < YT_ARRAY_LEN(repair_events); ++index) {
		drop_mines_fixture(&tape, &state);
		tape.player.mines = -2.0f;
		(void)yt_record_set_number(&tape.player.record, YT_F129, -2.0f);
		tape.fail_at = index;
		yt_error_clear(&error);
		if (yt_drop_mines_run(&state, &drop_mines_ops, &tape, &error)
		    || error.status != YT_IO_ERROR || state.complete
		    || state.carried != -2.0f || !state.negative_repair
		    || tape.calls != index + 1U
		    || memcmp(tape.events, repair_events,
		    tape.calls * sizeof(repair_events[0])) != 0
		    || (index > 1U && !state.repair_written)
		    || (index > 2U && !state.repair_flushed))
			return false;
	}

	/* Zero, Union, blank, below-one and above-carried routes do no sector
	 * I/O and never enable self-mine suppression. */
	drop_mines_fixture(&tape, &state);
	tape.player.mines = 0.0f;
	if (!yt_drop_mines_run(&state, &drop_mines_ops, &tape, NULL)
	    || state.route != YT_DROP_MINES_NO_MINES || tape.calls != 2U)
		return false;
	drop_mines_fixture(&tape, &state);
	tape.player.mines = 0.0f;
	tape.fail_at = 1U;
	if (yt_drop_mines_run(&state, &drop_mines_ops, &tape, NULL)
	    || state.complete || tape.calls != 2U)
		return false;
	drop_mines_fixture(&tape, &state);
	tape.player.sector = 7.0f;
	if (!yt_drop_mines_run(&state, &drop_mines_ops, &tape, NULL)
	    || state.route != YT_DROP_MINES_UNION_REFUSAL
	    || tape.row_count != 1U
	    || tape.kinds[0] != YT_DROP_MINES_UNION_ROW
	    || tape.row_lengths[0] != sizeof(union_refusal) - 1U
	    || memcmp(tape.rows[0], union_refusal,
	    sizeof(union_refusal) - 1U) != 0)
		return false;
	drop_mines_fixture(&tape, &state);
	tape.player.sector = 7.0f;
	tape.fail_at = 1U;
	if (yt_drop_mines_run(&state, &drop_mines_ops, &tape, NULL)
	    || state.complete || tape.calls != 2U)
		return false;
	drop_mines_fixture(&tape, &state);
	tape.response = "";
	if (!yt_drop_mines_run(&state, &drop_mines_ops, &tape, NULL)
	    || state.route != YT_DROP_MINES_CANCELLED || state.amount != 0.0f
	    || !state.amount_stored || tape.calls != 4U || tape.suppressed)
		return false;
	drop_mines_fixture(&tape, &state);
	tape.response = ".5";
	if (!yt_drop_mines_run(&state, &drop_mines_ops, &tape, NULL)
	    || state.route != YT_DROP_MINES_CANCELLED || state.amount != 0.5f
	    || tape.calls != 4U || tape.suppressed)
		return false;
	drop_mines_fixture(&tape, &state);
	tape.response = "5.5";
	if (!yt_drop_mines_run(&state, &drop_mines_ops, &tape, NULL)
	    || state.route != YT_DROP_MINES_CANCELLED || state.amount != 5.5f
	    || tape.calls != 4U || tape.suppressed)
		return false;

	drop_mines_fixture(&tape, &state);
	tape.response = "not-a-number";
	yt_error_clear(&error);
	if (yt_drop_mines_run(&state, &drop_mines_ops, &tape, &error)
	    || error.status != YT_RANGE || state.amount_stored
	    || strcmp(error.operation, "drop-mines:VAL") != 0)
		return false;

	drop_mines_fixture(&tape, &state);
	return !yt_drop_mines_run(NULL, &drop_mines_ops, &tape, NULL)
	    && !yt_drop_mines_run(&state, NULL, &tape, NULL);
}

int
main(void)
{
#ifdef _WIN32
	char directory[] = "yt-score-test";
	if (_mkdir(directory) != 0)
		return fail("cannot create temporary directory");
#else
	char directory[] = "/tmp/yt-score-test.XXXXXX";
	if (mkdtemp(directory) == NULL)
		return fail("cannot create temporary directory");
#endif
	struct yt_game game;
	struct score_clock_script clock_script = {{
		{2026, 12, 31, 23, 59, 59, 49},
		{2027, 1, 1, 0, 0, 0, 0},
		{2027, 1, 1, 0, 0, 1, 0},
		{2027, 1, 1, 0, 0, 2, 0},
		{2027, 1, 1, 0, 0, 3, 0},
		{2027, 1, 1, 0, 0, 4, 0},
		{2027, 1, 1, 0, 0, 5, 0},
		{2027, 1, 1, 0, 0, 6, 0}
	}, 0};
	struct score_progress_tape progress = {0};
	struct yt_error error;
	struct yt_record blank;
	struct yt_sector sector;
	struct yt_player player;
	static const uint8_t player_tail[YT_RECORD_TAIL_SIZE] =
	    {0xde, 0xad, 0xbe, 0xef};
	static const uint8_t long_identity[] =
	    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwx";
	FILE *score;
	unsigned char bytes[1024];
	unsigned char expected_screen[1024];
	struct score_line_tape score_screen = {0};
	struct score_line_tape rich_screen = {0};
	size_t length;
	size_t expected_screen_length = 0;
	size_t lines = 0;
	size_t index;
	int result = EXIT_FAILURE;

	if (!check_formatter_boundaries())
		return fail("PRINT USING boundary formatting differs");
	if (!check_startup_configuration_transaction())
		return fail("startup configuration transaction differs");
	if (!check_current_player_cache_model())
		return fail("current-player cache model differs");
	if (!check_friendship_model())
		return fail("friendship model differs");
	if (!check_team_loader_model())
		return fail("team-loader model differs");
	if (!check_team_loader_transaction())
		return fail("team-loader transaction differs");
	if (!check_death_team_remove_transaction())
		return fail("death team-removal transaction differs");
	if (!check_info_team_resolver_transaction())
		return fail("Info team resolver transaction differs");
	if (!check_info_panel_transaction())
		return fail("Info panel transaction differs");
	if (!check_spy_sweep_transaction())
		return fail("active-spy sweep transaction differs");
	if (!check_projectile_parent_model())
		return fail("projectile parent model differs");
	if (!check_projectile_command_transaction())
		return fail("projectile command transaction differs");
	if (!check_projectile_cruise_opening_transaction())
		return fail("projectile cruise-opening transaction differs");
	if (!check_projectile_plasma_opening_transaction())
		return fail("projectile plasma-opening transaction differs");
	if (!check_projectile_plasma_route_transaction())
		return fail("projectile plasma-route transaction differs");
	if (!check_projectile_route_entry_transaction())
		return fail("projectile route-entry transaction differs");
	if (!check_projectile_cruise_reroute_transaction())
		return fail("projectile cruise-reroute transaction differs");
	if (!check_projectile_union_police_transaction())
		return fail("projectile Union Police transaction differs");
	if (!check_projectile_sector_probe_transaction())
		return fail("projectile sector-probe transaction differs");
	if (!check_projectile_plasma_fighter_transaction())
		return fail("projectile plasma-fighter transaction differs");
	if (!check_projectile_plasma_mine_transaction())
		return fail("projectile plasma-mine transaction differs");
	if (!check_projectile_plasma_dispatch_transaction())
		return fail("projectile plasma-dispatch transaction differs");
	if (!check_projectile_plasma_player_transaction())
		return fail("projectile plasma-player transaction differs");
	if (!check_projectile_plasma_killed_transaction())
		return fail("projectile plasma-killed transaction differs");
	if (!check_projectile_plasma_planet_transaction())
		return fail("projectile plasma-planet transaction differs");
	if (!check_projectile_plasma_footer_transaction())
		return fail("projectile plasma-footer transaction differs");
	if (!check_projectile_defense_front_transaction())
		return fail("projectile defense-front transaction differs");
	if (!check_projectile_defense_combat_transaction())
		return fail("projectile defense-combat transaction differs");
	if (!check_projectile_sector_mine_transaction())
		return fail("projectile sector-mine transaction differs");
	if (!check_projectile_damage_model())
		return fail("projectile player-damage model differs");
	if (!check_projectile_persistence_model())
		return fail("projectile persistence model differs");
	if (!check_projectile_planet_damage_model())
		return fail("projectile planet-damage model differs");
	if (!check_projectile_planet_impact_transaction())
		return fail("projectile planet-impact transaction differs");
	if (!check_projectile_bridge())
		return fail("projectile debit/resolver bridge differs");
	if (!check_xannor_victory_transaction())
		return fail("Xannor Headquarters victory transaction differs");
	if (!check_xannor_retaliation_model())
		return fail("Xannor retaliation transaction differs");
	if (!check_counterlaunch_model())
		return fail("player counterlaunch transaction differs");
	if (!check_salvage_cargo_sampler())
		return fail("salvage cargo sampler differs");
	if (!check_salvage_transaction())
		return fail("ship salvage transaction differs");
	if (!check_port_market_update())
		return fail("ordinary-port market updater differs");
	if (!check_port_update_transaction())
		return fail("ordinary-port updater transaction differs");
	if (!check_port_report_transaction())
		return fail("ordinary-port report transaction differs");
	if (!check_commodity_trade_transaction())
		return fail("commodity-trade transaction differs");
	if (!check_ordinary_commerce_transaction())
		return fail("ordinary-commerce controller transaction differs");
	if (!check_port_docking_transaction())
		return fail("port-docking front transaction differs");
	if (!check_treasury_transaction())
		return fail("owned-port treasury transaction differs");
	if (!check_movement_transaction())
		return fail("ordinary movement transaction differs");
	if (!check_main_fighters_transaction())
		return fail("main sector-fighter transaction differs");
	if (!check_genesis_transaction())
		return fail("Genesis transaction differs");
	if (!check_port_rename_transaction())
		return fail("port rename transaction differs");
	if (!check_port_rename_cycle_transaction())
		return fail("port rename scanner cycle differs");
	if (!check_main_prompt_transaction())
		return fail("main prompt transaction differs");
	if (!check_port_name_editor_model())
		return fail("port name editor model differs");
	if (!check_port_purchase_accept_transaction())
		return fail("port purchase accepted transaction differs");
	if (!check_port_purchase_transaction())
		return fail("port purchase transaction differs");
	if (!check_port_purchase_cycle_transaction())
		return fail("port purchase scanner cycle differs");
	if (!check_port_name_editor_transaction())
		return fail("port name editor transaction differs");
	if (!check_planet_garrison_model())
		return fail("planet garrison model differs");
	if (!check_planet_permission_transaction())
		return fail("planet landing permission transaction differs");
	if (!check_planet_updater_transaction())
		return fail("planet updater transaction differs");
	if (!check_planet_landing_model())
		return fail("planet landing model differs");
	if (!check_planet_assault_model())
		return fail("planet assault model differs");
	if (!check_planet_creation_model())
		return fail("planet creation model differs");
	if (!check_planet_move_model())
		return fail("planet move model differs");
	if (!check_sector_mine_model())
		return fail("sector mine model differs");
	if (!check_sector_mine_transaction())
		return fail("sector mine transaction differs");
	if (!check_drop_mines_transaction())
		return fail("Drop Mine transaction differs");
	if (!check_direct_fighter_kill_model())
		return fail("direct fighter kill model differs");
	if (!check_common_fatal_transaction())
		return fail("common fatal transaction differs");
	if (!check_direct_fighter_kill_transaction())
		return fail("direct fighter kill transaction differs");
	if (!check_player_death_transaction())
		return fail("player death transaction differs");
	if (!check_player_death_model())
		return fail("player death model differs");
	if (!check_emergency_warp_model())
		return fail("emergency warp model differs");
	if (!check_movement_model())
		return fail("movement model differs");
	if (!check_maintenance_entry_output())
		return fail("maintenance entry output differs");
	if (!check_maintenance_config_defaults())
		return fail("maintenance configuration defaults differ");
	if (!check_maintenance_player_aging())
		return fail("maintenance player aging differs");
	if (!check_maintenance_port_model())
		return fail("maintenance port model differs");
	if (!check_maintenance_mercenary_output())
		return fail("maintenance Mercenary output differs");
	if (!check_maintenance_planet_model())
		return fail("maintenance planet model differs");
	if (!check_maintenance_wanderer_model())
		return fail("maintenance Wanderer model differs");
	if (!check_maintenance_xannor_home_model())
		return fail("maintenance Xannor home model differs");
	if (!check_maintenance_xannor_hunt_model())
		return fail("maintenance Xannor hunt model differs");
	if (!check_maintenance_xannor_target_model())
		return fail("maintenance Xannor target model differs");
	if (!check_maintenance_xannor_regeneration_model())
		return fail("maintenance Xannor regeneration model differs");
	if (!check_formatter_sweep())
		return fail("PRINT USING raw-MBF oracle sweep differs");
	if (!check_datetime_format())
		return fail("DATE$/TIME$ formatting differs");
	if (!check_date_serial())
		return fail("DATE$ serial helper differs");
	if (!check_maintenance_route_enqueue())
		return fail("maintenance route enqueue helper differs");
	if (!check_player_name_match())
		return fail("returning-player fixed-field name match differs");
	if (!check_hostile_surrender_transaction())
		return fail("hostile surrender transaction differs");
	if (!check_hostile_attack_persistence_transaction())
		return fail("hostile Attack persistence transaction differs");
	if (!check_hostile_attack_tail_transaction())
		return fail("hostile Attack tail transaction differs");
	if (!check_hostile_attack_combat_transaction())
		return fail("hostile Attack composite transaction differs");
	if (!check_hostile_bribe_accept_transaction())
		return fail("hostile Bribe acceptance transaction differs");
	if (!check_hostile_bribe_transaction())
		return fail("hostile Bribe transaction differs");
	if (!check_direct_attack_attrition_model())
		return fail("direct Attack attrition model differs");
	if (!check_fighter_shield_spill_transaction())
		return fail("fighter/shield spill transaction differs");
	if (!check_direct_attack_combat_transaction())
		return fail("direct Attack combat transaction differs");
	if (!check_direct_attack_transaction())
		return fail("direct Attack selector transaction differs");
	if (!check_direct_attack_radio_model())
		return fail("direct Attack casualty-radio alias differs");
	if (!check_planet_rename_model())
		return fail("planet Rename model differs");
	if (!check_planet_take_all_overlays())
		return fail("planet Take-All overlay arithmetic differs");
	if (!check_planet_bank_overlays())
		return fail("planet Bank overlay arithmetic differs");
	if (!check_planet_menu_selector())
		return fail("planet menu selector differs");
	if (!check_credit_mutation_transaction())
		return fail("shared credit mutation transaction differs");
	if (!check_planet_productivity_overlays())
		return fail("planet Productivity overlay arithmetic differs");
	if (!check_clearance_model())
		return fail("clearance-sale predicate arithmetic differs");
	if (!check_earth_anti_cloak_transaction())
		return fail("Earth Anti-Cloak transaction differs");
	if (!check_earth_report_model())
		return fail("Earth report arithmetic or selector differs");
	if (!check_port_owner_row_model())
		return fail("port owner row model differs");
	if (yt_chdir(directory) != 0)
		return fail("cannot enter temporary directory");
	if (!check_maintenance_headquarters_write())
		goto done;
	if (!check_maintenance_protected_mines())
		goto done;
	if (!check_maintenance_header_writer())
		goto done;
	if (!check_maintenance_player_pass())
		goto done;
	if (!check_maintenance_port_pass())
		goto done;
	if (!check_maintenance_mercenary_tax_pass())
		goto done;
	if (!check_maintenance_mercenary_base_pass())
		goto done;
	if (!check_maintenance_mercenary_funding_pass())
		goto done;
	if (!check_maintenance_mercenary_defection_pass())
		goto done;
	if (!check_maintenance_mercenary_movement_pass())
		goto done;
	if (!check_maintenance_mercenary_lower_reentry_pass())
		goto done;
	if (!check_maintenance_mercenary_destination_pass())
		goto done;
	if (!check_maintenance_mercenary_mine_pass())
		goto done;
	if (!check_maintenance_mercenary_planet_pass())
		goto done;
	if (!check_maintenance_super_lottery_pass())
		goto done;
	if (!check_maintenance_final_marker_pass())
		goto done;
	if (!check_maintenance_planet_pass())
		goto done;
	if (!check_maintenance_wanderer_pass())
		goto done;
	if (!check_maintenance_xannor_home_pass())
		goto done;
	if (!check_maintenance_xannor_hunt_pass())
		goto done;
	if (!check_maintenance_xannor_group_extraction_pass())
		goto done;
	if (!check_maintenance_xannor_group_persistence_pass())
		goto done;
	if (!check_maintenance_xannor_route_arrivals_pass())
		goto done;
	if (!check_maintenance_xannor_headquarters_reclaim_pass())
		goto done;
	if (!check_maintenance_xannor_headquarters_relocation_pass())
		goto done;
	if (!check_maintenance_xannor_revenge_slot_pass())
		goto done;
	if (!check_maintenance_xannor_roaming_split())
		goto done;
	if (!check_maintenance_xannor_candidate_discovery())
		goto done;
	if (!check_player_constructor_failures())
		goto done;
	if (!check_post_login_repairs())
		goto done;
	if (!check_sector_force_routes())
		goto done;
	if (!check_hostile_menu_front())
		goto done;
	if (!check_admission_news())
		goto done;
	if (!check_maintenance_writers())
		goto done;
	yt_platform_set_clock_provider(score_clock_read, &clock_script);
	memset(&game, 0, sizeof(game));
	yt_error_clear(&error);
	if (!yt_database_open(&game.database, "YTDATA.DAT", YT_OPEN_CREATE,
	    &error))
		goto done;
	strcpy(game.config.scoreboard, "YTSCORE.ASC");
	game.config.scoreboard_length = 11.0f;
	game.config.epoch_year = 0.0f;
	game.config.sector_offset = 3.0f;
	game.config.port_offset = 5.0f;
	game.config.planet_offset = 5.0f;
	game.config.total_records = 5.0f;
	game.config.turns_per_day = 123.0f;
	game.config.initial_fighters = 45.0f;
	game.config.initial_credits = 678.0f;
	game.config.initial_holds = 9.0f;
	if (!yt_config_store(&game.database, &game.config, &error))
		goto close;
	yt_record_blank(&blank);
	if (!yt_database_write(&game.database, 2, &blank, &error)
	    || !yt_database_write(&game.database, 3, &blank, &error))
		goto close;
	yt_player_decode(&player, &blank);
	strcpy(player.name, "Old Trader");
	player.name_length = 10.0f;
	player.team = 4.0f;
	player.score = 77.5f;
	memcpy(player.record.bytes + YT_RECORD_TAIL_OFFSET, player_tail,
	    sizeof(player_tail));
	if (!yt_game_write_player(&game, 2, &player, &error))
		goto close;
	game.config.turns_per_day = 999.0f;
	game.config.initial_fighters = 999.0f;
	game.config.initial_credits = 999.0f;
	game.config.initial_holds = 999.0f;
	if (!yt_game_construct_player(&game, 2, 321.0f, &player, &error)
	    || !yt_game_read_player(&game, 2, &player, &error)
	    || strcmp(player.name, "Old Trader") != 0
	    || player.name_length != 10.0f || player.score != 77.5f
	    || player.last_active != 321.0f || player.killed_by != 0.0f
	    || player.turns != 123.0f || player.fighters != 45.0f
	    || player.credits != 678.0f || player.holds != 9.0f
	    || player.team != 0.0f
	    || memcmp(player.record.bytes + YT_RECORD_TAIL_OFFSET,
	    player_tail, sizeof(player_tail)) != 0
	    || sizeof(long_identity) - 1U != 50U
	    || !yt_game_set_player_identity(&game, 2, long_identity,
	    sizeof(long_identity) - 1U, &player, &error)
	    || !yt_game_read_player(&game, 2, &player, &error)
	    || memcmp(player.record.bytes, long_identity,
	    YT_TEXT_FIELD_SIZE) != 0 || player.name_length != 50.0f
	    || player.team != 0.0f || player.score != 77.5f
	    || player.turns != 123.0f || player.fighters != 45.0f
	    || memcmp(player.record.bytes + YT_RECORD_TAIL_OFFSET,
	    player_tail, sizeof(player_tail)) != 0
	    || !yt_database_write(&game.database, 2, &blank, &error))
		goto close;
	memset(&sector, 0, sizeof(sector));
	yt_record_blank(&sector.record);
	sector.fighters = 10.0f;
	sector.fighter_owner = -1.0f;
	if (!yt_game_write_sector(&game, 1, &sector, &error))
		goto close;
	sector.fighters = 0.0f;
	sector.fighter_owner = 0.0f;
	if (!yt_game_write_sector(&game, 2, &sector, &error)
	    || !yt_maintenance_scoreboard(&game, score_line_collect,
	    &score_screen, &error))
		goto close;
	score = fopen("YTSCORE.ASC", "rb");
	if (score == NULL)
		goto close;
	length = fread(bytes, 1, sizeof(bytes), score);
	if (ferror(score) || fclose(score) != 0)
		goto close;
	if (length >= sizeof(bytes))
		goto close;
	bytes[length] = '\0';
	for (index = 0; index + 1U < length; ++index) {
		if (bytes[index] == '\r' && bytes[index + 1U] == '\n')
			++lines;
	}
	for (index = 0; index < length && bytes[index] != 0x1a; ++index) {
		if (bytes[index] == '\r' && index + 1U < length
		    && bytes[index + 1U] == '\n') {
			expected_screen[expected_screen_length++] = '\r';
			++index;
		}
		else
			expected_screen[expected_screen_length++] = bytes[index];
	}
	if (length != 603U || lines != 19U || bytes[length - 1U] != 0x1a)
		goto close;
	if (score_screen.lines != 19U
	    || score_screen.length != expected_screen_length
	    || memcmp(score_screen.data, expected_screen,
	    expected_screen_length) != 0)
		goto close;
	if (strstr((const char *)bytes,
	    "Last updated at: 12-31-2026 00:00:00\r\n") == NULL)
		goto close;
	if (strstr((const char *)bytes,
	    "            1,000   100.00%                  0    0.00%\r\n")
	    == NULL)
		goto close;
	if (!yt_game_read_player(&game, 2, &player, &error)
	    || player.score != -1.0f
	    || !yt_game_read_player(&game, 3, &player, &error)
	    || player.score != -1.0f)
		goto close;
	sector.fighters = 0.0f;
	sector.fighter_owner = 0.0f;
	if (!yt_game_write_sector(&game, 1, &sector, &error))
		goto close;
	strcpy(game.config.scoreboard, "ZERO.ASC");
	yt_error_clear(&error);
	if (yt_score_generate(&game, &error)
	    || error.status != YT_RANGE)
		goto close;
	score = fopen("ZERO.ASC", "rb");
	if (score == NULL)
		goto close;
	length = fread(bytes, 1, sizeof(bytes), score);
	if (ferror(score) || fclose(score) != 0 || length >= sizeof(bytes))
		goto close;
	bytes[length] = '\0';
	if (length == 0 || bytes[length - 1U] != 0x1a
	    || strstr((const char *)bytes,
	    "================== =========  ================= =======\r\n")
	    == NULL)
		goto close;
	sector.fighters = 10.0f;
	sector.fighter_owner = -1.0f;
	if (!yt_game_write_sector(&game, 1, &sector, &error))
		goto close;
	strcpy(game.config.scoreboard, "NUL");
	yt_error_clear(&error);
	if (!yt_score_generate_progress(&game, score_progress_collect,
	    &progress, &error)
	    || progress.count != YT_ARRAY_LEN(progress.phases)
	    || progress.phases[0] != 1U || progress.phases[1] != 2U
	    || progress.phases[2] != 3U || progress.phases[3] != 4U)
		goto close;
	score = fopen("yttemp", "rb");
	if (score == NULL)
		goto close;
	if (fseek(score, 0, SEEK_END) != 0 || ftell(score) != 603L) {
		(void)fclose(score);
		goto close;
	}
	if (fclose(score) != 0)
		goto close;
	score = fopen("NUL", "rb");
	if (score != NULL) {
		(void)fclose(score);
		goto close;
	}

	yt_player_decode(&player, &blank);
	strcpy(player.name, "Alice");
	player.name_length = 5.0f;
	player.credits = 100.0f;
	player.team = 1.0f;
	if (!yt_game_write_player(&game, 2, &player, &error))
		goto close;
	yt_player_decode(&player, &blank);
	strcpy(player.name, "Bob");
	player.name_length = 3.0f;
	player.killed_by = -1.0f;
	player.credits = 9999.0f;
	player.team = 2.0f;
	if (!yt_game_write_player(&game, 3, &player, &error))
		goto close;
	memset(&sector, 0, sizeof(sector));
	yt_record_blank(&sector.record);
	yt_record_set_text(&sector.record, (const uint8_t *)"Team One", 8U);
	sector.fighters = 1.0f;
	sector.fighter_owner = 3.0f;
	if (!yt_game_write_sector(&game, 1, &sector, &error))
		goto close;
	memset(&sector, 0, sizeof(sector));
	yt_record_blank(&sector.record);
	yt_record_set_text(&sector.record, (const uint8_t *)"Team Two", 8U);
	sector.fighters = 1.0f;
	sector.fighter_owner = -1.0f;
	if (!yt_game_write_sector(&game, 2, &sector, &error))
		goto close;
	strcpy(game.config.scoreboard, "RICH.ASC");
	yt_error_clear(&error);
	if (!yt_maintenance_scoreboard(&game, score_line_collect,
	    &rich_screen, &error))
		goto close;
	score = fopen("RICH.ASC", "rb");
	if (score == NULL)
		goto close;
	length = fread(bytes, 1, sizeof(bytes), score);
	if (ferror(score) || fclose(score) != 0 || length >= sizeof(bytes))
		goto close;
	bytes[length] = '\0';
	lines = 0;
	expected_screen_length = 0;
	for (index = 0; index + 1U < length; ++index) {
		if (bytes[index] == '\r' && bytes[index + 1U] == '\n')
			++lines;
	}
	for (index = 0; index < length && bytes[index] != 0x1a; ++index) {
		if (bytes[index] == '\r' && index + 1U < length
		    && bytes[index + 1U] == '\n') {
			expected_screen[expected_screen_length++] = '\r';
			++index;
		}
		else
			expected_screen[expected_screen_length++] = bytes[index];
	}
	{
		char *bob = strstr((char *)bytes, "Bob\r\n");
		char *alice = strstr((char *)bytes, "Alice\r\n");
		char *team_two = strstr((char *)bytes, "Team Two\r\n");
		char *team_one = strstr((char *)bytes, "Team One\r\n");

		if (length == 0 || bytes[length - 1U] != 0x1a || lines != 23U
		    || bob == NULL || alice == NULL || alice >= bob
		    || team_two == NULL || team_one == NULL || team_one >= team_two
		    || rich_screen.lines != 23U
		    || rich_screen.length != expected_screen_length
		    || memcmp(rich_screen.data, expected_screen,
		    expected_screen_length) != 0)
			goto close;
	}
	if (!yt_game_read_player(&game, 2, &player, &error)
	    || player.score != 100.0f
	    || !yt_game_read_player(&game, 3, &player, &error)
	    || player.score != 100.0f)
		goto close;
	if (clock_script.position != YT_ARRAY_LEN(clock_script.values))
		goto close;
	result = EXIT_SUCCESS;

close:
	yt_database_close(&game.database);
done:
	yt_platform_set_clock_provider(NULL, NULL);
	remove("YTSCORE.ASC");
	remove("ZERO.ASC");
	remove("yttemp");
	remove("NUL");
	remove("RICH.ASC");
	remove("YTDATA.DAT");
	if (yt_chdir("..") == 0)
		(void)yt_rmdir(directory);
	if (result == EXIT_SUCCESS)
		puts("test_score: ok");
	return result;
}
