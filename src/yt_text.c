#include "yt_text.h"
#include "yt_file.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static void set_error(struct yt_error *error, enum yt_status status,
    const char *operation, const char *path);
static bool text_input_open_execute(struct yt_text_input *input,
    const char *path, struct yt_error *error);
static bool text_input_close_execute(struct yt_text_input *input,
    struct yt_error *error);
static bool text_output_open_execute(struct yt_text_output *output,
    const char *path, struct yt_error *error);
static bool text_input_read_default(void *context, FILE *file,
    uint8_t *data, size_t requested,
    struct yt_text_input_read_observation *observation);
static bool text_input_get_byte(struct yt_text_input *input, uint8_t *value,
    bool *eof, struct yt_error *error);
static void text_input_read_snapshot(struct yt_text_input *input);

bool
yt_text_line_input_next(const uint8_t *data, size_t data_length,
    size_t *cursor, uint8_t *line, size_t capacity, size_t *line_length,
    bool *available)
{
	size_t start;
	size_t position;
	size_t output_length = 0U;
	size_t output_position = 0U;
	bool consumed = false;

	if (cursor == NULL || line_length == NULL || available == NULL
	    || (data == NULL && data_length != 0U) || *cursor > data_length)
		return false;
	*line_length = 0U;
	*available = false;
	start = *cursor;
	if (start == data_length || data[start] == 0x1aU)
		return true;
	position = start;
	while (position < data_length && data[position] != 0x1aU
	    && data[position] != '\r') {
		if (data[position] != 0U)
			++output_length;
		++position;
		consumed = true;
	}
	if (position < data_length && data[position] == '\r') {
		++position;
		consumed = true;
		if (position < data_length && data[position] == '\n')
			++position;
	}
	if (!consumed)
		return true;
	if (output_length > capacity
	    || (output_length != 0U && line == NULL))
		return false;
	for (size_t input = start; input < position; ++input) {
		if (data[input] == '\r')
			break;
		if (data[input] != 0U)
			line[output_position++] = data[input];
	}
	*cursor = position;
	*line_length = output_position;
	*available = true;
	return true;
}

enum yt_text_stream_line_status
yt_text_stream_line_input_next(FILE *file, uint8_t *line, size_t capacity,
    size_t *line_length)
{
	size_t used = 0U;
	bool consumed = false;
	bool overflow = false;

	if (file == NULL || line_length == NULL
	    || (line == NULL && capacity != 0U))
		return YT_TEXT_STREAM_LINE_IO_ERROR;
	*line_length = 0U;
	for (;;) {
		int value = fgetc(file);

		if (value == EOF) {
			if (ferror(file))
				return YT_TEXT_STREAM_LINE_IO_ERROR;
			break;
		}
		if ((uint8_t)value == 0x1aU) {
			if (ungetc(value, file) == EOF)
				return YT_TEXT_STREAM_LINE_IO_ERROR;
			break;
		}
		consumed = true;
		if ((uint8_t)value == '\r') {
			int following = fgetc(file);

			if (following == EOF) {
				if (ferror(file))
					return YT_TEXT_STREAM_LINE_IO_ERROR;
			}
			else if ((uint8_t)following != '\n'
			    && ungetc(following, file) == EOF)
				return YT_TEXT_STREAM_LINE_IO_ERROR;
			break;
		}
		if ((uint8_t)value == 0U)
			continue;
		if (used < capacity)
			line[used++] = (uint8_t)value;
		else
			overflow = true;
	}
	if (!consumed)
		return YT_TEXT_STREAM_LINE_EOF;
	if (overflow)
		return YT_TEXT_STREAM_LINE_TOO_LONG;
	*line_length = used;
	return YT_TEXT_STREAM_LINE_OK;
}

void
yt_text_input_init(struct yt_text_input *input)
{
	if (input != NULL)
		memset(input, 0, sizeof(*input));
}

void
yt_text_input_set_open_provider(struct yt_text_input *input,
    yt_text_open_provider provider, void *context)
{
	if (input == NULL)
		return;
	input->open_provider = provider;
	input->open_context = context;
}

void
yt_text_input_set_read_provider(struct yt_text_input *input,
    yt_text_input_read_provider provider, void *context)
{
	if (input == NULL)
		return;
	input->read_provider = provider;
	input->read_context = context;
}

void
yt_text_input_set_close_provider(struct yt_text_input *input,
    yt_text_close_provider provider, void *context)
{
	if (input == NULL)
		return;
	input->close_provider = provider;
	input->close_context = context;
}

bool
yt_text_input_open(struct yt_text_input *input, const char *path,
    struct yt_error *error)
{
	if (input != NULL && input->file == NULL
	    && input->orphaned_file == NULL) {
		memset(input->read_ahead, 0, sizeof(input->read_ahead));
		input->read_total = 0U;
		input->read_remaining = 0U;
		input->refill_index = 0U;
		input->logical_position = 0U;
		input->physical_position = 0;
		memset(&input->last_read, 0, sizeof(input->last_read));
		input->last_read.terminal_position = -1;
	}
	return text_input_open_execute(input, path, error);
}

static bool
text_input_reserve(struct yt_text_input *input, size_t needed,
    struct yt_error *error)
{
	size_t capacity;
	uint8_t *line;

	if (needed <= input->line_capacity)
		return true;
	capacity = input->line_capacity == 0U ? 128U : input->line_capacity;
	while (capacity < needed) {
		if (capacity > SIZE_MAX / 2U) {
			errno = 0;
			set_error(error, YT_NO_MEMORY, "grow text input line",
			    input->path);
			return false;
		}
		capacity *= 2U;
	}
	line = realloc(input->line, capacity);
	if (line == NULL) {
		set_error(error, YT_NO_MEMORY, "grow text input line",
		    input->path);
		return false;
	}
	input->line = line;
	input->line_capacity = capacity;
	return true;
}

static void
text_input_read_snapshot(struct yt_text_input *input)
{
	input->last_read.refill_index = input->refill_index;
	input->last_read.buffer_total = input->read_total;
	input->last_read.buffer_remaining = input->read_remaining;
	input->last_read.logical_position = input->logical_position;
	input->last_read.registered = input->file != NULL;
	input->last_read.handle_open = input->file != NULL
	    || input->orphaned_file != NULL;
}

static bool
text_input_refill(struct yt_text_input *input, struct yt_error *error)
{
	yt_text_input_read_provider provider = input->read_provider != NULL
	    ? input->read_provider : text_input_read_default;
	struct yt_text_input_read_observation observation;
	bool delivered;
	bool valid;

	memset(input->read_ahead, 0, sizeof(input->read_ahead));
	input->last_read.buffer_cleared = true;
	memset(&observation, 0, sizeof(observation));
	observation.terminal_position = -1;
	++input->last_read.operation_count;
	delivered = provider(input->read_context, input->file,
	    input->read_ahead, sizeof(input->read_ahead), &observation);
	valid = delivered
	    && observation.accepted <= sizeof(input->read_ahead)
	    && observation.terminal_position >= -1
	    && ((observation.carry
	    && observation.dos_error >= 1U
	    && observation.dos_error <= 0xffU
	    && observation.basic_error >= 1U
	    && observation.basic_error <= 0xffU)
	    || (!observation.carry && observation.dos_error == 0U
	    && observation.basic_error == 0U));
	input->last_read.accepted = observation.accepted;
	input->last_read.dos_error = observation.dos_error;
	input->last_read.basic_error = observation.basic_error;
	input->last_read.terminal_position = observation.terminal_position;
	if (!valid) {
		input->last_read.outcome = YT_TEXT_INPUT_READ_PROVIDER_ERROR;
		input->last_read.basic_error = 57U;
		text_input_read_snapshot(input);
		errno = 0;
		set_error(error, YT_IO_ERROR,
		    "sequential INPUT read provider", input->path);
		return false;
	}
	if (observation.terminal_position >= 0)
		input->physical_position = observation.terminal_position;
	if (observation.carry) {
		input->last_read.outcome = YT_TEXT_INPUT_READ_DISK_ERROR;
		text_input_read_snapshot(input);
		errno = 0;
		set_error(error, YT_IO_ERROR, "sequential INPUT read",
		    input->path);
		return false;
	}
	input->refill_index = (input->refill_index + 1U) & 0x00ffffffU;
	if (observation.accepted != 0U) {
		input->read_total = observation.accepted;
		input->read_remaining = observation.accepted;
	}
	return true;
}

static bool
text_input_get_byte(struct yt_text_input *input, uint8_t *value, bool *eof,
    struct yt_error *error)
{
	size_t index;

	*value = 0x1aU;
	*eof = false;
	if (input->read_remaining == 0U) {
		if (!text_input_refill(input, error))
			return false;
		if (input->read_remaining == 0U) {
			*eof = true;
			return true;
		}
	}
	index = input->read_total - input->read_remaining;
	*value = input->read_ahead[index];
	--input->read_remaining;
	if (*value == 0x1aU) {
		++input->read_remaining;
		*eof = true;
	}
	return true;
}

bool
yt_text_input_read_line(struct yt_text_input *input, const uint8_t **line,
    size_t *length, bool *available, struct yt_error *error)
{
	size_t used = 0U;
	uint64_t entry_position;
	bool consumed = false;

	if (line != NULL)
		*line = NULL;
	if (length != NULL)
		*length = 0U;
	if (available != NULL)
		*available = false;
	if (input == NULL || input->file == NULL || line == NULL
	    || length == NULL || available == NULL) {
		errno = 0;
		set_error(error, YT_INVALID, "LINE INPUT text", input == NULL
		    ? NULL : input->path);
		return false;
	}
	memset(&input->last_read, 0, sizeof(input->last_read));
	entry_position = input->logical_position;
	input->last_read.terminal_position = input->physical_position;
	input->last_read.registered = true;
	input->last_read.handle_open = true;
	for (;;) {
		uint8_t value;
		bool eof;

		if (!text_input_get_byte(input, &value, &eof, error)) {
			input->last_read.consumed = (size_t)(input->logical_position
			    - entry_position);
			input->last_read.returned = used;
			text_input_read_snapshot(input);
			return false;
		}
		if (eof)
			break;
		consumed = true;
		++input->logical_position;
		if (value == '\r') {
			uint8_t following;
			bool following_eof;

			if (!text_input_get_byte(input, &following,
			    &following_eof, error)) {
				input->last_read.consumed = (size_t)(input->logical_position
				    - entry_position);
				input->last_read.returned = used;
				text_input_read_snapshot(input);
				return false;
			}
			if (!following_eof) {
				if (following == '\n')
					++input->logical_position;
				else
					++input->read_remaining;
			}
			break;
		}
		if (value == 0U)
			continue;
		if (!text_input_reserve(input, used + 1U, error)) {
			input->last_read.outcome = YT_TEXT_INPUT_READ_MEMORY_ERROR;
			input->last_read.consumed = (size_t)(input->logical_position
			    - entry_position);
			input->last_read.returned = used;
			text_input_read_snapshot(input);
			return false;
		}
		input->line[used++] = value;
	}
	*line = input->line;
	*length = used;
	*available = consumed;
	input->last_read.outcome = YT_TEXT_INPUT_READ_RETURNED;
	input->last_read.consumed = (size_t)(input->logical_position
	    - entry_position);
	input->last_read.returned = used;
	input->last_read.eof = !consumed;
	text_input_read_snapshot(input);
	return true;
}

bool
yt_text_input_eof(struct yt_text_input *input, bool *eof,
    struct yt_error *error)
{
	uint8_t value;
	bool physical_eof;

