#include "yt_session_projectile_internal.h"

#include "qb.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static bool
missile_planet_impact(struct yt_session *session, int sector_number,
    struct yt_sector *sector, float *remaining, bool *early_return,
    struct yt_error *error)
{
	static const uint8_t destroyed[] = "The planet was destroyed!!";
	struct yt_planet planet;
	struct yt_planet updater_planet;
	int logical_planet;
	uint32_t physical_planet;
	uint32_t physical_sector;
	float original_ore;
	bool friendly = false;
	uint8_t planet_name[YT_TEXT_FIELD_SIZE];
	uint8_t attacker_name[YT_TEXT_FIELD_SIZE];
	uint8_t direct_row[256];
	uint8_t news_row[256];
	size_t planet_name_length;
	size_t attacker_name_length;
	size_t direct_length;
	size_t news_length;

	if (early_return == NULL)
		return false;
	*early_return = false;
	if (*remaining <= 0.0f) {
		*early_return = true;
		return true;
	}
	logical_planet = (int)sector->planet;
	if (logical_planet == 0)
		return true;
	physical_planet = session_planet_basic_record(session,
	    logical_planet);
	physical_sector = session_sector_basic_record(session,
	    sector_number);
	if (!yt_session_update_planet_physical(session, physical_planet,
	    &updater_planet, NULL, error))
		return false;
	/* DS:1A48 remains the updater's ore value across the independent GET. */
	original_ore = updater_planet.production[0];
	if (!read_planet_physical(session, physical_planet, &planet, error))
		return false;
	planet_name_length = yt_planet_stored_name(&planet, planet_name);
	if (planet.owner == (float)session_record(session))
		friendly = true;
	else if (planet.owner > 1.0f
	    && planet.owner <= session_sector_offset(session)) {
		if (!yt_session_players_are_friendly(session, (int)planet.owner,
		    &friendly, error))
			return false;
		if (!read_planet_physical(session, physical_planet, &planet,
		    error))
			return false;
	}
	if (friendly) {
		if (!yt_projectile_friendly_planet_row(planet_name,
		    planet_name_length, direct_row, sizeof(direct_row),
		    &direct_length))
			return false;
		return session_present_text(session, direct_row, direct_length,
		    SESSION_PRESENT_LINE,
		    "cruise missile friendly-planet row", error);
	}
	attacker_name_length = yt_player_stored_name(&session->player,
	    attacker_name);
	if (!yt_projectile_planet_attack_rows(false, attacker_name,
	    attacker_name_length, planet_name, planet_name_length,
	    (float)sector_number, direct_row, sizeof(direct_row),
	    &direct_length, news_row, sizeof(news_row), &news_length)
	    || !session_present_text(session, direct_row, direct_length,
	    SESSION_PRESENT_LINE, "cruise missile planet-attack row", error)
	    || !yt_news_append_bytes(news_row, news_length, error))
		return false;
	if (!session_sound(session, YT_SOUND_CUE_ATTACK,
	    "cruise missile planet attack sound", error))
		return false;
	if (planet.ground_forces != 0.0f) {
		struct yt_projectile_ground_result impact;
		struct yt_planet persistence;
		uint8_t row[256];
		size_t row_length;

		if (!yt_projectile_planet_ground_damage(planet.ground_forces,
		    planet.owner, remaining, &session->door->game.random, &impact,
		    error))
			return false;
		planet.ground_forces = impact.ground;
		planet.owner = impact.owner;
		if (!read_planet_physical(session, physical_planet, &persistence,
		    error)
		    || !yt_projectile_planet_ground_overlay(&persistence,
		    impact.ground, impact.owner)
		    || !session_write_planet_physical(session, physical_planet,
		    &persistence, false, error)
		    || !yt_projectile_planet_ground_row(impact.ground, row,
		    sizeof(row), &row_length)
		    || !session_present_text(session, row, row_length,
		    SESSION_PRESENT_LINE, "cruise missile planet impact row", error)
		    || !yt_news_append_bytes(row, row_length, error))
			return false;
		if (*remaining < 1.0f) {
			*early_return = true;
			return true;
		}
	}

