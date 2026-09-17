#include "yt_data.h"
#include "yt_file.h"
#include "yt_main_error.h"
#include "yt_platform.h"
#include "yt_random.h"
#include "yt_text.h"
#include "file_viewer_test_model.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
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
yt_file_delete(const char *path, bool missing_ok, struct yt_error *error)
{
	(void)error;
	if (remove(path) == 0)
		return true;
	return missing_ok && errno == ENOENT;
}

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
	size_t header_length;
	char name[64];
	uint8_t tail[4] = {1, 2, 3, 4};
	static const uint8_t dirty_zero[4] = {0x12, 0x34, 0x80, 0x00};
	static const uint8_t radio_dirty_zero[4] = {0x00, 0x00, 0x80, 0x00};

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
	const struct yt_clock clock = {scripted_clock_read, &script};
	struct yt_clock_value actual;
	struct yt_error error;
	yt_error_clear(&error);
	CHECK(yt_clock_read(&clock, &actual, &error));
	CHECK(memcmp(&actual, &samples[0], sizeof(actual)) == 0);
	CHECK(yt_clock_read(&clock, &actual, &error));
	CHECK(memcmp(&actual, &samples[1], sizeof(actual)) == 0);
	CHECK(!yt_clock_read(&clock, &actual, &error));
	CHECK(error.status == YT_IO_ERROR);
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
	CHECK(random.has_last && random.last == value);
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

static bool
database_close_fixture(struct yt_database *database, bool device)
{
	memset(database, 0, sizeof(*database));
	database->file = tmpfile();
	if (database->file == NULL)
		return false;
	database->records = 1U;
	database->last_open.device = device;
	(void)snprintf(database->path, sizeof(database->path), "%s",
	    "CLOSE-FIXTURE.DAT");
	return true;
}

static void
test_database_random_open(void)
{
	struct yt_database database;
	struct yt_error error;
	char path[192];
	char missing_parent_path[224];
	FILE *file;
	size_t index;

#ifdef _WIN32
	snprintf(path, sizeof(path), "YT-DATABASE-OPEN-%lu.DAT",
	    (unsigned long)GetCurrentProcessId());
	snprintf(missing_parent_path, sizeof(missing_parent_path),
	    "YT-DATABASE-MISSING-%lu/YTDATA.DAT",
	    (unsigned long)GetCurrentProcessId());
#else
	snprintf(path, sizeof(path), "/tmp/YT-DATABASE-OPEN-%ld.DAT",
	    (long)getpid());
	snprintf(missing_parent_path, sizeof(missing_parent_path),
	    "/tmp/YT-DATABASE-MISSING-%ld/YTDATA.DAT", (long)getpid());
#endif
	yt_error_clear(&error);
	CHECK(yt_file_delete(path, true, &error));
	file = fopen(path, "wb");
	CHECK(file != NULL);
	if (file != NULL) {
		for (index = 0U; index < 2U * YT_RECORD_SIZE; ++index)
			CHECK(fputc((int)(index & 0xffU), file) != EOF);
		CHECK(fclose(file) == 0);
	}
	CHECK(yt_database_open(&database, path, YT_OPEN_UPDATE_CREATE,
	    &error));
	CHECK(database.records == 2U && database.file != NULL
	    && ftell(database.file) == 0L);
	yt_database_close(&database);
	CHECK(yt_file_delete(path, false, &error));

	CHECK(yt_database_open(&database, path, YT_OPEN_UPDATE_CREATE,
	    &error));
	CHECK(database.records == 0U && database.file != NULL
	    && ftell(database.file) == 0L);
	yt_database_close(&database);
	CHECK(yt_file_delete(path, false, &error));

	yt_error_clear(&error);
	CHECK(!yt_database_open(&database, missing_parent_path,
	    YT_OPEN_UPDATE_CREATE, &error) && error.status == YT_IO_ERROR
	    && database.file == NULL);
}

static void
test_file_kill(void)
{
	struct yt_error error;
	char directory[256];
	char path[320];
	FILE *file;

#ifdef _WIN32
	snprintf(directory, sizeof(directory), "YT-FILE-KILL-%lu",
	    (unsigned long)GetCurrentProcessId());
#else
	snprintf(directory, sizeof(directory), "/tmp/YT-FILE-KILL-%ld",
	    (long)getpid());
#endif
	(void)mkdir_one(directory);
	snprintf(path, sizeof(path), "%s/YT.REG", directory);
	file = fopen(path, "wb");
	CHECK(file != NULL);
	if (file != NULL) {
		CHECK(fwrite("registration", 1U, 12U, file) == 12U);
		CHECK(fclose(file) == 0);
	}
	yt_error_clear(&error);
	CHECK(yt_file_kill(path, &error));
	file = fopen(path, "rb");
	CHECK(file == NULL);
	if (file != NULL)
		(void)fclose(file);
	yt_error_clear(&error);
	CHECK(!yt_file_kill(path, &error) && error.status == YT_NOT_FOUND);
#ifdef _WIN32
	_rmdir(directory);
#else
	rmdir(directory);
#endif
}

