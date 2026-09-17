#include "yt_session_internal.h"

#include "qb.h"
#include "yt_platform.h"

#include <math.h>
#include <string.h>

bool
yt_session_planet_assault(struct yt_session *session,
    uint32_t physical_planet,
    float commitment, bool *defeated, struct yt_error *error)
{
	static const uint8_t engaging[] = "Forces engaging!";
	static const uint8_t defenses[] = "Planetary defenses destroyed!";
	static const uint8_t defenses_news[] =
	    " +++ Planetary defenses destroyed!";
	static const uint8_t captured[] = "You've captured the planet!";
	struct yt_planet planet;
	uint8_t player_name[YT_TEXT_FIELD_SIZE];
	uint8_t planet_name[YT_TEXT_FIELD_SIZE];
	uint8_t row[320];
	size_t player_name_length;
	size_t planet_name_length;
	size_t row_length;
	float attackers = commitment;
	float defenders;
	float saved_foreground;

	if (defeated == NULL)
		return false;
	*defeated = false;
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "planet assault entry blank", error))
		return false;
	saved_foreground = session->presentation.foreground;
	if (!yt_session_update_planet_physical(session, physical_planet, &planet,
	    NULL, error)
	    || !read_planet_physical(session, physical_planet, &planet, error))
		return false;
	planet_name_length = yt_planet_stored_name(&planet, planet_name);
	if (!session_reload_player(session, error))
		return false;
	defenders = floorf(planet.ground_forces);
	player_name_length = yt_player_stored_name(&session->player,
	    player_name);
	yt_planet_assault_player_overlay(&session->player, commitment);
	if (!yt_database_write(&session->door->game.database,
	    (size_t)session_record(session), &session->player.record, error)
	    || !yt_database_flush(&session->door->game.database, error)
	    || !yt_planet_assault_attack_news(player_name, player_name_length,
	    planet_name, planet_name_length, commitment, row, sizeof(row),
	    &row_length)
	    || !yt_news_append_bytes(row, row_length, error))
		return false;
	yt_present_set_blink(&session->presentation, 1.0f);
	if (!session_present_text(session, engaging, sizeof(engaging) - 1U,
	    SESSION_PRESENT_BOLD_LINE, "planet assault engagement row", error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "planet assault engagement blank", error)
	    || !session_sound(session, YT_SOUND_CUE_ATTACK,
	    "planet assault engagement sound", error))
		return false;
	while (attackers > 0.0f && defenders > 0.0f) {
		float side;
		float amount;
		bool attacker_damage;

		if (!yt_random_next(&session->door->game.random, &side, error))
			return false;
		attacker_damage = side > 0.4000000059604645f;
		if (!yt_random_next(&session->door->game.random, &amount, error))
			return false;
		yt_planet_assault_round(attacker_damage, amount, &attackers,
		    &defenders);
		session_set_foreground(session, attacker_damage ? 3.0f : 4.0f);
		if (!yt_planet_assault_status_row(attacker_damage,
		    attacker_damage ? attackers : defenders, row, sizeof(row),
		    &row_length)
		    || !session_present_text(session, row, row_length,
		    SESSION_PRESENT_LINE, "planet assault force-status row", error))
			return false;
		if (!attacker_damage
		    && !session_sound(session, YT_SOUND_CUE_ATTACK,
			    "planet assault defender sound", error))
			return false;
	}
	session_set_foreground(session, saved_foreground);
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "planet assault terminal blank", error))
		return false;
	if (defenders <= 0.0f) {
		float owner = 0.0f;

		if (!session_present_text(session, defenses,
		    sizeof(defenses) - 1U, SESSION_PRESENT_BOLD_LINE,
		    "planet assault defenses-destroyed row", error)
		    || !yt_news_append_bytes(defenses_news,
		    sizeof(defenses_news) - 1U, error)
		    || !session_sound(session, YT_SOUND_CUE_REWARD,
		    "planet defenses destroyed sound", error))
			return false;
		if (attackers > 0.0f) {
			owner = (float)session_record(session);
			if (!session_present_text(session, NULL, 0,
			    SESSION_PRESENT_LINE, "planet assault capture blank", error))
				return false;
			yt_present_set_blink(&session->presentation, 1.0f);
			if (!session_present_text(session, captured,
			    sizeof(captured) - 1U, SESSION_PRESENT_BOLD_LINE,
			    "planet assault capture row", error)
			    || !yt_planet_assault_capture_news(player_name,
			    player_name_length, planet_name, planet_name_length,
			    row, sizeof(row), &row_length)
			    || !yt_news_append_bytes(row, row_length, error)
			    || !session_sound(session, YT_SOUND_CUE_REWARD,
			    "planet capture sound", error))
				return false;
		}
		else
			attackers = 0.0f;
		if (!read_planet_physical(session, physical_planet, &planet,
		    error))
			return false;
		yt_planet_assault_victory_overlay(&planet, owner, attackers);
		return session_write_planet_physical(session, physical_planet,
		    &planet, false, error);
	}
	if (!read_planet_physical(session, physical_planet, &planet, error))
		return false;
	yt_planet_assault_failure_overlay(&planet, defenders);
	if (!session_write_planet_physical(session, physical_planet, &planet,
	    false, error))
		return false;
	yt_present_set_blink(&session->presentation, 1.0f);
	if (!yt_planet_assault_failure_row(defenders, true, row, sizeof(row),
	    &row_length)
	    || !yt_news_append_bytes(row, row_length, error)
	    || !yt_planet_assault_failure_row(defenders, false, row,
	    sizeof(row), &row_length)
	    || !session_present_text(session, row, row_length,
	    SESSION_PRESENT_BOLD_LINE, "planet assault failure row", error))
		return false;
	*defeated = true;
	return true;
}

