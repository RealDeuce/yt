#include "yt_route.h"

#include "qb.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#define YT_ROUTE_HEAD_ADDRESS 0x52BCU
#define YT_ROUTE_TAIL_ADDRESS 0x52BEU
#define YT_ROUTE_AVOID_INDEX_ADDRESS 0x52C0U
#define YT_ROUTE_CURRENT_ADDRESS 0x52C4U
#define YT_ROUTE_WARP_ADDRESS 0x52C6U

static bool
route_error(struct yt_error *error, const char *operation)
{
	if (error != NULL) {
		error->status = YT_RANGE;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
	}
	return false;
}

static int16_t
route_word_from_bits(uint16_t bits)
{
	if (bits <= INT16_MAX)
		return (int16_t)bits;
	return (int16_t)(-(int32_t)(UINT32_C(0x10000) - bits));
}

static int16_t
route_word_increment(int16_t value)
{
	return route_word_from_bits((uint16_t)((uint16_t)value + 1U));
}

static uint16_t
route_index_address(uint16_t base, int16_t index)
{
	return (uint16_t)(base + (uint16_t)((uint16_t)index * 2U));
}

static int16_t
route_process_read_word(const struct yt_route_process *process,
    uint16_t address)
{
	uint16_t next = (uint16_t)(address + 1U);
	uint16_t bits = (uint16_t)process->bytes[address]
	    | (uint16_t)((uint16_t)process->bytes[next] << 8U);

	return route_word_from_bits(bits);
}

static void
route_process_write_word(struct yt_route_process *process, uint16_t address,
    int16_t value)
{
	uint16_t bits = (uint16_t)value;
	uint16_t next = (uint16_t)(address + 1U);

	process->bytes[address] = (uint8_t)bits;
	process->bytes[next] = (uint8_t)(bits >> 8U);
}

static float
route_process_read_single(const struct yt_route_process *process,
    uint16_t address)
{
	uint8_t raw[4];
	size_t offset;

	for (offset = 0U; offset < sizeof(raw); ++offset)
		raw[offset] = process->bytes[(uint16_t)(address + offset)];
	return qb_mbf32_decode(raw);
}

static bool
route_process_write_single(struct yt_route_process *process,
    uint16_t address, float value, struct yt_error *error)
{
	uint8_t raw[4];
	size_t offset;

	if (qb_mbf32_encode(value, raw) == QB_MBF_OVERFLOW)
		return route_error(error, "route avoid MBF32 encode");
	for (offset = 0U; offset < sizeof(raw); ++offset)
		process->bytes[(uint16_t)(address + offset)] = raw[offset];
	return true;
}

static bool
route_endpoint(float value, uint8_t conversion_mode, int16_t *index,
    struct yt_error *error, const char *operation)
{
	bool overflow;
	int32_t converted = qb_cint_mode((double)value, conversion_mode,
	    &overflow);

	if (overflow || converted < 0
	    || converted >= (int32_t)YT_ROUTE_CAPACITY)
		return route_error(error, operation);
	*index = (int16_t)converted;
	return true;
}

static bool
route_integer(float value, uint8_t conversion_mode, int16_t *index,
    struct yt_error *error, const char *operation)
{
	bool overflow;
	int32_t converted = qb_cint_mode((double)value, conversion_mode,
	    &overflow);

	if (overflow)
		return route_error(error, operation);
	*index = (int16_t)converted;
	return true;
}

bool
yt_route_process_set_avoid(struct yt_route_process *process,
    const float avoid[YT_ROUTE_AVOID_COUNT], struct yt_error *error)
{
	size_t position;

	if (process == NULL || avoid == NULL)
		return route_error(error, "route avoid arguments");
	for (position = 0U; position < YT_ROUTE_AVOID_COUNT; ++position) {
		if (!route_process_write_single(process,
		    (uint16_t)(YT_ROUTE_AVOID_ADDRESS + 4U * position),
		    avoid[position], error))
			return false;
	}
	return true;
}

