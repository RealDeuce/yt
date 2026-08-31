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

static bool
database_open_default(void *context, const char *path,
    enum yt_database_open_operation operation, uint8_t access,
    FILE *active_file, uint16_t prior_dos_error,
    struct yt_database_open_observation *observation)
{
	int result;
	int saved_errno;

	(void)context;
	memset(observation, 0, sizeof(*observation));
	switch (operation) {
	case YT_DATABASE_OPEN_EXISTING:
	case YT_DATABASE_OPEN_CREATE:
		errno = 0;
		observation->file = database_open_fd(path, access,
		    operation == YT_DATABASE_OPEN_CREATE);
		if (observation->file == NULL) {
			observation->carry = true;
			observation->dos_error = database_dos_error(path, errno);
		}
		else
			observation->handle_open = true;
		return true;
	case YT_DATABASE_OPEN_TEMP_CLOSE:
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
		    ? database_dos_error(NULL, saved_errno) : 0U;
		observation->handle_open = false;
		errno = saved_errno;
		return true;
	case YT_DATABASE_OPEN_QUERY_DEVICE:
		if (active_file == NULL)
			return false;
		observation->device = yt_isatty(yt_fileno(active_file)) != 0;
		observation->handle_open = true;
		return true;
	case YT_DATABASE_OPEN_CONFIGURE_DEVICE:
		if (active_file == NULL)
			return false;
		observation->handle_open = true;
		return true;
	case YT_DATABASE_OPEN_EXTENDED_ERROR:
		if (prior_dos_error != 5U)
			return false;
		observation->mapped_error = 75U;
		return true;
	default:
		return false;
	}
}

static void
database_open_failure(struct yt_database *database,
    enum yt_database_open_outcome outcome, uint16_t basic_error,
    uint16_t dos_error, struct yt_error *error)
{
	database->last_open.outcome = outcome;
	database->last_open.basic_error = basic_error;
	database->last_open.dos_error = dos_error;
	database->last_open.registered = database->file != NULL;
	database->last_open.handle_open = database->file != NULL
	    || database->orphaned_file != NULL;
	set_error(error, basic_error == 53U ? YT_NOT_FOUND : YT_IO_ERROR,
	    "random OPEN", database->path);
}

static bool
database_open_observe(struct yt_database *database, const char *path,
    enum yt_database_open_operation operation, uint8_t access,
    FILE *active_file, uint16_t prior_dos_error,
    yt_database_open_provider provider, void *context,
    struct yt_database_open_observation *observation)
{
	++database->last_open.operation_count;
	memset(observation, 0, sizeof(*observation));
	return provider(context, path, operation, access, active_file,
	    prior_dos_error, observation);
}

static bool
database_open_observation_valid(enum yt_database_open_operation operation,
    const struct yt_database_open_observation *observation)
{
	if (operation == YT_DATABASE_OPEN_EXTENDED_ERROR)
		return !observation->carry && observation->file == NULL
		    && observation->dos_error == 0U && !observation->device
		    && !observation->handle_open
		    && (observation->mapped_error == 70U
		    || observation->mapped_error == 75U);
	if (operation == YT_DATABASE_OPEN_EXISTING
	    || operation == YT_DATABASE_OPEN_CREATE) {
		if (observation->carry)
			return observation->file == NULL
			    && !observation->handle_open
			    && observation->dos_error >= 1U
			    && observation->dos_error <= 0xffU
			    && observation->mapped_error == 0U;
		return observation->file != NULL && observation->dos_error == 0U
		    && observation->mapped_error == 0U
		    && observation->handle_open;
	}
	if (operation == YT_DATABASE_OPEN_TEMP_CLOSE) {
		if (observation->carry)
			return observation->file == NULL
			    && observation->dos_error >= 1U
			    && observation->dos_error <= 0xffU
			    && observation->mapped_error == 0U;
		return observation->file == NULL
		    && observation->dos_error == 0U
		    && observation->mapped_error == 0U
		    && !observation->handle_open;
	}
	if (operation == YT_DATABASE_OPEN_QUERY_DEVICE)
		return observation->file == NULL && observation->mapped_error == 0U
		    && observation->handle_open;
	if (operation == YT_DATABASE_OPEN_CONFIGURE_DEVICE) {
		if (observation->carry)
			return observation->file == NULL
			    && observation->dos_error >= 1U
			    && observation->dos_error <= 0xffU
			    && observation->mapped_error == 0U
			    && observation->handle_open;
		return observation->file == NULL
		    && observation->dos_error == 0U
		    && observation->mapped_error == 0U
		    && observation->handle_open;
	}
	return false;
}

static bool
database_open_extended_error(struct yt_database *database, const char *path,
    uint16_t dos_error, enum yt_database_open_outcome outcome,
    yt_database_open_provider provider, void *context, struct yt_error *error)
{
	struct yt_database_open_observation observation;

	if (!database_open_observe(database, path,
	    YT_DATABASE_OPEN_EXTENDED_ERROR, 0U, NULL, dos_error,
	    provider, context, &observation)
	    || !database_open_observation_valid(
	    YT_DATABASE_OPEN_EXTENDED_ERROR, &observation)) {
		database_open_failure(database, YT_DATABASE_OPEN_PROVIDER_ERROR,
		    75U, dos_error, error);
		return false;
	}
	database_open_failure(database, outcome, observation.mapped_error,
	    dos_error, error);
	return false;
}

