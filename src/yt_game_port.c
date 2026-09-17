#include "yt_game.h"
#include "yt_game_internal.h"

#include "qb.h"

#include <math.h>
#include <string.h>

bool
yt_computer_port_select(const char *response, float maximum,
    float *selected, enum yt_computer_port_selection_route *route,
    struct yt_error *error)
{
	struct qb_val_result parsed;
	uint8_t raw[4];
	volatile float candidate;
	bool above;
	bool below;
	enum qb_mbf_status status;

	if (response == NULL || selected == NULL || route == NULL)
		return yt_game_error(error, YT_INVALID,
		    "computer port selection arguments");
	*selected = 0.0f;
	if (response[0] == '\0') {
		*route = YT_COMPUTER_PORT_SELECTION_EMPTY;
		return true;
	}
	parsed = qb_val(response);
	if (parsed.overflow)
		return yt_game_error(error, YT_RANGE,
		    "computer port sector VAL");
	candidate = (float)qb_int(parsed.valid ? parsed.value : 0.0);
	status = qb_mbf32_encode(candidate, raw);
	if (status == QB_MBF_OVERFLOW)
		return yt_game_error(error, YT_RANGE,
		    "computer port sector CSNG");
	*selected = status == QB_MBF_UNDERFLOW ? 0.0f
	    : qb_mbf32_decode(raw);
	above = *selected > maximum;
	below = *selected < 1.0f;
	*route = (above | below) ? YT_COMPUTER_PORT_SELECTION_INVALID
	    : YT_COMPUTER_PORT_SELECTION_ACCEPTED;
	return true;
}

bool
yt_computer_path_parse(const char *response, float *selected,
    struct yt_error *error)
{
	struct qb_val_result parsed;
	uint8_t integer_raw[8];
	uint8_t selected_raw[4];
	enum qb_mbf_status status;

	if (response == NULL || selected == NULL)
		return yt_game_error(error, YT_INVALID,
		    "computer path parse arguments");
	parsed = qb_val(response);
	if (parsed.overflow)
		return yt_game_error(error, YT_RANGE,
		    "computer path sector VAL");
	status = qb_mbf64_floor_raw(parsed.mbf, integer_raw);
	if (status != QB_MBF_OK)
		return yt_game_error(error, YT_RANGE,
		    "computer path sector INT");
	status = qb_mbf32_from_mbf64_raw(integer_raw, selected_raw);
	if (status == QB_MBF_OVERFLOW || status == QB_MBF_DOMAIN)
		return yt_game_error(error, YT_RANGE,
		    "computer path sector CSNG");
	if (status == QB_MBF_UNDERFLOW)
		memset(selected_raw, 0, 4U);
	*selected = qb_mbf32_decode(selected_raw);
	return true;
}

bool
yt_computer_path_append_hop(char *scratch, size_t capacity,
    size_t *length, float next_sector, float *hop_count,
    struct yt_error *error)
{
	char number[64];
	uint8_t hop_count_raw[4];
	int number_length;
	volatile float incremented;
	enum qb_mbf_status status;

	if (scratch == NULL || capacity == 0U || length == NULL
	    || *length >= capacity || scratch[*length] != '\0'
	    || hop_count == NULL)
		return yt_game_error(error, YT_INVALID,
		    "computer path scratch arguments");
	number_length = qb_str_single(number, sizeof(number), next_sector);
	if (number_length < 0 || (size_t)number_length + 3U
	    >= capacity - *length)
		return yt_game_error(error, YT_RANGE,
		    "computer path scratch append");
	scratch[(*length)++] = '\r';
	scratch[(*length)++] = 'M';
	scratch[(*length)++] = '\r';
	memcpy(scratch + *length, number, (size_t)number_length);
	*length += (size_t)number_length;
	scratch[*length] = '\0';
	incremented = *hop_count + 1.0f;
	status = qb_mbf32_encode(incremented, hop_count_raw);
	if (status != QB_MBF_OK)
		return yt_game_error(error, YT_RANGE,
		    "computer path hop increment");
	*hop_count = qb_mbf32_decode(hop_count_raw);
	return true;
}

bool
yt_computer_path_wrap_required(int local_column)
{
	return local_column > 74;
}

