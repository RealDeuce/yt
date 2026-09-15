#include "yt_session_internal.h"
#include "session_test_runtime.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(expression) do { \
	if (!(expression)) { \
		fprintf(stderr, "check failed at %s:%d: %s\n", \
		    __FILE__, __LINE__, #expression); \
		++failures; \
	} \
} while (0)

static void
test_real_radio_file_and_empty_compose(void)
{
	static const uint8_t message[] = "Status nominal.";
	static const uint8_t empty_target[] = "\r";
	struct yt_door door;
	struct yt_session session;
	struct yt_error error;

	(void)remove("YTRMSG.DAT");
	memset(&door, 0, sizeof(door));
	memset(&session, 0, sizeof(session));
	session.door = &door;
	session.active_player_record = 2;
	session.pager.nonstop = -1.0f;
	yt_error_clear(&error);
	CHECK(session_append_radio_bytes(message, sizeof(message) - 1U,
	    -1.0f, -2.0f, &error));
	CHECK(yt_session_radio_read(&session, true, &error));
	memcpy(session.io.typeahead, empty_target, sizeof(empty_target) - 1U);
	session.io.typeahead_length = sizeof(empty_target) - 1U;
	CHECK(yt_session_radio_compose(&session, &error));
	CHECK(session.io.typeahead_position == session.io.typeahead_length);
	CHECK(remove("YTRMSG.DAT") == 0);
}

int
main(void)
{
	session_test_runtime_start();
	test_real_radio_file_and_empty_compose();
	session_test_runtime_stop();
	return failures == 0 ? 0 : 1;
}
