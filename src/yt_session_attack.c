#include "yt_session_internal.h"

#include "qb.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static float
attack_single_add(float left, float right)
{
	volatile float result = left + right;

	return result;
}

static float
attack_single_div(float left, float right)
{
	volatile float result = left / right;

	return result;
}

static double
attack_double_add(double left, double right)
{
	volatile double result = left + right;

	return result;
}

static double
attack_double_sub(double left, double right)
{
	volatile double result = left - right;

	return result;
}

bool
session_read_combat_player(struct yt_session *session, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	if (player_record != session_record(session))
		return yt_game_read_player(&session->door->game, player_record,
		    player, error);
	if (!session_reload_player(session, error))
		return false;
	*player = session->player;
	return true;
}

bool
session_write_combat_player(struct yt_session *session, int player_record,
    const struct yt_player *player, struct yt_error *error)
{
	if (player_record == session_record(session))
		session->player = *player;
	return yt_database_write(&session->door->game.database,
	    (size_t)player_record, &player->record, error);
}

bool
yt_session_fighter_shield_spill(struct yt_session *session,
    double *fighters, float *shields, bool bind_hostile_cells,
    struct yt_error *error)
{
	uint8_t fighter_row[128];
	uint8_t shield_row[128];
	size_t fighter_length;
	size_t shield_length;

	while (*fighters > 0.0 && *shields > 0.0f) {
		float draw;

		if (!yt_random_next(&session->door->game.random, &draw, error)
		    || !yt_fighter_shield_spill_step(fighters, shields, draw))
			return false;
		if (bind_hostile_cells) {
			if (draw >= 0.5f)
				session->hostile_deployed_fighters = *fighters;
			else
				session->combat_ship_shields = *shields;
		}
	}
	return yt_fighter_shield_spill_rows(*fighters, *shields,
	    fighter_row, sizeof(fighter_row), &fighter_length,
	    shield_row, sizeof(shield_row), &shield_length)
	    && session_present_text(session, fighter_row, fighter_length,
	    SESSION_PRESENT_LINE, "fighter spill result", error)
	    && session_present_text(session, shield_row, shield_length,
	    SESSION_PRESENT_LINE, "shield spill result", error);
}

static bool
direct_attack_finish_kill(struct yt_session *session, int target_record,
    int current_player_record, float current_sector, float target_shields,
    struct yt_error *error)
{
	struct yt_player target;
	struct yt_sector sector;
	uint8_t saved_name[YT_TEXT_FIELD_SIZE];
	uint8_t warning[128];
	size_t saved_name_length;
	size_t warning_length;
	float saved_mines;
	volatile float deployed;
	bool terminal;

	if (target_shields > 0.0f)
		return true;
	if (!session_sound(session, 3.0f, "player kill sound", error)
	    || !yt_game_read_player(&session->door->game, target_record,
	    &target, error))
		return false;
	saved_mines = target.mines;
	saved_name_length = (size_t)target.name_length;
	if (saved_name_length != 0U)
		memcpy(saved_name, target.record.bytes, saved_name_length);
	if (!yt_session_kill_player(session, target_record,
	    (float)current_player_record, true, error)
	    || !yt_session_salvage_player(session, target_record,
	    current_player_record, error))
		return false;
	if (!(saved_mines > 0.0f))
		return true;
	if (!session_read_sector(session, (int)current_sector, &sector, error))
		return false;
	deployed = sector.mines + saved_mines;
	yt_sector_mine_sector_overlay(&sector, deployed);
	if (!yt_database_write(&session->door->game.database,
	    (size_t)yt_sector_basic_record(&session->door->game.config,
	    (int)current_sector),
	    &sector.record, error)
	    || !yt_direct_fighter_mine_warning(saved_name, saved_name_length,
	    warning, sizeof(warning), &warning_length)
	    || !session_present_alert(session, warning, warning_length,
	    "direct fighter mine warning", error)
	    || !session_append_news_bytes(session, warning, warning_length,
	    error))
		return false;
	terminal = false;
	if (!yt_session_mine_encounter(session, &terminal, error))
		return false;
	if (terminal || !session->destroyed)
		return true;
	return yt_session_common_fatal_self(session, error);
}

static bool
direct_attack_attrition(struct yt_session *session, double committed,
    double defenders, float cloak, double *attacker_loss,
    double *defender_loss, struct yt_error *error)
{
	*attacker_loss = 0.0;
	*defender_loss = 0.0;
	while (*attacker_loss < committed && *defender_loss < defenders) {
		double remaining_attacker = committed - *attacker_loss;
		double remaining_defender = defenders - *defender_loss;
		double minimum = remaining_attacker < remaining_defender
		    ? remaining_attacker : remaining_defender;
		volatile double integral = floor(minimum / 20.0);
		float quantum = (float)integral;
		float sampled;

		if (quantum < 1.0f)
			quantum = 1.0f;
		if (!yt_random_next(&session->door->game.random, &sampled,
		    error))
			return false;
		if (attack_single_add(attack_single_div(cloak, 10.0f), sampled)
		    < 0.44999998807907104f)
			*attacker_loss = attack_double_add(*attacker_loss,
			    (double)quantum);
		else
			*defender_loss = attack_double_add(*defender_loss,
			    (double)quantum);
	}
	return true;
}