	{
		struct yt_projectile_productivity_result impact;
		struct yt_planet persistence;
		uint8_t row[256];
		size_t row_length;

		if (!yt_projectile_planet_productivity_damage(original_ore,
		    planet.production, planet.stock, remaining,
		    &session->door->game.random, &impact, error)
		    || !yt_projectile_planet_productivity_row(impact.old_total,
		    impact.new_total, row, sizeof(row), &row_length)
		    || !session_present_text(session, row, row_length,
		    SESSION_PRESENT_LINE, "cruise missile planet impact row", error)
		    || !yt_news_append_bytes(row, row_length, error)
		    || !read_planet_physical(session, physical_planet, &persistence,
		    error)
		    || !yt_projectile_planet_productivity_overlay(&persistence,
		    planet.production, planet.stock)
		    || !session_write_planet_physical(session, physical_planet,
		    &persistence, false, error))
			return false;
	}

	if (planet.production[0] == 0.0f
	    && planet.production[1] == 0.0f
	    && planet.production[2] == 0.0f) {
		struct yt_planet destruction;
		struct yt_sector unlink;
		struct yt_record record;

		if (!read_planet_physical(session, physical_planet, &destruction,
		    error)
		    || !yt_projectile_planet_destroy_overlay(&destruction)
		    || !session_write_planet_physical(session, physical_planet,
		    &destruction, false, error)
		    || !yt_database_read(&session->door->game.database,
		    (size_t)physical_sector, &record, error))
			return false;
		yt_sector_decode(&unlink, &record);
		if (!yt_projectile_sector_unlink_overlay(&unlink)
		    || !yt_database_write(&session->door->game.database,
		    (size_t)physical_sector, &unlink.record, error)
		    || !session_present_text(session, destroyed,
		    sizeof(destroyed) - 1U, SESSION_PRESENT_LINE,
		    "cruise missile planet impact row", error)
		    || !session_sound(session, YT_SOUND_CUE_DESTRUCTION,
		    "cruise missile planet destruction sound", error)
		    || !yt_news_append_bytes(destroyed,
		    sizeof(destroyed) - 1U, error))
			return false;
	}
	if (*remaining < 1.0f)
		*early_return = true;
	return true;
}

static bool
deploy_victim_mines(struct yt_session *session, int sector_number,
    float mines, struct yt_error *error)
{
	struct yt_sector sector;

	if (!session_read_sector(session, sector_number, &sector,
	    error))
		return false;
	if (!yt_projectile_sector_mines_overlay(&sector, mines))
		return false;
	return yt_database_write(&session->door->game.database,
	    (size_t)session_sector_basic_record(session, sector_number),
	    &sector.record, error);
}

static bool
cruise_defense_damage_row(float destroyed, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t prefix[] = "The Missiles destroyed";
	static const uint8_t suffix[] = " fighters!";
	char number[64];
	int number_length;

	if (row == NULL || length == NULL)
		return false;
	number_length = qb_str_single(number, sizeof(number), destroyed);
	if (number_length < 0 || sizeof(prefix) - 1U + (size_t)number_length
	    + sizeof(suffix) - 1U > capacity)
		return false;
	memcpy(row, prefix, sizeof(prefix) - 1U);
	memcpy(row + sizeof(prefix) - 1U, number, (size_t)number_length);
	*length = sizeof(prefix) - 1U + (size_t)number_length;
	memcpy(row + *length, suffix, sizeof(suffix) - 1U);
	*length += sizeof(suffix) - 1U;
	return true;
}

