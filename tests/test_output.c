#include "yt_output.h"

#include "OpenDoor.h"
#include "ODScrn.h"

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
	BYTE column;
	BYTE row;
};

static struct output_call output_calls[8];
static struct emulated_call emulated_calls[16];
static struct output_call local_calls[8];
static BYTE attribute_calls[8];
static struct cursor_call cursor_calls[8];
static BOOL caret_calls[8];
static size_t output_call_count;
static size_t emulated_call_count;
static size_t local_call_count;
static size_t attribute_call_count;
static size_t cursor_call_count;
static size_t caret_call_count;
static size_t clear_call_count;
static tODScrnTextInfo screen_info;
static int failures;

static void
reset_calls(void)
{
	memset(output_calls, 0, sizeof(output_calls));
	memset(emulated_calls, 0, sizeof(emulated_calls));
	memset(local_calls, 0, sizeof(local_calls));
	memset(attribute_calls, 0, sizeof(attribute_calls));
	memset(cursor_calls, 0, sizeof(cursor_calls));
	memset(caret_calls, 0, sizeof(caret_calls));
	memset(&screen_info, 0, sizeof(screen_info));
	screen_info.curx = 1U;
	screen_info.cury = 1U;
	output_call_count = 0U;
	emulated_call_count = 0U;
	local_call_count = 0U;
	attribute_call_count = 0U;
	cursor_call_count = 0U;
	caret_call_count = 0U;
	clear_call_count = 0U;
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
od_clr_scr(void)
{
}

void ODCALL
ODScrnDisplayBuffer(const char *data, INT length)
{
	struct output_call *call;

	CHECK(local_call_count < YT_ARRAY_LEN(local_calls));
	CHECK(length >= 0 && length <= 32767);
	if (local_call_count >= YT_ARRAY_LEN(local_calls)
	    || length < 0 || length > 32767)
		return;
	call = &local_calls[local_call_count++];
	call->length = (size_t)length;
	if (length != 0)
		memcpy(call->data, data, (size_t)length);
}

void ODCALL
ODScrnSetAttribute(BYTE attribute)
{
	CHECK(attribute_call_count < YT_ARRAY_LEN(attribute_calls));
	if (attribute_call_count < YT_ARRAY_LEN(attribute_calls))
		attribute_calls[attribute_call_count++] = attribute;
}

void
ODScrnGetTextInfo(tODScrnTextInfo *info)
{
	*info = screen_info;
}

void ODCALL
ODScrnSetCursorPos(BYTE column, BYTE row)
{
	CHECK(cursor_call_count < YT_ARRAY_LEN(cursor_calls));
	if (cursor_call_count < YT_ARRAY_LEN(cursor_calls)) {
		cursor_calls[cursor_call_count].column = column;
		cursor_calls[cursor_call_count].row = row;
		++cursor_call_count;
	}
}

void
ODScrnEnableCaret(BOOL enabled)
{
	CHECK(caret_call_count < YT_ARRAY_LEN(caret_calls));
	if (caret_call_count < YT_ARRAY_LEN(caret_calls))
		caret_calls[caret_call_count++] = enabled;
}

void
ODScrnClear(void)
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
test_counted_routes(void)
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
	reset_calls();
	yt_out_remote_bytes(large, 32768U);
	CHECK(output_call_count == 2U
	    && !output_calls[0].local_echo && !output_calls[1].local_echo);
	free(large);
}

static void
test_emulated_route(void)
{
	static const uint8_t ansi[] = "\x1b[2J\x1b[4;7HX";
	static const uint8_t embedded_nul[] = {'A', 0U, 'B'};
	struct yt_error error;

	reset_calls();
	yt_error_clear(&error);
	CHECK(yt_out_local_emulated_bytes(ansi, sizeof(ansi) - 1U, &error)
	    && emulated_call_count == 1U
	    && !emulated_calls[0].remote_echo
	    && strcmp(emulated_calls[0].text, (const char *)ansi) == 0
	    && output_call_count == 0U && local_call_count == 0U);
	CHECK(yt_out_local_emulated_bytes(NULL, 0U, &error)
	    && emulated_call_count == 2U
	    && emulated_calls[1].text[0] == '\0');
	yt_error_clear(&error);
	CHECK(!yt_out_local_emulated_bytes(embedded_nul,
	    sizeof(embedded_nul), &error)
	    && error.status == YT_INVALID && emulated_call_count == 2U);
}

