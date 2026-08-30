#include "yt_data.h"
#include "yt_file.h"
#include "yt_main_error.h"
#include "yt_platform.h"
#include "yt_random.h"
#include "yt_text.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir_one(path) _mkdir(path)
#else
#include <sys/stat.h>
#include <unistd.h>
#define mkdir_one(path) mkdir(path, 0700)
#endif

static unsigned failures;

#define CHECK(expr) do { \
	if (!(expr)) { \
		fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #expr); \
		++failures; \
	} \
} while (0)

struct scripted_random {
	const uint8_t *bytes;
	size_t length;
	size_t position;
};

static bool write_bytes(const char *path, const uint8_t *bytes, size_t length);

struct main_error_present_capture {
	const char *path;
	const uint8_t *expected_before;
	size_t expected_before_length;
	uint8_t row[YT_MAIN_ERROR_TEXT];
	size_t row_length;
	unsigned calls;
	bool succeeds;
};

static bool
capture_main_error_row(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	struct main_error_present_capture *capture = context;
	struct yt_text_file before;

	++capture->calls;
	if (capture->expected_before == NULL) {
		FILE *file = fopen(capture->path, "rb");

		CHECK(file == NULL);
		if (file != NULL)
			(void)fclose(file);
	} else {
		CHECK(yt_text_read(capture->path, &before, error));
		if (before.data != NULL) {
			CHECK(before.length == capture->expected_before_length);
			CHECK(before.length != capture->expected_before_length
			    || memcmp(before.data, capture->expected_before,
			    before.length) == 0);
			yt_text_free(&before);
		}
	}
	CHECK(length <= sizeof(capture->row));
	if (length <= sizeof(capture->row)) {
		memcpy(capture->row, text, length);
		capture->row_length = length;
	}
	if (!capture->succeeds && error != NULL) {
		error->status = YT_IO_ERROR;
		snprintf(error->operation, sizeof(error->operation),
		    "present fatal main error");
	}
	return capture->succeeds;
}

static bool
scripted_fill(void *context, void *buffer, size_t length,
    struct yt_error *error)
{
	struct scripted_random *script = context;

	if (script->position + length > script->length) {
		if (error != NULL)
			error->status = YT_RANDOM_ERROR;
		return false;
	}
	memcpy(buffer, script->bytes + script->position, length);
	script->position += length;
	return true;
}

