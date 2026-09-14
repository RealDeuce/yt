#include "hostile_attack_model.h"

#include "qb.h"

#include <string.h>

static float
attack_single_add(float left, float right)
{
	volatile float result = left + right;

	return result;
}

static bool
attack_append(uint8_t *output, size_t capacity, size_t *position,
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

bool
test_hostile_attack_persistence_run(
    struct yt_hostile_attack_persistence_state *state,
    const struct test_hostile_attack_persistence_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t destroyed[] = " destroyed";
	static const uint8_t belonging[] = " fighters belonging to ";
	uint8_t news[320];
	char loss_number[64];
	size_t position = 0U;
	int loss_length;

	if (state == NULL || ops == NULL || ops->read_player == NULL
	    || ops->write_player == NULL || ops->read_sector == NULL
	    || ops->write_sector == NULL || ops->present_blank == NULL
	    || ops->append_news == NULL || ops->fatal == NULL
	    || (state->cached_player_name_length != 0U
	    && state->cached_player_name == NULL)
	    || (state->owner_label_length != 0U
	    && state->owner_label == NULL))
		return false;
	state->route = YT_HOSTILE_ATTACK_PERSISTENCE_NORMAL;
	state->player_written = false;
	state->sector_written = false;
	state->post_loss_read = false;
	state->news_written = false;
	state->mercenaries_hurt = false;
	state->complete = false;
	if (!ops->read_player(context, state->current_player_record,
	    &state->current, error))
		return false;
	yt_deployed_attack_player_overlay(&state->current, state->shields,
	    (float)state->ship_fighters);
	if (!ops->write_player(context, state->current_player_record,
	    &state->current, error))
		return false;
	state->player_written = true;
	if (!ops->read_sector(context, state->current_sector, &state->sector,
	    error))
		return false;
	yt_deployed_attack_sector_overlay(&state->sector,
	    (float)state->deployed_fighters);
	if (!ops->write_sector(context, state->current_sector, &state->sector,
	    error))
		return false;
	state->sector_written = true;
	if (state->ship_fighters < 1.0 && state->shields < 1.0f) {
		state->route = YT_HOSTILE_ATTACK_PERSISTENCE_FATAL;
		if (!ops->fatal(context, error))
			return false;
		state->complete = true;
		return true;
	}
	if (!ops->present_blank(context, error))
		return false;
	if (state->defender_loss > 0.0) {
		if (!ops->read_player(context, state->current_player_record,
		    &state->current, error))
			return false;
		state->post_loss_read = true;
		state->ship_fighters = (double)state->current.fighters;
		loss_length = qb_str_double(loss_number, sizeof(loss_number),
		    state->defender_loss);
		if (loss_length < 0
		    || !attack_append(news, sizeof(news), &position,
		    state->cached_player_name, state->cached_player_name_length)
		    || !attack_append(news, sizeof(news), &position,
		    destroyed, sizeof(destroyed) - 1U)
		    || !attack_append(news, sizeof(news), &position,
		    (const uint8_t *)loss_number, (size_t)loss_length)
		    || !attack_append(news, sizeof(news), &position,
		    belonging, sizeof(belonging) - 1U)
		    || !attack_append(news, sizeof(news), &position,
		    state->owner_label, state->owner_label_length)
		    || !ops->append_news(context, news, position, error))
			return false;
		state->news_written = true;
		state->mercenaries_hurt = state->old_owner == -2.0f;
	}
	state->complete = true;
	return true;
}

bool
test_hostile_attack_tail_run(struct yt_hostile_attack_tail_state *state,
    const struct test_hostile_attack_tail_ops *ops, void *context,
    struct yt_error *error)
{
	uint8_t display[240];
	uint8_t news[300];
	uint8_t defeated[160];
	size_t display_length;
	size_t news_length;
	size_t defeated_length;

	if (state == NULL || ops == NULL || ops->read_player == NULL
	    || ops->write_player == NULL || ops->present == NULL
	    || ops->append_news == NULL || ops->clearance == NULL
	    || ops->random == NULL || ops->victory == NULL
	    || (state->cached_player_name_length != 0U
	    && state->cached_player_name == NULL))
		return false;
	state->bonus = 0.0f;
	state->player_read = false;
	state->player_written = false;
	state->reward_presented = false;
	state->reward_news_written = false;
	state->clearance_called = false;
	state->draw_consumed = false;
	state->defeated_presented = false;
	state->victory_called = false;
	state->complete = false;
	if (state->old_owner == -1.0f && state->defender_loss > 0.0) {
		if (!ops->read_player(context, state->current_player_record,
		    &state->current, error))
			return false;
		state->player_read = true;
		state->ship_fighters = (double)state->current.fighters;
		state->bonus = yt_xannor_attack_bonus(state->defender_loss,
		    state->current.turns, state->turns_per_day);
		if (state->bonus >= 1.0f) {
			state->current.turns = attack_single_add(
			    state->current.turns, state->bonus);
			(void)yt_record_set_number(&state->current.record, YT_F49,
			    state->current.turns);
			if (!ops->write_player(context,
			    state->current_player_record, &state->current, error))
				return false;
			state->player_written = true;
			if (!yt_xannor_attack_reward_rows(
			    state->cached_player_name,
			    state->cached_player_name_length, state->bonus,
			    state->defender_loss, display, sizeof(display),
			    &display_length, news, sizeof(news), &news_length)
			    || !ops->present(context, display, display_length,
			    YT_HOSTILE_ATTACK_TAIL_REWARD_ROW, error))
				return false;
			state->reward_presented = true;
			if (!ops->append_news(context, news, news_length, error))
				return false;
			state->reward_news_written = true;
			if (state->deployed_fighters < 1.0) {
				if (!ops->clearance(context, error))
					return false;
				state->clearance_called = true;
			}
		}
	}
	if (!ops->random(context, &state->dominated_draw, error))
		return false;
	state->draw_consumed = true;
	if (state->deployed_fighters <= 0.0) {
		if (!yt_hostile_defeated_row(state->ship_fighters, defeated,
		    sizeof(defeated), &defeated_length)
		    || !ops->present(context, defeated, defeated_length,
		    YT_HOSTILE_ATTACK_TAIL_DEFEATED_ROW, error))
			return false;
		state->defeated_presented = true;
		if (state->old_owner == -1.0f
		    && state->current.sector == state->headquarters) {
			if (!ops->victory(context, error))
				return false;
			state->victory_called = true;
		}
	}
	state->complete = true;
	return true;
}
