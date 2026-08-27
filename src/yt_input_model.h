#ifndef YT_INPUT_MODEL_H
#define YT_INPUT_MODEL_H

#include "yt_common.h"

#define YT_INPUT_PENDING 4096U

enum yt_input_phase {
	YT_INPUT_PHASE_B05D,
	YT_INPUT_PHASE_AB36,
	YT_INPUT_PHASE_WAIT,
};

struct yt_input_value {
	uint8_t bytes[2];
	size_t length;
	uint64_t sequence;
};

struct yt_input_value_queue {
	struct yt_input_value values[YT_INPUT_PENDING];
	size_t position;
	size_t length;
};

struct yt_input_splitter {
	struct yt_input_value_queue local;
	struct yt_input_value_queue remote;
	uint64_t next_sequence;
};

struct yt_b05d_key_state {
	char *accumulator;
	size_t accumulator_capacity;
	char *queue;
	size_t queue_capacity;
	size_t *queue_position;
	size_t *queue_length;
	char *pager_key;
	size_t pager_key_capacity;
};

void yt_input_splitter_init(struct yt_input_splitter *splitter);
bool yt_input_splitter_can_push(const struct yt_input_splitter *splitter,
    bool remote);
bool yt_input_splitter_push(struct yt_input_splitter *splitter, bool remote,
    const struct yt_input_value *value);
struct yt_input_value yt_input_splitter_select(
    struct yt_input_splitter *splitter, float mode,
    enum yt_input_phase phase);
struct yt_input_value yt_input_splitter_select_merged(
    struct yt_input_splitter *splitter);
bool yt_b05d_process_key(const struct yt_input_value *value,
    struct yt_b05d_key_state *state);

#endif
