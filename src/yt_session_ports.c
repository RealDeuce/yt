#include "yt_session_internal.h"

#include "qb.h"
#include "yt_platform.h"
#include "yt_port_math.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

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
		    : yt_port_single_add(session_sector_offset(session),
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
	market->port_record_expression = yt_port_single_add(
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
	    : qb_brun_random_record_number(yt_port_single_add(
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

struct commodity_trade_terms {
	uint8_t selected_quantity_raw[8];
	uint8_t floored_quantity_raw[8];
	double selected_quantity;
	double displayed_hold;
	double credits;
	float factor;
	float price;
	float free_holds;
	float maximum;
	bool port_sells;
};

static bool
commodity_error(struct yt_error *error, const char *operation)
{
	if (error != NULL) {
		error->status = YT_RANGE;
		error->system_error = 0;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
		error->path[0] = '\0';
	}
	return false;
}

static double
commodity_double_sub(double left, double right)
{
	volatile double result = left - right;

	return result;
}

static double
commodity_double_div(double numerator, double denominator)
{
	volatile double result = numerator / denominator;

	return result;
}

static bool
commodity_row(char *row, size_t capacity, const char *format,
    const char *first, const char *second, struct yt_error *error,
    const char *operation)
{
	int length = snprintf(row, capacity, format, first, second);

	if (length < 0 || (size_t)length >= capacity)
		return commodity_error(error, operation);
	return true;
}

static bool
commodity_prepare(const struct yt_port_market_state *market,
    const struct yt_player *player, size_t commodity,
    struct commodity_trade_terms *terms, struct yt_error *error)
{
	uint8_t single_raw[4];
	uint8_t promoted_raw[8];
	double free_holds;

	memset(terms, 0, sizeof(*terms));
	memcpy(terms->selected_quantity_raw, market->capacity_raw[commodity],
	    sizeof(terms->selected_quantity_raw));
	terms->selected_quantity = qb_mbf64_decode(
	    terms->selected_quantity_raw);
	if (qb_mbf64_floor_raw(terms->selected_quantity_raw,
	    terms->floored_quantity_raw) != QB_MBF_OK)
		return commodity_error(error,
		    "commodity trade cached quantity INT");
	terms->displayed_hold = commodity == 0U ? (double)player->ore
	    : commodity == 1U ? (double)player->organics
	    : (double)player->equipment;
	terms->factor = market->port.factor[commodity];
	terms->price = market->price[commodity];
	terms->credits = (double)player->credits;
	free_holds = commodity_double_sub((double)player->holds,
	    (double)player->ore);
	free_holds = commodity_double_sub(free_holds,
	    (double)player->organics);
	free_holds = commodity_double_sub(free_holds,
	    (double)player->equipment);
	terms->free_holds = (float)free_holds;
	terms->port_sells = floorf(terms->factor) > 0.0f;
	if (!terms->port_sells) {
		if (qb_mbf32_from_mbf64_raw(terms->floored_quantity_raw,
		    single_raw) == QB_MBF_OVERFLOW)
			return commodity_error(error,
			    "commodity trade cached quantity CSNG");
		terms->maximum = qb_mbf32_decode(single_raw);
		if ((double)terms->maximum > floor(terms->displayed_hold))
			terms->maximum = (float)floor(terms->displayed_hold);
		return true;
	}
	terms->maximum = terms->free_holds;
	if (qb_mbf32_encode(terms->maximum, single_raw) == QB_MBF_OVERFLOW)
		return commodity_error(error, "commodity trade maximum MBF32");
	yt_port_mbf64_promote_single(single_raw, promoted_raw);
	if (yt_port_mbf64_compare(promoted_raw,
	    terms->floored_quantity_raw) > 0) {
		if (qb_mbf32_from_mbf64_raw(terms->floored_quantity_raw,
		    single_raw) == QB_MBF_OVERFLOW)
			return commodity_error(error,
			    "commodity trade cached quantity CSNG");
		terms->maximum = qb_mbf32_decode(single_raw);
	}
	if ((double)floorf(yt_port_single_mul(terms->price,
	    terms->maximum)) > terms->credits) {
		double affordable;

		if (terms->price == 0.0f)
			return commodity_error(error,
			    "commodity trade credit/price division");
		affordable = commodity_double_div(terms->credits,
		    (double)terms->price);
		if (!isfinite(affordable) || floor(affordable) > FLT_MAX
		    || floor(affordable) < -FLT_MAX)
			return commodity_error(error,
			    "commodity trade affordable CSNG");
		terms->maximum = (float)floor(affordable);
	}
	return true;
}

static bool
commodity_read_port(struct yt_session *session, uint32_t physical_record,
    struct yt_port *port, struct yt_error *error)
{
	struct yt_record record;

	if (!yt_database_read(&session->door->game.database,
	    (size_t)physical_record, &record, error))
		return false;
	yt_port_decode(port, &record);
	return true;
}

static bool
commodity_write_port(struct yt_session *session, uint32_t physical_record,
    const struct yt_port *port, struct yt_error *error)
{
	return yt_database_write_durable(&session->door->game.database,
	    (size_t)physical_record, &port->record, error);
}

bool
yt_session_trade_commodity(struct yt_session *session,
    const struct yt_port_market_state *market, size_t commodity,
    bool *prompt_reached, struct yt_error *error)
{
	static const char *const names[3] = {"Ore", "Organics", "Equipment"};
	static const uint8_t confirmation[] = "Do you agree? [Y/n] ";
	static const uint8_t never_mind[] = "Never mind!";
	static const uint8_t yours[] = "It's Yours!";
	static const uint8_t take[] = "We'll take them!";
	struct commodity_trade_terms terms;
	struct qb_val_result parsed;
	struct yt_port fresh_port;
	char first[64];
	char second[64];
	char row[256];
	char response[80];
	float quantity;
	float total;
	float direction;
	uint8_t single_raw[4];
	uint8_t promoted_raw[8];
	enum yt_yes_no_answer answer;

	if (session == NULL || market == NULL || commodity >= 3U)
		return false;
	if (prompt_reached != NULL)
		*prompt_reached = false;
	if (!session_reload_player(session, error)
	    || !commodity_prepare(market, &session->player, commodity,
	    &terms, error))
		return false;
	if (terms.maximum == 0.0f)
		return true;
	if (qb_str_double(first, sizeof(first), terms.credits) < 0
	    || qb_str_single(second, sizeof(second), terms.free_holds) < 0
	    || !commodity_row(row, sizeof(row),
	    "You have%s credits and%s empty cargo holds.", first, second,
	    error, "commodity trade status composition")
	    || !session_present_paged_line(session, (const uint8_t *)row,
	    strlen(row), "commodity trade player status", error)
	    || qb_str_mbf64(first, sizeof(first),
	    terms.floored_quantity_raw) < 0
	    || qb_str_double(second, sizeof(second), terms.displayed_hold) < 0
	    || !commodity_row(row, sizeof(row), terms.port_sells
	    ? "We are selling up to%s.  You have%s in your holds."
	    : "We are buying up to%s.  You have%s in your holds.",
	    first, second, error, "commodity trade market composition")
	    || !session_present_paged_line(session, (const uint8_t *)row,
	    strlen(row), "commodity trade market status", error))
		return false;
	for (;;) {
		if (qb_str_single(first, sizeof(first), terms.maximum) < 0
		    || snprintf(row, sizeof(row),
		    "How many holds of %s do you want to %s [%s ]? ",
		    names[commodity], terms.port_sells ? "buy" : "sell",
		    first) < 0)
			return commodity_error(error,
			    "commodity trade quantity prompt composition");
		if (prompt_reached != NULL)
			*prompt_reached = true;
		if (!session_present_timed_paged_row(session,
		    (const uint8_t *)row, strlen(row),
		    "commodity trade quantity prompt", error)
		    || !session_read_upper_command(session, response,
		    sizeof(response)))
			return false;
		if (strlen(response) > 4U)
			continue;
		if (response[0] == '\0')
			quantity = terms.maximum;
		else {
			parsed = qb_val(response);
			if (parsed.overflow)
				return commodity_error(error,
				    "commodity trade VAL");
			quantity = parsed.valid ? (float)floor(parsed.value) : 0.0f;
		}
		if (quantity < 1.0f)
			return true;
		if (qb_mbf32_encode(quantity, single_raw) == QB_MBF_OVERFLOW)
			return commodity_error(error,
			    "commodity trade quantity MBF32");
		yt_port_mbf64_promote_single(single_raw, promoted_raw);
		if (yt_port_mbf64_compare(promoted_raw,
		    terms.selected_quantity_raw) > 0) {
			const char *message = terms.port_sells
			    ? "We don't have that much!"
			    : "We don't need that much!";

			return session_present_alert(session,
			    (const uint8_t *)message, strlen(message),
			    "commodity trade capacity rejection", error);
		}
		if (terms.port_sells && quantity > terms.free_holds) {
			static const uint8_t message[] =
			    "You don't have enough cargo holds.";

			if (!session_present_alert(session, message,
			    sizeof(message) - 1U,
			    "commodity trade free-holds rejection", error)
			    || !session_present_text(session, NULL, 0U,
			    SESSION_PRESENT_LINE,
			    "commodity trade free-holds retry blank", error))
				return false;
			continue;
		}
		if (quantity > terms.maximum) {
			const char *message = terms.port_sells
			    ? "You can't afford that much!"
			    : "You don't have that much!";

			return session_present_alert(session,
			    (const uint8_t *)message, strlen(message),
			    "commodity trade maximum rejection", error);
		}
		if (terms.port_sells
		    && yt_port_mbf64_compare(promoted_raw,
		    terms.floored_quantity_raw) > 0) {
			static const uint8_t message[] =
			    "We're not selling that many.";

			if (!session_present_paged_fragment(session, message,
			    sizeof(message) - 1U))
				return false;
			continue;
		}
		if (!terms.port_sells
		    && yt_port_mbf64_compare(promoted_raw,
		    terms.floored_quantity_raw) > 0) {
			static const uint8_t message[] =
			    "We don't want that many.";

			if (!session_present_alert(session, message,
			    sizeof(message) - 1U,
			    "commodity trade buying retry", error))
				return false;
			continue;
		}
		if (!terms.port_sells
		    && (double)quantity > terms.displayed_hold) {
			static const uint8_t message[] =
			    "You don't have that much!";

			if (!session_present_alert(session, message,
			    sizeof(message) - 1U,
			    "commodity trade hold retry", error))
				return false;
			continue;
		}
		break;
	}
	total = floorf(yt_port_single_add(
	    yt_port_single_mul(terms.price, quantity), 0.5f));
	if (qb_str_single(first, sizeof(first), quantity) < 0
	    || snprintf(row, sizeof(row), "Agreed,%s units.", first) < 0
	    || !session_present_paged_fragment(session, (const uint8_t *)row,
	    strlen(row))
	    || qb_str_single(first, sizeof(first), total) < 0
	    || snprintf(row, sizeof(row), "We'll %s them for%s credits.",
	    terms.port_sells ? "sell" : "buy", first) < 0
	    || !session_present_paged_line(session, (const uint8_t *)row,
	    strlen(row), "commodity trade offer row", error)
	    || !session_confirm(session, confirmation,
	    sizeof(confirmation) - 1U, &answer, error))
		return false;
	if (answer == YT_YES_NO_NO)
		return session_present_paged_fragment(session, never_mind,
		    sizeof(never_mind) - 1U);
	if (!session_present_paged_fragment(session,
	    terms.port_sells ? yours : take,
	    terms.port_sells ? sizeof(yours) - 1U : sizeof(take) - 1U))
		return false;
	if (terms.port_sells && market->port.owner != 0.0f) {
		float receipt = total;

		if (market->port.owner == (float)session_record(session))
			receipt = floorf(yt_port_single_mul(
			    0.009999999776482582f, total));
		if (!commodity_read_port(session, market->port_physical_record,
		    &fresh_port, error))
			return false;
		yt_trade_treasury_overlay(&fresh_port, receipt);
		if (!commodity_write_port(session, market->port_physical_record,
		    &fresh_port, error))
			return false;
	}
	direction = terms.factor > 0.0f ? 1.0f
	    : terms.factor < 0.0f ? -1.0f : 0.0f;
	if (!session_mutate_player_credits(session,
	    -yt_port_single_mul(total, direction), NULL, error)
	    || !session_reload_player(session, error))
		return false;
	yt_trade_holds_overlay(&session->player, commodity, quantity, direction);
	if (!yt_database_write_durable(&session->door->game.database,
	    (size_t)session_record(session), &session->player.record, error)
	    || !commodity_read_port(session, market->port_physical_record,
	    &fresh_port, error))
		return false;
	if (qb_mbf32_encode(quantity, single_raw) == QB_MBF_OVERFLOW)
		return commodity_error(error,
		    "commodity trade stock quantity MBF32");
	yt_port_mbf64_promote_single(single_raw, promoted_raw);
	yt_port_mbf64_negate(promoted_raw);
	if (qb_mbf64_add_raw(terms.selected_quantity_raw, promoted_raw,
	    promoted_raw) != QB_MBF_OK
	    || qb_mbf32_from_mbf64_raw(promoted_raw, single_raw)
	    == QB_MBF_OVERFLOW
	    || !yt_record_set_raw_number(&fresh_port.record,
	    YT_F49 + commodity * 4U, single_raw))
		return commodity_error(error, "commodity trade stock overlay");
	fresh_port.stock[commodity] = qb_mbf32_decode(single_raw);
	return commodity_write_port(session, market->port_physical_record,
	    &fresh_port, error);
}

static bool
treasury_error(struct yt_error *error, const char *operation)
{
	if (error != NULL) {
		error->status = YT_RANGE;
		error->system_error = 0;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
		error->path[0] = '\0';
	}
	return false;
}

static bool
treasury_format_single(const char *prefix, float value, char *text,
    size_t capacity, struct yt_error *error, const char *operation)
{
	char number[64];
	int number_length = qb_str_single(number, sizeof(number), value);
	int result;

	if (number_length < 0)
		return treasury_error(error, operation);
	result = snprintf(text, capacity, "%s%s", prefix, number);
	if (result < 0 || (size_t)result >= capacity)
		return treasury_error(error, operation);
	return true;
}

static bool
treasury_format_double(const char *prefix, const uint8_t raw[8],
    const char *suffix, char *text, size_t capacity,
    struct yt_error *error, const char *operation)
{
	char number[96];
	int number_length = qb_str_mbf64(number, sizeof(number), raw);
	int result;

	if (number_length < 0)
		return treasury_error(error, operation);
	result = snprintf(text, capacity, "%s%s%s", prefix, number,
	    suffix == NULL ? "" : suffix);
	if (result < 0 || (size_t)result >= capacity)
		return treasury_error(error, operation);
	return true;
}

static bool
treasury_add(uint8_t total[8], const uint8_t value[4],
    struct yt_error *error)
{
	uint8_t promoted[8];
	uint8_t sum[8];

	yt_port_mbf64_promote_single(value, promoted);
	if (qb_mbf64_add_raw(total, promoted, sum) != QB_MBF_OK)
		return treasury_error(error, "treasury MBF56 accumulation");
	memcpy(total, sum, sizeof(sum));
	return true;
}

static bool
treasury_update_player(struct yt_player *player, float owned,
    const uint8_t total[8], struct yt_error *error)
{
	uint8_t fresh_credits[8];
	uint8_t summed_credits[8];
	uint8_t stored_credits[4];
	uint8_t stored_owned[4];

	yt_port_mbf64_promote_single(player->record.bytes + YT_F81,
	    fresh_credits);
	if (qb_mbf64_add_raw(fresh_credits, total, summed_credits) != QB_MBF_OK
	    || qb_mbf32_from_mbf64_raw(summed_credits, stored_credits)
	    == QB_MBF_OVERFLOW
	    || qb_mbf32_encode(owned, stored_owned) == QB_MBF_OVERFLOW
	    || !yt_record_set_raw_number(&player->record, YT_F117,
	    stored_owned)
	    || !yt_record_set_raw_number(&player->record, YT_F81,
	    stored_credits))
		return treasury_error(error, "treasury player overlay");
	player->ports_owned = qb_mbf32_decode(stored_owned);
	player->credits = qb_mbf32_decode(stored_credits);
	return true;
}

bool
yt_session_treasury(struct yt_session *session, bool collecting,
    struct yt_error *error)
{
	static const uint8_t dirty_zero[4] = {0x00, 0x00, 0x20, 0x00};
	static const uint8_t no_ports[] = "You don't OWN any ports!!!";
	static const uint8_t collect_prefix[] =
	    "Sending out armored cargo ships to";
	static const uint8_t report_prefix[] =
	    "Checking galactic bank statement for";
	static const uint8_t heading_suffix[] = " ports with credits...";
	struct yt_player player;
	struct yt_port port;
	struct yt_record record;
	uint8_t total[8] = {0};
	char text[192];
	float loop_bound;
	float counter;
	float owned = 0.0f;
	float credited = 0.0f;
	float barren;
	uint32_t player_record;

	if (session == NULL)
		return false;
	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "treasury opening blank", error))
		return false;
	player_record = (uint32_t)session_record(session);
	if (player_record == 0U)
		return treasury_error(error, "treasury player record conversion");
	if (!yt_database_read(&session->door->game.database,
	    (size_t)player_record, &record, error))
		return false;
	yt_player_decode(&player, &record);
	if (player.ports_owned < 1.0f) {
		yt_present_set_blink(&session->presentation, 1.0f);
		return session_present_text(session, no_ports,
		    sizeof(no_ports) - 1U, SESSION_PRESENT_BOLD_LINE,
		    "treasury no-owned notice", error);
	}
	if (!session_present_text(session,
	    collecting ? collect_prefix : report_prefix,
	    collecting ? sizeof(collect_prefix) - 1U
	    : sizeof(report_prefix) - 1U,
	    SESSION_PRESENT_RAW, "treasury heading prefix", error)
	    || !session_present_text(session, heading_suffix,
	    sizeof(heading_suffix) - 1U, SESSION_PRESENT_LINE,
	    "treasury heading suffix", error)
	    || !session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "treasury scan blank", error))
		return false;
	loop_bound = yt_port_single_sub(session_planet_offset(session),
	    session_port_offset(session));
	for (counter = 1.0f; counter <= loop_bound;
	    counter = yt_port_single_add(counter, 1.0f)) {
		float expression = yt_port_single_add(session_port_offset(session),
		    counter);
		uint32_t physical_record = qb_brun_random_record_number(expression);

		if (physical_record == 0U)
			return treasury_error(error,
			    "treasury port record conversion");
		if (!yt_database_read(&session->door->game.database,
		    (size_t)physical_record, &record, error))
			return false;
		yt_port_decode(&port, &record);
		if (port.owner != (float)session_record(session))
			continue;
		owned = yt_port_single_add(owned, 1.0f);
		if (!qb_mbf32_truth(port.record.bytes + YT_F89))
			continue;
		credited = yt_port_single_add(credited, 1.0f);
		if (!treasury_add(total, port.record.bytes + YT_F89, error)
		    || !treasury_format_single("Sector:", port.sector, text,
		    sizeof(text), error, "treasury sector field")
		    || !session_fixed_width_bytes(session, (const uint8_t *)text,
		    strlen(text), 14.0f, "treasury sector field", error))
			return false;
		{
			bool overflow = false;
			int32_t converted = qb_cint_mbf32(
			    port.record.bytes + YT_F85,
			    session->presentation.sound.conversion_mode, &overflow);
			size_t name_length;

			if (overflow || converted < 0)
				return treasury_error(error,
				    "treasury port-name length");
			name_length = (size_t)converted;
			if (name_length > YT_TEXT_FIELD_SIZE)
				name_length = YT_TEXT_FIELD_SIZE;
			if (!session_fixed_width_bytes(session, port.record.bytes,
			    name_length, 25.0f, "treasury port-name field", error))
				return false;
		}
		if (!treasury_format_single(" Credits:", port.treasury, text,
		    sizeof(text), error, "treasury credit field")
		    || !session_fixed_width_bytes(session, (const uint8_t *)text,
		    strlen(text), 20.0f, "treasury credit field", error)
		    || !treasury_format_double(" Total:", total, NULL, text,
		    sizeof(text), error, "treasury row total")
		    || !session_present_text(session, (const uint8_t *)text,
		    strlen(text), SESSION_PRESENT_LINE, "treasury row total",
		    error))
			return false;
		if (collecting) {
			port.treasury = 0.0f;
			if (!yt_record_set_raw_number(&port.record, YT_F89,
			    dirty_zero)
			    || !yt_database_write(&session->door->game.database,
			    (size_t)physical_record, &port.record, error))
				return false;
		}
	}
	if (total[7] != 0U
	    && !session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "treasury nonzero-total blank", error))
		return false;
	if (!treasury_format_single("Total ports...:", owned, text,
	    sizeof(text), error, "treasury total ports")
	    || !session_present_text(session, (const uint8_t *)text,
	    strlen(text), SESSION_PRESENT_LINE, "treasury total ports", error)
	    || !treasury_format_single("With credits..:", credited, text,
	    sizeof(text), error, "treasury credited ports")
	    || !session_present_text(session, (const uint8_t *)text,
	    strlen(text), SESSION_PRESENT_LINE, "treasury credited ports",
	    error))
		return false;
	barren = yt_port_single_sub(owned, credited);
	if (!treasury_format_single("Barren ports..:", barren, text,
	    sizeof(text), error, "treasury barren ports")
	    || !session_present_text(session, (const uint8_t *)text,
	    strlen(text), SESSION_PRESENT_LINE, "treasury barren ports", error)
	    || !treasury_format_double("Total credits.:", total, NULL, text,
	    sizeof(text), error, "treasury total credits")
	    || !session_present_text(session, (const uint8_t *)text,
	    strlen(text), SESSION_PRESENT_LINE, "treasury total credits", error)
	    || !session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "treasury summary blank", error))
		return false;
	if (!collecting) {
		if (!treasury_format_double("You have", total,
		    " credits in your port accounts.", text, sizeof(text), error,
		    "treasury report result"))
			return false;
		return session_present_text(session, (const uint8_t *)text,
		    strlen(text), SESSION_PRESENT_LINE,
		    "treasury report result", error);
	}
	if (!treasury_format_double("You collected a total of", total,
	    " credits.", text, sizeof(text), error,
	    "treasury collection result")
	    || !session_present_text(session, (const uint8_t *)text,
	    strlen(text), SESSION_PRESENT_LINE,
	    "treasury collection result", error)
	    || !yt_database_read(&session->door->game.database,
	    (size_t)player_record, &record, error))
		return false;
	yt_player_decode(&player, &record);
	if (!treasury_update_player(&player, owned, total, error)
	    || !yt_database_write(&session->door->game.database,
	    (size_t)player_record, &player.record, error)
	    || !yt_database_flush(&session->door->game.database, error))
		return false;
	session->player = player;
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

