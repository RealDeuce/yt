#include "yt_config_output.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { \
	if (!(condition)) { \
		fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, \
		    __LINE__, #condition); \
		return false; \
	} \
} while (0)

static bool
contains_bytes(const uint8_t *haystack, size_t haystack_length,
    const uint8_t *needle, size_t needle_length)
{
	size_t offset;

	if (needle_length > haystack_length)
		return false;
	for (offset = 0; offset <= haystack_length - needle_length; ++offset) {
		if (memcmp(haystack + offset, needle, needle_length) == 0)
			return true;
	}
	return false;
}

static bool
test_startup_working_values(void)
{
	static const uint8_t raw_path[] = {'A', 0x00U, 0x80U, 'Z'};
	struct yt_config config;
	struct yt_config_menu_working working;
	uint8_t path[41];

	memset(&config, 0, sizeof(config));
	config.local_screen = 0.6f;
	config.lottery_plays = 0.0f;
	config.maximum_holds = 19.0f;
	CHECK(yt_config_prepare_menu_working(&config, path, &working));
	CHECK(working.scoreboard_path == path);
	CHECK(working.scoreboard_path_length == 3U);
	CHECK(memcmp(path, "NUL", 3U) == 0);
	CHECK(working.local_screen == -1.0f);
	CHECK(working.lottery_plays == 1.0f);
	CHECK(working.maximum_holds == 200.0f);

	memcpy(config.record.bytes, raw_path, sizeof(raw_path));
	config.scoreboard_length = 3.6f;
	config.local_screen = -1.0f;
	config.lottery_plays = 2.0f;
	config.maximum_holds = 1001.0f;
	CHECK(yt_config_prepare_menu_working(&config, path, &working));
	CHECK(working.scoreboard_path_length == sizeof(raw_path));
	CHECK(memcmp(path, raw_path, sizeof(raw_path)) == 0);
	CHECK(working.local_screen == -1.0f);
	CHECK(working.lottery_plays == 2.0f);
	CHECK(working.maximum_holds == 1000.0f);
	return true;
}

static bool
test_canonical_menu(void)
{
	static const uint8_t expected[] =
	    "Yankee Trader Configuration Program\r"
	    "By Alan Davenport\r"
	    "\r"
	    "Version 1.8 -=- 03/13/94\r"
	    "\r"
	    "<A> Maximum Number of Holds: 100\r"
	    "<B> Turns per day: 50 \r"
	    "<C> Initial fighters: 500 \r"
	    "<D> Initial credits: 1000 \r"
	    "<E> Initial cargo holds: 20 \r"
	    "<F> Days until a dead player is deleted: 30 \r"
	    "<G> OK to run Maintenance?: Yes\r"
	    "<H> Xannor Headquarters is in: 85 \r"
	    "<I> Scoreboard File Path\\Name: YTSCORE.ASC\r"
	    "<J> Local Screen With Remote Callers: Off\r"
	    "<K> Maximum Lottery Plays Per Day : 3 \r"
	    "<L> Ports needed to initiate Genesis: DISABLED\r"
	    "<N> Player NAME/ALIAS editor.\r"
	    "<O> pOrt name editor.\r"
	    "<P> Planet name editor.\r"
	    "\r"
	    "<X> Exit Program\r"
	    "\r"
	    "Command: ";
	static const uint8_t scoreboard[] = "YTSCORE.ASC";
	struct yt_config config;
	struct yt_config_menu_working working;
	struct yt_config_output_result result;

	memset(&config, 0, sizeof(config));
	config.turns_per_day = 50.0f;
	config.initial_fighters = 500.0f;
	config.initial_credits = 1000.0f;
	config.initial_holds = 20.0f;
	config.retention_days = 30.0f;
	config.last_maintenance = 0.0f;
	config.headquarters = 85.0f;
	config.genesis_ports = 301.0f;
	working.scoreboard_path = scoreboard;
	working.scoreboard_path_length = sizeof(scoreboard) - 1U;
	working.local_screen = 0.0f;
	working.lottery_plays = 3.0f;
	working.maximum_holds = 100.0f;
	CHECK(yt_config_compose_menu_prompt(&config, &working, 42, 37,
	    &result));
	CHECK(result.output_length == sizeof(expected) - 1U);
	CHECK(memcmp(result.output, expected, sizeof(expected) - 1U) == 0);
	CHECK(result.final_column == 9U);
	return true;
}

