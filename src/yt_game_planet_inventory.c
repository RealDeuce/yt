#include "yt_game.h"
#include "yt_game_internal.h"

#include "qb.h"

#include <math.h>
#include <string.h>

static float *
take_all_player_item(struct yt_player *player, int item)
{
	switch (item) {
	case 1: return &player->ore;
	case 2: return &player->organics;
	case 3: return &player->equipment;
	case 4: return &player->fighters;
	case 5: return &player->missiles;
	case 6: return &player->mines;
	case 9: return &player->plasma;
	default: return NULL;
	}
}

static float *
take_all_planet_item(struct yt_planet *planet, int item)
{
	switch (item) {
	case 1: return &planet->stock[0];
	case 2: return &planet->stock[1];
	case 3: return &planet->stock[2];
	case 4: return &planet->fighters;
	case 5: return &planet->missiles;
	case 6: return &planet->mines;
	case 9: return &planet->plasma;
	default: return NULL;
	}
}

const char *
yt_planet_take_one_title(int item)
{
	static const char *const titles[7] = {
		"<Take Ore>", "<Take Organics)", "<Take Equipment)",
		"<Take Fighters)", "<Take Missiles)", "<Take Mines)",
		"<Take Plasma Bolts>"
	};

	if (item >= 1 && item <= 6)
		return titles[item - 1];
	return item == 9 ? titles[6] : NULL;
}

void
yt_planet_take_one_player_overlay(struct yt_player *player, int item,
    float amount)
{
	float *selected;

	if (player == NULL)
		return;
	selected = take_all_player_item(player, item);
	if (selected == NULL)
		return;
	if (item == 9)
		*selected = qb_single_add(*selected, amount);
	else
		*selected = (float)qb_double_add((double)*selected,
		    (double)amount);
}

void
yt_planet_take_one_planet_overlay(struct yt_planet *planet, int item,
    double cached_quantity, float amount)
{
	float *selected;

	if (planet == NULL)
		return;
	selected = take_all_planet_item(planet, item);
	if (selected == NULL)
		return;
	*selected = (float)qb_double_subtract(cached_quantity,
	    (double)amount);
}

void
yt_planet_take_all_weapon_player_overlay(struct yt_player *player,
    const double cached_quantity[10], double amount[10])
{
	static const int items[4] = {4, 5, 6, 9};
	size_t index;

	if (player == NULL || cached_quantity == NULL || amount == NULL)
		return;
	memset(amount, 0, 10U * sizeof(*amount));
	amount[4] = floor(cached_quantity[4]);
	for (index = 1; index < 4U; ++index)
		amount[items[index]] = (double)(float)floor(
		    cached_quantity[items[index]]);
	player->fighters = (float)qb_double_add(
	    (double)player->fighters, amount[4]);
	player->missiles = qb_single_add(player->missiles,
	    (float)amount[5]);
	player->mines = qb_single_add(player->mines, (float)amount[6]);
	player->plasma = qb_single_add(player->plasma, (float)amount[9]);
}

void
yt_planet_take_all_weapon_planet_overlay(struct yt_planet *planet,
    const double cached_quantity[10], const double amount[10])
{
	static const int items[4] = {4, 5, 6, 9};
	size_t index;

	if (planet == NULL || cached_quantity == NULL || amount == NULL)
		return;
	for (index = 0; index < 4U; ++index) {
		float *selected = take_all_planet_item(planet, items[index]);

		*selected = (float)qb_double_subtract(
		    cached_quantity[items[index]], amount[items[index]]);
	}
}

float
yt_planet_take_all_commodity_player_overlay(struct yt_player *player,
    int item, double cached_quantity)
{
	float *selected;
	float amount;
	float free_holds;
	double free_double;

	if (player == NULL || item < 1 || item > 3)
		return 0.0f;
	free_double = qb_double_subtract((double)player->holds,
	    (double)player->ore);
	free_double = qb_double_subtract(free_double,
	    (double)player->organics);
	free_double = qb_double_subtract(free_double,
	    (double)player->equipment);
	free_holds = (float)free_double;
	amount = (float)floor(cached_quantity);
	if (free_holds < amount)
		amount = free_holds;
	selected = take_all_player_item(player, item);
	*selected = (float)qb_double_add((double)*selected,
	    (double)amount);
	return amount;
}

