#include "yt_game.h"
#include "yt_game_internal.h"

#include "qb.h"

#include <math.h>
#include <string.h>

float
yt_planet_landing_attrition(float first_draw, float second_draw,
    float cached_ground_forces)
{
	return floorf(first_draw * second_draw * cached_ground_forces);
}

void
yt_planet_landing_vacancy_overlay(struct yt_planet *planet,
    float ground_forces, int current_player_record)
{
	int owner;

	if (planet == NULL)
		return;
	owner = ground_forces > 0.0f ? current_player_record : 0;
	planet->ground_forces = ground_forces;
	planet->owner = owner;
	(void)yt_record_set_number(&planet->record, YT_F77, ground_forces);
	(void)yt_record_set_number(&planet->record, YT_F73, (float)owner);
}

static bool
landing_join_number(const uint8_t *prefix, size_t prefix_length,
    float number, const uint8_t *suffix, size_t suffix_length,
    uint8_t *output, size_t capacity, size_t *length)
{
	char formatted[64];
	int formatted_length;
	size_t needed;

	if (length == NULL)
		return false;
	*length = 0U;
	formatted_length = qb_str_single(formatted, sizeof(formatted), number);
	if (formatted_length < 0)
		return false;
	needed = prefix_length + (size_t)formatted_length + suffix_length;
	if (needed > capacity || (needed != 0U && output == NULL))
		return false;
	memcpy(output, prefix, prefix_length);
	memcpy(output + prefix_length, formatted, (size_t)formatted_length);
	memcpy(output + prefix_length + (size_t)formatted_length, suffix,
	    suffix_length);
	*length = needed;
	return true;
}

bool
yt_planet_landing_traffic_row(const uint8_t *planet_name,
    size_t planet_name_length, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t prefix[] =
	    "This is space traffic control at planet ";
	size_t needed;

	if (length == NULL || (planet_name == NULL && planet_name_length != 0U))
		return false;
	*length = 0U;
	needed = sizeof(prefix) - 1U + planet_name_length;
	if (needed > capacity || (needed != 0U && row == NULL))
		return false;
	memcpy(row, prefix, sizeof(prefix) - 1U);
	if (planet_name_length != 0U)
		memcpy(row + sizeof(prefix) - 1U, planet_name,
		    planet_name_length);
	*length = needed;
	return true;
}

bool
yt_planet_landing_sensor_row(float fresh_ground_forces,
    float cached_carried_forces, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t first[] = "Sensors report ground forces of";
	static const uint8_t middle[] = " units. You have";
	static const uint8_t suffix[] = ".";
	char defenders[64];
	char carried[64];
	int defenders_length;
	int carried_length;
	size_t needed;

	if (length == NULL)
		return false;
	*length = 0U;
	defenders_length = qb_str_single(defenders, sizeof(defenders),
	    floorf(fresh_ground_forces));
	carried_length = qb_str_single(carried, sizeof(carried),
	    cached_carried_forces);
	if (defenders_length < 0 || carried_length < 0)
		return false;
	needed = sizeof(first) - 1U + (size_t)defenders_length
	    + sizeof(middle) - 1U + (size_t)carried_length
	    + sizeof(suffix) - 1U;
	if (needed > capacity || (needed != 0U && row == NULL))
		return false;
	memcpy(row, first, sizeof(first) - 1U);
	memcpy(row + sizeof(first) - 1U, defenders,
	    (size_t)defenders_length);
	memcpy(row + sizeof(first) - 1U + (size_t)defenders_length,
	    middle, sizeof(middle) - 1U);
	memcpy(row + sizeof(first) - 1U + (size_t)defenders_length
	    + sizeof(middle) - 1U, carried, (size_t)carried_length);
	memcpy(row + needed - (sizeof(suffix) - 1U), suffix,
	    sizeof(suffix) - 1U);
	*length = needed;
	return true;
}

bool
yt_planet_landing_amount_prompt(float cached_carried_forces,
    uint8_t *prompt, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] =
	    "Use how many ground forces? You have";
	static const uint8_t suffix[] = ". [0] ";

	return landing_join_number(prefix, sizeof(prefix) - 1U,
	    cached_carried_forces, suffix, sizeof(suffix) - 1U,
	    prompt, capacity, length);
}

