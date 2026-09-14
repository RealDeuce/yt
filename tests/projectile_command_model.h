#ifndef TEST_PROJECTILE_COMMAND_MODEL_H
#define TEST_PROJECTILE_COMMAND_MODEL_H

#include "yt_game.h"

enum test_projectile_command_route {
	YT_PROJECTILE_COMMAND_INCOMPLETE,
	YT_PROJECTILE_COMMAND_NO_TURNS,
	YT_PROJECTILE_COMMAND_NO_AMMUNITION,
	YT_PROJECTILE_COMMAND_TARGET_CANCELLED,
	YT_PROJECTILE_COMMAND_QUANTITY_CANCELLED,
	YT_PROJECTILE_COMMAND_TOO_MANY,
	YT_PROJECTILE_COMMAND_FINALIZER_TERMINAL,
	YT_PROJECTILE_COMMAND_RETURNED,
	YT_PROJECTILE_COMMAND_FATAL,
};

enum test_projectile_command_output_kind {
	YT_PROJECTILE_COMMAND_OPENING_BLANK,
	YT_PROJECTILE_COMMAND_NO_TURNS_ROW,
	YT_PROJECTILE_COMMAND_NO_AMMUNITION_ROW,
	YT_PROJECTILE_COMMAND_TARGET_PROMPT,
	YT_PROJECTILE_COMMAND_INVALID_SECTOR_ROW,
	YT_PROJECTILE_COMMAND_QUANTITY_PROMPT,
	YT_PROJECTILE_COMMAND_TOO_MANY_ROW,
	YT_PROJECTILE_COMMAND_ACCEPTED_BLANK,
};

struct test_projectile_command_state {
	int current_player_record;
	float maximum_sector;
	bool plasma;
	float displayed;
	struct yt_player first_hydration;
	struct yt_player live_hydration;
	struct yt_player post_finalizer;
	uint8_t turn_gate_result_raw[4];
	size_t turn_gate_result_stores;
	size_t attempts;
	size_t hydrations;
	float available;
	float target;
	uint8_t target_raw[4];
	bool target_stored;
	float amount;
	uint8_t amount_raw[4];
	bool amount_stored;
	float origin;
	uint8_t origin_raw[4];
	int counterattack;
	int xannor_provoker;
	bool finalizer_called;
	bool player_written;
	bool player_flushed;
	bool destruction_cleared;
	bool resolver_called;
	bool counterlaunch_called;
	bool xannor_called;
	bool fatal_called;
	bool *destroyed;
	enum test_projectile_command_route route;
	bool complete;
};

struct test_projectile_command_ops {
	bool (*hydrate)(void *context, int player_record,
	    struct yt_player *player, struct yt_error *error);
	bool (*present)(void *context, const uint8_t *text, size_t length,
	    enum test_projectile_command_output_kind kind,
	    struct yt_error *error);
	bool (*input)(void *context, char *response, size_t capacity,
	    struct yt_error *error);
	bool (*finalize)(void *context, struct yt_player *player,
	    struct yt_error *error);
	bool (*write_player)(void *context, int player_record,
	    struct yt_player *player, struct yt_error *error);
	bool (*flush)(void *context, struct yt_error *error);
	bool (*resolve)(void *context, float *origin, uint8_t origin_raw[4],
	    float *target, uint8_t target_raw[4], float *amount,
	    uint8_t amount_raw[4], bool plasma, int *counterattack,
	    int *xannor_provoker, struct yt_error *error);
	bool (*counterlaunch)(void *context, int *counterattack,
	    int *xannor_provoker, struct yt_error *error);
	bool (*xannor)(void *context, int *xannor_provoker,
	    struct yt_error *error);
	bool (*fatal)(void *context, struct yt_error *error);
	yt_destroyed_truth_fn destroyed_truth;
	bool (*counterattack_truth)(void *context);
	bool (*xannor_truth)(void *context);
	void (*store_turn_gate_result)(void *context, const uint8_t raw[4]);
};

bool test_projectile_command_run(
    struct test_projectile_command_state *state,
    const struct test_projectile_command_ops *ops, void *context,
    struct yt_error *error);

#endif