static void
test_file_rename(void)
{
	struct yt_error error;
	char directory[256];
	char old_path[320];
	char new_path[320];
	char missing_old[320];
	char missing_new[320];
	char bytes[6];
	FILE *file;

#ifdef _WIN32
	snprintf(directory, sizeof(directory), "YT-FILE-RENAME-%lu",
	    (unsigned long)GetCurrentProcessId());
#else
	snprintf(directory, sizeof(directory), "/tmp/YT-FILE-RENAME-%ld",
	    (long)getpid());
#endif
	(void)mkdir_one(directory);
	snprintf(old_path, sizeof(old_path), "%s/TEMP", directory);
	snprintf(new_path, sizeof(new_path), "%s/YTRMSG.DAT", directory);
	snprintf(missing_old, sizeof(missing_old), "%s/MISSING", directory);
	snprintf(missing_new, sizeof(missing_new), "%s/ABSENT/NAME", directory);
	file = fopen(old_path, "wb");
	CHECK(file != NULL);
	if (file != NULL) {
		CHECK(fwrite("source", 1U, 6U, file) == 6U);
		CHECK(fclose(file) == 0);
	}
	yt_error_clear(&error);
	CHECK(yt_file_rename(old_path, new_path, &error));
	file = fopen(new_path, "rb");
	CHECK(file != NULL);
	if (file != NULL) {
		CHECK(fread(bytes, 1U, sizeof(bytes), file) == sizeof(bytes)
		    && memcmp(bytes, "source", sizeof(bytes)) == 0);
		CHECK(fclose(file) == 0);
	}
	file = fopen(old_path, "wb");
	CHECK(file != NULL);
	if (file != NULL)
		CHECK(fclose(file) == 0);
	yt_error_clear(&error);
	CHECK(!yt_file_rename(old_path, new_path, &error)
	    && error.status == YT_IO_ERROR);
	CHECK(remove(new_path) == 0);
	yt_error_clear(&error);
	CHECK(!yt_file_rename(missing_old, new_path, &error)
	    && error.status == YT_NOT_FOUND);
	yt_error_clear(&error);
	CHECK(!yt_file_rename(old_path, missing_new, &error)
	    && error.status == YT_IO_ERROR);
	CHECK(remove(old_path) == 0);
#ifdef _WIN32
	_rmdir(directory);
#else
	rmdir(directory);
#endif
}

static void
test_database_random_close(void)
{
	struct yt_database database;
	struct yt_error error;
	unsigned device;

	memset(&database, 0, sizeof(database));
	CHECK(yt_database_random_close(&database, &error)
	    && database.last_close.outcome == YT_DATABASE_CLOSE_RETURNED
	    && database.last_close.missing
	    && database.last_close.attempt_count == 0U);
	CHECK(yt_database_close_all_single(&database, &error)
	    && database.last_close.outcome == YT_DATABASE_CLOSE_RETURNED
	    && database.last_close.close_all && !database.last_close.missing
	    && database.last_close.attempt_count == 0U);

	for (device = 0U; device < 2U; ++device) {
		CHECK(database_close_fixture(&database, device != 0U));
		yt_error_clear(&error);
		CHECK(yt_database_random_close(&database, &error)
		    && database.file == NULL && database.records == 0U
		    && database.last_close.outcome
		    == YT_DATABASE_CLOSE_RETURNED
		    && database.last_close.attempt_count == 1U
		    && database.last_close.device == (device != 0U)
		    && !database.last_close.missing);
	}

	CHECK(database_close_fixture(&database, false));
	yt_error_clear(&error);
	CHECK(yt_database_close_all_single(&database, &error)
	    && database.file == NULL
	    && database.last_close.close_all
	    && database.last_close.outcome == YT_DATABASE_CLOSE_RETURNED
	    && database.last_close.attempt_count == 1U);
}

