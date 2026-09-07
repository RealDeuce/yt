#include "yt_game.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARRAY_SIZE(values) (sizeof(values) / sizeof((values)[0]))

enum tape_event_kind {
	TAPE_READ = 1,
	TAPE_DAY,
	TAPE_TIMER,
	TAPE_PRESENT,
	TAPE_INPUT,
};

struct tape_record {
	uint32_t physical;
	struct yt_record record;
};

struct tape_event {
	enum tape_event_kind kind;
	enum yt_nearest_field_kind field_kind;
	enum yt_nearest_output_kind output_kind;
	enum yt_nearest_present_mode mode;
	uint32_t physical;
	float expression;
	struct yt_nearest_style style;
	struct yt_nearest_market market;
	float page_count;
	uint8_t page_count_raw[4];
	size_t length;
	uint8_t text[128];
};

struct nearest_tape {
	struct tape_record records[128];
	size_t record_count;
	struct tape_event events[2048];
	size_t event_count;
	float days[64];
	size_t day_count;
	size_t day_position;
	float timers[64];
	size_t timer_count;
	size_t timer_position;
	uint8_t inputs[64];
	bool input_available[64];
	size_t input_count;
	size_t input_position;
	bool ansi;
	struct yt_nearest_state *state;
	size_t fail_at;
};

static int
fail_at(const char *function, int line, const char *condition)
{
	fprintf(stderr, "test_nearest: %s:%d: %s\n", function, line,
	    condition);
	return EXIT_FAILURE;
}

#define CHECK(condition) do { \
	if (!(condition)) \
		return fail_at(__func__, __LINE__, #condition); \
} while (0)

static void
set_number(struct yt_record *record, size_t offset, float value)
{
	if (!yt_record_set_number(record, offset, value)) {
		fprintf(stderr, "test_nearest: cannot encode fixture SINGLE\n");
		exit(EXIT_FAILURE);
	}
}

static struct yt_record
player_record(const char *name, float sector, float team)
{
	struct yt_record record;

	yt_record_blank(&record);
	yt_record_set_text(&record, (const uint8_t *)name, strlen(name));
	set_number(&record, YT_F57, sector);
	set_number(&record, YT_F89, team);
	return record;
}

static struct yt_record
sector_record(const float warps[6], float port)
{
	struct yt_record record;
	size_t index;

	yt_record_blank(&record);
	for (index = 0U; index < 6U; ++index)
		set_number(&record, YT_F41 + index * 4U, warps[index]);
	set_number(&record, YT_F65, port);
	return record;
}

static struct yt_record
port_record(const char *name, float klass, float owner,
    const float stock[3])
{
	static const float production[3] = {10.0f, 20.0f, 30.0f};
	struct yt_record record;
	float factor[3];
	size_t index;

	yt_record_blank(&record);
	yt_record_set_text(&record, (const uint8_t *)name, strlen(name));
	factor[0] = klass == 3.0f ? 10.0f : -10.0f;
	factor[1] = klass == 2.0f ? 20.0f : -20.0f;
	factor[2] = klass == 1.0f ? 30.0f : -30.0f;
	set_number(&record, YT_F41, klass);
	for (index = 0U; index < 3U; ++index) {
		set_number(&record, YT_F49 + index * 4U, stock[index]);
		set_number(&record, YT_F61 + index * 4U, production[index]);
		set_number(&record, YT_F73 + index * 4U, factor[index]);
	}
	set_number(&record, YT_F85, (float)strlen(name));
	set_number(&record, YT_F97, owner);
	return record;
}

static void
add_record(struct nearest_tape *tape, uint32_t physical,
    struct yt_record record)
{
	if (tape->record_count == ARRAY_SIZE(tape->records)) {
		fprintf(stderr, "test_nearest: fixture record tape overflow\n");
		exit(EXIT_FAILURE);
	}
	tape->records[tape->record_count].physical = physical;
	tape->records[tape->record_count].record = record;
	++tape->record_count;
}

static struct tape_event *
add_event(struct nearest_tape *tape, enum tape_event_kind kind)
{
	struct tape_event *event;

	if (tape->event_count == ARRAY_SIZE(tape->events)) {
		fprintf(stderr, "test_nearest: fixture event tape overflow\n");
		exit(EXIT_FAILURE);
	}
	event = &tape->events[tape->event_count++];
	memset(event, 0, sizeof(*event));
	event->kind = kind;
	return event;
}

static bool
read_record(void *context, enum yt_nearest_field_kind kind, float expression,
    uint32_t physical, struct yt_record *record, struct yt_error *error)
{
	struct nearest_tape *tape = context;
	struct tape_event *event = add_event(tape, TAPE_READ);
	size_t index;

	event->field_kind = kind;
	event->expression = expression;
	event->physical = physical;
	if (tape->fail_at != 0U && tape->event_count == tape->fail_at) {
		if (error != NULL)
			error->status = YT_IO_ERROR;
		return false;
	}
	for (index = 0U; index < tape->record_count; ++index) {
		if (tape->records[index].physical == physical) {
			*record = tape->records[index].record;
			return true;
		}
	}
	if (error != NULL) {
		error->status = YT_IO_ERROR;
		error->system_error = 5;
		(void)snprintf(error->operation, sizeof(error->operation),
		    "fixture GET");
		error->path[0] = '\0';
	}
	return false;
}

