#include "yt_session_internal.h"

#include "qb.h"

#include <string.h>

bool
yt_session_common_fatal_self(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t notice[] = "Your ship has been destroyed!";
	char cached_name[sizeof(session->player.name)];
	int current_player_record;

	current_player_record = session_record(session);
	session_set_foreground(session, 3.0f);
	if (!session_present_alert(session, notice, sizeof(notice) - 1U,
	    "common fatal notice", error))
		return false;
	memcpy(cached_name, session->player.name, sizeof(cached_name));
	if (!session_reload_player(session, error))
		return false;
	memcpy(session->player.name, cached_name, sizeof(cached_name));
	if (!session_sound(session, 3.0f, "fatal destruction sound", error)
	    || !yt_session_kill_player(session, current_player_record,
	    (float)current_player_record, false, error)
	    || !session_wait(session, 5.0, "common fatal wait", error))
		return false;
	session->fatal_wait_complete = true;
	return true;
}

static int
death_port_count(const struct yt_session *session)
{
	return (int)(session_planet_offset(session)
	    - session_port_offset(session));
}

static bool
death_read_port(struct yt_session *session, int logical_port,
    struct yt_port *port, struct yt_error *error)
{
	struct yt_record record;

	if (!yt_database_read(&session->door->game.database,
	    (size_t)session_port_basic_record(session, (float)logical_port),
	    &record, error))
		return false;
	yt_port_decode(port, &record);
	return true;
}

static bool
death_write_port(struct yt_session *session, int logical_port,
    struct yt_port *port, struct yt_error *error)
{
	yt_port_encode(port);
	return yt_database_write(&session->door->game.database,
	    (size_t)session_port_basic_record(session, (float)logical_port),
	    &port->record, error);
}

static bool
death_remove_from_team(struct yt_session *session, int victim,
    struct yt_error *error)
{
	static const size_t roster_offsets[4] = {
		YT_F109, YT_F117, YT_F121, YT_F125
	};
	struct yt_player player;
	struct yt_record overlay;
	int team_id;
	float expression;
	uint32_t physical_record;
	size_t index;

	if (!yt_game_read_player(&session->door->game, victim, &player, error))
		return false;
	team_id = (int)player.team;
	if (team_id == 0)
		return true;

	if (!yt_session_load_team_cache(session, team_id,
	    session_record(session), NULL, NULL, NULL, error))
		return false;
	for (index = 0U; index < YT_ARRAY_LEN(session->team_cache.roster);
	    ++index) {
		if (session->team_cache.roster[index] == victim)
			session->team_cache.roster[index] = 0;
	}

	expression = qb_single_add(session_sector_offset(session),
	    (float)team_id);
	physical_record = qb_brun_random_record_number(expression);
	if (!yt_database_read(&session->door->game.database,
	    (size_t)physical_record, &overlay, error))
		return false;
	for (index = 0U; index < YT_ARRAY_LEN(roster_offsets); ++index)
		(void)yt_record_set_number(&overlay, roster_offsets[index],
		    (float)session->team_cache.roster[index]);
	if (!yt_database_write(&session->door->game.database,
	    (size_t)physical_record, &overlay, error)
	    || !yt_game_read_player(&session->door->game, victim, &player,
	    error))
		return false;
	player.team = 0.0f;
	(void)yt_record_set_number(&player.record, YT_F89, 0.0f);
	return yt_game_write_player(&session->door->game, victim, &player,
	    error);
}

bool
yt_session_kill_player(struct yt_session *session, int victim_record,
    float killer, bool wait_for_current, struct yt_error *error)
{
	struct yt_player victim;
	struct yt_player player;
	uint8_t victim_name[YT_TEXT_FIELD_SIZE];
	uint8_t row[300];
	size_t victim_name_length;
	size_t row_length;
	float old_ports_owned;
	float matched;
	int matched_ports = 0;
	int current_player_record;
	int logical;
	bool self;
	bool valid_killer;

	current_player_record = session_record(session);
	(void)yt_player_cache_set(&session->player_cache, victim_record,
	    YT_PLAYER_CACHE_SECTOR, 0.0f);
	if (!yt_game_read_player(&session->door->game, victim_record, &victim,
	    error))
		return false;
	victim_name_length = yt_player_stored_name(&victim, victim_name);
	old_ports_owned = victim.ports_owned;
	yt_death_player_overlay(&victim, killer);
	if (!yt_game_write_player(&session->door->game, victim_record, &victim,
	    error))
		return false;

	for (logical = 1; logical <= session_sector_count(session); ++logical) {
		struct yt_sector sector;

		if (!session_read_sector(session, logical, &sector, error))
			return false;
		if (yt_death_sector_overlay(&sector, (float)victim_record)
		    && !session_write_sector(session, logical, &sector, error))
			return false;
	}
	if (!death_remove_from_team(session, victim_record, error))
		return false;
	if (old_ports_owned != 0.0f) {
		for (logical = 1; logical <= death_port_count(session); ++logical) {
			struct yt_port port;
			enum yt_death_port_route route;

			if (!death_read_port(session, logical, &port, error))
				return false;
			route = yt_death_port_overlay(&port,
			    (float)victim_record, killer,
			    session_sector_offset(session));
			if (route == YT_DEATH_PORT_UNMATCHED)
				continue;
			++matched_ports;
			if (!death_write_port(session, logical, &port, error))
				return false;
		}
	}

	valid_killer = (killer != (float)victim_record)
	    & (killer > 1.0f)
	    & (killer <= session_sector_offset(session));
	matched = (float)matched_ports;
	if (valid_killer && matched_ports != 0) {
		if (!yt_death_title_row(victim_name, victim_name_length, matched,
		    row, sizeof(row), &row_length)
		    || !session_present_text(session, row, row_length,
		    SESSION_PRESENT_LINE, "death title row", error)
		    || !yt_game_read_player(&session->door->game, (int)killer,
		    &player, error))
			return false;
		yt_death_killer_credit_overlay(&player, matched);
		if (!yt_game_write_player(&session->door->game, (int)killer,
		    &player, error))
			return false;
	}
	self = killer == (float)victim_record;
	if (!self
	    && !yt_game_read_player(&session->door->game, victim_record,
	    &player, error))
		return false;
	if (!yt_death_kill_news_row((const uint8_t *)session->player.name,
	    strlen(session->player.name), victim_name, victim_name_length, self,
	    row, sizeof(row), &row_length)
	    || !yt_news_append_bytes(row, row_length, error))
		return false;
	if (!self && matched_ports != 0) {
		if (!yt_death_port_news_row(victim_name, victim_name_length,
		    matched, row, sizeof(row), &row_length)
		    || !yt_news_append_bytes(row, row_length, error))
			return false;
	}
	if (victim_record == current_player_record) {
		char cached_name[sizeof(session->player.name)];

		memcpy(cached_name, session->player.name, sizeof(cached_name));
		session->player = victim;
		memcpy(session->player.name, cached_name, sizeof(cached_name));
	}
	if (!yt_database_flush(&session->door->game.database, error))
		return false;
	if (victim_record == current_player_record && wait_for_current) {
		if (!session_wait(session, 5.0, "common fatal wait", error))
			return false;
		session->fatal_wait_complete = true;
	}
	return true;
}
