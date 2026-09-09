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
	bool provider_reject;
	uint16_t dos_error;
	uint16_t mapped_error;
	int64_t terminal_position;
	size_t calls;
	size_t requested;
};

struct database_read_script {
	const uint8_t *data;
	size_t accepted;
	bool carry;
	bool provider_reject;
	uint16_t dos_error;
	uint16_t mapped_error;
	int64_t terminal_position;
	size_t calls;
	size_t requested;
};

struct database_seek_script {
	bool success;
	bool provider_reject;
	uint16_t dos_error;
	int64_t terminal_position;
	size_t calls;
	int64_t absolute_offset;
};

struct database_close_script {
	bool success;
	bool handle_open;
	size_t calls;
	size_t attempt;
};

struct database_flush_script {
	bool success;
	size_t calls;
};

struct database_open_step {
	enum yt_database_open_operation operation;
	uint8_t access;
	uint16_t prior_dos_error;
	struct yt_database_open_observation observation;
	bool provider_ok;
	bool supply_file;
	bool close_active;
	size_t file_length;
};

struct database_open_script {
	struct database_open_step steps[12];
	size_t length;
	size_t position;
};

struct database_public_close_step {
	struct yt_database_close_observation observation;
	bool provider_ok;
	bool expect_active;
	bool close_active;
};

struct database_public_close_script {
	struct database_public_close_step steps[2];
	size_t length;
	size_t position;
};

struct database_lof_step {
	enum yt_database_lof_operation operation;
	uint32_t restore_position;
	struct yt_database_lof_observation observation;
	bool provider_ok;
};

struct database_lof_script {
	struct database_lof_step steps[3];
	size_t length;
	size_t position;
};

struct text_close_step {
	enum yt_text_close_operation operation;
	size_t requested;
	uint8_t expected[YT_TEXT_OUTPUT_BUFFER_SIZE];
	struct yt_text_close_observation observation;
	bool provider_ok;
	bool expect_active;
	bool close_active;
};

struct text_close_script {
	struct text_close_step steps[5];
	size_t length;
	size_t position;
};

struct text_output_write_step {
	uint8_t expected[YT_TEXT_OUTPUT_BUFFER_SIZE];
	struct yt_text_output_write_observation observation;
	bool provider_ok;
};

struct text_output_write_script {
	struct text_output_write_step steps[4];
	size_t length;
	size_t position;
};

struct text_device_write_step {
	enum yt_text_device_write_phase phase;
	uint8_t expected;
	size_t requested;
	struct yt_text_device_write_observation observation;
	bool provider_ok;
};

struct text_device_write_script {
	struct text_device_write_step steps[8];
	uint8_t physical[8];
	size_t length;
	size_t position;
	size_t physical_length;
};

struct text_device_close_script {
	struct yt_text_device_close_observation observation;
	size_t calls;
	bool provider_ok;
};

struct text_input_read_step {
	uint8_t data[YT_TEXT_INPUT_BUFFER_SIZE];
	struct yt_text_input_read_observation observation;
	bool provider_ok;
};

struct text_input_read_script {
	struct text_input_read_step steps[8];
	size_t length;
	size_t position;
};

struct file_kill_step {
	enum yt_file_kill_operation operation;
	struct yt_file_kill_observation observation;
	char selected[512];
	bool provider_ok;
	bool replace_selected;
};

struct file_kill_script {
	struct file_kill_step steps[4];
	size_t length;
	size_t position;
};

struct file_rename_step {
	enum yt_file_rename_operation operation;
	struct yt_file_rename_observation observation;
	char selected_old[512];
	char selected_new[512];
	bool provider_ok;
	bool replace_old;
	bool replace_new;
};

struct file_rename_script {
	struct file_rename_step steps[5];
	size_t length;
	size_t position;
};

struct text_open_step {
	enum yt_text_open_operation operation;
	uint8_t access;
	uint16_t prior_dos_error;
	int64_t offset;
	size_t requested;
	uint8_t data[YT_TEXT_OUTPUT_BUFFER_SIZE];
	struct yt_text_open_observation observation;
	bool provider_ok;
	bool expect_active;
	bool supply_file;
	bool close_active;
};

struct text_open_script {
	struct text_open_step steps[16];
	size_t length;
	size_t position;
};

static struct text_open_step *
text_open_add(struct text_open_script *script,
    enum yt_text_open_operation operation, uint8_t access,
    uint16_t prior_dos_error)
{
	struct text_open_step *step;

	CHECK(script->length < YT_ARRAY_LEN(script->steps));
	if (script->length >= YT_ARRAY_LEN(script->steps))
		return NULL;
	step = &script->steps[script->length++];
	memset(step, 0, sizeof(*step));
	step->operation = operation;
	step->access = access;
	step->prior_dos_error = prior_dos_error;
	step->observation.terminal_position = -1;
	step->provider_ok = true;
	step->expect_active = operation == YT_TEXT_OPEN_TEMP_CLOSE
	    || operation == YT_TEXT_OPEN_QUERY_DEVICE
	    || operation == YT_TEXT_OPEN_CONFIGURE_DEVICE
	    || operation == YT_TEXT_OPEN_SEEK_END
	    || operation == YT_TEXT_OPEN_SEEK_WINDOW
	    || operation == YT_TEXT_OPEN_READ_WINDOW
	    || operation == YT_TEXT_OPEN_SEEK_SELECTED;
	return step;
}

static void
text_open_add_error(struct text_open_script *script,
    enum yt_text_open_operation operation, uint8_t access,
    uint16_t prior_dos_error, uint16_t dos_error, uint16_t mapped_error)
{
	struct text_open_step *step = text_open_add(script,
	    operation, access, prior_dos_error);

	if (step == NULL)
		return;
	step->observation.carry = true;
	step->observation.dos_error = dos_error;
	step->observation.mapped_error = mapped_error;
	step->observation.handle_open = step->expect_active;
}

static void
text_open_add_file(struct text_open_script *script,
    enum yt_text_open_operation operation, uint8_t access)
{
	struct text_open_step *step = text_open_add(script,
	    operation, access, 0U);

	if (step == NULL)
		return;
	step->observation.handle_open = true;
	step->observation.terminal_position = 0;
	step->supply_file = true;
}

static void
text_open_add_query(struct text_open_script *script,
    uint8_t access, bool carry, uint16_t dos_error, bool device)
{
	struct text_open_step *step = text_open_add(script,
	    YT_TEXT_OPEN_QUERY_DEVICE, access, 0U);

	if (step == NULL)
		return;
	step->observation.carry = carry;
	step->observation.dos_error = dos_error;
	step->observation.device = device;
	step->observation.handle_open = true;
}

static void
text_open_add_extended(struct text_open_script *script,
    uint16_t prior_dos_error, uint16_t mapped_error)
{
	struct text_open_step *step = text_open_add(script,
	    YT_TEXT_OPEN_EXTENDED_ERROR, 0U, prior_dos_error);

	if (step != NULL)
		step->observation.mapped_error = mapped_error;
}

static void
text_open_add_seek(struct text_open_script *script,
    enum yt_text_open_operation operation, uint8_t access,
    int64_t offset, int64_t terminal_position)
{
	struct text_open_step *step = text_open_add(script,
	    operation, access, 0U);

	if (step == NULL)
		return;
	step->offset = offset;
	step->observation.handle_open = true;
	step->observation.terminal_position = terminal_position;
}

static void
text_open_add_read(struct text_open_script *script,
    uint8_t access, const uint8_t *data, size_t accepted,
    int64_t terminal_position)
{
	struct text_open_step *step = text_open_add(script,
	    YT_TEXT_OPEN_READ_WINDOW, access, 0U);

	if (step == NULL)
		return;
	step->requested = YT_TEXT_OUTPUT_BUFFER_SIZE;
	step->observation.accepted = accepted;
	step->observation.handle_open = true;
	step->observation.terminal_position = terminal_position;
	if (accepted != 0U)
		memcpy(step->data, data, accepted);
}

static bool
scripted_text_open(void *context, const char *path,
    enum yt_text_open_operation operation, uint8_t access,
    FILE *active_file, int64_t offset, uint8_t *data, size_t requested,
    uint16_t prior_dos_error,
    struct yt_text_open_observation *observation)
{
	struct text_open_script *script = context;
	struct text_open_step *step;

	(void)path;
	CHECK(script->position < script->length);
	if (script->position >= script->length)
		return false;
	step = &script->steps[script->position++];
	CHECK(operation == step->operation && access == step->access
	    && prior_dos_error == step->prior_dos_error
	    && (active_file != NULL) == step->expect_active
	    && offset == step->offset && requested == step->requested);
	if (operation != step->operation || access != step->access
	    || prior_dos_error != step->prior_dos_error
	    || (active_file != NULL) != step->expect_active
	    || offset != step->offset || requested != step->requested)
		return false;
	if (!step->provider_ok)
		return false;
	*observation = step->observation;
	if (step->supply_file) {
		observation->file = tmpfile();
		CHECK(observation->file != NULL);
		if (observation->file == NULL)
			return false;
	}
	if (step->observation.accepted != 0U) {
		CHECK(data != NULL);
		if (data == NULL)
			return false;
		memcpy(data, step->data, step->observation.accepted);
	}
	if (step->close_active) {
		CHECK(active_file != NULL);
		if (active_file == NULL || fclose(active_file) != 0)
			return false;
	}
	return true;
}

static void
text_open_use(struct yt_text_output *output,
    struct text_open_script *script)
{
	yt_text_output_init(output);
	yt_text_output_set_open_provider(output,
	    scripted_text_open, script);
}

static void
text_open_use_input(struct yt_text_input *input,
    struct text_open_script *script)
{
	yt_text_input_init(input);
	yt_text_input_set_open_provider(input,
	    scripted_text_open, script);
}

static void
text_open_check_consumed(const struct text_open_script *script)
{
	CHECK(script->position == script->length);
}

static void
database_open_add(struct database_open_script *script,
    enum yt_database_open_operation operation, uint8_t access,
    uint16_t prior_dos_error, bool carry, uint16_t dos_error,
    uint16_t mapped_error, bool device, bool handle_open,
    bool supply_file, bool close_active, size_t file_length)
{
	struct database_open_step *step;

	CHECK(script->length < YT_ARRAY_LEN(script->steps));
	if (script->length >= YT_ARRAY_LEN(script->steps))
		return;
	step = &script->steps[script->length++];
	memset(step, 0, sizeof(*step));
	step->operation = operation;
	step->access = access;
	step->prior_dos_error = prior_dos_error;
	step->observation.carry = carry;
	step->observation.dos_error = dos_error;
	step->observation.mapped_error = mapped_error;
	step->observation.device = device;
	step->observation.handle_open = handle_open;
	step->provider_ok = true;
	step->supply_file = supply_file;
	step->close_active = close_active;
	step->file_length = file_length;
}

static void
database_open_add_failure(struct database_open_script *script,
    enum yt_database_open_operation operation, uint8_t access,
    uint16_t prior_dos_error, uint16_t dos_error, bool handle_open)
{
	database_open_add(script, operation, access, prior_dos_error, true,
	    dos_error, 0U, false, handle_open, false, false, 0U);
}

static void
database_open_add_file(struct database_open_script *script, uint8_t access,
    size_t file_length)
{
	database_open_add(script, YT_DATABASE_OPEN_EXISTING, access, 0U, false,
	    0U, 0U, false, true, true, false, file_length);
}

static void
database_open_add_query(struct database_open_script *script, uint8_t access,
    bool carry, uint16_t dos_error, bool device)
{
	database_open_add(script, YT_DATABASE_OPEN_QUERY_DEVICE, access, 0U, carry,
	    dos_error, 0U, device, true, false, false, 0U);
}

static bool
scripted_database_open(void *context, const char *path,
    enum yt_database_open_operation operation, uint8_t access,
    FILE *active_file, uint16_t prior_dos_error,
    struct yt_database_open_observation *observation)
{
	struct database_open_script *script = context;
	struct database_open_step *step;
	size_t index;

	(void)path;
	CHECK(script->position < script->length);
	if (script->position >= script->length)
		return false;
	step = &script->steps[script->position++];
	CHECK(operation == step->operation && access == step->access
	    && prior_dos_error == step->prior_dos_error);
	if (operation != step->operation || access != step->access
	    || prior_dos_error != step->prior_dos_error)
		return false;
	if (!step->provider_ok)
		return false;
	*observation = step->observation;
	if (step->supply_file) {
		observation->file = tmpfile();
		CHECK(observation->file != NULL);
		if (observation->file == NULL)
			return false;
		for (index = 0U; index < step->file_length; ++index)
			CHECK(fputc((int)(index & 0xffU), observation->file) != EOF);
		CHECK(fflush(observation->file) == 0
		    && fseek(observation->file, 0L, SEEK_SET) == 0);
	}
	if (step->close_active) {
		CHECK(active_file != NULL);
		if (active_file == NULL || fclose(active_file) != 0)
			return false;
	}
	return true;
}

static void
database_open_check_consumed(const struct database_open_script *script)
{
	CHECK(script->position == script->length);
}

static void
database_open_add_missing_prefix(struct database_open_script *script)
{
	database_open_add_failure(script, YT_DATABASE_OPEN_EXISTING, 2U, 0U,
	    2U, false);
	database_open_add(script, YT_DATABASE_OPEN_CREATE, 2U, 0U, false, 0U,
	    0U, false, true, true, false, 0U);
}

static void
database_open_add_closed_missing_prefix(struct database_open_script *script)
{
	database_open_add_missing_prefix(script);
	database_open_add(script, YT_DATABASE_OPEN_TEMP_CLOSE, 0U, 0U, false,
	    0U, 0U, false, false, false, true, 0U);
}

static void
database_close_add(struct database_public_close_script *script, bool carry,
    uint16_t dos_error, bool handle_open, bool expect_active,
    bool close_active)
{
	struct database_public_close_step *step;

	CHECK(script->length < YT_ARRAY_LEN(script->steps));
	if (script->length >= YT_ARRAY_LEN(script->steps))
		return;
	step = &script->steps[script->length++];
	memset(step, 0, sizeof(*step));
	step->observation.carry = carry;
	step->observation.dos_error = dos_error;
	step->observation.handle_open = handle_open;
	step->provider_ok = true;
	step->expect_active = expect_active;
	step->close_active = close_active;
}

static bool
scripted_database_public_close(void *context, FILE *active_file,
    size_t attempt, struct yt_database_close_observation *observation)
{
	struct database_public_close_script *script = context;
	struct database_public_close_step *step;

	CHECK(script->position < script->length);
	if (script->position >= script->length)
		return false;
	step = &script->steps[script->position++];
	CHECK(attempt == script->position
	    && (active_file != NULL) == step->expect_active);
	if (attempt != script->position
	    || (active_file != NULL) != step->expect_active)
		return false;
	if (!step->provider_ok)
		return false;
	*observation = step->observation;
	if (step->close_active) {
		CHECK(active_file != NULL);
		if (active_file == NULL || fclose(active_file) != 0)
			return false;
	}
	return true;
}

static bool
database_close_fixture(struct yt_database *database, bool device,
    struct database_public_close_script *script)
{
	memset(database, 0, sizeof(*database));
	database->file = tmpfile();
	if (database->file == NULL)
		return false;
	(void)snprintf(database->path, sizeof(database->path), "%s",
	    "CLOSE-ORACLE.DAT");
	database->records = 2U;
	database->last_open.device = device;
	yt_database_set_close_provider(database, scripted_database_public_close,
	    script);
	return true;
}

static void
text_close_add(struct text_close_script *script,
    enum yt_text_close_operation operation, const uint8_t *expected,
    size_t requested, size_t accepted, bool carry, uint16_t dos_error,
    bool handle_open, bool expect_active, bool close_active)
{
	struct text_close_step *step;

	CHECK(script->length < YT_ARRAY_LEN(script->steps));
	if (script->length >= YT_ARRAY_LEN(script->steps))
		return;
	step = &script->steps[script->length++];
	memset(step, 0, sizeof(*step));
	step->operation = operation;
	step->requested = requested;
	CHECK(requested <= sizeof(step->expected));
	if (expected != NULL && requested <= sizeof(step->expected))
		memcpy(step->expected, expected, requested);
	step->observation.accepted = accepted;
	step->observation.carry = carry;
	step->observation.handle_open = handle_open;
	step->observation.dos_error = dos_error;
	step->observation.terminal_position = 37;
	step->provider_ok = true;
	step->expect_active = expect_active;
	step->close_active = close_active;
}

static bool
scripted_text_close(void *context, FILE *active_file,
    enum yt_text_close_operation operation, const uint8_t *data,
    size_t requested, struct yt_text_close_observation *observation)
{
	struct text_close_script *script = context;
	struct text_close_step *step;

	CHECK(script->position < script->length);
	if (script->position >= script->length)
		return false;
	step = &script->steps[script->position++];
	CHECK(operation == step->operation && requested == step->requested
	    && (active_file != NULL) == step->expect_active);
	if (operation != step->operation || requested != step->requested
	    || (active_file != NULL) != step->expect_active)
		return false;
	CHECK(requested == 0U || (data != NULL
	    && memcmp(data, step->expected, requested) == 0));
	if (requested != 0U && (data == NULL
	    || memcmp(data, step->expected, requested) != 0))
		return false;
	if (!step->provider_ok)
		return false;
	*observation = step->observation;
	if (step->close_active) {
		CHECK(active_file != NULL);
		if (active_file == NULL || fclose(active_file) != 0)
			return false;
	}
	return true;
}

static bool
text_output_close_fixture(struct yt_text_output *output,
    struct text_close_script *script, const uint8_t *pending,
    size_t pending_length)
{
	yt_text_output_init(output);
	output->file = tmpfile();
	if (output->file == NULL)
		return false;
	(void)snprintf(output->path, sizeof(output->path), "%s",
	    "TEXT-CLOSE-ORACLE.DAT");
	if (pending_length > sizeof(output->pending))
		return false;
	if (pending_length != 0U)
		memcpy(output->pending, pending, pending_length);
	output->pending_count = pending_length;
	yt_text_output_set_close_provider(output, scripted_text_close,
	    script);
	return true;
}

static bool
text_input_close_fixture(struct yt_text_input *input,
    struct text_close_script *script, bool device)
{
	yt_text_input_init(input);
	input->file = tmpfile();
	if (input->file == NULL)
		return false;
	(void)snprintf(input->path, sizeof(input->path), "%s",
	    "TEXT-INPUT-CLOSE-ORACLE.DAT");
	input->last_open.outcome = YT_TEXT_OPEN_RETURNED;
	input->last_open.device = device;
	input->last_open.registered = true;
	input->last_open.handle_open = true;
	yt_text_input_set_close_provider(input, scripted_text_close, script);
	return true;
}

static void
text_output_write_add(struct text_output_write_script *script,
    const uint8_t *expected, size_t accepted, bool carry,
    uint16_t dos_error, bool handle_open)
{
	struct text_output_write_step *step;

	CHECK(script->length < YT_ARRAY_LEN(script->steps));
	if (script->length >= YT_ARRAY_LEN(script->steps))
		return;
	step = &script->steps[script->length++];
	memset(step, 0, sizeof(*step));
	memcpy(step->expected, expected, sizeof(step->expected));
	step->observation.accepted = accepted;
	step->observation.carry = carry;
	step->observation.handle_open = handle_open;
	step->observation.dos_error = dos_error;
	step->observation.terminal_position = carry ? -1 : 41;
	step->provider_ok = true;
}

static bool
scripted_text_output_write(void *context, FILE *active_file,
    const uint8_t *data, size_t requested,
    struct yt_text_output_write_observation *observation)
{
	struct text_output_write_script *script = context;
	struct text_output_write_step *step;

	CHECK(script->position < script->length);
	if (script->position >= script->length)
		return false;
	step = &script->steps[script->position++];
	CHECK(active_file != NULL && requested == sizeof(step->expected)
	    && data != NULL
	    && memcmp(data, step->expected, sizeof(step->expected)) == 0);
	if (active_file == NULL || requested != sizeof(step->expected)
	    || data == NULL
	    || memcmp(data, step->expected, sizeof(step->expected)) != 0)
		return false;
	if (!step->provider_ok)
		return false;
	*observation = step->observation;
	return true;
}

static void
text_device_write_add(struct text_device_write_script *script,
    enum yt_text_device_write_phase phase, uint8_t expected,
    size_t requested, size_t accepted, bool carry, bool physical_unknown,
    uint16_t dos_error, uint16_t extended_ax)
{
	struct text_device_write_step *step;

	CHECK(script->length < YT_ARRAY_LEN(script->steps));
	if (script->length >= YT_ARRAY_LEN(script->steps))
		return;
	step = &script->steps[script->length++];
	memset(step, 0, sizeof(*step));
	step->phase = phase;
	step->expected = expected;
	step->requested = requested;
	step->observation.accepted = accepted;
	step->observation.carry = carry;
	step->observation.physical_unknown = physical_unknown;
	step->observation.dos_error = dos_error;
	step->observation.extended_ax = extended_ax;
	step->observation.terminal_position = carry ? -1 : 51;
	step->provider_ok = true;
}

static bool
scripted_text_device_write(void *context,
    enum yt_text_device_write_phase phase, const uint8_t *data,
    size_t requested, struct yt_text_device_write_observation *observation)
{
	struct text_device_write_script *script = context;
	struct text_device_write_step *step;

	CHECK(script->position < script->length);
	if (script->position >= script->length)
		return false;
	step = &script->steps[script->position++];
	CHECK(phase == step->phase && requested == step->requested
	    && data != NULL
	    && (requested == 0U || data[0] == step->expected));
	if (phase != step->phase || requested != step->requested
	    || data == NULL
	    || (requested != 0U && data[0] != step->expected))
		return false;
	if (!step->provider_ok)
		return false;
	*observation = step->observation;
	if (!observation->physical_unknown) {
		CHECK(script->physical_length + observation->accepted
		    <= sizeof(script->physical));
		if (script->physical_length + observation->accepted
		    <= sizeof(script->physical)
		    && observation->accepted != 0U)
			script->physical[script->physical_length++] = data[0];
	}
	return true;
}

static bool
scripted_text_device_close(void *context,
    struct yt_text_device_close_observation *observation)
{
	struct text_device_close_script *script = context;

	++script->calls;
	if (!script->provider_ok)
		return false;
	*observation = script->observation;
	return true;
}

static struct text_input_read_step *
text_input_read_add(struct text_input_read_script *script,
    const uint8_t *data, size_t accepted, bool carry, uint16_t dos_error,
    uint16_t basic_error, int64_t terminal_position)
{
	struct text_input_read_step *step;

	CHECK(script->length < YT_ARRAY_LEN(script->steps));
	if (script->length >= YT_ARRAY_LEN(script->steps))
		return NULL;
	step = &script->steps[script->length++];
	memset(step, 0, sizeof(*step));
	step->observation.accepted = accepted;
	step->observation.carry = carry;
	step->observation.dos_error = dos_error;
	step->observation.basic_error = basic_error;
	step->observation.terminal_position = terminal_position;
	step->provider_ok = true;
	if (data != NULL && accepted <= sizeof(step->data))
		memcpy(step->data, data, accepted);
	return step;
}

static bool
scripted_text_input_read(void *context, FILE *active_file, uint8_t *data,
    size_t requested, struct yt_text_input_read_observation *observation)
{
	struct text_input_read_script *script = context;
	struct text_input_read_step *step;

	CHECK(script->position < script->length);
	if (script->position >= script->length)
		return false;
	step = &script->steps[script->position++];
	CHECK(active_file != NULL && data != NULL
	    && requested == YT_TEXT_INPUT_BUFFER_SIZE);
	if (active_file == NULL || data == NULL
	    || requested != YT_TEXT_INPUT_BUFFER_SIZE)
		return false;
	if (!step->provider_ok)
		return false;
	*observation = step->observation;
	if (step->observation.accepted <= sizeof(step->data)
	    && step->observation.accepted != 0U)
		memcpy(data, step->data, step->observation.accepted);
	return true;
}

static bool
text_input_read_fixture(struct yt_text_input *input,
    struct text_input_read_script *script)
{
	yt_text_input_init(input);
	input->file = tmpfile();
	if (input->file == NULL)
		return false;
	(void)snprintf(input->path, sizeof(input->path), "%s",
	    "TEXT-INPUT-READ-ORACLE.DAT");
	input->last_open.outcome = YT_TEXT_OPEN_RETURNED;
	input->last_open.registered = true;
	input->last_open.handle_open = true;
	yt_text_input_set_read_provider(input, scripted_text_input_read, script);
	return true;
}

static struct file_kill_step *
file_kill_add(struct file_kill_script *script,
    enum yt_file_kill_operation operation, bool carry, uint16_t dos_error,
    bool open_collision, const char *selected)
{
	struct file_kill_step *step;

	CHECK(script->length < YT_ARRAY_LEN(script->steps));
	if (script->length >= YT_ARRAY_LEN(script->steps))
		return NULL;
	step = &script->steps[script->length++];
	memset(step, 0, sizeof(*step));
	step->operation = operation;
	step->observation.carry = carry;
	step->observation.dos_error = dos_error;
	step->observation.open_collision = open_collision;
	step->provider_ok = true;
	if (selected != NULL) {
		(void)snprintf(step->selected, sizeof(step->selected), "%s",
		    selected);
		step->replace_selected = true;
	}
	return step;
}

static void
file_kill_success_script(struct file_kill_script *script,
    uint16_t find_next_error)
{
	file_kill_add(script, YT_FILE_KILL_FIND_FIRST, false, 0U, false,
	    "yt.reg");
	file_kill_add(script, YT_FILE_KILL_CHECK_OPEN, false, 0U, false, NULL);
	file_kill_add(script, YT_FILE_KILL_DELETE, false, 0U, false, NULL);
	file_kill_add(script, YT_FILE_KILL_FIND_NEXT, true, find_next_error,
	    false, NULL);
}

static bool
scripted_file_kill(void *context, enum yt_file_kill_operation operation,
    const char *source, char *selected, size_t selected_size,
    struct yt_file_kill_observation *observation)
{
	struct file_kill_script *script = context;
	struct file_kill_step *step;

	CHECK(script->position < script->length);
	if (script->position >= script->length)
		return false;
	step = &script->steps[script->position++];
	CHECK(operation == step->operation && strcmp(source, "YT.REG") == 0
	    && selected_size == 512U);
	if (operation != step->operation || strcmp(source, "YT.REG") != 0
	    || selected_size != 512U)
		return false;
	if (!step->provider_ok)
		return false;
	if (step->replace_selected)
		(void)snprintf(selected, selected_size, "%s", step->selected);
	*observation = step->observation;
	return true;
}

static struct file_rename_step *
file_rename_add(struct file_rename_script *script,
    enum yt_file_rename_operation operation, bool carry, uint16_t dos_error,
    bool open_collision, const char *selected_old, const char *selected_new)
{
	struct file_rename_step *step;

	CHECK(script->length < YT_ARRAY_LEN(script->steps));
	if (script->length >= YT_ARRAY_LEN(script->steps))
		return NULL;
	step = &script->steps[script->length++];
	memset(step, 0, sizeof(*step));
	step->operation = operation;
	step->observation.carry = carry;
	step->observation.dos_error = dos_error;
	step->observation.open_collision = open_collision;
	step->provider_ok = true;
	if (selected_old != NULL) {
		(void)snprintf(step->selected_old, sizeof(step->selected_old),
		    "%s", selected_old);
		step->replace_old = true;
	}
	if (selected_new != NULL) {
		(void)snprintf(step->selected_new, sizeof(step->selected_new),
		    "%s", selected_new);
		step->replace_new = true;
	}
	return step;
}

static void
file_rename_success_script(struct file_rename_script *script)
{
	file_rename_add(script, YT_FILE_RENAME_PARSE_OLD, false, 0U, false,
	    "Temp", NULL);
	file_rename_add(script, YT_FILE_RENAME_PARSE_NEW, false, 0U, false,
	    NULL, "ytrmsg.dat");
	file_rename_add(script, YT_FILE_RENAME_CHECK_OLD_OPEN, false, 0U,
	    false, NULL, NULL);
	file_rename_add(script, YT_FILE_RENAME_CHECK_NEW_OPEN, false, 0U,
	    false, NULL, NULL);
	file_rename_add(script, YT_FILE_RENAME_RENAME, false, 0U, false,
	    NULL, NULL);
}

static bool
scripted_file_rename(void *context, enum yt_file_rename_operation operation,
    const char *old_source, const char *new_source, char *selected_old,
    size_t selected_old_size, char *selected_new, size_t selected_new_size,
    struct yt_file_rename_observation *observation)
{
	struct file_rename_script *script = context;
	struct file_rename_step *step;

	CHECK(script->position < script->length);
	if (script->position >= script->length)
		return false;
	step = &script->steps[script->position++];
	CHECK(operation == step->operation
	    && strcmp(old_source, "TEMP") == 0
	    && strcmp(new_source, "YTRMSG.DAT") == 0
	    && selected_old_size == 512U && selected_new_size == 512U);
	if (operation != step->operation || strcmp(old_source, "TEMP") != 0
	    || strcmp(new_source, "YTRMSG.DAT") != 0
	    || selected_old_size != 512U || selected_new_size != 512U)
		return false;
	if (!step->provider_ok)
		return false;
	if (step->replace_old)
		(void)snprintf(selected_old, selected_old_size, "%s",
		    step->selected_old);
	if (step->replace_new)
		(void)snprintf(selected_new, selected_new_size, "%s",
		    step->selected_new);
	*observation = step->observation;
	return true;
}

