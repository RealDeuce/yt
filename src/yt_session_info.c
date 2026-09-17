#include "yt_session_internal.h"

#include "qb.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static bool
info_failure(struct yt_error *error, const char *operation)
{
	if (error != NULL) {
		error->status = YT_RANGE;
		snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
	}
	return false;
}

static bool
info_refresh_time(struct yt_session *session, struct yt_error *error)
{
	DWORD elapsed_seconds;
	WORD elapsed_milliseconds;
	float remaining_seconds;
	enum yt_present_status status;

	od_get_time(&elapsed_seconds, &elapsed_milliseconds);
	remaining_seconds = (float)od_control.user_timelimit * 60.0f
	    - (float)(elapsed_seconds % 60U)
	    - (float)elapsed_milliseconds / 1000.0f;
	status = yt_present_format_remaining_seconds(&session->time,
	    remaining_seconds);
	if (status != YT_PRESENT_OK)
		return info_failure(error, "Info time refresh");
	return true;
}

static bool
info_line(struct yt_session *session, const void *text, size_t length,
    struct yt_error *error)
{
	return session_present_text(session, text, length, SESSION_PRESENT_LINE,
	    "Info line presentation", error);
}

static void
info_team_result(struct yt_session *session,
    const struct yt_player *current_player, const struct yt_team *team,
    bool is_captain, struct yt_team *resolved_team,
    bool *current_is_captain)
{
	session->player.record = current_player->record;
	session->player.team = current_player->team;
	if (resolved_team != NULL)
		*resolved_team = *team;
	if (current_is_captain != NULL)
		*current_is_captain = is_captain;
}

bool
yt_session_info_team_lines(struct yt_session *session,
    struct yt_team *resolved_team,
    bool *current_is_captain, struct yt_error *error)
{
	static const uint8_t none[] = "Team  : None";
	static const uint8_t promoted[] =
	    "Your team has no captain! You've been promoted to Captain!";
	static const uint8_t congratulations[] =
	    "Congratulations Captain! See Team Menu for your new options!";
	struct yt_player current_player;
	struct yt_player captain;
	struct yt_player ignored;
	struct yt_team team;
	struct yt_sector fresh;
	uint8_t captain_name[YT_TEXT_FIELD_SIZE];
	uint8_t row[256];
	size_t captain_name_length = 0U;
	size_t row_length;
	int current_record = session_record(session);
	int team_id;
	int captain_record;
	bool valid_captain = false;

	memset(&team, 0, sizeof(team));
	if (!yt_game_read_player(&session->door->game, current_record,
	    &current_player, error))
		return false;
	team_id = current_player.team;
	if (team_id == 0) {
		if (!info_line(session, none, sizeof(none) - 1U, error))
			return false;
		if (!info_line(session, NULL, 0U, error))
			return false;
		info_team_result(session, &current_player, &team, false,
		    resolved_team, current_is_captain);
		return true;
	}
	if (!session_load_team(session, team_id, &team, error))
		return false;
	if (!yt_info_team_row(YT_INFO_TEAM_SUMMARY, team_id,
	    (const uint8_t *)team.name, team.name_length, row, sizeof(row),
	    &row_length))
		return false;
	if (!info_line(session, row, row_length, error))
		return false;
	if (!info_line(session, NULL, 0U, error))
		return false;
	if (team.captain == current_record) {
		if (!yt_info_team_row(YT_INFO_TEAM_SELF_CAPTAIN, team_id, NULL,
		    0U, row, sizeof(row), &row_length))
			return info_failure(error, "Info team row");
		if (!info_line(session, row, row_length, error))
			return false;
		if (!info_line(session, NULL, 0U, error))
			return false;
		info_team_result(session, &current_player, &team, true,
		    resolved_team, current_is_captain);
		return true;
	}
	captain_record = team.captain;
	session->player_reference.record = captain_record;
	if (captain_record >= 2
	    && captain_record <= session_sector_offset(session)) {
		if (!yt_game_read_player(&session->door->game, captain_record,
		    &captain, error))
			return false;
		if (captain.name_length > 0U) {
			captain_name_length = yt_player_stored_name(&captain,
			    captain_name);
			valid_captain = captain.team == team_id;
		}
	}
	if (!valid_captain) {
		session->player_reference.record = current_record;
		session->team_cache.captain = current_record;
		session->team_cache.current_player_is_captain = true;
		if (!session_read_sector(session, team_id, &fresh, error))
			return false;
		if (!yt_record_set_number(&fresh.record, YT_F77,
		    (float)current_record))
			return false;
		team.overlay = fresh;
		team.captain = current_record;
		if (!yt_database_write(&session->door->game.database,
		    (size_t)yt_sector_basic_record(&session->door->game.config,
		    team_id),
		    &fresh.record, error))
			return false;
		if (!info_line(session, promoted, sizeof(promoted) - 1U, error))
			return false;
		if (!info_line(session, congratulations,
		    sizeof(congratulations) - 1U, error))
			return false;
		if (!info_line(session, NULL, 0U, error))
			return false;
		info_team_result(session, &current_player, &team, true,
		    resolved_team, current_is_captain);
		return true;
	}
	if (!yt_game_read_player(&session->door->game, captain_record, &ignored,
	    error))
		return false;
	if (!yt_info_team_row(YT_INFO_TEAM_OTHER_CAPTAIN, team_id, captain_name,
	    captain_name_length, row, sizeof(row), &row_length))
		return info_failure(error, "Info team row");
	if (!info_line(session, row, row_length, error))
		return false;
	if (!info_line(session, NULL, 0U, error))
		return false;
	info_team_result(session, &current_player, &team, false, resolved_team,
	    current_is_captain);
	return true;
}

