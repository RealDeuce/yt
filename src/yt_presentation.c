#include "yt_presentation.h"

#include "qb.h"

#include <limits.h>
#include <math.h>
#include <string.h>

float
yt_present_background(const struct yt_present_state *state)
{
	if (state == NULL)
		return 0.0f;
	return state->background;
}

void
yt_present_set_background(struct yt_present_state *state, float value)
{
	if (state == NULL)
		return;
	state->background = value;
}

float
yt_present_bold(const struct yt_present_state *state)
{
	if (state == NULL)
		return 0.0f;
	return state->bold;
}

void
yt_present_set_bold(struct yt_present_state *state, float value)
{
	if (state == NULL)
		return;
	state->bold = value;
}

float
yt_present_blink(const struct yt_present_state *state)
{
	if (state == NULL)
		return 0.0f;
	return state->blink;
}

void
yt_present_set_blink(struct yt_present_state *state, float value)
{
	if (state == NULL)
		return;
	state->blink = value;
}

float
yt_present_color_initialized(const struct yt_present_state *state)
{
	if (state == NULL)
		return 0.0f;
	return state->color_initialized;
}

void
yt_present_set_color_initialized(struct yt_present_state *state, float value)
{
	if (state == NULL)
		return;
	state->color_initialized = value;
}

float
yt_present_color_memory(const struct yt_present_state *state, size_t index)
{
	if (state == NULL || index >= 8U)
		return 0.0f;
	return state->color_memory[index];
}

static bool
color_memory_index(const struct yt_present_state *state, int index,
    float *value)
{
	if (state == NULL || value == NULL)
		return false;
	if (index < 0 || index >= 8)
		return false;
	*value = yt_present_color_memory(state, (size_t)index);
	return true;
}

void
yt_present_set_color_memory(struct yt_present_state *state, size_t index,
    float value)
{
	if (state == NULL || index >= 8U)
		return;
	state->color_memory[index] = value;
}

float
yt_present_cached_foreground(const struct yt_present_state *state)
{
	if (state == NULL)
		return 0.0f;
	return state->cached_foreground;
}

float
yt_present_cached_background(const struct yt_present_state *state)
{
	if (state == NULL)
		return 0.0f;
	return state->cached_background;
}

static enum yt_present_status
append_event(struct yt_present_result *result,
    enum yt_present_operation operation, const void *data, size_t length,
    int foreground, int background)
{
	struct yt_present_event *event;

	if (result->event_count >= YT_PRESENT_EVENTS
	    || length > YT_PRESENT_EVENT_DATA)
		return YT_PRESENT_CAPACITY;
	event = &result->events[result->event_count++];
	memset(event, 0, sizeof(*event));
	event->operation = operation;
	event->foreground = foreground;
	event->background = background;
	if (length != 0)
		memcpy(event->data, data, length);
	event->length = length;
	return YT_PRESENT_OK;
}

static enum yt_present_status
append_remote(struct yt_present_result *result,
    enum yt_present_operation operation, const void *data, size_t length)
{
	size_t suffix = operation == YT_PRESENT_REMOTE_LINE ? 1U : 0U;
	enum yt_present_status status;

	if (length > sizeof(result->remote) - result->remote_length
	    || suffix > sizeof(result->remote) - result->remote_length - length)
		return YT_PRESENT_CAPACITY;
	status = append_event(result, operation, data, length, 0, 0);
	if (status != YT_PRESENT_OK)
		return status;
	if (length != 0)
		memcpy(result->remote + result->remote_length, data, length);
	result->remote_length += length;
	if (suffix != 0)
		result->remote[result->remote_length++] = '\r';
	return YT_PRESENT_OK;
}

static enum yt_present_status
append_local(struct yt_present_result *result,
    enum yt_present_operation operation, const void *data, size_t length,
    int foreground, int background)
{
	return append_event(result, operation, data, length, foreground,
	    background);
}