static void
test_text_output_write(void)
{
	char directory[256];
	char path[320];
	char missing_parent_path[384];
	uint8_t bytes[300];
	uint8_t next = 0xeeU;
	struct yt_text_output output;
	struct yt_text_file text;
	struct yt_error error;
	size_t index;

	for (index = 0U; index < sizeof(bytes); ++index)
		bytes[index] = (uint8_t)(index & 0xffU);
#ifdef _WIN32
	snprintf(directory, sizeof(directory), "yt-text-write-%lu",
	    (unsigned long)GetCurrentProcessId());
#else
	snprintf(directory, sizeof(directory), "/tmp/yt-text-write-%ld",
	    (long)getpid());
#endif
	(void)mkdir_one(directory);
	snprintf(path, sizeof(path), "%s/output.dat", directory);
	snprintf(missing_parent_path, sizeof(missing_parent_path),
	    "%s/missing/output.dat", directory);

	yt_text_output_init(&output);
	yt_error_clear(&error);
	CHECK(!yt_text_output_open(&output, missing_parent_path, &error)
	    && output.last_output_open.outcome
	    == YT_TEXT_OPEN_INITIAL_ERROR
	    && output.last_output_open.basic_error == 76U
	    && output.last_output_open.dos_error == 3U
	    && output.last_output_open.access_attempt_count == 1U);
	yt_text_output_destroy(&output);

	/* The filesystem path composes arbitrary writes with close. */
	CHECK(write_bytes(path, (const uint8_t *)"stale-tail", 10U));
	yt_text_output_init(&output);
	CHECK(yt_text_output_open(&output, path, &error)
	    && output.last_output_open.outcome == YT_TEXT_OPEN_RETURNED
	    && !output.last_output_open.created
	    && output.last_output_open.access_attempt_count == 1U
	    && output.last_output_open.access_attempts[0] == 1U
	    && output.last_output_open.registered
	    && ftell(output.file) == 0L);
	CHECK(yt_text_read(path, &text, &error));
	if (text.data != NULL) {
		CHECK(text.length == 10U
		    && memcmp(text.data, "stale-tail", 10U) == 0);
		yt_text_free(&text);
	}
	CHECK(yt_text_output_write(&output, bytes, sizeof(bytes), &error)
	    && output.last_write.flush_count == 2U
	    && output.pending_count == 44U
	    && yt_text_output_close(&output, &error));
	CHECK(yt_text_read(path, &text, &error));
	if (text.data != NULL) {
		CHECK(text.length == sizeof(bytes) + 1U
		    && memcmp(text.data, bytes, sizeof(bytes)) == 0
		    && text.data[sizeof(bytes)] == 0x1aU);
		yt_text_free(&text);
	}
	yt_text_output_destroy(&output);
	CHECK(yt_text_write(path, bytes, sizeof(bytes), true, &error)
	    && yt_text_read(path, &text, &error));
	if (text.data != NULL) {
		CHECK(text.length == sizeof(bytes) + 1U
		    && memcmp(text.data, bytes, sizeof(bytes)) == 0
		    && text.data[sizeof(bytes)] == 0x1aU);
		yt_text_free(&text);
	}
	CHECK(yt_text_write(path, bytes, sizeof(bytes), false, &error)
	    && yt_text_read(path, &text, &error));
	if (text.data != NULL) {
		CHECK(text.length == sizeof(bytes)
		    && memcmp(text.data, bytes, sizeof(bytes)) == 0);
		yt_text_free(&text);
	}

	yt_text_output_init(&output);
	CHECK(!yt_text_output_write(&output, &next, 1U, &error)
	    && error.status == YT_INVALID);
	yt_text_output_destroy(&output);
	CHECK(yt_file_delete(path, false, &error));
#ifdef _WIN32
	_rmdir(directory);
#else
	rmdir(directory);
#endif
}

static void
test_text_output_close(void)
{
	static const uint8_t pending[] = {'A', 'B', 'C'};
	static const uint8_t eof_byte = 0x1aU;
	char directory[256];
	char path[320];
	struct yt_text_output output;
	struct yt_text_file text;
	struct yt_error error;

#ifdef _WIN32
	snprintf(directory, sizeof(directory), "yt-text-close-%lu",
	    (unsigned long)GetCurrentProcessId());
#else
	snprintf(directory, sizeof(directory), "/tmp/yt-text-close-%ld",
	    (long)getpid());
#endif
	(void)mkdir_one(directory);
	snprintf(path, sizeof(path), "%s/output.dat", directory);

	/* Native OUTPUT/CLOSE writes DOS EOF and truncates at the final cursor. */
	yt_text_output_init(&output);
	yt_error_clear(&error);
	CHECK(yt_text_output_open(&output, path, &error)
	    && output.last_output_open.outcome == YT_TEXT_OPEN_RETURNED
	    && output.last_output_open.created
	    && output.last_output_open.temporary_close_attempted
	    && output.last_output_open.operation_count == 5U
	    && output.last_output_open.access_attempt_count == 2U);
	CHECK(yt_text_output_close(&output, &error)
	    && output.last_close.outcome == YT_TEXT_CLOSE_RETURNED
	    && output.last_close.operation_count == 4U
	    && !output.last_close.close_all && !output.last_close.missing
	    && !output.last_close.registered && !output.last_close.handle_open);
	CHECK(yt_text_read(path, &text, &error));
	if (text.data != NULL) {
		CHECK(text.length == 1U && text.data[0] == eof_byte);
		yt_text_free(&text);
	}
	yt_text_output_destroy(&output);

	CHECK(write_bytes(path, (const uint8_t *)"stale-tail", 10U));
	yt_text_output_init(&output);
	output.file = fopen(path, "r+b");
	CHECK(output.file != NULL);
	(void)snprintf(output.path, sizeof(output.path), "%s", path);
	CHECK(output.file != NULL && fseek(output.file, 0L, SEEK_SET) == 0
	    && yt_text_output_stage(&output, pending, sizeof(pending), &error)
	    && yt_text_output_close(&output, &error));
	CHECK(yt_text_read(path, &text, &error));
	if (text.data != NULL) {
		CHECK(text.length == 4U
		    && memcmp(text.data, "ABC\x1a", 4U) == 0);
		yt_text_free(&text);
	}
	yt_text_output_destroy(&output);

	/* Missing explicit CLOSE and empty CLOSE-all retain distinct identities. */
	yt_text_output_init(&output);
	CHECK(yt_text_output_close(&output, &error)
	    && output.last_close.missing && !output.last_close.close_all
	    && output.last_close.operation_count == 0U);
	CHECK(yt_text_output_close_all(&output, &error)
	    && !output.last_close.missing && output.last_close.close_all
	    && output.last_close.operation_count == 0U);
	yt_text_output_destroy(&output);

	CHECK(yt_file_delete(path, false, &error));
#ifdef _WIN32
	_rmdir(directory);
#else
	rmdir(directory);
#endif
}