static bool
cruise_defense_news_row(const uint8_t *shooter, size_t shooter_length,
    float destroyed, float sector, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t damage[] = "'s Missiles destroyed";
	static const uint8_t location[] = " fighters in sector";
	char destroyed_text[64];
	char sector_text[64];
	int destroyed_length;
	int sector_length;
	size_t row_length;

	if (row == NULL || length == NULL
	    || (shooter == NULL && shooter_length != 0U))
		return false;
	destroyed_length = qb_str_single(destroyed_text,
	    sizeof(destroyed_text), destroyed);
	sector_length = qb_str_single(sector_text, sizeof(sector_text), sector);
	if (destroyed_length < 0 || sector_length < 0
	    || shooter_length + sizeof(damage) - 1U
	    + (size_t)destroyed_length + sizeof(location) - 1U
	    + (size_t)sector_length + 1U > capacity)
		return false;
	if (shooter_length != 0U)
		memcpy(row, shooter, shooter_length);
	row_length = shooter_length;
	memcpy(row + row_length, damage, sizeof(damage) - 1U);
	row_length += sizeof(damage) - 1U;
	memcpy(row + row_length, destroyed_text, (size_t)destroyed_length);
	row_length += (size_t)destroyed_length;
	memcpy(row + row_length, location, sizeof(location) - 1U);
	row_length += sizeof(location) - 1U;
	memcpy(row + row_length, sector_text, (size_t)sector_length);
	row_length += (size_t)sector_length;
	row[row_length++] = '!';
	*length = row_length;
	return true;
}

bool
yt_session_missile_sector(struct yt_session *session, int sector_number,
    float *remaining, int *counterattack, int *xannor_provoker,
    float *last_mine_news_sector, enum yt_missile_sector_route *route,
    struct yt_error *error)
{
	struct yt_sector sector;
	int basic;

