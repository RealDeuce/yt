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

static bool
test_headquarters_editor(void)
{
	static const uint8_t prompt[] =
	    "The Xannor Headquarters is currently in sector: 85 \r"
	    "Location? [ 8 to  2001 ] -=> ";
	static const uint8_t invalid[] = "Invalid Range!\r";
	static const uint8_t occupied[] =
	    "That sector is already occupied!\r";
	struct yt_config_output_result result;

	CHECK(yt_config_compose_hq_prompt(85.0f, 2001.0f, 0U, &result));
	CHECK(result.output_length == sizeof(prompt) - 1U);
	CHECK(memcmp(result.output, prompt, sizeof(prompt) - 1U) == 0);
	CHECK(result.final_column == 29U);
	CHECK(yt_config_compose_hq_diagnostic(YT_CONFIG_HQ_INVALID,
	    result.final_column, &result));
	CHECK(result.output_length == sizeof(invalid) - 1U);
	CHECK(memcmp(result.output, invalid, sizeof(invalid) - 1U) == 0);
	CHECK(result.final_column == 0U);
	CHECK(yt_config_compose_hq_diagnostic(YT_CONFIG_HQ_OCCUPIED, 29U,
	    &result));
	CHECK(result.output_length == sizeof(occupied) - 1U);
	CHECK(memcmp(result.output, occupied, sizeof(occupied) - 1U) == 0);
	CHECK(!yt_config_hq_in_range(7.9999995f, 2001.0f));
	CHECK(yt_config_hq_in_range(8.0f, 2001.0f));
	CHECK(yt_config_hq_in_range(2001.0f, 2001.0f));
	CHECK(!yt_config_hq_in_range(2001.0001f, 2001.0f));
	return true;
}

