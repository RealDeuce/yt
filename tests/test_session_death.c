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
write_player(struct yt_game *game, int record, const char *name,
    float sector, float ports, struct yt_error *error)
{
	struct yt_player player;

	memset(&player, 0, sizeof(player));
	yt_record_blank(&player.record);
	(void)snprintf(player.name, sizeof(player.name), "%s", name);
	player.name_length = (float)strlen(name);
	player.sector = sector;
	player.ports_owned = ports;
	yt_player_encode(&player);
	CHECK(yt_game_write_player(game, record, &player, error));
}

static void
test_distinct_player_death(void)
{
	static const char database_path[] = "SESSION-DEATH.DAT";
	static const char news_path[] = "YTNEWS.DAT";
	static const uint8_t expected_news[] =
	    "  -  Killer killed Victim\r\n"
	    "  -  Took 1 ports from Victim\r\n\x1a";
	static const uint8_t cache_zero[4] = {0x00U, 0x00U, 0x7aU, 0x00U};
	struct yt_door door;
	struct yt_session session;
	struct yt_player player;
	struct yt_sector sector;
	struct yt_port port;
	struct yt_record record;
	struct yt_error error;
	uint8_t cached_sector[4];
	uint8_t news[sizeof(expected_news)];
	FILE *file;

	(void)remove(database_path);
	(void)remove(news_path);
	memset(&door, 0, sizeof(door));
	memset(&session, 0, sizeof(session));
	session.door = &door;
	session.active_player_record = 2;
	session.presentation.foreground = 6.0f;
	session.pager.foreground = 6;
	session.pager.nonstop = -1.0f;
	door.game.config.sector_offset = 4.0f;
	door.game.config.port_offset = 6.0f;
	door.game.config.planet_offset = 8.0f;
	yt_error_clear(&error);
	CHECK(yt_database_open(&door.game.database, database_path,
	    YT_OPEN_CREATE, &error));
	write_player(&door.game, 2, "Killer", 1.0f, 0.0f, &error);
	write_player(&door.game, 3, "Victim", 1.0f, 1.0f, &error);
	CHECK(yt_game_read_player(&door.game, 2, &session.player, &error));

	memset(&sector, 0, sizeof(sector));
	yt_record_blank(&sector.record);
	sector.fighters = 10.0f;
	sector.fighter_owner = 3.0f;
	yt_sector_encode(&sector);
	CHECK(yt_database_write(&door.game.database, 5U, &sector.record,
	    &error));
	memset(&sector, 0, sizeof(sector));
	yt_record_blank(&sector.record);
	sector.fighters = 20.0f;
	sector.fighter_owner = 4.0f;
	yt_sector_encode(&sector);
	CHECK(yt_database_write(&door.game.database, 6U, &sector.record,
	    &error));

	memset(&port, 0, sizeof(port));
	yt_record_blank(&port.record);
	port.owner = 3.0f;
	port.treasury = 99.0f;
	yt_port_encode(&port);
	CHECK(yt_database_write(&door.game.database, 7U, &port.record,
	    &error));
	memset(&port, 0, sizeof(port));
	yt_record_blank(&port.record);
	port.owner = 4.0f;
	yt_port_encode(&port);
	CHECK(yt_database_write_durable(&door.game.database, 8U, &port.record,
	    &error));

	CHECK(yt_session_kill_player(&session, 3, 2.0f, true, &error));
	CHECK(!session.fatal_wait_complete);
	CHECK(session.player.ports_owned == 0.0f);
	yt_player_cache_raw(&session.player_cache, 3,
	    YT_PLAYER_CACHE_SECTOR, cached_sector);
	CHECK(memcmp(cached_sector, cache_zero, sizeof(cache_zero)) == 0);
	CHECK(yt_game_read_player(&door.game, 3, &player, &error));
	CHECK(player.killed_by == 2.0f && player.sector == 0.0f
	    && player.ports_owned == 0.0f);
	CHECK(yt_game_read_player(&door.game, 2, &player, &error));
	CHECK(player.ports_owned == 1.0f);
	CHECK(yt_database_read(&door.game.database, 5U, &record, &error));
	yt_sector_decode(&sector, &record);
	CHECK(sector.fighters == 10.0f && sector.fighter_owner == -2.0f);
	CHECK(yt_database_read(&door.game.database, 6U, &record, &error));
	yt_sector_decode(&sector, &record);
	CHECK(sector.fighters == 20.0f && sector.fighter_owner == 4.0f);
	CHECK(yt_database_read(&door.game.database, 7U, &record, &error));
	yt_port_decode(&port, &record);
	CHECK(port.owner == 2.0f && port.last_minute == 2.0f
	    && port.treasury == 99.0f);
	CHECK(yt_database_read(&door.game.database, 8U, &record, &error));
	yt_port_decode(&port, &record);
	CHECK(port.owner == 4.0f);
	yt_database_close(&door.game.database);

	file = fopen(news_path, "rb");
	CHECK(file != NULL);
	if (file != NULL) {
		CHECK(fread(news, 1U, sizeof(expected_news) - 1U, file)
		    == sizeof(expected_news) - 1U);
		CHECK(fgetc(file) == EOF);
		CHECK(memcmp(news, expected_news,
		    sizeof(expected_news) - 1U) == 0);
		CHECK(fclose(file) == 0);
	}
	CHECK(remove(database_path) == 0);
	CHECK(remove(news_path) == 0);
}

int
main(void)
{
	session_test_runtime_start();
	test_distinct_player_death();
	session_test_runtime_stop();
	return failures == 0 ? 0 : 1;
}
