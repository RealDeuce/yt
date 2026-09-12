#include "yt_route.h"

#include "qb.h"
#include "yt_main_error.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

_Noreturn void
yt_route_reconstruction_back_edge(void)
{
	/* YT-SUB:1374 repeats without an I/O or scheduler boundary. */
	for (;;) {
	}
}

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

static bool
route_integer_at(float value, uint8_t conversion_mode, int16_t *index,
    struct yt_error *error, const char *operation,
    enum yt_basic_fault_site site)
{
	bool overflow;
	int32_t converted = qb_cint_mode((double)value, conversion_mode,
	    &overflow);

	if (overflow) {
		(void)route_error(error, operation);
		(void)yt_error_attach_basic_fault_number(error, site, 6U);
		return false;
	}
	*index = (int16_t)converted;
	return true;
}

static bool
route_endpoint_at(float value, uint8_t conversion_mode, int16_t *index,
    struct yt_error *error, const char *operation,
    enum yt_basic_fault_site site)
{
	if (!route_integer_at(value, conversion_mode, index, error, operation,
	    site))
		return false;
	if (*index < 0 || *index >= (int16_t)YT_ROUTE_CAPACITY)
		return route_error(error, operation);
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

static bool
route_index_valid(int16_t index)
{
	return index >= 0 && index < (int16_t)YT_ROUTE_CAPACITY;
}

bool
yt_route_build(float start_value, float destination_value, float *status,
    const float avoid[YT_ROUTE_AVOID_COUNT], uint8_t conversion_mode,
    int16_t predecessor[YT_ROUTE_CAPACITY],
    int16_t second[YT_ROUTE_CAPACITY], yt_route_sector_reader reader,
    void *reader_context, enum yt_route_outcome *outcome,
    struct yt_error *error)
{
	uint8_t seen[(YT_ROUTE_CAPACITY + CHAR_BIT - 1U) / CHAR_BIT];
	bool avoid_enabled;
	int16_t start;
	int16_t destination;
	int16_t head = 1;
	int16_t tail = 1;

	if (status == NULL || avoid == NULL || predecessor == NULL
	    || second == NULL || reader == NULL || outcome == NULL)
		return route_error(error, "route arguments");
	avoid_enabled = *status != 0.0f;
	memset(predecessor, 0, YT_ROUTE_CAPACITY * sizeof(*predecessor));
	memset(second, 0, YT_ROUTE_CAPACITY * sizeof(*second));
	if (!route_endpoint_at(start_value, conversion_mode, &start, error,
	    "route start FIFO CINT", YT_BASIC_FAULT_ROUTE_START_FIFO_CINT))
		return false;
	if (start_value == destination_value) {
		predecessor[0] = start;
		second[0] = 0;
		if (!route_endpoint_at(start_value, conversion_mode, &start, error,
		    "route start predecessor CINT",
		    YT_BASIC_FAULT_ROUTE_START_PREDECESSOR_CINT))
			return false;
		second[start] = 0;
		*outcome = YT_ROUTE_SAME;
		return true;
	}

	second[1] = start;
	if (!route_endpoint_at(start_value, conversion_mode, &start, error,
	    "route start predecessor CINT",
	    YT_BASIC_FAULT_ROUTE_START_PREDECESSOR_CINT))
		return false;
	predecessor[start] = -1;
	if (avoid_enabled) {
		size_t position;

		for (position = 0U; position < YT_ROUTE_AVOID_COUNT;
		    ++position) {
			float value = avoid[position];
			int16_t blocked;
			int16_t marker;

			if (!route_integer_at(value, conversion_mode, &blocked,
			    error, "route avoid predecessor CINT",
			    YT_BASIC_FAULT_ROUTE_AVOID_PREDECESSOR_CINT))
				return false;
			if (!route_integer_at(value, conversion_mode, &marker,
			    error, "route avoid endpoint CINT",
			    YT_BASIC_FAULT_ROUTE_AVOID_ENDPOINT_CINT))
				return false;
			if (!route_index_valid(blocked))
				return route_error(error, "route avoid index");
			predecessor[blocked] = marker;
			if (value == start_value || value == destination_value)
				head = 2;
		}
	}
	if (!route_endpoint_at(destination_value, conversion_mode, &destination,
	    error, "route destination predecessor CINT",
	    YT_BASIC_FAULT_ROUTE_DESTINATION_PREDECESSOR_CINT))
		return false;

