#include "yt_framebuffer.h"

#include "yt_brun_fatal.h"

#include <limits.h>
#include <string.h>

#define QB_PRINT_HEIGHT 24U
#define SPACE 0x20U
#define DEFAULT_ATTRIBUTE 0x07U
#define HIDDEN_CURSOR_SHAPE 0x2000U

static const uint8_t profile[] = YT_FRAMEBUFFER_PROFILE;

static void
put_u16(uint8_t **output, uint16_t value)
{
	(*output)[0] = (uint8_t)(value & 0xffU);
	(*output)[1] = (uint8_t)(value >> 8);
	*output += 2;
}

static void
put_u32(uint8_t **output, uint32_t value)
{
	(*output)[0] = (uint8_t)(value & 0xffU);
	(*output)[1] = (uint8_t)((value >> 8) & 0xffU);
	(*output)[2] = (uint8_t)((value >> 16) & 0xffU);
	(*output)[3] = (uint8_t)(value >> 24);
	*output += 4;
}

static uint16_t
get_u16(const uint8_t **input)
{
	uint16_t value = (uint16_t)(*input)[0]
	    | (uint16_t)((uint16_t)(*input)[1] << 8);

	*input += 2;
	return value;
}

static uint32_t
get_u32(const uint8_t **input)
{
	uint32_t value = (uint32_t)(*input)[0]
	    | ((uint32_t)(*input)[1] << 8)
	    | ((uint32_t)(*input)[2] << 16)
	    | ((uint32_t)(*input)[3] << 24);

	*input += 4;
	return value;
}

void
yt_framebuffer_init(struct yt_framebuffer_state *state,
    bool process_entry_cursor_shape_present,
    uint16_t process_entry_cursor_shape)
{
	if (state == NULL)
		return;
	memset(state, 0, sizeof(*state));
	memset(state->characters, SPACE, sizeof(state->characters));
	memset(state->attributes, DEFAULT_ATTRIBUTE,
	    sizeof(state->attributes));
	state->bios_row = 1U;
	state->bios_column = 1U;
	state->qb_row = 1U;
	state->qb_column = 1U;
	state->brun_bios_cache_row = 1U;
	state->brun_bios_cache_column = 1U;
	state->qb_attribute = DEFAULT_ATTRIBUTE;
	state->cursor_shape = HIDDEN_CURSOR_SHAPE;
	state->ansi_attribute = DEFAULT_ATTRIBUTE;
	state->ansi_saved_row = 1U;
	state->ansi_saved_column = 1U;
	state->process_entry_cursor_shape_present =
	    process_entry_cursor_shape_present;
	state->process_entry_cursor_shape = process_entry_cursor_shape;
}

bool
yt_framebuffer_validate(const struct yt_framebuffer_state *state)
{
	if (state == NULL)
		return false;
	if (state->bios_row < 1U || state->bios_row > YT_FRAMEBUFFER_HEIGHT
	    || state->bios_column < 1U
	    || state->bios_column > YT_FRAMEBUFFER_WIDTH + 1U
	    || state->qb_row < 1U || state->qb_row > YT_FRAMEBUFFER_HEIGHT
	    || state->qb_column < 1U
	    || state->qb_column > YT_FRAMEBUFFER_WIDTH + 1U
	    || state->brun_bios_cache_row < 1U
	    || state->brun_bios_cache_row > YT_FRAMEBUFFER_HEIGHT
	    || state->brun_bios_cache_column < 1U
	    || state->brun_bios_cache_column > YT_FRAMEBUFFER_WIDTH + 1U
	    || state->ansi_saved_row < 1U
	    || state->ansi_saved_row > YT_FRAMEBUFFER_HEIGHT
	    || state->ansi_saved_column < 1U
	    || state->ansi_saved_column > YT_FRAMEBUFFER_WIDTH
	    || state->ansi_argument_index >= 10U)
		return false;
	if (state->ansi_csi_pending && !state->ansi_escape_pending)
		return false;
	return true;
}

static bool
event_valid(const struct yt_framebuffer_event *event)
{
	bool has_data;

	if (event == NULL || event->operation < YT_FRAMEBUFFER_COLOR
	    || event->operation > YT_FRAMEBUFFER_PROCESS_STARTUP)
		return false;
	if (event->length != 0U && event->data == NULL)
		return false;
	has_data = event->length != 0U;
	switch (event->operation) {
	case YT_FRAMEBUFFER_COLOR:
		return event->has_foreground && event->has_background
		    && !has_data && !event->has_row && !event->has_column
		    && !event->has_cursor && !event->has_function_bar;
	case YT_FRAMEBUFFER_LOCATE:
		return (event->has_row || event->has_column)
		    && (!event->has_row || (event->row >= 1U
		    && event->row <= YT_FRAMEBUFFER_HEIGHT))
		    && (!event->has_column || (event->column >= 1U
		    && event->column <= YT_FRAMEBUFFER_WIDTH))
		    && (!event->has_cursor || (event->cursor_start <= 0x1fU
		    && event->cursor_stop <= 0x1fU))
		    && !has_data && !event->has_foreground
		    && !event->has_background && !event->has_function_bar;
	case YT_FRAMEBUFFER_FUNCTION_BAR_SET:
		return event->length == YT_FRAMEBUFFER_WIDTH
		    && event->has_function_bar && !event->has_foreground
		    && !event->has_background && !event->has_row
		    && !event->has_column && !event->has_cursor;
	case YT_FRAMEBUFFER_PRINT_RAW:
	case YT_FRAMEBUFFER_PRINT_NL:
	case YT_FRAMEBUFFER_CON_DEVICE_STREAM:
		return !event->has_foreground && !event->has_background
		    && !event->has_row && !event->has_column
		    && !event->has_cursor && !event->has_function_bar;
	case YT_FRAMEBUFFER_CLS:
	case YT_FRAMEBUFFER_CURSOR_HIDE:
	case YT_FRAMEBUFFER_END_CLEANUP:
	case YT_FRAMEBUFFER_PROCESS_STARTUP:
		return !has_data && !event->has_foreground
		    && !event->has_background && !event->has_row
		    && !event->has_column && !event->has_cursor
		    && !event->has_function_bar;
	}
	return false;
}

