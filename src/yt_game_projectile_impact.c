#include "yt_game.h"
#include "yt_game_internal.h"

#include "qb.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

bool
yt_projectile_defense_row(uint16_t sector, const uint8_t *owner,
    size_t owner_length, double fighters, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t prefix[] = "Sector:";
	static const uint8_t owner_prefix[] = " defended by ";
	static const uint8_t fighter_prefix[] = " with";
	static const uint8_t suffix[] = " fighters.";
	char sector_text[64];
	char fighter_text[64];
	int sector_length;
	int fighter_length;
	size_t needed;
	size_t position = 0U;

	if (length == NULL || (owner == NULL && owner_length != 0U))
		return false;
	*length = 0U;
	sector_length = qb_str_single(sector_text, sizeof(sector_text),
	    (float)sector);
	fighter_length = qb_str_double(fighter_text, sizeof(fighter_text),
	    fighters);
	if (sector_length < 0 || fighter_length < 0)
		return false;
	needed = sizeof(prefix) - 1U + (size_t)sector_length
	    + sizeof(owner_prefix) - 1U + owner_length
	    + sizeof(fighter_prefix) - 1U + (size_t)fighter_length
	    + sizeof(suffix) - 1U;
	if (needed > capacity || (needed != 0U && row == NULL))
		return false;
	memcpy(row + position, prefix, sizeof(prefix) - 1U);
	position += sizeof(prefix) - 1U;
	memcpy(row + position, sector_text, (size_t)sector_length);
	position += (size_t)sector_length;
	memcpy(row + position, owner_prefix, sizeof(owner_prefix) - 1U);
	position += sizeof(owner_prefix) - 1U;
	if (owner_length != 0U)
		memcpy(row + position, owner, owner_length);
	position += owner_length;
	memcpy(row + position, fighter_prefix, sizeof(fighter_prefix) - 1U);
	position += sizeof(fighter_prefix) - 1U;
	memcpy(row + position, fighter_text, (size_t)fighter_length);
	position += (size_t)fighter_length;
	memcpy(row + position, suffix, sizeof(suffix) - 1U);
	position += sizeof(suffix) - 1U;
	*length = position;
	return true;
}

bool
yt_projectile_candidate_admitted(int candidate, float cached_cloak,
    int xannor_provoker)
{
	return (cached_cloak == 0.0f || candidate == xannor_provoker)
	    && (xannor_provoker == 0 || candidate == xannor_provoker);
}

enum yt_projectile_candidate_route
yt_projectile_candidate_route(int candidate, int shooter,
    int cached_sector, int sector, float remaining)
{
	if (cached_sector != sector)
		return YT_PROJECTILE_CANDIDATE_SKIP;
	if (remaining <= 0.0f)
		return YT_PROJECTILE_CANDIDATE_TERMINATE;
	if (candidate == shooter)
		return YT_PROJECTILE_CANDIDATE_SKIP;
	return YT_PROJECTILE_CANDIDATE_FRIENDSHIP;
}

bool
yt_projectile_player_survives(float shields)
{
	return shields >= 1.0f;
}

bool
yt_projectile_salvage_admitted(int counterattack, int xannor_provoker)
{
	return counterattack == 0 && xannor_provoker == 0;
}

enum yt_projectile_death_route
yt_projectile_death_continuation(float remaining, float saved_mines)
{
	if (remaining > 0.0f && saved_mines > 0.0f)
		return YT_PROJECTILE_DEATH_REENTER_MINES;
	if (remaining < 1.0f)
		return YT_PROJECTILE_DEATH_RETURN;
	return YT_PROJECTILE_DEATH_NEXT_PLAYER;
}

bool
yt_projectile_survivor_sets_counterattack(int shooter)
{
	return shooter != -1;
}

bool
yt_projectile_damage_iteration(float counter, float saved_missiles)
{
	return counter <= saved_missiles;
}

bool
yt_projectile_cruise_reroute_row(uint16_t hop, uint8_t *row,
    size_t capacity, size_t *length)
{
	static const uint8_t prefix[] =
	    "The missiles are deflected by a black hole in sector";
	static const uint8_t suffix[] = "!";
	char number[64];
	int number_length;
	size_t row_length;

	if (row == NULL || length == NULL)
		return false;
	number_length = qb_str_single(number, sizeof(number), (float)hop);
	if (number_length < 0
	    || sizeof(prefix) - 1U + (size_t)number_length + sizeof(suffix) - 1U
	    > capacity)
		return false;
	memcpy(row, prefix, sizeof(prefix) - 1U);
	memcpy(row + sizeof(prefix) - 1U, number, (size_t)number_length);
	row_length = sizeof(prefix) - 1U + (size_t)number_length;
	memcpy(row + row_length, suffix, sizeof(suffix) - 1U);
	row_length += sizeof(suffix) - 1U;
	*length = row_length;
	return true;
}

