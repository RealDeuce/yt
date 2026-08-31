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

struct database_write_script {
	size_t accepted;
	bool carry;
	uint16_t dos_error;
	uint16_t mapped_error;
	int64_t terminal_position;
	size_t calls;
	size_t requested;
};

struct database_seek_script {
	bool success;
	uint16_t dos_error;
	int64_t terminal_position;
	size_t calls;
	int64_t absolute_offset;
};

struct database_close_script {
	bool success;
	bool handle_open;
	size_t calls;
};

struct database_flush_script {
	bool success;
	size_t calls;
};

static bool
scripted_database_write(void *context, FILE *file, const uint8_t *data,
    size_t requested, struct yt_database_write_observation *observation)
{
	struct database_write_script *script = context;
	size_t count = script->accepted < requested ? script->accepted : requested;

	++script->calls;
	script->requested = requested;
	memset(observation, 0, sizeof(*observation));
	observation->accepted = fwrite(data, 1U, count, file);
	observation->carry = script->carry;
	observation->dos_error = script->dos_error;
	observation->mapped_error = script->mapped_error;
	observation->terminal_position = script->terminal_position;
	return observation->accepted == count && fflush(file) == 0;
}

static bool
scripted_database_seek(void *context, FILE *file, int64_t absolute_offset,
    struct yt_database_seek_observation *observation)
{
	struct database_seek_script *script = context;

	(void)file;
	++script->calls;
	script->absolute_offset = absolute_offset;
	observation->carry = !script->success;
	observation->dos_error = script->success ? 0U : script->dos_error;
	observation->terminal_position = script->terminal_position;
	return true;
}

static bool
scripted_database_close(void *context, FILE *file, bool *handle_open)
{
	struct database_close_script *script = context;

	++script->calls;
	*handle_open = script->handle_open;
	if (!script->success)
		return false;
	*handle_open = false;
	return fclose(file) == 0;
}

