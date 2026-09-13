#include "yt_session_internal.h"

#include "yt_file.h"

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
test_self_owned_planet(void)
{
	static const char path[] = "SESSION-PLANET.DAT";
	struct yt_door door;
	struct yt_session session;
	struct yt_planet planet;
	struct yt_error error;
	int today;
	int adjusted_year;
	bool denied;

	(void)remove(path);
	memset(&door, 0, sizeof(door));
	memset(&session, 0, sizeof(session));
	memset(&planet, 0, sizeof(planet));
	session.door = &door;
	session.player_record_carrier = 2;
	door.game.config.epoch_year = 26.0f;
	door.game.config.planet_offset = 3.0f;
	yt_error_clear(&error);
	CHECK(yt_current_date_serial(door.game.config.epoch_year, &today,
	    &adjusted_year, &error));
	planet.last_day = (float)today;
	planet.owner = 2.0f;
	planet.ground_forces = 5.0f;
	yt_record_blank(&planet.record);
	yt_planet_encode(&planet);
	CHECK(yt_database_open(&door.game.database, path, YT_OPEN_CREATE,
	    &error));
	CHECK(yt_database_write(&door.game.database, 4U, &planet.record,
	    &error));
	CHECK(yt_session_planet_permission(&session, 1, &denied, &error));
	CHECK(!denied);
	CHECK(door.game.random.draws == 0U);
	yt_database_close(&door.game.database);
	CHECK(remove(path) == 0);
}

int
main(void)
{
	test_self_owned_planet();
	return failures == 0 ? 0 : 1;
}
