#include "yt_file.h"
#include "qb.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <io.h>
#define yt_fseeko _fseeki64
#define yt_ftello _ftelli64
#else
#include <unistd.h>
#define yt_fseeko fseeko
#define yt_ftello ftello
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

bool
yt_database_open(struct yt_database *database, const char *path,
    enum yt_open_mode mode, struct yt_error *error)
{
	char resolved[sizeof(database->path)];
	const char *file_mode;
	off_t length;

	memset(database, 0, sizeof(*database));
	if (!yt_resolve_case_path(path, mode == YT_OPEN_CREATE
	    || mode == YT_OPEN_UPDATE_CREATE, resolved,
	    sizeof(resolved), error))
		return false;
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
	case YT_OPEN_UPDATE_CREATE:
		file_mode = "r+b";
		break;
	default:
		set_error(error, YT_INVALID, "database mode", path);
		return false;
	}
	database->file = fopen(resolved, file_mode);
	if (database->file == NULL && mode == YT_OPEN_UPDATE_CREATE
	    && errno == ENOENT)
		database->file = fopen(resolved, "w+b");
	if (database->file == NULL) {
		set_error(error, errno == ENOENT ? YT_NOT_FOUND : YT_IO_ERROR,
		    "open database", resolved);
		return false;
	}
	snprintf(database->path, sizeof(database->path), "%s", resolved);
	if (yt_fseeko(database->file, 0, SEEK_END) != 0) {
		set_error(error, YT_IO_ERROR, "seek database", resolved);
		yt_database_close(database);
		return false;
	}
	length = yt_ftello(database->file);
	if (length < 0) {
		set_error(error, YT_IO_ERROR, "size database", resolved);
		yt_database_close(database);
		return false;
	}
	database->records = (size_t)length / YT_RECORD_SIZE;
	if (yt_fseeko(database->file, 0, SEEK_SET) != 0) {
		set_error(error, YT_IO_ERROR, "rewind database", resolved);
		yt_database_close(database);
		return false;
	}
	return true;
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
	if (file != NULL)
		(void)fclose(file);
	if (orphaned != NULL && orphaned != file)
		(void)fclose(orphaned);
}

bool
yt_database_read(struct yt_database *database, size_t basic_record,
    struct yt_record *record, struct yt_error *error)
{
	off_t offset;

	if (basic_record == 0) {
		set_error(error, YT_RANGE, "read record", database->path);
		return false;
	}
	offset = (off_t)((basic_record - 1U) * YT_RECORD_SIZE);
	if (yt_fseeko(database->file, offset, SEEK_SET) != 0
	    || fread(record->bytes, 1, YT_RECORD_SIZE, database->file)
	    != YT_RECORD_SIZE) {
		set_error(error, feof(database->file) ? YT_EOF : YT_IO_ERROR,
		    "read record", database->path);
		return false;
	}
	return true;
}

bool
yt_database_random_get(struct yt_database *database, size_t basic_record,
    struct yt_record *record, size_t *accepted, struct yt_error *error)
{
	off_t offset;
	size_t count;

	if (accepted != NULL)
		*accepted = 0U;
	if (basic_record == 0) {
		set_error(error, YT_RANGE, "random GET", database->path);
		return false;
	}
	offset = (off_t)((basic_record - 1U) * YT_RECORD_SIZE);
	if (yt_fseeko(database->file, offset, SEEK_SET) != 0) {
		set_error(error, YT_IO_ERROR, "random GET", database->path);
		return false;
	}
	memset(record->bytes, 0, sizeof(record->bytes));
	count = fread(record->bytes, 1, sizeof(record->bytes), database->file);
	if (ferror(database->file)) {
		set_error(error, YT_IO_ERROR, "random GET", database->path);
		return false;
	}
	if (accepted != NULL)
		*accepted = count;
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
database_seek_default(void *context, FILE *file, int64_t absolute_offset)
{
	(void)context;
	return yt_fseeko(file, (off_t)absolute_offset, SEEK_SET) == 0;
}

static bool
database_write_default(void *context, FILE *file, const uint8_t *data,
    size_t requested, size_t *accepted, bool *write_error)
{
	(void)context;
	*accepted = fwrite(data, 1U, requested, file);
	*write_error = ferror(file) != 0;
	return true;
}

static bool
database_close_default(void *context, FILE *file, bool *handle_open)
{
	int result;

	(void)context;
	result = fclose(file);
	*handle_open = false;
	return result == 0;
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
	FILE *file = database->file;
	bool handle_open = false;

	database->file = NULL;
	database->records = 0U;
	database->short_close_attempted = true;
	provider = database->close_provider != NULL ? database->close_provider
	    : database_close_default;
	database->short_close_succeeded = provider(database->close_context, file,
	    &handle_open);
	if (!database->short_close_succeeded && handle_open)
		database->orphaned_file = file;
	set_error(error, YT_IO_ERROR, "random PUT rejected short", database->path);
}

bool
yt_database_random_put(struct yt_database *database, size_t basic_record,
    const struct yt_record *record, bool one_byte_short_ok, size_t *accepted,
    struct yt_error *error)
{
	yt_database_seek_provider seek_provider;
	yt_database_write_provider write_provider;
	off_t offset;
	size_t count = 0U;
	bool write_error = false;
	bool tolerated_short;

