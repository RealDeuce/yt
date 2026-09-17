#ifndef TEST_SECTOR_MINE_MODEL_H
#define TEST_SECTOR_MINE_MODEL_H

#include "yt_game.h"

enum test_sector_mine_output_kind {
	TEST_SECTOR_MINE_OUTPUT_LINE,
	TEST_SECTOR_MINE_OUTPUT_BOLD_LINE,
	TEST_SECTOR_MINE_OUTPUT_BOLD_RAW,
};

struct test_sector_mine_state {
	int current_player_record;
	float current_sector;
	uint8_t conversion_mode;
	float foreground;
	float background;
	bool blink;
	int pager_foreground;
	bool *destroyed;
	struct yt_player player;
	struct yt_sector sector;
	float mines_before;
	float batch;
	unsigned touched;
	size_t batches;
	bool terminal;
	bool complete;
};

struct test_sector_mine_ops {
	bool (*read_current)(void *context, struct yt_player *player,
	    struct yt_error *error);
	bool (*read_player)(void *context, int player_record,
	    struct yt_player *player, struct yt_error *error);
	bool (*write_player)(void *context, int player_record,
	    struct yt_player *player, struct yt_error *error);
	bool (*read_sector)(void *context, int logical_sector,
	    struct yt_sector *sector, struct yt_error *error);
	bool (*write_sector)(void *context, int logical_sector,
	    struct yt_sector *sector, struct yt_error *error);
	bool (*present)(void *context, const uint8_t *text, size_t length,
	    enum test_sector_mine_output_kind kind, struct yt_error *error);
	bool (*sound)(void *context, float selector, struct yt_error *error);
	bool (*news)(void *context, const uint8_t *text, size_t length,
	    struct yt_error *error);
	bool (*random)(void *context, float *value, struct yt_error *error);
	bool (*shrink)(void *context, float range, float *result,
	    struct yt_error *error);
	bool (*emergency_warp)(void *context, struct yt_error *error);
	void (*set_current)(void *context, const struct yt_player *player);
	void (*style)(void *context, float foreground, float background,
	    bool blink, int pager_foreground);
};

bool test_sector_mine_run(struct test_sector_mine_state *state,
    const struct test_sector_mine_ops *ops, void *context,
    struct yt_error *error);

#endif
