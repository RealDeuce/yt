#include "yt_session_internal.h"

#include "qb.h"
#include "yt_port_math.h"

#include <stdio.h>
#include <string.h>

bool
yt_session_players_are_friendly(struct yt_session *session,
    int candidate_record, bool *friendly, struct yt_error *error)
{
	struct yt_player current;
	struct yt_player candidate;
	int current_record;
	int last_player_record;

	if (session == NULL || friendly == NULL)
		return false;
	*friendly = false;
	current_record = session_record(session);
	last_player_record = (int)session_sector_offset(session);
	if (candidate_record < YT_PLAYER_FIRST_RECORD
	    || candidate_record > last_player_record
	    || current_record < YT_PLAYER_FIRST_RECORD
	    || current_record > last_player_record)
		return true;
	if (candidate_record == current_record) {
		*friendly = true;
		return true;
	}
	if (!yt_game_read_player(&session->door->game, current_record,
	    &current, error))
		return false;
	if (current.team == 0.0f)
		return true;
	if (!yt_game_read_player(&session->door->game, candidate_record,
	    &candidate, error))
		return false;
	*friendly = candidate.team == current.team;
	return true;
}

static bool
same_team(struct yt_session *session, int other_record,
    struct yt_error *error)
{
	bool friendly;

	if (!yt_session_players_are_friendly(session, other_record, &friendly,
	    error))
		return false;
	return friendly;
}

bool
yt_session_sector_force_is_friendly(struct yt_session *session,
    const struct yt_sector *sector, struct yt_error *error)
{
	enum yt_sector_force_route route;
	int owner;

	route = yt_sector_force_route(sector->fighters, sector->fighter_owner,
	    session_record(session), &owner);
	if (route == YT_SECTOR_FORCE_FRIENDLY)
		return true;
	if (route == YT_SECTOR_FORCE_OWNER_GET)
		return same_team(session, owner, error);
	return false;
}

static bool
scanner_read_sector(struct yt_session *session, float logical_sector,
    struct yt_sector *sector, struct yt_error *error)
{
	struct yt_record record;
	uint32_t physical = session_sector_basic_record(session, logical_sector);

	if (!yt_database_read(&session->door->game.database, (size_t)physical,
	    &record, error))
		return false;
	yt_sector_decode(sector, &record);
	return true;
}

static bool
scanner_read_port(struct yt_session *session, float logical_port,
    struct yt_port *port, uint32_t *physical_record, struct yt_error *error)
{
	struct yt_record record;
	uint32_t physical = session_port_basic_record(session, logical_port);

	if (!yt_database_read(&session->door->game.database, (size_t)physical,
	    &record, error))
		return false;
	yt_port_decode(port, &record);
	if (physical_record != NULL)
		*physical_record = physical;
	return true;
}

static bool
scanner_write_port(struct yt_session *session, uint32_t physical_record,
    struct yt_port *port, struct yt_error *error)
{
	yt_record_set_number_if_changed(&port->record, YT_F93, port->sector);
	return yt_database_write(&session->door->game.database,
	    (size_t)physical_record, &port->record, error);
}

static bool
scanner_read_planet(struct yt_session *session, uint32_t physical_record,
    struct yt_planet *planet, struct yt_error *error)
{
	struct yt_record record;

	if (!yt_database_read(&session->door->game.database,
	    (size_t)physical_record, &record, error))
		return false;
	yt_planet_decode(planet, &record);
	return true;
}

bool
session_read_player_expression(struct yt_session *session,
    float basic_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_record record;
	uint32_t physical = qb_brun_random_record_number(basic_record);

	if (!yt_database_read(&session->door->game.database, (size_t)physical,
	    &record, error))
		return false;
	yt_player_decode(player, &record);
	return true;
}

static void
scanner_cache_hostile_sector(struct yt_session *session,
    const struct yt_sector *sector)
{
	session->combat.deployed_fighters = (double)sector->fighters;
	session->combat.hostile_owner = sector->fighter_owner;
}

static bool
display_sector_one(struct yt_session *session, float logical_sector,
    struct yt_sector_pager_state *private_pager, struct yt_error *error)
{
	struct yt_sector sector;
	char sector_number[64];
	uint8_t row[512];
	size_t row_length;
	size_t slot;
	int basic;
	bool first_visible = true;
	bool first_warp = true;

