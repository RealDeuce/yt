#include "yt_session_internal.h"

#include "qb.h"

#include <stdio.h>
#include <string.h>

static bool
attack_deployed(struct yt_session *session, struct yt_sector *sector,
    struct yt_error *error)
{
	static const uint8_t heading[] = "<Attack>";
	static const uint8_t prompt[] = "Attack with how many fighters? ";
	static const uint8_t none[] = "You don't have any fighters!";
	char response[160];
	char available[64];
	char row[128];
	struct qb_val_result parsed;
	enum qb_mbf_status status;
	enum yt_hostile_attack_admission admission;
	double cached_ship_fighters;
	float commitment;
	uint8_t commitment_raw[4];

	if (!session_present_paged_fragment(session, heading, sizeof(heading) - 1U))
		return false;
	cached_ship_fighters = session->combat.ship_fighters;
	admission = yt_hostile_attack_admit((float)cached_ship_fighters, 0.0f);
	if (admission == YT_HOSTILE_ATTACK_NO_FIGHTERS)
		return session_present_alert(session, none, sizeof(none) - 1U,
		    "hostile Attack no fighters", error);
	if (!session_present_timed_paged_row(session, prompt, sizeof(prompt) - 1U,
	    "hostile Attack amount prompt", error)
	    || !session_read_number_command(session, response, sizeof(response)))
		return false;
	if (response[0] == '\0') {
		memset(&parsed, 0, sizeof(parsed));
		parsed.valid = true;
	}
	else
		parsed = qb_val(response);
	if (!parsed.valid || parsed.overflow) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "attack:VAL");
		}
		return false;
	}
	commitment = (float)parsed.value;
	status = qb_mbf32_encode(commitment, commitment_raw);
	if (status == QB_MBF_OVERFLOW) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "attack:amount-csng");
		}
		return false;
	}
	commitment = qb_mbf32_decode(commitment_raw);
	admission = yt_hostile_attack_admit((float)cached_ship_fighters,
	    commitment);
	if (admission == YT_HOSTILE_ATTACK_TOO_MANY) {
		if (qb_str_double(available, sizeof(available),
		    cached_ship_fighters) < 0
		    || snprintf(row, sizeof(row), "You only have%s!", available) < 0)
			return false;
		return session_present_alert(session, (const uint8_t *)row, strlen(row),
		    "hostile Attack too many", error);
	}
	if (admission == YT_HOSTILE_ATTACK_LESS_THAN_ONE)
		return true;
	return yt_session_attack_deployed(session, sector,
	    (double)commitment, true, error);
}

static bool
hostile_menu_help(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t heading[] = "<Help>";
	static const uint8_t attack[] = "A - <A>ttack";
	static const char *const rows[] = {
		"B - <B>ribe Fighters",
		"D - <D>rop a Mine",
		"I - <I>nformation about your ship",
		"Q - <Q>uit the game",
		"S - Display <S>ector",
		"T - <T>eam Menu",
		"W - Emergency <W>arp",
	};
	size_t index;

	if (!session_present_paged_line(session, heading, sizeof(heading) - 1U,
	    "hostile help heading", error)
	    || !session_present_paged_line(session, attack, sizeof(attack) - 1U,
	    "hostile help attack row", error))
		return false;
	for (index = 0; index < YT_ARRAY_LEN(rows); ++index) {
		if (!session_present_paged_fragment(session, (const uint8_t *)rows[index],
		    strlen(rows[index])))
			return false;
	}
	return true;
}

