#include "yt_session_team_internal.h"

#include "qb.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

bool
session_team_pick_name(struct yt_session *session, int team_id, char name[42],
    bool *accepted, struct yt_error *error)
{
	static const uint8_t prompt[] =
	    "Pick a name for your Team (41 chars. max)? ";
	static const uint8_t invalid[] =
	    "Team names MUST more than 2 letters!";
	struct yt_team team;
	char response[YT_COMMAND_SIZE];
	size_t name_length;

	if (accepted == NULL)
		return false;
	*accepted = false;
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "team name leading blank", error)
	    || !session_present_timed_paged_row(session, prompt, sizeof(prompt) - 1U,
	    "team name prompt", error)
	    || !session_read_command(session, response, sizeof(response)))
		return false;
	if (!yt_team_prepare_name(response, &name_length))
		return session_present_alert(session, invalid, sizeof(invalid) - 1U,
		    "team name invalid length", error);
	(void)snprintf(name, 42, "%s", response);
	if (!session_team_read_overlay(session, team_id, &team, error))
		return false;
	(void)snprintf(team.name, sizeof(team.name), "%s", response);
	yt_team_name_overlay(&team.overlay.record,
	    (const uint8_t *)response, name_length);
	if (!yt_database_write(&session->door->game.database,
	    (size_t)session_sector_basic_record(session, team.id),
	    &team.overlay.record, error))
		return false;
	*accepted = true;
	return true;
}

bool
session_team_create_password(struct yt_session *session, int team_id,
    char password[5], struct yt_error *error)
{
	static const uint8_t prompt[] =
	    "Please Pick a Password for your Team. (4 Chars.) :";
	static const uint8_t invalid[] = "Password MUST be 4 characters!";
	struct yt_team team;
	char response[80];
	char reminder[160];

	for (;;) {
		if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
		    "team password leading blank", error)
		    || !session_present_timed_paged_row(session, prompt, sizeof(prompt) - 1U,
		    "team password prompt", error)
		    || !session_read_upper_command(session, response, sizeof(response)))
			return false;
		if (strlen(response) != 4U) {
			if (!session_present_alert(session, invalid, sizeof(invalid) - 1U,
			    "team password invalid length", error))
				return false;
			continue;
		}
		memcpy(password, response, 4);
		password[4] = '\0';
		if (snprintf(reminder, sizeof(reminder),
		    "REMEMBER YOUR TEAM PASSWORD SO OTHERS CAN JOIN! -+> %s",
		    password) < 0
		    || !session_present_alert(session, (const uint8_t *)reminder,
		    strlen(reminder), "team password reminder", error)
		    || !session_team_read_overlay(session, team_id, &team, error))
			return false;
		memcpy(team.password, password, 5);
		yt_team_password_overlay(&team.overlay.record,
		    (const uint8_t *)password);
		return yt_database_write(&session->door->game.database,
		    (size_t)session_sector_basic_record(session, team.id),
		    &team.overlay.record, error);
	}
}

bool
session_team_create(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t entering[] = "Entering a New Team...";
	struct yt_team team;
	char name[42];
	char actor_name[42];
	char password[5];
	char number[64];
	char news[300];
	char success[300];
	int id;
	int selected;
	bool name_accepted;

	(void)snprintf(actor_name, sizeof(actor_name), "%s",
	    session->player.name);
	if (!session_present_alert(session, entering, sizeof(entering) - 1U,
	    "team create heading", error))
		return false;
	selected = (int)session->player.team;
	for (id = 1; id <= YT_DEFAULT_PLAYER_COUNT; ++id) {
		if (!session_load_team(session, id, &team, error))
			return false;
		if (!team.live) {
			selected = id;
			break;
		}
	}
	if (!session_team_pick_name(session, selected, name, &name_accepted,
	    error))
		return false;
	if (!name_accepted)
		return true;
	id = selected;
	if (!yt_game_read_player(&session->door->game, session_record(session),
	    &session->player, error))
		return false;
	yt_team_membership_apply_player(&session->player, id);
	if (!session_write_player(session, error)
	    || !session_team_read_overlay(session, id, &team, error))
		return false;
	team.id = id;
	team.captain = session_record(session);
	team.roster[0] = session_record(session);
	team.roster[1] = 0;
	team.roster[2] = 0;
	team.roster[3] = 0;
	yt_record_set_number_if_changed(&team.overlay.record, YT_F77,
	    (float)team.captain);
	yt_team_roster_overlay(&team.overlay.record, team.roster);
	if (!yt_database_write(&session->door->game.database,
	    (size_t)session_sector_basic_record(session, team.id),
	    &team.overlay.record, error)
	    || !session_team_create_password(session, id, password, error))
		return false;
	session_set_foreground(session, 3.0f);
	if (qb_str_single(number, sizeof(number), (float)selected) < 0
	    || snprintf(news, sizeof(news), "%s Created Team%s -=- %s",
	    actor_name, number, name) < 0
	    || !yt_news_append(news, error)
	    || snprintf(success, sizeof(success),
	    "Team number [%s ] [%s] CREATED!", number, name) < 0)
		return false;
	return session_present_alert(session, (const uint8_t *)success,
	    strlen(success), "team create success", error);
}

