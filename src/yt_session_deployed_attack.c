#include "yt_session_internal.h"

#include "qb.h"

#include <stdio.h>
#include <string.h>

struct hostile_surrender {
	int current_player_record;
	int old_owner;
	double attacker_loss;
	double defender_loss;
	double deployed_fighters;
	const uint8_t *cached_player_name;
	size_t cached_player_name_length;
	const uint8_t *real_first_name;
	size_t real_first_name_length;
	struct yt_player current;
	double ship_fighters;
	double deployed_remaining;
	int fighter_owner;
	bool accepted;
};

enum hostile_persistence_route {
	HOSTILE_PERSISTENCE_NORMAL,
	HOSTILE_PERSISTENCE_FATAL,
};

struct hostile_persistence {
	int current_player_record;
	int current_sector;
	double ship_fighters;
	float shields;
	double deployed_fighters;
	double defender_loss;
	int old_owner;
	const uint8_t *cached_player_name;
	size_t cached_player_name_length;
	const uint8_t *owner_label;
	size_t owner_label_length;
	struct yt_player current;
	struct yt_sector sector;
	enum hostile_persistence_route route;
	bool sector_written;
	bool mercenaries_hurt;
};

struct hostile_tail {
	int current_player_record;
	int old_owner;
	double defender_loss;
	double deployed_fighters;
	double ship_fighters;
	float turns_per_day;
	float headquarters;
	const uint8_t *cached_player_name;
	size_t cached_player_name_length;
	struct yt_player current;
};

static bool
hostile_surrender_run(struct yt_session *session,
    struct hostile_surrender *state, struct yt_error *error)
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
	enum yt_yes_no_answer answer;
	enum yt_hostile_surrender_route owner_route;
	bool accepted = false;

	state->fighter_owner = state->old_owner;
	state->deployed_remaining = state->deployed_fighters;
	state->accepted = false;
	if (!session_read_combat_player(session, state->current_player_record,
	    &state->current, error))
		return false;
	state->ship_fighters = (double)state->current.fighters;
	owner_route = yt_hostile_surrender_route(state->old_owner);
	if (!session_present_paged_line(session, radio, sizeof(radio) - 1U,
	    "surrender radio row", error))
		return false;
	if (!session_sound(session, YT_SOUND_CUE_ACTION,
	    "hostile surrender sound", error))
		return false;
	if (qb_str_single(sector_number, sizeof(sector_number),
	    (float)state->current.sector) < 0)
		return false;
	sector_length = strlen(sector_number);
	position = 0U;
	if (!session_buffer_append(captain, sizeof(captain), &position,
	    captain_prefix, sizeof(captain_prefix) - 1U))
		return false;
	if (!session_buffer_append(captain, sizeof(captain), &position,
	    (const uint8_t *)sector_number, sector_length))
		return false;
	if (!session_present_paged_line(session, captain, position,
	    "surrender captain row", error))
		return false;

	switch (owner_route) {
	case YT_HOSTILE_SURRENDER_PLAYER:
		if (!session_present_alert(session, wish, sizeof(wish) - 1U,
		    "surrender wish row", error))
			return false;
		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "surrender prompt blank", error))
			return false;
		if (!session_confirm(session, prompt, sizeof(prompt) - 1U,
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
		    sizeof(xannor_refusal) - 1U))
			return false;
		if (!session_sound(session, YT_SOUND_CUE_DAMAGE,
		    "hostile surrender sound", error))
			return false;
		break;
	case YT_HOSTILE_SURRENDER_MERCENARY:
		position = 0U;
		if (!session_buffer_append(refusal, sizeof(refusal),
		    &position, mercenary_prefix, sizeof(mercenary_prefix) - 1U))
			return false;
		if (!session_buffer_append(refusal, sizeof(refusal),
		    &position, state->real_first_name,
		    state->real_first_name_length))
			return false;
		if (!session_buffer_append(refusal, sizeof(refusal),
		    &position, mercenary_suffix,
		    sizeof(mercenary_suffix) - 1U))
			return false;
		if (!session_present_paged_fragment(session, refusal, position))
			return false;
		if (!session_sound(session, YT_SOUND_CUE_DAMAGE,
		    "hostile surrender sound", error))
			return false;
		break;
	case YT_HOSTILE_SURRENDER_QUIET:
		break;
	}
	state->accepted = accepted;
	if (!accepted)
		return true;
	if (!session_present_paged_line(session, joined, sizeof(joined) - 1U,
	    "surrender joined row", error))
		return false;
	if (!session_sound(session, YT_SOUND_CUE_REWARD,
	    "hostile surrender sound", error))
		return false;
	surrendered_fighters = qb_double_subtract(state->deployed_fighters,
	    state->defender_loss);
	if (qb_str_double(surrendered_number, sizeof(surrendered_number),
	    surrendered_fighters) < 0)
		return false;
	surrendered_length = strlen(surrendered_number);
	position = 0U;
	if (!session_buffer_append(news, sizeof(news), &position,
	    (const uint8_t *)surrendered_number, surrendered_length))
		return false;
	if (!session_buffer_append(news, sizeof(news), &position,
	    news_middle_one, sizeof(news_middle_one) - 1U))
		return false;
	if (!session_buffer_append(news, sizeof(news), &position,
	    (const uint8_t *)sector_number, sector_length))
		return false;
	if (!session_buffer_append(news, sizeof(news), &position,
	    news_middle_two, sizeof(news_middle_two) - 1U))
		return false;
	if (!session_buffer_append(news, sizeof(news), &position,
	    state->cached_player_name, state->cached_player_name_length))
		return false;
	if (!yt_news_append_bytes(news, position, error))
		return false;
	state->ship_fighters = qb_double_add(qb_double_subtract(qb_double_subtract(
	    (double)state->current.fighters, state->attacker_loss),
	    state->defender_loss), state->deployed_fighters);
	state->current.fighters = (float)state->ship_fighters;
	state->deployed_remaining = 0.0;
	state->fighter_owner = 0;
	session->combat.ship_fighters = state->ship_fighters;
	session->combat.deployed_fighters = state->deployed_remaining;
	position = 0U;
	if (!session_buffer_append(count, sizeof(count), &position,
	    (const uint8_t *)surrendered_number, surrendered_length))
		return false;
	if (!session_buffer_append(count, sizeof(count), &position,
	    count_suffix, sizeof(count_suffix) - 1U))
		return false;
	if (!session_present_paged_fragment(session, count, position))
		return false;
	return true;
}

