#include "yt_command_input.h"

#include "qb.h"

#include <string.h>


static bool
bounded_string_length(const char *text, size_t capacity, size_t *length)
{
	size_t index;

	if (text == NULL || capacity == 0U || length == NULL)
		return false;
	for (index = 0U; index < capacity; ++index) {
		if (text[index] == '\0') {
			*length = index;
			return true;
		}
	}
	return false;
}

enum yt_radio_body_key_action
yt_input_radio_body_key(uint8_t key, size_t current_length)
{
	if (key == '\r')
		return YT_RADIO_BODY_KEY_COMMIT;
	if (key == '\b' || key == 0x7fU)
		return current_length != 0U
		    ? YT_RADIO_BODY_KEY_BACKSPACE : YT_RADIO_BODY_KEY_IGNORE;
	if (key >= 0x20U && key < 0x7fU)
		return YT_RADIO_BODY_KEY_PRINTABLE;
	return YT_RADIO_BODY_KEY_IGNORE;
}

bool
yt_input_queue_pop(char *queue, size_t capacity, size_t *position,
    size_t *length, struct yt_input_value *selected)
{
	if (queue == NULL || capacity == 0U || position == NULL
	    || length == NULL || selected == NULL || *position > *length
	    || *length >= capacity)
		return false;
	memset(selected, 0, sizeof(*selected));
	if (*position == *length)
		return true;
	selected->bytes[0] = (uint8_t)queue[(*position)++];
	selected->length = 1U;
	if (*position == *length) {
		*position = 0U;
		*length = 0U;
		queue[0] = '\0';
	}
	return true;
}

bool
yt_input_queue_clear(char *queue, size_t capacity, size_t *position,
    size_t *length)
{
	if (queue == NULL || capacity == 0U || position == NULL
	    || length == NULL || *position > *length || *length >= capacity)
		return false;
	queue[0] = '\0';
	*position = 0U;
	*length = 0U;
	return true;
}

bool
yt_input_queue_prepend_program(char *queue, size_t capacity,
    size_t *position, size_t *length, const char *program,
    size_t program_length)
{
	size_t pending;

	if (queue == NULL || capacity == 0U || position == NULL
	    || length == NULL || (program == NULL && program_length != 0U)
	    || *position > *length || *length >= capacity)
		return false;
	pending = *length - *position;
	if (program_length >= capacity
	    || program_length + 1U >= capacity - pending)
		return false;
	memmove(queue + program_length + 1U, queue + *position, pending);
	if (program_length != 0U)
		memcpy(queue, program, program_length);
	queue[program_length] = '\r';
	queue[program_length + 1U + pending] = '\0';
	*position = 0U;
	*length = program_length + 1U + pending;
	return true;
}

bool
yt_input_repeat_requested(bool queued,
    const struct yt_input_value *selected)
{
	return !queued && selected != NULL && selected->length == 1U
	    && selected->bytes[0] == 0x12U;
}

bool
yt_input_submit_requested(uint8_t selected_key)
{
	return selected_key == '\r';
}

bool
yt_input_save_command(char *text, size_t text_capacity,
    char *queue, size_t queue_capacity, size_t *queue_position,
    size_t *queue_length, char *saved_command, size_t saved_capacity,
    char *output_source, size_t output_capacity, bool *notice_ready)
{
	static const char notice[] =
	    "Command Saved -+- Ctrl-R to Re-use -+- Ctrl-X to cancel.";
	size_t length;
	size_t stripped_length;

	if (text == NULL || text_capacity == 0U || queue == NULL
	    || queue_capacity == 0U || queue_position == NULL
	    || queue_length == NULL || *queue_position > *queue_length
	    || *queue_length >= queue_capacity || saved_command == NULL
	    || saved_capacity == 0U || output_source == NULL
	    || output_capacity == 0U || notice_ready == NULL
	    || !bounded_string_length(text, text_capacity, &length))
		return false;
	*notice_ready = false;
	if (length == 0U || text[length - 1U] != '/')
		return true;
	stripped_length = length - 1U;
	text[stripped_length] = '\0';
	queue[0] = '\0';
	*queue_position = 0U;
	*queue_length = 0U;
	if (stripped_length >= saved_capacity)
		return false;
	memcpy(saved_command, text, stripped_length + 1U);
	if (sizeof(notice) > output_capacity)
		return false;
	memcpy(output_source, notice, sizeof(notice));
	*notice_ready = true;
	return true;
}