static bool
test_alternate_rows_and_binary_path(void)
{
	static const uint8_t scoreboard[] = {'A', 0x00U, 0x80U, 'Z'};
	static const uint8_t ran[] =
	    "<G> OK to run Maintenance?: No, Ran Today Already\r";
	static const uint8_t on[] =
	    "<J> Local Screen With Remote Callers: On\r";
	static const uint8_t genesis[] =
	    "<L> Ports needed to initiate Genesis: 300\r";
	struct yt_config config;
	struct yt_config_menu_working working;
	struct yt_config_output_result result;

	memset(&config, 0, sizeof(config));
	config.last_maintenance = 42.0f;
	config.genesis_ports = 300.0f;
	working.scoreboard_path = scoreboard;
	working.scoreboard_path_length = sizeof(scoreboard);
	working.local_screen = -2.0f;
	working.lottery_plays = 1.0f;
	working.maximum_holds = 200.0f;
	CHECK(yt_config_compose_menu_prompt(&config, &working, 42, 0,
	    &result));
	CHECK(contains_bytes(result.output, result.output_length, ran,
	    sizeof(ran) - 1U));
	CHECK(contains_bytes(result.output, result.output_length, on,
	    sizeof(on) - 1U));
	CHECK(contains_bytes(result.output, result.output_length, genesis,
	    sizeof(genesis) - 1U));
	CHECK(contains_bytes(result.output, result.output_length, scoreboard,
	    sizeof(scoreboard)));
	return true;
}

static bool
test_dispatch_and_exit(void)
{
	static const uint8_t echo[] = "P\r\r";
	static const uint8_t ending[] = "-=* End of Run *=-\r";
	static const uint8_t missing[] =
	    "\aMAIN DATA FILE NOT FOUND. PLEASE RUN YT-INIT FIRST!\r";
	struct yt_config_output_result result;
	uint8_t folded;

	CHECK(yt_config_compose_command_echo((uint8_t)'p', 9, &folded,
	    &result));
	CHECK(folded == (uint8_t)'P');
	CHECK(result.output_length == sizeof(echo) - 1U);
	CHECK(memcmp(result.output, echo, sizeof(echo) - 1U) == 0);
	CHECK(result.final_column == 0U);
	CHECK(yt_config_compose_exit(0, &result));
	CHECK(result.output_length == sizeof(ending) - 1U);
	CHECK(memcmp(result.output, ending, sizeof(ending) - 1U) == 0);
	CHECK(result.final_column == 0U);
	CHECK(yt_config_compose_missing_data(0, &result));
	CHECK(result.output_length == sizeof(missing) - 1U);
	CHECK(memcmp(result.output, missing, sizeof(missing) - 1U) == 0);
	CHECK(result.final_column == 0U);
	return true;
}

static bool
test_genesis_editor(void)
{
	static const uint8_t expected[] =
	    "There are 1000 ports in the game, enter a number\r"
	    "greater than 1000 to TURN OFF the Genesis Function.\r"
	    "Version\r"
	    "How many ports will a player need to initiate Genesis? "
	    "[50 - 1000] ";
	struct yt_config_output_result result;

	CHECK(yt_config_compose_genesis_prompt((const uint8_t *)"Version", 7U,
	    0U, &result));
	CHECK(result.output_length == sizeof(expected) - 1U);
	CHECK(memcmp(result.output, expected, sizeof(expected) - 1U) == 0);
	CHECK(result.final_column == 67U);
	CHECK(result.local_beeps == 0U);
	CHECK(yt_config_compose_local_beep(result.final_column, &result));
	CHECK(result.output_length == 0U);
	CHECK(result.final_column == 67U);
	CHECK(result.local_beeps == 1U);
	CHECK(!yt_config_genesis_valid(49.999996f));
	CHECK(yt_config_genesis_valid(50.0f));
	CHECK(yt_config_genesis_valid(1001.0f));
	return true;
}

int
main(void)
{
	if (!test_startup_working_values()
	    || !test_canonical_menu()
	    || !test_alternate_rows_and_binary_path()
	    || !test_dispatch_and_exit()
	    || !test_genesis_editor())
		return 1;
	puts("ytconfig output tests passed");
	return 0;
}