	if (eof != NULL)
		*eof = false;
	if (input == NULL || input->file == NULL || eof == NULL) {
		errno = 0;
		set_error(error, YT_INVALID, "EOF text input", input == NULL
		    ? NULL : input->path);
		return false;
	}
	memset(&input->last_read, 0, sizeof(input->last_read));
	input->last_read.terminal_position = input->physical_position;
	input->last_read.eof_probe = true;
	input->last_read.registered = true;
	input->last_read.handle_open = true;
	if (!text_input_get_byte(input, &value, &physical_eof, error)) {
		text_input_read_snapshot(input);
		return false;
	}
	if (!physical_eof)
		++input->read_remaining;
	*eof = physical_eof;
	input->last_read.outcome = YT_TEXT_INPUT_READ_RETURNED;
	input->last_read.eof = physical_eof;
	text_input_read_snapshot(input);
	return true;
}

bool
yt_text_input_close(struct yt_text_input *input, struct yt_error *error)
{
	return text_input_close_execute(input, error);
}

void
yt_text_input_destroy(struct yt_text_input *input)
{
	if (input == NULL)
		return;
	if (input->file != NULL)
		(void)fclose(input->file);
	if (input->orphaned_file != NULL
	    && input->orphaned_file != input->file)
		(void)fclose(input->orphaned_file);
	free(input->line);
	memset(input, 0, sizeof(*input));
}

bool
yt_text_sequential_play_run(struct yt_text_sequential_play_state *state,
    const struct yt_text_sequential_play_ops *ops, void *context,
    struct yt_error *error)
{
	if (state == NULL || state->path == NULL || ops == NULL
	    || ops->close == NULL || ops->open == NULL || ops->read == NULL
	    || ops->present == NULL) {
		errno = 0;
		set_error(error, YT_INVALID, "sequential text playback", NULL);
		return false;
	}
	state->file_open = false;
	state->read_count = 0U;
	state->line_count = 0U;
	if (!ops->close(context, error)
	    || !ops->open(context, state->path, error))
		return false;
	state->file_open = true;
	for (;;) {
		const uint8_t *line;
		size_t length;
		bool available;

		if (!ops->read(context, &line, &length, &available, error))
			return false;
		++state->read_count;
		if (!available)
			break;
		if (!ops->present(context, line, length, error))
			return false;
		++state->line_count;
	}
	if (!ops->close(context, error))
		return false;
	state->file_open = false;
	return true;
}

static void file_viewer_classify(const uint8_t *line, size_t length,
    struct yt_file_viewer_record *record);

bool
yt_file_viewer_next(const uint8_t *data, size_t data_length,
    size_t *cursor, const char *pager_key, uint8_t *line, size_t capacity,
    struct yt_file_viewer_record *record)
{
	bool eof;
	bool stopped;

	if (cursor == NULL || pager_key == NULL || record == NULL
	    || (data == NULL && data_length != 0U) || *cursor > data_length)
		return false;
	memset(record, 0, sizeof(*record));
	record->eof_checked = true;
	eof = *cursor >= data_length
	    || (data != NULL && data[*cursor] == 0x1aU);
	record->key_checked = true;
	stopped = strcmp(pager_key, "Q") == 0;
	if (eof || stopped)
		return true;
	if (!yt_text_line_input_next(data, data_length, cursor, line, capacity,
	    &record->length, &record->available))
		return false;
	if (!record->available)
		return true;
	file_viewer_classify(line, record->length, record);
	return true;
}

bool
yt_file_viewer_entry(char *pager_key, float *line_count,
    yt_file_viewer_present_fn present, void *context,
    struct yt_error *error)
{
	static const uint8_t notice[] = "Cntl-X to Stop";

	if (pager_key == NULL || line_count == NULL || present == NULL)
		return false;
	pager_key[0] = '\0';
	if (!present(context, notice, sizeof(notice) - 1U, true, error)
	    || !present(context, NULL, 0U, false, error))
		return false;
	return true;
}

bool
yt_file_viewer_play(const uint8_t *data, size_t data_length,
    struct yt_file_viewer_play_state *state,
    yt_file_viewer_present_fn present, void *context,
    struct yt_error *error)
{
	uint8_t *line;
	size_t cursor = 0U;
	bool ok = true;

	if (state == NULL || state->foreground == NULL
	    || state->pager_foreground == NULL || state->bold == NULL
	    || state->line_count == NULL || state->pager_key == NULL
	    || present == NULL || (data == NULL && data_length != 0U))
		return false;
	line = malloc(data_length == 0U ? 1U : data_length);
	if (line == NULL) {
		if (error != NULL)
			error->status = YT_NO_MEMORY;
		return false;
	}
	for (;;) {
		struct yt_file_viewer_record record;

		if (!yt_file_viewer_next(data, data_length, &cursor,
		    state->pager_key, line, data_length, &record)) {
			ok = false;
			break;
		}
		if (!record.available)
			break;
		*state->foreground = (float)record.foreground;
		*state->pager_foreground = record.foreground;
		if (record.set_bold)
			*state->bold = 1.0f;
		if (!present(context, line, record.length, true, error)) {
			ok = false;
			break;
		}
	}
	free(line);
	if (!ok)
		return false;
	*state->line_count = 0.0f;
	*state->foreground = state->saved_foreground;
	*state->pager_foreground = state->saved_pager_foreground;
	return present(context, NULL, 0U, false, error);
}

static void
file_viewer_classify(const uint8_t *line, size_t length,
    struct yt_file_viewer_record *record)
{
	record->available = true;
	record->length = length;
	record->foreground = 2;
	if (length >= 4U && memcmp(line, "  - ", 4U) == 0)
		record->foreground = 3;
	else if (length >= 4U && memcmp(line, " ***", 4U) == 0)
		record->foreground = 4;
	else if (length >= 4U && memcmp(line, " +++", 4U) == 0)
		record->foreground = 1;
	else if (length >= 3U && memcmp(line, "-=*", 3U) == 0)
		record->foreground = 7;
	record->set_bold = record->foreground != 2;
}

bool
yt_file_viewer_stream_run(struct yt_file_viewer_stream_state *state,
    const struct yt_file_viewer_stream_ops *ops, void *context,
    struct yt_error *error)
{
	struct yt_file_viewer_play_state *play;

	if (state == NULL || state->path == NULL || ops == NULL
	    || ops->close == NULL || ops->open == NULL || ops->eof == NULL
	    || ops->read == NULL || ops->present == NULL) {
		errno = 0;
		set_error(error, YT_INVALID, "stream file viewer", NULL);
		return false;
	}
	play = &state->play;
	if (play->foreground == NULL || play->pager_foreground == NULL
	    || play->bold == NULL || play->line_count == NULL
	    || play->pager_key == NULL) {
		errno = 0;
		set_error(error, YT_INVALID, "stream file viewer", state->path);
		return false;
	}
	state->file_open = false;
	state->eof_checks = 0U;
	state->key_checks = 0U;
	state->read_count = 0U;
	state->line_count = 0U;
	if (!ops->close(context, error))
		return false;
	*play->line_count = 0.0f;
	if (!ops->open(context, state->path, error))
		return false;
	state->file_open = true;
	for (;;) {
		const uint8_t *line;
		size_t length;
		bool available;
		bool eof;
		bool stopped;
		struct yt_file_viewer_record record;

		if (!ops->eof(context, &eof, error))
			return false;
		++state->eof_checks;
		stopped = strcmp(play->pager_key, "Q") == 0;
		++state->key_checks;
		if (eof || stopped)
			break;
		if (!ops->read(context, &line, &length, &available, error))
			return false;
		++state->read_count;
		if (!available) {
			errno = 0;
			set_error(error, YT_EOF, "LINE INPUT after EOF check",
			    state->path);
			return false;
		}
		file_viewer_classify(line, length, &record);
		*play->foreground = (float)record.foreground;
		*play->pager_foreground = record.foreground;
		if (record.set_bold)
			*play->bold = 1.0f;
		if (!ops->present(context, line, length, true, error))
			return false;
		++state->line_count;
	}
	if (!ops->close(context, error))
		return false;
	state->file_open = false;
	*play->line_count = 0.0f;
	*play->foreground = play->saved_foreground;
	*play->pager_foreground = play->saved_pager_foreground;
	return ops->present(context, NULL, 0U, false, error);
}

bool
yt_file_viewer_missing(const uint8_t *path, size_t path_length,
    yt_file_viewer_present_fn present, yt_file_viewer_news_fn append_news,
    void *context, struct yt_error *error)
{
	static const uint8_t prefix[] = "*** GAME FILE [";
	static const uint8_t suffix[] = "] NOT FOUND! ***";
	uint8_t *row;
	size_t length;

	if ((path == NULL && path_length != 0U) || present == NULL
	    || append_news == NULL
	    || path_length > SIZE_MAX - (sizeof(prefix) - 1U)
	    || path_length + sizeof(prefix) - 1U
	    > SIZE_MAX - (sizeof(suffix) - 1U))
		return false;
	length = sizeof(prefix) - 1U + path_length + sizeof(suffix) - 1U;
	row = malloc(length == 0U ? 1U : length);
	if (row == NULL) {
		if (error != NULL)
			error->status = YT_NO_MEMORY;
		return false;
	}
	memcpy(row, prefix, sizeof(prefix) - 1U);
	if (path_length != 0U)
		memcpy(row + sizeof(prefix) - 1U, path, path_length);
	memcpy(row + sizeof(prefix) - 1U + path_length, suffix,
	    sizeof(suffix) - 1U);
	if (!present(context, row, length, true, error)
	    || !append_news(context, row, length, error)) {
		free(row);
		return false;
	}
	free(row);
	return true;
}

bool
yt_opening_stream_run(struct yt_opening_stream_state *state,
    const struct yt_opening_stream_ops *ops, void *context,
    struct yt_error *error)
{
	if (state == NULL || state->path == NULL || ops == NULL
	    || ops->open_input == NULL || ops->open_local == NULL
	    || ops->eof == NULL || ops->read == NULL
	    || ops->present_local == NULL || ops->poll_local == NULL
	    || ops->present_remote == NULL || ops->poll_remote == NULL
	    || ops->wait == NULL || ops->reset_remote == NULL
	    || ops->reset_local == NULL || ops->close_input == NULL
	    || ops->close_local == NULL) {
		errno = 0;
		set_error(error, YT_INVALID, "ANSI opening stream", NULL);
		return false;
	}
	state->exit_reason = YT_OPENING_EXIT_EOF;
	state->input_open = false;
	state->local_open = false;
	state->waited = false;
	state->remote_reset = false;
	state->local_reset = false;
	state->eof_checks = 0U;
	state->read_count = 0U;
	state->local_lines = 0U;
	state->local_polls = 0U;
	state->remote_lines = 0U;
	state->remote_polls = 0U;
	if (!ops->open_input(context, state->path, error))
		return false;
	state->input_open = true;
	if (!ops->open_local(context, error))
		return false;
	state->local_open = true;
	for (;;) {
		const uint8_t *line;
		size_t length;
		bool available;
		bool eof;
		bool ready;

		if (!ops->eof(context, &eof, error))
			return false;
		++state->eof_checks;
		if (eof) {
			if (!ops->wait(context, 3.0f, error))
				return false;
			state->waited = true;
			break;
		}
		if (!ops->read(context, &line, &length, &available, error))
			return false;
		++state->read_count;
		if (!available) {
			errno = 0;
			set_error(error, YT_EOF,
			    "ANSI LINE INPUT after EOF check", state->path);
			return false;
		}
		if (state->snoop != 0.0f) {
			if (!ops->present_local(context, line, length, error))
				return false;
			++state->local_lines;
		}
		if (!ops->poll_local(context, &ready, error))
			return false;
		++state->local_polls;
		if (ready) {
			state->exit_reason = YT_OPENING_EXIT_LOCAL_KEY;
			break;
		}
		if (state->mode != 1.0f) {
			if (!ops->present_remote(context, line, length, error))
				return false;
			++state->remote_lines;
			if (!ops->poll_remote(context, &ready, error))
				return false;
			++state->remote_polls;
			if (ready) {
				state->exit_reason =
				    YT_OPENING_EXIT_REMOTE_PENDING;
				break;
			}
		}
	}
	if (state->mode == 0.0f) {
		if (!ops->reset_remote(context, error))
			return false;
		state->remote_reset = true;
	}
	if (state->snoop != 0.0f) {
		if (!ops->reset_local(context, error))
			return false;
		state->local_reset = true;
	}
	if (!ops->close_input(context, error))
		return false;
	state->input_open = false;
	if (!ops->close_local(context, error))
		return false;
	state->local_open = false;
	return true;
}

