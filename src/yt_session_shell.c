#include "yt_session_internal.h"

#include "yt_output.h"

#include <stdio.h>
#include <string.h>

bool
session_quit_confirm(struct yt_session *session, bool *confirmed,
    struct yt_error *error)
{
	static const uint8_t heading[] = "<Quit>";
	static const uint8_t prompt[] = "Are you sure (Y/N)? ";

	if (confirmed == NULL)
		return false;
	*confirmed = false;
	session_set_foreground(session, 7.0f);
	if (!session_present_paged_fragment(session, heading, sizeof(heading) - 1U))
		return false;
	for (;;) {
		char response[80];
		enum yt_yes_no_answer answer;

		if (!session_present_text(session, prompt, sizeof(prompt) - 1U,
		    SESSION_PRESENT_RAW, "hostile quit prompt", error)
		    || !session_read_upper_command(session, response, sizeof(response)))
			return false;
		if (!yt_input_yes_no_candidate(response, session->io.text_workspace,
		    sizeof(session->io.text_workspace), &answer))
			return false;
		if (answer == YT_YES_NO_YES) {
			*confirmed = true;
			return true;
		}
		if (answer == YT_YES_NO_NO || answer == YT_YES_NO_EMPTY)
			return true;
		yt_present_set_bold(&session->presentation, 1.0f);
		session_clear_queue(session);
	}
}

static bool
show_help(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t heading[] = "<Help>";
	static const char *const pairs[][2] = {
		{"[ENTER] - Re-display sector",
		    "$ - Take Credits from your ports"},
		{"! - Launch a Cruise Missile",
		    "A - <A>ttack a player's ship"},
		{"B - <B>uy a Port", "C - Ship's <C>omputer"},
		{"D - <D>rop a Sector mine",
		    "F - Take or leave <F>ighters"},
		{"G - Initiate <G>enesis", "I - <I>nfo on your ship"},
		{"L - <L>and on or create a planet",
		    "M - <M>ove to another sector"},
		{"N - Re<N>ame Port",
		    "P - Dock at a <P>ort (and trade)"},
		{"Q - <Q>uit game", "S - <S>ensors"},
		{"T - <T>eam menu", "V - <V>ersion Info"},
		{"W - Emergency <W>arp", "X - Sound Effects On/Off"},
		{"Z - Instructions", "+ - Fire Plasma Bolt"},
	};
	static const char *const narrative[] = {
		"String commands by seperating them with a semicolons (;).",
		"To place an EXTRA 'hit enter' in a string, use an extra ';'.",
		"Save a command string by placing a '/' at the end.",
		"Then hit Control-R to [R]eplay the saved command.",
		"You may repeat any command up to 20 times by putting",
		"a /R# at the end of your command. Replace the '#' with",
		"any number between 2 and 20. Example: your command/R20"
	};
	char row[128];
	size_t index;

	session_set_foreground(session, 6.0f);
	if (!session_present_paged_line(session, heading, sizeof(heading) - 1U,
	    "main help heading", error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "main help table blank", error))
		return false;
	for (index = 0; index < sizeof(pairs) / sizeof(pairs[0]); ++index) {
		int length = snprintf(row, sizeof(row), "%-40s%s",
		    pairs[index][0], pairs[index][1]);

		if (length < 0 || (size_t)length >= sizeof(row)) {
			if (error != NULL) {
				error->status = YT_RANGE;
				(void)snprintf(error->operation,
				    sizeof(error->operation), "%s",
				    "main help row capacity");
			}
			return false;
		}
		if (!session_present_text(session, (const uint8_t *)row,
		    (size_t)length, SESSION_PRESENT_LINE,
		    "main help table row", error))
			return false;
	}
	if (!session_present_paged_line(session, (const uint8_t *)narrative[0],
	    strlen(narrative[0]), "main help narrative first", error))
		return false;
	for (index = 1; index < sizeof(narrative) / sizeof(narrative[0]);
	    ++index) {
		if (!session_present_paged_fragment(session, (const uint8_t *)narrative[index],
		    strlen(narrative[index])))
			return false;
	}
	return true;
}

