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
static bool command_line_parsed;
static bool mock_local;
static WORD init_maxtime;
static tODInputEvent next_event;
static bool event_ready;
static bool until_called;
static DWORD until_seconds;
static WORD until_milliseconds;
static unsigned exit_calls;
static BOOL exit_noexit;
static int failures;

#ifdef ODPLAT_WIN32
void ODCALL
od_parse_cmd_line(char *command_line)
{
	(void)command_line;
	command_line_parsed = true;
}
#else
void ODCALL
od_parse_cmd_line(int argc, char **argv)
{
	(void)argc;
	(void)argv;
	command_line_parsed = true;
}
#endif

void ODCALL
od_init(void)
{
	CHECK(command_line_parsed);
	init_maxtime = od_control.od_maxtime;
	od_control.od_force_local = mock_local ? TRUE : FALSE;
	od_control.baud = mock_local ? 0U : 19200U;
	od_control.user_ansi = TRUE;
	od_control.user_timelimit = 300;
	(void)snprintf(od_control.system_name, sizeof(od_control.system_name),
	    "TEST SYSTEM");
	(void)snprintf(od_control.sysop_name, sizeof(od_control.sysop_name),
	    "TEST SYSOP");
	(void)snprintf(od_control.user_name, sizeof(od_control.user_name),
	    "TEST USER");
	(void)snprintf(od_control.user_location,
	    sizeof(od_control.user_location), "TEST LOCATION");
	od_control.od_inactivity = 200;
	od_control.od_inactive_warning = 10;
	od_control.od_disable = DIS_CARRIERDETECT | DIS_TIMEOUT;
	od_control.od_disable_inactivity = TRUE;
	od_control.od_always_clear = TRUE;
	od_control.od_status_on = TRUE;
}

void ODCALL
od_exit(INT errorlevel, BOOL terminate_call)
{
	(void)errorlevel;
	(void)terminate_call;
	++exit_calls;
	exit_noexit = od_control.od_noexit;
	if (od_control.od_before_exit != NULL)
		od_control.od_before_exit();
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

BOOL ODCALL
od_get_input_until(tODInputEvent *event, DWORD seconds, WORD milliseconds,
    WORD flags)
{
	until_called = true;
	until_seconds = seconds;
	until_milliseconds = milliseconds;
	return od_get_input(event, 0, flags);
}

void ODCALL
od_get_time(DWORD *seconds, WORD *milliseconds)
{
	*seconds = 0U;
	*milliseconds = 0U;
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
test_open_doors_runtime_policy(void)
{
	struct yt_error error;
	char *remote_argv[] = {(char *)"yt", (char *)"DORINFO1.DEF"};
	char *local_argv[] = {(char *)"yt", (char *)"-l"};

	memset(&od_control, 0, sizeof(od_control));
	command_line_parsed = false;
	mock_local = false;
	yt_error_clear(&error);
	CHECK(yt_door_start(&test_door, 2, remote_argv, &error));
	CHECK(init_maxtime == 180U);
	CHECK(od_control.od_inactivity == 240);
	CHECK((od_control.od_disable
	    & (DIS_CARRIERDETECT | DIS_TIMEOUT)) == 0U);
	CHECK(!od_control.od_disable_inactivity);
	CHECK(od_control.od_inactive_warning == 0);
	CHECK(strcmp(od_control.od_inactivity_warning, "") == 0);
	CHECK(strcmp(od_control.od_time_warning, "") == 0);
	CHECK(strcmp(od_control.od_inactivity_timeout,
	    "\r\n\aUSER FELL ASLEEP!\n\r") == 0);
	CHECK(strcmp(od_control.od_no_time,
	    "\r\n\a\a\aTIME LIMIT EXCEEDED!\a\a\a\n\r") == 0);
	CHECK(!od_control.od_always_clear && !od_control.od_status_on);
	CHECK(od_control.od_before_exit == yt_door_cleanup);
	CHECK(!test_door.identity.local && test_door.identity.ansi);
	CHECK(strcmp(test_door.identity.real_first, "Test") == 0);
	CHECK(strcmp(test_door.identity.real_last, "User") == 0);

	memset(&od_control, 0, sizeof(od_control));
	command_line_parsed = false;
	mock_local = true;
	yt_error_clear(&error);
	CHECK(yt_door_start(&test_door, 2, local_argv, &error));
	CHECK(init_maxtime == 180U);
	CHECK(test_door.identity.local);
	CHECK(od_control.od_inactivity == 0);
	exit_calls = 0U;
	exit_noexit = FALSE;
	yt_door_shutdown_for_replace();
	CHECK(exit_calls == 1U && exit_noexit);
	CHECK(!test_door.open_doors_initialized);
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

static void
test_open_doors_deadline_wait(void)
{
	struct yt_input_splitter splitter;
	struct yt_input_value selected;
	bool timed_out = false;

	memset(&test_door, 0, sizeof(test_door));
	test_door.identity.local = false;
	od_control.od_force_local = FALSE;
	yt_input_splitter_init(&splitter);
	event_ready = false;
	until_called = false;
	CHECK(yt_input_wait_legacy_until(&splitter, 0.0f,
	    YT_INPUT_PHASE_WAIT, 12U, 345U, &selected, &timed_out));
	CHECK(until_called && until_seconds == 12U
	    && until_milliseconds == 345U);
	CHECK(timed_out && selected.length == 0U);
}

int
main(void)
{
	test_open_doors_runtime_policy();
	test_forced_local_stdio_origin();
	test_remote_origin_stays_remote();
	test_open_doors_deadline_wait();
	if (failures != 0) {
		fprintf(stderr, "test_input_adapter: %d failure(s)\n", failures);
		return 1;
	}
	puts("test_input_adapter: ok");
	return 0;
}
