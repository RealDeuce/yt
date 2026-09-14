#include "yt_game.h"
#include "yt_port_math.h"

#include "qb.h"
#include "yt_main_error.h"
#include "yt_pager.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool
startup_configuration_error(struct yt_error *error, enum yt_status status,
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

static float
startup_single_subtract(float left, float right)
{
	volatile float result = left - right;

	return result;
}

static float
startup_single_multiply(float left, float right)
{
	volatile float result = left * right;

	return result;
}

static float
startup_single_add(float left, float right)
{
	volatile float result = left + right;

	return result;
}


bool
yt_xannor_victory_mks_internal_fatal_run(uint16_t module_segment,
    bool redirected_stdin, bool function_bar, bool cursor_shape_known,
    uint16_t process_entry_cursor_shape,
    const struct yt_brun_internal_fatal_ops *ops, void *context,
    struct yt_brun_internal_fatal_state *state)
{
	return yt_brun_internal_fatal_run(YT_BRUN_INTERNAL_FATAL_GC,
	    "YT-SUB  ", true, 64006, module_segment, 0xA995U,
	    redirected_stdin, function_bar, cursor_shape_known,
	    process_entry_cursor_shape, ops, context, state);
}

bool
yt_main_startup_internal_fatal_run(uint16_t module_segment,
    bool redirected_stdin, bool function_bar, bool cursor_shape_known,
    uint16_t process_entry_cursor_shape,
    const struct yt_brun_internal_fatal_ops *ops, void *context,
    struct yt_brun_internal_fatal_state *state)
{
	return yt_brun_internal_fatal_run(YT_BRUN_INTERNAL_FATAL_OWNER,
	    "YT      ", true, 3, module_segment, 0x0136U, redirected_stdin,
	    function_bar, cursor_shape_known, process_entry_cursor_shape,
	    ops, context, state);
}

bool
yt_game_load_startup_configuration(struct yt_game *game, const char *path,
    bool local_mode, struct yt_player_cache *player_cache,
    float disruption_sectors[2], float *local_screen, struct yt_error *error)
{
	struct yt_config *config;
	bool overflow;
	int32_t path_count;
	float counter;
	size_t index;

	if (game == NULL || path == NULL || player_cache == NULL
	    || disruption_sectors == NULL || local_screen == NULL)
		return false;
	config = &game->config;
	if (!yt_database_random_close(&game->database, error)
	    || !yt_database_open(&game->database, path,
	    YT_OPEN_UPDATE_CREATE, error)
	    || !yt_config_load(&game->database, config, error))
		return false;
	path_count = qb_cint_mbf32(config->record.bytes + YT_F41, 0U,
	    &overflow);
	if (overflow || path_count < 0)
		return startup_configuration_error(error, YT_RANGE,
		    "startup scoreboard LEFT$");
	config->scoreboard_length = (float)path_count;
	if (config->scoreboard_length > YT_TEXT_FIELD_SIZE)
		config->scoreboard_length = YT_TEXT_FIELD_SIZE;
	memcpy(config->scoreboard, config->record.bytes,
	    (size_t)config->scoreboard_length);
	*local_screen = config->local_screen;
	qb_compat_upper_n((uint8_t *)config->scoreboard,
	    (size_t)config->scoreboard_length);
	config->scoreboard[(size_t)config->scoreboard_length] = '\0';

	if (config->headquarters == 0.0f) {
		static const uint8_t headquarters_default[4] = {
			0x00, 0x40, 0x37, 0x8a
		};

		if (!yt_record_set_raw_number(&config->record, YT_F117,
		    headquarters_default)
		    || !yt_database_write_durable(&game->database, 1U,
		    &config->record, error))
			return false;
		config->headquarters = 733.0f;
	}
	if (config->genesis_ports < 20.0f) {
		config->genesis_ports = 200.0f;
	}
	if (config->scoreboard_length == 0.0f) {
		static const char default_path[] = "YTSCORE.ASC";

		memcpy(config->scoreboard, default_path, sizeof(default_path));
		config->scoreboard_length = sizeof(default_path) - 1U;
	}
	if (config->local_screen < -1.0f || config->local_screen > 0.0f
	    || local_mode) {
		config->local_screen = -1.0f;
		*local_screen = -1.0f;
	}
	if (config->lottery_plays < 0.0f || config->lottery_plays > 9.0f) {
		config->lottery_plays = 3.0f;
	}
	if (config->maximum_planets == 0.0f) {
		config->maximum_planets = 100.0f;
	}
	if (config->maximum_holds < 5.0f || config->maximum_holds > 1000.0f) {
		config->maximum_holds = 1000.0f;
	}
	if (config->turns_per_day < 100.0f
	    || config->turns_per_day > 2500.0f) {
		config->turns_per_day = 500.0f;
	}

	counter = 2.0f;
	while (counter <= config->sector_offset) {
		struct yt_player player;
		int32_t basic = qb_cint(counter, &overflow);

		if (overflow || !yt_player_cache_contains(basic))
			return startup_configuration_error(error, YT_RANGE,
			    "startup player-cache index");
		if (!yt_game_read_player(game, basic, &player, error))
			return false;
		(void)yt_player_cache_set_raw(player_cache, basic,
		    YT_PLAYER_CACHE_SECTOR, player.record.bytes + YT_F57);
		(void)yt_player_cache_set_raw(player_cache, basic,
		    YT_PLAYER_CACHE_CLOAK, player.record.bytes + YT_F125);
		if (player.cloak < 0.0f || player.cloak > 1.0f) {
			static const uint8_t one[4] = {
				0x00U, 0x00U, 0x00U, 0x81U
			};

			player.cloak = 1.0f;
			(void)yt_player_cache_set_raw(player_cache, basic,
			    YT_PLAYER_CACHE_CLOAK, one);
			if (!yt_record_set_number(&player.record, YT_F125, 1.0f)
			    || !yt_database_write_durable(&game->database,
			    (size_t)basic, &player.record, error))
				return false;
		}
		counter = startup_single_add(counter, 1.0f);
	}
	for (index = 0U; index < 2U; ++index) {
		float draw;
		float difference;
		float span;
		float product;
		float integral;
		uint8_t raw[4];

		if (!yt_random_next(&game->random, &draw, error))
			return false;
		difference = startup_single_subtract(config->port_offset,
		    config->sector_offset);
		span = startup_single_subtract(difference, 2.0f);
		product = startup_single_multiply(draw, span);
		integral = floorf(product);
		disruption_sectors[index] = startup_single_add(integral, 2.0f);
		if (integral == -2.0f) {
			static const uint8_t dirty_zero[4] = {
				0x00U, 0x00U, 0x80U, 0x00U
			};

			memcpy(raw, dirty_zero, sizeof(raw));
		} else if (qb_mbf32_encode(disruption_sectors[index], raw)
		    != QB_MBF_OK) {
			return startup_configuration_error(error, YT_RANGE,
			    "startup disruption result");
		}
		disruption_sectors[index] = qb_mbf32_decode(raw);
	}
	return true;
}

static float projectile_single_add(float left, float right);
static float projectile_single_sub(float left, float right);
static float projectile_single_mul(float left, float right);



bool
yt_projectile_target_prompt(bool plasma, float displayed, float maximum,
    uint8_t *prompt, size_t capacity, size_t *length)
{
	const char *label = plasma ? " plasma bolt " : " cruise missile ";
	char displayed_text[64];
	char maximum_text[64];
	int written;

	if (prompt == NULL || length == NULL || capacity == 0U
	    || qb_str_single(displayed_text, sizeof(displayed_text), displayed)
	    < 0
	    || qb_str_single(maximum_text, sizeof(maximum_text), maximum) < 0)
		return false;
	written = snprintf((char *)prompt, capacity,
	    "You have%s. Send your%sto what sector? [ 1 to%s ] ?",
	    displayed_text, label, maximum_text);
	if (written < 0 || (size_t)written >= capacity)
		return false;
	*length = (size_t)written;
	return true;
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
yt_projectile_command_run(struct yt_projectile_command_state *state,
    const struct yt_projectile_command_ops *ops, void *context,
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
		yt_no_turn_gate_result_raw(false, state->turn_gate_result_raw);
		state->turn_gate_result_stores++;
		if (ops->store_turn_gate_result != NULL)
			ops->store_turn_gate_result(context,
			    state->turn_gate_result_raw);
		if (state->live_hydration.turns <= 0.0f) {
			yt_no_turn_gate_result_raw(true, state->turn_gate_result_raw);
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
	state->origin = state->post_finalizer.sector;
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
	}
	else {
		state->route = YT_PROJECTILE_COMMAND_RETURNED;
	}
	state->complete = true;
	return true;
}

void
yt_projectile_plasma_opening_values(float bolts, double *energy,
    float *hop_loss)
{
	volatile double quotient;
	volatile float rounded;

	*energy = (double)projectile_single_mul(2500000.0f, bolts);
	quotient = *energy / 50.0;
	rounded = (float)quotient;
	*hop_loss = rounded;
}

bool
yt_projectile_plasma_energy_row(double energy, uint8_t *row,
    size_t capacity, size_t *length)
{
	char number[64];
	int written;

	if (row == NULL || length == NULL
	    || qb_str_double(number, sizeof(number), energy) < 0)
		return false;
	written = snprintf((char *)row, capacity,
	    "Plasma bolts targeted... firing%s megawatts!", number);
	if (written < 0 || (size_t)written >= capacity)
		return false;
	*length = (size_t)written;
	return true;
}

bool
yt_projectile_plasma_firing_row(float counter, uint8_t *row,
    size_t capacity, size_t *length)
{
	char number[64];
	int written;

	if (row == NULL || length == NULL
	    || qb_str_single(number, sizeof(number), counter) < 0)
		return false;
	written = snprintf((char *)row, capacity, "Firing%s!", number);
	if (written < 0 || (size_t)written >= capacity)
		return false;
	*length = (size_t)written;
	return true;
}

float
yt_projectile_plasma_next_firing(float counter)
{
	return projectile_single_add(counter, 1.0f);
}

bool
yt_projectile_plasma_route_run(
    struct yt_projectile_plasma_route_state *state,
    const struct yt_projectile_plasma_route_ops *ops, void *context,
    struct yt_error *error)
{
	char first[64];
	char second[64];
	uint8_t row[192];
	size_t steps = 0U;
	bool rerouted;
	bool process_route;

	if (state == NULL || ops == NULL || state->origin == NULL
	    || state->destination == NULL || state->energy == NULL
	    || state->step_limit == 0U || ops->build_route == NULL
	    || ops->line == NULL || ops->attention == NULL
	    || ops->wait == NULL || ops->random == NULL
	    || ops->impact == NULL || ops->footer == NULL)
		return false;
	process_route = ops->read_route != NULL || ops->write_route != NULL;
	if ((process_route && (ops->read_route == NULL
	    || ops->write_route == NULL)) || (!process_route
	    && (state->route == NULL || state->route_capacity == 0U)))
		return false;
	state->route_calls = 0U;
	state->hops = 0U;
	for (;;) {
		bool overflow;
		int destination_index;

		if (++steps > state->step_limit)
			return false;
		if (*state->destination == *state->origin) {
			destination_index = qb_cint(*state->destination, &overflow);
			if (overflow || (!process_route && (destination_index < 0
			    || (size_t)destination_index >= state->route_capacity)))
				return false;
			*state->origin = 0.0f;
			if (ops->arguments_changed != NULL)
				ops->arguments_changed(context, *state->origin,
				    *state->destination,
				    YT_PROJECTILE_PLASMA_SAME_ORIGIN_ZERO);
			if (process_route) {
				ops->write_route(context, 0,
				    (int16_t)destination_index);
				ops->write_route(context,
				    (int16_t)destination_index, 0);
			}
			else {
				state->route[0] = (int16_t)destination_index;
				state->route[destination_index] = 0;
			}
		}
		else {
			state->route_status = 0.0f;
			++state->route_calls;
			if (!ops->build_route(context, state->origin,
			    state->destination, state->route,
			    state->route_capacity, &state->route_status, error))
				return false;
		}
		state->current_hop = *state->origin;
		rerouted = false;
		for (;;) {
			enum yt_projectile_plasma_impact_route impact_route;
			int current_index;
			int next_hop;
			int written;

			if (++steps > state->step_limit)
				return false;
			if (state->current_hop != *state->origin)
				*state->energy -= (double)state->hop_loss;
			current_index = qb_cint(state->current_hop, &overflow);
			if (overflow || (!process_route && (current_index < 0
			    || (size_t)current_index >= state->route_capacity)))
				return false;
			next_hop = process_route
			    ? ops->read_route(context, (int16_t)current_index)
			    : state->route[current_index];
			state->current_hop = (float)next_hop;
			if (next_hop == 0 || *state->energy < 1.0)
				return ops->footer(context, NULL, 0U, error);
			if (qb_str_single(first, sizeof(first), (float)next_hop) < 0
			    || qb_str_double(second, sizeof(second),
			    floor(*state->energy)) < 0)
				return false;
			written = snprintf((char *)row, sizeof(row),
			    "Bolt entering sector%s.%s Megawatts remaining.",
			    first, second);
			if (written < 0 || (size_t)written >= sizeof(row)
			    || !ops->line(context, row, (size_t)written, error)
			    || !ops->wait(context, 0.5f, error))
				return false;
			++state->hops;
			if ((float)next_hop == state->black_hole[0]
			    || (float)next_hop == state->black_hole[1]) {
				float draw;
				float span;

				*state->origin = (float)next_hop;
				if (ops->arguments_changed != NULL)
					ops->arguments_changed(context, *state->origin,
					    *state->destination,
					    YT_PROJECTILE_PLASMA_BLACK_HOLE_ORIGIN);
				if (!ops->random(context, &draw, error))
					return false;
				span = projectile_single_sub(state->port_record_offset,
				    state->sector_record_offset);
				*state->destination = floorf(projectile_single_add(
				    projectile_single_mul(draw, span), 1.0f));
				if (ops->arguments_changed != NULL)
					ops->arguments_changed(context, *state->origin,
					    *state->destination,
					    YT_PROJECTILE_PLASMA_BLACK_HOLE_DESTINATION);
				if (!ops->line(context, NULL, 0U, error)
				    || qb_str_single(first, sizeof(first),
				    (float)next_hop) < 0
				    || qb_str_single(second, sizeof(second),
				    *state->destination) < 0)
					return false;
				written = snprintf((char *)row, sizeof(row),
				    "The plasma bolt is deflected by a black hole in "
				    "sector%s to sector%s!", first, second);
				if (written < 0 || (size_t)written >= sizeof(row)
				    || !ops->attention(context, row, (size_t)written,
				    error)
				    || !ops->line(context, NULL, 0U, error))
					return false;
				rerouted = true;
				break;
			}
			impact_route = YT_PROJECTILE_PLASMA_NEXT_HOP;
			if (!ops->impact(context, next_hop, state->energy,
			    &impact_route, error))
				return false;
			if (impact_route == YT_PROJECTILE_PLASMA_FOOTER)
				return ops->footer(context, NULL, 0U, error);
			if (impact_route != YT_PROJECTILE_PLASMA_NEXT_HOP)
				return false;
		}
		if (!rerouted)
			return false;
	}
}

enum yt_projectile_target_result
yt_projectile_target_response(const char *response, float maximum,
    float *target)
{
	struct qb_val_result parsed;
	float candidate;

	if (response == NULL || target == NULL || response[0] == '\0')
		return YT_PROJECTILE_TARGET_CANCEL;
	parsed = qb_val(response);
	candidate = (float)(parsed.valid ? parsed.value : 0.0);
	if (candidate < 1.0f || candidate > maximum)
		return YT_PROJECTILE_TARGET_RETRY;
	*target = candidate;
	return YT_PROJECTILE_TARGET_ACCEPT;
}

float
yt_projectile_quantity_response(const char *response)
{
	struct qb_val_result parsed;

	if (response == NULL)
		return 0.0f;
	parsed = qb_val(response);
	return (float)floor(parsed.valid ? parsed.value : 0.0);
}

void
yt_projectile_debit_overlay(struct yt_player *player, bool plasma,
    float amount)
{
	volatile float remaining;
	size_t offset;

	if (player == NULL)
		return;
	if (plasma) {
		remaining = player->plasma - amount;
		player->plasma = remaining;
		offset = YT_F113;
	}
	else {
		remaining = player->missiles - amount;
		player->missiles = remaining;
		offset = YT_F97;
	}
	(void)yt_record_set_number(&player->record, offset, remaining);
}

bool
yt_projectile_commit(struct yt_game *game, int player_record,
    struct yt_player *player, bool plasma, float *origin, float target,
    float amount, bool *destroyed, int *counterattack, int *xannor_provoker,
    yt_projectile_resolver_fn resolver, void *resolver_context,
    struct yt_error *error)
{
	if (game == NULL || player == NULL || origin == NULL
	    || destroyed == NULL || counterattack == NULL
	    || xannor_provoker == NULL || resolver == NULL) {
		if (error != NULL) {
			error->status = YT_INVALID;
			error->system_error = 0;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "projectile commit");
		}
		return false;
	}
	yt_projectile_debit_overlay(player, plasma, amount);
	if (!yt_game_write_player(game, player_record, player, error)
	    || !yt_database_flush(&game->database, error))
		return false;
	/* YT:22AC clears the fatal result only after the PUT completes. */
	*destroyed = false;
	return resolver(resolver_context, origin, &target, &amount, plasma,
	    counterattack, xannor_provoker, error);
}

float
yt_counterlaunch_score_count(double cached_score, float retained)
{
	static const uint8_t score_factor_raw[8] = {
		0x84, 0x47, 0x1b, 0x47, 0xac, 0xc5, 0x27, 0x70
	};
	volatile double product;
	volatile double integral;
	volatile double result;

	if (cached_score <= 0.0)
		return retained;
	product = cached_score * qb_mbf64_decode(score_factor_raw);
	integral = floor(product);
	result = integral + 1.0;
	return (float)result;
}

void
yt_counterlaunch_debit_overlay(struct yt_player *fresh_target,
    float first_available, float selected_count)
{
	volatile float remaining;

	if (fresh_target == NULL)
		return;
	remaining = first_available - selected_count;
	fresh_target->missiles = remaining;
	(void)yt_record_set_number(&fresh_target->record, YT_F97, remaining);
}

bool
yt_counterlaunch_rows(const uint8_t *target_name,
    size_t target_name_length, float selected_count,
    const uint8_t *saved_name, size_t saved_name_length,
    uint8_t *terminal, size_t terminal_capacity, size_t *terminal_length,
    uint8_t *news, size_t news_capacity, size_t *news_length)
{
	static const uint8_t middle[] = " shot back with";
	static const uint8_t terminal_suffix[] = " missiles at you!";
	static const uint8_t news_middle[] = " missiles at ";
	char number[64];
	int number_length;
	size_t terminal_needed;
	size_t news_needed;
	size_t position;

	if (terminal_length == NULL || news_length == NULL
	    || (target_name == NULL && target_name_length != 0U)
	    || (saved_name == NULL && saved_name_length != 0U))
		return false;
	*terminal_length = 0U;
	*news_length = 0U;
	number_length = qb_str_single(number, sizeof(number), selected_count);
	if (number_length < 0)
		return false;
	terminal_needed = target_name_length + sizeof(middle) - 1U
	    + (size_t)number_length + sizeof(terminal_suffix) - 1U;
	news_needed = target_name_length + sizeof(middle) - 1U
	    + (size_t)number_length + sizeof(news_middle) - 1U
	    + saved_name_length + 1U;
	if (terminal_needed > terminal_capacity || news_needed > news_capacity
	    || (terminal_needed != 0U && terminal == NULL)
	    || (news_needed != 0U && news == NULL))
		return false;
	position = 0U;
	if (target_name_length != 0U)
		memcpy(terminal + position, target_name, target_name_length);
	position += target_name_length;
	memcpy(terminal + position, middle, sizeof(middle) - 1U);
	position += sizeof(middle) - 1U;
	memcpy(terminal + position, number, (size_t)number_length);
	position += (size_t)number_length;
	memcpy(terminal + position, terminal_suffix,
	    sizeof(terminal_suffix) - 1U);
	*terminal_length = terminal_needed;

	position = 0U;
	if (target_name_length != 0U)
		memcpy(news + position, target_name, target_name_length);
	position += target_name_length;
	memcpy(news + position, middle, sizeof(middle) - 1U);
	position += sizeof(middle) - 1U;
	memcpy(news + position, number, (size_t)number_length);
	position += (size_t)number_length;
	memcpy(news + position, news_middle, sizeof(news_middle) - 1U);
	position += sizeof(news_middle) - 1U;
	if (saved_name_length != 0U)
		memcpy(news + position, saved_name, saved_name_length);
	position += saved_name_length;
	news[position] = '!';
	*news_length = news_needed;
	return true;
}

static bool
salvage_append(uint8_t *row, size_t capacity, size_t *position,
    const void *text, size_t length)
{
	if (*position > capacity || length > capacity - *position)
		return false;
	if (length != 0U)
		memcpy(row + *position, text, length);
	*position += length;
	return true;
}

bool
yt_salvage_header_row(const uint8_t *salvor, size_t salvor_length,
    const uint8_t *victim, size_t victim_length, uint8_t *row,
    size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = " *** ";
	static const uint8_t middle[] = " salvaged the following from ";
	static const uint8_t suffix[] = "'s ship:";
	size_t position = 0U;

	if (length == NULL)
		return false;
	*length = 0U;
	if (row == NULL || (salvor == NULL && salvor_length != 0U)
	    || (victim == NULL && victim_length != 0U))
		return false;
	if (!salvage_append(row, capacity, &position, prefix,
	    sizeof(prefix) - 1U)
	    || !salvage_append(row, capacity, &position, salvor, salvor_length)
	    || !salvage_append(row, capacity, &position, middle,
	    sizeof(middle) - 1U)
	    || !salvage_append(row, capacity, &position, victim, victim_length)
	    || !salvage_append(row, capacity, &position, suffix,
	    sizeof(suffix) - 1U))
		return false;
	*length = position;
	return true;
}

bool
yt_salvage_simple_row(enum yt_salvage_simple_kind kind, float amount,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const char *const labels[] = {
		"Credits:", "Cruise Missiles:", "Plasma Bolts:",
		"Ground Forces:", "Sector Mines:"
	};
	static const uint8_t prefix[] = "  -  ";
	char number[64];
	int number_length;
	size_t position = 0U;

	if (length == NULL)
		return false;
	*length = 0U;
	if (row == NULL || kind < YT_SALVAGE_CREDITS
	    || kind > YT_SALVAGE_MINES)
		return false;
	number_length = qb_str_single(number, sizeof(number), amount);
	if (number_length < 0
	    || !salvage_append(row, capacity, &position, prefix,
	    sizeof(prefix) - 1U)
	    || !salvage_append(row, capacity, &position, labels[kind],
	    strlen(labels[kind]))
	    || !salvage_append(row, capacity, &position, number,
	    (size_t)number_length))
		return false;
	*length = position;
	return true;
}

bool
yt_salvage_cargo_row(enum yt_salvage_cargo_kind kind, float amount,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const char *const suffix[] = {
		" empty holds", " holds of ore", " holds of organics",
		" holds of equipment"
	};
	static const uint8_t prefix[] = "  - ";
	char number[64];
	int number_length;
	size_t position = 0U;

	if (length == NULL)
		return false;
	*length = 0U;
	if (row == NULL || kind < YT_SALVAGE_EMPTY_HOLDS
	    || kind > YT_SALVAGE_EQUIPMENT)
		return false;
	number_length = qb_str_single(number, sizeof(number), amount);
	if (number_length < 0
	    || !salvage_append(row, capacity, &position, prefix,
	    sizeof(prefix) - 1U)
	    || !salvage_append(row, capacity, &position, number,
	    (size_t)number_length)
	    || !salvage_append(row, capacity, &position, suffix[kind],
	    strlen(suffix[kind])))
		return false;
	*length = position;
	return true;
}

int
yt_nearest_filter_selector(const uint8_t *response, size_t length)
{
	static const uint8_t alphabet[] = "123ATYEU";
	size_t offset;

	if (length == 0U)
		return 4;
	if (length > sizeof(alphabet) - 1U)
		return 0;
	for (offset = 0U; offset + length <= sizeof(alphabet) - 1U;
	    ++offset) {
		if (memcmp(alphabet + offset, response, length) == 0)
			return (int)offset + 1;
	}
	return 0;
}

bool
yt_nearest_direction_prompt(int selector, uint8_t *prompt,
    size_t capacity, size_t *length)
{
	static const char *const commodities[3] = {
		"Equipment", "Organics", "Ore"
	};
	int written;

	if (selector < 1 || selector > 3 || prompt == NULL || length == NULL)
		return false;
	written = snprintf((char *)prompt, capacity,
	    "Find ports [B] Buying or [S] Selling %s -=> ",
	    commodities[selector - 1]);
	if (written < 0 || (size_t)written >= capacity)
		return false;
	*length = (size_t)written;
	return true;
}

static bool
nearest_error(struct yt_error *error, const char *operation)
{
	if (error != NULL) {
		error->status = YT_RANGE;
		error->system_error = 0;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
		error->path[0] = '\0';
	}
	return false;
}

static bool
nearest_single(float value, float *result, struct yt_error *error,
    const char *operation)
{
	uint8_t raw[4];
	enum qb_mbf_status status;

	status = qb_mbf32_encode(value, raw);
	if (!isfinite(value) || status == QB_MBF_OVERFLOW
	    || status == QB_MBF_DOMAIN)
		return nearest_error(error, operation);
	*result = qb_mbf32_decode(raw);
	return true;
}

static bool
nearest_add(float left, float right, float *result, struct yt_error *error,
    const char *operation)
{
	volatile float value = left + right;

	return nearest_single(value, result, error, operation);
}

static bool
nearest_sub(float left, float right, float *result, struct yt_error *error,
    const char *operation)
{
	volatile float value = left - right;

	return nearest_single(value, result, error, operation);
}

static bool
nearest_mul(float left, float right, float *result, struct yt_error *error,
    const char *operation)
{
	volatile float value = left * right;

	return nearest_single(value, result, error, operation);
}

static bool
nearest_div(float left, float right, float *result, struct yt_error *error,
    const char *operation)
{
	volatile float value;

	if (right == 0.0f)
		return nearest_error(error, operation);
	value = left / right;
	return nearest_single(value, result, error, operation);
}

static void
nearest_market_copy(struct yt_nearest_market *market,
    const struct yt_port *port)
{
	size_t index;

	memset(market, 0, sizeof(*market));
	market->stored_day = port->last_day;
	market->stored_minute = port->last_minute;
	for (index = 0U; index < 3U; ++index) {
		market->stock[index] = port->stock[index];
		market->production[index] = port->production[index];
		market->factor[index] = port->factor[index];
	}
}

bool
yt_nearest_market_project(struct yt_nearest_market *market,
    const struct yt_port *port, const float base_price[3],
    float current_day, float timer_seconds, struct yt_error *error)
{
	float day_delta;
	float minute_delta;
	size_t index;

	if (market == NULL || port == NULL || base_price == NULL)
		return startup_configuration_error(error, YT_INVALID,
		    "nearest market arguments");
	nearest_market_copy(market, port);
	if (!nearest_div(timer_seconds, 60.0f, &market->minute, error,
	    "nearest market minute")
	    || !nearest_sub(current_day, market->stored_day, &day_delta, error,
	    "nearest market day delta")
	    || !nearest_sub(market->minute, market->stored_minute, &minute_delta,
	    error, "nearest market minute delta")
	    || !nearest_div(minute_delta, 1440.0f, &minute_delta, error,
	    "nearest market minute fraction")
	    || !nearest_add(day_delta, minute_delta, &market->elapsed, error,
	    "nearest market elapsed"))
		return false;
	if (market->elapsed > 10.0f || market->elapsed < 0.0f)
		market->elapsed = 10.0f;

	for (index = 0U; index < 3U; ++index) {
		float growth;
		float candidate;
		float numerator;
		float denominator;
		float ratio;
		float scale;
		float raw;
		float rounded;

		if (!nearest_mul(market->production[index], market->elapsed,
		    &growth, error, "nearest market growth")
		    || !nearest_add(port->stock[index], growth,
		    &market->stock[index], error, "nearest market stock")
		    || !nearest_div(market->stock[index], 10.0f, &candidate,
		    error, "nearest market production comparison"))
			return false;
		if (candidate > market->production[index]
		    && !nearest_div(market->stock[index], 10.0f,
		    &market->production[index], error,
		    "nearest market production replacement"))
			return false;
		if (!nearest_mul(market->factor[index], market->stock[index],
		    &numerator, error, "nearest market numerator")
		    || !nearest_mul(market->production[index], 1000.0f,
		    &denominator, error, "nearest market denominator")
		    || !nearest_div(numerator, denominator, &ratio, error,
		    "nearest market ratio")
		    || !nearest_sub(1.0f, ratio, &scale, error,
		    "nearest market scale")
		    || !nearest_mul(base_price[index], scale, &raw, error,
		    "nearest market raw price")
		    || !nearest_add(raw, 0.5f, &rounded, error,
		    "nearest market price rounding"))
			return false;
		rounded = floorf(rounded);
		if (!nearest_single(rounded, &market->price[index], error,
		    "nearest market price INT"))
			return false;
		if (market->price[index] < 1.0f)
			market->price[index] = 1.0f;
	}
	return true;
}

static bool
nearest_cint(const struct yt_nearest_state *state, float value, int *result,
    struct yt_error *error, const char *operation)
{
	bool overflow;
	int32_t converted = qb_cint_mode((double)value,
	    state->conversion_mode, &overflow);

	if (overflow)
		return nearest_error(error, operation);
	*result = (int)converted;
	return true;
}

static bool
nearest_present(struct yt_nearest_state *state,
    const struct yt_nearest_ops *ops, void *context,
    enum yt_nearest_output_kind kind, enum yt_nearest_present_mode mode,
    const uint8_t *text, size_t length, struct yt_error *error)
{
	if (mode == YT_NEAREST_PRESENT_BOLD_LINE
	    || mode == YT_NEAREST_PRESENT_BOLD_RAW)
		state->style.bold = 1.0f;
	if (!ops->present(context, kind, mode, text, length, &state->style,
	    error))
		return false;
	++state->outputs;
	return true;
}

static bool
nearest_read(struct yt_nearest_state *state,
    const struct yt_nearest_ops *ops, void *context,
    enum yt_nearest_field_kind kind, float expression,
    struct yt_record *record, struct yt_error *error)
{
	uint32_t physical;

	if (!nearest_single(expression, &state->current_record_expression,
	    error, "nearest record expression"))
		return false;
	physical = qb_brun_random_record_number(
	    state->current_record_expression);
	if (!ops->read_record(context, kind, state->current_record_expression,
	    physical, record, error))
		return false;
	state->field = *record;
	state->field_kind = kind;
	state->field_record = physical;
	state->field_valid = true;
	++state->reads;
	return true;
}

static int
nearest_descending_compare(const void *left, const void *right)
{
	int a = *(const int *)left;
	int b = *(const int *)right;

	return a < b ? 1 : a > b ? -1 : 0;
}

static bool
nearest_filter(const struct yt_nearest_state *state, bool member)
{
	float klass = state->port.commodity_class;
	float owner = state->port.owner;
	bool accepted = false;

	if (state->selector >= 1 && state->selector <= 3) {
		accepted = state->direction == 'S'
		    ? klass == (float)state->selector
		    : state->direction == 'B'
		    && klass != (float)state->selector;
	}
	else if (state->selector == 4)
		accepted = true;
	else if (state->selector == 5)
		accepted = owner > 0.0f && owner != state->actor_number
		    && member;
	else if (state->selector == 6)
		accepted = owner == state->actor_number;
	else if (state->selector == 7)
		accepted = owner > 0.0f && owner != state->actor_number
		    && (state->current_team == 0.0f
		    || (state->current_team > 0.0f && !member));
	else if (state->selector == 8)
		accepted = owner == 0.0f;
	return accepted && klass != 0.0f;
}

static bool
nearest_price_cell(float price, char result[4])
{
	char number[64];
	char source[65];
	size_t length;
	size_t amount;
	size_t padding;
	int rendered = qb_str_single(number, sizeof(number), price);

	if (rendered < 0)
		return false;
	source[0] = ' ';
	memcpy(source + 1, number, (size_t)rendered);
	length = (size_t)rendered + 1U;
	amount = length < 3U ? length : 3U;
	padding = 3U - amount;
	memset(result, ' ', padding);
	memcpy(result + padding, source + length - amount, amount);
	result[3] = '\0';
	return true;
}

static bool
nearest_stock_cell(const struct yt_nearest_market *market, uint8_t result[7],
    struct yt_error *error)
{
	char number[64];
	char source[96];
	float total;
	float scaled;
	float rounded;
	int length;
	size_t source_length;

	if (!nearest_add(market->stock[0], market->stock[1], &total, error,
	    "nearest stock total one")
	    || !nearest_add(total, market->stock[2], &total, error,
	    "nearest stock total two")
	    || !nearest_div(total, 10000.0f, &scaled, error,
	    "nearest stock scale")
	    || !nearest_add(scaled, 0.5f, &rounded, error,
	    "nearest stock rounding"))
		return false;
	rounded = floorf(rounded);
	if (!nearest_single(rounded, &rounded, error, "nearest stock INT"))
		return false;
	length = qb_str_double(number, sizeof(number), (double)rounded);
	if (length < 0 || snprintf(source, sizeof(source), "      %sK  ",
	    number) < 0)
		return nearest_error(error, "nearest stock formatting");
	source_length = strlen(source);
	if (source_length < 7U)
		return nearest_error(error, "nearest stock width");
	memcpy(result, source + source_length - 7U, 7U);
	return true;
}

static bool
nearest_page_count(struct yt_nearest_state *state, float amount,
    struct yt_error *error)
{
	if (!nearest_add(state->page_count, amount, &state->page_count, error,
	    "nearest pager count"))
		return false;
	if (qb_mbf32_encode(state->page_count, state->page_count_raw)
	    == QB_MBF_OVERFLOW)
		return nearest_error(error, "nearest pager count raw");
	return true;
}

static bool
nearest_page(struct yt_nearest_state *state,
    const struct yt_nearest_ops *ops, void *context, bool *stop,
    struct yt_error *error)
{
	static const uint8_t prompt[] =
	    "More? [Y]es [N]o [+] Continuous [Y] ";
	uint8_t key;
	bool available;

	*stop = false;
	state->page_count = 0.0f;
	memcpy(state->page_count_raw, "\x00\x00\x30\x00", 4U);
	state->style.foreground = 3.0f;
	if (!nearest_present(state, ops, context, YT_NEAREST_PAGER_PROMPT,
	    YT_NEAREST_PRESENT_BOLD_RAW, prompt, sizeof(prompt) - 1U, error))
		return false;
	for (;;) {
		available = false;
		key = 0U;
		if (!ops->input(context, &key, &available, error))
			return false;
		if (!available)
			continue;
		if (key == '\r')
			key = 'Y';
		ops->uppercase(context, &key, 1U);
		if (key != 'Y' && key != 'N' && key != '+')
			continue;
		if (!nearest_present(state, ops, context, YT_NEAREST_PAGER_ECHO,
		    YT_NEAREST_PRESENT_LINE, &key, 1U, error))
			return false;
		if (key == '+')
			state->continuous = true;
		*stop = key == 'N';
		return true;
	}
}

bool
yt_nearest_run(struct yt_nearest_state *state,
    const struct yt_nearest_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t scanning[] = "Scanning Starmap Database...";
	static const uint8_t instruction[] =
	    "Owned ports show the name of the owner preceeded by a \">\".";
	static const uint8_t earth[] = "** Earth **";
	bool visited[3001] = {false};
	int current_layer[3001];
	int next_layer[3001];
	size_t current_count = 0U;
	struct yt_record raw;
	bool first_sector = true;

	if (state == NULL || ops == NULL || ops->read_record == NULL
	    || ops->observe_day == NULL || ops->observe_timer == NULL
	    || ops->present == NULL || ops->input == NULL
	    || ops->uppercase == NULL
	    || state->selector < 1 || state->selector > 8
	    || (state->selector <= 3 && state->direction != 'B'
	    && state->direction != 'S'))
		return startup_configuration_error(error, YT_INVALID,
		    "nearest arguments");
	state->current_record_expression = 0.0f;
	state->current_team = 0.0f;
	state->start_sector_raw = 0.0f;
	state->display_sector = 0.0f;
	state->timer_seconds = 0.0f;
	state->page_count = 4.0f;
	if (qb_mbf32_encode(4.0f, state->page_count_raw) == QB_MBF_OVERFLOW)
		return nearest_error(error, "nearest initial pager count");
	state->current_sector = 0;
	state->distance = 0;
	state->rows = 0U;
	state->outputs = 0U;
	state->reads = 0U;
	state->day_observations = 0U;
	state->timer_observations = 0U;
	state->roster_comparisons = 0U;
	state->continuous = false;
	state->stopped = false;
	state->complete = false;
	state->result = YT_NEAREST_INCOMPLETE;

	if (!nearest_single(state->actor_number, &state->actor_number, error,
	    "nearest actor number")
	    || !nearest_present(state, ops, context, YT_NEAREST_ENTRY_BLANK,
	    YT_NEAREST_PRESENT_LINE, NULL, 0U, error))
		return false;
	state->style.foreground = 3.0f;
	if (!nearest_present(state, ops, context, YT_NEAREST_SCANNING,
	    YT_NEAREST_PRESENT_BOLD_LINE, scanning, sizeof(scanning) - 1U,
	    error)
	    || !nearest_present(state, ops, context, YT_NEAREST_SCAN_BLANK,
	    YT_NEAREST_PRESENT_LINE, NULL, 0U, error))
		return false;
	state->style.foreground = 7.0f;
	if (!nearest_present(state, ops, context, YT_NEAREST_OWNER_INSTRUCTION,
	    YT_NEAREST_PRESENT_BOLD_LINE, instruction,
	    sizeof(instruction) - 1U, error)
	    || !nearest_present(state, ops, context, YT_NEAREST_OWNER_BLANK,
	    YT_NEAREST_PRESENT_LINE, NULL, 0U, error))
		return false;

	if (!nearest_read(state, ops, context, YT_NEAREST_FIELD_PLAYER,
	    state->actor_number, &raw, error))
		return false;
	yt_player_decode(&state->player, &raw);
	state->current_team = state->player.team;
	state->start_sector_raw = state->player.sector;
	if (!nearest_cint(state, state->start_sector_raw,
	    &state->current_sector, error, "nearest start-sector CINT"))
		return false;
	if (state->current_sector < 0 || state->current_sector > 3000)
		return nearest_error(error, "nearest start workspace");
	if (state->start_sector_raw != 0.0f) {
		visited[state->current_sector] = true;
		current_layer[current_count++] = state->current_sector;
	}

	while (current_count != 0U) {
		size_t layer_index;
		size_t next_count = 0U;
		bool heading_emitted = false;

		for (layer_index = 0U; layer_index < current_count; ++layer_index) {
			int sector_number = current_layer[layer_index];
			float sector_operand = first_sector
			    ? state->start_sector_raw : (float)sector_number;
			float expression;
			float raw_port;
			size_t slot;

			first_sector = false;
			state->current_sector = sector_number;
			state->display_sector = sector_operand;
			if (!nearest_add(state->sector_record_offset, sector_operand,
			    &expression, error, "nearest sector record expression")
			    || !nearest_read(state, ops, context,
			    YT_NEAREST_FIELD_SECTOR, expression, &raw, error))
				return false;
			yt_sector_decode(&state->sector, &raw);
			for (slot = 0U; slot < 6U; ++slot) {
				int target;
				float warp = state->sector.warps[slot];

				if (warp == 0.0f)
					continue;
				if (!nearest_cint(state, warp, &target, error,
				    "nearest warp CINT"))
					return false;
				if (target < 0 || target > 3000)
					return nearest_error(error,
					    "nearest warp workspace");
				if (visited[target])
					continue;
				visited[target] = true;
				next_layer[next_count++] = target;
			}
			raw_port = state->sector.port;
			if (raw_port == 0.0f)
				continue;
			if (!ops->observe_day(context, &state->current_day, error))
				return false;
			++state->day_observations;
			if (!nearest_single(state->current_day, &state->current_day,
			    error, "nearest current day")
			    || !nearest_add(state->port_record_offset, raw_port,
			    &expression, error, "nearest port record expression")
			    || !nearest_read(state, ops, context,
			    YT_NEAREST_FIELD_PORT, expression, &raw, error))
				return false;
			yt_port_decode(&state->port, &raw);
			{
				bool member = false;

				if (state->current_team != 0.0f) {
					for (slot = 0U; slot < 4U; ++slot) {
						++state->roster_comparisons;
						if (state->cached_roster[slot]
						    == state->port.owner)
							member = true;
					}
				}
				if (!nearest_filter(state, member))
					continue;
			}
			nearest_market_copy(&state->market, &state->port);
			if (!ops->observe_timer(context, &state->timer_seconds, error))
				return false;
			++state->timer_observations;
			if (!nearest_single(state->timer_seconds,
			    &state->timer_seconds, error, "nearest TIMER")
			    || !yt_nearest_market_project(&state->market, &state->port,
			    state->base_price, state->current_day,
			    state->timer_seconds, error))
				return false;

			if (!heading_emitted) {
				char number[64];
				uint8_t heading[96];
				int length = qb_str_single(number, sizeof(number),
				    (float)state->distance);

				if (length < 0 || 9U + (size_t)length > sizeof(heading))
					return nearest_error(error,
					    "nearest distance formatting");
				memcpy(heading, "Distance:", 9U);
				memcpy(heading + 9U, number, (size_t)length);
				state->style.foreground = 1.0f;
				if (!nearest_present(state, ops, context,
				    YT_NEAREST_DISTANCE, YT_NEAREST_PRESENT_BOLD_LINE,
				    heading, 9U + (size_t)length, error)
				    || !nearest_page_count(state, 1.0f, error))
					return false;
				heading_emitted = true;
			}
			{
				uint8_t name[43];
				uint8_t sector_cell[13];
				uint8_t ore[13];
				uint8_t organics[13];
				uint8_t equipment[11];
				uint8_t stock[7];
				char number[64];
				char price[4];
				size_t name_length;
				int rendered;
				int owner_record;
				bool stop;

				rendered = qb_str_single(number, sizeof(number),
				    state->display_sector);
				if (rendered < 0)
					return nearest_error(error,
					    "nearest sector formatting");
				memcpy(sector_cell, "Sector:", 7U);
				memset(sector_cell + 7U, ' ', 6U);
				memcpy(sector_cell + 7U, number,
				    (size_t)rendered < 6U ? (size_t)rendered : 6U);
				if (!nearest_price_cell(state->market.price[0], price))
					return nearest_error(error,
					    "nearest ore formatting");
				(void)snprintf((char *)ore, sizeof(ore),
				    " Ore @%c%s  ", state->port.commodity_class
				    == 3.0f ? 'S' : 'B', price);
				if (!nearest_price_cell(state->market.price[1], price))
					return nearest_error(error,
					    "nearest organics formatting");
				(void)snprintf((char *)organics,
				    sizeof(organics), " Org @%c%s  ",
				    state->port.commodity_class == 2.0f ? 'S' : 'B',
				    price);
				if (!nearest_price_cell(state->market.price[2], price))
					return nearest_error(error,
					    "nearest equipment formatting");
				(void)snprintf((char *)equipment,
				    sizeof(equipment), " Equ @%c%s",
				    state->port.commodity_class == 1.0f ? 'S' : 'B',
				    price);
				if (!nearest_stock_cell(&state->market, stock, error)
				    || !nearest_cint(state, state->port.name_length,
				    &rendered, error, "nearest name-length CINT"))
					return false;
				if (rendered < 0)
					return nearest_error(error,
					    "nearest name LEFT$ length");
				name_length = (size_t)rendered;
				if (name_length > YT_TEXT_FIELD_SIZE)
					name_length = YT_TEXT_FIELD_SIZE;
				memcpy(name, state->port.record.bytes, name_length);
				if (state->display_sector == 1.0f) {
					name_length = sizeof(earth) - 1U;
					memcpy(name, earth, name_length);
					memset(ore, 0, sizeof(ore));
					memset(organics, 0, sizeof(organics));
					memset(equipment, 0, sizeof(equipment));
					memset(stock, 0, sizeof(stock));
				}
				if (state->port.owner != 0.0f)
					state->style.bold = 1.0f;
				state->style.foreground = 2.0f;
				if (!nearest_present(state, ops, context,
				    YT_NEAREST_SECTOR, YT_NEAREST_PRESENT_RAW,
				    sector_cell, sizeof(sector_cell), error))
					return false;
				state->style.foreground =
				    state->port.commodity_class == 3.0f ? 7.0f : 6.0f;
				if (!nearest_present(state, ops, context, YT_NEAREST_ORE,
				    YT_NEAREST_PRESENT_BOLD_RAW, ore,
				    state->display_sector == 1.0f
				    ? 0U : sizeof(ore) - 1U,
				    error))
					return false;
				state->style.foreground =
				    state->port.commodity_class == 2.0f ? 7.0f : 6.0f;
				if (!nearest_present(state, ops, context,
				    YT_NEAREST_ORGANICS, YT_NEAREST_PRESENT_BOLD_RAW,
				    organics, state->display_sector == 1.0f
				    ? 0U : sizeof(organics) - 1U, error))
					return false;
				state->style.foreground =
				    state->port.commodity_class == 1.0f ? 7.0f : 6.0f;
				if (!nearest_present(state, ops, context,
				    YT_NEAREST_EQUIPMENT, YT_NEAREST_PRESENT_BOLD_RAW,
				    equipment, state->display_sector == 1.0f
				    ? 0U : sizeof(equipment) - 1U, error))
					return false;
				state->style.foreground = 2.0f;
				if (!nearest_present(state, ops, context, YT_NEAREST_STOCK,
				    YT_NEAREST_PRESENT_RAW, stock,
				    state->display_sector == 1.0f ? 0U : sizeof(stock),
				    error))
					return false;
				state->style.foreground = 3.0f;
				if (!nearest_cint(state, state->port.owner, &owner_record,
				    error, "nearest owner CINT"))
					return false;
				if (state->display_sector != 1.0f
				    && owner_record != 0) {
					if (!nearest_read(state, ops, context,
					    YT_NEAREST_FIELD_OWNER, state->port.owner,
					    &raw, error))
						return false;
					yt_player_decode(&state->owner, &raw);
					memmove(name + 2U, state->owner.record.bytes,
					    YT_TEXT_FIELD_SIZE);
					memcpy(name, "> ", 2U);
					name_length = qb_title_case_n(name,
					    YT_TEXT_FIELD_SIZE + 2U);
					if (name_length > 26U)
						name_length = 26U;
					if (state->port.owner == state->actor_number)
						state->style.foreground = 5.0f;
				}
				if (state->display_sector == 1.0f) {
					state->style.foreground = 3.0f;
					state->style.blink = 1.0f;
				}
				if (!nearest_present(state, ops, context, YT_NEAREST_NAME,
				    YT_NEAREST_PRESENT_BOLD_LINE, name, name_length,
				    error))
					return false;
				++state->rows;
				if (!nearest_page_count(state, 1.0f, error))
					return false;
				if (state->continuous) {
					state->page_count = 0.0f;
					memset(state->page_count_raw, 0, 4U);
				}
				if (state->page_count > 22.0f) {
					if (!nearest_page(state, ops, context, &stop,
					    error))
						return false;
					if (stop) {
						if (!nearest_present(state, ops, context,
						    YT_NEAREST_FINAL_BLANK,
						    YT_NEAREST_PRESENT_LINE, NULL, 0U,
						    error))
							return false;
						state->stopped = true;
						state->complete = true;
						state->result = YT_NEAREST_PAGE_STOP;
						return true;
					}
				}
			}
		}
		qsort(next_layer, next_count, sizeof(next_layer[0]),
		    nearest_descending_compare);
		memcpy(current_layer, next_layer,
		    next_count * sizeof(current_layer[0]));
		current_count = next_count;
		++state->distance;
	}
	if (!nearest_present(state, ops, context, YT_NEAREST_FINAL_BLANK,
	    YT_NEAREST_PRESENT_LINE, NULL, 0U, error))
		return false;
	state->complete = true;
	state->result = YT_NEAREST_COMPLETE;
	return true;
}

static bool
profit_present(struct yt_profit_state *state, const struct yt_profit_ops *ops,
    void *context, enum yt_profit_output_kind kind,
    enum yt_profit_present_mode mode, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	if (!ops->present(context, kind, mode, text, length, &state->style,
	    error))
		return false;
	++state->outputs;
	return true;
}

static bool
profit_checkpoint(const struct yt_profit_ops *ops, void *context,
    enum yt_profit_checkpoint checkpoint, struct yt_error *error)
{
	return ops->checkpoint(context, checkpoint, error);
}

static bool
profit_read(struct yt_profit_state *state, const struct yt_profit_ops *ops,
    void *context, enum yt_profit_field_kind kind, float expression,
    struct yt_record *record, struct yt_error *error)
{
	uint32_t physical;

	if (!nearest_single(expression, &state->current_record_expression,
	    error, "profit record expression"))
		return false;
	physical = qb_brun_random_record_number(state->current_record_expression);
	if (!ops->read_record(context, kind, state->current_record_expression,
	    physical, record, error))
		return false;
	state->field = *record;
	state->field_kind = kind;
	state->field_record = physical;
	state->field_valid = true;
	++state->reads;
	return true;
}

static bool
profit_cint(const struct yt_profit_state *state, float value, int *result,
    struct yt_error *error, const char *operation)
{
	bool overflow;
	int32_t converted = qb_cint_mode((double)value,
	    state->conversion_mode, &overflow);

	if (overflow)
		return nearest_error(error, operation);
	*result = (int)converted;
	return true;
}

static bool
profit_raw_memory_boundary(struct yt_profit_state *state,
    struct yt_error *error)
{
	state->result = YT_PROFIT_UNRESOLVED_RAW_MEMORY;
	if (error != NULL) {
		error->status = YT_INVALID;
		error->system_error = 0;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    "profit unresolved price-array index");
		error->path[0] = '\0';
	}
	return false;
}

