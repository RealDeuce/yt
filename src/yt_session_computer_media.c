#include "yt_session_internal.h"

#include "yt_score.h"

#include <string.h>
bool
yt_session_generate_scoreboard(struct yt_session *session,
    struct yt_error *error)
{
	static const uint8_t dot[] = ".";
	struct yt_scoreboard scoreboard;

	if (!yt_scoreboard_prepare(&scoreboard, &session->door->game,
	    session_sector_offset(session), session_port_offset(session), error)
	    || !session_present_text(session, dot, sizeof(dot) - 1U,
	    SESSION_PRESENT_RAW, "scoreboard progress dot", error)
	    || !yt_scoreboard_load_players(&scoreboard, error)
	    || !session_present_text(session, dot, sizeof(dot) - 1U,
	    SESSION_PRESENT_RAW, "scoreboard progress dot", error)
	    || !yt_scoreboard_score_sectors(&scoreboard, error)
	    || !session_present_text(session, dot, sizeof(dot) - 1U,
	    SESSION_PRESENT_RAW, "scoreboard progress dot", error))
		return false;
	yt_scoreboard_rank_players(&scoreboard);
	return session_present_text(session, dot, sizeof(dot) - 1U,
	    SESSION_PRESENT_RAW, "scoreboard progress dot", error)
	    && yt_scoreboard_write(&scoreboard, error);
}

bool
yt_session_computer_scoreboard(struct yt_session *session,
    struct yt_error *error)
{
	static const uint8_t prompt[] =
	    "Enter 'O' to see OLD scoreboard or press [ENTER] for UPDATED one. -=>";
	static const uint8_t heading[] = "P l a y e r  R a n k i n g s";
	char response[80];
	size_t length;

	session->pager.key[0] = '\0';
	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "scoreboard selector leading blank", error)
	    || !session_present_timed_paged_row(session, prompt,
	    sizeof(prompt) - 1U, "scoreboard selector prompt", error)
	    || !session_read_command(session, response, sizeof(response)))
		return false;
	length = strlen(response);
	yt_input_compat_upper_n((uint8_t *)session->io.text_workspace, length);
	yt_input_compat_upper_n((uint8_t *)response, length);
	session_set_pager_line_count(session, 0);
	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "scoreboard selector trailing blank", error))
		return false;
	if (!(length == 1U && response[0] == 'O')) {
		if (!session_present_timed_paged_row(session, heading,
		    sizeof(heading) - 1U, "scoreboard update heading", error)
		    || !yt_session_generate_scoreboard(session, error)
		    || !session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE, "scoreboard post-generator blank",
		    error))
			return false;
	}
	return session_display_game_file(session,
	    session->door->game.config.scoreboard, error);
}

bool
yt_session_computer_newspaper(struct yt_session *session,
    struct yt_error *error)
{
	static const uint8_t prompt[] =
	    "Do you want to read [T]oday's or [Y]esterday's news? [T/Y] -=> ";
	char response[80];
	enum yt_computer_newspaper_choice choice;

	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "newspaper selector leading blank", error))
		return false;
	do {
		if (!session_present_timed_paged_row(session, prompt,
		    sizeof(prompt) - 1U, "newspaper selector prompt", error)
		    || !session_read_command(session, response, sizeof(response)))
			return false;
		yt_input_compat_upper_n((uint8_t *)session->io.text_workspace,
		    strlen(response));
		yt_input_compat_upper_n((uint8_t *)response, strlen(response));
		choice = yt_computer_newspaper_select(response);
	} while (choice == YT_COMPUTER_NEWSPAPER_NONE);
	return session_display_game_file(session,
	    choice == YT_COMPUTER_NEWSPAPER_TODAY
	    ? "YTNEWS.DAT" : "YTYNEWS.DAT", error);
}
