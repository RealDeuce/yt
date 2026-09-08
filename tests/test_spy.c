#include "yt_game.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARRAY_SIZE(values) (sizeof(values) / sizeof((values)[0]))

enum spy_event_kind {
	SPY_READ = 1,
	SPY_PRESENT,
};

struct spy_event {
	enum spy_event_kind kind;
	enum yt_computer_spy_output_kind output_kind;
	size_t index;
	size_t length;
	uint8_t text[160];
};

struct spy_tape {
	int16_t targets[3];
	struct spy_event events[8];
	size_t event_count;
	size_t fail_at;
};

static int
fail_at(const char *function, int line, const char *condition)
{
	fprintf(stderr, "test_spy: %s:%d: %s\n", function, line, condition);
	return EXIT_FAILURE;
}

#define CHECK(condition) do { \
	if (!(condition)) \
		return fail_at(__func__, __LINE__, #condition); \
} while (0)

static struct spy_event *
add_event(struct spy_tape *tape, enum spy_event_kind kind)
{
	struct spy_event *event;

	if (tape->event_count == ARRAY_SIZE(tape->events)) {
		fprintf(stderr, "test_spy: event tape overflow\n");
		exit(EXIT_FAILURE);
	}
	event = &tape->events[tape->event_count++];
	memset(event, 0, sizeof(*event));
	event->kind = kind;
	return event;
}

static bool
read_target(void *context, size_t index, int16_t *target,
    struct yt_error *error)
{
	struct spy_tape *tape = context;
	struct spy_event *event = add_event(tape, SPY_READ);

	event->index = index;
	if (tape->fail_at != 0U && tape->event_count == tape->fail_at) {
		if (error != NULL)
			error->status = YT_IO_ERROR;
		return false;
	}
	if (index >= ARRAY_SIZE(tape->targets)) {
		if (error != NULL)
			error->status = YT_RANGE;
		return false;
	}
	*target = tape->targets[index];
	return true;
}

static bool
present(void *context, const uint8_t *text, size_t length,
    enum yt_computer_spy_output_kind kind, struct yt_error *error)
{
	struct spy_tape *tape = context;
	struct spy_event *event = add_event(tape, SPY_PRESENT);

	event->output_kind = kind;
	event->length = length;
	if (length > sizeof(event->text)) {
		if (error != NULL)
			error->status = YT_RANGE;
		return false;
	}
	if (length != 0U)
		memcpy(event->text, text, length);
	if (tape->fail_at != 0U && tape->event_count == tape->fail_at) {
		if (error != NULL)
			error->status = YT_IO_ERROR;
		return false;
	}
	return true;
}

static const struct yt_computer_spy_ops ops = {
	read_target,
	present,
};

static bool
event_is_text(const struct spy_event *event,
    enum yt_computer_spy_output_kind kind, const char *text)
{
	size_t length = strlen(text);

	return event->kind == SPY_PRESENT && event->output_kind == kind
	    && event->length == length
	    && memcmp(event->text, text, length) == 0;
}

static bool
same_event(const struct spy_event *left, const struct spy_event *right)
{
	return left->kind == right->kind
	    && left->output_kind == right->output_kind
	    && left->index == right->index && left->length == right->length
	    && memcmp(left->text, right->text, left->length) == 0;
}

static int
test_zero_count_notice(void)
{
	struct yt_computer_spy_state state = {.count = 0.0f};
	struct spy_tape tape = {{733, -1, 2004}, {{0}}, 0U, 0U};
	struct yt_error error = {0};

	CHECK(yt_computer_spy_run(&state, &ops, &tape, &error));
	CHECK(state.complete && state.counter == 0.0f
	    && state.target_reads == 0U && state.outputs == 1U
	    && state.rows == 0U && !state.target_valid);
	CHECK(tape.event_count == 1U
	    && event_is_text(&tape.events[0], YT_COMPUTER_SPY_NONE,
	    "You do not have any spies!"));
	return EXIT_SUCCESS;
}

static int
test_reachable_counts_and_exact_rows(void)
{
	static const char *const rows[] = {
		"Spy # 1 will hunt in sector 733.",
		"Spy # 2 will hunt in sector-1.",
		"Spy # 3 will hunt in sector 2004.",
	};
	size_t count;

	for (count = 1U; count <= 3U; ++count) {
		struct yt_computer_spy_state state = {.count = (float)count};
		struct spy_tape tape = {{733, -1, 2004}, {{0}}, 0U, 0U};
		struct yt_error error = {0};
		size_t index;

		CHECK(yt_computer_spy_run(&state, &ops, &tape, &error));
		CHECK(state.complete && state.counter == (float)(count + 1U)
		    && state.target_reads == count && state.outputs == count + 1U
		    && state.rows == count && state.target_valid
		    && state.current_target == tape.targets[count - 1U]);
		CHECK(tape.event_count == 1U + count * 2U
		    && tape.events[0].kind == SPY_PRESENT
		    && tape.events[0].output_kind
		    == YT_COMPUTER_SPY_LEADING_BLANK
		    && tape.events[0].length == 0U);
		for (index = 0U; index < count; ++index) {
			CHECK(tape.events[1U + index * 2U].kind == SPY_READ
			    && tape.events[1U + index * 2U].index == index);
			CHECK(event_is_text(&tape.events[2U + index * 2U],
			    YT_COMPUTER_SPY_ROW, rows[index]));
		}
	}
	return EXIT_SUCCESS;
}

static int
test_every_provider_failure_prefix(void)
{
	struct yt_computer_spy_state complete_state = {.count = 3.0f};
	struct spy_tape complete = {{733, -1, 2004}, {{0}}, 0U, 0U};
	struct yt_error error = {0};
	size_t cut;

	CHECK(yt_computer_spy_run(&complete_state, &ops, &complete, &error));
	CHECK(complete.event_count == 7U);
	for (cut = 1U; cut <= complete.event_count; ++cut) {
		struct yt_computer_spy_state state = {.count = 3.0f};
		struct spy_tape tape = {
			.targets = {733, -1, 2004},
			.fail_at = cut,
		};
		size_t index;

		memset(&error, 0, sizeof(error));
		CHECK(!yt_computer_spy_run(&state, &ops, &tape, &error));
		CHECK(error.status == YT_IO_ERROR && tape.event_count == cut
		    && !state.complete);
		for (index = 0U; index < cut; ++index)
			CHECK(same_event(&tape.events[index],
			    &complete.events[index]));
	}
	return EXIT_SUCCESS;
}

static int
test_corrupt_count_boundary(void)
{
	static const float counts[] = {-1.0f, 0.5f, 4.0f};
	size_t index;

	for (index = 0U; index < ARRAY_SIZE(counts); ++index) {
		struct yt_computer_spy_state state = {.count = counts[index]};
		struct spy_tape tape = {{733, -1, 2004}, {{0}}, 0U, 0U};
		struct yt_error error = {0};

		CHECK(!yt_computer_spy_run(&state, &ops, &tape, &error));
		CHECK(error.status == YT_RANGE && tape.event_count == 0U
		    && !state.complete && state.outputs == 0U
		    && state.target_reads == 0U);
	}
	return EXIT_SUCCESS;
}

int
main(void)
{
	int result = EXIT_SUCCESS;

	result |= test_zero_count_notice();
	result |= test_reachable_counts_and_exact_rows();
	result |= test_every_provider_failure_prefix();
	result |= test_corrupt_count_boundary();
	if (result == EXIT_SUCCESS)
		puts("test_spy: ok");
	return result;
}
