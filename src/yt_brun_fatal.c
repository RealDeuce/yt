#include "yt_brun_fatal.h"

#include <stdio.h>
#include <string.h>

struct runtime_error_name {
	uint8_t number;
	const char *name;
};

static const struct runtime_error_name runtime_error_names[] = {
	{0x02U, "Syntax error"},
	{0x03U, "RETURN without GOSUB"},
	{0x04U, "Out of data"},
	{0x05U, "Illegal function call"},
	{0x06U, "Overflow"},
	{0x07U, "Out of memory"},
	{0x09U, "Subscript out of range"},
	{0x0AU, "Redimensioned array"},
	{0x0BU, "Division by zero"},
	{0x0DU, "Type mismatch"},
	{0x0EU, "Out of string space"},
	{0x10U, "String formula too complex"},
	{0x14U, "RESUME without error"},
	{0x18U, "Device timeout"},
	{0x19U, "Device fault"},
	{0x1BU, "Out of paper"},
	{0x27U, "CASE ELSE expected"},
	{0x32U, "FIELD overflow"},
	{0x33U, "Internal error"},
	{0x34U, "Bad file number"},
	{0x35U, "File not found"},
	{0x36U, "Bad file mode"},
	{0x37U, "File already open"},
	{0x39U, "Device I/O error"},
	{0x3AU, "File already exists"},
	{0x3DU, "Disk full"},
	{0x3EU, "Input past end"},
	{0x3FU, "Bad record number"},
	{0x40U, "Bad file name"},
	{0x43U, "Too many files"},
	{0x44U, "Device unavailable"},
	{0x45U, "Communication buffer overflow"},
	{0x46U, "Permission denied"},
	{0x47U, "Disk not ready"},
	{0x48U, "Disk media error"},
	{0x49U, "Advanced feature error"},
	{0x4AU, "Rename across disks"},
	{0x4BU, "Path/file access error"},
	{0x4CU, "Path not found"},
};

bool
yt_brun_runtime_error_description(uint8_t error_number,
    const uint8_t **description, size_t *description_length)
{
	static const uint8_t fallback[] = "Unprintable error";
	size_t index;

	if (description == NULL || description_length == NULL)
		return false;
	for (index = 0U; index < YT_ARRAY_LEN(runtime_error_names); ++index) {
		if (runtime_error_names[index].number == error_number) {
			*description = (const uint8_t *)runtime_error_names[index].name;
			*description_length = strlen(runtime_error_names[index].name);
			return true;
		}
	}
	*description = fallback;
	*description_length = sizeof(fallback) - 1U;
	return true;
}

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

static bool
run_fatal_description(const uint8_t *description, size_t description_length,
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

	if ((description == NULL && description_length != 0U)
	    || description_length == 0U || module == NULL
	    || ops == NULL || ops->local == NULL
	    || ops->close_all == NULL || ops->restore_terminal == NULL
	    || ops->end == NULL || state == NULL
	    || (!redirected_stdin && ops->drain == NULL)
	    || (function_bar && ops->clear_function_bar == NULL))
		return false;
	while (module_length != 0U && module[module_length - 1U] == ' ')
		--module_length;
	if (module_length == 0U)
		return false;

	memset(state, 0, sizeof(*state));
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
	    &state->diagnostic_length, "\r")
	    || !append_bytes(state->diagnostic, sizeof(state->diagnostic),
	    &state->diagnostic_length, description, description_length)
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

bool
yt_brun_internal_fatal_run(enum yt_brun_internal_fatal_entry entry,
    const char module[8], bool has_source_line, int32_t source_line,
    uint16_t module_segment, uint16_t saved_ip, bool redirected_stdin,
    bool function_bar, bool cursor_shape_known,
    uint16_t process_entry_cursor_shape,
    const struct yt_brun_internal_fatal_ops *ops, void *context,
    struct yt_brun_internal_fatal_state *state)
{
	static const uint8_t gc[] = "String Space Corrupt during G.C.";
	static const uint8_t owner[] = "String Space Corrupt";
	const uint8_t *description;
	size_t description_length;

	if (entry == YT_BRUN_INTERNAL_FATAL_GC) {
		description = gc;
		description_length = sizeof(gc) - 1U;
	}
	else if (entry == YT_BRUN_INTERNAL_FATAL_OWNER) {
		description = owner;
		description_length = sizeof(owner) - 1U;
	}
	else
		return false;
	if (!run_fatal_description(description, description_length, module,
	    has_source_line, source_line, module_segment, saved_ip,
	    redirected_stdin, function_bar, cursor_shape_known,
	    process_entry_cursor_shape, ops, context, state))
		return false;
	state->entry = entry;
	return true;
}

bool
yt_brun_runtime_fatal_run(uint8_t error_number,
    const uint8_t *error_description, size_t error_description_length,
    const char module[8], bool has_source_line, int32_t source_line,
    uint16_t module_segment, uint16_t saved_ip, bool redirected_stdin,
    bool function_bar, bool cursor_shape_known,
    uint16_t process_entry_cursor_shape,
    const struct yt_brun_internal_fatal_ops *ops, void *context,
    struct yt_brun_runtime_fatal_state *state)
{
	if (state == NULL || error_number == 0U
	    || error_description == NULL || error_description_length == 0U
	    || error_description_length > sizeof(state->error_description))
		return false;
	memset(state, 0, sizeof(*state));
	state->error_number = error_number;
	memcpy(state->error_description, error_description,
	    error_description_length);
	state->error_description_length = error_description_length;
	return run_fatal_description(error_description, error_description_length,
	    module, has_source_line, source_line, module_segment, saved_ip,
	    redirected_stdin, function_bar, cursor_shape_known,
	    process_entry_cursor_shape, ops, context, &state->terminal);
}

bool
yt_brun_runtime_error_fatal_run(uint8_t error_number,
    const char module[8], bool has_source_line, int32_t source_line,
    uint16_t module_segment, uint16_t saved_ip, bool redirected_stdin,
    bool function_bar, bool cursor_shape_known,
    uint16_t process_entry_cursor_shape,
    const struct yt_brun_internal_fatal_ops *ops, void *context,
    struct yt_brun_runtime_fatal_state *state)
{
	const uint8_t *description;
	size_t description_length;

	if (!yt_brun_runtime_error_description(error_number, &description,
	    &description_length))
		return false;
	return yt_brun_runtime_fatal_run(error_number, description,
	    description_length, module, has_source_line, source_line,
	    module_segment, saved_ip, redirected_stdin, function_bar,
	    cursor_shape_known, process_entry_cursor_shape, ops, context, state);
}
