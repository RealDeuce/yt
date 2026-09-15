#include "yt_session_internal.h"

#include "qb.h"

#include <math.h>
#include <stdio.h>
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

static float
session_player_cache_value(const struct yt_session *session,
    int player_record, enum yt_player_cache_kind kind)
{
	return yt_player_cache_value(&session->player_cache, player_record, kind);
}

struct projectile_route_state {
	float origin;
	float destination;
	float amount;
};

static bool
missile_planet_impact(struct yt_session *session, int sector_number,
    struct yt_sector *sector, float *remaining, bool *early_return,
    struct yt_error *error)
{
	static const uint8_t destroyed[] = "The planet was destroyed!!";
	struct yt_planet planet;
	struct yt_planet updater_planet;
	int logical_planet;
	uint32_t physical_planet;
	uint32_t physical_sector;
	float original_ore;
	bool friendly = false;
	uint8_t planet_name[YT_TEXT_FIELD_SIZE];
	uint8_t attacker_name[YT_TEXT_FIELD_SIZE];
	uint8_t direct_row[256];
	uint8_t news_row[256];
	size_t planet_name_length;
	size_t attacker_name_length;
	size_t direct_length;
	size_t news_length;

	if (early_return == NULL)
		return false;
	*early_return = false;
	if (*remaining <= 0.0f) {
		*early_return = true;
		return true;
	}
	logical_planet = (int)sector->planet;
	if (logical_planet == 0)
		return true;
	physical_planet = yt_projectile_physical_record(
	    session_planet_offset(session), sector->planet);
	physical_sector = yt_projectile_physical_record(
	    session_sector_offset(session), (float)sector_number);
	if (!yt_session_update_planet_physical(session, physical_planet,
	    &updater_planet, NULL, error))
		return false;
	/* DS:1A48 remains the updater's ore value across the independent GET. */
	original_ore = updater_planet.production[0];
	if (!read_planet_physical(session, physical_planet, &planet, error))
		return false;
	if (!yt_planet_stored_name(&planet, planet_name, &planet_name_length,
	    error))
		return false;
	if (planet.owner == (float)session_record(session))
		friendly = true;
	else if (planet.owner > 1.0f
	    && planet.owner <= session_sector_offset(session)) {
		if (!yt_session_players_are_friendly(session, (int)planet.owner,
		    &friendly, error))
			return false;
		if (!read_planet_physical(session, physical_planet, &planet,
		    error))
			return false;
	}
	if (friendly) {
		if (!yt_projectile_friendly_planet_row(planet_name,
		    planet_name_length, direct_row, sizeof(direct_row),
		    &direct_length))
			return false;
		return session_present_text(session, direct_row, direct_length,
		    SESSION_PRESENT_LINE,
		    "cruise missile friendly-planet row", error);
	}
	if (!yt_player_stored_name(&session->player, attacker_name,
	    &attacker_name_length, error)
	    || !yt_projectile_planet_attack_rows(false, attacker_name,
	    attacker_name_length, planet_name, planet_name_length,
	    (float)sector_number, direct_row, sizeof(direct_row),
	    &direct_length, news_row, sizeof(news_row), &news_length)
	    || !session_present_text(session, direct_row, direct_length,
	    SESSION_PRESENT_LINE, "cruise missile planet-attack row", error)
	    || !yt_news_append_bytes(news_row, news_length, error))
		return false;
	if (!session_sound(session, 2.0f,
	    "cruise missile planet attack sound", error))
		return false;
	if (planet.ground_forces != 0.0f) {
		struct yt_projectile_ground_result impact;
		struct yt_planet persistence;
		uint8_t row[256];
		size_t row_length;

		if (!yt_projectile_planet_ground_damage(planet.ground_forces,
		    planet.owner, remaining, &session->door->game.random, &impact,
		    error))
			return false;
		planet.ground_forces = impact.ground;
		planet.owner = impact.owner;
		if (!read_planet_physical(session, physical_planet, &persistence,
		    error)
		    || !yt_projectile_planet_ground_overlay(&persistence,
		    impact.ground, impact.owner)
		    || !session_write_planet_physical(session, physical_planet,
		    &persistence, false, error)
		    || !yt_projectile_planet_ground_row(impact.ground, row,
		    sizeof(row), &row_length)
		    || !session_present_text(session, row, row_length,
		    SESSION_PRESENT_LINE, "cruise missile planet impact row", error)
		    || !yt_news_append_bytes(row, row_length, error))
			return false;
		if (*remaining < 1.0f) {
			*early_return = true;
			return true;
		}
	}

	{
		struct yt_projectile_productivity_result impact;
		struct yt_planet persistence;
		uint8_t row[256];
		size_t row_length;

		if (!yt_projectile_planet_productivity_damage(original_ore,
		    planet.production, planet.stock, remaining,
		    &session->door->game.random, &impact, error)
		    || !yt_projectile_planet_productivity_row(impact.old_total,
		    impact.new_total, row, sizeof(row), &row_length)
		    || !session_present_text(session, row, row_length,
		    SESSION_PRESENT_LINE, "cruise missile planet impact row", error)
		    || !yt_news_append_bytes(row, row_length, error)
		    || !read_planet_physical(session, physical_planet, &persistence,
		    error)
		    || !yt_projectile_planet_productivity_overlay(&persistence,
		    planet.production, planet.stock)
		    || !session_write_planet_physical(session, physical_planet,
		    &persistence, false, error))
			return false;
	}

	if (planet.production[0] == 0.0f
	    && planet.production[1] == 0.0f
	    && planet.production[2] == 0.0f) {
		struct yt_planet destruction;
		struct yt_sector unlink;
		struct yt_record record;

		if (!read_planet_physical(session, physical_planet, &destruction,
		    error)
		    || !yt_projectile_planet_destroy_overlay(&destruction)
		    || !session_write_planet_physical(session, physical_planet,
		    &destruction, false, error)
		    || !yt_database_read(&session->door->game.database,
		    (size_t)physical_sector, &record, error))
			return false;
		yt_sector_decode(&unlink, &record);
		if (!yt_projectile_sector_unlink_overlay(&unlink)
		    || !yt_database_write(&session->door->game.database,
		    (size_t)physical_sector, &unlink.record, error)
		    || !session_present_text(session, destroyed,
		    sizeof(destroyed) - 1U, SESSION_PRESENT_LINE,
		    "cruise missile planet impact row", error)
		    || !session_sound(session, 3.0f,
		    "cruise missile planet destruction sound", error)
		    || !yt_news_append_bytes(destroyed,
		    sizeof(destroyed) - 1U, error))
			return false;
	}
	if (*remaining < 1.0f)
		*early_return = true;
	return true;
}

