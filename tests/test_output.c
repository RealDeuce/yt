#include "yt_output.h"
#include "yt_door.h"

#include "OpenDoor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expression) do { \
	if (!(expression)) { \
		fprintf(stderr, "check failed at %s:%d: %s\n", \
		    __FILE__, __LINE__, #expression); \
		++failures; \
	} \
} while (0)

struct output_call {
	uint8_t data[32767];
	size_t length;
	bool local_echo;
};

struct emulated_call {
	char text[32768];
	bool remote_echo;
};

struct cursor_call {
	INT row;
	INT column;
};

tODControl od_control;
static struct output_call output_calls[16];
static struct emulated_call emulated_calls[32];
static INT attribute_calls[8];
static struct cursor_call cursor_calls[8];
static char putch_calls[8];
static size_t output_call_count;
static size_t emulated_call_count;
static size_t attribute_call_count;
static size_t cursor_call_count;
static size_t putch_call_count;
static size_t clear_call_count;
static INT current_row = 1;
static INT current_column = 1;
static int failures;

struct yt_door *
yt_door_current(void)
{
	return NULL;
}

static void
reset_calls(void)
{
	memset(output_calls, 0, sizeof(output_calls));
	memset(emulated_calls, 0, sizeof(emulated_calls));
	memset(attribute_calls, 0, sizeof(attribute_calls));
	memset(cursor_calls, 0, sizeof(cursor_calls));
	memset(putch_calls, 0, sizeof(putch_calls));
	output_call_count = 0U;
	emulated_call_count = 0U;
	attribute_call_count = 0U;
	cursor_call_count = 0U;
	putch_call_count = 0U;
	clear_call_count = 0U;
	current_row = 1;
	current_column = 1;
}

void ODCALL
od_disp(const char *data, INT length, BOOL local_echo)
{
	struct output_call *call;

	CHECK(output_call_count < YT_ARRAY_LEN(output_calls));
	CHECK(length >= 0 && length <= 32767);
	if (output_call_count >= YT_ARRAY_LEN(output_calls)
	    || length < 0 || length > 32767)
		return;
	call = &output_calls[output_call_count++];
	call->length = (size_t)length;
	call->local_echo = local_echo != FALSE;
	if (length != 0)
		memcpy(call->data, data, (size_t)length);
}

void ODCALL
od_disp_emu(const char *text, BOOL remote_echo)
{
	struct emulated_call *call;
	size_t length;

	CHECK(emulated_call_count < YT_ARRAY_LEN(emulated_calls));
	if (emulated_call_count >= YT_ARRAY_LEN(emulated_calls))
		return;
	call = &emulated_calls[emulated_call_count++];
	length = strlen(text);
	CHECK(length < sizeof(call->text));
	if (length >= sizeof(call->text))
		length = sizeof(call->text) - 1U;
	memcpy(call->text, text, length);
	call->text[length] = '\0';
	call->remote_echo = remote_echo != FALSE;
}

void ODCALL
od_set_attrib(INT attribute)
{
	CHECK(attribute_call_count < YT_ARRAY_LEN(attribute_calls));
	if (attribute_call_count < YT_ARRAY_LEN(attribute_calls))
		attribute_calls[attribute_call_count++] = attribute;
}

void ODCALL
od_get_cursor(INT *row, INT *column)
{
	*row = current_row;
	*column = current_column;
}

void ODCALL
od_set_cursor(INT row, INT column)
{
	CHECK(cursor_call_count < YT_ARRAY_LEN(cursor_calls));
	if (cursor_call_count < YT_ARRAY_LEN(cursor_calls)) {
		cursor_calls[cursor_call_count].row = row;
		cursor_calls[cursor_call_count].column = column;
		++cursor_call_count;
	}
	current_row = row;
	current_column = column;
}

void ODCALL
od_putch(char value)
{
	CHECK(putch_call_count < YT_ARRAY_LEN(putch_calls));
	if (putch_call_count < YT_ARRAY_LEN(putch_calls))
		putch_calls[putch_call_count++] = value;
}

void ODCALL
od_clr_scr(void)
{
	++clear_call_count;
}

static bool
poll_never(void *context, bool *ready, struct yt_error *error)
{
	(void)context;
	(void)error;
	*ready = false;
	return true;
}

static bool
wait_once(void *context, float seconds, struct yt_error *error)
{
	size_t *calls = context;

	(void)error;
	CHECK(seconds == 3.0f);
	++*calls;
	return true;
}

static void
test_counted_combined_route(void)
{
	uint8_t *large;
	size_t index;

	reset_calls();
	yt_out_plain("abc");
	CHECK(output_call_count == 1U
	    && output_calls[0].length == 3U
	    && output_calls[0].local_echo
	    && memcmp(output_calls[0].data, "abc", 3U) == 0);

	large = malloc(32768U);
	CHECK(large != NULL);
	if (large == NULL)
		return;
	for (index = 0U; index < 32768U; ++index)
		large[index] = (uint8_t)(index & 0xffU);
	reset_calls();
	yt_out_plain_bytes(large, 32768U);
	CHECK(output_call_count == 2U
	    && output_calls[0].length == 32767U
	    && output_calls[1].length == 1U
	    && output_calls[0].local_echo && output_calls[1].local_echo
	    && memcmp(output_calls[0].data, large, 32767U) == 0
	    && output_calls[1].data[0] == large[32767]);
	free(large);
}

