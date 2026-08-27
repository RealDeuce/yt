#include "qb.h"

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(CHAR_BIT == 8, "Yankee Trader requires 8-bit bytes");
_Static_assert(FLT_RADIX == 2, "Yankee Trader requires binary floating point");
_Static_assert(FLT_MANT_DIG == 24, "Yankee Trader requires IEEE binary32");
_Static_assert(DBL_MANT_DIG == 53, "Yankee Trader requires IEEE binary64");

float
qb_mbf32_decode(const uint8_t raw[4])
{
	uint32_t bits;
	uint32_t sign;
	uint32_t fraction;
	uint32_t exponent;
	uint32_t significand;
	uint32_t discarded;
	unsigned shift;
	float value;

	if (raw[3] == 0)
		return 0.0f;

	sign = (uint32_t)(raw[2] & 0x80U) << 24;
	fraction = (uint32_t)raw[0]
	    | ((uint32_t)raw[1] << 8)
	    | ((uint32_t)(raw[2] & 0x7fU) << 16);

	if (raw[3] >= 3) {
		exponent = (uint32_t)raw[3] - 2U;
		bits = sign | (exponent << 23) | fraction;
		memcpy(&value, &bits, sizeof(value));
		return value;
	}

	/* MBF's two lowest binades are IEEE binary32 subnormals. */
	significand = UINT32_C(0x800000) | fraction;
	shift = 3U - raw[3];
	discarded = significand & ((UINT32_C(1) << shift) - 1U);
	significand >>= shift;
	if (discarded > (UINT32_C(1) << (shift - 1U))
	    || (discarded == (UINT32_C(1) << (shift - 1U))
	    && (significand & 1U) != 0))
		++significand;
	bits = sign | significand;
	memcpy(&value, &bits, sizeof(value));
	return value;
}

enum qb_mbf_status
qb_mbf32_encode(float value, uint8_t raw[4])
{
	uint32_t bits;
	uint32_t exponent;
	uint32_t fraction;
	uint32_t magnitude;
	unsigned highest;

	memcpy(&bits, &value, sizeof(bits));
	magnitude = bits & UINT32_C(0x7fffffff);
	if (magnitude == 0) {
		memset(raw, 0, 4);
		return QB_MBF_OK;
	}
	exponent = (bits >> 23) & 0xffU;
	fraction = bits & 0x7fffffU;
	if (exponent == 0xffU)
		return QB_MBF_OVERFLOW;
	if (exponent == 0) {
		if (fraction < UINT32_C(0x200000)) {
			memset(raw, 0, 4);
			return QB_MBF_UNDERFLOW;
		}
		highest = 0;
		while ((fraction >> (highest + 1U)) != 0)
			++highest;
		fraction = (fraction << (23U - highest)) & UINT32_C(0x7fffff);
		exponent = highest - 20U;
	}
	else {
		if (exponent > 253U)
			return QB_MBF_OVERFLOW;
		exponent += 2U;
	}
	raw[0] = (uint8_t)(fraction & 0xffU);
	raw[1] = (uint8_t)((fraction >> 8) & 0xffU);
	raw[2] = (uint8_t)((fraction >> 16) & 0x7fU);
	if ((bits & UINT32_C(0x80000000)) != 0)
		raw[2] |= 0x80U;
	raw[3] = (uint8_t)exponent;
	return QB_MBF_OK;
}

double
qb_mbf64_decode(const uint8_t raw[8])
{
	uint64_t significand = UINT64_C(0x80000000000000);
	uint64_t discarded;
	uint64_t bits;
	uint64_t exponent;
	double value;
	size_t index;

	if (raw[7] == 0)
		return 0.0;
	for (index = 0; index < 6; ++index)
		significand |= (uint64_t)raw[index] << (index * 8U);
	significand |= (uint64_t)(raw[6] & 0x7fU) << 48;
	discarded = significand & 7U;
	significand >>= 3;
	if (discarded > 4U || (discarded == 4U && (significand & 1U) != 0))
		++significand;
	exponent = (uint64_t)((int)raw[7] - 129 + 1023);
	if (significand == UINT64_C(0x20000000000000)) {
		significand >>= 1;
		++exponent;
	}
	bits = ((uint64_t)(raw[6] & 0x80U) << 56)
	    | (exponent << 52)
	    | (significand & UINT64_C(0xfffffffffffff));
	memcpy(&value, &bits, sizeof(value));
	return value;
}

enum qb_mbf_status
qb_mbf64_encode(double value, uint8_t raw[8])
{
	uint64_t bits;
	uint64_t ieee_exponent;
	uint64_t fraction;
	int exponent;
	size_t index;

	memcpy(&bits, &value, sizeof(bits));
	ieee_exponent = (bits >> 52) & UINT64_C(0x7ff);
	fraction = bits & UINT64_C(0xfffffffffffff);
	if (ieee_exponent == 0 && fraction == 0) {
		memset(raw, 0, 8);
		return QB_MBF_OK;
	}
	if (ieee_exponent == UINT64_C(0x7ff))
		return QB_MBF_OVERFLOW;
	if (ieee_exponent == 0) {
		memset(raw, 0, 8);
		return QB_MBF_UNDERFLOW;
	}
	exponent = (int)ieee_exponent - 1023;
	if (exponent < -128) {
		memset(raw, 0, 8);
		return QB_MBF_UNDERFLOW;
	}
	if (exponent > 126)
		return QB_MBF_OVERFLOW;
	fraction <<= 3;
	for (index = 0; index < 6; ++index)
		raw[index] = (uint8_t)((fraction >> (index * 8U)) & 0xffU);
	raw[6] = (uint8_t)((fraction >> 48) & 0x7fU);
	if ((bits & UINT64_C(0x8000000000000000)) != 0)
		raw[6] |= 0x80U;
	raw[7] = (uint8_t)(exponent + 129);
	return QB_MBF_OK;
}

