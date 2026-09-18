#include "yt_score_format.h"

#include "qb.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define BIG_LIMBS 20U

struct big_uint {
	uint32_t limbs[BIG_LIMBS];
};

struct field_spec {
	int integer_columns;
	int fractional_digits;
	bool comma_grouping;
};

static const struct field_spec *
field_spec(enum yt_score_field field)
{
	static const struct field_spec specs[] = {
		{3, 0, false},
		{4, 0, false},
		{3, 2, false},
		{4, 2, false},
		{15, 0, true}
	};

	if ((unsigned)field >= YT_ARRAY_LEN(specs))
		return NULL;
	return &specs[field];
}

static void
big_from_u64(struct big_uint *value, uint64_t source)
{
	memset(value, 0, sizeof(*value));
	value->limbs[0] = (uint32_t)source;
	value->limbs[1] = (uint32_t)(source >> 32);
}

static int
big_compare(const struct big_uint *left, const struct big_uint *right)
{
	size_t index = BIG_LIMBS;

	while (index-- > 0) {
		if (left->limbs[index] < right->limbs[index])
			return -1;
		if (left->limbs[index] > right->limbs[index])
			return 1;
	}
	return 0;
}

static bool
big_multiply_small(struct big_uint *value, uint32_t factor)
{
	uint64_t carry = 0;
	size_t index;

	for (index = 0; index < BIG_LIMBS; ++index) {
		uint64_t product = (uint64_t)value->limbs[index] * factor
		    + carry;

		value->limbs[index] = (uint32_t)product;
		carry = product >> 32;
	}
	return carry == 0;
}

static bool
big_multiply_u64(const struct big_uint *value, uint64_t factor,
    struct big_uint *result)
{
	uint32_t parts[2];
	size_t part_count;
	size_t index;

	memset(result, 0, sizeof(*result));
	parts[0] = (uint32_t)factor;
	parts[1] = (uint32_t)(factor >> 32);
	part_count = parts[1] == 0 ? 1U : 2U;
	for (index = 0; index < BIG_LIMBS; ++index) {
		uint64_t carry = 0;
		size_t part;

		if (value->limbs[index] == 0)
			continue;
		for (part = 0; part < part_count; ++part) {
			size_t target = index + part;
			uint64_t product;

			if (target >= BIG_LIMBS)
				return false;
			product = (uint64_t)value->limbs[index] * parts[part]
			    + result->limbs[target] + carry;
			result->limbs[target] = (uint32_t)product;
			carry = product >> 32;
		}
		{
			size_t target = index + part_count;

			while (carry != 0) {
				uint64_t sum;

				if (target >= BIG_LIMBS)
					return false;
				sum = (uint64_t)result->limbs[target] + carry;
				result->limbs[target] = (uint32_t)sum;
				carry = sum >> 32;
				++target;
			}
		}
	}
	return true;
}

static void
big_subtract(struct big_uint *left, const struct big_uint *right)
{
	uint64_t borrow = 0;
	size_t index;

	for (index = 0; index < BIG_LIMBS; ++index) {
		uint64_t minuend = left->limbs[index];
		uint64_t subtrahend = (uint64_t)right->limbs[index] + borrow;

		left->limbs[index] = (uint32_t)(minuend - subtrahend);
		borrow = minuend < subtrahend;
	}
}

static bool
big_multiply_power10(struct big_uint *value, unsigned power)
{
	while (power-- > 0) {
		if (!big_multiply_small(value, 10))
			return false;
	}
	return true;
}

