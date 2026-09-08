#include "yt_game.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARRAY_SIZE(values) (sizeof(values) / sizeof((values)[0]))

enum event_kind {
	EVENT_READ = 1,
	EVENT_DAY,
	EVENT_TIMER,
	EVENT_PRESENT,
	EVENT_INPUT,
	EVENT_CHECKPOINT,
};

struct stored_record {
	uint32_t physical;
	struct yt_record record;
};

struct event {
	enum event_kind kind;
	enum yt_profit_field_kind field_kind;
	enum yt_profit_output_kind output_kind;
	enum yt_profit_present_mode mode;
	enum yt_profit_checkpoint checkpoint;
	uint32_t physical;
	float expression;
	struct yt_nearest_style style;
	size_t length;
	uint8_t text[128];
};

struct input_value {
	uint8_t bytes[8];
	size_t length;
	bool available;
};

struct profit_tape {
	struct stored_record records[96];
	size_t record_count;
	struct event events[1024];
	size_t event_count;
	float days[128];
	size_t day_count;
	size_t day_position;
	float timers[128];
	size_t timer_count;
	size_t timer_position;
	struct input_value inputs[16];
	size_t input_count;
	size_t input_position;
	size_t fail_at;
	bool ansi;
};

static int
fail_at(const char *function, int line, const char *condition)
{
	fprintf(stderr, "test_profit: %s:%d: %s\n", function, line, condition);
	return EXIT_FAILURE;
}

#define CHECK(condition) do { \
	if (!(condition)) \
		return fail_at(__func__, __LINE__, #condition); \
} while (0)

static struct event *
add_event(struct profit_tape *tape, enum event_kind kind)
{
	struct event *event;

	if (tape->event_count == ARRAY_SIZE(tape->events)) {
		fprintf(stderr, "test_profit: event tape overflow\n");
		exit(EXIT_FAILURE);
	}
	event = &tape->events[tape->event_count++];
	memset(event, 0, sizeof(*event));
	event->kind = kind;
	return event;
}

static bool
should_fail(struct profit_tape *tape, struct yt_error *error)
{
	if (tape->fail_at == 0U || tape->event_count != tape->fail_at)
		return false;
	if (error != NULL)
		error->status = YT_IO_ERROR;
	return true;
}

static void
set_number(struct yt_record *record, size_t offset, float value)
{
	if (!yt_record_set_number(record, offset, value)) {
		fprintf(stderr, "test_profit: cannot encode fixture SINGLE\n");
		exit(EXIT_FAILURE);
	}
}

static struct yt_record
sector_record(float port, const float warps[6])
{
	struct yt_record record;
	size_t index;

	yt_record_blank(&record);
	set_number(&record, YT_F65, port);
	for (index = 0U; index < 6U; ++index)
		set_number(&record, YT_F41 + 4U * index, warps[index]);
	return record;
}

static struct yt_record
port_record(float klass, const float factors[3])
{
	struct yt_record record;
	size_t index;

	yt_record_blank(&record);
	set_number(&record, YT_F41, klass);
	for (index = 0U; index < 3U; ++index) {
		set_number(&record, YT_F49 + 4U * index, 1000.0f);
		set_number(&record, YT_F61 + 4U * index, 100.0f);
		set_number(&record, YT_F73 + 4U * index, factors[index]);
	}
	return record;
}

static void
store_record(struct profit_tape *tape, uint32_t physical,
    struct yt_record record)
{
	if (tape->record_count == ARRAY_SIZE(tape->records)) {
		fprintf(stderr, "test_profit: record tape overflow\n");
		exit(EXIT_FAILURE);
	}
	tape->records[tape->record_count].physical = physical;
	tape->records[tape->record_count].record = record;
	++tape->record_count;
}

static bool
read_record(void *context, enum yt_profit_field_kind kind, float expression,
    uint32_t physical, struct yt_record *record, struct yt_error *error)
{
	struct profit_tape *tape = context;
	struct event *event = add_event(tape, EVENT_READ);
	size_t index;

	event->field_kind = kind;
	event->expression = expression;
	event->physical = physical;
	if (should_fail(tape, error))
		return false;
	for (index = 0U; index < tape->record_count; ++index) {
		if (tape->records[index].physical == physical) {
			*record = tape->records[index].record;
			return true;
		}
	}
	if (error != NULL) {
		error->status = YT_IO_ERROR;
		error->system_error = 5;
	}
	return false;
}

static bool
observe_day(void *context, float *day, struct yt_error *error)
{
	struct profit_tape *tape = context;

	(void)add_event(tape, EVENT_DAY);
	if (should_fail(tape, error) || tape->day_position == tape->day_count)
		return false;
	*day = tape->days[tape->day_position++];
	return true;
}

static bool
observe_timer(void *context, float *seconds, struct yt_error *error)
{
	struct profit_tape *tape = context;

	(void)add_event(tape, EVENT_TIMER);
	if (should_fail(tape, error)
	    || tape->timer_position == tape->timer_count)
		return false;
	*seconds = tape->timers[tape->timer_position++];
	return true;
}