	session->navigation.current_sector_physical_record = qb_single_add(
	    session_sector_offset(session), logical_sector);
	if (!scanner_read_sector(session, logical_sector, &sector, error))
		return false;
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "sector leading blank", error))
		return false;
	if (qb_str_single(sector_number, sizeof(sector_number),
	    logical_sector) < 0)
		return false;
	row_length = sizeof("Sector:") - 1U;
	memcpy(row, "Sector:", row_length);
	memcpy(row + row_length, sector_number, strlen(sector_number));
	row_length += strlen(sector_number);
	if (!session_present_text(session, row, row_length,
	    SESSION_PRESENT_LINE, "sector number row", error))
		return false;
	yt_sector_pager_add(private_pager, 1.0f);
	if ((logical_sector == session->disruption_sectors[0]
	    || logical_sector == session->disruption_sectors[1])
	    && !session_attention_bytes(session,
	    (const uint8_t *)"** Space-time disruption detected! **",
	    strlen("** Space-time disruption detected! **"),
	    "sector disruption attention", error))
		return false;
	if (logical_sector == session->disruption_sectors[0]
	    || logical_sector == session->disruption_sectors[1])
		yt_sector_pager_add(private_pager, 1.0f);
	if (sector.mines != 0.0f) {
		if (!yt_sector_mine_warning_row(sector.mines, row,
		    sizeof(row) - 1U, &row_length))
			return false;
		row[row_length] = '\0';
		if (!session_attention_bytes(session, row, row_length,
		    "sector mine attention", error))
			return false;
		for (slot = 0; slot < 3; ++slot) {
			if (!session_sound(session, YT_SOUND_CUE_ACTION,
			    "sector mine follow-up sound", error))
				return false;
		}
		yt_sector_pager_add(private_pager, 1.0f);
	}
	if (sector.port > 0.0f) {
		struct yt_port port;
		uint32_t physical_port;

		if (!scanner_read_port(session, sector.port, &port,
		    &physical_port, error)
		    || !yt_sector_port_row(&port, row, sizeof(row), &row_length)
		    || !session_present_text(session, row, row_length,
		    SESSION_PRESENT_LINE, "sector port row", error))
			return false;
		port.sector = logical_sector;
		if (!scanner_write_port(session, physical_port, &port,
		    error))
			return false;
		yt_sector_pager_add(private_pager, 1.0f);
	}
	if (!scanner_read_sector(session, logical_sector, &sector, error))
		return false;
	if (sector.planet > 0.0f) {
		struct yt_planet planet;
		uint32_t physical_planet = session_planet_basic_record(session,
		    sector.planet);
		float saved_foreground;

		if (!yt_session_update_planet_physical(session, physical_planet,
		    &planet, NULL, error)
		    || !scanner_read_planet(session, physical_planet, &planet,
		    error)
		    || !yt_sector_planet_row(&planet, row, sizeof(row),
		    &row_length))
			return false;
		saved_foreground = session->presentation.foreground;
		session_set_foreground(session, 3.0f);
		if (!session_present_text(session, row, row_length,
		    SESSION_PRESENT_BOLD_LINE, "sector planet row", error))
			return false;
		session_set_foreground(session, saved_foreground);
		yt_sector_pager_add(private_pager, 1.0f);
		if (!scanner_read_sector(session, logical_sector, &sector, error))
			return false;
	}
	for (basic = YT_PLAYER_FIRST_RECORD;
	    basic <= (int)session_sector_offset(session); ++basic) {
		float random_value;

		if (!yt_sector_candidate_eligible(basic, session_record(session),
		    yt_player_cache_value(&session->player_cache, basic,
		    YT_PLAYER_CACHE_SECTOR), logical_sector))
			continue;
		if (!yt_random_next(&session->door->game.random, &random_value,
		    error))
			return false;
		if (yt_sector_cloak_revealed(random_value,
		    session->player_cache.cloak[basic])) {
			static const uint8_t shimmer[] =
			    "You detect the shimmering of a cloaking device!";

			if (!session_present_text(session, shimmer,
			    sizeof(shimmer) - 1U, SESSION_PRESENT_BOLD_LINE,
			    "sector cloak shimmer row", error))
				return false;
			yt_sector_pager_add(private_pager, 1.0f);
			(void)yt_player_cache_set(&session->player_cache, basic,
			    YT_PLAYER_CACHE_CLOAK, 0.0f);
			if (!session_sound(session, YT_SOUND_CUE_ACTION,
			    "sector cloak-reveal sound", error))
				return false;
		}
		if (session->player_cache.cloak[basic] == 0.0f) {
			struct yt_player other;

			yt_sector_pager_add(private_pager, 1.0f);
			if (first_visible) {
				static const uint8_t heading[] = "Other Ships: ";

				if (!session_present_text(session, heading,
				    sizeof(heading) - 1U,
				    SESSION_PRESENT_BOLD_LINE,
				    "sector other-ships heading", error))
					return false;
				first_visible = false;
			}
			if (!yt_game_read_player(&session->door->game, basic,
			    &other, error)
			    || !yt_sector_player_row(&other, row, sizeof(row),
			    &row_length)
			    || !session_present_text(session, row, row_length,
			    SESSION_PRESENT_LINE, "sector visible-player row", error))
				return false;
		}
	}
	if (!scanner_read_sector(session, logical_sector, &sector, error))
		return false;
	scanner_cache_hostile_sector(session, &sector);
	if (sector.fighters != 0.0f) {
		static const uint8_t heading[] = "Fighters in sector:";
		struct yt_player owner;
		struct yt_team team;
		const struct yt_player *owner_pointer = NULL;
		const struct yt_team *team_pointer = NULL;
		bool scratch_changed;
		bool owner_team_nonzero = false;
		size_t scratch_length = session->combat.hostile_owner_label_length;

		if (!session_present_text(session, heading,
		    sizeof(heading) - 1U, SESSION_PRESENT_BOLD_RAW,
		    "sector fighter heading", error))
			return false;
		if (sector.fighter_owner != -1.0f
		    && sector.fighter_owner != -2.0f
		    && sector.fighter_owner != (float)session_record(session)) {
			if (!session_read_player_expression(session,
			    sector.fighter_owner,
			    &owner, error))
				return false;
			owner_pointer = &owner;
			owner_team_nonzero = owner.team != 0.0f;
			if (owner_team_nonzero) {
				uint8_t owner_name[YT_TEXT_FIELD_SIZE];
				size_t owner_name_length;
				char team_number[64];
				int team_number_length;
				static const uint8_t team_prefix[] = " Team [";

				owner_name_length = yt_player_stored_name(&owner,
				    owner_name);
				team_number_length = qb_str_single(team_number,
				    sizeof(team_number), owner.team);
				if (team_number_length < 1
				    || owner_name_length + sizeof(team_prefix) - 1U
				    + (size_t)team_number_length >
				    sizeof(session->combat.hostile_owner_label))
					return false;
				memcpy(session->combat.hostile_owner_label, owner_name,
				    owner_name_length);
				scratch_length = owner_name_length;
				memcpy(session->combat.hostile_owner_label + scratch_length,
				    team_prefix, sizeof(team_prefix) - 1U);
				scratch_length += sizeof(team_prefix) - 1U;
				memcpy(session->combat.hostile_owner_label + scratch_length,
				    team_number + 1,
				    (size_t)team_number_length - 1U);
				scratch_length += (size_t)team_number_length - 1U;
				session->combat.hostile_owner_label[scratch_length++] = ']';
				session->combat.hostile_owner_label_length = scratch_length;
				if (!yt_game_read_team(&session->door->game,
				    (int)owner.team, &team, error))
					return false;
				team_pointer = &team;
			}
		}
		if (!yt_sector_fighter_row(&sector, session_record(session),
		    owner_pointer, team_pointer, row, sizeof(row), &row_length,
		    session->combat.hostile_owner_label,
		    sizeof(session->combat.hostile_owner_label), &scratch_length,
		    &scratch_changed)
		    || !session_present_text(session, row, row_length,
		    SESSION_PRESENT_LINE, "sector fighter owner row", error))
			return false;
		if (scratch_changed)
			session->combat.hostile_owner_label_length = scratch_length;
		yt_sector_pager_add(private_pager,
		    owner_team_nonzero ? 3.0f : 2.0f);
	}
	if (!session_present_text(session, (const uint8_t *)"Warps lead to:",
	    sizeof("Warps lead to:") - 1U, SESSION_PRESENT_RAW,
	    "sector warp heading", error))
		return false;
	for (slot = 0; slot < YT_ARRAY_LEN(sector.warps); ++slot) {
		if (sector.warps[slot] != 0.0f) {
			char warp[64];
			int warp_size;
			size_t fragment_length = 0U;

			warp_size = qb_str_single(warp, sizeof(warp),
			    sector.warps[slot]);
			if (warp_size < 0)
				return false;
			if (!first_warp)
				row[fragment_length++] = ',';
			memcpy(row + fragment_length, warp, (size_t)warp_size);
			fragment_length += (size_t)warp_size;
			if (!session_present_text(session, row, fragment_length,
			    SESSION_PRESENT_RAW,
			    "sector warp target", error))
				return false;
			first_warp = false;
		}
	}
	if (!session_present_text(session, NULL, 0U,
	    SESSION_PRESENT_LINE, "sector warp terminator", error))
		return false;
	yt_sector_pager_add(private_pager, 1.0f);
	if (yt_sector_pager_finish_sector(private_pager)) {
		if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
		    "sector private-pause blank", error))
			return false;
		session_set_foreground(session, 7.0f);
		if (!session_present_text(session,
		    (const uint8_t *)"[ Pause ]", strlen("[ Pause ]"),
		    SESSION_PRESENT_BOLD_LINE, "sector private-pause prompt",
		    error))
			return false;
		if (!session_wait(session, 15.0,
		    "sector private-pause wait", error))
			return false;
		session_set_foreground(session, 1.0f);
	}
	return true;
}