static enum yt_present_status
append_locate(struct yt_present_result *result, int row, int column,
    int cursor_visible, int cursor_start, int cursor_stop)
{
	struct yt_present_event *event;
	enum yt_present_status status = append_event(result,
	    YT_PRESENT_LOCAL_LOCATE, NULL, 0, 0, 0);

	if (status != YT_PRESENT_OK)
		return status;
	event = &result->events[result->event_count - 1U];
	event->row = row;
	event->column = column;
	event->cursor_visible = cursor_visible;
	event->cursor_start = cursor_start;
	event->cursor_stop = cursor_stop;
	return YT_PRESENT_OK;
}

static enum yt_present_status
append_beep(struct yt_present_result *result)
{
	return append_event(result, YT_PRESENT_LOCAL_BEEP, NULL, 0, 0, 0);
}

static enum yt_present_status
convert(const struct yt_present_state *state, double value, int *converted)
{
	bool overflow;
	int32_t result = qb_cint_mode(value, state->sound.conversion_mode,
	    &overflow);

	if (overflow)
		return YT_PRESENT_OVERFLOW;
	*converted = (int)result;
	return YT_PRESENT_OK;
}

static enum yt_present_status
color_digit(float value, uint8_t *digit)
{
	char rendered[64];
	int length = qb_str_single(rendered, sizeof(rendered), value);

	if (length < 2)
		return YT_PRESENT_RANGE;
	*digit = (uint8_t)rendered[1];
	return YT_PRESENT_OK;
}

static enum yt_present_status
stage_color_cache(struct yt_present_state *state,
    struct yt_present_result *result, float foreground, float background)
{
	if (result->event_count == 0U)
		return YT_PRESENT_CAPACITY;
	if (result->events[result->event_count - 1U].operation
	    != YT_PRESENT_REMOTE_SEMI)
		return YT_PRESENT_OVERFLOW;
	state->cached_foreground = foreground;
	state->cached_background = background;
	return YT_PRESENT_OK;
}

static enum yt_present_status
build_color(struct yt_present_state *state,
    struct yt_present_result *result)
{
	static const float standard[8] = {0, 4, 2, 6, 1, 5, 3, 7};
	uint8_t sequence[32];
	size_t length = 0;
	float bright = 0.0f;
	float background = yt_present_background(state);
	float bold = yt_present_bold(state);
	float blink = yt_present_blink(state);
	float cached_foreground = yt_present_cached_foreground(state);
	float cached_background = yt_present_cached_background(state);
	int foreground_index;
	int background_index;
	int local_foreground;
	int local_background;
	float mapped_foreground;
	float mapped_background;
	int forced_bold;
	int forced_blink;
	enum yt_present_status status;
	size_t index;

	if (yt_present_color_initialized(state) == 0.0f) {
		yt_present_set_color_initialized(state, 1.0f);
		for (index = 0U; index < 8U; ++index)
			yt_present_set_color_memory(state, index, standard[index]);
	}
	if (state->foreground == background) {
		state->foreground = 3.0f;
		yt_present_set_background(state, 0.0f);
		background = 0.0f;
	}
	if (bold == 1.0f)
		bright = 8.0f;
	if (blink == 1.0f)
		bright += 16.0f;
	status = convert(state, state->foreground, &foreground_index);
	if (status != YT_PRESENT_OK)
		return status;
	status = convert(state, background, &background_index);
	if (status != YT_PRESENT_OK)
		return status;
	if (!color_memory_index(state, foreground_index, &mapped_foreground)
	    || !color_memory_index(state, background_index,
	    &mapped_background))
		return YT_PRESENT_RANGE;
	status = convert(state, (double)(float)(mapped_foreground + bright),
	    &local_foreground);
	if (status != YT_PRESENT_OK)
		return status;
	status = convert(state, mapped_background, &local_background);
	if (status != YT_PRESENT_OK)
		return status;
	status = append_local(result, YT_PRESENT_LOCAL_COLOR, NULL, 0,
	    local_foreground, local_background);
	if (status != YT_PRESENT_OK)
		return status;
	if (state->sound.mode == 0.0f) {
		memcpy(sequence + length, "\x1b[0;3", 5);
		length += 5;
		status = color_digit(state->foreground, &sequence[length++]);
		if (status != YT_PRESENT_OK)
			return status;
		memcpy(sequence + length, ";4", 2);
		length += 2;
		status = color_digit(background, &sequence[length++]);
		if (status != YT_PRESENT_OK)
			return status;
		if (blink == 1.0f) {
			memcpy(sequence + length, ";5", 2);
			length += 2;
		}
		if (bold == 1.0f) {
			memcpy(sequence + length, ";1", 2);
			length += 2;
		}
		sequence[length++] = 'm';
		status = convert(state, bold, &forced_bold);
		if (status != YT_PRESENT_OK)
			return status;
		status = convert(state, blink, &forced_blink);
		if (status != YT_PRESENT_OK)
			return status;
		if ((forced_bold | forced_blink) != 0) {
			status = append_remote(result, YT_PRESENT_REMOTE_SEMI,
			    sequence, length);
			if (status != YT_PRESENT_OK)
				return status;
			status = stage_color_cache(state, result, 0.0f, 0.0f);
			if (status != YT_PRESENT_OK)
				return status;
		}
		else if (state->foreground != cached_foreground
		    || background != cached_background) {
			status = append_remote(result, YT_PRESENT_REMOTE_SEMI,
			    sequence, length);
			if (status != YT_PRESENT_OK)
				return status;
			status = stage_color_cache(state, result,
			    state->foreground, background);
			if (status != YT_PRESENT_OK)
				return status;
		}
		else
			status = YT_PRESENT_OK;
	}
	yt_present_set_bold(state, 0.0f);
	yt_present_set_blink(state, 0.0f);
	return YT_PRESENT_OK;
}

