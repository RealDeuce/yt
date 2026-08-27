#ifndef YT_GAME_H
#define YT_GAME_H

#include "yt_config.h"
#include "yt_random.h"

struct yt_player {
	struct yt_record record;
	char name[42];
	float last_active;
	float killed_by;
	float turns;
	float shields;
	float sector;
	float fighters;
	float holds;
	float ore;
	float organics;
	float equipment;
	float credits;
	float name_length;
	float team;
	float danger_scanner;
	float missiles;
	float lottery_plays;
	float score;
	float plasma;
	float ports_owned;
	float ground_forces;
	float cloak;
	float mines;
};

struct yt_sector {
	struct yt_record record;
	float warps[6];
	float port;
	float fighters;
	float fighter_owner;
	float planet;
	float metadata;
	float mines;
};

struct yt_port {
	struct yt_record record;
	char name[42];
	float commodity_class;
	float last_day;
	float stock[3];
	float production[3];
	float factor[3];
	float name_length;
	float treasury;
	float sector;
	float owner;
	float last_minute;
};

struct yt_planet {
	struct yt_record record;
	char name[42];
	float last_day;
	float production[3];
	float stock[3];
	float missiles;
	float owner;
	float ground_forces;
	float name_length;
	float last_minute;
	float plasma;
	float bank;
	float mines;
	float fighters;
};

struct yt_game {
	struct yt_database database;
	struct yt_config config;
	struct yt_random random;
	int today;
	int adjusted_year;
};

void yt_player_decode(struct yt_player *player, const struct yt_record *record);
void yt_player_encode(struct yt_player *player);
void yt_sector_decode(struct yt_sector *sector, const struct yt_record *record);
void yt_sector_encode(struct yt_sector *sector);
void yt_port_decode(struct yt_port *port, const struct yt_record *record);
void yt_port_encode(struct yt_port *port);
void yt_planet_decode(struct yt_planet *planet, const struct yt_record *record);
void yt_planet_encode(struct yt_planet *planet);

bool yt_game_open(struct yt_game *game, enum yt_open_mode mode,
    struct yt_error *error);
void yt_game_close(struct yt_game *game);
bool yt_game_read_player(struct yt_game *game, int basic_record,
    struct yt_player *player, struct yt_error *error);
bool yt_game_write_player(struct yt_game *game, int basic_record,
    struct yt_player *player, struct yt_error *error);
bool yt_game_read_sector(struct yt_game *game, int logical_sector,
    struct yt_sector *sector, struct yt_error *error);
bool yt_game_write_sector(struct yt_game *game, int logical_sector,
    struct yt_sector *sector, struct yt_error *error);
bool yt_game_read_port(struct yt_game *game, int logical_port,
    struct yt_port *port, struct yt_error *error);
bool yt_game_write_port(struct yt_game *game, int logical_port,
    struct yt_port *port, struct yt_error *error);
bool yt_game_read_planet(struct yt_game *game, int logical_planet,
    struct yt_planet *planet, struct yt_error *error);
bool yt_game_write_planet(struct yt_game *game, int logical_planet,
    struct yt_planet *planet, struct yt_error *error);

void yt_player_construct(struct yt_player *player,
    const struct yt_config *config, float today);

#endif