#ifdef _WIN32
#include <io.h>
#define yt_text_fileno _fileno
#define yt_text_fseeko _fseeki64
#define yt_text_ftello _ftelli64
#define yt_text_isatty _isatty
#define yt_text_stat_is_dir(mode) (((mode) & _S_IFMT) == _S_IFDIR)
#else
#include <unistd.h>
#define yt_text_fileno fileno
#define yt_text_fseeko fseeko
#define yt_text_ftello ftello
#define yt_text_isatty isatty
#define yt_text_stat_is_dir(mode) S_ISDIR(mode)
#endif

static void
set_error(struct yt_error *error, enum yt_status status, const char *operation,
    const char *path)
{
	if (error == NULL)
		return;
	error->status = status;
	error->system_error = errno;
	snprintf(error->operation, sizeof(error->operation), "%s", operation);
	snprintf(error->path, sizeof(error->path), "%s", path != NULL ? path : "");
}

static bool
text_output_parent_exists(const char *path)
{
	const char *slash;
	char directory[512];
	struct stat info;
	size_t length;

	if (path == NULL)
		return true;
	slash = strrchr(path, '/');
#ifdef _WIN32
	{
		const char *backslash = strrchr(path, '\\');

		if (backslash != NULL && (slash == NULL || backslash > slash))
			slash = backslash;
	}
#endif
	if (slash == NULL)
		return true;
	length = (size_t)(slash - path);
	if (length == 0U)
		length = 1U;
#ifdef _WIN32
	if (length == 2U && path[1] == ':')
		++length;
#endif
	if (length >= sizeof(directory))
		return false;
	memcpy(directory, path, length);
	directory[length] = '\0';
	return stat(directory, &info) == 0
	    && yt_text_stat_is_dir(info.st_mode);
}

static uint16_t
text_output_dos_error(const char *path, int system_error)
{
	switch (system_error) {
	case ENOENT:
		return text_output_parent_exists(path) ? 2U : 3U;
	case ENOTDIR:
		return 3U;
#if defined(ENFILE) && ENFILE != EMFILE
	case ENFILE:
#endif
	case EMFILE:
		return 4U;
	case EACCES:
#if defined(EPERM) && EPERM != EACCES
	case EPERM:
#endif
#if defined(EROFS) && EROFS != EACCES && EROFS != EPERM
	case EROFS:
#endif
#if defined(EISDIR) && EISDIR != EACCES && EISDIR != EPERM \
    && EISDIR != EROFS
	case EISDIR:
#endif
		return 5U;
	case EBADF:
		return 6U;
	case ENOMEM:
		return 8U;
#ifdef EEXIST
	case EEXIST:
		return 80U;
#endif
	default:
		return 1U;
	}
}

static int64_t
text_output_position(FILE *file)
{
	int64_t position;

	if (file == NULL)
		return -1;
	position = (int64_t)yt_text_ftello(file);
	return position >= 0 ? position : -1;
}

static bool
text_input_read_default(void *context, FILE *file, uint8_t *data,
    size_t requested, struct yt_text_input_read_observation *observation)
{
	int saved_errno;

	(void)context;
	if (file == NULL || data == NULL || requested == 0U
	    || observation == NULL)
		return false;
	memset(observation, 0, sizeof(*observation));
	errno = 0;
	observation->accepted = fread(data, 1U, requested, file);
	saved_errno = errno;
	observation->carry = ferror(file) != 0;
	observation->dos_error = observation->carry
	    ? text_output_dos_error(NULL, saved_errno) : 0U;
	observation->basic_error = observation->carry ? 57U : 0U;
	observation->terminal_position = text_output_position(file);
	errno = saved_errno;
	return true;
}

static bool
text_output_write_default_common(FILE *file, const uint8_t *data,
    size_t requested, struct yt_text_output_write_observation *observation)
{
	int saved_errno;

	if (file == NULL || (data == NULL && requested != 0U))
		return false;
	memset(observation, 0, sizeof(*observation));
	errno = 0;
	observation->accepted = fwrite(data, 1U, requested, file);
	saved_errno = errno;
	observation->carry = ferror(file) != 0;
	observation->handle_open = true;
	observation->dos_error = observation->carry
	    ? text_output_dos_error(NULL, saved_errno) : 0U;
	observation->terminal_position = text_output_position(file);
	errno = saved_errno;
	return true;
}

static bool
text_output_write_default(void *context, FILE *file, const uint8_t *data,
    size_t requested, struct yt_text_output_write_observation *observation)
{
	bool delivered;

	(void)context;
	delivered = text_output_write_default_common(file, data, requested,
	    observation);
	if (delivered && observation->carry) {
		observation->accepted = 0U;
		observation->terminal_position = -1;
	}
	return delivered;
}

static bool
text_close_default(void *context, FILE *file,
    enum yt_text_close_operation operation, const uint8_t *data,
    size_t requested, struct yt_text_close_observation *observation)
{
	struct yt_text_output_write_observation write;
	int result;
	int saved_errno;

	(void)context;
	memset(observation, 0, sizeof(*observation));
	observation->terminal_position = text_output_position(file);
	switch (operation) {
	case YT_TEXT_CLOSE_PENDING_WRITE:
	case YT_TEXT_CLOSE_EOF_WRITE:
		if (!text_output_write_default_common(file, data, requested,
		    &write))
			return false;
		observation->accepted = write.accepted;
		observation->carry = write.carry;
		observation->handle_open = write.handle_open;
		observation->dos_error = write.dos_error;
		observation->terminal_position = write.terminal_position;
		return true;
	case YT_TEXT_CLOSE_TRUNCATE:
		if (file == NULL || data != NULL || requested != 0U)
			return false;
		errno = 0;
		observation->terminal_position = text_output_position(file);
		result = observation->terminal_position < 0 || fflush(file) != 0;
#ifdef _WIN32
		if (!result)
			result = _chsize_s(yt_text_fileno(file),
			    observation->terminal_position) != 0;
#else
		if (!result)
			result = ftruncate(yt_text_fileno(file),
			    (off_t)observation->terminal_position) != 0;
#endif
		saved_errno = errno;
		observation->carry = result != 0;
		observation->handle_open = true;
		observation->dos_error = result != 0
		    ? text_output_dos_error(NULL, saved_errno) : 0U;
		errno = saved_errno;
		return true;
	case YT_TEXT_CLOSE_HANDLE:
	case YT_TEXT_CLOSE_CLEANUP_HANDLE:
		if (data != NULL || requested != 0U)
			return false;
		if (file == NULL) {
			observation->carry = true;
			observation->dos_error = 6U;
			return true;
		}
		errno = 0;
		observation->terminal_position = text_output_position(file);
		result = fclose(file);
		saved_errno = errno;
		observation->carry = result != 0;
		observation->dos_error = result != 0
		    ? text_output_dos_error(NULL, saved_errno) : 0U;
		observation->handle_open = false;
		errno = saved_errno;
		return true;
	default:
		return false;
	}
}

static bool
text_close_observation_valid(
    enum yt_text_close_operation operation, size_t requested,
    const struct yt_text_close_observation *observation)
{
	bool write = operation == YT_TEXT_CLOSE_PENDING_WRITE
	    || operation == YT_TEXT_CLOSE_EOF_WRITE;
	bool handle = operation == YT_TEXT_CLOSE_HANDLE
	    || operation == YT_TEXT_CLOSE_CLEANUP_HANDLE;

	if (observation->accepted > requested
	    || observation->terminal_position < -1)
		return false;
	if (!write && observation->accepted != 0U)
		return false;
	if (observation->carry) {
		if (observation->dos_error < 1U
		    || observation->dos_error > 0xffU)
			return false;
		return handle || observation->handle_open;
	}
	if (observation->dos_error != 0U)
		return false;
	if (handle)
		return !observation->handle_open;
	return observation->handle_open;
}

static bool
text_input_close_observe(struct yt_text_input *input,
    yt_text_close_provider provider, FILE *file,
    enum yt_text_close_operation operation,
    struct yt_text_close_observation *observation)
{
	++input->last_close.operation_count;
	memset(observation, 0, sizeof(*observation));
	observation->terminal_position = -1;
	return provider(input->close_context, file, operation, NULL, 0U,
	    observation);
}

static void
text_input_close_failure(struct yt_text_input *input,
    enum yt_text_close_outcome outcome,
    enum yt_text_close_operation operation, uint16_t basic_error,
    uint16_t dos_error, const struct yt_text_close_observation *observed,
    struct yt_error *error)
{
	input->last_close.outcome = outcome;
	input->last_close.failed_operation = operation;
	input->last_close.basic_error = basic_error;
	input->last_close.dos_error = dos_error;
	if (observed != NULL) {
		input->last_close.accepted = observed->accepted;
		input->last_close.terminal_position =
		    observed->terminal_position;
	}
	input->last_close.registered = input->file != NULL;
	input->last_close.handle_open = input->file != NULL
	    || input->orphaned_file != NULL;
	set_error(error, YT_IO_ERROR, "sequential INPUT CLOSE", input->path);
}

static bool
text_input_close_execute(struct yt_text_input *input, struct yt_error *error)
{
	yt_text_close_provider provider;
	struct yt_text_close_observation observation;
	struct yt_text_close_observation cleanup;
	FILE *file;
	uint16_t first_dos_error;
	bool delivered;
	bool valid;
	bool first_handle_open;

	if (input == NULL) {
		errno = 0;
		set_error(error, YT_INVALID, "sequential INPUT CLOSE", NULL);
		return false;
	}
	memset(&input->last_close, 0, sizeof(input->last_close));
	input->last_close.failed_operation = YT_TEXT_CLOSE_HANDLE;
	input->last_close.terminal_position = -1;
	input->last_close.device = input->last_open.device;
	if (input->file == NULL) {
		input->last_close.outcome = YT_TEXT_CLOSE_RETURNED;
		input->last_close.missing = true;
		input->last_close.handle_open = input->orphaned_file != NULL;
		return true;
	}
	file = input->file;
	provider = input->close_provider != NULL ? input->close_provider
	    : text_close_default;
	delivered = text_input_close_observe(input, provider, file,
	    YT_TEXT_CLOSE_HANDLE, &observation);
	valid = delivered && text_close_observation_valid(
	    YT_TEXT_CLOSE_HANDLE, 0U, &observation);
	if (!valid) {
		if (delivered && !observation.handle_open)
			input->file = NULL;
		text_input_close_failure(input, YT_TEXT_CLOSE_PROVIDER_ERROR,
		    YT_TEXT_CLOSE_HANDLE, 57U, observation.dos_error,
		    delivered ? &observation : NULL, error);
		return false;
	}
	if (!observation.carry) {
		input->file = NULL;
		input->last_close.outcome = YT_TEXT_CLOSE_RETURNED;
		input->last_close.terminal_position =
		    observation.terminal_position;
		input->last_close.registered = false;
		input->last_close.handle_open = input->orphaned_file != NULL;
		return true;
	}
	first_dos_error = observation.dos_error;
	first_handle_open = observation.handle_open;
	input->file = NULL;
	input->last_close.cleanup_close_attempted = true;
	delivered = text_input_close_observe(input, provider,
	    first_handle_open ? file : NULL,
	    YT_TEXT_CLOSE_CLEANUP_HANDLE, &cleanup);
	valid = delivered && text_close_observation_valid(
	    YT_TEXT_CLOSE_CLEANUP_HANDLE, 0U, &cleanup);
	if ((!valid && first_handle_open) || (valid && cleanup.handle_open))
		input->orphaned_file = file;
	text_input_close_failure(input, valid ? YT_TEXT_CLOSE_DISK_ERROR
	    : YT_TEXT_CLOSE_PROVIDER_ERROR, YT_TEXT_CLOSE_HANDLE,
	    valid ? (input->last_close.device ? 57U : 70U) : 57U,
	    first_dos_error, &observation, error);
	return false;
}

