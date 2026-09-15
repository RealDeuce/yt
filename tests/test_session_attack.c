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

static bool
fill_max(void *context, void *buffer, size_t length,
    struct yt_error *error)
{
	struct zero_random *random = context;

	(void)error;
	memset(buffer, 0xff, length);
	++random->calls;
	return true;
}

static void
write_player(struct yt_game *game, int record, const char *name,
    float fighters, float shields, float credits, struct yt_error *error)
{
	struct yt_player player;

	memset(&player, 0, sizeof(player));
	yt_record_blank(&player.record);
	(void)snprintf(player.name, sizeof(player.name), "%s", name);
	player.name_length = (float)strlen(name);
	player.sector = 1.0f;
	player.fighters = fighters;
	player.shields = shields;
	player.credits = credits;
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
	session.active_player_record = 2;
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
	session.active_player_record = 2;
	session.pager.nonstop = -1.0f;
	door.game.config.sector_offset = 3.0f;
	yt_record_blank(&door.game.config.record);
	yt_random_set_provider(&door.game.random, fill_zero, &random);

	yt_error_clear(&error);
	CHECK(yt_database_open(&door.game.database, path, YT_OPEN_CREATE,
	    &error));
	CHECK(yt_database_write(&door.game.database, 1U,
	    &door.game.config.record, &error));
	write_player(&door.game, 2, "Attacker", 5.0f, 8.0f, 0.0f, &error);
	write_player(&door.game, 3, "Target", 2.0f, 10.0f, 0.0f, &error);
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

static void
test_deployed_surrender(void)
{
	static const char database_path[] = "SESSION-ATTACK-SURRENDER.DAT";
	static const char news_path[] = "YTNEWS.DAT";
	static const uint8_t expected_news[] =
	    "Attacker destroyed 10 fighters belonging to Mercenaries\r\n\x1a";
	struct zero_random random = {0};
	struct yt_door door;
	struct yt_session session;
	struct yt_player player;
	struct yt_sector sector;
	struct yt_error error;
	uint8_t news[sizeof(expected_news)];
	FILE *file;

	(void)remove(database_path);
	(void)remove(news_path);
	memset(&door, 0, sizeof(door));
	memset(&session, 0, sizeof(session));
	session.door = &door;
	session.active_player_record = 2;
	session.pager.nonstop = -1.0f;
	session.hostile_deployed_fighters = 10.0;
	session.combat_ship_fighters = 11.0;
	session.combat_ship_shields = 7.0f;
	memcpy(session.hostile_owner_label, "Mercenaries", 11U);
	session.hostile_owner_label_length = 11U;
	door.game.config.sector_offset = 3.0f;
	door.game.config.turns_per_day = 100.0f;
	door.game.config.headquarters = 7.0f;
	(void)snprintf(door.identity.real_first,
	    sizeof(door.identity.real_first), "%s", "Sysop");
	yt_random_init(&door.game.random);
	yt_random_set_provider(&door.game.random, fill_max, &random);

	yt_error_clear(&error);
	CHECK(yt_database_open(&door.game.database, database_path,
	    YT_OPEN_CREATE, &error));
	write_player(&door.game, 2, "Attacker", 11.0f, 7.0f, 0.0f, &error);
	CHECK(yt_game_read_player(&door.game, 2, &session.player, &error));
	memset(&sector, 0, sizeof(sector));
	yt_record_blank(&sector.record);
	sector.fighters = 10.0f;
	sector.fighter_owner = -2.0f;
	yt_sector_encode(&sector);
	CHECK(yt_database_write_durable(&door.game.database, 4U,
	    &sector.record, &error));

	CHECK(yt_session_attack_deployed(&session, &sector, 120.0, true,
	    &error));
	CHECK(random.calls == 11U && door.game.random.draws == 11U);
	CHECK(session.shared_status == 1.0f);
	CHECK(session.combat_ship_fighters == 11.0);
	CHECK(session.hostile_deployed_fighters == 0.0);
	CHECK(session.player.fighters == 11.0f);
	CHECK(session.mercenaries_hurt);
	CHECK(sector.fighters == 0.0f && sector.fighter_owner == 0.0f);
	CHECK(yt_game_read_player(&door.game, 2, &player, &error));
	CHECK(player.fighters == 11.0f && player.shields == 7.0f);
	CHECK(session_read_sector(&session, 1, &sector, &error));
	CHECK(sector.fighters == 0.0f && sector.fighter_owner == 0.0f);
	CHECK(yt_database_flush(&door.game.database, &error));
	yt_database_close(&door.game.database);

	CHECK(yt_database_open(&door.game.database, database_path,
	    YT_OPEN_READ, &error));
	CHECK(yt_game_read_player(&door.game, 2, &player, &error));
	CHECK(player.fighters == 11.0f && player.shields == 7.0f);
	CHECK(session_read_sector(&session, 1, &sector, &error));
	CHECK(sector.fighters == 0.0f && sector.fighter_owner == 0.0f);
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
test_accepted_bribe(void)
{
	static const char path[] = "SESSION-BRIBE.DAT";
	static const uint8_t answer[] = "40\r";
	struct zero_random random = {0};
	struct yt_door door;
	struct yt_session session;
	struct yt_player player;
	struct yt_sector sector;
	struct yt_error error;
	bool direct_hostile_menu = true;
	bool forced_attack = true;

	(void)remove(path);
	memset(&door, 0, sizeof(door));
	memset(&session, 0, sizeof(session));
	session.door = &door;
	session.active_player_record = 2;
	session.pager.nonstop = -1.0f;
	session.hostile_owner = -2.0f;
	session.hostile_deployed_fighters = 10.0;
	session.combat_ship_fighters = 20.0;
	session.combat_ship_shields = 7.0f;
	memcpy(session.queue, answer, sizeof(answer) - 1U);
	session.queue_length = sizeof(answer) - 1U;
	door.game.config.sector_offset = 3.0f;
	(void)snprintf(door.identity.real_first,
	    sizeof(door.identity.real_first), "%s", "Sysop");
	yt_random_init(&door.game.random);
	yt_random_set_provider(&door.game.random, fill_max, &random);

	yt_error_clear(&error);
	CHECK(yt_database_open(&door.game.database, path, YT_OPEN_CREATE,
	    &error));
	write_player(&door.game, 2, "Trader", 20.0f, 7.0f, 100.0f, &error);
	CHECK(yt_game_read_player(&door.game, 2, &session.player, &error));
	memset(&sector, 0, sizeof(sector));
	yt_record_blank(&sector.record);
	sector.fighters = 10.0f;
	sector.fighter_owner = -2.0f;
	yt_sector_encode(&sector);
	CHECK(yt_database_write_durable(&door.game.database, 4U,
	    &sector.record, &error));

	CHECK(yt_session_bribe_deployed(&session, &sector,
	    &direct_hostile_menu, &forced_attack, &error));
	CHECK(!direct_hostile_menu && !forced_attack);
	CHECK(random.calls == 3U && door.game.random.draws == 3U);
	CHECK(session.queue_position == session.queue_length);
	CHECK(session.player.fighters == 20.0f);
	CHECK(session.player.credits == 100.0f);
	CHECK(yt_game_read_player(&door.game, 2, &player, &error));
	CHECK(player.fighters == 30.0f);
	CHECK(player.credits == 60.0f);
	CHECK(session_read_sector(&session, 1, &sector, &error));
	CHECK(sector.fighters == 0.0f && sector.fighter_owner == 0.0f);
	CHECK(yt_database_flush(&door.game.database, &error));
	yt_database_close(&door.game.database);

	CHECK(yt_database_open(&door.game.database, path, YT_OPEN_READ,
	    &error));
	CHECK(yt_game_read_player(&door.game, 2, &player, &error));
	CHECK(player.fighters == 30.0f && player.credits == 60.0f);
	CHECK(session_read_sector(&session, 1, &sector, &error));
	CHECK(sector.fighters == 0.0f && sector.fighter_owner == 0.0f);
	yt_database_close(&door.game.database);
	CHECK(remove(path) == 0);
}

int
main(void)
{
	session_test_runtime_start();
	test_no_fighters();
	test_combat_attrition();
	test_deployed_surrender();
	test_accepted_bribe();
	session_test_runtime_stop();
	return failures == 0 ? 0 : 1;
}