static enum yt_present_status
prepare_color(struct yt_present_state *state,
    struct yt_present_result *result)
{
	if (state->sound.ansi == 0.0f)
		return YT_PRESENT_OK;
	return build_color(state, result);
}

static enum yt_present_status
emit_character(const uint8_t *text, size_t length,
    struct yt_present_state *state, struct yt_present_result *result)
{
	enum yt_present_status status = prepare_color(state, result);

	if (status != YT_PRESENT_OK)
		return status;
	if (state->sound.snoop != 0.0f) {
		status = append_local(result, YT_PRESENT_LOCAL_SEMI, text,
		    length, 0, 0);
		if (status != YT_PRESENT_OK)
			return status;
	}
	if (state->sound.mode != 1.0f)
		return append_remote(result, YT_PRESENT_REMOTE_SEMI, text,
		    length);
	return YT_PRESENT_OK;
}

static enum yt_present_status
emit_line(const uint8_t *text, size_t length,
    struct yt_present_state *state, struct yt_present_result *result)
{
	static const uint8_t line_feed[] = {'\n'};
	enum yt_present_status status = prepare_color(state, result);

	if (status != YT_PRESENT_OK)
		return status;
	if (state->sound.snoop != 0.0f) {
		status = append_local(result, YT_PRESENT_LOCAL_LINE, text,
		    length, 0, 0);
		if (status != YT_PRESENT_OK)
			return status;
	}
	if (state->sound.mode != 1.0f) {
		status = append_remote(result, YT_PRESENT_REMOTE_LINE, text,
		    length);
		if (status != YT_PRESENT_OK)
			return status;
		return append_remote(result, YT_PRESENT_REMOTE_SEMI, line_feed,
		    sizeof(line_feed));
	}
	return YT_PRESENT_OK;
}

enum yt_present_status
yt_present_color(struct yt_present_state *state,
    struct yt_present_result *result)
{
	memset(result, 0, sizeof(*result));
	return build_color(state, result);
}

enum yt_present_status
yt_present_line(const uint8_t *text, size_t length,
    struct yt_present_state *state, struct yt_present_result *result)
{
	memset(result, 0, sizeof(*result));
	return emit_line(text, length, state, result);
}

enum yt_present_status
yt_present_character(const uint8_t *text, size_t length,
    struct yt_present_state *state, struct yt_present_result *result)
{
	memset(result, 0, sizeof(*result));
	return emit_character(text, length, state, result);
}

enum yt_present_status
yt_present_bold_line(const uint8_t *text, size_t length,
    struct yt_present_state *state, struct yt_present_result *result)
{
	memset(result, 0, sizeof(*result));
	yt_present_set_bold(state, 1.0f);
	return emit_line(text, length, state, result);
}

