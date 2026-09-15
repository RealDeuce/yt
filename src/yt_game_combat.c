#include "yt_game.h"

#include "qb.h"

#include <string.h>

enum yt_hostile_attack_admission
yt_hostile_attack_admit(float ship_fighters, float commitment)
{
	if (ship_fighters < 1.0f)
		return YT_HOSTILE_ATTACK_NO_FIGHTERS;
	if (commitment > ship_fighters)
		return YT_HOSTILE_ATTACK_TOO_MANY;
	if (commitment < 1.0f)
		return YT_HOSTILE_ATTACK_LESS_THAN_ONE;
	return YT_HOSTILE_ATTACK_ADMITTED;
}

float
yt_hostile_attack_quantum(double remaining_attacker,
    double remaining_defender)
{
	double minimum = remaining_attacker < remaining_defender
	    ? remaining_attacker : remaining_defender;
	volatile double divided = minimum / 20.0;
	float quantum = (float)qb_int(divided);

	return quantum < 1.0f ? 1.0f : quantum;
}

bool
yt_hostile_attack_loses_attacker(float cloak, float draw)
{
	volatile float cloak_term = cloak / 10.0f;
	volatile float total = cloak_term + draw;

	return total < 0.44999998807907104f;
}

enum yt_hostile_surrender_route
yt_hostile_surrender_route(float owner)
{
	if (owner > 1.0f)
		return YT_HOSTILE_SURRENDER_PLAYER;
	if (owner == -1.0f)
		return YT_HOSTILE_SURRENDER_XANNOR;
	if (owner == -2.0f)
		return YT_HOSTILE_SURRENDER_MERCENARY;
	return YT_HOSTILE_SURRENDER_QUIET;
}

bool
yt_fighter_shield_spill_step(double *fighters, float *shields, float draw)
{
	float quantum;

	if (fighters == NULL || shields == NULL
	    || *fighters <= 0.0 || *shields <= 0.0f)
		return false;
	quantum = *fighters > 100.0 && *shields > 100.0f ? 100.0f : 1.0f;
	if (draw >= 0.5f) {
		volatile double reduced = *fighters - (double)quantum;

		*fighters = reduced;
	}
	else {
		volatile float reduced = *shields - quantum;

		*shields = reduced;
	}
	return true;
}

bool
yt_fighter_shield_spill_rows(double fighters, float shields,
    uint8_t *fighter_row, size_t fighter_capacity, size_t *fighter_length,
    uint8_t *shield_row, size_t shield_capacity, size_t *shield_length)
{
	static const char fighter_prefix[] = "Fighters remaining:";
	static const char shield_prefix[] = "Shields reduced to:";
	char fighter_number[64];
	char shield_number[64];
	int fighter_number_length;
	int shield_number_length;
	size_t first_needed;
	size_t second_needed;

	if (fighter_length == NULL || shield_length == NULL)
		return false;
	*fighter_length = 0;
	*shield_length = 0;
	fighter_number_length = qb_str_double(fighter_number,
	    sizeof(fighter_number), fighters);
	shield_number_length = qb_str_single(shield_number,
	    sizeof(shield_number), shields);
	if (fighter_number_length < 0 || shield_number_length < 0)
		return false;
	first_needed = sizeof(fighter_prefix) - 1U
	    + (size_t)fighter_number_length;
	second_needed = sizeof(shield_prefix) - 1U
	    + (size_t)shield_number_length;
	if (first_needed > fighter_capacity || second_needed > shield_capacity
	    || (first_needed != 0 && fighter_row == NULL)
	    || (second_needed != 0 && shield_row == NULL))
		return false;
	memcpy(fighter_row, fighter_prefix, sizeof(fighter_prefix) - 1U);
	memcpy(fighter_row + sizeof(fighter_prefix) - 1U, fighter_number,
	    (size_t)fighter_number_length);
	memcpy(shield_row, shield_prefix, sizeof(shield_prefix) - 1U);
	memcpy(shield_row + sizeof(shield_prefix) - 1U, shield_number,
	    (size_t)shield_number_length);
	*fighter_length = first_needed;
	*shield_length = second_needed;
	return true;
}

bool
yt_hostile_defeated_row(double fighters, uint8_t *row,
    size_t capacity, size_t *length)
{
	static const char prefix[] =
	    "You defeated all the fighters and have";
	static const char suffix[] = " left.";
	char number[64];
	int number_length;
	size_t needed;

	if (length == NULL)
		return false;
	*length = 0;
	number_length = qb_str_double(number, sizeof(number), fighters);
	if (number_length < 0)
		return false;
	needed = sizeof(prefix) - 1U + (size_t)number_length
	    + sizeof(suffix) - 1U;
	if (needed > capacity || (needed != 0 && row == NULL))
		return false;
	memcpy(row, prefix, sizeof(prefix) - 1U);
	memcpy(row + sizeof(prefix) - 1U, number, (size_t)number_length);
	memcpy(row + sizeof(prefix) - 1U + (size_t)number_length,
	    suffix, sizeof(suffix) - 1U);
	*length = needed;
	return true;
}

