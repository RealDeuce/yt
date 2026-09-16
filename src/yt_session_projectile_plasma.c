#include "yt_session_projectile_internal.h"

#include "qb.h"

#include <math.h>
#include <string.h>

static bool
plasma_fighter_damage_row(double destroyed, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t prefix[] = "The plasma bolts destroyed";
	static const uint8_t suffix[] = " fighters!";
	char number[64];
	int number_length;
	size_t row_length;

	if (row == NULL || length == NULL)
		return false;
	number_length = qb_str_double(number, sizeof(number), destroyed);
	if (number_length < 0 || sizeof(prefix) - 1U + (size_t)number_length
	    + sizeof(suffix) - 1U > capacity)
		return false;
	memcpy(row, prefix, sizeof(prefix) - 1U);
	memcpy(row + sizeof(prefix) - 1U, number, (size_t)number_length);
	row_length = sizeof(prefix) - 1U + (size_t)number_length;
	memcpy(row + row_length, suffix, sizeof(suffix) - 1U);
	row_length += sizeof(suffix) - 1U;
	*length = row_length;
	return true;
}

static bool
plasma_fighter_news_row(const uint8_t *attacker, size_t attacker_length,
    double destroyed, int sector, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t infix[] = "'s plasma bolts destroyed";
	static const uint8_t suffix[] = " fighters in sector";
	char destroyed_text[64];
	char sector_text[64];
	int destroyed_length;
	int sector_length;
	size_t row_length = 0U;

	if (row == NULL || length == NULL
	    || (attacker == NULL && attacker_length != 0U))
		return false;
	destroyed_length = qb_str_double(destroyed_text,
	    sizeof(destroyed_text), destroyed);
	sector_length = qb_str_single(sector_text, sizeof(sector_text),
	    (float)sector);
	if (destroyed_length < 0 || sector_length < 0
	    || attacker_length + sizeof(infix) - 1U
	    + (size_t)destroyed_length + sizeof(suffix) - 1U
	    + (size_t)sector_length + 1U > capacity)
		return false;
	if (attacker_length != 0U) {
		memcpy(row, attacker, attacker_length);
		row_length = attacker_length;
	}
	memcpy(row + row_length, infix, sizeof(infix) - 1U);
	row_length += sizeof(infix) - 1U;
	memcpy(row + row_length, destroyed_text, (size_t)destroyed_length);
	row_length += (size_t)destroyed_length;
	memcpy(row + row_length, suffix, sizeof(suffix) - 1U);
	row_length += sizeof(suffix) - 1U;
	memcpy(row + row_length, sector_text, (size_t)sector_length);
	row_length += (size_t)sector_length;
	row[row_length++] = '!';
	*length = row_length;
	return true;
}

static bool
plasma_mine_entry_news_row(const uint8_t *attacker, size_t attacker_length,
    int sector, uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t infix[] =
	    "'s Plasma Bolts hit sector mines in sector";
	char sector_text[64];
	int sector_length;
	size_t row_length = 0U;

	if (row == NULL || length == NULL
	    || (attacker == NULL && attacker_length != 0U))
		return false;
	sector_length = qb_str_single(sector_text, sizeof(sector_text),
	    (float)sector);
	if (sector_length < 0 || attacker_length + sizeof(infix) - 1U
	    + (size_t)sector_length + 1U > capacity)
		return false;
	if (attacker_length != 0U) {
		memcpy(row, attacker, attacker_length);
		row_length = attacker_length;
	}
	memcpy(row + row_length, infix, sizeof(infix) - 1U);
	row_length += sizeof(infix) - 1U;
	memcpy(row + row_length, sector_text, (size_t)sector_length);
	row_length += (size_t)sector_length;
	row[row_length++] = '!';
	*length = row_length;
	return true;
}