enum yt_present_status
yt_present_bold_character(const uint8_t *text, size_t length,
    struct yt_present_state *state, struct yt_present_result *result)
{
	memset(result, 0, sizeof(*result));
	if (state->sound.ansi != 0.0f)
		yt_present_set_bold(state, 1.0f);
	return emit_character(text, length, state, result);
}

enum yt_present_status
yt_present_paged_text(const uint8_t *text, size_t length,
    struct yt_present_state *state, struct yt_present_result *result)
{
	enum yt_present_status status;

	memset(result, 0, sizeof(*result));
	status = prepare_color(state, result);
	if (status != YT_PRESENT_OK)
		return status;
	if (state->sound.snoop != 0.0f) {
		status = append_local(result, YT_PRESENT_LOCAL_SEMI, text,
		    length, 0, 0);
		if (status != YT_PRESENT_OK)
			return status;
	}
	if (state->sound.mode == 0.0f)
		return append_remote(result, YT_PRESENT_REMOTE_SEMI, text,
		    length);
	return YT_PRESENT_OK;
}

enum yt_present_status
yt_present_paged_finish(bool suppress_newline,
    struct yt_present_state *state, struct yt_present_result *result)
{
	static const uint8_t line_feed = '\n';
	enum yt_present_status status;

	memset(result, 0, sizeof(*result));
	if (!suppress_newline) {
		if (state->sound.mode == 0.0f) {
			status = append_remote(result, YT_PRESENT_REMOTE_LINE,
			    &line_feed, 1);
			if (status != YT_PRESENT_OK)
				return status;
		}
		if (state->sound.snoop != 0.0f) {
			status = append_local(result, YT_PRESENT_LOCAL_LINE,
			    NULL, 0, 0, 0);
			if (status != YT_PRESENT_OK)
				return status;
		}
	}
	return append_local(result, YT_PRESENT_LOCAL_COLOR, NULL, 0, 7, 0);
}

enum yt_present_status
yt_present_editor_echo(const uint8_t *local, size_t local_length,
    const uint8_t *remote, size_t remote_length,
    struct yt_present_state *state, struct yt_present_result *result)
{
	enum yt_present_status status;

	memset(result, 0, sizeof(*result));
	if (state->sound.snoop != 0.0f) {
		status = append_local(result, YT_PRESENT_LOCAL_SEMI, local,
		    local_length, 0, 0);
		if (status != YT_PRESENT_OK)
			return status;
	}
	if (state->sound.mode == 0.0f)
		return append_remote(result, YT_PRESENT_REMOTE_SEMI, remote,
		    remote_length);
	return YT_PRESENT_OK;
}

enum yt_present_status
yt_present_local_line(const uint8_t *text, size_t length,
    struct yt_present_state *state, struct yt_present_result *result)
{
	memset(result, 0, sizeof(*result));
	if (state->sound.snoop != 0.0f)
		return append_local(result, YT_PRESENT_LOCAL_LINE, text, length,
		    0, 0);
	return YT_PRESENT_OK;
}

enum yt_present_status
yt_present_forced_local_line(const uint8_t *text, size_t length,
    struct yt_present_result *result)
{
	memset(result, 0, sizeof(*result));
	return append_local(result, YT_PRESENT_LOCAL_LINE, text, length, 0, 0);
}

enum yt_present_status
yt_present_local_beep(struct yt_present_result *result)
{
	memset(result, 0, sizeof(*result));
	return append_beep(result);
}

enum yt_present_status
yt_present_right_aligned(const uint8_t *text, size_t length, float width,
    struct yt_present_state *state, struct yt_present_result *result)
{
	uint8_t rendered[YT_PRESENT_EVENT_DATA];
	size_t rendered_length;
	int count;
	enum yt_present_status status;