static bool
deploy_victim_mines(struct yt_session *session, int sector_number,
    float mines, struct yt_error *error)
{
	struct yt_sector sector;

	if (!session_read_sector(session, sector_number, &sector,
	    error))
		return false;
	if (!yt_projectile_sector_mines_overlay(&sector, mines))
		return false;
	return yt_database_write(&session->door->game.database,
	    (size_t)session_sector_basic_record(session, (float)sector_number),
	    &sector.record, error);
}

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
	    single_sub(original, remaining));
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
cruise_defense_damage_row(float destroyed, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t prefix[] = "The Missiles destroyed";
	static const uint8_t suffix[] = " fighters!";
	char number[64];
	int number_length;

	if (row == NULL || length == NULL)
		return false;
	number_length = qb_str_single(number, sizeof(number), destroyed);
	if (number_length < 0 || sizeof(prefix) - 1U + (size_t)number_length
	    + sizeof(suffix) - 1U > capacity)
		return false;
	memcpy(row, prefix, sizeof(prefix) - 1U);
	memcpy(row + sizeof(prefix) - 1U, number, (size_t)number_length);
	*length = sizeof(prefix) - 1U + (size_t)number_length;
	memcpy(row + *length, suffix, sizeof(suffix) - 1U);
	*length += sizeof(suffix) - 1U;
	return true;
}

static bool
cruise_defense_news_row(const uint8_t *shooter, size_t shooter_length,
    float destroyed, float sector, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t damage[] = "'s Missiles destroyed";
	static const uint8_t location[] = " fighters in sector";
	char destroyed_text[64];
	char sector_text[64];
	int destroyed_length;
	int sector_length;
	size_t row_length;

	if (row == NULL || length == NULL
	    || (shooter == NULL && shooter_length != 0U))
		return false;
	destroyed_length = qb_str_single(destroyed_text,
	    sizeof(destroyed_text), destroyed);
	sector_length = qb_str_single(sector_text, sizeof(sector_text), sector);
	if (destroyed_length < 0 || sector_length < 0
	    || shooter_length + sizeof(damage) - 1U
	    + (size_t)destroyed_length + sizeof(location) - 1U
	    + (size_t)sector_length + 1U > capacity)
		return false;
	if (shooter_length != 0U)
		memcpy(row, shooter, shooter_length);
	row_length = shooter_length;
	memcpy(row + row_length, damage, sizeof(damage) - 1U);
	row_length += sizeof(damage) - 1U;
	memcpy(row + row_length, destroyed_text, (size_t)destroyed_length);
	row_length += (size_t)destroyed_length;
	memcpy(row + row_length, location, sizeof(location) - 1U);
	row_length += sizeof(location) - 1U;
	memcpy(row + row_length, sector_text, (size_t)sector_length);
	row_length += (size_t)sector_length;
	row[row_length++] = '!';
	*length = row_length;
	return true;
}

enum missile_sector_route {
	MISSILE_SECTOR_RETURN,
	MISSILE_SECTOR_POST_IMPACT,
};

static bool
missile_sector(struct yt_session *session, int sector_number,
    float *remaining, int *counterattack, int *xannor_provoker,
	float *last_mine_news_sector, enum missile_sector_route *route,
	struct yt_error *error)
{
	struct yt_sector sector;
	int basic;

