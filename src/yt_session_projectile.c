#include "yt_session_projectile_internal.h"

#include "qb.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

struct projectile_route_state {
	float origin;
	float destination;
	float amount;
};

static bool
projectile_opening(struct yt_session *session, float amount, bool plasma,
    float *last_mine_news_sector, double *energy, float *hop_loss,
    uint8_t *attacker, size_t attacker_capacity, size_t *attacker_length,
    struct yt_error *error)
{
	uint8_t player_name[YT_TEXT_FIELD_SIZE];
	size_t player_name_length;

	if (!plasma) {
		static const uint8_t loading[] =
		    "Loading course into misile targeting computer.";
		static const uint8_t tracking[] = "*** Tracking Report ***";

		*energy = 0.0;
		*hop_loss = 0.0f;
		*attacker_length = 0U;
		if (!session_sound(session, YT_SOUND_CUE_ACTION,
		    "cruise missile launch sound", error)
		    || !session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE, "cruise missile opening line", error)
		    || !session_present_text(session, loading,
		    sizeof(loading) - 1U, SESSION_PRESENT_RAW,
		    "cruise missile loading text", error)
		    || !session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE, "cruise missile opening line", error))
			return false;
		*last_mine_news_sector = 0.0f;
		return session_present_text(session, tracking,
		    sizeof(tracking) - 1U, SESSION_PRESENT_LINE,
		    "cruise missile tracking row", error);
	}
	static const uint8_t loading[] =
	    "Loading course into targeting computer.";
	static const uint8_t tracking[] = "* Tracking Report *";
	uint8_t row[192];
	size_t row_length;
	float firing_counter;

	player_name_length = yt_player_stored_name(&session->player,
	    player_name);
	if (player_name_length > attacker_capacity)
		return false;
	if (player_name_length != 0U)
		memcpy(attacker, player_name, player_name_length);
	*attacker_length = player_name_length;
	yt_projectile_plasma_opening_values(amount, energy, hop_loss);
	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "plasma opening line", error)
	    || !session_present_text(session, loading, sizeof(loading) - 1U,
	    SESSION_PRESENT_RAW, "plasma loading text", error)
	    || !session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "plasma opening line", error)
	    || !session_sound(session, YT_SOUND_CUE_ACTION, "plasma launch sound", error)
	    || !session_wait(session, 1.0, "plasma launch wait", error)
	    || !yt_projectile_plasma_energy_row(*energy, row, sizeof(row),
	    &row_length)
	    || !session_present_text(session, row, row_length,
	    SESSION_PRESENT_LINE, "plasma opening line", error)
	    || !session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "plasma opening line", error)
	    || !session_wait(session, 1.0, "plasma opening wait", error))
		return false;
	firing_counter = 1.0f;
	while (firing_counter <= amount) {
		if (!yt_projectile_plasma_firing_row(firing_counter, row,
		    sizeof(row), &row_length)
		    || !session_present_text(session, row, row_length,
		    SESSION_PRESENT_LINE, "plasma opening line", error)
		    || !session_sound(session, YT_SOUND_CUE_LAUNCH,
		    "plasma bolt firing sound", error))
			return false;
		firing_counter = yt_projectile_plasma_next_firing(firing_counter);
	}
	return session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "plasma opening line", error)
	    && session_present_text(session, tracking, sizeof(tracking) - 1U,
	    SESSION_PRESENT_LINE, "plasma opening line", error)
	    && session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "plasma opening line", error);
}

static bool
missile_route_failure_suffix(struct yt_session *session,
    struct yt_error *error)
{
	static const uint8_t row[] = "Missles self destructed!";

	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "cruise missile self-destruct blank", error))
		return false;
	yt_present_set_blink(&session->presentation, 1.0f);
	return session_present_text(session, row, sizeof(row) - 1U,
	    SESSION_PRESENT_BOLD_LINE, "cruise missile self-destruct row", error);
}