bool
yt_session_attack_player(struct yt_session *session, int target_record,
    double committed, struct yt_error *error)
{
	static const uint8_t eliminated[] =
	    "Fighters eliminated! Attacking the ship!";
	struct yt_player current;
	struct yt_player target;
	uint8_t row[300];
	uint8_t second[300];
	uint8_t stored_name[YT_TEXT_FIELD_SIZE];
	uint8_t radio[160];
	size_t row_length;
	size_t second_length;
	size_t name_length;
	size_t radio_length;
	double defenders;
	double cached_reserve;
	double attacking;
	double attacker_loss;
	double defender_loss;
	float target_shields;
	float current_sector;
	float remaining_shields;
	int current_player_record = session_record(session);

	if (!session_read_combat_player(session, target_record, &target, error))
		return false;
	defenders = (double)target.fighters;
	if (!session_read_combat_player(session, current_player_record,
	    &current, error))
		return false;
	if (committed > (double)current.fighters) {
		return yt_direct_attack_too_many_row((double)current.fighters,
		    row, sizeof(row), &row_length)
		    && session_present_alert(session, row, row_length,
		    "direct Attack too-many row", error);
	}

	cached_reserve = attack_double_sub((double)current.fighters, committed);
	yt_direct_attack_fighter_overlay(&current, (float)cached_reserve);
	if (!session_write_combat_player(session, current_player_record,
	    &current, error)
	    || !session_sound(session, 2.0f, "player attack opening sound",
	    error)
	    || !direct_attack_attrition(session, committed, defenders,
	    current.cloak, &attacker_loss, &defender_loss, error))
		return false;
	if (defender_loss > 0.0) {
		if (!yt_player_stored_name(&current, stored_name, &name_length,
		    error)
		    || !yt_direct_attack_radio_text(stored_name, name_length,
		    defender_loss, radio, sizeof(radio), &radio_length)
		    || !session_append_radio_bytes(radio, radio_length, -2.0f,
		    (float)target_record, error))
			return false;
	}

	if (!session_read_combat_player(session, current_player_record,
	    &current, error))
		return false;
	cached_reserve = (double)current.fighters;
	attacking = attack_double_sub(committed, attacker_loss);
	defenders = attack_double_sub(defenders, defender_loss);
	yt_direct_attack_fighter_overlay(&current,
	    (float)attack_double_add(cached_reserve, attacking));
	if (!session_write_combat_player(session, current_player_record,
	    &current, error)
	    || !session_read_combat_player(session, target_record, &target,
	    error))
		return false;
	current_sector = current.sector;
	target_shields = target.shields;
	yt_direct_attack_fighter_overlay(&target, (float)defenders);
	if (!session_write_combat_player(session, target_record, &target, error)
	    || !yt_direct_attack_result_rows(attacker_loss, cached_reserve,
	    defender_loss, defenders, row, sizeof(row), &row_length,
	    second, sizeof(second), &second_length)
	    || !session_present_paged_line(session, row, row_length,
	    "direct Attack attacker result", error)
	    || !session_present_paged_fragment(session, second, second_length))
		return false;
	if (defenders > 0.0 || attacking < 1.0)
		return true;
	if (!session_present_paged_line(session, eliminated,
	    sizeof(eliminated) - 1U, "direct Attack eliminated row", error))
		return false;
	if (target_shields > 0.0f
	    && !yt_session_fighter_shield_spill(session, &attacking,
	    &target_shields, false, error))
		return false;

	remaining_shields = target_shields;
	if (!session_read_combat_player(session, target_record, &target, error))
		return false;
	yt_direct_attack_shield_overlay(&target, remaining_shields);
	if (!session_write_combat_player(session, target_record, &target, error)
	    || !session_read_combat_player(session, current_player_record,
	    &current, error))
		return false;
	yt_direct_attack_fighter_overlay(&current,
	    (float)attack_double_add(cached_reserve, attacking));
	if (!session_write_combat_player(session, current_player_record,
	    &current, error))
		return false;
	if (target_shields > 0.0f)
		return true;
	return direct_attack_finish_kill(session, target_record,
	    current_player_record, current_sector, target_shields, error);
}

