#ifndef TEST_HOSTILE_BRIBE_MODEL_H
#define TEST_HOSTILE_BRIBE_MODEL_H

#include "yt_game.h"

struct test_hostile_bribe_accept_state {
	int current_player_record;
	int current_sector;
	double cached_defenders;
	float offer;
	struct yt_sector sector;
	struct yt_player current;
	float persisted_fighters;
	float persisted_credits;
	bool deal_presented;
	bool sound_played;
	bool sector_read;
	bool sector_written;
	bool player_read;
	bool player_written;
	bool complete;
};

struct test_hostile_bribe_accept_ops {
	bool (*present)(void *context, const uint8_t *text, size_t length,
	    struct yt_error *error);
	bool (*sound)(void *context, float selector, struct yt_error *error);
	bool (*read_sector)(void *context, int sector_number,
	    struct yt_sector *sector, struct yt_error *error);
	bool (*write_sector)(void *context, int sector_number,
	    const struct yt_sector *sector, struct yt_error *error);
	bool (*read_player)(void *context, int player_record,
	    struct yt_player *player, struct yt_error *error);
	bool (*write_player)(void *context, int player_record,
	    const struct yt_player *player, struct yt_error *error);
};

bool test_hostile_bribe_accept_run(
    struct test_hostile_bribe_accept_state *state,
    const struct test_hostile_bribe_accept_ops *ops, void *context,
    struct yt_error *error);

enum test_hostile_bribe_route {
	YT_HOSTILE_BRIBE_INCOMPLETE,
	YT_HOSTILE_BRIBE_SCANNER,
	YT_HOSTILE_BRIBE_HOSTILE_MENU,
	YT_HOSTILE_BRIBE_COMBAT,
	YT_HOSTILE_BRIBE_FATAL,
};

enum test_hostile_bribe_branch {
	YT_HOSTILE_BRIBE_BRANCH_INCOMPLETE,
	YT_HOSTILE_BRIBE_ORDINARY_REFUSAL,
	YT_HOSTILE_BRIBE_PLANET_REFUSAL,
	YT_HOSTILE_BRIBE_LIFE_DEMAND,
	YT_HOSTILE_BRIBE_EMPTY_OFFER,
	YT_HOSTILE_BRIBE_ACCEPTED,
	YT_HOSTILE_BRIBE_REJECTED,
};

enum test_hostile_bribe_output_kind {
	YT_HOSTILE_BRIBE_ORDINARY_REFUSAL_ROW,
	YT_HOSTILE_BRIBE_PLANET_REFUSAL_ROW,
	YT_HOSTILE_BRIBE_LIFE_DEMAND_ROW,
	YT_HOSTILE_BRIBE_INTRODUCTION_ROW,
	YT_HOSTILE_BRIBE_OFFER_PROMPT,
	YT_HOSTILE_BRIBE_REJECTED_ROW,
};

struct test_hostile_bribe_state {
	int current_player_record;
	int current_sector;
	float owner;
	double cached_defenders;
	double ship_fighters;
	float shields;
	double credits;
	float planet_link;
	bool mercenaries_hurt;
	const uint8_t *real_first_name;
	size_t real_first_name_length;
	float draws[3];
	size_t draws_consumed;
	float offer;
	bool above_credits;
	double threshold;
	float commitment;
	bool forced_attack;
	bool direct_hostile_menu;
	bool accepted_called;
	bool combat_called;
	bool fatal_called;
	struct test_hostile_bribe_accept_state accepted;
	enum test_hostile_bribe_branch branch;
	enum test_hostile_bribe_route route;
	bool complete;
};

struct test_hostile_bribe_ops {
	bool (*present)(void *context, const uint8_t *text, size_t length,
	    enum test_hostile_bribe_output_kind kind,
	    struct yt_error *error);
	bool (*random)(void *context, float *value, struct yt_error *error);
	bool (*amount)(void *context, char *response, size_t capacity,
	    struct yt_error *error);
	bool (*accept)(void *context,
	    struct test_hostile_bribe_accept_state *state,
	    struct yt_error *error);
	bool (*combat)(void *context, double commitment,
	    struct yt_error *error);
	bool (*fatal)(void *context, struct yt_error *error);
};

bool test_hostile_bribe_run(struct test_hostile_bribe_state *state,
    const struct test_hostile_bribe_ops *ops, void *context,
    struct yt_error *error);

#endif
