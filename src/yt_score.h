#ifndef YT_SCORE_H
#define YT_SCORE_H

#include "yt_game.h"

typedef bool (*yt_score_progress_fn)(void *context, unsigned phase,
    struct yt_error *error);

enum yt_score_field_kind {
	YT_SCORE_FIELD_NONE,
	YT_SCORE_FIELD_PLAYER,
	YT_SCORE_FIELD_SECTOR,
	YT_SCORE_FIELD_TEAM,
};

struct yt_score_field_observation {
	enum yt_score_field_kind kind;
	uint32_t physical_record;
	struct yt_record image;
	bool valid;
};

bool yt_score_generate(struct yt_game *game, struct yt_error *error);
bool yt_score_generate_progress(struct yt_game *game,
    yt_score_progress_fn progress, void *context, struct yt_error *error);
bool yt_score_generate_progress_observed(struct yt_game *game,
	yt_score_progress_fn progress, void *context,
	struct yt_score_field_observation *field, struct yt_error *error);
bool yt_score_generate_progress_with_layout(struct yt_game *game,
	float sector_record_offset, float port_record_offset,
	yt_score_progress_fn progress, void *context,
	struct yt_score_field_observation *field,
	struct yt_error *error);

#endif
