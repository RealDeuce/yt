#include "yt_session_internal.h"

#include "qb.h"
#include "yt_output.h"
#include "yt_port_math.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static bool
planet_move_friendship(struct yt_session *session, int owner,
    bool *friendly, struct yt_error *error)
{
	struct yt_player current;
	struct yt_player other;
	int last_player = session_sector_offset(session);

	if (friendly == NULL)
		return false;
	*friendly = false;
	session->player_reference.friendly = false;
	if (owner < 2 || owner > last_player
	    || session_record(session) < 2 || session_record(session) > last_player)
		return true;
	if (owner == session_record(session)) {
		*friendly = true;
		session->player_reference.friendly = true;
		return true;
	}
	if (!yt_game_read_player(&session->door->game, session_record(session),
	    &current, error))
		return false;
	if (current.team == 0.0f)
		return true;
	if (!yt_game_read_player(&session->door->game, owner,
	    &other, error))
		return false;
	*friendly = other.team == current.team;
	if (*friendly)
		session->player_reference.friendly = true;
	return true;
}

static bool
planet_move_hop(struct yt_session *session, int source_number,
    int destination, bool final_hop, bool *stop, struct yt_error *error)
{
	static const uint8_t xannor_prefix[] =
	    "No way! We don't want no trouble from no Xannor ";
	static const uint8_t xannor_slogan[] =
	    "(Xannoron Movers, We're MOVEers not FIGHTers.)";
	static const uint8_t occupied[] =
	    "There is already a planet in that sector!";
	static const uint8_t wanderer[] =
	    "The Wanderer vanishes from your sensors!";
	struct yt_sector source;
	struct yt_sector target;
	struct yt_planet planet;
	uint8_t planet_name[YT_TEXT_FIELD_SIZE];
	uint8_t player_name[YT_TEXT_FIELD_SIZE];
	uint8_t row[512];
	size_t planet_name_length;
	size_t player_name_length;
	size_t row_length;
	int source_link;
	int moving_planet;
	int actual_destination = destination;
	float draw;
	float xannor_planet = qb_single_subtract(
	    session->door->game.config.total_records,
	    (float)session_planet_offset(session));
	uint32_t moving_record;
	int source_record;
	int destination_record;

	if (stop == NULL)
		return false;
	*stop = false;
	if (!session_read_sector(session, source_number, &source, error))
		return false;
	if (source.planet == xannor_planet) {
		size_t first_name_length = strlen(session->door->identity.real_first);

		if (sizeof(xannor_prefix) - 1U + first_name_length + 1U
		    > sizeof(row))
			return false;
		memcpy(row, xannor_prefix, sizeof(xannor_prefix) - 1U);
		memcpy(row + sizeof(xannor_prefix) - 1U,
		    session->door->identity.real_first, first_name_length);
		row[sizeof(xannor_prefix) - 1U + first_name_length] = '!';
		if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
		    "planet move Xannor blank", error)
		    || !session_present_text(session, row,
		    sizeof(xannor_prefix) + first_name_length,
		    SESSION_PRESENT_LINE, "planet move Xannor refusal", error)
		    || !session_present_text(session, xannor_slogan,
		    sizeof(xannor_slogan) - 1U, SESSION_PRESENT_LINE,
		    "planet move Xannor slogan", error))
			return false;
		*stop = true;
		return true;
	}
	if (!session_read_sector(session, destination, &target, error))
		return false;
	if (target.planet > 0.0f) {
		if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
		    "planet move occupied blank", error)
		    || !session_present_text(session, occupied,
		    sizeof(occupied) - 1U, SESSION_PRESENT_LINE,
		    "planet move occupied row", error))
			return false;
		*stop = true;
	}
	if (!session_read_sector(session, source_number, &source, error))
		return false;
	source_link = (int)source.planet;
	moving_record = session_planet_basic_record(session, source_link);
	moving_planet = source_link;
	yt_planet_move_sector_overlay(&source, 0.0f);
	source_record = (int)session_sector_basic_record(session, source_number);
	if (!yt_database_write(&session->door->game.database,
	    (size_t)source_record, &source.record, error)
	    || !read_planet_physical(session, moving_record, &planet, error))
		return false;
	planet_name_length = yt_planet_stored_name(&planet, planet_name);
	if (!yt_random_next(&session->door->game.random, &draw, error))
		return false;
	if (draw > 0.9950000047683716f || *stop) {
		float loss = 0.0f;

		yt_planet_move_explosion_overlay(&planet);
		if (!session_write_planet_physical(session, moving_record, &planet,
		    false, error)
		    || !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "planet move explosion first blank", error)
		    || !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "planet move explosion second blank", error)
		    || !yt_planet_move_explosion_row(planet_name,
		    planet_name_length, row, sizeof(row), &row_length))
			return false;
		yt_present_set_bold(&session->presentation, 1.0f);
		if (!session_present_text(session, row, row_length,
		    SESSION_PRESENT_BOLD_LINE, "planet move explosion row", error))
			return false;
		player_name_length = yt_player_stored_name(&session->player,
		    player_name);
		if (!yt_planet_move_explosion_news(planet_name,
		    planet_name_length, player_name, player_name_length,
		    row, sizeof(row), &row_length)
		    || !yt_news_append_bytes(row, row_length, error)
		    || !session_sound(session, YT_SOUND_CUE_DESTRUCTION,
		    "planet move explosion sound", error)
		    || !session_reload_player(session, error))
			return false;
		if (session->player.fighters != 0.0f) {
			float range = session->player.fighters;

			if (!yt_random_nested_single(&session->door->game.random,
			    2.0f, &range, &loss, error))
				return false;
		}
		yt_planet_move_fighter_overlay(&session->player, loss);
		if (!yt_database_write(&session->door->game.database,
		    (size_t)session_record(session), &session->player.record, error))
			return false;
		if (loss > 0.0f) {
			static const uint8_t you[] = "You";

			if (!yt_planet_move_loss_row(you, sizeof(you) - 1U,
			    loss, row, sizeof(row), &row_length)
			    || !session_present_text(session, row, row_length,
			    SESSION_PRESENT_LINE, "planet move fighter loss row", error)
			    || !yt_planet_move_loss_row(player_name,
			    player_name_length, loss, row, sizeof(row), &row_length)
			    || !yt_news_append_bytes(row, row_length, error))
				return false;
			*stop = true;
		}
		return true;
	}
	if (moving_planet == 1) {
		float maximum = qb_single_subtract(
		    (float)session_port_offset(session),
		    (float)session_sector_offset(session));

		if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
		    "planet move Wanderer blank", error)
		    || !session_present_text(session, wanderer,
		    sizeof(wanderer) - 1U, SESSION_PRESENT_LINE,
		    "planet move Wanderer row", error))
			return false;
		*stop = true;
		for (;;) {
			if (!yt_random_next(&session->door->game.random, &draw,
			    error))
				return false;
			actual_destination = (int)(floorf(qb_single_multiply(draw,
			    maximum)) + 1.0f);
			if (!session_read_sector(session, actual_destination,
			    &target, error))
				return false;
			if (target.planet <= 0.0f)
				break;
		}
	}
	else if (final_hop) {
		if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
		    "planet move final first blank", error)
		    || !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "planet move final second blank", error)
		    || !yt_planet_move_success_row(planet_name,
		    planet_name_length, row, sizeof(row), &row_length)
		    || !session_present_text(session, row, row_length,
		    SESSION_PRESENT_LINE, "planet move final row", error)
		    || !session_sound(session, YT_SOUND_CUE_ACTION,
		    "planet move completion sound", error))
			return false;
	}
	if (!session_read_sector(session, actual_destination, &target,
	    error))
		return false;
	if (target.mines != 0.0f)
		*stop = true;
	if (target.fighters != 0.0f) {
		bool friendly;

		if (!planet_move_friendship(session, (int)target.fighter_owner,
		    &friendly, error))
			return false;
		if (!friendly)
			*stop = true;
		if (!session_read_sector(session, actual_destination, &target,
		    error))
			return false;
	}
	yt_planet_move_sector_overlay(&target, (float)moving_planet);
	destination_record = (int)session_sector_basic_record(session,
	    actual_destination);
	if (!yt_database_write(&session->door->game.database,
	    (size_t)destination_record, &target.record, error)
	    || !session_reload_player(session, error))
		return false;
	yt_planet_move_success_overlay(&session->player, (float)destination);
	return yt_database_write(&session->door->game.database,
	    (size_t)session_record(session), &session->player.record, error);
}

