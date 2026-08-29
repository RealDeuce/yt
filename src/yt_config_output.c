#include "yt_config_output.h"

#include "qb.h"

#include <string.h>

#define YT_CONFIG_SCREEN_WIDTH 80U

static bool
append_bytes(struct yt_config_output_result *result, const uint8_t *data,
    size_t length)
{
	size_t index;

	if (length > YT_CONFIG_OUTPUT_SIZE - result->output_length)
		return false;
	if (length != 0U)
		memcpy(result->output + result->output_length, data, length);
	result->output_length += length;
	for (index = 0; index < length; ++index) {
		if (data[index] == '\r')
			result->final_column = 0U;
		else
			result->final_column =
			    (result->final_column + 1U) % YT_CONFIG_SCREEN_WIDTH;
	}
	return true;
}

static bool
append_literal(struct yt_config_output_result *result, const char *text)
{
	return append_bytes(result, (const uint8_t *)text, strlen(text));
}

static bool
append_line(struct yt_config_output_result *result, const char *text)
{
	static const uint8_t newline = '\r';

	return append_literal(result, text)
	    && append_bytes(result, &newline, 1U);
}

static bool
append_binary_line(struct yt_config_output_result *result,
    const uint8_t *text, size_t length)
{
	static const uint8_t newline = '\r';

	return append_bytes(result, text, length)
	    && append_bytes(result, &newline, 1U);
}

static bool
append_single(struct yt_config_output_result *result, float value,
    bool print_space)
{
	char number[64];
	int length = print_space
	    ? qb_print_single(number, sizeof(number), value)
	    : qb_str_single(number, sizeof(number), value);

	return length >= 0 && append_bytes(result, (const uint8_t *)number,
	    (size_t)length);
}

static bool
append_numeric_line(struct yt_config_output_result *result,
    const char *prefix, float value)
{
	static const uint8_t newline = '\r';

	return append_literal(result, prefix)
	    && append_single(result, value, true)
	    && append_bytes(result, &newline, 1U);
}

static bool
append_genesis_line(struct yt_config_output_result *result, float value)
{
	static const uint8_t newline = '\r';

	if (!append_literal(result,
	    "<L> Ports needed to initiate Genesis:"))
		return false;
	if (value > 300.0f) {
		if (!append_literal(result, " DISABLED"))
			return false;
	}
	else if (!append_single(result, value, false))
		return false;
	return append_bytes(result, &newline, 1U);
}

bool
yt_config_prepare_menu_working(const struct yt_config *config,
    uint8_t scoreboard_path[41], struct yt_config_menu_working *working)
{
	bool overflow;
	int path_length;

	if (config == NULL || scoreboard_path == NULL || working == NULL)
		return false;
	path_length = (int)qb_cint(config->scoreboard_length, &overflow);
	if (overflow || path_length < 0 || path_length > 41)
		return false;
	working->local_screen = config->local_screen;
	working->lottery_plays = config->lottery_plays;
	working->maximum_holds = config->maximum_holds;
	if (working->maximum_holds < 20.0f)
		working->maximum_holds = 200.0f;
	if (path_length == 0) {
		memcpy(scoreboard_path, "NUL", 3U);
		path_length = 3;
	}
	else
		memcpy(scoreboard_path, config->record.bytes, (size_t)path_length);
	working->scoreboard_path = scoreboard_path;
	working->scoreboard_path_length = (size_t)path_length;
	if (working->local_screen < -1.0f || working->local_screen > 0.0f)
		working->local_screen = -1.0f;
	if (working->lottery_plays < 1.0f)
		working->lottery_plays = 1.0f;
	if (working->maximum_holds < 5.0f
	    || working->maximum_holds > 1000.0f)
		working->maximum_holds = 1000.0f;
	return true;
}