static bool
direct_attack_candidate_record(struct yt_session *session, float candidate,
    int *record, struct yt_error *error)
{
	bool overflow;
	int32_t converted;

	converted = qb_cint_mode((double)candidate,
	    session->presentation.sound.conversion_mode, &overflow);
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
yt_session_command_attack(struct yt_session *session, bool *enter_sector,
    struct yt_error *error)
{
	static const uint8_t title[] = "<Attack>";
	static const uint8_t no_fighters[] =
	    "You don't have any fighters.";
	static const uint8_t none_visible[] = "There's no one here!";
	static const uint8_t none_selected[] =
	    "There are no other ships in this sector.";
	struct yt_player current;
	struct yt_player candidate_player;
	uint8_t row[300];
	uint8_t target_name[YT_TEXT_FIELD_SIZE];
	char response[YT_COMMAND_SIZE];
	size_t row_length;
	size_t target_name_length;
	float candidate = 2.0f;
	float target_record_cell = 0.0f;
	bool encountered = false;

	if (enter_sector == NULL)
		return false;
	*enter_sector = false;
	if (!session_present_paged_fragment(session, title, sizeof(title) - 1U)
	    || !session_read_combat_player(session, session_record(session),
	    &current, error))
		return false;
	if (current.fighters < 1.0f)
		return session_present_alert(session, no_fighters,
		    sizeof(no_fighters) - 1U, "direct Attack no-fighters row",
		    error);

	while (candidate <= session_sector_offset(session)) {
		struct qb_val_result parsed;
		enum yt_yes_no_answer answer;
		uint8_t target_record_raw[4];
		float cached_sector;
		float cached_cloak;
		bool sector_mismatch;
		bool self;
		bool cloaked;
		bool positive_team;
		bool same_team;
		int record;

		if (!direct_attack_candidate_record(session, candidate, &record,
		    error))
			return false;
		cached_sector = yt_player_cache_value(&session->player_cache,
		    record, YT_PLAYER_CACHE_SECTOR);
		cached_cloak = yt_player_cache_value(&session->player_cache,
		    record, YT_PLAYER_CACHE_CLOAK);
		sector_mismatch = cached_sector != current.sector;
		self = record == session_record(session);
		cloaked = cached_cloak > 0.0f;
		if (sector_mismatch || self || cloaked) {
			candidate = attack_single_add(candidate, 1.0f);
			continue;
		}

		target_record_cell = candidate;
		(void)qb_mbf32_encode(candidate, target_record_raw);
		session->shared_target_record = qb_mbf32_decode(target_record_raw);
		if (!session_read_combat_player(session, record,
		    &candidate_player, error)
		    || !yt_player_stored_name(&candidate_player, target_name,
		    &target_name_length, error))
			return false;
		positive_team = candidate_player.team > 0.0f;
		same_team = candidate_player.team == current.team;
		if (positive_team && same_team) {
			if (!yt_direct_attack_team_row(target_name,
			    target_name_length, row, sizeof(row), &row_length)
			    || !session_present_paged_fragment(session, row,
			    row_length))
				return false;
			encountered = true;
			candidate = attack_single_add(candidate, 1.0f);
			continue;
		}

		encountered = true;
		if (!yt_direct_attack_candidate_prompt(target_name,
		    target_name_length, row, sizeof(row), &row_length)
		    || !session_confirm(session, row, row_length, &answer, error))
			return false;
		if (answer == YT_YES_NO_NO) {
			candidate = attack_single_add(candidate, 1.0f);
			continue;
		}
		if (answer != YT_YES_NO_YES && answer != YT_YES_NO_EMPTY)
			return false;
		if (!yt_direct_attack_commitment_prompt((double)current.fighters,
		    row, sizeof(row), &row_length)
		    || !session_present_timed_paged_row(session, row, row_length,
		    "direct Attack commitment prompt", error)
		    || !session_read_number_command(session, response,
		    sizeof(response)))
			return false;
		parsed = qb_val(response);
		if ((parsed.valid ? parsed.value : 0.0) < 1.0
		    || target_record_cell < 1.0f)
			return true;
		return yt_session_attack_player(session, record, parsed.value,
		    error);
	}

	if (encountered) {
		if (!session_present_paged_fragment(session, none_selected,
		    sizeof(none_selected) - 1U))
			return false;
	} else if (!session_present_alert(session, none_visible,
	    sizeof(none_visible) - 1U, "direct Attack no-visible-target row",
	    error)) {
		return false;
	}
	*enter_sector = true;
	return true;
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

static bool
hostile_surrender_run(struct yt_session *session,
    struct yt_hostile_surrender_state *state, struct yt_error *error)
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
	enum yt_yes_no_answer answer;
	bool accepted = false;

	state->fighter_owner = state->old_owner;
	state->deployed_remaining = state->deployed_fighters;
	state->checked = false;
	state->accepted = false;
	state->complete = false;
	if (!session_read_combat_player(session, state->current_player_record,
	    &state->current, error))
		return false;
	state->ship_fighters = (double)state->current.fighters;
	state->owner_route = yt_hostile_surrender_route(state->old_owner);
	if (!session_present_paged_line(session, radio, sizeof(radio) - 1U,
	    "surrender radio row", error)
	    || !session_sound(session, 4.0f, "hostile surrender sound", error))
		return false;
	if (qb_str_single(sector_number, sizeof(sector_number),
	    state->current.sector) < 0)
		return false;
	sector_length = strlen(sector_number);
	position = 0U;
	if (!attack_append(captain, sizeof(captain), &position,
	    captain_prefix, sizeof(captain_prefix) - 1U)
	    || !attack_append(captain, sizeof(captain), &position,
	    (const uint8_t *)sector_number, sector_length)
	    || !session_present_paged_line(session, captain, position,
	    "surrender captain row", error))
		return false;

	switch (state->owner_route) {
	case YT_HOSTILE_SURRENDER_PLAYER:
		if (!session_present_alert(session, wish, sizeof(wish) - 1U,
		    "surrender wish row", error)
		    || !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "surrender prompt blank", error)
		    || !session_confirm(session, prompt, sizeof(prompt) - 1U,
		    &answer, error))
			return false;
		if (answer != YT_YES_NO_NO && answer != YT_YES_NO_YES
		    && answer != YT_YES_NO_EMPTY)
			return false;
		accepted = answer == YT_YES_NO_YES
		    || answer == YT_YES_NO_EMPTY;
		break;
	case YT_HOSTILE_SURRENDER_XANNOR:
		if (!session_present_paged_fragment(session, xannor_refusal,
		    sizeof(xannor_refusal) - 1U)
		    || !session_sound(session, 5.0f,
		    "hostile surrender sound", error))
			return false;
		break;
	case YT_HOSTILE_SURRENDER_MERCENARY:
		position = 0U;
		if (!attack_append(refusal, sizeof(refusal),
		    &position, mercenary_prefix, sizeof(mercenary_prefix) - 1U)
		    || !attack_append(refusal, sizeof(refusal),
		    &position, state->real_first_name,
		    state->real_first_name_length)
		    || !attack_append(refusal, sizeof(refusal),
		    &position, mercenary_suffix,
		    sizeof(mercenary_suffix) - 1U)
		    || !session_present_paged_fragment(session, refusal, position)
		    || !session_sound(session, 5.0f,
		    "hostile surrender sound", error))
			return false;
		break;
	case YT_HOSTILE_SURRENDER_QUIET:
		break;
	}
	session->shared_status = 1.0f;
	state->checked = true;
	state->accepted = accepted;
	if (!accepted) {
		state->complete = true;
		return true;
	}
	if (!session_present_paged_line(session, joined, sizeof(joined) - 1U,
	    "surrender joined row", error)
	    || !session_sound(session, 1.0f, "hostile surrender sound", error))
		return false;
	state->surrendered_fighters = attack_double_sub(state->deployed_fighters,
	    state->defender_loss);
	if (qb_str_double(surrendered_number, sizeof(surrendered_number),
	    state->surrendered_fighters) < 0)
		return false;
	surrendered_length = strlen(surrendered_number);
	position = 0U;
	if (!attack_append(news, sizeof(news), &position,
	    (const uint8_t *)surrendered_number, surrendered_length)
	    || !attack_append(news, sizeof(news), &position,
	    news_middle_one, sizeof(news_middle_one) - 1U)
	    || !attack_append(news, sizeof(news), &position,
	    (const uint8_t *)sector_number, sector_length)
	    || !attack_append(news, sizeof(news), &position,
	    news_middle_two, sizeof(news_middle_two) - 1U)
	    || !attack_append(news, sizeof(news), &position,
	    state->cached_player_name, state->cached_player_name_length)
	    || !session_append_news_bytes(session, news, position, error))
		return false;
	state->ship_fighters = attack_double_add(attack_double_sub(attack_double_sub(
	    (double)state->current.fighters, state->attacker_loss),
	    state->defender_loss), state->deployed_fighters);
	state->current.fighters = (float)state->ship_fighters;
	state->deployed_remaining = 0.0;
	state->fighter_owner = 0.0f;
	session->combat_ship_fighters = state->ship_fighters;
	session->hostile_deployed_fighters = state->deployed_remaining;
	position = 0U;
	if (!attack_append(count, sizeof(count), &position,
	    (const uint8_t *)surrendered_number, surrendered_length)
	    || !attack_append(count, sizeof(count), &position,
	    count_suffix, sizeof(count_suffix) - 1U)
	    || !session_present_paged_fragment(session, count, position))
		return false;
	state->complete = true;
	return true;
}