static bool
profit_coerce_warps(const struct yt_profit_state *state,
    const struct yt_sector *sector, int warps[6], struct yt_error *error)
{
	size_t slot;

	for (slot = 0U; slot < 6U; ++slot) {
		if (!profit_cint(state, sector->warps[slot], &warps[slot], error,
		    "profit warp target CINT"))
			return false;
	}
	return true;
}

static bool
profit_project(struct yt_profit_state *state, const struct yt_profit_ops *ops,
    void *context, const struct yt_port *port,
    struct yt_nearest_market *market, float prices[4],
    struct yt_error *error)
{
	if (!ops->observe_day(context, &state->current_day, error))
		return false;
	++state->day_observations;
	if (!nearest_single(state->current_day, &state->current_day, error,
	    "profit current day")
	    || !ops->observe_timer(context, &state->timer_seconds, error))
		return false;
	++state->timer_observations;
	if (!nearest_single(state->timer_seconds, &state->timer_seconds, error,
	    "profit TIMER")
	    || !yt_nearest_market_project(market, port, state->base_price,
	    state->current_day, state->timer_seconds, error))
		return false;
	prices[0] = 0.0f;
	prices[1] = market->price[0];
	prices[2] = market->price[1];
	prices[3] = market->price[2];
	return true;
}

static void
profit_pair_color(struct yt_profit_state *state, float source, float target)
{
	if ((source == 1.0f && target == 2.0f)
	    || (source == 2.0f && target == 1.0f))
		state->style.foreground = 3.0f;
	else if ((source == 1.0f && target == 3.0f)
	    || (source == 3.0f && target == 1.0f))
		state->style.foreground = 2.0f;
	else if ((source == 2.0f && target == 3.0f)
	    || (source == 3.0f && target == 2.0f))
		state->style.foreground = 1.0f;
}

static bool
profit_right_four(uint8_t result[4], float value, bool integer)
{
	char number[64];
	uint8_t source[68];
	int rendered = integer
	    ? qb_str_integer(number, sizeof(number), (int16_t)(int)value)
	    : qb_str_single(number, sizeof(number), value);
	size_t length;
	size_t amount;

	if (rendered < 0)
		return false;
	source[0] = ' ';
	source[1] = ' ';
	memcpy(source + 2U, number, (size_t)rendered);
	length = 2U + (size_t)rendered;
	amount = length < 4U ? length : 4U;
	memset(result, ' ', 4U - amount);
	memcpy(result + 4U - amount, source + length - amount, amount);
	return true;
}

static bool
profit_compose_row(struct yt_profit_state *state,
    const struct yt_profit_ops *ops, void *context, float source_number,
    int target_number, const struct yt_port *source_port,
    const struct yt_port *target_port, const float source_price[4],
    const float target_price[4], uint8_t row[36],
    struct yt_error *error)
{
	static const uint8_t arrows[3][7] = {
		{'E', 'q', 'u', ' ', '-', '>', ' '},
		{'O', 'r', 'g', ' ', '-', '>', ' '},
		{'O', 'r', 'e', ' ', '-', '>', ' '},
	};
	static const uint8_t labels[3][3] = {
		{'E', 'q', 'u'}, {'O', 'r', 'g'}, {'O', 'r', 'e'},
	};
	float source_subscript;
	float target_subscript;
	float source_leg;
	float target_leg;
	float profit;
	int source_index;
	int target_index;
	int source_text = source_port->commodity_class == 1.0f ? 0
	    : source_port->commodity_class == 2.0f ? 1 : 2;
	int target_text = target_port->commodity_class == 1.0f ? 0
	    : target_port->commodity_class == 2.0f ? 1 : 2;
	char number[64];
	uint8_t field[68];
	int rendered;
	size_t length = 0U;

	if (!nearest_sub(4.0f, source_port->commodity_class,
	    &source_subscript, error, "profit source class subtraction")
	    || !profit_cint(state, source_subscript, &source_index, error,
	    "profit source class CINT")
	    || !nearest_sub(4.0f, target_port->commodity_class,
	    &target_subscript, error, "profit target class subtraction")
	    || !profit_cint(state, target_subscript, &target_index, error,
	    "profit target class CINT"))
		return false;
	if (source_index < 0 || source_index > 3
	    || target_index < 0 || target_index > 3)
		return profit_raw_memory_boundary(state, error);
	if (!nearest_sub(source_price[source_index],
	    target_price[source_index], &source_leg, error,
	    "profit source leg")
	    || !nearest_single(fabsf(source_leg), &source_leg, error,
	    "profit source ABS")
	    || !nearest_sub(target_price[target_index],
	    source_price[target_index], &target_leg, error,
	    "profit target leg")
	    || !nearest_single(fabsf(target_leg), &target_leg, error,
	    "profit target ABS")
	    || !nearest_add(source_leg, target_leg, &profit, error,
	    "profit spread"))
		return false;
	profit_pair_color(state, source_port->commodity_class,
	    target_port->commodity_class);
	if (!profit_checkpoint(ops, context, YT_PROFIT_ROW_CONSTRUCTION, error)
	    || !profit_right_four(row + length, source_number, false))
		return false;
	length += 4U;
	row[length++] = ',';
	if (!profit_right_four(row + length, (float)target_number, true))
		return nearest_error(error, "profit target formatting");
	length += 4U;
	row[length++] = ' ';
	memcpy(row + length, arrows[source_text], 7U);
	length += 7U;
	memcpy(row + length, labels[target_text], 3U);
	length += 3U;
	memcpy(row + length, " @ Profit of", 12U);
	length += 12U;
	rendered = qb_str_single(number, sizeof(number), profit);
	if (rendered < 0 || (size_t)rendered + 3U > sizeof(field))
		return nearest_error(error, "profit value formatting");
	memcpy(field, number, (size_t)rendered);
	memset(field + (size_t)rendered, ' ', 3U);
	memcpy(row + length, field, 4U);
	length += 4U;
	if (length != 36U)
		return nearest_error(error, "profit row width");
	return true;
}

static bool
profit_page(struct yt_profit_state *state, const struct yt_profit_ops *ops,
    void *context, bool *keep_going, struct yt_error *error)
{
	static const uint8_t prompt[] = "More? [Y/n] ";
	uint8_t response[8];
	size_t length;
	bool available;

	for (;;) {
		if (!profit_present(state, ops, context, YT_PROFIT_PAGER_PROMPT,
		    YT_PROFIT_PRESENT_RAW, prompt, sizeof(prompt) - 1U, error))
			return false;
		++state->pager_prompts;
		for (;;) {
			length = 0U;
			available = false;
			if (!ops->input(context, response, sizeof(response), &length,
			    &available, error))
				return false;
			if (available)
				break;
		}
		if (length == 1U && response[0] == '\r')
			response[0] = 'Y';
		ops->uppercase(context, response, length);
		if (!profit_present(state, ops, context, YT_PROFIT_PAGER_ECHO,
		    YT_PROFIT_PRESENT_LINE, response, length, error))
			return false;
		if (length == 1U && (response[0] == 'Y' || response[0] == 'N')) {
			*keep_going = response[0] == 'Y';
			return true;
		}
	}
}