bool
yt_config_compose_menu_prompt(const struct yt_config *config,
    const struct yt_config_menu_working *working, int today,
    size_t initial_column, struct yt_config_output_result *result)
{
	static const uint8_t newline = '\r';

	if (config == NULL || working == NULL || result == NULL
	    || (working->scoreboard_path == NULL
	    && working->scoreboard_path_length != 0U)
	    || working->scoreboard_path_length > 41U
	    || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	if (!append_line(result, "Yankee Trader Configuration Program")
	    || !append_line(result, "By Alan Davenport")
	    || !append_line(result, "")
	    || !append_line(result, "Version 1.8 -=- 03/13/94")
	    || !append_line(result, "")
	    || !append_literal(result, "<A> Maximum Number of Holds:")
	    || !append_single(result, working->maximum_holds, false)
	    || !append_bytes(result, &newline, 1U)
	    || !append_numeric_line(result, "<B> Turns per day:",
		config->turns_per_day)
	    || !append_numeric_line(result, "<C> Initial fighters:",
		config->initial_fighters)
	    || !append_numeric_line(result, "<D> Initial credits:",
		config->initial_credits)
	    || !append_numeric_line(result, "<E> Initial cargo holds:",
		config->initial_holds)
	    || !append_numeric_line(result,
		"<F> Days until a dead player is deleted:",
		config->retention_days)
	    || !append_literal(result, "<G> OK to run Maintenance?:")
	    || !append_line(result, config->last_maintenance == (float)today
		? " No, Ran Today Already" : " Yes")
	    || !append_numeric_line(result,
		"<H> Xannor Headquarters is in:", config->headquarters)
	    || !append_literal(result, "<I> Scoreboard File Path\\Name: ")
	    || !append_binary_line(result, working->scoreboard_path,
		working->scoreboard_path_length)
	    || !append_literal(result,
		"<J> Local Screen With Remote Callers: ")
	    || !append_line(result, working->local_screen == 0.0f ? "Off" : "On")
	    || !append_numeric_line(result,
		"<K> Maximum Lottery Plays Per Day :", working->lottery_plays)
	    || !append_genesis_line(result, config->genesis_ports)
	    || !append_line(result, "<N> Player NAME/ALIAS editor.")
	    || !append_line(result, "<O> pOrt name editor.")
	    || !append_line(result, "<P> Planet name editor.")
	    || !append_line(result, "")
	    || !append_line(result, "<X> Exit Program")
	    || !append_line(result, "")
	    || !append_literal(result, "Command: "))
		return false;
	return true;
}

bool
yt_config_compose_command_echo(uint8_t command, size_t initial_column,
    uint8_t *folded, struct yt_config_output_result *result)
{
	uint8_t output[3];

	if (folded == NULL || result == NULL
	    || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	*folded = command & 0xdfU;
	output[0] = *folded;
	output[1] = '\r';
	output[2] = '\r';
	return append_bytes(result, output, sizeof(output));
}

bool
yt_config_compose_exit(size_t initial_column,
    struct yt_config_output_result *result)
{
	if (result == NULL || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	return append_line(result, "-=* End of Run *=-");
}

bool
yt_config_compose_missing_data(size_t initial_column,
    struct yt_config_output_result *result)
{
	if (result == NULL || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	return append_line(result,
	    "\aMAIN DATA FILE NOT FOUND. PLEASE RUN YT-INIT FIRST!");
}

bool
yt_config_compose_genesis_prompt(const uint8_t *current_value,
    size_t current_value_length, size_t initial_column,
    struct yt_config_output_result *result)
{
	if (result == NULL || (current_value == NULL && current_value_length != 0U)
	    || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	return append_line(result,
	    "There are 1000 ports in the game, enter a number")
	    && append_line(result,
		"greater than 1000 to TURN OFF the Genesis Function.")
	    && append_binary_line(result, current_value, current_value_length)
	    && append_literal(result,
		"How many ports will a player need to initiate Genesis? "
		"[50 - 1000] ");
}

bool
yt_config_compose_local_beep(size_t initial_column,
    struct yt_config_output_result *result)
{
	if (result == NULL || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	result->local_beeps = 1U;
	return true;
}

bool
yt_config_genesis_valid(float threshold)
{
	return threshold >= 50.0f;
}

bool
yt_config_compose_hq_prompt(float current_hq, float upper_bound,
    size_t initial_column, struct yt_config_output_result *result)
{
	static const uint8_t newline = '\r';

	if (result == NULL || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	return append_literal(result,
	    "The Xannor Headquarters is currently in sector:")
	    && append_single(result, current_hq, true)
	    && append_bytes(result, &newline, 1U)
	    && append_literal(result, "Location? [ 8 to ")
	    && append_single(result, upper_bound, true)
	    && append_literal(result, "] -=> ");
}

bool
yt_config_compose_hq_diagnostic(enum yt_config_hq_diagnostic diagnostic,
    size_t initial_column, struct yt_config_output_result *result)
{
	const char *text;

	if (result == NULL || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	if (diagnostic == YT_CONFIG_HQ_INVALID)
		text = "Invalid Range!";
	else if (diagnostic == YT_CONFIG_HQ_OCCUPIED)
		text = "That sector is already occupied!";
	else
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	return append_line(result, text);
}

bool
yt_config_hq_in_range(float candidate, float upper_bound)
{
	return candidate >= 8.0f && candidate <= upper_bound;
}

bool
yt_config_compose_scoreboard_prompt(size_t initial_column,
    struct yt_config_output_result *result)
{
	if (result == NULL || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	return append_line(result,
	    "Enter new scoreboard and path or hit ENTER for 'YTSCORE.ASC'.");
}

bool
yt_config_compose_scoreboard_too_long(size_t initial_column,
    struct yt_config_output_result *result)
{
	if (result == NULL || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	return append_line(result, "Too long! 41 chars max!!");
}

bool
yt_config_compose_scalar_prompt(enum yt_config_scalar_key key,
    float working_maximum, size_t initial_column,
    struct yt_config_output_result *result)
{
	if (result == NULL || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	switch (key) {
	case YT_CONFIG_SCALAR_MAXIMUM_HOLDS:
		return append_literal(result,
		    "What is the Maximum amount of Cargo Holds allowed? "
		    "(5 - 1000) -=> ");
	case YT_CONFIG_SCALAR_TURNS:
		return append_literal(result,
		    "Turns allowed per day? (100 - 1000) ");
	case YT_CONFIG_SCALAR_FIGHTERS:
		return append_literal(result,
		    "Starting Number of Fighters? (1 to 10,000) -=> ");
	case YT_CONFIG_SCALAR_CREDITS:
		return append_literal(result,
		    "Starting credits? (25 to 10,000) -=> ");
	case YT_CONFIG_SCALAR_INITIAL_HOLDS:
		return append_literal(result,
		    "Starting Amount of Holds? (1 to ")
		    && append_single(result, working_maximum, true)
		    && append_literal(result, ") -=> ");
	case YT_CONFIG_SCALAR_DEAD_DAYS:
		return append_literal(result, "Days until deleted? ");
	case YT_CONFIG_SCALAR_MAINTENANCE:
		return append_literal(result,
		    "OK to Run Maintenence [Y/N] -=> ");
	case YT_CONFIG_SCALAR_LOTTERY:
		return append_literal(result,
		    "How many times per day may a user play the lottery? "
		    "(0 - 9) -=> ");
	default:
		return false;
	}
}

bool
yt_config_compose_scalar_rejection(enum yt_config_scalar_key key,
    size_t initial_column, struct yt_config_output_result *result)
{
	if (result == NULL || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	switch (key) {
	case YT_CONFIG_SCALAR_MAXIMUM_HOLDS:
	case YT_CONFIG_SCALAR_CREDITS:
	case YT_CONFIG_SCALAR_DEAD_DAYS:
		return true;
	case YT_CONFIG_SCALAR_TURNS:
	case YT_CONFIG_SCALAR_INITIAL_HOLDS:
		return append_line(result, "")
		    && append_line(result, " Invalid Range!");
	case YT_CONFIG_SCALAR_FIGHTERS:
		return append_line(result, "")
		    && append_line(result, "Invalid Range!");
	case YT_CONFIG_SCALAR_LOTTERY:
		return append_line(result, "Range is 1 to 10!");
	default:
		return false;
	}
}

bool
yt_config_scalar_blank_unchanged(enum yt_config_scalar_key key)
{
	return key != YT_CONFIG_SCALAR_MAXIMUM_HOLDS
	    && key != YT_CONFIG_SCALAR_LOTTERY;
}

bool
yt_config_scalar_valid(enum yt_config_scalar_key key, float value)
{
	switch (key) {
	case YT_CONFIG_SCALAR_MAXIMUM_HOLDS:
		return value >= 5.0f && value <= 1000.0f;
	case YT_CONFIG_SCALAR_TURNS:
		return value >= 100.0f && value <= 1000.0f;
	case YT_CONFIG_SCALAR_FIGHTERS:
		return value >= 0.0f && value <= 10000.0f;
	case YT_CONFIG_SCALAR_CREDITS:
		return value >= 25.0f && value <= 10000.0f;
	case YT_CONFIG_SCALAR_INITIAL_HOLDS:
		return value >= 0.0f && value <= 1000.0f;
	case YT_CONFIG_SCALAR_DEAD_DAYS:
		return value >= 1.0f;
	case YT_CONFIG_SCALAR_LOTTERY:
		return value >= 0.0f && value <= 9.0f;
	default:
		return false;
	}
}

bool
yt_config_compose_planet_entry(unsigned active_count, size_t initial_column,
    struct yt_config_output_result *result)
{
	static const uint8_t newline = '\r';

	if (result == NULL || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	if (!append_line(result, "Loading planet names...")
	    || !append_line(result, "")
	    || !append_literal(result, "There are")
	    || !append_single(result, (float)active_count, true)
	    || !append_line(result, " planets in your game."))
		return false;
	if (active_count != 0U)
		return true;
	result->local_beeps = 1U;
	return append_bytes(result, &newline, 1U)
	    && append_line(result, "YOUR GAME HAS NO PLANETS!");
}

bool
yt_config_compose_planet_menu(size_t initial_column,
    struct yt_config_output_result *result)
{
	if (result == NULL || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	return append_line(result, "Press enter to quit. Please Select:")
	    && append_line(result, "")
	    && append_line(result, "[L] List planets")
	    && append_line(result, "[C] Choose a planet to edit")
	    && append_line(result, "")
	    && append_literal(result, "-+> ");
}

bool
yt_config_compose_planet_key_echo(uint8_t key, size_t initial_column,
    uint8_t *folded, struct yt_config_output_result *result)
{
	uint8_t output[3];

	if (folded == NULL || result == NULL
	    || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	*folded = key & 0xdfU;
	output[0] = *folded;
	output[1] = '\r';
	output[2] = '\r';
	return append_bytes(result, output, sizeof(output));
}

bool
yt_config_compose_planet_number_prompt(size_t initial_column,
    struct yt_config_output_result *result)
{
	if (result == NULL || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	return append_literal(result, "Edit which planet number? ");
}

bool
yt_config_compose_planet_response_echo(uint8_t key, size_t initial_column,
    uint8_t *folded, struct yt_config_output_result *result)
{
	uint8_t output[2];

	if (folded == NULL || result == NULL
	    || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	*folded = key & 0xdfU;
	output[0] = *folded;
	output[1] = '\r';
	return append_bytes(result, output, sizeof(output));
}

bool
yt_config_compose_planet_list_header(size_t initial_column,
    struct yt_config_output_result *result)
{
	static const char rule[] =
	    "-------------------------------------------------------------------------------";

	if (result == NULL || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	return append_literal(result, "  #   Name")
	    && append_line(result, rule);
}

bool
yt_config_compose_planet_list_row(int logical, const uint8_t *name,
    size_t name_length, size_t initial_column,
    struct yt_config_output_result *result)
{
	if (result == NULL || logical < 1 || logical > 75
	    || (name == NULL && name_length != 0U)
	    || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	return (logical >= 10 || append_literal(result, " "))
	    && append_single(result, (float)logical, true)
	    && append_literal(result, ": ")
	    && append_binary_line(result, name, name_length);
}

bool
yt_config_compose_planet_pause(size_t initial_column,
    struct yt_config_output_result *result)
{
	if (result == NULL || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	return append_literal(result, "[ Pause ]");
}

bool
yt_config_compose_planet_blank(size_t initial_column,
    struct yt_config_output_result *result)
{
	if (result == NULL || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	return append_line(result, "");
}

bool
yt_config_compose_planet_invalid(const uint8_t *entered,
    size_t entered_length, size_t initial_column,
    struct yt_config_output_result *result)
{
	if (result == NULL || (entered == NULL && entered_length != 0U)
	    || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	result->local_beeps = 1U;
	return append_line(result, "")
	    && append_line(result, "INVALID PLANET NUMBER!!")
	    && append_binary_line(result, entered, entered_length);
}

bool
yt_config_compose_planet_protected(size_t initial_column,
    struct yt_config_output_result *result)
{
	if (result == NULL || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	result->local_beeps = 1U;
	return append_line(result,
	    "The planets \"The Wanderer\" and \"Xannoron\" cannot be re-named!")
	    && append_line(result, "");
}

bool
yt_config_compose_planet_edit(const uint8_t *name, size_t name_length,
    size_t initial_column, struct yt_config_output_result *result)
{
	if (result == NULL || (name == NULL && name_length != 0U)
	    || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	return append_literal(result, "Editing: ")
	    && append_binary_line(result, name, name_length)
	    && append_line(result, "")
	    && append_line(result, "Press enter to quit.")
	    && append_binary_line(result, name, name_length)
	    && append_literal(result, "Please enter new name. -=> ");
}

bool
yt_config_compose_planet_confirmation(const uint8_t *name,
    size_t name_length, size_t initial_column,
    struct yt_config_output_result *result)
{
	if (result == NULL || (name == NULL && name_length != 0U)
	    || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	return append_line(result, "")
	    && append_literal(result, "Change name to ")
	    && append_bytes(result, name, name_length)
	    && append_literal(result, "? [Y/N] -=> ");
}

bool
yt_config_compose_planet_cancel(const uint8_t *name, size_t name_length,
    size_t initial_column, struct yt_config_output_result *result)
{
	if (result == NULL || (name == NULL && name_length != 0U)
	    || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	return append_line(result, "")
	    && append_line(result, "Canceled!")
	    && append_binary_line(result, name, name_length);
}

bool
yt_config_compose_planet_saved(size_t initial_column,
    struct yt_config_output_result *result)
{
	if (result == NULL || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	return append_line(result, "New name saved! Press any key.");
}

bool
yt_config_planet_selection_in_range(float selection)
{
	return selection >= 1.0f && selection <= 75.0f;
}

bool
yt_config_planet_selection_protected(float selection)
{
	return selection == 1.0f || selection == 75.0f;
}

bool
yt_config_planet_pause_after(int logical, unsigned active_count)
{
	return logical % 20 == 0 || (unsigned)logical == active_count;
}

bool
yt_config_compose_port_search_prompt(size_t initial_column,
    struct yt_config_output_result *result)
{
	if (result == NULL || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	return append_line(result, "Press enter to quit.")
	    && append_line(result, "")
	    && append_literal(result,
		"Enter port name to change (Search String) -+> ");
}

bool
yt_config_compose_port_search_echo(const uint8_t *search,
    size_t search_length, size_t initial_column,
    struct yt_config_output_result *result)
{
	if (result == NULL || (search == NULL && search_length != 0U)
	    || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	return append_binary_line(result, search, search_length);
}

bool
yt_config_compose_port_match_prompt(const uint8_t *name,
    size_t name_length, size_t initial_column,
    struct yt_config_output_result *result)
{
	if (result == NULL || (name == NULL && name_length != 0U)
	    || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	return append_literal(result, "Change \"")
	    && append_bytes(result, name, name_length)
	    && append_literal(result, "\" [Y/N]? ");
}

bool
yt_config_compose_port_response_echo(uint8_t key, size_t initial_column,
    uint8_t *folded, struct yt_config_output_result *result)
{
	uint8_t output[3];

	if (folded == NULL || result == NULL
	    || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	*folded = key & 0xdfU;
	output[0] = *folded;
	output[1] = '\r';
	output[2] = '\r';
	return append_bytes(result, output, sizeof(output));
}

bool
yt_config_compose_port_not_found(size_t initial_column,
    struct yt_config_output_result *result)
{
	if (result == NULL || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	return append_line(result, "Not Found");
}

bool
yt_config_compose_port_end_list(size_t initial_column,
    struct yt_config_output_result *result)
{
	if (result == NULL || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	return append_line(result, "-= End of List =-");
}

bool
yt_config_compose_port_wait_prompt(size_t initial_column,
    struct yt_config_output_result *result)
{
	if (result == NULL || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	return append_literal(result, "Press Enter");
}

bool
yt_config_compose_port_replacement_prompt(size_t initial_column,
    struct yt_config_output_result *result)
{
	if (result == NULL || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	return append_line(result, "")
	    && append_line(result, "Please enter a new name for this port.")
	    && append_literal(result, "-=> ");
}

bool
yt_config_compose_port_confirmation(const uint8_t *name,
    size_t name_length, size_t initial_column,
    struct yt_config_output_result *result)
{
	if (result == NULL || (name == NULL && name_length != 0U)
	    || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	return append_literal(result, "\"")
	    && append_bytes(result, name, name_length)
	    && append_literal(result, "\" Is this OK? [Y/N]? ");
}

bool
yt_config_compose_port_cancel(size_t initial_column,
    struct yt_config_output_result *result)
{
	if (result == NULL || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	return append_line(result, "CANCELED!");
}

bool
yt_config_compose_port_saved(size_t initial_column,
    struct yt_config_output_result *result)
{
	if (result == NULL || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	return append_line(result, "Name change successful!!!");
}

bool
yt_config_compose_alias_entry(unsigned player_count, size_t initial_column,
    struct yt_config_output_result *result)
{
	static const uint8_t newline = '\r';

	if (result == NULL || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	if (!append_line(result, "Loading names...")
	    || !append_line(result, "")
	    || !append_literal(result, "There are")
	    || !append_single(result, (float)player_count, true)
	    || !append_line(result, " players in your game."))
		return false;
	if (player_count != 0U)
		return true;
	result->local_beeps = 1U;
	return append_bytes(result, &newline, 1U)
	    && append_line(result, "YOUR GAME HAS NO PLAYERS!");
}

bool
yt_config_compose_alias_menu(size_t initial_column,
    struct yt_config_output_result *result)
{
	if (result == NULL || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	return append_line(result, "Press enter to quit. Please Select:")
	    && append_line(result, "")
	    && append_line(result, "[L] List players/alias's")
	    && append_line(result, "[C] Choose player to edit")
	    && append_line(result, "")
	    && append_literal(result, "-+> ");
}

bool
yt_config_compose_alias_key_echo(uint8_t key, size_t initial_column,
    uint8_t *folded, struct yt_config_output_result *result)
{
	uint8_t output[3];

	if (folded == NULL || result == NULL
	    || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	*folded = key & 0xdfU;
	output[0] = *folded;
	output[1] = '\r';
	output[2] = '\r';
	return append_bytes(result, output, sizeof(output));
}

bool
yt_config_compose_alias_list_header(size_t initial_column,
    struct yt_config_output_result *result)
{
	static const char rule[] =
	    "============================================================================"
	    "===";

	if (result == NULL || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	return append_literal(result, "  #   Real Name")
	    && append_line(result, "Alias")
	    && append_line(result, rule);
}

bool
yt_config_compose_alias_list_row(int logical,
    const uint8_t *real_first, size_t real_first_length,
    const uint8_t *real_last, size_t real_last_length,
    const uint8_t *alias_first, size_t alias_first_length,
    const uint8_t *alias_last, size_t alias_last_length,
    size_t initial_column, struct yt_config_output_result *result)
{
	if (result == NULL || logical < 1
	    || (real_first == NULL && real_first_length != 0U)
	    || (real_last == NULL && real_last_length != 0U)
	    || (alias_first == NULL && alias_first_length != 0U)
	    || (alias_last == NULL && alias_last_length != 0U)
	    || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	return (logical >= 10 || append_literal(result, " "))
	    && append_single(result, (float)logical, true)
	    && append_literal(result, ": ")
	    && append_bytes(result, real_first, real_first_length)
	    && append_literal(result, " ")
	    && append_bytes(result, real_last, real_last_length)
	    && append_bytes(result, alias_first, alias_first_length)
	    && append_literal(result, " ")
	    && append_binary_line(result, alias_last, alias_last_length);
}

bool
yt_config_compose_alias_pause(size_t initial_column,
    struct yt_config_output_result *result)
{
	if (result == NULL || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	return append_literal(result, "[ Pause ]");
}

bool
yt_config_compose_alias_blank(size_t initial_column,
    struct yt_config_output_result *result)
{
	if (result == NULL || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	return append_line(result, "");
}

bool
yt_config_compose_alias_number_prompt(size_t initial_column,
    struct yt_config_output_result *result)
{
	if (result == NULL || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	return append_literal(result, "Edit which player number? ");
}

bool
yt_config_compose_alias_invalid(const uint8_t *entered,
    size_t entered_length, size_t initial_column,
    struct yt_config_output_result *result)
{
	if (result == NULL || (entered == NULL && entered_length != 0U)
	    || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	result->local_beeps = 1U;
	return append_line(result, "")
	    && append_line(result, "INVALID PLAYER NUMBER!!")
	    && append_binary_line(result, entered, entered_length);
}

bool
yt_config_compose_alias_edit(const uint8_t *real_first,
    size_t real_first_length, const uint8_t *real_last,
    size_t real_last_length, const uint8_t *alias_first,
    size_t alias_first_length, const uint8_t *alias_last,
    size_t alias_last_length, size_t initial_column,
    struct yt_config_output_result *result)
{
	if (result == NULL
	    || (real_first == NULL && real_first_length != 0U)
	    || (real_last == NULL && real_last_length != 0U)
	    || (alias_first == NULL && alias_first_length != 0U)
	    || (alias_last == NULL && alias_last_length != 0U)
	    || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	return append_literal(result, "Editing: ")
	    && append_bytes(result, real_first, real_first_length)
	    && append_literal(result, " ")
	    && append_bytes(result, real_last, real_last_length)
	    && append_literal(result, " a.k.a. ")
	    && append_bytes(result, alias_first, alias_first_length)
	    && append_literal(result, " ")
	    && append_binary_line(result, alias_last, alias_last_length)
	    && append_line(result, "")
	    && append_line(result, "Press enter to quit.")
	    && append_line(result, "")
	    && append_literal(result, "Please enter new Alias. -=> ");
}

bool
yt_config_compose_alias_confirmation(const uint8_t *alias,
    size_t alias_length, size_t initial_column,
    struct yt_config_output_result *result)
{
	if (result == NULL || (alias == NULL && alias_length != 0U)
	    || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	return append_line(result, "")
	    && append_literal(result, "Change player Alias to \"")
	    && append_bytes(result, alias, alias_length)
	    && append_literal(result, "\"? [Y/N] -=> ");
}

bool
yt_config_compose_alias_response_echo(uint8_t key, size_t initial_column,
    uint8_t *folded, struct yt_config_output_result *result)
{
	uint8_t output[2];

	if (folded == NULL || result == NULL
	    || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	*folded = key & 0xdfU;
	output[0] = *folded;
	output[1] = '\r';
	return append_bytes(result, output, sizeof(output));
}

bool
yt_config_compose_alias_cancel(size_t initial_column,
    struct yt_config_output_result *result)
{
	if (result == NULL || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	return append_line(result, "Canceled!") && append_line(result, "");
}

bool
yt_config_compose_alias_saved(size_t initial_column,
    struct yt_config_output_result *result)
{
	if (result == NULL || initial_column >= YT_CONFIG_SCREEN_WIDTH)
		return false;
	memset(result, 0, sizeof(*result));
	result->final_column = initial_column;
	return append_line(result, "New alias saved! Press any key.");
}

bool
yt_config_alias_selection_in_range(float selection, unsigned player_count)
{
	return selection >= 1.0f && selection <= (float)player_count;
}

bool
yt_config_alias_pause_after(int logical, unsigned player_count)
{
	return logical % 20 == 0 || (unsigned)logical == player_count;
}
