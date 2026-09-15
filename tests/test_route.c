#include "yt_session_internal.h"
#include "session_test_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned failures;

#define CHECK(expr) do { \
	if (!(expr)) { \
		fprintf(stderr, "%s:%d: check failed: %s\n", \
		    __FILE__, __LINE__, #expr); \
		++failures; \
	} \
} while (0)

static void
write_sector(struct yt_game *game, int logical, const float warps[6],
    struct yt_error *error)
{
	struct yt_sector sector;

	memset(&sector, 0, sizeof(sector));
	yt_record_blank(&sector.record);
	memcpy(sector.warps, warps, sizeof(sector.warps));
	yt_sector_encode(&sector);
	CHECK(yt_database_write_durable(&game->database,
	    (size_t)yt_sector_basic_record(&game->config, logical),
	    &sector.record, error));
}

static void
test_routes_from_database(void)
{
	static const char path[] = "ROUTE-TEST.DAT";
	static const float sector_one[6] = {3.0f, 2.0f, 1.0f, 1.0f, 1.0f, 1.0f};
	static const float sector_two[6] = {4.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f};
	static const float sector_three[6] = {4.0f, 3.0f, 3.0f, 3.0f, 3.0f, 3.0f};
	static const float sector_four[6] = {4.0f, 4.0f, 4.0f, 4.0f, 4.0f, 4.0f};
	static const float isolated_one[6] = {2.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
	static const float isolated_two[6] = {2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f};
	struct yt_door door;
	struct yt_session session;
	struct session_route_plan route;
	struct yt_error error;
	enum yt_route_outcome outcome;
	float status;
	bool found;

	(void)remove(path);
	memset(&door, 0, sizeof(door));
	memset(&session, 0, sizeof(session));
	session.door = &door;
	door.game.config.sector_offset = 1.0f;
	door.game.config.port_offset = 6.0f;
	yt_error_clear(&error);
	CHECK(yt_database_open(&door.game.database, path, YT_OPEN_CREATE,
	    &error));
	write_sector(&door.game, 1, sector_one, &error);
	write_sector(&door.game, 2, sector_two, &error);
	write_sector(&door.game, 3, sector_three, &error);
	write_sector(&door.game, 4, sector_four, &error);

	CHECK(yt_session_build_route(&session, 1.0f, 4.0f, &route, false,
	    &found, &outcome, &status, &error));
	CHECK(found && outcome == YT_ROUTE_FOUND && status == 0.0f);
	CHECK(route.next_hop[1] == 3
	    && route.next_hop[3] == 4
	    && route.next_hop[4] == 0);

	write_sector(&door.game, 1, isolated_one, &error);
	write_sector(&door.game, 2, isolated_two, &error);
	CHECK(yt_session_build_route(&session, 1.0f, 4.0f, &route, false,
	    &found, &outcome, &status, &error));
	CHECK(!found && outcome == YT_ROUTE_NOT_FOUND && status == 1.0f);
	CHECK(route.next_hop[1] == 0 && route.next_hop[2] == 2);

	write_sector(&door.game, 1, sector_one, &error);
	write_sector(&door.game, 2, sector_two, &error);
	memset(session.route_avoid, 0, sizeof(session.route_avoid));
	session.route_avoid[0] = 3.0f;
	CHECK(yt_session_build_route(&session, 1.0f, 4.0f, &route, true,
	    &found, &outcome, &status, &error));
	CHECK(found && outcome == YT_ROUTE_FOUND && status == 0.0f);
	CHECK(route.next_hop[1] == 2 && route.next_hop[2] == 4);

	session.route_avoid[0] = 1.0f;
	CHECK(yt_session_build_route(&session, 1.0f, 4.0f, &route, true,
	    &found, &outcome, &status, &error));
	CHECK(!found && outcome == YT_ROUTE_NOT_FOUND && status == 1.0f);

	CHECK(yt_session_build_route(&session, 1.0f, 1.0f, &route, true,
	    &found, &outcome, &status, &error));
	CHECK(found && outcome == YT_ROUTE_SAME && status == 1.0f);
	CHECK(route.next_hop[1] == 0);

	yt_database_close(&door.game.database);
	CHECK(remove(path) == 0);
}

int
main(void)
{
	session_test_runtime_start();
	test_routes_from_database();
	session_test_runtime_stop();
	if (failures != 0U)
		return EXIT_FAILURE;
	puts("test_route: ok");
	return EXIT_SUCCESS;
}
