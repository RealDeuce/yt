#include "yt_game.h"
#include "yt_game_internal.h"
#include "yt_port_math.h"

#include "qb.h"

#include <string.h>

static bool
market_encode_single(float value, uint8_t raw[4], struct yt_error *error,
    const char *operation)
{
	enum qb_mbf_status status = qb_mbf32_encode(value, raw);

	if (status == QB_MBF_OK || status == QB_MBF_UNDERFLOW)
		return true;
	return yt_game_error(error, YT_RANGE, operation);
}

typedef enum qb_mbf_status (*market_binary_fn)(const uint8_t left[8],
    const uint8_t right[8], uint8_t result[8]);

static bool
market_binary(market_binary_fn operation, const uint8_t left[8],
    const uint8_t right[8], uint8_t result[8], struct yt_error *error,
    const char *label)
{
	enum qb_mbf_status status = operation(left, right, result);

	if (status == QB_MBF_OK || status == QB_MBF_UNDERFLOW)
		return true;
	return yt_game_error(error, YT_RANGE, label);
}

bool
yt_port_market_update(struct yt_port_market_state *state,
    struct yt_error *error)
{
	uint8_t ten[8];
	uint8_t thousand[8];
	uint8_t one[8];
	uint8_t half[8];
	uint8_t zero[8] = {0};
	uint8_t current_day_raw[4];
	uint8_t current_minute_raw[4];
	uint8_t mutable_capacity[3][8];
	uint8_t mutable_production[3][4];
	uint8_t mutable_price[3][4];
	struct yt_record updated;
	bool raised[3] = {false, false, false};
	float minute;
	float elapsed;
	size_t index;

	if (state == NULL)
		return false;
	state->current_minute = 0.0f;
	state->elapsed = 0.0f;
	memset(state->capacity_raw, 0, sizeof(state->capacity_raw));
	memset(state->capacity, 0, sizeof(state->capacity));
	memset(state->production_raw, 0, sizeof(state->production_raw));
	memset(state->price_raw, 0, sizeof(state->price_raw));
	memset(state->price, 0, sizeof(state->price));
	memset(state->production_raised, 0,
	    sizeof(state->production_raised));
	state->completed_items = 0U;
	state->complete = false;

	if (qb_mbf64_from_u64(10U, ten) != QB_MBF_OK
	    || qb_mbf64_from_u64(1000U, thousand) != QB_MBF_OK
	    || qb_mbf64_from_u64(1U, one) != QB_MBF_OK
	    || qb_mbf64_encode(0.5, half) != QB_MBF_OK)
		return yt_game_error(error, YT_RANGE,
		    "ordinary port constants");
	minute = qb_single_divide(state->timer_seconds, 60.0f);
	elapsed = qb_single_add(
	    qb_single_subtract(state->current_day, state->port.last_day),
	    qb_single_divide(qb_single_subtract(minute,
	    state->port.last_minute), 1440.0f));
	if (elapsed > 10.0f || elapsed < 0.0f)
		elapsed = 10.0f;
	if (!market_encode_single(state->current_day, current_day_raw, error,
	    "ordinary port current day")
	    || !market_encode_single(minute, current_minute_raw, error,
	    "ordinary port current minute"))
		return false;

