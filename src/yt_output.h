#ifndef YT_OUTPUT_H
#define YT_OUTPUT_H

#include "yt_common.h"
#include "yt_presentation.h"
#include "yt_text.h"

void yt_out(const char *text);
void yt_out_bytes(const void *data, size_t length);
void yt_out_remote_bytes(const void *data, size_t length);
void yt_out_present_result(const struct yt_present_result *result);
void yt_out_cursor_position(int *row, int *column);
void yt_outf(const char *format, ...);
void yt_out_line(const char *text);
void yt_out_clear(void);
bool yt_out_file(const char *path, struct yt_error *error);
typedef bool (*yt_out_opening_poll_fn)(void *context, bool *ready,
	struct yt_error *error);
typedef bool (*yt_out_opening_wait_fn)(void *context, float seconds,
	struct yt_error *error);
bool yt_out_opening_file(const char *path, float mode, float snoop,
	yt_out_opening_poll_fn poll_local,
	yt_out_opening_poll_fn poll_remote, yt_out_opening_wait_fn wait,
	void *poll_context, struct yt_error *error);

#endif