	memset(result, 0, sizeof(*result));
	if ((float)length < width) {
		status = convert(state, (float)(width - (float)length), &count);
		if (status != YT_PRESENT_OK)
			return status;
		if (count < 0)
			return YT_PRESENT_RANGE;
		if (length > sizeof(rendered)
		    || (size_t)count > sizeof(rendered) - length)
			return YT_PRESENT_CAPACITY;
		memset(rendered, ' ', (size_t)count);
		if (length != 0)
			memcpy(rendered + count, text, length);
		rendered_length = (size_t)count + length;
	}
	else {
		status = convert(state, width, &count);
		if (status != YT_PRESENT_OK)
			return status;
		if (count < 0)
			return YT_PRESENT_RANGE;
		rendered_length = (size_t)count;
		if (rendered_length > length)
			rendered_length = length;
		if (rendered_length > sizeof(rendered))
			return YT_PRESENT_CAPACITY;
		if (rendered_length != 0)
			memcpy(rendered, text + length - rendered_length,
			    rendered_length);
	}
	return emit_character(rendered, rendered_length, state, result);
}

enum yt_present_status
yt_present_fixed_width(uint8_t *text, size_t *length, size_t capacity,
    float width, struct yt_present_state *state,
    struct yt_present_result *result)
{
	int space_count;
	int left_count;
	size_t concatenated;
	enum yt_present_status status;

	memset(result, 0, sizeof(*result));
	if (*length > capacity)
		return YT_PRESENT_CAPACITY;
	status = convert(state, width, &space_count);
	if (status != YT_PRESENT_OK)
		return status;
	if (space_count < 0)
		return YT_PRESENT_RANGE;
	if ((size_t)space_count > capacity - *length)
		return YT_PRESENT_CAPACITY;
	memset(text + *length, ' ', (size_t)space_count);
	concatenated = *length + (size_t)space_count;
	*length = concatenated;
	status = convert(state, width, &left_count);
	if (status != YT_PRESENT_OK)
		return status;
	if (left_count < 0)
		return YT_PRESENT_RANGE;
	if ((size_t)left_count < *length)
		*length = (size_t)left_count;
	return emit_character(text, *length, state, result);
}

enum yt_present_status
yt_present_centered_line(const uint8_t *text, size_t length,
    struct yt_present_state *state, struct yt_present_result *result)
{
	uint8_t rendered[YT_PRESENT_EVENT_DATA];
	size_t margin;

	memset(result, 0, sizeof(*result));
	if (length == 0 || length > 78U)
		return emit_line(text, length, state, result);
	margin = (79U - length) / 2U;
	memset(rendered, ' ', margin);
	memcpy(rendered + margin, text, length);
	return emit_line(rendered, margin + length, state, result);
}

enum yt_present_status
yt_present_attention(const uint8_t *text, size_t length,
    struct yt_present_state *state, struct yt_present_result *result)
{
	struct yt_sound_result sound;
	enum yt_sound_status sound_status;
	enum yt_present_status status;

	memset(result, 0, sizeof(*result));
	state->foreground = 3.0f;
	yt_present_set_background(state, 1.0f);
	yt_present_set_blink(state, 1.0f);
	if (state->sound.ansi != 0.0f)
		yt_present_set_bold(state, 1.0f);
	status = emit_character(text, length, state, result);
	if (status != YT_PRESENT_OK)
		return status;
	yt_present_set_background(state, 0.0f);
	status = emit_line(NULL, 0, state, result);
	if (status != YT_PRESENT_OK)
		return status;
	sound_status = yt_sound_dispatch(6.0f, &state->sound, &sound);
	if (sound_status != YT_SOUND_OK)
		return YT_PRESENT_SOUND_ERROR;
	if (sound.remote_length != 0) {
		status = append_remote(result, YT_PRESENT_REMOTE_SEMI,
		    sound.remote, sound.remote_length);
		if (status != YT_PRESENT_OK)
			return status;
	}
	if (sound.play_length != 0)
		return append_local(result, YT_PRESENT_LOCAL_PLAY,
		    sound.play, sound.play_length, 0, 0);
	return YT_PRESENT_OK;
}

static enum yt_present_status
present_sound_toggle_result(struct yt_present_state *state,
    struct yt_present_result *result, enum yt_sound_status sound_status,
    const struct yt_sound_result *sound)
{
	enum yt_present_status status;

	memset(result, 0, sizeof(*result));
	if (sound->line_length != 0) {
		status = emit_line(sound->line, sound->line_length, state, result);
		if (status != YT_PRESENT_OK)
			return status;
	}
	if (sound->remote_length != 0) {
		status = append_remote(result, YT_PRESENT_REMOTE_SEMI,
		    sound->remote, sound->remote_length);
		if (status != YT_PRESENT_OK)
			return status;
	}
	if (sound->play_length != 0) {
		status = append_local(result, YT_PRESENT_LOCAL_PLAY,
		    sound->play, sound->play_length, 0, 0);
		if (status != YT_PRESENT_OK)
			return status;
	}
	return sound_status == YT_SOUND_OK
	    ? YT_PRESENT_OK : YT_PRESENT_SOUND_ERROR;
}

