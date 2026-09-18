#include "yt_session_internal.h"
#include "qb.h"

#include <stdio.h>
#include <string.h>

static bool
movement_range_error(struct yt_error *error, const char *operation)
{
	if (error != NULL) {
		error->status = YT_RANGE;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
	}
	return false;
}

static bool
danger_append(uint8_t *row, size_t capacity, size_t *length,
    const void *data, size_t data_length)
{
	if (*length > capacity || data_length > capacity - *length)
		return false;
	if (data_length != 0U)
		memcpy(row + *length, data, data_length);
	*length += data_length;
	return true;
}

static bool
danger_first_warning(struct yt_session *session, float target, bool finding,
    struct yt_error *error)
{
	static const uint8_t warning[] = "*** WARNING! ***";
	static const uint8_t suffix[] =
	    " Danger Scanner has detected the following in sector!";
	uint8_t row[160];
	char number[64];
	int number_length;
	size_t length = 0U;

	if (finding)
		return true;
	if (!session_sound(session, YT_SOUND_CUE_DANGER,
	    "danger warning sound", error))
		return false;
	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "danger leading blank", error))
		return false;
	session->presentation.blink = true;
	if (!session_present_text(session, warning, sizeof(warning) - 1U,
	    SESSION_PRESENT_BOLD_RAW, "danger warning header", error))
		return false;
	number_length = qb_str_single(number, sizeof(number), target);
	if (number_length < 0)
		return movement_range_error(error, "danger warning target row");
	if (!danger_append(row, sizeof(row), &length, number,
	    (size_t)number_length))
		return movement_range_error(error, "danger warning target row");
	if (!danger_append(row, sizeof(row), &length, suffix,
	    sizeof(suffix) - 1U))
		return movement_range_error(error, "danger warning target row");
	if (!session_present_text(session, row, length,
	    SESSION_PRESENT_BOLD_LINE, "danger warning target", error))
		return false;
	return session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "danger warning blank", error);
}

