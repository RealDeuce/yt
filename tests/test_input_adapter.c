#include "yt_door.h"
#include "yt_input.h"

#include "OpenDoor.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expression) do { \
	if (!(expression)) { \
		fprintf(stderr, "check failed at %s:%d: %s\n", \
		    __FILE__, __LINE__, #expression); \
		++failures; \
	} \
} while (0)

tODControl od_control;

static struct yt_door test_door;
static tODInputEvent next_event;
static bool event_ready;
static int failures;

struct yt_door *
yt_door_current(void)
{
	return &test_door;
}

BOOL ODCALL
od_get_input(tODInputEvent *event, tODMilliSec wait, WORD flags)
{
	(void)wait;
	(void)flags;
	if (!event_ready)
		return FALSE;
	*event = next_event;
	event_ready = false;
	return TRUE;
}

char ODCALL
od_get_key(BOOL wait)
{
	(void)wait;
	return 0;
}

BOOL ODCALL
od_carrier(void)
{
	return TRUE;
}

void ODCALL
od_putch(char value)
{
	(void)value;
}

void
yt_out_plain(const char *text)
{
	(void)text;
}

static void
queue_character(char value, BOOL transport_remote)
{
	memset(&next_event, 0, sizeof(next_event));
	next_event.EventType = EVENT_CHARACTER;
	next_event.bFromRemote = transport_remote;
	next_event.chKeyPress = value;
	event_ready = true;
}

static void
test_forced_local_stdio_origin(void)
{
	struct yt_input_splitter splitter;
	struct yt_input_value selected;

	memset(&test_door, 0, sizeof(test_door));
	test_door.identity.local = true;
	od_control.od_force_local = TRUE;
	od_control.baud = 19200U;
	yt_input_splitter_init(&splitter);
	queue_character('K', TRUE);
	CHECK(yt_input_poll_legacy(&splitter, 1.0f,
	    YT_INPUT_PHASE_WAIT, &selected));
	CHECK(selected.length == 1U && selected.bytes[0] == 'K'
	    && !selected.remote);
}

static void
test_remote_origin_stays_remote(void)
{
	struct yt_input_splitter splitter;
	struct yt_input_value selected;

	memset(&test_door, 0, sizeof(test_door));
	od_control.od_force_local = FALSE;
	od_control.baud = 0U;
	yt_input_splitter_init(&splitter);
	queue_character('R', TRUE);
	CHECK(yt_input_poll_legacy(&splitter, 0.0f,
	    YT_INPUT_PHASE_B05D, &selected));
	CHECK(selected.length == 1U && selected.bytes[0] == 'R'
	    && selected.remote);
}

int
main(void)
{
	test_forced_local_stdio_origin();
	test_remote_origin_stays_remote();
	if (failures != 0) {
		fprintf(stderr, "test_input_adapter: %d failure(s)\n", failures);
		return 1;
	}
	puts("test_input_adapter: ok");
	return 0;
}