bool
yt_xannor_attack_reward_rows(const uint8_t *name, size_t name_length,
    float bonus, double defenders_destroyed,
    uint8_t *display, size_t display_capacity, size_t *display_length,
    uint8_t *news, size_t news_capacity, size_t *news_length)
{
	static const uint8_t collect[] = "Collect";
	static const uint8_t collected[] = " collected";
	static const uint8_t middle[] = " turns bonus for destroying";
	static const uint8_t suffix[] = " Xannor!!";
	char bonus_text[64];
	char loss_text[64];
	int bonus_length;
	int loss_length;
	size_t clause_length;
	size_t display_needed;
	size_t news_needed;

	if (display_length == NULL || news_length == NULL
	    || (name == NULL && name_length != 0U))
		return false;
	*display_length = 0U;
	*news_length = 0U;
	bonus_length = qb_str_single(bonus_text, sizeof(bonus_text), bonus);
	loss_length = qb_str_double(loss_text, sizeof(loss_text),
	    defenders_destroyed);
	if (bonus_length < 0 || loss_length < 0)
		return false;
	clause_length = (size_t)bonus_length + sizeof(middle) - 1U
	    + (size_t)loss_length + sizeof(suffix) - 1U;
	display_needed = sizeof(collect) - 1U + clause_length;
	news_needed = name_length + sizeof(collected) - 1U + clause_length;
	if (display_needed > display_capacity || news_needed > news_capacity
	    || (display_needed != 0U && display == NULL)
	    || (news_needed != 0U && news == NULL))
		return false;
	memcpy(display, collect, sizeof(collect) - 1U);
	memcpy(display + sizeof(collect) - 1U, bonus_text,
	    (size_t)bonus_length);
	memcpy(display + sizeof(collect) - 1U + (size_t)bonus_length,
	    middle, sizeof(middle) - 1U);
	memcpy(display + sizeof(collect) - 1U + (size_t)bonus_length
	    + sizeof(middle) - 1U, loss_text, (size_t)loss_length);
	memcpy(display + display_needed - (sizeof(suffix) - 1U), suffix,
	    sizeof(suffix) - 1U);
	if (name_length != 0U)
		memcpy(news, name, name_length);
	memcpy(news + name_length, collected, sizeof(collected) - 1U);
	memcpy(news + name_length + sizeof(collected) - 1U,
	    display + sizeof(collect) - 1U, clause_length);
	*display_length = display_needed;
	*news_length = news_needed;
	return true;
}

float
yt_xannor_attack_bonus(double defenders_destroyed, float turns,
    float turns_per_day)
{
	volatile double quotient = defenders_destroyed / 256000.0;
	float bonus = (float)qb_int(quotient);
	volatile float sum = turns + bonus;

	if (sum > turns_per_day) {
		volatile float clamped = turns_per_day - turns;

		bonus = clamped;
	}
	return bonus;
}

bool
yt_bribe_ordinary_forces(float owner, double defenders,
    double ship_fighters, float draw)
{
	return owner == -1.0f || (defenders > ship_fighters
	    && draw < 0.33000001311302185f);
}

bool
yt_bribe_mercenary_forces(double defenders, double ship_fighters,
    float first, float second, bool sticky)
{
	return first < 0.05000000074505806f
	    || (ship_fighters < defenders
	    && second > 0.8999999761581421f) || sticky;
}

double
yt_bribe_offer_threshold(double defenders, float draw)
{
	volatile double product = defenders * (double)draw;
	volatile double doubled = product * 2.0;
	volatile double threshold = doubled + defenders;

	return threshold;
}

bool
yt_bribe_offer_accepted(float offer, double credits, double threshold)
{
	return (double)offer <= credits && (double)offer >= threshold;
}

enum yt_bribe_forced_admission
yt_bribe_forced_admit(double ship_fighters, float shields,
    bool mercenary_fatal_gate, float commitment)
{
	if (mercenary_fatal_gate && ship_fighters < 1.0 && shields < 1.0f)
		return YT_BRIBE_FORCED_FATAL;
	if (commitment < 1.0f)
		return YT_BRIBE_FORCED_LESS_THAN_ONE;
	return YT_BRIBE_FORCED_ATTACK;
}

enum yt_sector_mine_admission
yt_sector_mine_admit(float carried, float amount)
{
	if (amount < 1.0f)
		return YT_SECTOR_MINE_BELOW_ONE;
	if (amount > carried)
		return YT_SECTOR_MINE_ABOVE_CARRIED;
	return YT_SECTOR_MINE_ACCEPTED;
}


bool
yt_no_turn_gate_denied(float turns)
{
	return turns <= 0.0f;
}

void
yt_no_turn_gate_result_raw(bool denied, uint8_t raw[4])
{
	static const uint8_t false_value[4] = {0x00, 0x00, 0x7d, 0x00};
	static const uint8_t true_value[4] = {0x00, 0x00, 0x00, 0x81};

	if (raw != NULL)
		memcpy(raw, denied ? true_value : false_value, 4U);
}

bool
yt_action_finalizer_turn_raw(const uint8_t before[4], uint8_t after[4])
{
	volatile float updated;

	if (before == NULL || after == NULL)
		return false;
	updated = qb_mbf32_decode(before) - 1.0f;
	return qb_mbf32_encode(updated, after) != QB_MBF_OVERFLOW;
}

bool
yt_action_finalizer_cloak_raw(const uint8_t before[4],
    uint8_t arithmetic[4], uint8_t result[4], bool *clamped)
{
	static const uint8_t dirty_zero[4] = {0x00, 0x00, 0xa3, 0x00};
	volatile float updated;

	if (before == NULL || arithmetic == NULL || result == NULL
	    || clamped == NULL)
		return false;
	updated = qb_mbf32_decode(before) - 0.009999999776482582f;
	if (qb_mbf32_encode(updated, arithmetic) == QB_MBF_OVERFLOW)
		return false;
	if (updated < 0.0f) {
		memcpy(result, dirty_zero, sizeof(dirty_zero));
		*clamped = true;
	}
	else {
		memcpy(result, arithmetic, 4U);
		*clamped = false;
	}
	return true;
}

