#ifndef TEST_HOSTILE_SURRENDER_MODEL_H
#define TEST_HOSTILE_SURRENDER_MODEL_H

#include "yt_game.h"

enum test_hostile_surrender_output_kind {
	YT_HOSTILE_SURRENDER_RADIO_ROW,
	YT_HOSTILE_SURRENDER_CAPTAIN_ROW,
	YT_HOSTILE_SURRENDER_WISH_ROW,
	YT_HOSTILE_SURRENDER_PROMPT_BLANK,
	YT_HOSTILE_SURRENDER_JOINED_ROW,
	YT_HOSTILE_SURRENDER_COUNT_ROW,
	YT_HOSTILE_SURRENDER_XANNOR_REFUSAL_ROW,
	YT_HOSTILE_SURRENDER_MERCENARY_REFUSAL_ROW,
};

enum test_hostile_surrender_sound_kind {
	YT_HOSTILE_SURRENDER_RADIO_SOUND,
	YT_HOSTILE_SURRENDER_XANNOR_SOUND,
	YT_HOSTILE_SURRENDER_MERCENARY_SOUND,
	YT_HOSTILE_SURRENDER_JOINED_SOUND,
};

enum yt_hostile_surrender_answer {
	YT_HOSTILE_SURRENDER_ANSWER_NO,
	YT_HOSTILE_SURRENDER_ANSWER_YES,
	YT_HOSTILE_SURRENDER_ANSWER_EMPTY,
};

struct test_hostile_surrender_state {
	int current_player_record;
	float old_owner;
	double attacker_loss;
	double defender_loss;
	double deployed_fighters;
	const uint8_t *cached_player_name;
	size_t cached_player_name_length;
	const uint8_t *real_first_name;
	size_t real_first_name_length;
	struct yt_player current;
	double ship_fighters;
	double deployed_remaining;
	float fighter_owner;
	bool accepted;
};

struct test_hostile_surrender_ops {
	bool (*read_player)(void *context, int player_record,
	    struct yt_player *player, struct yt_error *error);
	bool (*present)(void *context, const uint8_t *text, size_t length,
	    enum test_hostile_surrender_output_kind kind,
	    struct yt_error *error);
	bool (*sound)(void *context,
	    enum test_hostile_surrender_sound_kind kind, float selector,
	    struct yt_error *error);
	bool (*prompt)(void *context, const uint8_t *prompt, size_t length,
	    enum yt_hostile_surrender_answer *answer,
	    struct yt_error *error);
	bool (*append_news)(void *context, const uint8_t *text, size_t length,
	    struct yt_error *error);
	void (*cache_forces)(void *context, double ship_fighters,
	    double deployed_fighters);
	void (*mark_checked)(void *context);
};

bool test_hostile_attack_surrender_run(
    struct test_hostile_surrender_state *state,
    const struct test_hostile_surrender_ops *ops, void *context,
    struct yt_error *error);

#endif
