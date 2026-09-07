#include "qb.h"
#include "yt_config.h"
#include "yt_config_output.h"
#include "yt_file.h"
#include "yt_game.h"
#include "yt_init.h"
#include "yt_maint.h"
#include "yt_names.h"
#include "yt_platform.h"
#include "yt_portname.h"
#include "yt_text.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <process.h>
#define yt_chdir _chdir
#define yt_getcwd _getcwd
#define yt_mkdir(path) _mkdir(path)
#define yt_rmdir _rmdir
#else
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#define yt_chdir chdir
#define yt_getcwd getcwd
#define yt_rmdir rmdir
#endif

#ifndef YT_CONFIG_EXE
#error "YT_CONFIG_EXE must name the ytconfig executable under test"
#endif
#ifndef YT_PORTNAME_EXE
#error "YT_PORTNAME_EXE must name the portname executable under test"
#endif
#ifndef YT_RMT_INIT_EXE
#error "YT_RMT_INIT_EXE must name the rmt-init executable under test"
#endif
#ifndef YT_LOCAL_EXE
#error "YT_LOCAL_EXE must name the local executable under test"
#endif

static int
fail(const char *message)
{
	fprintf(stderr, "test_utilities: %s\n", message);
	return EXIT_FAILURE;
}

struct utility_random_script {
	const uint8_t *bytes;
	size_t length;
	size_t position;
};

static bool read_file(const char *path, uint8_t **data, size_t *length);
static bool write_file(const char *path, const void *data, size_t length);

static bool
bytes_contain(const uint8_t *data, size_t length, const uint8_t *needle,
    size_t needle_length)
{
	size_t offset;

	if (needle_length > length)
		return false;
	for (offset = 0U; offset <= length - needle_length; ++offset) {
		if (memcmp(data + offset, needle, needle_length) == 0)
			return true;
	}
	return false;
}

static bool
utility_random_fill(void *context, void *buffer, size_t length,
    struct yt_error *error)
{
	struct utility_random_script *script = context;

	if (script->position + length > script->length) {
		if (error != NULL)
			error->status = YT_RANDOM_ERROR;
		return false;
	}
	memcpy(buffer, script->bytes + script->position, length);
	script->position += length;
	return true;
}

static bool
test_port_name_generator(void)
{
	static const uint8_t zero_draws[12] = {0};
	static const uint8_t long_draws[] = {
		0x76, 0xbe, 0xff, 0x76, 0xbe, 0xff,
		0xf9, 0xc1, 0xfc, 0x80, 0x9a, 0xfd,
		0xbc, 0x93, 0xff, 0xe9, 0xdb, 0xff
	};
	struct utility_random_script zero = {
		zero_draws, sizeof(zero_draws), 0
	};
	struct utility_random_script longest = {
		long_draws, sizeof(long_draws), 0
	};
	struct yt_random random;
	struct yt_error error;
	char name[42];

	yt_error_clear(&error);
	yt_random_init(&random);
	yt_random_set_provider(&random, utility_random_fill, &zero);
	if (!yt_generate_port_name(&random, name, &error)
	    || strcmp(name, "Inging") != 0 || random.draws != 4U
	    || zero.position != sizeof(zero_draws))
		return false;
	yt_random_set_provider(&random, utility_random_fill, &longest);
	return yt_generate_port_name(&random, name, &error)
	    && strcmp(name, "Monkey Kangeroo Tractor Lightning") == 0
	    && random.draws == 6U && longest.position == sizeof(long_draws);
}

struct portname_output_tape {
	uint8_t bytes[1024];
	size_t length;
};

static bool
portname_output_collect(void *context, const uint8_t *data, size_t length,
    struct yt_error *error)
{
	struct portname_output_tape *tape = context;

	if (tape == NULL || (data == NULL && length != 0U)
	    || length > sizeof(tape->bytes) - tape->length) {
		if (error != NULL)
			error->status = YT_RANGE;
		return false;
	}
	memcpy(tape->bytes + tape->length, data, length);
	tape->length += length;
	return true;
}

struct portname_close_fault {
	size_t calls;
};

static bool
portname_close_fail(void *context, FILE *active_file, size_t attempt,
    struct yt_database_close_observation *observation)
{
	struct portname_close_fault *fault = context;

	if (active_file == NULL || attempt != fault->calls + 1U)
		return false;
	++fault->calls;
	memset(observation, 0, sizeof(*observation));
	observation->carry = true;
	observation->handle_open = true;
	observation->dos_error = attempt == 1U ? 5U : 6U;
	return true;
}

static bool
test_portname_controller(void)
{
	static const uint8_t intro[] =
	    "\r"
	    "          Yankee Trader Remote Port Rename Program\r"
	    "                     By Alan Davenport\r"
	    "\r\r"
	    "This program will apply new, random port names to an existing game without\r"
	    "effecting any other setting. Do you wish to continue? [y/N] -=> ";
	static const uint8_t missing[] =
	    "\r\r\aERROR! DATA FILES NOT FOUND!!!!!!!!!!!!!!!!!!!!!!!!\a\r";
	static const uint8_t accepted[] =
	    "\r\rRenaming ports...\r"
	    " 1 Earth\r 2 Inging\r 3 Inging\r"
	    "\rNew, random names applied to all ports!\r";
	static const uint8_t zero_iteration[] =
	    "\r\rRenaming ports...\r"
	    "\rNew, random names applied to all ports!\r";
	static const uint8_t short_get[] =
	    "\r\rRenaming ports...\r 1 Earth\r 2 Inging\r"
	    "\rNew, random names applied to all ports!\r";
	static const uint8_t zero_draws[24] = {0};
	struct utility_random_script script = {
		zero_draws, sizeof(zero_draws), 0U
	};
	struct portname_output_tape tape = {0};
	struct yt_portname_output output;
	struct yt_portname_result result;
	struct yt_database database;
	struct yt_record before[3];
	struct yt_record after;
	struct yt_record expected;
	struct yt_random random;
	struct yt_error error;
	struct portname_close_fault close_fault;
	uint8_t parsed[256];
	size_t parsed_length;
	int index;
	bool valid = false;

	if (yt_portname_confirm(NULL, 0U) != YT_PORTNAME_CONFIRM_BLANK
	    || yt_portname_confirm((const uint8_t *)"", 0U)
	    != YT_PORTNAME_CONFIRM_BLANK
	    || yt_portname_confirm((const uint8_t *)"yes", 3U)
	    != YT_PORTNAME_CONFIRM_ACCEPT
	    || yt_portname_confirm((const uint8_t *)" y", 2U)
	    != YT_PORTNAME_CONFIRM_REJECT
	    || yt_portname_parse_confirmation((const uint8_t *)"  yes  ", 7U,
	    parsed, sizeof(parsed), &parsed_length) != YT_PORTNAME_PARSE_VALID
	    || parsed_length != 3U || memcmp(parsed, "yes", 3U) != 0
	    || yt_portname_parse_confirmation((const uint8_t *)"\" Y\" \t", 6U,
	    parsed, sizeof(parsed), &parsed_length) != YT_PORTNAME_PARSE_VALID
	    || parsed_length != 2U || memcmp(parsed, " Y", 2U) != 0
	    || yt_portname_parse_confirmation((const uint8_t *)"Y,anything", 10U,
	    parsed, sizeof(parsed), &parsed_length) != YT_PORTNAME_PARSE_REDO
	    || yt_portname_parse_confirmation((const uint8_t *)"\"Y\"X", 4U,
	    parsed, sizeof(parsed), &parsed_length) != YT_PORTNAME_PARSE_REDO)
		return false;
	if (!yt_portname_compose_output(YT_PORTNAME_OUTPUT_INTRO, 0.0f,
	    NULL, 0U, &output) || output.length != sizeof(intro) - 1U
	    || memcmp(output.bytes, intro, sizeof(intro) - 1U) != 0
	    || !yt_portname_compose_output(YT_PORTNAME_OUTPUT_MISSING_DATA,
	    0.0f, NULL, 0U, &output)
	    || output.length != sizeof(missing) - 1U
	    || memcmp(output.bytes, missing, sizeof(missing) - 1U) != 0
	    || !yt_portname_compose_output(YT_PORTNAME_OUTPUT_PROGRESS, 1.0f,
	    (const uint8_t *)"Earth", 5U, &output)
	    || output.length != 9U || memcmp(output.bytes, " 1 Earth\r", 9U) != 0
	    || yt_portname_record_number(1.5f, 1.0f) != 2U
	    || yt_portname_record_number(-2.0f, 0.5f) != 0x00fffffeU
	    || yt_portname_record_number(16777216.0f, 1.0f) != 0U
	    || yt_portname_record_number(-16777216.0f, -1.0f) != 0x00ffffffU)
		return false;
	(void)remove("PORTTEST.DAT");
	memset(&database, 0, sizeof(database));
	yt_error_clear(&error);
	if (!yt_database_open(&database, "PORTTEST.DAT", YT_OPEN_CREATE,
	    &error))
		goto done;
	for (index = 0; index < 3; ++index) {
		memset(&before[index], 0x90 + index, sizeof(before[index]));
		before[index].bytes[YT_RECORD_TAIL_OFFSET] =
		    (uint8_t)(0xE0 + index);
		if (!yt_record_set_number(&before[index], YT_F85, 4.0f)
		    || !yt_database_write(&database, (size_t)index + 2U,
		    &before[index], &error))
			goto done;
	}
	yt_random_init(&random);
	yt_random_set_provider(&random, utility_random_fill, &script);
	if (!yt_portname_rename(&database, 1.0f, 4.0f, &random,
	    portname_output_collect, &tape, &result, &error)
	    || result.loop_bound != 3.0f || result.final_logical_port != 4.0f
	    || result.iterations != 3 || result.draws_consumed != 8U
	    || !result.play_event || database.file != NULL
	    || random.draws != 8U || script.position != sizeof(zero_draws)
	    || tape.length != sizeof(accepted) - 1U
	    || memcmp(tape.bytes, accepted, sizeof(accepted) - 1U) != 0)
		goto done;
	if (!yt_database_open(&database, "PORTTEST.DAT", YT_OPEN_READ, &error))
		goto done;
	for (index = 0; index < 3; ++index) {
		const uint8_t *name = index == 0
		    ? (const uint8_t *)"Earth" : (const uint8_t *)"Inging";
		size_t length = index == 0 ? 5U : 6U;

		expected = before[index];
		if (!yt_portname_overlay_record(&expected, name, length, &error)
		    || !yt_database_read(&database, (size_t)index + 2U, &after,
		    &error)
		    || memcmp(after.bytes, expected.bytes, YT_RECORD_SIZE) != 0)
			goto done;
	}
	yt_database_close(&database);
	(void)remove("PORTTEST.DAT");
	memset(&tape, 0, sizeof(tape));
	script = (struct utility_random_script){NULL, 0U, 0U};
	yt_random_init(&random);
	yt_random_set_provider(&random, utility_random_fill, &script);
	if (!yt_database_open(&database, "PORTTEST.DAT", YT_OPEN_CREATE, &error)
	    || !yt_portname_rename(&database, 2.0f, 2.5f, &random,
	    portname_output_collect, &tape, &result, &error)
	    || result.loop_bound != 0.5f || result.iterations != 0
	    || result.draws_consumed != 0U || !result.play_event
	    || tape.length != sizeof(zero_iteration) - 1U
	    || memcmp(tape.bytes, zero_iteration,
	    sizeof(zero_iteration) - 1U) != 0)
		goto done;
	(void)remove("PORTTEST.DAT");
	memset(&tape, 0, sizeof(tape));
	memset(&result, 0, sizeof(result));
	script = (struct utility_random_script){zero_draws, 12U, 0U};
	yt_random_init(&random);
	yt_random_set_provider(&random, utility_random_fill, &script);
	if (!yt_database_open(&database, "PORTTEST.DAT", YT_OPEN_CREATE, &error)
	    || !yt_database_write(&database, 2U, &before[0], &error)
	    || !yt_portname_rename(&database, 1.0f, 3.0f, &random,
	    portname_output_collect, &tape, &result, &error)
	    || !result.play_event || database.file != NULL || random.draws != 4U
	    || result.iterations != 2
	    || tape.length != sizeof(short_get) - 1U
	    || memcmp(tape.bytes, short_get, sizeof(short_get) - 1U) != 0
	    || !yt_database_open(&database, "PORTTEST.DAT", YT_OPEN_READ,
	    &error))
		goto done;
	expected = before[0];
	if (!yt_portname_overlay_record(&expected, (const uint8_t *)"Earth", 5U,
	    &error)
	    || !yt_database_read(&database, 2U, &after, &error)
	    || memcmp(after.bytes, expected.bytes, YT_RECORD_SIZE) != 0)
		goto done;
	yt_record_clear(&expected);
	if (!yt_portname_overlay_record(&expected, (const uint8_t *)"Inging", 6U,
	    &error)
	    || !yt_database_read(&database, 3U, &after, &error)
	    || memcmp(after.bytes, expected.bytes, YT_RECORD_SIZE) != 0)
		goto done;
	yt_database_close(&database);
	(void)remove("PORTTEST.DAT");
	memset(&tape, 0, sizeof(tape));
	memset(&result, 0, sizeof(result));
	memset(&close_fault, 0, sizeof(close_fault));
	script = (struct utility_random_script){NULL, 0U, 0U};
	yt_random_init(&random);
	yt_random_set_provider(&random, utility_random_fill, &script);
	if (!yt_database_open(&database, "PORTTEST.DAT", YT_OPEN_CREATE, &error))
		goto done;
	yt_database_set_close_provider(&database, portname_close_fail,
	    &close_fault);
	yt_error_clear(&error);
	if (yt_portname_rename(&database, 2.0f, 2.5f, &random,
	    portname_output_collect, &tape, &result, &error)
	    || error.status != YT_IO_ERROR || result.play_event
	    || close_fault.calls != 2U
	    || database.last_close.outcome != YT_DATABASE_CLOSE_DISK_ERROR
	    || database.last_close.basic_error != 70U
	    || database.last_close.dos_error != 5U
	    || database.last_close.registered
	    || !database.last_close.handle_open
	    || database.file != NULL || database.orphaned_file == NULL
	    || tape.length != sizeof(zero_iteration) - 1U
	    || memcmp(tape.bytes, zero_iteration,
	    sizeof(zero_iteration) - 1U) != 0)
		goto done;
	valid = !yt_portname_rename(NULL, 1.0f, 4.0f, &random,
	    portname_output_collect, &tape, &result, &error)
	    && !yt_portname_overlay_record(NULL, (const uint8_t *)"", 0U,
	    &error);

done:
	yt_database_close(&database);
	(void)remove("PORTTEST.DAT");
	return valid;
}

static bool
test_initializer_bounded(void)
{
	static const uint8_t draws[] = {
		0x00, 0x00, 0x00,
		0x00, 0x00, 0x80,
		0xff, 0xff, 0xff
	};
	struct utility_random_script script = {draws, sizeof(draws), 0};
	struct yt_random random;
	struct yt_error error;
	int value;

	yt_error_clear(&error);
	yt_random_init(&random);
	yt_random_set_provider(&random, utility_random_fill, &script);
	if (!yt_initializer_bounded(&random, 10, &value, &error) || value != 1
	    || !yt_initializer_bounded(&random, 10, &value, &error) || value != 6
	    || !yt_initializer_bounded(&random, 2004, &value, &error)
	    || value != 2004 || random.draws != 3U
	    || script.position != sizeof(draws))
		return false;
	yt_error_clear(&error);
	return !yt_initializer_bounded(&random, 0, &value, &error)
	    && error.status == YT_RANGE && random.draws == 3U;
}

static bool
test_initializer_confirmation(void)
{
	return yt_initializer_confirm_response("Y")
	    && yt_initializer_confirm_response("y")
	    && !yt_initializer_confirm_response("")
	    && !yt_initializer_confirm_response("N")
	    && !yt_initializer_confirm_response("yes")
	    && !yt_initializer_confirm_response("Y\r")
	    && !yt_initializer_confirm_response("yX")
	    && !yt_initializer_confirm_response(NULL);
}

static bool
test_rmt_output_helpers(void)
{
	static const uint8_t missing[] =
	    "\aERROR! OLD DATA FILES NOT FOUND!!!!!!!!!!!!!!!!!!!!!!!!\a";
	struct yt_rmt_output_result result;
	uint8_t local[80];
	uint8_t serial[80];

	memset(local, 0xcc, sizeof(local));
	memset(serial, 0xdd, sizeof(serial));
	if (!yt_rmt_output_compose(YT_RMT_OUTPUT_LINE,
	    (const uint8_t *)"ABC", 3U, false, local, sizeof(local), serial,
	    sizeof(serial), &result)
	    || result.local_length != 4U || result.serial_length != 5U
	    || !result.serial_first || memcmp(local, "ABC\r", 4U) != 0
	    || memcmp(serial, "\nABC\r", 5U) != 0)
		return false;
	if (!yt_rmt_output_compose(YT_RMT_OUTPUT_BLANK, NULL, 0U, false,
	    local, sizeof(local), serial, sizeof(serial), &result)
	    || result.local_length != 1U || result.serial_length != 2U
	    || !result.serial_first || local[0] != '\r'
	    || memcmp(serial, "\n\r", 2U) != 0)
		return false;
	if (!yt_rmt_output_compose(YT_RMT_OUTPUT_INLINE,
	    (const uint8_t *)".", 1U, false, local, sizeof(local), serial,
	    sizeof(serial), &result)
	    || result.local_length != 1U || result.serial_length != 1U
	    || result.serial_first || local[0] != '.' || serial[0] != '.')
		return false;
	if (!yt_rmt_output_compose(YT_RMT_OUTPUT_LINE,
	    (const uint8_t *)"ABC", 3U, true, local, sizeof(local), NULL, 0U,
	    &result) || result.local_length != 4U || result.serial_length != 0U
	    || memcmp(local, "ABC\r", 4U) != 0)
		return false;
	if (!yt_rmt_output_compose(YT_RMT_OUTPUT_LINE, missing,
	    sizeof(missing) - 1U, false, local + 2U, sizeof(local) - 2U,
	    serial, sizeof(serial), &result)
	    || result.local_length != sizeof(missing)
	    || result.serial_length != sizeof(missing) + 1U
	    || !result.serial_first)
		return false;
	local[0] = '\r';
	local[1] = '\r';
	if (memcmp(local, "\r\r\aERROR! OLD DATA FILES NOT FOUND!!!!!!!!!!!!!!!!!!!!!!!!\a\r",
	    sizeof(missing) + 2U) != 0
	    || memcmp(serial,
	    "\n\aERROR! OLD DATA FILES NOT FOUND!!!!!!!!!!!!!!!!!!!!!!!!\a\r",
	    sizeof(missing) + 1U) != 0)
		return false;
	return !yt_rmt_output_compose(YT_RMT_OUTPUT_BLANK,
	    (const uint8_t *)"X", 1U, false, local, sizeof(local), serial,
	    sizeof(serial), &result)
	    && !yt_rmt_output_compose(YT_RMT_OUTPUT_LINE,
	    (const uint8_t *)"ABC", 3U, false, local, 3U, serial,
	    sizeof(serial), &result);
}

static bool
test_rmt_output_state(void)
{
	struct yt_rmt_output_state state = {0U, 0U};
	struct yt_rmt_output_state final;
	struct yt_rmt_output_result result;
	uint8_t local[64];
	uint8_t serial[64];

	if (!yt_rmt_output_compose_state(YT_RMT_OUTPUT_COMMA_SERIAL_FIRST,
	    (const uint8_t *)" 8 - 90", 7U, false, &state, local,
	    sizeof(local), serial, sizeof(serial), &result, &final)
	    || result.local_length != 14U || result.serial_length != 14U
	    || !result.serial_first || final.local_column != 14U
	    || final.serial_column != 14U
	    || memcmp(local, " 8 - 90       ", 14U) != 0
	    || memcmp(serial, local, 14U) != 0)
		return false;
	state.local_column = 70U;
	state.serial_column = 70U;
	if (!yt_rmt_output_compose_state(YT_RMT_OUTPUT_INLINE,
	    (const uint8_t *)"1234567890", 10U, false, &state, local,
	    sizeof(local), serial, sizeof(serial), &result, &final)
	    || result.local_length != 11U || result.serial_length != 11U
	    || result.serial_first || final.local_column != 10U
	    || final.serial_column != 10U
	    || memcmp(local, "\r1234567890", 11U) != 0
	    || memcmp(serial, local, 11U) != 0)
		return false;
	if (!yt_rmt_output_compose_state(YT_RMT_OUTPUT_COMMA_SERIAL_FIRST,
	    (const uint8_t *)".", 1U, false, &state, local, sizeof(local),
	    serial, sizeof(serial), &result, &final)
	    || result.local_length != 2U || result.serial_length != 2U
	    || final.local_column != 0U || final.serial_column != 0U
	    || memcmp(local, ".\r", 2U) != 0
	    || memcmp(serial, local, 2U) != 0)
		return false;
	state.local_column = 11U;
	state.serial_column = 56U;
	if (!yt_rmt_output_compose_state(YT_RMT_OUTPUT_SERIAL_LINE, NULL, 0U,
	    false, &state, local, sizeof(local), serial, sizeof(serial), &result,
	    &final) || result.local_length != 0U || result.serial_length != 2U
	    || !result.serial_first || final.local_column != 11U
	    || final.serial_column != 0U || memcmp(serial, "\n\r", 2U) != 0)
		return false;
	if (!yt_rmt_output_compose_state(YT_RMT_OUTPUT_SERIAL_LINE, NULL, 0U,
	    true, &state, local, sizeof(local), NULL, 0U, &result, &final)
	    || result.local_length != 0U || result.serial_length != 0U
	    || final.local_column != 11U || final.serial_column != 56U)
		return false;
	state.local_column = 80U;
	return !yt_rmt_output_compose_state(YT_RMT_OUTPUT_LINE,
	    (const uint8_t *)"X", 1U, false, &state, local, sizeof(local),
	    serial, sizeof(serial), &result, &final);
}

struct rmt_output_tape {
	uint8_t bytes[32];
	size_t length;
	enum yt_rmt_output_endpoint endpoints[2];
	size_t endpoint_count;
	enum yt_rmt_output_endpoint failure_endpoint;
	size_t failure_prefix;
	bool fail;
	bool overaccept;
};

static bool
rmt_output_tape_write(struct rmt_output_tape *tape,
    enum yt_rmt_output_endpoint endpoint, const uint8_t *data, size_t length,
    size_t *accepted)
{
	size_t amount = length;

	if (tape->endpoint_count >= 2U || length > sizeof(tape->bytes) - tape->length)
		return false;
	tape->endpoints[tape->endpoint_count++] = endpoint;
	if (tape->overaccept) {
		*accepted = length + 1U;
		return true;
	}
	if (tape->fail && tape->failure_endpoint == endpoint
	    && tape->failure_prefix < amount)
		amount = tape->failure_prefix;
	if (amount != 0U)
		memcpy(tape->bytes + tape->length, data, amount);
	tape->length += amount;
	*accepted = amount;
	return !(tape->fail && tape->failure_endpoint == endpoint);
}

static bool
rmt_output_tape_local(void *context, const uint8_t *data, size_t length,
    size_t *accepted)
{
	return rmt_output_tape_write(context, YT_RMT_OUTPUT_ENDPOINT_LOCAL, data,
	    length, accepted);
}

static bool
rmt_output_tape_serial(void *context, const uint8_t *data, size_t length,
    size_t *accepted)
{
	return rmt_output_tape_write(context, YT_RMT_OUTPUT_ENDPOINT_SERIAL, data,
	    length, accepted);
}

static bool
test_rmt_output_adapter(void)
{
	struct yt_rmt_output_apply_result applied;
	struct yt_rmt_output_result output;
	struct rmt_output_tape tape;
	struct yt_rmt_output_sink sink = {
		.context = &tape,
		.local = rmt_output_tape_local,
		.serial = rmt_output_tape_serial,
	};
	uint8_t local[8];
	uint8_t serial[8];

	memset(&tape, 0, sizeof(tape));
	if (!yt_rmt_output_compose(YT_RMT_OUTPUT_LINE,
	    (const uint8_t *)"ABC", 3U, false, local, sizeof(local), serial,
	    sizeof(serial), &output)
	    || !yt_rmt_output_apply(local, serial, &output, &sink, &applied)
	    || applied.outcome != YT_RMT_OUTPUT_APPLY_SUCCESS
	    || applied.attempt_count != 2U
	    || applied.attempts[0].endpoint != YT_RMT_OUTPUT_ENDPOINT_SERIAL
	    || applied.attempts[0].requested != 5U
	    || applied.attempts[0].accepted != 5U
	    || applied.attempts[1].endpoint != YT_RMT_OUTPUT_ENDPOINT_LOCAL
	    || applied.attempts[1].requested != 4U
	    || applied.attempts[1].accepted != 4U
	    || tape.endpoint_count != 2U
	    || tape.endpoints[0] != YT_RMT_OUTPUT_ENDPOINT_SERIAL
	    || tape.endpoints[1] != YT_RMT_OUTPUT_ENDPOINT_LOCAL
	    || tape.length != 9U || memcmp(tape.bytes, "\nABC\rABC\r", 9U) != 0)
		return false;

	memset(&tape, 0, sizeof(tape));
	if (!yt_rmt_output_compose(YT_RMT_OUTPUT_INLINE,
	    (const uint8_t *)".", 1U, false, local, sizeof(local), serial,
	    sizeof(serial), &output)
	    || !yt_rmt_output_apply(local, serial, &output, &sink, &applied)
	    || applied.outcome != YT_RMT_OUTPUT_APPLY_SUCCESS
	    || applied.attempt_count != 2U
	    || tape.endpoints[0] != YT_RMT_OUTPUT_ENDPOINT_LOCAL
	    || tape.endpoints[1] != YT_RMT_OUTPUT_ENDPOINT_SERIAL
	    || tape.length != 2U || memcmp(tape.bytes, "..", 2U) != 0)
		return false;

	memset(&tape, 0, sizeof(tape));
	tape.fail = true;
	tape.failure_endpoint = YT_RMT_OUTPUT_ENDPOINT_SERIAL;
	tape.failure_prefix = 2U;
	if (!yt_rmt_output_compose(YT_RMT_OUTPUT_LINE,
	    (const uint8_t *)"ABC", 3U, false, local, sizeof(local), serial,
	    sizeof(serial), &output)
	    || !yt_rmt_output_apply(local, serial, &output, &sink, &applied)
	    || applied.outcome != YT_RMT_OUTPUT_APPLY_SERIAL_FAILURE
	    || applied.attempt_count != 1U || applied.attempts[0].accepted != 2U
	    || tape.endpoint_count != 1U || tape.length != 2U
	    || memcmp(tape.bytes, "\nA", 2U) != 0)
		return false;

	memset(&tape, 0, sizeof(tape));
	tape.fail = true;
	tape.failure_endpoint = YT_RMT_OUTPUT_ENDPOINT_LOCAL;
	tape.failure_prefix = 2U;
	if (!yt_rmt_output_apply(local, serial, &output, &sink, &applied)
	    || applied.outcome != YT_RMT_OUTPUT_APPLY_LOCAL_FAILURE
	    || applied.attempt_count != 2U
	    || applied.attempts[0].endpoint != YT_RMT_OUTPUT_ENDPOINT_SERIAL
	    || applied.attempts[0].accepted != 5U
	    || applied.attempts[1].endpoint != YT_RMT_OUTPUT_ENDPOINT_LOCAL
	    || applied.attempts[1].accepted != 2U
	    || tape.endpoint_count != 2U || tape.length != 7U
	    || memcmp(tape.bytes, "\nABC\rAB", 7U) != 0)
		return false;

	memset(&tape, 0, sizeof(tape));
	tape.fail = true;
	tape.failure_endpoint = YT_RMT_OUTPUT_ENDPOINT_LOCAL;
	tape.failure_prefix = 0U;
	if (!yt_rmt_output_compose(YT_RMT_OUTPUT_INLINE,
	    (const uint8_t *)".", 1U, false, local, sizeof(local), serial,
	    sizeof(serial), &output)
	    || !yt_rmt_output_apply(local, serial, &output, &sink, &applied)
	    || applied.outcome != YT_RMT_OUTPUT_APPLY_LOCAL_FAILURE
	    || applied.attempt_count != 1U || tape.endpoint_count != 1U
	    || tape.endpoints[0] != YT_RMT_OUTPUT_ENDPOINT_LOCAL
	    || tape.length != 0U)
		return false;

	memset(&tape, 0, sizeof(tape));
	if (!yt_rmt_output_compose(YT_RMT_OUTPUT_LINE,
	    (const uint8_t *)"ABC", 3U, true, local, sizeof(local), NULL, 0U,
	    &output))
		return false;
	sink.serial = NULL;
	if (!yt_rmt_output_apply(local, NULL, &output, &sink, &applied)
	    || applied.outcome != YT_RMT_OUTPUT_APPLY_SUCCESS
	    || applied.attempt_count != 1U
	    || applied.attempts[0].endpoint != YT_RMT_OUTPUT_ENDPOINT_LOCAL)
		return false;

	memset(&tape, 0, sizeof(tape));
	tape.overaccept = true;
	sink.serial = rmt_output_tape_serial;
	if (yt_rmt_output_apply(local, NULL, &output, &sink, &applied)
	    || yt_rmt_output_apply(NULL, NULL, NULL, &sink, &applied)
	    || yt_rmt_output_apply(NULL, NULL, &output, &sink, &applied))
		return false;
	sink.local = NULL;
	return !yt_rmt_output_apply(local, NULL, &output, &sink, &applied);
}

static bool
test_rmt_completion_output(void)
{
	static const uint8_t completed[] =
	    "\aInitialization completed sucessfully!\a";
	static const uint8_t congratulations[] =
	    "Congratulations Last Winner! You have fulfilled the prophesy!!";
	static const uint8_t returning[] = "Returning you to the BBS...";
	struct yt_rmt_completion_result result;
	struct yt_rmt_delay_result delay;
	char too_long[YT_RMT_COMPLETION_PAYLOAD + 1U];

	if (!yt_rmt_completion_compose(true, "The Sysop", &result)
	    || result.line_count != 1U || result.returns_to_bbs
	    || result.lines[0].length != sizeof(completed) - 1U
	    || memcmp(result.lines[0].bytes, completed,
	    sizeof(completed) - 1U) != 0)
		return false;
	if (!yt_rmt_completion_compose(false, "Last Winner", &result)
	    || result.line_count != 6U || !result.returns_to_bbs
	    || result.lines[1].length != 0U
	    || result.lines[2].length != sizeof(congratulations) - 1U
	    || memcmp(result.lines[2].bytes, congratulations,
	    sizeof(congratulations) - 1U) != 0
	    || result.lines[3].length != result.lines[2].length
	    || memcmp(result.lines[3].bytes, result.lines[2].bytes,
	    result.lines[2].length) != 0
	    || result.lines[4].length != result.lines[2].length
	    || memcmp(result.lines[4].bytes, result.lines[2].bytes,
	    result.lines[2].length) != 0
	    || result.lines[5].length != sizeof(returning) - 1U
	    || memcmp(result.lines[5].bytes, returning,
	    sizeof(returning) - 1U) != 0)
		return false;
	memset(too_long, 'X', sizeof(too_long) - 1U);
	too_long[sizeof(too_long) - 1U] = '\0';
	return yt_rmt_completion_delay(&delay)
	    && delay.admitted_values == 2222U
	    && delay.final_value == 2223.0f
	    && !yt_rmt_completion_delay(NULL)
	    && !yt_rmt_completion_compose(false, too_long, &result)
	    && !yt_rmt_completion_compose(false, NULL, &result)
	    && !yt_rmt_completion_compose(false, "The Sysop", NULL);
}

static bool
test_rmt_standalone_entry(void)
{
	static const uint8_t prompt[] =
	    "\r\r"
	    "Running stand alone... re-initializing using old sysop defined defaults.\r"
	    "\r"
	    "Do you wish to re-init Y.T. using your old default values?";
	struct yt_rmt_standalone_output output;
	uint8_t long_response[189];

	if (!yt_rmt_standalone_prompt_compose(&output)
	    || output.proceed || output.length != sizeof(prompt) - 1U
	    || memcmp(output.bytes, prompt, sizeof(prompt) - 1U) != 0
	    || !yt_rmt_standalone_response_compose((const uint8_t *)"Y", 1U,
	    &output) || !output.proceed || output.length != 6U
	    || memcmp(output.bytes, " Y  \r\r", 6U) != 0
	    || !yt_rmt_standalone_response_compose((const uint8_t *)"y", 1U,
	    &output) || !output.proceed
	    || !yt_rmt_standalone_response_compose((const uint8_t *)"N", 1U,
	    &output) || output.proceed || output.length != 5U
	    || memcmp(output.bytes, " N  \r", 5U) != 0
	    || !yt_rmt_standalone_response_compose((const uint8_t *)"yes", 3U,
	    &output) || output.proceed || output.length != 7U
	    || memcmp(output.bytes, " yes  \r", 7U) != 0
	    || !yt_rmt_standalone_response_compose(NULL, 0U, &output)
	    || output.proceed || output.length != 4U
	    || memcmp(output.bytes, "   \r", 4U) != 0
	    || yt_rmt_standalone_prompt_compose(NULL)
	    || yt_rmt_standalone_response_compose(NULL, 1U, &output))
		return false;
	memset(long_response, 'X', sizeof(long_response));
	return !yt_rmt_standalone_response_compose(long_response,
	    sizeof(long_response), &output);
}

static bool
test_rmt_handoff_parser(void)
{
	static const uint8_t cr[] = "C:\\BBS\\DORINFO1.DEF\rignored";
	static const uint8_t lf[] = "door.sys\nignored";
	static const uint8_t eof[] = "node.def\x1aignored";
	static const uint8_t leading_cr[] = "\rignored";
	struct yt_rmt_handoff_result result;
	uint8_t too_long[512];

	if (!yt_rmt_handoff_parse(NULL, 0U, &result) || !result.standalone
	    || result.path_length != 0U || result.path[0] != 0U
	    || !yt_rmt_handoff_parse(cr, sizeof(cr) - 1U, &result)
	    || result.standalone || result.path_length != 19U
	    || memcmp(result.path, "C:\\BBS\\DORINFO1.DEF", 19U) != 0
	    || !yt_rmt_handoff_parse(lf, sizeof(lf) - 1U, &result)
	    || result.standalone || result.path_length != 8U
	    || memcmp(result.path, "door.sys", 8U) != 0
	    || !yt_rmt_handoff_parse(eof, sizeof(eof) - 1U, &result)
	    || result.standalone || result.path_length != 8U
	    || memcmp(result.path, "node.def", 8U) != 0
	    || !yt_rmt_handoff_parse(leading_cr, sizeof(leading_cr) - 1U,
	    &result) || result.standalone || result.path_length != 0U
	    || result.path[0] != 0U || yt_rmt_handoff_parse(NULL, 1U, &result)
	    || yt_rmt_handoff_parse(NULL, 0U, NULL))
		return false;
	memset(too_long, 'X', sizeof(too_long));
	return !yt_rmt_handoff_parse(too_long, sizeof(too_long), &result);
}

struct rmt_handoff_read_script {
	size_t fail_at;
	size_t call_count;
	enum yt_rmt_handoff_read_operation calls[6];
	uint32_t size;
	const uint8_t *line;
	size_t line_length;
	bool line_available;
};

static bool
rmt_handoff_read_record(struct rmt_handoff_read_script *script,
    enum yt_rmt_handoff_read_operation operation)
{
	if (script->call_count >= sizeof(script->calls) / sizeof(script->calls[0]))
		return false;
	script->calls[script->call_count] = operation;
	return script->call_count++ != script->fail_at;
}

static bool
rmt_handoff_read_open_random(void *context, struct yt_error *error)
{
	(void)error;
	return rmt_handoff_read_record(context,
	    YT_RMT_HANDOFF_READ_OPEN_RANDOM);
}

static bool
rmt_handoff_read_lof(void *context, uint32_t *size,
    struct yt_error *error)
{
	struct rmt_handoff_read_script *script = context;

	(void)error;
	if (!rmt_handoff_read_record(script, YT_RMT_HANDOFF_READ_LOF))
		return false;
	*size = script->size;
	return true;
}

static bool
rmt_handoff_read_close_random(void *context, struct yt_error *error)
{
	(void)error;
	return rmt_handoff_read_record(context,
	    YT_RMT_HANDOFF_READ_CLOSE_RANDOM);
}

static bool
rmt_handoff_read_open_sequential(void *context, struct yt_error *error)
{
	(void)error;
	return rmt_handoff_read_record(context,
	    YT_RMT_HANDOFF_READ_OPEN_SEQUENTIAL);
}

static bool
rmt_handoff_read_line(void *context, const uint8_t **line, size_t *length,
    bool *available, struct yt_error *error)
{
	struct rmt_handoff_read_script *script = context;

	(void)error;
	if (!rmt_handoff_read_record(script, YT_RMT_HANDOFF_READ_LINE))
		return false;
	*line = script->line;
	*length = script->line_length;
	*available = script->line_available;
	return true;
}

static bool
rmt_handoff_read_close_sequential(void *context, struct yt_error *error)
{
	(void)error;
	return rmt_handoff_read_record(context,
	    YT_RMT_HANDOFF_READ_CLOSE_SEQUENTIAL);
}

static bool
test_rmt_handoff_read_transaction(void)
{
	static const struct yt_rmt_handoff_read_ops ops = {
		rmt_handoff_read_open_random,
		rmt_handoff_read_lof,
		rmt_handoff_read_close_random,
		rmt_handoff_read_open_sequential,
		rmt_handoff_read_line,
		rmt_handoff_read_close_sequential,
	};
	static const enum yt_rmt_handoff_read_operation expected_calls[] = {
		YT_RMT_HANDOFF_READ_OPEN_RANDOM,
		YT_RMT_HANDOFF_READ_LOF,
		YT_RMT_HANDOFF_READ_CLOSE_RANDOM,
		YT_RMT_HANDOFF_READ_OPEN_SEQUENTIAL,
		YT_RMT_HANDOFF_READ_LINE,
		YT_RMT_HANDOFF_READ_CLOSE_SEQUENTIAL,
	};
	static const uint8_t line[] = "DORINFO1.DEF";
	struct yt_rmt_handoff_read_ops incomplete = ops;
	struct rmt_handoff_read_script script;
	struct yt_rmt_handoff_read_state state;
	uint8_t too_long[512];
	size_t failed;

	script = (struct rmt_handoff_read_script){
		.fail_at = SIZE_MAX,
		.size = 15U,
		.line = line,
		.line_length = sizeof(line) - 1U,
		.line_available = true,
	};
	if (!yt_rmt_handoff_read_run(&state, &ops, &script, NULL)
	    || script.call_count != 6U
	    || memcmp(script.calls, expected_calls, sizeof(expected_calls)) != 0
	    || state.failed_operation != YT_RMT_HANDOFF_READ_NONE
	    || state.size != 15U || !state.random_opened || !state.lof_read
	    || !state.random_closed || !state.sequential_opened
	    || !state.line_read || !state.line_available
	    || state.empty_line_substituted || !state.line_parsed
	    || !state.sequential_closed || !state.complete
	    || state.result.standalone
	    || state.result.path_length != sizeof(line) - 1U
	    || memcmp(state.result.path, line, sizeof(line) - 1U) != 0)
		return false;

	memset(&script, 0, sizeof(script));
	script.fail_at = SIZE_MAX;
	if (!yt_rmt_handoff_read_run(&state, &ops, &script, NULL)
	    || script.call_count != 2U
	    || script.calls[0] != YT_RMT_HANDOFF_READ_OPEN_RANDOM
	    || script.calls[1] != YT_RMT_HANDOFF_READ_LOF
	    || !state.random_opened || !state.lof_read || state.random_closed
	    || state.sequential_opened || !state.line_parsed || !state.complete
	    || !state.result.standalone || state.result.path_length != 0U)
		return false;

	script = (struct rmt_handoff_read_script){
		.fail_at = SIZE_MAX,
		.size = 1U,
	};
	if (!yt_rmt_handoff_read_run(&state, &ops, &script, NULL)
	    || !state.empty_line_substituted || state.line_available
	    || state.result.standalone || state.result.path_length != 0U
	    || script.call_count != 6U)
		return false;

	for (failed = 0U; failed < 6U; ++failed) {
		script = (struct rmt_handoff_read_script){
			.fail_at = failed,
			.size = 15U,
			.line = line,
			.line_length = sizeof(line) - 1U,
			.line_available = true,
		};
		if (yt_rmt_handoff_read_run(&state, &ops, &script, NULL)
		    || state.failed_operation != expected_calls[failed]
		    || script.call_count != failed + 1U
		    || state.random_opened != (failed > 0U)
		    || state.lof_read != (failed > 1U)
		    || state.random_closed != (failed > 2U)
		    || state.sequential_opened != (failed > 3U)
		    || state.line_read != (failed > 4U)
		    || state.line_parsed != (failed > 4U)
		    || state.sequential_closed || state.complete)
			return false;
	}

	memset(too_long, 'X', sizeof(too_long));
	script = (struct rmt_handoff_read_script){
		.fail_at = SIZE_MAX,
		.size = 512U,
		.line = too_long,
		.line_length = sizeof(too_long),
		.line_available = true,
	};
	if (yt_rmt_handoff_read_run(&state, &ops, &script, NULL)
	    || state.failed_operation != YT_RMT_HANDOFF_READ_PARSE_LINE
	    || script.call_count != 5U || !state.line_read || state.line_parsed
	    || state.sequential_closed || state.complete)
		return false;

	incomplete.close_sequential = NULL;
	return !yt_rmt_handoff_read_run(&state, &incomplete, &script, NULL)
	    && !yt_rmt_handoff_read_run(NULL, &ops, &script, NULL)
	    && !yt_rmt_handoff_read_run(&state, NULL, &script, NULL);
}

struct rmt_handoff_cleanup_script {
	size_t fail_at;
	size_t call_count;
	enum yt_rmt_handoff_cleanup_operation calls[2];
};

static bool
rmt_handoff_cleanup_record(struct rmt_handoff_cleanup_script *script,
    enum yt_rmt_handoff_cleanup_operation operation)
{
	if (script->call_count >= sizeof(script->calls) / sizeof(script->calls[0]))
		return false;
	script->calls[script->call_count] = operation;
	return script->call_count++ != script->fail_at;
}

static bool
rmt_handoff_cleanup_close(void *context, struct yt_error *error)
{
	(void)error;
	return rmt_handoff_cleanup_record(context,
	    YT_RMT_HANDOFF_CLEANUP_CLOSE);
}

static bool
rmt_handoff_cleanup_kill(void *context, struct yt_error *error)
{
	(void)error;
	return rmt_handoff_cleanup_record(context,
	    YT_RMT_HANDOFF_CLEANUP_KILL);
}

static bool
test_rmt_handoff_cleanup_transaction(void)
{
	static const struct yt_rmt_handoff_cleanup_ops ops = {
		rmt_handoff_cleanup_close,
		rmt_handoff_cleanup_kill,
	};
	static const enum yt_rmt_handoff_cleanup_operation expected_calls[] = {
		YT_RMT_HANDOFF_CLEANUP_CLOSE,
		YT_RMT_HANDOFF_CLEANUP_KILL,
	};
	struct yt_rmt_handoff_cleanup_ops incomplete = ops;
	struct rmt_handoff_cleanup_script script;
	struct yt_rmt_handoff_cleanup_state state;
	size_t failed;

	script = (struct rmt_handoff_cleanup_script){.fail_at = SIZE_MAX};
	if (!yt_rmt_handoff_cleanup_run(&state, &ops, &script, NULL)
	    || script.call_count != 2U
	    || memcmp(script.calls, expected_calls, sizeof(expected_calls)) != 0
	    || state.failed_operation != YT_RMT_HANDOFF_CLEANUP_NONE
	    || !state.close_attempted || !state.close_completed
	    || !state.kill_attempted || !state.kill_completed || !state.complete)
		return false;
	for (failed = 0U; failed < 2U; ++failed) {
		script = (struct rmt_handoff_cleanup_script){.fail_at = failed};
		if (yt_rmt_handoff_cleanup_run(&state, &ops, &script, NULL)
		    || state.failed_operation != expected_calls[failed]
		    || script.call_count != failed + 1U
		    || !state.close_attempted
		    || state.close_completed != (failed > 0U)
		    || state.kill_attempted != (failed > 0U)
		    || state.kill_completed || state.complete)
			return false;
	}
	incomplete.kill = NULL;
	return !yt_rmt_handoff_cleanup_run(&state, &incomplete, &script, NULL)
	    && !yt_rmt_handoff_cleanup_run(NULL, &ops, &script, NULL)
	    && !yt_rmt_handoff_cleanup_run(&state, NULL, &script, NULL);
}

static bool
test_rmt_dorinfo_parser(void)
{
	static const uint8_t raw[] = {
	    'd','i','s','c','a','r','d','1','\r','\n',
	    'd','i','s',0,'c','a','r','d','2','\r',
	    'd','i','s','c','a','r','d','3','\r','\n',
	    'C','O','M','4',':','\r','\n',
	    '9','6','0','0',' ','b','a','u','d',',','n',',','8',',','1','\r','\n',
	    'u','n','u','s','e','d','\n','b','a','r','e','\r','\n',
	    ' ','j','a','n','e',' ','\r','\n',
	    ' ','d','o','e',' ','\r','\n',
	    'i','g','n','o','r','e','d',' ','n','i','n','t','h','\r','\n'
	};
	static const char *expected[YT_RMT_DORINFO_FIELDS] = {
		"discard1", "discard2", "discard3", "COM4:",
		"9600 baud,n,8,1", "unused\nbare", " jane ", " doe "
	};
	struct yt_rmt_dorinfo_result result;
	uint8_t storage[2048];
	uint8_t truncated[32];
	size_t field;
	size_t used;

	if (!yt_rmt_dorinfo_parse(raw, sizeof(raw), storage, sizeof(storage),
	    &result) || result.outcome != YT_RMT_DORINFO_SUCCESS
	    || result.fields_assigned != YT_RMT_DORINFO_FIELDS
	    || result.failed_field != 0U || result.error_number != 0
	    || result.cursor >= sizeof(raw) || raw[result.cursor] != 'i')
		return false;
	for (field = 0U; field < YT_RMT_DORINFO_FIELDS; ++field) {
		const uint8_t *value;
		size_t length;

		value = yt_rmt_dorinfo_field(&result, storage, field, &length);
		if (value == NULL || length != strlen(expected[field])
		    || memcmp(value, expected[field], length) != 0)
			return false;
	}
	used = 0U;
	for (field = 1U; field <= YT_RMT_DORINFO_FIELDS; ++field) {
		size_t prior;

		used = 0U;
		for (prior = 1U; prior < field; ++prior) {
			truncated[used++] = (uint8_t)('0' + prior);
			truncated[used++] = '\r';
		}
		truncated[used] = 0x1aU;
		if (!yt_rmt_dorinfo_parse(truncated, used + 1U, storage,
		    sizeof(storage), &result)
		    || result.outcome != YT_RMT_DORINFO_INPUT_PAST_END
		    || result.fields_assigned != field - 1U
		    || result.failed_field != field || result.cursor != used
		    || result.error_number != 62)
			return false;
	}
	memset(truncated, 0, sizeof(truncated));
	used = 0U;
	for (field = 1U; field <= YT_RMT_DORINFO_FIELDS; ++field) {
		truncated[used++] = field == YT_RMT_DORINFO_FIELDS
		    ? 'Z' : (uint8_t)('0' + field);
		if (field != YT_RMT_DORINFO_FIELDS)
			truncated[used++] = '\r';
	}
	truncated[used] = 0x1aU;
	if (!yt_rmt_dorinfo_parse(truncated, used + 1U, storage,
	    sizeof(storage), &result)
	    || result.outcome != YT_RMT_DORINFO_SUCCESS
	    || result.cursor != used
	    || result.fields[7].length != 1U
	    || storage[result.fields[7].offset] != 'Z'
	    || yt_rmt_dorinfo_field(&result, storage,
	    YT_RMT_DORINFO_FIELDS, &used) != NULL
	    || yt_rmt_dorinfo_parse((const uint8_t *)"X\r", 2U, NULL, 0U,
	    &result) || yt_rmt_dorinfo_parse(NULL, 1U, storage,
	    sizeof(storage), &result)
	    || yt_rmt_dorinfo_parse(NULL, 0U, storage,
	    sizeof(storage), NULL))
		return false;
	return true;
}

struct rmt_dorinfo_read_script {
	bool fail_open;
	bool fail_close;
	size_t fail_read;
	size_t eof_read;
	size_t read_index;
	size_t call_count;
	enum yt_rmt_dorinfo_read_operation calls[10];
	const uint8_t *lines[YT_RMT_DORINFO_FIELDS];
	size_t lengths[YT_RMT_DORINFO_FIELDS];
};

static void
rmt_dorinfo_read_script_init(struct rmt_dorinfo_read_script *script)
{
	static const uint8_t values[YT_RMT_DORINFO_FIELDS][9] = {
		"A", "BB", "CCC", "DDDD", "EEEEE", "FFFFFF", "GGGGGGG",
		"HHHHHHHH",
	};
	size_t field;

	memset(script, 0, sizeof(*script));
	script->fail_read = SIZE_MAX;
	script->eof_read = SIZE_MAX;
	for (field = 0U; field < YT_RMT_DORINFO_FIELDS; ++field) {
		script->lines[field] = values[field];
		script->lengths[field] = field + 1U;
	}
}

static bool
rmt_dorinfo_read_record(struct rmt_dorinfo_read_script *script,
    enum yt_rmt_dorinfo_read_operation operation)
{
	if (script->call_count >= sizeof(script->calls) / sizeof(script->calls[0]))
		return false;
	script->calls[script->call_count++] = operation;
	return true;
}

static bool
rmt_dorinfo_read_open(void *context, struct yt_error *error)
{
	struct rmt_dorinfo_read_script *script = context;

	(void)error;
	return rmt_dorinfo_read_record(script, YT_RMT_DORINFO_READ_OPEN)
	    && !script->fail_open;
}

static bool
rmt_dorinfo_read_line(void *context, const uint8_t **line, size_t *length,
    bool *available, size_t *cursor, struct yt_error *error)
{
	struct rmt_dorinfo_read_script *script = context;
	size_t field = script->read_index++;

	(void)error;
	if (!rmt_dorinfo_read_record(script, YT_RMT_DORINFO_READ_FIELD)
	    || field >= YT_RMT_DORINFO_FIELDS)
		return false;
	*cursor = 100U + field;
	if (field == script->fail_read)
		return false;
	*line = script->lines[field];
	*length = script->lengths[field];
	*available = field != script->eof_read;
	return true;
}

static bool
rmt_dorinfo_read_close(void *context, struct yt_error *error)
{
	struct rmt_dorinfo_read_script *script = context;

	(void)error;
	return rmt_dorinfo_read_record(script, YT_RMT_DORINFO_READ_CLOSE)
	    && !script->fail_close;
}

static bool
test_rmt_dorinfo_read_transaction(void)
{
	static const struct yt_rmt_dorinfo_read_ops ops = {
		rmt_dorinfo_read_open,
		rmt_dorinfo_read_line,
		rmt_dorinfo_read_close,
	};
	struct yt_rmt_dorinfo_read_ops incomplete = ops;
	struct rmt_dorinfo_read_script script;
	struct yt_rmt_dorinfo_read_state state;
	struct yt_error error;
	uint8_t storage[64];
	size_t field;
	size_t offset;

	rmt_dorinfo_read_script_init(&script);
	if (!yt_rmt_dorinfo_read_run(&state, &ops, &script, storage,
	    sizeof(storage), NULL)
	    || state.failed_operation != YT_RMT_DORINFO_READ_NONE
	    || !state.file_opened || state.read_attempts != 8U
	    || state.result.outcome != YT_RMT_DORINFO_SUCCESS
	    || state.result.fields_assigned != 8U || state.storage_used != 36U
	    || state.result.cursor != 107U
	    || !state.close_attempted || !state.file_closed || !state.complete
	    || script.call_count != 10U || script.read_index != 8U
	    || script.calls[0] != YT_RMT_DORINFO_READ_OPEN
	    || script.calls[9] != YT_RMT_DORINFO_READ_CLOSE)
		return false;
	offset = 0U;
	for (field = 0U; field < YT_RMT_DORINFO_FIELDS; ++field) {
		if (script.calls[field + 1U] != YT_RMT_DORINFO_READ_FIELD
		    || state.result.fields[field].offset != offset
		    || state.result.fields[field].length != field + 1U
		    || memcmp(storage + offset, script.lines[field], field + 1U)
		    != 0)
			return false;
		offset += field + 1U;
	}

	rmt_dorinfo_read_script_init(&script);
	script.fail_open = true;
	if (yt_rmt_dorinfo_read_run(&state, &ops, &script, storage,
	    sizeof(storage), NULL)
	    || state.failed_operation != YT_RMT_DORINFO_READ_OPEN
	    || state.file_opened || state.read_attempts != 0U
	    || state.close_attempted || script.call_count != 1U)
		return false;

	for (field = 0U; field < YT_RMT_DORINFO_FIELDS; ++field) {
		rmt_dorinfo_read_script_init(&script);
		script.fail_read = field;
		if (yt_rmt_dorinfo_read_run(&state, &ops, &script, storage,
		    sizeof(storage), NULL)
		    || state.failed_operation != YT_RMT_DORINFO_READ_FIELD
		    || !state.file_opened || state.read_attempts != field + 1U
		    || state.result.fields_assigned != field
		    || state.result.failed_field != field + 1U
		    || state.result.cursor != 100U + field
		    || state.close_attempted || state.complete
		    || script.call_count != field + 2U)
			return false;

		rmt_dorinfo_read_script_init(&script);
		script.eof_read = field;
		yt_error_clear(&error);
		if (yt_rmt_dorinfo_read_run(&state, &ops, &script, storage,
		    sizeof(storage), &error)
		    || state.failed_operation != YT_RMT_DORINFO_READ_FIELD
		    || state.result.outcome != YT_RMT_DORINFO_INPUT_PAST_END
		    || state.result.fields_assigned != field
		    || state.result.failed_field != field + 1U
		    || state.result.cursor != 100U + field
		    || state.result.error_number != 62 || error.status != YT_EOF
		    || state.close_attempted || state.complete)
			return false;
	}

	rmt_dorinfo_read_script_init(&script);
	if (yt_rmt_dorinfo_read_run(&state, &ops, &script, storage, 1U, NULL)
	    || state.failed_operation != YT_RMT_DORINFO_READ_COPY_FIELD
	    || state.result.fields_assigned != 1U
	    || state.result.failed_field != 2U || state.storage_used != 1U
	    || state.close_attempted || state.complete)
		return false;

	rmt_dorinfo_read_script_init(&script);
	script.lines[0] = NULL;
	if (yt_rmt_dorinfo_read_run(&state, &ops, &script, storage,
	    sizeof(storage), NULL)
	    || state.failed_operation != YT_RMT_DORINFO_READ_COPY_FIELD
	    || state.result.failed_field != 1U || state.storage_used != 0U)
		return false;

	rmt_dorinfo_read_script_init(&script);
	script.fail_close = true;
	if (yt_rmt_dorinfo_read_run(&state, &ops, &script, storage,
	    sizeof(storage), NULL)
	    || state.failed_operation != YT_RMT_DORINFO_READ_CLOSE
	    || state.result.fields_assigned != 8U || !state.close_attempted
	    || state.file_closed || state.complete || script.call_count != 10U)
		return false;

	incomplete.close = NULL;
	return !yt_rmt_dorinfo_read_run(&state, &incomplete, &script, storage,
	    sizeof(storage), NULL)
	    && !yt_rmt_dorinfo_read_run(NULL, &ops, &script, storage,
	    sizeof(storage), NULL)
	    && !yt_rmt_dorinfo_read_run(&state, NULL, &script, storage,
	    sizeof(storage), NULL)
	    && !yt_rmt_dorinfo_read_run(&state, &ops, &script, NULL, 1U, NULL);
}

static bool
test_rmt_remote_status(void)
{
	struct yt_rmt_standalone_output output;

	return yt_rmt_remote_status_compose(true, 2.0f, 2400.0f, &output)
	    && output.length == 32U
	    && memcmp(output.bytes, "Opening COM port 2 at 2400 baud\r", 32U)
	    == 0
	    && yt_rmt_remote_status_compose(false, 0.0f, 0.0f, &output)
	    && output.length == 19U
	    && memcmp(output.bytes, "Local Console Mode\r", 19U) == 0
	    && !yt_rmt_remote_status_compose(false, 0.0f, 0.0f, NULL);
}

static bool
serial_event_is(const struct yt_rmt_serial_event_result *result,
    size_t index, enum yt_rmt_serial_event_operation operation,
    uint16_t address, uint16_t value, int error_number, bool complete)
{
	const struct yt_rmt_serial_event *event;

	if (index >= result->event_count)
		return false;
	event = &result->events[index];
	return event->operation == operation && event->address == address
	    && event->value == value && event->error_number == error_number
	    && event->complete == complete;
}

static bool
test_rmt_remote_serial(void)
{
	struct yt_rmt_serial_event_result events;
	struct yt_rmt_serial_state state;

	if (!yt_rmt_serial_state_compose((const uint8_t *)"COM0", 4U,
	    (const uint8_t *)"ignored", 7U, 0U, 0U, &state)
	    || state.outcome != YT_RMT_SERIAL_LOCAL
	    || state.requested_port != 0 || state.detected_baud != 0.0f
	    || !yt_rmt_serial_events_compose(&state, 0xffU, 0xffU, 0,
	    0xffU, 0xffU, &events)
	    || events.outcome != YT_RMT_SERIAL_EVENTS_LOCAL
	    || events.event_count != 0U)
		return false;
	if (!yt_rmt_serial_state_compose((const uint8_t *)"COM5", 4U,
	    NULL, 0U, 1U, 0U, &state)
	    || state.outcome != YT_RMT_SERIAL_LOCAL
	    || state.requested_port != 5)
		return false;
	if (!yt_rmt_serial_state_compose((const uint8_t *)"COM4:", 5U,
	    (const uint8_t *)"1200 BAUD,N,8,1", 15U, 0x60U, 0U, &state)
	    || state.outcome != YT_RMT_SERIAL_REMOTE
	    || state.requested_port != 4 || state.brun_device != 2
	    || state.uart_base != 0x02e8U
	    || state.modem_status_port != 0x02eeU
	    || state.bios_address != 0x0402U
	    || state.bios_value != 0x02e8U
	    || state.opening_framing.opening_baud != 1200U
	    || state.opening_framing.parity != YT_STARTUP_PARITY_NONE
	    || state.opening_framing.data_bits != 8U
	    || state.opening_framing.stop_bits != 1U
	    || state.detected_baud != 1200.0f
	    || state.restored_dll != 0x60U || state.restored_dlm != 0U
	    || state.open_spec_length != 29U
	    || memcmp(state.open_spec,
	    "COM2:1200,N,8,1,CS65535,DS,CD", 29U) != 0
	    || !yt_rmt_serial_events_compose(&state, 0x1bU, 0x05U, 0,
	    0x03U, 0x02U, &events)
	    || events.outcome != YT_RMT_SERIAL_EVENTS_REMOTE
	    || events.event_count != 22U
	    || !serial_event_is(&events, 0U, YT_RMT_SERIAL_EVENT_DEF_SEG,
	    0U, 0U, 0, true)
	    || !serial_event_is(&events, 1U, YT_RMT_SERIAL_EVENT_POKE,
	    0x0402U, 0xe8U, 0, true)
	    || !serial_event_is(&events, 2U, YT_RMT_SERIAL_EVENT_POKE,
	    0x0403U, 0x02U, 0, true)
	    || !serial_event_is(&events, 3U, YT_RMT_SERIAL_EVENT_IN,
	    0x02ebU, 0x1bU, 0, true)
	    || !serial_event_is(&events, 4U, YT_RMT_SERIAL_EVENT_OUT,
	    0x02ebU, 0x1bU, 0, true)
	    || !serial_event_is(&events, 5U, YT_RMT_SERIAL_EVENT_IN,
	    0x02e9U, 0x05U, 0, true)
	    || !serial_event_is(&events, 6U, YT_RMT_SERIAL_EVENT_OUT,
	    0x02e9U, 0U, 0, true)
	    || !serial_event_is(&events, 7U, YT_RMT_SERIAL_EVENT_OUT,
	    0x02ebU, 0x9bU, 0, true)
	    || !serial_event_is(&events, 8U, YT_RMT_SERIAL_EVENT_IN,
	    0x02e9U, 0U, 0, true)
	    || !serial_event_is(&events, 9U, YT_RMT_SERIAL_EVENT_IN,
	    0x02e8U, 0x60U, 0, true)
	    || !serial_event_is(&events, 10U, YT_RMT_SERIAL_EVENT_OUT,
	    0x02ebU, 0x1bU, 0, true)
	    || !serial_event_is(&events, 11U, YT_RMT_SERIAL_EVENT_OUT,
	    0x02e9U, 0x05U, 0, true)
	    || !serial_event_is(&events, 12U, YT_RMT_SERIAL_EVENT_OUT,
	    0x02ebU, 0x1bU, 0, true)
	    || !serial_event_is(&events, 13U, YT_RMT_SERIAL_EVENT_OPEN,
	    3U, 0U, 0, true)
	    || !serial_event_is(&events, 14U, YT_RMT_SERIAL_EVENT_IN,
	    0x02ebU, 0x03U, 0, true)
	    || !serial_event_is(&events, 15U, YT_RMT_SERIAL_EVENT_IN,
	    0x02e9U, 0x02U, 0, true)
	    || !serial_event_is(&events, 16U, YT_RMT_SERIAL_EVENT_OUT,
	    0x02e9U, 0U, 0, true)
	    || !serial_event_is(&events, 17U, YT_RMT_SERIAL_EVENT_OUT,
	    0x02ebU, 0x83U, 0, true)
	    || !serial_event_is(&events, 18U, YT_RMT_SERIAL_EVENT_OUT,
	    0x02e8U, 0x60U, 0, true)
	    || !serial_event_is(&events, 19U, YT_RMT_SERIAL_EVENT_OUT,
	    0x02e9U, 0U, 0, true)
	    || !serial_event_is(&events, 20U, YT_RMT_SERIAL_EVENT_OUT,
	    0x02ebU, 0x03U, 0, true)
	    || !serial_event_is(&events, 21U, YT_RMT_SERIAL_EVENT_OUT,
	    0x02e9U, 0x02U, 0, true))
		return false;
	if (!yt_rmt_serial_state_compose((const uint8_t *)"COM3", 4U,
	    (const uint8_t *)"7,N,O", 5U, 0U, 0x1fU, &state)
	    || state.outcome != YT_RMT_SERIAL_REMOTE
	    || state.opening_framing.opening_baud != 1200U
	    || state.opening_framing.parity != YT_STARTUP_PARITY_EVEN
	    || state.opening_framing.data_bits != 7U
	    || state.opening_framing.stop_bits != 1U
	    || state.open_spec_length != 29U
	    || memcmp(state.open_spec,
	    "COM1:1200,E,7,1,CS65535,DS,CD", 29U) != 0
	    || state.restored_dll != 0U || state.restored_dlm != 30U)
		return false;
	if (!yt_rmt_serial_events_compose(&state, 0x03U, 0U, 5,
	    0xffU, 0xffU, &events)
	    || events.outcome != YT_RMT_SERIAL_EVENTS_OPEN_ERROR
	    || events.event_count != 14U
	    || !serial_event_is(&events, 13U, YT_RMT_SERIAL_EVENT_OPEN,
	    3U, 0U, 5, false))
		return false;
	if (!yt_rmt_serial_state_compose((const uint8_t *)"COM1", 4U,
	    (const uint8_t *)"N,8,1", 5U, 0U, 0U, &state)
	    || state.outcome != YT_RMT_SERIAL_ZERO_DIVISOR
	    || !yt_rmt_serial_events_compose(&state, 0x83U, 7U, 0,
	    0U, 0U, &events)
	    || events.outcome != YT_RMT_SERIAL_EVENTS_ZERO_DIVISOR
	    || events.event_count != 9U
	    || !serial_event_is(&events, 8U,
	    YT_RMT_SERIAL_EVENT_RUNTIME_ERROR, 0U, 0U, 11, false))
		return false;
	return !yt_rmt_serial_state_compose(NULL, 1U, NULL, 0U, 1U, 0U,
	    &state)
	    && !yt_rmt_serial_events_compose(&state, 0U, 0U, -1,
	    0U, 0U, &events);
}

static bool
test_rmt_remote_identity(void)
{
	enum { LONG_UNRELATED_LENGTH = 300 };
	char long_unrelated[LONG_UNRELATED_LENGTH + 1U];
	struct yt_name_row rows[4] = {
		{long_unrelated, "Player", "Ignored", "Alias"},
		{"Jane", "Doe", "First", "Alias"},
		{"Other", "Player", "Wrong", "Person"},
		{"Jane", "Doe", "Last", "Winner"}
	};
	struct yt_name_file names = {rows, 4U};
	char credited[90];
	char short_credit[5];

	memset(long_unrelated, 'Q', LONG_UNRELATED_LENGTH);
	long_unrelated[LONG_UNRELATED_LENGTH] = '\0';
	return yt_rmt_credited_name("  jANE ", " DOE  ", &names, credited,
	    sizeof(credited)) && strcmp(credited, "Last Winner") == 0
	    && yt_rmt_credited_name("Nobody", "Here", &names, credited,
	    sizeof(credited)) && credited[0] == '\0'
	    && !yt_rmt_credited_name("Jane", "Doe", &names, short_credit,
	    sizeof(short_credit))
	    && !yt_rmt_credited_name(NULL, "Doe", &names, credited,
	    sizeof(credited))
	    && !yt_rmt_credited_name("Jane", "Doe", NULL, credited,
	    sizeof(credited));
}

static bool
test_rmt_old_preprocess(void)
{
	static const uint8_t positive_cloak[] = {0x00, 0x00, 0x00, 0x82};
	static const uint8_t negative_cloak[] = {0x00, 0x00, 0x80, 0x82};
	struct yt_database database;
	struct yt_config config;
	struct yt_record player_two;
	struct yt_record player_three;
	struct yt_record result;
	struct yt_error error;
	uint8_t *before = NULL;
	uint8_t *after = NULL;
	size_t before_length = 0U;
	size_t after_length = 0U;
	bool valid = false;

	memset(&database, 0, sizeof(database));
	memset(&config, 0, sizeof(config));
	yt_record_blank(&config.record);
	config.sector_offset = 3.0f;
	config.headquarters = 0.0f;
	yt_record_set_number(&config.record, YT_F53, config.sector_offset);
	yt_record_set_number(&config.record, YT_F117, config.headquarters);
	memset(player_two.bytes, 0x52, sizeof(player_two.bytes));
	memset(player_three.bytes, 0x73, sizeof(player_three.bytes));
	yt_record_set_number(&player_two, YT_F57, 5.0f);
	yt_record_set_number(&player_two, YT_F125, 2.0f);
	yt_record_set_number(&player_three, YT_F57, 7.0f);
	yt_record_set_number(&player_three, YT_F125, -3.0f);
	yt_error_clear(&error);
	if (!yt_database_open(&database, "RMTOLD.DAT", YT_OPEN_CREATE, &error)
	    || !yt_database_write(&database, 1, &config.record, &error)
	    || !yt_database_write(&database, 2, &player_two, &error)
	    || !yt_database_write(&database, 3, &player_three, &error)
	    || !yt_rmt_preprocess_old_database(&database, &config, &error)
	    || config.headquarters != 85.0f
	    || !yt_database_read(&database, 1, &result, &error)
	    || yt_record_get_number(&result, YT_F117) != 85.0f
	    || !yt_database_read(&database, 2, &result, &error)
	    || memcmp(result.bytes + YT_F125, negative_cloak,
	    sizeof(negative_cloak)) != 0
	    || memcmp(result.bytes, player_two.bytes, YT_F125) != 0
	    || memcmp(result.bytes + YT_F129, player_two.bytes + YT_F129,
	    YT_RECORD_SIZE - YT_F129) != 0
	    || !yt_database_read(&database, 3, &result, &error)
	    || memcmp(result.bytes, player_three.bytes, YT_RECORD_SIZE) != 0
	    || memcmp(player_two.bytes + YT_F125, positive_cloak,
	    sizeof(positive_cloak)) != 0)
		goto done;
	if (!read_file("RMTOLD.DAT", &before, &before_length))
		goto done;
	config.sector_offset = 1.0f;
	config.headquarters = 85.0f;
	if (!yt_rmt_preprocess_old_database(&database, &config, &error)
	    || !read_file("RMTOLD.DAT", &after, &after_length)
	    || before_length != after_length
	    || memcmp(before, after, before_length) != 0)
		goto done;
	valid = true;

done:
	free(before);
	free(after);
	yt_database_close(&database);
	(void)remove("RMTOLD.DAT");
	return valid;
}

static bool
test_rmt_config_normalization(void)
{
	struct yt_config preserved;
	struct yt_config defaults;

	memset(&preserved, 0, sizeof(preserved));
	strcpy(preserved.scoreboard, "SCORE.TXT");
	preserved.epoch_year = 99.0f;
	preserved.turns_per_day = 777.0f;
	preserved.sector_offset = 6.0f;
	preserved.port_offset = 26.0f;
	preserved.planet_offset = 31.0f;
	preserved.initial_fighters = 12.0f;
	preserved.initial_credits = 3456.0f;
	preserved.initial_holds = 17.0f;
	preserved.retention_days = 9.0f;
	preserved.last_maintenance = 999.0f;
	preserved.local_screen = 0.0f;
	preserved.total_records = 33.0f;
	preserved.lottery_plays = 1.0f;
	preserved.genesis_ports = 20.0f;
	preserved.headquarters = 85.0f;
	preserved.maximum_holds = 5.0f;
	preserved.marker = 123.0f;
	preserved.maximum_planets = 456.0f;
	defaults = preserved;
	yt_rmt_normalize_config(&preserved, false);
	if (strcmp(preserved.scoreboard, "SCORE.TXT") != 0
	    || preserved.epoch_year != 99.0f
	    || preserved.turns_per_day != 777.0f
	    || preserved.sector_offset != 6.0f
	    || preserved.port_offset != 26.0f
	    || preserved.planet_offset != 31.0f
	    || preserved.initial_fighters != 12.0f
	    || preserved.initial_credits != 3456.0f
	    || preserved.initial_holds != 17.0f
	    || preserved.retention_days != 9.0f
	    || preserved.last_maintenance != 999.0f
	    || preserved.local_screen != 0.0f
	    || preserved.total_records != 33.0f
	    || preserved.lottery_plays != 1.0f
	    || preserved.genesis_ports != 20.0f
	    || preserved.headquarters != 85.0f
	    || preserved.maximum_holds != 5.0f
	    || preserved.marker != 6324.0f
	    || preserved.maximum_planets != 0.0f)
		return false;
	defaults.scoreboard[0] = '\0';
	defaults.local_screen = 1.0f;
	defaults.lottery_plays = 0.0f;
	defaults.genesis_ports = 301.0f;
	defaults.maximum_holds = 1001.0f;
	yt_rmt_normalize_config(&defaults, false);
	if (strcmp(defaults.scoreboard, "NUL") != 0
	    || defaults.local_screen != -1.0f
	    || defaults.lottery_plays != 1.0f
	    || defaults.genesis_ports != 200.0f
	    || defaults.maximum_holds != 50.0f
	    || defaults.marker != 6324.0f
	    || defaults.maximum_planets != 0.0f)
		return false;
	defaults.local_screen = 0.0f;
	defaults.genesis_ports = 19.0f;
	defaults.maximum_holds = 4.0f;
	yt_rmt_normalize_config(&defaults, true);
	return defaults.local_screen == -1.0f
	    && defaults.genesis_ports == 200.0f
	    && defaults.maximum_holds == 50.0f;
}

struct alias_compact_test_tape {
	enum yt_alias_compact_step events[40];
	size_t event_count;
	size_t fail_at;
	size_t input_closes;
	size_t output_closes;
	size_t row_index;
	size_t write_index;
	uint8_t output[256];
	size_t output_length;
};

static bool
alias_compact_test_record(struct alias_compact_test_tape *tape,
    enum yt_alias_compact_step step, struct yt_error *error)
{
	if (tape->event_count >= YT_ARRAY_LEN(tape->events))
		return false;
	tape->events[tape->event_count++] = step;
	if (tape->fail_at != 0U && tape->event_count == tape->fail_at) {
		if (error != NULL)
			error->status = YT_IO_ERROR;
		return false;
	}
	return true;
}

static bool
alias_compact_test_close_input(void *context, struct yt_error *error)
{
	struct alias_compact_test_tape *tape = context;
	enum yt_alias_compact_step step = tape->input_closes++ == 0U
	    ? YT_ALIAS_COMPACT_CLOSE_INPUT_INITIAL
	    : YT_ALIAS_COMPACT_CLOSE_INPUT_FINAL;

	return alias_compact_test_record(tape, step, error);
}

static bool
alias_compact_test_close_output(void *context, struct yt_error *error)
{
	struct alias_compact_test_tape *tape = context;
	enum yt_alias_compact_step step = tape->output_closes++ == 0U
	    ? YT_ALIAS_COMPACT_CLOSE_OUTPUT_INITIAL
	    : YT_ALIAS_COMPACT_CLOSE_OUTPUT_FINAL;

	return alias_compact_test_record(tape, step, error);
}

static bool
alias_compact_test_output_mode(void *context, struct yt_error *error)
{
	return alias_compact_test_record(context,
	    YT_ALIAS_COMPACT_SET_OUTPUT_MODE, error);
}

static bool
alias_compact_test_input_mode(void *context, struct yt_error *error)
{
	return alias_compact_test_record(context,
	    YT_ALIAS_COMPACT_SET_INPUT_MODE, error);
}

static bool
alias_compact_test_open_output(void *context, const char *path,
    struct yt_error *error)
{
	return strcmp(path, "TEMPWORK") == 0
	    && alias_compact_test_record(context, YT_ALIAS_COMPACT_OPEN_OUTPUT,
	    error);
}

static bool
alias_compact_test_open_input(void *context, const char *path,
    struct yt_error *error)
{
	return strcmp(path, "YTNAME.DAT") == 0
	    && alias_compact_test_record(context, YT_ALIAS_COMPACT_OPEN_INPUT,
	    error);
}

static bool
alias_compact_test_eof(void *context, bool *eof, struct yt_error *error)
{
	struct alias_compact_test_tape *tape = context;

	if (!alias_compact_test_record(tape, YT_ALIAS_COMPACT_EOF, error))
		return false;
	*eof = tape->row_index == 4U;
	return true;
}

static char *
alias_compact_test_string(const char *value)
{
	size_t length = strlen(value);
	char *copy = malloc(length + 1U);

	if (copy != NULL)
		memcpy(copy, value, length + 1U);
	return copy;
}

static bool
alias_compact_test_read_row(void *context, struct yt_name_row *row,
    size_t *staged_count, struct yt_error *error)
{
	static const char *const rows[4][4] = {
		{"Real", "One", "Same", "Alias"},
		{"Other", "Person", "Other", "Person"},
		{"Real", "Two", "Same", "Alias"},
		{"Real", "Three", "same", "Alias"},
	};
	struct alias_compact_test_tape *tape = context;
	char **fields[4] = {&row->real_first, &row->real_last,
	    &row->alias_first, &row->alias_last};
	size_t field;

	memset(row, 0, sizeof(*row));
	*staged_count = 0U;
	if (!alias_compact_test_record(tape, YT_ALIAS_COMPACT_READ_ROW,
	    error))
		return false;
	if (tape->row_index >= YT_ARRAY_LEN(rows))
		return false;
	for (field = 0U; field < 4U; ++field) {
		*fields[field] = alias_compact_test_string(
		    rows[tape->row_index][field]);
		if (*fields[field] == NULL)
			return false;
		*staged_count = field + 1U;
	}
	++tape->row_index;
	return true;
}

static bool
alias_compact_test_select_output(void *context, struct yt_error *error)
{
	return alias_compact_test_record(context, YT_ALIAS_COMPACT_SELECT_OUTPUT,
	    error);
}

static bool
alias_compact_test_write(void *context, const uint8_t *data, size_t length,
    bool newline, struct yt_error *error)
{
	static const enum yt_alias_compact_step steps[7] = {
		YT_ALIAS_COMPACT_WRITE_REAL_FIRST,
		YT_ALIAS_COMPACT_WRITE_COMMA_1,
		YT_ALIAS_COMPACT_WRITE_REAL_LAST,
		YT_ALIAS_COMPACT_WRITE_COMMA_2,
		YT_ALIAS_COMPACT_WRITE_ALIAS_FIRST,
		YT_ALIAS_COMPACT_WRITE_COMMA_3,
		YT_ALIAS_COMPACT_WRITE_ALIAS_LAST_LINE,
	};
	struct alias_compact_test_tape *tape = context;
	size_t part = tape->write_index++ % YT_ARRAY_LEN(steps);

	if (newline != (part == YT_ARRAY_LEN(steps) - 1U)
	    || !alias_compact_test_record(tape, steps[part], error))
		return false;
	if (length > sizeof(tape->output) - tape->output_length
	    || (newline && sizeof(tape->output) - tape->output_length - length
	    < 2U))
		return false;
	if (length != 0U)
		memcpy(tape->output + tape->output_length, data, length);
	tape->output_length += length;
	if (newline) {
		tape->output[tape->output_length++] = '\r';
		tape->output[tape->output_length++] = '\n';
	}
	return true;
}

static bool
alias_compact_test_kill(void *context, const char *path,
    struct yt_error *error)
{
	return strcmp(path, "YTNAME.DAT") == 0
	    && alias_compact_test_record(context, YT_ALIAS_COMPACT_KILL_SOURCE,
	    error);
}

static bool
alias_compact_test_rename(void *context, const char *old_path,
    const char *new_path, struct yt_error *error)
{
	return strcmp(old_path, "TEMPWORK") == 0
	    && strcmp(new_path, "YTNAME.DAT") == 0
	    && alias_compact_test_record(context, YT_ALIAS_COMPACT_RENAME_TEMP,
	    error);
}

static bool
test_maintenance_alias_compaction_transaction(void)
{
	static const struct yt_alias_compact_ops ops = {
		alias_compact_test_close_input,
		alias_compact_test_close_output,
		alias_compact_test_output_mode,
		alias_compact_test_open_output,
		alias_compact_test_input_mode,
		alias_compact_test_open_input,
		alias_compact_test_eof,
		alias_compact_test_read_row,
		alias_compact_test_select_output,
		alias_compact_test_write,
		alias_compact_test_kill,
		alias_compact_test_rename,
	};
	static const uint8_t expected_output[] =
	    "Other,Person,Other,Person\r\n"
	    "Real,Three,same,Alias\r\n";
	struct alias_compact_test_tape success = {0};
	struct yt_alias_compact_state state;
	struct yt_error error;
	size_t failure;
	bool result;

	yt_error_clear(&error);
	result = yt_maintenance_alias_compact_run(&state, "Same", "Alias",
	    &ops, &success, &error);
	if (!result || !state.complete || state.attempted != YT_ALIAS_COMPACT_NONE
	    || state.completed_steps != 35U || state.eof_checks != 5U
	    || state.rows_read != 4U || state.rows_removed != 2U
	    || state.rows_written != 2U || state.write_values_completed != 14U
	    || state.staged_count != 0U || success.event_count != 35U
	    || success.output_length != sizeof(expected_output) - 1U
	    || memcmp(success.output, expected_output,
	    sizeof(expected_output) - 1U) != 0) {
		yt_maintenance_alias_compact_state_free(&state);
		return false;
	}
	for (failure = 1U; failure <= success.event_count; ++failure) {
		struct alias_compact_test_tape tape = {.fail_at = failure};

		yt_error_clear(&error);
		result = yt_maintenance_alias_compact_run(&state, "Same", "Alias",
		    &ops, &tape, &error);
		if (result || error.status != YT_IO_ERROR || state.complete
		    || state.attempted != success.events[failure - 1U]
		    || state.completed_steps != failure - 1U
		    || tape.event_count != failure
		    || tape.output_length > success.output_length
		    || memcmp(tape.output, success.output,
		    tape.output_length) != 0) {
			yt_maintenance_alias_compact_state_free(&state);
			return false;
		}
		yt_maintenance_alias_compact_state_free(&state);
	}
	return !yt_maintenance_alias_compact_run(NULL, "Same", "Alias", &ops,
	    &success, &error)
	    && !yt_maintenance_alias_compact_run(&state, NULL, "Alias", &ops,
	    &success, &error)
	    && !yt_maintenance_alias_compact_run(&state, "Same", NULL, &ops,
	    &success, &error)
	    && !yt_maintenance_alias_compact_run(&state, "Same", "Alias", NULL,
	    &success, &error);
}

static bool
test_maintenance_alias_compaction(void)
{
	static const uint8_t input[] =
	    "Real,One,Same,Alias\r\n"
	    "Same,Alias,Other,Person\r\n"
	    "Real,Two,Same,Alias\r\n"
	    "Real,Three,same,Alias\r\n";
	static const uint8_t expected[] =
	    "Same,Alias,Other,Person\r\n"
	    "Real,Three,same,Alias\r\n"
	    "\x1a";
	static const uint8_t stale_temp[] =
	    "THIS OLD TEMPWORK TAIL MUST NOT SURVIVE THE FINAL CLOSE\r\n\x1a";
	struct yt_text_file result = {0};
	struct yt_error error;
	FILE *temporary;
	bool valid = false;

	yt_error_clear(&error);
	if (!yt_text_write("YTNAME.DAT", input, sizeof(input) - 1U, true,
	    &error)
	    || !write_file("TEMPWORK", stale_temp, sizeof(stale_temp) - 1U)
	    || !yt_maintenance_remove_alias("Same Alias", &error)
	    || !yt_text_read("YTNAME.DAT", &result, &error))
		goto done;
	temporary = fopen("TEMPWORK", "rb");
	if (temporary != NULL) {
		(void)fclose(temporary);
		goto done;
	}
	valid = result.length == sizeof(expected) - 1U
	    && memcmp(result.data, expected, sizeof(expected) - 1U) == 0;

done:
	yt_text_free(&result);
	(void)remove("YTNAME.DAT");
	(void)remove("TEMPWORK");
	return valid;
}

enum news_rotation_test_event {
	NEWS_ROTATION_TEST_OPEN,
	NEWS_ROTATION_TEST_CLOSE,
	NEWS_ROTATION_TEST_KILL,
	NEWS_ROTATION_TEST_RENAME,
};

struct news_rotation_test_tape {
	enum news_rotation_test_event events[6];
	char first_path[6][32];
	char second_path[6][32];
	size_t calls;
	size_t fail_at;
};

static bool
news_rotation_test_step(struct news_rotation_test_tape *tape,
    enum news_rotation_test_event event, const char *first_path,
    const char *second_path, struct yt_error *error)
{
	size_t call = tape->calls++;

	if (call >= sizeof(tape->events) / sizeof(tape->events[0]))
		return false;
	tape->events[call] = event;
	(void)snprintf(tape->first_path[call], sizeof(tape->first_path[call]),
	    "%s", first_path != NULL ? first_path : "");
	(void)snprintf(tape->second_path[call], sizeof(tape->second_path[call]),
	    "%s", second_path != NULL ? second_path : "");
	if (call != tape->fail_at)
		return true;
	if (error != NULL) {
		error->status = YT_IO_ERROR;
		(void)snprintf(error->operation, sizeof(error->operation),
		    "injected news rotation step");
	}
	return false;
}

static bool
news_rotation_test_open(void *context, const char *path,
    struct yt_error *error)
{
	return news_rotation_test_step(context, NEWS_ROTATION_TEST_OPEN, path,
	    NULL, error);
}

static bool
news_rotation_test_close(void *context, struct yt_error *error)
{
	return news_rotation_test_step(context, NEWS_ROTATION_TEST_CLOSE, NULL,
	    NULL, error);
}

static bool
news_rotation_test_kill(void *context, const char *path,
    struct yt_error *error)
{
	return news_rotation_test_step(context, NEWS_ROTATION_TEST_KILL, path,
	    NULL, error);
}

static bool
news_rotation_test_rename(void *context, const char *old_path,
    const char *new_path, struct yt_error *error)
{
	return news_rotation_test_step(context, NEWS_ROTATION_TEST_RENAME,
	    old_path, new_path, error);
}

static bool
test_news_rotation_transaction(void)
{
	static const struct yt_news_rotate_ops ops = {
		news_rotation_test_open,
		news_rotation_test_close,
		news_rotation_test_kill,
		news_rotation_test_rename,
	};
	static const enum news_rotation_test_event expected_events[] = {
		NEWS_ROTATION_TEST_OPEN,
		NEWS_ROTATION_TEST_CLOSE,
		NEWS_ROTATION_TEST_OPEN,
		NEWS_ROTATION_TEST_CLOSE,
		NEWS_ROTATION_TEST_KILL,
		NEWS_ROTATION_TEST_RENAME,
	};
	static const enum yt_news_rotate_step expected_steps[] = {
		YT_NEWS_ROTATE_OPEN_CURRENT,
		YT_NEWS_ROTATE_CLOSE_CURRENT,
		YT_NEWS_ROTATE_OPEN_YESTERDAY,
		YT_NEWS_ROTATE_CLOSE_YESTERDAY,
		YT_NEWS_ROTATE_KILL_YESTERDAY,
		YT_NEWS_ROTATE_RENAME_CURRENT,
	};
	struct news_rotation_test_tape tape;
	struct yt_news_rotate_state state;
	struct yt_news_rotate_ops incomplete = ops;
	struct yt_error error;
	size_t index;

	memset(&tape, 0, sizeof(tape));
	tape.fail_at = SIZE_MAX;
	memset(&state, 0xa5, sizeof(state));
	if (!yt_news_rotate_run(&state, &ops, &tape, NULL)
	    || !state.complete || state.completed_steps != 6U
	    || state.attempted != YT_NEWS_ROTATE_RENAME_CURRENT
	    || tape.calls != 6U
	    || memcmp(tape.events, expected_events, sizeof(expected_events)) != 0
	    || strcmp(tape.first_path[0], "YTNEWS.DAT") != 0
	    || tape.first_path[1][0] != '\0'
	    || strcmp(tape.first_path[2], "YTYNEWS.DAT") != 0
	    || tape.first_path[3][0] != '\0'
	    || strcmp(tape.first_path[4], "YTYNEWS.DAT") != 0
	    || strcmp(tape.first_path[5], "YTNEWS.DAT") != 0
	    || strcmp(tape.second_path[5], "YTYNEWS.DAT") != 0)
		return false;
	for (index = 0U; index < 6U; ++index) {
		memset(&tape, 0, sizeof(tape));
		tape.fail_at = index;
		memset(&state, 0xa5, sizeof(state));
		yt_error_clear(&error);
		if (yt_news_rotate_run(&state, &ops, &tape, &error)
		    || state.complete || state.completed_steps != index
		    || state.attempted != expected_steps[index]
		    || tape.calls != index + 1U
		    || error.status != YT_IO_ERROR)
			return false;
	}
	incomplete.rename = NULL;
	yt_error_clear(&error);
	return !yt_news_rotate_run(&state, &incomplete, &tape, &error)
	    && error.status == YT_INVALID
	    && strcmp(error.operation, "news rotation transaction") == 0
	    && !yt_news_rotate_run(NULL, &ops, &tape, NULL)
	    && !yt_news_rotate_run(&state, NULL, &tape, NULL);
}

enum radio_compaction_test_event {
	RADIO_COMPACTION_TEST_OUTPUT_OPEN,
	RADIO_COMPACTION_TEST_OUTPUT_CLOSE,
	RADIO_COMPACTION_TEST_KILL,
	RADIO_COMPACTION_TEST_RANDOM_OPEN,
	RADIO_COMPACTION_TEST_SIZE,
	RADIO_COMPACTION_TEST_GET,
	RADIO_COMPACTION_TEST_PUT,
	RADIO_COMPACTION_TEST_RANDOM_CLOSE,
	RADIO_COMPACTION_TEST_RENAME,
};

#define RADIO_COMPACTION_TEST_EVENTS 16U

struct radio_compaction_test_tape {
	enum radio_compaction_test_event events[RADIO_COMPACTION_TEST_EVENTS];
	int files[RADIO_COMPACTION_TEST_EVENTS];
	char first_path[RADIO_COMPACTION_TEST_EVENTS][32];
	char second_path[RADIO_COMPACTION_TEST_EVENTS][32];
	size_t widths[RADIO_COMPACTION_TEST_EVENTS];
	uint32_t records[RADIO_COMPACTION_TEST_EVENTS];
	struct yt_radio_record input[4];
	size_t input_accepted[4];
	struct yt_radio_record output[2];
	size_t output_count;
	uint64_t source_length;
	size_t calls;
	size_t fail_at;
};

static bool
radio_compaction_test_step(struct radio_compaction_test_tape *tape,
    enum radio_compaction_test_event event, int file, const char *first_path,
    const char *second_path, size_t width, uint32_t record,
    struct yt_error *error)
{
	size_t call = tape->calls++;

	if (call >= RADIO_COMPACTION_TEST_EVENTS)
		return false;
	tape->events[call] = event;
	tape->files[call] = file;
	tape->widths[call] = width;
	tape->records[call] = record;
	(void)snprintf(tape->first_path[call], sizeof(tape->first_path[call]),
	    "%s", first_path != NULL ? first_path : "");
	(void)snprintf(tape->second_path[call], sizeof(tape->second_path[call]),
	    "%s", second_path != NULL ? second_path : "");
	if (call != tape->fail_at)
		return true;
	if (error != NULL) {
		error->status = YT_IO_ERROR;
		(void)snprintf(error->operation, sizeof(error->operation),
		    "injected radio compaction step");
	}
	return false;
}

static bool
radio_compaction_test_output_open(void *context, const char *path,
    struct yt_error *error)
{
	return radio_compaction_test_step(context,
	    RADIO_COMPACTION_TEST_OUTPUT_OPEN, -1, path, NULL, 0U, 0U, error);
}

static bool
radio_compaction_test_output_close(void *context, struct yt_error *error)
{
	return radio_compaction_test_step(context,
	    RADIO_COMPACTION_TEST_OUTPUT_CLOSE, -1, NULL, NULL, 0U, 0U,
	    error);
}

static bool
radio_compaction_test_kill(void *context, const char *path,
    struct yt_error *error)
{
	return radio_compaction_test_step(context, RADIO_COMPACTION_TEST_KILL,
	    -1, path, NULL, 0U, 0U, error);
}

static bool
radio_compaction_test_random_open(void *context,
    enum yt_radio_compact_file file, const char *path, size_t text_width,
    struct yt_error *error)
{
	return radio_compaction_test_step(context,
	    RADIO_COMPACTION_TEST_RANDOM_OPEN, (int)file, path, NULL,
	    text_width, 0U, error);
}

static bool
radio_compaction_test_size(void *context, enum yt_radio_compact_file file,
    uint64_t *length, struct yt_error *error)
{
	struct radio_compaction_test_tape *tape = context;

	if (!radio_compaction_test_step(tape, RADIO_COMPACTION_TEST_SIZE,
	    (int)file, NULL, NULL, 0U, 0U, error))
		return false;
	*length = tape->source_length;
	return true;
}

static bool
radio_compaction_test_get(void *context, enum yt_radio_compact_file file,
    uint32_t basic_record, struct yt_radio_record *record, size_t *accepted,
    struct yt_error *error)
{
	struct radio_compaction_test_tape *tape = context;
	size_t index = basic_record - 1U;

	if (index >= sizeof(tape->input) / sizeof(tape->input[0]))
		return false;
	*record = tape->input[index];
	*accepted = tape->input_accepted[index];
	if (tape->calls == tape->fail_at)
		*accepted = 17U;
	return radio_compaction_test_step(tape, RADIO_COMPACTION_TEST_GET,
	    (int)file, NULL, NULL, 0U, basic_record, error);
}

static bool
radio_compaction_test_put(void *context, enum yt_radio_compact_file file,
    uint32_t basic_record, const struct yt_radio_record *record,
    struct yt_error *error)
{
	struct radio_compaction_test_tape *tape = context;

	if (tape->output_count >= sizeof(tape->output) / sizeof(tape->output[0]))
		return false;
	tape->output[tape->output_count++] = *record;
	return radio_compaction_test_step(tape, RADIO_COMPACTION_TEST_PUT,
	    (int)file, NULL, NULL, 0U, basic_record, error);
}

static bool
radio_compaction_test_random_close(void *context,
    enum yt_radio_compact_file file, struct yt_error *error)
{
	return radio_compaction_test_step(context,
	    RADIO_COMPACTION_TEST_RANDOM_CLOSE, (int)file, NULL, NULL, 0U, 0U,
	    error);
}

static bool
radio_compaction_test_rename(void *context, const char *old_path,
    const char *new_path, struct yt_error *error)
{
	return radio_compaction_test_step(context, RADIO_COMPACTION_TEST_RENAME,
	    -1, old_path, new_path, 0U, 0U, error);
}

static bool
radio_compaction_test_prepare(struct radio_compaction_test_tape *tape)
{
	static const uint8_t raw_zero[4] = {0x44, 0x33, 0x22, 0x00};

	memset(tape, 0, sizeof(*tape));
	tape->fail_at = SIZE_MAX;
	tape->source_length = YT_RADIO_RECORD_SIZE * 4U + 7U;
	memset(tape->input[0].bytes, 0x31, YT_RADIO_RECORD_SIZE);
	memcpy(tape->input[0].bytes, raw_zero, sizeof(raw_zero));
	memset(tape->input[1].bytes, 0xa5, YT_RADIO_RECORD_SIZE);
	memset(tape->input[2].bytes, 0x5a, YT_RADIO_RECORD_SIZE);
	memset(tape->input[3].bytes, 0xcc, YT_RADIO_RECORD_SIZE);
	tape->input_accepted[0] = YT_RADIO_RECORD_SIZE;
	tape->input_accepted[1] = YT_RADIO_RECORD_SIZE;
	tape->input_accepted[2] = YT_RADIO_RECORD_SIZE;
	tape->input_accepted[3] = 7U;
	return yt_radio_set_number(&tape->input[1], 0, 1.5f)
	    && yt_radio_set_number(&tape->input[2], 0, -0.5f);
}

static bool
test_radio_compaction_transaction(void)
{
	static const struct yt_radio_compact_ops ops = {
		radio_compaction_test_output_open,
		radio_compaction_test_output_close,
		radio_compaction_test_kill,
		radio_compaction_test_random_open,
		radio_compaction_test_size,
		radio_compaction_test_get,
		radio_compaction_test_put,
		radio_compaction_test_random_close,
		radio_compaction_test_rename,
	};
	static const enum radio_compaction_test_event expected_events[] = {
		RADIO_COMPACTION_TEST_OUTPUT_OPEN,
		RADIO_COMPACTION_TEST_OUTPUT_CLOSE,
		RADIO_COMPACTION_TEST_KILL,
		RADIO_COMPACTION_TEST_RANDOM_OPEN,
		RADIO_COMPACTION_TEST_RANDOM_OPEN,
		RADIO_COMPACTION_TEST_SIZE,
		RADIO_COMPACTION_TEST_GET,
		RADIO_COMPACTION_TEST_GET,
		RADIO_COMPACTION_TEST_PUT,
		RADIO_COMPACTION_TEST_GET,
		RADIO_COMPACTION_TEST_PUT,
		RADIO_COMPACTION_TEST_GET,
		RADIO_COMPACTION_TEST_RANDOM_CLOSE,
		RADIO_COMPACTION_TEST_RANDOM_CLOSE,
		RADIO_COMPACTION_TEST_KILL,
		RADIO_COMPACTION_TEST_RENAME,
	};
	static const enum yt_radio_compact_step expected_steps[] = {
		YT_RADIO_COMPACT_OPEN_TEMP_OUTPUT,
		YT_RADIO_COMPACT_CLOSE_TEMP_OUTPUT,
		YT_RADIO_COMPACT_KILL_TEMP,
		YT_RADIO_COMPACT_OPEN_TEMP_RANDOM,
		YT_RADIO_COMPACT_OPEN_SOURCE_RANDOM,
		YT_RADIO_COMPACT_SOURCE_LOF,
		YT_RADIO_COMPACT_SOURCE_GET,
		YT_RADIO_COMPACT_SOURCE_GET,
		YT_RADIO_COMPACT_DESTINATION_PUT,
		YT_RADIO_COMPACT_SOURCE_GET,
		YT_RADIO_COMPACT_DESTINATION_PUT,
		YT_RADIO_COMPACT_SOURCE_GET,
		YT_RADIO_COMPACT_CLOSE_SOURCE,
		YT_RADIO_COMPACT_CLOSE_DESTINATION,
		YT_RADIO_COMPACT_KILL_SOURCE,
		YT_RADIO_COMPACT_RENAME_TEMP,
	};
	struct radio_compaction_test_tape tape;
	struct yt_radio_compact_state state;
	struct yt_radio_compact_ops incomplete = ops;
	struct yt_error error;
	size_t index;

	if (!radio_compaction_test_prepare(&tape)
	    || !yt_radio_compact_run(&state, &ops, &tape, NULL)
	    || !state.complete
	    || state.completed_steps != RADIO_COMPACTION_TEST_EVENTS
	    || state.source_length != YT_RADIO_RECORD_SIZE * 4U + 7U
	    || state.record_count != 4U || state.source_record != 4U
	    || state.retained_record != 2U || state.accepted != 7U
	    || state.completed_gets != 4U || state.completed_puts != 2U
	    || tape.calls != RADIO_COMPACTION_TEST_EVENTS
	    || tape.output_count != 2U
	    || memcmp(tape.events, expected_events, sizeof(expected_events)) != 0
	    || strcmp(tape.first_path[0], "TEMP") != 0
	    || strcmp(tape.first_path[2], "TEMP") != 0
	    || strcmp(tape.first_path[3], "TEMP") != 0
	    || strcmp(tape.first_path[4], "YTRMSG.DAT") != 0
	    || tape.files[3] != YT_RADIO_COMPACT_DESTINATION
	    || tape.files[4] != YT_RADIO_COMPACT_SOURCE
	    || tape.widths[3] != 72U || tape.widths[4] != 72U
	    || tape.records[6] != 1U || tape.records[7] != 2U
	    || tape.records[8] != 1U || tape.records[9] != 3U
	    || tape.records[10] != 2U || tape.records[11] != 4U
	    || tape.files[12] != YT_RADIO_COMPACT_SOURCE
	    || tape.files[13] != YT_RADIO_COMPACT_DESTINATION
	    || strcmp(tape.first_path[14], "YTRMSG.DAT") != 0
	    || strcmp(tape.first_path[15], "TEMP") != 0
	    || strcmp(tape.second_path[15], "YTRMSG.DAT") != 0
	    || memcmp(tape.output[0].bytes, tape.input[1].bytes, 84U) != 0
	    || tape.output[0].bytes[84] != 0U
	    || tape.output[0].bytes[85] != 0U
	    || memcmp(tape.output[1].bytes, tape.input[2].bytes, 84U) != 0
	    || tape.output[1].bytes[84] != 0U
	    || tape.output[1].bytes[85] != 0U)
		return false;
	for (index = 0U; index < RADIO_COMPACTION_TEST_EVENTS; ++index) {
		if (!radio_compaction_test_prepare(&tape))
			return false;
		tape.fail_at = index;
		memset(&state, 0xa5, sizeof(state));
		yt_error_clear(&error);
		if (yt_radio_compact_run(&state, &ops, &tape, &error)
		    || state.complete || state.completed_steps != index
		    || state.attempted != expected_steps[index]
		    || tape.calls != index + 1U || error.status != YT_IO_ERROR)
			return false;
		if (index == 6U && (state.source_record != 1U
		    || state.retained_record != 0U || state.accepted != 17U
		    || state.completed_gets != 0U || state.completed_puts != 0U))
			return false;
		if (index == 8U && (state.source_record != 2U
		    || state.retained_record != 1U || state.accepted != 86U
		    || state.completed_gets != 2U || state.completed_puts != 0U))
			return false;
		if (index == 10U && (state.source_record != 3U
		    || state.retained_record != 2U || state.completed_gets != 3U
		    || state.completed_puts != 1U))
			return false;
		if (index == 12U && (state.source_record != 4U
		    || state.accepted != 7U || state.completed_gets != 4U
		    || state.completed_puts != 2U))
			return false;
	}
	incomplete.rename = NULL;
	yt_error_clear(&error);
	return !yt_radio_compact_run(&state, &incomplete, &tape, &error)
	    && error.status == YT_INVALID
	    && strcmp(error.operation, "radio compaction transaction") == 0
	    && !yt_radio_compact_run(NULL, &ops, &tape, NULL)
	    && !yt_radio_compact_run(&state, NULL, &tape, NULL);
}

static bool
test_maintenance_message_compaction(void)
{
	static const uint8_t raw_zero[4] = {0x44, 0x33, 0x22, 0x00};
	static const uint8_t partial[] = {
		0xde, 0xad, 0xbe, 0xef, 0x11, 0x22, 0x33
	};
	static const uint8_t stale_temp[YT_RADIO_RECORD_SIZE * 4U] = {0x7f};
	static const uint8_t current_news[] = "current news\r\n\x1a";
	static const uint8_t old_news[] = "old news\r\n\x1a";
	static const uint8_t stale_news[] = "retained news\r\n\x1astale tail";
	static const uint8_t normalized_news[] = "retained news\r\n\x1a";
	struct yt_radio_record zero;
	struct yt_radio_record first;
	struct yt_radio_record second;
	uint8_t input[sizeof(zero) + sizeof(first) + sizeof(second)
	    + sizeof(partial)];
	uint8_t expected[sizeof(first) + sizeof(second)];
	uint8_t *data = NULL;
	size_t length = 0;
	struct yt_error error;
	FILE *file;
	bool valid = false;

	memset(zero.bytes, 0x31, sizeof(zero.bytes));
	memset(first.bytes, 0xa5, sizeof(first.bytes));
	memset(second.bytes, 0x5a, sizeof(second.bytes));
	if (!yt_radio_set_raw_number(&zero, 0, raw_zero)
	    || !yt_radio_set_number(&first, 0, 1.5f)
	    || !yt_radio_set_number(&second, 0, -0.5f))
		return false;
	first.bytes[84] = 0xe1;
	first.bytes[85] = 0xe2;
	second.bytes[84] = 0xf1;
	second.bytes[85] = 0xf2;
	memcpy(input, zero.bytes, sizeof(zero.bytes));
	memcpy(input + sizeof(zero), first.bytes, sizeof(first.bytes));
	memcpy(input + sizeof(zero) + sizeof(first), second.bytes,
	    sizeof(second.bytes));
	memcpy(input + sizeof(zero) + sizeof(first) + sizeof(second), partial,
	    sizeof(partial));
	memcpy(expected, first.bytes, 84U);
	memset(expected + 84U, 0, 2U);
	memcpy(expected + sizeof(first), second.bytes, 84U);
	memset(expected + sizeof(first) + 84U, 0, 2U);
	yt_error_clear(&error);
	if (!write_file("YTRMSG.DAT", input, sizeof(input))
	    || !write_file("TEMP", stale_temp, sizeof(stale_temp))
	    || !yt_radio_compact(&error)
	    || !read_file("YTRMSG.DAT", &data, &length)
	    || length != sizeof(expected)
	    || memcmp(data, expected, sizeof(expected)) != 0)
		goto done;
	free(data);
	data = NULL;
	file = fopen("TEMP", "rb");
	if (file != NULL) {
		(void)fclose(file);
		goto done;
	}
	if (!write_file("YTNEWS.DAT", current_news, sizeof(current_news) - 1U)
	    || !write_file("YTYNEWS.DAT", old_news, sizeof(old_news) - 1U)
	    || !yt_news_rotate(&error)
	    || !read_file("YTYNEWS.DAT", &data, &length)
	    || length != sizeof(current_news) - 1U
	    || memcmp(data, current_news, sizeof(current_news) - 1U) != 0)
		goto done;
	file = fopen("YTNEWS.DAT", "rb");
	if (file != NULL) {
		(void)fclose(file);
		goto done;
	}
	free(data);
	data = NULL;
	(void)remove("YTYNEWS.DAT");
	if (!yt_news_rotate(&error)
	    || !read_file("YTYNEWS.DAT", &data, &length)
	    || length != 1U || data[0] != 0x1aU)
		goto done;
	free(data);
	data = NULL;
	(void)remove("YTYNEWS.DAT");
	if (!write_file("YTNEWS.DAT", stale_news, sizeof(stale_news) - 1U)
	    || !yt_news_rotate(&error)
	    || !read_file("YTYNEWS.DAT", &data, &length)
	    || length != sizeof(normalized_news) - 1U
	    || memcmp(data, normalized_news,
	    sizeof(normalized_news) - 1U) != 0)
		goto done;
	valid = true;

done:
	free(data);
	(void)remove("YTRMSG.DAT");
	(void)remove("TEMP");
	(void)remove("YTNEWS.DAT");
	(void)remove("YTYNEWS.DAT");
	return valid;
}

static bool
test_maintenance_random_helpers(void)
{
	static const uint8_t draws[] = {
		0x00, 0x00, 0x00,
		0x00, 0x00, 0x80,
		0xff, 0xff, 0xff,
		0x00, 0x00, 0x80,
		0x00, 0x00, 0x00
	};
	struct utility_random_script script = {draws, sizeof(draws), 0};
	struct yt_random random;
	struct yt_error error;
	int value;

	yt_error_clear(&error);
	yt_random_init(&random);
	yt_random_set_provider(&random, utility_random_fill, &script);
	if (!yt_maintenance_random_integer(&random, 10, &value, &error)
	    || value != 1
	    || !yt_maintenance_random_integer(&random, 10, &value, &error)
	    || value != 6
	    || !yt_maintenance_random_integer(&random, 1000, &value, &error)
	    || value != 1000
	    || !yt_maintenance_nested_integer(&random, 2, 100, &value, &error)
	    || value != 1 || random.draws != 5U
	    || script.position != sizeof(draws))
		return false;
	yt_error_clear(&error);
	if (yt_maintenance_random_integer(&random, 0, &value, &error)
	    || error.status != YT_RANGE || random.draws != 5U)
		return false;
	yt_error_clear(&error);
	value = 37;
	if (!yt_maintenance_nested_integer(&random, 0, 100, &value, &error)
	    || error.status != YT_OK || value != 37 || random.draws != 5U)
		return false;
	yt_error_clear(&error);
	if (!yt_maintenance_nested_integer(&random, 2, 0, &value, &error)
	    || error.status != YT_OK || value != 37 || random.draws != 5U)
		return false;
	script.position = 0;
	script.length = 3;
	yt_random_set_provider(&random, utility_random_fill, &script);
	value = 37;
	yt_error_clear(&error);
	return !yt_maintenance_nested_integer(&random, 2, 100, &value, &error)
	    && error.status == YT_RANDOM_ERROR && value == 1
	    && random.draws == 1U && script.position == 3U;
}

static bool
test_maintenance_xannor_defense(void)
{
	static const uint8_t zero_draw[3] = {0, 0, 0};
	static const uint8_t high_draw[3] = {0xff, 0xff, 0xff};
	struct utility_random_script script = {zero_draw, 0, 0};
	struct yt_random random;
	struct yt_error error;
	float group;
	float fighters;
	float owner;

	yt_error_clear(&error);
	yt_random_init(&random);
	yt_random_set_provider(&random, utility_random_fill, &script);
	group = 10.0f;
	fighters = 2.0f;
	owner = 0.0f;
	if (!yt_maintenance_xannor_defense(&random, &group, &fighters,
	    &owner, &error) || group != 10.0f || fighters != 2.0f
	    || owner != 0.0f || random.draws != 0U || script.position != 0U)
		return false;
	owner = -1.0f;
	if (!yt_maintenance_xannor_defense(&random, &group, &fighters,
	    &owner, &error) || group != 10.0f || fighters != 2.0f
	    || owner != -1.0f || random.draws != 0U)
		return false;
	owner = 7.0f;
	fighters = 0.0f;
	if (!yt_maintenance_xannor_defense(&random, &group, &fighters,
	    &owner, &error) || group != 10.0f || fighters != 0.0f
	    || owner != 7.0f || random.draws != 0U)
		return false;
	group = 0.0f;
	fighters = 2.0f;
	if (!yt_maintenance_xannor_defense(&random, &group, &fighters,
	    &owner, &error) || group != 0.0f || fighters != 2.0f
	    || owner != 7.0f || random.draws != 0U)
		return false;

	group = 1.0f;
	fighters = 1.0f;
	owner = 7.0f;
	yt_error_clear(&error);
	if (yt_maintenance_xannor_defense(&random, &group, &fighters,
	    &owner, &error) || error.status != YT_RANDOM_ERROR
	    || group != 1.0f || fighters != 1.0f || owner != 7.0f
	    || random.draws != 0U)
		return false;

	script = (struct utility_random_script){zero_draw,
	    sizeof(zero_draw), 0};
	yt_random_set_provider(&random, utility_random_fill, &script);
	yt_error_clear(&error);
	if (!yt_maintenance_xannor_defense(&random, &group, &fighters,
	    &owner, &error) || group != 1.0f || fighters != 0.0f
	    || owner != 0.0f || random.draws != 1U
	    || script.position != sizeof(zero_draw))
		return false;

	script = (struct utility_random_script){high_draw,
	    sizeof(high_draw), 0};
	yt_random_set_provider(&random, utility_random_fill, &script);
	group = 1.0f;
	fighters = 1.0f;
	owner = 7.0f;
	return yt_maintenance_xannor_defense(&random, &group, &fighters,
	    &owner, &error) && group == 0.0f && fighters == 1.0f
	    && owner == 7.0f && random.draws == 1U
	    && script.position == sizeof(high_draw);
}

static bool
test_maintenance_xannor_player(void)
{
	static const uint8_t kill_draws[] = {
		0x00, 0x00, 0x80,
		0x00, 0x00, 0x80
	};
	static const uint8_t xannor_loss[] = {0xff, 0xff, 0xff};
	struct utility_random_script script = {
		kill_draws, sizeof(kill_draws), 0
	};
	struct yt_random random;
	struct yt_maintenance_xannor_player_result result;
	struct yt_error error;
	float fighters = 1.0f;
	float shields = 1.0f;
	float xannor = 1.0f;
	char line[160];

	yt_error_clear(&error);
	yt_random_init(&random);
	yt_random_set_provider(&random, utility_random_fill, &script);
	if (!yt_maintenance_xannor_player_combat(&random, &fighters,
	    &shields, &xannor, &result, &error)
	    || fighters != 0.0f || shields != 0.0f || xannor != 1.0f
	    || result.player_fighter_losses != 1.0f
	    || result.xannor_losses != 0.0f || random.draws != 2U
	    || script.position != sizeof(kill_draws)
	    || !yt_maintenance_xannor_player_line("Alice", &result,
	    xannor, shields, true, line, sizeof(line))
	    || strcmp(line,
	    " *** Alice: lost 1, dstrd 0 (Player Killed)") != 0)
		return false;

	script = (struct utility_random_script){xannor_loss,
	    sizeof(xannor_loss), 0};
	yt_random_set_provider(&random, utility_random_fill, &script);
	fighters = 1.0f;
	shields = 10.0f;
	xannor = 1.0f;
	if (!yt_maintenance_xannor_player_combat(&random, &fighters,
	    &shields, &xannor, &result, &error)
	    || fighters != 1.0f || shields != 10.0f || xannor != 0.0f
	    || result.player_fighter_losses != 0.0f
	    || result.xannor_losses != 1.0f || random.draws != 1U
	    || !yt_maintenance_xannor_player_line("Bob", &result,
	    xannor, shields, false, line, sizeof(line))
	    || strcmp(line,
	    " *** Bob: lost 0, dstrd 1 (Xannor Lost) - Shields: 10") != 0)
		return false;
	return !yt_maintenance_xannor_player_line("Alice", &result,
	    xannor, shields, false, line, 1U);
}

struct utility_lcg {
	uint32_t state;
};

struct utility_graph_script {
	size_t draws;
};

struct rmt_presentation_tape {
	struct yt_rmt_output_state state;
	uint8_t local[8192];
	size_t local_length;
	uint8_t serial[8192];
	size_t serial_length;
	size_t calls;
	uint16_t sites[256];
	bool local_mode;
	bool fail;
	uint16_t fail_site;
};

static bool
rmt_presentation_append(struct rmt_presentation_tape *tape,
    enum yt_rmt_output_entry entry, const uint8_t *payload,
    size_t payload_length)
{
	struct yt_rmt_output_result output;
	struct yt_rmt_output_state final;
	uint8_t local[256];
	uint8_t serial[256];

	if (!yt_rmt_output_compose_state(entry, payload, payload_length,
	    tape->local_mode, &tape->state, local, sizeof(local), serial,
	    sizeof(serial), &output, &final)
	    || output.local_length > sizeof(tape->local) - tape->local_length
	    || output.serial_length > sizeof(tape->serial) - tape->serial_length)
		return false;
	if (output.local_length != 0U)
		memcpy(tape->local + tape->local_length, local,
		    output.local_length);
	if (output.serial_length != 0U)
		memcpy(tape->serial + tape->serial_length, serial,
		    output.serial_length);
	tape->local_length += output.local_length;
	tape->serial_length += output.serial_length;
	tape->state = final;
	return true;
}

static bool
rmt_presentation_collect(void *context, uint16_t site,
    enum yt_rmt_output_entry entry, const uint8_t *payload,
    size_t payload_length, struct yt_error *error)
{
	struct rmt_presentation_tape *tape = context;

	(void)error;
	if (tape == NULL || (tape->fail && tape->fail_site == site)
	    || !rmt_presentation_append(tape, entry, payload, payload_length))
		return false;
	if (tape->calls >= YT_ARRAY_LEN(tape->sites))
		return false;
	tape->sites[tape->calls] = site;
	++tape->calls;
	if (site == 0x10f1U && !tape->local_mode
	    && tape->state.serial_column > 50U) {
		if (!rmt_presentation_append(tape, YT_RMT_OUTPUT_SERIAL_LINE,
		    NULL, 0U))
			return false;
		if (tape->calls >= YT_ARRAY_LEN(tape->sites))
			return false;
		tape->sites[tape->calls] = 0x1113U;
		++tape->calls;
	}
	return true;
}

static size_t
rmt_presentation_site_count(const struct rmt_presentation_tape *tape,
    uint16_t site)
{
	size_t count = 0U;

	for (size_t index = 0U; index < tape->calls; ++index) {
		if (tape->sites[index] == site)
			++count;
	}
	return count;
}

static void
rmt_small_config(struct yt_config *config)
{
	memset(config, 0, sizeof(*config));
	strcpy(config->scoreboard, "SCORE.TXT");
	config->turns_per_day = 500.0f;
	config->sector_offset = 3.0f;
	config->port_offset = 23.0f;
	config->planet_offset = 28.0f;
	config->initial_fighters = 25.0f;
	config->initial_credits = 1005.0f;
	config->initial_holds = 10.0f;
	config->retention_days = 14.0f;
	config->local_screen = -1.0f;
	config->total_records = 30.0f;
	config->lottery_plays = 5.0f;
	config->genesis_ports = 200.0f;
	config->headquarters = 85.0f;
	config->maximum_holds = 1000.0f;
	config->marker = 6324.0f;
}

static bool
utility_lcg_fill(void *context, void *buffer, size_t length,
    struct yt_error *error)
{
	struct utility_lcg *lcg = context;
	uint8_t *bytes = buffer;

	if (length != 3U) {
		if (error != NULL)
			error->status = YT_RANDOM_ERROR;
		return false;
	}
	lcg->state = (lcg->state * UINT32_C(0x343fd)
	    + UINT32_C(0x1a9ec3)) & UINT32_C(0xffffff);
	bytes[0] = (uint8_t)lcg->state;
	bytes[1] = (uint8_t)(lcg->state >> 8);
	bytes[2] = (uint8_t)(lcg->state >> 16);
	return true;
}

static bool
utility_graph_retry_fill(void *context, void *buffer, size_t length,
    struct yt_error *error)
{
	struct utility_graph_script *script = context;
	uint8_t *bytes = buffer;
	uint32_t sample = UINT32_C(0x800000);

	if (length != 3U) {
		if (error != NULL)
			error->status = YT_RANDOM_ERROR;
		return false;
	}
	switch (script->draws) {
	case 7U: /* Sector 2 slot 1 targets occupied sector-1 slot 1. */
	case 8U:
	case 18U: /* Sector 3 slot 1 writes a reciprocal link to sector 2. */
	case 19U:
	case 20U: /* Sector 3 slot 2 finds an occupied destination slot. */
	case 21U:
	case 22U: /* Sector 3 slot 3 starts occupied at the source. */
	case 23U:
	case 24U: /* Sector 3 slot 4 suppresses a duplicate sector pair. */
	case 25U:
		sample = 0U;
		break;
	case 14U: /* Sector 2 long-warp probability: admit a target draw. */
		sample = UINT32_C(0xfd70a4);
		break;
	case 15U: /* Bound seven maps this sample back to sector 2 itself. */
		sample = UINT32_C(0x333333);
		break;
	case 16U: /* The required retry admits a second target draw. */
	case 28U: /* Sector 3 admits an empty long-warp destination. */
		sample = UINT32_C(0xfd70a4);
		break;
	case 17U: /* Sector 6 already owns its sixth reciprocal slot. */
		sample = UINT32_C(0xcccccc);
		break;
	case 29U: /* Bound seven maps the empty target to sector 4. */
		sample = UINT32_C(0x800000);
		break;
	case 42U:
	case 55U: /* Sector 7 remains empty after its first pass. */
	case 56U: /* Its retry takes a local warp to sector 6. */
	case 57U:
	case 63U:
	case 64U: /* The first shortcut position is 8, beyond sector 7. */
		sample = 0U;
		break;
	default:
		break;
	}
	bytes[0] = (uint8_t)sample;
	bytes[1] = (uint8_t)(sample >> 8);
	bytes[2] = (uint8_t)(sample >> 16);
	++script->draws;
	return true;
}

static bool
utility_fixed_clock(void *context, struct yt_clock_value *value,
    struct yt_error *error)
{
	(void)context;
	(void)error;
	*value = (struct yt_clock_value){2026, 7, 22, 22, 46, 8, 16};
	return true;
}

struct utility_clock_count {
	size_t calls;
};

static bool
utility_counted_fixed_clock(void *context, struct yt_clock_value *value,
    struct yt_error *error)
{
	struct utility_clock_count *count = context;

	if (count == NULL || count->calls >= 17U) {
		if (error != NULL)
			error->status = YT_RANGE;
		return false;
	}
	++count->calls;
	*value = (struct yt_clock_value){2026, 7, 22, 22, 46, 8, 16};
	return true;
}

struct yt_init_capture_event {
	uint16_t site;
	enum yt_init_output_entry entry;
	uint8_t payload[96];
	size_t length;
};

struct yt_init_capture {
	struct yt_init_capture_event events[12000];
	size_t calls;
	uint16_t fail_site;
};

static bool
yt_init_capture_write(void *context, uint16_t site,
    enum yt_init_output_entry entry, const uint8_t *payload,
    size_t payload_length, struct yt_error *error)
{
	struct yt_init_capture *capture = context;
	struct yt_init_capture_event *event;

	(void)error;
	if (capture == NULL || capture->fail_site == site
	    || capture->calls >= YT_ARRAY_LEN(capture->events)
	    || payload_length > sizeof(capture->events[0].payload)
	    || (payload == NULL && payload_length != 0U))
		return false;
	event = &capture->events[capture->calls++];
	event->site = site;
	event->entry = entry;
	event->length = payload_length;
	if (payload_length != 0U)
		memcpy(event->payload, payload, payload_length);
	return true;
}

static bool
yt_init_capture_is(const struct yt_init_capture *capture, size_t index,
    uint16_t site, enum yt_init_output_entry entry, const char *text)
{
	size_t length = strlen(text);

	return index < capture->calls && capture->events[index].site == site
	    && capture->events[index].entry == entry
	    && capture->events[index].length == length
	    && memcmp(capture->events[index].payload, text, length) == 0;
}

static bool
test_yt_init_pre_input_presentation(void)
{
	static const uint16_t prefix_sites[] = {
		0x060aU, 0x061eU, 0x0630U, 0x0641U, 0x0653U,
		0x0665U, 0x0677U, 0x0688U, 0x069aU
	};
	static const uint16_t prepared_sites[] = {
		0x0769U, 0x077bU, 0x0787U, 0x0799U, 0x07b6U,
		0x07c2U, 0x07ebU, 0x07f7U, 0x0820U, 0x082cU,
		0x0855U, 0x0861U, 0x089cU, 0x08c7U, 0x08d3U,
		0x08fcU, 0x0908U, 0x0960U, 0x0967U, 0x0990U,
		0x09ccU, 0x09d3U, 0x09e5U, 0x09f7U, 0x0a18U,
		0x0a29U, 0x0a3bU, 0x0a4dU, 0x0a5fU
	};
	struct utility_lcg lcg = {UINT32_C(0x89b405)};
	struct utility_clock_count clock = {0U};
	struct yt_random random;
	struct yt_initializer_preparation preparation;
	struct yt_init_capture capture = {0};
	struct yt_init_presenter presenter = {
		.context = &capture,
		.write = yt_init_capture_write
	};
	struct yt_error error;
	size_t index;

	yt_error_clear(&error);
	yt_initializer_layout_yt(&preparation);
	if (preparation.config.sector_offset != 51.0f
	    || preparation.config.port_offset != 2055.0f
	    || preparation.config.planet_offset != 3055.0f
	    || preparation.config.total_records != 3155.0f
	    || preparation.config.turns_per_day != 0.0f
	    || preparation.config.initial_fighters != 0.0f
	    || preparation.config.maximum_holds != 0.0f
	    || !yt_init_present_confirmation_prefix(&presenter, &error)
	    || capture.calls != YT_ARRAY_LEN(prefix_sites))
		return false;
	for (index = 0U; index < YT_ARRAY_LEN(prefix_sites); ++index) {
		if (capture.events[index].site != prefix_sites[index])
			return false;
	}
	if (!yt_init_capture_is(&capture, 0U, 0x060aU,
	    YT_INIT_OUTPUT_LINE, "")
	    || !yt_init_capture_is(&capture, 1U, 0x061eU,
	    YT_INIT_OUTPUT_LINE,
	    "            Yankee Trader Initialization Program")
	    || !yt_init_capture_is(&capture, 8U, 0x069aU,
	    YT_INIT_OUTPUT_INLINE, "Continue (Y/N)? "))
		return false;

	memset(&capture, 0, sizeof(capture));
	if (!yt_init_present_opening(&presenter, &error)
	    || capture.calls != 2U
	    || !yt_init_capture_is(&capture, 0U, 0x06dcU,
	    YT_INIT_OUTPUT_LINE, "")
	    || !yt_init_capture_is(&capture, 1U, 0x06eeU,
	    YT_INIT_OUTPUT_LINE, "Creating main data file: YTDATA.DAT"))
		return false;

	yt_random_init(&random);
	yt_random_set_provider(&random, utility_lcg_fill, &lcg);
	yt_platform_set_clock_provider(utility_counted_fixed_clock, &clock);
	yt_error_clear(&error);
	if (!yt_initializer_prepare_yt(&random, &preparation, &error)) {
		yt_platform_set_clock_provider(NULL, NULL);
		return false;
	}
	yt_platform_set_clock_provider(NULL, NULL);
	if (random.draws != 1U || lcg.state != UINT32_C(0x5dd6b4)
	    || clock.calls != 2U
	    || preparation.config.epoch_year != 26.0f
	    || preparation.config.headquarters != 733.0f
	    || preparation.config.last_maintenance != 202.0f
	    || preparation.today != 203)
		return false;

	memset(&capture, 0, sizeof(capture));
	if (!yt_init_present_prepared_configuration(&preparation,
	    &presenter, &error)
	    || capture.calls != YT_ARRAY_LEN(prepared_sites))
		return false;
	for (index = 0U; index < YT_ARRAY_LEN(prepared_sites); ++index) {
		if (capture.events[index].site != prepared_sites[index])
			return false;
	}
	if (!yt_init_capture_is(&capture, 2U, 0x0787U,
	    YT_INIT_OUTPUT_LINE, " 26")
	    || !yt_init_capture_is(&capture, 14U, 0x08d3U,
	    YT_INIT_OUTPUT_LINE, " 500")
	    || !yt_init_capture_is(&capture, 16U, 0x0908U,
	    YT_INIT_OUTPUT_LINE, " 5")
	    || !yt_init_capture_is(&capture, 18U, 0x0967U,
	    YT_INIT_OUTPUT_LINE, " 733 ")
	    || !yt_init_capture_is(&capture, 28U, 0x0a5fU,
	    YT_INIT_OUTPUT_INLINE, "-=> "))
		return false;

	memset(&capture, 0, sizeof(capture));
	capture.fail_site = 0x0653U;
	yt_error_clear(&error);
	return !yt_init_present_confirmation_prefix(&presenter, &error)
	    && error.status == YT_IO_ERROR && capture.calls == 4U
	    && !yt_initializer_prepare_yt(NULL, &preparation, &error)
	    && !yt_init_present_prepared_configuration(NULL, &presenter,
	    &error);
}

struct utility_clock_sequence {
	size_t calls;
};

static bool
utility_initializer_clock_sequence(void *context,
    struct yt_clock_value *value,
    struct yt_error *error)
{
	struct utility_clock_sequence *sequence = context;

	if (sequence->calls >= 17U) {
		if (error != NULL)
			error->status = YT_RANGE;
		return false;
	}
	if (sequence->calls == 0U)
		*value = (struct yt_clock_value){2026, 1, 1, 0, 0, 0, 0};
	else if (sequence->calls == 1U)
		*value = (struct yt_clock_value){2026, 1, 2, 0, 0, 0, 0};
	else
		*value = (struct yt_clock_value){2026, 1, 10, 0, 0, 0, 0};
	++sequence->calls;
	return true;
}

static uint64_t
utility_fnv1a64(const uint8_t *data, size_t length)
{
	uint64_t hash = UINT64_C(14695981039346656037);
	size_t index;

	for (index = 0; index < length; ++index) {
		hash ^= data[index];
		hash *= UINT64_C(1099511628211);
	}
	return hash;
}

static uint64_t
yt_init_capture_hash(const struct yt_init_capture *capture)
{
	uint64_t hash = UINT64_C(14695981039346656037);
	size_t index;

#define YT_INIT_HASH_BYTE(value) do { \
	hash ^= (uint8_t)(value); \
	hash *= UINT64_C(1099511628211); \
} while (0)
	for (index = 0U; index < capture->calls; ++index) {
		const struct yt_init_capture_event *event =
		    &capture->events[index];
		size_t byte;

		YT_INIT_HASH_BYTE(event->site);
		YT_INIT_HASH_BYTE(event->site >> 8);
		YT_INIT_HASH_BYTE(event->entry);
		YT_INIT_HASH_BYTE(event->length);
		YT_INIT_HASH_BYTE(event->length >> 8);
		for (byte = 0U; byte < event->length; ++byte)
			YT_INIT_HASH_BYTE(event->payload[byte]);
	}
#undef YT_INIT_HASH_BYTE
	return hash;
}

static size_t
yt_init_capture_site_count(const struct yt_init_capture *capture,
    uint16_t site)
{
	size_t count = 0U;
	size_t index;

	for (index = 0U; index < capture->calls; ++index) {
		if (capture->events[index].site == site)
			++count;
	}
	return count;
}

static size_t
yt_init_capture_first_site(const struct yt_init_capture *capture,
    uint16_t site)
{
	size_t index;

	for (index = 0U; index < capture->calls; ++index) {
		if (capture->events[index].site == site)
			return index;
	}
	return SIZE_MAX;
}

static bool
test_yt_init_presented_world(void)
{
	struct utility_lcg lcg = {UINT32_C(0x89b405)};
	struct utility_clock_count clock = {0U};
	struct yt_random random;
	struct yt_initializer_preparation preparation;
	struct yt_init_capture capture = {0};
	struct yt_init_presenter presenter = {
		.context = &capture,
		.write = yt_init_capture_write
	};
	struct yt_error error;
	uint8_t *database = NULL;
	size_t database_length = 0U;
	uint64_t tape_hash;
	bool ok;

	yt_error_clear(&error);
	yt_random_init(&random);
	yt_random_set_provider(&random, utility_lcg_fill, &lcg);
	yt_platform_set_clock_provider(utility_counted_fixed_clock, &clock);
	ok = yt_init_present_confirmation_prefix(&presenter, &error)
	    && yt_init_present_opening(&presenter, &error)
	    && yt_initialize_begin_yt(&error)
	    && yt_initializer_prepare_yt(&random, &preparation, &error)
	    && yt_init_present_prepared_configuration(&preparation,
	    &presenter, &error)
	    && yt_initialize_yt_prepared(&preparation, "YTSCORE.ASC", &random,
	    &presenter, &error);
	yt_platform_set_clock_provider(NULL, NULL);
	if (!ok || !read_file("YTDATA.DAT", &database, &database_length)) {
		free(database);
		return false;
	}
	tape_hash = yt_init_capture_hash(&capture);
	ok = random.draws == 31297U && lcg.state == UINT32_C(0x9f26f4)
	    && clock.calls == 17U
	    && database_length == 432235U
	    && utility_fnv1a64(database, database_length)
	    == UINT64_C(0xe1010e9fdf9998f1)
	    && capture.calls == 8379U
	    && tape_hash == UINT64_C(0xe72aa6db06fef26c)
	    && yt_init_capture_site_count(&capture, 0x10f8U) == 35U
	    && yt_init_capture_site_count(&capture, 0x1102U) == 35U
	    && yt_init_capture_site_count(&capture, 0x110aU) == 35U
	    && yt_init_capture_site_count(&capture, 0x1501U) == 21U
	    && yt_init_capture_site_count(&capture, 0x15c8U) == 21U
	    && yt_init_capture_site_count(&capture, 0x12acU) == 2003U
	    && yt_init_capture_site_count(&capture, 0x12b9U) == 2003U
	    && yt_init_capture_site_count(&capture, 0x12c1U) == 2003U
	    && yt_init_capture_site_count(&capture, 0x1b1eU) == 1000U
	    && yt_init_capture_site_count(&capture, 0x1c59U) == 1000U
	    && yt_init_capture_site_count(&capture, 0x08d3U) == 1U
	    && yt_init_capture_site_count(&capture, 0x0908U) == 1U
	    && yt_init_capture_site_count(&capture, 0x0967U) == 1U
	    && yt_init_capture_site_count(&capture, 0x0a5fU) == 1U
	    && yt_init_capture_site_count(&capture, 0x0b09U) == 1U
	    && yt_init_capture_site_count(&capture, 0x2339U) == 1U
	    && yt_init_capture_site_count(&capture, 0x235cU) == 1U
	    && yt_init_capture_site_count(&capture, 0x23c5U) == 1U
	    && yt_init_capture_first_site(&capture, 0x08d3U)
	    < yt_init_capture_first_site(&capture, 0x0908U)
	    && yt_init_capture_first_site(&capture, 0x0908U)
	    < yt_init_capture_first_site(&capture, 0x0967U)
	    && yt_init_capture_first_site(&capture, 0x0967U)
	    < yt_init_capture_first_site(&capture, 0x0a5fU)
	    && yt_init_capture_first_site(&capture, 0x2339U)
	    < yt_init_capture_first_site(&capture, 0x235cU)
	    && yt_init_capture_first_site(&capture, 0x235cU)
	    < yt_init_capture_first_site(&capture, 0x23c5U)
	    && capture.events[0].site == 0x060aU
	    && capture.events[capture.calls - 1U].site == 0x23cfU;
	free(database);
	return ok;
}

static bool
test_yt_init_presentation_pre_put_failure(void)
{
	struct utility_lcg lcg = {UINT32_C(0x89b405)};
	struct yt_random random;
	struct yt_initializer_preparation preparation;
	struct yt_init_capture capture = {.fail_site = 0x0abbU};
	struct yt_init_presenter presenter = {
		.context = &capture,
		.write = yt_init_capture_write
	};
	struct yt_error error;
	uint8_t *database = NULL;
	size_t database_length = 0U;
	bool initialized;
	bool read;

	yt_error_clear(&error);
	yt_random_init(&random);
	yt_random_set_provider(&random, utility_lcg_fill, &lcg);
	yt_platform_set_clock_provider(utility_fixed_clock, NULL);
	initialized = yt_initialize_begin_yt(&error)
	    && yt_initializer_prepare_yt(&random, &preparation, &error)
	    && yt_initialize_yt_prepared(&preparation, "YTSCORE.ASC", &random,
	    &presenter, &error);
	yt_platform_set_clock_provider(NULL, NULL);
	read = read_file("YTDATA.DAT", &database, &database_length);
	free(database);
	return !initialized && read && error.status == YT_IO_ERROR
	    && random.draws == 1U && lcg.state == UINT32_C(0x5dd6b4)
	    && capture.calls == 0U && database_length == 0U;
}

static bool
test_yt_init_presentation_failure_prefix(void)
{
	static const uint8_t sector_offset_raw[] = {0x00U, 0x00U, 0x4cU, 0x86U};
	struct utility_lcg lcg = {UINT32_C(0x89b405)};
	struct yt_random random;
	struct yt_initializer_preparation preparation;
	struct yt_init_capture capture = {.fail_site = 0x0b1bU};
	struct yt_init_presenter presenter = {
		.context = &capture,
		.write = yt_init_capture_write
	};
	struct yt_error error;
	uint8_t *database = NULL;
	size_t database_length = 0U;
	bool initialized;
	bool read;

	yt_error_clear(&error);
	yt_random_init(&random);
	yt_random_set_provider(&random, utility_lcg_fill, &lcg);
	yt_platform_set_clock_provider(utility_fixed_clock, NULL);
	initialized = yt_initialize_begin_yt(&error)
	    && yt_initializer_prepare_yt(&random, &preparation, &error)
	    && yt_initialize_yt_prepared(&preparation, "YTSCORE.ASC", &random,
	    &presenter, &error);
	yt_platform_set_clock_provider(NULL, NULL);
	read = read_file("YTDATA.DAT", &database, &database_length);
	free(database);
	return !initialized && read && error.status == YT_IO_ERROR
	    && random.draws == 1U && lcg.state == UINT32_C(0x5dd6b4)
	    && capture.calls == 2U
	    && capture.events[0].site == 0x0abbU
	    && capture.events[1].site == 0x0b09U
	    && capture.events[1].entry == YT_INIT_OUTPUT_LINE
	    && capture.events[1].length == sizeof(sector_offset_raw)
	    && memcmp(capture.events[1].payload, sector_offset_raw,
	    sizeof(sector_offset_raw)) == 0
	    && database_length == YT_RECORD_SIZE;
}

static bool
test_yt_init_sector_prepass(void)
{
	static const uint8_t port_offset_raw[] = {
		0x00U, 0x70U, 0x00U, 0x8cU
	};
	struct yt_database database = {0};
	struct yt_record initial;
	struct yt_record expected;
	struct yt_record actual;
	struct yt_error error;
	float port_offset = 0.0f;
	bool ok = false;

	(void)remove("YTPREPASS.DAT");
	memset(initial.bytes, 0xa5, sizeof(initial.bytes));
	expected = initial;
	memcpy(expected.bytes + YT_F57, port_offset_raw,
	    sizeof(port_offset_raw));
	yt_error_clear(&error);
	if (!yt_database_open(&database, "YTPREPASS.DAT", YT_OPEN_CREATE,
	    &error)
	    || !yt_database_write(&database, 1U, &initial, &error)
	    || !yt_database_flush(&database, &error)
	    || !yt_init_sector_prepass(&database, 51.0f, 2004,
	    &port_offset, &error)
	    || port_offset != 2055.0f
	    || !yt_database_read(&database, 1U, &actual, &error)
	    || memcmp(actual.bytes, expected.bytes, sizeof(actual.bytes)) != 0)
		goto done;
	yt_database_close(&database);
	memset(&database, 0, sizeof(database));
	yt_error_clear(&error);
	if (!yt_database_open(&database, "YTPREPASS.DAT", YT_OPEN_READ,
	    &error))
		goto done;
	port_offset = 0.0f;
	if (yt_init_sector_prepass(&database, 51.0f, 2004,
	    &port_offset, &error)
	    || port_offset != 2055.0f || error.status != YT_IO_ERROR
	    || strcmp(error.operation, "write record") != 0)
		goto done;
	yt_database_close(&database);
	memset(&database, 0, sizeof(database));
	(void)remove("YTPREPASS.DAT");
	yt_error_clear(&error);
	if (!yt_database_open(&database, "YTPREPASS.DAT", YT_OPEN_CREATE,
	    &error))
		goto done;
	port_offset = 0.0f;
	yt_record_clear(&expected);
	memcpy(expected.bytes + YT_F57, port_offset_raw,
	    sizeof(port_offset_raw));
	ok = yt_init_sector_prepass(&database, 51.0f, 2004,
	    &port_offset, &error)
	    && port_offset == 2055.0f && database.records == 1U
	    && yt_database_read(&database, 1U, &actual, &error)
	    && memcmp(actual.bytes, expected.bytes, sizeof(actual.bytes)) == 0;

done:
	yt_database_close(&database);
	(void)remove("YTPREPASS.DAT");
	return ok;
}

static bool
test_yt_init_random_binding(void)
{
	static const uint8_t short_record[] = {'A', 'B', 'C'};
	struct yt_database database = {0};
	struct yt_init_binding binding;
	struct yt_error error;
	size_t index;
	bool ok = false;

	(void)remove("YTDATA.DAT");
	yt_error_clear(&error);
	if (!yt_initialize_begin_yt(&error)
	    || !yt_initialize_bind_yt(&database, &binding, &error)
	    || database.file == NULL || database.records != 0U
	    || binding.first_accepted != 0U
	    || binding.second_accepted != 0U)
		goto done;
	for (index = 0U; index < YT_RECORD_SIZE; ++index) {
		if (binding.loaded.record.bytes[index] != 0U
		    || binding.second_record.bytes[index] != 0U)
			goto done;
	}
	if (binding.loaded.scoreboard[0] != '\0'
	    || binding.loaded.scoreboard_length != 0.0f
	    || binding.loaded.epoch_year != 0.0f
	    || binding.loaded.total_records != 0.0f)
		goto done;
	yt_database_close(&database);
	if (!write_file("YTDATA.DAT", short_record, sizeof(short_record)))
		goto done;
	yt_error_clear(&error);
	if (!yt_initialize_bind_yt(&database, &binding, &error)
	    || database.file == NULL || database.records != 0U
	    || binding.first_accepted != sizeof(short_record)
	    || binding.second_accepted != sizeof(short_record)
	    || memcmp(binding.loaded.record.bytes, short_record,
	    sizeof(short_record)) != 0
	    || memcmp(binding.second_record.bytes, short_record,
	    sizeof(short_record)) != 0)
		goto done;
	for (index = sizeof(short_record); index < YT_RECORD_SIZE; ++index) {
		if (binding.loaded.record.bytes[index] != 0U
		    || binding.second_record.bytes[index] != 0U)
			goto done;
	}
	yt_database_close(&database);
	(void)remove("YTDATA.DAT");
	yt_error_clear(&error);
	ok = !yt_initialize_bind_yt(&database, &binding, &error)
	    && database.file == NULL && error.status == YT_NOT_FOUND
	    && strcmp(error.operation, "resolve path") == 0;

done:
	yt_database_close(&database);
	(void)remove("YTDATA.DAT");
	return ok;
}

static bool
test_initializer_world_image(void)
{
	struct utility_lcg lcg = {UINT32_C(0x89b405)};
	struct yt_random random;
	struct yt_error error;
	uint8_t *database = NULL;
	size_t length = 0;
	uint64_t hash = 0;
	bool ok;

	yt_error_clear(&error);
	yt_random_init(&random);
	yt_random_set_provider(&random, utility_lcg_fill, &lcg);
	yt_platform_set_clock_provider(utility_fixed_clock, NULL);
	ok = yt_initialize_yt("YTSCORE.ASC", &random, &error);
	yt_platform_set_clock_provider(NULL, NULL);
	if (!ok || random.draws != 31297U || lcg.state != UINT32_C(0x9f26f4)
	    || !read_file("YTDATA.DAT", &database, &length)) {
		fprintf(stderr, "initializer image setup: ok=%d status=%d draws=%zu state=%06x length=%zu\n",
		    ok, error.status, random.draws, lcg.state, length);
		free(database);
		return false;
	}
	hash = utility_fnv1a64(database, length);
	ok = length == 432235U && hash == UINT64_C(0xe1010e9fdf9998f1);
	if (!ok)
		fprintf(stderr, "initializer image: length=%zu hash=%016llx\n",
		    length, (unsigned long long)hash);
	free(database);
	return ok;
}

static bool
test_initializer_graph_retries(void)
{
	struct utility_graph_script script = {0U};
	struct yt_random random;
	struct yt_initializer_options options;
	struct yt_config decoded;
	struct yt_error error;
	struct yt_record config_record;
	struct yt_record sector_two;
	struct yt_record sector_three;
	struct yt_record sector_four;
	struct yt_record sector_six;
	struct yt_record sector_seven;
	uint8_t *database = NULL;
	size_t length = 0U;
	size_t offset;
	uint64_t hash;
	bool ok;

	memset(&options, 0, sizeof(options));
	options.family = YT_INITIALIZER_YT;
	rmt_small_config(&options.config);
	options.config.epoch_year = 26.0f;
	options.config.port_offset = 10.0f;
	options.config.planet_offset = 14.0f;
	options.config.total_records = 15.0f;
	options.use_existing_config = true;
	yt_error_clear(&error);
	yt_random_init(&random);
	yt_random_set_provider(&random, utility_graph_retry_fill, &script);
	yt_platform_set_clock_provider(utility_fixed_clock, NULL);
	ok = yt_initialize_world(&options, &random, &error);
	yt_platform_set_clock_provider(NULL, NULL);
	if (!ok || random.draws != 105U || script.draws != 105U
	    || !read_file("YTDATA.DAT", &database, &length)
	    || length != 2055U) {
		fprintf(stderr, "initializer graph retry: ok=%d status=%d draws=%zu/%zu length=%zu\n",
		    ok, error.status, (size_t)random.draws, script.draws, length);
		free(database);
		return false;
	}
	memcpy(config_record.bytes, database, YT_RECORD_SIZE);
	if (!yt_config_decode(&decoded, &config_record, &error)) {
		free(database);
		return false;
	}
	offset = ((size_t)yt_sector_basic_record(&decoded, 2) - 1U)
	    * YT_RECORD_SIZE;
	memcpy(sector_two.bytes, database + offset, YT_RECORD_SIZE);
	offset = ((size_t)yt_sector_basic_record(&decoded, 3) - 1U)
	    * YT_RECORD_SIZE;
	memcpy(sector_three.bytes, database + offset, YT_RECORD_SIZE);
	offset = ((size_t)yt_sector_basic_record(&decoded, 4) - 1U)
	    * YT_RECORD_SIZE;
	memcpy(sector_four.bytes, database + offset, YT_RECORD_SIZE);
	offset = ((size_t)yt_sector_basic_record(&decoded, 6) - 1U)
	    * YT_RECORD_SIZE;
	memcpy(sector_six.bytes, database + offset, YT_RECORD_SIZE);
	offset = ((size_t)yt_sector_basic_record(&decoded, 7) - 1U)
	    * YT_RECORD_SIZE;
	memcpy(sector_seven.bytes, database + offset, YT_RECORD_SIZE);
	hash = utility_fnv1a64(database, length);
	ok = yt_record_get_number(&sector_two, YT_F41) == 3.0f
	    && yt_record_get_number(&sector_two, YT_F61) == 0.0f
	    && yt_record_get_number(&sector_three, YT_F41) == 2.0f
	    && yt_record_get_number(&sector_three, YT_F45) == 0.0f
	    && yt_record_get_number(&sector_three, YT_F53) == 0.0f
	    && yt_record_get_number(&sector_three, YT_F61) == 4.0f
	    && yt_record_get_number(&sector_four, YT_F61) == 3.0f
	    && yt_record_get_number(&sector_six, YT_F41) == 7.0f
	    && yt_record_get_number(&sector_seven, YT_F41) == 6.0f
	    && hash == UINT64_C(0x4cda66caf10b15d4);
	if (!ok)
		fprintf(stderr, "initializer graph retry image: hash=%016llx s2.0=%g s2.6=%g s3.0=%g s3.1=%g s3.3=%g s3.6=%g s4.6=%g s6.0=%g s7.0=%g\n",
		    (unsigned long long)hash,
		    (double)yt_record_get_number(&sector_two, YT_F41),
		    (double)yt_record_get_number(&sector_two, YT_F61),
		    (double)yt_record_get_number(&sector_three, YT_F41),
		    (double)yt_record_get_number(&sector_three, YT_F45),
		    (double)yt_record_get_number(&sector_three, YT_F53),
		    (double)yt_record_get_number(&sector_three, YT_F61),
		    (double)yt_record_get_number(&sector_four, YT_F61),
		    (double)yt_record_get_number(&sector_six, YT_F41),
		    (double)yt_record_get_number(&sector_seven, YT_F41));
	free(database);
	return ok;
}

static bool
test_yt_init_random_close_failure(void)
{
	struct utility_graph_script script = {0U};
	struct portname_close_fault close_fault = {0U};
	struct yt_initializer_options options;
	struct yt_database database = {0};
	struct yt_random random;
	struct yt_init_capture capture = {0};
	struct yt_init_presenter presenter = {
		.context = &capture,
		.write = yt_init_capture_write
	};
	struct yt_error error;
	uint8_t *auxiliary = NULL;
	size_t auxiliary_length = 0U;
	bool news_exists;
	bool names_exist;
	bool ok = false;

	(void)remove("YTDATA.DAT");
	(void)remove("YTNEWS.DAT");
	(void)remove("YTNAME.DAT");
	memset(&options, 0, sizeof(options));
	options.family = YT_INITIALIZER_YT;
	rmt_small_config(&options.config);
	options.config.epoch_year = 26.0f;
	options.config.port_offset = 10.0f;
	options.config.planet_offset = 14.0f;
	options.config.total_records = 15.0f;
	options.use_existing_config = true;
	options.bound_database = &database;
	options.yt_presenter = &presenter;
	yt_error_clear(&error);
	yt_random_init(&random);
	yt_random_set_provider(&random, utility_graph_retry_fill, &script);
	if (!yt_database_open(&database, "YTDATA.DAT", YT_OPEN_CREATE,
	    &error))
		goto done;
	yt_database_set_close_provider(&database, portname_close_fail,
	    &close_fault);
	yt_platform_set_clock_provider(utility_fixed_clock, NULL);
	if (yt_initialize_world(&options, &random, &error))
		goto clock_done;
	yt_platform_set_clock_provider(NULL, NULL);
	news_exists = read_file("YTNEWS.DAT", &auxiliary, &auxiliary_length);
	free(auxiliary);
	auxiliary = NULL;
	auxiliary_length = 0U;
	names_exist = read_file("YTNAME.DAT", &auxiliary, &auxiliary_length);
	free(auxiliary);
	auxiliary = NULL;
	ok = error.status == YT_IO_ERROR
	    && strcmp(error.operation, "random CLOSE") == 0
	    && random.draws == 105U && script.draws == 105U
	    && close_fault.calls == 2U
	    && database.last_close.outcome == YT_DATABASE_CLOSE_DISK_ERROR
	    && database.last_close.basic_error == 70U
	    && database.last_close.dos_error == 5U
	    && database.last_close.retry_attempted
	    && !database.last_close.registered
	    && database.last_close.handle_open
	    && database.file == NULL && database.orphaned_file != NULL
	    && capture.calls >= 2U
	    && yt_init_capture_is(&capture, capture.calls - 2U, 0x208cU,
	    YT_INIT_OUTPUT_LINE, "")
	    && yt_init_capture_is(&capture, capture.calls - 1U, 0x209eU,
	    YT_INIT_OUTPUT_LINE, "Setting up newspaper file.")
	    && !news_exists && !names_exist;
	goto done;

clock_done:
	yt_platform_set_clock_provider(NULL, NULL);
done:
	free(auxiliary);
	yt_database_close(&database);
	(void)remove("YTDATA.DAT");
	(void)remove("YTNEWS.DAT");
	(void)remove("YTNAME.DAT");
	return ok;
}

static bool
test_rmt_initializer_world_image(void)
{
	static const uint16_t expected_sites[] = {
		0x0863U, 0x0871U, 0x087fU, 0x0882U, 0x0885U, 0x089bU,
		0x0907U, 0x0915U, 0x0930U, 0x096eU, 0x099cU, 0x09caU,
		0x09fdU, 0x0a2bU, 0x0a59U, 0x0ab6U, 0x0ad1U, 0x0b0fU,
		0x0b35U, 0x0b5dU, 0x0b7bU, 0x0babU, 0x0d67U, 0x0d75U,
		0x0da0U, 0x0daeU, 0x0e22U, 0x0e30U, 0x0e33U, 0x1279U,
		0x127cU, 0x128aU, 0x128dU, 0x12f9U, 0x1307U, 0x130aU,
		0x1318U, 0x138eU, 0x139cU, 0x1678U, 0x167bU, 0x1689U,
		0x17f4U, 0x1802U, 0x1c9dU, 0x1ca0U, 0x1caeU, 0x1cc9U,
		0x1d53U, 0x1d61U, 0x1dfaU, 0x1e08U, 0x2014U, 0x2022U,
		0x20a1U, 0x20afU, 0x20ebU, 0x20f9U, 0x227eU,
	};
	struct utility_lcg lcg = {UINT32_C(0x123456)};
	struct yt_random random;
	struct yt_config config;
	struct yt_error error;
	struct rmt_presentation_tape tape = {.local_mode = true};
	struct yt_rmt_presenter presenter = {
		.context = &tape,
		.write = rmt_presentation_collect,
	};
	uint8_t *database = NULL;
	size_t length = 0;
	uint64_t hash = 0;
	bool ok;

	rmt_small_config(&config);
	yt_error_clear(&error);
	yt_random_init(&random);
	yt_random_set_provider(&random, utility_lcg_fill, &lcg);
	yt_platform_set_clock_provider(utility_fixed_clock, NULL);
	ok = yt_initialize_rmt_presented(&config, "The Sysop", &random,
	    &presenter, &error);
	yt_platform_set_clock_provider(NULL, NULL);
	if (!ok || random.draws != 245U || lcg.state != UINT32_C(0x3fed05)
	    || !read_file("YTDATA.DAT", &database, &length)) {
		fprintf(stderr, "RMT initializer image setup: ok=%d status=%d draws=%zu state=%06x length=%zu\n",
		    ok, error.status, random.draws, lcg.state, length);
		free(database);
		return false;
	}
	hash = utility_fnv1a64(database, length);
	ok = length == 4110U && hash == UINT64_C(0xb7c4d78892150634)
	    && tape.calls == 59U && tape.local_length == 1266U
	    && tape.calls == YT_ARRAY_LEN(expected_sites)
	    && memcmp(tape.sites, expected_sites, sizeof(expected_sites)) == 0
	    && utility_fnv1a64(tape.local, tape.local_length)
	    == UINT64_C(0xf8158dbf1e9c9bbf)
	    && tape.serial_length == 0U;
	if (!ok)
		fprintf(stderr, "RMT initializer image: length=%zu hash=%016llx calls=%zu local=%zu local-hash=%016llx serial=%zu\n",
		    length, (unsigned long long)hash, tape.calls, tape.local_length,
		    (unsigned long long)utility_fnv1a64(tape.local,
		    tape.local_length), tape.serial_length);
	free(database);
	return ok;
}

static bool write_file(const char *path, const void *data, size_t length);

static bool
test_rmt_presentation_failure_prefixes(void)
{
	static const uint8_t sentinel[] = "old-world";
	static const struct {
		uint16_t site;
		size_t draws;
		size_t file_length;
		bool preserves_old;
	} cases[] = {
		{0x0863U, 0U, sizeof(sentinel) - 1U, true},
		{0x0907U, 0U, 1U, false},
		{0x0ab6U, 1U, 1U, false},
		{0x0b7bU, 1U, YT_RECORD_SIZE, false},
	};
	struct yt_config config;
	size_t index;

	for (index = 0U; index < YT_ARRAY_LEN(cases); ++index) {
		struct utility_lcg lcg = {UINT32_C(0x123456)};
		struct yt_random random;
		struct yt_error error;
		struct rmt_presentation_tape tape = {
			.local_mode = true,
			.fail = true,
			.fail_site = cases[index].site,
		};
		struct yt_rmt_presenter presenter = {
			.context = &tape,
			.write = rmt_presentation_collect,
		};
		uint8_t *database = NULL;
		size_t length = 0U;
		bool ok;

		rmt_small_config(&config);
		if (!write_file("YTDATA.DAT", sentinel, sizeof(sentinel) - 1U))
			return false;
		yt_error_clear(&error);
		yt_random_init(&random);
		yt_random_set_provider(&random, utility_lcg_fill, &lcg);
		yt_platform_set_clock_provider(utility_fixed_clock, NULL);
		ok = yt_initialize_rmt_presented(&config, "The Sysop", &random,
		    &presenter, &error);
		yt_platform_set_clock_provider(NULL, NULL);
		if (ok || error.status != YT_IO_ERROR
		    || random.draws != cases[index].draws
		    || !read_file("YTDATA.DAT", &database, &length)
		    || length != cases[index].file_length
		    || (cases[index].preserves_old
		    && memcmp(database, sentinel, sizeof(sentinel) - 1U) != 0)) {
			fprintf(stderr, "RMT presentation cut %04x: ok=%d status=%d draws=%zu length=%zu\n",
			    cases[index].site, ok, error.status, random.draws, length);
			free(database);
			return false;
		}
		free(database);
	}
	return true;
}

static bool
test_rmt_dynamic_presentation(void)
{
	static const struct {
		uint32_t seed;
		size_t draws;
		uint32_t final_state;
		size_t calls;
		size_t local_length;
		uint64_t local_hash;
		size_t serial_length;
		uint64_t serial_hash;
		size_t long_links;
		size_t wraps;
		size_t repairs;
	} cases[] = {
		{UINT32_C(0x286), 244U, UINT32_C(0x9eff1a), 62U, 1350U,
		    UINT64_C(0xf0d1593bad8f2f96), 1409U,
		    UINT64_C(0xd32abf24cbe56624), 1U, 0U, 1U},
		{UINT32_C(0x7485), 293U, UINT32_C(0xd3c458), 64U, 1321U,
		    UINT64_C(0x23714298d909f079), 1380U,
		    UINT64_C(0x16d71f518f97cfa2), 4U, 1U, 0U},
	};

	for (size_t index = 0U; index < YT_ARRAY_LEN(cases); ++index) {
		struct utility_lcg lcg = {cases[index].seed};
		struct yt_random random;
		struct yt_config config;
		struct yt_error error;
		struct rmt_presentation_tape tape = {.local_mode = false};
		struct yt_rmt_presenter presenter = {
			.context = &tape,
			.write = rmt_presentation_collect,
		};
		bool ok;

		rmt_small_config(&config);
		config.local_screen = 0.0f;
		yt_error_clear(&error);
		yt_random_init(&random);
		yt_random_set_provider(&random, utility_lcg_fill, &lcg);
		yt_platform_set_clock_provider(utility_fixed_clock, NULL);
		ok = yt_initialize_rmt_presented(&config, "The Sysop", &random,
		    &presenter, &error);
		yt_platform_set_clock_provider(NULL, NULL);
		if (!ok || random.draws != cases[index].draws
		    || lcg.state != cases[index].final_state
		    || tape.calls != cases[index].calls
		    || tape.local_length != cases[index].local_length
		    || utility_fnv1a64(tape.local, tape.local_length)
		    != cases[index].local_hash
		    || tape.serial_length != cases[index].serial_length
		    || utility_fnv1a64(tape.serial, tape.serial_length)
		    != cases[index].serial_hash
		    || rmt_presentation_site_count(&tape, 0x10f1U)
		    != cases[index].long_links
		    || rmt_presentation_site_count(&tape, 0x1113U)
		    != cases[index].wraps
		    || rmt_presentation_site_count(&tape, 0x2f48U)
		    != cases[index].repairs
		    || rmt_presentation_site_count(&tape, 0x302cU)
		    != cases[index].repairs) {
			fprintf(stderr, "RMT dynamic presentation seed=%06x: ok=%d status=%d draws=%zu state=%06x calls=%zu local=%zu/%016llx serial=%zu/%016llx\n",
			    cases[index].seed, ok, error.status, random.draws,
			    lcg.state, tape.calls, tape.local_length,
			    (unsigned long long)utility_fnv1a64(tape.local,
			    tape.local_length), tape.serial_length,
			    (unsigned long long)utility_fnv1a64(tape.serial,
			    tape.serial_length));
			return false;
		}
	}
	{
		struct utility_lcg lcg = {UINT32_C(0x123456)};
		struct yt_random random;
		struct yt_config config;
		struct yt_error error;
		struct rmt_presentation_tape tape = {.local_mode = true};
		struct yt_rmt_presenter presenter = {
			.context = &tape,
			.write = rmt_presentation_collect,
		};
		bool ok;

		rmt_small_config(&config);
		config.port_offset = 103.0f;
		config.planet_offset = 133.0f;
		config.total_records = 135.0f;
		yt_error_clear(&error);
		yt_random_init(&random);
		yt_random_set_provider(&random, utility_lcg_fill, &lcg);
		yt_platform_set_clock_provider(utility_fixed_clock, NULL);
		ok = yt_initialize_rmt_presented(&config, "The Sysop", &random,
		    &presenter, &error);
		yt_platform_set_clock_provider(NULL, NULL);
		if (!ok || random.draws != 1288U
		    || lcg.state != UINT32_C(0x5473de)
		    || tape.calls != 68U || tape.local_length != 1386U
		    || utility_fnv1a64(tape.local, tape.local_length)
		    != UINT64_C(0xa45455d8e52700d9)
		    || tape.serial_length != 0U
		    || rmt_presentation_site_count(&tape, 0x13e9U) != 2U
		    || rmt_presentation_site_count(&tape, 0x1c46U) != 2U) {
			fprintf(stderr, "RMT progress presentation: ok=%d status=%d draws=%zu state=%06x calls=%zu local=%zu/%016llx sector-dots=%zu port-dots=%zu\n",
			    ok, error.status, random.draws, lcg.state, tape.calls,
			    tape.local_length,
			    (unsigned long long)utility_fnv1a64(tape.local,
			    tape.local_length),
			    rmt_presentation_site_count(&tape, 0x13e9U),
			    rmt_presentation_site_count(&tape, 0x1c46U));
			return false;
		}
	}
	return true;
}

static bool
test_yt_clock_boundaries(void)
{
	struct utility_lcg lcg = {UINT32_C(0x89b405)};
	struct utility_clock_sequence sequence = {0U};
	struct yt_random random;
	struct yt_initializer_preparation preparation;
	struct yt_config config;
	struct yt_error error;
	struct yt_record config_record;
	struct yt_record port_record;
	uint8_t *database = NULL;
	size_t length = 0U;
	size_t port_offset;
	bool ok;

	yt_error_clear(&error);
	yt_random_init(&random);
	yt_random_set_provider(&random, utility_lcg_fill, &lcg);
	yt_platform_set_clock_provider(utility_initializer_clock_sequence,
	    &sequence);
	ok = yt_initialize_begin_yt(&error)
	    && yt_initializer_prepare_yt(&random, &preparation, &error)
	    && yt_initialize_yt_prepared(&preparation, "YTSCORE.ASC", &random,
	    NULL, &error);
	yt_platform_set_clock_provider(NULL, NULL);
	if (!ok || sequence.calls != 17U
	    || !read_file("YTDATA.DAT", &database, &length)
	    || length < YT_RECORD_SIZE) {
		free(database);
		return false;
	}
	memcpy(config_record.bytes, database, YT_RECORD_SIZE);
	if (!yt_config_decode(&config, &config_record, &error)) {
		free(database);
		return false;
	}
	port_offset = ((size_t)yt_port_basic_record(&config, 1) - 1U)
	    * YT_RECORD_SIZE;
	if (length < port_offset + YT_RECORD_SIZE) {
		free(database);
		return false;
	}
	memcpy(port_record.bytes, database + port_offset, YT_RECORD_SIZE);
	free(database);
	return yt_record_get_number(&config_record, YT_F45) == 26.0f
	    && yt_record_get_number(&config_record, YT_F81) == 1.0f
	    && yt_record_get_number(&port_record, YT_F45) == 0.0f;
}

static bool
test_rmt_clock_boundaries(void)
{
	struct utility_lcg lcg = {UINT32_C(0x123456)};
	struct utility_clock_sequence sequence = {0U};
	struct yt_random random;
	struct yt_config config;
	struct yt_error error;
	struct yt_record config_record;
	struct yt_record port_record;
	uint8_t *database = NULL;
	size_t length = 0U;
	size_t port_offset;
	bool ok;

	rmt_small_config(&config);
	yt_error_clear(&error);
	yt_random_init(&random);
	yt_random_set_provider(&random, utility_lcg_fill, &lcg);
	yt_platform_set_clock_provider(utility_initializer_clock_sequence,
	    &sequence);
	ok = yt_initialize_rmt(&config, "The Sysop", &random, &error);
	yt_platform_set_clock_provider(NULL, NULL);
	port_offset = ((size_t)yt_port_basic_record(&config, 1) - 1U)
	    * YT_RECORD_SIZE;
	if (!ok || sequence.calls != 17U
	    || !read_file("YTDATA.DAT", &database, &length)
	    || length < port_offset + YT_RECORD_SIZE) {
		free(database);
		return false;
	}
	memcpy(config_record.bytes, database, YT_RECORD_SIZE);
	memcpy(port_record.bytes, database + port_offset, YT_RECORD_SIZE);
	free(database);
	return yt_record_get_number(&config_record, YT_F45) == 26.0f
	    && yt_record_get_number(&config_record, YT_F81) == 1.0f
	    && yt_record_get_number(&port_record, YT_F45) == 0.0f
	    && !yt_initialize_rmt_presented(NULL, "The Sysop", &random, NULL,
	    &error);
}

static bool
write_file(const char *path, const void *data, size_t length)
{
	FILE *file = fopen(path, "wb");

	if (file == NULL)
		return false;
	if (length > 0 && fwrite(data, 1, length, file) != length) {
		(void)fclose(file);
		return false;
	}
	return fclose(file) == 0;
}

static bool
file_size_is(const char *path, long expected)
{
	FILE *file = fopen(path, "rb");
	long length;

	if (file == NULL)
		return false;
	if (fseek(file, 0, SEEK_END) != 0
	    || (length = ftell(file)) < 0
	    || fclose(file) != 0)
		return false;
	return length == expected;
}

static bool
file_missing(const char *path)
{
	FILE *file = fopen(path, "rb");

	if (file == NULL)
		return true;
	(void)fclose(file);
	return false;
}

static bool
test_name_input_grammar(struct yt_error *error)
{
	enum { LONG_FIELD_LENGTH = 300 };
	static const uint8_t stream[] =
	    "A,B,C\r\nD,E,F,G,H\r\n\x1a";
	static const uint8_t duplicates[] =
	    "Jane,Doe,First,Alias\r\n"
	    "Other,Player,Wrong,Person\r\n"
	    "Jane,Doe,Last,Winner\r\n"
	    "A B,C,Split Alias,Tail\r\n\x1a";
	static const uint8_t quoted[] =
	    "\"Real, First\",Last,Alias,Name\r\n\x1a";
	static const uint8_t multiline[] =
	    "\"A\r\nB\",C,D,E\r\n\x1a";
	static const uint8_t double_quote[] =
	    "\"A\"\"B\",C,D\r\n\x1a";
	static const uint8_t nul_quote[] =
	    {0, '"', 'A', ',', 'B', '"', ',', 'C', ',', 'D', '\r', '\n', 0x1a};
	static const uint8_t lf_cr[] =
	    "A\n\rB,C,D,E\r\n\x1a";
	static const uint8_t closed_eof[] = "A,B,C,\"D\"\x1a";
	static const uint8_t lf_eof[] = "A,B,C,\n\x1a";
	static const uint8_t incomplete[] = "A,B,C\x1a";
	static const uint8_t staged_after_row[] =
	    "A,B,C,D\r\nE,F\x1a";
	static const uint8_t long_suffix[] = ",B,C,D\r\n\x1a";
	uint8_t long_stream[LONG_FIELD_LENGTH + sizeof(long_suffix) - 1U];
	struct yt_name_file names;
	struct yt_name_input_observation observation;
	bool ok = true;

#define LOAD_NAMES(bytes) (write_file("names.in", (bytes), sizeof(bytes) - 1U) \
	&& yt_names_load("names.in", &names, error))
#define REQUIRE_NAMES(expr, stage) do { \
	if (!(expr)) { \
		fprintf(stderr, "YTNAME grammar stage %s failed: status=%d op=%s\n", \
		    (stage), (int)error->status, error->operation); \
		return false; \
	} \
} while (0)
	REQUIRE_NAMES(LOAD_NAMES(stream), "cross-row load");
	ok = names.count == 2
	    && strcmp(names.rows[0].real_first, "A") == 0
	    && strcmp(names.rows[0].real_last, "B") == 0
	    && strcmp(names.rows[0].alias_first, "C") == 0
	    && strcmp(names.rows[0].alias_last, "D") == 0
	    && strcmp(names.rows[1].real_first, "E") == 0
	    && strcmp(names.rows[1].real_last, "F") == 0
	    && strcmp(names.rows[1].alias_first, "G") == 0
	    && strcmp(names.rows[1].alias_last, "H") == 0
	    && yt_names_find_real_last(&names, "a", "B") == NULL
	    && !yt_names_alias_exists(&names, "c", "D");
	yt_names_free(&names);
	REQUIRE_NAMES(ok, "cross-row values");
	REQUIRE_NAMES(LOAD_NAMES(duplicates), "duplicate real-name load");
	{
		const struct yt_name_row *match =
		    yt_names_find_real_last(&names, "Jane", "Doe");

		ok = names.count == 4U && match == &names.rows[2]
		    && strcmp(match->alias_first, "Last") == 0
		    && strcmp(match->alias_last, "Winner") == 0
		    && yt_names_find_real_last(&names, "A", "B C")
		    == &names.rows[3]
		    && yt_names_alias_exists(&names, "Split", "Alias Tail")
		    && !yt_names_alias_exists(&names, "split", "Alias Tail");
	}
	yt_names_free(&names);
	REQUIRE_NAMES(ok, "last duplicate real-name match");
	REQUIRE_NAMES(LOAD_NAMES(quoted), "quoted load");
	ok = names.count == 1
	    && strcmp(names.rows[0].real_first, "Real, First") == 0;
	yt_names_free(&names);
	REQUIRE_NAMES(ok, "quoted values");
	REQUIRE_NAMES(LOAD_NAMES(multiline), "multiline load");
	ok = names.count == 1
	    && strcmp(names.rows[0].real_first, "A\r\nB") == 0;
	yt_names_free(&names);
	REQUIRE_NAMES(ok, "multiline values");
	REQUIRE_NAMES(LOAD_NAMES(double_quote), "double quote load");
	ok = names.count == 1
	    && strcmp(names.rows[0].real_first, "A") == 0
	    && strcmp(names.rows[0].real_last, "B") == 0;
	yt_names_free(&names);
	REQUIRE_NAMES(ok, "double quote values");
	REQUIRE_NAMES(write_file("names.in", nul_quote, sizeof(nul_quote))
	    && yt_names_load("names.in", &names, error), "NUL quote load");
	ok = names.count == 1
	    && strcmp(names.rows[0].real_first, "A") == 0
	    && strcmp(names.rows[0].real_last, "B\"") == 0;
	yt_names_free(&names);
	REQUIRE_NAMES(ok, "NUL quote values");
	REQUIRE_NAMES(LOAD_NAMES(lf_cr), "LF-CR load");
	ok = names.count == 1
	    && strcmp(names.rows[0].real_first, "AB") == 0;
	yt_names_free(&names);
	REQUIRE_NAMES(ok, "LF-CR values");
	REQUIRE_NAMES(LOAD_NAMES(closed_eof), "closed EOF load");
	ok = names.count == 1 && strcmp(names.rows[0].alias_last, "D") == 0;
	yt_names_free(&names);
	REQUIRE_NAMES(ok, "closed EOF values");
	REQUIRE_NAMES(LOAD_NAMES(lf_eof), "LF EOF load");
	ok = names.count == 1
	    && (unsigned char)names.rows[0].alias_last[0] == 0x1a
	    && names.rows[0].alias_last[1] == '\0';
	yt_names_free(&names);
	REQUIRE_NAMES(ok, "LF EOF values");
	REQUIRE_NAMES(write_file("names.in", incomplete,
	    sizeof(incomplete) - 1U), "incomplete write");
	yt_error_clear(error);
	ok = !yt_names_load("names.in", &names, error)
	    && error->status == YT_EOF && names.rows == NULL && names.count == 0;
	REQUIRE_NAMES(ok, "incomplete group");
	yt_error_clear(error);
	ok = !yt_names_parse_input_groups(staged_after_row,
	    sizeof(staged_after_row) - 1U, &names, &observation, error)
	    && error->status == YT_EOF && names.count == 1U
	    && strcmp(names.rows[0].real_first, "A") == 0
	    && strcmp(names.rows[0].alias_last, "D") == 0
	    && observation.staged_count == 2U
	    && strcmp(observation.staged.real_first, "E") == 0
	    && strcmp(observation.staged.real_last, "F") == 0
	    && observation.staged.alias_first == NULL
	    && observation.staged.alias_last == NULL;
	yt_names_input_observation_free(&observation);
	yt_names_free(&names);
	REQUIRE_NAMES(ok, "staged incomplete group");
	memset(long_stream, 'A', LONG_FIELD_LENGTH);
	memcpy(long_stream + LONG_FIELD_LENGTH, long_suffix,
	    sizeof(long_suffix) - 1U);
	REQUIRE_NAMES(write_file("names.in", long_stream,
	    sizeof(long_stream)) && yt_names_load("names.in", &names, error),
	    "dynamic-field load");
	ok = names.count == 1U
	    && strlen(names.rows[0].real_first) == LONG_FIELD_LENGTH
	    && memcmp(names.rows[0].real_first, long_stream,
	    LONG_FIELD_LENGTH) == 0
	    && strcmp(names.rows[0].real_last, "B") == 0
	    && yt_names_write("names.out", &names, error);
	yt_names_free(&names);
	REQUIRE_NAMES(ok, "dynamic-field value/write");
	{
		struct yt_text_file written = {0};

		ok = yt_text_read("names.out", &written, error)
		    && written.length == sizeof(long_stream)
		    && memcmp(written.data, long_stream, sizeof(long_stream)) == 0;
		yt_text_free(&written);
	}
#undef LOAD_NAMES
#undef REQUIRE_NAMES
	return ok;
}

struct names_read_failure_script {
	size_t calls;
	bool fail_after_one_byte;
};

static bool
names_read_failure_provider(void *context, FILE *file, uint8_t *data,
    size_t requested, struct yt_text_input_read_observation *observation)
{
	struct names_read_failure_script *script = context;
	size_t call = script->calls++;

	(void)file;
	memset(observation, 0, sizeof(*observation));
	observation->terminal_position = 1;
	if (script->fail_after_one_byte && call == 0U) {
		if (requested == 0U)
			return false;
		data[0] = 'A';
		observation->accepted = 1U;
		return true;
	}
	observation->carry = true;
	observation->dos_error = 5U;
	observation->basic_error = 70U;
	return true;
}

static bool
names_close_failure_provider(void *context, FILE *file,
    enum yt_text_close_operation operation, const uint8_t *data,
    size_t requested, struct yt_text_close_observation *observation)
{
	(void)context;
	(void)file;
	(void)data;
	(void)requested;
	memset(observation, 0, sizeof(*observation));
	observation->terminal_position = 0;
	if (operation == YT_TEXT_CLOSE_HANDLE) {
		observation->carry = true;
		observation->dos_error = 5U;
		observation->handle_open = true;
		return true;
	}
	return operation == YT_TEXT_CLOSE_CLEANUP_HANDLE;
}

static bool
test_name_sequential_transaction(struct yt_error *error)
{
	static const uint8_t rows[] =
	    "A,B,C,D\r\nE,F,G,H\r\n\x1a";
	static const uint8_t empty_row[] = ",,,\r\n\x1a";
	static const uint8_t incomplete[] =
	    "A,B,C,D\r\nE,F\x1a";
	struct yt_names_sequential_state state;
	struct yt_name_input_observation observation;
	struct names_read_failure_script read_script;
	struct yt_text_input input;
	struct yt_name_file names;
	bool result;

	if (!write_file("names.in", rows, sizeof(rows) - 1U))
		return false;
	yt_text_input_init(&input);
	result = yt_names_load_sequential(&input, "names.in", &names,
	    &observation, &state, error);
	if (!result || state.failed_operation != YT_NAMES_SEQUENTIAL_NONE
	    || !state.file_opened || state.eof_checks != 3U
	    || state.token_reads != 8U || state.rows_committed != 2U
	    || !state.close_attempted || !state.file_closed || !state.complete
	    || input.file != NULL || names.count != 2U
	    || observation.staged_count != 0U || observation.cursor != 18U
	    || strcmp(names.rows[1].alias_last, "H") != 0) {
		yt_names_input_observation_free(&observation);
		yt_names_free(&names);
		yt_text_input_destroy(&input);
		return false;
	}
	yt_names_input_observation_free(&observation);
	yt_names_free(&names);
	yt_text_input_destroy(&input);

	if (!write_file("names.in", empty_row, sizeof(empty_row) - 1U))
		return false;
	yt_text_input_init(&input);
	result = yt_names_load_sequential(&input, "names.in", &names,
	    &observation, &state, error);
	if (!result || state.eof_checks != 2U || state.token_reads != 4U
	    || state.rows_committed != 1U || !state.complete
	    || observation.cursor != 5U || names.count != 1U
	    || names.rows[0].real_first == NULL
	    || names.rows[0].real_first[0] != '\0'
	    || names.rows[0].real_last[0] != '\0'
	    || names.rows[0].alias_first[0] != '\0'
	    || names.rows[0].alias_last[0] != '\0') {
		yt_names_input_observation_free(&observation);
		yt_names_free(&names);
		yt_text_input_destroy(&input);
		return false;
	}
	yt_names_input_observation_free(&observation);
	yt_names_free(&names);
	yt_text_input_destroy(&input);

	if (!write_file("names.in", incomplete, sizeof(incomplete) - 1U))
		return false;
	yt_error_clear(error);
	yt_text_input_init(&input);
	result = yt_names_load_sequential(&input, "names.in", &names,
	    &observation, &state, error);
	if (result || error->status != YT_EOF
	    || state.failed_operation != YT_NAMES_SEQUENTIAL_TOKEN
	    || !state.file_opened || state.eof_checks != 2U
	    || state.token_reads != 7U || state.rows_committed != 1U
	    || state.close_attempted || state.complete || input.file == NULL
	    || names.count != 1U || observation.staged_count != 2U
	    || strcmp(observation.staged.real_first, "E") != 0
	    || strcmp(observation.staged.real_last, "F") != 0
	    || observation.staged.alias_first != NULL
	    || observation.cursor != 12U) {
		yt_names_input_observation_free(&observation);
		yt_names_free(&names);
		yt_text_input_destroy(&input);
		return false;
	}
	yt_names_input_observation_free(&observation);
	yt_names_free(&names);
	yt_text_input_destroy(&input);

	read_script = (struct names_read_failure_script){0};
	yt_text_input_init(&input);
	yt_text_input_set_read_provider(&input, names_read_failure_provider,
	    &read_script);
	result = yt_names_load_sequential(&input, "names.in", &names,
	    &observation, &state, error);
	if (result || state.failed_operation != YT_NAMES_SEQUENTIAL_EOF
	    || state.eof_checks != 1U || state.token_reads != 0U
	    || state.rows_committed != 0U || input.last_read.dos_error != 5U) {
		yt_names_input_observation_free(&observation);
		yt_names_free(&names);
		yt_text_input_destroy(&input);
		return false;
	}
	yt_names_input_observation_free(&observation);
	yt_names_free(&names);
	yt_text_input_destroy(&input);

	read_script = (struct names_read_failure_script){
		.fail_after_one_byte = true,
	};
	yt_text_input_init(&input);
	yt_text_input_set_read_provider(&input, names_read_failure_provider,
	    &read_script);
	result = yt_names_load_sequential(&input, "names.in", &names,
	    &observation, &state, error);
	if (result || state.failed_operation != YT_NAMES_SEQUENTIAL_TOKEN
	    || state.eof_checks != 1U || state.token_reads != 1U
	    || state.rows_committed != 0U || observation.staged_count != 0U
	    || input.logical_position != 1U || input.last_read.dos_error != 5U) {
		yt_names_input_observation_free(&observation);
		yt_names_free(&names);
		yt_text_input_destroy(&input);
		return false;
	}
	yt_names_input_observation_free(&observation);
	yt_names_free(&names);
	yt_text_input_destroy(&input);

	if (!write_file("names.in", rows, sizeof(rows) - 1U))
		return false;
	yt_text_input_init(&input);
	yt_text_input_set_close_provider(&input, names_close_failure_provider,
	    NULL);
	result = yt_names_load_sequential(&input, "names.in", &names,
	    &observation, &state, error);
	if (result || state.failed_operation != YT_NAMES_SEQUENTIAL_CLOSE
	    || state.rows_committed != 2U || !state.close_attempted
	    || state.file_closed || state.complete || names.count != 2U
	    || input.last_close.basic_error != 70U
	    || !input.last_close.cleanup_close_attempted) {
		yt_names_input_observation_free(&observation);
		yt_names_free(&names);
		yt_text_input_destroy(&input);
		return false;
	}
	yt_names_input_observation_free(&observation);
	yt_names_free(&names);
	yt_text_input_destroy(&input);

	yt_text_input_init(&input);
	result = yt_names_load_sequential(&input, "missing/NAME.DAT", &names,
	    &observation, &state, error);
	yt_text_input_destroy(&input);
	return !result && state.failed_operation == YT_NAMES_SEQUENTIAL_OPEN
	    && !state.file_opened && state.eof_checks == 0U
	    && !yt_names_load_sequential(NULL, "names.in", &names,
	    &observation, &state, error)
	    && !yt_names_load_sequential(&input, NULL, &names,
	    &observation, &state, error)
	    && !yt_names_load_sequential(&input, "names.in", NULL,
	    &observation, &state, error)
	    && !yt_names_load_sequential(&input, "names.in", &names,
	    &observation, NULL, error);
}

static bool
names_output_write_failure_provider(void *context, FILE *file,
    const uint8_t *data, size_t requested,
    struct yt_text_output_write_observation *observation)
{
	(void)context;
	(void)file;
	(void)data;
	memset(observation, 0, sizeof(*observation));
	observation->carry = true;
	observation->handle_open = true;
	observation->dos_error = 5U;
	observation->terminal_position = requested == 0U ? 0 : 3;
	return true;
}

static bool
names_output_close_failure_provider(void *context, FILE *file,
    enum yt_text_close_operation operation, const uint8_t *data,
    size_t requested, struct yt_text_close_observation *observation)
{
	(void)context;
	(void)data;
	memset(observation, 0, sizeof(*observation));
	observation->accepted = requested;
	observation->handle_open = true;
	observation->terminal_position = 0;
	if (operation == YT_TEXT_CLOSE_HANDLE) {
		observation->carry = true;
		observation->dos_error = 5U;
	}
	else if (operation == YT_TEXT_CLOSE_CLEANUP_HANDLE) {
		if (file == NULL || fclose(file) != 0)
			return false;
		observation->handle_open = false;
	}
	return true;
}

static bool
test_name_sequential_output_transaction(struct yt_error *error)
{
	static const struct yt_name_row rows[] = {
		{.real_first = "A", .real_last = "B", .alias_first = "C",
		    .alias_last = "D"},
		{.real_first = "E", .real_last = "F", .alias_first = "G",
		    .alias_last = "H"},
	};
	static const uint8_t expected[] =
	    "A,B,C,D\r\nE,F,G,H\r\n\x1a";
	static const uint8_t stale[] =
	    "A STALE OUTPUT TAIL THAT MUST BE TRUNCATED\r\n\x1a";
	struct yt_names_output_state state;
	struct yt_text_output output;
	struct yt_text_file text = {0};
	struct yt_name_file names = {
		.rows = (struct yt_name_row *)rows,
		.count = YT_ARRAY_LEN(rows),
	};
	struct yt_name_row long_row = {
		.real_last = "B",
		.alias_first = "C",
		.alias_last = "D",
	};
	struct yt_name_file long_names = {.rows = &long_row, .count = 1U};
	char long_first[129];
	bool result;

	if (!write_file("names.out", stale, sizeof(stale) - 1U))
		return false;
	yt_text_output_init(&output);
	result = yt_names_write_sequential(&output, "names.out", &names,
	    &state, error);
	if (!result || !state.complete
	    || state.attempted != YT_NAMES_OUTPUT_NONE
	    || !state.file_opened || !state.close_attempted || !state.file_closed
	    || state.row_index != 2U || state.rows_completed != 2U
	    || state.values_completed != 14U || output.file != NULL
	    || !yt_text_read("names.out", &text, error)
	    || text.length != sizeof(expected) - 1U
	    || memcmp(text.data, expected, sizeof(expected) - 1U) != 0) {
		yt_text_free(&text);
		yt_text_output_destroy(&output);
		return false;
	}
	yt_text_free(&text);
	yt_text_output_destroy(&output);

	memset(long_first, 'X', sizeof(long_first) - 1U);
	long_first[sizeof(long_first) - 1U] = '\0';
	long_row.real_first = long_first;
	yt_text_output_init(&output);
	yt_text_output_set_write_provider(&output,
	    names_output_write_failure_provider, NULL);
	result = yt_names_write_sequential(&output, "names.out", &long_names,
	    &state, error);
	if (result || state.attempted != YT_NAMES_OUTPUT_COMMA_1
	    || !state.file_opened || state.close_attempted || state.complete
	    || state.row_index != 0U || state.rows_completed != 0U
	    || state.values_completed != 1U
	    || output.last_write.outcome != YT_TEXT_OUTPUT_WRITE_DISK_ERROR
	    || output.last_write.dos_error != 5U
	    || !output.last_write.cleanup_close_attempted) {
		yt_text_output_destroy(&output);
		return false;
	}
	yt_text_output_destroy(&output);

	yt_text_output_init(&output);
	yt_text_output_set_close_provider(&output,
	    names_output_close_failure_provider, NULL);
	result = yt_names_write_sequential(&output, "names.out", &long_names,
	    &state, error);
	if (result || state.attempted != YT_NAMES_OUTPUT_CLOSE
	    || state.rows_completed != 1U || state.values_completed != 7U
	    || !state.close_attempted || state.file_closed || state.complete
	    || output.last_close.outcome != YT_TEXT_CLOSE_DISK_ERROR
	    || output.last_close.failed_operation != YT_TEXT_CLOSE_HANDLE
	    || output.last_close.basic_error != 70U
	    || !output.last_close.cleanup_close_attempted) {
		yt_text_output_destroy(&output);
		return false;
	}
	yt_text_output_destroy(&output);

	yt_text_output_init(&output);
	result = yt_names_write_sequential(&output, "missing/NAME.DAT", &names,
	    &state, error);
	yt_text_output_destroy(&output);
	return !result && state.attempted == YT_NAMES_OUTPUT_OPEN
	    && !state.file_opened
	    && !yt_names_write_sequential(NULL, "names.out", &names, &state,
	    error)
	    && !yt_names_write_sequential(&output, NULL, &names, &state, error)
	    && !yt_names_write_sequential(&output, "names.out", NULL, &state,
	    error)
	    && !yt_names_write_sequential(&output, "names.out", &names, NULL,
	    error);
}

struct alias_propagate_test_event {
	enum yt_alias_propagate_operation operation;
	int basic_record;
};

struct alias_propagate_test_tape {
	struct yt_record source[52];
	struct yt_record durable[52];
	struct alias_propagate_test_event events[104];
	size_t event_count;
	size_t fail_at;
};

static void
alias_propagate_test_init(struct alias_propagate_test_tape *tape)
{
	int basic;

	memset(tape, 0, sizeof(*tape));
	for (basic = 2; basic <= 51; ++basic) {
		size_t byte;

		for (byte = 0U; byte < YT_RECORD_SIZE; ++byte)
			tape->source[basic].bytes[byte] =
			    (uint8_t)(basic * 7 + (int)byte);
		memset(tape->source[basic].bytes, ' ', YT_TEXT_FIELD_SIZE);
	}
	memcpy(tape->source[2].bytes, "Old Alias", 9U);
	memcpy(tape->source[4].bytes + 7U, "Old Alias", 9U);
	memcpy(tape->source[5].bytes, "old Alias", 9U);
	memcpy(tape->source[51].bytes, "Last Record", 11U);
	memcpy(tape->durable, tape->source, sizeof(tape->durable));
}

static bool
alias_propagate_test_event(struct alias_propagate_test_tape *tape,
    enum yt_alias_propagate_operation operation, int basic_record,
    struct yt_error *error)
{
	if (tape->event_count >= YT_ARRAY_LEN(tape->events))
		return false;
	tape->events[tape->event_count++] =
	    (struct alias_propagate_test_event){operation, basic_record};
	if (tape->fail_at != 0U && tape->event_count == tape->fail_at) {
		if (error != NULL)
			error->status = YT_IO_ERROR;
		return false;
	}
	return true;
}

static bool
alias_propagate_test_read(void *context, int basic_record,
    struct yt_record *record, struct yt_error *error)
{
	struct alias_propagate_test_tape *tape = context;

	if (!alias_propagate_test_event(tape, YT_ALIAS_PROPAGATE_READ_PLAYER,
	    basic_record, error))
		return false;
	*record = tape->source[basic_record];
	return true;
}

static bool
alias_propagate_test_write(void *context, int basic_record,
    const struct yt_record *record, struct yt_error *error)
{
	struct alias_propagate_test_tape *tape = context;

	if (!alias_propagate_test_event(tape, YT_ALIAS_PROPAGATE_WRITE_PLAYER,
	    basic_record, error))
		return false;
	tape->durable[basic_record] = *record;
	return true;
}

static bool
test_alias_propagation_transaction(void)
{
	static const struct yt_alias_propagate_ops ops = {
		alias_propagate_test_read,
		alias_propagate_test_write,
	};
	static const uint8_t old_alias[] = "Old Alias";
	static const uint8_t new_alias[] = "New Name";
	struct alias_propagate_test_tape success;
	struct yt_alias_propagate_state state;
	struct yt_error error;
	uint8_t expected_name[YT_TEXT_FIELD_SIZE];
	uint8_t expected_length[4];
	size_t failure;
	bool result;

	alias_propagate_test_init(&success);
	yt_error_clear(&error);
	result = yt_names_propagate_alias(&state, old_alias,
	    sizeof(old_alias) - 1U, new_alias, sizeof(new_alias) - 1U,
	    &ops, &success, &error);
	memset(expected_name, ' ', sizeof(expected_name));
	memcpy(expected_name, new_alias, sizeof(new_alias) - 1U);
	if (qb_mbf32_encode((float)(sizeof(new_alias) - 1U), expected_length)
	    == QB_MBF_OVERFLOW
	    || !result || !state.complete
	    || state.attempted != YT_ALIAS_PROPAGATE_NONE
	    || state.basic_record != 51 || state.records_read != 50U
	    || state.matches != 2U || state.records_written != 2U
	    || !state.field_loaded || state.name_overlaid
	    || state.length_overlaid || success.event_count != 52U
	    || memcmp(&state.field, &success.source[51], sizeof(state.field)) != 0
	    || memcmp(success.durable[2].bytes, expected_name,
	    sizeof(expected_name)) != 0
	    || memcmp(success.durable[4].bytes, expected_name,
	    sizeof(expected_name)) != 0
	    || memcmp(success.durable[2].bytes + YT_F85, expected_length, 4U) != 0
	    || memcmp(success.durable[4].bytes + YT_F85, expected_length, 4U) != 0
	    || memcmp(success.durable[5].bytes, success.source[5].bytes,
	    YT_RECORD_SIZE) != 0)
		return false;
	for (failure = 1U; failure <= success.event_count; ++failure) {
		struct alias_propagate_test_tape tape;
		bool written[52] = {false};
		size_t event;
		size_t writes = 0U;
		int basic;

		alias_propagate_test_init(&tape);
		tape.fail_at = failure;
		yt_error_clear(&error);
		result = yt_names_propagate_alias(&state, old_alias,
		    sizeof(old_alias) - 1U, new_alias, sizeof(new_alias) - 1U,
		    &ops, &tape, &error);
		for (event = 0U; event + 1U < failure; ++event) {
			if (success.events[event].operation ==
			    YT_ALIAS_PROPAGATE_WRITE_PLAYER) {
				++writes;
				written[success.events[event].basic_record] = true;
			}
		}
		if (result || error.status != YT_IO_ERROR || state.complete
		    || tape.event_count != failure || state.records_written != writes
		    || state.attempted != success.events[failure - 1U].operation
		    || state.basic_record != success.events[failure - 1U].basic_record
		    || memcmp(tape.events, success.events,
		    failure * sizeof(tape.events[0])) != 0)
			return false;
		for (basic = 2; basic <= 51; ++basic) {
			const struct yt_record *expected = written[basic]
			    ? &success.durable[basic] : &success.source[basic];

			if (memcmp(&tape.durable[basic], expected,
			    sizeof(*expected)) != 0)
				return false;
		}
	}
	return !yt_names_propagate_alias(NULL, old_alias,
	    sizeof(old_alias) - 1U, new_alias, sizeof(new_alias) - 1U,
	    &ops, &success, &error)
	    && !yt_names_propagate_alias(&state, NULL, 1U, new_alias,
	    sizeof(new_alias) - 1U, &ops, &success, &error)
	    && !yt_names_propagate_alias(&state, old_alias,
	    sizeof(old_alias) - 1U, NULL, 1U, &ops, &success, &error)
	    && !yt_names_propagate_alias(&state, old_alias,
	    sizeof(old_alias) - 1U, new_alias, YT_TEXT_FIELD_SIZE + 1U,
	    &ops, &success, &error)
	    && !yt_names_propagate_alias(&state, old_alias,
	    sizeof(old_alias) - 1U, new_alias, sizeof(new_alias) - 1U,
	    NULL, &success, &error);
}

static bool
test_alias_key_preparation(void)
{
	char alias[256];
	char first[128];
	char last[128];
	char display[258];
	char long_word[41];
	char expected[41];
	enum yt_alias_key_status status;

	alias[0] = '\0';
	status = yt_names_prepare_alias(alias, sizeof(alias), "John", "Doe",
	    first, sizeof(first), last, sizeof(last), display, sizeof(display));
	if (status != YT_ALIAS_KEY_READY || strcmp(alias, "John Doe ") != 0
	    || strcmp(first, "John") != 0 || strcmp(last, "Doe") != 0
	    || strcmp(display, "John Doe") != 0)
		return false;

	snprintf(alias, sizeof(alias), "  star,,lord\"  ");
	status = yt_names_prepare_alias(alias, sizeof(alias), "John", "Doe",
	    first, sizeof(first), last, sizeof(last), display, sizeof(display));
	if (status != YT_ALIAS_KEY_READY || strcmp(alias, "Star Lord ") != 0
	    || strcmp(first, "Star") != 0 || strcmp(last, "Lord") != 0
	    || strcmp(display, "Star Lord") != 0)
		return false;

	snprintf(alias, sizeof(alias), " , \" ");
	if (yt_names_prepare_alias(alias, sizeof(alias), "John", "Doe",
	    first, sizeof(first), last, sizeof(last), display, sizeof(display))
	    != YT_ALIAS_KEY_EMPTY)
		return false;
	snprintf(alias, sizeof(alias),
	    "Forty character prefix keeps going past clip Sysop");
	if (yt_names_prepare_alias(alias, sizeof(alias), "John", "Doe",
	    first, sizeof(first), last, sizeof(last), display, sizeof(display))
	    != YT_ALIAS_KEY_RESERVED)
		return false;

	snprintf(alias, sizeof(alias), "Solo");
	status = yt_names_prepare_alias(alias, sizeof(alias), "John", "Doe",
	    first, sizeof(first), last, sizeof(last), display, sizeof(display));
	if (status != YT_ALIAS_KEY_READY || strcmp(alias, "Solo ") != 0
	    || strcmp(first, "Solo") != 0 || last[0] != '\0'
	    || strcmp(display, "Solo ") != 0)
		return false;

	memset(long_word, 'R', 40);
	long_word[40] = '\0';
	snprintf(alias, sizeof(alias), "%s", long_word);
	memset(expected, 'r', 40);
	expected[0] = 'R';
	expected[40] = '\0';
	status = yt_names_prepare_alias(alias, sizeof(alias), "John", "Doe",
	    first, sizeof(first), last, sizeof(last), display, sizeof(display));
	if (status != YT_ALIAS_KEY_READY || strlen(alias) != 41
	    || alias[40] != ' ' || strcmp(first, expected) != 0
	    || last[0] != '\0' || strcmp(display, expected) != 0)
		return false;

	snprintf(alias, sizeof(alias), "Alias");
	return yt_names_prepare_alias(alias, 41, "John", "Doe", first,
	    sizeof(first), last, sizeof(last), display, sizeof(display))
	    == YT_ALIAS_KEY_RANGE;
}

static bool
test_name_append(struct yt_error *error)
{
	enum { LONG_ALIAS_LENGTH = 300 };
	static const struct yt_name_row first =
	    {"John", "Doe", "Star", "Lord"};
	static const struct yt_name_row second =
	    {"Jane", "Roe", "Void", "Walker"};
	static const uint8_t expected[] =
	    "John,Doe,Star,Lord\r\n"
	    "Jane,Roe,Void,Walker\r\n\x1a";
	struct yt_text_file text = {0};
	struct yt_name_file names;
	char long_alias[LONG_ALIAS_LENGTH + 1U];
	struct yt_name_row third = {"Long", "Alias", long_alias, "Tail"};
	bool valid;

	(void)remove("names.append");
	if (!yt_names_append("names.append", &first, error)
	    || !yt_names_append("names.append", &second, error)
	    || !yt_text_read("names.append", &text, error)) {
		yt_text_free(&text);
		(void)remove("names.append");
		return false;
	}
	valid = text.length == sizeof(expected) - 1U
	    && memcmp(text.data, expected, sizeof(expected) - 1U) == 0;
	yt_text_free(&text);
	memset(long_alias, 'Z', LONG_ALIAS_LENGTH);
	long_alias[LONG_ALIAS_LENGTH] = '\0';
	valid = valid && yt_names_append("names.append", &third, error)
	    && yt_names_load("names.append", &names, error);
	if (valid) {
		valid = names.count == 3U
		    && strlen(names.rows[2].alias_first) == LONG_ALIAS_LENGTH
		    && memcmp(names.rows[2].alias_first, long_alias,
		    LONG_ALIAS_LENGTH) == 0
		    && strcmp(names.rows[2].alias_last, "Tail") == 0;
		yt_names_free(&names);
	}
	(void)remove("names.append");
	(void)remove("names.out");
	return valid;
}

static bool
read_file(const char *path, uint8_t **data, size_t *length)
{
	FILE *file = fopen(path, "rb");
	long size;

	*data = NULL;
	*length = 0;
	if (file == NULL || fseek(file, 0, SEEK_END) != 0
	    || (size = ftell(file)) < 0 || fseek(file, 0, SEEK_SET) != 0) {
		if (file != NULL)
			(void)fclose(file);
		return false;
	}
	*data = malloc((size_t)size + 1U);
	if (*data == NULL) {
		(void)fclose(file);
		return false;
	}
	if (size > 0 && fread(*data, 1, (size_t)size, file) != (size_t)size) {
		free(*data);
		*data = NULL;
		(void)fclose(file);
		return false;
	}
	if (fclose(file) != 0) {
		free(*data);
		*data = NULL;
		return false;
	}
	(*data)[size] = 0;
	*length = (size_t)size;
	return true;
}

static bool
run_redirected(const char *program, const char *input, const char *output)
{
	char command[4096];
	int written = snprintf(command, sizeof(command),
	    "\"%s\" < \"%s\" > \"%s\" 2>&1", program, input, output);

	return written >= 0 && (size_t)written < sizeof(command)
	    && system(command) == 0;
}

static bool
initialize_direct(struct yt_error *error)
{
	struct yt_random random;

	yt_random_init(&random);
	return yt_initialize_yt("YTSCORE.ASC", &random, error);
}

static bool
test_expired_player_cleanup(struct yt_error *error)
{
	static const uint8_t names[] =
	    "Real,One,Victim,Pilot\r\n"
	    "Keep,Real,Other,Alias\r\n"
	    "Real,Two,Victim,Pilot\r\n";
	static const uint8_t expected_names[] =
	    "Keep,Real,Other,Alias\r\n\x1a";
	struct yt_game game;
	struct yt_player victim;
	struct yt_player other;
	struct yt_player unrelated;
	struct yt_sector team;
	struct yt_sector defense;
	struct yt_planet planet;
	struct yt_port port;
	struct yt_radio_record radio[3];
	struct yt_radio_record before[3];
	struct yt_text_file result_names = {0};
	float sector_cache[52];
	float cloak_cache[52];
	uint8_t *radio_bytes = NULL;
	size_t radio_length = 0;
	FILE *temporary;
	bool valid = false;

	memset(&game, 0, sizeof(game));
	memset(sector_cache, 0x42, sizeof(sector_cache));
	memset(cloak_cache, 0x24, sizeof(cloak_cache));
	if (!yt_game_open(&game, YT_OPEN_UPDATE, error)
	    || !yt_game_read_player(&game, 2, &victim, error)
	    || !yt_game_read_player(&game, 3, &other, error)
	    || !yt_game_read_player(&game, 4, &unrelated, error)
	    || !yt_game_read_sector(&game, 10, &team, error)
	    || !yt_game_read_sector(&game, 20, &defense, error)
	    || !yt_game_read_planet(&game, 1, &planet, error)
	    || !yt_game_read_port(&game, 1, &port, error))
		goto done;
	strcpy(victim.name, "Victim Pilot");
	victim.name_length = 12.0f;
	victim.team = 10.0f;
	victim.lottery_plays = 7.0f;
	victim.credits = 123.0f;
	victim.fighters = 45.0f;
	other.killed_by = 2.0f;
	unrelated.killed_by = -1.0f;
	yt_record_set_number(&team.record, YT_F109, 2.0f);
	yt_record_set_number(&team.record, YT_F117, 9.0f);
	yt_record_set_number(&team.record, YT_F121, 2.0f);
	yt_record_set_number(&team.record, YT_F125, -1.0f);
	defense.fighters = 12.0f;
	defense.fighter_owner = 2.0f;
	planet.owner = 2.0f;
	planet.ground_forces = 8.0f;
	planet.bank = 456.0f;
	port.owner = 2.0f;
	port.treasury = 99.0f;
	if (!yt_game_write_player(&game, 2, &victim, error)
	    || !yt_game_write_player(&game, 3, &other, error)
	    || !yt_game_write_player(&game, 4, &unrelated, error)
	    || !yt_game_write_sector(&game, 10, &team, error)
	    || !yt_game_write_sector(&game, 20, &defense, error)
	    || !yt_game_write_planet(&game, 1, &planet, error)
	    || !yt_game_write_port(&game, 1, &port, error))
		goto done;
	memset(radio, 0, sizeof(radio));
	yt_radio_set_number(&radio[0], 0, 5.0f);
	yt_radio_set_number(&radio[0], 4, 2.0f);
	yt_radio_set_number(&radio[0], 8, 9.0f);
	yt_radio_set_text(&radio[0], (const uint8_t *)"recipient", 9U, 74U);
	yt_radio_set_number(&radio[1], 0, 7.0f);
	yt_radio_set_number(&radio[1], 4, 9.0f);
	yt_radio_set_number(&radio[1], 8, 2.0f);
	yt_radio_set_text(&radio[1], (const uint8_t *)"sender", 6U, 74U);
	yt_radio_set_number(&radio[2], 0, 1.0f);
	yt_radio_set_number(&radio[2], 4, 9.0f);
	yt_radio_set_number(&radio[2], 8, 8.0f);
	yt_radio_set_text(&radio[2], (const uint8_t *)"unrelated", 9U, 74U);
	memcpy(before, radio, sizeof(before));
	sector_cache[2] = 20.0f;
	cloak_cache[2] = 0.75f;
	sector_cache[3] = 30.0f;
	cloak_cache[3] = 0.25f;
	if (!write_file("YTRMSG.DAT", radio, sizeof(radio))
	    || !yt_text_write("YTNAME.DAT", names, sizeof(names) - 1U, true,
	    error)
	    || !yt_maintenance_expire_player(&game, sector_cache, cloak_cache,
	    YT_ARRAY_LEN(sector_cache), 2, &victim, error)
	    || !yt_maintenance_remove_alias("Victim Pilot", error)
	    || !yt_game_read_player(&game, 2, &victim, error)
	    || !yt_game_read_player(&game, 3, &other, error)
	    || !yt_game_read_player(&game, 4, &unrelated, error)
	    || !yt_game_read_sector(&game, 10, &team, error)
	    || !yt_game_read_sector(&game, 20, &defense, error)
	    || !yt_game_read_planet(&game, 1, &planet, error)
	    || !yt_game_read_port(&game, 1, &port, error)
	    || !read_file("YTRMSG.DAT", &radio_bytes, &radio_length)
	    || !yt_text_read("YTNAME.DAT", &result_names, error))
		goto done;
	temporary = fopen("TEMPWORK", "rb");
	if (temporary != NULL) {
		(void)fclose(temporary);
		goto done;
	}
	valid = sector_cache[2] == 0.0f && cloak_cache[2] == 0.0f
	    && sector_cache[3] == 30.0f && cloak_cache[3] == 0.25f
	    && victim.name_length == 0.0f && victim.team == 0.0f
	    && victim.lottery_plays == 0.0f && victim.credits == 123.0f
	    && victim.fighters == 45.0f
	    && memcmp(victim.record.bytes, "Victim Pilot", 12U) == 0
	    && yt_record_get_number(&team.record, YT_F109) == 0.0f
	    && yt_record_get_number(&team.record, YT_F117) == 9.0f
	    && yt_record_get_number(&team.record, YT_F121) == 0.0f
	    && yt_record_get_number(&team.record, YT_F125) == -1.0f
	    && defense.fighters == 0.0f && defense.fighter_owner == 0.0f
	    && planet.owner == 0.0f && planet.ground_forces == 0.0f
	    && planet.bank == 456.0f
	    && port.owner == 2.0f && port.treasury == 99.0f
	    && other.killed_by == -98.0f && unrelated.killed_by == -1.0f
	    && radio_length == sizeof(radio)
	    && yt_radio_get_number((const struct yt_radio_record *)radio_bytes,
	    0) == 0.0f
	    && memcmp(radio_bytes + 4U, before[0].bytes + 4U, 82U) == 0
	    && yt_radio_get_number((const struct yt_radio_record *)(radio_bytes
	    + sizeof(struct yt_radio_record)), 0) == 0.0f
	    && memcmp(radio_bytes + sizeof(struct yt_radio_record) + 4U,
	    before[1].bytes + 4U, 82U) == 0
	    && memcmp(radio_bytes + 2U * sizeof(struct yt_radio_record),
	    before[2].bytes, sizeof(struct yt_radio_record)) == 0
	    && result_names.length == sizeof(expected_names) - 1U
	    && memcmp(result_names.data, expected_names,
	    sizeof(expected_names) - 1U) == 0;

done:
	free(radio_bytes);
	yt_text_free(&result_names);
	yt_game_close(&game);
	(void)remove("YTNAME.DAT");
	(void)remove("TEMPWORK");
	return valid;
}

static bool
test_immediate_death_cleanup(struct yt_error *error)
{
	struct yt_game game;
	struct yt_player victim;
	struct yt_sector team;
	struct yt_sector defense;
	struct yt_planet planet;
	struct yt_port owned_port;
	struct yt_port other_port;
	float sector_cache[52] = {0};
	float cloak_cache[52] = {0};
	bool valid = false;

	memset(&game, 0, sizeof(game));
	if (!yt_game_open(&game, YT_OPEN_UPDATE, error)
	    || !yt_game_read_player(&game, 2, &victim, error)
	    || !yt_game_read_sector(&game, 10, &team, error)
	    || !yt_game_read_sector(&game, 20, &defense, error)
	    || !yt_game_read_planet(&game, 1, &planet, error)
	    || !yt_game_read_port(&game, 1, &owned_port, error)
	    || !yt_game_read_port(&game, 2, &other_port, error))
		goto done;
	strcpy(victim.name, "Combat Victim");
	victim.name_length = 13.0f;
	victim.killed_by = 0.0f;
	victim.sector = 20.0f;
	victim.ground_forces = 6.0f;
	victim.team = 10.0f;
	victim.fighters = 45.0f;
	victim.credits = 123.0f;
	victim.cloak = 0.5f;
	victim.score = 789.0f;
	yt_record_set_number(&team.record, YT_F109, 2.0f);
	yt_record_set_number(&team.record, YT_F117, 9.0f);
	yt_record_set_number(&team.record, YT_F121, 2.0f);
	yt_record_set_number(&team.record, YT_F125, -1.0f);
	defense.fighters = 12.0f;
	defense.fighter_owner = 2.0f;
	planet.owner = 2.0f;
	planet.ground_forces = 8.0f;
	owned_port.owner = 2.0f;
	owned_port.treasury = 99.0f;
	other_port.owner = 3.0f;
	other_port.treasury = 88.0f;
	if (!yt_game_write_player(&game, 2, &victim, error)
	    || !yt_game_write_sector(&game, 10, &team, error)
	    || !yt_game_write_sector(&game, 20, &defense, error)
	    || !yt_game_write_planet(&game, 1, &planet, error)
	    || !yt_game_write_port(&game, 1, &owned_port, error)
	    || !yt_game_write_port(&game, 2, &other_port, error))
		goto done;
	sector_cache[2] = 20.0f;
	cloak_cache[2] = 0.5f;
	sector_cache[3] = 30.0f;
	cloak_cache[3] = 0.25f;
	if (!yt_maintenance_immediate_death(&game, sector_cache, cloak_cache,
	    YT_ARRAY_LEN(sector_cache), 2, -1.0f, &victim, error)
	    || !yt_game_read_player(&game, 2, &victim, error)
	    || !yt_game_read_sector(&game, 10, &team, error)
	    || !yt_game_read_sector(&game, 20, &defense, error)
	    || !yt_game_read_planet(&game, 1, &planet, error)
	    || !yt_game_read_port(&game, 1, &owned_port, error)
	    || !yt_game_read_port(&game, 2, &other_port, error))
		goto done;
	valid = sector_cache[2] == 0.0f && cloak_cache[2] == 0.0f
	    && sector_cache[3] == 30.0f && cloak_cache[3] == 0.25f
	    && victim.killed_by == -1.0f && victim.sector == 0.0f
	    && victim.ground_forces == 0.0f && victim.team == 0.0f
	    && victim.name_length == 13.0f && victim.fighters == 45.0f
	    && victim.credits == 123.0f && victim.cloak == 0.5f
	    && victim.score == 789.0f
	    && memcmp(victim.record.bytes, "Combat Victim", 13U) == 0
	    && yt_record_get_number(&team.record, YT_F109) == 0.0f
	    && yt_record_get_number(&team.record, YT_F117) == 9.0f
	    && yt_record_get_number(&team.record, YT_F121) == 0.0f
	    && yt_record_get_number(&team.record, YT_F125) == -1.0f
	    && defense.fighters == 12.0f && defense.fighter_owner == -2.0f
	    && planet.owner == 2.0f && planet.ground_forces == 8.0f
	    && owned_port.owner == 0.0f && owned_port.treasury == 0.0f
	    && other_port.owner == 3.0f && other_port.treasury == 88.0f;

done:
	yt_game_close(&game);
	return valid;
}

struct utility_line_tape {
	char line[3][420];
	size_t calls;
};

static bool
utility_capture_line(void *context, const uint8_t *line, size_t length,
    struct yt_error *error)
{
	struct utility_line_tape *tape = context;

	(void)error;
	if (tape == NULL || tape->calls >= YT_ARRAY_LEN(tape->line)
	    || length >= sizeof(tape->line[0]))
		return false;
	memcpy(tape->line[tape->calls], line, length);
	tape->line[tape->calls][length] = '\0';
	++tape->calls;
	return true;
}

static bool
test_xannor_player_arrival(struct yt_error *error)
{
	static const uint8_t draws[] = {
		0x00, 0x00, 0x80,
		0x00, 0x00, 0x80,
		0xff, 0xff, 0xff,
		0xff, 0xff, 0xff,
		0xff, 0xff, 0xff
	};
	static const char expected_line[] =
	    " *** Alice: lost 1, dstrd 0 (Player Killed)";
	static const char expected_survivor_line[] =
	    " *** Bob: lost 0, dstrd 1 (Xannor Lost) - Shields: 10";
	static const char expected_large_line[] =
	    " *** Carol: lost 0, dstrd 5001 (Xannor Lost) - Shields: 6000";
	static const uint8_t expected_news[] =
	    " *** Alice: lost 1, dstrd 0 (Player Killed)\r\n"
	    " *** Bob: lost 0, dstrd 1 (Xannor Lost) - Shields: 10\r\n"
	    " *** Carol: lost 0, dstrd 5001 (Xannor Lost) - Shields: 6000\r\n"
	    "\x1a";
	static const char *const expected_radio[] = {
		"Ha! We kilt 1 of yoor fyterz hoo-man slyme!",
		"Peh! Whee maik yoor wheak sheeldz 1 unitz!",
		"HA! We kilt yoo yoo hoo-man slyme bull!"
	};
	struct utility_random_script script = {draws, sizeof(draws), 0};
	struct utility_line_tape tape = {{{0}}, 0};
	struct yt_game game;
	struct yt_player player;
	struct yt_player survivor;
	struct yt_record survivor_before;
	struct yt_player large_player;
	struct yt_record large_player_before;
	struct yt_sector sector_before;
	struct yt_sector sector_after;
	struct yt_sector survivor_sector_before;
	struct yt_sector survivor_sector_after;
	struct yt_sector large_sector_before;
	struct yt_sector large_sector_after;
	struct yt_text_file news = {0};
	float sector_cache[52] = {0};
	float cloak_cache[52] = {0};
	float xannor = 1.0f;
	float location = 42.0f;
	float survivor_xannor = 1.0f;
	float survivor_location = 43.0f;
	float large_xannor = 5001.0f;
	float large_location = 44.0f;
	float exhausted_location = 42.0f;
	float exhausted_size = 0.0f;
	uint8_t *radio = NULL;
	size_t radio_length = 0;
	bool valid = false;
	size_t index;
	size_t offset;

	memset(&game, 0, sizeof(game));
	(void)remove("YTNEWS.DAT");
	(void)remove("YTRMSG.DAT");
	if (!yt_game_open(&game, YT_OPEN_UPDATE, error)
	    || !yt_game_read_player(&game, 2, &player, error))
		goto done;
	strcpy(player.name, "Alice");
	player.name_length = 5.0f;
	player.fighters = 1.0f;
	player.shields = 1.0f;
	player.sector = 42.0f;
	player.cloak = 0.25f;
	player.killed_by = 0.0f;
	player.team = 0.0f;
	player.ground_forces = 0.0f;
	if (!yt_game_write_player(&game, 2, &player, error))
		goto done;
	if (!yt_game_read_player(&game, 3, &survivor, error))
		goto done;
	strcpy(survivor.name, "Bob");
	survivor.name_length = 3.0f;
	survivor.fighters = 1.0f;
	survivor.shields = 10.0f;
	survivor.sector = 43.0f;
	survivor.cloak = 0.75f;
	survivor.killed_by = 0.0f;
	survivor.team = 0.0f;
	survivor.ground_forces = 0.0f;
	if (!yt_game_write_player(&game, 3, &survivor, error)
	    || !yt_game_read_player(&game, 3, &survivor, error))
		goto done;
	survivor_before = survivor.record;
	if (!yt_game_read_player(&game, 4, &large_player, error))
		goto done;
	strcpy(large_player.name, "Carol");
	large_player.name_length = 5.0f;
	large_player.fighters = 5001.0f;
	large_player.shields = 6000.0f;
	large_player.sector = 44.0f;
	large_player.cloak = 0.5f;
	large_player.killed_by = 0.0f;
	large_player.team = 0.0f;
	large_player.ground_forces = 0.0f;
	if (!yt_game_write_player(&game, 4, &large_player, error)
	    || !yt_game_read_player(&game, 4, &large_player, error))
		goto done;
	large_player_before = large_player.record;
	sector_cache[2] = 42.0f;
	cloak_cache[2] = 0.25f;
	sector_cache[3] = 43.0f;
	cloak_cache[3] = 0.75f;
	sector_cache[4] = 44.0f;
	cloak_cache[4] = 0.5f;
	yt_random_set_provider(&game.random, utility_random_fill, &script);
	if (!yt_game_read_sector(&game, 42, &sector_before, error)
	    || !yt_game_read_sector(&game, 43, &survivor_sector_before, error)
	    || !yt_game_read_sector(&game, 44, &large_sector_before, error)
	    || !yt_maintenance_xannor_target_finish(&game, sector_cache,
	    cloak_cache, YT_ARRAY_LEN(sector_cache), true, 20, 0,
	    &exhausted_location, &exhausted_size, utility_capture_line, &tape,
	    error)
	    || !yt_maintenance_xannor_target_finish(&game, sector_cache,
	    cloak_cache, YT_ARRAY_LEN(sector_cache), false, 20, 2,
	    &location, &xannor, utility_capture_line, &tape, error)
	    || !yt_maintenance_xannor_target_finish(&game, sector_cache,
	    cloak_cache, YT_ARRAY_LEN(sector_cache), true, 19, 2,
	    &location, &xannor, utility_capture_line, &tape, error)
	    || !yt_maintenance_xannor_target_finish(&game, sector_cache,
	    cloak_cache, YT_ARRAY_LEN(sector_cache), true, 20, 0,
	    &location, &xannor, utility_capture_line, &tape, error)
	    || !yt_maintenance_xannor_target_finish(&game, sector_cache,
	    cloak_cache, YT_ARRAY_LEN(sector_cache), true, 20, 2,
	    &location, &xannor, utility_capture_line, &tape, error)
	    || !yt_maintenance_xannor_target_finish(&game, sector_cache,
	    cloak_cache, YT_ARRAY_LEN(sector_cache), true, 20, 3,
	    &survivor_location, &survivor_xannor, utility_capture_line, &tape,
	    error)
	    || !yt_maintenance_xannor_target_finish(&game, sector_cache,
	    cloak_cache, YT_ARRAY_LEN(sector_cache), true, 20, 4,
	    &large_location, &large_xannor, utility_capture_line, &tape, error)
	    || !yt_game_read_player(&game, 2, &player, error)
	    || !yt_game_read_player(&game, 3, &survivor, error)
	    || !yt_game_read_player(&game, 4, &large_player, error)
	    || !yt_game_read_sector(&game, 42, &sector_after, error)
	    || !yt_game_read_sector(&game, 43, &survivor_sector_after, error)
	    || !yt_game_read_sector(&game, 44, &large_sector_after, error)
	    || !yt_text_read("YTNEWS.DAT", &news, error)
	    || !read_file("YTRMSG.DAT", &radio, &radio_length))
		goto done;
	valid = exhausted_location == 0.0f && exhausted_size == 0.0f
	    && location == 42.0f && xannor == 1.0f
	    && survivor_location == 0.0f && survivor_xannor == 0.0f
	    && large_location == 0.0f && large_xannor == 0.0f
	    && game.random.draws == 5U
	    && script.position == sizeof(draws)
	    && tape.calls == 3U && strcmp(tape.line[0], expected_line) == 0
	    && strcmp(tape.line[1], expected_survivor_line) == 0
	    && strcmp(tape.line[2], expected_large_line) == 0
	    && player.killed_by == -1.0f && player.fighters == 0.0f
	    && player.shields == 0.0f && player.sector == 0.0f
	    && sector_cache[2] == 0.0f && cloak_cache[2] == 0.0f
	    && survivor.killed_by == 0.0f && survivor.fighters == 1.0f
	    && survivor.shields == 10.0f && survivor.sector == 43.0f
	    && sector_cache[3] == 43.0f && cloak_cache[3] == 0.75f
	    && memcmp(survivor.record.bytes, survivor_before.bytes,
	    YT_RECORD_SIZE) == 0
	    && memcmp(survivor_sector_after.record.bytes,
	    survivor_sector_before.record.bytes, YT_RECORD_SIZE) == 0
	    && large_player.killed_by == 0.0f
	    && large_player.fighters == 5001.0f
	    && large_player.shields == 6000.0f
	    && large_player.sector == 44.0f
	    && sector_cache[4] == 44.0f && cloak_cache[4] == 0.5f
	    && memcmp(large_player.record.bytes, large_player_before.bytes,
	    YT_RECORD_SIZE) == 0
	    && memcmp(large_sector_after.record.bytes,
	    large_sector_before.record.bytes, YT_RECORD_SIZE) == 0
	    && sector_after.fighters == sector_before.fighters + 2.0f
	    && news.length == sizeof(expected_news) - 1U
	    && memcmp(news.data, expected_news, sizeof(expected_news) - 1U) == 0
	    && radio_length == 3U * sizeof(struct yt_radio_record);
	for (index = 0; valid && index < 3U; ++index) {
		const struct yt_radio_record *record =
		    (const struct yt_radio_record *)(radio
		    + index * sizeof(struct yt_radio_record));
		size_t length = strlen(expected_radio[index]);

		valid = yt_radio_get_number(record, 0) == 1.0f
		    && yt_radio_get_number(record, 4) == 2.0f
		    && yt_radio_get_number(record, 8) == -1.0f
		    && memcmp(record->bytes + 12U, expected_radio[index], length) == 0;
	}
	for (offset = 0U; valid && offset < YT_RECORD_SIZE; ++offset) {
		if (offset < YT_F81 || offset >= YT_F81 + 4U)
			valid = sector_after.record.bytes[offset]
			    == sector_before.record.bytes[offset];
	}

done:
	yt_text_free(&news);
	free(radio);
	yt_game_close(&game);
	(void)remove("YTNEWS.DAT");
	(void)remove("YTRMSG.DAT");
	return valid;
}

static bool
test_xannor_planet_arrival(struct yt_error *error)
{
	static const uint8_t high_draws[] = {
		0xff, 0xff, 0xff,
		0xff, 0xff, 0xff
	};
	static const char attack_line[] =
	    " *** 2 Xannor attacked the planet \"Terra\"";
	static const char fighters_line[] =
	    " *** Xannor fighters destroyed!";
	static const char planet_line[] =
	    " *** Planet \"Terra\" destroyed!";
	static const char expected_fighters_news[] =
	    " *** 2 Xannor attacked the planet \"Terra\"\r\n"
	    " *** Xannor fighters destroyed!\r\n\x1a";
	static const char expected_planet_news[] =
	    " *** 2 Xannor attacked the planet \"Terra\"\r\n"
	    " *** Planet \"Terra\" destroyed!\r\n\x1a";
	struct utility_random_script script = {NULL, 0, 0};
	struct utility_line_tape tape = {{{0}}, 0};
	struct yt_game game;
	struct yt_planet planet;
	struct yt_sector sector;
	struct yt_text_file news = {0};
	float location = 733.0f;
	float group_size = 2.0f;
	bool valid = false;

	memset(&game, 0, sizeof(game));
	memset(&sector, 0, sizeof(sector));
	(void)remove("YTNEWS.DAT");
	if (!yt_game_open(&game, YT_OPEN_UPDATE, error)
	    || !yt_game_read_planet(&game, 1, &planet, error))
		goto done;
	strcpy(planet.name, "Terra");
	planet.name_length = 5.0f;
	planet.owner = -1.0f;
	planet.ground_forces = 1.0f;
	planet.production[0] = 501.0f;
	planet.production[1] = 0.0f;
	planet.production[2] = 0.0f;
	sector.planet = 1.0f;
	if (!yt_game_write_planet(&game, 1, &planet, error))
		goto done;
	yt_random_set_provider(&game.random, utility_random_fill, &script);
	if (!yt_maintenance_xannor_planet_arrival(&game, &location,
	    &group_size, &sector, utility_capture_line, &tape, error)
	    || location != 733.0f || group_size != 2.0f
	    || sector.planet != 1.0f || game.random.draws != 0U
	    || script.position != 0U || tape.calls != 0U)
		goto done;

	planet.owner = 7.0f;
	planet.ground_forces = 1.0f;
	planet.production[0] = 501.0f;
	planet.production[1] = 0.0f;
	planet.production[2] = 0.0f;
	planet.stock[0] = 9999.0f;
	planet.stock[1] = 0.0f;
	planet.stock[2] = 0.0f;
	if (!yt_game_write_planet(&game, 1, &planet, error))
		goto done;
	script = (struct utility_random_script){high_draws,
	    sizeof(high_draws), 0};
	yt_random_set_provider(&game.random, utility_random_fill, &script);
	if (!yt_maintenance_xannor_planet_arrival(&game, &location,
	    &group_size, &sector, utility_capture_line, &tape, error)
	    || !yt_game_read_planet(&game, 1, &planet, error)
	    || !yt_text_read("YTNEWS.DAT", &news, error)
	    || location != 0.0f || group_size != 0.0f
	    || sector.planet != 1.0f || planet.owner != 0.0f
	    || planet.ground_forces != 0.0f
	    || planet.production[0] != 501.0f
	    || planet.stock[0] != 5010.0f || planet.name_length != 5.0f
	    || game.random.draws != 2U
	    || script.position != sizeof(high_draws) || tape.calls != 2U
	    || strcmp(tape.line[0], attack_line) != 0
	    || strcmp(tape.line[1], fighters_line) != 0
	    || news.length != sizeof(expected_fighters_news) - 1U
	    || memcmp(news.data, expected_fighters_news,
	    sizeof(expected_fighters_news) - 1U) != 0)
		goto done;

	yt_text_free(&news);
	(void)remove("YTNEWS.DAT");
	memset(&tape, 0, sizeof(tape));
	strcpy(planet.name, "Terra");
	planet.name_length = 5.0f;
	planet.owner = 7.0f;
	planet.ground_forces = 0.0f;
	planet.production[0] = 0.0f;
	planet.production[1] = 0.0f;
	planet.production[2] = 0.0f;
	planet.stock[0] = 10.0f;
	sector.planet = 1.0f;
	location = 733.0f;
	group_size = 2.0f;
	script = (struct utility_random_script){NULL, 0, 0};
	yt_random_set_provider(&game.random, utility_random_fill, &script);
	if (!yt_game_write_planet(&game, 1, &planet, error)
	    || !yt_maintenance_xannor_planet_arrival(&game, &location,
	    &group_size, &sector, utility_capture_line, &tape, error)
	    || !yt_game_read_planet(&game, 1, &planet, error)
	    || !yt_text_read("YTNEWS.DAT", &news, error))
		goto done;
	valid = location == 733.0f && group_size == 2.0f
	    && sector.planet == 0.0f && planet.owner == 0.0f
	    && planet.ground_forces == 0.0f && planet.name_length == 0.0f
	    && planet.stock[0] == 0.0f && game.random.draws == 0U
	    && script.position == 0U && tape.calls == 2U
	    && strcmp(tape.line[0], attack_line) == 0
	    && strcmp(tape.line[1], planet_line) == 0
	    && news.length == sizeof(expected_planet_news) - 1U
	    && memcmp(news.data, expected_planet_news,
	    sizeof(expected_planet_news) - 1U) == 0;

done:
	yt_text_free(&news);
	yt_game_close(&game);
	(void)remove("YTNEWS.DAT");
	return valid;
}

static bool
test_maintenance_route_builder(struct yt_error *error)
{
	static const uint8_t initial_news[] = "prior\r\n";
	static const uint8_t expected_news[] =
	    "prior\r\n"
	    "*** Error - Sector path not found - from sector 1 to sector  0\r\n"
	    "*** Error - Sector path not found - from sector 1 to sector  5\r\n"
	    "\x1a";
	struct yt_maintenance_route_cache cache = {0};
	struct yt_text_file news = {0};
	struct yt_game game;
	struct yt_sector sector;
	int next = -1;
	bool valid = false;

	memset(&game, 0, sizeof(game));
	if (!yt_game_open(&game, YT_OPEN_UPDATE, error))
		goto done;
	if (!yt_game_read_sector(&game, 1, &sector, error))
		goto done;
	memset(sector.warps, 0, sizeof(sector.warps));
	sector.warps[0] = 2.0f;
	sector.warps[1] = 3.0f;
	if (!yt_game_write_sector(&game, 1, &sector, error)
	    || !yt_game_read_sector(&game, 2, &sector, error))
		goto done;
	memset(sector.warps, 0, sizeof(sector.warps));
	sector.warps[0] = 4.0f;
	if (!yt_game_write_sector(&game, 2, &sector, error)
	    || !yt_game_read_sector(&game, 3, &sector, error))
		goto done;
	memset(sector.warps, 0, sizeof(sector.warps));
	sector.warps[0] = 4.0f;
	if (!yt_game_write_sector(&game, 3, &sector, error)
	    || !yt_game_read_sector(&game, 4, &sector, error))
		goto done;
	memset(sector.warps, 0, sizeof(sector.warps));
	if (!yt_game_write_sector(&game, 4, &sector, error)
	    || !yt_game_read_sector(&game, 5, &sector, error))
		goto done;
	memset(sector.warps, 0, sizeof(sector.warps));
	if (!yt_game_write_sector(&game, 5, &sector, error)
	    || !yt_text_write("YTNEWS.DAT", initial_news,
	    sizeof(initial_news) - 1U, true, error))
		goto done;

	if (!yt_maintenance_route_next_hop(&game, &cache, 1, 1, &next,
	    error) || next != 0 || cache.warps != NULL)
		goto done;
	if (!yt_maintenance_route_next_hop(&game, &cache, 1, 4, &next,
	    error) || next != 2 || cache.warps == NULL
	    || cache.successors == NULL || cache.successors[1] != 2
	    || cache.successors[2] != 4 || cache.successors[4] != 0
	    || cache.sector_count != 2004)
		goto done;

	if (!yt_game_read_sector(&game, 1, &sector, error))
		goto done;
	memset(sector.warps, 0, sizeof(sector.warps));
	sector.warps[0] = 3.0f;
	if (!yt_game_write_sector(&game, 1, &sector, error)
	    || !yt_maintenance_route_next_hop(&game, &cache, 1, 4, &next,
	    error) || next != 2
	    || !yt_maintenance_route_next_hop(&game, &cache, 1, 0, &next,
	    error) || next != 0 || cache.successors == NULL
	    || cache.successors[1] != 0
	    || !yt_maintenance_route_next_hop(&game, &cache, 1, 5, &next,
	    error) || next != 0 || cache.successors == NULL
	    || cache.successors[1] != 0
	    || !yt_text_read("YTNEWS.DAT", &news, error))
		goto done;
	valid = news.length == sizeof(expected_news) - 1U
	    && memcmp(news.data, expected_news, sizeof(expected_news) - 1U) == 0;

done:
	yt_text_free(&news);
	yt_maintenance_route_cache_free(&cache);
	yt_game_close(&game);
	return valid;
}

static bool
test_ytconfig(struct yt_error *error)
{
	uint8_t expected[4096];
	uint8_t scoreboard[41];
	uint8_t *actual = NULL;
	size_t expected_length = 0U;
	size_t actual_length = 0U;
	struct yt_game game;
	struct yt_config_menu_working working;
	struct yt_config_output_result output;
	int today;
	int year;
	uint8_t folded;
	static const char input[] = "JX";
	bool valid = false;

#define APPEND_CONFIG_OUTPUT() do { \
	if (output.output_length > sizeof(expected) - expected_length) \
		goto done; \
	memcpy(expected + expected_length, output.output, output.output_length); \
	expected_length += output.output_length; \
} while (0)

	memset(&game, 0, sizeof(game));
	if (!yt_database_open(&game.database, "YTDATA.DAT", YT_OPEN_UPDATE,
	    error) || !yt_config_load(&game.database, &game.config, error))
		return false;
	game.config.local_screen = -2.0f;
	game.config.last_maintenance = -1.0f;
	if (!yt_config_store(&game.database, &game.config, error))
		goto done;
	if (!yt_config_prepare_menu_working(&game.config, scoreboard, &working))
		goto done;
	if (!yt_current_date_serial(game.config.epoch_year, &today, &year,
	    error))
		goto done;
	if (!yt_config_compose_menu_prompt(&game.config, &working, today, 0U,
	    &output))
		goto done;
	APPEND_CONFIG_OUTPUT();
	if (!yt_config_compose_command_echo((uint8_t)'J',
	    output.final_column, &folded, &output) || folded != (uint8_t)'J')
		goto done;
	APPEND_CONFIG_OUTPUT();
	working.local_screen = 1.0f;
	game.config.local_screen = 1.0f;
	if (!yt_config_compose_menu_prompt(&game.config, &working, today, 0U,
	    &output))
		goto done;
	APPEND_CONFIG_OUTPUT();
	if (!yt_config_compose_command_echo((uint8_t)'X',
	    output.final_column, &folded, &output) || folded != (uint8_t)'X')
		goto done;
	APPEND_CONFIG_OUTPUT();
	if (!yt_config_compose_exit(output.final_column, &output))
		goto done;
	APPEND_CONFIG_OUTPUT();
	yt_game_close(&game);
	if (!write_file("config.in", input, sizeof(input) - 1U)
	    || !run_redirected(YT_CONFIG_EXE, "config.in", "config.out")
	    || !read_file("config.out", &actual, &actual_length)
	    || actual_length != expected_length
	    || memcmp(actual, expected, expected_length) != 0)
		goto done_closed;
	memset(&game, 0, sizeof(game));
	if (!yt_database_open(&game.database, "YTDATA.DAT", YT_OPEN_READ,
	    error) || !yt_config_load(&game.database, &game.config, error))
		goto done;
	valid = game.config.local_screen == 1.0f;

done:
	yt_game_close(&game);
done_closed:
	free(actual);
#undef APPEND_CONFIG_OUTPUT
	return valid;
}

static bool
test_ytconfig_missing_data(void)
{
	static const uint8_t expected[] =
	    "\aMAIN DATA FILE NOT FOUND. PLEASE RUN YT-INIT FIRST!\r";
	uint8_t *database = NULL;
	uint8_t *output = NULL;
	size_t database_length = 0U;
	size_t output_length = 0U;
	FILE *probe;
	bool removed;
	bool valid = false;

	if (!read_file("YTDATA.DAT", &database, &database_length)
	    || remove("YTDATA.DAT") != 0
	    || !write_file("config.in", "", 0U)
	    || !run_redirected(YT_CONFIG_EXE, "config.in", "config.out")
	    || !read_file("config.out", &output, &output_length))
		goto done;
	probe = fopen("YTDATA.DAT", "rb");
	removed = probe == NULL;
	if (probe != NULL)
		(void)fclose(probe);
	valid = removed && output_length == sizeof(expected) - 1U
	    && memcmp(output, expected, sizeof(expected) - 1U) == 0;

done:
	if (database != NULL
	    && !write_file("YTDATA.DAT", database, database_length))
		valid = false;
	free(database);
	free(output);
	return valid;
}

static bool
test_ytconfig_genesis(struct yt_error *error)
{
	static const uint8_t prompt[] =
	    "L\r\r"
	    "There are 1000 ports in the game, enter a number\r"
	    "greater than 1000 to TURN OFF the Genesis Function.\r"
	    "\r"
	    "How many ports will a player need to initiate Genesis? "
	    "[50 - 1000] ";
	static const uint8_t stored_row[] =
	    "<L> Ports needed to initiate Genesis: 100\r";
	static const char blank_input[] = "L\nX";
	static const char invalid_input[] = "L10\nX";
	static const char valid_input[] = "L100\nX";
	struct yt_game game;
	struct yt_record baseline;
	struct yt_record expected_record;
	uint8_t *output = NULL;
	size_t output_length = 0U;
	float original_genesis;
	bool valid = false;

	memset(&game, 0, sizeof(game));
	if (!yt_database_open(&game.database, "YTDATA.DAT", YT_OPEN_UPDATE,
	    error) || !yt_config_load(&game.database, &game.config, error))
		goto done;
	original_genesis = game.config.genesis_ports;
	game.config.genesis_ports = 300.0f;
	if (!yt_config_store(&game.database, &game.config, error))
		goto done;
	yt_game_close(&game);
	memset(&game, 0, sizeof(game));
	if (!yt_database_open(&game.database, "YTDATA.DAT", YT_OPEN_READ,
	    error) || !yt_config_load(&game.database, &game.config, error))
		goto done;
	baseline = game.config.record;
	yt_game_close(&game);
	if (!write_file("config.in", blank_input, sizeof(blank_input) - 1U)
	    || !run_redirected(YT_CONFIG_EXE, "config.in", "config.out")
	    || !read_file("config.out", &output, &output_length)
	    || !bytes_contain(output, output_length, prompt, sizeof(prompt) - 1U)
	    || memchr(output, '\a', output_length) != NULL)
		goto done_closed;
	free(output);
	output = NULL;
	memset(&game, 0, sizeof(game));
	if (!yt_database_open(&game.database, "YTDATA.DAT", YT_OPEN_READ,
	    error) || !yt_config_load(&game.database, &game.config, error)
	    || memcmp(game.config.record.bytes, baseline.bytes,
		YT_RECORD_SIZE) != 0)
		goto done;
	yt_game_close(&game);
	if (!write_file("config.in", invalid_input, sizeof(invalid_input) - 1U)
	    || !run_redirected(YT_CONFIG_EXE, "config.in", "config.out")
	    || !read_file("config.out", &output, &output_length)
	    || !bytes_contain(output, output_length, prompt, sizeof(prompt) - 1U)
	    || memchr(output, '\a', output_length) == NULL)
		goto done_closed;
	free(output);
	output = NULL;
	memset(&game, 0, sizeof(game));
	if (!yt_database_open(&game.database, "YTDATA.DAT", YT_OPEN_READ,
	    error) || !yt_config_load(&game.database, &game.config, error)
	    || game.config.genesis_ports != 300.0f
	    || memcmp(game.config.record.bytes, baseline.bytes,
		YT_RECORD_SIZE) != 0)
		goto done;
	yt_game_close(&game);
	if (!write_file("config.in", valid_input, sizeof(valid_input) - 1U)
	    || !run_redirected(YT_CONFIG_EXE, "config.in", "config.out")
	    || !read_file("config.out", &output, &output_length)
	    || !bytes_contain(output, output_length, prompt, sizeof(prompt) - 1U)
	    || !bytes_contain(output, output_length, stored_row,
		sizeof(stored_row) - 1U)
	    || memchr(output, '\a', output_length) != NULL)
		goto done_closed;
	memset(&game, 0, sizeof(game));
	if (!yt_database_open(&game.database, "YTDATA.DAT", YT_OPEN_UPDATE,
	    error) || !yt_config_load(&game.database, &game.config, error))
		goto done;
	expected_record = baseline;
	valid = yt_record_set_number(&expected_record, YT_F105, 100.0f)
	    && game.config.genesis_ports == 100.0f
	    && memcmp(game.config.record.bytes, expected_record.bytes,
		YT_RECORD_SIZE) == 0;
	game.config.genesis_ports = original_genesis;
	if (!yt_config_store(&game.database, &game.config, error))
		valid = false;

done:
	yt_game_close(&game);
done_closed:
	free(output);
	return valid;
}

struct config_hq_test_event {
	enum yt_config_hq_operation operation;
	size_t basic_record;
};

struct config_hq_test_tape {
	struct yt_record initial[256];
	struct yt_record durable[256];
	struct config_hq_test_event events[9];
	size_t event_count;
	size_t fail_at;
};

static void
config_hq_test_init(struct config_hq_test_tape *tape,
    struct yt_config *config)
{
	size_t record;

	memset(tape, 0, sizeof(*tape));
	memset(config, 0, sizeof(*config));
	config->sector_offset = 100.0f;
	config->planet_offset = 3055.0f;
	config->total_records = 3155.0f;
	config->headquarters = 85.0f;
	for (record = 0U; record < YT_ARRAY_LEN(tape->initial); ++record) {
		size_t byte;

		for (byte = 0U; byte < YT_RECORD_SIZE; ++byte)
			tape->initial[record].bytes[byte] =
			    (uint8_t)(record * 3U + byte);
	}
	(void)yt_record_set_number(&tape->initial[109], YT_F93, 0.0f);
	(void)yt_record_set_number(&tape->initial[109], YT_F81, 2.0f);
	(void)yt_record_set_number(&tape->initial[109], YT_F85, -1.0f);
	(void)yt_record_set_number(&tape->initial[185], YT_F81, 3.0f);
	config->record = tape->initial[1];
	tape->initial[1].bytes[YT_RECORD_TAIL_OFFSET] ^= 0x5aU;
	memcpy(tape->durable, tape->initial, sizeof(tape->durable));
}

static bool
config_hq_test_event(struct config_hq_test_tape *tape,
    enum yt_config_hq_operation operation, size_t basic_record,
    struct yt_error *error)
{
	if (tape->event_count >= YT_ARRAY_LEN(tape->events))
		return false;
	tape->events[tape->event_count++] =
	    (struct config_hq_test_event){operation, basic_record};
	if (tape->fail_at != 0U && tape->event_count == tape->fail_at) {
		if (error != NULL)
			error->status = YT_IO_ERROR;
		return false;
	}
	return true;
}

static bool
config_hq_test_read(void *context, size_t basic_record,
    struct yt_record *record, struct yt_error *error)
{
	struct config_hq_test_tape *tape = context;
	static const enum yt_config_hq_operation reads[] = {
		YT_CONFIG_HQ_READ_CANDIDATE_INITIAL,
		YT_CONFIG_HQ_READ_OLD,
		YT_CONFIG_HQ_READ_CANDIDATE_FRESH,
		YT_CONFIG_HQ_READ_SECTOR_ONE,
		YT_CONFIG_HQ_READ_CONFIG,
	};
	size_t read_index = 0U;
	size_t event;

	for (event = 0U; event < tape->event_count; ++event) {
		if (tape->events[event].operation == YT_CONFIG_HQ_READ_CANDIDATE_INITIAL
		    || tape->events[event].operation == YT_CONFIG_HQ_READ_OLD
		    || tape->events[event].operation == YT_CONFIG_HQ_READ_CANDIDATE_FRESH
		    || tape->events[event].operation == YT_CONFIG_HQ_READ_SECTOR_ONE
		    || tape->events[event].operation == YT_CONFIG_HQ_READ_CONFIG)
			++read_index;
	}
	if (read_index >= YT_ARRAY_LEN(reads)
	    || !config_hq_test_event(tape, reads[read_index], basic_record,
	    error))
		return false;
	*record = tape->durable[basic_record];
	return true;
}

static bool
config_hq_test_write(void *context, size_t basic_record,
    const struct yt_record *record, struct yt_error *error)
{
	struct config_hq_test_tape *tape = context;
	static const enum yt_config_hq_operation writes[] = {
		YT_CONFIG_HQ_WRITE_OLD,
		YT_CONFIG_HQ_WRITE_CANDIDATE,
		YT_CONFIG_HQ_WRITE_SECTOR_ONE,
		YT_CONFIG_HQ_WRITE_CONFIG,
	};
	size_t write_index = 0U;
	size_t event;

	for (event = 0U; event < tape->event_count; ++event) {
		if (tape->events[event].operation == YT_CONFIG_HQ_WRITE_OLD
		    || tape->events[event].operation == YT_CONFIG_HQ_WRITE_CANDIDATE
		    || tape->events[event].operation == YT_CONFIG_HQ_WRITE_SECTOR_ONE
		    || tape->events[event].operation == YT_CONFIG_HQ_WRITE_CONFIG)
			++write_index;
	}
	if (write_index >= YT_ARRAY_LEN(writes)
	    || !config_hq_test_event(tape, writes[write_index], basic_record,
	    error))
		return false;
	tape->durable[basic_record] = *record;
	return true;
}

static bool
test_ytconfig_headquarters_transaction(void)
{
	static const struct yt_config_record_ops ops = {
		config_hq_test_read,
		config_hq_test_write,
	};
	static const uint8_t raw_clear[4] = {0x00, 0x00, 0x80, 0x00};
	struct config_hq_test_tape success;
	struct yt_config_hq_state state;
	struct yt_config config;
	struct yt_error error;
	uint8_t candidate_raw[4];
	size_t failure;
	bool result;

	config_hq_test_init(&success, &config);
	yt_error_clear(&error);
	result = yt_config_headquarters_relocate(&state, &config, 8.6f, &ops,
	    &success, &error);
	if (qb_mbf32_encode(8.6f, candidate_raw) == QB_MBF_OVERFLOW
	    || !result || !state.complete
	    || state.route != YT_CONFIG_HQ_ROUTE_RELOCATED
	    || state.attempted != YT_CONFIG_HQ_NONE
	    || state.candidate_logical != 9 || state.old_logical != 85
	    || state.reads_completed != 5U || state.writes_completed != 4U
	    || state.captured_candidate_fighters != 2.0f
	    || state.merged_fighters != 5.0f || success.event_count != 9U
	    || memcmp(success.durable[185].bytes + YT_F81, raw_clear, 4U) != 0
	    || memcmp(success.durable[185].bytes + YT_F85, raw_clear, 4U) != 0
	    || memcmp(success.durable[185].bytes + YT_F93, raw_clear, 4U) != 0
	    || yt_record_get_number(&success.durable[109], YT_F81) != 5.0f
	    || yt_record_get_number(&success.durable[109], YT_F85) != -1.0f
	    || yt_record_get_number(&success.durable[109], YT_F93) != 100.0f
	    || memcmp(success.durable[101].bytes + YT_F105, candidate_raw,
	    4U) != 0
	    || memcmp(success.durable[1].bytes + YT_F117, candidate_raw, 4U) != 0
	    || memcmp(&state.field, &success.durable[1], sizeof(state.field)) != 0)
		return false;
	for (failure = 1U; failure <= success.event_count; ++failure) {
		struct config_hq_test_tape tape;
		bool written[256] = {false};
		unsigned reads = 0U;
		unsigned writes = 0U;
		size_t event;
		size_t record;

		config_hq_test_init(&tape, &config);
		tape.fail_at = failure;
		yt_error_clear(&error);
		result = yt_config_headquarters_relocate(&state, &config, 8.6f,
		    &ops, &tape, &error);
		for (event = 0U; event + 1U < failure; ++event) {
			if (success.events[event].operation == YT_CONFIG_HQ_WRITE_OLD
			    || success.events[event].operation ==
			    YT_CONFIG_HQ_WRITE_CANDIDATE
			    || success.events[event].operation ==
			    YT_CONFIG_HQ_WRITE_SECTOR_ONE
			    || success.events[event].operation ==
			    YT_CONFIG_HQ_WRITE_CONFIG) {
				++writes;
				written[success.events[event].basic_record] = true;
			}
			else
				++reads;
		}
		if (result || error.status != YT_IO_ERROR || state.complete
		    || state.attempted != success.events[failure - 1U].operation
		    || state.current_basic_record !=
		    success.events[failure - 1U].basic_record
		    || state.reads_completed != reads
		    || state.writes_completed != writes
		    || tape.event_count != failure
		    || memcmp(tape.events, success.events,
		    failure * sizeof(tape.events[0])) != 0)
			return false;
		for (record = 0U; record < YT_ARRAY_LEN(tape.durable); ++record) {
			const struct yt_record *expected = written[record]
			    ? &success.durable[record] : &tape.initial[record];

			if (memcmp(&tape.durable[record], expected,
			    sizeof(*expected)) != 0)
				return false;
		}
	}
	config_hq_test_init(&success, &config);
	(void)yt_record_set_number(&success.durable[109], YT_F85, 2.0f);
	yt_error_clear(&error);
	return yt_config_headquarters_relocate(&state, &config, 8.6f, &ops,
	    &success, &error)
	    && state.complete && state.route == YT_CONFIG_HQ_ROUTE_OCCUPIED
	    && state.reads_completed == 1U && state.writes_completed == 0U
	    && success.event_count == 1U
	    && !yt_config_headquarters_relocate(NULL, &config, 8.6f, &ops,
	    &success, &error)
	    && !yt_config_headquarters_relocate(&state, NULL, 8.6f, &ops,
	    &success, &error)
	    && !yt_config_headquarters_relocate(&state, &config, 8.6f, NULL,
	    &success, &error);
}

struct config_local_screen_test_tape {
	struct yt_record durable;
	unsigned reads;
	unsigned writes;
	unsigned fail_at;
};

static bool
config_local_screen_test_read(void *context, size_t basic_record,
    struct yt_record *record, struct yt_error *error)
{
	struct config_local_screen_test_tape *tape = context;

	if (basic_record != 1U)
		return false;
	++tape->reads;
	if (tape->fail_at == tape->reads + tape->writes) {
		if (error != NULL)
			error->status = YT_IO_ERROR;
		return false;
	}
	*record = tape->durable;
	return true;
}

static bool
config_local_screen_test_write(void *context, size_t basic_record,
    const struct yt_record *record, struct yt_error *error)
{
	struct config_local_screen_test_tape *tape = context;

	if (basic_record != 1U)
		return false;
	++tape->writes;
	if (tape->fail_at == tape->reads + tape->writes) {
		if (error != NULL)
			error->status = YT_IO_ERROR;
		return false;
	}
	tape->durable = *record;
	return true;
}

static bool
test_ytconfig_overlay_transaction(void)
{
	static const uint8_t raw_allow[4] = {0x00, 0x00, 0x7d, 0x00};
	static const struct yt_config_record_ops ops = {
		config_local_screen_test_read,
		config_local_screen_test_write,
	};
	struct config_local_screen_test_tape tape;
	struct yt_config_overlay_state state;
	struct yt_record loaded;
	struct yt_record expected;
	struct yt_error error;
	uint8_t raw[4];
	uint8_t raw_epoch[4];
	uint8_t second[] = {0xaa, 0xbb};
	uint8_t scoreboard[41];
	struct yt_config_overlay overlays[2];
	unsigned byte;

	for (byte = 0U; byte < YT_RECORD_SIZE; ++byte)
		tape.durable.bytes[byte] = (uint8_t)(byte * 13U + 7U);
	loaded = tape.durable;
	if (qb_mbf32_encode(123.5f, raw) == QB_MBF_OVERFLOW)
		return false;
	overlays[0] = (struct yt_config_overlay){YT_F105, raw, sizeof(raw)};
	overlays[1] = (struct yt_config_overlay){YT_F105 + 2U, second,
	    sizeof(second)};
	tape.reads = 0U;
	tape.writes = 0U;
	tape.fail_at = 0U;
	yt_error_clear(&error);
	if (!yt_config_apply_overlays(&state, overlays, 2U, &ops, &tape,
	    &error) || !state.complete
	    || state.attempted != YT_CONFIG_OVERLAY_NONE
	    || !state.field_loaded || !state.write_complete
	    || state.overlay_index != 2U || state.overlays_completed != 2U
	    || tape.reads != 1U || tape.writes != 1U
	    || memcmp(tape.durable.bytes + YT_F105, raw, 2U) != 0
	    || memcmp(tape.durable.bytes + YT_F105 + 2U, second,
	    sizeof(second)) != 0)
		return false;
	for (byte = 0U; byte < YT_RECORD_SIZE; ++byte)
		tape.durable.bytes[byte] = (uint8_t)(byte * 13U + 7U);
	loaded = tape.durable;
	expected = loaded;
	memset(scoreboard, ' ', sizeof(scoreboard));
	memcpy(scoreboard, "YTSCORE.ASC", 11U);
	if (qb_mbf32_encode(11.0f, raw) == QB_MBF_OVERFLOW)
		return false;
	memcpy(expected.bytes, scoreboard, sizeof(scoreboard));
	memcpy(expected.bytes + YT_F41, raw, sizeof(raw));
	overlays[0] = (struct yt_config_overlay){0U, scoreboard,
	    sizeof(scoreboard)};
	overlays[1] = (struct yt_config_overlay){YT_F41, raw, sizeof(raw)};
	tape.reads = 0U;
	tape.writes = 0U;
	tape.fail_at = 0U;
	yt_error_clear(&error);
	if (!yt_config_apply_overlays(&state, overlays, 2U, &ops, &tape,
	    &error) || !state.complete || tape.reads != 1U
	    || tape.writes != 1U || state.overlays_completed != 2U
	    || memcmp(&tape.durable, &expected, sizeof(expected)) != 0)
		return false;
	for (byte = 0U; byte < YT_RECORD_SIZE; ++byte)
		tape.durable.bytes[byte] = (uint8_t)(byte * 13U + 7U);
	loaded = tape.durable;
	expected = loaded;
	if (qb_mbf32_encode(26.0f, raw_epoch) == QB_MBF_OVERFLOW)
		return false;
	memcpy(expected.bytes + YT_F81, raw_allow, sizeof(raw_allow));
	memcpy(expected.bytes + YT_F45, raw_epoch, sizeof(raw_epoch));
	overlays[0] = (struct yt_config_overlay){YT_F81, raw_allow,
	    sizeof(raw_allow)};
	overlays[1] = (struct yt_config_overlay){YT_F45, raw_epoch,
	    sizeof(raw_epoch)};
	tape.reads = 0U;
	tape.writes = 0U;
	tape.fail_at = 0U;
	yt_error_clear(&error);
	if (!yt_config_apply_loaded_overlays(&state, &loaded, overlays, 2U,
	    &ops, &tape, &error) || !state.complete || tape.reads != 0U
	    || tape.writes != 1U || state.overlays_completed != 2U
	    || memcmp(&tape.durable, &expected, sizeof(expected)) != 0)
		return false;
	if (qb_mbf32_encode(123.5f, raw) == QB_MBF_OVERFLOW)
		return false;
	overlays[0] = (struct yt_config_overlay){YT_F105, raw, sizeof(raw)};
	overlays[1] = (struct yt_config_overlay){YT_F105 + 2U, second,
	    sizeof(second)};
	for (byte = 0U; byte < YT_RECORD_SIZE; ++byte)
		tape.durable.bytes[byte] = (uint8_t)(byte * 13U + 7U);
	tape.reads = 0U;
	tape.writes = 0U;
	tape.fail_at = 1U;
	yt_error_clear(&error);
	if (yt_config_apply_overlays(&state, overlays, 1U, &ops, &tape,
	    &error) || error.status != YT_IO_ERROR
	    || state.attempted != YT_CONFIG_OVERLAY_READ
	    || state.field_loaded || state.overlays_completed != 0U
	    || state.write_complete)
		return false;
	tape.reads = 0U;
	tape.writes = 0U;
	tape.fail_at = 2U;
	yt_error_clear(&error);
	if (yt_config_apply_overlays(&state, overlays, 1U, &ops, &tape,
	    &error) || error.status != YT_IO_ERROR
	    || state.attempted != YT_CONFIG_OVERLAY_WRITE
	    || !state.field_loaded || state.overlays_completed != 1U
	    || state.write_complete
	    || memcmp(state.field.bytes + YT_F105, raw, sizeof(raw)) != 0)
		return false;
	tape.durable = loaded;
	tape.reads = 0U;
	tape.writes = 0U;
	tape.fail_at = 0U;
	yt_error_clear(&error);
	if (!yt_config_apply_loaded_overlays(&state, &loaded, overlays, 1U,
	    &ops, &tape, &error) || !state.complete || tape.reads != 0U
	    || tape.writes != 1U || state.overlays_completed != 1U
	    || memcmp(tape.durable.bytes + YT_F105, raw, sizeof(raw)) != 0)
		return false;
	tape.durable = loaded;
	tape.reads = 0U;
	tape.writes = 0U;
	tape.fail_at = 1U;
	yt_error_clear(&error);
	if (yt_config_apply_loaded_overlays(&state, &loaded, overlays, 1U,
	    &ops, &tape, &error) || error.status != YT_IO_ERROR
	    || state.attempted != YT_CONFIG_OVERLAY_WRITE
	    || !state.field_loaded || state.overlays_completed != 1U
	    || state.write_complete || tape.reads != 0U || tape.writes != 1U
	    || memcmp(&tape.durable, &loaded, sizeof(loaded)) != 0)
		return false;
	return !yt_config_apply_overlays(NULL, overlays, 1U, &ops, &tape,
	    &error)
	    && !yt_config_apply_overlays(&state, NULL, 1U, &ops, &tape,
	    &error)
	    && !yt_config_apply_overlays(&state, overlays, 1U, NULL, &tape,
	    &error)
	    && !yt_config_apply_overlays(&state,
	    &(struct yt_config_overlay){YT_RECORD_SIZE, raw, 1U}, 1U,
	    &ops, &tape, &error)
	    && !yt_config_apply_loaded_overlays(NULL, &loaded, overlays, 1U,
	    &ops, &tape, &error)
	    && !yt_config_apply_loaded_overlays(&state, NULL, overlays, 1U,
	    &ops, &tape, &error);
}

static bool
test_ytconfig_local_screen_transaction(void)
{
	static const struct yt_config_record_ops ops = {
		config_local_screen_test_read,
		config_local_screen_test_write,
	};
	struct config_local_screen_test_tape tape;
	struct yt_config_local_screen_state state;
	struct yt_record original;
	struct yt_error error;
	unsigned byte;

	for (byte = 0U; byte < YT_RECORD_SIZE; ++byte)
		original.bytes[byte] = (uint8_t)(byte * 11U + 3U);
	(void)yt_record_set_number(&original, YT_F85, 0.6f);
	tape = (struct config_local_screen_test_tape){.durable = original};
	yt_error_clear(&error);
	if (!yt_config_toggle_local_screen(&state, &ops, &tape, &error)
	    || !state.complete
	    || state.attempted != YT_CONFIG_LOCAL_SCREEN_NONE
	    || !state.field_loaded || !state.overlay_complete
	    || !state.write_complete || state.stored != 0.6f
	    || state.toggled != -2.0f || tape.reads != 1U || tape.writes != 1U
	    || yt_record_get_number(&tape.durable, YT_F85) != -2.0f
	    || memcmp(tape.durable.bytes, original.bytes, YT_F85) != 0
	    || memcmp(tape.durable.bytes + YT_F89, original.bytes + YT_F89,
	    YT_RECORD_SIZE - YT_F89) != 0)
		return false;
	tape = (struct config_local_screen_test_tape){
		.durable = original,
		.fail_at = 1U,
	};
	yt_error_clear(&error);
	if (yt_config_toggle_local_screen(&state, &ops, &tape, &error)
	    || error.status != YT_IO_ERROR
	    || state.attempted != YT_CONFIG_LOCAL_SCREEN_READ
	    || state.field_loaded || state.overlay_complete || state.write_complete
	    || memcmp(&tape.durable, &original, sizeof(original)) != 0)
		return false;
	tape = (struct config_local_screen_test_tape){
		.durable = original,
		.fail_at = 2U,
	};
	yt_error_clear(&error);
	if (yt_config_toggle_local_screen(&state, &ops, &tape, &error)
	    || error.status != YT_IO_ERROR
	    || state.attempted != YT_CONFIG_LOCAL_SCREEN_WRITE
	    || !state.field_loaded || !state.overlay_complete
	    || state.write_complete || state.toggled != -2.0f
	    || yt_record_get_number(&state.field, YT_F85) != -2.0f
	    || memcmp(&tape.durable, &original, sizeof(original)) != 0)
		return false;
	(void)yt_record_set_number(&tape.durable, YT_F85, 0.0f);
	tape.fail_at = 0U;
	tape.reads = 0U;
	tape.writes = 0U;
	yt_error_clear(&error);
	if (!yt_config_toggle_local_screen(&state, &ops, &tape, &error)
	    || state.toggled != -1.0f)
		return false;
	(void)yt_record_set_number(&tape.durable, YT_F85, -1.0f);
	tape.reads = 0U;
	tape.writes = 0U;
	yt_error_clear(&error);
	return yt_config_toggle_local_screen(&state, &ops, &tape, &error)
	    && state.toggled == 0.0f
	    && !yt_config_toggle_local_screen(NULL, &ops, &tape, &error)
	    && !yt_config_toggle_local_screen(&state, NULL, &tape, &error);
}

static bool
test_ytconfig_redraw_repair_transaction(void)
{
	static const struct yt_config_record_ops ops = {
		config_local_screen_test_read,
		config_local_screen_test_write,
	};
	struct config_local_screen_test_tape tape;
	struct yt_config_redraw_repair_state state;
	struct yt_record original;
	struct yt_record holds;
	struct yt_record repaired;
	struct yt_error error;
	unsigned byte;

	for (byte = 0U; byte < YT_RECORD_SIZE; ++byte)
		original.bytes[byte] = (uint8_t)(byte * 17U + 9U);
	if (!yt_record_set_number(&original, YT_F73, 50.0f)
	    || !yt_record_set_number(&original, YT_F117, 0.0f))
		return false;
	holds = original;
	repaired = original;
	if (!yt_record_set_number(&holds, YT_F73, 20.0f)
	    || !yt_record_set_number(&repaired, YT_F73, 20.0f)
	    || !yt_record_set_number(&repaired, YT_F117, 85.0f))
		return false;
	tape = (struct config_local_screen_test_tape){.durable = original};
	yt_error_clear(&error);
	if (!yt_config_redraw_repairs(&state, &original, 20.0f, &ops, &tape,
	    &error) || !state.complete
	    || state.attempted != YT_CONFIG_REDRAW_REPAIR_NONE
	    || !state.holds_repaired || !state.headquarters_repaired
	    || !state.field_loaded || state.reads_completed != 2U
	    || state.writes_completed != 2U || tape.reads != 2U
	    || tape.writes != 2U
	    || memcmp(&state.field, &repaired, sizeof(repaired)) != 0
	    || memcmp(&tape.durable, &repaired, sizeof(repaired)) != 0)
		return false;
	tape = (struct config_local_screen_test_tape){
		.durable = original,
		.fail_at = 1U,
	};
	yt_error_clear(&error);
	if (yt_config_redraw_repairs(&state, &original, 20.0f, &ops, &tape,
	    &error) || error.status != YT_IO_ERROR
	    || state.attempted != YT_CONFIG_REDRAW_REPAIR_WRITE_HOLDS
	    || state.holds_repaired || state.writes_completed != 0U
	    || memcmp(&tape.durable, &original, sizeof(original)) != 0)
		return false;
	tape = (struct config_local_screen_test_tape){
		.durable = original,
		.fail_at = 2U,
	};
	yt_error_clear(&error);
	if (yt_config_redraw_repairs(&state, &original, 20.0f, &ops, &tape,
	    &error) || error.status != YT_IO_ERROR
	    || state.attempted != YT_CONFIG_REDRAW_REPAIR_READ_MENU
	    || !state.holds_repaired || state.field_loaded
	    || state.reads_completed != 0U || state.writes_completed != 1U
	    || memcmp(&tape.durable, &holds, sizeof(holds)) != 0)
		return false;
	tape = (struct config_local_screen_test_tape){
		.durable = original,
		.fail_at = 3U,
	};
	yt_error_clear(&error);
	if (yt_config_redraw_repairs(&state, &original, 20.0f, &ops, &tape,
	    &error) || error.status != YT_IO_ERROR
	    || state.attempted !=
	    YT_CONFIG_REDRAW_REPAIR_WRITE_HEADQUARTERS
	    || !state.holds_repaired || state.headquarters_repaired
	    || !state.field_loaded || state.reads_completed != 1U
	    || state.writes_completed != 1U
	    || memcmp(&state.field, &repaired, sizeof(repaired)) != 0
	    || memcmp(&tape.durable, &holds, sizeof(holds)) != 0)
		return false;
	tape = (struct config_local_screen_test_tape){
		.durable = original,
		.fail_at = 4U,
	};
	yt_error_clear(&error);
	if (yt_config_redraw_repairs(&state, &original, 20.0f, &ops, &tape,
	    &error) || error.status != YT_IO_ERROR
	    || state.attempted !=
	    YT_CONFIG_REDRAW_REPAIR_READ_HEADQUARTERS
	    || !state.holds_repaired || !state.headquarters_repaired
	    || state.field_loaded || state.reads_completed != 1U
	    || state.writes_completed != 2U
	    || memcmp(&tape.durable, &repaired, sizeof(repaired)) != 0)
		return false;
	if (!yt_record_set_number(&original, YT_F73, 10.0f)
	    || !yt_record_set_number(&original, YT_F117, 85.0f))
		return false;
	tape = (struct config_local_screen_test_tape){.durable = original};
	yt_error_clear(&error);
	return yt_config_redraw_repairs(&state, &original, 20.0f, &ops, &tape,
	    &error) && state.complete && !state.holds_repaired
	    && !state.headquarters_repaired && state.reads_completed == 1U
	    && state.writes_completed == 0U && tape.reads == 1U
	    && tape.writes == 0U
	    && !yt_config_redraw_repairs(NULL, &original, 20.0f, &ops, &tape,
	    &error)
	    && !yt_config_redraw_repairs(&state, NULL, 20.0f, &ops, &tape,
	    &error)
	    && !yt_config_redraw_repairs(&state, &original, 20.0f, NULL, &tape,
	    &error);
}

static bool
test_ytconfig_headquarters(struct yt_error *error)
{
	static const uint8_t raw_clear[4] = {0x00, 0x00, 0x80, 0x00};
	static const char input[] = "H8.6\nX";
	static const uint8_t command_prefix[] = "H\r\r";
	static const uint8_t stored_row[] =
	    "<H> Xannor Headquarters is in: 8.6 \r";
	struct yt_game game;
	struct yt_game restore;
	struct yt_config original_config;
	struct yt_sector original_old;
	struct yt_sector original_candidate;
	struct yt_sector original_one;
	struct yt_sector forced_candidate;
	struct yt_sector actual_old;
	struct yt_sector actual_candidate;
	struct yt_sector actual_one;
	struct yt_record expected_old;
	struct yt_record expected_candidate;
	struct yt_record expected_one;
	struct yt_record expected_config;
	struct yt_config_output_result prompt;
	uint8_t prompt_fragment[256];
	uint8_t *screen = NULL;
	size_t screen_length = 0U;
	size_t prompt_length;
	float merged;
	float planet_link;
	bool overflow;
	bool snapshots = false;
	bool valid = false;
	int old_number;
	const int candidate_number = 9;

	memset(&game, 0, sizeof(game));
	if (!yt_game_open(&game, YT_OPEN_UPDATE, error))
		goto done;
	original_config = game.config;
	old_number = (int)qb_cint(original_config.headquarters, &overflow);
	if (overflow || old_number == 1 || old_number == candidate_number
	    || !yt_game_read_sector(&game, old_number, &original_old, error)
	    || !yt_game_read_sector(&game, candidate_number,
		&original_candidate, error)
	    || !yt_game_read_sector(&game, 1, &original_one, error))
		goto done;
	snapshots = true;
	forced_candidate = original_candidate;
	forced_candidate.planet = 0.0f;
	forced_candidate.fighters = 2.0f;
	forced_candidate.fighter_owner = -1.0f;
	if (!yt_game_write_sector(&game, candidate_number, &forced_candidate,
	    error))
		goto done;
	if (!yt_config_compose_hq_prompt(original_config.headquarters,
	    original_config.port_offset - original_config.sector_offset, 0U,
	    &prompt))
		goto done;
	prompt_length = sizeof(command_prefix) - 1U + prompt.output_length;
	if (prompt_length > sizeof(prompt_fragment))
		goto done;
	memcpy(prompt_fragment, command_prefix, sizeof(command_prefix) - 1U);
	memcpy(prompt_fragment + sizeof(command_prefix) - 1U, prompt.output,
	    prompt.output_length);
	yt_game_close(&game);
	if (!write_file("config.in", input, sizeof(input) - 1U)
	    || !run_redirected(YT_CONFIG_EXE, "config.in", "config.out")
	    || !read_file("config.out", &screen, &screen_length)
	    || !bytes_contain(screen, screen_length, prompt_fragment,
		prompt_length)
	    || !bytes_contain(screen, screen_length, stored_row,
		sizeof(stored_row) - 1U))
		goto done_closed;
	memset(&game, 0, sizeof(game));
	if (!yt_game_open(&game, YT_OPEN_READ, error)
	    || !yt_game_read_sector(&game, old_number, &actual_old, error)
	    || !yt_game_read_sector(&game, candidate_number, &actual_candidate,
		error)
	    || !yt_game_read_sector(&game, 1, &actual_one, error))
		goto done;
	merged = original_old.fighters + forced_candidate.fighters;
	planet_link = original_config.total_records
	    - original_config.planet_offset;
	expected_old = original_old.record;
	yt_record_set_raw_number(&expected_old, YT_F93, raw_clear);
	yt_record_set_raw_number(&expected_old, YT_F85, raw_clear);
	yt_record_set_raw_number(&expected_old, YT_F81, raw_clear);
	expected_candidate = forced_candidate.record;
	expected_one = original_one.record;
	expected_config = original_config.record;
	valid = yt_record_set_number(&expected_candidate, YT_F85, -1.0f)
	    && yt_record_set_number(&expected_candidate, YT_F81, merged)
	    && yt_record_set_number(&expected_candidate, YT_F93, planet_link)
	    && yt_record_set_number(&expected_one, YT_F105, 8.6f)
	    && yt_record_set_number(&expected_config, YT_F117, 8.6f)
	    && memcmp(actual_old.record.bytes, expected_old.bytes,
		YT_RECORD_SIZE) == 0
	    && memcmp(actual_candidate.record.bytes, expected_candidate.bytes,
		YT_RECORD_SIZE) == 0
	    && memcmp(actual_one.record.bytes, expected_one.bytes,
		YT_RECORD_SIZE) == 0
	    && memcmp(game.config.record.bytes, expected_config.bytes,
		YT_RECORD_SIZE) == 0;

done:
	yt_game_close(&game);
done_closed:
	if (snapshots) {
		memset(&restore, 0, sizeof(restore));
		if (!yt_game_open(&restore, YT_OPEN_UPDATE, error)
		    || !yt_database_write(&restore.database, 1,
			&original_config.record, error)
		    || !yt_database_write(&restore.database,
			(size_t)yt_sector_basic_record(&original_config, old_number),
			&original_old.record, error)
		    || !yt_database_write(&restore.database,
			(size_t)yt_sector_basic_record(&original_config,
			candidate_number), &original_candidate.record, error)
		    || !yt_database_write(&restore.database,
			(size_t)yt_sector_basic_record(&original_config, 1),
			&original_one.record, error)
		    || !yt_database_flush(&restore.database, error))
			valid = false;
		yt_game_close(&restore);
	}
	free(screen);
	return valid;
}

static bool
test_ytconfig_scalar_options(struct yt_error *error)
{
	static const char input[] =
	    "Ixxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n"
	    "I\n"
	    "J"
	    "K\nK10\n"
	    "A\nA4\nA1000\n"
	    "B\nB99\nB1000\n"
	    "C\nC-1\nC0\n"
	    "D\nD1\nD25.5\n"
	    "E\nE-1\nE1000\n"
	    "F\nF0\nF123.5junk\n"
	    "G\nGx\ny\n"
	    "X";
	static const uint8_t scoreboard[] =
	    "I\r\r"
	    "Enter new scoreboard and path or hit ENTER for 'YTSCORE.ASC'.\r";
	static const uint8_t too_long[] = "Too long! 41 chars max!!\r";
	static const uint8_t lottery_invalid[] =
	    "K\r\rHow many times per day may a user play the lottery? "
	    "(0 - 9) -=> Range is 1 to 10!\r";
	static const uint8_t maximum_silent[] =
	    "A\r\rWhat is the Maximum amount of Cargo Holds allowed? "
	    "(5 - 1000) -=> Yankee Trader Configuration Program\r";
	static const uint8_t turns_invalid[] =
	    "B\r\rTurns allowed per day? (100 - 1000) "
	    "\r Invalid Range!\r";
	static const uint8_t fighters_invalid[] =
	    "C\r\rStarting Number of Fighters? (1 to 10,000) -=> "
	    "\rInvalid Range!\r";
	static const uint8_t credits_silent[] =
	    "D\r\rStarting credits? (25 to 10,000) -=> "
	    "Yankee Trader Configuration Program\r";
	static const uint8_t holds_invalid[] =
	    "E\r\rStarting Amount of Holds? (1 to  1000 ) -=> "
	    "\r Invalid Range!\r";
	static const uint8_t dead_silent[] =
	    "F\r\rDays until deleted? "
	    "Yankee Trader Configuration Program\r";
	static const uint8_t maintenance_reprompt[] =
	    "G\r\rOK to Run Maintenence [Y/N] -=> "
	    "OK to Run Maintenence [Y/N] -=> ";
	static const uint8_t ending[] = "X\r\r-=* End of Run *=-\r";
	static const uint8_t raw_allow[4] = {0x00, 0x00, 0x7d, 0x00};
	struct yt_game game;
	struct yt_game restore;
	struct yt_record original;
	struct yt_record baseline;
	struct yt_record expected;
	uint8_t *screen = NULL;
	size_t screen_length = 0U;
	bool snapshot = false;
	bool valid = false;

	memset(&game, 0, sizeof(game));
	if (!yt_game_open(&game, YT_OPEN_UPDATE, error))
		goto done;
	original = game.config.record;
	snapshot = true;
	snprintf(game.config.scoreboard, sizeof(game.config.scoreboard),
	    "OLD.ASC");
	game.config.local_screen = 0.0f;
	game.config.lottery_plays = 3.0f;
	game.config.maximum_holds = 200.0f;
	game.config.turns_per_day = 500.0f;
	game.config.initial_fighters = 500.0f;
	game.config.initial_credits = 1000.0f;
	game.config.initial_holds = 20.0f;
	game.config.retention_days = 30.0f;
	game.config.last_maintenance = -1.0f;
	if (game.config.headquarters == 0.0f)
		game.config.headquarters = 85.0f;
	if (!yt_config_store(&game.database, &game.config, error)
	    || !yt_database_flush(&game.database, error)
	    || !yt_config_load(&game.database, &game.config, error))
		goto done;
	baseline = game.config.record;
	yt_game_close(&game);
	if (!write_file("config.in", input, sizeof(input) - 1U)
	    || !run_redirected(YT_CONFIG_EXE, "config.in", "config.out")
	    || !read_file("config.out", &screen, &screen_length)
	    || !bytes_contain(screen, screen_length, scoreboard,
		sizeof(scoreboard) - 1U)
	    || !bytes_contain(screen, screen_length, too_long,
		sizeof(too_long) - 1U)
	    || !bytes_contain(screen, screen_length, lottery_invalid,
		sizeof(lottery_invalid) - 1U)
	    || !bytes_contain(screen, screen_length, maximum_silent,
		sizeof(maximum_silent) - 1U)
	    || !bytes_contain(screen, screen_length, turns_invalid,
		sizeof(turns_invalid) - 1U)
	    || !bytes_contain(screen, screen_length, fighters_invalid,
		sizeof(fighters_invalid) - 1U)
	    || !bytes_contain(screen, screen_length, credits_silent,
		sizeof(credits_silent) - 1U)
	    || !bytes_contain(screen, screen_length, holds_invalid,
		sizeof(holds_invalid) - 1U)
	    || !bytes_contain(screen, screen_length, dead_silent,
		sizeof(dead_silent) - 1U)
	    || !bytes_contain(screen, screen_length, maintenance_reprompt,
		sizeof(maintenance_reprompt) - 1U)
	    || !bytes_contain(screen, screen_length, ending,
		sizeof(ending) - 1U))
		goto done_closed;
	memset(&game, 0, sizeof(game));
	if (!yt_game_open(&game, YT_OPEN_READ, error))
		goto done;
	expected = baseline;
	yt_record_set_text(&expected, (const uint8_t *)"YTSCORE.ASC", 11U);
	valid = yt_record_set_number(&expected, YT_F41, 11.0f)
	    && yt_record_set_number(&expected, YT_F85, -1.0f)
	    && yt_record_set_number(&expected, YT_F101, 0.0f)
	    && yt_record_set_number(&expected, YT_F121, 1000.0f)
	    && yt_record_set_number(&expected, YT_F49, 1000.0f)
	    && yt_record_set_number(&expected, YT_F65, 0.0f)
	    && yt_record_set_number(&expected, YT_F69, 25.5f)
	    && yt_record_set_number(&expected, YT_F73, 1000.0f)
	    && yt_record_set_number(&expected, YT_F77, 123.5f)
	    && yt_record_set_number(&expected, YT_F45,
		game.config.epoch_year);
	yt_record_set_raw_number(&expected, YT_F81, raw_allow);
	valid = valid && game.config.last_maintenance == 0.0f
	    && memcmp(game.config.record.bytes, expected.bytes,
		YT_RECORD_SIZE) == 0;

done:
	yt_game_close(&game);
done_closed:
	if (snapshot) {
		memset(&restore, 0, sizeof(restore));
		if (!yt_game_open(&restore, YT_OPEN_UPDATE, error)
		    || !yt_database_write(&restore.database, 1, &original, error)
		    || !yt_database_flush(&restore.database, error))
			valid = false;
		yt_game_close(&restore);
	}
	free(screen);
	return valid;
}

static bool
test_ytconfig_planet_editor(struct yt_error *error)
{
	static const char empty_input[] = "PX";
	static const char input[] =
	    "P"
	    "Lz"
	    "C76\n"
	    "C3\n"
	    "C1\n"
	    "C1.4\n\n"
	    "C2\nmars base\nxnnew world\nyz"
	    "X";
	static const uint8_t empty_entry[] =
	    "P\r\r"
	    "Loading planet names...\r\r"
	    "There are 0  planets in your game.\r\r"
	    "YOUR GAME HAS NO PLANETS!\r\a";
	static const uint8_t entry[] =
	    "P\r\r"
	    "Loading planet names...\r\r"
	    "There are 4  planets in your game.\r";
	static const uint8_t list[] =
	    "L\r\r"
	    "  #   Name"
	    "-------------------------------------------------------------------------------\r"
	    "  1 : The Wanderer\r"
	    "  2 : Earth\r"
	    " 20 : Twenty\r"
	    "[ Pause ]\r"
	    " 75 : Legacy Slot\r\r";
	static const uint8_t range_invalid[] =
	    "C\r\rEdit which planet number? "
	    "\rINVALID PLANET NUMBER!!\r76\r\a";
	static const uint8_t slot_invalid[] =
	    "C\r\rEdit which planet number? "
	    "\rINVALID PLANET NUMBER!!\r3\r\a";
	static const uint8_t protected[] =
	    "C\r\rEdit which planet number? "
	    "The planets \"The Wanderer\" and \"Xannoron\" "
	    "cannot be re-named!\r\r\a";
	static const uint8_t bypass[] =
	    "C\r\rEdit which planet number? "
	    "Editing: The Wanderer\r\r"
	    "Press enter to quit.\r"
	    "The Wanderer\r"
	    "Please enter new name. -=> \r";
	static const uint8_t canceled[] =
	    "Editing: Earth\r\r"
	    "Press enter to quit.\r"
	    "Earth\r"
	    "Please enter new name. -=> "
	    "\rChange name to Mars Base? [Y/N] -=> X\r"
	    "\rChange name to Mars Base? [Y/N] -=> N\r"
	    "\rCanceled!\rMars Base\r"
	    "Editing: Earth\r\r";
	static const uint8_t saved[] =
	    "\rChange name to New World? [Y/N] -=> Y\r"
	    "New name saved! Press any key.\r";
	struct yt_game game;
	struct yt_game restore;
	struct yt_record originals[75];
	struct yt_record forced[75];
	struct yt_record expected;
	struct yt_planet actual;
	uint8_t *screen = NULL;
	size_t screen_length = 0U;
	bool snapshots = false;
	bool valid = false;
	int logical;

	memset(&game, 0, sizeof(game));
	if (!yt_game_open(&game, YT_OPEN_UPDATE, error))
		goto done;
	for (logical = 1; logical <= 75; ++logical) {
		size_t record = (size_t)yt_planet_basic_record(&game.config,
		    logical);

		if (!yt_database_read(&game.database, record,
		    &originals[logical - 1], error))
			goto done;
	}
	snapshots = true;
	for (logical = 1; logical <= 75; ++logical) {
		size_t record = (size_t)yt_planet_basic_record(&game.config,
		    logical);

		forced[logical - 1] = originals[logical - 1];
		yt_record_set_text(&forced[logical - 1], NULL, 0U);
		if (!yt_record_set_number(&forced[logical - 1], YT_F85, 0.0f)
		    || !yt_database_write(&game.database, record,
			&forced[logical - 1], error))
			goto done;
	}
	if (!yt_database_flush(&game.database, error))
		goto done;
	yt_game_close(&game);
	if (!write_file("config.in", empty_input, sizeof(empty_input) - 1U)
	    || !run_redirected(YT_CONFIG_EXE, "config.in", "config.out")
	    || !read_file("config.out", &screen, &screen_length)
	    || !bytes_contain(screen, screen_length, empty_entry,
		sizeof(empty_entry) - 1U))
		goto done_closed;
	free(screen);
	screen = NULL;
	memset(&game, 0, sizeof(game));
	if (!yt_game_open(&game, YT_OPEN_UPDATE, error))
		goto done;
#define SET_ACTIVE_PLANET(number, text) do { \
	struct yt_record *record = &forced[(number) - 1]; \
	yt_record_set_text(record, (const uint8_t *)(text), strlen(text)); \
	if (!yt_record_set_number(record, YT_F85, (float)strlen(text)) \
	    || !yt_database_write(&game.database, \
		(size_t)yt_planet_basic_record(&game.config, (number)), \
		record, error)) \
		goto done; \
} while (0)
	SET_ACTIVE_PLANET(1, "The Wanderer");
	SET_ACTIVE_PLANET(2, "Earth");
	SET_ACTIVE_PLANET(20, "Twenty");
	SET_ACTIVE_PLANET(75, "Legacy Slot");
#undef SET_ACTIVE_PLANET
	if (!yt_database_flush(&game.database, error))
		goto done;
	yt_game_close(&game);
	if (!write_file("config.in", input, sizeof(input) - 1U)
	    || !run_redirected(YT_CONFIG_EXE, "config.in", "config.out")
	    || !read_file("config.out", &screen, &screen_length)
	    || !bytes_contain(screen, screen_length, entry,
		sizeof(entry) - 1U)
	    || !bytes_contain(screen, screen_length, list,
		sizeof(list) - 1U)
	    || !bytes_contain(screen, screen_length, range_invalid,
		sizeof(range_invalid) - 1U)
	    || !bytes_contain(screen, screen_length, slot_invalid,
		sizeof(slot_invalid) - 1U)
	    || !bytes_contain(screen, screen_length, protected,
		sizeof(protected) - 1U)
	    || !bytes_contain(screen, screen_length, bypass,
		sizeof(bypass) - 1U)
	    || !bytes_contain(screen, screen_length, canceled,
		sizeof(canceled) - 1U)
	    || !bytes_contain(screen, screen_length, saved,
		sizeof(saved) - 1U))
		goto done_closed;
	memset(&game, 0, sizeof(game));
	if (!yt_game_open(&game, YT_OPEN_READ, error)
	    || !yt_game_read_planet(&game, 2, &actual, error))
		goto done;
	expected = forced[1];
	yt_record_set_text(&expected, (const uint8_t *)"New World", 9U);
	valid = yt_record_set_number(&expected, YT_F85, 9.0f)
	    && memcmp(actual.record.bytes, expected.bytes, YT_RECORD_SIZE) == 0;
	for (logical = 1; valid && logical <= 75; ++logical) {
		struct yt_record unchanged;

		if (logical == 2)
			continue;
		if (!yt_database_read(&game.database,
		    (size_t)yt_planet_basic_record(&game.config, logical),
		    &unchanged, error)
		    || memcmp(unchanged.bytes, forced[logical - 1].bytes,
			YT_RECORD_SIZE) != 0)
			valid = false;
	}

done:
	yt_game_close(&game);
done_closed:
	if (snapshots) {
		memset(&restore, 0, sizeof(restore));
		if (!yt_game_open(&restore, YT_OPEN_UPDATE, error))
			valid = false;
		else {
			for (logical = 1; logical <= 75; ++logical) {
				if (!yt_database_write(&restore.database,
				    (size_t)yt_planet_basic_record(&restore.config,
				    logical), &originals[logical - 1], error)) {
					valid = false;
					break;
				}
			}
			if (!yt_database_flush(&restore.database, error))
				valid = false;
		}
		yt_game_close(&restore);
	}
	free(screen);
	return valid;
}

static bool
test_ytconfig_port_editor(struct yt_error *error)
{
	static const char blank_input[] = "O\nX";
	static const char missing_input[] = "Omissing\nX";
	static const char end_input[] = "Oalpha\nnn\nX";
	static const char cancel_input[] = "Oalpha\nynew name\nxn\nX";
	static const char empty_input[] = "Oalpha\ny\nX";
	static const char save_input[] = "Oalpha\nynew harbor\ny\nX";
	static const uint8_t blank[] =
	    "O\r\rPress enter to quit.\r\r"
	    "Enter port name to change (Search String) -+> \r";
	static const uint8_t missing[] =
	    "O\r\rPress enter to quit.\r\r"
	    "Enter port name to change (Search String) -+> missing\r"
	    "Not Found\r";
	static const uint8_t end[] =
	    "Change \"Alpha\" [Y/N]? N\r\r"
	    "Change \"Alphabet\" [Y/N]? N\r\r"
	    "-= End of List =-\rPress Enter";
	static const uint8_t cancel[] =
	    "Change \"Alpha\" [Y/N]? Y\r\r\r"
	    "Please enter a new name for this port.\r-=> "
	    "\"New Name\" Is this OK? [Y/N]? X\r\r"
	    "\"New Name\" Is this OK? [Y/N]? N\r\r"
	    "CANCELED!\rPress Enter";
	static const uint8_t empty[] =
	    "Change \"Alpha\" [Y/N]? Y\r\r\r"
	    "Please enter a new name for this port.\r-=> ";
	static const uint8_t saved[] =
	    "Change \"Alpha\" [Y/N]? Y\r\r\r"
	    "Please enter a new name for this port.\r-=> "
	    "\"New Harbor\" Is this OK? [Y/N]? Y\r\r"
	    "Name change successful!!!\rPress Enter";
	struct yt_game game;
	struct yt_game restore;
	struct yt_record originals[299];
	struct yt_record forced[299];
	struct yt_record expected;
	struct yt_port actual;
	uint8_t *screen = NULL;
	size_t screen_length = 0U;
	bool snapshots = false;
	bool valid = false;
	int logical;

	memset(&game, 0, sizeof(game));
	if (!yt_game_open(&game, YT_OPEN_UPDATE, error))
		goto done;
	for (logical = 2; logical <= 300; ++logical) {
		if (!yt_database_read(&game.database,
		    (size_t)yt_port_basic_record(&game.config, logical),
		    &originals[logical - 2], error))
			goto done;
	}
	snapshots = true;
	for (logical = 2; logical <= 300; ++logical) {
		forced[logical - 2] = originals[logical - 2];
		yt_record_set_text(&forced[logical - 2],
		    (const uint8_t *)"Ordinary", 8U);
		if (!yt_record_set_number(&forced[logical - 2], YT_F85, 8.0f)
		    || !yt_database_write(&game.database,
			(size_t)yt_port_basic_record(&game.config, logical),
			&forced[logical - 2], error))
			goto done;
	}
	yt_record_set_text(&forced[0], (const uint8_t *)"Alpha", 5U);
	yt_record_set_text(&forced[1], (const uint8_t *)"Alphabet", 8U);
	if (!yt_record_set_number(&forced[0], YT_F85, 5.0f)
	    || !yt_record_set_number(&forced[1], YT_F85, 8.0f)
	    || !yt_database_write(&game.database,
		(size_t)yt_port_basic_record(&game.config, 2), &forced[0], error)
	    || !yt_database_write(&game.database,
		(size_t)yt_port_basic_record(&game.config, 3), &forced[1], error)
	    || !yt_database_flush(&game.database, error))
		goto done;
	yt_game_close(&game);

#define RUN_PORT_CASE(input_name, fragment) do { \
	if (!write_file("config.in", (input_name), sizeof(input_name) - 1U) \
	    || !run_redirected(YT_CONFIG_EXE, "config.in", "config.out") \
	    || !read_file("config.out", &screen, &screen_length) \
	    || !bytes_contain(screen, screen_length, (fragment), \
		sizeof(fragment) - 1U)) \
		goto done_closed; \
	free(screen); \
	screen = NULL; \
} while (0)
	RUN_PORT_CASE(blank_input, blank);
	RUN_PORT_CASE(missing_input, missing);
	RUN_PORT_CASE(end_input, end);
	RUN_PORT_CASE(cancel_input, cancel);
	RUN_PORT_CASE(empty_input, empty);
	memset(&game, 0, sizeof(game));
	if (!yt_game_open(&game, YT_OPEN_READ, error)
	    || !yt_game_read_port(&game, 2, &actual, error)
	    || memcmp(actual.record.bytes, forced[0].bytes,
		YT_RECORD_SIZE) != 0)
		goto done;
	yt_game_close(&game);
	RUN_PORT_CASE(save_input, saved);
#undef RUN_PORT_CASE
	memset(&game, 0, sizeof(game));
	if (!yt_game_open(&game, YT_OPEN_READ, error)
	    || !yt_game_read_port(&game, 2, &actual, error))
		goto done;
	expected = forced[0];
	yt_record_set_text(&expected, (const uint8_t *)"New Harbor", 10U);
	valid = yt_record_set_number(&expected, YT_F85, 10.0f)
	    && memcmp(actual.record.bytes, expected.bytes, YT_RECORD_SIZE) == 0;
	for (logical = 3; valid && logical <= 300; ++logical) {
		struct yt_record unchanged;

		if (!yt_database_read(&game.database,
		    (size_t)yt_port_basic_record(&game.config, logical),
		    &unchanged, error)
		    || memcmp(unchanged.bytes, forced[logical - 2].bytes,
			YT_RECORD_SIZE) != 0)
			valid = false;
	}

done:
	yt_game_close(&game);
done_closed:
	if (snapshots) {
		memset(&restore, 0, sizeof(restore));
		if (!yt_game_open(&restore, YT_OPEN_UPDATE, error))
			valid = false;
		else {
			for (logical = 2; logical <= 300; ++logical) {
				if (!yt_database_write(&restore.database,
				    (size_t)yt_port_basic_record(&restore.config,
				    logical), &originals[logical - 2], error)) {
					valid = false;
					break;
				}
			}
			if (!yt_database_flush(&restore.database, error))
				valid = false;
		}
		yt_game_close(&restore);
	}
	free(screen);
	return valid;
}

static bool
test_ytconfig_alias_editor(struct yt_error *error)
{
	static const uint8_t sentinel[] =
	    "Dummy,Dummy,Dummy,Dummy\r\n\x1a";
	static const uint8_t names[] =
	    "Dummy,Dummy,Dummy,Dummy\r\n"
	    "One,Real,Ace,Pilot\r\n"
	    "Two,Real,Beta,Tester\r\n\x1a";
	static const uint8_t changed_names[] =
	    "Dummy,Dummy,Dummy,Dummy\r\n"
	    "One,Real,New,Captain\r\n"
	    "Two,Real,Beta,Tester\r\n\x1a";
	static const char empty_input[] = "NX";
	static const char input[] =
	    "N"
	    "Lz"
	    "C2.1\n"
	    "C0\n"
	    "C1.5\nnew, CAPTAIN\nxn\n"
	    "C1\n,\n"
	    "C1\nnew, CAPTAIN\nxyq"
	    "X";
	static const uint8_t empty[] =
	    "N\r\rLoading names...\r\r"
	    "There are 0  players in your game.\r\r"
	    "YOUR GAME HAS NO PLAYERS!\r\a";
	static const uint8_t entry[] =
	    "N\r\rLoading names...\r\r"
	    "There are 2  players in your game.\r";
	static const uint8_t list[] =
	    "L\r\r"
	    "  #   Real NameAlias\r"
	    "============================================================================"
	    "===\r"
	    "  1 : One RealAce Pilot\r"
	    "  2 : Two RealBeta Tester\r"
	    "[ Pause ]\r\r";
	static const uint8_t invalid[] =
	    "C\r\rEdit which player number? "
	    "\rINVALID PLAYER NUMBER!!\r2.1\r\a";
	static const uint8_t fractional_cancel[] =
	    "C\r\rEdit which player number? \r"
	    "Editing: Two Real a.k.a. Beta Tester\r\r"
	    "Press enter to quit.\r\rPlease enter new Alias. -=> "
	    "\rChange player Alias to \"New Captain\"? [Y/N] -=> X\r"
	    "\rChange player Alias to \"New Captain\"? [Y/N] -=> N\r\r"
	    "Canceled!\r\r"
	    "Editing: Two Real a.k.a. Beta Tester\r";
	static const uint8_t saved[] =
	    "\rChange player Alias to \"New Captain\"? [Y/N] -=> X\r"
	    "\rChange player Alias to \"New Captain\"? [Y/N] -=> Y\r\r"
	    "New alias saved! Press any key.\r";
	struct yt_game game;
	struct yt_game restore;
	struct yt_record originals[50];
	struct yt_record forced[50];
	uint8_t *original_names = NULL;
	uint8_t *actual_names = NULL;
	uint8_t *screen = NULL;
	size_t original_names_length = 0U;
	size_t actual_names_length = 0U;
	size_t screen_length = 0U;
	bool snapshots = false;
	bool valid = false;
	int basic;

	if (!read_file("YTNAME.DAT", &original_names, &original_names_length))
		goto done_closed;
	memset(&game, 0, sizeof(game));
	if (!yt_game_open(&game, YT_OPEN_UPDATE, error))
		goto done;
	for (basic = 2; basic <= 51; ++basic) {
		if (!yt_database_read(&game.database, (size_t)basic,
		    &originals[basic - 2], error))
			goto done;
	}
	snapshots = true;
	for (basic = 2; basic <= 51; ++basic) {
		char ordinary[24];
		int length;

		forced[basic - 2] = originals[basic - 2];
		length = snprintf(ordinary, sizeof(ordinary), "Ordinary %d", basic);
		if (length < 0 || (size_t)length >= sizeof(ordinary))
			goto done;
		yt_record_set_text(&forced[basic - 2],
		    (const uint8_t *)ordinary, (size_t)length);
		if (!yt_record_set_number(&forced[basic - 2], YT_F85,
		    (float)length)
		    || !yt_database_write(&game.database, (size_t)basic,
			&forced[basic - 2], error))
			goto done;
	}
	yt_record_set_text(&forced[0], (const uint8_t *)"Ace Pilot", 9U);
	yt_record_set_text(&forced[1],
	    (const uint8_t *)"The Ace Pilot II", 16U);
	yt_record_set_text(&forced[2], (const uint8_t *)"ace Pilot", 9U);
	yt_record_set_text(&forced[3], (const uint8_t *)"Ace Pilot", 9U);
	if (!yt_record_set_number(&forced[0], YT_F85, 9.0f)
	    || !yt_record_set_number(&forced[1], YT_F85, 16.0f)
	    || !yt_record_set_number(&forced[2], YT_F85, 9.0f)
	    || !yt_record_set_number(&forced[3], YT_F85, 0.0f))
		goto done;
	for (basic = 2; basic <= 5; ++basic) {
		if (!yt_database_write(&game.database, (size_t)basic,
		    &forced[basic - 2], error))
			goto done;
	}
	if (!yt_database_flush(&game.database, error))
		goto done;
	yt_game_close(&game);
	if (!write_file("YTNAME.DAT", sentinel, sizeof(sentinel) - 1U)
	    || !write_file("config.in", empty_input, sizeof(empty_input) - 1U)
	    || !run_redirected(YT_CONFIG_EXE, "config.in", "config.out")
	    || !read_file("config.out", &screen, &screen_length)
	    || !bytes_contain(screen, screen_length, empty, sizeof(empty) - 1U))
		goto done_closed;
	free(screen);
	screen = NULL;
	if (!write_file("YTNAME.DAT", names, sizeof(names) - 1U)
	    || !write_file("config.in", input, sizeof(input) - 1U)
	    || !run_redirected(YT_CONFIG_EXE, "config.in", "config.out")
	    || !read_file("config.out", &screen, &screen_length)
	    || !bytes_contain(screen, screen_length, entry, sizeof(entry) - 1U)
	    || !bytes_contain(screen, screen_length, list, sizeof(list) - 1U)
	    || !bytes_contain(screen, screen_length, invalid,
		sizeof(invalid) - 1U)
	    || !bytes_contain(screen, screen_length, fractional_cancel,
		sizeof(fractional_cancel) - 1U)
	    || !bytes_contain(screen, screen_length, saved, sizeof(saved) - 1U)
	    || !read_file("YTNAME.DAT", &actual_names, &actual_names_length)
	    || actual_names_length != sizeof(changed_names) - 1U
	    || memcmp(actual_names, changed_names, sizeof(changed_names) - 1U)
	    != 0)
		goto done_closed;
	memset(&game, 0, sizeof(game));
	if (!yt_game_open(&game, YT_OPEN_READ, error))
		goto done;
	valid = true;
	for (basic = 2; valid && basic <= 51; ++basic) {
		struct yt_record actual;
		struct yt_record expected = forced[basic - 2];

		if (basic == 2 || basic == 3 || basic == 5) {
			yt_record_set_text(&expected,
			    (const uint8_t *)"New Captain", 11U);
			if (!yt_record_set_number(&expected, YT_F85, 11.0f)) {
				valid = false;
				break;
			}
		}
		if (!yt_database_read(&game.database, (size_t)basic, &actual,
		    error)
		    || memcmp(actual.bytes, expected.bytes, YT_RECORD_SIZE) != 0)
			valid = false;
	}

done:
	yt_game_close(&game);
done_closed:
	if (snapshots) {
		memset(&restore, 0, sizeof(restore));
		if (!yt_game_open(&restore, YT_OPEN_UPDATE, error))
			valid = false;
		else {
			for (basic = 2; basic <= 51; ++basic) {
				if (!yt_database_write(&restore.database,
				    (size_t)basic, &originals[basic - 2], error)) {
					valid = false;
					break;
				}
			}
			if (!yt_database_flush(&restore.database, error))
				valid = false;
		}
		yt_game_close(&restore);
	}
	if (original_names != NULL
	    && !write_file("YTNAME.DAT", original_names,
		original_names_length))
		valid = false;
	free(original_names);
	free(actual_names);
	free(screen);
	return valid;
}

static bool
test_portname(struct yt_error *error)
{
	static const uint8_t intro[] =
	    "\r"
	    "          Yankee Trader Remote Port Rename Program\r"
	    "                     By Alan Davenport\r"
	    "\r\r"
	    "This program will apply new, random port names to an existing game without\r"
	    "effecting any other setting. Do you wish to continue? [y/N] -=> ";
	static const uint8_t blank_suffix[] = "? \r";
	static const uint8_t abort_suffix[] = "? \r\rAborted!\r";
	static const uint8_t success_prefix[] = "? \r\r\rRenaming ports...\r 1 Earth\r";
	static const uint8_t success_suffix[] =
	    "\rNew, random names applied to all ports!\r";
	uint8_t *before = NULL;
	uint8_t *after = NULL;
	uint8_t *output = NULL;
	size_t before_length = 0;
	size_t after_length = 0;
	size_t output_length = 0;
	struct yt_game game;
	FILE *probe;
	int logical;
	static const char input[] = "yes\n";
	static const char blank_input[] = "\n";
	static const char abort_input[] = "N\n";

	if (!read_file("YTDATA.DAT", &before, &before_length))
		return false;
	if (!write_file("portname.in", blank_input, sizeof(blank_input) - 1U)
	    || !run_redirected(YT_PORTNAME_EXE, "portname.in", "portname.out")
	    || !read_file("YTDATA.DAT", &after, &after_length)
	    || !read_file("portname.out", &output, &output_length)
	    || after_length != before_length
	    || memcmp(after, before, before_length) != 0
	    || output_length != sizeof(intro) - 1U + sizeof(blank_suffix) - 1U
	    || memcmp(output, intro, sizeof(intro) - 1U) != 0
	    || memcmp(output + sizeof(intro) - 1U, blank_suffix,
	    sizeof(blank_suffix) - 1U) != 0) {
		free(before);
		free(after);
		free(output);
		return false;
	}
	free(after);
	after = NULL;
	free(output);
	output = NULL;
	if (!write_file("portname.in", abort_input, sizeof(abort_input) - 1U)
	    || !run_redirected(YT_PORTNAME_EXE, "portname.in", "portname.out")
	    || !read_file("YTDATA.DAT", &after, &after_length)
	    || !read_file("portname.out", &output, &output_length)
	    || after_length != before_length
	    || memcmp(after, before, before_length) != 0
	    || output_length != sizeof(intro) - 1U + sizeof(abort_suffix) - 1U
	    || memcmp(output, intro, sizeof(intro) - 1U) != 0
	    || memcmp(output + sizeof(intro) - 1U, abort_suffix,
	    sizeof(abort_suffix) - 1U) != 0) {
		free(before);
		free(after);
		free(output);
		return false;
	}
	free(after);
	after = NULL;
	free(output);
	output = NULL;
	if (!write_file("portname.in", input, sizeof(input) - 1U)
	    || !run_redirected(YT_PORTNAME_EXE, "portname.in",
	    "portname.out")
	    || !read_file("YTDATA.DAT", &after, &after_length)
	    || !read_file("portname.out", &output, &output_length)) {
		free(before);
		free(after);
		free(output);
		return false;
	}
	if (after_length != before_length
	    || output_length < sizeof(intro) - 1U + sizeof(success_prefix) - 1U
	    + sizeof(success_suffix) - 1U
	    || memcmp(output, intro, sizeof(intro) - 1U) != 0
	    || memcmp(output + sizeof(intro) - 1U, success_prefix,
	    sizeof(success_prefix) - 1U) != 0
	    || memcmp(output + output_length - (sizeof(success_suffix) - 1U),
	    success_suffix, sizeof(success_suffix) - 1U) != 0
	    || memchr(output, '\n', output_length) != NULL) {
		free(before);
		free(after);
		free(output);
		return false;
	}
	memset(&game, 0, sizeof(game));
	if (!yt_database_open(&game.database, "YTDATA.DAT", YT_OPEN_READ,
	    error) || !yt_config_load(&game.database, &game.config, error)) {
		free(before);
		free(after);
		free(output);
		return false;
	}
	for (logical = 1; logical <= 1000; ++logical) {
		struct yt_port port;
		size_t record = (size_t)yt_port_basic_record(&game.config,
		    logical);
		size_t base = (record - 1U) * YT_RECORD_SIZE;
		size_t byte;

		if (!yt_game_read_port(&game, logical, &port, error)
		    || port.name_length < 1.0f || port.name_length > 41.0f
		    || (logical == 1 && strcmp(port.name, "Earth") != 0)) {
			yt_game_close(&game);
			free(before);
			free(after);
			free(output);
			return false;
		}
		for (byte = 0; byte < YT_RECORD_SIZE; ++byte) {
			bool mutable = byte < 41U
			    || (byte >= YT_F85 && byte < YT_F85 + 4U);

			if (!mutable && before[base + byte] != after[base + byte]) {
				yt_game_close(&game);
				free(before);
				free(after);
				free(output);
				return false;
			}
		}
	}
	yt_game_close(&game);
	free(output);
	output = NULL;
	(void)remove("YTDATA.DAT");
	(void)remove("ytdata.dat");
	if (!write_file("portname.in", blank_input, sizeof(blank_input) - 1U)
	    || !run_redirected(YT_PORTNAME_EXE, "portname.in", "portname.out")
	    || !read_file("portname.out", &output, &output_length)) {
		free(before);
		free(after);
		free(output);
		return false;
	}
	probe = fopen("YTDATA.DAT", "rb");
	if (probe != NULL) {
		(void)fclose(probe);
		free(before);
		free(after);
		free(output);
		return false;
	}
	probe = fopen("ytdata.dat", "rb");
	if (probe != NULL) {
		(void)fclose(probe);
		free(before);
		free(after);
		free(output);
		return false;
	}
	{
		struct yt_portname_output expected_missing;

		if (!yt_portname_compose_output(YT_PORTNAME_OUTPUT_MISSING_DATA,
		    0.0f, NULL, 0U, &expected_missing)
		    || output_length != expected_missing.length
		    || memcmp(output, expected_missing.bytes,
		    expected_missing.length) != 0
		    || !write_file("YTDATA.DAT", after, after_length)) {
			free(before);
			free(after);
			free(output);
			return false;
		}
	}
	free(before);
	free(after);
	free(output);
	return true;
}

static bool
test_rmt_standalone_decline(struct yt_error *error)
{
	static const char input[] = "N\n";
	static const uint8_t expected[] =
	    "\r\r"
	    "Running stand alone... re-initializing using old sysop defined defaults.\r"
	    "\r"
	    "Do you wish to re-init Y.T. using your old default values? N  \r";
	struct yt_text_file output;
	bool result;

	(void)remove("RMTINIT.TMP");
	(void)remove("rmtinit.tmp");
	if (!write_file("rmt.in", input, sizeof(input) - 1U)
	    || !run_redirected(YT_RMT_INIT_EXE, "rmt.in", "rmt.out")
	    || !yt_text_read("rmt.out", &output, error))
		return false;
	result = output.length == sizeof(expected) - 1U
	    && memcmp(output.data, expected, sizeof(expected) - 1U) == 0
	    && file_size_is("RMTINIT.TMP", 0L)
	    && !file_size_is("rmtinit.tmp", 0L)
	    && file_size_is("YTDATA.DAT", 432235L);
	yt_text_free(&output);
	return result;
}

static bool
test_rmt_init(struct yt_error *error)
{
	static const uint8_t expected_prophecy[] =
	    "** The Prophesy has been fulfilled by The Sysop!! **";
	struct yt_text_file output;
	struct yt_text_file messages;
	struct yt_game game;
	static const char input[] = "Y\n";
	static const uint8_t expected_prefix[] =
	    "\r\r"
	    "Running stand alone... re-initializing using old sysop defined defaults.\r"
	    "\r"
	    "Do you wish to re-init Y.T. using your old default values? Y  \r\r";
	static const uint8_t expected_tail[] =
	    "\r\aInitialization completed sucessfully!\a\r";
	static const char scoreboard[] = "stale scoreboard survives";
	bool result;

	(void)remove("RMTINIT.TMP");
	(void)remove("rmtinit.tmp");
	if (!write_file("YTSCORE.ASC", scoreboard, sizeof(scoreboard) - 1U)
	    || !write_file("rmt.in", input, sizeof(input) - 1U)
	    || !run_redirected(YT_RMT_INIT_EXE, "rmt.in", "rmt.out")
	    || !file_size_is("YTDATA.DAT", 432235L)
	    || !file_size_is("YTNAME.DAT", 26L)
	    || !file_size_is("YTYNEWS.DAT", 151L)
	    || !file_size_is("YTRMSG.DAT", 430L)
	    || !file_size_is("YTSCORE.ASC",
	    (long)(sizeof(scoreboard) - 1U))
	    || !yt_text_read("rmt.out", &output, error))
		return false;
	if (!yt_text_read("YTRMSG.DAT", &messages, error)) {
		yt_text_free(&output);
		return false;
	}
	result = output.length >= sizeof(expected_prefix) - 1U
	    && memcmp(output.data, expected_prefix,
	    sizeof(expected_prefix) - 1U) == 0
	    && output.length >= sizeof(expected_tail) - 1U
	    && memcmp(output.data + output.length - (sizeof(expected_tail) - 1U),
	    expected_tail, sizeof(expected_tail) - 1U) == 0
	    && strstr((const char *)output.data, "Congratulations") == NULL
	    && strstr((const char *)output.data,
	    "Running stand alone... re-initializing using old sysop defined defaults.")
	    != NULL
	    && messages.length == 5U * sizeof(struct yt_radio_record);
	for (size_t index = 0U; result && index < 5U; ++index) {
		struct yt_radio_record record;

		memcpy(record.bytes, messages.data
		    + index * sizeof(record.bytes), sizeof(record.bytes));
		result = yt_radio_get_number(&record, 0U) == 50.0f
		    && yt_radio_get_number(&record, 4U) == -2.0f
		    && yt_radio_get_number(&record, 8U) == -2.0f
		    && memcmp(record.bytes + 12U, expected_prophecy,
		    sizeof(expected_prophecy) - 1U) == 0;
		for (size_t offset = 12U + sizeof(expected_prophecy) - 1U;
		    result && offset < sizeof(record.bytes); ++offset)
			result = record.bytes[offset] == (offset < 84U ? ' ' : 0U);
	}
	yt_text_free(&output);
	yt_text_free(&messages);
	if (!result)
		return false;
	memset(&game, 0, sizeof(game));
	if (!yt_database_open(&game.database, "YTDATA.DAT", YT_OPEN_READ,
	    error) || !yt_config_load(&game.database, &game.config, error))
		return false;
	result = game.config.local_screen == -1.0f
	    && game.config.maximum_holds == 1000.0f
	    && game.config.genesis_ports == 300.0f;
	yt_game_close(&game);
	return result;
}

static bool
test_rmt_missing_old(struct yt_error *error)
{
	static const char input[] = "Y\n";
	static const uint8_t expected_tail[] =
	    "\r\r\aERROR! OLD DATA FILES NOT FOUND!!!!!!!!!!!!!!!!!!!!!!!!\a\r";
	struct yt_text_file output;
	size_t tail_length = sizeof(expected_tail) - 1U;
	bool result;

	(void)remove("YTDATA.DAT");
	(void)remove("RMTINIT.TMP");
	(void)remove("rmtinit.tmp");
	if (!write_file("rmt.in", input, sizeof(input) - 1U)
	    || !run_redirected(YT_RMT_INIT_EXE, "rmt.in", "rmt.out")
	    || !yt_text_read("rmt.out", &output, error))
		return false;
	result = output.length >= tail_length
	    && memcmp(output.data + output.length - tail_length, expected_tail,
	    tail_length) == 0
	    && !file_size_is("YTDATA.DAT", 0L);
	yt_text_free(&output);
	return result;
}

struct genesis_rmt_join {
	struct yt_text_output output;
	bool producer_verified;
	bool consumer_invoked;
};

static bool
genesis_rmt_close_file5(void *context, struct yt_error *error)
{
	(void)context;
	(void)error;
	return true;
}

static bool
genesis_rmt_open_output(void *context, struct yt_error *error)
{
	struct genesis_rmt_join *join = context;

	return yt_text_output_open(&join->output, "RMTINIT.TMP", error);
}

static bool
genesis_rmt_print_command(void *context, struct yt_error *error)
{
	static const uint8_t command[] = "DORINFO1.DEF\r\n";
	struct genesis_rmt_join *join = context;

	return yt_text_output_write(&join->output, command,
	    sizeof(command) - 1U, error);
}

static bool
genesis_rmt_close_all(void *context, struct yt_error *error)
{
	struct genesis_rmt_join *join = context;

	return yt_text_output_close_all_method(&join->output, 0, error);
}

static bool
genesis_rmt_run_consumer(void *context, struct yt_error *error)
{
	static const uint8_t expected[] = "DORINFO1.DEF\r\n\x1a";
	struct genesis_rmt_join *join = context;
	uint8_t *file = NULL;
	size_t length = 0U;

	(void)error;
	join->consumer_invoked = true;
	join->producer_verified = read_file("RMTINIT.TMP", &file, &length)
	    && length == sizeof(expected) - 1U
	    && memcmp(file, expected, sizeof(expected) - 1U) == 0;
	free(file);
	return join->producer_verified
	    && run_redirected(YT_RMT_INIT_EXE, "rmt.in", "rmt.out");
}

static bool
test_genesis_rmt_producer_consumer(struct yt_error *error)
{
	static const struct yt_genesis_handoff_ops ops = {
		genesis_rmt_close_file5,
		genesis_rmt_open_output,
		genesis_rmt_print_command,
		genesis_rmt_close_all,
		genesis_rmt_run_consumer,
	};
	static const uint8_t dorinfo[] = {
	    'S','y','s','t','e','m','\r','\n',
	    'S','y','s','o','p','\r','\n',
	    'N','a','m','e','\r','\n',
	    'C','O','M','0',':','\r','\n',
	    '9','6','0','0',' ','B','A','U','D',',','N',',','8',',','1','\r','\n',
	    'u','n',0,'u','s','e','d','\r','\n',
	    ' ','j','A','N','E',' ','\r','\n',
	    ' ','D','O','E',' ','\x1a',
	};
	static const uint8_t names[] =
	    "Jane,Doe,First,Alias\r\n"
	    "Other,Player,Wrong,Person\r\n"
	    "Jane,Doe,Last,Winner\r\n\x1a";
	static const uint8_t expected_output[] =
	    "Local Console Mode\r"
	    "\r\r\aERROR! OLD DATA FILES NOT FOUND!!!!!!!!!!!!!!!!!!!!!!!!\a\r";
	struct genesis_rmt_join join;
	struct yt_genesis_handoff_state state;
	struct yt_text_file output;
	bool result;

	(void)remove("YTDATA.DAT");
	(void)remove("RMTINIT.TMP");
	(void)remove("rmtinit.tmp");
	if (!write_file("DORINFO1.DEF", dorinfo, sizeof(dorinfo))
	    || !write_file("YTNAME.DAT", names, sizeof(names) - 1U)
	    || !write_file("rmt.in", "", 0U))
		return false;
	memset(&join, 0, sizeof(join));
	yt_text_output_init(&join.output);
	result = yt_genesis_handoff_run(&state, &ops, &join, error);
	yt_text_output_destroy(&join.output);
	if (!result || !yt_text_read("rmt.out", &output, error))
		return false;
	result = state.complete && state.file5_closed && state.output_opened
	    && state.command_printed && state.close_all_completed
	    && state.run_invoked && state.failed_operation == YT_GENESIS_HANDOFF_NONE
	    && join.producer_verified && join.consumer_invoked
	    && output.length == sizeof(expected_output) - 1U
	    && memcmp(output.data, expected_output,
	    sizeof(expected_output) - 1U) == 0
	    && file_missing("RMTINIT.TMP")
	    && file_missing("rmtinit.tmp")
	    && file_missing("YTDATA.DAT");
	yt_text_free(&output);
	(void)remove("DORINFO1.DEF");
	return result;
}

static bool
test_rmt_remote_local_entry(struct yt_error *error)
{
	static const uint8_t handoff[] = "DORINFO1.DEF\rignored";
	static const uint8_t dorinfo[] = {
	    'S','y','s','t','e','m','\r','\n',
	    'S','y','s','o','p','\r','\n',
	    'N','a','m','e','\r','\n',
	    'C','O','M','0',':','\r','\n',
	    '9','6','0','0',' ','B','A','U','D',',','N',',','8',',','1','\r','\n',
	    'u','n',0,'u','s','e','d','\r','\n',
	    ' ','j','A','N','E',' ','\r','\n',
	    ' ','D','O','E',' ','\x1a',
	    'i','g','n','o','r','e','d'
	};
	static const uint8_t names[] =
	    "Jane,Doe,Alias,Person\r\n\x1a";
	static const uint8_t expected[] =
	    "Local Console Mode\r"
	    "\r\r\aERROR! OLD DATA FILES NOT FOUND!!!!!!!!!!!!!!!!!!!!!!!!\a\r";
	struct yt_text_file output;
	bool result;

	(void)remove("YTDATA.DAT");
	(void)remove("RMTINIT.TMP");
	(void)remove("rmtinit.tmp");
	if (!write_file("RMTINIT.TMP", handoff, sizeof(handoff) - 1U)
	    || !write_file("DORINFO1.DEF", dorinfo, sizeof(dorinfo))
	    || !write_file("YTNAME.DAT", names, sizeof(names) - 1U)
	    || !write_file("rmt.in", "", 0U)
	    || !run_redirected(YT_RMT_INIT_EXE, "rmt.in", "rmt.out")
	    || !yt_text_read("rmt.out", &output, error))
		return false;
	result = output.length == sizeof(expected) - 1U
	    && memcmp(output.data, expected, sizeof(expected) - 1U) == 0
	    && !file_size_is("YTDATA.DAT", 0L)
	    && !file_size_is("RMTINIT.TMP", (long)(sizeof(handoff) - 1U))
	    && !file_size_is("rmtinit.tmp", (long)(sizeof(handoff) - 1U));
	yt_text_free(&output);
	(void)remove("DORINFO1.DEF");
	return result;
}

#ifndef _WIN32
static bool
run_rmt_pty(uint8_t **output, size_t *output_length,
    struct termios *terminal_after)
{
	uint8_t *bytes = NULL;
	size_t capacity = 0U;
	size_t used = 0U;
	char *slave_name;
	struct termios terminal;
	pid_t child;
	int master = -1;
	int slave = -1;
	int flags;
	int status;
	bool valid = false;

	*output = NULL;
	*output_length = 0U;
	master = posix_openpt(O_RDWR | O_NOCTTY);
	if (master < 0 || grantpt(master) != 0 || unlockpt(master) != 0)
		goto done;
	slave_name = ptsname(master);
	if (slave_name == NULL)
		goto done;
	slave = open(slave_name, O_RDWR | O_NOCTTY);
	if (slave < 0 || tcgetattr(slave, &terminal) != 0)
		goto done;
	terminal.c_cflag &= (tcflag_t)~(CSIZE | PARENB | PARODD | CSTOPB);
	terminal.c_cflag |= CS8;
	if (cfsetispeed(&terminal, B38400) != 0
	    || cfsetospeed(&terminal, B38400) != 0
	    || tcsetattr(slave, TCSANOW, &terminal) != 0)
		goto done;
	child = fork();
	if (child < 0)
		goto done;
	if (child == 0) {
		(void)close(master);
		if (dup2(slave, STDIN_FILENO) != STDIN_FILENO
		    || dup2(slave, STDOUT_FILENO) != STDOUT_FILENO
		    || dup2(slave, STDERR_FILENO) != STDERR_FILENO)
			_exit(126);
		if (slave > STDERR_FILENO)
			(void)close(slave);
		execl(YT_RMT_INIT_EXE, YT_RMT_INIT_EXE, (char *)NULL);
		_exit(127);
	}
	if (waitpid(child, &status, 0) != child || !WIFEXITED(status)
	    || WEXITSTATUS(status) != 0)
		goto done;
	if (tcgetattr(slave, terminal_after) != 0)
		goto done;
	flags = fcntl(master, F_GETFL, 0);
	if (flags < 0 || fcntl(master, F_SETFL, flags | O_NONBLOCK) != 0)
		goto done;
	for (;;) {
		ssize_t count;

		if (used == capacity) {
			size_t next = capacity == 0U ? 256U : capacity * 2U;
			uint8_t *resized = realloc(bytes, next);

			if (resized == NULL)
				goto done;
			bytes = resized;
			capacity = next;
		}
		count = read(master, bytes + used, capacity - used);
		if (count > 0) {
			used += (size_t)count;
			continue;
		}
		if (count == 0 || errno == EAGAIN || errno == EWOULDBLOCK
		    || errno == EIO)
			break;
		if (errno != EINTR)
			goto done;
	}
	*output = bytes;
	*output_length = used;
	bytes = NULL;
	valid = true;

done:
	free(bytes);
	if (slave >= 0)
		(void)close(slave);
	if (master >= 0)
		(void)close(master);
	return valid;
}

static bool
test_rmt_remote_opendoors_entry(struct yt_error *error)
{
	static const uint8_t handoff[] = "DORINFO1.DEF\rignored";
	static const uint8_t dorinfo[] =
	    "System\r\nSysop\r\nName\r\nCOM1:\r\n"
	    "38400 BAUD,E,7,1\r\nunused\r\nJane\r\nDoe\x1a";
	static const uint8_t names[] = "Jane,Doe,Alias,Person\r\n\x1a";
	static const uint8_t expected[] =
	    "\n\aERROR! OLD DATA FILES NOT FOUND!!!!!!!!!!!!!!!!!!!!!!!!\a\r";
	struct termios terminal;
	uint8_t *output = NULL;
	size_t output_length = 0U;
	bool valid;

	(void)error;
	(void)remove("YTDATA.DAT");
	if (!write_file("RMTINIT.TMP", handoff, sizeof(handoff) - 1U)
	    || !write_file("DORINFO1.DEF", dorinfo, sizeof(dorinfo) - 1U)
	    || !write_file("YTNAME.DAT", names, sizeof(names) - 1U)
	    || !run_rmt_pty(&output, &output_length, &terminal)) {
		free(output);
		return false;
	}
	valid = output_length == sizeof(expected) - 1U
	    && memcmp(output, expected, sizeof(expected) - 1U) == 0
	    && cfgetispeed(&terminal) == B38400
	    && cfgetospeed(&terminal) == B38400
	    && (terminal.c_oflag & OPOST) == 0
	    && (terminal.c_cflag & CSIZE) == CS7
	    && (terminal.c_cflag & PARENB) != 0
	    && !file_size_is("YTDATA.DAT", 0L)
	    && !file_size_is("RMTINIT.TMP", (long)(sizeof(handoff) - 1U));
	if (!valid) {
		fprintf(stderr, "RMT PTY missing: got=%zu expected=%zu ispeed=%lu ospeed=%lu cflag=%lx data=",
		    output_length, sizeof(expected) - 1U,
		    (unsigned long)cfgetispeed(&terminal),
		    (unsigned long)cfgetospeed(&terminal),
		    (unsigned long)terminal.c_cflag);
		for (size_t index = 0U; index < output_length; ++index)
			fprintf(stderr, "%02x", output[index]);
		fputc('\n', stderr);
	}
	free(output);
	(void)remove("DORINFO1.DEF");
	return valid;
}

static bool
test_rmt_remote_opendoors_completion(struct yt_error *error)
{
	static const uint8_t handoff[] = "DORINFO1.DEF\rignored";
	static const uint8_t dorinfo[] =
	    "System\r\nSysop\r\nName\r\nCOM1:\r\n"
	    "38400 BAUD,E,7,1\r\nunused\r\nJane\r\nDoe\x1a";
	static const uint8_t names[] =
	    "Jane,Doe,First,Alias\r\n"
	    "Other,Player,Wrong,Person\r\n"
	    "Jane,Doe,Last,Winner\r\n\x1a";
	static const uint8_t expected_prefix[] =
	    "\n\r"
	    "\n          Yankee Trader Remote Initialization Program v2.2\r"
	    "\n                         By Alan Davenport\r";
	static const uint8_t expected_tail[] =
	    "\n\r"
	    "\n\aInitialization completed sucessfully!\a\r"
	    "\n\r"
	    "\nCongratulations Last Winner! You have fulfilled the prophesy!!\r"
	    "\nCongratulations Last Winner! You have fulfilled the prophesy!!\r"
	    "\nCongratulations Last Winner! You have fulfilled the prophesy!!\r"
	    "\n\r"
	    "\nReturning you to the BBS...\r";
	static const uint8_t expected_prophecy[] =
	    "** The Prophesy has been fulfilled by Last Winner!! **";
	struct termios terminal;
	struct yt_text_file news = {0};
	uint8_t *output = NULL;
	size_t output_length = 0U;
	bool valid;

	if (!initialize_direct(error)
	    || !write_file("RMTINIT.TMP", handoff, sizeof(handoff) - 1U)
	    || !write_file("DORINFO1.DEF", dorinfo, sizeof(dorinfo) - 1U)
	    || !write_file("YTNAME.DAT", names, sizeof(names) - 1U)
	    || !run_rmt_pty(&output, &output_length, &terminal)
	    || !yt_text_read("YTNEWS.DAT", &news, error)) {
		free(output);
		yt_text_free(&news);
		return false;
	}
	valid = output_length >= sizeof(expected_prefix) - 1U
	    + sizeof(expected_tail) - 1U
	    && memcmp(output, expected_prefix, sizeof(expected_prefix) - 1U) == 0
	    && memcmp(output + output_length - (sizeof(expected_tail) - 1U),
	    expected_tail, sizeof(expected_tail) - 1U) == 0
	    && cfgetispeed(&terminal) == B38400
	    && cfgetospeed(&terminal) == B38400
	    && (terminal.c_oflag & OPOST) == 0
	    && (terminal.c_cflag & CSIZE) == CS7
	    && (terminal.c_cflag & PARENB) != 0
	    && file_size_is("YTDATA.DAT", 432235L)
	    && !file_size_is("RMTINIT.TMP", (long)(sizeof(handoff) - 1U))
	    && bytes_contain(news.data, news.length, expected_prophecy,
	    sizeof(expected_prophecy) - 1U);
	if (!valid) {
		fprintf(stderr, "RMT PTY completion: got=%zu prefix=%zu tail=%zu ispeed=%lu ospeed=%lu cflag=%lx\n",
		    output_length, sizeof(expected_prefix) - 1U,
		    sizeof(expected_tail) - 1U,
		    (unsigned long)cfgetispeed(&terminal),
		    (unsigned long)cfgetospeed(&terminal),
		    (unsigned long)terminal.c_cflag);
	}
	free(output);
	yt_text_free(&news);
	(void)remove("DORINFO1.DEF");
	return valid;
}
#else
static bool
test_rmt_remote_opendoors_entry(struct yt_error *error)
{
	(void)error;
	return true;
}


static bool
test_rmt_remote_opendoors_completion(struct yt_error *error)
{
	(void)error;
	return true;
}
#endif

#ifndef _WIN32
static bool
copy_executable(const char *source, const char *dest)
{
	uint8_t *data;
	size_t length;
	bool result;

	if (!read_file(source, &data, &length))
		return false;
	result = write_file(dest, data, length) && chmod(dest, 0700) == 0;
	free(data);
	return result;
}

static bool
test_local(struct yt_error *error)
{
	static const char child[] =
	    "#!/bin/sh\n"
	    "printf '%s\\n' \"$1\" > child.out\n"
	    "exit 7\n";
	static const char command[] =
	    "./local '  jane   doe  ' > local.out 2>&1";
	static const char expected[] =
	    "Yankee Trader Local Logon Program\r\n"
	    "Alan\r\n"
	    "Davenport\r\n"
	    "COM0\r\n"
	    "0 BAUD,N,8,1\r\n"
	    " 0 \r\n"
	    "JANE\r\n"
	    "DOE\r\n"
	    "Anytown, USA\r\n"
	    " 1 \r\n"
	    " 100 \r\n"
	    " 180 \r\n"
	    " 0 \r\n"
	    "\x1a";
	struct yt_text_file dorinfo;
	struct yt_text_file child_output;
	bool result;

	if (!copy_executable(YT_LOCAL_EXE, "local")
	    || !write_file("yt", child, sizeof(child) - 1U)
	    || chmod("yt", 0700) != 0
	    || system(command) != 0
	    || !yt_text_read("DORINFO1.DEF", &dorinfo, error)
	    || !yt_text_read("child.out", &child_output, error))
		return false;
	result = dorinfo.length == sizeof(expected) - 1U
	    && memcmp(dorinfo.data, expected, sizeof(expected) - 1U) == 0
	    && strcmp((const char *)child_output.data, "DORINFO1.DEF\n") == 0;
	yt_text_free(&dorinfo);
	yt_text_free(&child_output);
	return result;
}
#endif

static void
cleanup_files(void)
{
	static const char *const paths[] = {
		"YTDATA.DAT", "YTNAME.DAT", "YTNEWS.DAT", "YTYNEWS.DAT",
		"YTRMSG.DAT", "YTSCORE.ASC", "RMTINIT.TMP", "rmtinit.tmp",
		"RMTOLD.DAT",
		"config.in", "config.out", "portname.in", "portname.out",
		"rmt.in", "rmt.out", "local", "local.out", "yt", "child.out",
		"DORINFO1.DEF", "names.in", "names.append"
	};
	size_t index;

	for (index = 0; index < sizeof(paths) / sizeof(paths[0]); ++index)
		(void)remove(paths[index]);
}

int
main(void)
{
	char original[1024];
	char directory[1024];
	struct yt_error error;
	const char *failure = NULL;

	if (yt_getcwd(original, sizeof(original)) == NULL)
		return fail("cannot determine original directory");
#ifdef _WIN32
	snprintf(directory, sizeof(directory), "%s\\yt-utilities-%lu",
	    original, (unsigned long)_getpid());
	if (yt_mkdir(directory) != 0)
		return fail("cannot create temporary directory");
#else
	snprintf(directory, sizeof(directory), "/tmp/yt-utilities.XXXXXX");
	if (mkdtemp(directory) == NULL)
		return fail("cannot create temporary directory");
#endif
	if (yt_chdir(directory) != 0)
		return fail("cannot enter temporary directory");
	yt_error_clear(&error);
	if (!test_port_name_generator())
		failure = "PORTNAME generator vectors differ";
	else if (!test_portname_controller())
		failure = "PORTNAME controller vectors differ";
	else if (!test_initializer_bounded())
		failure = "initializer bounded random vectors differ";
	else if (!test_initializer_confirmation())
		failure = "initializer confirmation predicate differs";
	else if (!test_yt_init_pre_input_presentation())
		failure = "YT-INIT pre-input presentation differs";
	else if (!test_rmt_output_helpers())
		failure = "RMT shared output helper vectors differ";
	else if (!test_rmt_output_state())
		failure = "RMT stateful output vectors differ";
	else if (!test_rmt_output_adapter())
		failure = "RMT ordered output adapter vectors differ";
	else if (!test_rmt_completion_output())
		failure = "RMT completion output vectors differ";
	else if (!test_rmt_standalone_entry())
		failure = "RMT standalone entry vectors differ";
	else if (!test_rmt_handoff_parser())
		failure = "RMT handoff parser vectors differ";
	else if (!test_rmt_handoff_read_transaction())
		failure = "RMT handoff read transaction differs";
	else if (!test_rmt_handoff_cleanup_transaction())
		failure = "RMT handoff cleanup transaction differs";
	else if (!test_rmt_dorinfo_parser())
		failure = "RMT DORINFO parser vectors differ";
	else if (!test_rmt_dorinfo_read_transaction())
		failure = "RMT DORINFO read transaction differs";
	else if (!test_rmt_remote_status())
		failure = "RMT remote status vectors differ";
	else if (!test_rmt_remote_serial())
		failure = "RMT remote serial setup vectors differ";
	else if (!test_rmt_remote_identity())
		failure = "RMT remote identity vectors differ";
	else if (!test_rmt_old_preprocess())
		failure = "RMT old-database prepass differs";
	else if (!test_rmt_config_normalization())
		failure = "RMT old-configuration normalization differs";
	else if (!test_maintenance_alias_compaction_transaction())
		failure = "maintenance alias transaction differs";
	else if (!test_maintenance_alias_compaction())
		failure = "maintenance alias compaction differs";
	else if (!test_news_rotation_transaction())
		failure = "maintenance news rotation transaction differs";
	else if (!test_radio_compaction_transaction())
		failure = "maintenance radio compaction transaction differs";
	else if (!test_maintenance_message_compaction())
		failure = "maintenance message/news compaction differs";
	else if (!test_maintenance_random_helpers())
		failure = "maintenance random helper vectors differ";
	else if (!test_maintenance_xannor_defense())
		failure = "maintenance Xannor defense gate differs";
	else if (!test_maintenance_xannor_player())
		failure = "maintenance Xannor player combat differs";
	else if (!test_yt_init_presented_world())
		failure = "deterministic YT-INIT presentation differs";
	else if (!test_yt_clock_boundaries())
		failure = "YT-INIT date observation boundaries differ";
	else if (!test_yt_init_presentation_pre_put_failure())
		failure = "YT-INIT pre-PUT presentation failure differs";
	else if (!test_yt_init_presentation_failure_prefix())
		failure = "YT-INIT presentation failure prefix differs";
	else if (!test_yt_init_sector_prepass())
		failure = "YT-INIT sector prepass differs";
	else if (!test_yt_init_random_binding())
		failure = "YT-INIT random-file binding differs";
	else if (!test_initializer_world_image())
		failure = "deterministic initializer world image differs";
	else if (!test_initializer_graph_retries())
		failure = "initializer graph retry fixture differs";
	else if (!test_yt_init_random_close_failure())
		failure = "YT-INIT random CLOSE failure differs";
	else if (!test_rmt_initializer_world_image())
		failure = "deterministic RMT initializer world image differs";
	else if (!test_rmt_presentation_failure_prefixes())
		failure = "RMT presentation failure prefixes differ";
	else if (!test_rmt_dynamic_presentation())
		failure = "RMT dynamic presentation differs";
	else if (!test_rmt_clock_boundaries())
		failure = "RMT date observation boundaries differ";
	else if (!initialize_direct(&error))
		failure = "cannot create utility test database";
	else if (!test_expired_player_cleanup(&error))
		failure = "final expired-player cleanup differs";
	else if (!initialize_direct(&error))
		failure = "cannot restore utility test database";
	else if (!test_immediate_death_cleanup(&error))
		failure = "immediate-death cleanup differs";
	else if (!initialize_direct(&error))
		failure = "cannot restore Xannor-arrival utility test database";
	else if (!test_xannor_player_arrival(&error))
		failure = "Xannor player-arrival transaction differs";
	else if (!initialize_direct(&error))
		failure = "cannot restore Xannor-planet utility test database";
	else if (!test_xannor_planet_arrival(&error))
		failure = "Xannor planet-arrival transaction differs";
	else if (!initialize_direct(&error))
		failure = "cannot restore post-Xannor utility test database";
	else if (!test_maintenance_route_builder(&error))
		failure = "maintenance route builder differs";
	else if (!initialize_direct(&error))
		failure = "cannot restore post-route utility test database";
	else if (!test_ytconfig(&error))
		failure = "YTCONFIG executable behavior differs";
	else if (!test_ytconfig_missing_data())
		failure = "YTCONFIG missing-data terminal differs";
	else if (!test_ytconfig_genesis(&error))
		failure = "YTCONFIG Genesis editor differs";
	else if (!test_ytconfig_headquarters_transaction())
		failure = "YTCONFIG Headquarters transaction differs";
	else if (!test_ytconfig_overlay_transaction())
		failure = "YTCONFIG raw overlay transaction differs";
	else if (!test_ytconfig_local_screen_transaction())
		failure = "YTCONFIG local-screen transaction differs";
	else if (!test_ytconfig_redraw_repair_transaction())
		failure = "YTCONFIG redraw-repair transaction differs";
	else if (!test_ytconfig_headquarters(&error))
		failure = "YTCONFIG Headquarters relocation differs";
	else if (!test_ytconfig_scalar_options(&error))
		failure = "YTCONFIG scalar options differ";
	else if (!test_ytconfig_planet_editor(&error))
		failure = "YTCONFIG planet editor differs";
	else if (!test_ytconfig_port_editor(&error))
		failure = "YTCONFIG port editor differs";
	else if (!test_ytconfig_alias_editor(&error))
		failure = "YTCONFIG alias editor differs";
	else if (!test_portname(&error))
		failure = "PORTNAME changed data outside its two owned fields";
	else if (!test_rmt_standalone_decline(&error))
		failure = "RMT-INIT standalone decline differs";
	else if (!test_rmt_init(&error))
		failure = "RMT-INIT standalone behavior differs";
	else if (!test_genesis_rmt_producer_consumer(&error))
		failure = "Genesis-to-RMT handoff transaction differs";
	else if (!test_rmt_remote_local_entry(&error))
		failure = "RMT-INIT remote local-console entry differs";
	else if (!test_rmt_remote_opendoors_entry(&error))
		failure = "RMT-INIT OpenDoors remote entry differs";
	else if (!test_rmt_remote_opendoors_completion(&error))
		failure = "RMT-INIT OpenDoors remote completion differs";
	else if (!test_rmt_missing_old(&error))
		failure = "RMT-INIT missing-old-data branch differs";
	else if (!test_name_input_grammar(&error))
		failure = "YTNAME INPUT# grammar differs";
	else if (!test_name_sequential_transaction(&error))
		failure = "YTNAME sequential transaction differs";
	else if (!test_name_sequential_output_transaction(&error))
		failure = "YTNAME sequential output transaction differs";
	else if (!test_alias_propagation_transaction())
		failure = "YTCONFIG alias propagation transaction differs";
	else if (!test_name_append(&error))
		failure = "YTNAME append bytes differ";
	else if (!test_alias_key_preparation())
		failure = "YT alias-key preparation differs";
#ifndef _WIN32
	else if (!test_local(&error))
		failure = "LOCAL child handoff or DORINFO bytes differ";
#endif
	cleanup_files();
	if (yt_chdir(original) != 0)
		return fail("cannot restore original directory");
	(void)yt_rmdir(directory);
	if (failure != NULL)
		return fail(failure);
	puts("test_utilities: ok");
	return EXIT_SUCCESS;
}
