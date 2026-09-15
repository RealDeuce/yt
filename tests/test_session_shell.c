#include "yt_session_internal.h"
#include "session_test_runtime.h"

#include <stdint.h>
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
test_quit_decline(void)
{
	static const uint8_t decline[] = "N\r";
	struct yt_door door;
	struct yt_session session;
	struct yt_error error;
	bool confirmed = true;

	memset(&door, 0, sizeof(door));
	memset(&session, 0, sizeof(session));
	session.door = &door;
	session.pager.nonstop = -1.0f;
	memcpy(session.queue, decline, sizeof(decline) - 1U);
	session.queue_length = sizeof(decline) - 1U;
	yt_error_clear(&error);
	CHECK(session_quit_confirm(&session, &confirmed, &error));
	CHECK(!confirmed);
	CHECK(session.queue_position == session.queue_length);
	CHECK(session.presentation.foreground == 7.0f);
}

int
main(void)
{
	session_test_runtime_start();
	test_quit_decline();
	session_test_runtime_stop();
	return failures == 0 ? 0 : 1;
}
