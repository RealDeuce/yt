#include "yt_session_internal.h"

#include "qb.h"
#include "yt_platform.h"
#include "yt_port_math.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

struct profit_report {
	bool global;
	float base_price[3];
	size_t result_count;
	size_t rows;
};

static bool
profit_error(struct yt_error *error, enum yt_status status,
    const char *operation)
{
	if (error != NULL) {
		error->status = status;
		error->system_error = 0;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
		error->path[0] = '\0';
	}
	return false;
}

static bool
profit_single(float value, float *result, struct yt_error *error,
    const char *operation)
{
	uint8_t raw[4];
	enum qb_mbf_status status = qb_mbf32_encode(value, raw);

	if (!isfinite(value) || status == QB_MBF_OVERFLOW
	    || status == QB_MBF_DOMAIN)
		return profit_error(error, YT_RANGE, operation);
	*result = qb_mbf32_decode(raw);
	return true;
}

static bool
profit_add(float left, float right, float *result, struct yt_error *error,
    const char *operation)
{
	return profit_single(qb_single_add(left, right), result, error,
	    operation);
}

static bool
profit_sub(float left, float right, float *result, struct yt_error *error,
    const char *operation)
{
	return profit_single(qb_single_subtract(left, right), result, error,
	    operation);
}

static bool
profit_project(struct yt_session *session, const struct profit_report *report,
    const struct yt_port *port, struct yt_nearest_market *market,
    float prices[4], struct yt_error *error)
{
	int today;
	int adjusted_year;
	int16_t current_day;
	float current_day_single;
	float timer_seconds;

	if (!yt_current_date_serial(&session->door->game.clock,
	    (float)session->door->game.config.epoch_year,
	    &today, &adjusted_year, error))
		return false;
	session->door->game.today = today;
	session->door->game.adjusted_year = adjusted_year;
	current_day_single = (float)today;
	if (!profit_single(current_day_single, &current_day_single, error,
	    "profit current day"))
		return false;
	current_day = (int16_t)current_day_single;
	timer_seconds = (float)yt_clock_timer(&session->door->game.clock);
	if (!profit_single(timer_seconds, &timer_seconds, error,
	    "profit TIMER"))
		return false;
	if (!yt_nearest_market_project(market, port, report->base_price,
	    current_day, timer_seconds, error))
		return false;
	prices[0] = 0.0f;
	prices[1] = market->price[0];
	prices[2] = market->price[1];
	prices[3] = market->price[2];
	return true;
}

static void
profit_warp_targets(const struct yt_sector *sector, int targets[6])
{
	size_t slot;

	for (slot = 0U; slot < 6U; ++slot)
		targets[slot] = sector->warps[slot];
}

static void
profit_pair_color(struct yt_session *session, int source, int target)
{
	if ((source == 1 && target == 2)
	    || (source == 2 && target == 1))
		session_set_foreground(session, 3);
	else if ((source == 1 && target == 3)
	    || (source == 3 && target == 1))
		session_set_foreground(session, 2);
	else if ((source == 2 && target == 3)
	    || (source == 3 && target == 2))
		session_set_foreground(session, 1);
}

static int
profit_price_index(int commodity_class)
{
	if (commodity_class == 1)
		return 3;
	if (commodity_class == 2)
		return 2;
	return 1;
}

static int
profit_class_text(int commodity_class)
{
	if (commodity_class == 1)
		return 0;
	if (commodity_class == 2)
		return 1;
	return 2;
}

static bool
profit_right_four(uint8_t result[4], float value, bool integer)
{
	char number[64];
	uint8_t source[68];
	int rendered = integer
	    ? qb_str_integer(number, sizeof(number), (int16_t)(int)value)
	    : qb_str_single(number, sizeof(number), value);
	size_t length;
	size_t amount;

	if (rendered < 0)
		return false;
	source[0] = ' ';
	source[1] = ' ';
	memcpy(source + 2U, number, (size_t)rendered);
	length = 2U + (size_t)rendered;
	amount = length < 4U ? length : 4U;
	memset(result, ' ', 4U - amount);
	memcpy(result + 4U - amount, source + length - amount, amount);
	return true;
}