enum yt_present_status
yt_present_sound_toggle(struct yt_present_state *state,
    struct yt_present_result *result)
{
	struct yt_sound_result sound;
	enum yt_sound_status sound_status;

	sound_status = yt_sound_toggle(&state->sound, &sound);
	return present_sound_toggle_result(state, result, sound_status, &sound);
}

enum yt_present_status
yt_present_sound(float selector, struct yt_present_state *state,
    struct yt_present_result *result)
{
	struct yt_sound_result sound;
	enum yt_sound_status sound_status;
	enum yt_present_status status;

	memset(result, 0, sizeof(*result));
	sound_status = yt_sound_dispatch(selector, &state->sound, &sound);
	if (sound.remote_length != 0) {
		status = append_remote(result, YT_PRESENT_REMOTE_SEMI,
		    sound.remote, sound.remote_length);
		if (status != YT_PRESENT_OK)
			return status;
	}
	if (sound.play_length != 0) {
		status = append_local(result, YT_PRESENT_LOCAL_PLAY,
		    sound.play, sound.play_length, 0, 0);
		if (status != YT_PRESENT_OK)
			return status;
	}
	return sound_status == YT_SOUND_OK
	    ? YT_PRESENT_OK : YT_PRESENT_SOUND_ERROR;
}

enum yt_present_status
yt_present_press_prompt(struct yt_present_state *state,
    struct yt_present_result *result, float *saved_foreground)
{
	static const uint8_t prompt[] = "*[ Press any Key ]*";

	memset(result, 0, sizeof(*result));
	*saved_foreground = state->foreground;
	state->foreground = 3.0f;
	return yt_present_bold_character(prompt, sizeof(prompt) - 1U, state,
	    result);
}

enum yt_present_status
yt_present_press_cleanup(float saved_foreground,
    struct yt_present_state *state, struct yt_present_result *result)
{
	static const uint8_t carriage_return = '\r';
	static const uint8_t spaces[] = "                   ";
	enum yt_present_status status;

	memset(result, 0, sizeof(*result));
	status = append_locate(result, -1, 1, -1, 0, 0);
	if (status != YT_PRESENT_OK)
		return status;
	if (state->sound.mode == 0.0f) {
		status = append_remote(result, YT_PRESENT_REMOTE_SEMI,
		    &carriage_return, 1);
		if (status != YT_PRESENT_OK)
			return status;
	}
	status = emit_character(spaces, sizeof(spaces) - 1U, state, result);
	if (status != YT_PRESENT_OK)
		return status;
	if (state->sound.mode == 0.0f) {
		status = append_remote(result, YT_PRESENT_REMOTE_SEMI,
		    &carriage_return, 1);
		if (status != YT_PRESENT_OK)
			return status;
	}
	status = append_locate(result, -1, 1, -1, 0, 0);
	if (status != YT_PRESENT_OK)
		return status;
	state->foreground = saved_foreground;
	return YT_PRESENT_OK;
}

enum yt_present_status
yt_present_lottery_rewind(int row, int column,
    struct yt_present_state *state, struct yt_present_result *result)
{
	static const uint8_t backspace = '\b';
	enum yt_present_status status;

	memset(result, 0, sizeof(*result));
	if (state->sound.mode == 0.0f) {
		status = append_remote(result, YT_PRESENT_REMOTE_SEMI,
		    &backspace, 1U);
		if (status != YT_PRESENT_OK)
			return status;
	}
	return append_locate(result, row, column, -1, 0, 0);
}

