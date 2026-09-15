#include "yt_session_internal.h"

#include "qb.h"
#include "yt_platform.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

bool
yt_session_load_team_cache(struct yt_session *session, int team_id,
    int current_player_record, struct yt_record *overlay,
    bool *overlay_loaded, bool *live, struct yt_error *error)
{
	struct yt_record loaded;
	float expression;
	uint32_t physical_record;

	if (overlay_loaded != NULL)
		*overlay_loaded = false;
	if (live != NULL)
		*live = false;
	memset(&session->team_cache, 0, sizeof(session->team_cache));
	if (team_id < 1 || team_id > YT_DEFAULT_PLAYER_COUNT)
		return true;
	expression = qb_single_add(session_sector_offset(session),
	    (float)team_id);
	physical_record = qb_brun_random_record_number(expression);
	if (!yt_database_read(&session->door->game.database,
	    (size_t)physical_record, &loaded, error))
		return false;
	if (overlay != NULL)
		*overlay = loaded;
	if (overlay_loaded != NULL)
		*overlay_loaded = true;
	yt_team_cache_load(&session->team_cache, &loaded,
	    current_player_record, live);
	return true;
}

bool
session_load_team(struct yt_session *session, int id, struct yt_team *team,
    struct yt_error *error)
{
	struct yt_record overlay;
	bool overlay_loaded;
	bool live;
	size_t index;

	if (team != NULL) {
		memset(team, 0, sizeof(*team));
		team->id = id;
	}
	if (!yt_session_load_team_cache(session, id, session_record(session),
	    &overlay, &overlay_loaded, &live, error))
		return false;
	if (team == NULL)
		return true;
	if (overlay_loaded)
		yt_sector_decode(&team->overlay, &overlay);
	memcpy(team->name, session->team_cache.name,
	    sizeof(team->name));
	team->name_length = session->team_cache.name_length;
	memcpy(team->password, session->team_cache.password,
	    sizeof(team->password));
	team->captain = session->team_cache.captain;
	team->live = live;
	team->full = team->live;
	for (index = 0; index < 4; ++index) {
		team->roster[index] = session->team_cache.roster[index];
		if (team->roster[index] <= 0)
			team->full = false;
	}
	return true;
}
static bool
team_store_inactive(struct yt_session *session, struct yt_team *team,
    struct yt_error *error)
{
	if (!session_read_sector(session, team->id, &team->overlay, error))
		return false;
	yt_team_inactive_overlay(&team->overlay.record);
	return yt_database_write(&session->door->game.database,
	    (size_t)session_sector_basic_record(session, (float)team->id),
	    &team->overlay.record, error);
}

static bool
team_read_overlay(struct yt_session *session, int id, struct yt_team *team,
    struct yt_error *error)
{
	memset(team, 0, sizeof(*team));
	team->id = id;
	if (id < 0 || id > YT_DEFAULT_PLAYER_COUNT)
		return true;
	return session_read_sector(session, id, &team->overlay,
	    error);
}

static bool
team_store_roster(struct yt_session *session, struct yt_team *team,
    struct yt_error *error)
{
	yt_team_roster_overlay(&team->overlay.record, team->roster);
	return yt_database_write(&session->door->game.database,
	    (size_t)session_sector_basic_record(session, (float)team->id),
	    &team->overlay.record, error);
}

static bool
team_audit(struct yt_session *session, int team_id,
    enum yt_team_audit_event event, const char *attempt,
    struct yt_error *error)
{
	uint8_t message[YT_COMMAND_SIZE + 128U];
	struct yt_clock_value now;
	char date[11] = "";
	char time_text[9] = "";
	size_t message_length;
	size_t index;

