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

struct zero_random {
	size_t calls;
};

static bool
fill_zero(void *context, void *buffer, size_t length,
    struct yt_error *error)
{
	struct zero_random *random = context;

	(void)error;
	memset(buffer, 0, length);
	++random->calls;
	return true;
}

static void
test_shielded_encounter(void)
{
	static const char database_path[] = "SESSION-MINES.DAT";
	static const char news_path[] = "YTNEWS.DAT";
	static const uint8_t expected_news[] =
	    "Ada hit sector mines in sector 42!\r\n"
	    "Shields reduced to 100 units!\r\n\x1a";
	struct yt_door door;
	struct yt_session session;
	struct yt_player player;
	struct yt_sector sector;
	struct yt_record record;
	struct yt_error error;
	struct zero_random random = {0};
	uint8_t news[sizeof(expected_news)];
	FILE *file;
	bool terminal = true;

	(void)remove(database_path);
	(void)remove(news_path);
	memset(&door, 0, sizeof(door));
	memset(&session, 0, sizeof(session));
	memset(&player, 0, sizeof(player));
	memset(&sector, 0, sizeof(sector));
	session.door = &door;
	session.active_player_record = 2;
	session.presentation.foreground = 6.0f;
	session.pager.foreground = 6;
	session.pager.nonstop = -1.0f;
	door.game.config.sector_offset = 51.0f;
	yt_random_init(&door.game.random);
	yt_random_set_provider(&door.game.random, fill_zero, &random);

	yt_record_blank(&player.record);
	memcpy(player.name, "Ada", 4U);
	player.name_length = 3U;
	player.sector = 42.0f;
	player.shields = 100.0f;
	player.holds = 20.0f;
	yt_player_encode(&player);
	session.player = player;
	yt_record_blank(&sector.record);
	sector.mines = 1.0f;
	yt_sector_encode(&sector);
	yt_error_clear(&error);
	CHECK(yt_database_open(&door.game.database, database_path,
	    YT_OPEN_CREATE, &error));
	CHECK(yt_database_write(&door.game.database, 2U, &player.record,
	    &error));
	CHECK(yt_database_write_durable(&door.game.database, 93U,
	    &sector.record, &error));
	CHECK(yt_session_mine_encounter(&session, &terminal, &error));
	CHECK(!terminal);
	CHECK(!session.destroyed);
	CHECK(random.calls == 3U && door.game.random.draws == 3U);
	CHECK(session.player.shields == 100.0f);
	CHECK(session.presentation.foreground == 3.0f);
	CHECK(yt_present_background(&session.presentation) == 1.0f);
	CHECK(yt_present_blink(&session.presentation) == 0.0f);
	CHECK(yt_database_read(&door.game.database, 93U, &record, &error));
	yt_sector_decode(&sector, &record);
	CHECK(sector.mines == 0.0f);
	CHECK(yt_database_read(&door.game.database, 2U, &record, &error));
	yt_player_decode(&player, &record);
	CHECK(player.shields == 100.0f && player.holds == 20.0f);
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

static void
test_unshielded_missile_draw(void)
{
	static const char database_path[] = "SESSION-MISSILES.DAT";
	static const char news_path[] = "YTNEWS.DAT";
	struct yt_door door;
	struct yt_session session;
	struct yt_player player;
	struct yt_sector sector;
	struct yt_record record;
	struct yt_error error;
	struct zero_random random = {0};
	bool terminal = true;

	(void)remove(database_path);
	(void)remove(news_path);
	memset(&door, 0, sizeof(door));
	memset(&session, 0, sizeof(session));
	memset(&player, 0, sizeof(player));
	memset(&sector, 0, sizeof(sector));
	session.door = &door;
	session.active_player_record = 2;
	session.presentation.foreground = 6.0f;
	session.pager.foreground = 6;
	session.pager.nonstop = -1.0f;
	door.game.config.sector_offset = 51.0f;
	yt_random_init(&door.game.random);
	yt_random_set_provider(&door.game.random, fill_zero, &random);

	yt_record_blank(&player.record);
	memcpy(player.name, "Max", 4U);
	player.name_length = 3U;
	player.sector = 42.0f;
	player.missiles = 3.0f;
	yt_player_encode(&player);
	session.player = player;
	yt_record_blank(&sector.record);
	sector.mines = 1.0f;
	yt_sector_encode(&sector);
	yt_error_clear(&error);
	CHECK(yt_database_open(&door.game.database, database_path,
	    YT_OPEN_CREATE, &error));
	CHECK(yt_database_write(&door.game.database, 2U, &player.record,
	    &error));
	CHECK(yt_database_write_durable(&door.game.database, 93U,
	    &sector.record, &error));
	CHECK(yt_session_mine_encounter(&session, &terminal, &error));
	CHECK(!terminal && !session.destroyed);
	CHECK(random.calls == 2U && door.game.random.draws == 2U);
	CHECK(yt_database_read(&door.game.database, 2U, &record, &error));
	yt_player_decode(&player, &record);
	CHECK(player.missiles == 2.0f);
	CHECK(yt_database_read(&door.game.database, 93U, &record, &error));
	yt_sector_decode(&sector, &record);
	CHECK(sector.mines == 0.0f);
	yt_database_close(&door.game.database);
	CHECK(remove(database_path) == 0);
	CHECK(remove(news_path) == 0);
}

int
main(void)
{
	session_test_runtime_start();
	test_shielded_encounter();
	test_unshielded_missile_draw();
	session_test_runtime_stop();
	return failures == 0 ? 0 : 1;
}
