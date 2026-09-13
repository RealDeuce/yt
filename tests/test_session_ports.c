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
test_port_update(void)
{
	static const char path[] = "SESSION-PORT.DAT";
	struct yt_door door;
	struct yt_session session;
	struct yt_sector sector;
	struct yt_port port;
	struct yt_port_market_state market;
	struct yt_record persisted;
	struct yt_error error;
	int today;
	int adjusted_year;
	size_t index;

	(void)remove(path);
	memset(&door, 0, sizeof(door));
	memset(&session, 0, sizeof(session));
	memset(&sector, 0, sizeof(sector));
	memset(&port, 0, sizeof(port));
	session.door = &door;
	door.game.config.epoch_year = 26.0f;
	door.game.config.sector_offset = 51.0f;
	door.game.config.port_offset = 2055.0f;
	session.market_bases[0] = 20.0f;
	session.market_bases[1] = 30.0f;
	session.market_bases[2] = 40.0f;
	yt_error_clear(&error);
	CHECK(yt_current_date_serial(door.game.config.epoch_year, &today,
	    &adjusted_year, &error));

	yt_record_blank(&sector.record);
	sector.port = 2.0f;
	yt_sector_encode(&sector);
	yt_record_blank(&port.record);
	port.last_day = (float)today;
	port.last_minute = 0.0f;
	for (index = 0U; index < 3U; ++index) {
		port.stock[index] = (float)(100U * (index + 1U));
		port.production[index] = (float)(10U * (index + 1U));
		port.factor[index] = index == 1U ? 74.0f
		    : (index == 0U ? -60.0f : -66.0f);
	}
	yt_port_encode(&port);

	CHECK(yt_database_open(&door.game.database, path, YT_OPEN_CREATE,
	    &error));
	CHECK(yt_database_write(&door.game.database, 58U, &sector.record,
	    &error));
	CHECK(yt_database_write_durable(&door.game.database, 2057U,
	    &port.record, &error));
	CHECK(yt_session_update_port(&session, 7, NULL, NULL, &market,
	    &error));
	CHECK(market.logical_port == 2.0f);
	CHECK(market.port_physical_record == 2057U);
	CHECK(market.current_day == (float)today);
	CHECK(market.complete);
	CHECK(yt_database_read(&door.game.database, 2057U, &persisted,
	    &error));
	CHECK(memcmp(persisted.bytes, market.port.record.bytes,
	    sizeof(persisted.bytes)) == 0);
	CHECK(yt_record_get_number(&persisted, YT_F45) == (float)today);
	yt_database_close(&door.game.database);
	CHECK(remove(path) == 0);
}

static void
test_zero_capacity_trade(void)
{
	static const char path[] = "SESSION-TRADE.DAT";
	struct yt_door door;
	struct yt_session session;
	struct yt_player player;
	struct yt_port_market_state market;
	struct yt_error error;
	bool prompt_reached = true;

	(void)remove(path);
	memset(&door, 0, sizeof(door));
	memset(&session, 0, sizeof(session));
	memset(&player, 0, sizeof(player));
	memset(&market, 0, sizeof(market));
	session.door = &door;
	session.player_record_carrier = 2;
	market.port_physical_record = 2057U;
	market.port.factor[0] = 60.0f;
	market.price[0] = 20.0f;
	CHECK(qb_mbf64_encode(0.0, market.capacity_raw[0]) == QB_MBF_OK);
	yt_record_blank(&player.record);
	player.holds = 35.0f;
	player.ore = 10.0f;
	player.organics = 20.0f;
	player.equipment = 5.0f;
	player.credits = 12345.0f;
	yt_player_encode(&player);
	yt_error_clear(&error);
	CHECK(yt_database_open(&door.game.database, path, YT_OPEN_CREATE,
	    &error));
	CHECK(yt_database_write_durable(&door.game.database, 2U,
	    &player.record, &error));
	CHECK(yt_session_trade_commodity(&session, &market, 0U,
	    &prompt_reached, &error));
	CHECK(!prompt_reached);
	CHECK(session.player.credits == 12345.0f);
	CHECK(session.player.ore == 10.0f);
	yt_database_close(&door.game.database);
	CHECK(remove(path) == 0);
}

int
main(void)
{
	test_port_update();
	test_zero_capacity_trade();
	return failures == 0 ? 0 : 1;
}