static bool
scripted_database_flush(void *context, FILE *file)
{
	struct database_flush_script *script = context;

	(void)file;
	++script->calls;
	return script->success;
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
	struct yt_record replacement;
	struct database_write_script write_script;
	struct database_seek_script seek_script;
	struct database_close_script close_script;
	struct database_flush_script flush_script;
	struct yt_database observer;
	struct yt_text_file text;
	struct yt_error error;
	size_t accepted;
	size_t index;
	unsigned dos_error;

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
	for (index = 0U; index < sizeof(replacement.bytes); ++index)
		replacement.bytes[index] = (uint8_t)(index ^ 0xa5U);
	write_script = (struct database_write_script){.accepted = 136U};
	yt_database_set_write_provider(&database, scripted_database_write,
	    &write_script);
	CHECK(yt_database_write(&database, 1U, &replacement, &error));
	CHECK(write_script.calls == 1U
	    && write_script.requested == YT_RECORD_SIZE);
	CHECK(database.last_put.outcome == YT_DATABASE_PUT_RETURNED
	    && database.last_put.accepted == 136U
	    && database.last_put.basic_error == 0U
	    && database.last_put.terminal_position == 136
	    && database.last_put.registered && database.last_put.handle_open);
	CHECK(yt_database_read(&database, 1U, &after, &error));
	CHECK(memcmp(after.bytes, replacement.bytes, 136U) == 0
	    && after.bytes[136] == before.bytes[136]);
	yt_database_set_write_provider(&database, NULL, NULL);
	CHECK(yt_database_write(&database, 1U, &before, &error));
	write_script = (struct database_write_script){.accepted = 136U};
	yt_database_set_write_provider(&database, scripted_database_write,
	    &write_script);
	yt_error_clear(&error);
	CHECK(!yt_database_random_put(&database, 1U, &replacement, false,
	    &accepted, &error) && error.status == YT_IO_ERROR
	    && accepted == 136U
	    && strcmp(error.operation, "random PUT rejected short") == 0);
	CHECK(database.file == NULL && database.orphaned_file == NULL
	    && database.records == 0U && database.short_close_attempted
	    && database.short_close_succeeded);
	CHECK(database.last_put.outcome == YT_DATABASE_PUT_REJECTED_SHORT
	    && database.last_put.accepted == 136U
	    && database.last_put.basic_error == 61U
	    && database.last_put.terminal_position == 136
	    && !database.last_put.registered
	    && database.last_put.close_attempted
	    && database.last_put.close_succeeded
	    && !database.last_put.handle_open);
	CHECK(yt_database_open(&database, database_path, YT_OPEN_UPDATE, &error));
	CHECK(yt_database_read(&database, 1U, &after, &error));
	CHECK(memcmp(after.bytes, replacement.bytes, 136U) == 0
	    && after.bytes[136] == before.bytes[136]);
	yt_database_set_write_provider(&database, NULL, NULL);
	CHECK(yt_database_write(&database, 1U, &before, &error));
	write_script = (struct database_write_script){
		.accepted = 3U,
		.carry = true,
		.dos_error = 6U,
		.terminal_position = 0x55667788,
	};
	yt_database_set_write_provider(&database, scripted_database_write,
	    &write_script);
	yt_error_clear(&error);
	CHECK(!yt_database_random_put(&database, 1U, &replacement, true,
	    &accepted, &error) && error.status == YT_IO_ERROR
	    && accepted == 3U);
	CHECK(database.last_put.outcome == YT_DATABASE_PUT_WRITE_ERROR
	    && database.last_put.accepted == 3U
	    && database.last_put.dos_error == 6U
	    && database.last_put.basic_error == 57U
	    && database.last_put.terminal_position == 0x55667788
	    && database.last_put.registered && database.last_put.handle_open
	    && !database.last_put.close_attempted);
	CHECK(yt_database_read(&database, 1U, &after, &error));
	CHECK(memcmp(after.bytes, replacement.bytes, 3U) == 0
	    && memcmp(after.bytes + 3U, before.bytes + 3U,
	    YT_RECORD_SIZE - 3U) == 0);
	yt_database_set_write_provider(&database, NULL, NULL);
	CHECK(yt_database_write(&database, 1U, &before, &error));
	write_script = (struct database_write_script){.accepted = 3U};
	close_script = (struct database_close_script){false, true, 0U};
	yt_database_set_write_provider(&database, scripted_database_write,
	    &write_script);
	yt_database_set_close_provider(&database, scripted_database_close,
	    &close_script);
	yt_error_clear(&error);
	CHECK(!yt_database_random_put(&database, 1U, &replacement, true,
	    &accepted, &error) && error.status == YT_IO_ERROR
	    && accepted == 3U && close_script.calls == 1U);
	CHECK(database.file == NULL && database.orphaned_file != NULL
	    && database.records == 0U && database.short_close_attempted
	    && !database.short_close_succeeded);
	CHECK(database.last_put.outcome == YT_DATABASE_PUT_REJECTED_SHORT
	    && database.last_put.basic_error == 61U
	    && !database.last_put.registered
	    && database.last_put.close_attempted
	    && !database.last_put.close_succeeded
	    && database.last_put.handle_open);
	CHECK(yt_database_open(&observer, database_path, YT_OPEN_READ, &error));
	CHECK(yt_database_read(&observer, 1U, &after, &error));
	CHECK(memcmp(after.bytes, replacement.bytes, 3U) == 0
	    && memcmp(after.bytes + 3U, before.bytes + 3U,
	    YT_RECORD_SIZE - 3U) == 0);
	yt_database_close(&observer);
	yt_database_close(&database);
	CHECK(yt_database_open(&database, database_path, YT_OPEN_UPDATE, &error));
	seek_script = (struct database_seek_script){0};
	write_script = (struct database_write_script){.accepted = 137U};
	yt_database_set_seek_provider(&database, scripted_database_seek,
	    &seek_script);
	yt_database_set_write_provider(&database, scripted_database_write,
	    &write_script);
	for (dos_error = 1U; dos_error <= 0xffU; ++dos_error) {
		seek_script = (struct database_seek_script){
			.success = false,
			.dos_error = (uint16_t)dos_error,
			.terminal_position = (int64_t)(0x4000U + dos_error),
		};
		yt_error_clear(&error);
		CHECK(!yt_database_random_put(&database, 1U, &replacement, true,
		    &accepted, &error) && error.status == YT_IO_ERROR
		    && accepted == 0U && seek_script.calls == 1U
		    && seek_script.absolute_offset == 0
		    && write_script.calls == 0U);
		CHECK(database.last_put.outcome == YT_DATABASE_PUT_SEEK_ERROR
		    && database.last_put.dos_error == dos_error
		    && database.last_put.basic_error == 52U
		    && database.last_put.terminal_position
		    == (int64_t)(0x4000U + dos_error)
		    && database.last_put.registered
		    && database.last_put.handle_open
		    && !database.last_put.close_attempted);
	}
	CHECK(database.file != NULL && database.orphaned_file == NULL);
	yt_database_set_seek_provider(&database, NULL, NULL);
	for (dos_error = 1U; dos_error <= 0xffU; ++dos_error) {
		write_script = (struct database_write_script){
			.carry = true,
			.dos_error = (uint16_t)dos_error,
			.mapped_error = dos_error == 5U ? 70U : 0U,
			.terminal_position = (int64_t)(0x2000U + dos_error),
		};
		yt_error_clear(&error);
		CHECK(!yt_database_random_put(&database, 1U, &replacement, true,
		    &accepted, &error) && error.status == YT_IO_ERROR
		    && accepted == 0U && write_script.calls == 1U);
		CHECK(database.last_put.outcome == YT_DATABASE_PUT_WRITE_ERROR
		    && database.last_put.dos_error == dos_error
		    && database.last_put.basic_error
		    == (dos_error == 5U ? 70U : 57U)
		    && database.last_put.terminal_position
		    == (int64_t)(0x2000U + dos_error)
		    && database.last_put.registered
		    && database.last_put.handle_open
		    && !database.last_put.close_attempted);
	}
	write_script = (struct database_write_script){
		.carry = true,
		.dos_error = 5U,
		.mapped_error = 75U,
		.terminal_position = 0x2750,
	};
	yt_error_clear(&error);
	CHECK(!yt_database_random_put(&database, 1U, &replacement, true,
	    &accepted, &error) && database.last_put.basic_error == 75U
	    && database.last_put.dos_error == 5U
	    && database.last_put.terminal_position == 0x2750);
	yt_database_set_write_provider(&database, NULL, NULL);
	flush_script = (struct database_flush_script){false, 0U};
	yt_database_set_flush_provider(&database, scripted_database_flush,
	    &flush_script);
	yt_error_clear(&error);
	CHECK(!yt_database_flush(&database, &error)
	    && error.status == YT_IO_ERROR && flush_script.calls == 1U
	    && database.file != NULL);
	yt_database_set_flush_provider(&database, NULL, NULL);
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

static void
test_radio_file(void)
{
	char directory[256];
	char mixed_path[320];
	char requested_path[320];
	char second_path[320];
	char failed_path[320];
	uint8_t partial[3] = {0x11, 0x22, 0x33};
	uint8_t complete[YT_RADIO_RECORD_SIZE];
	struct yt_radio_file radio;
	struct yt_radio_record record;
	struct yt_radio_record written;
	struct yt_error error;
	uint64_t size;
	uint32_t next;
	size_t accepted;
	size_t index;
	FILE *file;

#ifdef _WIN32
	snprintf(directory, sizeof(directory), "yt-radio-%lu",
	    (unsigned long)GetCurrentProcessId());
#else
	snprintf(directory, sizeof(directory), "/tmp/yt-radio-%ld",
	    (long)getpid());
#endif
	(void)mkdir_one(directory);
	snprintf(mixed_path, sizeof(mixed_path), "%s/ytRMSG.Dat", directory);
	snprintf(requested_path, sizeof(requested_path), "%s/YTRMSG.DAT",
	    directory);
	snprintf(second_path, sizeof(second_path), "%s/second.dat", directory);
	snprintf(failed_path, sizeof(failed_path), "%s/missing/YTRMSG.DAT",
	    directory);
	CHECK(write_bytes(mixed_path, partial, sizeof(partial)));

	yt_radio_file_init(&radio);
	yt_error_clear(&error);
	CHECK(yt_radio_file_open(&radio, requested_path, &error));
	CHECK(radio.file != NULL && strcmp(radio.path, mixed_path) == 0
	    && radio.record_length == YT_RADIO_RECORD_SIZE
	    && radio.field_count == YT_RADIO_FIELD_COUNT
	    && radio.fields[0].offset == 0U && radio.fields[0].length == 4U
	    && radio.fields[1].offset == 4U && radio.fields[1].length == 4U
	    && radio.fields[2].offset == 8U && radio.fields[2].length == 4U
	    && radio.fields[3].offset == 12U && radio.fields[3].length == 74U);
	CHECK(yt_radio_file_size(&radio, &size, &error) && size == 3U);
	memset(&record, 0xff, sizeof(record));
	CHECK(yt_radio_file_get(&radio, 1U, &record, &accepted, &error)
	    && accepted == sizeof(partial)
	    && memcmp(record.bytes, partial, sizeof(partial)) == 0);
	for (index = sizeof(partial); index < sizeof(record.bytes); ++index)
		CHECK(record.bytes[index] == 0U);
	memset(&record, 0xff, sizeof(record));
	CHECK(yt_radio_file_get(&radio, 2U, &record, &accepted, &error)
	    && accepted == 0U);
	for (index = 0U; index < sizeof(record.bytes); ++index)
		CHECK(record.bytes[index] == 0U);
	yt_error_clear(&error);
	CHECK(!yt_radio_file_next_record(&radio, &next, &error)
	    && error.status == YT_RANGE
	    && strcmp(error.operation, "radio record number") == 0);
	CHECK(yt_radio_file_size(&radio, &size, &error) && size == 3U);

	/* Reopening the same BASIC file slot closes the prior handle first. */
	CHECK(yt_radio_file_open(&radio, second_path, &error));
	CHECK(strcmp(radio.path, second_path) == 0
	    && yt_radio_file_size(&radio, &size, &error) && size == 0U);
	CHECK(yt_radio_file_next_record(&radio, &next, &error) && next == 1U);
	CHECK(yt_radio_message_record(&written, (const uint8_t *)"A\0B", 3U,
	    7.0f, -2.0f));
	CHECK(yt_radio_file_get(&radio, next, &record, &accepted, &error)
	    && accepted == 0U);
	record = written;
	CHECK(yt_radio_file_put(&radio, next, &record, &error));
	CHECK(yt_radio_file_size(&radio, &size, &error)
	    && size == YT_RADIO_RECORD_SIZE);
	CHECK(yt_radio_file_next_record(&radio, &next, &error) && next == 2U);
	CHECK(yt_radio_file_close(&radio, &error) && radio.file == NULL);

	file = fopen(second_path, "rb");
	CHECK(file != NULL);
	if (file != NULL) {
		CHECK(fread(complete, 1, sizeof(complete), file) == sizeof(complete));
		CHECK(fgetc(file) == EOF && !ferror(file));
		CHECK(fclose(file) == 0);
		CHECK(memcmp(complete, written.bytes, sizeof(complete)) == 0);
	}

	yt_radio_file_init(&radio);
	yt_error_clear(&error);
	CHECK(!yt_radio_file_open(&radio, failed_path, &error)
	    && error.status == YT_IO_ERROR && radio.file == NULL
	    && radio.field_count == 0U && radio.record_length == 0U);
	CHECK(!yt_radio_file_get(&radio, 1U, &record, NULL, &error));
	CHECK(!yt_radio_file_put(&radio, 1U, &record, &error));

	CHECK(yt_file_delete(mixed_path, false, &error));
	CHECK(yt_file_delete(second_path, false, &error));
#ifdef _WIN32
	_rmdir(directory);
#else
	rmdir(directory);
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

enum sequential_play_event {
	SEQUENTIAL_PLAY_PRECLOSE = 1,
	SEQUENTIAL_PLAY_OPEN,
	SEQUENTIAL_PLAY_READ,
	SEQUENTIAL_PLAY_PRESENT,
	SEQUENTIAL_PLAY_FINAL_CLOSE,
};

struct sequential_play_tape {
	enum sequential_play_event events[20];
	size_t event_count;
	size_t calls;
	size_t fail_at;
	bool open;
	char path[32];
	const uint8_t *lines[5];
	size_t lengths[5];
	size_t line_count;
	size_t read_position;
	uint8_t presented[5][96];
	size_t presented_length[5];
	size_t presented_count;
};

static bool
sequential_play_step(struct sequential_play_tape *tape,
    enum sequential_play_event event, struct yt_error *error)
{
	CHECK(tape->event_count < YT_ARRAY_LEN(tape->events));
	if (tape->event_count < YT_ARRAY_LEN(tape->events))
		tape->events[tape->event_count++] = event;
	++tape->calls;
	if (tape->calls == tape->fail_at) {
		if (error != NULL) {
			error->status = YT_IO_ERROR;
			(void)snprintf(error->operation, sizeof(error->operation), "%s",
			    "sequential fixture");
		}
		return false;
	}
	return true;
}

static bool
sequential_play_close(void *context, struct yt_error *error)
{
	struct sequential_play_tape *tape = context;
	enum sequential_play_event event = tape->open
	    ? SEQUENTIAL_PLAY_FINAL_CLOSE : SEQUENTIAL_PLAY_PRECLOSE;

	if (!sequential_play_step(tape, event, error))
		return false;
	tape->open = false;
	return true;
}

static bool
sequential_play_open(void *context, const char *path,
    struct yt_error *error)
{
	struct sequential_play_tape *tape = context;

	if (!sequential_play_step(tape, SEQUENTIAL_PLAY_OPEN, error))
		return false;
	(void)snprintf(tape->path, sizeof(tape->path), "%s", path);
	tape->open = true;
	return true;
}

static bool
sequential_play_read(void *context, const uint8_t **line, size_t *length,
    bool *available, struct yt_error *error)
{
	struct sequential_play_tape *tape = context;
	size_t index = tape->read_position;

	if (!sequential_play_step(tape, SEQUENTIAL_PLAY_READ, error))
		return false;
	if (index == tape->line_count) {
		*line = NULL;
		*length = 0U;
		*available = false;
		return true;
	}
	*line = tape->lines[index];
	*length = tape->lengths[index];
	*available = true;
	++tape->read_position;
	return true;
}

static bool
sequential_play_present(void *context, const uint8_t *line, size_t length,
    struct yt_error *error)
{
	struct sequential_play_tape *tape = context;
	size_t index = tape->presented_count;

	if (!sequential_play_step(tape, SEQUENTIAL_PLAY_PRESENT, error))
		return false;
	CHECK(index < YT_ARRAY_LEN(tape->presented)
	    && length <= sizeof(tape->presented[0]));
	if (index >= YT_ARRAY_LEN(tape->presented)
	    || length > sizeof(tape->presented[0]))
		return false;
	if (length != 0U)
		memcpy(tape->presented[index], line, length);
	tape->presented_length[index] = length;
	++tape->presented_count;
	return true;
}

static void
sequential_play_fixture(struct sequential_play_tape *tape)
{
	static const uint8_t empty[] = "";
	static const uint8_t first[] =
	    "Congratulations! You have defeated the Xannor Headquarters! This marks you as";
	static const uint8_t second[] =
	    "a SUPERIOR Trader! Be warned however, the Xannor have spies everywhere and";
	static const uint8_t third[] =
	    "will be on the lookout for you! Hide or defend yourself well tonight!";

	memset(tape, 0, sizeof(*tape));
	tape->lines[0] = empty;
	tape->lines[1] = first;
	tape->lines[2] = second;
	tape->lines[3] = third;
	tape->lines[4] = empty;
	tape->lengths[0] = 0U;
	tape->lengths[1] = sizeof(first) - 1U;
	tape->lengths[2] = sizeof(second) - 1U;
	tape->lengths[3] = sizeof(third) - 1U;
	tape->lengths[4] = 0U;
	tape->line_count = YT_ARRAY_LEN(tape->lines);
}

static void
test_sequential_text_playback(void)
{
	static const struct yt_text_sequential_play_ops ops = {
		sequential_play_close,
		sequential_play_open,
		sequential_play_read,
		sequential_play_present,
	};
	static const enum sequential_play_event expected[] = {
		SEQUENTIAL_PLAY_PRECLOSE,
		SEQUENTIAL_PLAY_OPEN,
		SEQUENTIAL_PLAY_READ,
		SEQUENTIAL_PLAY_PRESENT,
		SEQUENTIAL_PLAY_READ,
		SEQUENTIAL_PLAY_PRESENT,
		SEQUENTIAL_PLAY_READ,
		SEQUENTIAL_PLAY_PRESENT,
		SEQUENTIAL_PLAY_READ,
		SEQUENTIAL_PLAY_PRESENT,
		SEQUENTIAL_PLAY_READ,
		SEQUENTIAL_PLAY_PRESENT,
		SEQUENTIAL_PLAY_READ,
		SEQUENTIAL_PLAY_FINAL_CLOSE,
	};
	static const enum sequential_play_event empty_expected[] = {
		SEQUENTIAL_PLAY_PRECLOSE,
		SEQUENTIAL_PLAY_OPEN,
		SEQUENTIAL_PLAY_READ,
		SEQUENTIAL_PLAY_FINAL_CLOSE,
	};
	struct sequential_play_tape success;
	struct sequential_play_tape tape;
	struct yt_text_sequential_play_state state;
	struct yt_error error;
	size_t failure;

	sequential_play_fixture(&success);
	memset(&state, 0, sizeof(state));
	state.path = "XannorHQ.TXT";
	yt_error_clear(&error);
	CHECK(yt_text_sequential_play_run(&state, &ops, &success, &error));
	CHECK(success.calls == YT_ARRAY_LEN(expected)
	    && success.event_count == YT_ARRAY_LEN(expected)
	    && memcmp(success.events, expected, sizeof(expected)) == 0
	    && strcmp(success.path, "XannorHQ.TXT") == 0
	    && !success.open && !state.file_open
	    && state.read_count == 6U && state.line_count == 5U
	    && success.presented_count == 5U);
	for (failure = 0U; failure < success.line_count; ++failure) {
		CHECK(success.presented_length[failure]
		    == success.lengths[failure]);
		CHECK(memcmp(success.presented[failure], success.lines[failure],
		    success.lengths[failure]) == 0);
	}
	for (failure = 1U; failure <= success.calls; ++failure) {
		sequential_play_fixture(&tape);
		tape.fail_at = failure;
		memset(&state, 0, sizeof(state));
		state.path = "XannorHQ.TXT";
		yt_error_clear(&error);
		CHECK(!yt_text_sequential_play_run(&state, &ops, &tape, &error));
		CHECK(error.status == YT_IO_ERROR && tape.calls == failure
		    && tape.event_count == failure
		    && memcmp(tape.events, expected,
		    failure * sizeof(expected[0])) == 0);
		if (failure == 1U || failure == 2U)
			CHECK(!state.file_open);
		else
			CHECK(state.file_open);
	}
	sequential_play_fixture(&tape);
	tape.line_count = 0U;
	memset(&state, 0, sizeof(state));
	state.path = "XannorHQ.TXT";
	yt_error_clear(&error);
	CHECK(yt_text_sequential_play_run(&state, &ops, &tape, &error));
	CHECK(tape.event_count == YT_ARRAY_LEN(empty_expected)
	    && memcmp(tape.events, empty_expected, sizeof(empty_expected)) == 0
	    && tape.presented_count == 0U && state.read_count == 1U
	    && state.line_count == 0U && !state.file_open);
}

static void
test_text_input(void)
{
	char directory[256];
	char actual[320];
	char requested[320];
	char missing[320];
	uint8_t source[640];
	struct yt_text_input input;
	struct yt_error error;
	const uint8_t *line;
	size_t length;
	size_t position = 0U;
	bool available;
	bool eof;
	size_t index;

#ifdef _WIN32
	snprintf(directory, sizeof(directory), "yt-text-input-%lu",
	    (unsigned long)GetCurrentProcessId());
#else
	snprintf(directory, sizeof(directory), "/tmp/yt-text-input-%ld",
	    (long)getpid());
#endif
	(void)mkdir_one(directory);
	snprintf(actual, sizeof(actual), "%s/xannorhq.txt", directory);
	snprintf(requested, sizeof(requested), "%s/XannorHQ.TXT", directory);
	snprintf(missing, sizeof(missing), "%s/MISSING.TXT", directory);
	source[position++] = '\r';
	source[position++] = '\n';
	source[position++] = 'A';
	source[position++] = 0U;
	source[position++] = 'B';
	source[position++] = '\r';
	source[position++] = '\n';
	for (index = 0U; index < 600U; ++index)
		source[position++] = 'q';
	source[position++] = '\r';
	source[position++] = 'X';
	source[position++] = 0x1aU;
	source[position++] = 'Z';
	CHECK(write_bytes(actual, source, position));
	yt_text_input_init(&input);
	yt_error_clear(&error);
	CHECK(yt_text_input_close(&input, &error));
	CHECK(yt_text_input_open(&input, requested, &error));
	CHECK(input.file != NULL && strcmp(input.path, actual) == 0);
	CHECK(yt_text_input_eof(&input, &eof, &error) && !eof);
	CHECK(yt_text_input_read_line(&input, &line, &length, &available,
	    &error) && available && length == 0U);
	CHECK(yt_text_input_eof(&input, &eof, &error) && !eof);
	CHECK(yt_text_input_read_line(&input, &line, &length, &available,
	    &error) && available && length == 2U
	    && memcmp(line, "AB", 2U) == 0);
	CHECK(yt_text_input_eof(&input, &eof, &error) && !eof);
	CHECK(yt_text_input_read_line(&input, &line, &length, &available,
	    &error) && available && length == 600U
	    && input.line_capacity >= 600U);
	for (index = 0U; index < length; ++index)
		CHECK(line[index] == 'q');
	CHECK(yt_text_input_read_line(&input, &line, &length, &available,
	    &error) && available && length == 1U && line[0] == 'X');
	CHECK(yt_text_input_eof(&input, &eof, &error) && eof);
	CHECK(yt_text_input_eof(&input, &eof, &error) && eof);
	CHECK(yt_text_input_read_line(&input, &line, &length, &available,
	    &error) && !available && length == 0U);
	CHECK(yt_text_input_read_line(&input, &line, &length, &available,
	    &error) && !available && length == 0U);
	CHECK(yt_text_input_close(&input, &error) && input.file == NULL);
	CHECK(write_bytes(actual, (const uint8_t *)"tail", 4U));
	CHECK(yt_text_input_open(&input, requested, &error));
	CHECK(yt_text_input_eof(&input, &eof, &error) && !eof);
	CHECK(yt_text_input_read_line(&input, &line, &length, &available,
	    &error) && available && length == 4U
	    && memcmp(line, "tail", 4U) == 0);
	CHECK(yt_text_input_eof(&input, &eof, &error) && eof);
	CHECK(yt_text_input_read_line(&input, &line, &length, &available,
	    &error) && !available && length == 0U);
	CHECK(yt_text_input_close(&input, &error) && input.file == NULL);
	yt_error_clear(&error);
	CHECK(!yt_text_input_open(&input, missing, &error)
	    && error.status == YT_NOT_FOUND && input.file == NULL);
	yt_text_input_destroy(&input);
	CHECK(yt_file_delete(actual, false, &error));
#ifdef _WIN32
	_rmdir(directory);
#else
	rmdir(directory);
#endif
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
	    && key[0] == '\0' && line_count == 17.0f);
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

enum viewer_stream_event {
	VIEWER_STREAM_INITIAL_CLOSE,
	VIEWER_STREAM_OPEN,
	VIEWER_STREAM_EOF,
	VIEWER_STREAM_READ,
	VIEWER_STREAM_ROW,
	VIEWER_STREAM_FINAL_CLOSE,
	VIEWER_STREAM_FINAL_BLANK,
};

struct viewer_stream_tape {
	enum viewer_stream_event events[20];
	size_t event_count;
	size_t fail_at;
	bool open;
	char path[32];
	const uint8_t *lines[2];
	size_t lengths[2];
	size_t line_count;
	size_t position;
	size_t presented;
	uint8_t presented_bytes[2][32];
	size_t presented_lengths[2];
	char *key;
	float *pager_line_count;
	float observed_line_counts[20];
	bool stop_after_first;
	bool unavailable_read;
};

static bool
viewer_stream_record(struct viewer_stream_tape *tape,
    enum viewer_stream_event event, struct yt_error *error)
{
	CHECK(tape->event_count < YT_ARRAY_LEN(tape->events));
	if (tape->event_count < YT_ARRAY_LEN(tape->events))
		tape->events[tape->event_count] = event;
	if (tape->event_count < YT_ARRAY_LEN(tape->observed_line_counts))
		tape->observed_line_counts[tape->event_count] =
		    *tape->pager_line_count;
	++tape->event_count;
	if (tape->event_count != tape->fail_at)
		return true;
	if (error != NULL) {
		error->status = YT_IO_ERROR;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    "injected viewer failure");
	}
	return false;
}

static bool
viewer_stream_close(void *context, struct yt_error *error)
{
	struct viewer_stream_tape *tape = context;
	enum viewer_stream_event event = tape->open
	    ? VIEWER_STREAM_FINAL_CLOSE : VIEWER_STREAM_INITIAL_CLOSE;

	if (!viewer_stream_record(tape, event, error))
		return false;
	tape->open = false;
	return true;
}

static bool
viewer_stream_open(void *context, const char *path, struct yt_error *error)
{
	struct viewer_stream_tape *tape = context;

	if (!viewer_stream_record(tape, VIEWER_STREAM_OPEN, error))
		return false;
	(void)snprintf(tape->path, sizeof(tape->path), "%s", path);
	tape->open = true;
	return true;
}

static bool
viewer_stream_eof(void *context, bool *eof, struct yt_error *error)
{
	struct viewer_stream_tape *tape = context;

	if (!viewer_stream_record(tape, VIEWER_STREAM_EOF, error))
		return false;
	*eof = tape->position == tape->line_count;
	return true;
}

static bool
viewer_stream_read(void *context, const uint8_t **line, size_t *length,
    bool *available, struct yt_error *error)
{
	struct viewer_stream_tape *tape = context;
	size_t index = tape->position;

	if (!viewer_stream_record(tape, VIEWER_STREAM_READ, error))
		return false;
	if (tape->unavailable_read) {
		*line = NULL;
		*length = 0U;
		*available = false;
		return true;
	}
	CHECK(index < tape->line_count);
	if (index >= tape->line_count)
		return false;
	*line = tape->lines[index];
	*length = tape->lengths[index];
	*available = true;
	++tape->position;
	return true;
}

static bool
viewer_stream_present(void *context, const uint8_t *text, size_t length,
    bool paged, struct yt_error *error)
{
	struct viewer_stream_tape *tape = context;
	size_t index = tape->presented;

	if (!viewer_stream_record(tape, paged ? VIEWER_STREAM_ROW
	    : VIEWER_STREAM_FINAL_BLANK, error))
		return false;
	if (!paged) {
		CHECK(text == NULL && length == 0U);
		return true;
	}
	CHECK(index < YT_ARRAY_LEN(tape->presented_bytes)
	    && length <= sizeof(tape->presented_bytes[0]));
	if (index >= YT_ARRAY_LEN(tape->presented_bytes)
	    || length > sizeof(tape->presented_bytes[0]))
		return false;
	if (length != 0U)
		memcpy(tape->presented_bytes[index], text, length);
	tape->presented_lengths[index] = length;
	++tape->presented;
	if (tape->stop_after_first && tape->presented == 1U)
		(void)snprintf(tape->key, 2U, "%s", "Q");
	return true;
}

static const struct yt_file_viewer_stream_ops viewer_stream_ops = {
	viewer_stream_close,
	viewer_stream_open,
	viewer_stream_eof,
	viewer_stream_read,
	viewer_stream_present,
};

static void
viewer_stream_initialize(struct yt_file_viewer_stream_state *state,
    struct viewer_stream_tape *tape, float *foreground,
    int *pager_foreground, float *bold, float *line_count, char key[2])
{
	static const uint8_t ordinary[] = "ordinary";
	static const uint8_t marker[] = "  - item";

	memset(tape, 0, sizeof(*tape));
	tape->lines[0] = ordinary;
	tape->lines[1] = marker;
	tape->lengths[0] = sizeof(ordinary) - 1U;
	tape->lengths[1] = sizeof(marker) - 1U;
	tape->line_count = 2U;
	tape->key = key;
	tape->pager_line_count = line_count;
	*foreground = 5.0f;
	*pager_foreground = 5;
	*bold = 0.0f;
	*line_count = 17.0f;
	key[0] = '\0';
	memset(state, 0, sizeof(*state));
	state->path = "Viewer.TXT";
	state->play.foreground = foreground;
	state->play.pager_foreground = pager_foreground;
	state->play.bold = bold;
	state->play.line_count = line_count;
	state->play.pager_key = key;
	state->play.saved_foreground = 5.0f;
	state->play.saved_pager_foreground = 5;
}

static void
test_file_viewer_stream(void)
{
	static const enum viewer_stream_event expected[] = {
		VIEWER_STREAM_INITIAL_CLOSE,
		VIEWER_STREAM_OPEN,
		VIEWER_STREAM_EOF,
		VIEWER_STREAM_READ,
		VIEWER_STREAM_ROW,
		VIEWER_STREAM_EOF,
		VIEWER_STREAM_READ,
		VIEWER_STREAM_ROW,
		VIEWER_STREAM_EOF,
		VIEWER_STREAM_FINAL_CLOSE,
		VIEWER_STREAM_FINAL_BLANK,
	};
	static const enum viewer_stream_event stopped[] = {
		VIEWER_STREAM_INITIAL_CLOSE,
		VIEWER_STREAM_OPEN,
		VIEWER_STREAM_EOF,
		VIEWER_STREAM_READ,
		VIEWER_STREAM_ROW,
		VIEWER_STREAM_EOF,
		VIEWER_STREAM_FINAL_CLOSE,
		VIEWER_STREAM_FINAL_BLANK,
	};
	static const enum viewer_stream_event empty[] = {
		VIEWER_STREAM_INITIAL_CLOSE,
		VIEWER_STREAM_OPEN,
		VIEWER_STREAM_EOF,
		VIEWER_STREAM_FINAL_CLOSE,
		VIEWER_STREAM_FINAL_BLANK,
	};
	struct yt_file_viewer_stream_state state;
	struct viewer_stream_tape tape;
	struct yt_error error;
	float foreground;
	float bold;
	float line_count;
	int pager_foreground;
	char key[2];
	size_t failure;

	viewer_stream_initialize(&state, &tape, &foreground,
	    &pager_foreground, &bold, &line_count, key);
	yt_error_clear(&error);
	CHECK(yt_file_viewer_stream_run(&state, &viewer_stream_ops, &tape,
	    &error));
	CHECK(tape.event_count == YT_ARRAY_LEN(expected)
	    && memcmp(tape.events, expected, sizeof(expected)) == 0
	    && tape.observed_line_counts[0] == 17.0f
	    && tape.observed_line_counts[1] == 0.0f
	    && strcmp(tape.path, "Viewer.TXT") == 0 && !tape.open
	    && tape.presented == 2U
	    && tape.presented_lengths[0] == 8U
	    && memcmp(tape.presented_bytes[0], "ordinary", 8U) == 0
	    && tape.presented_lengths[1] == 8U
	    && memcmp(tape.presented_bytes[1], "  - item", 8U) == 0);
	CHECK(!state.file_open && state.eof_checks == 3U
	    && state.key_checks == 3U && state.read_count == 2U
	    && state.line_count == 2U && foreground == 5.0f
	    && pager_foreground == 5 && bold == 1.0f && line_count == 0.0f);

	for (failure = 1U; failure <= YT_ARRAY_LEN(expected); ++failure) {
		viewer_stream_initialize(&state, &tape, &foreground,
		    &pager_foreground, &bold, &line_count, key);
		tape.fail_at = failure;
		yt_error_clear(&error);
		CHECK(!yt_file_viewer_stream_run(&state, &viewer_stream_ops,
		    &tape, &error));
		CHECK(error.status == YT_IO_ERROR && tape.event_count == failure
		    && memcmp(tape.events, expected,
		    failure * sizeof(expected[0])) == 0);
		CHECK(state.file_open == (failure >= 3U && failure <= 10U));
		CHECK(state.eof_checks == (failure <= 3U ? 0U
		    : failure <= 6U ? 1U : failure <= 9U ? 2U : 3U));
		CHECK(state.key_checks == state.eof_checks);
		CHECK(state.read_count == (failure <= 4U ? 0U
		    : failure <= 7U ? 1U : 2U));
		CHECK(state.line_count == (failure <= 5U ? 0U
		    : failure <= 8U ? 1U : 2U));
		CHECK(line_count == (failure == 1U ? 17.0f : 0.0f));
		CHECK(foreground == (failure <= 4U ? 5.0f
		    : failure <= 7U ? 2.0f : failure <= 10U ? 3.0f : 5.0f));
	}

	viewer_stream_initialize(&state, &tape, &foreground,
	    &pager_foreground, &bold, &line_count, key);
	tape.stop_after_first = true;
	CHECK(yt_file_viewer_stream_run(&state, &viewer_stream_ops, &tape,
	    NULL));
	CHECK(tape.event_count == YT_ARRAY_LEN(stopped)
	    && memcmp(tape.events, stopped, sizeof(stopped)) == 0
	    && state.eof_checks == 2U && state.key_checks == 2U
	    && state.read_count == 1U && state.line_count == 1U
	    && strcmp(key, "Q") == 0 && !state.file_open);

	viewer_stream_initialize(&state, &tape, &foreground,
	    &pager_foreground, &bold, &line_count, key);
	tape.line_count = 0U;
	CHECK(yt_file_viewer_stream_run(&state, &viewer_stream_ops, &tape,
	    NULL));
	CHECK(tape.event_count == YT_ARRAY_LEN(empty)
	    && memcmp(tape.events, empty, sizeof(empty)) == 0
	    && state.eof_checks == 1U && state.key_checks == 1U
	    && state.read_count == 0U && state.line_count == 0U);

	viewer_stream_initialize(&state, &tape, &foreground,
	    &pager_foreground, &bold, &line_count, key);
	tape.unavailable_read = true;
	yt_error_clear(&error);
	CHECK(!yt_file_viewer_stream_run(&state, &viewer_stream_ops, &tape,
	    &error) && error.status == YT_EOF && state.file_open
	    && state.eof_checks == 1U && state.key_checks == 1U
	    && state.read_count == 1U && state.line_count == 0U);
}

struct viewer_file_context {
	struct yt_text_input input;
	uint8_t rows[2][32];
	size_t lengths[2];
	size_t row_count;
	bool final_blank;
};

static bool
viewer_file_close(void *context, struct yt_error *error)
{
	struct viewer_file_context *viewer = context;

	return yt_text_input_close(&viewer->input, error);
}

static bool
viewer_file_open(void *context, const char *path, struct yt_error *error)
{
	struct viewer_file_context *viewer = context;

	return yt_text_input_open(&viewer->input, path, error);
}

static bool
viewer_file_eof(void *context, bool *eof, struct yt_error *error)
{
	struct viewer_file_context *viewer = context;

	return yt_text_input_eof(&viewer->input, eof, error);
}

static bool
viewer_file_read(void *context, const uint8_t **line, size_t *length,
    bool *available, struct yt_error *error)
{
	struct viewer_file_context *viewer = context;

	return yt_text_input_read_line(&viewer->input, line, length, available,
	    error);
}

static bool
viewer_file_present(void *context, const uint8_t *text, size_t length,
    bool paged, struct yt_error *error)
{
	struct viewer_file_context *viewer = context;
	size_t index = viewer->row_count;

	(void)error;
	if (!paged) {
		CHECK(text == NULL && length == 0U);
		viewer->final_blank = true;
		return true;
	}
	CHECK(index < YT_ARRAY_LEN(viewer->rows)
	    && length <= sizeof(viewer->rows[0]));
	if (index >= YT_ARRAY_LEN(viewer->rows)
	    || length > sizeof(viewer->rows[0]))
		return false;
	if (length != 0U)
		memcpy(viewer->rows[index], text, length);
	viewer->lengths[index] = length;
	++viewer->row_count;
	return true;
}

static void
test_file_viewer_physical_stream(void)
{
	static const struct yt_file_viewer_stream_ops ops = {
		viewer_file_close,
		viewer_file_open,
		viewer_file_eof,
		viewer_file_read,
		viewer_file_present,
	};
	static const uint8_t source[] =
	    "ordinary\r\n  - item\r\n\x1aignored\r\n";
	char directory[256];
	char actual[320];
	char requested[320];
	struct viewer_file_context viewer;
	struct yt_file_viewer_stream_state state;
	struct yt_error error;
	float foreground = 5.0f;
	float bold = 0.0f;
	float line_count = 19.0f;
	int pager_foreground = 5;
	char key[2] = "";

#ifdef _WIN32
	(void)snprintf(directory, sizeof(directory), "yt-viewer-input-%lu",
	    (unsigned long)GetCurrentProcessId());
#else
	(void)snprintf(directory, sizeof(directory), "/tmp/yt-viewer-input-%ld",
	    (long)getpid());
#endif
	(void)mkdir_one(directory);
	(void)snprintf(actual, sizeof(actual), "%s/viewer.txt", directory);
	(void)snprintf(requested, sizeof(requested), "%s/Viewer.TXT",
	    directory);
	CHECK(write_bytes(actual, source, sizeof(source) - 1U));
	memset(&viewer, 0, sizeof(viewer));
	yt_text_input_init(&viewer.input);
	memset(&state, 0, sizeof(state));
	state.path = requested;
	state.play.foreground = &foreground;
	state.play.pager_foreground = &pager_foreground;
	state.play.bold = &bold;
	state.play.line_count = &line_count;
	state.play.pager_key = key;
	state.play.saved_foreground = foreground;
	state.play.saved_pager_foreground = pager_foreground;
	yt_error_clear(&error);
	CHECK(yt_file_viewer_stream_run(&state, &ops, &viewer, &error));
	CHECK(!state.file_open && state.eof_checks == 3U
	    && state.key_checks == 3U && state.read_count == 2U
	    && state.line_count == 2U && viewer.row_count == 2U
	    && viewer.final_blank && viewer.input.file == NULL);
	CHECK(viewer.lengths[0] == 8U
	    && memcmp(viewer.rows[0], "ordinary", 8U) == 0
	    && viewer.lengths[1] == 8U
	    && memcmp(viewer.rows[1], "  - item", 8U) == 0
	    && foreground == 5.0f && pager_foreground == 5
	    && bold == 1.0f && line_count == 0.0f);
	yt_text_input_destroy(&viewer.input);
	CHECK(yt_file_delete(actual, false, &error));
#ifdef _WIN32
	_rmdir(directory);
#else
	rmdir(directory);
#endif
}

enum opening_stream_event {
	OPENING_STREAM_OPEN_INPUT,
	OPENING_STREAM_OPEN_LOCAL,
	OPENING_STREAM_EOF,
	OPENING_STREAM_READ,
	OPENING_STREAM_LOCAL_ROW,
	OPENING_STREAM_LOCAL_POLL,
	OPENING_STREAM_REMOTE_ROW,
	OPENING_STREAM_REMOTE_POLL,
	OPENING_STREAM_WAIT,
	OPENING_STREAM_REMOTE_RESET,
	OPENING_STREAM_LOCAL_RESET,
	OPENING_STREAM_CLOSE_INPUT,
	OPENING_STREAM_CLOSE_LOCAL,
};

struct opening_stream_tape {
	enum opening_stream_event events[32];
	size_t event_count;
	size_t fail_at;
	bool input_open;
	bool local_open;
	char path[32];
	const uint8_t *lines[2];
	size_t lengths[2];
	size_t line_count;
	size_t position;
	size_t eof_successes;
	size_t read_successes;
	size_t local_successes;
	size_t local_poll_successes;
	size_t remote_successes;
	size_t remote_poll_successes;
	size_t local_key_at;
	size_t remote_pending_at;
	bool wait_success;
	float wait_seconds;
	bool remote_reset;
	bool local_reset;
	bool unavailable_read;
	uint8_t local_rows[2][16];
	size_t local_lengths[2];
	uint8_t remote_rows[2][16];
	size_t remote_lengths[2];
};

static bool
opening_stream_record(struct opening_stream_tape *tape,
    enum opening_stream_event event, struct yt_error *error)
{
	CHECK(tape->event_count < YT_ARRAY_LEN(tape->events));
	if (tape->event_count < YT_ARRAY_LEN(tape->events))
		tape->events[tape->event_count] = event;
	++tape->event_count;
	if (tape->event_count != tape->fail_at)
		return true;
	if (error != NULL) {
		error->status = YT_IO_ERROR;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    "injected opening failure");
	}
	return false;
}

