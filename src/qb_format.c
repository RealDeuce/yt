#include "qb.h"

#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Enough for every MBF value after the largest decimal rescaling below. */
#define QB_BIG_LIMBS 32

_Static_assert(CHAR_BIT == 8, "Yankee Trader requires 8-bit bytes");
_Static_assert(sizeof(float) == 4 && FLT_RADIX == 2 && FLT_MANT_DIG == 24,
    "Yankee Trader requires IEEE binary32");
_Static_assert(sizeof(double) == 8 && DBL_MANT_DIG == 53,
    "Yankee Trader requires IEEE binary64");

struct qb_big {
	uint32_t limb[QB_BIG_LIMBS];
	size_t used;
};

static bool big_add(struct qb_big *left, const struct qb_big *right);
static bool rounded_quotient(const struct qb_big *numerator,
    const struct qb_big *denominator, uint64_t maximum, uint64_t *result);

struct mbf64_parts {
	uint64_t significand;
	int binary_shift;
	bool negative;
};

static void
big_normalize(struct qb_big *value)
{
	while (value->used > 0 && value->limb[value->used - 1U] == 0)
		--value->used;
}

static struct qb_big
big_from_u64(uint64_t source)
{
	struct qb_big value = {{0}, 0};

	if (source != 0) {
		value.limb[0] = (uint32_t)source;
		value.limb[1] = (uint32_t)(source >> 32);
		value.used = value.limb[1] != 0 ? 2U : 1U;
	}
	return value;
}

static int
big_compare(const struct qb_big *left, const struct qb_big *right)
{
	size_t index;

	if (left->used != right->used)
		return left->used < right->used ? -1 : 1;
	index = left->used;
	while (index > 0) {
		--index;
		if (left->limb[index] != right->limb[index])
			return left->limb[index] < right->limb[index] ? -1 : 1;
	}
	return 0;
}

static bool
big_multiply_small(struct qb_big *value, uint32_t factor)
{
	uint64_t carry = 0;
	size_t index;

	for (index = 0; index < value->used; ++index) {
		uint64_t product = (uint64_t)value->limb[index] * factor + carry;

		value->limb[index] = (uint32_t)product;
		carry = product >> 32;
	}
	if (carry != 0) {
		if (value->used == QB_BIG_LIMBS)
			return false;
		value->limb[value->used++] = (uint32_t)carry;
	}
	return true;
}

static bool
big_add_small(struct qb_big *value, uint32_t addend)
{
	struct qb_big added = big_from_u64(addend);

	return big_add(value, &added);
}

static bool
big_shift_left(struct qb_big *value, unsigned bits)
{
	while (bits-- > 0) {
		if (!big_multiply_small(value, 2))
			return false;
	}
	return true;
}

static bool
big_multiply_power10(struct qb_big *value, unsigned power)
{
	while (power-- > 0) {
		if (!big_multiply_small(value, 10))
			return false;
	}
	return true;
}

static bool
big_add(struct qb_big *left, const struct qb_big *right)
{
	uint64_t carry = 0;
	size_t count = left->used > right->used ? left->used : right->used;
	size_t index;

	if (count > QB_BIG_LIMBS)
		return false;
	for (index = 0; index < count; ++index) {
		uint64_t sum = carry;

		if (index < left->used)
			sum += left->limb[index];
		if (index < right->used)
			sum += right->limb[index];
		left->limb[index] = (uint32_t)sum;
		carry = sum >> 32;
	}
	left->used = count;
	if (carry != 0) {
		if (left->used == QB_BIG_LIMBS)
			return false;
		left->limb[left->used++] = (uint32_t)carry;
	}
	return true;
}

static bool
big_multiply_u64(const struct qb_big *value, uint64_t factor,
    struct qb_big *result)
{
	struct qb_big term = *value;

	memset(result, 0, sizeof(*result));
	while (factor != 0) {
		if ((factor & 1U) != 0 && !big_add(result, &term))
			return false;
		factor >>= 1;
		if (factor != 0 && !big_shift_left(&term, 1))
			return false;
	}
	return true;
}

static void
big_subtract(struct qb_big *left, const struct qb_big *right)
{
	uint64_t borrow = 0;
	size_t index;

	for (index = 0; index < left->used; ++index) {
		uint64_t subtrahend = borrow;
		uint64_t original = left->limb[index];

		if (index < right->used)
			subtrahend += right->limb[index];
		left->limb[index] = (uint32_t)(original - subtrahend);
		borrow = original < subtrahend;
	}
	big_normalize(left);
}

