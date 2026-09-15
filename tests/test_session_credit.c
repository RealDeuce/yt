#include "yt_session_internal.h"
#include "session_test_runtime.h"

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
test_credit_mutation(void)
{
	static const char path[] = "SESSION-CREDIT.DAT";
	struct yt_door door;
	struct yt_session session;
	struct yt_player source;
	struct yt_record persisted;
	struct yt_error error;
	bool hydrated;
	size_t index;

	(void)remove(path);
	memset(&door, 0, sizeof(door));
	memset(&session, 0, sizeof(session));
	memset(&source, 0, sizeof(source));
	session.door = &door;
	session.active_player_record = 2;
	session.current_sector_record = -7.0f;
	yt_record_blank(&door.game.config.record);
	CHECK(yt_record_set_number(&door.game.config.record, YT_F53, 100.0f));
	door.game.config.sector_offset = 100.0f;
	yt_record_blank(&source.record);
	source.sector = 9.0f;
	source.fighters = 12.0f;
	source.credits = 100.75f;
	source.cloak = 0.75f;
	source.shields = 34.0f;
	yt_player_encode(&source);
	yt_error_clear(&error);
	CHECK(yt_database_open(&door.game.database, path, YT_OPEN_CREATE,
	    &error));
	CHECK(yt_database_write(&door.game.database, 1U,
	    &door.game.config.record, &error));
	CHECK(yt_database_write_durable(&door.game.database, 2U,
	    &source.record, &error));
	hydrated = false;
	CHECK(session_mutate_player_credits(&session, -1.25f, &hydrated,
	    &error));
	CHECK(hydrated);
	CHECK(session.player.credits == 99.0f);
	CHECK(session.current_sector_record == 109.0f);
	CHECK(session.combat_ship_fighters == 12.0);
	CHECK(session.combat_ship_shields == 34.0f);
	CHECK(session.player_cache.cloak[2] == 0.75f);
	CHECK(yt_database_read(&door.game.database, 2U, &persisted, &error));
	CHECK(yt_record_get_number(&persisted, YT_F81) == 99.0f);
	for (index = 0U; index < YT_RECORD_SIZE; ++index) {
		if (index < YT_F81 || index >= YT_F81 + 4U)
			CHECK(persisted.bytes[index] == source.record.bytes[index]);
	}
	yt_database_close(&door.game.database);

	yt_error_clear(&error);
	CHECK(yt_database_open(&door.game.database, path, YT_OPEN_READ, &error));
	hydrated = false;
	CHECK(!session_mutate_player_credits(&session, -1.0f, &hydrated,
	    &error));
	CHECK(hydrated);
	CHECK(session.player.credits == 98.0f);
	yt_database_close(&door.game.database);
	yt_error_clear(&error);
	CHECK(yt_database_open(&door.game.database, path, YT_OPEN_READ, &error));
	CHECK(yt_database_read(&door.game.database, 2U, &persisted, &error));
	CHECK(yt_record_get_number(&persisted, YT_F81) == 99.0f);
	yt_database_close(&door.game.database);
	CHECK(remove(path) == 0);
}

int
main(void)
{
	session_test_runtime_start();
	test_credit_mutation();
	session_test_runtime_stop();
	return failures == 0 ? 0 : 1;
}
