#ifndef YT_ROUTE_H
#define YT_ROUTE_H

#include "yt_file.h"

#include <stddef.h>
#include <stdint.h>

#define YT_ROUTE_CAPACITY 3001U
#define YT_ROUTE_AVOID_COUNT 30U

enum yt_route_outcome {
	YT_ROUTE_SAME,
	YT_ROUTE_FOUND,
	YT_ROUTE_NOT_FOUND,
};

typedef bool (*yt_route_sector_reader)(void *context, int sector,
    float warps[6], struct yt_error *error);

/*
 * Logical model of YT-SUB:1016's two aliased INTEGER-array halves.
 * status is both the avoid-enable input and the success/failure output.
 */
bool yt_route_build(float start_value, float destination_value, float *status,
    const float avoid[YT_ROUTE_AVOID_COUNT], int conversion_mode,
    int16_t predecessor[YT_ROUTE_CAPACITY],
    int16_t second[YT_ROUTE_CAPACITY], yt_route_sector_reader reader,
    void *reader_context, enum yt_route_outcome *outcome,
    struct yt_error *error);

#endif
