#include "yt_session_internal.h"

#include "qb.h"
#include "yt_platform.h"
#include "yt_port_math.h"

#include <string.h>

bool
session_read_port_physical(struct yt_session *session,
    uint32_t physical_record, struct yt_port *port, struct yt_error *error)
{
	struct yt_record record;

	if (!yt_database_read(&session->door->game.database,
	    (size_t)physical_record, &record, error))
		return false;
	yt_port_decode(port, &record);
	return true;
}

bool
yt_session_update_port(struct yt_session *session, int sector_number,
    const struct yt_sector *loaded_sector,
    struct yt_port_market_state *market, struct yt_error *error)
{
	struct yt_sector sector;
	struct yt_record record;
	uint32_t sector_physical_record;
	int today;
	int adjusted_year;

	if (session == NULL || market == NULL)
		return false;
	memset(market, 0, sizeof(*market));
	if (loaded_sector != NULL)
		sector = *loaded_sector;
	else {
		sector_physical_record = session_sector_basic_record(session,
		    sector_number);
		if (!read_database_record_at_fault(session,
		    sector_physical_record, &record,
		    YT_BASIC_FAULT_PORT_UPDATER_SECTOR_GET, error))
			return false;
		yt_sector_decode(&sector, &record);
	}
	market->logical_port = sector.port;
	market->port_physical_record = session_port_basic_record(session,
	    market->logical_port);
	if (!yt_current_date_serial(&session->door->game.clock,
	    session->door->game.config.epoch_year,
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
	market->timer_seconds = (float)yt_clock_timer(&session->door->game.clock);
	memcpy(market->base_price, session->market_bases,
	    sizeof(market->base_price));
	if (!yt_port_market_update(market, error))
		return false;
	return write_database_record_at_fault(session,
	    market->port_physical_record, &market->port.record,
	    YT_BASIC_FAULT_PORT_UPDATER_PORT_PUT, error);
}

static bool
port_report_owner(struct yt_session *session,
    const struct yt_port_market_state *market, struct yt_error *error)
{
	enum yt_port_owner_kind kind;
	struct yt_player owner;
	struct yt_record record;
	const uint8_t *owner_name = NULL;
	uint8_t row[256];
	size_t owner_name_length = 0U;
	size_t row_length;
	int owner_record;

	kind = yt_port_owner_classify((int)market->port.owner,
	    session_record(session), &owner_record);
	if (kind == YT_PORT_OWNER_SILENT)
		return true;
	if (kind == YT_PORT_OWNER_OTHER) {
		if (!read_database_record_at_fault(session,
		    (uint32_t)owner_record, &record,
		    YT_BASIC_FAULT_PORT_OWNER_PLAYER_GET, error))
			return false;
		yt_player_decode(&owner, &record);
		owner_name_length = owner.name_length;
		if (owner_name_length > YT_TEXT_FIELD_SIZE)
			owner_name_length = YT_TEXT_FIELD_SIZE;
		owner_name = owner.record.bytes;
	}
	if (!yt_port_owner_compose(kind, market->port.treasury,
	    owner_name, owner_name_length, row, sizeof(row), &row_length))
		return session_range_error(error, "port owner row composition");
	return session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "port owner leading blank", error)
	    && session_present_text(session, row, row_length,
	    SESSION_PRESENT_LINE, "port owner row", error);
}

bool
yt_session_port_report(struct yt_session *session, int logical_port,
    const struct yt_port_market_state *market,
    struct yt_port *terminal_port, struct yt_error *error)
{
	static const uint8_t header[] =
	    " Items         Status      # units    in holds   Cost";
	static const uint8_t rule[] =
	    "=======       =========   =========   ========   ====";
	struct yt_clock_value now;
	struct yt_player current_player;
	struct yt_port report_port;
	struct yt_port_report_text report;
	struct yt_record record;
	uint8_t date[10];
	uint8_t time_text[8];
	char rendered_date[11];
	char rendered_time[9];
	uint32_t physical_record;
	size_t index;

	if (session == NULL || market == NULL)
		return false;
	physical_record = market->port_physical_record != 0U
	    ? market->port_physical_record
	    : session_port_basic_record(session, logical_port);
	session_set_pager_line_count(session, 0.0f);
	if (!port_report_owner(session, market, error)
	    || !session_reload_player(session, error))
		return false;
	current_player = session->player;
	if (!read_database_record_at_fault(session, physical_record, &record,
	    YT_BASIC_FAULT_PORT_REPORT_PORT_GET, error))
		return false;
	yt_port_decode(&report_port, &record);
	if (!yt_clock_read(&session->door->game.clock, &now, error))
		return false;
	yt_format_date(&now, rendered_date);
	memcpy(date, rendered_date, sizeof(date));
	if (!yt_clock_read(&session->door->game.clock, &now, error))
		return false;
	yt_format_time(&now, rendered_time);
	memcpy(time_text, rendered_time, sizeof(time_text));
	if (!yt_port_report_compose(market, &current_player, &report_port,
	    date, time_text,
	    &report, error)
	    || !session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "port report title blank", error)
	    || !session_present_paged_row(session, report.title,
	    report.title_length)
	    || !session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "port report header blank", error)
	    || !session_present_paged_row(session, header, sizeof(header) - 1U))
		return false;
	yt_present_set_bold(&session->presentation, 1.0f);
	if (!session_present_paged_row(session, rule, sizeof(rule) - 1U))
		return false;
	for (index = 0U; index < 3U; ++index) {
		const struct yt_port_report_item *item = &report.item[index];

		session_set_foreground(session, item->foreground);
		if (!session_present_text(session, item->name_status,
		    sizeof(item->name_status), SESSION_PRESENT_RAW,
		    "port report commodity/status", error)
		    || !session_present_text(session, item->capacity,
		    sizeof(item->capacity), SESSION_PRESENT_RAW,
		    "port report stock", error)
		    || !session_present_text(session, item->hold,
		    sizeof(item->hold), SESSION_PRESENT_RAW,
		    "port report player hold", error)
		    || !session_present_text(session, item->price,
		    item->price_length, SESSION_PRESENT_LINE,
		    "port report price", error))
			return false;
	}
	session_set_foreground(session, 3.0f);
	if (terminal_port != NULL)
		*terminal_port = report_port;
	return true;
}