static void
test_database_random_lof(void)
{
	struct yt_database database;
	struct yt_error error;
	uint32_t length;

	memset(&database, 0, sizeof(database));
	database.file = tmpfile();
	CHECK(database.file != NULL);
	if (database.file != NULL) {
		CHECK(fwrite("abcde", 1U, 5U, database.file) == 5U
		    && fseek(database.file, 2L, SEEK_SET) == 0
		    && yt_database_random_lof(&database, &length, &error)
		    && length == 5U && ftell(database.file) == 2L
		    && database.last_lof.outcome == YT_DATABASE_LOF_RETURNED
		    && database.last_lof.saved_position == 2U
		    && database.last_lof.length == 5U
		    && database.last_lof.operation_count == 3U);
	}
	yt_database_close(&database);
	CHECK(!yt_database_random_lof(NULL, &length, &error)
	    && error.status == YT_INVALID);
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
	struct yt_text_file text;
	struct yt_error error;
	size_t accepted;
	size_t index;

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
	CHECK(database.last_get.outcome == YT_DATABASE_GET_RETURNED
	    && database.last_get.accepted == YT_RECORD_SIZE
	    && database.last_get.full_record
	    && database.last_get.terminal_position == YT_RECORD_SIZE
	    && database.last_get.registered && database.last_get.handle_open);
	for (index = 0U; index < sizeof(replacement.bytes); ++index)
		replacement.bytes[index] = (uint8_t)(index ^ 0xa5U);
	after = replacement;
	accepted = 99U;
	yt_error_clear(&error);
	CHECK(!yt_database_random_get(&database, 0U, &after, &accepted, &error)
	    && error.status == YT_RANGE && accepted == 0U
	    && memcmp(after.bytes, replacement.bytes, YT_RECORD_SIZE) == 0);
	CHECK(database.last_get.outcome == YT_DATABASE_GET_RECORD_ERROR
	    && database.last_get.basic_error == 63U
	    && database.last_get.current_record == 0U
	    && database.last_get.record_index == 0U
	    && database.last_get.desired_offset == 0
	    && database.last_get.registered && database.last_get.handle_open);
	after = replacement;
	accepted = 99U;
	yt_error_clear(&error);
	CHECK(!yt_database_random_get(&database, 0x1000000U, &after, &accepted,
	    &error) && error.status == YT_RANGE && accepted == 0U
	    && memcmp(after.bytes, replacement.bytes, YT_RECORD_SIZE) == 0
	    && database.last_get.outcome == YT_DATABASE_GET_RECORD_ERROR
	    && database.last_get.basic_error == 63U);
	CHECK(yt_database_random_get(&database, 0xFFFFFFU, &after, &accepted,
	    &error) && accepted == 0U
	    && database.last_get.outcome == YT_DATABASE_GET_RETURNED
	    && database.last_get.current_record == 0xFFFFFFU
	    && database.last_get.record_index == 0xFFFFFEU
	    && database.last_get.desired_offset
	    == (int64_t)(0xFFFFFFU - 1U) * YT_RECORD_SIZE
	    && database.last_get.terminal_position
	    == (int64_t)(0xFFFFFFU - 1U) * YT_RECORD_SIZE);
	accepted = 99U;
	CHECK(!yt_database_random_put(&database, 0U, &replacement, false,
	    &accepted, &error) && error.status == YT_RANGE && accepted == 0U
	    && database.last_put.outcome == YT_DATABASE_PUT_RECORD_ERROR
	    && database.last_put.basic_error == 63U
	    && database.last_put.current_record == 0U
	    && database.last_put.record_index == 0U
	    && database.last_put.desired_offset == 0
	    && database.last_put.registered && database.last_put.handle_open);
	accepted = 99U;
	CHECK(!yt_database_random_put(&database, 0x1000000U, &replacement,
	    false, &accepted, &error) && error.status == YT_RANGE
	    && accepted == 0U
	    && database.last_put.outcome == YT_DATABASE_PUT_RECORD_ERROR
	    && database.last_put.basic_error == 63U);
	yt_error_clear(&error);
	CHECK(yt_database_write_durable(&database, 1U, &replacement, &error)
	    && database.last_put.outcome == YT_DATABASE_PUT_RETURNED
	    && database.last_put.accepted == YT_RECORD_SIZE);
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
	char sized_path[320];
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
	snprintf(sized_path, sizeof(sized_path), "%s/sized.dat", directory);
	snprintf(failed_path, sizeof(failed_path), "%s/missing/YTRMSG.DAT",
	    directory);
	CHECK(write_bytes(mixed_path, partial, sizeof(partial)));

	yt_radio_file_init(&radio);
	yt_error_clear(&error);
	CHECK(yt_radio_file_open(&radio, requested_path, &error));
	CHECK(radio.random.file != NULL
	    && strcmp(radio.random.path, mixed_path) == 0
	    && radio.record_length == YT_RADIO_RECORD_SIZE
	    && radio.field_count == YT_RADIO_FIELD_COUNT
	    && radio.fields[0].offset == 0U && radio.fields[0].length == 4U
	    && radio.fields[1].offset == 4U && radio.fields[1].length == 4U
	    && radio.fields[2].offset == 8U && radio.fields[2].length == 4U
	    && radio.fields[3].offset == 12U && radio.fields[3].length == 74U);
	CHECK(yt_radio_file_close(&radio, &error));
	CHECK(yt_radio_file_open_text_width(&radio, requested_path, 72U,
	    &error)
	    && radio.record_length == YT_RADIO_RECORD_SIZE
	    && radio.field_count == YT_RADIO_FIELD_COUNT
	    && radio.fields[0].offset == 0U && radio.fields[0].length == 4U
	    && radio.fields[1].offset == 4U && radio.fields[1].length == 4U
	    && radio.fields[2].offset == 8U && radio.fields[2].length == 4U
	    && radio.fields[3].offset == 12U && radio.fields[3].length == 72U);
	memset(&record, 0xff, sizeof(record));
	accepted = 99U;
	CHECK(!yt_radio_file_get(&radio, 0U, &record, &accepted, &error)
	    && accepted == 0U && error.status == YT_RANGE
	    && radio.random.last_get.outcome == YT_DATABASE_GET_RECORD_ERROR
	    && radio.random.last_get.basic_error == 63U
	    && radio.random.last_get.registered
	    && radio.random.last_get.handle_open);
	CHECK(!yt_radio_file_get(&radio, 0x1000000U, &record, &accepted,
	    &error)
	    && radio.random.last_get.outcome == YT_DATABASE_GET_RECORD_ERROR
	    && radio.random.last_get.basic_error == 63U);
	CHECK(!yt_radio_file_put(&radio, 0U, &record, &error)
	    && error.status == YT_RANGE
	    && radio.random.last_put.outcome == YT_DATABASE_PUT_RECORD_ERROR
	    && radio.random.last_put.basic_error == 63U
	    && radio.random.last_put.registered
	    && radio.random.last_put.handle_open);
	CHECK(!yt_radio_file_put(&radio, 0x1000000U, &record, &error)
	    && radio.random.last_put.outcome == YT_DATABASE_PUT_RECORD_ERROR
	    && radio.random.last_put.basic_error == 63U);
	for (index = 0U; index < sizeof(record.bytes); ++index)
		CHECK(record.bytes[index] == 0xffU);
	CHECK(yt_radio_file_size(&radio, &size, &error) && size == 3U);
	CHECK(fseek(radio.random.file, 2L, SEEK_SET) == 0
	    && yt_radio_file_size(&radio, &size, &error) && size == 3U
	    && ftell(radio.random.file) == 2L
	    && radio.random.last_lof.outcome == YT_DATABASE_LOF_RETURNED
	    && radio.random.last_lof.saved_position == 2U
	    && radio.random.last_lof.length == 3U
	    && radio.random.last_lof.operation_count == 3U);
	memset(&record, 0xff, sizeof(record));
	CHECK(yt_radio_file_get(&radio, 1U, &record, &accepted, &error)
	    && accepted == sizeof(partial)
	    && memcmp(record.bytes, partial, sizeof(partial)) == 0
	    && radio.random.last_get.outcome == YT_DATABASE_GET_RETURNED
	    && radio.random.last_get.current_record == 1U
	    && radio.random.last_get.record_index == 0U
	    && radio.random.last_get.desired_offset == 0
	    && radio.random.last_get.terminal_position == 3
	    && !radio.random.last_get.full_record);
	for (index = sizeof(partial); index < sizeof(record.bytes); ++index)
		CHECK(record.bytes[index] == 0U);
	memset(&record, 0xff, sizeof(record));
	CHECK(yt_radio_file_get(&radio, 2U, &record, &accepted, &error)
	    && accepted == 0U
	    && radio.random.last_get.current_record == 2U
	    && radio.random.last_get.record_index == 1U
	    && radio.random.last_get.desired_offset == YT_RADIO_RECORD_SIZE
	    && radio.random.last_get.terminal_position == YT_RADIO_RECORD_SIZE
	    && !radio.random.last_get.full_record);
	for (index = 0U; index < sizeof(record.bytes); ++index)
		CHECK(record.bytes[index] == 0U);
	yt_error_clear(&error);
	CHECK(!yt_radio_file_next_record(&radio, &next, &error)
	    && error.status == YT_RANGE
	    && strcmp(error.operation, "radio record number") == 0);
	CHECK(yt_radio_file_size(&radio, &size, &error) && size == 3U);

	/* Reopening the same BASIC file slot closes the prior handle first. */
	CHECK(yt_radio_file_open(&radio, second_path, &error));
	CHECK(strcmp(radio.random.path, second_path) == 0
	    && yt_radio_file_size(&radio, &size, &error) && size == 0U);
	CHECK(yt_radio_file_next_record(&radio, &next, &error) && next == 1U);
	memset(&written, 0, sizeof(written));
	CHECK(yt_radio_set_number(&written, 0U, 30.0f)
	    && yt_radio_set_number(&written, 4U, -2.0f)
	    && yt_radio_set_number(&written, 8U, 7.0f));
	yt_radio_set_text(&written, (const uint8_t *)"A\0B", 3U, 74U);
	CHECK(yt_radio_file_get(&radio, next, &record, &accepted, &error)
	    && accepted == 0U);
	record = written;
	CHECK(yt_radio_file_put(&radio, next, &record, &error)
	    && radio.random.last_put.outcome == YT_DATABASE_PUT_RETURNED
	    && radio.random.last_put.accepted == YT_RADIO_RECORD_SIZE
	    && radio.random.last_put.current_record == 1U
	    && radio.random.last_put.record_index == 0U
	    && radio.random.last_put.desired_offset == 0
	    && radio.random.last_put.terminal_position == YT_RADIO_RECORD_SIZE
	    && radio.random.last_put.registered
	    && radio.random.last_put.handle_open);
	CHECK(yt_radio_file_size(&radio, &size, &error)
	    && size == YT_RADIO_RECORD_SIZE);
	CHECK(yt_radio_file_next_record(&radio, &next, &error) && next == 2U);
	CHECK(yt_radio_file_close(&radio, &error)
	    && radio.random.file == NULL
	    && radio.random.last_close.outcome == YT_DATABASE_CLOSE_RETURNED
	    && radio.random.last_close.attempt_count == 1U
	    && radio.record_length == 0U && radio.field_count == 0U);

	file = fopen(second_path, "rb");
	CHECK(file != NULL);
	if (file != NULL) {
		CHECK(fread(complete, 1, sizeof(complete), file) == sizeof(complete));
		CHECK(fgetc(file) == EOF && !ferror(file));
		CHECK(fclose(file) == 0);
		CHECK(memcmp(complete, written.bytes, sizeof(complete)) == 0);
	}
	file = fopen(sized_path, "wb");
	CHECK(file != NULL);
	if (file != NULL) {
		CHECK(fwrite(complete, 1U, sizeof(complete), file)
		    == sizeof(complete));
		CHECK(fwrite(complete, 1U, sizeof(complete), file)
		    == sizeof(complete));
		CHECK(fclose(file) == 0);
	}
	yt_radio_file_init(&radio);
	CHECK(yt_radio_file_open(&radio, sized_path, &error)
	    && radio.random.records == 2U
	    && yt_radio_file_size(&radio, &size, &error)
	    && size == 2U * YT_RADIO_RECORD_SIZE);
	CHECK(yt_radio_file_close(&radio, &error));

	yt_radio_file_init(&radio);
	yt_error_clear(&error);
	CHECK(!yt_radio_file_open(&radio, failed_path, &error)
	    && error.status == YT_IO_ERROR && radio.random.file == NULL
	    && radio.field_count == 0U && radio.record_length == 0U);
	CHECK(!yt_radio_file_get(&radio, 1U, &record, NULL, &error));
	CHECK(!yt_radio_file_put(&radio, 1U, &record, &error));

	CHECK(yt_file_delete(mixed_path, false, &error));
	CHECK(yt_file_delete(second_path, false, &error));
	CHECK(yt_file_delete(sized_path, false, &error));
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
	char missing_parent_path[384];
	uint8_t original[200];
	uint8_t payload[127];
	uint8_t embedded[] = {'A', 0x1a, 'B'};
	struct yt_text_output output;
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
	snprintf(missing_parent_path, sizeof(missing_parent_path),
	    "%s/missing/window.dat", directory);
	for (index = 0; index < sizeof(original); ++index)
		original[index] = (uint8_t)(index == 0 ? 0 : 'x');

	/* Native absence takes create/temporary-close/reopen; no parent is ERR76. */
	yt_text_output_init(&output);
	yt_error_clear(&error);
	CHECK(yt_text_output_open_append(&output, path, &error)
	    && output.last_append_open.outcome == YT_TEXT_OPEN_RETURNED
	    && output.last_append_open.created
	    && output.last_append_open.temporary_close_attempted
	    && output.last_append_open.access_attempt_count == 2U
	    && output.last_append_open.refill_count == 1U
	    && output.last_append_open.selected_position == 0);
	yt_text_output_destroy(&output);
	yt_text_output_init(&output);
	CHECK(!yt_text_output_open_append(&output, missing_parent_path, &error)
	    && output.last_append_open.outcome
	    == YT_TEXT_OPEN_INITIAL_ERROR
	    && output.last_append_open.basic_error == 76U
	    && output.last_append_open.dos_error == 3U
	    && output.last_append_open.access_attempt_count == 1U);
	yt_text_output_destroy(&output);

	/* Empty sequential OUTPUT/CLOSE retains the BRUN DOS EOF byte. */
	yt_error_clear(&error);
	CHECK(yt_text_write(path, NULL, 0U, true, &error));
	CHECK(yt_text_read(path, &text, &error));
	CHECK(text.length == 1U && text.data[0] == 0x1aU);
	yt_text_free(&text);

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

	/* OPEN APPEND selects the tail marker without changing stale bytes. */
	CHECK(write_bytes(path, (const uint8_t *)"old\x1a" "stale", 9U));
	yt_text_output_init(&output);
	CHECK(yt_text_output_open_append(&output, path, &error)
	    && ftell(output.file) == 3L
	    && output.last_append_open.outcome == YT_TEXT_OPEN_RETURNED
	    && output.last_append_open.physical_length == 9
	    && output.last_append_open.window_start == 0
	    && output.last_append_open.selected_position == 3
	    && output.last_append_open.refill_count == 1U);
	yt_text_output_destroy(&output);
	CHECK(yt_text_read(path, &text, &error));
	CHECK(text.length == 9U && memcmp(text.data, "old\x1a" "stale", 9U) == 0);
	yt_text_free(&text);

	/* Payload 128 stays pending; payload 129 flushes before accepting LF. */
	memset(payload, 'p', sizeof(payload));
	CHECK(write_bytes(path, NULL, 0U)
	    && yt_text_append_line(path, payload, 126U, &error)
	    && yt_text_read(path, &text, &error));
	CHECK(text.length == 129U
	    && memcmp(text.data, payload, 126U) == 0
	    && memcmp(text.data + 126U, "\r\n\x1a", 3U) == 0);
	yt_text_free(&text);
	CHECK(write_bytes(path, NULL, 0U)
	    && yt_text_append_line(path, payload, 127U, &error)
	    && yt_text_read(path, &text, &error));
	CHECK(text.length == 130U
	    && memcmp(text.data, payload, 127U) == 0
	    && memcmp(text.data + 127U, "\r\n\x1a", 3U) == 0);
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
	CHECK(capture_main_error_row(&capture, result.action,
	    result.action_length, &error)
	    && yt_main_error_append_fatal_to(path, &result, &error));
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
	CHECK(capture_main_error_row(&capture, result.action,
	    result.action_length, &error)
	    && yt_main_error_append_fatal_to(path, &result, &error));
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
	CHECK(!capture_main_error_row(&capture, result.action,
	    result.action_length, &error));
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
	CHECK(!yt_main_error_append_fatal_to(path, &invalid, &error));
	CHECK(capture.calls == 0U && error.status == YT_INVALID);

	memset(&capture, 0, sizeof(capture));
	capture.path = path;
	capture.expected_before = stale;
	capture.expected_before_length = sizeof(stale) - 1U;
	capture.succeeds = true;
	yt_error_clear(&error);
	CHECK(capture_main_error_row(&capture, result.action,
	    result.action_length, &error)
	    && !yt_main_error_append_fatal_to(failed_path, &result, &error));
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
test_text_input(void)
{
	char directory[256];
	char actual[320];
	char requested[320];
	char missing[320];
	char missing_parent[384];
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
	snprintf(missing_parent, sizeof(missing_parent),
	    "%s/absent/MISSING.TXT", directory);
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
	CHECK(input.file != NULL && strcmp(input.path, actual) == 0
	    && input.last_open.outcome == YT_TEXT_OPEN_RETURNED
	    && input.last_open.operation_count == 2U
	    && input.last_open.access_attempt_count == 1U
	    && input.last_open.access_attempts[0] == 0U
	    && input.last_open.terminal_position == 0
	    && input.last_open.registered && input.last_open.handle_open);
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
	CHECK(yt_text_input_close(&input, &error) && input.file == NULL
	    && input.last_close.outcome == YT_TEXT_CLOSE_RETURNED
	    && input.last_close.operation_count == 1U
	    && !input.last_close.missing && !input.last_close.device
	    && !input.last_close.registered
	    && !input.last_close.handle_open);
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
	    && error.status == YT_NOT_FOUND && input.file == NULL
	    && input.last_open.outcome == YT_TEXT_OPEN_INITIAL_ERROR
	    && input.last_open.dos_error == 2U
	    && input.last_open.basic_error == 53U);
	yt_error_clear(&error);
	CHECK(!yt_text_input_open(&input, missing_parent, &error)
	    && error.status == YT_NOT_FOUND && input.file == NULL
	    && input.last_open.outcome == YT_TEXT_OPEN_INITIAL_ERROR
	    && input.last_open.dos_error == 3U
	    && input.last_open.basic_error == 76U);
	yt_text_input_destroy(&input);
	CHECK(yt_file_delete(actual, false, &error));
#ifdef _WIN32
	_rmdir(directory);
#else
	rmdir(directory);
#endif
}

