#ifndef YT_MAINT_INTERNAL_H
#define YT_MAINT_INTERNAL_H

#include "yt_maint.h"

#include <math.h>

struct maint_state {
	struct yt_game game;
	float *player_sector;
	float *player_cloak;
	int player_count;
	int sector_count;
	int port_count;
	int planet_count;
	int today;
	struct yt_maintenance_route_cache route_cache;
};

static inline float
yt_maintenance_sint(float value)
{
	volatile float result = floorf(value);
	return result;
}

bool maintenance_copy_part(uint8_t *dest, size_t capacity, size_t *length,
    const uint8_t *data, size_t data_length);
const struct yt_maintenance_output_row *maintenance_find_output_row(
    const struct yt_maintenance_output_result *output, uint16_t address);
bool yt_maintenance_remove_player_from_teams(struct maint_state *state,
    int player_record, struct yt_error *error);
bool yt_maintenance_players_run(struct maint_state *state,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_error *error);
bool maintenance_emit_output_row(
    const struct yt_maintenance_output_result *output, uint16_t address,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_error *error);
bool maintenance_stdout_line(void *context, const uint8_t *line,
    size_t length, struct yt_error *error);
bool yt_maintenance_xannor_run(struct maint_state *state,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_error *error);
bool yt_maintenance_mercenaries_run(struct maint_state *state,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_error *error);

#endif
