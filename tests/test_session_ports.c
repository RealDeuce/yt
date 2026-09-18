#include "yt_session_internal.h"
#include "qb.h"
#include "session_test_runtime.h"

#include "yt_file.h"
#include "yt_platform.h"
#include "yt_port_math.h"

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
	door.game.config.epoch_year = 26U;
	door.game.config.sector_offset = 51.0f;
	door.game.config.port_offset = 2055.0f;
	session.market_bases[0] = 20.0f;
	session.market_bases[1] = 30.0f;
	session.market_bases[2] = 40.0f;
	yt_error_clear(&error);
	CHECK(yt_current_date_serial(&door.game.clock,
	    door.game.config.epoch_year, &today,
	    &adjusted_year, &error));

	yt_record_blank(&sector.record);
	sector.port = 2;
	yt_sector_encode(&sector);
	yt_record_blank(&port.record);
	port.last_day = (int16_t)today;
	port.last_minute = 0.0f;
	for (index = 0U; index < 3U; ++index) {
		port.stock[index] = (float)(100U * (index + 1U));
		port.production[index] = (float)(10U * (index + 1U));
		port.factor[index] = index == 1U ? 74
		    : (index == 0U ? -60 : -66);
	}
	yt_port_encode(&port);

	CHECK(yt_database_open(&door.game.database, path, YT_OPEN_CREATE,
	    &error));
	CHECK(yt_database_write(&door.game.database, 58U, &sector.record,
	    &error));
	CHECK(yt_database_write_durable(&door.game.database, 2057U,
	    &port.record, &error));
	CHECK(yt_session_update_port(&session, 7, NULL, &market, &error));
	CHECK(market.logical_port == 2);
	CHECK(market.port_physical_record == 2057U);
	CHECK(market.current_day == (int16_t)today);
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
	session.active_player_record = 2;
	market.port_physical_record = 2057U;
	market.port.factor[0] = 60;
	market.price[0] = 20U;
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

static void
test_no_port_purchase(void)
{
	static const char path[] = "SESSION-BUY-PORT.DAT";
	struct yt_door door;
	struct yt_session session;
	struct yt_player player;
	struct yt_sector sector;
	struct yt_record persisted;
	struct yt_error error;

	(void)remove(path);
	memset(&door, 0, sizeof(door));
	memset(&session, 0, sizeof(session));
	memset(&player, 0, sizeof(player));
	memset(&sector, 0, sizeof(sector));
	session.door = &door;
	session.active_player_record = 2;
	session.pager.nonstop = true;
	door.game.config.sector_offset = 100.0f;
	door.game.config.port_offset = 200.0f;
	(void)snprintf(door.identity.real_first,
	    sizeof(door.identity.real_first), "%s", "Pat");
	yt_record_blank(&player.record);
	player.sector = 9.0f;
	player.credits = 1000.0f;
	yt_player_encode(&player);
	yt_record_blank(&sector.record);
	memcpy(sector.record.bytes + YT_F65, "\x01\x02\x03\0", 4U);
	yt_error_clear(&error);
	CHECK(yt_database_open(&door.game.database, path, YT_OPEN_CREATE,
	    &error));
	CHECK(yt_database_write(&door.game.database, 2U, &player.record,
	    &error));
	CHECK(yt_database_write_durable(&door.game.database, 109U,
	    &sector.record, &error));
	CHECK(yt_session_command_buy_port(&session, &error));
	CHECK(session.player.sector == 9);
	CHECK(session.player.credits == 1000.0f);
	CHECK(yt_database_read(&door.game.database, 2U, &persisted, &error));
	CHECK(memcmp(persisted.bytes, player.record.bytes,
	    sizeof(persisted.bytes)) == 0);
	yt_database_close(&door.game.database);
	CHECK(remove(path) == 0);
}