static bool
profit_emit_pair(struct yt_profit_state *state,
    const struct yt_profit_ops *ops, void *context, float source_number,
    int target_number, const struct yt_port *source_port,
    const struct yt_port *target_port, const float source_price[4],
    const float target_price[4], bool *keep_going, struct yt_error *error)
{
	static const uint8_t separator[] = {' ', 0xba, ' '};
	uint8_t row[36];
	int count;

	*keep_going = true;
	if (state->global) {
		if (!nearest_add(state->result_count, 1.0f,
		    &state->result_count, error, "profit result counter")
		    || qb_mbf32_encode(state->result_count,
		    state->result_count_raw) == QB_MBF_OVERFLOW)
			return nearest_error(error, "profit result counter raw");
	}
	if (!profit_compose_row(state, ops, context, source_number,
	    target_number, source_port, target_port, source_price, target_price,
	    row, error))
		return false;
	if (!state->global) {
		if (!profit_present(state, ops, context, YT_PROFIT_ROW,
		    YT_PROFIT_PRESENT_BOLD_LINE, row, sizeof(row), error))
			return false;
		++state->rows;
		return true;
	}
	if (!profit_present(state, ops, context, YT_PROFIT_ROW,
	    YT_PROFIT_PRESENT_BOLD_RAW, row, sizeof(row), error))
		return false;
	++state->rows;
	state->style.foreground = 6.0f;
	if (!profit_cint(state, state->result_count, &count, error,
	    "profit parity CINT"))
		return false;
	if ((count & 1) != 0) {
		if (!profit_present(state, ops, context,
		    YT_PROFIT_COLUMN_SEPARATOR, YT_PROFIT_PRESENT_BOLD_RAW,
		    separator, sizeof(separator), error))
			return false;
	}
	else if (!profit_present(state, ops, context, YT_PROFIT_ROW_END,
	    YT_PROFIT_PRESENT_LINE, NULL, 0U, error))
		return false;
	if (!profit_cint(state, state->result_count, &count, error,
	    "profit pager CINT"))
		return false;
	if (count % 44 == 0 && !profit_page(state, ops, context,
	    keep_going, error))
		return false;
	return true;
}

bool
yt_profit_run(struct yt_profit_state *state, const struct yt_profit_ops *ops,
    void *context, struct yt_error *error)
{
	static const uint8_t title[] =
	    "Profits of a two way trade to ports in adjacent sectors.";
	static const uint8_t no_port[] = "NO trading port in your sector!";
	static const uint8_t no_results[] =
	    "No ports you can trade with in adjacent sectors!";
	static const uint8_t end_banner[] = " *-[ End of List ]-*";
	float source;

	if (state == NULL || ops == NULL || ops->read_record == NULL
	    || ops->observe_day == NULL || ops->observe_timer == NULL
	    || ops->present == NULL || ops->input == NULL
	    || ops->uppercase == NULL || ops->checkpoint == NULL)
		return startup_configuration_error(error, YT_INVALID,
		    "profit arguments");
	state->current_record_expression = 0.0f;
	state->timer_seconds = 0.0f;
	state->maximum_sector = 0.0f;
	state->result_count = 0.0f;
	memcpy(state->initial_result_raw,
	    state->global ? "\x00\x00\x04\x00" : "\x00\x00\x20\x00", 4U);
	memset(state->result_count_raw, 0, sizeof(state->result_count_raw));
	if (state->global)
		memcpy(state->result_count_raw, state->initial_result_raw,
		    sizeof(state->result_count_raw));
	state->rows = 0U;
	state->outputs = 0U;
	state->reads = 0U;
	state->day_observations = 0U;
	state->timer_observations = 0U;
	state->pager_prompts = 0U;
	state->stopped = false;
	state->complete = false;
	state->result = YT_PROFIT_INCOMPLETE;

	if (!state->global)
		state->style.foreground = 7.0f;
	if (!profit_present(state, ops, context, YT_PROFIT_LEADING_BLANK,
	    YT_PROFIT_PRESENT_LINE, NULL, 0U, error))
		return false;
	if (!state->global
	    && (!profit_present(state, ops, context, YT_PROFIT_TITLE,
	    YT_PROFIT_PRESENT_BOLD_LINE, title, sizeof(title) - 1U, error)
	    || !profit_present(state, ops, context, YT_PROFIT_TITLE_BLANK,
	    YT_PROFIT_PRESENT_LINE, NULL, 0U, error)))
		return false;

	if (state->global) {
		if (!nearest_sub(state->port_record_offset,
		    state->sector_record_offset, &state->maximum_sector, error,
		    "profit maximum sector")
		    || !profit_checkpoint(ops, context,
		    YT_PROFIT_MAXIMUM_READY, error))
			return false;
		source = 2.0f;
	}
	else {
		struct yt_record raw;
		struct yt_sector sector;
		struct yt_port source_port;
		struct yt_nearest_market source_market;
		float source_prices[4];
		float display_source;
		int warps[6];
		size_t slot;

		if (!nearest_single(state->current_sector_record,
		    &state->current_sector_record, error,
		    "profit current sector record")
		    || !profit_read(state, ops, context, YT_PROFIT_FIELD_SECTOR,
		    state->current_sector_record, &raw, error))
			return false;
		yt_sector_decode(&sector, &raw);
		if (sector.port == 0.0f || sector.port == 1.0f) {
			if (!profit_present(state, ops, context,
			    YT_PROFIT_NO_CURRENT_PORT,
			    YT_PROFIT_PRESENT_BOLD_LINE, no_port,
			    sizeof(no_port) - 1U, error))
				return false;
			state->complete = true;
			state->result = YT_PROFIT_NO_CURRENT_PORT_RESULT;
			return true;
		}
		if (!profit_coerce_warps(state, &sector, warps, error)
		    || !nearest_add(state->port_record_offset, sector.port,
		    &source, error, "profit current port record")
		    || !profit_read(state, ops, context, YT_PROFIT_FIELD_PORT,
		    source, &raw, error))
			return false;
		yt_port_decode(&source_port, &raw);
		if (!profit_project(state, ops, context, &source_port,
		    &source_market, source_prices, error)
		    || !profit_checkpoint(ops, context,
		    YT_PROFIT_SOURCE_ARROW_READY, error))
			return false;
		for (slot = 0U; slot < 6U; ++slot) {
			struct yt_sector target_sector;
			struct yt_port target_port;
			struct yt_nearest_market target_market;
			float target_prices[4];
			int target;
			bool keep_going;

			target = warps[slot];
			if (target <= 1)
				continue;
			if (!nearest_add(state->sector_record_offset, (float)target,
			    &source, error, "profit target sector record")
			    || !profit_read(state, ops, context,
			    YT_PROFIT_FIELD_SECTOR, source, &raw, error))
				return false;
			yt_sector_decode(&target_sector, &raw);
			if (target_sector.port == 0.0f)
				continue;
			if (!nearest_add(state->port_record_offset,
			    target_sector.port, &source, error,
			    "profit target port record")
			    || !profit_read(state, ops, context,
			    YT_PROFIT_FIELD_PORT, source, &raw, error))
				return false;
			yt_port_decode(&target_port, &raw);
			if (target_port.commodity_class
			    == source_port.commodity_class)
				continue;
			if (!profit_project(state, ops, context, &target_port,
			    &target_market, target_prices, error)
			    || !nearest_sub(state->current_sector_record,
			    state->sector_record_offset, &display_source, error,
			    "profit display sector")
			    || !profit_emit_pair(state, ops, context, display_source,
			    target, &source_port, &target_port, source_prices,
			    target_prices, &keep_going, error))
				return false;
		}
		if (state->rows == 0U) {
			if (!profit_present(state, ops, context, YT_PROFIT_NO_RESULTS,
			    YT_PROFIT_PRESENT_BOLD_LINE, no_results,
			    sizeof(no_results) - 1U, error))
				return false;
			state->result = YT_PROFIT_NO_RESULTS_RESULT;
		}
		else
			state->result = YT_PROFIT_COMPLETE;
		state->complete = true;
		return true;
	}

	while (source <= state->maximum_sector) {
		struct yt_record raw;
		struct yt_sector sector;
		struct yt_port source_port;
		struct yt_nearest_market source_market;
		float source_prices[4];
		float expression;
		int warps[6];
		size_t slot;

		if (!nearest_add(state->sector_record_offset, source,
		    &expression, error, "profit source sector record")
		    || !profit_read(state, ops, context, YT_PROFIT_FIELD_SECTOR,
		    expression, &raw, error))
			return false;
		yt_sector_decode(&sector, &raw);
		if (!profit_coerce_warps(state, &sector, warps, error))
			return false;
		if (sector.port > 0.0f) {
			if (!nearest_add(state->port_record_offset, sector.port,
			    &expression, error, "profit source port record")
			    || !profit_read(state, ops, context,
			    YT_PROFIT_FIELD_PORT, expression, &raw, error))
				return false;
			yt_port_decode(&source_port, &raw);
			if (!profit_project(state, ops, context, &source_port,
			    &source_market, source_prices, error)
			    || !profit_checkpoint(ops, context,
			    YT_PROFIT_SOURCE_ARROW_READY, error))
				return false;
			for (slot = 0U; slot < 6U; ++slot) {
				struct yt_sector target_sector;
				struct yt_port target_port;
				struct yt_nearest_market target_market;
				float target_prices[4];
				int target;
				bool keep_going;

				target = warps[slot];
				if (target <= 1)
					continue;
				if (!nearest_add(state->sector_record_offset,
				    (float)target, &expression, error,
				    "profit target sector record")
				    || !profit_read(state, ops, context,
				    YT_PROFIT_FIELD_SECTOR, expression, &raw, error))
					return false;
				yt_sector_decode(&target_sector, &raw);
				if (target_sector.port == 0.0f
				    || (float)target <= source)
					continue;
				if (!nearest_add(state->port_record_offset,
				    target_sector.port, &expression, error,
				    "profit target port record")
				    || !profit_read(state, ops, context,
				    YT_PROFIT_FIELD_PORT, expression, &raw, error))
					return false;
				yt_port_decode(&target_port, &raw);
				if (target_port.commodity_class
				    == source_port.commodity_class)
					continue;
				if (!profit_project(state, ops, context, &target_port,
				    &target_market, target_prices, error)
				    || !profit_emit_pair(state, ops, context, source,
				    target, &source_port, &target_port, source_prices,
				    target_prices, &keep_going, error))
					return false;
				if (!keep_going) {
					state->stopped = true;
					state->complete = true;
					state->result = YT_PROFIT_STOPPED_BY_N;
					return true;
				}
			}
		}
		if (!nearest_add(source, 1.0f, &source, error,
		    "profit source increment"))
			return false;
	}
	state->style.foreground = 7.0f;
	if (!profit_present(state, ops, context, YT_PROFIT_END_BANNER,
	    YT_PROFIT_PRESENT_BOLD_LINE, end_banner,
	    sizeof(end_banner) - 1U, error))
		return false;
	state->complete = true;
	state->result = YT_PROFIT_COMPLETE;
	return true;
}

static bool
current_player_hydration_fault(struct yt_error *error,
    enum yt_basic_fault_site site, const char *operation)
{
	if (error != NULL) {
		error->status = YT_RANGE;
		error->system_error = 0;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
		error->path[0] = '\0';
		(void)yt_error_attach_basic_fault_number(error, site, 6U);
	}
	return false;
}

static enum qb_mbf_status
current_player_add_single_raw(const uint8_t left[4], const uint8_t right[4],
    const uint8_t dirty_zero_source[4], uint8_t result[4])
{
	volatile float sum;
	enum qb_mbf_status status;

	if (right[3] == 0U) {
		memcpy(result, left, 4U);
		return QB_MBF_OK;
	}
	if (left[3] == 0U) {
		memcpy(result, right, 4U);
		return QB_MBF_OK;
	}
	sum = qb_mbf32_decode(left) + qb_mbf32_decode(right);
	status = qb_mbf32_encode(sum, result);
	if (status == QB_MBF_OVERFLOW || status == QB_MBF_DOMAIN)
		return status;
	if (status == QB_MBF_UNDERFLOW || sum == 0.0f) {
		memcpy(result, dirty_zero_source, 3U);
		result[3] = 0U;
	}
	return status;
}

bool
yt_current_player_hydrate(struct yt_player *player,
    const struct yt_player *fresh, int player_record,
    float sector_record_offset, bool anti_cloak_enabled,
    float *current_sector_record, struct yt_player_cache *player_cache,
    struct yt_error *error)
{
	uint8_t sector_record_offset_raw[4];
	uint8_t current_sector_raw[8] = {0};
	enum qb_mbf_status add_status;

	if (player == NULL || fresh == NULL || current_sector_record == NULL)
		return false;
	if (qb_mbf32_encode(sector_record_offset, sector_record_offset_raw)
	    != QB_MBF_OK)
		return false;

	player->record = fresh->record;
	player->sector = fresh->sector;
	player->fighters = fresh->fighters;
	add_status = current_player_add_single_raw(sector_record_offset_raw,
	    fresh->record.bytes + YT_F57, fresh->record.bytes + YT_F61,
	    current_sector_raw);
	if (add_status == QB_MBF_OVERFLOW || add_status == QB_MBF_DOMAIN)
		return current_player_hydration_fault(error,
		    YT_BASIC_FAULT_CURRENT_PLAYER_A41C_SECTOR_ADD,
		    "current-player A41C sector ADD_FLOAT");
	*current_sector_record = qb_mbf32_decode(current_sector_raw);
	player->turns = fresh->turns;
	player->credits = fresh->credits;
	player->danger_scanner = fresh->danger_scanner;
	player->missiles = fresh->missiles;
	player->mines = fresh->mines;
	player->team = fresh->team;
	player->holds = fresh->holds;
	player->ore = fresh->ore;
	player->organics = fresh->organics;
	player->equipment = fresh->equipment;
	player->plasma = fresh->plasma;
	player->score = fresh->score;
	player->ports_owned = fresh->ports_owned;
	player->ground_forces = fresh->ground_forces;
	player->cloak = fresh->cloak;
	if (!anti_cloak_enabled) {
		if (player_cache != NULL)
			(void)yt_player_cache_set_raw(player_cache, player_record,
			    YT_PLAYER_CACHE_CLOAK,
			    fresh->record.bytes + YT_F125);
	}
	player->shields = fresh->shields;
	return true;
}

void
yt_player_decode(struct yt_player *player, const struct yt_record *record)
{
	memset(player, 0, sizeof(*player));
	player->record = *record;
	yt_record_get_text(record, player->name, sizeof(player->name));
	player->last_active = yt_record_get_number(record, YT_F41);
	player->killed_by = yt_record_get_number(record, YT_F45);
	player->turns = yt_record_get_number(record, YT_F49);
	player->shields = yt_record_get_number(record, YT_F53);
	player->sector = yt_record_get_number(record, YT_F57);
	player->fighters = yt_record_get_number(record, YT_F61);
	player->holds = yt_record_get_number(record, YT_F65);
	player->ore = yt_record_get_number(record, YT_F69);
	player->organics = yt_record_get_number(record, YT_F73);
	player->equipment = yt_record_get_number(record, YT_F77);
	player->credits = yt_record_get_number(record, YT_F81);
	player->name_length = yt_record_get_number(record, YT_F85);
	player->team = yt_record_get_number(record, YT_F89);
	player->danger_scanner = yt_record_get_number(record, YT_F93);
	player->missiles = yt_record_get_number(record, YT_F97);
	player->lottery_plays = yt_record_get_number(record, YT_F105);
	player->score = yt_record_get_number(record, YT_F109);
	player->plasma = yt_record_get_number(record, YT_F113);
	player->ports_owned = yt_record_get_number(record, YT_F117);
	player->ground_forces = yt_record_get_number(record, YT_F121);
	player->cloak = yt_record_get_number(record, YT_F125);
	player->mines = yt_record_get_number(record, YT_F129);
}

void
yt_player_encode(struct yt_player *player)
{
	yt_record_set_text_if_changed(&player->record,
	    (const uint8_t *)player->name,
	    strlen(player->name));
	yt_record_set_number_if_changed(&player->record, YT_F41,
	    player->last_active);
	yt_record_set_number_if_changed(&player->record, YT_F45,
	    player->killed_by);
	yt_record_set_number_if_changed(&player->record, YT_F49, player->turns);
	yt_record_set_number_if_changed(&player->record, YT_F53, player->shields);
	yt_record_set_number_if_changed(&player->record, YT_F57, player->sector);
	yt_record_set_number_if_changed(&player->record, YT_F61, player->fighters);
	yt_record_set_number_if_changed(&player->record, YT_F65, player->holds);
	yt_record_set_number_if_changed(&player->record, YT_F69, player->ore);
	yt_record_set_number_if_changed(&player->record, YT_F73,
	    player->organics);
	yt_record_set_number_if_changed(&player->record, YT_F77,
	    player->equipment);
	yt_record_set_number_if_changed(&player->record, YT_F81, player->credits);
	yt_record_set_number_if_changed(&player->record, YT_F85,
	    player->name_length);
	yt_record_set_number_if_changed(&player->record, YT_F89, player->team);
	yt_record_set_number_if_changed(&player->record, YT_F93,
	    player->danger_scanner);
	yt_record_set_number_if_changed(&player->record, YT_F97,
	    player->missiles);
	yt_record_set_number_if_changed(&player->record, YT_F105,
	    player->lottery_plays);
	yt_record_set_number_if_changed(&player->record, YT_F109, player->score);
	yt_record_set_number_if_changed(&player->record, YT_F113, player->plasma);
	yt_record_set_number_if_changed(&player->record, YT_F117,
	    player->ports_owned);
	yt_record_set_number_if_changed(&player->record, YT_F121,
	    player->ground_forces);
	yt_record_set_number_if_changed(&player->record, YT_F125, player->cloak);
	yt_record_set_number_if_changed(&player->record, YT_F129, player->mines);
}

void
yt_sector_decode(struct yt_sector *sector, const struct yt_record *record)
{
	size_t index;

	memset(sector, 0, sizeof(*sector));
	sector->record = *record;
	for (index = 0; index < 6; ++index)
		sector->warps[index] = yt_record_get_number(record,
		    YT_F41 + index * 4U);
	sector->port = yt_record_get_number(record, YT_F65);
	sector->fighters = yt_record_get_number(record, YT_F81);
	sector->fighter_owner = yt_record_get_number(record, YT_F85);
	sector->planet = yt_record_get_number(record, YT_F93);
	sector->metadata = yt_record_get_number(record, YT_F105);
	sector->mines = yt_record_get_number(record, YT_F129);
}

void
yt_sector_encode(struct yt_sector *sector)
{
	size_t index;

	for (index = 0; index < 6; ++index)
		yt_record_set_number_if_changed(&sector->record,
		    YT_F41 + index * 4U,
		    sector->warps[index]);
	yt_record_set_number_if_changed(&sector->record, YT_F65, sector->port);
	yt_record_set_number_if_changed(&sector->record, YT_F81,
	    sector->fighters);
	yt_record_set_number_if_changed(&sector->record, YT_F85,
	    sector->fighter_owner);
	yt_record_set_number_if_changed(&sector->record, YT_F93, sector->planet);
	yt_record_set_number_if_changed(&sector->record, YT_F105,
	    sector->metadata);
	yt_record_set_number_if_changed(&sector->record, YT_F129, sector->mines);
}

void
yt_port_decode(struct yt_port *port, const struct yt_record *record)
{
	size_t index;

	memset(port, 0, sizeof(*port));
	port->record = *record;
	yt_record_get_text(record, port->name, sizeof(port->name));
	port->commodity_class = yt_record_get_number(record, YT_F41);
	port->last_day = yt_record_get_number(record, YT_F45);
	for (index = 0; index < 3; ++index) {
		port->stock[index] = yt_record_get_number(record, YT_F49 + index * 4U);
		port->production[index] =
		    yt_record_get_number(record, YT_F61 + index * 4U);
		port->factor[index] = yt_record_get_number(record, YT_F73 + index * 4U);
	}
	port->name_length = yt_record_get_number(record, YT_F85);
	port->treasury = yt_record_get_number(record, YT_F89);
	port->sector = yt_record_get_number(record, YT_F93);
	port->owner = yt_record_get_number(record, YT_F97);
	port->last_minute = yt_record_get_number(record, YT_F101);
}

void
yt_port_encode(struct yt_port *port)
{
	size_t index;

	yt_record_set_text_if_changed(&port->record, (const uint8_t *)port->name,
	    strlen(port->name));
	yt_record_set_number_if_changed(&port->record, YT_F41,
	    port->commodity_class);
	yt_record_set_number_if_changed(&port->record, YT_F45, port->last_day);
	for (index = 0; index < 3; ++index) {
		yt_record_set_number_if_changed(&port->record,
		    YT_F49 + index * 4U,
		    port->stock[index]);
		yt_record_set_number_if_changed(&port->record,
		    YT_F61 + index * 4U,
		    port->production[index]);
		yt_record_set_number_if_changed(&port->record,
		    YT_F73 + index * 4U,
		    port->factor[index]);
	}
	yt_record_set_number_if_changed(&port->record, YT_F85,
	    port->name_length);
	yt_record_set_number_if_changed(&port->record, YT_F89, port->treasury);
	yt_record_set_number_if_changed(&port->record, YT_F93, port->sector);
	yt_record_set_number_if_changed(&port->record, YT_F97, port->owner);
	yt_record_set_number_if_changed(&port->record, YT_F101,
	    port->last_minute);
}

void
yt_planet_decode(struct yt_planet *planet, const struct yt_record *record)
{
	size_t index;

	memset(planet, 0, sizeof(*planet));
	planet->record = *record;
	yt_record_get_text(record, planet->name, sizeof(planet->name));
	planet->last_day = yt_record_get_number(record, YT_F41);
	for (index = 0; index < 3; ++index) {
		planet->production[index] =
		    yt_record_get_number(record, YT_F45 + index * 4U);
		planet->stock[index] = yt_record_get_number(record, YT_F57 + index * 4U);
	}
	planet->missiles = yt_record_get_number(record, YT_F69);
	planet->owner = yt_record_get_number(record, YT_F73);
	planet->ground_forces = yt_record_get_number(record, YT_F77);
	planet->name_length = yt_record_get_number(record, YT_F85);
	planet->last_minute = yt_record_get_number(record, YT_F89);
	planet->plasma = yt_record_get_number(record, YT_F113);
	planet->bank = yt_record_get_number(record, YT_F117);
	planet->mines = yt_record_get_number(record, YT_F125);
	planet->fighters = yt_record_get_number(record, YT_F129);
}

void
yt_planet_encode(struct yt_planet *planet)
{
	size_t index;

	yt_record_set_text_if_changed(&planet->record,
	    (const uint8_t *)planet->name,
	    strlen(planet->name));
	yt_record_set_number_if_changed(&planet->record, YT_F41,
	    planet->last_day);
	for (index = 0; index < 3; ++index) {
		yt_record_set_number_if_changed(&planet->record,
		    YT_F45 + index * 4U,
		    planet->production[index]);
		yt_record_set_number_if_changed(&planet->record,
		    YT_F57 + index * 4U,
		    planet->stock[index]);
	}
	yt_record_set_number_if_changed(&planet->record, YT_F69,
	    planet->missiles);
	yt_record_set_number_if_changed(&planet->record, YT_F73, planet->owner);
	yt_record_set_number_if_changed(&planet->record, YT_F77,
	    planet->ground_forces);
	yt_record_set_number_if_changed(&planet->record, YT_F85,
	    planet->name_length);
	yt_record_set_number_if_changed(&planet->record, YT_F89,
	    planet->last_minute);
	yt_record_set_number_if_changed(&planet->record, YT_F113,
	    planet->plasma);
	yt_record_set_number_if_changed(&planet->record, YT_F117, planet->bank);
	yt_record_set_number_if_changed(&planet->record, YT_F125, planet->mines);
	yt_record_set_number_if_changed(&planet->record, YT_F129,
	    planet->fighters);
}

bool
yt_game_open(struct yt_game *game, enum yt_open_mode mode,
    struct yt_error *error)
{
	memset(game, 0, sizeof(*game));
	yt_random_init(&game->random);
	if (!yt_database_open(&game->database, "YTDATA.DAT", mode, error)
	    || !yt_config_load(&game->database, &game->config, error)) {
		yt_game_close(game);
		return false;
	}
	return yt_current_date_serial(game->config.epoch_year, &game->today,
	    &game->adjusted_year, error);
}

void
yt_game_close(struct yt_game *game)
{
	yt_database_close(&game->database);
}

bool
yt_game_read_player(struct yt_game *game, int basic_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_record record;

	if (!yt_database_read(&game->database, (size_t)basic_record, &record, error))
		return false;
	yt_player_decode(player, &record);
	return true;
}

bool
yt_game_write_player(struct yt_game *game, int basic_record,
    struct yt_player *player, struct yt_error *error)
{
	yt_player_encode(player);
	return yt_database_write(&game->database, (size_t)basic_record,
	    &player->record, error);
}

bool
yt_game_read_sector(struct yt_game *game, int logical_sector,
    struct yt_sector *sector, struct yt_error *error)
{
	struct yt_record record;

	if (!yt_database_read(&game->database,
	    (size_t)yt_sector_basic_record(&game->config, logical_sector),
	    &record, error))
		return false;
	yt_sector_decode(sector, &record);
	return true;
}

bool
yt_game_write_sector(struct yt_game *game, int logical_sector,
    struct yt_sector *sector, struct yt_error *error)
{
	yt_sector_encode(sector);
	return yt_database_write(&game->database,
	    (size_t)yt_sector_basic_record(&game->config, logical_sector),
	    &sector->record, error);
}

bool
yt_game_read_port(struct yt_game *game, int logical_port,
    struct yt_port *port, struct yt_error *error)
{
	struct yt_record record;

	if (!yt_database_read(&game->database,
	    (size_t)yt_port_basic_record(&game->config, logical_port),
	    &record, error))
		return false;
	yt_port_decode(port, &record);
	return true;
}

bool
yt_game_write_port(struct yt_game *game, int logical_port,
    struct yt_port *port, struct yt_error *error)
{
	yt_port_encode(port);
	return yt_database_write(&game->database,
	    (size_t)yt_port_basic_record(&game->config, logical_port),
	    &port->record, error);
}

bool
yt_game_read_planet(struct yt_game *game, int logical_planet,
    struct yt_planet *planet, struct yt_error *error)
{
	struct yt_record record;

	if (!yt_database_read(&game->database,
	    (size_t)yt_planet_basic_record(&game->config, logical_planet),
	    &record, error))
		return false;
	yt_planet_decode(planet, &record);
	return true;
}

bool
yt_game_write_planet(struct yt_game *game, int logical_planet,
    struct yt_planet *planet, struct yt_error *error)
{
	yt_planet_encode(planet);
	return yt_database_write(&game->database,
	    (size_t)yt_planet_basic_record(&game->config, logical_planet),
	    &planet->record, error);
}

bool
yt_game_construct_player(struct yt_game *game, int basic_record,
    const uint8_t today_raw[4], const uint8_t turns_raw[4],
    struct yt_player *player, struct yt_player_constructor_state *state,
    struct yt_error *error)
{
	static const uint8_t first_zero[4] = {0x00, 0x00, 0x0a, 0x00};
	static const uint8_t zero[4] = {0x00, 0x00, 0x00, 0x00};
	static const uint8_t one[4] = {0x00, 0x00, 0x00, 0x81};
	static const uint8_t hundred[4] = {0x00, 0x00, 0x48, 0x87};
	struct yt_record config_record;
	struct yt_record constructed;
	struct yt_player_constructor_state local_state;

	if (game == NULL || today_raw == NULL || turns_raw == NULL
	    || player == NULL)
		return false;
	if (state == NULL)
		state = &local_state;
	memset(state, 0, sizeof(*state));

	if (!yt_database_read(&game->database, 1, &config_record, error))
		return false;
	state->config_hydrated = true;
	if (!yt_game_read_player(game, basic_record, player, error))
		return false;
	state->player_hydrated = true;
	constructed = player->record;
	(void)yt_record_set_raw_number(&constructed, YT_F41, today_raw);
	(void)yt_record_set_raw_number(&constructed, YT_F45, first_zero);
	(void)yt_record_set_raw_number(&constructed, YT_F49, turns_raw);
	(void)yt_record_set_raw_number(&constructed, YT_F53, hundred);
	(void)yt_record_set_raw_number(&constructed, YT_F57, one);
	(void)yt_record_set_raw_number(&constructed, YT_F61,
	    config_record.bytes + YT_F65);
	(void)yt_record_set_raw_number(&constructed, YT_F65,
	    config_record.bytes + YT_F73);
	(void)yt_record_set_raw_number(&constructed, YT_F69, zero);
	(void)yt_record_set_raw_number(&constructed, YT_F73, zero);
	(void)yt_record_set_raw_number(&constructed, YT_F77, zero);
	(void)yt_record_set_raw_number(&constructed, YT_F81,
	    config_record.bytes + YT_F69);
	(void)yt_record_set_raw_number(&constructed, YT_F93, zero);
	(void)yt_record_set_raw_number(&constructed, YT_F97, one);
	(void)yt_record_set_raw_number(&constructed, YT_F101, zero);
	(void)yt_record_set_raw_number(&constructed, YT_F89, zero);
	(void)yt_record_set_raw_number(&constructed, YT_F105, zero);
	(void)yt_record_set_raw_number(&constructed, YT_F113, zero);
	(void)yt_record_set_raw_number(&constructed, YT_F125, one);
	(void)yt_record_set_raw_number(&constructed, YT_F117, zero);
	(void)yt_record_set_raw_number(&constructed, YT_F121, zero);
	(void)yt_record_set_raw_number(&constructed, YT_F129, zero);
	yt_player_decode(player, &constructed);
	state->put_attempted = true;
	return yt_database_write(&game->database, (size_t)basic_record,
	    &constructed, error);
}