void
yt_text_output_init(struct yt_text_output *output)
{
	if (output != NULL)
		memset(output, 0, sizeof(*output));
}

bool
yt_text_output_open(struct yt_text_output *output, const char *path,
    struct yt_error *error)
{
	return text_output_open_execute(output, path, error);
}

static bool
text_open_default(void *context, const char *path,
    enum yt_text_open_operation operation, uint8_t access,
    FILE *active_file, int64_t offset, uint8_t *data, size_t requested,
    uint16_t prior_dos_error,
    struct yt_text_open_observation *observation)
{
	int64_t position;
	int result;
	int saved_errno;

	(void)context;
	(void)prior_dos_error;
	memset(observation, 0, sizeof(*observation));
	observation->terminal_position = -1;
	switch (operation) {
	case YT_TEXT_OPEN_EXISTING:
	case YT_TEXT_OPEN_REOPEN:
	case YT_TEXT_OPEN_CREATE:
		errno = 0;
		observation->file = fopen(path,
		    operation == YT_TEXT_OPEN_CREATE ? "w+b"
		    : access == 0U ? "rb" : "r+b");
		if (observation->file == NULL) {
			observation->carry = true;
			observation->dos_error = text_output_dos_error(path, errno);
		}
		else {
			observation->terminal_position = 0;
			observation->handle_open = true;
		}
		return true;
	case YT_TEXT_OPEN_TEMP_CLOSE:
		if (active_file == NULL) {
			observation->carry = true;
			observation->dos_error = 6U;
			return true;
		}
		errno = 0;
		result = fclose(active_file);
		saved_errno = errno;
		observation->carry = result != 0;
		observation->dos_error = result != 0
		    ? text_output_dos_error(NULL, saved_errno) : 0U;
		observation->handle_open = false;
		errno = saved_errno;
		return true;
	case YT_TEXT_OPEN_EXTENDED_ERROR:
		observation->mapped_error = 75U;
		return true;
	case YT_TEXT_OPEN_QUERY_DEVICE:
		if (active_file == NULL)
			return false;
		observation->device = yt_text_isatty(
		    yt_text_fileno(active_file)) != 0;
		observation->handle_open = true;
		return true;
	case YT_TEXT_OPEN_CONFIGURE_DEVICE:
		if (active_file == NULL)
			return false;
		observation->handle_open = true;
		return true;
	case YT_TEXT_OPEN_SEEK_END:
	case YT_TEXT_OPEN_SEEK_WINDOW:
	case YT_TEXT_OPEN_SEEK_SELECTED:
		if (active_file == NULL)
			return false;
		errno = 0;
		result = yt_text_fseeko(active_file,
		    operation == YT_TEXT_OPEN_SEEK_END ? 0 : offset,
		    operation == YT_TEXT_OPEN_SEEK_END
		    ? SEEK_END : SEEK_SET);
		saved_errno = errno;
		position = (int64_t)yt_text_ftello(active_file);
		observation->carry = result != 0 || position < 0;
		observation->dos_error = observation->carry
		    ? text_output_dos_error(NULL, saved_errno) : 0U;
		observation->terminal_position = position >= 0 ? position : -1;
		observation->handle_open = true;
		errno = saved_errno;
		return true;
	case YT_TEXT_OPEN_READ_WINDOW:
		if (active_file == NULL || (data == NULL && requested != 0U))
			return false;
		errno = 0;
		observation->accepted = fread(data, 1U, requested, active_file);
		saved_errno = errno;
		position = (int64_t)yt_text_ftello(active_file);
		observation->carry = ferror(active_file) != 0;
		observation->dos_error = observation->carry
		    ? text_output_dos_error(NULL, saved_errno) : 0U;
		observation->mapped_error = observation->carry
		    && observation->dos_error == 5U ? 75U : 0U;
		observation->terminal_position = position >= 0 ? position : -1;
		observation->handle_open = true;
		errno = saved_errno;
		return true;
	default:
		return false;
	}
}

static void
text_append_open_failure(struct yt_text_output *output,
    enum yt_text_open_outcome outcome,
    enum yt_text_open_operation operation, uint16_t basic_error,
    uint16_t dos_error, struct yt_error *error)
{
	output->last_append_open.outcome = outcome;
	output->last_append_open.failed_operation = operation;
	output->last_append_open.basic_error = basic_error;
	output->last_append_open.dos_error = dos_error;
	output->last_append_open.registered = output->file != NULL;
	output->last_append_open.handle_open = output->file != NULL
	    || output->orphaned_file != NULL;
	set_error(error, basic_error == 53U || basic_error == 76U
	    ? YT_NOT_FOUND : YT_IO_ERROR,
	    "sequential APPEND OPEN", output->path);
}

static bool
text_append_observe(struct yt_text_output *output, const char *path,
    enum yt_text_open_operation operation, uint8_t access,
    FILE *active_file, int64_t offset, uint8_t *data, size_t requested,
    uint16_t prior_dos_error,
    struct yt_text_open_observation *observation)
{
	yt_text_open_provider provider = output->open_provider != NULL
	    ? output->open_provider : text_open_default;
	bool delivered;

	++output->last_append_open.operation_count;
	memset(observation, 0, sizeof(*observation));
	observation->terminal_position = -1;
	delivered = provider(output->open_context, path, operation, access,
	    active_file, offset, data, requested, prior_dos_error, observation);
	output->last_append_open.accepted = observation->accepted;
	if (observation->terminal_position >= 0)
		output->last_append_open.terminal_position =
		    observation->terminal_position;
	return delivered;
}

static bool
text_open_observation_valid(enum yt_text_open_operation operation,
    size_t requested, const struct yt_text_open_observation *observation)
{
	bool opening = operation == YT_TEXT_OPEN_EXISTING
	    || operation == YT_TEXT_OPEN_CREATE
	    || operation == YT_TEXT_OPEN_REOPEN;

	if (observation->terminal_position < -1
	    || observation->accepted > requested
	    || observation->dos_error > 0xffU)
		return false;
	if (opening)
		return observation->accepted == 0U
		    && observation->mapped_error == 0U
		    && (observation->carry
		    ? observation->file == NULL && !observation->handle_open
		    && observation->dos_error >= 1U
		    && observation->terminal_position == -1
		    : observation->file != NULL && observation->handle_open
		    && observation->dos_error == 0U
		    && observation->terminal_position == 0);
	if (operation == YT_TEXT_OPEN_TEMP_CLOSE)
		return observation->file == NULL && observation->accepted == 0U
		    && observation->terminal_position == -1
		    && observation->mapped_error == 0U
		    && (observation->carry ? observation->dos_error >= 1U
		    : observation->dos_error == 0U && !observation->handle_open);
	if (operation == YT_TEXT_OPEN_EXTENDED_ERROR)
		return observation->file == NULL && observation->accepted == 0U
		    && observation->terminal_position == -1
		    && !observation->carry && !observation->handle_open
		    && observation->dos_error == 0U
		    && (observation->mapped_error == 70U
		    || observation->mapped_error == 75U);
	if (operation == YT_TEXT_OPEN_QUERY_DEVICE)
		return observation->file == NULL && observation->handle_open
		    && observation->accepted == 0U
		    && observation->terminal_position == -1
		    && observation->mapped_error == 0U
		    && (observation->carry ? observation->dos_error >= 1U
		    : observation->dos_error == 0U);
	if (operation == YT_TEXT_OPEN_CONFIGURE_DEVICE)
		return observation->file == NULL && observation->handle_open
		    && observation->accepted == 0U
		    && observation->terminal_position == -1
		    && observation->mapped_error == 0U
		    && (observation->carry ? observation->dos_error >= 1U
		    : observation->dos_error == 0U);
	if (!observation->handle_open || observation->file != NULL)
		return false;
	if (operation == YT_TEXT_OPEN_READ_WINDOW) {
		if (observation->carry)
			return observation->dos_error >= 1U
			    && (observation->dos_error == 5U
			    ? observation->mapped_error == 70U
			    || observation->mapped_error == 75U
			    : observation->mapped_error == 0U
			    || observation->mapped_error == 57U);
		return observation->dos_error == 0U
		    && observation->mapped_error == 0U
		    && observation->terminal_position >= 0;
	}
	if (observation->accepted != 0U || observation->mapped_error != 0U)
		return false;
	if (observation->carry)
		return observation->dos_error >= 1U;
	return observation->dos_error == 0U
	    && observation->terminal_position >= 0;
}

static void
text_input_open_failure(struct yt_text_input *input,
    enum yt_text_open_outcome outcome,
    enum yt_text_open_operation operation, uint16_t basic_error,
    uint16_t dos_error, struct yt_error *error)
{
	input->last_open.outcome = outcome;
	input->last_open.failed_operation = operation;
	input->last_open.basic_error = basic_error;
	input->last_open.dos_error = dos_error;
	input->last_open.registered = input->file != NULL;
	input->last_open.handle_open = input->file != NULL
	    || input->orphaned_file != NULL;
	set_error(error, basic_error == 53U || basic_error == 76U
	    ? YT_NOT_FOUND : YT_IO_ERROR, "sequential INPUT OPEN", input->path);
}

static bool
text_input_open_observe(struct yt_text_input *input, const char *path,
    enum yt_text_open_operation operation, FILE *active_file,
    uint16_t prior_dos_error,
    struct yt_text_open_observation *observation)
{
	yt_text_open_provider provider = input->open_provider != NULL
	    ? input->open_provider : text_open_default;
	bool delivered;

	++input->last_open.operation_count;
	memset(observation, 0, sizeof(*observation));
	observation->terminal_position = -1;
	delivered = provider(input->open_context, path, operation, 0U,
	    active_file, 0, NULL, 0U, prior_dos_error, observation);
	input->last_open.accepted = observation->accepted;
	if (observation->terminal_position >= 0)
		input->last_open.terminal_position =
		    observation->terminal_position;
	return delivered;
}

static bool
text_input_open_execute(struct yt_text_input *input, const char *path,
    struct yt_error *error)
{
	char resolved[512];
	struct yt_text_open_observation observation;
	bool delivered;
	bool valid;