double
qb_int(double value)
{
	return floor(value);
}

double
qb_fix(double value)
{
	return trunc(value);
}

int32_t
qb_cint_mode(double value, uint8_t mode, bool *overflow)
{
	double rounded;

	if (overflow != NULL)
		*overflow = false;
	if (mode == 0x04U)
		rounded = floor(value);
	else
		rounded = value < 0.0 ? ceil(value - 0.5) : floor(value + 0.5);
	if (!isfinite(rounded) || rounded < -32768.0 || rounded > 32767.0) {
		if (overflow != NULL)
			*overflow = true;
		return 0;
	}
	return (int32_t)rounded;
}

int32_t
qb_cint(double value, bool *overflow)
{
	return qb_cint_mode(value, 0, overflow);
}

size_t
qb_ltrim_n(uint8_t *text, size_t length)
{
	size_t first = 0;

	while (first < length && text[first] == ' ')
		++first;
	if (first > 0)
		memmove(text, text + first, length - first);
	return length - first;
}

size_t
qb_rtrim_n(uint8_t *text, size_t length)
{
	while (length > 0 && text[length - 1] == ' ')
		--length;
	return length;
}

size_t
qb_trim_n(uint8_t *text, size_t length)
{
	length = qb_ltrim_n(text, length);
	return qb_rtrim_n(text, length);
}

size_t
qb_collapse_spaces_n(uint8_t *text, size_t length)
{
	size_t read;
	size_t write = 0;
	bool previous_space = false;

	for (read = 0; read < length; ++read) {
		if (text[read] == ' ') {
			if (!previous_space)
				text[write++] = text[read];
			previous_space = true;
		}
		else {
			text[write++] = text[read];
			previous_space = false;
		}
	}
	return write;
}

void
qb_ascii_upper_n(uint8_t *text, size_t length)
{
	size_t index;

	for (index = 0; index < length; ++index) {
		if (text[index] >= 'a' && text[index] <= 'z')
			text[index] &= 0xdfU;
	}
}

void
qb_compat_upper_n(uint8_t *text, size_t length)
{
	size_t index;

	for (index = 0; index < length; ++index) {
		if (text[index] > '@')
			text[index] &= 0xdfU;
	}
}

size_t
qb_title_case_n(uint8_t *text, size_t length)
{
	bool word_start = true;
	size_t index;

	length = qb_trim_n(text, length);
	length = qb_collapse_spaces_n(text, length);
	for (index = 0; index < length; ++index) {
		if (text[index] >= 'a' && text[index] <= 'z') {
			if (word_start)
				text[index] -= 'a' - 'A';
			word_start = false;
		}
		else if (text[index] >= 'A' && text[index] <= 'Z') {
			if (!word_start)
				text[index] += 'a' - 'A';
			word_start = false;
		}
		else if (text[index] != '\'') {
			word_start = true;
		}
	}
	return length;
}

size_t
qb_ltrim(char *text)
{
	size_t length = qb_ltrim_n((uint8_t *)text, strlen(text));

	text[length] = '\0';
	return length;
}

size_t
qb_rtrim(char *text)
{
	size_t length = qb_rtrim_n((uint8_t *)text, strlen(text));

	text[length] = '\0';
	return length;
}

size_t
qb_trim(char *text)
{
	size_t length = qb_trim_n((uint8_t *)text, strlen(text));

	text[length] = '\0';
	return length;
}

void
qb_collapse_spaces(char *text)
{
	size_t length = qb_collapse_spaces_n((uint8_t *)text, strlen(text));

	text[length] = '\0';
}

void
qb_ascii_upper(char *text)
{
	qb_ascii_upper_n((uint8_t *)text, strlen(text));
}

void
qb_compat_upper(char *text)
{
	qb_compat_upper_n((uint8_t *)text, strlen(text));
}

void
qb_title_case(char *text)
{
	size_t length = qb_title_case_n((uint8_t *)text, strlen(text));

	text[length] = '\0';
}

int
qb_ascii_casecmp(const char *left, const char *right)
{
	unsigned char a;
	unsigned char b;

	do {
		a = (unsigned char)*left++;
		b = (unsigned char)*right++;
		if (a >= 'a' && a <= 'z')
			a = (unsigned char)(a - ('a' - 'A'));
		if (b >= 'a' && b <= 'z')
			b = (unsigned char)(b - ('a' - 'A'));
		if (a != b)
			return a < b ? -1 : 1;
	} while (a != 0);
	return 0;
}

bool
qb_ascii_equal_nocase(const char *left, const char *right)
{
	return qb_ascii_casecmp(left, right) == 0;
}