static bool
plasma_footer(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t row[] = "Plasma bolts dissipated.";

	return session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "plasma footer leading blank", error)
	    && session_present_text(session, row, sizeof(row) - 1U,
	    SESSION_PRESENT_LINE, "plasma footer row", error)
	    && session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "plasma footer trailing blank", error);
}

static bool
missile_footer(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t row[] = "*** End of Report ***";

	return session_present_text(session, row, sizeof(row) - 1U,
	    SESSION_PRESENT_LINE,
	    "cruise missile end report", error);
}

static bool
plasma_route_run(struct yt_session *session,
    struct projectile_route_state *route, float *origin, float *destination,
    double *energy, float hop_loss, int *xannor_provoker,
    const uint8_t *attacker, size_t attacker_length, struct yt_error *error)
{
	char first[64];
	char second[64];
	uint8_t row[192];
	size_t steps = 0U;
	struct session_route_plan plan;

	for (;;) {
		bool overflow;
		int destination_index;
		float current_hop;

		if (++steps > YT_ROUTE_CAPACITY * 4U)
			return false;
		if (*destination == *origin) {
			destination_index = qb_cint(*destination, &overflow);
			if (overflow)
				return false;
			*origin = 0.0f;
			route->origin = 0.0f;
			plan.next_hop[0] = (int16_t)destination_index;
			plan.next_hop[(int16_t)destination_index] = 0;
		} else {
			if (!yt_session_build_route(session, route->origin,
			    route->destination, false, &plan, error))
				return false;
			*origin = route->origin;
			*destination = route->destination;
		}
		current_hop = *origin;
		for (;;) {
			struct yt_sector sector;
			int current_index;
			int next_hop;
			int written;

			if (++steps > YT_ROUTE_CAPACITY * 4U)
				return false;
			if (current_hop != *origin)
				*energy -= (double)hop_loss;
			current_index = qb_cint(current_hop, &overflow);
			if (overflow)
				return false;
			next_hop = plan.next_hop[(int16_t)current_index];
			current_hop = (float)next_hop;
			if (next_hop == 0 || *energy < 1.0)
				return plasma_footer(session, error);
			if (qb_str_single(first, sizeof(first), (float)next_hop) < 0
			    || qb_str_double(second, sizeof(second), floor(*energy)) < 0)
				return false;
			written = snprintf((char *)row, sizeof(row),
			    "Bolt entering sector%s.%s Megawatts remaining.",
			    first, second);
			if (written < 0 || (size_t)written >= sizeof(row)
			    || !session_present_text(session, row, (size_t)written,
			    SESSION_PRESENT_LINE, "plasma route line", error)
			    || !session_wait(session, 0.5, "plasma hop wait", error))
				return false;
			if (next_hop == session->disruption_sectors[0]
			    || next_hop == session->disruption_sectors[1]) {
				float draw;
				float span;

				*origin = (float)next_hop;
				route->origin = *origin;
				if (!yt_random_next(&session->door->game.random, &draw,
				    error))
					return false;
				span = qb_single_subtract(
				    (float)session_port_offset(session),
				    (float)session_sector_offset(session));
				*destination = floorf(qb_single_add(qb_single_multiply(draw, span),
				    1.0f));
				route->destination = *destination;
				if (!session_present_text(session, NULL, 0U,
				    SESSION_PRESENT_LINE, "plasma route line", error)
				    || qb_str_single(first, sizeof(first),
				    (float)next_hop) < 0
				    || qb_str_single(second, sizeof(second),
				    *destination) < 0)
					return false;
				written = snprintf((char *)row, sizeof(row),
				    "The plasma bolt is deflected by a black hole in "
				    "sector%s to sector%s!", first, second);
				if (written < 0 || (size_t)written >= sizeof(row)
				    || !session_attention_bytes(session, row,
				    (size_t)written, "plasma black-hole attention", error)
				    || !session_present_text(session, NULL, 0U,
				    SESSION_PRESENT_LINE, "plasma route line", error))
					return false;
				break;
			}
			if (!session_read_sector(session, next_hop, &sector, error))
				return false;
			if (!yt_projectile_sector_has_presence(&sector, next_hop,
			    session_sector_offset(session), &session->player_cache,
			    xannor_provoker != NULL ? *xannor_provoker : 0))
				continue;
			if (!yt_session_plasma_sector(session, next_hop, &sector, attacker,
			    attacker_length, energy, error))
				return false;
			if (*energy < 1.0)
				return plasma_footer(session, error);
		}
	}
}

