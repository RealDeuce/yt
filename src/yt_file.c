#include "yt_file.h"
#include "qb.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <io.h>
#define yt_fseeko _fseeki64
#define yt_ftello _ftelli64
#define yt_fdopen _fdopen
#define yt_fileno _fileno
#define yt_isatty _isatty
#define yt_open_fd _open
#define yt_close_fd _close
#define yt_stat_is_dir(mode) (((mode) & _S_IFMT) == _S_IFDIR)
#define YT_OPEN_BINARY _O_BINARY
#else
#include <unistd.h>
#define yt_fseeko fseeko
#define yt_ftello ftello
#define yt_fdopen fdopen
#define yt_fileno fileno
#define yt_isatty isatty
#define yt_open_fd open
#define yt_close_fd close
#define yt_stat_is_dir(mode) S_ISDIR(mode)
#define YT_OPEN_BINARY 0
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
path_exists(const char *path)
{
	struct stat info;

	return stat(path, &info) == 0;
}

bool
yt_resolve_case_path(const char *requested, bool allow_missing,
    char *resolved, size_t size, struct yt_error *error)
{
	const char *slash;
	char directory[512];
	const char *name;
	DIR *dir;
	struct dirent *entry;
	char match[512] = {0};
	unsigned matches = 0;
	int written;

	if (strlen(requested) >= size) {
		set_error(error, YT_RANGE, "resolve path", requested);
		return false;
	}
	if (path_exists(requested) || strchr(requested, '/') == NULL) {
		if (path_exists(requested)) {
			strcpy(resolved, requested);
			return true;
		}
	}

	slash = strrchr(requested, '/');
#ifdef _WIN32
	{
		const char *backslash = strrchr(requested, '\\');
		if (backslash != NULL && (slash == NULL || backslash > slash))
			slash = backslash;
	}
#endif
	if (slash == NULL) {
		strcpy(directory, ".");
		name = requested;
	}
	else {
		size_t length = (size_t)(slash - requested);
		if (length == 0)
			length = 1;
		if (length >= sizeof(directory)) {
			set_error(error, YT_RANGE, "resolve path", requested);
			return false;
		}
		memcpy(directory, requested, length);
		directory[length] = '\0';
		name = slash + 1;
	}

	dir = opendir(directory);
	if (dir != NULL) {
		while ((entry = readdir(dir)) != NULL) {
			if (qb_ascii_equal_nocase(entry->d_name, name)) {
				if (strlen(entry->d_name) >= sizeof(match)) {
					closedir(dir);
					set_error(error, YT_RANGE, "resolve path",
					    requested);
					return false;
				}
				strcpy(match, entry->d_name);
				++matches;
				if (strcmp(entry->d_name, name) == 0) {
					matches = 1;
					break;
				}
			}
		}
		if (match[0] != '\0' && matches == 1) {
			if (slash == NULL)
				written = snprintf(resolved, size, "%s", match);
			else
				written = snprintf(resolved, size, "%s/%s", directory, match);
			closedir(dir);
			if (written >= 0 && (size_t)written < size)
				return true;
			set_error(error, YT_RANGE, "resolve path", requested);
			return false;
		}
		closedir(dir);
		if (matches > 1) {
			set_error(error, YT_INVALID, "ambiguous DOS path", requested);
			return false;
		}
	}
	if (allow_missing) {
		strcpy(resolved, requested);
		return true;
	}
	set_error(error, YT_NOT_FOUND, "resolve path", requested);
	return false;
}

static bool
database_file_length(FILE *file, uint64_t *length)
{
#ifdef _WIN32
	struct _stat64 info;

	if (_fstat64(yt_fileno(file), &info) != 0 || info.st_size < 0)
		return false;
#else
	struct stat info;

	if (fstat(yt_fileno(file), &info) != 0 || info.st_size < 0)
		return false;
#endif
	*length = (uint64_t)info.st_size;
	return true;
}

static bool
database_parent_exists(const char *path)
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
	return stat(directory, &info) == 0 && yt_stat_is_dir(info.st_mode);
}