int16_t
yt_route_process_predecessor(const struct yt_route_process *process,
    int16_t index)
{
	if (process == NULL)
		return 0;
	return route_process_read_word(process, route_index_address(
	    YT_ROUTE_WORKSPACE_ADDRESS, index));
}

int16_t
yt_route_process_second(const struct yt_route_process *process,
    int16_t index)
{
	if (process == NULL)
		return 0;
	return route_process_read_word(process, route_index_address(
	    YT_ROUTE_SECOND_ADDRESS, index));
}

bool
yt_route_process_build(float start_value, float destination_value,
    float *status, uint8_t conversion_mode, struct yt_route_process *process,
    yt_route_sector_reader reader, void *reader_context,
    enum yt_route_outcome *outcome, struct yt_error *error)
{
	uint8_t seen[YT_ROUTE_PROCESS_SIZE / CHAR_BIT];
	bool avoid_enabled;
	int16_t start;
	int16_t destination;

	if (status == NULL || process == NULL || reader == NULL
	    || outcome == NULL)
		return route_error(error, "route arguments");
	if (!route_endpoint(start_value, conversion_mode, &start, error,
	    "route start CINT")
	    || !route_endpoint(destination_value, conversion_mode, &destination,
	    error, "route destination CINT"))
		return false;
	avoid_enabled = *status != 0.0f;
	route_process_write_word(process, YT_ROUTE_HEAD_ADDRESS, 1);
	route_process_write_word(process, YT_ROUTE_TAIL_ADDRESS, 1);
	memset(process->bytes + YT_ROUTE_WORKSPACE_ADDRESS, 0,
	    YT_ROUTE_WORKSPACE_BYTES);
	if (start_value == destination_value) {
		route_process_write_word(process, route_index_address(
		    YT_ROUTE_WORKSPACE_ADDRESS, 0), start);
		route_process_write_word(process, route_index_address(
		    YT_ROUTE_SECOND_ADDRESS, 0), 0);
		route_process_write_word(process, route_index_address(
		    YT_ROUTE_SECOND_ADDRESS, start), 0);
		*outcome = YT_ROUTE_SAME;
		return true;
	}

	route_process_write_word(process, route_index_address(
	    YT_ROUTE_SECOND_ADDRESS, 1), start);
	route_process_write_word(process, route_index_address(
	    YT_ROUTE_WORKSPACE_ADDRESS, start), -1);
	if (avoid_enabled) {
		size_t position;

		for (position = 0U; position < YT_ROUTE_AVOID_COUNT;
		    ++position) {
			float value = route_process_read_single(process,
			    (uint16_t)(YT_ROUTE_AVOID_ADDRESS + 4U * position));
			int16_t blocked;

			if (!route_process_write_single(process,
			    YT_ROUTE_AVOID_INDEX_ADDRESS, (float)(position + 1U),
			    error))
				return false;

			if (!route_integer(value, conversion_mode, &blocked, error,
			    "route avoid CINT"))
				return false;
			route_process_write_word(process, route_index_address(
			    YT_ROUTE_WORKSPACE_ADDRESS, blocked), blocked);
			if (value == start_value || value == destination_value)
				route_process_write_word(process,
				    YT_ROUTE_HEAD_ADDRESS, 2);
		}
		if (!route_process_write_single(process,
		    YT_ROUTE_AVOID_INDEX_ADDRESS,
		    (float)(YT_ROUTE_AVOID_COUNT + 1U), error))
			return false;
	}

