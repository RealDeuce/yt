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

int
main(void)
{
	test_no_fighters();
	return failures == 0 ? 0 : 1;
}
