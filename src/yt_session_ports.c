#include "yt_session_internal.h"

#include "yt_platform.h"

#include <stdio.h>
#include <string.h>

static float
port_single_add(float left, float right)
{
	volatile float result = left + right;

	return result;
}

static bool
port_update_error(struct yt_error *error, const char *operation)
{
	if (error != NULL) {
		error->status = YT_RANGE;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
	}
	return false;
}

bool
yt_session_update_port(struct yt_session *session, int sector_number,
    const float *sector_record_expression, const struct yt_sector *loaded_sector,
    struct yt_port_market_state *market, struct yt_error *error)
{
	struct yt_sector sector;
	struct yt_record record;
	float sector_expression;
	uint32_t sector_physical_record;
	int today;
	int adjusted_year;

	if (session == NULL || market == NULL)
		return false;
	memset(market, 0, sizeof(*market));
	if (loaded_sector != NULL)
		sector = *loaded_sector;
	else {
		sector_expression = sector_record_expression != NULL
		    ? *sector_record_expression
		    : port_single_add(session_sector_offset(session),
		    (float)sector_number);
		sector_physical_record = qb_brun_random_record_number(
		    sector_expression);
		if (sector_physical_record == 0U)
			return port_update_error(error,
			    "ordinary port sector record conversion");
		if (!read_database_record_at_fault(session,
		    sector_physical_record, &record,
		    YT_BASIC_FAULT_PORT_UPDATER_SECTOR_GET, error))
			return false;
		yt_sector_decode(&sector, &record);
	}
	market->logical_port = sector.port;
	market->port_record_expression = port_single_add(
	    session_port_offset(session), market->logical_port);
	market->port_physical_record = qb_brun_random_record_number(
	    market->port_record_expression);
	if (market->port_physical_record == 0U)
		return port_update_error(error,
		    "ordinary port record conversion");
	if (!yt_current_date_serial(session->door->game.config.epoch_year,
	    &today, &adjusted_year, error))
		return false;
	session->door->game.today = today;
	session->door->game.adjusted_year = adjusted_year;
	market->current_day = (float)today;
	if (!read_database_record_at_fault(session,
	    market->port_physical_record, &record,
	    YT_BASIC_FAULT_PORT_UPDATER_PORT_GET, error))
		return false;
	yt_port_decode(&market->port, &record);
	market->timer_seconds = (float)yt_platform_timer();
	memcpy(market->base_price, session->market_bases,
	    sizeof(market->base_price));
	if (!yt_port_market_update(market, error))
		return false;
	return write_database_record_at_fault(session,
	    market->port_physical_record, &market->port.record,
	    YT_BASIC_FAULT_PORT_UPDATER_PORT_PUT, error);
}
