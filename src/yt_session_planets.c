#include "yt_session_internal.h"

#include "yt_platform.h"

#include <math.h>
#include <string.h>

bool
read_planet_physical(struct yt_session *session, uint32_t physical_record,
    struct yt_planet *planet, struct yt_error *error)
{
	struct yt_record record;

	if (!yt_database_read(&session->door->game.database,
	    (size_t)physical_record, &record, error))
		return false;
	yt_planet_decode(planet, &record);
	return true;
}

bool
session_write_planet_physical(struct yt_session *session,
    uint32_t physical_record, struct yt_planet *planet, bool encode,
    struct yt_error *error)
{
	if (encode) {
		uint8_t stored_name[YT_TEXT_FIELD_SIZE];

		memcpy(stored_name, planet->record.bytes, sizeof(stored_name));
		yt_planet_encode(planet);
		memcpy(planet->record.bytes, stored_name, sizeof(stored_name));
	}
	return yt_database_write(&session->door->game.database,
	    (size_t)physical_record, &planet->record, error);
}

bool
yt_session_update_planet_physical(struct yt_session *session,
    uint32_t physical_record, struct yt_planet *planet,
    struct yt_planet_economy *economy, struct yt_error *error)
{
	struct yt_planet_economy updated_economy;
	struct yt_planet_update update;
	struct yt_record record;
	int today;
	int adjusted_year;
	float timer_seconds;

	if (!yt_current_date_serial(&session->door->game.clock,
	    session->door->game.config.epoch_year,
	    &today, &adjusted_year, error))
		return false;
	session->door->game.today = today;
	session->door->game.adjusted_year = adjusted_year;
	if (!yt_database_read(&session->door->game.database,
	    (size_t)physical_record, &record, error))
		return false;
	if (!yt_planet_update_prepare(&record, &update, error))
		return false;
	timer_seconds = (float)yt_clock_timer(&session->door->game.clock);
	if (!yt_planet_update_record(&record, &update, (float)today,
	    timer_seconds, &updated_economy, error)
	    || !yt_database_write(&session->door->game.database,
	    (size_t)physical_record, &record, error)
	    || !yt_database_flush(&session->door->game.database, error))
		return false;
	yt_planet_decode(planet, &record);
	session->planet.economy = updated_economy;
	if (economy != NULL)
		*economy = updated_economy;
	return true;
}

bool
yt_session_update_planet(struct yt_session *session, int logical_planet,
    struct yt_planet *planet, struct yt_planet_economy *economy,
    struct yt_error *error)
{
	return yt_session_update_planet_physical(session,
	    session_planet_basic_record(session, (float)logical_planet), planet,
	    economy, error);
}

bool
yt_session_planet_permission(struct yt_session *session,
    int logical_planet, bool *denied, struct yt_error *error)
{
	static const uint8_t governor[] =
	    "This planet has no governor! Hail to the new planetary governor!!";
	static const uint8_t unrest[] =
	    "Due to the unrest caused by the lack of planetary govornment, ";
	static const uint8_t permission[] = "Permission to land is ";
	static const uint8_t denial[] = "DENIED!";
	struct yt_planet planet;
	struct yt_planet fresh;
	struct yt_player current;
	struct yt_player owner;
	uint8_t cached_name[YT_TEXT_FIELD_SIZE];
	size_t cached_name_length;
	uint8_t row[256];
	size_t row_length;
	float cached_owner;
	float cached_ground_forces;
	float first_draw;
	float second_draw;
	float reduced_ground_forces;
	uint32_t physical_planet_record;
	int owner_record;
	bool friendly = false;
	bool vacant;

	if (denied == NULL)
		return false;
	*denied = false;
	physical_planet_record = session_planet_basic_record(session,
	    (float)logical_planet);
	if (!yt_session_update_planet_physical(session, physical_planet_record,
	    &(struct yt_planet){0}, NULL, error)
	    || !read_planet_physical(session, physical_planet_record,
	    &planet, error)
	    || !yt_planet_stored_name(&planet, cached_name,
	    &cached_name_length, error))
		return false;
	cached_owner = planet.owner;
	cached_ground_forces = planet.ground_forces;
	if (floorf(cached_ground_forces) <= 0.0f
	    || cached_owner == (float)session_record(session))
		return true;
	if (cached_owner >= 2.0f
	    && cached_owner <= (float)YT_PLAYER_LAST_RECORD) {
		owner_record = (int)cached_owner;
		if (!yt_game_read_player(&session->door->game,
		    session_record(session), &current, error))
			return false;
		if (current.team != 0.0f) {
			if (!yt_game_read_player(&session->door->game,
			    owner_record, &owner, error))
				return false;
			friendly = yt_sector_force_same_team(current.team,
			    owner.team);
		}
	}
	if (friendly)
		return true;
	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "planet permission late blank", error))
		return false;
	vacant = cached_owner == 0.0f;
	if (!vacant && cached_owner >= 2.0f
	    && cached_owner <= (float)YT_PLAYER_LAST_RECORD) {
		owner_record = (int)cached_owner;
		if (!yt_game_read_player(&session->door->game, owner_record,
		    &owner, error))
			return false;
		vacant = owner.killed_by != 0.0f;
	}
	if (vacant) {
		if (!session_present_text(session, governor,
		    sizeof(governor) - 1U, SESSION_PRESENT_LINE,
		    "vacant planet governor row", error)
		    || !session_sound(session, 1.0f, "vacant planet sound", error)
		    || !session_wait(session, 2.0,
		    "vacant-planet governor wait", error)
		    || !read_planet_physical(session, physical_planet_record,
		    &fresh, error)
		    || !yt_random_next(&session->door->game.random, &first_draw,
		    error)
		    || !yt_random_next(&session->door->game.random, &second_draw,
		    error))
			return false;
		reduced_ground_forces = yt_planet_landing_attrition(first_draw,
		    second_draw, cached_ground_forces);
		if (!session_present_text(session, unrest, sizeof(unrest) - 1U,
		    SESSION_PRESENT_LINE, "vacant planet unrest row", error)
		    || !yt_planet_landing_unrest_row(reduced_ground_forces,
		    cached_ground_forces, row, sizeof(row), &row_length)
		    || !session_present_text(session, row, row_length,
		    SESSION_PRESENT_LINE, "vacant planet reduction row", error))
			return false;
		yt_planet_landing_vacancy_overlay(&fresh,
		    reduced_ground_forces, session_record(session));
		if (!session_write_planet_physical(session,
		    physical_planet_record, &fresh, false, error)
		    || !session_wait(session, 5.0,
		    "vacant-planet unrest wait", error))
			return false;
		return true;
	}
	if (!yt_planet_landing_traffic_row(cached_name, cached_name_length,
	    row, sizeof(row), &row_length)
	    || !session_present_text(session, row, row_length,
	    SESSION_PRESENT_LINE, "planet permission traffic row", error)
	    || !session_present_text(session, permission,
	    sizeof(permission) - 1U, SESSION_PRESENT_RAW,
	    "planet permission prefix", error))
		return false;
	yt_present_set_blink(&session->presentation, 1.0f);
	session_set_foreground(session, 3.0f);
	if (!session_present_text(session, denial, sizeof(denial) - 1U,
	    SESSION_PRESENT_BOLD_LINE, "planet permission denial", error))
		return false;
	*denied = true;
	session_set_foreground(session, 6.0f);
	return true;
}
