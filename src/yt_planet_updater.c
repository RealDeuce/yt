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

static const enum yt_planet_updater_stage updater_lset_stages[14] = {
	YT_PLANET_UPDATER_LSET_DAY,
	YT_PLANET_UPDATER_LSET_BASE_ORE,
	YT_PLANET_UPDATER_LSET_BASE_ORGANICS,
	YT_PLANET_UPDATER_LSET_BASE_EQUIPMENT,
	YT_PLANET_UPDATER_LSET_STOCK_ORE,
	YT_PLANET_UPDATER_LSET_STOCK_ORGANICS,
	YT_PLANET_UPDATER_LSET_STOCK_EQUIPMENT,
	YT_PLANET_UPDATER_LSET_MISSILES,
	YT_PLANET_UPDATER_LSET_FORCES,
	YT_PLANET_UPDATER_LSET_MINUTE,
	YT_PLANET_UPDATER_LSET_PLASMA,
	YT_PLANET_UPDATER_LSET_BANK,
	YT_PLANET_UPDATER_LSET_MINES,
	YT_PLANET_UPDATER_LSET_FIGHTERS,
};

static const size_t updater_lset_offsets[14] = {
	YT_F41, YT_F45, YT_F49, YT_F53, YT_F57, YT_F61, YT_F65,
	YT_F69, YT_F77, YT_F89, YT_F113, YT_F117, YT_F125, YT_F129,
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

const char *
yt_planet_updater_stage_name(enum yt_planet_updater_stage stage)
{
	static const char *const names[YT_PLANET_UPDATER_STAGE_COUNT] = {
		"planet-updater:date-helper",
		"planet-updater:record-expression",
		"planet-updater:get",
		"planet-updater:timer",
		"planet-updater:lset-day",
		"planet-updater:lset-base-ore",
		"planet-updater:lset-base-organics",
		"planet-updater:lset-base-equipment",
		"planet-updater:lset-stock-ore",
		"planet-updater:lset-stock-organics",
		"planet-updater:lset-stock-equipment",
		"planet-updater:lset-missiles",
		"planet-updater:lset-forces",
		"planet-updater:lset-minute",
		"planet-updater:lset-plasma",
		"planet-updater:lset-bank",
		"planet-updater:lset-mines",
		"planet-updater:lset-fighters",
		"planet-updater:closing-record-expression",
		"planet-updater:put",
	};

	if (stage < 0 || stage >= YT_PLANET_UPDATER_STAGE_COUNT)
		return "planet-updater:invalid";
	return names[stage];
}

const char *
yt_planet_updater_stage_site(enum yt_planet_updater_stage stage)
{
	static const char *const sites[YT_PLANET_UPDATER_STAGE_COUNT] = {
		"YT-SUB2:0AA5", "YT-SUB2:0AB3", "YT-SUB2:0AC1",
		"YT-SUB2:0BF6", "YT-SUB2:0EBF", "YT-SUB2:0ECB",
		"YT-SUB2:0EE0", "YT-SUB2:0EF5", "YT-SUB2:0F0A",
		"YT-SUB2:0F1C", "YT-SUB2:0F2E", "YT-SUB2:0F40",
		"YT-SUB2:0F52", "YT-SUB2:0F64", "YT-SUB2:0F70",
		"YT-SUB2:0F82", "YT-SUB2:0F94", "YT-SUB2:0FA6",
		"YT-SUB2:0FBE", "YT-SUB2:0FCC",
	};

	if (stage < 0 || stage >= YT_PLANET_UPDATER_STAGE_COUNT)
		return "YT-SUB2:????";
	return sites[stage];
}

static float
updater_single_add(float left, float right)
{
	volatile float result = left + right;

	return result;
}

static float
updater_single_sub(float left, float right)
{
	volatile float result = left - right;

	return result;
}

static float
updater_single_mul(float left, float right)
{
	volatile float result = left * right;

	return result;
}

static float
updater_single_div(float left, float right)
{
	volatile float result = left / right;

	return result;
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
	enum qb_mbf_status status = qb_mbf64_int_positive_raw(source, raw);

	if (status == QB_MBF_OK || status == QB_MBF_UNDERFLOW)
		return true;
	return updater_error(error, YT_RANGE, operation);
}

static int
updater_compare_nonnegative(const uint8_t left[8], const uint8_t right[8])
{
	int index;

	if (left[7] == 0U)
		return right[7] == 0U ? 0 : -1;
	if (right[7] == 0U)
		return 1;
	if (left[7] != right[7])
		return left[7] < right[7] ? -1 : 1;
	for (index = 6; index >= 0; --index) {
		uint8_t lhs = left[index];
		uint8_t rhs = right[index];

		if (index == 6) {
			lhs &= 0x7fU;
			rhs &= 0x7fU;
		}
		if (lhs != rhs)
			return lhs < rhs ? -1 : 1;
	}
	return 0;
}

static bool
updater_dirty_zero(const uint8_t raw[4])
{
	return raw[3] == 0U
	    && (raw[0] != 0U || raw[1] != 0U || raw[2] != 0U);
}

static bool
updater_validate_record(const struct yt_record *record,
    struct yt_error *error)
{
	static const size_t offsets[] = {
		YT_F41, YT_F45, YT_F49, YT_F53, YT_F57, YT_F61, YT_F65,
		YT_F69, YT_F77, YT_F89, YT_F113, YT_F117, YT_F125, YT_F129,
	};
	static const size_t dirty_allowed[] = {
		YT_F57, YT_F61, YT_F65, YT_F69, YT_F77, YT_F113, YT_F117,
		YT_F125, YT_F129,
	};
	size_t index;

	for (index = 0U; index < YT_ARRAY_LEN(offsets); ++index) {
		const uint8_t *raw = record->bytes + offsets[index];
		bool allowed = false;
		size_t candidate;

		for (candidate = 0U; candidate < YT_ARRAY_LEN(dirty_allowed);
		    ++candidate) {
			if (offsets[index] == dirty_allowed[candidate]) {
				allowed = true;
				break;
			}
		}
		if (updater_dirty_zero(raw) && !allowed)
			return updater_error(error, YT_RANGE,
			    "planet updater unsupported dirty zero");
		if (raw[3] != 0U && (raw[2] & 0x80U) != 0U)
			return updater_error(error, YT_RANGE,
			    "planet updater unsupported negative field");
	}
	return true;
}

static bool
updater_begin(struct yt_planet_updater_state *state,
    enum yt_planet_updater_stage stage)
{
	state->stage = stage;
	++state->effect_count;
	return true;
}

static void
updater_complete(struct yt_planet_updater_state *state)
{
	++state->completed_effects;
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
	result = updater_single_sub(qb_mbf32_decode(left),
	    qb_mbf32_decode(right));
	if (result == 0.0f) {
		memcpy(raw, residue, 3U);
		raw[3] = 0U;
		return true;
	}
	return updater_encode_single(result, raw, error,
	    "planet updater base subtraction");
}

static uint32_t
updater_record_number(const uint8_t base_raw[4], const uint8_t logical_raw[4])
{
	float expression = updater_single_add(qb_mbf32_decode(base_raw),
	    qb_mbf32_decode(logical_raw));

	return qb_brun_random_record_number(expression);
}

bool
yt_planet_updater_run(struct yt_planet_updater_state *state,
    const struct yt_planet_updater_ops *ops, void *context,
    struct yt_error *error)
{
	static const size_t quantity_offsets[9] = {
		YT_F57, YT_F61, YT_F65, YT_F129, YT_F69, YT_F125, YT_F117,
		YT_F77, YT_F113,
	};
	uint8_t a_raw[10][4] = {{0}};
	uint8_t p_raw[10][4] = {{0}};
	uint8_t q_raw[10][8] = {{0}};
	uint8_t persisted[14][4] = {{0}};
	uint8_t elapsed_raw[4];
	uint8_t elapsed_double[8];
	uint8_t fraction_raw[4];
	uint8_t fraction_double[8];
	uint8_t increment_raw[4];
	uint8_t increment_double[8];
	uint8_t scratch[8];
	uint8_t scratch_two[8];
	uint8_t threshold_raw[4];
	uint8_t threshold_double[8];
	uint8_t float_residue[4];
	float p[10] = {0};
	float a[10] = {0};
	float logical;
	float base;
	float current_day;
	float timer_seconds;
	float current_minute;
	float stored_day;
	float stored_minute;
	float elapsed;
	float sum;
	float one_percent = qb_mbf32_decode(updater_one_percent_s);
	uint32_t closing_record;
	size_t index;

	if (state == NULL || ops == NULL || ops->date == NULL
	    || ops->record_expression == NULL || ops->get == NULL
	    || ops->timer == NULL || ops->lset == NULL || ops->put == NULL)
		return updater_error(error, YT_INVALID,
		    "planet updater arguments");
	logical = qb_mbf32_decode(state->logical_planet_raw);
	base = qb_mbf32_decode(state->planet_offset_raw);
	if (logical < 1.0f || logical != floorf(logical) || base < 0.0f
	    || base != floorf(base))
		return updater_error(error, YT_RANGE,
		    "planet updater logical record domain");
	memset(state->current_day_raw, 0, sizeof(state->current_day_raw));
	memset(state->timer_seconds_raw, 0, sizeof(state->timer_seconds_raw));
	memset(&state->cache, 0, sizeof(state->cache));
	state->physical_record = 0U;
	state->stage = YT_PLANET_UPDATER_STAGE_COUNT;
	state->effect_count = 0U;
	state->completed_effects = 0U;
	state->field_loaded = false;
	state->field_dirty = false;
	state->written = false;

	updater_begin(state, YT_PLANET_UPDATER_DATE_HELPER);
	if (!ops->date(context, state->current_day_raw, error))
		return false;
	updater_complete(state);
	current_day = qb_mbf32_decode(state->current_day_raw);
	if (current_day < 0.0f)
		return updater_error(error, YT_RANGE,
		    "planet updater current day domain");

	updater_begin(state, YT_PLANET_UPDATER_OPENING_RECORD_EXPRESSION);
	if (!ops->record_expression(context, false, error))
		return false;
	state->physical_record = updater_record_number(state->planet_offset_raw,
	    state->logical_planet_raw);
	updater_complete(state);

	updater_begin(state, YT_PLANET_UPDATER_GET);
	if (!ops->get(context, state->physical_record, &state->field, error))
		return false;
	state->field_loaded = true;
	updater_complete(state);
	if (!updater_validate_record(&state->field, error))
		return false;

	for (index = 1U; index <= 3U; ++index)
		p[index] = yt_record_get_number(&state->field,
		    YT_F45 + (index - 1U) * 4U);
	for (index = 1U; index <= 9U; ++index)
		updater_promote_single(state->field.bytes
		    + quantity_offsets[index - 1U], q_raw[index]);
	sum = updater_single_add(updater_single_add(p[1], p[2]), p[3]);
	p[4] = floorf(sum);
	sum = updater_single_add(updater_single_add(p[1], p[2]), p[3]);
	p[5] = floorf(updater_single_div(sum,
	    qb_mbf32_decode(updater_missile_divisor_s)));
	sum = updater_single_add(updater_single_add(p[1], p[2]), p[3]);
	p[6] = floorf(updater_single_div(sum,
	    qb_mbf32_decode(updater_mine_divisor_s)));
	sum = updater_single_add(updater_single_add(p[1], p[2]), p[3]);
	p[9] = floorf(updater_single_mul(sum,
	    qb_mbf32_decode(updater_plasma_rate_s)));

	updater_begin(state, YT_PLANET_UPDATER_TIMER);
	if (!ops->timer(context, state->timer_seconds_raw, error))
		return false;
	updater_complete(state);
	timer_seconds = qb_mbf32_decode(state->timer_seconds_raw);
	if (timer_seconds < 0.0f)
		return updater_error(error, YT_RANGE,
		    "planet updater TIMER domain");
	current_minute = updater_single_div(timer_seconds,
	    qb_mbf32_decode(updater_sixty_s));
	stored_day = yt_record_get_number(&state->field, YT_F41);
	stored_minute = yt_record_get_number(&state->field, YT_F89);
	elapsed = updater_single_add(
	    updater_single_sub(current_day, stored_day),
	    updater_single_div(updater_single_sub(current_minute, stored_minute),
	    qb_mbf32_decode(updater_minutes_per_day_s)));
	if (elapsed > qb_mbf32_decode(updater_ten_s) || elapsed < 0.0f)
		elapsed = qb_mbf32_decode(updater_ten_s);

	if (!updater_contribution(q_raw[7], updater_ten_thousand_d, false,
	    a_raw[1], error, "planet updater ore contribution")
	    || !updater_contribution(q_raw[7], updater_twenty_thousand_d,
	    false, a_raw[2], error, "planet updater organics contribution")
	    || !updater_contribution(q_raw[7], updater_thirty_thousand_d,
	    false, a_raw[3], error, "planet updater equipment contribution")
	    || !updater_contribution(q_raw[7], updater_five_hundred_d, false,
	    a_raw[4], error, "planet updater fighter contribution")
	    || !updater_contribution(q_raw[7], updater_one_e_minus_five_d, true,
	    a_raw[5], error, "planet updater missile contribution")
	    || !updater_contribution(q_raw[7], updater_four_e_minus_six_d, true,
	    a_raw[6], error, "planet updater mine contribution")
	    || !updater_contribution(q_raw[7], updater_ten_thousand_d, false,
	    a_raw[8], error, "planet updater force contribution")
	    || !updater_contribution(q_raw[7], updater_four_e_minus_eight_d,
	    true, a_raw[9], error, "planet updater plasma contribution"))
		return false;
	for (index = 1U; index <= 9U; ++index)
		a[index] = qb_mbf32_decode(a_raw[index]);

	if (!updater_encode_single(elapsed, elapsed_raw, error,
	    "planet updater elapsed MBF32"))
		return false;
	updater_promote_single(elapsed_raw, elapsed_double);
	if (!updater_encode_single(updater_single_mul(elapsed, one_percent),
	    fraction_raw, error, "planet updater bank fraction"))
		return false;
	updater_promote_single(fraction_raw, fraction_double);
	if (q_raw[7][7] != 0U) {
		if (!updater_raw_binary(qb_mbf64_mul_raw, q_raw[7],
		    fraction_double, scratch, error, "planet updater bank growth")
		    || !updater_raw_binary(qb_mbf64_add_raw, q_raw[7], scratch,
		    scratch_two, error, "planet updater bank total"))
			return false;
		memcpy(scratch, scratch_two, 8U);
	}
	else
		memcpy(scratch, q_raw[7], 8U);
	if (!updater_int(scratch, q_raw[7], error,
	    "planet updater bank INT"))
		return false;

	if (!updater_raw_binary(qb_mbf64_mul_raw, elapsed_double, q_raw[8],
	    scratch, error, "planet updater force growth elapsed")
	    || !updater_raw_binary(qb_mbf64_mul_raw, scratch,
	    updater_one_percent_d, scratch_two, error,
	    "planet updater force growth rate")
	    || !updater_encode_single(updater_single_mul(a[8], elapsed),
	    increment_raw, error, "planet updater force reinforcement"))
		return false;
	updater_promote_single(increment_raw, increment_double);
	if (q_raw[8][7] == 0U && increment_double[7] == 0U)
		memcpy(scratch, q_raw[8], 8U);
	else {
		if (!updater_raw_binary(qb_mbf64_add_raw, q_raw[8], scratch_two,
		    scratch, error, "planet updater force growth total")
		    || !updater_raw_binary(qb_mbf64_add_raw, scratch,
		    increment_double, scratch_two, error,
		    "planet updater force reinforcement total"))
			return false;
		memcpy(scratch, scratch_two, 8U);
	}
	if (!updater_int(scratch, q_raw[8], error,
	    "planet updater force INT"))
		return false;

	for (index = 1U; index <= 3U; ++index)
		p[index] = updater_single_add(p[index], updater_single_mul(
		    updater_single_mul(p[index], elapsed), one_percent));
	for (index = 1U; index <= 6U; ++index) {
		p[index] = updater_single_add(p[index], a[index]);
		if (!updater_encode_single(updater_single_mul(p[index], elapsed),
		    increment_raw, error, "planet updater production increment"))
			return false;
		updater_promote_single(increment_raw, increment_double);
		if (increment_double[7] != 0U) {
			if (!updater_raw_binary(qb_mbf64_add_raw, q_raw[index],
			    increment_double, scratch, error,
			    "planet updater quantity total"))
				return false;
			memcpy(q_raw[index], scratch, 8U);
		}
		if (index <= 3U) {
			if (!updater_encode_single(updater_single_mul(p[index],
			    qb_mbf32_decode(updater_ten_s)), threshold_raw, error,
			    "planet updater commodity threshold"))
				return false;
			updater_promote_single(threshold_raw, threshold_double);
			if (updater_compare_nonnegative(q_raw[index],
			    threshold_double) > 0) {
				updater_promote_single(a_raw[index], increment_double);
				if (!updater_raw_binary(qb_mbf64_div_raw,
				    q_raw[index], updater_ten_d, scratch, error,
				    "planet updater commodity catchup division")
				    || !updater_raw_binary(qb_mbf64_add_raw, scratch,
				    increment_double, scratch_two, error,
				    "planet updater commodity catchup addition")
				    || !updater_csng(scratch_two, p_raw[index], error,
				    "planet updater commodity catchup CSNG"))
					return false;
				p[index] = qb_mbf32_decode(p_raw[index]);
			}
		}
	}
	p[9] = updater_single_add(p[9], a[9]);
	if (!updater_encode_single(updater_single_mul(p[9], elapsed),
	    increment_raw, error, "planet updater plasma increment"))
		return false;
	updater_promote_single(increment_raw, increment_double);
	if (increment_double[7] != 0U) {
		if (!updater_raw_binary(qb_mbf64_add_raw, q_raw[9],
		    increment_double, scratch, error,
		    "planet updater plasma total"))
			return false;
		memcpy(q_raw[9], scratch, 8U);
	}

	memcpy(float_residue, q_raw[9] + 4U, 4U);
	for (index = 1U; index <= 3U; ++index) {
		if (!updater_encode_single(p[index], p_raw[index], error,
		    "planet updater base production MBF32")
		    || !updater_subtract_single(p_raw[index], a_raw[index],
		    float_residue, persisted[index], error))
			return false;
		memcpy(float_residue, persisted[index], 4U);
	}
	memcpy(persisted[0], state->current_day_raw, 4U);
	if (!updater_csng(q_raw[1], persisted[4], error,
	    "planet updater stock ore CSNG")
	    || !updater_csng(q_raw[2], persisted[5], error,
	    "planet updater stock organics CSNG")
	    || !updater_csng(q_raw[3], persisted[6], error,
	    "planet updater stock equipment CSNG")
	    || !updater_csng(q_raw[5], persisted[7], error,
	    "planet updater missiles CSNG")
	    || !updater_csng(q_raw[8], persisted[8], error,
	    "planet updater forces CSNG")
	    || !updater_encode_single(current_minute, persisted[9], error,
	    "planet updater minute MBF32")
	    || !updater_csng(q_raw[9], persisted[10], error,
	    "planet updater plasma CSNG")
	    || !updater_csng(q_raw[7], persisted[11], error,
	    "planet updater bank CSNG")
	    || !updater_csng(q_raw[6], persisted[12], error,
	    "planet updater mines CSNG")
	    || !updater_csng(q_raw[4], persisted[13], error,
	    "planet updater fighters CSNG"))
		return false;

	for (index = 0U; index < YT_ARRAY_LEN(updater_lset_stages); ++index) {
		updater_begin(state, updater_lset_stages[index]);
		if (!ops->lset(context, updater_lset_stages[index],
		    updater_lset_offsets[index], persisted[index], error))
			return false;
		memcpy(state->field.bytes + updater_lset_offsets[index],
		    persisted[index], 4U);
		state->field_dirty = true;
		updater_complete(state);
	}

	updater_begin(state, YT_PLANET_UPDATER_CLOSING_RECORD_EXPRESSION);
	if (!ops->record_expression(context, true, error))
		return false;
	closing_record = updater_record_number(state->planet_offset_raw,
	    state->logical_planet_raw);
	if (closing_record != state->physical_record)
		return updater_error(error, YT_RANGE,
		    "planet updater closing record changed");
	updater_complete(state);

	updater_begin(state, YT_PLANET_UPDATER_PUT);
	if (!ops->put(context, state->physical_record, &state->field, error))
		return false;
	state->field_dirty = false;
	state->written = true;
	updater_complete(state);

	state->cache.current_day = current_day;
	state->cache.current_minute = current_minute;
	state->cache.elapsed = elapsed;
	for (index = 0U; index < 10U; ++index) {
		state->cache.production[index] = p[index];
		state->cache.quantity[index] = qb_mbf64_decode(q_raw[index]);
		state->cache.contribution[index] = a[index];
		memcpy(state->cache.quantity_raw[index], q_raw[index], 8U);
	}
	return true;
}
