#include "yt_input_model.h"

#include "qb.h"

#include <math.h>
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
	struct yt_input_value value = {{0, 0}, 0, 0, false};

	if (pending(queue) == 0)
		return value;
	value = queue->values[queue->position++];
	if (queue->position == queue->length) {
		queue->position = 0;
		queue->length = 0;
	}
	return value;
}

static float
chat_single(float value)
{
	volatile float result = value;

	return result;
}

static float
chat_single_add(float left, float right)
{
	volatile float result = left + right;

	return result;
}

static float
chat_single_subtract(float left, float right)
{
	volatile float result = left - right;

	return result;
}

static bool
chat_append(struct yt_sysop_chat_output *output,
    enum yt_sysop_chat_destination destination, bool line,
    const uint8_t *data, size_t length)
{
	struct yt_sysop_chat_event *event;

	if (output->count >= YT_SYSOP_CHAT_EVENTS
	    || length > sizeof(output->events[0].data))
		return false;
	event = &output->events[output->count++];
	memset(event, 0, sizeof(*event));
	event->destination = destination;
	event->line = line;
	if (length != 0)
		memcpy(event->data, data, length);
	event->length = length;
	return true;
}

static bool
chat_key_accepted(const uint8_t *key, size_t length)
{
	if (length == 0)
		return false;
	if (key[0] < 0x7fU)
		return true;
	return key[0] == 0x7fU && length == 1U;
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
	queue->values[queue->length].sequence = splitter->next_sequence++;
	queue->values[queue->length++].remote = remote;
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
		struct yt_input_value empty = {{0, 0}, 0, 0, false};

		return empty;
	}
	if (!have_remote || (have_local
	    && splitter->local.values[splitter->local.position].sequence
	    < splitter->remote.values[splitter->remote.position].sequence))
		return pop(&splitter->local);
	return pop(&splitter->remote);
}

struct yt_input_value
yt_input_splitter_select_source(struct yt_input_splitter *splitter,
    bool remote)
{
	return pop(remote ? &splitter->remote : &splitter->local);
}

bool
yt_input_ab36_remote_replace(float mode, const struct yt_input_value *remote,
    struct yt_input_value *selected)
{
	if (remote == NULL || selected == NULL || remote->length > 2U)
		return false;
	if (mode != 1.0f && remote->length != 0)
		*selected = *remote;
	return true;
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

bool
yt_input_expand_repeat(char *text, size_t text_capacity,
    char *saved_command, size_t saved_capacity,
    struct yt_repeat_transform *result)
{
	char base[YT_INPUT_PENDING];
	struct qb_val_result parsed;
	size_t text_length;
	size_t prefix;
	size_t base_length;
	size_t used = 0;
	float count;
	int copies;
	int index;

	if (text == NULL || saved_command == NULL || result == NULL
	    || text_capacity == 0 || saved_capacity == 0)
		return false;
	result->emit_notice = false;
	result->count = 0.0f;
	text_length = strlen(text);
	for (prefix = 0; prefix + 1U < text_length; ++prefix) {
		uint8_t next = (uint8_t)text[prefix + 1U];

		if (next > (uint8_t)'@')
			next &= 0xdfU;
		if (text[prefix] == '/' && next == (uint8_t)'R')
			break;
	}
	if (prefix + 1U >= text_length)
		return true;
	if (prefix + 2U >= text_capacity || prefix + 1U >= sizeof(base))
		return false;
	parsed = qb_val(text + prefix + 2U);
	count = (float)qb_int(parsed.valid ? parsed.value : 0.0);
	if (count > 10.0f)
		count = 20.0f;
	result->count = count;
	text[prefix] = ';';
	text[prefix + 1U] = '\0';
	if (count <= 0.0f)
		return true;
	base_length = prefix + 1U;
	memcpy(base, text, base_length);
	copies = (int)count;
	for (index = 0; index < copies; ++index) {
		if (used > 500U)
			break;
		if (used + base_length >= text_capacity)
			return false;
		memcpy(text + used, base, base_length);
		used += base_length;
	}
	if (used == 0 || used >= saved_capacity)
		return false;
	text[--used] = '\0';
	memcpy(saved_command, text, used + 1U);
	result->emit_notice = true;
	return true;
}

bool
yt_input_split_semicolon(char *text, char *queue, size_t queue_capacity,
    size_t *queue_position, size_t *queue_length)
{
	char pending_bytes[YT_INPUT_PENDING];
	char *semicolon;
	size_t remainder_length;
	size_t pending_length;
	size_t index;

	if (text == NULL || queue == NULL || queue_position == NULL
	    || queue_length == NULL || *queue_length < *queue_position)
		return false;
	semicolon = strchr(text, ';');
	if (semicolon == NULL)
		return true;
	remainder_length = strlen(semicolon + 1U);
	pending_length = *queue_length - *queue_position;
	if (pending_length > sizeof(pending_bytes)
	    || remainder_length + pending_length + 1U >= queue_capacity)
		return false;
	if (pending_length != 0)
		memcpy(pending_bytes, queue + *queue_position, pending_length);
	for (index = 0; index < remainder_length; ++index) {
		char byte = semicolon[index + 1U];

		queue[index] = byte == ';' ? '\r' : byte;
	}
	if (pending_length != 0)
		memcpy(queue + remainder_length, pending_bytes, pending_length);
	queue[remainder_length + pending_length] = '\r';
	queue[remainder_length + pending_length + 1U] = '\0';
	*queue_position = 0;
	*queue_length = remainder_length + pending_length + 1U;
	*semicolon = '\0';
	return true;
}

bool
yt_input_yes_no_candidate(const char *command_accumulator,
    char *output_source, size_t output_source_capacity,
    enum yt_yes_no_answer *answer)
{
	size_t length;
	char first;