void
yt_input_compat_upper_n(uint8_t *text, size_t length)
{
	size_t index;

	if (text == NULL)
		return;
	for (index = 0U; index < length; ++index)
		if (text[index] > (uint8_t)'@')
			text[index] &= 0xdfU;
}

bool
yt_input_expand_repeat_with_notice(char *text, size_t text_capacity,
    char *saved_command, size_t saved_capacity,
    char *output_source, size_t output_capacity,
    struct yt_repeat_transform *result)
{
	uint8_t upper[YT_INPUT_PENDING];
	uint8_t mbf64[8];
	uint8_t mbf32[4];
	struct qb_val_result parsed;
	static const char notice_prefix[] = "Command Repeated";
	static const char notice_suffix[] =
	    " times -+- Ctrl-R to Re-use -+- Ctrl-X to cancel.";
	char count_text[64];
	double integer;
	size_t repeat_position = 0U;
	size_t prefix_length;
	size_t suffix_offset;
	size_t used = 0U;
	size_t text_length;
	size_t expanded_length;
	size_t count_length;
	size_t notice_length;
	float count;
	int copies;
	int index;

	if (text == NULL || saved_command == NULL || output_source == NULL
	    || result == NULL || text_capacity == 0 || saved_capacity == 0
	    || output_capacity == 0)
		return false;
	result->emit_notice = false;
	result->bold_committed = false;
	result->count = 0.0f;
	result->failure = YT_REPEAT_FAILURE_NONE;
	result->fault_site = YT_BASIC_FAULT_SITE_COUNT;
	result->fault_valid = false;
	if (!bounded_string_length(text, text_capacity, &text_length))
		return false;
	if (text_length >= sizeof(upper))
		return false;
	memcpy(upper, text, text_length + 1U);
	yt_input_compat_upper_n(upper, text_length);
	for (prefix_length = 0U; prefix_length + 1U < text_length;
	    ++prefix_length) {
		if (upper[prefix_length] == (uint8_t)'/'
		    && upper[prefix_length + 1U] == (uint8_t)'R') {
			repeat_position = prefix_length + 1U;
			break;
		}
	}
	if (repeat_position == 0U)
		return true;
	prefix_length = repeat_position - 1U;
	suffix_offset = repeat_position + 1U;
	if (prefix_length + 2U > text_capacity)
		return false;
	text[prefix_length] = ';';
	text[prefix_length + 1U] = '\0';
	parsed = qb_val((const char *)upper + suffix_offset);
	if (parsed.overflow) {
		result->failure = YT_REPEAT_FAILURE_VAL_OVERFLOW;
		result->fault_site = YT_BASIC_FAULT_REPEAT_VAL_OVERFLOW;
		result->fault_valid = true;
		return false;
	}
	integer = qb_int(parsed.valid ? parsed.value : 0.0);
	if (qb_mbf64_encode(integer, mbf64) != QB_MBF_OK)
		return false;
	count = (float)integer;
	if (qb_mbf32_encode(count, mbf32) == QB_MBF_OVERFLOW) {
		result->failure = YT_REPEAT_FAILURE_SINGLE_OVERFLOW;
		result->fault_site = YT_BASIC_FAULT_REPEAT_SINGLE_OVERFLOW;
		result->fault_valid = true;
		return false;
	}
	if (count > 10.0f)
		count = 20.0f;
	result->count = count;
	if (count <= 0.0f)
		return true;
	copies = (int)count;
	text_length = prefix_length + 1U;
	for (index = 0; index < copies && used <= 500U; ++index) {
		if (used + text_length >= sizeof(upper))
			return false;
		memcpy(upper + used, text, text_length);
		used += text_length;
	}
	if (used == 0U)
		return false;
	expanded_length = used - 1U;
	if (expanded_length >= text_capacity || expanded_length >= saved_capacity)
		return false;
	memcpy(text, upper, expanded_length);
	text[expanded_length] = '\0';
	memcpy(saved_command, text, expanded_length + 1U);
	result->bold_committed = true;
	if (qb_str_double(count_text, sizeof(count_text), (double)count) < 0)
		return false;
	count_length = strlen(count_text);
	notice_length = sizeof(notice_prefix) - 1U + count_length
	    + sizeof(notice_suffix) - 1U;
	if (notice_length >= output_capacity)
		return false;
	memcpy(output_source, notice_prefix, sizeof(notice_prefix) - 1U);
	memcpy(output_source + sizeof(notice_prefix) - 1U,
	    count_text, count_length);
	memcpy(output_source + sizeof(notice_prefix) - 1U + count_length,
	    notice_suffix, sizeof(notice_suffix));
	result->emit_notice = true;
	return true;
}

