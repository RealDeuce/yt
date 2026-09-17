#ifndef INPUT_EDITOR_TEST_MODEL_H
#define INPUT_EDITOR_TEST_MODEL_H

#include "yt_command_input.h"

#include <string.h>

typedef bool (*yt_input_repeat_emit_fn)(void *context,
    const uint8_t *prefix, size_t length);
typedef bool (*yt_input_submit_fn)(void *context);
typedef bool (*yt_input_echo_fn)(void *context,
    const uint8_t *local, size_t local_length, const uint8_t *remote,
    size_t remote_length);
typedef bool (*yt_input_continue_fn)(void *context);

static inline bool
test_editor_bounded_length(const char *text, size_t capacity, size_t *length)
{
	size_t index;

	if (text == NULL || length == NULL)
		return false;
	for (index = 0U; index < capacity; ++index) {
		if (text[index] == '\0') {
			*length = index;
			return true;
		}
	}
	return false;
}

static inline bool
yt_input_repeat_current_command(char *accumulator, size_t accumulator_capacity,
    const char *saved_command, size_t saved_capacity, char *paged_text,
    size_t paged_text_capacity, float *newline_flag, uint8_t *selected_key,
    yt_input_repeat_emit_fn emit, void *context)
{
	uint8_t prefix[YT_INPUT_PENDING];
	size_t prefix_length;
	size_t saved_length;

	if (newline_flag == NULL || selected_key == NULL || emit == NULL
	    || !test_editor_bounded_length(accumulator, accumulator_capacity,
	    &prefix_length)
	    || !test_editor_bounded_length(saved_command, saved_capacity,
	    &saved_length)
	    || prefix_length > sizeof(prefix)
	    || paged_text == NULL || prefix_length >= paged_text_capacity
	    || saved_length >= accumulator_capacity)
		return false;
	if (prefix_length != 0U)
		memcpy(prefix, accumulator, prefix_length);
	memcpy(paged_text, accumulator, prefix_length + 1U);
	*newline_flag = 1.0f;
	if (!emit(context, prefix, prefix_length))
		return false;
	memmove(accumulator, saved_command, saved_length + 1U);
	*selected_key = '\r';
	return true;
}

static inline bool
yt_input_submit(float *newline_flag, yt_input_submit_fn line,
    void *context)
{
	if (newline_flag == NULL || line == NULL)
		return false;
	*newline_flag = 0.0f;
	return line(context);
}

static inline bool
yt_input_apply_backspace(uint8_t selected_key, char *accumulator,
    size_t accumulator_capacity, bool *handled,
    yt_input_echo_fn echo, void *context)
{
	static const uint8_t local_erase[] = {0x1d, ' ', 0x1d};
	static const uint8_t remote_erase[] = {'\b', ' ', '\b'};
	size_t length;

	if (handled == NULL || echo == NULL
	    || !test_editor_bounded_length(accumulator, accumulator_capacity,
	    &length))
		return false;
	*handled = false;
	if (selected_key != '\b' || length == 0U)
		return true;
	accumulator[length - 1U] = '\0';
	*handled = true;
	return echo(context, local_erase, sizeof(local_erase), remote_erase,
	    sizeof(remote_erase));
}

static inline bool
yt_input_append_printable(uint8_t selected_key, char *accumulator,
    size_t accumulator_capacity, size_t response_capacity,
    char *paged_text, size_t paged_text_capacity, float *newline_flag,
    bool *handled, yt_input_echo_fn echo, yt_input_continue_fn carrier,
    void *context)
{
	size_t length;

	if (paged_text == NULL || paged_text_capacity < 2U
	    || newline_flag == NULL || handled == NULL || echo == NULL
	    || carrier == NULL
	    || !test_editor_bounded_length(accumulator, accumulator_capacity,
	    &length))
		return false;
	*handled = false;
	if (selected_key < 0x20U || selected_key > 0x7fU
	    || length + 1U >= accumulator_capacity
	    || length + 1U >= response_capacity)
		return true;
	*handled = true;
	if (!echo(context, &selected_key, 1U, &selected_key, 1U))
		return false;
	accumulator[length] = (char)selected_key;
	accumulator[length + 1U] = '\0';
	paged_text[0] = (char)selected_key;
	paged_text[1] = '\0';
	*newline_flag = 1.0f;
	return carrier(context);
}

#endif
