#include "yt_input_model.h"
#include "yt_platform.h"
#include "yt_startup_model.h"
#include "yt_text.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#endif

static unsigned failures;

#define CHECK(expr) do { \
	if (!(expr)) { \
		fprintf(stderr, "%s:%d: check failed: %s\n", \
		    __FILE__, __LINE__, #expr); \
		++failures; \
	} \
} while (0)

enum registration_event {
	REG_CLOSE,
	REG_RANDOM_OPEN,
	REG_SIZE,
	REG_DELETE,
	REG_SEQUENTIAL_OPEN,
	REG_READ,
	REG_CENTERED,
	REG_BEEP,
	REG_FORCED_LOCAL,
	REG_CLOSE_ALL,
	REG_END,
};

struct registration_tape {
	const uint8_t *file;
	size_t file_length;
	size_t cursor;
	enum registration_event event[32];
	size_t event_count;
	size_t fail_at;
	uint8_t presented[128];
	size_t presented_length;
};

static bool
registration_record(struct registration_tape *tape,
    enum registration_event event, struct yt_error *error)
{
	CHECK(tape->event_count < YT_ARRAY_LEN(tape->event));
	if (tape->event_count < YT_ARRAY_LEN(tape->event))
		tape->event[tape->event_count] = event;
	++tape->event_count;
	if (tape->event_count != tape->fail_at)
		return true;
	if (error != NULL) {
		error->status = YT_IO_ERROR;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    "injected registration failure");
	}
	return false;
}

static bool
registration_close(void *context, struct yt_error *error)
{
	return registration_record(context, REG_CLOSE, error);
}

static bool
registration_random_open(void *context, struct yt_error *error)
{
	return registration_record(context, REG_RANDOM_OPEN, error);
}

static bool
registration_size(void *context, uint64_t *size, struct yt_error *error)
{
	struct registration_tape *tape = context;

	if (!registration_record(tape, REG_SIZE, error))
		return false;
	*size = tape->file_length;
	return true;
}

static bool
registration_delete(void *context, struct yt_error *error)
{
	return registration_record(context, REG_DELETE, error);
}

static bool
registration_sequential_open(void *context, struct yt_error *error)
{
	struct registration_tape *tape = context;

	if (!registration_record(tape, REG_SEQUENTIAL_OPEN, error))
		return false;
	tape->cursor = 0U;
	return true;
}

static bool
registration_read(void *context, uint8_t *data, size_t capacity,
    size_t *length, struct yt_error *error)
{
	struct registration_tape *tape = context;
	bool available;

	if (!registration_record(tape, REG_READ, error))
		return false;
	if (!yt_text_line_input_next(tape->file, tape->file_length,
	    &tape->cursor, data, capacity, length, &available) || !available) {
		if (error != NULL) {
			error->status = YT_EOF;
			(void)snprintf(error->operation,
			    sizeof(error->operation), "%s",
			    "registration LINE INPUT");
		}
		return false;
	}
	return true;
}

static bool
registration_present(void *context, const uint8_t *text, size_t length,
    struct yt_error *error, enum registration_event event)
{
	struct registration_tape *tape = context;

	if (!registration_record(tape, event, error))
		return false;
	CHECK(length <= sizeof(tape->presented));
	if (length <= sizeof(tape->presented)) {
		memcpy(tape->presented, text, length);
		tape->presented_length = length;
	}
	return true;
}

static bool
registration_centered(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	return registration_present(context, text, length, error, REG_CENTERED);
}

static bool
registration_beep(void *context, struct yt_error *error)
{
	return registration_record(context, REG_BEEP, error);
}

static bool
registration_forced(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	return registration_present(context, text, length, error,
	    REG_FORCED_LOCAL);
}

static void
registration_close_all(void *context)
{
	struct registration_tape *tape = context;

	(void)registration_record(tape, REG_CLOSE_ALL, NULL);
}

static void
registration_end(void *context)
{
	struct registration_tape *tape = context;

	(void)registration_record(tape, REG_END, NULL);
}

static const struct yt_registration_ops registration_ops = {
	registration_close,
	registration_random_open,
	registration_size,
	registration_delete,
	registration_sequential_open,
	registration_read,
	registration_centered,
	registration_beep,
	registration_forced,
	registration_close_all,
	registration_end,
};

static void
registration_state_init(struct yt_registration_state *state,
	uint8_t storage[5][256])
{
	size_t index;

	memset(state, 0, sizeof(*state));
	for (index = 0U; index < 3U; ++index) {
		state->line[index].data = storage[index];
		state->line[index].capacity = sizeof(storage[index]);
	}
	for (index = 0U; index < 2U; ++index) {
		state->display[index].data = storage[index + 3U];
		state->display[index].capacity = sizeof(storage[index + 3U]);
	}
	state->expected_evaluation_sum[0] = 2085U;
	state->expected_evaluation_sum[1] = 3496U;
}

static struct yt_input_value
one(uint8_t byte)
{
	struct yt_input_value value = {{byte, 0}, 1, 0, false};

	return value;
}

static void
test_merged_fifo(void)
{
	struct yt_input_splitter splitter;
	struct yt_input_value remote_a = one('A');
	struct yt_input_value local_b = one('B');
	struct yt_input_value remote_c = one('C');
	struct yt_input_value selected;

	yt_input_splitter_init(&splitter);
	CHECK(yt_input_splitter_push(&splitter, true, &remote_a));
	CHECK(yt_input_splitter_push(&splitter, false, &local_b));
	CHECK(yt_input_splitter_push(&splitter, true, &remote_c));
	selected = yt_input_splitter_select_merged(&splitter);
	CHECK(selected.length == 1 && selected.bytes[0] == 'A'
	    && selected.remote);
	selected = yt_input_splitter_select_merged(&splitter);
	CHECK(selected.length == 1 && selected.bytes[0] == 'B'
	    && !selected.remote);
	selected = yt_input_splitter_select_merged(&splitter);
	CHECK(selected.length == 1 && selected.bytes[0] == 'C'
	    && selected.remote);
	selected = yt_input_splitter_select_merged(&splitter);
	CHECK(selected.length == 0);
}

static void
test_arbitration(void)
{
	struct yt_input_splitter splitter;
	struct yt_input_value local = one('L');
	struct yt_input_value remote = one('R');
	struct yt_input_value selected;

	yt_input_splitter_init(&splitter);
	CHECK(yt_input_splitter_push(&splitter, false, &local));
	CHECK(yt_input_splitter_push(&splitter, true, &remote));
	selected = yt_input_splitter_select(&splitter, 0.0f,
	    YT_INPUT_PHASE_B05D);
	CHECK(selected.length == 1 && selected.bytes[0] == 'R'
	    && selected.remote);
	CHECK(splitter.local.length == 0 && splitter.remote.length == 0);

	CHECK(yt_input_splitter_push(&splitter, false, &local));
	CHECK(yt_input_splitter_push(&splitter, true, &remote));
	selected = yt_input_splitter_select(&splitter, 2.0f,
	    YT_INPUT_PHASE_B05D);
	CHECK(selected.length == 1 && selected.bytes[0] == 'L'
	    && !selected.remote);
	CHECK(splitter.remote.length == 1);
	selected = yt_input_splitter_select(&splitter, 2.0f,
	    YT_INPUT_PHASE_AB36);
	CHECK(selected.length == 1 && selected.bytes[0] == 'R'
	    && selected.remote);

	CHECK(yt_input_splitter_push(&splitter, false, &local));
	CHECK(yt_input_splitter_push(&splitter, true, &remote));
	selected = yt_input_splitter_select(&splitter, 1.0f,
	    YT_INPUT_PHASE_AB36);
	CHECK(selected.length == 1 && selected.bytes[0] == 'L'
	    && !selected.remote);
	CHECK(splitter.remote.length == 1);
	selected = yt_input_splitter_select(&splitter, 2.0f,
	    YT_INPUT_PHASE_WAIT);
	CHECK(selected.length == 0 && splitter.remote.length == 1);
	selected = yt_input_splitter_select(&splitter, 0.0f,
	    YT_INPUT_PHASE_WAIT);
	CHECK(selected.length == 1 && selected.bytes[0] == 'R'
	    && selected.remote);

	selected = one('Q');
	remote = one('S');
	remote.remote = true;
	CHECK(yt_input_ab36_remote_replace(0.0f, &remote, &selected));
	CHECK(selected.length == 1 && selected.bytes[0] == 'S'
	    && selected.remote);
	selected = one('Q');
	CHECK(yt_input_ab36_remote_replace(2.0f, &remote, &selected));
	CHECK(selected.length == 1 && selected.bytes[0] == 'S'
	    && selected.remote);
	selected = one('Q');
	CHECK(yt_input_ab36_remote_replace(1.0f, &remote, &selected));
	CHECK(selected.length == 1 && selected.bytes[0] == 'Q'
	    && !selected.remote);
	remote.length = 0;
	CHECK(yt_input_ab36_remote_replace(0.0f, &remote, &selected));
	CHECK(selected.length == 1 && selected.bytes[0] == 'Q');
	remote.length = 3;
	CHECK(!yt_input_ab36_remote_replace(0.0f, &remote, &selected));
}

static void
test_ab36_inactivity_gate(void)
{
	CHECK(!yt_input_ab36_inactivity_expired(280.0f, 280.0f, 0.0f));
	CHECK(!yt_input_ab36_inactivity_expired(279.0f, 280.0f, 0.0f));
	CHECK(yt_input_ab36_inactivity_expired(281.0f, 280.0f, 0.0f));
	CHECK(!yt_input_ab36_inactivity_expired(281.0f, 280.0f, 1.0f));
	CHECK(yt_input_ab36_inactivity_expired(281.0f, 280.0f, 2.0f));
	CHECK(yt_input_ab36_inactivity_expired(281.0f, 280.0f, -1.0f));
	CHECK(!yt_input_ab36_session_expired(279.0f, 280.0f));
	CHECK(!yt_input_ab36_session_expired(280.0f, 280.0f));
	CHECK(yt_input_ab36_session_expired(281.0f, 280.0f));
	CHECK(yt_input_carrier_returns(0.0f, true));
	CHECK(!yt_input_carrier_returns(0.0f, false));
	CHECK(yt_input_carrier_returns(1.0f, false));
	CHECK(yt_input_carrier_returns(2.0f, false));
	CHECK(yt_input_carrier_returns(-1.0f, false));
	CHECK(yt_input_opening_row_route(false, false)
	    == YT_OPENING_ROW_CONTINUE);
	CHECK(yt_input_opening_row_route(true, false)
	    == YT_OPENING_ROW_STOP_LOCAL);
	CHECK(yt_input_opening_row_route(false, true)
	    == YT_OPENING_ROW_STOP_REMOTE);
	CHECK(yt_input_opening_row_route(true, true)
	    == YT_OPENING_ROW_STOP_LOCAL);
}

static void
test_ab36_queued_input(void)
{
	char queue[8] = "AB";
	size_t position = 0U;
	size_t length = 2U;
	struct yt_input_value selected;

	CHECK(yt_input_ab36_queue_pop(queue, sizeof(queue), &position,
	    &length, &selected));
	CHECK(selected.length == 1U && selected.bytes[0] == 'A'
	    && !selected.remote && position == 1U && length == 2U
	    && memcmp(queue, "AB", 2U) == 0);
	CHECK(yt_input_ab36_queue_pop(queue, sizeof(queue), &position,
	    &length, &selected));
	CHECK(selected.length == 1U && selected.bytes[0] == 'B'
	    && position == 0U && length == 0U && queue[0] == '\0');
	CHECK(yt_input_ab36_queue_pop(queue, sizeof(queue), &position,
	    &length, &selected) && selected.length == 0U);
	position = 2U;
	length = 1U;
	CHECK(!yt_input_ab36_queue_pop(queue, sizeof(queue), &position,
	    &length, &selected));
	position = 0U;
	length = sizeof(queue);
	CHECK(!yt_input_ab36_queue_pop(queue, sizeof(queue), &position,
	    &length, &selected));

	memcpy(queue, "AB", 3U);
	position = 1U;
	length = 2U;
	CHECK(yt_input_queue_clear(queue, sizeof(queue), &position, &length)
	    && queue[0] == '\0' && position == 0U && length == 0U);
	position = 2U;
	length = 1U;
	CHECK(!yt_input_queue_clear(queue, sizeof(queue), &position, &length));
}

static void
test_ab36_live_input(void)
{
	static const float replacing_modes[] = {0.0f, 2.0f, -1.0f};
	struct yt_input_splitter splitter;
	struct yt_input_value local = one('L');
	struct yt_input_value remote = one('R');
	struct yt_input_value selected;
	size_t index;

	for (index = 0U; index < sizeof(replacing_modes)
	    / sizeof(replacing_modes[0]); ++index) {
		yt_input_splitter_init(&splitter);
		CHECK(yt_input_splitter_push(&splitter, false, &local));
		CHECK(yt_input_splitter_push(&splitter, true, &remote));
		selected = yt_input_splitter_select(&splitter,
		    replacing_modes[index], YT_INPUT_PHASE_AB36);
		CHECK(selected.length == 1U && selected.bytes[0] == 'R'
		    && selected.remote && splitter.local.length == 0U
		    && splitter.remote.length == 0U);
	}

	yt_input_splitter_init(&splitter);
	CHECK(yt_input_splitter_push(&splitter, false, &local));
	CHECK(yt_input_splitter_push(&splitter, true, &remote));
	selected = yt_input_splitter_select(&splitter, 1.0f,
	    YT_INPUT_PHASE_AB36);
	CHECK(selected.length == 1U && selected.bytes[0] == 'L'
	    && !selected.remote && splitter.local.length == 0U
	    && splitter.remote.length == 1U);
	selected = yt_input_splitter_select(&splitter, 1.0f,
	    YT_INPUT_PHASE_AB36);
	CHECK(selected.length == 0U && splitter.remote.length == 1U);

	yt_input_splitter_init(&splitter);
	CHECK(yt_input_splitter_push(&splitter, false, &local));
	selected = yt_input_splitter_select(&splitter, 0.0f,
	    YT_INPUT_PHASE_AB36);
	CHECK(selected.length == 1U && selected.bytes[0] == 'L'
	    && !selected.remote);

	yt_input_splitter_init(&splitter);
	CHECK(yt_input_splitter_push(&splitter, true, &remote));
	selected = yt_input_splitter_select(&splitter, 0.0f,
	    YT_INPUT_PHASE_AB36);
	CHECK(selected.length == 1U && selected.bytes[0] == 'R'
	    && selected.remote);
}

static void
test_radio_body_live_input(void)
{
	static const float replacing_modes[] = {0.0f, 2.0f, -1.0f};
	struct yt_input_splitter splitter;
	struct yt_input_value local = one('L');
	struct yt_input_value remote = one('R');
	struct yt_input_value selected;
	size_t index;

	for (index = 0U; index < sizeof(replacing_modes)
	    / sizeof(replacing_modes[0]); ++index) {
		yt_input_splitter_init(&splitter);
		CHECK(yt_input_splitter_push(&splitter, false, &local));
		CHECK(yt_input_splitter_push(&splitter, true, &remote));
		selected = yt_input_splitter_select(&splitter,
		    replacing_modes[index], YT_INPUT_PHASE_RADIO_BODY);
		CHECK(selected.length == 1U && selected.bytes[0] == 'R'
		    && selected.remote && splitter.local.length == 0U
		    && splitter.remote.length == 0U);
	}

	yt_input_splitter_init(&splitter);
	CHECK(yt_input_splitter_push(&splitter, false, &local));
	CHECK(yt_input_splitter_push(&splitter, true, &remote));
	selected = yt_input_splitter_select(&splitter, 1.0f,
	    YT_INPUT_PHASE_RADIO_BODY);
	CHECK(selected.length == 1U && selected.bytes[0] == 'L'
	    && !selected.remote && splitter.local.length == 0U
	    && splitter.remote.length == 1U);
}

