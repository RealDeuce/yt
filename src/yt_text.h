#ifndef YT_TEXT_H
#define YT_TEXT_H

#include "yt_common.h"

struct yt_text_file {
	uint8_t *data;
	size_t length;
};

bool yt_text_read(const char *path, struct yt_text_file *text,
    struct yt_error *error);
void yt_text_free(struct yt_text_file *text);
bool yt_text_write(const char *path, const uint8_t *data, size_t length,
    bool dos_eof, struct yt_error *error);
bool yt_text_append_line(const char *path, const uint8_t *line, size_t length,
    struct yt_error *error);
bool yt_text_line_input_next(const uint8_t *data, size_t data_length,
    size_t *cursor, uint8_t *line, size_t capacity, size_t *line_length,
    bool *available);

enum yt_text_stream_line_status {
	YT_TEXT_STREAM_LINE_OK,
	YT_TEXT_STREAM_LINE_EOF,
	YT_TEXT_STREAM_LINE_TOO_LONG,
	YT_TEXT_STREAM_LINE_IO_ERROR,
};

enum yt_text_stream_line_status yt_text_stream_line_input_next(FILE *file,
    uint8_t *line, size_t capacity, size_t *line_length);

#define YT_TEXT_OUTPUT_BUFFER_SIZE 128U
#define YT_TEXT_INPUT_BUFFER_SIZE 128U

#define YT_TEXT_DEVICE_SCRN 0xFEU
#define YT_TEXT_DEVICE_CONS 0xFDU
#define YT_TEXT_DEVICE_COM1 0xFCU
#define YT_TEXT_DEVICE_COM2 0xFBU
#define YT_TEXT_DEVICE_LPT1 0xFAU
#define YT_TEXT_DEVICE_LPT2 0xF9U
#define YT_TEXT_DEVICE_LPT3 0xF8U
#define YT_TEXT_DEVICE_PROCESS_SIZE 0x10000U

struct yt_text_device_process_state {
	uint8_t *process;
	size_t process_size;
	bool physical_unknown;
};

enum yt_text_open_operation {
	YT_TEXT_OPEN_EXISTING,
	YT_TEXT_OPEN_CREATE,
	YT_TEXT_OPEN_TEMP_CLOSE,
	YT_TEXT_OPEN_REOPEN,
	YT_TEXT_OPEN_EXTENDED_ERROR,
	YT_TEXT_OPEN_QUERY_DEVICE,
	YT_TEXT_OPEN_CONFIGURE_DEVICE,
	YT_TEXT_OPEN_SEEK_END,
	YT_TEXT_OPEN_SEEK_WINDOW,
	YT_TEXT_OPEN_READ_WINDOW,
	YT_TEXT_OPEN_SEEK_SELECTED,
};

struct yt_text_open_observation {
	FILE *file;
	size_t accepted;
	bool carry;
	bool device;
	bool handle_open;
	uint16_t dos_error;
	uint16_t mapped_error;
	int64_t terminal_position;
};

typedef bool (*yt_text_open_provider)(void *context,
	const char *path, enum yt_text_open_operation operation,
	uint8_t access, FILE *active_file, int64_t offset, uint8_t *data,
	size_t requested, uint16_t prior_dos_error,
	struct yt_text_open_observation *observation);

enum yt_text_open_outcome {
	YT_TEXT_OPEN_NONE,
	YT_TEXT_OPEN_RETURNED,
	YT_TEXT_OPEN_INITIAL_ERROR,
	YT_TEXT_OPEN_CREATE_ERROR,
	YT_TEXT_OPEN_TEMP_CLOSE_ERROR,
	YT_TEXT_OPEN_REOPEN_ERROR,
	YT_TEXT_OPEN_DEVICE_ERROR,
	YT_TEXT_OPEN_SEEK_ERROR,
	YT_TEXT_OPEN_READ_ERROR,
	YT_TEXT_OPEN_PROVIDER_ERROR,
};

struct yt_text_open_result {
	enum yt_text_open_outcome outcome;
	enum yt_text_open_operation failed_operation;
	uint16_t dos_error;
	uint16_t basic_error;
	uint16_t temporary_close_retry_dos_error;
	uint8_t access_attempts[4];
	size_t access_attempt_count;
	size_t operation_count;
	size_t refill_count;
	size_t accepted;
	int64_t physical_length;
	int64_t window_start;
	int64_t selected_position;
	int64_t terminal_position;
	bool created;
	bool temporary_close_attempted;
	bool temporary_close_retried;
	bool device;
	bool registered;
	bool handle_open;
};