static bool
opening_stream_open_input(void *context, const char *path,
    struct yt_error *error)
{
	struct opening_stream_tape *tape = context;

	if (!opening_stream_record(tape, OPENING_STREAM_OPEN_INPUT, error))
		return false;
	(void)snprintf(tape->path, sizeof(tape->path), "%s", path);
	tape->input_open = true;
	return true;
}

static bool
opening_stream_open_local(void *context, struct yt_error *error)
{
	struct opening_stream_tape *tape = context;

	if (!opening_stream_record(tape, OPENING_STREAM_OPEN_LOCAL, error))
		return false;
	tape->local_open = true;
	return true;
}

static bool
opening_stream_eof(void *context, bool *eof, struct yt_error *error)
{
	struct opening_stream_tape *tape = context;

	if (!opening_stream_record(tape, OPENING_STREAM_EOF, error))
		return false;
	++tape->eof_successes;
	*eof = tape->position == tape->line_count;
	return true;
}

static bool
opening_stream_read(void *context, const uint8_t **line, size_t *length,
    bool *available, struct yt_error *error)
{
	struct opening_stream_tape *tape = context;
	size_t index = tape->position;

	if (!opening_stream_record(tape, OPENING_STREAM_READ, error))
		return false;
	++tape->read_successes;
	if (tape->unavailable_read) {
		*line = NULL;
		*length = 0U;
		*available = false;
		return true;
	}
	CHECK(index < tape->line_count);
	if (index >= tape->line_count)
		return false;
	*line = tape->lines[index];
	*length = tape->lengths[index];
	*available = true;
	++tape->position;
	return true;
}

