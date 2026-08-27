#include "yt_input_model.h"

#include <string.h>

static size_t
pending(const struct yt_input_value_queue *queue)
{
	return queue->length - queue->position;
}

static void
compact(struct yt_input_value_queue *queue)
{
	if (queue->position == 0)
		return;
	if (pending(queue) != 0)
		memmove(queue->values, queue->values + queue->position,
		    pending(queue) * sizeof(queue->values[0]));
	queue->length = pending(queue);
	queue->position = 0;
}

static struct yt_input_value
pop(struct yt_input_value_queue *queue)
{
	struct yt_input_value value = {{0, 0}, 0, 0};

	if (pending(queue) == 0)
		return value;
	value = queue->values[queue->position++];
	if (queue->position == queue->length) {
		queue->position = 0;
		queue->length = 0;
	}
	return value;
}

void
yt_input_splitter_init(struct yt_input_splitter *splitter)
{
	memset(splitter, 0, sizeof(*splitter));
}

bool
yt_input_splitter_can_push(const struct yt_input_splitter *splitter,
    bool remote)
{
	const struct yt_input_value_queue *queue = remote
	    ? &splitter->remote : &splitter->local;

	return pending(queue) < YT_INPUT_PENDING;
}

bool
yt_input_splitter_push(struct yt_input_splitter *splitter, bool remote,
    const struct yt_input_value *value)
{
	struct yt_input_value_queue *queue = remote
	    ? &splitter->remote : &splitter->local;

	if (value->length == 0 || value->length > sizeof(value->bytes)
	    || !yt_input_splitter_can_push(splitter, remote))
		return false;
	if (queue->length == YT_INPUT_PENDING)
		compact(queue);
	queue->values[queue->length] = *value;
	queue->values[queue->length++].sequence = splitter->next_sequence++;
	return true;
}

struct yt_input_value
yt_input_splitter_select(struct yt_input_splitter *splitter, float mode,
    enum yt_input_phase phase)
{
	struct yt_input_value selected = pop(&splitter->local);
	bool poll_remote;

	if (phase == YT_INPUT_PHASE_WAIT && selected.length != 0)
		return selected;
	poll_remote = phase == YT_INPUT_PHASE_AB36
	    ? mode != 1.0f : mode == 0.0f;

	if (poll_remote && pending(&splitter->remote) != 0)
		selected = pop(&splitter->remote);
	return selected;
}

struct yt_input_value
yt_input_splitter_select_merged(struct yt_input_splitter *splitter)
{
	bool have_local = pending(&splitter->local) != 0;
	bool have_remote = pending(&splitter->remote) != 0;

	if (!have_local && !have_remote) {
		struct yt_input_value empty = {{0, 0}, 0, 0};

		return empty;
	}
	if (!have_remote || (have_local
	    && splitter->local.values[splitter->local.position].sequence
	    < splitter->remote.values[splitter->remote.position].sequence))
		return pop(&splitter->local);
	return pop(&splitter->remote);
}

bool
yt_b05d_process_key(const struct yt_input_value *value,
    struct yt_b05d_key_state *state)
{
	size_t position = *state->queue_position;
	size_t length = *state->queue_length;
	size_t queued = length - position;
	uint8_t key;

	if (value->length != 1)
		return true;
	key = value->bytes[0];
	if (key == 0x18) {
		state->accumulator[0] = '\0';
		state->queue[0] = '\0';
		*state->queue_position = 0;
		*state->queue_length = 0;
		if (state->pager_key_capacity < 2U)
			return false;
		state->pager_key[0] = 'Q';
		state->pager_key[1] = '\0';
		return true;
	}
	if (key == 0x12 && queued != 0)
		return true;
	if (key >= 0x7f || (key < 0x20 && key != '\r'))
		return true;
	if (queued + 1U >= state->queue_capacity)
		return false;
	if (queued != 0 && position != 0)
		memmove(state->queue, state->queue + position, queued);
	state->queue[queued] = (char)key;
	state->queue[queued + 1U] = '\0';
	*state->queue_position = 0;
	*state->queue_length = queued + 1U;
	return true;
}