static bool
present(void *context, enum yt_profit_output_kind kind,
    enum yt_profit_present_mode mode, const uint8_t *text, size_t length,
    struct yt_nearest_style *style, struct yt_error *error)
{
	struct profit_tape *tape = context;
	struct event *event = add_event(tape, EVENT_PRESENT);

	event->output_kind = kind;
	event->mode = mode;
	event->style = *style;
	event->length = length;
	if (length > sizeof(event->text)) {
		if (error != NULL)
			error->status = YT_RANGE;
		return false;
	}
	if (length != 0U)
		memcpy(event->text, text, length);
	if (should_fail(tape, error))
		return false;
	if (mode == YT_PROFIT_PRESENT_BOLD_LINE
	    || (mode == YT_PROFIT_PRESENT_BOLD_RAW && tape->ansi))
		style->bold = 1.0f;
	if (tape->ansi) {
		style->bold = 0.0f;
		style->blink = 0.0f;
	}
	return true;
}

static bool
input(void *context, uint8_t *text, size_t capacity, size_t *length,
    bool *available, struct yt_error *error)
{
	struct profit_tape *tape = context;
	struct input_value *value;

	(void)add_event(tape, EVENT_INPUT);
	if (should_fail(tape, error) || tape->input_position == tape->input_count)
		return false;
	value = &tape->inputs[tape->input_position++];
	if (value->length > capacity)
		return false;
	if (value->length != 0U)
		memcpy(text, value->bytes, value->length);
	*length = value->length;
	*available = value->available;
	return true;
}

static void
uppercase(void *context, uint8_t *text, size_t length)
{
	(void)context;
	qb_compat_upper_n(text, length);
}

static bool
checkpoint(void *context, enum yt_profit_checkpoint point,
    struct yt_error *error)
{
	struct profit_tape *tape = context;
	struct event *event = add_event(tape, EVENT_CHECKPOINT);

	event->checkpoint = point;
	return !should_fail(tape, error);
}

static const struct yt_profit_ops profit_ops = {
	read_record,
	observe_day,
	observe_timer,
	present,
	input,
	uppercase,
	checkpoint,
};

static struct yt_profit_state
base_state(bool global)
{
	struct yt_profit_state state;

	memset(&state, 0, sizeof(state));
	state.global = global;
	state.current_sector_record = 53.0f;
	state.sector_record_offset = 51.0f;
	state.port_record_offset = global ? 55.0f : 2055.0f;
	state.base_price[0] = 20.0f;
	state.base_price[1] = 30.0f;
	state.base_price[2] = 40.0f;
	state.style.foreground = 1.0f;
	return state;
}

static void
add_clock(struct profit_tape *tape, float day, float timer)
{
	if (tape->day_count == ARRAY_SIZE(tape->days)
	    || tape->timer_count == ARRAY_SIZE(tape->timers)) {
		fprintf(stderr, "test_profit: clock tape overflow\n");
		exit(EXIT_FAILURE);
	}
	tape->days[tape->day_count++] = day;
	tape->timers[tape->timer_count++] = timer;
}

static void
add_input(struct profit_tape *tape, const uint8_t *bytes, size_t length,
    bool available)
{
	struct input_value *value;

	if (tape->input_count == ARRAY_SIZE(tape->inputs)
	    || length > sizeof(tape->inputs[0].bytes)) {
		fprintf(stderr, "test_profit: input tape overflow\n");
		exit(EXIT_FAILURE);
	}
	value = &tape->inputs[tape->input_count++];
	memset(value, 0, sizeof(*value));
	if (length != 0U)
		memcpy(value->bytes, bytes, length);
	value->length = length;
	value->available = available;
}

static const struct event *
nth_event(const struct profit_tape *tape, enum event_kind kind,
    size_t ordinal)
{
	size_t index;
	size_t seen = 0U;

	for (index = 0U; index < tape->event_count; ++index) {
		if (tape->events[index].kind != kind)
			continue;
		if (seen++ == ordinal)
			return &tape->events[index];
	}
	return NULL;
}

static const struct event *
nth_output(const struct profit_tape *tape,
    enum yt_profit_output_kind kind, size_t ordinal)
{
	size_t index;
	size_t seen = 0U;

	for (index = 0U; index < tape->event_count; ++index) {
		if (tape->events[index].kind != EVENT_PRESENT
		    || tape->events[index].output_kind != kind)
			continue;
		if (seen++ == ordinal)
			return &tape->events[index];
	}
	return NULL;
}

static struct profit_tape
canonical_adjacent_tape(void)
{
	static const float source_warps[6] = {4, 3, 0, 0, 0, 0};
	static const float empty_warps[6] = {0, 0, 0, 0, 0, 0};
	static const float source_factor[3] = {-20, -30, 50};
	static const float organics_factor[3] = {-40, 50, -25};
	static const float ore_factor[3] = {50, -20, -30};
	struct profit_tape tape = {0};

	store_record(&tape, 53U, sector_record(2.0f, source_warps));
	store_record(&tape, 2057U, port_record(1.0f, source_factor));
	store_record(&tape, 55U, sector_record(4.0f, empty_warps));
	store_record(&tape, 2059U, port_record(3.0f, ore_factor));
	store_record(&tape, 54U, sector_record(3.0f, empty_warps));
	store_record(&tape, 2058U, port_record(2.0f, organics_factor));
	add_clock(&tape, 0.0f, 0.0f);
	add_clock(&tape, 0.0f, 0.0f);
	add_clock(&tape, 0.0f, 0.0f);
	return tape;
}