static bool
launch_projectile(struct yt_session *session, float *target, float *amount,
    bool plasma, struct projectile_route_state *route,
    float *origin_alias,
    int *pending_counterattack, int *pending_xannor, struct yt_error *error)
{
	float destination = *target;
	bool overflow;
	int cursor;
	float *missiles = amount;
	double energy;
	float hop_loss;
	uint8_t attacker[YT_PROJECTILE_ATTACKER_CAPACITY];
	size_t attacker_length;
	struct session_route_plan plan;
	int local_counterattack = 0;
	int local_xannor_provoker = 0;
	int *counterattack = pending_counterattack != NULL
	    ? pending_counterattack : &local_counterattack;
	int *xannor_provoker = pending_xannor != NULL
	    ? pending_xannor : &local_xannor_provoker;
	float last_mine_news_sector;
	int start = origin_alias != NULL
	    ? (int)*origin_alias : session->player.sector;

	route->origin = *origin_alias;
	route->destination = *target;
	route->amount = *missiles;
	(void)qb_cint_mode((double)route->destination,
	    session->presentation.sound.conversion_mode, &overflow);
	if (overflow)
		return true;
	if (!projectile_opening(session, *amount, plasma,
	    &last_mine_news_sector, &energy, &hop_loss, attacker,
	    sizeof(attacker), &attacker_length, error))
		return false;
	if (plasma) {
		float local_origin = (float)start;
		float *origin = origin_alias != NULL ? origin_alias : &local_origin;

		return plasma_route_run(session, route, origin, target, &energy,
		    hop_loss, xannor_provoker, attacker, attacker_length, error);
	}
	for (;;) {
		bool rerouted = false;
		bool use_avoid = yt_projectile_route_avoid_enabled(plasma,
		    *counterattack, session_record(session));

		bool route_success = yt_session_build_route(session,
		    route->origin, route->destination, use_avoid, &plan, error);

		*origin_alias = route->origin;
		*target = route->destination;
		*missiles = route->amount;
		start = (int)*origin_alias;
		destination = *target;
		if (!route_success)
			return false;
		if (plan.outcome == YT_ROUTE_NOT_FOUND
		    || (plan.outcome == YT_ROUTE_SAME && use_avoid)) {
			if (!missile_route_failure_suffix(session, error))
				return false;
			return true;
		}
		if ((float)session_record(session) > 2.0f
		    && (float)session_record(session)
		    <= (float)session_sector_offset(session)) {
			struct yt_player shooter;

			if (!yt_game_read_player(&session->door->game,
			    session_record(session), &shooter, error))
				return false;
		}
		cursor = start;
		for (;;) {
			int next = plan.next_hop[cursor];

			if (!yt_projectile_route_has_next((int16_t)next))
				break;
			if (session_is_disruption_sector(session, next)) {
				uint8_t row[160];
				size_t row_length;
				float draw;

				if (!session_present_text(session, NULL, 0U,
				    SESSION_PRESENT_LINE, "cruise black-hole blank", error)
				    || !yt_projectile_cruise_reroute_row((float)next, row,
				    sizeof(row), &row_length)
				    || !session_attention_bytes(session, row, row_length,
				    "cruise black-hole attention", error)) {
					route->origin = *origin_alias;
					route->destination = *target;
					route->amount = *missiles;
					return false;
				}
				*origin_alias = (float)next;
				route->origin = *origin_alias;
				route->destination = *target;
				route->amount = *missiles;
				if (!yt_random_next(&session->door->game.random, &draw, error))
					return false;
				*target = yt_projectile_cruise_reroute_destination(draw,
				    session_sector_offset(session),
				    session_port_offset(session));
				route->origin = *origin_alias;
				route->destination = *target;
				route->amount = *missiles;
				start = next;
				destination = *target;
				rerouted = true;
				break;
			}
			static const uint8_t union_police_row[] =
			    "The Union Police have destroyed the Missiles!";

			if (yt_projectile_union_police_admitted((float)next,
			    destination, *counterattack, *xannor_provoker)) {
				if (!session_present_text(session, union_police_row,
				    sizeof(union_police_row) - 1U, SESSION_PRESENT_LINE,
				    "Union Police missile row", error))
					return false;
				return true;
			}
			enum yt_missile_sector_route sector_route;

			bool sector_success = yt_session_missile_sector(session, next,
			    missiles,
			    counterattack, xannor_provoker, &last_mine_news_sector,
			    &sector_route, error);

			route->amount = *missiles;
			if (!sector_success)
				return false;
			if (sector_route == MISSILE_SECTOR_RETURN)
				return true;
			if (yt_projectile_post_impact_route(*missiles)
			    == YT_PROJECTILE_POST_IMPACT_FOOTER)
				break;
			cursor = next;
		}
		if (!rerouted)
			break;
	}
	if (!missile_footer(session, error))
		return false;
	return true;
}

