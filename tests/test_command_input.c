#include "yt_input.h"
#include "input_editor_test_model.h"
#include "qb.h"
#include "yt_pager.h"
#include "yt_platform.h"
#include "yt_startup.h"
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

static struct yt_input_value
one(uint8_t byte)
{
	struct yt_input_value value = {{byte, 0}, 1, false};

	return value;
}

static void
test_ab36_queued_input(void)
{
	char queue[8] = "AB";
	size_t position = 0U;
	size_t length = 2U;
	struct yt_input_value selected;

	CHECK(yt_input_queue_pop(queue, sizeof(queue), &position,
	    &length, &selected));
	CHECK(selected.length == 1U && selected.bytes[0] == 'A'
	    && !selected.remote && position == 1U && length == 2U
	    && memcmp(queue, "AB", 2U) == 0);
	CHECK(yt_input_queue_pop(queue, sizeof(queue), &position,
	    &length, &selected));
	CHECK(selected.length == 1U && selected.bytes[0] == 'B'
	    && position == 0U && length == 0U && queue[0] == '\0');
	CHECK(yt_input_queue_pop(queue, sizeof(queue), &position,
	    &length, &selected) && selected.length == 0U);
	position = 2U;
	length = 1U;
	CHECK(!yt_input_queue_pop(queue, sizeof(queue), &position,
	    &length, &selected));
	position = 0U;
	length = sizeof(queue);
	CHECK(!yt_input_queue_pop(queue, sizeof(queue), &position,
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
test_ab36_repeat_recognition(void)
{
	struct yt_input_value selected = one(0x12U);

	CHECK(yt_input_repeat_requested(false, &selected));
	CHECK(!yt_input_repeat_requested(true, &selected));
	selected.bytes[0] = 0U;
	selected.bytes[1] = 0x12U;
	selected.length = 2U;
	CHECK(!yt_input_repeat_requested(false, &selected));
	selected = one('R');
	CHECK(!yt_input_repeat_requested(false, &selected));
	selected.length = 0U;
	CHECK(!yt_input_repeat_requested(false, &selected));
	CHECK(!yt_input_repeat_requested(false, NULL));
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

	CHECK(yt_input_repeat_current_command(accumulator, sizeof(accumulator),
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
	CHECK(!yt_input_repeat_current_command(accumulator, sizeof(accumulator),
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

	CHECK(yt_input_submit_requested('\r'));
	CHECK(!yt_input_submit_requested(0x12U));
	CHECK(!yt_input_submit_requested('\b'));
	CHECK(!yt_input_submit_requested('A'));
	CHECK(yt_input_submit(&newline_flag, ab36_submit_line, &tape));
	CHECK(tape.calls == 1U && newline_flag == 0.0f);

	newline_flag = 1.0f;
	tape.calls = 0U;
	tape.fail = true;
	CHECK(!yt_input_submit(&newline_flag, ab36_submit_line,
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

	CHECK(yt_input_apply_backspace('\b', accumulator,
	    sizeof(accumulator), &handled, ab36_backspace_echo, &tape));
	CHECK(handled && tape.calls == 1U && strcmp(accumulator, "A") == 0);

	memcpy(accumulator, "AB", 3U);
	tape.calls = 0U;
	CHECK(yt_input_apply_backspace(0x7fU, accumulator,
	    sizeof(accumulator), &handled, ab36_backspace_echo, &tape));
	CHECK(!handled && tape.calls == 0U && strcmp(accumulator, "AB") == 0);

	accumulator[0] = '\0';
	CHECK(yt_input_apply_backspace('\b', accumulator,
	    sizeof(accumulator), &handled, ab36_backspace_echo, &tape));
	CHECK(!handled && tape.calls == 0U && accumulator[0] == '\0');

	memcpy(accumulator, "AB", 3U);
	tape.fail = true;
	CHECK(!yt_input_apply_backspace('\b', accumulator,
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

	CHECK(yt_input_append_printable(0x7fU, accumulator,
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
	CHECK(yt_input_append_printable(0x1fU, accumulator,
	    sizeof(accumulator), sizeof(accumulator), paged_text,
	    sizeof(paged_text), &newline_flag, &handled,
	    ab36_printable_echo, ab36_printable_carrier, &tape));
	CHECK(!handled && tape.echo_calls == 0U && tape.carrier_calls == 0U
	    && strcmp(accumulator, "A") == 0
	    && strcmp(paged_text, "old") == 0 && newline_flag == -1.0f);
	CHECK(yt_input_append_printable(0x80U, accumulator,
	    sizeof(accumulator), sizeof(accumulator), paged_text,
	    sizeof(paged_text), &newline_flag, &handled,
	    ab36_printable_echo, ab36_printable_carrier, &tape));
	CHECK(!handled && tape.echo_calls == 0U && tape.carrier_calls == 0U
	    && strcmp(accumulator, "A") == 0
	    && strcmp(paged_text, "old") == 0 && newline_flag == -1.0f);

	tape.fail_echo = true;
	CHECK(!yt_input_append_printable(0x20U, accumulator,
	    sizeof(accumulator), sizeof(accumulator), paged_text,
	    sizeof(paged_text), &newline_flag, &handled,
	    ab36_printable_echo, ab36_printable_carrier, &tape));
	CHECK(handled && tape.echo_calls == 1U && tape.carrier_calls == 0U
	    && strcmp(accumulator, "A") == 0
	    && strcmp(paged_text, "old") == 0 && newline_flag == -1.0f);

	tape.fail_echo = false;
	tape.fail_carrier = true;
	tape.echo_calls = 0U;
	CHECK(!yt_input_append_printable('B', accumulator,
	    sizeof(accumulator), sizeof(accumulator), paged_text,
	    sizeof(paged_text), &newline_flag, &handled,
	    ab36_printable_echo, ab36_printable_carrier, &tape));
	CHECK(handled && tape.echo_calls == 1U && tape.carrier_calls == 1U
	    && strcmp(accumulator, "AB") == 0
	    && strcmp(paged_text, "B") == 0 && newline_flag == 1.0f);
}

static void
test_command_save_stages(void)
{
	static const char notice[] =
	    "Command Saved -+- Ctrl-R to Re-use -+- Ctrl-X to cancel.";
	char text[16];
	char queue[16];
	char saved[64];
	char output[128];
	size_t position;
	size_t length;
	bool notice_ready;

	memcpy(text, "A/", 3U);
	memcpy(queue, "XYZ", 4U);
	memcpy(saved, "OLD", 4U);
	memcpy(output, "OUT", 4U);
	position = 1U;
	length = 3U;
	CHECK(yt_input_save_command(text, sizeof(text), queue,
	    sizeof(queue), &position, &length, saved, sizeof(saved), output,
	    sizeof(output), &notice_ready));
	CHECK(notice_ready
	    && strcmp(text, "A") == 0 && queue[0] == '\0'
	    && position == 0U && length == 0U && strcmp(saved, "A") == 0
	    && strcmp(output, notice) == 0);

	memcpy(text, "ABC", 4U);
	memcpy(queue, "XYZ", 4U);
	memcpy(saved, "OLD", 4U);
	memcpy(output, "OUT", 4U);
	position = 1U;
	length = 3U;
	CHECK(yt_input_save_command(text, sizeof(text), queue,
	    sizeof(queue), &position, &length, saved, sizeof(saved), output,
	    sizeof(output), &notice_ready));
	CHECK(!notice_ready && strcmp(text, "ABC") == 0
	    && strcmp(queue, "XYZ") == 0 && position == 1U && length == 3U
	    && strcmp(saved, "OLD") == 0 && strcmp(output, "OUT") == 0);
}

static void
test_b05d_keys(void)
{
	char accumulator[16] = "typed";
	char queue[16] = "old";
	char pager[8] = "";
	size_t position = 1;
	size_t length = 3;
	struct yt_pager_key_state state = {
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

	CHECK(yt_pager_apply_key(&value, &state));
	CHECK(position == 1 && length == 3 && memcmp(queue, "old", 3) == 0);
	value = one('!');
	CHECK(yt_pager_apply_key(&value, &state));
	CHECK(position == 0 && length == 3 && memcmp(queue, "ld!", 3) == 0);
	value = one('\r');
	CHECK(yt_pager_apply_key(&value, &state));
	CHECK(length == 4 && memcmp(queue, "ld!\r", 4) == 0);
	value = one(0x7f);
	CHECK(yt_pager_apply_key(&value, &state));
	CHECK(length == 4);
	value = one(0x1f);
	CHECK(yt_pager_apply_key(&value, &state));
	CHECK(length == 4);
	value = one(' ');
	CHECK(yt_pager_apply_key(&value, &state));
	CHECK(length == 5 && queue[4] == ' ');
	value = one('~');
	CHECK(yt_pager_apply_key(&value, &state));
	CHECK(length == 6 && queue[5] == '~');
	value.bytes[0] = 0;
	value.bytes[1] = 0x48;
	value.length = 2;
	CHECK(yt_pager_apply_key(&value, &state));
	CHECK(length == 6);
	position = 0U;
	length = 0U;
	queue[0] = '\0';
	value = one(0x12);
	CHECK(yt_pager_apply_key(&value, &state));
	CHECK(position == 0U && length == 0U && queue[0] == '\0');
	value = one(0x18);
	CHECK(yt_pager_apply_key(&value, &state));
	CHECK(accumulator[0] == '\0' && queue[0] == '\0');
	CHECK(position == 0 && length == 0 && strcmp(pager, "Q") == 0);
}

static void
test_repeat_transform(void)
{
	struct yt_repeat_transform result;
	char text[1024];
	char saved[1024] = "old";
	char output[128] = "old-output";
	size_t index;
	size_t semicolons;

	snprintf(text, sizeof(text), "A/R3");
	CHECK(yt_input_expand_repeat_with_notice(text, sizeof(text), saved,
	    sizeof(saved), output, sizeof(output), &result));
	CHECK(result.emit_notice && result.count == 3.0f
	    && result.bold_committed
	    && result.failure == YT_REPEAT_FAILURE_NONE
	    && !result.fault_valid
	    && result.fault_site == YT_BASIC_FAULT_SITE_COUNT);
	CHECK(strcmp(text, "A;A;A") == 0 && strcmp(saved, "A;A;A") == 0
	    && strcmp(output, "Command Repeated 3 times -+- Ctrl-R to Re-use "
	    "-+- Ctrl-X to cancel.") == 0);

	snprintf(saved, sizeof(saved), "old");
	snprintf(text, sizeof(text), "A/r2/B/R3");
	CHECK(yt_input_expand_repeat_with_notice(text, sizeof(text), saved,
	    sizeof(saved), output, sizeof(output), &result));
	CHECK(result.emit_notice && result.count == 2.0f);
	CHECK(strcmp(text, "A;A") == 0 && strcmp(saved, "A;A") == 0);

	snprintf(saved, sizeof(saved), "old");
	snprintf(text, sizeof(text), "ABC");
	CHECK(yt_input_expand_repeat_with_notice(text, sizeof(text), saved,
	    sizeof(saved), output, sizeof(output), &result));
	CHECK(!result.emit_notice && result.count == 0.0f
	    && result.failure == YT_REPEAT_FAILURE_NONE
	    && strcmp(text, "ABC") == 0 && strcmp(saved, "old") == 0);

	snprintf(saved, sizeof(saved), "old");
	snprintf(text, sizeof(text), "A/R0");
	CHECK(yt_input_expand_repeat_with_notice(text, sizeof(text), saved,
	    sizeof(saved), output, sizeof(output), &result));
	CHECK(!result.emit_notice && result.count == 0.0f);
	CHECK(result.failure == YT_REPEAT_FAILURE_NONE);
	CHECK(strcmp(text, "A;") == 0 && strcmp(saved, "old") == 0);

	snprintf(saved, sizeof(saved), "old");
	snprintf(text, sizeof(text), "A/R1E999");
	CHECK(!yt_input_expand_repeat_with_notice(text, sizeof(text), saved,
	    sizeof(saved), output, sizeof(output), &result));
	CHECK(!result.emit_notice && result.count == 0.0f
	    && result.failure == YT_REPEAT_FAILURE_VAL_OVERFLOW
	    && result.fault_valid
	    && result.fault_site == YT_BASIC_FAULT_REPEAT_VAL_OVERFLOW);
	CHECK(strcmp(text, "A;") == 0 && strcmp(saved, "old") == 0);

	snprintf(saved, sizeof(saved), "old");
	snprintf(text, sizeof(text), "A/R1.7014118E38");
	CHECK(!yt_input_expand_repeat_with_notice(text, sizeof(text), saved,
	    sizeof(saved), output, sizeof(output), &result));
	CHECK(!result.emit_notice && result.count == 0.0f
	    && result.failure == YT_REPEAT_FAILURE_SINGLE_OVERFLOW
	    && result.fault_valid
	    && result.fault_site == YT_BASIC_FAULT_REPEAT_SINGLE_OVERFLOW);
	CHECK(strcmp(text, "A;") == 0
	    && strcmp(saved, "old") == 0);

	snprintf(text, sizeof(text), "A/R-.1");
	CHECK(yt_input_expand_repeat_with_notice(text, sizeof(text), saved,
	    sizeof(saved), output, sizeof(output), &result));
	CHECK(!result.emit_notice && result.count == -1.0f);
	CHECK(strcmp(text, "A;") == 0 && strcmp(saved, "old") == 0);

	snprintf(text, sizeof(text), "A/R11");
	CHECK(yt_input_expand_repeat_with_notice(text, sizeof(text), saved,
	    sizeof(saved), output, sizeof(output), &result));
	CHECK(result.emit_notice && result.count == 20.0f);
	CHECK(strlen(text) == 39 && strcmp(text, saved) == 0);
	for (index = 0; index < strlen(text); index += 2)
		CHECK(text[index] == 'A');

	for (index = 0; index < 100; ++index)
		text[index] = 'A';
	snprintf(text + 100, sizeof(text) - 100, "/R10");
	CHECK(yt_input_expand_repeat_with_notice(text, sizeof(text), saved,
	    sizeof(saved), output, sizeof(output), &result));
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
		CHECK(yt_input_expand_repeat_with_notice(text, sizeof(text), saved,
		    sizeof(saved), output, sizeof(output), &result));
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
	CHECK(yt_input_split_semicolon(text, queue, sizeof(queue),
	    &position, &length));
	CHECK(strcmp(text, "ABC") == 0 && strcmp(queue, "XYZ") == 0
	    && position == 1U && length == 3U);

	snprintf(text, sizeof(text), "A;B;C");
	queue[0] = '\0';
	position = length = 0;
	CHECK(yt_input_split_semicolon(text, queue, sizeof(queue),
	    &position, &length));
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
test_input_drain(void)
{
	struct yt_input_drain_state drain;
	struct yt_input_value initial = {{'o', 'k'}, 2, false};
	struct yt_input_value value = {{0, 0}, 0, false};
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
test_startup_helpers(void)
{
	struct yt_startup_framing framing;
	uint8_t text[64];
	size_t length;

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

int
main(void)
{
	test_ab36_queued_input();
	test_radio_body_key_classification();
	test_ab36_repeat_recognition();
	test_ab36_repeat_transaction();
	test_ab36_submission();
	test_ab36_backspace_transaction();
	test_ab36_printable_transaction();
	test_command_save_stages();
	test_b05d_keys();
	test_repeat_transform();
	test_semicolon_queue();
	test_queue_program_prepend();
	test_input_drain();
	test_yes_no_candidate();
	test_startup_helpers();
	test_platform_rmt_serial();
	if (failures != 0) {
		fprintf(stderr, "%u command-input test(s) failed\n", failures);
		return 1;
	}
	puts("command-input tests passed");
	return 0;
}