float
yt_planet_landing_commitment(const struct qb_val_result *parsed)
{
	return (float)qb_val_int_or_zero(parsed);
}

bool
yt_planet_landing_commitment_valid(float commitment,
    float cached_carried_forces)
{
	return commitment >= 1.0f && commitment <= cached_carried_forces;
}

bool
yt_planet_landing_unrest_row(float reduced, float original,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] =
	    "ground forces have been reduced to";
	static const uint8_t middle[] = " from";
	static const uint8_t suffix[] = "!";
	char reduced_text[64];
	char original_text[64];
	int reduced_length;
	int original_length;
	size_t needed;

	if (length == NULL)
		return false;
	*length = 0U;
	reduced_length = qb_str_single(reduced_text, sizeof(reduced_text),
	    reduced);
	original_length = qb_str_single(original_text, sizeof(original_text),
	    original);
	if (reduced_length < 0 || original_length < 0)
		return false;
	needed = sizeof(prefix) - 1U + (size_t)reduced_length
	    + sizeof(middle) - 1U + (size_t)original_length
	    + sizeof(suffix) - 1U;
	if (needed > capacity || (needed != 0U && row == NULL))
		return false;
	memcpy(row, prefix, sizeof(prefix) - 1U);
	memcpy(row + sizeof(prefix) - 1U, reduced_text,
	    (size_t)reduced_length);
	memcpy(row + sizeof(prefix) - 1U + (size_t)reduced_length,
	    middle, sizeof(middle) - 1U);
	memcpy(row + sizeof(prefix) - 1U + (size_t)reduced_length
	    + sizeof(middle) - 1U, original_text,
	    (size_t)original_length);
	memcpy(row + needed - (sizeof(suffix) - 1U), suffix,
	    sizeof(suffix) - 1U);
	*length = needed;
	return true;
}

void
yt_planet_assault_player_overlay(struct yt_player *player, float commitment)
{
	if (player == NULL)
		return;
	player->ground_forces -= commitment;
	(void)yt_record_set_number(&player->record, YT_F121,
	    player->ground_forces);
}

void
yt_planet_assault_victory_overlay(struct yt_planet *planet, int owner,
    float attackers)
{
	if (planet == NULL)
		return;
	planet->owner = owner;
	planet->ground_forces = floorf(attackers);
	(void)yt_record_set_number(&planet->record, YT_F73, (float)owner);
	(void)yt_record_set_number(&planet->record, YT_F77,
	    planet->ground_forces);
}

void
yt_planet_assault_failure_overlay(struct yt_planet *planet, float defenders)
{
	if (planet == NULL)
		return;
	planet->ground_forces = floorf(defenders);
	(void)yt_record_set_number(&planet->record, YT_F77,
	    planet->ground_forces);
}

void
yt_planet_assault_round(bool attacker_damage, float amount,
    float *attackers, float *defenders)
{
	if (attackers == NULL || defenders == NULL)
		return;
	if (attacker_damage) {
		*attackers = floorf(*attackers - amount * *defenders);
		if (*attackers < 0.0f)
			*attackers = 0.0f;
	}
	else {
		*defenders = floorf(*defenders - amount * *attackers);
		if (*defenders < 0.0f)
			*defenders = 0.0f;
	}
}

