#ifndef YT_MAINT_H
#define YT_MAINT_H

#include "yt_game.h"

bool yt_maintenance_run(struct yt_error *error);
bool yt_radio_append_maintenance(const char *text, float sender,
    float recipient, struct yt_error *error);
bool yt_radio_compact(struct yt_error *error);
bool yt_news_append(const char *text, struct yt_error *error);

#endif
