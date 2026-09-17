#include "yt_input_model.h"
#include "input_editor_test_model.h"
#include "qb.h"
#include "yt_pager.h"
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
	static const struct {
		enum yt_basic_fault_site site;
		const char *text;
		const char *queue;
		size_t queue_position;
		size_t queue_length;
		const char *saved;
		const char *output;
		bool requested;
		bool ready;
	} cases[] = {
		{YT_BASIC_FAULT_ADE0_SLASH_TEST_RIGHT_SPACE,
		    "A/", "XYZ", 1U, 3U, "OLD", "OUT", false, false},
		{YT_BASIC_FAULT_ADE0_SAVE_STRIP_LEFT_SPACE,
		    "A/", "XYZ", 1U, 3U, "OLD", "OUT", true, false},
		{YT_BASIC_FAULT_ADE0_SAVE_COMMAND_CLONE_SPACE,
		    "A", "", 0U, 0U, "OLD", "OUT", true, false},
		{YT_BASIC_FAULT_ADE0_SAVE_NOTICE_CLONE_SPACE,
		    "A", "", 0U, 0U, "A", "OUT", true, false},
		{YT_BASIC_FAULT_ADE0_SAVE_NOTICE_GOSUB_STACK,
		    "A", "", 0U, 0U, "A", notice, true, true},
	};
	struct yt_command_save_transform result;
	char text[16];
	char queue[16];
	char saved[64];
	char output[128];
	size_t position;
	size_t length;
	size_t index;

	for (index = 0U; index < YT_ARRAY_LEN(cases); ++index) {
		memcpy(text, "A/", 3U);
		memcpy(queue, "XYZ", 4U);
		memcpy(saved, "OLD", 4U);
		memcpy(output, "OUT", 4U);
		position = 1U;
		length = 3U;
		CHECK(!yt_input_command_save_staged(text, sizeof(text), queue,
		    sizeof(queue), &position, &length, saved, sizeof(saved),
		    output, sizeof(output), cases[index].site, &result));
		CHECK(result.fault_valid && result.fault_site == cases[index].site
		    && result.save_requested == cases[index].requested
		    && result.notice_ready == cases[index].ready
		    && strcmp(text, cases[index].text) == 0
		    && strcmp(queue, cases[index].queue) == 0
		    && position == cases[index].queue_position
		    && length == cases[index].queue_length
		    && strcmp(saved, cases[index].saved) == 0
		    && strcmp(output, cases[index].output) == 0);
	}

	memcpy(text, "A", 2U);
	memcpy(queue, "XYZ", 4U);
	memcpy(saved, "OLD", 4U);
	memcpy(output, "OUT", 4U);
	position = 1U;
	length = 3U;
	CHECK(!yt_input_command_save_staged(text, sizeof(text), queue,
	    sizeof(queue), &position, &length, saved, sizeof(saved),
	    output, sizeof(output),
	    YT_BASIC_FAULT_ADE0_SLASH_TEST_RIGHT_SPACE, &result));
	CHECK(result.fault_valid
	    && result.fault_site == YT_BASIC_FAULT_ADE0_SLASH_TEST_RIGHT_SPACE
	    && !result.save_requested && !result.notice_ready
	    && strcmp(text, "A") == 0 && strcmp(queue, "XYZ") == 0
	    && position == 1U && length == 3U
	    && strcmp(saved, "OLD") == 0 && strcmp(output, "OUT") == 0);

	memcpy(text, "A/", 3U);
	memcpy(queue, "XYZ", 4U);
	memcpy(saved, "OLD", 4U);
	memcpy(output, "OUT", 4U);
	position = 1U;
	length = 3U;
	CHECK(yt_input_command_save_staged(text, sizeof(text), queue,
	    sizeof(queue), &position, &length, saved, sizeof(saved), output,
	    sizeof(output), YT_BASIC_FAULT_SITE_COUNT, &result));
	CHECK(!result.fault_valid && result.save_requested && result.notice_ready
	    && strcmp(text, "A") == 0 && queue[0] == '\0'
	    && position == 0U && length == 0U && strcmp(saved, "A") == 0
	    && strcmp(output, notice) == 0);

	memcpy(text, "ABC", 4U);
	memcpy(queue, "XYZ", 4U);
	memcpy(saved, "OLD", 4U);
	memcpy(output, "OUT", 4U);
	position = 1U;
	length = 3U;
	CHECK(yt_input_command_save_staged(text, sizeof(text), queue,
	    sizeof(queue), &position, &length, saved, sizeof(saved), output,
	    sizeof(output), YT_BASIC_FAULT_SITE_COUNT, &result));
	CHECK(!result.fault_valid && !result.save_requested
	    && !result.notice_ready && strcmp(text, "ABC") == 0
	    && strcmp(queue, "XYZ") == 0 && position == 1U && length == 3U
	    && strcmp(saved, "OLD") == 0 && strcmp(output, "OUT") == 0);

	memcpy(text, "/", 2U);
	memcpy(queue, "XYZ", 4U);
	memcpy(saved, "OLD", 4U);
	memcpy(output, "OUT", 4U);
	position = 1U;
	length = 3U;
	CHECK(!yt_input_command_save_staged(text, sizeof(text), queue,
	    sizeof(queue), &position, &length, saved, sizeof(saved), output,
	    sizeof(output), YT_BASIC_FAULT_ADE0_SAVE_STRIP_LEFT_SPACE,
	    &result));
	CHECK(!result.fault_valid && strcmp(text, "/") == 0
	    && strcmp(queue, "XYZ") == 0 && strcmp(saved, "OLD") == 0
	    && strcmp(output, "OUT") == 0);

	memcpy(text, "A/", 3U);
	memcpy(queue, "XYZ", 4U);
	memcpy(saved, "OLD", 4U);
	memcpy(output, "OUT", 4U);
	position = 1U;
	length = 3U;
	CHECK(!yt_input_command_save_staged(text, sizeof(text), queue,
	    sizeof(queue), &position, &length, saved, sizeof(saved), output, 4U,
	    YT_BASIC_FAULT_SITE_COUNT, &result));
	CHECK(!result.fault_valid && result.save_requested
	    && !result.notice_ready && strcmp(text, "A") == 0
	    && queue[0] == '\0' && strcmp(saved, "A") == 0
	    && strcmp(output, "OUT") == 0);
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
test_upper_fault_stages(void)
{
	static const enum yt_basic_fault_site body_sites[] = {
		YT_BASIC_FAULT_UPPER_MID_COMPARE_STRING_SPACE,
		YT_BASIC_FAULT_UPPER_MID_VALUE_STRING_SPACE,
		YT_BASIC_FAULT_UPPER_CHR_STRING_SPACE,
	};
	struct yt_upper_transform result;
	char text[8];
	size_t index;

	memcpy(text, "a!b", 4U);
	CHECK(!yt_input_compat_upper_n_staged((uint8_t *)text, 3U,
	    YT_BASIC_FAULT_UPPER_FRAME_STACK, 1U, &result));
	CHECK(result.fault_valid
	    && result.fault_site == YT_BASIC_FAULT_UPPER_FRAME_STACK
	    && !result.scratch_initialized && !result.extracted_valid
	    && !result.mapped_valid && strcmp(text, "a!b") == 0);

	for (index = 0U; index < YT_ARRAY_LEN(body_sites); ++index) {
		memcpy(text, "a!b", 4U);
		CHECK(!yt_input_compat_upper_n_staged((uint8_t *)text, 3U,
		    body_sites[index], 3U, &result));
		CHECK(result.fault_valid && result.fault_site == body_sites[index]
		    && result.scratch_initialized && result.length == 3U
		    && result.index == 3U && strcmp(text, "A!b") == 0);
		if (body_sites[index] == YT_BASIC_FAULT_UPPER_CHR_STRING_SPACE) {
			CHECK(result.extracted_valid && result.extracted == (uint8_t)'b'
			    && result.mapped_valid && result.mapped == (uint8_t)'B');
		}
		else {
			CHECK(!result.extracted_valid && !result.mapped_valid);
		}
	}

	memcpy(text, "a!b", 4U);
	CHECK(yt_input_compat_upper_n_staged((uint8_t *)text, 3U,
	    YT_BASIC_FAULT_SITE_COUNT, 1U, &result));
	CHECK(!result.fault_valid && result.scratch_initialized
	    && result.fault_site == YT_BASIC_FAULT_SITE_COUNT
	    && result.length == 3U && result.index == 4U
	    && !result.extracted_valid && !result.mapped_valid
	    && strcmp(text, "A!B") == 0);

	memcpy(text, "a!b", 4U);
	CHECK(!yt_input_compat_upper_n_staged((uint8_t *)text, 3U,
	    YT_BASIC_FAULT_UPPER_MID_COMPARE_STRING_SPACE, 4U, &result));
	CHECK(!result.fault_valid && strcmp(text, "a!b") == 0);
	memcpy(text, "a!b", 4U);
	CHECK(!yt_input_compat_upper_n_staged((uint8_t *)text, 3U,
	    YT_BASIC_FAULT_UPPER_CHR_STRING_SPACE, 2U, &result));
	CHECK(!result.fault_valid && strcmp(text, "a!b") == 0);
}

