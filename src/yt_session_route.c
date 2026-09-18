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
route_sector_from_single(float value, uint8_t conversion_mode, int *sector,
    struct yt_error *error, const char *operation)
{
	bool overflow;
	int32_t converted = qb_cint_mode((double)value, conversion_mode,
	    &overflow);

	if (overflow || converted < 0
	    || converted >= (int32_t)YT_ROUTE_CAPACITY)
		return route_error(error, operation);
	*sector = (int)converted;
	return true;
}

bool
yt_session_build_route(struct yt_session *session, float start_value,
    float destination_value, bool use_avoid, struct session_route_plan *plan,
    struct yt_error *error)
{
	const float *avoid = session->navigation.avoided_sectors;
	uint8_t conversion_mode = session->presentation.conversion_mode;
	int predecessor[YT_ROUTE_CAPACITY];
	int head = 1;
	int tail = 1;

	if (plan == NULL)
		return route_error(error, "route arguments");
	memset(predecessor, 0, YT_ROUTE_CAPACITY * sizeof(*predecessor));
	memset(plan->next_hop, 0, sizeof(plan->next_hop));
	if (!route_sector_from_single(start_value, conversion_mode, &plan->start,
	    error, "route start sector"))
		return false;
	if (start_value == destination_value) {
		plan->destination = plan->start;
		predecessor[0] = plan->start;
		plan->next_hop[0] = 0;
		plan->next_hop[plan->start] = 0;
		plan->outcome = YT_ROUTE_SAME;
		return true;
	}

	plan->next_hop[1] = plan->start;
	predecessor[plan->start] = -1;
	if (use_avoid) {
		size_t position;

		for (position = 0U; position < YT_ROUTE_AVOID_COUNT; ++position) {
			float value = avoid[position];
			int blocked;

			if (!route_sector_from_single(value, conversion_mode,
			    &blocked, error, "route avoid sector"))
				return false;
			predecessor[blocked] = (int16_t)blocked;
			if (value == start_value || value == destination_value)
				head = 2;
		}
	}
	if (!route_sector_from_single(destination_value, conversion_mode,
	    &plan->destination, error, "route destination sector"))
		return false;

	while (predecessor[plan->destination] == 0 && tail >= head) {
		struct yt_sector sector;
		int current;
		size_t slot;

		current = plan->next_hop[head];
		if (!session_read_sector_at_fault(session, current, &sector,
		    YT_BASIC_FAULT_ROUTE_SECTOR_GET, error))
			return false;
		for (slot = 0U; slot < YT_ARRAY_LEN(sector.warps); ++slot) {
			int neighbor = sector.warps[slot];

			if (predecessor[neighbor] != 0)
				continue;
			predecessor[neighbor] = current;
			tail++;
			plan->next_hop[tail] = neighbor;
		}
		head++;
	}

	if (tail < head) {
		static const uint8_t failure[] =
		    "*** You can't get there without going someplace you dont want to!";

		plan->next_hop[plan->start] = 0;
		plan->outcome = YT_ROUTE_NOT_FOUND;
		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "route failure first blank", error))
			return false;
		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "route failure second blank", error))
			return false;
		session->presentation.blink = true;
		return session_present_text(session, failure,
		    sizeof(failure) - 1U, SESSION_PRESENT_BOLD_LINE,
		    "route failure row", error);
	}

	head = plan->destination;
	while (predecessor[head] != -1) {
		int child = head;
		int prior = predecessor[child];

		plan->next_hop[prior] = child;
		head = prior;
	}
	plan->next_hop[plan->destination] = 0;
	plan->outcome = YT_ROUTE_FOUND;
	return true;
}
