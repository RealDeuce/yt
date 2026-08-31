#ifndef YT_FILE_H
#define YT_FILE_H

#include "yt_data.h"

enum yt_open_mode {
	YT_OPEN_READ,
	YT_OPEN_UPDATE,
	YT_OPEN_CREATE,
	YT_OPEN_UPDATE_CREATE
};

enum yt_database_open_operation {
	YT_DATABASE_OPEN_EXISTING,
	YT_DATABASE_OPEN_CREATE,
	YT_DATABASE_OPEN_TEMP_CLOSE,
	YT_DATABASE_OPEN_QUERY_DEVICE,
	YT_DATABASE_OPEN_CONFIGURE_DEVICE,
	YT_DATABASE_OPEN_EXTENDED_ERROR,
};

struct yt_database_open_observation {
	FILE *file;
	bool carry;
	bool device;
	bool handle_open;
	uint16_t dos_error;
	uint16_t mapped_error;
};

enum yt_database_open_outcome {
	YT_DATABASE_OPEN_NONE,
	YT_DATABASE_OPEN_RETURNED,
	YT_DATABASE_OPEN_INITIAL_ERROR,
	YT_DATABASE_OPEN_CREATE_ERROR,
	YT_DATABASE_OPEN_TEMP_CLOSE_ERROR,
	YT_DATABASE_OPEN_REOPEN_ERROR,
	YT_DATABASE_OPEN_DEVICE_ERROR,
	YT_DATABASE_OPEN_PROVIDER_ERROR,
	YT_DATABASE_OPEN_SIZE_ERROR,
};

struct yt_database_open_result {
	enum yt_database_open_outcome outcome;
	uint16_t dos_error;
	uint16_t basic_error;
	uint8_t access_attempts[6];
	size_t access_attempt_count;
	size_t operation_count;
	bool created;
	bool temporary_close_attempted;
	bool temporary_close_retried;
	bool device;
	bool registered;
	bool handle_open;
};

struct yt_database_close_observation {
	bool carry;
	bool handle_open;
	uint16_t dos_error;
};

enum yt_database_close_outcome {
	YT_DATABASE_CLOSE_NONE,
	YT_DATABASE_CLOSE_RETURNED,
	YT_DATABASE_CLOSE_DISK_ERROR,
	YT_DATABASE_CLOSE_DEVICE_ERROR,
	YT_DATABASE_CLOSE_PROVIDER_ERROR,
};

struct yt_database_close_result {
	enum yt_database_close_outcome outcome;
	uint16_t dos_error;
	uint16_t basic_error;
	size_t attempt_count;
	bool missing;
	bool retry_attempted;
	bool device;
	bool close_all;
	bool registered;
	bool handle_open;
};

/*
 * CLOSE_NO_ARGS scans general-heap controls in physical high-to-low order.
 * The array is supplied in low-to-high allocator order; only live type-3
 * controls dispatch a class-specific close method.  Type-1 and type-2 blocks
 * remain visible so their skip/order behavior can be retained exactly.
 */
enum yt_close_all_heap_type {
	YT_CLOSE_ALL_HEAP_FREE = 1,
	YT_CLOSE_ALL_HEAP_NON_FILE = 2,
	YT_CLOSE_ALL_HEAP_FILE = 3,
};

typedef bool (*yt_close_all_method)(void *context, int8_t file_class,
	struct yt_error *error);
typedef void (*yt_close_all_fixed_method)(void *context);

struct yt_close_all_control {
	enum yt_close_all_heap_type heap_type;
	int8_t file_class;
	yt_close_all_method method;
	void *context;
};

struct yt_close_all_fixed_control {
	size_t *lazy_open_count;
	yt_close_all_fixed_method method;
	void *context;
};

struct yt_close_all_result {
	size_t scanned_count;
	size_t attempt_count;
	size_t completed_count;
	size_t failed_index;
	bool failed;
	bool fixed_was_open;
	bool fixed_close_attempted;
	bool returned;
};

enum yt_database_lof_operation {
	YT_DATABASE_LOF_OPERATION_NONE,
	YT_DATABASE_LOF_CURRENT,
	YT_DATABASE_LOF_END,
	YT_DATABASE_LOF_RESTORE,
};

struct yt_database_lof_observation {
	bool carry;
	uint16_t dos_error;
	int64_t terminal_position;
};

enum yt_database_lof_outcome {
	YT_DATABASE_LOF_NONE,
	YT_DATABASE_LOF_RETURNED,
	YT_DATABASE_LOF_SEEK_ERROR,
	YT_DATABASE_LOF_PROVIDER_ERROR,
};

struct yt_database_lof_result {
	enum yt_database_lof_outcome outcome;
	enum yt_database_lof_operation failed_operation;
	uint32_t length;
	uint32_t saved_position;
	uint16_t dos_error;
	uint16_t basic_error;
	size_t operation_count;
	int64_t terminal_position;
	bool device;
	bool registered;
	bool handle_open;
};

