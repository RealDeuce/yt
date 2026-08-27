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
	char name[64];
	uint8_t tail[4] = {1, 2, 3, 4};
	static const uint8_t dirty_zero[4] = {0x12, 0x34, 0x80, 0x00};

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

int
main(void)
{
	test_record();
	test_clock();
	test_random();
	test_files();
	test_append_window();
	if (failures != 0) {
		fprintf(stderr, "%u test(s) failed\n", failures);
		return EXIT_FAILURE;
	}
	puts("test_data: ok");
	return EXIT_SUCCESS;
}