static bool
hostile_attack_persistence_run(struct yt_session *session,
    struct yt_hostile_attack_persistence_state *state,
    struct yt_error *error)
{
	static const uint8_t destroyed[] = " destroyed";
	static const uint8_t belonging[] = " fighters belonging to ";
	uint8_t news[320];
	char loss_number[64];
	size_t position = 0U;
	int loss_length;

	state->route = YT_HOSTILE_ATTACK_PERSISTENCE_NORMAL;
	state->player_written = false;
	state->sector_written = false;
	state->post_loss_read = false;
	state->news_written = false;
	state->mercenaries_hurt = false;
	state->complete = false;
	if (!session_read_combat_player(session, state->current_player_record,
	    &state->current, error))
		return false;
	yt_deployed_attack_player_overlay(&state->current, state->shields,
	    (float)state->ship_fighters);
	if (!session_write_combat_player(session, state->current_player_record,
	    &state->current, error))
		return false;
	state->player_written = true;
	if (!session_read_sector(session, state->current_sector, &state->sector,
	    error))
		return false;
	yt_deployed_attack_sector_overlay(&state->sector,
	    (float)state->deployed_fighters);
	if (!yt_database_write(&session->door->game.database,
	    (size_t)yt_sector_basic_record(&session->door->game.config,
	    state->current_sector), &state->sector.record, error))
		return false;
	state->sector_written = true;
	if (state->ship_fighters < 1.0 && state->shields < 1.0f) {
		state->route = YT_HOSTILE_ATTACK_PERSISTENCE_FATAL;
		if (!yt_session_common_fatal_self(session, error))
			return false;
		state->complete = true;
		return true;
	}
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "deployed attack post-persist blank", error))
		return false;
	if (state->defender_loss > 0.0) {
		if (!session_read_combat_player(session,
		    state->current_player_record, &state->current, error))
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
		    || !session_append_news_bytes(session, news, position, error))
			return false;
		state->news_written = true;
		state->mercenaries_hurt = state->old_owner == -2.0f;
	}
	state->complete = true;
	return true;
}

