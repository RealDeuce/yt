#include "yt_text.h"
#include "yt_file.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

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
