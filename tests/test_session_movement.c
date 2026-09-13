#include "yt_session_internal.h"

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
write_player(struct yt_game *game, int record, const char *name, float team,
    struct yt_error *error)
{
	struct yt_player player;

	memset(&player, 0, sizeof(player));
	yt_record_blank(&player.record);
	(void)snprintf(player.name, sizeof(player.name), "%s", name);
	player.name_length = (float)strlen(name);
	player.team = team;
	yt_player_encode(&player);
	CHECK(yt_database_write_durable(&game->database, (size_t)record,
	    &player.record, error));
}

static void
write_sector(struct yt_game *game, const struct yt_config *config,
    int logical, struct yt_sector *sector, struct yt_error *error)
{
	yt_sector_encode(sector);
	CHECK(yt_database_write_durable(&game->database,
	    (size_t)yt_sector_basic_record(config, logical), &sector->record,
	    error));
}

static void
write_team_overlay(struct yt_game *game, const struct yt_config *config,
    int team, const char *name, struct yt_error *error)
{
	struct yt_record record;

	yt_record_blank(&record);
	memcpy(record.bytes, name, strlen(name));
	CHECK(yt_record_set_number(&record, YT_F73, (float)strlen(name)));
	CHECK(yt_database_write_durable(&game->database,
	    (size_t)yt_sector_basic_record(config, team), &record, error));
}

static void
test_destination_danger(void)
{
	static const char path[] = "SESSION-DANGER.DAT";
	struct yt_door door;
	struct yt_session session;
	struct yt_sector sector;
	struct yt_error error;
	bool dangerous;

	(void)remove(path);
	memset(&door, 0, sizeof(door));
	memset(&session, 0, sizeof(session));
	memset(&sector, 0, sizeof(sector));
	session.door = &door;
	session.player_record_carrier = 2;
	session.foreground = 7.0f;
	door.game.config.sector_offset = 51.0f;
	door.game.config.port_offset = 2055.0f;
	yt_error_clear(&error);
	CHECK(yt_database_open(&door.game.database, path, YT_OPEN_CREATE,
	    &error));
	write_player(&door.game, 2, "CURRENT", 7.0f, &error);
	write_player(&door.game, 3, "ALLY", 7.0f, &error);
	write_team_overlay(&door.game, &door.game.config, 7, "TEAM", &error);
	memset(&sector, 0, sizeof(sector));
	yt_record_blank(&sector.record);
	sector.fighters = 5.0f;
	sector.fighter_owner = 3.0f;
	write_sector(&door.game, &door.game.config, 13, &sector, &error);
	session.disruption_sectors[0] = 0.0f;
	session.shared_status = 0.0f;
	CHECK(yt_session_destination_is_dangerous(&session, 13.0f,
	    &dangerous, &error));
	CHECK(!dangerous);
	CHECK(session.shared_status == -1.0f);
	CHECK(session.foreground == 7.0f);
	CHECK(yt_present_background(&session.presentation) == 0.0f);
	CHECK(session.player.team == 7.0f);

	write_player(&door.game, 3, "SOLO", 0.0f, &error);
	session.shared_status = -1.0f;
	CHECK(yt_session_destination_is_dangerous(&session, 13.0f,
	    &dangerous, &error));
	CHECK(!dangerous);
	CHECK(session.shared_status == -1.0f);

	yt_database_close(&door.game.database);
	CHECK(yt_session_destination_is_dangerous(&session, 0.0f,
	    &dangerous, &error));
	CHECK(!dangerous);
	CHECK(remove(path) == 0);
}

int
main(void)
{
	test_destination_danger();
	return failures == 0 ? 0 : 1;
}