static void
position_bios(struct yt_framebuffer_state *state, uint16_t row,
    uint16_t column)
{
	state->brun_bios_cache_row = row;
	state->brun_bios_cache_column = column;
	state->bios_row = row;
	state->bios_column = column;
}

static void
sync_qb_cursor(struct yt_framebuffer_state *state)
{
	position_bios(state, state->qb_row, state->qb_column);
}

static void
scroll_rows(struct yt_framebuffer_state *state, size_t rows,
    uint8_t attribute)
{
	size_t end = rows * YT_FRAMEBUFFER_WIDTH;
	size_t retained = end - YT_FRAMEBUFFER_WIDTH;

	memmove(state->characters, state->characters + YT_FRAMEBUFFER_WIDTH,
	    retained);
	memmove(state->attributes, state->attributes + YT_FRAMEBUFFER_WIDTH,
	    retained);
	memset(state->characters + retained, SPACE, YT_FRAMEBUFFER_WIDTH);
	memset(state->attributes + retained, attribute,
	    YT_FRAMEBUFFER_WIDTH);
}

static void
qb_newline(struct yt_framebuffer_state *state)
{
	state->qb_column = 1U;
	if (state->qb_row >= QB_PRINT_HEIGHT) {
		scroll_rows(state, QB_PRINT_HEIGHT, state->qb_attribute);
		++state->qb_scrolls;
		state->qb_row = QB_PRINT_HEIGHT;
	} else {
		++state->qb_row;
	}
	sync_qb_cursor(state);
}

static enum yt_framebuffer_status
qb_write_at_bios(struct yt_framebuffer_state *state, uint8_t value)
{
	size_t index;

	if (state->bios_row < 1U || state->bios_row > YT_FRAMEBUFFER_HEIGHT
	    || state->bios_column < 1U
	    || state->bios_column > YT_FRAMEBUFFER_WIDTH)
		return YT_FRAMEBUFFER_INVALID_STATE;
	index = ((size_t)state->bios_row - 1U) * YT_FRAMEBUFFER_WIDTH
	    + (size_t)state->bios_column - 1U;
	state->characters[index] = value;
	state->attributes[index] = state->qb_attribute;
	return YT_FRAMEBUFFER_OK;
}

static void
function_bar_bytes(uint8_t output[YT_FRAMEBUFFER_WIDTH])
{
	size_t index;

	for (index = 0U; index < 10U; ++index) {
		size_t base = index * 8U;
		unsigned number = (unsigned)index + 1U;

		output[base] = number == 10U ? (uint8_t)'1' : (uint8_t)' ';
		output[base + 1U] = number == 10U
		    ? (uint8_t)'0' : (uint8_t)('0' + number);
		memset(output + base + 2U, SPACE, 6U);
	}
}

static void
qb_function_bar(struct yt_framebuffer_state *state, const uint8_t *data)
{
	size_t start = (YT_FRAMEBUFFER_HEIGHT - 1U) * YT_FRAMEBUFFER_WIDTH;

	memcpy(state->characters + start, data, YT_FRAMEBUFFER_WIDTH);
	memset(state->attributes + start, state->qb_attribute,
	    YT_FRAMEBUFFER_WIDTH);
	sync_qb_cursor(state);
}

static void
qb_cls(struct yt_framebuffer_state *state)
{
	size_t rows = state->function_bar ? QB_PRINT_HEIGHT
	    : YT_FRAMEBUFFER_HEIGHT;
	size_t count = rows * YT_FRAMEBUFFER_WIDTH;

	memset(state->characters, SPACE, count);
	memset(state->attributes, state->qb_attribute, count);
	if (state->function_bar) {
		uint8_t row[YT_FRAMEBUFFER_WIDTH];

		function_bar_bytes(row);
		qb_function_bar(state, row);
	}
	state->qb_row = 1U;
	state->qb_column = 1U;
	sync_qb_cursor(state);
}

static void
con_clear_parser(struct yt_framebuffer_state *state)
{
	state->ansi_escape_pending = false;
	state->ansi_csi_pending = false;
	memset(state->ansi_arguments, 0, sizeof(state->ansi_arguments));
	state->ansi_argument_index = 0U;
}

static void
con_scroll_if_needed(struct yt_framebuffer_state *state)
{
	if (state->bios_row == YT_FRAMEBUFFER_HEIGHT + 1U) {
		scroll_rows(state, YT_FRAMEBUFFER_HEIGHT, DEFAULT_ATTRIBUTE);
		++state->con_scrolls;
		--state->bios_row;
	}
}