static bool
hostile_attack_tail_run(struct yt_session *session,
    struct yt_hostile_attack_tail_state *state,
    const char *cached_player_name, struct yt_error *error)
{
	uint8_t display[240];
	uint8_t news[300];
	uint8_t defeated[160];
	size_t display_length;
	size_t news_length;
	size_t defeated_length;

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
		if (!session_read_combat_player(session,
		    state->current_player_record, &state->current, error))
			return false;
		(void)snprintf(session->player.name,
		    sizeof(session->player.name), "%s", cached_player_name);
		(void)snprintf(state->current.name,
		    sizeof(state->current.name), "%s", cached_player_name);
		state->player_read = true;
		state->ship_fighters = (double)state->current.fighters;
		state->bonus = yt_xannor_attack_bonus(state->defender_loss,
		    state->current.turns, state->turns_per_day);
		if (state->bonus >= 1.0f) {
			state->current.turns = attack_single_add(state->current.turns,
			    state->bonus);
			(void)yt_record_set_number(&state->current.record, YT_F49,
			    state->current.turns);
			if (!session_write_combat_player(session,
			    state->current_player_record, &state->current, error))
				return false;
			state->player_written = true;
			if (!yt_xannor_attack_reward_rows(
			    state->cached_player_name,
			    state->cached_player_name_length, state->bonus,
			    state->defender_loss, display, sizeof(display),
			    &display_length, news, sizeof(news), &news_length))
				return false;
			yt_present_set_bold(&session->presentation, 1.0f);
			if (!session_present_paged_fragment(session, display,
			    display_length))
				return false;
			state->reward_presented = true;
			if (!session_append_news_bytes(session, news, news_length,
			    error))
				return false;
			state->reward_news_written = true;
			if (state->deployed_fighters < 1.0) {
				if (!yt_session_clearance(session, true, error))
					return false;
				state->clearance_called = true;
			}
		}
	}
	if (!yt_random_next(&session->door->game.random,
	    &state->dominated_draw, error))
		return false;
	state->draw_consumed = true;
	if (state->deployed_fighters <= 0.0) {
		if (!yt_hostile_defeated_row(state->ship_fighters, defeated,
		    sizeof(defeated), &defeated_length)
		    || !session_present_paged_fragment(session, defeated,
		    defeated_length))
			return false;
		state->defeated_presented = true;
		if (state->old_owner == -1.0f
		    && state->current.sector == state->headquarters) {
			if (!yt_session_xannor_victory(session, error))
				return false;
			state->victory_called = true;
		}
	}
	state->complete = true;
	return true;
}

bool
yt_session_attack_deployed(struct yt_session *session,
    struct yt_sector *sector, double commitment, bool allow_surrender,
    struct yt_error *error)
{
	static const uint8_t lost_prefix[] = " You lost";
	static const uint8_t lost_suffix[] = " fighter(s)";
	static const uint8_t destroyed_prefix[] = " You destroyed";
	static const uint8_t destroyed_suffix[] = " enemy fighters.";
	static const uint8_t exposed[] =
	    "Fighters gone! Enemy attacking your ship!";
	uint8_t cached_player_name[YT_TEXT_FIELD_SIZE];
	uint8_t lost_row[128];
	uint8_t destroyed_row[128];
	char cached_player_name_text[sizeof(session->player.name)];
	char attacker_number[64];
	char defender_number[64];
	struct yt_sector opened_sector;
	struct yt_player current;
	struct yt_hostile_surrender_state surrender;
	struct yt_hostile_attack_persistence_state persistence;
	struct yt_hostile_attack_tail_state tail;
	size_t cached_player_name_length;
	size_t lost_length = 0U;
	size_t destroyed_length = 0U;
	double old_count = session->hostile_deployed_fighters;
	double old_ship;
	double attacker_loss = 0.0;
	double defender_loss = 0.0;
	double ship_fighters;
	double deployed_remaining = old_count;
	float old_owner;
	float quantum;
	float last_draw;
	int current_player_record = session_record(session);
	int current_sector = (int)session->player.sector;
	int attacker_length;
	int defender_length;
	bool surrender_checked = false;
	bool surrendered = false;
	bool child_result;