bool
yt_game_set_player_identity(struct yt_game *game, int basic_record,
    const uint8_t *name, size_t length, struct yt_player *player,
    struct yt_error *error)
{
	static const uint8_t zero[4] = {0x00, 0x00, 0x00, 0x00};
	struct yt_record identity;
	uint8_t length_raw[4];

	if ((name == NULL && length != 0)
	    || !yt_game_read_player(game, basic_record, player, error))
		return false;
	identity = player->record;
	yt_record_set_text(&identity, name, length);
	(void)qb_mbf32_encode((float)length, length_raw);
	(void)yt_record_set_raw_number(&identity, YT_F85, length_raw);
	(void)yt_record_set_raw_number(&identity, YT_F89, zero);
	yt_player_decode(player, &identity);
	return yt_database_write(&game->database, (size_t)basic_record,
	    &identity, error);
}

bool
yt_game_post_login_repairs(struct yt_game *game, int basic_record,
    const uint8_t one_raw[4], const uint8_t zero_raw[4],
    const uint8_t maximum_holds_raw[4], struct yt_player *player,
    struct yt_post_login_repairs *repairs, struct yt_error *error)
{
	struct yt_post_login_repairs applied = {false, false, 0};
	struct yt_record repaired;
	float maximum_holds;

	if (repairs != NULL)
		*repairs = applied;
	if (game == NULL || one_raw == NULL || zero_raw == NULL
	    || maximum_holds_raw == NULL || player == NULL)
		return false;
	maximum_holds = qb_mbf32_decode(maximum_holds_raw);
	if (!yt_game_read_player(game, basic_record, player, error))
		return false;
	if (qb_mbf32_decode(player->record.bytes + YT_F57)
	    < qb_mbf32_decode(one_raw)) {
		repaired = player->record;
		(void)yt_record_set_raw_number(&repaired, YT_F57, one_raw);
		yt_player_decode(player, &repaired);
		applied.sector = true;
		if (!yt_database_write(&game->database, (size_t)basic_record,
		    &repaired, error)
		    || !yt_database_flush(&game->database, error)) {
			if (repairs != NULL)
				*repairs = applied;
			return false;
		}
		applied.writes++;
	}
	if (!yt_game_read_player(game, basic_record, player, error)) {
		if (repairs != NULL)
			*repairs = applied;
		return false;
	}
	if ((double)player->holds > (double)maximum_holds) {
		repaired = player->record;
		(void)yt_record_set_raw_number(&repaired, YT_F69, zero_raw);
		(void)yt_record_set_raw_number(&repaired, YT_F73, zero_raw);
		(void)yt_record_set_raw_number(&repaired, YT_F77,
		    maximum_holds_raw);
		(void)yt_record_set_raw_number(&repaired, YT_F65,
		    maximum_holds_raw);
		yt_player_decode(player, &repaired);
		applied.holds = true;
		if (!yt_database_write(&game->database, (size_t)basic_record,
		    &repaired, error)
		    || !yt_database_flush(&game->database, error)) {
			if (repairs != NULL)
				*repairs = applied;
			return false;
		}
		applied.writes++;
	}
	if (repairs != NULL)
		*repairs = applied;
	return true;
}

bool
yt_sector_force_route(float fighters, float owner, int current_player_record,
    enum yt_sector_force_route *route, int *owner_record,
    struct yt_error *error)
{
	if (route == NULL || owner_record == NULL)
		return false;
	*owner_record = 0;
	if (fighters == 0.0f || owner == (float)current_player_record) {
		*route = YT_SECTOR_FORCE_FRIENDLY;
		return true;
	}
	if (owner <= 0.0f) {
		*route = YT_SECTOR_FORCE_HOSTILE;
		return true;
	}
	if (!isfinite(owner) || owner != floorf(owner)
	    || owner > (float)INT_MAX) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation,
			    sizeof(error->operation), "%s",
			    "fighter owner record");
		}
		return false;
	}
	*route = YT_SECTOR_FORCE_OWNER_GET;
	*owner_record = (int)owner;
	return true;
}

bool
yt_sector_is_black_hole(float current_sector, float first, float second)
{
	return current_sector == first || current_sector == second;
}

bool
yt_sector_mines_admitted(float mines, float suppression)
{
	return mines > 0.0f && suppression == 0.0f;
}

bool
yt_sector_force_same_team(float current_team, float owner_team)
{
	return current_team != 0.0f && owner_team == current_team;
}

enum yt_port_owner_kind
yt_port_owner_classify(float owner, int current_player_record,
    int *owner_record)
{
	uint32_t record;

	if (owner_record != NULL)
		*owner_record = 0;
	if (owner <= 1.0f)
		return YT_PORT_OWNER_SILENT;
	if (owner == (float)current_player_record)
		return YT_PORT_OWNER_SELF;
	if (!isfinite(owner))
		return YT_PORT_OWNER_INVALID;
	record = qb_brun_random_record_number(owner);
	if (record > (uint32_t)INT_MAX)
		return YT_PORT_OWNER_INVALID;
	if (owner_record != NULL)
		*owner_record = (int)record;
	return YT_PORT_OWNER_OTHER;
}

bool
yt_port_owner_compose(enum yt_port_owner_kind kind, float treasury,
    const uint8_t *owner_name, size_t owner_name_length,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "This port is owned by: ";
	static const uint8_t self[] = "YOU, Credits:";
	char treasury_text[80];
	const uint8_t *suffix;
	size_t suffix_length;
	size_t needed;
	int formatted_length;

	if (length == NULL)
		return false;
	*length = 0U;
	if (kind == YT_PORT_OWNER_SILENT)
		return true;
	if (kind == YT_PORT_OWNER_SELF) {
		formatted_length = qb_str_double(treasury_text,
		    sizeof(treasury_text), (double)treasury);
		if (formatted_length < 0)
			return false;
		needed = sizeof(prefix) - 1U + sizeof(self) - 1U
		    + (size_t)formatted_length;
		if (needed > capacity || (needed != 0U && row == NULL))
			return false;
		memcpy(row, prefix, sizeof(prefix) - 1U);
		memcpy(row + sizeof(prefix) - 1U, self, sizeof(self) - 1U);
		memcpy(row + sizeof(prefix) - 1U + sizeof(self) - 1U,
		    treasury_text, (size_t)formatted_length);
		*length = needed;
		return true;
	}
	if (kind != YT_PORT_OWNER_OTHER
	    || (owner_name == NULL && owner_name_length != 0U))
		return false;
	suffix = owner_name;
	suffix_length = owner_name_length;
	needed = sizeof(prefix) - 1U + suffix_length;
	if (needed > capacity || (needed != 0U && row == NULL))
		return false;
	memcpy(row, prefix, sizeof(prefix) - 1U);
	if (suffix_length != 0U)
		memcpy(row + sizeof(prefix) - 1U, suffix, suffix_length);
	*length = needed;
	return true;
}

bool
yt_hostile_menu_row(double ship_fighters, double deployed_fighters,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "Fighters:";
	static const uint8_t separator[] = " /";
	char ship[64];
	char deployed[64];
	int ship_length;
	int deployed_length;
	size_t needed;
	size_t position = 0;

	if (length == NULL)
		return false;
	*length = 0;
	ship_length = qb_str_double(ship, sizeof(ship), ship_fighters);
	deployed_length = qb_str_double(deployed, sizeof(deployed),
	    deployed_fighters);
	if (ship_length < 0 || deployed_length < 0)
		return false;
	needed = sizeof(prefix) - 1U + (size_t)ship_length
	    + sizeof(separator) - 1U + (size_t)deployed_length;
	if (needed > capacity || (needed != 0 && row == NULL))
		return false;
	memcpy(row + position, prefix, sizeof(prefix) - 1U);
	position += sizeof(prefix) - 1U;
	memcpy(row + position, ship, (size_t)ship_length);
	position += (size_t)ship_length;
	memcpy(row + position, separator, sizeof(separator) - 1U);
	position += sizeof(separator) - 1U;
	memcpy(row + position, deployed, (size_t)deployed_length);
	position += (size_t)deployed_length;
	*length = position;
	return true;
}

enum yt_hostile_menu_route
yt_hostile_menu_dispatch(const char *response)
{
	static const char dispatch[] = "AQBDWT";
	const char *position;

	if (response == NULL || response[0] == '\0'
	    || strcmp(response, "?") == 0)
		return YT_HOSTILE_MENU_HELP;
	if (strcmp(response, "S") == 0)
		return YT_HOSTILE_MENU_SECTOR;
	if (strcmp(response, "I") == 0)
		return YT_HOSTILE_MENU_INFO;
	position = strstr(dispatch, response);
	if (position == NULL)
		return YT_HOSTILE_MENU_INVALID;
	switch (position - dispatch) {
	case 0:
		return YT_HOSTILE_MENU_ATTACK;
	case 1:
		return YT_HOSTILE_MENU_QUIT;
	case 2:
		return YT_HOSTILE_MENU_BRIBE;
	case 3:
		return YT_HOSTILE_MENU_MINE;
	case 4:
		return YT_HOSTILE_MENU_WARP;
	case 5:
		return YT_HOSTILE_MENU_TEAM;
	default:
		return YT_HOSTILE_MENU_INVALID;
	}
}

enum yt_main_shell_route
yt_main_shell_dispatch(const char *response)
{
	static const char dispatch[] = "W)+ABCFLMPQTD$GN";
	static const enum yt_main_shell_route routes[] = {
		YT_MAIN_SHELL_WARP,
		YT_MAIN_SHELL_MISSILE,
		YT_MAIN_SHELL_PLASMA,
		YT_MAIN_SHELL_ATTACK,
		YT_MAIN_SHELL_BUY_PORT,
		YT_MAIN_SHELL_COMPUTER,
		YT_MAIN_SHELL_FIGHTERS,
		YT_MAIN_SHELL_LAND,
		YT_MAIN_SHELL_MOVE,
		YT_MAIN_SHELL_TRADE,
		YT_MAIN_SHELL_QUIT,
		YT_MAIN_SHELL_TEAM,
		YT_MAIN_SHELL_MINES,
		YT_MAIN_SHELL_COLLECT,
		YT_MAIN_SHELL_GENESIS,
		YT_MAIN_SHELL_RENAME_PORT,
	};
	const char *position;

	if (response == NULL || response[0] == '\0')
		return YT_MAIN_SHELL_DISPLAY;
	if (strcmp(response, "X") == 0)
		return YT_MAIN_SHELL_SOUND;
	if (strcmp(response, "S") == 0)
		return YT_MAIN_SHELL_SENSORS;
	position = strchr(dispatch, response[0]);
	if (position != NULL)
		return routes[position - dispatch];
	switch (response[0]) {
	case 'V':
		return YT_MAIN_SHELL_VERSION;
	case 'I':
		return YT_MAIN_SHELL_INFO;
	case 'Z':
		return YT_MAIN_SHELL_INSTRUCTIONS;
	case '?':
		return YT_MAIN_SHELL_HELP;
	default:
		return YT_MAIN_SHELL_INVALID;
	}
}

bool
yt_main_prompt_row(const uint8_t *time_text, size_t time_text_length,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "Time:";
	static const uint8_t suffix[] = "Main Command (?=Help)? ";
	size_t needed = sizeof(prefix) - 1U + time_text_length
	    + sizeof(suffix) - 1U;

	if (length == NULL || (time_text == NULL && time_text_length != 0U)
	    || (row == NULL && needed != 0U) || needed > capacity)
		return false;
	memcpy(row, prefix, sizeof(prefix) - 1U);
	if (time_text_length != 0U)
		memcpy(row + sizeof(prefix) - 1U, time_text, time_text_length);
	memcpy(row + sizeof(prefix) - 1U + time_text_length, suffix,
	    sizeof(suffix) - 1U);
	*length = needed;
	return true;
}

bool
yt_computer_prompt_row(const uint8_t *time_text, size_t time_text_length,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "Time:";
	static const uint8_t suffix[] = "Computer command (?=help)? ";
	size_t needed = sizeof(prefix) - 1U + time_text_length
	    + sizeof(suffix) - 1U;

	if (length == NULL || (time_text == NULL && time_text_length != 0U)
	    || (row == NULL && needed != 0U) || needed > capacity)
		return false;
	memcpy(row, prefix, sizeof(prefix) - 1U);
	if (time_text_length != 0U)
		memcpy(row + sizeof(prefix) - 1U, time_text, time_text_length);
	memcpy(row + sizeof(prefix) - 1U + time_text_length, suffix,
	    sizeof(suffix) - 1U);
	*length = needed;
	return true;
}

enum yt_computer_newspaper_choice
yt_computer_newspaper_select(const char *response)
{
	if (response == NULL || response[1] != '\0')
		return YT_COMPUTER_NEWSPAPER_NONE;
	if (response[0] == 'T')
		return YT_COMPUTER_NEWSPAPER_TODAY;
	if (response[0] == 'Y')
		return YT_COMPUTER_NEWSPAPER_YESTERDAY;
	return YT_COMPUTER_NEWSPAPER_NONE;
}

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

bool
yt_port_link_missing(float link)
{
	return link == 0.0f;
}

float
yt_port_selected_expression(float port_offset, float logical_link)
{
	volatile float expression = port_offset + logical_link;

	return expression;
}

bool
yt_computer_port_maximum(float port_offset, float sector_offset,
    float *maximum, struct yt_error *error)
{
	uint8_t raw[4];
	volatile float difference = port_offset - sector_offset;
	enum qb_mbf_status status;

	if (maximum == NULL)
		return startup_configuration_error(error, YT_INVALID,
		    "computer port maximum arguments");
	status = qb_mbf32_encode(difference, raw);
	if (status == QB_MBF_OVERFLOW)
		return startup_configuration_error(error, YT_RANGE,
		    "computer port maximum subtraction");
	*maximum = status == QB_MBF_UNDERFLOW ? 0.0f : qb_mbf32_decode(raw);
	return true;
}

bool
yt_computer_port_select(const char *response, float maximum,
    float *selected, enum yt_computer_port_selection_route *route,
    struct yt_error *error)
{
	struct qb_val_result parsed;
	uint8_t raw[4];
	volatile float candidate;
	bool above;
	bool below;
	enum qb_mbf_status status;

	if (response == NULL || selected == NULL || route == NULL)
		return startup_configuration_error(error, YT_INVALID,
		    "computer port selection arguments");
	*selected = 0.0f;
	if (response[0] == '\0') {
		*route = YT_COMPUTER_PORT_SELECTION_EMPTY;
		return true;
	}
	parsed = qb_val(response);
	if (parsed.overflow)
		return startup_configuration_error(error, YT_RANGE,
		    "computer port sector VAL");
	candidate = (float)qb_int(parsed.valid ? parsed.value : 0.0);
	status = qb_mbf32_encode(candidate, raw);
	if (status == QB_MBF_OVERFLOW)
		return startup_configuration_error(error, YT_RANGE,
		    "computer port sector CSNG");
	*selected = status == QB_MBF_UNDERFLOW ? 0.0f
	    : qb_mbf32_decode(raw);
	above = *selected > maximum;
	below = *selected < 1.0f;
	*route = (above | below) ? YT_COMPUTER_PORT_SELECTION_INVALID
	    : YT_COMPUTER_PORT_SELECTION_ACCEPTED;
	return true;
}

bool
yt_computer_path_maximum(float port_offset, float sector_offset,
    float *maximum, struct yt_error *error)
{
	uint8_t raw[4];
	volatile float difference = port_offset - sector_offset;
	enum qb_mbf_status status;

	if (maximum == NULL)
		return startup_configuration_error(error, YT_INVALID,
		    "computer path maximum arguments");
	status = qb_mbf32_encode(difference, raw);
	if (status == QB_MBF_OVERFLOW)
		return startup_configuration_error(error, YT_RANGE,
		    "computer path maximum subtraction");
	*maximum = status == QB_MBF_UNDERFLOW ? 0.0f
	    : qb_mbf32_decode(raw);
	return true;
}

bool
yt_computer_path_parse(const char *response, float *selected,
    uint8_t selected_raw[4], struct yt_error *error)
{
	struct qb_val_result parsed;
	uint8_t integer_raw[8];
	enum qb_mbf_status status;

	if (response == NULL || selected == NULL || selected_raw == NULL)
		return startup_configuration_error(error, YT_INVALID,
		    "computer path parse arguments");
	parsed = qb_val(response);
	if (parsed.overflow)
		return startup_configuration_error(error, YT_RANGE,
		    "computer path sector VAL");
	status = qb_mbf64_floor_raw(parsed.mbf, integer_raw);
	if (status != QB_MBF_OK)
		return startup_configuration_error(error, YT_RANGE,
		    "computer path sector INT");
	status = qb_mbf32_from_mbf64_raw(integer_raw, selected_raw);
	if (status == QB_MBF_OVERFLOW || status == QB_MBF_DOMAIN)
		return startup_configuration_error(error, YT_RANGE,
		    "computer path sector CSNG");
	if (status == QB_MBF_UNDERFLOW)
		memset(selected_raw, 0, 4U);
	*selected = qb_mbf32_decode(selected_raw);
	return true;
}

bool
yt_computer_path_append_hop(char *scratch, size_t capacity,
    size_t *length, float next_sector, float *hop_count,
    uint8_t hop_count_raw[4], struct yt_error *error)
{
	char number[64];
	int number_length;
	volatile float incremented;
	enum qb_mbf_status status;

	if (scratch == NULL || capacity == 0U || length == NULL
	    || *length >= capacity || scratch[*length] != '\0'
	    || hop_count == NULL || hop_count_raw == NULL)
		return startup_configuration_error(error, YT_INVALID,
		    "computer path scratch arguments");
	number_length = qb_str_single(number, sizeof(number), next_sector);
	if (number_length < 0 || (size_t)number_length + 3U
	    >= capacity - *length)
		return startup_configuration_error(error, YT_RANGE,
		    "computer path scratch append");
	scratch[(*length)++] = '\r';
	scratch[(*length)++] = 'M';
	scratch[(*length)++] = '\r';
	memcpy(scratch + *length, number, (size_t)number_length);
	*length += (size_t)number_length;
	scratch[*length] = '\0';
	incremented = *hop_count + 1.0f;
	status = qb_mbf32_encode(incremented, hop_count_raw);
	if (status != QB_MBF_OK)
		return startup_configuration_error(error, YT_RANGE,
		    "computer path hop increment");
	*hop_count = qb_mbf32_decode(hop_count_raw);
	return true;
}

bool
yt_computer_path_wrap_required(int local_column)
{
	return local_column > 74;
}

static bool
computer_avoid_csng(const struct qb_val_result *parsed, float *selected,
    struct yt_error *error, const char *operation)
{
	uint8_t raw[4];
	enum qb_mbf_status status;

	status = qb_mbf32_from_mbf64_raw(parsed->mbf, raw);
	if (status == QB_MBF_OVERFLOW || status == QB_MBF_DOMAIN)
		return startup_configuration_error(error, YT_RANGE, operation);
	*selected = status == QB_MBF_UNDERFLOW ? 0.0f
	    : qb_mbf32_decode(raw);
	return true;
}

bool
yt_computer_avoid_maximum(float port_offset, float sector_offset,
    float *maximum, struct yt_error *error)
{
	uint8_t raw[4];
	volatile float difference = port_offset - sector_offset;
	enum qb_mbf_status status;

	if (maximum == NULL)
		return startup_configuration_error(error, YT_INVALID,
		    "avoid maximum arguments");
	status = qb_mbf32_encode(difference, raw);
	if (status == QB_MBF_OVERFLOW)
		return startup_configuration_error(error, YT_RANGE,
		    "avoid maximum subtraction");
	*maximum = status == QB_MBF_UNDERFLOW ? 0.0f
	    : qb_mbf32_decode(raw);
	return true;
}

bool
yt_computer_avoid_select_slot(const char *response, uint8_t conversion_mode,
    float *selected, int *index,
    enum yt_computer_avoid_selection_route *route, struct yt_error *error)
{
	struct qb_val_result parsed;
	bool overflow;

	if (response == NULL || selected == NULL || index == NULL
	    || route == NULL)
		return startup_configuration_error(error, YT_INVALID,
		    "avoid slot arguments");
	*selected = 0.0f;
	*index = 0;
	*route = YT_COMPUTER_AVOID_SELECTION_INVALID;
	parsed = qb_val(response);
	if (parsed.overflow)
		return startup_configuration_error(error, YT_RANGE,
		    "avoid slot VAL");
	if (!computer_avoid_csng(&parsed, selected, error,
	    "avoid slot CSNG"))
		return false;
	if (*selected < 1.0f || *selected > 30.0f)
		return true;
	*index = (int)qb_cint_mode((double)*selected, conversion_mode,
	    &overflow);
	if (overflow || *index < 1 || *index > 30)
		return startup_configuration_error(error, YT_RANGE,
		    "avoid slot CINT");
	*route = YT_COMPUTER_AVOID_SELECTION_ACCEPTED;
	return true;
}

bool
yt_computer_avoid_select_sector(const char *response, float maximum,
    float *selected, enum yt_computer_avoid_selection_route *route,
    struct yt_error *error)
{
	struct qb_val_result parsed;

	if (response == NULL || selected == NULL || route == NULL)
		return startup_configuration_error(error, YT_INVALID,
		    "avoid sector arguments");
	*selected = 0.0f;
	*route = YT_COMPUTER_AVOID_SELECTION_INVALID;
	parsed = qb_val(response);
	if (parsed.overflow)
		return startup_configuration_error(error, YT_RANGE,
		    "avoid sector VAL");
	if (!computer_avoid_csng(&parsed, selected, error,
	    "avoid sector CSNG"))
		return false;
	if (*selected < 0.0f || *selected > maximum)
		return true;
	*route = YT_COMPUTER_AVOID_SELECTION_ACCEPTED;
	return true;
}

void
yt_computer_avoid_transition(float old_value, float new_value,
    bool *locked, bool *available)
{
	if (locked != NULL)
		*locked = new_value != 0.0f;
	if (available != NULL)
		*available = old_value != 0.0f && old_value != new_value;
}

bool
yt_port_name_display_row(const uint8_t *cached, size_t cached_length,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "This port is called: \"";
	static const uint8_t suffix[] = "\".";
	size_t needed;

	if (length == NULL || (cached == NULL && cached_length != 0U))
		return false;
	*length = 0U;
	needed = sizeof(prefix) - 1U + cached_length + sizeof(suffix) - 1U;
	if (needed > capacity || (needed != 0U && row == NULL))
		return false;
	memcpy(row, prefix, sizeof(prefix) - 1U);
	if (cached_length != 0U)
		memcpy(row + sizeof(prefix) - 1U, cached, cached_length);
	memcpy(row + sizeof(prefix) - 1U + cached_length, suffix,
	    sizeof(suffix) - 1U);
	*length = needed;
	return true;
}

bool
yt_port_name_prepare_candidate(const uint8_t *entered,
    size_t entered_length, const uint8_t *cached, size_t cached_length,
    uint8_t *candidate, size_t capacity, size_t *candidate_length)
{
	size_t normalized;

	if (candidate_length == NULL
	    || (entered == NULL && entered_length != 0U)
	    || (cached == NULL && cached_length != 0U))
		return false;
	*candidate_length = 0U;
	if (candidate == NULL || entered_length > capacity)
		return false;
	if (entered_length != 0U)
		memcpy(candidate, entered, entered_length);
	normalized = qb_title_case_n(candidate, entered_length);
	if (normalized > YT_TEXT_FIELD_SIZE)
		normalized = YT_TEXT_FIELD_SIZE;
	if (normalized != 0U) {
		*candidate_length = normalized;
		return true;
	}
	if (cached_length > capacity
	    || (cached_length != 0U && candidate == NULL))
		return false;
	if (cached_length != 0U)
		memcpy(candidate, cached, cached_length);
	*candidate_length = cached_length;
	return true;
}

bool
yt_port_name_confirmation_prompt(const uint8_t *candidate,
    size_t candidate_length, uint8_t *prompt, size_t capacity,
    size_t *length)
{
	static const uint8_t suffix[] = "\" Is this OK? [y/N]";
	size_t needed;

	if (length == NULL || (candidate == NULL && candidate_length != 0U))
		return false;
	*length = 0U;
	needed = 1U + candidate_length + sizeof(suffix) - 1U;
	if (needed > capacity || (needed != 0U && prompt == NULL))
		return false;
	prompt[0] = '"';
	if (candidate_length != 0U)
		memcpy(prompt + 1U, candidate, candidate_length);
	memcpy(prompt + 1U + candidate_length, suffix,
	    sizeof(suffix) - 1U);
	*length = needed;
	return true;
}

bool
yt_port_name_overlay(struct yt_port *port, const uint8_t *candidate,
    size_t candidate_length)
{
	size_t copied;

	if (port == NULL || (candidate == NULL && candidate_length != 0U))
		return false;
	yt_record_set_text(&port->record, candidate, candidate_length);
	if (!yt_record_set_number(&port->record, YT_F85,
	    (float)candidate_length))
		return false;
	port->name_length = (float)candidate_length;
	copied = candidate_length < YT_TEXT_FIELD_SIZE
	    ? candidate_length : YT_TEXT_FIELD_SIZE;
	if (copied != 0U)
		memcpy(port->name, candidate, copied);
	port->name[copied] = '\0';
	return true;
}

double
yt_port_purchase_price(const float production[3])
{
	volatile float sum12;
	volatile float sum123;
	volatile float divided;
	volatile float integral;
	volatile float result;

	if (production == NULL)
		return 0.0;
	sum12 = production[0] + production[1];
	sum123 = sum12 + production[2];
	divided = sum123 / 10.0f;
	integral = floorf(divided);
	result = integral + 1.0f;
	return (double)result;
}

float
yt_port_purchase_seller_credit(float treasury, float credits, double price)
{
	volatile double subtotal = (double)treasury + (double)credits;
	volatile double total = subtotal + price;

	return (float)total;
}

float
yt_port_purchase_buyer_credit(float credits, double price)
{
	volatile double result = (double)credits - price;

	return (float)result;
}

bool
yt_port_purchase_seller_overlay(struct yt_player *seller, float treasury,
    double price)
{
	volatile float ports;

	if (seller == NULL)
		return false;
	seller->credits = yt_port_purchase_seller_credit(treasury,
	    seller->credits, price);
	ports = seller->ports_owned - 1.0f;
	seller->ports_owned = ports;
	return yt_record_set_number(&seller->record, YT_F81, seller->credits)
	    && yt_record_set_number(&seller->record, YT_F117,
	    seller->ports_owned);
}

bool
yt_port_purchase_title_overlay(struct yt_port *port, int buyer_record)
{
	if (port == NULL)
		return false;
	port->owner = (float)buyer_record;
	port->treasury = 0.0f;
	return yt_record_set_number(&port->record, YT_F97, port->owner)
	    && yt_record_set_number(&port->record, YT_F89, 0.0f);
}

bool
yt_port_purchase_buyer_overlay(struct yt_player *buyer, double price)
{
	volatile float ports;

	if (buyer == NULL)
		return false;
	buyer->credits = yt_port_purchase_buyer_credit(buyer->credits, price);
	ports = buyer->ports_owned + 1.0f;
	buyer->ports_owned = ports;
	return yt_record_set_number(&buyer->record, YT_F81, buyer->credits)
	    && yt_record_set_number(&buyer->record, YT_F117,
	    buyer->ports_owned);
}

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

