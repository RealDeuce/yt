#ifndef TEST_HOSTILE_ATTACK_MODEL_H
#define TEST_HOSTILE_ATTACK_MODEL_H

#include "yt_game.h"

struct test_hostile_attack_persistence_ops {
	bool (*read_player)(void *context, int player_record,
	    struct yt_player *player, struct yt_error *error);
	bool (*write_player)(void *context, int player_record,
	    const struct yt_player *player, struct yt_error *error);
	bool (*read_sector)(void *context, int sector_number,
	    struct yt_sector *sector, struct yt_error *error);
	bool (*write_sector)(void *context, int sector_number,
	    const struct yt_sector *sector, struct yt_error *error);
	bool (*present_blank)(void *context, struct yt_error *error);
	bool (*append_news)(void *context, const uint8_t *text, size_t length,
	    struct yt_error *error);
	bool (*fatal)(void *context, struct yt_error *error);
};

bool test_hostile_attack_persistence_run(
    struct yt_hostile_attack_persistence_state *state,
    const struct test_hostile_attack_persistence_ops *ops, void *context,
    struct yt_error *error);

#endif
