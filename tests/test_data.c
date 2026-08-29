#include "yt_data.h"
#include "yt_file.h"
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
	CHECK(yt_radio_reader_mutate(&mutated, 1.0f));
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
	static const uint8_t market_bytes[] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
		0x00, 0x00, 0x40, 0x00, 0x00, 0xc0,
		0x00, 0x00, 0x80, 0x00, 0x00, 0x00
	};
	struct scripted_random script = {bytes, sizeof(bytes), 0};
	struct scripted_random market_script = {
		market_bytes, sizeof(market_bytes), 0
	};
	struct yt_random random;
	float value;
	float bases[3];

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
}

int
main(void)
{
	test_record();
	test_clock();
	test_random();
	test_files();
	test_append_window();
	test_line_input_grammar();
	if (failures != 0) {
		fprintf(stderr, "%u test(s) failed\n", failures);
		return EXIT_FAILURE;
	}
	puts("test_data: ok");
	return EXIT_SUCCESS;
}