static void
test_repeat_prefix_stages(void)
{
	struct yt_repeat_prefix_transform result;
	uint8_t scratch[32];
	uint8_t expected_raw[4];

	memcpy(scratch, "OLD", 4U);
	CHECK(!yt_input_repeat_prefix_staged((const uint8_t *)"a/R2", 4U,
	    scratch, sizeof(scratch),
	    YT_BASIC_FAULT_ADE0_UPPER_SCRATCH_CLONE_SPACE, 1U, &result));
	CHECK(result.fault_valid
	    && result.fault_site
	    == YT_BASIC_FAULT_ADE0_UPPER_SCRATCH_CLONE_SPACE
	    && strcmp((const char *)scratch, "OLD") == 0);

	memcpy(scratch, "OLD", 4U);
	CHECK(!yt_input_repeat_prefix_staged((const uint8_t *)"a/R2", 4U,
	    scratch, sizeof(scratch), YT_BASIC_FAULT_UPPER_CHR_STRING_SPACE,
	    1U, &result));
	CHECK(result.fault_valid
	    && result.fault_site == YT_BASIC_FAULT_UPPER_CHR_STRING_SPACE
	    && result.upper.fault_valid && result.upper.index == 1U
	    && result.upper.extracted_valid
	    && result.upper.extracted == (uint8_t)'a'
	    && result.upper.mapped_valid && result.upper.mapped == (uint8_t)'A'
	    && strcmp((const char *)scratch, "a/R2") == 0);

	memcpy(scratch, "OLD", 4U);
	CHECK(!yt_input_repeat_prefix_staged((const uint8_t *)"a/R2", 4U,
	    scratch, sizeof(scratch),
	    YT_BASIC_FAULT_UPPER_MID_COMPARE_STRING_SPACE, 3U, &result));
	CHECK(result.fault_valid
	    && result.fault_site
	    == YT_BASIC_FAULT_UPPER_MID_COMPARE_STRING_SPACE
	    && result.upper.index == 3U
	    && strcmp((const char *)scratch, "A/R2") == 0);

	memcpy(scratch, "OLD", 4U);
	CHECK(yt_input_repeat_prefix_staged((const uint8_t *)"a/r2", 4U,
	    scratch, sizeof(scratch), YT_BASIC_FAULT_SITE_COUNT, 1U, &result));
	CHECK(!result.fault_valid && !result.upper.fault_valid
	    && result.repeat_position == 2U
	    && strcmp((const char *)scratch, "A/R2") == 0
	    && qb_mbf32_encode(2.0f, expected_raw) == QB_MBF_OK
	    && memcmp(result.work_raw, expected_raw, sizeof(expected_raw)) == 0);

	CHECK(yt_input_repeat_prefix_staged((const uint8_t *)"abc", 3U,
	    scratch, sizeof(scratch), YT_BASIC_FAULT_SITE_COUNT, 1U, &result));
	CHECK(!result.fault_valid && result.repeat_position == 0U
	    && strcmp((const char *)scratch, "ABC") == 0
	    && qb_mbf32_encode(0.0f, expected_raw) == QB_MBF_OK
	    && memcmp(result.work_raw, expected_raw, sizeof(expected_raw)) == 0);

	memcpy(scratch, "OLD", 4U);
	CHECK(!yt_input_repeat_prefix_staged((const uint8_t *)"", 0U,
	    scratch, sizeof(scratch),
	    YT_BASIC_FAULT_ADE0_UPPER_SCRATCH_CLONE_SPACE, 1U, &result));
	CHECK(!result.fault_valid && strcmp((const char *)scratch, "OLD") == 0);
}

