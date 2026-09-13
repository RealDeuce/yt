#include "yt_session_internal.h"

#include "qb.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static bool
spy_failure(struct yt_error *error, const char *operation)
{
	if (error != NULL) {
		error->status = YT_RANGE;
		error->system_error = 0;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
		error->path[0] = '\0';
	}
	return false;
}

static float
single_mul(float left, float right)
{
	volatile float result = left * right;

	return result;
}

static bool
append_bytes(uint8_t *row, size_t capacity, size_t *length,
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
spy_line(struct yt_session *session, const uint8_t *text, size_t length,
    enum session_present_text_kind kind, struct yt_error *error)
{
	return session_present_text(session, text, length, kind,
	    "spy direct output", error);
}

static bool
spy_first_finding(struct yt_session *session, size_t spy, int sector,
    struct yt_error *error)
{
	static const uint8_t prefix[] = "*** RADIO MESSAGE FROM SPY ";
	static const uint8_t middle[] =
	    "! The following was found in sector";
	uint8_t row[256];
	char spy_number[64];
	char sector_number[64];
	size_t length = 0U;
	int amount;

	if (!spy_line(session, NULL, 0U, SESSION_PRESENT_LINE, error))
		return false;
	if (session->spy_found)
		return true;
	session->spy_found = true;
	session->spy_markers[spy] = sector;
	if (!session_sound(session, 9.0f, "spy finding sound", error))
		return false;
	amount = qb_str_single(spy_number, sizeof(spy_number), (float)(spy + 1U));
	if (amount < 1)
		return false;
	spy_number[0] = '#';
	if (qb_str_single(sector_number, sizeof(sector_number),
	    (float)sector) < 0
	    || !append_bytes(row, sizeof(row), &length,
	    prefix, sizeof(prefix) - 1U)
	    || !append_bytes(row, sizeof(row), &length,
	    spy_number, (size_t)amount)
	    || !append_bytes(row, sizeof(row), &length,
	    middle, sizeof(middle) - 1U)
	    || !append_bytes(row, sizeof(row), &length,
	    sector_number, strlen(sector_number))
	    || !append_bytes(row, sizeof(row), &length, ":", 1U)
	    || !spy_line(session, row, length, SESSION_PRESENT_BOLD_LINE, error)
	    || !spy_line(session, NULL, 0U, SESSION_PRESENT_LINE, error))
		return false;
	return true;
}

static void
spy_clear_cached_cloak(struct yt_session *session, int player_record)
{
	static const uint8_t zero[4] = {0};

	(void)yt_player_cache_set_raw(&session->player_cache, player_record,
	    YT_PLAYER_CACHE_CLOAK, zero);
}

static bool
spy_update_planet(struct yt_session *session, float link,
    struct yt_error *error)
{
	uint32_t physical = (uint32_t)yt_planet_basic_record(
	    &session->door->game.config, (int)link);
	struct yt_planet planet;

	return planet_update_cached_physical(session, physical, &planet, NULL,
	    error);
}

static bool
spy_read_planet(struct yt_session *session, float link,
    struct yt_planet *planet, struct yt_error *error)
{
	uint32_t physical = (uint32_t)yt_planet_basic_record(
	    &session->door->game.config, (int)link);

	return read_planet_physical(session, physical, planet, error);
}

static bool
spy_read_player(struct yt_session *session, float record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_record raw;
	uint32_t physical = qb_brun_random_record_number(record);

	if (!yt_database_read(&session->door->game.database, physical, &raw,
	    error))
		return false;
	yt_player_decode(player, &raw);
	return true;
}

static bool
spy_read_team(struct yt_session *session, float team,
    struct yt_sector *overlay, struct yt_error *error)
{
	struct yt_record raw;
	uint32_t physical = (uint32_t)yt_sector_basic_record(
	    &session->door->game.config, (int)team);

	if (!yt_database_read(&session->door->game.database, physical, &raw,
	    error))
		return false;
	yt_sector_decode(overlay, &raw);
	return true;
}

bool
yt_session_spy_sweep(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t disruption[] =
	    "** Space-time disruption detected! **";
	static const uint8_t cloak_notice[] =
	    "The spy detected the shimmering of a cloaking device!";
	static const uint8_t ship_heading[] = "Other Ships: ";
	static const uint8_t fighter_heading[] = "Fighters in sector:";
	float disruption_sectors[2] = {
		session->disruption_sectors[0],
		session->disruption_sectors[1]
	};
	float last_player_record = session_sector_offset(session);
	int current_player_record = session_record(session);
	int active_spies = session->spy_count;
	int spy_index;

	if (active_spies == 0)
		return true;
	for (spy_index = 0; spy_index < active_spies; ++spy_index) {
		struct yt_sector sector;
		bool overflow;
		size_t spy = (size_t)spy_index;
		int sector_number = session->spy_sectors[spy];
		bool first_ship = true;
		int candidate;

		session_set_foreground(session, 7.0f);
		if (!(sector_number == session->spy_markers[spy]
		    && sector_number != 0)) {
			session->spy_found = false;
			if (!session_read_sector(session, sector_number, &sector,
			    error))
				return false;
			if ((float)sector_number == disruption_sectors[0]
			    || (float)sector_number == disruption_sectors[1]) {
				if (!spy_first_finding(session, spy, sector_number, error)
				    || !session_attention_bytes(session, disruption,
				    sizeof(disruption) - 1U, "spy attention row", error))
					return false;
			}
			if (sector.mines != 0.0f) {
				uint8_t row[128];
				size_t length;

				if (!spy_first_finding(session, spy, sector_number, error)
				    || !yt_sector_mine_warning_row(sector.mines, row,
				    sizeof(row), &length)
				    || !session_attention_bytes(session, row, length,
				    "spy attention row", error))
					return false;
			}
			if (sector.planet > 0.0f) {
				struct yt_planet planet;
				uint8_t row[160];
				size_t length;

				if (!spy_update_planet(session, sector.planet, error)
				    || !spy_read_planet(session, sector.planet, &planet,
				    error)
				    || !spy_first_finding(session, spy, sector_number, error)
				    || !yt_sector_planet_row(&planet, row, sizeof(row),
				    &length, error)
				    || !spy_line(session, row, length,
				    SESSION_PRESENT_BOLD_LINE, error)
				    || !session_read_sector(session, sector_number, &sector,
				    error))
					return false;
			}
			for (candidate = 2;
			    (float)candidate <= last_player_record; ++candidate) {
				float draw;
				float cloak;
				bool detected;

				if (!yt_player_cache_contains(candidate))
					return spy_failure(error,
					    "last player cache aliases adjacent memory");
				if (!yt_sector_candidate_eligible(candidate,
				    current_player_record,
				    yt_player_cache_value(&session->player_cache,
				    candidate, YT_PLAYER_CACHE_SECTOR),
				    (float)sector_number))
					continue;
				if (!yt_random_next(&session->door->game.random, &draw,
				    error))
					return false;
				cloak = yt_player_cache_value(&session->player_cache,
				    candidate, YT_PLAYER_CACHE_CLOAK);
				detected = yt_sector_cloak_revealed(draw, cloak);
				if (detected) {
					if (!spy_first_finding(session, spy,
					    sector_number, error)
					    || !spy_line(session, cloak_notice,
					    sizeof(cloak_notice) - 1U,
					    SESSION_PRESENT_BOLD_LINE, error))
						return false;
					spy_clear_cached_cloak(session, candidate);
					if (!session_sound(session, 4.0f,
					    "spy cloak sound", error))
						return false;
				}
				if (yt_player_cache_value(&session->player_cache,
				    candidate, YT_PLAYER_CACHE_CLOAK) != 0.0f
				    && !detected)
					continue;
				if (!spy_first_finding(session, spy, sector_number, error))
					return false;
				if (first_ship) {
					if (!spy_line(session, ship_heading,
					    sizeof(ship_heading) - 1U,
					    SESSION_PRESENT_BOLD_LINE, error))
						return false;
					first_ship = false;
				}
				{
					struct yt_player player;
					uint8_t row[256];
					size_t length;

					if (!spy_read_player(session, (float)candidate,
					    &player, error)
					    || !yt_sector_player_row(&player, row,
					    sizeof(row), &length, error))
						return false;
					yt_present_set_bold(&session->presentation, 1.0f);
					if (!spy_line(session, row, length,
					    SESSION_PRESENT_LINE, error))
						return false;
				}
			}
			if (!session_read_sector(session, sector_number, &sector,
			    error))
				return false;
			if (sector.fighters != 0.0f
			    && sector.fighter_owner != (float)current_player_record) {
				struct yt_sector refreshed;
				struct yt_sector displayed;
				struct yt_player owner_player;
				struct yt_sector team_overlay;
				const struct yt_player *owner_pointer = NULL;
				const struct yt_sector *team_pointer = NULL;
				uint8_t row[256];
				uint8_t scratch[160];
				size_t length;
				size_t scratch_length = 0U;
				bool scratch_changed;
				float owner = sector.fighter_owner;

				if (!session_read_sector(session, sector_number, &refreshed,
				    error)
				    || !spy_first_finding(session, spy, sector_number, error)
				    || !spy_line(session, fighter_heading,
				    sizeof(fighter_heading) - 1U,
				    SESSION_PRESENT_BOLD_RAW, error))
					return false;
				yt_present_set_bold(&session->presentation, 1.0f);
				displayed = refreshed;
				displayed.fighter_owner = owner;
				if (owner != -1.0f && owner != -2.0f) {
					uint8_t ignored_name[YT_TEXT_FIELD_SIZE];
					size_t ignored_length;

					if (!spy_read_player(session, owner, &owner_player,
					    error)
					    || !yt_player_stored_name(&owner_player,
					    ignored_name, &ignored_length, error))
						return false;
					owner_pointer = &owner_player;
					if (owner_player.team != 0.0f) {
						if (!spy_read_team(session,
						    owner_player.team, &team_overlay, error))
							return false;
						team_pointer = &team_overlay;
					}
				}
				if (!yt_sector_fighter_row(&displayed,
				    current_player_record, owner_pointer, team_pointer,
				    row, sizeof(row), &length, scratch, sizeof(scratch),
				    &scratch_length, &scratch_changed, error)
				    || !spy_line(session, row, length,
				    SESSION_PRESENT_LINE, error))
					return false;
			}
		}
		if (session->spy_found) {
			if (!spy_line(session, NULL, 0U, SESSION_PRESENT_LINE, error)
			    || !session_press_any_key(session, true, error))
				return false;
		}
		if (!session_read_sector(session, sector_number, &sector, error))
			return false;
		{
			int32_t warps[6];
			size_t slot;

			for (slot = 0U; slot < YT_ARRAY_LEN(warps); ++slot) {
				warps[slot] = qb_cint_mbf32(sector.record.bytes
				    + YT_F105 + 4U * slot, 0U, &overflow);
				if (overflow)
					return spy_failure(error,
					    "active spy warp CINT");
			}
			for (;;) {
				float draw;
				int selected;

				if (!yt_random_next(&session->door->game.random, &draw,
				    error))
					return false;
				selected = (int)floorf(single_mul(draw, 6.0f));
				if (selected < 0 || selected >= 6)
					return spy_failure(error,
					    "active spy RND slot");
				if (warps[selected] != 0) {
					session->spy_sectors[spy] = warps[selected];
					break;
				}
			}
		}
	}
	session_set_foreground(session, 0.0f);
	return true;
}
