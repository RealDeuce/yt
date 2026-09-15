#include "yt_session_internal.h"

#include "qb.h"
#include "yt_port_math.h"

#include <stdio.h>
#include <string.h>

static bool
planet_menu_error(struct yt_error *error, const char *operation)
{
	if (error != NULL) {
		error->status = YT_RANGE;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
	}
	return false;
}

bool
yt_session_planet_menu(struct yt_session *session, int logical_planet,
    bool *enter_sector, struct yt_error *error)
{
	static const uint8_t prompt_prefix[] = "Time:";
	static const uint8_t prompt_body[] =
	    "Planet command (?=help) [A]? ";

	session->planet.current_physical_record = qb_single_add(
	    session_planet_offset(session), (float)logical_planet);
	for (;;) {
		char upper[80];
		char free_text[64];
		char free_row[160];
		uint8_t prompt[sizeof(prompt_prefix) - 1U
		    + sizeof(session->time.text) + sizeof(prompt_body) - 1U];
		size_t prompt_length = 0;
		double free_holds;
		int position;

		session_set_pager_line_count(session, 0.0f);
		if (!session_reload_player(session, error))
			return false;
		free_holds = qb_double_subtract(qb_double_subtract(
		    qb_double_subtract((double)session->player.holds,
		    (double)session->player.ore),
		    (double)session->player.organics),
		    (double)session->player.equipment);
		if (qb_str_double(free_text, sizeof(free_text), free_holds) < 0
		    || snprintf(free_row, sizeof(free_row),
		    "You have%s free cargo holds.", free_text) < 0
		    || !session_present_paged_line(session, (const uint8_t *)free_row,
		    strlen(free_row), "planet free-holds row", error)
		    || !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "planet prompt framing blank", error))
			return false;
		session_set_foreground(session, 6.0f);
		memcpy(prompt + prompt_length, prompt_prefix,
		    sizeof(prompt_prefix) - 1U);
		prompt_length += sizeof(prompt_prefix) - 1U;
		if (session->time.text_length > sizeof(session->time.text))
			return planet_menu_error(error,
			    "planet prompt time capacity");
		memcpy(prompt + prompt_length, session->time.text,
		    session->time.text_length);
		prompt_length += session->time.text_length;
		memcpy(prompt + prompt_length, prompt_body,
		    sizeof(prompt_body) - 1U);
		prompt_length += sizeof(prompt_body) - 1U;
		if (!session_reload_player(session, error)
		    || !yt_session_update_planet(session, logical_planet,
		    &(struct yt_planet){0}, NULL, error)
		    || !session_present_timed_paged_row(session, prompt,
		    prompt_length, "planet command prompt", error)
		    || !session_read_upper_command(session, upper, sizeof(upper)))
			return false;
		if (upper[0] == '\0')
			strcpy(upper, "A");
		if (strcmp(upper, "I") == 0) {
			if (!yt_session_show_ship(session, error))
				return false;
			continue;
		}
		if (strcmp(upper, "N") == 0) {
			if (!yt_session_planet_rename(session, logical_planet, NULL,
			    error))
				return false;
			continue;
		}
		if (strcmp(upper, "S") == 0) {
			if (!yt_session_display_sector(session, true, error))
				return false;
			continue;
		}
		if (strcmp(upper, "Q") == 0) {
			bool confirmed;

			if (!session_quit_confirm(session, &confirmed, error))
				return false;
			if (!confirmed)
				continue;
			if (!yt_session_quit(session, error))
				return false;
			session->running = false;
			session->terminated = true;
			return false;
		}
		if (strcmp(upper, "D") == 0) {
			if (!yt_session_planet_inventory(session, logical_planet,
			    error))
				return false;
			continue;
		}
		if (strcmp(upper, "?") == 0) {
			static const uint8_t heading[] = "<Help>";
			static const char *const rows[] = {
				"1 - Take Ore",
				"2 - Take Organics",
				"3 - Take Equipment",
				"4 - Take Fighters",
				"5 - Take Missiles",
				"6 - Take Mines",
				"9 - Take Plasma Bolts",
				"A - Take <A>ll (Default)",
				"B - Planet's <B>ank",
				"D - <D>isplay Planet",
				"F - Take/Leave Ground <F>orces",
				"L - <L>eave Planet",
				"N - Re-<N>ame Planet",
				"T - <T>ransfer Cargo to Planet",
				"! - Use Planet Thrusters",
				"$ - Raise Productivity"
			};
			size_t row;

			if (!session_present_paged_line(session, heading,
			    sizeof(heading) - 1U, "planet help heading", error)
			    || !session_present_paged_line(session,
			    (const uint8_t *)rows[0], strlen(rows[0]),
			    "planet help first row", error))
				return false;
			for (row = 1; row < YT_ARRAY_LEN(rows); ++row) {
				if (!session_present_paged_fragment(session,
				    (const uint8_t *)rows[row], strlen(rows[row])))
					return false;
			}
			continue;
		}
		position = yt_planet_menu_selector_position(upper);
		if (position == 0) {
			static const uint8_t invalid[] = "Invalid command.";

			if (!session_present_alert(session, invalid,
			    sizeof(invalid) - 1U, "planet invalid command", error))
				return false;
			continue;
		}
		switch (position - 1) {
		case 0:
			if (!yt_session_planet_garrison(session, logical_planet, error))
				return false;
			break;
		case 1:
			if (!yt_session_planet_move(session, enter_sector, error))
				return false;
			if (enter_sector != NULL && *enter_sector)
				return true;
			break;
		case 2: {
			bool moved;

			if (!yt_session_command_move(session, &moved, error))
				return false;
			if (enter_sector != NULL)
				*enter_sector = moved;
			return true;
		}
		case 3: {
			bool selected = false;

			if (!yt_session_command_trade(session, &selected, error))
				return false;
			if (selected) {
				if (enter_sector != NULL)
					*enter_sector = true;
				return true;
			}
			break;
		}
		case 4:
			return yt_session_computer_menu(session, enter_sector, error);
		case 5: case 6: case 7: case 8: case 9: case 10:
			if (!yt_session_planet_take_one(session, logical_planet,
			    position - 5, error))
				return false;
			break;
		case 11:
			if (!yt_session_planet_take_one(session, logical_planet, 9,
			    error))
				return false;
			break;
		case 12:
			if (enter_sector != NULL)
				*enter_sector = true;
			return true;
		case 13:
			if (!yt_session_planet_transfer(session, logical_planet, error))
				return false;
			break;
		case 14:
			if (!yt_session_planet_take_all(session, logical_planet, error))
				return false;
			break;
		case 15:
			if (!yt_session_planet_bank(session, logical_planet, error))
				return false;
			break;
		case 16:
			if (!yt_session_planet_productivity(session, logical_planet,
			    error))
				return false;
			break;
		default:
			break;
		}
		if (!session->running)
			return true;
	}
}