static void
test_radio_body_key_classification(void)
{
	unsigned key;

	for (key = 0U; key <= 0xffU; ++key) {
		enum yt_radio_body_key_action empty_expected;
		enum yt_radio_body_key_action nonempty_expected;

		if (key == '\r') {
			empty_expected = YT_RADIO_BODY_KEY_COMMIT;
			nonempty_expected = YT_RADIO_BODY_KEY_COMMIT;
		}
		else if (key == '\b' || key == 0x7fU) {
			empty_expected = YT_RADIO_BODY_KEY_IGNORE;
			nonempty_expected = YT_RADIO_BODY_KEY_BACKSPACE;
		}
		else if (key >= 0x20U && key < 0x7fU) {
			empty_expected = YT_RADIO_BODY_KEY_PRINTABLE;
			nonempty_expected = YT_RADIO_BODY_KEY_PRINTABLE;
		}
		else {
			empty_expected = YT_RADIO_BODY_KEY_IGNORE;
			nonempty_expected = YT_RADIO_BODY_KEY_IGNORE;
		}
		CHECK(yt_input_radio_body_key((uint8_t)key, 0U)
		    == empty_expected);
		CHECK(yt_input_radio_body_key((uint8_t)key, 1U)
		    == nonempty_expected);
	}
	CHECK(yt_input_radio_body_key('\n', 4U) == YT_RADIO_BODY_KEY_IGNORE);
}

static void
test_b05d_live_input(void)
{
	static const float local_only_modes[] = {1.0f, 2.0f, -1.0f};
	struct yt_input_splitter splitter;
	struct yt_input_value local = one('L');
	struct yt_input_value remote = one('R');
	struct yt_input_value selected;
	size_t index;

	yt_input_splitter_init(&splitter);
	CHECK(yt_input_splitter_push(&splitter, false, &local));
	CHECK(yt_input_splitter_push(&splitter, true, &remote));
	selected = yt_input_splitter_select(&splitter, 0.0f,
	    YT_INPUT_PHASE_B05D);
	CHECK(selected.length == 1U && selected.bytes[0] == 'R'
	    && selected.remote && splitter.local.length == 0U
	    && splitter.remote.length == 0U);

	for (index = 0U; index < sizeof(local_only_modes)
	    / sizeof(local_only_modes[0]); ++index) {
		yt_input_splitter_init(&splitter);
		CHECK(yt_input_splitter_push(&splitter, false, &local));
		CHECK(yt_input_splitter_push(&splitter, true, &remote));
		selected = yt_input_splitter_select(&splitter,
		    local_only_modes[index], YT_INPUT_PHASE_B05D);
		CHECK(selected.length == 1U && selected.bytes[0] == 'L'
		    && !selected.remote && splitter.local.length == 0U
		    && splitter.remote.length == 1U);
	}

	yt_input_splitter_init(&splitter);
	CHECK(yt_input_splitter_push(&splitter, true, &remote));
	selected = yt_input_splitter_select(&splitter, 2.0f,
	    YT_INPUT_PHASE_B05D);
	CHECK(selected.length == 0U && splitter.remote.length == 1U);

	yt_input_splitter_init(&splitter);
	CHECK(yt_input_splitter_push(&splitter, false, &local));
	selected = yt_input_splitter_select(&splitter, 0.0f,
	    YT_INPUT_PHASE_B05D);
	CHECK(selected.length == 1U && selected.bytes[0] == 'L'
	    && !selected.remote);
}

static void
test_ab36_repeat_recognition(void)
{
	struct yt_input_value selected = one(0x12U);

	CHECK(yt_input_ab36_repeat_requested(false, &selected));
	CHECK(!yt_input_ab36_repeat_requested(true, &selected));
	selected.bytes[0] = 0U;
	selected.bytes[1] = 0x12U;
	selected.length = 2U;
	CHECK(!yt_input_ab36_repeat_requested(false, &selected));
	selected = one('R');
	CHECK(!yt_input_ab36_repeat_requested(false, &selected));
	selected.length = 0U;
	CHECK(!yt_input_ab36_repeat_requested(false, &selected));
	CHECK(!yt_input_ab36_repeat_requested(false, NULL));
}

struct ab36_repeat_tape {
	uint8_t prefix[16];
	size_t length;
	size_t calls;
	char *accumulator;
	float *newline_flag;
	bool fail;
};

static bool
ab36_repeat_emit(void *context, const uint8_t *prefix, size_t length)
{
	struct ab36_repeat_tape *tape = context;

	++tape->calls;
	CHECK(*tape->newline_flag == 1.0f);
	CHECK(length <= sizeof(tape->prefix));
	if (length <= sizeof(tape->prefix))
		memcpy(tape->prefix, prefix, length);
	tape->length = length;
	if (tape->fail) {
		memcpy(tape->accumulator, "X", 2U);
		return false;
	}
	*tape->newline_flag = 0.0f;
	return true;
}

static void
test_ab36_repeat_transaction(void)
{
	char accumulator[16] = "AB";
	char saved[16] = "NS";
	char paged_text[16] = "old";
	float newline_flag = -1.0f;
	uint8_t selected_key = 0x12U;
	struct ab36_repeat_tape tape = {
		.accumulator = accumulator,
		.newline_flag = &newline_flag,
	};

	CHECK(yt_input_ab36_repeat_run(accumulator, sizeof(accumulator),
	    saved, sizeof(saved), paged_text, sizeof(paged_text),
	    &newline_flag, &selected_key,
	    ab36_repeat_emit, &tape));
	CHECK(tape.calls == 1U && tape.length == 2U
	    && memcmp(tape.prefix, "AB", 2U) == 0
	    && strcmp(paged_text, "AB") == 0
	    && strcmp(accumulator, "NS") == 0 && newline_flag == 0.0f
	    && selected_key == '\r');

	memcpy(accumulator, "AB", 3U);
	memcpy(paged_text, "old", 4U);
	newline_flag = -1.0f;
	selected_key = 0x12U;
	memset(&tape, 0, sizeof(tape));
	tape.accumulator = accumulator;
	tape.newline_flag = &newline_flag;
	tape.fail = true;
	CHECK(!yt_input_ab36_repeat_run(accumulator, sizeof(accumulator),
	    saved, sizeof(saved), paged_text, sizeof(paged_text),
	    &newline_flag, &selected_key,
	    ab36_repeat_emit, &tape));
	CHECK(tape.calls == 1U && tape.length == 2U
	    && memcmp(tape.prefix, "AB", 2U) == 0
	    && strcmp(paged_text, "AB") == 0
	    && strcmp(accumulator, "X") == 0 && newline_flag == 1.0f
	    && selected_key == 0x12U);
}

struct ab36_submit_tape {
	float *newline_flag;
	size_t calls;
	bool fail;
};

static bool
ab36_submit_line(void *context)
{
	struct ab36_submit_tape *tape = context;

	++tape->calls;
	CHECK(*tape->newline_flag == 0.0f);
	return !tape->fail;
}

static void
test_ab36_submission(void)
{
	float newline_flag = 1.0f;
	struct ab36_submit_tape tape = {
		.newline_flag = &newline_flag,
	};

	CHECK(yt_input_ab36_submit_requested('\r'));
	CHECK(!yt_input_ab36_submit_requested(0x12U));
	CHECK(!yt_input_ab36_submit_requested('\b'));
	CHECK(!yt_input_ab36_submit_requested('A'));
	CHECK(yt_input_ab36_submit_run(&newline_flag, ab36_submit_line, &tape));
	CHECK(tape.calls == 1U && newline_flag == 0.0f);

	newline_flag = 1.0f;
	tape.calls = 0U;
	tape.fail = true;
	CHECK(!yt_input_ab36_submit_run(&newline_flag, ab36_submit_line,
	    &tape));
	CHECK(tape.calls == 1U && newline_flag == 0.0f);
}

struct ab36_backspace_tape {
	char *accumulator;
	size_t calls;
	bool fail;
};

static bool
ab36_backspace_echo(void *context, const uint8_t *local,
    size_t local_length, const uint8_t *remote, size_t remote_length)
{
	static const uint8_t local_erase[] = {0x1d, ' ', 0x1d};
	static const uint8_t remote_erase[] = {'\b', ' ', '\b'};
	struct ab36_backspace_tape *tape = context;

	++tape->calls;
	CHECK(strcmp(tape->accumulator, "A") == 0);
	CHECK(local_length == sizeof(local_erase)
	    && memcmp(local, local_erase, sizeof(local_erase)) == 0);
	CHECK(remote_length == sizeof(remote_erase)
	    && memcmp(remote, remote_erase, sizeof(remote_erase)) == 0);
	return !tape->fail;
}

static void
test_ab36_backspace_transaction(void)
{
	char accumulator[8] = "AB";
	bool handled;
	struct ab36_backspace_tape tape = {
		.accumulator = accumulator,
	};

	CHECK(yt_input_ab36_backspace_run('\b', accumulator,
	    sizeof(accumulator), &handled, ab36_backspace_echo, &tape));
	CHECK(handled && tape.calls == 1U && strcmp(accumulator, "A") == 0);

	memcpy(accumulator, "AB", 3U);
	tape.calls = 0U;
	CHECK(yt_input_ab36_backspace_run(0x7fU, accumulator,
	    sizeof(accumulator), &handled, ab36_backspace_echo, &tape));
	CHECK(!handled && tape.calls == 0U && strcmp(accumulator, "AB") == 0);

	accumulator[0] = '\0';
	CHECK(yt_input_ab36_backspace_run('\b', accumulator,
	    sizeof(accumulator), &handled, ab36_backspace_echo, &tape));
	CHECK(!handled && tape.calls == 0U && accumulator[0] == '\0');

	memcpy(accumulator, "AB", 3U);
	tape.fail = true;
	CHECK(!yt_input_ab36_backspace_run('\b', accumulator,
	    sizeof(accumulator), &handled, ab36_backspace_echo, &tape));
	CHECK(handled && tape.calls == 1U && strcmp(accumulator, "A") == 0);
}

struct ab36_printable_tape {
	char *accumulator;
	char *paged_text;
	float *newline_flag;
	size_t echo_calls;
	size_t carrier_calls;
	bool fail_echo;
	bool fail_carrier;
};

static bool
ab36_printable_echo(void *context, const uint8_t *local,
    size_t local_length, const uint8_t *remote, size_t remote_length)
{
	struct ab36_printable_tape *tape = context;

	++tape->echo_calls;
	CHECK(strcmp(tape->accumulator, "A") == 0);
	CHECK(strcmp(tape->paged_text, "old") == 0);
	CHECK(*tape->newline_flag == -1.0f);
	CHECK(local_length == 1U && remote_length == 1U
	    && local[0] == remote[0]);
	return !tape->fail_echo;
}

static bool
ab36_printable_carrier(void *context)
{
	struct ab36_printable_tape *tape = context;

	++tape->carrier_calls;
	CHECK(tape->accumulator[0] == 'A'
	    && (uint8_t)tape->accumulator[1]
	    == (uint8_t)tape->paged_text[0]
	    && tape->accumulator[2] == '\0'
	    && tape->paged_text[1] == '\0'
	    && *tape->newline_flag == 1.0f);
	return !tape->fail_carrier;
}

static void
test_ab36_printable_transaction(void)
{
	char accumulator[8] = "A";
	char paged_text[8] = "old";
	float newline_flag = -1.0f;
	bool handled;
	struct ab36_printable_tape tape = {
		.accumulator = accumulator,
		.paged_text = paged_text,
		.newline_flag = &newline_flag,
	};

	CHECK(yt_input_ab36_printable_run(0x7fU, accumulator,
	    sizeof(accumulator), sizeof(accumulator), paged_text,
	    sizeof(paged_text), &newline_flag, &handled,
	    ab36_printable_echo, ab36_printable_carrier, &tape));
	CHECK(handled && tape.echo_calls == 1U && tape.carrier_calls == 1U
	    && (uint8_t)accumulator[1] == 0x7fU
	    && (uint8_t)paged_text[0] == 0x7fU && newline_flag == 1.0f);

	memcpy(accumulator, "A", 2U);
	memcpy(paged_text, "old", 4U);
	newline_flag = -1.0f;
	memset(&tape, 0, sizeof(tape));
	tape.accumulator = accumulator;
	tape.paged_text = paged_text;
	tape.newline_flag = &newline_flag;
	CHECK(yt_input_ab36_printable_run(0x1fU, accumulator,
	    sizeof(accumulator), sizeof(accumulator), paged_text,
	    sizeof(paged_text), &newline_flag, &handled,
	    ab36_printable_echo, ab36_printable_carrier, &tape));
	CHECK(!handled && tape.echo_calls == 0U && tape.carrier_calls == 0U
	    && strcmp(accumulator, "A") == 0
	    && strcmp(paged_text, "old") == 0 && newline_flag == -1.0f);
	CHECK(yt_input_ab36_printable_run(0x80U, accumulator,
	    sizeof(accumulator), sizeof(accumulator), paged_text,
	    sizeof(paged_text), &newline_flag, &handled,
	    ab36_printable_echo, ab36_printable_carrier, &tape));
	CHECK(!handled && tape.echo_calls == 0U && tape.carrier_calls == 0U
	    && strcmp(accumulator, "A") == 0
	    && strcmp(paged_text, "old") == 0 && newline_flag == -1.0f);

	tape.fail_echo = true;
	CHECK(!yt_input_ab36_printable_run(0x20U, accumulator,
	    sizeof(accumulator), sizeof(accumulator), paged_text,
	    sizeof(paged_text), &newline_flag, &handled,
	    ab36_printable_echo, ab36_printable_carrier, &tape));
	CHECK(handled && tape.echo_calls == 1U && tape.carrier_calls == 0U
	    && strcmp(accumulator, "A") == 0
	    && strcmp(paged_text, "old") == 0 && newline_flag == -1.0f);

	tape.fail_echo = false;
	tape.fail_carrier = true;
	tape.echo_calls = 0U;
	CHECK(!yt_input_ab36_printable_run('B', accumulator,
	    sizeof(accumulator), sizeof(accumulator), paged_text,
	    sizeof(paged_text), &newline_flag, &handled,
	    ab36_printable_echo, ab36_printable_carrier, &tape));
	CHECK(handled && tape.echo_calls == 1U && tape.carrier_calls == 1U
	    && strcmp(accumulator, "AB") == 0
	    && strcmp(paged_text, "B") == 0 && newline_flag == 1.0f);
}

static void
test_command_save_gate(void)
{
	bool requested;

	CHECK(yt_input_command_save_requested("", 1U, &requested)
	    && !requested);
	CHECK(yt_input_command_save_requested("A", 2U, &requested)
	    && !requested);
	CHECK(yt_input_command_save_requested("A/", 3U, &requested)
	    && requested);
	CHECK(yt_input_command_save_requested("/", 2U, &requested)
	    && requested);
	CHECK(yt_input_command_save_requested("A//", 4U, &requested)
	    && requested);
	CHECK(!yt_input_command_save_requested("A", 1U, &requested));
	CHECK(!yt_input_command_save_requested("A", 2U, NULL));
}

struct ab36_terminal_tape {
	uint8_t notice[64];
	size_t notice_length;
	size_t calls;
	size_t fail_call;
};

static bool
ab36_terminal_notice(void *context, const uint8_t *notice, size_t length)
{
	struct ab36_terminal_tape *tape = context;

	++tape->calls;
	if (length > sizeof(tape->notice))
		return false;
	memcpy(tape->notice, notice, length);
	tape->notice_length = length;
	return tape->calls != tape->fail_call;
}

static bool
ab36_terminal_close(void *context)
{
	struct ab36_terminal_tape *tape = context;

	++tape->calls;
	return tape->calls != tape->fail_call;
}

