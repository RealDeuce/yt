#ifndef YT_SCORE_H
#define YT_SCORE_H

#include "yt_game.h"

struct yt_score_player {
	int record;
	struct yt_player player;
	double score;
	bool occupied;
};

struct yt_score_team {
	int id;
	double score;
};

struct yt_scoreboard {
	struct yt_game *game;
	float sector_record_offset;
	int player_count;
	int sector_count;
	double xannor;
	double mercenaries;
	struct yt_score_player players[YT_DEFAULT_PLAYER_COUNT];
	struct yt_score_team teams[YT_DEFAULT_PLAYER_COUNT];
};

bool yt_score_generate(struct yt_game *game, struct yt_error *error);
bool yt_scoreboard_prepare(struct yt_scoreboard *scoreboard,
    struct yt_game *game, float sector_record_offset,
    float port_record_offset, struct yt_error *error);
bool yt_scoreboard_load_players(struct yt_scoreboard *scoreboard,
    struct yt_error *error);
bool yt_scoreboard_score_sectors(struct yt_scoreboard *scoreboard,
    struct yt_error *error);
void yt_scoreboard_rank_players(struct yt_scoreboard *scoreboard);
bool yt_scoreboard_write(struct yt_scoreboard *scoreboard,
    struct yt_error *error);

#endif
