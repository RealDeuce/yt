#include "direct_attack_model.h"

#include "qb.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static float
test_direct_attack_single_add(float left, float right)
{
	volatile float result = left + right;

	return result;
}

static float
test_direct_attack_single_div(float left, float right)
{
	volatile float result = left / right;

	return result;
}

static double
test_direct_attack_double_add(double left, double right)
{
	volatile double result = left + right;

	return result;
}

static double
test_direct_attack_double_sub(double left, double right)
{
	volatile double result = left - right;

	return result;
}

bool
test_direct_attack_attrition_run(
    struct test_direct_attack_attrition_state *state,
    bool (*draw)(void *context, float *value, struct yt_error *error),
    void *context, struct yt_error *error)
{
	if (state == NULL || draw == NULL)
		return false;
	state->attacker_loss = 0.0;
	state->defender_loss = 0.0;
	state->quantum = 0.0f;
	state->iterations = 0U;
	state->complete = false;
	while (state->attacker_loss < state->committed
	    && state->defender_loss < state->defenders) {
		double remaining_attacker = state->committed
		    - state->attacker_loss;
		double remaining_defender = state->defenders
		    - state->defender_loss;
		double minimum = remaining_attacker < remaining_defender
		    ? remaining_attacker : remaining_defender;
		volatile double integral = floor(minimum / 20.0);
		float sampled;

		state->quantum = (float)integral;
		if (state->quantum < 1.0f)
			state->quantum = 1.0f;
		if (!draw(context, &sampled, error))
			return false;
		if (test_direct_attack_single_add(test_direct_attack_single_div(
		    state->cloak, 10.0f), sampled) < 0.44999998807907104f)
			state->attacker_loss = test_direct_attack_double_add(
			    state->attacker_loss, (double)state->quantum);
		else
			state->defender_loss = test_direct_attack_double_add(
			    state->defender_loss, (double)state->quantum);
		++state->iterations;
	}
	state->complete = true;
	return true;
}

