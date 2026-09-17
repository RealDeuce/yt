#include "yt_game.h"
#include "yt_game_internal.h"

#include "qb.h"

#include <math.h>
#include <string.h>

bool
yt_genesis_confirmation_prompt(const uint8_t *trader, size_t trader_length,
    uint8_t *prompt, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "Are you that Trader ";
	static const uint8_t suffix[] = " [y/N]";
	size_t needed;

	if (length == NULL || (trader == NULL && trader_length != 0U))
		return false;
	*length = 0U;
	needed = sizeof(prefix) - 1U + trader_length + sizeof(suffix) - 1U;
	if (needed > capacity || (needed != 0U && prompt == NULL))
		return false;
	memcpy(prompt, prefix, sizeof(prefix) - 1U);
	if (trader_length != 0U)
		memcpy(prompt + sizeof(prefix) - 1U, trader, trader_length);
	memcpy(prompt + sizeof(prefix) - 1U + trader_length,
	    suffix, sizeof(suffix) - 1U);
	*length = needed;
	return true;
}

bool
yt_genesis_insufficient_rows(float required, float owned,
    uint8_t *first, size_t first_capacity, size_t *first_length,
    uint8_t *second, size_t second_capacity, size_t *second_length)
{
	static const uint8_t first_prefix[] =
	    "You are not up to the challenge. You must own";
	static const uint8_t first_suffix[] = " ports before you are powerful";
	static const uint8_t second_prefix[] =
	    "enough to initiate Genesis. You are";
	static const uint8_t second_suffix[] =
	    " short of fulfilling the prophesy.";
	volatile float shortfall = required - owned;
	char required_text[64];
	char shortfall_text[64];
	int required_length;
	int shortfall_length;
	size_t needed_first;
	size_t needed_second;

	if (first_length == NULL || second_length == NULL)
		return false;
	*first_length = 0U;
	*second_length = 0U;
	required_length = qb_str_single(required_text, sizeof(required_text),
	    required);
	shortfall_length = qb_str_single(shortfall_text, sizeof(shortfall_text),
	    shortfall);
	if (required_length < 0 || shortfall_length < 0)
		return false;
	needed_first = sizeof(first_prefix) - 1U + (size_t)required_length
	    + sizeof(first_suffix) - 1U;
	needed_second = sizeof(second_prefix) - 1U + (size_t)shortfall_length
	    + sizeof(second_suffix) - 1U;
	if (needed_first > first_capacity || needed_second > second_capacity
	    || (needed_first != 0U && first == NULL)
	    || (needed_second != 0U && second == NULL))
		return false;
	memcpy(first, first_prefix, sizeof(first_prefix) - 1U);
	memcpy(first + sizeof(first_prefix) - 1U, required_text,
	    (size_t)required_length);
	memcpy(first + sizeof(first_prefix) - 1U + (size_t)required_length,
	    first_suffix, sizeof(first_suffix) - 1U);
	memcpy(second, second_prefix, sizeof(second_prefix) - 1U);
	memcpy(second + sizeof(second_prefix) - 1U, shortfall_text,
	    (size_t)shortfall_length);
	memcpy(second + sizeof(second_prefix) - 1U + (size_t)shortfall_length,
	    second_suffix, sizeof(second_suffix) - 1U);
	*first_length = needed_first;
	*second_length = needed_second;
	return true;
}

bool
yt_main_fighters_sector_overlay(struct yt_sector *sector,
    const uint8_t desired_raw[4], int player_record)
{
	uint8_t owner_raw[4];

	if (sector == NULL || desired_raw == NULL
	    || qb_mbf32_encode((float)player_record, owner_raw)
	    == QB_MBF_OVERFLOW
	    || !yt_record_set_raw_number(&sector->record, YT_F81, desired_raw)
	    || !yt_record_set_raw_number(&sector->record, YT_F85, owner_raw))
		return false;
	sector->fighters = qb_mbf32_decode(desired_raw);
	sector->fighter_owner = qb_mbf32_decode(owner_raw);
	return true;
}

bool
yt_main_fighters_player_overlay(struct yt_player *player, float remaining)
{
	uint8_t remaining_raw[4];

	if (player == NULL
	    || qb_mbf32_encode(remaining, remaining_raw) == QB_MBF_OVERFLOW
	    || !yt_record_set_raw_number(&player->record, YT_F61,
	    remaining_raw))
		return false;
	player->fighters = qb_mbf32_decode(remaining_raw);
	return true;
}


bool
yt_planet_garrison_prompt(float player_forces, float planet_forces,
    uint8_t *prompt, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] =
	    "Drop how many ground force units on the planet?";
	static const uint8_t suffix[] = " Available ->";
	volatile float available = player_forces + planet_forces;
	char number[64];
	int number_length;
	size_t needed;

	if (length == NULL)
		return false;
	*length = 0U;
	number_length = qb_str_single(number, sizeof(number), available);
	if (number_length < 0)
		return false;
	needed = sizeof(prefix) - 1U + (size_t)number_length
	    + sizeof(suffix) - 1U;
	if (needed > capacity || (needed != 0U && prompt == NULL))
		return false;
	memcpy(prompt, prefix, sizeof(prefix) - 1U);
	memcpy(prompt + sizeof(prefix) - 1U, number, (size_t)number_length);
	memcpy(prompt + sizeof(prefix) - 1U + (size_t)number_length,
	    suffix, sizeof(suffix) - 1U);
	*length = needed;
	return true;
}