	if (command_accumulator == NULL || output_source == NULL
	    || output_source_capacity < 2U || answer == NULL)
		return false;
	length = strlen(command_accumulator);
	if (length >= output_source_capacity)
		return false;
	memcpy(output_source, command_accumulator, length + 1U);
	qb_compat_upper(output_source);
	first = output_source[0];
	if (first != '\0')
		output_source[1] = '\0';
	if (first == '\0')
		*answer = YT_YES_NO_EMPTY;
	else if (first == 'Y')
		*answer = YT_YES_NO_YES;
	else if (first == 'N')
		*answer = YT_YES_NO_NO;
	else
		*answer = YT_YES_NO_INVALID;
	return true;
}

void
yt_input_numeric_response(char *text)
{
	qb_compat_upper(text);
	if (strchr(text, 'E') != NULL)
		text[0] = '\0';
}

static float
wait_single(float value)
{
	volatile float result = value;

	return result;
}

static float
wait_single_add(float left, float right)
{
	volatile float result = left + right;

	return result;
}

bool
yt_timed_wait_begin(struct yt_timed_wait_state *state, float duration,
    float initial_timer)
{
	float rounded_duration;
	float rounded_timer;

	if (state == NULL
	    || state->serial_scratch_length > sizeof(state->serial_scratch))
		return false;
	rounded_duration = wait_single(duration);
	rounded_timer = wait_single(initial_timer);
	state->duration_cell = wait_single_add(rounded_timer,
	    rounded_duration);
	state->timer_reads = 1;
	state->local_reads = 0;
	state->loc_reads = 0;
	state->serial_reads = 0;
	return true;
}

enum yt_timed_wait_reason
yt_timed_wait_timer(struct yt_timed_wait_state *state, float current_timer)
{
	float current;

	if (state == NULL)
		return YT_TIMED_WAIT_ERROR;
	current = wait_single(current_timer);
	++state->timer_reads;
	return current >= state->duration_cell
	    ? YT_TIMED_WAIT_TIMER : YT_TIMED_WAIT_CONTINUE;
}

enum yt_timed_wait_reason
yt_timed_wait_input(struct yt_timed_wait_state *state, float mode,
    const struct yt_input_value *selected)
{
	if (state == NULL || selected == NULL || selected->length > 2U)
		return YT_TIMED_WAIT_ERROR;
	++state->local_reads;
	if (!selected->remote && selected->length != 0)
		return YT_TIMED_WAIT_LOCAL;
	if (mode != 0.0f)
		return YT_TIMED_WAIT_CONTINUE;
	++state->loc_reads;
	if (!selected->remote)
		return selected->length == 0
		    ? YT_TIMED_WAIT_CONTINUE : YT_TIMED_WAIT_ERROR;
	if (selected->length != 1U)
		return YT_TIMED_WAIT_ERROR;
	state->serial_scratch[0] = selected->bytes[0];
	state->serial_scratch_length = 1;
	++state->serial_reads;
	return YT_TIMED_WAIT_SERIAL;
}

bool
yt_input_drain_begin(struct yt_input_drain_state *state,
    const struct yt_input_value *initial_residue)
{
	if (state == NULL || initial_residue == NULL
	    || initial_residue->length > sizeof(state->residue))
		return false;
	memset(state, 0, sizeof(*state));
	if (initial_residue->length != 0)
		memcpy(state->residue, initial_residue->bytes,
		    initial_residue->length);
	state->residue_length = initial_residue->length;
	return true;
}

enum yt_input_drain_reason
yt_input_drain_local(struct yt_input_drain_state *state,
    const struct yt_input_value *selected)
{
	if (state == NULL || selected == NULL || selected->length > 2U
	    || (selected->remote && selected->length != 0))
		return YT_INPUT_DRAIN_ERROR;
	++state->local_reads;
	if (state->expect_paired_local) {
		if (selected->length != 0)
			memcpy(state->residue, selected->bytes,
			    selected->length);
		state->residue_length = selected->length;
		state->expect_paired_local = false;
		return YT_INPUT_DRAIN_CONTINUE;
	}
	if (selected->length == 0)
		return YT_INPUT_DRAIN_LOCAL_COMPLETE;
	state->expect_paired_local = true;
	return YT_INPUT_DRAIN_CONTINUE;
}

enum yt_input_drain_reason
yt_input_drain_serial(struct yt_input_drain_state *state, float mode,
    const struct yt_input_value *selected)
{
	if (state == NULL || selected == NULL || selected->length > 2U
	    || state->expect_paired_local)
		return YT_INPUT_DRAIN_ERROR;
	if (mode != 0.0f)
		return YT_INPUT_DRAIN_COMPLETE;
	++state->loc_reads;
	if (selected->length == 0)
		return YT_INPUT_DRAIN_COMPLETE;
	if (!selected->remote || selected->length != 1U)
		return YT_INPUT_DRAIN_ERROR;
	state->residue[0] = selected->bytes[0];
	state->residue_length = 1;
	++state->serial_reads;
	return YT_INPUT_DRAIN_CONTINUE;
}

bool
yt_sysop_chat_begin(struct yt_sysop_chat_state *state, float mode,
    float snoop, float deadline, float entry_timer,
    float inactivity_deadline, const uint8_t *command_accumulator,
    size_t command_accumulator_length, const uint8_t *queue,
    size_t queue_length)
{
	float rounded_deadline;
	float rounded_timer;

	if (state == NULL || command_accumulator_length > YT_INPUT_PENDING
	    || queue_length > YT_INPUT_PENDING
	    || (command_accumulator == NULL
	    && command_accumulator_length != 0U)
	    || (queue == NULL && queue_length != 0U))
		return false;
	memset(state, 0, sizeof(*state));
	state->mode = mode;
	state->snoop = snoop;
	state->foreground = 2.0f;
	state->bold = 1.0f;
	rounded_deadline = chat_single(deadline);
	rounded_timer = chat_single(entry_timer);
	state->deadline = rounded_deadline;
	state->inactivity_deadline = chat_single(inactivity_deadline);
	state->saved_remaining = chat_single_subtract(rounded_deadline,
	    chat_single(floorf(rounded_timer)));
	state->newline_flag = 0.0f;
	if (command_accumulator_length != 0U)
		memcpy(state->command_accumulator, command_accumulator,
		    command_accumulator_length);
	state->command_accumulator_length = command_accumulator_length;
	if (queue_length != 0U)
		memcpy(state->queue, queue, queue_length);
	state->queue_length = queue_length;
	state->timer_reads = 1U;
	return true;
}

enum yt_sysop_chat_step_result
yt_sysop_chat_step(struct yt_sysop_chat_state *state,
    const struct yt_sysop_chat_poll *poll,
    struct yt_sysop_chat_output *output)
{
	static const uint8_t local_erase[] = {0x1d, ' ', 0x1d};
	static const uint8_t remote_erase[] = {'\b', ' ', '\b'};
	const uint8_t *key;
	size_t key_length;