	for (index = 0U; index < 3U; ++index) {
		uint8_t growth_raw[4];
		uint8_t growth[8];
		uint8_t quotient[8];
		uint8_t promoted_production[8];
		uint8_t base_raw[4];
		uint8_t base[8];
		uint8_t factor[8];
		uint8_t numerator[8];
		uint8_t denominator[8];
		uint8_t ratio[8];
		uint8_t scale[8];
		uint8_t raw_price[8];
		uint8_t rounded_source[8];
		uint8_t rounded[8];
		float growth_value;

		yt_port_mbf64_promote_single(state->port.record.bytes
		    + YT_F49 + index * 4U, mutable_capacity[index]);
		memcpy(mutable_production[index], state->port.record.bytes
		    + YT_F61 + index * 4U, 4U);
		growth_value = qb_single_multiply(
		    qb_mbf32_decode(mutable_production[index]), elapsed);
		if (!market_encode_single(growth_value, growth_raw, error,
		    "ordinary port growth"))
			return false;
		yt_port_mbf64_promote_single(growth_raw, growth);
		if (!market_binary(qb_mbf64_add_raw, mutable_capacity[index],
		    growth, mutable_capacity[index], error,
		    "ordinary port capacity")
		    || !market_binary(qb_mbf64_div_raw, mutable_capacity[index],
		    ten, quotient, error, "ordinary port production comparison"))
			return false;
		yt_port_mbf64_promote_single(mutable_production[index],
		    promoted_production);
		if (yt_port_mbf64_compare(quotient, promoted_production) > 0) {
			if (!market_binary(qb_mbf64_div_raw,
			    mutable_capacity[index], ten, quotient, error,
			    "ordinary port production replacement")
			    || qb_mbf32_from_mbf64_raw(quotient,
			    mutable_production[index]) == QB_MBF_OVERFLOW)
				return yt_game_error(error, YT_RANGE,
				    "ordinary port production CSNG");
			raised[index] = true;
			yt_port_mbf64_promote_single(mutable_production[index],
			    promoted_production);
		}
		if (!market_encode_single(state->base_price[index], base_raw,
		    error, "ordinary port base price"))
			return false;
		yt_port_mbf64_promote_single(base_raw, base);
		yt_port_mbf64_promote_single(state->port.record.bytes
		    + YT_F73 + index * 4U, factor);
		if (!market_binary(qb_mbf64_mul_raw, factor,
		    mutable_capacity[index], numerator, error,
		    "ordinary port price numerator")
		    || !market_binary(qb_mbf64_mul_raw, promoted_production,
		    thousand, denominator, error,
		    "ordinary port price denominator")
		    || !market_binary(qb_mbf64_div_raw, numerator, denominator,
		    ratio, error, "ordinary port price division"))
			return false;
		memcpy(scale, ratio, 8U);
		yt_port_mbf64_negate(scale);
		if (!market_binary(qb_mbf64_add_raw, one, scale, scale, error,
		    "ordinary port price scale")
		    || !market_binary(qb_mbf64_mul_raw, base, scale, raw_price,
		    error, "ordinary port raw price")
		    || !market_binary(qb_mbf64_add_raw, raw_price, half,
		    rounded_source, error, "ordinary port price rounding"))
			return false;
		if (yt_port_mbf64_compare(rounded_source, zero) <= 0)
			memset(rounded, 0, sizeof(rounded));
		else {
			enum qb_mbf_status status = qb_mbf64_floor_positive_raw(
			    rounded_source, rounded);

			if (status != QB_MBF_OK && status != QB_MBF_UNDERFLOW)
				return yt_game_error(error, YT_RANGE,
				    "ordinary port price INT");
		}
		if (qb_mbf32_from_mbf64_raw(rounded, mutable_price[index])
		    == QB_MBF_OVERFLOW)
			return yt_game_error(error, YT_RANGE,
			    "ordinary port price CSNG");
		if (qb_mbf32_decode(mutable_price[index]) < 1.0f
		    && !market_encode_single(1.0f, mutable_price[index], error,
		    "ordinary port price floor"))
			return false;
		++state->completed_items;
	}

	updated = state->port.record;
	if (!yt_record_set_raw_number(&updated, YT_F45,
	    current_day_raw)
	    || !yt_record_set_raw_number(&updated, YT_F101,
	    current_minute_raw))
		return false;
	for (index = 0U; index < 3U; ++index) {
		uint8_t stored_capacity[4];

		if (qb_mbf32_from_mbf64_raw(mutable_capacity[index],
		    stored_capacity) == QB_MBF_OVERFLOW
		    || !yt_record_set_raw_number(&updated,
		    YT_F49 + index * 4U, stored_capacity)
		    || !yt_record_set_raw_number(&updated,
		    YT_F61 + index * 4U, mutable_production[index]))
			return yt_game_error(error, YT_RANGE,
			    "ordinary port FIELD overlay");
		memcpy(state->capacity_raw[index], mutable_capacity[index], 8U);
		state->capacity[index] = qb_mbf64_decode(mutable_capacity[index]);
		memcpy(state->production_raw[index], mutable_production[index], 4U);
		memcpy(state->price_raw[index], mutable_price[index], 4U);
		state->price[index] = qb_mbf32_decode(mutable_price[index]);
		state->production_raised[index] = raised[index];
	}
	state->current_minute = minute;
	state->elapsed = elapsed;
	yt_port_decode(&state->port, &updated);
	state->complete = true;
	return true;
}

static bool
port_report_append(uint8_t *row, size_t capacity, size_t *position,
    const void *text, size_t length)
{
	if (*position > capacity || length > capacity - *position
	    || (length != 0U && text == NULL))
		return false;
	if (length != 0U)
		memcpy(row + *position, text, length);
	*position += length;
	return true;
}

static bool
port_report_right_raw(const uint8_t *source, size_t source_length,
    size_t width, uint8_t *rendered)
{
	size_t amount = source_length < width ? source_length : width;
	size_t padding = width - amount;

	if (source == NULL || rendered == NULL)
		return false;
	memset(rendered, ' ', padding);
	memcpy(rendered + padding, source + source_length - amount, amount);
	return true;
}

bool
yt_port_report_compose(const struct yt_port_market_state *market,
    const struct yt_player *current_player,
    const struct yt_port *report_port,
    const uint8_t date[10], const uint8_t time_text[8],
    struct yt_port_report_text *report, struct yt_error *error)
{
	static const uint8_t title_prefix[] = "Commerce report for ";
	static const uint8_t title_separator[] = ": ";
	static const uint8_t commodity[3][14] = {
		"Ore..........", "Organics.....", "Equipment...."
	};
	static const uint8_t buying[] = "  Buying ";
	static const uint8_t selling[] = "  Selling";
	static const uint8_t padding[] = "    ";
	static const size_t hold_offset[3] = {YT_F69, YT_F73, YT_F77};
	uint8_t promoted_hold[8];
	uint8_t floored_capacity[8];
	char number[96];
	int formatted_length;
	size_t name_length;
	size_t number_length;
	size_t index;