bool
yt_session_sector_entry(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t hostile_warning[] =
	    "You have to defeat the fighters before you can enter this sector.";
	static const uint8_t black_hole[] = "A *-BLACK HOLE-* grabs you!";
	static const uint8_t hostile_prompt[] =
	    "Option? (A,B,D,I,Q,S,T,W,?=Help):? ";

	for (;;) {
		struct yt_sector sector;
		bool friendly;

		if (!yt_session_display_sector(session, false, error)
		    || !session_reload_player(session, error))
			return false;
		if (session_is_disruption_sector(session,
		    session->player.sector)) {
			if (!session_present_text(session, NULL, 0,
			    SESSION_PRESENT_LINE, "black hole leading blank", error)
			    || !session_attention_bytes(session, black_hole,
			    sizeof(black_hole) - 1U, "black hole attention", error))
				return false;
			session_clear_queue(session);
			if (!yt_session_emergency_warp(session, error))
				return false;
			continue;
		}
		if (!session_read_sector(session,
		    session->player.sector, &sector, error))
			return false;
		if (yt_sector_mines_admitted(sector.mines,
		    session->navigation.self_mines_suppressed ? 1.0f : 0.0f)) {
			{
				bool mine_terminal;

				if (!yt_session_mine_encounter(session, &mine_terminal,
				    error))
					return false;
				/*
				 * Both ordinary return and the zero-effect post-warp
				 * return test the same raw destruction cell before the
				 * scanner back-edge.  The mine and warp children have
				 * already installed their respective durable/FIELD state.
				 */
				if (mine_terminal && session->destroyed)
					return yt_session_common_fatal_self(session, error);
			}
			if (session->destroyed)
				return yt_session_common_fatal_self(session, error);
			continue;
		}
		friendly = yt_session_sector_force_is_friendly(session, &sector,
		    error);
		if (!friendly && error != NULL && error->status != YT_OK)
			return false;
		if (sector.fighters == 0.0f || friendly)
			return true;
		if (!session_present_alert(session, hostile_warning,
		    sizeof(hostile_warning) - 1U,
		    "hostile entry warning", error))
			return false;
		for (;;) {
			uint8_t row[160];
			size_t row_length;
			bool fresh_menu = true;

			if (!session_reload_player(session, error))
				return false;
			session_set_foreground(session, 3);
			if (!yt_hostile_menu_row(session->combat.ship_fighters,
			    session->combat.deployed_fighters, row,
			    sizeof(row), &row_length)) {
				if (error != NULL) {
					error->status = YT_RANGE;
					(void)snprintf(error->operation,
					    sizeof(error->operation), "%s",
					    "hostile fighter row");
				}
				return false;
			}
			if (!session_present_paged_line(session, row, row_length,
			    "hostile fighter row", error))
				return false;
			while (fresh_menu) {
				char response[80];
				enum yt_hostile_menu_route route;

				if (!session_present_timed_paged_row(session, hostile_prompt,
				    sizeof(hostile_prompt) - 1U,
				    "hostile option prompt", error)
				    || !session_read_upper_command(session, response,
				    sizeof(response)))
					return false;
				if (response[0] == '\0')
					(void)snprintf(response, sizeof(response), "%s", "?");
				route = yt_hostile_menu_dispatch(response);
				switch (route) {
				case YT_HOSTILE_MENU_HELP:
					if (!hostile_menu_help(session, error))
						return false;
					fresh_menu = false;
					break;
				case YT_HOSTILE_MENU_SECTOR:
					goto reenter_sector;
				case YT_HOSTILE_MENU_INFO:
					if (!yt_session_show_ship(session, error))
						return false;
					fresh_menu = false;
					break;
				case YT_HOSTILE_MENU_INVALID:
					if (!session_present_alert(session,
					    (const uint8_t *)"Invalid command.",
					    strlen("Invalid command."),
					    "hostile invalid command", error)
					    || !session_present_text(session, NULL, 0,
					    SESSION_PRESENT_LINE,
					    "hostile invalid trailing blank", error))
						return false;
					break;
				case YT_HOSTILE_MENU_ATTACK:
					if (!attack_deployed(session, &sector, error))
						return false;
					if (session->destroyed)
						return true;
					if (session->combat.deployed_fighters
					    <= 0.0) {
						session_set_foreground(session, 1);
						if (!yt_session_display_sector(session, false, error))
							return false;
						return true;
					}
					fresh_menu = false;
					break;
				case YT_HOSTILE_MENU_QUIT:
				{
					bool confirmed;

					if (!session_quit_confirm(session, &confirmed,
					    error))
						return false;
					if (!confirmed) {
						fresh_menu = false;
						break;
					}
					if (!yt_session_quit(session, error))
						return false;
					session->running = false;
					session->terminated = true;
					return false;
				}
				case YT_HOSTILE_MENU_BRIBE:
				{
					bool direct_hostile_menu;
					bool forced_attack;

					if (!yt_session_bribe_deployed(session, &sector,
					    &direct_hostile_menu, &forced_attack, error))
						return false;
					if (session->destroyed)
						return true;
					if (direct_hostile_menu) {
						fresh_menu = false;
						break;
					}
					if (forced_attack) {
						if (session->combat.deployed_fighters
						    <= 0.0) {
							session_set_foreground(session, 1);
							if (!yt_session_display_sector(session, false, error))
								return false;
							return true;
						}
						fresh_menu = false;
						break;
					}
					goto reenter_sector;
				}
				case YT_HOSTILE_MENU_MINE:
					if (!yt_session_command_mines(session, error))
						return false;
					goto reenter_sector;
				case YT_HOSTILE_MENU_WARP:
					if (!yt_session_direct_emergency_warp(session, error))
						return false;
					goto reenter_sector;
				case YT_HOSTILE_MENU_TEAM:
					if (!yt_session_command_team(session, error))
						return false;
					goto reenter_sector;
				}
			}
		}

reenter_sector:
		continue;
	}
}
