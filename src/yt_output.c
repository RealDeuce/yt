#include "yt_output.h"

#include "OpenDoor.h"
#include "yt_door.h"
#include "yt_text.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

void
yt_out_plain_bytes(const void *data, size_t length)
{
	const uint8_t *cursor = data;

	while (length > 0) {
		INT amount = length > 32767U ? 32767 : (INT)length;

		od_disp((const char *)cursor, amount, TRUE);
		cursor += (size_t)amount;
		length -= (size_t)amount;
	}
}

static void
out_emulated_bytes(const void *data, size_t length)
{
	char stack[1024];
	char *text = stack;

	if (length == 0U)
		return;
	if (data == NULL)
		return;
	if (memchr(data, 0, length) != NULL) {
		yt_out_plain_bytes(data, length);
		return;
	}
	if (length >= sizeof(stack)) {
		text = malloc(length + 1U);
		if (text == NULL) {
			yt_out_plain_bytes(data, length);
			return;
		}
	}
	memcpy(text, data, length);
	text[length] = '\0';
	/* OpenDoors' normal combined remote/local emulator path. */
	od_disp_emu(text, TRUE);
	if (text != stack)
		free(text);
}

static void
present_combined(const uint8_t *data, size_t length, bool line)
{
	static const uint8_t carriage_return = '\r';

	out_emulated_bytes(data, length);
	if (line)
		out_emulated_bytes(&carriage_return, 1U);
}

static bool
local_session(void)
{
	struct yt_door *door = yt_door_current();

	return door != NULL ? door->identity.local
	    : od_control.od_force_local != FALSE;
}

static void
present_local_color(int foreground, int background)
{
	uint8_t attribute;

	attribute = yt_present_pc_attribute(foreground, background);
	od_set_attrib(attribute);
}

static void
present_local_text(const uint8_t *data, size_t length, bool line)
{
	static const uint8_t newline[] = {'\r', '\n'};

	yt_out_plain_bytes(data, length);
	if (line)
		yt_out_plain_bytes(newline, sizeof(newline));
}

static void
present_local_locate(int row, int column, int cursor_visible,
    int cursor_start, int cursor_stop)
{
	INT current_row;
	INT current_column;

	(void)cursor_start;
	(void)cursor_stop;
	if (row < 1) {
		od_get_cursor(&current_row, &current_column);
		row = current_row;
		if (column < 1)
			column = current_column;
	}
	od_set_cursor(row, column);
	(void)cursor_visible;
}

static void
present_local_beep(void)
{
	static const uint8_t bell = '\a';

	od_putch((char)bell);
}

void
yt_out_present_result(const struct yt_present_result *result)
{
	const bool local = local_session();
	size_t index;

	if (result == NULL)
		return;
	for (index = 0U; index < result->event_count; ++index) {
		const struct yt_present_event *event = &result->events[index];

		switch (event->operation) {
		case YT_PRESENT_REMOTE_LINE:
		case YT_PRESENT_REMOTE_SEMI:
			if (!local) {
				present_combined(event->data, event->length,
				    event->operation == YT_PRESENT_REMOTE_LINE);
			}
			break;
		case YT_PRESENT_LOCAL_COLOR:
			if (local)
				present_local_color(event->foreground,
				    event->background);
			break;
		case YT_PRESENT_LOCAL_LINE:
		case YT_PRESENT_LOCAL_SEMI:
			if (local)
				present_local_text(event->data, event->length,
				    event->operation == YT_PRESENT_LOCAL_LINE);
			break;
		case YT_PRESENT_LOCAL_PLAY:
			break;
		case YT_PRESENT_LOCAL_LOCATE:
			if (local)
				present_local_locate(event->row, event->column,
				    event->cursor_visible, event->cursor_start,
				    event->cursor_stop);
			break;
		case YT_PRESENT_LOCAL_BEEP:
			if (local)
				present_local_beep();
			break;
		case YT_PRESENT_LOCAL_CLEAR:
			if (local)
				od_clr_scr();
			break;
		}
	}
}

void
yt_out_cursor_position(int *row, int *column)
{
	INT current_row;
	INT current_column;

	od_get_cursor(&current_row, &current_column);
	if (row != NULL)
		*row = current_row;
	if (column != NULL)
		*column = current_column;
}

static bool
out_opening_text_result(uint16_t *observed,
    bool ok, uint16_t basic_error, struct yt_error *error)
{
	*observed = ok ? 0U : basic_error;
	if (!ok && basic_error != 0U) {
		if (error == NULL)
			return ok;
		error->basic_error = basic_error;
		error->basic_error_valid = true;
	}
	return ok;
}

static bool
out_opening_retry_current(const struct yt_error *error)
{
	return error != NULL && error->basic_error_valid
	    && error->basic_error == 24U;
}