static void
test_record(void)
{
	struct yt_record record;
	struct yt_record preserved;
	struct yt_radio_record radio;
	struct yt_radio_record mutated;
	struct yt_radio_record before_mutation;
	struct yt_radio_reader_decision decision;
	struct yt_error error;
	uint8_t header[64];
	uint8_t long_radio_text[75];
	size_t header_length;
	size_t index;
	char name[64];
	uint8_t tail[4] = {1, 2, 3, 4};
	static const uint8_t dirty_zero[4] = {0x12, 0x34, 0x80, 0x00};
	static const uint8_t radio_dirty_zero[4] = {0x00, 0x00, 0x80, 0x00};
	static const uint8_t personal_prefix[14] = {
		0x00, 0x00, 0x00, 0x81,
		0x00, 0x00, 0x00, 0x82,
		0x00, 0x00, 0x40, 0x82,
		'H', 'i'
	};

	yt_record_blank(&record);
	memcpy(record.bytes + YT_RECORD_TAIL_OFFSET, tail, sizeof(tail));
	yt_record_set_text(&record, (const uint8_t *)"Port Alpha", 10);
	yt_record_set_number(&record, YT_F41, 3.0f);
	CHECK(yt_record_get_number(&record, YT_F41) == 3.0f);
	CHECK(yt_record_get_text(&record, name, sizeof(name)) == 10);
	CHECK(strcmp(name, "Port Alpha") == 0);
	CHECK(memcmp(record.bytes + YT_RECORD_TAIL_OFFSET, tail, sizeof(tail)) == 0);
	CHECK(yt_record_set_raw_number(&record, YT_F45, dirty_zero));
	CHECK(yt_record_set_number_if_changed(&record, YT_F45, 0.0f));
	CHECK(memcmp(record.bytes + YT_F45, dirty_zero, sizeof(dirty_zero)) == 0);
	CHECK(yt_record_set_number(&record, YT_F49, ldexpf(1.0f, -129)));
	CHECK(memcmp(record.bytes + YT_F49, "\0\0\0\0", 4) == 0);
	preserved = record;
	CHECK(!yt_record_set_number(&record, YT_F49, ldexpf(1.0f, 127)));
	CHECK(memcmp(&record, &preserved, sizeof(record)) == 0);

	memset(&radio, 0xff, sizeof(radio));
	CHECK(yt_radio_set_raw_number(&radio, 0, radio_dirty_zero));
	CHECK(yt_radio_get_number(&radio, 0) == 0.0f);
	CHECK(memcmp(radio.bytes, radio_dirty_zero,
	    sizeof(radio_dirty_zero)) == 0);
	CHECK(!yt_radio_set_raw_number(&radio, 2, radio_dirty_zero));
	CHECK(!yt_radio_set_raw_number(&radio, 12, radio_dirty_zero));
	CHECK(yt_radio_message_record(&radio, (const uint8_t *)"Hi", 2,
	    3.0f, 2.0f));
	CHECK(memcmp(radio.bytes, personal_prefix, sizeof(personal_prefix)) == 0);
	CHECK(memcmp(radio.bytes + sizeof(personal_prefix),
	    "                                                                        ",
	    sizeof(radio.bytes) - sizeof(personal_prefix)) == 0);
	CHECK(yt_radio_message_record(&radio, (const uint8_t *)"A", 1,
	    3.0f, -2.0f));
	CHECK(yt_radio_get_number(&radio, 0) == 30.0f
	    && yt_radio_get_number(&radio, 4) == -2.0f
	    && yt_radio_get_number(&radio, 8) == 3.0f
	    && radio.bytes[12] == 'A' && radio.bytes[85] == ' ');
	for (index = 0; index < sizeof(long_radio_text); ++index)
		long_radio_text[index] = (uint8_t)index;
	CHECK(yt_radio_message_record(&radio, long_radio_text,
	    sizeof(long_radio_text), -2.0f, -2.0f));
	CHECK(yt_radio_get_number(&radio, 0) == 30.0f
	    && yt_radio_get_number(&radio, 4) == -2.0f
	    && yt_radio_get_number(&radio, 8) == -2.0f
	    && memcmp(radio.bytes + 12U, long_radio_text, 74U) == 0
	    && radio.bytes[85] == long_radio_text[73]);
	CHECK(!yt_radio_message_record(NULL, NULL, 0, 0.0f, 0.0f));
	memset(&mutated, 0xaa, sizeof(mutated));
	before_mutation = mutated;
	CHECK(yt_radio_reader_mutate(&mutated, 30.0f));
	CHECK(yt_radio_get_number(&mutated, 0) == 28.0f);
	CHECK(memcmp(mutated.bytes + 4, before_mutation.bytes + 4,
	    sizeof(mutated.bytes) - 4U) == 0);
	CHECK(yt_radio_reader_mutate(&mutated, 2.0f));
	CHECK(memcmp(mutated.bytes, "\0\0\0\0", 4) == 0);
	CHECK(yt_radio_reader_mutate(&mutated, 1.0f));
	CHECK(memcmp(mutated.bytes, radio_dirty_zero,
	    sizeof(radio_dirty_zero)) == 0);
	CHECK(yt_radio_reader_mutate(&mutated, 0.0f));
	CHECK(memcmp(mutated.bytes, radio_dirty_zero,
	    sizeof(radio_dirty_zero)) == 0);
	CHECK(yt_radio_reader_header((const uint8_t *)"A\0da", 4,
	    (const uint8_t *)"Bob", 3, header, sizeof(header),
	    &header_length));
	CHECK(header_length == 28U);
	CHECK(memcmp(header, "Message to: A\0da * From: Bob", 28) == 0);
	CHECK(!yt_radio_reader_header((const uint8_t *)"Ada", 3,
	    (const uint8_t *)"Bob", 3, header, 26, &header_length));
	CHECK(header_length == 0);

	yt_error_clear(&error);
	CHECK(yt_radio_reader_decide(2.0f, 9.0f, 8.0f, 7.0f, 0.0f,
	    &decision, &error));
	CHECK(!decision.log_heading);
	CHECK(decision.visible);
	CHECK(decision.automatic_write);
	CHECK(yt_radio_reader_decide(1.0f, 7.0f, 8.0f, 7.0f, 0.0f,
	    &decision, &error));
	CHECK(decision.visible);
	CHECK(decision.automatic_write);
	CHECK(yt_radio_reader_decide(1.0f, 8.0f, 7.0f, 7.0f, 0.0f,
	    &decision, &error));
	CHECK(!decision.visible);
	CHECK(!decision.automatic_write);
	CHECK(yt_radio_reader_decide(1.0f, 8.0f, 7.0f, 7.0f, 1.0f,
	    &decision, &error));
	CHECK(decision.log_heading);
	CHECK(decision.visible);
	CHECK(!decision.automatic_write);
	CHECK(yt_radio_reader_decide(0.0f, 8.0f, 9.0f, 7.0f, 1.0f,
	    &decision, &error));
	CHECK(!decision.visible);
	CHECK(yt_radio_reader_decide(0.0f, 7.0f, 9.0f, 7.0f, 1.0f,
	    &decision, &error));
	CHECK(decision.log_heading && decision.visible
	    && !decision.automatic_write);
	CHECK(yt_radio_reader_decide(0.0f, 9.0f, 7.0f, 7.0f, 1.0f,
	    &decision, &error));
	CHECK(decision.log_heading && decision.visible
	    && !decision.automatic_write);
	CHECK(yt_radio_reader_decide(0.0f, 0.0f, 0.0f, 0.0f, 2.0f,
	    &decision, &error));
	CHECK(decision.log_heading && decision.visible
	    && !decision.automatic_write);
	CHECK(yt_radio_reader_decide(1.0f, 7.0f, 8.0f, 7.0f, 0.49f,
	    &decision, &error));
	CHECK(decision.log_heading);
	CHECK(decision.visible);
	CHECK(!decision.automatic_write);
	CHECK(yt_radio_reader_decide(1.0f, 8.0f, 7.0f, 7.0f, 0.49f,
	    &decision, &error));
	CHECK(decision.log_heading);
	CHECK(!decision.visible);
	CHECK(!decision.automatic_write);
	CHECK(yt_radio_reader_decide(2.0f, 8.0f, 9.0f, 7.0f, 0.49f,
	    &decision, &error));
	CHECK(decision.log_heading && decision.visible
	    && !decision.automatic_write);
	yt_error_clear(&error);
	CHECK(!yt_radio_reader_decide(1.0f, 7.0f, 8.0f, 7.0f,
	    40000.0f, &decision, &error));
	CHECK(error.status == YT_RANGE);
	CHECK(strcmp(error.operation, "radio reader mode CINT") == 0);
}

