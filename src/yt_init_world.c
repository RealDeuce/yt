#include "yt_init_internal.h"

#include "qb.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
set_error(struct yt_error *error, enum yt_status status,
    const char *operation, const char *path)
{
	if (error == NULL)
		return;
	error->status = status;
	error->system_error = 0;
	snprintf(error->operation, sizeof(error->operation), "%s", operation);
	snprintf(error->path, sizeof(error->path), "%s",
	    path != NULL ? path : "");
}

static float
float_bits(uint32_t bits)
{
	float value;

	memcpy(&value, &bits, sizeof(value));
	return value;
}

static bool
bounded_random(struct yt_random *random, int bound, int *value,
    struct yt_error *error)
{
	float sample;

	if (bound <= 0) {
		set_error(error, YT_RANGE, "bounded random", "");
		return false;
	}
	if (!yt_random_next(random, &sample, error))
		return false;
	*value = (int)floorf(qb_single_multiply(sample, (float)bound)) + 1;
	return true;
}

static bool
pair_already_linked(const struct yt_init_world *world, int source,
    int destination)
{
	int slot;

	for (slot = 0; slot < 6; ++slot) {
		if (world->warps[source][slot] == destination
		    || world->warps[destination][slot] == source)
			return true;
	}
	return false;
}

static bool
sector_nonempty(const struct yt_init_world *world, int sector)
{
	int slot;

	for (slot = 0; slot < 6; ++slot) {
		if (world->warps[sector][slot] != 0)
			return true;
	}
	return false;
}

static bool
randomize_sector(struct yt_init_world *world, int sector,
    struct yt_random *random, const struct yt_initializer_options *options,
    struct yt_error *error)
{
	const float local_threshold = float_bits(UINT32_C(0x3ecccccd));
	const float long_threshold = float_bits(UINT32_C(0x3f7c28f6));

	do {
		int slot;

		for (slot = 0; slot < 6; ++slot) {
			float probability;

			if (!yt_random_next(random, &probability, error))
				return false;
			if (probability <= local_threshold && slot < 5) {
				int distance;
				int destination;

				if (!bounded_random(random, 10, &distance, error))
					return false;
				if (sector > world->sectors - 10)
					distance = -distance;
				destination = sector + distance;
				if (destination < 1 || destination > world->sectors) {
					set_error(error, YT_RANGE,
					    "initializer local warp", "");
					return false;
				}
				if (world->warps[sector][slot] == 0
				    && world->warps[destination][slot] == 0
				    && !pair_already_linked(world, sector,
				    destination)) {
					world->warps[sector][slot] = destination;
					world->warps[destination][slot] = sector;
				}
			}

			if (slot == 5 && world->warps[sector][slot] == 0) {
				for (;;) {
					int destination;

					if (!yt_random_next(random, &probability, error))
						return false;
					if (probability < long_threshold)
						break;
					if (!bounded_random(random, world->sectors,
					    &destination, error))
						return false;
					if (destination == sector)
						continue;
					if (world->warps[destination][slot] > 0)
						break;
					world->warps[sector][slot] = destination;
					world->warps[destination][slot] = sector;
					{
						uint8_t payload[48];
						char source_text[16];
						char destination_text[16];
						int source_length = qb_str_single(source_text,
						    sizeof(source_text), (float)sector);
						int destination_length = qb_str_single(
						    destination_text, sizeof(destination_text),
						    (float)destination);
						size_t length;

						if (source_length < 0 || destination_length < 0)
							return false;
						length = (size_t)source_length + 2U
						    + (size_t)destination_length;
						memcpy(payload, source_text,
						    (size_t)source_length);
						memcpy(payload + source_length, " -", 2U);
						memcpy(payload + source_length + 2U,
						    destination_text,
						    (size_t)destination_length);
						if (!yt_present_number(options,
						    (float)sector, YT_INIT_OUTPUT_INLINE,
						    error))
							return false;
						if (!yt_present_text(options,
						    YT_INIT_OUTPUT_INLINE, "-", error))
							return false;
						if (!yt_present_number(options,
						    (float)destination, YT_INIT_OUTPUT_LINE,
						    error))
							return false;
						if (!rmt_present(options,
						    YT_RMT_OUTPUT_WORMHOLE, payload, length,
						    error))
							return false;
					}
					break;
				}
			}
		}
	} while (sector != 1 && !sector_nonempty(world, sector));
	return true;
}

static bool
reachable(const struct yt_init_world *world, int target, bool *result,
    struct yt_error *error)
{
	uint8_t *seen;
	int *queue;
	size_t head = 0;
	size_t tail = 0;

