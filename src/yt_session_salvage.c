#include "yt_session_internal.h"

#include <math.h>
#include <string.h>

static float
single_add(float left, float right)
{
	volatile float result = left + right;

	return result;
}

static float
single_sub(float left, float right)
{
	volatile float result = left - right;

	return result;
}

static float
single_mul(float left, float right)
{
	volatile float result = left * right;

	return result;
}

static bool
salvage_load_player(struct yt_session *session, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	if (player_record == session_record(session)) {
		if (!session_reload_player(session, error))
			return false;
		*player = session->player;
		return true;
	}
	return yt_game_read_player(&session->door->game, player_record, player,
	    error);
}

static bool
salvage_save_player(struct yt_session *session, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	if (!yt_game_write_player(&session->door->game, player_record, player,
	    error)
	    || !yt_database_flush(&session->door->game.database, error))
		return false;
	if (player_record == session_record(session))
		session->player = *player;
	return true;
}

bool
yt_session_salvage_player(struct yt_session *session, int victim_record,
    int killer_record, struct yt_error *error)
{
	static const uint8_t title[] =
	    "You destroyed the ship and salvaged the following:";
	static const uint8_t nothing[] = "  -  NOTHING!";
	static const enum yt_salvage_cargo_kind cargo_kind[4] = {
		YT_SALVAGE_EMPTY_HOLDS, YT_SALVAGE_ORE,
		YT_SALVAGE_ORGANICS, YT_SALVAGE_EQUIPMENT
	};
	static const size_t cargo_order[4] = {3U, 0U, 1U, 2U};
	struct yt_player victim;
	struct yt_player killer;
	float awards[6] = {0};
	float cargo_stock[3];
	float cargo_awards[4] = {0};
	float cargo_remaining;
	float requested_holds;
	float *simple_fields[5];
	uint8_t victim_name[YT_TEXT_FIELD_SIZE];
	uint8_t row[300];
	size_t victim_name_length;
	size_t row_length;
	size_t index;
	bool emitted = false;

	/*
	 * The victim GET precedes the killer-range gate. Player identities are
	 * integers because all callers and persisted producers write integers.
	 */
	if (!yt_game_read_player(&session->door->game, victim_record, &victim,
	    error))
		return false;
	if (killer_record < YT_PLAYER_FIRST_RECORD
	    || (float)killer_record > session->door->game.config.sector_offset)
		return true;
	if (!yt_player_stored_name(&victim, victim_name, &victim_name_length,
	    error)
	    || !session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "salvage result row", error)
	    || !session_present_text(session, title, sizeof(title) - 1U,
	    SESSION_PRESENT_BOLD_LINE, "salvage title", error)
	    || !yt_salvage_header_row((const uint8_t *)session->player.name,
	    strlen(session->player.name), victim_name, victim_name_length,
	    row, sizeof(row), &row_length)
	    || !yt_news_append_bytes(row, row_length, error)
	    || !session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "salvage result row", error))
		return false;

	for (index = 0U; index < YT_ARRAY_LEN(awards); ++index) {
		float stock;
		float draw;

		if (!yt_random_next(&session->door->game.random, &draw, error))
			return false;
		switch (index) {
		case 0U: stock = victim.holds; break;
		case 1U: stock = victim.credits; break;
		case 2U: stock = victim.missiles; break;
		case 3U: stock = victim.plasma; break;
		case 4U: stock = victim.ground_forces; break;
		default: stock = victim.mines; break;
		}
		awards[index] = floorf(single_mul(draw, stock));
	}
	if (!session_wait(session, 1.0, "ship salvage wait", error)
	    || !salvage_load_player(session, killer_record, &killer, error))
		return false;

	simple_fields[0] = &killer.credits;
	simple_fields[1] = &killer.missiles;
	simple_fields[2] = &killer.plasma;
	simple_fields[3] = &killer.ground_forces;
	simple_fields[4] = &killer.mines;
	for (index = 1U; index < YT_ARRAY_LEN(awards); ++index) {
		if (awards[index] == 0.0f)
			continue;
		if (!session_wait(session, 0.5, "ship salvage wait", error))
			return false;
		emitted = true;
		if (!yt_salvage_simple_row(
		    (enum yt_salvage_simple_kind)(index - 1U), awards[index],
		    row, sizeof(row), &row_length)
		    || !yt_news_append_bytes(row, row_length, error)
		    || !session_present_text(session, row, row_length,
		    SESSION_PRESENT_LINE, "salvage result row", error))
			return false;
		*simple_fields[index - 1U] = single_add(
		    *simple_fields[index - 1U], awards[index]);
	}
	if (!salvage_save_player(session, killer_record, &killer, error))
		return false;

	requested_holds = awards[0];
	if (single_add(killer.holds, requested_holds)
	    > session->door->game.config.maximum_holds)
		requested_holds = single_sub(
		    session->door->game.config.maximum_holds, killer.holds);
	if (requested_holds > 0.0f) {
		float counter;

		emitted = true;
		if (!yt_game_read_player(&session->door->game, victim_record,
		    &victim, error))
			return false;
		cargo_stock[0] = victim.ore;
		cargo_stock[1] = victim.organics;
		cargo_stock[2] = victim.equipment;
		cargo_remaining = victim.holds;
		for (counter = 1.0f; counter <= requested_holds;
		    counter = single_add(counter, 1.0f)) {
			float one_based;
			float pick;
			float boundary;
			int selected;

			if (!yt_random_one_based_single(
			    &session->door->game.random, cargo_remaining,
			    &one_based, error))
				return false;
			pick = single_sub(one_based, 1.0f);
			if (pick < cargo_stock[0])
				selected = 0;
			else {
				boundary = single_add(cargo_stock[0],
				    cargo_stock[1]);
				if (pick < boundary)
					selected = 1;
				else {
					boundary = single_add(boundary,
					    cargo_stock[2]);
					selected = pick < boundary ? 2 : 3;
				}
			}
			cargo_awards[selected] = single_add(
			    cargo_awards[selected], 1.0f);
			if (selected < 3)
				cargo_stock[selected] = single_sub(
				    cargo_stock[selected], 1.0f);
			cargo_remaining = single_sub(cargo_remaining, 1.0f);
		}
		if (!salvage_load_player(session, killer_record, &killer, error))
			return false;
		for (index = 0U; index < YT_ARRAY_LEN(cargo_awards); ++index)
			killer.holds = single_add(killer.holds,
			    cargo_awards[index]);
		killer.ore = single_add(killer.ore, cargo_awards[0]);
		killer.organics = single_add(killer.organics, cargo_awards[1]);
		killer.equipment = single_add(killer.equipment,
		    cargo_awards[2]);
		if (!salvage_save_player(session, killer_record, &killer, error)
		    || !session_wait(session, 0.5, "ship salvage wait", error))
			return false;
		for (index = 0U; index < YT_ARRAY_LEN(cargo_order); ++index) {
			size_t award = cargo_order[index];

			if ((award == 3U && cargo_awards[award] <= 0.0f)
			    || (award != 3U && cargo_awards[award] == 0.0f))
				continue;
			if (!session_wait(session, 0.5, "ship salvage wait", error)
			    || !yt_salvage_cargo_row(cargo_kind[index],
			    cargo_awards[award], row, sizeof(row), &row_length)
			    || !yt_news_append_bytes(row, row_length, error)
			    || !session_present_text(session, row, row_length,
			    SESSION_PRESENT_LINE, "salvage result row", error))
				return false;
		}
	}
	if (!emitted) {
		if (!session_wait(session, 0.5, "ship salvage wait", error)
		    || !yt_news_append_bytes(nothing,
		    sizeof(nothing) - 1U, error)
		    || !session_present_text(session, nothing,
		    sizeof(nothing) - 1U, SESSION_PRESENT_LINE,
		    "salvage result row", error))
			return false;
	}
	return session_wait(session, 4.0, "ship salvage wait", error);
}