struct scripted_clock {
	const struct yt_clock_value *values;
	size_t length;
	size_t position;
};

static bool
scripted_clock_read(void *context, struct yt_clock_value *value,
    struct yt_error *error)
{
	struct scripted_clock *script = context;

	if (script->position == script->length) {
		if (error != NULL)
			error->status = YT_IO_ERROR;
		return false;
	}
	*value = script->values[script->position++];
	return true;
}

static void
test_clock(void)
{
	static const struct yt_clock_value samples[] = {
		{1980, 1, 1, 12, 34, 56, 49},
		{2099, 12, 31, 23, 59, 59, 50}
	};
	struct scripted_clock script = {samples, YT_ARRAY_LEN(samples), 0};
	struct yt_clock_value actual;
	struct yt_error error;
	yt_error_clear(&error);
	yt_platform_set_clock_provider(scripted_clock_read, &script);
	CHECK(yt_platform_clock(&actual, &error));
	CHECK(memcmp(&actual, &samples[0], sizeof(actual)) == 0);
	CHECK(yt_platform_clock(&actual, &error));
	CHECK(memcmp(&actual, &samples[1], sizeof(actual)) == 0);
	CHECK(!yt_platform_clock(&actual, &error));
	CHECK(error.status == YT_IO_ERROR);
	yt_platform_set_clock_provider(NULL, NULL);
}

