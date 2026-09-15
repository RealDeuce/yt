#include "yt_input.h"

#include "OpenDoor.h"
#include "yt_door.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

static bool
input_session_local(void)
{
	struct yt_door *door = yt_door_current();

	return door != NULL ? door->identity.local
	    : od_control.od_force_local != FALSE;
}

static void
input_value(const tODInputEvent *event, struct yt_input_value *value)
{
	memset(value, 0, sizeof(*value));
	if (event->EventType == EVENT_EXTENDED_KEY) {
		value->bytes[0] = 0;
		value->bytes[1] = (uint8_t)event->chKeyPress;
		value->length = 2U;
	}
	else {
		value->bytes[0] = (uint8_t)event->chKeyPress;
		value->length = 1U;
	}
	value->remote = event->bFromRemote != FALSE && !input_session_local();
}

void
yt_input_init(struct yt_input *input)
{
	memset(input, 0, sizeof(*input));
}

static bool
input_pending_take(struct yt_input *input, struct yt_input_value *selected)
{
	if (!input->pending_valid)
		return false;
	*selected = input->pending;
	memset(&input->pending, 0, sizeof(input->pending));
	input->pending_valid = false;
	return true;
}

bool
yt_input_poll(struct yt_input *input, struct yt_input_value *selected)
{
	tODInputEvent event;

	if (input == NULL || selected == NULL)
		return false;
	if (input_pending_take(input, selected))
		return true;
	memset(selected, 0, sizeof(*selected));
	if (od_get_input(&event, 0, GETIN_RAW))
		input_value(&event, selected);
	return true;
}

bool
yt_input_wait(struct yt_input *input, struct yt_input_value *selected)
{
	tODInputEvent event;

	if (input == NULL || selected == NULL)
		return false;
	if (input_pending_take(input, selected))
		return true;
	if (!od_get_input(&event, OD_NO_TIMEOUT, GETIN_RAW))
		return false;
	input_value(&event, selected);
	return true;
}

bool
yt_input_wait_until(struct yt_input *input, uint32_t seconds,
    uint16_t milliseconds, struct yt_input_value *selected, bool *timed_out)
{
	tODInputEvent event;

	if (input == NULL || selected == NULL || timed_out == NULL)
		return false;
	if (input_pending_take(input, selected)) {
		*timed_out = false;
		return true;
	}
	memset(selected, 0, sizeof(*selected));
	if (!od_get_input_until(&event, seconds, milliseconds, GETIN_RAW)) {
		*timed_out = true;
		return true;
	}
	input_value(&event, selected);
	*timed_out = false;
	return true;
}

bool
yt_input_pause(struct yt_input *input, double seconds)
{
	struct yt_input_value selected = {{0, 0}, 0, false};
	uint64_t deadline_milliseconds;
	uint64_t duration_milliseconds;
	DWORD current_seconds;
	WORD current_milliseconds;
	bool timed_out;

	if (!isfinite(seconds))
		return false;
	if (seconds <= 0.0)
		return true;
	if (seconds > (double)UINT32_MAX)
		return false;
	od_get_time(&current_seconds, &current_milliseconds);
	duration_milliseconds = (uint64_t)llround(seconds * 1000.0);
	if (duration_milliseconds == 0U)
		duration_milliseconds = 1U;
	deadline_milliseconds = (uint64_t)current_seconds * 1000U
	    + current_milliseconds + duration_milliseconds;
	if (deadline_milliseconds > (uint64_t)UINT32_MAX * 1000U + 999U)
		return false;
	return yt_input_wait_until(input,
	    (uint32_t)(deadline_milliseconds / 1000U),
	    (uint16_t)(deadline_milliseconds % 1000U), &selected, &timed_out);
}

bool
yt_input_poll_source(struct yt_input *input, bool remote,
    struct yt_input_value *selected)
{
	tODInputEvent event;
	struct yt_input_value value;

	if (input == NULL || selected == NULL)
		return false;
	memset(selected, 0, sizeof(*selected));
	if (input->pending_valid) {
		if (input->pending.remote == remote)
			(void)input_pending_take(input, selected);
		return true;
	}
	if (!od_get_input(&event, 0, GETIN_RAW))
		return true;
	input_value(&event, &value);
	if (value.remote == remote)
		*selected = value;
	else {
		input->pending = value;
		input->pending_valid = true;
	}
	return true;
}

bool
yt_input_source_ready(struct yt_input *input, bool remote, bool *ready)
{
	tODInputEvent event;

	if (input == NULL || ready == NULL)
		return false;
	if (!input->pending_valid && od_get_input(&event, 0, GETIN_RAW)) {
		input_value(&event, &input->pending);
		input->pending_valid = true;
	}
	*ready = input->pending_valid && input->pending.remote == remote;
	return true;
}