static unsigned
bit_length_u64(uint64_t value)
{
	unsigned bits = 0;

	while (value != 0) {
		++bits;
		value >>= 1;
	}
	return bits;
}

static unsigned
big_bit_length(const struct qb_big *value)
{
	if (value->used == 0)
		return 0;
	return (unsigned)((value->used - 1U) * 32U)
	    + bit_length_u64(value->limb[value->used - 1U]);
}

static int
compare_ratio_power2(const struct qb_big *numerator,
    const struct qb_big *denominator, int exponent)
{
	struct qb_big left = *numerator;
	struct qb_big right = *denominator;

	if (exponent >= 0)
		(void)big_shift_left(&right, (unsigned)exponent);
	else
		(void)big_shift_left(&left, (unsigned)-exponent);
	return big_compare(&left, &right);
}

static bool
ratio_to_mbf64(const struct qb_big *numerator,
    const struct qb_big *denominator, bool negative, uint8_t raw[8])
{
	struct qb_big scaled_numerator = *numerator;
	struct qb_big scaled_denominator = *denominator;
	uint64_t significand;
	uint64_t fraction;
	int exponent;
	int shift;
	size_t index;

	memset(raw, 0, 8);
	if (numerator->used == 0)
		return true;
	exponent = (int)big_bit_length(numerator)
	    - (int)big_bit_length(denominator);
	if (compare_ratio_power2(numerator, denominator, exponent) < 0)
		--exponent;
	if (exponent < -128)
		return true;
	if (exponent > 126)
		return false;
	shift = 55 - exponent;
	if (shift >= 0)
		(void)big_shift_left(&scaled_numerator, (unsigned)shift);
	else
		(void)big_shift_left(&scaled_denominator, (unsigned)-shift);
	if (!rounded_quotient(&scaled_numerator, &scaled_denominator,
	    UINT64_C(0x100000000000000), &significand))
		return false;
	if (significand == UINT64_C(0x100000000000000)) {
		significand >>= 1;
		if (++exponent > 126)
			return false;
	}
	fraction = significand - UINT64_C(0x80000000000000);
	for (index = 0; index < 6U; ++index)
		raw[index] = (uint8_t)(fraction >> (index * 8U));
	raw[6] = (uint8_t)(fraction >> 48);
	if (negative)
		raw[6] |= 0x80U;
	raw[7] = (uint8_t)(exponent + 129);
	return true;
}

static struct mbf64_parts
mbf64_parts(const uint8_t raw[8])
{
	struct mbf64_parts result = {0, 0, false};
	size_t index;

	if (raw[7] == 0U)
		return result;
	result.significand = UINT64_C(0x80000000000000);
	for (index = 0; index < 6U; ++index)
		result.significand |= (uint64_t)raw[index] << (index * 8U);
	result.significand |= (uint64_t)(raw[6] & 0x7fU) << 48;
	result.binary_shift = (int)raw[7] - 129 - 55;
	result.negative = (raw[6] & 0x80U) != 0U;
	return result;
}

static enum qb_mbf_status
scaled_big_to_mbf64(const struct qb_big *magnitude, int binary_shift,
    bool negative, uint8_t raw[8])
{
	struct qb_big numerator = *magnitude;
	struct qb_big denominator = big_from_u64(1);

	if (magnitude->used == 0U) {
		memset(raw, 0, 8U);
		return QB_MBF_OK;
	}
	if (binary_shift >= 0) {
		if (!big_shift_left(&numerator, (unsigned)binary_shift))
			return QB_MBF_OVERFLOW;
	}
	else if (!big_shift_left(&denominator, (unsigned)-binary_shift))
		return QB_MBF_UNDERFLOW;
	if (!ratio_to_mbf64(&numerator, &denominator, negative, raw))
		return QB_MBF_OVERFLOW;
	return raw[7] == 0U ? QB_MBF_UNDERFLOW : QB_MBF_OK;
}

enum qb_mbf_status
qb_mbf64_from_u64(uint64_t value, uint8_t raw[8])
{
	struct qb_big magnitude = big_from_u64(value);

	return scaled_big_to_mbf64(&magnitude, 0, false, raw);
}