	if (event == YT_TEAM_AUDIT_JOIN || event == YT_TEAM_AUDIT_QUIT) {
		if (!yt_platform_clock(&now, error))
			return false;
		yt_format_date(&now, date);
		if (!yt_platform_clock(&now, error))
			return false;
		yt_format_time(&now, time_text);
	}
	if (!yt_team_audit_message(event, session->player.name, attempt, date,
	    time_text, message, sizeof(message), &message_length)) {
		if (error != NULL) {
			error->status = YT_RANGE;
			error->system_error = 0;
			(void)snprintf(error->operation,
			    sizeof(error->operation), "%s",
			    "team audit message length");
			error->path[0] = '\0';
		}
		return false;
	}
	if (!yt_session_load_team_cache(session, team_id,
	    session_record(session),
	    NULL, NULL, NULL, error))
		return false;
	for (index = 0U; index < YT_ARRAY_LEN(session->team_cache.roster);
	    ++index) {
		int recipient = session->team_cache.roster[index];

		if (recipient != 0 && recipient != session_record(session)
		    && !session_append_radio_bytes(message, message_length, -2.0f,
		    (float)recipient, error))
			return false;
	}
	return true;
}


static bool
team_pick_name(struct yt_session *session, float team_id, char name[42],
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
	if (team_id != floorf(team_id) || team_id < 0.0f
	    || team_id > (float)YT_DEFAULT_PLAYER_COUNT) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "team name record number");
		}
		return false;
	}
	(void)snprintf(name, 42, "%s", response);
	if (!team_read_overlay(session, (int)team_id, &team, error))
		return false;
	(void)snprintf(team.name, sizeof(team.name), "%s", response);
	yt_team_name_overlay(&team.overlay.record,
	    (const uint8_t *)response, name_length);
	if (!yt_database_write(&session->door->game.database,
	    (size_t)session_sector_basic_record(session, (float)team.id),
	    &team.overlay.record, error))
		return false;
	*accepted = true;
	return true;
}

static bool
team_create_password(struct yt_session *session, int team_id,
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
		    || !team_read_overlay(session, team_id, &team, error))
			return false;
		memcpy(team.password, password, 5);
		yt_team_password_overlay(&team.overlay.record,
		    (const uint8_t *)password);
		return yt_database_write(&session->door->game.database,
		    (size_t)session_sector_basic_record(session, (float)team.id),
		    &team.overlay.record, error);
	}
}

static bool
team_create(struct yt_session *session, struct yt_error *error)
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
	float selected;
	bool name_accepted;

	(void)snprintf(actor_name, sizeof(actor_name), "%s",
	    session->player.name);
	if (!session_present_alert(session, entering, sizeof(entering) - 1U,
	    "team create heading", error))
		return false;
	selected = session->player.team;
	for (id = 1; id <= YT_DEFAULT_PLAYER_COUNT; ++id) {
		if (!session_load_team(session, id, &team, error))
			return false;
		if (!team.live) {
			selected = (float)id;
			break;
		}
	}
	if (!team_pick_name(session, selected, name, &name_accepted, error))
		return false;
	if (!name_accepted)
		return true;
	id = (int)selected;
	if (!yt_game_read_player(&session->door->game, session_record(session),
	    &session->player, error))
		return false;
	yt_team_membership_apply_player(&session->player, id);
	if (!session_write_player(session, error)
	    || !team_read_overlay(session, id, &team, error))
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
	    (size_t)session_sector_basic_record(session, (float)team.id),
	    &team.overlay.record, error)
	    || !team_create_password(session, id, password, error))
		return false;
	session_set_foreground(session, 3.0f);
	if (qb_str_single(number, sizeof(number), selected) < 0
	    || snprintf(news, sizeof(news), "%s Created Team%s -=- %s",
	    actor_name, number, name) < 0
	    || !yt_news_append(news, error)
	    || snprintf(success, sizeof(success),
	    "Team number [%s ] [%s] CREATED!", number, name) < 0)
		return false;
	return session_present_alert(session, (const uint8_t *)success,
	    strlen(success), "team create success", error);
}