	if (!yt_player_stored_name(&session->player, cached_player_name,
	    &cached_player_name_length, error))
		return false;
	(void)snprintf(cached_player_name_text,
	    sizeof(cached_player_name_text), "%s", session->player.name);
	if (!session_read_sector(session, current_sector, &opened_sector, error))
		return false;
	old_owner = qb_mbf32_decode(&opened_sector.record.bytes[YT_F85]);
	if (!session_read_combat_player(session, current_player_record,
	    &current, error))
		return false;
	(void)snprintf(session->player.name, sizeof(session->player.name),
	    "%s", cached_player_name_text);
	(void)snprintf(current.name, sizeof(current.name), "%s",
	    cached_player_name_text);
	current.fighters = (float)session->combat_ship_fighters;
	current.cloak = session->player.cloak;
	current.shields = session->combat_ship_shields;
	old_ship = (double)current.fighters;
	if (!session_sound(session, 2.0f, "deployed attack opening sound",
	    error))
		return false;

	do {
		double remaining_attacker = attack_double_sub(commitment,
		    attacker_loss);
		double remaining_defender = attack_double_sub(old_count,
		    defender_loss);
		volatile double ratio;

		quantum = yt_hostile_attack_quantum(remaining_attacker,
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
		if (!surrender_checked && allow_surrender && ratio > 10.0) {
			surrender = (struct yt_hostile_surrender_state){
				.current_player_record = current_player_record,
				.old_owner = old_owner,
				.attacker_loss = attacker_loss,
				.defender_loss = defender_loss,
				.deployed_fighters = old_count,
				.cached_player_name = cached_player_name,
				.cached_player_name_length =
				    cached_player_name_length,
				.real_first_name =
				    (const uint8_t *)session->door->identity.real_first,
				.real_first_name_length =
				    strlen(session->door->identity.real_first),
				.current = current,
			};
			child_result = hostile_surrender_run(session, &surrender,
			    error);
			session->player = surrender.current;
			(void)snprintf(session->player.name,
			    sizeof(session->player.name), "%s",
			    cached_player_name_text);
			current = surrender.current;
			old_ship = surrender.ship_fighters;
			surrender_checked = surrender.checked;
			surrendered = surrender.accepted;
			session->player = current;
			(void)snprintf(session->player.name,
			    sizeof(session->player.name), "%s",
			    cached_player_name_text);
			if (!child_result)
				return false;
			if (surrendered) {
				ship_fighters = surrender.ship_fighters;
				deployed_remaining = surrender.deployed_remaining;
				sector->fighter_owner = surrender.fighter_owner;
				current.fighters = (float)ship_fighters;
				session->player = current;
				(void)snprintf(session->player.name,
				    sizeof(session->player.name), "%s",
				    cached_player_name_text);
				session->hostile_deployed_fighters =
				    deployed_remaining;
				break;
			}
		}
		if (!yt_random_next(&session->door->game.random, &last_draw, error))
			return false;
		if (yt_hostile_attack_loses_attacker(current.cloak, last_draw))
			attacker_loss = attack_double_add(attacker_loss, (double)quantum);
		else
			defender_loss = attack_double_add(defender_loss, (double)quantum);
	} while (attacker_loss < commitment && defender_loss < old_count);
	if (attacker_loss > commitment)
		attacker_loss = commitment;
	if (defender_loss > old_count)
		defender_loss = old_count;
	if (!surrendered) {
		ship_fighters = attack_double_sub(old_ship, attacker_loss);
		deployed_remaining = attack_double_sub(old_count, defender_loss);
		current.fighters = (float)ship_fighters;
		session->combat_ship_fighters = ship_fighters;
		session->player = current;
		(void)snprintf(session->player.name,
		    sizeof(session->player.name), "%s",
		    cached_player_name_text);
	}
	attacker_length = qb_str_double(attacker_number,
	    sizeof(attacker_number), attacker_loss);
	defender_length = qb_str_double(defender_number,
	    sizeof(defender_number), defender_loss);
	if (attacker_length < 0 || defender_length < 0
	    || !attack_append(lost_row, sizeof(lost_row),
	    &lost_length, lost_prefix, sizeof(lost_prefix) - 1U)
	    || !attack_append(lost_row, sizeof(lost_row),
	    &lost_length, (const uint8_t *)attacker_number,
	    (size_t)attacker_length)
	    || !attack_append(lost_row, sizeof(lost_row),
	    &lost_length, lost_suffix, sizeof(lost_suffix) - 1U)
	    || !attack_append(destroyed_row,
	    sizeof(destroyed_row), &destroyed_length, destroyed_prefix,
	    sizeof(destroyed_prefix) - 1U)
	    || !attack_append(destroyed_row,
	    sizeof(destroyed_row), &destroyed_length,
	    (const uint8_t *)defender_number, (size_t)defender_length)
	    || !attack_append(destroyed_row,
	    sizeof(destroyed_row), &destroyed_length, destroyed_suffix,
	    sizeof(destroyed_suffix) - 1U)
	    || !session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "deployed attack result blank", error)
	    || !session_present_paged_fragment(session, lost_row, lost_length)
	    || !session_present_paged_fragment(session, destroyed_row,
	    destroyed_length))
		return false;
	if (ship_fighters < 1.0 && deployed_remaining > 0.0) {
		if (!session_present_alert(session, exposed, sizeof(exposed) - 1U,
		    "deployed attack ship exposed", error)
		    || !session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE, "shield spill leading blank", error))
			return false;
		if (!yt_session_fighter_shield_spill(session,
		    &deployed_remaining, &current.shields, true, error)) {
			session->player = current;
			(void)snprintf(session->player.name,
			    sizeof(session->player.name), "%s",
			    cached_player_name_text);
			return false;
		}
		session->player = current;
		(void)snprintf(session->player.name,
		    sizeof(session->player.name), "%s",
		    cached_player_name_text);
	}
	sector->fighters = (float)deployed_remaining;
	session->hostile_deployed_fighters = deployed_remaining;
	persistence = (struct yt_hostile_attack_persistence_state){
		.current_player_record = current_player_record,
		.current_sector = current_sector,
		.ship_fighters = ship_fighters,
		.shields = current.shields,
		.deployed_fighters = deployed_remaining,
		.defender_loss = defender_loss,
		.old_owner = old_owner,
		.cached_player_name = cached_player_name,
		.cached_player_name_length = cached_player_name_length,
		.owner_label = session->hostile_owner_label,
		.owner_label_length = session->hostile_owner_label_length,
		.current = current,
		.sector = *sector,
	};
	child_result = hostile_attack_persistence_run(session, &persistence,
	    error);
	current = persistence.current;
	ship_fighters = persistence.ship_fighters;
	if (persistence.route != YT_HOSTILE_ATTACK_PERSISTENCE_FATAL) {
		session->player = persistence.current;
		(void)snprintf(session->player.name,
		    sizeof(session->player.name), "%s",
		    cached_player_name_text);
	}
	if (persistence.sector_written) {
		*sector = persistence.sector;
		session->hostile_deployed_fighters = deployed_remaining;
	}
	if (persistence.mercenaries_hurt)
		session->mercenaries_hurt = true;
	if (!child_result)
		return false;
	if (persistence.route == YT_HOSTILE_ATTACK_PERSISTENCE_FATAL)
		return true;
	tail = (struct yt_hostile_attack_tail_state){
		.current_player_record = current_player_record,
		.old_owner = old_owner,
		.defender_loss = defender_loss,
		.deployed_fighters = deployed_remaining,
		.ship_fighters = ship_fighters,
		.turns_per_day = session->door->game.config.turns_per_day,
		.headquarters = session->door->game.config.headquarters,
		.cached_player_name = cached_player_name,
		.cached_player_name_length = cached_player_name_length,
		.current = current,
	};
	child_result = hostile_attack_tail_run(session, &tail,
	    cached_player_name_text, error);
	return child_result;
}

