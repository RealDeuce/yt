#include "yt_output.h"

#include "OpenDoor.h"
#include "ODScrn.h"
#include "yt_input_model.h"
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

bool
yt_out_opening_file(const char *path, float mode, float snoop,
    yt_opening_poll_fn poll, void *poll_context,
    enum yt_opening_exit *exit_reason, struct yt_error *error)
{
	struct yt_text_file file;
	struct yt_present_result presentation;
	uint8_t *line;
	size_t cursor = 0U;
	bool available;
	bool success = true;
	enum yt_opening_exit reason = YT_OPENING_EXIT_EOF;

	if (poll == NULL || exit_reason == NULL) {
		if (error != NULL) {
			error->status = YT_INVALID;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "opening poll arguments");
		}
		return false;
	}
	if (!yt_text_read(path, &file, error))
		return false;
	line = malloc(file.length + 1U);
	if (line == NULL) {
		if (error != NULL) {
			error->status = YT_NO_MEMORY;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "allocate opening row");
			(void)snprintf(error->path, sizeof(error->path), "%s", path);
		}
		yt_text_free(&file);
		return false;
	}
	for (;;) {
		size_t length;
		enum yt_present_status status;
		bool local_key;
		bool remote_pending;
		enum yt_opening_row_route route;

		if (!yt_text_line_input_next(file.data, file.length, &cursor,
		    line, file.length, &length, &available)) {
			success = false;
			break;
		}
		if (!available)
			break;
		status = yt_present_opening_row(line, length, 1.0f, snoop,
		    &presentation);
		if (status != YT_PRESENT_OK) {
			success = false;
			break;
		}
		yt_out_present_result(&presentation);
		if (!poll(poll_context, &local_key, &remote_pending)) {
			success = false;
			break;
		}
		route = yt_input_opening_row_route(local_key, remote_pending);
		if (route == YT_OPENING_ROW_STOP_LOCAL) {
			reason = YT_OPENING_EXIT_LOCAL_KEY;
			break;
		}
		status = yt_present_opening_row(line, length, mode, 0.0f,
		    &presentation);
		if (status != YT_PRESENT_OK) {
			success = false;
			break;
		}
		yt_out_present_result(&presentation);
		if (route == YT_OPENING_ROW_STOP_REMOTE) {
			reason = YT_OPENING_EXIT_REMOTE_PENDING;
			break;
		}
	}
	if (!success && error != NULL) {
		error->status = YT_RANGE;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    "compose opening row");
		(void)snprintf(error->path, sizeof(error->path), "%s", path);
	}
	free(line);
	yt_text_free(&file);
	if (success)
		*exit_reason = reason;
	return success;
}
