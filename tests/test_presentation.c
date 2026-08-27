#include "yt_presentation.h"
#include "yt_pager.h"

#include <stdio.h>
#include <string.h>

static unsigned failures;

#define CHECK(expr) do { \
	if (!(expr)) { \
		fprintf(stderr, "%s:%d: check failed: %s\n", \
		    __FILE__, __LINE__, #expr); \
		++failures; \
	} \
} while (0)

struct replay_capture {
	enum yt_present_operation operations[YT_PRESENT_EVENTS];
	size_t count;
};

static void
capture_remote(void *context, const uint8_t *data, size_t length, bool line)
{
	struct replay_capture *capture = context;

	(void)data;
	(void)length;
	capture->operations[capture->count++] = line
	    ? YT_PRESENT_REMOTE_LINE : YT_PRESENT_REMOTE_SEMI;
}

static void
capture_color(void *context, int foreground, int background)
{
	struct replay_capture *capture = context;

	(void)foreground;
	(void)background;
	capture->operations[capture->count++] = YT_PRESENT_LOCAL_COLOR;
}

static void
capture_text(void *context, const uint8_t *data, size_t length, bool line)
{
	struct replay_capture *capture = context;

	(void)data;
	(void)length;
	capture->operations[capture->count++] = line
	    ? YT_PRESENT_LOCAL_LINE : YT_PRESENT_LOCAL_SEMI;
}

static struct yt_present_state
state(bool ansi)
{
	struct yt_present_state value;

	memset(&value, 0, sizeof(value));
	value.sound.ansi = ansi ? -1.0f : 0.0f;
	value.sound.user_sound = -1.0f;
	value.sound.snoop = -1.0f;
	value.sound.local_sound = -1.0f;
	value.foreground = 2.0f;
	return value;
}

static void
test_color(void)
{
	struct yt_present_state current = state(true);
	struct yt_present_result result;
	static const uint8_t green[] = "\x1b[0;32;40m";

	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == sizeof(green) - 1U);
	CHECK(memcmp(result.remote, green, sizeof(green) - 1U) == 0);
	CHECK(result.event_count == 2);
	CHECK(result.events[0].operation == YT_PRESENT_LOCAL_COLOR);
	CHECK(result.events[0].foreground == 2
	    && result.events[0].background == 0);
	CHECK(result.events[1].operation == YT_PRESENT_REMOTE_SEMI);
	CHECK(current.color_initialized == 1.0f);
	CHECK(current.cached_foreground == 2.0f
	    && current.cached_background == 0.0f);
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == 0 && result.event_count == 1);
	current.foreground = 1.0f;
	current.background = 1.0f;
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	CHECK(current.foreground == 3.0f && current.background == 0.0f);
	current.sound.mode = 2.0f;
	current.foreground = 4.0f;
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == 0);
	CHECK(current.cached_foreground == 3.0f);
}