static bool
plasma_mine_result_row(const uint8_t *attacker, size_t attacker_length,
    bool news, float destroyed, int sector, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t news_infix[] = "'s plasma bolts destroyed";
	static const uint8_t direct_prefix[] = "The plasma bolts destroyed";
	static const uint8_t result_infix[] = " mines in sector";
	const uint8_t *prefix = news ? news_infix : direct_prefix;
	size_t prefix_length = news ? sizeof(news_infix) - 1U
	    : sizeof(direct_prefix) - 1U;
	char destroyed_text[64];
	char sector_text[64];
	int destroyed_length;
	int sector_length;
	size_t row_length = 0U;

	if (row == NULL || length == NULL
	    || (attacker == NULL && attacker_length != 0U))
		return false;
	destroyed_length = qb_str_single(destroyed_text,
	    sizeof(destroyed_text), destroyed);
	sector_length = qb_str_single(sector_text, sizeof(sector_text),
	    (float)sector);
	if (destroyed_length < 0 || sector_length < 0
	    || (news ? attacker_length : 0U) + prefix_length
	    + (size_t)destroyed_length + sizeof(result_infix) - 1U
	    + (size_t)sector_length + 1U > capacity)
		return false;
	if (news && attacker_length != 0U) {
		memcpy(row, attacker, attacker_length);
		row_length = attacker_length;
	}
	memcpy(row + row_length, prefix, prefix_length);
	row_length += prefix_length;
	memcpy(row + row_length, destroyed_text, (size_t)destroyed_length);
	row_length += (size_t)destroyed_length;
	memcpy(row + row_length, result_infix, sizeof(result_infix) - 1U);
	row_length += sizeof(result_infix) - 1U;
	memcpy(row + row_length, sector_text, (size_t)sector_length);
	row_length += (size_t)sector_length;
	row[row_length++] = '!';
	*length = row_length;
	return true;
}

static bool
plasma_player_second_row(float remaining_shields, double destroyed_fighters,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "shields to";
	static const uint8_t infix[] = " units and destroying";
	static const uint8_t suffix[] = " fighters!";
	char shield_text[64];
	char fighter_text[64];
	int shield_length;
	int fighter_length;
	size_t row_length;

	if (row == NULL || length == NULL)
		return false;
	shield_length = qb_str_single(shield_text, sizeof(shield_text),
	    remaining_shields);
	fighter_length = qb_str_double(fighter_text, sizeof(fighter_text),
	    destroyed_fighters);
	if (shield_length < 0 || fighter_length < 0
	    || sizeof(prefix) - 1U + (size_t)shield_length
	    + sizeof(infix) - 1U + (size_t)fighter_length
	    + sizeof(suffix) - 1U > capacity)
		return false;
	memcpy(row, prefix, sizeof(prefix) - 1U);
	row_length = sizeof(prefix) - 1U;
	memcpy(row + row_length, shield_text, (size_t)shield_length);
	row_length += (size_t)shield_length;
	memcpy(row + row_length, infix, sizeof(infix) - 1U);
	row_length += sizeof(infix) - 1U;
	memcpy(row + row_length, fighter_text, (size_t)fighter_length);
	row_length += (size_t)fighter_length;
	memcpy(row + row_length, suffix, sizeof(suffix) - 1U);
	row_length += sizeof(suffix) - 1U;
	*length = row_length;
	return true;
}

static bool
plasma_ground_force_row(float original, float remaining, uint8_t *row,
    size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "Ground forces reduced by";
	static const uint8_t middle[] = " units to";
	char loss_text[64];
	char remaining_text[64];
	int loss_length;
	int remaining_length;
	size_t row_length;

	if (row == NULL || length == NULL)
		return false;
	loss_length = qb_str_single(loss_text, sizeof(loss_text),
	    qb_single_subtract(original, remaining));
	remaining_length = qb_str_single(remaining_text,
	    sizeof(remaining_text), remaining);
	if (loss_length < 0 || remaining_length < 0
	    || sizeof(prefix) - 1U + (size_t)loss_length
	    + sizeof(middle) - 1U + (size_t)remaining_length + 1U > capacity)
		return false;
	memcpy(row, prefix, sizeof(prefix) - 1U);
	row_length = sizeof(prefix) - 1U;
	memcpy(row + row_length, loss_text, (size_t)loss_length);
	row_length += (size_t)loss_length;
	memcpy(row + row_length, middle, sizeof(middle) - 1U);
	row_length += sizeof(middle) - 1U;
	memcpy(row + row_length, remaining_text, (size_t)remaining_length);
	row_length += (size_t)remaining_length;
	row[row_length++] = '!';
	*length = row_length;
	return true;
}