static struct profit_tape
canonical_global_tape(void)
{
	static const float warps_two[6] = {4, 3, 0, 0, 0, 0};
	static const float warps_three[6] = {2, 4, 0, 0, 0, 0};
	static const float warps_four[6] = {2, 3, 0, 0, 0, 0};
	static const float source_factor[3] = {-20, -30, 50};
	static const float organics_factor[3] = {-40, 50, -25};
	static const float ore_factor[3] = {50, -20, -30};
	struct profit_tape tape = {0};
	int sample;

	store_record(&tape, 53U, sector_record(2.0f, warps_two));
	store_record(&tape, 54U, sector_record(3.0f, warps_three));
	store_record(&tape, 55U, sector_record(4.0f, warps_four));
	store_record(&tape, 57U, port_record(1.0f, source_factor));
	store_record(&tape, 58U, port_record(2.0f, organics_factor));
	store_record(&tape, 59U, port_record(3.0f, ore_factor));
	for (sample = 0; sample < 6; ++sample)
		add_clock(&tape, 0.0f, 0.0f);
	return tape;
}

static int
test_adjacent_canonical(void)
{
	static const uint8_t row_one[] =
	    "   2,   4 Equ -> Ore @ Profit of 46 ";
	static const uint8_t row_two[] =
	    "   2,   3 Equ -> Org @ Profit of 54 ";
	static const uint32_t reads[] = {53, 2057, 55, 2059, 54, 2058};
	struct profit_tape tape = canonical_adjacent_tape();
	struct yt_profit_state state = base_state(false);
	struct yt_error error = {0};
	size_t index;

	CHECK(yt_profit_run(&state, &profit_ops, &tape, &error));
	CHECK(state.complete && state.result == YT_PROFIT_COMPLETE
	    && state.rows == 2U && state.outputs == 5U && state.reads == 6U
	    && state.day_observations == 3U
	    && state.timer_observations == 3U
	    && state.field_valid && state.field_kind == YT_PROFIT_FIELD_PORT
	    && state.field_record == 2058U && state.style.foreground == 3.0f);
	CHECK(memcmp(state.initial_result_raw, "\x00\x00\x20\x00", 4U) == 0
	    && memcmp(state.result_count_raw, "\x00\x00\x00\x00", 4U) == 0);
	CHECK(nth_output(&tape, YT_PROFIT_LEADING_BLANK, 0U)->mode
	    == YT_PROFIT_PRESENT_LINE
	    && nth_output(&tape, YT_PROFIT_LEADING_BLANK, 0U)->style.foreground
	    == 7.0f
	    && nth_output(&tape, YT_PROFIT_TITLE, 0U)->mode
	    == YT_PROFIT_PRESENT_BOLD_LINE
	    && nth_output(&tape, YT_PROFIT_TITLE_BLANK, 0U)->mode
	    == YT_PROFIT_PRESENT_LINE);
	for (index = 0U; index < ARRAY_SIZE(reads); ++index) {
		const struct event *event = nth_event(&tape, EVENT_READ, index);
		CHECK(event != NULL && event->physical == reads[index]);
	}
	CHECK(nth_output(&tape, YT_PROFIT_ROW, 0U)->length
	    == sizeof(row_one) - 1U
	    && nth_output(&tape, YT_PROFIT_ROW, 0U)->mode
	    == YT_PROFIT_PRESENT_BOLD_LINE
	    && nth_output(&tape, YT_PROFIT_ROW, 0U)->style.foreground == 2.0f
	    && memcmp(nth_output(&tape, YT_PROFIT_ROW, 0U)->text,
	    row_one, sizeof(row_one) - 1U) == 0);
	CHECK(nth_output(&tape, YT_PROFIT_ROW, 1U)->length
	    == sizeof(row_two) - 1U
	    && nth_output(&tape, YT_PROFIT_ROW, 1U)->mode
	    == YT_PROFIT_PRESENT_BOLD_LINE
	    && nth_output(&tape, YT_PROFIT_ROW, 1U)->style.foreground == 3.0f
	    && memcmp(nth_output(&tape, YT_PROFIT_ROW, 1U)->text,
	    row_two, sizeof(row_two) - 1U) == 0);
	return EXIT_SUCCESS;
}