enum qb_mbf_status
qb_mbf32_from_mbf64_raw(const uint8_t source[8], uint8_t raw[4])
{
	uint64_t significand = UINT64_C(0x80000000000000);
	uint64_t discarded;
	uint32_t rounded;
	unsigned exponent;
	size_t index;

	if (source == NULL || raw == NULL)
		return QB_MBF_DOMAIN;
	if (source[7] == 0U) {
		memcpy(raw, source + 4U, 4U);
		return QB_MBF_OK;
	}
	for (index = 0U; index < 6U; ++index)
		significand |= (uint64_t)source[index] << (index * 8U);
	significand |= (uint64_t)(source[6] & 0x7fU) << 48U;
	discarded = significand & UINT64_C(0xffffffff);
	rounded = (uint32_t)(significand >> 32U);
	if (discarded > UINT64_C(0x80000000)
	    || (discarded == UINT64_C(0x80000000)
	    && (rounded & 1U) != 0U))
		++rounded;
	exponent = source[7];
	if (rounded == UINT32_C(0x01000000)) {
		rounded >>= 1U;
		if (exponent == 255U)
			return QB_MBF_OVERFLOW;
		++exponent;
	}
	rounded &= UINT32_C(0x007fffff);
	raw[0] = (uint8_t)rounded;
	raw[1] = (uint8_t)(rounded >> 8U);
	raw[2] = (uint8_t)(rounded >> 16U);
	if ((source[6] & 0x80U) != 0U)
		raw[2] |= 0x80U;
	raw[3] = (uint8_t)exponent;
	return QB_MBF_OK;
}

enum qb_mbf_status
qb_mbf64_add_raw(const uint8_t left_raw[8], const uint8_t right_raw[8],
    uint8_t raw[8])
{
	struct mbf64_parts left;
	struct mbf64_parts right;
	struct qb_big left_magnitude;
	struct qb_big right_magnitude;
	struct qb_big result;
	int binary_shift;
	bool negative;
	int comparison;

	if (left_raw == NULL || right_raw == NULL || raw == NULL)
		return QB_MBF_DOMAIN;
	left = mbf64_parts(left_raw);
	right = mbf64_parts(right_raw);
	if (left.significand == 0U) {
		memcpy(raw, right_raw, 8U);
		if (raw[7] == 0U)
			memset(raw, 0, 8U);
		return QB_MBF_OK;
	}
	if (right.significand == 0U) {
		memcpy(raw, left_raw, 8U);
		return QB_MBF_OK;
	}
	binary_shift = left.binary_shift < right.binary_shift
	    ? left.binary_shift : right.binary_shift;
	left_magnitude = big_from_u64(left.significand);
	right_magnitude = big_from_u64(right.significand);
	if (!big_shift_left(&left_magnitude,
	    (unsigned)(left.binary_shift - binary_shift))
	    || !big_shift_left(&right_magnitude,
	    (unsigned)(right.binary_shift - binary_shift)))
		return QB_MBF_OVERFLOW;
	if (left.negative == right.negative) {
		result = left_magnitude;
		if (!big_add(&result, &right_magnitude))
			return QB_MBF_OVERFLOW;
		negative = left.negative;
	}
	else {
		comparison = big_compare(&left_magnitude, &right_magnitude);
		if (comparison == 0) {
			memset(raw, 0, 8U);
			return QB_MBF_OK;
		}
		if (comparison > 0) {
			result = left_magnitude;
			big_subtract(&result, &right_magnitude);
			negative = left.negative;
		}
		else {
			result = right_magnitude;
			big_subtract(&result, &left_magnitude);
			negative = right.negative;
		}
	}
	return scaled_big_to_mbf64(&result, binary_shift, negative, raw);
}

enum qb_mbf_status
qb_mbf64_mul_raw(const uint8_t left_raw[8], const uint8_t right_raw[8],
    uint8_t raw[8])
{
	struct mbf64_parts left;
	struct mbf64_parts right;
	struct qb_big left_magnitude;
	struct qb_big product;

	if (left_raw == NULL || right_raw == NULL || raw == NULL)
		return QB_MBF_DOMAIN;
	left = mbf64_parts(left_raw);
	right = mbf64_parts(right_raw);
	if (left.significand == 0U || right.significand == 0U) {
		memset(raw, 0, 8U);
		return QB_MBF_OK;
	}
	left_magnitude = big_from_u64(left.significand);
	if (!big_multiply_u64(&left_magnitude, right.significand, &product))
		return QB_MBF_OVERFLOW;
	return scaled_big_to_mbf64(&product,
	    left.binary_shift + right.binary_shift,
	    left.negative != right.negative, raw);
}

