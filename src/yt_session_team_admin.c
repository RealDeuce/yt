#include "yt_session_team_internal.h"

#include "qb.h"

#include <stdio.h>
#include <string.h>

bool
session_team_transfer(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t no_defense[] =
	    "There IS no defense force here!";
	static const uint8_t prompt[] =
	    "How many fighters do you wish to transfer? ";
	static const uint8_t success[] = "Fighters transferred!";
	struct yt_sector initial_sector;
	double initial_fighters;
	double initial_defense;
	int logical_sector;

	if (!session_reload_player(session, error))
		return false;
	logical_sector = session->player.sector;
	initial_fighters = (double)session->player.fighters;
	if (!session_read_sector(session, logical_sector,
	    &initial_sector, error))
		return false;
	initial_defense = (double)initial_sector.fighters;
	if (initial_defense == 0.0)
		return session_present_alert(session, no_defense,
		    sizeof(no_defense) - 1U, "team transfer no defense", error);
	for (;;) {
		char fighter_text[64];
		char defense_text[64];
		char row[160];
		char response[160];
		struct qb_val_result parsed;
		enum qb_mbf_status conversion;
		uint8_t amount_raw[4];
		float amount;

		if (qb_str_double(fighter_text, sizeof(fighter_text),
		    initial_fighters) < 0)
			return false;
		if (qb_str_double(defense_text, sizeof(defense_text),
		    initial_defense) < 0)
			return false;
		if (snprintf(row, sizeof(row), "You have%s fighters.",
		    fighter_text) < 0)
			return false;
		if (!session_present_paged_line(session, (const uint8_t *)row,
		    strlen(row), "team transfer carried row", error))
			return false;
		if (snprintf(row, sizeof(row), "There are%s fighters here.",
		    defense_text) < 0)
			return false;
		if (!session_present_paged_line(session, (const uint8_t *)row,
		    strlen(row), "team transfer deployed row", error))
			return false;
		if (!session_present_timed_paged_row(session, prompt,
		    sizeof(prompt) - 1U, "team transfer prompt", error))
			return false;
		if (!session_read_number_command(session, response, sizeof(response)))
			return false;
		parsed = qb_val(response);
		if (parsed.overflow) {
			if (error != NULL) {
				error->status = YT_RANGE;
				(void)snprintf(error->operation,
				    sizeof(error->operation), "%s", "team transfer:VAL");
			}
			return false;
		}
		amount = parsed.valid ? (float)parsed.value : 0.0f;
		conversion = qb_mbf32_encode(amount, amount_raw);
		if (conversion == QB_MBF_OVERFLOW) {
			if (error != NULL) {
				error->status = YT_RANGE;
				(void)snprintf(error->operation,
				    sizeof(error->operation), "%s",
				    "team transfer:amount-csng");
			}
			return false;
		}
		amount = qb_mbf32_decode(amount_raw);
		if (amount < 1.0f)
			return true;
		if ((double)amount > initial_fighters) {
			if (snprintf(row, sizeof(row), "You only have%s!",
			    fighter_text) < 0)
				return false;
			if (!session_present_alert(session, (const uint8_t *)row,
			    strlen(row), "team transfer too many", error))
				return false;
			continue;
		}
		{
			struct yt_sector fresh_sector;

			if (!session_read_sector(session,
			    logical_sector, &fresh_sector, error))
				return false;
			yt_team_transfer_apply_sector(&fresh_sector,
			    initial_defense, amount);
			if (!yt_database_write(&session->door->game.database,
			    (size_t)session_sector_basic_record(session,
			    logical_sector),
			    &fresh_sector.record, error))
				return false;
		}
		if (!session_reload_player(session, error))
			return false;
		yt_team_transfer_apply_player(&session->player, amount);
		if (!yt_database_write(&session->door->game.database,
		    (size_t)session_record(session), &session->player.record, error))
			return false;
		return session_present_alert(session, success, sizeof(success) - 1U,
		    "team transfer success", error);
	}
}

bool
session_team_banish(struct yt_session *session, struct yt_team *team,
    struct yt_error *error)
{
	static const uint8_t prompt_prefix[] = "Banish ";
	static const uint8_t prompt_suffix[] = " (Y/[N])? ";
	static const uint8_t end[] = "End of List";
	static const uint8_t success[] =
	    "Done. Now change your Team Password!";
	int team_id;
	size_t index;

	if (!session_reload_player(session, error))
		return false;
	team_id = session->player.team;
	if (!session_load_team(session, team_id, team, error))
		return false;
	for (index = 0; index < 4; ++index) {
		struct yt_player member;
		enum yt_yes_no_answer answer;
		uint8_t prompt[sizeof(prompt_prefix) - 1U + YT_TEXT_FIELD_SIZE
		    + sizeof(prompt_suffix) - 1U];
		size_t name_length;
		size_t prompt_length = 0;
		int member_record;

		if (team->roster[index] <= 0
		    || team->roster[index] == session_record(session))
			continue;
		member_record = team->roster[index];
		if (!yt_game_read_player(&session->door->game,
		    member_record, &member, error))
			return false;
		memcpy(prompt + prompt_length, prompt_prefix,
		    sizeof(prompt_prefix) - 1U);
		prompt_length += sizeof(prompt_prefix) - 1U;
		name_length = yt_player_stored_name(&member,
		    prompt + prompt_length);
		prompt_length += name_length;
		memcpy(prompt + prompt_length, prompt_suffix,
		    sizeof(prompt_suffix) - 1U);
		prompt_length += sizeof(prompt_suffix) - 1U;
		if (!session_confirm(session, prompt, prompt_length, &answer, error))
			return false;
		if (answer != YT_YES_NO_YES)
			continue;
		if (!yt_game_read_player(&session->door->game,
		    member_record, &member, error))
			return false;
		yt_team_banish_apply_player(&member);
		if (!yt_database_write(&session->door->game.database,
		    (size_t)member_record, &member.record, error))
			return false;
		team->roster[index] = 0;
		session->team_cache.roster[index] = 0;
		if (!session_read_sector(session, team_id,
		    &team->overlay, error))
			return false;
		if (!session_team_store_roster(session, team, error))
			return false;
		return session_present_alert(session, success, sizeof(success) - 1U,
		    "team banish success", error);
	}
	return session_present_paged_fragment(session, end, sizeof(end) - 1U);
}