enum yt_text_close_operation {
	YT_TEXT_CLOSE_PENDING_WRITE,
	YT_TEXT_CLOSE_EOF_WRITE,
	YT_TEXT_CLOSE_TRUNCATE,
	YT_TEXT_CLOSE_HANDLE,
	YT_TEXT_CLOSE_CLEANUP_HANDLE,
};

struct yt_text_close_observation {
	size_t accepted;
	bool carry;
	bool handle_open;
	uint16_t dos_error;
	int64_t terminal_position;
};

typedef bool (*yt_text_close_provider)(void *context, FILE *file,
	enum yt_text_close_operation operation, const uint8_t *data,
	size_t requested, struct yt_text_close_observation *observation);

enum yt_text_close_outcome {
	YT_TEXT_CLOSE_NONE,
	YT_TEXT_CLOSE_RETURNED,
	YT_TEXT_CLOSE_SHORT_ERROR,
	YT_TEXT_CLOSE_DISK_ERROR,
	YT_TEXT_CLOSE_PROVIDER_ERROR,
};

struct yt_text_close_result {
	enum yt_text_close_outcome outcome;
	enum yt_text_close_operation failed_operation;
	size_t operation_count;
	size_t accepted;
	uint16_t dos_error;
	uint16_t basic_error;
	uint16_t cleanup_dos_error;
	int64_t terminal_position;
	bool close_all;
	bool missing;
	bool device;
	bool cleanup_close_attempted;
	bool registered;
	bool handle_open;
};

struct yt_text_input_read_observation {
	size_t accepted;
	bool carry;
	uint16_t dos_error;
	uint16_t basic_error;
	int64_t terminal_position;
};

typedef bool (*yt_text_input_read_provider)(void *context, FILE *file,
	uint8_t *data, size_t requested,
	struct yt_text_input_read_observation *observation);

enum yt_text_input_read_outcome {
	YT_TEXT_INPUT_READ_NONE,
	YT_TEXT_INPUT_READ_RETURNED,
	YT_TEXT_INPUT_READ_DISK_ERROR,
	YT_TEXT_INPUT_READ_PROVIDER_ERROR,
	YT_TEXT_INPUT_READ_MEMORY_ERROR,
};

struct yt_text_input_read_result {
	enum yt_text_input_read_outcome outcome;
	size_t operation_count;
	size_t accepted;
	size_t consumed;
	size_t returned;
	uint16_t dos_error;
	uint16_t basic_error;
	uint32_t refill_index;
	size_t buffer_total;
	size_t buffer_remaining;
	uint64_t logical_position;
	int64_t terminal_position;
	bool eof_probe;
	bool eof;
	bool buffer_cleared;
	bool registered;
	bool handle_open;
};

struct yt_text_input {
	FILE *file;
	FILE *orphaned_file;
	char path[512];
	uint8_t *line;
	size_t line_capacity;
	uint8_t read_ahead[YT_TEXT_INPUT_BUFFER_SIZE];
	size_t read_total;
	size_t read_remaining;
	uint32_t refill_index;
	uint64_t logical_position;
	int64_t physical_position;
	yt_text_open_provider open_provider;
	void *open_context;
	yt_text_input_read_provider read_provider;
	void *read_context;
	yt_text_close_provider close_provider;
	void *close_context;
	struct yt_text_open_result last_open;
	struct yt_text_input_read_result last_read;
	struct yt_text_close_result last_close;
};

void yt_text_input_init(struct yt_text_input *input);
bool yt_text_input_open(struct yt_text_input *input, const char *path,
	struct yt_error *error);
bool yt_text_input_read_line(struct yt_text_input *input,
	const uint8_t **line, size_t *length, bool *available,
	struct yt_error *error);
bool yt_text_input_read_string_token(struct yt_text_input *input,
	const uint8_t **value, size_t *length, bool *available,
	struct yt_error *error);
bool yt_text_input_eof(struct yt_text_input *input, bool *eof,
	struct yt_error *error);
bool yt_text_input_close(struct yt_text_input *input,
	struct yt_error *error);
void yt_text_input_set_open_provider(struct yt_text_input *input,
	yt_text_open_provider provider, void *context);
void yt_text_input_set_read_provider(struct yt_text_input *input,
	yt_text_input_read_provider provider, void *context);
void yt_text_input_set_close_provider(struct yt_text_input *input,
	yt_text_close_provider provider, void *context);
void yt_text_input_destroy(struct yt_text_input *input);

struct yt_text_output_write_observation {
	size_t accepted;
	bool carry;
	bool handle_open;
	uint16_t dos_error;
	int64_t terminal_position;
};