static bool
team_join(struct yt_session *session, struct yt_error *error)
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

		if (!team_read_overlay(session, selected, &ignored, error))
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
		if (!team_audit(session, selected,
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

		if (!team_read_overlay(session, selected, &fresh, error))
			return false;
		memcpy(fresh.roster, team.roster, sizeof(fresh.roster));
		if (!team_store_roster(session, &fresh, error))
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
	return team_audit(session, selected, YT_TEAM_AUDIT_JOIN, "", error);
}

static bool
team_quit(struct yt_session *session, struct yt_team *team,
    struct yt_error *error)
{
	static const uint8_t prompt[] =
	    "Are you sure you wish to quit your team? [N] ";
	static const uint8_t success[] =
	    "You have been removed from Team play";
	enum yt_yes_no_answer answer;
	struct yt_player persisted;
	struct yt_sector explicit_overlay;
	float old_team;
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
	old_team = session->player.team;
	yt_team_membership_apply_player(&session->player, 0);
	persisted = session->player;
	if (!yt_game_write_player(&session->door->game, session_record(session),
	    &persisted, error))
		return false;
	if (old_team != floorf(old_team) || old_team < 1.0f
	    || old_team > (float)YT_DEFAULT_PLAYER_COUNT) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "team quit record number");
		}
		return false;
	}
	if (!session_read_sector(session, (int)old_team,
	    &explicit_overlay, error))
		return false;
	(void)explicit_overlay;
	if (!session_load_team(session, (int)old_team, team, error))
		return false;
	for (index = 0; index < 4; ++index) {
		if (team->roster[index] == session_record(session))
			team->roster[index] = 0;
	}
	if (!team_store_roster(session, team, error)
	    || !session_load_team(session, (int)old_team, team, error))
		return false;
	for (index = 0; index < 4; ++index)
		if (team->roster[index] != 0)
			live = true;
	if (!live) {
		team->captain = 0;
		memcpy(team->password, "    ", 4);
		team->password[4] = '\0';
		memset(team->roster, 0, sizeof(team->roster));
		if (!team_store_inactive(session, team, error))
			return false;
	}
	session_set_foreground(session, 6.0f);
	if (!session_present_paged_line(session, success, sizeof(success) - 1U,
	    "team quit success row", error)
	    || !team_audit(session, (int)old_team, YT_TEAM_AUDIT_QUIT, "",
	    error))
		return false;
	return true;
}

static bool
team_search(struct yt_session *session, struct yt_error *error)
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
	    player_record <= (int)session_sector_offset(session);
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
		if (qb_str_single(number, sizeof(number), player.sector) < 0)
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
			    && sector.fighter_owner == (float)player_record))
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
			if (sector.planet <= 0.0f)
				continue;
			if (!session_read_planet(session,
			    (int)sector.planet, &planet, error))
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

static bool
team_transfer(struct yt_session *session, struct yt_error *error)
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
	logical_sector = (int)session->player.sector;
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
		    initial_fighters) < 0
		    || qb_str_double(defense_text, sizeof(defense_text),
		    initial_defense) < 0
		    || snprintf(row, sizeof(row), "You have%s fighters.",
		    fighter_text) < 0
		    || !session_present_paged_line(session, (const uint8_t *)row, strlen(row),
		    "team transfer carried row", error)
		    || snprintf(row, sizeof(row), "There are%s fighters here.",
		    defense_text) < 0
		    || !session_present_paged_line(session, (const uint8_t *)row, strlen(row),
		    "team transfer deployed row", error)
		    || !session_present_timed_paged_row(session, prompt, sizeof(prompt) - 1U,
		    "team transfer prompt", error)
		    || !session_read_number_command(session, response, sizeof(response)))
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
			    fighter_text) < 0
			    || !session_present_alert(session, (const uint8_t *)row,
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
			    (float)logical_sector),
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

