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
test_replace_sector_force(void)
{
	static const char path[] = "SESSION-FIGHTERS.DAT";
	static const uint8_t desired[] = "7\r";
	struct yt_door door;
	struct yt_session session;
	struct yt_player player;
	struct yt_sector sector;
	struct yt_record persisted;
	struct yt_error error;

	(void)remove(path);
	memset(&door, 0, sizeof(door));
	memset(&session, 0, sizeof(session));
	memset(&player, 0, sizeof(player));
	memset(&sector, 0, sizeof(sector));
	session.door = &door;
	session.active_player_record = 2;
	session.pager.nonstop = true;
	memcpy(session.io.typeahead, desired, sizeof(desired) - 1U);
	session.io.typeahead_length = sizeof(desired) - 1U;
	door.game.config.sector_offset = 51.0f;
	yt_record_blank(&player.record);
	player.sector = 8.0f;
	player.fighters = 10.0f;
	yt_player_encode(&player);
	yt_record_blank(&sector.record);
	sector.fighters = 5.0f;
	sector.fighter_owner = 2;
	yt_sector_encode(&sector);
	yt_error_clear(&error);
	CHECK(yt_database_open(&door.game.database, path, YT_OPEN_CREATE,
	    &error));
	CHECK(yt_database_write(&door.game.database, 2U, &player.record,
	    &error));
	CHECK(yt_database_write_durable(&door.game.database, 59U,
	    &sector.record, &error));
	CHECK(yt_session_command_fighters(&session, &error));
	CHECK(session.io.typeahead_position == session.io.typeahead_length);
	CHECK(yt_database_read(&door.game.database, 59U, &persisted, &error));
	yt_sector_decode(&sector, &persisted);
	CHECK(sector.fighters == 7.0f);
	CHECK(sector.fighter_owner == 2);
	CHECK(yt_database_read(&door.game.database, 2U, &persisted, &error));
	yt_player_decode(&player, &persisted);
	CHECK(player.fighters == 8.0f);
	CHECK(player.sector == 8);
	yt_database_close(&door.game.database);
	CHECK(remove(path) == 0);
}

int
main(void)
{
	session_test_runtime_start();
	test_replace_sector_force();
	session_test_runtime_stop();
	return failures == 0 ? 0 : 1;
}