typedef bool (*yt_text_output_write_provider)(void *context, FILE *file,
	const uint8_t *data, size_t requested,
	struct yt_text_output_write_observation *observation);

enum yt_text_output_write_outcome {
	YT_TEXT_OUTPUT_WRITE_NONE,
	YT_TEXT_OUTPUT_WRITE_RETURNED,
	YT_TEXT_OUTPUT_WRITE_SHORT_ERROR,
	YT_TEXT_OUTPUT_WRITE_DISK_ERROR,
	YT_TEXT_OUTPUT_WRITE_PROVIDER_ERROR,
};

struct yt_text_output_write_result {
	enum yt_text_output_write_outcome outcome;
	size_t flush_count;
	size_t accepted;
	size_t failed_flush_accepted;
	uint16_t dos_error;
	uint16_t basic_error;
	uint16_t cleanup_dos_error;
	int64_t terminal_position;
	bool physical_unknown;
	bool cleanup_close_attempted;
	bool registered;
	bool handle_open;
};

enum yt_text_output_process_outcome {
	YT_TEXT_OUTPUT_PROCESS_NONE,
	YT_TEXT_OUTPUT_PROCESS_RETURNED,
	YT_TEXT_OUTPUT_PROCESS_RUNTIME_ERROR,
	YT_TEXT_OUTPUT_PROCESS_PROVIDER_BOUNDARY,
	YT_TEXT_OUTPUT_PROCESS_INTERNAL_ERROR,
};

struct yt_text_output_process_result {
	enum yt_text_output_process_outcome outcome;
	uint16_t control;
	uint16_t type_address;
	uint16_t basic_error;
	uint16_t internal_entry;
	bool released;
};

enum yt_text_device_write_phase {
	YT_TEXT_DEVICE_WRITE_VALUE,
	YT_TEXT_DEVICE_WRITE_COMPLETION,
};

struct yt_text_device_write_observation {
	size_t accepted;
	bool carry;
	bool physical_unknown;
	uint16_t dos_error;
	uint16_t extended_ax;
	int64_t terminal_position;
};

typedef bool (*yt_text_device_write_provider)(void *context,
	enum yt_text_device_write_phase phase, const uint8_t *data,
	size_t requested, struct yt_text_device_write_observation *observation);

struct yt_text_device_close_observation {
	bool carry;
	uint16_t dos_error;
};

typedef bool (*yt_text_device_close_provider)(void *context,
	struct yt_text_device_close_observation *observation);

/*
 * The raw PRINT error suffix keeps its selected-file pointer even when A43D
 * releases the pointed-to control.  Pointers in this carrier therefore name
 * controls independently of their current allocator/registration state.
 */
struct yt_text_device_control_state {
	bool allocated;
	bool registered;
	size_t field_binding_count;
};

struct yt_text_device_runtime_state {
	uint8_t error_status;
	bool defer_release;
	struct yt_text_device_control_state *selected_control;
	struct yt_text_device_control_state *active_close_control;
	yt_text_device_close_provider close_provider;
	void *close_context;
	size_t cleanup_close_count;
	bool cleanup_close_observed;
	bool cleanup_close_carry;
	uint16_t cleanup_close_dos_error;
};

enum yt_text_device_print_outcome {
	YT_TEXT_DEVICE_PRINT_NONE,
	YT_TEXT_DEVICE_PRINT_RETURNED,
	YT_TEXT_DEVICE_PRINT_VALUE_SHORT_ERROR,
	YT_TEXT_DEVICE_PRINT_VALUE_DISK_ERROR,
	YT_TEXT_DEVICE_PRINT_COMPLETION_ERROR,
	YT_TEXT_DEVICE_PRINT_RAW_INTERNAL_ERROR,
	YT_TEXT_DEVICE_PRINT_PROVIDER_ERROR,
};

struct yt_text_device_state {
	uint32_t index;
	uint8_t column;
	uint8_t buffer;
	bool pending;
	bool selected;
	bool physical_unknown;
};

struct yt_text_device_print_result {
	enum yt_text_device_print_outcome outcome;
	size_t logical_length;
	size_t staged;
	size_t write_count;
	size_t accepted_count;
	uint16_t dos_error;
	uint16_t basic_error;
	int64_t terminal_position;
	bool physical_unknown;
	bool selected;
	bool raw_release_attempted;
	uint16_t released_control;
	uint16_t internal_entry;
};