static enum yt_framebuffer_status
con_teletype(struct yt_framebuffer_state *state, uint8_t value,
    bool use_attribute)
{
	if (value == 0x07U) {
		/* BEL has no framebuffer effect. */
	} else if (value == 0x08U) {
		if (state->bios_column > 1U)
			--state->bios_column;
	} else if (value == 0x0dU) {
		state->bios_column = 1U;
	} else if (value == 0x0aU) {
		++state->bios_row;
	} else if (value == 0x09U) {
		enum yt_framebuffer_status status;

		do {
			status = con_teletype(state, SPACE, use_attribute);
			if (status != YT_FRAMEBUFFER_OK)
				return status;
		} while (((state->bios_column - 1U) % 8U) != 0U);
		return YT_FRAMEBUFFER_OK;
	} else {
		size_t index;

		if (state->bios_row < 1U
		    || state->bios_row > YT_FRAMEBUFFER_HEIGHT
		    || state->bios_column < 1U
		    || state->bios_column > YT_FRAMEBUFFER_WIDTH)
			return YT_FRAMEBUFFER_INVALID_STATE;
		index = ((size_t)state->bios_row - 1U) * YT_FRAMEBUFFER_WIDTH
		    + (size_t)state->bios_column - 1U;
		state->characters[index] = value;
		if (use_attribute)
			state->attributes[index] = state->ansi_attribute;
		++state->bios_column;
	}
	if (state->bios_column == YT_FRAMEBUFFER_WIDTH + 1U) {
		state->bios_column = 1U;
		++state->bios_row;
	}
	con_scroll_if_needed(state);
	return YT_FRAMEBUFFER_OK;
}

static void
ansi_sgr(struct yt_framebuffer_state *state)
{
	static const uint8_t color_bits[8] = { 0U, 4U, 2U, 6U,
	    1U, 5U, 3U, 7U };
	size_t index;

	for (index = 0U; index <= state->ansi_argument_index; ++index) {
		uint8_t value = state->ansi_arguments[index];

		state->ansi_enabled = true;
		if (value == 0U) {
			state->ansi_attribute = DEFAULT_ATTRIBUTE;
			state->ansi_enabled = false;
		} else if (value == 1U) {
			state->ansi_attribute |= 0x08U;
		} else if (value == 5U) {
			state->ansi_attribute |= 0x80U;
		} else if (value == 7U) {
			state->ansi_attribute = 0x70U;
		} else if (value >= 30U && value <= 37U) {
			state->ansi_attribute &= 0xf8U;
			state->ansi_attribute |= color_bits[value - 30U];
		} else if (value >= 40U && value <= 47U) {
			state->ansi_attribute &= 0x8fU;
			state->ansi_attribute |=
			    (uint8_t)(color_bits[value - 40U] << 4);
		}
	}
}

static void
ansi_command(struct yt_framebuffer_state *state, uint8_t command)
{
	uint8_t first = state->ansi_arguments[0];
	uint16_t amount;

	if (command == (uint8_t)'m') {
		ansi_sgr(state);
	} else if (command == (uint8_t)'H' || command == (uint8_t)'f') {
		uint16_t row = first == 0U ? 1U : first;
		uint16_t column = state->ansi_arguments[1] == 0U
		    ? 1U : state->ansi_arguments[1];

		state->bios_row = row > YT_FRAMEBUFFER_HEIGHT
		    ? YT_FRAMEBUFFER_HEIGHT : row;
		state->bios_column = column > YT_FRAMEBUFFER_WIDTH
		    ? YT_FRAMEBUFFER_WIDTH : column;
	} else if (command == (uint8_t)'A') {
		amount = first == 0U ? 1U : first;
		state->bios_row = state->bios_row > amount
		    ? (uint16_t)(state->bios_row - amount) : 1U;
	} else if (command == (uint8_t)'B') {
		amount = first == 0U ? 1U : first;
		state->bios_row = state->bios_row + amount
		    > YT_FRAMEBUFFER_HEIGHT ? YT_FRAMEBUFFER_HEIGHT
		    : (uint16_t)(state->bios_row + amount);
	} else if (command == (uint8_t)'C') {
		amount = first == 0U ? 1U : first;
		state->bios_column = state->bios_column + amount
		    > YT_FRAMEBUFFER_WIDTH ? YT_FRAMEBUFFER_WIDTH
		    : (uint16_t)(state->bios_column + amount);
	} else if (command == (uint8_t)'D') {
		amount = first == 0U ? 1U : first;
		state->bios_column = state->bios_column > amount
		    ? (uint16_t)(state->bios_column - amount) : 1U;
	} else if (command == (uint8_t)'J') {
		memset(state->characters, SPACE, sizeof(state->characters));
		memset(state->attributes, state->ansi_attribute,
		    sizeof(state->attributes));
		state->bios_row = 1U;
		state->bios_column = 1U;
	} else if (command == (uint8_t)'s') {
		state->ansi_saved_row = state->bios_row;
		state->ansi_saved_column = state->bios_column;
	} else if (command == (uint8_t)'u') {
		state->bios_row = state->ansi_saved_row;
		state->bios_column = state->ansi_saved_column;
	}
	con_clear_parser(state);
}


static enum yt_framebuffer_status
qb_printable(struct yt_framebuffer_state *state, uint8_t value)
{
	uint16_t target_column;
	enum yt_framebuffer_status status;

	if (state->qb_column > YT_FRAMEBUFFER_WIDTH)
		qb_newline(state);
	target_column = state->qb_column;
	if (state->brun_bios_cache_row != state->qb_row
	    || state->brun_bios_cache_column != target_column)
		position_bios(state, state->qb_row, target_column);
	status = qb_write_at_bios(state, value);
	if (status != YT_FRAMEBUFFER_OK)
		return status;
	++state->qb_column;
	if (target_column < YT_FRAMEBUFFER_WIDTH)
		position_bios(state, state->qb_row, state->qb_column);
	else
		position_bios(state, state->qb_row, YT_FRAMEBUFFER_WIDTH);
	return YT_FRAMEBUFFER_OK;
}