static bool
create_planet(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t no_planet[] =
	    "There is no planet in this sector.";
	static const uint8_t price[] = "Planets cost 25000 credits.";
	static const uint8_t too_poor[] = "You're too poor to buy one.";
	static const uint8_t buy_prompt[] =
	    "Do you wish to buy a planet(Y/N) [N]? ";
	static const uint8_t all_taken[] =
	    "I'm sorry, but all planets are taken.";
	static const uint8_t destroy_first[] =
	    "One has to be destroyed before you can buy a planet.";
	static const uint8_t advice[] =
	    "To increase productivity on your new planet, spend credits [$] on it.";
	struct yt_sector sector;
	struct yt_planet planet;
	struct yt_record raw;
	uint8_t cached_trader[YT_TEXT_FIELD_SIZE];
	uint8_t row[320];
	size_t cached_trader_length;
	size_t row_length;
	uint32_t selected_physical;
	uint32_t sector_physical;
	int selected_logical;
	uint32_t scan;
	float minute;
	int logical;
	int today;
	int adjusted_year;
	bool renamed;
	enum yt_yes_no_answer answer;

	if (!session_present_paged_line(session, no_planet, sizeof(no_planet) - 1U,
	    "planet creation opening", error)
	    || !session_present_paged_fragment(session, price, sizeof(price) - 1U))
		return false;
	cached_trader_length = yt_player_stored_name(&session->player,
	    cached_trader);
	if (!session_reload_player(session, error)
	    || !yt_planet_creation_credit_row((double)session->player.credits,
	    row, sizeof(row), &row_length)
	    || !session_present_paged_fragment(session, row, row_length))
		return false;
	if (25000.0f > session->player.credits)
		return session_present_alert(session, too_poor,
		    sizeof(too_poor) - 1U,
		    "planet creation insufficient credits", error);
	if (!session_confirm(session, buy_prompt, sizeof(buy_prompt) - 1U,
	    &answer, error))
		return false;
	if (answer != YT_YES_NO_YES)
		return true;
	scan = session_planet_basic_record(session, 2);
	for (;;) {
		if (!yt_database_read(&session->door->game.database,
		    (size_t)scan, &raw, error))
			return false;
		yt_planet_decode(&planet, &raw);
		if (planet.name_length == 0U) {
			selected_physical = scan;
			break;
		}
		if (scan >= (uint32_t)session->door->game.config.total_records) {
			if (!session_present_alert(session, all_taken,
			    sizeof(all_taken) - 1U,
			    "planet creation allocation full", error)
			    || !session_present_paged_fragment(session, destroy_first,
			    sizeof(destroy_first) - 1U))
				return false;
			return true;
		}
		++scan;
	}
	selected_logical = (int)selected_physical
	    - session_planet_offset(session);
	logical = selected_logical;
	if (!yt_session_planet_rename(session, logical, &renamed, error))
		return false;
	if (!renamed)
		return true;
	if (!read_planet_physical(session, selected_physical, &planet, error))
		return false;
	yt_planet_creation_overlay(&planet, session_record(session));
	if (!session_write_planet_physical(session, selected_physical, &planet,
	    false, error))
		return false;
	sector_physical = session_sector_basic_record(session,
	    session->player.sector);
	if (!yt_database_read(&session->door->game.database,
	    (size_t)sector_physical, &raw, error))
		return false;
	yt_sector_decode(&sector, &raw);
	sector.planet = (float)selected_logical;
	(void)yt_record_set_number(&sector.record, YT_F93,
	    (float)selected_logical);
	if (!yt_database_write(&session->door->game.database,
	    (size_t)sector_physical, &sector.record, error)
	    || !session_current_date_serial(session, &today, &adjusted_year,
	    error))
		return false;
	session->door->game.today = today;
	session->door->game.adjusted_year = adjusted_year;
	minute = floorf(qb_single_divide((float)yt_clock_timer(
	    &session->door->game.clock), 60.0f));
	if (!read_planet_physical(session, selected_physical, &planet, error))
		return false;
	yt_planet_creation_timestamp_overlay(&planet, (float)today, minute);
	if (!session_write_planet_physical(session, selected_physical, &planet,
	    false, error))
		return false;
	if (!session_mutate_player_credits(session, -25000.0f, NULL, error)
	    || !yt_planet_creation_news(cached_trader, cached_trader_length,
	    (const uint8_t *)session->planet.name, strlen(session->planet.name),
	    row, sizeof(row), &row_length)
	    || !yt_news_append_bytes(row, row_length, error)
	    || !yt_planet_creation_success_row(
	    (const uint8_t *)session->planet.name, strlen(session->planet.name),
	    row, sizeof(row), &row_length)
	    || !session_present_paged_line(session, row, row_length,
	    "planet creation success row", error)
	    || !session_sound(session, YT_SOUND_CUE_ACTION, "planet creation sound", error)
	    || !session_present_paged_line(session, advice, sizeof(advice) - 1U,
	    "planet creation advice row", error))
		return false;
	return true;
}

