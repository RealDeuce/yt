#ifndef YT_OUTPUT_H
#define YT_OUTPUT_H

#include "yt_common.h"
#include "yt_presentation.h"

void yt_out(const char *text);
void yt_out_bytes(const void *data, size_t length);
void yt_out_remote_bytes(const void *data, size_t length);
void yt_out_present_result(const struct yt_present_result *result);
void yt_out_cursor_position(int *row, int *column);
void yt_outf(const char *format, ...);
void yt_out_line(const char *text);
void yt_out_clear(void);
bool yt_out_file(const char *path, struct yt_error *error);

#endif