enum qb_mbf_status
qb_mbf64_div_raw(const uint8_t numerator_raw[8],
    const uint8_t denominator_raw[8], uint8_t raw[8])
{
	struct mbf64_parts numerator;
	struct mbf64_parts denominator;
	struct qb_big numerator_big;
	struct qb_big denominator_big;
	int binary_shift;

	if (numerator_raw == NULL || denominator_raw == NULL || raw == NULL)
		return QB_MBF_DOMAIN;
	numerator = mbf64_parts(numerator_raw);
	denominator = mbf64_parts(denominator_raw);
	if (denominator.significand == 0U)
		return QB_MBF_DOMAIN;
	if (numerator.significand == 0U) {
		memset(raw, 0, 8U);
		return QB_MBF_OK;
	}
	numerator_big = big_from_u64(numerator.significand);
	denominator_big = big_from_u64(denominator.significand);
	binary_shift = numerator.binary_shift - denominator.binary_shift;
	if (binary_shift >= 0) {
		if (!big_shift_left(&numerator_big, (unsigned)binary_shift))
			return QB_MBF_OVERFLOW;
	}
	else if (!big_shift_left(&denominator_big, (unsigned)-binary_shift))
		return QB_MBF_UNDERFLOW;
	if (!ratio_to_mbf64(&numerator_big, &denominator_big,
	    numerator.negative != denominator.negative, raw))
		return QB_MBF_OVERFLOW;
	return raw[7] == 0U ? QB_MBF_UNDERFLOW : QB_MBF_OK;
}

static uint64_t
integer_sqrt_u64(uint64_t value)
{
	uint64_t result = 0;
	uint64_t bit = UINT64_C(1) << 62;

	while (bit > value)
		bit >>= 2;
	while (bit != 0U) {
		if (value >= result + bit) {
			value -= result + bit;
			result = (result >> 1) + bit;
		}
		else
			result >>= 1;
		bit >>= 2;
	}
	return result;
}

static enum qb_mbf_status
mbf32_sqrt_positive_raw(const uint8_t operand[4], uint8_t raw[4])
{
	uint32_t significand;
	uint64_t radicand;
	uint64_t root;
	uint64_t midpoint;
	int exponent;
	int result_exponent;
	unsigned shift;

	if (operand[3] == 0U) {
		memset(raw, 0, 4U);
		return QB_MBF_OK;
	}
	if ((operand[2] & 0x80U) != 0U)
		return QB_MBF_DOMAIN;
	significand = UINT32_C(0x800000) | (uint32_t)operand[0]
	    | ((uint32_t)operand[1] << 8)
	    | ((uint32_t)(operand[2] & 0x7fU) << 16);
	exponent = (int)operand[3] - 129;
	result_exponent = exponent >= 0 ? exponent / 2
	    : -((-exponent + 1) / 2);
	shift = (unsigned)(23 + exponent - 2 * result_exponent);
	radicand = (uint64_t)significand << shift;
	root = integer_sqrt_u64(radicand);
	midpoint = 2U * root + 1U;
	if (4U * radicand > midpoint * midpoint
	    || (4U * radicand == midpoint * midpoint && (root & 1U) != 0U))
		++root;
	if (root == UINT64_C(0x1000000)) {
		root >>= 1;
		++result_exponent;
	}
	if (result_exponent < -128) {
		memset(raw, 0, 4U);
		return QB_MBF_UNDERFLOW;
	}
	if (result_exponent > 126)
		return QB_MBF_OVERFLOW;
	root -= UINT64_C(0x800000);
	raw[0] = (uint8_t)root;
	raw[1] = (uint8_t)(root >> 8);
	raw[2] = (uint8_t)(root >> 16);
	raw[3] = (uint8_t)(result_exponent + 129);
	return QB_MBF_OK;
}