	if (route == NULL)
		return false;
	*route = MISSILE_SECTOR_RETURN;
	if (!session_read_sector(session, sector_number, &sector,
	    error))
		return false;
	if (!yt_projectile_sector_has_presence(&sector, sector_number,
	    (int)session_sector_offset(session), &session->player_cache,
	    *xannor_provoker)) {
		*route = MISSILE_SECTOR_POST_IMPACT;
		return true;
	}
	if (!((double)sector.fighters > 0.0))
		goto missile_mines;
	{
		static const uint8_t xannor[] = "The Xannor";
		static const uint8_t mercenaries[] = "Mercenaries";
		static const uint8_t you[] = "YOU";
		uint8_t owner_name[YT_TEXT_FIELD_SIZE];
		uint8_t row[256];
		const uint8_t *initial = xannor;
		size_t owner_length = sizeof(xannor) - 1U;
		size_t row_length;
		bool friendly = false;

		if (sector.fighter_owner == -2.0f) {
			initial = mercenaries;
			owner_length = sizeof(mercenaries) - 1U;
		}
		memcpy(owner_name, initial, owner_length);
		if (sector.fighter_owner > 1.0f) {
			struct yt_player defender;
			uint32_t owner_record = qb_brun_random_record_number(
			    sector.fighter_owner);

			if (!yt_game_read_player(&session->door->game,
			    (int)owner_record, &defender, error)
			    || !yt_player_stored_name(&defender, owner_name,
			    &owner_length, error)
			    || owner_length > sizeof(owner_name)
			    || !yt_session_players_are_friendly(session,
			    (int)sector.fighter_owner, &friendly, error))
				return false;
		}
		if (sector.fighter_owner == (float)session_record(session)) {
			memcpy(owner_name, you, sizeof(you) - 1U);
			owner_length = sizeof(you) - 1U;
			friendly = true;
		}
		if (!yt_projectile_defense_row((float)sector_number, owner_name,
		    owner_length, (double)sector.fighters, row, sizeof(row),
		    &row_length)
		    || !session_present_text(session, row, row_length,
		    SESSION_PRESENT_BOLD_LINE, "cruise missile defense report",
		    error))
			return false;
		if (friendly)
			goto missile_mines;
		yt_present_set_bold(&session->presentation, 1.0f);
		if (!session_sound(session, 2.0f,
		    "cruise missile fighter-defense sound", error))
			return false;
	}
	if (!(*remaining > 0.0f))
		goto missile_mines;
	{
		static const uint8_t dirty_zero[4] = {
			0x00, 0x00, 0x10, 0x00
		};
		struct yt_sector persistence;
		double original_fighters = (double)sector.fighters;
		double remaining_fighters;
		float owner = sector.fighter_owner;
		float saved_missiles = *remaining;
		float destroyed = 0.0f;
		float counter = 1.0f;
		uint8_t row[256];
		size_t row_length;

		while (counter <= saved_missiles
		    && (double)destroyed < original_fighters) {
			float draw;

			if (!yt_random_next(&session->door->game.random, &draw, error))
				return false;
			destroyed = floorf(single_add(single_mul(draw, 5000.0f),
			    destroyed));
			*remaining = single_sub(*remaining, 1.0f);
			if ((double)destroyed >= original_fighters) {
				destroyed = (float)original_fighters;
				break;
			}
			counter = single_add(counter, 1.0f);
		}
		if (!cruise_defense_damage_row(destroyed, row, sizeof(row),
		    &row_length)
		    || !session_present_text(session, row, row_length,
		    SESSION_PRESENT_LINE, "cruise missile destroyed-defense row",
		    error))
			return false;
		remaining_fighters = original_fighters - (double)destroyed;
		if (destroyed > 9.0f
		    && (!cruise_defense_news_row(
		    (const uint8_t *)session->player.name,
		    strlen(session->player.name), destroyed, (float)sector_number,
		    row, sizeof(row), &row_length)
		    || !yt_news_append_bytes(row, row_length, error)))
			return false;
		if (!session_read_sector(session, sector_number, &persistence,
		    error))
			return false;
		persistence.fighters = (float)remaining_fighters;
		if (!yt_record_set_number(&persistence.record, YT_F81,
		    persistence.fighters))
			return false;
		if (remaining_fighters == 0.0) {
			persistence.fighters = 0.0f;
			persistence.fighter_owner = 0.0f;
			if (!yt_record_set_raw_number(&persistence.record, YT_F81,
			    dirty_zero)
			    || !yt_record_set_raw_number(&persistence.record, YT_F85,
			    dirty_zero))
				return false;
		}
		else if (owner == -1.0f) {
			*xannor_provoker = session_record(session);
		}
		if (!yt_database_write(&session->door->game.database,
		    (size_t)session_sector_basic_record(session,
		    (float)sector_number), &persistence.record, error))
			return false;
		if (remaining_fighters == 0.0
		    && (float)sector_number
		    == session->door->game.config.headquarters
		    && owner == -1.0f
		    && !yt_session_xannor_victory(session, error))
			return false;
		if (*remaining < 1.0f)
			return true;
	}

missile_mines:
	for (;;) {
		struct yt_sector mine_sector;
		double observed_mines;
		float destroyed;
		volatile float missiles_after;
		uint8_t row[256];
		size_t row_length;

		if (!session_read_sector(session, sector_number, &mine_sector,
		    error))
			return false;
		observed_mines = (double)mine_sector.mines;
		if (!(observed_mines > 0.0))
			break;
		if (!yt_projectile_sector_mine_hit_row(observed_mines,
		    (float)sector_number, row, sizeof(row), &row_length)
		    || !session_present_text(session, row, row_length,
		    SESSION_PRESENT_BOLD_LINE, "cruise missile sector-mine row",
		    error)
		    || !session_sound(session, 5.0f,
		    "cruise missile sector-mine sound", error))
			return false;
		if (*last_mine_news_sector != (float)sector_number) {
			if (!yt_projectile_sector_mine_news_row(
			    (const uint8_t *)session->player.name,
			    strlen(session->player.name), (float)sector_number,
			    row, sizeof(row), &row_length)
			    || !yt_news_append_bytes(row, row_length,
			    error))
				return false;
			*last_mine_news_sector = (float)sector_number;
		}
		destroyed = (double)*remaining < observed_mines
		    ? *remaining : (float)observed_mines;
		if (!yt_projectile_sector_mine_destroyed_row(destroyed, row,
		    sizeof(row), &row_length)
		    || !session_present_text(session, row, row_length,
		    SESSION_PRESENT_BOLD_LINE, "cruise missile sector-mine row",
		    error)
		    || !session_read_sector(session, sector_number, &mine_sector,
		    error))
			return false;
		mine_sector.mines = (float)(observed_mines - (double)destroyed);
		if (!yt_record_set_number(&mine_sector.record, YT_F129,
		    mine_sector.mines)
		    || !yt_database_write(&session->door->game.database,
		    (size_t)session_sector_basic_record(session,
		    (float)sector_number), &mine_sector.record, error))
			return false;
		missiles_after = *remaining - destroyed;
		*remaining = missiles_after;
		if (*remaining < 1.0f)
			return true;
	}
	for (basic = YT_PLAYER_FIRST_RECORD;
	    basic <= (int)session_sector_offset(session); ++basic) {
		struct yt_player target;
		struct yt_player presentation_target;
		struct yt_projectile_damage_result damage;
		bool scanner_disabled = false;
		uint8_t attacker_name[YT_TEXT_FIELD_SIZE];
		uint8_t victim_name[YT_TEXT_FIELD_SIZE];
		uint8_t first_news[256];
		uint8_t first_direct[256];
		size_t attacker_length;
		size_t victim_length;
		size_t first_news_length;
		size_t first_direct_length;
		char shield_text[64];
		char fighter_text[64];
		char row[256];

		enum yt_projectile_candidate_route candidate_route =
		    yt_projectile_candidate_route(basic, session_record(session),
		    session_player_cache_value(session, basic,
		    YT_PLAYER_CACHE_SECTOR), (float)sector_number,
		    *remaining);

		if (candidate_route == YT_PROJECTILE_CANDIDATE_TERMINATE)
			break;
		if (candidate_route == YT_PROJECTILE_CANDIDATE_SKIP)
			continue;
		/* YT-SUB:974F is called for its exact GET effects; its result is ignored. */
		{
			bool ignored_friendship;

			if (!yt_session_players_are_friendly(session, basic,
			    &ignored_friendship, error))
				return false;
		}
		if (!yt_game_read_player(&session->door->game, basic, &target,
		    error))
			return false;
		if (!yt_projectile_candidate_admitted(basic,
		    session_player_cache_value(session, basic,
		    YT_PLAYER_CACHE_CLOAK), *xannor_provoker))
			continue;
		if (!session_sound(session, 2.0f,
		    "cruise missile player-attack sound", error))
			return false;
		if (!yt_projectile_player_damage(&target, remaining,
		    &session->door->game.random, &damage, error))
			return false;
		scanner_disabled = damage.scanner_disabled;
		session_set_foreground(session, 5.0f);
		if (!yt_game_read_player(&session->door->game, basic,
		    &presentation_target, error))
			return false;
		qb_str_single(shield_text, sizeof(shield_text), target.shields);
		qb_str_double(fighter_text, sizeof(fighter_text),
		    damage.fighters);
		if (!yt_player_stored_name(&session->player, attacker_name,
		    &attacker_length, error)
		    || !yt_player_stored_name(&presentation_target, victim_name,
		    &victim_length, error)
		    || !yt_projectile_attack_first_rows(false,
		    attacker_name, attacker_length, victim_name, victim_length,
		    (float)sector_number, first_news, sizeof(first_news),
		    &first_news_length, first_direct, sizeof(first_direct),
		    &first_direct_length)
		    || !yt_news_append_bytes(first_news, first_news_length,
		    error)
		    || !session_present_text(session, first_direct,
		    first_direct_length, SESSION_PRESENT_BOLD_LINE,
		    "cruise missile player attack first row", error))
			return false;
		snprintf(row, sizeof(row), "shields to%s units and destroying%s "
		    "fighters!", shield_text, fighter_text);
		if (!yt_news_append(row, error))
			return false;
		if (!session_present_text(session, (const uint8_t *)row,
		    strlen(row), SESSION_PRESENT_BOLD_LINE,
		    "cruise missile player attack second row", error))
			return false;
		session_set_foreground(session, 0.0f);
		if (!yt_projectile_player_survives(target.shields)) {
			float mines;
			uint8_t killed_name[YT_TEXT_FIELD_SIZE];
			uint8_t destroyed_row[128];
			uint8_t warning_row[160];
			size_t killed_name_length;
			size_t destroyed_length;
			size_t warning_length;

			if (!yt_game_read_player(&session->door->game, basic,
			    &target, error))
				return false;
			if (!yt_player_stored_name(&target, killed_name,
			    &killed_name_length, error)
			    || !yt_projectile_destroyed_rows(killed_name,
			    killed_name_length, destroyed_row, sizeof(destroyed_row),
			    &destroyed_length, warning_row, sizeof(warning_row),
			    &warning_length))
				return false;
			if (!yt_projectile_victim_mines_overlay(&target, &mines)
			    || !yt_database_write(&session->door->game.database,
			    (size_t)basic, &target.record, error))
				return false;
			yt_present_set_blink(&session->presentation, 1.0f);
			if (!session_present_text(session, destroyed_row,
			    destroyed_length, SESSION_PRESENT_BOLD_LINE,
			    "cruise missile destroyed-player row", error))
				return false;
			if (mines != 0.0f) {
				yt_present_set_blink(&session->presentation, 1.0f);
				if (!session_present_text(session, warning_row,
				    warning_length,
				    SESSION_PRESENT_BOLD_LINE,
				    "cruise missile carried-mine warning", error))
					return false;
			}
			if (mines != 0.0f
			    && !deploy_victim_mines(session, sector_number,
			    mines, error))
				return false;
			if (!yt_session_kill_player(session, basic,
			    (float)session_record(session), true, error))
				return false;
			if (yt_projectile_salvage_admitted(*counterattack,
			    *xannor_provoker)) {
				if (!session_sound(session, 3.0f,
				    "cruise missile salvage sound", error)
				    || !yt_session_salvage_player(session, basic,
				    session_record(session), error))
					return false;
			}
			switch (yt_projectile_death_continuation(*remaining,
			    mines)) {
			case YT_PROJECTILE_DEATH_REENTER_MINES:
				goto missile_mines;
			case YT_PROJECTILE_DEATH_RETURN:
				return true;
			case YT_PROJECTILE_DEATH_NEXT_PLAYER:
				break;
			}
		}
		else {
			struct yt_player persistence;

			if (!yt_game_read_player(&session->door->game, basic,
			    &persistence, error))
				return false;
			if (!yt_projectile_survivor_overlay(&persistence,
			    target.shields, (double)target.fighters,
			    target.danger_scanner, scanner_disabled))
				return false;
			if (!yt_database_write(&session->door->game.database,
			    (size_t)basic, &persistence.record, error))
				return false;
			{
				uint8_t counterattack_raw[4];

				if (yt_projectile_survivor_store_counterattack(
				    session_record(session), basic, counterattack,
				    counterattack_raw))
					session->counterattack_player =
					    (int)qb_mbf32_decode(counterattack_raw);
			}
			return true;
		}
	}
	if (!session_read_sector(session, sector_number, &sector,
	    error))
		return false;
	{
		bool early_return;

		if (!missile_planet_impact(session, sector_number, &sector,
		    remaining, &early_return, error))
			return false;
		if (!early_return)
			*route = MISSILE_SECTOR_POST_IMPACT;
		return true;
	}
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
	if (!yt_planet_stored_name(&planet, planet_name, &planet_name_length,
	    error)
	    || !yt_projectile_planet_attack_rows(true, attacker, attacker_length,
	    planet_name, planet_name_length, (float)sector_number, direct_row,
	    sizeof(direct_row), &direct_length, news_row, sizeof(news_row),
	    &news_length)
	    || !session_present_text(session, direct_row, direct_length,
	    SESSION_PRESENT_LINE, "plasma planet-hit row", error)
	    || !yt_news_append_bytes(news_row, news_length, error)
	    || !session_sound(session, 2.0f, "plasma planet attack sound",
	    error))
		return false;