void
yt_planet_take_all_commodity_planet_overlay(struct yt_planet *planet,
    int item, double cached_quantity, float amount)
{
	float *selected;

	if (planet == NULL || item < 1 || item > 3)
		return;
	selected = take_all_planet_item(planet, item);
	*selected = (float)qb_double_subtract(cached_quantity,
	    (double)amount);
}

void
yt_planet_transfer_cargo_cache(float rate[10], double quantity[10],
    const double held[3])
{
	size_t index;

	if (rate == NULL || quantity == NULL || held == NULL)
		return;
	for (index = 0; index < 3U; ++index) {
		int item = (int)index + 1;
		float threshold = qb_single_multiply(rate[item], 10.0f);
		double total = qb_double_add(quantity[item], held[index]);

		if (total > (double)threshold)
			rate[item] = (float)qb_double_add(
			    qb_double_divide(floor(total), 10.0), 1.0);
		quantity[item] = qb_double_add(quantity[item], held[index]);
	}
}

void
yt_planet_transfer_cargo_player_overlay(struct yt_player *player)
{
	if (player == NULL)
		return;
	player->ore = 0.0f;
	player->organics = 0.0f;
	player->equipment = 0.0f;
}

void
yt_planet_transfer_cargo_planet_overlay(struct yt_planet *planet,
    const float rate[10], const double quantity[10],
    const float contribution[10])
{
	size_t index;

	if (planet == NULL || rate == NULL || quantity == NULL
	    || contribution == NULL)
		return;
	for (index = 0; index < 3U; ++index) {
		int item = (int)index + 1;

		planet->production[index] = qb_single_subtract(rate[item],
		    contribution[item]);
		planet->stock[index] = (float)quantity[item];
	}
}

void
yt_planet_transfer_direct_player_overlay(struct yt_player *player, int item)
{
	float *selected;

	if (player == NULL)
		return;
	selected = take_all_player_item(player, item);
	if (selected != NULL)
		*selected = 0.0f;
}

void
yt_planet_transfer_direct_planet_overlay(struct yt_planet *planet, int item,
    double cached_quantity, float cached_amount)
{
	float *selected;

	if (planet == NULL)
		return;
	selected = take_all_planet_item(planet, item);
	if (selected != NULL)
		*selected = (float)qb_double_add(cached_quantity,
		    (double)cached_amount);
}

void
yt_planet_transfer_fighter_player_overlay(struct yt_player *player,
    float cached_fighters, float amount)
{
	if (player != NULL)
		player->fighters = (float)qb_double_subtract(
		    (double)cached_fighters, (double)amount);
}

void
yt_planet_transfer_fighter_planet_overlay(struct yt_planet *planet,
    double cached_quantity, float amount)
{
	if (planet != NULL)
		planet->fighters = (float)qb_double_add(cached_quantity,
		    (double)amount);
}

int
yt_planet_transfer_selector_position(const char *command)
{
	const char *position;

	if (command == NULL)
		return 0;
	position = strstr("CSFMB", command);
	return position == NULL ? 0 : (int)(position - "CSFMB") + 1;
}

int
yt_planet_menu_selector_position(const char *command)
{
	static const char selector[] = "F!MPC1234569LTAB$";
	const char *position;

	if (command == NULL)
		return 0;
	position = strstr(selector, command);
	return position == NULL ? 0 : (int)(position - selector) + 1;
}

bool
yt_planet_transfer_cargo_empty(const double held[3])
{
	return held != NULL && held[0] == 0.0 && held[1] == 0.0
	    && held[2] == 0.0;
}

bool
yt_planet_transfer_fighter_rejected(float amount, float cached_fighters)
{
	return amount < 0.0f || (double)amount > (double)cached_fighters;
}

bool
yt_planet_transfer_fighter_amount(const struct qb_val_result *parsed,
    float *amount,
    struct yt_error *error)
{
	enum qb_mbf_status status;

	if (parsed == NULL || amount == NULL)
		return yt_game_error(error, YT_INVALID,
		    "planet Transfer fighter amount arguments");
	if (parsed->overflow)
		return yt_game_error(error, YT_RANGE,
		    "planet Transfer fighter VAL");
	status = qb_val_single_or_zero(parsed, amount);
	if (status == QB_MBF_OVERFLOW)
		return yt_game_error(error, YT_RANGE,
		    "planet Transfer fighter CSNG");
	return true;
}