bool
yt_session_command_trade(struct yt_session *session, bool *enter_sector,
    struct yt_error *error)
{
	static const uint8_t label[] = "<Port>";
	static const uint8_t no_port[] = "No port here!";
	static const uint8_t docking[] = "Docking, ";
	struct yt_sector sector;
	struct yt_record record;
	float selected_port_expression;
	uint32_t sector_physical_record;
	uint32_t port_physical_record;
	uint8_t selected_raw[4];
	bool denied;

	if (session == NULL)
		return false;
	if (enter_sector != NULL)
		*enter_sector = false;
	if (!session_present_paged_fragment(session, label, sizeof(label) - 1U))
		return false;
	session_set_foreground(session, 3.0f);
	if (!yt_session_fresh_no_turn_gate(session, &denied, error))
		return false;
	if (denied) {
		if (enter_sector != NULL)
			*enter_sector = true;
		return true;
	}
	sector_physical_record = qb_brun_random_record_number(
	    session->current_sector_record);
	if (sector_physical_record == 0U)
		return port_update_error(error,
		    "port docking sector record conversion");
	if (!yt_database_read(&session->door->game.database,
	    (size_t)sector_physical_record, &record, error))
		return false;
	yt_sector_decode(&sector, &record);
	selected_port_expression = yt_port_selected_expression(
	    session_port_offset(session), sector.port);
	{
		enum qb_mbf_status status = qb_mbf32_encode(
		    selected_port_expression, selected_raw);

		if (status == QB_MBF_OVERFLOW)
			return port_update_error(error,
			    "port docking selected expression add");
		selected_port_expression = status == QB_MBF_UNDERFLOW
		    ? 0.0f : qb_mbf32_decode(selected_raw);
	}
	if (yt_port_link_missing(sector.port)) {
		if (!session_present_alert(session, no_port, sizeof(no_port) - 1U,
		    "port docking no port", error))
			return false;
		if (enter_sector != NULL)
			*enter_sector = true;
		return true;
	}
	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "port docking leading blank", error)
	    || !session_present_timed_paged_row(session, docking,
	    sizeof(docking) - 1U, "port docking prelude", error))
		return false;
	if (!yt_session_finalize_action(session, 1.0f, error)) {
		if (error == NULL || error->status == YT_OK)
			return true;
		return false;
	}
	port_physical_record = qb_brun_random_record_number(
	    selected_port_expression);
	if (port_physical_record == 0U)
		return port_update_error(error,
		    "port docking selected record conversion");
	if (!yt_database_read(&session->door->game.database,
	    (size_t)port_physical_record, &record, error))
		return false;
	if (session->player.sector == 1.0f)
		return yt_session_earth_store(session, enter_sector, error);
	if (!yt_session_ordinary_commerce(session, (int)session->player.sector,
	    session->current_sector_record, error))
		return false;
	if (enter_sector != NULL)
		*enter_sector = true;
	return true;
}