enum qb_mbf_status
qb_mbf64_sqrt_raw(const uint8_t operand[8], uint8_t raw[8])
{
	uint8_t seed_single[4];
	uint8_t current[8] = {0};
	uint8_t quotient[8];
	uint8_t total[8];
	enum qb_mbf_status status;
	unsigned pass;

	if (operand == NULL || raw == NULL)
		return QB_MBF_DOMAIN;
	if (operand[7] == 0U) {
		/* BRUN clears only the exponent cell on the zero SQR lane. */
		memcpy(raw, operand, 8U);
		raw[7] = 0U;
		return QB_MBF_OK;
	}
	if ((operand[6] & 0x80U) != 0U)
		return QB_MBF_DOMAIN;
	status = mbf32_sqrt_positive_raw(operand + 4U, seed_single);
	if (status != QB_MBF_OK)
		return status;
	memcpy(current + 4U, seed_single, 4U);
	for (pass = 0; pass < 2U; ++pass) {
		status = qb_mbf64_div_raw(operand, current, quotient);
		if (status != QB_MBF_OK)
			return status;
		status = qb_mbf64_add_raw(quotient, current, total);
		if (status != QB_MBF_OK)
			return status;
		if (total[7] <= 1U) {
			memset(current, 0, 8U);
			return QB_MBF_UNDERFLOW;
		}
		memcpy(current, total, 8U);
		--current[7];
	}
	memcpy(raw, current, 8U);
	return QB_MBF_OK;
}

enum qb_mbf_status
qb_mbf64_floor_positive_raw(const uint8_t operand[8], uint8_t raw[8])
{
	struct mbf64_parts value;
	int exponent;
	uint64_t integer;

	if (operand == NULL || raw == NULL)
		return QB_MBF_DOMAIN;
	value = mbf64_parts(operand);
	if (value.negative)
		return QB_MBF_DOMAIN;
	if (value.significand == 0U) {
		memset(raw, 0, 8U);
		return QB_MBF_OK;
	}
	exponent = (int)operand[7] - 129;
	if (exponent < 0) {
		memset(raw, 0, 8U);
		return QB_MBF_OK;
	}
	if (exponent >= 55) {
		memcpy(raw, operand, 8U);
		return QB_MBF_OK;
	}
	integer = value.significand >> (unsigned)(55 - exponent);
	return qb_mbf64_from_u64(integer, raw);
}

enum qb_mbf_status
qb_mbf64_int_positive_raw(const uint8_t operand[8], uint8_t raw[8])
{
	if (operand == NULL || raw == NULL)
		return QB_MBF_DOMAIN;
	if (operand[7] != 0U && (operand[6] & 0x80U) != 0U)
		return QB_MBF_DOMAIN;
	memcpy(raw, operand, 8U);
	if (operand[7] == 0U)
		return QB_MBF_OK;
	if (operand[7] < 129U) {
		raw[7] = 0U;
		return QB_MBF_OK;
	}
	return qb_mbf64_floor_positive_raw(operand, raw);
}

static bool
binary_in_mbf_range(uint64_t mantissa, int binary_exponent)
{
	int exponent;

	if (mantissa == 0)
		return true;
	exponent = binary_exponent + (int)bit_length_u64(mantissa) - 1;
	return exponent >= -128 && exponent <= 126;
}

/* Compare mantissa * 2^binary_exponent with 10^decimal_exponent. */
static int
compare_power10(uint64_t mantissa, int binary_exponent,
    int decimal_exponent)
{
	struct qb_big left = big_from_u64(mantissa);
	struct qb_big right = big_from_u64(1);

	if (binary_exponent >= 0)
		(void)big_shift_left(&left, (unsigned)binary_exponent);
	else
		(void)big_shift_left(&right, (unsigned)-binary_exponent);
	if (decimal_exponent >= 0)
		(void)big_multiply_power10(&right,
		    (unsigned)decimal_exponent);
	else
		(void)big_multiply_power10(&left,
		    (unsigned)-decimal_exponent);
	return big_compare(&left, &right);
}

static uint64_t
power10_u64(unsigned power)
{
	uint64_t value = 1;

	while (power-- > 0)
		value *= 10U;
	return value;
}

static bool
rounded_quotient(const struct qb_big *numerator,
    const struct qb_big *denominator, uint64_t maximum, uint64_t *result)
{
	uint64_t low = 0;
	uint64_t high = maximum;
	struct qb_big product;

	while (low < high) {
		uint64_t middle = low + (high - low + 1U) / 2U;

		if (!big_multiply_u64(denominator, middle, &product))
			return false;
		if (big_compare(&product, numerator) <= 0)
			low = middle;
		else
			high = middle - 1U;
	}
	if (!big_multiply_u64(denominator, low, &product))
		return false;
	{
		struct qb_big remainder = *numerator;
		int comparison;

		big_subtract(&remainder, &product);
		if (!big_multiply_small(&remainder, 2))
			return false;
		comparison = big_compare(&remainder, denominator);
		if (comparison > 0 || (comparison == 0 && (low & 1U) != 0))
			++low;
	}
	*result = low;
	return true;
}