static void
test_ab36_terminal_transaction(void)
{
	static const uint8_t inactivity[] = "\aUSER FELL ASLEEP!";
	static const uint8_t session_limit[] =
	    "\a\a\aTIME LIMIT EXCEEDED!\a\a\a";
	struct ab36_terminal_tape tape;
	bool running;
	bool terminated;

	memset(&tape, 0, sizeof(tape));
	running = true;
	terminated = false;
	CHECK(yt_input_ab36_terminal_run(YT_AB36_TERMINAL_INACTIVITY,
	    &running, &terminated, ab36_terminal_notice,
	    ab36_terminal_close, &tape));
	CHECK(tape.calls == 2U
	    && tape.notice_length == sizeof(inactivity) - 1U
	    && memcmp(tape.notice, inactivity, sizeof(inactivity) - 1U) == 0
	    && !running && terminated);

	memset(&tape, 0, sizeof(tape));
	running = true;
	terminated = false;
	CHECK(yt_input_ab36_terminal_run(YT_AB36_TERMINAL_SESSION_LIMIT,
	    &running, &terminated, ab36_terminal_notice,
	    ab36_terminal_close, &tape));
	CHECK(tape.calls == 2U
	    && tape.notice_length == sizeof(session_limit) - 1U
	    && memcmp(tape.notice, session_limit,
	    sizeof(session_limit) - 1U) == 0 && !running && terminated);

	for (tape.fail_call = 1U; tape.fail_call <= 2U; ++tape.fail_call) {
		size_t fail_call = tape.fail_call;

		memset(&tape, 0, sizeof(tape));
		tape.fail_call = fail_call;
		running = true;
		terminated = false;
		CHECK(!yt_input_ab36_terminal_run(
		    YT_AB36_TERMINAL_INACTIVITY, &running, &terminated,
		    ab36_terminal_notice, ab36_terminal_close, &tape));
		CHECK(tape.calls == fail_call && running && !terminated);
	}
	CHECK(!yt_input_ab36_terminal_run((enum yt_ab36_terminal_kind)99,
	    &running, &terminated, ab36_terminal_notice,
	    ab36_terminal_close, &tape));
}

static void
test_source_fifo(void)
{
	struct yt_input_splitter splitter;
	struct yt_input_value a = one('A');
	struct yt_input_value b = one('B');
	struct yt_input_value local = one('L');
	struct yt_input_value selected;

	yt_input_splitter_init(&splitter);
	CHECK(yt_input_splitter_push(&splitter, true, &a));
	CHECK(yt_input_splitter_push(&splitter, false, &local));
	CHECK(yt_input_splitter_push(&splitter, true, &b));
	selected = yt_input_splitter_select_source(&splitter, false);
	CHECK(selected.length == 1 && selected.bytes[0] == 'L'
	    && !selected.remote && splitter.remote.length == 2);
	selected = yt_input_splitter_select_source(&splitter, false);
	CHECK(selected.length == 0);
	selected = yt_input_splitter_select_source(&splitter, true);
	CHECK(selected.length == 1 && selected.bytes[0] == 'A'
	    && selected.remote);
	selected = yt_input_splitter_select_source(&splitter, true);
	CHECK(selected.length == 1 && selected.bytes[0] == 'B'
	    && selected.remote);
	selected = yt_input_splitter_select_source(&splitter, true);
	CHECK(selected.length == 0);
}

static void
test_b05d_keys(void)
{
	char accumulator[16] = "typed";
	char queue[16] = "old";
	char pager[8] = "";
	size_t position = 1;
	size_t length = 3;
	struct yt_b05d_key_state state = {
		.accumulator = accumulator,
		.accumulator_capacity = sizeof(accumulator),
		.queue = queue,
		.queue_capacity = sizeof(queue),
		.queue_position = &position,
		.queue_length = &length,
		.pager_key = pager,
		.pager_key_capacity = sizeof(pager),
	};
	struct yt_input_value value = one(0x12);

	CHECK(yt_b05d_process_key(&value, &state));
	CHECK(position == 1 && length == 3 && memcmp(queue, "old", 3) == 0);
	value = one('!');
	CHECK(yt_b05d_process_key(&value, &state));
	CHECK(position == 0 && length == 3 && memcmp(queue, "ld!", 3) == 0);
	value = one('\r');
	CHECK(yt_b05d_process_key(&value, &state));
	CHECK(length == 4 && memcmp(queue, "ld!\r", 4) == 0);
	value = one(0x7f);
	CHECK(yt_b05d_process_key(&value, &state));
	CHECK(length == 4);
	value = one(0x1f);
	CHECK(yt_b05d_process_key(&value, &state));
	CHECK(length == 4);
	value = one(' ');
	CHECK(yt_b05d_process_key(&value, &state));
	CHECK(length == 5 && queue[4] == ' ');
	value = one('~');
	CHECK(yt_b05d_process_key(&value, &state));
	CHECK(length == 6 && queue[5] == '~');
	value.bytes[0] = 0;
	value.bytes[1] = 0x48;
	value.length = 2;
	CHECK(yt_b05d_process_key(&value, &state));
	CHECK(length == 6);
	position = 0U;
	length = 0U;
	queue[0] = '\0';
	value = one(0x12);
	CHECK(yt_b05d_process_key(&value, &state));
	CHECK(position == 0U && length == 0U && queue[0] == '\0');
	value = one(0x18);
	CHECK(yt_b05d_process_key(&value, &state));
	CHECK(accumulator[0] == '\0' && queue[0] == '\0');
	CHECK(position == 0 && length == 0 && strcmp(pager, "Q") == 0);
}

static void
test_repeat_transform(void)
{
	struct yt_repeat_transform result;
	char text[1024];
	char saved[1024] = "old";
	size_t index;
	size_t semicolons;

	snprintf(text, sizeof(text), "A/R3");
	CHECK(yt_input_expand_repeat(text, sizeof(text), saved, sizeof(saved),
	    &result));
	CHECK(result.emit_notice && result.count == 3.0f);
	CHECK(strcmp(text, "A;A;A") == 0 && strcmp(saved, "A;A;A") == 0);

	snprintf(saved, sizeof(saved), "old");
	snprintf(text, sizeof(text), "A/r2/B/R3");
	CHECK(yt_input_expand_repeat(text, sizeof(text), saved, sizeof(saved),
	    &result));
	CHECK(result.emit_notice && result.count == 2.0f);
	CHECK(strcmp(text, "A;A") == 0 && strcmp(saved, "A;A") == 0);

	snprintf(saved, sizeof(saved), "old");
	snprintf(text, sizeof(text), "ABC");
	CHECK(yt_input_expand_repeat(text, sizeof(text), saved, sizeof(saved),
	    &result));
	CHECK(!result.emit_notice && result.count == 0.0f
	    && strcmp(text, "ABC") == 0 && strcmp(saved, "old") == 0);

	snprintf(saved, sizeof(saved), "old");
	snprintf(text, sizeof(text), "A/R0");
	CHECK(yt_input_expand_repeat(text, sizeof(text), saved, sizeof(saved),
	    &result));
	CHECK(!result.emit_notice && result.count == 0.0f);
	CHECK(strcmp(text, "A;") == 0 && strcmp(saved, "old") == 0);

	snprintf(text, sizeof(text), "A/R-.1");
	CHECK(yt_input_expand_repeat(text, sizeof(text), saved, sizeof(saved),
	    &result));
	CHECK(!result.emit_notice && result.count == -1.0f);
	CHECK(strcmp(text, "A;") == 0 && strcmp(saved, "old") == 0);

	snprintf(text, sizeof(text), "A/R11");
	CHECK(yt_input_expand_repeat(text, sizeof(text), saved, sizeof(saved),
	    &result));
	CHECK(result.emit_notice && result.count == 20.0f);
	CHECK(strlen(text) == 39 && strcmp(text, saved) == 0);
	for (index = 0; index < strlen(text); index += 2)
		CHECK(text[index] == 'A');

	for (index = 0; index < 100; ++index)
		text[index] = 'A';
	snprintf(text + 100, sizeof(text) - 100, "/R10");
	CHECK(yt_input_expand_repeat(text, sizeof(text), saved, sizeof(saved),
	    &result));
	CHECK(result.emit_notice && result.count == 10.0f);
	CHECK(strlen(text) == 504 && strcmp(text, saved) == 0);
	semicolons = 0;
	for (index = 0; text[index] != '\0'; ++index) {
		if (text[index] == ';')
			++semicolons;
	}
	CHECK(semicolons == 4);

	for (index = 0; index < 4; ++index) {
		static const char *const suffixes[] = {
		    "3.9", "&H3", "&O3", "3garbage"
		};

		snprintf(text, sizeof(text), "A/R%s", suffixes[index]);
		CHECK(yt_input_expand_repeat(text, sizeof(text), saved,
		    sizeof(saved), &result));
		CHECK(result.emit_notice && result.count == 3.0f);
		CHECK(strcmp(text, "A;A;A") == 0);
	}
}

static void
test_semicolon_queue(void)
{
	char text[32];
	char queue[32];
	size_t position;
	size_t length;

	snprintf(text, sizeof(text), "ABC");
	snprintf(queue, sizeof(queue), "XYZ");
	position = 1U;
	length = 3U;
	CHECK(yt_input_split_semicolon(text, queue, sizeof(queue), &position,
	    &length));
	CHECK(strcmp(text, "ABC") == 0 && strcmp(queue, "XYZ") == 0
	    && position == 1U && length == 3U);

	snprintf(text, sizeof(text), "A;B;C");
	queue[0] = '\0';
	position = length = 0;
	CHECK(yt_input_split_semicolon(text, queue, sizeof(queue), &position,
	    &length));
	CHECK(strcmp(text, "A") == 0 && position == 0 && length == 4);
	CHECK(memcmp(queue, "B\rC\r", 5) == 0);

	snprintf(text, sizeof(text), "M;42");
	snprintf(queue, sizeof(queue), "X");
	position = 0;
	length = 1;
	CHECK(yt_input_split_semicolon(text, queue, sizeof(queue), &position,
	    &length));
	CHECK(strcmp(text, "M") == 0 && position == 0 && length == 4);
	CHECK(memcmp(queue, "42X\r", 5) == 0);

	snprintf(text, sizeof(text), "A;;Y");
	snprintf(queue, sizeof(queue), "old");
	position = 1;
	length = 3;
	CHECK(yt_input_split_semicolon(text, queue, sizeof(queue), &position,
	    &length));
	CHECK(strcmp(text, "A") == 0 && position == 0 && length == 5);
	CHECK(memcmp(queue, "\rYld\r", 6) == 0);

	snprintf(text, sizeof(text), "Q;");
	queue[0] = '\0';
	position = length = 0;
	CHECK(yt_input_split_semicolon(text, queue, sizeof(queue), &position,
	    &length));
	CHECK(strcmp(text, "Q") == 0 && position == 0 && length == 1);
	CHECK(memcmp(queue, "\r", 2) == 0);
}

static void
test_queue_program_prepend(void)
{
	static const char route[] = "1\rM\r 2";
	char queue[32] = "XXQ\r";
	size_t position = 2U;
	size_t length = 4U;

	CHECK(yt_input_queue_prepend_program(queue, sizeof(queue), &position,
	    &length, route, sizeof(route) - 1U));
	CHECK(position == 0U && length == 9U
	    && memcmp(queue, "1\rM\r 2\rQ\r", 10U) == 0);

	position = 0U;
	length = 9U;
	CHECK(!yt_input_queue_prepend_program(queue, 10U, &position, &length,
	    route, sizeof(route) - 1U)
	    && position == 0U && length == 9U
	    && memcmp(queue, "1\rM\r 2\rQ\r", 10U) == 0);
	CHECK(!yt_input_queue_prepend_program(NULL, sizeof(queue), &position,
	    &length, route, sizeof(route) - 1U));
}

static void
test_timed_wait(void)
{
	struct yt_timed_wait_state wait;
	struct yt_input_value value = {{0, 0}, 0, 0, false};
	enum yt_timed_wait_reason reason;

	memset(&wait, 0, sizeof(wait));
	memcpy(wait.serial_scratch, "old", 3);
	wait.serial_scratch_length = 3;
	CHECK(yt_timed_wait_begin(&wait, 33.0f, 0.1f));
	CHECK(wait.duration_cell == 33.099998474121094f
	    && wait.timer_reads == 1);
	reason = yt_timed_wait_timer(&wait, 33.1f);
	CHECK(reason == YT_TIMED_WAIT_TIMER && wait.timer_reads == 2
	    && wait.local_reads == 0 && wait.loc_reads == 0);
	CHECK(wait.serial_scratch_length == 3
	    && memcmp(wait.serial_scratch, "old", 3) == 0);

	/* YT:07F5 owns a distinct 99-second post-login caller cell. */
	memset(&wait, 0, sizeof(wait));
	CHECK(yt_timed_wait_begin(&wait, 99.0f, 0.1f));
	CHECK(wait.duration_cell == 99.099998474121094f
	    && wait.timer_reads == 1);
	CHECK(yt_timed_wait_timer(&wait, 99.099998474121094f)
	    == YT_TIMED_WAIT_TIMER);

	/* YT-SUB:A177 copies the stored MBF32 .33 heat-tick duration. */
	memset(&wait, 0, sizeof(wait));
	CHECK(yt_timed_wait_begin(&wait, 0.33000001311302185f, 10.0f));
	CHECK(wait.duration_cell == 10.329999923706055f
	    && wait.timer_reads == 1);
	CHECK(yt_timed_wait_timer(&wait, 10.329999923706055f)
	    == YT_TIMED_WAIT_TIMER);

	/* Other connected gameplay caller durations remain distinct cells. */
	memset(&wait, 0, sizeof(wait));
	CHECK(yt_timed_wait_begin(&wait, 0.5f, 100.0f)
	    && wait.duration_cell == 100.5f);
	memset(&wait, 0, sizeof(wait));
	CHECK(yt_timed_wait_begin(&wait, 1.0f, 100.0f)
	    && wait.duration_cell == 101.0f);
	memset(&wait, 0, sizeof(wait));
	CHECK(yt_timed_wait_begin(&wait, 2.0f, 100.0f)
	    && wait.duration_cell == 102.0f);
	memset(&wait, 0, sizeof(wait));
	CHECK(yt_timed_wait_begin(&wait, 5.0f, 100.0f)
	    && wait.duration_cell == 105.0f);
	memset(&wait, 0, sizeof(wait));
	CHECK(yt_timed_wait_begin(&wait, 9.0f, 100.0f)
	    && wait.duration_cell == 109.0f);
	memset(&wait, 0, sizeof(wait));
	CHECK(yt_timed_wait_begin(&wait, 10.0f, 100.0f)
	    && wait.duration_cell == 110.0f);
	memset(&wait, 0, sizeof(wait));
	CHECK(yt_timed_wait_begin(&wait, 4.0f, 100.0f)
	    && wait.duration_cell == 104.0f);
	memset(&wait, 0, sizeof(wait));
	CHECK(yt_timed_wait_begin(&wait, 15.0f, 100.0f)
	    && wait.duration_cell == 115.0f);
	memset(&wait, 0, sizeof(wait));
	CHECK(yt_timed_wait_begin(&wait, 0.4000000059604645f, 100.0f)
	    && wait.duration_cell == 100.40000152587890625f);
	memset(&wait, 0, sizeof(wait));
	CHECK(yt_timed_wait_begin(&wait, 0.004999999888241291f, 100.0f)
	    && wait.duration_cell == 100.00499725341796875f);
	memset(&wait, 0, sizeof(wait));
	CHECK(yt_timed_wait_begin(&wait, 3.0f, 100.0f)
	    && wait.duration_cell == 103.0f);

	memset(&wait, 0, sizeof(wait));
	memcpy(wait.serial_scratch, "old", 3);
	wait.serial_scratch_length = 3;
	CHECK(yt_timed_wait_begin(&wait, 33.0f, 100.0f));
	CHECK(yt_timed_wait_timer(&wait, 101.0f)
	    == YT_TIMED_WAIT_CONTINUE);
	value.bytes[0] = 0;
	value.bytes[1] = 0x3b;
	value.length = 2;
	value.remote = false;
	reason = yt_timed_wait_input(&wait, 0.0f, &value);
	CHECK(reason == YT_TIMED_WAIT_LOCAL && wait.timer_reads == 2
	    && wait.local_reads == 1 && wait.loc_reads == 0);
	CHECK(wait.serial_scratch_length == 3
	    && memcmp(wait.serial_scratch, "old", 3) == 0);

	memset(&wait, 0, sizeof(wait));
	memcpy(wait.serial_scratch, "old", 3);
	wait.serial_scratch_length = 3;
	CHECK(yt_timed_wait_begin(&wait, 33.0f, 100.0f));
	CHECK(yt_timed_wait_timer(&wait, 101.0f)
	    == YT_TIMED_WAIT_CONTINUE);
	value.length = 0;
	value.remote = false;
	CHECK(yt_timed_wait_input(&wait, 0.0f, &value)
	    == YT_TIMED_WAIT_CONTINUE);
	CHECK(yt_timed_wait_timer(&wait, 102.0f)
	    == YT_TIMED_WAIT_CONTINUE);
	value.bytes[0] = 'q';
	value.bytes[1] = 0;
	value.length = 1;
	value.remote = true;
	reason = yt_timed_wait_input(&wait, 0.0f, &value);
	CHECK(reason == YT_TIMED_WAIT_SERIAL && wait.timer_reads == 3
	    && wait.local_reads == 2 && wait.loc_reads == 2
	    && wait.serial_reads == 1);
	CHECK(wait.serial_scratch_length == 1
	    && wait.serial_scratch[0] == 'q');

	memset(&wait, 0, sizeof(wait));
	CHECK(yt_timed_wait_begin(&wait, 33.0f, 86390.0f));
	CHECK(wait.duration_cell == 86423.0f);
	CHECK(yt_timed_wait_timer(&wait, 0.0f)
	    == YT_TIMED_WAIT_CONTINUE);
	value.length = 0;
	value.remote = false;
	CHECK(yt_timed_wait_input(&wait, 2.0f, &value)
	    == YT_TIMED_WAIT_CONTINUE);
	CHECK(wait.loc_reads == 0);
	CHECK(yt_timed_wait_timer(&wait, 1.0f)
	    == YT_TIMED_WAIT_CONTINUE);
	value.bytes[0] = 'k';
	value.length = 1;
	value.remote = false;
	CHECK(yt_timed_wait_input(&wait, 2.0f, &value)
	    == YT_TIMED_WAIT_LOCAL);
	CHECK(wait.timer_reads == 3 && wait.local_reads == 2
	    && wait.loc_reads == 0);
}