static bool
bribe_name_row(const uint8_t *prefix, size_t prefix_length,
    const uint8_t *name, size_t name_length, const uint8_t *suffix,
    size_t suffix_length, uint8_t *row, size_t capacity, size_t *length)
{
	size_t used = 0U;

	if (!attack_append(row, capacity, &used, prefix, prefix_length)
	    || !attack_append(row, capacity, &used, name, name_length)
	    || !attack_append(row, capacity, &used, suffix, suffix_length))
		return false;
	*length = used;
	return true;
}

static bool
bribe_accept(struct yt_session *session, double cached_defenders,
    float offer, struct yt_error *error)
{
	static const uint8_t deal[] = "Good Deal! We join up with you!";
	struct yt_sector sector;
	struct yt_player current;
	volatile double fighters;
	volatile double credits;
	int current_sector = (int)session->player.sector;
	int player_record = session_record(session);

	if (!session_present_alert(session, deal, sizeof(deal) - 1U,
	    "accepted Mercenary Bribe", error)
	    || !session_sound(session, 1.0f, "accepted bribe sound", error)
	    || !session_read_sector(session, current_sector, &sector, error))
		return false;
	yt_bribe_sector_overlay(&sector);
	if (!yt_database_write(&session->door->game.database,
	    (size_t)yt_sector_basic_record(&session->door->game.config,
	    current_sector), &sector.record, error)
	    || !session_reload_player(session, error))
		return false;
	current = session->player;
	fighters = attack_double_add((double)current.fighters,
	    cached_defenders);
	credits = attack_double_sub((double)current.credits, (double)offer);
	yt_bribe_player_overlay(&current, (float)fighters, (float)credits);
	return yt_database_write(&session->door->game.database,
	    (size_t)player_record, &current.record, error);
}

static bool
bribe_force_attack(struct yt_session *session, struct yt_sector *sector,
    bool mercenary_fatal_gate, bool *direct_hostile_menu,
    bool *forced_attack, struct yt_error *error)
{
	enum qb_mbf_status conversion;
	float commitment = (float)session->combat_ship_fighters;
	uint8_t raw[4];

	*forced_attack = true;
	conversion = qb_mbf32_encode(commitment, raw);
	if (conversion == QB_MBF_OVERFLOW) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "bribe:commitment-csng");
		}
		return false;
	}
	commitment = qb_mbf32_decode(raw);
	switch (yt_bribe_forced_admit(session->combat_ship_fighters,
	    session->combat_ship_shields, mercenary_fatal_gate, commitment)) {
	case YT_BRIBE_FORCED_FATAL:
		return yt_session_common_fatal_self(session, error);
	case YT_BRIBE_FORCED_LESS_THAN_ONE:
		*direct_hostile_menu = true;
		return true;
	case YT_BRIBE_FORCED_ATTACK:
		return yt_session_attack_deployed(session, sector,
		    (double)commitment, true, error);
	}
	return false;
}