static int
test_adjacent_terminal_routes(void)
{
	static const float empty[6] = {0, 0, 0, 0, 0, 0};
	static const float factors[3] = {-20, -30, 50};
	struct profit_tape no_port = {0};
	struct profit_tape no_rows = {0};
	struct yt_profit_state no_port_state = base_state(false);
	struct yt_profit_state no_rows_state = base_state(false);
	struct yt_error error = {0};

	store_record(&no_port, 53U, sector_record(1.0f, empty));
	CHECK(yt_profit_run(&no_port_state, &profit_ops, &no_port, &error));
	CHECK(no_port_state.result == YT_PROFIT_NO_CURRENT_PORT_RESULT
	    && no_port_state.reads == 1U && no_port_state.outputs == 4U);

	store_record(&no_rows, 53U, sector_record(2.0f, empty));
	store_record(&no_rows, 2057U, port_record(1.0f, factors));
	add_clock(&no_rows, 4.0f, 120.0f);
	CHECK(yt_profit_run(&no_rows_state, &profit_ops, &no_rows, &error));
	CHECK(no_rows_state.result == YT_PROFIT_NO_RESULTS_RESULT
	    && no_rows_state.rows == 0U && no_rows_state.outputs == 4U
	    && no_rows_state.reads == 2U
	    && no_rows_state.day_observations == 1U
	    && no_rows_state.timer_observations == 1U);
	return EXIT_SUCCESS;
}

static int
test_adjacent_fractional_records(void)
{
	static const float warps[6] = {4, 0, 0, 0, 0, 0};
	static const float empty[6] = {0, 0, 0, 0, 0, 0};
	static const float source_factor[3] = {-20, -30, 50};
	static const float target_factor[3] = {50, -20, -30};
	struct profit_tape tape = {0};
	struct yt_profit_state state = base_state(false);
	struct yt_error error = {0};
	const struct event *row;

	state.current_sector_record = 53.75f;
	store_record(&tape, 53U, sector_record(2.75f, warps));
	store_record(&tape, 2057U, port_record(1.0f, source_factor));
	store_record(&tape, 55U, sector_record(4.25f, empty));
	store_record(&tape, 2059U, port_record(3.0f, target_factor));
	add_clock(&tape, 0.0f, 0.0f);
	add_clock(&tape, 0.0f, 0.0f);
	CHECK(yt_profit_run(&state, &profit_ops, &tape, &error));
	CHECK(state.rows == 1U && state.field_record == 2059U);
	CHECK(nth_event(&tape, EVENT_READ, 0U)->expression == 53.75f
	    && nth_event(&tape, EVENT_READ, 1U)->expression == 2057.75f
	    && nth_event(&tape, EVENT_READ, 3U)->expression == 2059.25f);
	row = nth_output(&tape, YT_PROFIT_ROW, 0U);
	CHECK(row != NULL && row->length == 36U
	    && memcmp(row->text, "2.75,   4", 9U) == 0);
	return EXIT_SUCCESS;
}

static int
test_global_canonical(void)
{
	static const uint8_t row_one[] =
	    "   2,   4 Equ -> Ore @ Profit of 46 ";
	static const uint8_t row_two[] =
	    "   2,   3 Equ -> Org @ Profit of 54 ";
	static const uint8_t row_three[] =
	    "   3,   4 Org -> Ore @ Profit of 39 ";
	struct profit_tape tape = canonical_global_tape();
	struct yt_profit_state state = base_state(true);
	struct yt_error error = {0};
	uint8_t raw[4];

	CHECK(yt_profit_run(&state, &profit_ops, &tape, &error));
	CHECK(state.complete && !state.stopped
	    && state.result == YT_PROFIT_COMPLETE && state.maximum_sector == 4.0f
	    && state.rows == 3U
	    && state.result_count == 3.0f && state.pager_prompts == 0U
	    && state.day_observations == 6U
	    && state.timer_observations == 6U && state.style.foreground == 7.0f
	    && state.field_kind == YT_PROFIT_FIELD_SECTOR
	    && state.field_record == 54U);
	CHECK(memcmp(state.initial_result_raw, "\x00\x00\x04\x00", 4U) == 0);
	CHECK(qb_mbf32_encode(3.0f, raw) == QB_MBF_OK
	    && memcmp(raw, state.result_count_raw, sizeof(raw)) == 0);
	CHECK(nth_output(&tape, YT_PROFIT_ROW, 0U)->length == 36U
	    && memcmp(nth_output(&tape, YT_PROFIT_ROW, 0U)->text,
	    row_one, sizeof(row_one) - 1U) == 0
	    && memcmp(nth_output(&tape, YT_PROFIT_ROW, 1U)->text,
	    row_two, sizeof(row_two) - 1U) == 0
	    && memcmp(nth_output(&tape, YT_PROFIT_ROW, 2U)->text,
	    row_three, sizeof(row_three) - 1U) == 0);
	CHECK(nth_output(&tape, YT_PROFIT_COLUMN_SEPARATOR, 0U) != NULL
	    && nth_output(&tape, YT_PROFIT_ROW, 0U)->mode
	    == YT_PROFIT_PRESENT_BOLD_RAW
	    && nth_output(&tape, YT_PROFIT_COLUMN_SEPARATOR, 0U)->mode
	    == YT_PROFIT_PRESENT_BOLD_RAW
	    && nth_output(&tape, YT_PROFIT_COLUMN_SEPARATOR, 0U)->style.foreground
	    == 6.0f
	    && nth_output(&tape, YT_PROFIT_ROW_END, 0U) != NULL
	    && nth_output(&tape, YT_PROFIT_ROW_END, 0U)->mode
	    == YT_PROFIT_PRESENT_LINE
	    && nth_output(&tape, YT_PROFIT_COLUMN_SEPARATOR, 1U) != NULL
	    && nth_output(&tape, YT_PROFIT_END_BANNER, 0U) != NULL
	    && nth_output(&tape, YT_PROFIT_END_BANNER, 0U)->mode
	    == YT_PROFIT_PRESENT_BOLD_LINE
	    && nth_output(&tape, YT_PROFIT_END_BANNER, 0U)->style.foreground
	    == 7.0f);
	return EXIT_SUCCESS;
}