static void
test_input_drain(void)
{
	struct yt_input_drain_state drain;
	struct yt_input_value initial = {{'o', 'k'}, 2, 0, false};
	struct yt_input_value value = {{0, 0}, 0, 0, false};
	enum yt_input_drain_reason reason;

	CHECK(yt_input_drain_begin(&drain, &initial));
	value.bytes[0] = 'A';
	value.length = 1;
	CHECK(yt_input_drain_local(&drain, &value)
	    == YT_INPUT_DRAIN_CONTINUE);
	value.bytes[0] = 'B';
	CHECK(yt_input_drain_local(&drain, &value)
	    == YT_INPUT_DRAIN_CONTINUE);
	value.bytes[0] = 0;
	value.bytes[1] = 0x3b;
	value.length = 2;
	CHECK(yt_input_drain_local(&drain, &value)
	    == YT_INPUT_DRAIN_CONTINUE);
	value.bytes[0] = 'Z';
	value.bytes[1] = 0;
	value.length = 1;
	CHECK(yt_input_drain_local(&drain, &value)
	    == YT_INPUT_DRAIN_CONTINUE);
	value.length = 0;
	reason = yt_input_drain_local(&drain, &value);
	CHECK(reason == YT_INPUT_DRAIN_LOCAL_COMPLETE
	    && drain.local_reads == 5 && drain.residue_length == 1
	    && drain.residue[0] == 'Z');
	CHECK(yt_input_drain_serial(&drain, 2.0f, &value)
	    == YT_INPUT_DRAIN_COMPLETE);
	CHECK(drain.loc_reads == 0 && drain.serial_reads == 0);

	CHECK(yt_input_drain_begin(&drain, &initial));
	value.bytes[0] = 'A';
	value.length = 1;
	CHECK(yt_input_drain_local(&drain, &value)
	    == YT_INPUT_DRAIN_CONTINUE);
	value.bytes[0] = 'B';
	CHECK(yt_input_drain_local(&drain, &value)
	    == YT_INPUT_DRAIN_CONTINUE);
	value.bytes[0] = 'C';
	CHECK(yt_input_drain_local(&drain, &value)
	    == YT_INPUT_DRAIN_CONTINUE);
	value.length = 0;
	CHECK(yt_input_drain_local(&drain, &value)
	    == YT_INPUT_DRAIN_CONTINUE);
	CHECK(drain.residue_length == 0 && drain.local_reads == 4);
	CHECK(yt_input_drain_local(&drain, &value)
	    == YT_INPUT_DRAIN_LOCAL_COMPLETE);
	CHECK(drain.local_reads == 5);

	CHECK(yt_input_drain_begin(&drain, &initial));
	value.length = 0;
	CHECK(yt_input_drain_local(&drain, &value)
	    == YT_INPUT_DRAIN_LOCAL_COMPLETE);
	value.bytes[0] = 'x';
	value.length = 1;
	value.remote = true;
	CHECK(yt_input_drain_serial(&drain, 0.0f, &value)
	    == YT_INPUT_DRAIN_CONTINUE);
	value.bytes[0] = 'y';
	CHECK(yt_input_drain_serial(&drain, 0.0f, &value)
	    == YT_INPUT_DRAIN_CONTINUE);
	value.length = 0;
	value.remote = false;
	CHECK(yt_input_drain_serial(&drain, 0.0f, &value)
	    == YT_INPUT_DRAIN_COMPLETE);
	CHECK(drain.residue_length == 1 && drain.residue[0] == 'y'
	    && drain.local_reads == 1 && drain.loc_reads == 3
	    && drain.serial_reads == 2);
}

static void
test_yes_no_candidate(void)
{
	char accumulator[32];
	char output_source[32];
	enum yt_yes_no_answer answer;

	snprintf(accumulator, sizeof(accumulator), "%s", "no");
	snprintf(output_source, sizeof(output_source), "%s", "old source");
	CHECK(yt_input_yes_no_candidate(accumulator, output_source,
	    sizeof(output_source), &answer));
	CHECK(strcmp(accumulator, "no") == 0 && strcmp(output_source, "N") == 0
	    && answer == YT_YES_NO_NO);

	snprintf(accumulator, sizeof(accumulator), "%s", "YES");
	CHECK(yt_input_yes_no_candidate(accumulator, output_source,
	    sizeof(output_source), &answer));
	CHECK(strcmp(accumulator, "YES") == 0 && strcmp(output_source, "Y") == 0
	    && answer == YT_YES_NO_YES);

	accumulator[0] = '\0';
	CHECK(yt_input_yes_no_candidate(accumulator, output_source,
	    sizeof(output_source), &answer));
	CHECK(output_source[0] == '\0' && answer == YT_YES_NO_EMPTY);

	snprintf(accumulator, sizeof(accumulator), "%s", " x");
	CHECK(yt_input_yes_no_candidate(accumulator, output_source,
	    sizeof(output_source), &answer));
	CHECK(strcmp(accumulator, " x") == 0 && strcmp(output_source, " ") == 0
	    && answer == YT_YES_NO_INVALID);

	snprintf(accumulator, sizeof(accumulator), "%s", "nonsense");
	CHECK(yt_input_yes_no_candidate(accumulator, output_source,
	    sizeof(output_source), &answer));
	CHECK(strcmp(accumulator, "nonsense") == 0
	    && strcmp(output_source, "N") == 0 && answer == YT_YES_NO_NO);

	CHECK(!yt_input_yes_no_candidate(accumulator, output_source, 1,
	    &answer));
	CHECK(!yt_input_yes_no_candidate("toolong", output_source, 4,
	    &answer));
}

static void
test_numeric_response(void)
{
	char text[32];

	text[0] = '\0';
	yt_input_numeric_response(text);
	CHECK(text[0] == '\0');
	snprintf(text, sizeof(text), "%s", "12.5");
	yt_input_numeric_response(text);
	CHECK(strcmp(text, "12.5") == 0);
	snprintf(text, sizeof(text), "%s", "abc");
	yt_input_numeric_response(text);
	CHECK(strcmp(text, "ABC") == 0);
	snprintf(text, sizeof(text), "%s", "1e2");
	yt_input_numeric_response(text);
	CHECK(text[0] == '\0');
	snprintf(text, sizeof(text), "%s", "never");
	yt_input_numeric_response(text);
	CHECK(text[0] == '\0');
	snprintf(text, sizeof(text), "%s", "e");
	yt_input_numeric_response(text);
	CHECK(text[0] == '\0');
}

static struct yt_sysop_chat_poll
chat_poll(const uint8_t *local, size_t local_length,
    const uint8_t *remote, size_t remote_length, uint8_t modem_status,
    int position_after_output)
{
	struct yt_sysop_chat_poll poll;

	memset(&poll, 0, sizeof(poll));
	if (local_length != 0U)
		memcpy(poll.local.bytes, local, local_length);
	poll.local.length = local_length;
	if (remote_length != 0U)
		memcpy(poll.remote.bytes, remote, remote_length);
	poll.remote.length = remote_length;
	poll.remote.remote = true;
	poll.modem_status = modem_status;
	poll.position_after_output = position_after_output;
	return poll;
}

static bool
chat_start(struct yt_sysop_chat_state *state, float mode, float snoop,
    float deadline, float timer)
{
	static const uint8_t accumulator[] = "LOOK";
	static const uint8_t queue[] = ";X";

	return yt_sysop_chat_begin(state, mode, snoop, deadline, timer, 280.0f,
	    accumulator, sizeof(accumulator) - 1U, queue, sizeof(queue) - 1U);
}

static void
test_sysop_chat(void)
{
	static const uint8_t local_erase[] = {0x1d, ' ', 0x1d};
	static const uint8_t remote_erase[] = {'\b', ' ', '\b'};
	static const uint8_t accepted[][2] = {
		{0x00, 0x00}, {0x01, 0x00}, {0x7f, 0x00}, {0x00, 0x4b}
	};
	static const size_t accepted_lengths[] = {1U, 1U, 1U, 2U};
	static const uint8_t rejected[][2] = {
		{0x80, 0x00}, {0xff, 0x00}, {0x7f, 0x41}
	};
	static const size_t rejected_lengths[] = {1U, 1U, 2U};
	struct yt_sysop_chat_state chat;
	struct yt_sysop_chat_state before;
	struct yt_sysop_chat_output output;
	struct yt_sysop_chat_poll poll;
	enum yt_sysop_chat_step_result step;
	uint8_t local[2];
	uint8_t remote[2] = {0, 0};
	size_t index;

	CHECK(chat_start(&chat, 0.0f, -1.0f, 700.0f, 100.75f));
	CHECK(chat.mode == 0.0f && chat.snoop == -1.0f
	    && chat.deadline == 700.0f && chat.saved_remaining == 600.0f
	    && chat.inactivity_deadline == 280.0f
	    && chat.foreground == 2.0f && chat.bold == 1.0f
	    && chat.newline_flag == 0.0f && chat.timer_reads == 1U
	    && chat.command_accumulator_length == 4U
	    && memcmp(chat.command_accumulator, "LOOK", 4U) == 0
	    && chat.queue_length == 2U && memcmp(chat.queue, ";X", 2U) == 0);
	local[0] = 'A';
	remote[0] = 'B';
	poll = chat_poll(local, 1U, remote, 1U, 0x80U, 10);
	step = yt_sysop_chat_step(&chat, &poll, &output);
	CHECK(step == YT_SYSOP_CHAT_CONTINUE && chat.key_length == 1U
	    && chat.key[0] == 'B' && chat.foreground == 3.0f
	    && chat.polls == 1U && chat.carrier_checks == 1U
	    && output.count == 2U);
	CHECK(output.events[0].destination == YT_SYSOP_CHAT_LOCAL_GATED
	    && !output.events[0].line && output.events[0].length == 1U
	    && output.events[0].data[0] == 'B');
	CHECK(output.events[1].destination == YT_SYSOP_CHAT_SERIAL
	    && !output.events[1].line && output.events[1].length == 1U
	    && output.events[1].data[0] == 'B');

	local[0] = 0x1b;
	poll = chat_poll(local, 1U, NULL, 0U, 0x80U, 10);
	CHECK(yt_sysop_chat_step(&chat, &poll, &output)
	    == YT_SYSOP_CHAT_CONTINUE);
	CHECK(!chat.exited && output.count == 2U
	    && output.events[1].destination == YT_SYSOP_CHAT_SERIAL
	    && output.events[1].data[0] == 0x1b);
	local[0] = 'X';
	poll = chat_poll(local, 1U, NULL, 0U, 0x80U, 10);
	CHECK(yt_sysop_chat_step(&chat, &poll, &output) == YT_SYSOP_CHAT_EXIT);
	CHECK(chat.exited && chat.polls == 2U && output.count == 0U);
	CHECK(yt_sysop_chat_finish(&chat, 160.9f, 161.1f));
	CHECK(chat.deadline == 760.0f && chat.inactivity_deadline == 401.0f
	    && chat.command_accumulator_length == 0U
	    && chat.queue_length == 1U && chat.queue[0] == '\r'
	    && chat.timer_reads == 3U);

	CHECK(chat_start(&chat, 2.0f, -1.0f, 700.0f, 100.0f));
	local[0] = '\r';
	poll = chat_poll(local, 1U, NULL, 0U, 0x80U, 0);
	CHECK(yt_sysop_chat_step(&chat, &poll, &output)
	    == YT_SYSOP_CHAT_CONTINUE);
	CHECK(chat.key_length == 0U && output.count == 2U
	    && output.events[0].destination == YT_SYSOP_CHAT_LOCAL_GATED
	    && output.events[0].line
	    && output.events[1].destination == YT_SYSOP_CHAT_SERIAL
	    && output.events[1].line);

	CHECK(chat_start(&chat, 0.0f, 0.0f, 700.0f, 100.0f));
	local[0] = 'A';
	poll = chat_poll(local, 1U, NULL, 0U, 0x80U, 1);
	CHECK(yt_sysop_chat_step(&chat, &poll, &output)
	    == YT_SYSOP_CHAT_CONTINUE);
	CHECK(output.count == 1U
	    && output.events[0].destination == YT_SYSOP_CHAT_SERIAL);
	CHECK(chat_start(&chat, 1.0f, -1.0f, 700.0f, 100.0f));
	CHECK(yt_sysop_chat_step(&chat, &poll, &output)
	    == YT_SYSOP_CHAT_CONTINUE);
	CHECK(output.count == 1U
	    && output.events[0].destination == YT_SYSOP_CHAT_LOCAL_GATED);
	CHECK(chat_start(&chat, 2.0f, -1.0f, 700.0f, 100.0f));
	remote[0] = 'B';
	poll = chat_poll(local, 1U, remote, 1U, 0x80U, 1);
	CHECK(yt_sysop_chat_step(&chat, &poll, &output)
	    == YT_SYSOP_CHAT_CONTINUE);
	CHECK(chat.key[0] == 'A' && chat.foreground == 6.0f
	    && output.count == 2U && output.events[0].data[0] == 'A'
	    && output.events[1].data[0] == 'A');

	CHECK(chat_start(&chat, 0.0f, -1.0f, 700.0f, 100.0f));
	poll = chat_poll(local, 1U, remote, 1U, 0x00U, 1);
	before = chat;
	CHECK(yt_sysop_chat_step(&chat, &poll, &output)
	    == YT_SYSOP_CHAT_CARRIER_END);
	CHECK(chat.terminated && chat.foreground == 3.0f
	    && chat.polls == 1U && chat.carrier_checks == 1U
	    && output.count == 0U);
	CHECK(yt_sysop_chat_finish(&chat, 160.0f, 160.0f));
	CHECK(chat.deadline == before.deadline
	    && chat.inactivity_deadline == before.inactivity_deadline
	    && chat.command_accumulator_length
	    == before.command_accumulator_length
	    && chat.queue_length == before.queue_length);
	CHECK(chat_start(&chat, 1.0f, -1.0f, 700.0f, 100.0f));
	poll = chat_poll(NULL, 0U, NULL, 0U, 0x00U, 0);
	CHECK(yt_sysop_chat_step(&chat, &poll, &output)
	    == YT_SYSOP_CHAT_CONTINUE);
	CHECK(!chat.terminated && chat.carrier_checks == 1U);

	for (index = 0; index < sizeof(accepted_lengths)
	    / sizeof(accepted_lengths[0]); ++index) {
		CHECK(chat_start(&chat, 0.0f, 0.0f, 700.0f, 100.0f));
		poll = chat_poll(accepted[index], accepted_lengths[index],
		    NULL, 0U, 0x80U, 1);
		CHECK(yt_sysop_chat_step(&chat, &poll, &output)
		    == YT_SYSOP_CHAT_CONTINUE);
		CHECK(output.count == 1U
		    && output.events[0].destination == YT_SYSOP_CHAT_SERIAL
		    && output.events[0].length == accepted_lengths[index]
		    && memcmp(output.events[0].data, accepted[index],
		    accepted_lengths[index]) == 0);
	}
	CHECK(chat_start(&chat, 0.0f, 0.0f, 700.0f, 100.0f));
	poll = chat_poll(NULL, 0U, NULL, 0U, 0x80U, 1);
	CHECK(yt_sysop_chat_step(&chat, &poll, &output)
	    == YT_SYSOP_CHAT_CONTINUE && output.count == 0U);
	for (index = 0; index < sizeof(rejected_lengths)
	    / sizeof(rejected_lengths[0]); ++index) {
		CHECK(chat_start(&chat, 0.0f, 0.0f, 700.0f, 100.0f));
		poll = chat_poll(rejected[index], rejected_lengths[index],
		    NULL, 0U, 0x80U, 1);
		CHECK(yt_sysop_chat_step(&chat, &poll, &output)
		    == YT_SYSOP_CHAT_CONTINUE && output.count == 0U);
	}

	CHECK(chat_start(&chat, 0.0f, 0.0f, 700.0f, 100.0f));
	local[0] = '\b';
	poll = chat_poll(local, 1U, NULL, 0U, 0x80U, 1);
	CHECK(yt_sysop_chat_step(&chat, &poll, &output)
	    == YT_SYSOP_CHAT_CONTINUE);
	CHECK(output.count == 2U
	    && output.events[0].destination == YT_SYSOP_CHAT_LOCAL_RAW
	    && output.events[0].length == sizeof(local_erase)
	    && memcmp(output.events[0].data, local_erase,
	    sizeof(local_erase)) == 0
	    && output.events[1].destination == YT_SYSOP_CHAT_SERIAL
	    && output.events[1].length == sizeof(remote_erase)
	    && memcmp(output.events[1].data, remote_erase,
	    sizeof(remote_erase)) == 0);
	CHECK(chat_start(&chat, 2.0f, 0.0f, 700.0f, 100.0f));
	CHECK(yt_sysop_chat_step(&chat, &poll, &output)
	    == YT_SYSOP_CHAT_CONTINUE);
	CHECK(output.count == 1U
	    && output.events[0].destination == YT_SYSOP_CHAT_LOCAL_RAW);
	CHECK(chat_start(&chat, 0.0f, 0.0f, 700.0f, 100.0f));
	poll.position_after_output = 0;
	CHECK(yt_sysop_chat_step(&chat, &poll, &output)
	    == YT_SYSOP_CHAT_CONTINUE);
	CHECK(output.count == 1U && output.events[0].length == 1U
	    && output.events[0].data[0] == '\b');

	local[0] = ' ';
	for (index = 0; index < 5U; ++index) {
		static const int positions[] = {70, 71, 79, 80, 80};
		static const uint8_t keys[] = {' ', ' ', 'A', 'A', ' '};
		static const size_t counts[] = {2U, 4U, 2U, 4U, 4U};

		CHECK(chat_start(&chat, 0.0f, -1.0f, 700.0f, 100.0f));
		local[0] = keys[index];
		poll = chat_poll(local, 1U, NULL, 0U, 0x80U,
		    positions[index]);
		CHECK(yt_sysop_chat_step(&chat, &poll, &output)
		    == YT_SYSOP_CHAT_CONTINUE);
		CHECK(output.count == counts[index]);
		if (counts[index] == 4U)
			CHECK(output.events[2].line && output.events[3].line);
	}

	CHECK(chat_start(&chat, 0.0f, -1.0f, 90.0f, 100.0f));
	chat.exited = true;
	CHECK(yt_sysop_chat_finish(&chat, 160.0f, 160.0f));
	CHECK(chat.deadline == 150.0f && chat.inactivity_deadline == 400.0f);
	CHECK(chat_start(&chat, 0.0f, -1.0f, 86500.0f, 86390.0f));
	chat.exited = true;
	CHECK(yt_sysop_chat_finish(&chat, 20.0f, 20.0f));
	CHECK(chat.deadline == 130.0f && chat.inactivity_deadline == 260.0f);
	CHECK(chat_start(&chat, 0.0f, -1.0f, 700.0f, 100.0f));
	before = chat;
	CHECK(yt_sysop_chat_finish(&chat, 160.0f, 161.0f));
	CHECK(memcmp(&chat, &before, sizeof(chat)) == 0);

	CHECK(chat_start(&chat, 0.0f, -1.0f, 700.0f, 100.0f));
	poll = chat_poll(local, 1U, remote, 2U, 0x80U, 1);
	CHECK(yt_sysop_chat_step(&chat, &poll, &output)
	    == YT_SYSOP_CHAT_INVALID);
}

