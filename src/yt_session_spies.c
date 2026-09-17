#include "yt_session_internal.h"

#include "qb.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

bool
yt_session_list_spies(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t none[] = "You do not have any spies!";
	size_t index;

	if (session->spies.count == 0)
		return session_present_alert(session, none, sizeof(none) - 1U,
		    "active-spy none notice", error);
	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "active-spy leading blank", error))
		return false;
	for (index = 0U; index < (size_t)session->spies.count; ++index) {
		char counter[64];
		char target[64];
		uint8_t row[160];
		int counter_length;
		int target_length;
		int row_length;

		counter_length = qb_str_single(counter, sizeof(counter),
		    (float)(index + 1U));
		target_length = qb_str_integer(target, sizeof(target),
		    (int16_t)session->spies.sectors[index]);
		if (counter_length < 0 || target_length < 0)
			return false;
		row_length = snprintf((char *)row, sizeof(row),
		    "Spy #%.*s will hunt in sector%.*s.", counter_length,
		    counter, target_length, target);
		if (row_length < 0 || (size_t)row_length >= sizeof(row))
			return false;
		session->presentation.bold = true;
		if (!session_present_paged_fragment(session, row,
		    (size_t)row_length))
			return false;
	}
	return true;
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

	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "spy direct output", error))
		return false;
	if (session->spies.found)
		return true;
	session->spies.found = true;
	session->spies.last_findings[spy] = sector;
	if (!session_sound(session, YT_SOUND_CUE_SPY, "spy finding sound", error))
		return false;
	amount = qb_str_single(spy_number, sizeof(spy_number), (float)(spy + 1U));
	if (amount < 1)
		return false;
	spy_number[0] = '#';
	if (qb_str_single(sector_number, sizeof(sector_number),
	    (float)sector) < 0)
		return false;
	if (!append_bytes(row, sizeof(row), &length,
	    prefix, sizeof(prefix) - 1U))
		return false;
	if (!append_bytes(row, sizeof(row), &length,
	    spy_number, (size_t)amount))
		return false;
	if (!append_bytes(row, sizeof(row), &length,
	    middle, sizeof(middle) - 1U))
		return false;
	if (!append_bytes(row, sizeof(row), &length,
	    sector_number, strlen(sector_number)))
		return false;
	if (!append_bytes(row, sizeof(row), &length, ":", 1U))
		return false;
	if (!session_present_text(session, row, length,
	    SESSION_PRESENT_BOLD_LINE, "spy direct output", error))
		return false;
	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "spy direct output", error))
		return false;
	return true;
}