	if (input == NULL || path == NULL || input->file != NULL
	    || input->orphaned_file != NULL) {
		errno = 0;
		set_error(error, YT_INVALID, "open text input", path);
		return false;
	}
	memset(&input->last_open, 0, sizeof(input->last_open));
	input->last_open.failed_operation = YT_TEXT_OPEN_EXISTING;
	input->last_open.terminal_position = -1;
	if (!yt_resolve_case_path(path, true, resolved, sizeof(resolved), error))
		return false;
	(void)snprintf(input->path, sizeof(input->path), "%s", resolved);
	input->last_open.access_attempts[
	    input->last_open.access_attempt_count++] = 0U;
	delivered = text_input_open_observe(input, resolved,
	    YT_TEXT_OPEN_EXISTING, NULL, 0U, &observation);
	valid = delivered && text_open_observation_valid(
	    YT_TEXT_OPEN_EXISTING, 0U, &observation);
	if (!valid) {
		if (observation.file != NULL && observation.handle_open)
			input->orphaned_file = observation.file;
		text_input_open_failure(input, YT_TEXT_OPEN_PROVIDER_ERROR,
		    YT_TEXT_OPEN_EXISTING, 53U, observation.dos_error, error);
		return false;
	}
	if (observation.carry) {
		uint16_t dos_error = observation.dos_error;

		if (dos_error == 5U) {
			delivered = text_input_open_observe(input, resolved,
			    YT_TEXT_OPEN_EXTENDED_ERROR, NULL, dos_error,
			    &observation);
			valid = delivered && text_open_observation_valid(
			    YT_TEXT_OPEN_EXTENDED_ERROR, 0U, &observation);
			if (!valid) {
				text_input_open_failure(input,
				    YT_TEXT_OPEN_PROVIDER_ERROR,
				    YT_TEXT_OPEN_EXTENDED_ERROR, 53U,
				    dos_error, error);
				return false;
			}
			text_input_open_failure(input,
			    YT_TEXT_OPEN_INITIAL_ERROR,
			    YT_TEXT_OPEN_EXTENDED_ERROR,
			    observation.mapped_error, dos_error, error);
			return false;
		}
		text_input_open_failure(input, YT_TEXT_OPEN_INITIAL_ERROR,
		    YT_TEXT_OPEN_EXISTING, dos_error == 3U ? 76U : 53U,
		    dos_error, error);
		return false;
	}
	input->file = observation.file;
	delivered = text_input_open_observe(input, resolved,
	    YT_TEXT_OPEN_QUERY_DEVICE, input->file, 0U, &observation);
	valid = delivered && text_open_observation_valid(
	    YT_TEXT_OPEN_QUERY_DEVICE, 0U, &observation);
	if (!valid) {
		text_input_open_failure(input, YT_TEXT_OPEN_PROVIDER_ERROR,
		    YT_TEXT_OPEN_QUERY_DEVICE, 57U, observation.dos_error, error);
		return false;
	}
	input->last_open.device = observation.device;
	if (observation.device) {
		delivered = text_input_open_observe(input, resolved,
		    YT_TEXT_OPEN_CONFIGURE_DEVICE, input->file, 0U,
		    &observation);
		valid = delivered && text_open_observation_valid(
		    YT_TEXT_OPEN_CONFIGURE_DEVICE, 0U, &observation);
		if (!valid) {
			text_input_open_failure(input,
			    YT_TEXT_OPEN_PROVIDER_ERROR,
			    YT_TEXT_OPEN_CONFIGURE_DEVICE, 57U,
			    observation.dos_error, error);
			return false;
		}
		if (observation.carry) {
			text_input_open_failure(input,
			    YT_TEXT_OPEN_DEVICE_ERROR,
			    YT_TEXT_OPEN_CONFIGURE_DEVICE, 57U,
			    observation.dos_error, error);
			return false;
		}
	}
	input->last_open.outcome = YT_TEXT_OPEN_RETURNED;
	input->last_open.registered = true;
	input->last_open.handle_open = true;
	return true;
}

static bool
text_output_open_observe(struct yt_text_output *output, const char *path,
    enum yt_text_open_operation operation, uint8_t access,
    FILE *active_file, uint16_t prior_dos_error,
    struct yt_text_open_observation *observation)
{
	yt_text_open_provider provider = output->open_provider != NULL
	    ? output->open_provider : text_open_default;
	bool delivered;

	++output->last_output_open.operation_count;
	memset(observation, 0, sizeof(*observation));
	observation->terminal_position = -1;
	delivered = provider(output->open_context, path, operation, access,
	    active_file, 0, NULL, 0U, prior_dos_error, observation);
	output->last_output_open.accepted = observation->accepted;
	if (observation->terminal_position >= 0)
		output->last_output_open.terminal_position =
		    observation->terminal_position;
	return delivered;
}

static void
text_output_open_failure(struct yt_text_output *output,
    enum yt_text_open_outcome outcome,
    enum yt_text_open_operation operation, uint16_t basic_error,
    uint16_t dos_error, struct yt_error *error)
{
	output->last_output_open.outcome = outcome;
	output->last_output_open.failed_operation = operation;
	output->last_output_open.basic_error = basic_error;
	output->last_output_open.dos_error = dos_error;
	output->last_output_open.registered = output->file != NULL;
	output->last_output_open.handle_open = output->file != NULL
	    || output->orphaned_file != NULL;
	set_error(error, basic_error == 53U || basic_error == 76U
	    ? YT_NOT_FOUND : YT_IO_ERROR, "sequential OUTPUT OPEN", output->path);
}

static bool
text_output_open_execute(struct yt_text_output *output, const char *path,
    struct yt_error *error)
{
	char resolved[512];
	struct yt_text_open_observation observation;
	FILE *temporary = NULL;
	uint16_t first_close_error;
	enum yt_text_open_operation open_operation;
	bool delivered;
	bool valid;
	bool reopening = false;
	bool temporary_open = false;

	if (output == NULL || path == NULL || output->file != NULL
	    || output->orphaned_file != NULL) {
		errno = 0;
		set_error(error, YT_INVALID, "open text output", path);
		return false;
	}
	memset(&output->last_output_open, 0,
	    sizeof(output->last_output_open));
	output->last_output_open.failed_operation =
	    YT_TEXT_OPEN_EXISTING;
	output->last_output_open.terminal_position = -1;
	memset(&output->last_append_open, 0,
	    sizeof(output->last_append_open));
	if (!yt_resolve_case_path(path, true, resolved, sizeof(resolved), error))
		return false;
	(void)snprintf(output->path, sizeof(output->path), "%s", resolved);

open_attempt:
	if (output->last_output_open.access_attempt_count
	    >= sizeof(output->last_output_open.access_attempts)) {
		text_output_open_failure(output,
		    YT_TEXT_OPEN_PROVIDER_ERROR,
		    reopening ? YT_TEXT_OPEN_REOPEN
		    : YT_TEXT_OPEN_EXISTING, 67U, 0U, error);
		return false;
	}
	output->last_output_open.access_attempts[
	    output->last_output_open.access_attempt_count++] = 1U;
	open_operation = reopening ? YT_TEXT_OPEN_REOPEN
	    : YT_TEXT_OPEN_EXISTING;
	delivered = text_output_open_observe(output, resolved, open_operation,
	    1U, NULL, 0U, &observation);
	valid = delivered && text_open_observation_valid(open_operation,
	    0U, &observation);
	if (!valid) {
		if (observation.file != NULL && observation.handle_open)
			output->orphaned_file = observation.file;
		text_output_open_failure(output, YT_TEXT_OPEN_PROVIDER_ERROR,
		    open_operation, 67U, observation.dos_error, error);
		return false;
	}
	if (!observation.carry) {
		output->file = observation.file;
		goto opened;
	}
	if (observation.dos_error == 5U) {
		delivered = text_output_open_observe(output, resolved,
		    YT_TEXT_OPEN_EXTENDED_ERROR, 0U, NULL, 5U,
		    &observation);
		valid = delivered && text_open_observation_valid(
		    YT_TEXT_OPEN_EXTENDED_ERROR, 0U, &observation);
		if (!valid) {
			text_output_open_failure(output,
			    YT_TEXT_OPEN_PROVIDER_ERROR,
			    YT_TEXT_OPEN_EXTENDED_ERROR, 67U, 5U, error);
			return false;
		}
		text_output_open_failure(output, reopening
		    ? YT_TEXT_OPEN_REOPEN_ERROR
		    : YT_TEXT_OPEN_INITIAL_ERROR,
		    YT_TEXT_OPEN_EXTENDED_ERROR, observation.mapped_error,
		    5U, error);
		return false;
	}
	if (!reopening && observation.dos_error == 2U)
		goto create_missing;
	text_output_open_failure(output, reopening
	    ? YT_TEXT_OPEN_REOPEN_ERROR
	    : YT_TEXT_OPEN_INITIAL_ERROR, open_operation,
	    !reopening && observation.dos_error == 3U ? 76U
	    : reopening && observation.dos_error == 2U ? 53U : 67U,
	    observation.dos_error, error);
	return false;

create_missing:
	delivered = text_output_open_observe(output, resolved,
	    YT_TEXT_OPEN_CREATE, 1U, NULL, 0U, &observation);
	valid = delivered && text_open_observation_valid(
	    YT_TEXT_OPEN_CREATE, 0U, &observation);
	if (!valid) {
		if (observation.file != NULL && observation.handle_open)
			output->orphaned_file = observation.file;
		text_output_open_failure(output, YT_TEXT_OPEN_PROVIDER_ERROR,
		    YT_TEXT_OPEN_CREATE, 67U, observation.dos_error, error);
		return false;
	}
	if (observation.carry) {
		uint16_t create_error = observation.dos_error;

		if (create_error == 5U) {
			delivered = text_output_open_observe(output, resolved,
			    YT_TEXT_OPEN_EXTENDED_ERROR, 0U, NULL,
			    create_error, &observation);
			valid = delivered && text_open_observation_valid(
			    YT_TEXT_OPEN_EXTENDED_ERROR, 0U,
			    &observation);
			if (!valid) {
				text_output_open_failure(output,
				    YT_TEXT_OPEN_PROVIDER_ERROR,
				    YT_TEXT_OPEN_EXTENDED_ERROR, 67U,
				    create_error, error);
				return false;
			}
			text_output_open_failure(output,
			    YT_TEXT_OPEN_CREATE_ERROR,
			    YT_TEXT_OPEN_EXTENDED_ERROR,
			    observation.mapped_error, create_error, error);
			return false;
		}
		text_output_open_failure(output, YT_TEXT_OPEN_CREATE_ERROR,
		    YT_TEXT_OPEN_CREATE,
		    create_error == 2U ? 53U : 67U, create_error, error);
		return false;
	}
	output->last_output_open.created = true;
	temporary = observation.file;
	temporary_open = true;
	output->last_output_open.temporary_close_attempted = true;
	delivered = text_output_open_observe(output, resolved,
	    YT_TEXT_OPEN_TEMP_CLOSE, 0U, temporary, 0U, &observation);
	valid = delivered && text_open_observation_valid(
	    YT_TEXT_OPEN_TEMP_CLOSE, 0U, &observation);
	if (!valid) {
		if (!delivered || observation.handle_open)
			output->orphaned_file = temporary;
		text_output_open_failure(output, YT_TEXT_OPEN_PROVIDER_ERROR,
		    YT_TEXT_OPEN_TEMP_CLOSE, 67U, observation.dos_error,
		    error);
		return false;
	}
	temporary_open = observation.handle_open;
	if (observation.carry) {
		first_close_error = observation.dos_error;
		output->last_output_open.temporary_close_retried = true;
		delivered = text_output_open_observe(output, resolved,
		    YT_TEXT_OPEN_TEMP_CLOSE, 0U,
		    temporary_open ? temporary : NULL, first_close_error,
		    &observation);
		valid = delivered && text_open_observation_valid(
		    YT_TEXT_OPEN_TEMP_CLOSE, 0U, &observation);
		if (!valid || observation.handle_open)
			output->orphaned_file = temporary;
		text_output_open_failure(output,
		    YT_TEXT_OPEN_TEMP_CLOSE_ERROR,
		    YT_TEXT_OPEN_TEMP_CLOSE, 70U, first_close_error, error);
		return false;
	}
	reopening = true;
	goto open_attempt;

opened:
	delivered = text_output_open_observe(output, resolved,
	    YT_TEXT_OPEN_QUERY_DEVICE, 1U, output->file, 0U,
	    &observation);
	valid = delivered && text_open_observation_valid(
	    YT_TEXT_OPEN_QUERY_DEVICE, 0U, &observation);
	if (!valid) {
		text_output_open_failure(output, YT_TEXT_OPEN_PROVIDER_ERROR,
		    YT_TEXT_OPEN_QUERY_DEVICE, 57U, observation.dos_error,
		    error);
		return false;
	}
	output->last_output_open.device = observation.device;
	if (observation.device) {
		delivered = text_output_open_observe(output, resolved,
		    YT_TEXT_OPEN_CONFIGURE_DEVICE, 1U, output->file, 0U,
		    &observation);
		valid = delivered && text_open_observation_valid(
		    YT_TEXT_OPEN_CONFIGURE_DEVICE, 0U, &observation);
		if (!valid) {
			text_output_open_failure(output,
			    YT_TEXT_OPEN_PROVIDER_ERROR,
			    YT_TEXT_OPEN_CONFIGURE_DEVICE, 57U,
			    observation.dos_error, error);
			return false;
		}
		if (observation.carry) {
			text_output_open_failure(output,
			    YT_TEXT_OPEN_DEVICE_ERROR,
			    YT_TEXT_OPEN_CONFIGURE_DEVICE, 57U,
			    observation.dos_error, error);
			return false;
		}
	}
	output->last_output_open.outcome = YT_TEXT_OPEN_RETURNED;
	output->last_output_open.registered = true;
	output->last_output_open.handle_open = true;
	output->pending_count = 0U;
	memset(&output->last_write, 0, sizeof(output->last_write));
	memset(&output->last_close, 0, sizeof(output->last_close));
	return true;
}

