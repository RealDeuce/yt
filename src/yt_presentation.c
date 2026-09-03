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
append_clear(struct yt_present_result *result)
{
	return append_event(result, YT_PRESENT_LOCAL_CLEAR, NULL, 0, 0, 0);
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
yt_present_forced_local_line(const uint8_t *text, size_t length,
    struct yt_present_result *result)
{
	memset(result, 0, sizeof(*result));
	return append_local(result, YT_PRESENT_LOCAL_LINE, text, length, 0, 0);
}

enum yt_present_status
yt_present_serial_startup_missing_command(struct yt_present_result *result)
{
	static const char diagnostic[] = "Command line missing! Aborting!";
	static const char usage[] =
	    "BBS usage: YT.EXE C:\\BBS\\DORINFO1.DEF";
	enum yt_present_status status;

	if (result == NULL)
		return YT_PRESENT_CAPACITY;
	memset(result, 0, sizeof(*result));
	status = append_local(result, YT_PRESENT_LOCAL_LINE, NULL, 0U, 0, 0);
	if (status != YT_PRESENT_OK)
		return status;
	status = append_local(result, YT_PRESENT_LOCAL_LINE, diagnostic,
	    sizeof(diagnostic) - 1U, 0, 0);
	if (status != YT_PRESENT_OK)
		return status;
	status = append_local(result, YT_PRESENT_LOCAL_LINE, NULL, 0U, 0, 0);
	if (status != YT_PRESENT_OK)
		return status;
	return append_local(result, YT_PRESENT_LOCAL_LINE, usage,
	    sizeof(usage) - 1U, 0, 0);
}

enum yt_present_status
yt_present_serial_startup_status(int port, float detected_baud,
    struct yt_present_result *result)
{
	static const char local[] = "Local Console Mode";
	uint8_t rendered[YT_PRESENT_EVENT_DATA];
	size_t length = 0U;
	char number[64];
	int number_length;

	if (result == NULL)
		return YT_PRESENT_CAPACITY;
	memset(result, 0, sizeof(*result));
	if (port < 1 || port > 4)
		return append_local(result, YT_PRESENT_LOCAL_LINE, local,
		    sizeof(local) - 1U, 0, 0);
	memcpy(rendered, "Opening COM port", strlen("Opening COM port"));
	length = strlen("Opening COM port");
	number_length = qb_print_single(number, sizeof(number), (float)port);
	if (number_length < 0
	    || (size_t)number_length > sizeof(rendered) - length)
		return YT_PRESENT_OVERFLOW;
	memcpy(rendered + length, number, (size_t)number_length);
	length += (size_t)number_length;
	if (sizeof("at") - 1U > sizeof(rendered) - length)
		return YT_PRESENT_CAPACITY;
	memcpy(rendered + length, "at", sizeof("at") - 1U);
	length += sizeof("at") - 1U;
	number_length = qb_print_single(number, sizeof(number), detected_baud);
	if (number_length < 0
	    || (size_t)number_length > sizeof(rendered) - length)
		return YT_PRESENT_OVERFLOW;
	memcpy(rendered + length, number, (size_t)number_length);
	length += (size_t)number_length;
	if (sizeof("baud") - 1U > sizeof(rendered) - length)
		return YT_PRESENT_CAPACITY;
	memcpy(rendered + length, "baud", sizeof("baud") - 1U);
	length += sizeof("baud") - 1U;
	return append_local(result, YT_PRESENT_LOCAL_LINE, rendered, length,
	    0, 0);
}

enum yt_present_status
yt_present_carrier_drop(struct yt_present_state *state,
    struct yt_present_result *result)
{
	static const uint8_t notice[] =
	    "(**CARRIER DROPPED**) Returning to bbs!";

	if (state == NULL || result == NULL)
		return YT_PRESENT_CAPACITY;
	return yt_present_local_line(notice, sizeof(notice) - 1U, state,
	    result);
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
yt_present_sound_toggle_process(const uint8_t mode[4],
    uint8_t user_sound[4], uint8_t local_sound[4],
    struct yt_present_state *state, struct yt_present_result *result)
{
	struct yt_sound_result sound;
	enum yt_sound_status sound_status;

	sound_status = yt_sound_toggle_process(&state->sound, mode, user_sound,
	    local_sound, &sound);
	return present_sound_toggle_result(state, result, sound_status, &sound);
}

static enum yt_present_status
present_sysop_sound_toggle_result(bool enabled,
    enum yt_sound_status sound_status, struct yt_present_result *result)
{
	static const uint8_t prefix[] = "Sound ";
	static const uint8_t on[] = "ON";
	static const uint8_t off[] = "OFF";
	const uint8_t *suffix;
	size_t suffix_length;
	enum yt_present_status status;

	memset(result, 0, sizeof(*result));
	if (sound_status != YT_SOUND_OK)
		return YT_PRESENT_SOUND_ERROR;
	status = append_local(result, YT_PRESENT_LOCAL_LINE, NULL, 0, 0, 0);
	if (status != YT_PRESENT_OK)
		return status;
	status = append_local(result, YT_PRESENT_LOCAL_SEMI, prefix,
	    sizeof(prefix) - 1U, 0, 0);
	if (status != YT_PRESENT_OK)
		return status;
	suffix = enabled ? on : off;
	suffix_length = enabled ? sizeof(on) - 1U : sizeof(off) - 1U;
	return append_local(result, YT_PRESENT_LOCAL_LINE, suffix,
	    suffix_length, 0, 0);
}

enum yt_present_status
yt_present_sysop_sound_toggle(struct yt_present_state *state,
    struct yt_present_result *result)
{
	bool enabled = false;
	enum yt_sound_status status;

	status = yt_sound_sysop_toggle(&state->sound, &enabled);
	return present_sysop_sound_toggle_result(enabled, status, result);
}

enum yt_present_status
yt_present_sysop_sound_toggle_process(const uint8_t mode[4],
    uint8_t local_sound[4], uint8_t user_sound[4],
    struct yt_present_state *state, struct yt_present_result *result)
{
	bool enabled = false;
	enum yt_sound_status status;

	status = yt_sound_sysop_toggle_process(&state->sound, mode,
	    local_sound, user_sound, &enabled);
	return present_sysop_sound_toggle_result(enabled, status, result);
}

static enum yt_present_status status_row_append(const uint8_t *real_name,
    size_t real_name_length, const uint8_t *alias, size_t alias_length,
    struct yt_present_state *state, struct yt_present_result *result);

static enum yt_present_status
present_sysop_snoop_toggle(const uint8_t *real_name,
    size_t real_name_length, const uint8_t *alias, size_t alias_length,
    const uint8_t mode[4], uint8_t snoop[4],
    struct yt_present_state *state, struct yt_present_result *result)
{
	static const uint8_t notice[] = "SNOOP ON";
	bool returned_early = false;
	bool enabled = false;
	enum yt_present_status status;

	if (state == NULL || result == NULL)
		return YT_PRESENT_CAPACITY;
	if (mode != NULL && snoop != NULL) {
		state->sound.mode = qb_mbf32_decode(mode);
		state->sound.snoop = qb_mbf32_decode(snoop);
	}
	memset(result, 0, sizeof(*result));
	if (yt_sound_sysop_snoop_toggle(&state->sound, &returned_early,
	    &enabled) != YT_SOUND_OK)
		return YT_PRESENT_SOUND_ERROR;
	if (returned_early)
		return YT_PRESENT_OK;
	if (snoop != NULL
	    && qb_mbf32_encode(state->sound.snoop, snoop) != QB_MBF_OK)
		return YT_PRESENT_OVERFLOW;
	if (!enabled) {
		status = append_locate(result, -1, -1, 0, 0, 0);
		return status == YT_PRESENT_OK ? append_clear(result) : status;
	}
	status = status_row_append(real_name, real_name_length, alias,
	    alias_length, state, result);
	if (status != YT_PRESENT_OK)
		return status;
	status = append_locate(result, 24, 1, -1, 0, 0);
	return status == YT_PRESENT_OK
	    ? append_local(result, YT_PRESENT_LOCAL_LINE, notice,
	    sizeof(notice) - 1U, 0, 0) : status;
}

enum yt_present_status
yt_present_sysop_snoop_toggle(const uint8_t *real_name,
    size_t real_name_length, const uint8_t *alias, size_t alias_length,
    struct yt_present_state *state, struct yt_present_result *result)
{
	return present_sysop_snoop_toggle(real_name, real_name_length, alias,
	    alias_length, NULL, NULL, state, result);
}

enum yt_present_status
yt_present_sysop_snoop_toggle_process(const uint8_t *real_name,
    size_t real_name_length, const uint8_t *alias, size_t alias_length,
    const uint8_t mode[4], uint8_t snoop[4],
    struct yt_present_state *state, struct yt_present_result *result)
{
	if (mode == NULL || snoop == NULL)
		return YT_PRESENT_CAPACITY;
	return present_sysop_snoop_toggle(real_name, real_name_length, alias,
	    alias_length, mode, snoop, state, result);
}

static enum yt_present_status
sysop_single(double value, float *single)
{
	uint8_t raw[4];
	enum qb_mbf_status status;

	status = qb_mbf32_encode((float)value, raw);
	if (status == QB_MBF_OVERFLOW)
		return YT_PRESENT_OVERFLOW;
	*single = qb_mbf32_decode(raw);
	return YT_PRESENT_OK;
}

enum yt_present_status
yt_present_sysop_time_prompt(float deadline, float timer,
    struct yt_present_result *result)
{
	static const char prefix[] =
	    "SysOp, how many minutes till user is forced off [";
	char current[64];
	uint8_t prompt[180];
	float integral_timer;
	float remaining;
	float minutes;
	size_t length = 0;
	int number_length;
	enum yt_present_status status;

	memset(result, 0, sizeof(*result));
	integral_timer = floorf(timer);
	if (sysop_single((double)deadline - integral_timer, &remaining)
	    != YT_PRESENT_OK
	    || sysop_single((double)remaining / 60.0, &minutes)
	    != YT_PRESENT_OK)
		return YT_PRESENT_OVERFLOW;
	minutes = floorf(minutes);
	number_length = qb_print_single(current, sizeof(current), minutes);
	if (number_length < 0)
		return YT_PRESENT_OVERFLOW;
	memcpy(prompt + length, prefix, sizeof(prefix) - 1U);
	length += sizeof(prefix) - 1U;
	memcpy(prompt + length, current, (size_t)number_length);
	length += (size_t)number_length;
	memcpy(prompt + length, "] ? ", 4U);
	length += 4U;
	status = append_local(result, YT_PRESENT_LOCAL_LINE, NULL, 0, 0, 0);
	return status == YT_PRESENT_OK
	    ? append_local(result, YT_PRESENT_LOCAL_SEMI, prompt, length, 0, 0)
	    : status;
}

enum yt_present_status
yt_present_sysop_time_replace(const uint8_t *entered, size_t entered_length,
    float commit_timer, float *deadline, float *minutes, bool *changed)
{
	char buffer[YT_PRESENT_EVENT_DATA + 1U];
	struct qb_val_result parsed;
	float selected;
	float scaled;
	float replacement;
	enum yt_present_status status;

	if (deadline == NULL || minutes == NULL || changed == NULL
	    || entered_length > YT_PRESENT_EVENT_DATA
	    || (entered == NULL && entered_length != 0U))
		return YT_PRESENT_CAPACITY;
	*changed = false;
	*minutes = 0.0f;
	if (entered_length == 0U)
		return YT_PRESENT_OK;
	memcpy(buffer, entered, entered_length);
	buffer[entered_length] = '\0';
	parsed = qb_val(buffer);
	if (parsed.overflow)
		return YT_PRESENT_OVERFLOW;
	status = sysop_single(parsed.valid ? parsed.value : 0.0, &selected);
	if (status != YT_PRESENT_OK)
		return status;
	if (selected > 90.0f)
		selected = 90.0f;
	status = sysop_single((double)selected * 60.0, &scaled);
	if (status != YT_PRESENT_OK)
		return status;
	status = sysop_single((double)floorf(commit_timer) + scaled,
	    &replacement);
	if (status != YT_PRESENT_OK)
		return status;
	*deadline = replacement;
	*minutes = selected;
	*changed = true;
	return YT_PRESENT_OK;
}

enum yt_present_status
yt_present_sysop_time_replace_process(const uint8_t *entered,
    size_t entered_length, float commit_timer, uint8_t deadline[4],
    float *minutes, bool *changed)
{
	float value;
	enum yt_present_status status;

	if (deadline == NULL)
		return YT_PRESENT_CAPACITY;
	value = qb_mbf32_decode(deadline);
	status = yt_present_sysop_time_replace(entered, entered_length,
	    commit_timer, &value, minutes, changed);
	if (status != YT_PRESENT_OK || !*changed)
		return status;
	return qb_mbf32_encode(value, deadline) == QB_MBF_OK
	    ? YT_PRESENT_OK : YT_PRESENT_OVERFLOW;
}

enum yt_present_status
yt_present_sysop_time_handler(float prompt_timer, const uint8_t *entered,
    size_t entered_length, float commit_timer, float *deadline,
    float *minutes, bool *changed, struct yt_present_result *prompt,
    yt_present_sysop_replay_fn replay, void *replay_context)
{
	enum yt_present_status status;

	if (deadline == NULL || prompt == NULL || replay == NULL)
		return YT_PRESENT_CAPACITY;
	status = yt_present_sysop_time_prompt(*deadline, prompt_timer, prompt);
	if (status != YT_PRESENT_OK)
		return status;
	status = yt_present_sysop_time_replace(entered, entered_length,
	    commit_timer, deadline, minutes, changed);
	return status == YT_PRESENT_OK ? replay(replay_context) : status;
}

enum yt_present_status
yt_present_sysop_time_handler_process(float prompt_timer,
    const uint8_t *entered, size_t entered_length, float commit_timer,
    uint8_t deadline[4], float *minutes, bool *changed,
    struct yt_present_result *prompt, yt_present_sysop_replay_fn replay,
    void *replay_context)
{
	enum yt_present_status status;

	if (deadline == NULL || prompt == NULL || replay == NULL)
		return YT_PRESENT_CAPACITY;
	status = yt_present_sysop_time_prompt(qb_mbf32_decode(deadline),
	    prompt_timer, prompt);
	if (status != YT_PRESENT_OK)
		return status;
	status = yt_present_sysop_time_replace_process(entered, entered_length,
	    commit_timer, deadline, minutes, changed);
	return status == YT_PRESENT_OK ? replay(replay_context) : status;
}

enum yt_present_status
yt_present_sysop_chat_header(const uint8_t *sysop, size_t sysop_length,
    int local_background, struct yt_present_result *result)
{
	static const uint8_t suffix[] = " - Hit ESC to exit chat mode";
	uint8_t line[YT_PRESENT_EVENT_DATA];
	enum yt_present_status status;

	if (result == NULL || sysop_length > sizeof(line) - (sizeof(suffix) - 1U)
	    || (sysop == NULL && sysop_length != 0U))
		return YT_PRESENT_CAPACITY;
	memset(result, 0, sizeof(*result));
	if (sysop_length != 0U)
		memcpy(line, sysop, sysop_length);
	memcpy(line + sysop_length, suffix, sizeof(suffix) - 1U);
	status = append_local(result, YT_PRESENT_LOCAL_COLOR, NULL, 0U,
	    30, local_background);
	if (status != YT_PRESENT_OK)
		return status;
	status = append_local(result, YT_PRESENT_LOCAL_LINE, NULL, 0U, 0, 0);
	if (status != YT_PRESENT_OK)
		return status;
	status = append_local(result, YT_PRESENT_LOCAL_LINE, NULL, 0U, 0, 0);
	return status == YT_PRESENT_OK
	    ? append_local(result, YT_PRESENT_LOCAL_LINE, line,
	    sysop_length + sizeof(suffix) - 1U, 0, 0) : status;
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
yt_present_refresh_time_process(struct yt_present_time_state *time,
    uint8_t deadline[4], uint8_t next_refresh[4], const float *timer_reads,
    size_t timer_count, size_t *timer_used, int cursor_row, int cursor_column,
    struct yt_present_state *state, struct yt_present_result *result,
    bool *updated)
{
	uint8_t raw[4];
	float initial_deadline = qb_mbf32_decode(deadline);
	float initial_next_refresh = qb_mbf32_decode(next_refresh);
	enum yt_present_status status;

	time->deadline = initial_deadline;
	time->next_refresh = initial_next_refresh;
	status = yt_present_refresh_time(time, timer_reads, timer_count,
	    timer_used, cursor_row, cursor_column, state, result, updated);
	if (time->deadline != initial_deadline) {
		if (qb_mbf32_encode(time->deadline, raw) == QB_MBF_OVERFLOW)
			return YT_PRESENT_OVERFLOW;
		memcpy(deadline, raw, sizeof(raw));
	}
	if (time->next_refresh != initial_next_refresh) {
		if (qb_mbf32_encode(time->next_refresh, raw) == QB_MBF_OVERFLOW)
			return YT_PRESENT_OVERFLOW;
		memcpy(next_refresh, raw, sizeof(raw));
	}
	return status;
}

enum yt_present_status
yt_present_opening_row(const uint8_t *text, size_t length, float mode,
    float snoop, struct yt_present_result *result)
{
	static const uint8_t line_feed = '\n';
	enum yt_present_status status;

	if (result == NULL || (text == NULL && length != 0U))
		return YT_PRESENT_CAPACITY;
	memset(result, 0, sizeof(*result));
	if (snoop != 0.0f) {
		status = append_local(result, YT_PRESENT_LOCAL_LINE, text, length,
		    0, 0);
		if (status != YT_PRESENT_OK)
			return status;
	}
	if (mode == 1.0f)
		return YT_PRESENT_OK;
	status = append_remote(result, YT_PRESENT_REMOTE_SEMI, text, length);
	if (status != YT_PRESENT_OK)
		return status;
	return append_remote(result, YT_PRESENT_REMOTE_LINE, &line_feed, 1U);
}

enum yt_present_status
yt_present_opening_cleanup(float mode, float snoop,
    struct yt_present_result *result)
{
	static const uint8_t reset[] = "\x1b[0m";
	enum yt_present_status status;

	if (result == NULL)
		return YT_PRESENT_CAPACITY;
	memset(result, 0, sizeof(*result));
	if (mode == 0.0f) {
		status = append_remote(result, YT_PRESENT_REMOTE_SEMI, reset,
		    sizeof(reset) - 1U);
		if (status != YT_PRESENT_OK)
			return status;
	}
	if (snoop != 0.0f)
		return append_local(result, YT_PRESENT_LOCAL_SEMI, reset,
		    sizeof(reset) - 1U, 0, 0);
	return YT_PRESENT_OK;
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

enum yt_present_status
yt_present_low_time_process(const uint8_t *text, size_t length,
    uint8_t remembered[4], struct yt_present_state *state,
    struct yt_present_result *result, bool *warned)
{
	uint8_t narrowed[4];
	bool changed;
	enum yt_present_status status;

	memset(result, 0, sizeof(*result));
	*warned = false;
	status = low_time_value(text, length,
	    (double)qb_mbf32_decode(remembered), narrowed, &changed);
	if (status != YT_PRESENT_OK || !changed)
		return status;
	memcpy(remembered, narrowed, sizeof(narrowed));
	return low_time_warning(text, length, qb_mbf32_decode(remembered),
	    state, result, warned);
}

static enum yt_present_status
status_row_append(const uint8_t *real_name, size_t real_name_length,
    const uint8_t *alias, size_t alias_length,
    struct yt_present_state *state, struct yt_present_result *result)
{
	static const uint8_t title[] = " Yankee Trader ";
	uint8_t clear[79];
	uint8_t expression[63];
	size_t length = 0;
	enum yt_present_status status;

	if (state->sound.snoop == 0.0f)
		return YT_PRESENT_OK;
	memset(clear, ' ', sizeof(clear));
	status = append_locate(result, 25, 1, -1, 0, 0);
	if (status != YT_PRESENT_OK)
		return status;
	status = append_local(result, YT_PRESENT_LOCAL_COLOR, NULL, 0, 11, 1);
	if (status != YT_PRESENT_OK)
		return status;
	status = append_local(result, YT_PRESENT_LOCAL_SEMI, clear,
	    sizeof(clear), 0, 0);
	if (status != YT_PRESENT_OK)
		return status;
	status = append_locate(result, 25, 1, -1, 0, 0);
	if (status != YT_PRESENT_OK)
		return status;
	status = append_local(result, YT_PRESENT_LOCAL_COLOR, NULL, 0, 14, 3);
	if (status != YT_PRESENT_OK)
		return status;
	status = append_local(result, YT_PRESENT_LOCAL_SEMI, title,
	    sizeof(title) - 1U, 0, 0);
	if (status != YT_PRESENT_OK)
		return status;
	status = append_local(result, YT_PRESENT_LOCAL_COLOR, NULL, 0, 11, 1);
	if (status != YT_PRESENT_OK)
		return status;
	status = append_local(result, YT_PRESENT_LOCAL_SEMI,
	    (const uint8_t *)" ", 1, 0, 0);
	if (status != YT_PRESENT_OK)
		return status;
	expression[length++] = ' ';
	expression[length++] = '|';
	expression[length++] = ' ';
	if (real_name_length > sizeof(expression) - length)
		real_name_length = sizeof(expression) - length;
	memcpy(expression + length, real_name, real_name_length);
	length += real_name_length;
	if (length < sizeof(expression))
		expression[length++] = ' ';
	if (length < sizeof(expression))
		expression[length++] = '|';
	if (length < sizeof(expression))
		expression[length++] = ' ';
	if (alias_length > sizeof(expression) - length)
		alias_length = sizeof(expression) - length;
	memcpy(expression + length, alias, alias_length);
	length += alias_length;
	status = append_local(result, YT_PRESENT_LOCAL_SEMI, expression,
	    length, 0, 0);
	if (status != YT_PRESENT_OK)
		return status;
	return append_local(result, YT_PRESENT_LOCAL_COLOR, NULL, 0, 7, 0);
}

enum yt_present_status
yt_present_status_row(const uint8_t *real_name, size_t real_name_length,
    const uint8_t *alias, size_t alias_length,
    struct yt_present_state *state, struct yt_present_result *result)
{
	memset(result, 0, sizeof(*result));
	return status_row_append(real_name, real_name_length, alias,
	    alias_length, state, result);
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
		case YT_PRESENT_LOCAL_CLEAR:
			if (sink->local_clear != NULL)
				sink->local_clear(sink->context);
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
