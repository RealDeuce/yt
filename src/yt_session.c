#include "yt_session_internal.h"

#include "qb.h"
#include "yt_file.h"
#include "yt_input.h"
#include "yt_main_error.h"
#include "yt_maint.h"
#include "yt_names.h"
#include "yt_output.h"
#include "yt_pager.h"
#include "yt_platform.h"
#include "yt_route.h"
#include "yt_sound.h"
#include "yt_startup_model.h"
#include "yt_text.h"
#include "yt_team.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

float
session_sector_offset(const struct yt_session *session)
{
	return session->door->game.config.sector_offset;
}

float
session_port_offset(const struct yt_session *session)
{
	return session->door->game.config.port_offset;
}

float
session_planet_offset(const struct yt_session *session)
{
	return session->door->game.config.planet_offset;
}

void
session_set_pager_line_count_raw(struct yt_session *session,
    const uint8_t raw[4])
{
	session->pager.line_count = qb_mbf32_decode(raw);
}

void
session_set_pager_line_count(struct yt_session *session, float value)
{
	session->pager.line_count = value;
}

void
session_set_foreground(struct yt_session *session, float value)
{
	session->presentation.foreground = value;
	session->pager.foreground = (int)value;
}

bool
session_current_date_serial(struct yt_session *session, int *serial,
    int *adjusted_year, struct yt_error *error)
{
	return yt_current_date_serial(session->door->game.config.epoch_year,
	    serial, adjusted_year, error);
}

int
session_record(const struct yt_session *session)
{
	return session->active_player_record;
}

bool
session_is_disruption_sector(const struct yt_session *session, float sector)
{
	return sector == session->disruption_sectors[0]
	    || sector == session->disruption_sectors[1];
}

bool
session_wait(struct yt_session *session, double seconds,
    const char *operation, struct yt_error *error)
{
	if (yt_input_pause(&session->io.input, seconds))
		return true;
	if (error != NULL) {
		error->status = YT_IO_ERROR;
		snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
	}
	return false;
}

bool
session_range_error(struct yt_error *error, const char *operation)
{
	if (error != NULL) {
		error->status = YT_RANGE;
		snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
	}
	return false;
}

bool
session_buffer_append(uint8_t *buffer, size_t capacity, size_t *length,
    const void *data, size_t data_length)
{
	if (buffer == NULL || length == NULL || *length > capacity
	    || data_length > capacity - *length
	    || (data == NULL && data_length != 0U))
		return false;
	if (data_length != 0U)
		memcpy(buffer + *length, data, data_length);
	*length += data_length;
	return true;
}

void
session_close_game(struct yt_session *session)
{
	if (session->door->game_open) {
		session->door->game_open = false;
		yt_game_close(&session->door->game);
	}
}

uint32_t
session_sector_basic_record(const struct yt_session *session,
    float logical_sector)
{
	return (uint32_t)yt_sector_basic_record(&session->door->game.config,
	    (int)logical_sector);
}

uint32_t
session_port_basic_record(const struct yt_session *session, float logical_port)
{
	return (uint32_t)yt_port_basic_record(&session->door->game.config,
	    (int)logical_port);
}

uint32_t
session_planet_basic_record(const struct yt_session *session,
    float logical_planet)
{
	return (uint32_t)yt_planet_basic_record(&session->door->game.config,
	    (int)logical_planet);
}

bool
session_read_sector(struct yt_session *session, int logical_sector,
    struct yt_sector *sector, struct yt_error *error)
{
	struct yt_record record;

	if (!yt_database_read(&session->door->game.database,
	    (size_t)session_sector_basic_record(session, (float)logical_sector),
	    &record, error))
		return false;
	yt_sector_decode(sector, &record);
	return true;
}

bool
session_write_sector(struct yt_session *session, int logical_sector,
    struct yt_sector *sector, struct yt_error *error)
{
	yt_sector_encode(sector);
	return yt_database_write(&session->door->game.database,
	    (size_t)session_sector_basic_record(session, (float)logical_sector),
	    &sector->record, error);
}

bool
session_read_port(struct yt_session *session, int logical_port,
    struct yt_port *port, struct yt_error *error)
{
	struct yt_record record;

	if (!yt_database_read(&session->door->game.database,
	    (size_t)session_port_basic_record(session, (float)logical_port),
	    &record, error))
		return false;
	yt_port_decode(port, &record);
	return true;
}

bool
session_write_port(struct yt_session *session, int logical_port,
    struct yt_port *port, struct yt_error *error)
{
	yt_port_encode(port);
	return yt_database_write(&session->door->game.database,
	    (size_t)session_port_basic_record(session, (float)logical_port),
	    &port->record, error);
}

bool
session_read_planet(struct yt_session *session, int logical_planet,
    struct yt_planet *planet, struct yt_error *error)
{
	struct yt_record record;

	if (!yt_database_read(&session->door->game.database,
	    (size_t)session_planet_basic_record(session, (float)logical_planet),
	    &record, error))
		return false;
	yt_planet_decode(planet, &record);
	return true;
}