bool
yt_text_output_open_append(struct yt_text_output *output, const char *path,
    struct yt_error *error)
{
	char resolved[512];
	struct yt_text_open_observation observation;
	FILE *temporary = NULL;
	uint8_t window[YT_TEXT_OUTPUT_BUFFER_SIZE];
	uint8_t access = 2U;
	uint16_t first_close_error;
	int64_t length;
	int64_t start;
	int64_t candidate;
	int64_t cursor;
	size_t index;
	enum yt_text_open_operation open_operation;
	bool delivered;
	bool valid;
	bool reopening = false;
	bool temporary_open = false;

	if (output == NULL || path == NULL || output->file != NULL
	    || output->orphaned_file != NULL) {
		errno = 0;
		set_error(error, YT_INVALID, "open text append", path);
		return false;
	}
	memset(&output->last_append_open, 0, sizeof(output->last_append_open));
	output->last_append_open.failed_operation = YT_TEXT_OPEN_EXISTING;
	output->last_append_open.terminal_position = -1;
	memset(&output->last_output_open, 0,
	    sizeof(output->last_output_open));
	if (!yt_resolve_case_path(path, true, resolved, sizeof(resolved), error))
		return false;
	(void)snprintf(output->path, sizeof(output->path), "%s", resolved);

open_attempt:
	if (output->last_append_open.access_attempt_count
	    >= sizeof(output->last_append_open.access_attempts)) {
		text_append_open_failure(output,
		    YT_TEXT_OPEN_PROVIDER_ERROR,
		    reopening ? YT_TEXT_OPEN_REOPEN
		    : YT_TEXT_OPEN_EXISTING, 75U, 0U, error);
		return false;
	}
	output->last_append_open.access_attempts[
	    output->last_append_open.access_attempt_count++] = access;
	open_operation = reopening ? YT_TEXT_OPEN_REOPEN
	    : YT_TEXT_OPEN_EXISTING;
	delivered = text_append_observe(output, resolved, open_operation,
	    access, NULL, 0, NULL, 0U, 0U, &observation);
	valid = delivered && text_open_observation_valid(open_operation,
	    0U, &observation);
	if (!valid) {
		if (observation.file != NULL && observation.handle_open)
			output->orphaned_file = observation.file;
		text_append_open_failure(output, YT_TEXT_OPEN_PROVIDER_ERROR,
		    open_operation, 75U, observation.dos_error, error);
		return false;
	}
	if (!observation.carry) {
		output->file = observation.file;
		goto opened;
	}
	if (observation.dos_error == 5U && access > 1U) {
		--access;
		goto open_attempt;
	}
	if (observation.dos_error == 5U) {
		if (!text_append_observe(output, resolved,
		    YT_TEXT_OPEN_EXTENDED_ERROR, 0U, NULL, 0, NULL, 0U,
		    observation.dos_error, &observation)
		    || !text_open_observation_valid(
		    YT_TEXT_OPEN_EXTENDED_ERROR, 0U, &observation)) {
			text_append_open_failure(output,
			    YT_TEXT_OPEN_PROVIDER_ERROR,
			    YT_TEXT_OPEN_EXTENDED_ERROR, 75U, 5U, error);
			return false;
		}
		text_append_open_failure(output, reopening
		    ? YT_TEXT_OPEN_REOPEN_ERROR
		    : YT_TEXT_OPEN_INITIAL_ERROR,
		    YT_TEXT_OPEN_EXTENDED_ERROR, observation.mapped_error,
		    5U, error);
		return false;
	}
	if (!reopening && observation.dos_error == 2U)
		goto create_missing;
	text_append_open_failure(output, reopening
	    ? YT_TEXT_OPEN_REOPEN_ERROR
	    : YT_TEXT_OPEN_INITIAL_ERROR, reopening
	    ? YT_TEXT_OPEN_REOPEN : YT_TEXT_OPEN_EXISTING,
	    !reopening && observation.dos_error == 3U ? 76U
	    : reopening && observation.dos_error == 2U ? 53U : 75U,
	    observation.dos_error, error);
	return false;

create_missing:
	delivered = text_append_observe(output, resolved,
	    YT_TEXT_OPEN_CREATE, 2U, NULL, 0, NULL, 0U, 0U,
	    &observation);
	valid = delivered && text_open_observation_valid(
	    YT_TEXT_OPEN_CREATE, 0U, &observation);
	if (!valid) {
		if (observation.file != NULL && observation.handle_open)
			output->orphaned_file = observation.file;
		text_append_open_failure(output, YT_TEXT_OPEN_PROVIDER_ERROR,
		    YT_TEXT_OPEN_CREATE, 75U, observation.dos_error, error);
		return false;
	}
	if (observation.carry) {
		uint16_t create_error = observation.dos_error;

		if (create_error == 5U) {
			delivered = text_append_observe(output, resolved,
			    YT_TEXT_OPEN_EXTENDED_ERROR, 0U, NULL, 0,
			    NULL, 0U, create_error, &observation);
			valid = delivered && text_open_observation_valid(
			    YT_TEXT_OPEN_EXTENDED_ERROR, 0U,
			    &observation);
			if (!valid) {
				text_append_open_failure(output,
				    YT_TEXT_OPEN_PROVIDER_ERROR,
				    YT_TEXT_OPEN_EXTENDED_ERROR, 75U,
				    create_error, error);
				return false;
			}
			text_append_open_failure(output,
			    YT_TEXT_OPEN_CREATE_ERROR,
			    YT_TEXT_OPEN_EXTENDED_ERROR,
			    observation.mapped_error, create_error, error);
			return false;
		}
		text_append_open_failure(output, YT_TEXT_OPEN_CREATE_ERROR,
		    YT_TEXT_OPEN_CREATE,
		    create_error == 2U ? 53U : 75U, create_error, error);
		return false;
	}
	output->last_append_open.created = true;
	temporary = observation.file;
	temporary_open = true;
	output->last_append_open.temporary_close_attempted = true;
	delivered = text_append_observe(output, resolved,
	    YT_TEXT_OPEN_TEMP_CLOSE, 0U, temporary, 0, NULL, 0U, 0U,
	    &observation);
	valid = delivered && text_open_observation_valid(
	    YT_TEXT_OPEN_TEMP_CLOSE, 0U, &observation);
	if (!valid) {
		if (!delivered || observation.handle_open)
			output->orphaned_file = temporary;
		text_append_open_failure(output, YT_TEXT_OPEN_PROVIDER_ERROR,
		    YT_TEXT_OPEN_TEMP_CLOSE, 75U, observation.dos_error,
		    error);
		return false;
	}
	temporary_open = observation.handle_open;
	if (observation.carry) {
		first_close_error = observation.dos_error;
		output->last_append_open.temporary_close_retried = true;
		delivered = text_append_observe(output, resolved,
		    YT_TEXT_OPEN_TEMP_CLOSE, 0U,
		    temporary_open ? temporary : NULL, 0, NULL, 0U,
		    first_close_error, &observation);
		valid = delivered && text_open_observation_valid(
		    YT_TEXT_OPEN_TEMP_CLOSE, 0U, &observation);
		if (!valid || observation.handle_open)
			output->orphaned_file = temporary;
		text_append_open_failure(output,
		    YT_TEXT_OPEN_TEMP_CLOSE_ERROR,
		    YT_TEXT_OPEN_TEMP_CLOSE, 70U, first_close_error, error);
		return false;
	}
	reopening = true;
	access = 2U;
	goto open_attempt;

opened:
	delivered = text_append_observe(output, resolved,
	    YT_TEXT_OPEN_QUERY_DEVICE, access, output->file, 0, NULL, 0U,
	    0U, &observation);
	valid = delivered && text_open_observation_valid(
	    YT_TEXT_OPEN_QUERY_DEVICE, 0U, &observation);
	if (!valid) {
		text_append_open_failure(output, YT_TEXT_OPEN_PROVIDER_ERROR,
		    YT_TEXT_OPEN_QUERY_DEVICE, 75U, observation.dos_error,
		    error);
		return false;
	}
	output->last_append_open.device = observation.device;
	if (observation.device) {
		delivered = text_append_observe(output, resolved,
		    YT_TEXT_OPEN_CONFIGURE_DEVICE, access, output->file, 0,
		    NULL, 0U, 0U, &observation);
		valid = delivered && text_open_observation_valid(
		    YT_TEXT_OPEN_CONFIGURE_DEVICE, 0U, &observation);
		if (!valid) {
			text_append_open_failure(output,
			    YT_TEXT_OPEN_PROVIDER_ERROR,
			    YT_TEXT_OPEN_CONFIGURE_DEVICE, 57U,
			    observation.dos_error, error);
			return false;
		}
		if (observation.carry) {
			text_append_open_failure(output,
			    YT_TEXT_OPEN_DEVICE_ERROR,
			    YT_TEXT_OPEN_CONFIGURE_DEVICE, 57U,
			    observation.dos_error, error);
			return false;
		}
		text_append_open_failure(output, YT_TEXT_OPEN_DEVICE_ERROR,
		    YT_TEXT_OPEN_CONFIGURE_DEVICE, 57U, 0U, error);
		return false;
	}
	delivered = text_append_observe(output, resolved,
	    YT_TEXT_OPEN_SEEK_END, access, output->file, 0, NULL, 0U, 0U,
	    &observation);
	valid = delivered && text_open_observation_valid(
	    YT_TEXT_OPEN_SEEK_END, 0U, &observation);
	if (!valid) {
		text_append_open_failure(output, YT_TEXT_OPEN_PROVIDER_ERROR,
		    YT_TEXT_OPEN_SEEK_END, 57U, observation.dos_error, error);
		return false;
	}
	if (observation.carry) {
		text_append_open_failure(output, YT_TEXT_OPEN_SEEK_ERROR,
		    YT_TEXT_OPEN_SEEK_END, 52U, observation.dos_error, error);
		return false;
	}
	length = observation.terminal_position;
	start = length > YT_TEXT_OUTPUT_BUFFER_SIZE
	    ? length - YT_TEXT_OUTPUT_BUFFER_SIZE : 0;
	output->last_append_open.physical_length = length;
	output->last_append_open.window_start = start;
	cursor = start;
	if (length != 0) {
		delivered = text_append_observe(output, resolved,
		    YT_TEXT_OPEN_SEEK_WINDOW, access, output->file, start,
		    NULL, 0U, 0U, &observation);
		valid = delivered && text_open_observation_valid(
		    YT_TEXT_OPEN_SEEK_WINDOW, 0U, &observation);
		if (!valid) {
			text_append_open_failure(output,
			    YT_TEXT_OPEN_PROVIDER_ERROR,
			    YT_TEXT_OPEN_SEEK_WINDOW, 57U,
			    observation.dos_error, error);
			return false;
		}
		if (observation.carry) {
			text_append_open_failure(output,
			    YT_TEXT_OPEN_SEEK_ERROR,
			    YT_TEXT_OPEN_SEEK_WINDOW, 52U,
			    observation.dos_error, error);
			return false;
		}
		if (observation.terminal_position != start) {
			text_append_open_failure(output,
			    YT_TEXT_OPEN_PROVIDER_ERROR,
			    YT_TEXT_OPEN_SEEK_WINDOW, 57U, 0U, error);
			return false;
		}
	}
	candidate = start;
	for (;;) {
		delivered = text_append_observe(output, resolved,
		    YT_TEXT_OPEN_READ_WINDOW, access, output->file, 0, window,
		    sizeof(window), 0U, &observation);
		valid = delivered && text_open_observation_valid(
		    YT_TEXT_OPEN_READ_WINDOW, sizeof(window), &observation);
		if (!valid) {
			text_append_open_failure(output,
			    YT_TEXT_OPEN_PROVIDER_ERROR,
			    YT_TEXT_OPEN_READ_WINDOW, 57U,
			    observation.dos_error, error);
			return false;
		}
		if (observation.carry) {
			text_append_open_failure(output, YT_TEXT_OPEN_READ_ERROR,
			    YT_TEXT_OPEN_READ_WINDOW,
			    observation.dos_error == 5U
			    ? observation.mapped_error : 57U,
			    observation.dos_error, error);
			return false;
		}
		if (observation.accepted > (size_t)(length - cursor)
		    || observation.terminal_position
		    != cursor + (int64_t)observation.accepted) {
			text_append_open_failure(output,
			    YT_TEXT_OPEN_PROVIDER_ERROR,
			    YT_TEXT_OPEN_READ_WINDOW, 57U, 0U, error);
			return false;
		}
		++output->last_append_open.refill_count;
		cursor = observation.terminal_position;
		if (observation.accepted == 0U)
			break;
		for (index = 0U; index < observation.accepted; ++index) {
			if (window[index] == 0x1aU)
				goto selected;
			++candidate;
		}
	}

selected:
	delivered = text_append_observe(output, resolved,
	    YT_TEXT_OPEN_SEEK_SELECTED, access, output->file, candidate,
	    NULL, 0U, 0U, &observation);
	valid = delivered && text_open_observation_valid(
	    YT_TEXT_OPEN_SEEK_SELECTED, 0U, &observation);
	if (!valid) {
		text_append_open_failure(output, YT_TEXT_OPEN_PROVIDER_ERROR,
		    YT_TEXT_OPEN_SEEK_SELECTED, 57U,
		    observation.dos_error, error);
		return false;
	}
	if (observation.carry) {
		text_append_open_failure(output, YT_TEXT_OPEN_SEEK_ERROR,
		    YT_TEXT_OPEN_SEEK_SELECTED, 52U,
		    observation.dos_error, error);
		return false;
	}
	if (observation.terminal_position != candidate) {
		text_append_open_failure(output,
		    YT_TEXT_OPEN_PROVIDER_ERROR,
		    YT_TEXT_OPEN_SEEK_SELECTED, 57U, 0U, error);
		return false;
	}
	output->last_append_open.outcome = YT_TEXT_OPEN_RETURNED;
	output->last_append_open.selected_position = candidate;
	output->last_append_open.registered = true;
	output->last_append_open.handle_open = true;
	output->pending_count = 0U;
	memset(&output->last_write, 0, sizeof(output->last_write));
	memset(&output->last_close, 0, sizeof(output->last_close));
	return true;
}