bool
yt_session_display_sector(struct yt_session *session, bool adjacent,
    struct yt_error *error)
{
	float current;
	struct yt_sector_pager_state private_pager;
	float caller_warps[6];
	float targets[6];
	float saved_foreground = session->presentation.foreground;
	size_t target_count;
	size_t slot;

	yt_sector_pager_begin(&private_pager);
	if (!adjacent) {
		session_set_foreground(session, 1.0f);
		if (!yt_game_read_player(&session->door->game,
		    session_record(session), &session->player, error))
			return false;
		current = session->player.sector;
		if (!display_sector_one(session, current, &private_pager, error)
		    || !yt_game_read_player(&session->door->game,
		    session_record(session), &session->player, error))
			return false;
		session_set_foreground(session, saved_foreground);
		return true;
	}
	memcpy(caller_warps, session->navigation.current_warps,
	    sizeof(session->navigation.current_warps));
	target_count = yt_sector_sensor_targets(caller_warps, targets);
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "adjacent-sector sensor leading blank", error))
		return false;
	session_set_foreground(session, 7.0f);
	if (!session_present_text(session,
	    (const uint8_t *)"[ Sensors Activated ]",
	    strlen("[ Sensors Activated ]"), SESSION_PRESENT_BOLD_LINE,
	    "adjacent-sector sensor heading", error))
		return false;
	if (!session_sound(session, YT_SOUND_CUE_ACTION,
	    "adjacent-sector sensor sound", error))
		return false;
	session_set_foreground(session, 1.0f);
	for (slot = 0; slot < target_count; ++slot) {
		if (!display_sector_one(session, targets[slot],
		    &private_pager, error))
			return false;
	}
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "adjacent-sector sensor ending blank", error))
		return false;
	session_set_foreground(session, 7.0f);
	if (!session_present_text(session,
	    (const uint8_t *)"[ End Sensor Scan ]",
	    strlen("[ End Sensor Scan ]"), SESSION_PRESENT_BOLD_LINE,
	    "adjacent-sector sensor ending", error)
	    || !yt_game_read_player(&session->door->game,
	    session_record(session), &session->player, error))
		return false;
	session_set_foreground(session, saved_foreground);
	return true;
}

bool
yt_session_display_current_sector_cached(struct yt_session *session,
    struct yt_error *error)
{
	struct yt_sector_pager_state private_pager;
	float saved_foreground = session->presentation.foreground;
	float current = session->player.sector;
	bool ok;

	yt_sector_pager_begin(&private_pager);
	session_set_foreground(session, 1.0f);
	ok = display_sector_one(session, current, &private_pager, error)
	    && yt_game_read_player(&session->door->game,
	    session_record(session), &session->player, error);
	if (ok) {
		session_set_foreground(session, saved_foreground);
	}
	return ok;
}