static void
spy_clear_cached_cloak(struct yt_session *session, int player_record)
{
	(void)yt_player_cache_set_cloak(&session->player_cache, player_record,
	    0.0f);
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
	int disruption_sectors[2] = {
		session->disruption_sectors[0],
		session->disruption_sectors[1]
	};
	int last_player_record = session_sector_offset(session);
	int current_player_record = session_record(session);
	int active_spies = session->spies.count;
	int spy_index;

	if (active_spies == 0)
		return true;
	for (spy_index = 0; spy_index < active_spies; ++spy_index) {
		struct yt_sector sector;
		size_t spy = (size_t)spy_index;
		int sector_number = session->spies.sectors[spy];
		bool first_ship = true;
		int candidate;

		session_set_foreground(session, 7);
		if (!(sector_number == session->spies.last_findings[spy]
		    && sector_number != 0)) {
			session->spies.found = false;
			if (!session_read_sector(session, sector_number, &sector,
			    error))
				return false;
			if (sector_number == disruption_sectors[0]
			    || sector_number == disruption_sectors[1]) {
				if (!spy_first_finding(session, spy, sector_number,
				    error))
					return false;
				if (!session_attention_bytes(session, disruption,
				    sizeof(disruption) - 1U, "spy attention row", error))
					return false;
			}
			if (sector.mines != 0.0f) {
				uint8_t row[128];
				size_t length;

				if (!spy_first_finding(session, spy, sector_number,
				    error))
					return false;
				if (!yt_sector_mine_warning_row(sector.mines, row,
				    sizeof(row), &length))
					return false;
				if (!session_attention_bytes(session, row, length,
				    "spy attention row", error))
					return false;
			}
			if (sector.planet > 0) {
				struct yt_planet planet;
				struct yt_planet updated_planet;
				uint8_t row[160];
				size_t length;
				uint32_t physical = session_planet_basic_record(session,
				    sector.planet);

				if (!yt_session_update_planet_physical(session, physical,
				    &updated_planet, NULL, error))
					return false;
				if (!read_planet_physical(session, physical, &planet,
				    error))
					return false;
				if (!spy_first_finding(session, spy, sector_number,
				    error))
					return false;
				if (!yt_sector_planet_row(&planet, row, sizeof(row),
				    &length))
					return false;
				if (!session_present_text(session, row, length,
				    SESSION_PRESENT_BOLD_LINE, "spy direct output", error))
					return false;
				if (!session_read_sector(session, sector_number, &sector,
				    error))
					return false;
			}
			for (candidate = 2; candidate <= last_player_record;
			    ++candidate) {
				float draw;
				float cloak;
				bool detected;

				if (!yt_sector_candidate_eligible(candidate,
				    current_player_record,
				    yt_player_cache_sector(&session->player_cache,
				    candidate), sector_number))
					continue;
				if (!yt_random_next(&session->door->game.random, &draw,
				    error))
					return false;
				cloak = yt_player_cache_cloak(&session->player_cache,
				    candidate);
				detected = yt_sector_cloak_revealed(draw, cloak);
				if (detected) {
					if (!spy_first_finding(session, spy,
					    sector_number, error))
						return false;
					if (!session_present_text(session, cloak_notice,
					    sizeof(cloak_notice) - 1U,
					    SESSION_PRESENT_BOLD_LINE, "spy direct output",
					    error))
						return false;
					spy_clear_cached_cloak(session, candidate);
					if (!session_sound(session, YT_SOUND_CUE_ACTION,
					    "spy cloak sound", error))
						return false;
				}
				if (yt_player_cache_cloak(&session->player_cache,
				    candidate) != 0.0f
				    && !detected)
					continue;
				if (!spy_first_finding(session, spy, sector_number, error))
					return false;
				if (first_ship) {
					if (!session_present_text(session, ship_heading,
					    sizeof(ship_heading) - 1U,
					    SESSION_PRESENT_BOLD_LINE, "spy direct output",
					    error))
						return false;
					first_ship = false;
				}
				{
					struct yt_player player;
					uint8_t row[256];
					size_t length;

					if (!yt_game_read_player(&session->door->game,
					    candidate, &player, error))
						return false;
					if (!yt_sector_player_row(&player, row,
					    sizeof(row), &length))
						return false;
					session->presentation.bold = true;
					if (!session_present_text(session, row, length,
					    SESSION_PRESENT_LINE, "spy direct output", error))
						return false;
				}
			}
			if (!session_read_sector(session, sector_number, &sector,
			    error))
				return false;
			if (sector.fighters != 0.0f
			    && sector.fighter_owner != current_player_record) {
				struct yt_sector refreshed;
				struct yt_sector displayed;
				struct yt_player owner_player;
				struct yt_team team;
				const struct yt_player *owner_pointer = NULL;
				const struct yt_team *team_pointer = NULL;
				uint8_t row[256];
				uint8_t scratch[160];
				size_t length;
				size_t scratch_length = 0U;
				bool scratch_changed;
				int owner = sector.fighter_owner;

				if (!session_read_sector(session, sector_number, &refreshed,
				    error))
					return false;
				if (!spy_first_finding(session, spy, sector_number,
				    error))
					return false;
				if (!session_present_text(session, fighter_heading,
				    sizeof(fighter_heading) - 1U,
				    SESSION_PRESENT_BOLD_RAW, "spy direct output", error))
					return false;
				session->presentation.bold = true;
				displayed = refreshed;
				displayed.fighter_owner = owner;
				if (owner != -1 && owner != -2) {
					if (!yt_game_read_player(&session->door->game,
					    owner, &owner_player, error))
						return false;
					owner_pointer = &owner_player;
					if (owner_player.team != 0) {
						if (!yt_game_read_team(&session->door->game,
						    owner_player.team, &team, error))
							return false;
						team_pointer = &team;
					}
				}
				if (!yt_sector_fighter_row(&displayed,
				    current_player_record, owner_pointer, team_pointer,
				    row, sizeof(row), &length, scratch, sizeof(scratch),
				    &scratch_length, &scratch_changed))
					return false;
				if (!session_present_text(session, row, length,
				    SESSION_PRESENT_LINE, "spy direct output", error))
					return false;
			}
		}
		if (session->spies.found) {
			if (!session_present_text(session, NULL, 0U,
			    SESSION_PRESENT_LINE, "spy direct output", error))
				return false;
			if (!session_press_any_key(session, true, error))
				return false;
		}
		if (!session_read_sector(session, sector_number, &sector, error))
			return false;
		{
			int destinations[6];
			size_t slot;

			for (slot = 0U; slot < YT_ARRAY_LEN(destinations); ++slot)
				destinations[slot] = sector.warps[slot];
			for (;;) {
				float draw;
				int selected;

				if (!yt_random_next(&session->door->game.random, &draw,
				    error))
					return false;
				selected = (int)floorf(qb_single_multiply(draw, 6.0f));
				if (destinations[selected] != 0) {
					session->spies.sectors[spy] =
					    destinations[selected];
					break;
				}
			}
		}
	}
	session_set_foreground(session, 0);
	return true;
}
