#include "yt_output.h"

#include "OpenDoor.h"
#include "yt_door.h"
#include "yt_text.h"

#include <stdlib.h>
#include <string.h>

void
yt_out_plain(const char *text)
{
	od_disp(text, (INT)strlen(text), TRUE);
}

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

static void
present_local_clear(void)
{
	od_clr_scr();
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
				present_local_clear();
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

void
yt_out_plain_line(const char *text)
{
	yt_out_plain(text);
	yt_out_plain("\r\n");
}

void
yt_out_clear(void)
{
	od_clr_scr();
}

struct yt_out_opening_context {
	struct yt_text_input input;
	uint16_t basic_error;
	yt_out_opening_poll_fn poll_local;
	yt_out_opening_poll_fn poll_remote;
	yt_out_opening_wait_fn wait;
	void *client;
};

static bool
out_opening_text_result(struct yt_out_opening_context *opening,
    bool ok, uint16_t basic_error, struct yt_error *error)
{
	opening->basic_error = ok ? 0U : basic_error;
	if (!ok && basic_error != 0U) {
		if (error == NULL)
			return ok;
		error->basic_error = basic_error;
		error->basic_error_valid = true;
	}
	return ok;
}

static bool
out_opening_open_input(void *context, const char *path,
    struct yt_error *error)
{
	struct yt_out_opening_context *opening = context;

	bool ok = yt_text_input_open(&opening->input, path, error);

	return out_opening_text_result(opening, ok,
	    opening->input.last_open.basic_error, error);
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

	bool ok = yt_text_input_eof(&opening->input, eof, error);

	return out_opening_text_result(opening, ok,
	    opening->input.last_read.basic_error, error);
}

static bool
out_opening_read(void *context, const uint8_t **line, size_t *length,
    bool *available, struct yt_error *error)
{
	struct yt_out_opening_context *opening = context;

	bool ok = yt_text_input_read_line(&opening->input, line, length,
	    available, error);

	return out_opening_text_result(opening, ok,
	    opening->input.last_read.basic_error, error);
}

static bool
out_opening_present_local(void *context, const uint8_t *line, size_t length,
    struct yt_error *error)
{
	static const uint8_t newline[] = {'\r', '\n'};

	(void)context;
	(void)error;
	if (local_session()) {
		out_emulated_bytes(line, length);
		out_emulated_bytes(newline, sizeof(newline));
	}
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
	out_emulated_bytes(line, length);
	out_emulated_bytes(newline, sizeof(newline));
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
	static const uint8_t escape[] = "\x1b";
	static const uint8_t suffix[] = "[0m";

	(void)context;
	(void)error;
	out_emulated_bytes(escape, sizeof(escape) - 1U);
	out_emulated_bytes(suffix, sizeof(suffix) - 1U);
	return true;
}

static bool
out_opening_reset_local(void *context, struct yt_error *error)
{
	static const uint8_t reset[] = "\x1b[0m";

	(void)context;
	(void)error;
	if (local_session())
		out_emulated_bytes(reset, sizeof(reset) - 1U);
	return true;
}

static bool
out_opening_close_input(void *context, struct yt_error *error)
{
	struct yt_out_opening_context *opening = context;

	bool ok = yt_text_input_close(&opening->input, error);

	return out_opening_text_result(opening, ok,
	    opening->input.last_close.basic_error, error);
}

static bool
out_opening_close_local(void *context, struct yt_error *error)
{
	(void)context;
	(void)error;
	return true;
}

bool
yt_out_opening_file_observed(const char *path, float mode, float snoop,
    yt_out_opening_poll_fn poll_local,
    yt_out_opening_poll_fn poll_remote, yt_out_opening_wait_fn wait,
    void *poll_context, uint16_t *basic_error, struct yt_error *error)
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

	if (basic_error != NULL)
		*basic_error = 0U;
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
	if (basic_error != NULL)
		*basic_error = context.basic_error;
	yt_text_input_destroy(&context.input);
	return ok;
}

bool
yt_out_opening_file(const char *path, float mode, float snoop,
    yt_out_opening_poll_fn poll_local,
    yt_out_opening_poll_fn poll_remote, yt_out_opening_wait_fn wait,
    void *poll_context, struct yt_error *error)
{
	return yt_out_opening_file_observed(path, mode, snoop, poll_local,
	    poll_remote, wait, poll_context, NULL, error);
}