bool
yt_session_destination_is_dangerous(struct yt_session *session, float target,
    bool *dangerous, struct yt_error *error)
{
	static const uint8_t disruption[] = "** Space-time disruption! **";
	static const uint8_t mine_prefix[] = "**";
	static const uint8_t mine_suffix[] = " SECTOR MINES! **";
	static const uint8_t fighter_prefix[] = "***";
	static const uint8_t fighter_middle[] = " Fighters Belonging to ";
	static const uint8_t xannor[] = "The Xannor";
	static const uint8_t mercenaries[] = "Mercenaries";
	static const uint8_t team_prefix[] = " * Team [";
	static const uint8_t team_name_prefix[] = " [";
	static const uint8_t closing_bracket[] = "]";
	static const uint8_t deactivated[] =
	    "*** WARP DRIVE DEACTIVATED ***";
	struct yt_sector sector;
	int saved_foreground;
	bool finding = false;
	uint8_t row[256];
	char number[80];
	size_t row_length;

	if (session == NULL || dangerous == NULL)
		return false;
	*dangerous = false;
	if (target < 1.0f || target > (float)session_sector_count(session))
		return true;
	saved_foreground = session->presentation.foreground;
	session_set_foreground(session, 3);
	session->presentation.background = 4;
	if (!session_read_sector(session, (int)target, &sector, error))
		return false;
	if (target == (float)session->disruption_sectors[0]
	    || target == (float)session->disruption_sectors[1]) {
		if (!danger_first_warning(session, target, finding, error))
			return false;
		if (!session_present_text(session, disruption,
		    sizeof(disruption) - 1U, SESSION_PRESENT_BOLD_LINE,
		    "danger disruption row", error))
			return false;
		finding = true;
	}
	if (sector.mines != 0.0f) {
		int number_length;

		if (!danger_first_warning(session, target, finding, error))
			return false;
		number_length = qb_str_single(number, sizeof(number), sector.mines);
		row_length = 0U;
		if (number_length < 0)
			return movement_range_error(error, "danger mines row");
		if (!danger_append(row, sizeof(row), &row_length, mine_prefix,
		    sizeof(mine_prefix) - 1U))
			return movement_range_error(error, "danger mines row");
		if (!danger_append(row, sizeof(row), &row_length, number,
		    (size_t)number_length))
			return movement_range_error(error, "danger mines row");
		if (!danger_append(row, sizeof(row), &row_length, mine_suffix,
		    sizeof(mine_suffix) - 1U))
			return movement_range_error(error, "danger mines row");
		if (!session_present_text(session, row, row_length,
		    SESSION_PRESENT_BOLD_LINE, "danger mines row", error))
			return false;
		finding = true;
	}
	if (sector.fighters > 0.0f) {
		int owner = sector.fighter_owner;
		bool hostile;
		int number_length = qb_str_double(number, sizeof(number),
		    (double)sector.fighters);

		row_length = 0U;
		if (number_length < 0)
			return movement_range_error(error, "danger fighters row");
		if (!danger_append(row, sizeof(row), &row_length,
		    fighter_prefix, sizeof(fighter_prefix) - 1U))
			return movement_range_error(error, "danger fighters row");
		if (!danger_append(row, sizeof(row), &row_length, number,
		    (size_t)number_length))
			return movement_range_error(error, "danger fighters row");
		if (!danger_append(row, sizeof(row), &row_length,
		    fighter_middle, sizeof(fighter_middle) - 1U))
			return movement_range_error(error, "danger fighters row");
		if (owner == -1) {
			if (!danger_append(row, sizeof(row), &row_length, xannor,
			    sizeof(xannor) - 1U))
				return movement_range_error(error, "danger fighters row");
		}
		else if (owner == -2) {
			if (!danger_append(row, sizeof(row), &row_length,
			    mercenaries, sizeof(mercenaries) - 1U))
				return movement_range_error(error, "danger fighters row");
		}
		else {
			struct yt_player owner_player;
			size_t name_length;

			if (!yt_game_read_player(&session->door->game, owner,
			    &owner_player, error))
				return false;
			name_length = owner_player.name_length;
			if (name_length > YT_TEXT_FIELD_SIZE)
				name_length = YT_TEXT_FIELD_SIZE;
			if (!danger_append(row, sizeof(row), &row_length,
			    owner_player.record.bytes, name_length))
				return movement_range_error(error, "danger owner name row");
			if (owner_player.team != 0) {
				struct yt_team team;
				bool friendly;

				session->player_reference.friendly = false;
				if (!yt_session_players_are_friendly(session,
				    owner, &friendly, error))
					return false;
				session->player_reference.friendly = friendly;
				number_length = qb_str_single(number, sizeof(number),
				    (float)owner_player.team);
				if (number_length < 1)
					return movement_range_error(error,
					    "danger team number row");
				if (!danger_append(row, sizeof(row), &row_length,
				    team_prefix, sizeof(team_prefix) - 1U))
					return movement_range_error(error,
					    "danger team number row");
				if (!danger_append(row, sizeof(row), &row_length,
				    number + 1, (size_t)number_length - 1U))
					return movement_range_error(error,
					    "danger team number row");
				if (!danger_append(row, sizeof(row), &row_length,
				    closing_bracket, sizeof(closing_bracket) - 1U))
					return movement_range_error(error,
					    "danger team number row");
				if (!yt_game_read_team(&session->door->game,
				    owner_player.team, &team, error))
					return false;
				if (team.name_length > 0U) {
					if (!danger_append(row, sizeof(row), &row_length,
					    team_name_prefix,
					    sizeof(team_name_prefix) - 1U))
						return movement_range_error(error,
						    "danger team name row");
					if (!danger_append(row, sizeof(row), &row_length,
					    team.name, team.name_length))
						return movement_range_error(error,
						    "danger team name row");
					if (!danger_append(row, sizeof(row), &row_length,
					    closing_bracket,
					    sizeof(closing_bracket) - 1U))
						return movement_range_error(error,
						    "danger team name row");
				}
			}
		}
		hostile = owner < 0;
		if (!hostile && owner > 1
		    && owner <= session_sector_offset(session)
		    && owner != session_record(session))
			hostile = !session->player_reference.friendly;
		if (hostile) {
			if (!danger_first_warning(session, target, finding, error))
				return false;
			finding = true;
			if (!session_present_text(session, row, row_length,
			    SESSION_PRESENT_BOLD_LINE, "danger fighters row", error))
				return false;
		}
	}
	if (!session_reload_player(session, error))
		return false;
	if (finding) {
		if (!session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE, "danger final blank", error))
			return false;
		session->presentation.blink = true;
		if (!session_present_text(session, deactivated,
		    sizeof(deactivated) - 1U, SESSION_PRESENT_BOLD_LINE,
		    "danger deactivation row", error))
			return false;
	}
	session_set_foreground(session, saved_foreground);
	session->presentation.background = 0;
	*dangerous = finding;
	return true;
}

