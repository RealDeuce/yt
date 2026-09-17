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
		input->logical_position = 0U;
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
		++input->logical_position;
		if (value == '\r') {
			uint8_t following;
			bool following_eof;

			if (!text_input_get_byte(input, &following,
			    &following_eof, error))
				return false;
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
	if (!*eof)
		++input->logical_position;
	return true;
}

static void
text_input_unread_byte(struct yt_text_input *input)
{
	++input->read_remaining;
	--input->logical_position;
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
			if (byte != 0U
			    && !text_input_token_append(input, &used, byte, error))
				goto failed;
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

struct text_output_write_observation {
	size_t accepted;
	bool carry;
	bool handle_open;
	uint16_t dos_error;
	int64_t terminal_position;
};

static bool
text_output_write_default_common(FILE *file, const uint8_t *data,
    size_t requested, struct text_output_write_observation *observation)
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
text_output_write_perform(FILE *file, const uint8_t *data,
    size_t requested, struct text_output_write_observation *observation)
{
	bool delivered;

	delivered = text_output_write_default_common(file, data, requested,
	    observation);
	if (delivered && observation->carry) {
		observation->accepted = 0U;
		observation->terminal_position = -1;
	}
	return delivered;
}

struct text_close_observation {
	size_t accepted;
	bool carry;
	bool handle_open;
	uint16_t dos_error;
	int64_t terminal_position;
};

static bool
text_close_perform(FILE *file,
    enum yt_text_close_operation operation, const uint8_t *data,
    size_t requested, struct text_close_observation *observation)
{
	struct text_output_write_observation write;
	int result;
	int saved_errno;

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
text_open_perform(const char *path,
    enum yt_text_open_operation operation, uint8_t access,
    FILE *active_file, int64_t offset, uint8_t *data, size_t requested,
    uint16_t prior_dos_error,
    struct yt_text_open_observation *observation)
{
	int64_t position;
	int result;
	int saved_errno;

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
	bool delivered;

	++output->last_append_open.operation_count;
	memset(observation, 0, sizeof(*observation));
	observation->terminal_position = -1;
	delivered = text_open_perform(path, operation, access, active_file,
	    offset, data, requested, prior_dos_error, observation);
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

static bool
text_output_open_observe(struct yt_text_output *output, const char *path,
    enum yt_text_open_operation operation, uint8_t access,
    FILE *active_file, uint16_t prior_dos_error,
    struct yt_text_open_observation *observation)
{
	bool delivered;

	++output->last_output_open.operation_count;
	memset(observation, 0, sizeof(*observation));
	observation->terminal_position = -1;
	delivered = text_open_perform(path, operation, access, active_file,
	    0, NULL, 0U, prior_dos_error, observation);
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
		output->last_output_open.temporary_close_retry_dos_error
		    = observation.dos_error;
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
	output->device = observation.device;
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
	output->last_write_basic_error = 0U;
	output->last_close_basic_error = 0U;
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
		output->last_append_open.temporary_close_retry_dos_error
		    = observation.dos_error;
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
	output->device = observation.device;
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
	struct text_close_observation cleanup;

	output->file = NULL;
	output->pending_count = 0U;
	memset(&cleanup, 0, sizeof(cleanup));
	cleanup.terminal_position = -1;
	if (!text_close_perform(file,
	    YT_TEXT_CLOSE_CLEANUP_HANDLE, NULL, 0U, &cleanup)) {
		output->orphaned_file = file;
		text_output_write_failure(output, 57U, error);
		return false;
	}
	if (cleanup.handle_open)
		output->orphaned_file = file;
	text_output_write_failure(output, 71U, error);
	return false;
}

bool
yt_text_output_write(struct yt_text_output *output, const uint8_t *data,
    size_t length, struct yt_error *error)
{
	struct text_output_write_observation observation;
	FILE *file;
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
			memset(&observation, 0, sizeof(observation));
			observation.terminal_position = -1;
			if (!text_output_write_perform(file, output->pending,
			    sizeof(output->pending), &observation)) {
				text_output_write_failure(output, 57U, error);
				return false;
			}
			if (observation.carry)
				return text_output_write_cleanup_after_carry(output,
				    file, error);
			if (observation.accepted != sizeof(output->pending)) {
				output->file = NULL;
				output->orphaned_file = file;
				text_output_write_failure(output, 61U, error);
				return false;
			}
		}
		output->pending[output->pending_count++] = data[index];
	}
	return true;
}

static bool
text_output_close_observe(FILE *file,
    enum yt_text_close_operation operation, const uint8_t *data,
    size_t requested, struct text_close_observation *observation)
{
	memset(observation, 0, sizeof(*observation));
	observation->terminal_position = -1;
	return text_close_perform(file, operation, data, requested,
	    observation);
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
    FILE *file, bool handle_open, bool close_all, struct yt_error *error)
{
	struct text_close_observation cleanup;

	output->file = NULL;
	output->pending_count = 0U;
	if (!text_output_close_observe(
	    handle_open ? file : NULL, YT_TEXT_CLOSE_CLEANUP_HANDLE,
	    NULL, 0U, &cleanup)) {
		if (handle_open)
			output->orphaned_file = file;
		text_output_close_failure(output, close_all, 57U, error);
		return false;
	}
	if (cleanup.handle_open)
		output->orphaned_file = file;
	text_output_close_failure(output, close_all,
	    output->device ? 57U : 70U, error);
	return false;
}

static bool
text_output_close_execute(struct yt_text_output *output, bool close_all,
    struct yt_error *error)
{
	struct text_close_observation observation;
	FILE *file;
	const uint8_t *data;
	size_t requested;
	enum yt_text_close_operation operation;
	static const uint8_t eof_byte = 0x1aU;

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
	for (operation = output->device ? YT_TEXT_CLOSE_HANDLE
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
		if (!text_output_close_observe(file, operation, data, requested,
		    &observation)) {
			text_output_close_failure(output, close_all, 57U, error);
			return false;
		}
		if (observation.carry)
			return text_output_cleanup_after_carry(output, file,
			    observation.handle_open, close_all, error);
		if (observation.accepted != requested) {
			output->file = NULL;
			output->orphaned_file = file;
			text_output_close_failure(output, close_all, 61U, error);
			return false;
		}
	}
	output->file = NULL;
	output->pending_count = 0U;
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
