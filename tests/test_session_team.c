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
test_teamless_exit(void)
{
	static const char path[] = "SESSION-TEAM.DAT";
	static const uint8_t answer[] = "1\r";
	struct yt_door door;
	struct yt_session session;
	struct yt_player player;
	struct yt_record persisted;
	struct yt_error error;

	(void)remove(path);
	memset(&door, 0, sizeof(door));
	memset(&session, 0, sizeof(session));
	memset(&player, 0, sizeof(player));
	session.door = &door;
	session.active_player_record = 2;
	session.pager.nonstop = -1.0f;
	memcpy(session.io.typeahead, answer, sizeof(answer) - 1U);
	session.io.typeahead_length = sizeof(answer) - 1U;
	door.game.config.sector_offset = 3.0f;
	door.game.config.port_offset = 4.0f;
	yt_record_blank(&door.game.config.record);
	yt_record_blank(&player.record);
	(void)snprintf(player.name, sizeof(player.name), "%s", "Teamless");
	player.name_length = 8.0f;
	player.sector = 1.0f;
	player.team = 0.0f;
	yt_player_encode(&player);

	yt_error_clear(&error);
	CHECK(yt_database_open(&door.game.database, path, YT_OPEN_CREATE,
	    &error));
	CHECK(yt_database_write(&door.game.database, 1U,
	    &door.game.config.record, &error));
	CHECK(yt_database_write_durable(&door.game.database, 2U,
	    &player.record, &error));

	CHECK(yt_session_command_team(&session, &error));
	CHECK(session.io.typeahead_position == session.io.typeahead_length);
	CHECK(session.player.team == 0.0f);
	CHECK(yt_database_read(&door.game.database, 2U, &persisted, &error));
	CHECK(memcmp(persisted.bytes, player.record.bytes,
	    sizeof(persisted.bytes)) == 0);

	yt_database_close(&door.game.database);
	CHECK(remove(path) == 0);
}

int
main(void)
{
	session_test_runtime_start();
	test_teamless_exit();
	session_test_runtime_stop();
	return failures == 0 ? 0 : 1;
}