bool
yt_planet_creation_credit_row(double credits, uint8_t *row,
    size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "You have";
	static const uint8_t suffix[] = " credits.";
	char number[64];
	int number_length;
	size_t needed;

	if (length == NULL)
		return false;
	*length = 0U;
	number_length = qb_str_double(number, sizeof(number), credits);
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

void
yt_planet_creation_overlay(struct yt_planet *planet,
    int current_player_record)
{
	static const uint8_t dirty_zero[4] = {0x00, 0x00, 0x20, 0x00};
	size_t index;

	if (planet == NULL)
		return;
	for (index = 0U; index < 3U; ++index) {
		planet->production[index] = 1.0f;
		planet->stock[index] = 10.0f;
		(void)yt_record_set_number(&planet->record,
		    YT_F45 + index * 4U, 1.0f);
		(void)yt_record_set_number(&planet->record,
		    YT_F57 + index * 4U, 10.0f);
	}
	planet->mines = 0.0f;
	planet->missiles = 0.0f;
	planet->owner = (float)current_player_record;
	planet->ground_forces = 1.0f;
	planet->plasma = 0.0f;
	planet->bank = 0.0f;
	planet->fighters = 30.0f;
	(void)yt_record_set_raw_number(&planet->record, YT_F125, dirty_zero);
	(void)yt_record_set_raw_number(&planet->record, YT_F69, dirty_zero);
	(void)yt_record_set_number(&planet->record, YT_F73, planet->owner);
	(void)yt_record_set_number(&planet->record, YT_F77, 1.0f);
	(void)yt_record_set_number(&planet->record, YT_F113, 0.0f);
	(void)yt_record_set_number(&planet->record, YT_F117, 0.0f);
	(void)yt_record_set_number(&planet->record, YT_F129, 30.0f);
}

void
yt_planet_creation_timestamp_overlay(struct yt_planet *planet,
    float day, float minute)
{
	if (planet == NULL)
		return;
	planet->last_day = day;
	planet->last_minute = minute;
	(void)yt_record_set_number(&planet->record, YT_F41, day);
	(void)yt_record_set_number(&planet->record, YT_F89, minute);
}

void
yt_planet_creation_credit_overlay(struct yt_player *player,
    float price_argument)
{
	volatile float sum;
	volatile float integral;

	if (player == NULL)
		return;
	sum = player->credits + price_argument;
	integral = floorf(sum);
	player->credits = integral;
	(void)yt_record_set_number(&player->record, YT_F81, integral);
}

bool
yt_planet_creation_news(const uint8_t *trader_name,
    size_t trader_name_length, const uint8_t *planet_name,
    size_t planet_name_length, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t prefix[] = "  -  ";
	static const uint8_t middle[] = " made a planet: ";
	size_t needed;
	size_t cursor = 0U;

	if (length == NULL || (trader_name == NULL && trader_name_length != 0U)
	    || (planet_name == NULL && planet_name_length != 0U))
		return false;
	*length = 0U;
	needed = sizeof(prefix) - 1U + trader_name_length
	    + sizeof(middle) - 1U + planet_name_length;
	if (needed > capacity || (needed != 0U && row == NULL))
		return false;
	memcpy(row + cursor, prefix, sizeof(prefix) - 1U);
	cursor += sizeof(prefix) - 1U;
	if (trader_name_length != 0U) {
		memcpy(row + cursor, trader_name, trader_name_length);
		cursor += trader_name_length;
	}
	memcpy(row + cursor, middle, sizeof(middle) - 1U);
	cursor += sizeof(middle) - 1U;
	if (planet_name_length != 0U)
		memcpy(row + cursor, planet_name, planet_name_length);
	*length = needed;
	return true;
}

bool
yt_planet_creation_success_row(const uint8_t *planet_name,
    size_t planet_name_length, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t prefix[] = "Planet \"";
	static const uint8_t suffix[] =
	    "\" created with Genesis Device!";
	size_t needed;

	if (length == NULL || (planet_name == NULL && planet_name_length != 0U))
		return false;
	*length = 0U;
	needed = sizeof(prefix) - 1U + planet_name_length
	    + sizeof(suffix) - 1U;
	if (needed > capacity || (needed != 0U && row == NULL))
		return false;
	memcpy(row, prefix, sizeof(prefix) - 1U);
	if (planet_name_length != 0U)
		memcpy(row + sizeof(prefix) - 1U, planet_name,
		    planet_name_length);
	memcpy(row + sizeof(prefix) - 1U + planet_name_length, suffix,
	    sizeof(suffix) - 1U);
	*length = needed;
	return true;
}

float
yt_planet_move_destination(const char *response)
{
	struct qb_val_result parsed;
	volatile double integral;

	if (response == NULL)
		return 0.0f;
	parsed = qb_val(response);
	integral = floor(parsed.valid ? parsed.value : 0.0);
	return (float)integral;
}

float
yt_planet_move_maximum(float port_record_offset,
    float sector_record_offset)
{
	volatile float result = port_record_offset - sector_record_offset;

	return result;
}

float
yt_planet_move_add_cost(float cost)
{
	volatile float result = cost + 10.0f;

	return result;
}

float
yt_planet_move_fighter_loss(float fighters, float first_draw,
    float second_draw)
{
	volatile float first_product = first_draw * fighters;
	volatile float first = floorf(first_product) + 1.0f;
	volatile float second_product = second_draw * first;
	volatile float loss = floorf(second_product) + 1.0f;

	return loss;
}

void
yt_planet_move_sector_overlay(struct yt_sector *sector, float planet_link)
{
	if (sector == NULL)
		return;
	sector->planet = planet_link;
	(void)yt_record_set_number(&sector->record, YT_F93, planet_link);
}

void
yt_planet_move_explosion_overlay(struct yt_planet *planet)
{
	static const uint8_t zero_raw[4] = {0, 0, 0, 0};

	if (planet == NULL)
		return;
	planet->name[0] = '\0';
	planet->name_length = 0.0f;
	memcpy(planet->record.bytes, zero_raw, sizeof(zero_raw));
	memset(planet->record.bytes + sizeof(zero_raw), ' ',
	    YT_TEXT_FIELD_SIZE - sizeof(zero_raw));
	(void)yt_record_set_raw_number(&planet->record, YT_F85, zero_raw);
}

void
yt_planet_move_fighter_overlay(struct yt_player *player, float loss)
{
	volatile float remaining;

	if (player == NULL)
		return;
	remaining = player->fighters - loss;
	player->fighters = remaining;
	(void)yt_record_set_number(&player->record, YT_F61, remaining);
}

void
yt_planet_move_success_overlay(struct yt_player *player,
    float requested_destination)
{
	volatile float remaining;

	if (player == NULL)
		return;
	remaining = player->turns + -10.0f;
	player->turns = remaining;
	player->sector = requested_destination;
	(void)yt_record_set_number(&player->record, YT_F49, remaining);
	(void)yt_record_set_number(&player->record, YT_F57,
	    requested_destination);
}

static bool
move_join_parts(const uint8_t *first, size_t first_length,
    const uint8_t *second, size_t second_length,
    const uint8_t *third, size_t third_length,
    const uint8_t *fourth, size_t fourth_length,
    const uint8_t *fifth, size_t fifth_length,
    uint8_t *row, size_t capacity, size_t *length)
{
	const uint8_t *parts[5] = {first, second, third, fourth, fifth};
	const size_t sizes[5] = {first_length, second_length, third_length,
	    fourth_length, fifth_length};
	size_t needed = 0U;
	size_t cursor = 0U;
	size_t index;

	if (length == NULL)
		return false;
	*length = 0U;
	for (index = 0U; index < 5U; ++index) {
		if (parts[index] == NULL && sizes[index] != 0U)
			return false;
		if (SIZE_MAX - needed < sizes[index])
			return false;
		needed += sizes[index];
	}
	if (needed > capacity || (row == NULL && needed != 0U))
		return false;
	for (index = 0U; index < 5U; ++index) {
		if (sizes[index] != 0U) {
			memcpy(row + cursor, parts[index], sizes[index]);
			cursor += sizes[index];
		}
	}
	*length = needed;
	return true;
}

bool
yt_planet_move_path_heading(float start, float destination,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t first[] = "The shortest path from sector";
	static const uint8_t middle[] = " to sector";
	static const uint8_t suffix[] = " is:";
	char start_text[64];
	char destination_text[64];
	int start_length = qb_str_single(start_text, sizeof(start_text), start);
	int destination_length = qb_str_single(destination_text,
	    sizeof(destination_text), destination);

	if (start_length < 0 || destination_length < 0)
		return false;
	return move_join_parts(first, sizeof(first) - 1U,
	    (const uint8_t *)start_text, (size_t)start_length,
	    middle, sizeof(middle) - 1U,
	    (const uint8_t *)destination_text, (size_t)destination_length,
	    suffix, sizeof(suffix) - 1U, row, capacity, length);
}

bool
yt_planet_move_summary(float cost, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t first[] = "Distance is";
	static const uint8_t middle[] = " and will take";
	static const uint8_t suffix[] = " turns.";
	volatile float distance = cost / 10.0f;
	char distance_text[64];
	char cost_text[64];
	int distance_length = qb_str_single(distance_text,
	    sizeof(distance_text), distance);
	int cost_length = qb_str_single(cost_text, sizeof(cost_text), cost);

	if (distance_length < 0 || cost_length < 0)
		return false;
	return move_join_parts(first, sizeof(first) - 1U,
	    (const uint8_t *)distance_text, (size_t)distance_length,
	    middle, sizeof(middle) - 1U,
	    (const uint8_t *)cost_text, (size_t)cost_length,
	    suffix, sizeof(suffix) - 1U, row, capacity, length);
}

bool
yt_planet_move_turns_row(float turns, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t first[] = "You have";
	static const uint8_t suffix[] = " turns left.";
	char number[64];
	int number_length = qb_str_single(number, sizeof(number), turns);

	if (number_length < 0)
		return false;
	return move_join_parts(first, sizeof(first) - 1U,
	    (const uint8_t *)number, (size_t)number_length,
	    suffix, sizeof(suffix) - 1U, NULL, 0U, NULL, 0U,
	    row, capacity, length);
}

bool
yt_planet_move_explosion_row(const uint8_t *planet_name,
    size_t planet_name_length, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t first[] = "The stress was too much! PLANET ";
	static const uint8_t suffix[] = " EXPLODED!";

	return move_join_parts(first, sizeof(first) - 1U,
	    planet_name, planet_name_length, suffix, sizeof(suffix) - 1U,
	    NULL, 0U, NULL, 0U, row, capacity, length);
}

bool
yt_planet_move_explosion_news(const uint8_t *planet_name,
    size_t planet_name_length, const uint8_t *player_name,
    size_t player_name_length, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t first[] = " *** Planet ";
	static const uint8_t middle[] = " EXPLODED while being moved by ";
	static const uint8_t suffix[] = "!!!";

	return move_join_parts(first, sizeof(first) - 1U,
	    planet_name, planet_name_length, middle, sizeof(middle) - 1U,
	    player_name, player_name_length, suffix, sizeof(suffix) - 1U,
	    row, capacity, length);
}

bool
yt_planet_move_loss_row(const uint8_t *actor, size_t actor_length,
    float loss, uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t middle[] = " lost";
	static const uint8_t suffix[] = " fighters in the explosion!";
	char number[64];
	int number_length = qb_str_single(number, sizeof(number), loss);

	if (number_length < 0)
		return false;
	return move_join_parts(actor, actor_length, middle, sizeof(middle) - 1U,
	    (const uint8_t *)number, (size_t)number_length,
	    suffix, sizeof(suffix) - 1U, NULL, 0U,
	    row, capacity, length);
}

bool
yt_planet_move_success_row(const uint8_t *planet_name,
    size_t planet_name_length, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t suffix[] =
	    " moved! (Xannoron Movers, we move anyTHING, anyWHERE!)";

	return move_join_parts(planet_name, planet_name_length,
	    suffix, sizeof(suffix) - 1U, NULL, 0U, NULL, 0U, NULL, 0U,
	    row, capacity, length);
}

float
yt_sector_mine_batch(float mines_before)
{
	volatile float quotient;

	if (!(mines_before > 19.0f))
		return 1.0f;
	quotient = mines_before / 10.0f;
	return floorf(quotient);
}

float
yt_sector_mine_shield_result(float shields, float batch, float draw)
{
	volatile float product = draw * 1001.0f;
	volatile float quantum = floorf(product);
	volatile float loss = quantum * batch;
	volatile float result = shields - loss;

	return result < 1.0f ? 0.0f : result;
}

float
yt_sector_mine_cloak_loss(float cloak, float batch, float draw)
{
	volatile float first = draw * batch;
	volatile float scaled = first * 100.0f;
	volatile float integral = floorf(scaled);
	volatile float loss = integral / 100.0f;

	return loss > cloak ? cloak : loss;
}

float
yt_sector_mine_missile_loss(float missiles, float batch, float draw)
{
	volatile float range = batch * missiles;
	volatile float product = draw * range;
	volatile float loss = floorf(product) + 1.0f;

	return loss > missiles ? missiles : loss;
}

float
yt_sector_mine_empty_holds(const struct yt_player *player)
{
	volatile float empty;

	if (player == NULL)
		return 0.0f;
	empty = player->holds - player->equipment;
	empty = empty - player->organics;
	empty = empty - player->ore;
	return empty;
}

void
yt_sector_mine_sector_overlay(struct yt_sector *sector, float mines_after)
{
	if (sector == NULL)
		return;
	sector->mines = mines_after;
	(void)yt_record_set_number(&sector->record, YT_F129, mines_after);
}

void
yt_sector_mine_player_overlay(struct yt_player *fresh,
    const struct yt_player *working, unsigned fields)
{
	static const uint8_t scanner_zero[4] = {0x00, 0x00, 0x48, 0x00};

	if (fresh == NULL || working == NULL)
		return;
#define MINE_OVERLAY(flag, member, offset) do { \
	if ((fields & (flag)) != 0U) { \
		fresh->member = working->member; \
		(void)yt_record_set_number(&fresh->record, (offset), \
		    working->member); \
	} \
} while (0)
	MINE_OVERLAY(YT_SECTOR_MINE_DAMAGE_SHIELDS, shields, YT_F53);
	MINE_OVERLAY(YT_SECTOR_MINE_DAMAGE_FIGHTERS, fighters, YT_F61);
	MINE_OVERLAY(YT_SECTOR_MINE_DAMAGE_HOLDS, holds, YT_F65);
	MINE_OVERLAY(YT_SECTOR_MINE_DAMAGE_ORE, ore, YT_F69);
	MINE_OVERLAY(YT_SECTOR_MINE_DAMAGE_ORGANICS, organics, YT_F73);
	MINE_OVERLAY(YT_SECTOR_MINE_DAMAGE_EQUIPMENT, equipment, YT_F77);
	if ((fields & YT_SECTOR_MINE_DAMAGE_SCANNER) != 0U) {
		fresh->danger_scanner = working->danger_scanner;
		if (working->danger_scanner == 0.0f)
			(void)yt_record_set_raw_number(&fresh->record, YT_F93,
			    scanner_zero);
		else
			(void)yt_record_set_number(&fresh->record, YT_F93,
			    working->danger_scanner);
	}
	MINE_OVERLAY(YT_SECTOR_MINE_DAMAGE_MISSILES, missiles, YT_F97);
	MINE_OVERLAY(YT_SECTOR_MINE_DAMAGE_CLOAK, cloak, YT_F125);
	MINE_OVERLAY(YT_SECTOR_MINE_DAMAGE_CARRIED_MINES, mines, YT_F129);
#undef MINE_OVERLAY
}

bool
yt_sector_mine_explosion_row(float mines_before, float batch,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t first[] = "There are";
	static const uint8_t middle[] = " mines here!";
	static const uint8_t suffix[] = " EXPLODE!";
	char before_text[64];
	char batch_text[64];
	int before_length = qb_str_single(before_text, sizeof(before_text),
	    mines_before);
	int batch_length = qb_str_single(batch_text, sizeof(batch_text), batch);

	if (before_length < 0 || batch_length < 0)
		return false;
	return move_join_parts(first, sizeof(first) - 1U,
	    (const uint8_t *)before_text, (size_t)before_length,
	    middle, sizeof(middle) - 1U,
	    (const uint8_t *)batch_text, (size_t)batch_length,
	    suffix, sizeof(suffix) - 1U, row, capacity, length);
}

bool
yt_sector_mine_shields_row(float shields, uint8_t *row,
    size_t capacity, size_t *length)
{
	static const uint8_t first[] = "Shields down to";
	static const uint8_t suffix[] = " units!";
	char number[64];
	int number_length = qb_str_single(number, sizeof(number), shields);

	if (number_length < 0)
		return false;
	return move_join_parts(first, sizeof(first) - 1U,
	    (const uint8_t *)number, (size_t)number_length,
	    suffix, sizeof(suffix) - 1U, NULL, 0U, NULL, 0U,
	    row, capacity, length);
}

bool
yt_sector_mine_loss_row(enum yt_sector_mine_loss_kind kind, float loss,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t first[] = "Lost";
	static const char *const suffixes[] = {
		" fighters!", "% cloak!", " Missiles!", " mines!",
		" holds of ore!", " holds of organics!",
		" holds of equipment!", " empty holds!",
	};
	char number[64];
	int number_length;
	const char *suffix;

	if (kind < YT_SECTOR_MINE_LOSS_FIGHTERS
	    || kind > YT_SECTOR_MINE_LOSS_EMPTY_HOLDS)
		return false;
	suffix = suffixes[kind];
	number_length = qb_str_single(number, sizeof(number), loss);
	if (number_length < 0)
		return false;
	return move_join_parts(first, sizeof(first) - 1U,
	    (const uint8_t *)number, (size_t)number_length,
	    (const uint8_t *)suffix, strlen(suffix), NULL, 0U, NULL, 0U,
	    row, capacity, length);
}

bool
yt_sector_mine_entry_news(const uint8_t *player_name,
    size_t player_name_length, float sector, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t middle[] = " hit sector mines in sector";
	static const uint8_t suffix[] = "!";
	char number[64];
	int number_length = qb_str_single(number, sizeof(number), sector);

	if (number_length < 0)
		return false;
	return move_join_parts(player_name, player_name_length,
	    middle, sizeof(middle) - 1U,
	    (const uint8_t *)number, (size_t)number_length,
	    suffix, sizeof(suffix) - 1U, NULL, 0U,
	    row, capacity, length);
}

bool
yt_sector_mine_final_news(float shields, uint8_t *row,
    size_t capacity, size_t *length)
{
	static const uint8_t first[] = "Shields reduced to";
	static const uint8_t suffix[] = " units!";
	char number[64];
	int number_length = qb_str_single(number, sizeof(number), shields);

	if (number_length < 0)
		return false;
	return move_join_parts(first, sizeof(first) - 1U,
	    (const uint8_t *)number, (size_t)number_length,
	    suffix, sizeof(suffix) - 1U, NULL, 0U, NULL, 0U,
	    row, capacity, length);
}

bool
yt_direct_fighter_mine_warning(const uint8_t *victim_name,
    size_t victim_name_length, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t prefix[] = "  -  ";
	static const uint8_t suffix[] =
	    " had sector mines! They EXPLODED!";

	return move_join_parts(prefix, sizeof(prefix) - 1U,
	    victim_name, victim_name_length, suffix, sizeof(suffix) - 1U,
	    NULL, 0U, NULL, 0U, row, capacity, length);
}

float
yt_emergency_warp_duration(float first, float second)
{
	volatile float first_part = first * 70.0f;
	volatile float second_part = second * 70.0f;
	volatile float result = first_part + second_part;

	return result;
}

float
yt_emergency_warp_destination(float draw, float sector_count)
{
	volatile float product = draw * sector_count;
	volatile float integral = floorf(product);
	volatile float result = integral + 1.0f;

	return result;
}

float
yt_emergency_warp_cost(float heat, float draw, float turns, bool meltdown)
{
	volatile float heat_cost = heat * 4.0f;
	volatile float jitter_product = draw * 4.0f;
	volatile float jitter = floorf(jitter_product);
	volatile float result = heat_cost + jitter;

	if (result > turns)
		result = turns;
	if (meltdown)
		result = turns;
	return result;
}

void
yt_emergency_warp_player_overlay(struct yt_player *player,
    float destination, float cost)
{
	volatile float remaining;

	if (player == NULL)
		return;
	remaining = player->turns - cost;
	player->sector = destination;
	player->turns = remaining;
	(void)yt_record_set_number(&player->record, YT_F57, destination);
	(void)yt_record_set_number(&player->record, YT_F49, remaining);
}

bool
yt_gameplay_hazard_error_project(unsigned error_number, unsigned saved_ip,
    struct yt_gameplay_hazard_error_request *request)
{
	if (request == NULL || error_number == 0U || error_number > UINT8_MAX
	    || saved_ip > UINT16_MAX)
		return false;
	request->error_number = (uint8_t)error_number;
	request->saved_ip = (uint16_t)saved_ip;
	request->handler = 0x45F7U;
	return true;
}

bool
yt_emergency_warp_result_row(float destination, float cost,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t first[] = "sector";
	static const uint8_t middle[] =
	    ". However, it takes you";
	static const uint8_t suffix[] =
	    " turns to recharge your engines!";
	char destination_text[64];
	char cost_text[64];
	int destination_length = qb_str_single(destination_text,
	    sizeof(destination_text), destination);
	int cost_length = qb_str_single(cost_text, sizeof(cost_text), cost);

	if (destination_length < 0 || cost_length < 0)
		return false;
	return move_join_parts(first, sizeof(first) - 1U,
	    (const uint8_t *)destination_text, (size_t)destination_length,
	    middle, sizeof(middle) - 1U,
	    (const uint8_t *)cost_text, (size_t)cost_length,
	    suffix, sizeof(suffix) - 1U, row, capacity, length);
}

bool
yt_emergency_warp_stranded_row(float destination, uint8_t *row,
    size_t capacity, size_t *length)
{
	static const uint8_t first[] = "You are stranded in sector";
	static const uint8_t suffix[] = ".";
	char destination_text[64];
	int destination_length = qb_str_single(destination_text,
	    sizeof(destination_text), destination);

	if (destination_length < 0)
		return false;
	return move_join_parts(first, sizeof(first) - 1U,
	    (const uint8_t *)destination_text, (size_t)destination_length,
	    suffix, sizeof(suffix) - 1U, NULL, 0U, NULL, 0U,
	    row, capacity, length);
}

bool
yt_movement_warp_row(const float warps[6], uint8_t *row,
    size_t capacity, size_t *length)
{
	static const uint8_t heading[] = "Warps lead to";
	size_t used = sizeof(heading) - 1U;
	size_t slot;

	if (warps == NULL || row == NULL || length == NULL
	    || used > capacity)
		return false;
	memcpy(row, heading, used);
	for (slot = 0U; slot < 6U; ++slot) {
		char number[64];
		int number_length;

		if (warps[slot] == 0.0f)
			continue;
		number_length = qb_str_single(number, sizeof(number), warps[slot]);
		if (number_length < 0 || used + 1U + (size_t)number_length
		    > capacity)
			return false;
		row[used++] = ',';
		memcpy(row + used, number, (size_t)number_length);
		used += (size_t)number_length;
	}
	*length = used;
	return true;
}

bool
yt_movement_confirmation_prompt(float target, uint8_t *row,
    size_t capacity, size_t *length)
{
	static const uint8_t first[] = "Move into sector";
	static const uint8_t suffix[] = "? [y/N] ";
	char number[64];
	int number_length = qb_str_single(number, sizeof(number), target);

	if (number_length < 0)
		return false;
	return move_join_parts(first, sizeof(first) - 1U,
	    (const uint8_t *)number, (size_t)number_length,
	    suffix, sizeof(suffix) - 1U, NULL, 0U, NULL, 0U,
	    row, capacity, length);
}

void
yt_movement_player_overlay(struct yt_player *player, float target)
{
	if (player == NULL)
		return;
	player->sector = target;
	(void)yt_record_set_number(&player->record, YT_F57, target);
}

static bool
market_encode_single(float value, uint8_t raw[4], struct yt_error *error,
    const char *operation)
{
	enum qb_mbf_status status = qb_mbf32_encode(value, raw);

	if (status == QB_MBF_OK || status == QB_MBF_UNDERFLOW)
		return true;
	return startup_configuration_error(error, YT_RANGE, operation);
}

typedef enum qb_mbf_status (*market_binary_fn)(const uint8_t left[8],
    const uint8_t right[8], uint8_t result[8]);

static bool
market_binary(market_binary_fn operation, const uint8_t left[8],
    const uint8_t right[8], uint8_t result[8], struct yt_error *error,
    const char *label)
{
	enum qb_mbf_status status = operation(left, right, result);

	if (status == QB_MBF_OK || status == QB_MBF_UNDERFLOW)
		return true;
	return startup_configuration_error(error, YT_RANGE, label);
}

bool
yt_port_market_update(struct yt_port_market_state *state,
    struct yt_error *error)
{
	uint8_t ten[8];
	uint8_t thousand[8];
	uint8_t one[8];
	uint8_t half[8];
	uint8_t zero[8] = {0};
	uint8_t current_day_raw[4];
	uint8_t current_minute_raw[4];
	uint8_t mutable_capacity[3][8];
	uint8_t mutable_production[3][4];
	uint8_t mutable_price[3][4];
	struct yt_record updated;
	bool raised[3] = {false, false, false};
	float minute;
	float elapsed;
	size_t index;

	if (state == NULL)
		return false;
	state->current_minute = 0.0f;
	state->elapsed = 0.0f;
	memset(state->capacity_raw, 0, sizeof(state->capacity_raw));
	memset(state->capacity, 0, sizeof(state->capacity));
	memset(state->production_raw, 0, sizeof(state->production_raw));
	memset(state->price_raw, 0, sizeof(state->price_raw));
	memset(state->price, 0, sizeof(state->price));
	memset(state->production_raised, 0,
	    sizeof(state->production_raised));
	state->completed_items = 0U;
	state->complete = false;

	if (qb_mbf64_from_u64(10U, ten) != QB_MBF_OK
	    || qb_mbf64_from_u64(1000U, thousand) != QB_MBF_OK
	    || qb_mbf64_from_u64(1U, one) != QB_MBF_OK
	    || qb_mbf64_encode(0.5, half) != QB_MBF_OK)
		return startup_configuration_error(error, YT_RANGE,
		    "ordinary port constants");
	minute = yt_port_single_div(state->timer_seconds, 60.0f);
	elapsed = yt_port_single_add(
	    yt_port_single_sub(state->current_day, state->port.last_day),
	    yt_port_single_div(yt_port_single_sub(minute,
	    state->port.last_minute), 1440.0f));
	if (elapsed > 10.0f || elapsed < 0.0f)
		elapsed = 10.0f;
	if (!market_encode_single(state->current_day, current_day_raw, error,
	    "ordinary port current day")
	    || !market_encode_single(minute, current_minute_raw, error,
	    "ordinary port current minute"))
		return false;

	for (index = 0U; index < 3U; ++index) {
		uint8_t growth_raw[4];
		uint8_t growth[8];
		uint8_t quotient[8];
		uint8_t promoted_production[8];
		uint8_t base_raw[4];
		uint8_t base[8];
		uint8_t factor[8];
		uint8_t numerator[8];
		uint8_t denominator[8];
		uint8_t ratio[8];
		uint8_t scale[8];
		uint8_t raw_price[8];
		uint8_t rounded_source[8];
		uint8_t rounded[8];
		float growth_value;

		yt_port_mbf64_promote_single(state->port.record.bytes
		    + YT_F49 + index * 4U, mutable_capacity[index]);
		memcpy(mutable_production[index], state->port.record.bytes
		    + YT_F61 + index * 4U, 4U);
		growth_value = yt_port_single_mul(
		    qb_mbf32_decode(mutable_production[index]), elapsed);
		if (!market_encode_single(growth_value, growth_raw, error,
		    "ordinary port growth"))
			return false;
		yt_port_mbf64_promote_single(growth_raw, growth);
		if (!market_binary(qb_mbf64_add_raw, mutable_capacity[index],
		    growth, mutable_capacity[index], error,
		    "ordinary port capacity")
		    || !market_binary(qb_mbf64_div_raw, mutable_capacity[index],
		    ten, quotient, error, "ordinary port production comparison"))
			return false;
		yt_port_mbf64_promote_single(mutable_production[index],
		    promoted_production);
		if (yt_port_mbf64_compare(quotient, promoted_production) > 0) {
			if (!market_binary(qb_mbf64_div_raw,
			    mutable_capacity[index], ten, quotient, error,
			    "ordinary port production replacement")
			    || qb_mbf32_from_mbf64_raw(quotient,
			    mutable_production[index]) == QB_MBF_OVERFLOW)
				return startup_configuration_error(error, YT_RANGE,
				    "ordinary port production CSNG");
			raised[index] = true;
			yt_port_mbf64_promote_single(mutable_production[index],
			    promoted_production);
		}
		if (!market_encode_single(state->base_price[index], base_raw,
		    error, "ordinary port base price"))
			return false;
		yt_port_mbf64_promote_single(base_raw, base);
		yt_port_mbf64_promote_single(state->port.record.bytes
		    + YT_F73 + index * 4U, factor);
		if (!market_binary(qb_mbf64_mul_raw, factor,
		    mutable_capacity[index], numerator, error,
		    "ordinary port price numerator")
		    || !market_binary(qb_mbf64_mul_raw, promoted_production,
		    thousand, denominator, error,
		    "ordinary port price denominator")
		    || !market_binary(qb_mbf64_div_raw, numerator, denominator,
		    ratio, error, "ordinary port price division"))
			return false;
		memcpy(scale, ratio, 8U);
		yt_port_mbf64_negate(scale);
		if (!market_binary(qb_mbf64_add_raw, one, scale, scale, error,
		    "ordinary port price scale")
		    || !market_binary(qb_mbf64_mul_raw, base, scale, raw_price,
		    error, "ordinary port raw price")
		    || !market_binary(qb_mbf64_add_raw, raw_price, half,
		    rounded_source, error, "ordinary port price rounding"))
			return false;
		if (yt_port_mbf64_compare(rounded_source, zero) <= 0)
			memset(rounded, 0, sizeof(rounded));
		else {
			enum qb_mbf_status status = qb_mbf64_floor_positive_raw(
			    rounded_source, rounded);

			if (status != QB_MBF_OK && status != QB_MBF_UNDERFLOW)
				return startup_configuration_error(error, YT_RANGE,
				    "ordinary port price INT");
		}
		if (qb_mbf32_from_mbf64_raw(rounded, mutable_price[index])
		    == QB_MBF_OVERFLOW)
			return startup_configuration_error(error, YT_RANGE,
			    "ordinary port price CSNG");
		if (qb_mbf32_decode(mutable_price[index]) < 1.0f
		    && !market_encode_single(1.0f, mutable_price[index], error,
		    "ordinary port price floor"))
			return false;
		++state->completed_items;
	}

