#include "yt_session_internal.h"

#include <stdio.h>
#include <string.h>

static bool
danger_error(struct yt_error *error, const char *operation)
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
	if (!session_sound(session, 8.0f, "danger warning sound", error)
	    || !session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "danger leading blank", error))
		return false;
	yt_present_set_blink(&session->presentation, 1.0f);
	if (!session_present_text(session, warning, sizeof(warning) - 1U,
	    SESSION_PRESENT_BOLD_RAW, "danger warning header", error))
		return false;
	number_length = qb_str_single(number, sizeof(number), target);
	if (number_length < 0
	    || !danger_append(row, sizeof(row), &length, number,
	    (size_t)number_length)
	    || !danger_append(row, sizeof(row), &length, suffix,
	    sizeof(suffix) - 1U))
		return danger_error(error, "danger warning target row");
	return session_present_text(session, row, length,
	    SESSION_PRESENT_BOLD_LINE, "danger warning target", error)
	    && session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
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
	float saved_foreground;
	bool finding = false;
	uint8_t row[256];
	char number[80];
	size_t row_length;
	bool overflow;

	if (session == NULL || dangerous == NULL)
		return false;
	*dangerous = false;
	if (target < 1.0f || target > (float)session_sector_count(session))
		return true;
	saved_foreground = session->foreground;
	session_set_foreground(session, 3.0f);
	yt_present_set_background(&session->presentation, 4.0f);
	if (!session_read_sector(session, (int)target, &sector, error))
		return false;
	if (target == session->disruption_sectors[0]
	    || target == session->disruption_sectors[1]) {
		if (!danger_first_warning(session, target, finding, error)
		    || !session_present_text(session, disruption,
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
		if (number_length < 0
		    || !danger_append(row, sizeof(row), &row_length, mine_prefix,
		    sizeof(mine_prefix) - 1U)
		    || !danger_append(row, sizeof(row), &row_length, number,
		    (size_t)number_length)
		    || !danger_append(row, sizeof(row), &row_length, mine_suffix,
		    sizeof(mine_suffix) - 1U))
			return danger_error(error, "danger mines row");
		if (!session_present_text(session, row, row_length,
		    SESSION_PRESENT_BOLD_LINE, "danger mines row", error))
			return false;
		finding = true;
	}
	if (sector.fighters > 0.0f) {
		float owner = sector.fighter_owner;
		bool hostile;
		int number_length = qb_str_double(number, sizeof(number),
		    (double)sector.fighters);

		row_length = 0U;
		if (number_length < 0
		    || !danger_append(row, sizeof(row), &row_length,
		    fighter_prefix, sizeof(fighter_prefix) - 1U)
		    || !danger_append(row, sizeof(row), &row_length, number,
		    (size_t)number_length)
		    || !danger_append(row, sizeof(row), &row_length,
		    fighter_middle, sizeof(fighter_middle) - 1U))
			return danger_error(error, "danger fighters row");
		if (owner == -1.0f) {
			if (!danger_append(row, sizeof(row), &row_length, xannor,
			    sizeof(xannor) - 1U))
				return danger_error(error, "danger fighters row");
		}
		else if (owner == -2.0f) {
			if (!danger_append(row, sizeof(row), &row_length,
			    mercenaries, sizeof(mercenaries) - 1U))
				return danger_error(error, "danger fighters row");
		}
		else {
			struct yt_player owner_player;
			int name_length;

			if (!yt_game_read_player(&session->door->game, (int)owner,
			    &owner_player, error))
				return false;
			name_length = qb_cint_mbf32(
			    owner_player.record.bytes + YT_F85, 0U, &overflow);
			if (overflow || name_length < 0)
				return danger_error(error,
				    "danger owner name length");
			if ((size_t)name_length > YT_TEXT_FIELD_SIZE)
				name_length = (int)YT_TEXT_FIELD_SIZE;
			if (!danger_append(row, sizeof(row), &row_length,
			    owner_player.record.bytes, (size_t)name_length))
				return danger_error(error, "danger owner name row");
			if (owner_player.team != 0.0f) {
				struct yt_sector team;
				bool friendly;
				int team_name_length;

				session->shared_status = 0.0f;
				if (!yt_session_players_are_friendly(session,
				    (int)owner, &friendly, error))
					return false;
				session->shared_status = friendly ? -1.0f : 0.0f;
				number_length = qb_str_single(number, sizeof(number),
				    owner_player.team);
				if (number_length < 1
				    || !danger_append(row, sizeof(row), &row_length,
				    team_prefix, sizeof(team_prefix) - 1U)
				    || !danger_append(row, sizeof(row), &row_length,
				    number + 1, (size_t)number_length - 1U)
				    || !danger_append(row, sizeof(row), &row_length,
				    closing_bracket, sizeof(closing_bracket) - 1U))
					return danger_error(error,
					    "danger team number row");
				if (!session_read_sector(session, (int)owner_player.team,
				    &team, error))
					return false;
				team_name_length = qb_cint_mbf32(
				    team.record.bytes + YT_F73, 0U, &overflow);
				if (overflow || team_name_length < 0)
					return danger_error(error,
					    "danger team name length");
				if (team_name_length > 0) {
					size_t amount = (size_t)team_name_length;

					if (amount > YT_TEXT_FIELD_SIZE)
						amount = YT_TEXT_FIELD_SIZE;
					if (!danger_append(row, sizeof(row), &row_length,
					    team_name_prefix,
					    sizeof(team_name_prefix) - 1U)
					    || !danger_append(row, sizeof(row), &row_length,
					    team.record.bytes, amount)
					    || !danger_append(row, sizeof(row), &row_length,
					    closing_bracket,
					    sizeof(closing_bracket) - 1U))
						return danger_error(error,
						    "danger team name row");
				}
			}
		}
		hostile = owner < 0.0f;
		if (!hostile && owner > 1.0f
		    && owner <= session_sector_offset(session)
		    && owner != (float)session_record(session))
			hostile = session->shared_status != -1.0f;
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
		yt_present_set_blink(&session->presentation, 1.0f);
		if (!session_present_text(session, deactivated,
		    sizeof(deactivated) - 1U, SESSION_PRESENT_BOLD_LINE,
		    "danger deactivation row", error))
			return false;
	}
	session_set_foreground(session, saved_foreground);
	yt_present_set_background(&session->presentation, 0.0f);
	*dangerous = finding;
	return true;
}
