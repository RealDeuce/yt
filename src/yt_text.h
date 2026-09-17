#ifndef YT_TEXT_H
#define YT_TEXT_H

#include "yt_common.h"

bool yt_text_write(const char *path, const uint8_t *data, size_t length,
    bool dos_eof, struct yt_error *error);
bool yt_text_append_line(const char *path, const uint8_t *line, size_t length,
    struct yt_error *error);
#define YT_TEXT_OUTPUT_BUFFER_SIZE 128U
#define YT_TEXT_INPUT_BUFFER_SIZE 128U

struct yt_text_input {
	FILE *file;
	char path[512];
	bool device;
	uint8_t *line;
	size_t line_capacity;
	uint8_t read_ahead[YT_TEXT_INPUT_BUFFER_SIZE];
	size_t read_total;
	size_t read_remaining;
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

struct yt_text_output {
	FILE *file;
	FILE *orphaned_file;
	char path[512];
	bool device;
	uint8_t pending[YT_TEXT_OUTPUT_BUFFER_SIZE];
	size_t pending_count;
	uint16_t last_output_open_basic_error;
	uint16_t last_append_open_basic_error;
	uint16_t last_write_basic_error;
	uint16_t last_close_basic_error;
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
