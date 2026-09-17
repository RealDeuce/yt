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
test_earth_store_exit(void)
{
	static const char path[] = "SESSION-EARTH.DAT";
	static const uint8_t leave[] = "0\r";
	struct yt_door door;
	struct yt_session session;
	struct yt_player player;
	struct yt_port earth;
	struct yt_error error;
	bool enter_sector = false;

	(void)remove(path);
	memset(&door, 0, sizeof(door));
	memset(&session, 0, sizeof(session));
	memset(&player, 0, sizeof(player));
	memset(&earth, 0, sizeof(earth));
	session.door = &door;
	session.active_player_record = 2;
	session.pager.nonstop = true;
	session.earth.report_seen = true;
	memcpy(session.io.typeahead, leave, sizeof(leave) - 1U);
	session.io.typeahead_length = sizeof(leave) - 1U;
	door.game.config.sector_offset = 51.0f;
	door.game.config.port_offset = 2055.0f;
	yt_record_blank(&player.record);
	player.credits = 123456.0f;
	player.holds = 25.0f;
	player.sector = 1.0f;
	yt_player_encode(&player);
	yt_record_blank(&earth.record);
	earth.owner = 0;
	earth.treasury = 5000.0f;
	yt_port_encode(&earth);
	yt_error_clear(&error);
	CHECK(yt_database_open(&door.game.database, path, YT_OPEN_CREATE,
	    &error));
	CHECK(yt_database_write(&door.game.database, 2U, &player.record,
	    &error));
	CHECK(yt_database_write_durable(&door.game.database, 2056U,
	    &earth.record, &error));
	CHECK(yt_session_earth_store(&session, &enter_sector, &error));
	CHECK(enter_sector);
	CHECK(session.io.typeahead_position == session.io.typeahead_length);
	CHECK(session.player.credits == 123456.0f);
	CHECK(session.player.sector == 1);
	CHECK(session.earth.report_seen == false);
	yt_database_close(&door.game.database);
	CHECK(remove(path) == 0);
}

int
main(void)
{
	session_test_runtime_start();
	test_earth_store_exit();
	session_test_runtime_stop();
	return failures == 0 ? 0 : 1;
}