static bool
team_banish(struct yt_session *session, struct yt_team *team,
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
	team_id = (int)session->player.team;
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
		if (!yt_player_stored_name(&member, prompt + prompt_length,
		    &name_length, error))
			return false;
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
		    &team->overlay, error)
		    || !team_store_roster(session, team, error))
			return false;
		return session_present_alert(session, success, sizeof(success) - 1U,
		    "team banish success", error);
	}
	return session_present_paged_fragment(session, end, sizeof(end) - 1U);
}

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
		uint8_t numeric_raw[4];
		uint8_t prompt[sizeof(prompt_prefix) - 1U
		    + sizeof(session->time.text) + sizeof(prompt_body) - 1U];
		size_t prompt_length = 0;
		size_t index;
		float numeric;
		int32_t captain_cint;
		int32_t team_cint;
		bool overflow;
		bool invalid;

		session_set_foreground(session, 6.0f);
		if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
		    "team front leading blank", error)
		    || !yt_session_info_team_lines(session, &team, &captain,
		    error))
			return false;
		session_set_pager_line_count(session, 0.0f);
		if (!session_reload_player(session, error)
		    || !session_present_paged_line(session, exit_row, sizeof(exit_row) - 1U,
		    "team exit row", error)
		    || !session_reload_player(session, error))
			return false;
		if (session->player.team == 0.0f) {
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
		session_set_foreground(session, 6.0f);
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
		    "team command prompt", error)
		    || !session_read_command(session, line, sizeof(line)))
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
		numeric = (float)parsed.value;
		conversion = qb_mbf32_encode(numeric, numeric_raw);
		if (conversion == QB_MBF_OVERFLOW) {
			if (error != NULL) {
				error->status = YT_RANGE;
				(void)snprintf(error->operation,
				    sizeof(error->operation), "%s", "team:choice-csng");
			}
			return false;
		}
		numeric = qb_mbf32_decode(numeric_raw);
		captain_cint = qb_cint(captain ? -1.0 : 0.0, &overflow);
		if (overflow) {
			if (error != NULL) {
				error->status = YT_RANGE;
				(void)snprintf(error->operation,
				    sizeof(error->operation), "%s",
				    "team:captain-cint");
			}
			return false;
		}
		team_cint = qb_cint_mbf32(session->player.record.bytes + YT_F89, 0U,
		    &overflow);
		if (overflow) {
			if (error != NULL) {
				error->status = YT_RANGE;
				(void)snprintf(error->operation,
				    sizeof(error->operation), "%s", "team:team-cint");
			}
			return false;
		}
		invalid = yt_team_choice_rejected(numeric, session->player.team,
		    captain_cint, team_cint);
		if (invalid) {
			if (!session_present_alert(session, invalid_row,
			    sizeof(invalid_row) - 1U, "team invalid choice", error))
				return false;
			continue;
		}
		if (strcmp(line, "1") == 0)
			return true;
		if (strcmp(line, "2") == 0) {
			if (!team_create(session, error))
				return false;
		}
		else if (strcmp(line, "3") == 0) {
			if (!team_join(session, error))
				return false;
		}
		else if (strcmp(line, "4") == 0) {
			if (!team_quit(session, &team, error))
				return false;
		}
		else if (strcmp(line, "5") == 0) {
			if (!team_search(session, error))
				return false;
		}
		else if (strcmp(line, "6") == 0) {
			if (!team_transfer(session, error))
				return false;
		}
		else if (strcmp(line, "7") == 0) {
			if (!team_banish(session, &team, error))
				return false;
		}
		else if (strcmp(line, "8") == 0) {
			if (!team_create_password(session, team.id,
			    team.password, error))
				return false;
		}
		else if (strcmp(line, "9") == 0) {
			char name[42];
			bool accepted;

			if (!team_pick_name(session, (float)team.id, name,
			    &accepted, error))
				return false;
			(void)accepted;
		}
	}
}