	seen = calloc((size_t)world->sectors + 1U, 1);
	queue = malloc(((size_t)world->sectors + 1U) * sizeof(*queue));
	if (seen == NULL || queue == NULL) {
		free(seen);
		free(queue);
		set_error(error, YT_NO_MEMORY, "initializer BFS", "");
		return false;
	}
	seen[1] = 1;
	queue[tail++] = 1;
	while (head < tail) {
		int current = queue[head++];
		int slot;

		for (slot = 0; slot < 6; ++slot) {
			int neighbor = world->warps[current][slot];

			if (!seen[neighbor]) {
				seen[neighbor] = 1;
				queue[tail++] = neighbor;
			}
		}
	}
	*result = seen[target] != 0;
	free(seen);
	free(queue);
	return true;
}

bool
yt_init_world_build_graph(struct yt_init_world *world,
    enum yt_initializer_family family, struct yt_random *random,
    const struct yt_initializer_options *options, struct yt_error *error)
{
	int sector;
	int slot;
	float position;

	for (slot = 0; slot < 6; ++slot) {
		int destination = family == YT_INITIALIZER_YT ? slot + 1 : slot + 2;

		if (destination > world->sectors) {
			set_error(error, YT_RANGE, "initializer fixed warps", "");
			return false;
		}
		world->warps[1][slot] = destination;
		world->warps[destination][slot] = 1;
	}
	for (sector = 1; sector <= world->sectors; ++sector) {
		if (!randomize_sector(world, sector, random, options, error))
			return false;
	}
	if (!yt_present_text(options, YT_INIT_OUTPUT_LINE, "", error))
		return false;
	if (!yt_present_text(options, YT_INIT_OUTPUT_LINE, "", error))
		return false;
	if (!yt_present_text(options, YT_INIT_OUTPUT_LINE,
	    "Verifying warps.. linking isolated sectors.", error))
		return false;
	if (!rmt_present(options, YT_RMT_OUTPUT_BLANK, NULL, 0U, error))
		return false;
	if (!rmt_present(options, YT_RMT_OUTPUT_BLANK, NULL, 0U, error))
		return false;
	if (!rmt_present_text(options, YT_RMT_OUTPUT_LINE,
	    "Verifying warps.. linking isolated sectors.", error))
		return false;
	if (!rmt_present(options, YT_RMT_OUTPUT_BLANK, NULL, 0U, error))
		return false;
	for (sector = 2; sector <= world->sectors; ++sector) {
		bool found;

		if (!yt_present(options, YT_INIT_OUTPUT_LOCATE_COLUMN_ONE,
		    NULL, 0U, error))
			return false;
		if (!yt_present_text(options, YT_INIT_OUTPUT_INLINE,
		    "Verifying warp to sector", error))
			return false;
		if (!yt_present_number(options, (float)sector,
		    YT_INIT_OUTPUT_INLINE, error))
			return false;
		if (!reachable(world, sector, &found, error))
			return false;
		if (!found) {
			int candidate;
			uint8_t payload[96];
			char target_text[16];
			char candidate_text[16];
			int target_length;
			int candidate_length;
			size_t length;

			do {
				if (!bounded_random(random, sector - 1, &candidate,
				    error))
					return false;
			} while (world->warps[candidate][5] != 0);
			target_length = qb_str_single(target_text,
			    sizeof(target_text), (float)sector);
			candidate_length = qb_str_single(candidate_text,
			    sizeof(candidate_text), (float)candidate);
			if (target_length < 0 || candidate_length < 0)
				return false;
			length = sizeof("*** Error - No Path to sector") - 1U
			    + (size_t)target_length + 2U;
			memcpy(payload, "*** Error - No Path to sector",
			    sizeof("*** Error - No Path to sector") - 1U);
			memcpy(payload + sizeof("*** Error - No Path to sector") - 1U,
			    target_text, (size_t)target_length);
			memcpy(payload + length - 2U, "!!", 2U);
			if (!yt_present(options, YT_INIT_OUTPUT_LOCATE_COLUMN_ONE,
			    NULL, 0U, error))
				return false;
			if (!yt_present_text(options, YT_INIT_OUTPUT_INLINE,
			    "*** Error - No Path to sector", error))
				return false;
			if (!yt_present(options, YT_INIT_OUTPUT_INLINE,
			    (const uint8_t *)target_text,
			    (size_t)target_length, error))
				return false;
			if (!yt_present_text(options, YT_INIT_OUTPUT_LINE,
			    "!!", error))
				return false;
			if (!rmt_present(options, YT_RMT_OUTPUT_LINE,
			    payload, length, error))
				return false;
			length = sizeof("Sector") - 1U + (size_t)target_length
			    + sizeof(" has been linked to sector") - 1U
			    + (size_t)candidate_length;
			memcpy(payload, "Sector", sizeof("Sector") - 1U);
			memcpy(payload + sizeof("Sector") - 1U, target_text,
			    (size_t)target_length);
			memcpy(payload + sizeof("Sector") - 1U
			    + (size_t)target_length, " has been linked to sector",
			    sizeof(" has been linked to sector") - 1U);
			memcpy(payload + length - (size_t)candidate_length,
			    candidate_text, (size_t)candidate_length);
			if (!yt_present_text(options, YT_INIT_OUTPUT_INLINE,
			    "Sector", error))
				return false;
			if (!yt_present_number(options, (float)sector,
			    YT_INIT_OUTPUT_INLINE, error))
				return false;
			if (!yt_present_text(options, YT_INIT_OUTPUT_INLINE,
			    "has been linked to sector", error))
				return false;
			if (!yt_present_number(options, (float)candidate,
			    YT_INIT_OUTPUT_LINE, error))
				return false;
			if (!rmt_present(options, YT_RMT_OUTPUT_LINE,
			    payload, length, error))
				return false;
			/* The stale reciprocal at the target is deliberately kept. */
			world->warps[sector][5] = candidate;
			world->warps[candidate][5] = sector;
		}
	}
	if (!yt_present_text(options, YT_INIT_OUTPUT_LINE, "", error))
		return false;
	if (!yt_present_text(options, YT_INIT_OUTPUT_LINE, "", error))
		return false;
	if (!yt_present_text(options, YT_INIT_OUTPUT_LINE,
	    " ** Warp verification complete!! **", error))
		return false;
	if (!yt_present_text(options, YT_INIT_OUTPUT_LINE, "", error))
		return false;
	if (!yt_present_text(options, YT_INIT_OUTPUT_LINE,
	    " ** Building shortcuts back to sector 1", error))
		return false;
	if (!rmt_present(options, YT_RMT_OUTPUT_BLANK, NULL, 0U, error))
		return false;
	if (!rmt_present_text(options, YT_RMT_OUTPUT_LINE,
	    " ** Warp verification complete!! **", error))
		return false;
	if (!rmt_present(options, YT_RMT_OUTPUT_BLANK, NULL, 0U, error))
		return false;
	if (!rmt_present_text(options, YT_RMT_OUTPUT_LINE,
	    " ** Building shortcuts back to sector 1", error))
		return false;
	if (!yt_random_next(random, &position, error))
		return false;
	position = qb_single_add(qb_single_multiply(position, 400.0f), 8.0f);
	while (position < (float)world->sectors) {
		bool overflow;
		int selected = (int)qb_cint(position, &overflow);
		float increment;

		if (overflow || selected < 1 || selected > world->sectors) {
			set_error(error, YT_RANGE, "initializer shortcut", "");
			return false;
		}
		if (world->warps[selected][5] == 0)
			world->warps[selected][5] = 1;
		if (!yt_random_next(random, &increment, error))
			return false;
		position = qb_single_add(position,
		    qb_single_multiply(increment, 400.0f));
	}
	return true;
}