static bool
profit_compose_row(struct yt_session *session, uint16_t source_number,
    uint16_t target_number, const struct yt_port *source_port,
    const struct yt_port *target_port, const float source_price[4],
    const float target_price[4], uint8_t row[36], struct yt_error *error)
{
	static const uint8_t arrows[3][7] = {
		{'E', 'q', 'u', ' ', '-', '>', ' '},
		{'O', 'r', 'g', ' ', '-', '>', ' '},
		{'O', 'r', 'e', ' ', '-', '>', ' '},
	};
	static const uint8_t labels[3][3] = {
		{'E', 'q', 'u'}, {'O', 'r', 'g'}, {'O', 'r', 'e'},
	};
	float source_leg;
	float target_leg;
	float profit;
	int source_index = profit_price_index(source_port->commodity_class);
	int target_index = profit_price_index(target_port->commodity_class);
	int source_text = profit_class_text(source_port->commodity_class);
	int target_text = profit_class_text(target_port->commodity_class);
	char number[64];
	uint8_t field[68];
	int rendered;
	size_t length = 0U;

	if (!profit_sub(source_price[source_index], target_price[source_index],
	    &source_leg, error, "profit source leg"))
		return false;
	if (!profit_single(fabsf(source_leg), &source_leg, error,
	    "profit source ABS"))
		return false;
	if (!profit_sub(target_price[target_index], source_price[target_index],
	    &target_leg, error, "profit target leg"))
		return false;
	if (!profit_single(fabsf(target_leg), &target_leg, error,
	    "profit target ABS"))
		return false;
	if (!profit_add(source_leg, target_leg, &profit, error,
	    "profit spread"))
		return false;
	profit_pair_color(session, source_port->commodity_class,
	    target_port->commodity_class);
	if (!profit_right_four(row + length, (float)source_number, false))
		return profit_error(error, YT_RANGE, "profit source formatting");
	length += 4U;
	row[length++] = ',';
	if (!profit_right_four(row + length, (float)target_number, true))
		return profit_error(error, YT_RANGE, "profit target formatting");
	length += 4U;
	row[length++] = ' ';
	memcpy(row + length, arrows[source_text], 7U);
	length += 7U;
	memcpy(row + length, labels[target_text], 3U);
	length += 3U;
	memcpy(row + length, " @ Profit of", 12U);
	length += 12U;
	rendered = qb_str_single(number, sizeof(number), profit);
	if (rendered < 0 || (size_t)rendered + 3U > sizeof(field))
		return profit_error(error, YT_RANGE, "profit value formatting");
	memcpy(field, number, (size_t)rendered);
	memset(field + (size_t)rendered, ' ', 3U);
	memcpy(row + length, field, 4U);
	length += 4U;
	if (length != 36U)
		return profit_error(error, YT_RANGE, "profit row width");
	return true;
}

static bool
profit_page(struct yt_session *session, bool *keep_going,
    struct yt_error *error)
{
	static const uint8_t prompt[] = "More? [Y/n] ";
	struct yt_input_value selected;
	uint8_t response[8];
	size_t length;

	for (;;) {
		if (!session_present_text(session, prompt, sizeof(prompt) - 1U,
		    SESSION_PRESENT_RAW, "profit pager prompt", error))
			return false;
		for (;;) {
			if (!yt_input_wait(&session->io.input, &selected))
				return false;
			if (selected.length != 0U)
				break;
		}
		if (selected.length > sizeof(response))
			return profit_error(error, YT_RANGE,
			    "profit pager response");
		length = selected.length;
		memcpy(response, selected.bytes, length);
		if (length == 1U && response[0] == '\r')
			response[0] = 'Y';
		yt_input_compat_upper_n(response, length);
		if (!session_present_text(session, response, length,
		    SESSION_PRESENT_LINE, "profit pager echo", error))
			return false;
		if (length == 1U && (response[0] == 'Y' || response[0] == 'N')) {
			*keep_going = response[0] == 'Y';
			return true;
		}
	}
}

