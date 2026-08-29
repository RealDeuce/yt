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
	if (database->file != NULL)
		fclose(database->file);
	database->file = NULL;
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
yt_database_write(struct yt_database *database, size_t basic_record,
    const struct yt_record *record, struct yt_error *error)
{
	off_t offset;

	if (basic_record == 0) {
		set_error(error, YT_RANGE, "write record", database->path);
		return false;
	}
	offset = (off_t)((basic_record - 1U) * YT_RECORD_SIZE);
	if (yt_fseeko(database->file, offset, SEEK_SET) != 0
	    || fwrite(record->bytes, 1, YT_RECORD_SIZE, database->file)
	    != YT_RECORD_SIZE) {
		set_error(error, YT_IO_ERROR, "write record", database->path);
		return false;
	}
	if (basic_record > database->records)
		database->records = basic_record;
	return true;
}

bool
yt_database_flush(struct yt_database *database, struct yt_error *error)
{
	if (fflush(database->file) != 0) {
		set_error(error, YT_IO_ERROR, "flush database", database->path);
		return false;
	}
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