bool
yt_session_store_move(struct yt_session *session, int target,
    struct yt_error *error)
{
	int player_record;

	if (session == NULL)
		return false;
	player_record = session_record(session);
	session->navigation.self_mines_suppressed = false;
	if (!session_reload_player(session, error))
		return false;
	yt_movement_player_overlay(&session->player, target);
	if (!yt_database_write(&session->door->game.database,
	    (size_t)player_record, &session->player.record, error))
		return false;
	if (!yt_database_flush(&session->door->game.database, error))
		return false;
	return yt_player_cache_set_sector(&session->player_cache, player_record,
	    target);
}

bool
yt_session_command_move(struct yt_session *session, bool *moved,
    struct yt_error *error)
{
	static const uint8_t prompt[] = "Move to which sector? ";
	static const uint8_t same_sector[] =
	    "That was quick! Felt like we didn't even move!";
	static const uint8_t not_adjacent[] =
	    "You can't get there from here.";
	uint8_t row[256];
	char response[YT_COMMAND_SIZE];
	struct qb_val_result parsed;
	float maximum;
	float target;
	uint8_t target_raw[4];
	size_t row_length;
	size_t slot;
	bool denied;
	bool adjacent = false;

	if (session == NULL || moved == NULL)
		return false;
	*moved = false;
	if (!yt_session_fresh_no_turn_gate(session, &denied, error))
		return false;
	if (denied)
		return true;
	if (!yt_movement_warp_row(session->navigation.current_warps, row, sizeof(row),
	    &row_length))
		return movement_range_error(error, "movement warp row");
	if (!session_present_paged_line(session, row, row_length,
	    "movement warp row", error))
		return false;
	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "movement post-warp blank", error))
		return false;
	for (;;) {
		if (!session_present_timed_paged_row(session, prompt,
		    sizeof(prompt) - 1U, "movement destination prompt", error))
			return false;
		memset(response, 0, sizeof(response));
		if (!session_read_number_command(session, response,
		    sizeof(response)))
			return false;
		if (strcmp(response, "M") != 0)
			break;
	}
	parsed = qb_val(response);
	if (parsed.overflow)
		return movement_range_error(error, "movement destination VAL");
	target = (float)(parsed.valid ? parsed.value : 0.0);
	if (qb_mbf32_encode(target, target_raw) == QB_MBF_OVERFLOW)
		return movement_range_error(error, "movement destination CSNG");
	target = qb_mbf32_decode(target_raw);
	maximum = qb_single_subtract((float)session_port_offset(session),
	    (float)session_sector_offset(session));
	if (target < 1.0f || target > maximum)
		return true;
	if (target == (float)session->player.sector)
		return session_present_alert(session, same_sector,
		    sizeof(same_sector) - 1U, "movement same-sector row", error);
	for (slot = 0U; slot < YT_ARRAY_LEN(session->navigation.current_warps); ++slot) {
		if ((float)session->navigation.current_warps[slot] == target) {
			adjacent = true;
			break;
		}
	}
	if (!adjacent)
		return session_present_alert(session, not_adjacent,
		    sizeof(not_adjacent) - 1U, "movement not-adjacent row", error);
	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "movement accepted blank", error))
		return false;
	if (session->player.danger_scanner != 0) {
		bool dangerous;

		if (!yt_session_destination_is_dangerous(session, target,
		    &dangerous, error))
			return false;
		if (dangerous) {
			enum yt_yes_no_answer answer;

			if (!session_present_text(session, NULL, 0U,
			    SESSION_PRESENT_LINE, "danger confirmation blank", error))
				return false;
			session_clear_queue(session);
			if (!yt_movement_confirmation_prompt(target, row,
			    sizeof(row), &row_length))
				return movement_range_error(error,
				    "movement confirmation prompt");
			if (!session_confirm(session, row, row_length, &answer,
			    error))
				return false;
			if (answer != YT_YES_NO_YES)
				return true;
		}
	}
	if (!yt_session_finalize_action(session, error)) {
		if (error != NULL && error->status != YT_OK)
			return false;
		return true;
	}
	if (!yt_session_store_move(session, (int)target, error))
		return false;
	*moved = true;
	return true;
}