struct close_all_tape {
	int identifiers[8];
	int classes[8];
	size_t length;
	size_t fixed_count_at_call;
	unsigned fixed_calls;
};

struct close_all_entry {
	struct close_all_tape *tape;
	int identifier;
	bool succeeds;
};

struct close_all_fixed_entry {
	struct close_all_tape *tape;
	size_t *lazy_open_count;
};

static bool
scripted_close_all_method(void *context, int8_t file_class,
    struct yt_error *error)
{
	struct close_all_entry *entry = context;
	struct close_all_tape *tape = entry->tape;

	CHECK(tape->length < YT_ARRAY_LEN(tape->identifiers));
	if (tape->length < YT_ARRAY_LEN(tape->identifiers)) {
		tape->identifiers[tape->length] = entry->identifier;
		tape->classes[tape->length] = file_class;
		++tape->length;
	}
	if (!entry->succeeds && error != NULL) {
		error->status = YT_IO_ERROR;
		snprintf(error->operation, sizeof(error->operation),
		    "scripted CLOSE all method");
	}
	return entry->succeeds;
}

static void
scripted_close_all_fixed(void *context)
{
	struct close_all_fixed_entry *entry = context;

	entry->tape->fixed_count_at_call = *entry->lazy_open_count;
	++entry->tape->fixed_calls;
}

static void
database_lof_add(struct database_lof_script *script,
    enum yt_database_lof_operation operation, uint32_t restore_position,
    bool carry, uint16_t dos_error, int64_t terminal_position)
{
	struct database_lof_step *step;

	CHECK(script->length < YT_ARRAY_LEN(script->steps));
	if (script->length >= YT_ARRAY_LEN(script->steps))
		return;
	step = &script->steps[script->length++];
	memset(step, 0, sizeof(*step));
	step->operation = operation;
	step->restore_position = restore_position;
	step->observation.carry = carry;
	step->observation.dos_error = dos_error;
	step->observation.terminal_position = terminal_position;
	step->provider_ok = true;
}

static bool
scripted_database_lof(void *context, FILE *active_file,
    enum yt_database_lof_operation operation, uint32_t restore_position,
    struct yt_database_lof_observation *observation)
{
	struct database_lof_script *script = context;
	struct database_lof_step *step;

	CHECK(active_file != NULL && script->position < script->length);
	if (active_file == NULL || script->position >= script->length)
		return false;
	step = &script->steps[script->position++];
	CHECK(operation == step->operation
	    && restore_position == step->restore_position);
	if (operation != step->operation
	    || restore_position != step->restore_position
	    || !step->provider_ok)
		return false;
	*observation = step->observation;
	return true;
}

static void
test_database_random_open(void)
{
	static const uint8_t retry_accesses[] = {2U, 1U, 0U};
	struct database_open_script script;
	struct yt_database database;
	struct yt_error error;
	char path[160];
	char native_path[160];
	char missing_parent_path[192];
	FILE *file;
	unsigned dos_error;
	unsigned mapped_error;
	size_t index;

#ifdef _WIN32
	(void)snprintf(path, sizeof(path), "yt-open-oracle-%lu.dat",
	    (unsigned long)GetCurrentProcessId());
	(void)snprintf(native_path, sizeof(native_path), "yt-open-native-%lu.dat",
	    (unsigned long)GetCurrentProcessId());
	(void)snprintf(missing_parent_path, sizeof(missing_parent_path),
	    "yt-open-missing-%lu/child.dat",
	    (unsigned long)GetCurrentProcessId());
#else
	(void)snprintf(path, sizeof(path), "/tmp/yt-open-oracle-%ld.dat",
	    (long)getpid());
	(void)snprintf(native_path, sizeof(native_path),
	    "/tmp/yt-open-native-%ld.dat", (long)getpid());
	(void)snprintf(missing_parent_path, sizeof(missing_parent_path),
	    "/tmp/yt-open-missing-%ld/child.dat", (long)getpid());
#endif

	/* Existing disk file; IOCTL query carry is ignored and DL selects disk. */
	memset(&script, 0, sizeof(script));
	database_open_add_file(&script, 2U, 2U * YT_RECORD_SIZE);
	database_open_add_query(&script, 2U, true, 6U, false);
	yt_error_clear(&error);
	CHECK(yt_database_open_observed(&database, path, YT_OPEN_UPDATE_CREATE,
	    scripted_database_open, &script, &error));
	database_open_check_consumed(&script);
	CHECK(database.records == 2U && database.file != NULL
	    && database.orphaned_file == NULL
	    && database.last_open.outcome == YT_DATABASE_OPEN_RETURNED
	    && database.last_open.access_attempt_count == 1U
	    && database.last_open.access_attempts[0] == 2U
	    && database.last_open.operation_count == 2U
	    && !database.last_open.created && !database.last_open.device
	    && database.last_open.registered && database.last_open.handle_open
	    && ftell(database.file) == 0L);
	yt_database_close(&database);

	/* Access denied retries RANDOM access 2 -> 1 -> 0. */
	memset(&script, 0, sizeof(script));
	database_open_add_failure(&script, YT_DATABASE_OPEN_EXISTING, 2U, 0U,
	    5U, false);
	database_open_add_failure(&script, YT_DATABASE_OPEN_EXISTING, 1U, 0U,
	    5U, false);
	database_open_add_file(&script, 0U, YT_RECORD_SIZE);
	database_open_add_query(&script, 0U, false, 0U, false);
	CHECK(yt_database_open_observed(&database, path, YT_OPEN_UPDATE_CREATE,
	    scripted_database_open, &script, &error));
	database_open_check_consumed(&script);
	CHECK(database.last_open.access_attempt_count == 3U
	    && memcmp(database.last_open.access_attempts, retry_accesses,
	    sizeof(retry_accesses)) == 0
	    && database.last_open.operation_count == 4U);
	yt_database_close(&database);

	/* Exhausted access retries use the one exact extended-error result. */
	for (mapped_error = 70U; mapped_error <= 75U; mapped_error += 5U) {
		memset(&script, 0, sizeof(script));
		for (index = 0U; index < YT_ARRAY_LEN(retry_accesses); ++index)
			database_open_add_failure(&script,
			    YT_DATABASE_OPEN_EXISTING, retry_accesses[index], 0U,
			    5U, false);
		database_open_add(&script, YT_DATABASE_OPEN_EXTENDED_ERROR, 0U,
		    5U, false, 0U, (uint16_t)mapped_error, false, false,
		    false, false, 0U);
		yt_error_clear(&error);
		CHECK(!yt_database_open_observed(&database, path,
		    YT_OPEN_UPDATE_CREATE, scripted_database_open, &script,
		    &error) && error.status == YT_IO_ERROR);
		database_open_check_consumed(&script);
		CHECK(database.last_open.outcome == YT_DATABASE_OPEN_INITIAL_ERROR
		    && database.last_open.basic_error == mapped_error
		    && database.last_open.dos_error == 5U
		    && database.last_open.access_attempt_count == 3U
		    && database.last_open.operation_count == 4U
		    && !database.last_open.registered
		    && !database.last_open.handle_open);
		yt_database_close(&database);
	}

	/* Only initial DOS 2, 3, and 5 are special for RANDOM OPEN. */
	for (dos_error = 1U; dos_error <= 0xffU; ++dos_error) {
		if (dos_error == 2U || dos_error == 5U)
			continue;
		memset(&script, 0, sizeof(script));
		database_open_add_failure(&script, YT_DATABASE_OPEN_EXISTING, 2U,
		    0U, (uint16_t)dos_error, false);
		yt_error_clear(&error);
		CHECK(!yt_database_open_observed(&database, path,
		    YT_OPEN_UPDATE_CREATE, scripted_database_open, &script,
		    &error));
		database_open_check_consumed(&script);
		CHECK(database.last_open.outcome == YT_DATABASE_OPEN_INITIAL_ERROR
		    && database.last_open.basic_error
		    == (dos_error == 3U ? 76U : 75U)
		    && database.last_open.dos_error == dos_error);
		yt_database_close(&database);
	}

	/* Missing file: create, temporary close, reopen, query, return. */
	memset(&script, 0, sizeof(script));
	database_open_add_closed_missing_prefix(&script);
	database_open_add_file(&script, 2U, 3U * YT_RECORD_SIZE);
	database_open_add_query(&script, 2U, false, 0U, false);
	CHECK(yt_database_open_observed(&database, path, YT_OPEN_UPDATE_CREATE,
	    scripted_database_open, &script, &error));
	database_open_check_consumed(&script);
	CHECK(database.records == 3U && database.last_open.created
	    && database.last_open.temporary_close_attempted
	    && !database.last_open.temporary_close_retried
	    && database.last_open.access_attempt_count == 2U
	    && database.last_open.access_attempts[0] == 2U
	    && database.last_open.access_attempts[1] == 2U
	    && database.last_open.operation_count == 5U
	    && database.last_open.outcome == YT_DATABASE_OPEN_RETURNED);
	yt_database_close(&database);

	/* Every CREATE error byte: DOS 2 -> ERR53, DOS 5 -> extended, else 75. */
	for (dos_error = 1U; dos_error <= 0xffU; ++dos_error) {
		if (dos_error == 5U)
			continue;
		memset(&script, 0, sizeof(script));
		database_open_add_failure(&script, YT_DATABASE_OPEN_EXISTING, 2U,
		    0U, 2U, false);
		database_open_add_failure(&script, YT_DATABASE_OPEN_CREATE, 2U,
		    0U, (uint16_t)dos_error, false);
		yt_error_clear(&error);
		CHECK(!yt_database_open_observed(&database, path,
		    YT_OPEN_UPDATE_CREATE, scripted_database_open, &script,
		    &error));
		database_open_check_consumed(&script);
		CHECK(database.last_open.outcome == YT_DATABASE_OPEN_CREATE_ERROR
		    && database.last_open.basic_error
		    == (dos_error == 2U ? 53U : 75U)
		    && database.last_open.dos_error == dos_error);
		yt_database_close(&database);
	}
	for (mapped_error = 70U; mapped_error <= 75U; mapped_error += 5U) {
		memset(&script, 0, sizeof(script));
		database_open_add_failure(&script, YT_DATABASE_OPEN_EXISTING, 2U,
		    0U, 2U, false);
		database_open_add_failure(&script, YT_DATABASE_OPEN_CREATE, 2U,
		    0U, 5U, false);
		database_open_add(&script, YT_DATABASE_OPEN_EXTENDED_ERROR, 0U,
		    5U, false, 0U, (uint16_t)mapped_error, false, false,
		    false, false, 0U);
		CHECK(!yt_database_open_observed(&database, path,
		    YT_OPEN_UPDATE_CREATE, scripted_database_open, &script,
		    &error));
		database_open_check_consumed(&script);
		CHECK(database.last_open.outcome == YT_DATABASE_OPEN_CREATE_ERROR
		    && database.last_open.basic_error == mapped_error);
		yt_database_close(&database);
	}

	/* A failed temporary close is retried once; its result is ignored. */
	for (index = 0U; index < 3U; ++index) {
		memset(&script, 0, sizeof(script));
		database_open_add_missing_prefix(&script);
		if (index == 2U)
			database_open_add(&script, YT_DATABASE_OPEN_TEMP_CLOSE,
			    0U, 0U, true, 5U, 0U, false, false, false, true,
			    0U);
		else
			database_open_add_failure(&script,
			    YT_DATABASE_OPEN_TEMP_CLOSE, 0U, 0U, 5U, true);
		database_open_add(&script, YT_DATABASE_OPEN_TEMP_CLOSE, 0U,
		    5U, index != 1U, index != 1U ? 6U : 0U, 0U, false,
		    index == 0U, false, index == 1U, 0U);
		yt_error_clear(&error);
		CHECK(!yt_database_open_observed(&database, path,
		    YT_OPEN_UPDATE_CREATE, scripted_database_open, &script,
		    &error) && error.status == YT_IO_ERROR);
		database_open_check_consumed(&script);
		CHECK(database.last_open.outcome
		    == YT_DATABASE_OPEN_TEMP_CLOSE_ERROR
		    && database.last_open.basic_error == 70U
		    && database.last_open.dos_error == 5U
		    && database.last_open.temporary_close_retry_dos_error
		    == (index == 1U ? 0U : 6U)
		    && database.last_open.created
		    && database.last_open.temporary_close_attempted
		    && database.last_open.temporary_close_retried
		    && database.last_open.operation_count == 4U
		    && !database.last_open.registered
		    && database.last_open.handle_open == (index == 0U));
		yt_database_close(&database);
	}
	/* The ignored retry's complete DOS-error domain remains observable. */
	for (dos_error = 1U; dos_error <= 0xffU; ++dos_error) {
		memset(&script, 0, sizeof(script));
		database_open_add_missing_prefix(&script);
		database_open_add_failure(&script,
		    YT_DATABASE_OPEN_TEMP_CLOSE, 0U, 0U, 5U, true);
		database_open_add(&script, YT_DATABASE_OPEN_TEMP_CLOSE, 0U,
		    5U, true, (uint16_t)dos_error, 0U, false, false, false,
		    true, 0U);
		CHECK(!yt_database_open_observed(&database, path,
		    YT_OPEN_UPDATE_CREATE, scripted_database_open, &script,
		    &error));
		database_open_check_consumed(&script);
		CHECK(database.last_open.outcome
		    == YT_DATABASE_OPEN_TEMP_CLOSE_ERROR
		    && database.last_open.dos_error == 5U
		    && database.last_open.temporary_close_retry_dos_error
		    == dos_error
		    && !database.last_open.handle_open);
		yt_database_close(&database);
	}

	/* Reopen DOS 2 is ERR53; all other non-5 bytes, including 3, are 75. */
	for (dos_error = 1U; dos_error <= 0xffU; ++dos_error) {
		if (dos_error == 5U)
			continue;
		memset(&script, 0, sizeof(script));
		database_open_add_closed_missing_prefix(&script);
		database_open_add_failure(&script, YT_DATABASE_OPEN_EXISTING, 2U,
		    0U, (uint16_t)dos_error, false);
		yt_error_clear(&error);
		CHECK(!yt_database_open_observed(&database, path,
		    YT_OPEN_UPDATE_CREATE, scripted_database_open, &script,
		    &error));
		database_open_check_consumed(&script);
		CHECK(database.last_open.outcome == YT_DATABASE_OPEN_REOPEN_ERROR
		    && database.last_open.basic_error
		    == (dos_error == 2U ? 53U : 75U)
		    && database.last_open.dos_error == dos_error
		    && database.last_open.operation_count == 4U);
		yt_database_close(&database);
	}
	for (mapped_error = 70U; mapped_error <= 75U; mapped_error += 5U) {
		memset(&script, 0, sizeof(script));
		database_open_add_closed_missing_prefix(&script);
		for (index = 0U; index < YT_ARRAY_LEN(retry_accesses); ++index)
			database_open_add_failure(&script,
			    YT_DATABASE_OPEN_EXISTING, retry_accesses[index], 0U,
			    5U, false);
		database_open_add(&script, YT_DATABASE_OPEN_EXTENDED_ERROR, 0U,
		    5U, false, 0U, (uint16_t)mapped_error, false, false,
		    false, false, 0U);
		CHECK(!yt_database_open_observed(&database, path,
		    YT_OPEN_UPDATE_CREATE, scripted_database_open, &script,
		    &error));
		database_open_check_consumed(&script);
		CHECK(database.last_open.outcome == YT_DATABASE_OPEN_REOPEN_ERROR
		    && database.last_open.basic_error == mapped_error
		    && database.last_open.operation_count == 7U
		    && database.last_open.access_attempt_count == 4U
		    && database.last_open.access_attempts[0] == 2U
		    && memcmp(&database.last_open.access_attempts[1],
		    retry_accesses, sizeof(retry_accesses)) == 0);
		yt_database_close(&database);
	}
	/* Both access-denied ladders can coexist at the six-attempt bound. */
	memset(&script, 0, sizeof(script));
	database_open_add_failure(&script, YT_DATABASE_OPEN_EXISTING, 2U, 0U,
	    5U, false);
	database_open_add_failure(&script, YT_DATABASE_OPEN_EXISTING, 1U, 0U,
	    5U, false);
	database_open_add_failure(&script, YT_DATABASE_OPEN_EXISTING, 0U, 0U,
	    2U, false);
	database_open_add(&script, YT_DATABASE_OPEN_CREATE, 2U, 0U, false, 0U,
	    0U, false, true, true, false, 0U);
	database_open_add(&script, YT_DATABASE_OPEN_TEMP_CLOSE, 0U, 0U, false,
	    0U, 0U, false, false, false, true, 0U);
	for (index = 0U; index < YT_ARRAY_LEN(retry_accesses); ++index)
		database_open_add_failure(&script, YT_DATABASE_OPEN_EXISTING,
		    retry_accesses[index], 0U, 5U, false);
	database_open_add(&script, YT_DATABASE_OPEN_EXTENDED_ERROR, 0U, 5U,
	    false, 0U, 70U, false, false, false, false, 0U);
	CHECK(!yt_database_open_observed(&database, path,
	    YT_OPEN_UPDATE_CREATE, scripted_database_open, &script, &error));
	database_open_check_consumed(&script);
	CHECK(database.last_open.outcome == YT_DATABASE_OPEN_REOPEN_ERROR
	    && database.last_open.basic_error == 70U
	    && database.last_open.access_attempt_count == 6U
	    && memcmp(database.last_open.access_attempts, retry_accesses,
	    sizeof(retry_accesses)) == 0
	    && memcmp(&database.last_open.access_attempts[3], retry_accesses,
	    sizeof(retry_accesses)) == 0
	    && database.last_open.operation_count == 9U);
	yt_database_close(&database);

	/* Character device configuration retains the registered handle. */
	memset(&script, 0, sizeof(script));
	database_open_add_file(&script, 2U, 0U);
	database_open_add_query(&script, 2U, true, 6U, true);
	database_open_add(&script, YT_DATABASE_OPEN_CONFIGURE_DEVICE, 2U, 0U,
	    false, 0U, 0U, false, true, false, false, 0U);
	CHECK(yt_database_open_observed(&database, path, YT_OPEN_UPDATE_CREATE,
	    scripted_database_open, &script, &error));
	database_open_check_consumed(&script);
	CHECK(database.last_open.device && database.last_open.registered
	    && database.last_open.handle_open
	    && database.last_open.operation_count == 3U);
	yt_database_close(&database);
	for (dos_error = 1U; dos_error <= 0xffU; ++dos_error) {
		memset(&script, 0, sizeof(script));
		database_open_add_file(&script, 2U, 0U);
		database_open_add_query(&script, 2U, false, 0U, true);
		database_open_add_failure(&script,
		    YT_DATABASE_OPEN_CONFIGURE_DEVICE, 2U, 0U,
		    (uint16_t)dos_error, true);
		yt_error_clear(&error);
		CHECK(!yt_database_open_observed(&database, path,
		    YT_OPEN_UPDATE_CREATE, scripted_database_open, &script,
		    &error) && error.status == YT_IO_ERROR);
		database_open_check_consumed(&script);
		CHECK(database.last_open.outcome == YT_DATABASE_OPEN_DEVICE_ERROR
		    && database.last_open.basic_error == 57U
		    && database.last_open.dos_error == dos_error
		    && database.last_open.device && database.last_open.registered
		    && database.last_open.handle_open);
		yt_database_close(&database);
	}

	/* Malformed provider observations are typed adapter failures, not DOS. */
	memset(&script, 0, sizeof(script));
	database_open_add_file(&script, 2U, 0U);
	script.steps[0].observation.dos_error = 1U;
	yt_error_clear(&error);
	CHECK(!yt_database_open_observed(&database, path, YT_OPEN_UPDATE_CREATE,
	    scripted_database_open, &script, &error));
	database_open_check_consumed(&script);
	CHECK(database.last_open.outcome == YT_DATABASE_OPEN_PROVIDER_ERROR
	    && !database.last_open.registered && database.last_open.handle_open
	    && database.orphaned_file != NULL);
	yt_database_close(&database);
	memset(&script, 0, sizeof(script));
	database_open_add_failure(&script, YT_DATABASE_OPEN_EXISTING, 2U, 0U,
	    2U, false);
	script.steps[0].provider_ok = false;
	yt_error_clear(&error);
	CHECK(!yt_database_open_observed(&database, path, YT_OPEN_UPDATE_CREATE,
	    scripted_database_open, &script, &error));
	database_open_check_consumed(&script);
	CHECK(database.last_open.outcome == YT_DATABASE_OPEN_PROVIDER_ERROR
	    && database.last_open.operation_count == 1U
	    && !database.last_open.registered
	    && !database.last_open.handle_open);
	yt_database_close(&database);

	/* The host adapter performs the same existing and create/reopen paths. */
	yt_error_clear(&error);
	CHECK(yt_file_delete(native_path, true, &error));
	file = fopen(native_path, "wb");
	CHECK(file != NULL);
	if (file != NULL) {
		for (index = 0U; index < 2U * YT_RECORD_SIZE; ++index)
			CHECK(fputc((int)(index & 0xffU), file) != EOF);
		CHECK(fclose(file) == 0);
	}
	CHECK(yt_database_open(&database, native_path, YT_OPEN_UPDATE_CREATE,
	    &error));
	CHECK(database.records == 2U && !database.last_open.created
	    && database.last_open.operation_count == 2U
	    && database.last_open.access_attempt_count == 1U
	    && ftell(database.file) == 0L);
	yt_database_close(&database);
	CHECK(yt_file_delete(native_path, false, &error));
	CHECK(yt_database_open(&database, native_path, YT_OPEN_UPDATE_CREATE,
	    &error));
	CHECK(database.records == 0U && database.last_open.created
	    && database.last_open.operation_count == 5U
	    && database.last_open.access_attempt_count == 2U
	    && ftell(database.file) == 0L);
	yt_database_close(&database);
	CHECK(yt_file_delete(native_path, false, &error));
	yt_error_clear(&error);
	CHECK(!yt_database_open(&database, missing_parent_path,
	    YT_OPEN_UPDATE_CREATE, &error) && error.status == YT_IO_ERROR
	    && database.last_open.outcome == YT_DATABASE_OPEN_INITIAL_ERROR
	    && database.last_open.dos_error == 3U
	    && database.last_open.basic_error == 76U
	    && database.last_open.operation_count == 1U
	    && database.last_open.access_attempt_count == 1U
	    && database.file == NULL && database.orphaned_file == NULL);
}

static void
test_file_kill(void)
{
	struct file_kill_script script;
	struct yt_file_kill_result result;
	struct yt_error error;
	char directory[256];
	char actual[320];
	char requested[320];
	FILE *file;
	unsigned dos_error;
	unsigned ordinal;

	/* One rooted literal owns find/check/delete/ignored FindNext carry. */
	memset(&script, 0, sizeof(script));
	file_kill_success_script(&script, 18U);
	yt_error_clear(&error);
	CHECK(yt_file_kill_observed("YT.REG", scripted_file_kill, &script,
	    &result, &error)
	    && result.outcome == YT_FILE_KILL_RETURNED
	    && result.operation_count == 4U && result.deleted_count == 1U
	    && result.found && result.checked_open && result.deleted
	    && result.find_next_attempted
	    && strcmp(result.selected_path, "yt.reg") == 0
	    && result.find_next_dos_error == 18U
	    && result.dos_error == 0U && result.basic_error == 0U
	    && script.position == script.length);

	/* FindFirst maps every DOS error byte, including 5, to ERR53. */
	for (dos_error = 1U; dos_error <= 0xffU; ++dos_error) {
		memset(&script, 0, sizeof(script));
		file_kill_add(&script, YT_FILE_KILL_FIND_FIRST, true,
		    (uint16_t)dos_error, false, NULL);
		yt_error_clear(&error);
		CHECK(!yt_file_kill_observed("YT.REG", scripted_file_kill,
		    &script, &result, &error)
		    && result.outcome == YT_FILE_KILL_FIND_ERROR
		    && result.failed_operation == YT_FILE_KILL_FIND_FIRST
		    && result.operation_count == 1U
		    && result.dos_error == dos_error
		    && result.basic_error == 53U
		    && !result.found && !result.deleted
		    && error.status == YT_NOT_FOUND
		    && script.position == script.length);
	}

	/* Delete maps only DOS error 5 to ERR75; every other byte is ERR53. */
	for (dos_error = 1U; dos_error <= 0xffU; ++dos_error) {
		memset(&script, 0, sizeof(script));
		file_kill_add(&script, YT_FILE_KILL_FIND_FIRST, false, 0U,
		    false, "yt.reg");
		file_kill_add(&script, YT_FILE_KILL_CHECK_OPEN, false, 0U,
		    false, NULL);
		file_kill_add(&script, YT_FILE_KILL_DELETE, true,
		    (uint16_t)dos_error, false, NULL);
		CHECK(!yt_file_kill_observed("YT.REG", scripted_file_kill,
		    &script, &result, &error)
		    && result.outcome == YT_FILE_KILL_DELETE_ERROR
		    && result.failed_operation == YT_FILE_KILL_DELETE
		    && result.operation_count == 3U
		    && result.dos_error == dos_error
		    && result.basic_error == (dos_error == 5U ? 75U : 53U)
		    && result.found && result.checked_open && !result.deleted
		    && error.status == YT_IO_ERROR
		    && script.position == script.length);
	}

	/* FindNext carry ignores its complete DOS-byte domain after deletion. */
	for (dos_error = 1U; dos_error <= 0xffU; ++dos_error) {
		memset(&script, 0, sizeof(script));
		file_kill_success_script(&script, (uint16_t)dos_error);
		CHECK(yt_file_kill_observed("YT.REG", scripted_file_kill,
		    &script, &result, &error)
		    && result.outcome == YT_FILE_KILL_RETURNED
		    && result.operation_count == 4U && result.deleted_count == 1U
		    && result.deleted && result.find_next_attempted
		    && result.find_next_dos_error == dos_error
		    && result.dos_error == 0U && result.basic_error == 0U
		    && script.position == script.length);
	}

	/* A matching live control enters ERR55 before delete. */
	memset(&script, 0, sizeof(script));
	file_kill_add(&script, YT_FILE_KILL_FIND_FIRST, false, 0U, false,
	    "yt.reg");
	file_kill_add(&script, YT_FILE_KILL_CHECK_OPEN, false, 0U, true, NULL);
	CHECK(!yt_file_kill_observed("YT.REG", scripted_file_kill, &script,
	    &result, &error)
	    && result.outcome == YT_FILE_KILL_OPEN_ERROR
	    && result.failed_operation == YT_FILE_KILL_CHECK_OPEN
	    && result.operation_count == 2U && result.basic_error == 55U
	    && result.found && result.checked_open && !result.deleted
	    && script.position == script.length);

	/* Provider rejection at every ordinal preserves its completed prefix. */
	for (ordinal = 0U; ordinal < 4U; ++ordinal) {
		memset(&script, 0, sizeof(script));
		file_kill_success_script(&script, 18U);
		script.length = ordinal + 1U;
		script.steps[ordinal].provider_ok = false;
		CHECK(!yt_file_kill_observed("YT.REG", scripted_file_kill,
		    &script, &result, &error)
		    && result.outcome == YT_FILE_KILL_PROVIDER_ERROR
		    && result.failed_operation
		    == (enum yt_file_kill_operation)ordinal
		    && result.operation_count == ordinal + 1U
		    && result.found == (ordinal > 0U)
		    && result.checked_open == (ordinal > 1U)
		    && result.deleted == (ordinal > 2U)
		    && result.deleted_count == (ordinal > 2U ? 1U : 0U)
		    && script.position == script.length);
	}

	/* Malformed result shapes cannot be mistaken for DOS branches. */
	for (ordinal = 0U; ordinal < 8U; ++ordinal) {
		memset(&script, 0, sizeof(script));
		file_kill_success_script(&script, 18U);
		switch (ordinal) {
		case 0U:
			script.steps[0].replace_selected = false;
			break;
		case 1U:
			script.steps[0].observation.carry = true;
			script.steps[0].observation.dos_error = 2U;
			break;
		case 2U:
			script.steps[0].observation.dos_error = 2U;
			break;
		case 3U:
			script.steps[1].observation.carry = true;
			script.steps[1].observation.dos_error = 1U;
			break;
		case 4U:
			(void)snprintf(script.steps[1].selected,
			    sizeof(script.steps[1].selected), "%s", "other.reg");
			script.steps[1].replace_selected = true;
			break;
		case 5U:
			script.steps[2].observation.open_collision = true;
			break;
		case 6U:
			script.steps[2].observation.dos_error = 1U;
			break;
		default:
			script.steps[3].observation.carry = false;
			script.steps[3].observation.dos_error = 0U;
			break;
		}
		CHECK(!yt_file_kill_observed("YT.REG", scripted_file_kill,
		    &script, &result, &error)
		    && result.outcome == YT_FILE_KILL_PROVIDER_ERROR);
	}

	/* The host adapter resolves case, removes the literal, and rejects missing. */
#ifdef _WIN32
	snprintf(directory, sizeof(directory), "yt-file-kill-%lu",
	    (unsigned long)GetCurrentProcessId());
#else
	snprintf(directory, sizeof(directory), "/tmp/yt-file-kill-%ld",
	    (long)getpid());
#endif
	(void)mkdir_one(directory);
	snprintf(actual, sizeof(actual), "%s/yt.reg", directory);
	snprintf(requested, sizeof(requested), "%s/YT.REG", directory);
	file = fopen(actual, "wb");
	CHECK(file != NULL);
	if (file != NULL) {
		CHECK(fwrite("registration", 1U, 12U, file) == 12U);
		CHECK(fclose(file) == 0);
	}
	CHECK(yt_file_kill(requested, &result, &error)
	    && result.outcome == YT_FILE_KILL_RETURNED
	    && result.operation_count == 4U && result.deleted_count == 1U
	    && strcmp(result.selected_path, actual) == 0);
	file = fopen(actual, "rb");
	CHECK(file == NULL);
	if (file != NULL)
		(void)fclose(file);
	yt_error_clear(&error);
	CHECK(!yt_file_kill(requested, &result, &error)
	    && result.outcome == YT_FILE_KILL_FIND_ERROR
	    && result.basic_error == 53U && error.status == YT_NOT_FOUND);
#ifdef _WIN32
	_rmdir(directory);
#else
	rmdir(directory);
#endif
}

