#include "yt_session_internal.h"

#include "qb.h"
#include "yt_platform.h"

#include <stdio.h>
#include <string.h>

bool
session_port_owner_row_capture(struct yt_session *session,
    const struct yt_port *port,
    uint8_t *captured_name, size_t captured_capacity,
    size_t *captured_length, struct yt_error *error)
{
	enum yt_port_owner_kind kind;
	const uint8_t *owner_name = NULL;
	size_t owner_name_length = 0U;
	uint8_t row[256];
	size_t length;
	int owner_record;

	if (captured_length != NULL)
		*captured_length = 0U;
	kind = yt_port_owner_classify(port->owner, session_record(session),
	    &owner_record);
	if (kind == YT_PORT_OWNER_INVALID)
		return session_range_error(error,
		    "port owner record conversion");
	if (kind == YT_PORT_OWNER_SILENT)
		return true;
	if (kind == YT_PORT_OWNER_OTHER) {
		struct yt_player owner;

		if (!yt_game_read_player(&session->door->game, owner_record,
		    &owner, error))
			return false;
		owner_name_length = owner.name_length;
		if (owner_name_length > YT_TEXT_FIELD_SIZE)
			owner_name_length = YT_TEXT_FIELD_SIZE;
		owner_name = owner.record.bytes;
		if (captured_length != NULL) {
			if (owner_name_length > captured_capacity
			    || (owner_name_length != 0U && captured_name == NULL))
				return session_range_error(error,
				    "port owner captured name");
			if (owner_name_length != 0U)
				memcpy(captured_name, owner_name, owner_name_length);
			*captured_length = owner_name_length;
		}
	}
	if (!yt_port_owner_compose(kind, port->treasury, owner_name,
	    owner_name_length, row, sizeof(row), &length))
		return session_range_error(error, "port owner row composition");
	return session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "port owner leading blank", error)
	    && session_present_text(session, row, length, SESSION_PRESENT_LINE,
	    "port owner row", error);
}

bool
session_earth_receipt(struct yt_session *session, const struct yt_port *cached_earth,
    float cost,
    struct yt_error *error)
{
	struct yt_port earth;

	if (!session_mutate_player_credits(session, -cost, NULL, error))
		return false;
	if (cached_earth->owner != 0.0f) {
		float receipt = yt_earth_receipt_amount(cached_earth->owner,
		    session_record(session), cost);

		if (!session_read_port(session, 1, &earth, error))
			return false;
		earth.treasury = qb_single_add(earth.treasury, receipt);
		if (!session_write_port(session, 1, &earth, error))
			return false;
	}
	return true;
}

bool
yt_session_clearance(struct yt_session *session, bool create,
    struct yt_error *error)
{
	static const char *const name[4] = {
		"Holds", "Fighters", "Shields", "Ground Forces"
	};
	bool announced = false;
	size_t index;

	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "clearance leading blank", error))
		return false;
	for (index = 0U; index < YT_ARRAY_LEN(session->earth.clearance_discounts);
	    ++index) {
		float discount = session->earth.clearance_discounts[index];
		float draw;
		char percent[64];
		char row[192];
		int row_length;

		if (!yt_random_next(&session->door->game.random, &draw, error))
			return false;
		if (yt_clearance_candidate_needed(index, draw, discount, create)) {
			if (!yt_random_next(&session->door->game.random, &draw, error))
				return false;
			discount = draw;
			session->earth.clearance_discounts[index] = discount;
		}
		if (!yt_clearance_normalize(index, &discount)) {
			session->earth.clearance_discounts[index] = 0.0f;
			continue;
		}
		session->earth.clearance_discounts[index] = discount;
		if (qb_str_single(percent, sizeof(percent),
		    yt_clearance_percentage(discount)) < 0)
			return false;
		row_length = snprintf(row, sizeof(row),
		    "Special clearance sale! The Trader's Guild is selling "
		    "%s for%s%% off!", name[index], percent);
		if (row_length < 0 || (size_t)row_length >= sizeof(row)
		    || !session_present_text(session, (const uint8_t *)row,
		    (size_t)row_length, SESSION_PRESENT_LINE,
		    "clearance announcement", error))
			return false;
		announced = true;
	}
	if (!announced)
		return true;
	return session_sound(session, YT_SOUND_CUE_REWARD, "clearance sale sound", error)
	    && session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "clearance trailing blank", error);
}

