#include "qb.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned failures;

#define CHECK(expr) do { \
	if (!(expr)) { \
		fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #expr); \
		++failures; \
	} \
} while (0)

static float
float_from_bits(uint32_t bits)
{
	float value;

	memcpy(&value, &bits, sizeof(value));
	return value;
}

static uint32_t
float_bits(float value)
{
	uint32_t bits;

	memcpy(&bits, &value, sizeof(bits));
	return bits;
}

static double
double_from_bits(uint64_t bits)
{
	double value;

	memcpy(&value, &bits, sizeof(value));
	return value;
}

static uint64_t
double_bits(double value)
{
	uint64_t bits;

	memcpy(&bits, &value, sizeof(bits));
	return bits;
}

static void
test_mbf32(void)
{
	static const uint8_t minus_one[4] = {0x00, 0x00, 0x80, 0x81};
	static const uint8_t marker[4] = {0x00, 0xa0, 0x45, 0x8d};
	static const uint8_t launch[4] = {0x9e, 0x69, 0x20, 0x85};
	static const uint8_t raw_zero[4] = {0xff, 0xff, 0x80, 0x00};
	static const uint8_t minimum[4] = {0x00, 0x00, 0x00, 0x01};
	static const uint8_t tie_even_down[4] = {0x02, 0x00, 0x00, 0x01};
	static const uint8_t tie_even_up[4] = {0x06, 0x00, 0x00, 0x01};
	static const uint8_t maximum[4] = {0xff, 0xff, 0x7f, 0xff};
	uint8_t encoded[4];
	unsigned exponent;

	CHECK(qb_mbf32_decode(minus_one) == -1.0f);
	CHECK(qb_mbf32_decode(marker) == 6324.0f);
	CHECK(qb_mbf32_decode(raw_zero) == 0.0f);
	CHECK(qb_mbf32_decode(launch) == 20.051570892333984f);
	CHECK(float_bits(qb_mbf32_decode(minimum)) == UINT32_C(0x00200000));
	CHECK(float_bits(qb_mbf32_decode(tie_even_down))
	    == UINT32_C(0x00200000));
	CHECK(float_bits(qb_mbf32_decode(tie_even_up))
	    == UINT32_C(0x00200002));
	CHECK(qb_mbf32_encode(-1.0f, encoded) == QB_MBF_OK);
	CHECK(memcmp(encoded, minus_one, sizeof(encoded)) == 0);
	CHECK(qb_mbf32_encode(6324.0f, encoded) == QB_MBF_OK);
	CHECK(memcmp(encoded, marker, sizeof(encoded)) == 0);
	CHECK(qb_mbf32_encode(qb_mbf32_decode(launch), encoded) == QB_MBF_OK);
	CHECK(memcmp(encoded, launch, sizeof(encoded)) == 0);
	CHECK(qb_mbf32_encode(float_from_bits(UINT32_C(0x00200000)), encoded)
	    == QB_MBF_OK);
	CHECK(memcmp(encoded, minimum, sizeof(encoded)) == 0);
	memset(encoded, 0xa5, sizeof(encoded));
	CHECK(qb_mbf32_encode(float_from_bits(UINT32_C(0x001fffff)), encoded)
	    == QB_MBF_UNDERFLOW);
	CHECK(memcmp(encoded, "\0\0\0\0", sizeof(encoded)) == 0);
	CHECK(qb_mbf32_encode(float_from_bits(UINT32_C(0x7effffff)), encoded)
	    == QB_MBF_OK);
	CHECK(memcmp(encoded, maximum, sizeof(encoded)) == 0);
	memset(encoded, 0xa5, sizeof(encoded));
	CHECK(qb_mbf32_encode(float_from_bits(UINT32_C(0x7f000000)), encoded)
	    == QB_MBF_OVERFLOW);
	CHECK(memcmp(encoded, "\xa5\xa5\xa5\xa5", sizeof(encoded)) == 0);
	CHECK(qb_mbf32_encode(INFINITY, encoded) == QB_MBF_OVERFLOW);
	CHECK(qb_mbf32_encode(NAN, encoded) == QB_MBF_OVERFLOW);
	CHECK(!qb_mbf32_truth(NULL));
	for (exponent = 0; exponent <= UINT8_MAX; ++exponent) {
		uint8_t raw[4] = {0xa5, 0x5a, 0x80, (uint8_t)exponent};

		CHECK(qb_mbf32_truth(raw) == (exponent != 0U));
	}
}

