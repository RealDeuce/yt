#ifndef YT_SESSION_PROJECTILE_INTERNAL_H
#define YT_SESSION_PROJECTILE_INTERNAL_H

#include "yt_session_internal.h"

enum yt_missile_sector_route {
	MISSILE_SECTOR_RETURN,
	MISSILE_SECTOR_POST_IMPACT,
};

bool yt_session_missile_sector(struct yt_session *session, int sector_number,
    float *remaining, int *counterattack, int *xannor_provoker,
    float *last_mine_news_sector, enum yt_missile_sector_route *route,
    struct yt_error *error);
bool yt_session_plasma_sector(struct yt_session *session, int sector_number,
    const struct yt_sector *initial, const uint8_t *attacker,
    size_t attacker_length, double *energy, struct yt_error *error);

#endif
