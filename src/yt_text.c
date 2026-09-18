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
static bool text_input_get_byte(struct yt_text_input *input, uint8_t *value,
    bool *eof, struct yt_error *error);
static uint16_t text_output_dos_error(const char *path, int system_error);
static int64_t text_output_position(FILE *file);

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
	if (input != NULL && input->file == NULL) {
		memset(input->read_ahead, 0, sizeof(input->read_ahead));
		input->read_total = 0U;
		input->read_remaining = 0U;
		input->last_read_basic_error = 0U;
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

static bool
text_input_refill(struct yt_text_input *input, struct yt_error *error)
{
	size_t accepted;
	int saved_errno;

	memset(input->read_ahead, 0, sizeof(input->read_ahead));
	errno = 0;
	accepted = fread(input->read_ahead, 1U, sizeof(input->read_ahead),
	    input->file);
	saved_errno = errno;
	if (ferror(input->file) != 0) {
		input->last_read_basic_error = 57U;
		errno = saved_errno;
		set_error(error, YT_IO_ERROR, "sequential INPUT read",
		    input->path);
		return false;
	}
	errno = saved_errno;
	if (accepted != 0U) {
		input->read_total = accepted;
		input->read_remaining = accepted;
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
	input->last_read_basic_error = 0U;
	for (;;) {
		uint8_t value;
		bool eof;

		if (!text_input_get_byte(input, &value, &eof, error))
			return false;
		if (eof)
			break;
		consumed = true;
		if (value == '\r') {
			uint8_t following;
			bool following_eof;

			if (!text_input_get_byte(input, &following,
			    &following_eof, error))
				return false;
			if (!following_eof) {
				if (following != '\n')
					++input->read_remaining;
			}
			break;
		}
		if (value == 0U)
			continue;
		if (!text_input_reserve(input, used + 1U, error)) {
			return false;
		}
		input->line[used++] = value;
	}
	*line = input->line;
	*length = used;
	*available = consumed;
	return true;
}

static bool
text_input_consume_byte(struct yt_text_input *input, uint8_t *value,
    bool *eof, struct yt_error *error)
{
	if (!text_input_get_byte(input, value, eof, error))
		return false;
	return true;
}

static void
text_input_unread_byte(struct yt_text_input *input)
{
	++input->read_remaining;
}

static bool
text_input_token_append(struct yt_text_input *input, size_t *used,
    uint8_t value, struct yt_error *error)
{
	if (!text_input_reserve(input, *used + 1U, error)) {
		return false;
	}
	input->line[(*used)++] = value;
	return true;
}

bool
yt_text_input_read_string_token(struct yt_text_input *input,
    const uint8_t **value, size_t *length, bool *available,
    struct yt_error *error)
{
	size_t used = 0U;
	size_t start;
	size_t output;
	uint8_t byte;
	bool eof;
	bool provider_quote;

	if (value != NULL)
		*value = NULL;
	if (length != NULL)
		*length = 0U;
	if (available != NULL)
		*available = false;
	if (input == NULL || input->file == NULL || value == NULL
	    || length == NULL || available == NULL) {
		errno = 0;
		set_error(error, YT_INVALID, "INPUT string token", input == NULL
		    ? NULL : input->path);
		return false;
	}
	input->last_read_basic_error = 0U;
	do {
		if (!text_input_consume_byte(input, &byte, &eof, error))
			goto failed;
		if (eof) {
			goto returned;
		}
	} while (byte == ' ');
	provider_quote = byte == '"';
	if (provider_quote) {
		if (!text_input_token_append(input, &used, byte, error))
			goto failed;
		for (;;) {
			if (!text_input_consume_byte(input, &byte, &eof, error))
				goto failed;
			if (eof) {
				break;
			}
			if (byte == 0U)
				continue;
			if (byte != '"') {
				if (!text_input_token_append(input, &used, byte, error))
					goto failed;
				continue;
			}
			do {
				if (!text_input_consume_byte(input, &byte, &eof,
				    error))
					goto failed;
				if (eof) {
					break;
				}
			} while (byte == ' ');
			if (!eof && byte == '\r') {
				if (!text_input_consume_byte(input, &byte, &eof,
				    error))
					goto failed;
				if (!eof && byte != '\n')
					text_input_unread_byte(input);
			}
			else if (!eof && byte != ',')
				text_input_unread_byte(input);
			break;
		}
	}
	else {
		for (;;) {
			if (byte == '\r') {
				if (!text_input_consume_byte(input, &byte, &eof,
				    error))
					goto failed;
				if (!eof && byte != '\n')
					text_input_unread_byte(input);
				break;
			}
			if (byte == '\n') {
				do {
					if (!text_input_consume_byte(input, &byte,
					    &eof, error))
						goto failed;
					if (eof) {
					if (!text_input_token_append(input,
						    &used, 0x1aU, error))
							goto failed;
						goto convert;
					}
				} while (byte == '\n');
				if (byte == '\r') {
					if (!text_input_consume_byte(input, &byte,
					    &eof, error))
						goto failed;
					if (eof) {
						break;
					}
				}
				continue;
			}
			if (byte == ',')
				break;
			if (byte != 0U) {
				if (!text_input_token_append(input, &used, byte,
				    error))
					goto failed;
			}
			if (!text_input_consume_byte(input, &byte, &eof, error))
				goto failed;
			if (eof) {
				break;
			}
		}
	}

convert:
	start = 0U;
	while (start < used && (input->line[start] == ' '
	    || input->line[start] == '\t' || input->line[start] == '\n'))
		++start;
	if (start < used && input->line[start] == '"') {
		++start;
		output = start;
		while (output < used && input->line[output] != '"')
			++output;
	}
	else {
		output = used;
		while (output > start && input->line[output - 1U] == ' ')
			--output;
	}
	*value = input->line == NULL ? NULL : input->line + start;
	*length = output - start;
	*available = true;

returned:
	return true;

failed:
	return false;
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
	input->last_read_basic_error = 0U;
	if (!text_input_get_byte(input, &value, &physical_eof, error))
		return false;
	if (!physical_eof)
		++input->read_remaining;
	*eof = physical_eof;
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
	free(input->line);
	memset(input, 0, sizeof(*input));
}

int
yt_file_viewer_line_foreground(const uint8_t *line, size_t length)
{
	if (length >= 4U && memcmp(line, "  - ", 4U) == 0)
		return 3;
	if (length >= 4U && memcmp(line, " ***", 4U) == 0)
		return 4;
	if (length >= 4U && memcmp(line, " +++", 4U) == 0)
		return 1;
	if (length >= 3U && memcmp(line, "-=*", 3U) == 0)
		return 7;
	return 2;
}

bool
yt_file_viewer_missing_row(const char *path, uint8_t *row,
    size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "*** GAME FILE [";
	static const uint8_t suffix[] = "] NOT FOUND! ***";
	size_t path_length;
	size_t needed;

	if (path == NULL || row == NULL || length == NULL)
		return false;
	path_length = strlen(path);
	if (path_length > SIZE_MAX - (sizeof(prefix) - 1U)
	    || path_length + sizeof(prefix) - 1U
	    > SIZE_MAX - (sizeof(suffix) - 1U))
		return false;
	needed = sizeof(prefix) - 1U + path_length + sizeof(suffix) - 1U;
	if (needed > capacity)
		return false;
	memcpy(row, prefix, sizeof(prefix) - 1U);
	if (path_length != 0U)
		memcpy(row + sizeof(prefix) - 1U, path, path_length);
	memcpy(row + sizeof(prefix) - 1U + path_length, suffix,
	    sizeof(suffix) - 1U);
	*length = needed;
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
	if (stat(directory, &info) != 0)
		return false;
	return yt_text_stat_is_dir(info.st_mode);
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
text_input_close_execute(struct yt_text_input *input, struct yt_error *error)
{
	FILE *file;
	int saved_errno;

	if (input == NULL) {
		errno = 0;
		set_error(error, YT_INVALID, "sequential INPUT CLOSE", NULL);
		return false;
	}
	input->last_close_basic_error = 0U;
	if (input->file == NULL)
		return true;
	file = input->file;
	input->file = NULL;
	errno = 0;
	if (fclose(file) == 0)
		return true;
	saved_errno = errno;
	input->last_close_basic_error = input->device ? 57U : 70U;
	errno = saved_errno;
	set_error(error, YT_IO_ERROR, "sequential INPUT CLOSE", input->path);
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
text_input_open_execute(struct yt_text_input *input, const char *path,
    struct yt_error *error)
{
	char resolved[512];
	uint16_t dos_error;
	uint16_t basic_error;
	int saved_errno;

	if (input == NULL || path == NULL || input->file != NULL) {
		errno = 0;
		set_error(error, YT_INVALID, "open text input", path);
		return false;
	}
	input->last_open_basic_error = 0U;
	if (!yt_resolve_case_path(path, true, resolved, sizeof(resolved), error))
		return false;
	(void)snprintf(input->path, sizeof(input->path), "%s", resolved);
	errno = 0;
	input->file = fopen(resolved, "rb");
	if (input->file == NULL) {
		saved_errno = errno;
		dos_error = text_output_dos_error(resolved, saved_errno);
		basic_error = dos_error == 5U ? 75U
		    : dos_error == 3U ? 76U : 53U;
		input->last_open_basic_error = basic_error;
		errno = saved_errno;
		set_error(error, basic_error == 53U || basic_error == 76U
		    ? YT_NOT_FOUND : YT_IO_ERROR, "sequential INPUT OPEN",
		    input->path);
		return false;
	}
	input->device = yt_text_isatty(yt_text_fileno(input->file)) != 0;
	return true;
}

static void
text_output_open_failure(struct yt_text_output *output,
    bool append, uint16_t basic_error, int saved_errno,
    struct yt_error *error)
{
	if (!append)
		output->last_output_open_basic_error = basic_error;
	errno = saved_errno;
	set_error(error, basic_error == 53U || basic_error == 76U
	    ? YT_NOT_FOUND : YT_IO_ERROR,
	    append ? "sequential APPEND OPEN" : "sequential OUTPUT OPEN",
	    output->path);
}

static bool
text_output_open_execute(struct yt_text_output *output, const char *path,
    struct yt_error *error)
{
	char resolved[512];
	FILE *temporary = NULL;
	uint16_t basic_error;
	uint16_t dos_error;
	int saved_errno;
	bool reopening = false;

	if (output == NULL || path == NULL || output->file != NULL
	    || output->orphaned_file != NULL) {
		errno = 0;
		set_error(error, YT_INVALID, "open text output", path);
		return false;
	}
	output->last_output_open_basic_error = 0U;
	if (!yt_resolve_case_path(path, true, resolved, sizeof(resolved), error))
		return false;
	(void)snprintf(output->path, sizeof(output->path), "%s", resolved);

open_attempt:
	errno = 0;
	output->file = fopen(resolved, "r+b");
	if (output->file != NULL)
		goto opened;
	saved_errno = errno;
	dos_error = text_output_dos_error(resolved, saved_errno);
	if (!reopening && dos_error == 2U)
		goto create_missing;
	basic_error = dos_error == 5U ? 75U
	    : !reopening && dos_error == 3U ? 76U
	    : reopening && dos_error == 2U ? 53U : 67U;
	text_output_open_failure(output, false, basic_error, saved_errno, error);
	return false;

create_missing:
	errno = 0;
	temporary = fopen(resolved, "w+b");
	if (temporary == NULL) {
		saved_errno = errno;
		dos_error = text_output_dos_error(resolved, saved_errno);
		basic_error = dos_error == 5U ? 75U
		    : dos_error == 2U ? 53U : 67U;
		text_output_open_failure(output, false, basic_error, saved_errno,
		    error);
		return false;
	}
	errno = 0;
	if (fclose(temporary) != 0) {
		saved_errno = errno;
		output->orphaned_file = temporary;
		text_output_open_failure(output, false, 70U, saved_errno, error);
		return false;
	}
	temporary = NULL;
	reopening = true;
	goto open_attempt;

opened:
	output->device = yt_text_isatty(yt_text_fileno(output->file)) != 0;
	output->pending_count = 0U;
	output->last_write_basic_error = 0U;
	output->last_close_basic_error = 0U;
	return true;
}

bool
yt_text_output_open_append(struct yt_text_output *output, const char *path,
    struct yt_error *error)
{
	char resolved[512];
	FILE *temporary = NULL;
	uint8_t window[YT_TEXT_OUTPUT_BUFFER_SIZE];
	uint16_t basic_error;
	uint16_t dos_error;
	int64_t length;
	int64_t start;
	int64_t candidate;
	int saved_errno;
	size_t accepted;
	size_t index;
	bool reopening = false;

	if (output == NULL || path == NULL || output->file != NULL
	    || output->orphaned_file != NULL) {
		errno = 0;
		set_error(error, YT_INVALID, "open text append", path);
		return false;
	}
	output->last_output_open_basic_error = 0U;
	if (!yt_resolve_case_path(path, true, resolved, sizeof(resolved), error))
		return false;
	(void)snprintf(output->path, sizeof(output->path), "%s", resolved);

open_attempt:
	errno = 0;
	output->file = fopen(resolved, "r+b");
	if (output->file != NULL)
		goto opened;
	saved_errno = errno;
	dos_error = text_output_dos_error(resolved, saved_errno);
	if (!reopening && dos_error == 2U)
		goto create_missing;
	basic_error = !reopening && dos_error == 3U ? 76U
	    : reopening && dos_error == 2U ? 53U : 75U;
	text_output_open_failure(output, true, basic_error, saved_errno, error);
	return false;

create_missing:
	errno = 0;
	temporary = fopen(resolved, "w+b");
	if (temporary == NULL) {
		saved_errno = errno;
		dos_error = text_output_dos_error(resolved, saved_errno);
		basic_error = dos_error == 2U ? 53U : 75U;
		text_output_open_failure(output, true, basic_error, saved_errno,
		    error);
		return false;
	}
	errno = 0;
	if (fclose(temporary) != 0) {
		saved_errno = errno;
		output->orphaned_file = temporary;
		text_output_open_failure(output, true, 70U, saved_errno, error);
		return false;
	}
	temporary = NULL;
	reopening = true;
	goto open_attempt;

opened:
	output->device = yt_text_isatty(yt_text_fileno(output->file)) != 0;
	if (output->device) {
		text_output_open_failure(output, true, 57U, 0, error);
		return false;
	}
	errno = 0;
	if (yt_text_fseeko(output->file, 0, SEEK_END) != 0) {
		saved_errno = errno;
		text_output_open_failure(output, true, 52U, saved_errno, error);
		return false;
	}
	length = (int64_t)yt_text_ftello(output->file);
	if (length < 0) {
		saved_errno = errno;
		text_output_open_failure(output, true, 52U, saved_errno, error);
		return false;
	}
	start = length > YT_TEXT_OUTPUT_BUFFER_SIZE
	    ? length - YT_TEXT_OUTPUT_BUFFER_SIZE : 0;
	errno = 0;
	if (yt_text_fseeko(output->file, start, SEEK_SET) != 0) {
		saved_errno = errno;
		text_output_open_failure(output, true, 52U, saved_errno, error);
		return false;
	}
	candidate = start;
	for (;;) {
		errno = 0;
		accepted = fread(window, 1U, sizeof(window), output->file);
		if (ferror(output->file) != 0) {
			saved_errno = errno;
			dos_error = text_output_dos_error(NULL, saved_errno);
			text_output_open_failure(output, true,
			    dos_error == 5U ? 75U : 57U, saved_errno, error);
			return false;
		}
		if (accepted == 0U)
			break;
		for (index = 0U; index < accepted; ++index) {
			if (window[index] == 0x1aU)
				goto selected;
			++candidate;
		}
	}

selected:
	errno = 0;
	if (yt_text_fseeko(output->file, candidate, SEEK_SET) != 0) {
		saved_errno = errno;
		text_output_open_failure(output, true, 52U, saved_errno, error);
		return false;
	}
	output->pending_count = 0U;
	output->last_write_basic_error = 0U;
	output->last_close_basic_error = 0U;
	return true;
}

static void
text_output_write_failure(struct yt_text_output *output, uint16_t basic_error,
    struct yt_error *error)
{
	output->last_write_basic_error = basic_error;
	set_error(error, YT_IO_ERROR, "sequential PRINT", output->path);
}

static bool
text_output_write_cleanup_after_carry(struct yt_text_output *output,
    FILE *file, struct yt_error *error)
{
	int saved_errno;

	output->file = NULL;
	output->pending_count = 0U;
	errno = 0;
	(void)fclose(file);
	saved_errno = errno;
	errno = saved_errno;
	text_output_write_failure(output, 71U, error);
	return false;
}

bool
yt_text_output_write(struct yt_text_output *output, const uint8_t *data,
    size_t length, struct yt_error *error)
{
	FILE *file;
	int saved_errno;
	size_t accepted;
	size_t index;

	if (output == NULL || output->file == NULL
	    || (data == NULL && length != 0U)) {
		errno = 0;
		set_error(error, YT_INVALID, "sequential PRINT",
		    output == NULL ? NULL : output->path);
		return false;
	}
	output->last_write_basic_error = 0U;
	file = output->file;
	for (index = 0U; index < length; ++index) {
		if (output->pending_count == sizeof(output->pending)) {
			output->pending_count = 0U;
			errno = 0;
			accepted = fwrite(output->pending, 1U,
			    sizeof(output->pending), file);
			saved_errno = errno;
			if (ferror(file) != 0) {
				errno = saved_errno;
				return text_output_write_cleanup_after_carry(output,
				    file, error);
			}
			if (accepted != sizeof(output->pending)) {
				output->file = NULL;
				output->orphaned_file = file;
				errno = saved_errno;
				text_output_write_failure(output, 61U, error);
				return false;
			}
		}
		output->pending[output->pending_count++] = data[index];
	}
	return true;
}

static void
text_output_close_failure(struct yt_text_output *output, bool close_all,
    uint16_t basic_error, struct yt_error *error)
{
	output->last_close_basic_error = basic_error;
	set_error(error, YT_IO_ERROR, close_all
	    ? "CLOSE all" : "sequential CLOSE", output->path);
}

static bool
text_output_cleanup_after_carry(struct yt_text_output *output,
    FILE *file, bool close_all, struct yt_error *error)
{
	int saved_errno;

	output->file = NULL;
	output->pending_count = 0U;
	errno = 0;
	(void)fclose(file);
	saved_errno = errno;
	errno = saved_errno;
	text_output_close_failure(output, close_all,
	    output->device ? 57U : 70U, error);
	return false;
}

static bool
text_output_close_write(struct yt_text_output *output, FILE *file,
    const uint8_t *data, size_t length, bool close_all,
    struct yt_error *error)
{
	int saved_errno;
	size_t accepted;

	errno = 0;
	accepted = fwrite(data, 1U, length, file);
	saved_errno = errno;
	if (ferror(file) != 0) {
		errno = saved_errno;
		return text_output_cleanup_after_carry(output, file, close_all,
		    error);
	}
	if (accepted == length)
		return true;
	output->file = NULL;
	output->orphaned_file = file;
	output->pending_count = 0U;
	errno = saved_errno;
	text_output_close_failure(output, close_all, 61U, error);
	return false;
}

static bool
text_output_close_execute(struct yt_text_output *output, bool close_all,
    struct yt_error *error)
{
	static const uint8_t eof_byte = 0x1aU;
	FILE *file;
	int64_t position;
	int result;
	int saved_errno;

	if (output == NULL) {
		errno = 0;
		set_error(error, YT_INVALID,
		    close_all ? "CLOSE all" : "sequential CLOSE", NULL);
		return false;
	}
	output->last_close_basic_error = 0U;
	if (output->file == NULL)
		return true;
	file = output->file;
	if (!output->device) {
		size_t pending_count = output->pending_count;

		output->pending_count = 0U;
		if (!text_output_close_write(output, file, output->pending,
		    pending_count, close_all, error))
			return false;
		if (!text_output_close_write(output, file, &eof_byte, 1U,
		    close_all, error))
			return false;
		errno = 0;
		position = text_output_position(file);
		if (position < 0)
			result = 1;
		else
			result = fflush(file) != 0;
#ifdef _WIN32
		if (!result)
			result = _chsize_s(yt_text_fileno(file), position) != 0;
#else
		if (!result)
			result = ftruncate(yt_text_fileno(file), (off_t)position) != 0;
#endif
		if (result) {
			saved_errno = errno;
			errno = saved_errno;
			return text_output_cleanup_after_carry(output, file,
			    close_all, error);
		}
	}
	output->file = NULL;
	output->pending_count = 0U;
	errno = 0;
	result = fclose(file);
	saved_errno = errno;
	if (result != 0) {
		errno = saved_errno;
		text_output_close_failure(output, close_all,
		    output->device ? 57U : 70U, error);
		return false;
	}
	return true;
}

bool
yt_text_output_close(struct yt_text_output *output, struct yt_error *error)
{
	return text_output_close_execute(output, false, error);
}

bool
yt_text_output_close_all(struct yt_text_output *output,
    struct yt_error *error)
{
	return text_output_close_execute(output, true, error);
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
yt_text_write(const char *path, const uint8_t *data, size_t length,
    bool dos_eof, struct yt_error *error)
{
	struct yt_text_output output;
	char resolved[512];
	FILE *file;
	bool result = false;

	if (dos_eof) {
		yt_text_output_init(&output);
		if (!yt_text_output_open(&output, path, error))
			goto dos_eof_done;
		if (!yt_text_output_write(&output, data, length, error))
			goto dos_eof_done;
		if (!yt_text_output_close(&output, error))
			goto dos_eof_done;
		result = true;
dos_eof_done:
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
	if (length > 0) {
		if (fwrite(data, 1, length, file) != length) {
			set_error(error, YT_IO_ERROR, "write output", resolved);
			return false;
		}
	}
	if (fclose(file) != 0) {
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
	if (!yt_text_output_open_append(&output, path, error))
		goto done;
	if (!yt_text_output_write(&output, line, length, error))
		goto done;
	if (!yt_text_output_write(&output, newline, sizeof(newline), error))
		goto done;
	if (!yt_text_output_close(&output, error))
		goto done;
	result = true;
done:
	yt_text_output_destroy(&output);
	return result;
}