static void
test_repeat_parse_stages(void)
{
	struct yt_repeat_parse_transform result;
	char text[64];
	uint8_t upper[64];
	uint8_t expected_raw[8];
	struct qb_val_result parsed;

	memcpy(text, "Ab/R3", 6U);
	memcpy(upper, "AB/R3", 6U);
	CHECK(!yt_input_repeat_parse_staged(text, sizeof(text), upper,
	    sizeof(upper), 3U, YT_BASIC_FAULT_ADE0_REPEAT_PREFIX_LEFT_SPACE,
	    &result));
	CHECK(result.fault_valid
	    && result.fault_site == YT_BASIC_FAULT_ADE0_REPEAT_PREFIX_LEFT_SPACE
	    && result.repeat_reached
	    && result.pending_role == YT_REPEAT_PENDING_NONE
	    && strcmp(text, "Ab/R3") == 0
	    && strcmp((const char *)upper, "AB/R3") == 0);

	memcpy(text, "Ab/R3", 6U);
	memcpy(upper, "AB/R3", 6U);
	CHECK(!yt_input_repeat_parse_staged(text, sizeof(text), upper,
	    sizeof(upper), 3U,
	    YT_BASIC_FAULT_ADE0_REPEAT_SEMICOLON_CONCAT_SPACE, &result));
	CHECK(result.fault_valid
	    && result.fault_site
	    == YT_BASIC_FAULT_ADE0_REPEAT_SEMICOLON_CONCAT_SPACE
	    && result.pending_role == YT_REPEAT_PENDING_PREFIX
	    && result.pending_length == 2U
	    && strcmp(result.pending_string, "Ab") == 0
	    && strcmp(text, "Ab/R3") == 0
	    && strcmp((const char *)upper, "AB/R3") == 0);

	memcpy(text, "Ab/R3", 6U);
	memcpy(upper, "AB/R3", 6U);
	CHECK(!yt_input_repeat_parse_staged(text, sizeof(text), upper,
	    sizeof(upper), 3U,
	    YT_BASIC_FAULT_ADE0_REPEAT_SUFFIX_RIGHT_SPACE, &result));
	CHECK(result.fault_valid
	    && result.fault_site == YT_BASIC_FAULT_ADE0_REPEAT_SUFFIX_RIGHT_SPACE
	    && result.pending_role == YT_REPEAT_PENDING_NONE
	    && strcmp(text, "Ab;") == 0
	    && strcmp((const char *)upper, "AB/R3") == 0);

	memcpy(text, "A/R1E999", 10U);
	memcpy(upper, "A/R1E999", 10U);
	CHECK(!yt_input_repeat_parse_staged(text, sizeof(text), upper,
	    sizeof(upper), 2U, YT_BASIC_FAULT_SITE_COUNT, &result));
	CHECK(result.fault_valid
	    && result.fault_site == YT_BASIC_FAULT_REPEAT_VAL_OVERFLOW
	    && result.failure == YT_REPEAT_FAILURE_VAL_OVERFLOW
	    && result.pending_role == YT_REPEAT_PENDING_SUFFIX
	    && result.pending_length == 5U
	    && strcmp(result.pending_string, "1E999") == 0
	    && strcmp(text, "A;") == 0
	    && strcmp((const char *)upper, "A/R1E999") == 0);

	memcpy(text, "A/R1.7014118E38", 16U);
	memcpy(upper, "A/R1.7014118E38", 16U);
	CHECK(!yt_input_repeat_parse_staged(text, sizeof(text), upper,
	    sizeof(upper), 2U, YT_BASIC_FAULT_SITE_COUNT, &result));
	CHECK(result.fault_valid
	    && result.fault_site == YT_BASIC_FAULT_REPEAT_SINGLE_OVERFLOW
	    && result.failure == YT_REPEAT_FAILURE_SINGLE_OVERFLOW
	    && result.pending_role == YT_REPEAT_PENDING_INTEGER
	    && result.pending_double_valid && result.pending_length == 0U
	    && strcmp(text, "A;") == 0
	    && strcmp((const char *)upper, "A/R1.7014118E38") == 0);
	parsed = qb_val("1.7014118E38");
	CHECK(parsed.valid && !parsed.overflow
	    && qb_mbf64_encode(qb_int(parsed.value), expected_raw) == QB_MBF_OK
	    && memcmp(result.pending_double_raw, expected_raw,
	    sizeof(expected_raw)) == 0);

	memcpy(text, "Ab/R3", 6U);
	memcpy(upper, "AB/R3", 6U);
	CHECK(yt_input_repeat_parse_staged(text, sizeof(text), upper,
	    sizeof(upper), 3U, YT_BASIC_FAULT_SITE_COUNT, &result));
	CHECK(!result.fault_valid && result.repeat_reached
	    && result.failure == YT_REPEAT_FAILURE_NONE && result.count == 3.0f
	    && result.pending_role == YT_REPEAT_PENDING_NONE
	    && !result.pending_double_valid && strcmp(text, "Ab;") == 0
	    && upper[0] == '\0'
	    && qb_mbf32_encode(3.0f, expected_raw) == QB_MBF_OK
	    && memcmp(result.count_raw, expected_raw, 4U) == 0);

	memcpy(text, "A/R11", 6U);
	memcpy(upper, "A/R11", 6U);
	CHECK(yt_input_repeat_parse_staged(text, sizeof(text), upper,
	    sizeof(upper), 2U, YT_BASIC_FAULT_SITE_COUNT, &result));
	CHECK(result.count == 20.0f
	    && qb_mbf32_encode(20.0f, expected_raw) == QB_MBF_OK
	    && memcmp(result.count_raw, expected_raw, 4U) == 0);

	memcpy(text, "/R", 3U);
	memcpy(upper, "/R", 3U);
	CHECK(yt_input_repeat_parse_staged(text, sizeof(text), upper,
	    sizeof(upper), 1U, YT_BASIC_FAULT_SITE_COUNT, &result));
	CHECK(result.repeat_reached && result.count == 0.0f
	    && strcmp(text, ";") == 0 && upper[0] == '\0');
}