static void
test_main_startup_prefix(void)
{
	static const uint16_t keys[] = {4U, 5U, 8U, 9U, 10U};
	static const uint16_t handlers[] =
	    {0xA9E1U, 0xBA00U, 0xB6D7U, 0xB66AU, 0xB3FBU};
	static const char *const labels[] = {
		"Ore..........", "Organics.....", "Equipment....",
		"Fighters.....", "Missiles.....", "Mines........",
		"Credits......", "Forces.......", "Plasma bolts.", "YT.REG",
	};
	static const uint8_t signature[] =
	    {0x00, 0x00, 0xa8, 0x43, 0x3b, 0x6a, 0x4f, 0xa5};
	uint8_t first[] = "  aDA   loVELACE ";
	uint8_t last[] = " o'NEIL-jR ";
	struct yt_startup_main_prefix prefix;
	size_t first_length = sizeof(first) - 1U;
	size_t last_length = sizeof(last) - 1U;
	size_t index;

	CHECK(yt_startup_main_prefix_begin(&prefix));
	CHECK(prefix.key_count == YT_ARRAY_LEN(keys)
	    && prefix.main_error_handler_installed
	    && prefix.main_error_handler == 0xB2DAU
	    && !prefix.serial_setup_entered && prefix.continuation == 0U);
	for (index = 0U; index < YT_ARRAY_LEN(keys); ++index)
		CHECK(prefix.key[index].key == keys[index]
		    && prefix.key[index].handler == handlers[index]
		    && prefix.key[index].enabled);
	CHECK(yt_startup_main_prefix_finish(first, &first_length, last,
	    &last_length, &prefix));
	CHECK(first_length == 12U && memcmp(first, "Ada Lovelace", 12U) == 0);
	CHECK(last_length == 9U && memcmp(last, "O'neil-Jr", 9U) == 0);
	CHECK(prefix.key_count == YT_ARRAY_LEN(keys)
	    && prefix.main_error_handler_installed
	    && prefix.main_error_handler == 0xB2DAU
	    && prefix.serial_setup_entered && prefix.continuation == 0x0409U);
	CHECK(prefix.carriage_return == '\r' && prefix.line_feed == '\n'
	    && memcmp(prefix.local_erase, "\x1d \x1d", 3U) == 0
	    && memcmp(prefix.remote_erase, "\x08 \x08", 3U) == 0
	    && memcmp(prefix.registration_signature, signature,
	    sizeof(signature)) == 0);
	for (index = 0U; index < YT_ARRAY_LEN(labels); ++index)
		CHECK(prefix.display_label_length[index] == strlen(labels[index])
		    && memcmp(prefix.display_label[index], labels[index],
		    strlen(labels[index])) == 0);
	first_length = 0U;
	last_length = 0U;
	CHECK(yt_startup_main_prefix_compose(NULL, &first_length, NULL,
	    &last_length, &prefix));
	memset(&prefix, 0, sizeof(prefix));
	CHECK(!yt_startup_main_prefix_finish(NULL, &first_length, NULL,
	    &last_length, &prefix));
	CHECK(!yt_startup_main_prefix_begin(NULL));
	CHECK(!yt_startup_main_prefix_compose(NULL, NULL, NULL,
	    &last_length, &prefix));
	first_length = 1U;
	CHECK(!yt_startup_main_prefix_compose(NULL, &first_length, NULL,
	    &last_length, &prefix));
	first_length = 0U;
	CHECK(!yt_startup_main_prefix_compose(NULL, &first_length, NULL,
	    &last_length, NULL));
}

static void
test_serial_startup_model(void)
{
	static const struct {
		uint8_t dll;
		uint8_t dlm;
		float baud;
	} divisors[] = {
		{1U, 0U, 115200.0f},
		{2U, 0U, 57600.0f},
		{3U, 0U, 38400.0f},
		{0U, 6U, 75.0f},
	};
	struct yt_startup_command_split split;
	struct yt_startup_entry_result entry;
	struct yt_startup_framing framing;
	struct yt_startup_serial_layout layout;
	uint8_t text[YT_STARTUP_COMMAND_SIZE];
	uint8_t dll;
	uint8_t dlm;
	float baud;
	size_t length;
	size_t index;

	CHECK(yt_startup_split_command((const uint8_t *)"A B C", 5U,
	    &split));
	CHECK(split.path_length == 2U
	    && memcmp(split.path, "A ", 2U) == 0
	    && split.remainder_length == 3U
	    && memcmp(split.remainder, "B C", 3U) == 0);
	CHECK(yt_startup_split_command((const uint8_t *)" A", 2U, &split)
	    && split.path_length == 1U && split.path[0] == ' '
	    && split.remainder_length == 1U && split.remainder[0] == 'A');
	CHECK(yt_startup_split_command(NULL, 0U, &split)
	    && split.path_length == 0U && split.remainder_length == 0U);
	CHECK(!yt_startup_split_command(NULL, 1U, &split));
	CHECK(yt_startup_compose_entry((const uint8_t *)"A B C", 5U,
	    &entry));
	CHECK(entry.outcome == YT_STARTUP_ENTRY_CONTINUE
	    && entry.handler_installed && entry.installed_handler == 0x45F7U
	    && !entry.process_end && entry.command.path_length == 2U
	    && memcmp(entry.command.path, "A ", 2U) == 0
	    && entry.command.remainder_length == 3U
	    && memcmp(entry.command.remainder, "B C", 3U) == 0);
	CHECK(yt_startup_compose_entry(NULL, 0U, &entry));
	CHECK(entry.outcome == YT_STARTUP_ENTRY_MISSING_COMMAND_END
	    && entry.handler_installed && entry.installed_handler == 0x45F7U
	    && entry.process_end && entry.exit_status == 0
	    && entry.command.path_length == 0U
	    && entry.command.remainder_length == 0U);
	CHECK(!yt_startup_compose_entry(NULL, 1U, &entry));
	CHECK(!yt_startup_compose_entry(NULL, 0U, NULL));

	CHECK(yt_startup_parse_port((const uint8_t *)"COM3:", 5U) == 3);
	CHECK(yt_startup_parse_port((const uint8_t *)"COM2 ", 5U) == 0);
	CHECK(yt_startup_parse_port((const uint8_t *)"9", 1U) == 9);
	CHECK(yt_startup_parse_port(NULL, 0U) == 0);
	CHECK(yt_startup_serial_layout(3, &layout)
	    && layout.brun_device == 1 && layout.uart_base == 0x3E8U
	    && layout.modem_status_port == 0x3EEU
	    && layout.bios_address == 0x400U
	    && layout.bios_value == 0x03E8U);
	CHECK(yt_startup_serial_layout(4, &layout)
	    && layout.brun_device == 2 && layout.uart_base == 0x2E8U
	    && layout.modem_status_port == 0x2EEU
	    && layout.bios_address == 0x402U
	    && layout.bios_value == 0x02E8U);
	CHECK(!yt_startup_serial_layout(0, &layout));

	for (index = 0U; index < YT_ARRAY_LEN(divisors); ++index) {
		CHECK(yt_startup_detect_baud(divisors[index].dll,
		    divisors[index].dlm, &baud));
		CHECK(baud == divisors[index].baud);
		CHECK(yt_startup_restored_divisor(baud, &dll, &dlm)
		    && dll == divisors[index].dll
		    && dlm == divisors[index].dlm);
	}
	CHECK(yt_startup_detect_baud(255U, 255U, &baud));
	CHECK(baud == 115200.0f / 65535.0f);
	CHECK(yt_startup_restored_divisor(baud, &dll, &dlm)
	    && dll == 255U && dlm == 255U);
	CHECK(!yt_startup_detect_baud(0U, 0U, &baud));
	CHECK(!yt_startup_restored_divisor(0.0f, &dll, &dlm));
	CHECK(yt_startup_divisor_from_observed_baud(38400U, &dll, &dlm)
	    && dll == 3U && dlm == 0U);
	CHECK(yt_startup_divisor_from_observed_baud(75U, &dll, &dlm)
	    && dll == 0U && dlm == 6U);
	CHECK(!yt_startup_divisor_from_observed_baud(0U, &dll, &dlm));
	CHECK(!yt_startup_divisor_from_observed_baud(230400U, &dll, &dlm));
	CHECK(!yt_startup_divisor_from_observed_baud(12345U, &dll, &dlm));
	CHECK(!yt_startup_divisor_from_observed_baud(38400U, NULL, &dlm));
	CHECK(!yt_startup_divisor_from_observed_baud(38400U, &dll, NULL));
	CHECK(yt_startup_framing_compose(
	    (const uint8_t *)"38400 BAUD,O,8,1",
	    strlen("38400 BAUD,O,8,1"), &framing)
	    && framing.opening_baud == 1200U
	    && framing.parity == YT_STARTUP_PARITY_NONE
	    && framing.data_bits == 8U && framing.stop_bits == 1U);
	CHECK(yt_startup_framing_compose((const uint8_t *)"7,N,O", 5U,
	    &framing) && framing.opening_baud == 1200U
	    && framing.parity == YT_STARTUP_PARITY_EVEN
	    && framing.data_bits == 7U && framing.stop_bits == 1U);
	CHECK(!yt_startup_framing_compose(NULL, 1U, &framing));
	CHECK(!yt_startup_framing_compose(NULL, 0U, NULL));

	CHECK(yt_startup_open_spec(3,
	    (const uint8_t *)"57600 BAUD,E,7,1",
	    strlen("57600 BAUD,E,7,1"), text, sizeof(text),
	    &length));
	CHECK(length == strlen("COM1:1200,E,7,1,CS65535,DS,CD")
	    && memcmp(text, "COM1:1200,E,7,1,CS65535,DS,CD", length) == 0);
	CHECK(yt_startup_open_spec(2,
	    (const uint8_t *)"38400 BAUD,O,8,1",
	    strlen("38400 BAUD,O,8,1"), text, sizeof(text),
	    &length));
	CHECK(length == strlen("COM2:1200,N,8,1,CS65535,DS,CD")
	    && memcmp(text, "COM2:1200,N,8,1,CS65535,DS,CD", length) == 0);
	CHECK(!yt_startup_open_spec(0, NULL, 0U, text, sizeof(text),
	    &length));

	CHECK(yt_startup_session_deadline(100.9f, 60.0, 100.9f)
	    == 3697.0f);
	CHECK(yt_startup_session_deadline(86399.9f, 180.0, 0.1f)
	    == 10800.0f);
	CHECK(yt_startup_session_deadline(100.9f, -5.0, 200.2f)
	    == -203.0f);
	CHECK(yt_startup_session_deadline(100.9f, 1000.0, 200.2f)
	    == 11000.0f);

	CHECK(yt_startup_canonical_name((const uint8_t *)"  jANE", 6U,
	    (const uint8_t *)"  o'NEIL  ", 10U, text, sizeof(text), &length));
	CHECK(length == strlen("Jane O'neil")
	    && memcmp(text, "Jane O'neil", length) == 0);
	CHECK(!yt_startup_canonical_name(NULL, 1U, NULL, 0U, text,
	    sizeof(text), &length));
}