	original_productivity = single_add(single_add(production[0],
	    production[1]), production[2]);
	while ((stale_ore > 0.0f || production[1] > 0.0f
	    || production[2] > 0.0f) && *energy > 0.0) {
		float draw;
		volatile double product = *energy * 0.000004;
		float quantity = (float)product;

		remaining_ground = single_sub(remaining_ground, quantity);
		for (index = 0U; index < 3U; ++index)
			production[index] = single_sub(production[index], quantity);
		if (!yt_random_next(&session->door->game.random, &draw, error))
			return false;
		*energy -= (double)single_mul(draw, 25000.0f);
	}
	for (index = 0U; index < 3U; ++index) {
		float cap;

		if (production[index] < 0.0f)
			production[index] = 0.0f;
		cap = single_mul(production[index], 10.0f);
		if (stock[index] > cap)
			stock[index] = cap;
	}
	remaining_productivity = single_add(single_add(production[0],
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

static bool
plasma_sector_loaded(struct yt_session *session, int sector_number,
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
				*energy -= (double)single_mul(draw, 25000.0f);
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
			*energy -= (double)single_mul(draw, 25000.0f);
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
		sector.mines = single_sub((float)original_mines, destroyed);
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
		if (session_player_cache_value(session, basic,
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
			saved_foreground = session->foreground;
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
				*energy -= (double)single_mul(draw, 25000.0f);
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
				*energy -= (double)single_mul(draw, 25000.0f);
			}
			if (destroyed_fighters > original_fighters)
				destroyed_fighters = original_fighters;
			if (destroyed_shields > original_shields)
				destroyed_shields = original_shields;
			remaining_fighters = original_fighters - destroyed_fighters;
			remaining_shields = single_sub(original_shields,
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
				mine_persistence.mines = single_add(
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

static bool
projectile_opening(struct yt_session *session, float amount, bool plasma,
    float *last_mine_news_sector, double *energy, float *hop_loss,
    uint8_t *attacker, size_t attacker_capacity, size_t *attacker_length,
    struct yt_error *error)
{
	uint8_t player_name[YT_TEXT_FIELD_SIZE];
	size_t player_name_length;

	if (!plasma) {
		static const uint8_t loading[] =
		    "Loading course into misile targeting computer.";
		static const uint8_t tracking[] = "*** Tracking Report ***";

		*energy = 0.0;
		*hop_loss = 0.0f;
		*attacker_length = 0U;
		if (!session_sound(session, 4.0f,
		    "cruise missile launch sound", error)
		    || !session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE, "cruise missile opening line", error)
		    || !session_present_text(session, loading,
		    sizeof(loading) - 1U, SESSION_PRESENT_RAW,
		    "cruise missile loading text", error)
		    || !session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE, "cruise missile opening line", error))
			return false;
		*last_mine_news_sector = 0.0f;
		return session_present_text(session, tracking,
		    sizeof(tracking) - 1U, SESSION_PRESENT_LINE,
		    "cruise missile tracking row", error);
	}
	static const uint8_t loading[] =
	    "Loading course into targeting computer.";
	static const uint8_t tracking[] = "* Tracking Report *";
	uint8_t row[192];
	size_t row_length;
	float firing_counter;

	if (!yt_player_stored_name(&session->player, player_name,
	    &player_name_length, error))
		return false;
	if (player_name_length > attacker_capacity)
		return false;
	if (player_name_length != 0U)
		memcpy(attacker, player_name, player_name_length);
	*attacker_length = player_name_length;
	yt_projectile_plasma_opening_values(amount, energy, hop_loss);
	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "plasma opening line", error)
	    || !session_present_text(session, loading, sizeof(loading) - 1U,
	    SESSION_PRESENT_RAW, "plasma loading text", error)
	    || !session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "plasma opening line", error)
	    || !session_sound(session, 4.0f, "plasma launch sound", error)
	    || !session_wait(session, 1.0, "plasma launch wait", error)
	    || !yt_projectile_plasma_energy_row(*energy, row, sizeof(row),
	    &row_length)
	    || !session_present_text(session, row, row_length,
	    SESSION_PRESENT_LINE, "plasma opening line", error)
	    || !session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "plasma opening line", error)
	    || !session_wait(session, 1.0, "plasma opening wait", error))
		return false;
	firing_counter = 1.0f;
	while (firing_counter <= amount) {
		if (!yt_projectile_plasma_firing_row(firing_counter, row,
		    sizeof(row), &row_length)
		    || !session_present_text(session, row, row_length,
		    SESSION_PRESENT_LINE, "plasma opening line", error)
		    || !session_sound(session, 7.0f,
		    "plasma bolt firing sound", error))
			return false;
		firing_counter = yt_projectile_plasma_next_firing(firing_counter);
	}
	return session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "plasma opening line", error)
	    && session_present_text(session, tracking, sizeof(tracking) - 1U,
	    SESSION_PRESENT_LINE, "plasma opening line", error)
	    && session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "plasma opening line", error);
}