static enum yt_framebuffer_status
qb_fast_string(struct yt_framebuffer_state *state, const uint8_t *data,
    size_t length)
{
	size_t index;
	uint16_t initial_column = state->qb_column;
	enum yt_framebuffer_status status;

	if (state->brun_bios_cache_row != state->qb_row
	    || state->brun_bios_cache_column != state->qb_column)
		position_bios(state, state->qb_row, state->qb_column);
	for (index = 0U; index < length; ++index) {
		status = qb_write_at_bios(state, data[index]);
		if (status != YT_FRAMEBUFFER_OK)
			return status;
		state->bios_row = state->qb_row;
		state->bios_column = (uint16_t)(initial_column + index + 1U);
	}
	state->qb_column = (uint16_t)(state->qb_column + length);
	position_bios(state, state->qb_row, state->qb_column);
	return YT_FRAMEBUFFER_OK;
}

static enum yt_framebuffer_status
qb_character(struct yt_framebuffer_state *state, uint8_t value)
{
	enum yt_framebuffer_status status;

	if (value <= 0x06U || (value >= 0x0eU && value <= 0x13U)
	    || (value >= 0x15U && value <= 0x1bU) || value == 0x07U)
		return YT_FRAMEBUFFER_OK;
	if (value == 0x08U) {
		if (state->qb_column > 1U) {
			--state->qb_column;
			if (state->brun_bios_cache_row != state->qb_row
			    || state->brun_bios_cache_column != state->qb_column)
				position_bios(state, state->qb_row,
				    state->qb_column);
			return qb_write_at_bios(state, SPACE);
		}
		return YT_FRAMEBUFFER_OK;
	}
	if (value == 0x09U) {
		do {
			status = qb_printable(state, SPACE);
			if (status != YT_FRAMEBUFFER_OK)
				return status;
		} while (((state->qb_column - 1U) % 8U) != 0U);
		return YT_FRAMEBUFFER_OK;
	}
	if (value == 0x0aU || value == 0x0dU) {
		qb_newline(state);
		return YT_FRAMEBUFFER_OK;
	}
	if (value == 0x0bU) {
		state->qb_row = 1U;
		state->qb_column = 1U;
		return YT_FRAMEBUFFER_OK;
	}
	if (value == 0x0cU) {
		uint16_t old_row = state->qb_row;
		uint16_t old_column = state->qb_column;

		memset(state->characters, SPACE, sizeof(state->characters));
		memset(state->attributes, state->qb_attribute,
		    sizeof(state->attributes));
		position_bios(state, old_row, old_column);
		state->qb_row = 1U;
		state->qb_column = 1U;
		return YT_FRAMEBUFFER_OK;
	}
	if (value == 0x14U)
		return YT_FRAMEBUFFER_UNSUPPORTED_CONTROL;
	if (value == 0x1cU) {
		if (state->qb_column < YT_FRAMEBUFFER_WIDTH)
			++state->qb_column;
		else if (state->qb_row < QB_PRINT_HEIGHT) {
			++state->qb_row;
			state->qb_column = 1U;
		}
		sync_qb_cursor(state);
		return YT_FRAMEBUFFER_OK;
	}
	if (value == 0x1dU) {
		if (state->qb_column > 1U)
			--state->qb_column;
		else if (state->qb_row > 1U
		    && state->qb_row <= QB_PRINT_HEIGHT) {
			--state->qb_row;
			state->qb_column = YT_FRAMEBUFFER_WIDTH;
		}
		sync_qb_cursor(state);
		return YT_FRAMEBUFFER_OK;
	}
	if (value == 0x1eU) {
		if (state->qb_row > 1U && state->qb_row <= QB_PRINT_HEIGHT)
			--state->qb_row;
		sync_qb_cursor(state);
		return YT_FRAMEBUFFER_OK;
	}
	if (value == 0x1fU) {
		if (state->qb_row < QB_PRINT_HEIGHT)
			++state->qb_row;
		sync_qb_cursor(state);
		return YT_FRAMEBUFFER_OK;
	}
	return qb_printable(state, value);
}

static enum yt_framebuffer_status
qb_print_raw(struct yt_framebuffer_state *state, const uint8_t *data,
    size_t length)
{
	size_t index;
	bool printable = true;

	if (length > 4U && length <= YT_FRAMEBUFFER_WIDTH
	    && state->qb_column <= YT_FRAMEBUFFER_WIDTH + 1U
	    && length <= YT_FRAMEBUFFER_WIDTH + 1U - state->qb_column) {
		for (index = 0U; index < length; ++index) {
			if (data[index] <= 0x1fU) {
				printable = false;
				break;
			}
		}
		if (printable)
			return qb_fast_string(state, data, length);
	}
	for (index = 0U; index < length; ++index) {
		enum yt_framebuffer_status status = qb_character(state,
		    data[index]);

		if (status != YT_FRAMEBUFFER_OK)
			return status;
	}
	return YT_FRAMEBUFFER_OK;
}

