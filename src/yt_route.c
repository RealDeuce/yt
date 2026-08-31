#include "yt_route.h"

#include "qb.h"

#include <stdio.h>
#include <string.h>

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
route_index(float value, uint8_t conversion_mode, int *index,
    struct yt_error *error, const char *operation)
{
	bool overflow;
	int32_t converted = qb_cint_mode((double)value, conversion_mode,
	    &overflow);

	if (overflow || converted < 0
	    || converted >= (int32_t)YT_ROUTE_CAPACITY)
		return route_error(error, operation);
	*index = (int)converted;
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
	bool avoid_enabled;
	bool seen[YT_ROUTE_CAPACITY];
	int start;
	int destination;
	int head = 1;
	int tail = 1;

	if (status == NULL || avoid == NULL || predecessor == NULL
	    || second == NULL || reader == NULL || outcome == NULL)
		return route_error(error, "route arguments");
	if (!route_index(start_value, conversion_mode, &start, error,
	    "route start CINT")
	    || !route_index(destination_value, conversion_mode, &destination,
	    error, "route destination CINT"))
		return false;
	avoid_enabled = *status != 0.0f;
	memset(predecessor, 0,
	    YT_ROUTE_CAPACITY * sizeof(*predecessor));
	memset(second, 0, YT_ROUTE_CAPACITY * sizeof(*second));
	if (start_value == destination_value) {
		predecessor[0] = (int16_t)start;
		second[0] = 0;
		second[start] = 0;
		*outcome = YT_ROUTE_SAME;
		return true;
	}

	second[1] = (int16_t)start;
	predecessor[start] = -1;
	if (avoid_enabled) {
		size_t position;

		for (position = 0; position < YT_ROUTE_AVOID_COUNT;
		    ++position) {
			int blocked;

			if (!route_index(avoid[position], conversion_mode, &blocked,
			    error, "route avoid CINT"))
				return false;
			predecessor[blocked] = (int16_t)blocked;
			if (avoid[position] == start_value
			    || avoid[position] == destination_value)
				head = 2;
		}
	}

	while (predecessor[destination] == 0 && tail >= head) {
		float raw_warps[6];
		int warps[6];
		int current = second[head];
		size_t slot;

		if (!reader(reader_context, current, raw_warps, error))
			return false;
		for (slot = 0; slot < YT_ARRAY_LEN(warps); ++slot) {
			if (!route_index(raw_warps[slot], conversion_mode,
			    &warps[slot], error, "route warp CINT"))
				return false;
		}
		for (slot = 0; slot < YT_ARRAY_LEN(warps); ++slot) {
			int neighbor = warps[slot];

			if (predecessor[neighbor] != 0)
				continue;
			predecessor[neighbor] = (int16_t)current;
			++tail;
			if (tail >= (int)YT_ROUTE_CAPACITY)
				return route_error(error, "route FIFO overflow");
			second[tail] = (int16_t)neighbor;
		}
		++head;
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
		int prior;

		if (seen[head])
			return route_error(error, "route predecessor cycle");
		seen[head] = true;
		prior = predecessor[head];
		if (prior < 0 || prior >= (int)YT_ROUTE_CAPACITY)
			return route_error(error, "route predecessor index");
		second[prior] = (int16_t)head;
		head = prior;
	}
	second[destination] = 0;
	*status = 0.0f;
	*outcome = YT_ROUTE_FOUND;
	return true;
}