static int
compare_ratio_power10(const struct big_uint *numerator,
    const struct big_uint *denominator, int exponent)
{
	struct big_uint scaled;

	if (exponent >= 0) {
		scaled = *denominator;
		if (!big_multiply_power10(&scaled, (unsigned)exponent))
			return -1;
		return big_compare(numerator, &scaled);
	}
	scaled = *numerator;
	if (!big_multiply_power10(&scaled, (unsigned)-exponent))
		return 1;
	return big_compare(&scaled, denominator);
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
round_ratio(const struct big_uint *numerator,
    const struct big_uint *denominator, uint64_t limit, bool ties_to_even,
    uint64_t *rounded)
{
	uint64_t low = 0;
	uint64_t high = limit;
	struct big_uint product;
	struct big_uint remainder;
	struct big_uint doubled;
	int comparison;

	while (low + 1U < high) {
		uint64_t middle = low + (high - low) / 2U;

		if (!big_multiply_u64(denominator, middle, &product))
			return false;
		if (big_compare(&product, numerator) <= 0)
			low = middle;
		else
			high = middle;
	}
	if (!big_multiply_u64(denominator, low, &product))
		return false;
	remainder = *numerator;
	big_subtract(&remainder, &product);
	doubled = remainder;
	if (!big_multiply_small(&doubled, 2))
		return false;
	comparison = big_compare(&doubled, denominator);
	if (comparison > 0
	    || (comparison == 0 && (!ties_to_even || (low & 1U) != 0)))
		++low;
	*rounded = low;
	return true;
}

static bool
decode_ratio(const uint8_t *raw, size_t raw_size, bool *negative,
    struct big_uint *numerator, struct big_uint *denominator,
    int *binary_exponent)
{
	unsigned precision;
	uint64_t fraction = 0;
	uint64_t significand;
	int shift;
	size_t index;

	if (raw_size != 4U && raw_size != 8U)
		return false;
	precision = raw_size == 4U ? 24U : 56U;
	*negative = raw[raw_size - 1U] != 0
	    && (raw[raw_size - 2U] & 0x80U) != 0;
	if (raw[raw_size - 1U] == 0) {
		big_from_u64(numerator, 0);
		big_from_u64(denominator, 1);
		*binary_exponent = 0;
		return true;
	}
	for (index = 0; index + 2U < raw_size; ++index)
		fraction |= (uint64_t)raw[index] << (index * 8U);
	fraction |= (uint64_t)(raw[raw_size - 2U] & 0x7fU)
	    << ((raw_size - 2U) * 8U);
	significand = (UINT64_C(1) << (precision - 1U)) + fraction;
	*binary_exponent = (int)raw[raw_size - 1U] - 129;
	shift = *binary_exponent - (int)(precision - 1U);
	big_from_u64(numerator, significand);
	big_from_u64(denominator, 1);
	if (shift >= 0) {
		while (shift-- > 0) {
			if (!big_multiply_small(numerator, 2))
				return false;
		}
	}
	else {
		while (shift++ < 0) {
			if (!big_multiply_small(denominator, 2))
				return false;
		}
	}
	return true;
}

static bool
decimal_digit_stream(const uint8_t *raw, size_t raw_size, bool *negative,
    uint64_t *digits, int *stream_exponent)
{
	struct big_uint numerator;
	struct big_uint denominator;
	struct big_uint scaled_numerator;
	struct big_uint scaled_denominator;
	int binary_exponent;
	int decimal_exponent;
	int scale_exponent;
	/* AA2D emits a guard stream before the field-specific half-up round. */
	unsigned precision = raw_size == 4U ? 9U : 18U;
	uint64_t limit = power10_u64(precision);

	if (!decode_ratio(raw, raw_size, negative, &numerator, &denominator,
	    &binary_exponent))
		return false;
	if (raw[raw_size - 1U] == 0) {
		*negative = false;
		*digits = 0;
		*stream_exponent = 0;
		return true;
	}
	decimal_exponent = (int)floor((double)binary_exponent
	    * 0.30102999566398119521);
	while (compare_ratio_power10(&numerator, &denominator,
	    decimal_exponent) < 0)
		--decimal_exponent;
	while (compare_ratio_power10(&numerator, &denominator,
	    decimal_exponent + 1) >= 0)
		++decimal_exponent;

	scaled_numerator = numerator;
	scaled_denominator = denominator;
	scale_exponent = decimal_exponent - (int)precision + 1;
	if (scale_exponent >= 0) {
		if (!big_multiply_power10(&scaled_denominator,
		    (unsigned)scale_exponent))
			return false;
	}
	else if (!big_multiply_power10(&scaled_numerator,
	    (unsigned)-scale_exponent))
		return false;
	if (!round_ratio(&scaled_numerator, &scaled_denominator, limit,
	    true, digits))
		return false;
	if (*digits == limit) {
		*digits /= 10U;
		++decimal_exponent;
	}
	*stream_exponent = decimal_exponent - (int)precision + 1;
	return true;
}

static bool
rounded_units(char *dest, size_t size, uint64_t digits,
    int stream_exponent, int fractional_digits)
{
	int power = stream_exponent + fractional_digits;
	int written;

	if (power >= 0) {
		size_t length;

		written = snprintf(dest, size, "%" PRIu64, digits);
		if (written < 0 || (size_t)written >= size)
			return false;
		length = (size_t)written;
		if ((size_t)power >= size - length)
			return false;
		memset(dest + length, '0', (size_t)power);
		dest[length + (size_t)power] = '\0';
		return true;
	}
	{
		unsigned divisor_power = (unsigned)-power;
		uint64_t rounded = 0;

		if (divisor_power <= 19U) {
			uint64_t divisor = power10_u64(divisor_power);
			uint64_t remainder;

			rounded = digits / divisor;
			remainder = digits % divisor;
			if (remainder * 2U >= divisor)
				++rounded;
		}
		written = snprintf(dest, size, "%" PRIu64, rounded);
		return written >= 0 && (size_t)written < size;
	}
}

static bool
fixed_magnitude(char *dest, size_t size, const char *units,
    const struct field_spec *spec)
{
	char padded[128];
	size_t units_length = strlen(units);
	size_t padded_length = units_length;
	size_t minimum = (size_t)spec->fractional_digits + 1U;
	size_t index;
	size_t output = 0;

	if (units_length < minimum) {
		size_t padding = minimum - units_length;

		if (minimum >= sizeof(padded))
			return false;
		memset(padded, '0', padding);
		memcpy(padded + padding, units, units_length + 1U);
		padded_length = minimum;
	}
	else {
		if (units_length >= sizeof(padded))
			return false;
		memcpy(padded, units, units_length + 1U);
	}
	if (spec->fractional_digits != 0) {
		size_t integer_length = padded_length
		    - (size_t)spec->fractional_digits;

		if (padded_length + 1U >= size)
			return false;
		memcpy(dest, padded, integer_length);
		dest[integer_length] = '.';
		memcpy(dest + integer_length + 1U, padded + integer_length,
		    (size_t)spec->fractional_digits);
		dest[padded_length + 1U] = '\0';
		return true;
	}
	for (index = 0; index < padded_length; ++index) {
		if (spec->comma_grouping && index != 0
		    && (padded_length - index) % 3U == 0) {
			if (output + 1U >= size)
				return false;
			dest[output++] = ',';
		}
		if (output + 1U >= size)
			return false;
		dest[output++] = padded[index];
	}
	dest[output] = '\0';
	return true;
}

static bool
exponential_magnitude(char *dest, size_t size, const char *units,
    const struct field_spec *spec, size_t raw_size)
{
	char mantissa[32];
	size_t units_length = strlen(units);
	size_t integer_digits = (size_t)spec->integer_columns - 1U;
	size_t significant_digits = integer_digits
	    + (size_t)spec->fractional_digits;
	int decimal_exponent = (int)units_length - 1
	    - spec->fractional_digits;
	int exponent;
	bool carry = false;
	size_t index;
	size_t output = 0;
	unsigned magnitude;
	char letter = raw_size == 4U ? 'E' : 'D';

	if (units_length == 1U && units[0] == '0')
		return false;
	if (significant_digits + 1U > sizeof(mantissa))
		return false;
	for (index = 0; index < significant_digits; ++index)
		mantissa[index] = index < units_length ? units[index] : '0';
	mantissa[significant_digits] = '\0';
	if (units_length > significant_digits
	    && units[significant_digits] >= '5') {
		index = significant_digits;
		carry = true;
		while (index > 0 && carry) {
			--index;
			if (mantissa[index] == '9')
				mantissa[index] = '0';
			else {
				++mantissa[index];
				carry = false;
			}
		}
		if (carry) {
			mantissa[0] = '1';
			memset(mantissa + 1, '0', significant_digits - 1U);
			++decimal_exponent;
		}
	}
	exponent = decimal_exponent - (int)integer_digits + 1;
	for (index = 0; index < significant_digits; ++index) {
		if (index == integer_digits
		    && spec->fractional_digits != 0) {
			if (output + 1U >= size)
				return false;
			dest[output++] = '.';
		}
		if (output + 1U >= size)
			return false;
		dest[output++] = mantissa[index];
	}
	if (output + 5U >= size)
		return false;
	dest[output++] = letter;
	dest[output++] = exponent < 0 ? '-' : '+';
	magnitude = (unsigned)(exponent < 0 ? -exponent : exponent);
	if (magnitude >= 100U) {
		dest[output++] = '%';
		dest[output++] = (char)('0' + magnitude % 10U);
	}
	else {
		dest[output++] = (char)('0' + magnitude / 10U);
		dest[output++] = (char)('0' + magnitude % 10U);
	}
	dest[output] = '\0';
	return true;
}

bool
yt_score_format_mbf(char *dest, size_t size, const uint8_t *raw,
    size_t raw_size, enum yt_score_field field)
{
	const struct field_spec *spec = field_spec(field);
	uint64_t digits;
	int stream_exponent;
	bool negative;
	char units[128];
	char magnitude[160];
	int fixed_width;
	int excess;
	size_t magnitude_length;
	size_t output = 0;

	if (dest == NULL || size == 0 || raw == NULL || spec == NULL
	    || (raw_size != 4U && raw_size != 8U))
		return false;
	if (!decimal_digit_stream(raw, raw_size, &negative, &digits,
	    &stream_exponent))
		return false;
	if (!rounded_units(units, sizeof(units), digits, stream_exponent,
	    spec->fractional_digits))
		return false;
	if (!fixed_magnitude(magnitude, sizeof(magnitude), units, spec))
		return false;
	magnitude_length = strlen(magnitude);
	fixed_width = spec->integer_columns
	    + (spec->fractional_digits == 0
	    ? 0 : spec->fractional_digits + 1);
	excess = (int)magnitude_length + 1 - fixed_width;
	if (excess <= 0) {
		size_t content = magnitude_length + (negative ? 1U : 0U);
		size_t padding;

		if (content > (size_t)fixed_width)
			return false;
		padding = (size_t)fixed_width - content;
		if (padding + content >= size)
			return false;
		memset(dest, ' ', padding);
		output = padding;
		if (negative)
			dest[output++] = '-';
		memcpy(dest + output, magnitude, magnitude_length + 1U);
		return true;
	}
	if (output + 1U >= size)
		return false;
	dest[output++] = '%';
	if (negative) {
		if (output + 1U >= size)
			return false;
		dest[output++] = '-';
	}
	if (excess <= 4) {
		if (magnitude_length >= size - output)
			return false;
		memcpy(dest + output, magnitude, magnitude_length + 1U);
		return true;
	}
	if (!exponential_magnitude(magnitude, sizeof(magnitude), units, spec,
	    raw_size))
		return false;
	magnitude_length = strlen(magnitude);
	if (magnitude_length >= size - output)
		return false;
	memcpy(dest + output, magnitude, magnitude_length + 1U);
	return true;
}

bool
yt_score_format_single(char *dest, size_t size, float value,
    enum yt_score_field field)
{
	uint8_t raw[4];

	if (qb_mbf32_encode(value, raw) == QB_MBF_OVERFLOW)
		return false;
	return yt_score_format_mbf(dest, size, raw, sizeof(raw), field);
}

bool
yt_score_format_double(char *dest, size_t size, double value,
    enum yt_score_field field)
{
	uint8_t raw[8];

	if (qb_mbf64_encode(value, raw) == QB_MBF_OVERFLOW)
		return false;
	return yt_score_format_mbf(dest, size, raw, sizeof(raw), field);
}
