#include "yt_session_internal.h"
#include "session_test_runtime.h"
#include "random_test_support.h"

#include <stdint.h>
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

struct entropy_sequence {
	const uint32_t *samples;
	size_t count;
	size_t position;
};

static bool
sequence_fill(void *context, void *buffer, size_t length,
    struct yt_error *error)
{
	struct entropy_sequence *sequence = context;
	uint8_t *bytes = buffer;
	uint32_t sample;

	(void)error;
	if (length != 3U || sequence->position >= sequence->count)
		return false;
	sample = sequence->samples[sequence->position++];
	bytes[0] = (uint8_t)sample;
	bytes[1] = (uint8_t)(sample >> 8);
	bytes[2] = (uint8_t)(sample >> 16);
	return true;
}

static void
write_player(struct yt_database *database, struct yt_player *player,
    struct yt_error *error)
{
	yt_player_encode(player);
	CHECK(yt_database_write_durable(database, 2U, &player->record, error));
}

static void
test_direct_warp_decline(void)
{
	static const char path[] = "SESSION-ACTION-DECLINE.DAT";
	static const uint8_t decline[] = "N\r";
	struct yt_door door;
	struct yt_session session;
	struct yt_player player;
	struct yt_record persisted;
	struct yt_error error;

	(void)remove(path);
	memset(&door, 0, sizeof(door));
	memset(&session, 0, sizeof(session));
	memset(&player, 0, sizeof(player));
	session.door = &door;
	session.active_player_record = 2;
	session.pager.nonstop = -1.0f;
	memcpy(session.io.typeahead, decline, sizeof(decline) - 1U);
	session.io.typeahead_length = sizeof(decline) - 1U;
	yt_record_blank(&player.record);
	player.turns = 20.0f;
	player.sector = 7.0f;
	yt_error_clear(&error);
	CHECK(yt_database_open(&door.game.database, path, YT_OPEN_CREATE,
	    &error));
	write_player(&door.game.database, &player, &error);
	CHECK(yt_session_direct_emergency_warp(&session, &error));
	CHECK(session.io.typeahead_position == session.io.typeahead_length);
	CHECK(yt_database_read(&door.game.database, 2U, &persisted, &error));
	yt_player_decode(&player, &persisted);
	CHECK(player.turns == 20.0f);
	CHECK(player.sector == 7.0f);
	yt_database_close(&door.game.database);
	CHECK(remove(path) == 0);
}

static void
test_emergency_warp_persistence(void)
{
	static const char path[] = "SESSION-ACTION-WARP.DAT";
	static const uint32_t samples[6] = {0, 0, 0, 0, 0, 0};
	struct entropy_sequence sequence = {samples, 6U, 0U};
	struct yt_door door;
	struct yt_session session;
	struct yt_player player;
	struct yt_record persisted;
	struct yt_error error;

	(void)remove(path);
	memset(&door, 0, sizeof(door));
	memset(&session, 0, sizeof(session));
	memset(&player, 0, sizeof(player));
	session.door = &door;
	session.active_player_record = 2;
	session.pager.nonstop = -1.0f;
	session.presentation.foreground = 7.0f;
	door.game.config.sector_offset = 51.0f;
	door.game.config.port_offset = 2055.0f;
	door.game.config.headquarters = 42.0f;
	yt_test_random_use_provider(&door.game.random, sequence_fill, &sequence);
	yt_record_blank(&player.record);
	player.turns = 20.0f;
	player.sector = 7.0f;
	yt_error_clear(&error);
	CHECK(yt_database_open(&door.game.database, path, YT_OPEN_CREATE,
	    &error));
	write_player(&door.game.database, &player, &error);
	CHECK(yt_session_emergency_warp(&session, &error));
	CHECK(sequence.position == 6U);
	CHECK(door.game.random.draws == 6U);
	CHECK(yt_player_cache_value(&session.player_cache, 2,
	    YT_PLAYER_CACHE_SECTOR) == 1.0f);
	CHECK(yt_database_read(&door.game.database, 2U, &persisted, &error));
	yt_player_decode(&player, &persisted);
	CHECK(player.turns == 20.0f);
	CHECK(player.sector == 1.0f);
	yt_database_close(&door.game.database);
	CHECK(remove(path) == 0);
}

int
main(void)
{
	session_test_runtime_start();
	test_direct_warp_decline();
	test_emergency_warp_persistence();
	session_test_runtime_stop();
	return failures == 0 ? 0 : 1;
}