uint16_t
yt_projectile_cruise_reroute_destination(float draw,
    uint16_t sector_record_offset, uint16_t port_record_offset)
{
	float span = (float)(port_record_offset - sector_record_offset);
	float selected = floorf(qb_single_multiply(draw, span));

	return (uint16_t)selected + 1U;
}

bool
yt_projectile_union_police_admitted(uint16_t hop, float destination,
    int counterattack, int xannor_provoker)
{
	return hop < 8U && destination < 8.0f
	    && counterattack == 0 && xannor_provoker == 0;
}

bool
yt_projectile_sector_has_presence(const struct yt_sector *sector,
    int sector_number, int last_player,
    const struct yt_player_cache *player_cache, int xannor_provoker)
{
	bool present;
	int player;

	if (sector == NULL || player_cache == NULL)
		return false;
	present = sector->mines > 0.0f || sector->fighters > 0.0f
	    || sector->port > 0 || sector->planet > 0;
	for (player = YT_PLAYER_FIRST_RECORD; player <= last_player; ++player) {
		if (yt_player_cache_sector(player_cache, player) == sector_number
		    && (yt_player_cache_cloak(player_cache, player) == 0.0f
		    || player == xannor_provoker))
			return true;
	}
	return present;
}




enum yt_projectile_post_impact_route
yt_projectile_post_impact_route(float remaining)
{
	return remaining > 0.0f ? YT_PROJECTILE_POST_IMPACT_NEXT_HOP
	    : YT_PROJECTILE_POST_IMPACT_FOOTER;
}

bool
yt_projectile_route_has_next(int16_t next_hop)
{
	return next_hop != 0;
}

bool
yt_projectile_route_avoid_enabled(bool plasma, int counterattack, int shooter)
{
	return !plasma && counterattack == 0 && shooter != -1;
}

bool
yt_projectile_player_damage(struct yt_player *target, float *remaining,
    struct yt_random *random,
    struct yt_projectile_damage_result *result, struct yt_error *error)
{
	double original_fighters;
	double fighter_damage = 0.0;
	float original_shields;
	float shield_damage = 0.0f;
	float saved_missiles;
	float counter = 1.0f;
	bool scanner_disabled = false;

	if (target == NULL || remaining == NULL || random == NULL
	    || result == NULL)
		return false;
	original_fighters = (double)target->fighters;
	original_shields = target->shields;
	saved_missiles = *remaining;
	while (yt_projectile_damage_iteration(counter, saved_missiles)) {
		float value;
		float scanner_product;

		*remaining = qb_single_subtract(*remaining, 1.0f);
		if (!yt_random_next(random, &value, error))
			return false;
		scanner_product = qb_single_multiply(value, *remaining);
		if (scanner_product > 100.0f
		    && target->danger_scanner != 0) {
			target->danger_scanner = 0;
			scanner_disabled = true;
		}
		if (!yt_random_next(random, &value, error))
			return false;
		fighter_damage = floor((double)qb_single_multiply(value,
		    4001.0f) + fighter_damage);
		if (!yt_random_next(random, &value, error))
			return false;
		/* The SINGLE draw is promoted for the DOUBLE fighter operand. */
		if ((double)value * original_fighters < fighter_damage) {
			if (!yt_random_next(random, &value, error))
				return false;
			shield_damage = qb_single_add(shield_damage,
			    floorf(qb_single_multiply(value, 1001.0f)));
		}
		if (fighter_damage >= original_fighters
		    && shield_damage >= original_shields)
			break;
		counter = qb_single_add(counter, 1.0f);
	}
	if (fighter_damage > original_fighters)
		fighter_damage = original_fighters;
	if (shield_damage > original_shields)
		shield_damage = original_shields;
	target->fighters = (float)(original_fighters - fighter_damage);
	target->shields = qb_single_subtract(original_shields,
	    shield_damage);
	result->fighters = fighter_damage;
	result->shields = shield_damage;
	result->scanner_disabled = scanner_disabled;
	return true;
}

