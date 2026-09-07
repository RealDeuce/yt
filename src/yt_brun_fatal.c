#include "yt_brun_fatal.h"

#include <stdio.h>
#include <string.h>

static bool
append_bytes(uint8_t *output, size_t capacity, size_t *length,
    const void *data, size_t count)
{
	if (count > capacity - *length)
		return false;
	if (count != 0U)
		memcpy(output + *length, data, count);
	*length += count;
	return true;
}

static bool
append_literal(uint8_t *output, size_t capacity, size_t *length,
    const char *literal)
{
	return append_bytes(output, capacity, length, literal, strlen(literal));
}

static bool
append_number(uint8_t *output, size_t capacity, size_t *length,
    int32_t value)
{
	char text[24];
	int written = snprintf(text, sizeof(text), "%ld", (long)value);

	return written >= 0 && (size_t)written < sizeof(text)
	    && append_bytes(output, capacity, length, text, (size_t)written);
}

static bool
append_hex_word(uint8_t *output, size_t capacity, size_t *length,
    uint16_t value)
{
	char text[5];
	int written = snprintf(text, sizeof(text), "%04X", (unsigned)value);

	return written == 4
	    && append_bytes(output, capacity, length, text, 4U);
}

bool
yt_brun_internal_fatal_run(enum yt_brun_internal_fatal_entry entry,
    const char module[8], bool has_source_line, int32_t source_line,
    uint16_t module_segment, uint16_t saved_ip, bool redirected_stdin,
    bool function_bar, bool cursor_shape_known,
    uint16_t process_entry_cursor_shape,
    const struct yt_brun_internal_fatal_ops *ops, void *context,
    struct yt_brun_internal_fatal_state *state)
{
	static const uint8_t prompt[] =
	    "\rHit any key to return to system";
	size_t module_length = 8U;

	if (module == NULL || ops == NULL || ops->local == NULL
	    || ops->close_all == NULL || ops->restore_terminal == NULL
	    || ops->end == NULL || state == NULL
	    || (entry != YT_BRUN_INTERNAL_FATAL_GC
	    && entry != YT_BRUN_INTERNAL_FATAL_OWNER)
	    || (!redirected_stdin && ops->drain == NULL)
	    || (function_bar && ops->clear_function_bar == NULL))
		return false;
	while (module_length != 0U && module[module_length - 1U] == ' ')
		--module_length;
	if (module_length == 0U)
		return false;

	memset(state, 0, sizeof(*state));
	state->entry = entry;
	memcpy(state->module, module, 8U);
	state->module[8] = '\0';
	state->module_segment = module_segment;
	state->saved_ip = saved_ip;
	state->source_line = source_line;
	state->has_source_line = has_source_line;
	state->redirected_stdin = redirected_stdin;
	state->function_bar_before = function_bar;
	state->function_bar_after = function_bar;
	state->cursor_shape_known = cursor_shape_known;
	state->process_entry_cursor_shape = process_entry_cursor_shape;
	if (!append_literal(state->diagnostic, sizeof(state->diagnostic),
	    &state->diagnostic_length, "\rString Space Corrupt")
	    || (entry == YT_BRUN_INTERNAL_FATAL_GC
	    && !append_literal(state->diagnostic, sizeof(state->diagnostic),
	    &state->diagnostic_length, " during G.C."))
	    || !append_literal(state->diagnostic, sizeof(state->diagnostic),
	    &state->diagnostic_length, " in ")
	    || (has_source_line
	    && (!append_literal(state->diagnostic, sizeof(state->diagnostic),
	    &state->diagnostic_length, "line ")
	    || !append_number(state->diagnostic, sizeof(state->diagnostic),
	    &state->diagnostic_length, source_line)
	    || !append_literal(state->diagnostic, sizeof(state->diagnostic),
	    &state->diagnostic_length, " of ")))
	    || !append_literal(state->diagnostic, sizeof(state->diagnostic),
	    &state->diagnostic_length, "module ")
	    || !append_bytes(state->diagnostic, sizeof(state->diagnostic),
	    &state->diagnostic_length, module, 8U)
	    || !append_literal(state->diagnostic, sizeof(state->diagnostic),
	    &state->diagnostic_length, " at address ")
	    || !append_hex_word(state->diagnostic, sizeof(state->diagnostic),
	    &state->diagnostic_length, module_segment)
	    || !append_literal(state->diagnostic, sizeof(state->diagnostic),
	    &state->diagnostic_length, ":")
	    || !append_hex_word(state->diagnostic, sizeof(state->diagnostic),
	    &state->diagnostic_length, saved_ip)
	    || !append_literal(state->diagnostic, sizeof(state->diagnostic),
	    &state->diagnostic_length, "\r")
	    || !append_bytes(state->prompt, sizeof(state->prompt),
	    &state->prompt_length, prompt, sizeof(prompt) - 1U)
	    || !append_bytes(state->local_bytes, sizeof(state->local_bytes),
	    &state->local_length, state->diagnostic,
	    state->diagnostic_length)
	    || !append_bytes(state->local_bytes, sizeof(state->local_bytes),
	    &state->local_length, state->prompt, state->prompt_length)
	    || (redirected_stdin
	    && !append_literal(state->local_bytes, sizeof(state->local_bytes),
	    &state->local_length, "\r")))
		return false;

	ops->local(context, state->diagnostic, state->diagnostic_length);
	ops->close_all(context);
	state->close_all_completed = true;
	ops->local(context, state->prompt, state->prompt_length);
	if (redirected_stdin) {
		static const uint8_t carriage_return = '\r';

		ops->local(context, &carriage_return, 1U);
	}
	else {
		state->drained_word_count = ops->drain(context,
		    state->drained_words, YT_BRUN_FATAL_DRAIN_WORDS);
		if (state->drained_word_count > YT_BRUN_FATAL_DRAIN_WORDS)
			return false;
		state->input_drained = true;
	}
	if (function_bar) {
		ops->clear_function_bar(context);
		state->function_bar_after = false;
	}
	ops->restore_terminal(context, cursor_shape_known,
	    process_entry_cursor_shape);
	state->terminal_restored = true;
	ops->end(context, 0U);
	state->ended = true;
	state->exit_status = 0U;
	return true;
}