static bool
opening_stream_present_local(void *context, const uint8_t *line,
    size_t length, struct yt_error *error)
{
	struct opening_stream_tape *tape = context;
	size_t index = tape->local_successes;

	if (!opening_stream_record(tape, OPENING_STREAM_LOCAL_ROW, error))
		return false;
	CHECK(index < YT_ARRAY_LEN(tape->local_rows)
	    && length <= sizeof(tape->local_rows[0]));
	if (index >= YT_ARRAY_LEN(tape->local_rows)
	    || length > sizeof(tape->local_rows[0]))
		return false;
	if (length != 0U)
		memcpy(tape->local_rows[index], line, length);
	tape->local_lengths[index] = length;
	++tape->local_successes;
	return true;
}

static bool
opening_stream_poll_local(void *context, bool *ready,
    struct yt_error *error)
{
	struct opening_stream_tape *tape = context;

	if (!opening_stream_record(tape, OPENING_STREAM_LOCAL_POLL, error))
		return false;
	++tape->local_poll_successes;
	*ready = tape->local_key_at != 0U
	    && tape->local_poll_successes == tape->local_key_at;
	return true;
}

static bool
opening_stream_present_remote(void *context, const uint8_t *line,
    size_t length, struct yt_error *error)
{
	struct opening_stream_tape *tape = context;
	size_t index = tape->remote_successes;

	if (!opening_stream_record(tape, OPENING_STREAM_REMOTE_ROW, error))
		return false;
	CHECK(index < YT_ARRAY_LEN(tape->remote_rows)
	    && length <= sizeof(tape->remote_rows[0]));
	if (index >= YT_ARRAY_LEN(tape->remote_rows)
	    || length > sizeof(tape->remote_rows[0]))
		return false;
	if (length != 0U)
		memcpy(tape->remote_rows[index], line, length);
	tape->remote_lengths[index] = length;
	++tape->remote_successes;
	return true;
}

