#ifndef YT_NAMES_H
#define YT_NAMES_H

#include "yt_common.h"

struct yt_name_row {
	char real_first[128];
	char real_last[128];
	char alias_first[128];
	char alias_last[128];
};

struct yt_name_file {
	struct yt_name_row *rows;
	size_t count;
};

bool yt_names_load(const char *path, struct yt_name_file *names,
    struct yt_error *error);
void yt_names_free(struct yt_name_file *names);
bool yt_names_write(const char *path, const struct yt_name_file *names,
    struct yt_error *error);
bool yt_names_append(const char *path, const struct yt_name_row *row,
    struct yt_error *error);
const struct yt_name_row *yt_names_find_real_last(
    const struct yt_name_file *names, const char *first, const char *last);
bool yt_names_alias_exists(const struct yt_name_file *names,
    const char *first, const char *last);
void yt_names_split(const char *name, char *first, size_t first_size,
    char *last, size_t last_size);
bool yt_names_create_default(struct yt_error *error);

#endif