/* A false provider return rejects the observation and performs no I/O. */
typedef bool (*yt_database_open_provider)(void *context, const char *path,
    enum yt_database_open_operation operation, uint8_t access,
    FILE *active_file, uint16_t prior_dos_error,
    struct yt_database_open_observation *observation);
/* A false provider return rejects the observation and performs no I/O. */
typedef bool (*yt_database_close_provider)(void *context, FILE *active_file,
    size_t attempt, struct yt_database_close_observation *observation);
/* A false provider return rejects the observation and performs no I/O. */
typedef bool (*yt_database_lof_provider)(void *context, FILE *active_file,
    enum yt_database_lof_operation operation, uint32_t restore_position,
    struct yt_database_lof_observation *observation);

struct yt_database_seek_observation {
	bool carry;
	uint16_t dos_error;
	int64_t terminal_position;
};

struct yt_database_write_observation {
	size_t accepted;
	bool carry;
	uint16_t dos_error;
	uint16_t mapped_error;
	int64_t terminal_position;
};

struct yt_database_read_observation {
	size_t accepted;
	bool carry;
	uint16_t dos_error;
	uint16_t mapped_error;
	int64_t terminal_position;
};

enum yt_database_get_outcome {
	YT_DATABASE_GET_NONE,
	YT_DATABASE_GET_RETURNED,
	YT_DATABASE_GET_RECORD_ERROR,
	YT_DATABASE_GET_SEEK_ERROR,
	YT_DATABASE_GET_READ_ERROR,
	YT_DATABASE_GET_PROVIDER_ERROR,
};

struct yt_database_get_result {
	enum yt_database_get_outcome outcome;
	size_t accepted;
	uint32_t current_record;
	uint32_t record_index;
	uint16_t dos_error;
	uint16_t basic_error;
	int64_t desired_offset;
	int64_t terminal_position;
	bool full_record;
	bool registered;
	bool handle_open;
};

enum yt_database_put_outcome {
	YT_DATABASE_PUT_NONE,
	YT_DATABASE_PUT_RETURNED,
	YT_DATABASE_PUT_RECORD_ERROR,
	YT_DATABASE_PUT_SEEK_ERROR,
	YT_DATABASE_PUT_WRITE_ERROR,
	YT_DATABASE_PUT_REJECTED_SHORT,
	YT_DATABASE_PUT_PROVIDER_ERROR,
};

struct yt_database_put_result {
	enum yt_database_put_outcome outcome;
	size_t accepted;
	uint32_t current_record;
	uint32_t record_index;
	uint16_t dos_error;
	uint16_t basic_error;
	int64_t desired_offset;
	int64_t terminal_position;
	bool registered;
	bool close_attempted;
	bool close_succeeded;
	bool handle_open;
};

/* A false provider return rejects the observation and performs no I/O. */
typedef bool (*yt_database_seek_provider)(void *context, FILE *file,
    int64_t absolute_offset, struct yt_database_seek_observation *observation);
/* A false provider return rejects the observation and performs no I/O. */
typedef bool (*yt_database_read_provider)(void *context, FILE *file,
    uint8_t *data, size_t requested,
    struct yt_database_read_observation *observation);
/* A false provider return rejects the observation and performs no I/O. */
typedef bool (*yt_database_write_provider)(void *context, FILE *file,
    const uint8_t *data, size_t requested,
    struct yt_database_write_observation *observation);
typedef bool (*yt_database_flush_provider)(void *context, FILE *file);