static bool
test_scalar_options(void)
{
	static const uint8_t scoreboard_prompt[] =
	    "Enter new scoreboard and path or hit ENTER for 'YTSCORE.ASC'.\r";
	static const uint8_t scoreboard_long[] =
	    "Too long! 41 chars max!!\r";
	static const struct {
		enum yt_config_scalar_key key;
		const char *prompt;
		const char *rejection;
	} cases[] = {
		{YT_CONFIG_SCALAR_MAXIMUM_HOLDS,
		    "What is the Maximum amount of Cargo Holds allowed? "
		    "(5 - 1000) -=> ", ""},
		{YT_CONFIG_SCALAR_TURNS,
		    "Turns allowed per day? (100 - 1000) ",
		    "\r Invalid Range!\r"},
		{YT_CONFIG_SCALAR_FIGHTERS,
		    "Starting Number of Fighters? (1 to 10,000) -=> ",
		    "\rInvalid Range!\r"},
		{YT_CONFIG_SCALAR_CREDITS,
		    "Starting credits? (25 to 10,000) -=> ", ""},
		{YT_CONFIG_SCALAR_INITIAL_HOLDS,
		    "Starting Amount of Holds? (1 to  200 ) -=> ",
		    "\r Invalid Range!\r"},
		{YT_CONFIG_SCALAR_DEAD_DAYS,
		    "Days until deleted? ", ""},
		{YT_CONFIG_SCALAR_MAINTENANCE,
		    "OK to Run Maintenence [Y/N] -=> ", NULL},
		{YT_CONFIG_SCALAR_LOTTERY,
		    "How many times per day may a user play the lottery? "
		    "(0 - 9) -=> ", "Range is 1 to 10!\r"}
	};
	struct yt_config_output_result result;
	size_t index;

	CHECK(yt_config_compose_scoreboard_prompt(17U, &result));
	CHECK(result.output_length == sizeof(scoreboard_prompt) - 1U);
	CHECK(memcmp(result.output, scoreboard_prompt,
	    sizeof(scoreboard_prompt) - 1U) == 0);
	CHECK(result.final_column == 0U);
	CHECK(yt_config_compose_scoreboard_too_long(29U, &result));
	CHECK(result.output_length == sizeof(scoreboard_long) - 1U);
	CHECK(memcmp(result.output, scoreboard_long,
	    sizeof(scoreboard_long) - 1U) == 0);
	for (index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
		CHECK(yt_config_compose_scalar_prompt(cases[index].key, 200.0f,
		    0U, &result));
		CHECK(result.output_length == strlen(cases[index].prompt));
		CHECK(memcmp(result.output, cases[index].prompt,
		    result.output_length) == 0);
		if (cases[index].rejection == NULL)
			continue;
		CHECK(yt_config_compose_scalar_rejection(cases[index].key,
		    result.final_column, &result));
		CHECK(result.output_length == strlen(cases[index].rejection));
		CHECK(memcmp(result.output, cases[index].rejection,
		    result.output_length) == 0);
	}
	CHECK(!yt_config_scalar_blank_unchanged(
	    YT_CONFIG_SCALAR_MAXIMUM_HOLDS));
	CHECK(!yt_config_scalar_blank_unchanged(YT_CONFIG_SCALAR_LOTTERY));
	CHECK(yt_config_scalar_blank_unchanged(YT_CONFIG_SCALAR_TURNS));
	CHECK(!yt_config_scalar_valid(YT_CONFIG_SCALAR_MAXIMUM_HOLDS, 4.0f));
	CHECK(yt_config_scalar_valid(YT_CONFIG_SCALAR_MAXIMUM_HOLDS, 5.0f));
	CHECK(yt_config_scalar_valid(YT_CONFIG_SCALAR_MAXIMUM_HOLDS, 1000.0f));
	CHECK(!yt_config_scalar_valid(YT_CONFIG_SCALAR_MAXIMUM_HOLDS, 1001.0f));
	CHECK(!yt_config_scalar_valid(YT_CONFIG_SCALAR_TURNS, 99.0f));
	CHECK(yt_config_scalar_valid(YT_CONFIG_SCALAR_TURNS, 1000.0f));
	CHECK(!yt_config_scalar_valid(YT_CONFIG_SCALAR_TURNS, 1001.0f));
	CHECK(yt_config_scalar_valid(YT_CONFIG_SCALAR_FIGHTERS, 0.0f));
	CHECK(!yt_config_scalar_valid(YT_CONFIG_SCALAR_FIGHTERS, -0.01f));
	CHECK(yt_config_scalar_valid(YT_CONFIG_SCALAR_FIGHTERS, 10000.0f));
	CHECK(!yt_config_scalar_valid(YT_CONFIG_SCALAR_CREDITS, 24.999f));
	CHECK(yt_config_scalar_valid(YT_CONFIG_SCALAR_CREDITS, 25.0f));
	CHECK(yt_config_scalar_valid(YT_CONFIG_SCALAR_INITIAL_HOLDS, 0.0f));
	CHECK(yt_config_scalar_valid(YT_CONFIG_SCALAR_INITIAL_HOLDS, 1000.0f));
	CHECK(!yt_config_scalar_valid(YT_CONFIG_SCALAR_DEAD_DAYS, 0.999f));
	CHECK(yt_config_scalar_valid(YT_CONFIG_SCALAR_DEAD_DAYS, 1.0f));
	CHECK(yt_config_scalar_valid(YT_CONFIG_SCALAR_DEAD_DAYS, 1.0e20f));
	CHECK(yt_config_scalar_valid(YT_CONFIG_SCALAR_LOTTERY, 0.0f));
	CHECK(yt_config_scalar_valid(YT_CONFIG_SCALAR_LOTTERY, 9.0f));
	CHECK(!yt_config_scalar_valid(YT_CONFIG_SCALAR_LOTTERY, 9.01f));
	return true;
}