bool
yt_session_planet_move(struct yt_session *session, bool *enter_sector,
    struct yt_error *error)
{
	static const uint8_t cost_notice[] =
	    "Moving planets costs 10 turns per sector.";
	static const uint8_t destination_prompt[] =
	    "Move planet to what sector? ";
	static const uint8_t same_sector[] =
	    "Hey, look out the window dummy!";
	static const uint8_t range_prefix[] =
	    "Valid sector numbers are from 1 to";
	static const uint8_t working[] = "Working. ";
	static const uint8_t route_failure[] =
	    "*** You can't get there without going someplace you dont want to!";
	static const uint8_t insufficient[] =
	    "Not enough turns left to move the planet that far!";
	static const uint8_t confirmation[] = "Move the planet? (Y/[N])";
	static const uint8_t engaged[] = "Planet thrusters engaged.";
	static const uint8_t moving[] = "Moving to sector:";
	char response[160];
	char number[64];
	uint8_t row[512];
	size_t row_length;
	struct session_route_plan route;
	float start = session->player.sector;
	float destination;
	float maximum = (float)session_sector_count(session);
	float cost = 0.0f;
	int start_node;
	int destination_node;
	int cursor;
	bool conversion_overflow;
	bool found;
	bool stop = false;
	bool final = false;
	enum yt_yes_no_answer answer;

