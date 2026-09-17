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
test_one_sector_all_ports(void)
{
	static const char path[] = "SESSION-NEAREST.DAT";
	static const uint8_t answer[] = "A\r";
	struct yt_door door;
	struct yt_session session;
	struct yt_player player;
	struct yt_sector sector;
	struct yt_port port;
	struct yt_record persisted_player;
	struct yt_record persisted_sector;
	struct yt_record persisted_port;
	struct yt_error error;

	(void)remove(path);
	memset(&door, 0, sizeof(door));
	memset(&session, 0, sizeof(session));
	memset(&player, 0, sizeof(player));
	memset(&sector, 0, sizeof(sector));
	memset(&port, 0, sizeof(port));
	session.door = &door;
	session.active_player_record = 2;
	session.pager.nonstop = true;
	memcpy(session.io.typeahead, answer, sizeof(answer) - 1U);
	session.io.typeahead_length = sizeof(answer) - 1U;
	session.market_bases[0] = 100.0f;
	session.market_bases[1] = 100.0f;
	session.market_bases[2] = 100.0f;
	door.game.config.epoch_year = 26.0f;
	door.game.config.sector_offset = 3.0f;
	door.game.config.port_offset = 4.0f;
	yt_record_blank(&door.game.config.record);

	yt_record_blank(&player.record);
	(void)snprintf(player.name, sizeof(player.name), "%s", "Scanner");
	player.name_length = 7U;
	player.sector = 1.0f;
	yt_player_encode(&player);
	yt_record_blank(&sector.record);
	sector.port = 1;
	yt_sector_encode(&sector);
	yt_record_blank(&port.record);
	(void)snprintf(port.name, sizeof(port.name), "%s", "Earth");
	port.name_length = 5U;
	port.commodity_class = 1;
	port.sector = 1;
	port.production[0] = 10.0f;
	port.production[1] = 10.0f;
	port.production[2] = 10.0f;
	port.stock[0] = 100.0f;
	port.stock[1] = 100.0f;
	port.stock[2] = 100.0f;
	yt_port_encode(&port);

	yt_error_clear(&error);
	CHECK(yt_database_open(&door.game.database, path, YT_OPEN_CREATE,
	    &error));
	CHECK(yt_database_write(&door.game.database, 1U,
	    &door.game.config.record, &error));
	CHECK(yt_database_write(&door.game.database, 2U, &player.record,
	    &error));
	CHECK(yt_database_write(&door.game.database, 4U, &sector.record,
	    &error));
	CHECK(yt_database_write_durable(&door.game.database, 5U, &port.record,
	    &error));

	CHECK(yt_session_computer_nearest_ports(&session, &error));
	CHECK(session.io.typeahead_position == session.io.typeahead_length);
	CHECK(session.player.sector == 1);
	CHECK(door.game.today != 0);
	CHECK(yt_database_read(&door.game.database, 2U, &persisted_player,
	    &error));
	CHECK(yt_database_read(&door.game.database, 4U, &persisted_sector,
	    &error));
	CHECK(yt_database_read(&door.game.database, 5U, &persisted_port,
	    &error));
	CHECK(memcmp(persisted_player.bytes, player.record.bytes,
	    sizeof(persisted_player.bytes)) == 0);
	CHECK(memcmp(persisted_sector.bytes, sector.record.bytes,
	    sizeof(persisted_sector.bytes)) == 0);
	CHECK(memcmp(persisted_port.bytes, port.record.bytes,
	    sizeof(persisted_port.bytes)) == 0);

	yt_database_close(&door.game.database);
	CHECK(remove(path) == 0);
}

int
main(void)
{
	session_test_runtime_start();
	test_one_sector_all_ports();
	session_test_runtime_stop();
	return failures == 0 ? 0 : 1;
}