static enum yt_present_status
radio_column(int line_number, size_t suffix, int *column)
{
	char number[64];
	int length;

	if (line_number < 1 || line_number > 21 || column == NULL
	    || suffix > (size_t)INT_MAX)
		return YT_PRESENT_RANGE;
	length = qb_str_single(number, sizeof(number), (float)line_number);
	if (length < 0 || suffix > (size_t)(INT_MAX - length))
		return YT_PRESENT_RANGE;
	*column = length + (int)suffix;
	return YT_PRESENT_OK;
}

enum yt_present_status
yt_present_radio_backspace(int line_number, size_t shortened_length,
    struct yt_present_state *state, struct yt_present_result *result)
{
	static const uint8_t remote[] = {'\b', ' ', '\b'};
	static const uint8_t space = ' ';
	enum yt_present_status status;
	int column;

	if (state == NULL || result == NULL || shortened_length > 74U)
		return YT_PRESENT_RANGE;
	memset(result, 0, sizeof(*result));
	status = radio_column(line_number, shortened_length + 2U, &column);
	if (status != YT_PRESENT_OK)
		return status;
	if (state->sound.mode == 0.0f) {
		status = append_remote(result, YT_PRESENT_REMOTE_SEMI,
		    remote, sizeof(remote));
		if (status != YT_PRESENT_OK)
			return status;
	}
	status = append_locate(result, -1, column, -1, 0, 0);
	if (status != YT_PRESENT_OK)
		return status;
	status = append_local(result, YT_PRESENT_LOCAL_SEMI, &space, 1U, 0, 0);
	if (status != YT_PRESENT_OK)
		return status;
	return append_locate(result, -1, column, -1, 0, 0);
}

enum yt_present_status
yt_present_radio_wrap_cleanup(int line_number, size_t wrap_marker,
    struct yt_present_state *state, struct yt_present_result *result)
{
	uint8_t remote[149];
	uint8_t spaces[74];
	enum yt_present_status status;
	size_t erased;
	int column;

	if (state == NULL || result == NULL || wrap_marker < 1U
	    || wrap_marker > 75U)
		return YT_PRESENT_RANGE;
	memset(result, 0, sizeof(*result));
	erased = 75U - wrap_marker;
	status = radio_column(line_number, wrap_marker + 1U, &column);
	if (status != YT_PRESENT_OK)
		return status;
	status = append_locate(result, -1, column, -1, 0, 0);
	if (status != YT_PRESENT_OK)
		return status;
	memset(spaces, ' ', erased);
	status = append_local(result, YT_PRESENT_LOCAL_SEMI,
	    spaces, erased, 0, 0);
	if (status != YT_PRESENT_OK)
		return status;
	if (state->sound.mode != 1.0f) {
		memset(remote, '\b', erased);
		memset(remote + erased, ' ', erased);
		remote[erased * 2U] = '\r';
		status = append_remote(result, YT_PRESENT_REMOTE_SEMI,
		    remote, erased * 2U + 1U);
		if (status != YT_PRESENT_OK)
			return status;
	}
	return YT_PRESENT_OK;
}

static enum yt_present_status
format_remaining(float deadline, float timer,
    struct yt_present_time_state *time)
{
	char minutes[64];
	char seconds[64];
	float deadline_minutes = (float)(deadline / 60.0f);
	float timer_minutes = (float)(timer / 60.0f);
	float remaining = (float)(deadline_minutes - timer_minutes);
	float whole = floorf(remaining);
	float fraction = (float)(remaining - whole);
	float seconds_value = floorf((float)(fraction * 60.0f));
	int minute_length;
	int second_length;
	size_t length;

	time->remaining_minutes = remaining;
	minute_length = qb_str_single(minutes, sizeof(minutes), whole);
	second_length = qb_str_single(seconds, sizeof(seconds), seconds_value);

	if (minute_length < 0 || second_length < 2)
		return YT_PRESENT_OVERFLOW;
	length = (size_t)minute_length + 1U;
	if (second_length == 2)
		++length;
	length += (size_t)second_length - 1U + 2U;
	if (length > sizeof(time->text)
	    || (size_t)second_length - 1U > sizeof(time->seconds_text))
		return YT_PRESENT_CAPACITY;
	memcpy(time->text, minutes, (size_t)minute_length);
	time->text[minute_length] = ':';
	time->text_length = (size_t)minute_length + 1U;
	if (second_length == 2)
		time->text[time->text_length++] = '0';
	memcpy(time->text + time->text_length, seconds + 1,
	    (size_t)second_length - 1U);
	time->text_length += (size_t)second_length - 1U;
	time->text[time->text_length++] = ' ';
	time->text[time->text_length++] = ' ';
	memcpy(time->seconds_text, seconds + 1,
	    (size_t)second_length - 1U);
	time->seconds_length = (size_t)second_length - 1U;
	return YT_PRESENT_OK;
}