static bool
database_open_random(struct yt_database *database, const char *path,
    yt_database_open_provider provider, void *context, struct yt_error *error)
{
	struct yt_database_open_observation observation;
	FILE *temporary = NULL;
	uint64_t length;
	uint16_t first_close_error;
	uint8_t access = 2U;
	bool reopening = false;
	bool temporary_open = false;

open_attempt:
	if (database->last_open.access_attempt_count
	    >= YT_ARRAY_LEN(database->last_open.access_attempts)) {
		database_open_failure(database, YT_DATABASE_OPEN_PROVIDER_ERROR,
		    75U, 0U, error);
		return false;
	}
	database->last_open.access_attempts[
	    database->last_open.access_attempt_count++] = access;
	if (!database_open_observe(database, path,
	    YT_DATABASE_OPEN_EXISTING, access, NULL, 0U,
	    provider, context, &observation)
	    || !database_open_observation_valid(YT_DATABASE_OPEN_EXISTING,
	    &observation)) {
		if (observation.file != NULL && observation.handle_open)
			database->orphaned_file = observation.file;
		database_open_failure(database, YT_DATABASE_OPEN_PROVIDER_ERROR,
		    75U, observation.dos_error, error);
		return false;
	}
	if (!observation.carry) {
		database->file = observation.file;
		goto opened;
	}
	if (observation.dos_error == 5U && access > 0U) {
		--access;
		goto open_attempt;
	}
	if (observation.dos_error == 5U)
		return database_open_extended_error(database, path,
		    observation.dos_error, reopening
		    ? YT_DATABASE_OPEN_REOPEN_ERROR
		    : YT_DATABASE_OPEN_INITIAL_ERROR,
		    provider, context, error);
	if (!reopening && observation.dos_error == 2U)
		goto create_missing;
	if (!reopening && observation.dos_error == 3U) {
		database_open_failure(database, YT_DATABASE_OPEN_INITIAL_ERROR,
		    76U, observation.dos_error, error);
		return false;
	}
	if (reopening && observation.dos_error == 2U) {
		database_open_failure(database, YT_DATABASE_OPEN_REOPEN_ERROR,
		    53U, observation.dos_error, error);
		return false;
	}
	database_open_failure(database, reopening
	    ? YT_DATABASE_OPEN_REOPEN_ERROR : YT_DATABASE_OPEN_INITIAL_ERROR,
	    75U, observation.dos_error, error);
	return false;

create_missing:
	if (!database_open_observe(database, path, YT_DATABASE_OPEN_CREATE,
	    2U, NULL, 0U, provider, context, &observation)
	    || !database_open_observation_valid(YT_DATABASE_OPEN_CREATE,
	    &observation)) {
		if (observation.file != NULL && observation.handle_open)
			database->orphaned_file = observation.file;
		database_open_failure(database, YT_DATABASE_OPEN_PROVIDER_ERROR,
		    75U, observation.dos_error, error);
		return false;
	}
	if (observation.carry) {
		if (observation.dos_error == 5U)
			return database_open_extended_error(database, path,
			    observation.dos_error, YT_DATABASE_OPEN_CREATE_ERROR,
			    provider, context, error);
		database_open_failure(database, YT_DATABASE_OPEN_CREATE_ERROR,
		    observation.dos_error == 2U ? 53U : 75U,
		    observation.dos_error, error);
		return false;
	}
	database->last_open.created = true;
	temporary = observation.file;
	temporary_open = true;
	database->last_open.temporary_close_attempted = true;
	{
		bool observed = database_open_observe(database, path,
		    YT_DATABASE_OPEN_TEMP_CLOSE, 0U, temporary, 0U,
		    provider, context, &observation);

		if (observed)
			temporary_open = observation.handle_open;
		if (!observed || !database_open_observation_valid(
		    YT_DATABASE_OPEN_TEMP_CLOSE, &observation)) {
			if (temporary_open)
				database->orphaned_file = temporary;
			database_open_failure(database,
			    YT_DATABASE_OPEN_PROVIDER_ERROR, 75U,
			    observation.dos_error, error);
			return false;
		}
	}
	if (observation.carry) {
		first_close_error = observation.dos_error;
		database->last_open.temporary_close_retried = true;
		{
			bool observed = database_open_observe(database, path,
			    YT_DATABASE_OPEN_TEMP_CLOSE, 0U,
			    temporary_open ? temporary : NULL,
			    first_close_error, provider, context, &observation);

			if (observed)
				temporary_open = observation.handle_open;
			if (!observed || !database_open_observation_valid(
			    YT_DATABASE_OPEN_TEMP_CLOSE, &observation)) {
				if (temporary_open)
					database->orphaned_file = temporary;
				database_open_failure(database,
				    YT_DATABASE_OPEN_PROVIDER_ERROR, 75U,
				    first_close_error, error);
				return false;
			}
		}
		if (temporary_open)
			database->orphaned_file = temporary;
		database_open_failure(database,
		    YT_DATABASE_OPEN_TEMP_CLOSE_ERROR, 70U,
		    first_close_error, error);
		return false;
	}
	temporary = NULL;
	reopening = true;
	access = 2U;
	goto open_attempt;

opened:
	if (!database_open_observe(database, path,
	    YT_DATABASE_OPEN_QUERY_DEVICE, access, database->file, 0U,
	    provider, context, &observation)
	    || !database_open_observation_valid(YT_DATABASE_OPEN_QUERY_DEVICE,
	    &observation)) {
		database_open_failure(database, YT_DATABASE_OPEN_PROVIDER_ERROR,
		    75U, observation.dos_error, error);
		return false;
	}
	database->last_open.device = observation.device;
	if (observation.device) {
		if (!database_open_observe(database, path,
		    YT_DATABASE_OPEN_CONFIGURE_DEVICE, access, database->file, 0U,
		    provider, context, &observation)
		    || !database_open_observation_valid(
		    YT_DATABASE_OPEN_CONFIGURE_DEVICE, &observation)) {
			database_open_failure(database,
			    YT_DATABASE_OPEN_PROVIDER_ERROR, 75U,
			    observation.dos_error, error);
			return false;
		}
		if (observation.carry) {
			database_open_failure(database,
			    YT_DATABASE_OPEN_DEVICE_ERROR, 57U,
			    observation.dos_error, error);
			return false;
		}
	}
	if (!database_file_length(database->file, &length)) {
		database_open_failure(database, YT_DATABASE_OPEN_SIZE_ERROR,
		    57U, database_dos_error(path, errno), error);
		return false;
	}
	database->records = (size_t)(length / YT_RECORD_SIZE);
	database->last_open.outcome = YT_DATABASE_OPEN_RETURNED;
	database->last_open.registered = true;
	database->last_open.handle_open = true;
	return true;
}

