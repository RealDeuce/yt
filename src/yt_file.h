#ifndef YT_FILE_H
#define YT_FILE_H

#include "yt_data.h"

enum yt_open_mode {
	YT_OPEN_READ,
	YT_OPEN_UPDATE,
	YT_OPEN_CREATE,
	YT_OPEN_UPDATE_CREATE
};

typedef bool (*yt_database_seek_provider)(void *context, FILE *file,
    int64_t absolute_offset);
typedef bool (*yt_database_write_provider)(void *context, FILE *file,
    const uint8_t *data, size_t requested, size_t *accepted,
    bool *write_error);
typedef bool (*yt_database_close_provider)(void *context, FILE *file,
    bool *handle_open);
typedef bool (*yt_database_flush_provider)(void *context, FILE *file);

struct yt_database {
	FILE *file;
	FILE *orphaned_file;
	char path[512];
	size_t records;
	yt_database_seek_provider seek_provider;
	void *seek_context;
	yt_database_write_provider write_provider;
	void *write_context;
	yt_database_close_provider close_provider;
	void *close_context;
	yt_database_flush_provider flush_provider;
	void *flush_context;
	bool short_close_attempted;
	bool short_close_succeeded;
};

#define YT_RADIO_FIELD_COUNT 4U

struct yt_radio_field {
	size_t offset;
	size_t length;
};

struct yt_radio_file {
	FILE *file;
	char path[512];
	size_t record_length;
	struct yt_radio_field fields[YT_RADIO_FIELD_COUNT];
	size_t field_count;
};

bool yt_resolve_case_path(const char *requested, bool allow_missing,
    char *resolved, size_t size, struct yt_error *error);
bool yt_database_open(struct yt_database *database, const char *path,
    enum yt_open_mode mode, struct yt_error *error);
void yt_database_close(struct yt_database *database);
bool yt_database_read(struct yt_database *database, size_t basic_record,
    struct yt_record *record, struct yt_error *error);
bool yt_database_random_get(struct yt_database *database,
    size_t basic_record, struct yt_record *record, size_t *accepted,
    struct yt_error *error);
bool yt_database_write(struct yt_database *database, size_t basic_record,
    const struct yt_record *record, struct yt_error *error);
bool yt_database_random_put(struct yt_database *database,
    size_t basic_record, const struct yt_record *record,
    bool one_byte_short_ok, size_t *accepted, struct yt_error *error);
void yt_database_set_write_provider(struct yt_database *database,
    yt_database_write_provider provider, void *context);
void yt_database_set_seek_provider(struct yt_database *database,
    yt_database_seek_provider provider, void *context);
void yt_database_set_close_provider(struct yt_database *database,
    yt_database_close_provider provider, void *context);
void yt_database_set_flush_provider(struct yt_database *database,
    yt_database_flush_provider provider, void *context);
bool yt_database_flush(struct yt_database *database, struct yt_error *error);
void yt_radio_file_init(struct yt_radio_file *radio);
bool yt_radio_file_open(struct yt_radio_file *radio, const char *path,
    struct yt_error *error);
bool yt_radio_file_close(struct yt_radio_file *radio,
    struct yt_error *error);
bool yt_radio_file_size(struct yt_radio_file *radio, uint64_t *size,
    struct yt_error *error);
bool yt_radio_file_get(struct yt_radio_file *radio, uint32_t basic_record,
    struct yt_radio_record *record, size_t *accepted,
    struct yt_error *error);
bool yt_radio_file_put(struct yt_radio_file *radio, uint32_t basic_record,
    const struct yt_radio_record *record, struct yt_error *error);
bool yt_radio_file_next_record(struct yt_radio_file *radio,
    uint32_t *basic_record, struct yt_error *error);
bool yt_file_delete(const char *path, bool missing_ok, struct yt_error *error);
bool yt_file_rename(const char *old_path, const char *new_path,
    struct yt_error *error);
bool yt_file_size(const char *path, size_t *size, struct yt_error *error);

#endif