static bool
opening_stream_poll_remote(void *context, bool *ready,
    struct yt_error *error)
{
	struct opening_stream_tape *tape = context;

	if (!opening_stream_record(tape, OPENING_STREAM_REMOTE_POLL, error))
		return false;
	++tape->remote_poll_successes;
	*ready = tape->remote_pending_at != 0U
	    && tape->remote_poll_successes == tape->remote_pending_at;
	return true;
}

static bool
opening_stream_wait(void *context, float seconds, struct yt_error *error)
{
	struct opening_stream_tape *tape = context;

	if (!opening_stream_record(tape, OPENING_STREAM_WAIT, error))
		return false;
	tape->wait_success = true;
	tape->wait_seconds = seconds;
	return true;
}

static bool
opening_stream_reset_remote(void *context, struct yt_error *error)
{
	struct opening_stream_tape *tape = context;

	if (!opening_stream_record(tape, OPENING_STREAM_REMOTE_RESET, error))
		return false;
	tape->remote_reset = true;
	return true;
}

static bool
opening_stream_reset_local(void *context, struct yt_error *error)
{
	struct opening_stream_tape *tape = context;

	if (!opening_stream_record(tape, OPENING_STREAM_LOCAL_RESET, error))
		return false;
	tape->local_reset = true;
	return true;
}