bool
yt_database_open_observed(struct yt_database *database, const char *path,
    enum yt_open_mode mode, yt_database_open_provider provider, void *context,
    struct yt_error *error)
{
	char resolved[sizeof(database->path)];
	const char *file_mode;
	uint64_t length;

	if (database == NULL || path == NULL) {
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
		return database_open_random(database, resolved,
		    provider != NULL ? provider : database_open_default,
		    context, error);
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
	database->records = (size_t)(length / YT_RECORD_SIZE);
	database->last_open.outcome = YT_DATABASE_OPEN_RETURNED;
	database->last_open.registered = true;
	database->last_open.handle_open = true;
	return true;
}

bool
yt_database_open(struct yt_database *database, const char *path,
    enum yt_open_mode mode, struct yt_error *error)
{
	return yt_database_open_observed(database, path, mode, NULL, NULL, error);
}

void
yt_database_close(struct yt_database *database)
{
	FILE *file;
	FILE *orphaned;

	if (database == NULL)
		return;
	file = database->file;
	orphaned = database->orphaned_file;
	database->file = NULL;
	database->orphaned_file = NULL;
	database->records = 0U;
	if (file != NULL)
		(void)fclose(file);
	if (orphaned != NULL && orphaned != file)
		(void)fclose(orphaned);
}

static bool database_seek_default(void *context, FILE *file,
    int64_t absolute_offset,
    struct yt_database_seek_observation *observation);

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
database_read_default(void *context, FILE *file, uint8_t *data,
    size_t requested, struct yt_database_read_observation *observation)
{
	off_t position;
	int saved_errno;

	(void)context;
	memset(observation, 0, sizeof(*observation));
	database_prepare_io(file);
	observation->accepted = fread(data, 1U, requested, file);
	saved_errno = errno;
	observation->carry = ferror(file) != 0;
	position = yt_ftello(file);
	observation->terminal_position = position >= 0 ? (int64_t)position : 0;
	if (observation->carry) {
		observation->dos_error = (saved_errno == EACCES
		    || saved_errno == EPERM) ? 5U : 1U;
		observation->mapped_error = observation->dos_error == 5U
		    ? 70U : 57U;
	}
	errno = saved_errno;
	return true;
}

bool
yt_database_random_get(struct yt_database *database, size_t basic_record,
    struct yt_record *record, size_t *accepted, struct yt_error *error)
{
	yt_database_seek_provider seek_provider;
	yt_database_read_provider read_provider;
	struct yt_database_seek_observation seek = {0};
	struct yt_database_read_observation read = {0};
	off_t offset;