static uint16_t
database_dos_error(const char *path, int system_error)
{
	switch (system_error) {
	case ENOENT:
		return database_parent_exists(path) ? 2U : 3U;
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

static FILE *
database_open_fd(const char *path, uint8_t access, bool create)
{
	const char *stream_mode;
	int flags = YT_OPEN_BINARY;
	int descriptor;
	int saved_errno;

	if (create) {
		flags |= O_RDWR | O_CREAT | O_TRUNC;
		stream_mode = "r+b";
	}
	else if (access == 2U) {
		flags |= O_RDWR;
		stream_mode = "r+b";
	}
	else if (access == 1U) {
		flags |= O_WRONLY;
		stream_mode = "wb";
	}
	else {
		flags |= O_RDONLY;
		stream_mode = "rb";
	}
#ifdef _WIN32
	descriptor = create
	    ? yt_open_fd(path, flags, _S_IREAD | _S_IWRITE)
	    : yt_open_fd(path, flags);
#else
	descriptor = create ? yt_open_fd(path, flags, 0666)
	    : yt_open_fd(path, flags);
#endif
	if (descriptor < 0)
		return NULL;
	{
		FILE *file = yt_fdopen(descriptor, stream_mode);

		if (file != NULL)
			return file;
	}
	saved_errno = errno;
	(void)yt_close_fd(descriptor);
	errno = saved_errno;
	return NULL;
}

static void
database_open_failure(struct yt_database *database,
    uint16_t basic_error, struct yt_error *error)
{
	database->last_open_basic_error = basic_error;
	set_error(error, basic_error == 53U ? YT_NOT_FOUND : YT_IO_ERROR,
	    "random OPEN", database->path);
}

static bool
database_open_random(struct yt_database *database, const char *path,
    size_t record_size, struct yt_error *error)
{
	uint64_t length;
	uint8_t access = 2U;
	size_t access_attempts = 0U;
	bool created = false;

	for (;;) {
		uint16_t dos_error;
		int saved_errno;

		if (access_attempts++ >= 6U) {
			database_open_failure(database, 75U, error);
			return false;
		}
		errno = 0;
		database->file = database_open_fd(path, access, false);
		if (database->file != NULL)
			break;
		saved_errno = errno;
		dos_error = database_dos_error(path, saved_errno);
		if (dos_error == 5U && access > 0U) {
			--access;
			continue;
		}
		if (!created && dos_error == 2U) {
			FILE *temporary;

			errno = 0;
			temporary = database_open_fd(path, 2U, true);
			if (temporary == NULL) {
				saved_errno = errno;
				dos_error = database_dos_error(path, saved_errno);
				database_open_failure(database,
				    dos_error == 2U ? 53U : 75U, error);
				return false;
			}
			errno = 0;
			if (fclose(temporary) != 0) {
				saved_errno = errno;
				database_open_failure(database, 70U, error);
				return false;
			}
			created = true;
			access = 2U;
			continue;
		}
		database_open_failure(database,
		    dos_error == 2U ? 53U : dos_error == 3U ? 76U : 75U,
		    error);
		return false;
	}

	database->device = yt_isatty(yt_fileno(database->file)) != 0;
	if (!database_file_length(database->file, &length)) {
		database_open_failure(database, 57U, error);
		return false;
	}
	database->records = (size_t)(length / record_size);
	return true;
}

static bool
database_open_sized(struct yt_database *database, const char *path,
    enum yt_open_mode mode, size_t record_size, struct yt_error *error)
{
	char resolved[sizeof(database->path)];
	const char *file_mode;
	uint64_t length;

	if (database == NULL || path == NULL || record_size == 0U) {
		set_error(error, YT_INVALID, "database open", path);
		return false;
	}
	memset(database, 0, sizeof(*database));
	if (!yt_resolve_case_path(path, mode == YT_OPEN_CREATE
	    || mode == YT_OPEN_UPDATE_CREATE, resolved,
	    sizeof(resolved), error))
		return false;
	(void)snprintf(database->path, sizeof(database->path), "%s", resolved);
	if (mode == YT_OPEN_UPDATE_CREATE)
		return database_open_random(database, resolved, record_size, error);
	switch (mode) {
	case YT_OPEN_READ:
		file_mode = "rb";
		break;
	case YT_OPEN_UPDATE:
		file_mode = "r+b";
		break;
	case YT_OPEN_CREATE:
		file_mode = "w+b";
		break;
	default:
		set_error(error, YT_INVALID, "database mode", path);
		return false;
	}
	database->file = fopen(resolved, file_mode);
	if (database->file == NULL) {
		set_error(error, errno == ENOENT ? YT_NOT_FOUND : YT_IO_ERROR,
		    "open database", resolved);
		return false;
	}
	if (!database_file_length(database->file, &length)) {
		set_error(error, YT_IO_ERROR, "size database", resolved);
		yt_database_close(database);
		return false;
	}
	database->records = (size_t)(length / record_size);
	return true;
}

bool
yt_database_open(struct yt_database *database, const char *path,
    enum yt_open_mode mode, struct yt_error *error)
{
	return database_open_sized(database, path, mode, YT_RECORD_SIZE, error);
}

void
yt_database_close(struct yt_database *database)
{
	FILE *file;

	if (database == NULL)
		return;
	file = database->file;
	database->file = NULL;
	database->records = 0U;
	if (file != NULL)
		(void)fclose(file);
}

static bool database_seek(FILE *file, int64_t absolute_offset);

bool
yt_database_read(struct yt_database *database, size_t basic_record,
    struct yt_record *record, struct yt_error *error)
{
	bool read = yt_database_random_get(database, basic_record, record, NULL,
	    error);

	if (!read && error != NULL)
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    "read record");
	return read;
}

static void
database_prepare_io(FILE *file)
{
	/* DOS carry describes one call; C stream errors and errno are sticky. */
	clearerr(file);
	errno = 0;
}

static bool
database_random_get_bytes(struct yt_database *database, size_t basic_record,
    uint8_t *data, size_t record_size, size_t *accepted,
    struct yt_error *error)
{
	int64_t offset;
	size_t read_count;
	int saved_errno;
	uint16_t dos_error;

	if (accepted != NULL)
		*accepted = 0U;
	if (database == NULL || database->file == NULL || data == NULL
	    || record_size == 0U) {
		set_error(error, YT_INVALID, "random GET",
		    database != NULL ? database->path : NULL);
		return false;
	}
	database->last_get_basic_error = 0U;
	if (basic_record == 0U || basic_record > 0xFFFFFFU) {
		database->last_get_basic_error = 63U;
		set_error(error, YT_RANGE, "random GET", database->path);
		return false;
	}
	offset = (int64_t)((uint64_t)(basic_record - 1U) * record_size);
	if (!database_seek(database->file, offset)) {
		database->last_get_basic_error = 52U;
		set_error(error, YT_IO_ERROR, "random GET seek", database->path);
		return false;
	}
	memset(data, 0, record_size);
	database_prepare_io(database->file);
	read_count = fread(data, 1U, record_size, database->file);
	saved_errno = errno;
	if (ferror(database->file) != 0) {
		dos_error = (saved_errno == EACCES
		    || saved_errno == EPERM) ? 5U : 1U;
		database->last_get_basic_error = dos_error == 5U ? 70U : 57U;
		if (accepted != NULL)
			*accepted = read_count;
		errno = saved_errno;
		set_error(error, YT_IO_ERROR, "random GET", database->path);
		return false;
	}
	errno = saved_errno;
	if (accepted != NULL)
		*accepted = read_count;
	return true;
}

bool
yt_database_random_get(struct yt_database *database, size_t basic_record,
    struct yt_record *record, size_t *accepted, struct yt_error *error)
{
	return database_random_get_bytes(database, basic_record,
	    record != NULL ? record->bytes : NULL, YT_RECORD_SIZE, accepted,
	    error);
}

bool
yt_database_write(struct yt_database *database, size_t basic_record,
    const struct yt_record *record, struct yt_error *error)
{
	bool written = yt_database_random_put(database, basic_record, record,
	    false, NULL, error);

	if (!written && error != NULL)
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    "write record");
	return written;
}

