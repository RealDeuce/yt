#ifndef TEXT_TEST_SUPPORT_H
#define TEXT_TEST_SUPPORT_H

#include "yt_common.h"

struct yt_text_file {
	uint8_t *data;
	size_t length;
};

bool yt_text_read(const char *path, struct yt_text_file *text,
    struct yt_error *error);
void yt_text_free(struct yt_text_file *text);

#endif