static void
test_repeat_build_stages(void)
{
	static const char notice[] =
	    "Command Repeated 3 times -+- Ctrl-R to Re-use -+- "
	    "Ctrl-X to cancel.";
	struct yt_repeat_build_transform result;
	char text[64];
	uint8_t scratch[64];
	char saved[64];
	char output[128];

	memcpy(text, "A;", 3U);
	scratch[0] = '\0';
	memcpy(saved, "old", 4U);
	memcpy(output, "old-output", 11U);
	CHECK(!yt_input_repeat_build_staged(text, sizeof(text), scratch,
	    sizeof(scratch), saved, sizeof(saved), output, sizeof(output), 3.0f,
	    YT_BASIC_FAULT_ADE0_REPEAT_BUILD_CONCAT_SPACE, 2U, &result));
	CHECK(result.fault_valid
	    && result.fault_site == YT_BASIC_FAULT_ADE0_REPEAT_BUILD_CONCAT_SPACE
	    && result.completed_iterations == 1U
	    && result.pending_role == YT_REPEAT_PENDING_NONE
	    && strcmp(text, "A;") == 0
	    && strcmp((const char *)scratch, "A;") == 0
	    && strcmp(saved, "old") == 0 && strcmp(output, "old-output") == 0
	    && !result.bold_committed && !result.notice_ready);

	memcpy(text, "A;", 3U);
	scratch[0] = '\0';
	memcpy(saved, "old", 4U);
	memcpy(output, "old-output", 11U);
	CHECK(!yt_input_repeat_build_staged(text, sizeof(text), scratch,
	    sizeof(scratch), saved, sizeof(saved), output, sizeof(output), 3.0f,
	    YT_BASIC_FAULT_ADE0_REPEAT_FINAL_LEFT_SPACE, 1U, &result));
	CHECK(result.fault_valid
	    && result.fault_site == YT_BASIC_FAULT_ADE0_REPEAT_FINAL_LEFT_SPACE
	    && result.completed_iterations == 3U
	    && result.pending_role == YT_REPEAT_PENDING_EXPANDED
	    && result.pending_length == 5U
	    && strcmp(result.pending_string, "A;A;A") == 0
	    && strcmp(text, "A;") == 0
	    && strcmp((const char *)scratch, "A;A;A;") == 0
	    && strcmp(saved, "old") == 0 && strcmp(output, "old-output") == 0
	    && !result.bold_committed && !result.notice_ready);

	memcpy(text, "A;", 3U);
	scratch[0] = '\0';
	memcpy(saved, "old", 4U);
	memcpy(output, "old-output", 11U);
	CHECK(!yt_input_repeat_build_staged(text, sizeof(text), scratch,
	    sizeof(scratch), saved, sizeof(saved), output, sizeof(output), 3.0f,
	    YT_BASIC_FAULT_ADE0_REPEAT_SAVE_CLONE_SPACE, 1U, &result));
	CHECK(result.fault_valid
	    && result.fault_site == YT_BASIC_FAULT_ADE0_REPEAT_SAVE_CLONE_SPACE
	    && strcmp(text, "A;A;A") == 0 && scratch[0] == '\0'
	    && strcmp(saved, "old") == 0 && strcmp(output, "old-output") == 0
	    && result.pending_role == YT_REPEAT_PENDING_NONE
	    && !result.bold_committed && !result.notice_ready);

	memcpy(text, "A;", 3U);
	scratch[0] = '\0';
	memcpy(saved, "old", 4U);
	memcpy(output, "old-output", 11U);
	CHECK(!yt_input_repeat_build_staged(text, sizeof(text), scratch,
	    sizeof(scratch), saved, sizeof(saved), output, sizeof(output), 3.0f,
	    YT_BASIC_FAULT_ADE0_REPEAT_COUNT_STR_SPACE, 1U, &result));
	CHECK(result.fault_valid
	    && result.fault_site == YT_BASIC_FAULT_ADE0_REPEAT_COUNT_STR_SPACE
	    && strcmp(text, "A;A;A") == 0 && scratch[0] == '\0'
	    && strcmp(saved, "A;A;A") == 0
	    && strcmp(output, "old-output") == 0
	    && result.pending_role == YT_REPEAT_PENDING_NONE
	    && result.bold_committed && !result.notice_ready);

	memcpy(text, "A;", 3U);
	scratch[0] = '\0';
	memcpy(saved, "old", 4U);
	memcpy(output, "old-output", 11U);
	CHECK(!yt_input_repeat_build_staged(text, sizeof(text), scratch,
	    sizeof(scratch), saved, sizeof(saved), output, sizeof(output), 3.0f,
	    YT_BASIC_FAULT_ADE0_REPEAT_PREFIX_CONCAT_SPACE, 1U, &result));
	CHECK(result.fault_valid
	    && result.fault_site == YT_BASIC_FAULT_ADE0_REPEAT_PREFIX_CONCAT_SPACE
	    && result.pending_role == YT_REPEAT_PENDING_COUNT_TEXT
	    && result.pending_length == 2U
	    && strcmp(result.pending_string, " 3") == 0
	    && strcmp(output, "old-output") == 0
	    && result.bold_committed && !result.notice_ready);

	memcpy(text, "A;", 3U);
	scratch[0] = '\0';
	memcpy(saved, "old", 4U);
	memcpy(output, "old-output", 11U);
	CHECK(!yt_input_repeat_build_staged(text, sizeof(text), scratch,
	    sizeof(scratch), saved, sizeof(saved), output, sizeof(output), 3.0f,
	    YT_BASIC_FAULT_ADE0_REPEAT_NOTICE_CONCAT_SPACE, 1U, &result));
	CHECK(result.fault_valid
	    && result.fault_site == YT_BASIC_FAULT_ADE0_REPEAT_NOTICE_CONCAT_SPACE
	    && result.pending_role == YT_REPEAT_PENDING_NOTICE_PREFIX
	    && strcmp(result.pending_string, "Command Repeated 3") == 0
	    && strcmp(output, "old-output") == 0
	    && result.bold_committed && !result.notice_ready);

	memcpy(text, "A;", 3U);
	scratch[0] = '\0';
	memcpy(saved, "old", 4U);
	memcpy(output, "old-output", 11U);
	CHECK(!yt_input_repeat_build_staged(text, sizeof(text), scratch,
	    sizeof(scratch), saved, sizeof(saved), output, sizeof(output), 3.0f,
	    YT_BASIC_FAULT_ADE0_REPEAT_NOTICE_GOSUB_STACK, 1U, &result));
	CHECK(result.fault_valid
	    && result.fault_site == YT_BASIC_FAULT_ADE0_REPEAT_NOTICE_GOSUB_STACK
	    && result.pending_role == YT_REPEAT_PENDING_NONE
	    && strcmp(output, notice) == 0
	    && result.bold_committed && result.notice_ready);

	memcpy(text, "A;", 3U);
	scratch[0] = '\0';
	memcpy(saved, "old", 4U);
	memcpy(output, "old-output", 11U);
	CHECK(yt_input_repeat_build_staged(text, sizeof(text), scratch,
	    sizeof(scratch), saved, sizeof(saved), output, sizeof(output), 3.0f,
	    YT_BASIC_FAULT_SITE_COUNT, 1U, &result));
	CHECK(!result.fault_valid && result.fault_site == YT_BASIC_FAULT_SITE_COUNT
	    && strcmp(text, "A;A;A") == 0 && scratch[0] == '\0'
	    && strcmp(saved, "A;A;A") == 0 && strcmp(output, notice) == 0
	    && result.bold_committed && result.notice_ready);
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
test_semicolon_stages(void)
{
	struct yt_semicolon_transform result;
	char text[32];
	char queue[32];
	size_t position;
	size_t length;

	memcpy(text, "A;B;C", 6U);
	memcpy(queue, "XY", 3U);
	position = 1U;
	length = 2U;
	CHECK(!yt_input_split_semicolon_staged(text, sizeof(text), queue,
	    sizeof(queue), &position, &length,
	    YT_BASIC_FAULT_ADE0_SEMICOLON_TAIL_MID_SPACE, 1U, &result));
	CHECK(result.fault_valid
	    && result.fault_site == YT_BASIC_FAULT_ADE0_SEMICOLON_TAIL_MID_SPACE
	    && result.semicolon_position == 2U
	    && result.pending_role == YT_SEMICOLON_PENDING_NONE
	    && strcmp(text, "A;B;C") == 0 && strcmp(queue, "XY") == 0
	    && position == 1U && length == 2U);

	memcpy(text, "A;B;C", 6U);
	memcpy(queue, "XY", 3U);
	position = 1U;
	length = 2U;
	CHECK(!yt_input_split_semicolon_staged(text, sizeof(text), queue,
	    sizeof(queue), &position, &length,
	    YT_BASIC_FAULT_ADE0_SEMICOLON_QUEUE_CONCAT_SPACE, 1U, &result));
	CHECK(result.fault_valid
	    && result.fault_site
	    == YT_BASIC_FAULT_ADE0_SEMICOLON_QUEUE_CONCAT_SPACE
	    && result.pending_role == YT_SEMICOLON_PENDING_TAIL
	    && result.pending_length == 3U
	    && strcmp(result.pending_string, "B;C") == 0
	    && strcmp(text, "A;B;C") == 0 && strcmp(queue, "XY") == 0
	    && position == 1U && length == 2U);

	memcpy(text, "A;B;C", 6U);
	memcpy(queue, "XY", 3U);
	position = 1U;
	length = 2U;
	CHECK(!yt_input_split_semicolon_staged(text, sizeof(text), queue,
	    sizeof(queue), &position, &length,
	    YT_BASIC_FAULT_ADE0_SEMICOLON_PREFIX_LEFT_SPACE, 1U, &result));
	CHECK(result.fault_valid
	    && result.fault_site
	    == YT_BASIC_FAULT_ADE0_SEMICOLON_PREFIX_LEFT_SPACE
	    && result.pending_role == YT_SEMICOLON_PENDING_NONE
	    && strcmp(text, "A;B;C") == 0 && strcmp(queue, "B;CY") == 0
	    && position == 0U && length == 4U);

	memcpy(text, "A;B;C", 6U);
	memcpy(queue, "XY", 3U);
	position = 1U;
	length = 2U;
	CHECK(!yt_input_split_semicolon_staged(text, sizeof(text), queue,
	    sizeof(queue), &position, &length,
	    YT_BASIC_FAULT_ADE0_SEMICOLON_REPLACEMENT_CHR_SPACE, 1U, &result));
	CHECK(result.fault_valid
	    && result.fault_site
	    == YT_BASIC_FAULT_ADE0_SEMICOLON_REPLACEMENT_CHR_SPACE
	    && result.replacements == 0U && strcmp(text, "A") == 0
	    && strcmp(queue, "B;CY") == 0 && position == 0U && length == 4U);

	memcpy(text, "A;B;C;D", 8U);
	queue[0] = '\0';
	position = 0U;
	length = 0U;
	CHECK(!yt_input_split_semicolon_staged(text, sizeof(text), queue,
	    sizeof(queue), &position, &length,
	    YT_BASIC_FAULT_ADE0_SEMICOLON_REPLACEMENT_CHR_SPACE, 2U, &result));
	CHECK(result.fault_valid && result.replacements == 1U
	    && strcmp(text, "A") == 0 && memcmp(queue, "B\rC;D", 6U) == 0
	    && length == 5U);

	memcpy(text, "A;B;C", 6U);
	memcpy(queue, "XY", 3U);
	position = 1U;
	length = 2U;
	CHECK(!yt_input_split_semicolon_staged(text, sizeof(text), queue,
	    sizeof(queue), &position, &length,
	    YT_BASIC_FAULT_ADE0_SEMICOLON_FINAL_CR_SPACE, 1U, &result));
	CHECK(result.fault_valid
	    && result.fault_site == YT_BASIC_FAULT_ADE0_SEMICOLON_FINAL_CR_SPACE
	    && result.replacements == 1U && strcmp(text, "A") == 0
	    && memcmp(queue, "B\rCY", 5U) == 0 && length == 4U
	    && result.pending_role == YT_SEMICOLON_PENDING_NONE);

	memcpy(text, "A;B;C", 6U);
	memcpy(queue, "XY", 3U);
	position = 1U;
	length = 2U;
	CHECK(!yt_input_split_semicolon_staged(text, sizeof(text), queue,
	    sizeof(queue), &position, &length,
	    YT_BASIC_FAULT_ADE0_SEMICOLON_FINAL_CONCAT_SPACE, 1U, &result));
	CHECK(result.fault_valid
	    && result.fault_site
	    == YT_BASIC_FAULT_ADE0_SEMICOLON_FINAL_CONCAT_SPACE
	    && result.replacements == 1U && strcmp(text, "A") == 0
	    && memcmp(queue, "B\rCY", 5U) == 0 && length == 4U
	    && result.pending_role == YT_SEMICOLON_PENDING_FINAL_CR
	    && result.pending_length == 1U && result.pending_string[0] == '\r');

	memcpy(text, "A;B;C", 6U);
	memcpy(queue, "XY", 3U);
	position = 1U;
	length = 2U;
	CHECK(yt_input_split_semicolon_staged(text, sizeof(text), queue,
	    sizeof(queue), &position, &length, YT_BASIC_FAULT_SITE_COUNT, 1U,
	    &result));
	CHECK(!result.fault_valid && result.fault_site == YT_BASIC_FAULT_SITE_COUNT
	    && result.replacements == 1U && strcmp(text, "A") == 0
	    && memcmp(queue, "B\rCY\r", 6U) == 0
	    && position == 0U && length == 5U
	    && result.pending_role == YT_SEMICOLON_PENDING_NONE);
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

static bool
a8d2_case(const char *response, enum yt_confirmation_fault_site target,
    uint16_t error_number, struct yt_confirmation_transform *result,
	char output[32], char prompt[32], char queue[32],
	size_t *queue_position, size_t *queue_length, float *bold)
{
	size_t prompt_length;

	snprintf(output, 32U, "%s", "old output");
	snprintf(prompt, 32U, "%s", "[y/N] -=> ");
	snprintf(queue, 32U, "%s", "Q\r");
	*queue_position = 0U;
	*queue_length = 2U;
	*bold = 0.0f;
	prompt_length = strlen(prompt);
	return yt_input_confirmation_staged(response, output, 32U,
	    (uint8_t *)prompt, 32U, &prompt_length, queue, 32U,
	    queue_position, queue_length, bold, target,
	    error_number, result);
}

static void
test_a8d2_fault_stages(void)
{
	static const struct {
		enum yt_confirmation_fault_site site;
		uint16_t instruction;
		uint16_t saved_ip;
		uint16_t statement;
		uint16_t destination;
		size_t errors;
	} identities[] = {
		{YT_CONFIRMATION_FAULT_LEFT_ONE, 0xA8EFU, 0xA8F2U, 0xA8E7U,
		    0x4C9AU, 3U},
		{YT_CONFIRMATION_FAULT_FIRST_COPY, 0xA8F4U, 0xA8F7U, 0xA8E7U,
		    0x4C9AU, 1U},
		{YT_CONFIRMATION_FAULT_INVALID_QUEUE_CLEAR, 0xA938U, 0xA93BU,
		    0xA932U, 0x4BE0U, 1U},
		{YT_CONFIRMATION_FAULT_PROMPT_CLEAR, 0xA943U, 0xA946U, 0xA93DU,
		    0x4D3AU, 1U},
	};
	struct yt_confirmation_transform result;
	char output[32];
	char prompt[32];
	char queue[32];
	size_t queue_position;
	size_t queue_length;
	float bold;
	size_t index;

	for (index = 0U; index < YT_ARRAY_LEN(identities); ++index) {
		const struct yt_confirmation_fault_identity *identity =
		    yt_input_confirmation_fault_identity(identities[index].site);

		CHECK(identity != NULL
		    && identity->instruction == identities[index].instruction
		    && identity->saved_ip == identities[index].saved_ip
		    && identity->statement == identities[index].statement
		    && identity->source_line == 40001
		    && identity->destination == identities[index].destination
		    && identity->error_count == identities[index].errors);
	}
	CHECK(yt_input_confirmation_fault_identity(YT_CONFIRMATION_FAULT_NONE) == NULL
	    && yt_input_confirmation_fault_identity(YT_CONFIRMATION_FAULT_SITE_COUNT) == NULL);

	CHECK(a8d2_case("ab", YT_CONFIRMATION_FAULT_LEFT_ONE, 14U, &result,
	    output, prompt, queue, &queue_position, &queue_length, &bold));
	CHECK(result.outcome == YT_CONFIRMATION_BASIC_ERROR
	    && result.fault_site == YT_CONFIRMATION_FAULT_LEFT_ONE
	    && result.error_number == 14U && result.uppercase_complete
	    && !result.left_complete && !result.answer_valid
	    && strcmp(output, "AB") == 0
	    && strcmp(prompt, "[y/N] -=> ") == 0
	    && strcmp(queue, "Q\r") == 0 && queue_length == 2U
	    && bold == 0.0f);
	CHECK(a8d2_case("ab", YT_CONFIRMATION_FAULT_LEFT_ONE, 16U, &result,
	    output, prompt, queue, &queue_position, &queue_length, &bold)
	    && result.outcome == YT_CONFIRMATION_BASIC_ERROR
	    && result.error_number == 16U && strcmp(output, "AB") == 0);
	CHECK(a8d2_case("ab", YT_CONFIRMATION_FAULT_LEFT_ONE, 0x0AC9U, &result,
	    output, prompt, queue, &queue_position, &queue_length, &bold)
	    && result.outcome == YT_CONFIRMATION_INTERNAL_FATAL
	    && result.error_number == 0x0AC9U && strcmp(output, "AB") == 0);

	CHECK(a8d2_case("ab", YT_CONFIRMATION_FAULT_FIRST_COPY, 0x0ACCU,
	    &result, output, prompt, queue, &queue_position, &queue_length,
	    &bold));
	CHECK(result.outcome == YT_CONFIRMATION_INTERNAL_FATAL
	    && result.left_complete && !result.first_copy_complete
	    && !result.answer_valid && strcmp(output, "AB") == 0
	    && strcmp(prompt, "[y/N] -=> ") == 0
	    && queue_length == 2U && bold == 0.0f);
	CHECK(a8d2_case("x", YT_CONFIRMATION_FAULT_INVALID_QUEUE_CLEAR, 0x0ACCU,
	    &result, output, prompt, queue, &queue_position, &queue_length,
	    &bold));
	CHECK(result.outcome == YT_CONFIRMATION_INTERNAL_FATAL
	    && result.answer_valid && result.answer == YT_YES_NO_INVALID
	    && result.first_copy_complete && result.bold_committed
	    && !result.queue_cleared && strcmp(output, "X") == 0
	    && strcmp(queue, "Q\r") == 0 && queue_length == 2U
	    && strcmp(prompt, "[y/N] -=> ") == 0 && bold == 1.0f);
	CHECK(a8d2_case("n", YT_CONFIRMATION_FAULT_PROMPT_CLEAR, 0x0ACCU,
	    &result, output, prompt, queue, &queue_position, &queue_length,
	    &bold));
	CHECK(result.outcome == YT_CONFIRMATION_INTERNAL_FATAL
	    && result.answer_valid && result.answer == YT_YES_NO_NO
	    && !result.prompt_cleared && strcmp(output, "N") == 0
	    && strcmp(prompt, "[y/N] -=> ") == 0
	    && strcmp(queue, "Q\r") == 0 && queue_length == 2U
	    && bold == 0.0f);
	CHECK(a8d2_case("x", YT_CONFIRMATION_FAULT_NONE, 0U, &result,
	    output, prompt, queue, &queue_position, &queue_length, &bold)
	    && result.outcome == YT_CONFIRMATION_RETRY && result.queue_cleared
	    && queue_length == 0U && queue[0] == '\0' && bold == 1.0f
	    && strcmp(prompt, "[y/N] -=> ") == 0);
	CHECK(a8d2_case("n", YT_CONFIRMATION_FAULT_NONE, 0U, &result,
	    output, prompt, queue, &queue_position, &queue_length, &bold)
	    && result.outcome == YT_CONFIRMATION_RETURNED && result.prompt_cleared
	    && prompt[0] == '\0' && strcmp(queue, "Q\r") == 0
	    && queue_length == 2U && strcmp(output, "N") == 0);

	CHECK(!a8d2_case("", YT_CONFIRMATION_FAULT_LEFT_ONE, 14U, &result,
	    output, prompt, queue, &queue_position, &queue_length, &bold)
	    && !a8d2_case("n", YT_CONFIRMATION_FAULT_INVALID_QUEUE_CLEAR, 0x0ACCU,
	    &result, output, prompt, queue, &queue_position, &queue_length,
	    &bold)
	    && !a8d2_case("x", YT_CONFIRMATION_FAULT_PROMPT_CLEAR, 0x0ACCU,
	    &result, output, prompt, queue, &queue_position, &queue_length,
	    &bold));
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
	test_upper_fault_stages();
	test_repeat_prefix_stages();
	test_repeat_parse_stages();
	test_repeat_build_stages();
	test_repeat_transform();
	test_semicolon_stages();
	test_semicolon_queue();
	test_queue_program_prepend();
	test_input_drain();
	test_yes_no_candidate();
	test_a8d2_fault_stages();
	test_startup_helpers();
	test_platform_rmt_serial();
	if (failures != 0) {
		fprintf(stderr, "%u input-model test(s) failed\n", failures);
		return 1;
	}
	puts("input-model tests passed");
	return 0;
}