static enum yt_framebuffer_status
con_write(struct yt_framebuffer_state *state, const uint8_t *data,
    size_t length)
{
	size_t index;

	for (index = 0U; index < length; ++index) {
		uint8_t value = data[index];
		enum yt_framebuffer_status status;

		if (!state->ansi_escape_pending) {
			if (value == 0x1bU) {
				con_clear_parser(state);
				state->ansi_escape_pending = true;
				continue;
			}
			if (value == 0x0aU && state->ansi_lastwrite != 0x0dU) {
				status = con_teletype(state, 0x0dU,
				    state->ansi_enabled);
				if (status != YT_FRAMEBUFFER_OK)
					return status;
			}
			status = con_teletype(state, value,
			    state->ansi_enabled);
			if (status != YT_FRAMEBUFFER_OK)
				return status;
			state->ansi_lastwrite = value;
			continue;
		}
		if (!state->ansi_csi_pending) {
			if (value == (uint8_t)'[')
				state->ansi_csi_pending = true;
			else
				con_clear_parser(state);
			continue;
		}
		if (value >= (uint8_t)'0' && value <= (uint8_t)'9') {
			uint8_t *current = &state->ansi_arguments[
			    state->ansi_argument_index];

			*current = (uint8_t)((unsigned)*current * 10U
			    + (unsigned)(value - (uint8_t)'0'));
			continue;
		}
		if (value == (uint8_t)';') {
			if (state->ansi_argument_index == 9U)
				return YT_FRAMEBUFFER_ANSI_OVERFLOW;
			++state->ansi_argument_index;
			continue;
		}
		ansi_command(state, value);
	}
	return YT_FRAMEBUFFER_OK;
}

static enum yt_framebuffer_status
apply_valid(struct yt_framebuffer_state *state,
    const struct yt_framebuffer_event *event)
{
	enum yt_framebuffer_status status;

	switch (event->operation) {
	case YT_FRAMEBUFFER_COLOR:
		state->qb_attribute = (uint8_t)((event->foreground & 0x0fU)
		    | ((event->background & 0x07U) << 4)
		    | ((event->foreground & 0x10U) != 0U ? 0x80U : 0U));
		return YT_FRAMEBUFFER_OK;
	case YT_FRAMEBUFFER_CLS:
		qb_cls(state);
		return YT_FRAMEBUFFER_OK;
	case YT_FRAMEBUFFER_LOCATE:
		if (event->has_row)
			state->qb_row = event->row;
		if (event->has_column)
			state->qb_column = event->column;
		if (event->has_cursor) {
			state->cursor_shape = (uint16_t)(
			    (uint16_t)event->cursor_start << 8)
			    | event->cursor_stop;
			state->cursor_visible = event->cursor_visible;
		}
		sync_qb_cursor(state);
		return YT_FRAMEBUFFER_OK;
	case YT_FRAMEBUFFER_CURSOR_HIDE:
		state->cursor_shape = HIDDEN_CURSOR_SHAPE;
		state->cursor_visible = false;
		return YT_FRAMEBUFFER_OK;
	case YT_FRAMEBUFFER_FUNCTION_BAR_SET:
		qb_function_bar(state, event->data);
		state->function_bar = event->function_bar;
		return YT_FRAMEBUFFER_OK;
	case YT_FRAMEBUFFER_END_CLEANUP: {
		size_t start = (YT_FRAMEBUFFER_HEIGHT - 1U)
		    * YT_FRAMEBUFFER_WIDTH;
		uint16_t next_row = state->brun_bios_cache_row;

		memset(state->characters + start, SPACE, YT_FRAMEBUFFER_WIDTH);
		memset(state->attributes + start, state->qb_attribute,
		    YT_FRAMEBUFFER_WIDTH);
		if (next_row > YT_FRAMEBUFFER_HEIGHT - 1U)
			next_row = YT_FRAMEBUFFER_HEIGHT - 1U;
		position_bios(state, (uint16_t)(next_row + 1U), 1U);
		return YT_FRAMEBUFFER_OK;
	}
	case YT_FRAMEBUFFER_PRINT_RAW:
		return qb_print_raw(state, event->data, event->length);
	case YT_FRAMEBUFFER_PRINT_NL:
		status = qb_print_raw(state, event->data, event->length);
		if (status == YT_FRAMEBUFFER_OK)
			qb_newline(state);
		return status;
	case YT_FRAMEBUFFER_CON_DEVICE_STREAM:
		return con_write(state, event->data, event->length);
	case YT_FRAMEBUFFER_PROCESS_STARTUP: {
		struct yt_framebuffer_state fresh;
		uint8_t ansi_arguments[10];
		uint8_t ansi_attribute = state->ansi_attribute;
		bool ansi_enabled = state->ansi_enabled;
		uint16_t ansi_saved_row = state->ansi_saved_row;
		uint16_t ansi_saved_column = state->ansi_saved_column;
		uint8_t ansi_lastwrite = state->ansi_lastwrite;
		bool ansi_escape = state->ansi_escape_pending;
		bool ansi_csi = state->ansi_csi_pending;
		uint8_t ansi_index = state->ansi_argument_index;

		memcpy(ansi_arguments, state->ansi_arguments,
		    sizeof(ansi_arguments));
		yt_framebuffer_init(&fresh,
		    state->process_entry_cursor_shape_present,
		    state->process_entry_cursor_shape);
		fresh.ansi_attribute = ansi_attribute;
		fresh.ansi_enabled = ansi_enabled;
		fresh.ansi_saved_row = ansi_saved_row;
		fresh.ansi_saved_column = ansi_saved_column;
		fresh.ansi_lastwrite = ansi_lastwrite;
		fresh.ansi_escape_pending = ansi_escape;
		fresh.ansi_csi_pending = ansi_csi;
		fresh.ansi_argument_index = ansi_index;
		memcpy(fresh.ansi_arguments, ansi_arguments,
		    sizeof(ansi_arguments));
		*state = fresh;
		return YT_FRAMEBUFFER_OK;
	}
	}
	return YT_FRAMEBUFFER_INVALID_ARGUMENT;
}

