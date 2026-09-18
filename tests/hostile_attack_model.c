#include "hostile_attack_model.h"

#include "qb.h"

#include <stdio.h>
#include <string.h>

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
    struct test_hostile_attack_persistence_state *state,
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
	state->sector_written = false;
	state->mercenaries_hurt = false;
	if (!ops->read_player(context, state->current_player_record,
	    &state->current, error))
		return false;
	yt_deployed_attack_player_overlay(&state->current, state->shields,
	    (float)state->ship_fighters);
	if (!ops->write_player(context, state->current_player_record,
	    &state->current, error))
		return false;
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
		return true;
	}
	if (!ops->present_blank(context, error))
		return false;
	if (state->defender_loss > 0.0) {
		if (!ops->read_player(context, state->current_player_record,
		    &state->current, error))
			return false;
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
		state->mercenaries_hurt = state->old_owner == -2;
	}
	return true;
}
bool
test_hostile_attack_combat_run(
    struct test_hostile_attack_combat_state *state,
    const struct test_hostile_attack_combat_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t lost_prefix[] = " You lost";
	static const uint8_t lost_suffix[] = " fighter(s)";
	static const uint8_t destroyed_prefix[] = " You destroyed";
	static const uint8_t destroyed_suffix[] = " enemy fighters.";
	static const uint8_t exposed[] =
	    "Fighters gone! Enemy attacking your ship!";
	uint8_t lost_row[128];
	uint8_t destroyed_row[128];
	char attacker_number[64];
	char defender_number[64];
	size_t lost_length = 0U;
	size_t destroyed_length = 0U;
	int attacker_length;
	int defender_length;
	bool child_result;

	if (state == NULL || ops == NULL || ops->read_sector == NULL
	    || ops->read_player == NULL || ops->sound == NULL
	    || ops->random == NULL || ops->store_ship == NULL
	    || ops->surrender == NULL
	    || ops->present == NULL || ops->cache_player == NULL
	    || ops->cache_sector == NULL || ops->spill == NULL
	    || ops->persistence == NULL || ops->tail == NULL
	    || (state->cached_player_name_length != 0U
	    && state->cached_player_name == NULL)
	    || (state->real_first_name_length != 0U
	    && state->real_first_name == NULL)
	    || (state->owner_label_length != 0U
	    && state->owner_label == NULL))
		return false;
	state->route = YT_HOSTILE_ATTACK_COMBAT_NORMAL;
	state->complete = false;
	state->old_count = state->cached_defenders;
	state->old_ship = 0.0;
	state->attacker_loss = 0.0;
	state->defender_loss = 0.0;
	state->ship_fighters = 0.0;
	state->deployed_remaining = state->old_count;
	state->quantum = 0.0f;
	state->last_draw = 0.0f;
	state->iterations = 0U;
	state->surrender_checked = false;
	state->surrendered = false;
	state->spill_called = false;
	memset(&state->surrender, 0, sizeof(state->surrender));
	memset(&state->persistence, 0, sizeof(state->persistence));
	memset(&state->tail, 0, sizeof(state->tail));
	if (!ops->read_sector(context, state->current_sector,
	    &state->opened_sector, error))
		return false;
	state->old_owner = (int)qb_mbf32_decode(
	    &state->opened_sector.record.bytes[YT_F85]);
	if (!ops->read_player(context, state->current_player_record,
	    &state->current, error))
		return false;
	state->old_ship = (double)state->current.fighters;
	if (!ops->sound(context, 2.0f, error))
		return false;
	do {
		double remaining_attacker = (
		    state->commitment - state->attacker_loss);
		double remaining_defender = (
		    state->old_count - state->defender_loss);
		state->quantum = yt_hostile_attack_quantum(remaining_attacker,
		    remaining_defender);
		if (remaining_defender == 0.0) {
			if (error != NULL) {
				error->status = YT_RANGE;
				(void)snprintf(error->operation,
				    sizeof(error->operation), "%s",
				    "attack:surrender-ratio-divide");
			}
			return false;
		}
		if (!state->surrender_checked && state->allow_surrender
		    && remaining_attacker / remaining_defender > 10.0) {
			state->surrender = (struct test_hostile_surrender_state){
				.current_player_record = state->current_player_record,
				.old_owner = state->old_owner,
				.attacker_loss = state->attacker_loss,
				.defender_loss = state->defender_loss,
				.deployed_fighters = state->old_count,
				.cached_player_name = state->cached_player_name,
				.cached_player_name_length =
				    state->cached_player_name_length,
				.real_first_name = state->real_first_name,
				.real_first_name_length = state->real_first_name_length,
				.current = state->current,
			};
			child_result = ops->surrender(context, &state->surrender,
			    error);
			state->current = state->surrender.current;
			state->old_ship = state->surrender.ship_fighters;
			state->surrendered = state->surrender.accepted;
			ops->cache_player(context, &state->current);
			if (!child_result)
				return false;
			state->surrender_checked = true;
			if (state->surrendered) {
				state->ship_fighters =
				    state->surrender.ship_fighters;
				state->deployed_remaining =
				    state->surrender.deployed_remaining;
				state->sector.fighter_owner =
				    state->surrender.fighter_owner;
				state->current.fighters =
				    (float)state->ship_fighters;
				ops->cache_player(context, &state->current);
				ops->cache_sector(context, &state->sector,
				    state->deployed_remaining);
				break;
			}
		}
		if (!ops->random(context, &state->last_draw, error))
			return false;
		if (yt_hostile_attack_loses_attacker(state->current.cloak,
		    state->last_draw)) {
			state->attacker_loss = (
			    state->attacker_loss + (double)state->quantum);
		} else {
			state->defender_loss = (
			    state->defender_loss + (double)state->quantum);
		}
		++state->iterations;
	} while (state->attacker_loss < state->commitment
	    && state->defender_loss < state->old_count);
	if (state->attacker_loss > state->commitment) {
		state->attacker_loss = state->commitment;
	}
	if (state->defender_loss > state->old_count) {
		state->defender_loss = state->old_count;
	}
	if (!state->surrendered) {
		state->ship_fighters = (state->old_ship -
		    state->attacker_loss);
		state->deployed_remaining = (
		    state->old_count - state->defender_loss);
		state->current.fighters = (float)state->ship_fighters;
		ops->store_ship(context, state->ship_fighters);
		ops->cache_player(context, &state->current);
	}
	attacker_length = qb_str_double(attacker_number,
	    sizeof(attacker_number), state->attacker_loss);
	defender_length = qb_str_double(defender_number,
	    sizeof(defender_number), state->defender_loss);
	if (attacker_length < 0 || defender_length < 0
	    || !attack_append(lost_row, sizeof(lost_row), &lost_length,
	    lost_prefix, sizeof(lost_prefix) - 1U)
	    || !attack_append(lost_row, sizeof(lost_row), &lost_length,
	    (const uint8_t *)attacker_number, (size_t)attacker_length)
	    || !attack_append(lost_row, sizeof(lost_row), &lost_length,
	    lost_suffix, sizeof(lost_suffix) - 1U)
	    || !attack_append(destroyed_row, sizeof(destroyed_row),
	    &destroyed_length, destroyed_prefix,
	    sizeof(destroyed_prefix) - 1U)
	    || !attack_append(destroyed_row, sizeof(destroyed_row),
	    &destroyed_length, (const uint8_t *)defender_number,
	    (size_t)defender_length)
	    || !attack_append(destroyed_row, sizeof(destroyed_row),
	    &destroyed_length, destroyed_suffix,
	    sizeof(destroyed_suffix) - 1U)
	    || !ops->present(context, NULL, 0U,
	    YT_HOSTILE_ATTACK_COMBAT_RESULT_BLANK, error)
	    || !ops->present(context, lost_row, lost_length,
	    YT_HOSTILE_ATTACK_COMBAT_LOSS_ROW, error)
	    || !ops->present(context, destroyed_row, destroyed_length,
	    YT_HOSTILE_ATTACK_COMBAT_DESTROYED_ROW, error))
		return false;
	if (state->ship_fighters < 1.0 && state->deployed_remaining > 0.0) {
		if (!ops->present(context, exposed, sizeof(exposed) - 1U,
		    YT_HOSTILE_ATTACK_COMBAT_EXPOSED_ROW, error)
		    || !ops->present(context, NULL, 0U,
		    YT_HOSTILE_ATTACK_COMBAT_SPILL_BLANK, error))
			return false;
		if (!ops->spill(context, &state->deployed_remaining,
		    &state->current.shields, error)) {
			ops->cache_player(context, &state->current);
			return false;
		}
		state->spill_called = true;
		ops->cache_player(context, &state->current);
	}
	state->sector.fighters = (float)state->deployed_remaining;
	ops->cache_sector(context, &state->sector, state->deployed_remaining);
	state->persistence =
	    (struct test_hostile_attack_persistence_state){
		.current_player_record = state->current_player_record,
		.current_sector = state->current_sector,
		.ship_fighters = state->ship_fighters,
		.shields = state->current.shields,
		.deployed_fighters = state->deployed_remaining,
		.defender_loss = state->defender_loss,
		.old_owner = state->old_owner,
		.cached_player_name = state->cached_player_name,
		.cached_player_name_length = state->cached_player_name_length,
		.owner_label = state->owner_label,
		.owner_label_length = state->owner_label_length,
		.current = state->current,
		.sector = state->sector,
	};
	child_result = ops->persistence(context, &state->persistence, error);
	state->current = state->persistence.current;
	state->ship_fighters = state->persistence.ship_fighters;
	if (state->persistence.sector_written) {
		state->sector = state->persistence.sector;
		ops->cache_sector(context, &state->sector,
		    state->deployed_remaining);
	}
	if (state->persistence.route == YT_HOSTILE_ATTACK_PERSISTENCE_FATAL)
		state->route = YT_HOSTILE_ATTACK_COMBAT_FATAL;
	if (!child_result)
		return false;
	if (state->route == YT_HOSTILE_ATTACK_COMBAT_FATAL) {
		state->complete = true;
		return true;
	}
	state->tail = (struct test_hostile_attack_tail_state){
		.current_player_record = state->current_player_record,
		.old_owner = state->old_owner,
		.defender_loss = state->defender_loss,
		.deployed_fighters = state->deployed_remaining,
		.ship_fighters = state->ship_fighters,
		.turns_per_day = state->turns_per_day,
		.headquarters = state->headquarters,
		.cached_player_name = state->cached_player_name,
		.cached_player_name_length = state->cached_player_name_length,
		.current = state->current,
	};
	child_result = ops->tail(context, &state->tail, error);
	state->current = state->tail.current;
	state->ship_fighters = state->tail.ship_fighters;
	if (!child_result)
		return false;
	state->complete = true;
	return true;
}



