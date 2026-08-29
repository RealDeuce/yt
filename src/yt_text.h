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

bool yt_file_viewer_play(const uint8_t *data, size_t data_length,
    struct yt_file_viewer_play_state *state,
    yt_file_viewer_present_fn present, void *context,
    struct yt_error *error);

#endif