bool
yt_projectile_attack_first_rows(bool plasma,
    const uint8_t *attacker, size_t attacker_length,
    const uint8_t *victim, size_t victim_length, uint16_t sector,
    uint8_t *news, size_t news_capacity, size_t *news_length,
    uint8_t *direct, size_t direct_capacity, size_t *direct_length)
{
	static const uint8_t missile_news[] = "'s missiles attacked ";
	static const uint8_t missile_direct[] = "The missiles attacked ";
	static const uint8_t plasma_news[] = "'s plasma bolts hit ";
	static const uint8_t plasma_direct[] = "The plasma bolts hit ";
	static const uint8_t middle[] = " in";
	static const uint8_t suffix[] = " reducing";
	const uint8_t *news_infix = plasma ? plasma_news : missile_news;
	size_t news_infix_length = plasma
	    ? sizeof(plasma_news) - 1U : sizeof(missile_news) - 1U;
	const uint8_t *direct_prefix = plasma ? plasma_direct : missile_direct;
	size_t direct_prefix_length = plasma
	    ? sizeof(plasma_direct) - 1U : sizeof(missile_direct) - 1U;
	char sector_text[64];
	int sector_length;
	size_t news_needed;
	size_t direct_needed;
	size_t position;

	if (news_length == NULL || direct_length == NULL
	    || (attacker == NULL && attacker_length != 0U)
	    || (victim == NULL && victim_length != 0U))
		return false;
	*news_length = 0U;
	*direct_length = 0U;
	sector_length = qb_str_single(sector_text, sizeof(sector_text),
	    (float)sector);
	if (sector_length < 0)
		return false;
	news_needed = attacker_length + news_infix_length + victim_length
	    + sizeof(middle) - 1U + (size_t)sector_length
	    + sizeof(suffix) - 1U;
	direct_needed = direct_prefix_length + victim_length
	    + sizeof(middle) - 1U + (size_t)sector_length
	    + sizeof(suffix) - 1U;
	if (news_needed > news_capacity || direct_needed > direct_capacity
	    || (news_needed != 0U && news == NULL)
	    || (direct_needed != 0U && direct == NULL))
		return false;
	position = 0U;
	if (attacker_length != 0U)
		memcpy(news + position, attacker, attacker_length);
	position += attacker_length;
	memcpy(news + position, news_infix, news_infix_length);
	position += news_infix_length;
	if (victim_length != 0U)
		memcpy(news + position, victim, victim_length);
	position += victim_length;
	memcpy(news + position, middle, sizeof(middle) - 1U);
	position += sizeof(middle) - 1U;
	memcpy(news + position, sector_text, (size_t)sector_length);
	position += (size_t)sector_length;
	memcpy(news + position, suffix, sizeof(suffix) - 1U);
	position += sizeof(suffix) - 1U;
	*news_length = position;
	position = 0U;
	memcpy(direct + position, direct_prefix, direct_prefix_length);
	position += direct_prefix_length;
	if (victim_length != 0U)
		memcpy(direct + position, victim, victim_length);
	position += victim_length;
	memcpy(direct + position, middle, sizeof(middle) - 1U);
	position += sizeof(middle) - 1U;
	memcpy(direct + position, sector_text, (size_t)sector_length);
	position += (size_t)sector_length;
	memcpy(direct + position, suffix, sizeof(suffix) - 1U);
	position += sizeof(suffix) - 1U;
	*direct_length = position;
	return true;
}

bool
yt_projectile_destroyed_rows(const uint8_t *victim,
    size_t victim_length, uint8_t *destroyed, size_t destroyed_capacity,
    size_t *destroyed_length, uint8_t *warning, size_t warning_capacity,
    size_t *warning_length)
{
	static const uint8_t destroyed_suffix[] = " was destroyed!";
	static const uint8_t warning_prefix[] = "*** WARNING, ";
	static const uint8_t warning_suffix[] = " had sector mines!";
	size_t destroyed_needed;
	size_t warning_needed;

	if (destroyed_length == NULL || warning_length == NULL
	    || (victim == NULL && victim_length != 0U))
		return false;
	*destroyed_length = 0U;
	*warning_length = 0U;
	destroyed_needed = victim_length + sizeof(destroyed_suffix) - 1U;
	warning_needed = sizeof(warning_prefix) - 1U + victim_length
	    + sizeof(warning_suffix) - 1U;
	if (destroyed_needed > destroyed_capacity
	    || warning_needed > warning_capacity
	    || (destroyed_needed != 0U && destroyed == NULL)
	    || (warning_needed != 0U && warning == NULL))
		return false;
	if (victim_length != 0U)
		memcpy(destroyed, victim, victim_length);
	memcpy(destroyed + victim_length, destroyed_suffix,
	    sizeof(destroyed_suffix) - 1U);
	memcpy(warning, warning_prefix, sizeof(warning_prefix) - 1U);
	if (victim_length != 0U)
		memcpy(warning + sizeof(warning_prefix) - 1U,
		    victim, victim_length);
	memcpy(warning + sizeof(warning_prefix) - 1U + victim_length,
	    warning_suffix, sizeof(warning_suffix) - 1U);
	*destroyed_length = destroyed_needed;
	*warning_length = warning_needed;
	return true;
}