bool
yt_planet_assault_attack_news(const uint8_t *player_name,
    size_t player_name_length, const uint8_t *planet_name,
    size_t planet_name_length, float commitment, uint8_t *row,
    size_t capacity, size_t *length)
{
	static const uint8_t marker[] = " +++ ";
	static const uint8_t attacked[] = " attacked planet ";
	static const uint8_t with[] = " with";
	static const uint8_t suffix[] = " ground forces!";
	char number[64];
	int number_length;
	size_t needed;
	size_t cursor = 0U;

	if (length == NULL || (player_name == NULL && player_name_length != 0U)
	    || (planet_name == NULL && planet_name_length != 0U))
		return false;
	*length = 0U;
	number_length = qb_str_single(number, sizeof(number), commitment);
	if (number_length < 0)
		return false;
	needed = sizeof(marker) - 1U + player_name_length
	    + sizeof(attacked) - 1U + planet_name_length + sizeof(with) - 1U
	    + (size_t)number_length + sizeof(suffix) - 1U;
	if (needed > capacity || (needed != 0U && row == NULL))
		return false;
	memcpy(row + cursor, marker, sizeof(marker) - 1U);
	cursor += sizeof(marker) - 1U;
	if (player_name_length != 0U) {
		memcpy(row + cursor, player_name, player_name_length);
		cursor += player_name_length;
	}
	memcpy(row + cursor, attacked, sizeof(attacked) - 1U);
	cursor += sizeof(attacked) - 1U;
	if (planet_name_length != 0U) {
		memcpy(row + cursor, planet_name, planet_name_length);
		cursor += planet_name_length;
	}
	memcpy(row + cursor, with, sizeof(with) - 1U);
	cursor += sizeof(with) - 1U;
	memcpy(row + cursor, number, (size_t)number_length);
	cursor += (size_t)number_length;
	memcpy(row + cursor, suffix, sizeof(suffix) - 1U);
	*length = needed;
	return true;
}

bool
yt_planet_assault_status_row(bool attacker_damage, float remaining,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t attacker[] = "Your forces remaining  :";
	static const uint8_t defender[] = "Ground forces remaining:";
	static const uint8_t suffix[] = "!";
	const uint8_t *prefix = attacker_damage ? attacker : defender;
	size_t prefix_length = attacker_damage
	    ? sizeof(attacker) - 1U : sizeof(defender) - 1U;

	return landing_join_number(prefix, prefix_length, remaining,
	    suffix, sizeof(suffix) - 1U, row, capacity, length);
}

bool
yt_planet_assault_capture_news(const uint8_t *player_name,
    size_t player_name_length, const uint8_t *planet_name,
    size_t planet_name_length, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t marker[] = " +++ ";
	static const uint8_t captured[] = " captured planet ";
	static const uint8_t suffix[] = "!";
	size_t needed;
	size_t cursor = 0U;

	if (length == NULL || (player_name == NULL && player_name_length != 0U)
	    || (planet_name == NULL && planet_name_length != 0U))
		return false;
	*length = 0U;
	needed = sizeof(marker) - 1U + player_name_length
	    + sizeof(captured) - 1U + planet_name_length + sizeof(suffix) - 1U;
	if (needed > capacity || (needed != 0U && row == NULL))
		return false;
	memcpy(row + cursor, marker, sizeof(marker) - 1U);
	cursor += sizeof(marker) - 1U;
	if (player_name_length != 0U) {
		memcpy(row + cursor, player_name, player_name_length);
		cursor += player_name_length;
	}
	memcpy(row + cursor, captured, sizeof(captured) - 1U);
	cursor += sizeof(captured) - 1U;
	if (planet_name_length != 0U) {
		memcpy(row + cursor, planet_name, planet_name_length);
		cursor += planet_name_length;
	}
	memcpy(row + cursor, suffix, sizeof(suffix) - 1U);
	*length = needed;
	return true;
}

bool
yt_planet_assault_failure_row(float defenders, bool news,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t marker[] = " +++ ";
	static const uint8_t prefix[] =
	    "Attack Failed! Ground Forces remaining:";
	static const uint8_t suffix[] = "!";
	uint8_t screen[128];
	size_t screen_length;

	if (!landing_join_number(prefix, sizeof(prefix) - 1U,
	    floorf(defenders), suffix, sizeof(suffix) - 1U,
	    screen, sizeof(screen), &screen_length))
		return false;
	if (length == NULL)
		return false;
	*length = 0U;
	if (screen_length + (news ? sizeof(marker) - 1U : 0U) > capacity
	    || (row == NULL && screen_length != 0U))
		return false;
	if (news) {
		memcpy(row, marker, sizeof(marker) - 1U);
		memcpy(row + sizeof(marker) - 1U, screen, screen_length);
		*length = sizeof(marker) - 1U + screen_length;
	}
	else {
		memcpy(row, screen, screen_length);
		*length = screen_length;
	}
	return true;
}