	updated = state->port.record;
	if (!yt_record_set_raw_number(&updated, YT_F45,
	    current_day_raw)
	    || !yt_record_set_raw_number(&updated, YT_F101,
	    current_minute_raw))
		return false;
	for (index = 0U; index < 3U; ++index) {
		uint8_t stored_capacity[4];

		if (qb_mbf32_from_mbf64_raw(mutable_capacity[index],
		    stored_capacity) == QB_MBF_OVERFLOW
		    || !yt_record_set_raw_number(&updated,
		    YT_F49 + index * 4U, stored_capacity)
		    || !yt_record_set_raw_number(&updated,
		    YT_F61 + index * 4U, mutable_production[index]))
			return startup_configuration_error(error, YT_RANGE,
			    "ordinary port FIELD overlay");
		memcpy(state->capacity_raw[index], mutable_capacity[index], 8U);
		state->capacity[index] = qb_mbf64_decode(mutable_capacity[index]);
		memcpy(state->production_raw[index], mutable_production[index], 4U);
		memcpy(state->price_raw[index], mutable_price[index], 4U);
		state->price[index] = qb_mbf32_decode(mutable_price[index]);
		state->production_raised[index] = raised[index];
	}
	state->current_minute = minute;
	state->elapsed = elapsed;
	yt_port_decode(&state->port, &updated);
	state->complete = true;
	return true;
}

static bool
port_report_append(uint8_t *row, size_t capacity, size_t *position,
    const void *text, size_t length)
{
	if (*position > capacity || length > capacity - *position
	    || (length != 0U && text == NULL))
		return false;
	if (length != 0U)
		memcpy(row + *position, text, length);
	*position += length;
	return true;
}

static bool
port_report_field_length(const uint8_t raw[4], uint8_t conversion_mode,
    size_t *length, struct yt_error *error, const char *operation)
{
	bool overflow;
	int32_t converted;

	converted = qb_cint_mbf32(raw, conversion_mode, &overflow);
	if (overflow || converted < 0)
		return startup_configuration_error(error, YT_RANGE, operation);
	*length = (size_t)converted;
	if (*length > YT_TEXT_FIELD_SIZE)
		*length = YT_TEXT_FIELD_SIZE;
	return true;
}

static bool
port_report_right_raw(const uint8_t *source, size_t source_length,
    size_t width, uint8_t *rendered)
{
	size_t amount = source_length < width ? source_length : width;
	size_t padding = width - amount;

	if (source == NULL || rendered == NULL)
		return false;
	memset(rendered, ' ', padding);
	memcpy(rendered + padding, source + source_length - amount, amount);
	return true;
}

bool
yt_port_report_compose(const struct yt_port_market_state *market,
    const struct yt_player *current_player,
    const struct yt_port *report_port, uint8_t conversion_mode,
    const uint8_t date[10], const uint8_t time_text[8],
    struct yt_port_report_text *report, struct yt_error *error)
{
	static const uint8_t title_prefix[] = "Commerce report for ";
	static const uint8_t title_separator[] = ": ";
	static const uint8_t commodity[3][14] = {
		"Ore..........", "Organics.....", "Equipment...."
	};
	static const uint8_t buying[] = "  Buying ";
	static const uint8_t selling[] = "  Selling";
	static const uint8_t padding[] = "    ";
	static const size_t hold_offset[3] = {YT_F69, YT_F73, YT_F77};
	uint8_t promoted_hold[8];
	uint8_t floored_capacity[8];
	char number[96];
	int formatted_length;
	size_t name_length;
	size_t number_length;
	size_t index;

	if (market == NULL || current_player == NULL || report_port == NULL
	    || date == NULL || time_text == NULL || report == NULL)
		return startup_configuration_error(error, YT_INVALID,
		    "port report arguments");
	memset(report, 0, sizeof(*report));
	if (!port_report_field_length(report_port->record.bytes + YT_F85,
	    conversion_mode, &name_length, error, "port report name length"))
		return false;
	if (!port_report_append(report->title, sizeof(report->title),
	    &report->title_length, title_prefix, sizeof(title_prefix) - 1U)
	    || !port_report_append(report->title, sizeof(report->title),
	    &report->title_length, report_port->record.bytes, name_length)
	    || !port_report_append(report->title, sizeof(report->title),
	    &report->title_length, title_separator,
	    sizeof(title_separator) - 1U)
	    || !port_report_append(report->title, sizeof(report->title),
	    &report->title_length, date, 10U)
	    || !port_report_append(report->title, sizeof(report->title),
	    &report->title_length, " ", 1U)
	    || !port_report_append(report->title, sizeof(report->title),
	    &report->title_length, time_text, 8U))
		return startup_configuration_error(error, YT_RANGE,
		    "port report title composition");

	for (index = 0U; index < 3U; ++index) {
		struct yt_port_report_item *item = &report->item[index];
		const uint8_t *status;
		size_t position = 0U;

		if (market->port.factor[index] < 0.0f) {
			status = buying;
			item->foreground = 3.0f;
		}
		else {
			status = selling;
			item->foreground = 2.0f;
		}
		if (!port_report_append(item->name_status,
		    sizeof(item->name_status), &position, commodity[index],
		    sizeof(commodity[index]) - 1U)
		    || !port_report_append(item->name_status,
		    sizeof(item->name_status), &position, status,
		    sizeof(buying) - 1U))
			return startup_configuration_error(error, YT_RANGE,
			    "port report item composition");
		if (qb_mbf64_floor_raw(market->capacity_raw[index],
		    floored_capacity) != QB_MBF_OK)
			return startup_configuration_error(error, YT_RANGE,
			    "port report stock INT");
		formatted_length = qb_str_mbf64(number, sizeof(number),
		    floored_capacity);
		if (formatted_length < 0
		    || !port_report_right_raw((const uint8_t *)number,
		    (size_t)formatted_length, sizeof(item->capacity),
		    item->capacity))
			return startup_configuration_error(error, YT_RANGE,
			    "port report stock formatting");
		yt_port_mbf64_promote_single(current_player->record.bytes
		    + hold_offset[index], promoted_hold);
		formatted_length = qb_str_mbf64(number, sizeof(number),
		    promoted_hold);
		if (formatted_length < 0
		    || !port_report_right_raw((const uint8_t *)number,
		    (size_t)formatted_length, sizeof(item->hold), item->hold))
			return startup_configuration_error(error, YT_RANGE,
			    "port report hold formatting");
		formatted_length = qb_str_mbf32(number, sizeof(number),
		    market->price_raw[index]);
		if (formatted_length < 0)
			return startup_configuration_error(error, YT_RANGE,
			    "port report price formatting");
		number_length = (size_t)formatted_length;
		position = 0U;
		if (!port_report_append(item->price, sizeof(item->price),
		    &position, number, number_length)
		    || !port_report_append(item->price, sizeof(item->price),
		    &position, padding, sizeof(padding) - 1U))
			return startup_configuration_error(error, YT_RANGE,
			    "port report price composition");
		item->price_length = position;
	}
	return true;
}

int
yt_computer_selector_position(const char *command)
{
	static const char selector[] = "+!LMP?123459";
	const char *match;

	if (command == NULL)
		return 0;
	match = strstr(selector, command);
	return match == NULL ? 0 : (int)(match - selector) + 1;
}

void
yt_trade_treasury_overlay(struct yt_port *port, float receipt)
{
	volatile float updated;

	if (port == NULL)
		return;
	updated = port->treasury + receipt;
	port->treasury = updated;
	(void)yt_record_set_number(&port->record, YT_F89, updated);
}

void
yt_trade_holds_overlay(struct yt_player *player, size_t commodity,
    float quantity, float direction)
{
	float *selected;
	volatile float single_delta;
	volatile double updated;

	if (player == NULL || commodity >= 3U)
		return;
	selected = commodity == 0U ? &player->ore
	    : commodity == 1U ? &player->organics : &player->equipment;
	single_delta = quantity * direction;
	updated = (double)*selected + (double)single_delta;
	*selected = (float)updated;
	(void)yt_record_set_number(&player->record, YT_F69, player->ore);
	(void)yt_record_set_number(&player->record, YT_F73, player->organics);
	(void)yt_record_set_number(&player->record, YT_F77, player->equipment);
}

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

static float
take_all_single_add(float left, float right)
{
	volatile float result = left + right;

	return result;
}

static double
take_all_double_add(double left, double right)
{
	volatile double result = left + right;

	return result;
}

static double
take_all_double_sub(double left, double right)
{
	volatile double result = left - right;

	return result;
}

static float
take_all_single_mul(float left, float right)
{
	volatile float result = left * right;

	return result;
}

static float
take_all_single_sub(float left, float right)
{
	volatile float result = left - right;

	return result;
}

static float
take_all_single_div(float left, float right)
{
	volatile float result = left / right;

	return result;
}