static bool
plasma_planet_impact(struct yt_session *session, int sector_number,
    struct yt_sector *sector, const uint8_t *attacker,
    size_t attacker_length, double *energy, struct yt_error *error)
{
	static const uint8_t destroyed_row[] = "The planet was destroyed!!";
	struct yt_planet_economy economy;
	struct yt_planet updated;
	struct yt_planet planet;
	struct yt_planet persistence;
	struct yt_sector unlink;
	float stale_ore;
	float production[3];
	float stock[3];
	float original_productivity;
	float remaining_productivity;
	float original_ground;
	float remaining_ground;
	uint8_t planet_name[YT_TEXT_FIELD_SIZE];
	uint8_t direct_row[256];
	uint8_t news_row[256];
	uint8_t row[256];
	size_t planet_name_length;
	size_t direct_length;
	size_t news_length;
	size_t row_length;
	size_t index;
	int logical_planet;

	if (*energy <= 0.0)
		return true;
	logical_planet = (int)sector->planet;
	if (logical_planet == 0)
		return true;
	if (!yt_session_update_planet(session, logical_planet, &updated,
	    &economy, error))
		return false;
	/* The updater returns its P(1) cache before this fresh planet read. */
	stale_ore = economy.production[1];
	if (!session_read_planet(session, logical_planet, &planet, error))
		return false;
	for (index = 0U; index < 3U; ++index) {
		production[index] = planet.production[index];
		stock[index] = planet.stock[index];
	}
	original_ground = planet.ground_forces;
	remaining_ground = original_ground;
	planet_name_length = yt_planet_stored_name(&planet, planet_name);
	if (!yt_projectile_planet_attack_rows(true, attacker, attacker_length,
	    planet_name, planet_name_length, (float)sector_number, direct_row,
	    sizeof(direct_row), &direct_length, news_row, sizeof(news_row),
	    &news_length)
	    || !session_present_text(session, direct_row, direct_length,
	    SESSION_PRESENT_LINE, "plasma planet-hit row", error)
	    || !yt_news_append_bytes(news_row, news_length, error)
	    || !session_sound(session, 2.0f, "plasma planet attack sound",
	    error))
		return false;

	original_productivity = qb_single_add(qb_single_add(production[0],
	    production[1]), production[2]);
	while ((stale_ore > 0.0f || production[1] > 0.0f
	    || production[2] > 0.0f) && *energy > 0.0) {
		float draw;
		volatile double product = *energy * 0.000004;
		float quantity = (float)product;

		remaining_ground = qb_single_subtract(remaining_ground, quantity);
		for (index = 0U; index < 3U; ++index)
			production[index] = qb_single_subtract(production[index], quantity);
		if (!yt_random_next(&session->door->game.random, &draw, error))
			return false;
		*energy -= (double)qb_single_multiply(draw, 25000.0f);
	}
	for (index = 0U; index < 3U; ++index) {
		float cap;

		if (production[index] < 0.0f)
			production[index] = 0.0f;
		cap = qb_single_multiply(production[index], 10.0f);
		if (stock[index] > cap)
			stock[index] = cap;
	}
	remaining_productivity = qb_single_add(qb_single_add(production[0],
	    production[1]), production[2]);
	if (!yt_projectile_planet_productivity_row(original_productivity,
	    remaining_productivity, row, sizeof(row), &row_length)
	    || !session_present_text(session, row, row_length,
	    SESSION_PRESENT_LINE, "plasma productivity row", error)
	    || !yt_news_append_bytes(row, row_length, error)
	    || !session_read_planet(session, logical_planet, &persistence,
	    error)
	    || !yt_projectile_planet_productivity_overlay(&persistence,
	    production, stock))
		return false;
	remaining_ground = floorf(remaining_ground);
	if (remaining_ground < 1.0f) {
		remaining_ground = 0.0f;
		persistence.owner = 0.0f;
		if (!yt_record_set_number(&persistence.record, YT_F73, 0.0f))
			return false;
	}
	persistence.ground_forces = remaining_ground;
	if (!yt_record_set_number(&persistence.record, YT_F77,
	    remaining_ground)
	    || !yt_database_write(&session->door->game.database,
	    (size_t)session_planet_basic_record(session, (float)logical_planet),
	    &persistence.record, error))
		return false;

	if (production[0] == 0.0f && production[1] == 0.0f
	    && production[2] == 0.0f) {
		if (!session_read_planet(session, logical_planet, &persistence,
		    error))
			return false;
		persistence.name_length = 0.0f;
		if (!yt_record_set_number(&persistence.record, YT_F85, 0.0f)
		    || !yt_database_write(&session->door->game.database,
		    (size_t)session_planet_basic_record(session,
		    (float)logical_planet), &persistence.record, error)
		    || !session_read_sector(session, sector_number, &unlink, error))
			return false;
		unlink.planet = 0.0f;
		if (!yt_record_set_number(&unlink.record, YT_F93, 0.0f)
		    || !yt_database_write(&session->door->game.database,
		    (size_t)session_sector_basic_record(session,
		    (float)sector_number), &unlink.record, error)
		    || !session_present_text(session, destroyed_row,
		    sizeof(destroyed_row) - 1U, SESSION_PRESENT_LINE,
		    "plasma planet-destroyed row", error)
		    || !session_sound(session, 3.0f,
		    "plasma planet destruction sound", error)
		    || !yt_news_append_bytes(destroyed_row,
		    sizeof(destroyed_row) - 1U, error))
			return false;
	}
	else if (original_ground != 0.0f) {
		if (!plasma_ground_force_row(original_ground, remaining_ground,
		    row, sizeof(row), &row_length)
		    || !session_present_text(session, row, row_length,
		    SESSION_PRESENT_LINE, "plasma ground-force row", error)
		    || !yt_news_append_bytes(row, row_length, error))
			return false;
	}
	return true;
}

