#include "yt_session_internal.h"

#include "qb.h"
#include "yt_port_math.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

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
	free_holds = qb_double_subtract((double)player->holds,
	    (double)player->ore);
	free_holds = qb_double_subtract(free_holds,
	    (double)player->organics);
	free_holds = qb_double_subtract(free_holds,
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
	if ((double)floorf(qb_single_multiply(terms->price,
	    terms->maximum)) > terms->credits) {
		double affordable;

		if (terms->price == 0.0f)
			return commodity_error(error,
			    "commodity trade credit/price division");
		affordable = qb_double_divide(terms->credits,
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
	total = floorf(qb_single_add(
	    qb_single_multiply(terms.price, quantity), 0.5f));
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
			receipt = floorf(qb_single_multiply(
			    0.009999999776482582f, total));
		if (!session_read_port_physical(session, market->port_physical_record,
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
	    -qb_single_multiply(total, direction), NULL, error)
	    || !session_reload_player(session, error))
		return false;
	yt_trade_holds_overlay(&session->player, commodity, quantity, direction);
	if (!yt_database_write_durable(&session->door->game.database,
	    (size_t)session_record(session), &session->player.record, error)
	    || !session_read_port_physical(session, market->port_physical_record,
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
		if (!session_buffer_append(row, sizeof(row), &length, refusal_prefix,
		    sizeof(refusal_prefix) - 1U)
		    || !session_buffer_append(row, sizeof(row), &length, first_name,
		    first_name_length)
		    || !session_buffer_append(row, sizeof(row), &length, refusal_suffix,
		    sizeof(refusal_suffix) - 1U))
			return session_range_error(error,
			    "ordinary commerce refusal composition");
		if (!session_present_alert(session, row, length,
		    "port docking refusal", error))
			return false;
	}
	if (!session_reload_player(session, error))
		return false;
	free = qb_double_subtract((double)session->player.holds,
	    (double)session->player.ore);
	free = qb_double_subtract(free, (double)session->player.organics);
	free = qb_double_subtract(free, (double)session->player.equipment);
	if (qb_str_double(credits, sizeof(credits),
	    (double)session->player.credits) < 0
	    || qb_str_double(free_holds, sizeof(free_holds), free) < 0)
		return session_range_error(error,
		    "ordinary commerce status formatting");
	{
		int length = snprintf((char *)row, sizeof(row),
		    "You have%s credits and%s empty cargo holds.",
		    credits, free_holds);

		if (length < 0 || (size_t)length >= sizeof(row))
			return session_range_error(error,
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
		return session_range_error(error,
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
			return session_range_error(error,
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
	if (!yt_session_finalize_action(session, error)) {
		if (error == NULL || error->status == YT_OK)
			return true;
		return false;
	}
	port_physical_record = qb_brun_random_record_number(
	    selected_port_expression);
	if (port_physical_record == 0U)
		return session_range_error(error,
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
