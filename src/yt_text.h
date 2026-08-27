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

#endif