bool
session_team_join(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t selection_prompt[] =
	    "Which team do you wish to join (0=quit)? ";
	static const uint8_t dead[] = "That team is dead!";
	static const uint8_t full[] = "The Team you picked is full!!";
	static const uint8_t password_prompt[] =
	    "Please enter Password to Join Team? ";
	static const uint8_t invalid[] = "Invalid Password entered!";
	static const uint8_t success[] =
	    "Your Team info has been recorded!  Have fun!";
	struct yt_team team;
	struct qb_val_result parsed;
	char line[80];
	char actor_name[42];
	char number[64];
	char row[160];
	int id;
	int selected;
	size_t index;
	char news[300];
	bool listed;

	(void)snprintf(actor_name, sizeof(actor_name), "%s",
	    session->player.name);
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "team join leading blank", error))
		return false;

	for (id = 1; id <= YT_DEFAULT_PLAYER_COUNT; ++id) {
		if (!session_load_team(session, id, &team, error))
			return false;
		listed = team.live;
		if (listed) {
			if (qb_str_single(number, sizeof(number), (float)id) < 0
			    || snprintf(row, sizeof(row), "%s] %s", number,
			    team.name) < 0
			    || !session_present_paged_fragment(session, (const uint8_t *)row,
			    strlen(row)))
				return false;
		}
	}
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "team join selection blank", error)
	    || !session_present_timed_paged_row(session, selection_prompt,
	    sizeof(selection_prompt) - 1U, "team join selection prompt", error)
	    || !session_read_number_command(session, line, sizeof(line)))
		return false;
	parsed = qb_val(line);
	if (!parsed.valid || parsed.overflow) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "team join:VAL");
		}
		return false;
	}
	selected = (int)floor(parsed.value);
	if (selected < 1)
		return true;
	if (!session_load_team(session, selected, &team, error))
		return false;
	if (!team.live)
		return session_present_alert(session, dead, sizeof(dead) - 1U,
		    "team join dead", error);
	if (team.full)
		return session_present_alert(session, full, sizeof(full) - 1U,
		    "team join full", error);
	{
		struct yt_team ignored;

		if (!session_team_read_overlay(session, selected, &ignored, error))
			return false;
	}
	if (qb_str_single(number, sizeof(number), (float)selected) < 0
	    || snprintf(row, sizeof(row), "Team #%s: %s", number,
	    team.name) < 0
	    || !session_present_paged_fragment(session, (const uint8_t *)row, strlen(row))
	    || !session_present_timed_paged_row(session, password_prompt,
	    sizeof(password_prompt) - 1U, "team join password prompt", error)
	    || !session_read_upper_command(session, line, sizeof(line)))
		return false;
	if (strlen(line) != 4U || memcmp(line, team.password, 4) != 0) {
		if (!session_team_audit(session, selected,
		    YT_TEAM_AUDIT_INVALID_PASSWORD, line, error))
			return false;
		return session_present_alert(session, invalid, sizeof(invalid) - 1U,
		    "invalid team password row", error);
	}
	if (!yt_game_read_player(&session->door->game, session_record(session),
	    &session->player, error))
		return false;
	yt_team_membership_apply_player(&session->player, selected);
	if (!session_write_player(session, error))
		return false;
	for (index = 0; index < 4; ++index) {
		if (team.roster[index] == 0) {
			team.roster[index] = session_record(session);
			break;
		}
	}
	{
		struct yt_team fresh;

		if (!session_team_read_overlay(session, selected, &fresh, error))
			return false;
		memcpy(fresh.roster, team.roster, sizeof(fresh.roster));
		if (!session_team_store_roster(session, &fresh, error))
			return false;
	}
	if (qb_str_single(number, sizeof(number), (float)selected) < 0
	    || snprintf(news, sizeof(news), "%s Joined Team%s",
	    actor_name, number) < 0)
		return false;
	if (!yt_news_append(news, error))
		return false;
	session_set_foreground(session, 3.0f);
	if (!session_present_alert(session, success, sizeof(success) - 1U,
	    "team join success row", error))
		return false;
	return session_team_audit(session, selected, YT_TEAM_AUDIT_JOIN, "", error);
}

bool
session_team_quit(struct yt_session *session, struct yt_team *team,
    struct yt_error *error)
{
	static const uint8_t prompt[] =
	    "Are you sure you wish to quit your team? [N] ";
	static const uint8_t success[] =
	    "You have been removed from Team play";
	enum yt_yes_no_answer answer;
	struct yt_player persisted;
	struct yt_sector explicit_overlay;
	int old_team;
	size_t index;
	bool live = false;

