#include "yt_presentation.h"

#include "qb.h"

#include <math.h>
#include <string.h>

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
build_color(struct yt_present_state *state,
    struct yt_present_result *result)
{
	static const float standard[8] = {0, 4, 2, 6, 1, 5, 3, 7};
	uint8_t sequence[32];
	size_t length = 0;
	float bright = 0.0f;
	int foreground_index;
	int background_index;
	int local_foreground;
	int local_background;
	int forced_bold;
	int forced_blink;
	enum yt_present_status status;

	if (state->color_initialized == 0.0f) {
		memcpy(state->color_memory, standard, sizeof(standard));
		state->color_initialized = 1.0f;
	}
	if (state->foreground == state->background) {
		state->foreground = 3.0f;
		state->background = 0.0f;
	}
	if (state->bold == 1.0f)
		bright = 8.0f;
	if (state->blink == 1.0f)
		bright += 16.0f;
	status = convert(state, state->foreground, &foreground_index);
	if (status != YT_PRESENT_OK)
		return status;
	status = convert(state, state->background, &background_index);
	if (status != YT_PRESENT_OK)
		return status;
	if (foreground_index < 0 || foreground_index >= 8
	    || background_index < 0 || background_index >= 8)
		return YT_PRESENT_RANGE;
	status = convert(state,
	    (double)(float)(state->color_memory[foreground_index] + bright),
	    &local_foreground);
	if (status != YT_PRESENT_OK)
		return status;
	status = convert(state, state->color_memory[background_index],
	    &local_background);
	if (status != YT_PRESENT_OK)
		return status;
	status = append_local(result, YT_PRESENT_LOCAL_COLOR, NULL, 0,
	    local_foreground, local_background);
	if (status != YT_PRESENT_OK)
		return status;

	memcpy(sequence + length, "\x1b[0;3", 5);
	length += 5;
	status = color_digit(state->foreground, &sequence[length++]);
	if (status != YT_PRESENT_OK)
		return status;
	memcpy(sequence + length, ";4", 2);
	length += 2;
	status = color_digit(state->background, &sequence[length++]);
	if (status != YT_PRESENT_OK)
		return status;
	if (state->blink == 1.0f) {
		memcpy(sequence + length, ";5", 2);
		length += 2;
	}
	if (state->bold == 1.0f) {
		memcpy(sequence + length, ";1", 2);
		length += 2;
	}
	sequence[length++] = 'm';
	if (state->sound.mode == 0.0f) {
		status = convert(state, state->bold, &forced_bold);
		if (status != YT_PRESENT_OK)
			return status;
		status = convert(state, state->blink, &forced_blink);
		if (status != YT_PRESENT_OK)
			return status;
		if ((forced_bold | forced_blink) != 0) {
			status = append_remote(result, YT_PRESENT_REMOTE_SEMI,
			    sequence, length);
			state->cached_foreground = 0.0f;
			state->cached_background = 0.0f;
		}
		else if (state->foreground != state->cached_foreground
		    || state->background != state->cached_background) {
			status = append_remote(result, YT_PRESENT_REMOTE_SEMI,
			    sequence, length);
			state->cached_foreground = state->foreground;
			state->cached_background = state->background;
		}
		else
			status = YT_PRESENT_OK;
		if (status != YT_PRESENT_OK)
			return status;
	}
	state->bold = 0.0f;
	state->blink = 0.0f;
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
	state->bold = 1.0f;
	return emit_line(text, length, state, result);
}

enum yt_present_status
yt_present_bold_character(const uint8_t *text, size_t length,
    struct yt_present_state *state, struct yt_present_result *result)
{
	memset(result, 0, sizeof(*result));
	if (state->sound.ansi != 0.0f)
		state->bold = 1.0f;
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
	state->background = 1.0f;
	state->blink = 1.0f;
	if (state->sound.ansi != 0.0f)
		state->bold = 1.0f;
	status = emit_character(text, length, state, result);
	if (status != YT_PRESENT_OK)
		return status;
	state->background = 0.0f;
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

enum yt_present_status
yt_present_sound_toggle(struct yt_present_state *state,
    struct yt_present_result *result)
{
	struct yt_sound_result sound;
	enum yt_sound_status sound_status;
	enum yt_present_status status;

	memset(result, 0, sizeof(*result));
	sound_status = yt_sound_toggle(&state->sound, &sound);
	if (sound.line_length != 0) {
		status = emit_line(sound.line, sound.line_length, state, result);
		if (status != YT_PRESENT_OK)
			return status;
	}
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

static enum yt_present_status
next_timer(const float *reads, size_t count, size_t *used, float *value)
{
	if (*used >= count)
		return YT_PRESENT_TIMER_EXHAUSTED;
	*value = reads[(*used)++];
	return YT_PRESENT_OK;
}

static enum yt_present_status
parse_value(const uint8_t *text, size_t length, double *value)
{
	char buffer[YT_PRESENT_EVENT_DATA + 1U];
	struct qb_val_result parsed;

	if (length > YT_PRESENT_EVENT_DATA)
		return YT_PRESENT_CAPACITY;
	if (length != 0)
		memcpy(buffer, text, length);
	buffer[length] = '\0';
	parsed = qb_val(buffer);
	if (parsed.overflow)
		return YT_PRESENT_OVERFLOW;
	*value = parsed.value;
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
	int minute_length = qb_str_single(minutes, sizeof(minutes), whole);
	int second_length = qb_str_single(seconds, sizeof(seconds), seconds_value);
	size_t length;

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
	time->remaining_minutes = remaining;
	return YT_PRESENT_OK;
}

enum yt_present_status
yt_present_refresh_time(struct yt_present_time_state *time,
    const float *timer_reads, size_t timer_count, size_t *timer_used,
    int cursor_row, int cursor_column, struct yt_present_state *state,
    struct yt_present_result *result, bool *updated)
{
	float timer;
	double prior_value;
	size_t used = 0;
	enum yt_present_status status;

