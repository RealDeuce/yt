#ifndef YT_SCORE_H
#define YT_SCORE_H

#include "yt_game.h"

typedef bool (*yt_score_progress_fn)(void *context, unsigned phase,
    struct yt_error *error);

bool yt_score_generate(struct yt_game *game, struct yt_error *error);
bool yt_score_generate_progress(struct yt_game *game,
    yt_score_progress_fn progress, void *context, struct yt_error *error);

#endif
