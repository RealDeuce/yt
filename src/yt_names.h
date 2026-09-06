#ifndef YT_NAMES_H
#define YT_NAMES_H

#include "yt_common.h"

struct yt_name_row {
	char *real_first;
	char *real_last;
	char *alias_first;
	char *alias_last;
};

struct yt_name_file {
	struct yt_name_row *rows;
	size_t count;
};

enum yt_alias_key_status {
	YT_ALIAS_KEY_READY,
	YT_ALIAS_KEY_EMPTY,
	YT_ALIAS_KEY_RESERVED,
	YT_ALIAS_KEY_RANGE
};

bool yt_names_load(const char *path, struct yt_name_file *names,
    struct yt_error *error);
void yt_names_free(struct yt_name_file *names);
bool yt_names_write(const char *path, const struct yt_name_file *names,
    struct yt_error *error);
bool yt_names_append(const char *path, const struct yt_name_row *row,
    struct yt_error *error);
bool yt_names_set_alias(struct yt_name_file *names, size_t index,
    const char *first, const char *last, struct yt_error *error);
const struct yt_name_row *yt_names_find_real_last(
    const struct yt_name_file *names, const char *first, const char *last);
bool yt_names_alias_exists(const struct yt_name_file *names,
    const char *first, const char *last);
void yt_names_split(const char *name, char *first, size_t first_size,
    char *last, size_t last_size);
enum yt_alias_key_status yt_names_prepare_alias(char *alias,
    size_t alias_size, const char *real_first, const char *real_last,
    char *alias_first, size_t first_size, char *alias_last,
    size_t last_size, char *display, size_t display_size);
bool yt_names_create_default(struct yt_error *error);

#endif