#ifndef _WIN32
static void
test_platform_rmt_serial(void)
{
	struct yt_platform_rmt_serial serial;
	struct yt_startup_framing framing = {
		.opening_baud = 1200U,
		.parity = YT_STARTUP_PARITY_EVEN,
		.data_bits = 7U,
		.stop_bits = 1U,
	};
	struct yt_error error;
	struct termios terminal;
	char *slave_name;
	int saved_stdin = -1;
	int master = -1;
	int slave = -1;
	bool replaced = false;

	yt_error_clear(&error);
	memset(&serial, 0, sizeof(serial));
	CHECK(!yt_platform_rmt_serial_prepare(0, &framing, &serial, &error)
	    && error.status == YT_INVALID && !serial.prepared);
	master = posix_openpt(O_RDWR | O_NOCTTY);
	CHECK(master >= 0);
	if (master < 0)
		goto done;
	CHECK(grantpt(master) == 0 && unlockpt(master) == 0);
	slave_name = ptsname(master);
	CHECK(slave_name != NULL);
	if (slave_name == NULL)
		goto done;
	slave = open(slave_name, O_RDWR | O_NOCTTY);
	CHECK(slave >= 0);
	if (slave < 0)
		goto done;
	CHECK(tcgetattr(slave, &terminal) == 0);
	terminal.c_cflag &= (tcflag_t)~(CSIZE | PARENB | PARODD | CSTOPB);
	terminal.c_cflag |= CS8;
	CHECK(cfsetispeed(&terminal, B38400) == 0
	    && cfsetospeed(&terminal, B38400) == 0
	    && tcsetattr(slave, TCSANOW, &terminal) == 0);
	saved_stdin = dup(STDIN_FILENO);
	CHECK(saved_stdin >= 0 && dup2(slave, STDIN_FILENO) == STDIN_FILENO);
	if (saved_stdin < 0)
		goto done;
	replaced = true;
	yt_error_clear(&error);
	CHECK(yt_platform_rmt_serial_prepare(3, &framing, &serial, &error)
	    && serial.prepared && !serial.restored && !serial.owns_handle
	    && serial.native_handle == 0U && serial.observed_baud == 38400U);
	CHECK(tcgetattr(STDIN_FILENO, &terminal) == 0
	    && cfgetispeed(&terminal) == B1200
	    && cfgetospeed(&terminal) == B1200
	    && (terminal.c_oflag & OPOST) == 0
	    && (terminal.c_cflag & CSIZE) == CS7
	    && (terminal.c_cflag & PARENB) != 0);
	CHECK(yt_platform_rmt_serial_restore(&serial, &error)
	    && serial.restored
	    && tcgetattr(STDIN_FILENO, &terminal) == 0
	    && cfgetispeed(&terminal) == B38400
	    && cfgetospeed(&terminal) == B38400
	    && (terminal.c_oflag & OPOST) == 0
	    && (terminal.c_cflag & CSIZE) == CS7
	    && (terminal.c_cflag & PARENB) != 0);
	CHECK(!yt_platform_rmt_serial_restore(&serial, &error)
	    && error.status == YT_INVALID);
	yt_platform_rmt_serial_close(&serial);
	CHECK(!serial.prepared && !serial.restored);

	CHECK(tcgetattr(STDIN_FILENO, &terminal) == 0);
	terminal.c_cflag &= (tcflag_t)~(CSIZE | PARENB | PARODD | CSTOPB);
	terminal.c_cflag |= CS8;
	CHECK(tcsetattr(STDIN_FILENO, TCSANOW, &terminal) == 0);
	framing.parity = YT_STARTUP_PARITY_NONE;
	framing.data_bits = 8U;
	CHECK(yt_platform_rmt_serial_prepare(1, &framing, &serial, &error)
	    && serial.prepared && !serial.restored);
	yt_platform_rmt_serial_close(&serial);
	CHECK(tcgetattr(STDIN_FILENO, &terminal) == 0
	    && cfgetispeed(&terminal) == B38400
	    && cfgetospeed(&terminal) == B38400
	    && (terminal.c_cflag & CSIZE) == CS8
	    && (terminal.c_cflag & PARENB) == 0);

done:
	yt_platform_rmt_serial_close(&serial);
	if (replaced)
		(void)dup2(saved_stdin, STDIN_FILENO);
	if (saved_stdin >= 0)
		(void)close(saved_stdin);
	if (slave >= 0)
		(void)close(slave);
	if (master >= 0)
		(void)close(master);
}
#else
static void
test_platform_rmt_serial(void)
{
}
#endif

static void
test_sysop_f5(void)
{
	static const struct {
		enum yt_sysop_f5_phase phase;
		uint16_t address;
	} expected[] = {
		{YT_SYSOP_F5_CHECKPOINT, 0xBA00U},
		{YT_SYSOP_F5_BASIC_END, 0xBA01U},
		{YT_SYSOP_F5_REGISTERED_CLEANUP, 0x3B2FU},
		{YT_SYSOP_F5_CALLBACK_CLEANUP, 0x3B29U},
		{YT_SYSOP_F5_RUNTIME_CLEANUP, 0x1810U},
		{YT_SYSOP_F5_COMMON_CLEANUP, 0x1C21U},
		{YT_SYSOP_F5_DOS_EXIT, 0x064EU},
	};
	struct yt_sysop_f5_result result;
	size_t index;

	CHECK(yt_sysop_f5_compose(false, &result));
	CHECK(result.event_count == YT_ARRAY_LEN(expected)
	    && !result.same_f5_pending && result.local_end_cleanup
	    && !result.event_returned && result.terminated
	    && result.exit_status == 0);
	for (index = 0U; index < YT_ARRAY_LEN(expected); ++index)
		CHECK(result.events[index].phase == expected[index].phase
		    && result.events[index].address == expected[index].address);
	CHECK(yt_sysop_f5_compose(true, &result)
	    && result.same_f5_pending && result.terminated
	    && !result.event_returned && result.exit_status == 0);
	CHECK(!yt_sysop_f5_compose(false, NULL));
}

static void
test_startup_dorinfo_parser(void)
{
	static const uint8_t first[] = "A\0B\r\n";
	static const uint8_t second[] = "C\nD\r";
	static const uint8_t third[] = "E\r\n";
	uint8_t raw[2048];
	uint8_t storage[2048];
	struct yt_startup_dorinfo_result parsed;
	const uint8_t *value;
	size_t position = 0U;
	size_t length;
	size_t field;

	memcpy(raw + position, first, sizeof(first) - 1U);
	position += sizeof(first) - 1U;
	memcpy(raw + position, second, sizeof(second) - 1U);
	position += sizeof(second) - 1U;
	memcpy(raw + position, third, sizeof(third) - 1U);
	position += sizeof(third) - 1U;
	for (field = 3U; field < YT_STARTUP_DORINFO_FIELDS; ++field) {
		size_t count = field == 6U ? 256U : field;

		memset(raw + position, (int)('A' + field), count);
		position += count;
		raw[position++] = '\r';
		raw[position++] = '\n';
	}
	memcpy(raw + position, "IGNORED-13\r\n", 12U);
	position += 12U;
	CHECK(yt_startup_parse_dorinfo(raw, position, storage,
	    sizeof(storage), &parsed));
	CHECK(parsed.outcome == YT_STARTUP_DORINFO_SUCCESS
	    && parsed.fields_assigned == YT_STARTUP_DORINFO_FIELDS
	    && parsed.failed_field == 0U && parsed.error_number == 0
	    && parsed.cursor == position - 12U);
	value = yt_startup_dorinfo_field(&parsed, storage, 0U, &length);
	CHECK(value != NULL && length == 2U && memcmp(value, "AB", 2U) == 0);
	value = yt_startup_dorinfo_field(&parsed, storage, 1U, &length);
	CHECK(value != NULL && length == 3U
	    && memcmp(value, "C\nD", 3U) == 0);
	value = yt_startup_dorinfo_field(&parsed, storage, 2U, &length);
	CHECK(value != NULL && length == 1U && value[0] == 'E');
	value = yt_startup_dorinfo_field(&parsed, storage, 6U, &length);
	CHECK(value != NULL && length == 256U && value[0] == 'G'
	    && value[255] == 'G');
	CHECK(yt_startup_dorinfo_field(&parsed, storage, 12U, &length)
	    == NULL);

	for (field = 1U; field <= YT_STARTUP_DORINFO_FIELDS; ++field) {
		position = 0U;
		while (position / 3U < field - 1U) {
			raw[position++] = 'X';
			raw[position++] = '\r';
			raw[position++] = '\n';
		}
		CHECK(yt_startup_parse_dorinfo(raw, position, storage,
		    sizeof(storage), &parsed));
		CHECK(parsed.outcome == YT_STARTUP_DORINFO_INPUT_PAST_END
		    && parsed.fields_assigned == field - 1U
		    && parsed.failed_field == field && parsed.error_number == 62
		    && parsed.cursor == position);
	}

	memcpy(raw, "\0\0\x1aTAIL", 7U);
	CHECK(yt_startup_parse_dorinfo(raw, 7U, storage, sizeof(storage),
	    &parsed));
	CHECK(parsed.outcome == YT_STARTUP_DORINFO_INPUT_PAST_END
	    && parsed.fields_assigned == 1U && parsed.failed_field == 2U
	    && parsed.cursor == 2U && parsed.fields[0].length == 0U);
	CHECK(!yt_startup_parse_dorinfo((const uint8_t *)"TOO-LONG\r", 9U,
	    storage, 2U, &parsed));
	CHECK(!yt_startup_parse_dorinfo(NULL, 1U, storage, sizeof(storage),
	    &parsed));
	CHECK(!yt_startup_parse_dorinfo(NULL, 0U, storage, sizeof(storage),
	    NULL));
}

