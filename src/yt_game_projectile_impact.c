#include "yt_game.h"
#include "yt_game_internal.h"

#include "qb.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

bool
yt_projectile_defense_row(float sector, const uint8_t *owner,
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
	sector_length = qb_str_single(sector_text, sizeof(sector_text), sector);
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
    float cached_sector, float sector, float remaining)
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
yt_projectile_survivor_store_counterattack(int shooter, int player_record,
    int *counterattack, uint8_t raw[4])
{
	if (counterattack == NULL || raw == NULL
	    || !yt_projectile_survivor_sets_counterattack(shooter))
		return false;
	if (qb_mbf32_encode((float)player_record, raw) != QB_MBF_OK)
		return false;
	*counterattack = player_record;
	return true;
}

bool
yt_projectile_damage_iteration(float counter, float saved_missiles)
{
	return counter <= saved_missiles;
}

bool
yt_projectile_cruise_reroute_row(float hop, uint8_t *row,
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
	number_length = qb_str_single(number, sizeof(number), hop);
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

float
yt_projectile_cruise_reroute_destination(float draw,
    float sector_record_offset, float port_record_offset)
{
	float span = qb_single_subtract(port_record_offset,
	    sector_record_offset);
	float selected = floorf(qb_single_multiply(draw, span));

	return qb_single_add(selected, 1.0f);
}

bool
yt_projectile_union_police_admitted(float hop, float destination,
    int counterattack, int xannor_provoker)
{
	return hop < 8.0f && destination < 8.0f
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
	    || sector->port > 0.0f || sector->planet > 0.0f;
	for (player = YT_PLAYER_FIRST_RECORD; player <= last_player; ++player) {
		if (yt_player_cache_value(player_cache, player,
		    YT_PLAYER_CACHE_SECTOR) == (float)sector_number
		    && (yt_player_cache_value(player_cache, player,
		    YT_PLAYER_CACHE_CLOAK) == 0.0f
		    || player == xannor_provoker))
			return true;
	}
	return present;
}




bool
yt_projectile_sector_mine_hit_row(double mines, float sector,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t hit_prefix[] = "The missiles hit";
	static const uint8_t hit_middle[] = " SECTOR MINES in sector";
	char mine_text[64];
	char sector_text[64];
	int mine_length;
	int sector_length;
	size_t row_length;

	if (row == NULL || length == NULL)
		return false;
	sector_length = qb_str_single(sector_text, sizeof(sector_text),
	    sector);
	mine_length = qb_str_double(mine_text, sizeof(mine_text), mines);
	if (sector_length < 0 || mine_length < 0
	    || sizeof(hit_prefix) - 1U + (size_t)mine_length
	    + sizeof(hit_middle) - 1U + (size_t)sector_length + 1U
	    > capacity)
		return false;
	memcpy(row, hit_prefix, sizeof(hit_prefix) - 1U);
	memcpy(row + sizeof(hit_prefix) - 1U, mine_text,
	    (size_t)mine_length);
	row_length = sizeof(hit_prefix) - 1U + (size_t)mine_length;
	memcpy(row + row_length, hit_middle, sizeof(hit_middle) - 1U);
	row_length += sizeof(hit_middle) - 1U;
	memcpy(row + row_length, sector_text, (size_t)sector_length);
	row_length += (size_t)sector_length;
	row[row_length++] = '!';
	*length = row_length;
	return true;
}

bool
yt_projectile_sector_mine_news_row(const uint8_t *shooter,
    size_t shooter_length, float sector, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t middle[] =
	    "'s Missiles hit sector mines in sector";
	char sector_text[64];
	int sector_length;
	size_t row_length = 0U;

	if (row == NULL || length == NULL
	    || (shooter == NULL && shooter_length != 0U))
		return false;
	sector_length = qb_str_single(sector_text, sizeof(sector_text), sector);
	if (sector_length < 0 || shooter_length + sizeof(middle) - 1U
	    + (size_t)sector_length + 1U > capacity)
		return false;
	if (shooter_length != 0U) {
		memcpy(row, shooter, shooter_length);
		row_length = shooter_length;
	}
	memcpy(row + row_length, middle, sizeof(middle) - 1U);
	row_length += sizeof(middle) - 1U;
	memcpy(row + row_length, sector_text, (size_t)sector_length);
	row_length += (size_t)sector_length;
	row[row_length++] = '!';
	*length = row_length;
	return true;
}

bool
yt_projectile_sector_mine_destroyed_row(float destroyed,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "The missile";
	static const uint8_t middle[] = " destroyed";
	static const uint8_t mine[] = " mine";
	char number[64];
	int number_length;
	size_t plural = destroyed > 1.0f ? 1U : 0U;
	size_t row_length;

	if (row == NULL || length == NULL)
		return false;
	number_length = qb_str_single(number, sizeof(number), destroyed);
	if (number_length < 0 || sizeof(prefix) - 1U + plural
	    + sizeof(middle) - 1U + (size_t)number_length
	    + sizeof(mine) - 1U + plural + 1U > capacity)
		return false;
	memcpy(row, prefix, sizeof(prefix) - 1U);
	row_length = sizeof(prefix) - 1U;
	if (plural != 0U)
		row[row_length++] = 's';
	memcpy(row + row_length, middle, sizeof(middle) - 1U);
	row_length += sizeof(middle) - 1U;
	memcpy(row + row_length, number, (size_t)number_length);
	row_length += (size_t)number_length;
	memcpy(row + row_length, mine, sizeof(mine) - 1U);
	row_length += sizeof(mine) - 1U;
	if (plural != 0U)
		row[row_length++] = 's';
	row[row_length++] = '!';
	*length = row_length;
	return true;
}

bool
yt_projectile_survivor_overlay(struct yt_player *player, float shields,
    double fighters, float scanner, bool scanner_disabled)
{
	static const uint8_t scanner_zero[4] = {
		0x00, 0x00, 0x48, 0x00
	};

	if (player == NULL)
		return false;
	player->shields = shields;
	player->fighters = (float)fighters;
	player->danger_scanner = scanner_disabled ? 0.0f : scanner;
	return yt_record_set_number(&player->record, YT_F53, shields)
	    && yt_record_set_number(&player->record, YT_F61,
	    player->fighters)
	    && (scanner_disabled
	    ? yt_record_set_raw_number(&player->record, YT_F93, scanner_zero)
	    : yt_record_set_number(&player->record, YT_F93, scanner));
}

bool
yt_projectile_victim_mines_overlay(struct yt_player *player,
    float *saved_mines)
{
	if (player == NULL || saved_mines == NULL)
		return false;
	*saved_mines = player->mines;
	player->mines = 0.0f;
	return yt_record_set_number(&player->record, YT_F129, 0.0f);
}

bool
yt_projectile_sector_mines_overlay(struct yt_sector *sector,
    float carried_mines)
{
	if (sector == NULL)
		return false;
	sector->mines = qb_single_add(sector->mines, carried_mines);
	return yt_record_set_number(&sector->record, YT_F129, sector->mines);
}

uint32_t
yt_projectile_physical_record(float offset, float logical)
{
	return qb_brun_random_record_number(qb_single_add(offset,
	    logical));
}

bool
yt_projectile_planet_ground_overlay(struct yt_planet *planet,
    float ground, float owner)
{
	if (planet == NULL)
		return false;
	planet->ground_forces = ground;
	planet->owner = owner;
	return yt_record_set_number(&planet->record, YT_F77, ground)
	    && yt_record_set_number(&planet->record, YT_F73, owner);
}

bool
yt_projectile_planet_productivity_overlay(struct yt_planet *planet,
    const float production[3], const float stock[3])
{
	size_t index;

	if (planet == NULL || production == NULL || stock == NULL)
		return false;
	for (index = 0U; index < 3U; ++index) {
		planet->production[index] = production[index];
		planet->stock[index] = stock[index];
		if (!yt_record_set_number(&planet->record, YT_F45 + index * 4U,
		    production[index])
		    || !yt_record_set_number(&planet->record,
		    YT_F57 + index * 4U, stock[index]))
			return false;
	}
	return true;
}

bool
yt_projectile_planet_destroy_overlay(struct yt_planet *planet)
{
	static const uint8_t link_zero[4] = {
		0x00, 0x00, 0x20, 0x00
	};

	if (planet == NULL)
		return false;
	planet->name_length = 0U;
	return yt_record_set_raw_number(&planet->record, YT_F85, link_zero);
}

bool
yt_projectile_sector_unlink_overlay(struct yt_sector *sector)
{
	static const uint8_t link_zero[4] = {
		0x00, 0x00, 0x20, 0x00
	};

	if (sector == NULL)
		return false;
	sector->planet = 0.0f;
	return yt_record_set_raw_number(&sector->record, YT_F93, link_zero);
}

bool
yt_projectile_planet_ground_damage(float ground, float owner,
    float *remaining, struct yt_random *random,
    struct yt_projectile_ground_result *result, struct yt_error *error)
{
	size_t iterations = 0U;

	if (remaining == NULL || random == NULL || result == NULL)
		return false;
	result->ground = ground;
	result->owner = owner;
	result->iterations = 0U;
	while (ground > 0.0f && *remaining > 0.0f) {
		float value;

		if (!yt_random_next(random, &value, error))
			return false;
		ground = qb_single_subtract(ground,
		    qb_single_multiply(value, 25.0f));
		*remaining = qb_single_subtract(*remaining, 1.0f);
		++iterations;
		result->ground = ground;
		result->iterations = iterations;
	}
	ground = floorf(ground);
	if (ground < 1.0f) {
		ground = 0.0f;
		owner = 0.0f;
	}
	result->ground = ground;
	result->owner = owner;
	result->iterations = iterations;
	return true;
}

bool
yt_projectile_planet_productivity_damage(float updater_ore,
    float production[3], float stock[3], float *remaining,
    struct yt_random *random,
    struct yt_projectile_productivity_result *result,
    struct yt_error *error)
{
	float old_total;
	float new_total;
	size_t iterations = 0U;
	size_t index;

	if (production == NULL || stock == NULL || remaining == NULL
	    || random == NULL || result == NULL)
		return false;
	old_total = qb_single_add(qb_single_add(production[0],
	    production[1]), production[2]);
	while ((updater_ore > 0.0f || production[1] > 0.0f
	    || production[2] > 0.0f) && *remaining > 0.0f) {
		for (index = 0U; index < 3U; ++index) {
			float value;

			if (!yt_random_next(random, &value, error))
				return false;
			production[index] = qb_single_subtract(
			    production[index], qb_single_multiply(value,
			    2000.0f));
		}
		*remaining = qb_single_subtract(*remaining, 1.0f);
		++iterations;
	}
	for (index = 0U; index < 3U; ++index) {
		float cap;

		if (production[index] < 0.0f)
			production[index] = 0.0f;
		cap = qb_single_multiply(production[index], 10.0f);
		if (stock[index] > cap)
			stock[index] = cap;
	}
	new_total = qb_single_add(qb_single_add(production[0],
	    production[1]), production[2]);
	result->old_total = old_total;
	result->new_total = new_total;
	result->iterations = iterations;
	return true;
}

bool
yt_projectile_planet_ground_row(float ground, uint8_t *row,
    size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "Ground forces reduced to";
	static const uint8_t suffix[] = "!";
	struct yt_game_row_builder builder = {row, capacity, 0U};

	if (length != NULL)
		*length = 0U;
	if (!yt_game_row_append(&builder, prefix, sizeof(prefix) - 1U)
	    || !yt_game_row_number(&builder, ground, false)
	    || !yt_game_row_append(&builder, suffix, sizeof(suffix) - 1U))
		return false;
	if (length != NULL)
		*length = builder.length;
	return true;
}

bool
yt_projectile_planet_productivity_row(float old_total, float new_total,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "Productivity reduced by";
	static const uint8_t middle[] = " units to";
	static const uint8_t suffix[] = " units!";
	struct yt_game_row_builder builder = {row, capacity, 0U};

	if (length != NULL)
		*length = 0U;
	if (!yt_game_row_append(&builder, prefix, sizeof(prefix) - 1U)
	    || !yt_game_row_number(&builder,
	    qb_single_subtract(old_total, new_total), false)
	    || !yt_game_row_append(&builder, middle, sizeof(middle) - 1U)
	    || !yt_game_row_number(&builder, new_total, false)
	    || !yt_game_row_append(&builder, suffix, sizeof(suffix) - 1U))
		return false;
	if (length != NULL)
		*length = builder.length;
	return true;
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
yt_projectile_route_failure_row(bool caller_suffix, uint8_t *row,
    size_t capacity, size_t *length)
{
	static const uint8_t helper[] =
	    "*** You can't get there without going someplace you dont want to!";
	static const uint8_t suffix[] = "Missles self destructed!";
	const uint8_t *source = caller_suffix ? suffix : helper;
	size_t source_length = caller_suffix
	    ? sizeof(suffix) - 1U : sizeof(helper) - 1U;

	if (length == NULL)
		return false;
	*length = 0U;
	if (capacity < source_length || row == NULL)
		return false;
	memcpy(row, source, source_length);
	*length = source_length;
	return true;
}

bool
yt_projectile_footer_row(uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t footer[] = "*** End of Report ***";

	if (length == NULL)
		return false;
	*length = 0U;
	if (capacity < sizeof(footer) - 1U || row == NULL)
		return false;
	memcpy(row, footer, sizeof(footer) - 1U);
	*length = sizeof(footer) - 1U;
	return true;
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
	uint8_t scanner_raw[4];
	float counter = 1.0f;
	bool scanner_disabled = false;
	size_t iterations = 0U;

	if (target == NULL || remaining == NULL || random == NULL
	    || result == NULL)
		return false;
	original_fighters = (double)target->fighters;
	original_shields = target->shields;
	saved_missiles = *remaining;
	memcpy(scanner_raw, target->record.bytes + YT_F113,
	    sizeof(scanner_raw));
	while (yt_projectile_damage_iteration(counter, saved_missiles)) {
		bool overflow;
		float value;
		float scanner_product;
		int32_t scanner;

		++iterations;
		*remaining = qb_single_subtract(*remaining, 1.0f);
		if (!yt_random_next(random, &value, error))
			return false;
		scanner_product = qb_single_multiply(value, *remaining);
		scanner = qb_cint_mbf32(scanner_raw, 0U, &overflow);
		if (overflow) {
			if (error != NULL) {
				error->status = YT_RANGE;
				(void)snprintf(error->operation,
				    sizeof(error->operation), "%s",
				    "cruise missile scanner CINT");
			}
			return false;
		}
		if (scanner_product > 100.0f && scanner != 0) {
			target->danger_scanner = 0.0f;
			memset(scanner_raw, 0, sizeof(scanner_raw));
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
	result->iterations = iterations;
	return true;
}

bool
yt_projectile_attack_first_rows(bool plasma,
    const uint8_t *attacker, size_t attacker_length,
    const uint8_t *victim, size_t victim_length, float sector,
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
	sector_length = qb_str_single(sector_text, sizeof(sector_text), sector);
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

bool
yt_projectile_friendly_planet_row(const uint8_t *planet,
    size_t planet_length, uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "NOT attacking friendly planet \"";
	static const uint8_t suffix[] = "\"!";
	size_t needed = sizeof(prefix) - 1U + planet_length
	    + sizeof(suffix) - 1U;

	if (length == NULL || (planet == NULL && planet_length != 0U))
		return false;
	*length = 0U;
	if (needed > capacity || (needed != 0U && row == NULL))
		return false;
	memcpy(row, prefix, sizeof(prefix) - 1U);
	if (planet_length != 0U)
		memcpy(row + sizeof(prefix) - 1U, planet, planet_length);
	memcpy(row + sizeof(prefix) - 1U + planet_length,
	    suffix, sizeof(suffix) - 1U);
	*length = needed;
	return true;
}

bool
yt_projectile_planet_attack_rows(bool plasma,
    const uint8_t *attacker, size_t attacker_length,
    const uint8_t *planet, size_t planet_length, float sector,
    uint8_t *direct, size_t direct_capacity, size_t *direct_length,
    uint8_t *news, size_t news_capacity, size_t *news_length)
{
	static const uint8_t missile_direct[] = "The Missiles attacked planet ";
	static const uint8_t missile_news[] = "'s Missiles attacked planet ";
	static const uint8_t plasma_direct[] = "The plasma bolts hit planet ";
	static const uint8_t plasma_news[] = "'s plasma bolts hit planet ";
	static const uint8_t sector_prefix[] = " in sector";
	const uint8_t *direct_prefix = plasma ? plasma_direct : missile_direct;
	size_t direct_prefix_length = plasma
	    ? sizeof(plasma_direct) - 1U : sizeof(missile_direct) - 1U;
	const uint8_t *news_infix = plasma ? plasma_news : missile_news;
	size_t news_infix_length = plasma
	    ? sizeof(plasma_news) - 1U : sizeof(missile_news) - 1U;
	char sector_text[64];
	int sector_length;
	size_t direct_needed;
	size_t news_needed;
	size_t position;

	if (direct_length == NULL || news_length == NULL
	    || (attacker == NULL && attacker_length != 0U)
	    || (planet == NULL && planet_length != 0U))
		return false;
	*direct_length = 0U;
	*news_length = 0U;
	sector_length = qb_str_single(sector_text, sizeof(sector_text), sector);
	if (sector_length < 0)
		return false;
	direct_needed = direct_prefix_length + planet_length
	    + sizeof(sector_prefix) - 1U + (size_t)sector_length + 1U;
	news_needed = attacker_length + news_infix_length + planet_length
	    + sizeof(sector_prefix) - 1U + (size_t)sector_length + 1U;
	if (direct_needed > direct_capacity || news_needed > news_capacity
	    || (direct_needed != 0U && direct == NULL)
	    || (news_needed != 0U && news == NULL))
		return false;
	position = 0U;
	memcpy(direct + position, direct_prefix, direct_prefix_length);
	position += direct_prefix_length;
	if (planet_length != 0U)
		memcpy(direct + position, planet, planet_length);
	position += planet_length;
	memcpy(direct + position, sector_prefix, sizeof(sector_prefix) - 1U);
	position += sizeof(sector_prefix) - 1U;
	memcpy(direct + position, sector_text, (size_t)sector_length);
	position += (size_t)sector_length;
	direct[position++] = '!';
	*direct_length = position;
	position = 0U;
	if (attacker_length != 0U)
		memcpy(news + position, attacker, attacker_length);
	position += attacker_length;
	memcpy(news + position, news_infix, news_infix_length);
	position += news_infix_length;
	if (planet_length != 0U)
		memcpy(news + position, planet, planet_length);
	position += planet_length;
	memcpy(news + position, sector_prefix, sizeof(sector_prefix) - 1U);
	position += sizeof(sector_prefix) - 1U;
	memcpy(news + position, sector_text, (size_t)sector_length);
	position += (size_t)sector_length;
	news[position++] = '!';
	*news_length = position;
	return true;
}
