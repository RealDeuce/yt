#include "hostile_bribe_model.h"

#include "qb.h"

#include <stdio.h>
#include <string.h>

static bool
bribe_append(uint8_t *output, size_t capacity, size_t *position,
    const uint8_t *text, size_t length)
{
	if ((text == NULL && length != 0U) || *position > capacity
	    || length > capacity - *position)
		return false;
	if (length != 0U)
		memcpy(output + *position, text, length);
	*position += length;
	return true;
}

static double
bribe_double_add(double left, double right)
{
	volatile double result = left + right;

	return result;
}

static double
bribe_double_sub(double left, double right)
{
	volatile double result = left - right;

	return result;
}

bool
test_hostile_bribe_accept_run(
    struct test_hostile_bribe_accept_state *state,
    const struct test_hostile_bribe_accept_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t deal[] = "Good Deal! We join up with you!";
	volatile double fighters;
	volatile double credits;

	if (state == NULL || ops == NULL || ops->present == NULL
	    || ops->sound == NULL || ops->read_sector == NULL
	    || ops->write_sector == NULL || ops->read_player == NULL
	    || ops->write_player == NULL)
		return false;
	state->persisted_fighters = 0.0f;
	state->persisted_credits = 0.0f;
	state->deal_presented = false;
	state->sound_played = false;
	state->sector_read = false;
	state->sector_written = false;
	state->player_read = false;
	state->player_written = false;
	state->complete = false;
	memset(&state->sector, 0, sizeof(state->sector));
	memset(&state->current, 0, sizeof(state->current));
	if (!ops->present(context, deal, sizeof(deal) - 1U, error))
		return false;
	state->deal_presented = true;
	if (!ops->sound(context, 1.0f, error))
		return false;
	state->sound_played = true;
	if (!ops->read_sector(context, state->current_sector, &state->sector,
	    error))
		return false;
	state->sector_read = true;
	yt_bribe_sector_overlay(&state->sector);
	if (!ops->write_sector(context, state->current_sector, &state->sector,
	    error))
		return false;
	state->sector_written = true;
	if (!ops->read_player(context, state->current_player_record,
	    &state->current, error))
		return false;
	state->player_read = true;
	fighters = bribe_double_add((double)state->current.fighters,
	    (double)state->cached_defenders);
	credits = bribe_double_sub((double)state->current.credits,
	    (double)state->offer);
	state->persisted_fighters = (float)fighters;
	state->persisted_credits = (float)credits;
	yt_bribe_player_overlay(&state->current, state->persisted_fighters,
	    state->persisted_credits);
	if (!ops->write_player(context, state->current_player_record,
	    &state->current, error))
		return false;
	state->player_written = true;
	state->complete = true;
	return true;
}

static bool
bribe_name_row(const uint8_t *prefix, size_t prefix_length,
    const uint8_t *name, size_t name_length, const uint8_t *suffix,
    size_t suffix_length, uint8_t *row, size_t capacity, size_t *length)
{
	size_t used = 0U;

	if (length == NULL)
		return false;
	*length = 0U;
	if (!bribe_append(row, capacity, &used, prefix, prefix_length)
	    || !bribe_append(row, capacity, &used, name, name_length)
	    || !bribe_append(row, capacity, &used, suffix, suffix_length))
		return false;
	*length = used;
	return true;
}

static bool
bribe_force_run(struct test_hostile_bribe_state *state,
    const struct test_hostile_bribe_ops *ops, void *context,
    bool mercenary_fatal_gate, struct yt_error *error)
{
	enum qb_mbf_status conversion;
	float commitment = (float)state->ship_fighters;
	uint8_t raw[4];

	state->forced_attack = true;
	conversion = qb_mbf32_encode(commitment, raw);
	if (conversion == QB_MBF_OVERFLOW) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "bribe:commitment-csng");
		}
		return false;
	}
	state->commitment = qb_mbf32_decode(raw);
	switch (yt_bribe_forced_admit(state->ship_fighters, state->shields,
	    mercenary_fatal_gate, state->commitment)) {
	case YT_BRIBE_FORCED_FATAL:
		state->route = YT_HOSTILE_BRIBE_FATAL;
		state->fatal_called = true;
		if (!ops->fatal(context, error))
			return false;
		break;
	case YT_BRIBE_FORCED_LESS_THAN_ONE:
		state->direct_hostile_menu = true;
		state->route = YT_HOSTILE_BRIBE_HOSTILE_MENU;
		break;
	case YT_BRIBE_FORCED_ATTACK:
		state->route = YT_HOSTILE_BRIBE_COMBAT;
		state->combat_called = true;
		if (!ops->combat(context, (double)state->commitment, error))
			return false;
		break;
	default:
		return false;
	}
	state->complete = true;
	return true;
}

