#include "yt_input.h"

#include "OpenDoor.h"
#include "yt_door.h"

#include <string.h>

static bool
input_session_local(void)
{
	struct yt_door *door = yt_door_current();

	return door != NULL ? door->identity.local
	    : od_control.od_force_local != FALSE;
}

static bool
push_event(struct yt_input_splitter *splitter,
    const tODInputEvent *event)
{
	struct yt_input_value value = {{0, 0}, 0, 0, false};

	if (event->EventType == EVENT_EXTENDED_KEY) {
		value.bytes[0] = 0;
		value.bytes[1] = (uint8_t)event->chKeyPress;
		value.length = 2;
	}
	else {
		value.bytes[0] = (uint8_t)event->chKeyPress;
		value.length = 1;
	}
	/* A Unix forced-local session uses the stdio transport, whose
	 * events retain their transport origin.  They are nevertheless
	 * local input to the game. */
	value.remote = event->bFromRemote != FALSE && !input_session_local();
	return yt_input_splitter_push(splitter, value.remote, &value);
}

static bool
poll_splitter(struct yt_input_splitter *splitter)
{
	tODInputEvent event;

	while (yt_input_splitter_can_push(splitter, false)
	    && yt_input_splitter_can_push(splitter, true)
	    && od_get_input(&event, 0, GETIN_RAW))
		if (!push_event(splitter, &event))
			return false;
	return true;
}

bool
yt_input_poll_legacy(struct yt_input_splitter *splitter, float mode,
    enum yt_input_phase phase, struct yt_input_value *selected)
{
	if (!poll_splitter(splitter))
		return false;
	*selected = yt_input_splitter_select(splitter, mode, phase);
	return true;
}

bool
yt_input_wait_legacy(struct yt_input_splitter *splitter, float mode,
    enum yt_input_phase phase, struct yt_input_value *selected)
{
	tODInputEvent event;

	for (;;) {
		if (!yt_input_poll_legacy(splitter, mode, phase, selected))
			return false;
		if (selected->length != 0U)
			return true;
		if (!od_get_input(&event, OD_NO_TIMEOUT, GETIN_RAW)
		    || !push_event(splitter, &event))
			return false;
	}
}

bool
yt_input_wait_legacy_until(struct yt_input_splitter *splitter,
    float mode, enum yt_input_phase phase, uint32_t seconds,
    uint16_t milliseconds, struct yt_input_value *selected, bool *timed_out)
{
	tODInputEvent event;

	if (timed_out == NULL)
		return false;
	for (;;) {
		if (!yt_input_poll_legacy(splitter, mode, phase, selected))
			return false;
		if (selected->length != 0U) {
			*timed_out = false;
			return true;
		}
		if (!od_get_input_until(&event, seconds, milliseconds, GETIN_RAW)) {
			*timed_out = true;
			return true;
		}
		if (!push_event(splitter, &event))
			return false;
	}
}

bool
yt_input_poll_merged(struct yt_input_splitter *splitter,
    struct yt_input_value *selected)
{
	if (!poll_splitter(splitter))
		return false;
	*selected = yt_input_splitter_select_merged(splitter);
	return true;
}

bool
yt_input_poll_source(struct yt_input_splitter *splitter, bool remote,
    struct yt_input_value *selected)
{
	if (!poll_splitter(splitter))
		return false;
	*selected = yt_input_splitter_select_source(splitter, remote);
	return true;
}
