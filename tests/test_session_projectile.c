#include "yt_session_internal.h"
#include "session_test_runtime.h"

#include <stdio.h>
#include <string.h>

static int failures;

struct zero_random {
	size_t calls;
};

#define CHECK(expression) do { \
	if (!(expression)) { \
		fprintf(stderr, "check failed at %s:%d: %s\n", \
		    __FILE__, __LINE__, #expression); \
		++failures; \
	} \
} while (0)

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
test_same_sector_plasma_route(void)
{
	static const char path[] = "SESSION-PROJECTILE.DAT";
	static const uint8_t answers[] = "1\r1\r";
	struct zero_random random = {0};
	struct yt_door door;
	struct yt_session session;
	struct yt_player player;
	struct yt_sector sector;
	struct yt_error error;

	(void)remove(path);
	memset(&door, 0, sizeof(door));
	memset(&session, 0, sizeof(session));
	memset(&player, 0, sizeof(player));
	memset(&sector, 0, sizeof(sector));
	session.door = &door;
	session.active_player_record = 2;
	session.pager.nonstop = -1.0f;
	memcpy(session.io.typeahead, answers, sizeof(answers) - 1U);
	session.io.typeahead_length = sizeof(answers) - 1U;
	door.game.config.sector_offset = 3.0f;
	door.game.config.port_offset = 4.0f;
	yt_record_blank(&door.game.config.record);
	(void)snprintf(door.identity.real_first,
	    sizeof(door.identity.real_first), "%s", "Sysop");
	yt_random_init(&door.game.random);
	yt_random_set_provider(&door.game.random, fill_zero, &random);

	yt_record_blank(&player.record);
	(void)snprintf(player.name, sizeof(player.name), "%s", "Launcher");
	player.name_length = 8.0f;
	player.sector = 1.0f;
	player.turns = 10.0f;
	player.plasma = 1.0f;
	yt_player_encode(&player);
	session.player = player;
	yt_record_blank(&sector.record);
	yt_sector_encode(&sector);

	yt_error_clear(&error);
	CHECK(yt_database_open(&door.game.database, path, YT_OPEN_CREATE,
	    &error));
	CHECK(yt_database_write(&door.game.database, 1U,
	    &door.game.config.record, &error));
	CHECK(yt_database_write(&door.game.database, 2U, &player.record,
	    &error));
	CHECK(yt_database_write_durable(&door.game.database, 4U,
	    &sector.record, &error));

	CHECK(yt_session_command_projectile(&session, true, &error));
	CHECK(session.io.typeahead_position == session.io.typeahead_length);
	CHECK(random.calls == 1U && door.game.random.draws == 1U);
	CHECK(session.player.turns == 9.0f && session.player.plasma == 0.0f);
	CHECK(yt_game_read_player(&door.game, 2, &player, &error));
	CHECK(player.turns == 9.0f && player.plasma == 0.0f);

	yt_database_close(&door.game.database);
	CHECK(remove(path) == 0);
}

int
main(void)
{
	session_test_runtime_start();
	test_same_sector_plasma_route();
	session_test_runtime_stop();
	return failures == 0 ? 0 : 1;
}
