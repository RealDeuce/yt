#ifndef YT_FILE_H
#define YT_FILE_H

#include "yt_data.h"

enum yt_open_mode {
	YT_OPEN_READ,
	YT_OPEN_UPDATE,
	YT_OPEN_CREATE,
	YT_OPEN_UPDATE_CREATE
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
	uint16_t temporary_close_retry_dos_error;
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

enum yt_database_close_outcome {
	YT_DATABASE_CLOSE_NONE,
	YT_DATABASE_CLOSE_RETURNED,
	YT_DATABASE_CLOSE_DISK_ERROR,
	YT_DATABASE_CLOSE_DEVICE_ERROR,
};

struct yt_database_close_result {
	enum yt_database_close_outcome outcome;
	uint16_t dos_error;
	uint16_t basic_error;
	size_t attempt_count;
	bool missing;
	bool device;
	bool close_all;
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

enum yt_com_close_outcome {
	YT_COM_CLOSE_RETURNED,
	YT_COM_CLOSE_RUNTIME_ERROR,
};

struct yt_com_close_state {
	int8_t file_class;
	uint8_t binary_mode;
	uint8_t kind;
	uint16_t control;
	uint16_t port_descriptor;
	uint16_t transmit_count;
	uint32_t saved_vector;
	bool field_bound;
	bool control_live;
	bool interrupt_installed;
};

struct yt_com_close_observation {
	bool optional_status_supplied;
	uint8_t optional_status;
	const uint8_t *drain_statuses;
	size_t drain_status_count;
};

struct yt_com_close_result {
	enum yt_com_close_outcome outcome;
	uint16_t port_state_address;
	uint16_t basic_error;
	size_t drain_status_count;
	bool optional_eof_write;
	bool teardown_completed;
	bool control_released;
};

bool yt_com_close_run(struct yt_com_close_state *state,
    const struct yt_com_close_observation *observation,
    struct yt_com_close_result *result);

enum yt_database_lof_operation {
	YT_DATABASE_LOF_OPERATION_NONE,
	YT_DATABASE_LOF_CURRENT,
	YT_DATABASE_LOF_END,
	YT_DATABASE_LOF_RESTORE,
};

enum yt_database_lof_outcome {
	YT_DATABASE_LOF_NONE,
	YT_DATABASE_LOF_RETURNED,
	YT_DATABASE_LOF_SEEK_ERROR,
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

enum yt_database_get_outcome {
	YT_DATABASE_GET_NONE,
	YT_DATABASE_GET_RETURNED,
	YT_DATABASE_GET_RECORD_ERROR,
	YT_DATABASE_GET_SEEK_ERROR,
	YT_DATABASE_GET_READ_ERROR,
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
};

struct yt_database_put_result {
	enum yt_database_put_outcome outcome;
	size_t accepted;
	uint32_t current_record;
	uint32_t record_index;
	uint16_t dos_error;
	uint16_t basic_error;
	uint16_t close_dos_error;
	int64_t desired_offset;
	int64_t terminal_position;
	bool registered;
	bool close_attempted;
	bool close_succeeded;
	bool handle_open;
};

struct yt_database {
	FILE *file;
	char path[512];
	size_t records;
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

bool yt_resolve_case_path(const char *requested, bool allow_missing,
    char *resolved, size_t size, struct yt_error *error);
bool yt_database_open(struct yt_database *database, const char *path,
    enum yt_open_mode mode, struct yt_error *error);
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
bool yt_file_delete(const char *path, bool missing_ok, struct yt_error *error);
bool yt_file_rename(const char *old_path, const char *new_path,
    struct yt_error *error);
bool yt_file_size(const char *path, size_t *size, struct yt_error *error);

#endif