static bool
route_failure_report(struct yt_session *session, struct yt_error *error)
{
	uint8_t row[96];
	size_t length;

	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "projectile route failure blank", error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "projectile route failure blank", error)
	    || !yt_projectile_route_failure_row(false, row, sizeof(row),
	    &length))
		return false;
	yt_present_set_blink(&session->presentation, 1.0f);
	return session_present_text(session, row, length,
	    SESSION_PRESENT_BOLD_LINE, "projectile route failure row", error);
}

static bool
missile_route_failure_suffix(struct yt_session *session,
    struct yt_error *error)
{
	uint8_t row[32];
	size_t length;

	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "cruise missile self-destruct blank", error)
	    || !yt_projectile_route_failure_row(true, row, sizeof(row),
	    &length))
		return false;
	yt_present_set_blink(&session->presentation, 1.0f);
	return session_present_text(session, row, length,
	    SESSION_PRESENT_BOLD_LINE, "cruise missile self-destruct row", error);
}

static bool
plasma_footer(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t row[] = "Plasma bolts dissipated.";

	return session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "plasma footer leading blank", error)
	    && session_present_text(session, row, sizeof(row) - 1U,
	    SESSION_PRESENT_LINE, "plasma footer row", error)
	    && session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "plasma footer trailing blank", error);
}

static bool
missile_footer(struct yt_session *session, struct yt_error *error)
{
	uint8_t row[32];
	size_t length;

	return yt_projectile_footer_row(row, sizeof(row), &length)
	    && session_present_text(session, row, length, SESSION_PRESENT_LINE,
	    "cruise missile end report", error);
}