	if (accepted != NULL)
		*accepted = 0U;
	if (database == NULL || database->file == NULL || record == NULL) {
		set_error(error, YT_INVALID, "random GET",
		    database != NULL ? database->path : NULL);
		return false;
	}
	memset(&database->last_get, 0, sizeof(database->last_get));
	database->last_get.registered = true;
	database->last_get.handle_open = true;
	if (basic_record == 0U || basic_record > 0xFFFFFFU) {
		database->last_get.outcome = YT_DATABASE_GET_RECORD_ERROR;
		database->last_get.basic_error = 63U;
		set_error(error, YT_RANGE, "random GET", database->path);
		return false;
	}
	offset = (off_t)((basic_record - 1U) * YT_RECORD_SIZE);
	database->last_get.current_record = (uint32_t)basic_record;
	database->last_get.record_index = (uint32_t)basic_record - 1U;
	database->last_get.desired_offset = (int64_t)offset;
	seek_provider = database->seek_provider != NULL ? database->seek_provider
	    : database_seek_default;
	if (!seek_provider(database->seek_context, database->file,
	    (int64_t)offset, &seek) || (seek.carry
	    && (seek.dos_error == 0U || seek.dos_error > 0xffU))) {
		database->last_get.outcome = YT_DATABASE_GET_SEEK_ERROR;
		database->last_get.basic_error = 52U;
		database->last_get.dos_error = seek.dos_error;
		database->last_get.terminal_position = seek.terminal_position;
		set_error(error, YT_IO_ERROR, "random GET seek", database->path);
		return false;
	}
	if (seek.carry) {
		database->last_get.outcome = YT_DATABASE_GET_SEEK_ERROR;
		database->last_get.basic_error = 52U;
		database->last_get.dos_error = seek.dos_error;
		database->last_get.terminal_position = seek.terminal_position;
		set_error(error, YT_IO_ERROR, "random GET seek", database->path);
		return false;
	}
	memset(record->bytes, 0, sizeof(record->bytes));
	read_provider = database->read_provider != NULL ? database->read_provider
	    : database_read_default;
	if (!read_provider(database->read_context, database->file, record->bytes,
	    sizeof(record->bytes), &read) || read.accepted > YT_RECORD_SIZE
	    || (read.carry && (read.dos_error == 0U
	    || read.dos_error > 0xffU))
	    || (read.carry && read.dos_error == 5U
	    && read.mapped_error != 70U && read.mapped_error != 75U)
	    || (read.carry && read.dos_error != 5U
	    && read.mapped_error != 0U && read.mapped_error != 57U)
	    || (!read.carry && (read.dos_error != 0U
	    || read.mapped_error != 0U))) {
		database->last_get.outcome = YT_DATABASE_GET_READ_ERROR;
		database->last_get.accepted = read.accepted;
		database->last_get.basic_error = 57U;
		database->last_get.dos_error = read.dos_error;
		database->last_get.terminal_position = read.terminal_position;
		set_error(error, YT_IO_ERROR, "random GET provider", database->path);
		return false;
	}
	if (accepted != NULL)
		*accepted = read.accepted;
	database->last_get.accepted = read.accepted;
	if (read.carry) {
		database->last_get.outcome = YT_DATABASE_GET_READ_ERROR;
		database->last_get.dos_error = read.dos_error;
		database->last_get.basic_error = read.dos_error == 5U
		    ? read.mapped_error : 57U;
		database->last_get.terminal_position = read.terminal_position;
		set_error(error, YT_IO_ERROR, "random GET", database->path);
		return false;
	}
	database->last_get.outcome = YT_DATABASE_GET_RETURNED;
	database->last_get.full_record = read.accepted == YT_RECORD_SIZE;
	database->last_get.terminal_position = (int64_t)offset
	    + (int64_t)read.accepted;
	return true;
}

bool
yt_database_write(struct yt_database *database, size_t basic_record,
    const struct yt_record *record, struct yt_error *error)
{
	bool written = yt_database_random_put(database, basic_record, record,
	    true, NULL, error);

	if (!written && error != NULL)
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    "write record");
	return written;
}

static bool
database_seek_default(void *context, FILE *file, int64_t absolute_offset,
    struct yt_database_seek_observation *observation)
{
	off_t position;
	int saved_errno;

	(void)context;
	memset(observation, 0, sizeof(*observation));
	database_prepare_io(file);
	if (yt_fseeko(file, (off_t)absolute_offset, SEEK_SET) == 0) {
		observation->terminal_position = absolute_offset;
		return true;
	}
	saved_errno = errno;
	position = yt_ftello(file);
	observation->carry = true;
	observation->dos_error = (saved_errno == EACCES || saved_errno == EPERM)
	    ? 5U : 1U;
	observation->terminal_position = position >= 0 ? (int64_t)position : 0;
	errno = saved_errno;
	return true;
}

static bool
database_write_default(void *context, FILE *file, const uint8_t *data,
    size_t requested, struct yt_database_write_observation *observation)
{
	off_t position;
	int saved_errno;

	(void)context;
	memset(observation, 0, sizeof(*observation));
	database_prepare_io(file);
	observation->accepted = fwrite(data, 1U, requested, file);
	saved_errno = errno;
	observation->carry = ferror(file) != 0;
	position = yt_ftello(file);
	observation->terminal_position = position >= 0 ? (int64_t)position : 0;
	if (observation->carry) {
		observation->dos_error = (saved_errno == EACCES
		    || saved_errno == EPERM) ? 5U : 1U;
		observation->mapped_error = observation->dos_error == 5U
		    ? 70U : 57U;
	}
	errno = saved_errno;
	return true;
}

static bool
database_close_default(void *context, FILE *file, size_t attempt,
    struct yt_database_close_observation *observation)
{
	int result;
	int saved_errno;