bool
yt_session_plasma_sector(struct yt_session *session, int sector_number,
    const struct yt_sector *initial, const uint8_t *attacker,
    size_t launch_attacker_length, double *energy, struct yt_error *error)
{
	struct yt_sector sector;
	float planet_link;
	int basic;

	if (initial != NULL)
		sector = *initial;
	else if (!session_read_sector(session, sector_number,
	    &sector, error))
		return false;
	if ((double)sector.fighters > 0.0) {
		static const uint8_t xannor[] = "The Xannor";
		static const uint8_t mercenaries[] = "Mercenaries";
		static const uint8_t you[] = "YOU";
		static const uint8_t dirty_zero[4] = {
			0x00, 0x00, 0x10, 0x00
		};
		double original_fighters = (double)sector.fighters;
		double destroyed = 0.0;
		double remaining_fighters;
		uint8_t owner_name[YT_TEXT_FIELD_SIZE];
		uint8_t row[256];
		const uint8_t *initial_owner = xannor;
		size_t owner_length = sizeof(xannor) - 1U;
		size_t row_length;

		if (sector.fighter_owner == -2.0f) {
			initial_owner = mercenaries;
			owner_length = sizeof(mercenaries) - 1U;
		}
		memcpy(owner_name, initial_owner, owner_length);
		if (sector.fighter_owner > 1.0f) {
			struct yt_player defender;

			if (!yt_game_read_player(&session->door->game,
			    (int)sector.fighter_owner, &defender, error)
			    || !yt_player_stored_name(&defender, owner_name,
			    &owner_length, error)
			    || owner_length > sizeof(owner_name))
				return false;
		}
		if (sector.fighter_owner == (float)session_record(session)) {
			memcpy(owner_name, you, sizeof(you) - 1U);
			owner_length = sizeof(you) - 1U;
		}
		if (!yt_projectile_defense_row((float)sector_number, owner_name,
		    owner_length, original_fighters, row, sizeof(row), &row_length)
		    || !session_present_text(session, row, row_length,
		    SESSION_PRESENT_BOLD_LINE, "plasma defense report", error))
			return false;
		session->presentation.bold = 1.0f;
		if (!session_sound(session, 2.0f, "plasma fighter-defense sound",
		    error))
			return false;
		if (*energy > 0.0) {
			while (*energy > 0.0 && destroyed < original_fighters) {
				float draw;

				destroyed += floor(*energy / 5000.0) + 1.0;
				if (!yt_random_next(&session->door->game.random, &draw, error))
					return false;
				*energy -= (double)qb_single_multiply(draw, 25000.0f);
			}
			if (*energy < 0.0)
				*energy = 0.0;
			if (destroyed > original_fighters)
				destroyed = original_fighters;
			if (!plasma_fighter_damage_row(destroyed, row, sizeof(row),
			    &row_length)
			    || !session_present_text(session, row, row_length,
			    SESSION_PRESENT_LINE, "plasma destroyed-defense row",
			    error))
				return false;
			remaining_fighters = original_fighters - destroyed;
			if (destroyed > 9.0
			    && (!plasma_fighter_news_row(attacker,
			    launch_attacker_length, destroyed, sector_number, row,
			    sizeof(row), &row_length)
			    || !yt_news_append_bytes(row, row_length,
			    error)))
				return false;
			if (!session_read_sector(session, sector_number, &sector,
			    error))
				return false;
			sector.fighters = (float)remaining_fighters;
			if (!yt_record_set_number(&sector.record, YT_F81,
			    sector.fighters))
				return false;
			if (remaining_fighters == 0.0) {
				sector.fighters = 0.0f;
				sector.fighter_owner = 0.0f;
				if (!yt_record_set_raw_number(&sector.record, YT_F81,
				    dirty_zero)
				    || !yt_record_set_raw_number(&sector.record, YT_F85,
				    dirty_zero))
					return false;
			}
			if (!yt_database_write(&session->door->game.database,
			    (size_t)session_sector_basic_record(session,
			    (float)sector_number), &sector.record, error))
				return false;
			if (remaining_fighters == 0.0
			    && (float)sector_number
			    == session->door->game.config.headquarters
			    && !yt_session_xannor_victory(session, error))
				return false;
			if (*energy < 1.0)
				return true;
		}
	}
plasma_reload_sector:
	/* B099 performs a new sector GET before caching mines and planet link. */
	if (!session_read_sector(session, sector_number, &sector,
	    error))
		return false;
	planet_link = sector.planet;
	if ((double)sector.mines > 0.0) {
		double original_mines = (double)sector.mines;
		float destroyed = 0.0f;
		uint8_t row[256];
		size_t row_length;

		if (!session_sound(session, 5.0f, "plasma sector-mine sound",
		    error)
		    || !plasma_mine_entry_news_row(attacker,
		    launch_attacker_length, sector_number, row, sizeof(row),
		    &row_length)
		    || !yt_news_append_bytes(row, row_length, error))
			return false;
		while (*energy > 0.0 && (double)destroyed < original_mines) {
			float draw;
			volatile double quantum = floor(*energy * 0.000001);
			volatile double accumulated = (double)destroyed + quantum;

			destroyed = (float)(accumulated + 1.0);
			if (!yt_random_next(&session->door->game.random, &draw, error))
				return false;
			*energy -= (double)qb_single_multiply(draw, 25000.0f);
		}
		if (*energy < 0.0)
			*energy = 0.0;
		if ((double)destroyed > original_mines)
			destroyed = (float)original_mines;
		if (!plasma_mine_result_row(attacker, launch_attacker_length,
		    true, destroyed, sector_number, row, sizeof(row), &row_length)
		    || !yt_news_append_bytes(row, row_length, error)
		    || !plasma_mine_result_row(NULL, 0U, false, destroyed,
		    sector_number, row, sizeof(row), &row_length)
		    || !session_present_text(session, row, row_length,
		    SESSION_PRESENT_BOLD_LINE, "plasma destroyed-mines row", error)
		    || !session_read_sector(session, sector_number, &sector, error))
			return false;
		sector.mines = qb_single_subtract((float)original_mines, destroyed);
		if (!yt_record_set_number(&sector.record, YT_F129, sector.mines)
		    || !yt_database_write(&session->door->game.database,
		    (size_t)session_sector_basic_record(session,
		    (float)sector_number), &sector.record, error))
			return false;
		if (*energy < 1.0)
			return true;
	}
	for (basic = YT_PLAYER_FIRST_RECORD;
	    basic <= (int)session_sector_offset(session); ++basic) {
		if (yt_player_cache_value(&session->player_cache, basic,
		    YT_PLAYER_CACHE_SECTOR) != (float)sector_number
		    || !(*energy > 0.0))
			continue;
		{
			struct yt_player target;
			struct yt_player persistence;
			double original_fighters;
			double destroyed_fighters = 0.0;
			double remaining_fighters;
			float original_shields;
			float destroyed_shields = 0.0f;
			float remaining_shields;
			float saved_foreground;
			uint8_t victim[YT_TEXT_FIELD_SIZE];
			uint8_t news_row[256];
			uint8_t direct_row[256];
			uint8_t second_row[256];
			size_t victim_length;
			size_t news_length;
			size_t direct_length;
			size_t second_length;

			if (!yt_game_read_player(&session->door->game, basic, &target,
			    error))
				return false;
			original_fighters = (double)target.fighters;
			original_shields = target.shields;
			saved_foreground = session->presentation.foreground;
			session_set_foreground(session, 5.0f);
			if (!session_sound(session, 2.0f,
			    "plasma player-attack sound", error))
				return false;
			while (*energy > 0.0
			    && destroyed_fighters < original_fighters) {
				float draw;
				volatile double quantum = floor(*energy / 5000.0);
				volatile double accumulated = destroyed_fighters + quantum;

				destroyed_fighters = accumulated + 1.0;
				if (!yt_random_next(&session->door->game.random, &draw, error))
					return false;
				*energy -= (double)qb_single_multiply(draw, 25000.0f);
			}
			while (*energy > 0.0
			    && destroyed_shields < original_shields) {
				float draw;
				volatile double quantum = floor(*energy / 10000.0);
				volatile double accumulated =
				    (double)destroyed_shields + quantum;

				destroyed_shields = (float)(accumulated + 1.0);
				if (!yt_random_next(&session->door->game.random, &draw, error))
					return false;
				*energy -= (double)qb_single_multiply(draw, 25000.0f);
			}
			if (destroyed_fighters > original_fighters)
				destroyed_fighters = original_fighters;
			if (destroyed_shields > original_shields)
				destroyed_shields = original_shields;
			remaining_fighters = original_fighters - destroyed_fighters;
			remaining_shields = qb_single_subtract(original_shields,
			    destroyed_shields);
			if (!yt_game_read_player(&session->door->game, basic, &target,
			    error)
			    || !yt_player_stored_name(&target, victim, &victim_length,
			    error)
			    || !yt_projectile_attack_first_rows(true, attacker,
			    launch_attacker_length, victim, victim_length,
			    (float)sector_number, news_row, sizeof(news_row),
			    &news_length, direct_row, sizeof(direct_row), &direct_length)
			    || !yt_news_append_bytes(news_row, news_length,
			    error)
			    || !session_present_text(session, direct_row, direct_length,
			    SESSION_PRESENT_BOLD_LINE,
			    "plasma player attack first row", error)
			    || !plasma_player_second_row(remaining_shields,
			    destroyed_fighters, second_row, sizeof(second_row),
			    &second_length)
			    || !yt_news_append_bytes(second_row,
			    second_length, error)
			    || !session_present_text(session, second_row, second_length,
			    SESSION_PRESENT_BOLD_LINE,
			    "plasma player attack second row", error))
				return false;
			session_set_foreground(session, saved_foreground);
			if (remaining_shields >= 1.0f) {
				if (!yt_game_read_player(&session->door->game, basic,
				    &persistence, error))
					return false;
				persistence.shields = remaining_shields;
				persistence.fighters = (float)remaining_fighters;
				if (!yt_record_set_number(&persistence.record, YT_F53,
				    persistence.shields)
				    || !yt_record_set_number(&persistence.record, YT_F61,
				    persistence.fighters)
				    || !yt_database_write(&session->door->game.database,
				    (size_t)basic, &persistence.record, error))
					return false;
				if (*energy < 1.0)
					return true;
				continue;
			}
		}
		{
			static const uint8_t self_row[] = "YOU were destroyed!";
			static const uint8_t cache_zero[4] = {
				0x00, 0x00, 0x80, 0x00
			};
			struct yt_player victim;
			struct yt_sector mine_persistence;
			uint8_t victim_name[YT_TEXT_FIELD_SIZE];
			uint8_t destroyed_row[128];
			uint8_t warning_row[160];
			size_t victim_name_length = 0U;
			size_t destroyed_length = 0U;
			size_t warning_length = 0U;
			float saved_mines;
			bool self_hit = basic == session_record(session);
			bool rows_ready = false;

			if (!yt_game_read_player(&session->door->game, basic, &victim,
			    error))
				return false;
			if (!self_hit) {
				if (!yt_player_stored_name(&victim, victim_name,
				    &victim_name_length, error)
				    || !yt_projectile_destroyed_rows(victim_name,
				    victim_name_length, destroyed_row,
				    sizeof(destroyed_row), &destroyed_length, warning_row,
				    sizeof(warning_row), &warning_length))
					return false;
				rows_ready = true;
			}
			saved_mines = victim.mines;
			victim.mines = 0.0f;
			victim.danger_scanner = 0.0f;
			if (!yt_record_set_number(&victim.record, YT_F129, 0.0f)
			    || !yt_record_set_number(&victim.record, YT_F93, 0.0f)
			    || !yt_database_write(&session->door->game.database,
			    (size_t)basic, &victim.record, error))
				return false;

			yt_present_set_blink(&session->presentation, 1.0f);
			if (self_hit) {
				if (!session_present_text(session, self_row,
				    sizeof(self_row) - 1U, SESSION_PRESENT_BOLD_LINE,
				    "plasma self-destruction row", error))
					return false;
			}
			else if (!session_present_text(session, destroyed_row,
			    destroyed_length, SESSION_PRESENT_BOLD_LINE,
			    "plasma victim-destruction row", error))
				return false;

			if (saved_mines != 0.0f) {
				if (!rows_ready
				    && (!yt_player_stored_name(&victim, victim_name,
				    &victim_name_length, error)
				    || !yt_projectile_destroyed_rows(victim_name,
				    victim_name_length, destroyed_row,
				    sizeof(destroyed_row), &destroyed_length, warning_row,
				    sizeof(warning_row), &warning_length)))
					return false;
				yt_present_set_blink(&session->presentation, 1.0f);
				if (!session_present_text(session, warning_row,
				    warning_length, SESSION_PRESENT_BOLD_LINE,
				    "plasma carried-mine warning", error)
				    || !session_read_sector(session, sector_number,
				    &mine_persistence, error))
					return false;
				mine_persistence.mines = qb_single_add(
				    mine_persistence.mines, saved_mines);
				if (!yt_record_set_number(&mine_persistence.record, YT_F129,
				    mine_persistence.mines)
				    || !yt_database_write(&session->door->game.database,
				    (size_t)session_sector_basic_record(session,
				    (float)sector_number), &mine_persistence.record, error))
					return false;
			}

			if (self_hit) {
				session->destroyed = true;
				if (!yt_player_cache_set_raw(&session->player_cache, basic,
				    YT_PLAYER_CACHE_SECTOR, cache_zero))
					return false;
			}
			else if (!yt_session_kill_player(session, basic,
			    (float)session_record(session), true, error)
			    || !session_sound(session, 3.0f, "plasma salvage sound",
			    error)
			    || !yt_session_salvage_player(session, basic,
			    session_record(session), error))
				return false;

			if (*energy > 0.0 && saved_mines > 0.0f)
				goto plasma_reload_sector;
			if (*energy < 1.0)
				return true;
		}
	}
	if (!(*energy > 0.0) || planet_link == 0.0f)
		return true;
	/* The B099 dispatch cached this link before mines and the player scan. */
	sector.planet = planet_link;
	return plasma_planet_impact(session, sector_number, &sector, attacker,
	    launch_attacker_length, energy, error);
}
