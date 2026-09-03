#include "yt_route.h"

#include "qb.h"
#include "yt_main_error.h"

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
route_integer_at(float value, uint8_t conversion_mode, int16_t *index,
    struct yt_error *error, const char *operation,
    enum yt_basic_fault_site site)
{
	bool overflow;
	int32_t converted = qb_cint_mode((double)value, conversion_mode,
	    &overflow);

	if (overflow) {
		(void)route_error(error, operation);
		(void)yt_error_attach_basic_fault(error, site);
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

bool
yt_route_process_set_avoid_slot(struct yt_route_process *process,
    size_t slot, float value, struct yt_error *error)
{
	if (process == NULL || slot >= YT_ROUTE_AVOID_COUNT)
		return route_error(error, "route avoid slot");
	return route_process_write_single(process,
	    (uint16_t)(YT_ROUTE_AVOID_ADDRESS + 4U * slot), value, error);
}

float
yt_route_process_avoid(const struct yt_route_process *process, size_t slot)
{
	if (process == NULL || slot >= YT_ROUTE_AVOID_COUNT)
		return 0.0f;
	return route_process_read_single(process,
	    (uint16_t)(YT_ROUTE_AVOID_ADDRESS + 4U * slot));
}

void
yt_route_process_set_raw_single(struct yt_route_process *process,
    uint16_t address, const uint8_t raw[4])
{
	size_t offset;

	if (process == NULL || raw == NULL)
		return;
	for (offset = 0U; offset < 4U; ++offset)
		process->bytes[(uint16_t)(address + offset)] = raw[offset];
}

void
yt_route_process_raw_single(const struct yt_route_process *process,
    uint16_t address, uint8_t raw[4])
{
	size_t offset;

	if (process == NULL || raw == NULL)
		return;
	for (offset = 0U; offset < 4U; ++offset)
		raw[offset] = process->bytes[(uint16_t)(address + offset)];
}

float
yt_route_process_single(const struct yt_route_process *process,
    uint16_t address)
{
	if (process == NULL)
		return 0.0f;
	return route_process_read_single(process, address);
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

void
yt_route_process_set_second(struct yt_route_process *process, int16_t index,
    int16_t value)
{
	if (process != NULL)
		route_process_write_word(process, route_index_address(
		    YT_ROUTE_SECOND_ADDRESS, index), value);
}

struct route_arguments {
	bool start_addressed;
	bool destination_addressed;
	bool status_addressed;
	uint16_t start_address;
	uint16_t destination_address;
	uint16_t status_address;
	float start_value;
	float destination_value;
	float *status;
};

static float
route_argument_start(const struct route_arguments *arguments,
    const struct yt_route_process *process)
{
	return arguments->start_addressed
	    ? route_process_read_single(process, arguments->start_address)
	    : arguments->start_value;
}

static float
route_argument_destination(const struct route_arguments *arguments,
    const struct yt_route_process *process)
{
	return arguments->destination_addressed
	    ? route_process_read_single(process, arguments->destination_address)
	    : arguments->destination_value;
}

static float
route_argument_status(const struct route_arguments *arguments,
    const struct yt_route_process *process)
{
	return arguments->status_addressed
	    ? route_process_read_single(process, arguments->status_address)
	    : *arguments->status;
}

static bool
route_argument_write_status(struct route_arguments *arguments,
    struct yt_route_process *process, float value, struct yt_error *error)
{
	if (arguments->status_addressed)
		return route_process_write_single(process,
		    arguments->status_address, value, error);
	*arguments->status = value;
	return true;
}

static bool
route_process_build(struct route_arguments *arguments,
    uint8_t conversion_mode, struct yt_route_process *process,
    yt_route_sector_reader reader, void *reader_context,
    enum yt_route_outcome *outcome, struct yt_error *error)
{
	uint8_t seen[YT_ROUTE_PROCESS_SIZE / CHAR_BIT];
	bool avoid_enabled;
	int16_t start;
	int16_t destination;
	float start_value;
	float destination_value;

	if (arguments == NULL || (!arguments->status_addressed
	    && arguments->status == NULL) || process == NULL || reader == NULL
	    || outcome == NULL)
		return route_error(error, "route arguments");
	start_value = route_argument_start(arguments, process);
	destination_value = route_argument_destination(arguments, process);
	avoid_enabled = route_argument_status(arguments, process) != 0.0f;
	route_process_write_word(process, YT_ROUTE_HEAD_ADDRESS, 1);
	route_process_write_word(process, YT_ROUTE_TAIL_ADDRESS, 1);
	memset(process->bytes + YT_ROUTE_WORKSPACE_ADDRESS, 0,
	    YT_ROUTE_WORKSPACE_BYTES);
	if (!route_endpoint_at(start_value, conversion_mode, &start, error,
	    "route start FIFO CINT", YT_BASIC_FAULT_ROUTE_START_FIFO_CINT))
		return false;
	if (start_value == destination_value) {
		route_process_write_word(process, route_index_address(
		    YT_ROUTE_WORKSPACE_ADDRESS, 0), start);
		route_process_write_word(process, route_index_address(
		    YT_ROUTE_SECOND_ADDRESS, 0), 0);
		if (!route_endpoint_at(start_value, conversion_mode, &start, error,
		    "route start predecessor CINT",
		    YT_BASIC_FAULT_ROUTE_START_PREDECESSOR_CINT))
			return false;
		route_process_write_word(process, route_index_address(
		    YT_ROUTE_SECOND_ADDRESS, start), 0);
		*outcome = YT_ROUTE_SAME;
		return true;
	}

	route_process_write_word(process, route_index_address(
	    YT_ROUTE_SECOND_ADDRESS, 1), start);
	if (!route_endpoint_at(start_value, conversion_mode, &start, error,
	    "route start predecessor CINT",
	    YT_BASIC_FAULT_ROUTE_START_PREDECESSOR_CINT))
		return false;
	route_process_write_word(process, route_index_address(
	    YT_ROUTE_WORKSPACE_ADDRESS, start), -1);
	if (avoid_enabled) {
		size_t position;

		for (position = 0U; position < YT_ROUTE_AVOID_COUNT;
		    ++position) {
			float value = route_process_read_single(process,
			    (uint16_t)(YT_ROUTE_AVOID_ADDRESS + 4U * position));
			int16_t blocked;
			int16_t marker;

			if (!route_process_write_single(process,
			    YT_ROUTE_AVOID_INDEX_ADDRESS, (float)(position + 1U),
			    error))
				return false;

			if (!route_integer_at(value, conversion_mode, &blocked, error,
			    "route avoid predecessor CINT",
			    YT_BASIC_FAULT_ROUTE_AVOID_PREDECESSOR_CINT))
				return false;
			if (!route_integer_at(value, conversion_mode, &marker, error,
			    "route avoid endpoint CINT",
			    YT_BASIC_FAULT_ROUTE_AVOID_ENDPOINT_CINT))
				return false;
			route_process_write_word(process, route_index_address(
			    YT_ROUTE_WORKSPACE_ADDRESS, blocked), marker);
			start_value = route_argument_start(arguments, process);
			destination_value = route_argument_destination(arguments,
			    process);
			if (value == start_value || value == destination_value)
				route_process_write_word(process,
				    YT_ROUTE_HEAD_ADDRESS, 2);
		}
		if (!route_process_write_single(process,
		    YT_ROUTE_AVOID_INDEX_ADDRESS,
		    (float)(YT_ROUTE_AVOID_COUNT + 1U), error))
			return false;
	}
	destination_value = route_argument_destination(arguments, process);
	if (!route_endpoint_at(destination_value, conversion_mode, &destination,
	    error, "route destination predecessor CINT",
	    YT_BASIC_FAULT_ROUTE_DESTINATION_PREDECESSOR_CINT))
		return false;

	while (yt_route_process_predecessor(process, destination) == 0
	    && route_process_read_word(process, YT_ROUTE_TAIL_ADDRESS)
	    >= route_process_read_word(process, YT_ROUTE_HEAD_ADDRESS)) {
		float raw_warps[6];
		int16_t warps[6];
		int16_t raw_head = route_process_read_word(process,
		    YT_ROUTE_HEAD_ADDRESS);
		int16_t head;
		int16_t current;
		int16_t expanded;
		size_t slot;

		if (!route_integer_at((float)raw_head, conversion_mode, &head,
		    error, "route FIFO head CINT",
		    YT_BASIC_FAULT_ROUTE_FIFO_HEAD_CINT))
			return false;
		current = yt_route_process_second(process, head);
		if (!route_integer_at((float)current, conversion_mode, &current,
		    error, "route FIFO node CINT",
		    YT_BASIC_FAULT_ROUTE_FIFO_NODE_CINT))
			return false;
		route_process_write_word(process, YT_ROUTE_CURRENT_ADDRESS,
		    current);
		if (!route_integer_at((float)current, conversion_mode, &expanded,
		    error, "route expanded-node CINT",
		    YT_BASIC_FAULT_ROUTE_EXPANDED_NODE_CINT))
			return false;
		if (!reader(reader_context, expanded, raw_warps, error))
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
		if (!route_argument_write_status(arguments, process, 1.0f, error))
			return false;
		*outcome = YT_ROUTE_NOT_FOUND;
		return true;
	}

	memset(seen, 0, sizeof(seen));
	route_process_write_word(process, YT_ROUTE_HEAD_ADDRESS, destination);
	while (yt_route_process_predecessor(process,
	    route_process_read_word(process, YT_ROUTE_HEAD_ADDRESS)) != -1) {
		int16_t raw_head = route_process_read_word(process,
		    YT_ROUTE_HEAD_ADDRESS);
		int16_t head;
		uint16_t key;
		int16_t prior;

		if (!route_integer_at((float)raw_head, conversion_mode, &head,
		    error, "route reconstruction child CINT",
		    YT_BASIC_FAULT_ROUTE_RECONSTRUCTION_CHILD_CINT))
			return false;
		key = (uint16_t)head;
		if ((seen[key / CHAR_BIT]
		    & (uint8_t)(1U << (key % CHAR_BIT))) != 0U) {
			*outcome = YT_ROUTE_BACK_EDGE;
			return true;
		}
		seen[key / CHAR_BIT] |= (uint8_t)(1U << (key % CHAR_BIT));
		prior = yt_route_process_predecessor(process, head);
		if (!route_integer_at((float)prior, conversion_mode, &prior,
		    error, "route reconstruction parent CINT",
		    YT_BASIC_FAULT_ROUTE_RECONSTRUCTION_PARENT_CINT))
			return false;
		route_process_write_word(process, route_index_address(
		    YT_ROUTE_SECOND_ADDRESS, prior), head);
		route_process_write_word(process, YT_ROUTE_HEAD_ADDRESS, prior);
	}
	destination_value = route_argument_destination(arguments, process);
	if (!route_endpoint_at(destination_value, conversion_mode, &destination,
	    error, "route next-hop CINT", YT_BASIC_FAULT_ROUTE_NEXT_HOP_CINT))
		return false;
	route_process_write_word(process, route_index_address(
	    YT_ROUTE_SECOND_ADDRESS, destination), 0);
	if (!route_argument_write_status(arguments, process, 0.0f, error))
		return false;
	*outcome = YT_ROUTE_FOUND;
	return true;
}

static bool
route_argument_pointer_valid(uint16_t address)
{
	return address < YT_ROUTE_WORKSPACE_ADDRESS
	    || address >= YT_ROUTE_WORKSPACE_ADDRESS + YT_ROUTE_WORKSPACE_BYTES;
}

bool
yt_route_process_build_at(uint16_t start_address,
    uint16_t destination_address, uint16_t status_address,
    uint8_t conversion_mode, struct yt_route_process *process,
    yt_route_sector_reader reader, void *reader_context,
    enum yt_route_outcome *outcome, struct yt_error *error)
{
	struct route_arguments arguments = {
		.start_addressed = true,
		.destination_addressed = true,
		.status_addressed = true,
		.start_address = start_address,
		.destination_address = destination_address,
		.status_address = status_address,
	};

	if (!route_argument_pointer_valid(start_address)
	    || !route_argument_pointer_valid(destination_address)
	    || !route_argument_pointer_valid(status_address))
		return route_error(error, "route argument workspace alias");
	return route_process_build(&arguments, conversion_mode, process, reader,
	    reader_context, outcome, error);
}

bool
yt_route_process_build_cells(uint16_t start_address,
    uint16_t destination_address, float *status, uint8_t conversion_mode,
    struct yt_route_process *process, yt_route_sector_reader reader,
    void *reader_context, enum yt_route_outcome *outcome,
    struct yt_error *error)
{
	struct route_arguments arguments = {
		.start_addressed = true,
		.destination_addressed = true,
		.status_addressed = false,
		.start_address = start_address,
		.destination_address = destination_address,
		.status = status,
	};

	if (!route_argument_pointer_valid(start_address)
	    || !route_argument_pointer_valid(destination_address))
		return route_error(error, "route argument workspace alias");
	return route_process_build(&arguments, conversion_mode, process, reader,
	    reader_context, outcome, error);
}

bool
yt_route_process_build(float start_value, float destination_value,
    float *status, uint8_t conversion_mode, struct yt_route_process *process,
    yt_route_sector_reader reader, void *reader_context,
    enum yt_route_outcome *outcome, struct yt_error *error)
{
	struct route_arguments arguments = {
		.start_addressed = false,
		.destination_addressed = false,
		.status_addressed = false,
		.start_value = start_value,
		.destination_value = destination_value,
		.status = status,
	};

	return route_process_build(&arguments, conversion_mode, process, reader,
	    reader_context, outcome, error);
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
