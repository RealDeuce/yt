#include "yt_game.h"
#include "yt_game_internal.h"

#include "qb.h"

#include <math.h>
#include <string.h>

bool
yt_computer_port_select(const struct qb_val_result *parsed, bool blank,
    uint16_t maximum,
    uint16_t *selected, enum yt_computer_port_selection_route *route,
    struct yt_error *error)
{
	float converted;
	bool above;
	bool below;
	enum qb_mbf_status status;

	if (parsed == NULL || selected == NULL || route == NULL)
		return yt_game_error(error, YT_INVALID,
		    "computer port selection arguments");
	*selected = 0U;
	if (blank) {
		*route = YT_COMPUTER_PORT_SELECTION_EMPTY;
		return true;
	}
	if (parsed->overflow)
		return yt_game_error(error, YT_RANGE,
		    "computer port sector VAL");
	status = qb_val_int_single_or_zero(parsed, &converted);
	if (status == QB_MBF_OVERFLOW)
		return yt_game_error(error, YT_RANGE,
		    "computer port sector CSNG");
	above = converted > (float)maximum;
	below = converted < 1.0f;
	*route = (above | below) ? YT_COMPUTER_PORT_SELECTION_INVALID
	    : YT_COMPUTER_PORT_SELECTION_ACCEPTED;
	if (*route == YT_COMPUTER_PORT_SELECTION_ACCEPTED)
		*selected = (uint16_t)converted;
	return true;
}

bool
yt_computer_path_parse(const struct qb_val_result *parsed, float *selected,
    struct yt_error *error)
{
	enum qb_mbf_status status;

	if (parsed == NULL || selected == NULL)
		return yt_game_error(error, YT_INVALID,
		    "computer path parse arguments");
	if (parsed->overflow)
		return yt_game_error(error, YT_RANGE,
		    "computer path sector VAL");
	status = qb_val_int_single_or_zero(parsed, selected);
	if (status == QB_MBF_OVERFLOW || status == QB_MBF_DOMAIN)
		return yt_game_error(error, YT_RANGE,
		    "computer path sector CSNG");
	return true;
}

bool
yt_computer_path_append_hop(char *scratch, size_t capacity,
    size_t *length, uint16_t next_sector, uint16_t *hop_count,
    struct yt_error *error)
{
	char number[64];
	int number_length;

	if (scratch == NULL || capacity == 0U || length == NULL
	    || *length >= capacity || scratch[*length] != '\0'
	    || hop_count == NULL)
		return yt_game_error(error, YT_INVALID,
		    "computer path scratch arguments");
	number_length = qb_str_single(number, sizeof(number),
	    (float)next_sector);
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
	if (*hop_count == UINT16_MAX)
		return yt_game_error(error, YT_RANGE,
		    "computer path hop increment");
	++*hop_count;
	return true;
}

bool
yt_computer_path_wrap_required(int local_column)
{
	return local_column > 74;
}

bool
yt_computer_avoid_select_slot(const struct qb_val_result *parsed,
    uint8_t conversion_mode,
    int *index,
    enum yt_computer_avoid_selection_route *route, struct yt_error *error)
{
	float selected;
	bool overflow;
	enum qb_mbf_status status;

	if (parsed == NULL || index == NULL
	    || route == NULL)
		return yt_game_error(error, YT_INVALID,
		    "avoid slot arguments");
	selected = 0.0f;
	*index = 0;
	*route = YT_COMPUTER_AVOID_SELECTION_INVALID;
	if (parsed->overflow)
		return yt_game_error(error, YT_RANGE,
		    "avoid slot VAL");
	status = qb_val_single_or_zero(parsed, &selected);
	if (status == QB_MBF_OVERFLOW || status == QB_MBF_DOMAIN)
		return yt_game_error(error, YT_RANGE, "avoid slot CSNG");
	if (selected < 1.0f || selected > 30.0f)
		return true;
	*index = (int)qb_cint_mode((double)selected, conversion_mode,
	    &overflow);
	if (overflow || *index < 1 || *index > 30)
		return yt_game_error(error, YT_RANGE,
		    "avoid slot CINT");
	*route = YT_COMPUTER_AVOID_SELECTION_ACCEPTED;
	return true;
}

bool
yt_computer_avoid_select_sector(const struct qb_val_result *parsed,
    uint16_t maximum,
    float *selected, enum yt_computer_avoid_selection_route *route,
    struct yt_error *error)
{
	enum qb_mbf_status status;

	if (parsed == NULL || selected == NULL || route == NULL)
		return yt_game_error(error, YT_INVALID,
		    "avoid sector arguments");
	*selected = 0.0f;
	*route = YT_COMPUTER_AVOID_SELECTION_INVALID;
	if (parsed->overflow)
		return yt_game_error(error, YT_RANGE,
		    "avoid sector VAL");
	status = qb_val_single_or_zero(parsed, selected);
	if (status == QB_MBF_OVERFLOW || status == QB_MBF_DOMAIN)
		return yt_game_error(error, YT_RANGE, "avoid sector CSNG");
	if (*selected < 0.0f || *selected > (float)maximum)
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
	if (production == NULL)
		return 0.0;
	return (double)(floorf((production[0] + production[1]
	    + production[2]) / 10.0f) + 1.0f);
}

float
yt_port_purchase_seller_credit(float treasury, float credits, double price)
{
	return (float)((double)treasury + (double)credits + price);
}

float
yt_port_purchase_buyer_credit(float credits, double price)
{
	return (float)((double)credits - price);
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
	if (!yt_record_set_number(&seller->record, YT_F81, seller->credits))
		return false;
	return yt_record_set_number(&seller->record, YT_F117,
	    (float)seller->ports_owned);
}

bool
yt_port_purchase_title_overlay(struct yt_port *port, int buyer_record)
{
	if (port == NULL)
		return false;
	port->owner = buyer_record;
	port->treasury = 0.0f;
	if (!yt_record_set_number(&port->record, YT_F97,
	    (float)port->owner))
		return false;
	return yt_record_set_number(&port->record, YT_F89, 0.0f);
}

bool
yt_port_purchase_buyer_overlay(struct yt_player *buyer, double price)
{
	if (buyer == NULL)
		return false;
	buyer->credits = yt_port_purchase_buyer_credit(buyer->credits, price);
	++buyer->ports_owned;
	if (!yt_record_set_number(&buyer->record, YT_F81, buyer->credits))
		return false;
	return yt_record_set_number(&buyer->record, YT_F117,
	    (float)buyer->ports_owned);
}
