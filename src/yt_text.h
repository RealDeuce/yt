#ifndef YT_TEXT_H
#define YT_TEXT_H

#include "yt_common.h"

bool yt_text_write(const char *path, const uint8_t *data, size_t length,
    bool dos_eof, struct yt_error *error);
bool yt_text_append_line(const char *path, const uint8_t *line, size_t length,
    struct yt_error *error);
#define YT_TEXT_OUTPUT_BUFFER_SIZE 128U
#define YT_TEXT_INPUT_BUFFER_SIZE 128U

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

struct yt_text_input {
	FILE *file;
	char path[512];
	bool device;
	uint8_t *line;
	size_t line_capacity;
	uint8_t read_ahead[YT_TEXT_INPUT_BUFFER_SIZE];
	size_t read_total;
	size_t read_remaining;
	uint64_t logical_position;
	uint16_t last_open_basic_error;
	uint16_t last_read_basic_error;
	uint16_t last_close_basic_error;
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
void yt_text_input_destroy(struct yt_text_input *input);

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

struct yt_text_output {
	FILE *file;
	FILE *orphaned_file;
	char path[512];
	uint8_t pending[YT_TEXT_OUTPUT_BUFFER_SIZE];
	size_t pending_count;
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
bool yt_text_output_write(struct yt_text_output *output,
	const uint8_t *data, size_t length, struct yt_error *error);
bool yt_text_output_close(struct yt_text_output *output,
	struct yt_error *error);
bool yt_text_output_close_all(struct yt_text_output *output,
	struct yt_error *error);
void yt_text_output_destroy(struct yt_text_output *output);

int yt_file_viewer_line_foreground(const uint8_t *line, size_t length);

bool yt_file_viewer_missing_row(const char *path, uint8_t *row,
	size_t capacity, size_t *length);

#endif