struct yt_database {
	FILE *file;
	FILE *orphaned_file;
	char path[512];
	size_t records;
	yt_database_seek_provider seek_provider;
	void *seek_context;
	yt_database_read_provider read_provider;
	void *read_context;
	yt_database_write_provider write_provider;
	void *write_context;
	yt_database_close_provider close_provider;
	void *close_context;
	yt_database_lof_provider lof_provider;
	void *lof_context;
	yt_database_flush_provider flush_provider;
	void *flush_context;
	uint32_t device_position;
	bool short_close_attempted;
	bool short_close_succeeded;
	struct yt_database_open_result last_open;
	struct yt_database_close_result last_close;
	struct yt_database_lof_result last_lof;
	struct yt_database_get_result last_get;
	struct yt_database_put_result last_put;
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

enum yt_file_kill_operation {
	YT_FILE_KILL_FIND_FIRST,
	YT_FILE_KILL_CHECK_OPEN,
	YT_FILE_KILL_DELETE,
	YT_FILE_KILL_FIND_NEXT,
};

struct yt_file_kill_observation {
	bool carry;
	bool open_collision;
	uint16_t dos_error;
};

typedef bool (*yt_file_kill_provider)(void *context,
	enum yt_file_kill_operation operation, const char *source,
	char *selected, size_t selected_size,
	struct yt_file_kill_observation *observation);

enum yt_file_kill_outcome {
	YT_FILE_KILL_NONE,
	YT_FILE_KILL_RETURNED,
	YT_FILE_KILL_FIND_ERROR,
	YT_FILE_KILL_OPEN_ERROR,
	YT_FILE_KILL_DELETE_ERROR,
	YT_FILE_KILL_PROVIDER_ERROR,
};

struct yt_file_kill_result {
	enum yt_file_kill_outcome outcome;
	enum yt_file_kill_operation failed_operation;
	size_t operation_count;
	size_t deleted_count;
	uint16_t dos_error;
	uint16_t basic_error;
	char selected_path[512];
	bool found;
	bool checked_open;
	bool deleted;
	bool find_next_attempted;
};

enum yt_file_rename_operation {
	YT_FILE_RENAME_PARSE_OLD,
	YT_FILE_RENAME_PARSE_NEW,
	YT_FILE_RENAME_CHECK_OLD_OPEN,
	YT_FILE_RENAME_CHECK_NEW_OPEN,
	YT_FILE_RENAME_RENAME,
};

struct yt_file_rename_observation {
	bool carry;
	bool open_collision;
	bool path_error;
	uint16_t dos_error;
};

typedef bool (*yt_file_rename_provider)(void *context,
	enum yt_file_rename_operation operation, const char *old_source,
	const char *new_source, char *selected_old, size_t selected_old_size,
	char *selected_new, size_t selected_new_size,
	struct yt_file_rename_observation *observation);

enum yt_file_rename_outcome {
	YT_FILE_RENAME_NONE,
	YT_FILE_RENAME_RETURNED,
	YT_FILE_RENAME_OLD_PATH_ERROR,
	YT_FILE_RENAME_NEW_PATH_ERROR,
	YT_FILE_RENAME_OLD_OPEN_ERROR,
	YT_FILE_RENAME_NEW_OPEN_ERROR,
	YT_FILE_RENAME_DOS_ERROR,
	YT_FILE_RENAME_PROVIDER_ERROR,
};

struct yt_file_rename_result {
	enum yt_file_rename_outcome outcome;
	enum yt_file_rename_operation failed_operation;
	size_t operation_count;
	uint16_t dos_error;
	uint16_t basic_error;
	char selected_old[512];
	char selected_new[512];
	bool old_parsed;
	bool new_parsed;
	bool old_checked_open;
	bool new_checked_open;
	bool renamed;
};

bool yt_resolve_case_path(const char *requested, bool allow_missing,
    char *resolved, size_t size, struct yt_error *error);
bool yt_database_open(struct yt_database *database, const char *path,
    enum yt_open_mode mode, struct yt_error *error);
bool yt_database_open_observed(struct yt_database *database, const char *path,
    enum yt_open_mode mode, yt_database_open_provider provider, void *context,
    struct yt_error *error);
bool yt_database_random_close(struct yt_database *database,
    struct yt_error *error);
/* CLOSE-all projection for a registry known to contain at most this control. */
bool yt_database_close_all_single(struct yt_database *database,
    struct yt_error *error);
bool yt_database_close_all_method(void *context, int8_t file_class,
    struct yt_error *error);
bool yt_close_all_run(const struct yt_close_all_control *controls,
    size_t control_count, const struct yt_close_all_fixed_control *fixed,
    struct yt_close_all_result *result, struct yt_error *error);
bool yt_database_random_lof(struct yt_database *database, uint32_t *length,
    struct yt_error *error);
bool yt_random_file_lof(FILE *file, const char *path, uint32_t *length,
    struct yt_database_lof_result *result, struct yt_error *error);
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
void yt_database_set_read_provider(struct yt_database *database,
    yt_database_read_provider provider, void *context);
void yt_database_set_seek_provider(struct yt_database *database,
    yt_database_seek_provider provider, void *context);
void yt_database_set_close_provider(struct yt_database *database,
    yt_database_close_provider provider, void *context);
void yt_database_set_lof_provider(struct yt_database *database,
    yt_database_lof_provider provider, void *context);
void yt_database_set_flush_provider(struct yt_database *database,
    yt_database_flush_provider provider, void *context);
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
bool yt_file_kill(const char *path, struct yt_file_kill_result *result,
	struct yt_error *error);
bool yt_file_kill_observed(const char *path, yt_file_kill_provider provider,
	void *context, struct yt_file_kill_result *result,
	struct yt_error *error);
bool yt_file_delete(const char *path, bool missing_ok, struct yt_error *error);
bool yt_file_rename(const char *old_path, const char *new_path,
    struct yt_error *error);
bool yt_file_rename_observed(const char *old_path, const char *new_path,
	yt_file_rename_provider provider, void *context,
	struct yt_file_rename_result *result, struct yt_error *error);
bool yt_file_size(const char *path, size_t *size, struct yt_error *error);

#endif