static bool
earth_report_row(struct yt_session *session, const char *label, float price,
    bool lottery_price, struct yt_error *error)
{
	char price_text[64];
	char cost[96];
	char affordable_text[80];
	char tail[96];
	double affordable;

	if (!session_fixed_width_bytes(session, (const uint8_t *)label,
	    strlen(label), 22.0f,
	    "Earth report item field", error))
		return false;
	if (lottery_price)
		snprintf(cost, sizeof(cost), "%s", "* 5");
	else {
		if (qb_str_single(price_text, sizeof(price_text), price) < 0
		    || snprintf(cost, sizeof(cost), "*%s ", price_text) < 0)
			return session_range_error(error,
			    "Earth report price format");
	}
	if (!session_fixed_width_bytes(session, (const uint8_t *)cost,
	    strlen(cost), 9.0f,
	    "Earth report cost field", error))
		return false;
	affordable = yt_earth_affordable(session->player.credits, price);
	if (qb_str_double(affordable_text, sizeof(affordable_text),
	    affordable) < 0
	    || snprintf(tail, sizeof(tail), "*%s", affordable_text) < 0)
		return session_range_error(error,
		    "Earth report affordability format");
	return session_present_paged_row(session, (const uint8_t *)tail, strlen(tail));
}

bool
session_earth_report(struct yt_session *session, struct yt_port *earth,
    float price[4], struct yt_error *error)
{
	static const uint8_t separator[] =
	    "----------------------*--------*------------";
	static const uint8_t header[] =
	    "         ITEM         *  COST  * CAN AFFORD";
	static const char *const item_label[9] = {
		"[1] Cloak Energy", "[2] Cargo Holds", "[3] Fighters",
		"[4] Play Lottery", "[5] Danger Scanner",
		"[6] Anti-Cloak Device", "[7] Ground Forces",
		"[8] Shield Power", "[9] Hire Spies (Each)"
	};
	struct yt_clock_value date_now;
	struct yt_clock_value time_now;
	char date[11];
	char time_text[9];
	char title[128];
	float discount[4];
	size_t index;

	if (earth == NULL || price == NULL)
		return false;
	session_set_pager_line_count(session, 0.0f);
	if (!session_read_port_at_fault(session, 1, earth,
	    YT_BASIC_FAULT_PORT_EARTH_GET, error))
		return false;
	session_set_foreground(session, 3.0f);
	if (!yt_clock_read(&session->door->game.clock, &date_now, error)
	    || !yt_clock_read(&session->door->game.clock, &time_now, error))
		return false;
	yt_format_date(&date_now, date);
	yt_format_time(&time_now, time_text);
	if (snprintf(title, sizeof(title),
	    "Commerce report for Earth: %s %s", date, time_text) < 0
	    || !session_present_paged_line(session, (const uint8_t *)title,
	    strlen(title), "Earth report title", error)
	    || !session_port_owner_row_capture(session, earth, NULL, 0U, NULL,
	    error))
		return false;
	memcpy(discount, session->earth.clearance_discounts, sizeof(discount));
	yt_earth_prices(discount, price);
	if (!session->earth.report_seen) {
		if (!yt_session_clearance(session, false, error))
			return false;
	}
	else if (!session_present_text(session, NULL, 0,
	    SESSION_PRESENT_LINE, "Earth report ordinary blank", error))
		return false;
	session->earth.report_seen = true;
	if (!session_reload_player(session, error))
		return false;
	if (!session_present_paged_row(session, separator, sizeof(separator) - 1U)
	    || !session_present_paged_row(session, header, sizeof(header) - 1U)
	    || !session_present_paged_row(session, separator, sizeof(separator) - 1U))
		return false;
	for (index = 0; index < 9U; ++index) {
		float item_price;
		bool lottery_price = index == 3U;

		if (index == 0U)
			item_price = 1000.0f;
		else if (index == 1U)
			item_price = price[0];
		else if (index == 2U)
			item_price = price[1];
		else if (index == 3U)
			item_price = 5.0f;
		else if (index == 4U)
			item_price = 500000.0f;
		else if (index == 5U || index == 8U)
			item_price = 1000000000.0f;
		else if (index == 6U)
			item_price = price[3];
		else
			item_price = price[2];
		if (!earth_report_row(session, item_label[index], item_price,
		    lottery_price, error))
			return false;
	}
	return session_present_paged_row(session, separator, sizeof(separator) - 1U);
}