static void
test_mbf64(void)
{
	static const uint8_t value[8] =
	    {0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x74, 0x92};
	static const uint8_t minimum[8] =
	    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
	static const uint8_t tie_even_down[8] =
	    {0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x81};
	static const uint8_t tie_even_up[8] =
	    {0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x81};
	static const uint8_t maximum_double[8] =
	    {0xf8, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0xff};
	static const uint8_t csng_source[8] =
	    {0x00, 0x00, 0x00, 0x90, 0x7c, 0xd6, 0x0d, 0x81};
	static const uint8_t csng_expected[4] = {0x7d, 0xd6, 0x0d, 0x81};
	static const uint8_t fractional[8] =
	    {0x00, 0x00, 0x00, 0x00, 0x9a, 0x99, 0x19, 0x80};
	static const uint8_t fractional_int[8] =
	    {0x00, 0x00, 0x00, 0x00, 0x9a, 0x99, 0x19, 0x00};
	uint8_t encoded[8];
	uint8_t encoded_single[4];
	uint8_t scratch[8];
	uint8_t result[8];
	static const uint8_t signature[8] =
	    {0x00, 0x00, 0xa8, 0x43, 0x3b, 0x6a, 0x4f, 0xa5};
	static const uint8_t first_sum[8] =
	    {0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x08, 0x8c};
	static const uint8_t first_product[8] =
	    {0x00, 0x6a, 0xb3, 0x86, 0xb9, 0x94, 0x5c, 0xb0};
	static const uint8_t first_root[8] =
	    {0x3e, 0x9e, 0x8d, 0x48, 0xa9, 0xa1, 0x6d, 0x98};
	static const uint8_t first_integer[8] =
	    {0x00, 0x00, 0x00, 0x00, 0xa9, 0xa1, 0x6d, 0x98};
	static const uint8_t second_sum[8] =
	    {0x00, 0x00, 0x00, 0x00, 0x84, 0xb2, 0x6d, 0x98};
	static const uint8_t second_product[8] =
	    {0xb3, 0x21, 0x9e, 0xad, 0xfb, 0x95, 0x40, 0xbd};
	static const uint8_t seven_product[8] =
	    {0x7d, 0x5d, 0xea, 0x37, 0x3c, 0x83, 0x28, 0xc0};
	static const uint8_t final_root[8] =
	    {0xad, 0x10, 0x12, 0x8f, 0x2a, 0xb3, 0x4f, 0xa0};
	static const uint8_t final_integer[8] =
	    {0x00, 0x00, 0x00, 0x8f, 0x2a, 0xb3, 0x4f, 0xa0};

	CHECK(qb_mbf64_decode(value) == 250000.0);
	CHECK(double_bits(qb_mbf64_decode(tie_even_down))
	    == UINT64_C(0x3ff0000000000000));
	CHECK(double_bits(qb_mbf64_decode(tie_even_up))
	    == UINT64_C(0x3ff0000000000002));
	CHECK(qb_mbf64_encode(250000.0, encoded) == QB_MBF_OK);
	CHECK(memcmp(encoded, value, sizeof(encoded)) == 0);
	CHECK(qb_mbf64_encode(ldexp(1.0, -128), encoded) == QB_MBF_OK);
	CHECK(memcmp(encoded, minimum, sizeof(encoded)) == 0);
	memset(encoded, 0xa5, sizeof(encoded));
	CHECK(qb_mbf64_encode(ldexp(1.0, -129), encoded)
	    == QB_MBF_UNDERFLOW);
	CHECK(memcmp(encoded, "\0\0\0\0\0\0\0\0", sizeof(encoded)) == 0);
	CHECK(qb_mbf64_encode(double_from_bits(UINT64_C(0x47dfffffffffffff)),
	    encoded) == QB_MBF_OK);
	CHECK(memcmp(encoded, maximum_double, sizeof(encoded)) == 0);
	memset(encoded, 0xa5, sizeof(encoded));
	CHECK(qb_mbf64_encode(ldexp(1.0, 127), encoded) == QB_MBF_OVERFLOW);
	CHECK(memcmp(encoded, "\xa5\xa5\xa5\xa5\xa5\xa5\xa5\xa5",
	    sizeof(encoded)) == 0);
	CHECK(qb_mbf64_encode(INFINITY, encoded) == QB_MBF_OVERFLOW);
	CHECK(qb_mbf64_encode(NAN, encoded) == QB_MBF_OVERFLOW);
	CHECK(qb_mbf32_from_mbf64_raw(csng_source, encoded_single) == QB_MBF_OK);
	CHECK(memcmp(encoded_single, csng_expected, sizeof(encoded_single)) == 0);
	CHECK(qb_mbf64_int_positive_raw(fractional, encoded) == QB_MBF_OK);
	CHECK(memcmp(encoded, fractional_int, sizeof(encoded)) == 0);
	CHECK(qb_mbf32_from_mbf64_raw(encoded, encoded_single) == QB_MBF_OK);
	CHECK(memcmp(encoded_single, fractional_int + 4U,
	    sizeof(encoded_single)) == 0);
	CHECK(qb_mbf64_encode(-5.5, scratch) == QB_MBF_OK);
	CHECK(qb_mbf64_floor_raw(scratch, scratch) == QB_MBF_OK);
	CHECK(qb_mbf64_encode(-6.0, result) == QB_MBF_OK);
	CHECK(memcmp(scratch, result, sizeof(scratch)) == 0);
	CHECK(qb_mbf64_encode(-5.0, scratch) == QB_MBF_OK);
	CHECK(qb_mbf64_floor_raw(scratch, encoded) == QB_MBF_OK);
	CHECK(memcmp(scratch, encoded, sizeof(scratch)) == 0);
	CHECK(qb_mbf64_encode(-0.5, scratch) == QB_MBF_OK);
	CHECK(qb_mbf64_floor_raw(scratch, encoded) == QB_MBF_OK);
	CHECK(qb_mbf64_encode(-1.0, result) == QB_MBF_OK);
	CHECK(memcmp(encoded, result, sizeof(encoded)) == 0);

	/* The registration path needs all 56 MBF bits, not host binary64. */
	CHECK(qb_mbf64_mul_raw(signature, first_sum, scratch) == QB_MBF_OK);
	CHECK(memcmp(scratch, first_product, sizeof(scratch)) == 0);
	CHECK(qb_mbf64_sqrt_raw(scratch, result) == QB_MBF_OK);
	CHECK(memcmp(result, first_root, sizeof(result)) == 0);
	CHECK(qb_mbf64_floor_positive_raw(result, scratch) == QB_MBF_OK);
	CHECK(memcmp(scratch, first_integer, sizeof(scratch)) == 0);
	CHECK(qb_mbf64_from_u64(UINT64_C(15577732), scratch) == QB_MBF_OK);
	CHECK(memcmp(scratch, second_sum, sizeof(scratch)) == 0);
	CHECK(qb_mbf64_mul_raw(signature, scratch, result) == QB_MBF_OK);
	CHECK(memcmp(result, second_product, sizeof(result)) == 0);
	CHECK(qb_mbf64_from_u64(7U, scratch) == QB_MBF_OK);
	CHECK(qb_mbf64_mul_raw(result, scratch, result) == QB_MBF_OK);
	CHECK(memcmp(result, seven_product, sizeof(result)) == 0);
	CHECK(qb_mbf64_sqrt_raw(result, scratch) == QB_MBF_OK);
	CHECK(memcmp(scratch, final_root, sizeof(scratch)) == 0);
	CHECK(qb_mbf64_floor_positive_raw(scratch, result) == QB_MBF_OK);
	CHECK(memcmp(result, final_integer, sizeof(result)) == 0);
	memset(scratch, 0, sizeof(scratch));
	CHECK(qb_mbf64_div_raw(signature, scratch, result) == QB_MBF_DOMAIN);
	memcpy(scratch, signature, sizeof(scratch));
	scratch[6] |= 0x80U;
	CHECK(qb_mbf64_sqrt_raw(scratch, result) == QB_MBF_DOMAIN);
	memcpy(scratch, "\xa5\x5a\x11\x22\x33\x44\xd5\0", 8U);
	CHECK(qb_mbf64_sqrt_raw(scratch, result) == QB_MBF_OK
	    && memcmp(result, scratch, sizeof(result)) == 0);
}