static void
test_random(void)
{
	static const uint8_t bytes[] = {0xb4, 0xf2, 0x2a, 0xa7, 0x96, 0x2d};
	static const uint8_t integer_bytes[] = {
		0x00, 0x00, 0x00,
		0xff, 0xff, 0xff,
		0x99, 0x99, 0x19,
		0x9a, 0x99, 0x19,
		0x80, 0x00, 0x80,
	};
	static const uint8_t one_based_bytes[] = {
		0x00, 0x00, 0x80,
		0x00, 0x00, 0x80,
		0x00, 0x00, 0x80,
	};
	static const uint8_t nested_bytes[] = {
		0x00, 0x00, 0x80,
		0x00, 0x00, 0x40,
		0xff, 0xff, 0xff,
	};
	static const uint8_t negative_nested_bytes[] = {
		0x00, 0x00, 0x80,
		0x00, 0x00, 0x80,
		0x00, 0x00, 0x80,
	};
	static const uint8_t market_bytes[] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
		0x00, 0x00, 0x40, 0x00, 0x00, 0xc0,
		0x00, 0x00, 0x80, 0x00, 0x00, 0x00
	};
	struct scripted_random script = {bytes, sizeof(bytes), 0};
	struct scripted_random market_script = {
		market_bytes, sizeof(market_bytes), 0
	};
	struct scripted_random integer_script = {
		integer_bytes, sizeof(integer_bytes), 0
	};
	struct scripted_random one_based_script = {
		one_based_bytes, sizeof(one_based_bytes), 0
	};
	struct scripted_random nested_script = {
		nested_bytes, sizeof(nested_bytes), 0
	};
	struct scripted_random negative_nested_script = {
		negative_nested_bytes, sizeof(negative_nested_bytes), 0
	};
	struct yt_random random;
	struct yt_error error;
	float value;
	float range;
	float bases[3];
	int integer = -1;

	yt_random_init(&random);
	yt_random_set_provider(&random, scripted_fill, &script);
	CHECK(yt_random_next(&random, &value, NULL));
	CHECK(value == 0.16776585578918457f);
	CHECK(yt_random_last(&random) == value);
	CHECK(yt_random_next(&random, &value, NULL));
	CHECK(value == 0.1780800223350525f);
	CHECK(random.draws == 2);
	yt_random_set_provider(&random, scripted_fill, &market_script);
	CHECK(yt_random_market_bases(&random, bases, NULL));
	CHECK(bases[0] == 22.5f);
	CHECK(bases[1] == 33.5f);
	CHECK(bases[2] == 35.0f);
	CHECK(random.draws == 6 && market_script.position == sizeof(market_bytes));

	yt_random_set_provider(&random, scripted_fill, &integer_script);
	CHECK(yt_random_integer(&random, 2004, &integer, NULL));
	CHECK(integer == 1);
	CHECK(yt_random_integer(&random, 2004, &integer, NULL));
	CHECK(integer == 2004);
	CHECK(yt_random_integer(&random, 10, &integer, NULL));
	CHECK(integer == 1);
	CHECK(yt_random_integer(&random, 10, &integer, NULL));
	CHECK(integer == 2);
	CHECK(yt_random_integer(&random, 1, &integer, NULL));
	CHECK(integer == 1 && random.draws == 5
	    && integer_script.position == sizeof(integer_bytes));
	yt_error_clear(&error);
	integer = 77;
	CHECK(!yt_random_integer(&random, 0, &integer, &error));
	CHECK(error.status == YT_RANGE && integer == 77 && random.draws == 5
	    && integer_script.position == sizeof(integer_bytes));
	yt_error_clear(&error);
	CHECK(!yt_random_integer(&random, 10, &integer, &error));
	CHECK(error.status == YT_RANDOM_ERROR && integer == 77
	    && random.draws == 5);

	yt_random_set_provider(&random, scripted_fill, &one_based_script);
	value = 77.0f;
	CHECK(yt_random_one_based_single(&random, 3.5f, &value, NULL));
	CHECK(value == 2.0f);
	CHECK(yt_random_one_based_single(&random, -3.5f, &value, NULL));
	CHECK(value == -1.0f);
	CHECK(yt_random_one_based_single(&random, 0.0f, &value, NULL));
	CHECK(value == 1.0f && random.draws == 3
	    && one_based_script.position == sizeof(one_based_bytes));
	yt_error_clear(&error);
	value = 77.0f;
	CHECK(!yt_random_one_based_single(&random, 3.5f, &value, &error));
	CHECK(error.status == YT_RANDOM_ERROR && value == 77.0f
	    && random.draws == 3);

	/* YT-SUB:4ADB copies its raw SINGLE terminal, mutates the range and
	 * result after every draw, and gates only exact numeric zero. */
	yt_random_set_provider(&random, scripted_fill, &nested_script);
	range = 100.0f;
	value = 77.0f;
	CHECK(yt_random_nested_single(&random, 3.0f, &range, &value, NULL));
	CHECK(value == 13.0f && range == 13.0f && random.draws == 3
	    && nested_script.position == sizeof(nested_bytes));
	range = 10.0f;
	value = 77.0f;
	CHECK(yt_random_nested_single(&random, 0.5f, &range, &value, NULL));
	CHECK(value == 77.0f && range == 10.0f && random.draws == 3);
	CHECK(yt_random_nested_single(&random, -2.0f, &range, &value, NULL));
	CHECK(value == 77.0f && range == 10.0f && random.draws == 3);
	CHECK(yt_random_nested_single(&random, 0.0f, &range, &value, NULL));
	CHECK(value == 77.0f && range == 10.0f && random.draws == 3);
	range = 0.0f;
	CHECK(yt_random_nested_single(&random, 3.0f, &range, &value, NULL));
	CHECK(value == 77.0f && range == 0.0f && random.draws == 3);

	yt_random_set_provider(&random, scripted_fill, &negative_nested_script);
	range = -3.5f;
	value = 77.0f;
	CHECK(yt_random_nested_single(&random, 2.5f, &range, &value, NULL));
	CHECK(value == 0.0f && range == 0.0f && random.draws == 2
	    && negative_nested_script.position == 6);

	/* A provider failure exposes every completed assignment prefix. */
	nested_script.position = 0;
	nested_script.length = 0;
	yt_random_set_provider(&random, scripted_fill, &nested_script);
	range = 100.0f;
	value = 77.0f;
	yt_error_clear(&error);
	CHECK(!yt_random_nested_single(&random, 3.0f, &range, &value, &error));
	CHECK(error.status == YT_RANDOM_ERROR && value == 77.0f
	    && range == 100.0f && random.draws == 0
	    && nested_script.position == 0);
	nested_script.length = 3;
	yt_random_set_provider(&random, scripted_fill, &nested_script);
	yt_error_clear(&error);
	CHECK(!yt_random_nested_single(&random, 3.0f, &range, &value, &error));
	CHECK(error.status == YT_RANDOM_ERROR && value == 51.0f
	    && range == 51.0f && random.draws == 1
	    && nested_script.position == 3);
	nested_script.length = 6;
	nested_script.position = 0;
	yt_random_set_provider(&random, scripted_fill, &nested_script);
	range = 100.0f;
	value = 77.0f;
	yt_error_clear(&error);
	CHECK(!yt_random_nested_single(&random, 3.0f, &range, &value, &error));
	CHECK(error.status == YT_RANDOM_ERROR && value == 13.0f
	    && range == 13.0f && random.draws == 2
	    && nested_script.position == 6);
	nested_script.length = 3;
	nested_script.position = 0;
	yt_random_set_provider(&random, scripted_fill, &nested_script);
	integer = 77;
	yt_error_clear(&error);
	CHECK(!yt_random_nested_integer(&random, 3, 100, &integer, &error));
	CHECK(error.status == YT_RANDOM_ERROR && integer == 51
	    && random.draws == 1 && nested_script.position == 3);
}