static bool
observe_day(void *context, float *day, struct yt_error *error)
{
	struct nearest_tape *tape = context;

	(void)add_event(tape, TAPE_DAY);
	if (tape->fail_at != 0U && tape->event_count == tape->fail_at) {
		if (error != NULL)
			error->status = YT_IO_ERROR;
		return false;
	}
	if (tape->day_position == tape->day_count) {
		if (error != NULL)
			error->status = YT_IO_ERROR;
		return false;
	}
	*day = tape->days[tape->day_position++];
	return true;
}

static bool
observe_timer(void *context, float *seconds, struct yt_error *error)
{
	struct nearest_tape *tape = context;
	struct tape_event *event;

	event = add_event(tape, TAPE_TIMER);
	if (tape->state != NULL)
		event->market = tape->state->market;
	if (tape->fail_at != 0U && tape->event_count == tape->fail_at) {
		if (error != NULL)
			error->status = YT_IO_ERROR;
		return false;
	}
	if (tape->timer_position == tape->timer_count) {
		if (error != NULL)
			error->status = YT_IO_ERROR;
		return false;
	}
	*seconds = tape->timers[tape->timer_position++];
	return true;
}

static bool
present(void *context, enum yt_nearest_output_kind kind,
    enum yt_nearest_present_mode mode, const uint8_t *text, size_t length,
    struct yt_nearest_style *style, struct yt_error *error)
{
	struct nearest_tape *tape = context;
	struct tape_event *event = add_event(tape, TAPE_PRESENT);

	(void)error;
	event->output_kind = kind;
	event->mode = mode;
	event->style = *style;
	if (tape->state != NULL) {
		event->page_count = tape->state->page_count;
		memcpy(event->page_count_raw, tape->state->page_count_raw,
		    sizeof(event->page_count_raw));
	}
	event->length = length;
	if (length > sizeof(event->text)) {
		fprintf(stderr, "test_nearest: fixture presentation overflow\n");
		exit(EXIT_FAILURE);
	}
	if (length != 0U)
		memcpy(event->text, text, length);
	if (tape->fail_at != 0U && tape->event_count == tape->fail_at) {
		if (error != NULL)
			error->status = YT_IO_ERROR;
		return false;
	}
	if (tape->ansi) {
		style->bold = 0.0f;
		style->blink = 0.0f;
	}
	return true;
}

static bool
input(void *context, uint8_t *key, bool *available, struct yt_error *error)
{
	struct nearest_tape *tape = context;

	(void)add_event(tape, TAPE_INPUT);
	if (tape->fail_at != 0U && tape->event_count == tape->fail_at) {
		if (error != NULL)
			error->status = YT_IO_ERROR;
		return false;
	}
	if (tape->input_position == tape->input_count) {
		if (error != NULL)
			error->status = YT_IO_ERROR;
		return false;
	}
	*key = tape->inputs[tape->input_position];
	*available = tape->input_available[tape->input_position];
	++tape->input_position;
	return true;
}

static void
uppercase(void *context, uint8_t *text, size_t length)
{
	(void)context;
	qb_compat_upper_n(text, length);
}

static const struct yt_nearest_ops nearest_ops = {
	read_record,
	observe_day,
	observe_timer,
	present,
	input,
	uppercase,
};

static struct yt_nearest_state
base_state(void)
{
	struct yt_nearest_state state;

	memset(&state, 0, sizeof(state));
	state.selector = 4;
	state.direction = 'A';
	state.actor_number = 2.0f;
	state.sector_record_offset = 51.0f;
	state.port_record_offset = 2055.0f;
	state.base_price[0] = 20.0f;
	state.base_price[1] = 30.0f;
	state.base_price[2] = 40.0f;
	state.style.foreground = 7.0f;
	return state;
}