	if (route == NULL)
		return false;
	*route = MISSILE_SECTOR_RETURN;
	if (!session_read_sector(session, sector_number, &sector,
	    error))
		return false;
	if (!yt_projectile_sector_has_presence(&sector, sector_number,
	    (int)session_sector_offset(session), &session->player_cache,
	    *xannor_provoker)) {
		*route = MISSILE_SECTOR_POST_IMPACT;
		return true;
	}
	if (!((double)sector.fighters > 0.0))
		goto missile_mines;
	{
		static const uint8_t xannor[] = "The Xannor";
		static const uint8_t mercenaries[] = "Mercenaries";
		static const uint8_t you[] = "YOU";
		uint8_t owner_name[YT_TEXT_FIELD_SIZE];
		uint8_t row[256];
		const uint8_t *initial = xannor;
		size_t owner_length = sizeof(xannor) - 1U;
		size_t row_length;
		bool friendly = false;

		if (sector.fighter_owner == -2.0f) {
			initial = mercenaries;
			owner_length = sizeof(mercenaries) - 1U;
		}
		memcpy(owner_name, initial, owner_length);
		if (sector.fighter_owner > 1.0f) {
			struct yt_player defender;
			int owner_record = (int)sector.fighter_owner;

			if (!yt_game_read_player(&session->door->game,
			    owner_record, &defender, error))
				return false;
			owner_length = yt_player_stored_name(&defender, owner_name);
			if (!yt_session_players_are_friendly(session, owner_record,
			    &friendly, error))
				return false;
		}
		if (sector.fighter_owner == (float)session_record(session)) {
			memcpy(owner_name, you, sizeof(you) - 1U);
			owner_length = sizeof(you) - 1U;
			friendly = true;
		}
		if (!yt_projectile_defense_row((float)sector_number, owner_name,
		    owner_length, (double)sector.fighters, row, sizeof(row),
		    &row_length)
		    || !session_present_text(session, row, row_length,
		    SESSION_PRESENT_BOLD_LINE, "cruise missile defense report",
		    error))
			return false;
		if (friendly)
			goto missile_mines;
		yt_present_set_bold(&session->presentation, 1.0f);
		if (!session_sound(session, YT_SOUND_CUE_ATTACK,
		    "cruise missile fighter-defense sound", error))
			return false;
	}
	if (!(*remaining > 0.0f))
		goto missile_mines;
	{
		static const uint8_t dirty_zero[4] = {
			0x00, 0x00, 0x10, 0x00
		};
		struct yt_sector persistence;
		double original_fighters = (double)sector.fighters;
		double remaining_fighters;
		float owner = sector.fighter_owner;
		float saved_missiles = *remaining;
		float destroyed = 0.0f;
		float counter = 1.0f;
		uint8_t row[256];
		size_t row_length;

		while (counter <= saved_missiles
		    && (double)destroyed < original_fighters) {
			float draw;

			if (!yt_random_next(&session->door->game.random, &draw, error))
				return false;
			destroyed = floorf(qb_single_add(qb_single_multiply(draw, 5000.0f),
			    destroyed));
			*remaining = qb_single_subtract(*remaining, 1.0f);
			if ((double)destroyed >= original_fighters) {
				destroyed = (float)original_fighters;
				break;
			}
			counter = qb_single_add(counter, 1.0f);
		}
		if (!cruise_defense_damage_row(destroyed, row, sizeof(row),
		    &row_length)
		    || !session_present_text(session, row, row_length,
		    SESSION_PRESENT_LINE, "cruise missile destroyed-defense row",
		    error))
			return false;
		remaining_fighters = original_fighters - (double)destroyed;
		if (destroyed > 9.0f
		    && (!cruise_defense_news_row(
		    (const uint8_t *)session->player.name,
		    strlen(session->player.name), destroyed, (float)sector_number,
		    row, sizeof(row), &row_length)
		    || !yt_news_append_bytes(row, row_length, error)))
			return false;
		if (!session_read_sector(session, sector_number, &persistence,
		    error))
			return false;
		persistence.fighters = (float)remaining_fighters;
		if (!yt_record_set_number(&persistence.record, YT_F81,
		    persistence.fighters))
			return false;
		if (remaining_fighters == 0.0) {
			persistence.fighters = 0.0f;
			persistence.fighter_owner = 0.0f;
			if (!yt_record_set_raw_number(&persistence.record, YT_F81,
			    dirty_zero)
			    || !yt_record_set_raw_number(&persistence.record, YT_F85,
			    dirty_zero))
				return false;
		}
		else if (owner == -1.0f) {
			*xannor_provoker = session_record(session);
		}
		if (!yt_database_write(&session->door->game.database,
		    (size_t)session_sector_basic_record(session,
		    sector_number), &persistence.record, error))
			return false;
		if (remaining_fighters == 0.0
		    && (float)sector_number
		    == session->door->game.config.headquarters
		    && owner == -1.0f
		    && !yt_session_xannor_victory(session, error))
			return false;
		if (*remaining < 1.0f)
			return true;
	}

missile_mines:
	for (;;) {
		struct yt_sector mine_sector;
		double observed_mines;
		float destroyed;
		volatile float missiles_after;
		uint8_t row[256];
		size_t row_length;

		if (!session_read_sector(session, sector_number, &mine_sector,
		    error))
			return false;
		observed_mines = (double)mine_sector.mines;
		if (!(observed_mines > 0.0))
			break;
		if (!yt_projectile_sector_mine_hit_row(observed_mines,
		    (float)sector_number, row, sizeof(row), &row_length)
		    || !session_present_text(session, row, row_length,
		    SESSION_PRESENT_BOLD_LINE, "cruise missile sector-mine row",
		    error)
		    || !session_sound(session, YT_SOUND_CUE_DAMAGE,
		    "cruise missile sector-mine sound", error))
			return false;
		if (*last_mine_news_sector != (float)sector_number) {
			if (!yt_projectile_sector_mine_news_row(
			    (const uint8_t *)session->player.name,
			    strlen(session->player.name), (float)sector_number,
			    row, sizeof(row), &row_length)
			    || !yt_news_append_bytes(row, row_length,
			    error))
				return false;
			*last_mine_news_sector = (float)sector_number;
		}
		destroyed = (double)*remaining < observed_mines
		    ? *remaining : (float)observed_mines;
		if (!yt_projectile_sector_mine_destroyed_row(destroyed, row,
		    sizeof(row), &row_length)
		    || !session_present_text(session, row, row_length,
		    SESSION_PRESENT_BOLD_LINE, "cruise missile sector-mine row",
		    error)
		    || !session_read_sector(session, sector_number, &mine_sector,
		    error))
			return false;
		mine_sector.mines = (float)(observed_mines - (double)destroyed);
		if (!yt_record_set_number(&mine_sector.record, YT_F129,
		    mine_sector.mines)
		    || !yt_database_write(&session->door->game.database,
		    (size_t)session_sector_basic_record(session,
		    sector_number), &mine_sector.record, error))
			return false;
		missiles_after = *remaining - destroyed;
		*remaining = missiles_after;
		if (*remaining < 1.0f)
			return true;
	}
	for (basic = YT_PLAYER_FIRST_RECORD;
	    basic <= (int)session_sector_offset(session); ++basic) {
		struct yt_player target;
		struct yt_player presentation_target;
		struct yt_projectile_damage_result damage;
		bool scanner_disabled = false;
		uint8_t attacker_name[YT_TEXT_FIELD_SIZE];
		uint8_t victim_name[YT_TEXT_FIELD_SIZE];
		uint8_t first_news[256];
		uint8_t first_direct[256];
		size_t attacker_length;
		size_t victim_length;
		size_t first_news_length;
		size_t first_direct_length;
		char shield_text[64];
		char fighter_text[64];
		char row[256];

		enum yt_projectile_candidate_route candidate_route =
		    yt_projectile_candidate_route(basic, session_record(session),
		    yt_player_cache_value(&session->player_cache, basic,
		    YT_PLAYER_CACHE_SECTOR), (float)sector_number,
		    *remaining);

		if (candidate_route == YT_PROJECTILE_CANDIDATE_TERMINATE)
			break;
		if (candidate_route == YT_PROJECTILE_CANDIDATE_SKIP)
			continue;
		/* YT-SUB:974F is called for its exact GET effects; its result is ignored. */
		{
			bool ignored_friendship;

			if (!yt_session_players_are_friendly(session, basic,
			    &ignored_friendship, error))
				return false;
		}
		if (!yt_game_read_player(&session->door->game, basic, &target,
		    error))
			return false;
		if (!yt_projectile_candidate_admitted(basic,
		    yt_player_cache_value(&session->player_cache, basic,
		    YT_PLAYER_CACHE_CLOAK), *xannor_provoker))
			continue;
		if (!session_sound(session, YT_SOUND_CUE_ATTACK,
		    "cruise missile player-attack sound", error))
			return false;
		if (!yt_projectile_player_damage(&target, remaining,
		    &session->door->game.random, &damage, error))
			return false;
		scanner_disabled = damage.scanner_disabled;
		session_set_foreground(session, 5.0f);
		if (!yt_game_read_player(&session->door->game, basic,
		    &presentation_target, error))
			return false;
		qb_str_single(shield_text, sizeof(shield_text), target.shields);
		qb_str_double(fighter_text, sizeof(fighter_text),
		    damage.fighters);
		attacker_length = yt_player_stored_name(&session->player,
		    attacker_name);
		victim_length = yt_player_stored_name(&presentation_target,
		    victim_name);
		if (!yt_projectile_attack_first_rows(false,
		    attacker_name, attacker_length, victim_name, victim_length,
		    (float)sector_number, first_news, sizeof(first_news),
		    &first_news_length, first_direct, sizeof(first_direct),
		    &first_direct_length)
		    || !yt_news_append_bytes(first_news, first_news_length,
		    error)
		    || !session_present_text(session, first_direct,
		    first_direct_length, SESSION_PRESENT_BOLD_LINE,
		    "cruise missile player attack first row", error))
			return false;
		snprintf(row, sizeof(row), "shields to%s units and destroying%s "
		    "fighters!", shield_text, fighter_text);
		if (!yt_news_append(row, error))
			return false;
		if (!session_present_text(session, (const uint8_t *)row,
		    strlen(row), SESSION_PRESENT_BOLD_LINE,
		    "cruise missile player attack second row", error))
			return false;
		session_set_foreground(session, 0.0f);
		if (!yt_projectile_player_survives(target.shields)) {
			float mines;
			uint8_t killed_name[YT_TEXT_FIELD_SIZE];
			uint8_t destroyed_row[128];
			uint8_t warning_row[160];
			size_t killed_name_length;
			size_t destroyed_length;
			size_t warning_length;

			if (!yt_game_read_player(&session->door->game, basic,
			    &target, error))
				return false;
			killed_name_length = yt_player_stored_name(&target,
			    killed_name);
			if (!yt_projectile_destroyed_rows(killed_name,
			    killed_name_length, destroyed_row, sizeof(destroyed_row),
			    &destroyed_length, warning_row, sizeof(warning_row),
			    &warning_length))
				return false;
			if (!yt_projectile_victim_mines_overlay(&target, &mines)
			    || !yt_database_write(&session->door->game.database,
			    (size_t)basic, &target.record, error))
				return false;
			yt_present_set_blink(&session->presentation, 1.0f);
			if (!session_present_text(session, destroyed_row,
			    destroyed_length, SESSION_PRESENT_BOLD_LINE,
			    "cruise missile destroyed-player row", error))
				return false;
			if (mines != 0.0f) {
				yt_present_set_blink(&session->presentation, 1.0f);
				if (!session_present_text(session, warning_row,
				    warning_length,
				    SESSION_PRESENT_BOLD_LINE,
				    "cruise missile carried-mine warning", error))
					return false;
			}
			if (mines != 0.0f
			    && !deploy_victim_mines(session, sector_number,
			    mines, error))
				return false;
			if (!yt_session_kill_player(session, basic,
			    (float)session_record(session), true, error))
				return false;
			if (yt_projectile_salvage_admitted(*counterattack,
			    *xannor_provoker)) {
				if (!session_sound(session, YT_SOUND_CUE_DESTRUCTION,
				    "cruise missile salvage sound", error)
				    || !yt_session_salvage_player(session, basic,
				    session_record(session), error))
					return false;
			}
			switch (yt_projectile_death_continuation(*remaining,
			    mines)) {
			case YT_PROJECTILE_DEATH_REENTER_MINES:
				goto missile_mines;
			case YT_PROJECTILE_DEATH_RETURN:
				return true;
			case YT_PROJECTILE_DEATH_NEXT_PLAYER:
				break;
			}
		}
		else {
			struct yt_player persistence;

			if (!yt_game_read_player(&session->door->game, basic,
			    &persistence, error))
				return false;
			if (!yt_projectile_survivor_overlay(&persistence,
			    target.shields, (double)target.fighters,
			    target.danger_scanner, scanner_disabled))
				return false;
			if (!yt_database_write(&session->door->game.database,
			    (size_t)basic, &persistence.record, error))
				return false;
			if (yt_projectile_survivor_sets_counterattack(
			    session_record(session))) {
				*counterattack = basic;
				session->projectile.pending_counterattack_player = basic;
			}
			return true;
		}
	}
	if (!session_read_sector(session, sector_number, &sector,
	    error))
		return false;
	{
		bool early_return;

		if (!missile_planet_impact(session, sector_number, &sector,
		    remaining, &early_return, error))
			return false;
		if (!early_return)
			*route = MISSILE_SECTOR_POST_IMPACT;
		return true;
	}
}