static bool
profit_emit_pair(struct yt_session *session, struct profit_report *report,
    uint16_t source_number, uint16_t target_number,
    const struct yt_port *source_port, const struct yt_port *target_port,
    const float source_price[4], const float target_price[4],
    bool *keep_going, struct yt_error *error)
{
	static const uint8_t separator[] = {' ', 0xba, ' '};
	uint8_t row[36];
	*keep_going = true;
	if (report->global)
		++report->result_count;
	if (!profit_compose_row(session, source_number, target_number,
	    source_port, target_port, source_price, target_price, row, error))
		return false;
	if (!report->global) {
		if (!session_present_text(session, row, sizeof(row),
		    SESSION_PRESENT_BOLD_LINE, "profit row", error))
			return false;
		++report->rows;
		return true;
	}
	if (!session_present_text(session, row, sizeof(row),
	    SESSION_PRESENT_BOLD_RAW, "profit row", error))
		return false;
	++report->rows;
	session_set_foreground(session, 6);
	if ((report->result_count & 1U) != 0U) {
		if (!session_present_text(session, separator, sizeof(separator),
		    SESSION_PRESENT_BOLD_RAW, "global profit separator", error))
			return false;
	} else if (!session_present_text(session, NULL, 0U,
	    SESSION_PRESENT_LINE, "global profit row ending", error)) {
		return false;
	}
	if (report->result_count % 44U == 0U) {
		if (!profit_page(session, keep_going, error))
			return false;
	}
	return true;
}

static bool
profit_adjacent(struct yt_session *session, struct profit_report *report,
    struct yt_error *error)
{
	static const uint8_t title[] =
	    "Profits of a two way trade to ports in adjacent sectors.";
	static const uint8_t no_port[] = "NO trading port in your sector!";
	static const uint8_t no_results[] =
	    "No ports you can trade with in adjacent sectors!";
	struct yt_record raw;
	struct yt_sector sector;
	struct yt_port source_port;
	struct yt_nearest_market source_market;
	float source_prices[4];
	int current_sector_record =
	    session->navigation.current_sector_physical_record;
	uint16_t display_source = (uint16_t)(current_sector_record
	    - session_sector_offset(session));
	int warps[6];
	size_t slot;

	session_set_foreground(session, 7);
	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "profit leading blank", error))
		return false;
	if (!session_present_text(session, title, sizeof(title) - 1U,
	    SESSION_PRESENT_BOLD_LINE, "adjacent profit title", error))
		return false;
	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "adjacent profit title blank", error))
		return false;
	if (!yt_database_read(&session->door->game.database,
	    (size_t)current_sector_record, &raw, error))
		return false;
	yt_sector_decode(&sector, &raw);
	if (sector.port == 0 || sector.port == 1)
		return session_present_text(session, no_port, sizeof(no_port) - 1U,
		    SESSION_PRESENT_BOLD_LINE, "adjacent profit no-port row",
		    error);
	profit_warp_targets(&sector, warps);
	if (!yt_database_read(&session->door->game.database,
	    (size_t)session_port_basic_record(session, sector.port),
	    &raw, error))
		return false;
	yt_port_decode(&source_port, &raw);
	if (!profit_project(session, report, &source_port, &source_market,
	    source_prices, error))
		return false;
	for (slot = 0U; slot < 6U; ++slot) {
		struct yt_sector target_sector;
		struct yt_port target_port;
		struct yt_nearest_market target_market;
		float target_prices[4];
		int target_record;
		int target = warps[slot];
		bool keep_going;

		if (target <= 1)
			continue;
		target_record = (int)session_sector_basic_record(session, target);
		if (!yt_database_read(&session->door->game.database,
		    (size_t)target_record, &raw, error))
			return false;
		yt_sector_decode(&target_sector, &raw);
		if (target_sector.port == 0)
			continue;
		target_record = (int)session_port_basic_record(session,
		    target_sector.port);
		if (!yt_database_read(&session->door->game.database,
		    (size_t)target_record, &raw, error))
			return false;
		yt_port_decode(&target_port, &raw);
		if (target_port.commodity_class == source_port.commodity_class)
			continue;
		if (!profit_project(session, report, &target_port,
		    &target_market, target_prices, error))
			return false;
		if (!profit_emit_pair(session, report, display_source,
		    (uint16_t)target,
		    &source_port, &target_port, source_prices, target_prices,
		    &keep_going, error))
			return false;
	}
	if (report->rows == 0U)
		return session_present_text(session, no_results,
		    sizeof(no_results) - 1U, SESSION_PRESENT_BOLD_LINE,
		    "adjacent profit empty row", error);
	return true;
}