static bool
opening_stream_close_input(void *context, struct yt_error *error)
{
	struct opening_stream_tape *tape = context;

	if (!opening_stream_record(tape, OPENING_STREAM_CLOSE_INPUT, error))
		return false;
	tape->input_open = false;
	return true;
}

static bool
opening_stream_close_local(void *context, struct yt_error *error)
{
	struct opening_stream_tape *tape = context;

	if (!opening_stream_record(tape, OPENING_STREAM_CLOSE_LOCAL, error))
		return false;
	tape->local_open = false;
	return true;
}

static const struct yt_opening_stream_ops opening_stream_ops = {
	opening_stream_open_input,
	opening_stream_open_local,
	opening_stream_eof,
	opening_stream_read,
	opening_stream_present_local,
	opening_stream_poll_local,
	opening_stream_present_remote,
	opening_stream_poll_remote,
	opening_stream_wait,
	opening_stream_reset_remote,
	opening_stream_reset_local,
	opening_stream_close_input,
	opening_stream_close_local,
};

static void
opening_stream_initialize(struct yt_opening_stream_state *state,
    struct opening_stream_tape *tape)
{
	static const uint8_t first[] = "first";
	static const uint8_t second[] = "second";

	memset(tape, 0, sizeof(*tape));
	tape->lines[0] = first;
	tape->lines[1] = second;
	tape->lengths[0] = sizeof(first) - 1U;
	tape->lengths[1] = sizeof(second) - 1U;
	tape->line_count = 2U;
	memset(state, 0, sizeof(*state));
	state->path = "Opening.ANS";
	state->mode = 0.0f;
	state->snoop = -1.0f;
}