bool
test_hostile_bribe_run(struct test_hostile_bribe_state *state,
    const struct test_hostile_bribe_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t ordinary_prefix[] =
	    "We don't accept no Bribes ";
	static const uint8_t planet_prefix[] = "Scram ";
	static const uint8_t planet_suffix[] = ", This planet is OURS!";
	static const uint8_t life_prefix[] =
	    "We just want your miserable life ";
	static const uint8_t introduction_prefix[] =
	    "We MAY join up if you pay us enough ";
	static const uint8_t rejected_prefix[] = "You insult us ";
	static const uint8_t rejected_suffix[] = "! Prepare to DIE!";
	static const uint8_t bang[] = "!";
	static const uint8_t prompt_prefix[] = "You have";
	static const uint8_t prompt_suffix[] =
	    " credits. How much do you offer? -+>";
	uint8_t row[512];
	uint8_t prompt[512];
	char credits[64];
	char response[4096] = {0};
	struct qb_val_result parsed;
	enum qb_mbf_status conversion;
	uint8_t raw[4];
	size_t row_length;
	size_t prompt_length = 0U;
	int credits_length;
	bool force_attack;

	if (state == NULL || ops == NULL || ops->present == NULL
	    || ops->random == NULL || ops->amount == NULL
	    || ops->accept == NULL || ops->combat == NULL
	    || ops->fatal == NULL
	    || (state->real_first_name_length != 0U
	    && state->real_first_name == NULL))
		return false;
	memset(state->draws, 0, sizeof(state->draws));
	state->draws_consumed = 0U;
	state->offer = 0.0f;
	state->above_credits = false;
	state->threshold = 0.0;
	state->commitment = 0.0f;
	state->forced_attack = false;
	state->direct_hostile_menu = false;
	state->accepted_called = false;
	state->combat_called = false;
	state->fatal_called = false;
	memset(&state->accepted, 0, sizeof(state->accepted));
	state->branch = YT_HOSTILE_BRIBE_BRANCH_INCOMPLETE;
	state->route = YT_HOSTILE_BRIBE_INCOMPLETE;
	state->complete = false;

	if (state->owner != -2.0f) {
		state->branch = YT_HOSTILE_BRIBE_ORDINARY_REFUSAL;
		if (!bribe_name_row(ordinary_prefix,
		    sizeof(ordinary_prefix) - 1U, state->real_first_name,
		    state->real_first_name_length, bang, sizeof(bang) - 1U,
		    row, sizeof(row), &row_length)
		    || !ops->present(context, row, row_length,
		    YT_HOSTILE_BRIBE_ORDINARY_REFUSAL_ROW, error))
			return false;
		if (!ops->random(context, &state->draws[0], error))
			return false;
		state->draws_consumed = 1U;
		force_attack = yt_bribe_ordinary_forces(state->owner,
		    state->cached_defenders, state->ship_fighters,
		    state->draws[0]);
		if (!force_attack) {
			state->route = YT_HOSTILE_BRIBE_SCANNER;
			state->complete = true;
			return true;
		}
		return bribe_force_run(state, ops, context, false, error);
	}

	if (state->planet_link != 0.0f) {
		state->branch = YT_HOSTILE_BRIBE_PLANET_REFUSAL;
		if (!bribe_name_row(planet_prefix,
		    sizeof(planet_prefix) - 1U, state->real_first_name,
		    state->real_first_name_length, planet_suffix,
		    sizeof(planet_suffix) - 1U, row, sizeof(row), &row_length)
		    || !ops->present(context, row, row_length,
		    YT_HOSTILE_BRIBE_PLANET_REFUSAL_ROW, error))
			return false;
		state->route = YT_HOSTILE_BRIBE_SCANNER;
		state->complete = true;
		return true;
	}

	if (!ops->random(context, &state->draws[0], error))
		return false;
	state->draws_consumed = 1U;
	if (!ops->random(context, &state->draws[1], error))
		return false;
	state->draws_consumed = 2U;
	force_attack = yt_bribe_mercenary_forces(state->cached_defenders,
	    state->ship_fighters, state->draws[0], state->draws[1],
	    state->mercenaries_hurt);
	if (force_attack) {
		state->branch = YT_HOSTILE_BRIBE_LIFE_DEMAND;
		if (!bribe_name_row(life_prefix, sizeof(life_prefix) - 1U,
		    state->real_first_name, state->real_first_name_length, bang,
		    sizeof(bang) - 1U, row, sizeof(row), &row_length)
		    || !ops->present(context, row, row_length,
		    YT_HOSTILE_BRIBE_LIFE_DEMAND_ROW, error))
			return false;
		return bribe_force_run(state, ops, context, true, error);
	}

	if (!bribe_name_row(introduction_prefix,
	    sizeof(introduction_prefix) - 1U, state->real_first_name,
	    state->real_first_name_length, bang, sizeof(bang) - 1U,
	    row, sizeof(row), &row_length)
	    || !ops->present(context, row, row_length,
	    YT_HOSTILE_BRIBE_INTRODUCTION_ROW, error))
		return false;
	credits_length = qb_str_double(credits, sizeof(credits), state->credits);
	if (credits_length < 0
	    || !bribe_append(prompt, sizeof(prompt), &prompt_length,
	    prompt_prefix, sizeof(prompt_prefix) - 1U)
	    || !bribe_append(prompt, sizeof(prompt), &prompt_length,
	    (const uint8_t *)credits, (size_t)credits_length)
	    || !bribe_append(prompt, sizeof(prompt), &prompt_length,
	    prompt_suffix, sizeof(prompt_suffix) - 1U)
	    || !ops->present(context, prompt, prompt_length,
	    YT_HOSTILE_BRIBE_OFFER_PROMPT, error)
	    || !ops->amount(context, response, sizeof(response), error))
		return false;
	if (response[0] == '\0') {
		state->branch = YT_HOSTILE_BRIBE_EMPTY_OFFER;
		state->route = YT_HOSTILE_BRIBE_SCANNER;
		state->complete = true;
		return true;
	}
	parsed = qb_val(response);
	if (!parsed.valid || parsed.overflow) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "bribe:VAL");
		}
		return false;
	}
	state->offer = (float)parsed.value;
	conversion = qb_mbf32_encode(state->offer, raw);
	if (conversion == QB_MBF_OVERFLOW) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "bribe:offer-csng");
		}
		state->offer = 0.0f;
		return false;
	}
	state->offer = qb_mbf32_decode(raw);
	state->above_credits = (double)state->offer > state->credits;
	if (!ops->random(context, &state->draws[2], error))
		return false;
	state->draws_consumed = 3U;
	state->threshold = yt_bribe_offer_threshold(state->cached_defenders,
	    state->draws[2]);
	if (!state->above_credits && yt_bribe_offer_accepted(state->offer,
	    state->credits, state->threshold)) {
		state->branch = YT_HOSTILE_BRIBE_ACCEPTED;
		state->accepted = (struct test_hostile_bribe_accept_state){
			.current_player_record = state->current_player_record,
			.current_sector = state->current_sector,
			.cached_defenders = state->cached_defenders,
			.offer = state->offer,
		};
		state->accepted_called = true;
		if (!ops->accept(context, &state->accepted, error))
			return false;
		state->route = YT_HOSTILE_BRIBE_SCANNER;
		state->complete = true;
		return true;
	}

	state->branch = YT_HOSTILE_BRIBE_REJECTED;
	if (!bribe_name_row(rejected_prefix,
	    sizeof(rejected_prefix) - 1U, state->real_first_name,
	    state->real_first_name_length, rejected_suffix,
	    sizeof(rejected_suffix) - 1U, row, sizeof(row), &row_length)
	    || !ops->present(context, row, row_length,
	    YT_HOSTILE_BRIBE_REJECTED_ROW, error))
		return false;
	return bribe_force_run(state, ops, context, true, error);
}
