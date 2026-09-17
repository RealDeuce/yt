#include "projectile_command_model.h"

#include "qb.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static void
turn_gate_result_raw(bool denied, uint8_t raw[4])
{
	static const uint8_t false_value[4] = {0x00, 0x00, 0x7d, 0x00};
	static const uint8_t true_value[4] = {0x00, 0x00, 0x00, 0x81};

	memcpy(raw, denied ? true_value : false_value, 4U);
}

static bool
projectile_command_error(struct yt_error *error, enum yt_status status,
    const char *operation)
{
	if (error != NULL) {
		error->status = status;
		error->system_error = 0;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
		error->path[0] = '\0';
	}
	return false;
}

bool
test_projectile_command_run(struct test_projectile_command_state *state,
    const struct test_projectile_command_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t no_turns[] =
	    "Sorry but you have no turns left.";
	static const uint8_t no_ammunition[] = "You dont have any!";
	static const uint8_t invalid_sector[] = "Invalid Sector number!";
	static const uint8_t quantity_prompt[] = "Send how many? [0] ?";
	static const uint8_t too_many[] = "You dont have that many!";
	uint8_t prompt[192];
	char response[4096];
	struct qb_val_result parsed;
	enum qb_mbf_status conversion;
	size_t prompt_length;
	double integral;

	if (state == NULL || ops == NULL || state->destroyed == NULL
	    || state->current_player_record < 1 || state->maximum_sector < 1.0f
	    || ops->hydrate == NULL || ops->present == NULL
	    || ops->input == NULL || ops->finalize == NULL
	    || ops->write_player == NULL || ops->flush == NULL
	    || ops->resolve == NULL || ops->counterlaunch == NULL
	    || ops->xannor == NULL || ops->fatal == NULL)
		return projectile_command_error(error, YT_INVALID,
		    "projectile-command state");
	memset(&state->first_hydration, 0, sizeof(state->first_hydration));
	memset(&state->live_hydration, 0, sizeof(state->live_hydration));
	memset(&state->post_finalizer, 0, sizeof(state->post_finalizer));
	state->turn_gate_result_stores = 0U;
	state->attempts = 0U;
	state->hydrations = 0U;
	state->available = 0.0f;
	state->target = 0.0f;
	memset(state->target_raw, 0, sizeof(state->target_raw));
	state->target_stored = false;
	state->amount = 0.0f;
	memset(state->amount_raw, 0, sizeof(state->amount_raw));
	state->amount_stored = false;
	state->origin = 0.0f;
	memset(state->origin_raw, 0, sizeof(state->origin_raw));
	state->counterattack = 0;
	state->xannor_provoker = 0;
	state->finalizer_called = false;
	state->player_written = false;
	state->player_flushed = false;
	state->destruction_cleared = false;
	state->resolver_called = false;
	state->counterlaunch_called = false;
	state->xannor_called = false;
	state->fatal_called = false;
	state->route = YT_PROJECTILE_COMMAND_INCOMPLETE;
	state->complete = false;

	for (;;) {
		state->attempts++;
		if (!ops->present(context, NULL, 0U,
		    YT_PROJECTILE_COMMAND_OPENING_BLANK, error)
		    || !ops->hydrate(context, state->current_player_record,
		    &state->first_hydration, error))
			return false;
		state->hydrations++;
		if (!ops->hydrate(context, state->current_player_record,
		    &state->live_hydration, error))
			return false;
		state->hydrations++;
		turn_gate_result_raw(false, state->turn_gate_result_raw);
		state->turn_gate_result_stores++;
		if (ops->store_turn_gate_result != NULL)
			ops->store_turn_gate_result(context,
			    state->turn_gate_result_raw);
		if (state->live_hydration.turns <= 0.0f) {
			turn_gate_result_raw(true,
			    state->turn_gate_result_raw);
			state->turn_gate_result_stores++;
			if (ops->store_turn_gate_result != NULL)
				ops->store_turn_gate_result(context,
				    state->turn_gate_result_raw);
			state->route = YT_PROJECTILE_COMMAND_NO_TURNS;
			if (!ops->present(context, no_turns,
			    sizeof(no_turns) - 1U,
			    YT_PROJECTILE_COMMAND_NO_TURNS_ROW, error))
				return false;
			state->complete = true;
			return true;
		}
		state->available = state->plasma
		    ? state->live_hydration.plasma
		    : state->live_hydration.missiles;
		if (state->available < 1.0f) {
			state->route = YT_PROJECTILE_COMMAND_NO_AMMUNITION;
			if (!ops->present(context, no_ammunition,
			    sizeof(no_ammunition) - 1U,
			    YT_PROJECTILE_COMMAND_NO_AMMUNITION_ROW, error))
				return false;
			state->complete = true;
			return true;
		}
		if (!yt_projectile_target_prompt(state->plasma, state->displayed,
		    state->maximum_sector, prompt, sizeof(prompt), &prompt_length)
		    || !ops->present(context, prompt, prompt_length,
		    YT_PROJECTILE_COMMAND_TARGET_PROMPT, error)
		    || !ops->input(context, response, sizeof(response), error))
			return false;
		if (response[0] == '\0') {
			state->route = YT_PROJECTILE_COMMAND_TARGET_CANCELLED;
			state->complete = true;
			return true;
		}
		parsed = qb_val(response);
		if (parsed.overflow)
			return projectile_command_error(error, YT_RANGE,
			    "projectile target VAL");
		state->target = (float)(parsed.valid ? parsed.value : 0.0);
		conversion = qb_mbf32_encode(state->target, state->target_raw);
		if (conversion == QB_MBF_OVERFLOW)
			return projectile_command_error(error, YT_RANGE,
			    "projectile target CSNG");
		state->target = qb_mbf32_decode(state->target_raw);
		state->target_stored = true;
		if (state->target >= 1.0f
		    && state->target <= state->maximum_sector)
			break;
		if (!ops->present(context, invalid_sector,
		    sizeof(invalid_sector) - 1U,
		    YT_PROJECTILE_COMMAND_INVALID_SECTOR_ROW, error))
			return false;
	}

	if (!ops->present(context, quantity_prompt,
	    sizeof(quantity_prompt) - 1U,
	    YT_PROJECTILE_COMMAND_QUANTITY_PROMPT, error)
	    || !ops->input(context, response, sizeof(response), error))
		return false;
	parsed = qb_val(response);
	if (parsed.overflow)
		return projectile_command_error(error, YT_RANGE,
		    "projectile quantity VAL");
	integral = floor(parsed.valid ? parsed.value : 0.0);
	state->amount = (float)integral;
	conversion = qb_mbf32_encode(state->amount, state->amount_raw);
	if (conversion == QB_MBF_OVERFLOW)
		return projectile_command_error(error, YT_RANGE,
		    "projectile quantity CSNG");
	state->amount = qb_mbf32_decode(state->amount_raw);
	state->amount_stored = true;
	if (state->amount < 1.0f) {
		state->route = YT_PROJECTILE_COMMAND_QUANTITY_CANCELLED;
		state->complete = true;
		return true;
	}
	if (state->amount > state->available) {
		state->route = YT_PROJECTILE_COMMAND_TOO_MANY;
		if (!ops->present(context, too_many, sizeof(too_many) - 1U,
		    YT_PROJECTILE_COMMAND_TOO_MANY_ROW, error))
			return false;
		state->complete = true;
		return true;
	}
	if (!ops->present(context, NULL, 0U,
	    YT_PROJECTILE_COMMAND_ACCEPTED_BLANK, error))
		return false;
	state->finalizer_called = true;
	if (!ops->finalize(context, &state->post_finalizer, error)) {
		if (error == NULL || error->status == YT_OK) {
			state->route = YT_PROJECTILE_COMMAND_FINALIZER_TERMINAL;
			state->complete = true;
			return true;
		}
		return false;
	}
	state->origin = (float)state->post_finalizer.sector;
	memcpy(state->origin_raw,
	    state->post_finalizer.record.bytes + YT_F57,
	    sizeof(state->origin_raw));
	yt_projectile_debit_overlay(&state->post_finalizer, state->plasma,
	    state->amount);
	if (!ops->write_player(context, state->current_player_record,
	    &state->post_finalizer, error))
		return false;
	state->player_written = true;
	if (!ops->flush(context, error))
		return false;
	state->player_flushed = true;
	*state->destroyed = false;
	state->destruction_cleared = true;
	state->resolver_called = true;
	if (!ops->resolve(context, &state->origin, state->origin_raw,
	    &state->target, state->target_raw, &state->amount,
	    state->amount_raw, state->plasma, &state->counterattack,
	    &state->xannor_provoker, error))
		return false;
	if (ops->counterattack_truth != NULL
	    ? ops->counterattack_truth(context) : state->counterattack != 0) {
		state->counterlaunch_called = true;
		if (!ops->counterlaunch(context, &state->counterattack,
		    &state->xannor_provoker, error))
			return false;
	}
	if (ops->xannor_truth != NULL
	    ? ops->xannor_truth(context) : state->xannor_provoker != 0) {
		state->xannor_called = true;
		if (!ops->xannor(context, &state->xannor_provoker, error))
			return false;
	}
	if (ops->destroyed_truth != NULL
	    ? ops->destroyed_truth(context) : *state->destroyed) {
		state->route = YT_PROJECTILE_COMMAND_FATAL;
		state->fatal_called = true;
		if (!ops->fatal(context, error))
			return false;
	} else {
		state->route = YT_PROJECTILE_COMMAND_RETURNED;
	}
	state->complete = true;
	return true;
}
