#include "yt_text.h"
#include "yt_file.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

static void set_error(struct yt_error *error, enum yt_status status,
    const char *operation, const char *path);

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

bool
yt_text_input_open(struct yt_text_input *input, const char *path,
    struct yt_error *error)
{
	char resolved[512];

	if (input == NULL || path == NULL || input->file != NULL) {
		errno = 0;
		set_error(error, YT_INVALID, "open text input", path);
		return false;
	}
	if (!yt_resolve_case_path(path, false, resolved, sizeof(resolved), error))
		return false;
	errno = 0;
	input->file = fopen(resolved, "rb");
	if (input->file == NULL) {
		set_error(error, errno == ENOENT ? YT_NOT_FOUND : YT_IO_ERROR,
		    "open text input", resolved);
		return false;
	}
	(void)snprintf(input->path, sizeof(input->path), "%s", resolved);
	return true;
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

bool
yt_text_input_read_line(struct yt_text_input *input, const uint8_t **line,
    size_t *length, bool *available, struct yt_error *error)
{
	size_t used = 0U;
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
	for (;;) {
		int value = fgetc(input->file);

		if (value == EOF) {
			if (ferror(input->file)) {
				set_error(error, YT_IO_ERROR, "LINE INPUT text",
				    input->path);
				return false;
			}
			break;
		}
		if ((uint8_t)value == 0x1aU) {
			if (ungetc(value, input->file) == EOF) {
				set_error(error, YT_IO_ERROR, "LINE INPUT text",
				    input->path);
				return false;
			}
			break;
		}
		consumed = true;
		if ((uint8_t)value == '\r') {
			int following = fgetc(input->file);

			if (following == EOF) {
				if (ferror(input->file)) {
					set_error(error, YT_IO_ERROR,
					    "LINE INPUT text", input->path);
					return false;
				}
			}
			else if ((uint8_t)following != '\n'
			    && ungetc(following, input->file) == EOF) {
				set_error(error, YT_IO_ERROR, "LINE INPUT text",
				    input->path);
				return false;
			}
			break;
		}
		if ((uint8_t)value == 0U)
			continue;
		if (!text_input_reserve(input, used + 1U, error))
			return false;
		input->line[used++] = (uint8_t)value;
	}
	*line = input->line;
	*length = used;
	*available = consumed;
	return true;
}

bool
yt_text_input_eof(struct yt_text_input *input, bool *eof,
    struct yt_error *error)
{
	int value;