static void
opening_stream_check_state(const struct yt_opening_stream_state *state,
    const struct opening_stream_tape *tape)
{
	CHECK(state->input_open == tape->input_open
	    && state->local_open == tape->local_open
	    && state->eof_checks == tape->eof_successes
	    && state->read_count == tape->read_successes
	    && state->local_lines == tape->local_successes
	    && state->local_polls == tape->local_poll_successes
	    && state->remote_lines == tape->remote_successes
	    && state->remote_polls == tape->remote_poll_successes
	    && state->waited == tape->wait_success
	    && state->remote_reset == tape->remote_reset
	    && state->local_reset == tape->local_reset);
}

static void
test_opening_stream(void)
{
	static const enum opening_stream_event natural[] = {
		OPENING_STREAM_OPEN_INPUT, OPENING_STREAM_OPEN_LOCAL,
		OPENING_STREAM_EOF, OPENING_STREAM_READ,
		OPENING_STREAM_LOCAL_ROW, OPENING_STREAM_LOCAL_POLL,
		OPENING_STREAM_REMOTE_ROW, OPENING_STREAM_REMOTE_POLL,
		OPENING_STREAM_EOF, OPENING_STREAM_READ,
		OPENING_STREAM_LOCAL_ROW, OPENING_STREAM_LOCAL_POLL,
		OPENING_STREAM_REMOTE_ROW, OPENING_STREAM_REMOTE_POLL,
		OPENING_STREAM_EOF, OPENING_STREAM_WAIT,
		OPENING_STREAM_REMOTE_RESET, OPENING_STREAM_LOCAL_RESET,
		OPENING_STREAM_CLOSE_INPUT, OPENING_STREAM_CLOSE_LOCAL,
	};
	static const enum opening_stream_event local_stop[] = {
		OPENING_STREAM_OPEN_INPUT, OPENING_STREAM_OPEN_LOCAL,
		OPENING_STREAM_EOF, OPENING_STREAM_READ,
		OPENING_STREAM_LOCAL_ROW, OPENING_STREAM_LOCAL_POLL,
		OPENING_STREAM_REMOTE_ROW, OPENING_STREAM_REMOTE_POLL,
		OPENING_STREAM_EOF, OPENING_STREAM_READ,
		OPENING_STREAM_LOCAL_ROW, OPENING_STREAM_LOCAL_POLL,
		OPENING_STREAM_REMOTE_RESET, OPENING_STREAM_LOCAL_RESET,
		OPENING_STREAM_CLOSE_INPUT, OPENING_STREAM_CLOSE_LOCAL,
	};
	static const enum opening_stream_event remote_stop[] = {
		OPENING_STREAM_OPEN_INPUT, OPENING_STREAM_OPEN_LOCAL,
		OPENING_STREAM_EOF, OPENING_STREAM_READ,
		OPENING_STREAM_LOCAL_ROW, OPENING_STREAM_LOCAL_POLL,
		OPENING_STREAM_REMOTE_ROW, OPENING_STREAM_REMOTE_POLL,
		OPENING_STREAM_EOF, OPENING_STREAM_READ,
		OPENING_STREAM_LOCAL_ROW, OPENING_STREAM_LOCAL_POLL,
		OPENING_STREAM_REMOTE_ROW, OPENING_STREAM_REMOTE_POLL,
		OPENING_STREAM_REMOTE_RESET, OPENING_STREAM_LOCAL_RESET,
		OPENING_STREAM_CLOSE_INPUT, OPENING_STREAM_CLOSE_LOCAL,
	};
	struct yt_opening_stream_state state;
	struct opening_stream_tape tape;
	struct yt_error error;
	size_t failure;

	opening_stream_initialize(&state, &tape);
	yt_error_clear(&error);
	CHECK(yt_opening_stream_run(&state, &opening_stream_ops, &tape,
	    &error));
	CHECK(tape.event_count == YT_ARRAY_LEN(natural)
	    && memcmp(tape.events, natural, sizeof(natural)) == 0
	    && strcmp(tape.path, "Opening.ANS") == 0
	    && state.exit_reason == YT_OPENING_EXIT_EOF
	    && tape.wait_seconds == 3.0f
	    && tape.local_lengths[0] == 5U
	    && memcmp(tape.local_rows[0], "first", 5U) == 0
	    && tape.remote_lengths[1] == 6U
	    && memcmp(tape.remote_rows[1], "second", 6U) == 0);
	opening_stream_check_state(&state, &tape);

	for (failure = 1U; failure <= YT_ARRAY_LEN(natural); ++failure) {
		opening_stream_initialize(&state, &tape);
		tape.fail_at = failure;
		yt_error_clear(&error);
		CHECK(!yt_opening_stream_run(&state, &opening_stream_ops, &tape,
		    &error));
		CHECK(error.status == YT_IO_ERROR
		    && tape.event_count == failure
		    && memcmp(tape.events, natural,
		    failure * sizeof(natural[0])) == 0);
		opening_stream_check_state(&state, &tape);
	}

	opening_stream_initialize(&state, &tape);
	tape.local_key_at = 2U;
	CHECK(yt_opening_stream_run(&state, &opening_stream_ops, &tape,
	    NULL));
	CHECK(tape.event_count == YT_ARRAY_LEN(local_stop)
	    && memcmp(tape.events, local_stop, sizeof(local_stop)) == 0
	    && state.exit_reason == YT_OPENING_EXIT_LOCAL_KEY
	    && !state.waited && state.remote_lines == 1U
	    && state.remote_polls == 1U);

	opening_stream_initialize(&state, &tape);
	tape.remote_pending_at = 2U;
	CHECK(yt_opening_stream_run(&state, &opening_stream_ops, &tape,
	    NULL));
	CHECK(tape.event_count == YT_ARRAY_LEN(remote_stop)
	    && memcmp(tape.events, remote_stop, sizeof(remote_stop)) == 0
	    && state.exit_reason == YT_OPENING_EXIT_REMOTE_PENDING
	    && !state.waited && state.remote_lines == 2U
	    && state.remote_polls == 2U);

	opening_stream_initialize(&state, &tape);
	state.mode = 1.0f;
	CHECK(yt_opening_stream_run(&state, &opening_stream_ops, &tape,
	    NULL));
	CHECK(state.exit_reason == YT_OPENING_EXIT_EOF && state.waited
	    && state.local_lines == 2U && state.local_polls == 2U
	    && state.remote_lines == 0U && state.remote_polls == 0U
	    && !state.remote_reset && state.local_reset);

	opening_stream_initialize(&state, &tape);
	state.mode = 2.0f;
	state.snoop = 0.0f;
	CHECK(yt_opening_stream_run(&state, &opening_stream_ops, &tape,
	    NULL));
	CHECK(state.exit_reason == YT_OPENING_EXIT_EOF && state.waited
	    && state.local_lines == 0U && state.local_polls == 2U
	    && state.remote_lines == 2U && state.remote_polls == 2U
	    && !state.remote_reset && !state.local_reset);

	opening_stream_initialize(&state, &tape);
	tape.unavailable_read = true;
	yt_error_clear(&error);
	CHECK(!yt_opening_stream_run(&state, &opening_stream_ops, &tape,
	    &error) && error.status == YT_EOF && state.input_open
	    && state.local_open && state.eof_checks == 1U
	    && state.read_count == 1U && state.local_lines == 0U);
}

