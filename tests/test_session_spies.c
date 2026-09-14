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
test_active_spy_list(void)
{
	struct yt_door door;
	struct yt_session session;
	struct yt_error error;

	memset(&door, 0, sizeof(door));
	memset(&session, 0, sizeof(session));
	session.door = &door;
	session.pager.nonstop = -1.0f;
	session.spy_count = 2;
	session.spy_sectors[0] = 7;
	session.spy_sectors[1] = 19;
	yt_error_clear(&error);
	CHECK(yt_session_list_spies(&session, &error));
	CHECK(session.spy_count == 2);
	CHECK(session.spy_sectors[0] == 7);
	CHECK(session.spy_sectors[1] == 19);

	memcpy(session.queue, "X", 1U);
	session.queue_length = 1U;
	session.queue_position = 0U;
	session.spy_count = 0;
	CHECK(yt_session_list_spies(&session, &error));
	CHECK(session.queue_length == 0U);
}

int
main(void)
{
	session_test_runtime_start();
	test_active_spy_list();
	session_test_runtime_stop();
	return failures == 0 ? 0 : 1;
}