bool
yt_database_write_durable(struct yt_database *database, size_t basic_record,
    const struct yt_record *record, struct yt_error *error)
{
	return yt_database_write(database, basic_record, record, error)
	    && yt_database_flush(database, error);
}

static bool
database_seek(FILE *file, int64_t absolute_offset)
{
	int saved_errno;

	database_prepare_io(file);
	if (yt_fseeko(file, absolute_offset, SEEK_SET) == 0)
		return true;
	saved_errno = errno;
	errno = saved_errno;
	return false;
}

static bool
database_close_execute(struct yt_database *database, bool close_all,
    struct yt_error *error)
{
	FILE *file;
	int saved_errno;

	if (database == NULL) {
		set_error(error, YT_INVALID,
		    close_all ? "CLOSE all" : "random CLOSE", NULL);
		return false;
	}
	database->last_close_basic_error = 0U;
	if (database->file == NULL)
		return true;
	file = database->file;
	database->file = NULL;
	database->records = 0U;
	errno = 0;
	if (fclose(file) == 0)
		return true;
	saved_errno = errno;
	database->last_close_basic_error = database->device ? 57U : 70U;
	errno = saved_errno;
	set_error(error, YT_IO_ERROR,
	    close_all ? "CLOSE all" : "random CLOSE", database->path);
	return false;
}