enum yt_present_status
yt_present_format_remaining_seconds(struct yt_present_time_state *time,
    float remaining_seconds)
{
	if (time == NULL || !isfinite(remaining_seconds))
		return YT_PRESENT_OVERFLOW;
	if (remaining_seconds < 0.0f)
		remaining_seconds = 0.0f;
	return format_remaining(remaining_seconds, 0.0f, time);
}

static enum yt_present_status
low_time_value(const uint8_t *text, size_t length, double old_value,
    uint8_t narrowed[4], bool *changed)
{
	struct qb_val_result parsed;
	enum qb_mbf_status status;

	*changed = false;
	if (length > YT_PRESENT_EVENT_DATA
	    || (text == NULL && length != 0U))
		return YT_PRESENT_CAPACITY;
	parsed = qb_val_n(text, length);
	if (parsed.overflow)
		return YT_PRESENT_OVERFLOW;
	if (old_value == parsed.value)
		return YT_PRESENT_OK;
	status = qb_mbf32_from_mbf64_raw(parsed.mbf, narrowed);
	if (status == QB_MBF_OVERFLOW || status == QB_MBF_DOMAIN)
		return YT_PRESENT_OVERFLOW;
	*changed = true;
	return YT_PRESENT_OK;
}

static enum yt_present_status
low_time_warning(const uint8_t *text, size_t length, float remembered,
    struct yt_present_state *state, struct yt_present_result *result,
    bool *warned)
{
	static const uint8_t label[] = "Time Left:";
	static const uint8_t bell = '\a';
	uint8_t warning[YT_PRESENT_EVENT_DATA];
	enum yt_present_status status;

	if (remembered >= 6.0f)
		return YT_PRESENT_OK;
	status = emit_line(NULL, 0, state, result);
	if (status != YT_PRESENT_OK)
		return status;
	state->foreground = 5.0f;
	yt_present_set_blink(state, 1.0f);
	if (state->sound.mode != 0.0f)
		status = append_beep(result);
	else
		status = append_remote(result, YT_PRESENT_REMOTE_SEMI, &bell, 1);
	if (status != YT_PRESENT_OK)
		return status;
	if (length > sizeof(warning) - (sizeof(label) - 1U))
		return YT_PRESENT_CAPACITY;
	memcpy(warning, label, sizeof(label) - 1U);
	if (length != 0)
		memcpy(warning + sizeof(label) - 1U, text, length);
	yt_present_set_bold(state, 1.0f);
	status = emit_line(warning, sizeof(label) - 1U + length, state,
	    result);
	if (status != YT_PRESENT_OK)
		return status;
	status = emit_line(NULL, 0, state, result);
	if (status == YT_PRESENT_OK)
		*warned = true;
	return status;
}

enum yt_present_status
yt_present_low_time(const uint8_t *text, size_t length, float *remembered,
    struct yt_present_state *state, struct yt_present_result *result,
    bool *warned)
{
	uint8_t narrowed[4];
	bool changed;
	enum yt_present_status status;

	memset(result, 0, sizeof(*result));
	*warned = false;
	status = low_time_value(text, length, (double)*remembered, narrowed,
	    &changed);
	if (status != YT_PRESENT_OK || !changed)
		return status;
	*remembered = qb_mbf32_decode(narrowed);
	return low_time_warning(text, length, *remembered, state, result,
	    warned);
}

uint8_t
yt_present_pc_attribute(int foreground, int background)
{
	unsigned attribute = (unsigned)foreground & 0x0fU;

	attribute |= ((unsigned)background & 0x07U) << 4;
	attribute |= ((unsigned)foreground & 0x10U) << 3;
	return (uint8_t)attribute;
}
