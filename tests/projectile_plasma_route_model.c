#include "projectile_plasma_route_model.h"

#include "qb.h"

#include <math.h>
#include <stdio.h>

bool
test_projectile_plasma_route_run(
    struct test_projectile_plasma_route_state *state,
    const struct test_projectile_plasma_route_ops *ops, void *context,
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
			} else {
				state->route[0] = (int16_t)destination_index;
				state->route[destination_index] = 0;
			}
		} else {
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
			enum test_projectile_plasma_impact_route impact_route;
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
				span = (state->port_record_offset -
				    state->sector_record_offset);
				*state->destination = floorf((
				    (draw * span) + 1.0f));
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