	(void)context;
	(void)attempt;
	memset(observation, 0, sizeof(*observation));
	if (file == NULL) {
		observation->carry = true;
		observation->dos_error = 6U;
		return true;
	}
	errno = 0;
	result = fclose(file);
	saved_errno = errno;
	observation->carry = result != 0;
	observation->dos_error = result != 0
	    ? database_dos_error(NULL, saved_errno) : 0U;
	observation->handle_open = false;
	errno = saved_errno;
	return true;
}

static bool
database_close_observation_valid(
    const struct yt_database_close_observation *observation,
    bool active_handle)
{
	if (observation->handle_open && !active_handle)
		return false;
	if (observation->carry)
		return observation->dos_error >= 1U
		    && observation->dos_error <= 0xffU;
	return observation->dos_error == 0U && !observation->handle_open;
}

static bool
database_close_observe(struct yt_database *database,
    yt_database_close_provider provider, FILE *file, size_t attempt,
    struct yt_database_close_observation *observation)
{
	++database->last_close.attempt_count;
	memset(observation, 0, sizeof(*observation));
	return provider(database->close_context, file, attempt, observation);
}

static void
database_close_failure(struct yt_database *database,
    enum yt_database_close_outcome outcome, uint16_t basic_error,
    uint16_t dos_error, struct yt_error *error)
{
	database->last_close.outcome = outcome;
	database->last_close.basic_error = basic_error;
	database->last_close.dos_error = dos_error;
	database->last_close.registered = database->file != NULL;
	database->last_close.handle_open = database->file != NULL
	    || database->orphaned_file != NULL;
	set_error(error, YT_IO_ERROR, "random CLOSE", database->path);
}

bool
yt_database_random_close(struct yt_database *database, struct yt_error *error)
{
	yt_database_close_provider provider;
	struct yt_database_close_observation observation;
	FILE *file;
	uint16_t first_dos_error;
	bool delivered;
	bool handle_open;
	bool retry_active;

	if (database == NULL) {
		set_error(error, YT_INVALID, "random CLOSE", NULL);
		return false;
	}
	memset(&database->last_close, 0, sizeof(database->last_close));
	if (database->file == NULL) {
		database->last_close.outcome = YT_DATABASE_CLOSE_RETURNED;
		database->last_close.missing = true;
		database->last_close.handle_open = database->orphaned_file != NULL;
		return true;
	}
	database->last_close.device = database->last_open.device;
	file = database->file;
	provider = database->close_provider != NULL ? database->close_provider
	    : database_close_default;
	delivered = database_close_observe(database, provider, file, 1U,
	    &observation);
	if (delivered && !observation.handle_open) {
		database->file = NULL;
		database->records = 0U;
	}
	if (!delivered || !database_close_observation_valid(&observation, true)) {
		database_close_failure(database, YT_DATABASE_CLOSE_PROVIDER_ERROR,
		    57U, observation.dos_error, error);
		return false;
	}
	if (!observation.carry) {
		database->file = NULL;
		database->records = 0U;
		database->last_close.outcome = YT_DATABASE_CLOSE_RETURNED;
		database->last_close.registered = false;
		database->last_close.handle_open = database->orphaned_file != NULL;
		return true;
	}
	first_dos_error = observation.dos_error;
	handle_open = observation.handle_open;
	database->file = NULL;
	database->records = 0U;
	database->last_close.retry_attempted = true;
	retry_active = handle_open;
	delivered = database_close_observe(database, provider,
	    retry_active ? file : NULL, 2U, &observation);
	if (!delivered || !database_close_observation_valid(&observation,
	    retry_active)) {
		if ((delivered && observation.handle_open)
		    || (!delivered && handle_open))
			database->orphaned_file = file;
		database_close_failure(database, YT_DATABASE_CLOSE_PROVIDER_ERROR,
		    57U, first_dos_error, error);
		return false;
	}
	handle_open = observation.handle_open;
	if (handle_open)
		database->orphaned_file = file;
	database_close_failure(database, database->last_close.device
	    ? YT_DATABASE_CLOSE_DEVICE_ERROR : YT_DATABASE_CLOSE_DISK_ERROR,
	    database->last_close.device ? 57U : 70U, first_dos_error, error);
	return false;
}

static bool
database_lof_default(void *context, FILE *file,
    enum yt_database_lof_operation operation, uint32_t restore_position,
    struct yt_database_lof_observation *observation)
{
	int64_t position;
	off_t offset;
	int saved_errno;
	int whence;

	(void)context;
	memset(observation, 0, sizeof(*observation));
	if (file == NULL)
		return false;
	switch (operation) {
	case YT_DATABASE_LOF_CURRENT:
		offset = 0;
		whence = SEEK_CUR;
		break;
	case YT_DATABASE_LOF_END:
		offset = 0;
		whence = SEEK_END;
		break;
	case YT_DATABASE_LOF_RESTORE:
		offset = (off_t)restore_position;
		whence = SEEK_SET;
		break;
	default:
		return false;
	}
	database_prepare_io(file);
	if (yt_fseeko(file, offset, whence) != 0) {
		saved_errno = errno;
		position = yt_ftello(file);
		observation->carry = true;
		observation->dos_error = database_dos_error(NULL, saved_errno);
		observation->terminal_position = position >= 0 ? position : -1;
		errno = saved_errno;
		return true;
	}
	position = yt_ftello(file);
	saved_errno = errno;
	observation->terminal_position = position >= 0 ? position : -1;
	if (position < 0 || (uint64_t)position > UINT32_MAX) {
		observation->carry = true;
		observation->dos_error = position < 0
		    ? database_dos_error(NULL, saved_errno) : 1U;
	}
	errno = saved_errno;
	return true;
}