static void
test_startup_dorinfo_state(void)
{
	static const uint8_t remote_raw[] =
	    "Example BBS\r\nThe\r\nSysop\r\nCOM3:\r\n"
	    "57600 baud,o,7,{\xe1\r\n0\r\n  jANE\r\n  o'NEIL  \r\n"
	    "Detroit\r\n&H1\r\n100\r\n60\r\nIGNORED\r\n";
	static const uint8_t local_raw[] =
	    "BBS\r\nAlan\r\nDavenport\r\nCOM0\r\n0 BAUD,N,8,1\r\n"
	    "0\r\nJane\r\nDoe\r\nCity\r\n0\r\n100\r\n180\r\n";
	uint8_t storage[1024];
	uint8_t canonical[128];
	struct yt_startup_dorinfo_result dorinfo;
	struct yt_startup_state_result state_result;
	struct yt_startup_event_result event_result;
	const uint8_t *field;
	size_t field_length;
	size_t index;

	CHECK(yt_startup_parse_dorinfo(remote_raw, sizeof(remote_raw) - 1U,
	    storage, sizeof(storage), &dorinfo));
	CHECK(yt_startup_compose_state(&dorinfo, storage, 2U, 0U, 100.9f,
	    100.9f, canonical, sizeof(canonical), &state_result));
	CHECK(state_result.outcome == YT_STARTUP_STATE_REMOTE_READY
	    && state_result.requested_port == 3
	    && state_result.detected_baud == 57600.0f
	    && state_result.open_spec_length
	    == strlen("COM1:1200,E,7,1,CS65535,DS,CD")
	    && memcmp(state_result.open_spec,
	    "COM1:1200,E,7,1,CS65535,DS,CD",
	    state_result.open_spec_length) == 0
	    && state_result.canonical_name_length == strlen("Jane O'neil")
	    && memcmp(canonical, "Jane O'neil",
	    state_result.canonical_name_length) == 0
	    && state_result.ansi_flag == 1.0f
	    && state_result.deadline_set && state_result.deadline == 3697.0f
	    && state_result.local_mode == 0.0f
	    && state_result.carrier_local_screen == -1.0f
	    && state_result.local_sound == 0.0f
	    && state_result.game_sound == -1.0f
	    && state_result.sampled_dll == 2U
	    && state_result.sampled_dlm == 0U
	    && state_result.restored_dll == 2U
	    && state_result.restored_dlm == 0U);
	field = yt_startup_dorinfo_field(&dorinfo, storage, 4U,
	    &field_length);
	CHECK(field != NULL
	    && field_length == sizeof("57600 BAUD,O,7,[\xc1") - 1U
	    && memcmp(field, "57600 BAUD,O,7,[\xc1", field_length) == 0);
	for (index = 0U; index < YT_STARTUP_DORINFO_FIELDS; ++index)
		CHECK(state_result.cleared_fields[index]
		    == (index == 3U || index == 4U || index == 8U
		    || index == 9U || index == 10U));
	CHECK(yt_startup_compose_events(&state_result, 0x1bU, 0x05U, 0,
	    YT_STARTUP_WAIT_SERIAL_KEY, 0x80U, 0x03U, 0x01U,
	    &event_result));
	CHECK(event_result.outcome == YT_STARTUP_EVENTS_REMOTE_READY
	    && event_result.event_count == 26U
	    && event_result.wait_exit == YT_STARTUP_WAIT_SERIAL_KEY
	    && event_result.carrier_checked && event_result.carrier_detected);
	CHECK(event_result.events[0].operation == YT_STARTUP_EVENT_DEF_SEG);
	CHECK(event_result.events[1].operation == YT_STARTUP_EVENT_POKE
	    && event_result.events[1].address == 0x400U
	    && event_result.events[1].value == 0xe8U);
	CHECK(event_result.events[2].operation == YT_STARTUP_EVENT_POKE
	    && event_result.events[2].address == 0x401U
	    && event_result.events[2].value == 0x03U);
	CHECK(event_result.events[3].operation == YT_STARTUP_EVENT_IN
	    && event_result.events[3].address == 0x3ebU
	    && event_result.events[3].value == 0x1bU);
	CHECK(event_result.events[4].operation == YT_STARTUP_EVENT_OUT
	    && event_result.events[4].address == 0x3ebU
	    && event_result.events[4].value == 0x1bU);
	CHECK(event_result.events[5].operation == YT_STARTUP_EVENT_IN
	    && event_result.events[5].address == 0x3e9U
	    && event_result.events[5].value == 0x05U);
	CHECK(event_result.events[7].operation == YT_STARTUP_EVENT_OUT
	    && event_result.events[7].address == 0x3ebU
	    && event_result.events[7].value == 0x9bU);
	CHECK(event_result.events[8].operation == YT_STARTUP_EVENT_IN
	    && event_result.events[8].address == 0x3e9U
	    && event_result.events[8].value == 0U);
	CHECK(event_result.events[9].operation == YT_STARTUP_EVENT_IN
	    && event_result.events[9].address == 0x3e8U
	    && event_result.events[9].value == 2U);
	CHECK(event_result.events[13].operation == YT_STARTUP_EVENT_UPPERCASE
	    && event_result.events[13].address == 0x5690U);
	CHECK(event_result.events[14].operation == YT_STARTUP_EVENT_OPEN
	    && event_result.events[14].address == 3U
	    && event_result.events[14].complete);
	CHECK(event_result.events[15].operation
	    == YT_STARTUP_EVENT_SET_LOCAL_SOUND
	    && event_result.events[15].address == 0x4b70U);
	CHECK(event_result.events[16].operation == YT_STARTUP_EVENT_WAIT
	    && event_result.events[16].value == YT_STARTUP_WAIT_SERIAL_KEY);
	CHECK(event_result.events[17].operation
	    == YT_STARTUP_EVENT_CARRIER_CHECK
	    && event_result.events[17].address == 0x3eeU
	    && event_result.events[17].value == 0x80U);
	CHECK(event_result.events[18].operation == YT_STARTUP_EVENT_IN
	    && event_result.events[18].address == 0x3ebU
	    && event_result.events[18].value == 0x03U);
	CHECK(event_result.events[21].operation == YT_STARTUP_EVENT_OUT
	    && event_result.events[21].address == 0x3ebU
	    && event_result.events[21].value == 0x83U);
	CHECK(event_result.events[22].operation == YT_STARTUP_EVENT_OUT
	    && event_result.events[22].address == 0x3e8U
	    && event_result.events[22].value == 2U);
	CHECK(event_result.events[25].operation == YT_STARTUP_EVENT_OUT
	    && event_result.events[25].address == 0x3e9U
	    && event_result.events[25].value == 0x01U);

	CHECK(yt_startup_compose_events(&state_result, 0x1bU, 0x05U, 64,
	    YT_STARTUP_WAIT_TIMER, 0x80U, 0x03U, 0x01U, &event_result));
	CHECK(event_result.outcome == YT_STARTUP_EVENTS_OPEN_ERROR
	    && event_result.event_count == 15U
	    && event_result.events[13].operation == YT_STARTUP_EVENT_UPPERCASE
	    && event_result.events[14].operation == YT_STARTUP_EVENT_OPEN
	    && !event_result.events[14].complete
	    && event_result.events[14].error_number == 64
	    && !event_result.carrier_checked);
	CHECK(yt_startup_compose_events(&state_result, 0x1bU, 0x05U, 0,
	    YT_STARTUP_WAIT_LOCAL_KEY, 0x00U, 0x03U, 0x01U, &event_result));
	CHECK(event_result.outcome == YT_STARTUP_EVENTS_CARRIER_DROP
	    && event_result.event_count == 21U
	    && event_result.carrier_checked && !event_result.carrier_detected
	    && event_result.events[17].operation
	    == YT_STARTUP_EVENT_CARRIER_CHECK
	    && event_result.events[18].operation
	    == YT_STARTUP_EVENT_CARRIER_NOTICE
	    && event_result.events[19].operation == YT_STARTUP_EVENT_CLOSE
	    && event_result.events[20].operation
	    == YT_STARTUP_EVENT_PROCESS_END);
	state_result.carrier_local_screen = 0.0f;
	CHECK(yt_startup_compose_events(&state_result, 0x1bU, 0x05U, 0,
	    YT_STARTUP_WAIT_LOCAL_KEY, 0x00U, 0x03U, 0x01U, &event_result));
	CHECK(event_result.outcome == YT_STARTUP_EVENTS_CARRIER_DROP
	    && event_result.event_count == 20U
	    && event_result.events[17].operation
	    == YT_STARTUP_EVENT_CARRIER_CHECK
	    && event_result.events[18].operation == YT_STARTUP_EVENT_CLOSE
	    && event_result.events[19].operation
	    == YT_STARTUP_EVENT_PROCESS_END);

	CHECK(yt_startup_parse_dorinfo(remote_raw, sizeof(remote_raw) - 1U,
	    storage, sizeof(storage), &dorinfo));
	CHECK(yt_startup_compose_state(&dorinfo, storage, 0U, 0U, 100.9f,
	    100.9f, canonical, sizeof(canonical), &state_result));
	CHECK(state_result.outcome == YT_STARTUP_STATE_ZERO_DIVISOR
	    && state_result.requested_port == 3
	    && state_result.canonical_name_length == strlen("Jane O'neil")
	    && !state_result.deadline_set && state_result.open_spec_length == 0U
	    && state_result.carrier_local_screen == 0.0f
	    && state_result.game_sound == 0.0f);
	field = yt_startup_dorinfo_field(&dorinfo, storage, 4U,
	    &field_length);
	CHECK(field != NULL
	    && field_length == sizeof("57600 baud,o,7,{\xe1") - 1U
	    && memcmp(field, "57600 baud,o,7,{\xe1", field_length) == 0);
	CHECK(yt_startup_compose_events(&state_result, 0x03U, 0x00U, 0,
	    YT_STARTUP_WAIT_TIMER, 0x80U, 0x03U, 0x00U, &event_result));
	CHECK(event_result.outcome == YT_STARTUP_EVENTS_ZERO_DIVISOR
	    && event_result.event_count == 11U
	    && event_result.events[10].operation
	    == YT_STARTUP_EVENT_RUNTIME_ERROR
	    && event_result.events[10].error_number == 11
	    && !event_result.events[10].complete);

	CHECK(yt_startup_parse_dorinfo(local_raw, sizeof(local_raw) - 1U,
	    storage, sizeof(storage), &dorinfo));
	CHECK(yt_startup_compose_state(&dorinfo, storage, 0U, 0U, 86399.9f,
	    0.1f, canonical, sizeof(canonical), &state_result));
	CHECK(state_result.outcome == YT_STARTUP_STATE_LOCAL_READY
	    && state_result.requested_port == 0
	    && state_result.ansi_flag == 0.0f
	    && state_result.deadline == 10800.0f
	    && state_result.local_mode == 1.0f
	    && state_result.local_sound == -1.0f
	    && state_result.game_sound == -1.0f);
	CHECK(yt_startup_compose_events(&state_result, 0x03U, 0x00U, 0,
	    YT_STARTUP_WAIT_TIMER, 0x80U, 0x03U, 0x00U, &event_result));
	CHECK(event_result.outcome == YT_STARTUP_EVENTS_LOCAL_READY
	    && event_result.event_count == 0U && !event_result.carrier_checked);
	CHECK(!yt_startup_compose_state(NULL, storage, 0U, 0U, 0.0f, 0.0f,
	    canonical, sizeof(canonical), &state_result));
	CHECK(!yt_startup_compose_events(NULL, 0U, 0U, 0,
	    YT_STARTUP_WAIT_TIMER, 0U, 0U, 0U, &event_result));
}

enum lockout_event {
	LOCKOUT_OPEN_RANDOM,
	LOCKOUT_EMPTY,
	LOCKOUT_CLOSE,
	LOCKOUT_OPEN_INPUT,
	LOCKOUT_READ,
	LOCKOUT_PRESENT_BLANK,
	LOCKOUT_PRESENT_REVOKED,
	LOCKOUT_PRESENT_CONTACT,
	LOCKOUT_WAIT,
	LOCKOUT_CLOSE_ALL,
	LOCKOUT_END,
};

struct lockout_tape {
	const uint8_t *file;
	size_t file_length;
	size_t cursor;
	uint8_t line[128];
	enum lockout_event events[20];
	size_t event_count;
	size_t fail_at;
	bool open;
	char random_path[32];
	char input_path[32];
	uint8_t presented[3][96];
	size_t presented_length[3];
	float wait_seconds;
	bool ended;
};

static const uint8_t lockout_identity[] = "John Doe";
static const uint8_t lockout_contact[] =
    "Please contact your sysop Jane Sysop.";
static const uint8_t lockout_revoked[] =
    "\aYOUR ACCESS TO THIS GAME HAS BEEN REVOKED!\a";

static bool
lockout_tape_record(struct lockout_tape *tape, enum lockout_event event,
    struct yt_error *error)
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
		    "injected lockout failure");
	}
	return false;
}

static bool
lockout_tape_open_random(void *context, const char *path,
    struct yt_error *error)
{
	struct lockout_tape *tape = context;

	if (!lockout_tape_record(tape, LOCKOUT_OPEN_RANDOM, error))
		return false;
	(void)snprintf(tape->random_path, sizeof(tape->random_path), "%s",
	    path);
	tape->open = true;
	return true;
}

static bool
lockout_tape_empty(void *context, bool *empty, struct yt_error *error)
{
	struct lockout_tape *tape = context;

	if (!lockout_tape_record(tape, LOCKOUT_EMPTY, error))
		return false;
	*empty = tape->file_length == 0U;
	return true;
}

static bool
lockout_tape_close(void *context, struct yt_error *error)
{
	struct lockout_tape *tape = context;

	if (!lockout_tape_record(tape, LOCKOUT_CLOSE, error))
		return false;
	tape->open = false;
	return true;
}

static bool
lockout_tape_open_input(void *context, const char *path,
    struct yt_error *error)
{
	struct lockout_tape *tape = context;

	if (!lockout_tape_record(tape, LOCKOUT_OPEN_INPUT, error))
		return false;
	(void)snprintf(tape->input_path, sizeof(tape->input_path), "%s", path);
	tape->cursor = 0U;
	tape->open = true;
	return true;
}

static bool
lockout_tape_read(void *context, const uint8_t **line, size_t *length,
    bool *available, struct yt_error *error)
{
	struct lockout_tape *tape = context;

	if (!lockout_tape_record(tape, LOCKOUT_READ, error))
		return false;
	if (!yt_text_line_input_next(tape->file, tape->file_length,
	    &tape->cursor, tape->line, sizeof(tape->line), length, available))
		return false;
	*line = tape->line;
	return true;
}

static bool
lockout_tape_present(void *context, enum yt_startup_lockout_row row,
    const uint8_t *text, size_t length, struct yt_error *error)
{
	struct lockout_tape *tape = context;
	enum lockout_event event = row == YT_STARTUP_LOCKOUT_BLANK
	    ? LOCKOUT_PRESENT_BLANK : row == YT_STARTUP_LOCKOUT_REVOKED
	    ? LOCKOUT_PRESENT_REVOKED : LOCKOUT_PRESENT_CONTACT;

	if (!lockout_tape_record(tape, event, error))
		return false;
	CHECK((size_t)row < YT_ARRAY_LEN(tape->presented)
	    && length <= sizeof(tape->presented[0]));
	if ((size_t)row >= YT_ARRAY_LEN(tape->presented)
	    || length > sizeof(tape->presented[0]))
		return false;
	if (length != 0U)
		memcpy(tape->presented[row], text, length);
	tape->presented_length[row] = length;
	return true;
}

static bool
lockout_tape_wait(void *context, float seconds, struct yt_error *error)
{
	struct lockout_tape *tape = context;

	if (!lockout_tape_record(tape, LOCKOUT_WAIT, error))
		return false;
	tape->wait_seconds = seconds;
	return true;
}

static bool
lockout_tape_close_all(void *context, struct yt_error *error)
{
	struct lockout_tape *tape = context;

	if (!lockout_tape_record(tape, LOCKOUT_CLOSE_ALL, error))
		return false;
	tape->open = false;
	return true;
}

static void
lockout_tape_end(void *context)
{
	struct lockout_tape *tape = context;

	(void)lockout_tape_record(tape, LOCKOUT_END, NULL);
	tape->ended = true;
}

static const struct yt_startup_lockout_ops lockout_ops = {
	lockout_tape_open_random,
	lockout_tape_empty,
	lockout_tape_close,
	lockout_tape_open_input,
	lockout_tape_read,
	lockout_tape_present,
	lockout_tape_wait,
	lockout_tape_close_all,
	lockout_tape_end,
};

static void
lockout_state_init(struct yt_startup_lockout_state *state)
{
	memset(state, 0, sizeof(*state));
	state->random_path = "lockout.dat";
	state->input_path = "Lockout.dat";
	state->identity = lockout_identity;
	state->identity_length = sizeof(lockout_identity) - 1U;
	state->contact = lockout_contact;
	state->contact_length = sizeof(lockout_contact) - 1U;
}

static void
test_startup_lockout_transaction(void)
{
	static const uint8_t matching[] =
	    "Other User\r\n  jOhN   dOE \r\nIgnored User\r\n";
	static const uint8_t nonmatching[] =
	    "Other User\r\nStill Else\r\n";
	static const enum lockout_event denied_events[] = {
		LOCKOUT_OPEN_RANDOM,
		LOCKOUT_EMPTY,
		LOCKOUT_CLOSE,
		LOCKOUT_OPEN_INPUT,
		LOCKOUT_READ,
		LOCKOUT_READ,
		LOCKOUT_PRESENT_BLANK,
		LOCKOUT_PRESENT_REVOKED,
		LOCKOUT_PRESENT_CONTACT,
		LOCKOUT_WAIT,
		LOCKOUT_CLOSE_ALL,
		LOCKOUT_END,
	};
	static const enum lockout_event nonmatch_events[] = {
		LOCKOUT_OPEN_RANDOM,
		LOCKOUT_EMPTY,
		LOCKOUT_CLOSE,
		LOCKOUT_OPEN_INPUT,
		LOCKOUT_READ,
		LOCKOUT_READ,
		LOCKOUT_READ,
		LOCKOUT_CLOSE,
	};
	static const enum lockout_event empty_events[] = {
		LOCKOUT_OPEN_RANDOM,
		LOCKOUT_EMPTY,
		LOCKOUT_CLOSE,
	};
	struct yt_startup_lockout_state state;
	struct lockout_tape tape;
	struct yt_error error;
	size_t failure;

	lockout_state_init(&state);
	memset(&tape, 0, sizeof(tape));
	tape.file = matching;
	tape.file_length = sizeof(matching) - 1U;
	yt_error_clear(&error);
	CHECK(yt_startup_lockout_run(&state, &lockout_ops, &tape, &error));
	CHECK(tape.event_count == YT_ARRAY_LEN(denied_events)
	    && memcmp(tape.events, denied_events, sizeof(denied_events)) == 0
	    && strcmp(tape.random_path, "lockout.dat") == 0
	    && strcmp(tape.input_path, "Lockout.dat") == 0
	    && !tape.open && tape.ended && tape.wait_seconds == 10.0f);
	CHECK(state.denied && state.terminated && !state.file_open
	    && state.lines_read == 2U);
	CHECK(tape.presented_length[YT_STARTUP_LOCKOUT_BLANK] == 0U
	    && tape.presented_length[YT_STARTUP_LOCKOUT_REVOKED]
	    == sizeof(lockout_revoked) - 1U
	    && memcmp(tape.presented[YT_STARTUP_LOCKOUT_REVOKED],
	    lockout_revoked, sizeof(lockout_revoked) - 1U) == 0
	    && tape.presented_length[YT_STARTUP_LOCKOUT_CONTACT]
	    == sizeof(lockout_contact) - 1U
	    && memcmp(tape.presented[YT_STARTUP_LOCKOUT_CONTACT],
	    lockout_contact, sizeof(lockout_contact) - 1U) == 0);

	for (failure = 1U; failure < YT_ARRAY_LEN(denied_events); ++failure) {
		lockout_state_init(&state);
		memset(&tape, 0, sizeof(tape));
		tape.file = matching;
		tape.file_length = sizeof(matching) - 1U;
		tape.fail_at = failure;
		yt_error_clear(&error);
		CHECK(!yt_startup_lockout_run(&state, &lockout_ops, &tape,
		    &error));
		CHECK(error.status == YT_IO_ERROR && tape.event_count == failure
		    && memcmp(tape.events, denied_events,
		    failure * sizeof(denied_events[0])) == 0
		    && !state.terminated && !tape.ended);
		CHECK(state.lines_read == (failure <= 5U ? 0U
		    : failure == 6U ? 1U : 2U));
		CHECK(state.denied == (failure >= 7U));
		CHECK(state.file_open == (failure == 2U || failure == 3U
		    || failure >= 5U));
	}

	lockout_state_init(&state);
	memset(&tape, 0, sizeof(tape));
	tape.file = nonmatching;
	tape.file_length = sizeof(nonmatching) - 1U;
	CHECK(yt_startup_lockout_run(&state, &lockout_ops, &tape, NULL));
	CHECK(tape.event_count == YT_ARRAY_LEN(nonmatch_events)
	    && memcmp(tape.events, nonmatch_events, sizeof(nonmatch_events)) == 0
	    && !state.denied && !state.terminated && !state.file_open
	    && state.lines_read == 2U && !tape.open && !tape.ended);
	for (failure = 1U; failure <= YT_ARRAY_LEN(nonmatch_events);
	    ++failure) {
		lockout_state_init(&state);
		memset(&tape, 0, sizeof(tape));
		tape.file = nonmatching;
		tape.file_length = sizeof(nonmatching) - 1U;
		tape.fail_at = failure;
		yt_error_clear(&error);
		CHECK(!yt_startup_lockout_run(&state, &lockout_ops, &tape,
		    &error));
		CHECK(error.status == YT_IO_ERROR && tape.event_count == failure
		    && memcmp(tape.events, nonmatch_events,
		    failure * sizeof(nonmatch_events[0])) == 0
		    && !state.denied && !state.terminated && !tape.ended);
		CHECK(state.lines_read == (failure <= 5U ? 0U
		    : failure == 6U ? 1U : 2U));
		CHECK(state.file_open == (failure == 2U || failure == 3U
		    || failure >= 5U));
	}

	lockout_state_init(&state);
	memset(&tape, 0, sizeof(tape));
	CHECK(yt_startup_lockout_run(&state, &lockout_ops, &tape, NULL));
	CHECK(tape.event_count == YT_ARRAY_LEN(empty_events)
	    && memcmp(tape.events, empty_events, sizeof(empty_events)) == 0
	    && !state.denied && !state.terminated && !state.file_open
	    && state.lines_read == 0U && !tape.open && !tape.ended);
	for (failure = 1U; failure <= YT_ARRAY_LEN(empty_events); ++failure) {
		lockout_state_init(&state);
		memset(&tape, 0, sizeof(tape));
		tape.fail_at = failure;
		yt_error_clear(&error);
		CHECK(!yt_startup_lockout_run(&state, &lockout_ops, &tape,
		    &error));
		CHECK(error.status == YT_IO_ERROR && tape.event_count == failure
		    && memcmp(tape.events, empty_events,
		    failure * sizeof(empty_events[0])) == 0
		    && !state.denied && !state.terminated && !tape.ended
		    && state.lines_read == 0U);
		CHECK(state.file_open == (failure >= 2U));
	}
	CHECK(!yt_startup_lockout_run(NULL, &lockout_ops, &tape, NULL));
}