enum yt_framebuffer_status
yt_framebuffer_apply(struct yt_framebuffer_state *state,
    const struct yt_framebuffer_event *event)
{
	struct yt_framebuffer_state next;
	enum yt_framebuffer_status status;

	if (!yt_framebuffer_validate(state))
		return YT_FRAMEBUFFER_INVALID_STATE;
	if (!event_valid(event))
		return YT_FRAMEBUFFER_INVALID_ARGUMENT;
	next = *state;
	status = apply_valid(&next, event);
	if (status != YT_FRAMEBUFFER_OK)
		return status;
	if (!yt_framebuffer_validate(&next))
		return YT_FRAMEBUFFER_INVALID_STATE;
	*state = next;
	return YT_FRAMEBUFFER_OK;
}

enum yt_framebuffer_status
yt_framebuffer_apply_all(struct yt_framebuffer_state *state,
    const struct yt_framebuffer_event *events, size_t count)
{
	struct yt_framebuffer_state next;
	size_t index;

	if (!yt_framebuffer_validate(state))
		return YT_FRAMEBUFFER_INVALID_STATE;
	if (count != 0U && events == NULL)
		return YT_FRAMEBUFFER_INVALID_ARGUMENT;
	next = *state;
	for (index = 0U; index < count; ++index) {
		enum yt_framebuffer_status status = yt_framebuffer_apply(&next,
		    &events[index]);

		if (status != YT_FRAMEBUFFER_OK)
			return status;
	}
	*state = next;
	return YT_FRAMEBUFFER_OK;
}

enum yt_framebuffer_status
yt_framebuffer_apply_present_event(struct yt_framebuffer_state *state,
    const struct yt_present_event *event)
{
	struct yt_framebuffer_event mapped;

	if (event == NULL)
		return YT_FRAMEBUFFER_INVALID_ARGUMENT;
	if (event->length > sizeof(event->data))
		return YT_FRAMEBUFFER_INVALID_ARGUMENT;
	memset(&mapped, 0, sizeof(mapped));
	switch (event->operation) {
	case YT_PRESENT_REMOTE_LINE:
	case YT_PRESENT_REMOTE_SEMI:
	case YT_PRESENT_LOCAL_PLAY:
	case YT_PRESENT_LOCAL_BEEP:
		return yt_framebuffer_validate(state) ? YT_FRAMEBUFFER_OK
		    : YT_FRAMEBUFFER_INVALID_STATE;
	case YT_PRESENT_LOCAL_COLOR:
		if (event->foreground < 0 || event->foreground > UCHAR_MAX
		    || event->background < 0 || event->background > UCHAR_MAX)
			return YT_FRAMEBUFFER_INVALID_ARGUMENT;
		mapped.operation = YT_FRAMEBUFFER_COLOR;
		mapped.has_foreground = true;
		mapped.foreground = (uint8_t)event->foreground;
		mapped.has_background = true;
		mapped.background = (uint8_t)event->background;
		break;
	case YT_PRESENT_LOCAL_LINE:
	case YT_PRESENT_LOCAL_SEMI:
		mapped.operation = event->operation == YT_PRESENT_LOCAL_LINE
		    ? YT_FRAMEBUFFER_PRINT_NL : YT_FRAMEBUFFER_PRINT_RAW;
		mapped.data = event->data;
		mapped.length = event->length;
		break;
	case YT_PRESENT_LOCAL_LOCATE:
		if (event->row == -1 && event->column == -1
		    && event->cursor_visible == 0) {
			mapped.operation = YT_FRAMEBUFFER_CURSOR_HIDE;
			break;
		}
		mapped.operation = YT_FRAMEBUFFER_LOCATE;
		if (event->row != -1) {
			if (event->row < 1
			    || event->row > (int)YT_FRAMEBUFFER_HEIGHT)
				return YT_FRAMEBUFFER_INVALID_ARGUMENT;
			mapped.has_row = true;
			mapped.row = (uint16_t)event->row;
		}
		if (event->column != -1) {
			if (event->column < 1
			    || event->column > (int)YT_FRAMEBUFFER_WIDTH)
				return YT_FRAMEBUFFER_INVALID_ARGUMENT;
			mapped.has_column = true;
			mapped.column = (uint16_t)event->column;
		}
		if (event->cursor_visible >= 0) {
			if (event->cursor_visible > 1 || event->cursor_start < 0
			    || event->cursor_start > 0x1f
			    || event->cursor_stop < 0
			    || event->cursor_stop > 0x1f)
				return YT_FRAMEBUFFER_INVALID_ARGUMENT;
			mapped.has_cursor = true;
			mapped.cursor_visible = event->cursor_visible != 0;
			mapped.cursor_start = (uint8_t)event->cursor_start;
			mapped.cursor_stop = (uint8_t)event->cursor_stop;
		}
		break;
	case YT_PRESENT_LOCAL_CLEAR:
		mapped.operation = YT_FRAMEBUFFER_CLS;
		break;
	default:
		return YT_FRAMEBUFFER_INVALID_ARGUMENT;
	}
	return yt_framebuffer_apply(state, &mapped);
}

enum yt_framebuffer_status
yt_framebuffer_apply_present_result(struct yt_framebuffer_state *state,
    const struct yt_present_result *result)
{
	struct yt_framebuffer_state next;
	size_t index;

	if (!yt_framebuffer_validate(state))
		return YT_FRAMEBUFFER_INVALID_STATE;
	if (result == NULL || result->event_count > YT_PRESENT_EVENTS)
		return YT_FRAMEBUFFER_INVALID_ARGUMENT;
	next = *state;
	for (index = 0U; index < result->event_count; ++index) {
		enum yt_framebuffer_status status =
		    yt_framebuffer_apply_present_event(&next,
		    &result->events[index]);

		if (status != YT_FRAMEBUFFER_OK)
			return status;
	}
	*state = next;
	return YT_FRAMEBUFFER_OK;
}