	if (!session_confirm(session, prompt, sizeof(prompt) - 1U,
	    &answer, error))
		return false;
	if (answer != YT_YES_NO_YES)
		return true;
	if (!yt_game_read_player(&session->door->game, session_record(session),
	    &session->player, error))
		return false;
	old_team = (int)session->player.team;
	yt_team_membership_apply_player(&session->player, 0);
	persisted = session->player;
	if (!yt_game_write_player(&session->door->game, session_record(session),
	    &persisted, error))
		return false;
	if (!session_read_sector(session, old_team,
	    &explicit_overlay, error))
		return false;
	(void)explicit_overlay;
	if (!session_load_team(session, old_team, team, error))
		return false;
	for (index = 0; index < 4; ++index) {
		if (team->roster[index] == session_record(session))
			team->roster[index] = 0;
	}
	if (!session_team_store_roster(session, team, error)
	    || !session_load_team(session, old_team, team, error))
		return false;
	for (index = 0; index < 4; ++index)
		if (team->roster[index] != 0)
			live = true;
	if (!live) {
		team->captain = 0;
		memcpy(team->password, "    ", 4);
		team->password[4] = '\0';
		memset(team->roster, 0, sizeof(team->roster));
		if (!session_team_store_inactive(session, team, error))
			return false;
	}
	session_set_foreground(session, 6.0f);
	if (!session_present_paged_line(session, success, sizeof(success) - 1U,
	    "team quit success row", error)
	    || !session_team_audit(session, old_team, YT_TEAM_AUDIT_QUIT, "",
	    error))
		return false;
	return true;
}

bool
session_team_search(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t locating[] =
	    "Locating Team Members, Planets & Defenses.";
	static const uint8_t heading[] =
	    "Name                                     Sector";
	static const uint8_t rule[] =
	    "====================                     ======";
	static const uint8_t defending[] = "Defending;";
	static const uint8_t planets[] = "Planets;";
	static const uint8_t none[] = "None Found";
	const float cached_team = session->player.team;
	int player_record;
	bool found = false;

	if (!session_present_paged_line(session, locating, sizeof(locating) - 1U,
	    "team resource locating row", error))
		return false;
	for (player_record = YT_PLAYER_FIRST_RECORD;
	    player_record <= session_sector_offset(session);
	    ++player_record) {
		struct yt_player player;
		uint8_t row[YT_TEXT_FIELD_SIZE + 64U];
		char number[64];
		size_t number_length;
		int logical_sector;
		bool first;

		if (!yt_game_read_player(&session->door->game, player_record,
		    &player, error))
			return false;
		if (player.team != cached_team
		    || player_record == session_record(session))
			continue;
		if (qb_str_single(number, sizeof(number),
		    (float)player.sector) < 0)
			return false;
		number_length = strlen(number);
		memcpy(row, player.record.bytes, YT_TEXT_FIELD_SIZE);
		memcpy(row + YT_TEXT_FIELD_SIZE, number, number_length);
		if (!session_present_paged_line(session, heading, sizeof(heading) - 1U,
		    "team resource player heading", error)
		    || !session_present_paged_fragment(session, rule, sizeof(rule) - 1U)
		    || !session_present_paged_fragment(session, row,
		    YT_TEXT_FIELD_SIZE + number_length))
			return false;
		found = true;

		first = true;
		for (logical_sector = 1;
		    logical_sector <= session_sector_count(session);
		    ++logical_sector) {
			struct yt_sector sector;

			if (!session_read_sector(session,
			    logical_sector, &sector, error))
				return false;
			if (!(sector.fighters > 0.0f
			    && sector.fighter_owner == player_record))
				continue;
			if (first && !session_present_text(session, defending,
			    sizeof(defending) - 1U, SESSION_PRESENT_RAW,
			    "team resource defense label", error))
				return false;
			first = false;
			if (qb_str_single(number, sizeof(number),
			    (float)logical_sector) < 0
			    || !session_present_text(session,
			    (const uint8_t *)number, strlen(number),
			    SESSION_PRESENT_RAW, "team resource defense sector",
			    error))
				return false;
		}
		if (!first && !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "team resource defense terminator",
		    error))
			return false;

		first = true;
		for (logical_sector = 1;
		    logical_sector <= session_sector_count(session);
		    ++logical_sector) {
			struct yt_sector sector;
			struct yt_planet planet;

			if (!session_read_sector(session,
			    logical_sector, &sector, error))
				return false;
			if (sector.planet <= 0)
				continue;
			if (!session_read_planet(session,
			    sector.planet, &planet, error))
				return false;
			if (planet.owner != (float)player_record)
				continue;
			if (first && !session_present_text(session, planets,
			    sizeof(planets) - 1U, SESSION_PRESENT_RAW,
			    "team resource planet label", error))
				return false;
			first = false;
			if (qb_str_single(number, sizeof(number),
			    (float)logical_sector) < 0
			    || !session_present_text(session,
			    (const uint8_t *)number, strlen(number),
			    SESSION_PRESENT_RAW, "team resource planet sector",
			    error))
				return false;
		}
		if (!first && !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "team resource planet terminator",
		    error))
			return false;
	}
	if (!found)
		return session_present_paged_fragment(session, none, sizeof(none) - 1U);
	return true;
}