static const struct tape_event *
nth_event(const struct nearest_tape *tape, enum tape_event_kind kind,
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

static const struct tape_event *
find_output(const struct nearest_tape *tape,
    enum yt_nearest_output_kind kind, size_t ordinal)
{
	size_t index;
	size_t seen = 0U;

	for (index = 0U; index < tape->event_count; ++index) {
		if (tape->events[index].kind != TAPE_PRESENT
		    || tape->events[index].output_kind != kind)
			continue;
		if (seen++ == ordinal)
			return &tape->events[index];
	}
	return NULL;
}

static size_t
collect_row(const struct nearest_tape *tape, size_t row, uint8_t *result,
    size_t capacity)
{
	static const enum yt_nearest_output_kind kinds[6] = {
		YT_NEAREST_SECTOR,
		YT_NEAREST_ORE,
		YT_NEAREST_ORGANICS,
		YT_NEAREST_EQUIPMENT,
		YT_NEAREST_STOCK,
		YT_NEAREST_NAME,
	};
	size_t index;
	size_t length = 0U;

	for (index = 0U; index < ARRAY_SIZE(kinds); ++index) {
		const struct tape_event *event = find_output(tape, kinds[index], row);

		if (event == NULL || length + event->length > capacity)
			return SIZE_MAX;
		memcpy(result + length, event->text, event->length);
		length += event->length;
	}
	return length;
}

static int
test_opening_failure_preserves_inherited_field(void)
{
	static const enum yt_nearest_output_kind opening[5] = {
		YT_NEAREST_ENTRY_BLANK,
		YT_NEAREST_SCANNING,
		YT_NEAREST_SCAN_BLANK,
		YT_NEAREST_OWNER_INSTRUCTION,
		YT_NEAREST_OWNER_BLANK,
	};
	struct nearest_tape tape = {0};
	struct yt_nearest_state state = base_state();
	struct yt_error error = {0};
	size_t index;

	memset(state.field.bytes, 'I', sizeof(state.field.bytes));
	state.field_valid = true;
	state.field_kind = YT_NEAREST_FIELD_OWNER;
	state.field_record = 99U;
	CHECK(!yt_nearest_run(&state, &nearest_ops, &tape, &error));
	CHECK(tape.event_count == 6U);
	for (index = 0U; index < ARRAY_SIZE(opening); ++index) {
		CHECK(tape.events[index].kind == TAPE_PRESENT);
		CHECK(tape.events[index].output_kind == opening[index]);
	}
	CHECK(tape.events[5].kind == TAPE_READ);
	CHECK(tape.events[5].field_kind == YT_NEAREST_FIELD_PLAYER);
	CHECK(tape.events[5].physical == 2U);
	CHECK(state.outputs == 5U && state.reads == 0U);
	CHECK(state.field_valid && state.field_kind == YT_NEAREST_FIELD_OWNER);
	CHECK(state.field_record == 99U && state.field.bytes[0] == 'I');
	CHECK(state.current_record_expression == 2.0f);
	CHECK(!state.complete && state.result == YT_NEAREST_INCOMPLETE);
	return EXIT_SUCCESS;
}

static int
test_earth_call_order_and_residue(void)
{
	static const float no_warps[6] = {0};
	static const float stock[3] = {100.0f, 200.0f, 300.0f};
	static const uint8_t earth[] = "** Earth **";
	struct nearest_tape tape = {0};
	struct yt_nearest_state state = base_state();
	struct yt_error error = {0};
	const struct tape_event *event;
	uint8_t row[128];
	size_t length;

	add_record(&tape, 2U, player_record("PILOT", 1.0f, 0.0f));
	add_record(&tape, 52U, sector_record(no_warps, 1.0f));
	add_record(&tape, 2056U, port_record("NOT EARTH", 1.0f, 2.0f, stock));
	tape.days[tape.day_count++] = 0.0f;
	tape.timers[tape.timer_count++] = 0.0f;
	CHECK(yt_nearest_run(&state, &nearest_ops, &tape, &error));
	CHECK(state.complete && state.result == YT_NEAREST_COMPLETE);
	CHECK(state.rows == 1U && state.reads == 3U);
	CHECK(state.day_observations == 1U && state.timer_observations == 1U);
	CHECK(state.field_kind == YT_NEAREST_FIELD_PORT && state.field_record == 2056U);
	CHECK(state.page_count == 6.0f);
	event = nth_event(&tape, TAPE_READ, 0U);
	CHECK(event != NULL && event->physical == 2U);
	event = nth_event(&tape, TAPE_READ, 1U);
	CHECK(event != NULL && event->physical == 52U);
	CHECK(nth_event(&tape, TAPE_DAY, 0U) != NULL);
	event = nth_event(&tape, TAPE_READ, 2U);
	CHECK(event != NULL && event->physical == 2056U);
	CHECK(nth_event(&tape, TAPE_TIMER, 0U) != NULL);
	CHECK(find_output(&tape, YT_NEAREST_ORE, 0U)->length == 0U);
	CHECK(find_output(&tape, YT_NEAREST_ORGANICS, 0U)->length == 0U);
	CHECK(find_output(&tape, YT_NEAREST_EQUIPMENT, 0U)->length == 0U);
	CHECK(find_output(&tape, YT_NEAREST_STOCK, 0U)->length == 0U);
	event = find_output(&tape, YT_NEAREST_NAME, 0U);
	CHECK(event != NULL && event->length == sizeof(earth) - 1U);
	CHECK(memcmp(event->text, earth, sizeof(earth) - 1U) == 0);
	CHECK(event->style.foreground == 3.0f && event->style.blink == 1.0f);
	length = collect_row(&tape, 0U, row, sizeof(row));
	CHECK(length == 13U + sizeof(earth) - 1U);
	CHECK(memcmp(row, "Sector: 1    ** Earth **", length) == 0);
	CHECK(state.style.bold == 1.0f && state.style.blink == 1.0f);
	return EXIT_SUCCESS;
}

static int
test_descending_layer_and_exact_rows(void)
{
	static const float first_warps[6] = {2.0f, 3.0f, 0, 0, 0, 0};
	static const float no_warps[6] = {0};
	static const float stock[3] = {100.0f, 200.0f, 300.0f};
	static const uint8_t first_row[] =
	    "Sector: 3     Ore @S 18   Org @B 36   Equ @B 52   0K  > Pilot";
	static const uint8_t second_row[] =
	    "Sector: 2     Ore @B 22   Org @S 24   Equ @B 52   0K  ALPHA";
	static const uint32_t reads[] = {2U, 52U, 54U, 2058U, 2U, 53U, 2057U};
	struct nearest_tape tape = {0};
	struct yt_nearest_state state = base_state();
	struct yt_error error = {0};
	uint8_t row[128];
	size_t index;
	size_t length;

	add_record(&tape, 2U, player_record("PILOT", 1.0f, 0.0f));
	add_record(&tape, 52U, sector_record(first_warps, 0.0f));
	add_record(&tape, 53U, sector_record(no_warps, 2.0f));
	add_record(&tape, 54U, sector_record(no_warps, 3.0f));
	add_record(&tape, 2057U, port_record("ALPHA", 2.0f, 0.0f, stock));
	add_record(&tape, 2058U, port_record("IGNORED", 3.0f, 2.0f, stock));
	tape.days[tape.day_count++] = 0.0f;
	tape.days[tape.day_count++] = 0.0f;
	tape.timers[tape.timer_count++] = 0.0f;
	tape.timers[tape.timer_count++] = 0.0f;
	CHECK(yt_nearest_run(&state, &nearest_ops, &tape, &error));
	CHECK(state.rows == 2U && state.distance == 2);
	for (index = 0U; index < ARRAY_SIZE(reads); ++index) {
		const struct tape_event *event = nth_event(&tape, TAPE_READ, index);
		CHECK(event != NULL && event->physical == reads[index]);
	}
	CHECK(nth_event(&tape, TAPE_READ, ARRAY_SIZE(reads)) == NULL);
	CHECK(find_output(&tape, YT_NEAREST_DISTANCE, 0U) != NULL);
	CHECK(find_output(&tape, YT_NEAREST_DISTANCE, 1U) == NULL);
	length = collect_row(&tape, 0U, row, sizeof(row));
	CHECK(length == sizeof(first_row) - 1U);
	CHECK(memcmp(row, first_row, length) == 0);
	length = collect_row(&tape, 1U, row, sizeof(row));
	CHECK(length == sizeof(second_row) - 1U);
	CHECK(memcmp(row, second_row, length) == 0);
	CHECK(state.field_kind == YT_NEAREST_FIELD_PORT && state.field_record == 2057U);
	return EXIT_SUCCESS;
}

static int
test_date_filter_timer_order_and_roster(void)
{
	static const float first_warps[6] = {2.0f, 0, 0, 0, 0, 0};
	static const float no_warps[6] = {0};
	static const float stock[3] = {100.0f, 200.0f, 300.0f};
	struct nearest_tape tape = {0};
	struct yt_nearest_state state = base_state();
	struct yt_error error = {0};
	size_t day_one;
	size_t port_one;
	size_t day_two;
	size_t port_two;
	size_t timer;
	size_t index;

	state.selector = 1;
	state.direction = 'S';
	state.cached_roster[0] = 7.0f;
	tape.state = &state;
	add_record(&tape, 2U, player_record("PILOT", 1.0f, 5.0f));
	add_record(&tape, 52U, sector_record(first_warps, 1.0f));
	add_record(&tape, 53U, sector_record(no_warps, 2.0f));
	add_record(&tape, 2056U, port_record("REJECT", 2.0f, 7.0f, stock));
	add_record(&tape, 2057U, port_record("ACCEPT", 1.0f, 0.0f, stock));
	tape.days[tape.day_count++] = 10.0f;
	tape.days[tape.day_count++] = 11.0f;
	tape.timers[tape.timer_count++] = 120.0f;
	CHECK(yt_nearest_run(&state, &nearest_ops, &tape, &error));
	CHECK(state.rows == 1U && state.roster_comparisons == 8U);
	CHECK(state.day_observations == 2U && state.timer_observations == 1U);
	day_one = day_two = port_one = port_two = timer = SIZE_MAX;
	for (index = 0U; index < tape.event_count; ++index) {
		const struct tape_event *event = &tape.events[index];

		if (event->kind == TAPE_DAY && day_one == SIZE_MAX)
			day_one = index;
		else if (event->kind == TAPE_DAY)
			day_two = index;
		else if (event->kind == TAPE_READ && event->physical == 2056U)
			port_one = index;
		else if (event->kind == TAPE_READ && event->physical == 2057U)
			port_two = index;
		else if (event->kind == TAPE_TIMER)
			timer = index;
	}
	CHECK(day_one < port_one && port_one < day_two);
	CHECK(day_two < port_two && port_two < timer);
	CHECK(tape.events[timer].market.stock[0] == 100.0f);
	CHECK(tape.events[timer].market.production[1] == 20.0f);
	CHECK(tape.events[timer].market.factor[2] == 30.0f);
	CHECK(tape.events[timer].market.price[0] == 0.0f);
	CHECK(state.current_day == 11.0f && state.timer_seconds == 120.0f);
	return EXIT_SUCCESS;
}

static void
build_chain(struct nearest_tape *tape, size_t count)
{
	static const float stock[3] = {100.0f, 200.0f, 300.0f};
	size_t number;

	add_record(tape, 2U, player_record("PILOT", 1.0f, 0.0f));
	for (number = 1U; number <= count; ++number) {
		float warps[6] = {0};
		char name[16];

		if (number < count)
			warps[0] = (float)(number + 1U);
		(void)snprintf(name, sizeof(name), "P%zu", number);
		add_record(tape, 51U + (uint32_t)number,
		    sector_record(warps, (float)number));
		add_record(tape, 2055U + (uint32_t)number,
		    port_record(name, 1.0f, 0.0f, stock));
		tape->days[tape->day_count++] = 0.0f;
		tape->timers[tape->timer_count++] = 0.0f;
	}
}

static int
test_pager_invalid_cr_resume_and_raw_counter(void)
{
	static const uint8_t reset_raw[4] = {0x00, 0x00, 0x30, 0x00};
	static const uint8_t two_raw[4] = {0x00, 0x00, 0x00, 0x82};
	struct nearest_tape tape = {0};
	struct yt_nearest_state state = base_state();
	struct yt_error error = {0};
	const struct tape_event *event;

	tape.state = &state;
	build_chain(&tape, 11U);
	tape.inputs[0] = 'x';
	tape.input_available[0] = true;
	tape.inputs[1] = '\r';
	tape.input_available[1] = true;
	tape.input_count = 2U;
	CHECK(yt_nearest_run(&state, &nearest_ops, &tape, &error));
	CHECK(state.rows == 11U && state.complete && !state.stopped);
	CHECK(tape.input_position == 2U);
	CHECK(find_output(&tape, YT_NEAREST_PAGER_PROMPT, 0U) != NULL);
	CHECK(find_output(&tape, YT_NEAREST_PAGER_PROMPT, 1U) == NULL);
	event = find_output(&tape, YT_NEAREST_PAGER_ECHO, 0U);
	CHECK(event != NULL && event->length == 1U && event->text[0] == 'Y');
	CHECK(find_output(&tape, YT_NEAREST_PAGER_ECHO, 1U) == NULL);
	CHECK(state.page_count == 2.0f);
	CHECK(memcmp(state.page_count_raw, two_raw, sizeof(two_raw)) == 0);
	/* The prompt itself is observed only after the dirty reset is installed. */
	event = find_output(&tape, YT_NEAREST_PAGER_PROMPT, 0U);
	CHECK(event->style.foreground == 3.0f && event->style.bold == 1.0f);
	CHECK(event->page_count == 0.0f);
	CHECK(memcmp(event->page_count_raw, reset_raw, sizeof(reset_raw)) == 0);
	return EXIT_SUCCESS;
}

static int
test_pager_n_stops_before_next_sector(void)
{
	struct nearest_tape tape = {0};
	struct yt_nearest_state state = base_state();
	struct yt_error error = {0};
	const struct tape_event *event;
	size_t index;

	build_chain(&tape, 11U);
	tape.inputs[0] = 'n';
	tape.input_available[0] = true;
	tape.input_count = 1U;
	CHECK(yt_nearest_run(&state, &nearest_ops, &tape, &error));
	CHECK(state.rows == 10U && state.stopped && state.complete);
	CHECK(state.result == YT_NEAREST_PAGE_STOP);
	event = find_output(&tape, YT_NEAREST_PAGER_ECHO, 0U);
	CHECK(event != NULL && event->text[0] == 'N');
	CHECK(find_output(&tape, YT_NEAREST_FINAL_BLANK, 0U) != NULL);
	for (index = 0U; index < tape.event_count; ++index) {
		if (tape.events[index].kind == TAPE_READ)
			CHECK(tape.events[index].physical != 62U);
	}
	return EXIT_SUCCESS;
}

static int
test_owner_failure_retains_partial_row_and_port_field(void)
{
	static const float no_warps[6] = {0};
	static const float stock[3] = {100.0f, 200.0f, 300.0f};
	struct nearest_tape tape = {0};
	struct yt_nearest_state state = base_state();
	struct yt_error error = {0};
	const struct tape_event *event;

	add_record(&tape, 2U, player_record("PILOT", 2.0f, 0.0f));
	add_record(&tape, 53U, sector_record(no_warps, 1.0f));
	add_record(&tape, 2056U, port_record("PORT", 1.0f, 9.0f, stock));
	tape.days[tape.day_count++] = 0.0f;
	tape.timers[tape.timer_count++] = 0.0f;
	CHECK(!yt_nearest_run(&state, &nearest_ops, &tape, &error));
	CHECK(state.rows == 0U && !state.complete);
	CHECK(state.field_kind == YT_NEAREST_FIELD_PORT && state.field_record == 2056U);
	event = nth_event(&tape, TAPE_READ, 3U);
	CHECK(event != NULL && event->field_kind == YT_NEAREST_FIELD_OWNER);
	CHECK(event->physical == 9U);
	CHECK(find_output(&tape, YT_NEAREST_SECTOR, 0U) != NULL);
	CHECK(find_output(&tape, YT_NEAREST_STOCK, 0U) != NULL);
	CHECK(find_output(&tape, YT_NEAREST_NAME, 0U) == NULL);
	CHECK(find_output(&tape, YT_NEAREST_FINAL_BLANK, 0U) == NULL);
	CHECK(state.style.foreground == 3.0f);
	return EXIT_SUCCESS;
}

static int
test_fractional_start_and_owner_gate(void)
{
	static const float no_warps[6] = {0};
	static const float stock[3] = {100.0f, 200.0f, 300.0f};
	struct nearest_tape tape = {0};
	struct yt_nearest_state state = base_state();
	struct yt_error error = {0};
	const struct tape_event *event;

	add_record(&tape, 2U, player_record("PILOT", 1.4f, 0.0f));
	add_record(&tape, 52U, sector_record(no_warps, 1.0f));
	add_record(&tape, 2056U, port_record("NOT EARTH", 1.0f, 0.4f, stock));
	tape.days[tape.day_count++] = 0.0f;
	tape.timers[tape.timer_count++] = 0.0f;
	CHECK(yt_nearest_run(&state, &nearest_ops, &tape, &error));
	CHECK(state.rows == 1U && state.reads == 3U);
	event = nth_event(&tape, TAPE_READ, 1U);
	CHECK(event != NULL && event->physical == 52U);
	CHECK(event->expression == state.start_sector_raw + 51.0f);
	event = find_output(&tape, YT_NEAREST_SECTOR, 0U);
	CHECK(event != NULL && event->length == 13U);
	CHECK(memcmp(event->text, "Sector: 1.4  ", 13U) == 0);
	event = find_output(&tape, YT_NEAREST_NAME, 0U);
	CHECK(event != NULL && event->length == strlen("NOT EARTH"));
	CHECK(memcmp(event->text, "NOT EARTH", event->length) == 0);
	CHECK(nth_event(&tape, TAPE_READ, 3U) == NULL);
	return EXIT_SUCCESS;
}

static int
test_record_conversion_boundaries(void)
{
	static const float no_warps[6] = {0};
	static const float stock[3] = {100.0f, 200.0f, 300.0f};
	static const float owners[2] = {0.5f, -0.6f};
	static const uint32_t owner_records[2] = {0U, UINT32_C(0x00ffffff)};
	struct yt_error error = {0};
	struct nearest_tape zero = {0};
	struct yt_nearest_state zero_state = base_state();
	struct nearest_tape fractional = {0};
	struct yt_nearest_state fractional_state = base_state();
	struct nearest_tape overflow = {0};
	struct yt_nearest_state overflow_state = base_state();
	const struct tape_event *event;
	uint8_t raw[4];
	float expected_expression;
	size_t index;

	add_record(&zero, 2U, player_record("PILOT", 0.0f, 0.0f));
	CHECK(yt_nearest_run(&zero_state, &nearest_ops, &zero, &error));
	CHECK(zero_state.rows == 0U && zero_state.reads == 1U);
	CHECK(zero_state.field_kind == YT_NEAREST_FIELD_PLAYER);
	CHECK(nth_event(&zero, TAPE_READ, 1U) == NULL);
	CHECK(find_output(&zero, YT_NEAREST_FINAL_BLANK, 0U) != NULL);

	add_record(&fractional, 2U, player_record("PILOT", 2.0f, 0.0f));
	add_record(&fractional, 53U, sector_record(no_warps, 1.6f));
	add_record(&fractional, 2056U,
	    port_record("TRUNCATED", 1.0f, 0.0f, stock));
	fractional.days[fractional.day_count++] = 0.0f;
	fractional.timers[fractional.timer_count++] = 0.0f;
	CHECK(yt_nearest_run(&fractional_state, &nearest_ops, &fractional,
	    &error));
	event = nth_event(&fractional, TAPE_READ, 2U);
	CHECK(event != NULL && event->physical == 2056U);
	CHECK(qb_mbf32_encode(2056.6f, raw) == QB_MBF_OK);
	expected_expression = qb_mbf32_decode(raw);
	CHECK(event->expression == expected_expression);
	CHECK(fractional_state.field_record == 2056U);

	for (index = 0U; index < ARRAY_SIZE(owners); ++index) {
		struct nearest_tape tape = {0};
		struct yt_nearest_state state = base_state();

		add_record(&tape, 2U, player_record("PILOT", 2.0f, 0.0f));
		add_record(&tape, 53U, sector_record(no_warps, 1.0f));
		add_record(&tape, 2056U,
		    port_record("PORT", 1.0f, owners[index], stock));
		tape.days[tape.day_count++] = 0.0f;
		tape.timers[tape.timer_count++] = 0.0f;
		memset(&error, 0, sizeof(error));
		CHECK(!yt_nearest_run(&state, &nearest_ops, &tape, &error));
		event = nth_event(&tape, TAPE_READ, 3U);
		CHECK(event != NULL && event->field_kind == YT_NEAREST_FIELD_OWNER);
		CHECK(event->physical == owner_records[index]);
		CHECK(event->expression == yt_record_get_number(
		    &tape.records[2].record, YT_F97));
		CHECK(state.field_kind == YT_NEAREST_FIELD_PORT);
		CHECK(state.field_record == 2056U && state.rows == 0U);
		CHECK(find_output(&tape, YT_NEAREST_STOCK, 0U) != NULL);
		CHECK(find_output(&tape, YT_NEAREST_NAME, 0U) == NULL);
	}

	add_record(&overflow, 2U, player_record("PILOT", 40000.0f, 0.0f));
	memset(&error, 0, sizeof(error));
	CHECK(!yt_nearest_run(&overflow_state, &nearest_ops, &overflow,
	    &error));
	CHECK(overflow_state.reads == 1U && overflow_state.outputs == 5U);
	CHECK(overflow_state.field_kind == YT_NEAREST_FIELD_PLAYER);
	CHECK(nth_event(&overflow, TAPE_READ, 1U) == NULL);
	CHECK(strcmp(error.operation, "nearest start-sector CINT") == 0);
	return EXIT_SUCCESS;
}

static void build_earth(struct nearest_tape *tape);

static int
test_market_projection_and_stock_rounding(void)
{
	static const float stock[3] = {5000.0f, 5000.0f, 5000.0f};
	static const float base[3] = {20.0f, 30.0f, 40.0f};
	struct yt_record raw = port_record("PORT", 1.0f, 0.0f, stock);
	struct yt_port port;
	struct yt_nearest_market market;
	struct yt_error error = {0};

	yt_port_decode(&port, &raw);
	CHECK(yt_nearest_market_project(&market, &port, base, 0.0f, 0.0f,
	    &error));
	CHECK(market.stock[0] == 5000.0f && market.stock[1] == 5000.0f
	    && market.stock[2] == 5000.0f);
	CHECK(market.production[0] == 500.0f
	    && market.production[1] == 500.0f
	    && market.production[2] == 500.0f);
	CHECK(market.price[0] == 22.0f && market.price[1] == 36.0f
	    && market.price[2] == 28.0f);
	return EXIT_SUCCESS;
}

static void
build_filter_world(struct nearest_tape *tape)
{
	static const float warps[6] = {2.0f, 3.0f, 4.0f, 5.0f, 0, 0};
	static const float no_warps[6] = {0};
	static const float stock[3] = {100.0f, 200.0f, 300.0f};
	size_t index;

	add_record(tape, 2U, player_record("PILOT", 1.0f, 5.0f));
	add_record(tape, 3U, player_record("TEAMMATE", 9.0f, 5.0f));
	add_record(tape, 9U, player_record("ENEMY", 9.0f, 8.0f));
	add_record(tape, 52U, sector_record(warps, 1.0f));
	for (index = 2U; index <= 5U; ++index)
		add_record(tape, 51U + (uint32_t)index,
		    sector_record(no_warps, (float)index));
	add_record(tape, 2056U, port_record("ZERO", 0.0f, 3.0f, stock));
	add_record(tape, 2057U, port_record("SELF", 1.0f, 2.0f, stock));
	add_record(tape, 2058U, port_record("TEAM", 1.0f, 3.0f, stock));
	add_record(tape, 2059U, port_record("ENEMY", 1.0f, 9.0f, stock));
	add_record(tape, 2060U, port_record("FREE", 1.0f, 0.0f, stock));
	for (index = 0U; index < 5U; ++index)
		tape->days[tape->day_count++] = 0.0f;
	tape->timers[tape->timer_count++] = 0.0f;
}

static int
test_team_owned_enemy_and_unowned_filters(void)
{
	static const int selectors[4] = {5, 6, 7, 8};
	static const char *const sectors[4] = {
		"Sector: 3    ",
		"Sector: 2    ",
		"Sector: 4    ",
		"Sector: 5    ",
	};
	size_t route;

	for (route = 0U; route < ARRAY_SIZE(selectors); ++route) {
		struct nearest_tape tape = {0};
		struct yt_nearest_state state = base_state();
		struct yt_error error = {0};
		const struct tape_event *event;

		state.selector = selectors[route];
		state.cached_roster[0] = 2.0f;
		state.cached_roster[1] = 3.0f;
		build_filter_world(&tape);
		CHECK(yt_nearest_run(&state, &nearest_ops, &tape, &error));
		CHECK(state.rows == 1U);
		CHECK(state.roster_comparisons == 20U);
		CHECK(state.day_observations == 5U);
		CHECK(state.timer_observations == 1U);
		event = find_output(&tape, YT_NEAREST_SECTOR, 0U);
		CHECK(event != NULL && event->length == 13U);
		CHECK(memcmp(event->text, sectors[route], 13U) == 0);
		CHECK(find_output(&tape, YT_NEAREST_SECTOR, 1U) == NULL);
	}
	return EXIT_SUCCESS;
}

static int
test_pager_input_failure_and_continuous_mode(void)
{
	struct nearest_tape failed = {0};
	struct yt_nearest_state failed_state = base_state();
	struct yt_error error = {0};
	struct nearest_tape continuous = {0};
	struct yt_nearest_state continuous_state = base_state();
	static const uint8_t zero[4] = {0};

	build_chain(&failed, 10U);
	CHECK(!yt_nearest_run(&failed_state, &nearest_ops, &failed, &error));
	CHECK(failed_state.rows == 10U && !failed_state.complete);
	CHECK(find_output(&failed, YT_NEAREST_PAGER_PROMPT, 0U) != NULL);
	CHECK(find_output(&failed, YT_NEAREST_PAGER_ECHO, 0U) == NULL);
	CHECK(find_output(&failed, YT_NEAREST_FINAL_BLANK, 0U) == NULL);
	CHECK(failed.events[failed.event_count - 1U].kind == TAPE_INPUT);

	build_chain(&continuous, 20U);
	continuous.inputs[0] = '+';
	continuous.input_available[0] = true;
	continuous.input_count = 1U;
	CHECK(yt_nearest_run(&continuous_state, &nearest_ops, &continuous,
	    &error));
	CHECK(continuous_state.rows == 20U && continuous_state.continuous);
	CHECK(find_output(&continuous, YT_NEAREST_PAGER_PROMPT, 0U) != NULL);
	CHECK(find_output(&continuous, YT_NEAREST_PAGER_PROMPT, 1U) == NULL);
	CHECK(continuous_state.page_count == 0.0f);
	CHECK(memcmp(continuous_state.page_count_raw, zero, sizeof(zero)) == 0);
	return EXIT_SUCCESS;
}

static int
test_ansi_consumes_transient_earth_style(void)
{
	struct nearest_tape tape = {0};
	struct yt_nearest_state state = base_state();
	struct yt_error error = {0};

	build_earth(&tape);
	tape.ansi = true;
	CHECK(yt_nearest_run(&state, &nearest_ops, &tape, &error));
	CHECK(state.style.bold == 0.0f && state.style.blink == 0.0f);
	return EXIT_SUCCESS;
}

static void
build_earth(struct nearest_tape *tape)
{
	static const float no_warps[6] = {0};
	static const float stock[3] = {100.0f, 200.0f, 300.0f};

	add_record(tape, 2U, player_record("PILOT", 1.0f, 0.0f));
	add_record(tape, 52U, sector_record(no_warps, 1.0f));
	add_record(tape, 2056U, port_record("EARTH", 1.0f, 0.0f, stock));
	tape->days[tape->day_count++] = 0.0f;
	tape->timers[tape->timer_count++] = 0.0f;
}

static bool
same_event(const struct tape_event *left, const struct tape_event *right)
{
	return left->kind == right->kind
	    && left->field_kind == right->field_kind
	    && left->output_kind == right->output_kind
	    && left->mode == right->mode
	    && left->physical == right->physical
	    && left->expression == right->expression
	    && left->length == right->length
	    && memcmp(left->text, right->text, left->length) == 0;
}

static int
test_every_earth_provider_failure_retains_exact_prefix(void)
{
	struct nearest_tape complete = {0};
	struct yt_nearest_state complete_state = base_state();
	struct yt_error error = {0};
	size_t cut;

	build_earth(&complete);
	CHECK(yt_nearest_run(&complete_state, &nearest_ops, &complete, &error));
	for (cut = 1U; cut <= complete.event_count; ++cut) {
		struct nearest_tape tape = {0};
		struct yt_nearest_state state = base_state();
		size_t index;
		size_t reads = 0U;
		size_t outputs = 0U;
		size_t days = 0U;
		size_t timers = 0U;
		enum yt_nearest_field_kind field_kind = YT_NEAREST_FIELD_OWNER;
		uint32_t field_record = 99U;
		struct yt_record field;

		memset(field.bytes, 'I', sizeof(field.bytes));
		state.field = field;
		state.field_valid = true;
		state.field_kind = field_kind;
		state.field_record = field_record;
		build_earth(&tape);
		tape.fail_at = cut;
		memset(&error, 0, sizeof(error));
		CHECK(!yt_nearest_run(&state, &nearest_ops, &tape, &error));
		CHECK(tape.event_count == cut);
		for (index = 0U; index < cut; ++index)
			CHECK(same_event(&tape.events[index], &complete.events[index]));
		for (index = 0U; index + 1U < cut; ++index) {
			const struct tape_event *event = &complete.events[index];

			if (event->kind == TAPE_PRESENT)
				++outputs;
			else if (event->kind == TAPE_DAY)
				++days;
			else if (event->kind == TAPE_TIMER)
				++timers;
			else if (event->kind == TAPE_READ) {
				size_t record_index;

				++reads;
				field_kind = event->field_kind;
				field_record = event->physical;
				for (record_index = 0U;
				    record_index < tape.record_count; ++record_index) {
					if (tape.records[record_index].physical
					    == field_record) {
						field = tape.records[record_index].record;
						break;
					}
				}
				CHECK(record_index < tape.record_count);
			}
		}
		CHECK(state.outputs == outputs && state.reads == reads);
		CHECK(state.day_observations == days);
		CHECK(state.timer_observations == timers);
		CHECK(state.field_valid && state.field_kind == field_kind);
		CHECK(state.field_record == field_record);
		CHECK(memcmp(state.field.bytes, field.bytes,
		    sizeof(field.bytes)) == 0);
		CHECK(!state.complete && state.result == YT_NEAREST_INCOMPLETE);
	}
	return EXIT_SUCCESS;
}

int
main(void)
{
	int result = EXIT_SUCCESS;

	result |= test_opening_failure_preserves_inherited_field();
	result |= test_earth_call_order_and_residue();
	result |= test_descending_layer_and_exact_rows();
	result |= test_date_filter_timer_order_and_roster();
	result |= test_pager_invalid_cr_resume_and_raw_counter();
	result |= test_pager_n_stops_before_next_sector();
	result |= test_owner_failure_retains_partial_row_and_port_field();
	result |= test_fractional_start_and_owner_gate();
	result |= test_record_conversion_boundaries();
	result |= test_market_projection_and_stock_rounding();
	result |= test_team_owned_enemy_and_unowned_filters();
	result |= test_pager_input_failure_and_continuous_mode();
	result |= test_ansi_consumes_transient_earth_style();
	result |= test_every_earth_provider_failure_retains_exact_prefix();
	if (result == EXIT_SUCCESS)
		puts("test_nearest: ok");
	return result;
}