static bool
database_lof_observation_valid(
    const struct yt_database_lof_observation *observation)
{
	if (observation->carry)
		return observation->dos_error >= 1U
		    && observation->dos_error <= 0xffU;
	return observation->dos_error == 0U
	    && observation->terminal_position >= 0
	    && (uint64_t)observation->terminal_position <= UINT32_MAX;
}

static bool
database_lof_fail(struct yt_database *database,
    enum yt_database_lof_outcome outcome,
    enum yt_database_lof_operation operation, uint16_t dos_error,
    struct yt_error *error)
{
	database->last_lof.outcome = outcome;
	database->last_lof.failed_operation = operation;
	database->last_lof.dos_error = dos_error;
	database->last_lof.basic_error = 52U;
	database->last_lof.registered = database->file != NULL;
	database->last_lof.handle_open = database->file != NULL
	    || database->orphaned_file != NULL;
	set_error(error, YT_IO_ERROR, outcome == YT_DATABASE_LOF_SEEK_ERROR
	    ? "random LOF seek" : "random LOF provider", database->path);
	return false;
}

bool
yt_database_random_lof(struct yt_database *database, uint32_t *length,
    struct yt_error *error)
{
	static const enum yt_database_lof_operation operations[] = {
		YT_DATABASE_LOF_CURRENT,
		YT_DATABASE_LOF_END,
		YT_DATABASE_LOF_RESTORE,
	};
	yt_database_lof_provider provider;
	struct yt_database_lof_observation observation;
	uint32_t saved_position = 0U;
	uint32_t restore_position = 0U;
	size_t index;

	if (length != NULL)
		*length = 0U;
	if (database == NULL || database->file == NULL || length == NULL) {
		set_error(error, YT_INVALID, "random LOF", database != NULL
		    ? database->path : NULL);
		return false;
	}
	memset(&database->last_lof, 0, sizeof(database->last_lof));
	database->last_lof.device = database->last_open.device;
	database->last_lof.registered = true;
	database->last_lof.handle_open = true;
	if (database->last_lof.device) {
		database->last_lof.outcome = YT_DATABASE_LOF_RETURNED;
		database->last_lof.length = database->device_position;
		database->last_lof.saved_position = database->device_position;
		database->last_lof.terminal_position = database->device_position;
		*length = database->device_position;
		return true;
	}
	provider = database->lof_provider != NULL ? database->lof_provider
	    : database_lof_default;
	for (index = 0U; index < YT_ARRAY_LEN(operations); ++index) {
		enum yt_database_lof_operation operation = operations[index];

		++database->last_lof.operation_count;
		memset(&observation, 0, sizeof(observation));
		if (!provider(database->lof_context, database->file, operation,
		    operation == YT_DATABASE_LOF_RESTORE ? restore_position : 0U,
		    &observation))
			return database_lof_fail(database,
			    YT_DATABASE_LOF_PROVIDER_ERROR, operation, 0U, error);
		database->last_lof.terminal_position
		    = observation.terminal_position;
		if (!database_lof_observation_valid(&observation))
			return database_lof_fail(database,
			    YT_DATABASE_LOF_PROVIDER_ERROR, operation,
			    observation.dos_error, error);
		if (observation.carry)
			return database_lof_fail(database,
			    YT_DATABASE_LOF_SEEK_ERROR, operation,
			    observation.dos_error, error);
		if (operation == YT_DATABASE_LOF_CURRENT) {
			saved_position = (uint32_t)observation.terminal_position;
			restore_position = saved_position;
			database->last_lof.saved_position = saved_position;
		}
		else if (operation == YT_DATABASE_LOF_END)
			database->last_lof.length
			    = (uint32_t)observation.terminal_position;
	}
	database->last_lof.outcome = YT_DATABASE_LOF_RETURNED;
	*length = database->last_lof.length;
	return true;
}

bool
yt_random_file_lof(FILE *file, const char *path, uint32_t *length,
    struct yt_database_lof_result *result, struct yt_error *error)
{
	struct yt_database database;
	bool returned;

	memset(&database, 0, sizeof(database));
	database.file = file;
	if (path != NULL)
		(void)snprintf(database.path, sizeof(database.path), "%s", path);
	returned = yt_database_random_lof(&database, length, error);
	if (result != NULL)
		*result = database.last_lof;
	return returned;
}

static bool
database_flush_default(void *context, FILE *file)
{
	(void)context;
	return fflush(file) == 0;
}

