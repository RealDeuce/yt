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
prepare_port(struct yt_port *port, int commodity_class,
    const float factor[3])
{
	size_t index;

	memset(port, 0, sizeof(*port));
	yt_record_blank(&port->record);
	port->commodity_class = commodity_class;
	for (index = 0U; index < 3U; ++index) {
		port->stock[index] = 1000.0f;
		port->production[index] = 100.0f;
		port->factor[index] = factor[index];
	}
	yt_port_encode(port);
}

static void
test_adjacent_and_global_reports(void)
{
	static const char path[] = "SESSION-PROFIT.DAT";
	static const float source_factor[3] = {-20.0f, -30.0f, 50.0f};
	static const float target_factor[3] = {-40.0f, 50.0f, -25.0f};
	struct yt_door door;
	struct yt_session session;
	struct yt_sector earth;
	struct yt_sector source;
	struct yt_sector target;
	struct yt_port earth_port;
	struct yt_port source_port;
	struct yt_port target_port;
	struct yt_record before[6];
	struct yt_record after;
	struct yt_error error;
	size_t index;

	(void)remove(path);
	memset(&door, 0, sizeof(door));
	memset(&session, 0, sizeof(session));
	memset(&earth, 0, sizeof(earth));
	memset(&source, 0, sizeof(source));
	memset(&target, 0, sizeof(target));
	session.door = &door;
	session.active_player_record = 2;
	session.navigation.current_sector_physical_record = 4;
	session.presentation.foreground = 1;
	session.market_bases[0] = 20.0f;
	session.market_bases[1] = 30.0f;
	session.market_bases[2] = 40.0f;
	door.game.config.epoch_year = 26.0f;
	door.game.config.sector_offset = 2.0f;
	door.game.config.port_offset = 5.0f;
	yt_record_blank(&door.game.config.record);

	yt_record_blank(&earth.record);
	earth.port = 1;
	yt_sector_encode(&earth);
	yt_record_blank(&source.record);
	source.port = 2;
	source.warps[0] = 3;
	yt_sector_encode(&source);
	yt_record_blank(&target.record);
	target.port = 3;
	target.warps[0] = 2;
	yt_sector_encode(&target);
	prepare_port(&earth_port, 1.0f, source_factor);
	prepare_port(&source_port, 1.0f, source_factor);
	prepare_port(&target_port, 2.0f, target_factor);

	before[0] = earth.record;
	before[1] = source.record;
	before[2] = target.record;
	before[3] = earth_port.record;
	before[4] = source_port.record;
	before[5] = target_port.record;
	yt_error_clear(&error);
	CHECK(yt_database_open(&door.game.database, path, YT_OPEN_CREATE,
	    &error));
	CHECK(yt_database_write(&door.game.database, 1U,
	    &door.game.config.record, &error));
	for (index = 0U; index < 6U; ++index)
		CHECK(yt_database_write(&door.game.database, index + 3U,
		    &before[index], &error));
	CHECK(yt_database_flush(&door.game.database, &error));

	CHECK(yt_session_computer_profit(&session, false, &error));
	CHECK(session.presentation.foreground == 3);
	CHECK(door.game.today != 0);
	CHECK(yt_session_computer_profit(&session, true, &error));
	CHECK(session.presentation.foreground == 7);
	for (index = 0U; index < 6U; ++index) {
		CHECK(yt_database_read(&door.game.database, index + 3U,
		    &after, &error));
		CHECK(memcmp(after.bytes, before[index].bytes,
		    sizeof(after.bytes)) == 0);
	}

	yt_database_close(&door.game.database);
	CHECK(remove(path) == 0);
}

int
main(void)
{
	session_test_runtime_start();
	test_adjacent_and_global_reports();
	session_test_runtime_stop();
	return failures == 0 ? 0 : 1;
}