struct opening_physical_context {
	struct yt_text_input input;
	bool local_open;
	bool waited;
	float wait_seconds;
	size_t local_polls;
	size_t remote_polls;
	uint8_t local[3000];
	size_t local_length;
	uint8_t remote[3000];
	size_t remote_length;
};

static bool
opening_physical_open_input(void *context, const char *path,
    struct yt_error *error)
{
	struct opening_physical_context *opening = context;

	return yt_text_input_open(&opening->input, path, error);
}

static bool
opening_physical_open_local(void *context, struct yt_error *error)
{
	struct opening_physical_context *opening = context;

	(void)error;
	opening->local_open = true;
	return true;
}

static bool
opening_physical_eof(void *context, bool *eof, struct yt_error *error)
{
	struct opening_physical_context *opening = context;

	return yt_text_input_eof(&opening->input, eof, error);
}

static bool
opening_physical_read(void *context, const uint8_t **line, size_t *length,
    bool *available, struct yt_error *error)
{
	struct opening_physical_context *opening = context;

	return yt_text_input_read_line(&opening->input, line, length, available,
	    error);
}

static bool
opening_physical_append(uint8_t *destination, size_t capacity,
    size_t *used, const uint8_t *data, size_t length)
{
	if (*used > capacity || length > capacity - *used)
		return false;
	if (length != 0U)
		memcpy(destination + *used, data, length);
	*used += length;
	return true;
}

static bool
opening_physical_present_local(void *context, const uint8_t *line,
    size_t length, struct yt_error *error)
{
	static const uint8_t newline[] = {'\r', '\n'};
	struct opening_physical_context *opening = context;

	(void)error;
	return opening_physical_append(opening->local,
	    sizeof(opening->local), &opening->local_length, line, length)
	    && opening_physical_append(opening->local,
	    sizeof(opening->local), &opening->local_length, newline,
	    sizeof(newline));
}

static bool
opening_physical_poll_local(void *context, bool *ready,
    struct yt_error *error)
{
	struct opening_physical_context *opening = context;

	(void)error;
	++opening->local_polls;
	*ready = false;
	return true;
}

static bool
opening_physical_present_remote(void *context, const uint8_t *line,
    size_t length, struct yt_error *error)
{
	static const uint8_t newline[] = {'\n', '\r'};
	struct opening_physical_context *opening = context;

	(void)error;
	return opening_physical_append(opening->remote,
	    sizeof(opening->remote), &opening->remote_length, line, length)
	    && opening_physical_append(opening->remote,
	    sizeof(opening->remote), &opening->remote_length, newline,
	    sizeof(newline));
}

static bool
opening_physical_poll_remote(void *context, bool *ready,
    struct yt_error *error)
{
	struct opening_physical_context *opening = context;

	(void)error;
	++opening->remote_polls;
	*ready = false;
	return true;
}

static bool
opening_physical_wait(void *context, float seconds, struct yt_error *error)
{
	struct opening_physical_context *opening = context;

	(void)error;
	opening->waited = true;
	opening->wait_seconds = seconds;
	return true;
}

static bool
opening_physical_reset_remote(void *context, struct yt_error *error)
{
	static const uint8_t reset[] = "\x1b[0m";
	struct opening_physical_context *opening = context;

	(void)error;
	return opening_physical_append(opening->remote,
	    sizeof(opening->remote), &opening->remote_length, reset,
	    sizeof(reset) - 1U);
}

static bool
opening_physical_reset_local(void *context, struct yt_error *error)
{
	static const uint8_t reset[] = "\x1b[0m";
	struct opening_physical_context *opening = context;

	(void)error;
	return opening_physical_append(opening->local,
	    sizeof(opening->local), &opening->local_length, reset,
	    sizeof(reset) - 1U);
}

static bool
opening_physical_close_input(void *context, struct yt_error *error)
{
	struct opening_physical_context *opening = context;

	return yt_text_input_close(&opening->input, error);
}

static bool
opening_physical_close_local(void *context, struct yt_error *error)
{
	struct opening_physical_context *opening = context;

	(void)error;
	opening->local_open = false;
	return true;
}

static void
test_opening_physical_stream(void)
{
	static const struct yt_opening_stream_ops ops = {
		opening_physical_open_input,
		opening_physical_open_local,
		opening_physical_eof,
		opening_physical_read,
		opening_physical_present_local,
		opening_physical_poll_local,
		opening_physical_present_remote,
		opening_physical_poll_remote,
		opening_physical_wait,
		opening_physical_reset_remote,
		opening_physical_reset_local,
		opening_physical_close_input,
		opening_physical_close_local,
	};
	static const uint8_t local_suffix[] = "\x1b[0m\r\n\x1b[0m";
	static const uint8_t remote_suffix[] = "\x1b[0m\n\r\x1b[0m";
	struct opening_physical_context opening;
	struct yt_opening_stream_state state = {
		.path = YT_DATA_DIR "YTOPEN.ANS",
		.mode = 0.0f,
		.snoop = -1.0f,
	};
	struct yt_error error;

	memset(&opening, 0, sizeof(opening));
	yt_text_input_init(&opening.input);
	yt_error_clear(&error);
	CHECK(yt_opening_stream_run(&state, &ops, &opening, &error));
	CHECK(state.exit_reason == YT_OPENING_EXIT_EOF
	    && !state.input_open && !state.local_open && state.waited
	    && state.eof_checks == 68U && state.read_count == 67U
	    && state.local_lines == 67U && state.local_polls == 67U
	    && state.remote_lines == 67U && state.remote_polls == 67U
	    && state.remote_reset && state.local_reset
	    && opening.waited && opening.wait_seconds == 3.0f
	    && opening.local_polls == 67U && opening.remote_polls == 67U
	    && opening.local_length == 2633U
	    && opening.remote_length == 2633U
	    && opening.input.file == NULL && !opening.local_open);
	CHECK(memcmp(opening.local, "\x1b[40m", 5U) == 0
	    && memcmp(opening.remote, "\x1b[40m", 5U) == 0
	    && memcmp(opening.local + opening.local_length
	    - (sizeof(local_suffix) - 1U), local_suffix,
	    sizeof(local_suffix) - 1U) == 0
	    && memcmp(opening.remote + opening.remote_length
	    - (sizeof(remote_suffix) - 1U), remote_suffix,
	    sizeof(remote_suffix) - 1U) == 0);
	yt_text_input_destroy(&opening.input);
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
	test_radio_file();
	test_append_window();
	test_main_error_fatal_transaction();
	test_line_input_grammar();
	test_sequential_text_playback();
	test_text_input();
	test_file_viewer_records();
	test_file_viewer_entry();
	test_file_viewer_play();
	test_file_viewer_stream();
	test_file_viewer_physical_stream();
	test_opening_stream();
	test_opening_physical_stream();
	test_file_viewer_missing();
	if (failures != 0) {
		fprintf(stderr, "%u test(s) failed\n", failures);
		return EXIT_FAILURE;
	}
	puts("test_data: ok");
	return EXIT_SUCCESS;
}
