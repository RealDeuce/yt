#include "yt_game.h"

#include "qb.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const uint8_t updater_ten_s[4] = {0x00, 0x00, 0x20, 0x84};
static const uint8_t updater_sixty_s[4] = {0x00, 0x00, 0x70, 0x86};
static const uint8_t updater_minutes_per_day_s[4] = {0x00, 0x00, 0x34, 0x8b};
static const uint8_t updater_missile_divisor_s[4] = {0x00, 0x40, 0x1c, 0x8c};
static const uint8_t updater_mine_divisor_s[4] = {0x00, 0x50, 0x43, 0x8f};
static const uint8_t updater_plasma_rate_s[4] = {0xac, 0xc5, 0x27, 0x70};
static const uint8_t updater_one_percent_s[4] = {0x0a, 0xd7, 0x23, 0x7a};
static const uint8_t updater_ten_thousand_d[8] =
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x1c, 0x8e};
static const uint8_t updater_twenty_thousand_d[8] =
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x1c, 0x8f};
static const uint8_t updater_thirty_thousand_d[8] =
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x6a, 0x8f};
static const uint8_t updater_five_hundred_d[8] =
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7a, 0x89};
static const uint8_t updater_one_e_minus_five_d[8] =
    {0x84, 0x47, 0x1b, 0x47, 0xac, 0xc5, 0x27, 0x70};
static const uint8_t updater_four_e_minus_six_d[8] =
    {0x6a, 0x6c, 0xaf, 0x05, 0xbd, 0x37, 0x06, 0x6f};
static const uint8_t updater_four_e_minus_eight_d[8] =
    {0xcf, 0x61, 0x84, 0x11, 0x77, 0xcc, 0x2b, 0x68};
static const uint8_t updater_one_percent_d[8] =
    {0x00, 0x00, 0x00, 0x00, 0x0a, 0xd7, 0x23, 0x7a};
static const uint8_t updater_ten_d[8] =
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x84};

static const size_t persisted_offsets[14] = {
	YT_F41, YT_F45, YT_F49, YT_F53, YT_F57, YT_F61, YT_F65,
	YT_F69, YT_F77, YT_F89, YT_F113, YT_F117, YT_F125, YT_F129,
};

struct planet_update_work {
	uint8_t quantity[10][8];
	uint8_t production[10][4];
	uint8_t contribution[10][4];
	uint8_t current_day[4];
	uint8_t current_minute[4];
};