bool
yt_session_quit(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t generating[] = "Generating ScoreBoard";
	static const uint8_t reminder[] =
	    "PLEASE HELP YOUR SYSOP REGISTER THIS GAME.";
	char returning[sizeof(session->door->identity.system) + 20U];
	int length;

	if (!session->door->game_open)
		return true;
	session_set_foreground(session, 1.0f);
	if (!yt_session_show_ship(session, error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "normal-exit post-Info blank", error)
	    || !session_present_timed_paged_row(session, generating, sizeof(generating) - 1U,
	    "normal-exit generating row", error)
	    || !yt_session_generate_scoreboard(session, error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "normal-exit post-generator blank", error))
		return false;
	session->pager.nonstop = true;
	if (!session_display_game_file(session,
	    session->door->game.config.scoreboard, error))
		return false;
	if (!session->registered) {
		if (!session_attention_bytes(session, reminder, sizeof(reminder) - 1U,
		    "normal-exit registration reminder", error)
		    || !session_wait(session, 10.0,
		    "normal-exit registration wait", error)
		    || !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "normal-exit reminder blank", error))
			return false;
	}
	length = snprintf(returning, sizeof(returning), "Returning to %s...",
	    session->door->identity.system);
	if (length < 0 || (size_t)length >= sizeof(returning)) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "normal-exit BBS row capacity");
		}
		return false;
	}
	return session_present_paged_fragment(session, (const uint8_t *)returning,
	    (size_t)length);
}