static void
test_files(void)
{
	char template_path[256];
	char database_path[320];
	char text_path[320];
	char mixed_path[320];
	char collision_path[320];
	char requested_path[320];
	char resolved_path[512];
	struct yt_database database;
	struct yt_record before;
	struct yt_record after;
	struct yt_text_file text;
	struct yt_error error;

#ifdef _WIN32
	snprintf(template_path, sizeof(template_path), "yt-test-%lu",
	    (unsigned long)GetCurrentProcessId());
#else
	snprintf(template_path, sizeof(template_path), "/tmp/yt-test-%ld",
	    (long)getpid());
#endif
	(void)mkdir_one(template_path);
	snprintf(database_path, sizeof(database_path), "%s/YTDATA.DAT",
	    template_path);
	snprintf(text_path, sizeof(text_path), "%s/YTNEWS.DAT", template_path);
	snprintf(mixed_path, sizeof(mixed_path), "%s/Mixed.Dat", template_path);
	snprintf(collision_path, sizeof(collision_path), "%s/MIXED.DAT",
	    template_path);
	snprintf(requested_path, sizeof(requested_path), "%s/mixed.dat",
	    template_path);

	yt_error_clear(&error);
	CHECK(yt_database_open(&database, database_path, YT_OPEN_CREATE, &error));
	yt_record_blank(&before);
	yt_record_set_text(&before, (const uint8_t *)"Test", 4);
	yt_record_set_number(&before, YT_F41, 42.0f);
	CHECK(yt_database_write(&database, 1, &before, &error));
	CHECK(yt_database_flush(&database, &error));
	CHECK(yt_database_read(&database, 1, &after, &error));
	CHECK(memcmp(before.bytes, after.bytes, sizeof(before.bytes)) == 0);
	yt_database_close(&database);

	CHECK(yt_text_append_line(text_path, (const uint8_t *)"One", 3, &error));
	CHECK(yt_text_append_line(text_path, (const uint8_t *)"Two", 3, &error));
	CHECK(yt_text_read(text_path, &text, &error));
	CHECK(text.length == 11);
	CHECK(memcmp(text.data, "One\r\nTwo\r\n\x1a", 11) == 0);
	yt_text_free(&text);

	CHECK(write_bytes(mixed_path, (const uint8_t *)"one", 3));
	CHECK(yt_resolve_case_path(requested_path, false, resolved_path,
	    sizeof(resolved_path), &error));
	CHECK(strcmp(resolved_path, mixed_path) == 0);
	CHECK(write_bytes(collision_path, (const uint8_t *)"two", 3));
	CHECK(yt_resolve_case_path(mixed_path, false, resolved_path,
	    sizeof(resolved_path), &error));
	CHECK(strcmp(resolved_path, mixed_path) == 0);
	yt_error_clear(&error);
	CHECK(!yt_resolve_case_path(requested_path, false, resolved_path,
	    sizeof(resolved_path), &error));
	CHECK(error.status == YT_INVALID);

	CHECK(yt_file_delete(database_path, false, &error));
	CHECK(yt_file_delete(text_path, false, &error));
	CHECK(yt_file_delete(mixed_path, false, &error));
	CHECK(yt_file_delete(collision_path, false, &error));
#ifdef _WIN32
	_rmdir(template_path);
#else
	rmdir(template_path);
#endif
}

static bool
write_bytes(const char *path, const uint8_t *bytes, size_t length)
{
	FILE *file = fopen(path, "wb");
	bool ok;

	if (file == NULL)
		return false;
	ok = length == 0 || fwrite(bytes, 1, length, file) == length;
	return fclose(file) == 0 && ok;
}

static void
test_append_window(void)
{
	char directory[256];
	char path[320];
	uint8_t original[200];
	uint8_t embedded[] = {'A', 0x1a, 'B'};
	struct yt_text_file text;
	struct yt_error error;
	size_t index;

#ifdef _WIN32
	snprintf(directory, sizeof(directory), "yt-append-%lu",
	    (unsigned long)GetCurrentProcessId());
#else
	snprintf(directory, sizeof(directory), "/tmp/yt-append-%ld",
	    (long)getpid());
#endif
	(void)mkdir_one(directory);
	snprintf(path, sizeof(path), "%s/window.dat", directory);
	for (index = 0; index < sizeof(original); ++index)
		original[index] = (uint8_t)(index == 0 ? 0 : 'x');

	/* The marker at length-129 is ignored; length-128 is included. */
	original[71] = 0x1a;
	original[72] = 0x1a;
	original[90] = 0x1a;
	CHECK(write_bytes(path, original, sizeof(original)));
	yt_error_clear(&error);
	CHECK(yt_text_append_line(path, (const uint8_t *)"Z", 1, &error));
	CHECK(yt_text_read(path, &text, &error));
	CHECK(text.length == 76);
	CHECK(memcmp(text.data, original, 72) == 0);
	CHECK(memcmp(text.data + 72, "Z\r\n\x1a", 4) == 0);
	CHECK(text.data[0] == 0);
	yt_text_free(&text);

	/* With no marker in the final window, APPEND starts at physical EOF. */
	memset(original, 'q', sizeof(original));
	original[71] = 0x1a;
	CHECK(write_bytes(path, original, sizeof(original)));
	CHECK(yt_text_append_line(path, NULL, 0, &error));
	CHECK(yt_text_read(path, &text, &error));
	CHECK(text.length == 203);
	CHECK(memcmp(text.data, original, sizeof(original)) == 0);
	CHECK(memcmp(text.data + sizeof(original), "\r\n\x1a", 3) == 0);
	yt_text_free(&text);

	/* An emitted EOF byte is literal and becomes the next append point. */
	CHECK(write_bytes(path, NULL, 0));
	CHECK(yt_text_append_line(path, embedded, sizeof(embedded), &error));
	CHECK(yt_text_append_line(path, (const uint8_t *)"C", 1, &error));
	CHECK(yt_text_read(path, &text, &error));
	CHECK(text.length == 5);
	CHECK(memcmp(text.data, "AC\r\n\x1a", 5) == 0);
	yt_text_free(&text);

	CHECK(yt_file_delete(path, false, &error));
#ifdef _WIN32
	_rmdir(directory);
#else
	rmdir(directory);
#endif
}