bool
yt_session_bribe_deployed(struct yt_session *session,
    struct yt_sector *sector, bool *direct_hostile_menu,
    bool *forced_attack, struct yt_error *error)
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
	double cached_defenders;
	double ship_fighters;
	double available_credits;
	double threshold;
	float draw;
	float second_draw;
	float offer;
	const uint8_t *name;
	size_t name_length;
	int credits_length;
	bool force_attack;

	if (session == NULL || sector == NULL || direct_hostile_menu == NULL
	    || forced_attack == NULL)
		return false;
	*direct_hostile_menu = false;
	*forced_attack = false;
	cached_defenders = session->hostile_deployed_fighters;
	ship_fighters = session->combat_ship_fighters;
	available_credits = (double)session->player.credits;
	name = (const uint8_t *)session->door->identity.real_first;
	name_length = strlen(session->door->identity.real_first);

	if (session->hostile_owner != -2.0f) {
		if (!bribe_name_row(ordinary_prefix,
		    sizeof(ordinary_prefix) - 1U, name, name_length, bang,
		    sizeof(bang) - 1U, row, sizeof(row), &row_length)
		    || !session_present_alert(session, row, row_length,
		    "ordinary Bribe refusal", error)
		    || !yt_random_next(&session->door->game.random, &draw, error))
			return false;
		force_attack = yt_bribe_ordinary_forces(session->hostile_owner,
		    cached_defenders, ship_fighters, draw);
		if (!force_attack)
			return true;
		return bribe_force_attack(session, sector, false,
		    direct_hostile_menu, forced_attack, error);
	}

	if (sector->planet != 0.0f) {
		return bribe_name_row(planet_prefix,
		    sizeof(planet_prefix) - 1U, name, name_length, planet_suffix,
		    sizeof(planet_suffix) - 1U, row, sizeof(row), &row_length)
		    && session_present_alert(session, row, row_length,
		    "Mercenary planet refusal", error);
	}

	if (!yt_random_next(&session->door->game.random, &draw, error)
	    || !yt_random_next(&session->door->game.random, &second_draw,
	    error))
		return false;
	force_attack = yt_bribe_mercenary_forces(cached_defenders,
	    ship_fighters, draw, second_draw, session->mercenaries_hurt);
	if (force_attack) {
		if (!bribe_name_row(life_prefix, sizeof(life_prefix) - 1U,
		    name, name_length, bang, sizeof(bang) - 1U, row,
		    sizeof(row), &row_length)
		    || !session_present_alert(session, row, row_length,
		    "Mercenary life demand", error))
			return false;
		return bribe_force_attack(session, sector, true,
		    direct_hostile_menu, forced_attack, error);
	}

	if (!bribe_name_row(introduction_prefix,
	    sizeof(introduction_prefix) - 1U, name, name_length, bang,
	    sizeof(bang) - 1U, row, sizeof(row), &row_length)
	    || !session_present_paged_line(session, row, row_length,
	    "Mercenary Bribe introduction", error))
		return false;
	credits_length = qb_str_double(credits, sizeof(credits),
	    available_credits);
	if (credits_length < 0
	    || !attack_append(prompt, sizeof(prompt), &prompt_length,
	    prompt_prefix, sizeof(prompt_prefix) - 1U)
	    || !attack_append(prompt, sizeof(prompt), &prompt_length,
	    (const uint8_t *)credits, (size_t)credits_length)
	    || !attack_append(prompt, sizeof(prompt), &prompt_length,
	    prompt_suffix, sizeof(prompt_suffix) - 1U)
	    || !session_present_timed_paged_row(session, prompt, prompt_length,
	    "Mercenary Bribe offer prompt", error)
	    || !session_read_number_command(session, response, sizeof(response)))
		return false;
	if (response[0] == '\0')
		return true;
	parsed = qb_val(response);
	if (!parsed.valid || parsed.overflow) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "bribe:VAL");
		}
		return false;
	}
	offer = (float)parsed.value;
	conversion = qb_mbf32_encode(offer, raw);
	if (conversion == QB_MBF_OVERFLOW) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "bribe:offer-csng");
		}
		return false;
	}
	offer = qb_mbf32_decode(raw);
	if (!yt_random_next(&session->door->game.random, &draw, error))
		return false;
	threshold = yt_bribe_offer_threshold(cached_defenders, draw);
	if ((double)offer <= available_credits
	    && yt_bribe_offer_accepted(offer, available_credits, threshold))
		return bribe_accept(session, cached_defenders, offer, error);

	if (!bribe_name_row(rejected_prefix, sizeof(rejected_prefix) - 1U,
	    name, name_length, rejected_suffix, sizeof(rejected_suffix) - 1U,
	    row, sizeof(row), &row_length)
	    || !session_present_alert(session, row, row_length,
	    "Mercenary rejected offer", error))
		return false;
	return bribe_force_attack(session, sector, true, direct_hostile_menu,
	    forced_attack, error);
}