bool
yt_input_split_semicolon(char *text, char *queue, size_t queue_capacity,
    size_t *queue_position, size_t *queue_length)
{
	char old_queue[YT_INPUT_PENDING];
	char *semicolon;
	char *replacement;
	size_t text_length;
	size_t tail_length;
	size_t old_length;
	size_t combined_length;
	size_t prefix_length;

	if (text == NULL || queue == NULL
	    || queue_capacity == 0U || queue_position == NULL
	    || queue_length == NULL
	    || *queue_length < *queue_position
	    || *queue_length >= queue_capacity)
		return false;
	text_length = strlen(text);
	semicolon = memchr(text, ';', text_length);
	if (semicolon == NULL)
		return true;
	prefix_length = (size_t)(semicolon - text);
	tail_length = text_length - prefix_length - 1U;
	old_length = *queue_length - *queue_position;
	if (tail_length >= sizeof(old_queue))
		return false;
	combined_length = tail_length + old_length;
	if (old_length > sizeof(old_queue) || combined_length >= queue_capacity)
		return false;
	if (old_length != 0U)
		memcpy(old_queue, queue + *queue_position, old_length);
	if (tail_length != 0U)
		memcpy(queue, semicolon + 1U, tail_length);
	if (old_length != 0U)
		memcpy(queue + tail_length, old_queue, old_length);
	queue[combined_length] = '\0';
	*queue_position = 0U;
	*queue_length = combined_length;
	text[prefix_length] = '\0';
	for (;;) {
		replacement = memchr(queue, ';', *queue_length);
		if (replacement == NULL)
			break;
		*replacement = '\r';
	}
	if (*queue_length + 1U >= queue_capacity)
		return false;
	queue[*queue_length] = '\r';
	++*queue_length;
	queue[*queue_length] = '\0';
	return true;
}

bool
yt_input_confirmation(const char *command_accumulator,
    char *output_source, size_t output_source_capacity,
    uint8_t *prompt, size_t prompt_capacity, size_t *prompt_length,
    char *queue, size_t queue_capacity, size_t *queue_position,
    size_t *queue_length, float *bold, enum yt_yes_no_answer *answer,
    enum yt_confirmation_outcome *outcome)
{
	size_t length;
	char first;

	if (command_accumulator == NULL || output_source == NULL
	    || output_source_capacity < 2U || prompt == NULL
	    || prompt_capacity == 0U || prompt_length == NULL
	    || *prompt_length > prompt_capacity
	    || queue == NULL || queue_capacity == 0U
	    || queue_position == NULL || queue_length == NULL || bold == NULL
	    || answer == NULL || outcome == NULL
	    || *queue_position > *queue_length || *queue_length >= queue_capacity)
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
	if (*answer == YT_YES_NO_INVALID) {
		*bold = 1.0f;
		if (!yt_input_queue_clear(queue, queue_capacity,
		    queue_position, queue_length))
			return false;
		*outcome = YT_CONFIRMATION_RETRY;
		return true;
	}
	*prompt_length = 0U;
	prompt[0] = 0U;
	*outcome = YT_CONFIRMATION_RETURNED;
	return true;
}

bool
yt_input_yes_no_candidate(const char *command_accumulator,
    char *output_source, size_t output_source_capacity,
    enum yt_yes_no_answer *answer)
{
	enum yt_confirmation_outcome outcome;
	uint8_t prompt[1] = {0U};
	size_t prompt_length = 0U;
	char queue[1] = "";
	size_t queue_position = 0U;
	size_t queue_length = 0U;
	float bold = 0.0f;

	if (answer == NULL || !yt_input_confirmation(command_accumulator,
	    output_source, output_source_capacity, prompt, sizeof(prompt),
	    &prompt_length, queue, sizeof(queue), &queue_position, &queue_length,
	    &bold, answer, &outcome))
		return false;
	return true;
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
yt_input_drain_serial(struct yt_input_drain_state *state, bool local_mode,
    const struct yt_input_value *selected)
{
	if (state == NULL || selected == NULL || selected->length > 2U
	    || state->expect_paired_local)
		return YT_INPUT_DRAIN_ERROR;
	if (local_mode)
		return YT_INPUT_DRAIN_COMPLETE;
	if (selected->length == 0)
		return YT_INPUT_DRAIN_COMPLETE;
	if (!selected->remote || selected->length != 1U)
		return YT_INPUT_DRAIN_ERROR;
	state->residue[0] = selected->bytes[0];
	state->residue_length = 1;
	return YT_INPUT_DRAIN_CONTINUE;
}