static bool
plasma_route_run(struct yt_session *session,
    struct projectile_route_state *route, float *origin, float *destination,
    double *energy, float hop_loss, int *xannor_provoker,
    const uint8_t *attacker, size_t attacker_length, struct yt_error *error)
{
	char first[64];
	char second[64];
	uint8_t row[192];
	size_t steps = 0U;

	for (;;) {
		bool overflow;
		int destination_index;
		float current_hop;

		if (++steps > YT_ROUTE_CAPACITY * 4U)
			return false;
		if (*destination == *origin) {
			destination_index = qb_cint(*destination, &overflow);
			if (overflow)
				return false;
			*origin = 0.0f;
			route->origin = 0.0f;
			session->route_second[0] = (int16_t)destination_index;
			session->route_second[(int16_t)destination_index] = 0;
		} else {
			bool found;
			enum yt_route_outcome outcome;
			float status = 0.0f;

			if (!yt_session_build_route(session, route->origin,
			    route->destination, NULL, false, &found, &outcome,
			    &status, error))
				return false;
			*origin = route->origin;
			*destination = route->destination;
		}
		current_hop = *origin;
		for (;;) {
			struct yt_sector sector;
			int current_index;
			int next_hop;
			int written;

			if (++steps > YT_ROUTE_CAPACITY * 4U)
				return false;
			if (current_hop != *origin)
				*energy -= (double)hop_loss;
			current_index = qb_cint(current_hop, &overflow);
			if (overflow)
				return false;
			next_hop = session->route_second[(int16_t)current_index];
			current_hop = (float)next_hop;
			if (next_hop == 0 || *energy < 1.0)
				return plasma_footer(session, error);
			if (qb_str_single(first, sizeof(first), (float)next_hop) < 0
			    || qb_str_double(second, sizeof(second), floor(*energy)) < 0)
				return false;
			written = snprintf((char *)row, sizeof(row),
			    "Bolt entering sector%s.%s Megawatts remaining.",
			    first, second);
			if (written < 0 || (size_t)written >= sizeof(row)
			    || !session_present_text(session, row, (size_t)written,
			    SESSION_PRESENT_LINE, "plasma route line", error)
			    || !session_wait(session, 0.5, "plasma hop wait", error))
				return false;
			if ((float)next_hop == session->disruption_sectors[0]
			    || (float)next_hop == session->disruption_sectors[1]) {
				float draw;
				float span;

				*origin = (float)next_hop;
				route->origin = *origin;
				if (!yt_random_next(&session->door->game.random, &draw,
				    error))
					return false;
				span = single_sub(session_port_offset(session),
				    session_sector_offset(session));
				*destination = floorf(single_add(single_mul(draw, span),
				    1.0f));
				route->destination = *destination;
				if (!session_present_text(session, NULL, 0U,
				    SESSION_PRESENT_LINE, "plasma route line", error)
				    || qb_str_single(first, sizeof(first),
				    (float)next_hop) < 0
				    || qb_str_single(second, sizeof(second),
				    *destination) < 0)
					return false;
				written = snprintf((char *)row, sizeof(row),
				    "The plasma bolt is deflected by a black hole in "
				    "sector%s to sector%s!", first, second);
				if (written < 0 || (size_t)written >= sizeof(row)
				    || !session_attention_bytes(session, row,
				    (size_t)written, "plasma black-hole attention", error)
				    || !session_present_text(session, NULL, 0U,
				    SESSION_PRESENT_LINE, "plasma route line", error))
					return false;
				break;
			}
			if (!session_read_sector(session, next_hop, &sector, error))
				return false;
			if (!yt_projectile_sector_has_presence(&sector, next_hop,
			    (int)session_sector_offset(session), &session->player_cache,
			    xannor_provoker != NULL ? *xannor_provoker : 0))
				continue;
			if (!plasma_sector_loaded(session, next_hop, &sector, attacker,
			    attacker_length, energy, error))
				return false;
			if (*energy < 1.0)
				return plasma_footer(session, error);
		}
	}
}

static bool
launch_projectile(struct yt_session *session, float *target, float *amount,
    bool plasma, struct projectile_route_state *route,
    float *origin_alias, const uint8_t origin_raw[4],
    const uint8_t target_raw[4], const uint8_t amount_raw[4],
    int *pending_counterattack, int *pending_xannor, struct yt_error *error)
{
	float destination = *target;
	bool overflow;
	bool found;
	int cursor;
	float *missiles = amount;
	double energy;
	float hop_loss;
	uint8_t attacker[YT_PROJECTILE_ATTACKER_CAPACITY];
	size_t attacker_length;
	int local_counterattack = 0;
	int local_xannor_provoker = 0;
	int *counterattack = pending_counterattack != NULL
	    ? pending_counterattack : &local_counterattack;
	int *xannor_provoker = pending_xannor != NULL
	    ? pending_xannor : &local_xannor_provoker;
	float last_mine_news_sector;
	int start = (int)(origin_alias != NULL
	    ? *origin_alias : session->player.sector);

	if (origin_raw != NULL && target_raw != NULL && amount_raw != NULL) {
		route->origin = qb_mbf32_decode(origin_raw);
		route->destination = qb_mbf32_decode(target_raw);
		route->amount = qb_mbf32_decode(amount_raw);
	}
	else {
		route->origin = *origin_alias;
		route->destination = *target;
		route->amount = *missiles;
	}
	(void)qb_cint_mode((double)route->destination,
	    session->presentation.sound.conversion_mode, &overflow);
	if (overflow)
		return true;
	if (!projectile_opening(session, *amount, plasma,
	    &last_mine_news_sector, &energy, &hop_loss, attacker,
	    sizeof(attacker), &attacker_length, error))
		return false;
	if (plasma) {
		float local_origin = (float)start;
		float *origin = origin_alias != NULL ? origin_alias : &local_origin;

		return plasma_route_run(session, route, origin, target, &energy,
		    hop_loss, xannor_provoker, attacker, attacker_length, error);
	}
	for (;;) {
		bool rerouted = false;
		enum yt_route_outcome route_outcome;
		float route_status;

		bool route_success = yt_session_build_route(session,
		    route->origin, route->destination, NULL,
		    yt_projectile_route_avoid_enabled(plasma, *counterattack,
		    session_record(session)), &found, &route_outcome,
		    &route_status, error);

		*origin_alias = route->origin;
		*target = route->destination;
		*missiles = route->amount;
		start = (int)*origin_alias;
		destination = *target;
		if (!route_success)
			return false;
		if (route_outcome == YT_ROUTE_NOT_FOUND
		    && !route_failure_report(session, error)) {
			return false;
		}
		if (route_status != 0.0f) {
			if (!missile_route_failure_suffix(session, error))
				return false;
			return true;
		}
		if ((float)session_record(session) > 2.0f
		    && (float)session_record(session)
		    <= session_sector_offset(session)) {
			struct yt_player shooter;

			if (!yt_game_read_player(&session->door->game,
			    session_record(session), &shooter, error))
				return false;
		}
		cursor = start;
		for (;;) {
			int next = session->route_second[cursor];

			if (!yt_projectile_route_has_next((int16_t)next))
				break;
			if (session_is_disruption_sector(session, (float)next)) {
				uint8_t row[160];
				size_t row_length;
				float draw;

				if (!session_present_text(session, NULL, 0U,
				    SESSION_PRESENT_LINE, "cruise black-hole blank", error)
				    || !yt_projectile_cruise_reroute_row((float)next, row,
				    sizeof(row), &row_length)
				    || !session_attention_bytes(session, row, row_length,
				    "cruise black-hole attention", error)) {
					route->origin = *origin_alias;
					route->destination = *target;
					route->amount = *missiles;
					return false;
				}
				*origin_alias = (float)next;
				route->origin = *origin_alias;
				route->destination = *target;
				route->amount = *missiles;
				if (!yt_random_next(&session->door->game.random, &draw, error))
					return false;
				*target = yt_projectile_cruise_reroute_destination(draw,
				    session_sector_offset(session),
				    session_port_offset(session));
				route->origin = *origin_alias;
				route->destination = *target;
				route->amount = *missiles;
				start = next;
				destination = *target;
				rerouted = true;
				break;
			}
			static const uint8_t union_police_row[] =
			    "The Union Police have destroyed the Missiles!";

			if (yt_projectile_union_police_admitted((float)next,
			    destination, *counterattack, *xannor_provoker)) {
				if (!session_present_text(session, union_police_row,
				    sizeof(union_police_row) - 1U, SESSION_PRESENT_LINE,
				    "Union Police missile row", error))
					return false;
				return true;
			}
			enum missile_sector_route sector_route;

			bool sector_success = missile_sector(session, next, missiles,
			    counterattack, xannor_provoker, &last_mine_news_sector,
			    &sector_route, error);

			route->amount = *missiles;
			if (!sector_success)
				return false;
			if (sector_route == MISSILE_SECTOR_RETURN)
				return true;
			if (yt_projectile_post_impact_route(*missiles)
			    == YT_PROJECTILE_POST_IMPACT_FOOTER)
				break;
			cursor = next;
		}
		if (!rerouted)
			break;
	}
	if (!missile_footer(session, error))
		return false;
	return true;
}