static bool
profit_global(struct yt_session *session, struct profit_report *report,
    struct yt_error *error)
{
	static const uint8_t end_banner[] = " *-[ End of List ]-*";
	int maximum_sector = session_sector_count(session);
	int source;

	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "profit leading blank", error))
		return false;
	for (source = 2; source <= maximum_sector; ++source) {
		struct yt_record raw;
		struct yt_sector sector;
		struct yt_port source_port;
		struct yt_nearest_market source_market;
		float source_prices[4];
		int physical_record;
		int warps[6];
		size_t slot;

		physical_record = (int)session_sector_basic_record(session, source);
		if (!yt_database_read(&session->door->game.database,
		    (size_t)physical_record, &raw, error))
			return false;
		yt_sector_decode(&sector, &raw);
		profit_warp_targets(&sector, warps);
		if (sector.port <= 0)
			continue;
		physical_record = (int)session_port_basic_record(session,
		    sector.port);
		if (!yt_database_read(&session->door->game.database,
		    (size_t)physical_record, &raw, error))
			return false;
		yt_port_decode(&source_port, &raw);
		if (!profit_project(session, report, &source_port,
		    &source_market, source_prices, error))
			return false;
		for (slot = 0U; slot < 6U; ++slot) {
			struct yt_sector target_sector;
			struct yt_port target_port;
			struct yt_nearest_market target_market;
			float target_prices[4];
			int target = warps[slot];
			bool keep_going;

			if (target <= 1)
				continue;
			physical_record = (int)session_sector_basic_record(session,
			    target);
			if (!yt_database_read(&session->door->game.database,
			    (size_t)physical_record, &raw, error))
				return false;
			yt_sector_decode(&target_sector, &raw);
			if (target_sector.port == 0 || target <= source)
				continue;
			physical_record = (int)session_port_basic_record(session,
			    target_sector.port);
			if (!yt_database_read(&session->door->game.database,
			    (size_t)physical_record, &raw, error))
				return false;
			yt_port_decode(&target_port, &raw);
			if (target_port.commodity_class
			    == source_port.commodity_class)
				continue;
			if (!profit_project(session, report, &target_port,
			    &target_market, target_prices, error))
				return false;
			if (!profit_emit_pair(session, report, (uint16_t)source,
			    (uint16_t)target, &source_port, &target_port,
			    source_prices, target_prices, &keep_going, error))
				return false;
			if (!keep_going)
				return true;
		}
	}
	session_set_foreground(session, 7);
	return session_present_text(session, end_banner,
	    sizeof(end_banner) - 1U, SESSION_PRESENT_BOLD_LINE,
	    "global profit end row", error);
}

bool
yt_session_computer_profit(struct yt_session *session, bool global,
    struct yt_error *error)
{
	struct profit_report report;

	memset(&report, 0, sizeof(report));
	report.global = global;
	memcpy(report.base_price, session->market_bases,
	    sizeof(report.base_price));
	return global ? profit_global(session, &report, error)
	    : profit_adjacent(session, &report, error);
}
