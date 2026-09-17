#ifndef YT_OUTPUT_H
#define YT_OUTPUT_H

#include "yt_common.h"
#include "yt_input.h"
#include "yt_presentation.h"

void yt_out_plain(const char *text);
void yt_out_plain_bytes(const void *data, size_t length);
void yt_out_present_result(const struct yt_present_result *result);
void yt_out_cursor_position(int *row, int *column);
bool yt_out_opening_file(const char *path, float mode, float snoop,
	struct yt_input *input, uint16_t *basic_error,
	struct yt_error *error);

#endif
