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
test_activate_and_deactivate(void)
{
	static const char path[] = "SESSION-COMPUTER.DAT";
	static const uint8_t command[] = "1\r";
	static const uint8_t avoid[] = "1\r7\r";
	struct yt_door door;
	struct yt_session session;
	struct yt_player player;
	struct yt_error error;
	bool enter_sector = false;

	(void)remove(path);
	memset(&door, 0, sizeof(door));
	memset(&session, 0, sizeof(session));
	memset(&player, 0, sizeof(player));
	session.door = &door;
	session.player_record_carrier = 2;
	session.pager.nonstop = -1.0f;
	session.running = true;
	memcpy(session.queue, command, sizeof(command) - 1U);
	session.queue_length = sizeof(command) - 1U;
	yt_record_blank(&player.record);
	player.sector = 1.0f;
	yt_player_encode(&player);
	yt_error_clear(&error);
	CHECK(yt_database_open(&door.game.database, path, YT_OPEN_CREATE,
	    &error));
	CHECK(yt_database_write(&door.game.database, 2U, &player.record,
	    &error));
	CHECK(yt_session_computer_menu(&session, &enter_sector, &error));
	CHECK(enter_sector);
	CHECK(session.queue_position == session.queue_length);
	CHECK(session.shared_status == 0.0f);
	door.game.config.sector_offset = 3.0f;
	door.game.config.port_offset = 23.0f;
	memcpy(session.queue, avoid, sizeof(avoid) - 1U);
	session.queue_length = sizeof(avoid) - 1U;
	session.queue_position = 0U;
	CHECK(yt_session_computer_avoid(&session, &error));
	CHECK(session.route_avoid[0] == 7.0f);
	CHECK(session.queue_position == session.queue_length);
	yt_database_close(&door.game.database);
	CHECK(remove(path) == 0);
}

int
main(void)
{
	session_test_runtime_start();
	test_activate_and_deactivate();
	session_test_runtime_stop();
	return failures == 0 ? 0 : 1;
}