static void
test_numeric(void)
{
	static const uint8_t val_twelve_point_five[8] =
	    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0x84};
	static const uint8_t val_one_e_minus_38[8] =
	    {0x22, 0xc7, 0x53, 0xed, 0xdc, 0xc7, 0x59, 0x02};
	static const uint8_t val_long_fraction[8] =
	    {0xe4, 0x38, 0x8e, 0xe3, 0x38, 0x8e, 0x63, 0x7d};
	static const uint8_t raw_single_one[4] = {0x00, 0x00, 0x01, 0x81};
	static const uint8_t raw_single_two[4] = {0x82, 0x00, 0x00, 0x81};
	static const uint8_t raw_double[8] =
	    {0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x81};
	static const uint8_t raw_double_fixed[8] =
	    {0xfc, 0xff, 0x03, 0xbf, 0xc9, 0x1b, 0x0e, 0xb6};
	static const uint8_t raw_double_exponential[8] =
	    {0x00, 0x00, 0x04, 0xbf, 0xc9, 0x1b, 0x0e, 0xb6};
	static const uint8_t raw_double_high[8] =
	    {0x89, 0x0d, 0xb5, 0x50, 0x99, 0x76, 0x16, 0xff};
	static const uint8_t raw_dirty_zero[4] = {0x00, 0x00, 0x80, 0x00};
	static const uint8_t raw_single_midpoint[4] = {0x42, 0xe5, 0x31, 0xd6};
	bool overflow;
	struct qb_val_result value;
	char rendered[64];

	CHECK(qb_int(-1.2) == -2.0);
	CHECK(qb_fix(-1.2) == -1.0);
	CHECK(qb_cint(1.5, &overflow) == 2 && !overflow);
	CHECK(qb_cint(2.5, &overflow) == 3 && !overflow);
	CHECK(qb_cint(-1.5, &overflow) == -2 && !overflow);
	CHECK(qb_cint(-2.5, &overflow) == -3 && !overflow);
	CHECK(qb_cint_mode(2.5, 4, &overflow) == 2 && !overflow);
	CHECK(qb_cint_mode(-0.5, 4, &overflow) == -1 && !overflow);
	CHECK(qb_cint_mode(-2.25, 4, &overflow) == -3 && !overflow);
	CHECK(qb_cint_mode(0.75, 4, &overflow) == 0 && !overflow);
	CHECK(qb_cint_mode(2.5, 0xa5, &overflow) == 3 && !overflow);
	CHECK(qb_brun_random_record_number(2057.5f) == 2057U);
	CHECK(qb_brun_random_record_number(-1.25f) == 0x00fffffeU);
	CHECK(qb_brun_random_record_number(16777216.0f) == 0U);
	CHECK(qb_brun_random_record_number(-16777216.0f) == 0x00ffffffU);
	CHECK(qb_cint(32767.49, &overflow) == 32767 && !overflow);
	CHECK(qb_cint(-32767.5, &overflow) == -32768 && !overflow);
	(void)qb_cint(32767.6, &overflow);
	CHECK(overflow);
	(void)qb_cint(-32768.5, &overflow);
	CHECK(overflow);
	value = qb_val("  -12.5E1xyz");
	CHECK(value.valid && value.value == -125.0 && value.consumed == 9);
	value = qb_val("not numeric");
	CHECK(!value.valid);
	value = qb_val(" & H F F ");
	CHECK(value.valid && value.value == 0.0);
	value = qb_val("-&o10tail");
	CHECK(!value.valid && value.value == 0.0);
	value = qb_val("1 D 2 ignored");
	CHECK(value.valid && value.value == 100.0);
	value = qb_val("1 2.5");
	CHECK(value.valid && value.value == 12.5);
	CHECK(memcmp(value.mbf, val_twelve_point_five, 8) == 0);
	CHECK(qb_val("1\t2.5").value == 12.5);
	CHECK(qb_val("1\n2.5").value == 12.5);
	CHECK(qb_val("1\r2.5").value == 1.0);
	CHECK(qb_val("&10").value == 8.0);
	CHECK(qb_val("&H1 0").value == 1.0);
	CHECK(qb_val("&HFFFF").value == -1.0);
	CHECK(qb_val("&O177777").value == -1.0);
	CHECK(qb_val("&H8000").value == -32768.0);
	CHECK(qb_val("&H10000").overflow);
	CHECK(qb_val("&O200000").overflow);
	value = qb_val("1D-56");
	CHECK(!value.overflow && value.value == 0.0);
	value = qb_val("1D-39");
	CHECK(!value.overflow && value.value == 0.0);
	value = qb_val("1D-38");
	CHECK(!value.overflow && value.value != 0.0);
	CHECK(memcmp(value.mbf, val_one_e_minus_38, 8) == 0);
	value = qb_val("0.11111111111111111111111111111111111111111111111111111111");
	CHECK(!value.overflow);
	CHECK(memcmp(value.mbf, val_long_fraction, 8) == 0);
	CHECK(qb_val("0D39").overflow);
	CHECK(qb_val("1D39").overflow);
	CHECK(qb_val("2E38").overflow);
	CHECK(qb_val("1.8E38").overflow);
	CHECK(qb_val("12.5%trailing").value == 12.5);
	CHECK(qb_val("12.5#trailing").value == 12.5);
	CHECK(qb_val("12.5!trailing").value == 12.5);
	qb_str_double(rendered, sizeof(rendered), 11.0);
	CHECK(strcmp(rendered, " 11") == 0);
	qb_str_double(rendered, sizeof(rendered), -11.0);
	CHECK(strcmp(rendered, "-11") == 0);
	qb_str_single(rendered, sizeof(rendered), 0.5f);
	CHECK(strcmp(rendered, " .5") == 0);
	qb_str_single(rendered, sizeof(rendered), 1.0e-7f);
	CHECK(strcmp(rendered, " .0000001") == 0);
	qb_str_single(rendered, sizeof(rendered), 1.0e-8f);
	CHECK(strcmp(rendered, " 1E-08") == 0);
	qb_str_single(rendered, sizeof(rendered), 9999999.0f);
	CHECK(strcmp(rendered, " 9999999") == 0);
	qb_str_single(rendered, sizeof(rendered), 10000000.0f);
	CHECK(strcmp(rendered, " 1E+07") == 0);
	qb_str_single(rendered, sizeof(rendered), -1.0e-7f);
	CHECK(strcmp(rendered, "-.0000001") == 0);
	qb_str_single(rendered, sizeof(rendered), -1.0e-8f);
	CHECK(strcmp(rendered, "-1E-08") == 0);
	qb_str_single(rendered, sizeof(rendered), 1.2345674f);
	CHECK(strcmp(rendered, " 1.234567") == 0);
	qb_str_single(rendered, sizeof(rendered), 1.2345675f);
	CHECK(strcmp(rendered, " 1.234568") == 0);
	qb_str_single(rendered, sizeof(rendered), 9.9999995f);
	CHECK(strcmp(rendered, " 9.999999") == 0);
	qb_str_single(rendered, sizeof(rendered), 9.9999996f);
	CHECK(strcmp(rendered, " 10") == 0);
	qb_str_double(rendered, sizeof(rendered), 1.0e-16);
	CHECK(strcmp(rendered, " .0000000000000001") == 0);
	qb_str_double(rendered, sizeof(rendered), 1.0e-17);
	CHECK(strcmp(rendered, " 1D-17") == 0);
	qb_str_double(rendered, sizeof(rendered), 1.0e15);
	CHECK(strcmp(rendered, " 1000000000000000") == 0);
	qb_str_double(rendered, sizeof(rendered), 1.0e16);
	CHECK(strcmp(rendered, " 1D+16") == 0);
	qb_str_mbf32(rendered, sizeof(rendered), raw_single_one);
	CHECK(strcmp(rendered, " 1.007813") == 0);
	qb_str_mbf32(rendered, sizeof(rendered), raw_single_two);
	CHECK(strcmp(rendered, " 1.000016") == 0);
	qb_str_mbf64(rendered, sizeof(rendered), raw_double);
	CHECK(strcmp(rendered, " 1.000015258789063") == 0);
	qb_str_mbf64(rendered, sizeof(rendered), raw_double_fixed);
	CHECK(strcmp(rendered, " 9999999999999999") == 0);
	qb_str_mbf64(rendered, sizeof(rendered), raw_double_exponential);
	CHECK(strcmp(rendered, " 1D+16") == 0);
	qb_str_mbf64(rendered, sizeof(rendered), raw_double_high);
	CHECK(strcmp(rendered, " 1D+38") == 0);
	qb_str_mbf32(rendered, sizeof(rendered), raw_dirty_zero);
	CHECK(strcmp(rendered, " 0") == 0);
	qb_str_mbf32(rendered, sizeof(rendered), raw_single_midpoint);
	CHECK(strcmp(rendered, " 5.376563E+25") == 0);
	qb_str_integer(rendered, sizeof(rendered), INT16_MIN);
	CHECK(strcmp(rendered, "-32768") == 0);
	qb_str_integer(rendered, sizeof(rendered), INT16_MAX);
	CHECK(strcmp(rendered, " 32767") == 0);
	CHECK(qb_print_integer(rendered, sizeof(rendered), 5) == 3
	    && strcmp(rendered, " 5 ") == 0);
	CHECK(qb_print_single(rendered, sizeof(rendered), -5.0f) == 3
	    && strcmp(rendered, "-5 ") == 0);
	CHECK(qb_print_double(rendered, sizeof(rendered), 0.5) == 4
	    && strcmp(rendered, " .5 ") == 0);
	CHECK(qb_print_number(rendered, sizeof(rendered), -1.0e-17) == 7
	    && strcmp(rendered, "-1D-17 ") == 0);
	memset(rendered, 0xa5, sizeof(rendered));
	CHECK(qb_print_integer(rendered, 3U, 5) == 3
	    && memcmp(rendered, " 5\0", 3U) == 0);
}

