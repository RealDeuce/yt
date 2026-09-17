#include "yt_session_internal.h"

#include "qb.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

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
				session->combat.deployed_fighters = *fighters;
			else
				session->combat.ship_shields = *shields;
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
	int current_player_record, int current_sector, float target_shields,
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
	if (!session_sound(session, YT_SOUND_CUE_DESTRUCTION, "player kill sound", error)
	    || !yt_game_read_player(&session->door->game, target_record,
	    &target, error))
		return false;
	saved_mines = target.mines;
	saved_name_length = target.name_length;
	if (saved_name_length != 0U)
		memcpy(saved_name, target.record.bytes, saved_name_length);
	if (!yt_session_kill_player(session, target_record,
	    (float)current_player_record, true, error)
	    || !yt_session_salvage_player(session, target_record,
	    current_player_record, error))
		return false;
	if (!(saved_mines > 0.0f))
		return true;
	if (!session_read_sector(session, current_sector, &sector, error))
		return false;
	deployed = sector.mines + saved_mines;
	yt_sector_mine_sector_overlay(&sector, deployed);
	if (!yt_database_write(&session->door->game.database,
	    (size_t)yt_sector_basic_record(&session->door->game.config,
	    current_sector),
	    &sector.record, error)
	    || !yt_direct_fighter_mine_warning(saved_name, saved_name_length,
	    warning, sizeof(warning), &warning_length)
	    || !session_present_alert(session, warning, warning_length,
	    "direct fighter mine warning", error)
	    || !yt_news_append_bytes(warning, warning_length,
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
		if (qb_single_add(qb_single_divide(cloak, 10.0f), sampled)
		    < 0.44999998807907104f)
			*attacker_loss = qb_double_add(*attacker_loss,
			    (double)quantum);
		else
			*defender_loss = qb_double_add(*defender_loss,
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
	int current_sector;
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

	cached_reserve = qb_double_subtract((double)current.fighters, committed);
	yt_direct_attack_fighter_overlay(&current, (float)cached_reserve);
	if (!session_write_combat_player(session, current_player_record,
	    &current, error)
	    || !session_sound(session, YT_SOUND_CUE_ATTACK, "player attack opening sound",
	    error)
	    || !direct_attack_attrition(session, committed, defenders,
	    current.cloak, &attacker_loss, &defender_loss, error))
		return false;
	if (defender_loss > 0.0) {
		name_length = yt_player_stored_name(&current, stored_name);
		if (!yt_direct_attack_radio_text(stored_name, name_length,
		    defender_loss, radio, sizeof(radio), &radio_length)
		    || !session_append_radio_bytes(radio, radio_length, -2.0f,
		    (float)target_record, error))
			return false;
	}

	if (!session_read_combat_player(session, current_player_record,
	    &current, error))
		return false;
	cached_reserve = (double)current.fighters;
	attacking = qb_double_subtract(committed, attacker_loss);
	defenders = qb_double_subtract(defenders, defender_loss);
	yt_direct_attack_fighter_overlay(&current,
	    (float)qb_double_add(cached_reserve, attacking));
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
	    (float)qb_double_add(cached_reserve, attacking));
	if (!session_write_combat_player(session, current_player_record,
	    &current, error))
		return false;
	if (target_shields > 0.0f)
		return true;
	return direct_attack_finish_kill(session, target_record,
	    current_player_record, current_sector, target_shields, error);
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
	int candidate = 2;
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
		int cached_sector;
		float cached_cloak;
		bool sector_mismatch;
		bool self;
		bool cloaked;
		bool positive_team;
		bool same_team;
		int record = candidate;

		cached_sector = yt_player_cache_sector(&session->player_cache,
		    record);
		cached_cloak = yt_player_cache_cloak(&session->player_cache,
		    record);
		sector_mismatch = cached_sector != (int)current.sector;
		self = record == session_record(session);
		cloaked = cached_cloak > 0.0f;
		if (sector_mismatch || self || cloaked) {
			++candidate;
			continue;
		}

		session->player_reference.record = candidate;
		if (!session_read_combat_player(session, record,
		    &candidate_player, error))
			return false;
		target_name_length = yt_player_stored_name(&candidate_player,
		    target_name);
		positive_team = candidate_player.team > 0.0f;
		same_team = candidate_player.team == current.team;
		if (positive_team && same_team) {
			if (!yt_direct_attack_team_row(target_name,
			    target_name_length, row, sizeof(row), &row_length)
			    || !session_present_paged_fragment(session, row,
			    row_length))
				return false;
			encountered = true;
			++candidate;
			continue;
		}

		encountered = true;
		if (!yt_direct_attack_candidate_prompt(target_name,
		    target_name_length, row, sizeof(row), &row_length)
		    || !session_confirm(session, row, row_length, &answer, error))
			return false;
		if (answer == YT_YES_NO_NO) {
			++candidate;
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
		if ((parsed.valid ? parsed.value : 0.0) < 1.0)
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