static void
test_main_error_fatal_transaction(void)
{
	static const uint8_t record[] =
	    "YTMerg2 1.15 Untrapped Error ERL= 12345 ERR= 11 "
	    "Date >07-23-2026 14:05:09";
	static const uint8_t stale[] = "old\r\n\x1a" "STALE";
	static const uint8_t stale_expected[] =
	    "old\r\n"
	    "YTMerg2 1.15 Untrapped Error ERL= 12345 ERR= 11 "
	    "Date >07-23-2026 14:05:09\r\n\x1a";
	char directory[256];
	char path[320];
	char failed_path[320];
	struct yt_main_error_result result;
	struct yt_main_error_result invalid;
	struct main_error_present_capture capture;
	struct yt_text_file text;
	struct yt_error error;

#ifdef _WIN32
	snprintf(directory, sizeof(directory), "yt-main-error-%lu",
	    (unsigned long)GetCurrentProcessId());
#else
	snprintf(directory, sizeof(directory), "/tmp/yt-main-error-%ld",
	    (long)getpid());
#endif
	(void)mkdir_one(directory);
	snprintf(path, sizeof(path), "%s/ERRORS.DOR", directory);
	snprintf(failed_path, sizeof(failed_path), "%s", directory);
	yt_error_clear(&error);
	CHECK(yt_main_error_compose(11, 12345, NULL, 0U,
	    (const uint8_t *)"07-23-2026", strlen("07-23-2026"),
	    (const uint8_t *)"14:05:09", strlen("14:05:09"), &result));
	CHECK(result.route == YT_MAIN_ERROR_FATAL
	    && result.action_length == sizeof(record) - 1U
	    && memcmp(result.action, record, sizeof(record) - 1U) == 0);

	memset(&capture, 0, sizeof(capture));
	capture.path = path;
	capture.succeeds = true;
	CHECK(yt_main_error_commit_fatal_to(path, &result,
	    capture_main_error_row, &capture, &error));
	CHECK(capture.calls == 1U
	    && capture.row_length == sizeof(record) - 1U
	    && memcmp(capture.row, record, sizeof(record) - 1U) == 0);
	CHECK(yt_text_read(path, &text, &error));
	CHECK(text.length == sizeof(record) + 2U
	    && memcmp(text.data, record, sizeof(record) - 1U) == 0
	    && memcmp(text.data + sizeof(record) - 1U, "\r\n\x1a", 3U) == 0);
	yt_text_free(&text);

	CHECK(write_bytes(path, stale, sizeof(stale) - 1U));
	memset(&capture, 0, sizeof(capture));
	capture.path = path;
	capture.expected_before = stale;
	capture.expected_before_length = sizeof(stale) - 1U;
	capture.succeeds = true;
	CHECK(yt_main_error_commit_fatal_to(path, &result,
	    capture_main_error_row, &capture, &error));
	CHECK(capture.calls == 1U);
	CHECK(yt_text_read(path, &text, &error));
	CHECK(text.length == sizeof(stale_expected) - 1U
	    && memcmp(text.data, stale_expected,
	    sizeof(stale_expected) - 1U) == 0);
	yt_text_free(&text);

	CHECK(write_bytes(path, stale, sizeof(stale) - 1U));
	memset(&capture, 0, sizeof(capture));
	capture.path = path;
	capture.expected_before = stale;
	capture.expected_before_length = sizeof(stale) - 1U;
	capture.succeeds = false;
	yt_error_clear(&error);
	CHECK(!yt_main_error_commit_fatal_to(path, &result,
	    capture_main_error_row, &capture, &error));
	CHECK(capture.calls == 1U && error.status == YT_IO_ERROR);
	CHECK(yt_text_read(path, &text, &error));
	CHECK(text.length == sizeof(stale) - 1U
	    && memcmp(text.data, stale, sizeof(stale) - 1U) == 0);
	yt_text_free(&text);

	invalid = result;
	invalid.route = YT_MAIN_ERROR_GAMEPLAY;
	memset(&capture, 0, sizeof(capture));
	capture.path = path;
	capture.expected_before = stale;
	capture.expected_before_length = sizeof(stale) - 1U;
	capture.succeeds = true;
	yt_error_clear(&error);
	CHECK(!yt_main_error_commit_fatal_to(path, &invalid,
	    capture_main_error_row, &capture, &error));
	CHECK(capture.calls == 0U && error.status == YT_INVALID);

	memset(&capture, 0, sizeof(capture));
	capture.path = path;
	capture.expected_before = stale;
	capture.expected_before_length = sizeof(stale) - 1U;
	capture.succeeds = true;
	yt_error_clear(&error);
	CHECK(!yt_main_error_commit_fatal_to(failed_path, &result,
	    capture_main_error_row, &capture, &error));
	CHECK(capture.calls == 1U && error.status == YT_IO_ERROR);
	CHECK(yt_text_read(path, &text, &error));
	CHECK(text.length == sizeof(stale) - 1U
	    && memcmp(text.data, stale, sizeof(stale) - 1U) == 0);
	yt_text_free(&text);

	CHECK(yt_file_delete(path, false, &error));
#ifdef _WIN32
	_rmdir(directory);
#else
	rmdir(directory);
#endif
}

