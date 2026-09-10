#ifndef YT_INPUT_H
#define YT_INPUT_H

#include "yt_common.h"
#include "yt_input_model.h"

bool yt_input_poll_legacy(struct yt_input_splitter *splitter, float mode,
    enum yt_input_phase phase, struct yt_input_value *selected);
bool yt_input_wait_legacy(struct yt_input_splitter *splitter, float mode,
    enum yt_input_phase phase, struct yt_input_value *selected);
bool yt_input_wait_legacy_until(struct yt_input_splitter *splitter,
    float mode, enum yt_input_phase phase, uint32_t seconds,
    uint16_t milliseconds, struct yt_input_value *selected, bool *timed_out);
bool yt_input_poll_merged(struct yt_input_splitter *splitter,
    struct yt_input_value *selected);
bool yt_input_poll_source(struct yt_input_splitter *splitter, bool remote,
    struct yt_input_value *selected);

#endif