bool
yt_database_random_close(struct yt_database *database, struct yt_error *error)
{
	return database_close_execute(database, false, error);
}

bool
yt_database_close_all_single(struct yt_database *database,
    struct yt_error *error)
{
	return database_close_execute(database, true, error);
}

static bool
database_lof_fail(struct yt_database *database, struct yt_error *error)
{
	int saved_errno = errno;

	database->last_lof_basic_error = 52U;
	errno = saved_errno;
	set_error(error, YT_IO_ERROR, "random LOF seek", database->path);
	return false;
}

bool
yt_database_random_lof(struct yt_database *database, uint32_t *length,
    struct yt_error *error)
{
	int64_t position;
	uint32_t saved_position;
	uint32_t file_length;

	if (length != NULL)
		*length = 0U;
	if (database == NULL || database->file == NULL || length == NULL) {
		set_error(error, YT_INVALID, "random LOF", database != NULL
		    ? database->path : NULL);
		return false;
	}
	database->last_lof_basic_error = 0U;
	if (database->device) {
		*length = database->device_position;
		return true;
	}

	database_prepare_io(database->file);
	if (yt_fseeko(database->file, 0, SEEK_CUR) != 0
	    || (position = yt_ftello(database->file)) < 0
	    || (uint64_t)position > UINT32_MAX)
		return database_lof_fail(database, error);
	saved_position = (uint32_t)position;

	database_prepare_io(database->file);
	if (yt_fseeko(database->file, 0, SEEK_END) != 0
	    || (position = yt_ftello(database->file)) < 0
	    || (uint64_t)position > UINT32_MAX)
		return database_lof_fail(database, error);
	file_length = (uint32_t)position;

	database_prepare_io(database->file);
	if (yt_fseeko(database->file, (int64_t)saved_position, SEEK_SET) != 0)
		return database_lof_fail(database, error);
	*length = file_length;
	return true;
}

static void
database_reject_short(struct yt_database *database, struct yt_error *error)
{
	FILE *file = database->file;
	int saved_errno;

	database->file = NULL;
	database->records = 0U;
	errno = 0;
	(void)fclose(file);
	saved_errno = errno;
	database->last_put_basic_error = 61U;
	errno = saved_errno;
	set_error(error, YT_IO_ERROR, "random PUT rejected short", database->path);
}