static uint64_t
hash_byte(uint64_t hash, uint8_t byte)
{
	return (hash ^ byte) * UINT64_C(1099511628211);
}

static uint64_t
hash_rendered(uint64_t hash, const char *text)
{
	do {
		hash = hash_byte(hash, (uint8_t)*text);
	} while (*text++ != '\0');
	return hash;
}

static void
test_numeric_raw_sweep(void)
{
	uint64_t hash32 = UINT64_C(14695981039346656037);
	uint64_t hash64 = UINT64_C(14695981039346656037);
	uint64_t decode32 = UINT64_C(14695981039346656037);
	uint64_t decode64 = UINT64_C(14695981039346656037);
	uint64_t encode32 = UINT64_C(14695981039346656037);
	uint64_t encode64 = UINT64_C(14695981039346656037);
	uint32_t state32 = UINT32_C(0x12345678);
	uint64_t state64 = UINT64_C(0x123456789abcdef0);
	char rendered[64];
	unsigned iteration;

	for (iteration = 0; iteration < 4096U; ++iteration) {
		uint8_t raw[4];
		unsigned index;

		state32 ^= state32 << 13;
		state32 ^= state32 >> 17;
		state32 ^= state32 << 5;
		for (index = 0; index < 4U; ++index)
			raw[index] = (uint8_t)(state32 >> (index * 8U));
		CHECK(qb_str_mbf32(rendered, sizeof(rendered), raw) >= 0);
		hash32 = hash_rendered(hash32, rendered);
		for (index = 0; index < 4U; ++index)
			decode32 = hash_byte(decode32,
			    (uint8_t)(float_bits(qb_mbf32_decode(raw))
			    >> (index * 8U)));
	}
	for (iteration = 0; iteration < 4096U; ++iteration) {
		uint8_t raw[8];
		unsigned index;

		state64 ^= state64 << 13;
		state64 ^= state64 >> 7;
		state64 ^= state64 << 17;
		for (index = 0; index < 8U; ++index)
			raw[index] = (uint8_t)(state64 >> (index * 8U));
		CHECK(qb_str_mbf64(rendered, sizeof(rendered), raw) >= 0);
		hash64 = hash_rendered(hash64, rendered);
		for (index = 0; index < 8U; ++index)
			decode64 = hash_byte(decode64,
			    (uint8_t)(double_bits(qb_mbf64_decode(raw))
			    >> (index * 8U)));
	}
	CHECK(hash32 == UINT64_C(0xd8e874719f20f9e0));
	CHECK(hash64 == UINT64_C(0x526ba912f75cbe34));
	CHECK(decode32 == UINT64_C(0xa21c5ab98513787e));
	CHECK(decode64 == UINT64_C(0xfd92b45064032fa0));

	state32 = UINT32_C(0x8badf00d);
	for (iteration = 0; iteration < 4096U; ++iteration) {
		uint8_t raw[4];
		enum qb_mbf_status status;
		unsigned index;

		state32 ^= state32 << 13;
		state32 ^= state32 >> 17;
		state32 ^= state32 << 5;
		memset(raw, 0xa5, sizeof(raw));
		status = qb_mbf32_encode(float_from_bits(state32), raw);
		encode32 = hash_byte(encode32, (uint8_t)status);
		for (index = 0; index < 4U; ++index)
			encode32 = hash_byte(encode32, raw[index]);
	}
	state64 = UINT64_C(0xd1b54a32d192ed03);
	for (iteration = 0; iteration < 4096U; ++iteration) {
		uint8_t raw[8];
		enum qb_mbf_status status;
		unsigned index;

		state64 ^= state64 << 13;
		state64 ^= state64 >> 7;
		state64 ^= state64 << 17;
		memset(raw, 0xa5, sizeof(raw));
		status = qb_mbf64_encode(double_from_bits(state64), raw);
		encode64 = hash_byte(encode64, (uint8_t)status);
		for (index = 0; index < 8U; ++index)
			encode64 = hash_byte(encode64, raw[index]);
	}
	CHECK(encode32 == UINT64_C(0x19f4cee7817b7044));
	CHECK(encode64 == UINT64_C(0x7d549b35bee7251a));
}

