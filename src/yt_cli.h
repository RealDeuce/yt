#ifndef YT_CLI_H
#define YT_CLI_H

#include "yt_common.h"

void yt_cli_error(const char *program, const struct yt_error *error);
bool yt_cli_line(char *text, size_t size);
int yt_cli_key(void);
bool yt_cli_write_dorinfo(const char *first, const char *last,
    struct yt_error *error);

#endif