static void
test_file_rename(void)
{
	struct file_rename_script script;
	struct yt_file_rename_result result;
	struct yt_error error;
	char directory[256];
	char old_actual[320];
	char old_requested[320];
	char new_path[320];
	char missing_old[320];
	char missing_new[320];
	char bytes[6];
	FILE *file;
	unsigned dos_error;
	unsigned ordinal;

	/* The rooted NAME tape parses and checks both names before one rename. */
	memset(&script, 0, sizeof(script));
	file_rename_success_script(&script);
	yt_error_clear(&error);
	CHECK(yt_file_rename_observed("TEMP", "YTRMSG.DAT",
	    scripted_file_rename, &script, &result, &error)
	    && result.outcome == YT_FILE_RENAME_RETURNED
	    && result.operation_count == 5U
	    && result.old_parsed && result.new_parsed
	    && result.old_checked_open && result.new_checked_open
	    && result.renamed && result.dos_error == 0U
	    && result.basic_error == 0U
	    && strcmp(result.selected_old, "Temp") == 0
	    && strcmp(result.selected_new, "ytrmsg.dat") == 0
	    && script.position == script.length);

	/* The physical classifier consumes the complete DOS error-byte domain. */
	for (dos_error = 1U; dos_error <= 0xffU; ++dos_error) {
		uint16_t expected = dos_error == 2U ? 53U
		    : dos_error == 3U ? 76U
		    : dos_error == 5U ? 58U : 5U;

		memset(&script, 0, sizeof(script));
		file_rename_success_script(&script);
		script.steps[4].observation.carry = true;
		script.steps[4].observation.dos_error = (uint16_t)dos_error;
		CHECK(!yt_file_rename_observed("TEMP", "YTRMSG.DAT",
		    scripted_file_rename, &script, &result, &error)
		    && result.outcome == YT_FILE_RENAME_DOS_ERROR
		    && result.failed_operation == YT_FILE_RENAME_RENAME
		    && result.operation_count == 5U
		    && result.dos_error == dos_error
		    && result.basic_error == expected
		    && result.old_parsed && result.new_parsed
		    && result.old_checked_open && result.new_checked_open
		    && !result.renamed
		    && script.position == script.length);
	}

	/* Either live-name collision enters the same ERR55 terminal pre-rename. */
	for (ordinal = 2U; ordinal <= 3U; ++ordinal) {
		memset(&script, 0, sizeof(script));
		file_rename_success_script(&script);
		script.length = ordinal + 1U;
		script.steps[ordinal].observation.open_collision = true;
		CHECK(!yt_file_rename_observed("TEMP", "YTRMSG.DAT",
		    scripted_file_rename, &script, &result, &error)
		    && result.outcome == (ordinal == 2U
		    ? YT_FILE_RENAME_OLD_OPEN_ERROR
		    : YT_FILE_RENAME_NEW_OPEN_ERROR)
		    && result.failed_operation
		    == (enum yt_file_rename_operation)ordinal
		    && result.basic_error == 55U && !result.renamed
		    && script.position == script.length);
	}

	/* A rooted parser failure is ERR64 and suppresses every later operation. */
	for (ordinal = 0U; ordinal <= 1U; ++ordinal) {
		memset(&script, 0, sizeof(script));
		file_rename_success_script(&script);
		script.length = ordinal + 1U;
		script.steps[ordinal].observation.path_error = true;
		if (ordinal == 0U)
			script.steps[ordinal].replace_old = false;
		else
			script.steps[ordinal].replace_new = false;
		CHECK(!yt_file_rename_observed("TEMP", "YTRMSG.DAT",
		    scripted_file_rename, &script, &result, &error)
		    && result.outcome == (ordinal == 0U
		    ? YT_FILE_RENAME_OLD_PATH_ERROR
		    : YT_FILE_RENAME_NEW_PATH_ERROR)
		    && result.basic_error == 64U
		    && result.operation_count == ordinal + 1U
		    && script.position == script.length);
	}

	/* Provider rejection at each ordinal retains the completed prefix. */
	for (ordinal = 0U; ordinal < 5U; ++ordinal) {
		memset(&script, 0, sizeof(script));
		file_rename_success_script(&script);
		script.length = ordinal + 1U;
		script.steps[ordinal].provider_ok = false;
		CHECK(!yt_file_rename_observed("TEMP", "YTRMSG.DAT",
		    scripted_file_rename, &script, &result, &error)
		    && result.outcome == YT_FILE_RENAME_PROVIDER_ERROR
		    && result.failed_operation
		    == (enum yt_file_rename_operation)ordinal
		    && result.operation_count == ordinal + 1U
		    && result.old_parsed == (ordinal > 0U)
		    && result.new_parsed == (ordinal > 1U)
		    && result.old_checked_open == (ordinal > 2U)
		    && result.new_checked_open == (ordinal > 3U)
		    && !result.renamed
		    && script.position == script.length);
	}

	/* Malformed observation shapes cannot alias a canonical terminal. */
	for (ordinal = 0U; ordinal < 8U; ++ordinal) {
		memset(&script, 0, sizeof(script));
		file_rename_success_script(&script);
		switch (ordinal) {
		case 0U:
			script.steps[0].replace_old = false;
			break;
		case 1U:
			script.steps[0].observation.dos_error = 1U;
			break;
		case 2U:
			(void)snprintf(script.steps[1].selected_old,
			    sizeof(script.steps[1].selected_old), "%s", "Other");
			script.steps[1].replace_old = true;
			break;
		case 3U:
			script.steps[1].replace_new = false;
			break;
		case 4U:
			script.steps[2].observation.carry = true;
			script.steps[2].observation.dos_error = 1U;
			break;
		case 5U:
			(void)snprintf(script.steps[3].selected_new,
			    sizeof(script.steps[3].selected_new), "%s", "Other");
			script.steps[3].replace_new = true;
			break;
		case 6U:
			script.steps[4].observation.open_collision = true;
			break;
		default:
			script.steps[4].observation.dos_error = 1U;
			break;
		}
		CHECK(!yt_file_rename_observed("TEMP", "YTRMSG.DAT",
		    scripted_file_rename, &script, &result, &error)
		    && result.outcome == YT_FILE_RENAME_PROVIDER_ERROR);
	}

	/* Native DOS-like behavior resolves source case and never replaces a target. */
#ifdef _WIN32
	snprintf(directory, sizeof(directory), "yt-file-rename-%lu",
	    (unsigned long)GetCurrentProcessId());
#else
	snprintf(directory, sizeof(directory), "/tmp/yt-file-rename-%ld",
	    (long)getpid());
#endif
	(void)mkdir_one(directory);
	snprintf(old_actual, sizeof(old_actual), "%s/Temp", directory);
	snprintf(old_requested, sizeof(old_requested), "%s/TEMP", directory);
	snprintf(new_path, sizeof(new_path), "%s/ytrmsg.dat", directory);
	snprintf(missing_old, sizeof(missing_old), "%s/MISSING", directory);
	snprintf(missing_new, sizeof(missing_new), "%s/absent/name", directory);
	file = fopen(old_actual, "wb");
	CHECK(file != NULL);
	if (file != NULL) {
		CHECK(fwrite("source", 1U, 6U, file) == 6U);
		CHECK(fclose(file) == 0);
	}
	CHECK(yt_file_rename_observed(old_requested, new_path, NULL, NULL,
	    &result, &error)
	    && result.outcome == YT_FILE_RENAME_RETURNED && result.renamed
	    && strcmp(result.selected_old, old_actual) == 0
	    && strcmp(result.selected_new, new_path) == 0);
	file = fopen(new_path, "rb");
	CHECK(file != NULL);
	if (file != NULL) {
		CHECK(fread(bytes, 1U, sizeof(bytes), file) == sizeof(bytes)
		    && memcmp(bytes, "source", sizeof(bytes)) == 0);
		CHECK(fclose(file) == 0);
	}

	file = fopen(old_actual, "wb");
	CHECK(file != NULL);
	if (file != NULL)
		CHECK(fclose(file) == 0);
	CHECK(!yt_file_rename_observed(old_requested, new_path, NULL, NULL,
	    &result, &error)
	    && result.outcome == YT_FILE_RENAME_DOS_ERROR
	    && result.dos_error == 5U && result.basic_error == 58U);
	CHECK(!yt_file_rename_observed(missing_old, new_path, NULL, NULL,
	    &result, &error)
	    && result.dos_error == 5U && result.basic_error == 58U);
	(void)remove(new_path);
	CHECK(!yt_file_rename_observed(missing_old, new_path, NULL, NULL,
	    &result, &error)
	    && result.dos_error == 2U && result.basic_error == 53U);
	CHECK(!yt_file_rename_observed(old_requested, missing_new, NULL, NULL,
	    &result, &error)
	    && result.dos_error == 3U && result.basic_error == 76U);
	(void)remove(old_actual);
#ifdef _WIN32
	_rmdir(directory);
#else
	rmdir(directory);
#endif
}

static void
test_close_all_registry(void)
{
	struct close_all_tape tape;
	struct close_all_entry entries[3];
	struct yt_close_all_control controls[5];
	struct close_all_fixed_entry fixed_entry;
	struct yt_close_all_fixed_control fixed;
	struct yt_close_all_result result;
	struct yt_error error;
	struct yt_database databases[2];
	struct database_public_close_script close_scripts[2];
	struct yt_close_all_control database_controls[2];
	size_t lazy_open_count;
	static const int expected_identifiers[] = {50, 30, 10};
	static const int expected_classes[] = {-1, -4, 0};

	memset(&tape, 0, sizeof(tape));
	yt_error_clear(&error);
	CHECK(yt_close_all_run(NULL, 0U, NULL, &result, &error)
	    && result.scanned_count == 0U && result.attempt_count == 0U
	    && result.completed_count == 0U && result.failed_index == SIZE_MAX
	    && !result.failed && !result.fixed_was_open
	    && !result.fixed_close_attempted && result.returned);

	entries[0] = (struct close_all_entry){&tape, 10, true};
	entries[1] = (struct close_all_entry){&tape, 30, true};
	entries[2] = (struct close_all_entry){&tape, 50, true};
	controls[0] = (struct yt_close_all_control){
		YT_CLOSE_ALL_HEAP_FILE, 0, scripted_close_all_method, &entries[0]};
	controls[1] = (struct yt_close_all_control){
		YT_CLOSE_ALL_HEAP_NON_FILE, 0, NULL, NULL};
	controls[2] = (struct yt_close_all_control){
		YT_CLOSE_ALL_HEAP_FILE, -4, scripted_close_all_method, &entries[1]};
	controls[3] = (struct yt_close_all_control){
		YT_CLOSE_ALL_HEAP_FREE, 0, NULL, NULL};
	controls[4] = (struct yt_close_all_control){
		YT_CLOSE_ALL_HEAP_FILE, -1, scripted_close_all_method, &entries[2]};
	lazy_open_count = 2U;
	fixed_entry = (struct close_all_fixed_entry){&tape, &lazy_open_count};
	fixed = (struct yt_close_all_fixed_control){
		&lazy_open_count, scripted_close_all_fixed, &fixed_entry};
	CHECK(yt_close_all_run(controls, YT_ARRAY_LEN(controls), &fixed,
	    &result, &error));
	CHECK(tape.length == YT_ARRAY_LEN(expected_identifiers)
	    && memcmp(tape.identifiers, expected_identifiers,
	    sizeof(expected_identifiers)) == 0
	    && memcmp(tape.classes, expected_classes,
	    sizeof(expected_classes)) == 0);
	CHECK(result.scanned_count == YT_ARRAY_LEN(controls)
	    && result.attempt_count == 3U && result.completed_count == 3U
	    && result.failed_index == SIZE_MAX && !result.failed
	    && result.fixed_was_open && result.fixed_close_attempted
	    && result.returned && lazy_open_count == 0U
	    && tape.fixed_calls == 1U && tape.fixed_count_at_call == 0U);

	/* A nonlocal file-method failure commits only higher controls. */
	memset(&tape, 0, sizeof(tape));
	entries[1].succeeds = false;
	lazy_open_count = 2U;
	yt_error_clear(&error);
	CHECK(!yt_close_all_run(controls, YT_ARRAY_LEN(controls), &fixed,
	    &result, &error)
	    && error.status == YT_IO_ERROR
	    && strcmp(error.operation, "scripted CLOSE all method") == 0);
	CHECK(tape.length == 2U && tape.identifiers[0] == 50
	    && tape.identifiers[1] == 30
	    && result.scanned_count == 3U && result.attempt_count == 2U
	    && result.completed_count == 1U && result.failed_index == 2U
	    && result.failed && !result.fixed_was_open
	    && !result.fixed_close_attempted && !result.returned
	    && lazy_open_count == 2U && tape.fixed_calls == 0U);

	/* A zero lazy count skips the stable fixed control. */
	memset(&tape, 0, sizeof(tape));
	entries[1].succeeds = true;
	lazy_open_count = 0U;
	CHECK(yt_close_all_run(NULL, 0U, &fixed, &result, &error)
	    && result.returned && !result.fixed_was_open
	    && !result.fixed_close_attempted && tape.fixed_calls == 0U);

	/* Ordinary controls compose the exact per-file method in registry order. */
	memset(close_scripts, 0, sizeof(close_scripts));
	database_close_add(&close_scripts[0], false, 0U, false, true, true);
	database_close_add(&close_scripts[1], false, 0U, false, true, true);
	CHECK(database_close_fixture(&databases[0], false, &close_scripts[0])
	    && database_close_fixture(&databases[1], false, &close_scripts[1]));
	database_controls[0] = (struct yt_close_all_control){
		YT_CLOSE_ALL_HEAP_FILE, 0, yt_database_close_all_method,
		&databases[0]};
	database_controls[1] = (struct yt_close_all_control){
		YT_CLOSE_ALL_HEAP_FILE, 0, yt_database_close_all_method,
		&databases[1]};
	CHECK(yt_close_all_run(database_controls,
	    YT_ARRAY_LEN(database_controls), NULL, &result, &error)
	    && close_scripts[0].position == close_scripts[0].length
	    && close_scripts[1].position == close_scripts[1].length
	    && databases[0].last_close.close_all
	    && databases[1].last_close.close_all
	    && databases[0].file == NULL && databases[1].file == NULL);
	yt_database_close(&databases[0]);
	yt_database_close(&databases[1]);

	/* Failure of the high control leaves the lower ordinary file untouched. */
	memset(close_scripts, 0, sizeof(close_scripts));
	database_close_add(&close_scripts[0], false, 0U, false, true, true);
	database_close_add(&close_scripts[1], true, 5U, true, true, false);
	database_close_add(&close_scripts[1], false, 0U, false, true, true);
	CHECK(database_close_fixture(&databases[0], false, &close_scripts[0])
	    && database_close_fixture(&databases[1], false, &close_scripts[1]));
	yt_error_clear(&error);
	CHECK(!yt_close_all_run(database_controls,
	    YT_ARRAY_LEN(database_controls), NULL, &result, &error)
	    && result.failed_index == 1U && result.scanned_count == 1U
	    && result.attempt_count == 1U && result.completed_count == 0U
	    && close_scripts[0].position == 0U
	    && close_scripts[1].position == close_scripts[1].length
	    && databases[0].file != NULL
	    && databases[0].last_close.outcome == YT_DATABASE_CLOSE_NONE
	    && databases[1].file == NULL
	    && databases[1].last_close.outcome == YT_DATABASE_CLOSE_DISK_ERROR
	    && strcmp(error.operation, "CLOSE all") == 0);
	yt_database_close(&databases[0]);
	yt_database_close(&databases[1]);

	/* Malformed registries are rejected before any control is touched. */
	memset(&tape, 0, sizeof(tape));
	controls[0].method = NULL;
	yt_error_clear(&error);
	CHECK(!yt_close_all_run(controls, YT_ARRAY_LEN(controls), NULL,
	    &result, &error) && error.status == YT_INVALID
	    && strcmp(error.operation, "CLOSE all registry") == 0
	    && tape.length == 0U && result.scanned_count == 0U
	    && result.failed_index == SIZE_MAX && !result.returned);
	yt_error_clear(&error);
	CHECK(!yt_close_all_run(NULL, 1U, NULL, &result, &error)
	    && error.status == YT_INVALID && result.scanned_count == 0U);
}

static void
test_database_random_close(void)
{
	struct database_public_close_script script;
	struct yt_database database;
	struct yt_error error;
	unsigned device;
	unsigned dos_error;

	/* Missing file numbers return without any external operation. */
	memset(&database, 0, sizeof(database));
	memset(&script, 0, sizeof(script));
	database.last_open.device = true; /* A missing entry has no live class. */
	yt_database_set_close_provider(&database, scripted_database_public_close,
	    &script);
	CHECK(yt_database_random_close(&database, &error)
	    && database.last_close.outcome == YT_DATABASE_CLOSE_RETURNED
	    && database.last_close.missing
	    && database.last_close.attempt_count == 0U
	    && !database.last_close.device
	    && !database.last_close.registered
	    && !database.last_close.handle_open
	    && script.position == 0U);
	database.orphaned_file = tmpfile();
	CHECK(database.orphaned_file != NULL);
	if (database.orphaned_file != NULL) {
		CHECK(yt_database_random_close(&database, &error)
		    && database.last_close.missing
		    && database.last_close.attempt_count == 0U
		    && !database.last_close.registered
		    && database.last_close.handle_open
		    && database.orphaned_file != NULL
		    && script.position == 0U);
	}
	yt_database_close(&database);

	/* CLOSE-all has a distinct public identity over the same file method. */
	memset(&database, 0, sizeof(database));
	memset(&script, 0, sizeof(script));
	yt_database_set_close_provider(&database, scripted_database_public_close,
	    &script);
	yt_error_clear(&error);
	CHECK(yt_database_close_all_single(&database, &error)
	    && database.last_close.outcome == YT_DATABASE_CLOSE_RETURNED
	    && database.last_close.close_all && !database.last_close.missing
	    && database.last_close.attempt_count == 0U
	    && script.position == 0U);
	memset(&script, 0, sizeof(script));
	database_close_add(&script, false, 0U, false, true, true);
	CHECK(database_close_fixture(&database, false, &script));
	yt_error_clear(&error);
	CHECK(yt_database_close_all_single(&database, &error)
	    && database.last_close.close_all && !database.last_close.missing
	    && database.last_close.outcome == YT_DATABASE_CLOSE_RETURNED
	    && database.last_close.attempt_count == 1U
	    && database.file == NULL && script.position == script.length);
	yt_database_close(&database);
	memset(&script, 0, sizeof(script));
	database_close_add(&script, true, 5U, true, true, false);
	database_close_add(&script, false, 0U, false, true, true);
	CHECK(database_close_fixture(&database, false, &script));
	yt_error_clear(&error);
	CHECK(!yt_database_close_all_single(&database, &error)
	    && database.last_close.close_all
	    && database.last_close.outcome == YT_DATABASE_CLOSE_DISK_ERROR
	    && database.last_close.basic_error == 70U
	    && database.last_close.dos_error == 5U
	    && strcmp(error.operation, "CLOSE all") == 0);
	yt_database_close(&database);

	/* One clear-carry CLOSE unregisters ordinary and device random files. */
	for (device = 0U; device < 2U; ++device) {
		memset(&script, 0, sizeof(script));
		database_close_add(&script, false, 0U, false, true, true);
		CHECK(database_close_fixture(&database, device != 0U, &script));
		yt_error_clear(&error);
		CHECK(yt_database_random_close(&database, &error));
		CHECK(script.position == script.length
		    && database.file == NULL && database.orphaned_file == NULL
		    && database.records == 0U
		    && database.last_close.outcome
		    == YT_DATABASE_CLOSE_RETURNED
		    && database.last_close.attempt_count == 1U
		    && database.last_close.device == (device != 0U)
		    && !database.last_close.missing
		    && !database.last_close.retry_attempted
		    && !database.last_close.registered
		    && !database.last_close.handle_open);
		yt_database_close(&database);
	}

	/* Every first DOS error is retained; the ignored retry may succeed. */
	for (device = 0U; device < 2U; ++device) {
		for (dos_error = 1U; dos_error <= 0xffU; ++dos_error) {
			memset(&script, 0, sizeof(script));
			database_close_add(&script, true, (uint16_t)dos_error,
			    true, true, false);
			database_close_add(&script, false, 0U, false, true, true);
			CHECK(database_close_fixture(&database, device != 0U,
			    &script));
			yt_error_clear(&error);
			CHECK(!yt_database_random_close(&database, &error)
			    && error.status == YT_IO_ERROR
			    && script.position == script.length
			    && database.file == NULL
			    && database.orphaned_file == NULL
			    && database.records == 0U
			    && database.last_close.outcome == (device != 0U
			    ? YT_DATABASE_CLOSE_DEVICE_ERROR
			    : YT_DATABASE_CLOSE_DISK_ERROR)
			    && database.last_close.basic_error
			    == (device != 0U ? 57U : 70U)
			    && database.last_close.dos_error == dos_error
			    && database.last_close.retry_dos_error == 0U
			    && database.last_close.attempt_count == 2U
			    && database.last_close.retry_attempted
			    && !database.last_close.registered
			    && !database.last_close.handle_open);
			yt_database_close(&database);
		}
	}

	/* Every retry carry result is ignored and may retain the host handle. */
	for (device = 0U; device < 2U; ++device) {
		for (dos_error = 1U; dos_error <= 0xffU; ++dos_error) {
			memset(&script, 0, sizeof(script));
			database_close_add(&script, true, 1U, true, true, false);
			database_close_add(&script, true, (uint16_t)dos_error,
			    true, true, false);
			CHECK(database_close_fixture(&database, device != 0U,
			    &script));
			yt_error_clear(&error);
			CHECK(!yt_database_random_close(&database, &error)
			    && database.last_close.outcome == (device != 0U
			    ? YT_DATABASE_CLOSE_DEVICE_ERROR
			    : YT_DATABASE_CLOSE_DISK_ERROR)
			    && database.last_close.dos_error == 1U
			    && database.last_close.retry_dos_error == dos_error
			    && database.last_close.basic_error
			    == (device != 0U ? 57U : 70U)
			    && !database.last_close.registered
			    && database.last_close.handle_open
			    && database.file == NULL
			    && database.orphaned_file != NULL);
			yt_database_close(&database);
		}
	}

	/* A failed close may consume the host stream before the required retry. */
	memset(&script, 0, sizeof(script));
	database_close_add(&script, true, 5U, false, true, true);
	database_close_add(&script, true, 6U, false, false, false);
	CHECK(database_close_fixture(&database, false, &script));
	yt_error_clear(&error);
	CHECK(!yt_database_random_close(&database, &error)
	    && script.position == script.length
	    && database.last_close.outcome == YT_DATABASE_CLOSE_DISK_ERROR
	    && database.last_close.dos_error == 5U
	    && !database.last_close.handle_open
	    && database.orphaned_file == NULL);
	yt_database_close(&database);

	/* Provider transport and malformed observations stay distinct from DOS. */
	memset(&script, 0, sizeof(script));
	database_close_add(&script, true, 5U, true, true, false);
	script.steps[0].provider_ok = false;
	CHECK(database_close_fixture(&database, false, &script));
	yt_error_clear(&error);
	CHECK(!yt_database_random_close(&database, &error)
	    && database.last_close.outcome == YT_DATABASE_CLOSE_PROVIDER_ERROR
	    && database.last_close.attempt_count == 1U
	    && database.last_close.registered
	    && database.last_close.handle_open
	    && database.file != NULL);
	yt_database_close(&database);
	memset(&script, 0, sizeof(script));
	database_close_add(&script, true, 0U, true, true, false);
	CHECK(database_close_fixture(&database, false, &script));
	yt_error_clear(&error);
	CHECK(!yt_database_random_close(&database, &error)
	    && database.last_close.outcome == YT_DATABASE_CLOSE_PROVIDER_ERROR
	    && database.last_close.registered
	    && database.last_close.handle_open);
	yt_database_close(&database);
	memset(&script, 0, sizeof(script));
	database_close_add(&script, true, 5U, true, true, false);
	database_close_add(&script, false, 0U, false, true, true);
	script.steps[1].provider_ok = false;
	CHECK(database_close_fixture(&database, false, &script));
	yt_error_clear(&error);
	CHECK(!yt_database_random_close(&database, &error)
	    && database.last_close.outcome == YT_DATABASE_CLOSE_PROVIDER_ERROR
	    && database.last_close.attempt_count == 2U
	    && !database.last_close.registered
	    && database.last_close.handle_open
	    && database.orphaned_file != NULL);
	yt_database_close(&database);
	memset(&script, 0, sizeof(script));
	database_close_add(&script, true, 5U, true, true, false);
	database_close_add(&script, false, 5U, false, true, true);
	CHECK(database_close_fixture(&database, false, &script));
	yt_error_clear(&error);
	CHECK(!yt_database_random_close(&database, &error)
	    && database.last_close.outcome == YT_DATABASE_CLOSE_PROVIDER_ERROR
	    && database.last_close.dos_error == 5U
	    && database.last_close.attempt_count == 2U
	    && !database.last_close.registered
	    && !database.last_close.handle_open
	    && database.orphaned_file == NULL);
	yt_database_close(&database);

	/* The default host adapter closes a live file without changing the tape. */
	memset(&database, 0, sizeof(database));
	database.file = tmpfile();
	CHECK(database.file != NULL);
	if (database.file != NULL) {
		database.records = 1U;
		yt_error_clear(&error);
		CHECK(yt_database_random_close(&database, &error)
		    && database.last_close.outcome
		    == YT_DATABASE_CLOSE_RETURNED
		    && database.last_close.attempt_count == 1U
		    && database.file == NULL && database.records == 0U);
	}
	yt_database_close(&database);
}

static void
text_output_add_success(struct text_close_script *script,
    enum yt_text_close_operation operation, const uint8_t *pending,
    size_t pending_length)
{
	static const uint8_t eof_byte = 0x1aU;
	const uint8_t *expected = NULL;
	size_t requested = 0U;
	bool handle_open = true;
	bool close_active = false;

	if (operation == YT_TEXT_CLOSE_PENDING_WRITE) {
		expected = pending;
		requested = pending_length;
	}
	else if (operation == YT_TEXT_CLOSE_EOF_WRITE) {
		expected = &eof_byte;
		requested = 1U;
	}
	else if (operation == YT_TEXT_CLOSE_HANDLE) {
		handle_open = false;
		close_active = true;
	}
	text_close_add(script, operation, expected, requested, requested,
	    false, 0U, handle_open, true, close_active);
}