static int
test_zero_iteration_global(void)
{
	struct profit_tape tape = {0};
	struct yt_profit_state state = base_state(true);
	struct yt_error error = {0};
	struct yt_record inherited;

	state.port_record_offset = 52.0f;
	memset(inherited.bytes, 0xa5, sizeof(inherited.bytes));
	state.field = inherited;
	state.field_kind = YT_PROFIT_FIELD_PLAYER;
	state.field_record = 777U;
	state.field_valid = true;
	CHECK(yt_profit_run(&state, &profit_ops, &tape, &error));
	CHECK(state.complete && state.result == YT_PROFIT_COMPLETE
	    && state.maximum_sector == 1.0f && state.rows == 0U
	    && state.reads == 0U && state.outputs == 2U && state.field_valid
	    && state.field_kind == YT_PROFIT_FIELD_PLAYER
	    && state.field_record == 777U
	    && memcmp(state.field.bytes, inherited.bytes,
	    sizeof(inherited.bytes)) == 0
	    && memcmp(state.initial_result_raw, "\x00\x00\x04\x00", 4U) == 0
	    && memcmp(state.result_count_raw, "\x00\x00\x04\x00", 4U) == 0
	    && nth_output(&tape, YT_PROFIT_END_BANNER, 0U) != NULL);
	return EXIT_SUCCESS;
}

static int
test_fractional_global_upper_bound(void)
{
	static const float empty[6] = {0, 0, 0, 0, 0, 0};
	struct profit_tape tape = {0};
	struct yt_profit_state state = base_state(true);
	struct yt_error error = {0};

	state.port_record_offset = 54.5f;
	store_record(&tape, 53U, sector_record(0.0f, empty));
	store_record(&tape, 54U, sector_record(0.0f, empty));
	CHECK(yt_profit_run(&state, &profit_ops, &tape, &error));
	CHECK(state.maximum_sector == 3.5f && state.reads == 2U
	    && state.field_kind == YT_PROFIT_FIELD_SECTOR
	    && state.field_record == 54U
	    && nth_event(&tape, EVENT_READ, 0U)->physical == 53U
	    && nth_event(&tape, EVENT_READ, 1U)->physical == 54U
	    && nth_event(&tape, EVENT_READ, 2U) == NULL);
	return EXIT_SUCCESS;
}

static int
test_warp_conversion_precedes_source_port(void)
{
	static const float overflow_warps[6] = {4, 0, 0, 0, 0, 40000};
	struct profit_tape adjacent = {0};
	struct profit_tape global = {0};
	struct yt_profit_state adjacent_state = base_state(false);
	struct yt_profit_state global_state = base_state(true);
	struct yt_error error = {0};

	store_record(&adjacent, 53U, sector_record(2.0f, overflow_warps));
	CHECK(!yt_profit_run(&adjacent_state, &profit_ops, &adjacent, &error));
	CHECK(error.status == YT_RANGE && adjacent_state.reads == 1U
	    && adjacent_state.outputs == 3U
	    && adjacent_state.day_observations == 0U
	    && adjacent_state.field_kind == YT_PROFIT_FIELD_SECTOR
	    && nth_event(&adjacent, EVENT_READ, 1U) == NULL);

	memset(&error, 0, sizeof(error));
	store_record(&global, 53U, sector_record(0.0f, overflow_warps));
	CHECK(!yt_profit_run(&global_state, &profit_ops, &global, &error));
	CHECK(error.status == YT_RANGE && global_state.reads == 1U
	    && global_state.outputs == 1U
	    && global_state.day_observations == 0U
	    && global_state.field_kind == YT_PROFIT_FIELD_SECTOR
	    && nth_event(&global, EVENT_READ, 1U) == NULL);
	return EXIT_SUCCESS;
}

static int
test_failed_first_get_preserves_inherited_field(void)
{
	struct profit_tape tape = {0};
	struct yt_profit_state state = base_state(false);
	struct yt_error error = {0};
	struct yt_record inherited;

	memset(inherited.bytes, 0x5a, sizeof(inherited.bytes));
	state.field = inherited;
	state.field_kind = YT_PROFIT_FIELD_PLAYER;
	state.field_record = 777U;
	state.field_valid = true;
	CHECK(!yt_profit_run(&state, &profit_ops, &tape, &error));
	CHECK(state.reads == 0U && state.outputs == 3U && state.field_valid
	    && state.field_kind == YT_PROFIT_FIELD_PLAYER
	    && state.field_record == 777U
	    && memcmp(state.field.bytes, inherited.bytes,
	    sizeof(inherited.bytes)) == 0);
	return EXIT_SUCCESS;
}