bool
session_launch_projectile(struct yt_session *session, float *origin,
    float *target,
    float *amount, bool plasma, int *counterattack, int *xannor_provoker,
    struct yt_error *error)
{
	struct projectile_route_state route;
	bool result;

	if (counterattack != NULL)
		*counterattack = session->counterattack_player;
	if (xannor_provoker != NULL)
		*xannor_provoker = session->xannor_provoker;
	result = launch_projectile(session, target, amount, plasma, &route, origin,
	    NULL, NULL, NULL, counterattack, xannor_provoker, error);
	if (counterattack != NULL)
		*counterattack = session->counterattack_player;
	if (xannor_provoker != NULL)
		*xannor_provoker = session->xannor_provoker;
	return result;
}

static bool
session_counterlaunch_projectile(struct yt_session *session, float *origin,
    float *target,
    float *amount, bool plasma, int *counterattack, int *xannor_provoker,
    struct yt_error *error)
{
	struct projectile_route_state route;
	bool result;

	if (counterattack != NULL)
		*counterattack = session->counterattack_player;
	if (xannor_provoker != NULL)
		*xannor_provoker = session->xannor_provoker;
	result = launch_projectile(session, target, amount, plasma,
	    &route, origin, NULL, NULL, NULL,
	    counterattack, xannor_provoker, error);
	if (counterattack != NULL)
		*counterattack = session->counterattack_player;
	if (xannor_provoker != NULL)
		*xannor_provoker = session->xannor_provoker;
	return result;
}

static bool
launch_player_counterattack(struct yt_session *session, int *counterattacker,
    int *xannor_provoker, struct yt_error *error)
{
	struct yt_player saved_player;
	struct yt_player attacker;
	struct yt_player debit_player;
	struct yt_player final_player;
	int saved_record;
	float available;
	float target;
	float origin;
	uint8_t stored_name[YT_TEXT_FIELD_SIZE];
	uint8_t saved_name[YT_TEXT_FIELD_SIZE];
	size_t stored_name_length;
	size_t saved_name_length;
	uint8_t terminal_row[256];
	uint8_t news_row[256];
	size_t terminal_length;
	size_t news_length;
	char attacker_name[YT_TEXT_FIELD_SIZE + 1U];
	bool valid_cache;
	uint8_t saved_cloak_raw[4];

	if (counterattacker != NULL)
		*counterattacker = session->counterattack_player;
	session->player_record_carrier = session_record(session);
	saved_record = session_record(session);
	if (*counterattacker < YT_PLAYER_FIRST_RECORD
	    || *counterattacker > (int)session_sector_offset(session)
	    || *counterattacker == saved_record)
		return true;
	if (!yt_game_read_player(&session->door->game, *counterattacker,
	    &attacker, error))
		return false;
	available = attacker.missiles;
	if (qb_mbf32_truth(attacker.record.bytes + YT_F45)
	    || available < 1.0f) {
		*counterattacker = 0;
		return true;
	}

	saved_player = session->player;
	target = saved_player.sector;
	saved_name_length = strlen(saved_player.name);
	if (saved_name_length > sizeof(saved_name))
		saved_name_length = sizeof(saved_name);
	memcpy(saved_name, saved_player.name, saved_name_length);
	valid_cache = yt_player_cache_contains(saved_record);
	if (valid_cache) {
		static const uint8_t zero[4] = {0};

		yt_player_cache_raw(&session->player_cache, saved_record,
		    YT_PLAYER_CACHE_CLOAK, saved_cloak_raw);
		(void)yt_player_cache_set_raw(&session->player_cache, saved_record,
		    YT_PLAYER_CACHE_CLOAK, zero);
	}
	session->player_record_carrier = *counterattacker;
	if (!yt_player_stored_name(&attacker, stored_name,
	    &stored_name_length, error))
		return false;
	memset(attacker_name, 0, sizeof(attacker_name));
	memcpy(attacker_name, stored_name, stored_name_length);
	memcpy(session->player.name, attacker_name, sizeof(session->player.name));

	session->counterlaunch_count = yt_counterlaunch_score_count(
	    (double)saved_player.score, session->counterlaunch_count);
	if (session->counterlaunch_count > available
	    || session->counterlaunch_count == 0.0f) {
		float draw;
		volatile float product;
		volatile float integral;
		volatile float selected;

		if (!yt_random_next(&session->door->game.random, &draw, error))
			return false;
		product = draw * available;
		integral = floorf(product);
		selected = integral + 1.0f;
		session->counterlaunch_count = selected;
	}
	if (!yt_game_read_player(&session->door->game, *counterattacker,
	    &debit_player, error))
		return false;
	yt_counterlaunch_debit_overlay(&debit_player, available,
	    session->counterlaunch_count);
	if (!yt_game_write_player(&session->door->game, *counterattacker,
	    &debit_player, error)
	    || !session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "player counterlaunch blank", error)
	    || !yt_counterlaunch_rows(stored_name, stored_name_length,
	    session->counterlaunch_count, saved_name, saved_name_length,
	    terminal_row, sizeof(terminal_row), &terminal_length, news_row,
	    sizeof(news_row), &news_length)
	    || !session_present_text(session, terminal_row, terminal_length,
	    SESSION_PRESENT_BOLD_LINE, "player counterlaunch row", error)
	    || !yt_news_append_bytes(news_row, news_length, error))
		return false;
	origin = attacker.sector;
	if (!session_counterlaunch_projectile(session, &origin, &target,
	    &session->counterlaunch_count, false, counterattacker,
	    xannor_provoker, error))
		return false;

	*counterattacker = 0;
	session->player_record_carrier = saved_record;
	session->player = saved_player;
	if (valid_cache)
		(void)yt_player_cache_set_raw(&session->player_cache, saved_record,
		    YT_PLAYER_CACHE_CLOAK, saved_cloak_raw);
	if (!yt_game_read_player(&session->door->game, saved_record,
	    &final_player, error))
		return false;
	if (qb_mbf32_truth(final_player.record.bytes + YT_F45))
		session->destroyed = true;
	return session_wait(session, 4.0, "player counterattack wait", error);
}

