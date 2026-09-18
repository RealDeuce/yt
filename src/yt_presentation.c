#include "yt_presentation.h"

#include "qb.h"

#include <limits.h>
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
append_locate(struct yt_present_result *result, int row, int column)
{
	struct yt_present_event *event;
	enum yt_present_status status = append_event(result,
	    YT_PRESENT_LOCAL_LOCATE, NULL, 0, 0, 0);

	if (status != YT_PRESENT_OK)
		return status;
	event = &result->events[result->event_count - 1U];
	event->row = row;
	event->column = column;
	return YT_PRESENT_OK;
}

static enum yt_present_status
append_beep(struct yt_present_result *result)
{
	return append_event(result, YT_PRESENT_LOCAL_BEEP, NULL, 0, 0, 0);
}

static enum yt_present_status
color_digit(int value, uint8_t *digit)
{
	if (value < 0 || value > 7)
		return YT_PRESENT_RANGE;
	*digit = (uint8_t)('0' + value);
	return YT_PRESENT_OK;
}

static enum yt_present_status
stage_color_cache(struct yt_present_state *state,
    struct yt_present_result *result, int foreground, int background)
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
	static const int standard[8] = {0, 4, 2, 6, 1, 5, 3, 7};
	uint8_t sequence[32];
	size_t length = 0;
	int background = state->background;
	bool bold = state->bold;
	bool blink = state->blink;
	int cached_foreground = state->cached_foreground;
	int cached_background = state->cached_background;
	int local_foreground;
	int local_background;
	enum yt_present_status status;

	if (state->foreground == background) {
		state->foreground = 3;
		state->background = 0;
		background = 0;
	}
	if (state->foreground < 0 || state->foreground >= 8
	    || background < 0 || background >= 8)
		return YT_PRESENT_RANGE;
	local_foreground = standard[state->foreground]
	    + (bold ? 8 : 0) + (blink ? 16 : 0);
	local_background = standard[background];
	status = append_local(result, YT_PRESENT_LOCAL_COLOR, NULL, 0,
	    local_foreground, local_background);
	if (status != YT_PRESENT_OK)
		return status;
	if (!state->sound.local_mode) {
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
		if (blink) {
			memcpy(sequence + length, ";5", 2);
			length += 2;
		}
		if (bold) {
			memcpy(sequence + length, ";1", 2);
			length += 2;
		}
		sequence[length++] = 'm';
		if (bold || blink) {
			status = append_remote(result, YT_PRESENT_REMOTE_SEMI,
			    sequence, length);
			if (status != YT_PRESENT_OK)
				return status;
			status = stage_color_cache(state, result, 0, 0);
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
	state->bold = false;
	state->blink = false;
	return YT_PRESENT_OK;
}

static enum yt_present_status
prepare_color(struct yt_present_state *state,
    struct yt_present_result *result)
{
	if (!state->sound.ansi)
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
	if (state->sound.local_output) {
		status = append_local(result, YT_PRESENT_LOCAL_SEMI, text,
		    length, 0, 0);
		if (status != YT_PRESENT_OK)
			return status;
	}
	if (!state->sound.local_mode)
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
	if (state->sound.local_output) {
		status = append_local(result, YT_PRESENT_LOCAL_LINE, text,
		    length, 0, 0);
		if (status != YT_PRESENT_OK)
			return status;
	}
	if (!state->sound.local_mode) {
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
	state->bold = true;
	return emit_line(text, length, state, result);
}

enum yt_present_status
yt_present_bold_character(const uint8_t *text, size_t length,
    struct yt_present_state *state, struct yt_present_result *result)
{
	memset(result, 0, sizeof(*result));
	if (state->sound.ansi)
		state->bold = true;
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
	if (state->sound.local_output) {
		status = append_local(result, YT_PRESENT_LOCAL_SEMI, text,
		    length, 0, 0);
		if (status != YT_PRESENT_OK)
			return status;
	}
	if (!state->sound.local_mode)
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
		if (!state->sound.local_mode) {
			status = append_remote(result, YT_PRESENT_REMOTE_LINE,
			    &line_feed, 1);
			if (status != YT_PRESENT_OK)
				return status;
		}
		if (state->sound.local_output) {
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
	if (state->sound.local_output) {
		status = append_local(result, YT_PRESENT_LOCAL_SEMI, local,
		    local_length, 0, 0);
		if (status != YT_PRESENT_OK)
			return status;
	}
	if (!state->sound.local_mode)
		return append_remote(result, YT_PRESENT_REMOTE_SEMI, remote,
		    remote_length);
	return YT_PRESENT_OK;
}

enum yt_present_status
yt_present_local_line(const uint8_t *text, size_t length,
    struct yt_present_state *state, struct yt_present_result *result)
{
	memset(result, 0, sizeof(*result));
	if (state->sound.local_output)
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
yt_present_right_aligned(const uint8_t *text, size_t length, uint8_t width,
    struct yt_present_state *state, struct yt_present_result *result)
{
	uint8_t rendered[YT_PRESENT_EVENT_DATA];
	size_t rendered_length;
	size_t count;

	memset(result, 0, sizeof(*result));
	if (length < (size_t)width) {
		count = (size_t)width - length;
		if (length > sizeof(rendered)
		    || count > sizeof(rendered) - length)
			return YT_PRESENT_CAPACITY;
		memset(rendered, ' ', count);
		if (length != 0)
			memcpy(rendered + count, text, length);
		rendered_length = count + length;
	}
	else {
		rendered_length = width;
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
    uint8_t width, struct yt_present_state *state,
    struct yt_present_result *result)
{
	size_t space_count = width;
	size_t left_count = width;
	size_t concatenated;

	memset(result, 0, sizeof(*result));
	if (*length > capacity)
		return YT_PRESENT_CAPACITY;
	if (space_count > capacity - *length)
		return YT_PRESENT_CAPACITY;
	memset(text + *length, ' ', space_count);
	concatenated = *length + space_count;
	*length = concatenated;
	if (left_count < *length)
		*length = left_count;
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
	state->blink = true;
	if (state->sound.ansi)
		state->bold = true;
	status = emit_character(text, length, state, result);
	if (status != YT_PRESENT_OK)
		return status;
	state->background = 0.0f;
	status = emit_line(NULL, 0, state, result);
	if (status != YT_PRESENT_OK)
		return status;
	sound_status = yt_sound_dispatch(YT_SOUND_CUE_ATTENTION,
	    &state->sound, &sound);
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
yt_present_sound(enum yt_sound_cue cue, struct yt_present_state *state,
    struct yt_present_result *result)
{
	struct yt_sound_result sound;
	enum yt_sound_status sound_status;
	enum yt_present_status status;

	memset(result, 0, sizeof(*result));
	sound_status = yt_sound_dispatch(cue, &state->sound, &sound);
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
    struct yt_present_result *result, int *saved_foreground)
{
	static const uint8_t prompt[] = "*[ Press any Key ]*";

	memset(result, 0, sizeof(*result));
	*saved_foreground = state->foreground;
	state->foreground = 3;
	return yt_present_bold_character(prompt, sizeof(prompt) - 1U, state,
	    result);
}

enum yt_present_status
yt_present_press_cleanup(int saved_foreground,
    struct yt_present_state *state, struct yt_present_result *result)
{
	static const uint8_t carriage_return = '\r';
	static const uint8_t spaces[] = "                   ";
	enum yt_present_status status;

	memset(result, 0, sizeof(*result));
	status = append_locate(result, -1, 1);
	if (status != YT_PRESENT_OK)
		return status;
	if (!state->sound.local_mode) {
		status = append_remote(result, YT_PRESENT_REMOTE_SEMI,
		    &carriage_return, 1);
		if (status != YT_PRESENT_OK)
			return status;
	}
	status = emit_character(spaces, sizeof(spaces) - 1U, state, result);
	if (status != YT_PRESENT_OK)
		return status;
	if (!state->sound.local_mode) {
		status = append_remote(result, YT_PRESENT_REMOTE_SEMI,
		    &carriage_return, 1);
		if (status != YT_PRESENT_OK)
			return status;
	}
	status = append_locate(result, -1, 1);
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
	if (!state->sound.local_mode) {
		status = append_remote(result, YT_PRESENT_REMOTE_SEMI,
		    &backspace, 1U);
		if (status != YT_PRESENT_OK)
			return status;
	}
	return append_locate(result, row, column);
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
	if (!state->sound.local_mode) {
		status = append_remote(result, YT_PRESENT_REMOTE_SEMI,
		    remote, sizeof(remote));
		if (status != YT_PRESENT_OK)
			return status;
	}
	status = append_locate(result, -1, column);
	if (status != YT_PRESENT_OK)
		return status;
	status = append_local(result, YT_PRESENT_LOCAL_SEMI, &space, 1U, 0, 0);
	if (status != YT_PRESENT_OK)
		return status;
	return append_locate(result, -1, column);
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
	status = append_locate(result, -1, column);
	if (status != YT_PRESENT_OK)
		return status;
	memset(spaces, ' ', erased);
	status = append_local(result, YT_PRESENT_LOCAL_SEMI,
	    spaces, erased, 0, 0);
	if (status != YT_PRESENT_OK)
		return status;
	if (!state->sound.local_mode) {
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
	uint16_t whole = (uint16_t)floorf(remaining);
	float fraction = (float)(remaining - (float)whole);
	uint8_t seconds_value = (uint8_t)floorf((float)(fraction * 60.0f));
	int minute_length;
	int second_length;
	size_t length;

	minute_length = qb_str_single(minutes, sizeof(minutes), (float)whole);
	second_length = qb_str_single(seconds, sizeof(seconds),
	    (float)seconds_value);

	if (minute_length < 0 || second_length < 2)
		return YT_PRESENT_OVERFLOW;
	length = (size_t)minute_length + 1U;
	if (second_length == 2)
		++length;
	length += (size_t)second_length - 1U + 2U;
	if (length > sizeof(time->text))
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
low_time_value(const uint8_t *text, size_t length, uint16_t old_value,
    uint16_t *new_value, bool *changed)
{
	struct qb_val_result parsed;
	enum qb_mbf_status status;
	uint8_t narrowed[4];
	float decoded;

	*changed = false;
	if (length > YT_PRESENT_EVENT_DATA
	    || (text == NULL && length != 0U))
		return YT_PRESENT_CAPACITY;
	parsed = qb_val_n(text, length);
	if (parsed.overflow)
		return YT_PRESENT_OVERFLOW;
	if ((double)old_value == parsed.value)
		return YT_PRESENT_OK;
	status = qb_mbf32_from_mbf64_raw(parsed.mbf, narrowed);
	if (status == QB_MBF_OVERFLOW || status == QB_MBF_DOMAIN)
		return YT_PRESENT_OVERFLOW;
	decoded = qb_mbf32_decode(narrowed);
	if (decoded < 0.0f || decoded > (float)UINT16_MAX)
		return YT_PRESENT_RANGE;
	*new_value = (uint16_t)decoded;
	*changed = true;
	return YT_PRESENT_OK;
}

static enum yt_present_status
low_time_warning(const uint8_t *text, size_t length, uint16_t remembered,
    struct yt_present_state *state, struct yt_present_result *result,
    bool *warned)
{
	static const uint8_t label[] = "Time Left:";
	static const uint8_t bell = '\a';
	uint8_t warning[YT_PRESENT_EVENT_DATA];
	enum yt_present_status status;

	if (remembered >= 6U)
		return YT_PRESENT_OK;
	status = emit_line(NULL, 0, state, result);
	if (status != YT_PRESENT_OK)
		return status;
	state->foreground = 5.0f;
	state->blink = true;
	if (state->sound.local_mode)
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
	state->bold = true;
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
yt_present_low_time(const uint8_t *text, size_t length, uint16_t *remembered,
    struct yt_present_state *state, struct yt_present_result *result,
    bool *warned)
{
	uint16_t new_value = 0U;
	bool changed;
	enum yt_present_status status;

	memset(result, 0, sizeof(*result));
	*warned = false;
	status = low_time_value(text, length, *remembered, &new_value,
	    &changed);
	if (status != YT_PRESENT_OK || !changed)
		return status;
	*remembered = new_value;
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