static bool
info_panel_append(uint8_t *row, size_t capacity, size_t *length,
    const void *text, size_t text_length)
{
	if (row == NULL || length == NULL || *length > capacity
	    || text_length > capacity - *length
	    || (text == NULL && text_length != 0U))
		return false;
	if (text_length != 0U)
		memcpy(row + *length, text, text_length);
	*length += text_length;
	return true;
}

static bool
info_panel_cell(uint8_t *cell, size_t capacity, size_t *length,
    const char *label, const char *value)
{
	static const uint8_t bar = 0xba;

	*length = 0U;
	if (!info_panel_append(cell, capacity, length, &bar, 1U))
		return false;
	if (!info_panel_append(cell, capacity, length, label, strlen(label)))
		return false;
	return info_panel_append(cell, capacity, length, value, strlen(value));
}

static bool
info_panel_fixed(struct yt_session *session, const uint8_t *text,
    size_t length, float width, struct yt_error *error)
{
	return session_fixed_width_bytes(session, text, length, width,
	    "Info fixed-width presentation", error);
}

static bool
info_panel_ordinary(struct yt_session *session,
    const char *left_label, const char *left_value,
    const char *right_label, const char *right_value,
    struct yt_error *error)
{
	static const uint8_t bar = 0xba;
	uint8_t left[160];
	uint8_t right[160];
	size_t left_length;
	size_t right_length;

	if (!info_panel_cell(left, sizeof(left), &left_length, left_label,
	    left_value))
		return false;
	if (!info_panel_cell(right, sizeof(right), &right_length, right_label,
	    right_value))
		return false;
	if (!info_panel_fixed(session, left, left_length, 26.0f, error))
		return false;
	if (!info_panel_fixed(session, right, right_length, 23.0f, error))
		return false;
	return info_line(session, &bar, 1U, error);
}

static bool
info_panel_commodity(struct yt_session *session,
    const char *left_label, const char *left_value,
    const char *right_label, float right_value, struct yt_error *error)
{
	static const uint8_t bar = 0xba;
	uint8_t left[160];
	uint8_t right[160];
	char number[64];
	size_t left_length;
	size_t right_length;
	int number_length;

	number_length = qb_str_single(number, sizeof(number), right_value);
	if (number_length < 0)
		return false;
	if (!info_panel_cell(left, sizeof(left), &left_length, left_label,
	    left_value))
		return false;
	if (!info_panel_cell(right, sizeof(right), &right_length,
	    right_label, ""))
		return false;
	if (!info_panel_fixed(session, left, left_length, 26.0f, error))
		return false;
	if (!info_panel_fixed(session, right, right_length, 17.0f, error))
		return false;
	if (right_value != 0.0f) {
		session->presentation.bold = true;
		session_set_foreground(session, 7);
		session->presentation.background = 4;
	}
	if (!info_panel_fixed(session, (const uint8_t *)number,
	    (size_t)number_length, 6.0f, error))
		return false;
	session_set_foreground(session, 2);
	session->presentation.background = 0;
	return info_line(session, &bar, 1U, error);
}

