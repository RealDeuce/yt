#include "yt_session_internal.h"
#include "session_test_runtime.h"

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
	struct yt_player player;
	struct yt_planet planet;
	struct yt_record persisted;
	struct yt_error error;
	int today;
	int adjusted_year;
	bool denied;

	(void)remove(path);
	memset(&door, 0, sizeof(door));
	memset(&session, 0, sizeof(session));
	memset(&player, 0, sizeof(player));
	memset(&planet, 0, sizeof(planet));
	session.door = &door;
	session.player_record_carrier = 2;
	session.pager.nonstop = -1.0f;
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
	yt_record_blank(&player.record);
	player.holds = 20.0f;
	player.credits = 1000.0f;
	player.sector = 1.0f;
	yt_player_encode(&player);
	CHECK(yt_database_open(&door.game.database, path, YT_OPEN_CREATE,
	    &error));
	CHECK(yt_database_write(&door.game.database, 2U, &player.record,
	    &error));
	CHECK(yt_database_write(&door.game.database, 4U, &planet.record,
	    &error));
	CHECK(yt_session_planet_permission(&session, 1, &denied, &error));
	CHECK(!denied);
	CHECK(door.game.random.draws == 0U);
	CHECK(session.planet_economy.current_day == (float)today);
	CHECK(yt_database_read(&door.game.database, 4U, &persisted, &error));
	CHECK(yt_record_get_number(&persisted, YT_F41) == (float)today);
	CHECK(yt_record_get_number(&persisted, YT_F73) == 2.0f);
	CHECK(yt_record_get_number(&persisted, YT_F77) == 5.0f);
	CHECK(yt_session_planet_inventory(&session, 1, &error));
	CHECK(session.player.holds == 20.0f);
	yt_database_close(&door.game.database);
	CHECK(remove(path) == 0);
}

static void
test_land_and_leave_owned_planet(void)
{
	static const char path[] = "SESSION-LAND.DAT";
	static const uint8_t answer[] = "L\r";
	static const uint8_t same_sector[] = "1\r";
	struct yt_door door;
	struct yt_session session;
	struct yt_player player;
	struct yt_sector sector;
	struct yt_planet planet;
	struct yt_error error;
	int today;
	int adjusted_year;
	bool enter_sector = false;

	(void)remove(path);
	memset(&door, 0, sizeof(door));
	memset(&session, 0, sizeof(session));
	memset(&player, 0, sizeof(player));
	memset(&sector, 0, sizeof(sector));
	memset(&planet, 0, sizeof(planet));
	session.door = &door;
	session.player_record_carrier = 2;
	session.pager.nonstop = -1.0f;
	session.running = true;
	memcpy(session.queue, answer, sizeof(answer) - 1U);
	session.queue_length = sizeof(answer) - 1U;
	door.game.config.epoch_year = 26.0f;
	door.game.config.sector_offset = 3.0f;
	door.game.config.port_offset = 5.0f;
	door.game.config.planet_offset = 6.0f;
	door.game.config.total_records = 7.0f;
	yt_error_clear(&error);
	CHECK(yt_current_date_serial(door.game.config.epoch_year, &today,
	    &adjusted_year, &error));

	yt_record_blank(&player.record);
	(void)snprintf(player.name, sizeof(player.name), "%s", "Owner");
	player.name_length = 5.0f;
	player.sector = 1.0f;
	player.holds = 20.0f;
	player.credits = 1000.0f;
	yt_player_encode(&player);
	yt_record_blank(&sector.record);
	sector.planet = 1.0f;
	yt_sector_encode(&sector);
	yt_record_blank(&planet.record);
	(void)snprintf(planet.name, sizeof(planet.name), "%s", "Home");
	planet.name_length = 4.0f;
	planet.last_day = (float)today;
	planet.owner = 2.0f;
	planet.ground_forces = 5.0f;
	yt_planet_encode(&planet);

	CHECK(yt_database_open(&door.game.database, path, YT_OPEN_CREATE,
	    &error));
	CHECK(yt_database_write(&door.game.database, 2U, &player.record,
	    &error));
	CHECK(yt_database_write(&door.game.database, 4U, &sector.record,
	    &error));
	CHECK(yt_database_write(&door.game.database, 7U, &planet.record,
	    &error));
	CHECK(yt_session_command_land(&session, &enter_sector, &error));
	CHECK(enter_sector);
	CHECK(session.queue_position == session.queue_length);
	CHECK(session.player.sector == 1.0f);
	memcpy(session.queue, same_sector, sizeof(same_sector) - 1U);
	session.queue_length = sizeof(same_sector) - 1U;
	session.queue_position = 0U;
	enter_sector = false;
	CHECK(yt_session_planet_move(&session, &enter_sector, &error));
	CHECK(!enter_sector);
	CHECK(session.queue_length == 0U);
	CHECK(door.game.random.draws == 0U);
	yt_database_close(&door.game.database);
	CHECK(remove(path) == 0);
}

int
main(void)
{
	session_test_runtime_start();
	test_self_owned_planet();
	test_land_and_leave_owned_planet();
	session_test_runtime_stop();
	return failures == 0 ? 0 : 1;
}