static bool
projectile_command_error(struct yt_error *error, enum yt_status status,
    const char *operation)
{
	if (error != NULL) {
		error->status = status;
		error->system_error = 0;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
		error->path[0] = '\0';
	}
	return false;
}

bool
yt_session_command_projectile(struct yt_session *session, bool plasma,
    struct yt_error *error)
{
	static const uint8_t no_turns[] =
	    "Sorry but you have no turns left.";
	static const uint8_t no_ammunition[] = "You dont have any!";
	static const uint8_t invalid_sector[] = "Invalid Sector number!";
	static const uint8_t quantity_prompt[] = "Send how many? [0] ?";
	static const uint8_t too_many[] = "You dont have that many!";
	uint8_t prompt[192];
	char response[4096];
	struct qb_val_result parsed;
	enum qb_mbf_status conversion;
	uint8_t target_raw[4];
	uint8_t amount_raw[4];
	uint8_t origin_raw[4];
	size_t prompt_length;
	double integral;
	float displayed = plasma ? session->player.plasma
	    : session->player.missiles;
	float maximum_sector = (float)session_sector_count(session);
	float available;
	float target;
	float amount;
	float origin;
	struct projectile_route_state route;
	int counterattack;
	int xannor_provoker;

	for (;;) {
		if (!session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE, "projectile target opening blank", error)
		    || !session_reload_player(session, error)
		    || !session_reload_player(session, error))
			return false;
		yt_no_turn_gate_result_raw(false, target_raw);
		session->shared_status = qb_mbf32_decode(target_raw);
		if (session->player.turns <= 0.0f) {
			yt_no_turn_gate_result_raw(true, target_raw);
			session->shared_status = qb_mbf32_decode(target_raw);
			return session_present_alert(session, no_turns,
			    sizeof(no_turns) - 1U, "no-turn gate notice", error);
		}
		available = plasma ? session->player.plasma
		    : session->player.missiles;
		if (available < 1.0f)
			return session_present_alert(session, no_ammunition,
			    sizeof(no_ammunition) - 1U,
			    "projectile ammunition refusal", error);
		if (!yt_projectile_target_prompt(plasma, displayed,
		    maximum_sector, prompt, sizeof(prompt), &prompt_length)
		    || !session_present_timed_paged_row(session, prompt,
		    prompt_length, "projectile target prompt", error)
		    || !session_read_number_command(session, response,
		    sizeof(response)))
			return false;
		if (response[0] == '\0')
			return true;
		parsed = qb_val(response);
		if (parsed.overflow)
			return projectile_command_error(error, YT_RANGE,
			    "projectile target VAL");
		target = (float)(parsed.valid ? parsed.value : 0.0);
		conversion = qb_mbf32_encode(target, target_raw);
		if (conversion == QB_MBF_OVERFLOW)
			return projectile_command_error(error, YT_RANGE,
			    "projectile target CSNG");
		target = qb_mbf32_decode(target_raw);
		if (target >= 1.0f && target <= maximum_sector)
			break;
		if (!session_present_alert(session, invalid_sector,
		    sizeof(invalid_sector) - 1U, "projectile invalid sector",
		    error))
			return false;
	}

	if (!session_present_timed_paged_row(session, quantity_prompt,
	    sizeof(quantity_prompt) - 1U, "projectile quantity prompt", error)
	    || !session_read_number_command(session, response, sizeof(response)))
		return false;
	parsed = qb_val(response);
	if (parsed.overflow)
		return projectile_command_error(error, YT_RANGE,
		    "projectile quantity VAL");
	integral = floor(parsed.valid ? parsed.value : 0.0);
	amount = (float)integral;
	conversion = qb_mbf32_encode(amount, amount_raw);
	if (conversion == QB_MBF_OVERFLOW)
		return projectile_command_error(error, YT_RANGE,
		    "projectile quantity CSNG");
	amount = qb_mbf32_decode(amount_raw);
	if (amount < 1.0f)
		return true;
	if (amount > available)
		return session_present_paged_fragment(session, too_many,
		    sizeof(too_many) - 1U);
	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "projectile accepted blank", error))
		return false;
	if (!yt_session_finalize_action(session, error))
		return error == NULL || error->status == YT_OK;
	origin = session->player.sector;
	memcpy(origin_raw, session->player.record.bytes + YT_F57,
	    sizeof(origin_raw));
	yt_projectile_debit_overlay(&session->player, plasma, amount);
	if (!yt_game_write_player(&session->door->game, session_record(session),
	    &session->player, error)
	    || !yt_database_flush(&session->door->game.database, error))
		return false;
	session->destroyed = false;
	counterattack = session->counterattack_player;
	xannor_provoker = session->xannor_provoker;
	if (!launch_projectile(session, &target, &amount, plasma,
	    &route, &origin, origin_raw, target_raw,
	    amount_raw, &counterattack, &xannor_provoker, error))
		return false;
	counterattack = session->counterattack_player;
	xannor_provoker = session->xannor_provoker;
	if (session->counterattack_player != 0
	    && !launch_player_counterattack(session, &counterattack,
	    &xannor_provoker, error))
		return false;
	if (session->xannor_provoker != 0
	    && !yt_session_launch_xannor_retaliation(session,
	    &xannor_provoker, error))
		return false;
	if (session->destroyed)
		return yt_session_common_fatal_self(session, error);
	return true;
}