	if (output != NULL)
		memset(output, 0, sizeof(*output));
	if (state == NULL || poll == NULL || output == NULL
	    || state->key_length > sizeof(state->key)
	    || state->command_accumulator_length > YT_INPUT_PENDING
	    || state->queue_length > YT_INPUT_PENDING
	    || poll->local.length > sizeof(poll->local.bytes)
	    || poll->remote.length > 1U
	    || poll->position_after_output < 0)
		return YT_SYSOP_CHAT_INVALID;
	if (state->terminated)
		return YT_SYSOP_CHAT_CARRIER_END;
	if (state->exited)
		return YT_SYSOP_CHAT_EXIT;
	if (state->key_length == 1U && state->key[0] == 0x1bU) {
		state->exited = true;
		return YT_SYSOP_CHAT_EXIT;
	}

	state->key_length = poll->local.length;
	if (state->key_length != 0U) {
		memcpy(state->key, poll->local.bytes, state->key_length);
		state->foreground = 6.0f;
	}
	if (state->mode == 0.0f && poll->remote.length != 0U) {
		state->key[0] = poll->remote.bytes[0];
		state->key_length = 1U;
		state->foreground = 3.0f;
	}
	++state->polls;
	++state->carrier_checks;
	if (state->mode == 0.0f && poll->modem_status < 0x80U) {
		state->terminated = true;
		return YT_SYSOP_CHAT_CARRIER_END;
	}