static void
test_ansi_opening_routes(void)
{
	static const uint8_t file_data[] = "\x1b[2JX\r\n\x1a";
	const char *path = "test-output-opening.dat";
	const char *missing_path = "TEST-OUTPUT-MISSING-53.ANS";
	struct yt_error error;
	uint16_t open_basic_error = 0U;
	FILE *file;
	size_t waits = 0U;

	file = fopen(path, "wb");
	CHECK(file != NULL);
	if (file == NULL)
		return;
	CHECK(fwrite(file_data, 1U, sizeof(file_data) - 1U, file)
	    == sizeof(file_data) - 1U);
	CHECK(fclose(file) == 0);

	reset_calls();
	od_control.od_force_local = TRUE;
	od_control.baud = 19200U;
	yt_error_clear(&error);
	CHECK(yt_out_opening_file(path, 1.0f, 1.0f, poll_never,
	    poll_never, wait_once, &waits, &error)
	    && waits == 1U && output_call_count == 0U
	    && emulated_call_count == 3U
	    && strcmp(emulated_calls[0].text, "\x1b[2JX") == 0
	    && strcmp(emulated_calls[1].text, "\r\n") == 0
	    && strcmp(emulated_calls[2].text, "\x1b[0m") == 0
	    && emulated_calls[0].remote_echo
	    && emulated_calls[1].remote_echo
	    && emulated_calls[2].remote_echo);

	waits = 0U;
	reset_calls();
	od_control.od_force_local = FALSE;
	od_control.baud = 38400U;
	yt_error_clear(&error);
	CHECK(yt_out_opening_file(path, 0.0f, 1.0f, poll_never,
	    poll_never, wait_once, &waits, &error)
	    && waits == 1U && output_call_count == 0U
	    && emulated_call_count == 4U
	    && strcmp(emulated_calls[0].text, "\x1b[2JX") == 0
	    && strcmp(emulated_calls[1].text, "\n\r") == 0
	    && strcmp(emulated_calls[2].text, "\x1b") == 0
	    && strcmp(emulated_calls[3].text, "[0m") == 0
	    && emulated_calls[0].remote_echo
	    && emulated_calls[1].remote_echo
	    && emulated_calls[2].remote_echo
	    && emulated_calls[3].remote_echo);
	CHECK(remove(path) == 0);

	yt_error_clear(&error);
	CHECK(!yt_out_opening_file_observed(missing_path, 0.0f, 1.0f,
	    poll_never, poll_never, wait_once, &waits, &open_basic_error,
	    &error)
	    && open_basic_error == 53U && error.status == YT_NOT_FOUND);
}

static void
set_event(struct yt_present_event *event,
    enum yt_present_operation operation, const void *data, size_t length)
{
	memset(event, 0, sizeof(*event));
	event->operation = operation;
	event->length = length;
	if (length != 0U)
		memcpy(event->data, data, length);
}

static void
build_adapter_result(struct yt_present_result *result)
{
	static const uint8_t remote_semi[] = {'A', 0U, 'B'};
	static const uint8_t local_semi[] = {'L', 0U};

	memset(result, 0, sizeof(*result));
	set_event(&result->events[result->event_count++],
	    YT_PRESENT_REMOTE_SEMI, remote_semi, sizeof(remote_semi));
	set_event(&result->events[result->event_count++],
	    YT_PRESENT_REMOTE_LINE, "X", 1U);
	set_event(&result->events[result->event_count++],
	    YT_PRESENT_LOCAL_COLOR, NULL, 0U);
	result->events[result->event_count - 1U].foreground = 30;
	result->events[result->event_count - 1U].background = 4;
	set_event(&result->events[result->event_count++],
	    YT_PRESENT_LOCAL_SEMI, local_semi, sizeof(local_semi));
	set_event(&result->events[result->event_count++],
	    YT_PRESENT_LOCAL_LINE, "row", 3U);
	set_event(&result->events[result->event_count++],
	    YT_PRESENT_LOCAL_PLAY, "ignored", 7U);
	set_event(&result->events[result->event_count++],
	    YT_PRESENT_LOCAL_LOCATE, NULL, 0U);
	result->events[result->event_count - 1U].row = 4;
	result->events[result->event_count - 1U].column = 7;
	result->events[result->event_count - 1U].cursor_visible = 1;
	set_event(&result->events[result->event_count++],
	    YT_PRESENT_LOCAL_BEEP, NULL, 0U);
	set_event(&result->events[result->event_count++],
	    YT_PRESENT_LOCAL_CLEAR, NULL, 0U);
}

