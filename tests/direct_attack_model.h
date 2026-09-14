#ifndef TEST_DIRECT_ATTACK_MODEL_H
#define TEST_DIRECT_ATTACK_MODEL_H

#include "yt_game.h"

enum test_direct_attack_route {
	YT_DIRECT_ATTACK_INCOMPLETE,
	YT_DIRECT_ATTACK_NO_FIGHTERS,
	YT_DIRECT_ATTACK_EXHAUSTED,
	YT_DIRECT_ATTACK_CANCELLED,
	YT_DIRECT_ATTACK_COMBAT_RETURN,
};

enum test_direct_attack_output_kind {
	YT_DIRECT_ATTACK_TITLE_ROW,
	YT_DIRECT_ATTACK_NO_FIGHTERS_ROW,
	YT_DIRECT_ATTACK_TEAM_ROW,
	YT_DIRECT_ATTACK_COMMITMENT_PROMPT,
	YT_DIRECT_ATTACK_NONE_SELECTED_ROW,
	YT_DIRECT_ATTACK_NONE_VISIBLE_ROW,
};

enum test_direct_attack_confirmation {
	YT_DIRECT_ATTACK_CONFIRM_NO,
	YT_DIRECT_ATTACK_CONFIRM_YES,
	YT_DIRECT_ATTACK_CONFIRM_EMPTY,
};

struct test_direct_attack_state {
	int current_player_record;
	float last_player_record;
	uint8_t conversion_mode;
	const struct yt_player_cache *player_cache;
	struct yt_player current;
	struct yt_player candidate_player;
	float candidate;
	float target_record_cell;
	double committed;
	bool encountered;
	bool enter_sector;
	enum test_direct_attack_route route;
	bool complete;
};

struct test_direct_attack_ops {
	bool (*read_player)(void *context, int player_record,
	    struct yt_player *player, struct yt_error *error);
	void (*store_target_record)(void *context, const uint8_t raw[4]);
	bool (*present)(void *context, const uint8_t *text, size_t length,
	    enum test_direct_attack_output_kind kind,
	    struct yt_error *error);
	bool (*confirm)(void *context, const uint8_t *prompt, size_t length,
	    enum test_direct_attack_confirmation *answer,
	    struct yt_error *error);
	bool (*amount)(void *context, char *response, size_t capacity,
	    struct yt_error *error);
	bool (*combat)(void *context, int target_record, double committed,
	    struct yt_error *error);
};

bool test_direct_attack_run(struct test_direct_attack_state *state,
    const struct test_direct_attack_ops *ops, void *context,
    struct yt_error *error);

#endif