	key = state->key;
	key_length = state->key_length;
	if (key_length == 1U && key[0] == '\r') {
		state->key_length = 0U;
		key_length = 0U;
		if (state->snoop != 0.0f
		    && !chat_append(output, YT_SYSOP_CHAT_LOCAL_GATED,
		    true, NULL, 0U))
			return YT_SYSOP_CHAT_INVALID;
		if (state->mode != 1.0f
		    && !chat_append(output, YT_SYSOP_CHAT_SERIAL,
		    true, NULL, 0U))
			return YT_SYSOP_CHAT_INVALID;
	}

	if (!chat_key_accepted(key, key_length))
		return YT_SYSOP_CHAT_CONTINUE;
	if (key[0] == '\b' && poll->position_after_output > 0) {
		if (!chat_append(output, YT_SYSOP_CHAT_LOCAL_RAW, false,
		    local_erase, sizeof(local_erase)))
			return YT_SYSOP_CHAT_INVALID;
		if (state->mode == 0.0f
		    && !chat_append(output, YT_SYSOP_CHAT_SERIAL, false,
		    remote_erase, sizeof(remote_erase)))
			return YT_SYSOP_CHAT_INVALID;
		return YT_SYSOP_CHAT_CONTINUE;
	}

	state->bold = 1.0f;
	if (state->snoop != 0.0f
	    && !chat_append(output, YT_SYSOP_CHAT_LOCAL_GATED, false,
	    key, key_length))
		return YT_SYSOP_CHAT_INVALID;
	if (state->mode != 1.0f
	    && !chat_append(output, YT_SYSOP_CHAT_SERIAL, false,
	    key, key_length))
		return YT_SYSOP_CHAT_INVALID;
	if ((poll->position_after_output > 70
	    && key_length == 1U && key[0] == ' ')
	    || poll->position_after_output > 79) {
		if (state->snoop != 0.0f
		    && !chat_append(output, YT_SYSOP_CHAT_LOCAL_GATED,
		    true, NULL, 0U))
			return YT_SYSOP_CHAT_INVALID;
		if (state->mode != 1.0f
		    && !chat_append(output, YT_SYSOP_CHAT_SERIAL,
		    true, NULL, 0U))
			return YT_SYSOP_CHAT_INVALID;
	}
	return YT_SYSOP_CHAT_CONTINUE;
}

bool
yt_sysop_chat_finish(struct yt_sysop_chat_state *state,
    float deadline_timer, float inactivity_timer)
{
	float deadline_sample;
	float inactivity_sample;

	if (state == NULL || state->command_accumulator_length > YT_INPUT_PENDING
	    || state->queue_length > YT_INPUT_PENDING)
		return false;
	if (!state->exited || state->terminated)
		return true;
	deadline_sample = chat_single(floorf(chat_single(deadline_timer)));
	inactivity_sample = chat_single(floorf(chat_single(inactivity_timer)));
	state->deadline = chat_single_add(deadline_sample,
	    chat_single(state->saved_remaining));
	state->inactivity_deadline = chat_single_add(inactivity_sample, 240.0f);
	memset(state->command_accumulator, 0,
	    sizeof(state->command_accumulator));
	state->command_accumulator_length = 0U;
	memset(state->queue, 0, sizeof(state->queue));
	state->queue[0] = '\r';
	state->queue_length = 1U;
	state->timer_reads += 2U;
	return true;
}

bool
yt_sysop_f5_compose(bool same_f5_make, struct yt_sysop_f5_result *result)
{
	static const struct yt_sysop_f5_event events[YT_SYSOP_F5_PHASES] = {
		{YT_SYSOP_F5_CHECKPOINT, 0xBA00U},
		{YT_SYSOP_F5_BASIC_END, 0xBA01U},
		{YT_SYSOP_F5_REGISTERED_CLEANUP, 0x3B2FU},
		{YT_SYSOP_F5_CALLBACK_CLEANUP, 0x3B29U},
		{YT_SYSOP_F5_RUNTIME_CLEANUP, 0x1810U},
		{YT_SYSOP_F5_COMMON_CLEANUP, 0x1C21U},
		{YT_SYSOP_F5_DOS_EXIT, 0x064EU},
	};

	if (result == NULL)
		return false;
	memset(result, 0, sizeof(*result));
	memcpy(result->events, events, sizeof(events));
	result->event_count = YT_ARRAY_LEN(events);
	result->same_f5_pending = same_f5_make;
	result->local_end_cleanup = true;
	result->terminated = true;
	return true;
}