static void
test_presentation_adapter(void)
{
	struct yt_present_result result;
	int row;
	int column;

	build_adapter_result(&result);
	reset_calls();
	od_control.od_force_local = FALSE;
	od_control.baud = 38400U;
	yt_out_present_result(&result);
	CHECK(output_call_count == 1U
	    && output_calls[0].length == 3U
	    && output_calls[0].local_echo
	    && memcmp(output_calls[0].data, "A\0B", 3U) == 0
	    && emulated_call_count == 2U
	    && strcmp(emulated_calls[0].text, "X") == 0
	    && strcmp(emulated_calls[1].text, "\r") == 0
	    && emulated_calls[0].remote_echo && emulated_calls[1].remote_echo
	    && attribute_call_count == 0U && cursor_call_count == 0U
	    && putch_call_count == 0U && clear_call_count == 0U);

	reset_calls();
	od_control.od_force_local = TRUE;
	od_control.baud = 19200U;
	yt_out_present_result(&result);
	CHECK(output_call_count == 3U
	    && output_calls[0].length == 2U
	    && output_calls[0].local_echo
	    && memcmp(output_calls[0].data, "L\0", 2U) == 0
	    && output_calls[1].length == 3U
	    && memcmp(output_calls[1].data, "row", 3U) == 0
	    && output_calls[2].length == 2U
	    && memcmp(output_calls[2].data, "\r\n", 2U) == 0
	    && emulated_call_count == 0U
	    && attribute_call_count == 1U && attribute_calls[0] == 0xce
	    && cursor_call_count == 1U
	    && cursor_calls[0].row == 4 && cursor_calls[0].column == 7
	    && putch_call_count == 1U && putch_calls[0] == '\a'
	    && clear_call_count == 1U);

	current_row = 17;
	current_column = 23;
	yt_out_cursor_position(&row, &column);
	CHECK(row == 17 && column == 23);
}

static void
test_empty_output(void)
{
	struct yt_present_result result;

	memset(&result, 0, sizeof(result));
	set_event(&result.events[result.event_count++],
	    YT_PRESENT_REMOTE_SEMI, NULL, 0U);
	reset_calls();
	od_control.od_force_local = FALSE;
	od_control.baud = 38400U;
	yt_out_present_result(&result);
	CHECK(output_call_count == 0U && emulated_call_count == 0U);
}

static void
test_sound_adapter(void)
{
	static const uint8_t selector_one[] =
	    "\x1b[MBO4L32P32CP64CP64CP64L16EP64L32CP64L12E\x0e";
	struct yt_present_state current;
	struct yt_present_result result;

	memset(&current, 0, sizeof(current));
	current.sound.ansi = 1.0f;
	current.sound.user_sound = -1.0f;
	current.sound.snoop = -1.0f;
	current.sound.local_sound = -1.0f;
	CHECK(yt_present_sound(1.0f, &current, &result) == YT_PRESENT_OK);
	reset_calls();
	od_control.od_force_local = FALSE;
	od_control.baud = 38400U;
	yt_out_present_result(&result);
	CHECK(output_call_count == 0U && emulated_call_count == 1U
	    && emulated_calls[0].remote_echo
	    && strcmp(emulated_calls[0].text,
	    (const char *)selector_one) == 0);

	memset(&current, 0, sizeof(current));
	current.sound.user_sound = -1.0f;
	current.sound.snoop = -1.0f;
	current.sound.local_sound = -1.0f;
	CHECK(yt_present_sound(8.0f, &current, &result) == YT_PRESENT_OK);
	reset_calls();
	od_control.od_force_local = FALSE;
	od_control.baud = 38400U;
	yt_out_present_result(&result);
	CHECK(output_call_count == 0U && emulated_call_count == 1U
	    && emulated_calls[0].text[0] == '\a'
	    && emulated_calls[0].text[1] == '\0'
	    && emulated_calls[0].remote_echo);

	memset(&current, 0, sizeof(current));
	current.sound.ansi = 1.0f;
	current.sound.mode = 1.0f;
	current.sound.user_sound = -1.0f;
	current.sound.snoop = -1.0f;
	current.sound.local_sound = -1.0f;
	CHECK(yt_present_sound(1.0f, &current, &result) == YT_PRESENT_OK
	    && result.remote_length == 0U && result.event_count == 1U
	    && result.events[0].operation == YT_PRESENT_LOCAL_PLAY);
	reset_calls();
	od_control.od_force_local = TRUE;
	od_control.baud = 19200U;
	yt_out_present_result(&result);
	CHECK(output_call_count == 0U && emulated_call_count == 0U
	    && attribute_call_count == 0U && cursor_call_count == 0U
	    && putch_call_count == 0U && clear_call_count == 0U);
}

int
main(void)
{
	test_counted_combined_route();
	test_ansi_opening_routes();
	test_presentation_adapter();
	test_empty_output();
	test_sound_adapter();
	if (failures != 0) {
		fprintf(stderr, "test_output: %d failure(s)\n", failures);
		return 1;
	}
	puts("test_output: ok");
	return 0;
}
