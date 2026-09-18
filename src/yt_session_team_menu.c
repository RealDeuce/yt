#include "yt_session_team_internal.h"

#include "qb.h"

#include <stdio.h>
#include <string.h>

bool
yt_session_command_team(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t exit_row[] = "1) Exit Team menu";
	static const char *const teamless_rows[] = {
		"2) Create a Team",
		"3) Join a Team",
	};
	static const char *const member_rows[] = {
		"4) Quit a Team",
		"5) Search for Team Members & Resources",
		"6) Transfer Fighters to Defense Force",
	};
	static const char *const captain_rows[] = {
		"7) Banish a Team Member",
		"8) Change Team Password",
		"9) Change Team Name",
	};
	static const uint8_t prompt_prefix[] = "Time:";
	static const uint8_t prompt_body[] = "Team Command? ";
	static const uint8_t invalid_row[] = "Invalid Choice!";

	for (;;) {
		char line[80];
		struct yt_team team;
		bool captain = false;
		struct qb_val_result parsed;
		enum qb_mbf_status conversion;
		uint8_t prompt[sizeof(prompt_prefix) - 1U
		    + sizeof(session->time.text) + sizeof(prompt_body) - 1U];
		size_t prompt_length = 0;
		size_t index;
		float numeric;
		int32_t captain_cint;
		bool invalid;

		session_set_foreground(session, 6);
		if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
		    "team front leading blank", error))
			return false;
		if (!yt_session_info_team_lines(session, &team, &captain, error))
			return false;
		session->pager.line_count = 0;
		if (!session_reload_player(session, error))
			return false;
		if (!session_present_paged_line(session, exit_row,
		    sizeof(exit_row) - 1U, "team exit row", error))
			return false;
		if (!session_reload_player(session, error))
			return false;
		if (session->player.team == 0) {
			for (index = 0; index < YT_ARRAY_LEN(teamless_rows); ++index)
				if (!session_present_paged_fragment(session,
				    (const uint8_t *)teamless_rows[index],
				    strlen(teamless_rows[index])))
					return false;
		}
		else {
			for (index = 0; index < YT_ARRAY_LEN(member_rows); ++index)
				if (!session_present_paged_fragment(session,
				    (const uint8_t *)member_rows[index],
				    strlen(member_rows[index])))
					return false;
			if (captain)
				for (index = 0; index < YT_ARRAY_LEN(captain_rows);
				    ++index)
					if (!session_present_paged_fragment(session,
					    (const uint8_t *)captain_rows[index],
					    strlen(captain_rows[index])))
						return false;
		}
		session_set_foreground(session, 6);
		if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
		    "team prompt blank", error))
			return false;
		memcpy(prompt + prompt_length, prompt_prefix,
		    sizeof(prompt_prefix) - 1U);
		prompt_length += sizeof(prompt_prefix) - 1U;
		if (session->time.text_length > sizeof(session->time.text)) {
			if (error != NULL) {
				error->status = YT_RANGE;
				(void)snprintf(error->operation,
				    sizeof(error->operation), "%s",
				    "team prompt time capacity");
			}
			return false;
		}
		memcpy(prompt + prompt_length, session->time.text,
		    session->time.text_length);
		prompt_length += session->time.text_length;
		memcpy(prompt + prompt_length, prompt_body,
		    sizeof(prompt_body) - 1U);
		prompt_length += sizeof(prompt_body) - 1U;
		if (!session_present_timed_paged_row(session, prompt, prompt_length,
		    "team command prompt", error))
			return false;
		if (!session_read_command(session, line, sizeof(line)))
			return false;
		parsed = qb_val(line);
		if (!parsed.valid || parsed.overflow) {
			if (error != NULL) {
				error->status = YT_RANGE;
				(void)snprintf(error->operation,
				    sizeof(error->operation), "%s", "team:VAL");
			}
			return false;
		}
		conversion = qb_val_single_or_zero(&parsed, &numeric);
		if (conversion == QB_MBF_OVERFLOW) {
			if (error != NULL) {
				error->status = YT_RANGE;
				(void)snprintf(error->operation,
				    sizeof(error->operation), "%s", "team:choice-csng");
			}
			return false;
		}
		captain_cint = captain ? -1 : 0;
		invalid = yt_team_choice_rejected(numeric, session->player.team,
		    captain_cint);
		if (invalid) {
			if (!session_present_alert(session, invalid_row,
			    sizeof(invalid_row) - 1U, "team invalid choice", error))
				return false;
			continue;
		}
		if (strcmp(line, "1") == 0)
			return true;
		if (strcmp(line, "2") == 0) {
			if (!session_team_create(session, error))
				return false;
		}
		else if (strcmp(line, "3") == 0) {
			if (!session_team_join(session, error))
				return false;
		}
		else if (strcmp(line, "4") == 0) {
			if (!session_team_quit(session, &team, error))
				return false;
		}
		else if (strcmp(line, "5") == 0) {
			if (!session_team_search(session, error))
				return false;
		}
		else if (strcmp(line, "6") == 0) {
			if (!session_team_transfer(session, error))
				return false;
		}
		else if (strcmp(line, "7") == 0) {
			if (!session_team_banish(session, &team, error))
				return false;
		}
		else if (strcmp(line, "8") == 0) {
			if (!session_team_create_password(session, team.id,
			    team.password, error))
				return false;
		}
		else if (strcmp(line, "9") == 0) {
			char name[42];
			bool accepted;

			if (!session_team_pick_name(session, team.id, name,
			    &accepted, error))
				return false;
			(void)accepted;
		}
	}
}
