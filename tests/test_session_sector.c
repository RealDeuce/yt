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
test_current_sector_from_real_records(void)
{
	static const char path[] = "SESSION-SECTOR.DAT";
	struct yt_door door;
	struct yt_session session;
	struct yt_player player;
	struct yt_sector sector;
	struct yt_error error;

	(void)remove(path);
	memset(&door, 0, sizeof(door));
	memset(&session, 0, sizeof(session));
	memset(&player, 0, sizeof(player));
	memset(&sector, 0, sizeof(sector));
	session.door = &door;
	session.active_player_record = 2;
	session.pager.nonstop = -1.0f;
	door.game.config.sector_offset = 3.0f;
	door.game.config.port_offset = 5.0f;
	yt_record_blank(&player.record);
	player.sector = 1.0f;
	yt_player_encode(&player);
	yt_record_blank(&sector.record);
	sector.warps[0] = 2;
	yt_sector_encode(&sector);
	yt_error_clear(&error);
	CHECK(yt_database_open(&door.game.database, path, YT_OPEN_CREATE,
	    &error));
	CHECK(yt_database_write(&door.game.database, 2U, &player.record,
	    &error));
	CHECK(yt_database_write_durable(&door.game.database, 4U,
	    &sector.record, &error));
	CHECK(yt_session_display_sector(&session, false, &error));
	CHECK(session.navigation.current_sector_physical_record == 4);
	CHECK(session.player.sector == 1);
	CHECK(yt_session_sector_entry(&session, &error));
	CHECK(session.navigation.current_sector_physical_record == 4);
	CHECK(session.player.sector == 1);
	sector.fighters = 1.0f;
	sector.fighter_owner = 2;
	CHECK(yt_session_sector_force_is_friendly(&session, &sector, &error));
	yt_database_close(&door.game.database);
	CHECK(remove(path) == 0);
}

int
main(void)
{
	session_test_runtime_start();
	test_current_sector_from_real_records();
	session_test_runtime_stop();
	return failures == 0 ? 0 : 1;
}