static void
database_reject_short(struct yt_database *database, struct yt_error *error)
{
	yt_database_close_provider provider;
	struct yt_database_close_observation observation = {0};
	FILE *file = database->file;
	bool delivered;
	bool valid;

	database->file = NULL;
	database->records = 0U;
	database->short_close_attempted = true;
	provider = database->close_provider != NULL ? database->close_provider
	    : database_close_default;
	delivered = provider(database->close_context, file, 1U, &observation);
	valid = delivered && database_close_observation_valid(&observation, true);
	database->short_close_succeeded = valid && !observation.carry;
	if ((!delivered || observation.handle_open)
	    && !database->short_close_succeeded)
		database->orphaned_file = file;
	database->last_put.outcome = YT_DATABASE_PUT_REJECTED_SHORT;
	database->last_put.basic_error = 61U;
	database->last_put.registered = false;
	database->last_put.close_attempted = true;
	database->last_put.close_succeeded = database->short_close_succeeded;
	database->last_put.handle_open = database->orphaned_file != NULL;
	set_error(error, YT_IO_ERROR, "random PUT rejected short", database->path);
}

bool
yt_database_random_put(struct yt_database *database, size_t basic_record,
    const struct yt_record *record, bool one_byte_short_ok, size_t *accepted,
    struct yt_error *error)
{
	yt_database_seek_provider seek_provider;
	yt_database_write_provider write_provider;
	struct yt_database_seek_observation seek = {0};
	struct yt_database_write_observation write = {0};
	off_t offset;
	bool tolerated_short;

	if (accepted != NULL)
		*accepted = 0U;
	if (database == NULL || database->file == NULL || record == NULL) {
		set_error(error, YT_INVALID, "random PUT",
		    database != NULL ? database->path : NULL);
		return false;
	}
	memset(&database->last_put, 0, sizeof(database->last_put));
	database->last_put.registered = true;
	database->last_put.handle_open = true;

	if (basic_record == 0) {
		set_error(error, YT_RANGE, "random PUT", database->path);
		return false;
	}
	offset = (off_t)((basic_record - 1U) * YT_RECORD_SIZE);
	database->short_close_attempted = false;
	database->short_close_succeeded = false;
	seek_provider = database->seek_provider != NULL ? database->seek_provider
	    : database_seek_default;
	if (!seek_provider(database->seek_context, database->file,
	    (int64_t)offset, &seek) || (seek.carry
	    && (seek.dos_error == 0U || seek.dos_error > 0xffU))) {
		database->last_put.outcome = YT_DATABASE_PUT_SEEK_ERROR;
		database->last_put.basic_error = 52U;
		database->last_put.dos_error = seek.dos_error;
		database->last_put.terminal_position = seek.terminal_position;
		set_error(error, YT_IO_ERROR, "random PUT seek", database->path);
		return false;
	}
	if (seek.carry) {
		database->last_put.outcome = YT_DATABASE_PUT_SEEK_ERROR;
		database->last_put.basic_error = 52U;
		database->last_put.dos_error = seek.dos_error;
		database->last_put.terminal_position = seek.terminal_position;
		set_error(error, YT_IO_ERROR, "random PUT seek", database->path);
		return false;
	}
	write_provider = database->write_provider != NULL ? database->write_provider
	    : database_write_default;
	if (!write_provider(database->write_context, database->file, record->bytes,
	    YT_RECORD_SIZE, &write) || write.accepted > YT_RECORD_SIZE
	    || (write.carry && (write.dos_error == 0U
	    || write.dos_error > 0xffU))
	    || (write.carry && write.dos_error == 5U
	    && write.mapped_error != 70U && write.mapped_error != 75U)
	    || (write.carry && write.dos_error != 5U
	    && write.mapped_error != 0U && write.mapped_error != 57U)
	    || (!write.carry && (write.dos_error != 0U
	    || write.mapped_error != 0U))) {
		database->last_put.outcome = YT_DATABASE_PUT_WRITE_ERROR;
		database->last_put.accepted = write.accepted;
		database->last_put.basic_error = 57U;
		database->last_put.dos_error = write.dos_error;
		database->last_put.terminal_position = write.terminal_position;
		set_error(error, YT_IO_ERROR, "random PUT provider", database->path);
		return false;
	}
	if (accepted != NULL)
		*accepted = write.accepted;
	database->last_put.accepted = write.accepted;
	if (write.carry) {
		database->last_put.outcome = YT_DATABASE_PUT_WRITE_ERROR;
		database->last_put.dos_error = write.dos_error;
		database->last_put.basic_error = write.dos_error == 5U
		    ? write.mapped_error : 57U;
		database->last_put.terminal_position = write.terminal_position;
		set_error(error, YT_IO_ERROR, "random PUT", database->path);
		return false;
	}
	database->last_put.terminal_position = (int64_t)offset
	    + (int64_t)write.accepted;
	tolerated_short = one_byte_short_ok
	    && write.accepted == YT_RECORD_SIZE - 1U;
	if (write.accepted != YT_RECORD_SIZE && !tolerated_short) {
		database_reject_short(database, error);
		return false;
	}
	database->last_put.outcome = YT_DATABASE_PUT_RETURNED;
	if (basic_record > database->records)
		database->records = basic_record;
	return true;
}

void
yt_database_set_write_provider(struct yt_database *database,
    yt_database_write_provider provider, void *context)
{
	if (database == NULL)
		return;
	database->write_provider = provider;
	database->write_context = context;
}