bool
yt_session_show_ship(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t top[50] = {
		0xc9, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
		0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
		0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcb, 0xcd, 0xcd, 0xcd,
		0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
		0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xbb
	};
	static const uint8_t bottom[50] = {
		0xc8, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
		0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
		0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xca, 0xcd, 0xcd, 0xcd,
		0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
		0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xbc
	};
	static const uint8_t title[] = "[ Info ]";
	uint8_t row[256];
	char left[64];
	char right[64];
	size_t row_length;
	size_t cached_name_length = session->cached_player_name_length;
	int saved_foreground = session->presentation.foreground;
	float cloak_percent;
	bool anti_cloak = session->earth.anti_cloak_enabled;
	int length;

	if (!info_refresh_time(session, error))
		return false;
	session_set_foreground(session, 2);
	if (!info_line(session, NULL, 0U, error))
		return false;
	if (!info_panel_fixed(session, NULL, 0U, 20.0f, error))
		return false;
	if (!info_line(session, title, sizeof(title) - 1U, error))
		return false;
	if (!info_line(session, NULL, 0U, error))
		return false;
	row_length = 0U;
	if (!info_panel_append(row, sizeof(row), &row_length, "Name  : ", 8U))
		return false;
	if (!info_panel_append(row, sizeof(row), &row_length,
	    session->cached_player_name, cached_name_length))
		return false;
	if (!info_line(session, row, row_length, error))
		return false;
	row_length = 0U;
	if (!info_panel_append(row, sizeof(row), &row_length, "Time  :", 7U))
		return false;
	if (!info_panel_append(row, sizeof(row), &row_length,
	    session->time.text, session->time.text_length))
		return false;
	if (!info_line(session, row, row_length, error))
		return false;
	if (!yt_session_info_team_lines(session, NULL, NULL, error))
		return false;
	if (!session_reload_player(session, error))
		return false;
	if (!info_line(session, top, sizeof(top), error))
		return false;
	length = qb_str_double(left, sizeof(left),
	    (double)session->player.credits);
	if (length < 0)
		return false;
	length = qb_str_single(right, sizeof(right),
	    (float)session->player.sector);
	if (length < 0)
		return false;
	if (!info_panel_ordinary(session,
	    " Credits.. :", left, " Sector....... :", right, error))
		return false;
	if (qb_str_single(left, sizeof(left), session->player.turns) < 0)
		return false;
	if (qb_str_single(right, sizeof(right), session->player.holds) < 0)
		return false;
	if (!info_panel_ordinary(session, " Turns.... :", left,
	    " Holds........ :", right, error))
		return false;
	if (qb_str_double(left, sizeof(left),
	    (double)session->player.fighters) < 0)
		return false;
	if (!info_panel_commodity(session, " Fighters. :", left,
	    " Ore.......... :", session->player.ore, error))
		return false;
	if (qb_str_single(left, sizeof(left), session->player.mines) < 0)
		return false;
	if (!info_panel_commodity(session, " Mines.... :", left,
	    " Organics..... :", session->player.organics, error))
		return false;
	if (qb_str_single(left, sizeof(left), session->player.missiles) < 0)
		return false;
	if (!info_panel_commodity(session, " Missiles. :", left,
	    " Equipment.... :", session->player.equipment, error))
		return false;
	(void)snprintf(left, sizeof(left), "%s",
	    session->player.danger_scanner == 0 ? " NONE" : " Installed");
	if (qb_str_single(right, sizeof(right),
	    (float)session->player.ports_owned) < 0)
		return false;
	if (!info_panel_ordinary(session, " Scanner.. :", left,
	    " Ports Owned.. :", right, error))
		return false;
	if (qb_str_double(left, sizeof(left),
	    (double)session->player.shields) < 0)
		return false;
	if (anti_cloak)
		(void)snprintf(right, sizeof(right), "%s", " FAIL");
	else {
		cloak_percent = floorf(qb_single_multiply(session->player.cloak, 100.0f));
		if (qb_str_single(right, sizeof(right), cloak_percent) < 0
		    || strlen(right) + 1U >= sizeof(right))
			return false;
		strcat(right, "%");
	}
	if (!info_panel_ordinary(session, " Shields.. :", left,
	    " Cloak Energy. :", right, error))
		return false;
	if (qb_str_single(left, sizeof(left),
	    session->player.ground_forces) < 0)
		return false;
	if (qb_str_single(right, sizeof(right), session->player.plasma) < 0)
		return false;
	if (!info_panel_ordinary(session, " Forces... :", left,
	    " Plasma Bolts. :", right, error))
		return false;
	if (!info_line(session, bottom, sizeof(bottom), error))
		return false;
	session_set_foreground(session, saved_foreground);
	return true;
}