bool
test_direct_attack_combat_run(
    struct test_direct_attack_combat_state *state,
    const struct test_direct_attack_combat_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t eliminated[] =
	    "Fighters eliminated! Attacking the ship!";
	uint8_t row[300];
	uint8_t second[300];
	uint8_t stored_name[YT_TEXT_FIELD_SIZE];
	uint8_t radio[160];
	size_t row_length;
	size_t second_length;
	size_t name_length;
	size_t radio_length;
	float remaining_shields;

	if (state == NULL || ops == NULL || ops->read_player == NULL
	    || ops->write_player == NULL || ops->present == NULL
	    || ops->sound == NULL || ops->radio == NULL
	    || ops->random == NULL || ops->spill == NULL
	    || ops->kill == NULL)
		return false;
	state->route = YT_DIRECT_ATTACK_COMBAT_INCOMPLETE;
	state->reserve_written = false;
	state->current_casualty_written = false;
	state->target_casualty_written = false;
	state->target_shield_written = false;
	state->current_final_written = false;
	state->complete = false;
	state->defenders = 0.0;
	state->cached_reserve = 0.0;
	state->attacking = 0.0;
	state->target_shields = 0.0f;
	state->current_sector = 0.0f;
	memset(&state->attrition, 0, sizeof(state->attrition));

	if (!ops->read_player(context, state->target_record, &state->target,
	    error))
		return false;
	state->defenders = (double)state->target.fighters;
	if (!ops->read_player(context, state->current_player_record,
	    &state->current, error))
		return false;
	if (state->committed > (double)state->current.fighters) {
		if (!yt_direct_attack_too_many_row(
		    (double)state->current.fighters, row, sizeof(row),
		    &row_length)
		    || !ops->present(context, row, row_length,
		    YT_DIRECT_ATTACK_COMBAT_TOO_MANY_ROW, error))
			return false;
		state->route = YT_DIRECT_ATTACK_COMBAT_TOO_MANY;
		state->complete = true;
		return true;
	}

	state->cached_reserve = test_direct_attack_double_sub(
	    (double)state->current.fighters, state->committed);
	yt_direct_attack_fighter_overlay(&state->current,
	    (float)state->cached_reserve);
	if (!ops->write_player(context, state->current_player_record,
	    &state->current, error))
		return false;
	state->reserve_written = true;
	if (!ops->sound(context, 2.0f, error))
		return false;

	state->attrition = (struct test_direct_attack_attrition_state){
		.committed = state->committed,
		.defenders = state->defenders,
		.cloak = state->current.cloak,
	};
	if (!test_direct_attack_attrition_run(&state->attrition, ops->random,
	    context, error))
		return false;
	if (state->attrition.defender_loss > 0.0) {
		name_length = yt_player_stored_name(&state->current, stored_name);
		if (!yt_direct_attack_radio_text(stored_name, name_length,
		    state->attrition.defender_loss, radio, sizeof(radio),
		    &radio_length)
		    || !ops->radio(context, radio, radio_length,
		    (float)state->target_record, error))
			return false;
	}

	if (!ops->read_player(context, state->current_player_record,
	    &state->current, error))
		return false;
	state->cached_reserve = (double)state->current.fighters;
	state->attacking = test_direct_attack_double_sub(state->committed,
	    state->attrition.attacker_loss);
	state->defenders = test_direct_attack_double_sub(state->defenders,
	    state->attrition.defender_loss);
	yt_direct_attack_fighter_overlay(&state->current,
	    (float)test_direct_attack_double_add(state->cached_reserve,
	    state->attacking));
	if (!ops->write_player(context, state->current_player_record,
	    &state->current, error))
		return false;
	state->current_casualty_written = true;
	if (!ops->read_player(context, state->target_record, &state->target,
	    error))
		return false;
	state->current_sector = (float)state->current.sector;
	state->target_shields = state->target.shields;
	yt_direct_attack_fighter_overlay(&state->target,
	    (float)state->defenders);
	if (!ops->write_player(context, state->target_record,
	    &state->target, error))
		return false;
	state->target_casualty_written = true;
	if (!yt_direct_attack_result_rows(state->attrition.attacker_loss,
	    state->cached_reserve, state->attrition.defender_loss,
	    state->defenders, row, sizeof(row), &row_length,
	    second, sizeof(second), &second_length)
	    || !ops->present(context, row, row_length,
	    YT_DIRECT_ATTACK_COMBAT_ATTACKER_ROW, error)
	    || !ops->present(context, second, second_length,
	    YT_DIRECT_ATTACK_COMBAT_DEFENDER_ROW, error))
		return false;
	if (state->defenders > 0.0 || state->attacking < 1.0) {
		state->route = YT_DIRECT_ATTACK_COMBAT_CASUALTY_RETURN;
		state->complete = true;
		return true;
	}
	if (!ops->present(context, eliminated, sizeof(eliminated) - 1U,
	    YT_DIRECT_ATTACK_COMBAT_ELIMINATED_ROW, error))
		return false;
	if (state->target_shields > 0.0f
	    && !ops->spill(context, &state->attacking,
	    &state->target_shields, error))
		return false;

	remaining_shields = state->target_shields;
	if (!ops->read_player(context, state->target_record, &state->target,
	    error))
		return false;
	yt_direct_attack_shield_overlay(&state->target, remaining_shields);
	if (!ops->write_player(context, state->target_record,
	    &state->target, error))
		return false;
	state->target_shield_written = true;
	if (!ops->read_player(context, state->current_player_record,
	    &state->current, error))
		return false;
	yt_direct_attack_fighter_overlay(&state->current,
	    (float)test_direct_attack_double_add(state->cached_reserve,
	    state->attacking));
	if (!ops->write_player(context, state->current_player_record,
	    &state->current, error))
		return false;
	state->current_final_written = true;
	if (state->target_shields > 0.0f) {
		state->route = YT_DIRECT_ATTACK_COMBAT_SHIELD_RETURN;
		state->complete = true;
		return true;
	}
	if (!ops->kill(context, state->target_record,
	    state->current_player_record, state->current_sector,
	    state->target_shields, error))
		return false;
	state->route = YT_DIRECT_ATTACK_COMBAT_KILL_RETURN;
	state->complete = true;
	return true;
}

