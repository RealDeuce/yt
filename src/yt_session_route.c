#include "yt_session_internal.h"

#include <string.h>

static bool
route_sector_reader(void *context, int logical_sector, float warps[6],
    struct yt_error *error)
{
	struct yt_session *session = context;
	struct yt_sector sector;

	if (!session_read_sector_at_fault(session, logical_sector, &sector,
	    YT_BASIC_FAULT_ROUTE_SECTOR_GET, error))
		return false;
	memcpy(warps, sector.warps, sizeof(sector.warps));
	return true;
}

static void
route_require_returned(enum yt_route_outcome outcome)
{
	if (outcome == YT_ROUTE_BACK_EDGE)
		yt_route_reconstruction_back_edge();
}

bool
yt_session_build_route(struct yt_session *session, float start, float destination,
    int16_t *next_hop, bool use_avoid, bool *found,
    enum yt_route_outcome *route_outcome, float *returned_status,
    struct yt_error *error)
{
	float status = use_avoid ? 1.0f : 0.0f;
	enum yt_route_outcome outcome;
	bool success;
	size_t index;

	success = yt_route_build(start, destination, &status,
	    session->route_avoid, session->presentation.sound.conversion_mode,
	    session->route_predecessor, session->route_second,
	    route_sector_reader, session, &outcome, error);
	if (next_hop != NULL)
		for (index = 0U; index < YT_ROUTE_CAPACITY; ++index)
			next_hop[index] = session->route_second[index];
	if (!success)
		return false;
	route_require_returned(outcome);
	*found = outcome == YT_ROUTE_FOUND || outcome == YT_ROUTE_SAME;
	if (route_outcome != NULL)
		*route_outcome = outcome;
	if (returned_status != NULL)
		*returned_status = status;
	return true;
}