	if (enter_sector != NULL)
		*enter_sector = false;
	if (!session_present_paged_line(session, cost_notice,
	    sizeof(cost_notice) - 1U, "planet Thrusters cost notice", error)
	    || !yt_session_display_sector(session, false, error)
	    || !session_present_timed_paged_row(session, destination_prompt,
	    sizeof(destination_prompt) - 1U,
	    "planet Thrusters destination prompt", error)
	    || !session_read_number_command(session, response, sizeof(response)))
		return false;
	destination = yt_planet_move_destination(response);
	if (destination == start)
		return session_present_alert(session, same_sector,
		    sizeof(same_sector) - 1U,
		    "planet Thrusters same-sector", error);
	if (destination < 1.0f || destination > maximum) {
		int number_length = qb_str_single(number, sizeof(number), maximum);

		if (number_length < 0
		    || sizeof(range_prefix) - 1U + (size_t)number_length + 1U
		    > sizeof(row))
			return false;
		memcpy(row, range_prefix, sizeof(range_prefix) - 1U);
		memcpy(row + sizeof(range_prefix) - 1U, number,
		    (size_t)number_length);
		row[sizeof(range_prefix) - 1U + (size_t)number_length] = '!';
		return session_present_alert(session, row,
		    sizeof(range_prefix) + (size_t)number_length,
		    "planet Thrusters range", error);
	}
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "planet Thrusters working blank", error)
	    || !session_present_timed_paged_row(session, working,
	    sizeof(working) - 1U, "planet Thrusters working", error))
		return false;
	start_node = (int)qb_cint_mode((double)start,
	    session->presentation.sound.conversion_mode, &conversion_overflow);
	if (conversion_overflow) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation), "%s",
			    "planet Thrusters start CINT");
		}
		return false;
	}
	destination_node = (int)qb_cint_mode((double)destination,
	    session->presentation.sound.conversion_mode, &conversion_overflow);
	if (conversion_overflow) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation), "%s",
			    "planet Thrusters destination CINT");
		}
		return false;
	}
	if (!yt_session_build_route(session, start, destination, &route, true,
	    &found, NULL, NULL, error))
		return false;
	if (!found) {
		bool ok = session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "planet Thrusters route first blank", error)
		    && session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "planet Thrusters route second blank", error);

		yt_present_set_blink(&session->presentation, 1.0f);
		if (ok)
			ok = session_present_text(session, route_failure,
			    sizeof(route_failure) - 1U, SESSION_PRESENT_BOLD_LINE,
			    "planet Thrusters route failure", error);
		return ok;
	}
	if (!yt_planet_move_path_heading(start, destination, row, sizeof(row),
	    &row_length)
	    || !session_present_paged_fragment(session, row, row_length)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "planet Thrusters route leading blank", error)
	    || qb_str_single(number, sizeof(number), start) < 0
	    || !session_present_timed_paged_row(session,
	    (const uint8_t *)number, strlen(number),
	    "planet Thrusters route start", error))
		return false;
	cursor = start_node;
	for (;;) {
		int next = route.next_hop[cursor];
		int column;
		int ignored_row;
		int number_length;

		if (next == 0)
			break;
		session_set_pager_line_count(session, 0.0f);
		number_length = qb_str_single(number, sizeof(number), (float)next);
		if (number_length < 0)
			return false;
		if (next != destination_node)
			number[number_length++] = ',';
		number[number_length] = '\0';
		if (!session_present_timed_paged_row(session,
		    (const uint8_t *)number, (size_t)number_length,
		    "planet Thrusters route token", error))
			return false;
		yt_out_cursor_position(&ignored_row, &column);
		if (column > 74 && !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "planet Thrusters route wrap", error))
			return false;
		cost = yt_planet_move_add_cost(cost);
		cursor = next;
	}
	session_set_pager_line_count(session, 0.0f);
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "planet Thrusters route ending", error)
	    || !yt_planet_move_summary(cost, row, sizeof(row), &row_length)
	    || !session_present_paged_line(session, row, row_length,
	    "planet Thrusters distance summary", error)
	    || !session_reload_player(session, error))
		return false;
	if (cost > session->player.turns)
		return session_present_alert(session, insufficient,
		    sizeof(insufficient) - 1U,
		    "planet Thrusters insufficient turns", error);
	if (!yt_planet_move_turns_row(session->player.turns, row, sizeof(row),
	    &row_length)
	    || !session_present_paged_fragment(session, row, row_length)
	    || !session_confirm(session, confirmation, sizeof(confirmation) - 1U,
	    &answer, error))
		return false;
	if (answer != YT_YES_NO_YES)
		return true;
	yt_present_set_bold(&session->presentation, 1.0f);
	yt_present_set_blink(&session->presentation, 1.0f);
	if (!session_present_paged_line(session, engaged, sizeof(engaged) - 1U,
	    "planet Thrusters engaged", error))
		return false;
	session_set_pager_line_count(session, 0.0f);
	if (!session_present_timed_paged_row(session, moving,
	    sizeof(moving) - 1U, "planet Thrusters moving prefix", error))
		return false;
	cursor = start_node;
	for (;;) {
		int next = route.next_hop[cursor];
		int number_length;

		if (next == 0)
			break;
		number_length = qb_str_single(number, sizeof(number), (float)next);
		if (number_length < 0 || !session_present_timed_paged_row(session,
		    (const uint8_t *)number, (size_t)number_length,
		    "planet Thrusters movement token", error))
			return false;
		if (next == destination_node)
			final = true;
		if (!planet_move_hop(session, cursor, next, final, &stop, error))
			return false;
		cursor = next;
		if (stop)
			break;
	}
	if (!stop && !session_reload_player(session, error))
		return false;
	if (enter_sector != NULL)
		*enter_sector = true;
	return true;
}