struct yt_text_output {
	FILE *file;
	FILE *orphaned_file;
	char path[512];
	uint8_t pending[YT_TEXT_OUTPUT_BUFFER_SIZE];
	size_t pending_count;
	yt_text_open_provider open_provider;
	void *open_context;
	yt_text_output_write_provider write_provider;
	void *write_context;
	yt_text_close_provider close_provider;
	void *close_context;
	struct yt_text_output_write_result last_write;
	struct yt_text_close_result last_close;
	struct yt_text_open_result last_output_open;
	struct yt_text_open_result last_append_open;
};

void yt_text_output_init(struct yt_text_output *output);
bool yt_text_output_open(struct yt_text_output *output, const char *path,
	struct yt_error *error);
bool yt_text_output_open_append(struct yt_text_output *output,
	const char *path, struct yt_error *error);
bool yt_text_output_stage(struct yt_text_output *output,
	const uint8_t *data, size_t length, struct yt_error *error);
bool yt_text_output_write(struct yt_text_output *output,
	const uint8_t *data, size_t length, struct yt_error *error);
bool yt_text_device_print(struct yt_text_device_state *state,
	const uint8_t *data, size_t length, bool newline, uint8_t device_code,
	uint8_t status, uint8_t dos_major,
	yt_text_device_write_provider provider, void *context,
	struct yt_text_device_print_result *result, struct yt_error *error);
bool yt_text_device_print_runtime(struct yt_text_device_state *state,
	const uint8_t *data, size_t length, bool newline, uint8_t device_code,
	uint8_t status, uint8_t dos_major,
	yt_text_device_write_provider provider, void *context,
	struct yt_text_device_runtime_state *runtime,
	struct yt_text_device_print_result *result, struct yt_error *error);
/* Commit the infallible DS prefix after a file number has resolved to control. */
bool yt_text_device_print_process_prepare(
	struct yt_text_device_process_state *process_state, uint16_t control,
	uint16_t selector_handler_sp, uint16_t value_handler_sp, bool newline,
	struct yt_error *error);
/*
 * DS-only projection of the shared character-device reducer.  It owns the
 * selected control device fields and external unknown-prefix lane; selector
 * setup, allocator/FIELD cleanup, and CPU/SS frames remain separate.
 */
bool yt_text_device_print_process(
	struct yt_text_device_process_state *process_state,
	const uint8_t *data, size_t length, bool newline,
	yt_text_device_write_provider provider, void *context,
	struct yt_text_device_runtime_state *runtime,
	struct yt_text_device_print_result *result, struct yt_error *error);
bool yt_text_output_close(struct yt_text_output *output,
	struct yt_error *error);
bool yt_text_output_write_process_apply(uint8_t *process,
	size_t process_size, uint16_t control,
	const struct yt_text_output_write_result *write_result,
	struct yt_text_output_process_result *result, struct yt_error *error);
bool yt_text_output_close_process_apply(uint8_t *process,
	size_t process_size, uint16_t control,
	const struct yt_text_close_result *close_result,
	struct yt_text_output_process_result *result, struct yt_error *error);
bool yt_text_output_close_all_method(void *context, int8_t file_class,
	struct yt_error *error);
void yt_text_output_set_close_provider(struct yt_text_output *output,
	yt_text_close_provider provider, void *context);
void yt_text_output_set_write_provider(struct yt_text_output *output,
	yt_text_output_write_provider provider, void *context);
void yt_text_output_set_open_provider(struct yt_text_output *output,
	yt_text_open_provider provider, void *context);
void yt_text_output_destroy(struct yt_text_output *output);

struct yt_text_sequential_play_state {
	const char *path;
	bool file_open;
	size_t read_count;
	size_t line_count;
};

typedef bool (*yt_text_sequential_close_fn)(void *context,
	struct yt_error *error);
typedef bool (*yt_text_sequential_open_fn)(void *context, const char *path,
	struct yt_error *error);
typedef bool (*yt_text_sequential_read_fn)(void *context,
	const uint8_t **line, size_t *length, bool *available,
	struct yt_error *error);
typedef bool (*yt_text_sequential_present_fn)(void *context,
	const uint8_t *line, size_t length, struct yt_error *error);

struct yt_text_sequential_play_ops {
	yt_text_sequential_close_fn close;
	yt_text_sequential_open_fn open;
	yt_text_sequential_read_fn read;
	yt_text_sequential_present_fn present;
};

bool yt_text_sequential_play_run(
	struct yt_text_sequential_play_state *state,
	const struct yt_text_sequential_play_ops *ops, void *context,
	struct yt_error *error);

struct yt_file_viewer_record {
	bool eof_checked;
	bool key_checked;
	bool available;
	int foreground;
	bool set_bold;
	size_t length;
};