static int
test_class_four_retains_foreground(void)
{
	static const float warps[6] = {3, 0, 0, 0, 0, 0};
	static const float empty[6] = {0, 0, 0, 0, 0, 0};
	static const float class_four_factor[3] = {50, -20, -30};
	static const float class_one_factor[3] = {-20, -30, 50};
	static const uint8_t expected[] =
	    "   2,   3 Ore -> Equ @ Profit of 32 ";
	static const uint8_t fractional_expected[] =
	    "   2,   3 Ore -> Equ @ Profit of 64 ";
	struct profit_tape tape = {0};
	struct profit_tape global = {0};
	struct profit_tape fractional = {0};
	struct yt_profit_state state = base_state(false);
	struct yt_profit_state global_state = base_state(true);
	struct yt_profit_state fractional_state = base_state(false);
	struct yt_error error = {0};
	const struct event *row;

	store_record(&tape, 53U, sector_record(2.0f, warps));
	store_record(&tape, 2057U,
	    port_record(4.0f, class_four_factor));
	store_record(&tape, 54U, sector_record(3.0f, empty));
	store_record(&tape, 2058U, port_record(1.0f, class_one_factor));
	add_clock(&tape, 0.0f, 0.0f);
	add_clock(&tape, 0.0f, 0.0f);
	CHECK(yt_profit_run(&state, &profit_ops, &tape, &error));
	row = nth_output(&tape, YT_PROFIT_ROW, 0U);
	CHECK(state.rows == 1U && state.style.foreground == 7.0f
	    && row != NULL && row->style.foreground == 7.0f
	    && row->length == sizeof(expected) - 1U
	    && memcmp(row->text, expected, sizeof(expected) - 1U) == 0);

	global_state.port_record_offset = 54.0f;
	global_state.style.foreground = 5.0f;
	store_record(&global, 53U, sector_record(2.0f, warps));
	store_record(&global, 54U, sector_record(3.0f, empty));
	store_record(&global, 56U, port_record(4.0f, class_four_factor));
	store_record(&global, 57U, port_record(1.0f, class_one_factor));
	add_clock(&global, 0.0f, 0.0f);
	add_clock(&global, 0.0f, 0.0f);
	add_clock(&global, 0.0f, 0.0f);
	CHECK(yt_profit_run(&global_state, &profit_ops, &global, &error));
	row = nth_output(&global, YT_PROFIT_ROW, 0U);
	CHECK(global_state.rows == 1U && global_state.style.foreground == 7.0f
	    && row != NULL && row->style.foreground == 5.0f
	    && row->length == sizeof(expected) - 1U
	    && memcmp(row->text, expected, sizeof(expected) - 1U) == 0);

	store_record(&fractional, 53U, sector_record(2.0f, warps));
	store_record(&fractional, 2057U,
	    port_record(1.5f, class_four_factor));
	store_record(&fractional, 54U, sector_record(3.0f, empty));
	store_record(&fractional, 2058U,
	    port_record(1.0f, class_one_factor));
	add_clock(&fractional, 0.0f, 0.0f);
	add_clock(&fractional, 0.0f, 0.0f);
	CHECK(yt_profit_run(&fractional_state, &profit_ops, &fractional,
	    &error));
	row = nth_output(&fractional, YT_PROFIT_ROW, 0U);
	CHECK(row != NULL && row->length == sizeof(fractional_expected) - 1U
	    && memcmp(row->text, fractional_expected,
	    sizeof(fractional_expected) - 1U) == 0);
	return EXIT_SUCCESS;
}

