#ifndef YT_ROUTE_H
#define YT_ROUTE_H

#include "yt_file.h"

#include <stddef.h>
#include <stdint.h>

#define YT_ROUTE_CAPACITY 3001U
#define YT_ROUTE_AVOID_COUNT 30U
#define YT_ROUTE_PROCESS_SIZE 0x10000U
#define YT_ROUTE_AVOID_ADDRESS 0x18B8U
#define YT_ROUTE_WORKSPACE_ADDRESS 0x1C6CU
#define YT_ROUTE_SECOND_ADDRESS \
	(YT_ROUTE_WORKSPACE_ADDRESS + 2U * YT_ROUTE_CAPACITY)
#define YT_ROUTE_WORKSPACE_BYTES (4U * YT_ROUTE_CAPACITY)

enum yt_route_outcome {
	YT_ROUTE_SAME,
	YT_ROUTE_FOUND,
	YT_ROUTE_NOT_FOUND,
	YT_ROUTE_BACK_EDGE,
};

struct yt_route_process {
	uint8_t bytes[YT_ROUTE_PROCESS_SIZE];
};

typedef bool (*yt_route_sector_reader)(void *context, int sector,
    float warps[6], struct yt_error *error);

/*
 * Logical model of YT-SUB:1016's two aliased INTEGER-array halves.
 * status is both the avoid-enable input and the success/failure output.
 * The process variant is the raw 64-KiB DS carrier: INTEGER subscript
 * addresses wrap at 16 bits and can name bytes outside either array half.
 * YT_ROUTE_BACK_EDGE is a bounded observation of the routine's non-returning
 * reconstruction edge, not a returned BASIC error.
 */
bool yt_route_build(float start_value, float destination_value, float *status,
    const float avoid[YT_ROUTE_AVOID_COUNT], uint8_t conversion_mode,
    int16_t predecessor[YT_ROUTE_CAPACITY],
    int16_t second[YT_ROUTE_CAPACITY], yt_route_sector_reader reader,
    void *reader_context, enum yt_route_outcome *outcome,
    struct yt_error *error);

bool yt_route_process_set_avoid(struct yt_route_process *process,
    const float avoid[YT_ROUTE_AVOID_COUNT], struct yt_error *error);
bool yt_route_process_set_avoid_slot(struct yt_route_process *process,
	size_t slot, float value, struct yt_error *error);
float yt_route_process_avoid(const struct yt_route_process *process,
	size_t slot);
int16_t yt_route_process_predecessor(const struct yt_route_process *process,
    int16_t index);
int16_t yt_route_process_second(const struct yt_route_process *process,
    int16_t index);
bool yt_route_process_build(float start_value, float destination_value,
    float *status, uint8_t conversion_mode, struct yt_route_process *process,
    yt_route_sector_reader reader, void *reader_context,
    enum yt_route_outcome *outcome, struct yt_error *error);

#endif