bool
yt_init_world_assign_ports(struct yt_init_world *world,
    struct yt_random *random, struct yt_error *error)
{
	uint8_t *occupied;
	int port;

	if (world->ports < 4 || world->sectors < 7) {
		set_error(error, YT_RANGE, "initializer port layout", "");
		return false;
	}
	occupied = calloc((size_t)world->sectors + 1U, 1);
	if (occupied == NULL) {
		set_error(error, YT_NO_MEMORY, "initializer ports", "");
		return false;
	}
	world->port_sectors[1] = 1;
	world->port_sectors[2] = 3;
	world->port_sectors[3] = 5;
	world->port_sectors[4] = 7;
	/* Compiled bug: mark sector indices 1..4, not 1,3,5,7. */
	for (port = 1; port <= 4; ++port)
		occupied[port] = 1;
	for (port = 5; port <= world->ports; ++port) {
		int sector;

		do {
			if (!bounded_random(random, world->sectors - 1,
			    &sector, error)) {
				free(occupied);
				return false;
			}
			++sector;
		} while (occupied[sector]);
		world->port_sectors[port] = sector;
		occupied[sector] = 1;
	}
	for (port = 1; port <= world->ports; ++port)
		world->sector_ports[world->port_sectors[port]] = port;
	free(occupied);
	return true;
}

bool
yt_init_world_allocate(struct yt_init_world *world, struct yt_error *error)
{
	world->warps = calloc((size_t)world->sectors + 1U,
	    sizeof(*world->warps));
	world->port_sectors = calloc((size_t)world->ports + 1U,
	    sizeof(*world->port_sectors));
	world->sector_ports = calloc((size_t)world->sectors + 1U,
	    sizeof(*world->sector_ports));
	if (world->warps == NULL || world->port_sectors == NULL
	    || world->sector_ports == NULL) {
		set_error(error, YT_NO_MEMORY, "initializer world", "");
		return false;
	}
	return true;
}

void
yt_init_world_free(struct yt_init_world *world)
{
	free(world->warps);
	free(world->port_sectors);
	free(world->sector_ports);
	memset(world, 0, sizeof(*world));
}