static bool
test_planet_editor_output(void)
{
	static const uint8_t entry[] =
	    "Loading planet names...\r"
	    "\r"
	    "There are 3  planets in your game.\r";
	static const uint8_t empty[] =
	    "Loading planet names...\r"
	    "\r"
	    "There are 0  planets in your game.\r"
	    "\r"
	    "YOUR GAME HAS NO PLANETS!\r";
	static const uint8_t menu[] =
	    "Press enter to quit. Please Select:\r"
	    "\r"
	    "[L] List planets\r"
	    "[C] Choose a planet to edit\r"
	    "\r"
	    "-+> ";
	static const uint8_t header[] =
	    "  #   Name"
	    "-------------------------------------------------------------------------------\r";
	static const uint8_t row[] = "  1 : Earth\r";
	static const uint8_t invalid[] =
	    "\rINVALID PLANET NUMBER!!\r99\r";
	static const uint8_t protected[] =
	    "The planets \"The Wanderer\" and \"Xannoron\" cannot be re-named!\r\r";
	static const uint8_t edit[] =
	    "Editing: Earth\r\r"
	    "Press enter to quit.\r"
	    "Earth\r"
	    "Please enter new name. -=> ";
	static const uint8_t confirmation[] =
	    "\rChange name to Mars? [Y/N] -=> ";
	static const uint8_t cancel[] = "\rCanceled!\rMars\r";
	static const uint8_t saved[] = "New name saved! Press any key.\r";
	struct yt_config_output_result result;
	uint8_t folded;

	CHECK(yt_config_compose_planet_entry(3U, 0U, &result));
	CHECK(result.output_length == sizeof(entry) - 1U);
	CHECK(memcmp(result.output, entry, sizeof(entry) - 1U) == 0);
	CHECK(result.local_beeps == 0U);
	CHECK(yt_config_compose_planet_entry(0U, 0U, &result));
	CHECK(result.output_length == sizeof(empty) - 1U);
	CHECK(memcmp(result.output, empty, sizeof(empty) - 1U) == 0);
	CHECK(result.local_beeps == 1U);
	CHECK(yt_config_compose_planet_menu(0U, &result));
	CHECK(result.output_length == sizeof(menu) - 1U);
	CHECK(memcmp(result.output, menu, sizeof(menu) - 1U) == 0);
	CHECK(yt_config_compose_planet_key_echo('l', result.final_column,
	    &folded, &result));
	CHECK(folded == 'L');
	CHECK(result.output_length == 3U);
	CHECK(memcmp(result.output, "L\r\r", 3U) == 0);
	CHECK(yt_config_compose_planet_list_header(0U, &result));
	CHECK(result.output_length == sizeof(header) - 1U);
	CHECK(memcmp(result.output, header, sizeof(header) - 1U) == 0);
	CHECK(yt_config_compose_planet_list_row(1,
	    (const uint8_t *)"Earth", 5U, 0U, &result));
	CHECK(result.output_length == sizeof(row) - 1U);
	CHECK(memcmp(result.output, row, sizeof(row) - 1U) == 0);
	CHECK(yt_config_compose_planet_pause(0U, &result));
	CHECK(result.output_length == 9U);
	CHECK(memcmp(result.output, "[ Pause ]", 9U) == 0);
	CHECK(yt_config_compose_planet_invalid((const uint8_t *)"99", 2U,
	    0U, &result));
	CHECK(result.output_length == sizeof(invalid) - 1U);
	CHECK(memcmp(result.output, invalid, sizeof(invalid) - 1U) == 0);
	CHECK(result.local_beeps == 1U);
	CHECK(yt_config_compose_planet_protected(0U, &result));
	CHECK(result.output_length == sizeof(protected) - 1U);
	CHECK(memcmp(result.output, protected, sizeof(protected) - 1U) == 0);
	CHECK(result.local_beeps == 1U);
	CHECK(yt_config_compose_planet_edit((const uint8_t *)"Earth", 5U,
	    0U, &result));
	CHECK(result.output_length == sizeof(edit) - 1U);
	CHECK(memcmp(result.output, edit, sizeof(edit) - 1U) == 0);
	CHECK(yt_config_compose_planet_confirmation((const uint8_t *)"Mars", 4U,
	    result.final_column, &result));
	CHECK(result.output_length == sizeof(confirmation) - 1U);
	CHECK(memcmp(result.output, confirmation,
	    sizeof(confirmation) - 1U) == 0);
	CHECK(yt_config_compose_planet_response_echo('x', result.final_column,
	    &folded, &result));
	CHECK(folded == 'X');
	CHECK(result.output_length == 2U);
	CHECK(memcmp(result.output, "X\r", 2U) == 0);
	CHECK(yt_config_compose_planet_cancel((const uint8_t *)"Mars", 4U,
	    0U, &result));
	CHECK(result.output_length == sizeof(cancel) - 1U);
	CHECK(memcmp(result.output, cancel, sizeof(cancel) - 1U) == 0);
	CHECK(yt_config_compose_planet_saved(0U, &result));
	CHECK(result.output_length == sizeof(saved) - 1U);
	CHECK(memcmp(result.output, saved, sizeof(saved) - 1U) == 0);
	CHECK(!yt_config_planet_selection_in_range(0.999999f));
	CHECK(yt_config_planet_selection_in_range(1.0f));
	CHECK(yt_config_planet_selection_in_range(75.0f));
	CHECK(!yt_config_planet_selection_in_range(75.00001f));
	CHECK(yt_config_planet_selection_protected(1.0f));
	CHECK(!yt_config_planet_selection_protected(1.4f));
	CHECK(!yt_config_planet_selection_protected(74.6f));
	CHECK(yt_config_planet_selection_protected(75.0f));
	CHECK(yt_config_planet_pause_after(20, 3U));
	CHECK(yt_config_planet_pause_after(3, 3U));
	CHECK(!yt_config_planet_pause_after(4, 3U));
	return true;
}

int
main(void)
{
	if (!test_startup_working_values()
	    || !test_canonical_menu()
	    || !test_alternate_rows_and_binary_path()
	    || !test_dispatch_and_exit()
	    || !test_genesis_editor()
	    || !test_headquarters_editor()
	    || !test_scalar_options()
	    || !test_planet_editor_output())
		return 1;
	puts("ytconfig output tests passed");
	return 0;
}