static void
test_direct_output(void)
{
	struct yt_present_state current = state(false);
	struct yt_present_result result;

	CHECK(yt_present_line((const uint8_t *)"row", 3, &current, &result)
	    == YT_PRESENT_OK);
	CHECK(result.remote_length == 5);
	CHECK(memcmp(result.remote, "row\r\n", 5) == 0);
	CHECK(result.event_count == 3);
	CHECK(result.events[0].operation == YT_PRESENT_LOCAL_LINE);
	CHECK(result.events[1].operation == YT_PRESENT_REMOTE_LINE);
	CHECK(result.events[2].operation == YT_PRESENT_REMOTE_SEMI);
	current.sound.mode = 1.0f;
	CHECK(yt_present_character((const uint8_t *)"x", 1,
	    &current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == 0 && result.event_count == 1);
	CHECK(result.events[0].operation == YT_PRESENT_LOCAL_SEMI);
	current.sound.snoop = 0.0f;
	CHECK(yt_present_line((const uint8_t *)"hidden", 6,
	    &current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == 0 && result.event_count == 0);
}

static void
test_paged_output(void)
{
	struct yt_present_state current = state(false);
	struct yt_present_result result;

	CHECK(yt_present_paged_text((const uint8_t *)"row", 3,
	    &current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == 3
	    && memcmp(result.remote, "row", 3) == 0);
	CHECK(result.event_count == 2);
	CHECK(result.events[0].operation == YT_PRESENT_LOCAL_SEMI);
	CHECK(result.events[1].operation == YT_PRESENT_REMOTE_SEMI);
	CHECK(yt_present_paged_finish(false, &current, &result)
	    == YT_PRESENT_OK);
	CHECK(result.remote_length == 2
	    && memcmp(result.remote, "\n\r", 2) == 0);
	CHECK(result.event_count == 3);
	CHECK(result.events[0].operation == YT_PRESENT_REMOTE_LINE);
	CHECK(result.events[1].operation == YT_PRESENT_LOCAL_LINE);
	CHECK(result.events[2].operation == YT_PRESENT_LOCAL_COLOR
	    && result.events[2].foreground == 7
	    && result.events[2].background == 0);

	current.sound.mode = 2.0f;
	CHECK(yt_present_paged_text((const uint8_t *)"local", 5,
	    &current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == 0 && result.event_count == 1);
	CHECK(result.events[0].operation == YT_PRESENT_LOCAL_SEMI);
	CHECK(yt_present_paged_finish(false, &current, &result)
	    == YT_PRESENT_OK);
	CHECK(result.remote_length == 0 && result.event_count == 2);
	CHECK(result.events[0].operation == YT_PRESENT_LOCAL_LINE);
	CHECK(result.events[1].operation == YT_PRESENT_LOCAL_COLOR);
	CHECK(yt_present_paged_finish(true, &current, &result)
	    == YT_PRESENT_OK);
	CHECK(result.remote_length == 0 && result.event_count == 1);
	CHECK(result.events[0].operation == YT_PRESENT_LOCAL_COLOR);
}

static void
test_editor_echo(void)
{
	static const uint8_t local_erase[] = {0x1d, ' ', 0x1d};
	static const uint8_t remote_erase[] = {'\b', ' ', '\b'};
	struct yt_present_state current = state(false);
	struct yt_present_result result;

	CHECK(yt_present_editor_echo(local_erase, sizeof(local_erase),
	    remote_erase, sizeof(remote_erase), &current, &result)
	    == YT_PRESENT_OK);
	CHECK(result.remote_length == sizeof(remote_erase)
	    && memcmp(result.remote, remote_erase, sizeof(remote_erase)) == 0);
	CHECK(result.event_count == 2);
	CHECK(result.events[0].operation == YT_PRESENT_LOCAL_SEMI
	    && result.events[0].length == sizeof(local_erase)
	    && memcmp(result.events[0].data, local_erase,
	    sizeof(local_erase)) == 0);
	CHECK(result.events[1].operation == YT_PRESENT_REMOTE_SEMI);
	current.sound.mode = 2.0f;
	CHECK(yt_present_editor_echo((const uint8_t *)"x", 1,
	    (const uint8_t *)"x", 1, &current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == 0 && result.event_count == 1);
	CHECK(result.events[0].operation == YT_PRESENT_LOCAL_SEMI);
	CHECK(yt_present_local_line((const uint8_t *)"carrier", 7,
	    &current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == 0 && result.event_count == 1);
	CHECK(result.events[0].operation == YT_PRESENT_LOCAL_LINE);
	current.sound.snoop = 0.0f;
	CHECK(yt_present_local_line((const uint8_t *)"hidden", 6,
	    &current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == 0 && result.event_count == 0);
}

struct pager_capture {
	uint8_t remote[256];
	size_t remote_length;
	int last_local_foreground;
	int last_local_background;
};

static void
pager_capture_result(struct pager_capture *capture,
    const struct yt_present_result *result)
{
	size_t index;

	CHECK(result->remote_length <= sizeof(capture->remote)
	    - capture->remote_length);
	if (result->remote_length <= sizeof(capture->remote)
	    - capture->remote_length) {
		memcpy(capture->remote + capture->remote_length, result->remote,
		    result->remote_length);
		capture->remote_length += result->remote_length;
	}
	for (index = 0; index < result->event_count; ++index) {
		if (result->events[index].operation
		    == YT_PRESENT_LOCAL_COLOR) {
			capture->last_local_foreground =
			    result->events[index].foreground;
			capture->last_local_background =
			    result->events[index].background;
		}
	}
}

static void
pager_fixture_b05d(struct yt_pager_state *pager,
    struct yt_present_state *present, const uint8_t *text, size_t length,
    struct pager_capture *capture)
{
	struct yt_present_result result;
	int unused;

	CHECK(yt_present_paged_text(text, length, present, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	CHECK(yt_present_paged_finish(pager->newline_flag != 0.0f, present,
	    &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	CHECK(!yt_pager_advance(pager, present, &unused));
	pager->newline_flag = 0.0f;
}

static struct pager_capture
pager_fixture(const char *answer, bool ansi, float mode,
    struct yt_pager_state *pager, struct yt_present_state *present)
{
	static const uint8_t prompt[] =
	    "[ENTER] for more, [E] to end, or [NS] for Non-stop ";
	static const uint8_t notice[] = "Ctrl-X to Stop";
	struct pager_capture capture;
	struct yt_present_result result;
	char accumulator[80];
	char response[80];
	size_t index;
	int saved_foreground;
	bool emit_notice;

	memset(&capture, 0, sizeof(capture));
	*present = state(ansi);
	present->sound.mode = mode;
	present->foreground = 6.0f;
	memset(pager, 0, sizeof(*pager));
	pager->line_count = 22.0f;
	pager->foreground = 6;
	CHECK(yt_pager_advance(pager, present, &saved_foreground));
	CHECK(saved_foreground == 6 && pager->line_count == 0.0f);
	pager_fixture_b05d(pager, present, prompt, sizeof(prompt) - 1U,
	    &capture);
	yt_pager_editor_enter(pager, accumulator, sizeof(accumulator));
	CHECK(pager->line_count == 0.0f && pager->nonstop == 0.0f
	    && pager->key[0] == '\0' && accumulator[0] == '\0');
	for (index = 0; answer[index] != '\0'; ++index) {
		uint8_t byte = (uint8_t)answer[index];

		accumulator[index] = (char)byte;
		accumulator[index + 1U] = '\0';
		CHECK(yt_present_editor_echo(&byte, 1, &byte, 1, present,
		    &result) == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		pager->newline_flag = 1.0f;
	}
	pager->newline_flag = 0.0f;
	CHECK(yt_present_line(NULL, 0, present, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	snprintf(response, sizeof(response), "%s", accumulator);
	emit_notice = yt_pager_accept_response(pager, response,
	    sizeof(response));
	if (emit_notice)
		pager_fixture_b05d(pager, present, notice,
		    sizeof(notice) - 1U, &capture);
	yt_pager_complete(pager, present, saved_foreground);
	return capture;
}

static void
test_pager_transactions(void)
{
	static const uint8_t prompt[] =
	    "[ENTER] for more, [E] to end, or [NS] for Non-stop ";
	static const uint8_t ansi_prompt[] = "\x1b[0;33;40;1m";
	static const uint8_t ansi_submit[] = "\x1b[0;33;40m";
	struct yt_pager_state pager;
	struct yt_present_state present;
	struct pager_capture capture;
	uint8_t expected[256];
	size_t length;

	capture = pager_fixture("", false, 0.0f, &pager, &present);
	length = sizeof(prompt) - 1U;
	memcpy(expected, prompt, length);
	memcpy(expected + length, "\r\n", 2);
	length += 2U;
	CHECK(capture.remote_length == length
	    && memcmp(capture.remote, expected, length) == 0);
	CHECK(pager.line_count == 0.0f && pager.nonstop == 0.0f
	    && pager.key[0] == '\0' && pager.foreground == 6
	    && present.foreground == 6.0f && present.bold == 1.0f);
	CHECK(capture.last_local_foreground == 7
	    && capture.last_local_background == 0);

	capture = pager_fixture("E", false, 0.0f, &pager, &present);
	length = sizeof(prompt) - 1U;
	memcpy(expected, prompt, length);
	expected[length++] = 'E';
	memcpy(expected + length, "\r\n", 2);
	length += 2U;
	CHECK(capture.remote_length == length
	    && memcmp(capture.remote, expected, length) == 0);
	CHECK(strcmp(pager.key, "Q") == 0 && pager.nonstop == 0.0f);

	capture = pager_fixture("NS", false, 0.0f, &pager, &present);
	length = sizeof(prompt) - 1U;
	memcpy(expected, prompt, length);
	memcpy(expected + length, "NS\r\nCtrl-X to Stop\n\r", 20);
	length += 20U;
	CHECK(capture.remote_length == length
	    && memcmp(capture.remote, expected, length) == 0);
	CHECK(strcmp(pager.key, "NS") == 0 && pager.nonstop == 1.0f
	    && pager.line_count == 1.0f);
	CHECK(capture.last_local_foreground == 7
	    && capture.last_local_background == 0);

	capture = pager_fixture("", true, 0.0f, &pager, &present);
	length = 0;
	memcpy(expected + length, ansi_prompt, sizeof(ansi_prompt) - 1U);
	length += sizeof(ansi_prompt) - 1U;
	memcpy(expected + length, prompt, sizeof(prompt) - 1U);
	length += sizeof(prompt) - 1U;
	memcpy(expected + length, ansi_submit, sizeof(ansi_submit) - 1U);
	length += sizeof(ansi_submit) - 1U;
	memcpy(expected + length, "\r\n", 2);
	length += 2U;
	CHECK(capture.remote_length == length
	    && memcmp(capture.remote, expected, length) == 0);
	CHECK(present.bold == 0.0f && present.foreground == 6.0f);
	CHECK(capture.last_local_foreground == 6
	    && capture.last_local_background == 0);

	capture = pager_fixture("E", false, 2.0f, &pager, &present);
	CHECK(capture.remote_length == 2
	    && memcmp(capture.remote, "\r\n", 2) == 0);
}

static void
test_pager_gates(void)
{
	struct yt_present_state present = state(false);
	struct yt_pager_state pager;
	char response[80];
	int saved = -1;

	memset(&pager, 0, sizeof(pager));
	pager.foreground = 5;
	pager.line_count = 21.0f;
	CHECK(!yt_pager_advance(&pager, &present, &saved));
	CHECK(pager.line_count == 22.0f && saved == -1);
	pager.line_count = 22.0f;
	pager.nonstop = 2.0f;
	CHECK(!yt_pager_advance(&pager, &present, &saved));
	CHECK(pager.line_count == 23.0f);
	pager.nonstop = 0.0f;
	pager.line_count = 22.0f;
	pager.newline_flag = -1.0f;
	CHECK(!yt_pager_advance(&pager, &present, &saved));
	pager.newline_flag = 0.0f;
	pager.line_count = 24.0f;
	CHECK(yt_pager_advance(&pager, &present, &saved));
	CHECK(saved == 5 && pager.line_count == 0.0f
	    && pager.foreground == 3 && present.foreground == 3.0f
	    && present.bold == 1.0f && pager.newline_flag == 1.0f);

	snprintf(response, sizeof(response), "never");
	CHECK(!yt_pager_accept_response(&pager, response, sizeof(response)));
	CHECK(strcmp(response, "NEVER") == 0
	    && strcmp(pager.key, "NEVER") == 0);
	yt_pager_complete(&pager, &present, saved);
	CHECK(strcmp(pager.key, "Q") == 0 && pager.foreground == 5
	    && present.foreground == 5.0f);
}

static struct pager_capture
fatal_fixture(const uint8_t *notice, size_t notice_length, bool ansi)
{
	struct yt_pager_state pager;
	struct yt_present_state present = state(ansi);
	struct yt_present_result result;
	struct pager_capture capture;

	memset(&pager, 0, sizeof(pager));
	memset(&capture, 0, sizeof(capture));
	pager.foreground = 3;
	present.foreground = 3.0f;
	CHECK(yt_present_line(NULL, 0, &present, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &present, notice, notice_length, &capture);
	return capture;
}

static void
test_editor_terminal_notices(void)
{
	static const uint8_t inactivity[] = "\aUSER FELL ASLEEP!";
	static const uint8_t session[] =
	    "\a\a\aTIME LIMIT EXCEEDED!\a\a\a";
	static const uint8_t ansi[] = "\x1b[0;33;40m";
	struct pager_capture capture;
	uint8_t expected[80];
	size_t length;

	capture = fatal_fixture(inactivity, sizeof(inactivity) - 1U, false);
	length = 0;
	memcpy(expected + length, "\r\n", 2);
	length += 2U;
	memcpy(expected + length, inactivity, sizeof(inactivity) - 1U);
	length += sizeof(inactivity) - 1U;
	memcpy(expected + length, "\n\r", 2);
	length += 2U;
	CHECK(capture.remote_length == length
	    && memcmp(capture.remote, expected, length) == 0);

	capture = fatal_fixture(inactivity, sizeof(inactivity) - 1U, true);
	length = 0;
	memcpy(expected + length, ansi, sizeof(ansi) - 1U);
	length += sizeof(ansi) - 1U;
	memcpy(expected + length, "\r\n", 2);
	length += 2U;
	memcpy(expected + length, inactivity, sizeof(inactivity) - 1U);
	length += sizeof(inactivity) - 1U;
	memcpy(expected + length, "\n\r", 2);
	length += 2U;
	CHECK(capture.remote_length == length
	    && memcmp(capture.remote, expected, length) == 0);

	capture = fatal_fixture(session, sizeof(session) - 1U, false);
	length = 0;
	memcpy(expected + length, "\r\n", 2);
	length += 2U;
	memcpy(expected + length, session, sizeof(session) - 1U);
	length += sizeof(session) - 1U;
	memcpy(expected + length, "\n\r", 2);
	length += 2U;
	CHECK(capture.remote_length == length
	    && memcmp(capture.remote, expected, length) == 0);

	capture = fatal_fixture(session, sizeof(session) - 1U, true);
	length = 0;
	memcpy(expected + length, ansi, sizeof(ansi) - 1U);
	length += sizeof(ansi) - 1U;
	memcpy(expected + length, "\r\n", 2);
	length += 2U;
	memcpy(expected + length, session, sizeof(session) - 1U);
	length += sizeof(session) - 1U;
	memcpy(expected + length, "\n\r", 2);
	length += 2U;
	CHECK(capture.remote_length == length
	    && memcmp(capture.remote, expected, length) == 0);
}

static void
test_formatting_wrappers(void)
{
	struct yt_present_state current = state(false);
	struct yt_present_result result;
	uint8_t mutable[16] = {'a', 'b'};
	size_t mutable_length = 2;
	uint8_t overlong[79];

	CHECK(yt_present_right_aligned((const uint8_t *)"abcd", 4, 2.5f,
	    &current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == 3
	    && memcmp(result.remote, "bcd", 3) == 0);
	current.sound.conversion_mode = 4;
	CHECK(yt_present_right_aligned((const uint8_t *)"abcd", 4, 2.5f,
	    &current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == 2
	    && memcmp(result.remote, "cd", 2) == 0);
	CHECK(yt_present_right_aligned((const uint8_t *)"abc", 3, 3.4f,
	    &current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == 3
	    && memcmp(result.remote, "abc", 3) == 0);

	current.sound.conversion_mode = 0;
	CHECK(yt_present_fixed_width(mutable, &mutable_length,
	    sizeof(mutable), 4.0f, &current, &result) == YT_PRESENT_OK);
	CHECK(mutable_length == 4 && memcmp(mutable, "ab  ", 4) == 0);
	CHECK(result.remote_length == 4
	    && memcmp(result.remote, "ab  ", 4) == 0);

	current.bold = 0.0f;
	CHECK(yt_present_bold_line((const uint8_t *)"x", 1,
	    &current, &result) == YT_PRESENT_OK);
	CHECK(current.bold == 1.0f);
	CHECK(result.remote_length == 3
	    && memcmp(result.remote, "x\r\n", 3) == 0);
	current.bold = 7.0f;
	CHECK(yt_present_bold_character((const uint8_t *)"y", 1,
	    &current, &result) == YT_PRESENT_OK);
	CHECK(current.bold == 7.0f);

	CHECK(yt_present_centered_line((const uint8_t *)"A", 1,
	    &current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == 42);
	CHECK(result.remote[38] == ' ' && result.remote[39] == 'A');
	CHECK(result.remote[40] == '\r' && result.remote[41] == '\n');
	CHECK(yt_present_centered_line(NULL, 0, &current, &result)
	    == YT_PRESENT_OK);
	CHECK(result.remote_length == 2
	    && memcmp(result.remote, "\r\n", 2) == 0);
	memset(overlong, 'Z', sizeof(overlong));
	CHECK(yt_present_centered_line(overlong, sizeof(overlong),
	    &current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == sizeof(overlong) + 2U);
}

static void
test_attention(void)
{
	static const uint8_t cue[] =
	    "MBO2T200L64FBEAP8FBEAP8FBEAP4FBEAP8FBEAP8FBEAP4T128";
	static const uint8_t first_color[] = "\x1b[0;33;41;5;1m";
	static const uint8_t second_color[] = "\x1b[0;33;40m\r\n\x1b[";
	struct yt_present_state current = state(true);
	struct yt_present_result result;
	struct replay_capture capture;
	const struct yt_present_sink sink = {
		.context = &capture,
		.remote = capture_remote,
		.local_color = capture_color,
		.local_text = capture_text,
	};
	uint8_t expected[256];
	size_t expected_length = 0;

	memcpy(expected + expected_length, first_color,
	    sizeof(first_color) - 1U);
	expected_length += sizeof(first_color) - 1U;
	memcpy(expected + expected_length, "ALERT", 5);
	expected_length += 5;
	memcpy(expected + expected_length, second_color,
	    sizeof(second_color) - 1U);
	expected_length += sizeof(second_color) - 1U;
	memcpy(expected + expected_length, cue, sizeof(cue) - 1U);
	expected_length += sizeof(cue) - 1U;
	expected[expected_length++] = 0x0e;
	CHECK(yt_present_attention((const uint8_t *)"ALERT", 5,
	    &current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == expected_length);
	CHECK(memcmp(result.remote, expected, expected_length) == 0);
	CHECK(result.event_count == 11);
	CHECK(result.events[0].operation == YT_PRESENT_LOCAL_COLOR);
	CHECK(result.events[0].foreground == 30
	    && result.events[0].background == 4);
	CHECK(result.events[1].operation == YT_PRESENT_REMOTE_SEMI);
	CHECK(result.events[2].operation == YT_PRESENT_LOCAL_SEMI);
	CHECK(result.events[3].operation == YT_PRESENT_REMOTE_SEMI);
	CHECK(result.events[4].operation == YT_PRESENT_LOCAL_COLOR);
	CHECK(result.events[4].foreground == 6
	    && result.events[4].background == 0);
	CHECK(result.events[5].operation == YT_PRESENT_REMOTE_SEMI);
	CHECK(result.events[6].operation == YT_PRESENT_LOCAL_LINE);
	CHECK(result.events[7].operation == YT_PRESENT_REMOTE_LINE);
	CHECK(result.events[8].operation == YT_PRESENT_REMOTE_SEMI);
	CHECK(result.events[9].operation == YT_PRESENT_REMOTE_SEMI);
	CHECK(result.events[10].operation == YT_PRESENT_LOCAL_PLAY);
	CHECK(result.events[10].length == sizeof(cue) - 1U);
	memset(&capture, 0, sizeof(capture));
	yt_present_replay(&result, &sink);
	CHECK(capture.count == 10);
	for (size_t index = 0; index < capture.count; ++index)
		CHECK(capture.operations[index] == result.events[index].operation);
	CHECK(yt_present_pc_attribute(30, 4) == 0xce);
	CHECK(current.foreground == 3.0f && current.background == 0.0f);
	CHECK(current.bold == 0.0f && current.blink == 0.0f);
	CHECK(current.sound.scratch_length == 0);

	current = state(false);
	CHECK(yt_present_attention((const uint8_t *)"ALERT", 5,
	    &current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == 8);
	CHECK(memcmp(result.remote, "ALERT\r\n\x07", 8) == 0);
	CHECK(result.event_count == 7);
	CHECK(result.events[6].operation == YT_PRESENT_LOCAL_PLAY);
	CHECK(result.events[6].length == 11);
}

static void
test_sound_toggle(void)
{
	struct yt_present_state current = state(true);
	struct yt_present_result result;

	current.sound.user_sound = 0.0f;
	CHECK(yt_present_sound_toggle(&current, &result) == YT_PRESENT_OK);
	CHECK(current.sound.user_sound == -1.0f);
	CHECK(result.remote_length > 9);
	CHECK(result.event_count == 7);
	CHECK(result.events[0].operation == YT_PRESENT_LOCAL_COLOR);
	CHECK(result.events[1].operation == YT_PRESENT_REMOTE_SEMI);
	CHECK(result.events[2].operation == YT_PRESENT_LOCAL_LINE);
	CHECK(result.events[3].operation == YT_PRESENT_REMOTE_LINE);
	CHECK(result.events[3].length == 8
	    && memcmp(result.events[3].data, "Sound ON", 8) == 0);
	CHECK(result.events[4].operation == YT_PRESENT_REMOTE_SEMI);
	CHECK(result.events[5].operation == YT_PRESENT_REMOTE_SEMI);
	CHECK(result.events[6].operation == YT_PRESENT_LOCAL_PLAY);

	current = state(false);
	current.sound.user_sound = 40000.0f;
	CHECK(yt_present_sound_toggle(&current, &result)
	    == YT_PRESENT_SOUND_ERROR);
	CHECK(result.event_count == 0 && result.remote_length == 0);

	current = state(true);
	CHECK(yt_present_sound(4.0f, &current, &result) == YT_PRESENT_OK);
	CHECK(result.event_count == 2);
	CHECK(result.events[0].operation == YT_PRESENT_REMOTE_SEMI);
	CHECK(result.events[1].operation == YT_PRESENT_LOCAL_PLAY);
}

static void
test_time_helpers(void)
{
	struct yt_present_state current = state(true);
	struct yt_present_time_state time;
	struct yt_present_result result;
	static const float update_reads[] = {100, 100, 100, 100};
	static const float rollover_reads[] = {1, 10, 10};
	size_t used;
	bool updated;
	bool warned;
	float remembered = 6.0f;

	memset(&time, 0, sizeof(time));
	time.deadline = 460.0f;
	memcpy(time.text, " 6:00  ", 7);
	time.text_length = 7;
	CHECK(yt_present_refresh_time(&time, update_reads,
	    sizeof(update_reads) / sizeof(update_reads[0]), &used, 4, 9,
	    &current, &result, &updated) == YT_PRESENT_OK);
	CHECK(updated && used == 4);
	CHECK(time.next_refresh == 101.0f);
	CHECK(time.text_length == 7
	    && memcmp(time.text, " 6:00  ", 7) == 0);
	CHECK(time.saved_row == 4 && time.saved_column == 9);
	CHECK(result.event_count == 5);
	CHECK(result.events[0].operation == YT_PRESENT_LOCAL_LOCATE
	    && result.events[0].row == 25 && result.events[0].column == 71);
	CHECK(result.events[1].operation == YT_PRESENT_LOCAL_COLOR
	    && result.events[1].foreground == 11
	    && result.events[1].background == 1);
	CHECK(result.events[2].operation == YT_PRESENT_LOCAL_SEMI);
	CHECK(result.events[3].operation == YT_PRESENT_LOCAL_LOCATE
	    && result.events[3].row == 4 && result.events[3].column == 9
	    && result.events[3].cursor_visible == 1
	    && result.events[3].cursor_start == 1
	    && result.events[3].cursor_stop == 16);
	CHECK(result.events[4].operation == YT_PRESENT_LOCAL_COLOR
	    && result.events[4].foreground == 7);

	memset(&time, 0, sizeof(time));
	time.deadline = 80000.0f;
	CHECK(yt_present_refresh_time(&time, rollover_reads,
	    sizeof(rollover_reads) / sizeof(rollover_reads[0]), &used, 1, 1,
	    &current, &result, &updated) == YT_PRESENT_OK);
	CHECK(!updated && used == 3);
	CHECK(time.deadline == -6400.0f && time.next_refresh == 11.0f);
	CHECK(result.event_count == 0);

	current = state(true);
	CHECK(yt_present_low_time((const uint8_t *)" 5:59  ", 7,
	    &remembered, &current, &result, &warned) == YT_PRESENT_OK);
	CHECK(warned && remembered == 5.0f);
	CHECK(result.event_count == 16);
	CHECK(result.events[5].operation == YT_PRESENT_REMOTE_SEMI
	    && result.events[5].length == 1
	    && result.events[5].data[0] == '\a');
	CHECK(result.events[8].operation == YT_PRESENT_LOCAL_LINE
	    && result.events[8].length == 17
	    && memcmp(result.events[8].data, "Time Left: 5:59  ", 17) == 0);
	CHECK(current.bold == 0.0f && current.blink == 0.0f);
	CHECK(yt_present_low_time((const uint8_t *)" 5:59  ", 7,
	    &remembered, &current, &result, &warned) == YT_PRESENT_OK);
	CHECK(!warned && result.event_count == 0);

	current = state(false);
	current.sound.mode = 1.0f;
	remembered = 6.0f;
	CHECK(yt_present_low_time((const uint8_t *)" 5:59  ", 7,
	    &remembered, &current, &result, &warned) == YT_PRESENT_OK);
	CHECK(warned && result.event_count == 4);
	CHECK(result.events[1].operation == YT_PRESENT_LOCAL_BEEP);
	CHECK(current.bold == 1.0f && current.blink == 1.0f);
}

static void
test_press_any_key_presentation(void)
{
	static const uint8_t ansi_prompt[] =
	    "\x1b[0;33;40;1m*[ Press any Key ]*";
	static const uint8_t ansi_cleanup[] =
	    "\r\x1b[0;33;40m                   \r";
	struct yt_present_state current = state(true);
	struct yt_present_result result;
	float saved;

	current.foreground = 7.0f;
	CHECK(yt_present_press_prompt(&current, &result, &saved)
	    == YT_PRESENT_OK);
	CHECK(saved == 7.0f && current.foreground == 3.0f);
	CHECK(result.remote_length == sizeof(ansi_prompt) - 1U
	    && memcmp(result.remote, ansi_prompt, sizeof(ansi_prompt) - 1U)
	    == 0);
	CHECK(result.event_count == 4);
	CHECK(result.events[0].operation == YT_PRESENT_LOCAL_COLOR
	    && result.events[0].foreground == 14
	    && result.events[0].background == 0);
	CHECK(result.events[2].operation == YT_PRESENT_LOCAL_SEMI);

	CHECK(yt_present_press_cleanup(saved, &current, &result)
	    == YT_PRESENT_OK);
	CHECK(current.foreground == 7.0f);
	CHECK(result.remote_length == sizeof(ansi_cleanup) - 1U
	    && memcmp(result.remote, ansi_cleanup,
	    sizeof(ansi_cleanup) - 1U) == 0);
	CHECK(result.events[0].operation == YT_PRESENT_LOCAL_LOCATE
	    && result.events[0].row == -1 && result.events[0].column == 1);
	CHECK(result.events[2].operation == YT_PRESENT_LOCAL_COLOR
	    && result.events[2].foreground == 6
	    && result.events[2].background == 0);
	CHECK(result.events[result.event_count - 1U].operation
	    == YT_PRESENT_LOCAL_LOCATE);

	current = state(false);
	current.foreground = 5.0f;
	CHECK(yt_present_press_prompt(&current, &result, &saved)
	    == YT_PRESENT_OK);
	CHECK(result.remote_length == strlen("*[ Press any Key ]*")
	    && memcmp(result.remote, "*[ Press any Key ]*",
	    result.remote_length) == 0);
	CHECK(yt_present_press_cleanup(saved, &current, &result)
	    == YT_PRESENT_OK);
	CHECK(result.remote_length == 21U
	    && result.remote[0] == '\r' && result.remote[20] == '\r');

	current = state(false);
	current.sound.mode = 2.0f;
	CHECK(yt_present_press_prompt(&current, &result, &saved)
	    == YT_PRESENT_OK);
	CHECK(yt_present_press_cleanup(saved, &current, &result)
	    == YT_PRESENT_OK);
	CHECK(result.remote_length == 19U
	    && memcmp(result.remote, "                   ", 19) == 0);
}

int
main(void)
{
	test_color();
	test_direct_output();
	test_paged_output();
	test_editor_echo();
	test_pager_transactions();
	test_pager_gates();
	test_editor_terminal_notices();
	test_formatting_wrappers();
	test_attention();
	test_sound_toggle();
	test_time_helpers();
	test_press_any_key_presentation();
	if (failures != 0) {
		fprintf(stderr, "%u test(s) failed\n", failures);
		return 1;
	}
	puts("test_presentation: ok");
	return 0;
}