static void
test_line_input_grammar(void)
{
	static const uint8_t source[] = {
		'\r', '\n', 'A', 0, 'B', '\r', '\n',
		'C', '\n', 'D', '\r', 'E', 0x1a, 'Z'
	};
	static const uint8_t expected_one[] = {'A', 'B'};
	static const uint8_t expected_two[] = {'C', '\n', 'D'};
	uint8_t line[16];
	size_t cursor = 0U;
	size_t length;
	bool available;

	CHECK(yt_text_line_input_next(source, sizeof(source), &cursor,
	    line, sizeof(line), &length, &available));
	CHECK(available && length == 0U && cursor == 2U);
	CHECK(yt_text_line_input_next(source, sizeof(source), &cursor,
	    line, sizeof(line), &length, &available));
	CHECK(available && length == sizeof(expected_one)
	    && memcmp(line, expected_one, length) == 0 && cursor == 7U);
	CHECK(yt_text_line_input_next(source, sizeof(source), &cursor,
	    line, sizeof(line), &length, &available));
	CHECK(available && length == sizeof(expected_two)
	    && memcmp(line, expected_two, length) == 0 && cursor == 11U);
	CHECK(yt_text_line_input_next(source, sizeof(source), &cursor,
	    line, sizeof(line), &length, &available));
	CHECK(available && length == 1U && line[0] == 'E' && cursor == 12U);
	CHECK(yt_text_line_input_next(source, sizeof(source), &cursor,
	    line, sizeof(line), &length, &available));
	CHECK(!available && length == 0U && cursor == 12U);

	cursor = 2U;
	CHECK(!yt_text_line_input_next(source, sizeof(source), &cursor,
	    line, 1U, &length, &available));
	CHECK(cursor == 2U);
	{
		static const uint8_t nul_tail[] = {0, 0x1a};

		cursor = 0U;
		CHECK(yt_text_line_input_next(nul_tail, sizeof(nul_tail), &cursor,
		    line, sizeof(line), &length, &available));
		CHECK(available && length == 0U && cursor == 1U);
	}
	{
		FILE *file = tmpfile();

		CHECK(file != NULL);
		if (file != NULL) {
			CHECK(fwrite(source, 1U, sizeof(source), file)
			    == sizeof(source));
			rewind(file);
			CHECK(yt_text_stream_line_input_next(file, line,
			    sizeof(line), &length) == YT_TEXT_STREAM_LINE_OK
			    && length == 0U);
			CHECK(yt_text_stream_line_input_next(file, line, 1U,
			    &length) == YT_TEXT_STREAM_LINE_TOO_LONG);
			CHECK(yt_text_stream_line_input_next(file, line,
			    sizeof(line), &length) == YT_TEXT_STREAM_LINE_OK
			    && length == sizeof(expected_two)
			    && memcmp(line, expected_two, length) == 0);
			CHECK(yt_text_stream_line_input_next(file, line,
			    sizeof(line), &length) == YT_TEXT_STREAM_LINE_OK
			    && length == 1U && line[0] == 'E');
			CHECK(yt_text_stream_line_input_next(file, line,
			    sizeof(line), &length) == YT_TEXT_STREAM_LINE_EOF);
			CHECK(yt_text_stream_line_input_next(file, line,
			    sizeof(line), &length) == YT_TEXT_STREAM_LINE_EOF);
			CHECK(fclose(file) == 0);
		}
	}
}

static void
test_file_viewer_records(void)
{
	static const uint8_t source[] =
	    "ordinary\r\n  - item\r\n ***blue\r\n +++red\r\n-=*white\r\nA\nB\r\n\x1aZ";
	static const int foreground[] = {2, 3, 4, 1, 7, 2};
	struct yt_file_viewer_record record;
	uint8_t line[32];
	size_t cursor = 0U;
	size_t index;

	for (index = 0U; index < YT_ARRAY_LEN(foreground); ++index) {
		CHECK(yt_file_viewer_next(source, sizeof(source) - 1U, &cursor,
		    "", line, sizeof(line), &record));
		CHECK(record.eof_checked && record.key_checked && record.available
		    && record.foreground == foreground[index]
		    && record.set_bold == (foreground[index] != 2));
	}
	CHECK(record.length == 3U && line[0] == 'A' && line[1] == '\n'
	    && line[2] == 'B');
	CHECK(yt_file_viewer_next(source, sizeof(source) - 1U, &cursor, "Q",
	    line, sizeof(line), &record));
	CHECK(record.eof_checked && record.key_checked && !record.available);

	cursor = 0U;
	CHECK(yt_file_viewer_next(source, sizeof(source) - 1U, &cursor, "Q",
	    line, sizeof(line), &record));
	CHECK(record.eof_checked && record.key_checked && !record.available
	    && cursor == 0U);
}

struct viewer_play_tape {
	int events[4];
	size_t lengths[4];
	size_t calls;
	size_t fail_call;
};

struct viewer_entry_tape {
	int events[2];
	size_t calls;
	size_t fail_call;
	char *key;
};

static bool
viewer_entry_present(void *context, const uint8_t *text, size_t length,
    bool paged, struct yt_error *error)
{
	static const uint8_t notice[] = "Cntl-X to Stop";
	struct viewer_entry_tape *tape = context;

	(void)error;
	CHECK(tape->key[0] == '\0');
	if (paged)
		CHECK(length == sizeof(notice) - 1U
		    && memcmp(text, notice, length) == 0);
	else
		CHECK(text == NULL && length == 0U);
	tape->events[tape->calls++] = paged ? 1 : 2;
	return tape->fail_call == 0U || tape->calls != tape->fail_call;
}