bool
yt_session_command_shell(struct yt_session *session, struct yt_error *error)
{
	while (session->running && !session->destroyed) {
		char command[YT_COMMAND_SIZE];
		uint8_t prompt[512];
		size_t prompt_length;
		size_t response_length;
		enum yt_main_shell_route route;
		bool enter_sector = false;

		session->pager.line_count = 0.0f;
		if (!session_reload_player(session, error))
			return false;
		session_set_foreground(session, 2.0f);
		if (!session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE, "main prompt leading blank", error))
			return false;
		if (!yt_main_prompt_row((const uint8_t *)session->time.text,
		    session->time.text_length, prompt, sizeof(prompt),
		    &prompt_length))
			return false;
		memcpy(session->io.text_workspace, prompt, prompt_length);
		session->io.text_workspace[prompt_length] = '\0';
		if (!session_present_timed_paged_row(session, prompt,
		    prompt_length, "main prompt low-time warning", error))
			return false;
		if (!session_read_upper_command(session, command,
		    sizeof(command)))
			return true;
		response_length = strlen(command);
		memcpy(session->io.text_workspace, command,
		    response_length + 1U);
		route = yt_main_shell_dispatch(command);
		switch (route) {
		case YT_MAIN_SHELL_DISPLAY:
			if (!session_present_paged_line(session,
			    (const uint8_t *)"<Display>",
			    strlen("<Display>"), "main display heading", error))
				return false;
			if (!yt_session_display_sector(session, false, error))
				return false;
			continue;
		case YT_MAIN_SHELL_SOUND:
		{
			struct yt_present_result presentation;
			enum yt_present_status status =
			    yt_present_sound_toggle(&session->presentation,
			    &presentation);

			yt_out_present_result(&presentation);
			if (status != YT_PRESENT_OK) {
				if (error != NULL) {
					error->status = YT_RANGE;
					snprintf(error->operation,
					    sizeof(error->operation),
					    "sound toggle");
				}
				return false;
			}
			if (!yt_session_display_sector(session, false, error))
				return false;
			continue;
		}
		case YT_MAIN_SHELL_SENSORS:
			if (!yt_session_display_sector(session, true, error))
				return false;
			continue;
		case YT_MAIN_SHELL_VERSION:
			session_set_foreground(session, 6.0f);
			if (!yt_session_registration(session, error))
				return false;
			if (!session->running)
				return true;
			continue;
		case YT_MAIN_SHELL_INFO:
			if (!yt_session_show_ship(session, error))
				return false;
			continue;
		case YT_MAIN_SHELL_INSTRUCTIONS:
			if (!session_present_paged_fragment(session,
			    (const uint8_t *)"<Instructions>",
			    strlen("<Instructions>"))
			    || !yt_session_instruction_offer(session, error))
				return false;
			continue;
		case YT_MAIN_SHELL_HELP:
			if (!show_help(session, error))
				return false;
			continue;
		case YT_MAIN_SHELL_INVALID:
			if (!session_present_alert(session,
			    (const uint8_t *)"Invalid command.",
			    strlen("Invalid command."),
			    "main invalid command", error))
				return false;
			continue;
		case YT_MAIN_SHELL_WARP:
			if (!yt_session_direct_emergency_warp(session, error))
				return false;
			enter_sector = true;
			break;
		case YT_MAIN_SHELL_MISSILE:
			if (!yt_session_command_projectile(session, false, error))
				return false;
			if (!session->destroyed
			    && !yt_session_display_sector(session, false, error))
				return false;
			break;
		case YT_MAIN_SHELL_PLASMA:
			if (!yt_session_command_projectile(session, true, error))
				return false;
			if (!session->destroyed
			    && !yt_session_display_sector(session, false, error))
				return false;
			break;
		case YT_MAIN_SHELL_ATTACK:
			if (!yt_session_command_attack(session, &enter_sector, error))
				return false;
			break;
		case YT_MAIN_SHELL_BUY_PORT:
			if (!yt_session_command_buy_port(session, error)
			    || !yt_session_display_current_sector_cached(session,
			    error))
				return false;
			break;
		case YT_MAIN_SHELL_COMPUTER:
			if (!yt_session_computer_menu(session, &enter_sector, error))
				return false;
			break;
		case YT_MAIN_SHELL_FIGHTERS:
			if (!yt_session_command_fighters(session, error))
				return false;
			break;
		case YT_MAIN_SHELL_LAND:
			if (!yt_session_command_land(session, &enter_sector, error))
				return false;
			break;
		case YT_MAIN_SHELL_MOVE:
			if (!yt_session_command_move(session, &enter_sector, error))
				return false;
			break;
		case YT_MAIN_SHELL_TRADE:
			if (!yt_session_command_trade(session, &enter_sector, error))
				return false;
			break;
		case YT_MAIN_SHELL_QUIT:
		{
			bool confirmed;

			if (!session_quit_confirm(session, &confirmed, error))
				return false;
			if (!confirmed)
				break;
			if (!yt_session_quit(session, error))
				return false;
			session->running = false;
			session->terminated = true;
			return false;
		}
		case YT_MAIN_SHELL_TEAM:
			if (!yt_session_command_team(session, error))
				return false;
			enter_sector = true;
			break;
		case YT_MAIN_SHELL_MINES:
			if (!yt_session_command_mines(session, error))
				return false;
			session_set_foreground(session, 1.0f);
			if (!yt_session_display_sector(session, false, error))
				return false;
			break;
		case YT_MAIN_SHELL_COLLECT:
			if (!yt_session_treasury(session, true, error))
				return false;
			enter_sector = true;
			break;
		case YT_MAIN_SHELL_GENESIS:
			if (!yt_session_command_genesis(session, error))
				return false;
			break;
		case YT_MAIN_SHELL_RENAME_PORT:
			if (!yt_session_command_rename_port(session, error)
			    || !yt_session_display_current_sector_cached(session,
			    error))
				return false;
			break;
		}
		if (enter_sector && session->running && !session->destroyed
		    && !yt_session_sector_entry(session, error))
			return false;
	}
	return true;
}