static void
out_opening_set_error(struct yt_error *error, enum yt_status status,
    const char *operation, const char *path)
{
	if (error == NULL)
		return;
	error->status = status;
	error->system_error = errno;
	(void)snprintf(error->operation, sizeof(error->operation), "%s",
	    operation);
	(void)snprintf(error->path, sizeof(error->path), "%s",
	    path != NULL ? path : "");
}

static bool
out_opening_poll_local(struct yt_input *input, bool *ready,
    struct yt_error *error)
{
	struct yt_input_value local = {{0, 0}, 0, false};

	if (!yt_input_poll_source(input, false, &local)) {
		out_opening_set_error(error, YT_IO_ERROR,
		    "ANSI opening local input poll", NULL);
		return false;
	}
	*ready = local.length != 0U;
	return true;
}

static bool
out_opening_wait(struct yt_input *input, struct yt_error *error)
{
	if (yt_input_pause(input, 3.0))
		return true;
	out_opening_set_error(error, YT_IO_ERROR, "ANSI opening EOF wait", NULL);
	return false;
}

bool
yt_out_opening_file(const char *path, float mode, float snoop,
    struct yt_input *session_input, uint16_t *basic_error,
    struct yt_error *error)
{
	static const uint8_t local_newline[] = {'\r', '\n'};
	static const uint8_t remote_newline[] = {'\n', '\r'};
	static const uint8_t escape[] = "\x1b";
	static const uint8_t reset_suffix[] = "[0m";
	static const uint8_t reset[] = "\x1b[0m";
	struct yt_text_input input;
	struct yt_error local_error;
	struct yt_error *active_error = error != NULL ? error : &local_error;
	uint16_t observed_basic_error = 0U;
	bool result = false;

	if (basic_error != NULL)
		*basic_error = 0U;
	if (session_input == NULL) {
		errno = 0;
		out_opening_set_error(error, YT_INVALID,
		    "opening stream arguments", NULL);
		return false;
	}
	if (path == NULL) {
		errno = 0;
		out_opening_set_error(error, YT_INVALID, "ANSI opening stream",
		    NULL);
		return false;
	}
	yt_text_input_init(&input);
#define OPENING_RETRY(call) do { \
	for (;;) { \
		yt_error_clear(active_error); \
		if (call) \
			break; \
		if (!out_opening_retry_current(active_error)) \
			goto done; \
	} \
} while (0)
#define OPENING_TEXT_RETRY(call, basic) do { \
	for (;;) { \
		bool opening_ok; \
		yt_error_clear(active_error); \
		opening_ok = (call); \
		if (out_opening_text_result(&observed_basic_error, opening_ok, \
		    (basic), active_error)) \
			break; \
		if (!out_opening_retry_current(active_error)) \
			goto done; \
	} \
} while (0)
	OPENING_TEXT_RETRY(yt_text_input_open(&input, path, active_error),
	    input.last_open.basic_error);
	for (;;) {
		const uint8_t *line;
		size_t length;
		bool available;
		bool eof;
		bool ready;

		OPENING_TEXT_RETRY(yt_text_input_eof(&input, &eof, active_error),
		    input.last_read_basic_error);
		if (eof) {
			if (!out_opening_wait(session_input, active_error))
				goto done;
			break;
		}
		OPENING_TEXT_RETRY(yt_text_input_read_line(&input, &line, &length,
		    &available, active_error), input.last_read_basic_error);
		if (!available) {
			errno = 0;
			out_opening_set_error(active_error, YT_EOF,
			    "ANSI LINE INPUT after EOF check", path);
			goto done;
		}
		if (snoop != 0.0f) {
			yt_error_clear(active_error);
			if (local_session()) {
				out_emulated_bytes(line, length);
				out_emulated_bytes(local_newline,
				    sizeof(local_newline));
			}
		}
		OPENING_RETRY(out_opening_poll_local(session_input, &ready,
		    active_error));
		if (ready)
			break;
		if (mode != 1.0f) {
			yt_error_clear(active_error);
			out_emulated_bytes(line, length);
			out_emulated_bytes(remote_newline,
			    sizeof(remote_newline));
			OPENING_RETRY(yt_input_source_ready(session_input, true,
			    &ready));
			if (ready)
				break;
		}
	}
	if (mode == 0.0f) {
		yt_error_clear(active_error);
		out_emulated_bytes(escape, sizeof(escape) - 1U);
		out_emulated_bytes(reset_suffix, sizeof(reset_suffix) - 1U);
	}
	if (snoop != 0.0f) {
		yt_error_clear(active_error);
		if (local_session())
			out_emulated_bytes(reset, sizeof(reset) - 1U);
	}
	OPENING_TEXT_RETRY(yt_text_input_close(&input, active_error),
	    input.last_close.basic_error);
	result = true;

done:
#undef OPENING_TEXT_RETRY
#undef OPENING_RETRY
	if (basic_error != NULL)
		*basic_error = observed_basic_error;
	yt_text_input_destroy(&input);
	return result;
}