static void
test_file_viewer_entry(void)
{
	struct viewer_entry_tape tape;
	char key[2] = "Q";
	float line_count = 17.0f;

	memset(&tape, 0, sizeof(tape));
	tape.key = key;
	CHECK(yt_file_viewer_entry(key, &line_count, viewer_entry_present,
	    &tape, NULL));
	CHECK(tape.calls == 2U && tape.events[0] == 1 && tape.events[1] == 2
	    && key[0] == '\0' && line_count == 0.0f);
	memset(&tape, 0, sizeof(tape));
	tape.key = key;
	tape.fail_call = 1U;
	key[0] = 'Q';
	line_count = 17.0f;
	CHECK(!yt_file_viewer_entry(key, &line_count, viewer_entry_present,
	    &tape, NULL) && tape.calls == 1U && key[0] == '\0'
	    && line_count == 17.0f);
	memset(&tape, 0, sizeof(tape));
	tape.key = key;
	tape.fail_call = 2U;
	key[0] = 'Q';
	CHECK(!yt_file_viewer_entry(key, &line_count, viewer_entry_present,
	    &tape, NULL) && tape.calls == 2U && line_count == 17.0f);
}

static bool
viewer_play_present(void *context, const uint8_t *text, size_t length,
    bool paged, struct yt_error *error)
{
	struct viewer_play_tape *tape = context;
	size_t call = tape->calls++;

	(void)error;
	if (call >= YT_ARRAY_LEN(tape->events))
		return false;
	tape->events[call] = paged ? 1 : 2;
	tape->lengths[call] = length;
	if (!paged)
		CHECK(text == NULL && length == 0U);
	return tape->fail_call == 0U || call + 1U != tape->fail_call;
}

static void
test_file_viewer_play(void)
{
	static const uint8_t source[] = "ordinary\r\n  - item\r\n";
	struct yt_file_viewer_play_state state;
	struct viewer_play_tape tape;
	float foreground = 5.0f;
	float bold = 0.0f;
	float line_count = 17.0f;
	int pager_foreground = 5;
	char key[2] = "";

	memset(&tape, 0, sizeof(tape));
	state.foreground = &foreground;
	state.pager_foreground = &pager_foreground;
	state.bold = &bold;
	state.line_count = &line_count;
	state.pager_key = key;
	state.saved_foreground = 5.0f;
	state.saved_pager_foreground = 5;
	CHECK(yt_file_viewer_play(source, sizeof(source) - 1U, &state,
	    viewer_play_present, &tape, NULL));
	CHECK(tape.calls == 3U && tape.events[0] == 1 && tape.events[1] == 1
	    && tape.events[2] == 2 && tape.lengths[0] == 8U
	    && tape.lengths[1] == 8U && foreground == 5.0f
	    && pager_foreground == 5 && bold == 1.0f && line_count == 0.0f);

	memset(&tape, 0, sizeof(tape));
	tape.fail_call = 2U;
	foreground = 5.0f;
	pager_foreground = 5;
	bold = 0.0f;
	line_count = 17.0f;
	CHECK(!yt_file_viewer_play(source, sizeof(source) - 1U, &state,
	    viewer_play_present, &tape, NULL));
	CHECK(tape.calls == 2U && foreground == 3.0f
	    && pager_foreground == 3 && bold == 1.0f
	    && line_count == 17.0f);
}

struct viewer_missing_tape {
	int events[2];
	size_t calls;
	size_t fail_call;
	uint8_t row[96];
	size_t length;
};

static bool
viewer_missing_present(void *context, const uint8_t *text, size_t length,
    bool paged, struct yt_error *error)
{
	struct viewer_missing_tape *tape = context;

	(void)error;
	CHECK(paged && length <= sizeof(tape->row));
	tape->events[tape->calls++] = 1;
	memcpy(tape->row, text, length);
	tape->length = length;
	return tape->fail_call != tape->calls;
}

static bool
viewer_missing_news(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	struct viewer_missing_tape *tape = context;

	(void)error;
	CHECK(length == tape->length
	    && memcmp(text, tape->row, length) == 0);
	tape->events[tape->calls++] = 2;
	return tape->fail_call != tape->calls;
}

static void
test_file_viewer_missing(void)
{
	static const uint8_t path[] = {'A', 0, 'B'};
	static const uint8_t expected[] =
	    "*** GAME FILE [A\0B] NOT FOUND! ***";
	struct viewer_missing_tape tape;

	memset(&tape, 0, sizeof(tape));
	CHECK(yt_file_viewer_missing(path, sizeof(path), viewer_missing_present,
	    viewer_missing_news, &tape, NULL));
	CHECK(tape.calls == 2U && tape.events[0] == 1 && tape.events[1] == 2
	    && tape.length == sizeof(expected) - 1U
	    && memcmp(tape.row, expected, sizeof(expected) - 1U) == 0);
	memset(&tape, 0, sizeof(tape));
	tape.fail_call = 1U;
	CHECK(!yt_file_viewer_missing(path, sizeof(path), viewer_missing_present,
	    viewer_missing_news, &tape, NULL) && tape.calls == 1U);
	memset(&tape, 0, sizeof(tape));
	tape.fail_call = 2U;
	CHECK(!yt_file_viewer_missing(path, sizeof(path), viewer_missing_present,
	    viewer_missing_news, &tape, NULL) && tape.calls == 2U);
}

int
main(void)
{
	test_record();
	test_clock();
	test_random();
	test_files();
	test_append_window();
	test_main_error_fatal_transaction();
	test_line_input_grammar();
	test_file_viewer_records();
	test_file_viewer_entry();
	test_file_viewer_play();
	test_file_viewer_missing();
	if (failures != 0) {
		fprintf(stderr, "%u test(s) failed\n", failures);
		return EXIT_FAILURE;
	}
	puts("test_data: ok");
	return EXIT_SUCCESS;
}
