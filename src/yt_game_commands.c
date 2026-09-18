#include "yt_game.h"

#include "qb.h"

#include <string.h>

bool
yt_hostile_menu_row(double ship_fighters, double deployed_fighters,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "Fighters:";
	static const uint8_t separator[] = " /";
	char ship[64];
	char deployed[64];
	int ship_length;
	int deployed_length;
	size_t needed;
	size_t position = 0;

	if (length == NULL)
		return false;
	*length = 0;
	ship_length = qb_str_double(ship, sizeof(ship), ship_fighters);
	deployed_length = qb_str_double(deployed, sizeof(deployed),
	    deployed_fighters);
	if (ship_length < 0 || deployed_length < 0)
		return false;
	needed = sizeof(prefix) - 1U + (size_t)ship_length
	    + sizeof(separator) - 1U + (size_t)deployed_length;
	if (needed > capacity || (needed != 0 && row == NULL))
		return false;
	memcpy(row + position, prefix, sizeof(prefix) - 1U);
	position += sizeof(prefix) - 1U;
	memcpy(row + position, ship, (size_t)ship_length);
	position += (size_t)ship_length;
	memcpy(row + position, separator, sizeof(separator) - 1U);
	position += sizeof(separator) - 1U;
	memcpy(row + position, deployed, (size_t)deployed_length);
	position += (size_t)deployed_length;
	*length = position;
	return true;
}

enum yt_hostile_menu_route
yt_hostile_menu_dispatch(const char *response)
{
	static const char dispatch[] = "AQBDWT";
	const char *position;

	if (response == NULL || response[0] == '\0'
	    || strcmp(response, "?") == 0)
		return YT_HOSTILE_MENU_HELP;
	if (strcmp(response, "S") == 0)
		return YT_HOSTILE_MENU_SECTOR;
	if (strcmp(response, "I") == 0)
		return YT_HOSTILE_MENU_INFO;
	position = strstr(dispatch, response);
	if (position == NULL)
		return YT_HOSTILE_MENU_INVALID;
	switch (position - dispatch) {
	case 0:
		return YT_HOSTILE_MENU_ATTACK;
	case 1:
		return YT_HOSTILE_MENU_QUIT;
	case 2:
		return YT_HOSTILE_MENU_BRIBE;
	case 3:
		return YT_HOSTILE_MENU_MINE;
	case 4:
		return YT_HOSTILE_MENU_WARP;
	case 5:
		return YT_HOSTILE_MENU_TEAM;
	default:
		return YT_HOSTILE_MENU_INVALID;
	}
}

enum yt_main_shell_route
yt_main_shell_dispatch(const char *response, char missile_key)
{
	char dispatch[] = "W)+ABCFLMPQTD$GN";
	static const enum yt_main_shell_route routes[] = {
		YT_MAIN_SHELL_WARP,
		YT_MAIN_SHELL_MISSILE,
		YT_MAIN_SHELL_PLASMA,
		YT_MAIN_SHELL_ATTACK,
		YT_MAIN_SHELL_BUY_PORT,
		YT_MAIN_SHELL_COMPUTER,
		YT_MAIN_SHELL_FIGHTERS,
		YT_MAIN_SHELL_LAND,
		YT_MAIN_SHELL_MOVE,
		YT_MAIN_SHELL_TRADE,
		YT_MAIN_SHELL_QUIT,
		YT_MAIN_SHELL_TEAM,
		YT_MAIN_SHELL_MINES,
		YT_MAIN_SHELL_COLLECT,
		YT_MAIN_SHELL_GENESIS,
		YT_MAIN_SHELL_RENAME_PORT,
	};
	const char *position;

	dispatch[1] = missile_key;

	if (response == NULL || response[0] == '\0')
		return YT_MAIN_SHELL_DISPLAY;
	if (strcmp(response, "X") == 0)
		return YT_MAIN_SHELL_SOUND;
	if (strcmp(response, "S") == 0)
		return YT_MAIN_SHELL_SENSORS;
	position = strchr(dispatch, response[0]);
	if (position != NULL)
		return routes[position - dispatch];
	switch (response[0]) {
	case 'V':
		return YT_MAIN_SHELL_VERSION;
	case 'I':
		return YT_MAIN_SHELL_INFO;
	case 'Z':
		return YT_MAIN_SHELL_INSTRUCTIONS;
	case '?':
		return YT_MAIN_SHELL_HELP;
	default:
		return YT_MAIN_SHELL_INVALID;
	}
}

bool
yt_main_prompt_row(const uint8_t *time_text, size_t time_text_length,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "Time:";
	static const uint8_t suffix[] = "Main Command (?=Help)? ";
	size_t needed = sizeof(prefix) - 1U + time_text_length
	    + sizeof(suffix) - 1U;

	if (length == NULL || (time_text == NULL && time_text_length != 0U)
	    || (row == NULL && needed != 0U) || needed > capacity)
		return false;
	memcpy(row, prefix, sizeof(prefix) - 1U);
	if (time_text_length != 0U)
		memcpy(row + sizeof(prefix) - 1U, time_text, time_text_length);
	memcpy(row + sizeof(prefix) - 1U + time_text_length, suffix,
	    sizeof(suffix) - 1U);
	*length = needed;
	return true;
}

bool
yt_computer_prompt_row(const uint8_t *time_text, size_t time_text_length,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "Time:";
	static const uint8_t suffix[] = "Computer command (?=help)? ";
	size_t needed = sizeof(prefix) - 1U + time_text_length
	    + sizeof(suffix) - 1U;

	if (length == NULL || (time_text == NULL && time_text_length != 0U)
	    || (row == NULL && needed != 0U) || needed > capacity)
		return false;
	memcpy(row, prefix, sizeof(prefix) - 1U);
	if (time_text_length != 0U)
		memcpy(row + sizeof(prefix) - 1U, time_text, time_text_length);
	memcpy(row + sizeof(prefix) - 1U + time_text_length, suffix,
	    sizeof(suffix) - 1U);
	*length = needed;
	return true;
}

enum yt_computer_newspaper_choice
yt_computer_newspaper_select(const char *response)
{
	if (response == NULL || response[1] != '\0')
		return YT_COMPUTER_NEWSPAPER_NONE;
	if (response[0] == 'T')
		return YT_COMPUTER_NEWSPAPER_TODAY;
	if (response[0] == 'Y')
		return YT_COMPUTER_NEWSPAPER_YESTERDAY;
	return YT_COMPUTER_NEWSPAPER_NONE;
}