static bool
computer_avoid_csng(const struct qb_val_result *parsed, float *selected,
    struct yt_error *error, const char *operation)
{
	uint8_t raw[4];
	enum qb_mbf_status status;

	status = qb_mbf32_from_mbf64_raw(parsed->mbf, raw);
	if (status == QB_MBF_OVERFLOW || status == QB_MBF_DOMAIN)
		return yt_game_error(error, YT_RANGE, operation);
	*selected = status == QB_MBF_UNDERFLOW ? 0.0f
	    : qb_mbf32_decode(raw);
	return true;
}

bool
yt_computer_avoid_select_slot(const char *response, uint8_t conversion_mode,
    float *selected, int *index,
    enum yt_computer_avoid_selection_route *route, struct yt_error *error)
{
	struct qb_val_result parsed;
	bool overflow;

	if (response == NULL || selected == NULL || index == NULL
	    || route == NULL)
		return yt_game_error(error, YT_INVALID,
		    "avoid slot arguments");
	*selected = 0.0f;
	*index = 0;
	*route = YT_COMPUTER_AVOID_SELECTION_INVALID;
	parsed = qb_val(response);
	if (parsed.overflow)
		return yt_game_error(error, YT_RANGE,
		    "avoid slot VAL");
	if (!computer_avoid_csng(&parsed, selected, error,
	    "avoid slot CSNG"))
		return false;
	if (*selected < 1.0f || *selected > 30.0f)
		return true;
	*index = (int)qb_cint_mode((double)*selected, conversion_mode,
	    &overflow);
	if (overflow || *index < 1 || *index > 30)
		return yt_game_error(error, YT_RANGE,
		    "avoid slot CINT");
	*route = YT_COMPUTER_AVOID_SELECTION_ACCEPTED;
	return true;
}

bool
yt_computer_avoid_select_sector(const char *response, float maximum,
    float *selected, enum yt_computer_avoid_selection_route *route,
    struct yt_error *error)
{
	struct qb_val_result parsed;

	if (response == NULL || selected == NULL || route == NULL)
		return yt_game_error(error, YT_INVALID,
		    "avoid sector arguments");
	*selected = 0.0f;
	*route = YT_COMPUTER_AVOID_SELECTION_INVALID;
	parsed = qb_val(response);
	if (parsed.overflow)
		return yt_game_error(error, YT_RANGE,
		    "avoid sector VAL");
	if (!computer_avoid_csng(&parsed, selected, error,
	    "avoid sector CSNG"))
		return false;
	if (*selected < 0.0f || *selected > maximum)
		return true;
	*route = YT_COMPUTER_AVOID_SELECTION_ACCEPTED;
	return true;
}

void
yt_computer_avoid_transition(float old_value, float new_value,
    bool *locked, bool *available)
{
	if (locked != NULL)
		*locked = new_value != 0.0f;
	if (available != NULL)
		*available = old_value != 0.0f && old_value != new_value;
}

bool
yt_port_name_display_row(const uint8_t *cached, size_t cached_length,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "This port is called: \"";
	static const uint8_t suffix[] = "\".";
	size_t needed;

	if (length == NULL || (cached == NULL && cached_length != 0U))
		return false;
	*length = 0U;
	needed = sizeof(prefix) - 1U + cached_length + sizeof(suffix) - 1U;
	if (needed > capacity || (needed != 0U && row == NULL))
		return false;
	memcpy(row, prefix, sizeof(prefix) - 1U);
	if (cached_length != 0U)
		memcpy(row + sizeof(prefix) - 1U, cached, cached_length);
	memcpy(row + sizeof(prefix) - 1U + cached_length, suffix,
	    sizeof(suffix) - 1U);
	*length = needed;
	return true;
}