struct upper_store_tape {
	enum qb_compat_upper_store_kind kind[16];
	float value[16];
	size_t count;
};

static void
upper_store(void *context, enum qb_compat_upper_store_kind kind, float value)
{
	struct upper_store_tape *tape = context;

	if (tape->count >= YT_ARRAY_LEN(tape->kind))
		return;
	tape->kind[tape->count] = kind;
	tape->value[tape->count] = value;
	++tape->count;
}

static void
test_strings(void)
{
	struct upper_store_tape upper_tape;
	char title[64] = "  aLAN  o'BRIEN, jr  ";
	char upper[32] = "List \x82";
	char compatibility[] = "az{_@\xe1";
	uint8_t ascii_domain[256];
	uint8_t compat_domain[256];
	uint8_t one[1];
	size_t byte;
	static const unsigned char expected[] =
	    {'A', 'Z', '[', '_', '@', 0xc1, 0};

	qb_title_case(title);
	CHECK(strcmp(title, "Alan O'brien, Jr") == 0);
	qb_ascii_upper(upper);
	CHECK(strcmp(upper, "LIST \x82") == 0);
	qb_compat_upper(compatibility);
	CHECK(memcmp(compatibility, expected, sizeof(expected)) == 0);
	memset(&upper_tape, 0, sizeof(upper_tape));
	memcpy(compatibility, "a@B{", 5U);
	qb_compat_upper_n_observed((uint8_t *)compatibility, 4U,
	    upper_store, &upper_tape);
	CHECK(strcmp(compatibility, "A@B[") == 0);
	CHECK(upper_tape.count == 11U);
	CHECK(upper_tape.kind[0] == QB_COMPAT_UPPER_STORE_NUMERIC_TEMP
	    && upper_tape.value[0] == 4.0f);
	CHECK(upper_tape.kind[1] == QB_COMPAT_UPPER_STORE_LENGTH
	    && upper_tape.value[1] == 4.0f);
	CHECK(upper_tape.kind[2] == QB_COMPAT_UPPER_STORE_INDEX
	    && upper_tape.value[2] == 1.0f);
	for (byte = 0U; byte < 4U; ++byte) {
		CHECK(upper_tape.kind[3U + byte * 2U]
		    == QB_COMPAT_UPPER_STORE_NUMERIC_TEMP);
		CHECK(upper_tape.kind[4U + byte * 2U]
		    == QB_COMPAT_UPPER_STORE_INDEX);
		CHECK(upper_tape.value[3U + byte * 2U] == (float)(byte + 2U)
		    && upper_tape.value[4U + byte * 2U]
		    == (float)(byte + 2U));
	}
	memset(&upper_tape, 0, sizeof(upper_tape));
	qb_compat_upper_n_observed(one, 0U, upper_store, &upper_tape);
	CHECK(upper_tape.count == 3U
	    && upper_tape.kind[0] == QB_COMPAT_UPPER_STORE_NUMERIC_TEMP
	    && upper_tape.value[0] == 0.0f
	    && upper_tape.kind[1] == QB_COMPAT_UPPER_STORE_LENGTH
	    && upper_tape.value[1] == 0.0f
	    && upper_tape.kind[2] == QB_COMPAT_UPPER_STORE_INDEX
	    && upper_tape.value[2] == 1.0f);
	for (byte = 0; byte < 256; ++byte) {
		ascii_domain[byte] = (uint8_t)byte;
		compat_domain[byte] = (uint8_t)byte;
	}
	qb_ascii_upper_n(ascii_domain, sizeof(ascii_domain));
	qb_compat_upper_n(compat_domain, sizeof(compat_domain));
	one[0] = 0xa5U;
	qb_compat_upper_n(one, 0U);
	CHECK(one[0] == 0xa5U);
	for (byte = 0; byte < 256; ++byte) {
		uint8_t ascii_expected = (uint8_t)byte;
		uint8_t compat_expected = (uint8_t)byte;

		if (ascii_expected >= 'a' && ascii_expected <= 'z')
			ascii_expected &= 0xdfU;
		if (compat_expected > '@')
			compat_expected &= 0xdfU;
		CHECK(ascii_domain[byte] == ascii_expected);
		CHECK(compat_domain[byte] == compat_expected);
		one[0] = (uint8_t)byte;
		if (byte == ' ')
			CHECK(qb_title_case_n(one, 1) == 0);
		else {
			CHECK(qb_title_case_n(one, 1) == 1);
			CHECK(one[0] == (byte >= 'a' && byte <= 'z'
			    ? (uint8_t)(byte - ('a' - 'A')) : (uint8_t)byte));
		}
	}
	{
		uint8_t embedded[] = {'a', 0, 'B', '\'', 'C', '-', 'd'};
		static const uint8_t embedded_expected[] =
		    {'A', 0, 'B', '\'', 'c', '-', 'D'};

		CHECK(qb_title_case_n(embedded, sizeof(embedded))
		    == sizeof(embedded));
		CHECK(memcmp(embedded, embedded_expected, sizeof(embedded)) == 0);
	}
	CHECK(qb_ascii_equal_nocase("YtData.Dat", "ytdata.dat"));
}

int
main(void)
{
	test_mbf32();
	test_mbf64();
	test_numeric();
	test_numeric_raw_sweep();
	test_strings();
	if (failures != 0) {
		fprintf(stderr, "%u test(s) failed\n", failures);
		return EXIT_FAILURE;
	}
	puts("test_qb: ok");
	return EXIT_SUCCESS;
}