static void
test_text_output_write(void)
{
	char directory[256];
	char path[320];
	char missing_parent_path[384];
	uint8_t bytes[300];
	uint8_t full[YT_TEXT_OUTPUT_BUFFER_SIZE];
	uint8_t next = 0xeeU;
	struct text_output_write_script write_script;
	struct text_close_script close_script;
	struct yt_text_output output;
	struct yt_text_file text;
	struct yt_error error;
	unsigned accepted;
	unsigned dos_error;
	size_t index;

	for (index = 0U; index < sizeof(bytes); ++index)
		bytes[index] = (uint8_t)(index & 0xffU);
	memcpy(full, bytes, sizeof(full));
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

	/* Bytes 1..128 remain pending; byte 129 flushes the prior block first. */
	memset(&write_script, 0, sizeof(write_script));
	memset(&close_script, 0, sizeof(close_script));
	text_output_write_add(&write_script, full, sizeof(full), false, 0U,
	    true);
	CHECK(text_output_close_fixture(&output, &close_script, NULL, 0U));
	yt_text_output_set_write_provider(&output, scripted_text_output_write,
	    &write_script);
	CHECK(yt_text_output_write(&output, bytes, sizeof(full), &error)
	    && output.pending_count == sizeof(full)
	    && output.last_write.outcome == YT_TEXT_OUTPUT_WRITE_RETURNED
	    && output.last_write.accepted == sizeof(full)
	    && output.last_write.flush_count == 0U
	    && write_script.position == 0U);
	CHECK(yt_text_output_write(&output, &next, 1U, &error)
	    && write_script.position == write_script.length
	    && output.pending_count == 1U && output.pending[0] == next
	    && output.last_write.accepted == 1U
	    && output.last_write.flush_count == 1U
	    && output.last_write.terminal_position == 41
	    && output.last_write.registered && output.last_write.handle_open);
	yt_text_output_destroy(&output);

	/* Two full blocks flush before byte 257 is accepted. */
	memset(&write_script, 0, sizeof(write_script));
	memset(&close_script, 0, sizeof(close_script));
	text_output_write_add(&write_script, bytes, sizeof(full), false, 0U,
	    true);
	text_output_write_add(&write_script, bytes + sizeof(full), sizeof(full),
	    false, 0U, true);
	CHECK(text_output_close_fixture(&output, &close_script, NULL, 0U));
	yt_text_output_set_write_provider(&output, scripted_text_output_write,
	    &write_script);
	CHECK(yt_text_output_write(&output, bytes, 257U, &error)
	    && write_script.position == write_script.length
	    && output.last_write.accepted == 257U
	    && output.last_write.flush_count == 2U
	    && output.pending_count == 1U && output.pending[0] == bytes[256]);
	yt_text_output_destroy(&output);

	/* Every clear short prefix drops the entry before accepting byte 129. */
	for (accepted = 0U; accepted < sizeof(full); ++accepted) {
		memset(&write_script, 0, sizeof(write_script));
		memset(&close_script, 0, sizeof(close_script));
		text_output_write_add(&write_script, full, accepted, false, 0U,
		    true);
		CHECK(text_output_close_fixture(&output, &close_script, full,
		    sizeof(full)));
		yt_text_output_set_write_provider(&output,
		    scripted_text_output_write, &write_script);
		yt_error_clear(&error);
		CHECK(!yt_text_output_write(&output, &next, 1U, &error)
		    && error.status == YT_IO_ERROR
		    && strcmp(error.operation, "sequential PRINT") == 0
		    && write_script.position == write_script.length
		    && output.last_write.outcome
		    == YT_TEXT_OUTPUT_WRITE_SHORT_ERROR
		    && output.last_write.basic_error == 61U
		    && output.last_write.dos_error == 0U
		    && output.last_write.accepted == 0U
		    && output.last_write.failed_flush_accepted == accepted
		    && !output.last_write.physical_unknown
		    && !output.last_write.cleanup_close_attempted
		    && !output.last_write.registered
		    && output.last_write.handle_open
		    && output.pending_count == 0U && output.file == NULL
		    && output.orphaned_file != NULL);
		yt_text_output_destroy(&output);
	}

	/* An explicitly unknown carry retries CLOSE and routes ERR 71. */
	for (dos_error = 1U; dos_error <= 0xffU; ++dos_error) {
		memset(&write_script, 0, sizeof(write_script));
		memset(&close_script, 0, sizeof(close_script));
		text_output_write_add(&write_script, full, 0U, true,
		    (uint16_t)dos_error, true);
		text_close_add(&close_script,
		    YT_TEXT_CLOSE_CLEANUP_HANDLE, NULL, 0U, 0U, false,
		    0U, false, true, true);
		CHECK(text_output_close_fixture(&output, &close_script, full,
		    sizeof(full)));
		yt_text_output_set_write_provider(&output,
		    scripted_text_output_write, &write_script);
		yt_error_clear(&error);
		CHECK(!yt_text_output_write(&output, &next, 1U, &error)
		    && write_script.position == write_script.length
		    && close_script.position == close_script.length
		    && output.last_write.outcome
		    == YT_TEXT_OUTPUT_WRITE_DISK_ERROR
		    && output.last_write.basic_error == 71U
		    && output.last_write.dos_error == dos_error
		    && output.last_write.accepted == 0U
		    && output.last_write.failed_flush_accepted == 0U
		    && output.last_write.physical_unknown
		    && output.last_write.cleanup_close_attempted
		    && !output.last_write.registered
		    && !output.last_write.handle_open
		    && output.pending_count == 0U && output.file == NULL
		    && output.orphaned_file == NULL);
		yt_text_output_destroy(&output);
	}
	memset(&write_script, 0, sizeof(write_script));
	memset(&close_script, 0, sizeof(close_script));
	text_output_write_add(&write_script, full, 0U, true, 5U, true);
	text_close_add(&close_script,
	    YT_TEXT_CLOSE_CLEANUP_HANDLE, NULL, 0U, 0U, true, 6U,
	    true, true, false);
	CHECK(text_output_close_fixture(&output, &close_script, full,
	    sizeof(full)));
	yt_text_output_set_write_provider(&output, scripted_text_output_write,
	    &write_script);
	CHECK(!yt_text_output_write(&output, &next, 1U, &error)
	    && output.last_write.outcome == YT_TEXT_OUTPUT_WRITE_DISK_ERROR
	    && output.last_write.basic_error == 71U
	    && output.last_write.dos_error == 5U
	    && output.last_write.cleanup_dos_error == 6U
	    && output.last_write.physical_unknown
	    && output.last_write.cleanup_close_attempted
	    && !output.last_write.registered && output.last_write.handle_open
	    && output.file == NULL && output.orphaned_file != NULL);
	yt_text_output_destroy(&output);

	/* Provider transport failure is not mistaken for a DOS result. */
	memset(&write_script, 0, sizeof(write_script));
	memset(&close_script, 0, sizeof(close_script));
	text_output_write_add(&write_script, full, sizeof(full), false, 0U,
	    true);
	write_script.steps[0].provider_ok = false;
	CHECK(text_output_close_fixture(&output, &close_script, full,
	    sizeof(full)));
	yt_text_output_set_write_provider(&output, scripted_text_output_write,
	    &write_script);
	CHECK(!yt_text_output_write(&output, &next, 1U, &error)
	    && output.last_write.outcome == YT_TEXT_OUTPUT_WRITE_PROVIDER_ERROR
	    && output.last_write.basic_error == 57U
	    && output.last_write.flush_count == 1U
	    && output.last_write.registered && output.last_write.handle_open
	    && output.pending_count == 0U && output.file != NULL);
	yt_text_output_destroy(&output);

	/* The host adapter composes arbitrary writes with the close transaction. */
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
test_genesis_sequential_handoff_boundaries(void)
{
	uint8_t command126[128];
	uint8_t command127[129];
	uint8_t command_carry[129];
	uint8_t first_block[YT_TEXT_OUTPUT_BUFFER_SIZE];
	struct text_output_write_script write_script;
	struct text_close_script close_script;
	struct yt_text_output output;
	struct yt_error error;
	unsigned operation;

	memset(command126, 'A', 126U);
	command126[126] = '\r';
	command126[127] = '\n';
	memset(&write_script, 0, sizeof(write_script));
	memset(&close_script, 0, sizeof(close_script));
	for (operation = YT_TEXT_CLOSE_PENDING_WRITE;
	    operation <= YT_TEXT_CLOSE_HANDLE; ++operation)
		text_output_add_success(&close_script,
		    (enum yt_text_close_operation)operation,
		    command126, sizeof(command126));
	CHECK(text_output_close_fixture(&output, &close_script, NULL, 0U));
	yt_text_output_set_write_provider(&output, scripted_text_output_write,
	    &write_script);
	yt_error_clear(&error);
	CHECK(yt_text_output_write(&output, command126, sizeof(command126),
	    &error)
	    && output.last_write.outcome == YT_TEXT_OUTPUT_WRITE_RETURNED
	    && output.last_write.flush_count == 0U
	    && output.pending_count == sizeof(command126)
	    && write_script.position == 0U
	    && yt_text_output_close_all_method(&output, 0, &error)
	    && close_script.position == close_script.length
	    && output.last_close.outcome == YT_TEXT_CLOSE_RETURNED
	    && output.last_close.operation_count == 4U
	    && output.file == NULL && output.pending_count == 0U);
	yt_text_output_destroy(&output);

	memset(command127, 'B', 127U);
	command127[127] = '\r';
	command127[128] = '\n';
	memcpy(first_block, command127, sizeof(first_block));
	memset(&write_script, 0, sizeof(write_script));
	text_output_write_add(&write_script, first_block,
	    sizeof(first_block), false, 0U, true);
	memset(&close_script, 0, sizeof(close_script));
	for (operation = YT_TEXT_CLOSE_PENDING_WRITE;
	    operation <= YT_TEXT_CLOSE_HANDLE; ++operation)
		text_output_add_success(&close_script,
		    (enum yt_text_close_operation)operation,
		    command127 + 128U, 1U);
	CHECK(text_output_close_fixture(&output, &close_script, NULL, 0U));
	yt_text_output_set_write_provider(&output, scripted_text_output_write,
	    &write_script);
	yt_error_clear(&error);
	CHECK(yt_text_output_write(&output, command127, sizeof(command127),
	    &error)
	    && output.last_write.outcome == YT_TEXT_OUTPUT_WRITE_RETURNED
	    && output.last_write.flush_count == 1U
	    && output.last_write.accepted == sizeof(command127)
	    && write_script.position == write_script.length
	    && output.pending_count == 1U && output.pending[0] == '\n'
	    && yt_text_output_close_all_method(&output, 0, &error)
	    && close_script.position == close_script.length
	    && output.last_close.outcome == YT_TEXT_CLOSE_RETURNED
	    && output.last_close.operation_count == 4U
	    && output.file == NULL && output.pending_count == 0U);
	yt_text_output_destroy(&output);

	/* A carry can retain an externally observed prefix and cursor. */
	memset(command_carry, 'C', 127U);
	command_carry[127] = '\r';
	command_carry[128] = '\n';
	memcpy(first_block, command_carry, sizeof(first_block));
	memset(&write_script, 0, sizeof(write_script));
	text_output_write_add(&write_script, first_block, 7U, true, 29U, true);
	write_script.steps[0].observation.terminal_position = 7;
	memset(&close_script, 0, sizeof(close_script));
	text_close_add(&close_script, YT_TEXT_CLOSE_CLEANUP_HANDLE, NULL, 0U,
	    0U, false, 0U, false, true, true);
	CHECK(text_output_close_fixture(&output, &close_script, NULL, 0U));
	yt_text_output_set_write_provider(&output, scripted_text_output_write,
	    &write_script);
	yt_error_clear(&error);
	CHECK(!yt_text_output_write(&output, command_carry,
	    sizeof(command_carry), &error)
	    && output.last_write.outcome == YT_TEXT_OUTPUT_WRITE_DISK_ERROR
	    && output.last_write.basic_error == 71U
	    && output.last_write.dos_error == 29U
	    && output.last_write.accepted == 128U
	    && output.last_write.failed_flush_accepted == 7U
	    && output.last_write.terminal_position == 7
	    && !output.last_write.physical_unknown
	    && output.last_write.cleanup_close_attempted
	    && !output.last_write.registered && !output.last_write.handle_open
	    && output.pending_count == 0U && output.file == NULL
	    && output.orphaned_file == NULL
	    && write_script.position == write_script.length
	    && close_script.position == close_script.length);
	yt_text_output_destroy(&output);
}

static void
test_text_device_print(void)
{
	static const uint8_t values[] = {'A', '\r', 'B'};
	static const uint8_t pair[] = {'A', 'B'};
	static const uint8_t wrap_pair[] = {'Z', 'A'};
	static const uint8_t one[] = {'X'};
	static const uint8_t file_hooks[] = {
		0xffU, 0xc8U, 0x10U, 0xc9U, 0x9bU, 0xc9U, 0xeeU,
		0xc8U, 0x0dU, 0xcaU, 0x0fU, 0xcaU, 0x22U, 0xcaU,
	};
	struct text_device_write_script script;
	struct yt_text_device_state state;
	struct yt_text_device_control_state selected_control;
	struct yt_text_device_control_state active_close_control;
	struct yt_text_device_runtime_state runtime;
	struct text_device_close_script close_script;
	struct yt_text_device_print_result result;
	struct yt_error error;
	struct yt_text_device_process_state process_state;
	struct yt_brun_type3_release_result release;
	static uint8_t process[YT_TEXT_DEVICE_PROCESS_SIZE];
	uint16_t control;
	uint16_t found_control;

	/* One logical byte is offered per write, with the final byte at completion. */
	memset(&script, 0, sizeof(script));
	text_device_write_add(&script, YT_TEXT_DEVICE_WRITE_VALUE, 'A',
	    1U, 1U, false, false, 0U, 0U);
	text_device_write_add(&script, YT_TEXT_DEVICE_WRITE_VALUE, '\r',
	    1U, 1U, false, false, 0U, 0U);
	text_device_write_add(&script, YT_TEXT_DEVICE_WRITE_COMPLETION, 'B',
	    1U, 1U, false, false, 0U, 0U);
	state = (struct yt_text_device_state){.selected = true};
	yt_error_clear(&error);
	CHECK(yt_text_device_print(&state, values, sizeof(values), false,
	    YT_TEXT_DEVICE_CONS, 0x82U, 5U, scripted_text_device_write,
	    &script, &result, &error)
	    && script.position == script.length
	    && script.physical_length == sizeof(values)
	    && memcmp(script.physical, values, sizeof(values)) == 0
	    && result.outcome == YT_TEXT_DEVICE_PRINT_RETURNED
	    && result.logical_length == 3U && result.staged == 3U
	    && result.write_count == 3U && result.accepted_count == 3U
	    && result.terminal_position == 51
	    && state.index == 3U && state.column == 1U
	    && !state.pending && state.buffer == 'B' && !state.selected);

	/* COM newline is CR only; CONS retains LF after CR. */
	memset(&script, 0, sizeof(script));
	text_device_write_add(&script, YT_TEXT_DEVICE_WRITE_VALUE, 'X',
	    1U, 1U, false, false, 0U, 0U);
	text_device_write_add(&script, YT_TEXT_DEVICE_WRITE_COMPLETION, '\r',
	    1U, 1U, false, false, 0U, 0U);
	state = (struct yt_text_device_state){.selected = true};
	CHECK(yt_text_device_print(&state, one, sizeof(one), true,
	    YT_TEXT_DEVICE_COM1, 0x82U, 5U, scripted_text_device_write,
	    &script, &result, &error)
	    && result.logical_length == 2U && state.index == 2U
	    && state.column == 0U && state.buffer == '\r');
	memset(&script, 0, sizeof(script));
	text_device_write_add(&script, YT_TEXT_DEVICE_WRITE_VALUE, 'X',
	    1U, 1U, false, false, 0U, 0U);
	text_device_write_add(&script, YT_TEXT_DEVICE_WRITE_VALUE, '\r',
	    1U, 1U, false, false, 0U, 0U);
	text_device_write_add(&script, YT_TEXT_DEVICE_WRITE_COMPLETION, '\n',
	    1U, 1U, false, false, 0U, 0U);
	state = (struct yt_text_device_state){.selected = true};
	CHECK(yt_text_device_print(&state, one, sizeof(one), true,
	    YT_TEXT_DEVICE_CONS, 0x82U, 5U, scripted_text_device_write,
	    &script, &result, &error)
	    && result.logical_length == 3U && state.index == 3U
	    && state.column == 0U && state.buffer == '\n');

	/* Compatibility bit 02 accepts a zero-count write as a dropped byte. */
	memset(&script, 0, sizeof(script));
	text_device_write_add(&script, YT_TEXT_DEVICE_WRITE_VALUE, 'A',
	    1U, 0U, false, false, 0U, 0U);
	text_device_write_add(&script, YT_TEXT_DEVICE_WRITE_COMPLETION, 'B',
	    1U, 1U, false, false, 0U, 0U);
	state = (struct yt_text_device_state){.selected = true};
	CHECK(yt_text_device_print(&state, pair, sizeof(pair), false,
	    YT_TEXT_DEVICE_COM1, 0x82U, 5U, scripted_text_device_write,
	    &script, &result, &error)
	    && script.physical_length == 1U && script.physical[0] == 'B'
	    && result.accepted_count == 1U && state.index == 2U
	    && state.column == 2U && state.buffer == 'B');

	/* Without bit 02, value-time zero retains the previous pending byte. */
	memset(&script, 0, sizeof(script));
	text_device_write_add(&script, YT_TEXT_DEVICE_WRITE_VALUE, 'A',
	    1U, 0U, false, false, 0U, 0U);
	state = (struct yt_text_device_state){.selected = true};
	CHECK(!yt_text_device_print(&state, pair, sizeof(pair), false,
	    YT_TEXT_DEVICE_CONS, 0x80U, 5U, scripted_text_device_write,
	    &script, &result, &error)
	    && result.outcome == YT_TEXT_DEVICE_PRINT_VALUE_SHORT_ERROR
	    && result.basic_error == 57U && result.staged == 1U
	    && state.index == 1U && state.column == 1U
	    && state.pending && state.buffer == 'A' && state.selected);

	/* Value carry preserves pending state and uses the DOS-version mapping. */
	memset(&script, 0, sizeof(script));
	text_device_write_add(&script, YT_TEXT_DEVICE_WRITE_VALUE, 'Z',
	    1U, 0U, true, true, 5U, 0x0021U);
	state = (struct yt_text_device_state){
		.index = 0xffffffU,
		.selected = true,
	};
	CHECK(!yt_text_device_print(&state, wrap_pair, sizeof(wrap_pair), false,
	    YT_TEXT_DEVICE_COM1, 0x82U, 5U, scripted_text_device_write,
	    &script, &result, &error)
	    && result.outcome == YT_TEXT_DEVICE_PRINT_VALUE_DISK_ERROR
	    && result.basic_error == 70U && result.dos_error == 5U
	    && result.physical_unknown && state.physical_unknown
	    && state.index == 0U && state.pending && state.buffer == 'Z'
	    && state.selected);
	memset(&script, 0, sizeof(script));
	text_device_write_add(&script, YT_TEXT_DEVICE_WRITE_VALUE, 'A',
	    1U, 0U, true, false, 6U, 0x0021U);
	state = (struct yt_text_device_state){.selected = true};
	CHECK(!yt_text_device_print(&state, pair, sizeof(pair), false,
	    YT_TEXT_DEVICE_COM1, 0x82U, 2U, scripted_text_device_write,
	    &script, &result, &error)
	    && result.basic_error == 52U && result.dos_error == 6U);

	/* Completion exchanges pending to zero before carry routes to ERR57. */
	memset(&script, 0, sizeof(script));
	text_device_write_add(&script, YT_TEXT_DEVICE_WRITE_COMPLETION, 'X',
	    1U, 0U, true, false, 5U, 0U);
	state = (struct yt_text_device_state){.selected = true};
	CHECK(!yt_text_device_print(&state, one, sizeof(one), false,
	    YT_TEXT_DEVICE_COM1, 0x82U, 5U, scripted_text_device_write,
	    &script, &result, &error)
	    && result.outcome == YT_TEXT_DEVICE_PRINT_COMPLETION_ERROR
	    && result.basic_error == 57U && result.dos_error == 5U
	    && !state.pending && state.buffer == 'X' && state.selected);

	/* Even an empty statement performs one zero-length completion write. */
	memset(&script, 0, sizeof(script));
	text_device_write_add(&script, YT_TEXT_DEVICE_WRITE_COMPLETION, 0U,
	    0U, 0U, false, false, 0U, 0U);
	state = (struct yt_text_device_state){.selected = true};
	CHECK(yt_text_device_print(&state, NULL, 0U, false,
	    YT_TEXT_DEVICE_COM1, 0x82U, 5U, scripted_text_device_write,
	    &script, &result, &error)
	    && result.logical_length == 0U && result.staged == 0U
	    && result.write_count == 1U && !state.selected);

	/* Adapter rejection preserves completion's already-cleared pending count. */
	memset(&script, 0, sizeof(script));
	text_device_write_add(&script, YT_TEXT_DEVICE_WRITE_COMPLETION, 'X',
	    1U, 1U, false, false, 0U, 0U);
	script.steps[0].provider_ok = false;
	state = (struct yt_text_device_state){.selected = true};
	CHECK(!yt_text_device_print(&state, one, sizeof(one), false,
	    YT_TEXT_DEVICE_COM1, 0x82U, 5U, scripted_text_device_write,
	    &script, &result, &error)
	    && result.outcome == YT_TEXT_DEVICE_PRINT_PROVIDER_ERROR
	    && result.basic_error == 57U && !state.pending
	    && state.buffer == 'X' && state.selected);

	state = (struct yt_text_device_state){.selected = true};
	CHECK(!yt_text_device_print(&state, one, sizeof(one), false,
	    YT_TEXT_DEVICE_COM1, 0x02U, 5U, scripted_text_device_write,
	    &script, &result, &error) && error.status == YT_INVALID);

	/* Value carry conditionally frees the selected control before routing. */
	memset(&script, 0, sizeof(script));
	text_device_write_add(&script, YT_TEXT_DEVICE_WRITE_VALUE, 'A',
	    1U, 0U, true, false, 5U, 0x0021U);
	state = (struct yt_text_device_state){.selected = true};
	selected_control = (struct yt_text_device_control_state){true, true, 3U};
	runtime = (struct yt_text_device_runtime_state){
		.error_status = 1U,
		.selected_control = &selected_control,
	};
	CHECK(!yt_text_device_print_runtime(&state, pair, sizeof(pair), false,
	    YT_TEXT_DEVICE_COM1, 0x82U, 5U, scripted_text_device_write,
	    &script, &runtime, &result, &error)
	    && result.outcome == YT_TEXT_DEVICE_PRINT_VALUE_DISK_ERROR
	    && result.basic_error == 70U
	    && !selected_control.allocated && !selected_control.registered
	    && selected_control.field_binding_count == 0U
	    && runtime.selected_control == &selected_control
	    && state.selected && state.pending && state.buffer == 'A');
	memset(&script, 0, sizeof(script));
	text_device_write_add(&script, YT_TEXT_DEVICE_WRITE_VALUE, 'A',
	    1U, 0U, true, false, 6U, 0U);
	state = (struct yt_text_device_state){.selected = true};
	selected_control = (struct yt_text_device_control_state){true, true, 4U};
	runtime = (struct yt_text_device_runtime_state){
		.selected_control = &selected_control,
	};
	CHECK(!yt_text_device_print_runtime(&state, pair, sizeof(pair), false,
	    YT_TEXT_DEVICE_COM1, 0x82U, 2U, scripted_text_device_write,
	    &script, &runtime, &result, &error)
	    && result.basic_error == 52U && selected_control.allocated
	    && selected_control.registered
	    && selected_control.field_binding_count == 4U);

	/* Completion failure closes and frees only a nonzero active-CLOSE root. */
	memset(&script, 0, sizeof(script));
	text_device_write_add(&script, YT_TEXT_DEVICE_WRITE_COMPLETION, 'X',
	    1U, 0U, true, false, 5U, 0U);
	memset(&close_script, 0, sizeof(close_script));
	close_script.provider_ok = true;
	close_script.observation.carry = true;
	close_script.observation.dos_error = 9U;
	state = (struct yt_text_device_state){.selected = true};
	selected_control = (struct yt_text_device_control_state){true, true, 2U};
	active_close_control = (struct yt_text_device_control_state){
		true, true, 5U};
	runtime = (struct yt_text_device_runtime_state){
		.selected_control = &selected_control,
		.active_close_control = &active_close_control,
		.close_provider = scripted_text_device_close,
		.close_context = &close_script,
	};
	CHECK(!yt_text_device_print_runtime(&state, one, sizeof(one), false,
	    YT_TEXT_DEVICE_COM1, 0x82U, 5U, scripted_text_device_write,
	    &script, &runtime, &result, &error)
	    && result.outcome == YT_TEXT_DEVICE_PRINT_COMPLETION_ERROR
	    && close_script.calls == 1U && runtime.cleanup_close_count == 1U
	    && runtime.cleanup_close_observed && runtime.cleanup_close_carry
	    && runtime.cleanup_close_dos_error == 9U
	    && runtime.active_close_control == NULL
	    && !active_close_control.allocated
	    && !active_close_control.registered
	    && active_close_control.field_binding_count == 0U
	    && selected_control.allocated && selected_control.registered
	    && selected_control.field_binding_count == 2U
	    && state.selected && !state.pending && state.buffer == 'X');

	/* Alias identity and an unavailable ignored-result adapter remain exact. */
	memset(&script, 0, sizeof(script));
	text_device_write_add(&script, YT_TEXT_DEVICE_WRITE_COMPLETION, 'X',
	    1U, 0U, false, false, 0U, 0U);
	state = (struct yt_text_device_state){.selected = true};
	selected_control = (struct yt_text_device_control_state){true, true, 7U};
	runtime = (struct yt_text_device_runtime_state){
		.selected_control = &selected_control,
		.active_close_control = &selected_control,
	};
	CHECK(!yt_text_device_print_runtime(&state, one, sizeof(one), false,
	    YT_TEXT_DEVICE_CONS, 0x80U, 5U, scripted_text_device_write,
	    &script, &runtime, &result, &error)
	    && result.outcome == YT_TEXT_DEVICE_PRINT_COMPLETION_ERROR
	    && runtime.cleanup_close_count == 1U
	    && !runtime.cleanup_close_observed
	    && runtime.active_close_control == NULL
	    && runtime.selected_control == &selected_control
	    && !selected_control.allocated && !selected_control.registered
	    && selected_control.field_binding_count == 0U);

	/* The raw process projection reads and commits the documented cells. */
	memset(process, 0x5a, sizeof(process));
	control = 0x2000U;
	process[0x0eceU] = 0xfdU;
	process[0x0ecfU] = 0x21U;
	process[0x21fdU] = 2U;
	process[0x21faU] = 0x00U;
	process[0x21fbU] = 0x01U;
	process[0x20fdU] = 3U;
	process[0x20faU] = 0x00U;
	process[0x20fbU] = 0x01U;
	process[0x20fcU] = 3U;
	process[0x1ffdU] = 4U;
	CHECK(yt_brun_file_control_find(process, sizeof(process), 3U,
	    &found_control, &error) && found_control == control);
	process[0x0a3aU] = (uint8_t)control;
	process[0x0a3bU] = (uint8_t)(control >> 8U);
	process[0x0020U] = 5U;
	process[control + 0x27U] = 0xfeU;
	process[control + 0x28U] = 0xffU;
	process[control + 0x2aU] = 0U;
	process[control + 0x2dU] = 0xffU;
	process[control + 0x2eU] = YT_TEXT_DEVICE_COM1;
	process[control + 0x31U] = 0x82U;
	process[control + 0x32U] = 5U;
	process[control + 0x33U] = 0x71U;
	process_state = (struct yt_text_device_process_state){
		.process = process,
		.process_size = sizeof(process),
		.physical_unknown = true,
	};
	CHECK(yt_text_device_print_process_prepare(&process_state, control,
	    0x1234U, 0x5678U, false, &error)
	    && process[0x0a08U] == 0x78U
	    && process[0x0a09U] == 0x56U
	    && process[0x0a3aU] == (uint8_t)control
	    && process[0x0a3bU] == (uint8_t)(control >> 8U)
	    && process[0x0a60U] == 0xd3U
	    && process[0x0a61U] == 0x99U
	    && process[0x0eb6U] == 0U && process[0x0eb7U] == 0U
	    && memcmp(process + 0x0ebaU, file_hooks, sizeof(file_hooks)) == 0
	    && process[0x10d8U] == 0x03U
	    && process[0x10d9U] == 0x01U);
	memset(&script, 0, sizeof(script));
	text_device_write_add(&script, YT_TEXT_DEVICE_WRITE_VALUE, 'A',
	    1U, 1U, false, false, 0U, 0U);
	text_device_write_add(&script, YT_TEXT_DEVICE_WRITE_COMPLETION, 'B',
	    1U, 1U, false, false, 0U, 0U);
	CHECK(yt_text_device_print_process(&process_state, pair, sizeof(pair),
	    false, scripted_text_device_write, &script, NULL, &result, &error)
	    && script.position == script.length
	    && result.outcome == YT_TEXT_DEVICE_PRINT_RETURNED
	    && result.physical_unknown && process_state.physical_unknown
	    && process[0x0a3aU] == 0U && process[0x0a3bU] == 0U
	    && process[control + 0x27U] == 0U
	    && process[control + 0x28U] == 0U
	    && process[control + 0x2dU] == 0U
	    && process[control + 0x2aU] == 0U
	    && process[control + 0x32U] == 7U
	    && process[control + 0x33U] == 'B');
	CHECK(yt_text_device_print_process_prepare(&process_state, control,
	    0xabcdU, 0x2468U, true, &error)
	    && process[0x0a08U] == 0x68U && process[0x0a09U] == 0x24U
	    && process[0x10d8U] == 0x03U
	    && process[0x10d9U] == 0x02U);

	/* Value failure retains selection, pending byte, and raw index/column. */
	memset(process, 0, sizeof(process));
	process[0x0a3aU] = (uint8_t)control;
	process[0x0a3bU] = (uint8_t)(control >> 8U);
	process[0x0020U] = 5U;
	process[control + 0x27U] = 0xffU;
	process[control + 0x28U] = 0xffU;
	process[control + 0x2dU] = 0xffU;
	process[control + 0x2eU] = YT_TEXT_DEVICE_COM1;
	process[control + 0x31U] = 0x82U;
	process[control + 0x32U] = 9U;
	process_state.physical_unknown = false;
	memset(&script, 0, sizeof(script));
	text_device_write_add(&script, YT_TEXT_DEVICE_WRITE_VALUE, 'A',
	    1U, 0U, true, true, 5U, 0x0021U);
	CHECK(!yt_text_device_print_process(&process_state, pair, sizeof(pair),
	    false, scripted_text_device_write, &script, NULL, &result, &error)
	    && result.outcome == YT_TEXT_DEVICE_PRINT_VALUE_DISK_ERROR
	    && result.basic_error == 70U && process_state.physical_unknown
	    && process[0x0a3aU] == (uint8_t)control
	    && process[0x0a3bU] == (uint8_t)(control >> 8U)
	    && process[control + 0x27U] == 0U
	    && process[control + 0x28U] == 0U
	    && process[control + 0x2dU] == 0U
	    && process[control + 0x2aU] == 1U
	    && process[control + 0x32U] == 10U
	    && process[control + 0x33U] == 'A');

	/* Completion failure has distinct zero-pending stale-byte residue. */
	memset(process, 0, sizeof(process));
	process[0x0a3aU] = (uint8_t)control;
	process[0x0a3bU] = (uint8_t)(control >> 8U);
	process[0x0020U] = 5U;
	process[control + 0x2eU] = YT_TEXT_DEVICE_COM1;
	process[control + 0x31U] = 0x82U;
	process_state.physical_unknown = false;
	memset(&script, 0, sizeof(script));
	text_device_write_add(&script, YT_TEXT_DEVICE_WRITE_COMPLETION, 'X',
	    1U, 0U, true, false, 5U, 0U);
	CHECK(!yt_text_device_print_process(&process_state, one, sizeof(one),
	    false, scripted_text_device_write, &script, NULL, &result, &error)
	    && result.outcome == YT_TEXT_DEVICE_PRINT_COMPLETION_ERROR
	    && process[0x0a3aU] == (uint8_t)control
	    && process[control + 0x27U] == 1U
	    && process[control + 0x28U] == 0U
	    && process[control + 0x2dU] == 0U
	    && process[control + 0x2aU] == 0U
	    && process[control + 0x32U] == 1U
	    && process[control + 0x33U] == 'X');

	/* An impossible pending count is rejected before provider or DS writes. */
	process[control + 0x2aU] = 2U;
	memset(&script, 0, sizeof(script));
	yt_error_clear(&error);
	CHECK(!yt_text_device_print_process(&process_state, one, sizeof(one),
	    false, scripted_text_device_write, &script, NULL, &result, &error)
	    && error.status == YT_INVALID && script.position == 0U
	    && process[control + 0x2aU] == 2U
	    && process[0x0a3aU] == (uint8_t)control);

	/* Missing and cyclic raw registries remain distinct outcomes. */
	memset(process, 0, sizeof(process));
	process[0x0eceU] = 0x00U;
	process[0x0ecfU] = 0x30U;
	process[0x3000U] = 4U;
	found_control = 0xffffU;
	CHECK(yt_brun_file_control_find(process, sizeof(process), 3U,
	    &found_control, &error) && found_control == 0U);
	process[0x3000U] = 1U;
	found_control = 0xffffU;
	yt_error_clear(&error);
	CHECK(!yt_brun_file_control_find(process, sizeof(process), 3U,
	    &found_control, &error) && found_control == 0U
	    && error.status == YT_INVALID);

	/* A43D/A560 frees the type-3 block and its FIELD aliases in order. */
	memset(process, 0x5a, sizeof(process));
	process[control - 2U] = 0x00U;
	process[control - 1U] = 0x01U;
	process[0x1ffeU] = 0x00U;
	process[0x1fffU] = 0x01U;
	process[0x20faU] = 0x00U;
	process[0x20fbU] = 0x01U;
	process[0x20fcU] = 3U;
	process[0x20fdU] = 3U;
	process[0x20f6U] = 4U;
	process[0x20f7U] = 0U;
	process[0x20f8U] = 0x00U;
	process[0x20f9U] = 0x30U;
	process[0x3000U] = 0x00U;
	process[0x3001U] = 0x40U;
	process[0x3002U] = 0x00U;
	process[0x3003U] = 0x50U;
	memset(process + 0x4000U, 0x11, 4U);
	memset(process + 0x5000U, 0x22, 4U);
	CHECK(yt_brun_type3_release(process, sizeof(process), control,
	    &release, &error)
	    && release.outcome == YT_BRUN_TYPE3_RELEASE_RETURNED
	    && release.control == control && release.type_address == 0x20fdU
	    && release.internal_entry == 0U
	    && release.cleared_descriptor_count == 2U
	    && process[0x20fdU] == 1U && process[0x20fcU] == 3U
	    && process[0x20f6U] == 0U && process[0x20f7U] == 0U
	    && process[0x20f8U] == 0U && process[0x20f9U] == 0U
	    && process[0x2ffeU] == 5U && process[0x2fffU] == 0U
	    && process[0x3000U] == 0U && process[0x3001U] == 0x40U
	    && process[0x3002U] == 0U && process[0x3003U] == 0x50U
	    && process[0x4000U] == 0U && process[0x4001U] == 0U
	    && process[0x4002U] == 0U && process[0x4003U] == 0U
	    && process[0x5000U] == 0U && process[0x5001U] == 0U
	    && process[0x5002U] == 0U && process[0x5003U] == 0U);

	/* A zero registration length skips A560 and retains pointer residue. */
	memset(process, 0, sizeof(process));
	process[control - 1U] = 0x01U;
	process[0x1fffU] = 0x01U;
	process[0x20fbU] = 0x01U;
	process[0x20fdU] = 3U;
	process[0x20f8U] = 0x56U;
	process[0x20f9U] = 0x34U;
	CHECK(yt_brun_type3_release(process, sizeof(process), control,
	    &release, &error)
	    && release.outcome == YT_BRUN_TYPE3_RELEASE_RETURNED
	    && release.cleared_descriptor_count == 0U
	    && process[0x20fdU] == 1U
	    && process[0x20f8U] == 0x56U && process[0x20f9U] == 0x34U);

	/* A mismatched mirrored span reaches internal 0ACC without DS writes. */
	process[0x20fdU] = 3U;
	process[0x20faU] = 0x80U;
	process[0x20fbU] = 0U;
	CHECK(yt_brun_type3_release(process, sizeof(process), control,
	    &release, &error)
	    && release.outcome == YT_BRUN_TYPE3_RELEASE_INTERNAL_ERROR
	    && release.internal_entry == 0x0accU
	    && release.cleared_descriptor_count == 0U
	    && process[0x20fdU] == 3U
	    && process[0x20f8U] == 0x56U && process[0x20f9U] == 0x34U);
}

static void
test_text_output_close(void)
{
	static const uint8_t pending[] = {'A', 'B', 'C'};
	static const uint8_t eof_byte = 0x1aU;
	char directory[256];
	char path[320];
	struct text_close_script script;
	struct database_public_close_script database_script;
	struct yt_database database;
	struct yt_text_output output;
	struct yt_close_all_control control;
	struct yt_close_all_control genesis_controls[2];
	struct yt_close_all_result close_all;
	struct yt_text_file text;
	struct yt_error error;
	unsigned failed_operation;
	unsigned dos_error;

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

	/* The typed provider sees the four exact operations and staged bytes. */
	memset(&script, 0, sizeof(script));
	for (failed_operation = YT_TEXT_CLOSE_PENDING_WRITE;
	    failed_operation <= YT_TEXT_CLOSE_HANDLE;
	    ++failed_operation)
		text_output_add_success(&script,
		    (enum yt_text_close_operation)failed_operation,
		    pending, sizeof(pending));
	CHECK(text_output_close_fixture(&output, &script, pending,
	    sizeof(pending)));
	control = (struct yt_close_all_control){YT_CLOSE_ALL_HEAP_FILE, 0,
	    yt_text_output_close_all_method, &output};
	CHECK(yt_close_all_run(&control, 1U, NULL, &close_all, &error)
	    && close_all.returned && close_all.completed_count == 1U
	    && script.position == script.length
	    && output.last_close.close_all
	    && output.last_close.outcome == YT_TEXT_CLOSE_RETURNED
	    && output.last_close.operation_count == 4U
	    && output.last_close.terminal_position == 37
	    && output.file == NULL && output.orphaned_file == NULL);
	yt_text_output_destroy(&output);

	/* Character output status skips buffered disk writes and maps close carry. */
	memset(&script, 0, sizeof(script));
	text_close_add(&script, YT_TEXT_CLOSE_HANDLE, NULL, 0U,
	    0U, false, 0U, false, true, true);
	CHECK(text_output_close_fixture(&output, &script, pending,
	    sizeof(pending)));
	output.last_output_open.device = true;
	CHECK(yt_text_output_close(&output, &error)
	    && script.position == script.length
	    && output.last_close.outcome == YT_TEXT_CLOSE_RETURNED
	    && output.last_close.device
	    && output.last_close.operation_count == 1U
	    && output.pending_count == 0U
	    && !output.last_close.registered
	    && !output.last_close.handle_open);
	yt_text_output_destroy(&output);

	for (dos_error = 1U; dos_error <= 0xffU; ++dos_error) {
		memset(&script, 0, sizeof(script));
		text_close_add(&script, YT_TEXT_CLOSE_HANDLE, NULL, 0U,
		    0U, true, (uint16_t)dos_error, true, true, false);
		text_close_add(&script, YT_TEXT_CLOSE_CLEANUP_HANDLE, NULL, 0U,
		    0U, false, 0U, false, true, true);
		CHECK(text_output_close_fixture(&output, &script, pending,
		    sizeof(pending)));
		output.last_output_open.device = true;
		CHECK(!yt_text_output_close(&output, &error)
		    && script.position == script.length
		    && output.last_close.outcome == YT_TEXT_CLOSE_DISK_ERROR
		    && output.last_close.device
		    && output.last_close.basic_error == 57U
		    && output.last_close.dos_error == dos_error
		    && output.last_close.operation_count == 2U
		    && output.pending_count == 0U
		    && !output.last_close.registered
		    && !output.last_close.handle_open);
		yt_text_output_destroy(&output);
	}

	/* Genesis allocates database #1 before output #5, so #5 closes first. */
	memset(&script, 0, sizeof(script));
	for (failed_operation = YT_TEXT_CLOSE_PENDING_WRITE;
	    failed_operation <= YT_TEXT_CLOSE_HANDLE;
	    ++failed_operation)
		text_output_add_success(&script,
		    (enum yt_text_close_operation)failed_operation,
		    pending, sizeof(pending));
	memset(&database_script, 0, sizeof(database_script));
	database_close_add(&database_script, false, 0U, false, true, true);
	CHECK(text_output_close_fixture(&output, &script, pending,
	    sizeof(pending))
	    && database_close_fixture(&database, false, &database_script));
	genesis_controls[0] = (struct yt_close_all_control){
		YT_CLOSE_ALL_HEAP_FILE, 0, yt_database_close_all_method,
		&database};
	genesis_controls[1] = (struct yt_close_all_control){
		YT_CLOSE_ALL_HEAP_FILE, 0, yt_text_output_close_all_method,
		&output};
	CHECK(yt_close_all_run(genesis_controls,
	    YT_ARRAY_LEN(genesis_controls), NULL, &close_all, &error)
	    && close_all.scanned_count == 2U && close_all.attempt_count == 2U
	    && close_all.completed_count == 2U && close_all.returned
	    && script.position == script.length
	    && database_script.position == database_script.length
	    && output.last_close.close_all && database.last_close.close_all
	    && output.file == NULL && database.file == NULL);
	yt_text_output_destroy(&output);
	yt_database_close(&database);

	/* A file-5 failure stops the walk before the older database control. */
	memset(&script, 0, sizeof(script));
	text_close_add(&script, YT_TEXT_CLOSE_PENDING_WRITE,
	    pending, sizeof(pending), 2U, false, 0U, true, true, false);
	memset(&database_script, 0, sizeof(database_script));
	database_close_add(&database_script, false, 0U, false, true, true);
	CHECK(text_output_close_fixture(&output, &script, pending,
	    sizeof(pending))
	    && database_close_fixture(&database, false, &database_script));
	yt_error_clear(&error);
	CHECK(!yt_close_all_run(genesis_controls,
	    YT_ARRAY_LEN(genesis_controls), NULL, &close_all, &error)
	    && close_all.failed && close_all.failed_index == 1U
	    && close_all.scanned_count == 1U && close_all.attempt_count == 1U
	    && close_all.completed_count == 0U && !close_all.returned
	    && script.position == script.length
	    && database_script.position == 0U && database.file != NULL
	    && database.last_close.outcome == YT_DATABASE_CLOSE_NONE
	    && output.last_close.outcome == YT_TEXT_CLOSE_SHORT_ERROR);
	yt_text_output_destroy(&output);
	yt_database_close(&database);

	/* Every physical operation retains every DOS error and then retries CLOSE. */
	for (failed_operation = YT_TEXT_CLOSE_PENDING_WRITE;
	    failed_operation <= YT_TEXT_CLOSE_HANDLE;
	    ++failed_operation) {
		for (dos_error = 1U; dos_error <= 0xffU; ++dos_error) {
			memset(&script, 0, sizeof(script));
			for (unsigned operation =
			    YT_TEXT_CLOSE_PENDING_WRITE;
			    operation < failed_operation; ++operation)
				text_output_add_success(&script,
				    (enum yt_text_close_operation)operation,
				    pending, sizeof(pending));
			{
				const uint8_t *expected = NULL;
				size_t requested = 0U;

				if (failed_operation
				    == YT_TEXT_CLOSE_PENDING_WRITE) {
					expected = pending;
					requested = sizeof(pending);
				}
				else if (failed_operation
				    == YT_TEXT_CLOSE_EOF_WRITE) {
					expected = &eof_byte;
					requested = 1U;
				}
				text_close_add(&script,
				    (enum yt_text_close_operation)failed_operation,
				    expected, requested, 0U, true,
				    (uint16_t)dos_error, true, true, false);
			}
			text_close_add(&script,
			    YT_TEXT_CLOSE_CLEANUP_HANDLE, NULL, 0U, 0U,
			    false, 0U, false, true, true);
			CHECK(text_output_close_fixture(&output, &script, pending,
			    sizeof(pending)));
			yt_error_clear(&error);
			CHECK(!yt_text_output_close(&output, &error)
			    && error.status == YT_IO_ERROR
			    && strcmp(error.operation, "sequential CLOSE") == 0
			    && script.position == script.length
			    && output.last_close.outcome
			    == YT_TEXT_CLOSE_DISK_ERROR
			    && output.last_close.failed_operation
			    == (enum yt_text_close_operation)failed_operation
			    && output.last_close.basic_error == 70U
			    && output.last_close.dos_error == dos_error
			    && output.last_close.cleanup_close_attempted
			    && output.last_close.operation_count
			    == (size_t)failed_operation + 2U
			    && !output.last_close.registered
			    && !output.last_close.handle_open
			    && output.file == NULL
			    && output.orphaned_file == NULL);
			yt_text_output_destroy(&output);
		}
	}

	/* Clear-carry short writes route ERR61 without the DOS CLOSE retry. */
	memset(&script, 0, sizeof(script));
	text_close_add(&script, YT_TEXT_CLOSE_PENDING_WRITE,
	    pending, sizeof(pending), 2U, false, 0U, true, true, false);
	CHECK(text_output_close_fixture(&output, &script, pending,
	    sizeof(pending)));
	yt_error_clear(&error);
	CHECK(!yt_text_output_close(&output, &error)
	    && script.position == script.length
	    && output.last_close.outcome == YT_TEXT_CLOSE_SHORT_ERROR
	    && output.last_close.failed_operation
	    == YT_TEXT_CLOSE_PENDING_WRITE
	    && output.last_close.basic_error == 61U
	    && output.last_close.accepted == 2U
	    && output.last_close.operation_count == 1U
	    && !output.last_close.cleanup_close_attempted
	    && !output.last_close.registered && output.last_close.handle_open
	    && output.file == NULL && output.orphaned_file != NULL
	    && output.pending_count == 0U);
	yt_text_output_destroy(&output);

	memset(&script, 0, sizeof(script));
	text_output_add_success(&script,
	    YT_TEXT_CLOSE_PENDING_WRITE, pending, sizeof(pending));
	text_close_add(&script, YT_TEXT_CLOSE_EOF_WRITE,
	    &eof_byte, 1U, 0U, false, 0U, true, true, false);
	CHECK(text_output_close_fixture(&output, &script, pending,
	    sizeof(pending)));
	yt_error_clear(&error);
	CHECK(!yt_text_output_close(&output, &error)
	    && output.last_close.outcome == YT_TEXT_CLOSE_SHORT_ERROR
	    && output.last_close.failed_operation
	    == YT_TEXT_CLOSE_EOF_WRITE
	    && output.last_close.operation_count == 2U
	    && !output.last_close.cleanup_close_attempted
	    && output.pending_count == 0U && output.orphaned_file != NULL);
	yt_text_output_destroy(&output);

	/* Cleanup carry is ignored, while adapter faults remain separately typed. */
	memset(&script, 0, sizeof(script));
	text_close_add(&script, YT_TEXT_CLOSE_PENDING_WRITE,
	    pending, sizeof(pending), 0U, true, 5U, true, true, false);
	text_close_add(&script, YT_TEXT_CLOSE_CLEANUP_HANDLE,
	    NULL, 0U, 0U, true, 6U, true, true, false);
	CHECK(text_output_close_fixture(&output, &script, pending,
	    sizeof(pending)));
	CHECK(!yt_text_output_close(&output, &error)
	    && output.last_close.outcome == YT_TEXT_CLOSE_DISK_ERROR
	    && output.last_close.dos_error == 5U
	    && output.last_close.cleanup_dos_error == 6U
	    && output.last_close.handle_open
	    && output.file == NULL && output.orphaned_file != NULL);
	yt_text_output_destroy(&output);

	memset(&script, 0, sizeof(script));
	text_output_add_success(&script,
	    YT_TEXT_CLOSE_PENDING_WRITE, pending, sizeof(pending));
	text_output_add_success(&script, YT_TEXT_CLOSE_EOF_WRITE,
	    pending, sizeof(pending));
	text_output_add_success(&script, YT_TEXT_CLOSE_TRUNCATE,
	    pending, sizeof(pending));
	text_close_add(&script, YT_TEXT_CLOSE_HANDLE, NULL, 0U,
	    0U, true, 5U, false, true, true);
	text_close_add(&script, YT_TEXT_CLOSE_CLEANUP_HANDLE, NULL,
	    0U, 0U, true, 6U, false, false, false);
	CHECK(text_output_close_fixture(&output, &script, pending,
	    sizeof(pending)));
	CHECK(!yt_text_output_close(&output, &error)
	    && script.position == script.length
	    && output.last_close.outcome == YT_TEXT_CLOSE_DISK_ERROR
	    && output.last_close.dos_error == 5U
	    && output.last_close.cleanup_dos_error == 6U
	    && output.last_close.cleanup_close_attempted
	    && !output.last_close.handle_open
	    && output.file == NULL && output.orphaned_file == NULL);
	yt_text_output_destroy(&output);

	memset(&script, 0, sizeof(script));
	text_close_add(&script, YT_TEXT_CLOSE_PENDING_WRITE,
	    pending, sizeof(pending), sizeof(pending), false, 0U, true,
	    true, false);
	script.steps[0].provider_ok = false;
	CHECK(text_output_close_fixture(&output, &script, pending,
	    sizeof(pending)));
	CHECK(!yt_text_output_close(&output, &error)
	    && output.last_close.outcome == YT_TEXT_CLOSE_PROVIDER_ERROR
	    && output.last_close.basic_error == 57U
	    && output.last_close.operation_count == 1U
	    && output.last_close.registered && output.last_close.handle_open
	    && output.file != NULL && output.orphaned_file == NULL);
	yt_text_output_destroy(&output);

	/* Missing explicit CLOSE and empty CLOSE-all retain distinct identities. */
	yt_text_output_init(&output);
	CHECK(yt_text_output_close(&output, &error)
	    && output.last_close.missing && !output.last_close.close_all
	    && output.last_close.operation_count == 0U);
	CHECK(yt_text_output_close_all_method(&output, 0, &error)
	    && !output.last_close.missing && output.last_close.close_all
	    && output.last_close.operation_count == 0U);
	CHECK(!yt_text_output_close_all_method(&output, -1, &error)
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
test_database_random_lof(void)
{
	static const enum yt_database_lof_operation operations[] = {
		YT_DATABASE_LOF_CURRENT,
		YT_DATABASE_LOF_END,
		YT_DATABASE_LOF_RESTORE,
	};
	struct database_lof_script script;
	struct yt_database database;
	struct yt_error error;
	uint32_t length;
	unsigned dos_error;
	size_t failed;

	memset(&database, 0, sizeof(database));
	database.file = tmpfile();
	CHECK(database.file != NULL);
	if (database.file == NULL)
		return;
	(void)snprintf(database.path, sizeof(database.path), "%s", "LOF.DAT");
	memset(&script, 0, sizeof(script));
	database_lof_add(&script, YT_DATABASE_LOF_CURRENT, 0U, false, 0U,
	    0x234);
	database_lof_add(&script, YT_DATABASE_LOF_END, 0U, false, 0U,
	    0x12345);
	database_lof_add(&script, YT_DATABASE_LOF_RESTORE, 0x234U, false, 0U,
	    0x234);
	yt_database_set_lof_provider(&database, scripted_database_lof, &script);
	yt_error_clear(&error);
	CHECK(yt_database_random_lof(&database, &length, &error)
	    && length == 0x12345U && script.position == script.length
	    && database.last_lof.outcome == YT_DATABASE_LOF_RETURNED
	    && database.last_lof.operation_count == 3U
	    && database.last_lof.saved_position == 0x234U
	    && database.last_lof.length == 0x12345U
	    && database.last_lof.terminal_position == 0x234
	    && database.last_lof.registered && database.last_lof.handle_open);

	/* Each seek ordinal accepts every DOS error and retains its prefix. */
	for (failed = 0U; failed < YT_ARRAY_LEN(operations); ++failed) {
		for (dos_error = 1U; dos_error <= 0xffU; ++dos_error) {
			memset(&script, 0, sizeof(script));
			if (failed > 0U)
				database_lof_add(&script, YT_DATABASE_LOF_CURRENT,
				    0U, false, 0U, 0x234);
			if (failed > 1U)
				database_lof_add(&script, YT_DATABASE_LOF_END,
				    0U, false, 0U, 0x12345);
			database_lof_add(&script, operations[failed],
			    failed == 2U ? 0x234U : 0U, true,
			    (uint16_t)dos_error, 0x345);
			yt_database_set_lof_provider(&database,
			    scripted_database_lof, &script);
			yt_error_clear(&error);
			length = UINT32_MAX;
			CHECK(!yt_database_random_lof(&database, &length, &error)
			    && length == 0U && error.status == YT_IO_ERROR
			    && database.last_lof.outcome
			    == YT_DATABASE_LOF_SEEK_ERROR
			    && database.last_lof.failed_operation
			    == operations[failed]
			    && database.last_lof.dos_error == dos_error
			    && database.last_lof.basic_error == 52U
			    && database.last_lof.operation_count == failed + 1U
			    && database.last_lof.saved_position
			    == (failed > 0U ? 0x234U : 0U)
			    && database.last_lof.length
			    == (failed > 1U ? 0x12345U : 0U)
			    && database.last_lof.terminal_position == 0x345
			    && database.last_lof.registered
			    && database.last_lof.handle_open);
		}
	}

	/* Rejected and malformed provider results do not masquerade as DOS. */
	memset(&script, 0, sizeof(script));
	database_lof_add(&script, YT_DATABASE_LOF_CURRENT, 0U, false, 0U, 0U);
	script.steps[0].provider_ok = false;
	yt_database_set_lof_provider(&database, scripted_database_lof, &script);
	CHECK(!yt_database_random_lof(&database, &length, &error)
	    && database.last_lof.outcome == YT_DATABASE_LOF_PROVIDER_ERROR
	    && database.last_lof.failed_operation == YT_DATABASE_LOF_CURRENT);
	memset(&script, 0, sizeof(script));
	database_lof_add(&script, YT_DATABASE_LOF_CURRENT, 0U, false, 0U,
	    (int64_t)UINT32_MAX + 1);
	yt_database_set_lof_provider(&database, scripted_database_lof, &script);
	CHECK(!yt_database_random_lof(&database, &length, &error)
	    && database.last_lof.outcome == YT_DATABASE_LOF_PROVIDER_ERROR);

	/* Device random files return their position window without a seek. */
	memset(&script, 0, sizeof(script));
	database.last_open.device = true;
	database.device_position = UINT32_MAX;
	yt_database_set_lof_provider(&database, scripted_database_lof, &script);
	CHECK(yt_database_random_lof(&database, &length, &error)
	    && length == UINT32_MAX && script.position == 0U
	    && database.last_lof.device
	    && database.last_lof.operation_count == 0U
	    && database.last_lof.saved_position == UINT32_MAX
	    && database.last_lof.terminal_position == UINT32_MAX);
	yt_database_close(&database);

	/* The host adapter restores the physical cursor after measuring EOF. */
	memset(&database, 0, sizeof(database));
	database.file = tmpfile();
	CHECK(database.file != NULL);
	if (database.file != NULL) {
		CHECK(fwrite("abcde", 1U, 5U, database.file) == 5U
		    && fseek(database.file, 2L, SEEK_SET) == 0
		    && yt_database_random_lof(&database, &length, &error)
		    && length == 5U && ftell(database.file) == 2L
		    && database.last_lof.saved_position == 2U
		    && database.last_lof.operation_count == 3U);
	}
	yt_database_close(&database);
	CHECK(!yt_database_random_lof(NULL, &length, &error)
	    && error.status == YT_INVALID);
}

static bool
scripted_database_read(void *context, FILE *file, uint8_t *data,
    size_t requested, struct yt_database_read_observation *observation)
{
	struct database_read_script *script = context;
	size_t count = script->accepted < requested ? script->accepted : requested;

	(void)file;
	++script->calls;
	script->requested = requested;
	memset(observation, 0, sizeof(*observation));
	if (script->provider_reject)
		return false;
	if (count != 0U && script->data != NULL)
		memcpy(data, script->data, count);
	observation->accepted = count;
	observation->carry = script->carry;
	observation->dos_error = script->dos_error;
	observation->mapped_error = script->mapped_error;
	observation->terminal_position = script->terminal_position;
	return true;
}

static bool
scripted_database_write(void *context, FILE *file, const uint8_t *data,
    size_t requested, struct yt_database_write_observation *observation)
{
	struct database_write_script *script = context;
	size_t count = script->accepted < requested ? script->accepted : requested;

	++script->calls;
	script->requested = requested;
	memset(observation, 0, sizeof(*observation));
	if (script->provider_reject)
		return false;
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
	memset(observation, 0, sizeof(*observation));
	if (script->provider_reject)
		return false;
	observation->carry = !script->success;
	observation->dos_error = script->success ? 0U : script->dos_error;
	observation->terminal_position = script->terminal_position;
	return true;
}

static bool
scripted_database_close(void *context, FILE *file, size_t attempt,
    struct yt_database_close_observation *observation)
{
	struct database_close_script *script = context;

	++script->calls;
	script->attempt = attempt;
	memset(observation, 0, sizeof(*observation));
	if (!script->success) {
		observation->carry = true;
		observation->dos_error = 6U;
		observation->handle_open = script->handle_open;
		return true;
	}
	if (fclose(file) != 0)
		return false;
	return true;
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
	struct database_read_script read_script;
	struct database_write_script write_script;
	struct database_seek_script seek_script;
	struct database_close_script close_script;
	struct database_public_close_script short_close_script;
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
	CHECK(database.last_get.outcome == YT_DATABASE_GET_RETURNED
	    && database.last_get.accepted == YT_RECORD_SIZE
	    && database.last_get.full_record
	    && database.last_get.terminal_position == YT_RECORD_SIZE
	    && database.last_get.registered && database.last_get.handle_open);
	for (index = 0U; index < sizeof(replacement.bytes); ++index)
		replacement.bytes[index] = (uint8_t)(index ^ 0xa5U);
	seek_script = (struct database_seek_script){.success = true};
	read_script = (struct database_read_script){.accepted = YT_RECORD_SIZE};
	yt_database_set_seek_provider(&database, scripted_database_seek,
	    &seek_script);
	yt_database_set_read_provider(&database, scripted_database_read,
	    &read_script);
	after = replacement;
	accepted = 99U;
	yt_error_clear(&error);
	CHECK(!yt_database_random_get(&database, 0U, &after, &accepted, &error)
	    && error.status == YT_RANGE && accepted == 0U
	    && seek_script.calls == 0U && read_script.calls == 0U
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
	    && seek_script.calls == 0U && read_script.calls == 0U
	    && memcmp(after.bytes, replacement.bytes, YT_RECORD_SIZE) == 0
	    && database.last_get.outcome == YT_DATABASE_GET_RECORD_ERROR
	    && database.last_get.basic_error == 63U);
	seek_script = (struct database_seek_script){.success = true};
	read_script = (struct database_read_script){0};
	CHECK(yt_database_random_get(&database, 0xFFFFFFU, &after, &accepted,
	    &error) && accepted == 0U && seek_script.calls == 1U
	    && seek_script.absolute_offset
	    == (int64_t)(0xFFFFFFU - 1U) * YT_RECORD_SIZE
	    && read_script.calls == 1U
	    && database.last_get.outcome == YT_DATABASE_GET_RETURNED
	    && database.last_get.current_record == 0xFFFFFFU
	    && database.last_get.record_index == 0xFFFFFEU
	    && database.last_get.desired_offset
	    == (int64_t)(0xFFFFFFU - 1U) * YT_RECORD_SIZE
	    && database.last_get.terminal_position
	    == (int64_t)(0xFFFFFFU - 1U) * YT_RECORD_SIZE);
	yt_database_set_seek_provider(&database, NULL, NULL);
	yt_database_set_read_provider(&database, NULL, NULL);
	CHECK(yt_database_random_get(&database, 0xFFFFFFU, &after, &accepted,
	    &error) && accepted == 0U
	    && database.last_get.desired_offset
	    == (int64_t)(0xFFFFFFU - 1U) * YT_RECORD_SIZE
	    && database.last_get.terminal_position
	    == (int64_t)(0xFFFFFFU - 1U) * YT_RECORD_SIZE);
	read_script = (struct database_read_script){
		.data = replacement.bytes,
		.accepted = 3U,
	};
	yt_database_set_read_provider(&database, scripted_database_read,
	    &read_script);
	CHECK(yt_database_read(&database, 1U, &after, &error)
	    && read_script.calls == 1U && read_script.requested == YT_RECORD_SIZE);
	CHECK(memcmp(after.bytes, replacement.bytes, 3U) == 0);
	for (index = 3U; index < sizeof(after.bytes); ++index)
		CHECK(after.bytes[index] == 0U);
	CHECK(database.last_get.outcome == YT_DATABASE_GET_RETURNED
	    && database.last_get.accepted == 3U
	    && !database.last_get.full_record
	    && database.last_get.terminal_position == 3);
	read_script = (struct database_read_script){0};
	CHECK(yt_database_random_get(&database, 1U, &after, &accepted, &error)
	    && accepted == 0U && read_script.calls == 1U);
	for (index = 0U; index < sizeof(after.bytes); ++index)
		CHECK(after.bytes[index] == 0U);
	CHECK(database.last_get.outcome == YT_DATABASE_GET_RETURNED
	    && database.last_get.accepted == 0U
	    && !database.last_get.full_record
	    && database.last_get.terminal_position == 0);
	yt_database_set_read_provider(&database, NULL, NULL);
	read_script = (struct database_read_script){
		.data = replacement.bytes,
		.accepted = 3U,
		.carry = true,
		.dos_error = 6U,
		.terminal_position = 0x55667788,
	};
	yt_database_set_read_provider(&database, scripted_database_read,
	    &read_script);
	yt_error_clear(&error);
	CHECK(!yt_database_random_get(&database, 1U, &after, &accepted, &error)
	    && error.status == YT_IO_ERROR && accepted == 3U);
	CHECK(memcmp(after.bytes, replacement.bytes, 3U) == 0);
	for (index = 3U; index < sizeof(after.bytes); ++index)
		CHECK(after.bytes[index] == 0U);
	CHECK(database.last_get.outcome == YT_DATABASE_GET_READ_ERROR
	    && database.last_get.accepted == 3U
	    && database.last_get.dos_error == 6U
	    && database.last_get.basic_error == 57U
	    && database.last_get.terminal_position == 0x55667788
	    && database.last_get.registered && database.last_get.handle_open);
	for (dos_error = 1U; dos_error <= 0xffU; ++dos_error) {
		read_script = (struct database_read_script){
			.carry = true,
			.dos_error = (uint16_t)dos_error,
			.mapped_error = dos_error == 5U ? 70U : 0U,
			.terminal_position = (int64_t)(0x3000U + dos_error),
		};
		yt_error_clear(&error);
		CHECK(!yt_database_random_get(&database, 1U, &after, &accepted,
		    &error) && error.status == YT_IO_ERROR && accepted == 0U
		    && read_script.calls == 1U);
		CHECK(database.last_get.outcome == YT_DATABASE_GET_READ_ERROR
		    && database.last_get.dos_error == dos_error
		    && database.last_get.basic_error
		    == (dos_error == 5U ? 70U : 57U)
		    && database.last_get.terminal_position
		    == (int64_t)(0x3000U + dos_error)
		    && database.last_get.registered
		    && database.last_get.handle_open);
	}
	read_script = (struct database_read_script){
		.carry = true,
		.dos_error = 5U,
		.mapped_error = 75U,
		.terminal_position = 0x3750,
	};
	CHECK(!yt_database_random_get(&database, 1U, &after, &accepted, &error)
	    && database.last_get.basic_error == 75U
	    && database.last_get.dos_error == 5U
	    && database.last_get.terminal_position == 0x3750);
	yt_database_set_read_provider(&database, NULL, NULL);
	seek_script = (struct database_seek_script){0};
	read_script = (struct database_read_script){.accepted = 137U};
	yt_database_set_seek_provider(&database, scripted_database_seek,
	    &seek_script);
	yt_database_set_read_provider(&database, scripted_database_read,
	    &read_script);
	for (dos_error = 1U; dos_error <= 0xffU; ++dos_error) {
		seek_script = (struct database_seek_script){
			.dos_error = (uint16_t)dos_error,
			.terminal_position = (int64_t)(0x5000U + dos_error),
		};
		yt_error_clear(&error);
		CHECK(!yt_database_random_get(&database, 1U, &after, &accepted,
		    &error) && error.status == YT_IO_ERROR && accepted == 0U
		    && seek_script.calls == 1U && read_script.calls == 0U);
		CHECK(database.last_get.outcome == YT_DATABASE_GET_SEEK_ERROR
		    && database.last_get.dos_error == dos_error
		    && database.last_get.basic_error == 52U
		    && database.last_get.terminal_position
		    == (int64_t)(0x5000U + dos_error)
		    && database.last_get.registered
		    && database.last_get.handle_open);
	}
	seek_script = (struct database_seek_script){
		.success = true,
		.provider_reject = true,
	};
	read_script = (struct database_read_script){.accepted = YT_RECORD_SIZE};
	accepted = 99U;
	CHECK(!yt_database_random_get(&database, 1U, &after, &accepted, &error)
	    && accepted == 0U && seek_script.calls == 1U
	    && read_script.calls == 0U
	    && database.last_get.outcome == YT_DATABASE_GET_PROVIDER_ERROR
	    && database.last_get.basic_error == 52U
	    && database.last_get.dos_error == 0U
	    && database.last_get.registered && database.last_get.handle_open);
	seek_script = (struct database_seek_script){
		.success = false,
		.dos_error = 0U,
		.terminal_position = 0x5678,
	};
	CHECK(!yt_database_random_get(&database, 1U, &after, &accepted, &error)
	    && seek_script.calls == 1U && read_script.calls == 0U
	    && database.last_get.outcome == YT_DATABASE_GET_PROVIDER_ERROR
	    && database.last_get.basic_error == 52U
	    && database.last_get.dos_error == 0U
	    && database.last_get.terminal_position == 0x5678);
	yt_database_set_seek_provider(&database, NULL, NULL);
	read_script = (struct database_read_script){.provider_reject = true};
	accepted = 99U;
	after = replacement;
	CHECK(!yt_database_random_get(&database, 1U, &after, &accepted, &error)
	    && accepted == 0U && read_script.calls == 1U
	    && database.last_get.outcome == YT_DATABASE_GET_PROVIDER_ERROR
	    && database.last_get.basic_error == 57U
	    && database.last_get.dos_error == 0U
	    && database.last_get.accepted == 0U);
	for (index = 0U; index < sizeof(after.bytes); ++index)
		CHECK(after.bytes[index] == 0U);
	read_script = (struct database_read_script){.dos_error = 1U};
	CHECK(!yt_database_random_get(&database, 1U, &after, &accepted, &error)
	    && database.last_get.outcome == YT_DATABASE_GET_PROVIDER_ERROR
	    && database.last_get.basic_error == 57U
	    && database.last_get.dos_error == 1U);
	read_script = (struct database_read_script){
		.carry = true,
		.dos_error = 5U,
	};
	CHECK(!yt_database_random_get(&database, 1U, &after, &accepted, &error)
	    && database.last_get.outcome == YT_DATABASE_GET_PROVIDER_ERROR
	    && database.last_get.basic_error == 57U
	    && database.last_get.dos_error == 5U);
	yt_database_set_read_provider(&database, NULL, NULL);
	seek_script = (struct database_seek_script){.success = true};
	write_script = (struct database_write_script){.accepted = YT_RECORD_SIZE};
	yt_database_set_seek_provider(&database, scripted_database_seek,
	    &seek_script);
	yt_database_set_write_provider(&database, scripted_database_write,
	    &write_script);
	accepted = 99U;
	CHECK(!yt_database_random_put(&database, 0U, &replacement, false,
	    &accepted, &error) && error.status == YT_RANGE && accepted == 0U
	    && seek_script.calls == 0U && write_script.calls == 0U
	    && database.last_put.outcome == YT_DATABASE_PUT_RECORD_ERROR
	    && database.last_put.basic_error == 63U
	    && database.last_put.current_record == 0U
	    && database.last_put.record_index == 0U
	    && database.last_put.desired_offset == 0
	    && database.last_put.registered && database.last_put.handle_open);
	accepted = 99U;
	CHECK(!yt_database_random_put(&database, 0x1000000U, &replacement,
	    false, &accepted, &error) && error.status == YT_RANGE
	    && accepted == 0U && seek_script.calls == 0U
	    && write_script.calls == 0U
	    && database.last_put.outcome == YT_DATABASE_PUT_RECORD_ERROR
	    && database.last_put.basic_error == 63U);
	yt_database_set_seek_provider(&database, NULL, NULL);
	yt_database_set_write_provider(&database, NULL, NULL);
	write_script = (struct database_write_script){.accepted = 136U};
	yt_database_set_write_provider(&database, scripted_database_write,
	    &write_script);
	CHECK(yt_database_random_put(&database, 1U, &replacement, true,
	    &accepted, &error));
	CHECK(write_script.calls == 1U
	    && write_script.requested == YT_RECORD_SIZE && accepted == 136U);
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
	CHECK(!yt_database_write(&database, 1U, &replacement, &error)
	    && error.status == YT_IO_ERROR
	    && strcmp(error.operation, "write record") == 0);
	CHECK(database.file == NULL && database.orphaned_file == NULL
	    && database.records == 0U && database.short_close_attempted
	    && database.short_close_succeeded);
	CHECK(database.last_put.outcome == YT_DATABASE_PUT_REJECTED_SHORT
	    && database.last_put.accepted == 136U
	    && database.last_put.basic_error == 61U
	    && database.last_put.close_dos_error == 0U
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
	close_script = (struct database_close_script){
		.success = false,
		.handle_open = true,
	};
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
	    && database.last_put.close_dos_error == 6U
	    && !database.last_put.registered
	    && database.last_put.close_attempted
	    && !database.last_put.close_succeeded
	    && database.last_put.handle_open);
	CHECK(yt_database_open(&observer, database_path, YT_OPEN_READ, &error));
	CHECK(fputc(0, observer.file) == EOF && ferror(observer.file));
	CHECK(yt_database_read(&observer, 1U, &after, &error));
	CHECK(!ferror(observer.file));
	CHECK(memcmp(after.bytes, replacement.bytes, 3U) == 0
	    && memcmp(after.bytes + 3U, before.bytes + 3U,
	    YT_RECORD_SIZE - 3U) == 0);
	yt_database_close(&observer);
	yt_database_close(&database);
	CHECK(yt_database_open(&database, database_path, YT_OPEN_UPDATE, &error));
	for (dos_error = 1U; dos_error <= 0xffU; ++dos_error) {
		write_script = (struct database_write_script){.accepted = 3U};
		memset(&short_close_script, 0, sizeof(short_close_script));
		database_close_add(&short_close_script, true,
		    (uint16_t)dos_error, false, true, true);
		yt_database_set_write_provider(&database,
		    scripted_database_write, &write_script);
		yt_database_set_close_provider(&database,
		    scripted_database_public_close, &short_close_script);
		CHECK(!yt_database_random_put(&database, 1U, &replacement,
		    false, &accepted, &error)
		    && short_close_script.position == short_close_script.length
		    && database.last_put.outcome
		    == YT_DATABASE_PUT_REJECTED_SHORT
		    && database.last_put.close_dos_error == dos_error
		    && !database.last_put.registered
		    && database.last_put.close_attempted
		    && !database.last_put.close_succeeded
		    && !database.last_put.handle_open);
		CHECK(yt_database_open(&database, database_path,
		    YT_OPEN_UPDATE, &error));
	}
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
	seek_script = (struct database_seek_script){
		.success = true,
		.provider_reject = true,
	};
	write_script = (struct database_write_script){
		.accepted = YT_RECORD_SIZE,
	};
	accepted = 99U;
	CHECK(!yt_database_random_put(&database, 1U, &replacement, true,
	    &accepted, &error) && accepted == 0U && seek_script.calls == 1U
	    && write_script.calls == 0U
	    && database.last_put.outcome == YT_DATABASE_PUT_PROVIDER_ERROR
	    && database.last_put.basic_error == 52U
	    && database.last_put.dos_error == 0U
	    && database.last_put.registered && database.last_put.handle_open
	    && !database.last_put.close_attempted);
	seek_script = (struct database_seek_script){
		.success = false,
		.dos_error = 0U,
		.terminal_position = 0x6789,
	};
	CHECK(!yt_database_random_put(&database, 1U, &replacement, true,
	    &accepted, &error) && seek_script.calls == 1U
	    && write_script.calls == 0U
	    && database.last_put.outcome == YT_DATABASE_PUT_PROVIDER_ERROR
	    && database.last_put.basic_error == 52U
	    && database.last_put.dos_error == 0U
	    && database.last_put.terminal_position == 0x6789);
	yt_database_set_seek_provider(&database, NULL, NULL);
	write_script = (struct database_write_script){.provider_reject = true};
	accepted = 99U;
	CHECK(!yt_database_random_put(&database, 1U, &replacement, true,
	    &accepted, &error) && accepted == 0U && write_script.calls == 1U
	    && database.last_put.outcome == YT_DATABASE_PUT_PROVIDER_ERROR
	    && database.last_put.basic_error == 57U
	    && database.last_put.dos_error == 0U
	    && database.last_put.accepted == 0U
	    && database.last_put.registered && database.last_put.handle_open
	    && !database.last_put.close_attempted);
	write_script = (struct database_write_script){.dos_error = 1U};
	CHECK(!yt_database_random_put(&database, 1U, &replacement, true,
	    &accepted, &error)
	    && database.last_put.outcome == YT_DATABASE_PUT_PROVIDER_ERROR
	    && database.last_put.basic_error == 57U
	    && database.last_put.dos_error == 1U);
	write_script = (struct database_write_script){
		.carry = true,
		.dos_error = 5U,
	};
	CHECK(!yt_database_random_put(&database, 1U, &replacement, true,
	    &accepted, &error)
	    && database.last_put.outcome == YT_DATABASE_PUT_PROVIDER_ERROR
	    && database.last_put.basic_error == 57U
	    && database.last_put.dos_error == 5U);
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

	/* A durable native write is one PUT boundary with ordered host steps. */
	flush_script = (struct database_flush_script){true, 0U};
	yt_database_set_flush_provider(&database, scripted_database_flush,
	    &flush_script);
	write_script = (struct database_write_script){
		.accepted = 3U,
		.carry = true,
		.dos_error = 6U,
		.terminal_position = 3,
	};
	yt_error_clear(&error);
	CHECK(!yt_database_write_durable(&database, 1U, &replacement, &error)
	    && write_script.calls == 1U && flush_script.calls == 0U
	    && database.last_put.outcome == YT_DATABASE_PUT_WRITE_ERROR
	    && database.last_put.accepted == 3U
	    && strcmp(error.operation, "write record") == 0);
	write_script = (struct database_write_script){
		.accepted = YT_RECORD_SIZE,
	};
	flush_script = (struct database_flush_script){false, 0U};
	yt_error_clear(&error);
	CHECK(!yt_database_write_durable(&database, 1U, &replacement, &error)
	    && write_script.calls == 1U && flush_script.calls == 1U
	    && database.last_put.outcome == YT_DATABASE_PUT_RETURNED
	    && database.last_put.accepted == YT_RECORD_SIZE
	    && strcmp(error.operation, "flush database") == 0);
	write_script = (struct database_write_script){
		.accepted = YT_RECORD_SIZE,
	};
	flush_script = (struct database_flush_script){true, 0U};
	yt_error_clear(&error);
	CHECK(yt_database_write_durable(&database, 1U, &replacement, &error)
	    && write_script.calls == 1U && flush_script.calls == 1U
	    && database.last_put.outcome == YT_DATABASE_PUT_RETURNED);

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
	char sized_path[320];
	char failed_path[320];
	uint8_t partial[3] = {0x11, 0x22, 0x33};
	uint8_t complete[YT_RADIO_RECORD_SIZE];
	struct yt_radio_file radio;
	struct yt_radio_record record;
	struct yt_radio_record written;
	struct database_public_close_script close_script;
	struct database_read_script read_script;
	struct database_write_script write_script;
	struct database_seek_script seek_script;
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
	seek_script = (struct database_seek_script){.success = true};
	read_script = (struct database_read_script){
		.accepted = YT_RADIO_RECORD_SIZE,
	};
	write_script = (struct database_write_script){
		.accepted = YT_RADIO_RECORD_SIZE,
	};
	yt_database_set_seek_provider(&radio.random, scripted_database_seek,
	    &seek_script);
	yt_database_set_read_provider(&radio.random, scripted_database_read,
	    &read_script);
	yt_database_set_write_provider(&radio.random, scripted_database_write,
	    &write_script);
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
	    && radio.random.last_put.basic_error == 63U
	    && seek_script.calls == 0U && read_script.calls == 0U
	    && write_script.calls == 0U);
	for (index = 0U; index < sizeof(record.bytes); ++index)
		CHECK(record.bytes[index] == 0xffU);
	yt_database_set_seek_provider(&radio.random, NULL, NULL);
	yt_database_set_read_provider(&radio.random, NULL, NULL);
	yt_database_set_write_provider(&radio.random, NULL, NULL);
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
	memset(&close_script, 0, sizeof(close_script));
	database_close_add(&close_script, false, 0U, false, true, true);
	yt_database_set_close_provider(&radio.random,
	    scripted_database_public_close, &close_script);
	CHECK(yt_radio_file_open(&radio, second_path, &error));
	CHECK(close_script.position == close_script.length
	    && strcmp(radio.random.path, second_path) == 0
	    && yt_radio_file_size(&radio, &size, &error) && size == 0U);
	CHECK(yt_radio_file_next_record(&radio, &next, &error) && next == 1U);
	CHECK(yt_radio_message_record(&written, (const uint8_t *)"A\0B", 3U,
	    7.0f, -2.0f));
	for (index = 0U; index <= YT_RADIO_RECORD_SIZE; ++index) {
		read_script = (struct database_read_script){
			.data = written.bytes,
			.accepted = index,
		};
		yt_database_set_read_provider(&radio.random,
		    scripted_database_read, &read_script);
		memset(&record, 0xff, sizeof(record));
		CHECK(yt_radio_file_get(&radio, 1U, &record, &accepted, &error)
		    && accepted == index && read_script.calls == 1U
		    && read_script.requested == YT_RADIO_RECORD_SIZE
		    && radio.random.last_get.outcome
		    == YT_DATABASE_GET_RETURNED
		    && radio.random.last_get.accepted == index
		    && radio.random.last_get.terminal_position == (int64_t)index
		    && radio.random.last_get.full_record
		    == (index == YT_RADIO_RECORD_SIZE));
		CHECK(memcmp(record.bytes, written.bytes, index) == 0);
		for (accepted = index; accepted < sizeof(record.bytes); ++accepted)
			CHECK(record.bytes[accepted] == 0U);
	}
	yt_database_set_read_provider(&radio.random, NULL, NULL);
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

	seek_script = (struct database_seek_script){
		.success = false,
		.dos_error = 19U,
		.terminal_position = 0x1234,
	};
	yt_database_set_seek_provider(&radio.random, scripted_database_seek,
	    &seek_script);
	CHECK(!yt_radio_file_get(&radio, 1U, &record, &accepted, &error)
	    && accepted == 0U && seek_script.calls == 1U
	    && radio.random.last_get.outcome == YT_DATABASE_GET_SEEK_ERROR
	    && radio.random.last_get.basic_error == 52U
	    && radio.random.last_get.dos_error == 19U
	    && radio.random.last_get.terminal_position == 0x1234);
	yt_database_set_seek_provider(&radio.random, NULL, NULL);
	read_script = (struct database_read_script){
		.data = written.bytes,
		.accepted = 3U,
		.carry = true,
		.dos_error = 6U,
		.terminal_position = 0x2345,
	};
	yt_database_set_read_provider(&radio.random, scripted_database_read,
	    &read_script);
	CHECK(!yt_radio_file_get(&radio, 1U, &record, &accepted, &error)
	    && accepted == 3U
	    && radio.random.last_get.outcome == YT_DATABASE_GET_READ_ERROR
	    && radio.random.last_get.basic_error == 57U
	    && radio.random.last_get.dos_error == 6U
	    && radio.random.last_get.terminal_position == 0x2345
	    && memcmp(record.bytes, written.bytes, 3U) == 0);
	for (index = 3U; index < sizeof(record.bytes); ++index)
		CHECK(record.bytes[index] == 0U);
	yt_database_set_read_provider(&radio.random, NULL, NULL);
	write_script = (struct database_write_script){
		.accepted = 3U,
		.carry = true,
		.dos_error = 6U,
		.terminal_position = 0x3456,
	};
	yt_database_set_write_provider(&radio.random, scripted_database_write,
	    &write_script);
	CHECK(!yt_radio_file_put(&radio, 1U, &written, &error)
	    && radio.random.last_put.outcome == YT_DATABASE_PUT_WRITE_ERROR
	    && radio.random.last_put.accepted == 3U
	    && radio.random.last_put.basic_error == 57U
	    && radio.random.last_put.dos_error == 6U
	    && radio.random.last_put.terminal_position == 0x3456
	    && radio.random.last_put.registered
	    && radio.random.last_put.handle_open);
	yt_database_set_write_provider(&radio.random, NULL, NULL);
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
	CHECK(yt_radio_file_open(&radio, second_path, &error));
	memset(&close_script, 0, sizeof(close_script));
	database_close_add(&close_script, true, 5U, true, true, false);
	database_close_add(&close_script, false, 0U, false, true, true);
	yt_database_set_close_provider(&radio.random,
	    scripted_database_public_close, &close_script);
	yt_error_clear(&error);
	CHECK(!yt_radio_file_close(&radio, &error)
	    && error.status == YT_IO_ERROR
	    && strcmp(error.operation, "random CLOSE") == 0
	    && close_script.position == close_script.length
	    && radio.random.file == NULL
	    && radio.random.orphaned_file == NULL
	    && radio.random.last_close.outcome == YT_DATABASE_CLOSE_DISK_ERROR
	    && radio.random.last_close.basic_error == 70U
	    && radio.random.last_close.dos_error == 5U
	    && radio.random.last_close.attempt_count == 2U
	    && radio.random.last_close.retry_attempted
	    && radio.record_length == 0U && radio.field_count == 0U);
	yt_database_close(&radio.random);
	for (index = 0U; index < YT_RADIO_RECORD_SIZE; ++index) {
		CHECK(yt_radio_file_open(&radio, second_path, &error));
		write_script = (struct database_write_script){
			.accepted = index,
		};
		memset(&close_script, 0, sizeof(close_script));
		database_close_add(&close_script, false, 0U, false, true, true);
		yt_database_set_write_provider(&radio.random,
		    scripted_database_write, &write_script);
		yt_database_set_close_provider(&radio.random,
		    scripted_database_public_close, &close_script);
		CHECK(!yt_radio_file_put(&radio, 1U, &written, &error)
		    && close_script.position == close_script.length
		    && radio.random.last_put.outcome
		    == YT_DATABASE_PUT_REJECTED_SHORT
		    && radio.random.last_put.accepted == index
		    && radio.random.last_put.basic_error == 61U
		    && radio.random.last_put.terminal_position == (int64_t)index
		    && !radio.random.last_put.registered
		    && radio.random.last_put.close_attempted
		    && radio.random.last_put.close_succeeded
		    && !radio.random.last_put.handle_open);
	}
	CHECK(yt_radio_file_close(&radio, &error));
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
text_append_existing_prefix(struct text_open_script *script,
    int64_t length)
{
	text_open_add_file(script, YT_TEXT_OPEN_EXISTING, 2U);
	text_open_add_query(script, 2U, false, 0U, false);
	text_open_add_seek(script, YT_TEXT_OPEN_SEEK_END, 2U, 0,
	    length);
	if (length != 0)
		text_open_add_seek(script, YT_TEXT_OPEN_SEEK_WINDOW,
		    2U, length > YT_TEXT_OUTPUT_BUFFER_SIZE
		    ? length - YT_TEXT_OUTPUT_BUFFER_SIZE : 0,
		    length > YT_TEXT_OUTPUT_BUFFER_SIZE
		    ? length - YT_TEXT_OUTPUT_BUFFER_SIZE : 0);
}

static void
text_append_missing_closed_prefix(struct text_open_script *script)
{
	struct text_open_step *step;

	text_open_add_error(script, YT_TEXT_OPEN_EXISTING, 2U,
	    0U, 2U, 0U);
	text_open_add_file(script, YT_TEXT_OPEN_CREATE, 2U);
	step = text_open_add(script, YT_TEXT_OPEN_TEMP_CLOSE, 0U,
	    0U);
	if (step != NULL)
		step->close_active = true;
}

static void
text_output_missing_closed_prefix(struct text_open_script *script)
{
	struct text_open_step *step;

	text_open_add_error(script, YT_TEXT_OPEN_EXISTING, 1U,
	    0U, 2U, 0U);
	text_open_add_file(script, YT_TEXT_OPEN_CREATE, 1U);
	step = text_open_add(script, YT_TEXT_OPEN_TEMP_CLOSE, 0U,
	    0U);
	if (step != NULL)
		step->close_active = true;
}

static void
test_input_read_model(void)
{
	static const uint8_t registration[] =
	    "This BBS\r\nThe Sysop\r\n3484625551\r\n";
	static const uint8_t two_lines[] = "A\r\nB\r\n";
	static const uint8_t adjacent[] = "A\rB\r";
	static const uint8_t bare_lf[] = {'A', 0U, '\n', 'B', '\r', '\n'};
	static const uint8_t nul_only[] = {0U};
	static const uint8_t marker[] = "\x1aignored";
	struct text_input_read_script script;
	struct text_input_read_step *step;
	struct yt_text_input input;
	struct yt_error error;
	const uint8_t *line;
	size_t length;
	bool available;
	bool eof;
	uint8_t bytes[YT_TEXT_INPUT_BUFFER_SIZE];
	unsigned value;
	size_t accepted;
	size_t index;

	/* The shipped registration records share one 128-byte DOS refill. */
	memset(&script, 0, sizeof(script));
	text_input_read_add(&script, registration, sizeof(registration) - 1U,
	    false, 0U, 0U, (int64_t)(sizeof(registration) - 1U));
	CHECK(text_input_read_fixture(&input, &script));
	CHECK(yt_text_input_read_line(&input, &line, &length, &available,
	    &error) && available && length == 8U
	    && memcmp(line, "This BBS", 8U) == 0
	    && input.last_read.operation_count == 1U
	    && input.last_read.accepted == sizeof(registration) - 1U
	    && input.last_read.consumed == 10U
	    && input.last_read.returned == 8U
	    && input.last_read.refill_index == 1U
	    && input.last_read.buffer_total == sizeof(registration) - 1U
	    && input.last_read.buffer_remaining == 23U
	    && input.last_read.logical_position == 10U
	    && input.last_read.terminal_position
	    == (int64_t)(sizeof(registration) - 1U));
	CHECK(yt_text_input_read_line(&input, &line, &length, &available,
	    &error) && available && length == 9U
	    && memcmp(line, "The Sysop", 9U) == 0
	    && input.last_read.operation_count == 0U
	    && input.last_read.consumed == 11U
	    && input.last_read.buffer_remaining == 12U
	    && input.last_read.logical_position == 21U);
	CHECK(yt_text_input_read_line(&input, &line, &length, &available,
	    &error) && available && length == 10U
	    && memcmp(line, "3484625551", 10U) == 0
	    && input.last_read.operation_count == 0U
	    && input.last_read.consumed == 12U
	    && input.last_read.buffer_remaining == 0U
	    && input.last_read.logical_position == sizeof(registration) - 1U
	    && script.position == 1U);
	yt_text_input_destroy(&input);

	/* EOF fills and unreads; LINE INPUT consumes the same live buffer. */
	memset(&script, 0, sizeof(script));
	text_input_read_add(&script, two_lines, sizeof(two_lines) - 1U,
	    false, 0U, 0U, (int64_t)(sizeof(two_lines) - 1U));
	CHECK(text_input_read_fixture(&input, &script));
	CHECK(yt_text_input_eof(&input, &eof, &error) && !eof
	    && input.last_read.eof_probe
	    && input.last_read.operation_count == 1U
	    && input.read_remaining == sizeof(two_lines) - 1U
	    && input.logical_position == 0U
	    && input.physical_position == (int64_t)(sizeof(two_lines) - 1U));
	CHECK(yt_text_input_read_line(&input, &line, &length, &available,
	    &error) && available && length == 1U && line[0] == 'A'
	    && input.last_read.operation_count == 0U
	    && input.last_read.consumed == 3U
	    && input.read_remaining == 3U && input.logical_position == 3U
	    && script.position == 1U);
	yt_text_input_destroy(&input);

	/* A non-LF lookahead after CR remains unread for the next record. */
	memset(&script, 0, sizeof(script));
	text_input_read_add(&script, adjacent, sizeof(adjacent) - 1U,
	    false, 0U, 0U, (int64_t)(sizeof(adjacent) - 1U));
	text_input_read_add(&script, NULL, 0U, false, 0U, 0U,
	    (int64_t)(sizeof(adjacent) - 1U));
	CHECK(text_input_read_fixture(&input, &script));
	CHECK(yt_text_input_read_line(&input, &line, &length, &available,
	    &error) && available && length == 1U && line[0] == 'A'
	    && input.logical_position == 2U && input.read_remaining == 2U);
	CHECK(yt_text_input_read_line(&input, &line, &length, &available,
	    &error) && available && length == 1U && line[0] == 'B'
	    && input.logical_position == 4U && input.refill_index == 2U
	    && script.position == 2U);
	yt_text_input_destroy(&input);

	/* NUL is consumed, while a bare LF remains ordinary returned data. */
	memset(&script, 0, sizeof(script));
	text_input_read_add(&script, bare_lf, sizeof(bare_lf), false, 0U, 0U,
	    (int64_t)sizeof(bare_lf));
	CHECK(text_input_read_fixture(&input, &script));
	CHECK(yt_text_input_read_line(&input, &line, &length, &available,
	    &error) && available && length == 3U
	    && memcmp(line, "A\nB", 3U) == 0
	    && input.last_read.consumed == sizeof(bare_lf)
	    && input.last_read.returned == 3U);
	yt_text_input_destroy(&input);

	/* Discarded NUL still makes a final physical-EOF record available. */
	memset(&script, 0, sizeof(script));
	text_input_read_add(&script, nul_only, sizeof(nul_only), false, 0U, 0U,
	    (int64_t)sizeof(nul_only));
	text_input_read_add(&script, NULL, 0U, false, 0U, 0U,
	    (int64_t)sizeof(nul_only));
	CHECK(text_input_read_fixture(&input, &script));
	CHECK(yt_text_input_read_line(&input, &line, &length, &available,
	    &error) && available && length == 0U
	    && input.last_read.consumed == 1U
	    && input.last_read.returned == 0U
	    && input.refill_index == 2U && script.position == 2U);
	yt_text_input_destroy(&input);

	/* DOS text EOF is never consumed and avoids repeated physical reads. */
	memset(&script, 0, sizeof(script));
	text_input_read_add(&script, marker, sizeof(marker) - 1U,
	    false, 0U, 0U, (int64_t)(sizeof(marker) - 1U));
	CHECK(text_input_read_fixture(&input, &script));
	CHECK(yt_text_input_eof(&input, &eof, &error) && eof
	    && input.read_remaining == sizeof(marker) - 1U
	    && input.logical_position == 0U && script.position == 1U);
	CHECK(yt_text_input_eof(&input, &eof, &error) && eof
	    && input.read_remaining == sizeof(marker) - 1U
	    && input.logical_position == 0U && script.position == 1U);
	CHECK(yt_text_input_read_line(&input, &line, &length, &available,
	    &error) && !available && length == 0U
	    && input.read_remaining == sizeof(marker) - 1U
	    && input.last_read.eof && script.position == 1U);
	yt_text_input_destroy(&input);

	/* Physical EOF performs a new zero-byte refill on every probe. */
	memset(&script, 0, sizeof(script));
	text_input_read_add(&script, NULL, 0U, false, 0U, 0U, 0);
	text_input_read_add(&script, NULL, 0U, false, 0U, 0U, 0);
	CHECK(text_input_read_fixture(&input, &script));
	CHECK(yt_text_input_eof(&input, &eof, &error) && eof
	    && input.refill_index == 1U && script.position == 1U);
	CHECK(yt_text_input_eof(&input, &eof, &error) && eof
	    && input.refill_index == 2U && script.position == 2U);
	yt_text_input_destroy(&input);

	/* Every successful DOS short-count fills a zero-tailed 128-byte block. */
	memset(bytes, 'Q', sizeof(bytes));
	for (accepted = 0U; accepted <= sizeof(bytes); ++accepted) {
		memset(&script, 0, sizeof(script));
		text_input_read_add(&script, bytes, accepted, false, 0U, 0U,
		    (int64_t)accepted);
		CHECK(text_input_read_fixture(&input, &script));
		CHECK(yt_text_input_eof(&input, &eof, &error)
		    && eof == (accepted == 0U)
		    && input.last_read.outcome == YT_TEXT_INPUT_READ_RETURNED
		    && input.last_read.operation_count == 1U
		    && input.last_read.accepted == accepted
		    && input.refill_index == 1U
		    && input.read_remaining == accepted
		    && input.logical_position == 0U
		    && input.physical_position == (int64_t)accepted);
		for (index = 0U; index < accepted; ++index)
			CHECK(input.read_ahead[index] == 'Q');
		for (index = accepted; index < sizeof(bytes); ++index)
			CHECK(input.read_ahead[index] == 0U);
		yt_text_input_destroy(&input);
	}

	/* Carry may leave a physical prefix, but commits no refill/count state. */
	for (accepted = 0U; accepted <= sizeof(bytes); ++accepted) {
		memset(&script, 0, sizeof(script));
		text_input_read_add(&script, bytes, accepted, true, 5U, 57U, 0);
		CHECK(text_input_read_fixture(&input, &script));
		input.read_total = 77U;
		input.refill_index = 42U;
		CHECK(!yt_text_input_eof(&input, &eof, &error)
		    && input.last_read.outcome == YT_TEXT_INPUT_READ_DISK_ERROR
		    && input.last_read.accepted == accepted
		    && input.last_read.dos_error == 5U
		    && input.last_read.basic_error == 57U
		    && input.read_total == 77U && input.read_remaining == 0U
		    && input.refill_index == 42U && input.logical_position == 0U);
		for (index = 0U; index < accepted; ++index)
			CHECK(input.read_ahead[index] == 'Q');
		for (index = accepted; index < sizeof(bytes); ++index)
			CHECK(input.read_ahead[index] == 0U);
		yt_text_input_destroy(&input);
	}

	/* A mid-record refill failure abandons its returned prefix exactly once. */
	for (value = 1U; value <= 0xffU; ++value) {
		memset(&script, 0, sizeof(script));
		text_input_read_add(&script, NULL, 0U, true, (uint16_t)value,
		    (uint16_t)value, 128);
		CHECK(text_input_read_fixture(&input, &script));
		memset(input.read_ahead, 'A', sizeof(input.read_ahead));
		input.read_total = sizeof(input.read_ahead);
		input.read_remaining = sizeof(input.read_ahead);
		input.refill_index = 1U;
		input.physical_position = 128;
		line = (const uint8_t *)(uintptr_t)1U;
		length = 99U;
		available = true;
		yt_error_clear(&error);
		CHECK(!yt_text_input_read_line(&input, &line, &length,
		    &available, &error)
		    && line == NULL && length == 0U && !available
		    && error.status == YT_IO_ERROR
		    && strcmp(error.operation, "sequential INPUT read") == 0
		    && input.last_read.outcome == YT_TEXT_INPUT_READ_DISK_ERROR
		    && input.last_read.operation_count == 1U
		    && input.last_read.dos_error == value
		    && input.last_read.basic_error == value
		    && input.last_read.consumed == sizeof(input.read_ahead)
		    && input.last_read.returned == sizeof(input.read_ahead)
		    && input.logical_position == sizeof(input.read_ahead)
		    && input.physical_position == 128
		    && input.read_total == sizeof(input.read_ahead)
		    && input.read_remaining == 0U
		    && input.refill_index == 1U
		    && input.last_read.buffer_cleared);
		for (index = 0U; index < sizeof(input.read_ahead); ++index)
			CHECK(input.read_ahead[index] == 0U);
		yt_text_input_destroy(&input);
	}

	/* Rejected and malformed provider observations retain a live input. */
	for (value = 0U; value < 7U; ++value) {
		memset(&script, 0, sizeof(script));
		step = text_input_read_add(&script, NULL, 0U,
		    false, 0U, 0U, 0);
		if (step != NULL) {
			switch (value) {
			case 0U:
				step->provider_ok = false;
				break;
			case 1U:
				step->observation.accepted
				    = YT_TEXT_INPUT_BUFFER_SIZE + 1U;
				break;
			case 2U:
				step->observation.carry = true;
				step->observation.basic_error = 1U;
				break;
			case 3U:
				step->observation.carry = true;
				step->observation.dos_error = 1U;
				break;
			case 4U:
				step->observation.dos_error = 1U;
				break;
			case 5U:
				step->observation.basic_error = 1U;
				break;
			default:
				step->observation.terminal_position = -2;
				break;
			}
		}
		CHECK(text_input_read_fixture(&input, &script));
		CHECK(!yt_text_input_eof(&input, &eof, &error)
		    && input.last_read.outcome
		    == YT_TEXT_INPUT_READ_PROVIDER_ERROR
		    && input.last_read.basic_error == 57U
		    && input.last_read.registered
		    && input.last_read.handle_open
		    && input.file != NULL && input.orphaned_file == NULL
		    && input.refill_index == 0U && input.read_remaining == 0U);
		yt_text_input_destroy(&input);
	}

	/* The on-disk 24-bit refill counter wraps independently of logical EOF. */
	memset(&script, 0, sizeof(script));
	text_input_read_add(&script, NULL, 0U, false, 0U, 0U, 0);
	CHECK(text_input_read_fixture(&input, &script));
	input.refill_index = 0x00ffffffU;
	CHECK(yt_text_input_eof(&input, &eof, &error) && eof
	    && input.refill_index == 0U
	    && input.last_read.refill_index == 0U);
	yt_text_input_destroy(&input);
}

static void
test_output_open_success_model(void)
{
	struct text_open_script script;
	struct text_open_step *step;
	struct yt_text_output output;
	struct yt_error error;

	memset(&script, 0, sizeof(script));
	text_open_add_file(&script, YT_TEXT_OPEN_EXISTING, 1U);
	text_open_add_query(&script, 1U, false, 0U, false);
	text_open_use(&output, &script);
	yt_error_clear(&error);
	CHECK(yt_text_output_open(&output, "output-script.dat", &error)
	    && output.last_output_open.outcome == YT_TEXT_OPEN_RETURNED
	    && output.last_output_open.operation_count == 2U
	    && output.last_output_open.access_attempt_count == 1U
	    && output.last_output_open.access_attempts[0] == 1U
	    && output.last_output_open.terminal_position == 0
	    && !output.last_output_open.created
	    && !output.last_output_open.device
	    && output.last_output_open.registered
	    && output.last_output_open.handle_open
	    && ftell(output.file) == 0L);
	text_open_check_consumed(&script);
	yt_text_output_destroy(&output);

	memset(&script, 0, sizeof(script));
	text_output_missing_closed_prefix(&script);
	text_open_add_file(&script, YT_TEXT_OPEN_REOPEN, 1U);
	text_open_add_query(&script, 1U, true, 5U, false);
	text_open_use(&output, &script);
	CHECK(yt_text_output_open(&output, "output-script.dat", &error)
	    && output.last_output_open.outcome == YT_TEXT_OPEN_RETURNED
	    && output.last_output_open.created
	    && output.last_output_open.temporary_close_attempted
	    && !output.last_output_open.temporary_close_retried
	    && output.last_output_open.access_attempt_count == 2U
	    && output.last_output_open.access_attempts[0] == 1U
	    && output.last_output_open.access_attempts[1] == 1U
	    && output.last_output_open.operation_count == 5U);
	text_open_check_consumed(&script);
	yt_text_output_destroy(&output);

	/* Device query carry is ignored; a clear configure result returns. */
	memset(&script, 0, sizeof(script));
	text_open_add_file(&script, YT_TEXT_OPEN_EXISTING, 1U);
	text_open_add_query(&script, 1U, true, 1U, true);
	step = text_open_add(&script,
	    YT_TEXT_OPEN_CONFIGURE_DEVICE, 1U, 0U);
	if (step != NULL)
		step->observation.handle_open = true;
	text_open_use(&output, &script);
	CHECK(yt_text_output_open(&output, "output-script.dat", &error)
	    && output.last_output_open.outcome == YT_TEXT_OPEN_RETURNED
	    && output.last_output_open.device
	    && output.last_output_open.registered);
	text_open_check_consumed(&script);
	yt_text_output_destroy(&output);
}

static void
test_input_open_success_model(void)
{
	struct text_open_script script;
	struct text_open_step *step;
	struct yt_text_input input;
	struct yt_error error;

	memset(&script, 0, sizeof(script));
	text_open_add_file(&script, YT_TEXT_OPEN_EXISTING, 0U);
	text_open_add_query(&script, 0U, false, 0U, false);
	text_open_use_input(&input, &script);
	yt_error_clear(&error);
	CHECK(yt_text_input_open(&input, "input-script.dat", &error)
	    && input.last_open.outcome == YT_TEXT_OPEN_RETURNED
	    && input.last_open.operation_count == 2U
	    && input.last_open.access_attempt_count == 1U
	    && input.last_open.access_attempts[0] == 0U
	    && input.last_open.terminal_position == 0
	    && !input.last_open.created && !input.last_open.device
	    && input.last_open.registered && input.last_open.handle_open
	    && ftell(input.file) == 0L);
	text_open_check_consumed(&script);
	yt_text_input_destroy(&input);

	/* Query carry is ignored; returned device state still selects configure. */
	memset(&script, 0, sizeof(script));
	text_open_add_file(&script, YT_TEXT_OPEN_EXISTING, 0U);
	text_open_add_query(&script, 0U, true, 6U, true);
	step = text_open_add(&script,
	    YT_TEXT_OPEN_CONFIGURE_DEVICE, 0U, 0U);
	if (step != NULL)
		step->observation.handle_open = true;
	text_open_use_input(&input, &script);
	CHECK(yt_text_input_open(&input, "input-script.dat", &error)
	    && input.last_open.outcome == YT_TEXT_OPEN_RETURNED
	    && input.last_open.operation_count == 3U
	    && input.last_open.device && input.last_open.registered
	    && input.last_open.handle_open);
	text_open_check_consumed(&script);
	yt_text_input_destroy(&input);
}

static void
test_input_open_error_model(void)
{
	struct text_open_script script;
	struct text_open_step *step;
	struct yt_text_input input;
	struct yt_error error;
	unsigned dos_error;
	unsigned mapped_error;

	for (dos_error = 1U; dos_error <= 0xffU; ++dos_error) {
		if (dos_error == 5U)
			continue;
		memset(&script, 0, sizeof(script));
		text_open_add_error(&script, YT_TEXT_OPEN_EXISTING,
		    0U, 0U, (uint16_t)dos_error, 0U);
		text_open_use_input(&input, &script);
		yt_error_clear(&error);
		CHECK(!yt_text_input_open(&input, "input-script.dat", &error)
		    && input.last_open.outcome == YT_TEXT_OPEN_INITIAL_ERROR
		    && input.last_open.basic_error
		    == (dos_error == 3U ? 76U : 53U)
		    && input.last_open.dos_error == dos_error
		    && input.last_open.operation_count == 1U
		    && input.last_open.access_attempt_count == 1U
		    && input.last_open.access_attempts[0] == 0U
		    && !input.last_open.registered
		    && !input.last_open.handle_open);
		text_open_check_consumed(&script);
		yt_text_input_destroy(&input);
	}
	for (mapped_error = 70U; mapped_error <= 75U; mapped_error += 5U) {
		memset(&script, 0, sizeof(script));
		text_open_add_error(&script, YT_TEXT_OPEN_EXISTING,
		    0U, 0U, 5U, 0U);
		text_open_add_extended(&script, 5U,
		    (uint16_t)mapped_error);
		text_open_use_input(&input, &script);
		CHECK(!yt_text_input_open(&input, "input-script.dat", &error)
		    && input.last_open.outcome == YT_TEXT_OPEN_INITIAL_ERROR
		    && input.last_open.failed_operation
		    == YT_TEXT_OPEN_EXTENDED_ERROR
		    && input.last_open.basic_error == mapped_error
		    && input.last_open.dos_error == 5U
		    && input.last_open.operation_count == 2U);
		text_open_check_consumed(&script);
		yt_text_input_destroy(&input);
	}

	/* Device configuration carry maps every DOS byte to ERR57. */
	for (dos_error = 1U; dos_error <= 0xffU; ++dos_error) {
		memset(&script, 0, sizeof(script));
		text_open_add_file(&script, YT_TEXT_OPEN_EXISTING, 0U);
		text_open_add_query(&script, 0U, false, 0U, true);
		step = text_open_add(&script,
		    YT_TEXT_OPEN_CONFIGURE_DEVICE, 0U, 0U);
		if (step != NULL) {
			step->observation.carry = true;
			step->observation.dos_error = (uint16_t)dos_error;
			step->observation.handle_open = true;
		}
		text_open_use_input(&input, &script);
		CHECK(!yt_text_input_open(&input, "input-script.dat", &error)
		    && input.last_open.outcome == YT_TEXT_OPEN_DEVICE_ERROR
		    && input.last_open.failed_operation
		    == YT_TEXT_OPEN_CONFIGURE_DEVICE
		    && input.last_open.basic_error == 57U
		    && input.last_open.dos_error == dos_error
		    && input.last_open.device && input.last_open.registered
		    && input.last_open.handle_open);
		text_open_check_consumed(&script);
		yt_text_input_destroy(&input);
	}

	memset(&script, 0, sizeof(script));
	step = text_open_add(&script, YT_TEXT_OPEN_EXISTING, 0U, 0U);
	if (step != NULL)
		step->provider_ok = false;
	text_open_use_input(&input, &script);
	CHECK(!yt_text_input_open(&input, "input-script.dat", &error)
	    && input.last_open.outcome == YT_TEXT_OPEN_PROVIDER_ERROR
	    && !input.last_open.registered && !input.last_open.handle_open);
	text_open_check_consumed(&script);
	yt_text_input_destroy(&input);

	memset(&script, 0, sizeof(script));
	text_open_add_error(&script, YT_TEXT_OPEN_EXISTING,
	    0U, 0U, 5U, 0U);
	step = text_open_add(&script, YT_TEXT_OPEN_EXTENDED_ERROR,
	    0U, 5U);
	text_open_use_input(&input, &script);
	CHECK(!yt_text_input_open(&input, "input-script.dat", &error)
	    && input.last_open.outcome == YT_TEXT_OPEN_PROVIDER_ERROR
	    && input.last_open.failed_operation
	    == YT_TEXT_OPEN_EXTENDED_ERROR
	    && input.last_open.operation_count == 2U
	    && !input.last_open.registered && !input.last_open.handle_open);
	text_open_check_consumed(&script);
	yt_text_input_destroy(&input);

	memset(&script, 0, sizeof(script));
	text_open_add_file(&script, YT_TEXT_OPEN_EXISTING, 0U);
	step = text_open_add(&script, YT_TEXT_OPEN_QUERY_DEVICE, 0U, 0U);
	if (step != NULL)
		step->provider_ok = false;
	text_open_use_input(&input, &script);
	CHECK(!yt_text_input_open(&input, "input-script.dat", &error)
	    && input.last_open.outcome == YT_TEXT_OPEN_PROVIDER_ERROR
	    && input.last_open.failed_operation == YT_TEXT_OPEN_QUERY_DEVICE
	    && input.last_open.operation_count == 2U
	    && input.last_open.registered && input.last_open.handle_open);
	text_open_check_consumed(&script);
	yt_text_input_destroy(&input);

	/* A malformed supplied live handle is retained for explicit cleanup. */
	memset(&script, 0, sizeof(script));
	text_open_add_file(&script, YT_TEXT_OPEN_EXISTING, 0U);
	if (script.length != 0U)
		script.steps[0].observation.terminal_position = 1;
	text_open_use_input(&input, &script);
	CHECK(!yt_text_input_open(&input, "input-script.dat", &error)
	    && input.last_open.outcome == YT_TEXT_OPEN_PROVIDER_ERROR
	    && !input.last_open.registered && input.last_open.handle_open
	    && input.file == NULL && input.orphaned_file != NULL);
	text_open_check_consumed(&script);
	yt_text_input_destroy(&input);
}

static void
test_input_close_model(void)
{
	struct text_close_script script;
	struct text_close_step *step;
	struct yt_text_input input;
	struct yt_error error;
	unsigned device;
	unsigned dos_error;
	unsigned retry_error;

	/* A missing explicit CLOSE returns without a physical operation. */
	yt_text_input_init(&input);
	yt_error_clear(&error);
	CHECK(yt_text_input_close(&input, &error)
	    && input.last_close.outcome == YT_TEXT_CLOSE_RETURNED
	    && input.last_close.missing
	    && input.last_close.operation_count == 0U
	    && !input.last_close.device
	    && !input.last_close.registered
	    && !input.last_close.handle_open);
	yt_text_input_destroy(&input);

	/* Ordinary and device inputs share the one-operation success suffix. */
	for (device = 0U; device <= 1U; ++device) {
		memset(&script, 0, sizeof(script));
		text_close_add(&script, YT_TEXT_CLOSE_HANDLE, NULL, 0U,
		    0U, false, 0U, false, true, true);
		CHECK(text_input_close_fixture(&input, &script, device != 0U));
		yt_error_clear(&error);
		CHECK(yt_text_input_close(&input, &error)
		    && script.position == script.length
		    && input.last_close.outcome == YT_TEXT_CLOSE_RETURNED
		    && input.last_close.operation_count == 1U
		    && input.last_close.terminal_position == 37
		    && input.last_close.device == (device != 0U)
		    && !input.last_close.missing
		    && !input.last_close.cleanup_close_attempted
		    && !input.last_close.registered
		    && !input.last_close.handle_open
		    && input.file == NULL && input.orphaned_file == NULL);
		yt_text_input_destroy(&input);
	}

	/* Every first DOS error retries once, frees registration, and routes by class. */
	for (device = 0U; device <= 1U; ++device) {
		for (dos_error = 1U; dos_error <= 0xffU; ++dos_error) {
			memset(&script, 0, sizeof(script));
			text_close_add(&script, YT_TEXT_CLOSE_HANDLE, NULL, 0U,
			    0U, true, (uint16_t)dos_error, true, true, false);
			text_close_add(&script, YT_TEXT_CLOSE_CLEANUP_HANDLE,
			    NULL, 0U, 0U, false, 0U, false, true, true);
			CHECK(text_input_close_fixture(&input, &script,
			    device != 0U));
			yt_error_clear(&error);
			CHECK(!yt_text_input_close(&input, &error)
			    && error.status == YT_IO_ERROR
			    && strcmp(error.operation,
			    "sequential INPUT CLOSE") == 0
			    && script.position == script.length
			    && input.last_close.outcome
			    == YT_TEXT_CLOSE_DISK_ERROR
			    && input.last_close.failed_operation
			    == YT_TEXT_CLOSE_HANDLE
			    && input.last_close.basic_error
			    == (device != 0U ? 57U : 70U)
			    && input.last_close.dos_error == dos_error
			    && input.last_close.operation_count == 2U
			    && input.last_close.cleanup_close_attempted
			    && !input.last_close.registered
			    && !input.last_close.handle_open
			    && input.file == NULL
			    && input.orphaned_file == NULL);
			yt_text_input_destroy(&input);
		}
	}

	/* The retry result is ignored over its complete clear/carry domain. */
	for (retry_error = 0U; retry_error <= 0xffU; ++retry_error) {
		memset(&script, 0, sizeof(script));
		text_close_add(&script, YT_TEXT_CLOSE_HANDLE, NULL, 0U,
		    0U, true, 5U, true, true, false);
		text_close_add(&script, YT_TEXT_CLOSE_CLEANUP_HANDLE, NULL, 0U,
		    0U, retry_error != 0U, (uint16_t)retry_error,
		    retry_error != 0U, true, retry_error == 0U);
		CHECK(text_input_close_fixture(&input, &script, false));
		CHECK(!yt_text_input_close(&input, &error)
		    && script.position == script.length
		    && input.last_close.outcome == YT_TEXT_CLOSE_DISK_ERROR
		    && input.last_close.basic_error == 70U
		    && input.last_close.dos_error == 5U
		    && input.last_close.cleanup_dos_error == retry_error
		    && !input.last_close.registered
		    && input.last_close.handle_open == (retry_error != 0U)
		    && (input.orphaned_file != NULL) == (retry_error != 0U));
		yt_text_input_destroy(&input);
	}

	/* Even a host adapter that consumed the first handle receives the retry. */
	memset(&script, 0, sizeof(script));
	text_close_add(&script, YT_TEXT_CLOSE_HANDLE, NULL, 0U,
	    0U, true, 5U, false, true, true);
	text_close_add(&script, YT_TEXT_CLOSE_CLEANUP_HANDLE, NULL, 0U,
	    0U, true, 6U, false, false, false);
	CHECK(text_input_close_fixture(&input, &script, false));
	CHECK(!yt_text_input_close(&input, &error)
	    && script.position == script.length
	    && input.last_close.outcome == YT_TEXT_CLOSE_DISK_ERROR
	    && input.last_close.operation_count == 2U
	    && input.last_close.basic_error == 70U
	    && input.last_close.dos_error == 5U
	    && input.last_close.cleanup_dos_error == 6U
	    && !input.last_close.registered
	    && !input.last_close.handle_open);
	yt_text_input_destroy(&input);

	/* Provider rejection before CLOSE retains the registered live handle. */
	memset(&script, 0, sizeof(script));
	text_close_add(&script, YT_TEXT_CLOSE_HANDLE, NULL, 0U,
	    0U, false, 0U, true, true, false);
	script.steps[0].provider_ok = false;
	CHECK(text_input_close_fixture(&input, &script, false));
	CHECK(!yt_text_input_close(&input, &error)
	    && input.last_close.outcome == YT_TEXT_CLOSE_PROVIDER_ERROR
	    && input.last_close.basic_error == 57U
	    && input.last_close.operation_count == 1U
	    && input.last_close.registered && input.last_close.handle_open
	    && input.file != NULL && input.orphaned_file == NULL);
	yt_text_input_destroy(&input);

	/* A malformed handle observation is rejected without losing ownership. */
	memset(&script, 0, sizeof(script));
	text_close_add(&script, YT_TEXT_CLOSE_HANDLE, NULL, 0U,
	    0U, false, 0U, true, true, false);
	step = &script.steps[0];
	step->observation.accepted = 1U;
	CHECK(text_input_close_fixture(&input, &script, false));
	CHECK(!yt_text_input_close(&input, &error)
	    && input.last_close.outcome == YT_TEXT_CLOSE_PROVIDER_ERROR
	    && input.last_close.operation_count == 1U
	    && input.last_close.registered && input.last_close.handle_open);
	yt_text_input_destroy(&input);

	/* Cleanup adapter failure retains the now-unregistered orphaned handle. */
	memset(&script, 0, sizeof(script));
	text_close_add(&script, YT_TEXT_CLOSE_HANDLE, NULL, 0U,
	    0U, true, 5U, true, true, false);
	text_close_add(&script, YT_TEXT_CLOSE_CLEANUP_HANDLE, NULL, 0U,
	    0U, false, 0U, true, true, false);
	script.steps[1].provider_ok = false;
	CHECK(text_input_close_fixture(&input, &script, false));
	CHECK(!yt_text_input_close(&input, &error)
	    && input.last_close.outcome == YT_TEXT_CLOSE_PROVIDER_ERROR
	    && input.last_close.basic_error == 57U
	    && input.last_close.dos_error == 5U
	    && input.last_close.operation_count == 2U
	    && !input.last_close.registered && input.last_close.handle_open
	    && input.file == NULL && input.orphaned_file != NULL);
	yt_text_input_destroy(&input);
}

static void
test_output_open_error_model(void)
{
	struct text_open_script script;
	struct text_open_step *step;
	struct yt_text_output output;
	struct yt_error error;
	unsigned dos_error;
	unsigned mapped_error;

	for (dos_error = 1U; dos_error <= 0xffU; ++dos_error) {
		if (dos_error == 2U || dos_error == 5U)
			continue;
		memset(&script, 0, sizeof(script));
		text_open_add_error(&script,
		    YT_TEXT_OPEN_EXISTING, 1U, 0U,
		    (uint16_t)dos_error, 0U);
		text_open_use(&output, &script);
		yt_error_clear(&error);
		CHECK(!yt_text_output_open(&output, "output-script.dat", &error)
		    && output.last_output_open.outcome
		    == YT_TEXT_OPEN_INITIAL_ERROR
		    && output.last_output_open.basic_error
		    == (dos_error == 3U ? 76U : 67U)
		    && output.last_output_open.dos_error == dos_error
		    && output.last_output_open.access_attempt_count == 1U);
		text_open_check_consumed(&script);
		yt_text_output_destroy(&output);
	}
	for (mapped_error = 70U; mapped_error <= 75U; mapped_error += 5U) {
		memset(&script, 0, sizeof(script));
		text_open_add_error(&script,
		    YT_TEXT_OPEN_EXISTING, 1U, 0U, 5U, 0U);
		text_open_add_extended(&script, 5U,
		    (uint16_t)mapped_error);
		text_open_use(&output, &script);
		CHECK(!yt_text_output_open(&output, "output-script.dat", &error)
		    && output.last_output_open.outcome
		    == YT_TEXT_OPEN_INITIAL_ERROR
		    && output.last_output_open.failed_operation
		    == YT_TEXT_OPEN_EXTENDED_ERROR
		    && output.last_output_open.basic_error == mapped_error
		    && output.last_output_open.operation_count == 2U);
		text_open_check_consumed(&script);
		yt_text_output_destroy(&output);
	}

	for (dos_error = 1U; dos_error <= 0xffU; ++dos_error) {
		memset(&script, 0, sizeof(script));
		text_open_add_error(&script,
		    YT_TEXT_OPEN_EXISTING, 1U, 0U, 2U, 0U);
		text_open_add_error(&script, YT_TEXT_OPEN_CREATE,
		    1U, 0U, (uint16_t)dos_error, 0U);
		if (dos_error == 5U)
			text_open_add_extended(&script, 5U, 70U);
		text_open_use(&output, &script);
		CHECK(!yt_text_output_open(&output, "output-script.dat", &error)
		    && output.last_output_open.outcome
		    == YT_TEXT_OPEN_CREATE_ERROR
		    && output.last_output_open.basic_error
		    == (dos_error == 2U ? 53U
		    : dos_error == 5U ? 70U : 67U)
		    && output.last_output_open.dos_error == dos_error);
		text_open_check_consumed(&script);
		yt_text_output_destroy(&output);
	}

	for (dos_error = 1U; dos_error <= 0xffU; ++dos_error) {
		memset(&script, 0, sizeof(script));
		text_open_add_error(&script,
		    YT_TEXT_OPEN_EXISTING, 1U, 0U, 2U, 0U);
		text_open_add_file(&script, YT_TEXT_OPEN_CREATE, 1U);
		text_open_add_error(&script,
		    YT_TEXT_OPEN_TEMP_CLOSE, 0U, 0U,
		    (uint16_t)dos_error, 0U);
		step = text_open_add(&script,
		    YT_TEXT_OPEN_TEMP_CLOSE, 0U, (uint16_t)dos_error);
		if (step != NULL) {
			step->observation.carry = true;
			step->observation.dos_error = (uint16_t)dos_error;
			step->close_active = true;
		}
		text_open_use(&output, &script);
		CHECK(!yt_text_output_open(&output, "output-script.dat", &error)
		    && output.last_output_open.outcome
		    == YT_TEXT_OPEN_TEMP_CLOSE_ERROR
		    && output.last_output_open.basic_error == 70U
		    && output.last_output_open.dos_error == dos_error
		    && output.last_output_open.temporary_close_retry_dos_error
		    == dos_error
		    && output.last_output_open.temporary_close_retried
		    && !output.last_output_open.handle_open);
		text_open_check_consumed(&script);
		yt_text_output_destroy(&output);
	}

	for (dos_error = 1U; dos_error <= 0xffU; ++dos_error) {
		memset(&script, 0, sizeof(script));
		text_output_missing_closed_prefix(&script);
		text_open_add_error(&script, YT_TEXT_OPEN_REOPEN,
		    1U, 0U, (uint16_t)dos_error, 0U);
		if (dos_error == 5U)
			text_open_add_extended(&script, 5U, 75U);
		text_open_use(&output, &script);
		CHECK(!yt_text_output_open(&output, "output-script.dat", &error)
		    && output.last_output_open.outcome
		    == YT_TEXT_OPEN_REOPEN_ERROR
		    && output.last_output_open.basic_error
		    == (dos_error == 2U ? 53U
		    : dos_error == 5U ? 75U : 67U)
		    && output.last_output_open.dos_error == dos_error);
		text_open_check_consumed(&script);
		yt_text_output_destroy(&output);
	}

	/* Device configuration carry maps every DOS byte to ERR57. */
	for (dos_error = 1U; dos_error <= 0xffU; ++dos_error) {
		memset(&script, 0, sizeof(script));
		text_open_add_file(&script,
		    YT_TEXT_OPEN_EXISTING, 1U);
		text_open_add_query(&script, 1U, false, 0U, true);
		step = text_open_add(&script,
		    YT_TEXT_OPEN_CONFIGURE_DEVICE, 1U, 0U);
		if (step != NULL) {
			step->observation.carry = true;
			step->observation.dos_error = (uint16_t)dos_error;
			step->observation.handle_open = true;
		}
		text_open_use(&output, &script);
		CHECK(!yt_text_output_open(&output, "output-script.dat", &error)
		    && output.last_output_open.outcome
		    == YT_TEXT_OPEN_DEVICE_ERROR
		    && output.last_output_open.basic_error == 57U
		    && output.last_output_open.dos_error == dos_error
		    && output.last_output_open.registered);
		text_open_check_consumed(&script);
		yt_text_output_destroy(&output);
	}

	memset(&script, 0, sizeof(script));
	step = text_open_add(&script, YT_TEXT_OPEN_EXISTING, 1U,
	    0U);
	if (step != NULL)
		step->provider_ok = false;
	text_open_use(&output, &script);
	CHECK(!yt_text_output_open(&output, "output-script.dat", &error)
	    && output.last_output_open.outcome
	    == YT_TEXT_OPEN_PROVIDER_ERROR);
	text_open_check_consumed(&script);
	yt_text_output_destroy(&output);
}

static void
test_append_open_success_model(void)
{
	struct text_open_script script;
	struct text_open_step *step;
	struct yt_text_output output;
	struct yt_error error;
	uint8_t window[YT_TEXT_OUTPUT_BUFFER_SIZE];

	memset(window, 'x', sizeof(window));
	window[18] = 0x1aU;
	memset(&script, 0, sizeof(script));
	text_append_existing_prefix(&script, 200);
	text_open_add_read(&script, 2U, window, sizeof(window), 200);
	text_open_add_seek(&script, YT_TEXT_OPEN_SEEK_SELECTED, 2U,
	    90, 90);
	text_open_use(&output, &script);
	yt_error_clear(&error);
	CHECK(yt_text_output_open_append(&output, "append-script.dat", &error)
	    && output.last_append_open.outcome == YT_TEXT_OPEN_RETURNED
	    && output.last_append_open.operation_count == 6U
	    && output.last_append_open.access_attempt_count == 1U
	    && output.last_append_open.access_attempts[0] == 2U
	    && output.last_append_open.refill_count == 1U
	    && output.last_append_open.physical_length == 200
	    && output.last_append_open.window_start == 72
	    && output.last_append_open.selected_position == 90
	    && output.last_append_open.terminal_position == 90
	    && output.last_append_open.registered
	    && output.last_append_open.handle_open
	    && !output.last_append_open.created
	    && !output.last_append_open.device);
	text_open_check_consumed(&script);
	yt_text_output_destroy(&output);

	/* Access denied retries ordinary APPEND once with access selector one. */
	memset(&script, 0, sizeof(script));
	text_open_add_error(&script, YT_TEXT_OPEN_EXISTING, 2U,
	    0U, 5U, 0U);
	text_open_add_file(&script, YT_TEXT_OPEN_EXISTING, 1U);
	text_open_add_query(&script, 1U, false, 0U, false);
	text_open_add_seek(&script, YT_TEXT_OPEN_SEEK_END, 1U, 0, 0);
	text_open_add_read(&script, 1U, NULL, 0U, 0);
	text_open_add_seek(&script, YT_TEXT_OPEN_SEEK_SELECTED, 1U,
	    0, 0);
	text_open_use(&output, &script);
	CHECK(yt_text_output_open_append(&output, "append-script.dat", &error)
	    && output.last_append_open.access_attempt_count == 2U
	    && output.last_append_open.access_attempts[0] == 2U
	    && output.last_append_open.access_attempts[1] == 1U
	    && output.last_append_open.operation_count == 6U
	    && output.last_append_open.selected_position == 0);
	text_open_check_consumed(&script);
	yt_text_output_destroy(&output);

	/* A full final window with no marker performs the terminal zero refill. */
	memset(window, 'q', sizeof(window));
	memset(&script, 0, sizeof(script));
	text_append_existing_prefix(&script, 200);
	text_open_add_read(&script, 2U, window, sizeof(window), 200);
	text_open_add_read(&script, 2U, NULL, 0U, 200);
	text_open_add_seek(&script, YT_TEXT_OPEN_SEEK_SELECTED, 2U,
	    200, 200);
	text_open_use(&output, &script);
	CHECK(yt_text_output_open_append(&output, "append-script.dat", &error)
	    && output.last_append_open.refill_count == 2U
	    && output.last_append_open.selected_position == 200
	    && output.last_append_open.operation_count == 7U);
	text_open_check_consumed(&script);
	yt_text_output_destroy(&output);

	/* Missing-file create, temporary close, and reopen are one transaction. */
	memset(&script, 0, sizeof(script));
	text_append_missing_closed_prefix(&script);
	text_open_add_file(&script, YT_TEXT_OPEN_REOPEN, 2U);
	text_open_add_query(&script, 2U, false, 0U, false);
	text_open_add_seek(&script, YT_TEXT_OPEN_SEEK_END, 2U, 0, 0);
	text_open_add_read(&script, 2U, NULL, 0U, 0);
	text_open_add_seek(&script, YT_TEXT_OPEN_SEEK_SELECTED, 2U,
	    0, 0);
	text_open_use(&output, &script);
	CHECK(yt_text_output_open_append(&output, "append-script.dat", &error)
	    && output.last_append_open.created
	    && output.last_append_open.temporary_close_attempted
	    && !output.last_append_open.temporary_close_retried
	    && output.last_append_open.access_attempt_count == 2U
	    && output.last_append_open.access_attempts[0] == 2U
	    && output.last_append_open.access_attempts[1] == 2U
	    && output.last_append_open.refill_count == 1U
	    && output.last_append_open.selected_position == 0);
	text_open_check_consumed(&script);
	yt_text_output_destroy(&output);

	/* Carry from the device query is ignored; returned DL still selects. */
	memset(&script, 0, sizeof(script));
	text_open_add_file(&script, YT_TEXT_OPEN_EXISTING, 2U);
	text_open_add_query(&script, 2U, true, 5U, false);
	text_open_add_seek(&script, YT_TEXT_OPEN_SEEK_END, 2U, 0, 0);
	text_open_add_read(&script, 2U, NULL, 0U, 0);
	text_open_add_seek(&script, YT_TEXT_OPEN_SEEK_SELECTED, 2U,
	    0, 0);
	text_open_use(&output, &script);
	CHECK(yt_text_output_open_append(&output, "append-script.dat", &error)
	    && output.last_append_open.outcome == YT_TEXT_OPEN_RETURNED);
	text_open_check_consumed(&script);
	yt_text_output_destroy(&output);

	/* A selected device's configuration carry maps to ERR57. */
	memset(&script, 0, sizeof(script));
	text_open_add_file(&script, YT_TEXT_OPEN_EXISTING, 2U);
	text_open_add_query(&script, 2U, true, 1U, true);
	step = text_open_add(&script,
	    YT_TEXT_OPEN_CONFIGURE_DEVICE, 2U, 0U);
	if (step != NULL) {
		step->observation.carry = true;
		step->observation.dos_error = 5U;
		step->observation.handle_open = true;
	}
	text_open_use(&output, &script);
	CHECK(!yt_text_output_open_append(&output, "append-script.dat", &error)
	    && output.last_append_open.outcome
	    == YT_TEXT_OPEN_DEVICE_ERROR
	    && output.last_append_open.failed_operation
	    == YT_TEXT_OPEN_CONFIGURE_DEVICE
	    && output.last_append_open.basic_error == 57U
	    && output.last_append_open.dos_error == 5U
	    && output.last_append_open.registered);
	text_open_check_consumed(&script);
	yt_text_output_destroy(&output);
}

static void
test_append_open_error_model(void)
{
	struct text_open_script script;
	struct text_open_step *step;
	struct yt_text_output output;
	struct yt_error error;
	uint8_t window[YT_TEXT_OUTPUT_BUFFER_SIZE];
	unsigned dos_error;
	unsigned mapped_error;
	unsigned failed_seek;

	/* Every ordinary initial OPEN error byte has the APPEND classifier. */
	for (dos_error = 1U; dos_error <= 0xffU; ++dos_error) {
		if (dos_error == 2U || dos_error == 5U)
			continue;
		memset(&script, 0, sizeof(script));
		text_open_add_error(&script,
		    YT_TEXT_OPEN_EXISTING, 2U, 0U,
		    (uint16_t)dos_error, 0U);
		text_open_use(&output, &script);
		yt_error_clear(&error);
		CHECK(!yt_text_output_open_append(&output, "append-script.dat",
		    &error)
		    && output.last_append_open.outcome
		    == YT_TEXT_OPEN_INITIAL_ERROR
		    && output.last_append_open.basic_error
		    == (dos_error == 3U ? 76U : 75U)
		    && output.last_append_open.dos_error == dos_error);
		text_open_check_consumed(&script);
		yt_text_output_destroy(&output);
	}
	for (mapped_error = 70U; mapped_error <= 75U; mapped_error += 5U) {
		memset(&script, 0, sizeof(script));
		text_open_add_error(&script,
		    YT_TEXT_OPEN_EXISTING, 2U, 0U, 5U, 0U);
		text_open_add_error(&script,
		    YT_TEXT_OPEN_EXISTING, 1U, 0U, 5U, 0U);
		text_open_add_extended(&script, 5U,
		    (uint16_t)mapped_error);
		text_open_use(&output, &script);
		CHECK(!yt_text_output_open_append(&output, "append-script.dat",
		    &error)
		    && output.last_append_open.outcome
		    == YT_TEXT_OPEN_INITIAL_ERROR
		    && output.last_append_open.failed_operation
		    == YT_TEXT_OPEN_EXTENDED_ERROR
		    && output.last_append_open.basic_error == mapped_error
		    && output.last_append_open.access_attempt_count == 2U
		    && output.last_append_open.access_attempts[0] == 2U
		    && output.last_append_open.access_attempts[1] == 1U);
		text_open_check_consumed(&script);
		yt_text_output_destroy(&output);
	}

	/* Create error 5 alone uses extended classification; error 2 is ERR53. */
	for (dos_error = 1U; dos_error <= 0xffU; ++dos_error) {
		memset(&script, 0, sizeof(script));
		text_open_add_error(&script,
		    YT_TEXT_OPEN_EXISTING, 2U, 0U, 2U, 0U);
		text_open_add_error(&script, YT_TEXT_OPEN_CREATE,
		    2U, 0U, (uint16_t)dos_error, 0U);
		if (dos_error == 5U)
			text_open_add_extended(&script, 5U, 70U);
		text_open_use(&output, &script);
		CHECK(!yt_text_output_open_append(&output, "append-script.dat",
		    &error)
		    && output.last_append_open.outcome
		    == YT_TEXT_OPEN_CREATE_ERROR
		    && output.last_append_open.basic_error
		    == (dos_error == 2U ? 53U
		    : dos_error == 5U ? 70U : 75U)
		    && output.last_append_open.dos_error == dos_error);
		text_open_check_consumed(&script);
		yt_text_output_destroy(&output);
	}

	/* A failed temporary create-handle CLOSE retries once, then ERR70. */
	for (dos_error = 1U; dos_error <= 0xffU; ++dos_error) {
		memset(&script, 0, sizeof(script));
		text_open_add_error(&script,
		    YT_TEXT_OPEN_EXISTING, 2U, 0U, 2U, 0U);
		text_open_add_file(&script, YT_TEXT_OPEN_CREATE, 2U);
		text_open_add_error(&script,
		    YT_TEXT_OPEN_TEMP_CLOSE, 0U, 0U,
		    (uint16_t)dos_error, 0U);
		step = text_open_add(&script,
		    YT_TEXT_OPEN_TEMP_CLOSE, 0U, (uint16_t)dos_error);
		if (step != NULL) {
			step->observation.carry = true;
			step->observation.dos_error = (uint16_t)dos_error;
			step->close_active = true;
		}
		text_open_use(&output, &script);
		CHECK(!yt_text_output_open_append(&output, "append-script.dat",
		    &error)
		    && output.last_append_open.outcome
		    == YT_TEXT_OPEN_TEMP_CLOSE_ERROR
		    && output.last_append_open.basic_error == 70U
		    && output.last_append_open.dos_error == dos_error
		    && output.last_append_open.temporary_close_retry_dos_error
		    == dos_error
		    && output.last_append_open.created
		    && output.last_append_open.temporary_close_retried
		    && !output.last_append_open.registered
		    && !output.last_append_open.handle_open);
		text_open_check_consumed(&script);
		yt_text_output_destroy(&output);
	}
	/* The ignored retry may itself carry and retain or lose the handle. */
	memset(&script, 0, sizeof(script));
	text_open_add_error(&script, YT_TEXT_OPEN_EXISTING, 2U,
	    0U, 2U, 0U);
	text_open_add_file(&script, YT_TEXT_OPEN_CREATE, 2U);
	text_open_add_error(&script, YT_TEXT_OPEN_TEMP_CLOSE, 0U,
	    0U, 5U, 0U);
	text_open_add_error(&script, YT_TEXT_OPEN_TEMP_CLOSE, 0U,
	    5U, 6U, 0U);
	text_open_use(&output, &script);
	CHECK(!yt_text_output_open_append(&output, "append-script.dat", &error)
	    && output.last_append_open.outcome
	    == YT_TEXT_OPEN_TEMP_CLOSE_ERROR
	    && output.last_append_open.basic_error == 70U
	    && output.last_append_open.dos_error == 5U
	    && output.last_append_open.temporary_close_retry_dos_error == 6U
	    && output.last_append_open.handle_open
	    && output.orphaned_file != NULL);
	text_open_check_consumed(&script);
	yt_text_output_destroy(&output);

	memset(&script, 0, sizeof(script));
	text_open_add_error(&script, YT_TEXT_OPEN_EXISTING, 2U,
	    0U, 2U, 0U);
	text_open_add_file(&script, YT_TEXT_OPEN_CREATE, 2U);
	text_open_add_error(&script, YT_TEXT_OPEN_TEMP_CLOSE, 0U,
	    0U, 5U, 0U);
	script.steps[2].observation.handle_open = false;
	script.steps[2].close_active = true;
	step = text_open_add(&script, YT_TEXT_OPEN_TEMP_CLOSE, 0U,
	    5U);
	if (step != NULL) {
		step->expect_active = false;
		step->observation.carry = true;
		step->observation.dos_error = 6U;
	}
	text_open_use(&output, &script);
	CHECK(!yt_text_output_open_append(&output, "append-script.dat", &error)
	    && output.last_append_open.outcome
	    == YT_TEXT_OPEN_TEMP_CLOSE_ERROR
	    && output.last_append_open.basic_error == 70U
	    && output.last_append_open.dos_error == 5U
	    && !output.last_append_open.handle_open
	    && output.orphaned_file == NULL);
	text_open_check_consumed(&script);
	yt_text_output_destroy(&output);

	/* Reopen does not repeat initial error 3's path-not-found mapping. */
	for (dos_error = 1U; dos_error <= 0xffU; ++dos_error) {
		memset(&script, 0, sizeof(script));
		text_append_missing_closed_prefix(&script);
		text_open_add_error(&script, YT_TEXT_OPEN_REOPEN,
		    2U, 0U, (uint16_t)dos_error, 0U);
		if (dos_error == 5U) {
			text_open_add_error(&script,
			    YT_TEXT_OPEN_REOPEN, 1U, 0U, 5U, 0U);
			text_open_add_extended(&script, 5U, 75U);
		}
		text_open_use(&output, &script);
		CHECK(!yt_text_output_open_append(&output, "append-script.dat",
		    &error)
		    && output.last_append_open.outcome
		    == YT_TEXT_OPEN_REOPEN_ERROR
		    && output.last_append_open.basic_error
		    == (dos_error == 2U ? 53U : 75U)
		    && output.last_append_open.dos_error == dos_error);
		text_open_check_consumed(&script);
		yt_text_output_destroy(&output);
	}

	/* Each of the three APPEND seeks maps every DOS error to ERR52. */
	memset(window, 'x', sizeof(window));
	window[18] = 0x1aU;
	for (failed_seek = 0U; failed_seek < 3U; ++failed_seek) {
		for (dos_error = 1U; dos_error <= 0xffU; ++dos_error) {
			memset(&script, 0, sizeof(script));
			text_open_add_file(&script,
			    YT_TEXT_OPEN_EXISTING, 2U);
			text_open_add_query(&script, 2U, false, 0U, false);
			if (failed_seek == 0U) {
				text_open_add_error(&script,
				    YT_TEXT_OPEN_SEEK_END, 2U, 0U,
				    (uint16_t)dos_error, 0U);
			}
			else {
				text_open_add_seek(&script,
				    YT_TEXT_OPEN_SEEK_END, 2U, 0, 200);
				if (failed_seek == 1U) {
					step = text_open_add(&script,
					    YT_TEXT_OPEN_SEEK_WINDOW, 2U, 0U);
					if (step != NULL) {
						step->offset = 72;
						step->observation.carry = true;
						step->observation.dos_error =
						    (uint16_t)dos_error;
						step->observation.handle_open = true;
					}
				}
				else {
					text_open_add_seek(&script,
					    YT_TEXT_OPEN_SEEK_WINDOW,
					    2U, 72, 72);
					text_open_add_read(&script, 2U,
					    window, sizeof(window), 200);
					step = text_open_add(&script,
					    YT_TEXT_OPEN_SEEK_SELECTED,
					    2U, 0U);
					if (step != NULL) {
						step->offset = 90;
						step->observation.carry = true;
						step->observation.dos_error =
						    (uint16_t)dos_error;
						step->observation.handle_open = true;
					}
				}
			}
			text_open_use(&output, &script);
			CHECK(!yt_text_output_open_append(&output,
			    "append-script.dat", &error)
			    && output.last_append_open.outcome
			    == YT_TEXT_OPEN_SEEK_ERROR
			    && output.last_append_open.basic_error == 52U
			    && output.last_append_open.dos_error == dos_error);
			text_open_check_consumed(&script);
			yt_text_output_destroy(&output);
		}
	}

	/* Window reads retain their accepted prefix and extended error mapping. */
	for (dos_error = 1U; dos_error <= 0xffU; ++dos_error) {
		memset(&script, 0, sizeof(script));
		text_append_existing_prefix(&script, 200);
		step = text_open_add(&script,
		    YT_TEXT_OPEN_READ_WINDOW, 2U, 0U);
		if (step != NULL) {
			step->requested = YT_TEXT_OUTPUT_BUFFER_SIZE;
			step->observation.accepted = 7U;
			step->observation.carry = true;
			step->observation.dos_error = (uint16_t)dos_error;
			step->observation.mapped_error = dos_error == 5U ? 70U : 0U;
			step->observation.handle_open = true;
			step->observation.terminal_position = 79;
		}
		text_open_use(&output, &script);
		CHECK(!yt_text_output_open_append(&output, "append-script.dat",
		    &error)
		    && output.last_append_open.outcome
		    == YT_TEXT_OPEN_READ_ERROR
		    && output.last_append_open.basic_error
		    == (dos_error == 5U ? 70U : 57U)
		    && output.last_append_open.dos_error == dos_error
		    && output.last_append_open.accepted == 7U
		    && output.last_append_open.terminal_position == 79);
		text_open_check_consumed(&script);
		yt_text_output_destroy(&output);
	}

	/* A missing provider result is not mistaken for a runtime DOS edge. */
	memset(&script, 0, sizeof(script));
	step = text_open_add(&script, YT_TEXT_OPEN_EXISTING, 2U,
	    0U);
	if (step != NULL)
		step->provider_ok = false;
	text_open_use(&output, &script);
	CHECK(!yt_text_output_open_append(&output, "append-script.dat", &error)
	    && output.last_append_open.outcome
	    == YT_TEXT_OPEN_PROVIDER_ERROR
	    && output.last_append_open.failed_operation
	    == YT_TEXT_OPEN_EXISTING);
	text_open_check_consumed(&script);
	yt_text_output_destroy(&output);
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
	state.path = "XANNORHQ.TXT";
	yt_error_clear(&error);
	CHECK(yt_text_sequential_play_run(&state, &ops, &success, &error));
	CHECK(success.calls == YT_ARRAY_LEN(expected)
	    && success.event_count == YT_ARRAY_LEN(expected)
	    && memcmp(success.events, expected, sizeof(expected)) == 0
	    && strcmp(success.path, "XANNORHQ.TXT") == 0
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
		state.path = "XANNORHQ.TXT";
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
	state.path = "XANNORHQ.TXT";
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
	size_t bold_calls;
	float bold;
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
viewer_play_set_bold(void *context, float value)
{
	struct viewer_play_tape *tape = context;

	++tape->bold_calls;
	tape->bold = value;
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
	state.set_bold = viewer_play_set_bold;
	state.line_count = &line_count;
	state.pager_key = key;
	state.saved_foreground = 5.0f;
	state.saved_pager_foreground = 5;
	CHECK(yt_file_viewer_play(source, sizeof(source) - 1U, &state,
	    viewer_play_present, &tape, NULL));
	CHECK(tape.calls == 3U && tape.events[0] == 1 && tape.events[1] == 1
	    && tape.events[2] == 2 && tape.lengths[0] == 8U
	    && tape.lengths[1] == 8U && foreground == 5.0f
	    && pager_foreground == 5 && bold == 1.0f && line_count == 0.0f
	    && tape.bold_calls == 1U && tape.bold == 1.0f);

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
	    && line_count == 17.0f && tape.bold_calls == 1U
	    && tape.bold == 1.0f);
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
	size_t timeout_at;
	bool timeout_fired;
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
	if (!tape->timeout_fired
	    && tape->event_count == tape->timeout_at) {
		tape->timeout_fired = true;
		if (error != NULL) {
			error->status = YT_IO_ERROR;
			error->basic_error = 24U;
			error->basic_error_valid = true;
		}
		return false;
	}
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

	for (failure = 1U; failure <= YT_ARRAY_LEN(natural); ++failure) {
		if (natural[failure - 1U] == OPENING_STREAM_WAIT)
			continue;
		opening_stream_initialize(&state, &tape);
		tape.timeout_at = failure;
		yt_error_clear(&error);
		CHECK(yt_opening_stream_run(&state, &opening_stream_ops,
		    &tape, &error));
		CHECK(tape.timeout_fired
		    && tape.event_count == YT_ARRAY_LEN(natural) + 1U
		    && tape.events[failure - 1U] == natural[failure - 1U]
		    && tape.events[failure] == natural[failure - 1U]
		    && memcmp(tape.events, natural,
		    (failure - 1U) * sizeof(natural[0])) == 0
		    && memcmp(tape.events + failure + 1U, natural + failure,
		    (YT_ARRAY_LEN(natural) - failure)
		    * sizeof(natural[0])) == 0);
		opening_stream_check_state(&state, &tape);
	}

	opening_stream_initialize(&state, &tape);
	tape.timeout_at = 1U;
	CHECK(yt_opening_stream_run(&state, &opening_stream_ops,
	    &tape, NULL) && tape.timeout_fired);
	opening_stream_check_state(&state, &tape);

	opening_stream_initialize(&state, &tape);
	tape.timeout_at = 16U;
	yt_error_clear(&error);
	CHECK(!yt_opening_stream_run(&state, &opening_stream_ops, &tape,
	    &error) && tape.timeout_fired
	    && tape.event_count == 16U && !state.waited
	    && error.basic_error_valid && error.basic_error == 24U);

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
	test_database_random_open();
	test_file_kill();
	test_file_rename();
	test_close_all_registry();
	test_database_random_close();
	test_text_output_write();
	test_genesis_sequential_handoff_boundaries();
	test_text_device_print();
	test_text_output_close();
	test_database_random_lof();
	test_files();
	test_radio_file();
	test_input_open_success_model();
	test_input_open_error_model();
	test_input_read_model();
	test_input_close_model();
	test_output_open_success_model();
	test_output_open_error_model();
	test_append_open_success_model();
	test_append_open_error_model();
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