static bool
database_random_put_bytes(struct yt_database *database, size_t basic_record,
    const uint8_t *data, size_t record_size, bool one_byte_short_ok,
    size_t *accepted, struct yt_error *error)
{
	int64_t offset;
	size_t write_count;
	int saved_errno;
	uint16_t dos_error;
	bool tolerated_short;

	if (accepted != NULL)
		*accepted = 0U;
	if (database == NULL || database->file == NULL || data == NULL
	    || record_size == 0U) {
		set_error(error, YT_INVALID, "random PUT",
		    database != NULL ? database->path : NULL);
		return false;
	}
	database->last_put_basic_error = 0U;

	if (basic_record == 0U || basic_record > 0xFFFFFFU) {
		database->last_put_basic_error = 63U;
		set_error(error, YT_RANGE, "random PUT", database->path);
		return false;
	}
	offset = (int64_t)((uint64_t)(basic_record - 1U) * record_size);
	if (!database_seek(database->file, offset)) {
		database->last_put_basic_error = 52U;
		set_error(error, YT_IO_ERROR, "random PUT seek", database->path);
		return false;
	}
	database_prepare_io(database->file);
	write_count = fwrite(data, 1U, record_size, database->file);
	saved_errno = errno;
	if (accepted != NULL)
		*accepted = write_count;
	if (ferror(database->file) != 0) {
		dos_error = (saved_errno == EACCES
		    || saved_errno == EPERM) ? 5U : 1U;
		database->last_put_basic_error = dos_error == 5U ? 70U : 57U;
		errno = saved_errno;
		set_error(error, YT_IO_ERROR, "random PUT", database->path);
		return false;
	}
	errno = saved_errno;
	tolerated_short = one_byte_short_ok
	    && write_count == record_size - 1U;
	if (write_count != record_size && !tolerated_short) {
		database_reject_short(database, error);
		return false;
	}
	if (basic_record > database->records)
		database->records = basic_record;
	return true;
}

bool
yt_database_random_put(struct yt_database *database, size_t basic_record,
    const struct yt_record *record, bool one_byte_short_ok, size_t *accepted,
    struct yt_error *error)
{
	return database_random_put_bytes(database, basic_record,
	    record != NULL ? record->bytes : NULL, YT_RECORD_SIZE,
	    one_byte_short_ok, accepted, error);
}

bool
yt_database_flush(struct yt_database *database, struct yt_error *error)
{
	if (database == NULL || database->file == NULL) {
		set_error(error, YT_INVALID, "flush database",
		    database != NULL ? database->path : NULL);
		return false;
	}
	if (fflush(database->file) != 0) {
		set_error(error, YT_IO_ERROR, "flush database", database->path);
		return false;
	}
	return true;
}

void
yt_radio_file_init(struct yt_radio_file *radio)
{
	if (radio != NULL)
		memset(radio, 0, sizeof(*radio));
}

bool
yt_radio_file_close(struct yt_radio_file *radio, struct yt_error *error)
{
	bool result;

	if (radio == NULL) {
		set_error(error, YT_INVALID, "close radio", NULL);
		return false;
	}
	result = yt_database_random_close(&radio->random, error);
	radio->record_length = 0U;
	radio->field_count = 0U;
	memset(radio->fields, 0, sizeof(radio->fields));
	return result;
}

bool
yt_radio_file_open(struct yt_radio_file *radio, const char *path,
    struct yt_error *error)
{
	return yt_radio_file_open_text_width(radio, path, 74U, error);
}

bool
yt_radio_file_open_text_width(struct yt_radio_file *radio, const char *path,
    size_t text_width, struct yt_error *error)
{
	const struct yt_radio_field fields[YT_RADIO_FIELD_COUNT] = {
		{0U, 4U},
		{4U, 4U},
		{8U, 4U},
		{12U, text_width},
	};
	if (radio == NULL || path == NULL) {
		set_error(error, YT_INVALID, "open radio", path);
		return false;
	}
	if (text_width > YT_RADIO_RECORD_SIZE - 12U) {
		set_error(error, YT_RANGE, "radio FIELD", path);
		return false;
	}
	if (!yt_radio_file_close(radio, error))
		return false;
	if (!database_open_sized(&radio->random, path,
	    YT_OPEN_UPDATE_CREATE, YT_RADIO_RECORD_SIZE, error))
		return false;
	radio->record_length = YT_RADIO_RECORD_SIZE;
	memcpy(radio->fields, fields, sizeof(fields));
	radio->field_count = YT_RADIO_FIELD_COUNT;
	return true;
}

bool
yt_radio_file_size(struct yt_radio_file *radio, uint64_t *size,
    struct yt_error *error)
{
	uint32_t length;

