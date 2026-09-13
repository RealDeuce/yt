#include "yt_game.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(expression) do { \
	if (!(expression)) { \
		fprintf(stderr, "check failed at %s:%d: %s\n", \
		    __FILE__, __LINE__, #expression); \
		++failures; \
	} \
} while (0)

static void
set_number(struct yt_record *record, size_t offset, float value)
{
	CHECK(yt_record_set_number(record, offset, value));
}

static void
planet_fixture(struct yt_record *record)
{
	static const uint8_t dirty_zero[4] = {0x00, 0x00, 0x20, 0x00};
	size_t index;

	for (index = 0U; index < sizeof(record->bytes); ++index)
		record->bytes[index] = (uint8_t)((index * 7U + 11U) & 0xffU);
	memcpy(record->bytes, "Haven", 5U);
	set_number(record, YT_F41, 100.0f);
	set_number(record, YT_F45, 100.0f);
	set_number(record, YT_F49, 200.0f);
	set_number(record, YT_F53, 300.0f);
	set_number(record, YT_F57, 1000.0f);
	set_number(record, YT_F61, 2000.0f);
	set_number(record, YT_F65, 3000.0f);
	CHECK(yt_record_set_raw_number(record, YT_F69, dirty_zero));
	set_number(record, YT_F77, 12.75f);
	set_number(record, YT_F89, 60.0f);
	set_number(record, YT_F113, 2.0f);
	set_number(record, YT_F117, 10000.5f);
	CHECK(yt_record_set_raw_number(record, YT_F125, dirty_zero));
	set_number(record, YT_F129, 30.0f);
}

static bool
decode_hex(const char *hex, uint8_t *raw, size_t length)
{
	size_t index;

	for (index = 0U; index < length; ++index) {
		unsigned high;
		unsigned low;
		char first = hex[index * 2U];
		char second = hex[index * 2U + 1U];

		high = first >= '0' && first <= '9' ? (unsigned)(first - '0')
		    : first >= 'a' && first <= 'f'
		    ? (unsigned)(first - 'a' + 10) : 16U;
		low = second >= '0' && second <= '9' ? (unsigned)(second - '0')
		    : second >= 'a' && second <= 'f'
		    ? (unsigned)(second - 'a' + 10) : 16U;
		if (high > 15U || low > 15U)
			return false;
		raw[index] = (uint8_t)((high << 4U) | low);
	}
	return hex[length * 2U] == '\0';
}

static bool
update_record(struct yt_record *record, float current_day,
    float timer_seconds, struct yt_planet_economy *economy,
    struct yt_error *error)
{
	struct yt_planet_update update;

	return yt_planet_update_prepare(record, &update, error)
	    && yt_planet_update_record(record, &update, current_day,
	    timer_seconds, economy, error);
}

static void
test_exact_record(void)
{
	static const char expected_hex[] =
	    "486176656e2e353c434a51585f666d747b828990979ea5acb3bac1c8cfd6dde4"
	    "ebf2f900070e151c2300004a8739425d87391a5d881dce258964490a8b64300a"
	    "8ca4414f8c1058557d0a11181f00005084424950575e656c7300007087969da4"
	    "abb2b9c0c7ced5dce3eaf1f8ff060d141bd406008200e01d8e5a61686fd9ac2"
	    "a7c66f5288aaeb5bcc3";
	struct yt_planet_economy economy;
	struct yt_record original;
	struct yt_record expected;
	struct yt_error error;

	planet_fixture(&original);
	CHECK(decode_hex(expected_hex, expected.bytes, sizeof(expected.bytes)));
	yt_error_clear(&error);
	CHECK(update_record(&original, 101.0f, 7200.0f,
	    &economy, &error));
	CHECK(memcmp(original.bytes, expected.bytes, sizeof(expected.bytes)) == 0);
	CHECK(economy.elapsed == 1.04166662693023681640625f);
}

static void
test_zero_elapsed(void)
{
	struct yt_planet_economy economy;
	struct yt_record record;
	struct yt_error error;

	planet_fixture(&record);
	yt_error_clear(&error);
	CHECK(update_record(&record, 100.0f, 3600.0f,
	    &economy, &error));
	CHECK(economy.elapsed == 0.0f);
	CHECK(memcmp(record.bytes + YT_F69, "\0\0\x20\0", 4U) == 0);
	CHECK(memcmp(record.bytes + YT_F125, "\0\0\x20\0", 4U) == 0);
}

static void
test_fractional_quantities(void)
{
	struct yt_planet_economy economy;
	struct yt_record record;
	struct yt_error error;

	planet_fixture(&record);
	set_number(&record, YT_F77, 0.6f);
	set_number(&record, YT_F117, 0.6f);
	yt_error_clear(&error);
	CHECK(update_record(&record, 100.0f, 3600.0f,
	    &economy, &error));
	CHECK(memcmp(record.bytes + YT_F77, "\x9a\x99\x19\0", 4U) == 0);
	CHECK(memcmp(record.bytes + YT_F117, "\x9a\x99\x19\0", 4U) == 0);
	CHECK(update_record(&record, 100.0f, 3600.0f,
	    &economy, &error));
	CHECK(memcmp(record.bytes + YT_F77, "\x9a\x99\x19\0", 4U) == 0);
	CHECK(memcmp(record.bytes + YT_F117, "\x9a\x99\x19\0", 4U) == 0);
}

static void
test_growth_boundaries(void)
{
	struct yt_planet_economy economy;
	struct yt_record record;
	struct yt_error error;

	planet_fixture(&record);
	set_number(&record, YT_F45, 10.0f);
	set_number(&record, YT_F49, 0.0f);
	set_number(&record, YT_F53, 0.0f);
	set_number(&record, YT_F57, 100.0f);
	set_number(&record, YT_F61, 0.0f);
	set_number(&record, YT_F65, 0.0f);
	set_number(&record, YT_F77, 0.0f);
	set_number(&record, YT_F89, 60.0f);
	set_number(&record, YT_F113, 0.0f);
	set_number(&record, YT_F117, 0.0f);
	set_number(&record, YT_F129, 0.0f);
	yt_error_clear(&error);
	CHECK(update_record(&record, 100.0f, 3600.0f,
	    &economy, &error));
	CHECK(yt_record_get_number(&record, YT_F45) == 10.0f);
	set_number(&record, YT_F57, 101.0f);
	CHECK(update_record(&record, 100.0f, 3600.0f,
	    &economy, &error));
	CHECK(yt_record_get_number(&record, YT_F45) == 10.1f);

	planet_fixture(&record);
	set_number(&record, YT_F45, 2499.0f);
	set_number(&record, YT_F49, 0.0f);
	set_number(&record, YT_F53, 0.0f);
	set_number(&record, YT_F57, 0.0f);
	set_number(&record, YT_F61, 0.0f);
	set_number(&record, YT_F65, 0.0f);
	set_number(&record, YT_F77, 0.0f);
	set_number(&record, YT_F89, 0.0f);
	set_number(&record, YT_F113, 0.0f);
	set_number(&record, YT_F117, 0.0f);
	set_number(&record, YT_F129, 0.0f);
	CHECK(update_record(&record, 110.0f, 0.0f,
	    &economy, &error));
	CHECK(economy.elapsed == 10.0f);
	CHECK(memcmp(record.bytes + YT_F69, "\0\0\x20\0", 4U) == 0);
}

int
main(void)
{
	test_exact_record();
	test_zero_elapsed();
	test_fractional_quantities();
	test_growth_boundaries();
	return failures == 0 ? 0 : 1;
}