	if (eof != NULL)
		*eof = false;
	if (input == NULL || input->file == NULL || eof == NULL) {
		errno = 0;
		set_error(error, YT_INVALID, "EOF text input", input == NULL
		    ? NULL : input->path);
		return false;
	}
	errno = 0;
	value = fgetc(input->file);
	if (value == EOF) {
		if (ferror(input->file)) {
			set_error(error, YT_IO_ERROR, "EOF text input", input->path);
			return false;
		}
		*eof = true;
		return true;
	}
	if (ungetc(value, input->file) == EOF) {
		set_error(error, YT_IO_ERROR, "EOF text input", input->path);
		return false;
	}
	*eof = (uint8_t)value == 0x1aU;
	return true;
}

bool
yt_text_input_close(struct yt_text_input *input, struct yt_error *error)
{
	FILE *file;

	if (input == NULL) {
		errno = 0;
		set_error(error, YT_INVALID, "close text input", NULL);
		return false;
	}
	file = input->file;
	input->file = NULL;
	if (file == NULL)
		return true;
	errno = 0;
	if (fclose(file) != 0) {
		set_error(error, YT_IO_ERROR, "close text input", input->path);
		return false;
	}
	return true;
}

void
yt_text_input_destroy(struct yt_text_input *input)
{
	if (input == NULL)
		return;
	if (input->file != NULL)
		(void)fclose(input->file);
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
#define yt_text_ftello _ftelli64
#else
#include <unistd.h>
#define yt_text_fileno fileno
#define yt_text_ftello ftello
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
truncate_at_cursor(FILE *file)
{
	long position = ftell(file);

	if (position < 0 || fflush(file) != 0)
		return false;
#ifdef _WIN32
	return _chsize_s(_fileno(file), (long long)position) == 0;
#else
	return ftruncate(fileno(file), (off_t)position) == 0;
#endif
}

static uint16_t
text_output_dos_error(int system_error)
{
	switch (system_error) {
	case ENOENT:
		return 2U;
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
text_output_close_default(void *context, FILE *file,
    enum yt_text_output_close_operation operation, const uint8_t *data,
    size_t requested, struct yt_text_output_close_observation *observation)
{
	int result;
	int saved_errno;

	(void)context;
	memset(observation, 0, sizeof(*observation));
	observation->terminal_position = text_output_position(file);
	switch (operation) {
	case YT_TEXT_OUTPUT_CLOSE_PENDING_WRITE:
	case YT_TEXT_OUTPUT_CLOSE_EOF_WRITE:
		if (file == NULL || (data == NULL && requested != 0U))
			return false;
		errno = 0;
		observation->accepted = fwrite(data, 1U, requested, file);
		saved_errno = errno;
		observation->carry = ferror(file) != 0;
		observation->handle_open = true;
		observation->dos_error = observation->carry
		    ? text_output_dos_error(saved_errno) : 0U;
		observation->terminal_position = text_output_position(file);
		errno = saved_errno;
		return true;
	case YT_TEXT_OUTPUT_CLOSE_TRUNCATE:
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
		    ? text_output_dos_error(saved_errno) : 0U;
		errno = saved_errno;
		return true;
	case YT_TEXT_OUTPUT_CLOSE_HANDLE:
	case YT_TEXT_OUTPUT_CLOSE_CLEANUP_HANDLE:
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
		    ? text_output_dos_error(saved_errno) : 0U;
		observation->handle_open = false;
		errno = saved_errno;
		return true;
	default:
		return false;
	}
}

static bool
text_output_close_observation_valid(
    enum yt_text_output_close_operation operation, size_t requested,
    const struct yt_text_output_close_observation *observation)
{
	bool write = operation == YT_TEXT_OUTPUT_CLOSE_PENDING_WRITE
	    || operation == YT_TEXT_OUTPUT_CLOSE_EOF_WRITE;
	bool handle = operation == YT_TEXT_OUTPUT_CLOSE_HANDLE
	    || operation == YT_TEXT_OUTPUT_CLOSE_CLEANUP_HANDLE;

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
	char resolved[512];

	if (output == NULL || path == NULL || output->file != NULL
	    || output->orphaned_file != NULL) {
		errno = 0;
		set_error(error, YT_INVALID, "open text output", path);
		return false;
	}
	if (!yt_resolve_case_path(path, true, resolved, sizeof(resolved), error))
		return false;
	errno = 0;
	output->file = fopen(resolved, "w+b");
	if (output->file == NULL) {
		set_error(error, YT_IO_ERROR, "open text output", resolved);
		return false;
	}
	(void)snprintf(output->path, sizeof(output->path), "%s", resolved);
	output->pending_count = 0U;
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
text_output_close_observe(struct yt_text_output *output,
    yt_text_output_close_provider provider, FILE *file,
    enum yt_text_output_close_operation operation, const uint8_t *data,
    size_t requested, struct yt_text_output_close_observation *observation)
{
	++output->last_close.operation_count;
	memset(observation, 0, sizeof(*observation));
	observation->terminal_position = -1;
	return provider(output->close_context, file, operation, data, requested,
	    observation);
}

static void
text_output_close_failure(struct yt_text_output *output,
    enum yt_text_output_close_outcome outcome,
    enum yt_text_output_close_operation operation, uint16_t basic_error,
    uint16_t dos_error, const struct yt_text_output_close_observation *observed,
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
    yt_text_output_close_provider provider, FILE *file, bool handle_open,
    struct yt_error *error,
    enum yt_text_output_close_operation failed_operation,
    const struct yt_text_output_close_observation *failure)
{
	struct yt_text_output_close_observation cleanup;
	bool delivered;
	bool valid;

	output->file = NULL;
	output->pending_count = 0U;
	output->last_close.cleanup_close_attempted = true;
	delivered = text_output_close_observe(output, provider,
	    handle_open ? file : NULL, YT_TEXT_OUTPUT_CLOSE_CLEANUP_HANDLE,
	    NULL, 0U, &cleanup);
	valid = delivered && text_output_close_observation_valid(
	    YT_TEXT_OUTPUT_CLOSE_CLEANUP_HANDLE, 0U, &cleanup);
	if ((!valid && handle_open) || (valid && cleanup.handle_open))
		output->orphaned_file = file;
	text_output_close_failure(output, valid
	    ? YT_TEXT_OUTPUT_CLOSE_DISK_ERROR
	    : YT_TEXT_OUTPUT_CLOSE_PROVIDER_ERROR, failed_operation,
	    valid ? 70U : 57U, failure->dos_error, failure, error);
	return false;
}

static bool
text_output_close_execute(struct yt_text_output *output, bool close_all,
    struct yt_error *error)
{
	yt_text_output_close_provider provider;
	struct yt_text_output_close_observation observation;
	FILE *file;
	const uint8_t *data;
	size_t requested;
	enum yt_text_output_close_operation operation;
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
	if (output->file == NULL) {
		output->last_close.outcome = YT_TEXT_OUTPUT_CLOSE_RETURNED;
		output->last_close.missing = !close_all;
		output->last_close.handle_open = output->orphaned_file != NULL;
		return true;
	}
	file = output->file;
	provider = output->close_provider != NULL ? output->close_provider
	    : text_output_close_default;
	for (operation = YT_TEXT_OUTPUT_CLOSE_PENDING_WRITE;
	    operation <= YT_TEXT_OUTPUT_CLOSE_HANDLE; ++operation) {
		if (operation == YT_TEXT_OUTPUT_CLOSE_PENDING_WRITE) {
			data = output->pending;
			requested = output->pending_count;
			output->pending_count = 0U;
		}
		else if (operation == YT_TEXT_OUTPUT_CLOSE_EOF_WRITE) {
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
		if (!delivered || !text_output_close_observation_valid(operation,
		    requested, &observation)) {
			if (delivered && !observation.handle_open)
				output->file = NULL;
			text_output_close_failure(output,
			    YT_TEXT_OUTPUT_CLOSE_PROVIDER_ERROR, operation, 57U,
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
			    YT_TEXT_OUTPUT_CLOSE_SHORT_ERROR, operation, 61U, 0U,
			    &observation, error);
			return false;
		}
		output->last_close.terminal_position =
		    observation.terminal_position;
	}
	output->file = NULL;
	output->pending_count = 0U;
	output->last_close.outcome = YT_TEXT_OUTPUT_CLOSE_RETURNED;
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
    yt_text_output_close_provider provider, void *context)
{
	if (output == NULL)
		return;
	output->close_provider = provider;
	output->close_context = context;
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
	char resolved[512];
	FILE *file;
	static const uint8_t eof_byte = 0x1a;

	if (!yt_resolve_case_path(path, true, resolved, sizeof(resolved), error))
		return false;
	file = fopen(resolved, "wb");
	if (file == NULL) {
		set_error(error, YT_IO_ERROR, "open output", resolved);
		return false;
	}
	if ((length > 0 && fwrite(data, 1, length, file) != length)
	    || (dos_eof && fwrite(&eof_byte, 1, 1, file) != 1)
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
	char resolved[512];
	FILE *file;
	long end;
	long window_start;
	size_t window_length;
	size_t index;
	uint8_t window[128];
	static const uint8_t ending[] = {'\r', '\n', 0x1a};

	if (!yt_resolve_case_path(path, true, resolved, sizeof(resolved), error))
		return false;
	file = fopen(resolved, "r+b");
	if (file == NULL && errno == ENOENT)
		file = fopen(resolved, "w+b");
	if (file == NULL) {
		set_error(error, YT_IO_ERROR, "open append", resolved);
		return false;
	}
	if (fseek(file, 0, SEEK_END) != 0 || (end = ftell(file)) < 0) {
		set_error(error, YT_IO_ERROR, "seek append", resolved);
		fclose(file);
		return false;
	}
	window_start = end > (long)sizeof(window)
	    ? end - (long)sizeof(window) : 0;
	window_length = (size_t)(end - window_start);
	if (fseek(file, window_start, SEEK_SET) != 0
	    || (window_length > 0
	    && fread(window, 1, window_length, file) != window_length)) {
		set_error(error, YT_IO_ERROR, "read append window", resolved);
		fclose(file);
		return false;
	}
	for (index = 0; index < window_length; ++index) {
		if (window[index] == 0x1a) {
			end = window_start + (long)index;
			break;
		}
	}
	if (fseek(file, end, SEEK_SET) != 0
	    || (length > 0 && fwrite(line, 1, length, file) != length)
	    || fwrite(ending, 1, sizeof(ending), file) != sizeof(ending)
	    || !truncate_at_cursor(file)) {
		set_error(error, YT_IO_ERROR, "append line", resolved);
		fclose(file);
		return false;
	}
	if (fclose(file) != 0) {
		set_error(error, YT_IO_ERROR, "close append", resolved);
		return false;
	}
	return true;
}