static int
test_failure_prefix_residue(void)
{
	struct profit_tape date_cut = canonical_adjacent_tape();
	struct profit_tape timer_cut = canonical_adjacent_tape();
	struct profit_tape target_get_cut = canonical_adjacent_tape();
	struct profit_tape row_cut = canonical_adjacent_tape();
	struct yt_profit_state date_state = base_state(false);
	struct yt_profit_state timer_state = base_state(false);
	struct yt_profit_state target_state = base_state(false);
	struct yt_profit_state row_state = base_state(false);
	struct yt_error error = {0};

	/* A failed DATE call retains the incoming process game-day cell. */
	date_state.current_day = 19.0f;
	date_cut.fail_at = 6U;
	CHECK(!yt_profit_run(&date_state, &profit_ops, &date_cut, &error));
	CHECK(date_state.current_day == 19.0f
	    && date_state.day_observations == 0U
	    && date_state.timer_observations == 0U
	    && date_state.field_kind == YT_PROFIT_FIELD_PORT
	    && date_state.field_record == 2057U && date_state.rows == 0U);

	/* Three headings, current-sector GET, current-port GET, DATE, TIMER. */
	memset(&error, 0, sizeof(error));
	timer_cut.days[0] = 7.0f;
	timer_cut.fail_at = 7U;
	CHECK(!yt_profit_run(&timer_state, &profit_ops, &timer_cut, &error));
	CHECK(timer_state.current_day == 7.0f
	    && timer_state.day_observations == 1U
	    && timer_state.timer_observations == 0U
	    && timer_state.field_kind == YT_PROFIT_FIELD_PORT
	    && timer_state.field_record == 2057U && timer_state.rows == 0U);

	/* The failed target-port GET retains the successful target-sector FIELD. */
	memset(&error, 0, sizeof(error));
	target_get_cut.fail_at = 10U;
	CHECK(!yt_profit_run(&target_state, &profit_ops, &target_get_cut,
	    &error));
	CHECK(target_state.field_kind == YT_PROFIT_FIELD_SECTOR
	    && target_state.field_record == 55U && target_state.reads == 3U
	    && target_state.rows == 0U);

	/* Pair color is assigned before the fallible row-construction seam. */
	memset(&error, 0, sizeof(error));
	row_cut.fail_at = 13U;
	CHECK(!yt_profit_run(&row_state, &profit_ops, &row_cut, &error));
	CHECK(row_state.style.foreground == 2.0f && row_state.rows == 0U
	    && nth_output(&row_cut, YT_PROFIT_ROW, 0U) == NULL);
	return EXIT_SUCCESS;
}

static int
test_global_counter_precedes_calculation(void)
{
	static const float warps[6] = {3, 0, 0, 0, 0, 0};
	static const float empty[6] = {0, 0, 0, 0, 0, 0};
	static const float factors[3] = {-20, -30, 50};
	struct profit_tape tape = {0};
	struct yt_profit_state state = base_state(true);
	struct yt_error error = {0};
	uint8_t one[4];

	state.port_record_offset = 54.0f;
	store_record(&tape, 53U, sector_record(2.0f, warps));
	store_record(&tape, 54U, sector_record(3.0f, empty));
	store_record(&tape, 56U, port_record(0.0f, factors));
	store_record(&tape, 57U, port_record(1.0f, factors));
	add_clock(&tape, 0.0f, 0.0f);
	add_clock(&tape, 0.0f, 0.0f);
	CHECK(!yt_profit_run(&state, &profit_ops, &tape, &error));
	CHECK(error.status == YT_INVALID
	    && state.result == YT_PROFIT_UNRESOLVED_RAW_MEMORY
	    && state.result_count == 1.0f && state.rows == 0U
	    && state.style.foreground == 1.0f);
	CHECK(qb_mbf32_encode(1.0f, one) == QB_MBF_OK
	    && memcmp(one, state.result_count_raw, sizeof(one)) == 0);
	return EXIT_SUCCESS;
}

static int
test_global_pager_and_cr_rule(void)
{
	static const float target_warps[6] = {0, 0, 0, 0, 0, 0};
	static const float source_factor[3] = {-20, -30, 50};
	static const float target_factor[3] = {-40, 50, -25};
	struct profit_tape tape = {0};
	struct profit_tape input_cut;
	struct profit_tape cr_tape;
	struct yt_profit_state state = base_state(true);
	struct yt_profit_state cut_state = base_state(true);
	struct yt_profit_state cr_state = base_state(true);
	struct yt_error error = {0};
	float warps[6] = {10, 10, 10, 10, 10, 10};
	uint8_t cr = '\r';
	uint8_t lf = '\n';
	uint8_t n = 'n';
	int source;
	uint8_t raw[4];

	state.port_record_offset = 61.0f;
	for (source = 2; source <= 9; ++source) {
		store_record(&tape, (uint32_t)(51 + source),
		    sector_record((float)source, warps));
		store_record(&tape, (uint32_t)(61 + source),
		    port_record(1.0f, source_factor));
	}
	store_record(&tape, 61U, sector_record(10.0f, target_warps));
	store_record(&tape, 71U, port_record(2.0f, target_factor));
	for (source = 0; source < 52; ++source)
		add_clock(&tape, 0.0f, 0.0f);
	add_input(&tape, NULL, 0U, false);
	add_input(&tape, &lf, 1U, true);
	add_input(&tape, &n, 1U, true);
	input_cut = tape;
	cr_tape = tape;
	cr_tape.input_count = 1U;
	cr_tape.inputs[0].bytes[0] = cr;
	cr_tape.inputs[0].length = 1U;
	cr_tape.inputs[0].available = true;
	for (source = 0; source < 5; ++source)
		add_clock(&cr_tape, 0.0f, 0.0f);
	CHECK(yt_profit_run(&state, &profit_ops, &tape, &error));
	CHECK(state.complete && state.stopped
	    && state.result == YT_PROFIT_STOPPED_BY_N
	    && state.rows == 44U && state.result_count == 44.0f
	    && state.pager_prompts == 2U
	    && nth_output(&tape, YT_PROFIT_END_BANNER, 0U) == NULL);
	CHECK(qb_mbf32_encode(44.0f, raw) == QB_MBF_OK
	    && memcmp(raw, state.result_count_raw, sizeof(raw)) == 0);
	CHECK(nth_output(&tape, YT_PROFIT_PAGER_ECHO, 0U)->length == 1U
	    && nth_output(&tape, YT_PROFIT_PAGER_PROMPT, 0U)->mode
	    == YT_PROFIT_PRESENT_RAW
	    && nth_output(&tape, YT_PROFIT_PAGER_PROMPT, 0U)->style.foreground
	    == 6.0f
	    && nth_output(&tape, YT_PROFIT_PAGER_ECHO, 0U)->mode
	    == YT_PROFIT_PRESENT_LINE
	    && nth_output(&tape, YT_PROFIT_PAGER_ECHO, 0U)->text[0] == '\n'
	    && nth_output(&tape, YT_PROFIT_PAGER_ECHO, 1U)->text[0] == 'N');
	{
		const struct event *first_input = nth_event(&tape, EVENT_INPUT, 0U);

		CHECK(first_input != NULL);
		input_cut.fail_at = (size_t)(first_input - tape.events) + 1U;
		cut_state.port_record_offset = 61.0f;
		memset(&error, 0, sizeof(error));
		CHECK(!yt_profit_run(&cut_state, &profit_ops, &input_cut, &error));
		CHECK(error.status == YT_IO_ERROR && cut_state.rows == 44U
		    && cut_state.style.foreground == 6.0f
		    && cut_state.pager_prompts == 1U
		    && nth_output(&input_cut, YT_PROFIT_PAGER_PROMPT, 0U) != NULL
		    && nth_output(&input_cut, YT_PROFIT_PAGER_ECHO, 0U) == NULL);
	}
	cr_state.port_record_offset = 61.0f;
	memset(&error, 0, sizeof(error));
	CHECK(yt_profit_run(&cr_state, &profit_ops, &cr_tape, &error));
	CHECK(cr_state.complete && !cr_state.stopped && cr_state.rows == 48U
	    && cr_state.pager_prompts == 1U
	    && nth_output(&cr_tape, YT_PROFIT_PAGER_ECHO, 0U)->length == 1U
	    && nth_output(&cr_tape, YT_PROFIT_PAGER_ECHO, 0U)->text[0] == 'Y'
	    && nth_output(&cr_tape, YT_PROFIT_END_BANNER, 0U) != NULL);
	return EXIT_SUCCESS;
}