	memset(result, 0, sizeof(*result));
	*updated = false;
	status = next_timer(timer_reads, timer_count, &used, &timer);
	if (status != YT_PRESENT_OK)
		goto done;
	if ((float)(time->deadline - timer) > 70000.0f) {
		time->deadline = (float)(time->deadline - 86400.0f);
		status = next_timer(timer_reads, timer_count, &used, &timer);
		if (status != YT_PRESENT_OK)
			goto done;
		time->next_refresh = (float)(timer + 1.0f);
	}
	status = next_timer(timer_reads, timer_count, &used, &timer);
	if (status != YT_PRESENT_OK)
		goto done;
	if (timer < time->next_refresh) {
		status = YT_PRESENT_OK;
		goto done;
	}
	status = next_timer(timer_reads, timer_count, &used, &timer);
	if (status != YT_PRESENT_OK)
		goto done;
	time->next_refresh = (float)(timer + 1.0f);
	time->saved_row = cursor_row;
	time->saved_column = cursor_column;
	status = append_locate(result, 25, 71, -1, 0, 0);
	if (status != YT_PRESENT_OK)
		goto done;
	status = append_local(result, YT_PRESENT_LOCAL_COLOR, NULL, 0,
	    11, 1);
	if (status != YT_PRESENT_OK)
		goto done;
	status = parse_value(time->text, time->text_length, &prior_value);
	if (status != YT_PRESENT_OK)
		goto done;
	if (prior_value < 5.0) {
		status = append_local(result, YT_PRESENT_LOCAL_COLOR, NULL, 0,
		    12, 1);
		if (status != YT_PRESENT_OK)
			goto done;
	}
	status = next_timer(timer_reads, timer_count, &used, &timer);
	if (status != YT_PRESENT_OK)
		goto done;
	status = format_remaining(time->deadline, timer, time);
	if (status != YT_PRESENT_OK)
		goto done;
	if (state->sound.snoop != 0.0f) {
		status = append_local(result, YT_PRESENT_LOCAL_SEMI,
		    time->text, time->text_length, 0, 0);
		if (status != YT_PRESENT_OK)
			goto done;
	}
	status = append_locate(result, cursor_row, cursor_column, 1, 1, 16);
	if (status != YT_PRESENT_OK)
		goto done;
	status = append_local(result, YT_PRESENT_LOCAL_COLOR, NULL, 0, 7, 0);
	if (status == YT_PRESENT_OK)
		*updated = true;
done:
	*timer_used = used;
	return status;
}

enum yt_present_status
yt_present_low_time(const uint8_t *text, size_t length, float *remembered,
    struct yt_present_state *state, struct yt_present_result *result,
    bool *warned)
{
	static const uint8_t label[] = "Time Left:";
	static const uint8_t bell = '\a';
	uint8_t warning[YT_PRESENT_EVENT_DATA];
	double parsed;
	enum yt_present_status status;

	memset(result, 0, sizeof(*result));
	*warned = false;
	status = parse_value(text, length, &parsed);
	if (status != YT_PRESENT_OK)
		return status;
	if ((double)*remembered == parsed)
		return YT_PRESENT_OK;
	*remembered = (float)parsed;
	if (*remembered >= 6.0f)
		return YT_PRESENT_OK;
	status = emit_line(NULL, 0, state, result);
	if (status != YT_PRESENT_OK)
		return status;
	state->foreground = 5.0f;
	state->blink = 1.0f;
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
	state->bold = 1.0f;
	status = emit_line(warning, sizeof(label) - 1U + length, state,
	    result);
	if (status != YT_PRESENT_OK)
		return status;
	status = emit_line(NULL, 0, state, result);
	if (status == YT_PRESENT_OK)
		*warned = true;
	return status;
}

void
yt_present_replay(const struct yt_present_result *result,
    const struct yt_present_sink *sink)
{
	size_t index;

	for (index = 0; index < result->event_count; ++index) {
		const struct yt_present_event *event = &result->events[index];

		switch (event->operation) {
		case YT_PRESENT_REMOTE_LINE:
		case YT_PRESENT_REMOTE_SEMI:
			if (sink->remote != NULL)
				sink->remote(sink->context, event->data,
				    event->length,
				    event->operation == YT_PRESENT_REMOTE_LINE);
			break;
		case YT_PRESENT_LOCAL_COLOR:
			if (sink->local_color != NULL)
				sink->local_color(sink->context, event->foreground,
				    event->background);
			break;
		case YT_PRESENT_LOCAL_LINE:
		case YT_PRESENT_LOCAL_SEMI:
			if (sink->local_text != NULL)
				sink->local_text(sink->context, event->data,
				    event->length,
				    event->operation == YT_PRESENT_LOCAL_LINE);
			break;
		case YT_PRESENT_LOCAL_PLAY:
			/* Authorized logical-event/no-host-audio departure. */
			break;
		case YT_PRESENT_LOCAL_LOCATE:
			if (sink->local_locate != NULL)
				sink->local_locate(sink->context, event->row,
				    event->column, event->cursor_visible,
				    event->cursor_start, event->cursor_stop);
			break;
		case YT_PRESENT_LOCAL_BEEP:
			if (sink->local_beep != NULL)
				sink->local_beep(sink->context);
			break;
		}
	}
}

uint8_t
yt_present_pc_attribute(int foreground, int background)
{
	unsigned attribute = (unsigned)foreground & 0x0fU;

	attribute |= ((unsigned)background & 0x07U) << 4;
	attribute |= ((unsigned)foreground & 0x10U) << 3;
	return (uint8_t)attribute;
}