bool
test_direct_attack_run(struct test_direct_attack_state *state,
    const struct test_direct_attack_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t title[] = "<Attack>";
	static const uint8_t no_fighters[] =
	    "You don't have any fighters.";
	static const uint8_t none_visible[] = "There's no one here!";
	static const uint8_t none_selected[] =
	    "There are no other ships in this sector.";
	uint8_t row[300];
	uint8_t target_name[YT_TEXT_FIELD_SIZE];
	char response[4096];
	size_t row_length;
	size_t target_name_length;

	if (state == NULL || ops == NULL || ops->read_player == NULL
	    || ops->present == NULL || ops->confirm == NULL
	    || ops->amount == NULL || ops->combat == NULL
	    || state->player_cache == NULL)
		return false;
	state->candidate = 2;
	state->target_record_cell = 0;
	state->committed = 0.0;
	state->encountered = false;
	state->enter_sector = false;
	state->route = YT_DIRECT_ATTACK_INCOMPLETE;
	state->complete = false;
	if (!ops->present(context, title, sizeof(title) - 1U,
	    YT_DIRECT_ATTACK_TITLE_ROW, error)
	    || !ops->read_player(context, state->current_player_record,
	    &state->current, error))
		return false;
	if (state->current.fighters < 1.0f) {
		if (!ops->present(context, no_fighters,
		    sizeof(no_fighters) - 1U,
		    YT_DIRECT_ATTACK_NO_FIGHTERS_ROW, error))
			return false;
		state->route = YT_DIRECT_ATTACK_NO_FIGHTERS;
		state->complete = true;
		return true;
	}

	while (state->candidate <= state->last_player_record) {
		enum test_direct_attack_confirmation answer;
		struct qb_val_result parsed;
		int cached_sector;
		float cached_cloak;
		bool sector_mismatch;
		bool self;
		bool cloaked;
		bool positive_team;
		bool same_team;
		int record = state->candidate;

		cached_sector = yt_player_cache_sector(state->player_cache, record);
		cached_cloak = yt_player_cache_cloak(state->player_cache, record);
		sector_mismatch = cached_sector != (int)state->current.sector;
		self = record == state->current_player_record;
		cloaked = cached_cloak > 0.0f;
		if (sector_mismatch || self || cloaked) {
			++state->candidate;
			continue;
		}

		state->target_record_cell = state->candidate;
		if (ops->store_target_record != NULL) {
			uint8_t target_record_raw[4];

			(void)qb_mbf32_encode((float)state->candidate,
			    target_record_raw);
			ops->store_target_record(context, target_record_raw);
		}
		if (!ops->read_player(context, record,
		    &state->candidate_player, error))
			return false;
		target_name_length = yt_player_stored_name(
		    &state->candidate_player, target_name);
		positive_team = state->candidate_player.team > 0.0f;
		same_team = state->candidate_player.team == state->current.team;
		if (positive_team && same_team) {
			if (!yt_direct_attack_team_row(target_name,
			    target_name_length, row, sizeof(row), &row_length)
			    || !ops->present(context, row, row_length,
			    YT_DIRECT_ATTACK_TEAM_ROW, error))
				return false;
			state->encountered = true;
			++state->candidate;
			continue;
		}

		state->encountered = true;
		if (!yt_direct_attack_candidate_prompt(target_name,
		    target_name_length, row, sizeof(row), &row_length)
		    || !ops->confirm(context, row, row_length, &answer, error))
			return false;
		if (answer == YT_DIRECT_ATTACK_CONFIRM_NO) {
			++state->candidate;
			continue;
		}
		if (answer != YT_DIRECT_ATTACK_CONFIRM_YES
		    && answer != YT_DIRECT_ATTACK_CONFIRM_EMPTY)
			return false;
		if (!yt_direct_attack_commitment_prompt(
		    (double)state->current.fighters, row, sizeof(row),
		    &row_length)
		    || !ops->present(context, row, row_length,
		    YT_DIRECT_ATTACK_COMMITMENT_PROMPT, error)
		    || !ops->amount(context, response, sizeof(response), error))
			return false;
		parsed = qb_val(response);
		state->committed = parsed.valid ? parsed.value : 0.0;
		if (state->committed < 1.0 || state->target_record_cell < 1) {
			state->route = YT_DIRECT_ATTACK_CANCELLED;
			state->complete = true;
			return true;
		}
		if (!ops->combat(context, record, state->committed, error))
			return false;
		state->route = YT_DIRECT_ATTACK_COMBAT_RETURN;
		state->complete = true;
		return true;
	}

	state->enter_sector = true;
	if (!ops->present(context,
	    state->encountered ? none_selected : none_visible,
	    state->encountered ? sizeof(none_selected) - 1U
	    : sizeof(none_visible) - 1U,
	    state->encountered ? YT_DIRECT_ATTACK_NONE_SELECTED_ROW
	    : YT_DIRECT_ATTACK_NONE_VISIBLE_ROW, error))
		return false;
	state->route = YT_DIRECT_ATTACK_EXHAUSTED;
	state->complete = true;
	return true;
}
