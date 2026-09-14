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
test_same_sector_plasma_route(void)
{
	static const char path[] = "SESSION-PROJECTILE.DAT";
	struct yt_door door;
	struct yt_session session;
	struct yt_player player;
	struct yt_sector sector;
	struct yt_error error;
	float origin = 1.0f;
	float target = 1.0f;
	float amount = 1.0f;
	int counterattack = 0;
	int xannor_provoker = 0;

	(void)remove(path);
	memset(&door, 0, sizeof(door));
	memset(&session, 0, sizeof(session));
	memset(&player, 0, sizeof(player));
	memset(&sector, 0, sizeof(sector));
	session.door = &door;
	session.player_record_carrier = 2;
	session.pager.nonstop = -1.0f;
	door.game.config.sector_offset = 3.0f;
	yt_record_blank(&door.game.config.record);
	(void)snprintf(door.identity.real_first,
	    sizeof(door.identity.real_first), "%s", "Sysop");
	yt_random_init(&door.game.random);

	yt_record_blank(&player.record);
	(void)snprintf(player.name, sizeof(player.name), "%s", "Launcher");
	player.name_length = 8.0f;
	player.sector = 1.0f;
	yt_player_encode(&player);
	session.player = player;
	yt_record_blank(&sector.record);
	yt_sector_encode(&sector);

	yt_error_clear(&error);
	CHECK(yt_database_open(&door.game.database, path, YT_OPEN_CREATE,
	    &error));
	CHECK(yt_database_write(&door.game.database, 1U,
	    &door.game.config.record, &error));
	CHECK(yt_database_write(&door.game.database, 2U, &player.record,
	    &error));
	CHECK(yt_database_write_durable(&door.game.database, 4U,
	    &sector.record, &error));

	CHECK(session_launch_projectile(&session, &origin, &target, &amount,
	    true, &counterattack, &xannor_provoker, &error));
	CHECK(origin == 0.0f && target == 1.0f && amount == 1.0f);
	CHECK(session.projectile_main_route.origin == 0.0f);
	CHECK(session.projectile_main_route.destination == 1.0f);
	CHECK(session.route_second[0] == 1);
	CHECK(session.route_second[1] == 0);
	CHECK(counterattack == 0 && xannor_provoker == 0);

	yt_database_close(&door.game.database);
	CHECK(remove(path) == 0);
}

int
main(void)
{
	session_test_runtime_start();
	test_same_sector_plasma_route();
	session_test_runtime_stop();
	return failures == 0 ? 0 : 1;
}