bool
yt_session_command_land(struct yt_session *session, bool *enter_sector,
    struct yt_error *error)
{
	static const uint8_t title[] = "<Land/Create planet>";
	static const uint8_t landing[] = "Landing...";
	static const uint8_t confirmation[] =
	    "Do you wish to try to force a landing? [y/N] ";
	struct yt_sector sector;
	struct yt_planet planet;
	uint8_t row[256];
	uint8_t prompt[160];
	size_t row_length;
	size_t prompt_length;
	uint32_t physical;
	float cached_carried;
	int logical;
	bool permission_denied;

	if (!session_present_paged_line(session, title, sizeof(title) - 1U,
	    "planet landing title", error)
	    || !session_reload_player(session, error))
		return false;
	cached_carried = session->player.ground_forces;
	if (!session_read_sector(session,
	    session->player.sector, &sector, error))
		return false;
	session->planet.fallback_index = (int)sector.planet;
	if (sector.planet == 0.0f) {
		bool created = create_planet(session, error);

		if (created && enter_sector != NULL)
			*enter_sector = true;
		return created;
	}
	session_set_foreground(session, 6.0f);
	if (!session_present_paged_line(session, landing, sizeof(landing) - 1U,
	    "planet landing progress", error))
		return false;
	session->planet.current_record = session_planet_basic_record(session,
	    (int)sector.planet);
	logical = (int)sector.planet;
	physical = session_planet_basic_record(session, logical);
	if (!yt_session_planet_permission(session, logical, &permission_denied,
	    error))
		return false;
	if (permission_denied) {
		enum yt_yes_no_answer answer;
		char response[YT_COMMAND_SIZE];
		float commitment;
		bool defeated;

		if (!read_planet_physical(session, physical, &planet, error)
		    || !yt_planet_landing_sensor_row(planet.ground_forces,
		    cached_carried, row, sizeof(row), &row_length)
		    || !session_present_paged_line(session, row, row_length,
		    "planet landing sensor row", error))
			return false;
		if (cached_carried < 1.0f) {
			if (enter_sector != NULL)
				*enter_sector = true;
			return true;
		}
		if (!session_confirm(session, confirmation,
		    sizeof(confirmation) - 1U, &answer, error))
			return false;
		if (answer != YT_YES_NO_YES) {
			if (enter_sector != NULL)
				*enter_sector = true;
			return true;
		}
		if (!yt_planet_landing_amount_prompt(cached_carried, prompt,
		    sizeof(prompt), &prompt_length)
		    || !session_present_timed_paged_row(session, prompt, prompt_length,
		    "planet landing commitment prompt", error)
		    || !session_read_number_command(session, response, sizeof(response)))
			return false;
		commitment = yt_planet_landing_commitment(response);
		if (!yt_planet_landing_commitment_valid(commitment,
		    cached_carried)) {
			if (enter_sector != NULL)
				*enter_sector = true;
			return true;
		}
		if (!yt_session_planet_assault(session, physical, commitment,
		    &defeated, error))
			return false;
		if (defeated) {
			if (enter_sector != NULL)
				*enter_sector = true;
			return true;
		}
	}
	if (!yt_session_planet_inventory(session, logical, error))
		return false;
	return yt_session_planet_menu(session, logical, enter_sector, error);
}
