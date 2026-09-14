#include "yt_session_internal.h"

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
write_player(struct yt_game *game, int record, const char *name,
    float fighters, float shields, struct yt_error *error)
{
	struct yt_player player;

	memset(&player, 0, sizeof(player));
	yt_record_blank(&player.record);
	(void)snprintf(player.name, sizeof(player.name), "%s", name);
	player.name_length = (float)strlen(name);
	player.sector = 1.0f;
	player.fighters = fighters;
	player.shields = shields;
	yt_player_encode(&player);
	CHECK(yt_game_write_player(game, record, &player, error));
}

static void
test_no_fighters(void)
{
	static const char path[] = "SESSION-ATTACK.DAT";
	struct yt_door door;
	struct yt_session session;
	struct yt_player source;
	struct yt_record persisted;
	struct yt_error error;
	bool enter_sector = true;

	(void)remove(path);
	memset(&door, 0, sizeof(door));
	memset(&session, 0, sizeof(session));
	memset(&source, 0, sizeof(source));
	session.door = &door;
	session.player_record_carrier = 2;
	session.pager.nonstop = -1.0f;
	door.game.config.sector_offset = 3.0f;
	yt_record_blank(&door.game.config.record);
	yt_record_blank(&source.record);
	(void)snprintf(source.name, sizeof(source.name), "%s", "Unarmed");
	source.name_length = 7.0f;
	source.sector = 1.0f;
	source.fighters = 0.0f;
	yt_player_encode(&source);

	yt_error_clear(&error);
	CHECK(yt_database_open(&door.game.database, path, YT_OPEN_CREATE,
	    &error));
	CHECK(yt_database_write(&door.game.database, 1U,
	    &door.game.config.record, &error));
	CHECK(yt_database_write_durable(&door.game.database, 2U,
	    &source.record, &error));
	CHECK(yt_session_command_attack(&session, &enter_sector, &error));
	CHECK(!enter_sector);
	CHECK(session.player.fighters == 0.0f);
	CHECK(yt_database_read(&door.game.database, 2U, &persisted, &error));
	CHECK(memcmp(persisted.bytes, source.record.bytes,
	    sizeof(persisted.bytes)) == 0);
	yt_database_close(&door.game.database);
	CHECK(remove(path) == 0);
}

static void
test_combat_attrition(void)
{
	static const char path[] = "SESSION-ATTACK-COMBAT.DAT";
	struct zero_random random = {0};
	struct yt_door door;
	struct yt_session session;
	struct yt_player player;
	struct yt_error error;

	(void)remove(path);
	memset(&door, 0, sizeof(door));
	memset(&session, 0, sizeof(session));
	session.door = &door;
	session.player_record_carrier = 2;
	session.pager.nonstop = -1.0f;
	door.game.config.sector_offset = 3.0f;
	yt_record_blank(&door.game.config.record);
	yt_random_set_provider(&door.game.random, fill_zero, &random);

	yt_error_clear(&error);
	CHECK(yt_database_open(&door.game.database, path, YT_OPEN_CREATE,
	    &error));
	CHECK(yt_database_write(&door.game.database, 1U,
	    &door.game.config.record, &error));
	write_player(&door.game, 2, "Attacker", 5.0f, 8.0f, &error);
	write_player(&door.game, 3, "Target", 2.0f, 10.0f, &error);
	CHECK(yt_database_flush(&door.game.database, &error));

	CHECK(yt_session_attack_player(&session, 3, 3.0, &error));
	CHECK(random.calls == 3U);
	CHECK(door.game.random.draws == 3U);
	CHECK(yt_game_read_player(&door.game, 2, &player, &error));
	CHECK(player.fighters == 2.0f);
	CHECK(player.shields == 8.0f);
	CHECK(yt_game_read_player(&door.game, 3, &player, &error));
	CHECK(player.fighters == 2.0f);
	CHECK(player.shields == 10.0f);
	CHECK(yt_database_flush(&door.game.database, &error));
	yt_database_close(&door.game.database);

	CHECK(yt_database_open(&door.game.database, path, YT_OPEN_READ,
	    &error));
	CHECK(yt_game_read_player(&door.game, 2, &player, &error));
	CHECK(player.fighters == 2.0f);
	CHECK(yt_game_read_player(&door.game, 3, &player, &error));
	CHECK(player.fighters == 2.0f);
	yt_database_close(&door.game.database);
	CHECK(remove(path) == 0);
}

int
main(void)
{
	test_no_fighters();
	test_combat_attrition();
	return failures == 0 ? 0 : 1;
}
