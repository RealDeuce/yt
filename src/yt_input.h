#ifndef YT_INPUT_H
#define YT_INPUT_H

#include "yt_common.h"
#include "yt_input_model.h"

struct yt_input {
	struct yt_input_value pending;
	bool pending_valid;
};

void yt_input_init(struct yt_input *input);
bool yt_input_poll(struct yt_input *input, struct yt_input_value *selected);
bool yt_input_wait(struct yt_input *input, struct yt_input_value *selected);
bool yt_input_wait_until(struct yt_input *input, uint32_t seconds,
    uint16_t milliseconds, struct yt_input_value *selected, bool *timed_out);
bool yt_input_pause(struct yt_input *input, double seconds);
bool yt_input_poll_source(struct yt_input *input, bool remote,
    struct yt_input_value *selected);
bool yt_input_source_ready(struct yt_input *input, bool remote, bool *ready);

#endif
