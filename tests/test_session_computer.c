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
	static const uint8_t route[] = "1\r2\r";
	static const uint8_t planet_report[] = "7\r";
	struct yt_door door;
	struct yt_session session;
	struct yt_player player;
	struct yt_sector sector;
	struct yt_planet planet;
	struct yt_error error;
	bool enter_sector = false;

	(void)remove(path);
	memset(&door, 0, sizeof(door));
	memset(&session, 0, sizeof(session));
	memset(&player, 0, sizeof(player));
	memset(&sector, 0, sizeof(sector));
	memset(&planet, 0, sizeof(planet));
	session.door = &door;
	session.active_player_record = 2;
	session.pager.nonstop = -1.0f;
	session.running = true;
	memcpy(session.io.typeahead, command, sizeof(command) - 1U);
	session.io.typeahead_length = sizeof(command) - 1U;
	yt_record_blank(&player.record);
	player.sector = 1.0f;
	player.turns = 10.0f;
	yt_player_encode(&player);
	yt_error_clear(&error);
	CHECK(yt_database_open(&door.game.database, path, YT_OPEN_CREATE,
	    &error));
	CHECK(yt_database_write(&door.game.database, 2U, &player.record,
	    &error));
	CHECK(yt_session_computer_menu(&session, &enter_sector, &error));
	CHECK(enter_sector);
	CHECK(session.io.typeahead_position == session.io.typeahead_length);
	door.game.config.sector_offset = 3.0f;
	door.game.config.port_offset = 23.0f;
	door.game.config.planet_offset = 43.0f;
	door.game.config.total_records = 44.0f;
	memcpy(session.io.typeahead, avoid, sizeof(avoid) - 1U);
	session.io.typeahead_length = sizeof(avoid) - 1U;
	session.io.typeahead_position = 0U;
	CHECK(yt_session_computer_avoid(&session, &error));
	CHECK(session.navigation.avoided_sectors[0] == 7.0f);
	CHECK(session.io.typeahead_position == session.io.typeahead_length);
	yt_record_blank(&sector.record);
	sector.warps[0] = 2;
	yt_sector_encode(&sector);
	CHECK(yt_database_write(&door.game.database, 4U, &sector.record,
	    &error));
	yt_record_blank(&sector.record);
	yt_sector_encode(&sector);
	CHECK(yt_database_write_durable(&door.game.database, 5U,
	    &sector.record, &error));
	memcpy(session.io.typeahead, route, sizeof(route) - 1U);
	session.io.typeahead_length = sizeof(route) - 1U;
	session.io.typeahead_position = 0U;
	CHECK(yt_session_computer_route(&session, false, &error));
	CHECK(session.io.typeahead_position == session.io.typeahead_length);
	CHECK(session.navigation.route_marker == 0.0f);
	yt_record_blank(&sector.record);
	sector.planet = 1;
	yt_sector_encode(&sector);
	CHECK(yt_database_write_durable(&door.game.database, 10U,
	    &sector.record, &error));
	yt_record_blank(&planet.record);
	planet.owner = 2;
	yt_planet_encode(&planet);
	CHECK(yt_database_write_durable(&door.game.database, 44U,
	    &planet.record, &error));
	memcpy(session.io.typeahead, planet_report, sizeof(planet_report) - 1U);
	session.io.typeahead_length = sizeof(planet_report) - 1U;
	session.io.typeahead_position = 0U;
	CHECK(yt_session_computer_planet_report(&session, &error));
	CHECK(session.io.typeahead_position == session.io.typeahead_length);
	CHECK(session.planet.current_record == 44U);
	yt_database_close(&door.game.database);
	CHECK(remove(path) == 0);
}

static void
test_port_visibility_through_report(void)
{
	static const char path[] = "SESSION-COMPUTER-PORT.DAT";
	static const uint8_t sector_number[] = "7\r";
	struct yt_door door;
	struct yt_session session;
	struct yt_player current;
	struct yt_player owner;
	struct yt_sector sector;
	struct yt_error error;
	bool enter_sector = true;

	(void)remove(path);
	memset(&door, 0, sizeof(door));
	memset(&session, 0, sizeof(session));
	memset(&current, 0, sizeof(current));
	memset(&owner, 0, sizeof(owner));
	memset(&sector, 0, sizeof(sector));
	session.door = &door;
	session.active_player_record = 2;
	session.planet.fallback_index = 52;
	session.pager.nonstop = -1.0f;
	door.game.config.sector_offset = 51.0f;
	door.game.config.port_offset = 2055.0f;
	door.game.config.planet_offset = 3055.0f;
	yt_record_blank(&current.record);
	current.team = 7.0f;
	yt_player_encode(&current);
	yt_record_blank(&owner.record);
	owner.team = 7.0f;
	yt_player_encode(&owner);
	yt_record_blank(&sector.record);
	sector.port = 0;
	sector.fighters = 10.0f;
	sector.fighter_owner = 3;
	yt_sector_encode(&sector);
	yt_error_clear(&error);
	CHECK(yt_database_open(&door.game.database, path, YT_OPEN_CREATE,
	    &error));
	CHECK(yt_database_write(&door.game.database, 2U, &current.record,
	    &error));
	CHECK(yt_database_write(&door.game.database, 3U, &owner.record,
	    &error));
	CHECK(yt_database_write_durable(&door.game.database, 58U,
	    &sector.record, &error));

	memcpy(session.io.typeahead, sector_number, sizeof(sector_number) - 1U);
	session.io.typeahead_length = sizeof(sector_number) - 1U;
	CHECK(yt_session_computer_port_report(&session, &enter_sector, &error));
	CHECK(!enter_sector);
	CHECK(session.io.typeahead_position == session.io.typeahead_length);
	CHECK(session.player_reference.friendly);
	CHECK(session.navigation.route_marker == 0.0f);
	CHECK(session.planet.current_record == 3107U);

	owner.team = 8.0f;
	yt_player_encode(&owner);
	CHECK(yt_database_write_durable(&door.game.database, 3U,
	    &owner.record, &error));
	memcpy(session.io.typeahead, sector_number, sizeof(sector_number) - 1U);
	session.io.typeahead_length = sizeof(sector_number) - 1U;
	session.io.typeahead_position = 0U;
	CHECK(yt_session_computer_port_report(&session, &enter_sector, &error));
	CHECK(!session.player_reference.friendly);

	sector.fighter_owner = 2;
	yt_sector_encode(&sector);
	CHECK(yt_database_write_durable(&door.game.database, 58U,
	    &sector.record, &error));
	memcpy(session.io.typeahead, sector_number, sizeof(sector_number) - 1U);
	session.io.typeahead_length = sizeof(sector_number) - 1U;
	session.io.typeahead_position = 0U;
	CHECK(yt_session_computer_port_report(&session, &enter_sector, &error));
	CHECK(session.player_reference.friendly);

	yt_database_close(&door.game.database);
	CHECK(remove(path) == 0);
}

int
main(void)
{
	session_test_runtime_start();
	test_activate_and_deactivate();
	test_port_visibility_through_report();
	session_test_runtime_stop();
	return failures == 0 ? 0 : 1;
}
