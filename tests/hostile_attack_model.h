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

enum test_hostile_attack_tail_output_kind {
	YT_HOSTILE_ATTACK_TAIL_REWARD_ROW,
	YT_HOSTILE_ATTACK_TAIL_DEFEATED_ROW,
};

struct test_hostile_attack_tail_ops {
	bool (*read_player)(void *context, int player_record,
	    struct yt_player *player, struct yt_error *error);
	bool (*write_player)(void *context, int player_record,
	    const struct yt_player *player, struct yt_error *error);
	bool (*present)(void *context, const uint8_t *text, size_t length,
	    enum test_hostile_attack_tail_output_kind kind,
	    struct yt_error *error);
	bool (*append_news)(void *context, const uint8_t *text, size_t length,
	    struct yt_error *error);
	bool (*clearance)(void *context, struct yt_error *error);
	bool (*random)(void *context, float *value, struct yt_error *error);
	bool (*victory)(void *context, struct yt_error *error);
};

bool test_hostile_attack_tail_run(struct yt_hostile_attack_tail_state *state,
    const struct test_hostile_attack_tail_ops *ops, void *context,
    struct yt_error *error);

#endif