void
yt_database_set_read_provider(struct yt_database *database,
    yt_database_read_provider provider, void *context)
{
	if (database == NULL)
		return;
	database->read_provider = provider;
	database->read_context = context;
}

void
yt_database_set_seek_provider(struct yt_database *database,
    yt_database_seek_provider provider, void *context)
{
	if (database == NULL)
		return;
	database->seek_provider = provider;
	database->seek_context = context;
}

void
yt_database_set_close_provider(struct yt_database *database,
    yt_database_close_provider provider, void *context)
{
	if (database == NULL)
		return;
	database->close_provider = provider;
	database->close_context = context;
}

void
yt_database_set_lof_provider(struct yt_database *database,
    yt_database_lof_provider provider, void *context)
{
	if (database == NULL)
		return;
	database->lof_provider = provider;
	database->lof_context = context;
}

void
yt_database_set_flush_provider(struct yt_database *database,
    yt_database_flush_provider provider, void *context)
{
	if (database == NULL)
		return;
	database->flush_provider = provider;
	database->flush_context = context;
}

bool
yt_database_flush(struct yt_database *database, struct yt_error *error)
{
	yt_database_flush_provider provider;

	if (database == NULL || database->file == NULL) {
		set_error(error, YT_INVALID, "flush database",
		    database != NULL ? database->path : NULL);
		return false;
	}
	provider = database->flush_provider != NULL ? database->flush_provider
	    : database_flush_default;
	if (!provider(database->flush_context, database->file)) {
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
	static const struct yt_radio_field fields[YT_RADIO_FIELD_COUNT] = {
		{0U, 4U},
		{4U, 4U},
		{8U, 4U},
		{12U, 74U},
	};
	if (radio == NULL || path == NULL) {
		set_error(error, YT_INVALID, "open radio", path);
		return false;
	}
	if (!yt_radio_file_close(radio, error))
		return false;
	if (!yt_database_open(&radio->random, path, YT_OPEN_UPDATE_CREATE,
	    error))
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
	off_t offset;
	size_t count;

	if (accepted != NULL)
		*accepted = 0U;
	if (radio == NULL || radio->random.file == NULL || record == NULL
	    || basic_record == 0U || basic_record > 0xFFFFFFU) {
		set_error(error, YT_RANGE, "radio GET", radio != NULL
		    ? radio->random.path : NULL);
		return false;
	}
	offset = (off_t)((uint64_t)(basic_record - 1U)
	    * YT_RADIO_RECORD_SIZE);
	if (yt_fseeko(radio->random.file, offset, SEEK_SET) != 0) {
		set_error(error, YT_IO_ERROR, "radio GET", radio->random.path);
		return false;
	}
	memset(record->bytes, 0, sizeof(record->bytes));
	clearerr(radio->random.file);
	count = fread(record->bytes, 1, sizeof(record->bytes),
	    radio->random.file);
	if (ferror(radio->random.file)) {
		set_error(error, YT_IO_ERROR, "radio GET", radio->random.path);
		return false;
	}
	if (accepted != NULL)
		*accepted = count;
	return true;
}

bool
yt_radio_file_put(struct yt_radio_file *radio, uint32_t basic_record,
    const struct yt_radio_record *record, struct yt_error *error)
{
	off_t offset;

	if (radio == NULL || radio->random.file == NULL || record == NULL
	    || basic_record == 0U || basic_record > 0xFFFFFFU) {
		set_error(error, YT_RANGE, "radio PUT", radio != NULL
		    ? radio->random.path : NULL);
		return false;
	}
	offset = (off_t)((uint64_t)(basic_record - 1U)
	    * YT_RADIO_RECORD_SIZE);
	if (yt_fseeko(radio->random.file, offset, SEEK_SET) != 0
	    || fwrite(record->bytes, 1, sizeof(record->bytes),
	    radio->random.file)
	    != sizeof(record->bytes)) {
		set_error(error, YT_IO_ERROR, "radio PUT", radio->random.path);
		return false;
	}
	return true;
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
yt_file_delete(const char *path, bool missing_ok, struct yt_error *error)
{
	char resolved[512];
	struct yt_error local_error;
	struct yt_error *active_error = error != NULL ? error : &local_error;

	yt_error_clear(active_error);
	if (!yt_resolve_case_path(path, false, resolved, sizeof(resolved),
	    active_error))
		return missing_ok && active_error->status == YT_NOT_FOUND;
	if (remove(resolved) != 0) {
		if (missing_ok && errno == ENOENT)
			return true;
		set_error(error, YT_IO_ERROR, "delete", resolved);
		return false;
	}
	return true;
}

bool
yt_file_rename(const char *old_path, const char *new_path,
    struct yt_error *error)
{
	char old_resolved[512];
	char new_resolved[512];

	if (!yt_resolve_case_path(old_path, false, old_resolved,
	    sizeof(old_resolved), error)
	    || !yt_resolve_case_path(new_path, true, new_resolved,
	    sizeof(new_resolved), error))
		return false;
	if (rename(old_resolved, new_resolved) != 0) {
		set_error(error, YT_IO_ERROR, "rename", old_resolved);
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
