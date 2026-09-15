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
test_genesis_decline(void)
{
	static const char path[] = "SESSION-GENESIS.DAT";
	static const uint8_t decline[] = "N\r";
	struct yt_door door;
	struct yt_session session;
	struct yt_player player;
	struct yt_error error;

	(void)remove(path);
	memset(&door, 0, sizeof(door));
	memset(&session, 0, sizeof(session));
	memset(&player, 0, sizeof(player));
	session.door = &door;
	session.active_player_record = 2;
	session.pager.nonstop = -1.0f;
	(void)snprintf(session.player.name, sizeof(session.player.name), "%s",
	    "Baron");
	memcpy(session.queue, decline, sizeof(decline) - 1U);
	session.queue_length = sizeof(decline) - 1U;
	door.game.config.genesis_ports = 300.0f;
	yt_record_blank(&player.record);
	(void)snprintf(player.name, sizeof(player.name), "%s", "Baron");
	player.name_length = 5.0f;
	player.ports_owned = 300.0f;
	yt_player_encode(&player);
	yt_error_clear(&error);
	CHECK(yt_database_open(&door.game.database, path, YT_OPEN_CREATE,
	    &error));
	door.game_open = true;
	CHECK(yt_database_write_durable(&door.game.database, 2U,
	    &player.record, &error));
	CHECK(yt_session_command_genesis(&session, &error));
	CHECK(session.queue_position == session.queue_length);
	CHECK(door.game_open);
	CHECK(door.game.database.file != NULL);
	CHECK(session.player.ports_owned == 300.0f);
	yt_database_close(&door.game.database);
	CHECK(remove(path) == 0);
}

int
main(void)
{
	session_test_runtime_start();
	test_genesis_decline();
	session_test_runtime_stop();
	return failures == 0 ? 0 : 1;
}