static void
test_startup_lockout_scan(void)
{
	static const uint8_t identity[] = "John Doe";
	static const uint8_t matching[] = {
		'\r', '\n',
		'P', 'R', 'O', 'B', 'L', 'E', 'M', ' ', 'U', 'S', 'E', 'R',
		'\r', '\n',
		' ', 'j', 'o', 'h', 'n', 0, ' ', ' ', ' ', 'D', 'O', 'E', ' ',
		'\r', '\n',
		'X', 0x1a, 'J', 'o', 'h', 'n', ' ', 'D', 'o', 'e',
	};
	static const uint8_t bare_lf[] = {
		'J', 'o', 'h', 'n', '\n', 'D', 'o', 'e', '\r',
	};
	static const uint8_t eof_before_match[] = {
		'P', 'r', 'o', 'b', 'l', 'e', 'm', '\r', '\n', 0x1a,
		'J', 'o', 'h', 'n', ' ', 'D', 'o', 'e', '\r',
	};
	size_t lines;
	bool matched;

	CHECK(yt_startup_lockout_scan(matching, sizeof(matching), identity,
	    sizeof(identity) - 1U, &matched, &lines));
	CHECK(matched && lines == 3U);
	CHECK(yt_startup_lockout_scan(bare_lf, sizeof(bare_lf), identity,
	    sizeof(identity) - 1U, &matched, &lines));
	CHECK(!matched && lines == 1U);
	CHECK(yt_startup_lockout_scan(eof_before_match,
	    sizeof(eof_before_match), identity, sizeof(identity) - 1U,
	    &matched, &lines));
	CHECK(!matched && lines == 1U);
	CHECK(yt_startup_lockout_scan(NULL, 0U, identity,
	    sizeof(identity) - 1U, &matched, &lines));
	CHECK(!matched && lines == 0U);
	CHECK(!yt_startup_lockout_scan(NULL, 1U, identity,
	    sizeof(identity) - 1U, &matched, &lines));
}

static void
test_registration_transaction(void)
{
	static const uint8_t shipped[] =
	    "This BBS\r\nThe Sysop\r\n3484625551\r\nignored\r\n";
	static const uint8_t odd_bytes[] = {
		' ', 'a', 0, 'B', '\'', 'C', '-', 0x80, ' ', '\r', '\n',
		'x', '\n', 'Y', '\r', '\n',
		'0', '\r', '\n',
	};
	static const uint8_t expected_key[8] =
	    {0x00, 0x00, 0x00, 0x8f, 0x2a, 0xb3, 0x4f, 0xa0};
	static const uint8_t expected_first_sum[8] =
	    {0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x08, 0x8c};
	static const uint8_t expected_first_product[8] =
	    {0x00, 0x6a, 0xb3, 0x86, 0xb9, 0x94, 0x5c, 0xb0};
	static const uint8_t expected_first_root[8] =
	    {0x00, 0x00, 0x00, 0x00, 0xa9, 0xa1, 0x6d, 0x98};
	static const uint8_t expected_second_sum[8] =
	    {0x00, 0x00, 0x00, 0x00, 0x84, 0xb2, 0x6d, 0x98};
	static const uint8_t expected_final_product[8] =
	    {0x7d, 0x5d, 0xea, 0x37, 0x3c, 0x83, 0x28, 0xc0};
	static const enum registration_event registered_events[] = {
		REG_CLOSE, REG_RANDOM_OPEN, REG_SIZE, REG_CLOSE,
		REG_SEQUENTIAL_OPEN, REG_READ, REG_READ, REG_READ, REG_CLOSE,
	};
	static const enum registration_event invalid_tail[] = {
		REG_BEEP, REG_BEEP, REG_FORCED_LOCAL, REG_BEEP, REG_BEEP,
		REG_CLOSE_ALL, REG_END,
	};
	struct yt_registration_state state;
	struct registration_tape tape;
	struct yt_error error;
	uint8_t storage[5][256];
	size_t index;

	registration_state_init(&state, storage);
	memset(&tape, 0, sizeof(tape));
	tape.file = shipped;
	tape.file_length = sizeof(shipped) - 1U;
	yt_error_clear(&error);
	CHECK(yt_registration_run(&state, &registration_ops, &tape, &error));
	CHECK(state.outcome == YT_REGISTRATION_REGISTERED && state.nonempty
	    && state.registered && !state.ended && !state.closed_all);
	CHECK(state.line[0].length == 8U
	    && memcmp(state.line[0].data, "This Bbs", 8U) == 0);
	CHECK(state.line[1].length == 9U
	    && memcmp(state.line[1].data, "The Sysop", 9U) == 0);
	CHECK(state.display[0].length == 22U
	    && memcmp(state.display[0].data, "Registered to This Bbs", 22U) == 0);
	CHECK(state.display[1].length == 23U
	    && memcmp(state.display[1].data, "Registered by The Sysop", 23U) == 0);
	CHECK(memcmp(state.calculated_key, expected_key, 8U) == 0
	    && memcmp(state.parsed_key, expected_key, 8U) == 0);
	CHECK(memcmp(state.first_sum, expected_first_sum, 8U) == 0
	    && memcmp(state.first_product, expected_first_product, 8U) == 0
	    && memcmp(state.first_root, expected_first_root, 8U) == 0
	    && memcmp(state.second_sum, expected_second_sum, 8U) == 0
	    && memcmp(state.final_product, expected_final_product, 8U) == 0);
	CHECK(tape.event_count == YT_ARRAY_LEN(registered_events)
	    && memcmp(tape.event, registered_events,
	    sizeof(registered_events)) == 0);

	/* Every fallible file/read boundary retains its accepted prefix. */
	for (index = 1U; index <= YT_ARRAY_LEN(registered_events); ++index) {
		registration_state_init(&state, storage);
		memset(&tape, 0, sizeof(tape));
		tape.file = shipped;
		tape.file_length = sizeof(shipped) - 1U;
		tape.fail_at = index;
		yt_error_clear(&error);
		CHECK(!yt_registration_run(&state, &registration_ops, &tape,
		    &error));
		CHECK(tape.event_count == index && error.status == YT_IO_ERROR);
		CHECK(state.outcome == YT_REGISTRATION_IN_PROGRESS
		    && !state.registered);
		CHECK(memcmp(tape.event, registered_events,
		    index * sizeof(registered_events[0])) == 0);
		if (index <= 6U)
			CHECK(state.line[0].length == 0U);
		else
			CHECK(state.line[0].length == 8U
			    && memcmp(state.line[0].data, "This BBS", 8U) == 0);
		if (index <= 7U)
			CHECK(state.line[1].length == 0U);
		else
			CHECK(state.line[1].length == 9U
			    && memcmp(state.line[1].data, "The Sysop", 9U) == 0);
		CHECK(state.line[2].length == (index == 9U ? 10U : 0U));
	}

	/* Empty input is deleted before ordinary evaluation construction. */
	registration_state_init(&state, storage);
	memset(&tape, 0, sizeof(tape));
	CHECK(yt_registration_run(&state, &registration_ops, &tape, NULL));
	CHECK(state.outcome == YT_REGISTRATION_EVALUATION && !state.nonempty
	    && !state.registered && state.evaluation_sum[0] == 2085U
	    && state.evaluation_sum[1] == 3496U
	    && state.evaluation_counter[0] == 30.0f
	    && state.evaluation_counter[1] == 51.0f);
	CHECK(state.display[0].length == 29U
	    && state.display[1].length == 50U
	    && tape.event_count == 5U && tape.event[4] == REG_DELETE);
	for (index = 1U; index <= 5U; ++index) {
		registration_state_init(&state, storage);
		memset(&tape, 0, sizeof(tape));
		tape.fail_at = index;
		CHECK(!yt_registration_run(&state, &registration_ops, &tape,
		    NULL));
		CHECK(tape.event_count == index);
		CHECK(state.nonempty == (index < 5U));
	}

	/* The beta terminal has no explicit CLOSE ALL. */
	registration_state_init(&state, storage);
	state.beta_only = true;
	memset(&tape, 0, sizeof(tape));
	CHECK(yt_registration_run(&state, &registration_ops, &tape, NULL));
	CHECK(state.outcome == YT_REGISTRATION_BETA_END && state.ended
	    && !state.closed_all && tape.event_count == 9U
	    && tape.event[5] == REG_CENTERED && tape.event[6] == REG_BEEP
	    && tape.event[7] == REG_BEEP && tape.event[8] == REG_END);
	CHECK(tape.presented_length == 48U
	    && memcmp(tape.presented,
	    "ONLY REGISTERED SYSOPS CAN RUN BETA TEST COPIES!", 48U) == 0);
	for (index = 6U; index <= 8U; ++index) {
		registration_state_init(&state, storage);
		state.beta_only = true;
		memset(&tape, 0, sizeof(tape));
		tape.fail_at = index;
		CHECK(!yt_registration_run(&state, &registration_ops, &tape,
		    NULL));
		CHECK(tape.event_count == index && !state.ended);
	}

	/* Either immutable checksum mismatch closes all and does not END. */
	registration_state_init(&state, storage);
	state.expected_evaluation_sum[1] = 3495U;
	memset(&tape, 0, sizeof(tape));
	CHECK(yt_registration_run(&state, &registration_ops, &tape, NULL));
	CHECK(state.outcome == YT_REGISTRATION_ANTI_TAMPER_BUSY_LOOP
	    && state.closed_all && state.busy_loop && !state.ended
	    && tape.event_count == 6U && tape.event[5] == REG_CLOSE_ALL);

	/* NUL is discarded, bare LF retained, and high bytes survive title case. */
	registration_state_init(&state, storage);
	memset(&tape, 0, sizeof(tape));
	tape.file = odd_bytes;
	tape.file_length = sizeof(odd_bytes);
	CHECK(yt_registration_run(&state, &registration_ops, &tape, NULL));
	CHECK(state.outcome == YT_REGISTRATION_INVALID_END);
	CHECK(state.line[0].length == 6U
	    && memcmp(state.line[0].data, "Ab'c-\x80", 6U) == 0);
	CHECK(state.line[1].length == 3U
	    && memcmp(state.line[1].data, "X\nY", 3U) == 0);
	CHECK(tape.event_count == 16U
	    && memcmp(tape.event + 9U, invalid_tail,
	    sizeof(invalid_tail)) == 0);
	CHECK(tape.presented_length == 30U
	    && memcmp(tape.presented, " * INVALID REGISTRATION KEY! *", 30U)
	    == 0);
	for (index = 10U; index <= 14U; ++index) {
		registration_state_init(&state, storage);
		memset(&tape, 0, sizeof(tape));
		tape.file = odd_bytes;
		tape.file_length = sizeof(odd_bytes);
		tape.fail_at = index;
		CHECK(!yt_registration_run(&state, &registration_ops, &tape,
		    NULL));
		CHECK(tape.event_count == index && !state.closed_all
		    && !state.ended);
	}

	/* VAL overflow faults after the file has closed and before arithmetic. */
	{
		static const uint8_t overflow[] = "A\rB\r1E99\r";

		registration_state_init(&state, storage);
		memset(&tape, 0, sizeof(tape));
		tape.file = overflow;
		tape.file_length = sizeof(overflow) - 1U;
		yt_error_clear(&error);
		CHECK(!yt_registration_run(&state, &registration_ops, &tape,
		    &error));
		CHECK(tape.event_count == 9U && error.status == YT_RANGE
		    && strcmp(error.operation, "registration key VAL") == 0);
	}
	{
		static const uint8_t short_file[] = "First\r\nSecond\r\n";

		registration_state_init(&state, storage);
		memset(&tape, 0, sizeof(tape));
		tape.file = short_file;
		tape.file_length = sizeof(short_file) - 1U;
		yt_error_clear(&error);
		CHECK(!yt_registration_run(&state, &registration_ops, &tape,
		    &error));
		CHECK(tape.event_count == 8U && error.status == YT_EOF
		    && state.line[0].length == 5U && state.line[1].length == 6U
		    && state.line[2].length == 0U);
	}

	CHECK(!yt_registration_run(NULL, &registration_ops, &tape, NULL));
	CHECK(!yt_registration_run(&state, NULL, &tape, NULL));
}

int
main(void)
{
	test_arbitration();
	test_ab36_inactivity_gate();
	test_ab36_queued_input();
	test_ab36_live_input();
	test_radio_body_live_input();
	test_radio_body_key_classification();
	test_b05d_live_input();
	test_ab36_repeat_recognition();
	test_ab36_repeat_transaction();
	test_ab36_submission();
	test_ab36_backspace_transaction();
	test_ab36_printable_transaction();
	test_command_save_gate();
	test_ab36_terminal_transaction();
	test_source_fifo();
	test_merged_fifo();
	test_b05d_keys();
	test_repeat_transform();
	test_semicolon_queue();
	test_queue_program_prepend();
	test_timed_wait();
	test_input_drain();
	test_yes_no_candidate();
	test_numeric_response();
	test_sysop_chat();
	test_main_startup_prefix();
	test_serial_startup_model();
	test_platform_rmt_serial();
	test_sysop_f5();
	test_startup_dorinfo_parser();
	test_startup_dorinfo_state();
	test_startup_lockout_transaction();
	test_startup_lockout_scan();
	test_registration_transaction();
	if (failures != 0) {
		fprintf(stderr, "%u input-model test(s) failed\n", failures);
		return 1;
	}
	puts("input-model tests passed");
	return 0;
}
