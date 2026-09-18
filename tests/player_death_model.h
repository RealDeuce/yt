#ifndef TEST_PLAYER_DEATH_MODEL_H
#define TEST_PLAYER_DEATH_MODEL_H

#include "yt_game.h"

struct test_player_death_state {
	int victim_record;
	int current_player_record;
	int killer;
	int sector_count;
	int port_count;
	int last_player_record;
	const uint8_t *current_name;
	size_t current_name_length;
	struct yt_player victim;
	uint16_t old_ports_owned;
	uint16_t matched_ports;
	uint8_t victim_name[YT_TEXT_FIELD_SIZE];
	size_t victim_name_length;
	bool complete;
};

struct test_player_death_ops {
	void (*clear_active_cache)(void *context, int victim_record,
	    const uint8_t raw[4]);
	bool (*read_player)(void *context, int player_record,
	    struct yt_player *player, struct yt_error *error);
	bool (*write_player)(void *context, int player_record,
	    struct yt_player *player, struct yt_error *error);
	bool (*read_sector)(void *context, int logical_sector,
	    struct yt_sector *sector, struct yt_error *error);
	bool (*write_sector)(void *context, int logical_sector,
	    struct yt_sector *sector, struct yt_error *error);
	bool (*remove_team)(void *context, int victim_record,
	    struct yt_error *error);
	bool (*read_port)(void *context, int logical_port,
	    struct yt_port *port, struct yt_error *error);
	bool (*write_port)(void *context, int logical_port,
	    struct yt_port *port, struct yt_error *error);
	bool (*present)(void *context, const uint8_t *text, size_t length,
	    struct yt_error *error);
	bool (*news)(void *context, const uint8_t *text, size_t length,
	    struct yt_error *error);
	void (*set_current_player)(void *context,
	    const struct yt_player *player);
	bool (*flush)(void *context, struct yt_error *error);
};

bool test_player_death_run(struct test_player_death_state *state,
    const struct test_player_death_ops *ops, void *context,
    struct yt_error *error);
void test_death_team_roster_overlay(struct yt_record *record, float victim);

#endif