bool
session_write_planet(struct yt_session *session, int logical_planet,
    struct yt_planet *planet, struct yt_error *error)
{
	yt_planet_encode(planet);
	return yt_database_write(&session->door->game.database,
	    (size_t)session_planet_basic_record(session, (float)logical_planet),
	    &planet->record, error);
}

int
session_sector_count(const struct yt_session *session)
{
	return (int)(session_port_offset(session)
	    - session_sector_offset(session));
}

bool
session_write_player(struct yt_session *session, struct yt_error *error)
{
	return yt_game_write_player(&session->door->game,
	    session_record(session), &session->player, error)
	    && yt_database_flush(&session->door->game.database, error);
}


static bool
basic_fault_retries(const struct yt_error *error)
{
	struct yt_basic_fault_projection projection;

	return yt_basic_fault_project(error, NULL, 0U, NULL, 0U, NULL, 0U,
	    &projection)
	    && projection.disposition == YT_BASIC_FAULT_RETRY_STATEMENT;
}

bool
read_database_record_at_fault(struct yt_session *session,
    uint32_t physical_record, struct yt_record *record,
    enum yt_basic_fault_site site, struct yt_error *error)
{
	for (;;) {
		if (yt_database_read(&session->door->game.database,
		    (size_t)physical_record, record, error))
			return true;
		attach_database_get_fault(session, error, site);
		if (!basic_fault_retries(error))
			return false;
		yt_error_clear(error);
	}
}

bool
session_read_player_at_fault(struct yt_session *session, int player_record,
    struct yt_player *player, enum yt_basic_fault_site site,
    struct yt_error *error)
{
	for (;;) {
		if (yt_game_read_player(&session->door->game, player_record, player,
		    error))
			return true;
		attach_database_get_fault(session, error, site);
		if (!basic_fault_retries(error))
			return false;
		yt_error_clear(error);
	}
}

bool
session_read_sector_at_fault(struct yt_session *session, int logical_sector,
    struct yt_sector *sector, enum yt_basic_fault_site site,
    struct yt_error *error)
{
	for (;;) {
		if (session_read_sector(session, logical_sector, sector,
		    error))
			return true;
		attach_database_get_fault(session, error, site);
		if (!basic_fault_retries(error))
			return false;
		yt_error_clear(error);
	}
}

bool
session_read_port_at_fault(struct yt_session *session, int logical_port,
    struct yt_port *port, enum yt_basic_fault_site site,
    struct yt_error *error)
{
	for (;;) {
		if (session_read_port(session, logical_port, port,
		    error))
			return true;
		attach_database_get_fault(session, error, site);
		if (!basic_fault_retries(error))
			return false;
		yt_error_clear(error);
	}
}

bool
write_database_record_at_fault(struct yt_session *session,
    uint32_t physical_record, const struct yt_record *record,
    enum yt_basic_fault_site site, struct yt_error *error)
{
	for (;;) {
		if (yt_database_write(&session->door->game.database,
		    (size_t)physical_record, record, error))
			break;
		attach_database_put_fault(session, error, site);
		if (!basic_fault_retries(error))
			return false;
		yt_error_clear(error);
	}
	return yt_database_flush(&session->door->game.database, error);
}

bool
session_reload_player(struct yt_session *session, struct yt_error *error)
{
	struct yt_player fresh;

	if (!session_read_player_at_fault(session, session_record(session), &fresh,
	    YT_BASIC_FAULT_CURRENT_PLAYER_A41C_GET, error)
	    || !yt_current_player_hydrate(&session->player, &fresh,
	    session_record(session), session_sector_offset(session),
	    session->earth.anti_cloak_enabled, &session->navigation.current_sector_physical_record,
	    &session->player_cache, error))
		return false;
	session->combat.ship_fighters = session->player.fighters;
	session->combat.ship_shields = session->player.shields;
	return true;
}

bool
session_mutate_player_credits(struct yt_session *session, float argument,
    bool *hydrated, struct yt_error *error)
{
	uint8_t raw[4];
	float sum;
	float result;

	if (hydrated != NULL)
		*hydrated = false;
	if (qb_mbf32_encode(argument, raw) == QB_MBF_OVERFLOW) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation,
			    sizeof(error->operation), "%s",
			    "credit mutation argument MBF32");
		}
		return false;
	}
	if (!session_reload_player(session, error))
		return false;
	if (hydrated != NULL)
		*hydrated = true;
	sum = qb_single_add(session->player.credits, argument);
	result = floorf(sum);
	if (qb_mbf32_encode(sum, raw) == QB_MBF_OVERFLOW
	    || qb_mbf32_encode(result, raw) == QB_MBF_OVERFLOW) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation,
			    sizeof(error->operation), "%s",
			    "credit mutation result MBF32");
		}
		return false;
	}
	session->player.credits = qb_mbf32_decode(raw);
	if (!yt_record_set_raw_number(&session->player.record, YT_F81, raw)) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation,
			    sizeof(error->operation), "%s",
			    "credit mutation overlay");
		}
		return false;
	}
	return yt_database_write_durable(&session->door->game.database,
	    (size_t)session_record(session), &session->player.record, error);
}