	if (radio == NULL || radio->random.file == NULL || size == NULL) {
		set_error(error, YT_INVALID, "radio LOF", radio != NULL
		    ? radio->random.path : NULL);
		return false;
	}
	if (!yt_database_random_lof(&radio->random, &length, error))
		return false;
	*size = length;
	return true;
}

bool
yt_radio_file_get(struct yt_radio_file *radio, uint32_t basic_record,
    struct yt_radio_record *record, size_t *accepted,
    struct yt_error *error)
{
	if (accepted != NULL)
		*accepted = 0U;
	if (radio == NULL || radio->random.file == NULL || record == NULL) {
		set_error(error, YT_RANGE, "radio GET", radio != NULL
		    ? radio->random.path : NULL);
		return false;
	}
	return database_random_get_bytes(&radio->random, basic_record,
	    record->bytes, YT_RADIO_RECORD_SIZE, accepted, error);
}

bool
yt_radio_file_put(struct yt_radio_file *radio, uint32_t basic_record,
    const struct yt_radio_record *record, struct yt_error *error)
{
	if (radio == NULL || radio->random.file == NULL || record == NULL) {
		set_error(error, YT_RANGE, "radio PUT", radio != NULL
		    ? radio->random.path : NULL);
		return false;
	}
	return database_random_put_bytes(&radio->random, basic_record,
	    record->bytes, YT_RADIO_RECORD_SIZE, false, NULL, error);
}

bool
yt_radio_file_next_record(struct yt_radio_file *radio,
    uint32_t *basic_record, struct yt_error *error)
{
	uint64_t length;
	uint64_t record;

	if (basic_record == NULL || !yt_radio_file_size(radio, &length, error))
		return false;
	if (length % YT_RADIO_RECORD_SIZE != 0U) {
		set_error(error, YT_RANGE, "radio record number",
		    radio->random.path);
		return false;
	}
	record = length / YT_RADIO_RECORD_SIZE + 1U;
	if (record > 0xFFFFFFU) {
		set_error(error, YT_RANGE, "radio record number",
		    radio->random.path);
		return false;
	}
	*basic_record = (uint32_t)record;
	return true;
}

bool
yt_file_kill(const char *path, struct yt_error *error)
{
	char resolved[512];

	if (path == NULL) {
		errno = 0;
		set_error(error, YT_INVALID, "KILL", NULL);
		return false;
	}
	if (!yt_resolve_case_path(path, false, resolved, sizeof(resolved),
	    error))
		return false;
	if (remove(resolved) != 0) {
		set_error(error, errno == ENOENT ? YT_NOT_FOUND : YT_IO_ERROR,
		    "KILL", resolved);
		return false;
	}
	return true;
}

bool
yt_file_rename(const char *old_path, const char *new_path,
    struct yt_error *error)
{
	char resolved_old[512];
	char resolved_new[512];

	if (old_path == NULL || new_path == NULL) {
		errno = 0;
		set_error(error, YT_INVALID, "NAME", NULL);
		return false;
	}
	if (!yt_resolve_case_path(old_path, false, resolved_old,
	    sizeof(resolved_old), error)
	    || !yt_resolve_case_path(new_path, true, resolved_new,
	    sizeof(resolved_new), error))
		return false;
	if (path_exists(resolved_new)) {
		errno = EEXIST;
		set_error(error, YT_IO_ERROR, "NAME", resolved_new);
		return false;
	}
	if (rename(resolved_old, resolved_new) != 0) {
		set_error(error, errno == ENOENT && !path_exists(resolved_old)
		    ? YT_NOT_FOUND : YT_IO_ERROR, "NAME", resolved_old);
		return false;
	}
	return true;
}

bool
yt_file_size(const char *path, size_t *size, struct yt_error *error)
{
	char resolved[512];
	struct stat info;

	if (!yt_resolve_case_path(path, false, resolved, sizeof(resolved), error))
		return false;
	if (stat(resolved, &info) != 0 || info.st_size < 0) {
		set_error(error, YT_IO_ERROR, "file size", resolved);
		return false;
	}
	*size = (size_t)info.st_size;
	return true;
}
