#include "yt_session_team_internal.h"

#include "qb.h"
#include "yt_platform.h"

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
bool
session_team_store_inactive(struct yt_session *session, struct yt_team *team,
    struct yt_error *error)
{
	if (!session_read_sector(session, team->id, &team->overlay, error))
		return false;
	yt_team_inactive_overlay(&team->overlay.record);
	return yt_database_write(&session->door->game.database,
	    (size_t)session_sector_basic_record(session, (float)team->id),
	    &team->overlay.record, error);
}

bool
session_team_read_overlay(struct yt_session *session, int id, struct yt_team *team,
    struct yt_error *error)
{
	memset(team, 0, sizeof(*team));
	team->id = id;
	if (id < 0 || id > YT_DEFAULT_PLAYER_COUNT)
		return true;
	return session_read_sector(session, id, &team->overlay,
	    error);
}

bool
session_team_store_roster(struct yt_session *session, struct yt_team *team,
    struct yt_error *error)
{
	yt_team_roster_overlay(&team->overlay.record, team->roster);
	return yt_database_write(&session->door->game.database,
	    (size_t)session_sector_basic_record(session, (float)team->id),
	    &team->overlay.record, error);
}

bool
session_team_audit(struct yt_session *session, int team_id,
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