static int
copy_result(char *dest, size_t size, const char *source, size_t length)
{
	if (length > (size_t)INT_MAX)
		return -1;
	if (size != 0) {
		size_t amount = length < size - 1U ? length : size - 1U;

		memcpy(dest, source, amount);
		dest[amount] = '\0';
	}
	return (int)length;
}

static int
format_binary(char *dest, size_t size, bool negative, uint64_t mantissa,
    int binary_exponent, bool single)
{
	unsigned precision = single ? 7U : 16U;
	unsigned guard_precision = single ? 9U : 18U;
	uint64_t guard_limit = power10_u64(guard_precision);
	uint64_t guard_value;
	uint64_t retained = 0;
	struct qb_big numerator = big_from_u64(mantissa);
	struct qb_big denominator = big_from_u64(1);
	char guard[24];
	char digits[24];
	char rendered[128];
	size_t used = 0;
	size_t digit_length;
	int decimal_exponent;
	int scale;
	int position;
	int displacement;
	unsigned index;

	if (mantissa == 0)
		return copy_result(dest, size, " 0", 2);
	if (!binary_in_mbf_range(mantissa, binary_exponent))
		return -1;

	decimal_exponent = (int)floor(log10(ldexp((double)mantissa,
	    binary_exponent)));
	while (compare_power10(mantissa, binary_exponent,
	    decimal_exponent) < 0)
		--decimal_exponent;
	while (compare_power10(mantissa, binary_exponent,
	    decimal_exponent + 1) >= 0)
		++decimal_exponent;

	if (binary_exponent >= 0)
		(void)big_shift_left(&numerator, (unsigned)binary_exponent);
	else
		(void)big_shift_left(&denominator, (unsigned)-binary_exponent);
	scale = decimal_exponent - (int)guard_precision + 1;
	if (scale >= 0)
		(void)big_multiply_power10(&denominator, (unsigned)scale);
	else
		(void)big_multiply_power10(&numerator, (unsigned)-scale);
	if (!rounded_quotient(&numerator, &denominator, guard_limit,
	    &guard_value))
		return -1;
	if (guard_value == guard_limit) {
		guard_value /= 10U;
		++decimal_exponent;
	}
	if (snprintf(guard, sizeof(guard), "%0*" PRIu64,
	    (int)guard_precision, guard_value) != (int)guard_precision)
		return -1;
	for (index = 0; index < precision; ++index)
		retained = retained * 10U + (uint64_t)(guard[index] - '0');
	if (guard[precision] >= '5') {
		++retained;
		if (retained == power10_u64(precision)) {
			retained /= 10U;
			++decimal_exponent;
		}
	}
	while (retained >= 10U && retained % 10U == 0)
		retained /= 10U;
	if (snprintf(digits, sizeof(digits), "%" PRIu64, retained) < 1)
		return -1;
	digit_length = strlen(digits);
	position = decimal_exponent + 1;
	displacement = position - (int)digit_length;
	rendered[used++] = negative ? '-' : ' ';
	if (abs(displacement) > (int)precision || position > (int)precision) {
		char exponent[16];
		int exponent_length;

		rendered[used++] = digits[0];
		if (digit_length > 1U) {
			rendered[used++] = '.';
			memcpy(rendered + used, digits + 1, digit_length - 1U);
			used += digit_length - 1U;
		}
		rendered[used++] = single ? 'E' : 'D';
		exponent_length = snprintf(exponent, sizeof(exponent), "%c%02d",
		    decimal_exponent < 0 ? '-' : '+', abs(decimal_exponent));
		if (exponent_length < 0
		    || (size_t)exponent_length >= sizeof(exponent))
			return -1;
		memcpy(rendered + used, exponent, (size_t)exponent_length);
		used += (size_t)exponent_length;
	}
	else if (position <= 0) {
		rendered[used++] = '.';
		while (position++ < 0)
			rendered[used++] = '0';
		memcpy(rendered + used, digits, digit_length);
		used += digit_length;
	}
	else if ((size_t)position >= digit_length) {
		memcpy(rendered + used, digits, digit_length);
		used += digit_length;
		while ((size_t)position > digit_length++)
			rendered[used++] = '0';
	}
	else {
		memcpy(rendered + used, digits, (size_t)position);
		used += (size_t)position;
		rendered[used++] = '.';
		memcpy(rendered + used, digits + position,
		    digit_length - (size_t)position);
		used += digit_length - (size_t)position;
	}
	rendered[used] = '\0';
	return copy_result(dest, size, rendered, used);
}