	while (predecessor[destination] == 0 && tail >= head) {
		float raw_warps[6];
		int16_t warps[6];
		int16_t queue_index;
		int16_t current;
		int16_t expanded;
		size_t slot;

		if (!route_integer_at((float)head, conversion_mode, &queue_index,
		    error, "route FIFO head CINT",
		    YT_BASIC_FAULT_ROUTE_FIFO_HEAD_CINT))
			return false;
		if (!route_index_valid(queue_index))
			return route_error(error, "route FIFO index");
		current = second[queue_index];
		if (!route_integer_at((float)current, conversion_mode, &current,
		    error, "route FIFO node CINT",
		    YT_BASIC_FAULT_ROUTE_FIFO_NODE_CINT))
			return false;
		if (!route_endpoint_at((float)current, conversion_mode, &expanded,
		    error, "route expanded-node CINT",
		    YT_BASIC_FAULT_ROUTE_EXPANDED_NODE_CINT))
			return false;
		if (!reader(reader_context, expanded, raw_warps, error))
			return false;
		for (slot = 0U; slot < YT_ARRAY_LEN(warps); ++slot) {
			if (!route_integer(raw_warps[slot], conversion_mode,
			    &warps[slot], error, "route warp CINT"))
				return false;
			if (!route_index_valid(warps[slot]))
				return route_error(error, "route warp index");
		}
		for (slot = 0U; slot < YT_ARRAY_LEN(warps); ++slot) {
			int16_t neighbor = warps[slot];

			if (predecessor[neighbor] != 0)
				continue;
			predecessor[neighbor] = current;
			if (tail == (int16_t)(YT_ROUTE_CAPACITY - 1U))
				return route_error(error, "route FIFO capacity");
			tail++;
			second[tail] = neighbor;
		}
		head++;
	}

	if (tail < head) {
		second[start] = 0;
		*status = 1.0f;
		*outcome = YT_ROUTE_NOT_FOUND;
		return true;
	}

	memset(seen, 0, sizeof(seen));
	head = destination;
	while (predecessor[head] != -1) {
		int16_t child;
		int16_t prior;
		size_t key;

		if (!route_integer_at((float)head, conversion_mode, &child, error,
		    "route reconstruction child CINT",
		    YT_BASIC_FAULT_ROUTE_RECONSTRUCTION_CHILD_CINT))
			return false;
		if (!route_index_valid(child))
			return route_error(error, "route reconstruction child");
		key = (size_t)child;
		if ((seen[key / CHAR_BIT]
		    & (uint8_t)(1U << (key % CHAR_BIT))) != 0U) {
			*outcome = YT_ROUTE_BACK_EDGE;
			return true;
		}
		seen[key / CHAR_BIT] |= (uint8_t)(1U << (key % CHAR_BIT));
		prior = predecessor[child];
		if (!route_integer_at((float)prior, conversion_mode, &prior,
		    error, "route reconstruction parent CINT",
		    YT_BASIC_FAULT_ROUTE_RECONSTRUCTION_PARENT_CINT))
			return false;
		if (!route_index_valid(prior))
			return route_error(error, "route reconstruction parent");
		second[prior] = child;
		head = prior;
	}
	if (!route_endpoint_at(destination_value, conversion_mode, &destination,
	    error, "route next-hop CINT", YT_BASIC_FAULT_ROUTE_NEXT_HOP_CINT))
		return false;
	second[destination] = 0;
	*status = 0.0f;
	*outcome = YT_ROUTE_FOUND;
	return true;
}