static double
take_all_double_div(double left, double right)
{
	volatile double result = left / right;

	return result;
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
		*selected = take_all_single_add(*selected, amount);
	else
		*selected = (float)take_all_double_add((double)*selected,
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
	*selected = (float)take_all_double_sub(cached_quantity,
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
	player->fighters = (float)take_all_double_add(
	    (double)player->fighters, amount[4]);
	player->missiles = take_all_single_add(player->missiles,
	    (float)amount[5]);
	player->mines = take_all_single_add(player->mines, (float)amount[6]);
	player->plasma = take_all_single_add(player->plasma, (float)amount[9]);
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

		*selected = (float)take_all_double_sub(
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
	free_double = take_all_double_sub((double)player->holds,
	    (double)player->ore);
	free_double = take_all_double_sub(free_double,
	    (double)player->organics);
	free_double = take_all_double_sub(free_double,
	    (double)player->equipment);
	free_holds = (float)free_double;
	amount = (float)floor(cached_quantity);
	if (free_holds < amount)
		amount = free_holds;
	selected = take_all_player_item(player, item);
	*selected = (float)take_all_double_add((double)*selected,
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
	*selected = (float)take_all_double_sub(cached_quantity,
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
		float threshold = take_all_single_mul(rate[item], 10.0f);
		double total = take_all_double_add(quantity[item], held[index]);

		if (total > (double)threshold)
			rate[item] = (float)take_all_double_add(
			    take_all_double_div(floor(total), 10.0), 1.0);
		quantity[item] = take_all_double_add(quantity[item], held[index]);
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

		planet->production[index] = take_all_single_sub(rate[item],
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
		*selected = (float)take_all_double_add(cached_quantity,
		    (double)cached_amount);
}

void
yt_planet_transfer_fighter_player_overlay(struct yt_player *player,
    float cached_fighters, float amount)
{
	if (player != NULL)
		player->fighters = (float)take_all_double_sub(
		    (double)cached_fighters, (double)amount);
}

void
yt_planet_transfer_fighter_planet_overlay(struct yt_planet *planet,
    double cached_quantity, float amount)
{
	if (planet != NULL)
		planet->fighters = (float)take_all_double_add(cached_quantity,
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
yt_planet_transfer_fighter_amount(const char *response, float *amount,
    struct yt_error *error)
{
	struct qb_val_result parsed;
	uint8_t raw[4];
	volatile float candidate;
	enum qb_mbf_status status;

	if (response == NULL || amount == NULL)
		return startup_configuration_error(error, YT_INVALID,
		    "planet Transfer fighter amount arguments");
	parsed = qb_val(response);
	if (parsed.overflow)
		return startup_configuration_error(error, YT_RANGE,
		    "planet Transfer fighter VAL");
	candidate = (float)(parsed.valid ? parsed.value : 0.0);
	status = qb_mbf32_encode(candidate, raw);
	if (status == QB_MBF_OVERFLOW)
		return startup_configuration_error(error, YT_RANGE,
		    "planet Transfer fighter CSNG");
	*amount = status == QB_MBF_UNDERFLOW ? 0.0f : qb_mbf32_decode(raw);
	return true;
}

double
yt_planet_bank_available(float cached_credits, float cached_bank)
{
	return take_all_double_add((double)cached_credits,
	    (double)cached_bank);
}

double
yt_planet_bank_remaining(float cached_credits, float cached_bank,
    double target)
{
	double after_target = take_all_double_sub((double)cached_credits,
	    target);

	return take_all_double_add(after_target, (double)cached_bank);
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
	return (float)take_all_double_sub((double)cached_bank, target);
}

void
yt_planet_bank_credit_overlay(struct yt_player *player, float argument)
{
	if (player != NULL)
		player->credits = floorf(take_all_single_add(player->credits,
		    argument));
}

double
yt_planet_productivity_units(double spend)
{
	return take_all_double_div(spend, 250.0);
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
	old_sum = take_all_single_add(take_all_single_add(rate[1], rate[2]),
	    rate[3]);
	for (index = 1; index <= 3U; ++index)
		rate[index] = (float)take_all_double_add((double)rate[index],
		    units);
	new_sum = take_all_single_add(take_all_single_add(rate[1], rate[2]),
	    rate[3]);
	delta[0] = take_all_single_sub(floorf(new_sum), floorf(old_sum));
	new_value = floorf(take_all_single_div(new_sum, 2500.0f));
	old_value = floorf(take_all_single_div(old_sum, 2500.0f));
	delta[1] = take_all_single_sub(new_value, old_value);
	new_value = floorf(take_all_single_div(new_sum, 25000.0f));
	old_value = floorf(take_all_single_div(old_sum, 25000.0f));
	delta[2] = take_all_single_sub(new_value, old_value);
	new_value = floorf(take_all_single_mul(new_sum, plasma_multiplier));
	old_value = floorf(take_all_single_mul(old_sum, plasma_multiplier));
	delta[3] = take_all_single_sub(new_value, old_value);
}

float
yt_planet_productivity_credit_argument(double units)
{
	volatile double cost = units * 250.0;
	volatile float single_cost = (float)cost;

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
yt_planet_rename_protected(float current_record, float planet_offset,
    float total_record_marker)
{
	volatile float relative = current_record - planet_offset;
	volatile float marker_minus_one = total_record_marker + -1.0f;

	return relative == 1.0f || current_record == total_record_marker
	    || current_record == marker_minus_one;
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
	planet->name_length = (float)length;
}

bool
yt_clearance_candidate_needed(size_t item, float trigger_draw,
    float discount, bool create)
{
	static const float trigger[4] = {
		0.7900000214576721f, 0.7900000214576721f,
		0.8399999737739563f, 0.8899999856948853f
	};

	return item < 4U && trigger_draw > trigger[item]
	    && discount == 0.0f && create;
}

bool
yt_clearance_normalize(size_t item, float *discount)
{
	static const float maximum[4] = {
		0.9509999752044678f, 0.9800000190734863f,
		0.800000011920929f, 0.8999999761581421f
	};

	if (item >= 4U || discount == NULL)
		return false;
	if (*discount < 0.10000000149011612f
	    || *discount > maximum[item]) {
		*discount = 0.0f;
		return false;
	}
	return true;
}

float
yt_clearance_percentage(float discount)
{
	return floorf(take_all_single_mul(100.0f, discount));
}

void
yt_earth_prices(const float discount[4], float price[4])
{
	if (discount == NULL || price == NULL)
		return;
	price[0] = floorf(take_all_single_sub(250.0f,
	    take_all_single_mul(250.0f, discount[0])));
	price[1] = floorf(take_all_single_sub(50.0f,
	    take_all_single_mul(50.0f, discount[1])));
	price[2] = floorf(take_all_single_mul(50.0f,
	    take_all_single_sub(1.0f, discount[2])));
	price[3] = floorf(take_all_single_mul(200.5f,
	    take_all_single_sub(1.0f, discount[3])));
}

double
yt_earth_affordable(float credits, float price)
{
	return floor(take_all_double_div((double)credits, (double)price));
}

int
yt_earth_selector_position(const char *command)
{
	const char *position;

	if (command == NULL)
		return 0;
	position = strstr("LM0C", command);
	return position == NULL ? 0 : (int)(position - "LM0C") + 1;
}

float
yt_earth_purchase_quantity(double value)
{
	return (float)floor(value);
}

float
yt_earth_receipt_amount(float owner, int buyer_record, float cost)
{
	if (owner == 0.0f)
		return 0.0f;
	if (owner == (float)buyer_record)
		return floorf(take_all_single_mul(0.009999999776482582f, cost));
	return cost;
}

float
yt_earth_cloak_points(float cloak)
{
	return floorf(take_all_single_mul(50.0f, cloak));
}

float
yt_earth_cloak_default(float deficit, float credits)
{
	if (take_all_single_mul(deficit, 1000.0f) > credits)
		return (float)yt_earth_affordable(credits, 1000.0f);
	return deficit;
}

float
yt_earth_cloak_overlay(float points, float quantity)
{
	return take_all_single_div(floorf(take_all_single_add(points, quantity)),
	    50.0f);
}

void
yt_earth_supply_overlay(struct yt_player *player, int choice, float quantity)
{
	if (player == NULL)
		return;
	if (choice == 3)
		player->fighters = take_all_single_add(player->fighters, quantity);
	else if (choice == 7)
		player->ground_forces = floorf(take_all_single_add(
		    player->ground_forces, quantity));
	else if (choice == 8)
		player->shields = floorf(take_all_single_add(
		    player->shields, quantity));
}

int
yt_lottery_match_count(const int winning[6], const char ticket[6],
    bool matched_winning[6])
{
	bool used_winning[6] = {0};
	bool used_ticket[6] = {0};
	int matches = 0;
	int index;

	if (winning == NULL || ticket == NULL || matched_winning == NULL)
		return 0;
	memset(matched_winning, 0, 6U * sizeof(*matched_winning));
	for (index = 0; index < 6; ++index) {
		int candidate;

		for (candidate = 0; candidate < 6; ++candidate) {
			if (!used_winning[index] && !used_ticket[candidate]
			    && winning[index] == ticket[candidate] - '0') {
				used_winning[index] = true;
				used_ticket[candidate] = true;
				matched_winning[index] = true;
				++matches;
				break;
			}
		}
	}
	return matches;
}

float
yt_lottery_award(int matches)
{
	static const float awards[6] = {
		100.0f, 1000.0f, 10000.0f, 100000.0f,
		1000000.0f, 100000000.0f
	};

	return matches < 1 || matches > 6 ? 0.0f : awards[matches - 1];
}

bool
yt_player_stored_name(const struct yt_player *player,
    uint8_t name[YT_TEXT_FIELD_SIZE], size_t *length,
    struct yt_error *error)
{
	bool overflow;
	int requested = (int)qb_cint_mbf32(
	    player->record.bytes + YT_F85, 0U, &overflow);
	size_t stored;

	if (length != NULL)
		*length = 0;
	if (overflow || requested < 0) {
		if (error != NULL) {
			error->status = YT_RANGE;
			snprintf(error->operation, sizeof(error->operation),
			    "%s", overflow ? "player name CINT"
			    : "player name LEFT$ length");
		}
		return false;
	}
	stored = (size_t)requested;
	if (stored > YT_TEXT_FIELD_SIZE)
		stored = YT_TEXT_FIELD_SIZE;
	if (stored > 0 && name != NULL)
		memcpy(name, player->record.bytes, stored);
	if (length != NULL)
		*length = stored;
	return true;
}

static bool
stored_record_name(const struct yt_record *record,
    const char *operation, uint8_t name[YT_TEXT_FIELD_SIZE], size_t *length,
    struct yt_error *error)
{
	bool overflow;
	int requested = (int)qb_cint_mbf32(
	    record->bytes + YT_F85, 0U, &overflow);
	size_t stored;

	if (length != NULL)
		*length = 0U;
	if (overflow || requested < 0) {
		if (error != NULL) {
			error->status = YT_RANGE;
			snprintf(error->operation, sizeof(error->operation), "%s",
			    operation);
		}
		return false;
	}
	stored = (size_t)requested;
	if (stored > YT_TEXT_FIELD_SIZE)
		stored = YT_TEXT_FIELD_SIZE;
	if (stored > 0U && name != NULL)
		memcpy(name, record->bytes, stored);
	if (length != NULL)
		*length = stored;
	return true;
}

bool
yt_port_stored_name(const struct yt_port *port,
    uint8_t name[YT_TEXT_FIELD_SIZE], size_t *length,
    struct yt_error *error)
{
	return stored_record_name(&port->record,
	    "port name LEFT$ length", name, length, error);
}

bool
yt_planet_stored_name(const struct yt_planet *planet,
    uint8_t name[YT_TEXT_FIELD_SIZE], size_t *length,
    struct yt_error *error)
{
	return stored_record_name(&planet->record,
	    "planet name LEFT$ length", name, length, error);
}

struct sector_row_builder {
	uint8_t *row;
	size_t capacity;
	size_t length;
};

static bool
sector_row_append(struct sector_row_builder *builder, const void *data,
    size_t length)
{
	if (length > builder->capacity - builder->length
	    || (length != 0U && (builder->row == NULL || data == NULL)))
		return false;
	if (length != 0U)
		memcpy(builder->row + builder->length, data, length);
	builder->length += length;
	return true;
}

static bool
sector_row_number(struct sector_row_builder *builder, float value,
    bool promoted)
{
	char number[64];
	int length = promoted
	    ? qb_str_double(number, sizeof(number), (double)value)
	    : qb_str_single(number, sizeof(number), value);

	return length >= 0 && sector_row_append(builder, number, (size_t)length);
}

bool
yt_sector_mine_warning_row(float mines, uint8_t *row,
    size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "** WARNING! SECTOR HAS";
	static const uint8_t suffix[] = " MINES! **";
	struct sector_row_builder builder = {row, capacity, 0U};

	if (length != NULL)
		*length = 0U;
	if (!sector_row_append(&builder, prefix, sizeof(prefix) - 1U)
	    || !sector_row_number(&builder, mines, false)
	    || !sector_row_append(&builder, suffix, sizeof(suffix) - 1U))
		return false;
	if (length != NULL)
		*length = builder.length;
	return true;
}

bool
yt_sector_candidate_eligible(int candidate, int current_player_record,
    float cached_sector, float logical_sector)
{
	return candidate != current_player_record
	    && cached_sector == logical_sector;
}

bool
yt_sector_cloak_revealed(float draw, float cached_cloak)
{
	return draw > cached_cloak && cached_cloak != 0.0f;
}

size_t
yt_sector_sensor_targets(const float caller_warps[6], float targets[6])
{
	size_t count = 0U;
	size_t slot;

	if (caller_warps == NULL || targets == NULL)
		return 0U;
	for (slot = 0U; slot < 6U; ++slot) {
		if (caller_warps[slot] != 0.0f)
			targets[count++] = caller_warps[slot];
	}
	return count;
}

bool
yt_sector_port_row(const struct yt_port *port, uint8_t *row,
    size_t capacity, size_t *length, struct yt_error *error)
{
	static const uint8_t prefix[] = "Port: ";
	static const uint8_t separator[] = ", Selling: ";
	static const uint8_t equipment[] = "Equ";
	static const uint8_t organics[] = "Org";
	static const uint8_t ore[] = "Ore";
	const uint8_t *commodity = ore;
	uint8_t name[YT_TEXT_FIELD_SIZE];
	size_t name_length;
	struct sector_row_builder builder = {row, capacity, 0U};

	if (length != NULL)
		*length = 0U;
	if (port == NULL || !yt_port_stored_name(port, name, &name_length,
	    error))
		return false;
	if (port->commodity_class == 1.0f)
		commodity = equipment;
	else if (port->commodity_class == 2.0f)
		commodity = organics;
	if (!sector_row_append(&builder, prefix, sizeof(prefix) - 1U)
	    || !sector_row_append(&builder, name, name_length)
	    || !sector_row_append(&builder, separator, sizeof(separator) - 1U)
	    || !sector_row_append(&builder, commodity, sizeof(ore) - 1U))
		return false;
	if (length != NULL)
		*length = builder.length;
	return true;
}

bool
yt_sector_planet_row(const struct yt_planet *planet, uint8_t *row,
    size_t capacity, size_t *length, struct yt_error *error)
{
	static const uint8_t prefix[] = "Planet: ";
	static const uint8_t separator[] = " * Forces:";
	uint8_t name[YT_TEXT_FIELD_SIZE];
	size_t name_length;
	struct sector_row_builder builder = {row, capacity, 0U};
	float forces;

	if (length != NULL)
		*length = 0U;
	if (planet == NULL || !yt_planet_stored_name(planet, name,
	    &name_length, error))
		return false;
	forces = (float)qb_int((double)planet->ground_forces);
	if (!sector_row_append(&builder, prefix, sizeof(prefix) - 1U)
	    || !sector_row_append(&builder, name, name_length)
	    || !sector_row_append(&builder, separator, sizeof(separator) - 1U)
	    || !sector_row_number(&builder, forces, false))
		return false;
	if (length != NULL)
		*length = builder.length;
	return true;
}

bool
yt_sector_player_row(const struct yt_player *player, uint8_t *row,
    size_t capacity, size_t *length, struct yt_error *error)
{
	static const uint8_t indent[] = "    ";
	static const uint8_t team[] = " - Team:";
	static const uint8_t fighters[] = " - Fighters:";
	static const uint8_t shields[] = " - Shields:";
	uint8_t name[YT_TEXT_FIELD_SIZE];
	size_t name_length;
	struct sector_row_builder builder = {row, capacity, 0U};

	if (length != NULL)
		*length = 0U;
	if (player == NULL || !yt_player_stored_name(player, name,
	    &name_length, error))
		return false;
	if (!sector_row_append(&builder, indent, sizeof(indent) - 1U)
	    || !sector_row_append(&builder, name, name_length))
		return false;
	if (player->team > 0.0f
	    && (!sector_row_append(&builder, team, sizeof(team) - 1U)
	    || !sector_row_number(&builder, player->team, false)))
		return false;
	if (!sector_row_append(&builder, fighters, sizeof(fighters) - 1U)
	    || !sector_row_number(&builder, player->fighters, true)
	    || !sector_row_append(&builder, shields, sizeof(shields) - 1U)
	    || !sector_row_number(&builder, player->shields, false))
		return false;
	if (length != NULL)
		*length = builder.length;
	return true;
}

bool
yt_sector_fighter_row(const struct yt_sector *sector,
    int current_player_record, const struct yt_player *owner,
    const struct yt_sector *team_overlay, uint8_t *row, size_t capacity,
    size_t *length, uint8_t *scratch, size_t scratch_capacity,
    size_t *scratch_length, bool *scratch_changed, struct yt_error *error)
{
	static const uint8_t belonging[] = " (Belong to ";
	static const uint8_t xannor[] = "The Xannor";
	static const uint8_t mercenaries[] = "Mercenaries";
	static const uint8_t self[] = "YOU)";
	static const uint8_t team_prefix[] = " Team [";
	static const uint8_t overlay_prefix[] = " [";
	uint8_t owner_name[YT_TEXT_FIELD_SIZE];
	size_t owner_name_length = 0U;
	struct sector_row_builder builder = {row, capacity, 0U};
	struct sector_row_builder scratch_builder = {
		scratch, scratch_capacity, 0U
	};
	bool changed = false;

	if (length != NULL)
		*length = 0U;
	if (scratch_changed != NULL)
		*scratch_changed = false;
	if (sector == NULL
	    || !sector_row_number(&builder, sector->fighters, true)
	    || !sector_row_append(&builder, belonging,
	    sizeof(belonging) - 1U))
		return false;
	if (sector->fighter_owner == (float)current_player_record) {
		if (!sector_row_append(&builder, self, sizeof(self) - 1U))
			return false;
	}
	else {
		if (sector->fighter_owner == -1.0f) {
			if (!sector_row_append(&scratch_builder, xannor,
			    sizeof(xannor) - 1U))
				return false;
			changed = true;
		}
		else if (sector->fighter_owner == -2.0f) {
			if (!sector_row_append(&scratch_builder, mercenaries,
			    sizeof(mercenaries) - 1U))
				return false;
			changed = true;
		}
		else {
			char number[64];
			int number_length;
			bool overflow;
			int team_name_length;
			size_t stored_team_length;

			if (owner == NULL || !yt_player_stored_name(owner,
			    owner_name, &owner_name_length, error)
			    || !sector_row_append(&scratch_builder, owner_name,
			    owner_name_length))
				return false;
			changed = true;
			if (owner->team != 0.0f) {
				number_length = qb_str_single(number, sizeof(number),
				    owner->team);
				if (number_length < 1 || team_overlay == NULL
				    || !sector_row_append(&scratch_builder,
				    team_prefix, sizeof(team_prefix) - 1U)
				    || !sector_row_append(&scratch_builder,
				    number + 1, (size_t)number_length - 1U)
				    || !sector_row_append(&scratch_builder, "]", 1U))
					return false;
				team_name_length = (int)qb_cint_mbf32(
				    team_overlay->record.bytes + YT_F73, 0U,
				    &overflow);
				if (overflow || team_name_length < 0) {
					if (error != NULL) {
						error->status = YT_RANGE;
						snprintf(error->operation,
						    sizeof(error->operation), "%s",
						    "team name LEFT$ length");
					}
					return false;
				}
				stored_team_length = (size_t)team_name_length;
				if (stored_team_length > YT_TEXT_FIELD_SIZE)
					stored_team_length = YT_TEXT_FIELD_SIZE;
				if (stored_team_length > 0U
				    && (!sector_row_append(&scratch_builder,
				    overlay_prefix, sizeof(overlay_prefix) - 1U)
				    || !sector_row_append(&scratch_builder,
				    team_overlay->record.bytes, stored_team_length)
				    || !sector_row_append(&scratch_builder, "]", 1U)))
					return false;
			}
		}
		if (!sector_row_append(&builder, scratch_builder.row,
		    scratch_builder.length)
		    || !sector_row_append(&builder, ")", 1U))
			return false;
	}
	if (length != NULL)
		*length = builder.length;
	if (changed && scratch_length != NULL)
		*scratch_length = scratch_builder.length;
	if (scratch_changed != NULL)
		*scratch_changed = changed;
	return true;
}

bool
yt_projectile_defense_row(float sector, const uint8_t *owner,
    size_t owner_length, double fighters, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t prefix[] = "Sector:";
	static const uint8_t owner_prefix[] = " defended by ";
	static const uint8_t fighter_prefix[] = " with";
	static const uint8_t suffix[] = " fighters.";
	char sector_text[64];
	char fighter_text[64];
	int sector_length;
	int fighter_length;
	size_t needed;
	size_t position = 0U;

	if (length == NULL || (owner == NULL && owner_length != 0U))
		return false;
	*length = 0U;
	sector_length = qb_str_single(sector_text, sizeof(sector_text), sector);
	fighter_length = qb_str_double(fighter_text, sizeof(fighter_text),
	    fighters);
	if (sector_length < 0 || fighter_length < 0)
		return false;
	needed = sizeof(prefix) - 1U + (size_t)sector_length
	    + sizeof(owner_prefix) - 1U + owner_length
	    + sizeof(fighter_prefix) - 1U + (size_t)fighter_length
	    + sizeof(suffix) - 1U;
	if (needed > capacity || (needed != 0U && row == NULL))
		return false;
	memcpy(row + position, prefix, sizeof(prefix) - 1U);
	position += sizeof(prefix) - 1U;
	memcpy(row + position, sector_text, (size_t)sector_length);
	position += (size_t)sector_length;
	memcpy(row + position, owner_prefix, sizeof(owner_prefix) - 1U);
	position += sizeof(owner_prefix) - 1U;
	if (owner_length != 0U)
		memcpy(row + position, owner, owner_length);
	position += owner_length;
	memcpy(row + position, fighter_prefix, sizeof(fighter_prefix) - 1U);
	position += sizeof(fighter_prefix) - 1U;
	memcpy(row + position, fighter_text, (size_t)fighter_length);
	position += (size_t)fighter_length;
	memcpy(row + position, suffix, sizeof(suffix) - 1U);
	position += sizeof(suffix) - 1U;
	*length = position;
	return true;
}

bool
yt_projectile_candidate_admitted(int candidate, float cached_cloak,
    int xannor_provoker)
{
	return (cached_cloak == 0.0f || candidate == xannor_provoker)
	    && (xannor_provoker == 0 || candidate == xannor_provoker);
}

enum yt_projectile_candidate_route
yt_projectile_candidate_route(int candidate, int shooter,
    float cached_sector, float sector, float remaining)
{
	if (cached_sector != sector)
		return YT_PROJECTILE_CANDIDATE_SKIP;
	if (remaining <= 0.0f)
		return YT_PROJECTILE_CANDIDATE_TERMINATE;
	if (candidate == shooter)
		return YT_PROJECTILE_CANDIDATE_SKIP;
	return YT_PROJECTILE_CANDIDATE_FRIENDSHIP;
}

bool
yt_projectile_player_survives(float shields)
{
	return shields >= 1.0f;
}

bool
yt_projectile_salvage_admitted(int counterattack, int xannor_provoker)
{
	return counterattack == 0 && xannor_provoker == 0;
}

enum yt_projectile_death_route
yt_projectile_death_continuation(float remaining, float saved_mines)
{
	if (remaining > 0.0f && saved_mines > 0.0f)
		return YT_PROJECTILE_DEATH_REENTER_MINES;
	if (remaining < 1.0f)
		return YT_PROJECTILE_DEATH_RETURN;
	return YT_PROJECTILE_DEATH_NEXT_PLAYER;
}

bool
yt_projectile_survivor_sets_counterattack(int shooter)
{
	return shooter != -1;
}

bool
yt_projectile_survivor_store_counterattack(int shooter, int player_record,
    int *counterattack, uint8_t raw[4])
{
	if (counterattack == NULL || raw == NULL
	    || !yt_projectile_survivor_sets_counterattack(shooter))
		return false;
	if (qb_mbf32_encode((float)player_record, raw) != QB_MBF_OK)
		return false;
	*counterattack = player_record;
	return true;
}

bool
yt_projectile_damage_iteration(float counter, float saved_missiles)
{
	return counter <= saved_missiles;
}

static float
projectile_single_add(float left, float right)
{
	volatile float result = left + right;

	return result;
}

static float
projectile_single_sub(float left, float right)
{
	volatile float result = left - right;

	return result;
}

static float
projectile_single_mul(float left, float right)
{
	volatile float result = left * right;

	return result;
}

bool
yt_projectile_is_black_hole(float hop, float first, float second)
{
	return hop == first || hop == second;
}

bool
yt_projectile_cruise_reroute_row(float hop, uint8_t *row,
    size_t capacity, size_t *length)
{
	static const uint8_t prefix[] =
	    "The missiles are deflected by a black hole in sector";
	static const uint8_t suffix[] = "!";
	char number[64];
	int number_length;
	size_t row_length;

	if (row == NULL || length == NULL)
		return false;
	number_length = qb_str_single(number, sizeof(number), hop);
	if (number_length < 0
	    || sizeof(prefix) - 1U + (size_t)number_length + sizeof(suffix) - 1U
	    > capacity)
		return false;
	memcpy(row, prefix, sizeof(prefix) - 1U);
	memcpy(row + sizeof(prefix) - 1U, number, (size_t)number_length);
	row_length = sizeof(prefix) - 1U + (size_t)number_length;
	memcpy(row + row_length, suffix, sizeof(suffix) - 1U);
	row_length += sizeof(suffix) - 1U;
	*length = row_length;
	return true;
}

float
yt_projectile_cruise_reroute_destination(float draw,
    float sector_record_offset, float port_record_offset)
{
	float span = projectile_single_sub(port_record_offset,
	    sector_record_offset);
	float selected = floorf(projectile_single_mul(draw, span));

	return projectile_single_add(selected, 1.0f);
}

bool
yt_projectile_union_police_admitted(float hop, float destination,
    int counterattack, int xannor_provoker)
{
	return hop < 8.0f && destination < 8.0f
	    && counterattack == 0 && xannor_provoker == 0;
}

bool
yt_projectile_sector_has_presence(const struct yt_sector *sector,
    int sector_number, int last_player,
    const struct yt_player_cache *player_cache, int xannor_provoker)
{
	bool present;
	int player;

	if (sector == NULL || player_cache == NULL)
		return false;
	present = sector->mines > 0.0f || sector->fighters > 0.0f
	    || sector->port > 0.0f || sector->planet > 0.0f;
	for (player = YT_PLAYER_FIRST_RECORD; player <= last_player; ++player) {
		if (yt_player_cache_value(player_cache, player,
		    YT_PLAYER_CACHE_SECTOR) == (float)sector_number
		    && (yt_player_cache_value(player_cache, player,
		    YT_PLAYER_CACHE_CLOAK) == 0.0f
		    || player == xannor_provoker))
			return true;
	}
	return present;
}




bool
yt_projectile_sector_mine_hit_row(double mines, float sector,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t hit_prefix[] = "The missiles hit";
	static const uint8_t hit_middle[] = " SECTOR MINES in sector";
	char mine_text[64];
	char sector_text[64];
	int mine_length;
	int sector_length;
	size_t row_length;

	if (row == NULL || length == NULL)
		return false;
	sector_length = qb_str_single(sector_text, sizeof(sector_text),
	    sector);
	mine_length = qb_str_double(mine_text, sizeof(mine_text), mines);
	if (sector_length < 0 || mine_length < 0
	    || sizeof(hit_prefix) - 1U + (size_t)mine_length
	    + sizeof(hit_middle) - 1U + (size_t)sector_length + 1U
	    > capacity)
		return false;
	memcpy(row, hit_prefix, sizeof(hit_prefix) - 1U);
	memcpy(row + sizeof(hit_prefix) - 1U, mine_text,
	    (size_t)mine_length);
	row_length = sizeof(hit_prefix) - 1U + (size_t)mine_length;
	memcpy(row + row_length, hit_middle, sizeof(hit_middle) - 1U);
	row_length += sizeof(hit_middle) - 1U;
	memcpy(row + row_length, sector_text, (size_t)sector_length);
	row_length += (size_t)sector_length;
	row[row_length++] = '!';
	*length = row_length;
	return true;
}

bool
yt_projectile_sector_mine_news_row(const uint8_t *shooter,
    size_t shooter_length, float sector, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t middle[] =
	    "'s Missiles hit sector mines in sector";
	char sector_text[64];
	int sector_length;
	size_t row_length = 0U;

	if (row == NULL || length == NULL
	    || (shooter == NULL && shooter_length != 0U))
		return false;
	sector_length = qb_str_single(sector_text, sizeof(sector_text), sector);
	if (sector_length < 0 || shooter_length + sizeof(middle) - 1U
	    + (size_t)sector_length + 1U > capacity)
		return false;
	if (shooter_length != 0U) {
		memcpy(row, shooter, shooter_length);
		row_length = shooter_length;
	}
	memcpy(row + row_length, middle, sizeof(middle) - 1U);
	row_length += sizeof(middle) - 1U;
	memcpy(row + row_length, sector_text, (size_t)sector_length);
	row_length += (size_t)sector_length;
	row[row_length++] = '!';
	*length = row_length;
	return true;
}

bool
yt_projectile_sector_mine_destroyed_row(float destroyed,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "The missile";
	static const uint8_t middle[] = " destroyed";
	static const uint8_t mine[] = " mine";
	char number[64];
	int number_length;
	size_t plural = destroyed > 1.0f ? 1U : 0U;
	size_t row_length;

	if (row == NULL || length == NULL)
		return false;
	number_length = qb_str_single(number, sizeof(number), destroyed);
	if (number_length < 0 || sizeof(prefix) - 1U + plural
	    + sizeof(middle) - 1U + (size_t)number_length
	    + sizeof(mine) - 1U + plural + 1U > capacity)
		return false;
	memcpy(row, prefix, sizeof(prefix) - 1U);
	row_length = sizeof(prefix) - 1U;
	if (plural != 0U)
		row[row_length++] = 's';
	memcpy(row + row_length, middle, sizeof(middle) - 1U);
	row_length += sizeof(middle) - 1U;
	memcpy(row + row_length, number, (size_t)number_length);
	row_length += (size_t)number_length;
	memcpy(row + row_length, mine, sizeof(mine) - 1U);
	row_length += sizeof(mine) - 1U;
	if (plural != 0U)
		row[row_length++] = 's';
	row[row_length++] = '!';
	*length = row_length;
	return true;
}

bool
yt_projectile_survivor_overlay(struct yt_player *player, float shields,
    double fighters, float scanner, bool scanner_disabled)
{
	static const uint8_t scanner_zero[4] = {
		0x00, 0x00, 0x48, 0x00
	};

	if (player == NULL)
		return false;
	player->shields = shields;
	player->fighters = (float)fighters;
	player->danger_scanner = scanner_disabled ? 0.0f : scanner;
	return yt_record_set_number(&player->record, YT_F53, shields)
	    && yt_record_set_number(&player->record, YT_F61,
	    player->fighters)
	    && (scanner_disabled
	    ? yt_record_set_raw_number(&player->record, YT_F93, scanner_zero)
	    : yt_record_set_number(&player->record, YT_F93, scanner));
}

bool
yt_projectile_victim_mines_overlay(struct yt_player *player,
    float *saved_mines)
{
	if (player == NULL || saved_mines == NULL)
		return false;
	*saved_mines = player->mines;
	player->mines = 0.0f;
	return yt_record_set_number(&player->record, YT_F129, 0.0f);
}

bool
yt_projectile_sector_mines_overlay(struct yt_sector *sector,
    float carried_mines)
{
	if (sector == NULL)
		return false;
	sector->mines = projectile_single_add(sector->mines, carried_mines);
	return yt_record_set_number(&sector->record, YT_F129, sector->mines);
}

uint32_t
yt_projectile_physical_record(float offset, float logical)
{
	return qb_brun_random_record_number(projectile_single_add(offset,
	    logical));
}

bool
yt_projectile_planet_ground_overlay(struct yt_planet *planet,
    float ground, float owner)
{
	if (planet == NULL)
		return false;
	planet->ground_forces = ground;
	planet->owner = owner;
	return yt_record_set_number(&planet->record, YT_F77, ground)
	    && yt_record_set_number(&planet->record, YT_F73, owner);
}

bool
yt_projectile_planet_productivity_overlay(struct yt_planet *planet,
    const float production[3], const float stock[3])
{
	size_t index;

	if (planet == NULL || production == NULL || stock == NULL)
		return false;
	for (index = 0U; index < 3U; ++index) {
		planet->production[index] = production[index];
		planet->stock[index] = stock[index];
		if (!yt_record_set_number(&planet->record, YT_F45 + index * 4U,
		    production[index])
		    || !yt_record_set_number(&planet->record,
		    YT_F57 + index * 4U, stock[index]))
			return false;
	}
	return true;
}

bool
yt_projectile_planet_destroy_overlay(struct yt_planet *planet)
{
	static const uint8_t link_zero[4] = {
		0x00, 0x00, 0x20, 0x00
	};

	if (planet == NULL)
		return false;
	planet->name_length = 0.0f;
	return yt_record_set_raw_number(&planet->record, YT_F85, link_zero);
}

bool
yt_projectile_sector_unlink_overlay(struct yt_sector *sector)
{
	static const uint8_t link_zero[4] = {
		0x00, 0x00, 0x20, 0x00
	};

	if (sector == NULL)
		return false;
	sector->planet = 0.0f;
	return yt_record_set_raw_number(&sector->record, YT_F93, link_zero);
}

bool
yt_projectile_planet_ground_damage(float ground, float owner,
    float *remaining, yt_projectile_damage_draw_fn draw, void *context,
    struct yt_projectile_ground_result *result, struct yt_error *error)
{
	size_t iterations = 0U;

	if (remaining == NULL || draw == NULL || result == NULL)
		return false;
	result->ground = ground;
	result->owner = owner;
	result->iterations = 0U;
	while (ground > 0.0f && *remaining > 0.0f) {
		float value;

		if (!draw(context, &value, error))
			return false;
		ground = projectile_single_sub(ground,
		    projectile_single_mul(value, 25.0f));
		*remaining = projectile_single_sub(*remaining, 1.0f);
		++iterations;
		result->ground = ground;
		result->iterations = iterations;
	}
	ground = floorf(ground);
	if (ground < 1.0f) {
		ground = 0.0f;
		owner = 0.0f;
	}
	result->ground = ground;
	result->owner = owner;
	result->iterations = iterations;
	return true;
}

bool
yt_projectile_planet_productivity_damage(float updater_ore,
    float production[3], float stock[3], float *remaining,
    yt_projectile_damage_draw_fn draw, void *context,
    struct yt_projectile_productivity_result *result,
    struct yt_error *error)
{
	float old_total;
	float new_total;
	size_t iterations = 0U;
	size_t index;

	if (production == NULL || stock == NULL || remaining == NULL
	    || draw == NULL || result == NULL)
		return false;
	old_total = projectile_single_add(projectile_single_add(production[0],
	    production[1]), production[2]);
	while ((updater_ore > 0.0f || production[1] > 0.0f
	    || production[2] > 0.0f) && *remaining > 0.0f) {
		for (index = 0U; index < 3U; ++index) {
			float value;

			if (!draw(context, &value, error))
				return false;
			production[index] = projectile_single_sub(
			    production[index], projectile_single_mul(value,
			    2000.0f));
		}
		*remaining = projectile_single_sub(*remaining, 1.0f);
		++iterations;
	}
	for (index = 0U; index < 3U; ++index) {
		float cap;

		if (production[index] < 0.0f)
			production[index] = 0.0f;
		cap = projectile_single_mul(production[index], 10.0f);
		if (stock[index] > cap)
			stock[index] = cap;
	}
	new_total = projectile_single_add(projectile_single_add(production[0],
	    production[1]), production[2]);
	result->old_total = old_total;
	result->new_total = new_total;
	result->iterations = iterations;
	return true;
}

bool
yt_projectile_planet_ground_row(float ground, uint8_t *row,
    size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "Ground forces reduced to";
	static const uint8_t suffix[] = "!";
	struct sector_row_builder builder = {row, capacity, 0U};

	if (length != NULL)
		*length = 0U;
	if (!sector_row_append(&builder, prefix, sizeof(prefix) - 1U)
	    || !sector_row_number(&builder, ground, false)
	    || !sector_row_append(&builder, suffix, sizeof(suffix) - 1U))
		return false;
	if (length != NULL)
		*length = builder.length;
	return true;
}

bool
yt_projectile_planet_productivity_row(float old_total, float new_total,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "Productivity reduced by";
	static const uint8_t middle[] = " units to";
	static const uint8_t suffix[] = " units!";
	struct sector_row_builder builder = {row, capacity, 0U};

	if (length != NULL)
		*length = 0U;
	if (!sector_row_append(&builder, prefix, sizeof(prefix) - 1U)
	    || !sector_row_number(&builder,
	    projectile_single_sub(old_total, new_total), false)
	    || !sector_row_append(&builder, middle, sizeof(middle) - 1U)
	    || !sector_row_number(&builder, new_total, false)
	    || !sector_row_append(&builder, suffix, sizeof(suffix) - 1U))
		return false;
	if (length != NULL)
		*length = builder.length;
	return true;
}


enum yt_projectile_post_impact_route
yt_projectile_post_impact_route(float remaining)
{
	return remaining > 0.0f ? YT_PROJECTILE_POST_IMPACT_NEXT_HOP
	    : YT_PROJECTILE_POST_IMPACT_FOOTER;
}

bool
yt_projectile_route_has_next(int16_t next_hop)
{
	return next_hop != 0;
}

bool
yt_projectile_route_avoid_enabled(bool plasma, int counterattack, int shooter)
{
	return !plasma && counterattack == 0 && shooter != -1;
}

bool
yt_projectile_route_failure_row(bool caller_suffix, uint8_t *row,
    size_t capacity, size_t *length)
{
	static const uint8_t helper[] =
	    "*** You can't get there without going someplace you dont want to!";
	static const uint8_t suffix[] = "Missles self destructed!";
	const uint8_t *source = caller_suffix ? suffix : helper;
	size_t source_length = caller_suffix
	    ? sizeof(suffix) - 1U : sizeof(helper) - 1U;

	if (length == NULL)
		return false;
	*length = 0U;
	if (capacity < source_length || row == NULL)
		return false;
	memcpy(row, source, source_length);
	*length = source_length;
	return true;
}

bool
yt_projectile_footer_row(uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t footer[] = "*** End of Report ***";

	if (length == NULL)
		return false;
	*length = 0U;
	if (capacity < sizeof(footer) - 1U || row == NULL)
		return false;
	memcpy(row, footer, sizeof(footer) - 1U);
	*length = sizeof(footer) - 1U;
	return true;
}

bool
yt_projectile_player_damage(struct yt_player *target, float *remaining,
    yt_projectile_damage_draw_fn draw, void *context,
    struct yt_projectile_damage_result *result, struct yt_error *error)
{
	double original_fighters;
	double fighter_damage = 0.0;
	float original_shields;
	float shield_damage = 0.0f;
	float saved_missiles;
	uint8_t scanner_raw[4];
	float counter = 1.0f;
	bool scanner_disabled = false;
	size_t iterations = 0U;

	if (target == NULL || remaining == NULL || draw == NULL
	    || result == NULL)
		return false;
	original_fighters = (double)target->fighters;
	original_shields = target->shields;
	saved_missiles = *remaining;
	memcpy(scanner_raw, target->record.bytes + YT_F113,
	    sizeof(scanner_raw));
	while (yt_projectile_damage_iteration(counter, saved_missiles)) {
		bool overflow;
		float value;
		float scanner_product;
		int32_t scanner;

		++iterations;
		*remaining = projectile_single_sub(*remaining, 1.0f);
		if (!draw(context, &value, error))
			return false;
		scanner_product = projectile_single_mul(value, *remaining);
		scanner = qb_cint_mbf32(scanner_raw, 0U, &overflow);
		if (overflow) {
			if (error != NULL) {
				error->status = YT_RANGE;
				(void)snprintf(error->operation,
				    sizeof(error->operation), "%s",
				    "cruise missile scanner CINT");
			}
			return false;
		}
		if (scanner_product > 100.0f && scanner != 0) {
			target->danger_scanner = 0.0f;
			memset(scanner_raw, 0, sizeof(scanner_raw));
			scanner_disabled = true;
		}
		if (!draw(context, &value, error))
			return false;
		fighter_damage = floor((double)projectile_single_mul(value,
		    4001.0f) + fighter_damage);
		if (!draw(context, &value, error))
			return false;
		/* The SINGLE draw is promoted for the DOUBLE fighter operand. */
		if ((double)value * original_fighters < fighter_damage) {
			if (!draw(context, &value, error))
				return false;
			shield_damage = projectile_single_add(shield_damage,
			    floorf(projectile_single_mul(value, 1001.0f)));
		}
		if (fighter_damage >= original_fighters
		    && shield_damage >= original_shields)
			break;
		counter = projectile_single_add(counter, 1.0f);
	}
	if (fighter_damage > original_fighters)
		fighter_damage = original_fighters;
	if (shield_damage > original_shields)
		shield_damage = original_shields;
	target->fighters = (float)(original_fighters - fighter_damage);
	target->shields = projectile_single_sub(original_shields,
	    shield_damage);
	result->fighters = fighter_damage;
	result->shields = shield_damage;
	result->scanner_disabled = scanner_disabled;
	result->iterations = iterations;
	return true;
}

bool
yt_projectile_attack_first_rows(bool plasma,
    const uint8_t *attacker, size_t attacker_length,
    const uint8_t *victim, size_t victim_length, float sector,
    uint8_t *news, size_t news_capacity, size_t *news_length,
    uint8_t *direct, size_t direct_capacity, size_t *direct_length)
{
	static const uint8_t missile_news[] = "'s missiles attacked ";
	static const uint8_t missile_direct[] = "The missiles attacked ";
	static const uint8_t plasma_news[] = "'s plasma bolts hit ";
	static const uint8_t plasma_direct[] = "The plasma bolts hit ";
	static const uint8_t middle[] = " in";
	static const uint8_t suffix[] = " reducing";
	const uint8_t *news_infix = plasma ? plasma_news : missile_news;
	size_t news_infix_length = plasma
	    ? sizeof(plasma_news) - 1U : sizeof(missile_news) - 1U;
	const uint8_t *direct_prefix = plasma ? plasma_direct : missile_direct;
	size_t direct_prefix_length = plasma
	    ? sizeof(plasma_direct) - 1U : sizeof(missile_direct) - 1U;
	char sector_text[64];
	int sector_length;
	size_t news_needed;
	size_t direct_needed;
	size_t position;

	if (news_length == NULL || direct_length == NULL
	    || (attacker == NULL && attacker_length != 0U)
	    || (victim == NULL && victim_length != 0U))
		return false;
	*news_length = 0U;
	*direct_length = 0U;
	sector_length = qb_str_single(sector_text, sizeof(sector_text), sector);
	if (sector_length < 0)
		return false;
	news_needed = attacker_length + news_infix_length + victim_length
	    + sizeof(middle) - 1U + (size_t)sector_length
	    + sizeof(suffix) - 1U;
	direct_needed = direct_prefix_length + victim_length
	    + sizeof(middle) - 1U + (size_t)sector_length
	    + sizeof(suffix) - 1U;
	if (news_needed > news_capacity || direct_needed > direct_capacity
	    || (news_needed != 0U && news == NULL)
	    || (direct_needed != 0U && direct == NULL))
		return false;
	position = 0U;
	if (attacker_length != 0U)
		memcpy(news + position, attacker, attacker_length);
	position += attacker_length;
	memcpy(news + position, news_infix, news_infix_length);
	position += news_infix_length;
	if (victim_length != 0U)
		memcpy(news + position, victim, victim_length);
	position += victim_length;
	memcpy(news + position, middle, sizeof(middle) - 1U);
	position += sizeof(middle) - 1U;
	memcpy(news + position, sector_text, (size_t)sector_length);
	position += (size_t)sector_length;
	memcpy(news + position, suffix, sizeof(suffix) - 1U);
	position += sizeof(suffix) - 1U;
	*news_length = position;
	position = 0U;
	memcpy(direct + position, direct_prefix, direct_prefix_length);
	position += direct_prefix_length;
	if (victim_length != 0U)
		memcpy(direct + position, victim, victim_length);
	position += victim_length;
	memcpy(direct + position, middle, sizeof(middle) - 1U);
	position += sizeof(middle) - 1U;
	memcpy(direct + position, sector_text, (size_t)sector_length);
	position += (size_t)sector_length;
	memcpy(direct + position, suffix, sizeof(suffix) - 1U);
	position += sizeof(suffix) - 1U;
	*direct_length = position;
	return true;
}

bool
yt_projectile_destroyed_rows(const uint8_t *victim,
    size_t victim_length, uint8_t *destroyed, size_t destroyed_capacity,
    size_t *destroyed_length, uint8_t *warning, size_t warning_capacity,
    size_t *warning_length)
{
	static const uint8_t destroyed_suffix[] = " was destroyed!";
	static const uint8_t warning_prefix[] = "*** WARNING, ";
	static const uint8_t warning_suffix[] = " had sector mines!";
	size_t destroyed_needed;
	size_t warning_needed;

	if (destroyed_length == NULL || warning_length == NULL
	    || (victim == NULL && victim_length != 0U))
		return false;
	*destroyed_length = 0U;
	*warning_length = 0U;
	destroyed_needed = victim_length + sizeof(destroyed_suffix) - 1U;
	warning_needed = sizeof(warning_prefix) - 1U + victim_length
	    + sizeof(warning_suffix) - 1U;
	if (destroyed_needed > destroyed_capacity
	    || warning_needed > warning_capacity
	    || (destroyed_needed != 0U && destroyed == NULL)
	    || (warning_needed != 0U && warning == NULL))
		return false;
	if (victim_length != 0U)
		memcpy(destroyed, victim, victim_length);
	memcpy(destroyed + victim_length, destroyed_suffix,
	    sizeof(destroyed_suffix) - 1U);
	memcpy(warning, warning_prefix, sizeof(warning_prefix) - 1U);
	if (victim_length != 0U)
		memcpy(warning + sizeof(warning_prefix) - 1U,
		    victim, victim_length);
	memcpy(warning + sizeof(warning_prefix) - 1U + victim_length,
	    warning_suffix, sizeof(warning_suffix) - 1U);
	*destroyed_length = destroyed_needed;
	*warning_length = warning_needed;
	return true;
}

bool
yt_projectile_friendly_planet_row(const uint8_t *planet,
    size_t planet_length, uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "NOT attacking friendly planet \"";
	static const uint8_t suffix[] = "\"!";
	size_t needed = sizeof(prefix) - 1U + planet_length
	    + sizeof(suffix) - 1U;

	if (length == NULL || (planet == NULL && planet_length != 0U))
		return false;
	*length = 0U;
	if (needed > capacity || (needed != 0U && row == NULL))
		return false;
	memcpy(row, prefix, sizeof(prefix) - 1U);
	if (planet_length != 0U)
		memcpy(row + sizeof(prefix) - 1U, planet, planet_length);
	memcpy(row + sizeof(prefix) - 1U + planet_length,
	    suffix, sizeof(suffix) - 1U);
	*length = needed;
	return true;
}

bool
yt_projectile_planet_attack_rows(bool plasma,
    const uint8_t *attacker, size_t attacker_length,
    const uint8_t *planet, size_t planet_length, float sector,
    uint8_t *direct, size_t direct_capacity, size_t *direct_length,
    uint8_t *news, size_t news_capacity, size_t *news_length)
{
	static const uint8_t missile_direct[] = "The Missiles attacked planet ";
	static const uint8_t missile_news[] = "'s Missiles attacked planet ";
	static const uint8_t plasma_direct[] = "The plasma bolts hit planet ";
	static const uint8_t plasma_news[] = "'s plasma bolts hit planet ";
	static const uint8_t sector_prefix[] = " in sector";
	const uint8_t *direct_prefix = plasma ? plasma_direct : missile_direct;
	size_t direct_prefix_length = plasma
	    ? sizeof(plasma_direct) - 1U : sizeof(missile_direct) - 1U;
	const uint8_t *news_infix = plasma ? plasma_news : missile_news;
	size_t news_infix_length = plasma
	    ? sizeof(plasma_news) - 1U : sizeof(missile_news) - 1U;
	char sector_text[64];
	int sector_length;
	size_t direct_needed;
	size_t news_needed;
	size_t position;

	if (direct_length == NULL || news_length == NULL
	    || (attacker == NULL && attacker_length != 0U)
	    || (planet == NULL && planet_length != 0U))
		return false;
	*direct_length = 0U;
	*news_length = 0U;
	sector_length = qb_str_single(sector_text, sizeof(sector_text), sector);
	if (sector_length < 0)
		return false;
	direct_needed = direct_prefix_length + planet_length
	    + sizeof(sector_prefix) - 1U + (size_t)sector_length + 1U;
	news_needed = attacker_length + news_infix_length + planet_length
	    + sizeof(sector_prefix) - 1U + (size_t)sector_length + 1U;
	if (direct_needed > direct_capacity || news_needed > news_capacity
	    || (direct_needed != 0U && direct == NULL)
	    || (news_needed != 0U && news == NULL))
		return false;
	position = 0U;
	memcpy(direct + position, direct_prefix, direct_prefix_length);
	position += direct_prefix_length;
	if (planet_length != 0U)
		memcpy(direct + position, planet, planet_length);
	position += planet_length;
	memcpy(direct + position, sector_prefix, sizeof(sector_prefix) - 1U);
	position += sizeof(sector_prefix) - 1U;
	memcpy(direct + position, sector_text, (size_t)sector_length);
	position += (size_t)sector_length;
	direct[position++] = '!';
	*direct_length = position;
	position = 0U;
	if (attacker_length != 0U)
		memcpy(news + position, attacker, attacker_length);
	position += attacker_length;
	memcpy(news + position, news_infix, news_infix_length);
	position += news_infix_length;
	if (planet_length != 0U)
		memcpy(news + position, planet, planet_length);
	position += planet_length;
	memcpy(news + position, sector_prefix, sizeof(sector_prefix) - 1U);
	position += sizeof(sector_prefix) - 1U;
	memcpy(news + position, sector_text, (size_t)sector_length);
	position += (size_t)sector_length;
	news[position++] = '!';
	*news_length = position;
	return true;
}

bool
yt_xannor_victory_winner(const uint8_t *player, size_t player_length,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "Congratulations go to ";
	static const uint8_t suffix[] = " who defeated the Xannor HQ!!!";
	size_t needed = sizeof(prefix) - 1U + player_length
	    + sizeof(suffix) - 1U;

	if (length == NULL || (player == NULL && player_length != 0U))
		return false;
	*length = 0U;
	if (needed > capacity || (needed != 0U && row == NULL))
		return false;
	memcpy(row, prefix, sizeof(prefix) - 1U);
	if (player_length != 0U)
		memcpy(row + sizeof(prefix) - 1U, player, player_length);
	memcpy(row + sizeof(prefix) - 1U + player_length,
	    suffix, sizeof(suffix) - 1U);
	*length = needed;
	return true;
}

bool
yt_fixed_text_contains(const uint8_t field[YT_TEXT_FIELD_SIZE],
    const uint8_t *needle, size_t needle_length)
{
	size_t offset;

	if (field == NULL || (needle == NULL && needle_length != 0U))
		return false;
	if (needle_length == 0U)
		return true;
	if (needle_length > YT_TEXT_FIELD_SIZE)
		return false;
	for (offset = 0; offset + needle_length <= YT_TEXT_FIELD_SIZE;
	    ++offset) {
		if (memcmp(field + offset, needle, needle_length) == 0)
			return true;
	}
	return false;
}

bool
yt_radio_player_prompt(const struct yt_player *player, uint8_t *prompt,
    size_t capacity, size_t *length, struct yt_error *error)
{
	static const uint8_t suffix[] = " [Y]? ";
	size_t stored;

	if (length != NULL)
		*length = 0;
	if (player == NULL || prompt == NULL || length == NULL)
		return false;
	if (!yt_player_stored_name(player, prompt, &stored, error))
		return false;
	if (stored + sizeof(suffix) - 1U > capacity) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation,
			    sizeof(error->operation), "%s",
			    "radio player prompt capacity");
		}
		return false;
	}
	memcpy(prompt + stored, suffix, sizeof(suffix) - 1U);
	*length = stored + sizeof(suffix) - 1U;
	return true;
}

bool
yt_radio_tuning_row(const struct yt_player *player, uint8_t *row,
    size_t capacity, size_t *length, struct yt_error *error)
{
	static const uint8_t prefix[] = "Tuning in to ";
	static const uint8_t suffix[] = "'s frequency.";
	uint8_t stored[YT_TEXT_FIELD_SIZE];
	size_t stored_length;
	size_t needed;

	if (length != NULL)
		*length = 0U;
	if (player == NULL || row == NULL || length == NULL)
		return false;
	if (!yt_player_stored_name(player, stored, &stored_length, error))
		return false;
	needed = sizeof(prefix) - 1U + stored_length + sizeof(suffix) - 1U;
	if (needed > capacity) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation,
			    sizeof(error->operation), "%s",
			    "radio tuning row capacity");
		}
		return false;
	}
	memcpy(row, prefix, sizeof(prefix) - 1U);
	if (stored_length != 0U)
		memcpy(row + sizeof(prefix) - 1U, stored, stored_length);
	memcpy(row + sizeof(prefix) - 1U + stored_length,
	    suffix, sizeof(suffix) - 1U);
	*length = needed;
	return true;
}

bool
yt_direct_attack_radio_text(const uint8_t *name, size_t name_length,
    double defender_loss, uint8_t *text, size_t capacity, size_t *length)
{
	static const uint8_t middle[] = " destroyed";
	static const uint8_t suffix[] = " of your fighters!";
	uint8_t raw_double[8];
	char aliased[64];
	int aliased_length;
	size_t total;

	if (length != NULL)
		*length = 0;
	if ((name == NULL && name_length != 0U) || text == NULL
	    || length == NULL)
		return false;
	if (qb_mbf64_encode(defender_loss, raw_double) != QB_MBF_OK)
		return false;
	aliased_length = qb_str_mbf32(aliased, sizeof(aliased), raw_double);
	if (aliased_length < 0)
		return false;
	total = name_length + sizeof(middle) - 1U + (size_t)aliased_length
	    + sizeof(suffix) - 1U;
	if (total > capacity)
		return false;
	if (name_length != 0U)
		memcpy(text, name, name_length);
	memcpy(text + name_length, middle, sizeof(middle) - 1U);
	memcpy(text + name_length + sizeof(middle) - 1U, aliased,
	    (size_t)aliased_length);
	memcpy(text + name_length + sizeof(middle) - 1U
	    + (size_t)aliased_length, suffix, sizeof(suffix) - 1U);
	*length = total;
	return true;
}

static bool
direct_attack_append(uint8_t *output, size_t capacity, size_t *position,
    const void *data, size_t length)
{
	if (output == NULL || position == NULL || (data == NULL && length != 0U)
	    || *position > capacity || length > capacity - *position)
		return false;
	if (length != 0U)
		memcpy(output + *position, data, length);
	*position += length;
	return true;
}

bool
yt_direct_attack_team_row(const uint8_t *name, size_t name_length,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "NOT attacking team member ";
	static const uint8_t suffix[] = "!";
	size_t position = 0U;

	if (length == NULL || (name == NULL && name_length != 0U))
		return false;
	*length = 0U;
	if (!direct_attack_append(row, capacity, &position, prefix,
	    sizeof(prefix) - 1U)
	    || !direct_attack_append(row, capacity, &position, name,
	    name_length)
	    || !direct_attack_append(row, capacity, &position, suffix,
	    sizeof(suffix) - 1U))
		return false;
	*length = position;
	return true;
}

bool
yt_direct_attack_candidate_prompt(const uint8_t *name,
    size_t name_length, uint8_t *prompt, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "Attack ";
	static const uint8_t suffix[] = " (Y/N)[Y]? ";
	size_t position = 0U;

	if (length == NULL || (name == NULL && name_length != 0U))
		return false;
	*length = 0U;
	if (!direct_attack_append(prompt, capacity, &position, prefix,
	    sizeof(prefix) - 1U)
	    || !direct_attack_append(prompt, capacity, &position, name,
	    name_length)
	    || !direct_attack_append(prompt, capacity, &position, suffix,
	    sizeof(suffix) - 1U))
		return false;
	*length = position;
	return true;
}

bool
yt_direct_attack_commitment_prompt(double fighters, uint8_t *prompt,
    size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "You have";
	static const uint8_t suffix[] = ". Use how many fighters? [0] ";
	char number[64];
	int number_length;
	size_t position = 0U;

	if (length == NULL)
		return false;
	*length = 0U;
	number_length = qb_str_double(number, sizeof(number), fighters);
	if (number_length < 0
	    || !direct_attack_append(prompt, capacity, &position, prefix,
	    sizeof(prefix) - 1U)
	    || !direct_attack_append(prompt, capacity, &position, number,
	    (size_t)number_length)
	    || !direct_attack_append(prompt, capacity, &position, suffix,
	    sizeof(suffix) - 1U))
		return false;
	*length = position;
	return true;
}

bool
yt_direct_attack_too_many_row(double fighters, uint8_t *row,
    size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "You only have";
	static const uint8_t suffix[] = "!";
	char number[64];
	int number_length;
	size_t position = 0U;

	if (length == NULL)
		return false;
	*length = 0U;
	number_length = qb_str_double(number, sizeof(number), fighters);
	if (number_length < 0
	    || !direct_attack_append(row, capacity, &position, prefix,
	    sizeof(prefix) - 1U)
	    || !direct_attack_append(row, capacity, &position, number,
	    (size_t)number_length)
	    || !direct_attack_append(row, capacity, &position, suffix,
	    sizeof(suffix) - 1U))
		return false;
	*length = position;
	return true;
}

bool
yt_direct_attack_result_rows(double attacker_loss,
    double cached_reserve, double defender_loss, double defenders,
    uint8_t *attacker_row, size_t attacker_capacity,
    size_t *attacker_length, uint8_t *defender_row,
    size_t defender_capacity, size_t *defender_length)
{
	static const uint8_t attacker_prefix[] = "You lost";
	static const uint8_t attacker_middle[] = " fighter(s),";
	static const uint8_t remain[] = " remain.";
	static const uint8_t defender_prefix[] = "You destroyed";
	static const uint8_t defender_middle[] = " enemy fighters,";
	char attacker_loss_text[64];
	char reserve_text[64];
	char defender_loss_text[64];
	char defenders_text[64];
	int attacker_loss_length;
	int reserve_length;
	int defender_loss_length;
	int defenders_length;
	size_t attacker_position = 0U;
	size_t defender_position = 0U;

	if (attacker_length == NULL || defender_length == NULL)
		return false;
	*attacker_length = 0U;
	*defender_length = 0U;
	attacker_loss_length = qb_str_double(attacker_loss_text,
	    sizeof(attacker_loss_text), attacker_loss);
	reserve_length = qb_str_double(reserve_text, sizeof(reserve_text),
	    cached_reserve);
	defender_loss_length = qb_str_double(defender_loss_text,
	    sizeof(defender_loss_text), defender_loss);
	defenders_length = qb_str_double(defenders_text,
	    sizeof(defenders_text), defenders);
	if (attacker_loss_length < 0 || reserve_length < 0
	    || defender_loss_length < 0 || defenders_length < 0
	    || !direct_attack_append(attacker_row, attacker_capacity,
	    &attacker_position, attacker_prefix, sizeof(attacker_prefix) - 1U)
	    || !direct_attack_append(attacker_row, attacker_capacity,
	    &attacker_position, attacker_loss_text,
	    (size_t)attacker_loss_length)
	    || !direct_attack_append(attacker_row, attacker_capacity,
	    &attacker_position, attacker_middle, sizeof(attacker_middle) - 1U)
	    || !direct_attack_append(attacker_row, attacker_capacity,
	    &attacker_position, reserve_text, (size_t)reserve_length)
	    || !direct_attack_append(attacker_row, attacker_capacity,
	    &attacker_position, remain, sizeof(remain) - 1U)
	    || !direct_attack_append(defender_row, defender_capacity,
	    &defender_position, defender_prefix, sizeof(defender_prefix) - 1U)
	    || !direct_attack_append(defender_row, defender_capacity,
	    &defender_position, defender_loss_text,
	    (size_t)defender_loss_length)
	    || !direct_attack_append(defender_row, defender_capacity,
	    &defender_position, defender_middle, sizeof(defender_middle) - 1U)
	    || !direct_attack_append(defender_row, defender_capacity,
	    &defender_position, defenders_text, (size_t)defenders_length)
	    || !direct_attack_append(defender_row, defender_capacity,
	    &defender_position, remain, sizeof(remain) - 1U))
		return false;
	*attacker_length = attacker_position;
	*defender_length = defender_position;
	return true;
}

static float
direct_attack_single_add(float left, float right)
{
	volatile float result = left + right;

	return result;
}

static double
direct_attack_double_add(double left, double right)
{
	volatile double result = left + right;

	return result;
}

static double
direct_attack_double_sub(double left, double right)
{
	volatile double result = left - right;

	return result;
}

bool
yt_hostile_attack_persistence_run(
    struct yt_hostile_attack_persistence_state *state,
    const struct yt_hostile_attack_persistence_ops *ops, void *context,
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
		    || !direct_attack_append(news, sizeof(news), &position,
		    state->cached_player_name,
		    state->cached_player_name_length)
		    || !direct_attack_append(news, sizeof(news), &position,
		    destroyed, sizeof(destroyed) - 1U)
		    || !direct_attack_append(news, sizeof(news), &position,
		    (const uint8_t *)loss_number, (size_t)loss_length)
		    || !direct_attack_append(news, sizeof(news), &position,
		    belonging, sizeof(belonging) - 1U)
		    || !direct_attack_append(news, sizeof(news), &position,
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
yt_hostile_attack_tail_run(struct yt_hostile_attack_tail_state *state,
    const struct yt_hostile_attack_tail_ops *ops, void *context,
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
			state->current.turns = direct_attack_single_add(
			    state->current.turns, state->bonus);
			(void)yt_record_set_number(&state->current.record, YT_F49,
			    state->current.turns);
			if (!ops->write_player(context, state->current_player_record,
			    &state->current, error))
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

bool
yt_hostile_attack_combat_run(struct yt_hostile_attack_combat_state *state,
    const struct yt_hostile_attack_combat_ops *ops, void *context,
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
	state->old_owner = qb_mbf32_decode(
	    &state->opened_sector.record.bytes[YT_F85]);
	if (!ops->read_player(context, state->current_player_record,
	    &state->current, error))
		return false;
	state->old_ship = (double)state->current.fighters;
	if (!ops->sound(context, 2.0f, error))
		return false;
	do {
		double remaining_attacker = direct_attack_double_sub(
		    state->commitment, state->attacker_loss);
		double remaining_defender = direct_attack_double_sub(
		    state->old_count, state->defender_loss);
		volatile double ratio;

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
		ratio = remaining_attacker / remaining_defender;
		if (!state->surrender_checked && state->allow_surrender
		    && ratio > 10.0) {
			state->surrender = (struct yt_hostile_surrender_state){
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
			state->surrender_checked = state->surrender.checked;
			state->surrendered = state->surrender.accepted;
			ops->cache_player(context, &state->current);
			if (!child_result)
				return false;
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
			state->attacker_loss = direct_attack_double_add(
			    state->attacker_loss, (double)state->quantum);
		} else {
			state->defender_loss = direct_attack_double_add(
			    state->defender_loss, (double)state->quantum);
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
		state->ship_fighters = direct_attack_double_sub(state->old_ship,
		    state->attacker_loss);
		state->deployed_remaining = direct_attack_double_sub(
		    state->old_count, state->defender_loss);
		state->current.fighters = (float)state->ship_fighters;
		ops->store_ship(context, state->ship_fighters);
		ops->cache_player(context, &state->current);
	}
	attacker_length = qb_str_double(attacker_number,
	    sizeof(attacker_number), state->attacker_loss);
	defender_length = qb_str_double(defender_number,
	    sizeof(defender_number), state->defender_loss);
	if (attacker_length < 0 || defender_length < 0
	    || !direct_attack_append(lost_row, sizeof(lost_row), &lost_length,
	    lost_prefix, sizeof(lost_prefix) - 1U)
	    || !direct_attack_append(lost_row, sizeof(lost_row), &lost_length,
	    attacker_number, (size_t)attacker_length)
	    || !direct_attack_append(lost_row, sizeof(lost_row), &lost_length,
	    lost_suffix, sizeof(lost_suffix) - 1U)
	    || !direct_attack_append(destroyed_row, sizeof(destroyed_row),
	    &destroyed_length, destroyed_prefix,
	    sizeof(destroyed_prefix) - 1U)
	    || !direct_attack_append(destroyed_row, sizeof(destroyed_row),
	    &destroyed_length, defender_number, (size_t)defender_length)
	    || !direct_attack_append(destroyed_row, sizeof(destroyed_row),
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
	    (struct yt_hostile_attack_persistence_state){
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
	state->tail = (struct yt_hostile_attack_tail_state){
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
yt_hostile_bribe_accept_run(struct yt_hostile_bribe_accept_state *state,
    const struct yt_hostile_bribe_accept_ops *ops, void *context,
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
	fighters = direct_attack_double_add((double)state->current.fighters,
	    (double)state->cached_defenders);
	credits = direct_attack_double_sub((double)state->current.credits,
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
hostile_bribe_name_row(const uint8_t *prefix, size_t prefix_length,
    const uint8_t *name, size_t name_length, const uint8_t *suffix,
    size_t suffix_length, uint8_t *row, size_t capacity, size_t *length)
{
	size_t used = 0U;

	if (length == NULL)
		return false;
	*length = 0U;
	if (!direct_attack_append(row, capacity, &used, prefix, prefix_length)
	    || !direct_attack_append(row, capacity, &used, name, name_length)
	    || !direct_attack_append(row, capacity, &used, suffix,
	    suffix_length))
		return false;
	*length = used;
	return true;
}

static bool
hostile_bribe_force_run(struct yt_hostile_bribe_state *state,
    const struct yt_hostile_bribe_ops *ops, void *context,
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
yt_hostile_bribe_run(struct yt_hostile_bribe_state *state,
    const struct yt_hostile_bribe_ops *ops, void *context,
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
		if (!hostile_bribe_name_row(ordinary_prefix,
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
		return hostile_bribe_force_run(state, ops, context, false, error);
	}

	if (state->planet_link != 0.0f) {
		state->branch = YT_HOSTILE_BRIBE_PLANET_REFUSAL;
		if (!hostile_bribe_name_row(planet_prefix,
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
		if (!hostile_bribe_name_row(life_prefix,
		    sizeof(life_prefix) - 1U, state->real_first_name,
		    state->real_first_name_length, bang, sizeof(bang) - 1U,
		    row, sizeof(row), &row_length)
		    || !ops->present(context, row, row_length,
		    YT_HOSTILE_BRIBE_LIFE_DEMAND_ROW, error))
			return false;
		return hostile_bribe_force_run(state, ops, context, true, error);
	}

	if (!hostile_bribe_name_row(introduction_prefix,
	    sizeof(introduction_prefix) - 1U, state->real_first_name,
	    state->real_first_name_length, bang, sizeof(bang) - 1U,
	    row, sizeof(row), &row_length)
	    || !ops->present(context, row, row_length,
	    YT_HOSTILE_BRIBE_INTRODUCTION_ROW, error))
		return false;
	credits_length = qb_str_double(credits, sizeof(credits), state->credits);
	if (credits_length < 0
	    || !direct_attack_append(prompt, sizeof(prompt), &prompt_length,
	    prompt_prefix, sizeof(prompt_prefix) - 1U)
	    || !direct_attack_append(prompt, sizeof(prompt), &prompt_length,
	    credits, (size_t)credits_length)
	    || !direct_attack_append(prompt, sizeof(prompt), &prompt_length,
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
		state->accepted = (struct yt_hostile_bribe_accept_state){
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
	if (!hostile_bribe_name_row(rejected_prefix,
	    sizeof(rejected_prefix) - 1U, state->real_first_name,
	    state->real_first_name_length, rejected_suffix,
	    sizeof(rejected_suffix) - 1U, row, sizeof(row), &row_length)
	    || !ops->present(context, row, row_length,
	    YT_HOSTILE_BRIBE_REJECTED_ROW, error))
		return false;
	return hostile_bribe_force_run(state, ops, context, true, error);
}

void
yt_direct_attack_fighter_overlay(struct yt_player *player, float fighters)
{
	if (player == NULL)
		return;
	player->fighters = fighters;
	(void)yt_record_set_number(&player->record, YT_F61, fighters);
}

void
yt_direct_attack_shield_overlay(struct yt_player *player, float shields)
{
	if (player == NULL)
		return;
	player->shields = shields;
	(void)yt_record_set_number(&player->record, YT_F53, shields);
}

void
yt_deployed_attack_player_overlay(struct yt_player *player,
    float shields, float fighters)
{
	if (player == NULL)
		return;
	player->shields = shields;
	player->fighters = fighters;
	(void)yt_record_set_number(&player->record, YT_F53, shields);
	(void)yt_record_set_number(&player->record, YT_F61, fighters);
}

void
yt_deployed_attack_sector_overlay(struct yt_sector *sector, float fighters)
{
	if (sector == NULL)
		return;
	sector->fighters = fighters;
	(void)yt_record_set_number(&sector->record, YT_F81, fighters);
	if (fighters < 1.0f) {
		sector->fighter_owner = 0.0f;
		(void)yt_record_set_number(&sector->record, YT_F85, 0.0f);
	}
}

void
yt_death_player_overlay(struct yt_player *player, float killer)
{
	static const uint8_t dirty_zero[4] = {0x00, 0x00, 0x7a, 0x00};

	if (player == NULL)
		return;
	player->killed_by = killer;
	player->sector = 0.0f;
	player->ports_owned = 0.0f;
	(void)yt_record_set_number(&player->record, YT_F45, killer);
	(void)yt_record_set_raw_number(&player->record, YT_F57, dirty_zero);
	(void)yt_record_set_raw_number(&player->record, YT_F117, dirty_zero);
}

bool
yt_death_sector_overlay(struct yt_sector *sector, float victim)
{
	if (sector == NULL || sector->fighter_owner != victim)
		return false;
	sector->fighter_owner = -2.0f;
	(void)yt_record_set_number(&sector->record, YT_F85, -2.0f);
	return true;
}

enum yt_death_port_route
yt_death_port_overlay(struct yt_port *port, float victim, float killer,
    float last_player)
{
	bool valid;

	if (port == NULL || port->owner != victim)
		return YT_DEATH_PORT_UNMATCHED;
	valid = (killer != victim) & (killer > 1.0f)
	    & (killer <= last_player);
	if (valid) {
		port->owner = killer;
		port->last_minute = killer;
		(void)yt_record_set_number(&port->record, YT_F97, killer);
		(void)yt_record_set_number(&port->record, YT_F101, killer);
		return YT_DEATH_PORT_TRANSFERRED;
	}
	port->owner = 0.0f;
	port->treasury = 0.0f;
	(void)yt_record_set_number(&port->record, YT_F97, 0.0f);
	(void)yt_record_set_number(&port->record, YT_F89, 0.0f);
	return YT_DEATH_PORT_CLEARED;
}

void
yt_death_killer_credit_overlay(struct yt_player *player, float ports)
{
	volatile float updated;

	if (player == NULL)
		return;
	updated = player->ports_owned + ports;
	player->ports_owned = updated;
	(void)yt_record_set_number(&player->record, YT_F117, updated);
}

bool
yt_death_title_row(const uint8_t *victim, size_t victim_length,
    float ports, uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "The titles to";
	static const uint8_t middle[] = " ports of ";
	static const uint8_t suffix[] = "'s are now yours!";
	char number[64];
	int number_length;
	size_t position = 0U;

	if (length == NULL)
		return false;
	*length = 0U;
	if (row == NULL || (victim == NULL && victim_length != 0U))
		return false;
	number_length = qb_str_single(number, sizeof(number), ports);
	if (number_length < 0
	    || !direct_attack_append(row, capacity, &position, prefix,
	    sizeof(prefix) - 1U)
	    || !direct_attack_append(row, capacity, &position,
	    (const uint8_t *)number, (size_t)number_length)
	    || !direct_attack_append(row, capacity, &position, middle,
	    sizeof(middle) - 1U)
	    || !direct_attack_append(row, capacity, &position, victim,
	    victim_length)
	    || !direct_attack_append(row, capacity, &position, suffix,
	    sizeof(suffix) - 1U))
		return false;
	*length = position;
	return true;
}

bool
yt_death_kill_news_row(const uint8_t *killer, size_t killer_length,
    const uint8_t *victim, size_t victim_length, bool self,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "  -  ";
	static const uint8_t self_suffix[] = " was killed!";
	static const uint8_t other_infix[] = " killed ";
	size_t position = 0U;

	if (length == NULL)
		return false;
	*length = 0U;
	if (row == NULL || (killer == NULL && killer_length != 0U)
	    || (victim == NULL && victim_length != 0U))
		return false;
	if (!direct_attack_append(row, capacity, &position, prefix,
	    sizeof(prefix) - 1U)
	    || !direct_attack_append(row, capacity, &position, killer,
	    killer_length)
	    || !direct_attack_append(row, capacity, &position,
	    self ? self_suffix : other_infix,
	    self ? sizeof(self_suffix) - 1U : sizeof(other_infix) - 1U)
	    || (!self && !direct_attack_append(row, capacity, &position,
	    victim, victim_length)))
		return false;
	*length = position;
	return true;
}

bool
yt_death_port_news_row(const uint8_t *victim, size_t victim_length,
    float ports, uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "  -  Took";
	static const uint8_t middle[] = " ports from ";
	char number[64];
	int number_length;
	size_t position = 0U;

	if (length == NULL)
		return false;
	*length = 0U;
	if (row == NULL || (victim == NULL && victim_length != 0U))
		return false;
	number_length = qb_str_single(number, sizeof(number), ports);
	if (number_length < 0
	    || !direct_attack_append(row, capacity, &position, prefix,
	    sizeof(prefix) - 1U)
	    || !direct_attack_append(row, capacity, &position,
	    (const uint8_t *)number, (size_t)number_length)
	    || !direct_attack_append(row, capacity, &position, middle,
	    sizeof(middle) - 1U)
	    || !direct_attack_append(row, capacity, &position, victim,
	    victim_length))
		return false;
	*length = position;
	return true;
}

void
yt_bribe_sector_overlay(struct yt_sector *sector)
{
	if (sector == NULL)
		return;
	sector->fighter_owner = 0.0f;
	(void)yt_record_set_number(&sector->record, YT_F85, 0.0f);
	sector->fighters = 0.0f;
	(void)yt_record_set_number(&sector->record, YT_F81, 0.0f);
}

void
yt_bribe_player_overlay(struct yt_player *player, float fighters,
    float credits)
{
	if (player == NULL)
		return;
	player->fighters = fighters;
	player->credits = credits;
	(void)yt_record_set_number(&player->record, YT_F61, fighters);
	(void)yt_record_set_number(&player->record, YT_F81, credits);
}

bool
yt_player_killer_row(const struct yt_player *player, uint8_t *row,
    size_t capacity, size_t *length, bool *emit, struct yt_error *error)
{
	static const uint8_t suffix[] = " destroyed your ship!";
	size_t prefix;

	if (length != NULL)
		*length = 0;
	if (emit != NULL)
		*emit = false;
	if (player == NULL || row == NULL || length == NULL || emit == NULL)
		return false;
	if (player->name_length == 0.0f)
		return true;
	if (!yt_player_stored_name(player, row, &prefix, error))
		return false;
	if (prefix + sizeof(suffix) - 1U > capacity) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation,
			    sizeof(error->operation), "%s",
			    "killer row capacity");
		}
		return false;
	}
	memcpy(row + prefix, suffix, sizeof(suffix) - 1U);
	*length = prefix + sizeof(suffix) - 1U;
	*emit = true;
	return true;
}

bool
yt_player_name_matches(const struct yt_player *player, const uint8_t *name,
    size_t length, bool *matches, struct yt_error *error)
{
	uint8_t stored_name[YT_TEXT_FIELD_SIZE];
	size_t stored;

	if (matches != NULL)
		*matches = false;
	if (!yt_player_stored_name(player, stored_name, &stored, error))
		return false;
	if (matches != NULL)
		*matches = length == stored
		    && (stored == 0 || memcmp(stored_name, name,
		    stored) == 0);
	return true;
}

void
yt_player_construct(struct yt_player *player, const struct yt_config *config,
    float today)
{
	player->last_active = today;
	player->killed_by = 0;
	player->turns = config->turns_per_day;
	player->shields = 100;
	player->sector = 1;
	player->fighters = config->initial_fighters;
	player->holds = config->initial_holds;
	player->ore = 0;
	player->organics = 0;
	player->equipment = 0;
	player->credits = config->initial_credits;
	player->team = 0;
	player->danger_scanner = 0;
	player->missiles = 1;
	yt_record_set_number(&player->record, YT_F101, 0);
	player->lottery_plays = 0;
	player->plasma = 0;
	player->ports_owned = 0;
	player->ground_forces = 0;
	player->cloak = 1;
	player->mines = 0;
}