struct viewer_file_context {
	uint8_t rows[3][32];
	size_t lengths[3];
	size_t row_count;
	size_t blank_count;
};

static bool
viewer_file_present(void *context, const uint8_t *text, size_t length,
    bool paged, struct yt_error *error)
{
	struct viewer_file_context *viewer = context;
	size_t index = viewer->row_count;

	(void)error;
	if (!paged) {
		CHECK(text == NULL && length == 0U);
		++viewer->blank_count;
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
	static const uint8_t source[] =
	    "ordinary\r\n  - item\r\n\x1aignored\r\n";
	char directory[256];
	char actual[320];
	char requested[320];
	struct viewer_file_context viewer;
	struct yt_file_viewer_state state;
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
	state.foreground = &foreground;
	state.pager_foreground = &pager_foreground;
	state.bold = &bold;
	state.line_count = &line_count;
	state.pager_key = key;
	state.saved_foreground = foreground;
	state.saved_pager_foreground = pager_foreground;
	yt_error_clear(&error);
	CHECK(yt_file_viewer_display(requested, &state,
	    viewer_file_present, &viewer, &error));
	CHECK(viewer.row_count == 3U && viewer.blank_count == 2U);
	CHECK(viewer.lengths[0] == strlen("Cntl-X to Stop")
	    && memcmp(viewer.rows[0], "Cntl-X to Stop",
	    strlen("Cntl-X to Stop")) == 0
	    && viewer.lengths[1] == 8U
	    && memcmp(viewer.rows[1], "ordinary", 8U) == 0
	    && viewer.lengths[2] == 8U
	    && memcmp(viewer.rows[2], "  - item", 8U) == 0
	    && foreground == 5.0f && pager_foreground == 5
	    && bold == 1.0f && line_count == 0.0f);
	CHECK(yt_file_delete(actual, false, &error));
#ifdef _WIN32
	_rmdir(directory);
#else
	rmdir(directory);
#endif
}

static void
test_file_viewer_missing(void)
{
	static const uint8_t expected[] =
	    "*** GAME FILE [YTNEWS.DAT] NOT FOUND! ***";
	uint8_t row[64];
	size_t length = 0U;

	CHECK(yt_file_viewer_missing_row("YTNEWS.DAT", row, sizeof(row),
	    &length));
	CHECK(length == sizeof(expected) - 1U
	    && memcmp(row, expected, sizeof(expected) - 1U) == 0);
	CHECK(!yt_file_viewer_missing_row("YTNEWS.DAT", row,
	    sizeof(expected) - 2U, &length));
}

int
main(void)
{
	test_record();
	test_clock();
	test_random();
	test_database_random_open();
	test_file_kill();
	test_file_rename();
	test_database_random_close();
	test_text_output_write();
	test_text_output_close();
	test_database_random_lof();
	test_files();
	test_radio_file();
	test_append_window();
	test_main_error_fatal_transaction();
	test_line_input_grammar();
	test_text_input();
	test_file_viewer_physical_stream();
	test_file_viewer_missing();
	if (failures != 0) {
		fprintf(stderr, "%u test(s) failed\n", failures);
		return EXIT_FAILURE;
	}
	puts("test_data: ok");
	return EXIT_SUCCESS;
}
