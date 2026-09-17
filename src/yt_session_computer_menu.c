#include "yt_session_internal.h"

#include <string.h>

static bool
computer_activate(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t notice[] = "<Computer activated>";

	session_set_foreground(session, 1.0f);
	return session_present_paged_line(session, notice, sizeof(notice) - 1U,
	    "computer activation notice", error)
	    && session_sound(session, YT_SOUND_CUE_ACTION, "computer activation sound", error);
}

static bool
computer_help(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t heading[] = " Computer commands:";
	static const char *const left[8] = {
		" 1) Exit Computer", " 3) Autopilot",
		" 5) Send Radio Message",
		" 7) Set autopilot Sectors to Avoid",
		" 9) Planet Report", "11) Fighter Finder (Yours)",
		"13) Planet Finder (Yours)", "15) Show Active Spies"
	};
	static const uint8_t *const right[8] = {
		(const uint8_t *)" 2) Port Report",
		(const uint8_t *)" 4) Rank Teams & Players",
		(const uint8_t *)" 6) Radio Message Log",
		(const uint8_t *)" 8) Galactic Newspaper",
		(const uint8_t *)"10) Path Finder",
		(const uint8_t *)"12) Port(s) Treasury Report",
		(const uint8_t *)"14) Find Nearest Ports",
		(const uint8_t *)"16) Find Port Pairs"
	};
	static const uint8_t final[] =
	    "17) Check Profits of Adjacent Ports";
	size_t index;

	session_set_pager_line_count(session, 0.0f);
	if (!session_present_paged_line(session, heading, sizeof(heading) - 1U,
	    "computer help heading", error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "computer help blank", error))
		return false;
	for (index = 0; index < 8U; ++index) {
		if (!session_fixed_width_bytes(session,
		    (const uint8_t *)left[index], strlen(left[index]), 40.0f,
		    "computer help left cell", error)
		    || !session_present_paged_fragment(session, right[index],
		    strlen((const char *)right[index])))
			return false;
	}
	return session_present_paged_fragment(session, final, sizeof(final) - 1U);
}

static bool
computer_menu_prompt(struct yt_session *session, char *command,
    size_t capacity, struct yt_error *error)
{
	uint8_t prompt[512];
	size_t prompt_length;
	size_t response_length;

	if (command == NULL || capacity < 3U
	    || !session_reload_player(session, error))
		return false;
	session->shared_status = 0.0f;
	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "computer prompt leading blank", error))
		return false;
	session_set_foreground(session, 1.0f);
	if (!yt_computer_prompt_row((const uint8_t *)session->time.text,
	    session->time.text_length, prompt, sizeof(prompt), &prompt_length)
	    || !session_present_timed_paged_row(session, prompt, prompt_length,
	    "computer prompt", error)
	    || !session_read_upper_command(session, command, capacity))
		return false;
	response_length = strlen(command);
	if (response_length == 0U) {
		command[0] = '?';
		command[1] = '\0';
	}
	else if (response_length > 2U)
		command[2] = '\0';
	return true;
}

bool
yt_session_computer_menu(struct yt_session *session, bool *enter_sector,
    struct yt_error *error)
{
	if (enter_sector != NULL)
		*enter_sector = false;
	if (!computer_activate(session, error))
		return false;
	for (;;) {
		char command[80];
		int position;

		if (!computer_menu_prompt(session, command, sizeof(command), error))
			return false;
		if (strcmp(command, "I") == 0) {
			if (!yt_session_show_ship(session, error))
				return false;
			continue;
		}
		if (strcmp(command, "17") == 0) {
			if (!yt_session_computer_profit(session, false, error))
				return false;
			continue;
		}
		if (strcmp(command, "!") == 0) {
			if (!yt_session_treasury(session, true, error))
				return false;
			continue;
		}
		if (strcmp(command, "S") == 0) {
			if (!yt_session_display_sector(session, true, error))
				return false;
			continue;
		}
		if (strcmp(command, "Q") == 0) {
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
		if (strcmp(command, "10") == 0) {
			if (!yt_session_computer_route(session, false, error))
				return false;
			continue;
		}
		if (strcmp(command, "11") == 0) {
			if (!yt_session_computer_owned_fighters(session, error))
				return false;
			continue;
		}
		if (strcmp(command, "12") == 0) {
			if (!yt_session_treasury(session, false, error))
				return false;
			continue;
		}
		if (strcmp(command, "13") == 0) {
			if (!yt_session_computer_owned_planets(session, error))
				return false;
			continue;
		}
		if (strcmp(command, "15") == 0) {
			if (!yt_session_list_spies(session, error))
				return false;
			continue;
		}
		if (strcmp(command, "16") == 0) {
			if (!yt_session_computer_profit(session, true, error))
				return false;
			continue;
		}
		if (strcmp(command, "14") == 0) {
			if (!yt_session_computer_nearest_ports(session, error))
				return false;
			continue;
		}
		if (strcmp(command, "7") == 0) {
			if (!yt_session_computer_avoid(session, error))
				return false;
			continue;
		}

		position = yt_computer_selector_position(command);
		if (position != 0) {
			switch (position - 1) {
			case 0:
				if (!yt_session_command_projectile(session, true, error))
					return false;
				if (enter_sector != NULL)
					*enter_sector = true;
				return true;
			case 1:
				if (!yt_session_command_projectile(session, false, error))
					return false;
				if (enter_sector != NULL)
					*enter_sector = true;
				return true;
			case 2:
				return yt_session_command_land(session, enter_sector, error);
			case 3: {
				bool moved;

				if (!yt_session_command_move(session, &moved, error))
					return false;
				if (enter_sector != NULL)
					*enter_sector = moved;
				return true;
			}
			case 4:
				if (!yt_session_command_trade(session, enter_sector, error))
					return false;
				return true;
			case 5:
				if (!computer_help(session, error))
					return false;
				continue;
			case 6: {
				static const uint8_t off[] = "<Computer deactivated>";

				if (!session_present_paged_line(session, off,
				    sizeof(off) - 1U,
				    "computer deactivation notice", error))
					return false;
				if (enter_sector != NULL)
					*enter_sector = true;
				return true;
			}
			case 7: {
				bool selected = false;

				if (!yt_session_computer_port_report(session, &selected,
				    error))
					return false;
				if (selected) {
					if (enter_sector != NULL)
						*enter_sector = true;
					return true;
				}
				continue;
			}
			case 8:
				if (!yt_session_computer_route(session, true, error))
					return false;
				continue;
			case 9:
				if (!yt_session_computer_scoreboard(session, error))
					return false;
				continue;
			case 10:
				if (!yt_session_radio_compose(session, error))
					return false;
				continue;
			case 11:
				if (!yt_session_computer_planet_report(session, error))
					return false;
				continue;
			default:
				break;
			}
		}
		if (strcmp(command, "6") == 0) {
			session->shared_status = 1.0f;
			if (!yt_session_radio_read(session, true, error))
				return false;
			continue;
		}
		if (strcmp(command, "8") == 0) {
			if (!yt_session_computer_newspaper(session, error))
				return false;
			continue;
		}
		if (strcmp(command, "C") == 0) {
			static const uint8_t warning[] =
			    "Don't BREAK the 'ON' button!";

			if (!session_present_paged_line(session, warning,
			    sizeof(warning) - 1U,
			    "computer reactivation warning", error)
			    || !computer_activate(session, error))
				return false;
			continue;
		}
		{
			static const uint8_t invalid[] = "Does not compute";

			if (!session_present_alert(session, invalid,
			    sizeof(invalid) - 1U, "computer invalid command", error))
				return false;
		}
	}
}