bool
yt_text_output_stage(struct yt_text_output *output, const uint8_t *data,
    size_t length, struct yt_error *error)
{
	if (output == NULL || output->file == NULL
	    || (data == NULL && length != 0U)
	    || length > sizeof(output->pending)) {
		errno = 0;
		set_error(error, YT_INVALID, "stage text output",
		    output == NULL ? NULL : output->path);
		return false;
	}
	if (length != 0U)
		memcpy(output->pending, data, length);
	output->pending_count = length;
	return true;
}

static bool
text_device_write_observation_valid(size_t requested,
    const struct yt_text_device_write_observation *observation)
{
	if (requested > 1U || observation->accepted > requested
	    || observation->terminal_position < -1
	    || observation->dos_error > 0xffU)
		return false;
	if (observation->carry)
		return observation->dos_error >= 1U
		    && (!observation->physical_unknown
		    || observation->accepted == 0U);
	return !observation->physical_unknown
	    && observation->dos_error == 0U
	    && observation->extended_ax == 0U;
}

static uint8_t
text_device_logical_byte(const uint8_t *data, size_t length,
    size_t logical_index, uint8_t device_code)
{
	if (logical_index < length)
		return data[logical_index];
	if (logical_index == length)
		return '\r';
	(void)device_code;
	return '\n';
}

static bool
text_device_print_failure(struct yt_text_device_state *state,
    struct yt_text_device_print_result *result,
    enum yt_text_device_print_outcome outcome, uint16_t basic_error,
    uint16_t dos_error, struct yt_error *error)
{
	result->outcome = outcome;
	result->basic_error = basic_error;
	result->dos_error = dos_error;
	result->physical_unknown = state->physical_unknown;
	result->selected = state->selected;
	set_error(error, YT_IO_ERROR, "character-device PRINT", NULL);
	return false;
}

static bool
text_device_print_observe(struct yt_text_device_state *state,
    yt_text_device_write_provider provider, void *context,
    enum yt_text_device_write_phase phase, const uint8_t *data,
    size_t requested, struct yt_text_device_print_result *result,
    struct yt_text_device_write_observation *observation,
    struct yt_error *error)
{
	bool delivered;

	++result->write_count;
	memset(observation, 0, sizeof(*observation));
	observation->terminal_position = -1;
	delivered = provider(context, phase, data, requested, observation);
	if (!delivered || !text_device_write_observation_valid(requested,
	    observation))
		return text_device_print_failure(state, result,
		    YT_TEXT_DEVICE_PRINT_PROVIDER_ERROR, 57U,
		    delivered ? observation->dos_error : 0U, error);
	result->terminal_position = observation->terminal_position;
	if (observation->physical_unknown)
		state->physical_unknown = true;
	else
		result->accepted_count += observation->accepted;
	return true;
}

bool
yt_text_device_print(struct yt_text_device_state *state,
    const uint8_t *data, size_t length, bool newline, uint8_t device_code,
    uint8_t status, uint8_t dos_major,
    yt_text_device_write_provider provider, void *context,
    struct yt_text_device_print_result *result, struct yt_error *error)
{
	struct yt_text_device_write_observation observation;
	size_t logical_length;
	size_t logical_index;
	size_t newline_length = 0U;
	size_t completion_requested;
	uint8_t value;

	if (newline)
		newline_length = device_code == YT_TEXT_DEVICE_SCRN
		    || device_code == YT_TEXT_DEVICE_COM1
		    || device_code == YT_TEXT_DEVICE_COM2 ? 1U : 2U;
	if (state == NULL || result == NULL || provider == NULL
	    || (data == NULL && length != 0U) || !state->selected
	    || state->index > 0xffffffU || !(status & 0x80U)
	    || length > SIZE_MAX - newline_length) {
		if (result != NULL)
			memset(result, 0, sizeof(*result));
		set_error(error, YT_INVALID, "character-device PRINT", NULL);
		return false;
	}
	logical_length = length + newline_length;
	memset(result, 0, sizeof(*result));
	result->logical_length = logical_length;
	result->terminal_position = -1;
	result->physical_unknown = state->physical_unknown;
	result->selected = state->selected;

	for (logical_index = 0U; logical_index < logical_length;
	    ++logical_index) {
		if (state->pending) {
			if (!text_device_print_observe(state, provider, context,
			    YT_TEXT_DEVICE_WRITE_VALUE, &state->buffer, 1U,
			    result, &observation, error))
				return false;
			if (observation.carry) {
				uint16_t mapped = dos_major < 3U ? 52U
				    : observation.extended_ax == 0x0021U
				    ? 70U : 52U;

				return text_device_print_failure(state, result,
				    YT_TEXT_DEVICE_PRINT_VALUE_DISK_ERROR,
				    mapped, observation.dos_error, error);
			}
			if (observation.accepted == 0U && !(status & 0x02U))
				return text_device_print_failure(state, result,
				    YT_TEXT_DEVICE_PRINT_VALUE_SHORT_ERROR,
				    57U, 0U, error);
			state->pending = false;
		}
		value = text_device_logical_byte(data, length, logical_index,
		    device_code);
		state->buffer = value;
		state->pending = true;
		state->index = (state->index + 1U) & 0xffffffU;
		if (value == '\r')
			state->column = 0U;
		else if (value >= 0x20U)
			++state->column;
		++result->staged;
	}

	completion_requested = state->pending ? 1U : 0U;
	state->pending = false;
	if (!text_device_print_observe(state, provider, context,
	    YT_TEXT_DEVICE_WRITE_COMPLETION, &state->buffer,
	    completion_requested, result, &observation, error))
		return false;
	if (observation.carry
	    || (completion_requested != 0U && observation.accepted == 0U
	    && !(status & 0x02U)))
		return text_device_print_failure(state, result,
		    YT_TEXT_DEVICE_PRINT_COMPLETION_ERROR, 57U,
		    observation.carry ? observation.dos_error : 0U, error);
	state->selected = false;
	result->outcome = YT_TEXT_DEVICE_PRINT_RETURNED;
	result->physical_unknown = state->physical_unknown;
	result->selected = false;
	return true;
}

static bool
text_output_write_observation_valid(size_t requested,
    const struct yt_text_output_write_observation *observation)
{
	if (requested != YT_TEXT_OUTPUT_BUFFER_SIZE
	    || observation->accepted > requested
	    || observation->terminal_position < -1
	    || !observation->handle_open)
		return false;
	if (observation->carry)
		return observation->accepted == 0U
		    && observation->terminal_position == -1
		    && observation->dos_error >= 1U
		    && observation->dos_error <= 0xffU;
	return observation->dos_error == 0U;
}

static void
text_output_write_failure(struct yt_text_output *output,
    enum yt_text_output_write_outcome outcome, uint16_t basic_error,
    uint16_t dos_error, size_t accepted, size_t failed_flush_accepted,
    int64_t terminal_position, bool physical_unknown, struct yt_error *error)
{
	output->last_write.outcome = outcome;
	output->last_write.accepted = accepted;
	output->last_write.failed_flush_accepted = failed_flush_accepted;
	output->last_write.basic_error = basic_error;
	output->last_write.dos_error = dos_error;
	output->last_write.terminal_position = terminal_position;
	output->last_write.physical_unknown = physical_unknown;
	output->last_write.registered = output->file != NULL;
	output->last_write.handle_open = output->file != NULL
	    || output->orphaned_file != NULL;
	set_error(error, YT_IO_ERROR, "sequential PRINT", output->path);
}

static bool
text_output_write_cleanup_after_carry(struct yt_text_output *output,
    FILE *file, size_t accepted,
    const struct yt_text_output_write_observation *failure,
    struct yt_error *error)
{
	yt_text_close_provider provider;
	struct yt_text_close_observation cleanup;
	bool delivered;
	bool valid;

	output->file = NULL;
	output->pending_count = 0U;
	output->last_write.cleanup_close_attempted = true;
	provider = output->close_provider != NULL ? output->close_provider
	    : text_close_default;
	memset(&cleanup, 0, sizeof(cleanup));
	cleanup.terminal_position = -1;
	delivered = provider(output->close_context, file,
	    YT_TEXT_CLOSE_CLEANUP_HANDLE, NULL, 0U, &cleanup);
	valid = delivered && text_close_observation_valid(
	    YT_TEXT_CLOSE_CLEANUP_HANDLE, 0U, &cleanup);
	if (!valid || cleanup.handle_open)
		output->orphaned_file = file;
	text_output_write_failure(output, valid
	    ? YT_TEXT_OUTPUT_WRITE_DISK_ERROR
	    : YT_TEXT_OUTPUT_WRITE_PROVIDER_ERROR, valid ? 71U : 57U,
	    failure->dos_error, accepted, 0U, failure->terminal_position,
	    true, error);
	return false;
}