static bool
hostile_attack_persistence_run(struct yt_session *session,
    struct hostile_persistence *state,
    struct yt_error *error)
{
	static const uint8_t destroyed[] = " destroyed";
	static const uint8_t belonging[] = " fighters belonging to ";
	uint8_t news[320];
	char loss_number[64];
	size_t position = 0U;
	int loss_length;

	state->route = HOSTILE_PERSISTENCE_NORMAL;
	state->sector_written = false;
	state->mercenaries_hurt = false;
	if (!session_read_combat_player(session, state->current_player_record,
	    &state->current, error))
		return false;
	yt_deployed_attack_player_overlay(&state->current, state->shields,
	    (float)state->ship_fighters);
	if (!session_write_combat_player(session, state->current_player_record,
	    &state->current, error))
		return false;
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
		state->route = HOSTILE_PERSISTENCE_FATAL;
		if (!yt_session_common_fatal_self(session, error))
			return false;
		return true;
	}
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "deployed attack post-persist blank", error))
		return false;
	if (state->defender_loss > 0.0) {
		if (!session_read_combat_player(session,
		    state->current_player_record, &state->current, error))
			return false;
		state->ship_fighters = (double)state->current.fighters;
		loss_length = qb_str_double(loss_number, sizeof(loss_number),
		    state->defender_loss);
		if (loss_length < 0)
			return false;
		if (!session_buffer_append(news, sizeof(news), &position,
		    state->cached_player_name, state->cached_player_name_length))
			return false;
		if (!session_buffer_append(news, sizeof(news), &position,
		    destroyed, sizeof(destroyed) - 1U))
			return false;
		if (!session_buffer_append(news, sizeof(news), &position,
		    (const uint8_t *)loss_number, (size_t)loss_length))
			return false;
		if (!session_buffer_append(news, sizeof(news), &position,
		    belonging, sizeof(belonging) - 1U))
			return false;
		if (!session_buffer_append(news, sizeof(news), &position,
		    state->owner_label, state->owner_label_length))
			return false;
		if (!yt_news_append_bytes(news, position, error))
			return false;
		state->mercenaries_hurt = state->old_owner == -2;
	}
	return true;
}