int
qb_str_integer(char *dest, size_t size, int16_t value)
{
	char rendered[8];
	int length = snprintf(rendered, sizeof(rendered), "%c%u",
	    value < 0 ? '-' : ' ',
	    value < 0 ? (unsigned)(-(int)value) : (unsigned)value);

	if (length < 0)
		return length;
	return copy_result(dest, size, rendered, (size_t)length);
}

int
qb_str_single(char *dest, size_t size, float value)
{
	uint8_t raw[4];
	enum qb_mbf_status status;

	status = qb_mbf32_encode(value, raw);
	if (status == QB_MBF_OVERFLOW)
		return -1;
	return qb_str_mbf32(dest, size, raw);
}

int
qb_str_double(char *dest, size_t size, double value)
{
	uint8_t raw[8];
	enum qb_mbf_status status;

	status = qb_mbf64_encode(value, raw);
	if (status == QB_MBF_OVERFLOW)
		return -1;
	return qb_str_mbf64(dest, size, raw);
}

int
qb_str_mbf32(char *dest, size_t size, const uint8_t raw[4])
{
	uint64_t mantissa;
	bool negative;

	if (raw[3] == 0)
		return copy_result(dest, size, " 0", 2);
	mantissa = UINT64_C(0x800000) | raw[0]
	    | ((uint64_t)raw[1] << 8) | ((uint64_t)(raw[2] & 0x7fU) << 16);
	negative = (raw[2] & 0x80U) != 0;
	return format_binary(dest, size, negative, mantissa,
	    (int)raw[3] - 129 - 23, true);
}

int
qb_str_mbf64(char *dest, size_t size, const uint8_t raw[8])
{
	uint64_t mantissa = UINT64_C(0x80000000000000);
	bool negative;
	size_t index;

	if (raw[7] == 0)
		return copy_result(dest, size, " 0", 2);
	for (index = 0; index < 6; ++index)
		mantissa |= (uint64_t)raw[index] << (index * 8U);
	mantissa |= (uint64_t)(raw[6] & 0x7fU) << 48;
	negative = (raw[6] & 0x80U) != 0;
	return format_binary(dest, size, negative, mantissa,
	    (int)raw[7] - 129 - 55, false);
}

static int
append_print_space(char *dest, size_t size, int length)
{
	if (length < 0)
		return length;
	if (size != 0 && (size_t)length < size - 1U) {
		dest[length] = ' ';
		dest[length + 1] = '\0';
	}
	return length == INT_MAX ? -1 : length + 1;
}

int
qb_print_integer(char *dest, size_t size, int16_t value)
{
	return append_print_space(dest, size,
	    qb_str_integer(dest, size, value));
}

int
qb_print_single(char *dest, size_t size, float value)
{
	return append_print_space(dest, size, qb_str_single(dest, size, value));
}

int
qb_print_double(char *dest, size_t size, double value)
{
	return append_print_space(dest, size, qb_str_double(dest, size, value));
}

int
qb_print_number(char *dest, size_t size, double value)
{
	return qb_print_double(dest, size, value);
}

struct val_reader {
	const uint8_t *data;
	size_t length;
	size_t position;
	bool last_consumed;
};

static uint8_t
val_fetch(struct val_reader *reader, bool skip_decimal_space)
{
	uint8_t value;

	if (skip_decimal_space) {
		while (reader->position < reader->length
		    && (reader->data[reader->position] == 0x09U
		    || reader->data[reader->position] == 0x0aU
		    || reader->data[reader->position] == 0x20U))
			++reader->position;
	}
	if (reader->position == reader->length) {
		reader->last_consumed = false;
		return 0;
	}
	value = reader->data[reader->position++];
	reader->last_consumed = true;
	if (value >= 'a' && value <= 'z')
		value &= 0x5fU;
	return value;
}

static void
val_unread(struct val_reader *reader)
{
	if (reader->last_consumed)
		--reader->position;
	reader->last_consumed = false;
}

static void
val_set_integer(struct qb_val_result *result, int value)
{
	qb_mbf64_encode((double)value, result->mbf);
	result->value = (double)value;
}