static bool
updater_error(struct yt_error *error, enum yt_status status,
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

static void
updater_promote_single(const uint8_t single[4], uint8_t raw[8])
{
	memset(raw, 0, 4U);
	memcpy(raw + 4U, single, 4U);
}

static bool
updater_encode_single(float value, uint8_t raw[4], struct yt_error *error,
    const char *operation)
{
	enum qb_mbf_status status = qb_mbf32_encode(value, raw);

	if (status == QB_MBF_OK || status == QB_MBF_UNDERFLOW)
		return true;
	return updater_error(error, YT_RANGE, operation);
}

typedef enum qb_mbf_status (*updater_raw_binary_fn)(const uint8_t left[8],
    const uint8_t right[8], uint8_t raw[8]);

static bool
updater_raw_binary(updater_raw_binary_fn operation,
    const uint8_t left[8], const uint8_t right[8], uint8_t raw[8],
    struct yt_error *error, const char *label)
{
	enum qb_mbf_status status = operation(left, right, raw);

	if (status == QB_MBF_OK || status == QB_MBF_UNDERFLOW)
		return true;
	return updater_error(error, YT_RANGE, label);
}

static bool
updater_csng(const uint8_t source[8], uint8_t raw[4],
    struct yt_error *error, const char *operation)
{
	enum qb_mbf_status status = qb_mbf32_from_mbf64_raw(source, raw);

	if (status == QB_MBF_OK || status == QB_MBF_UNDERFLOW)
		return true;
	return updater_error(error, YT_RANGE, operation);
}

static bool
updater_int(const uint8_t source[8], uint8_t raw[8],
    struct yt_error *error, const char *operation)
{
	enum qb_mbf_status status = source[7] != 0U
	    && (source[6] & 0x80U) != 0U
	    ? qb_mbf64_floor_raw(source, raw)
	    : qb_mbf64_int_positive_raw(source, raw);

	if (status == QB_MBF_OK || status == QB_MBF_UNDERFLOW)
		return true;
	return updater_error(error, YT_RANGE, operation);
}

static int
updater_compare(const uint8_t left[8], const uint8_t right[8])
{
	bool left_negative;
	bool right_negative;
	int magnitude = 0;
	int index;

	if (left[7] == 0U)
		return right[7] == 0U ? 0
		    : ((right[6] & 0x80U) != 0U ? 1 : -1);
	if (right[7] == 0U)
		return (left[6] & 0x80U) != 0U ? -1 : 1;
	left_negative = (left[6] & 0x80U) != 0U;
	right_negative = (right[6] & 0x80U) != 0U;
	if (left_negative != right_negative)
		return left_negative ? -1 : 1;
	if (left[7] != right[7])
		magnitude = left[7] < right[7] ? -1 : 1;
	else {
		for (index = 6; index >= 0; --index) {
			uint8_t lhs = left[index];
			uint8_t rhs = right[index];

			if (index == 6) {
				lhs &= 0x7fU;
				rhs &= 0x7fU;
			}
			if (lhs != rhs) {
				magnitude = lhs < rhs ? -1 : 1;
				break;
			}
		}
	}
	return left_negative ? -magnitude : magnitude;
}

static bool
updater_contribution(const uint8_t bank[8], const uint8_t constant[8],
    bool multiply, uint8_t raw[4], struct yt_error *error,
    const char *operation)
{
	uint8_t result[8];

	if (!updater_raw_binary(multiply ? qb_mbf64_mul_raw
	    : qb_mbf64_div_raw, bank, constant, result, error, operation))
		return false;
	return updater_csng(result, raw, error, operation);
}

static bool
updater_subtract_single(const uint8_t left[4], const uint8_t right[4],
    const uint8_t residue[4], uint8_t raw[4], struct yt_error *error)
{
	float result;

	if (right[3] == 0U) {
		memcpy(raw, left, 4U);
		return true;
	}
	if (left[3] == 0U)
		return updater_encode_single(-qb_mbf32_decode(right), raw, error,
		    "planet updater base subtraction");
	result = (qb_mbf32_decode(left) -
	    qb_mbf32_decode(right));
	if (result == 0.0f) {
		memcpy(raw, residue, 3U);
		raw[3] = 0U;
		return true;
	}
	return updater_encode_single(result, raw, error,
	    "planet updater base subtraction");
}

static bool
updater_prepare_production(const struct yt_record *record,
    float production[10], struct yt_error *error)
{
	uint8_t raw[4];
	float sum;
	size_t index;

	memset(production, 0, sizeof(float) * 10U);
	for (index = 1U; index <= 3U; ++index)
		production[index] = yt_record_get_number(record,
		    YT_F45 + (index - 1U) * 4U);

	sum = ((production[1] +
	    production[2]) + production[3]);
	production[4] = floorf(sum);
	if (!updater_encode_single(production[4], raw, error,
	    "planet updater P4"))
		return false;
	production[4] = qb_mbf32_decode(raw);

	sum = ((production[1] +
	    production[2]) + production[3]);
	production[5] = floorf((sum /
	    qb_mbf32_decode(updater_missile_divisor_s)));
	if (!updater_encode_single(production[5], raw, error,
	    "planet updater P5"))
		return false;
	production[5] = qb_mbf32_decode(raw);

	sum = ((production[1] +
	    production[2]) + production[3]);
	production[6] = floorf((sum /
	    qb_mbf32_decode(updater_mine_divisor_s)));
	if (!updater_encode_single(production[6], raw, error,
	    "planet updater P6"))
		return false;
	production[6] = qb_mbf32_decode(raw);

	sum = ((production[1] +
	    production[2]) + production[3]);
	production[9] = floorf((sum *
	    qb_mbf32_decode(updater_plasma_rate_s)));
	if (!updater_encode_single(production[9], raw, error,
	    "planet updater P9"))
		return false;
	production[9] = qb_mbf32_decode(raw);
	return true;
}

bool
yt_planet_update_record(struct yt_record *record,
    int16_t current_day, float timer_seconds,
    struct yt_planet_economy *economy, struct yt_error *error)
{
	static const size_t quantity_offsets[9] = {
		YT_F57, YT_F61, YT_F65, YT_F129, YT_F69, YT_F125, YT_F117,
		YT_F77, YT_F113,
	};
	struct planet_update_work work = {0};
	struct yt_record field;
	uint8_t (*contribution_raw)[4] = work.contribution;
	uint8_t (*production_raw)[4] = work.production;
	uint8_t (*quantity_raw)[8] = work.quantity;
	uint8_t persisted[14][4] = {{0}};
	uint8_t elapsed_raw[4];
	uint8_t elapsed_double[8];
	uint8_t fraction_raw[4];
	uint8_t fraction_double[8];
	uint8_t increment_raw[4];
	uint8_t increment_double[8];
	uint8_t timer_raw[4];
	uint8_t scratch[8];
	uint8_t scratch_two[8];
	uint8_t threshold_raw[4];
	uint8_t threshold_double[8];
	uint8_t float_residue[4];
	float production[10] = {0};
	float contribution[10] = {0};
	float current_day_single;
	float current_minute;
	float stored_day;
	float stored_minute;
	float elapsed;
	float one_percent = qb_mbf32_decode(updater_one_percent_s);
	size_t index;

	if (record == NULL || economy == NULL)
		return updater_error(error, YT_INVALID,
		    "planet updater arguments");
	if (!updater_prepare_production(record, production, error))
		return false;
	if (!updater_encode_single((float)current_day, work.current_day, error,
	    "planet updater current day MBF32"))
		return false;
	current_day_single = qb_mbf32_decode(work.current_day);
	if (current_day_single < 0.0f || timer_seconds < 0.0f)
		return updater_error(error, YT_RANGE,
		    "planet updater clock domain");
	field = *record;
	memset(economy, 0, sizeof(*economy));

	for (index = 1U; index <= 6U; ++index) {
		if (!updater_encode_single(production[index],
		    production_raw[index], error,
		    "planet updater prepared production"))
			return false;
	}
	if (!updater_encode_single(production[9], production_raw[9], error,
	    "planet updater prepared plasma"))
		return false;
	for (index = 1U; index <= 9U; ++index)
		updater_promote_single(field.bytes
		    + quantity_offsets[index - 1U], quantity_raw[index]);

	if (!updater_encode_single(timer_seconds, timer_raw, error,
	    "planet updater TIMER MBF32"))
		return false;
	timer_seconds = qb_mbf32_decode(timer_raw);
	current_minute = (timer_seconds /
	    qb_mbf32_decode(updater_sixty_s));
	stored_day = yt_record_get_number(&field, YT_F41);
	stored_minute = yt_record_get_number(&field, YT_F89);
	elapsed = (
	    (current_day_single - stored_day) +
	    ((current_minute - stored_minute) /
	    qb_mbf32_decode(updater_minutes_per_day_s)));
	if (elapsed > qb_mbf32_decode(updater_ten_s) || elapsed < 0.0f)
		elapsed = qb_mbf32_decode(updater_ten_s);
	if (!updater_encode_single(current_minute, work.current_minute, error,
	    "planet updater minute MBF32"))
		return false;
	if (!updater_encode_single(elapsed, elapsed_raw, error,
	    "planet updater elapsed MBF32"))
		return false;
	updater_promote_single(elapsed_raw, elapsed_double);

	if (!updater_contribution(quantity_raw[7], updater_ten_thousand_d, false,
	    contribution_raw[1], error, "planet updater ore contribution"))
		return false;
	if (!updater_contribution(quantity_raw[7], updater_twenty_thousand_d,
	    false, contribution_raw[2], error,
	    "planet updater organics contribution"))
		return false;
	if (!updater_contribution(quantity_raw[7], updater_thirty_thousand_d,
	    false, contribution_raw[3], error,
	    "planet updater equipment contribution"))
		return false;
	if (!updater_contribution(quantity_raw[7], updater_five_hundred_d, false,
	    contribution_raw[4], error, "planet updater fighter contribution"))
		return false;
	if (!updater_contribution(quantity_raw[7], updater_one_e_minus_five_d,
	    true, contribution_raw[5], error,
	    "planet updater missile contribution"))
		return false;
	if (!updater_contribution(quantity_raw[7], updater_four_e_minus_six_d,
	    true, contribution_raw[6], error,
	    "planet updater mine contribution"))
		return false;
	if (!updater_contribution(quantity_raw[7], updater_ten_thousand_d, false,
	    contribution_raw[8], error, "planet updater force contribution"))
		return false;
	if (!updater_contribution(quantity_raw[7], updater_four_e_minus_eight_d,
	    true, contribution_raw[9], error,
	    "planet updater plasma contribution"))
		return false;
	for (index = 1U; index <= 9U; ++index)
		contribution[index] = qb_mbf32_decode(contribution_raw[index]);

	if (!updater_encode_single((elapsed * one_percent),
	    fraction_raw, error, "planet updater bank fraction"))
		return false;
	updater_promote_single(fraction_raw, fraction_double);
	if (quantity_raw[7][7] != 0U) {
		if (!updater_raw_binary(qb_mbf64_mul_raw, quantity_raw[7],
		    fraction_double, scratch, error,
		    "planet updater bank growth"))
			return false;
		if (!updater_raw_binary(qb_mbf64_add_raw, quantity_raw[7],
		    scratch, scratch_two, error, "planet updater bank total"))
			return false;
		memcpy(scratch, scratch_two, 8U);
	}
	else
		memcpy(scratch, quantity_raw[7], 8U);
	if (!updater_int(scratch, quantity_raw[7], error,
	    "planet updater bank INT"))
		return false;

	if (!updater_raw_binary(qb_mbf64_mul_raw, elapsed_double,
	    quantity_raw[8], scratch, error,
	    "planet updater force growth elapsed"))
		return false;
	if (!updater_raw_binary(qb_mbf64_mul_raw, scratch,
	    updater_one_percent_d, scratch_two, error,
	    "planet updater force growth rate"))
		return false;
	if (!updater_encode_single((contribution[8] * elapsed),
	    increment_raw, error, "planet updater force reinforcement"))
		return false;
	updater_promote_single(increment_raw, increment_double);
	if (quantity_raw[8][7] == 0U && increment_double[7] == 0U)
		memcpy(scratch, quantity_raw[8], 8U);
	else {
		if (!updater_raw_binary(qb_mbf64_add_raw, quantity_raw[8], scratch_two,
		    scratch, error, "planet updater force growth total"))
			return false;
		if (!updater_raw_binary(qb_mbf64_add_raw, scratch,
		    increment_double, scratch_two, error,
		    "planet updater force reinforcement total"))
			return false;
		memcpy(scratch, scratch_two, 8U);
	}
	if (!updater_int(scratch, quantity_raw[8], error,
	    "planet updater force INT"))
		return false;

	for (index = 1U; index <= 3U; ++index) {
		production[index] = (production[index] + (
		    (production[index] * elapsed) * one_percent));
		if (!updater_encode_single(production[index], production_raw[index], error,
		    index == 1U ? "YT-SUB2:0D63 planet updater ERR6"
		    : "planet updater base growth")) {
			if (index == 1U && error != NULL) {
				error->basic_error = 6U;
				error->basic_error_valid = true;
			}
			return false;
		}
		production[index] = qb_mbf32_decode(production_raw[index]);
	}
	for (index = 1U; index <= 6U; ++index) {
		production[index] = (production[index] + contribution[index]);
		if (!updater_encode_single(production[index], production_raw[index], error,
		    "planet updater production rate"))
			return false;
		production[index] = qb_mbf32_decode(production_raw[index]);
		if (!updater_encode_single((production[index] * elapsed),
		    increment_raw, error, "planet updater production increment"))
			return false;
		updater_promote_single(increment_raw, increment_double);
		if (increment_double[7] != 0U) {
			if (!updater_raw_binary(qb_mbf64_add_raw, quantity_raw[index],
			    increment_double, scratch, error,
			    "planet updater quantity total"))
				return false;
			memcpy(quantity_raw[index], scratch, 8U);
		}
		if (index <= 3U) {
			if (!updater_encode_single((production[index] *
			    qb_mbf32_decode(updater_ten_s)), threshold_raw, error,
			    "planet updater commodity threshold"))
				return false;
			updater_promote_single(threshold_raw, threshold_double);
			if (updater_compare(quantity_raw[index],
			    threshold_double) > 0) {
				updater_promote_single(contribution_raw[index], increment_double);
				if (!updater_raw_binary(qb_mbf64_div_raw,
				    quantity_raw[index], updater_ten_d, scratch, error,
				    "planet updater commodity catchup division"))
					return false;
				if (!updater_raw_binary(qb_mbf64_add_raw, scratch,
				    increment_double, scratch_two, error,
				    "planet updater commodity catchup addition"))
					return false;
				if (!updater_csng(scratch_two,
				    production_raw[index], error,
				    "planet updater commodity catchup CSNG"))
					return false;
				production[index] = qb_mbf32_decode(production_raw[index]);
			}
		}
	}
	production[9] = (production[9] + contribution[9]);
	if (!updater_encode_single(production[9], production_raw[9], error,
	    "planet updater plasma rate"))
		return false;
	production[9] = qb_mbf32_decode(production_raw[9]);
	if (!updater_encode_single((production[9] * elapsed),
	    increment_raw, error, "planet updater plasma increment"))
		return false;
	updater_promote_single(increment_raw, increment_double);
	if (increment_double[7] != 0U) {
		if (!updater_raw_binary(qb_mbf64_add_raw, quantity_raw[9],
		    increment_double, scratch, error,
		    "planet updater plasma total"))
			return false;
		memcpy(quantity_raw[9], scratch, 8U);
	}

	memcpy(float_residue, quantity_raw[9] + 4U, 4U);
	for (index = 1U; index <= 3U; ++index) {
		if (!updater_encode_single(production[index],
		    production_raw[index], error,
		    "planet updater base production MBF32"))
			return false;
		if (!updater_subtract_single(production_raw[index],
		    contribution_raw[index], float_residue, persisted[index],
		    error))
			return false;
		memcpy(float_residue, persisted[index], 4U);
	}
	memcpy(persisted[0], work.current_day, 4U);
	memcpy(persisted[9], work.current_minute, 4U);
	if (!updater_csng(quantity_raw[1], persisted[4], error,
	    "planet updater stock ore CSNG"))
		return false;
	if (!updater_csng(quantity_raw[2], persisted[5], error,
	    "planet updater stock organics CSNG"))
		return false;
	if (!updater_csng(quantity_raw[3], persisted[6], error,
	    "planet updater stock equipment CSNG"))
		return false;
	if (!updater_csng(quantity_raw[5], persisted[7], error,
	    "planet updater missiles CSNG"))
		return false;
	if (!updater_csng(quantity_raw[8], persisted[8], error,
	    "planet updater forces CSNG"))
		return false;
	if (!updater_csng(quantity_raw[9], persisted[10], error,
	    "planet updater plasma CSNG"))
		return false;
	if (!updater_csng(quantity_raw[7], persisted[11], error,
	    "planet updater bank CSNG"))
		return false;
	if (!updater_csng(quantity_raw[6], persisted[12], error,
	    "planet updater mines CSNG"))
		return false;
	if (!updater_csng(quantity_raw[4], persisted[13], error,
	    "planet updater fighters CSNG"))
		return false;
	for (index = 0U; index < YT_ARRAY_LEN(persisted_offsets); ++index) {
		memcpy(field.bytes + persisted_offsets[index],
		    persisted[index], 4U);
	}

	*record = field;
	economy->current_day = current_day;
	economy->current_minute = current_minute;
	economy->elapsed = elapsed;
	for (index = 0U; index < 10U; ++index) {
		economy->production[index] = production[index];
		economy->quantity[index] = qb_mbf64_decode(quantity_raw[index]);
		economy->contribution[index] = contribution[index];
	}
	return true;
}