double
yt_planet_bank_available(float cached_credits, float cached_bank)
{
	return qb_double_add((double)cached_credits,
	    (double)cached_bank);
}

double
yt_planet_bank_remaining(float cached_credits, float cached_bank,
    double target)
{
	double after_target = qb_double_subtract((double)cached_credits,
	    target);

	return qb_double_add(after_target, (double)cached_bank);
}

void
yt_planet_bank_planet_overlay(struct yt_planet *planet, double target)
{
	if (planet != NULL)
		planet->bank = (float)target;
}

float
yt_planet_bank_credit_argument(float cached_bank, double target)
{
	return (float)qb_double_subtract((double)cached_bank, target);
}

double
yt_planet_productivity_units(double spend)
{
	return qb_double_divide(spend, 250.0);
}

void
yt_planet_productivity_cache(float rate[10], double units, float delta[4])
{
	static const float plasma_multiplier = 0x1.0c6f7ap-18f;
	float old_sum;
	float new_sum;
	float old_value;
	float new_value;
	size_t index;

	if (rate == NULL || delta == NULL)
		return;
	old_sum = qb_single_add(qb_single_add(rate[1], rate[2]),
	    rate[3]);
	for (index = 1; index <= 3U; ++index)
		rate[index] = (float)qb_double_add((double)rate[index],
		    units);
	new_sum = qb_single_add(qb_single_add(rate[1], rate[2]),
	    rate[3]);
	delta[0] = qb_single_subtract(floorf(new_sum), floorf(old_sum));
	new_value = floorf(qb_single_divide(new_sum, 2500.0f));
	old_value = floorf(qb_single_divide(old_sum, 2500.0f));
	delta[1] = qb_single_subtract(new_value, old_value);
	new_value = floorf(qb_single_divide(new_sum, 25000.0f));
	old_value = floorf(qb_single_divide(old_sum, 25000.0f));
	delta[2] = qb_single_subtract(new_value, old_value);
	new_value = floorf(qb_single_multiply(new_sum, plasma_multiplier));
	old_value = floorf(qb_single_multiply(old_sum, plasma_multiplier));
	delta[3] = qb_single_subtract(new_value, old_value);
}

float
yt_planet_productivity_credit_argument(double units)
{
	double cost = units * 250.0;
	float single_cost = (float)cost;

	return -single_cost;
}

void
yt_planet_productivity_planet_overlay(struct yt_planet *planet,
    const float rate[10], const double quantity[10],
    const float contribution[10])
{
	yt_planet_transfer_cargo_planet_overlay(planet, rate, quantity,
	    contribution);
}

bool
yt_planet_rename_protected(int current_record, int planet_offset,
    int total_record_marker)
{
	return current_record - planet_offset == 1
	    || current_record == total_record_marker
	    || current_record == total_record_marker - 1;
}

enum yt_planet_rename_name_result
yt_planet_rename_prepare_name(char *name, size_t *length)
{
	size_t normalized;

	if (name == NULL || length == NULL)
		return YT_PLANET_RENAME_EMPTY;
	normalized = qb_title_case_n((uint8_t *)name, strlen(name));
	name[normalized] = '\0';
	if (normalized == 0U) {
		*length = 0U;
		return YT_PLANET_RENAME_EMPTY;
	}
	if (strcmp(name, "The Wanderer") == 0
	    || strcmp(name, "Xannoron") == 0
	    || strcmp(name, "Mercenary Base") == 0) {
		*length = normalized;
		return YT_PLANET_RENAME_RESERVED;
	}
	if (normalized > YT_TEXT_FIELD_SIZE)
		normalized = YT_TEXT_FIELD_SIZE;
	name[normalized] = '\0';
	*length = normalized;
	return YT_PLANET_RENAME_ACCEPTED;
}

void
yt_planet_rename_overlay(struct yt_planet *planet, const char *name,
    size_t length)
{
	if (planet == NULL || name == NULL)
		return;
	if (length > YT_TEXT_FIELD_SIZE)
		length = YT_TEXT_FIELD_SIZE;
	memcpy(planet->name, name, length);
	planet->name[length] = '\0';
	planet->name_length = length;
}
