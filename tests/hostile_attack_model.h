#ifndef TEST_HOSTILE_ATTACK_MODEL_H
#define TEST_HOSTILE_ATTACK_MODEL_H

#include "hostile_surrender_model.h"
#include "yt_game.h"

enum test_hostile_attack_persistence_route {
	YT_HOSTILE_ATTACK_PERSISTENCE_NORMAL,
	YT_HOSTILE_ATTACK_PERSISTENCE_FATAL,
};

struct test_hostile_attack_persistence_state {
	int current_player_record;
	int current_sector;
	double ship_fighters;
	float shields;
	double deployed_fighters;
	double defender_loss;
	float old_owner;
	const uint8_t *cached_player_name;
	size_t cached_player_name_length;
	const uint8_t *owner_label;
	size_t owner_label_length;
	struct yt_player current;
	struct yt_sector sector;
	enum test_hostile_attack_persistence_route route;
	bool sector_written;
	bool mercenaries_hurt;
};

struct test_hostile_attack_tail_state {
	int current_player_record;
	float old_owner;
	double defender_loss;
	double deployed_fighters;
	double ship_fighters;
	float turns_per_day;
	float headquarters;
	const uint8_t *cached_player_name;
	size_t cached_player_name_length;
	struct yt_player current;
};

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
    struct test_hostile_attack_persistence_state *state,
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

bool test_hostile_attack_tail_run(
    struct test_hostile_attack_tail_state *state,
    const struct test_hostile_attack_tail_ops *ops, void *context,
    struct yt_error *error);

enum test_hostile_attack_combat_route {
	YT_HOSTILE_ATTACK_COMBAT_NORMAL,
	YT_HOSTILE_ATTACK_COMBAT_FATAL,
};

enum test_hostile_attack_combat_output_kind {
	YT_HOSTILE_ATTACK_COMBAT_RESULT_BLANK,
	YT_HOSTILE_ATTACK_COMBAT_LOSS_ROW,
	YT_HOSTILE_ATTACK_COMBAT_DESTROYED_ROW,
	YT_HOSTILE_ATTACK_COMBAT_EXPOSED_ROW,
	YT_HOSTILE_ATTACK_COMBAT_SPILL_BLANK,
};

struct test_hostile_attack_combat_state {
	int current_player_record;
	int current_sector;
	double commitment;
	bool allow_surrender;
	double cached_defenders;
	struct yt_sector sector;
	struct yt_sector opened_sector;
	struct yt_player current;
	const uint8_t *cached_player_name;
	size_t cached_player_name_length;
	const uint8_t *real_first_name;
	size_t real_first_name_length;
	const uint8_t *owner_label;
	size_t owner_label_length;
	float turns_per_day;
	float headquarters;
	float old_owner;
	double old_count;
	double old_ship;
	double attacker_loss;
	double defender_loss;
	double ship_fighters;
	double deployed_remaining;
	float quantum;
	float last_draw;
	size_t iterations;
	bool surrender_checked;
	bool surrendered;
	bool spill_called;
	struct test_hostile_surrender_state surrender;
	struct test_hostile_attack_persistence_state persistence;
	struct test_hostile_attack_tail_state tail;
	enum test_hostile_attack_combat_route route;
	bool complete;
};

struct test_hostile_attack_combat_ops {
	bool (*read_sector)(void *context, int sector_number,
	    struct yt_sector *sector, struct yt_error *error);
	bool (*read_player)(void *context, int player_record,
	    struct yt_player *player, struct yt_error *error);
	bool (*sound)(void *context, float selector, struct yt_error *error);
	bool (*random)(void *context, float *value, struct yt_error *error);
	void (*store_ship)(void *context, double ship_fighters);
	bool (*surrender)(void *context,
	    struct test_hostile_surrender_state *state,
	    struct yt_error *error);
	bool (*present)(void *context, const uint8_t *text, size_t length,
	    enum test_hostile_attack_combat_output_kind kind,
	    struct yt_error *error);
	void (*cache_player)(void *context, const struct yt_player *player);
	void (*cache_sector)(void *context, const struct yt_sector *sector,
	    double deployed_fighters);
	bool (*spill)(void *context, double *fighters, float *shields,
	    struct yt_error *error);
	bool (*persistence)(void *context,
	    struct test_hostile_attack_persistence_state *state,
	    struct yt_error *error);
	bool (*tail)(void *context,
	    struct test_hostile_attack_tail_state *state,
	    struct yt_error *error);
};

bool test_hostile_attack_combat_run(
    struct test_hostile_attack_combat_state *state,
    const struct test_hostile_attack_combat_ops *ops, void *context,
    struct yt_error *error);

#endif
