#include "yt_session_internal.h"

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
route_sector_from_single(float value, uint8_t conversion_mode, int16_t *sector,
    struct yt_error *error, const char *operation)
{
	bool overflow;
	int32_t converted = qb_cint_mode((double)value, conversion_mode,
	    &overflow);

	if (overflow || converted < 0
	    || converted >= (int32_t)YT_ROUTE_CAPACITY)
		return route_error(error, operation);
	*sector = (int16_t)converted;
	return true;
}

static bool
route_build(struct yt_session *session, float start_value,
    float destination_value, int16_t predecessor[YT_ROUTE_CAPACITY],
    int16_t next_hop[YT_ROUTE_CAPACITY], float *status,
    enum yt_route_outcome *outcome, struct yt_error *error)
{
	const float *avoid = session->navigation.avoided_sectors;
	uint8_t conversion_mode = session->presentation.sound.conversion_mode;
	bool avoid_enabled;
	int16_t start;
	int16_t destination;
	int16_t head = 1;
	int16_t tail = 1;

	if (status == NULL || outcome == NULL)
		return route_error(error, "route arguments");
	avoid_enabled = *status != 0.0f;
	memset(predecessor, 0, YT_ROUTE_CAPACITY * sizeof(*predecessor));
	memset(next_hop, 0, YT_ROUTE_CAPACITY * sizeof(*next_hop));
	if (!route_sector_from_single(start_value, conversion_mode, &start,
	    error, "route start sector"))
		return false;
	if (start_value == destination_value) {
		predecessor[0] = start;
		next_hop[0] = 0;
		next_hop[start] = 0;
		*outcome = YT_ROUTE_SAME;
		return true;
	}

	next_hop[1] = start;
	predecessor[start] = -1;
	if (avoid_enabled) {
		size_t position;

		for (position = 0U; position < YT_ROUTE_AVOID_COUNT; ++position) {
			float value = avoid[position];
			int16_t blocked;

			if (!route_sector_from_single(value, conversion_mode,
			    &blocked, error, "route avoid sector"))
				return false;
			predecessor[blocked] = blocked;
			if (value == start_value || value == destination_value)
				head = 2;
		}
	}
	if (!route_sector_from_single(destination_value, conversion_mode,
	    &destination, error, "route destination sector"))
		return false;

	while (predecessor[destination] == 0 && tail >= head) {
		struct yt_sector sector;
		int16_t warps[6];
		int16_t current;
		size_t slot;

		current = next_hop[head];
		if (!session_read_sector_at_fault(session, current, &sector,
		    YT_BASIC_FAULT_ROUTE_SECTOR_GET, error))
			return false;
		for (slot = 0U; slot < YT_ARRAY_LEN(warps); ++slot)
			warps[slot] = (int16_t)sector.warps[slot];
		for (slot = 0U; slot < YT_ARRAY_LEN(warps); ++slot) {
			int16_t neighbor = warps[slot];

			if (predecessor[neighbor] != 0)
				continue;
			predecessor[neighbor] = current;
			tail++;
			next_hop[tail] = neighbor;
		}
		head++;
	}

	if (tail < head) {
		next_hop[start] = 0;
		*status = 1.0f;
		*outcome = YT_ROUTE_NOT_FOUND;
		return true;
	}

	head = destination;
	while (predecessor[head] != -1) {
		int16_t child = head;
		int16_t prior = predecessor[child];

		next_hop[prior] = child;
		head = prior;
	}
	next_hop[destination] = 0;
	*status = 0.0f;
	*outcome = YT_ROUTE_FOUND;
	return true;
}

bool
yt_session_build_route(struct yt_session *session, float start,
    float destination, struct session_route_plan *plan, bool use_avoid,
    bool *found, enum yt_route_outcome *route_outcome,
    float *returned_status, struct yt_error *error)
{
	struct session_route_plan discarded_plan;
	int16_t predecessor[YT_ROUTE_CAPACITY];
	float status = use_avoid ? 1.0f : 0.0f;
	enum yt_route_outcome outcome;
	bool success;

	if (plan == NULL)
		plan = &discarded_plan;

	success = route_build(session, start, destination, predecessor,
	    plan->next_hop, &status, &outcome, error);
	if (!success)
		return false;
	*found = outcome == YT_ROUTE_FOUND || outcome == YT_ROUTE_SAME;
	if (route_outcome != NULL)
		*route_outcome = outcome;
	if (returned_status != NULL)
		*returned_status = status;
	return true;
}