float
yt_planet_garrison_after(float player_forces, float desired,
    float planet_forces)
{
	volatile float subtracted = player_forces - desired;
	volatile float result = subtracted + planet_forces;

	return result;
}

void
yt_planet_garrison_overlay(struct yt_planet *planet, float desired,
    int player_record)
{
	static const uint8_t dirty_zero[4] = {0x00, 0x00, 0x40, 0x00};

	if (planet == NULL)
		return;
	planet->ground_forces = desired;
	(void)yt_record_set_number(&planet->record, YT_F77, desired);
	(void)yt_record_set_raw_number(&planet->record, YT_F73, dirty_zero);
	planet->owner = 0.0f;
	if (desired >= 1.0f && player_record != 0) {
		planet->owner = (float)player_record;
		(void)yt_record_set_number(&planet->record, YT_F73,
		    planet->owner);
	}
}

void
yt_planet_garrison_player_overlay(struct yt_player *player, float remaining)
{
	volatile float integral = floorf(remaining);

	if (player == NULL)
		return;
	(void)yt_record_set_number(&player->record, YT_F121, integral);
}

bool
yt_planet_garrison_success_row(float desired, uint8_t *row,
    size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "Ground force strength now at";
	static const uint8_t suffix[] = " units!";
	char number[64];
	int number_length;
	size_t needed;

	if (length == NULL)
		return false;
	*length = 0U;
	number_length = qb_str_single(number, sizeof(number), desired);
	if (number_length < 0)
		return false;
	needed = sizeof(prefix) - 1U + (size_t)number_length
	    + sizeof(suffix) - 1U;
	if (needed > capacity || (needed != 0U && row == NULL))
		return false;
	memcpy(row, prefix, sizeof(prefix) - 1U);
	memcpy(row + sizeof(prefix) - 1U, number, (size_t)number_length);
	memcpy(row + sizeof(prefix) - 1U + (size_t)number_length,
	    suffix, sizeof(suffix) - 1U);
	*length = needed;
	return true;
}

float
yt_planet_landing_attrition(float first_draw, float second_draw,
    float cached_ground_forces)
{
	volatile float product = first_draw * second_draw;
	volatile float scaled = product * cached_ground_forces;

	return floorf(scaled);
}

void
yt_planet_landing_vacancy_overlay(struct yt_planet *planet,
    float ground_forces, int current_player_record)
{
	float owner;

	if (planet == NULL)
		return;
	owner = ground_forces > 0.0f ? (float)current_player_record : 0.0f;
	planet->ground_forces = ground_forces;
	planet->owner = owner;
	(void)yt_record_set_number(&planet->record, YT_F77, ground_forces);
	(void)yt_record_set_number(&planet->record, YT_F73, owner);
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
yt_planet_landing_commitment(const char *response)
{
	struct qb_val_result parsed;
	volatile double integral;

	if (response == NULL)
		return 0.0f;
	parsed = qb_val(response);
	integral = floor(parsed.valid ? parsed.value : 0.0);
	return (float)integral;
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
	volatile float remaining;

	if (player == NULL)
		return;
	remaining = player->ground_forces - commitment;
	player->ground_forces = remaining;
	(void)yt_record_set_number(&player->record, YT_F121, remaining);
}

void
yt_planet_assault_victory_overlay(struct yt_planet *planet, float owner,
    float attackers)
{
	volatile float integral = floorf(attackers);

	if (planet == NULL)
		return;
	planet->owner = owner;
	planet->ground_forces = integral;
	(void)yt_record_set_number(&planet->record, YT_F73, owner);
	(void)yt_record_set_number(&planet->record, YT_F77, integral);
}

void
yt_planet_assault_failure_overlay(struct yt_planet *planet, float defenders)
{
	volatile float integral = floorf(defenders);

	if (planet == NULL)
		return;
	planet->ground_forces = integral;
	(void)yt_record_set_number(&planet->record, YT_F77, integral);
}

void
yt_planet_assault_round(bool attacker_damage, float amount,
    float *attackers, float *defenders)
{
	volatile float product;
	volatile float reduced;

	if (attackers == NULL || defenders == NULL)
		return;
	if (attacker_damage) {
		product = amount * *defenders;
		reduced = *attackers - product;
		*attackers = floorf(reduced);
		if (*attackers < 0.0f)
			*attackers = 0.0f;
	}
	else {
		product = amount * *attackers;
		reduced = *defenders - product;
		*defenders = floorf(reduced);
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
	    screen, sizeof(screen), &screen_length) || length == NULL)
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
