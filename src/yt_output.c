#include "yt_output.h"

#include "OpenDoor.h"
#include "ODScrn.h"
#include "yt_text.h"

#include <stdlib.h>
#include <string.h>

static struct yt_text_device_state remote_device;

void
yt_out_remote_device_reset(void)
{
	memset(&remote_device, 0, sizeof(remote_device));
}

void
yt_out_remote_device_state(struct yt_text_device_state *state)
{
	if (state != NULL)
		*state = remote_device;
}

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

static bool
remote_device_write(void *context, enum yt_text_device_write_phase phase,
    const uint8_t *data, size_t requested,
    struct yt_text_device_write_observation *observation)
{
	(void)context;
	(void)phase;
	(void)data;
	memset(observation, 0, sizeof(*observation));
	observation->terminal_position = -1;
	observation->accepted = requested;
	return true;
}

static bool
remote_device_apply_observed(const uint8_t *data, size_t length, bool line,
    uint16_t *basic_error)
{
	struct yt_text_device_print_result result;
	struct yt_error error;
	bool ok;

	if (basic_error != NULL)
		*basic_error = 0U;
	remote_device.selected = true;
	yt_error_clear(&error);
	ok = yt_text_device_print(&remote_device, data, length, line,
	    YT_TEXT_DEVICE_COM1, 0x82U, 5U, remote_device_write, NULL,
	    &result, &error);
	if (!ok && basic_error != NULL)
		*basic_error = result.basic_error;
	return ok;
}

static bool
remote_device_apply(const uint8_t *data, size_t length, bool line)
{
	return remote_device_apply_observed(data, length, line, NULL);
}

bool
yt_out_local_emulated_bytes(const void *data, size_t length,
    struct yt_error *error)
{
	char stack[1024];
	char *text = stack;

	if ((data == NULL && length != 0U) || length == SIZE_MAX
	    || (length != 0U && memchr(data, 0, length) != NULL)) {
		if (error != NULL) {
			error->status = YT_INVALID;
			error->system_error = 0;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "local emulated output");
			error->path[0] = '\0';
		}
		return false;
	}
	if (length >= sizeof(stack)) {
		text = malloc(length + 1U);
		if (text == NULL) {
			if (error != NULL) {
				error->status = YT_NO_MEMORY;
				error->system_error = 0;
				(void)snprintf(error->operation,
				    sizeof(error->operation), "%s",
				    "allocate local emulated output");
				error->path[0] = '\0';
			}
			return false;
		}
	}
	if (length != 0U)
		memcpy(text, data, length);
	text[length] = '\0';
	od_disp_emu(text, FALSE);
	if (text != stack)
		free(text);
	return true;
}

static bool
present_remote(void *context, const uint8_t *data, size_t length, bool line)
{
	static const uint8_t carriage_return = '\r';

	(void)context;
	if (!remote_device_apply(data, length, line))
		return false;
	yt_out_remote_bytes(data, length);
	if (line)
		yt_out_remote_bytes(&carriage_return, 1U);
	return true;
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

	(void)yt_present_replay(result, &sink);
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
out_opening_remote_statement(struct yt_out_opening_context *opening,
    const uint8_t *data, size_t length, bool newline,
    struct yt_error *error)
{
	uint16_t basic_error;

	for (;;) {
		if (remote_device_apply_observed(data, length, newline,
		    &basic_error)) {
			opening->basic_error = 0U;
			return true;
		}
		if (basic_error != 24U) {
			if (error != NULL) {
				error->status = YT_IO_ERROR;
				error->system_error = 0;
				(void)snprintf(error->operation,
				    sizeof(error->operation), "%s",
				    "remote opening PRINT");
				error->path[0] = '\0';
			}
			return out_opening_text_result(opening, false,
			    basic_error, error);
		}
	}
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
	return yt_out_local_emulated_bytes(line, length, error)
	    && yt_out_local_emulated_bytes(newline, sizeof(newline), error);
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
	struct yt_out_opening_context *opening = context;

	if (!out_opening_remote_statement(opening, line, length, false, error))
		return false;
	yt_out_remote_bytes(line, length);
	if (!out_opening_remote_statement(opening, newline, 1U, true, error))
		return false;
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
	static const uint8_t escape[] = "\x1b";
	static const uint8_t suffix[] = "[0m";
	struct yt_out_opening_context *opening = context;

	if (!out_opening_remote_statement(opening, escape,
	    sizeof(escape) - 1U, false, error))
		return false;
	yt_out_remote_bytes(escape, sizeof(escape) - 1U);
	if (!out_opening_remote_statement(opening, suffix,
	    sizeof(suffix) - 1U, false, error))
		return false;
	yt_out_remote_bytes(suffix, sizeof(suffix) - 1U);
	return true;
}

static bool
out_opening_reset_local(void *context, struct yt_error *error)
{
	static const uint8_t reset[] = "\x1b[0m";

	(void)context;
	return yt_out_local_emulated_bytes(reset, sizeof(reset) - 1U, error);
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
