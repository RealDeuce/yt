#include "yt_session_internal.h"

#include "qb.h"
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

static bool
port_name_length(const uint8_t raw[4], uint8_t conversion_mode,
    size_t *length, struct yt_error *error)
{
	bool overflow;
	int32_t converted = qb_cint_mbf32(raw, conversion_mode, &overflow);

	if (overflow || converted < 0)
		return port_update_error(error, "port owner name length");
	*length = (size_t)converted;
	if (*length > YT_TEXT_FIELD_SIZE)
		*length = YT_TEXT_FIELD_SIZE;
	return true;
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

	kind = yt_port_owner_classify(market->port.owner,
	    session_record(session), &owner_record);
	if (kind == YT_PORT_OWNER_INVALID)
		return port_update_error(error, "port owner record conversion");
	if (kind == YT_PORT_OWNER_SILENT)
		return true;
	if (kind == YT_PORT_OWNER_OTHER) {
		if (!read_database_record_at_fault(session,
		    (uint32_t)owner_record, &record,
		    YT_BASIC_FAULT_PORT_OWNER_PLAYER_GET, error))
			return false;
		yt_player_decode(&owner, &record);
		if (!port_name_length(owner.record.bytes + YT_F85,
		    session->presentation.sound.conversion_mode,
		    &owner_name_length, error))
			return false;
		owner_name = owner.record.bytes;
	}
	if (!yt_port_owner_compose(kind, market->port.treasury,
	    owner_name, owner_name_length, row, sizeof(row), &row_length))
		return port_update_error(error, "port owner row composition");
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
	static const uint8_t pager_dirty_zero[4] = {0x00, 0x00, 0x01, 0x00};
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
	    : qb_brun_random_record_number(port_single_add(
	    session_port_offset(session), (float)logical_port));
	if (physical_record == 0U)
		return port_update_error(error, "port report record conversion");
	session_set_pager_line_count_raw(session, pager_dirty_zero);
	if (!port_report_owner(session, market, error)
	    || !session_reload_player(session, error))
		return false;
	current_player = session->player;
	if (!read_database_record_at_fault(session, physical_record, &record,
	    YT_BASIC_FAULT_PORT_REPORT_PORT_GET, error))
		return false;
	yt_port_decode(&report_port, &record);
	if (!yt_platform_clock(&now, error))
		return false;
	yt_format_date(&now, rendered_date);
	memcpy(date, rendered_date, sizeof(date));
	if (!yt_platform_clock(&now, error))
		return false;
	yt_format_time(&now, rendered_time);
	memcpy(time_text, rendered_time, sizeof(time_text));
	if (!yt_port_report_compose(market, &current_player, &report_port,
	    session->presentation.sound.conversion_mode, date, time_text,
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

static double
port_double_sub(double left, double right)
{
	volatile double result = left - right;

	return result;
}

static bool
port_append(uint8_t *buffer, size_t capacity, size_t *length,
    const void *text, size_t text_length)
{
	if (*length > capacity || text_length > capacity - *length
	    || (text == NULL && text_length != 0U))
		return false;
	if (text_length != 0U)
		memcpy(buffer + *length, text, text_length);
	*length += text_length;
	return true;
}

bool
yt_session_ordinary_commerce(struct yt_session *session,
    int sector_number, float sector_record_expression,
    struct yt_error *error)
{
	static const uint8_t refusal_prefix[] =
	    "We don't want your goods and you can't buy ours ";
	static const uint8_t refusal_suffix[] = "!";
	struct yt_port_market_state market;
	uint8_t row[256];
	char credits[64];
	char free_holds[64];
	double free;
	bool prompt_reached = false;
	size_t index;

	if (!yt_session_update_port(session, sector_number,
	    &sector_record_expression, NULL, &market, error)
	    || !yt_session_port_report(session, (int)market.logical_port,
	    &market, NULL, error))
		return false;
	for (index = 0U; index < 3U; ++index) {
		bool reached = false;

		if (market.port.factor[index] < 0.0f
		    && !yt_session_trade_commodity(session, &market, index,
		    &reached, error))
			return false;
		if (reached)
			prompt_reached = true;
	}
	for (index = 0U; index < 3U; ++index) {
		bool reached = false;

		if (market.port.factor[index] > 0.0f
		    && !yt_session_trade_commodity(session, &market, index,
		    &reached, error))
			return false;
		if (reached)
			prompt_reached = true;
	}
	if (!prompt_reached) {
		size_t length = 0U;
		const char *first_name = session->door->identity.real_first;
		size_t first_name_length = strlen(first_name);

		session_set_foreground(session, 6.0f);
		if (!port_append(row, sizeof(row), &length, refusal_prefix,
		    sizeof(refusal_prefix) - 1U)
		    || !port_append(row, sizeof(row), &length, first_name,
		    first_name_length)
		    || !port_append(row, sizeof(row), &length, refusal_suffix,
		    sizeof(refusal_suffix) - 1U))
			return port_update_error(error,
			    "ordinary commerce refusal composition");
		if (!session_present_alert(session, row, length,
		    "port docking refusal", error))
			return false;
	}
	if (!session_reload_player(session, error))
		return false;
	free = port_double_sub((double)session->player.holds,
	    (double)session->player.ore);
	free = port_double_sub(free, (double)session->player.organics);
	free = port_double_sub(free, (double)session->player.equipment);
	if (qb_str_double(credits, sizeof(credits),
	    (double)session->player.credits) < 0
	    || qb_str_double(free_holds, sizeof(free_holds), free) < 0)
		return port_update_error(error,
		    "ordinary commerce status formatting");
	{
		int length = snprintf((char *)row, sizeof(row),
		    "You have%s credits and%s empty cargo holds.",
		    credits, free_holds);

		if (length < 0 || (size_t)length >= sizeof(row))
			return port_update_error(error,
			    "ordinary commerce status composition");
		if (!session_present_paged_line(session, row, (size_t)length,
		    "port docking cargo status", error))
			return false;
	}
	session->inherited_loop_index = 4.0f;
	return true;
}