enum yt_framebuffer_status
yt_framebuffer_apply_con_observation(struct yt_framebuffer_state *state,
    const uint8_t *requested, size_t requested_length,
    const uint8_t *accepted, size_t accepted_length, bool completed,
    uint8_t error)
{
	struct yt_framebuffer_event event;

	if ((requested_length != 0U && requested == NULL)
	    || (accepted_length != 0U && accepted == NULL)
	    || accepted_length > requested_length
	    || (accepted_length != 0U
	    && memcmp(requested, accepted, accepted_length) != 0)
	    || (completed && accepted_length != requested_length)
	    || (completed && error != 0U) || (!completed && error == 0U))
		return YT_FRAMEBUFFER_INVALID_ARGUMENT;
	memset(&event, 0, sizeof(event));
	event.operation = YT_FRAMEBUFFER_CON_DEVICE_STREAM;
	event.data = accepted;
	event.length = accepted_length;
	return yt_framebuffer_apply(state, &event);
}

enum yt_framebuffer_status
yt_framebuffer_apply_brun_fatal(struct yt_framebuffer_state *state,
    const struct yt_brun_internal_fatal_state *terminal)
{
	struct yt_framebuffer_event events[5];
	uint8_t blank_row[YT_FRAMEBUFFER_WIDTH];
	size_t count = 0U;
	size_t expected_length;

	if (!yt_framebuffer_validate(state) || terminal == NULL
	    || terminal->diagnostic_length > sizeof(terminal->diagnostic)
	    || terminal->prompt_length > sizeof(terminal->prompt)
	    || terminal->local_length > sizeof(terminal->local_bytes)
	    || !terminal->close_all_completed || !terminal->terminal_restored
	    || !terminal->ended || terminal->exit_status != 0U
	    || terminal->function_bar_after
	    || terminal->function_bar_before != state->function_bar
	    || terminal->cursor_shape_known
	    != state->process_entry_cursor_shape_present
	    || (terminal->cursor_shape_known
	    && terminal->process_entry_cursor_shape
	    != state->process_entry_cursor_shape)
	    || (!terminal->redirected_stdin && !terminal->input_drained))
		return YT_FRAMEBUFFER_INVALID_ARGUMENT;
	expected_length = terminal->diagnostic_length + terminal->prompt_length
	    + (terminal->redirected_stdin ? 1U : 0U);
	if (expected_length > sizeof(terminal->local_bytes)
	    || terminal->local_length != expected_length
	    || memcmp(terminal->local_bytes, terminal->diagnostic,
	    terminal->diagnostic_length) != 0
	    || memcmp(terminal->local_bytes + terminal->diagnostic_length,
	    terminal->prompt, terminal->prompt_length) != 0
	    || (terminal->redirected_stdin
	    && terminal->local_bytes[expected_length - 1U] != '\r'))
		return YT_FRAMEBUFFER_INVALID_ARGUMENT;
	memset(events, 0, sizeof(events));
	events[count].operation = YT_FRAMEBUFFER_PRINT_RAW;
	events[count].data = terminal->diagnostic;
	events[count++].length = terminal->diagnostic_length;
	events[count].operation = YT_FRAMEBUFFER_PRINT_RAW;
	events[count].data = terminal->prompt;
	events[count++].length = terminal->prompt_length;
	if (terminal->redirected_stdin) {
		events[count].operation = YT_FRAMEBUFFER_PRINT_RAW;
		events[count].data = terminal->local_bytes
		    + terminal->local_length - 1U;
		events[count++].length = 1U;
	}
	if (terminal->function_bar_before) {
		memset(blank_row, SPACE, sizeof(blank_row));
		events[count].operation = YT_FRAMEBUFFER_FUNCTION_BAR_SET;
		events[count].data = blank_row;
		events[count].length = sizeof(blank_row);
		events[count].has_function_bar = true;
		events[count++].function_bar = false;
	}
	events[count++].operation = YT_FRAMEBUFFER_END_CLEANUP;
	return yt_framebuffer_apply_all(state, events, count);
}

