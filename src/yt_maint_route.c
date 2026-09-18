#include "yt_maint.h"

#include "qb.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

static void
set_error(struct yt_error *error, enum yt_status status,
    const char *operation, const char *path)
{
	if (error == NULL)
		return;
	error->status = status;
	error->system_error = errno;
	snprintf(error->operation, sizeof(error->operation), "%s", operation);
	snprintf(error->path, sizeof(error->path), "%s",
	    path != NULL ? path : "");
}

enum enqueue_result {
	ENQUEUE_INVALID,
	ENQUEUE_SKIPPED,
	ENQUEUE_ADDED
};

static enum enqueue_result
route_enqueue(int neighbor, int predecessor, int sector_count,
    int *queue, size_t queue_capacity, size_t *tail, int *previous)
{
	if (queue == NULL || tail == NULL || previous == NULL
	    || sector_count < 1 || neighbor < 0 || neighbor > sector_count
	    || *tail > queue_capacity)
		return ENQUEUE_INVALID;
	if (neighbor == 0 || previous[neighbor] != 0)
		return ENQUEUE_SKIPPED;
	if (*tail == queue_capacity)
		return ENQUEUE_INVALID;
	previous[neighbor] = predecessor;
	queue[(*tail)++] = neighbor;
	return ENQUEUE_ADDED;
}

void
yt_maintenance_route_cache_free(struct yt_maintenance_route_cache *cache)
{
	if (cache == NULL)
		return;
	free(cache->warps);
	free(cache->successors);
	cache->warps = NULL;
	cache->successors = NULL;
	cache->sector_count = 0;
}

bool
yt_maintenance_route_next_hop(struct yt_game *game,
    struct yt_maintenance_route_cache *cache, int source, int target,
    int *next_hop, struct yt_error *error)
{
	int *queue;
	int *previous;
	int *successors;
	int sector_count;
	size_t head = 0;
	size_t tail = 0;
	int result = 0;
	bool found = false;
	int logical;

	if (game == NULL || cache == NULL || next_hop == NULL) {
		set_error(error, YT_INVALID, "maintenance route", "");
		return false;
	}
	sector_count = (int)game->config.port_offset
	    - (int)game->config.sector_offset;
	if (source < 1 || source > sector_count
	    || target < 0 || target > sector_count) {
		set_error(error, YT_RANGE, "maintenance route endpoint",
		    "YTDATA.DAT");
		return false;
	}
	if (source == target) {
		free(cache->successors);
		cache->successors = NULL;
		*next_hop = 0;
		return true;
	}
	if (cache->warps == NULL) {
		cache->warps = malloc(((size_t)sector_count + 1U) * 6U
		    * sizeof(*cache->warps));
		if (cache->warps == NULL) {
			set_error(error, YT_NO_MEMORY, "maintenance route cache", "");
			return false;
		}
		cache->sector_count = sector_count;
		for (logical = 1; logical <= sector_count; ++logical) {
			struct yt_sector sector;

			if (!yt_game_read_sector(game, logical, &sector, error)) {
				yt_maintenance_route_cache_free(cache);
				return false;
			}
			memcpy(cache->warps + (size_t)logical * 6U, sector.warps,
			    6U * sizeof(*sector.warps));
		}
	}
	else if (cache->sector_count != sector_count) {
		set_error(error, YT_RANGE, "maintenance route cache",
		    "YTDATA.DAT");
		return false;
	}

	queue = malloc(((size_t)sector_count + 1U) * sizeof(*queue));
	previous = calloc((size_t)sector_count + 1U,
	    sizeof(*previous));
	successors = calloc((size_t)sector_count + 1U,
	    sizeof(*successors));
	if (queue == NULL || previous == NULL || successors == NULL) {
		free(queue);
		free(previous);
		free(successors);
		set_error(error, YT_NO_MEMORY, "maintenance route", "");
		return false;
	}
	previous[source] = -1;
	queue[tail++] = source;
	while (head < tail && !found) {
		int current = queue[head++];
		int slot;

		for (slot = 0; slot < 6; ++slot) {
			int neighbor = cache->warps[(size_t)current * 6U
			    + (size_t)slot];
			enum enqueue_result enqueued;

			enqueued = route_enqueue(neighbor, current,
			    sector_count, queue, (size_t)sector_count + 1U, &tail,
			    previous);
			if (enqueued == ENQUEUE_INVALID) {
				set_error(error, YT_RANGE, "maintenance route warp",
				    "YTDATA.DAT");
				goto done;
			}
			if (enqueued == ENQUEUE_SKIPPED)
				continue;
			if (neighbor == target) {
				found = true;
				break;
			}
		}
	}
	if (found) {
		int current = target;

		while (previous[current] != -1) {
			int predecessor = previous[current];

			if (predecessor < 1 || predecessor > sector_count) {
				set_error(error, YT_RANGE,
				    "maintenance route predecessor", "YTDATA.DAT");
				goto done;
			}
			successors[predecessor] = current;
			current = predecessor;
		}
		result = successors[source];
	}
	else {
		char from[48];
		char to[48];
		char line[180];

		qb_str_double(from, sizeof(from), source);
		qb_str_double(to, sizeof(to), target);
		snprintf(line, sizeof(line),
		    "*** Error - Sector path not found - from sector%s to sector %s",
		    from, to);
		if (!yt_news_append(line, error))
			goto done;
	}
	*next_hop = result;
	free(cache->successors);
	cache->successors = successors;
	free(queue);
	free(previous);
	return true;

done:
	free(queue);
	free(previous);
	free(successors);
	return false;
}