static int
test_every_adjacent_provider_cut(void)
{
	struct profit_tape complete = canonical_adjacent_tape();
	struct yt_profit_state complete_state = base_state(false);
	struct yt_error error = {0};
	size_t cut;

	CHECK(yt_profit_run(&complete_state, &profit_ops, &complete, &error));
	for (cut = 1U; cut <= complete.event_count; ++cut) {
		struct profit_tape tape = canonical_adjacent_tape();
		struct yt_profit_state state = base_state(false);

		memset(&error, 0, sizeof(error));
		tape.fail_at = cut;
		CHECK(!yt_profit_run(&state, &profit_ops, &tape, &error));
		CHECK(error.status == YT_IO_ERROR && tape.event_count == cut
		    && !state.complete);
	}
	return EXIT_SUCCESS;
}

static int
test_every_global_provider_cut(void)
{
	struct profit_tape complete = canonical_global_tape();
	struct yt_profit_state complete_state = base_state(true);
	struct yt_error error = {0};
	size_t cut;

	CHECK(yt_profit_run(&complete_state, &profit_ops, &complete, &error));
	for (cut = 1U; cut <= complete.event_count; ++cut) {
		struct profit_tape tape = canonical_global_tape();
		struct yt_profit_state state = base_state(true);

		memset(&error, 0, sizeof(error));
		tape.fail_at = cut;
		CHECK(!yt_profit_run(&state, &profit_ops, &tape, &error));
		CHECK(error.status == YT_IO_ERROR && tape.event_count == cut
		    && !state.complete);
	}
	return EXIT_SUCCESS;
}

int
main(void)
{
	if (test_adjacent_canonical() != EXIT_SUCCESS
	    || test_adjacent_terminal_routes() != EXIT_SUCCESS
	    || test_adjacent_fractional_records() != EXIT_SUCCESS
	    || test_global_canonical() != EXIT_SUCCESS
	    || test_zero_iteration_global() != EXIT_SUCCESS
	    || test_fractional_global_upper_bound() != EXIT_SUCCESS
	    || test_warp_conversion_precedes_source_port() != EXIT_SUCCESS
	    || test_failed_first_get_preserves_inherited_field() != EXIT_SUCCESS
	    || test_class_four_retains_foreground() != EXIT_SUCCESS
	    || test_failure_prefix_residue() != EXIT_SUCCESS
	    || test_global_counter_precedes_calculation() != EXIT_SUCCESS
	    || test_global_pager_and_cr_rule() != EXIT_SUCCESS
	    || test_every_adjacent_provider_cut() != EXIT_SUCCESS
	    || test_every_global_provider_cut() != EXIT_SUCCESS)
		return EXIT_FAILURE;
	puts("test_profit: ok");
	return EXIT_SUCCESS;
}