bool
yt_text_output_write(struct yt_text_output *output, const uint8_t *data,
    size_t length, struct yt_error *error)
{
	yt_text_output_write_provider provider;
	struct yt_text_output_write_observation observation;
	FILE *file;
	size_t index;
	bool delivered;

	if (output == NULL || output->file == NULL
	    || (data == NULL && length != 0U)) {
		errno = 0;
		set_error(error, YT_INVALID, "sequential PRINT",
		    output == NULL ? NULL : output->path);
		return false;
	}
	memset(&output->last_write, 0, sizeof(output->last_write));
	output->last_write.terminal_position = -1;
	file = output->file;
	provider = output->write_provider != NULL ? output->write_provider
	    : text_output_write_default;
	for (index = 0U; index < length; ++index) {
		if (output->pending_count == sizeof(output->pending)) {
			output->pending_count = 0U;
			++output->last_write.flush_count;
			memset(&observation, 0, sizeof(observation));
			observation.terminal_position = -1;
			delivered = provider(output->write_context, file,
			    output->pending, sizeof(output->pending), &observation);
			if (!delivered || !text_output_write_observation_valid(
			    sizeof(output->pending), &observation)) {
				if (delivered && !observation.handle_open)
					output->file = NULL;
				text_output_write_failure(output,
				    YT_TEXT_OUTPUT_WRITE_PROVIDER_ERROR, 57U,
				    observation.dos_error, index,
				    delivered ? observation.accepted : 0U,
				    delivered ? observation.terminal_position : -1,
				    false, error);
				return false;
			}
			if (observation.carry)
				return text_output_write_cleanup_after_carry(output,
				    file, index, &observation, error);
			if (observation.accepted != sizeof(output->pending)) {
				output->file = NULL;
				output->orphaned_file = file;
				text_output_write_failure(output,
				    YT_TEXT_OUTPUT_WRITE_SHORT_ERROR, 61U, 0U,
				    index, observation.accepted,
				    observation.terminal_position, false, error);
				return false;
			}
			output->last_write.terminal_position =
			    observation.terminal_position;
		}
		output->pending[output->pending_count++] = data[index];
	}
	output->last_write.outcome = YT_TEXT_OUTPUT_WRITE_RETURNED;
	output->last_write.accepted = length;
	output->last_write.registered = true;
	output->last_write.handle_open = true;
	return true;
}

static bool
text_output_close_observe(struct yt_text_output *output,
    yt_text_close_provider provider, FILE *file,
    enum yt_text_close_operation operation, const uint8_t *data,
    size_t requested, struct yt_text_close_observation *observation)
{
	++output->last_close.operation_count;
	memset(observation, 0, sizeof(*observation));
	observation->terminal_position = -1;
	return provider(output->close_context, file, operation, data, requested,
	    observation);
}

static void
text_output_close_failure(struct yt_text_output *output,
    enum yt_text_close_outcome outcome,
    enum yt_text_close_operation operation, uint16_t basic_error,
    uint16_t dos_error, const struct yt_text_close_observation *observed,
    struct yt_error *error)
{
	output->last_close.outcome = outcome;
	output->last_close.failed_operation = operation;
	output->last_close.basic_error = basic_error;
	output->last_close.dos_error = dos_error;
	if (observed != NULL) {
		output->last_close.accepted = observed->accepted;
		output->last_close.terminal_position =
		    observed->terminal_position;
	}
	output->last_close.registered = output->file != NULL;
	output->last_close.handle_open = output->file != NULL
	    || output->orphaned_file != NULL;
	set_error(error, YT_IO_ERROR, output->last_close.close_all
	    ? "CLOSE all" : "sequential CLOSE", output->path);
}

static bool
text_output_cleanup_after_carry(struct yt_text_output *output,
    yt_text_close_provider provider, FILE *file, bool handle_open,
    struct yt_error *error,
    enum yt_text_close_operation failed_operation,
    const struct yt_text_close_observation *failure)
{
	struct yt_text_close_observation cleanup;
	bool delivered;
	bool valid;

	output->file = NULL;
	output->pending_count = 0U;
	output->last_close.cleanup_close_attempted = true;
	delivered = text_output_close_observe(output, provider,
	    handle_open ? file : NULL, YT_TEXT_CLOSE_CLEANUP_HANDLE,
	    NULL, 0U, &cleanup);
	valid = delivered && text_close_observation_valid(
	    YT_TEXT_CLOSE_CLEANUP_HANDLE, 0U, &cleanup);
	if ((!valid && handle_open) || (valid && cleanup.handle_open))
		output->orphaned_file = file;
	text_output_close_failure(output, valid
	    ? YT_TEXT_CLOSE_DISK_ERROR
	    : YT_TEXT_CLOSE_PROVIDER_ERROR, failed_operation,
	    valid ? (output->last_close.device ? 57U : 70U) : 57U,
	    failure->dos_error, failure, error);
	return false;
}

static bool
text_output_close_execute(struct yt_text_output *output, bool close_all,
    struct yt_error *error)
{
	yt_text_close_provider provider;
	struct yt_text_close_observation observation;
	FILE *file;
	const uint8_t *data;
	size_t requested;
	enum yt_text_close_operation operation;
	bool delivered;
	static const uint8_t eof_byte = 0x1aU;

	if (output == NULL) {
		errno = 0;
		set_error(error, YT_INVALID,
		    close_all ? "CLOSE all" : "sequential CLOSE", NULL);
		return false;
	}
	memset(&output->last_close, 0, sizeof(output->last_close));
	output->last_close.close_all = close_all;
	output->last_close.terminal_position = -1;
	output->last_close.device = output->last_output_open.device
	    || output->last_append_open.device;
	if (output->file == NULL) {
		output->last_close.outcome = YT_TEXT_CLOSE_RETURNED;
		output->last_close.missing = !close_all;
		output->last_close.handle_open = output->orphaned_file != NULL;
		return true;
	}
	file = output->file;
	provider = output->close_provider != NULL ? output->close_provider
	    : text_close_default;
	for (operation = output->last_close.device ? YT_TEXT_CLOSE_HANDLE
	    : YT_TEXT_CLOSE_PENDING_WRITE;
	    operation <= YT_TEXT_CLOSE_HANDLE; ++operation) {
		if (operation == YT_TEXT_CLOSE_PENDING_WRITE) {
			data = output->pending;
			requested = output->pending_count;
			output->pending_count = 0U;
		}
		else if (operation == YT_TEXT_CLOSE_EOF_WRITE) {
			output->pending[0] = eof_byte;
			output->pending_count = 1U;
			data = output->pending;
			requested = 1U;
			output->pending_count = 0U;
		}
		else {
			data = NULL;
			requested = 0U;
		}
		delivered = text_output_close_observe(output, provider, file,
		    operation, data, requested, &observation);
		if (!delivered || !text_close_observation_valid(operation,
		    requested, &observation)) {
			if (delivered && !observation.handle_open)
				output->file = NULL;
			text_output_close_failure(output,
			    YT_TEXT_CLOSE_PROVIDER_ERROR, operation, 57U,
			    observation.dos_error, delivered ? &observation : NULL,
			    error);
			return false;
		}
		if (observation.carry)
			return text_output_cleanup_after_carry(output, provider,
			    file, observation.handle_open, error, operation,
			    &observation);
		if (observation.accepted != requested) {
			output->file = NULL;
			output->orphaned_file = file;
			text_output_close_failure(output,
			    YT_TEXT_CLOSE_SHORT_ERROR, operation, 61U, 0U,
			    &observation, error);
			return false;
		}
		output->last_close.terminal_position =
		    observation.terminal_position;
	}
	output->file = NULL;
	output->pending_count = 0U;
	output->last_close.outcome = YT_TEXT_CLOSE_RETURNED;
	output->last_close.registered = false;
	output->last_close.handle_open = output->orphaned_file != NULL;
	return true;
}

bool
yt_text_output_close(struct yt_text_output *output, struct yt_error *error)
{
	return text_output_close_execute(output, false, error);
}

bool
yt_text_output_close_all_method(void *context, int8_t file_class,
    struct yt_error *error)
{
	if (file_class < 0) {
		errno = 0;
		set_error(error, YT_INVALID, "CLOSE all class", NULL);
		return false;
	}
	return text_output_close_execute(context, true, error);
}

void
yt_text_output_set_close_provider(struct yt_text_output *output,
    yt_text_close_provider provider, void *context)
{
	if (output == NULL)
		return;
	output->close_provider = provider;
	output->close_context = context;
}

void
yt_text_output_set_write_provider(struct yt_text_output *output,
    yt_text_output_write_provider provider, void *context)
{
	if (output == NULL)
		return;
	output->write_provider = provider;
	output->write_context = context;
}

void
yt_text_output_set_open_provider(struct yt_text_output *output,
    yt_text_open_provider provider, void *context)
{
	if (output == NULL)
		return;
	output->open_provider = provider;
	output->open_context = context;
}

void
yt_text_output_destroy(struct yt_text_output *output)
{
	if (output == NULL)
		return;
	if (output->file != NULL)
		(void)fclose(output->file);
	if (output->orphaned_file != NULL
	    && output->orphaned_file != output->file)
		(void)fclose(output->orphaned_file);
	memset(output, 0, sizeof(*output));
}

bool
yt_text_read(const char *path, struct yt_text_file *text,
    struct yt_error *error)
{
	char resolved[512];
	FILE *file;
	long length;

	memset(text, 0, sizeof(*text));
	if (!yt_resolve_case_path(path, false, resolved, sizeof(resolved), error))
		return false;
	file = fopen(resolved, "rb");
	if (file == NULL) {
		set_error(error, YT_IO_ERROR, "open text", resolved);
		return false;
	}
	if (fseek(file, 0, SEEK_END) != 0 || (length = ftell(file)) < 0
	    || fseek(file, 0, SEEK_SET) != 0) {
		set_error(error, YT_IO_ERROR, "size text", resolved);
		fclose(file);
		return false;
	}
	text->data = malloc((size_t)length + 1U);
	if (text->data == NULL) {
		set_error(error, YT_NO_MEMORY, "allocate text", resolved);
		fclose(file);
		return false;
	}
	if (length > 0
	    && fread(text->data, 1, (size_t)length, file) != (size_t)length) {
		set_error(error, YT_IO_ERROR, "read text", resolved);
		yt_text_free(text);
		fclose(file);
		return false;
	}
	fclose(file);
	text->data[length] = 0;
	text->length = (size_t)length;
	return true;
}

void
yt_text_free(struct yt_text_file *text)
{
	free(text->data);
	text->data = NULL;
	text->length = 0;
}

bool
yt_text_write(const char *path, const uint8_t *data, size_t length,
    bool dos_eof, struct yt_error *error)
{
	struct yt_text_output output;
	char resolved[512];
	FILE *file;
	bool result = false;

	if (dos_eof) {
		yt_text_output_init(&output);
		if (yt_text_output_open(&output, path, error)
		    && yt_text_output_write(&output, data, length, error)
		    && yt_text_output_close(&output, error))
			result = true;
		yt_text_output_destroy(&output);
		return result;
	}

	if (!yt_resolve_case_path(path, true, resolved, sizeof(resolved), error))
		return false;
	file = fopen(resolved, "wb");
	if (file == NULL) {
		set_error(error, YT_IO_ERROR, "open output", resolved);
		return false;
	}
	if ((length > 0 && fwrite(data, 1, length, file) != length)
	    || fclose(file) != 0) {
		set_error(error, YT_IO_ERROR, "write output", resolved);
		return false;
	}
	return true;
}

bool
yt_text_append_line(const char *path, const uint8_t *line, size_t length,
    struct yt_error *error)
{
	static const uint8_t newline[] = {'\r', '\n'};
	struct yt_text_output output;
	bool result = false;

	yt_text_output_init(&output);
	if (yt_text_output_open_append(&output, path, error)
	    && yt_text_output_write(&output, line, length, error)
	    && yt_text_output_write(&output, newline, sizeof(newline), error)
	    && yt_text_output_close(&output, error))
		result = true;
	yt_text_output_destroy(&output);
	return result;
}
