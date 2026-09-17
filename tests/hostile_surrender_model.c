#include "hostile_surrender_model.h"

#include "qb.h"

#include <string.h>

static bool
surrender_append(uint8_t *output, size_t capacity, size_t *position,
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
surrender_double_add(double left, double right)
{
	volatile double result = left + right;

	return result;
}

static double
surrender_double_sub(double left, double right)
{
	volatile double result = left - right;

	return result;
}

bool
test_hostile_attack_surrender_run(
    struct test_hostile_surrender_state *state,
    const struct test_hostile_surrender_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t radio[] = "RADIO MESSAGE COMING IN!";
	static const uint8_t captain_prefix[] =
	    "This is the captain of the fighter group in sector";
	static const uint8_t wish[] = "WE WISH TO SURRENDER!!!";
	static const uint8_t prompt[] =
	    "Will you accept our surrender? [Y]/N -=>";
	static const uint8_t joined[] = " We join your forces!";
	static const uint8_t xannor_refusal[] =
	    "Whee fyte to the deeth hoo-man slyme!";
	static const uint8_t mercenary_prefix[] =
	    "We'll DIE before joining with a slyme like you ";
	static const uint8_t mercenary_suffix[] = "!";
	static const uint8_t news_middle_one[] = " fighters in sector";
	static const uint8_t news_middle_two[] = " surrendered to ";
	static const uint8_t count_suffix[] = " fighters surrendered!";
	uint8_t captain[160];
	uint8_t refusal[256];
	uint8_t news[320];
	uint8_t count[128];
	char sector_number[64];
	char surrendered_number[64];
	size_t position;
	size_t sector_length;
	size_t surrendered_length;
	double surrendered_fighters;
	enum yt_hostile_surrender_answer answer =
	    YT_HOSTILE_SURRENDER_ANSWER_NO;
	enum yt_hostile_surrender_route owner_route;
	bool accepted = false;

	if (state == NULL || ops == NULL || ops->read_player == NULL
	    || ops->present == NULL || ops->sound == NULL
	    || ops->prompt == NULL || ops->append_news == NULL
	    || ops->cache_forces == NULL || ops->mark_checked == NULL
	    || (state->cached_player_name_length != 0U
	    && state->cached_player_name == NULL)
	    || (state->real_first_name_length != 0U
	    && state->real_first_name == NULL))
		return false;
	state->fighter_owner = state->old_owner;
	state->deployed_remaining = state->deployed_fighters;
	state->accepted = false;
	if (!ops->read_player(context, state->current_player_record,
	    &state->current, error))
		return false;
	state->ship_fighters = (double)state->current.fighters;
	owner_route = yt_hostile_surrender_route(state->old_owner);
	if (!ops->present(context, radio, sizeof(radio) - 1U,
	    YT_HOSTILE_SURRENDER_RADIO_ROW, error)
	    || !ops->sound(context, YT_HOSTILE_SURRENDER_RADIO_SOUND, 4.0f,
	    error))
		return false;
	if (qb_str_single(sector_number, sizeof(sector_number),
	    state->current.sector) < 0)
		return false;
	sector_length = strlen(sector_number);
	position = 0U;
	if (!surrender_append(captain, sizeof(captain), &position,
	    captain_prefix, sizeof(captain_prefix) - 1U)
	    || !surrender_append(captain, sizeof(captain), &position,
	    (const uint8_t *)sector_number, sector_length)
	    || !ops->present(context, captain, position,
	    YT_HOSTILE_SURRENDER_CAPTAIN_ROW, error))
		return false;

	switch (owner_route) {
	case YT_HOSTILE_SURRENDER_PLAYER:
		if (!ops->present(context, wish, sizeof(wish) - 1U,
		    YT_HOSTILE_SURRENDER_WISH_ROW, error)
		    || !ops->present(context, NULL, 0U,
		    YT_HOSTILE_SURRENDER_PROMPT_BLANK, error)
		    || !ops->prompt(context, prompt, sizeof(prompt) - 1U,
		    &answer, error))
			return false;
		if (answer != YT_HOSTILE_SURRENDER_ANSWER_NO
		    && answer != YT_HOSTILE_SURRENDER_ANSWER_YES
		    && answer != YT_HOSTILE_SURRENDER_ANSWER_EMPTY)
			return false;
		accepted = answer == YT_HOSTILE_SURRENDER_ANSWER_YES
		    || answer == YT_HOSTILE_SURRENDER_ANSWER_EMPTY;
		break;
	case YT_HOSTILE_SURRENDER_XANNOR:
		if (!ops->present(context, xannor_refusal,
		    sizeof(xannor_refusal) - 1U,
		    YT_HOSTILE_SURRENDER_XANNOR_REFUSAL_ROW, error)
		    || !ops->sound(context, YT_HOSTILE_SURRENDER_XANNOR_SOUND,
		    5.0f, error))
			return false;
		break;
	case YT_HOSTILE_SURRENDER_MERCENARY:
		position = 0U;
		if (!surrender_append(refusal, sizeof(refusal), &position,
		    mercenary_prefix, sizeof(mercenary_prefix) - 1U)
		    || !surrender_append(refusal, sizeof(refusal), &position,
		    state->real_first_name, state->real_first_name_length)
		    || !surrender_append(refusal, sizeof(refusal), &position,
		    mercenary_suffix, sizeof(mercenary_suffix) - 1U)
		    || !ops->present(context, refusal, position,
		    YT_HOSTILE_SURRENDER_MERCENARY_REFUSAL_ROW, error)
		    || !ops->sound(context, YT_HOSTILE_SURRENDER_MERCENARY_SOUND,
		    5.0f, error))
			return false;
		break;
	case YT_HOSTILE_SURRENDER_QUIET:
		break;
	}
	ops->mark_checked(context);
	state->accepted = accepted;
	if (!accepted)
		return true;
	if (!ops->present(context, joined, sizeof(joined) - 1U,
	    YT_HOSTILE_SURRENDER_JOINED_ROW, error)
	    || !ops->sound(context, YT_HOSTILE_SURRENDER_JOINED_SOUND, 1.0f,
	    error))
		return false;
	surrendered_fighters = surrender_double_sub(
	    state->deployed_fighters, state->defender_loss);
	if (qb_str_double(surrendered_number, sizeof(surrendered_number),
	    surrendered_fighters) < 0)
		return false;
	surrendered_length = strlen(surrendered_number);
	position = 0U;
	if (!surrender_append(news, sizeof(news), &position,
	    (const uint8_t *)surrendered_number, surrendered_length)
	    || !surrender_append(news, sizeof(news), &position,
	    news_middle_one, sizeof(news_middle_one) - 1U)
	    || !surrender_append(news, sizeof(news), &position,
	    (const uint8_t *)sector_number, sector_length)
	    || !surrender_append(news, sizeof(news), &position,
	    news_middle_two, sizeof(news_middle_two) - 1U)
	    || !surrender_append(news, sizeof(news), &position,
	    state->cached_player_name, state->cached_player_name_length)
	    || !ops->append_news(context, news, position, error))
		return false;
	state->ship_fighters = surrender_double_add(surrender_double_sub(
	    surrender_double_sub((double)state->current.fighters,
	    state->attacker_loss), state->defender_loss),
	    state->deployed_fighters);
	state->current.fighters = (float)state->ship_fighters;
	state->deployed_remaining = 0.0;
	state->fighter_owner = 0;
	ops->cache_forces(context, state->ship_fighters,
	    state->deployed_remaining);
	position = 0U;
	if (!surrender_append(count, sizeof(count), &position,
	    (const uint8_t *)surrendered_number, surrendered_length)
	    || !surrender_append(count, sizeof(count), &position,
	    count_suffix, sizeof(count_suffix) - 1U)
	    || !ops->present(context, count, position,
	    YT_HOSTILE_SURRENDER_COUNT_ROW, error))
		return false;
	return true;
}