bool
session_launch_projectile(struct yt_session *session, float *origin,
    float *target,
    float *amount, bool plasma, int *counterattack, int *xannor_provoker,
    struct yt_error *error)
{
	struct projectile_route_state route;
	bool result;

	if (counterattack != NULL)
		*counterattack = session->projectile.pending_counterattack_player;
	if (xannor_provoker != NULL)
		*xannor_provoker = session->projectile.pending_xannor_provoker;
	result = launch_projectile(session, target, amount, plasma, &route, origin,
	    counterattack, xannor_provoker, error);
	if (counterattack != NULL)
		*counterattack = session->projectile.pending_counterattack_player;
	if (xannor_provoker != NULL)
		*xannor_provoker = session->projectile.pending_xannor_provoker;
	return result;
}

static bool
session_counterlaunch_projectile(struct yt_session *session, float *origin,
    float *target,
    float *amount, bool plasma, int *counterattack, int *xannor_provoker,
    struct yt_error *error)
{
	struct projectile_route_state route;
	bool result;

	if (counterattack != NULL)
		*counterattack = session->projectile.pending_counterattack_player;
	if (xannor_provoker != NULL)
		*xannor_provoker = session->projectile.pending_xannor_provoker;
	result = launch_projectile(session, target, amount, plasma,
	    &route, origin,
	    counterattack, xannor_provoker, error);
	if (counterattack != NULL)
		*counterattack = session->projectile.pending_counterattack_player;
	if (xannor_provoker != NULL)
		*xannor_provoker = session->projectile.pending_xannor_provoker;
	return result;
}