	if (accepted != NULL)
		*accepted = 0U;
	if (database == NULL || database->file == NULL || record == NULL) {
		set_error(error, YT_INVALID, "random PUT",
		    database != NULL ? database->path : NULL);
		return false;
	}

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
	    (int64_t)offset)) {
		set_error(error, YT_IO_ERROR, "random PUT", database->path);
		return false;
	}
	write_provider = database->write_provider != NULL ? database->write_provider
	    : database_write_default;
	if (!write_provider(database->write_context, database->file, record->bytes,
	    YT_RECORD_SIZE, &count, &write_error) || count > YT_RECORD_SIZE) {
		set_error(error, YT_IO_ERROR, "random PUT provider", database->path);
		return false;
	}
	if (accepted != NULL)
		*accepted = count;
	tolerated_short = one_byte_short_ok && count == YT_RECORD_SIZE - 1U;
	if (write_error) {
		set_error(error, YT_IO_ERROR, "random PUT", database->path);
		return false;
	}
	if (count != YT_RECORD_SIZE && !tolerated_short) {
		database_reject_short(database, error);
		return false;
	}
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
	FILE *file;

	if (radio == NULL) {
		set_error(error, YT_INVALID, "close radio", NULL);
		return false;
	}
	file = radio->file;
	radio->file = NULL;
	if (file != NULL && fclose(file) != 0) {
		set_error(error, YT_IO_ERROR, "close radio", radio->path);
		return false;
	}
	return true;
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
	char resolved[512];

	if (radio == NULL || path == NULL) {
		set_error(error, YT_INVALID, "open radio", path);
		return false;
	}
	if (!yt_radio_file_close(radio, error))
		return false;
	radio->path[0] = '\0';
	radio->record_length = 0U;
	radio->field_count = 0U;
	memset(radio->fields, 0, sizeof(radio->fields));
	if (!yt_resolve_case_path(path, true, resolved, sizeof(resolved), error))
		return false;
	radio->file = fopen(resolved, "r+b");
	if (radio->file == NULL && errno == ENOENT)
		radio->file = fopen(resolved, "w+b");
	if (radio->file == NULL) {
		set_error(error, YT_IO_ERROR, "open radio", resolved);
		return false;
	}
	(void)snprintf(radio->path, sizeof(radio->path), "%s", resolved);
	radio->record_length = YT_RADIO_RECORD_SIZE;
	memcpy(radio->fields, fields, sizeof(fields));
	radio->field_count = YT_RADIO_FIELD_COUNT;
	return true;
}

bool
yt_radio_file_size(struct yt_radio_file *radio, uint64_t *size,
    struct yt_error *error)
{
	off_t position;
	off_t length;

	if (radio == NULL || radio->file == NULL || size == NULL) {
		set_error(error, YT_INVALID, "radio LOF", radio != NULL
		    ? radio->path : NULL);
		return false;
	}
	position = yt_ftello(radio->file);
	if (position < 0 || yt_fseeko(radio->file, 0, SEEK_END) != 0) {
		set_error(error, YT_IO_ERROR, "radio LOF", radio->path);
		return false;
	}
	length = yt_ftello(radio->file);
	if (length < 0 || yt_fseeko(radio->file, position, SEEK_SET) != 0) {
		set_error(error, YT_IO_ERROR, "radio LOF", radio->path);
		return false;
	}
	*size = (uint64_t)length;
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
	if (radio == NULL || radio->file == NULL || record == NULL
	    || basic_record == 0U || basic_record > 0xFFFFFFU) {
		set_error(error, YT_RANGE, "radio GET", radio != NULL
		    ? radio->path : NULL);
		return false;
	}
	offset = (off_t)((uint64_t)(basic_record - 1U)
	    * YT_RADIO_RECORD_SIZE);
	if (yt_fseeko(radio->file, offset, SEEK_SET) != 0) {
		set_error(error, YT_IO_ERROR, "radio GET", radio->path);
		return false;
	}
	memset(record->bytes, 0, sizeof(record->bytes));
	clearerr(radio->file);
	count = fread(record->bytes, 1, sizeof(record->bytes), radio->file);
	if (ferror(radio->file)) {
		set_error(error, YT_IO_ERROR, "radio GET", radio->path);
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

	if (radio == NULL || radio->file == NULL || record == NULL
	    || basic_record == 0U || basic_record > 0xFFFFFFU) {
		set_error(error, YT_RANGE, "radio PUT", radio != NULL
		    ? radio->path : NULL);
		return false;
	}
	offset = (off_t)((uint64_t)(basic_record - 1U)
	    * YT_RADIO_RECORD_SIZE);
	if (yt_fseeko(radio->file, offset, SEEK_SET) != 0
	    || fwrite(record->bytes, 1, sizeof(record->bytes), radio->file)
	    != sizeof(record->bytes)) {
		set_error(error, YT_IO_ERROR, "radio PUT", radio->path);
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
		set_error(error, YT_RANGE, "radio record number", radio->path);
		return false;
	}
	record = length / YT_RADIO_RECORD_SIZE + 1U;
	if (record > 0xFFFFFFU) {
		set_error(error, YT_RANGE, "radio record number", radio->path);
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
