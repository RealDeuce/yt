#include "yt_session_internal.h"

#include "qb.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

bool
yt_session_command_fighters(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t title[] = "<Drop/Take Fighters>";
	static const uint8_t union_refusal[] =
	    "You can't leave fighters in the Union (sectors 1-7)";
	static const uint8_t foreign_refusal[] =
	    "There are already fighters in this sector!";
	static const uint8_t prompt[] =
	    "Defend this sector with how many? ";
	static const uint8_t insufficient[] = "You don't have that many!";
	struct yt_sector first_sector;
	struct yt_sector accepted_sector;
	struct yt_player accepted_player;
	struct qb_val_result parsed;
	char response[160];
	char number[64];
	char row[160];
	uint8_t desired_raw[4];
	double available;
	double desired_integer;
	float desired;
	float delta;
	float remaining;
	int logical_sector;
	int amount;

	if (!session_present_paged_fragment(session, title, sizeof(title) - 1U)
	    || !session_reload_player(session, error))
		return false;
	if (session->player.sector < 8.0f)
		return session_present_alert(session, union_refusal,
		    sizeof(union_refusal) - 1U, "fighter Union refusal", error);
	logical_sector = (int)session->player.sector;
	if (!session_read_sector(session, logical_sector, &first_sector, error))
		return false;
	if (first_sector.fighters > 0.0f
	    && first_sector.fighter_owner != (float)session_record(session))
		return session_present_alert(session, foreign_refusal,
		    sizeof(foreign_refusal) - 1U,
		    "fighter foreign-force refusal", error);
	available = qb_double_add((double)first_sector.fighters,
	    (double)session->player.fighters);
	amount = qb_str_double(number, sizeof(number), available);
	if (amount < 0 || snprintf(row, sizeof(row),
	    "You have%s fighters available.", number) < 0)
		return false;
	if (!session_present_paged_fragment(session, (const uint8_t *)row,
	    strlen(row))
	    || !session_present_timed_paged_row(session, prompt,
	    sizeof(prompt) - 1U, "fighter desired-count prompt", error))
		return false;
	memset(response, 0, sizeof(response));
	if (!session_read_number_command(session, response, sizeof(response)))
		return false;
	if (memchr(response, '\0', sizeof(response)) == NULL) {
		if (error != NULL) {
			error->status = YT_RANGE;
			snprintf(error->operation, sizeof(error->operation), "%s",
			    "fighter desired-count response");
		}
		return false;
	}
	if (response[0] == '\0')
		return true;
	parsed = qb_val(response);
	if (parsed.overflow) {
		if (error != NULL) {
			error->status = YT_RANGE;
			snprintf(error->operation, sizeof(error->operation), "%s",
			    "fighter desired-count VAL");
		}
		return false;
	}
	desired_integer = floor(parsed.valid ? parsed.value : 0.0);
	desired = (float)desired_integer;
	if (qb_mbf32_encode(desired, desired_raw) == QB_MBF_OVERFLOW) {
		if (error != NULL) {
			error->status = YT_RANGE;
			snprintf(error->operation, sizeof(error->operation), "%s",
			    "fighter desired-count CSNG");
		}
		return false;
	}
	desired = qb_mbf32_decode(desired_raw);
	if (desired < 0.0f)
		return true;
	delta = qb_single_subtract(first_sector.fighters, desired);
	remaining = (float)qb_double_add((double)session->player.fighters,
	    (double)delta);
	if (remaining < 0.0f)
		return session_present_alert(session, insufficient,
		    sizeof(insufficient) - 1U, "fighter insufficient notice", error);
	if (!session_read_sector(session, logical_sector, &accepted_sector,
	    error)
	    || !yt_main_fighters_sector_overlay(&accepted_sector, desired_raw,
	    session_record(session))
	    || !session_write_sector(session, logical_sector, &accepted_sector,
	    error)
	    || !yt_game_read_player(&session->door->game,
	    session_record(session), &accepted_player, error)
	    || !yt_main_fighters_player_overlay(&accepted_player, remaining)
	    || !yt_game_write_player(&session->door->game,
	    session_record(session), &accepted_player, error))
		return false;
	amount = qb_str_single(number, sizeof(number), remaining);
	if (amount < 0 || snprintf(row, sizeof(row),
	    "Done.  You have%s fighters left.", number) < 0)
		return false;
	return session_present_paged_fragment(session, (const uint8_t *)row,
	    strlen(row))
	    && session_sound(session, 4.0f, "sector fighter sound", error);
}
