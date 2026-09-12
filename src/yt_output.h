#ifndef YT_OUTPUT_H
#define YT_OUTPUT_H

#include "yt_common.h"
#include "yt_presentation.h"

void yt_out_plain(const char *text);
void yt_out_plain_bytes(const void *data, size_t length);
void yt_out_present_result(const struct yt_present_result *result);
void yt_out_cursor_position(int *row, int *column);
void yt_out_plain_line(const char *text);
void yt_out_clear(void);
typedef bool (*yt_out_opening_poll_fn)(void *context, bool *ready,
	struct yt_error *error);
typedef bool (*yt_out_opening_wait_fn)(void *context, float seconds,
	struct yt_error *error);
bool yt_out_opening_file(const char *path, float mode, float snoop,
	yt_out_opening_poll_fn poll_local,
	yt_out_opening_poll_fn poll_remote, yt_out_opening_wait_fn wait,
	void *poll_context, uint16_t *basic_error,
	struct yt_error *error);

#endif
