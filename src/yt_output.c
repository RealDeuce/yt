#include "yt_output.h"

#include "OpenDoor.h"
#include "ODScrn.h"
#include "yt_text.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

void
yt_out(const char *text)
{
	od_disp(text, (INT)strlen(text), TRUE);
}

void
yt_out_bytes(const void *data, size_t length)
{
	const uint8_t *cursor = data;

	while (length > 0) {
		INT amount = length > 32767U ? 32767 : (INT)length;

		od_disp((const char *)cursor, amount, TRUE);
		cursor += (size_t)amount;
		length -= (size_t)amount;
	}
}

void
yt_out_remote_bytes(const void *data, size_t length)
{
	const uint8_t *cursor = data;

	while (length > 0) {
		INT amount = length > 32767U ? 32767 : (INT)length;

		od_disp((const char *)cursor, amount, FALSE);
		cursor += (size_t)amount;
		length -= (size_t)amount;
	}
}

static void
present_remote(void *context, const uint8_t *data, size_t length, bool line)
{
	static const uint8_t carriage_return = '\r';

	(void)context;
	yt_out_remote_bytes(data, length);
	if (line)
		yt_out_remote_bytes(&carriage_return, 1);
}

static void
present_local_bytes(const uint8_t *data, size_t length)
{
	while (length > 0) {
		INT amount = length > 32767U ? 32767 : (INT)length;

		ODScrnDisplayBuffer((const char *)data, amount);
		data += (size_t)amount;
		length -= (size_t)amount;
	}
}

static void
present_local_color(void *context, int foreground, int background)
{
	uint8_t attribute;

	(void)context;
	attribute = yt_present_pc_attribute(foreground, background);
	ODScrnSetAttribute(attribute);
}

static void
present_local_text(void *context, const uint8_t *data, size_t length,
    bool line)
{
	static const uint8_t newline[] = {'\r', '\n'};

	(void)context;
	present_local_bytes(data, length);
	if (line)
		present_local_bytes(newline, sizeof(newline));
}

static void
present_local_locate(void *context, int row, int column, int cursor_visible,
    int cursor_start, int cursor_stop)
{
	tODScrnTextInfo info;

	(void)context;
	(void)cursor_start;
	(void)cursor_stop;
	if (row < 1) {
		ODScrnGetTextInfo(&info);
		row = info.cury;
		if (column < 1)
			column = info.curx;
	}
	ODScrnSetCursorPos((BYTE)column, (BYTE)row);
	if (cursor_visible >= 0)
		ODScrnEnableCaret(cursor_visible != 0);
}

static void
present_local_beep(void *context)
{
	static const uint8_t bell = '\a';

	(void)context;
	present_local_bytes(&bell, 1);
}

static void
present_local_clear(void *context)
{
	(void)context;
	ODScrnClear();
}

void
yt_out_present_result(const struct yt_present_result *result)
{
	const struct yt_present_sink sink = {
		.context = NULL,
		.remote = present_remote,
		.local_color = present_local_color,
		.local_text = present_local_text,
		.local_locate = present_local_locate,
		.local_beep = present_local_beep,
		.local_clear = present_local_clear,
	};

	yt_present_replay(result, &sink);
}

void
yt_out_cursor_position(int *row, int *column)
{
	tODScrnTextInfo info;

	ODScrnGetTextInfo(&info);
	if (row != NULL)
		*row = info.cury;
	if (column != NULL)
		*column = info.curx;
}

void
yt_outf(const char *format, ...)
{
	char stack[1024];
	char *buffer = stack;
	va_list arguments;
	va_list copy;
	int length;

	va_start(arguments, format);
	va_copy(copy, arguments);
	length = vsnprintf(stack, sizeof(stack), format, arguments);
	va_end(arguments);
	if (length < 0) {
		va_end(copy);
		return;
	}
	if ((size_t)length >= sizeof(stack)) {
		buffer = malloc((size_t)length + 1U);
		if (buffer == NULL) {
			va_end(copy);
			return;
		}
		(void)vsnprintf(buffer, (size_t)length + 1U, format, copy);
	}
	va_end(copy);
	yt_out_bytes(buffer, (size_t)length);
	if (buffer != stack)
		free(buffer);
}

void
yt_out_line(const char *text)
{
	yt_out(text);
	yt_out("\r\n");
}

void
yt_out_clear(void)
{
	od_clr_scr();
}

bool
yt_out_file(const char *path, struct yt_error *error)
{
	struct yt_text_file file;

	if (!yt_text_read(path, &file, error))
		return false;
	yt_out_bytes(file.data, file.length);
	yt_text_free(&file);
	return true;
}

