#include "direct_attack_model.h"

#include "qb.h"

#include <stdio.h>

static float
test_direct_attack_single_add(float left, float right)
{
	volatile float result = left + right;

	return result;
}

static bool
test_direct_attack_candidate_record(struct test_direct_attack_state *state,
    int *record, struct yt_error *error)
{
	bool overflow;
	int32_t converted = qb_cint_mode((double)state->candidate,
	    state->conversion_mode, &overflow);

	if (!overflow && yt_player_cache_contains((int)converted)) {
		*record = (int)converted;
		return true;
	}
	if (error != NULL) {
		error->status = YT_RANGE;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    "direct Attack candidate cache CINT");
	}
	return false;
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
	state->candidate = 2.0f;
	state->target_record_cell = 0.0f;
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
		float cached_sector;
		float cached_cloak;
		bool sector_mismatch;
		bool self;
		bool cloaked;
		bool positive_team;
		bool same_team;
		int record;

		if (!test_direct_attack_candidate_record(state, &record, error))
			return false;
		cached_sector = yt_player_cache_value(state->player_cache, record,
		    YT_PLAYER_CACHE_SECTOR);
		cached_cloak = yt_player_cache_value(state->player_cache, record,
		    YT_PLAYER_CACHE_CLOAK);
		sector_mismatch = cached_sector != state->current.sector;
		self = record == state->current_player_record;
		cloaked = cached_cloak > 0.0f;
		if (sector_mismatch || self || cloaked) {
			state->candidate = test_direct_attack_single_add(
			    state->candidate, 1.0f);
			continue;
		}

		state->target_record_cell = state->candidate;
		if (ops->store_target_record != NULL) {
			uint8_t target_record_raw[4];

			(void)qb_mbf32_encode(state->candidate,
			    target_record_raw);
			ops->store_target_record(context, target_record_raw);
		}
		if (!ops->read_player(context, record,
		    &state->candidate_player, error)
		    || !yt_player_stored_name(&state->candidate_player,
		    target_name, &target_name_length, error))
			return false;
		positive_team = state->candidate_player.team > 0.0f;
		same_team = state->candidate_player.team == state->current.team;
		if (positive_team && same_team) {
			if (!yt_direct_attack_team_row(target_name,
			    target_name_length, row, sizeof(row), &row_length)
			    || !ops->present(context, row, row_length,
			    YT_DIRECT_ATTACK_TEAM_ROW, error))
				return false;
			state->encountered = true;
			state->candidate = test_direct_attack_single_add(
			    state->candidate, 1.0f);
			continue;
		}

		state->encountered = true;
		if (!yt_direct_attack_candidate_prompt(target_name,
		    target_name_length, row, sizeof(row), &row_length)
		    || !ops->confirm(context, row, row_length, &answer, error))
			return false;
		if (answer == YT_DIRECT_ATTACK_CONFIRM_NO) {
			state->candidate = test_direct_attack_single_add(
			    state->candidate, 1.0f);
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
		if (state->committed < 1.0
		    || state->target_record_cell < 1.0f) {
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
