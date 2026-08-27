#include "yt_input_model.h"

#include <stdio.h>
#include <string.h>

static unsigned failures;

#define CHECK(expr) do { \
	if (!(expr)) { \
		fprintf(stderr, "%s:%d: check failed: %s\n", \
		    __FILE__, __LINE__, #expr); \
		++failures; \
	} \
} while (0)

static struct yt_input_value
one(uint8_t byte)
{
	struct yt_input_value value = {{byte, 0}, 1, 0};

	return value;
}

static void
test_merged_fifo(void)
{
	struct yt_input_splitter splitter;
	struct yt_input_value remote_a = one('A');
	struct yt_input_value local_b = one('B');
	struct yt_input_value remote_c = one('C');
	struct yt_input_value selected;

	yt_input_splitter_init(&splitter);
	CHECK(yt_input_splitter_push(&splitter, true, &remote_a));
	CHECK(yt_input_splitter_push(&splitter, false, &local_b));
	CHECK(yt_input_splitter_push(&splitter, true, &remote_c));
	selected = yt_input_splitter_select_merged(&splitter);
	CHECK(selected.length == 1 && selected.bytes[0] == 'A');
	selected = yt_input_splitter_select_merged(&splitter);
	CHECK(selected.length == 1 && selected.bytes[0] == 'B');
	selected = yt_input_splitter_select_merged(&splitter);
	CHECK(selected.length == 1 && selected.bytes[0] == 'C');
	selected = yt_input_splitter_select_merged(&splitter);
	CHECK(selected.length == 0);
}

static void
test_arbitration(void)
{
	struct yt_input_splitter splitter;
	struct yt_input_value local = one('L');
	struct yt_input_value remote = one('R');
	struct yt_input_value selected;

	yt_input_splitter_init(&splitter);
	CHECK(yt_input_splitter_push(&splitter, false, &local));
	CHECK(yt_input_splitter_push(&splitter, true, &remote));
	selected = yt_input_splitter_select(&splitter, 0.0f,
	    YT_INPUT_PHASE_B05D);
	CHECK(selected.length == 1 && selected.bytes[0] == 'R');
	CHECK(splitter.local.length == 0 && splitter.remote.length == 0);

	CHECK(yt_input_splitter_push(&splitter, false, &local));
	CHECK(yt_input_splitter_push(&splitter, true, &remote));
	selected = yt_input_splitter_select(&splitter, 2.0f,
	    YT_INPUT_PHASE_B05D);
	CHECK(selected.length == 1 && selected.bytes[0] == 'L');
	CHECK(splitter.remote.length == 1);
	selected = yt_input_splitter_select(&splitter, 2.0f,
	    YT_INPUT_PHASE_AB36);
	CHECK(selected.length == 1 && selected.bytes[0] == 'R');

	CHECK(yt_input_splitter_push(&splitter, false, &local));
	CHECK(yt_input_splitter_push(&splitter, true, &remote));
	selected = yt_input_splitter_select(&splitter, 1.0f,
	    YT_INPUT_PHASE_AB36);
	CHECK(selected.length == 1 && selected.bytes[0] == 'L');
	CHECK(splitter.remote.length == 1);
	selected = yt_input_splitter_select(&splitter, 2.0f,
	    YT_INPUT_PHASE_WAIT);
	CHECK(selected.length == 0 && splitter.remote.length == 1);
	selected = yt_input_splitter_select(&splitter, 0.0f,
	    YT_INPUT_PHASE_WAIT);
	CHECK(selected.length == 1 && selected.bytes[0] == 'R');
}

static void
test_source_fifo(void)
{
	struct yt_input_splitter splitter;
	struct yt_input_value a = one('A');
	struct yt_input_value b = one('B');
	struct yt_input_value selected;

	yt_input_splitter_init(&splitter);
	CHECK(yt_input_splitter_push(&splitter, true, &a));
	CHECK(yt_input_splitter_push(&splitter, true, &b));
	selected = yt_input_splitter_select(&splitter, 0.0f,
	    YT_INPUT_PHASE_B05D);
	CHECK(selected.length == 1 && selected.bytes[0] == 'A');
	selected = yt_input_splitter_select(&splitter, 0.0f,
	    YT_INPUT_PHASE_B05D);
	CHECK(selected.length == 1 && selected.bytes[0] == 'B');
	selected = yt_input_splitter_select(&splitter, 0.0f,
	    YT_INPUT_PHASE_B05D);
	CHECK(selected.length == 0);
}

static void
test_b05d_keys(void)
{
	char accumulator[16] = "typed";
	char queue[16] = "old";
	char pager[8] = "";
	size_t position = 1;
	size_t length = 3;
	struct yt_b05d_key_state state = {
		.accumulator = accumulator,
		.accumulator_capacity = sizeof(accumulator),
		.queue = queue,
		.queue_capacity = sizeof(queue),
		.queue_position = &position,
		.queue_length = &length,
		.pager_key = pager,
		.pager_key_capacity = sizeof(pager),
	};
	struct yt_input_value value = one(0x12);

	CHECK(yt_b05d_process_key(&value, &state));
	CHECK(position == 1 && length == 3 && memcmp(queue, "old", 3) == 0);
	value = one('!');
	CHECK(yt_b05d_process_key(&value, &state));
	CHECK(position == 0 && length == 3 && memcmp(queue, "ld!", 3) == 0);
	value = one('\r');
	CHECK(yt_b05d_process_key(&value, &state));
	CHECK(length == 4 && memcmp(queue, "ld!\r", 4) == 0);
	value = one(0x7f);
	CHECK(yt_b05d_process_key(&value, &state));
	CHECK(length == 4);
	value.bytes[0] = 0;
	value.bytes[1] = 0x48;
	value.length = 2;
	CHECK(yt_b05d_process_key(&value, &state));
	CHECK(length == 4);
	value = one(0x18);
	CHECK(yt_b05d_process_key(&value, &state));
	CHECK(accumulator[0] == '\0' && queue[0] == '\0');
	CHECK(position == 0 && length == 0 && strcmp(pager, "Q") == 0);
}

int
main(void)
{
	test_arbitration();
	test_source_fifo();
	test_merged_fifo();
	test_b05d_keys();
	if (failures != 0) {
		fprintf(stderr, "%u input-model test(s) failed\n", failures);
		return 1;
	}
	puts("input-model tests passed");
	return 0;
}