bool
test_hostile_attack_tail_run(struct test_hostile_attack_tail_state *state,
    const struct test_hostile_attack_tail_ops *ops, void *context,
    struct yt_error *error)
{
	uint8_t display[240];
	uint8_t news[300];
	uint8_t defeated[160];
	size_t display_length;
	size_t news_length;
	size_t defeated_length;
	float bonus;
	float dominated_draw;

	if (state == NULL || ops == NULL || ops->read_player == NULL
	    || ops->write_player == NULL || ops->present == NULL
	    || ops->append_news == NULL || ops->clearance == NULL
	    || ops->random == NULL || ops->victory == NULL
	    || (state->cached_player_name_length != 0U
	    && state->cached_player_name == NULL))
		return false;
	if (state->old_owner == -1 && state->defender_loss > 0.0) {
		if (!ops->read_player(context, state->current_player_record,
		    &state->current, error))
			return false;
		state->ship_fighters = (double)state->current.fighters;
		bonus = yt_xannor_attack_bonus(state->defender_loss,
		    state->current.turns, state->turns_per_day);
		if (bonus >= 1.0f) {
			state->current.turns = (
			    state->current.turns + bonus);
			(void)yt_record_set_number(&state->current.record, YT_F49,
			    state->current.turns);
			if (!ops->write_player(context,
			    state->current_player_record, &state->current, error))
				return false;
			if (!yt_xannor_attack_reward_rows(
			    state->cached_player_name,
			    state->cached_player_name_length, bonus,
			    state->defender_loss, display, sizeof(display),
			    &display_length, news, sizeof(news), &news_length)
			    || !ops->present(context, display, display_length,
			    YT_HOSTILE_ATTACK_TAIL_REWARD_ROW, error))
				return false;
			if (!ops->append_news(context, news, news_length, error))
				return false;
			if (state->deployed_fighters < 1.0) {
				if (!ops->clearance(context, error))
					return false;
			}
		}
	}
	if (!ops->random(context, &dominated_draw, error))
		return false;
	if (state->deployed_fighters <= 0.0) {
		if (!yt_hostile_defeated_row(state->ship_fighters, defeated,
		    sizeof(defeated), &defeated_length)
		    || !ops->present(context, defeated, defeated_length,
		    YT_HOSTILE_ATTACK_TAIL_DEFEATED_ROW, error))
			return false;
		if (state->old_owner == -1
		    && (float)state->current.sector == state->headquarters) {
			if (!ops->victory(context, error))
				return false;
		}
	}
	return true;
}
