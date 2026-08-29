#include "yt_text.h"
#include "yt_file.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

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
	record->foreground = 2;
	if (record->length >= 4U && memcmp(line, "  - ", 4U) == 0)
		record->foreground = 3;
	else if (record->length >= 4U && memcmp(line, " ***", 4U) == 0)
		record->foreground = 4;
	else if (record->length >= 4U && memcmp(line, " +++", 4U) == 0)
		record->foreground = 1;
	else if (record->length >= 3U && memcmp(line, "-=*", 3U) == 0)
		record->foreground = 7;
	record->set_bold = record->foreground != 2;
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

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
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