static void
test_ansi_opening_local_route(void)
{
	static const uint8_t file_data[] = "\x1b[2JX\r\n\x1a";
	const char *path = "test-output-opening.dat";
	struct yt_error error;
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
	yt_error_clear(&error);
	CHECK(yt_out_opening_file(path, 1.0f, 1.0f, poll_never,
	    poll_never, wait_once, &waits, &error)
	    && waits == 1U && output_call_count == 0U
	    && local_call_count == 0U && emulated_call_count == 3U
	    && strcmp(emulated_calls[0].text, "\x1b[2JX") == 0
	    && strcmp(emulated_calls[1].text, "\r\n") == 0
	    && strcmp(emulated_calls[2].text, "\x1b[0m") == 0
	    && !emulated_calls[0].remote_echo
	    && !emulated_calls[1].remote_echo
	    && !emulated_calls[2].remote_echo);
	CHECK(remove(path) == 0);
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
test_presentation_adapter(void)
{
	static const uint8_t remote_semi[] = {'A', 0U, 'B'};
	static const uint8_t local_semi[] = {'L', 0U};
	struct yt_present_result result;
	int row;
	int column;

	memset(&result, 0, sizeof(result));
	set_event(&result.events[result.event_count++],
	    YT_PRESENT_REMOTE_SEMI, remote_semi, sizeof(remote_semi));
	set_event(&result.events[result.event_count++],
	    YT_PRESENT_REMOTE_LINE, "X", 1U);
	set_event(&result.events[result.event_count++],
	    YT_PRESENT_LOCAL_COLOR, NULL, 0U);
	result.events[result.event_count - 1U].foreground = 30;
	result.events[result.event_count - 1U].background = 4;
	set_event(&result.events[result.event_count++],
	    YT_PRESENT_LOCAL_SEMI, local_semi, sizeof(local_semi));
	set_event(&result.events[result.event_count++],
	    YT_PRESENT_LOCAL_LINE, "row", 3U);
	set_event(&result.events[result.event_count++],
	    YT_PRESENT_LOCAL_PLAY, "ignored", 7U);
	set_event(&result.events[result.event_count++],
	    YT_PRESENT_LOCAL_LOCATE, NULL, 0U);
	result.events[result.event_count - 1U].row = 4;
	result.events[result.event_count - 1U].column = 7;
	result.events[result.event_count - 1U].cursor_visible = 1;
	set_event(&result.events[result.event_count++],
	    YT_PRESENT_LOCAL_BEEP, NULL, 0U);
	set_event(&result.events[result.event_count++],
	    YT_PRESENT_LOCAL_CLEAR, NULL, 0U);

	reset_calls();
	yt_out_present_result(&result);
	CHECK(output_call_count == 3U
	    && output_calls[0].length == sizeof(remote_semi)
	    && !output_calls[0].local_echo
	    && memcmp(output_calls[0].data, remote_semi,
	    sizeof(remote_semi)) == 0
	    && output_calls[1].length == 1U
	    && output_calls[1].data[0] == 'X'
	    && !output_calls[1].local_echo
	    && output_calls[2].length == 1U
	    && output_calls[2].data[0] == '\r'
	    && !output_calls[2].local_echo);
	CHECK(local_call_count == 4U
	    && local_calls[0].length == sizeof(local_semi)
	    && memcmp(local_calls[0].data, local_semi,
	    sizeof(local_semi)) == 0
	    && local_calls[1].length == 3U
	    && memcmp(local_calls[1].data, "row", 3U) == 0
	    && local_calls[2].length == 2U
	    && memcmp(local_calls[2].data, "\r\n", 2U) == 0
	    && local_calls[3].length == 1U
	    && local_calls[3].data[0] == '\a');
	CHECK(attribute_call_count == 1U && attribute_calls[0] == 0xceU
	    && cursor_call_count == 1U
	    && cursor_calls[0].column == 7U && cursor_calls[0].row == 4U
	    && caret_call_count == 1U && caret_calls[0] != FALSE
	    && clear_call_count == 1U && emulated_call_count == 0U);

	screen_info.curx = 23U;
	screen_info.cury = 17U;
	yt_out_cursor_position(&row, &column);
	CHECK(row == 17 && column == 23);
}

int
main(void)
{
	test_counted_routes();
	test_emulated_route();
	test_ansi_opening_local_route();
	test_presentation_adapter();
	if (failures != 0) {
		fprintf(stderr, "test_output: %d failure(s)\n", failures);
		return 1;
	}
	puts("test_output: ok");
	return 0;
}