struct yt_out_opening_context {
	struct yt_text_input input;
	yt_out_opening_poll_fn poll_local;
	yt_out_opening_poll_fn poll_remote;
	yt_out_opening_wait_fn wait;
	void *client;
};

static bool
out_opening_open_input(void *context, const char *path,
    struct yt_error *error)
{
	struct yt_out_opening_context *opening = context;

	return yt_text_input_open(&opening->input, path, error);
}

static bool
out_opening_open_local(void *context, struct yt_error *error)
{
	(void)context;
	(void)error;
	return true;
}

static bool
out_opening_eof(void *context, bool *eof, struct yt_error *error)
{
	struct yt_out_opening_context *opening = context;

	return yt_text_input_eof(&opening->input, eof, error);
}

static bool
out_opening_read(void *context, const uint8_t **line, size_t *length,
    bool *available, struct yt_error *error)
{
	struct yt_out_opening_context *opening = context;

	return yt_text_input_read_line(&opening->input, line, length, available,
	    error);
}

static bool
out_opening_present_local(void *context, const uint8_t *line, size_t length,
    struct yt_error *error)
{
	static const uint8_t newline[] = {'\r', '\n'};

	(void)context;
	(void)error;
	present_local_bytes(line, length);
	present_local_bytes(newline, sizeof(newline));
	return true;
}

static bool
out_opening_poll_local(void *context, bool *ready, struct yt_error *error)
{
	struct yt_out_opening_context *opening = context;

	return opening->poll_local(opening->client, ready, error);
}

static bool
out_opening_present_remote(void *context, const uint8_t *line, size_t length,
    struct yt_error *error)
{
	static const uint8_t newline[] = {'\n', '\r'};

	(void)context;
	(void)error;
	yt_out_remote_bytes(line, length);
	yt_out_remote_bytes(newline, sizeof(newline));
	return true;
}

static bool
out_opening_poll_remote(void *context, bool *ready, struct yt_error *error)
{
	struct yt_out_opening_context *opening = context;

	return opening->poll_remote(opening->client, ready, error);
}

static bool
out_opening_wait(void *context, float seconds, struct yt_error *error)
{
	struct yt_out_opening_context *opening = context;

	return opening->wait(opening->client, seconds, error);
}

static bool
out_opening_reset_remote(void *context, struct yt_error *error)
{
	static const uint8_t reset[] = "\x1b[0m";

	(void)context;
	(void)error;
	yt_out_remote_bytes(reset, sizeof(reset) - 1U);
	return true;
}

static bool
out_opening_reset_local(void *context, struct yt_error *error)
{
	static const uint8_t reset[] = "\x1b[0m";

	(void)context;
	(void)error;
	present_local_bytes(reset, sizeof(reset) - 1U);
	return true;
}

static bool
out_opening_close_input(void *context, struct yt_error *error)
{
	struct yt_out_opening_context *opening = context;

	return yt_text_input_close(&opening->input, error);
}

static bool
out_opening_close_local(void *context, struct yt_error *error)
{
	(void)context;
	(void)error;
	return true;
}

bool
yt_out_opening_file(const char *path, float mode, float snoop,
    yt_out_opening_poll_fn poll_local,
    yt_out_opening_poll_fn poll_remote, yt_out_opening_wait_fn wait,
    void *poll_context, struct yt_error *error)
{
	static const struct yt_opening_stream_ops ops = {
		out_opening_open_input,
		out_opening_open_local,
		out_opening_eof,
		out_opening_read,
		out_opening_present_local,
		out_opening_poll_local,
		out_opening_present_remote,
		out_opening_poll_remote,
		out_opening_wait,
		out_opening_reset_remote,
		out_opening_reset_local,
		out_opening_close_input,
		out_opening_close_local,
	};
	struct yt_out_opening_context context = {
		.poll_local = poll_local,
		.poll_remote = poll_remote,
		.wait = wait,
		.client = poll_context,
	};
	struct yt_opening_stream_state state = {
		.path = path,
		.mode = mode,
		.snoop = snoop,
	};
	bool ok;

	if (poll_local == NULL || poll_remote == NULL || wait == NULL) {
		if (error != NULL) {
			error->status = YT_INVALID;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "opening stream arguments");
		}
		return false;
	}
	yt_text_input_init(&context.input);
	ok = yt_opening_stream_run(&state, &ops, &context, error);
	yt_text_input_destroy(&context.input);
	return ok;
}
