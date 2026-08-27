#ifndef YT_SCORE_FORMAT_H
#define YT_SCORE_FORMAT_H

#include "yt_common.h"

enum yt_score_field {
	YT_SCORE_FIELD_RANK,
	YT_SCORE_FIELD_PORTS,
	YT_SCORE_FIELD_PERCENT,
	YT_SCORE_FIELD_XANNOR_PERCENT,
	YT_SCORE_FIELD_SCORE
};

bool yt_score_format_mbf(char *dest, size_t size, const uint8_t *raw,
    size_t raw_size, enum yt_score_field field);
bool yt_score_format_single(char *dest, size_t size, float value,
    enum yt_score_field field);
bool yt_score_format_double(char *dest, size_t size, double value,
    enum yt_score_field field);

#endif