static bool
hostile_attack_tail_run(struct yt_session *session,
    struct hostile_tail *state,
    const char *cached_player_name, struct yt_error *error)
{
	uint8_t display[240];
	uint8_t news[300];
	uint8_t defeated[160];
	size_t display_length;
	size_t news_length;
	size_t defeated_length;
	float bonus;
	float dominated_draw;

	if (state->old_owner == -1 && state->defender_loss > 0.0) {
		if (!session_read_combat_player(session,
		    state->current_player_record, &state->current, error))
			return false;
		(void)snprintf(session->player.name,
		    sizeof(session->player.name), "%s", cached_player_name);
		(void)snprintf(state->current.name,
		    sizeof(state->current.name), "%s", cached_player_name);
		state->ship_fighters = (double)state->current.fighters;
		bonus = yt_xannor_attack_bonus(state->defender_loss,
		    state->current.turns, state->turns_per_day);
		if (bonus >= 1.0f) {
			state->current.turns = qb_single_add(state->current.turns,
			    bonus);
			(void)yt_record_set_number(&state->current.record, YT_F49,
			    state->current.turns);
			if (!session_write_combat_player(session,
			    state->current_player_record, &state->current, error))
				return false;
			if (!yt_xannor_attack_reward_rows(
			    state->cached_player_name,
			    state->cached_player_name_length, bonus,
			    state->defender_loss, display, sizeof(display),
			    &display_length, news, sizeof(news), &news_length))
				return false;
			session->presentation.bold = true;
			if (!session_present_paged_fragment(session, display,
			    display_length))
				return false;
			if (!yt_news_append_bytes(news, news_length,
			    error))
				return false;
			if (state->deployed_fighters < 1.0) {
				if (!yt_session_clearance(session, true, error))
					return false;
			}
		}
	}
	if (!yt_random_next(&session->door->game.random,
	    &dominated_draw, error))
		return false;
	if (state->deployed_fighters <= 0.0) {
		if (!yt_hostile_defeated_row(state->ship_fighters, defeated,
		    sizeof(defeated), &defeated_length))
			return false;
		if (!session_present_paged_fragment(session, defeated,
		    defeated_length))
			return false;
		if (state->old_owner == -1
		    && (float)state->current.sector == state->headquarters) {
			if (!yt_session_xannor_victory(session, error))
				return false;
		}
	}
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
	struct hostile_surrender surrender;
	struct hostile_persistence persistence;
	struct hostile_tail tail;
	size_t cached_player_name_length;
	size_t lost_length = 0U;
	size_t destroyed_length = 0U;
	double old_count = session->combat.deployed_fighters;
	double old_ship;
	double attacker_loss = 0.0;
	double defender_loss = 0.0;
	double ship_fighters;
	double deployed_remaining = old_count;
	int old_owner;
	float quantum;
	float last_draw;
	int current_player_record = session_record(session);
	int current_sector = session->player.sector;
	int attacker_length;
	int defender_length;
	bool surrender_checked = false;
	bool surrendered = false;
	bool child_result;

	cached_player_name_length = yt_player_stored_name(&session->player,
	    cached_player_name);
	(void)snprintf(cached_player_name_text,
	    sizeof(cached_player_name_text), "%s", session->player.name);
	if (!session_read_sector(session, current_sector, &opened_sector, error))
		return false;
	old_owner = opened_sector.fighter_owner;
	if (!session_read_combat_player(session, current_player_record,
	    &current, error))
		return false;
	(void)snprintf(session->player.name, sizeof(session->player.name),
	    "%s", cached_player_name_text);
	(void)snprintf(current.name, sizeof(current.name), "%s",
	    cached_player_name_text);
	current.fighters = (float)session->combat.ship_fighters;
	current.cloak = session->player.cloak;
	current.shields = session->combat.ship_shields;
	old_ship = (double)current.fighters;
	if (!session_sound(session, YT_SOUND_CUE_ATTACK, "deployed attack opening sound",
	    error))
		return false;

	do {
		double remaining_attacker = qb_double_subtract(commitment,
		    attacker_loss);
		double remaining_defender = qb_double_subtract(old_count,
		    defender_loss);
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
		if (!surrender_checked && allow_surrender
		    && remaining_attacker / remaining_defender > 10.0) {
			surrender = (struct hostile_surrender){
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
			surrendered = surrender.accepted;
			session->player = current;
			(void)snprintf(session->player.name,
			    sizeof(session->player.name), "%s",
			    cached_player_name_text);
			if (!child_result)
				return false;
			surrender_checked = true;
			if (surrendered) {
				ship_fighters = surrender.ship_fighters;
				deployed_remaining = surrender.deployed_remaining;
				sector->fighter_owner = surrender.fighter_owner;
				current.fighters = (float)ship_fighters;
				session->player = current;
				(void)snprintf(session->player.name,
				    sizeof(session->player.name), "%s",
				    cached_player_name_text);
				session->combat.deployed_fighters =
				    deployed_remaining;
				break;
			}
		}
		if (!yt_random_next(&session->door->game.random, &last_draw, error))
			return false;
		if (yt_hostile_attack_loses_attacker(current.cloak, last_draw))
			attacker_loss = qb_double_add(attacker_loss, (double)quantum);
		else
			defender_loss = qb_double_add(defender_loss, (double)quantum);
	} while (attacker_loss < commitment && defender_loss < old_count);
	if (attacker_loss > commitment)
		attacker_loss = commitment;
	if (defender_loss > old_count)
		defender_loss = old_count;
	if (!surrendered) {
		ship_fighters = qb_double_subtract(old_ship, attacker_loss);
		deployed_remaining = qb_double_subtract(old_count, defender_loss);
		current.fighters = (float)ship_fighters;
		session->combat.ship_fighters = ship_fighters;
		session->player = current;
		(void)snprintf(session->player.name,
		    sizeof(session->player.name), "%s",
		    cached_player_name_text);
	}
	attacker_length = qb_str_double(attacker_number,
	    sizeof(attacker_number), attacker_loss);
	defender_length = qb_str_double(defender_number,
	    sizeof(defender_number), defender_loss);
	if (attacker_length < 0 || defender_length < 0)
		return false;
	if (!session_buffer_append(lost_row, sizeof(lost_row),
	    &lost_length, lost_prefix, sizeof(lost_prefix) - 1U))
		return false;
	if (!session_buffer_append(lost_row, sizeof(lost_row),
	    &lost_length, (const uint8_t *)attacker_number,
	    (size_t)attacker_length))
		return false;
	if (!session_buffer_append(lost_row, sizeof(lost_row),
	    &lost_length, lost_suffix, sizeof(lost_suffix) - 1U))
		return false;
	if (!session_buffer_append(destroyed_row,
	    sizeof(destroyed_row), &destroyed_length, destroyed_prefix,
	    sizeof(destroyed_prefix) - 1U))
		return false;
	if (!session_buffer_append(destroyed_row,
	    sizeof(destroyed_row), &destroyed_length,
	    (const uint8_t *)defender_number, (size_t)defender_length))
		return false;
	if (!session_buffer_append(destroyed_row,
	    sizeof(destroyed_row), &destroyed_length, destroyed_suffix,
	    sizeof(destroyed_suffix) - 1U))
		return false;
	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "deployed attack result blank", error))
		return false;
	if (!session_present_paged_fragment(session, lost_row, lost_length))
		return false;
	if (!session_present_paged_fragment(session, destroyed_row,
	    destroyed_length))
		return false;
	if (ship_fighters < 1.0 && deployed_remaining > 0.0) {
		if (!session_present_alert(session, exposed, sizeof(exposed) - 1U,
		    "deployed attack ship exposed", error))
			return false;
		if (!session_present_text(session, NULL, 0U,
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
	session->combat.deployed_fighters = deployed_remaining;
	persistence = (struct hostile_persistence){
		.current_player_record = current_player_record,
		.current_sector = current_sector,
		.ship_fighters = ship_fighters,
		.shields = current.shields,
		.deployed_fighters = deployed_remaining,
		.defender_loss = defender_loss,
		.old_owner = old_owner,
		.cached_player_name = cached_player_name,
		.cached_player_name_length = cached_player_name_length,
		.owner_label = session->combat.hostile_owner_label,
		.owner_label_length = session->combat.hostile_owner_label_length,
		.current = current,
		.sector = *sector,
	};
	child_result = hostile_attack_persistence_run(session, &persistence,
	    error);
	current = persistence.current;
	ship_fighters = persistence.ship_fighters;
	if (persistence.route != HOSTILE_PERSISTENCE_FATAL) {
		session->player = persistence.current;
		(void)snprintf(session->player.name,
		    sizeof(session->player.name), "%s",
		    cached_player_name_text);
	}
	if (persistence.sector_written) {
		*sector = persistence.sector;
		session->combat.deployed_fighters = deployed_remaining;
	}
	if (persistence.mercenaries_hurt)
		session->combat.mercenaries_hurt = true;
	if (!child_result)
		return false;
	if (persistence.route == HOSTILE_PERSISTENCE_FATAL)
		return true;
	tail = (struct hostile_tail){
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