	while (yt_route_process_predecessor(process, destination) == 0
	    && route_process_read_word(process, YT_ROUTE_TAIL_ADDRESS)
	    >= route_process_read_word(process, YT_ROUTE_HEAD_ADDRESS)) {
		float raw_warps[6];
		int16_t warps[6];
		int16_t head = route_process_read_word(process,
		    YT_ROUTE_HEAD_ADDRESS);
		int16_t current = yt_route_process_second(process, head);
		size_t slot;

		route_process_write_word(process, YT_ROUTE_CURRENT_ADDRESS,
		    current);
		if (!reader(reader_context, current, raw_warps, error))
			return false;
		for (slot = 0U; slot < YT_ARRAY_LEN(warps); ++slot) {
			if (!route_integer(raw_warps[slot], conversion_mode,
			    &warps[slot], error, "route warp CINT"))
				return false;
			route_process_write_word(process,
			    (uint16_t)(YT_ROUTE_WARP_ADDRESS + 2U * slot),
			    warps[slot]);
		}
		for (slot = 0U; slot < YT_ARRAY_LEN(warps); ++slot) {
			int16_t neighbor = warps[slot];
			int16_t tail;

			if (yt_route_process_predecessor(process, neighbor) != 0)
				continue;
			route_process_write_word(process, route_index_address(
			    YT_ROUTE_WORKSPACE_ADDRESS, neighbor), current);
			tail = route_word_increment(route_process_read_word(process,
			    YT_ROUTE_TAIL_ADDRESS));
			route_process_write_word(process, YT_ROUTE_TAIL_ADDRESS,
			    tail);
			route_process_write_word(process, route_index_address(
			    YT_ROUTE_SECOND_ADDRESS, tail), neighbor);
		}
		route_process_write_word(process, YT_ROUTE_HEAD_ADDRESS,
		    route_word_increment(head));
	}

	if (route_process_read_word(process, YT_ROUTE_TAIL_ADDRESS)
	    < route_process_read_word(process, YT_ROUTE_HEAD_ADDRESS)) {
		route_process_write_word(process, route_index_address(
		    YT_ROUTE_SECOND_ADDRESS, start), 0);
		*status = 1.0f;
		*outcome = YT_ROUTE_NOT_FOUND;
		return true;
	}

	memset(seen, 0, sizeof(seen));
	route_process_write_word(process, YT_ROUTE_HEAD_ADDRESS, destination);
	while (yt_route_process_predecessor(process,
	    route_process_read_word(process, YT_ROUTE_HEAD_ADDRESS)) != -1) {
		int16_t head = route_process_read_word(process,
		    YT_ROUTE_HEAD_ADDRESS);
		uint16_t key = (uint16_t)head;
		int16_t prior;

		if ((seen[key / CHAR_BIT]
		    & (uint8_t)(1U << (key % CHAR_BIT))) != 0U) {
			*outcome = YT_ROUTE_BACK_EDGE;
			return true;
		}
		seen[key / CHAR_BIT] |= (uint8_t)(1U << (key % CHAR_BIT));
		prior = yt_route_process_predecessor(process, head);
		route_process_write_word(process, route_index_address(
		    YT_ROUTE_SECOND_ADDRESS, prior), head);
		route_process_write_word(process, YT_ROUTE_HEAD_ADDRESS, prior);
	}
	route_process_write_word(process, route_index_address(
	    YT_ROUTE_SECOND_ADDRESS, destination), 0);
	*status = 0.0f;
	*outcome = YT_ROUTE_FOUND;
	return true;
}

bool
yt_route_build(float start_value, float destination_value, float *status,
    const float avoid[YT_ROUTE_AVOID_COUNT], uint8_t conversion_mode,
    int16_t predecessor[YT_ROUTE_CAPACITY],
    int16_t second[YT_ROUTE_CAPACITY], yt_route_sector_reader reader,
    void *reader_context, enum yt_route_outcome *outcome,
    struct yt_error *error)
{
	struct yt_route_process process;
	bool success;
	size_t index;

	if (status == NULL || avoid == NULL || predecessor == NULL
	    || second == NULL || reader == NULL || outcome == NULL)
		return route_error(error, "route arguments");
	memset(&process, 0, sizeof(process));
	if (!yt_route_process_set_avoid(&process, avoid, error))
		return false;
	success = yt_route_process_build(start_value, destination_value, status,
	    conversion_mode, &process, reader, reader_context, outcome, error);
	for (index = 0U; index < YT_ROUTE_CAPACITY; ++index) {
		predecessor[index] = yt_route_process_predecessor(&process,
		    (int16_t)index);
		second[index] = yt_route_process_second(&process, (int16_t)index);
	}
	return success;
}