enum yt_framebuffer_status
yt_framebuffer_serialize(const struct yt_framebuffer_state *state,
    uint8_t *output, size_t capacity, size_t *written)
{
	uint8_t *cursor;
	size_t index;

	if (!yt_framebuffer_validate(state) || output == NULL)
		return YT_FRAMEBUFFER_INVALID_ARGUMENT;
	if (capacity < YT_FRAMEBUFFER_STATE_BYTES)
		return YT_FRAMEBUFFER_CAPACITY;
	cursor = output;
	memcpy(cursor, "YTFB\x01", 5U);
	cursor += 5;
	put_u16(&cursor, (uint16_t)(sizeof(profile) - 1U));
	memcpy(cursor, profile, sizeof(profile) - 1U);
	cursor += sizeof(profile) - 1U;
	for (index = 0U; index < YT_FRAMEBUFFER_CELLS; ++index) {
		*cursor++ = state->characters[index];
		*cursor++ = state->attributes[index];
	}
	put_u16(&cursor, state->bios_row);
	put_u16(&cursor, state->bios_column);
	put_u16(&cursor, state->qb_row);
	put_u16(&cursor, state->qb_column);
	put_u16(&cursor, state->brun_bios_cache_row);
	put_u16(&cursor, state->brun_bios_cache_column);
	*cursor++ = state->qb_attribute;
	put_u16(&cursor, state->cursor_shape);
	*cursor++ = state->cursor_visible ? 1U : 0U;
	*cursor++ = state->ansi_attribute;
	*cursor++ = state->ansi_enabled ? 1U : 0U;
	put_u16(&cursor, state->ansi_saved_row);
	put_u16(&cursor, state->ansi_saved_column);
	*cursor++ = state->ansi_lastwrite;
	*cursor++ = state->ansi_escape_pending ? 1U : 0U;
	*cursor++ = state->ansi_csi_pending ? 1U : 0U;
	*cursor++ = state->ansi_argument_index;
	memcpy(cursor, state->ansi_arguments, sizeof(state->ansi_arguments));
	cursor += sizeof(state->ansi_arguments);
	put_u32(&cursor, state->qb_scrolls);
	put_u32(&cursor, state->con_scrolls);
	*cursor++ = state->function_bar ? 1U : 0U;
	*cursor++ = state->process_entry_cursor_shape_present ? 1U : 0U;
	put_u16(&cursor, state->process_entry_cursor_shape_present
	    ? state->process_entry_cursor_shape : 0U);
	if ((size_t)(cursor - output) != YT_FRAMEBUFFER_STATE_BYTES)
		return YT_FRAMEBUFFER_INVALID_STATE;
	if (written != NULL)
		*written = YT_FRAMEBUFFER_STATE_BYTES;
	return YT_FRAMEBUFFER_OK;
}

enum yt_framebuffer_status
yt_framebuffer_deserialize(struct yt_framebuffer_state *state,
    const uint8_t *input, size_t length)
{
	struct yt_framebuffer_state next;
	const uint8_t *cursor;
	size_t index;
	uint16_t profile_length;
	uint8_t boolean;
	uint8_t canonical[YT_FRAMEBUFFER_STATE_BYTES];

	if (state == NULL || input == NULL
	    || length != YT_FRAMEBUFFER_STATE_BYTES
	    || memcmp(input, "YTFB\x01", 5U) != 0)
		return YT_FRAMEBUFFER_INVALID_ARGUMENT;
	cursor = input + 5U;
	profile_length = get_u16(&cursor);
	if (profile_length != sizeof(profile) - 1U
	    || memcmp(cursor, profile, sizeof(profile) - 1U) != 0)
		return YT_FRAMEBUFFER_INVALID_ARGUMENT;
	cursor += sizeof(profile) - 1U;
	memset(&next, 0, sizeof(next));
	for (index = 0U; index < YT_FRAMEBUFFER_CELLS; ++index) {
		next.characters[index] = *cursor++;
		next.attributes[index] = *cursor++;
	}
	next.bios_row = get_u16(&cursor);
	next.bios_column = get_u16(&cursor);
	next.qb_row = get_u16(&cursor);
	next.qb_column = get_u16(&cursor);
	next.brun_bios_cache_row = get_u16(&cursor);
	next.brun_bios_cache_column = get_u16(&cursor);
	next.qb_attribute = *cursor++;
	next.cursor_shape = get_u16(&cursor);
	boolean = *cursor++;
	if (boolean > 1U)
		return YT_FRAMEBUFFER_INVALID_ARGUMENT;
	next.cursor_visible = boolean != 0U;
	next.ansi_attribute = *cursor++;
	boolean = *cursor++;
	if (boolean > 1U)
		return YT_FRAMEBUFFER_INVALID_ARGUMENT;
	next.ansi_enabled = boolean != 0U;
	next.ansi_saved_row = get_u16(&cursor);
	next.ansi_saved_column = get_u16(&cursor);
	next.ansi_lastwrite = *cursor++;
	boolean = *cursor++;
	if (boolean > 1U)
		return YT_FRAMEBUFFER_INVALID_ARGUMENT;
	next.ansi_escape_pending = boolean != 0U;
	boolean = *cursor++;
	if (boolean > 1U)
		return YT_FRAMEBUFFER_INVALID_ARGUMENT;
	next.ansi_csi_pending = boolean != 0U;
	next.ansi_argument_index = *cursor++;
	memcpy(next.ansi_arguments, cursor, sizeof(next.ansi_arguments));
	cursor += sizeof(next.ansi_arguments);
	next.qb_scrolls = get_u32(&cursor);
	next.con_scrolls = get_u32(&cursor);
	boolean = *cursor++;
	if (boolean > 1U)
		return YT_FRAMEBUFFER_INVALID_ARGUMENT;
	next.function_bar = boolean != 0U;
	boolean = *cursor++;
	if (boolean > 1U)
		return YT_FRAMEBUFFER_INVALID_ARGUMENT;
	next.process_entry_cursor_shape_present = boolean != 0U;
	next.process_entry_cursor_shape = get_u16(&cursor);
	if (!next.process_entry_cursor_shape_present
	    && next.process_entry_cursor_shape != 0U)
		return YT_FRAMEBUFFER_INVALID_ARGUMENT;
	if ((size_t)(cursor - input) != length
	    || !yt_framebuffer_validate(&next))
		return YT_FRAMEBUFFER_INVALID_ARGUMENT;
	if (yt_framebuffer_serialize(&next, canonical, sizeof(canonical), NULL)
	    != YT_FRAMEBUFFER_OK
	    || memcmp(canonical, input, sizeof(canonical)) != 0)
		return YT_FRAMEBUFFER_INVALID_ARGUMENT;
	*state = next;
	return YT_FRAMEBUFFER_OK;
}