bool
yt_port_name_prepare_candidate(const uint8_t *entered,
    size_t entered_length, const uint8_t *cached, size_t cached_length,
    uint8_t *candidate, size_t capacity, size_t *candidate_length)
{
	size_t normalized;

	if (candidate_length == NULL
	    || (entered == NULL && entered_length != 0U)
	    || (cached == NULL && cached_length != 0U))
		return false;
	*candidate_length = 0U;
	if (candidate == NULL || entered_length > capacity)
		return false;
	if (entered_length != 0U)
		memcpy(candidate, entered, entered_length);
	normalized = qb_title_case_n(candidate, entered_length);
	if (normalized > YT_TEXT_FIELD_SIZE)
		normalized = YT_TEXT_FIELD_SIZE;
	if (normalized != 0U) {
		*candidate_length = normalized;
		return true;
	}
	if (cached_length > capacity
	    || (cached_length != 0U && candidate == NULL))
		return false;
	if (cached_length != 0U)
		memcpy(candidate, cached, cached_length);
	*candidate_length = cached_length;
	return true;
}

bool
yt_port_name_confirmation_prompt(const uint8_t *candidate,
    size_t candidate_length, uint8_t *prompt, size_t capacity,
    size_t *length)
{
	static const uint8_t suffix[] = "\" Is this OK? [y/N]";
	size_t needed;

	if (length == NULL || (candidate == NULL && candidate_length != 0U))
		return false;
	*length = 0U;
	needed = 1U + candidate_length + sizeof(suffix) - 1U;
	if (needed > capacity || (needed != 0U && prompt == NULL))
		return false;
	prompt[0] = '"';
	if (candidate_length != 0U)
		memcpy(prompt + 1U, candidate, candidate_length);
	memcpy(prompt + 1U + candidate_length, suffix,
	    sizeof(suffix) - 1U);
	*length = needed;
	return true;
}

bool
yt_port_name_overlay(struct yt_port *port, const uint8_t *candidate,
    size_t candidate_length)
{
	size_t copied;

	if (port == NULL || (candidate == NULL && candidate_length != 0U))
		return false;
	yt_record_set_text(&port->record, candidate, candidate_length);
	if (!yt_record_set_number(&port->record, YT_F85,
	    (float)candidate_length))
		return false;
	port->name_length = candidate_length;
	copied = candidate_length < YT_TEXT_FIELD_SIZE
	    ? candidate_length : YT_TEXT_FIELD_SIZE;
	if (copied != 0U)
		memcpy(port->name, candidate, copied);
	port->name[copied] = '\0';
	return true;
}

double
yt_port_purchase_price(const float production[3])
{
	volatile float sum12;
	volatile float sum123;
	volatile float divided;
	volatile float integral;
	volatile float result;

	if (production == NULL)
		return 0.0;
	sum12 = production[0] + production[1];
	sum123 = sum12 + production[2];
	divided = sum123 / 10.0f;
	integral = floorf(divided);
	result = integral + 1.0f;
	return (double)result;
}

float
yt_port_purchase_seller_credit(float treasury, float credits, double price)
{
	volatile double subtotal = (double)treasury + (double)credits;
	volatile double total = subtotal + price;

	return (float)total;
}

float
yt_port_purchase_buyer_credit(float credits, double price)
{
	volatile double result = (double)credits - price;

	return (float)result;
}

bool
yt_port_purchase_seller_overlay(struct yt_player *seller, float treasury,
    double price)
{
	if (seller == NULL)
		return false;
	seller->credits = yt_port_purchase_seller_credit(treasury,
	    seller->credits, price);
	--seller->ports_owned;
	return yt_record_set_number(&seller->record, YT_F81, seller->credits)
	    && yt_record_set_number(&seller->record, YT_F117,
	    (float)seller->ports_owned);
}

bool
yt_port_purchase_title_overlay(struct yt_port *port, int buyer_record)
{
	if (port == NULL)
		return false;
	port->owner = buyer_record;
	port->treasury = 0.0f;
	return yt_record_set_number(&port->record, YT_F97,
	    (float)port->owner)
	    && yt_record_set_number(&port->record, YT_F89, 0.0f);
}

bool
yt_port_purchase_buyer_overlay(struct yt_player *buyer, double price)
{
	if (buyer == NULL)
		return false;
	buyer->credits = yt_port_purchase_buyer_credit(buyer->credits, price);
	++buyer->ports_owned;
	return yt_record_set_number(&buyer->record, YT_F81, buyer->credits)
	    && yt_record_set_number(&buyer->record, YT_F117,
	    (float)buyer->ports_owned);
}