	if (market == NULL || current_player == NULL || report_port == NULL
	    || date == NULL || time_text == NULL || report == NULL)
		return yt_game_error(error, YT_INVALID,
		    "port report arguments");
	memset(report, 0, sizeof(*report));
	name_length = report_port->name_length;
	if (name_length > YT_TEXT_FIELD_SIZE)
		name_length = YT_TEXT_FIELD_SIZE;
	if (!port_report_append(report->title, sizeof(report->title),
	    &report->title_length, title_prefix, sizeof(title_prefix) - 1U)
	    || !port_report_append(report->title, sizeof(report->title),
	    &report->title_length, report_port->record.bytes, name_length)
	    || !port_report_append(report->title, sizeof(report->title),
	    &report->title_length, title_separator,
	    sizeof(title_separator) - 1U)
	    || !port_report_append(report->title, sizeof(report->title),
	    &report->title_length, date, 10U)
	    || !port_report_append(report->title, sizeof(report->title),
	    &report->title_length, " ", 1U)
	    || !port_report_append(report->title, sizeof(report->title),
	    &report->title_length, time_text, 8U))
		return yt_game_error(error, YT_RANGE,
		    "port report title composition");

	for (index = 0U; index < 3U; ++index) {
		struct yt_port_report_item *item = &report->item[index];
		const uint8_t *status;
		size_t position = 0U;

		if (market->port.factor[index] < 0.0f) {
			status = buying;
			item->foreground = 3.0f;
		}
		else {
			status = selling;
			item->foreground = 2.0f;
		}
		if (!port_report_append(item->name_status,
		    sizeof(item->name_status), &position, commodity[index],
		    sizeof(commodity[index]) - 1U)
		    || !port_report_append(item->name_status,
		    sizeof(item->name_status), &position, status,
		    sizeof(buying) - 1U))
			return yt_game_error(error, YT_RANGE,
			    "port report item composition");
		if (qb_mbf64_floor_raw(market->capacity_raw[index],
		    floored_capacity) != QB_MBF_OK)
			return yt_game_error(error, YT_RANGE,
			    "port report stock INT");
		formatted_length = qb_str_mbf64(number, sizeof(number),
		    floored_capacity);
		if (formatted_length < 0
		    || !port_report_right_raw((const uint8_t *)number,
		    (size_t)formatted_length, sizeof(item->capacity),
		    item->capacity))
			return yt_game_error(error, YT_RANGE,
			    "port report stock formatting");
		yt_port_mbf64_promote_single(current_player->record.bytes
		    + hold_offset[index], promoted_hold);
		formatted_length = qb_str_mbf64(number, sizeof(number),
		    promoted_hold);
		if (formatted_length < 0
		    || !port_report_right_raw((const uint8_t *)number,
		    (size_t)formatted_length, sizeof(item->hold), item->hold))
			return yt_game_error(error, YT_RANGE,
			    "port report hold formatting");
		formatted_length = qb_str_mbf32(number, sizeof(number),
		    market->price_raw[index]);
		if (formatted_length < 0)
			return yt_game_error(error, YT_RANGE,
			    "port report price formatting");
		number_length = (size_t)formatted_length;
		position = 0U;
		if (!port_report_append(item->price, sizeof(item->price),
		    &position, number, number_length)
		    || !port_report_append(item->price, sizeof(item->price),
		    &position, padding, sizeof(padding) - 1U))
			return yt_game_error(error, YT_RANGE,
			    "port report price composition");
		item->price_length = position;
	}
	return true;
}

int
yt_computer_selector_position(const char *command)
{
	static const char selector[] = "+!LMP?123459";
	const char *match;

	if (command == NULL)
		return 0;
	match = strstr(selector, command);
	return match == NULL ? 0 : (int)(match - selector) + 1;
}

void
yt_trade_treasury_overlay(struct yt_port *port, float receipt)
{
	volatile float updated;

	if (port == NULL)
		return;
	updated = port->treasury + receipt;
	port->treasury = updated;
	(void)yt_record_set_number(&port->record, YT_F89, updated);
}

void
yt_trade_holds_overlay(struct yt_player *player, size_t commodity,
    float quantity, float direction)
{
	float *selected;
	volatile float single_delta;
	volatile double updated;

	if (player == NULL || commodity >= 3U)
		return;
	selected = commodity == 0U ? &player->ore
	    : commodity == 1U ? &player->organics : &player->equipment;
	single_delta = quantity * direction;
	updated = (double)*selected + (double)single_delta;
	*selected = (float)updated;
	(void)yt_record_set_number(&player->record, YT_F69, player->ore);
	(void)yt_record_set_number(&player->record, YT_F73, player->organics);
	(void)yt_record_set_number(&player->record, YT_F77, player->equipment);
}