static bool
launch_player_counterattack(struct yt_session *session, int *counterattacker,
    int *xannor_provoker, struct yt_error *error)
{
	struct yt_player saved_player;
	struct yt_player attacker;
	struct yt_player debit_player;
	struct yt_player final_player;
	int saved_record;
	float available;
	float target;
	float origin;
	uint8_t stored_name[YT_TEXT_FIELD_SIZE];
	uint8_t saved_name[YT_TEXT_FIELD_SIZE];
	size_t stored_name_length;
	size_t saved_name_length;
	uint8_t terminal_row[256];
	uint8_t news_row[256];
	size_t terminal_length;
	size_t news_length;
	char attacker_name[YT_TEXT_FIELD_SIZE + 1U];
	bool valid_cache;
	float saved_cloak;

	if (counterattacker != NULL)
		*counterattacker = session->projectile.pending_counterattack_player;
	session->active_player_record = session_record(session);
	saved_record = session_record(session);
	if (*counterattacker < YT_PLAYER_FIRST_RECORD
	    || *counterattacker > session_sector_offset(session)
	    || *counterattacker == saved_record)
		return true;
	if (!yt_game_read_player(&session->door->game, *counterattacker,
	    &attacker, error))
		return false;
	available = attacker.missiles;
	if (attacker.killed_by != 0
	    || available < 1.0f) {
		*counterattacker = 0;
		return true;
	}

	saved_player = session->player;
	target = (float)saved_player.sector;
	saved_name_length = strlen(saved_player.name);
	if (saved_name_length > sizeof(saved_name))
		saved_name_length = sizeof(saved_name);
	memcpy(saved_name, saved_player.name, saved_name_length);
	valid_cache = yt_player_cache_contains(saved_record);
	if (valid_cache) {
		saved_cloak = yt_player_cache_cloak(&session->player_cache,
		    saved_record);
		(void)yt_player_cache_set_cloak(&session->player_cache,
		    saved_record, 0.0f);
	}
	session->active_player_record = *counterattacker;
	stored_name_length = yt_player_stored_name(&attacker, stored_name);
	memset(attacker_name, 0, sizeof(attacker_name));
	memcpy(attacker_name, stored_name, stored_name_length);
	memcpy(session->player.name, attacker_name, sizeof(session->player.name));

	session->projectile.retained_counterlaunch_missiles = yt_counterlaunch_score_count(
	    (double)saved_player.score, session->projectile.retained_counterlaunch_missiles);
	if (session->projectile.retained_counterlaunch_missiles > available
	    || session->projectile.retained_counterlaunch_missiles == 0.0f) {
		float draw;
		volatile float product;
		volatile float integral;
		volatile float selected;

		if (!yt_random_next(&session->door->game.random, &draw, error))
			return false;
		product = draw * available;
		integral = floorf(product);
		selected = integral + 1.0f;
		session->projectile.retained_counterlaunch_missiles = selected;
	}
	if (!yt_game_read_player(&session->door->game, *counterattacker,
	    &debit_player, error))
		return false;
	yt_counterlaunch_debit_overlay(&debit_player, available,
	    session->projectile.retained_counterlaunch_missiles);
	if (!yt_game_write_player(&session->door->game, *counterattacker,
	    &debit_player, error)
	    || !session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "player counterlaunch blank", error)
	    || !yt_counterlaunch_rows(stored_name, stored_name_length,
	    session->projectile.retained_counterlaunch_missiles, saved_name, saved_name_length,
	    terminal_row, sizeof(terminal_row), &terminal_length, news_row,
	    sizeof(news_row), &news_length)
	    || !session_present_text(session, terminal_row, terminal_length,
	    SESSION_PRESENT_BOLD_LINE, "player counterlaunch row", error)
	    || !yt_news_append_bytes(news_row, news_length, error))
		return false;
	origin = (float)attacker.sector;
	if (!session_counterlaunch_projectile(session, &origin, &target,
	    &session->projectile.retained_counterlaunch_missiles, false, counterattacker,
	    xannor_provoker, error))
		return false;

	*counterattacker = 0;
	session->active_player_record = saved_record;
	session->player = saved_player;
	if (valid_cache)
		(void)yt_player_cache_set_cloak(&session->player_cache,
		    saved_record, saved_cloak);
	if (!yt_game_read_player(&session->door->game, saved_record,
	    &final_player, error))
		return false;
	if (final_player.killed_by != 0)
		session->destroyed = true;
	return session_wait(session, 4.0, "player counterattack wait", error);
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
yt_session_command_projectile(struct yt_session *session, bool plasma,
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
	uint8_t target_raw[4];
	uint8_t amount_raw[4];
	size_t prompt_length;
	double integral;
	float displayed = plasma ? session->player.plasma
	    : session->player.missiles;
	float maximum_sector = (float)session_sector_count(session);
	float available;
	float target;
	float amount;
	float origin;
	struct projectile_route_state route;
	int counterattack;
	int xannor_provoker;

	for (;;) {
		if (!session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE, "projectile target opening blank", error)
		    || !session_reload_player(session, error)
		    || !session_reload_player(session, error))
			return false;
		if (session->player.turns <= 0.0f) {
			return session_present_alert(session, no_turns,
			    sizeof(no_turns) - 1U, "no-turn gate notice", error);
		}
		available = plasma ? session->player.plasma
		    : session->player.missiles;
		if (available < 1.0f)
			return session_present_alert(session, no_ammunition,
			    sizeof(no_ammunition) - 1U,
			    "projectile ammunition refusal", error);
		if (!yt_projectile_target_prompt(plasma, displayed,
		    maximum_sector, prompt, sizeof(prompt), &prompt_length)
		    || !session_present_timed_paged_row(session, prompt,
		    prompt_length, "projectile target prompt", error)
		    || !session_read_number_command(session, response,
		    sizeof(response)))
			return false;
		if (response[0] == '\0')
			return true;
		parsed = qb_val(response);
		if (parsed.overflow)
			return projectile_command_error(error, YT_RANGE,
			    "projectile target VAL");
		target = (float)(parsed.valid ? parsed.value : 0.0);
		conversion = qb_mbf32_encode(target, target_raw);
		if (conversion == QB_MBF_OVERFLOW)
			return projectile_command_error(error, YT_RANGE,
			    "projectile target CSNG");
		target = qb_mbf32_decode(target_raw);
		if (target >= 1.0f && target <= maximum_sector)
			break;
		if (!session_present_alert(session, invalid_sector,
		    sizeof(invalid_sector) - 1U, "projectile invalid sector",
		    error))
			return false;
	}

	if (!session_present_timed_paged_row(session, quantity_prompt,
	    sizeof(quantity_prompt) - 1U, "projectile quantity prompt", error)
	    || !session_read_number_command(session, response, sizeof(response)))
		return false;
	parsed = qb_val(response);
	if (parsed.overflow)
		return projectile_command_error(error, YT_RANGE,
		    "projectile quantity VAL");
	integral = floor(parsed.valid ? parsed.value : 0.0);
	amount = (float)integral;
	conversion = qb_mbf32_encode(amount, amount_raw);
	if (conversion == QB_MBF_OVERFLOW)
		return projectile_command_error(error, YT_RANGE,
		    "projectile quantity CSNG");
	amount = qb_mbf32_decode(amount_raw);
	if (amount < 1.0f)
		return true;
	if (amount > available)
		return session_present_paged_fragment(session, too_many,
		    sizeof(too_many) - 1U);
	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "projectile accepted blank", error))
		return false;
	if (!yt_session_finalize_action(session, error))
		return error == NULL || error->status == YT_OK;
	origin = (float)session->player.sector;
	yt_projectile_debit_overlay(&session->player, plasma, amount);
	if (!yt_game_write_player(&session->door->game, session_record(session),
	    &session->player, error)
	    || !yt_database_flush(&session->door->game.database, error))
		return false;
	session->destroyed = false;
	counterattack = session->projectile.pending_counterattack_player;
	xannor_provoker = session->projectile.pending_xannor_provoker;
	if (!launch_projectile(session, &target, &amount, plasma,
	    &route, &origin, &counterattack, &xannor_provoker, error))
		return false;
	counterattack = session->projectile.pending_counterattack_player;
	xannor_provoker = session->projectile.pending_xannor_provoker;
	if (session->projectile.pending_counterattack_player != 0
	    && !launch_player_counterattack(session, &counterattack,
	    &xannor_provoker, error))
		return false;
	if (session->projectile.pending_xannor_provoker != 0
	    && !yt_session_launch_xannor_retaliation(session,
	    &xannor_provoker, error))
		return false;
	if (session->destroyed)
		return yt_session_common_fatal_self(session, error);
	return true;
}