bool yt_file_viewer_next(const uint8_t *data, size_t data_length,
    size_t *cursor, const char *pager_key, uint8_t *line, size_t capacity,
    struct yt_file_viewer_record *record);

struct yt_file_viewer_play_state {
	float *foreground;
	int *pager_foreground;
	float *bold;
	void (*set_bold)(void *context, float value);
	float *line_count;
	char *pager_key;
	float saved_foreground;
	int saved_pager_foreground;
};

typedef bool (*yt_file_viewer_present_fn)(void *context,
    const uint8_t *text, size_t length, bool paged,
    struct yt_error *error);

bool yt_file_viewer_entry(char *pager_key, float *line_count,
    yt_file_viewer_present_fn present, void *context,
    struct yt_error *error);

bool yt_file_viewer_play(const uint8_t *data, size_t data_length,
    struct yt_file_viewer_play_state *state,
    yt_file_viewer_present_fn present, void *context,
    struct yt_error *error);

struct yt_file_viewer_stream_state {
	struct yt_file_viewer_play_state play;
	const char *path;
	bool file_open;
	size_t eof_checks;
	size_t key_checks;
	size_t read_count;
	size_t line_count;
};

typedef bool (*yt_file_viewer_close_fn)(void *context,
	struct yt_error *error);
typedef bool (*yt_file_viewer_open_fn)(void *context, const char *path,
	struct yt_error *error);
typedef bool (*yt_file_viewer_eof_fn)(void *context, bool *eof,
	struct yt_error *error);
typedef bool (*yt_file_viewer_read_fn)(void *context, const uint8_t **line,
	size_t *length, bool *available, struct yt_error *error);

struct yt_file_viewer_stream_ops {
	yt_file_viewer_close_fn close;
	yt_file_viewer_open_fn open;
	yt_file_viewer_eof_fn eof;
	yt_file_viewer_read_fn read;
	yt_file_viewer_present_fn present;
};

bool yt_file_viewer_stream_run(struct yt_file_viewer_stream_state *state,
	const struct yt_file_viewer_stream_ops *ops, void *context,
	struct yt_error *error);

typedef bool (*yt_file_viewer_news_fn)(void *context,
    const uint8_t *text, size_t length, struct yt_error *error);

bool yt_file_viewer_missing(const uint8_t *path, size_t path_length,
    yt_file_viewer_present_fn present, yt_file_viewer_news_fn append_news,
    void *context, struct yt_error *error);

enum yt_opening_exit {
	YT_OPENING_EXIT_EOF,
	YT_OPENING_EXIT_LOCAL_KEY,
	YT_OPENING_EXIT_REMOTE_PENDING,
};

struct yt_opening_stream_state {
	const char *path;
	float mode;
	float snoop;
	enum yt_opening_exit exit_reason;
	bool input_open;
	bool local_open;
	bool waited;
	bool remote_reset;
	bool local_reset;
	size_t eof_checks;
	size_t read_count;
	size_t local_lines;
	size_t local_polls;
	size_t remote_lines;
	size_t remote_polls;
};

typedef bool (*yt_opening_open_input_fn)(void *context, const char *path,
	struct yt_error *error);
typedef bool (*yt_opening_action_fn)(void *context,
	struct yt_error *error);
typedef bool (*yt_opening_eof_fn)(void *context, bool *eof,
	struct yt_error *error);
typedef bool (*yt_opening_read_fn)(void *context, const uint8_t **line,
	size_t *length, bool *available, struct yt_error *error);
typedef bool (*yt_opening_present_fn)(void *context, const uint8_t *line,
	size_t length, struct yt_error *error);
typedef bool (*yt_opening_poll_fn)(void *context, bool *ready,
	struct yt_error *error);
typedef bool (*yt_opening_wait_fn)(void *context, float seconds,
	struct yt_error *error);

struct yt_opening_stream_ops {
	yt_opening_open_input_fn open_input;
	yt_opening_action_fn open_local;
	yt_opening_eof_fn eof;
	yt_opening_read_fn read;
	yt_opening_present_fn present_local;
	yt_opening_poll_fn poll_local;
	yt_opening_present_fn present_remote;
	yt_opening_poll_fn poll_remote;
	yt_opening_wait_fn wait;
	yt_opening_action_fn reset_remote;
	yt_opening_action_fn reset_local;
	yt_opening_action_fn close_input;
	yt_opening_action_fn close_local;
};

bool yt_opening_stream_run(struct yt_opening_stream_state *state,
	const struct yt_opening_stream_ops *ops, void *context,
	struct yt_error *error);

#endif
