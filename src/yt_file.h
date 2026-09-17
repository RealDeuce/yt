#ifndef YT_FILE_H
#define YT_FILE_H

#include "yt_data.h"

enum yt_open_mode {
	YT_OPEN_READ,
	YT_OPEN_UPDATE,
	YT_OPEN_CREATE,
	YT_OPEN_UPDATE_CREATE
};

struct yt_database {
	FILE *file;
	char path[512];
	size_t records;
	uint32_t device_position;
	bool device;
	uint16_t last_open_basic_error;
	uint16_t last_close_basic_error;
	uint16_t last_lof_basic_error;
	uint16_t last_get_basic_error;
	uint16_t last_put_basic_error;
};

#define YT_RADIO_FIELD_COUNT 4U

struct yt_radio_field {
	size_t offset;
	size_t length;
};

struct yt_radio_file {
	struct yt_database random;
	size_t record_length;
	struct yt_radio_field fields[YT_RADIO_FIELD_COUNT];
	size_t field_count;
};

bool yt_resolve_case_path(const char *requested, bool allow_missing,
    char *resolved, size_t size, struct yt_error *error);
bool yt_database_open(struct yt_database *database, const char *path,
    enum yt_open_mode mode, struct yt_error *error);
bool yt_database_random_close(struct yt_database *database,
    struct yt_error *error);
/* CLOSE-all semantics for a native owner with one random-file resource. */
bool yt_database_close_all_single(struct yt_database *database,
    struct yt_error *error);
bool yt_database_random_lof(struct yt_database *database, uint32_t *length,
    struct yt_error *error);
void yt_database_close(struct yt_database *database);
bool yt_database_read(struct yt_database *database, size_t basic_record,
    struct yt_record *record, struct yt_error *error);
bool yt_database_random_get(struct yt_database *database,
    size_t basic_record, struct yt_record *record, size_t *accepted,
    struct yt_error *error);
bool yt_database_write(struct yt_database *database, size_t basic_record,
    const struct yt_record *record, struct yt_error *error);
bool yt_database_write_durable(struct yt_database *database,
    size_t basic_record, const struct yt_record *record,
    struct yt_error *error);
bool yt_database_random_put(struct yt_database *database,
    size_t basic_record, const struct yt_record *record,
    bool one_byte_short_ok, size_t *accepted, struct yt_error *error);
bool yt_database_flush(struct yt_database *database, struct yt_error *error);
void yt_radio_file_init(struct yt_radio_file *radio);
bool yt_radio_file_open(struct yt_radio_file *radio, const char *path,
    struct yt_error *error);
bool yt_radio_file_open_text_width(struct yt_radio_file *radio,
    const char *path, size_t text_width, struct yt_error *error);
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
bool yt_file_kill(const char *path, struct yt_error *error);
bool yt_file_rename(const char *old_path, const char *new_path,
    struct yt_error *error);
bool yt_file_size(const char *path, size_t *size, struct yt_error *error);

#endif
