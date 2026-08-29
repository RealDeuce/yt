#ifndef YT_INPUT_H
#define YT_INPUT_H

#include "yt_common.h"
#include "yt_input_model.h"

int yt_input_key(void);
int yt_input_command(void);
bool yt_input_line(char *text, size_t size);
bool yt_input_number(const char *prompt, double *value);
bool yt_input_yes_no(const char *prompt);
bool yt_input_poll_legacy(struct yt_input_splitter *splitter, float mode,
    enum yt_input_phase phase, struct yt_input_value *selected);
bool yt_input_poll_merged(struct yt_input_splitter *splitter,
    struct yt_input_value *selected);
bool yt_input_poll_source(struct yt_input_splitter *splitter, bool remote,
    struct yt_input_value *selected);

#endif