static void
test_owned_port_purchase(void)
{
	static const char path[] = "SESSION-BUY-OWNED-PORT.DAT";
	static const char answers[] = "Y\rNova\rY\r";
	struct yt_door door;
	struct yt_session session;
	struct yt_player buyer;
	struct yt_player seller;
	struct yt_sector sector;
	struct yt_port port;
	struct yt_record persisted;
	struct yt_error error;
	int today;
	int adjusted_year;
	size_t index;

	(void)remove(path);
	(void)remove("YTRMSG.DAT");
	memset(&door, 0, sizeof(door));
	memset(&session, 0, sizeof(session));
	memset(&buyer, 0, sizeof(buyer));
	memset(&seller, 0, sizeof(seller));
	memset(&sector, 0, sizeof(sector));
	memset(&port, 0, sizeof(port));
	session.door = &door;
	session.active_player_record = 2;
	session.pager.nonstop = true;
	door.game.config.epoch_year = 26U;
	door.game.config.sector_offset = 100.0f;
	door.game.config.port_offset = 200.0f;
	session.market_bases[0] = 20.0f;
	session.market_bases[1] = 30.0f;
	session.market_bases[2] = 40.0f;
	(void)snprintf(door.identity.real_first,
	    sizeof(door.identity.real_first), "%s", "Pat");
	memcpy(session.io.typeahead, answers, sizeof(answers) - 1U);
	session.io.typeahead_length = sizeof(answers) - 1U;

	yt_record_blank(&buyer.record);
	(void)snprintf(buyer.name, sizeof(buyer.name), "%s", "Pat");
	buyer.name_length = 3U;
	buyer.sector = 9.0f;
	buyer.credits = 1000.0f;
	buyer.ports_owned = 1;
	yt_player_encode(&buyer);
	yt_record_blank(&seller.record);
	(void)snprintf(seller.name, sizeof(seller.name), "%s", "Seller");
	seller.name_length = 6U;
	seller.credits = 10.0f;
	seller.ports_owned = 3;
	yt_player_encode(&seller);
	yt_record_blank(&sector.record);
	sector.port = 3;
	yt_sector_encode(&sector);
	CHECK(yt_current_date_serial(&door.game.clock,
	    door.game.config.epoch_year, &today,
	    &adjusted_year, &error));
	yt_record_blank(&port.record);
	(void)snprintf(port.name, sizeof(port.name), "%s", "Old Port");
	port.name_length = 8U;
	port.last_day = (int16_t)today;
	port.last_minute = qb_single_divide((float)yt_clock_timer(
	    &door.game.clock),
	    60.0f);
	port.treasury = 4.0f;
	port.sector = 9;
	port.owner = 7;
	for (index = 0U; index < 3U; ++index) {
		port.stock[index] = (float)(100U * (index + 1U));
		port.production[index] = (float)(10U * (index + 1U));
		port.factor[index] = index == 0U ? 60 : -60;
	}
	yt_port_encode(&port);
	yt_error_clear(&error);
	CHECK(yt_database_open(&door.game.database, path, YT_OPEN_CREATE,
	    &error));
	CHECK(yt_database_write(&door.game.database, 2U, &buyer.record,
	    &error));
	CHECK(yt_database_write(&door.game.database, 7U, &seller.record,
	    &error));
	CHECK(yt_database_write(&door.game.database, 109U, &sector.record,
	    &error));
	CHECK(yt_database_write_durable(&door.game.database, 203U,
	    &port.record, &error));
	CHECK(yt_session_command_buy_port(&session, &error));
	CHECK(session.io.typeahead_position == session.io.typeahead_length);
	CHECK(yt_database_read(&door.game.database, 7U, &persisted, &error));
	yt_player_decode(&seller, &persisted);
	CHECK(seller.credits == 21.0f);
	CHECK(seller.ports_owned == 2);
	CHECK(yt_database_read(&door.game.database, 2U, &persisted, &error));
	yt_player_decode(&buyer, &persisted);
	CHECK(buyer.credits == 993.0f);
	CHECK(buyer.ports_owned == 2);
	CHECK(yt_database_read(&door.game.database, 203U, &persisted, &error));
	yt_port_decode(&port, &persisted);
	CHECK(port.owner == 2);
	CHECK(port.treasury == 0.0f);
	CHECK(port.name_length == 4U);
	CHECK(memcmp(port.record.bytes, "Nova", 4U) == 0);
	yt_database_close(&door.game.database);
	CHECK(remove(path) == 0);
	CHECK(remove("YTRMSG.DAT") == 0);
}

static void
test_player_friendship(void)
{
	static const char path[] = "SESSION-FRIENDSHIP.DAT";
	struct yt_door door;
	struct yt_session session;
	struct yt_player current;
	struct yt_player candidate;
	struct yt_error error;
	bool friendly;

	(void)remove(path);
	memset(&door, 0, sizeof(door));
	memset(&session, 0, sizeof(session));
	memset(&current, 0, sizeof(current));
	memset(&candidate, 0, sizeof(candidate));
	session.door = &door;
	session.active_player_record = 2;
	door.game.config.sector_offset = 51.0f;
	yt_record_blank(&current.record);
	current.team = 7;
	yt_player_encode(&current);
	yt_record_blank(&candidate.record);
	candidate.team = 7;
	yt_player_encode(&candidate);
	yt_error_clear(&error);
	CHECK(yt_database_open(&door.game.database, path, YT_OPEN_CREATE,
	    &error));
	CHECK(yt_database_write(&door.game.database, 2U, &current.record,
	    &error));
	CHECK(yt_database_write_durable(&door.game.database, 3U,
	    &candidate.record, &error));

	CHECK(yt_session_players_are_friendly(&session, 3, &friendly,
	    &error));
	CHECK(friendly);
	candidate.team = 8;
	yt_player_encode(&candidate);
	CHECK(yt_database_write_durable(&door.game.database, 3U,
	    &candidate.record, &error));
	CHECK(yt_session_players_are_friendly(&session, 3, &friendly,
	    &error));
	CHECK(!friendly);
	CHECK(yt_session_players_are_friendly(&session, 2, &friendly,
	    &error));
	CHECK(friendly);
	CHECK(yt_session_players_are_friendly(&session, 1, &friendly,
	    &error));
	CHECK(!friendly);
	CHECK(yt_session_players_are_friendly(&session, 52, &friendly,
	    &error));
	CHECK(!friendly);

	current.team = 0;
	yt_player_encode(&current);
	CHECK(yt_database_write_durable(&door.game.database, 2U,
	    &current.record, &error));
	CHECK(yt_session_players_are_friendly(&session, 4, &friendly,
	    &error));
	CHECK(!friendly);

	yt_database_close(&door.game.database);
	CHECK(remove(path) == 0);
}

int
main(void)
{
	session_test_runtime_start();
	test_port_update();
	test_zero_capacity_trade();
	test_no_port_purchase();
	test_owned_port_purchase();
	test_player_friendship();
	session_test_runtime_stop();
	return failures == 0 ? 0 : 1;
}