static void
val_set_ratio(struct qb_val_result *result, const struct qb_big *numerator,
    const struct qb_big *denominator, bool negative)
{
	if (!ratio_to_mbf64(numerator, denominator, negative, result->mbf)) {
		result->overflow = true;
		return;
	}
	result->value = qb_mbf64_decode(result->mbf);
}

struct qb_val_result
qb_val_n(const uint8_t *text, size_t length)
{
	struct qb_val_result result = {0};
	struct val_reader reader;
	struct qb_big integer = big_from_u64(0);
	uint8_t character;
	bool negative = false;
	bool after_decimal = false;
	bool any_digit = false;
	unsigned significant_digits = 0;
	unsigned fractional_digits = 0;
	int exponent = 0;
	int scale;

	if (text == NULL && length != 0U)
		return result;
	reader.data = text;
	reader.length = length;
	reader.position = 0;
	reader.last_consumed = false;
	character = val_fetch(&reader, true);
	if (character == '&') {
		unsigned base;
		uint32_t word = 0;

		character = val_fetch(&reader, false);
		if (character == 'H') {
			base = 16U;
			character = val_fetch(&reader, false);
		}
		else if (character == 'O') {
			base = 8U;
			character = val_fetch(&reader, false);
		}
		else
			base = 8U;
		result.valid = true;
		while (true) {
			unsigned digit;

			if (character >= '0' && character <= '9')
				digit = (unsigned)(character - '0');
			else if (character >= 'A' && character <= 'F')
				digit = (unsigned)(character - 'A') + 10U;
			else
				break;
			if (digit >= base)
				break;
			if (word > (UINT32_C(0xffff) - digit) / base) {
				result.overflow = true;
				result.consumed = reader.position - 1U;
				return result;
			}
			word = word * base + digit;
			result.consumed = reader.position;
			character = val_fetch(&reader, false);
		}
		val_set_integer(&result,
		    (word & UINT32_C(0x8000)) != 0
		    ? (int)word - 0x10000 : (int)word);
		return result;
	}

	val_unread(&reader);
	character = val_fetch(&reader, true);
	negative = character == '-';
	if (character == '+' || character == '-')
		result.consumed = reader.position;
	else
		val_unread(&reader);
	while (true) {
		character = val_fetch(&reader, true);
		if (character >= '0' && character <= '9') {
			uint32_t digit = (uint32_t)(character - '0');

			if (!big_multiply_small(&integer, 10)
			    || !big_add_small(&integer, digit)) {
				result.overflow = true;
				return result;
			}
			any_digit = true;
			if (significant_digits != 0 || digit != 0)
				++significant_digits;
			if (after_decimal)
				++fractional_digits;
			result.consumed = reader.position;
			continue;
		}
		if (character == '.' && !after_decimal) {
			after_decimal = true;
			result.consumed = reader.position;
			continue;
		}
		val_unread(&reader);
		break;
	}
	character = val_fetch(&reader, true);
	if (character == 'D' || character == 'E') {
		bool exponent_negative;

		result.consumed = reader.position;
		character = val_fetch(&reader, true);
		exponent_negative = character == '-';
		if (character == '+' || character == '-')
			result.consumed = reader.position;
		else
			val_unread(&reader);
		while (true) {
			character = val_fetch(&reader, true);
			if (character < '0' || character > '9') {
				val_unread(&reader);
				break;
			}
			if (exponent < 32767)
				exponent = exponent * 10 + (int)(character - '0');
			if (exponent > 32767)
				exponent = 32767;
			result.consumed = reader.position;
		}
		if (exponent_negative)
			exponent = -exponent;
	}
	result.valid = any_digit;
	scale = exponent - (int)fractional_digits;
	if (scale > 38) {
		result.overflow = true;
		return result;
	}
	if (integer.used == 0)
		return result;
	{
		int decimal_order = (int)significant_digits + scale - 1;
		struct qb_big denominator = big_from_u64(1);

		if (decimal_order < -39)
			return result;
		if (decimal_order > 38) {
			result.overflow = true;
			return result;
		}
		if (scale >= 0) {
			if (!big_multiply_power10(&integer, (unsigned)scale)) {
				result.overflow = true;
				return result;
			}
		}
		else if (!big_multiply_power10(&denominator, (unsigned)-scale)) {
			result.overflow = true;
			return result;
		}
		val_set_ratio(&result, &integer, &denominator, negative);
	}
	return result;
}

struct qb_val_result
qb_val(const char *text)
{
	return qb_val_n((const uint8_t *)text,
	    text == NULL ? 0U : strlen(text));
}
