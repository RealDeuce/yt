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

struct yt_text_input {
	FILE *file;
	char path[512];
	uint8_t *line;
	size_t line_capacity;
};

void yt_text_input_init(struct yt_text_input *input);
bool yt_text_input_open(struct yt_text_input *input, const char *path,
	struct yt_error *error);
bool yt_text_input_read_line(struct yt_text_input *input,
	const uint8_t **line, size_t *length, bool *available,
	struct yt_error *error);
bool yt_text_input_eof(struct yt_text_input *input, bool *eof,
	struct yt_error *error);
bool yt_text_input_close(struct yt_text_input *input,
	struct yt_error *error);
void yt_text_input_destroy(struct yt_text_input *input);

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

#endif
