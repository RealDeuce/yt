#include "yt_presentation.h"
#include "yt_pager.h"
#include "yt_main_error.h"
#include "yt_game.h"
#include "yt_text.h"
#include "qb.h"

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

struct replay_capture {
	enum yt_present_operation operations[YT_PRESENT_EVENTS];
	size_t count;
};

static void
capture_remote(void *context, const uint8_t *data, size_t length, bool line)
{
	struct replay_capture *capture = context;

	(void)data;
	(void)length;
	capture->operations[capture->count++] = line
	    ? YT_PRESENT_REMOTE_LINE : YT_PRESENT_REMOTE_SEMI;
}

static void
capture_color(void *context, int foreground, int background)
{
	struct replay_capture *capture = context;

	(void)foreground;
	(void)background;
	capture->operations[capture->count++] = YT_PRESENT_LOCAL_COLOR;
}

static void
capture_text(void *context, const uint8_t *data, size_t length, bool line)
{
	struct replay_capture *capture = context;

	(void)data;
	(void)length;
	capture->operations[capture->count++] = line
	    ? YT_PRESENT_LOCAL_LINE : YT_PRESENT_LOCAL_SEMI;
}

static struct yt_present_state
state(bool ansi)
{
	struct yt_present_state value;

	memset(&value, 0, sizeof(value));
	value.sound.ansi = ansi ? -1.0f : 0.0f;
	value.sound.user_sound = -1.0f;
	value.sound.snoop = -1.0f;
	value.sound.local_sound = -1.0f;
	value.foreground = 2.0f;
	return value;
}

struct xannor_file_capture {
	struct yt_text_input input;
	struct yt_present_state presentation;
	uint8_t remote[512];
	size_t remote_length;
	size_t presented;
	size_t local_colors;
	size_t local_lines;
	size_t remote_lines;
	size_t remote_fragments;
};

static bool
xannor_file_close(void *context, struct yt_error *error)
{
	struct xannor_file_capture *capture = context;

	return yt_text_input_close(&capture->input, error);
}

static bool
xannor_file_open(void *context, const char *path, struct yt_error *error)
{
	struct xannor_file_capture *capture = context;

	return yt_text_input_open(&capture->input, path, error);
}

static bool
xannor_file_read(void *context, const uint8_t **line, size_t *length,
    bool *available, struct yt_error *error)
{
	struct xannor_file_capture *capture = context;

	return yt_text_input_read_line(&capture->input, line, length, available,
	    error);
}

static bool
xannor_file_present(void *context, const uint8_t *line, size_t length,
    struct yt_error *error)
{
	struct xannor_file_capture *capture = context;
	struct yt_present_result result;
	size_t index;

	(void)error;
	if (yt_present_line(line, length, &capture->presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	CHECK(result.remote_length <= sizeof(capture->remote)
	    - capture->remote_length);
	if (result.remote_length > sizeof(capture->remote)
	    - capture->remote_length)
		return false;
	memcpy(capture->remote + capture->remote_length, result.remote,
	    result.remote_length);
	capture->remote_length += result.remote_length;
	for (index = 0U; index < result.event_count; ++index) {
		switch (result.events[index].operation) {
		case YT_PRESENT_LOCAL_COLOR:
			++capture->local_colors;
			break;
		case YT_PRESENT_LOCAL_LINE:
			++capture->local_lines;
			break;
		case YT_PRESENT_REMOTE_LINE:
			++capture->remote_lines;
			break;
		case YT_PRESENT_REMOTE_SEMI:
			++capture->remote_fragments;
			break;
		default:
			CHECK(false);
			break;
		}
	}
	++capture->presented;
	return true;
}

static void
test_xannor_file_playback(void)
{
	static const struct yt_text_sequential_play_ops ops = {
		xannor_file_close,
		xannor_file_open,
		xannor_file_read,
		xannor_file_present,
	};
	static const uint8_t plain[] =
	    "\r\n"
	    "Congratulations! You have defeated the Xannor Headquarters! This marks you as\r\n"
	    "a SUPERIOR Trader! Be warned however, the Xannor have spies everywhere and\r\n"
	    "will be on the lookout for you! Hide or defend yourself well tonight!\r\n"
	    "\r\n";
	static const uint8_t ansi[] =
	    "\x1b[0;37;40m"
	    "\r\n"
	    "Congratulations! You have defeated the Xannor Headquarters! This marks you as\r\n"
	    "a SUPERIOR Trader! Be warned however, the Xannor have spies everywhere and\r\n"
	    "will be on the lookout for you! Hide or defend yourself well tonight!\r\n"
	    "\r\n";
	struct xannor_file_capture capture;
	struct yt_text_sequential_play_state playback;
	struct yt_error error;
	size_t pass;

	for (pass = 0U; pass < 2U; ++pass) {
		const uint8_t *expected = pass == 0U ? plain : ansi;
		size_t expected_length = pass == 0U ? sizeof(plain) - 1U
		    : sizeof(ansi) - 1U;

		memset(&capture, 0, sizeof(capture));
		yt_text_input_init(&capture.input);
		capture.presentation = state(pass != 0U);
		capture.presentation.foreground = 7.0f;
		memset(&playback, 0, sizeof(playback));
		playback.path = YT_DATA_DIR "XANNORHQ.TXT";
		yt_error_clear(&error);
		CHECK(yt_text_sequential_play_run(&playback, &ops, &capture,
		    &error));
		CHECK(!playback.file_open && playback.read_count == 6U
		    && playback.line_count == 5U && capture.presented == 5U);
		CHECK(capture.remote_length == expected_length
		    && memcmp(capture.remote, expected, expected_length) == 0);
		CHECK(capture.local_lines == 5U && capture.remote_lines == 5U
		    && capture.remote_fragments == (pass == 0U ? 5U : 6U)
		    && capture.local_colors == (pass == 0U ? 0U : 5U));
		CHECK(capture.presentation.foreground == 7.0f
		    && capture.presentation.background == 0.0f
		    && capture.presentation.bold == 0.0f
		    && capture.presentation.blink == 0.0f);
		yt_text_input_destroy(&capture.input);
	}
}

static void
test_color(void)
{
	struct yt_present_state current = state(false);
	struct yt_present_result result;
	static const uint8_t green[] = "\x1b[0;32;40m";
	static const uint8_t emphasized[] = "\x1b[0;33;40;5;1m";

	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == sizeof(green) - 1U);
	CHECK(memcmp(result.remote, green, sizeof(green) - 1U) == 0);
	CHECK(result.event_count == 2);
	CHECK(result.events[0].operation == YT_PRESENT_LOCAL_COLOR);
	CHECK(result.events[0].foreground == 2
	    && result.events[0].background == 0);
	CHECK(result.events[1].operation == YT_PRESENT_REMOTE_SEMI);
	CHECK(current.color_initialized == 1.0f);
	CHECK(current.cached_foreground == 2.0f
	    && current.cached_background == 0.0f);
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == 0 && result.event_count == 1);
	current.foreground = 1.0f;
	current.background = 1.0f;
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	CHECK(current.foreground == 3.0f && current.background == 0.0f);
	current.sound.mode = 2.0f;
	current.foreground = 4.0f;
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == 0);
	CHECK(current.cached_foreground == 3.0f);

	current = state(false);
	current.foreground = 3.0f;
	current.bold = 1.0f;
	current.blink = 1.0f;
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == sizeof(emphasized) - 1U
	    && memcmp(result.remote, emphasized, sizeof(emphasized) - 1U) == 0);
	CHECK(result.event_count == 2U
	    && result.events[0].operation == YT_PRESENT_LOCAL_COLOR
	    && result.events[0].foreground == 30
	    && result.events[0].background == 0
	    && result.events[1].operation == YT_PRESENT_REMOTE_SEMI);
	CHECK(current.cached_foreground == 0.0f
	    && current.cached_background == 0.0f
	    && current.bold == 0.0f && current.blink == 0.0f);

	current = state(false);
	current.bold = 2.0f;
	current.sound.snoop = 0.0f;
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == sizeof(green) - 1U
	    && memcmp(result.remote, green, sizeof(green) - 1U) == 0);
	CHECK(result.event_count == 2U
	    && result.events[0].operation == YT_PRESENT_LOCAL_COLOR
	    && result.events[0].foreground == 2
	    && result.events[0].background == 0
	    && result.events[1].operation == YT_PRESENT_REMOTE_SEMI);
	CHECK(current.cached_foreground == 0.0f
	    && current.cached_background == 0.0f
	    && current.bold == 0.0f && current.blink == 0.0f);
}

static void
test_color_process_cache(void)
{
	static const uint8_t raw_zero[] = {0x00, 0x00, 0x00, 0x00};
	static const uint8_t raw_one[] = {0x00, 0x00, 0x00, 0x81};
	static const uint8_t raw_two[] = {0x00, 0x00, 0x00, 0x82};
	static const uint8_t dirty_zero[] = {0x5a, 0xa5, 0x80, 0x00};
	static const uint8_t other_dirty_zero[] = {0x33, 0xcc, 0x80, 0x00};
	uint8_t foreground_raw[4];
	uint8_t background_raw[4];
	struct yt_present_state current;
	struct yt_present_result result;

	memcpy(foreground_raw, raw_two, sizeof(foreground_raw));
	memcpy(background_raw, dirty_zero, sizeof(background_raw));
	current = state(false);
	yt_present_bind_cached_foreground_process(&current, foreground_raw);
	yt_present_bind_cached_background_process(&current, background_raw);
	CHECK(yt_present_cached_foreground(&current) == 2.0f
	    && yt_present_cached_background(&current) == 0.0f);
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == 0U && result.event_count == 1U
	    && memcmp(foreground_raw, raw_two, sizeof(foreground_raw)) == 0
	    && memcmp(background_raw, dirty_zero,
	    sizeof(background_raw)) == 0);

	current.bold = 1.0f;
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	CHECK(memcmp(foreground_raw, raw_zero, sizeof(foreground_raw)) == 0
	    && memcmp(background_raw, raw_zero, sizeof(background_raw)) == 0);

	memcpy(foreground_raw, dirty_zero, sizeof(foreground_raw));
	memcpy(background_raw, other_dirty_zero, sizeof(background_raw));
	current = state(false);
	yt_present_bind_cached_foreground_process(&current, foreground_raw);
	yt_present_bind_cached_background_process(&current, background_raw);
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	CHECK(memcmp(foreground_raw, raw_two, sizeof(foreground_raw)) == 0
	    && memcmp(background_raw, raw_zero, sizeof(background_raw)) == 0);

	memcpy(foreground_raw, dirty_zero, sizeof(foreground_raw));
	memcpy(background_raw, raw_one, sizeof(background_raw));
	current = state(false);
	current.foreground = 40000.0f;
	yt_present_bind_cached_foreground_process(&current, foreground_raw);
	yt_present_bind_cached_background_process(&current, background_raw);
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OVERFLOW);
	CHECK(memcmp(foreground_raw, dirty_zero, sizeof(foreground_raw)) == 0
	    && memcmp(background_raw, raw_one, sizeof(background_raw)) == 0);
}

static void
test_color_process_table(void)
{
	static const float standard[] = {0, 4, 2, 6, 1, 5, 3, 7};
	static const uint8_t dirty_zero[] = {0x91, 0x82, 0x80, 0x00};
	static const uint8_t raw_one[] = {0x00, 0x00, 0x00, 0x81};
	static const uint8_t raw_true[] = {0x00, 0x00, 0x80, 0x81};
	uint8_t initialized_raw[4];
	uint8_t table_raw[32];
	uint8_t expected[4];
	uint8_t preserved[32];
	struct yt_present_state current;
	struct yt_present_result result;
	size_t index;

	memcpy(initialized_raw, dirty_zero, sizeof(initialized_raw));
	memset(table_raw, 0xa5, sizeof(table_raw));
	current = state(false);
	yt_present_bind_color_table_process(&current, initialized_raw,
	    table_raw);
	CHECK(yt_present_color_initialized(&current) == 0.0f);
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	CHECK(memcmp(initialized_raw, raw_one, sizeof(initialized_raw)) == 0);
	for (index = 0U; index < YT_ARRAY_LEN(standard); ++index) {
		CHECK(qb_mbf32_encode(standard[index], expected)
		    == QB_MBF_OK);
		CHECK(memcmp(table_raw + index * 4U, expected,
		    sizeof(expected)) == 0);
		CHECK(yt_present_color_memory(&current, index)
		    == standard[index]);
	}

	memcpy(initialized_raw, raw_true, sizeof(initialized_raw));
	memset(table_raw, 0, sizeof(table_raw));
	CHECK(qb_mbf32_encode(3.0f, table_raw) == QB_MBF_OK);
	CHECK(qb_mbf32_encode(5.0f, table_raw + 8U) == QB_MBF_OK);
	memcpy(preserved, table_raw, sizeof(preserved));
	current = state(false);
	current.sound.mode = 2.0f;
	yt_present_bind_color_table_process(&current, initialized_raw,
	    table_raw);
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == 0U && result.event_count == 1U
	    && result.events[0].operation == YT_PRESENT_LOCAL_COLOR
	    && result.events[0].foreground == 5
	    && result.events[0].background == 3
	    && memcmp(initialized_raw, raw_true,
	    sizeof(initialized_raw)) == 0
	    && memcmp(table_raw, preserved, sizeof(table_raw)) == 0);
}

static void
test_direct_output(void)
{
	struct yt_present_state current = state(false);
	struct yt_present_result result;

	CHECK(yt_present_line((const uint8_t *)"row", 3, &current, &result)
	    == YT_PRESENT_OK);
	CHECK(result.remote_length == 5);
	CHECK(memcmp(result.remote, "row\r\n", 5) == 0);
	CHECK(result.event_count == 3);
	CHECK(result.events[0].operation == YT_PRESENT_LOCAL_LINE);
	CHECK(result.events[1].operation == YT_PRESENT_REMOTE_LINE);
	CHECK(result.events[2].operation == YT_PRESENT_REMOTE_SEMI);
	current.sound.mode = 1.0f;
	CHECK(yt_present_character((const uint8_t *)"x", 1,
	    &current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == 0 && result.event_count == 1);
	CHECK(result.events[0].operation == YT_PRESENT_LOCAL_SEMI);
	current.sound.snoop = 0.0f;
	CHECK(yt_present_line((const uint8_t *)"hidden", 6,
	    &current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == 0 && result.event_count == 0);
	CHECK(yt_present_forced_local_line((const uint8_t *)"forced", 6,
	    &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == 0 && result.event_count == 1
	    && result.events[0].operation == YT_PRESENT_LOCAL_LINE
	    && result.events[0].length == 6
	    && memcmp(result.events[0].data, "forced", 6) == 0);
	CHECK(yt_present_local_beep(&result) == YT_PRESENT_OK);
	CHECK(result.remote_length == 0 && result.event_count == 1
	    && result.events[0].operation == YT_PRESENT_LOCAL_BEEP);
}

static void
test_paged_output(void)
{
	struct yt_present_state current = state(false);
	struct yt_present_result result;
	static const uint8_t green[] = "\x1b[0;32;40m";

	CHECK(yt_present_paged_text((const uint8_t *)"row", 3,
	    &current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == 3
	    && memcmp(result.remote, "row", 3) == 0);
	CHECK(result.event_count == 2);
	CHECK(result.events[0].operation == YT_PRESENT_LOCAL_SEMI);
	CHECK(result.events[1].operation == YT_PRESENT_REMOTE_SEMI);
	CHECK(yt_present_paged_finish(false, &current, &result)
	    == YT_PRESENT_OK);
	CHECK(result.remote_length == 2
	    && memcmp(result.remote, "\n\r", 2) == 0);
	CHECK(result.event_count == 3);
	CHECK(result.events[0].operation == YT_PRESENT_REMOTE_LINE);
	CHECK(result.events[1].operation == YT_PRESENT_LOCAL_LINE);
	CHECK(result.events[2].operation == YT_PRESENT_LOCAL_COLOR
	    && result.events[2].foreground == 7
	    && result.events[2].background == 0);

	current.sound.mode = 2.0f;
	CHECK(yt_present_paged_text((const uint8_t *)"local", 5,
	    &current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == 0 && result.event_count == 1);
	CHECK(result.events[0].operation == YT_PRESENT_LOCAL_SEMI);
	CHECK(yt_present_paged_finish(false, &current, &result)
	    == YT_PRESENT_OK);
	CHECK(result.remote_length == 0 && result.event_count == 2);
	CHECK(result.events[0].operation == YT_PRESENT_LOCAL_LINE);
	CHECK(result.events[1].operation == YT_PRESENT_LOCAL_COLOR);
	CHECK(yt_present_paged_finish(true, &current, &result)
	    == YT_PRESENT_OK);
	CHECK(result.remote_length == 0 && result.event_count == 1);
	CHECK(result.events[0].operation == YT_PRESENT_LOCAL_COLOR);
	current.sound.snoop = 0.0f;
	CHECK(yt_present_paged_finish(false, &current, &result)
	    == YT_PRESENT_OK);
	CHECK(result.remote_length == 0U && result.event_count == 1U
	    && result.events[0].operation == YT_PRESENT_LOCAL_COLOR);
	current.sound.mode = 0.0f;
	CHECK(yt_present_paged_finish(false, &current, &result)
	    == YT_PRESENT_OK);
	CHECK(result.remote_length == 2U
	    && memcmp(result.remote, "\n\r", 2U) == 0
	    && result.event_count == 2U
	    && result.events[0].operation == YT_PRESENT_REMOTE_LINE
	    && result.events[1].operation == YT_PRESENT_LOCAL_COLOR);

	current = state(true);
	CHECK(yt_present_paged_text((const uint8_t *)"row", 3,
	    &current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == sizeof(green) - 1U + 3U
	    && memcmp(result.remote, green, sizeof(green) - 1U) == 0
	    && memcmp(result.remote + sizeof(green) - 1U, "row", 3U) == 0);
	CHECK(result.event_count == 4U
	    && result.events[0].operation == YT_PRESENT_LOCAL_COLOR
	    && result.events[1].operation == YT_PRESENT_REMOTE_SEMI
	    && result.events[2].operation == YT_PRESENT_LOCAL_SEMI
	    && result.events[3].operation == YT_PRESENT_REMOTE_SEMI);
	CHECK(current.color_initialized == 1.0f
	    && current.cached_foreground == 2.0f
	    && current.cached_background == 0.0f);

	current = state(true);
	current.sound.mode = 2.0f;
	current.sound.snoop = 0.0f;
	CHECK(yt_present_paged_text((const uint8_t *)"hidden", 6,
	    &current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == 0U && result.event_count == 1U
	    && result.events[0].operation == YT_PRESENT_LOCAL_COLOR);

	current = state(false);
	current.sound.snoop = 0.0f;
	CHECK(yt_present_paged_text((const uint8_t *)"remote", 6,
	    &current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == 6U
	    && memcmp(result.remote, "remote", 6U) == 0
	    && result.event_count == 1U
	    && result.events[0].operation == YT_PRESENT_REMOTE_SEMI);
}

static void
test_editor_echo(void)
{
	static const uint8_t local_erase[] = {0x1d, ' ', 0x1d};
	static const uint8_t remote_erase[] = {'\b', ' ', '\b'};
	struct yt_present_state current = state(false);
	struct yt_present_result result;

	CHECK(yt_present_editor_echo(local_erase, sizeof(local_erase),
	    remote_erase, sizeof(remote_erase), &current, &result)
	    == YT_PRESENT_OK);
	CHECK(result.remote_length == sizeof(remote_erase)
	    && memcmp(result.remote, remote_erase, sizeof(remote_erase)) == 0);
	CHECK(result.event_count == 2);
	CHECK(result.events[0].operation == YT_PRESENT_LOCAL_SEMI
	    && result.events[0].length == sizeof(local_erase)
	    && memcmp(result.events[0].data, local_erase,
	    sizeof(local_erase)) == 0);
	CHECK(result.events[1].operation == YT_PRESENT_REMOTE_SEMI);
	current.sound.mode = 2.0f;
	CHECK(yt_present_editor_echo((const uint8_t *)"x", 1,
	    (const uint8_t *)"x", 1, &current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == 0 && result.event_count == 1);
	CHECK(result.events[0].operation == YT_PRESENT_LOCAL_SEMI);
	CHECK(yt_present_local_line((const uint8_t *)"carrier", 7,
	    &current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == 0 && result.event_count == 1);
	CHECK(result.events[0].operation == YT_PRESENT_LOCAL_LINE);
	current.sound.snoop = 0.0f;
	CHECK(yt_present_local_line((const uint8_t *)"hidden", 6,
	    &current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == 0 && result.event_count == 0);
}

struct pager_capture {
	uint8_t remote[4096];
	size_t remote_length;
	int last_local_foreground;
	int last_local_background;
	size_t local_event_count;
	size_t local_color_count;
	size_t local_line_count;
	size_t local_fragment_count;
	size_t local_byte_count;
	uint64_t local_fnv;
};

static void
pager_capture_local_hash(struct pager_capture *capture, uint8_t byte)
{
	if (capture->local_fnv == 0U)
		capture->local_fnv = UINT64_C(14695981039346656037);
	capture->local_fnv ^= byte;
	capture->local_fnv *= UINT64_C(1099511628211);
}

static void
pager_capture_result(struct pager_capture *capture,
    const struct yt_present_result *result)
{
	size_t index;

	CHECK(result->remote_length <= sizeof(capture->remote)
	    - capture->remote_length);
	if (result->remote_length <= sizeof(capture->remote)
	    - capture->remote_length) {
		memcpy(capture->remote + capture->remote_length, result->remote,
		    result->remote_length);
		capture->remote_length += result->remote_length;
	}
	for (index = 0; index < result->event_count; ++index) {
		const struct yt_present_event *event = &result->events[index];
		size_t byte;

		if (event->operation == YT_PRESENT_LOCAL_COLOR) {
			pager_capture_local_hash(capture, 'C');
			pager_capture_local_hash(capture,
			    (uint8_t)event->foreground);
			pager_capture_local_hash(capture,
			    (uint8_t)event->background);
			capture->local_color_count++;
			capture->last_local_foreground =
			    event->foreground;
			capture->last_local_background =
			    event->background;
		}
		else if (event->operation == YT_PRESENT_LOCAL_LINE
		    || event->operation == YT_PRESENT_LOCAL_SEMI) {
			pager_capture_local_hash(capture,
			    event->operation == YT_PRESENT_LOCAL_LINE ? 'N' : 'R');
			pager_capture_local_hash(capture, (uint8_t)event->length);
			pager_capture_local_hash(capture,
			    (uint8_t)(event->length >> 8U));
			for (byte = 0U; byte < event->length; ++byte)
				pager_capture_local_hash(capture, event->data[byte]);
			capture->local_byte_count += event->length;
			if (event->operation == YT_PRESENT_LOCAL_LINE)
				capture->local_line_count++;
			else
				capture->local_fragment_count++;
		}
		else
			continue;
		capture->local_event_count++;
	}
}

static void
pager_fixture_b05d(struct yt_pager_state *pager,
    struct yt_present_state *present, const uint8_t *text, size_t length,
    struct pager_capture *capture)
{
	struct yt_present_result result;
	int unused;

	CHECK(yt_present_paged_text(text, length, present, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	CHECK(yt_present_paged_finish(pager->newline_flag != 0.0f, present,
	    &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	CHECK(!yt_pager_advance(pager, present, &unused));
	pager->newline_flag = 0.0f;
}

static void
pager_capture_line(struct pager_capture *capture,
    struct yt_present_state *present, const uint8_t *text, size_t length)
{
	struct yt_present_result result;

	CHECK(yt_present_line(text, length, present, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
}

static void
test_projectile_parent_presentation(void)
{
	static const uint8_t expected[] =
	    "\r\nYou have 5. Send your cruise missile to what sector? "
	    "[ 1 to 2004 ] ?0\r\n\r\nInvalid Sector number!\n\r"
	    "\r\nYou have 5. Send your cruise missile to what sector? "
	    "[ 1 to 2004 ] ?";
	static const uint8_t invalid[] = "Invalid Sector number!";
	struct yt_present_state current = state(false);
	struct yt_pager_state pager;
	struct pager_capture capture;
	struct yt_present_result result;
	uint8_t prompt[192];
	size_t prompt_length;
	char accumulator[80];
	uint8_t response = '0';

	memset(&pager, 0, sizeof(pager));
	memset(&capture, 0, sizeof(capture));
	pager.foreground = 2;
	CHECK(yt_projectile_target_prompt(false, 5.0f, 2004.0f,
	    prompt, sizeof(prompt), &prompt_length));
	CHECK(yt_present_line(NULL, 0U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, prompt, prompt_length, &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo(&response, 1U, &response, 1U,
	    &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.bold = 1.0f;
	current.blink = 1.0f;
	pager_fixture_b05d(&pager, &current, invalid, sizeof(invalid) - 1U,
	    &capture);
	CHECK(yt_present_line(NULL, 0U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, prompt, prompt_length, &capture);
	CHECK(capture.remote_length == sizeof(expected) - 1U
	    && memcmp(capture.remote, expected, sizeof(expected) - 1U) == 0);
}

static struct pager_capture
projectile_ordinary_cycle_fixture(bool ansi, struct yt_present_state *current,
    struct yt_pager_state *pager)
{
	static const uint8_t main_prompt[] =
	    "Time:10:00  Main Command (?=Help)? ";
	static const uint8_t quantity_prompt[] = "Send how many? [0] ?";
	static const uint8_t turn_row[] = "One Turn Deducted, 59 left.";
	static const uint8_t loading[] =
	    "Loading course into misile targeting computer.";
	static const uint8_t tracking[] = "*** Tracking Report ***";
	static const uint8_t ending[] = "*** End of Report ***";
	static const uint8_t sector[] = "Sector: 7";
	static const uint8_t warps[] = "Warps lead to: 9";
	struct pager_capture capture;
	struct yt_present_result result;
	uint8_t target_prompt[192];
	size_t target_prompt_length;
	char accumulator[80] = "";
	uint8_t response;

	*current = state(ansi);
	current->foreground = 2.0f;
	memset(pager, 0, sizeof(*pager));
	pager->foreground = 2;
	memset(&capture, 0, sizeof(capture));
	if (ansi) {
		CHECK(yt_present_color(current, &result) == YT_PRESENT_OK);
		memset(&capture, 0, sizeof(capture));
	}
	CHECK(yt_projectile_target_prompt(false, 9.0f, 20.0f,
	    target_prompt, sizeof(target_prompt), &target_prompt_length));

	pager_capture_line(&capture, current, NULL, 0U);
	pager->newline_flag = 1.0f;
	pager_fixture_b05d(pager, current, main_prompt,
	    sizeof(main_prompt) - 1U, &capture);
	yt_pager_editor_enter(pager, accumulator, sizeof(accumulator));
	response = ')';
	CHECK(yt_present_editor_echo(&response, 1U, &response, 1U,
	    current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_capture_line(&capture, current, NULL, 0U);

	pager_capture_line(&capture, current, NULL, 0U);
	pager->newline_flag = 1.0f;
	pager_fixture_b05d(pager, current, target_prompt,
	    target_prompt_length, &capture);
	yt_pager_editor_enter(pager, accumulator, sizeof(accumulator));
	response = '9';
	CHECK(yt_present_editor_echo(&response, 1U, &response, 1U,
	    current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_capture_line(&capture, current, NULL, 0U);

	pager->newline_flag = 1.0f;
	pager_fixture_b05d(pager, current, quantity_prompt,
	    sizeof(quantity_prompt) - 1U, &capture);
	yt_pager_editor_enter(pager, accumulator, sizeof(accumulator));
	response = '1';
	CHECK(yt_present_editor_echo(&response, 1U, &response, 1U,
	    current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_capture_line(&capture, current, NULL, 0U);

	pager_capture_line(&capture, current, NULL, 0U);
	pager->newline_flag = 0.0f;
	pager_fixture_b05d(pager, current, turn_row, sizeof(turn_row) - 1U,
	    &capture);
	pager_capture_line(&capture, current, NULL, 0U);
	CHECK(yt_present_character(loading, sizeof(loading) - 1U, current,
	    &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_capture_line(&capture, current, NULL, 0U);
	pager_capture_line(&capture, current, tracking, sizeof(tracking) - 1U);
	pager_capture_line(&capture, current, ending, sizeof(ending) - 1U);

	current->foreground = 1.0f;
	pager->foreground = 1;
	pager_capture_line(&capture, current, NULL, 0U);
	pager_capture_line(&capture, current, sector, sizeof(sector) - 1U);
	pager_capture_line(&capture, current, warps, sizeof(warps) - 1U);

	pager->line_count = 0.0f;
	current->foreground = 2.0f;
	pager->foreground = 2;
	pager_capture_line(&capture, current, NULL, 0U);
	pager->newline_flag = 1.0f;
	pager_fixture_b05d(pager, current, main_prompt,
	    sizeof(main_prompt) - 1U, &capture);
	yt_pager_editor_enter(pager, accumulator, sizeof(accumulator));
	return capture;
}

static void
test_projectile_ordinary_cycle_presentation(void)
{
	static const uint8_t plain[] =
	    "\r\nTime:10:00  Main Command (?=Help)? )\r\n"
	    "\r\nYou have 9. Send your cruise missile to what sector? "
	    "[ 1 to 20 ] ?9\r\nSend how many? [0] ?1\r\n"
	    "\r\nOne Turn Deducted, 59 left.\n\r"
	    "\r\nLoading course into misile targeting computer.\r\n"
	    "*** Tracking Report ***\r\n*** End of Report ***\r\n"
	    "\r\nSector: 7\r\nWarps lead to: 9\r\n"
	    "\r\nTime:10:00  Main Command (?=Help)? ";
	static const uint8_t ansi[] =
	    "\r\nTime:10:00  Main Command (?=Help)? )\r\n"
	    "\r\nYou have 9. Send your cruise missile to what sector? "
	    "[ 1 to 20 ] ?9\r\nSend how many? [0] ?1\r\n"
	    "\r\nOne Turn Deducted, 59 left.\n\r"
	    "\r\nLoading course into misile targeting computer.\r\n"
	    "*** Tracking Report ***\r\n*** End of Report ***\r\n"
	    "\x1b[0;31;40m\r\nSector: 7\r\nWarps lead to: 9\r\n"
	    "\x1b[0;32;40m\r\nTime:10:00  Main Command (?=Help)? ";
	struct yt_present_state current;
	struct yt_pager_state pager;
	struct pager_capture capture;

	capture = projectile_ordinary_cycle_fixture(false, &current, &pager);
	CHECK(sizeof(plain) - 1U == 331U);
	CHECK(capture.remote_length == sizeof(plain) - 1U
	    && memcmp(capture.remote, plain, sizeof(plain) - 1U) == 0);
	CHECK(pager.line_count == 0.0f);
	capture = projectile_ordinary_cycle_fixture(true, &current, &pager);
	CHECK(sizeof(ansi) - 1U == 351U);
	CHECK(capture.remote_length == sizeof(ansi) - 1U
	    && memcmp(capture.remote, ansi, sizeof(ansi) - 1U) == 0);
	CHECK(pager.line_count == 0.0f);
}

static void
test_counterlaunch_presentation(void)
{
	static const uint8_t row[] = "BOB shot back with 3 missiles at you!";
	static const uint8_t plain[] =
	    "\r\nBOB shot back with 3 missiles at you!\r\n";
	static const uint8_t ansi[] =
	    "\x1b[0;33;40m\r\n"
	    "\x1b[0;33;40;1mBOB shot back with 3 missiles at you!\r\n";
	struct yt_present_state current = state(false);
	struct yt_present_result result;
	struct pager_capture capture;

	memset(&capture, 0, sizeof(capture));
	current.foreground = 3.0f;
	pager_capture_line(&capture, &current, NULL, 0U);
	CHECK(yt_present_bold_line(row, sizeof(row) - 1U, &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(capture.remote_length == sizeof(plain) - 1U
	    && memcmp(capture.remote, plain, sizeof(plain) - 1U) == 0);

	current = state(true);
	current.foreground = 5.0f;
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	memset(&capture, 0, sizeof(capture));
	current.foreground = 3.0f;
	pager_capture_line(&capture, &current, NULL, 0U);
	CHECK(yt_present_bold_line(row, sizeof(row) - 1U, &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(capture.remote_length == sizeof(ansi) - 1U
	    && memcmp(capture.remote, ansi, sizeof(ansi) - 1U) == 0);
}

static void
test_xannor_retaliation_presentation(void)
{
	static const uint8_t row[] =
	    "The Xannor have launched 19 missiles at sector 733!";
	static const uint8_t plain[] =
	    "\r\nThe Xannor have launched 19 missiles at sector 733!\r\n";
	static const uint8_t ansi[] =
	    "\r\n\x1b[0;36;40;1m"
	    "The Xannor have launched 19 missiles at sector 733!\r\n";
	struct yt_present_state current = state(false);
	struct yt_present_result result;
	struct pager_capture capture;

	current.foreground = 6.0f;
	memset(&capture, 0, sizeof(capture));
	pager_capture_line(&capture, &current, NULL, 0U);
	CHECK(yt_present_bold_line(row, sizeof(row) - 1U, &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(sizeof(plain) - 1U == 55U);
	CHECK(capture.remote_length == sizeof(plain) - 1U
	    && memcmp(capture.remote, plain, sizeof(plain) - 1U) == 0);

	current = state(true);
	current.foreground = 6.0f;
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	memset(&capture, 0, sizeof(capture));
	pager_capture_line(&capture, &current, NULL, 0U);
	CHECK(yt_present_bold_line(row, sizeof(row) - 1U, &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(sizeof(ansi) - 1U == 67U);
	CHECK(capture.remote_length == sizeof(ansi) - 1U
	    && memcmp(capture.remote, ansi, sizeof(ansi) - 1U) == 0);
}

static void
test_projectile_refusal_presentation(void)
{
	static const uint8_t no_ammo[] = "You dont have any!";
	static const uint8_t excessive[] = "You dont have that many!";
	static const uint8_t quantity_prompt[] = "Send how many? [0] ?";
	static const uint8_t no_ammo_expected[] =
	    "\r\n\r\nYou dont have any!\n\r";
	static const uint8_t excessive_expected[] =
	    "\r\nYou have 3. Send your cruise missile to what sector? "
	    "[ 1 to 2004 ] ?42\r\nSend how many? [0] ?4\r\n"
	    "You dont have that many!\n\r";
	struct yt_present_state current = state(false);
	struct yt_pager_state pager;
	struct pager_capture capture;
	struct yt_present_result result;
	uint8_t prompt[192];
	size_t prompt_length;
	char accumulator[80] = "";
	static const uint8_t target_response[] = "42";
	uint8_t quantity_response = '4';

	memset(&pager, 0, sizeof(pager));
	memset(&capture, 0, sizeof(capture));
	pager_capture_line(&capture, &current, NULL, 0U);
	pager_capture_line(&capture, &current, NULL, 0U);
	current.bold = 1.0f;
	current.blink = 1.0f;
	pager.newline_flag = 0.0f;
	pager_fixture_b05d(&pager, &current, no_ammo,
	    sizeof(no_ammo) - 1U, &capture);
	CHECK(capture.remote_length == sizeof(no_ammo_expected) - 1U
	    && memcmp(capture.remote, no_ammo_expected,
	    sizeof(no_ammo_expected) - 1U) == 0);

	current = state(false);
	memset(&pager, 0, sizeof(pager));
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_projectile_target_prompt(false, 3.0f, 2004.0f,
	    prompt, sizeof(prompt), &prompt_length));
	pager_capture_line(&capture, &current, NULL, 0U);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, prompt, prompt_length, &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo(target_response,
	    sizeof(target_response) - 1U, target_response,
	    sizeof(target_response) - 1U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_capture_line(&capture, &current, NULL, 0U);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, quantity_prompt,
	    sizeof(quantity_prompt) - 1U, &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo(&quantity_response, 1U,
	    &quantity_response, 1U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_capture_line(&capture, &current, NULL, 0U);
	pager.newline_flag = 0.0f;
	pager_fixture_b05d(&pager, &current, excessive,
	    sizeof(excessive) - 1U, &capture);
	CHECK(capture.remote_length == sizeof(excessive_expected) - 1U
	    && memcmp(capture.remote, excessive_expected,
	    sizeof(excessive_expected) - 1U) == 0);
}

static void
test_projectile_early_terminal_presentation(void)
{
	static const uint8_t route[] =
	    "*** You can't get there without going someplace you dont want to!";
	static const uint8_t self_destruct[] = "Missles self destructed!";
	static const uint8_t police[] =
	    "The Union Police have destroyed the Missiles!";
	static const uint8_t plasma[] = "Plasma bolts dissipated.";
	static const uint8_t route_expected[] =
	    "\r\n\r\n"
	    "*** You can't get there without going someplace you dont want to!"
	    "\r\n\r\nMissles self destructed!\r\n";
	static const uint8_t self_expected[] =
	    "\r\nMissles self destructed!\r\n";
	static const uint8_t police_expected[] =
	    "The Union Police have destroyed the Missiles!\r\n";
	static const uint8_t plasma_expected[] =
	    "\r\nPlasma bolts dissipated.\r\n\r\n";
	static const uint8_t route_ansi_expected[] =
	    "\x1b[0;32;40m\r\n\r\n\x1b[0;32;40;5;1m"
	    "*** You can't get there without going someplace you dont want to!"
	    "\r\n\x1b[0;32;40m\r\n\x1b[0;32;40;5;1m"
	    "Missles self destructed!\r\n";
	struct yt_present_state current = state(false);
	struct pager_capture capture;
	struct yt_present_result result;
	uint8_t row[96];
	size_t row_length;

	memset(&capture, 0, sizeof(capture));
	pager_capture_line(&capture, &current, NULL, 0U);
	pager_capture_line(&capture, &current, NULL, 0U);
	CHECK(yt_projectile_route_failure_row(false, row, sizeof(row),
	    &row_length));
	CHECK(row_length == sizeof(route) - 1U
	    && memcmp(row, route, row_length) == 0);
	current.blink = 1.0f;
	CHECK(yt_present_bold_line(row, row_length, &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_capture_line(&capture, &current, NULL, 0U);
	CHECK(yt_projectile_route_failure_row(true, row, sizeof(row),
	    &row_length));
	CHECK(row_length == sizeof(self_destruct) - 1U
	    && memcmp(row, self_destruct, row_length) == 0);
	current.blink = 1.0f;
	CHECK(yt_present_bold_line(row, row_length, &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(capture.remote_length == sizeof(route_expected) - 1U
	    && memcmp(capture.remote, route_expected,
	    sizeof(route_expected) - 1U) == 0);
	CHECK(current.bold == 1.0f && current.blink == 1.0f);

	current = state(false);
	memset(&capture, 0, sizeof(capture));
	pager_capture_line(&capture, &current, NULL, 0U);
	current.blink = 1.0f;
	CHECK(yt_present_bold_line(self_destruct,
	    sizeof(self_destruct) - 1U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(capture.remote_length == sizeof(self_expected) - 1U
	    && memcmp(capture.remote, self_expected,
	    sizeof(self_expected) - 1U) == 0);

	memset(&capture, 0, sizeof(capture));
	pager_capture_line(&capture, &current, police, sizeof(police) - 1U);
	CHECK(capture.remote_length == sizeof(police_expected) - 1U
	    && memcmp(capture.remote, police_expected,
	    sizeof(police_expected) - 1U) == 0);

	memset(&capture, 0, sizeof(capture));
	pager_capture_line(&capture, &current, NULL, 0U);
	pager_capture_line(&capture, &current, plasma, sizeof(plasma) - 1U);
	pager_capture_line(&capture, &current, NULL, 0U);
	CHECK(capture.remote_length == sizeof(plasma_expected) - 1U
	    && memcmp(capture.remote, plasma_expected,
	    sizeof(plasma_expected) - 1U) == 0);

	current = state(true);
	memset(&capture, 0, sizeof(capture));
	pager_capture_line(&capture, &current, NULL, 0U);
	pager_capture_line(&capture, &current, NULL, 0U);
	current.blink = 1.0f;
	CHECK(yt_present_bold_line(route, sizeof(route) - 1U, &current,
	    &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_capture_line(&capture, &current, NULL, 0U);
	current.blink = 1.0f;
	CHECK(yt_present_bold_line(self_destruct,
	    sizeof(self_destruct) - 1U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(capture.remote_length == sizeof(route_ansi_expected) - 1U
	    && memcmp(capture.remote, route_ansi_expected,
	    sizeof(route_ansi_expected) - 1U) == 0);
	CHECK(current.bold == 0.0f && current.blink == 0.0f);
}

static struct pager_capture
pager_fixture(const char *answer, bool ansi, float mode,
    struct yt_pager_state *pager, struct yt_present_state *present)
{
	static const uint8_t prompt[] =
	    "[ENTER] for more, [E] to end, or [NS] for Non-stop ";
	static const uint8_t notice[] = "Ctrl-X to Stop";
	struct pager_capture capture;
	struct yt_present_result result;
	char accumulator[80];
	char response[80];
	size_t index;
	int saved_foreground;
	bool emit_notice;

	memset(&capture, 0, sizeof(capture));
	*present = state(ansi);
	present->sound.mode = mode;
	present->foreground = 6.0f;
	memset(pager, 0, sizeof(*pager));
	pager->line_count = 22.0f;
	pager->foreground = 6;
	CHECK(yt_pager_advance(pager, present, &saved_foreground));
	CHECK(saved_foreground == 6 && pager->line_count == 0.0f);
	pager_fixture_b05d(pager, present, prompt, sizeof(prompt) - 1U,
	    &capture);
	yt_pager_editor_enter(pager, accumulator, sizeof(accumulator));
	CHECK(pager->line_count == 0.0f && pager->nonstop == 0.0f
	    && pager->key[0] == '\0' && accumulator[0] == '\0');
	for (index = 0; answer[index] != '\0'; ++index) {
		uint8_t byte = (uint8_t)answer[index];

		accumulator[index] = (char)byte;
		accumulator[index + 1U] = '\0';
		CHECK(yt_present_editor_echo(&byte, 1, &byte, 1, present,
		    &result) == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		pager->newline_flag = 1.0f;
	}
	pager->newline_flag = 0.0f;
	CHECK(yt_present_line(NULL, 0, present, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	snprintf(response, sizeof(response), "%s", accumulator);
	emit_notice = yt_pager_accept_response(pager, response,
	    sizeof(response));
	if (emit_notice)
		pager_fixture_b05d(pager, present, notice,
		    sizeof(notice) - 1U, &capture);
	yt_pager_complete(pager, present, saved_foreground);
	return capture;
}

static void
test_pager_transactions(void)
{
	static const uint8_t prompt[] =
	    "[ENTER] for more, [E] to end, or [NS] for Non-stop ";
	static const uint8_t ansi_prompt[] = "\x1b[0;33;40;1m";
	static const uint8_t ansi_submit[] = "\x1b[0;33;40m";
	struct yt_pager_state pager;
	struct yt_present_state present;
	struct pager_capture capture;
	uint8_t expected[256];
	size_t length;

	capture = pager_fixture("", false, 0.0f, &pager, &present);
	length = sizeof(prompt) - 1U;
	memcpy(expected, prompt, length);
	memcpy(expected + length, "\r\n", 2);
	length += 2U;
	CHECK(capture.remote_length == length
	    && memcmp(capture.remote, expected, length) == 0);
	CHECK(pager.line_count == 0.0f && pager.nonstop == 0.0f
	    && pager.key[0] == '\0' && pager.foreground == 6
	    && present.foreground == 6.0f && present.bold == 1.0f);
	CHECK(capture.last_local_foreground == 7
	    && capture.last_local_background == 0);

	capture = pager_fixture("E", false, 0.0f, &pager, &present);
	length = sizeof(prompt) - 1U;
	memcpy(expected, prompt, length);
	expected[length++] = 'E';
	memcpy(expected + length, "\r\n", 2);
	length += 2U;
	CHECK(capture.remote_length == length
	    && memcmp(capture.remote, expected, length) == 0);
	CHECK(strcmp(pager.key, "Q") == 0 && pager.nonstop == 0.0f);

	capture = pager_fixture("NS", false, 0.0f, &pager, &present);
	length = sizeof(prompt) - 1U;
	memcpy(expected, prompt, length);
	memcpy(expected + length, "NS\r\nCtrl-X to Stop\n\r", 20);
	length += 20U;
	CHECK(capture.remote_length == length
	    && memcmp(capture.remote, expected, length) == 0);
	CHECK(strcmp(pager.key, "NS") == 0 && pager.nonstop == 1.0f
	    && pager.line_count == 1.0f);
	CHECK(capture.last_local_foreground == 7
	    && capture.last_local_background == 0);

	capture = pager_fixture("", true, 0.0f, &pager, &present);
	length = 0;
	memcpy(expected + length, ansi_prompt, sizeof(ansi_prompt) - 1U);
	length += sizeof(ansi_prompt) - 1U;
	memcpy(expected + length, prompt, sizeof(prompt) - 1U);
	length += sizeof(prompt) - 1U;
	memcpy(expected + length, ansi_submit, sizeof(ansi_submit) - 1U);
	length += sizeof(ansi_submit) - 1U;
	memcpy(expected + length, "\r\n", 2);
	length += 2U;
	CHECK(capture.remote_length == length
	    && memcmp(capture.remote, expected, length) == 0);
	CHECK(present.bold == 0.0f && present.foreground == 6.0f);
	CHECK(capture.last_local_foreground == 6
	    && capture.last_local_background == 0);

	capture = pager_fixture("E", false, 2.0f, &pager, &present);
	CHECK(capture.remote_length == 2
	    && memcmp(capture.remote, "\r\n", 2) == 0);
}

static void
test_pager_gates(void)
{
	struct yt_present_state present = state(false);
	struct yt_pager_state pager;
	char response[80];
	int saved = -1;

	memset(&pager, 0, sizeof(pager));
	pager.foreground = 5;
	pager.line_count = 21.0f;
	CHECK(!yt_pager_advance(&pager, &present, &saved));
	CHECK(pager.line_count == 22.0f && saved == -1);
	pager.line_count = 22.0f;
	pager.nonstop = 2.0f;
	CHECK(!yt_pager_advance(&pager, &present, &saved));
	CHECK(pager.line_count == 23.0f);
	pager.nonstop = 0.0f;
	pager.line_count = 22.0f;
	pager.newline_flag = -1.0f;
	CHECK(!yt_pager_advance(&pager, &present, &saved));
	pager.newline_flag = 0.0f;
	pager.line_count = 24.0f;
	CHECK(yt_pager_advance(&pager, &present, &saved));
	CHECK(saved == 5 && pager.line_count == 0.0f
	    && pager.foreground == 3 && present.foreground == 3.0f
	    && present.bold == 1.0f && pager.newline_flag == 1.0f);

	snprintf(response, sizeof(response), "never");
	CHECK(!yt_pager_accept_response(&pager, response, sizeof(response)));
	CHECK(strcmp(response, "NEVER") == 0
	    && strcmp(pager.key, "NEVER") == 0);
	yt_pager_complete(&pager, &present, saved);
	CHECK(strcmp(pager.key, "Q") == 0 && pager.foreground == 5
	    && present.foreground == 5.0f);
}

struct raw_pager_context {
	unsigned finish_calls;
};

static bool
raw_pager_carrier(void *context)
{
	(void)context;
	return true;
}

static bool
raw_pager_sample(void *context, struct yt_input_value *sampled)
{
	(void)context;
	memset(sampled, 0, sizeof(*sampled));
	return true;
}

static bool
raw_pager_present(void *context, const uint8_t *text, size_t length)
{
	(void)context;
	(void)text;
	(void)length;
	return true;
}

static bool
raw_pager_finish(void *context, bool newline_flag)
{
	struct raw_pager_context *raw = context;

	(void)newline_flag;
	++raw->finish_calls;
	return true;
}

static bool
raw_pager_response(void *context, char *response, size_t capacity)
{
	(void)context;
	if (capacity == 0U)
		return false;
	response[0] = '\0';
	return true;
}

static void
test_pager_raw_process_cells(void)
{
	static const struct yt_paged_row_ops ops = {
		raw_pager_carrier,
		raw_pager_sample,
		raw_pager_present,
		raw_pager_finish,
		raw_pager_response,
	};
	static const uint8_t count_22[4] = {0x00, 0x00, 0x30, 0x85};
	static const uint8_t count_23[4] = {0x00, 0x00, 0x38, 0x85};
	static const uint8_t dirty_line_zero[4] = {0x00, 0x00, 0x38, 0x00};
	static const uint8_t dirty_zero[4] = {0x00, 0x00, 0x0c, 0x00};
	static const uint8_t zero[4] = {0x00, 0x00, 0x00, 0x00};
	static const uint8_t one[4] = {0x00, 0x00, 0x00, 0x81};
	static const uint8_t three[4] = {0x00, 0x00, 0x40, 0x82};
	static const uint8_t seven[4] = {0x00, 0x00, 0x60, 0x83};
	struct yt_pager_state pager;
	struct yt_present_state present = state(false);
	struct yt_b05d_key_state key_state;
	struct raw_pager_context context = {0};
	uint8_t line_count[4];
	uint8_t nonstop[4];
	uint8_t newline[4];
	uint8_t foreground[4];
	uint8_t saved_foreground[4] = {0xde, 0xad, 0xbe, 0xef};
	char accumulator[8] = "x";
	char queue[8] = "";
	char pager_key[8] = "";
	char response[8] = "ns";
	size_t queue_position = 0U;
	size_t queue_length = 0U;
	int saved = -1;

	memset(&pager, 0, sizeof(pager));
	memcpy(line_count, count_22, sizeof(line_count));
	memcpy(nonstop, one, sizeof(nonstop));
	memcpy(newline, zero, sizeof(newline));
	memcpy(foreground, seven, sizeof(foreground));
	yt_pager_bind_process_cells(&pager, line_count, nonstop, newline,
	    foreground, saved_foreground);
	CHECK(!yt_pager_advance(&pager, &present, &saved));
	CHECK(memcmp(line_count, count_23, sizeof(line_count)) == 0);

	memcpy(line_count, count_22, sizeof(line_count));
	memcpy(nonstop, zero, sizeof(nonstop));
	CHECK(yt_pager_advance(&pager, &present, &saved));
	CHECK(saved == 7
	    && memcmp(saved_foreground, seven, sizeof(saved_foreground)) == 0
	    && memcmp(line_count, dirty_line_zero, sizeof(line_count)) == 0
	    && memcmp(foreground, three, sizeof(foreground)) == 0
	    && memcmp(newline, one, sizeof(newline)) == 0);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(accumulator[0] == '\0'
	    && memcmp(nonstop, dirty_zero, sizeof(nonstop)) == 0
	    && memcmp(line_count, dirty_zero, sizeof(line_count)) == 0);
	CHECK(yt_pager_accept_response(&pager, response, sizeof(response))
	    && memcmp(nonstop, one, sizeof(nonstop)) == 0);
	yt_pager_complete(&pager, &present, saved);
	CHECK(memcmp(foreground, seven, sizeof(foreground)) == 0
	    && present.foreground == 7.0f);

	memcpy(line_count, zero, sizeof(line_count));
	memcpy(newline, one, sizeof(newline));
	memset(&key_state, 0, sizeof(key_state));
	key_state.accumulator = accumulator;
	key_state.accumulator_capacity = sizeof(accumulator);
	key_state.queue = queue;
	key_state.queue_capacity = sizeof(queue);
	key_state.queue_position = &queue_position;
	key_state.queue_length = &queue_length;
	key_state.pager_key = pager_key;
	key_state.pager_key_capacity = sizeof(pager_key);
	CHECK(yt_paged_row_run(&pager, &present, &key_state,
	    (const uint8_t *)"x", 1U, &ops, &context));
	CHECK(context.finish_calls == 1U
	    && memcmp(newline, dirty_zero, sizeof(newline)) == 0);
}

enum viewer_pager_event {
	VIEWER_PAGER_CARRIER,
	VIEWER_PAGER_SAMPLE,
	VIEWER_PAGER_PRESENT,
	VIEWER_PAGER_FINISH,
	VIEWER_PAGER_RESPONSE,
};

struct viewer_pager_join {
	struct yt_pager_state pager;
	struct yt_present_state presentation;
	struct yt_b05d_key_state key_state;
	struct pager_capture capture;
	enum viewer_pager_event events[4096];
	size_t event_count;
	size_t fail_at;
	struct yt_error *active_error;
	char accumulator[80];
	char queue[80];
	size_t queue_position;
	size_t queue_length;
	char response[80];
	char source[80];
	size_t source_length;
	uint8_t *remote_output;
	size_t remote_capacity;
	size_t remote_length;
	size_t sample_calls;
	struct yt_input_value injected_sample;
	size_t injected_sample_call;
	size_t injected_sample_hits;
	float first_finish_line_count;
	size_t response_calls;
	bool response_from_queue;
	size_t direct_calls;
	size_t total_rows;
	size_t position;
	size_t eof_calls;
	size_t read_calls;
	size_t active_row;
	size_t active_sample;
	size_t ctrl_x_row;
	uint8_t line[32];
	uint8_t local_rows[600][80];
	size_t local_lengths[600];
	size_t local_row_count;
	uint8_t local_fragment[80];
	size_t local_fragment_length;
	int local_foregrounds[1200];
	int local_backgrounds[1200];
	size_t local_color_count;
	bool file_open;
	bool final_blank;
};

static bool
viewer_pager_record(struct viewer_pager_join *join,
    enum viewer_pager_event event)
{
	CHECK(join->event_count < YT_ARRAY_LEN(join->events));
	if (join->event_count < YT_ARRAY_LEN(join->events))
		join->events[join->event_count] = event;
	++join->event_count;
	if (join->event_count != join->fail_at)
		return true;
	if (join->active_error != NULL) {
		join->active_error->status = YT_IO_ERROR;
		(void)snprintf(join->active_error->operation,
		    sizeof(join->active_error->operation), "%s",
		    "injected joined pager failure");
	}
	return false;
}

static void
viewer_pager_capture_result(struct viewer_pager_join *join,
    const struct yt_present_result *result)
{
	size_t index;

	if (join->remote_output == NULL)
		pager_capture_result(&join->capture, result);
	else {
		CHECK(join->remote_length <= join->remote_capacity
		    && result->remote_length <= join->remote_capacity
		    - join->remote_length);
		if (join->remote_length <= join->remote_capacity
		    && result->remote_length <= join->remote_capacity
		    - join->remote_length) {
			memcpy(join->remote_output + join->remote_length,
			    result->remote, result->remote_length);
			join->remote_length += result->remote_length;
		}
	}
	for (index = 0U; index < result->event_count; ++index) {
		const struct yt_present_event *event = &result->events[index];

		if (event->operation == YT_PRESENT_LOCAL_COLOR) {
			join->capture.last_local_foreground = event->foreground;
			join->capture.last_local_background = event->background;
			CHECK(join->local_color_count
			    < YT_ARRAY_LEN(join->local_foregrounds));
			if (join->local_color_count
			    < YT_ARRAY_LEN(join->local_foregrounds)) {
				join->local_foregrounds[join->local_color_count] =
				    event->foreground;
				join->local_backgrounds[join->local_color_count] =
				    event->background;
			}
			++join->local_color_count;
			continue;
		}
		if (event->operation != YT_PRESENT_LOCAL_SEMI
		    && event->operation != YT_PRESENT_LOCAL_LINE)
			continue;
		CHECK(event->length <= sizeof(join->local_fragment)
		    - join->local_fragment_length);
		if (event->length > sizeof(join->local_fragment)
		    - join->local_fragment_length)
			continue;
		if (event->length != 0U)
			memcpy(join->local_fragment + join->local_fragment_length,
			    event->data, event->length);
		join->local_fragment_length += event->length;
		if (event->operation != YT_PRESENT_LOCAL_LINE)
			continue;
		CHECK(join->local_row_count < YT_ARRAY_LEN(join->local_rows));
		if (join->local_row_count < YT_ARRAY_LEN(join->local_rows)) {
			memcpy(join->local_rows[join->local_row_count],
			    join->local_fragment, join->local_fragment_length);
			join->local_lengths[join->local_row_count] =
			    join->local_fragment_length;
		}
		++join->local_row_count;
		join->local_fragment_length = 0U;
	}
}

static bool
viewer_pager_carrier(void *context)
{
	return viewer_pager_record(context, VIEWER_PAGER_CARRIER);
}

static bool
viewer_pager_sample(void *context, struct yt_input_value *sampled)
{
	struct viewer_pager_join *join = context;

	if (!viewer_pager_record(join, VIEWER_PAGER_SAMPLE))
		return false;
	memset(sampled, 0, sizeof(*sampled));
	++join->sample_calls;
	++join->active_sample;
	if (join->injected_sample_call != 0U
	    && join->sample_calls == join->injected_sample_call) {
		*sampled = join->injected_sample;
		++join->injected_sample_hits;
		return true;
	}
	if (join->ctrl_x_row != 0U
	    && join->active_row == join->ctrl_x_row
	    && join->active_sample == 1U) {
		sampled->bytes[0] = 0x18U;
		sampled->length = 1U;
	}
	return true;
}

static bool
viewer_pager_present(void *context, const uint8_t *text, size_t length)
{
	struct viewer_pager_join *join = context;
	struct yt_present_result result;

	if (!viewer_pager_record(join, VIEWER_PAGER_PRESENT))
		return false;
	CHECK(length < sizeof(join->source));
	if (length >= sizeof(join->source))
		return false;
	if (length != 0U)
		memcpy(join->source, text, length);
	join->source[length] = '\0';
	join->source_length = length;
	if (yt_present_paged_text(text, length, &join->presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	return true;
}

static bool
viewer_pager_finish(void *context, bool newline_flag)
{
	struct viewer_pager_join *join = context;
	struct yt_present_result result;

	if (!viewer_pager_record(join, VIEWER_PAGER_FINISH))
		return false;
	if (join->sample_calls == 1U)
		join->first_finish_line_count = join->pager.line_count;
	if (yt_present_paged_finish(newline_flag, &join->presentation,
	    &result) != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	return true;
}

static bool
viewer_pager_response(void *context, char *response, size_t capacity)
{
	struct viewer_pager_join *join = context;
	struct yt_present_result result;
	size_t length = strlen(join->response);
	size_t index;

	if (!viewer_pager_record(join, VIEWER_PAGER_RESPONSE))
		return false;
	++join->response_calls;
	if (join->response_from_queue) {
		struct yt_input_value selected;

		yt_pager_editor_enter(&join->pager, join->accumulator,
		    sizeof(join->accumulator));
		for (;;) {
			if (!yt_input_ab36_queue_pop(join->queue,
			    sizeof(join->queue), &join->queue_position,
			    &join->queue_length, &selected)
			    || selected.length != 1U)
				return false;
			if (selected.bytes[0] == '\r')
				break;
			length = strlen(join->accumulator);
			if (length + 1U >= sizeof(join->accumulator)
			    || length + 1U >= capacity)
				return false;
			join->accumulator[length] = (char)selected.bytes[0];
			join->accumulator[length + 1U] = '\0';
			join->pager.newline_flag = 1.0f;
			if (yt_present_editor_echo(selected.bytes, 1U,
			    selected.bytes, 1U, &join->presentation, &result)
			    != YT_PRESENT_OK)
				return false;
			viewer_pager_capture_result(join, &result);
		}
		join->pager.newline_flag = 0.0f;
		if (yt_present_line(NULL, 0U, &join->presentation, &result)
		    != YT_PRESENT_OK)
			return false;
		viewer_pager_capture_result(join, &result);
		(void)snprintf(response, capacity, "%s", join->accumulator);
		join->source[0] = '\r';
		join->source[1] = '\0';
		join->source_length = 1U;
		return true;
	}
	if (length >= capacity)
		return false;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	for (index = 0U; index < length; ++index) {
		uint8_t byte = (uint8_t)join->response[index];

		join->accumulator[index] = (char)byte;
		join->accumulator[index + 1U] = '\0';
		join->pager.newline_flag = 1.0f;
		if (yt_present_editor_echo(&byte, 1U, &byte, 1U,
		    &join->presentation, &result) != YT_PRESENT_OK)
			return false;
		viewer_pager_capture_result(join, &result);
	}
	join->pager.newline_flag = 0.0f;
	if (yt_present_line(NULL, 0U, &join->presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	(void)snprintf(response, capacity, "%s", join->accumulator);
	join->source[0] = '\r';
	join->source[1] = '\0';
	join->source_length = 1U;
	return true;
}

static const struct yt_paged_row_ops viewer_pager_ops = {
	viewer_pager_carrier,
	viewer_pager_sample,
	viewer_pager_present,
	viewer_pager_finish,
	viewer_pager_response,
};

static bool
viewer_pager_close(void *context, struct yt_error *error)
{
	struct viewer_pager_join *join = context;

	(void)error;
	join->file_open = false;
	return true;
}

static bool
viewer_pager_open(void *context, const char *path, struct yt_error *error)
{
	struct viewer_pager_join *join = context;

	(void)path;
	(void)error;
	join->file_open = true;
	return true;
}

static bool
viewer_pager_eof(void *context, bool *eof, struct yt_error *error)
{
	struct viewer_pager_join *join = context;

	(void)error;
	++join->eof_calls;
	*eof = join->position == join->total_rows;
	return true;
}

static bool
viewer_pager_read(void *context, const uint8_t **line, size_t *length,
    bool *available, struct yt_error *error)
{
	struct viewer_pager_join *join = context;
	int count;

	(void)error;
	CHECK(join->position < join->total_rows);
	if (join->position >= join->total_rows)
		return false;
	++join->position;
	++join->read_calls;
	count = snprintf((char *)join->line, sizeof(join->line), "row %02zu",
	    join->position);
	CHECK(count > 0 && (size_t)count < sizeof(join->line));
	if (count <= 0 || (size_t)count >= sizeof(join->line))
		return false;
	*line = join->line;
	*length = (size_t)count;
	*available = true;
	return true;
}

static bool
viewer_pager_stream_present(void *context, const uint8_t *text,
    size_t length, bool paged, struct yt_error *error)
{
	struct viewer_pager_join *join = context;
	struct yt_present_result result;
	bool ok;

	if (!paged) {
		++join->direct_calls;
		CHECK(text == NULL && length == 0U);
		if (yt_present_line(NULL, 0U, &join->presentation, &result)
		    != YT_PRESENT_OK)
			return false;
		viewer_pager_capture_result(join, &result);
		join->final_blank = true;
		return true;
	}
	join->active_row = join->position;
	join->active_sample = 0U;
	join->active_error = error;
	ok = yt_paged_row_run(&join->pager, &join->presentation,
	    &join->key_state, text, length, &viewer_pager_ops, join);
	join->active_error = NULL;
	return ok;
}

static const struct yt_file_viewer_stream_ops viewer_pager_stream_ops = {
	viewer_pager_close,
	viewer_pager_open,
	viewer_pager_eof,
	viewer_pager_read,
	viewer_pager_stream_present,
};

static void
viewer_pager_initialize(struct viewer_pager_join *join,
    struct yt_file_viewer_stream_state *stream, size_t rows,
    float initial_count, const char *response, size_t ctrl_x_row)
{
	memset(join, 0, sizeof(*join));
	join->presentation = state(false);
	join->presentation.foreground = 6.0f;
	join->pager.foreground = 6;
	join->pager.line_count = initial_count;
	join->total_rows = rows;
	join->ctrl_x_row = ctrl_x_row;
	(void)snprintf(join->response, sizeof(join->response), "%s", response);
	(void)snprintf(join->accumulator, sizeof(join->accumulator), "%s",
	    "typed");
	(void)snprintf(join->queue, sizeof(join->queue), "%s", "abc");
	join->queue_length = 3U;
	join->key_state.accumulator = join->accumulator;
	join->key_state.accumulator_capacity = sizeof(join->accumulator);
	join->key_state.queue = join->queue;
	join->key_state.queue_capacity = sizeof(join->queue);
	join->key_state.queue_position = &join->queue_position;
	join->key_state.queue_length = &join->queue_length;
	join->key_state.pager_key = join->pager.key;
	join->key_state.pager_key_capacity = sizeof(join->pager.key);
	memset(stream, 0, sizeof(*stream));
	stream->path = "joined.txt";
	stream->play.foreground = &join->presentation.foreground;
	stream->play.pager_foreground = &join->pager.foreground;
	stream->play.bold = &join->presentation.bold;
	stream->play.line_count = &join->pager.line_count;
	stream->play.pager_key = join->pager.key;
	stream->play.saved_foreground = 6.0f;
	stream->play.saved_pager_foreground = 6;
}

static void
test_file_viewer_pager_join(void)
{
	static const uint8_t ansi_one_row[] =
	    "\x1b[0;32;40mrow 01\n\r\x1b[0;36;40m\r\n";
	static const enum viewer_pager_event threshold_events[] = {
		VIEWER_PAGER_CARRIER, VIEWER_PAGER_SAMPLE,
		VIEWER_PAGER_PRESENT, VIEWER_PAGER_CARRIER,
		VIEWER_PAGER_FINISH,
		VIEWER_PAGER_CARRIER, VIEWER_PAGER_SAMPLE,
		VIEWER_PAGER_PRESENT, VIEWER_PAGER_CARRIER,
		VIEWER_PAGER_FINISH, VIEWER_PAGER_RESPONSE,
	};
	struct viewer_pager_join join;
	struct yt_file_viewer_stream_state stream;
	struct yt_error error;
	size_t failure;
	size_t index;

	viewer_pager_initialize(&join, &stream, 24U, 0.0f, "E", 0U);
	CHECK(yt_file_viewer_stream_run(&stream, &viewer_pager_stream_ops,
	    &join, NULL));
	CHECK(join.read_calls == 23U && join.eof_calls == 24U
	    && strcmp(join.pager.key, "Q") == 0
	    && join.source_length == 1U && join.source[0] == '\r'
	    && !join.file_open && join.final_blank);

	viewer_pager_initialize(&join, &stream, 24U, 0.0f, "", 5U);
	CHECK(yt_file_viewer_stream_run(&stream, &viewer_pager_stream_ops,
	    &join, NULL));
	CHECK(join.read_calls == 5U && join.eof_calls == 6U
	    && strcmp(join.pager.key, "Q") == 0
	    && join.source_length == 6U
	    && memcmp(join.source, "row 05", 6U) == 0
	    && join.accumulator[0] == '\0' && join.queue_length == 0U);

	viewer_pager_initialize(&join, &stream, 24U, 0.0f, "", 23U);
	CHECK(yt_file_viewer_stream_run(&stream, &viewer_pager_stream_ops,
	    &join, NULL));
	CHECK(join.read_calls == 24U && join.eof_calls == 25U
	    && join.pager.key[0] == '\0' && join.queue_length == 0U
	    && join.source_length == 6U
	    && memcmp(join.source, "row 24", 6U) == 0);

	viewer_pager_initialize(&join, &stream, 24U, 0.0f, "NS", 0U);
	CHECK(yt_file_viewer_stream_run(&stream, &viewer_pager_stream_ops,
	    &join, NULL));
	CHECK(join.read_calls == 24U && join.eof_calls == 25U
	    && strcmp(join.pager.key, "NS") == 0
	    && join.pager.nonstop == 1.0f
	    && join.source_length == 6U
	    && memcmp(join.source, "row 24", 6U) == 0);

	viewer_pager_initialize(&join, &stream, 1U, 0.0f, "", 0U);
	join.presentation = state(true);
	join.presentation.foreground = 6.0f;
	CHECK(yt_file_viewer_stream_run(&stream, &viewer_pager_stream_ops,
	    &join, NULL));
	CHECK(join.capture.remote_length == sizeof(ansi_one_row) - 1U
	    && memcmp(join.capture.remote, ansi_one_row,
	    sizeof(ansi_one_row) - 1U) == 0);
	CHECK(join.presentation.foreground == 6.0f);
	CHECK(join.presentation.cached_foreground == 6.0f);
	CHECK(join.capture.last_local_foreground == 3);

	viewer_pager_initialize(&join, &stream, 1U, 0.0f, "", 0U);
	join.presentation.sound.mode = 2.0f;
	CHECK(yt_file_viewer_stream_run(&stream, &viewer_pager_stream_ops,
	    &join, NULL));
	CHECK(join.capture.remote_length == 2U
	    && memcmp(join.capture.remote, "\r\n", 2U) == 0);
	CHECK(join.presentation.foreground == 6.0f);
	CHECK(join.capture.last_local_foreground == 7);

	for (failure = 1U; failure <= YT_ARRAY_LEN(threshold_events);
	    ++failure) {
		viewer_pager_initialize(&join, &stream, 23U, 0.0f, "E", 0U);
		join.fail_at = 110U + failure;
		yt_error_clear(&error);
		CHECK(!yt_file_viewer_stream_run(&stream,
		    &viewer_pager_stream_ops, &join, &error));
		CHECK(error.status == YT_IO_ERROR
		    && join.event_count == 110U + failure
		    && memcmp(join.events + 110U, threshold_events,
		    failure * sizeof(threshold_events[0])) == 0
		    && stream.file_open && join.file_open
		    && stream.eof_checks == 23U && stream.read_count == 23U
		    && stream.line_count == 22U);
		for (index = 0U; index < 110U; ++index)
			CHECK(join.events[index]
			    == threshold_events[index % 5U]);
	}
}

static const char *const startup_ascii_lines[] = {
	"",
	"Welcome to .....",
	"",
	"Y A N K E E   T R A D E R ! ! !",
	"",
	"Coming to you compliments of your Sysop!",
	"",
	"Stacked commands are available in YT!",
	"EXAMPLE: M;7   (Move to sector 7)",
	"EXAMPLE: L;A;L (Land at planet, take all, leave planet)",
	"Command strings can be as l-o-n-g as you want!",
	"",
	"If something is in [brackets], it is a default and",
	"you can just hit enter. (Or use an extra \";\".)",
	"",
	"SAVE COMMANDS FOR RE-USE BY PUTTING A \"/\" AS LAST CHARACTER!",
	"RE-USE THEM WITH ONE [!] KEYSTROKE BY HITTING CTRL-R!!",
};

struct physical_viewer_join {
	struct viewer_pager_join join;
	struct yt_text_input input;
	const uint8_t *fixture;
	size_t fixture_length;
	const char *expected_path;
	size_t close_calls;
	size_t open_calls;
	bool force_missing;
};

static bool
physical_viewer_close(void *context, struct yt_error *error)
{
	struct physical_viewer_join *startup = context;
	bool ok;

	++startup->close_calls;
	ok = yt_text_input_close(&startup->input, error);
	if (ok)
		startup->join.file_open = false;
	return ok;
}

static bool
physical_viewer_open(void *context, const char *path, struct yt_error *error)
{
	struct physical_viewer_join *startup = context;
	bool ok;

	++startup->open_calls;
	if (startup->force_missing) {
		if (error != NULL) {
			error->status = YT_NOT_FOUND;
			(void)snprintf(error->operation,
			    sizeof(error->operation), "%s", "open input");
			(void)snprintf(error->path, sizeof(error->path), "%s", path);
		}
		return false;
	}
	if (startup->fixture == NULL)
		ok = yt_text_input_open(&startup->input, path, error);
	else {
		CHECK(startup->expected_path != NULL
		    && strcmp(path, startup->expected_path) == 0
		    && startup->input.file == NULL);
		if (startup->expected_path == NULL
		    || strcmp(path, startup->expected_path) != 0
		    || startup->input.file != NULL)
			return false;
		startup->input.file = tmpfile();
		ok = startup->input.file != NULL
		    && fwrite(startup->fixture, 1U, startup->fixture_length,
		    startup->input.file) == startup->fixture_length
		    && fseek(startup->input.file, 0L, SEEK_SET) == 0;
		if (ok)
			(void)snprintf(startup->input.path,
			    sizeof(startup->input.path), "%s", path);
		else if (startup->input.file != NULL) {
			(void)fclose(startup->input.file);
			startup->input.file = NULL;
		}
	}
	if (ok)
		startup->join.file_open = true;
	return ok;
}

static bool
physical_viewer_eof(void *context, bool *eof, struct yt_error *error)
{
	struct physical_viewer_join *startup = context;

	return yt_text_input_eof(&startup->input, eof, error);
}

static bool
physical_viewer_read(void *context, const uint8_t **line, size_t *length,
    bool *available, struct yt_error *error)
{
	struct physical_viewer_join *startup = context;
	bool ok = yt_text_input_read_line(&startup->input, line, length,
	    available, error);

	if (ok && *available)
		++startup->join.position;
	return ok;
}

static bool
physical_viewer_present(void *context, const uint8_t *text, size_t length,
    bool paged, struct yt_error *error)
{
	struct physical_viewer_join *startup = context;

	return viewer_pager_stream_present(&startup->join, text, length, paged,
	    error);
}

static const struct yt_file_viewer_stream_ops physical_viewer_ops = {
	physical_viewer_close,
	physical_viewer_open,
	physical_viewer_eof,
	physical_viewer_read,
	physical_viewer_present,
};

static bool
startup_ascii_append(uint8_t *output, size_t capacity, size_t *length,
    const void *data, size_t data_length)
{
	if (*length > capacity || data_length > capacity - *length)
		return false;
	if (data_length != 0U)
		memcpy(output + *length, data, data_length);
	*length += data_length;
	return true;
}

static size_t
startup_ascii_expected(uint8_t *output, size_t capacity, bool ansi,
    size_t body_rows)
{
	static const uint8_t prefix[] = "\r\nCntl-X to Stop\n\r\r\n";
	static const uint8_t body_color[] = "\x1b[0;32;40m";
	static const uint8_t restored_color[] = "\x1b[0;36;40m";
	static const uint8_t paged_end[] = "\n\r";
	static const uint8_t direct_end[] = "\r\n";
	size_t length = 0U;
	size_t index;

	CHECK(body_rows <= YT_ARRAY_LEN(startup_ascii_lines));
	if (body_rows > YT_ARRAY_LEN(startup_ascii_lines))
		return 0U;
	CHECK(startup_ascii_append(output, capacity, &length, prefix,
	    sizeof(prefix) - 1U));
	if (ansi)
		CHECK(startup_ascii_append(output, capacity, &length, body_color,
		    sizeof(body_color) - 1U));
	for (index = 0U; index < body_rows; ++index) {
		CHECK(startup_ascii_append(output, capacity, &length,
		    startup_ascii_lines[index],
		    strlen(startup_ascii_lines[index])));
		CHECK(startup_ascii_append(output, capacity, &length, paged_end,
		    sizeof(paged_end) - 1U));
	}
	if (ansi)
		CHECK(startup_ascii_append(output, capacity, &length,
		    restored_color, sizeof(restored_color) - 1U));
	CHECK(startup_ascii_append(output, capacity, &length, direct_end,
	    sizeof(direct_end) - 1U));
	return length;
}

static void
startup_ascii_initialize(struct physical_viewer_join *startup,
    struct yt_file_viewer_stream_state *stream, bool ansi, float mode,
    float snoop, size_t ctrl_x_row)
{
	viewer_pager_initialize(&startup->join, stream, 0U, 0.0f, "",
	    ctrl_x_row);
	startup->join.presentation = state(ansi);
	startup->join.presentation.sound.mode = mode;
	startup->join.presentation.sound.snoop = snoop;
	startup->join.presentation.foreground = 6.0f;
	startup->join.presentation.cached_foreground = ansi ? 6.0f : 0.0f;
	startup->join.pager.foreground = 6;
	startup->join.pager.nonstop = 1.0f;
	startup->join.accumulator[0] = '\0';
	startup->join.queue[0] = '\0';
	startup->join.queue_position = 0U;
	startup->join.queue_length = 0U;
	stream->path = YT_DATA_DIR "YTOPEN.ASC";
	yt_text_input_init(&startup->input);
}

static bool
physical_viewer_run(struct physical_viewer_join *startup,
    struct yt_file_viewer_stream_state *stream, struct yt_error *error)
{
	struct yt_present_result result;

	if (yt_present_line(NULL, 0U, &startup->join.presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(&startup->join, &result);
	if (!yt_file_viewer_entry(startup->join.pager.key,
	    &startup->join.pager.line_count, viewer_pager_stream_present,
	    &startup->join, error))
		return false;
	return yt_file_viewer_stream_run(stream, &physical_viewer_ops, startup,
	    error);
}

static void
startup_ascii_check_rows(const struct viewer_pager_join *join,
    size_t body_rows)
{
	size_t index;

	CHECK(join->local_row_count == body_rows + 4U
	    && join->local_lengths[0] == 0U
	    && join->local_lengths[1] == strlen("Cntl-X to Stop")
	    && memcmp(join->local_rows[1], "Cntl-X to Stop",
	    strlen("Cntl-X to Stop")) == 0
	    && join->local_lengths[2] == 0U);
	for (index = 0U; index < body_rows; ++index) {
		size_t length = strlen(startup_ascii_lines[index]);

		CHECK(join->local_lengths[index + 3U] == length
		    && memcmp(join->local_rows[index + 3U],
		    startup_ascii_lines[index], length) == 0);
	}
	CHECK(join->local_lengths[body_rows + 3U] == 0U);
}

static size_t
viewer_local_color_count(const struct viewer_pager_join *join,
    int foreground, int background)
{
	size_t count = 0U;
	size_t index;

	for (index = 0U; index < join->local_color_count; ++index) {
		if (join->local_foregrounds[index] == foreground
		    && join->local_backgrounds[index] == background)
			++count;
	}
	return count;
}

static void
test_startup_ascii_physical_join(void)
{
	struct physical_viewer_join startup;
	struct yt_file_viewer_stream_state stream;
	struct yt_error error;
	uint8_t expected[600];
	size_t expected_length;

	memset(&startup, 0, sizeof(startup));
	startup_ascii_initialize(&startup, &stream, true, 0.0f, -1.0f, 0U);
	yt_error_clear(&error);
	CHECK(physical_viewer_run(&startup, &stream, &error));
	expected_length = startup_ascii_expected(expected, sizeof(expected),
	    true, YT_ARRAY_LEN(startup_ascii_lines));
	CHECK(expected_length == 544U
	    && startup.join.capture.remote_length == expected_length
	    && memcmp(startup.join.capture.remote, expected,
	    expected_length) == 0);
	startup_ascii_check_rows(&startup.join,
	    YT_ARRAY_LEN(startup_ascii_lines));
	CHECK(!stream.file_open && !startup.join.file_open
	    && startup.input.file == NULL && startup.close_calls == 2U
	    && startup.open_calls == 1U && stream.eof_checks == 18U
	    && stream.key_checks == 18U && stream.read_count == 17U
	    && stream.line_count == 17U && startup.join.position == 17U
	    && startup.join.sample_calls == 18U
	    && startup.join.event_count == 90U
	    && startup.join.pager.line_count == 0.0f
	    && startup.join.pager.nonstop == 1.0f
	    && startup.join.pager.key[0] == '\0'
	    && startup.join.accumulator[0] == '\0'
	    && startup.join.queue_length == 0U
	    && startup.join.presentation.foreground == 6.0f
	    && startup.join.presentation.cached_foreground == 6.0f
	    && startup.join.capture.last_local_foreground == 3
	    && startup.join.local_color_count == 39U
	    && viewer_local_color_count(&startup.join, 2, 0) == 17U
	    && viewer_local_color_count(&startup.join, 3, 0) == 4U
	    && viewer_local_color_count(&startup.join, 7, 0) == 18U
	    && startup.join.source_length
	    == strlen(startup_ascii_lines[16])
	    && memcmp(startup.join.source, startup_ascii_lines[16],
	    startup.join.source_length) == 0);
	yt_text_input_destroy(&startup.input);

	memset(&startup, 0, sizeof(startup));
	startup_ascii_initialize(&startup, &stream, false, 0.0f, -1.0f, 0U);
	CHECK(physical_viewer_run(&startup, &stream, NULL));
	expected_length = startup_ascii_expected(expected, sizeof(expected),
	    false, YT_ARRAY_LEN(startup_ascii_lines));
	CHECK(expected_length == 524U
	    && startup.join.capture.remote_length == expected_length
	    && memcmp(startup.join.capture.remote, expected,
	    expected_length) == 0
	    && startup.join.local_color_count == 18U
	    && viewer_local_color_count(&startup.join, 7, 0) == 18U
	    && startup.join.capture.last_local_foreground == 7);
	startup_ascii_check_rows(&startup.join,
	    YT_ARRAY_LEN(startup_ascii_lines));
	yt_text_input_destroy(&startup.input);

	memset(&startup, 0, sizeof(startup));
	startup_ascii_initialize(&startup, &stream, true, 1.0f, -1.0f, 0U);
	CHECK(physical_viewer_run(&startup, &stream, NULL));
	CHECK(startup.join.capture.remote_length == 0U
	    && startup.join.sample_calls == 18U
	    && stream.read_count == 17U);
	startup_ascii_check_rows(&startup.join,
	    YT_ARRAY_LEN(startup_ascii_lines));
	yt_text_input_destroy(&startup.input);

	memset(&startup, 0, sizeof(startup));
	startup_ascii_initialize(&startup, &stream, true, 2.0f, 0.0f, 0U);
	CHECK(physical_viewer_run(&startup, &stream, NULL));
	CHECK(startup.join.capture.remote_length == 6U
	    && memcmp(startup.join.capture.remote,
	    "\r\n\r\n\r\n", 6U) == 0
	    && startup.join.local_row_count == 0U
	    && startup.join.sample_calls == 18U
	    && stream.read_count == 17U);
	yt_text_input_destroy(&startup.input);

	memset(&startup, 0, sizeof(startup));
	startup_ascii_initialize(&startup, &stream, true, 0.0f, -1.0f, 2U);
	(void)snprintf(startup.join.accumulator,
	    sizeof(startup.join.accumulator), "%s", "typed");
	(void)snprintf(startup.join.queue, sizeof(startup.join.queue), "%s",
	    "abc");
	startup.join.queue_length = 3U;
	CHECK(physical_viewer_run(&startup, &stream, NULL));
	expected_length = startup_ascii_expected(expected, sizeof(expected),
	    true, 2U);
	CHECK(startup.join.capture.remote_length == expected_length
	    && memcmp(startup.join.capture.remote, expected,
	    expected_length) == 0
	    && stream.eof_checks == 3U && stream.read_count == 2U
	    && stream.line_count == 2U && startup.join.position == 2U
	    && startup.join.sample_calls == 3U
	    && strcmp(startup.join.pager.key, "Q") == 0
	    && startup.join.accumulator[0] == '\0'
	    && startup.join.queue_length == 0U);
	startup_ascii_check_rows(&startup.join, 2U);
	yt_text_input_destroy(&startup.input);
}

static uint64_t
viewer_bytes_fnv1a64(const uint8_t *data, size_t length)
{
	uint64_t value = UINT64_C(14695981039346656037);
	size_t index;

	for (index = 0U; index < length; ++index) {
		value ^= data[index];
		value *= UINT64_C(1099511628211);
	}
	return value;
}

static uint64_t
viewer_rows_fnv1a64(const struct viewer_pager_join *join)
{
	uint64_t value = UINT64_C(14695981039346656037);
	size_t row;

	for (row = 0U; row < join->local_row_count; ++row) {
		size_t index;
		uint8_t low = (uint8_t)(join->local_lengths[row] & 0xffU);
		uint8_t high = (uint8_t)(join->local_lengths[row] >> 8U);

		value ^= low;
		value *= UINT64_C(1099511628211);
		value ^= high;
		value *= UINT64_C(1099511628211);
		for (index = 0U; index < join->local_lengths[row]; ++index) {
			value ^= join->local_rows[row][index];
			value *= UINT64_C(1099511628211);
		}
	}
	return value;
}

static uint64_t
viewer_colors_fnv1a64(const struct viewer_pager_join *join)
{
	uint64_t value = UINT64_C(14695981039346656037);
	size_t index;

	for (index = 0U; index < join->local_color_count; ++index) {
		value ^= (uint8_t)join->local_foregrounds[index];
		value *= UINT64_C(1099511628211);
		value ^= (uint8_t)join->local_backgrounds[index];
		value *= UINT64_C(1099511628211);
	}
	return value;
}

static void
instruction_viewer_initialize(struct physical_viewer_join *viewer,
    struct yt_file_viewer_stream_state *stream, int foreground, bool ansi,
    uint8_t *remote, size_t remote_capacity)
{
	viewer_pager_initialize(&viewer->join, stream, 0U, 0.0f, "", 0U);
	viewer->join.presentation = state(ansi);
	viewer->join.presentation.foreground = (float)foreground;
	viewer->join.presentation.cached_foreground =
	    ansi ? (float)foreground : 0.0f;
	viewer->join.pager.foreground = foreground;
	viewer->join.pager.nonstop = 0.0f;
	viewer->join.accumulator[0] = '\0';
	viewer->join.queue[0] = '\0';
	viewer->join.queue_position = 0U;
	viewer->join.queue_length = 0U;
	viewer->join.remote_output = remote;
	viewer->join.remote_capacity = remote_capacity;
	stream->path = YT_DATA_DIR "YTINSTR.DOC";
	stream->play.saved_foreground = (float)foreground;
	stream->play.saved_pager_foreground = foreground;
	yt_text_input_init(&viewer->input);
}

static void
test_instruction_physical_viewer_join(void)
{
	static const struct {
		int foreground;
		bool ansi;
		size_t remote_length;
		uint64_t remote_fnv;
		size_t color_count;
		size_t color_2;
		size_t color_5;
		size_t color_6;
		size_t color_7;
		size_t color_14;
		int final_local_foreground;
		float final_cached_foreground;
	} cases[] = {
		{2, true, 26713U, UINT64_C(0x42977bcf6e6d8fd2),
		    1158U, 546U, 0U, 23U, 566U, 23U, 2, 2.0f},
		{2, false, 25977U, UINT64_C(0x6dfc2dba3b88596e),
		    566U, 0U, 0U, 0U, 566U, 0U, 7, 0.0f},
		{5, true, 26733U, UINT64_C(0xab525e365be36563),
		    1158U, 542U, 4U, 23U, 566U, 23U, 5, 5.0f},
		{5, false, 25977U, UINT64_C(0x6dfc2dba3b88596e),
		    566U, 0U, 0U, 0U, 566U, 0U, 7, 0.0f},
	};
	static const uint8_t final_row[] =
	    "Door Distribution System Headquarters";
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	uint8_t remote[27000];
	size_t pass;

	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		memset(&viewer, 0, sizeof(viewer));
		instruction_viewer_initialize(&viewer, &stream,
		    cases[pass].foreground, cases[pass].ansi, remote,
		    sizeof(remote));
		CHECK(physical_viewer_run(&viewer, &stream, NULL));
		if (viewer.join.remote_length != cases[pass].remote_length
		    || viewer_bytes_fnv1a64(remote, viewer.join.remote_length)
		    != cases[pass].remote_fnv
		    || viewer.join.local_row_count != 569U
		    || viewer_rows_fnv1a64(&viewer.join)
		    != UINT64_C(0x2eff94178c21f61e)
		    || viewer.join.event_count != 2853U) {
			fprintf(stderr, "instruction pass %zu: remote=%zu/%llx "
			    "rows=%zu/%llx colors=%zu "
			    "c2=%zu c5=%zu c6=%zu c7=%zu c14=%zu "
			    "cache=%g local=%d samples=%zu responses=%zu "
			    "direct=%zu events=%zu\n", pass,
			    viewer.join.remote_length,
			    (unsigned long long)viewer_bytes_fnv1a64(remote,
			    viewer.join.remote_length), viewer.join.local_row_count,
			    (unsigned long long)viewer_rows_fnv1a64(&viewer.join),
			    viewer.join.local_color_count,
			    viewer_local_color_count(&viewer.join, 2, 0),
			    viewer_local_color_count(&viewer.join, 5, 0),
			    viewer_local_color_count(&viewer.join, 6, 0),
			    viewer_local_color_count(&viewer.join, 7, 0),
			    viewer_local_color_count(&viewer.join, 14, 0),
			    (double)viewer.join.presentation.cached_foreground,
			    viewer.join.capture.last_local_foreground,
			    viewer.join.sample_calls, viewer.join.response_calls,
			    viewer.join.direct_calls, viewer.join.event_count);
		}
		CHECK(viewer.join.remote_length == cases[pass].remote_length
		    && viewer_bytes_fnv1a64(remote, viewer.join.remote_length)
		    == cases[pass].remote_fnv);
		CHECK(viewer.join.local_row_count == 569U
		    && viewer_rows_fnv1a64(&viewer.join)
		    == UINT64_C(0x2eff94178c21f61e)
		    && viewer.join.local_fragment_length == 0U
		    && viewer.join.local_color_count == cases[pass].color_count
		    && viewer_local_color_count(&viewer.join, 2, 0)
		    == cases[pass].color_2
		    && viewer_local_color_count(&viewer.join, 5, 0)
		    == cases[pass].color_5
		    && viewer_local_color_count(&viewer.join, 6, 0)
		    == cases[pass].color_6
		    && viewer_local_color_count(&viewer.join, 7, 0)
		    == cases[pass].color_7
		    && viewer_local_color_count(&viewer.join, 14, 0)
		    == cases[pass].color_14
		    && viewer.join.capture.last_local_foreground
		    == cases[pass].final_local_foreground);
		CHECK(viewer.join.presentation.cached_foreground
		    == cases[pass].final_cached_foreground
		    && viewer.join.presentation.foreground
		    == (float)cases[pass].foreground
		    && viewer.join.pager.foreground == cases[pass].foreground
		    && viewer.join.pager.line_count == 0.0f
		    && viewer.join.pager.nonstop == 0.0f
		    && viewer.join.pager.key[0] == '\0'
		    && viewer.join.sample_calls == 566U
		    && viewer.join.response_calls == 23U
		    && viewer.join.direct_calls == 2U
		    && viewer.join.event_count == 2853U);
		CHECK(viewer.join.position == 542U
		    && stream.eof_checks == 543U
		    && stream.key_checks == 543U
		    && stream.read_count == 542U
		    && stream.line_count == 542U
		    && !stream.file_open && !viewer.join.file_open
		    && viewer.input.file == NULL && viewer.close_calls == 2U
		    && viewer.open_calls == 1U
		    && viewer.join.source_length == sizeof(final_row) - 1U
		    && memcmp(viewer.join.source, final_row,
		    sizeof(final_row) - 1U) == 0
		    && viewer.join.accumulator[0] == '\0'
		    && viewer.join.queue_length == 0U);
		yt_text_input_destroy(&viewer.input);
	}
}

static const uint8_t retained_current_news[] =
    "22:47:29 07-22-2026: Maintenance Program Ran (Revision 03/14/94)\r\n"
    "  -  The Wanderer is missing or has been destroyed!\r\n"
    "  -  The Wanderer regenerated with P.H.O.E.N.I.X. device!\r\n"
    "  -  The Xannor have made a Planet!\r\n"
    "The Xannor home base now has a planet!\r\n"
    "  -  Xannor report:\r\n"
    "Calculated Dynamic Xannor Regeneration is 0 fighters.\r\n"
    "  -  Mercenary Report:\r\n"
    "  -  The Mercenaries have built a home base using a captured "
    "Genesis Device!\r\n"
    "\x1a";

static const uint8_t retained_yesterday_news[] =
    "22:47:23 07-22-2026 **********************\r\n"
    "22:47:23 07-22-2026 **********************\r\n"
    "22:47:23 07-22-2026 **                  **\r\n"
    "22:47:23 07-22-2026 ** Game Initialized **\r\n"
    "22:47:23 07-22-2026 **                  **\r\n"
    "22:47:23 07-22-2026 **********************\r\n"
    "22:47:23 07-22-2026 **********************\r\n"
    "\x1a";

static void
fixture_viewer_initialize(struct physical_viewer_join *viewer,
    struct yt_file_viewer_stream_state *stream, const uint8_t *fixture,
    size_t fixture_length, const char *path, bool ansi, uint8_t *remote,
    size_t remote_capacity)
{
	viewer_pager_initialize(&viewer->join, stream, 0U, 0.0f, "", 0U);
	viewer->join.presentation = state(ansi);
	viewer->join.presentation.foreground = 1.0f;
	viewer->join.presentation.cached_foreground = ansi ? 1.0f : 0.0f;
	viewer->join.pager.foreground = 1;
	viewer->join.pager.nonstop = 0.0f;
	viewer->join.accumulator[0] = '\0';
	viewer->join.queue[0] = '\0';
	viewer->join.queue_position = 0U;
	viewer->join.queue_length = 0U;
	viewer->join.remote_output = remote;
	viewer->join.remote_capacity = remote_capacity;
	viewer->fixture = fixture;
	viewer->fixture_length = fixture_length;
	viewer->expected_path = path;
	stream->path = path;
	stream->play.saved_foreground = 1.0f;
	stream->play.saved_pager_foreground = 1;
	yt_text_input_init(&viewer->input);
}

static bool
newspaper_viewer_typed_run(struct physical_viewer_join *viewer,
    struct yt_file_viewer_stream_state *stream, const uint8_t *typed,
    size_t typed_length)
{
	static const uint8_t prompt[] =
	    "Do you want to read [T]oday's or [Y]esterday's news? [T/Y] -=> ";
	struct yt_present_result result;
	size_t accepted_length;

	if ((typed == NULL && typed_length != 0U)
	    || typed_length >= sizeof(viewer->join.accumulator))
		return false;
	if (yt_present_line(NULL, 0U, &viewer->join.presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(&viewer->join, &result);
	viewer->join.pager.newline_flag = 1.0f;
	if (!yt_paged_row_run(&viewer->join.pager,
	    &viewer->join.presentation, &viewer->join.key_state, prompt,
	    sizeof(prompt) - 1U, &viewer_pager_ops, &viewer->join))
		return false;
	yt_pager_editor_enter(&viewer->join.pager, viewer->join.accumulator,
	    sizeof(viewer->join.accumulator));
	if (typed_length != 0U)
		memcpy(viewer->join.accumulator, typed, typed_length);
	viewer->join.accumulator[typed_length] = '\0';
	if (!yt_input_split_semicolon(viewer->join.accumulator,
	    viewer->join.queue, sizeof(viewer->join.queue),
	    &viewer->join.queue_position, &viewer->join.queue_length))
		return false;
	accepted_length = strlen(viewer->join.accumulator);
	if (accepted_length != 1U
	    || (viewer->join.accumulator[0] != 'T'
	    && viewer->join.accumulator[0] != 't'
	    && viewer->join.accumulator[0] != 'Y'
	    && viewer->join.accumulator[0] != 'y'))
		return false;
	if (yt_present_editor_echo(typed, typed_length, typed, typed_length,
	    &viewer->join.presentation, &result) != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(&viewer->join, &result);
	if (yt_present_line(NULL, 0U, &viewer->join.presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(&viewer->join, &result);
	return physical_viewer_run(viewer, stream, NULL);
}

static bool
newspaper_viewer_run(struct physical_viewer_join *viewer,
    struct yt_file_viewer_stream_state *stream, uint8_t response)
{
	return newspaper_viewer_typed_run(viewer, stream, &response, 1U);
}

static void
test_newspaper_physical_viewer_join(void)
{
	static const uint8_t today_final[] =
	    "  -  The Mercenaries have built a home base using a captured "
	    "Genesis Device!";
	static const uint8_t yesterday_final[] =
	    "22:47:23 07-22-2026 **********************";
	static const struct {
		uint8_t response;
		bool ansi;
		const uint8_t *fixture;
		size_t fixture_length;
		uint64_t fixture_fnv;
		const char *path;
		size_t lines;
		size_t remote_length;
		uint64_t remote_fnv;
		size_t local_rows;
		uint64_t local_fnv;
		size_t color_count;
		size_t color_2;
		size_t color_4;
		size_t color_7;
		size_t color_14;
		int final_local_foreground;
		float final_cached_foreground;
		float final_bold;
		const uint8_t *final_row;
		size_t final_length;
	} cases[] = {
		{'T', true, retained_current_news,
		    sizeof(retained_current_news) - 1U,
		    UINT64_C(0xfb0c273ced751bf2), "ytnews.dat", 9U,
		    635U, UINT64_C(0xac361e7754a450cb), 15U,
		    UINT64_C(0xcad0a9fb8ef9163e), 27U, 3U, 7U, 11U,
		    6U, 4, 1.0f, 0.0f, today_final,
		    sizeof(today_final) - 1U},
		{'T', false, retained_current_news,
		    sizeof(retained_current_news) - 1U,
		    UINT64_C(0xfb0c273ced751bf2), "ytnews.dat", 9U,
		    523U, UINT64_C(0x94346bf3d8d4e0de), 15U,
		    UINT64_C(0xcad0a9fb8ef9163e), 11U, 0U, 0U, 11U,
		    0U, 7, 0.0f, 1.0f, today_final,
		    sizeof(today_final) - 1U},
		{'Y', true, retained_yesterday_news,
		    sizeof(retained_yesterday_news) - 1U,
		    UINT64_C(0x05a68e56016562c5), "YTYNEWS.DAT", 7U,
		    418U, UINT64_C(0x9a9613242a316ba3), 13U,
		    UINT64_C(0x89bb3b2737cdd9d9), 23U, 7U, 7U, 9U,
		    0U, 4, 1.0f, 0.0f, yesterday_final,
		    sizeof(yesterday_final) - 1U},
		{'Y', false, retained_yesterday_news,
		    sizeof(retained_yesterday_news) - 1U,
		    UINT64_C(0x05a68e56016562c5), "YTYNEWS.DAT", 7U,
		    398U, UINT64_C(0xe3aacbb5cc2624f2), 13U,
		    UINT64_C(0x89bb3b2737cdd9d9), 9U, 0U, 0U, 9U,
		    0U, 7, 0.0f, 0.0f, yesterday_final,
		    sizeof(yesterday_final) - 1U},
	};
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	uint8_t remote[800];
	size_t pass;

	CHECK(sizeof(retained_current_news) - 1U == 434U
	    && sizeof(retained_yesterday_news) - 1U == 309U);
	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		memset(&viewer, 0, sizeof(viewer));
		CHECK(viewer_bytes_fnv1a64(cases[pass].fixture,
		    cases[pass].fixture_length) == cases[pass].fixture_fnv);
		fixture_viewer_initialize(&viewer, &stream,
		    cases[pass].fixture, cases[pass].fixture_length,
		    cases[pass].path, cases[pass].ansi, remote,
		    sizeof(remote));
		CHECK(newspaper_viewer_run(&viewer, &stream,
		    cases[pass].response));
		CHECK(viewer.join.remote_length == cases[pass].remote_length
		    && viewer_bytes_fnv1a64(remote, viewer.join.remote_length)
		    == cases[pass].remote_fnv);
		CHECK(viewer.join.local_row_count == cases[pass].local_rows
		    && viewer_rows_fnv1a64(&viewer.join)
		    == cases[pass].local_fnv
		    && viewer.join.local_fragment_length == 0U
		    && viewer.join.local_color_count == cases[pass].color_count
		    && viewer_local_color_count(&viewer.join, 2, 0)
		    == cases[pass].color_2
		    && viewer_local_color_count(&viewer.join, 4, 0)
		    == cases[pass].color_4
		    && viewer_local_color_count(&viewer.join, 7, 0)
		    == cases[pass].color_7
		    && viewer_local_color_count(&viewer.join, 14, 0)
		    == cases[pass].color_14
		    && viewer.join.capture.last_local_foreground
		    == cases[pass].final_local_foreground);
		CHECK(viewer.join.presentation.foreground == 1.0f
		    && viewer.join.presentation.cached_foreground
		    == cases[pass].final_cached_foreground
		    && viewer.join.presentation.bold == cases[pass].final_bold
		    && viewer.join.presentation.blink == 0.0f
		    && viewer.join.pager.foreground == 1
		    && viewer.join.pager.line_count == 0.0f
		    && viewer.join.pager.nonstop == 0.0f
		    && viewer.join.pager.key[0] == '\0');
		CHECK(viewer.join.position == cases[pass].lines
		    && stream.eof_checks == cases[pass].lines + 1U
		    && stream.key_checks == cases[pass].lines + 1U
		    && stream.read_count == cases[pass].lines
		    && stream.line_count == cases[pass].lines
		    && viewer.join.sample_calls == cases[pass].lines + 2U
		    && viewer.join.response_calls == 0U
		    && viewer.join.direct_calls == 2U
		    && viewer.join.event_count == 5U * (cases[pass].lines + 2U)
		    && !stream.file_open && !viewer.join.file_open
		    && viewer.input.file == NULL && viewer.close_calls == 2U
		    && viewer.open_calls == 1U
		    && viewer.join.source_length == cases[pass].final_length
		    && memcmp(viewer.join.source, cases[pass].final_row,
		    cases[pass].final_length) == 0
		    && viewer.join.accumulator[0]
		    == (char)cases[pass].response
		    && viewer.join.queue_length == 0U);
		yt_text_input_destroy(&viewer.input);
	}
}

static void
test_newspaper_endpoint_modes(void)
{
	static const uint8_t corrupt[] =
	    "\r\n\r\n\r\n\r\n\r\n";
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	uint8_t remote[32];

	memset(&viewer, 0, sizeof(viewer));
	fixture_viewer_initialize(&viewer, &stream,
	    retained_current_news, sizeof(retained_current_news) - 1U,
	    "ytnews.dat", true, remote, sizeof(remote));
	viewer.join.presentation.sound.mode = 1.0f;
	CHECK(newspaper_viewer_run(&viewer, &stream, 'T')
	    && viewer.join.remote_length == 0U);
	yt_text_input_destroy(&viewer.input);

	memset(&viewer, 0, sizeof(viewer));
	fixture_viewer_initialize(&viewer, &stream,
	    retained_current_news, sizeof(retained_current_news) - 1U,
	    "ytnews.dat", true, remote, sizeof(remote));
	viewer.join.presentation.sound.mode = 2.0f;
	CHECK(newspaper_viewer_run(&viewer, &stream, 'T')
	    && viewer.join.remote_length == sizeof(corrupt) - 1U
	    && memcmp(remote, corrupt, sizeof(corrupt) - 1U) == 0);
	yt_text_input_destroy(&viewer.input);
}

static void
test_newspaper_pagination_and_ctrl_x(void)
{
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	uint8_t long_news[256];
	uint8_t remote[512];
	size_t position = 0U;
	size_t row;

	for (row = 1U; row <= 24U; ++row) {
		int written = snprintf((char *)long_news + position,
		    sizeof(long_news) - position, "L%02zu\r\n", row);

		CHECK(written == 5);
		if (written != 5)
			return;
		position += (size_t)written;
	}
	long_news[position++] = 0x1aU;

	memset(&viewer, 0, sizeof(viewer));
	fixture_viewer_initialize(&viewer, &stream, long_news, position,
	    "ytnews.dat", true, remote, sizeof(remote));
	(void)snprintf(viewer.join.response, sizeof(viewer.join.response),
	    "%s", "E");
	CHECK(newspaper_viewer_run(&viewer, &stream, 'T')
	    && viewer.join.position == 23U
	    && stream.read_count == 23U && stream.line_count == 23U
	    && viewer.join.response_calls == 1U
	    && strcmp(viewer.join.pager.key, "Q") == 0
	    && viewer.join.source_length == 1U
	    && viewer.join.source[0] == '\r');
	yt_text_input_destroy(&viewer.input);

	memset(&viewer, 0, sizeof(viewer));
	fixture_viewer_initialize(&viewer, &stream,
	    retained_current_news, sizeof(retained_current_news) - 1U,
	    "ytnews.dat", true, remote, sizeof(remote));
	viewer.join.injected_sample_call = 2U;
	viewer.join.injected_sample.bytes[0] = 0x18U;
	viewer.join.injected_sample.length = 1U;
	CHECK(newspaper_viewer_run(&viewer, &stream, 'T')
	    && viewer.join.injected_sample_hits == 1U
	    && viewer.join.position == 0U && stream.read_count == 0U
	    && strcmp(viewer.join.pager.key, "Q") == 0
	    && viewer.join.source_length == sizeof("Cntl-X to Stop") - 1U
	    && memcmp(viewer.join.source, "Cntl-X to Stop",
	    sizeof("Cntl-X to Stop") - 1U) == 0);
	yt_text_input_destroy(&viewer.input);
}

static const uint8_t retained_scoreboard[] =
    "\r\n"
    "Y a n k e e   T r a d e r   S c o r e b o a r d\r\n"
    "\r\n"
    "Last updated at: 07-22-2026 22:47:35\r\n"
    "\r\n"
    "Rank  Rank%        Score        Team   Ports   Player\r\n"
    "==== ======= ================= ====== ======= "
    "================================\r\n"
    "\r\n"
    "T e a m   R a n k i n g s\r\n"
    "\r\n"
    "Rank  Rank%        Score        Team   Team Name\r\n"
    "==== ======= ================= ====== "
    "========================================\r\n"
    "\r\n"
    "N o n  -  H u m a n   P l a y e r s\r\n"
    "\r\n"
    "    The Xannor       Rank%     The Mercenaries   Rank%\r\n"
    "================== =========  ================= =======\r\n"
    "        1,000,000   100.00%                  0    0.00%\r\n"
    "\r\n"
    "\x1a";

static bool
scoreboard_viewer_run(struct physical_viewer_join *viewer,
    struct yt_file_viewer_stream_state *stream, const uint8_t *response,
    size_t response_length, bool updated)
{
	static const uint8_t prompt[] =
	    "Enter 'O' to see OLD scoreboard or press [ENTER] for UPDATED "
	    "one. -=>";
	static const uint8_t heading[] = "P l a y e r  R a n k i n g s";
	static const uint8_t dot[] = ".";
	struct yt_present_result result;
	size_t index;

	if (yt_present_line(NULL, 0U, &viewer->join.presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(&viewer->join, &result);
	viewer->join.pager.newline_flag = 1.0f;
	if (!yt_paged_row_run(&viewer->join.pager,
	    &viewer->join.presentation, &viewer->join.key_state, prompt,
	    sizeof(prompt) - 1U, &viewer_pager_ops, &viewer->join))
		return false;
	yt_pager_editor_enter(&viewer->join.pager, viewer->join.accumulator,
	    sizeof(viewer->join.accumulator));
	if (response_length >= sizeof(viewer->join.accumulator))
		return false;
	if (response_length != 0U) {
		memcpy(viewer->join.accumulator, response, response_length);
		if (yt_present_editor_echo(response, response_length, response,
		    response_length, &viewer->join.presentation, &result)
		    != YT_PRESENT_OK)
			return false;
		viewer_pager_capture_result(&viewer->join, &result);
	}
	viewer->join.accumulator[response_length] = '\0';
	if (yt_present_line(NULL, 0U, &viewer->join.presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(&viewer->join, &result);
	viewer->join.pager.line_count = 0.0f;
	if (yt_present_line(NULL, 0U, &viewer->join.presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(&viewer->join, &result);
	if (updated) {
		viewer->join.pager.newline_flag = 1.0f;
		if (!yt_paged_row_run(&viewer->join.pager,
		    &viewer->join.presentation, &viewer->join.key_state, heading,
		    sizeof(heading) - 1U, &viewer_pager_ops, &viewer->join))
			return false;
		for (index = 0U; index < 4U; ++index) {
			if (yt_present_character(dot, sizeof(dot) - 1U,
			    &viewer->join.presentation, &result) != YT_PRESENT_OK)
				return false;
			viewer_pager_capture_result(&viewer->join, &result);
		}
		if (yt_present_line(NULL, 0U, &viewer->join.presentation,
		    &result) != YT_PRESENT_OK)
			return false;
		viewer_pager_capture_result(&viewer->join, &result);
	}
	return physical_viewer_run(viewer, stream, NULL);
}

static void
test_scoreboard_physical_viewer_join(void)
{
	static const uint8_t old[] = "O";
	static const uint8_t non_old[] = "X";
	static const struct {
		const uint8_t *response;
		size_t response_length;
		bool updated;
		bool ansi;
		size_t remote_length;
		uint64_t remote_fnv;
		size_t local_rows;
		uint64_t local_fnv;
		size_t color_count;
		size_t color_2;
		size_t color_4;
		size_t color_7;
		int final_local_foreground;
	} cases[] = {
		{old, sizeof(old) - 1U, false, true, 720U,
		    UINT64_C(0x3643e0163f6e5e61), 26U,
		    UINT64_C(0x373d597c7fe94fec), 48U, 19U, 8U, 21U, 4},
		{old, sizeof(old) - 1U, false, false, 700U,
		    UINT64_C(0x9511e83e029c05b0), 26U,
		    UINT64_C(0x373d597c7fe94fec), 21U, 0U, 0U, 21U, 7},
		{NULL, 0U, true, true, 753U,
		    UINT64_C(0xd77d6159416ea73f), 27U,
		    UINT64_C(0x2be53f5d3ddc2bc6), 55U, 19U, 14U, 22U, 4},
		{NULL, 0U, true, false, 733U,
		    UINT64_C(0x6fb5e2607dc47132), 27U,
		    UINT64_C(0x2be53f5d3ddc2bc6), 22U, 0U, 0U, 22U, 7},
		{non_old, sizeof(non_old) - 1U, true, true, 754U,
		    UINT64_C(0xbedfd0fdcf2a9c4f), 27U,
		    UINT64_C(0xd5ee0fa188f736cd), 55U, 19U, 14U, 22U, 4},
		{non_old, sizeof(non_old) - 1U, true, false, 734U,
		    UINT64_C(0x2266f5f875126782), 27U,
		    UINT64_C(0xd5ee0fa188f736cd), 22U, 0U, 0U, 22U, 7},
	};
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	uint8_t remote[800];
	size_t pass;

	CHECK(sizeof(retained_scoreboard) - 1U == 603U
	    && viewer_bytes_fnv1a64(retained_scoreboard,
	    sizeof(retained_scoreboard) - 1U)
	    == UINT64_C(0x27e10c0bc2db3e4f));
	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		size_t b05d_calls = cases[pass].updated ? 22U : 21U;

		memset(&viewer, 0, sizeof(viewer));
		fixture_viewer_initialize(&viewer, &stream,
		    retained_scoreboard, sizeof(retained_scoreboard) - 1U,
		    "YTSCORE.ASC", cases[pass].ansi, remote, sizeof(remote));
		CHECK(scoreboard_viewer_run(&viewer, &stream,
		    cases[pass].response, cases[pass].response_length,
		    cases[pass].updated));
		CHECK(viewer.join.remote_length == cases[pass].remote_length
		    && viewer_bytes_fnv1a64(remote, viewer.join.remote_length)
		    == cases[pass].remote_fnv);
		CHECK(viewer.join.local_row_count == cases[pass].local_rows
		    && viewer_rows_fnv1a64(&viewer.join)
		    == cases[pass].local_fnv
		    && viewer.join.local_fragment_length == 0U
		    && viewer.join.local_color_count == cases[pass].color_count
		    && viewer_local_color_count(&viewer.join, 2, 0)
		    == cases[pass].color_2
		    && viewer_local_color_count(&viewer.join, 4, 0)
		    == cases[pass].color_4
		    && viewer_local_color_count(&viewer.join, 7, 0)
		    == cases[pass].color_7
		    && viewer.join.capture.last_local_foreground
		    == cases[pass].final_local_foreground);
		CHECK(viewer.join.presentation.foreground == 1.0f
		    && viewer.join.presentation.cached_foreground
		    == (cases[pass].ansi ? 1.0f : 0.0f)
		    && viewer.join.presentation.bold == 0.0f
		    && viewer.join.presentation.blink == 0.0f
		    && viewer.join.pager.foreground == 1
		    && viewer.join.pager.line_count == 0.0f
		    && viewer.join.pager.nonstop == 0.0f
		    && viewer.join.pager.key[0] == '\0');
		CHECK(viewer.join.position == 19U
		    && stream.eof_checks == 20U && stream.key_checks == 20U
		    && stream.read_count == 19U && stream.line_count == 19U
		    && viewer.join.sample_calls == b05d_calls
		    && viewer.join.response_calls == 0U
		    && viewer.join.direct_calls == 2U
		    && viewer.join.event_count == 5U * b05d_calls
		    && !stream.file_open && !viewer.join.file_open
		    && viewer.input.file == NULL && viewer.close_calls == 2U
		    && viewer.open_calls == 1U
		    && viewer.join.source_length == 0U
		    && strlen(viewer.join.accumulator)
		    == cases[pass].response_length
		    && (cases[pass].response_length == 0U
		    || memcmp(viewer.join.accumulator, cases[pass].response,
		    cases[pass].response_length) == 0)
		    && viewer.join.queue_length == 0U);
		yt_text_input_destroy(&viewer.input);
	}
}

static void
test_scoreboard_endpoint_modes(void)
{
	static const uint8_t old[] = "O";
	static const uint8_t corrupt_old[] =
	    "\r\n\r\n\r\n\r\n\r\n\r\n";
	static const uint8_t corrupt_updated[] =
	    "\r\n\r\n\r\n....\r\n\r\n\r\n\r\n";
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	uint8_t remote[64];

	memset(&viewer, 0, sizeof(viewer));
	fixture_viewer_initialize(&viewer, &stream,
	    retained_scoreboard, sizeof(retained_scoreboard) - 1U,
	    "YTSCORE.ASC", true, remote, sizeof(remote));
	viewer.join.presentation.sound.mode = 1.0f;
	CHECK(scoreboard_viewer_run(&viewer, &stream, NULL, 0U, true)
	    && viewer.join.remote_length == 0U);
	yt_text_input_destroy(&viewer.input);

	memset(&viewer, 0, sizeof(viewer));
	fixture_viewer_initialize(&viewer, &stream,
	    retained_scoreboard, sizeof(retained_scoreboard) - 1U,
	    "YTSCORE.ASC", true, remote, sizeof(remote));
	viewer.join.presentation.sound.mode = 2.0f;
	CHECK(scoreboard_viewer_run(&viewer, &stream, old,
	    sizeof(old) - 1U, false)
	    && viewer.join.remote_length == sizeof(corrupt_old) - 1U
	    && memcmp(remote, corrupt_old, sizeof(corrupt_old) - 1U) == 0);
	yt_text_input_destroy(&viewer.input);

	memset(&viewer, 0, sizeof(viewer));
	fixture_viewer_initialize(&viewer, &stream,
	    retained_scoreboard, sizeof(retained_scoreboard) - 1U,
	    "YTSCORE.ASC", true, remote, sizeof(remote));
	viewer.join.presentation.sound.mode = 2.0f;
	CHECK(scoreboard_viewer_run(&viewer, &stream, NULL, 0U, true)
	    && viewer.join.remote_length == sizeof(corrupt_updated) - 1U
	    && memcmp(remote, corrupt_updated,
	    sizeof(corrupt_updated) - 1U) == 0);
	yt_text_input_destroy(&viewer.input);
}

static void
test_normal_exit_scoreboard_viewer_join(void)
{
	static const struct {
		bool ansi;
		size_t remote_length;
		uint64_t remote_fnv;
		size_t color_count;
		size_t color_2;
		size_t color_4;
		size_t color_7;
		int final_local_foreground;
		float final_bold;
	} cases[] = {
		{true, 644U, UINT64_C(0x69a557fced8584c9),
		    43U, 19U, 4U, 20U, 4, 0.0f},
		{false, 624U, UINT64_C(0xec9a3c8421b271f8),
		    20U, 0U, 0U, 20U, 7, 1.0f},
	};
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	uint8_t remote[700];
	size_t pass;

	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		memset(&viewer, 0, sizeof(viewer));
		fixture_viewer_initialize(&viewer, &stream,
		    retained_scoreboard, sizeof(retained_scoreboard) - 1U,
		    "YTSCORE.ASC", cases[pass].ansi, remote, sizeof(remote));
		viewer.join.presentation.bold = cases[pass].ansi ? 0.0f : 1.0f;
		viewer.join.pager.line_count = 2.0f;
		viewer.join.pager.nonstop = 1.0f;
		CHECK(physical_viewer_run(&viewer, &stream, NULL));
		CHECK(viewer.join.remote_length == cases[pass].remote_length
		    && viewer_bytes_fnv1a64(remote, viewer.join.remote_length)
		    == cases[pass].remote_fnv);
		CHECK(viewer.join.local_row_count == 23U
		    && viewer_rows_fnv1a64(&viewer.join)
		    == UINT64_C(0xab40768a3e8feefd)
		    && viewer.join.local_fragment_length == 0U
		    && viewer.join.local_color_count == cases[pass].color_count
		    && viewer_local_color_count(&viewer.join, 2, 0)
		    == cases[pass].color_2
		    && viewer_local_color_count(&viewer.join, 4, 0)
		    == cases[pass].color_4
		    && viewer_local_color_count(&viewer.join, 7, 0)
		    == cases[pass].color_7
		    && viewer.join.capture.last_local_foreground
		    == cases[pass].final_local_foreground);
		CHECK(viewer.join.first_finish_line_count == 2.0f
		    && viewer.join.presentation.foreground == 1.0f
		    && viewer.join.presentation.cached_foreground
		    == (cases[pass].ansi ? 1.0f : 0.0f)
		    && viewer.join.presentation.bold == cases[pass].final_bold
		    && viewer.join.presentation.blink == 0.0f
		    && viewer.join.pager.foreground == 1
		    && viewer.join.pager.line_count == 0.0f
		    && viewer.join.pager.nonstop == 1.0f
		    && viewer.join.pager.key[0] == '\0');
		CHECK(viewer.join.position == 19U
		    && stream.eof_checks == 20U && stream.key_checks == 20U
		    && stream.read_count == 19U && stream.line_count == 19U
		    && viewer.join.sample_calls == 20U
		    && viewer.join.response_calls == 0U
		    && viewer.join.direct_calls == 2U
		    && viewer.join.event_count == 100U
		    && !stream.file_open && !viewer.join.file_open
		    && viewer.input.file == NULL && viewer.close_calls == 2U
		    && viewer.open_calls == 1U
		    && viewer.join.source_length == 0U
		    && viewer.join.accumulator[0] == '\0'
		    && viewer.join.queue_length == 0U);
		yt_text_input_destroy(&viewer.input);
	}
}

static void
test_sector_private_pager(void)
{
	struct yt_sector_pager_state pager;

	yt_sector_pager_begin(&pager);
	CHECK(pager.line_count == 3.0f);
	yt_sector_pager_add(&pager, 12.0f);
	CHECK(!yt_sector_pager_finish_sector(&pager)
	    && pager.line_count == 15.0f);
	yt_sector_pager_add(&pager, 1.0f);
	CHECK(yt_sector_pager_finish_sector(&pager)
	    && pager.line_count == 0.0f);
	yt_sector_pager_add(&pager, 10.0f);
	CHECK(!yt_sector_pager_finish_sector(&pager)
	    && pager.line_count == 10.0f);
	yt_sector_pager_add(&pager, 6.0f);
	CHECK(yt_sector_pager_finish_sector(&pager)
	    && pager.line_count == 0.0f);
}

static void
test_sector_scanner_rows(void)
{
	static const uint8_t raw_name[] = {'A', 0, 'B'};
	static const uint8_t raw_team_name[] = {'R', 0, 'V'};
	static const uint8_t mine_expected[] =
	    "** WARNING! SECTOR HAS 3 MINES! **";
	static const uint8_t port_equ_expected[] = {
		'P', 'o', 'r', 't', ':', ' ', 'A', 0, 'B',
		',', ' ', 'S', 'e', 'l', 'l', 'i', 'n', 'g', ':', ' ',
		'E', 'q', 'u'
	};
	static const uint8_t planet_expected[] = {
		'P', 'l', 'a', 'n', 'e', 't', ':', ' ', 'A', 0, 'B',
		' ', '*', ' ', 'F', 'o', 'r', 'c', 'e', 's', ':', '-', '2'
	};
	static const uint8_t player_expected[] = {
		' ', ' ', ' ', ' ', 'A', 0, 'B',
		' ', '-', ' ', 'T', 'e', 'a', 'm', ':', ' ', '4',
		' ', '-', ' ', 'F', 'i', 'g', 'h', 't', 'e', 'r', 's', ':',
		' ', '1', '2', '0', '0',
		' ', '-', ' ', 'S', 'h', 'i', 'e', 'l', 'd', 's', ':',
		' ', '8', '0'
	};
	static const uint8_t owner_expected[] = {
		' ', '1', '2', '3', ' ', '(', 'B', 'e', 'l', 'o', 'n', 'g',
		' ', 't', 'o', ' ', 'A', 0, 'B', ' ', 'T', 'e', 'a', 'm',
		' ', '[', '4', ']', ' ', '[', 'R', 0, 'V', ']', ')'
	};
	static const uint8_t scratch_expected[] = {
		'A', 0, 'B', ' ', 'T', 'e', 'a', 'm', ' ', '[', '4', ']',
		' ', '[', 'R', 0, 'V', ']'
	};
	static const uint8_t xannor_expected[] =
	    " 123 (Belong to The Xannor)";
	static const uint8_t mercenary_expected[] =
	    " 123 (Belong to Mercenaries)";
	static const uint8_t self_expected[] = " 123 (Belong to YOU)";
	struct yt_record record;
	struct yt_port port;
	struct yt_planet planet;
	struct yt_player player;
	struct yt_sector sector;
	struct yt_sector overlay;
	struct yt_error error;
	uint8_t row[256];
	uint8_t scratch[160] = {'k', 'e', 'e', 'p'};
	float caller_warps[6] = {9.0f, 0.0f, 9.0f, 42.0f, 0.0f, 7.0f};
	float targets[6] = {0};
	size_t length;
	size_t scratch_length = 4U;
	bool changed;

	yt_error_clear(&error);
	CHECK(yt_sector_mine_warning_row(3.0f, row, sizeof(row), &length)
	    && length == sizeof(mine_expected) - 1U
	    && memcmp(row, mine_expected, length) == 0);
	CHECK(!yt_sector_candidate_eligible(2, 2, 42.0f, 42.0f)
	    && !yt_sector_candidate_eligible(3, 2, 41.0f, 42.0f)
	    && yt_sector_candidate_eligible(3, 2, 42.0f, 42.0f));
	CHECK(!yt_sector_cloak_revealed(0.9f, 0.0f)
	    && !yt_sector_cloak_revealed(0.5f, 0.5f)
	    && yt_sector_cloak_revealed(0.5001f, 0.5f));
	CHECK(yt_sector_sensor_targets(caller_warps, targets) == 4U
	    && targets[0] == 9.0f && targets[1] == 9.0f
	    && targets[2] == 42.0f && targets[3] == 7.0f);

	yt_record_blank(&record);
	memcpy(record.bytes, raw_name, sizeof(raw_name));
	yt_record_set_number(&record, YT_F41, 1.0f);
	yt_record_set_number(&record, YT_F85, 3.0f);
	yt_port_decode(&port, &record);
	CHECK(yt_sector_port_row(&port, row, sizeof(row), &length, &error)
	    && length == sizeof(port_equ_expected)
	    && memcmp(row, port_equ_expected, length) == 0);
	port.commodity_class = 2.0f;
	CHECK(yt_sector_port_row(&port, row, sizeof(row), &length, &error)
	    && memcmp(row + length - 3U, "Org", 3U) == 0);
	port.commodity_class = -7.0f;
	CHECK(yt_sector_port_row(&port, row, sizeof(row), &length, &error)
	    && memcmp(row + length - 3U, "Ore", 3U) == 0);
	CHECK(!yt_sector_port_row(&port, row, length - 1U, &length, &error));

	yt_record_blank(&record);
	memcpy(record.bytes, raw_name, sizeof(raw_name));
	yt_record_set_number(&record, YT_F77, -1.25f);
	yt_record_set_number(&record, YT_F85, 3.0f);
	yt_planet_decode(&planet, &record);
	CHECK(yt_sector_planet_row(&planet, row, sizeof(row), &length, &error)
	    && length == sizeof(planet_expected)
	    && memcmp(row, planet_expected, length) == 0);

	yt_record_blank(&record);
	memcpy(record.bytes, raw_name, sizeof(raw_name));
	yt_record_set_number(&record, YT_F53, 80.0f);
	yt_record_set_number(&record, YT_F61, 1200.0f);
	yt_record_set_number(&record, YT_F85, 3.0f);
	yt_record_set_number(&record, YT_F89, 4.0f);
	yt_player_decode(&player, &record);
	CHECK(yt_sector_player_row(&player, row, sizeof(row), &length, &error)
	    && length == sizeof(player_expected)
	    && memcmp(row, player_expected, length) == 0);

	memset(&sector, 0, sizeof(sector));
	sector.fighters = 123.0f;
	sector.fighter_owner = 3.0f;
	player.team = -4.0f;
	yt_record_blank(&record);
	memcpy(record.bytes, raw_team_name, sizeof(raw_team_name));
	yt_record_set_number(&record, YT_F73, 3.0f);
	yt_sector_decode(&overlay, &record);
	CHECK(yt_sector_fighter_row(&sector, 2, &player, &overlay,
	    row, sizeof(row), &length, scratch, sizeof(scratch),
	    &scratch_length, &changed, &error)
	    && changed && length == sizeof(owner_expected)
	    && memcmp(row, owner_expected, length) == 0
	    && scratch_length == sizeof(scratch_expected)
	    && memcmp(scratch, scratch_expected, scratch_length) == 0);

	sector.fighter_owner = -1.0f;
	CHECK(yt_sector_fighter_row(&sector, 2, NULL, NULL,
	    row, sizeof(row), &length, scratch, sizeof(scratch),
	    &scratch_length, &changed, &error)
	    && changed && length == sizeof(xannor_expected) - 1U
	    && memcmp(row, xannor_expected, length) == 0
	    && scratch_length == strlen("The Xannor")
	    && memcmp(scratch, "The Xannor", scratch_length) == 0);
	sector.fighter_owner = -2.0f;
	CHECK(yt_sector_fighter_row(&sector, 2, NULL, NULL,
	    row, sizeof(row), &length, scratch, sizeof(scratch),
	    &scratch_length, &changed, &error)
	    && changed && length == sizeof(mercenary_expected) - 1U
	    && memcmp(row, mercenary_expected, length) == 0);
	memcpy(scratch, "keep", 4U);
	scratch_length = 4U;
	sector.fighter_owner = 2.0f;
	CHECK(yt_sector_fighter_row(&sector, 2, NULL, NULL,
	    row, sizeof(row), &length, scratch, sizeof(scratch),
	    &scratch_length, &changed, &error)
	    && !changed && scratch_length == 4U
	    && memcmp(scratch, "keep", 4U) == 0
	    && length == sizeof(self_expected) - 1U
	    && memcmp(row, self_expected, length) == 0);
}

static void
test_radio_private_pager(void)
{
	struct yt_radio_pager_state pager;
	int body;

	yt_radio_pager_begin(&pager);
	yt_radio_pager_add_pair(&pager);
	for (body = 1; body <= 20; ++body)
		CHECK(!yt_radio_pager_add_body(&pager));
	CHECK(pager.line_count == 22.0f);
	CHECK(yt_radio_pager_add_body(&pager) && pager.line_count == 0.0f);
	CHECK(!yt_radio_pager_add_body(&pager) && pager.line_count == 1.0f);

	yt_radio_pager_begin(&pager);
	for (body = 1; body <= 7; ++body) {
		yt_radio_pager_add_pair(&pager);
		CHECK(!yt_radio_pager_add_body(&pager));
	}
	yt_radio_pager_add_pair(&pager);
	CHECK(yt_radio_pager_add_body(&pager) && pager.line_count == 0.0f);
}

static void
test_radio_reader_presentation(void)
{
	static const uint8_t automatic_heading[] =
	    "Checking for Radio Messages.";
	static const uint8_t empty_expected[] =
	    "\r\nChecking for Radio Messages.\r\nNone Found.\r\n";
	static const uint8_t header[] = "Message to: Ada * From: Bob";
	struct yt_present_state current = state(false);
	struct yt_present_result result;
	struct pager_capture capture;
	uint8_t body[74];
	uint8_t expected[139];
	size_t length = 0;

	memset(&capture, 0, sizeof(capture));
	current.bold = 1.0f;
	current.blink = 1.0f;
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(automatic_heading,
	    sizeof(automatic_heading) - 1U, &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line((const uint8_t *)"None Found.",
	    strlen("None Found."), &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(capture.remote_length == sizeof(empty_expected) - 1U
	    && memcmp(capture.remote, empty_expected,
	    sizeof(empty_expected) - 1U) == 0);
	CHECK(current.foreground == 2.0f && current.background == 0.0f
	    && current.bold == 1.0f && current.blink == 1.0f);

	memset(&capture, 0, sizeof(capture));
	current = state(false);
	memset(body, ' ', sizeof(body));
	memcpy(body, "HELLO", strlen("HELLO"));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(automatic_heading,
	    sizeof(automatic_heading) - 1U, &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(header, sizeof(header) - 1U, &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(body, sizeof(body), &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	memcpy(expected + length, "\r\n", 2);
	length += 2U;
	memcpy(expected + length, automatic_heading,
	    sizeof(automatic_heading) - 1U);
	length += sizeof(automatic_heading) - 1U;
	memcpy(expected + length, "\r\n\r\n", 4);
	length += 4U;
	memcpy(expected + length, header, sizeof(header) - 1U);
	length += sizeof(header) - 1U;
	memcpy(expected + length, "\r\n", 2);
	length += 2U;
	memcpy(expected + length, body, sizeof(body));
	length += sizeof(body);
	memcpy(expected + length, "\r\n", 2);
	length += 2U;
	CHECK(length == sizeof(expected));
	CHECK(capture.remote_length == sizeof(expected)
	    && memcmp(capture.remote, expected, sizeof(expected)) == 0);
}

static void
test_editor_aux_notices(void)
{
	static const uint8_t save[] =
	    "Command Saved -+- Ctrl-R to Re-use -+- Ctrl-X to cancel.";
	static const uint8_t repeat[] =
	    "Command Repeated 3 times -+- Ctrl-R to Re-use -+- "
	    "Ctrl-X to cancel.";
	static const uint8_t ansi_bold[] = "\x1b[0;33;40;1m";
	static const uint8_t ansi_normal[] = "\x1b[0;33;40m";
	struct yt_present_state present;
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	uint8_t expected[256];
	size_t length;

	present = state(true);
	present.foreground = 3.0f;
	CHECK(yt_present_color(&present, &result) == YT_PRESENT_OK);
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 3;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &present, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &present, save, sizeof(save) - 1U,
	    &capture);
	length = 0;
	memcpy(expected + length, "\r\n", 2);
	length += 2U;
	memcpy(expected + length, save, sizeof(save) - 1U);
	length += sizeof(save) - 1U;
	memcpy(expected + length, "\n\r", 2);
	length += 2U;
	CHECK(capture.remote_length == length
	    && memcmp(capture.remote, expected, length) == 0);
	CHECK(pager.line_count == 1.0f);

	present = state(true);
	present.foreground = 3.0f;
	CHECK(yt_present_color(&present, &result) == YT_PRESENT_OK);
	present.bold = 1.0f;
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 3;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &present, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &present, repeat, sizeof(repeat) - 1U,
	    &capture);
	length = 0;
	memcpy(expected + length, ansi_bold, sizeof(ansi_bold) - 1U);
	length += sizeof(ansi_bold) - 1U;
	memcpy(expected + length, "\r\n", 2);
	length += 2U;
	memcpy(expected + length, ansi_normal, sizeof(ansi_normal) - 1U);
	length += sizeof(ansi_normal) - 1U;
	memcpy(expected + length, repeat, sizeof(repeat) - 1U);
	length += sizeof(repeat) - 1U;
	memcpy(expected + length, "\n\r", 2);
	length += 2U;
	CHECK(capture.remote_length == length
	    && memcmp(capture.remote, expected, length) == 0);
	CHECK(pager.line_count == 1.0f && present.bold == 0.0f);
}

static struct pager_capture
fatal_fixture(const uint8_t *notice, size_t notice_length, bool ansi)
{
	struct yt_pager_state pager;
	struct yt_present_state present = state(ansi);
	struct yt_present_result result;
	struct pager_capture capture;

	memset(&pager, 0, sizeof(pager));
	memset(&capture, 0, sizeof(capture));
	pager.foreground = 3;
	present.foreground = 3.0f;
	CHECK(yt_present_line(NULL, 0, &present, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &present, notice, notice_length, &capture);
	return capture;
}

static void
test_editor_terminal_notices(void)
{
	static const uint8_t inactivity[] = "\aUSER FELL ASLEEP!";
	static const uint8_t session[] =
	    "\a\a\aTIME LIMIT EXCEEDED!\a\a\a";
	static const uint8_t ansi[] = "\x1b[0;33;40m";
	struct pager_capture capture;
	uint8_t expected[80];
	size_t length;

	capture = fatal_fixture(inactivity, sizeof(inactivity) - 1U, false);
	length = 0;
	memcpy(expected + length, "\r\n", 2);
	length += 2U;
	memcpy(expected + length, inactivity, sizeof(inactivity) - 1U);
	length += sizeof(inactivity) - 1U;
	memcpy(expected + length, "\n\r", 2);
	length += 2U;
	CHECK(capture.remote_length == length
	    && memcmp(capture.remote, expected, length) == 0);

	capture = fatal_fixture(inactivity, sizeof(inactivity) - 1U, true);
	length = 0;
	memcpy(expected + length, ansi, sizeof(ansi) - 1U);
	length += sizeof(ansi) - 1U;
	memcpy(expected + length, "\r\n", 2);
	length += 2U;
	memcpy(expected + length, inactivity, sizeof(inactivity) - 1U);
	length += sizeof(inactivity) - 1U;
	memcpy(expected + length, "\n\r", 2);
	length += 2U;
	CHECK(capture.remote_length == length
	    && memcmp(capture.remote, expected, length) == 0);

	capture = fatal_fixture(session, sizeof(session) - 1U, false);
	length = 0;
	memcpy(expected + length, "\r\n", 2);
	length += 2U;
	memcpy(expected + length, session, sizeof(session) - 1U);
	length += sizeof(session) - 1U;
	memcpy(expected + length, "\n\r", 2);
	length += 2U;
	CHECK(capture.remote_length == length
	    && memcmp(capture.remote, expected, length) == 0);

	capture = fatal_fixture(session, sizeof(session) - 1U, true);
	length = 0;
	memcpy(expected + length, ansi, sizeof(ansi) - 1U);
	length += sizeof(ansi) - 1U;
	memcpy(expected + length, "\r\n", 2);
	length += 2U;
	memcpy(expected + length, session, sizeof(session) - 1U);
	length += sizeof(session) - 1U;
	memcpy(expected + length, "\n\r", 2);
	length += 2U;
	CHECK(capture.remote_length == length
	    && memcmp(capture.remote, expected, length) == 0);
}

static void
test_common_fatal_notice(void)
{
	static const uint8_t notice[] = "Your ship has been destroyed!";
	static const uint8_t ansi_plain[] = "\x1b[0;33;40m";
	static const uint8_t ansi_emphasis[] = "\x1b[0;33;40;5;1m";
	static const uint8_t mine_fatal_splice[] =
	    "\r\nYour ship has been destroyed!\n\r\x07";
	struct yt_pager_state pager;
	struct yt_present_state present;
	struct yt_present_result result;
	struct pager_capture capture;
	uint8_t expected[96];
	size_t length;

	present = state(false);
	present.foreground = 3.0f;
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 3;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &present, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	present.bold = 1.0f;
	present.blink = 1.0f;
	pager.newline_flag = 0.0f;
	pager_fixture_b05d(&pager, &present, notice, sizeof(notice) - 1U,
	    &capture);
	length = 0;
	memcpy(expected + length, "\r\n", 2);
	length += 2U;
	memcpy(expected + length, notice, sizeof(notice) - 1U);
	length += sizeof(notice) - 1U;
	memcpy(expected + length, "\n\r", 2);
	length += 2U;
	CHECK(capture.remote_length == length
	    && memcmp(capture.remote, expected, length) == 0);
	CHECK(present.bold == 1.0f && present.blink == 1.0f);
	CHECK(pager.line_count == 1.0f && pager.newline_flag == 0.0f);
	CHECK(capture.last_local_foreground == 7
	    && capture.last_local_background == 0);

	present = state(true);
	present.foreground = 3.0f;
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 3;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &present, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	present.bold = 1.0f;
	present.blink = 1.0f;
	pager.newline_flag = 0.0f;
	pager_fixture_b05d(&pager, &present, notice, sizeof(notice) - 1U,
	    &capture);
	length = 0;
	memcpy(expected + length, ansi_plain, sizeof(ansi_plain) - 1U);
	length += sizeof(ansi_plain) - 1U;
	memcpy(expected + length, "\r\n", 2);
	length += 2U;
	memcpy(expected + length, ansi_emphasis,
	    sizeof(ansi_emphasis) - 1U);
	length += sizeof(ansi_emphasis) - 1U;
	memcpy(expected + length, notice, sizeof(notice) - 1U);
	length += sizeof(notice) - 1U;
	memcpy(expected + length, "\n\r", 2);
	length += 2U;
	CHECK(capture.remote_length == length
	    && memcmp(capture.remote, expected, length) == 0);
	CHECK(present.bold == 0.0f && present.blink == 0.0f);
	CHECK(pager.line_count == 1.0f && pager.newline_flag == 0.0f);
	CHECK(capture.last_local_foreground == 7
	    && capture.last_local_background == 0);

	/* A destroyed mine return carries background one into this bridge. */
	present = state(false);
	present.foreground = 3.0f;
	present.background = 1.0f;
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 3;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &present, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	present.bold = 1.0f;
	present.blink = 1.0f;
	pager_fixture_b05d(&pager, &present, notice, sizeof(notice) - 1U,
	    &capture);
	CHECK(yt_present_sound(3.0f, &present, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(capture.remote_length == sizeof(mine_fatal_splice) - 1U
	    && memcmp(capture.remote, mine_fatal_splice,
	    sizeof(mine_fatal_splice) - 1U) == 0);
}

static void
test_formatting_wrappers(void)
{
	struct yt_present_state current = state(false);
	struct yt_present_result result;
	uint8_t mutable[16] = {'a', 'b'};
	size_t mutable_length = 2;
	static const uint8_t binary_three[] = {'A', 0, 'C'};
	uint8_t exact_limit[78];
	uint8_t overlong[79];
	uint8_t over_capacity[YT_PRESENT_EVENT_DATA + 1U];

	CHECK(yt_present_right_aligned((const uint8_t *)"abcd", 4, 2.5f,
	    &current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == 3
	    && memcmp(result.remote, "bcd", 3) == 0);
	current.sound.conversion_mode = 4;
	CHECK(yt_present_right_aligned((const uint8_t *)"abcd", 4, 2.5f,
	    &current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == 2
	    && memcmp(result.remote, "cd", 2) == 0);
	CHECK(yt_present_right_aligned((const uint8_t *)"abc", 3, 3.4f,
	    &current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == 3
	    && memcmp(result.remote, "abc", 3) == 0);

	current.sound.conversion_mode = 0;
	CHECK(yt_present_fixed_width(mutable, &mutable_length,
	    sizeof(mutable), 4.0f, &current, &result) == YT_PRESENT_OK);
	CHECK(mutable_length == 4 && memcmp(mutable, "ab  ", 4) == 0);
	CHECK(result.remote_length == 4
	    && memcmp(result.remote, "ab  ", 4) == 0);

	current.bold = 0.0f;
	CHECK(yt_present_bold_line((const uint8_t *)"x", 1,
	    &current, &result) == YT_PRESENT_OK);
	CHECK(current.bold == 1.0f);
	CHECK(result.remote_length == 3
	    && memcmp(result.remote, "x\r\n", 3) == 0);
	current.bold = 7.0f;
	CHECK(yt_present_bold_character((const uint8_t *)"y", 1,
	    &current, &result) == YT_PRESENT_OK);
	CHECK(current.bold == 7.0f);

	CHECK(yt_present_centered_line((const uint8_t *)"A", 1,
	    &current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == 42);
	CHECK(result.remote[38] == ' ' && result.remote[39] == 'A');
	CHECK(result.remote[40] == '\r' && result.remote[41] == '\n');
	CHECK(yt_present_centered_line(binary_three, sizeof(binary_three),
	    &current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == 43U
	    && memcmp(result.remote + 38U, binary_three,
	    sizeof(binary_three)) == 0
	    && result.remote[41] == '\r' && result.remote[42] == '\n');
	CHECK(yt_present_centered_line(NULL, 0, &current, &result)
	    == YT_PRESENT_OK);
	CHECK(result.remote_length == 2
	    && memcmp(result.remote, "\r\n", 2) == 0);
	memset(exact_limit, 'Y', sizeof(exact_limit));
	CHECK(yt_present_centered_line(exact_limit, sizeof(exact_limit),
	    &current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == sizeof(exact_limit) + 2U
	    && memcmp(result.remote, exact_limit, sizeof(exact_limit)) == 0);
	memset(overlong, 'Z', sizeof(overlong));
	CHECK(yt_present_centered_line(overlong, sizeof(overlong),
	    &current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == sizeof(overlong) + 2U
	    && memcmp(result.remote, overlong, sizeof(overlong)) == 0);
	memset(over_capacity, 'Q', sizeof(over_capacity));
	CHECK(yt_present_centered_line(over_capacity, sizeof(over_capacity),
	    &current, &result) == YT_PRESENT_CAPACITY);
	CHECK(result.event_count == 0U && result.remote_length == 0U);
}

static void
test_attention(void)
{
	static const uint8_t cue[] =
	    "MBO2T200L64FBEAP8FBEAP8FBEAP4FBEAP8FBEAP8FBEAP4T128";
	static const uint8_t first_color[] = "\x1b[0;33;41;5;1m";
	static const uint8_t second_color[] = "\x1b[0;33;40m\r\n\x1b[";
	struct yt_present_state current = state(true);
	struct yt_present_result result;
	struct replay_capture capture;
	const struct yt_present_sink sink = {
		.context = &capture,
		.remote = capture_remote,
		.local_color = capture_color,
		.local_text = capture_text,
	};
	uint8_t expected[256];
	uint8_t background_raw[] = {0xa5, 0x5a, 0x80, 0x00};
	uint8_t bold_raw[] = {0x11, 0x22, 0x80, 0x00};
	uint8_t blink_raw[] = {0x33, 0x44, 0x80, 0x00};
	uint8_t mode_raw[4];
	uint8_t user_sound_raw[] = {0x5a, 0xa5, 0x80, 0x00};
	uint8_t local_sound_raw[4];
	static const uint8_t raw_zero[] = {0x00, 0x00, 0x00, 0x00};
	static const uint8_t raw_one[] = {0x00, 0x00, 0x00, 0x81};
	static const uint8_t dirty_bold[] = {0x11, 0x22, 0x80, 0x00};
	uint8_t over_capacity[YT_PRESENT_REMOTE_SIZE + 1U];
	size_t expected_length = 0;

	memcpy(expected + expected_length, first_color,
	    sizeof(first_color) - 1U);
	expected_length += sizeof(first_color) - 1U;
	memcpy(expected + expected_length, "ALERT", 5);
	expected_length += 5;
	memcpy(expected + expected_length, second_color,
	    sizeof(second_color) - 1U);
	expected_length += sizeof(second_color) - 1U;
	memcpy(expected + expected_length, cue, sizeof(cue) - 1U);
	expected_length += sizeof(cue) - 1U;
	expected[expected_length++] = 0x0e;
	CHECK(yt_present_attention((const uint8_t *)"ALERT", 5,
	    &current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == expected_length);
	CHECK(memcmp(result.remote, expected, expected_length) == 0);
	CHECK(result.event_count == 11);
	CHECK(result.events[0].operation == YT_PRESENT_LOCAL_COLOR);
	CHECK(result.events[0].foreground == 30
	    && result.events[0].background == 4);
	CHECK(result.events[1].operation == YT_PRESENT_REMOTE_SEMI);
	CHECK(result.events[2].operation == YT_PRESENT_LOCAL_SEMI);
	CHECK(result.events[3].operation == YT_PRESENT_REMOTE_SEMI);
	CHECK(result.events[4].operation == YT_PRESENT_LOCAL_COLOR);
	CHECK(result.events[4].foreground == 6
	    && result.events[4].background == 0);
	CHECK(result.events[5].operation == YT_PRESENT_REMOTE_SEMI);
	CHECK(result.events[6].operation == YT_PRESENT_LOCAL_LINE);
	CHECK(result.events[7].operation == YT_PRESENT_REMOTE_LINE);
	CHECK(result.events[8].operation == YT_PRESENT_REMOTE_SEMI);
	CHECK(result.events[9].operation == YT_PRESENT_REMOTE_SEMI);
	CHECK(result.events[10].operation == YT_PRESENT_LOCAL_PLAY);
	CHECK(result.events[10].length == sizeof(cue) - 1U);
	memset(&capture, 0, sizeof(capture));
	yt_present_replay(&result, &sink);
	CHECK(capture.count == 10);
	for (size_t index = 0; index < capture.count; ++index)
		CHECK(capture.operations[index] == result.events[index].operation);
	CHECK(yt_present_pc_attribute(30, 4) == 0xce);
	CHECK(current.foreground == 3.0f && current.background == 0.0f);
	CHECK(current.bold == 0.0f && current.blink == 0.0f);
	CHECK(current.sound.scratch_length == 0);

	current = state(false);
	CHECK(yt_present_attention((const uint8_t *)"ALERT", 5,
	    &current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == 8);
	CHECK(memcmp(result.remote, "ALERT\r\n\x07", 8) == 0);
	CHECK(result.event_count == 7);
	CHECK(result.events[6].operation == YT_PRESENT_LOCAL_PLAY);
	CHECK(result.events[6].length == 11);
	CHECK(current.foreground == 3.0f && current.background == 0.0f
	    && current.bold == 0.0f && current.blink == 1.0f);

	current = state(false);
	CHECK(qb_mbf32_encode(0.0f, mode_raw) == QB_MBF_OK
	    && qb_mbf32_encode(-1.0f, local_sound_raw) == QB_MBF_OK);
	yt_sound_bind_endpoint_process(&current.sound, mode_raw,
	    user_sound_raw, local_sound_raw);
	CHECK(yt_present_attention((const uint8_t *)"ALERT", 5,
	    &current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == 7U
	    && memcmp(result.remote, "ALERT\r\n", 7U) == 0
	    && result.event_count == 6U
	    && result.events[5].operation == YT_PRESENT_LOCAL_PLAY
	    && memcmp(user_sound_raw,
	    (uint8_t[]){0x5a, 0xa5, 0x80, 0x00}, sizeof(user_sound_raw)) == 0);

	current = state(true);
	current.sound.mode = 1.0f;
	CHECK(yt_present_attention((const uint8_t *)"ALERT", 5,
	    &current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == 0U && result.event_count == 5U);
	CHECK(result.events[0].operation == YT_PRESENT_LOCAL_COLOR
	    && result.events[0].foreground == 30
	    && result.events[0].background == 4);
	CHECK(result.events[1].operation == YT_PRESENT_LOCAL_SEMI
	    && result.events[1].length == 5U
	    && memcmp(result.events[1].data, "ALERT", 5U) == 0);
	CHECK(result.events[2].operation == YT_PRESENT_LOCAL_COLOR
	    && result.events[2].foreground == 6
	    && result.events[2].background == 0);
	CHECK(result.events[3].operation == YT_PRESENT_LOCAL_LINE
	    && result.events[3].length == 0U);
	CHECK(result.events[4].operation == YT_PRESENT_LOCAL_PLAY
	    && result.events[4].length == sizeof(cue) - 1U);
	CHECK(current.foreground == 3.0f && current.background == 0.0f
	    && current.bold == 0.0f && current.blink == 0.0f);

	current = state(false);
	current.sound.user_sound = 40000.0f;
	CHECK(yt_present_attention((const uint8_t *)"ALERT", 5,
	    &current, &result) == YT_PRESENT_SOUND_ERROR);
	CHECK(result.remote_length == 7U
	    && memcmp(result.remote, "ALERT\r\n", 7U) == 0);
	CHECK(result.event_count == 5U
	    && result.events[0].operation == YT_PRESENT_LOCAL_SEMI
	    && result.events[1].operation == YT_PRESENT_REMOTE_SEMI
	    && result.events[2].operation == YT_PRESENT_LOCAL_LINE
	    && result.events[3].operation == YT_PRESENT_REMOTE_LINE
	    && result.events[4].operation == YT_PRESENT_REMOTE_SEMI);
	CHECK(current.foreground == 3.0f && current.background == 0.0f
	    && current.bold == 0.0f && current.blink == 1.0f);

	current = state(true);
	yt_present_bind_background_process(&current, background_raw);
	yt_present_bind_bold_process(&current, bold_raw);
	yt_present_bind_blink_process(&current, blink_raw);
	CHECK(yt_present_background(&current) == 0.0f
	    && yt_present_bold(&current) == 0.0f
	    && yt_present_blink(&current) == 0.0f
	    && memcmp(background_raw, (uint8_t[]){0xa5, 0x5a, 0x80, 0x00},
	    sizeof(background_raw)) == 0);
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	CHECK(memcmp(background_raw,
	    (uint8_t[]){0xa5, 0x5a, 0x80, 0x00},
	    sizeof(background_raw)) == 0
	    && memcmp(bold_raw, raw_zero, sizeof(bold_raw)) == 0
	    && memcmp(blink_raw, raw_zero, sizeof(blink_raw)) == 0);
	CHECK(yt_present_attention((const uint8_t *)"ALERT", 5U,
	    &current, &result) == YT_PRESENT_OK);
	CHECK(current.background == 0.0f
	    && yt_present_background(&current) == 0.0f
	    && memcmp(background_raw, raw_zero, sizeof(background_raw)) == 0
	    && memcmp(bold_raw, raw_zero, sizeof(bold_raw)) == 0
	    && memcmp(blink_raw, raw_zero, sizeof(blink_raw)) == 0);

	memset(over_capacity, 'Q', sizeof(over_capacity));
	memcpy(background_raw, raw_zero, sizeof(background_raw));
	memcpy(bold_raw, raw_zero, sizeof(bold_raw));
	memcpy(blink_raw, raw_zero, sizeof(blink_raw));
	current = state(true);
	yt_present_bind_background_process(&current, background_raw);
	yt_present_bind_bold_process(&current, bold_raw);
	yt_present_bind_blink_process(&current, blink_raw);
	CHECK(yt_present_attention(over_capacity, sizeof(over_capacity),
	    &current, &result) == YT_PRESENT_CAPACITY);
	CHECK(current.background == 1.0f
	    && yt_present_background(&current) == 1.0f
	    && memcmp(background_raw, raw_one, sizeof(background_raw)) == 0
	    && memcmp(bold_raw, raw_zero, sizeof(bold_raw)) == 0
	    && memcmp(blink_raw, raw_zero, sizeof(blink_raw)) == 0);

	memcpy(background_raw, raw_zero, sizeof(background_raw));
	memcpy(bold_raw, dirty_bold, sizeof(bold_raw));
	memcpy(blink_raw, raw_zero, sizeof(blink_raw));
	current = state(false);
	yt_present_bind_background_process(&current, background_raw);
	yt_present_bind_bold_process(&current, bold_raw);
	yt_present_bind_blink_process(&current, blink_raw);
	CHECK(yt_present_attention((const uint8_t *)"ALERT", 5U,
	    &current, &result) == YT_PRESENT_OK);
	CHECK(memcmp(background_raw, raw_zero, sizeof(background_raw)) == 0
	    && memcmp(bold_raw, dirty_bold, sizeof(bold_raw)) == 0
	    && memcmp(blink_raw, raw_one, sizeof(blink_raw)) == 0);

	memcpy(bold_raw, raw_zero, sizeof(bold_raw));
	memcpy(blink_raw, raw_zero, sizeof(blink_raw));
	current = state(true);
	current.foreground = 40000.0f;
	yt_present_bind_bold_process(&current, bold_raw);
	yt_present_bind_blink_process(&current, blink_raw);
	CHECK(yt_present_bold_line((const uint8_t *)"X", 1U,
	    &current, &result) == YT_PRESENT_OVERFLOW);
	CHECK(memcmp(bold_raw, raw_one, sizeof(bold_raw)) == 0
	    && memcmp(blink_raw, raw_zero, sizeof(blink_raw)) == 0);
}

static void
test_sound_toggle(void)
{
	static const uint8_t ansi_on[] =
	    "Sound ON\r\n"
	    "\x1b[MBO4L32P32CP64CP64CP64L16EP64L32CP64L12E\x0e";
	static const uint8_t plain_on[] = "Sound ON\r\n\x07";
	static const uint8_t off[] = "Sound OFF\r\n";
	static const uint8_t ansi_cue[] =
	    "MBO4L32P32CP64CP64CP64L16EP64L32CP64L12E";
	struct yt_present_state current = state(true);
	struct yt_present_result result;
	uint8_t mode_raw[4];
	uint8_t user_raw[4];
	uint8_t local_raw[4];

	current.color_initialized = 1.0f;
	current.cached_foreground = current.foreground;
	current.cached_background = current.background;
	current.sound.user_sound = 0.0f;
	CHECK(yt_present_sound_toggle(&current, &result) == YT_PRESENT_OK);
	CHECK(current.sound.user_sound == -1.0f);
	CHECK(current.sound.local_sound == -1.0f);
	CHECK(result.remote_length == sizeof(ansi_on) - 1U);
	CHECK(memcmp(result.remote, ansi_on, sizeof(ansi_on) - 1U) == 0);
	CHECK(result.event_count == 6);
	CHECK(result.events[0].operation == YT_PRESENT_LOCAL_COLOR);
	CHECK(result.events[1].operation == YT_PRESENT_LOCAL_LINE);
	CHECK(result.events[1].length == 8
	    && memcmp(result.events[1].data, "Sound ON", 8) == 0);
	CHECK(result.events[2].operation == YT_PRESENT_REMOTE_LINE);
	CHECK(result.events[3].operation == YT_PRESENT_REMOTE_SEMI);
	CHECK(result.events[4].operation == YT_PRESENT_REMOTE_SEMI);
	CHECK(result.events[4].length == sizeof(ansi_cue) + 2U);
	CHECK(result.events[5].operation == YT_PRESENT_LOCAL_PLAY);
	CHECK(result.events[5].length == sizeof(ansi_cue) - 1U
	    && memcmp(result.events[5].data, ansi_cue,
	    sizeof(ansi_cue) - 1U) == 0);

	current = state(false);
	current.sound.user_sound = 0.0f;
	current.sound.local_sound = 77.0f;
	CHECK(yt_present_sound_toggle(&current, &result) == YT_PRESENT_OK);
	CHECK(current.sound.user_sound == -1.0f);
	CHECK(current.sound.local_sound == 77.0f);
	CHECK(result.remote_length == sizeof(plain_on) - 1U
	    && memcmp(result.remote, plain_on, sizeof(plain_on) - 1U) == 0);
	CHECK(result.event_count == 5);
	CHECK(result.events[0].operation == YT_PRESENT_LOCAL_LINE);
	CHECK(result.events[1].operation == YT_PRESENT_REMOTE_LINE);
	CHECK(result.events[2].operation == YT_PRESENT_REMOTE_SEMI);
	CHECK(result.events[3].operation == YT_PRESENT_REMOTE_SEMI
	    && result.events[3].length == 1U
	    && result.events[3].data[0] == '\x07');
	CHECK(result.events[4].operation == YT_PRESENT_LOCAL_PLAY);

	current = state(true);
	current.color_initialized = 1.0f;
	current.cached_foreground = current.foreground;
	current.cached_background = current.background;
	current.sound.user_sound = -1.0f;
	CHECK(yt_present_sound_toggle(&current, &result) == YT_PRESENT_OK);
	CHECK(current.sound.user_sound == 0.0f);
	CHECK(result.remote_length == sizeof(off) - 1U
	    && memcmp(result.remote, off, sizeof(off) - 1U) == 0);
	CHECK(result.event_count == 4);
	CHECK(result.events[0].operation == YT_PRESENT_LOCAL_COLOR);
	CHECK(result.events[1].operation == YT_PRESENT_LOCAL_LINE);
	CHECK(result.events[2].operation == YT_PRESENT_REMOTE_LINE);
	CHECK(result.events[3].operation == YT_PRESENT_REMOTE_SEMI);

	current = state(true);
	current.sound.mode = 1.0f;
	current.sound.user_sound = 0.0f;
	current.sound.local_sound = 77.0f;
	CHECK(yt_present_sound_toggle(&current, &result) == YT_PRESENT_OK);
	CHECK(current.sound.user_sound == -1.0f
	    && current.sound.local_sound == -1.0f);
	CHECK(result.remote_length == 0 && result.event_count == 3);
	CHECK(result.events[0].operation == YT_PRESENT_LOCAL_COLOR);
	CHECK(result.events[1].operation == YT_PRESENT_LOCAL_LINE);
	CHECK(result.events[2].operation == YT_PRESENT_LOCAL_PLAY);

	current = state(true);
	current.sound.mode = 2.0f;
	current.sound.user_sound = 0.0f;
	current.sound.local_sound = 77.0f;
	CHECK(yt_present_sound_toggle(&current, &result) == YT_PRESENT_OK);
	CHECK(current.sound.user_sound == -1.0f
	    && current.sound.local_sound == -1.0f);
	CHECK(result.remote_length == 10U
	    && memcmp(result.remote, "Sound ON\r\n", 10U) == 0);
	CHECK(result.event_count == 5);
	CHECK(result.events[0].operation == YT_PRESENT_LOCAL_COLOR);
	CHECK(result.events[1].operation == YT_PRESENT_LOCAL_LINE);
	CHECK(result.events[2].operation == YT_PRESENT_REMOTE_LINE);
	CHECK(result.events[3].operation == YT_PRESENT_REMOTE_SEMI);
	CHECK(result.events[4].operation == YT_PRESENT_LOCAL_PLAY);

	current = state(false);
	current.sound.user_sound = 40000.0f;
	CHECK(yt_present_sound_toggle(&current, &result)
	    == YT_PRESENT_SOUND_ERROR);
	CHECK(result.event_count == 0 && result.remote_length == 0);

	current = state(true);
	CHECK(qb_mbf32_encode(0.0f, mode_raw) == QB_MBF_OK
	    && qb_mbf32_encode(-1.0f, user_raw) == QB_MBF_OK
	    && qb_mbf32_encode(77.0f, local_raw) == QB_MBF_OK);
	CHECK(yt_present_sound_toggle_process(mode_raw, user_raw, local_raw,
	    &current, &result) == YT_PRESENT_OK);
	CHECK(memcmp(user_raw, "\xff\xff\x00\x00", 4U) == 0);
	CHECK(qb_mbf32_decode(local_raw) == 77.0f);
	CHECK(result.remote_length >= sizeof(off) - 1U);
	CHECK(memcmp(result.remote + result.remote_length - (sizeof(off) - 1U),
	    off, sizeof(off) - 1U) == 0);

	current = state(true);
	CHECK(yt_present_sound(4.0f, &current, &result) == YT_PRESENT_OK);
	CHECK(result.event_count == 2);
	CHECK(result.events[0].operation == YT_PRESENT_REMOTE_SEMI);
	CHECK(result.events[1].operation == YT_PRESENT_LOCAL_PLAY);

	current = state(true);
	current.sound.mode = 0.0f;
	current.sound.local_sound = 0.0f;
	current.sound.user_sound = -1.0f;
	CHECK(yt_present_sysop_sound_toggle(&current, &result)
	    == YT_PRESENT_OK);
	CHECK(current.sound.local_sound == -1.0f
	    && current.sound.user_sound == -1.0f);
	CHECK(result.remote_length == 0 && result.event_count == 3);
	CHECK(result.events[0].operation == YT_PRESENT_LOCAL_LINE
	    && result.events[0].length == 0);
	CHECK(result.events[1].operation == YT_PRESENT_LOCAL_SEMI
	    && result.events[1].length == 6
	    && memcmp(result.events[1].data, "Sound ", 6) == 0);
	CHECK(result.events[2].operation == YT_PRESENT_LOCAL_LINE
	    && result.events[2].length == 2
	    && memcmp(result.events[2].data, "ON", 2) == 0);
	current.sound.mode = 1.0f;
	CHECK(yt_present_sysop_sound_toggle(&current, &result)
	    == YT_PRESENT_OK);
	CHECK(current.sound.local_sound == 0.0f
	    && current.sound.user_sound == 0.0f);
	CHECK(result.remote_length == 0 && result.event_count == 3
	    && result.events[2].length == 3
	    && memcmp(result.events[2].data, "OFF", 3) == 0);
	current.sound.local_sound = 40000.0f;
	current.sound.user_sound = 77.0f;
	CHECK(yt_present_sysop_sound_toggle(&current, &result)
	    == YT_PRESENT_SOUND_ERROR);
	CHECK(current.sound.local_sound == 40000.0f
	    && current.sound.user_sound == 77.0f
	    && result.remote_length == 0 && result.event_count == 0);

	current = state(false);
	CHECK(qb_mbf32_encode(1.0f, mode_raw) == QB_MBF_OK
	    && qb_mbf32_encode(-1.0f, local_raw) == QB_MBF_OK
	    && qb_mbf32_encode(77.0f, user_raw) == QB_MBF_OK);
	CHECK(yt_present_sysop_sound_toggle_process(mode_raw, local_raw,
	    user_raw, &current, &result) == YT_PRESENT_OK);
	CHECK(memcmp(local_raw, "\xff\xff\x00\x00", 4U) == 0
	    && memcmp(user_raw, local_raw, 4U) == 0
	    && result.event_count == 3U
	    && result.events[2].length == 3U
	    && memcmp(result.events[2].data, "OFF", 3U) == 0);

	current = state(false);
	current.sound.mode = 0.0f;
	current.sound.snoop = 0.0f;
	CHECK(yt_present_sysop_snoop_toggle((const uint8_t *)"Grace Hopper",
	    12, (const uint8_t *)"COBOL", 5, &current, &result)
	    == YT_PRESENT_OK);
	CHECK(current.sound.snoop == -1.0f && result.remote_length == 0
	    && result.event_count == 12);
	CHECK(result.events[0].operation == YT_PRESENT_LOCAL_LOCATE
	    && result.events[0].row == 25 && result.events[0].column == 1);
	CHECK(result.events[9].operation == YT_PRESENT_LOCAL_COLOR
	    && result.events[9].foreground == 7
	    && result.events[9].background == 0);
	CHECK(result.events[10].operation == YT_PRESENT_LOCAL_LOCATE
	    && result.events[10].row == 24 && result.events[10].column == 1);
	CHECK(result.events[11].operation == YT_PRESENT_LOCAL_LINE
	    && result.events[11].length == 8
	    && memcmp(result.events[11].data, "SNOOP ON", 8) == 0);
	CHECK(yt_present_sysop_snoop_toggle((const uint8_t *)"R", 1,
	    (const uint8_t *)"A", 1, &current, &result) == YT_PRESENT_OK);
	CHECK(current.sound.snoop == 0.0f && result.remote_length == 0
	    && result.event_count == 2);
	CHECK(result.events[0].operation == YT_PRESENT_LOCAL_LOCATE
	    && result.events[0].row == -1 && result.events[0].column == -1
	    && result.events[0].cursor_visible == 0);
	CHECK(result.events[1].operation == YT_PRESENT_LOCAL_CLEAR);
	current.sound.mode = 1.0f;
	current.sound.snoop = 40000.0f;
	CHECK(yt_present_sysop_snoop_toggle((const uint8_t *)"R", 1,
	    (const uint8_t *)"A", 1, &current, &result) == YT_PRESENT_OK);
	CHECK(current.sound.snoop == 40000.0f && result.remote_length == 0
	    && result.event_count == 0);
	{
		static const uint8_t negative_one[4] = {
			0U, 0U, 0x80U, 0x81U,
		};
		static const uint8_t zero[4] = {0U, 0U, 0U, 0U};
		static const uint8_t dirty_zero[4] = {
			0x11U, 0x22U, 0x33U, 0U,
		};
		uint8_t mode[4];
		uint8_t snoop[4];
		uint8_t before[4];

		CHECK(qb_mbf32_encode(0.0f, mode) == QB_MBF_OK);
		memcpy(snoop, dirty_zero, sizeof(snoop));
		current = state(false);
		CHECK(yt_present_sysop_snoop_toggle_process(
		    (const uint8_t *)"R", 1U, (const uint8_t *)"A", 1U,
		    mode, snoop, &current, &result) == YT_PRESENT_OK
		    && current.sound.snoop == -1.0f
		    && memcmp(snoop, negative_one, sizeof(snoop)) == 0);
		CHECK(yt_present_sysop_snoop_toggle_process(
		    (const uint8_t *)"R", 1U, (const uint8_t *)"A", 1U,
		    mode, snoop, &current, &result) == YT_PRESENT_OK
		    && current.sound.snoop == 0.0f
		    && memcmp(snoop, zero, sizeof(snoop)) == 0);
		CHECK(qb_mbf32_encode(1.0f, mode) == QB_MBF_OK
		    && qb_mbf32_encode(40000.0f, snoop) == QB_MBF_OK);
		memcpy(before, snoop, sizeof(before));
		CHECK(yt_present_sysop_snoop_toggle_process(
		    (const uint8_t *)"R", 1U, (const uint8_t *)"A", 1U,
		    mode, snoop, &current, &result) == YT_PRESENT_OK
		    && memcmp(snoop, before, sizeof(snoop)) == 0);
		CHECK(qb_mbf32_encode(0.0f, mode) == QB_MBF_OK
		    && yt_present_sysop_snoop_toggle_process(
		    (const uint8_t *)"R", 1U, (const uint8_t *)"A", 1U,
		    mode, snoop, &current, &result) == YT_PRESENT_SOUND_ERROR
		    && memcmp(snoop, before, sizeof(snoop)) == 0);
	}
}

struct sysop_replay_tape {
	float *deadline;
	float expected_deadline;
	size_t calls;
	bool fail;
};

struct sysop_process_replay_tape {
	uint8_t *deadline;
	float expected_deadline;
	size_t calls;
	bool fail;
};

static enum yt_present_status
capture_sysop_replay(void *context)
{
	struct sysop_replay_tape *tape = context;

	++tape->calls;
	CHECK(*tape->deadline == tape->expected_deadline);
	return tape->fail ? YT_PRESENT_RANGE : YT_PRESENT_OK;
}

static enum yt_present_status
capture_sysop_process_replay(void *context)
{
	struct sysop_process_replay_tape *tape = context;

	++tape->calls;
	CHECK(qb_mbf32_decode(tape->deadline) == tape->expected_deadline);
	return tape->fail ? YT_PRESENT_RANGE : YT_PRESENT_OK;
}

static void
test_sysop_time(void)
{
	static const uint8_t positive_prompt[] =
	    "SysOp, how many minutes till user is forced off [ 5 ] ? ";
	static const uint8_t negative_prompt[] =
	    "SysOp, how many minutes till user is forced off [-5 ] ? ";
	static const struct {
		const char *entered;
		float expected_deadline;
		float expected_minutes;
	} vectors[] = {
		{"0", 1000.0f, 0.0f},
		{"-5", 700.0f, -5.0f},
		{"30", 2800.0f, 30.0f},
		{"90", 6400.0f, 90.0f},
		{"91", 6400.0f, 90.0f},
		{"180", 6400.0f, 90.0f},
		{"abc", 1000.0f, 0.0f},
		{"&H1E", 2800.0f, 30.0f},
		{"-&O10", 1000.0f, 0.0f},
		{"&H5B", 6400.0f, 90.0f},
		{"-100", -5000.0f, -100.0f},
	};
	struct yt_present_result result;
	struct sysop_replay_tape tape;
	float deadline;
	float minutes;
	bool changed;
	size_t index;

	CHECK(yt_present_sysop_time_prompt(1300.0f, 1000.99f, &result)
	    == YT_PRESENT_OK);
	CHECK(result.remote_length == 0 && result.event_count == 2
	    && result.events[0].operation == YT_PRESENT_LOCAL_LINE
	    && result.events[0].length == 0
	    && result.events[1].operation == YT_PRESENT_LOCAL_SEMI
	    && result.events[1].length == sizeof(positive_prompt) - 1U
	    && memcmp(result.events[1].data, positive_prompt,
	    sizeof(positive_prompt) - 1U) == 0);
	CHECK(yt_present_sysop_time_prompt(700.0f, 1000.99f, &result)
	    == YT_PRESENT_OK);
	CHECK(result.event_count == 2
	    && result.events[1].length == sizeof(negative_prompt) - 1U
	    && memcmp(result.events[1].data, negative_prompt,
	    sizeof(negative_prompt) - 1U) == 0);

	deadline = 9999.0f;
	CHECK(yt_present_sysop_time_replace(NULL, 0, 1000.75f, &deadline,
	    &minutes, &changed) == YT_PRESENT_OK);
	CHECK(!changed && deadline == 9999.0f && minutes == 0.0f);
	for (index = 0; index < YT_ARRAY_LEN(vectors); ++index) {
		deadline = 9999.0f;
		CHECK(yt_present_sysop_time_replace(
		    (const uint8_t *)vectors[index].entered,
		    strlen(vectors[index].entered), 1000.75f, &deadline,
		    &minutes, &changed) == YT_PRESENT_OK);
		CHECK(changed && deadline == vectors[index].expected_deadline
		    && minutes == vectors[index].expected_minutes);
	}
	deadline = 9999.0f;
	CHECK(yt_present_sysop_time_replace((const uint8_t *)"30", 2,
	    2000.25f, &deadline, &minutes, &changed) == YT_PRESENT_OK);
	CHECK(changed && deadline == 3800.0f && minutes == 30.0f);
	deadline = 9999.0f;
	CHECK(yt_present_sysop_time_replace((const uint8_t *)"1E9999", 6,
	    1000.75f, &deadline, &minutes, &changed)
	    == YT_PRESENT_OVERFLOW);
	CHECK(!changed && deadline == 9999.0f);

	tape = (struct sysop_replay_tape){&deadline, 3800.0f, 0, false};
	deadline = 9999.0f;
	CHECK(yt_present_sysop_time_handler(1000.75f,
	    (const uint8_t *)"30", 2, 2000.25f, &deadline, &minutes,
	    &changed, &result, capture_sysop_replay, &tape) == YT_PRESENT_OK);
	CHECK(tape.calls == 1U && changed && deadline == 3800.0f
	    && result.event_count == 2);
	tape = (struct sysop_replay_tape){&deadline, 9999.0f, 0, true};
	deadline = 9999.0f;
	CHECK(yt_present_sysop_time_handler(1000.75f, NULL, 0, 0.0f,
	    &deadline, &minutes, &changed, &result, capture_sysop_replay,
	    &tape) == YT_PRESENT_RANGE);
	CHECK(tape.calls == 1U && !changed && deadline == 9999.0f);
	{
		static const uint8_t dirty_zero[4] = {
			0x11U, 0x22U, 0x33U, 0U,
		};
		struct sysop_process_replay_tape process_tape;
		uint8_t raw_deadline[4];
		uint8_t before[4];

		memcpy(raw_deadline, dirty_zero, sizeof(raw_deadline));
		memcpy(before, raw_deadline, sizeof(before));
		CHECK(yt_present_sysop_time_replace_process(NULL, 0U, 1.0f,
		    raw_deadline, &minutes, &changed) == YT_PRESENT_OK
		    && !changed
		    && memcmp(raw_deadline, before, sizeof(raw_deadline)) == 0);
		CHECK(qb_mbf32_encode(9999.0f, raw_deadline) == QB_MBF_OK
		    && yt_present_sysop_time_replace_process(
		    (const uint8_t *)"30", 2U, 2000.25f, raw_deadline,
		    &minutes, &changed) == YT_PRESENT_OK
		    && changed && minutes == 30.0f
		    && qb_mbf32_decode(raw_deadline) == 3800.0f);
		CHECK(qb_mbf32_encode(9999.0f, raw_deadline) == QB_MBF_OK);
		process_tape = (struct sysop_process_replay_tape){
			raw_deadline, 3800.0f, 0U, true,
		};
		CHECK(yt_present_sysop_time_handler_process(1000.75f,
		    (const uint8_t *)"30", 2U, 2000.25f, raw_deadline,
		    &minutes, &changed, &result, capture_sysop_process_replay,
		    &process_tape) == YT_PRESENT_RANGE
		    && process_tape.calls == 1U && changed
		    && qb_mbf32_decode(raw_deadline) == 3800.0f);
	}
}

static void
test_sysop_chat_header(void)
{
	static const uint8_t expected[] =
	    "Ada - Hit ESC to exit chat mode";
	uint8_t too_long[YT_PRESENT_EVENT_DATA];
	struct yt_present_result result;

	CHECK(yt_present_sysop_chat_header((const uint8_t *)"Ada", 3U, 5,
	    &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == 0U && result.event_count == 4U);
	CHECK(result.events[0].operation == YT_PRESENT_LOCAL_COLOR
	    && result.events[0].foreground == 30
	    && result.events[0].background == 5);
	CHECK(result.events[1].operation == YT_PRESENT_LOCAL_LINE
	    && result.events[1].length == 0U);
	CHECK(result.events[2].operation == YT_PRESENT_LOCAL_LINE
	    && result.events[2].length == 0U);
	CHECK(result.events[3].operation == YT_PRESENT_LOCAL_LINE
	    && result.events[3].length == sizeof(expected) - 1U
	    && memcmp(result.events[3].data, expected,
	    sizeof(expected) - 1U) == 0);
	CHECK(yt_present_sysop_chat_header(NULL, 0U, 0, &result)
	    == YT_PRESENT_OK);
	CHECK(result.events[3].length
	    == strlen(" - Hit ESC to exit chat mode"));
	memset(too_long, 'X', sizeof(too_long));
	CHECK(yt_present_sysop_chat_header(too_long, sizeof(too_long), 0,
	    &result) == YT_PRESENT_CAPACITY);
	CHECK(yt_present_sysop_chat_header(NULL, 1U, 0, &result)
	    == YT_PRESENT_CAPACITY);
}

static void
test_basic_fault_registry(void)
{
	static const struct {
		enum yt_basic_fault_module module;
		uint16_t instruction;
		uint16_t saved_ip;
		uint16_t retry_statement;
		int32_t source_line;
		uint16_t handler;
		uint8_t domain;
	} expected[] = {
		{YT_BASIC_FAULT_MAIN, 0x8E10U, 0x8E13U, 0x8DFCU, 33780,
		    0xB2DAU, 0U},
		{YT_BASIC_FAULT_SHARED, 0x97C6U, 0x97C9U, 0x97BBU, 64006,
		    0x45F7U, 0U},
		{YT_BASIC_FAULT_SHARED, 0x97EEU, 0x97F1U, 0x97E3U, 64006,
		    0x45F7U, 0U},
		{YT_BASIC_FAULT_MAIN, 0x67D8U, 0x67DBU, 0x67CDU, 33000,
		    0xB2DAU, 0U},
		{YT_BASIC_FAULT_MAIN, 0x680BU, 0x680EU, 0x6800U, 33000,
		    0xB2DAU, 0U},
		{YT_BASIC_FAULT_MAIN, 0x6AE6U, 0x6AE9U, 0x6ADBU, 33000,
		    0xB2DAU, 1U},
		{YT_BASIC_FAULT_MAIN, 0xA9A2U, 0xA9A5U, 0xA997U, 40001,
		    0xB2DAU, 0U},
		{YT_BASIC_FAULT_MAIN, 0xA428U, 0xA42BU, 0xA41DU, 33990,
		    0xB2DAU, 0U},
		{YT_BASIC_FAULT_MAIN, 0x6B0BU, 0x6B0EU, 0x6B00U, 33100,
		    0xB2DAU, 0U},
		{YT_BASIC_FAULT_MAIN, 0x6CDBU, 0x6CDEU, 0x6CC7U, 33150,
		    0xB2DAU, 0U},
		{YT_BASIC_FAULT_SHARED, 0x103FU, 0x1042U, 0x103CU, 0,
		    0x45F7U, 2U},
		{YT_BASIC_FAULT_SHARED, 0x1050U, 0x1053U, 0x104DU, 0,
		    0x45F7U, 2U},
		{YT_BASIC_FAULT_SHARED, 0x1067U, 0x106AU, 0x1064U, 0,
		    0x45F7U, 2U},
		{YT_BASIC_FAULT_SHARED, 0x1072U, 0x1075U, 0x106FU, 0,
		    0x45F7U, 2U},
		{YT_BASIC_FAULT_SHARED, 0x1099U, 0x109CU, 0x1096U, 0,
		    0x45F7U, 2U},
		{YT_BASIC_FAULT_SHARED, 0x10A6U, 0x10A9U, 0x1096U, 0,
		    0x45F7U, 2U},
		{YT_BASIC_FAULT_SHARED, 0x10B4U, 0x10B7U, 0x10B1U, 0,
		    0x45F7U, 2U},
		{YT_BASIC_FAULT_SHARED, 0x1105U, 0x1108U, 0x1102U, 0,
		    0x45F7U, 2U},
		{YT_BASIC_FAULT_SHARED, 0x1156U, 0x1159U, 0x113EU, 0,
		    0x45F7U, 0U},
		{YT_BASIC_FAULT_SHARED, 0x134CU, 0x134FU, 0x1349U, 0,
		    0x45F7U, 2U},
		{YT_BASIC_FAULT_SHARED, 0x136CU, 0x136FU, 0x1369U, 0,
		    0x45F7U, 2U},
		{YT_BASIC_FAULT_SHARED, 0x13ADU, 0x13B0U, 0x13AAU, 0,
		    0x45F7U, 2U},
		{YT_BASIC_FAULT_MAIN, 0x90E9U, 0x90ECU, 0x90E1U, 33880,
		    0xB2DAU, 2U},
		{YT_BASIC_FAULT_MAIN, 0x9123U, 0x9126U, 0x9120U, 33880,
		    0xB2DAU, 2U},
		{YT_BASIC_FAULT_MAIN, 0x92E1U, 0x92E4U, 0x92CCU, 33890,
		    0xB2DAU, 0U},
		{YT_BASIC_FAULT_MAIN, 0x025CU, 0x025FU, 0x0259U, 60,
		    0xB2DAU, 2U},
		{YT_BASIC_FAULT_SHARED, 0x1584U, 0x1587U, 0x1579U, 0,
		    0x45F7U, 3U},
		{YT_BASIC_FAULT_SHARED, 0x163CU, 0x163FU, 0x1631U, 0,
		    0x45F7U, 4U},
		{YT_BASIC_FAULT_SHARED, 0x17E7U, 0x17EAU, 0x17DFU, 0,
		    0x45F7U, 3U},
		{YT_BASIC_FAULT_SHARED, 0x180AU, 0x180DU, 0x1804U, 0,
		    0x45F7U, 2U},
		{YT_BASIC_FAULT_SHARED, 0x1812U, 0x1815U, 0x1804U, 0,
		    0x45F7U, 5U},
	};
	struct yt_error error;
	size_t index;

	CHECK(YT_ARRAY_LEN(expected) == YT_BASIC_FAULT_SITE_COUNT);
	for (index = 0U; index < YT_ARRAY_LEN(expected); ++index) {
		const struct yt_basic_fault_identity *identity =
		    yt_basic_fault_identity((enum yt_basic_fault_site)index);
		unsigned error_number;

		CHECK(identity != NULL && identity->name != NULL
		    && identity->module == expected[index].module
		    && identity->instruction == expected[index].instruction
		    && identity->saved_ip == expected[index].saved_ip
		    && identity->retry_statement
		    == expected[index].retry_statement
		    && identity->source_line == expected[index].source_line
		    && identity->handler == expected[index].handler);
		if (expected[index].domain == 2U) {
			CHECK(identity->error_count == 1U
			    && yt_basic_fault_admits(
			    (enum yt_basic_fault_site)index, 6U)
			    && !yt_basic_fault_admits(
			    (enum yt_basic_fault_site)index, 57U));
		}
		else if (expected[index].domain < 3U) {
			CHECK(identity->error_count
			    == (expected[index].domain == 1U ? 7U : 6U)
			    && yt_basic_fault_admits(
			    (enum yt_basic_fault_site)index, 5U)
			    && yt_basic_fault_admits(
			    (enum yt_basic_fault_site)index, 57U)
			    && yt_basic_fault_admits(
			    (enum yt_basic_fault_site)index, 75U)
			    && (yt_basic_fault_admits(
			    (enum yt_basic_fault_site)index, 61U)
			    == (expected[index].domain == 1U)));
		}
		else if (expected[index].domain < 5U) {
			CHECK(identity->error_count
			    == (expected[index].domain == 4U ? 5U : 4U)
			    && !yt_basic_fault_admits(
			    (enum yt_basic_fault_site)index, 5U)
			    && yt_basic_fault_admits(
			    (enum yt_basic_fault_site)index, 52U)
			    && yt_basic_fault_admits(
			    (enum yt_basic_fault_site)index, 57U)
			    && yt_basic_fault_admits(
			    (enum yt_basic_fault_site)index, 75U)
			    && (yt_basic_fault_admits(
			    (enum yt_basic_fault_site)index, 61U)
			    == (expected[index].domain == 4U)));
		}
		else {
			CHECK(identity->error_count == 3U
			    && yt_basic_fault_admits(
			    (enum yt_basic_fault_site)index, 5U)
			    && yt_basic_fault_admits(
			    (enum yt_basic_fault_site)index, 14U)
			    && yt_basic_fault_admits(
			    (enum yt_basic_fault_site)index, 16U)
			    && !yt_basic_fault_admits(
			    (enum yt_basic_fault_site)index, 6U));
		}
		for (error_number = 0U; error_number <= UINT8_MAX;
		    ++error_number) {
			static const uint8_t get_errors[] = {
				5U, 52U, 57U, 63U, 70U, 75U,
			};
			static const uint8_t put_errors[] = {
				5U, 52U, 57U, 61U, 63U, 70U, 75U,
			};
			static const uint8_t returning_get_errors[] = {
				52U, 57U, 70U, 75U,
			};
			static const uint8_t returning_put_errors[] = {
				52U, 57U, 61U, 70U, 75U,
			};
			static const uint8_t left_errors[] = {5U, 14U, 16U};
			struct yt_basic_fault_projection projection;
			const uint8_t *domain = expected[index].domain == 1U
			    ? put_errors : expected[index].domain == 2U
			    ? (const uint8_t[]){6U} : expected[index].domain == 3U
			    ? returning_get_errors : expected[index].domain == 4U
			    ? returning_put_errors : expected[index].domain == 5U
			    ? left_errors : get_errors;
			size_t domain_length = expected[index].domain == 1U
			    ? YT_ARRAY_LEN(put_errors) : expected[index].domain == 2U
			    ? 1U : expected[index].domain == 3U
			    ? YT_ARRAY_LEN(returning_get_errors)
			    : expected[index].domain == 4U
			    ? YT_ARRAY_LEN(returning_put_errors)
			    : expected[index].domain == 5U
			    ? YT_ARRAY_LEN(left_errors)
			    : YT_ARRAY_LEN(get_errors);
			bool admitted = false;
			size_t error_index;

			for (error_index = 0U; error_index < domain_length;
			    ++error_index)
				if (domain[error_index] == error_number)
					admitted = true;
			CHECK(yt_basic_fault_admits(
			    (enum yt_basic_fault_site)index,
			    (uint8_t)error_number) == admitted);
			if (!admitted)
				continue;
			yt_error_clear(&error);
			CHECK(yt_error_attach_basic_fault_number(&error,
			    (enum yt_basic_fault_site)index,
			    (uint16_t)error_number));
			CHECK(yt_basic_fault_project(&error, NULL, 0U, NULL, 0U,
			    NULL, 0U, &projection));
			CHECK(projection.identity == identity
			    && projection.error_number == error_number);
			if (identity->module == YT_BASIC_FAULT_SHARED) {
				CHECK(projection.disposition == YT_BASIC_FAULT_END
				    && projection.shared.ends);
			}
			else if (error_number == 57U) {
				CHECK(projection.disposition
				    == YT_BASIC_FAULT_RETRY_STATEMENT
				    && projection.main.route
				    == YT_MAIN_ERROR_RETRY_CURRENT);
			}
			else if (error_number == 5U || error_number == 6U
			    || error_number == 13U || error_number == 15U) {
				CHECK(projection.disposition
				    == YT_BASIC_FAULT_RESUME_GAMEPLAY
				    && projection.main.route
				    == YT_MAIN_ERROR_GAMEPLAY);
			}
			else {
				CHECK(projection.disposition == YT_BASIC_FAULT_END
				    && projection.main.route == YT_MAIN_ERROR_FATAL);
			}
		}
	}
	CHECK(yt_basic_fault_identity(YT_BASIC_FAULT_SITE_COUNT) == NULL
	    && !yt_basic_fault_admits(YT_BASIC_FAULT_SITE_COUNT, 5U));
	yt_error_clear(&error);
	CHECK(yt_error_attach_basic_fault(&error,
	    YT_BASIC_FAULT_ROUTE_FINAL_SECTOR_GET)
	    && error.basic_fault_valid
	    && error.basic_fault_site == YT_BASIC_FAULT_ROUTE_FINAL_SECTOR_GET
	    && !error.basic_error_valid
	    && !yt_error_attach_basic_fault(&error, YT_BASIC_FAULT_SITE_COUNT));
}

static void
test_basic_fault_projection(void)
{
	static const uint8_t date[] = "09-02-2026";
	static const uint8_t time_text[] = "12:34:56";
	struct yt_basic_fault_projection projection;
	struct yt_error error;

	yt_error_clear(&error);
	CHECK(yt_error_attach_basic_fault_number(&error,
	    YT_BASIC_FAULT_ROUTE_FINAL_SECTOR_GET, 57U)
	    && error.basic_fault_valid && error.basic_error_valid
	    && error.basic_error == 57U
	    && yt_basic_fault_project(&error, NULL, 0U, date,
	    sizeof(date) - 1U, time_text, sizeof(time_text) - 1U, &projection)
	    && projection.disposition == YT_BASIC_FAULT_RETRY_STATEMENT
	    && projection.identity->saved_ip == 0x92E4U
	    && projection.identity->retry_statement == 0x92CCU
	    && projection.main.route == YT_MAIN_ERROR_RETRY_CURRENT);

	yt_error_clear(&error);
	CHECK(yt_error_attach_basic_fault_number(&error,
	    YT_BASIC_FAULT_ROUTE_DISPLAY_VERTEX_CINT, 6U)
	    && yt_basic_fault_project(&error, NULL, 0U, date,
	    sizeof(date) - 1U, time_text, sizeof(time_text) - 1U, &projection)
	    && projection.disposition == YT_BASIC_FAULT_RESUME_GAMEPLAY
	    && projection.identity->source_line == 33880
	    && projection.main.route == YT_MAIN_ERROR_GAMEPLAY);

	yt_error_clear(&error);
	CHECK(yt_error_attach_basic_fault_number(&error,
	    YT_BASIC_FAULT_NORMAL_EXIT_REGISTERED_CINT, 6U)
	    && yt_basic_fault_project(&error, NULL, 0U, date,
	    sizeof(date) - 1U, time_text, sizeof(time_text) - 1U, &projection)
	    && projection.disposition == YT_BASIC_FAULT_RESUME_GAMEPLAY
	    && projection.identity->saved_ip == 0x025FU
	    && projection.identity->retry_statement == 0x0259U
	    && projection.identity->source_line == 60
	    && projection.main.route == YT_MAIN_ERROR_GAMEPLAY);

	yt_error_clear(&error);
	CHECK(yt_error_attach_basic_fault_number(&error,
	    YT_BASIC_FAULT_PORT_SELECTED_SECTOR_GET, 52U)
	    && yt_basic_fault_project(&error, NULL, 0U, date,
	    sizeof(date) - 1U, time_text, sizeof(time_text) - 1U, &projection)
	    && projection.disposition == YT_BASIC_FAULT_END
	    && projection.main.route == YT_MAIN_ERROR_FATAL
	    && projection.identity->retry_statement == 0x8DFCU);

	yt_error_clear(&error);
	CHECK(yt_error_attach_basic_fault_number(&error,
	    YT_BASIC_FAULT_PORT_FRIENDSHIP_CURRENT_GET, 57U)
	    && yt_basic_fault_project(&error, NULL, 0U, date,
	    sizeof(date) - 1U, time_text, sizeof(time_text) - 1U, &projection)
	    && projection.disposition == YT_BASIC_FAULT_END
	    && projection.shared.route == YT_SHARED_ERROR_DORINFO_COM
	    && projection.shared.ends
	    && projection.identity->retry_statement == 0x97BBU);

	yt_error_clear(&error);
	CHECK(yt_error_attach_basic_fault_number(&error,
	    YT_BASIC_FAULT_ROUTE_START_FIFO_CINT, 6U)
	    && yt_basic_fault_project(&error, NULL, 0U, date,
	    sizeof(date) - 1U, time_text, sizeof(time_text) - 1U, &projection)
	    && projection.disposition == YT_BASIC_FAULT_END
	    && projection.shared.route == YT_SHARED_ERROR_GENERIC
	    && projection.identity->retry_statement == 0x103CU);

	yt_error_clear(&error);
	CHECK(!yt_error_attach_basic_fault_number(&error,
	    YT_BASIC_FAULT_PORT_SELECTED_SECTOR_GET, 61U)
	    && !error.basic_fault_valid && !error.basic_error_valid
	    && !yt_basic_fault_project(&error, NULL, 0U, date,
	    sizeof(date) - 1U, time_text, sizeof(time_text) - 1U, &projection));
	CHECK(!yt_error_attach_basic_fault_number(&error,
	    YT_BASIC_FAULT_SITE_COUNT, 6U));
	CHECK(yt_error_attach_basic_fault(&error,
	    YT_BASIC_FAULT_ROUTE_FINAL_SECTOR_GET)
	    && error.basic_fault_valid && !error.basic_error_valid
	    && error.basic_error == 0U);
}

static void
test_main_error_model(void)
{
	static const uint8_t debug[] =
	    "YT DEBUG Error Trap Entry ERL=  40000   ERR=  53 ";
	static const uint8_t missing[] =
	    "*** GAME FILE [YTOPEN.Asc] NOT FOUND! ***";
	static const uint8_t fatal[] =
	    "YTMerg2 1.15 Untrapped Error ERL= 12345 ERR= 11 "
	    "Date >07-23-2026 14:05:09";
	static const int16_t gameplay_errors[] = {5, 6, 13, 15};
	uint8_t long_path[YT_MAIN_ERROR_TEXT];
	struct yt_main_error_result error;
	struct yt_present_result presentation;
	size_t index;

	CHECK(yt_main_error_compose(24, 40000, NULL, 1U, NULL, 1U,
	    NULL, 1U, &error));
	CHECK(error.route == YT_MAIN_ERROR_RETRY_CURRENT
	    && error.debug_length == 0U && error.action_length == 0U);
	CHECK(yt_main_error_compose(57, 12345, NULL, 1U, NULL, 1U,
	    NULL, 1U, &error));
	CHECK(error.route == YT_MAIN_ERROR_RETRY_CURRENT
	    && error.debug_length == 0U && error.action_length == 0U);

	CHECK(yt_main_error_compose(53, 40000,
	    (const uint8_t *)"YTOPEN.Asc", strlen("YTOPEN.Asc"),
	    NULL, 1U, NULL, 1U, &error));
	CHECK(error.route == YT_MAIN_ERROR_MISSING_FILE
	    && error.debug_length == sizeof(debug) - 1U
	    && memcmp(error.debug, debug, sizeof(debug) - 1U) == 0
	    && error.action_length == sizeof(missing) - 1U
	    && memcmp(error.action, missing, sizeof(missing) - 1U) == 0);
	for (index = 0; index < YT_ARRAY_LEN(gameplay_errors); ++index) {
		CHECK(yt_main_error_compose(gameplay_errors[index], 40000,
		    (const uint8_t *)"ytinstr.doc", strlen("ytinstr.doc"),
		    NULL, 1U, NULL, 1U, &error));
		CHECK(error.route == YT_MAIN_ERROR_MISSING_FILE);
		CHECK(yt_main_error_compose(gameplay_errors[index], 12345,
		    NULL, 1U, NULL, 1U, NULL, 1U, &error));
		CHECK(error.route == YT_MAIN_ERROR_GAMEPLAY
		    && error.debug_length != 0U && error.action_length == 0U);
	}

	CHECK(yt_main_error_compose(11, 12345, NULL, 0U,
	    (const uint8_t *)"07-23-2026", strlen("07-23-2026"),
	    (const uint8_t *)"14:05:09", strlen("14:05:09"), &error));
	CHECK(error.route == YT_MAIN_ERROR_FATAL
	    && error.action_length == sizeof(fatal) - 1U
	    && memcmp(error.action, fatal, sizeof(fatal) - 1U) == 0);
	CHECK(yt_present_forced_local_line(error.debug, error.debug_length,
	    &presentation) == YT_PRESENT_OK);
	CHECK(presentation.remote_length == 0U
	    && presentation.event_count == 1U
	    && presentation.events[0].operation == YT_PRESENT_LOCAL_LINE
	    && presentation.events[0].length == error.debug_length
	    && memcmp(presentation.events[0].data, error.debug,
	    error.debug_length) == 0);
	CHECK(yt_main_error_compose(-1, -2, NULL, 0U,
	    (const uint8_t *)"D", 1U, (const uint8_t *)"T", 1U, &error));
	CHECK(error.route == YT_MAIN_ERROR_FATAL
	    && error.action_length
	    == strlen("YTMerg2 1.15 Untrapped Error ERL=-2 ERR=-1 Date >D T")
	    && memcmp(error.action,
	    "YTMerg2 1.15 Untrapped Error ERL=-2 ERR=-1 Date >D T",
	    error.action_length) == 0);
	CHECK(!yt_main_error_compose(11, 12345, NULL, 0U, NULL, 1U,
	    (const uint8_t *)"T", 1U, &error));
	memset(long_path, 'X', sizeof(long_path));
	CHECK(!yt_main_error_compose(53, 40000, long_path,
	    sizeof(long_path), NULL, 0U, NULL, 0U, &error));
	CHECK(!yt_main_error_compose(11, 12345, NULL, 0U, NULL, 0U,
	    NULL, 0U, NULL));
}

static void
test_shared_error_model(void)
{
	static const int32_t special_lines[] = {
		38100, 630, 2710, 64001, 64004, 64005, 64006
	};
	static const struct {
		int32_t source_line;
		enum yt_shared_error_route route;
		const char *message;
	} diagnostics[] = {
		{630, YT_SHARED_ERROR_DATA_OPEN,
		    "Error opening ytDATA.DAT"},
		{2710, YT_SHARED_ERROR_ANSI_OPEN,
		    "Please Create YTOPEN.ANS for ANSI graphics users!"},
		{64001, YT_SHARED_ERROR_RANKINGS_FILESPEC,
		    "Error with Player Rankings filespec"},
		{64004, YT_SHARED_ERROR_ALIAS_FILE,
		    "Error with YTNAME.DAT file!"},
		{64005, YT_SHARED_ERROR_ALIAS_FILE,
		    "Error with YTNAME.DAT file!"},
	};
	static const uint8_t debug[] =
	    "YT-SUB DEBUG Error Trap Entry ERL=  2710 ERR= 53 ";
	static const uint8_t negative_debug[] =
	    "YT-SUB DEBUG Error Trap Entry ERL= -1 ERR=-2 ";
	static const uint8_t autopilot[] =
	    " *** Not Enough System Memory for Autopilot Function! ***";
	static const uint8_t generic[] =
	    "Untrapped YT-SUB Error>  5 Line>  1234";
	struct yt_shared_error_result error;
	struct yt_present_result presentation;
	struct yt_present_state current = state(false);
	size_t index;

	for (index = 0; index < YT_ARRAY_LEN(special_lines); ++index) {
		CHECK(yt_shared_error_compose(24, special_lines[index], &error));
		CHECK(error.route == YT_SHARED_ERROR_RETRY_CURRENT
		    && error.debug_length == 0U && error.event_count == 0U
		    && !error.ends);
	}
	CHECK(yt_shared_error_compose(53, 2710, &error));
	CHECK(error.debug_length == sizeof(debug) - 1U
	    && memcmp(error.debug, debug, sizeof(debug) - 1U) == 0);
	CHECK(yt_shared_error_compose(-2, -1, &error));
	CHECK(error.debug_length == sizeof(negative_debug) - 1U
	    && memcmp(error.debug, negative_debug,
	    sizeof(negative_debug) - 1U) == 0);

	for (index = 0; index < YT_ARRAY_LEN(diagnostics); ++index) {
		CHECK(yt_shared_error_compose(7, diagnostics[index].source_line,
		    &error));
		CHECK(error.route == diagnostics[index].route && error.ends
		    && error.event_count == 1U
		    && error.events[0].destination
		    == YT_SHARED_ERROR_LOCAL_DIAGNOSTIC
		    && error.events[0].length == strlen(diagnostics[index].message)
		    && memcmp(error.events[0].data, diagnostics[index].message,
		    error.events[0].length) == 0);
	}

	CHECK(yt_shared_error_compose(7, 38100, &error));
	CHECK(error.route == YT_SHARED_ERROR_AUTOPILOT_MEMORY && error.ends
	    && error.event_count == 1U
	    && error.events[0].destination
	    == YT_SHARED_ERROR_SESSION_AND_NEWS
	    && error.events[0].length == sizeof(autopilot) - 1U
	    && memcmp(error.events[0].data, autopilot,
	    sizeof(autopilot) - 1U) == 0);
	CHECK(yt_present_line(error.events[0].data, error.events[0].length,
	    &current, &presentation) == YT_PRESENT_OK);
	CHECK(presentation.remote_length == sizeof(autopilot) + 1U
	    && presentation.event_count == 3U
	    && presentation.events[0].operation == YT_PRESENT_LOCAL_LINE);

	CHECK(yt_shared_error_compose(53, 64006, &error));
	CHECK(error.route == YT_SHARED_ERROR_DORINFO_COM
	    && error.event_count == 2U
	    && error.events[1].length == strlen("DORINFO not found!")
	    && memcmp(error.events[1].data, "DORINFO not found!",
	    error.events[1].length) == 0);
	CHECK(yt_shared_error_compose(64, 64006, &error));
	CHECK(error.event_count == 2U
	    && error.events[1].length == strlen("Error opening the COM Port!")
	    && memcmp(error.events[1].data, "Error opening the COM Port!",
	    error.events[1].length) == 0);
	CHECK(yt_shared_error_compose(5, 64006, &error));
	CHECK(error.event_count == 1U);

	CHECK(yt_shared_error_compose(5, 1234, &error));
	CHECK(error.route == YT_SHARED_ERROR_GENERIC && error.ends
	    && error.event_count == 3U
	    && error.events[0].destination == YT_SHARED_ERROR_NEWS
	    && error.events[0].length == sizeof(generic) - 1U
	    && memcmp(error.events[0].data, generic,
	    sizeof(generic) - 1U) == 0
	    && error.events[1].length
	    == strlen("Please record error and circumstances. Also, if the error")
	    && error.events[2].length
	    == strlen("is Severe, Please inform Alan Davenport!"));
	for (index = 0; index < error.event_count; ++index)
		CHECK(error.events[index].destination == YT_SHARED_ERROR_NEWS);
	CHECK(yt_shared_error_compose(5, 64003, &error)
	    && error.route == YT_SHARED_ERROR_GENERIC);
	CHECK(yt_shared_error_compose(5, 64006, &error)
	    && error.route == YT_SHARED_ERROR_DORINFO_COM);

	CHECK(yt_shared_error_compose(53, 2710, &error));
	current.sound.snoop = 0.0f;
	CHECK(yt_present_local_line(error.debug, error.debug_length, &current,
	    &presentation) == YT_PRESENT_OK);
	CHECK(presentation.remote_length == 0U
	    && presentation.event_count == 0U);
	current.sound.snoop = -1.0f;
	CHECK(yt_present_local_line(error.events[0].data,
	    error.events[0].length, &current, &presentation) == YT_PRESENT_OK);
	CHECK(presentation.remote_length == 0U
	    && presentation.event_count == 1U
	    && presentation.events[0].operation == YT_PRESENT_LOCAL_LINE);
	CHECK(!yt_shared_error_compose(5, 1234, NULL));
}

static void
test_serial_startup_output(void)
{
	static const char carrier[] =
	    "(**CARRIER DROPPED**) Returning to bbs!";
	static const char *missing_rows[] = {
		"",
		"Command line missing! Aborting!",
		"",
		"BBS usage: YT.EXE C:\\BBS\\DORINFO1.DEF",
	};
	static const struct {
		int port;
		float baud;
		const char *expected;
	} status[] = {
		{1, 38400.0f, "Opening COM port 1 at 38400 baud"},
		{3, 57600.0f, "Opening COM port 3 at 57600 baud"},
		{4, 115200.0f, "Opening COM port 4 at 115200 baud"},
		{0, 0.0f, "Local Console Mode"},
		{5, 0.0f, "Local Console Mode"},
		{-1, 0.0f, "Local Console Mode"},
	};
	struct yt_present_result result;
	struct yt_present_state current = state(false);
	size_t index;

	CHECK(yt_present_serial_startup_missing_command(&result)
	    == YT_PRESENT_OK);
	CHECK(result.remote_length == 0U && result.event_count == 4U);
	for (index = 0; index < YT_ARRAY_LEN(missing_rows); ++index) {
		CHECK(result.events[index].operation == YT_PRESENT_LOCAL_LINE
		    && result.events[index].length == strlen(missing_rows[index])
		    && memcmp(result.events[index].data, missing_rows[index],
		    result.events[index].length) == 0);
	}
	for (index = 0; index < YT_ARRAY_LEN(status); ++index) {
		CHECK(yt_present_serial_startup_status(status[index].port,
		    status[index].baud, &result) == YT_PRESENT_OK);
		CHECK(result.remote_length == 0U && result.event_count == 1U
		    && result.events[0].operation == YT_PRESENT_LOCAL_LINE
		    && result.events[0].length == strlen(status[index].expected)
		    && memcmp(result.events[0].data, status[index].expected,
		    result.events[0].length) == 0);
	}
	CHECK(yt_present_carrier_drop(&current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == 0U && result.event_count == 1U
	    && result.events[0].operation == YT_PRESENT_LOCAL_LINE
	    && result.events[0].length == sizeof(carrier) - 1U
	    && memcmp(result.events[0].data, carrier,
	    sizeof(carrier) - 1U) == 0);
	current.sound.snoop = 0.0f;
	CHECK(yt_present_carrier_drop(&current, &result) == YT_PRESENT_OK
	    && result.remote_length == 0U && result.event_count == 0U);
	CHECK(yt_present_serial_startup_missing_command(NULL)
	    == YT_PRESENT_CAPACITY);
	CHECK(yt_present_serial_startup_status(1, 38400.0f, NULL)
	    == YT_PRESENT_CAPACITY);
	CHECK(yt_present_carrier_drop(NULL, &result) == YT_PRESENT_CAPACITY);
	CHECK(yt_present_carrier_drop(&current, NULL) == YT_PRESENT_CAPACITY);
}

static void
test_time_helpers(void)
{
	struct yt_present_state current = state(true);
	struct yt_present_time_state time;
	struct yt_present_result result;
	uint8_t long_time[YT_PRESENT_EVENT_DATA - 9U];
	uint8_t deadline_raw[4];
	uint8_t next_refresh_raw[4];
	uint8_t raw_remembered[4];
	uint8_t expected_raw[4];
	uint8_t before_raw[4];
	static const float update_reads[] = {100, 100, 100, 100};
	static const float gated_reads[] = {100, 100};
	static const float rollover_reads[] = {1, 10, 10};
	static const float warning_reads[] = {100, 100, 100, 100};
	size_t used;
	bool updated;
	bool warned;
	float remembered = 6.0f;

	memset(&time, 0, sizeof(time));
	time.deadline = 460.0f;
	memcpy(time.text, " 6:00  ", 7);
	time.text_length = 7;
	CHECK(yt_present_refresh_time(&time, update_reads,
	    sizeof(update_reads) / sizeof(update_reads[0]), &used, 4, 9,
	    &current, &result, &updated) == YT_PRESENT_OK);
	CHECK(updated && used == 4);
	CHECK(time.next_refresh == 101.0f);
	CHECK(time.text_length == 7
	    && memcmp(time.text, " 6:00  ", 7) == 0);
	CHECK(time.saved_row == 4 && time.saved_column == 9);
	CHECK(result.event_count == 5);
	CHECK(result.events[0].operation == YT_PRESENT_LOCAL_LOCATE
	    && result.events[0].row == 25 && result.events[0].column == 71);
	CHECK(result.events[1].operation == YT_PRESENT_LOCAL_COLOR
	    && result.events[1].foreground == 11
	    && result.events[1].background == 1);
	CHECK(result.events[2].operation == YT_PRESENT_LOCAL_SEMI);
	CHECK(result.events[3].operation == YT_PRESENT_LOCAL_LOCATE
	    && result.events[3].row == 4 && result.events[3].column == 9
	    && result.events[3].cursor_visible == 1
	    && result.events[3].cursor_start == 1
	    && result.events[3].cursor_stop == 16);
	CHECK(result.events[4].operation == YT_PRESENT_LOCAL_COLOR
	    && result.events[4].foreground == 7);

	memset(&time, 0, sizeof(time));
	time.deadline = 460.0f;
	time.next_refresh = 101.0f;
	CHECK(yt_present_refresh_time(&time, gated_reads,
	    sizeof(gated_reads) / sizeof(gated_reads[0]), &used, 2, 3,
	    &current, &result, &updated) == YT_PRESENT_OK);
	CHECK(!updated && used == 2 && result.event_count == 0);
	CHECK(time.deadline == 460.0f && time.next_refresh == 101.0f);

	memset(&time, 0, sizeof(time));
	time.deadline = 80000.0f;
	CHECK(yt_present_refresh_time(&time, rollover_reads,
	    sizeof(rollover_reads) / sizeof(rollover_reads[0]), &used, 1, 1,
	    &current, &result, &updated) == YT_PRESENT_OK);
	CHECK(!updated && used == 3);
	CHECK(time.deadline == -6400.0f && time.next_refresh == 11.0f);
	CHECK(result.event_count == 0);

	current.sound.snoop = 0.0f;
	memset(&time, 0, sizeof(time));
	time.deadline = 430.0f;
	memcpy(time.text, " 4:59  ", 7);
	time.text_length = 7;
	CHECK(yt_present_refresh_time(&time, warning_reads,
	    sizeof(warning_reads) / sizeof(warning_reads[0]), &used, 8, 12,
	    &current, &result, &updated) == YT_PRESENT_OK);
	CHECK(updated && used == 4 && time.next_refresh == 101.0f);
	CHECK(time.text_length == 7
	    && memcmp(time.text, " 5:30  ", 7) == 0);
	CHECK(time.saved_row == 8 && time.saved_column == 12);
	CHECK(result.remote_length == 0 && result.event_count == 5);
	CHECK(result.events[0].operation == YT_PRESENT_LOCAL_LOCATE
	    && result.events[0].row == 25 && result.events[0].column == 71);
	CHECK(result.events[1].operation == YT_PRESENT_LOCAL_COLOR
	    && result.events[1].foreground == 11
	    && result.events[1].background == 1);
	CHECK(result.events[2].operation == YT_PRESENT_LOCAL_COLOR
	    && result.events[2].foreground == 12
	    && result.events[2].background == 1);
	CHECK(result.events[3].operation == YT_PRESENT_LOCAL_LOCATE
	    && result.events[3].row == 8 && result.events[3].column == 12
	    && result.events[3].cursor_visible == 1
	    && result.events[3].cursor_start == 1
	    && result.events[3].cursor_stop == 16);
	CHECK(result.events[4].operation == YT_PRESENT_LOCAL_COLOR
	    && result.events[4].foreground == 7
	    && result.events[4].background == 0);

	{
		static const uint8_t dirty_zero[] = {0xa5, 0x5a, 0x80, 0x00};
		static const uint8_t raw_101[] = {0x00, 0x00, 0x4a, 0x87};

		memcpy(deadline_raw, dirty_zero, sizeof(deadline_raw));
		memcpy(next_refresh_raw, raw_101, sizeof(next_refresh_raw));
		memset(&time, 0, sizeof(time));
		CHECK(yt_present_refresh_time_process(&time, deadline_raw,
		    next_refresh_raw, gated_reads,
		    sizeof(gated_reads) / sizeof(gated_reads[0]), &used, 2, 3,
		    &current, &result, &updated) == YT_PRESENT_OK);
		CHECK(!updated && used == 2 && result.event_count == 0);
		CHECK(memcmp(deadline_raw, dirty_zero, sizeof(deadline_raw)) == 0
		    && memcmp(next_refresh_raw, raw_101,
		    sizeof(next_refresh_raw)) == 0);
	}

	{
		static const uint8_t raw_460[] = {0x00, 0x00, 0x66, 0x89};
		static const uint8_t dirty_zero[] = {0xff, 0xff, 0x80, 0x00};
		static const uint8_t raw_101[] = {0x00, 0x00, 0x4a, 0x87};
		static const uint8_t raw_4[] = {0x00, 0x00, 0x00, 0x83};
		static const uint8_t raw_6[] = {0x00, 0x00, 0x40, 0x83};
		static const uint8_t raw_9[] = {0x00, 0x00, 0x10, 0x84};
		uint8_t saved_row[4] = {0xde, 0xad, 0xbe, 0x00};
		uint8_t saved_column[4] = {0xca, 0xfe, 0xba, 0x00};
		uint8_t remaining[4] = {0x12, 0x34, 0x56, 0x00};

		memcpy(deadline_raw, raw_460, sizeof(deadline_raw));
		memcpy(next_refresh_raw, dirty_zero, sizeof(next_refresh_raw));
		memset(&time, 0, sizeof(time));
		yt_present_bind_time_process_cells(&time, saved_row,
		    saved_column, remaining);
		memcpy(time.text, " 6:00  ", 7U);
		time.text_length = 7U;
		CHECK(yt_present_refresh_time_process(&time, deadline_raw,
		    next_refresh_raw, update_reads,
		    sizeof(update_reads) / sizeof(update_reads[0]), &used, 4, 9,
		    &current, &result, &updated) == YT_PRESENT_OK);
		CHECK(updated && used == 4);
		CHECK(memcmp(deadline_raw, raw_460, sizeof(deadline_raw)) == 0
		    && memcmp(next_refresh_raw, raw_101,
		    sizeof(next_refresh_raw)) == 0);
		CHECK(memcmp(saved_row, raw_4, sizeof(saved_row)) == 0
		    && memcmp(saved_column, raw_9, sizeof(saved_column)) == 0
		    && memcmp(remaining, raw_6, sizeof(remaining)) == 0);
	}

	{
		static const uint8_t raw_80000[] = {0x00, 0x40, 0x1c, 0x91};
		static const uint8_t dirty_zero[] = {0x12, 0x34, 0x80, 0x00};
		static const uint8_t raw_negative_6400[] = {
			0x00, 0x00, 0xc8, 0x8d
		};
		static const uint8_t raw_11[] = {0x00, 0x00, 0x30, 0x84};

		memcpy(deadline_raw, raw_80000, sizeof(deadline_raw));
		memcpy(next_refresh_raw, dirty_zero, sizeof(next_refresh_raw));
		memset(&time, 0, sizeof(time));
		CHECK(yt_present_refresh_time_process(&time, deadline_raw,
		    next_refresh_raw, rollover_reads,
		    sizeof(rollover_reads) / sizeof(rollover_reads[0]), &used, 1,
		    1, &current, &result, &updated) == YT_PRESENT_OK);
		CHECK(!updated && used == 3 && result.event_count == 0);
		CHECK(memcmp(deadline_raw, raw_negative_6400,
		    sizeof(deadline_raw)) == 0
		    && memcmp(next_refresh_raw, raw_11,
		    sizeof(next_refresh_raw)) == 0);

		memcpy(deadline_raw, raw_80000, sizeof(deadline_raw));
		memcpy(next_refresh_raw, dirty_zero, sizeof(next_refresh_raw));
		memset(&time, 0, sizeof(time));
		CHECK(yt_present_refresh_time_process(&time, deadline_raw,
		    next_refresh_raw, rollover_reads, 1U, &used, 1, 1, &current,
		    &result, &updated) == YT_PRESENT_TIMER_EXHAUSTED);
		CHECK(!updated && used == 1);
		CHECK(memcmp(deadline_raw, raw_negative_6400,
		    sizeof(deadline_raw)) == 0
		    && memcmp(next_refresh_raw, dirty_zero,
		    sizeof(next_refresh_raw)) == 0);
	}

	{
		static const uint8_t raw_460[] = {0x00, 0x00, 0x66, 0x89};
		static const uint8_t dirty_zero[] = {0x7f, 0x55, 0x80, 0x00};
		static const uint8_t raw_101[] = {0x00, 0x00, 0x4a, 0x87};
		static const uint8_t raw_4[] = {0x00, 0x00, 0x00, 0x83};
		static const uint8_t raw_9[] = {0x00, 0x00, 0x10, 0x84};
		static const uint8_t old_remaining[] = {0x12, 0x34, 0x56, 0x00};
		uint8_t saved_row[4] = {0};
		uint8_t saved_column[4] = {0};
		uint8_t remaining[4];

		memcpy(deadline_raw, raw_460, sizeof(deadline_raw));
		memcpy(next_refresh_raw, dirty_zero, sizeof(next_refresh_raw));
		memcpy(remaining, old_remaining, sizeof(remaining));
		memset(&time, 0, sizeof(time));
		yt_present_bind_time_process_cells(&time, saved_row,
		    saved_column, remaining);
		memcpy(time.text, " 6:00  ", 7U);
		time.text_length = 7U;
		CHECK(yt_present_refresh_time_process(&time, deadline_raw,
		    next_refresh_raw, update_reads, 3U, &used, 4, 9, &current,
		    &result, &updated) == YT_PRESENT_TIMER_EXHAUSTED);
		CHECK(!updated && used == 3 && result.event_count == 2);
		CHECK(memcmp(deadline_raw, raw_460, sizeof(deadline_raw)) == 0
		    && memcmp(next_refresh_raw, raw_101,
		    sizeof(next_refresh_raw)) == 0
		    && memcmp(saved_row, raw_4, sizeof(saved_row)) == 0
		    && memcmp(saved_column, raw_9, sizeof(saved_column)) == 0
		    && memcmp(remaining, old_remaining, sizeof(remaining)) == 0);
	}

	current = state(true);
	CHECK(yt_present_low_time((const uint8_t *)" 5:59  ", 7,
	    &remembered, &current, &result, &warned) == YT_PRESENT_OK);
	CHECK(warned && remembered == 5.0f);
	CHECK(result.event_count == 16);
	CHECK(result.events[5].operation == YT_PRESENT_REMOTE_SEMI
	    && result.events[5].length == 1
	    && result.events[5].data[0] == '\a');
	CHECK(result.events[8].operation == YT_PRESENT_LOCAL_LINE
	    && result.events[8].length == 17
	    && memcmp(result.events[8].data, "Time Left: 5:59  ", 17) == 0);
	CHECK(current.bold == 0.0f && current.blink == 0.0f);
	CHECK(yt_present_low_time((const uint8_t *)" 5:59  ", 7,
	    &remembered, &current, &result, &warned) == YT_PRESENT_OK);
	CHECK(!warned && result.event_count == 0);

	current = state(false);
	current.sound.mode = 1.0f;
	remembered = 6.0f;
	CHECK(yt_present_low_time((const uint8_t *)" 5:59  ", 7,
	    &remembered, &current, &result, &warned) == YT_PRESENT_OK);
	CHECK(warned && result.event_count == 4);
	CHECK(result.events[1].operation == YT_PRESENT_LOCAL_BEEP);
	CHECK(current.bold == 1.0f && current.blink == 1.0f);

	current = state(false);
	current.sound.snoop = 0.0f;
	remembered = 7.0f;
	CHECK(yt_present_low_time((const uint8_t *)" 6:00  ", 7,
	    &remembered, &current, &result, &warned) == YT_PRESENT_OK);
	CHECK(!warned && remembered == 6.0f && result.event_count == 0U
	    && result.remote_length == 0U);
	remembered = 6.0f;
	CHECK(yt_present_low_time((const uint8_t *)"5.9999999", 9,
	    &remembered, &current, &result, &warned) == YT_PRESENT_OK);
	CHECK(!warned && remembered == 6.0f && result.event_count == 0U);

	remembered = 6.0f;
	CHECK(yt_present_low_time((const uint8_t *)"5.9:00", 6,
	    &remembered, &current, &result, &warned) == YT_PRESENT_OK);
	CHECK(warned && remembered == (float)5.9
	    && result.remote_length != 0U);
	CHECK(yt_present_low_time((const uint8_t *)"5.9:00", 6,
	    &remembered, &current, &result, &warned) == YT_PRESENT_OK);
	CHECK(warned && remembered == (float)5.9
	    && result.remote_length != 0U);

	current = state(false);
	current.sound.snoop = 0.0f;
	remembered = 6.0f;
	memset(long_time, 'x', sizeof(long_time));
	long_time[0] = '5';
	CHECK(yt_present_low_time(long_time, sizeof(long_time), &remembered,
	    &current, &result, &warned) == YT_PRESENT_CAPACITY);
	CHECK(!warned && remembered == 5.0f && current.foreground == 5.0f
	    && current.blink == 1.0f && current.bold == 0.0f
	    && result.event_count == 3U && result.remote_length == 3U
	    && memcmp(result.remote, "\r\n\a", 3U) == 0);

	current = state(false);
	remembered = 6.0f;
	CHECK(yt_present_low_time((const uint8_t *)"2E38", 4, &remembered,
	    &current, &result, &warned) == YT_PRESENT_OVERFLOW);
	CHECK(!warned && remembered == 6.0f && current.foreground == 2.0f
	    && current.blink == 0.0f && result.event_count == 0U
	    && result.remote_length == 0U);

	current = state(false);
	CHECK(qb_mbf32_encode(7.0f, raw_remembered) == QB_MBF_OK
	    && qb_mbf32_encode(6.0f, expected_raw) == QB_MBF_OK);
	CHECK(yt_present_low_time_process((const uint8_t *)" 6:00  ", 7U,
	    raw_remembered, &current, &result, &warned) == YT_PRESENT_OK);
	CHECK(!warned
	    && memcmp(raw_remembered, expected_raw, sizeof(expected_raw)) == 0
	    && result.event_count == 0U && result.remote_length == 0U);

	raw_remembered[0] = 0x12U;
	raw_remembered[1] = 0x34U;
	raw_remembered[2] = 0x56U;
	raw_remembered[3] = 0x00U;
	memcpy(before_raw, raw_remembered, sizeof(before_raw));
	CHECK(yt_present_low_time_process((const uint8_t *)"0", 1U,
	    raw_remembered, &current, &result, &warned) == YT_PRESENT_OK);
	CHECK(!warned
	    && memcmp(raw_remembered, before_raw, sizeof(before_raw)) == 0
	    && result.event_count == 0U && result.remote_length == 0U);

	CHECK(qb_mbf32_encode(6.0f, raw_remembered) == QB_MBF_OK);
	current.sound.snoop = 0.0f;
	memset(long_time, 'x', sizeof(long_time));
	long_time[0] = '5';
	CHECK(yt_present_low_time_process(long_time, sizeof(long_time),
	    raw_remembered, &current, &result, &warned)
	    == YT_PRESENT_CAPACITY);
	CHECK(qb_mbf32_encode(5.0f, expected_raw) == QB_MBF_OK);
	CHECK(memcmp(raw_remembered, expected_raw, sizeof(expected_raw)) == 0);
	CHECK(!warned);
	CHECK(result.event_count == 3U);
	CHECK(result.remote_length == 3U);

	CHECK(qb_mbf32_encode(6.0f, raw_remembered) == QB_MBF_OK);
	memcpy(before_raw, raw_remembered, sizeof(before_raw));
	CHECK(yt_present_low_time_process((const uint8_t *)"2E38", 4U,
	    raw_remembered, &current, &result, &warned)
	    == YT_PRESENT_OVERFLOW);
	CHECK(memcmp(raw_remembered, before_raw, sizeof(before_raw)) == 0
	    && !warned && result.event_count == 0U
	    && result.remote_length == 0U);
}

static void
test_opening_streamer(void)
{
	static const uint8_t row[] = {'A', 0, 'B'};
	static const uint8_t remote[] = {'A', 0, 'B', '\n', '\r'};
	static const uint8_t reset[] = "\x1b[0m";
	struct yt_present_result result;

	CHECK(yt_present_opening_row(row, sizeof(row), 0.0f, -1.0f,
	    &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == sizeof(remote)
	    && memcmp(result.remote, remote, sizeof(remote)) == 0
	    && result.event_count == 3);
	CHECK(result.events[0].operation == YT_PRESENT_LOCAL_LINE
	    && result.events[0].length == sizeof(row)
	    && memcmp(result.events[0].data, row, sizeof(row)) == 0);
	CHECK(result.events[1].operation == YT_PRESENT_REMOTE_SEMI
	    && result.events[1].length == sizeof(row));
	CHECK(result.events[2].operation == YT_PRESENT_REMOTE_LINE
	    && result.events[2].length == 1U
	    && result.events[2].data[0] == '\n');

	CHECK(yt_present_opening_row(row, sizeof(row), 1.0f, 1.0f,
	    &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == 0U && result.event_count == 1U
	    && result.events[0].operation == YT_PRESENT_LOCAL_LINE);
	CHECK(yt_present_opening_row(row, sizeof(row), 2.0f, 0.0f,
	    &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == sizeof(remote)
	    && memcmp(result.remote, remote, sizeof(remote)) == 0
	    && result.event_count == 2U);

	CHECK(yt_present_opening_cleanup(0.0f, -1.0f, &result)
	    == YT_PRESENT_OK);
	CHECK(result.remote_length == sizeof(reset) - 1U
	    && memcmp(result.remote, reset, sizeof(reset) - 1U) == 0
	    && result.event_count == 2U
	    && result.events[0].operation == YT_PRESENT_REMOTE_SEMI
	    && result.events[1].operation == YT_PRESENT_LOCAL_SEMI
	    && result.events[1].length == sizeof(reset) - 1U);
	CHECK(yt_present_opening_cleanup(2.0f, 1.0f, &result)
	    == YT_PRESENT_OK);
	CHECK(result.remote_length == 0U && result.event_count == 1U
	    && result.events[0].operation == YT_PRESENT_LOCAL_SEMI);
	CHECK(yt_present_opening_cleanup(0.0f, 0.0f, &result)
	    == YT_PRESENT_OK);
	CHECK(result.remote_length == sizeof(reset) - 1U
	    && result.event_count == 1U
	    && result.events[0].operation == YT_PRESENT_REMOTE_SEMI);
	CHECK(yt_present_opening_row(NULL, 1U, 0.0f, 0.0f, &result)
	    == YT_PRESENT_CAPACITY);
	CHECK(yt_present_opening_row(NULL, 0U, 0.0f, 0.0f, NULL)
	    == YT_PRESENT_CAPACITY);
	CHECK(yt_present_opening_cleanup(0.0f, 0.0f, NULL)
	    == YT_PRESENT_CAPACITY);
}

static void
test_press_any_key_presentation(void)
{
	static const uint8_t ansi_prompt[] =
	    "\x1b[0;33;40;1m*[ Press any Key ]*";
	static const uint8_t ansi_cleanup[] =
	    "\r\x1b[0;33;40m                   \r";
	struct yt_present_state current = state(true);
	struct yt_present_result result;
	float saved;

	current.foreground = 7.0f;
	CHECK(yt_present_press_prompt(&current, &result, &saved)
	    == YT_PRESENT_OK);
	CHECK(saved == 7.0f && current.foreground == 3.0f);
	CHECK(result.remote_length == sizeof(ansi_prompt) - 1U
	    && memcmp(result.remote, ansi_prompt, sizeof(ansi_prompt) - 1U)
	    == 0);
	CHECK(result.event_count == 4);
	CHECK(result.events[0].operation == YT_PRESENT_LOCAL_COLOR
	    && result.events[0].foreground == 14
	    && result.events[0].background == 0);
	CHECK(result.events[2].operation == YT_PRESENT_LOCAL_SEMI);

	CHECK(yt_present_press_cleanup(saved, &current, &result)
	    == YT_PRESENT_OK);
	CHECK(current.foreground == 7.0f);
	CHECK(result.remote_length == sizeof(ansi_cleanup) - 1U
	    && memcmp(result.remote, ansi_cleanup,
	    sizeof(ansi_cleanup) - 1U) == 0);
	CHECK(result.events[0].operation == YT_PRESENT_LOCAL_LOCATE
	    && result.events[0].row == -1 && result.events[0].column == 1);
	CHECK(result.events[2].operation == YT_PRESENT_LOCAL_COLOR
	    && result.events[2].foreground == 6
	    && result.events[2].background == 0);
	CHECK(result.events[result.event_count - 1U].operation
	    == YT_PRESENT_LOCAL_LOCATE);

	current = state(false);
	current.foreground = 5.0f;
	CHECK(yt_present_press_prompt(&current, &result, &saved)
	    == YT_PRESENT_OK);
	CHECK(result.remote_length == strlen("*[ Press any Key ]*")
	    && memcmp(result.remote, "*[ Press any Key ]*",
	    result.remote_length) == 0);
	CHECK(yt_present_press_cleanup(saved, &current, &result)
	    == YT_PRESENT_OK);
	CHECK(result.remote_length == 21U
	    && result.remote[0] == '\r' && result.remote[20] == '\r');

	current = state(false);
	current.sound.mode = 2.0f;
	CHECK(yt_present_press_prompt(&current, &result, &saved)
	    == YT_PRESENT_OK);
	CHECK(yt_present_press_cleanup(saved, &current, &result)
	    == YT_PRESENT_OK);
	CHECK(result.remote_length == 19U
	    && memcmp(result.remote, "                   ", 19) == 0);
}

static void
test_post_login_press_presentation(void)
{
	static const uint8_t prompt[] = "[ Press any Key ]";
	static const uint8_t main_prompt[] =
	    "Time: 14:59  Main Command (?=Help)? ";
	static const uint8_t press_expected[] =
	    "\r\n[ Press any Key ]\r\n";
	static const uint8_t main_expected[] =
	    "\r\nTime: 14:59  Main Command (?=Help)? ";
	struct yt_present_state current = state(false);
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	char accumulator[80] = "inherited";

	memset(&pager, 0, sizeof(pager));
	memset(&capture, 0, sizeof(capture));
	current.foreground = 6.0f;
	pager.foreground = 6;
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, prompt, sizeof(prompt) - 1U,
	    &capture);
	CHECK(pager.line_count == 1.0f && pager.newline_flag == 0.0f);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(capture.remote_length == sizeof(press_expected) - 1U
	    && memcmp(capture.remote, press_expected,
	    sizeof(press_expected) - 1U) == 0);
	CHECK(capture.last_local_foreground == 7
	    && capture.last_local_background == 0);

	memset(&capture, 0, sizeof(capture));
	pager.line_count = 0.0f;
	current.foreground = 2.0f;
	pager.foreground = 2;
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, main_prompt,
	    sizeof(main_prompt) - 1U, &capture);
	CHECK(pager.line_count == 1.0f && pager.newline_flag == 0.0f);
	CHECK(capture.remote_length == sizeof(main_expected) - 1U
	    && memcmp(capture.remote, main_expected,
	    sizeof(main_expected) - 1U) == 0);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(pager.line_count == 0.0f && pager.nonstop == 0.0f
	    && pager.key[0] == '\0' && accumulator[0] == '\0');
}

static void
test_gameplay_reentry_hostile_warning(void)
{
	static const uint8_t warning[] =
	    "You have to defeat the fighters before you can enter this sector.";
	static const uint8_t expected[] =
	    "\r\nYou have to defeat the fighters before you can enter this sector.\n\r";
	struct yt_present_state current = state(false);
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;

	memset(&pager, 0, sizeof(pager));
	memset(&capture, 0, sizeof(capture));
	current.foreground = 3.0f;
	pager.foreground = 3;
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.bold = 1.0f;
	current.blink = 1.0f;
	pager.newline_flag = 0.0f;
	pager_fixture_b05d(&pager, &current, warning, sizeof(warning) - 1U,
	    &capture);
	CHECK(capture.remote_length == sizeof(expected) - 1U
	    && memcmp(capture.remote, expected, sizeof(expected) - 1U) == 0);
	CHECK(pager.line_count == 1.0f && pager.newline_flag == 0.0f
	    && current.bold == 1.0f && current.blink == 1.0f);
}

static void
test_hostile_menu_presentation(void)
{
	static const uint8_t fighter_row[] = "Fighters: 1000 / 1250";
	static const uint8_t prompt[] =
	    "Option? (A,B,D,I,Q,S,T,W,?=Help):? ";
	static const uint8_t attack_expected[] =
	    "\x1b[0;33;40m\r\n"
	    "Fighters: 1000 / 1250\n\r"
	    "Option? (A,B,D,I,Q,S,T,W,?=Help):? A\r\n";
	static const uint8_t invalid_expected[] =
	    "\x1b[0;33;40m\r\n"
	    "Fighters: 1000 / 1250\n\r"
	    "Option? (A,B,D,I,Q,S,T,W,?=Help):? Z\r\n"
	    "\r\n\x1b[0;33;40;5;1mInvalid command.\n\r"
	    "\x1b[0;33;40m\r\n";
	static const uint8_t help_expected[] =
	    "\x1b[0;33;40m\r\n"
	    "Fighters: 1000 / 1250\n\r"
	    "Option? (A,B,D,I,Q,S,T,W,?=Help):? ?\r\n"
	    "\r\n<Help>\n\r\r\nA - <A>ttack\n\r"
	    "B - <B>ribe Fighters\n\r"
	    "D - <D>rop a Mine\n\r"
	    "I - <I>nformation about your ship\n\r"
	    "Q - <Q>uit the game\n\r"
	    "S - Display <S>ector\n\r"
	    "T - <T>eam Menu\n\r"
	    "W - Emergency <W>arp\n\r";
	static const char *const help_rows[] = {
		"B - <B>ribe Fighters",
		"D - <D>rop a Mine",
		"I - <I>nformation about your ship",
		"Q - <Q>uit the game",
		"S - Display <S>ector",
		"T - <T>eam Menu",
		"W - Emergency <W>arp",
	};
	struct yt_present_state current = state(true);
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	char accumulator[80] = "inherited";
	uint8_t key = 'A';
	size_t index;

	memset(&pager, 0, sizeof(pager));
	memset(&capture, 0, sizeof(capture));
	current.foreground = 3.0f;
	pager.foreground = 3;
	pager.line_count = 1.0f;
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current, fighter_row,
	    sizeof(fighter_row) - 1U, &capture);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, prompt, sizeof(prompt) - 1U,
	    &capture);
	CHECK(pager.line_count == 3.0f);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(pager.line_count == 0.0f && accumulator[0] == '\0');
	CHECK(yt_present_editor_echo(&key, 1, &key, 1, &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(capture.remote_length == sizeof(attack_expected) - 1U
	    && memcmp(capture.remote, attack_expected,
	    sizeof(attack_expected) - 1U) == 0);
	CHECK(capture.remote_length == 73U);
	CHECK(current.foreground == 3.0f && current.background == 0.0f
	    && current.bold == 0.0f && current.blink == 0.0f);
	CHECK(capture.last_local_foreground == 6
	    && capture.last_local_background == 0);

	current = state(true);
	current.foreground = 3.0f;
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 3;
	pager.line_count = 1.0f;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current, fighter_row,
	    sizeof(fighter_row) - 1U, &capture);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, prompt, sizeof(prompt) - 1U,
	    &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	key = 'Z';
	CHECK(yt_present_editor_echo(&key, 1, &key, 1, &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.bold = 1.0f;
	current.blink = 1.0f;
	pager_fixture_b05d(&pager, &current,
	    (const uint8_t *)"Invalid command.", strlen("Invalid command."),
	    &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(capture.remote_length == sizeof(invalid_expected) - 1U
	    && memcmp(capture.remote, invalid_expected,
	    sizeof(invalid_expected) - 1U) == 0);
	CHECK(capture.remote_length == 119U && pager.line_count == 1.0f
	    && current.bold == 0.0f && current.blink == 0.0f);

	current = state(true);
	current.foreground = 3.0f;
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 3;
	pager.line_count = 1.0f;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current, fighter_row,
	    sizeof(fighter_row) - 1U, &capture);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, prompt, sizeof(prompt) - 1U,
	    &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	key = '?';
	CHECK(yt_present_editor_echo(&key, 1, &key, 1, &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current, (const uint8_t *)"<Help>",
	    strlen("<Help>"), &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current,
	    (const uint8_t *)"A - <A>ttack", strlen("A - <A>ttack"),
	    &capture);
	for (index = 0; index < YT_ARRAY_LEN(help_rows); ++index)
		pager_fixture_b05d(&pager, &current,
		    (const uint8_t *)help_rows[index], strlen(help_rows[index]),
		    &capture);
	CHECK(capture.remote_length == sizeof(help_expected) - 1U
	    && memcmp(capture.remote, help_expected,
	    sizeof(help_expected) - 1U) == 0);
	CHECK(capture.remote_length == 257U && pager.line_count == 9.0f);
}

static void
test_hostile_quit_presentation(void)
{
	static const uint8_t heading[] = "<Quit>";
	static const uint8_t prompt[] = "Are you sure (Y/N)? ";
	static const uint8_t expected[] =
	    "\x1b[0;37;40m<Quit>\n\rAre you sure (Y/N)? N\r\n";
	struct yt_present_state current = state(true);
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	char accumulator[80] = "Q";
	uint8_t key = 'N';

	current.foreground = 3.0f;
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	memset(&pager, 0, sizeof(pager));
	memset(&capture, 0, sizeof(capture));
	current.foreground = 7.0f;
	pager.foreground = 7;
	pager_fixture_b05d(&pager, &current, heading, sizeof(heading) - 1U,
	    &capture);
	CHECK(yt_present_character(prompt, sizeof(prompt) - 1U, &current,
	    &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(accumulator[0] == '\0' && pager.line_count == 0.0f);
	CHECK(yt_present_editor_echo(&key, 1, &key, 1, &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(capture.remote_length == sizeof(expected) - 1U
	    && memcmp(capture.remote, expected, sizeof(expected) - 1U) == 0);
	CHECK(current.foreground == 7.0f && pager.line_count == 0.0f
	    && capture.last_local_foreground == 7
	    && capture.last_local_background == 0);
}

static void
test_direct_emergency_warp_presentation(void)
{
	static const uint8_t warning_one[] =
	    "This is a desperate move! Your engines will be drained and will take time";
	static const uint8_t warning_two[] =
	    "to recharge! You also risk a melt down! Are you sure you wish to do this?";
	static const uint8_t prompt[] = "[y/N] -=> ";
	static const uint8_t plain[] =
	    "\r\n"
	    "This is a desperate move! Your engines will be drained and will take time\n\r"
	    "to recharge! You also risk a melt down! Are you sure you wish to do this?\n\r"
	    "\r\n[y/N] -=> N\r\n";
	static const uint8_t ansi[] =
	    "\r\n"
	    "\x1b[0;37;40;1m"
	    "This is a desperate move! Your engines will be drained and will take time\n\r"
	    "\x1b[0;37;40;1m"
	    "to recharge! You also risk a melt down! Are you sure you wish to do this?\n\r"
	    "\x1b[0;37;40m\r\n"
	    "\x1b[0;37;40;1m[y/N] -=> N"
	    "\x1b[0;37;40m\r\n";
	static const uint8_t answer[] = "N";
	static const uint8_t no_turns[] = "Sorry but you have no turns left.";
	static const uint8_t no_turns_plain[] =
	    "\r\nSorry but you have no turns left.\n\r";
	static const uint8_t no_turns_ansi[] =
	    "\r\n\x1b[0;32;40;5;1mSorry but you have no turns left.\n\r";
	struct yt_present_state current;
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	char accumulator[80] = "W";
	int pass;

	for (pass = 0; pass < 2; ++pass) {
		current = state(pass != 0);
		CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
		memset(&pager, 0, sizeof(pager));
		pager.foreground = 2;
		memset(&capture, 0, sizeof(capture));
		CHECK(yt_present_line(NULL, 0, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		current.bold = 1.0f;
		current.foreground = 7.0f;
		pager.foreground = 7;
		pager_fixture_b05d(&pager, &current, warning_one,
		    sizeof(warning_one) - 1U, &capture);
		current.bold = 1.0f;
		pager_fixture_b05d(&pager, &current, warning_two,
		    sizeof(warning_two) - 1U, &capture);
		CHECK(yt_present_line(NULL, 0, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		current.bold = 1.0f;
		CHECK(yt_present_character(prompt, sizeof(prompt) - 1U,
		    &current, &result) == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
		CHECK(yt_present_editor_echo(answer, sizeof(answer) - 1U,
		    answer, sizeof(answer) - 1U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		CHECK(yt_present_line(NULL, 0, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		if (pass == 0) {
			CHECK(capture.remote_length == sizeof(plain) - 1U
			    && memcmp(capture.remote, plain,
			    sizeof(plain) - 1U) == 0);
			CHECK(capture.remote_length == 167U);
		}
		else {
			CHECK(capture.remote_length == sizeof(ansi) - 1U
			    && memcmp(capture.remote, ansi,
			    sizeof(ansi) - 1U) == 0);
			CHECK(capture.remote_length == 223U);
		}
		CHECK(pager.line_count == 0.0f
		    && current.foreground == 7.0f
		    && current.bold == (pass == 0 ? 1.0f : 0.0f)
		    && capture.last_local_foreground == 7
		    && capture.last_local_background == 0);
	}
	for (pass = 0; pass < 2; ++pass) {
		current = state(pass != 0);
		CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
		memset(&pager, 0, sizeof(pager));
		pager.foreground = 2;
		memset(&capture, 0, sizeof(capture));
		CHECK(yt_present_line(NULL, 0, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		current.bold = 1.0f;
		current.blink = 1.0f;
		pager_fixture_b05d(&pager, &current, no_turns,
		    sizeof(no_turns) - 1U, &capture);
		if (pass == 0)
			CHECK(capture.remote_length == sizeof(no_turns_plain) - 1U
			    && memcmp(capture.remote, no_turns_plain,
			    sizeof(no_turns_plain) - 1U) == 0);
		else
			CHECK(capture.remote_length == sizeof(no_turns_ansi) - 1U
			    && memcmp(capture.remote, no_turns_ansi,
			    sizeof(no_turns_ansi) - 1U) == 0);
		CHECK(pager.line_count == 1.0f);
	}
}

static void
test_team_front_presentation(void)
{
	static const uint8_t exit_row[] = "1) Exit Team menu";
	static const uint8_t create_row[] = "2) Create a Team";
	static const uint8_t join_row[] = "3) Join a Team";
	static const uint8_t prompt[] = "Time: 14:59  Team Command? ";
	static const uint8_t one[] = "1";
	static const uint8_t expected[] =
	    "\r\nTeam  : None\r\n\r\n\r\n"
	    "1) Exit Team menu\n\r"
	    "2) Create a Team\n\r"
	    "3) Join a Team\n\r"
	    "\r\nTime: 14:59  Team Command? 1\r\n";
	struct yt_present_state current;
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	char accumulator[80] = "T";
	int pass;

	for (pass = 0; pass < 2; ++pass) {
		current = state(pass != 0);
		current.foreground = 6.0f;
		CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
		memset(&pager, 0, sizeof(pager));
		pager.foreground = 6;
		memset(&capture, 0, sizeof(capture));
		CHECK(yt_present_line(NULL, 0, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		CHECK(yt_present_line((const uint8_t *)"Team  : None", 12,
		    &current, &result) == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		CHECK(yt_present_line(NULL, 0, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		pager.line_count = 0.0f;
		CHECK(yt_present_line(NULL, 0, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		pager_fixture_b05d(&pager, &current, exit_row,
		    sizeof(exit_row) - 1U, &capture);
		pager_fixture_b05d(&pager, &current, create_row,
		    sizeof(create_row) - 1U, &capture);
		pager_fixture_b05d(&pager, &current, join_row,
		    sizeof(join_row) - 1U, &capture);
		CHECK(yt_present_line(NULL, 0, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		pager.newline_flag = 1.0f;
		pager_fixture_b05d(&pager, &current, prompt,
		    sizeof(prompt) - 1U, &capture);
		yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
		CHECK(yt_present_editor_echo(one, sizeof(one) - 1U,
		    one, sizeof(one) - 1U, &current, &result) == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		CHECK(yt_present_line(NULL, 0, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		CHECK(capture.remote_length == sizeof(expected) - 1U
		    && memcmp(capture.remote, expected, sizeof(expected) - 1U)
		    == 0);
		CHECK(capture.remote_length == 105U
		    && pager.line_count == 0.0f
		    && current.foreground == 6.0f
		    && capture.last_local_foreground == (pass == 0 ? 7 : 3)
		    && capture.last_local_background == 0);
	}
}

static void
test_team_create_presentation(void)
{
	static const uint8_t entering[] = "Entering a New Team...";
	static const uint8_t name_prompt[] =
	    "Pick a name for your Team (41 chars. max)? ";
	static const uint8_t name[] = "Raiders";
	static const uint8_t password_prompt[] =
	    "Please Pick a Password for your Team. (4 Chars.) :";
	static const uint8_t password_echo[] = "pass";
	static const uint8_t reminder[] =
	    "REMEMBER YOUR TEAM PASSWORD SO OTHERS CAN JOIN! -+> PASS";
	static const uint8_t success[] =
	    "Team number [ 1 ] [Raiders] CREATED!";
	static const uint8_t expected[] =
	    "\r\nEntering a New Team...\n\r"
	    "\r\nPick a name for your Team (41 chars. max)? Raiders\r\n"
	    "\r\nPlease Pick a Password for your Team. (4 Chars.) :pass\r\n"
	    "\r\nREMEMBER YOUR TEAM PASSWORD SO OTHERS CAN JOIN! -+> PASS\n\r"
	    "\r\nTeam number [ 1 ] [Raiders] CREATED!\n\r";
	struct yt_present_state current = state(false);
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	char accumulator[80] = "2";

	current.foreground = 6.0f;
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 6;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.bold = 1.0f;
	current.blink = 1.0f;
	pager_fixture_b05d(&pager, &current, entering,
	    sizeof(entering) - 1U, &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, name_prompt,
	    sizeof(name_prompt) - 1U, &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo(name, sizeof(name) - 1U,
	    name, sizeof(name) - 1U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, password_prompt,
	    sizeof(password_prompt) - 1U, &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo(password_echo,
	    sizeof(password_echo) - 1U, password_echo,
	    sizeof(password_echo) - 1U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.bold = 1.0f;
	current.blink = 1.0f;
	pager_fixture_b05d(&pager, &current, reminder,
	    sizeof(reminder) - 1U, &capture);
	current.foreground = 3.0f;
	pager.foreground = 3;
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.bold = 1.0f;
	current.blink = 1.0f;
	pager_fixture_b05d(&pager, &current, success,
	    sizeof(success) - 1U, &capture);
	CHECK(capture.remote_length == sizeof(expected) - 1U
	    && memcmp(capture.remote, expected, sizeof(expected) - 1U) == 0);
	CHECK(pager.line_count == 2.0f && current.foreground == 3.0f);
}

static void
test_team_join_presentation(void)
{
	static const uint8_t list_row[] = " 1] Raiders";
	static const uint8_t selection_prompt[] =
	    "Which team do you wish to join (0=quit)? ";
	static const uint8_t one[] = "1";
	static const uint8_t team_row[] = "Team # 1: Raiders";
	static const uint8_t password_prompt[] =
	    "Please enter Password to Join Team? ";
	static const uint8_t password[] = "pass";
	static const uint8_t success[] =
	    "Your Team info has been recorded!  Have fun!";
	static const uint8_t expected[] =
	    "\r\n 1] Raiders\n\r"
	    "\r\nWhich team do you wish to join (0=quit)? 1\r\n"
	    "Team # 1: Raiders\n\r"
	    "Please enter Password to Join Team? pass\r\n"
	    "\r\nYour Team info has been recorded!  Have fun!\n\r";
	struct yt_present_state current = state(false);
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	char accumulator[80] = "3";

	current.foreground = 6.0f;
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 6;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current, list_row,
	    sizeof(list_row) - 1U, &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, selection_prompt,
	    sizeof(selection_prompt) - 1U, &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo(one, sizeof(one) - 1U,
	    one, sizeof(one) - 1U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current, team_row,
	    sizeof(team_row) - 1U, &capture);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, password_prompt,
	    sizeof(password_prompt) - 1U, &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo(password, sizeof(password) - 1U,
	    password, sizeof(password) - 1U, &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.foreground = 3.0f;
	pager.foreground = 3;
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.bold = 1.0f;
	current.blink = 1.0f;
	pager_fixture_b05d(&pager, &current, success,
	    sizeof(success) - 1U, &capture);
	CHECK(capture.remote_length == sizeof(expected) - 1U
	    && memcmp(capture.remote, expected, sizeof(expected) - 1U) == 0);
	CHECK(pager.line_count == 1.0f && current.foreground == 3.0f);
}

static void
test_team_quit_presentation(void)
{
	static const uint8_t prompt[] =
	    "Are you sure you wish to quit your team? [N] ";
	static const uint8_t yes[] = "Y";
	static const uint8_t success[] =
	    "You have been removed from Team play";
	static const uint8_t expected[] =
	    "Are you sure you wish to quit your team? [N] Y\r\n"
	    "\r\nYou have been removed from Team play\n\r";
	struct yt_present_state current = state(false);
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	char accumulator[80] = "4";

	current.foreground = 6.0f;
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 6;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_character(prompt, sizeof(prompt) - 1U,
	    &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo(yes, sizeof(yes) - 1U,
	    yes, sizeof(yes) - 1U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current, success,
	    sizeof(success) - 1U, &capture);
	CHECK(capture.remote_length == sizeof(expected) - 1U
	    && memcmp(capture.remote, expected, sizeof(expected) - 1U) == 0);
	CHECK(pager.line_count == 1.0f && current.foreground == 6.0f);
}

static void
test_team_resource_presentation(void)
{
	static const uint8_t locating[] =
	    "Locating Team Members, Planets & Defenses.";
	static const uint8_t heading[] =
	    "Name                                     Sector";
	static const uint8_t rule[] =
	    "====================                     ======";
	static const uint8_t player_row[] =
	    "Bob"
	    "          " "          " "          " "        "
	    " 5";
	static const uint8_t defending[] = "Defending;";
	static const uint8_t two[] = " 2";
	static const uint8_t three[] = " 3";
	static const uint8_t planets[] = "Planets;";
	static const uint8_t none[] = "None Found";
	static const uint8_t search_expected[] =
	    "\r\nLocating Team Members, Planets & Defenses.\n\r"
	    "\r\nName                                     Sector\n\r"
	    "====================                     ======\n\r"
	    "Bob"
	    "          " "          " "          " "        "
	    " 5\n\r"
	    "Defending; 2 3\r\n"
	    "Planets; 3\r\n";
	static const uint8_t none_expected[] =
	    "\r\nLocating Team Members, Planets & Defenses.\n\r"
	    "None Found\n\r";
	static const uint8_t carried[] = "You have 30 fighters.";
	static const uint8_t deployed[] = "There are 10 fighters here.";
	static const uint8_t prompt[] =
	    "How many fighters do you wish to transfer? ";
	static const uint8_t amount[] = "5";
	static const uint8_t success[] = "Fighters transferred!";
	static const uint8_t transfer_expected[] =
	    "\r\nYou have 30 fighters.\n\r"
	    "\r\nThere are 10 fighters here.\n\r"
	    "How many fighters do you wish to transfer? 5\r\n"
	    "\r\n\x1b[0;36;40;5;1mFighters transferred!\n\r";
	static const uint8_t no_defense[] =
	    "There IS no defense force here!";
	static const uint8_t no_defense_expected[] =
	    "\r\n\x1b[0;36;40;5;1m"
	    "There IS no defense force here!\n\r";
	struct yt_present_state current;
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	char accumulator[80] = "6";

	current = state(true);
	current.foreground = 6.0f;
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 6;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current, locating,
	    sizeof(locating) - 1U, &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current, heading,
	    sizeof(heading) - 1U, &capture);
	pager_fixture_b05d(&pager, &current, rule, sizeof(rule) - 1U,
	    &capture);
	pager_fixture_b05d(&pager, &current, player_row,
	    sizeof(player_row) - 1U, &capture);
	CHECK(yt_present_character(defending, sizeof(defending) - 1U,
	    &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_character(two, sizeof(two) - 1U, &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_character(three, sizeof(three) - 1U,
	    &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_character(planets, sizeof(planets) - 1U,
	    &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_character(three, sizeof(three) - 1U,
	    &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(capture.remote_length == sizeof(search_expected) - 1U
	    && memcmp(capture.remote, search_expected,
	    sizeof(search_expected) - 1U) == 0);
	CHECK(capture.remote_length == 219U && pager.line_count == 4.0f);

	current = state(false);
	current.foreground = 6.0f;
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 6;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current, locating,
	    sizeof(locating) - 1U, &capture);
	pager_fixture_b05d(&pager, &current, none, sizeof(none) - 1U,
	    &capture);
	CHECK(capture.remote_length == sizeof(none_expected) - 1U
	    && memcmp(capture.remote, none_expected,
	    sizeof(none_expected) - 1U) == 0);
	CHECK(capture.remote_length == 58U && pager.line_count == 2.0f);

	current = state(true);
	current.foreground = 6.0f;
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 6;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current, carried,
	    sizeof(carried) - 1U, &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current, deployed,
	    sizeof(deployed) - 1U, &capture);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, prompt, sizeof(prompt) - 1U,
	    &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo(amount, sizeof(amount) - 1U,
	    amount, sizeof(amount) - 1U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.bold = 1.0f;
	current.blink = 1.0f;
	pager_fixture_b05d(&pager, &current, success,
	    sizeof(success) - 1U, &capture);
	CHECK(capture.remote_length == sizeof(transfer_expected) - 1U
	    && memcmp(capture.remote, transfer_expected,
	    sizeof(transfer_expected) - 1U) == 0);
	CHECK(capture.remote_length == 141U && pager.line_count == 1.0f);

	current = state(true);
	current.foreground = 6.0f;
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 6;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.bold = 1.0f;
	current.blink = 1.0f;
	pager_fixture_b05d(&pager, &current, no_defense,
	    sizeof(no_defense) - 1U, &capture);
	CHECK(capture.remote_length == sizeof(no_defense_expected) - 1U
	    && memcmp(capture.remote, no_defense_expected,
	    sizeof(no_defense_expected) - 1U) == 0);
	CHECK(capture.remote_length == 49U && pager.line_count == 1.0f);
}

static void
test_team_banish_presentation(void)
{
	static const uint8_t prompt[] = "Banish Morgan (Y/[N])? ";
	static const uint8_t no[] = "N";
	static const uint8_t yes[] = "Y";
	static const uint8_t end[] = "End of List";
	static const uint8_t success[] =
	    "Done. Now change your Team Password!";
	static const uint8_t reject_expected[] =
	    "Banish Morgan (Y/[N])? N\r\nEnd of List\n\r";
	static const uint8_t accept_expected[] =
	    "Banish Morgan (Y/[N])? Y\r\n"
	    "\r\n\x1b[0;36;40;5;1m"
	    "Done. Now change your Team Password!\n\r";
	struct yt_present_state current;
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	char accumulator[80] = "7";

	current = state(false);
	current.foreground = 6.0f;
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 6;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_character(prompt, sizeof(prompt) - 1U,
	    &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo(no, sizeof(no) - 1U, no,
	    sizeof(no) - 1U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current, end, sizeof(end) - 1U,
	    &capture);
	CHECK(capture.remote_length == sizeof(reject_expected) - 1U
	    && memcmp(capture.remote, reject_expected,
	    sizeof(reject_expected) - 1U) == 0);
	CHECK(capture.remote_length == 39U && pager.line_count == 1.0f);

	current = state(true);
	current.foreground = 6.0f;
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 6;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_character(prompt, sizeof(prompt) - 1U,
	    &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo(yes, sizeof(yes) - 1U, yes,
	    sizeof(yes) - 1U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.bold = 1.0f;
	current.blink = 1.0f;
	pager_fixture_b05d(&pager, &current, success,
	    sizeof(success) - 1U, &capture);
	CHECK(capture.remote_length == sizeof(accept_expected) - 1U
	    && memcmp(capture.remote, accept_expected,
	    sizeof(accept_expected) - 1U) == 0);
	CHECK(capture.remote_length == 80U && pager.line_count == 1.0f);
}

static void
test_port_docking_controller_presentation(void)
{
	static const uint8_t heading[] = "<Port>";
	static const uint8_t no_port[] = "No port here!";
	static const uint8_t docking[] = "Docking, ";
	static const uint8_t turn[] = "One Turn Deducted, 59 left.";
	static const uint8_t plain_no_port[] =
	    "<Port>\n\r\r\nNo port here!\n\r";
	static const uint8_t ansi_no_port[] =
	    "\x1b[0;36;40m<Port>\n\r"
	    "\x1b[0;33;40m\r\n"
	    "\x1b[0;33;40;5;1mNo port here!\n\r";
	static const uint8_t ordinary_prelude[] =
	    "<Port>\n\r\r\nDocking, One Turn Deducted, 59 left.\n\r";
	static const uint8_t refusal[] =
	    "We don't want your goods and you can't buy ours Pat!";
	static const uint8_t status[] =
	    "You have 777 credits and 15 empty cargo holds.";
	static const uint8_t refusal_status[] =
	    "\r\nWe don't want your goods and you can't buy ours Pat!\n\r"
	    "\r\nYou have 777 credits and 15 empty cargo holds.\n\r";
	struct yt_present_state current;
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	int ansi;

	for (ansi = 0; ansi < 2; ++ansi) {
		current = state(ansi != 0);
		current.foreground = 6.0f;
		memset(&pager, 0, sizeof(pager));
		pager.foreground = 6;
		memset(&capture, 0, sizeof(capture));
		pager_fixture_b05d(&pager, &current, heading,
		    sizeof(heading) - 1U, &capture);
		current.foreground = 3.0f;
		pager.foreground = 3;
		CHECK(yt_present_line(NULL, 0, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		current.bold = 1.0f;
		current.blink = 1.0f;
		pager_fixture_b05d(&pager, &current, no_port,
		    sizeof(no_port) - 1U, &capture);
		if (ansi == 0)
			CHECK(capture.remote_length == sizeof(plain_no_port) - 1U
			    && memcmp(capture.remote, plain_no_port,
			    sizeof(plain_no_port) - 1U) == 0);
		else
			CHECK(capture.remote_length == sizeof(ansi_no_port) - 1U
			    && memcmp(capture.remote, ansi_no_port,
			    sizeof(ansi_no_port) - 1U) == 0);
	}

	current = state(false);
	current.foreground = 6.0f;
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 6;
	memset(&capture, 0, sizeof(capture));
	pager_fixture_b05d(&pager, &current, heading,
	    sizeof(heading) - 1U, &capture);
	current.foreground = 3.0f;
	pager.foreground = 3;
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, docking,
	    sizeof(docking) - 1U, &capture);
	pager_fixture_b05d(&pager, &current, turn, sizeof(turn) - 1U,
	    &capture);
	CHECK(capture.remote_length == sizeof(ordinary_prelude) - 1U
	    && memcmp(capture.remote, ordinary_prelude,
	    sizeof(ordinary_prelude) - 1U) == 0);

	current = state(false);
	current.foreground = 6.0f;
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 6;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.bold = 1.0f;
	current.blink = 1.0f;
	pager_fixture_b05d(&pager, &current, refusal,
	    sizeof(refusal) - 1U, &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current, status, sizeof(status) - 1U,
	    &capture);
	CHECK(capture.remote_length == sizeof(refusal_status) - 1U
	    && memcmp(capture.remote, refusal_status,
	    sizeof(refusal_status) - 1U) == 0);
}

static void
test_action_finalizer_presentation(void)
{
	static const uint8_t cloak[] = "Cloak at 24%";
	static const uint8_t turn[] = "One Turn Deducted, 50 left.";
	static const uint8_t expected[] =
	    "Cloak at 24%\r\n\r\nOne Turn Deducted, 50 left.\n\r";
	struct yt_present_state current = state(false);
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;

	current.foreground = 3.0f;
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 3;
	memset(&capture, 0, sizeof(capture));
	current.foreground = 7.0f;
	pager.foreground = 7;
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, cloak, sizeof(cloak) - 1U,
	    &capture);
	current.foreground = 3.0f;
	pager.foreground = 3;
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.bold = 1.0f;
	current.blink = 1.0f;
	pager_fixture_b05d(&pager, &current, turn, sizeof(turn) - 1U,
	    &capture);
	CHECK(capture.remote_length == sizeof(expected) - 1U
	    && memcmp(capture.remote, expected, sizeof(expected) - 1U) == 0);
	CHECK(pager.line_count == 2.0f && current.foreground == 3.0f);
}

struct commodity_trade_join {
	struct yt_present_state current;
	struct yt_pager_state pager;
	struct pager_capture capture;
	char accumulator[80];
};

static void
commodity_trade_join_init(struct commodity_trade_join *join)
{
	join->current = state(true);
	join->current.foreground = 6.0f;
	join->current.cached_foreground = 6.0f;
	join->current.cached_background = 0.0f;
	memset(&join->pager, 0, sizeof(join->pager));
	join->pager.foreground = 6;
	memset(&join->capture, 0, sizeof(join->capture));
	memset(join->accumulator, 0, sizeof(join->accumulator));
}

static void
commodity_trade_join_line(struct commodity_trade_join *join)
{
	struct yt_present_result result;

	CHECK(yt_present_line(NULL, 0, &join->current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&join->capture, &result);
}

static void
commodity_trade_join_b05d(struct commodity_trade_join *join,
    const uint8_t *text, size_t length, bool prompt)
{
	join->pager.newline_flag = prompt ? 1.0f : 0.0f;
	pager_fixture_b05d(&join->pager, &join->current, text, length,
	    &join->capture);
}

static void
commodity_trade_join_0317(struct commodity_trade_join *join,
    const uint8_t *text, size_t length)
{
	commodity_trade_join_line(join);
	commodity_trade_join_b05d(join, text, length, false);
}

static void
commodity_trade_join_02db(struct commodity_trade_join *join,
    const uint8_t *text, size_t length)
{
	commodity_trade_join_line(join);
	join->current.bold = 1.0f;
	join->current.blink = 1.0f;
	commodity_trade_join_b05d(join, text, length, false);
}

static void
commodity_trade_join_input(struct commodity_trade_join *join,
    const uint8_t *response, size_t length)
{
	struct yt_present_result result;

	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	CHECK(yt_present_editor_echo(response, length, response, length,
	    &join->current, &result) == YT_PRESENT_OK);
	pager_capture_result(&join->capture, &result);
	commodity_trade_join_line(join);
}

static void
commodity_trade_join_front(struct commodity_trade_join *join,
    const uint8_t *player_status, size_t player_status_length,
    const uint8_t *market_status, size_t market_status_length,
    const uint8_t *prompt, size_t prompt_length,
    const uint8_t *response, size_t response_length)
{
	commodity_trade_join_0317(join, player_status, player_status_length);
	commodity_trade_join_0317(join, market_status, market_status_length);
	commodity_trade_join_b05d(join, prompt, prompt_length, true);
	commodity_trade_join_input(join, response, response_length);
}

static void
commodity_trade_join_accepted_tail(struct commodity_trade_join *join,
    const uint8_t *agreed, size_t agreed_length, const uint8_t *offer,
    size_t offer_length, const uint8_t *success, size_t success_length)
{
	static const uint8_t confirmation[] = "Do you agree? [Y/n] ";
	struct yt_present_result result;

	commodity_trade_join_b05d(join, agreed, agreed_length, false);
	commodity_trade_join_0317(join, offer, offer_length);
	CHECK(yt_present_character(confirmation, sizeof(confirmation) - 1U,
	    &join->current, &result) == YT_PRESENT_OK);
	pager_capture_result(&join->capture, &result);
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	commodity_trade_join_line(join);
	commodity_trade_join_b05d(join, success, success_length, false);
}

static void
commodity_trade_join_check(const struct commodity_trade_join *join,
    const uint8_t *expected, size_t expected_length)
{
	CHECK(join->capture.remote_length == expected_length
	    && memcmp(join->capture.remote, expected, expected_length) == 0);
}

struct commodity_b05d_cut {
	struct commodity_trade_join *join;
	struct yt_b05d_key_state key_state;
	char queue[8];
	size_t queue_position;
	size_t queue_length;
	size_t carrier_calls;
	size_t fail_carrier_at;
	size_t sample_calls;
	struct yt_input_value sampled;
	size_t present_calls;
	size_t finish_calls;
	size_t response_calls;
	bool accept_response;
};

static bool
commodity_b05d_cut_carrier(void *context)
{
	struct commodity_b05d_cut *cut = context;
	size_t call = cut->carrier_calls++;

	return call != cut->fail_carrier_at;
}

static bool
commodity_b05d_cut_sample(void *context, struct yt_input_value *sampled)
{
	struct commodity_b05d_cut *cut = context;

	++cut->sample_calls;
	*sampled = cut->sampled;
	return true;
}

static bool
commodity_b05d_cut_present(void *context, const uint8_t *text, size_t length)
{
	struct commodity_b05d_cut *cut = context;
	struct yt_present_result result;

	++cut->present_calls;
	if (yt_present_paged_text(text, length, &cut->join->current, &result)
	    != YT_PRESENT_OK)
		return false;
	pager_capture_result(&cut->join->capture, &result);
	return true;
}

static bool
commodity_b05d_cut_finish(void *context, bool newline_flag)
{
	struct commodity_b05d_cut *cut = context;
	struct yt_present_result result;

	++cut->finish_calls;
	if (yt_present_paged_finish(newline_flag, &cut->join->current, &result)
	    != YT_PRESENT_OK)
		return false;
	pager_capture_result(&cut->join->capture, &result);
	return true;
}

static bool
commodity_b05d_cut_response(void *context, char *response, size_t capacity)
{
	struct commodity_b05d_cut *cut = context;

	++cut->response_calls;
	if (!cut->accept_response || capacity == 0U)
		return false;
	response[0] = '\0';
	commodity_trade_join_input(cut->join, NULL, 0U);
	return true;
}

static const struct yt_paged_row_ops commodity_b05d_cut_ops = {
	commodity_b05d_cut_carrier,
	commodity_b05d_cut_sample,
	commodity_b05d_cut_present,
	commodity_b05d_cut_finish,
	commodity_b05d_cut_response,
};

static void
commodity_b05d_cut_init(struct commodity_b05d_cut *cut,
    struct commodity_trade_join *join, size_t fail_carrier_at)
{
	memset(cut, 0, sizeof(*cut));
	cut->join = join;
	cut->fail_carrier_at = fail_carrier_at;
	cut->key_state.accumulator = join->accumulator;
	cut->key_state.accumulator_capacity = sizeof(join->accumulator);
	cut->key_state.queue = cut->queue;
	cut->key_state.queue_capacity = sizeof(cut->queue);
	cut->key_state.queue_position = &cut->queue_position;
	cut->key_state.queue_length = &cut->queue_length;
	cut->key_state.pager_key = join->pager.key;
	cut->key_state.pager_key_capacity = sizeof(join->pager.key);
}

struct commodity_editor_cut {
	struct commodity_trade_join *join;
	size_t carrier_calls;
	size_t fail_carrier_at;
};

struct commodity_terminal_join {
	struct commodity_trade_join *join;
	bool closed;
};

static bool
commodity_terminal_notice(void *context, const uint8_t *notice,
    size_t length)
{
	struct commodity_terminal_join *terminal = context;

	commodity_trade_join_0317(terminal->join, notice, length);
	return true;
}

static bool
commodity_terminal_close(void *context)
{
	struct commodity_terminal_join *terminal = context;

	terminal->closed = true;
	return true;
}

static bool
commodity_editor_cut_echo(void *context, const uint8_t *local,
    size_t local_length, const uint8_t *remote, size_t remote_length)
{
	struct commodity_editor_cut *cut = context;
	struct yt_present_result result;

	if (yt_present_editor_echo(local, local_length, remote, remote_length,
	    &cut->join->current, &result) != YT_PRESENT_OK)
		return false;
	pager_capture_result(&cut->join->capture, &result);
	return true;
}

static bool
commodity_editor_cut_carrier(void *context)
{
	struct commodity_editor_cut *cut = context;
	size_t call = cut->carrier_calls++;

	return call != cut->fail_carrier_at;
}

static void
test_commodity_trade_presentation(void)
{
	static const uint8_t player_status[] =
	    "You have 12345 credits and 65 empty cargo holds.";
	static const uint8_t market_status[] =
	    "We are selling up to 100.  You have 10 in your holds.";
	static const uint8_t prompt[] =
	    "How many holds of Ore do you want to buy [ 65 ]? ";
	static const uint8_t three[] = "3";
	static const uint8_t agreed[] = "Agreed, 3 units.";
	static const uint8_t offer[] = "We'll sell them for 60 credits.";
	static const uint8_t success[] = "It's Yours!";
	static const uint8_t expected[] =
	    "\r\nYou have 12345 credits and 65 empty cargo holds.\n\r"
	    "\r\nWe are selling up to 100.  You have 10 in your holds.\n\r"
	    "How many holds of Ore do you want to buy [ 65 ]? 3\r\n"
	    "Agreed, 3 units.\n\r"
	    "\r\nWe'll sell them for 60 credits.\n\r"
	    "Do you agree? [Y/n] \r\n"
	    "It's Yours!\n\r";
	struct commodity_trade_join join;

	commodity_trade_join_init(&join);
	commodity_trade_join_front(&join, player_status,
	    sizeof(player_status) - 1U, market_status,
	    sizeof(market_status) - 1U, prompt, sizeof(prompt) - 1U,
	    three, sizeof(three) - 1U);
	commodity_trade_join_accepted_tail(&join, agreed, sizeof(agreed) - 1U,
	    offer, sizeof(offer) - 1U, success, sizeof(success) - 1U);
	CHECK(join.capture.remote_length == sizeof(expected) - 1U
	    && memcmp(join.capture.remote, expected,
	    sizeof(expected) - 1U) == 0);
	CHECK(join.capture.remote_length == 249U
	    && join.pager.line_count == 1.0f);
}

static void
test_commodity_trade_branch_presentation(void)
{
	static const uint8_t status[] =
	    "You have 12345 credits and 65 empty cargo holds.";
	static const uint8_t selling[] =
	    "We are selling up to 100.  You have 10 in your holds.";
	static const uint8_t buying[] =
	    "We are buying up to 80.  You have 20 in your holds.";
	static const uint8_t ore_prompt[] =
	    "How many holds of Ore do you want to buy [ 65 ]? ";
	static const uint8_t organics_prompt[] =
	    "How many holds of Organics do you want to sell [ 20 ]? ";
	static const uint8_t cancel[] =
	    "\r\nYou have 12345 credits and 65 empty cargo holds.\n\r"
	    "\r\nWe are selling up to 100.  You have 10 in your holds.\n\r"
	    "How many holds of Ore do you want to buy [ 65 ]? NO\r\n";
	static const uint8_t negative_cancel[] =
	    "\r\nYou have 12345 credits and 65 empty cargo holds.\n\r"
	    "\r\nWe are selling up to 100.  You have 10 in your holds.\n\r"
	    "How many holds of Ore do you want to buy [ 65 ]? -.1\r\n";
	static const uint8_t selling_capacity[] =
	    "\r\nYou have 12345 credits and 65 empty cargo holds.\n\r"
	    "\r\nWe are selling up to 100.  You have 10 in your holds.\n\r"
	    "How many holds of Ore do you want to buy [ 65 ]? 101\r\n"
	    "\r\n\x1b[0;36;40;5;1mWe don't have that much!\n\r";
	static const uint8_t buying_capacity[] =
	    "\r\nYou have 12345 credits and 65 empty cargo holds.\n\r"
	    "\r\nWe are buying up to 80.  You have 20 in your holds.\n\r"
	    "How many holds of Organics do you want to sell [ 20 ]? 81\r\n"
	    "\r\n\x1b[0;36;40;5;1mWe don't need that much!\n\r";
	static const uint8_t selling_maximum[] =
	    "\r\nYou have 100 credits and 65 empty cargo holds.\n\r"
	    "\r\nWe are selling up to 100.  You have 10 in your holds.\n\r"
	    "How many holds of Ore do you want to buy [ 5 ]? 6\r\n"
	    "\r\n\x1b[0;36;40;5;1mYou can't afford that much!\n\r";
	static const uint8_t buying_maximum[] =
	    "\r\nYou have 12345 credits and 65 empty cargo holds.\n\r"
	    "\r\nWe are buying up to 80.  You have 20 in your holds.\n\r"
	    "How many holds of Organics do you want to sell [ 20 ]? 21\r\n"
	    "\r\n\x1b[0;36;40;5;1mYou don't have that much!\n\r";
	static const uint8_t free_retry[] =
	    "\r\nYou have-100 credits and 65 empty cargo holds.\n\r"
	    "\r\nWe are selling up to 100.  You have 10 in your holds.\n\r"
	    "How many holds of Ore do you want to buy [ 100 ]? 66\r\n"
	    "\r\n\x1b[0;36;40;5;1mYou don't have enough cargo holds.\n\r"
	    "\x1b[0;36;40m\r\n"
	    "How many holds of Ore do you want to buy [ 100 ]? 2\r\n"
	    "Agreed, 2 units.\n\r"
	    "\r\nWe'll sell them for-2 credits.\n\r"
	    "Do you agree? [Y/n] \r\n"
	    "It's Yours!\n\r";
	static const uint8_t buying_accepted[] =
	    "\r\nYou have 12345 credits and 65 empty cargo holds.\n\r"
	    "\r\nWe are buying up to 80.  You have 20 in your holds.\n\r"
	    "How many holds of Organics do you want to sell [ 20 ]? \r\n"
	    "Agreed, 20 units.\n\r"
	    "\r\nWe'll buy them for 600 credits.\n\r"
	    "Do you agree? [Y/n] \r\n"
	    "We'll take them!\n\r";
	static const uint8_t status_100[] =
	    "You have 100 credits and 65 empty cargo holds.";
	static const uint8_t status_negative[] =
	    "You have-100 credits and 65 empty cargo holds.";
	static const uint8_t prompt_5[] =
	    "How many holds of Ore do you want to buy [ 5 ]? ";
	static const uint8_t prompt_100[] =
	    "How many holds of Ore do you want to buy [ 100 ]? ";
	static const uint8_t capacity_sell_error[] =
	    "We don't have that much!";
	static const uint8_t capacity_buy_error[] =
	    "We don't need that much!";
	static const uint8_t maximum_sell_error[] =
	    "You can't afford that much!";
	static const uint8_t maximum_buy_error[] =
	    "You don't have that much!";
	static const uint8_t free_error[] =
	    "You don't have enough cargo holds.";
	static const uint8_t agreed_2[] = "Agreed, 2 units.";
	static const uint8_t offer_negative[] =
	    "We'll sell them for-2 credits.";
	static const uint8_t agreed_20[] = "Agreed, 20 units.";
	static const uint8_t offer_600[] =
	    "We'll buy them for 600 credits.";
	static const uint8_t take_them[] = "We'll take them!";
	struct commodity_trade_join join;

	commodity_trade_join_init(&join);
	commodity_trade_join_front(&join, status, sizeof(status) - 1U,
	    selling, sizeof(selling) - 1U, ore_prompt,
	    sizeof(ore_prompt) - 1U, (const uint8_t *)"NO", 2U);
	commodity_trade_join_check(&join, cancel, sizeof(cancel) - 1U);

	commodity_trade_join_init(&join);
	commodity_trade_join_front(&join, status, sizeof(status) - 1U,
	    selling, sizeof(selling) - 1U, ore_prompt,
	    sizeof(ore_prompt) - 1U, (const uint8_t *)"-.1", 3U);
	commodity_trade_join_check(&join, negative_cancel,
	    sizeof(negative_cancel) - 1U);

	commodity_trade_join_init(&join);
	commodity_trade_join_front(&join, status, sizeof(status) - 1U,
	    selling, sizeof(selling) - 1U, ore_prompt,
	    sizeof(ore_prompt) - 1U, (const uint8_t *)"101", 3U);
	commodity_trade_join_02db(&join, capacity_sell_error,
	    sizeof(capacity_sell_error) - 1U);
	commodity_trade_join_check(&join, selling_capacity,
	    sizeof(selling_capacity) - 1U);

	commodity_trade_join_init(&join);
	commodity_trade_join_front(&join, status, sizeof(status) - 1U,
	    buying, sizeof(buying) - 1U, organics_prompt,
	    sizeof(organics_prompt) - 1U, (const uint8_t *)"81", 2U);
	commodity_trade_join_02db(&join, capacity_buy_error,
	    sizeof(capacity_buy_error) - 1U);
	commodity_trade_join_check(&join, buying_capacity,
	    sizeof(buying_capacity) - 1U);

	commodity_trade_join_init(&join);
	commodity_trade_join_front(&join, status_100,
	    sizeof(status_100) - 1U, selling, sizeof(selling) - 1U,
	    prompt_5, sizeof(prompt_5) - 1U, (const uint8_t *)"6", 1U);
	commodity_trade_join_02db(&join, maximum_sell_error,
	    sizeof(maximum_sell_error) - 1U);
	commodity_trade_join_check(&join, selling_maximum,
	    sizeof(selling_maximum) - 1U);

	commodity_trade_join_init(&join);
	commodity_trade_join_front(&join, status, sizeof(status) - 1U,
	    buying, sizeof(buying) - 1U, organics_prompt,
	    sizeof(organics_prompt) - 1U, (const uint8_t *)"21", 2U);
	commodity_trade_join_02db(&join, maximum_buy_error,
	    sizeof(maximum_buy_error) - 1U);
	commodity_trade_join_check(&join, buying_maximum,
	    sizeof(buying_maximum) - 1U);

	commodity_trade_join_init(&join);
	commodity_trade_join_front(&join, status_negative,
	    sizeof(status_negative) - 1U, selling, sizeof(selling) - 1U,
	    prompt_100, sizeof(prompt_100) - 1U, (const uint8_t *)"66", 2U);
	commodity_trade_join_02db(&join, free_error,
	    sizeof(free_error) - 1U);
	commodity_trade_join_line(&join);
	commodity_trade_join_b05d(&join, prompt_100,
	    sizeof(prompt_100) - 1U, true);
	commodity_trade_join_input(&join, (const uint8_t *)"2", 1U);
	commodity_trade_join_accepted_tail(&join, agreed_2,
	    sizeof(agreed_2) - 1U, offer_negative,
	    sizeof(offer_negative) - 1U, (const uint8_t *)"It's Yours!", 11U);
	commodity_trade_join_check(&join, free_retry, sizeof(free_retry) - 1U);

	commodity_trade_join_init(&join);
	commodity_trade_join_front(&join, status, sizeof(status) - 1U,
	    buying, sizeof(buying) - 1U, organics_prompt,
	    sizeof(organics_prompt) - 1U, NULL, 0U);
	commodity_trade_join_accepted_tail(&join, agreed_20,
	    sizeof(agreed_20) - 1U, offer_600, sizeof(offer_600) - 1U,
	    take_them, sizeof(take_them) - 1U);
	commodity_trade_join_check(&join, buying_accepted,
	    sizeof(buying_accepted) - 1U);
}

static void
test_commodity_trade_adapter_cuts(void)
{
	static const uint8_t status[] =
	    "You have 12345 credits and 65 empty cargo holds.";
	static const uint8_t selling[] =
	    "We are selling up to 100.  You have 10 in your holds.";
	static const uint8_t prompt[] =
	    "How many holds of Ore do you want to buy [ 65 ]? ";
	static const uint8_t agreed[] = "Agreed, 3 units.";
	static const uint8_t offer[] =
	    "We'll sell them for 60 credits.";
	static const uint8_t success[] = "It's Yours!";
	static const uint8_t carrier_before[] = "\r\n";
	static const uint8_t carrier_after[] =
	    "\r\nYou have 12345 credits and 65 empty cargo holds.";
	static const uint8_t editor_loop_head[] =
	    "\r\nYou have 12345 credits and 65 empty cargo holds.\n\r"
	    "\r\nWe are selling up to 100.  You have 10 in your holds.\n\r"
	    "How many holds of Ore do you want to buy [ 65 ]? ";
	static const uint8_t editor_after_two[] =
	    "\r\nYou have 12345 credits and 65 empty cargo holds.\n\r"
	    "\r\nWe are selling up to 100.  You have 10 in your holds.\n\r"
	    "How many holds of Ore do you want to buy [ 65 ]? 12";
	static const uint8_t confirmation_after_y[] =
	    "\r\nYou have 12345 credits and 65 empty cargo holds.\n\r"
	    "\r\nWe are selling up to 100.  You have 10 in your holds.\n\r"
	    "How many holds of Ore do you want to buy [ 65 ]? 3\r\n"
	    "Agreed, 3 units.\n\r"
	    "\r\nWe'll sell them for 60 credits.\n\r"
	    "Do you agree? [Y/n] Y";
	static const uint8_t confirmation[] = "Do you agree? [Y/n] ";
	static const uint8_t inactivity[] =
	    "\r\n\aUSER FELL ASLEEP!\n\r";
	static const uint8_t session_limit[] =
	    "\r\n\a\a\aTIME LIMIT EXCEEDED!\a\a\a\n\r";
	static const struct {
		enum yt_ab36_terminal_kind kind;
		const uint8_t *suffix;
		size_t suffix_length;
	} terminals[] = {
		{YT_AB36_TERMINAL_INACTIVITY, inactivity,
		    sizeof(inactivity) - 1U},
		{YT_AB36_TERMINAL_SESSION_LIMIT, session_limit,
		    sizeof(session_limit) - 1U},
	};
	static const uint8_t low_time[] =
	    "\r\nYou have 12345 credits and 65 empty cargo holds.\n\r"
	    "\r\nWe are selling up to 100.  You have 10 in your holds.\n\r"
	    "\r\n\x07\x1b[0;35;40;5;1mTime Left: 5:59  \r\n"
	    "\x1b[0;35;40m\r\n"
	    "How many holds of Ore do you want to buy [ 65 ]? 3\r\n"
	    "Agreed, 3 units.\n\r"
	    "\r\nWe'll sell them for 60 credits.\n\r"
	    "Do you agree? [Y/n] \r\n"
	    "It's Yours!\n\r";
	struct commodity_trade_join join;
	struct commodity_b05d_cut cut;
	struct commodity_editor_cut editor;
	struct yt_present_result result;
	char paged_text[80];
	float newline_flag;
	float remembered;
	bool handled;
	bool warned;
	size_t terminal_index;

	commodity_trade_join_init(&join);
	commodity_trade_join_line(&join);
	commodity_b05d_cut_init(&cut, &join, 0U);
	CHECK(!yt_paged_row_run(&join.pager, &join.current, &cut.key_state,
	    status, sizeof(status) - 1U, &commodity_b05d_cut_ops, &cut));
	commodity_trade_join_check(&join, carrier_before,
	    sizeof(carrier_before) - 1U);
	CHECK(cut.carrier_calls == 1U && cut.sample_calls == 0U
	    && cut.present_calls == 0U && cut.finish_calls == 0U
	    && join.pager.line_count == 0.0f);

	commodity_trade_join_init(&join);
	commodity_trade_join_line(&join);
	commodity_b05d_cut_init(&cut, &join, 1U);
	CHECK(!yt_paged_row_run(&join.pager, &join.current, &cut.key_state,
	    status, sizeof(status) - 1U, &commodity_b05d_cut_ops, &cut));
	commodity_trade_join_check(&join, carrier_after,
	    sizeof(carrier_after) - 1U);
	CHECK(cut.carrier_calls == 2U && cut.sample_calls == 1U
	    && cut.present_calls == 1U && cut.finish_calls == 0U
	    && join.pager.line_count == 0.0f);

	commodity_trade_join_init(&join);
	commodity_trade_join_0317(&join, status, sizeof(status) - 1U);
	commodity_trade_join_0317(&join, selling, sizeof(selling) - 1U);
	commodity_trade_join_b05d(&join, prompt, sizeof(prompt) - 1U, true);
	yt_pager_editor_enter(&join.pager, join.accumulator,
	    sizeof(join.accumulator));
	editor.join = &join;
	editor.carrier_calls = 0U;
	editor.fail_carrier_at = 0U;
	CHECK(!commodity_editor_cut_carrier(&editor));
	commodity_trade_join_check(&join, editor_loop_head,
	    sizeof(editor_loop_head) - 1U);
	CHECK(editor.carrier_calls == 1U && join.accumulator[0] == '\0'
	    && join.pager.line_count == 0.0f && join.pager.nonstop == 0.0f
	    && join.pager.key[0] == '\0');
	for (terminal_index = 0U; terminal_index < YT_ARRAY_LEN(terminals);
	    ++terminal_index) {
		struct commodity_terminal_join terminal;
		bool running = true;
		bool terminated = false;

		commodity_trade_join_init(&join);
		commodity_trade_join_0317(&join, status, sizeof(status) - 1U);
		commodity_trade_join_0317(&join, selling,
		    sizeof(selling) - 1U);
		commodity_trade_join_b05d(&join, prompt,
		    sizeof(prompt) - 1U, true);
		yt_pager_editor_enter(&join.pager, join.accumulator,
		    sizeof(join.accumulator));
		terminal.join = &join;
		terminal.closed = false;
		CHECK(yt_input_ab36_terminal_run(terminals[terminal_index].kind,
		    &running, &terminated, commodity_terminal_notice,
		    commodity_terminal_close, &terminal));
		CHECK(join.capture.remote_length
		    == sizeof(editor_loop_head) - 1U
		    + terminals[terminal_index].suffix_length
		    && memcmp(join.capture.remote, editor_loop_head,
		    sizeof(editor_loop_head) - 1U) == 0
		    && memcmp(join.capture.remote + sizeof(editor_loop_head) - 1U,
		    terminals[terminal_index].suffix,
		    terminals[terminal_index].suffix_length) == 0
		    && !running && terminated && terminal.closed
		    && join.pager.line_count == 1.0f
		    && join.pager.newline_flag == 0.0f);
	}

	commodity_trade_join_init(&join);
	commodity_trade_join_0317(&join, status, sizeof(status) - 1U);
	commodity_trade_join_0317(&join, selling, sizeof(selling) - 1U);
	commodity_trade_join_b05d(&join, prompt, sizeof(prompt) - 1U, true);
	yt_pager_editor_enter(&join.pager, join.accumulator,
	    sizeof(join.accumulator));
	(void)snprintf(paged_text, sizeof(paged_text), "%s", prompt);
	newline_flag = 0.0f;
	editor.join = &join;
	editor.carrier_calls = 0U;
	editor.fail_carrier_at = 1U;
	CHECK(yt_input_ab36_printable_run('1', join.accumulator,
	    sizeof(join.accumulator), sizeof(join.accumulator), paged_text,
	    sizeof(paged_text), &newline_flag, &handled,
	    commodity_editor_cut_echo, commodity_editor_cut_carrier, &editor));
	CHECK(handled);
	CHECK(!yt_input_ab36_printable_run('2', join.accumulator,
	    sizeof(join.accumulator), sizeof(join.accumulator), paged_text,
	    sizeof(paged_text), &newline_flag, &handled,
	    commodity_editor_cut_echo, commodity_editor_cut_carrier, &editor));
	commodity_trade_join_check(&join, editor_after_two,
	    sizeof(editor_after_two) - 1U);
	CHECK(handled && editor.carrier_calls == 2U
	    && strcmp(join.accumulator, "12") == 0
	    && strcmp(paged_text, "2") == 0 && newline_flag == 1.0f);

	commodity_trade_join_init(&join);
	commodity_trade_join_front(&join, status, sizeof(status) - 1U,
	    selling, sizeof(selling) - 1U, prompt, sizeof(prompt) - 1U,
	    (const uint8_t *)"3", 1U);
	commodity_trade_join_b05d(&join, agreed, sizeof(agreed) - 1U, false);
	commodity_trade_join_0317(&join, offer, sizeof(offer) - 1U);
	CHECK(yt_present_character(confirmation, sizeof(confirmation) - 1U,
	    &join.current, &result) == YT_PRESENT_OK);
	pager_capture_result(&join.capture, &result);
	yt_pager_editor_enter(&join.pager, join.accumulator,
	    sizeof(join.accumulator));
	(void)snprintf(paged_text, sizeof(paged_text), "%s", confirmation);
	newline_flag = 0.0f;
	editor.join = &join;
	editor.carrier_calls = 0U;
	editor.fail_carrier_at = 0U;
	CHECK(!yt_input_ab36_printable_run('Y', join.accumulator,
	    sizeof(join.accumulator), sizeof(join.accumulator), paged_text,
	    sizeof(paged_text), &newline_flag, &handled,
	    commodity_editor_cut_echo, commodity_editor_cut_carrier, &editor));
	commodity_trade_join_check(&join, confirmation_after_y,
	    sizeof(confirmation_after_y) - 1U);
	CHECK(handled && editor.carrier_calls == 1U
	    && strcmp(join.accumulator, "Y") == 0
	    && strcmp(paged_text, "Y") == 0 && newline_flag == 1.0f);
	for (terminal_index = 0U; terminal_index < YT_ARRAY_LEN(terminals);
	    ++terminal_index) {
		struct commodity_terminal_join terminal;
		bool running = true;
		bool terminated = false;
		size_t prefix_length = sizeof(confirmation_after_y) - 2U;

		commodity_trade_join_init(&join);
		commodity_trade_join_front(&join, status, sizeof(status) - 1U,
		    selling, sizeof(selling) - 1U, prompt, sizeof(prompt) - 1U,
		    (const uint8_t *)"3", 1U);
		commodity_trade_join_b05d(&join, agreed,
		    sizeof(agreed) - 1U, false);
		commodity_trade_join_0317(&join, offer, sizeof(offer) - 1U);
		CHECK(yt_present_character(confirmation,
		    sizeof(confirmation) - 1U, &join.current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&join.capture, &result);
		yt_pager_editor_enter(&join.pager, join.accumulator,
		    sizeof(join.accumulator));
		terminal.join = &join;
		terminal.closed = false;
		CHECK(yt_input_ab36_terminal_run(terminals[terminal_index].kind,
		    &running, &terminated, commodity_terminal_notice,
		    commodity_terminal_close, &terminal));
		CHECK(join.capture.remote_length == prefix_length
		    + terminals[terminal_index].suffix_length
		    && memcmp(join.capture.remote, confirmation_after_y,
		    prefix_length) == 0
		    && memcmp(join.capture.remote + prefix_length,
		    terminals[terminal_index].suffix,
		    terminals[terminal_index].suffix_length) == 0
		    && !running && terminated && terminal.closed
		    && join.pager.line_count == 1.0f
		    && join.pager.newline_flag == 0.0f);
	}

	commodity_trade_join_init(&join);
	commodity_trade_join_0317(&join, status, sizeof(status) - 1U);
	commodity_trade_join_0317(&join, selling, sizeof(selling) - 1U);
	remembered = 6.0f;
	CHECK(yt_present_low_time((const uint8_t *)" 5:59  ", 7U,
	    &remembered, &join.current, &result, &warned) == YT_PRESENT_OK);
	pager_capture_result(&join.capture, &result);
	CHECK(warned && remembered == 5.0f);
	commodity_trade_join_b05d(&join, prompt, sizeof(prompt) - 1U, true);
	commodity_trade_join_input(&join, (const uint8_t *)"3", 1U);
	commodity_trade_join_accepted_tail(&join, agreed, sizeof(agreed) - 1U,
	    offer, sizeof(offer) - 1U, success, sizeof(success) - 1U);
	commodity_trade_join_check(&join, low_time, sizeof(low_time) - 1U);
	CHECK(join.capture.remote_length == 297U);
}

static void
test_commodity_trade_recursive_pager(void)
{
	static const uint8_t status[] =
	    "You have 12345 credits and 65 empty cargo holds.";
	static const uint8_t selling[] =
	    "We are selling up to 100.  You have 10 in your holds.";
	static const uint8_t prompt[] =
	    "How many holds of Ore do you want to buy [ 65 ]? ";
	static const uint8_t agreed[] = "Agreed, 3 units.";
	static const uint8_t offer[] =
	    "We'll sell them for 60 credits.";
	static const uint8_t success[] = "It's Yours!";
	static const uint8_t expected[] =
	    "\r\nYou have 12345 credits and 65 empty cargo holds.\n\r"
	    "\x1b[0;33;40;1m"
	    "[ENTER] for more, [E] to end, or [NS] for Non-stop "
	    "\x1b[0;33;40m\r\n"
	    "\x1b[0;36;40m\r\n"
	    "We are selling up to 100.  You have 10 in your holds.\n\r"
	    "How many holds of Ore do you want to buy [ 65 ]? 3\r\n"
	    "Agreed, 3 units.\n\r"
	    "\r\nWe'll sell them for 60 credits.\n\r"
	    "Do you agree? [Y/n] \r\n"
	    "It's Yours!\n\r";
	struct commodity_trade_join join;
	struct commodity_b05d_cut cut;

	commodity_trade_join_init(&join);
	join.pager.line_count = 22.0f;
	commodity_trade_join_line(&join);
	commodity_b05d_cut_init(&cut, &join, SIZE_MAX);
	cut.accept_response = true;
	CHECK(yt_paged_row_run(&join.pager, &join.current, &cut.key_state,
	    status, sizeof(status) - 1U, &commodity_b05d_cut_ops, &cut));
	CHECK(cut.carrier_calls == 4U && cut.sample_calls == 2U
	    && cut.present_calls == 2U && cut.finish_calls == 2U
	    && cut.response_calls == 1U);
	commodity_trade_join_0317(&join, selling, sizeof(selling) - 1U);
	commodity_trade_join_b05d(&join, prompt, sizeof(prompt) - 1U, true);
	commodity_trade_join_input(&join, (const uint8_t *)"3", 1U);
	commodity_trade_join_accepted_tail(&join, agreed, sizeof(agreed) - 1U,
	    offer, sizeof(offer) - 1U, success, sizeof(success) - 1U);
	commodity_trade_join_check(&join, expected, sizeof(expected) - 1U);
	CHECK(join.capture.remote_length == 334U
	    && join.pager.line_count == 1.0f && join.pager.nonstop == 0.0f
	    && join.pager.key[0] == '\0' && join.current.foreground == 6.0f);

	commodity_trade_join_init(&join);
	commodity_trade_join_line(&join);
	commodity_b05d_cut_init(&cut, &join, SIZE_MAX);
	cut.sampled.bytes[0] = '3';
	cut.sampled.length = 1U;
	CHECK(yt_paged_row_run(&join.pager, &join.current, &cut.key_state,
	    status, sizeof(status) - 1U, &commodity_b05d_cut_ops, &cut));
	CHECK(cut.carrier_calls == 2U && cut.sample_calls == 1U
	    && cut.present_calls == 1U && cut.finish_calls == 1U
	    && cut.response_calls == 0U && cut.queue_position == 0U
	    && cut.queue_length == 1U && strcmp(cut.queue, "3") == 0
	    && join.accumulator[0] == '\0' && join.pager.key[0] == '\0'
	    && join.pager.line_count == 1.0f);
}

static void
test_port_report_b05d_adapter_cuts(void)
{
	static const uint8_t owner[] =
	    "This port is owned by: YOU, Credits: 1234.5";
	static const uint8_t title[] =
	    "Commerce report for Argus: 07-25-2026 12:34:56";
	static const uint8_t header[] =
	    " Items         Status      # units    in holds   Cost";
	static const uint8_t rule[] =
	    "=======       =========   =========   ========   ====";
	static const uint8_t carrier_before[] =
	    "\r\nThis port is owned by: YOU, Credits: 1234.5\r\n\r\n";
	static const uint8_t carrier_after[] =
	    "\r\nThis port is owned by: YOU, Credits: 1234.5\r\n"
	    "\r\nCommerce report for Argus: 07-25-2026 12:34:56";
	struct commodity_trade_join join;
	struct commodity_b05d_cut cut;

	commodity_trade_join_init(&join);
	join.current = state(false);
	commodity_trade_join_line(&join);
	pager_capture_line(&join.capture, &join.current, owner,
	    sizeof(owner) - 1U);
	commodity_trade_join_line(&join);
	commodity_b05d_cut_init(&cut, &join, 0U);
	CHECK(!yt_paged_row_run(&join.pager, &join.current, &cut.key_state,
	    title, sizeof(title) - 1U, &commodity_b05d_cut_ops, &cut));
	CHECK(sizeof(carrier_before) - 1U == 49U);
	commodity_trade_join_check(&join, carrier_before,
	    sizeof(carrier_before) - 1U);
	CHECK(cut.carrier_calls == 1U && cut.sample_calls == 0U
	    && cut.present_calls == 0U && cut.finish_calls == 0U
	    && join.pager.line_count == 0.0f);

	commodity_trade_join_init(&join);
	join.current = state(false);
	commodity_trade_join_line(&join);
	pager_capture_line(&join.capture, &join.current, owner,
	    sizeof(owner) - 1U);
	commodity_trade_join_line(&join);
	commodity_b05d_cut_init(&cut, &join, 1U);
	CHECK(!yt_paged_row_run(&join.pager, &join.current, &cut.key_state,
	    title, sizeof(title) - 1U, &commodity_b05d_cut_ops, &cut));
	CHECK(sizeof(carrier_after) - 1U == 95U);
	commodity_trade_join_check(&join, carrier_after,
	    sizeof(carrier_after) - 1U);
	CHECK(cut.carrier_calls == 2U && cut.sample_calls == 1U
	    && cut.present_calls == 1U && cut.finish_calls == 0U
	    && join.pager.line_count == 0.0f);

	commodity_trade_join_init(&join);
	join.current = state(false);
	commodity_b05d_cut_init(&cut, &join, SIZE_MAX);
	cut.sampled.bytes[0] = 'A';
	cut.sampled.length = 1U;
	CHECK(yt_paged_row_run(&join.pager, &join.current, &cut.key_state,
	    title, sizeof(title) - 1U, &commodity_b05d_cut_ops, &cut));
	cut.sampled.bytes[0] = 0x18U;
	CHECK(yt_paged_row_run(&join.pager, &join.current, &cut.key_state,
	    header, sizeof(header) - 1U, &commodity_b05d_cut_ops, &cut));
	join.current.bold = 1.0f;
	cut.sampled.bytes[0] = 'B';
	CHECK(yt_paged_row_run(&join.pager, &join.current, &cut.key_state,
	    rule, sizeof(rule) - 1U, &commodity_b05d_cut_ops, &cut));
	CHECK(cut.carrier_calls == 6U && cut.sample_calls == 3U
	    && cut.present_calls == 3U && cut.finish_calls == 3U
	    && cut.response_calls == 0U && cut.queue_position == 0U
	    && cut.queue_length == 1U && strcmp(cut.queue, "B") == 0
	    && strcmp(join.pager.key, "Q") == 0
	    && join.pager.line_count == 3.0f);
}

static void
computer_return_prompt_fixture(bool ansi, float mode,
    struct pager_capture *capture, struct yt_present_state *current,
    struct yt_pager_state *pager)
{
	static const uint8_t prompt[] =
	    "Time: 14:59  Computer command (?=help)? ";
	struct yt_present_result result;

	*current = state(ansi);
	current->sound.mode = mode;
	current->foreground = 6.0f;
	if (ansi)
		CHECK(yt_present_color(current, &result) == YT_PRESENT_OK);
	memset(pager, 0, sizeof(*pager));
	pager->foreground = 6;
	memset(capture, 0, sizeof(*capture));
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	current->foreground = 1.0f;
	pager->foreground = 1;
	pager->newline_flag = 1.0f;
	pager_fixture_b05d(pager, current, prompt, sizeof(prompt) - 1U,
	    capture);
}

static void
test_computer_return_prompt_presentation(void)
{
	static const uint8_t ansi[] =
	    "\r\n\x1b[0;31;40mTime: 14:59  Computer command (?=help)? ";
	static const uint8_t plain[] =
	    "\r\nTime: 14:59  Computer command (?=help)? ";
	struct yt_present_state current;
	struct yt_pager_state pager;
	struct pager_capture capture;

	computer_return_prompt_fixture(true, 0.0f, &capture, &current,
	    &pager);
	CHECK(sizeof(ansi) - 1U == 52U);
	CHECK(capture.remote_length == sizeof(ansi) - 1U
	    && memcmp(capture.remote, ansi, sizeof(ansi) - 1U) == 0);
	CHECK(pager.line_count == 1.0f && pager.newline_flag == 0.0f);

	computer_return_prompt_fixture(false, 0.0f, &capture, &current,
	    &pager);
	CHECK(sizeof(plain) - 1U == 42U);
	CHECK(capture.remote_length == sizeof(plain) - 1U
	    && memcmp(capture.remote, plain, sizeof(plain) - 1U) == 0);

	computer_return_prompt_fixture(true, 1.0f, &capture, &current,
	    &pager);
	CHECK(capture.remote_length == 0U && pager.line_count == 1.0f);

	computer_return_prompt_fixture(true, 2.0f, &capture, &current,
	    &pager);
	CHECK(capture.remote_length == 2U
	    && memcmp(capture.remote, "\r\n", 2U) == 0
	    && pager.line_count == 1.0f);
}

static void
computer_quit_cancel_cycle_fixture(bool ansi,
    struct pager_capture *capture, struct yt_present_state *current,
    struct yt_pager_state *pager)
{
	static const uint8_t heading[] = "<Quit>";
	static const uint8_t confirmation[] = "Are you sure (Y/N)? ";
	static const uint8_t prompt[] =
	    "Time: 14:59  Computer command (?=help)? ";
	static const uint8_t q[] = "Q";
	static const uint8_t n[] = "N";
	struct yt_present_result result;
	char accumulator[80] = "";

	computer_return_prompt_fixture(ansi, 0.0f, capture, current, pager);
	yt_pager_editor_enter(pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo(q, sizeof(q) - 1U, q,
	    sizeof(q) - 1U, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);

	current->foreground = 7.0f;
	pager->foreground = 7;
	pager_fixture_b05d(pager, current, heading, sizeof(heading) - 1U,
	    capture);
	CHECK(yt_present_character(confirmation, sizeof(confirmation) - 1U,
	    current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	yt_pager_editor_enter(pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo(n, sizeof(n) - 1U, n,
	    sizeof(n) - 1U, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);

	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	current->foreground = 1.0f;
	pager->foreground = 1;
	pager->newline_flag = 1.0f;
	pager_fixture_b05d(pager, current, prompt, sizeof(prompt) - 1U,
	    capture);
}

static void
test_computer_quit_cancel_presentation(void)
{
	static const uint8_t ansi[] =
	    "\r\n\x1b[0;31;40m"
	    "Time: 14:59  Computer command (?=help)? Q\r\n"
	    "\x1b[0;37;40m<Quit>\n\r"
	    "Are you sure (Y/N)? N\r\n"
	    "\r\n\x1b[0;31;40m"
	    "Time: 14:59  Computer command (?=help)? ";
	static const uint8_t plain[] =
	    "\r\nTime: 14:59  Computer command (?=help)? Q\r\n"
	    "<Quit>\n\rAre you sure (Y/N)? N\r\n"
	    "\r\nTime: 14:59  Computer command (?=help)? ";
	struct yt_present_state current;
	struct yt_pager_state pager;
	struct pager_capture capture;

	computer_quit_cancel_cycle_fixture(true, &capture, &current, &pager);
	CHECK(sizeof(ansi) - 1U == 148U);
	CHECK(capture.remote_length == sizeof(ansi) - 1U
	    && memcmp(capture.remote, ansi, sizeof(ansi) - 1U) == 0);
	CHECK(pager.line_count == 1.0f && pager.newline_flag == 0.0f
	    && current.foreground == 1.0f);

	computer_quit_cancel_cycle_fixture(false, &capture, &current, &pager);
	CHECK(sizeof(plain) - 1U == 118U);
	CHECK(capture.remote_length == sizeof(plain) - 1U
	    && memcmp(capture.remote, plain, sizeof(plain) - 1U) == 0);
	CHECK(pager.line_count == 1.0f && pager.newline_flag == 0.0f);
}

static void
test_main_quit_cancel_presentation(void)
{
	static const uint8_t heading[] = "<Quit>";
	static const uint8_t confirmation[] = "Are you sure (Y/N)? ";
	static const uint8_t prompt[] =
	    "Time: 14:59  Main Command (?=Help)? ";
	static const uint8_t command[] = "Qjunk";
	static const uint8_t answer[] = "N";
	static const uint8_t ansi[] =
	    "Qjunk\r\n\x1b[0;37;40m<Quit>\n\r"
	    "Are you sure (Y/N)? N\r\n"
	    "\x1b[0;32;40m\r\nTime: 14:59  Main Command (?=Help)? ";
	static const uint8_t plain[] =
	    "Qjunk\r\n<Quit>\n\rAre you sure (Y/N)? N\r\n"
	    "\r\nTime: 14:59  Main Command (?=Help)? ";
	struct yt_present_state current;
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	char accumulator[80] = "";
	int ansi_enabled;

	for (ansi_enabled = 0; ansi_enabled <= 1; ++ansi_enabled) {
		const uint8_t *expected = ansi_enabled ? ansi : plain;
		size_t expected_length = ansi_enabled
		    ? sizeof(ansi) - 1U : sizeof(plain) - 1U;

		current = state(ansi_enabled != 0);
		if (ansi_enabled != 0)
			CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
		memset(&pager, 0, sizeof(pager));
		pager.foreground = 2;
		memset(&capture, 0, sizeof(capture));
		yt_pager_editor_enter(&pager, accumulator,
		    sizeof(accumulator));
		CHECK(yt_present_editor_echo(command, sizeof(command) - 1U,
		    command, sizeof(command) - 1U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		CHECK(yt_present_line(NULL, 0, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		current.foreground = 7.0f;
		pager.foreground = 7;
		pager_fixture_b05d(&pager, &current, heading,
		    sizeof(heading) - 1U, &capture);
		CHECK(yt_present_character(confirmation,
		    sizeof(confirmation) - 1U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		yt_pager_editor_enter(&pager, accumulator,
		    sizeof(accumulator));
		CHECK(yt_present_editor_echo(answer, sizeof(answer) - 1U,
		    answer, sizeof(answer) - 1U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		CHECK(yt_present_line(NULL, 0, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		current.foreground = 2.0f;
		pager.foreground = 2;
		CHECK(yt_present_line(NULL, 0, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		pager.newline_flag = 1.0f;
		pager_fixture_b05d(&pager, &current, prompt,
		    sizeof(prompt) - 1U, &capture);
		CHECK(capture.remote_length == expected_length);
		CHECK(capture.remote_length != expected_length
		    || memcmp(capture.remote, expected, expected_length) == 0);
	}
}

static void
main_shell_help_cycle_fixture(bool ansi, struct pager_capture *capture,
    struct yt_present_state *current, struct yt_pager_state *pager)
{
	static const uint8_t prompt[] =
	    "Time: 14:59  Main Command (?=Help)? ";
	static const uint8_t question[] = "?";
	static const uint8_t heading[] = "<Help>";
	static const char *const pairs[][2] = {
		{"[ENTER] - Re-display sector",
		    "$ - Take Credits from your ports"},
		{"! - Launch a Cruise Missile",
		    "A - <A>ttack a player's ship"},
		{"B - <B>uy a Port", "C - Ship's <C>omputer"},
		{"D - <D>rop a Sector mine",
		    "F - Take or leave <F>ighters"},
		{"G - Initiate <G>enesis", "I - <I>nfo on your ship"},
		{"L - <L>and on or create a planet",
		    "M - <M>ove to another sector"},
		{"N - Re<N>ame Port",
		    "P - Dock at a <P>ort (and trade)"},
		{"Q - <Q>uit game", "S - <S>ensors"},
		{"T - <T>eam menu", "V - <V>ersion Info"},
		{"W - Emergency <W>arp", "X - Sound Effects On/Off"},
		{"Z - Instructions", "+ - Fire Plasma Bolt"},
	};
	static const char *const narrative[] = {
		"String commands by seperating them with a semicolons (;).",
		"To place an EXTRA 'hit enter' in a string, use an extra ';'.",
		"Save a command string by placing a '/' at the end.",
		"Then hit Control-R to [R]eplay the saved command.",
		"You may repeat any command up to 20 times by putting",
		"a /R# at the end of your command. Replace the '#' with",
		"any number between 2 and 20. Example: your command/R20",
	};
	struct yt_present_result result;
	char accumulator[80] = "inherited";
	char row[128];
	size_t index;

	*current = state(ansi);
	memset(pager, 0, sizeof(*pager));
	pager->foreground = 2;
	memset(capture, 0, sizeof(*capture));
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager->newline_flag = 1.0f;
	pager_fixture_b05d(pager, current, prompt, sizeof(prompt) - 1U,
	    capture);
	yt_pager_editor_enter(pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo(question, sizeof(question) - 1U,
	    question, sizeof(question) - 1U, current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);

	current->foreground = 6.0f;
	pager->foreground = 6;
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager_fixture_b05d(pager, current, heading, sizeof(heading) - 1U,
	    capture);
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	for (index = 0; index < YT_ARRAY_LEN(pairs); ++index) {
		int length = snprintf(row, sizeof(row), "%-40s%s",
		    pairs[index][0], pairs[index][1]);

		CHECK(length > 0 && (size_t)length < sizeof(row));
		if (length > 0 && (size_t)length < sizeof(row))
			pager_capture_line(capture, current,
			    (const uint8_t *)row, (size_t)length);
	}
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager_fixture_b05d(pager, current,
	    (const uint8_t *)narrative[0], strlen(narrative[0]), capture);
	for (index = 1; index < YT_ARRAY_LEN(narrative); ++index)
		pager_fixture_b05d(pager, current,
		    (const uint8_t *)narrative[index], strlen(narrative[index]),
		    capture);

	pager->line_count = 0.0f;
	current->foreground = 2.0f;
	pager->foreground = 2;
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager->newline_flag = 1.0f;
	pager_fixture_b05d(pager, current, prompt, sizeof(prompt) - 1U,
	    capture);
}

static void
test_main_shell_front_presentation(void)
{
	static const uint8_t plain[] =
	    "\r\nTime: 14:59  Main Command (?=Help)? ?\r\n"
	    "\r\n<Help>\n\r\r\n"
	    "[ENTER] - Re-display sector             $ - Take Credits from your ports\r\n"
	    "! - Launch a Cruise Missile             A - <A>ttack a player's ship\r\n"
	    "B - <B>uy a Port                        C - Ship's <C>omputer\r\n"
	    "D - <D>rop a Sector mine                F - Take or leave <F>ighters\r\n"
	    "G - Initiate <G>enesis                  I - <I>nfo on your ship\r\n"
	    "L - <L>and on or create a planet        M - <M>ove to another sector\r\n"
	    "N - Re<N>ame Port                       P - Dock at a <P>ort (and trade)\r\n"
	    "Q - <Q>uit game                         S - <S>ensors\r\n"
	    "T - <T>eam menu                         V - <V>ersion Info\r\n"
	    "W - Emergency <W>arp                    X - Sound Effects On/Off\r\n"
	    "Z - Instructions                        + - Fire Plasma Bolt\r\n"
	    "\r\nString commands by seperating them with a semicolons (;).\n\r"
	    "To place an EXTRA 'hit enter' in a string, use an extra ';'.\n\r"
	    "Save a command string by placing a '/' at the end.\n\r"
	    "Then hit Control-R to [R]eplay the saved command.\n\r"
	    "You may repeat any command up to 20 times by putting\n\r"
	    "a /R# at the end of your command. Replace the '#' with\n\r"
	    "any number between 2 and 20. Example: your command/R20\n\r"
	    "\r\nTime: 14:59  Main Command (?=Help)? ";
	static const uint8_t ansi[] =
	    "\x1b[0;32;40m\r\nTime: 14:59  Main Command (?=Help)? ?\r\n"
	    "\x1b[0;36;40m\r\n<Help>\n\r\r\n"
	    "[ENTER] - Re-display sector             $ - Take Credits from your ports\r\n"
	    "! - Launch a Cruise Missile             A - <A>ttack a player's ship\r\n"
	    "B - <B>uy a Port                        C - Ship's <C>omputer\r\n"
	    "D - <D>rop a Sector mine                F - Take or leave <F>ighters\r\n"
	    "G - Initiate <G>enesis                  I - <I>nfo on your ship\r\n"
	    "L - <L>and on or create a planet        M - <M>ove to another sector\r\n"
	    "N - Re<N>ame Port                       P - Dock at a <P>ort (and trade)\r\n"
	    "Q - <Q>uit game                         S - <S>ensors\r\n"
	    "T - <T>eam menu                         V - <V>ersion Info\r\n"
	    "W - Emergency <W>arp                    X - Sound Effects On/Off\r\n"
	    "Z - Instructions                        + - Fire Plasma Bolt\r\n"
	    "\r\nString commands by seperating them with a semicolons (;).\n\r"
	    "To place an EXTRA 'hit enter' in a string, use an extra ';'.\n\r"
	    "Save a command string by placing a '/' at the end.\n\r"
	    "Then hit Control-R to [R]eplay the saved command.\n\r"
	    "You may repeat any command up to 20 times by putting\n\r"
	    "a /R# at the end of your command. Replace the '#' with\n\r"
	    "any number between 2 and 20. Example: your command/R20\n\r"
	    "\x1b[0;32;40m\r\nTime: 14:59  Main Command (?=Help)? ";
	struct yt_present_state current;
	struct yt_pager_state pager;
	struct pager_capture capture;

	main_shell_help_cycle_fixture(false, &capture, &current, &pager);
	CHECK(sizeof(plain) - 1U == 1212U);
	CHECK(capture.remote_length == sizeof(plain) - 1U
	    && memcmp(capture.remote, plain, sizeof(plain) - 1U) == 0);
	CHECK(pager.line_count == 1.0f && pager.newline_flag == 0.0f
	    && current.foreground == 2.0f);

	main_shell_help_cycle_fixture(true, &capture, &current, &pager);
	CHECK(sizeof(ansi) - 1U == 1242U);
	CHECK(capture.remote_length == sizeof(ansi) - 1U
	    && memcmp(capture.remote, ansi, sizeof(ansi) - 1U) == 0);
	CHECK(pager.line_count == 1.0f && pager.newline_flag == 0.0f
	    && current.foreground == 2.0f);
}

static void
test_main_shell_branch_presentation(void)
{
	static const uint8_t prompt[] =
	    "Time: 14:59  Main Command (?=Help)? ";
	static const uint8_t invalid[] = "Invalid command.";
	static const uint8_t plain_invalid[] =
	    "@\r\n\r\nInvalid command.\n\r"
	    "\r\nTime: 14:59  Main Command (?=Help)? ";
	static const uint8_t ansi_invalid[] =
	    "@\r\n\r\n\x1b[0;32;40;5;1mInvalid command.\n\r"
	    "\x1b[0;32;40m\r\nTime: 14:59  Main Command (?=Help)? ";
	static const uint8_t empty_display[] = "\r\n\r\n<Display>\n\r";
	static const uint8_t instruction_default[] =
	    "Zjunk\r\n<Instructions>\n\r"
	    "Do you want instructions (Y/N) [N]? \r\n";
	struct yt_present_state current;
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	char accumulator[80] = "inherited";
	int ansi_enabled;

	for (ansi_enabled = 0; ansi_enabled <= 1; ++ansi_enabled) {
		const uint8_t command = '@';
		const uint8_t *expected = ansi_enabled
		    ? ansi_invalid : plain_invalid;
		size_t expected_length = ansi_enabled
		    ? sizeof(ansi_invalid) - 1U : sizeof(plain_invalid) - 1U;

		current = state(ansi_enabled != 0);
		if (ansi_enabled != 0)
			CHECK(yt_present_color(&current, &result)
			    == YT_PRESENT_OK);
		memset(&pager, 0, sizeof(pager));
		pager.foreground = 2;
		memset(&capture, 0, sizeof(capture));
		yt_pager_editor_enter(&pager, accumulator,
		    sizeof(accumulator));
		CHECK(yt_present_editor_echo(&command, 1U, &command, 1U,
		    &current, &result) == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		CHECK(yt_present_line(NULL, 0, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		CHECK(yt_present_line(NULL, 0, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		current.bold = 1.0f;
		current.blink = 1.0f;
		pager_fixture_b05d(&pager, &current, invalid,
		    sizeof(invalid) - 1U, &capture);
		pager.line_count = 0.0f;
		CHECK(yt_present_line(NULL, 0, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		pager.newline_flag = 1.0f;
		pager_fixture_b05d(&pager, &current, prompt,
		    sizeof(prompt) - 1U, &capture);
		CHECK(capture.remote_length == expected_length
		    && memcmp(capture.remote, expected, expected_length) == 0);
	}

	current = state(false);
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 2;
	memset(&capture, 0, sizeof(capture));
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current, (const uint8_t *)"<Display>",
	    strlen("<Display>"), &capture);
	CHECK(capture.remote_length == sizeof(empty_display) - 1U
	    && memcmp(capture.remote, empty_display,
	    sizeof(empty_display) - 1U) == 0);

	current = state(false);
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 2;
	memset(&capture, 0, sizeof(capture));
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo((const uint8_t *)"Zjunk", 5U,
	    (const uint8_t *)"Zjunk", 5U, &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current,
	    (const uint8_t *)"<Instructions>", strlen("<Instructions>"),
	    &capture);
	CHECK(yt_present_character(
	    (const uint8_t *)"Do you want instructions (Y/N) [N]? ",
	    strlen("Do you want instructions (Y/N) [N]? "), &current,
	    &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(capture.remote_length == sizeof(instruction_default) - 1U
	    && memcmp(capture.remote, instruction_default,
	    sizeof(instruction_default) - 1U) == 0);
}

static void
planet_quit_cancel_cycle_fixture(bool ansi,
    struct pager_capture *capture, struct yt_present_state *current,
    struct yt_pager_state *pager)
{
	static const uint8_t free_holds[] =
	    "You have 5 free cargo holds.";
	static const uint8_t prompt[] =
	    "Time: 14:59  Planet command (?=help) [A]? ";
	static const uint8_t heading[] = "<Quit>";
	static const uint8_t confirmation[] = "Are you sure (Y/N)? ";
	static const uint8_t command[] = "Q";
	static const uint8_t answer[] = "N";
	struct yt_present_result result;
	char accumulator[80] = "";

	*current = state(ansi);
	current->foreground = 6.0f;
	if (ansi)
		CHECK(yt_present_color(current, &result) == YT_PRESENT_OK);
	memset(pager, 0, sizeof(*pager));
	pager->foreground = 6;
	memset(capture, 0, sizeof(*capture));
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager_fixture_b05d(pager, current, free_holds,
	    sizeof(free_holds) - 1U, capture);
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager->newline_flag = 1.0f;
	pager_fixture_b05d(pager, current, prompt, sizeof(prompt) - 1U,
	    capture);
	yt_pager_editor_enter(pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo(command, sizeof(command) - 1U,
	    command, sizeof(command) - 1U, current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);

	current->foreground = 7.0f;
	pager->foreground = 7;
	pager_fixture_b05d(pager, current, heading, sizeof(heading) - 1U,
	    capture);
	CHECK(yt_present_character(confirmation, sizeof(confirmation) - 1U,
	    current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	yt_pager_editor_enter(pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo(answer, sizeof(answer) - 1U,
	    answer, sizeof(answer) - 1U, current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);

	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager_fixture_b05d(pager, current, free_holds,
	    sizeof(free_holds) - 1U, capture);
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	current->foreground = 6.0f;
	pager->foreground = 6;
	pager->newline_flag = 1.0f;
	pager_fixture_b05d(pager, current, prompt, sizeof(prompt) - 1U,
	    capture);
}

static void
test_planet_quit_cancel_presentation(void)
{
	static const uint8_t ansi[] =
	    "\r\nYou have 5 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? Q\r\n"
	    "\x1b[0;37;40m<Quit>\n\r"
	    "Are you sure (Y/N)? N\r\n"
	    "\r\nYou have 5 free cargo holds.\n\r"
	    "\r\n\x1b[0;36;40m"
	    "Time: 14:59  Planet command (?=help) [A]? ";
	static const uint8_t plain[] =
	    "\r\nYou have 5 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? Q\r\n"
	    "<Quit>\n\rAre you sure (Y/N)? N\r\n"
	    "\r\nYou have 5 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? ";
	struct yt_present_state current;
	struct yt_pager_state pager;
	struct pager_capture capture;

	planet_quit_cancel_cycle_fixture(true, &capture, &current, &pager);
	CHECK(sizeof(ansi) - 1U == 206U);
	CHECK(capture.remote_length == sizeof(ansi) - 1U
	    && memcmp(capture.remote, ansi, sizeof(ansi) - 1U) == 0);
	CHECK(pager.line_count == 2.0f && pager.newline_flag == 0.0f);
	planet_quit_cancel_cycle_fixture(false, &capture, &current, &pager);
	CHECK(sizeof(plain) - 1U == 186U);
	CHECK(capture.remote_length == sizeof(plain) - 1U
	    && memcmp(capture.remote, plain, sizeof(plain) - 1U) == 0);
	CHECK(pager.line_count == 2.0f && pager.newline_flag == 0.0f);
}

static void
planet_menu_front_cycle_fixture(bool help,
    struct pager_capture *capture, struct yt_present_state *current,
    struct yt_pager_state *pager)
{
	static const uint8_t free_holds[] =
	    "You have 65 free cargo holds.";
	static const uint8_t prompt[] =
	    "Time: 14:59  Planet command (?=help) [A]? ";
	static const uint8_t heading[] = "<Help>";
	static const uint8_t invalid[] = "Invalid command.";
	static const char *const rows[] = {
		"1 - Take Ore", "2 - Take Organics", "3 - Take Equipment",
		"4 - Take Fighters", "5 - Take Missiles", "6 - Take Mines",
		"9 - Take Plasma Bolts", "A - Take <A>ll (Default)",
		"B - Planet's <B>ank", "D - <D>isplay Planet",
		"F - Take/Leave Ground <F>orces", "L - <L>eave Planet",
		"N - Re-<N>ame Planet", "T - <T>ransfer Cargo to Planet",
		"! - Use Planet Thrusters", "$ - Raise Productivity"
	};
	const uint8_t response = help ? '?' : 'X';
	struct yt_present_result result;
	char accumulator[80] = "";
	size_t row;

	*current = state(true);
	current->foreground = 6.0f;
	CHECK(yt_present_color(current, &result) == YT_PRESENT_OK);
	memset(pager, 0, sizeof(*pager));
	pager->foreground = 6;
	memset(capture, 0, sizeof(*capture));
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager_fixture_b05d(pager, current, free_holds,
	    sizeof(free_holds) - 1U, capture);
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager->newline_flag = 1.0f;
	pager_fixture_b05d(pager, current, prompt, sizeof(prompt) - 1U,
	    capture);
	yt_pager_editor_enter(pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo(&response, 1, &response, 1,
	    current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);

	if (help) {
		CHECK(yt_present_line(NULL, 0, current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(capture, &result);
		pager_fixture_b05d(pager, current, heading,
		    sizeof(heading) - 1U, capture);
		CHECK(yt_present_line(NULL, 0, current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(capture, &result);
		pager_fixture_b05d(pager, current,
		    (const uint8_t *)rows[0], strlen(rows[0]), capture);
		for (row = 1; row < YT_ARRAY_LEN(rows); ++row)
			pager_fixture_b05d(pager, current,
			    (const uint8_t *)rows[row], strlen(rows[row]), capture);
	}
	else {
		CHECK(yt_present_line(NULL, 0, current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(capture, &result);
		current->bold = 1.0f;
		current->blink = 1.0f;
		pager_fixture_b05d(pager, current, invalid,
		    sizeof(invalid) - 1U, capture);
	}

	pager->line_count = 0.0f;
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager_fixture_b05d(pager, current, free_holds,
	    sizeof(free_holds) - 1U, capture);
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	current->foreground = 6.0f;
	pager->foreground = 6;
	pager->newline_flag = 1.0f;
	pager_fixture_b05d(pager, current, prompt, sizeof(prompt) - 1U,
	    capture);
}

static void
test_planet_menu_front_presentation(void)
{
	static const uint8_t help[] =
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? ?\r\n"
	    "\r\n<Help>\n\r\r\n1 - Take Ore\n\r"
	    "2 - Take Organics\n\r3 - Take Equipment\n\r"
	    "4 - Take Fighters\n\r5 - Take Missiles\n\r6 - Take Mines\n\r"
	    "9 - Take Plasma Bolts\n\rA - Take <A>ll (Default)\n\r"
	    "B - Planet's <B>ank\n\rD - <D>isplay Planet\n\r"
	    "F - Take/Leave Ground <F>orces\n\rL - <L>eave Planet\n\r"
	    "N - Re-<N>ame Planet\n\rT - <T>ransfer Cargo to Planet\n\r"
	    "! - Use Planet Thrusters\n\r$ - Raise Productivity\n\r"
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? ";
	static const uint8_t invalid[] =
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? X\r\n"
	    "\r\n\x1b[0;36;40;5;1mInvalid command.\n\r"
	    "\x1b[0;36;40m\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? ";
	struct yt_present_state current;
	struct yt_pager_state pager;
	struct pager_capture capture;

	planet_menu_front_cycle_fixture(true, &capture, &current, &pager);
	CHECK(sizeof(help) - 1U == 524U);
	CHECK(capture.remote_length == sizeof(help) - 1U
	    && memcmp(capture.remote, help, sizeof(help) - 1U) == 0);
	CHECK(pager.line_count == 2.0f);
	planet_menu_front_cycle_fixture(false, &capture, &current, &pager);
	CHECK(sizeof(invalid) - 1U == 201U);
	CHECK(capture.remote_length == sizeof(invalid) - 1U
	    && memcmp(capture.remote, invalid, sizeof(invalid) - 1U) == 0);
	CHECK(pager.line_count == 2.0f);
}

static void
test_planet_take_all_default_cycle_presentation(void)
{
	static const uint8_t free_holds[] =
	    "You have 65 free cargo holds.";
	static const uint8_t prompt[] =
	    "Time: 14:59  Planet command (?=help) [A]? ";
	static const uint8_t title[] = "<Take all>";
	static const uint8_t taking[] = "Taking:";
	static const char *const rows[] = {
		"Fighters..... 404", "Missiles..... 5", "Mines........ 6",
		"Plasma bolts. 9", "Equipment.... 65", "Organics..... 0",
		"Ore.......... 0"
	};
	static const uint8_t expected[] =
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? \r\n"
	    "\r\n<Take all>\n\r\r\nTaking:\n\r"
	    "\r\nFighters..... 404\n\r"
	    "Missiles..... 5\n\rMines........ 6\n\r"
	    "Plasma bolts. 9\n\rEquipment.... 65\n\r"
	    "Organics..... 0\n\rOre.......... 0\n\r"
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? ";
	struct yt_present_state current = state(true);
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	char accumulator[80] = "";
	size_t row;

	current.foreground = 6.0f;
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 6;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current, free_holds,
	    sizeof(free_holds) - 1U, &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, prompt, sizeof(prompt) - 1U,
	    &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo(NULL, 0, NULL, 0, &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);

	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current, title, sizeof(title) - 1U,
	    &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current, taking, sizeof(taking) - 1U,
	    &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current, (const uint8_t *)rows[0],
	    strlen(rows[0]), &capture);
	for (row = 1; row < YT_ARRAY_LEN(rows); ++row)
		pager_fixture_b05d(&pager, &current,
		    (const uint8_t *)rows[row], strlen(rows[row]), &capture);

	pager.line_count = 0.0f;
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current, free_holds,
	    sizeof(free_holds) - 1U, &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, prompt, sizeof(prompt) - 1U,
	    &capture);
	CHECK(sizeof(expected) - 1U == 305U);
	CHECK(capture.remote_length == sizeof(expected) - 1U
	    && memcmp(capture.remote, expected, sizeof(expected) - 1U) == 0);
	CHECK(pager.line_count == 2.0f && pager.newline_flag == 0.0f);
}

static void
test_planet_take_one_presentation(void)
{
	static const uint8_t title[] = "<Take Ore>";
	static const uint8_t prompt[] = "How much [ 65 ]? ";
	static const uint8_t amount[] = "3";
	static const uint8_t stock_amount[] = "102";
	static const uint8_t stock[] = "They don't have that many.";
	static const uint8_t accepted[] =
	    "\r\n<Take Ore>\n\r\r\nHow much [ 65 ]? 3\r\n";
	static const uint8_t rejected[] =
	    "\r\n<Take Ore>\n\r\r\nHow much [ 65 ]? 102\r\n"
	    "\r\n\x1b[0;36;40;5;1mThey don't have that many.\n\r";
	struct yt_present_state current;
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	char accumulator[80] = "1";

	current = state(true);
	current.foreground = 6.0f;
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 6;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current, title, sizeof(title) - 1U,
	    &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, prompt, sizeof(prompt) - 1U,
	    &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo(amount, sizeof(amount) - 1U, amount,
	    sizeof(amount) - 1U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(sizeof(accepted) - 1U == 36U);
	CHECK(capture.remote_length == sizeof(accepted) - 1U
	    && memcmp(capture.remote, accepted, sizeof(accepted) - 1U) == 0);
	CHECK(pager.line_count == 0.0f);

	current = state(true);
	current.foreground = 6.0f;
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 6;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current, title, sizeof(title) - 1U,
	    &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, prompt, sizeof(prompt) - 1U,
	    &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo(stock_amount,
	    sizeof(stock_amount) - 1U, stock_amount,
	    sizeof(stock_amount) - 1U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.bold = 1.0f;
	current.blink = 1.0f;
	pager_fixture_b05d(&pager, &current, stock, sizeof(stock) - 1U,
	    &capture);
	CHECK(sizeof(rejected) - 1U == 82U);
	CHECK(capture.remote_length == sizeof(rejected) - 1U
	    && memcmp(capture.remote, rejected, sizeof(rejected) - 1U) == 0);
	CHECK(pager.line_count == 1.0f);
}

enum planet_transfer_fixture_outcome {
	PLANET_TRANSFER_SELECTOR_ONLY,
	PLANET_TRANSFER_NO_CARGO,
	PLANET_TRANSFER_CARGO,
	PLANET_TRANSFER_PLASMA,
	PLANET_TRANSFER_MISSILES,
	PLANET_TRANSFER_MINES,
	PLANET_TRANSFER_FIGHTER_CANCEL,
	PLANET_TRANSFER_FIGHTER_SUCCESS
};

static void
planet_transfer_0317(struct yt_pager_state *pager,
    struct yt_present_state *current, const uint8_t *text, size_t length,
    struct pager_capture *capture)
{
	struct yt_present_result result;

	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager_fixture_b05d(pager, current, text, length, capture);
}

static struct pager_capture
planet_transfer_fixture(const uint8_t *selector, size_t selector_length,
    const uint8_t *fighter, size_t fighter_length,
    enum planet_transfer_fixture_outcome outcome,
    struct yt_pager_state *pager_out)
{
	static const uint8_t title[] = "<Transfer items to planet>";
	static const uint8_t question[] = "Transfer which item?";
	static const uint8_t plasma_row[] = "[B] Plasma Bolts";
	static const uint8_t cargo_row[] = "[C] Cargo";
	static const uint8_t fighter_row[] = "[F] Fighters";
	static const uint8_t missile_row[] = "[S] Missiles";
	static const uint8_t mine_row[] = "[M] Mines";
	static const uint8_t selector_prompt[] = "-=>";
	static const uint8_t no_cargo[] = "You don't have any cargo!";
	static const uint8_t cargo_success[] = "Cargo transferred!!";
	static const uint8_t plasma_success[] = "Plasma Bolts Transferred!";
	static const uint8_t missile_success[] = "Missiles Transferred!";
	static const uint8_t mine_success[] = "Mines Transferred!";
	static const uint8_t fighter_prompt[] =
	    "You have 7 fighters. Transfer how many -=>";
	static const uint8_t fighter_success[] = "Fighters Transferred!";
	struct yt_present_state current = state(true);
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	char accumulator[80] = "T";

	current.foreground = 6.0f;
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 6;
	memset(&capture, 0, sizeof(capture));
	planet_transfer_0317(&pager, &current, title, sizeof(title) - 1U,
	    &capture);
	planet_transfer_0317(&pager, &current, question,
	    sizeof(question) - 1U, &capture);
	planet_transfer_0317(&pager, &current, plasma_row,
	    sizeof(plasma_row) - 1U, &capture);
	pager_fixture_b05d(&pager, &current, cargo_row,
	    sizeof(cargo_row) - 1U, &capture);
	pager_fixture_b05d(&pager, &current, fighter_row,
	    sizeof(fighter_row) - 1U, &capture);
	pager_fixture_b05d(&pager, &current, missile_row,
	    sizeof(missile_row) - 1U, &capture);
	pager_fixture_b05d(&pager, &current, mine_row,
	    sizeof(mine_row) - 1U, &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, selector_prompt,
	    sizeof(selector_prompt) - 1U, &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo(selector, selector_length, selector,
	    selector_length, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);

	if (outcome == PLANET_TRANSFER_SELECTOR_ONLY)
		goto done;
	if (outcome == PLANET_TRANSFER_NO_CARGO) {
		CHECK(yt_present_line(NULL, 0, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		current.bold = 1.0f;
		current.blink = 1.0f;
		pager_fixture_b05d(&pager, &current, no_cargo,
		    sizeof(no_cargo) - 1U, &capture);
		goto done;
	}
	if (outcome == PLANET_TRANSFER_CARGO) {
		CHECK(yt_present_line(NULL, 0, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		pager_fixture_b05d(&pager, &current, cargo_success,
		    sizeof(cargo_success) - 1U, &capture);
		goto done;
	}
	if (outcome == PLANET_TRANSFER_FIGHTER_CANCEL
	    || outcome == PLANET_TRANSFER_FIGHTER_SUCCESS) {
		CHECK(yt_present_line(NULL, 0, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		pager.newline_flag = 1.0f;
		pager_fixture_b05d(&pager, &current, fighter_prompt,
		    sizeof(fighter_prompt) - 1U, &capture);
		yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
		CHECK(yt_present_editor_echo(fighter, fighter_length, fighter,
		    fighter_length, &current, &result) == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		CHECK(yt_present_line(NULL, 0, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		if (outcome == PLANET_TRANSFER_FIGHTER_CANCEL)
			goto done;
	}
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.blink = 1.0f;
	if (outcome == PLANET_TRANSFER_PLASMA)
		pager_fixture_b05d(&pager, &current, plasma_success,
		    sizeof(plasma_success) - 1U, &capture);
	else if (outcome == PLANET_TRANSFER_MISSILES)
		pager_fixture_b05d(&pager, &current, missile_success,
		    sizeof(missile_success) - 1U, &capture);
	else if (outcome == PLANET_TRANSFER_MINES)
		pager_fixture_b05d(&pager, &current, mine_success,
		    sizeof(mine_success) - 1U, &capture);
	else
		pager_fixture_b05d(&pager, &current, fighter_success,
		    sizeof(fighter_success) - 1U, &capture);

done:
	*pager_out = pager;
	return capture;
}

static void
test_planet_transfer_presentation(void)
{
	static const uint8_t prefix[] =
	    "\r\n<Transfer items to planet>\n\r"
	    "\r\nTransfer which item?\n\r"
	    "\r\n[B] Plasma Bolts\n\r"
	    "[C] Cargo\n\r[F] Fighters\n\r[S] Missiles\n\r[M] Mines\n\r"
	    "\r\n-=>";
	static const uint8_t no_cargo_suffix[] =
	    "C\r\n\r\n\x1b[0;36;40;5;1mYou don't have any cargo!\n\r";
	static const uint8_t cargo_suffix[] =
	    "C\r\n\r\nCargo transferred!!\n\r";
	static const uint8_t plasma_suffix[] =
	    "B\r\n\r\n\x1b[0;36;40;5mPlasma Bolts Transferred!\n\r";
	static const uint8_t missile_suffix[] =
	    "S\r\n\r\n\x1b[0;36;40;5mMissiles Transferred!\n\r";
	static const uint8_t mine_suffix[] =
	    "M\r\n\r\n\x1b[0;36;40;5mMines Transferred!\n\r";
	static const uint8_t fighter_cancel_suffix[] =
	    "F\r\n\r\nYou have 7 fighters. Transfer how many -=>\r\n";
	static const uint8_t fighter_success_suffix[] =
	    "F\r\n\r\nYou have 7 fighters. Transfer how many -=>3\r\n"
	    "\r\n\x1b[0;36;40;5mFighters Transferred!\n\r";
	struct {
		const char *selector;
		const char *fighter;
		enum planet_transfer_fixture_outcome outcome;
		const uint8_t *suffix;
		size_t suffix_length;
		size_t expected_length;
	} cases[] = {
		{"", "", PLANET_TRANSFER_SELECTOR_ONLY,
		    (const uint8_t *)"\r\n", 2U, 131U},
		{"X", "", PLANET_TRANSFER_SELECTOR_ONLY,
		    (const uint8_t *)"X\r\n", 3U, 132U},
		{"SF", "", PLANET_TRANSFER_SELECTOR_ONLY,
		    (const uint8_t *)"SF\r\n", 4U, 133U},
		{"C", "", PLANET_TRANSFER_NO_CARGO, no_cargo_suffix,
		    sizeof(no_cargo_suffix) - 1U, 175U},
		{"C", "", PLANET_TRANSFER_CARGO, cargo_suffix,
		    sizeof(cargo_suffix) - 1U, 155U},
		{"B", "", PLANET_TRANSFER_PLASMA, plasma_suffix,
		    sizeof(plasma_suffix) - 1U, 173U},
		{"S", "", PLANET_TRANSFER_MISSILES, missile_suffix,
		    sizeof(missile_suffix) - 1U, 169U},
		{"M", "", PLANET_TRANSFER_MINES, mine_suffix,
		    sizeof(mine_suffix) - 1U, 166U},
		{"F", "", PLANET_TRANSFER_FIGHTER_CANCEL,
		    fighter_cancel_suffix, sizeof(fighter_cancel_suffix) - 1U, 178U},
		{"F", "3", PLANET_TRANSFER_FIGHTER_SUCCESS,
		    fighter_success_suffix, sizeof(fighter_success_suffix) - 1U, 216U}
	};
	size_t index;

	for (index = 0; index < YT_ARRAY_LEN(cases); ++index) {
		struct yt_pager_state pager;
		struct pager_capture capture = planet_transfer_fixture(
		    (const uint8_t *)cases[index].selector,
		    strlen(cases[index].selector),
		    (const uint8_t *)cases[index].fighter,
		    strlen(cases[index].fighter), cases[index].outcome, &pager);

		CHECK(capture.remote_length == cases[index].expected_length);
		CHECK(capture.remote_length == sizeof(prefix) - 1U
		    + cases[index].suffix_length);
		CHECK(memcmp(capture.remote, prefix, sizeof(prefix) - 1U) == 0);
		CHECK(memcmp(capture.remote + sizeof(prefix) - 1U,
		    cases[index].suffix, cases[index].suffix_length) == 0);
		if (cases[index].outcome == PLANET_TRANSFER_FIGHTER_CANCEL)
			CHECK(pager.line_count == 0.0f);
	}
}

enum planet_bank_fixture_outcome {
	PLANET_BANK_CANCEL,
	PLANET_BANK_SAVINGS_ERROR,
	PLANET_BANK_CREDIT_ERROR,
	PLANET_BANK_ACCEPTED,
	PLANET_BANK_ZERO
};

static struct pager_capture
planet_bank_fixture(const uint8_t *response, size_t response_length,
    enum planet_bank_fixture_outcome outcome, struct yt_pager_state *pager_out)
{
	static const uint8_t title[] =
	    "Welcome to the intergalactic bank of New Terra!";
	static const uint8_t prompt[] =
	    "How many credits do you want in the account? 13345 Available ->";
	static const uint8_t savings[] =
	    "We are a SAVINGS not a LOAN institution!";
	static const uint8_t insufficient[] =
	    "You don't have that many Credits!";
	static const uint8_t accepted[] =
	    "You have 1500 credits on deposit at 1% interest. Have a nice day!";
	static const uint8_t zero[] = "Have a nice day!";
	struct yt_present_state current = state(true);
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	char accumulator[80] = "B";

	current.foreground = 6.0f;
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 6;
	memset(&capture, 0, sizeof(capture));
	planet_transfer_0317(&pager, &current, title, sizeof(title) - 1U,
	    &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, prompt, sizeof(prompt) - 1U,
	    &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo(response, response_length, response,
	    response_length, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	if (outcome == PLANET_BANK_CANCEL)
		goto done;
	if (outcome == PLANET_BANK_SAVINGS_ERROR
	    || outcome == PLANET_BANK_CREDIT_ERROR) {
		const uint8_t *error_text = outcome == PLANET_BANK_SAVINGS_ERROR
		    ? savings : insufficient;
		size_t error_length = outcome == PLANET_BANK_SAVINGS_ERROR
		    ? sizeof(savings) - 1U : sizeof(insufficient) - 1U;

		CHECK(yt_present_line(NULL, 0, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		current.bold = 1.0f;
		current.blink = 1.0f;
		pager_fixture_b05d(&pager, &current, error_text, error_length,
		    &capture);
		goto done;
	}
	planet_transfer_0317(&pager, &current,
	    outcome == PLANET_BANK_ZERO ? zero : accepted,
	    outcome == PLANET_BANK_ZERO ? sizeof(zero) - 1U
	    : sizeof(accepted) - 1U, &capture);
	CHECK(yt_present_sound(4.0f, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);

done:
	*pager_out = pager;
	return capture;
}

static void
test_planet_bank_presentation(void)
{
	static const uint8_t accepted[] =
	    "\r\nWelcome to the intergalactic bank of New Terra!\n\r"
	    "\r\nHow many credits do you want in the account? 13345 Available ->"
	    "1500\r\n"
	    "\r\nYou have 1500 credits on deposit at 1% interest. "
	    "Have a nice day!\n\r"
	    "\x1b[MBT128O5L48P64CP64C\x0e";
	static const uint8_t zero[] =
	    "\r\nWelcome to the intergalactic bank of New Terra!\n\r"
	    "\r\nHow many credits do you want in the account? 13345 Available ->"
	    "0\r\n\r\nHave a nice day!\n\r"
	    "\x1b[MBT128O5L48P64CP64C\x0e";
	static const uint8_t cancel[] =
	    "\r\nWelcome to the intergalactic bank of New Terra!\n\r"
	    "\r\nHow many credits do you want in the account? 13345 Available ->"
	    "\r\n";
	static const uint8_t savings[] =
	    "\r\nWelcome to the intergalactic bank of New Terra!\n\r"
	    "\r\nHow many credits do you want in the account? 13345 Available ->"
	    "-1\r\n\r\n\x1b[0;36;40;5;1m"
	    "We are a SAVINGS not a LOAN institution!\n\r";
	static const uint8_t insufficient[] =
	    "\r\nWelcome to the intergalactic bank of New Terra!\n\r"
	    "\r\nHow many credits do you want in the account? 13345 Available ->"
	    "13346\r\n\r\n\x1b[0;36;40;5;1m"
	    "You don't have that many Credits!\n\r";
	struct yt_pager_state pager;
	struct pager_capture capture;

	capture = planet_bank_fixture((const uint8_t *)"1500", 4U,
	    PLANET_BANK_ACCEPTED, &pager);
	CHECK(sizeof(accepted) - 1U == 213U);
	CHECK(capture.remote_length == sizeof(accepted) - 1U
	    && memcmp(capture.remote, accepted, sizeof(accepted) - 1U) == 0);
	CHECK(pager.line_count == 1.0f);
	capture = planet_bank_fixture((const uint8_t *)"0", 1U,
	    PLANET_BANK_ZERO, &pager);
	CHECK(sizeof(zero) - 1U == 161U);
	CHECK(capture.remote_length == sizeof(zero) - 1U
	    && memcmp(capture.remote, zero, sizeof(zero) - 1U) == 0);
	capture = planet_bank_fixture(NULL, 0, PLANET_BANK_CANCEL, &pager);
	CHECK(capture.remote_length == sizeof(cancel) - 1U
	    && memcmp(capture.remote, cancel, sizeof(cancel) - 1U) == 0);
	CHECK(pager.line_count == 0.0f);
	capture = planet_bank_fixture((const uint8_t *)"-1", 2U,
	    PLANET_BANK_SAVINGS_ERROR, &pager);
	CHECK(capture.remote_length == sizeof(savings) - 1U
	    && memcmp(capture.remote, savings, sizeof(savings) - 1U) == 0);
	capture = planet_bank_fixture((const uint8_t *)"13346", 5U,
	    PLANET_BANK_CREDIT_ERROR, &pager);
	CHECK(capture.remote_length == sizeof(insufficient) - 1U
	    && memcmp(capture.remote, insufficient,
	    sizeof(insufficient) - 1U) == 0);
}

static struct pager_capture
planet_productivity_fixture(const uint8_t *response, size_t response_length,
    const uint8_t *units, size_t units_length, const float delta[4],
    struct yt_pager_state *pager_out)
{
	static const uint8_t explanation[] =
	    "Productivity is increased by 1 Unit of EQU, ORG  & ORE "
	    "for each 250 credits.";
	static const uint8_t credits[] = "You have 12345 Credits.";
	static const uint8_t prompt[] =
	    "Spend how much to raise productivity? -+> ";
	static const char *const descriptors[4] = {
		"Also increased: Fighters:", ", Missiles:",
		", Mines:", ", Plasma Bolts:"
	};
	struct yt_present_state current = state(true);
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	char accumulator[80] = "$";
	uint8_t success[160];
	size_t success_length = 0;
	size_t index;

	current.foreground = 6.0f;
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 6;
	memset(&capture, 0, sizeof(capture));
	planet_transfer_0317(&pager, &current, explanation,
	    sizeof(explanation) - 1U, &capture);
	planet_transfer_0317(&pager, &current, credits,
	    sizeof(credits) - 1U, &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, prompt, sizeof(prompt) - 1U,
	    &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo(response, response_length, response,
	    response_length, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	memcpy(success + success_length, "Productivity increased by",
	    strlen("Productivity increased by"));
	success_length += strlen("Productivity increased by");
	memcpy(success + success_length, units, units_length);
	success_length += units_length;
	memcpy(success + success_length, " units of ORE, ORG & EQU!",
	    strlen(" units of ORE, ORG & EQU!"));
	success_length += strlen(" units of ORE, ORG & EQU!");
	planet_transfer_0317(&pager, &current, success, success_length, &capture);
	for (index = 0; index < 4U; ++index) {
		uint8_t fragment[96];
		size_t fragment_length;

		if (delta[index] == 0.0f)
			continue;
		fragment_length = strlen(descriptors[index]);
		memcpy(fragment, descriptors[index], fragment_length);
		memcpy(fragment + fragment_length, " 1", 2U);
		if (index == 0)
			fragment[fragment_length + 1U] = '3';
		fragment_length += 2U;
		pager.newline_flag = 1.0f;
		pager_fixture_b05d(&pager, &current, fragment, fragment_length,
		    &capture);
	}
	if (delta[0] != 0.0f) {
		CHECK(yt_present_line(NULL, 0, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
	}
	*pager_out = pager;
	return capture;
}

static void
test_planet_productivity_presentation(void)
{
	static const uint8_t canonical[] =
	    "\r\nProductivity is increased by 1 Unit of EQU, ORG  & ORE "
	    "for each 250 credits.\n\r"
	    "\r\nYou have 12345 Credits.\n\r"
	    "\r\nSpend how much to raise productivity? -+> 250\r\n"
	    "\r\nProductivity increased by 1 units of ORE, ORG & EQU!\n\r"
	    "Also increased: Fighters: 3\r\n";
	static const uint8_t all_fragments[] =
	    "\r\nProductivity is increased by 1 Unit of EQU, ORG  & ORE "
	    "for each 250 credits.\n\r"
	    "\r\nYou have 12345 Credits.\n\r"
	    "\r\nSpend how much to raise productivity? -+> 250\r\n"
	    "\r\nProductivity increased by 1 units of ORE, ORG & EQU!\n\r"
	    "Also increased: Fighters: 3, Missiles: 1, Mines: 1, "
	    "Plasma Bolts: 1\r\n";
	static const uint8_t one_credit[] =
	    "\r\nProductivity is increased by 1 Unit of EQU, ORG  & ORE "
	    "for each 250 credits.\n\r"
	    "\r\nYou have 12345 Credits.\n\r"
	    "\r\nSpend how much to raise productivity? -+> 1\r\n"
	    "\r\nProductivity increased by .004 units of ORE, ORG & EQU!\n\r";
	static const uint8_t later_suffix[] =
	    ", Missiles: 1, Mines: 1, Plasma Bolts: 1";
	const float fighter_only[4] = {3.0f, 0.0f, 0.0f, 0.0f};
	const float all[4] = {3.0f, 1.0f, 1.0f, 1.0f};
	const float none[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	const float later[4] = {0.0f, 1.0f, 1.0f, 1.0f};
	struct yt_pager_state pager;
	struct pager_capture capture;

	capture = planet_productivity_fixture((const uint8_t *)"250", 3U,
	    (const uint8_t *)" 1", 2U, fighter_only, &pager);
	CHECK(sizeof(canonical) - 1U == 241U);
	CHECK(capture.remote_length == sizeof(canonical) - 1U
	    && memcmp(capture.remote, canonical, sizeof(canonical) - 1U) == 0);
	CHECK(pager.line_count == 2.0f);
	capture = planet_productivity_fixture((const uint8_t *)"250", 3U,
	    (const uint8_t *)" 1", 2U, all, &pager);
	CHECK(sizeof(all_fragments) - 1U == 281U);
	CHECK(capture.remote_length == sizeof(all_fragments) - 1U
	    && memcmp(capture.remote, all_fragments,
	    sizeof(all_fragments) - 1U) == 0);
	capture = planet_productivity_fixture((const uint8_t *)"1", 1U,
	    (const uint8_t *)" .004", 5U, none, &pager);
	CHECK(sizeof(one_credit) - 1U == 213U);
	CHECK(capture.remote_length == sizeof(one_credit) - 1U
	    && memcmp(capture.remote, one_credit,
	    sizeof(one_credit) - 1U) == 0);
	capture = planet_productivity_fixture((const uint8_t *)"250", 3U,
	    (const uint8_t *)" 1", 2U, later, &pager);
	CHECK(capture.remote_length >= sizeof(later_suffix) - 1U);
	CHECK(memcmp(capture.remote + capture.remote_length
	    - (sizeof(later_suffix) - 1U), later_suffix,
	    sizeof(later_suffix) - 1U) == 0);
	CHECK(capture.remote[capture.remote_length - 1U] == '1'
	    && pager.line_count == 4.0f);
}

static void
test_clearance_presentation(void)
{
	static const uint8_t holds[] =
	    "Special clearance sale! The Trader's Guild is selling Holds "
	    "for 10% off!";
	static const uint8_t fighters[] =
	    "Special clearance sale! The Trader's Guild is selling Fighters "
	    "for 50% off!";
	static const uint8_t plain[] =
	    "\r\nSpecial clearance sale! The Trader's Guild is selling Holds "
	    "for 10% off!\r\n"
	    "Special clearance sale! The Trader's Guild is selling Fighters "
	    "for 50% off!\r\n\a\r\n";
	static const uint8_t ansi[] =
	    "\r\nSpecial clearance sale! The Trader's Guild is selling Holds "
	    "for 10% off!\r\n"
	    "Special clearance sale! The Trader's Guild is selling Fighters "
	    "for 50% off!\r\n"
	    "\x1b[MBO4L32P32CP64CP64CP64L16EP64L32CP64L12E\x0e\r\n";
	struct yt_present_state current;
	struct yt_present_result result;
	struct pager_capture capture;
	bool use_ansi;
	int pass;

	for (pass = 0; pass < 2; ++pass) {
		use_ansi = pass != 0;
		current = state(use_ansi);
		current.foreground = 3.0f;
		if (use_ansi)
			CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
		memset(&capture, 0, sizeof(capture));
		CHECK(yt_present_line(NULL, 0, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		CHECK(yt_present_line(holds, sizeof(holds) - 1U, &current,
		    &result) == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		CHECK(yt_present_line(fighters, sizeof(fighters) - 1U, &current,
		    &result) == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		CHECK(yt_present_sound(1.0f, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		CHECK(yt_present_line(NULL, 0, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		if (use_ansi)
			CHECK(capture.remote_length == sizeof(ansi) - 1U
			    && memcmp(capture.remote, ansi, sizeof(ansi) - 1U) == 0);
		else
			CHECK(capture.remote_length == sizeof(plain) - 1U
			    && memcmp(capture.remote, plain,
			    sizeof(plain) - 1U) == 0);
	}
}

static void
earth_report_fixed(struct pager_capture *capture,
    struct yt_present_state *current, const char *text, float width)
{
	struct yt_present_result result;
	uint8_t field[80];
	size_t length = strlen(text);

	CHECK(length <= sizeof(field));
	if (length > sizeof(field))
		return;
	memcpy(field, text, length);
	CHECK(yt_present_fixed_width(field, &length, sizeof(field), width,
	    current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
}

static void
earth_report_row_fixture(struct pager_capture *capture,
    struct yt_pager_state *pager, struct yt_present_state *current,
    const char *label, const char *cost, const char *affordable)
{
	earth_report_fixed(capture, current, label, 22.0f);
	earth_report_fixed(capture, current, cost, 9.0f);
	pager_fixture_b05d(pager, current, (const uint8_t *)affordable,
	    strlen(affordable), capture);
}

static void
earth_report_fixture(struct pager_capture *capture,
    struct yt_pager_state *pager, struct yt_present_state *current)
{
	static const char *const label[9] = {
		"[1] Cloak Energy", "[2] Cargo Holds", "[3] Fighters",
		"[4] Play Lottery", "[5] Danger Scanner",
		"[6] Anti-Cloak Device", "[7] Ground Forces",
		"[8] Shield Power", "[9] Hire Spies (Each)"
	};
	static const char *const cost[9] = {
		"* 1000 ", "* 250 ", "* 50 ", "* 5", "* 500000 ",
		"* 1E+09 ", "* 200 ", "* 50 ", "* 1E+09 "
	};
	static const char *const affordable[9] = {
		"* 12", "* 49", "* 246", "* 2469", "* 0", "* 0",
		"* 61", "* 246", "* 0"
	};
	static const uint8_t title[] =
	    "Commerce report for Earth: 07-25-2026 12:34:56";
	static const uint8_t separator[] =
	    "----------------------*--------*------------";
	static const uint8_t header[] =
	    "         ITEM         *  COST  * CAN AFFORD";
	struct yt_present_result result;
	size_t index;

	pager->line_count = 0.0f;
	current->foreground = 3.0f;
	pager->foreground = 3;
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager_fixture_b05d(pager, current, title, sizeof(title) - 1U,
	    capture);
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager_fixture_b05d(pager, current, separator,
	    sizeof(separator) - 1U, capture);
	pager_fixture_b05d(pager, current, header, sizeof(header) - 1U,
	    capture);
	pager_fixture_b05d(pager, current, separator,
	    sizeof(separator) - 1U, capture);
	for (index = 0U; index < 9U; ++index)
		earth_report_row_fixture(capture, pager, current, label[index],
		    cost[index], affordable[index]);
	pager_fixture_b05d(pager, current, separator,
	    sizeof(separator) - 1U, capture);
}

static void
test_earth_report_presentation(void)
{
	static const uint8_t canonical[] =
	    "\r\nCommerce report for Earth: 07-25-2026 12:34:56\n\r"
	    "\r\n"
	    "----------------------*--------*------------\n\r"
	    "         ITEM         *  COST  * CAN AFFORD\n\r"
	    "----------------------*--------*------------\n\r"
	    "[1] Cloak Energy      * 1000   * 12\n\r"
	    "[2] Cargo Holds       * 250    * 49\n\r"
	    "[3] Fighters          * 50     * 246\n\r"
	    "[4] Play Lottery      * 5      * 2469\n\r"
	    "[5] Danger Scanner    * 500000 * 0\n\r"
	    "[6] Anti-Cloak Device * 1E+09  * 0\n\r"
	    "[7] Ground Forces     * 200    * 61\n\r"
	    "[8] Shield Power      * 50     * 246\n\r"
	    "[9] Hire Spies (Each) * 1E+09  * 0\n\r"
	    "----------------------*--------*------------\n\r"
	    "\r\n[I] Ship Info -=*=- [0] Leave Port\n\r"
	    "\r\nCredits: 12345 -=*=- Buy Which Item? -=>0\r\n";
	static const uint8_t menu[] =
	    "[I] Ship Info -=*=- [0] Leave Port";
	static const uint8_t prompt[] =
	    "Credits: 12345 -=*=- Buy Which Item? -=>";
	struct yt_present_state current = state(false);
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;

	memset(&pager, 0, sizeof(pager));
	memset(&capture, 0, sizeof(capture));
	pager.foreground = 3;
	current.foreground = 3.0f;
	earth_report_fixture(&capture, &pager, &current);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current, menu, sizeof(menu) - 1U,
	    &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, prompt, sizeof(prompt) - 1U,
	    &capture);
	CHECK(yt_present_line((const uint8_t *)"0", 1U, &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(capture.remote_length == sizeof(canonical) - 1U
	    && memcmp(capture.remote, canonical, sizeof(canonical) - 1U) == 0);
	CHECK(pager.line_count == 16.0f);
}

static void
earth_purchase_prompt(struct pager_capture *capture,
    struct yt_pager_state *pager, struct yt_present_state *current,
    const char *prompt, const char *response)
{
	struct yt_present_result result;

	pager->newline_flag = 1.0f;
	pager_fixture_b05d(pager, current, (const uint8_t *)prompt,
	    strlen(prompt), capture);
	CHECK(yt_present_line((const uint8_t *)response, strlen(response),
	    current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
}

static void
test_earth_purchase_presentation(void)
{
	static const uint8_t holds[] =
	    "\r\n\r\nYou need 80 holds.\n\r"
	    "Buy how many holds? [0]? 1\r\n";
	static const uint8_t fighters[] =
	    "\r\nBuy how many fighters? [0]? 1\r\n";
	static const uint8_t cloak[] =
	    "\r\nCloak energy is down by 25%.\n\r"
	    "Buy how many points of Cloak Energy? "
	    "(0 - 25) [ 25 ] ?1\r\n";
	static const uint8_t scanner[] =
	    "\r\nDanger Scanner installed in your ship!\n\r";
	static const uint8_t ground[] =
	    "\r\nBuy how many ground force units? [0]? 1\r\n";
	static const uint8_t shields[] =
	    "\r\nBuy how much shield power? [0]? 1\r\n";
	struct yt_present_state current;
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;

	current = state(false);
	memset(&pager, 0, sizeof(pager));
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current,
	    (const uint8_t *)"You need 80 holds.",
	    strlen("You need 80 holds."), &capture);
	earth_purchase_prompt(&capture, &pager, &current,
	    "Buy how many holds? [0]? ", "1");
	CHECK(capture.remote_length == sizeof(holds) - 1U
	    && memcmp(capture.remote, holds, sizeof(holds) - 1U) == 0);
	CHECK(pager.line_count == 2.0f);

	current = state(false);
	memset(&pager, 0, sizeof(pager));
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	earth_purchase_prompt(&capture, &pager, &current,
	    "Buy how many fighters? [0]? ", "1");
	CHECK(capture.remote_length == sizeof(fighters) - 1U
	    && memcmp(capture.remote, fighters, sizeof(fighters) - 1U) == 0);

	current = state(false);
	memset(&pager, 0, sizeof(pager));
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current,
	    (const uint8_t *)"Cloak energy is down by 25%.",
	    strlen("Cloak energy is down by 25%."), &capture);
	earth_purchase_prompt(&capture, &pager, &current,
	    "Buy how many points of Cloak Energy? (0 - 25) [ 25 ] ?", "1");
	CHECK(capture.remote_length == sizeof(cloak) - 1U
	    && memcmp(capture.remote, cloak, sizeof(cloak) - 1U) == 0);

	current = state(false);
	memset(&pager, 0, sizeof(pager));
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.bold = 1.0f;
	pager_fixture_b05d(&pager, &current,
	    (const uint8_t *)"Danger Scanner installed in your ship!",
	    strlen("Danger Scanner installed in your ship!"), &capture);
	CHECK(capture.remote_length == sizeof(scanner) - 1U
	    && memcmp(capture.remote, scanner, sizeof(scanner) - 1U) == 0);

	current = state(false);
	memset(&pager, 0, sizeof(pager));
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	earth_purchase_prompt(&capture, &pager, &current,
	    "Buy how many ground force units? [0]? ", "1");
	CHECK(capture.remote_length == sizeof(ground) - 1U
	    && memcmp(capture.remote, ground, sizeof(ground) - 1U) == 0);

	current = state(false);
	memset(&pager, 0, sizeof(pager));
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	earth_purchase_prompt(&capture, &pager, &current,
	    "Buy how much shield power? [0]? ", "1");
	CHECK(capture.remote_length == sizeof(shields) - 1U
	    && memcmp(capture.remote, shields, sizeof(shields) - 1U) == 0);
}

static void
test_earth_anti_cloak_presentation(void)
{
	static const uint8_t activation[] =
	    "ti-Cloaking device activated!\xd4" "D";
	static const uint8_t waves[] =
	    "Waves of electromagnetic disruption flood the galaxy..."
	    "\xd4\x0e\x00\x86\xc1" " is uncl";
	static const uint8_t expected[] =
	    "\r\nAnti-Cloaking Device works for this logon only. "
	    "Buy one? [y/N]y\r\n"
	    "\r\nti-Cloaking device activated!\xd4" "D\r\n\r\n"
	    "Waves of electromagnetic disruption flood the galaxy..."
	    "\xd4\x0e\x00\x86\xc1" " is uncl\r\n\r\n"
	    "ALPHA is uncloaked!\r\n"
	    "\r\n...the effect fades.\r\n"
	    "\r\nHit [Enter]\r\n";
	struct yt_present_state current = state(false);
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;

	current.sound.user_sound = 0.0f;
	memset(&pager, 0, sizeof(pager));
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_character(
	    (const uint8_t *)"Anti-Cloaking Device works for this logon only. "
	    "Buy one? [y/N]", strlen("Anti-Cloaking Device works for this "
	    "logon only. Buy one? [y/N]"), &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line((const uint8_t *)"y", 1U, &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(activation, sizeof(activation) - 1U, &current,
	    &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.foreground = 2.0f;
	CHECK(yt_present_bold_line(waves, sizeof(waves) - 1U, &current,
	    &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_sound(2.0f, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.foreground = 6.0f;
	CHECK(yt_present_bold_line((const uint8_t *)"ALPHA is uncloaked!",
	    strlen("ALPHA is uncloaked!"), &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_sound(1.0f, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.foreground = 2.0f;
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_bold_line((const uint8_t *)"...the effect fades.",
	    strlen("...the effect fades."), &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_sound(5.0f, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.foreground = 3.0f;
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	earth_purchase_prompt(&capture, &pager, &current, "Hit [Enter]", "");
	CHECK(capture.remote_length == sizeof(expected) - 1U
	    && memcmp(capture.remote, expected, sizeof(expected) - 1U) == 0);
}

static void
test_earth_spy_purchase_presentation(void)
{
	static const uint8_t expected[] =
	    "\r\nHire how many spies? [0]? 1\r\n"
	    "\r\nStart spy # 1 in what sector?-1\r\n"
	    "\r\nSpy # 1 will hunt in sector-1.\n\r"
	    "\r\n*[ Press any Key ]*\r                   \r";
	struct yt_present_state current = state(false);
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	float saved;

	memset(&pager, 0, sizeof(pager));
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	earth_purchase_prompt(&capture, &pager, &current,
	    "Hire how many spies? [0]? ", "1");
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	earth_purchase_prompt(&capture, &pager, &current,
	    "Start spy # 1 in what sector?", "-1");
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.bold = 1.0f;
	pager_fixture_b05d(&pager, &current,
	    (const uint8_t *)"Spy # 1 will hunt in sector-1.",
	    strlen("Spy # 1 will hunt in sector-1."), &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_press_prompt(&current, &result, &saved)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_press_cleanup(saved, &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(capture.remote_length == sizeof(expected) - 1U
	    && memcmp(capture.remote, expected, sizeof(expected) - 1U) == 0);
}

static void
test_earth_lottery_loss_presentation(void)
{
	static const uint8_t opening[] =
	    "\r\nYou may play 3 times daily.\r\n"
	    "You've played 1 times already.\r\n"
	    "\r\nWelcome to the Intergalactic Pick-6 Lottery!\n\r"
	    "\r\nEnter a 6 digit number for the lottery computer -+>999999\r\n"
	    "\r\nThe Galactic Lottery Computer picked: ";
	static const uint8_t ending[] =
	    "\r\n\r\nSorry, you didn't win this time.\r\n";
	struct yt_present_state current = state(false);
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	uint8_t expected[512];
	size_t expected_length = 0;
	int actual;
	int dummy;

	current.sound.user_sound = 0.0f;
	memset(&pager, 0, sizeof(pager));
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line((const uint8_t *)"You may play 3 times daily.",
	    strlen("You may play 3 times daily."), &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line((const uint8_t *)
	    "You've played 1 times already.",
	    strlen("You've played 1 times already."), &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.foreground = 1.0f;
	current.bold = 1.0f;
	pager_fixture_b05d(&pager, &current,
	    (const uint8_t *)"Welcome to the Intergalactic Pick-6 Lottery!",
	    strlen("Welcome to the Intergalactic Pick-6 Lottery!"), &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.foreground = 2.0f;
	current.bold = 1.0f;
	earth_purchase_prompt(&capture, &pager, &current,
	    "Enter a 6 digit number for the lottery computer -+>", "999999");
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_character((const uint8_t *)
	    "The Galactic Lottery Computer picked: ",
	    strlen("The Galactic Lottery Computer picked: "),
	    &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	for (actual = 0; actual < 6; ++actual) {
		uint8_t actual_digit = (uint8_t)('0' + actual);

		current.foreground = 6.0f;
		for (dummy = 0; dummy < 18; ++dummy) {
			CHECK(yt_present_character((const uint8_t *)"9", 1U,
			    &current, &result) == YT_PRESENT_OK);
			pager_capture_result(&capture, &result);
			CHECK(yt_present_lottery_rewind(1, 39 + actual,
			    &current, &result) == YT_PRESENT_OK);
			CHECK(result.event_count == 2
			    && result.events[1].operation
			    == YT_PRESENT_LOCAL_LOCATE
			    && result.events[1].column == 39 + actual);
			pager_capture_result(&capture, &result);
		}
		current.foreground = 7.0f;
		CHECK(yt_present_character(&actual_digit, 1U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
	}
	current.foreground = 2.0f;
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(
	    (const uint8_t *)"Sorry, you didn't win this time.",
	    strlen("Sorry, you didn't win this time."), &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	memcpy(expected + expected_length, opening, sizeof(opening) - 1U);
	expected_length += sizeof(opening) - 1U;
	for (actual = 0; actual < 6; ++actual) {
		for (dummy = 0; dummy < 18; ++dummy) {
			expected[expected_length++] = '9';
			expected[expected_length++] = '\b';
		}
		expected[expected_length++] = (uint8_t)('0' + actual);
	}
	memcpy(expected + expected_length, ending, sizeof(ending) - 1U);
	expected_length += sizeof(ending) - 1U;
	CHECK(expected_length == 472U && capture.remote_length == expected_length
	    && memcmp(capture.remote, expected, expected_length) == 0);
	current = state(false);
	current.sound.mode = 2.0f;
	CHECK(yt_present_lottery_rewind(4, 17, &current, &result)
	    == YT_PRESENT_OK);
	CHECK(result.remote_length == 0 && result.event_count == 1
	    && result.events[0].operation == YT_PRESENT_LOCAL_LOCATE
	    && result.events[0].row == 4 && result.events[0].column == 17);
}

static void
planet_computer_entry_cycle_fixture(bool ansi,
    struct pager_capture *capture, struct yt_present_state *current,
    struct yt_pager_state *pager)
{
	static const uint8_t free_holds[] =
	    "You have 65 free cargo holds.";
	static const uint8_t planet_prompt[] =
	    "Time: 14:59  Planet command (?=help) [A]? ";
	static const uint8_t activated[] = "<Computer activated>";
	static const uint8_t computer_prompt[] =
	    "Time: 14:59  Computer command (?=help)? ";
	static const uint8_t command[] = "C";
	struct yt_present_result result;
	char accumulator[80] = "";

	*current = state(ansi);
	current->foreground = 6.0f;
	if (ansi)
		CHECK(yt_present_color(current, &result) == YT_PRESENT_OK);
	memset(pager, 0, sizeof(*pager));
	pager->foreground = 6;
	memset(capture, 0, sizeof(*capture));
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager_fixture_b05d(pager, current, free_holds,
	    sizeof(free_holds) - 1U, capture);
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager->newline_flag = 1.0f;
	pager_fixture_b05d(pager, current, planet_prompt,
	    sizeof(planet_prompt) - 1U, capture);
	yt_pager_editor_enter(pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo(command, sizeof(command) - 1U,
	    command, sizeof(command) - 1U, current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);

	current->foreground = 1.0f;
	pager->foreground = 1;
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager_fixture_b05d(pager, current, activated,
	    sizeof(activated) - 1U, capture);
	CHECK(yt_present_sound(4.0f, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager->newline_flag = 1.0f;
	pager_fixture_b05d(pager, current, computer_prompt,
	    sizeof(computer_prompt) - 1U, capture);
}

static void
test_planet_computer_entry_cycle_presentation(void)
{
	static const uint8_t ansi[] =
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? C\r\n"
	    "\x1b[0;31;40m\r\n<Computer activated>\n\r"
	    "\x1b[MBT128O5L48P64CP64C\x0e"
	    "\r\nTime: 14:59  Computer command (?=help)? ";
	static const uint8_t plain[] =
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? C\r\n"
	    "\r\n<Computer activated>\n\r"
	    "\r\nTime: 14:59  Computer command (?=help)? ";
	struct yt_present_state current;
	struct yt_pager_state pager;
	struct pager_capture capture;

	planet_computer_entry_cycle_fixture(true, &capture, &current, &pager);
	CHECK(sizeof(ansi) - 1U == 178U);
	CHECK(capture.remote_length == sizeof(ansi) - 1U
	    && memcmp(capture.remote, ansi, sizeof(ansi) - 1U) == 0);
	CHECK(pager.line_count == 2.0f && pager.newline_flag == 0.0f);
	planet_computer_entry_cycle_fixture(false, &capture, &current, &pager);
	CHECK(sizeof(plain) - 1U == 146U);
	CHECK(capture.remote_length == sizeof(plain) - 1U
	    && memcmp(capture.remote, plain, sizeof(plain) - 1U) == 0);
	CHECK(pager.line_count == 2.0f && pager.newline_flag == 0.0f);
}

static void
planet_display_cycle_fixture(bool ansi,
    struct pager_capture *capture, struct yt_present_state *current,
    struct yt_pager_state *pager)
{
	static const uint8_t free_holds[] =
	    "You have 65 free cargo holds.";
	static const uint8_t prompt[] =
	    "Time: 14:59  Planet command (?=help) [A]? ";
	static const uint8_t title[] = "Planet: New Terra";
	static const uint8_t header[] =
	    " Item           Production     Amount    In Holds";
	static const uint8_t rule[] =
	    "=============  ============   ========  ==========";
	static const char *const labels[9] = {
		"Ore..........", "Organics.....", "Equipment....",
		"Fighters.....", "Missiles.....", "Mines........",
		"Credits......", "Forces.......", "Plasma bolts."
	};
	static const char *const production[9] = {
		" 11", " 22", " 33", " 66", " 2", " 1", " 9", " 2", " 3"
	};
	static const char *const amount[9] = {
		" 101", " 202", " 303", " 404", " 5", " 6", " 1000", " 250", " 9"
	};
	static const char *const held[9] = {
		" 10", " 20", " 5", " 7", " 2", " 3", " 12345", " 8", " 4"
	};
	static const uint8_t command[] = "D";
	struct yt_present_result result;
	char accumulator[80] = "";
	int index;

	*current = state(ansi);
	current->foreground = 6.0f;
	if (ansi)
		CHECK(yt_present_color(current, &result) == YT_PRESENT_OK);
	memset(pager, 0, sizeof(*pager));
	pager->foreground = 6;
	memset(capture, 0, sizeof(*capture));
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager_fixture_b05d(pager, current, free_holds,
	    sizeof(free_holds) - 1U, capture);
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager->newline_flag = 1.0f;
	pager_fixture_b05d(pager, current, prompt, sizeof(prompt) - 1U,
	    capture);
	yt_pager_editor_enter(pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo(command, sizeof(command) - 1U,
	    command, sizeof(command) - 1U, current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);

	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager_fixture_b05d(pager, current, title, sizeof(title) - 1U,
	    capture);
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager_fixture_b05d(pager, current, header, sizeof(header) - 1U,
	    capture);
	pager_fixture_b05d(pager, current, rule, sizeof(rule) - 1U,
	    capture);
	for (index = 0; index < 9; ++index) {
		CHECK(yt_present_character((const uint8_t *)labels[index],
		    strlen(labels[index]), current, &result) == YT_PRESENT_OK);
		pager_capture_result(capture, &result);
		CHECK(yt_present_right_aligned(
		    (const uint8_t *)production[index],
		    strlen(production[index]), 13.0f, current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(capture, &result);
		CHECK(yt_present_right_aligned((const uint8_t *)amount[index],
		    strlen(amount[index]), 11.0f, current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(capture, &result);
		CHECK(yt_present_right_aligned((const uint8_t *)held[index],
		    strlen(held[index]), 12.0f, current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(capture, &result);
		CHECK(yt_present_line(NULL, 0, current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(capture, &result);
	}

	pager->line_count = 0.0f;
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager_fixture_b05d(pager, current, free_holds,
	    sizeof(free_holds) - 1U, capture);
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	current->foreground = 6.0f;
	pager->foreground = 6;
	pager->newline_flag = 1.0f;
	pager_fixture_b05d(pager, current, prompt, sizeof(prompt) - 1U,
	    capture);
}

static void
test_planet_display_cycle_presentation(void)
{
	static const uint8_t expected[] =
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? D\r\n"
	    "\r\nPlanet: New Terra\n\r"
	    "\r\n Item           Production     Amount    In Holds\n\r"
	    "=============  ============   ========  ==========\n\r"
	    "Ore..........           11        101          10\r\n"
	    "Organics.....           22        202          20\r\n"
	    "Equipment....           33        303           5\r\n"
	    "Fighters.....           66        404           7\r\n"
	    "Missiles.....            2          5           2\r\n"
	    "Mines........            1          6           3\r\n"
	    "Credits......            9       1000       12345\r\n"
	    "Forces.......            2        250           8\r\n"
	    "Plasma bolts.            3          9           4\r\n"
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? ";
	struct yt_present_state current;
	struct yt_pager_state pager;
	struct pager_capture capture;

	planet_display_cycle_fixture(true, &capture, &current, &pager);
	CHECK(sizeof(expected) - 1U == 742U);
	CHECK(sizeof(expected) - 1U - 80U == 662U);
	CHECK(capture.remote_length == sizeof(expected) - 1U
	    && memcmp(capture.remote, expected, sizeof(expected) - 1U) == 0);
	CHECK(capture.remote_length >= 80U
	    && memcmp(capture.remote + 80U, expected + 80U, 662U) == 0);
	CHECK(pager.line_count == 2.0f && pager.newline_flag == 0.0f);
	planet_display_cycle_fixture(false, &capture, &current, &pager);
	CHECK(capture.remote_length == sizeof(expected) - 1U
	    && memcmp(capture.remote, expected, sizeof(expected) - 1U) == 0);
	CHECK(pager.line_count == 2.0f && pager.newline_flag == 0.0f);
}

static void
planet_sensor_all_zero_cycle_fixture(bool ansi,
    struct pager_capture *capture, struct yt_present_state *current,
    struct yt_pager_state *pager)
{
	static const uint8_t free_holds[] =
	    "You have 65 free cargo holds.";
	static const uint8_t prompt[] =
	    "Time: 14:59  Planet command (?=help) [A]? ";
	static const uint8_t heading[] = "[ Sensors Activated ]";
	static const uint8_t ending[] = "[ End Sensor Scan ]";
	static const uint8_t command[] = "S";
	struct yt_present_result result;
	char accumulator[80] = "";

	*current = state(ansi);
	current->foreground = 6.0f;
	if (ansi)
		CHECK(yt_present_color(current, &result) == YT_PRESENT_OK);
	memset(pager, 0, sizeof(*pager));
	pager->foreground = 6;
	memset(capture, 0, sizeof(*capture));
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager_fixture_b05d(pager, current, free_holds,
	    sizeof(free_holds) - 1U, capture);
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager->newline_flag = 1.0f;
	pager_fixture_b05d(pager, current, prompt, sizeof(prompt) - 1U,
	    capture);
	yt_pager_editor_enter(pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo(command, sizeof(command) - 1U,
	    command, sizeof(command) - 1U, current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);

	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	current->foreground = 7.0f;
	pager->foreground = 7;
	CHECK(yt_present_bold_line(heading, sizeof(heading) - 1U,
	    current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	CHECK(yt_present_sound(4.0f, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	current->foreground = 1.0f;
	pager->foreground = 1;
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	current->foreground = 7.0f;
	pager->foreground = 7;
	CHECK(yt_present_bold_line(ending, sizeof(ending) - 1U,
	    current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);

	pager->line_count = 0.0f;
	current->foreground = 6.0f;
	pager->foreground = 6;
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager_fixture_b05d(pager, current, free_holds,
	    sizeof(free_holds) - 1U, capture);
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager->newline_flag = 1.0f;
	pager_fixture_b05d(pager, current, prompt, sizeof(prompt) - 1U,
	    capture);
}

static void
test_planet_sensor_all_zero_cycle_presentation(void)
{
	static const uint8_t ansi[] =
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? S\r\n"
	    "\r\n\x1b[0;37;40;1m[ Sensors Activated ]\r\n"
	    "\x1b[MBT128O5L48P64CP64C\x0e"
	    "\x1b[0;31;40m\r\n"
	    "\x1b[0;37;40;1m[ End Sensor Scan ]\r\n"
	    "\x1b[0;36;40m\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? ";
	static const uint8_t plain[] =
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? S\r\n"
	    "\r\n[ Sensors Activated ]\r\n"
	    "\r\n[ End Sensor Scan ]\r\n"
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? ";
	struct yt_present_state current;
	struct yt_pager_state pager;
	struct pager_capture capture;

	planet_sensor_all_zero_cycle_fixture(true, &capture, &current, &pager);
	CHECK(sizeof(ansi) - 1U == 271U);
	CHECK(capture.remote_length == sizeof(ansi) - 1U
	    && memcmp(capture.remote, ansi, sizeof(ansi) - 1U) == 0);
	CHECK(pager.line_count == 2.0f && pager.newline_flag == 0.0f);
	planet_sensor_all_zero_cycle_fixture(false, &capture, &current, &pager);
	CHECK(sizeof(plain) - 1U == 205U);
	CHECK(capture.remote_length == sizeof(plain) - 1U
	    && memcmp(capture.remote, plain, sizeof(plain) - 1U) == 0);
	CHECK(pager.line_count == 2.0f && pager.newline_flag == 0.0f);
}

static void
test_planet_rename_presentation(void)
{
	static const uint8_t name_prompt[] =
	    "What do you want to name this planet? -=>";
	static const uint8_t confirmation[] =
	    "\"Nova\" Is this OK? (Y/n) [Y] ?";
	static const uint8_t confirmation_suffix[] =
	    " Is this OK? (Y/n) [Y] ?";
	static const uint8_t protected[] =
	    "You can't re-name this planet!";
	static const uint8_t reserved[] = "I Don't think so!";
	static const uint8_t accepted_expected[] =
	    "\r\nWhat do you want to name this planet? -=>Nova\r\n"
	    "\r\n\"Nova\" Is this OK? (Y/n) [Y] ?Y\r\n";
	static const uint8_t protected_expected[] =
	    "\r\n\x1b[0;36;40;5;1mYou can't re-name this planet!\n\r";
	static const uint8_t reserved_expected[] =
	    "\r\nWhat do you want to name this planet? -=>the WANDERER\r\n"
	    "\r\n\x1b[0;36;40;5;1mI Don't think so!\n\r";
	struct yt_present_state current;
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	char accumulator[300] = "";
	uint8_t long_name[256];
	uint8_t long_confirmation[2U + 41U
	    + sizeof(confirmation_suffix) - 1U];
	uint8_t long_name_expected[373];
	uint8_t long_answer[256];
	uint8_t long_answer_expected[339];
	size_t expected_length;

	current = state(true);
	current.foreground = 6.0f;
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 6;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, name_prompt,
	    sizeof(name_prompt) - 1U, &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo((const uint8_t *)"Nova", 4,
	    (const uint8_t *)"Nova", 4, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_character(confirmation, sizeof(confirmation) - 1U,
	    &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo((const uint8_t *)"Y", 1,
	    (const uint8_t *)"Y", 1, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(sizeof(accepted_expected) - 1U == 84U);
	CHECK(capture.remote_length == sizeof(accepted_expected) - 1U
	    && memcmp(capture.remote, accepted_expected,
	    sizeof(accepted_expected) - 1U) == 0);

	memset(long_name, 'A', sizeof(long_name));
	long_confirmation[0] = '"';
	long_confirmation[1] = 'A';
	memset(long_confirmation + 2U, 'a', 40U);
	long_confirmation[42U] = '"';
	memcpy(long_confirmation + 43U,
	    confirmation_suffix, sizeof(confirmation_suffix) - 1U);
	expected_length = 0U;
	memcpy(long_name_expected + expected_length, "\r\n", 2U);
	expected_length += 2U;
	memcpy(long_name_expected + expected_length, name_prompt,
	    sizeof(name_prompt) - 1U);
	expected_length += sizeof(name_prompt) - 1U;
	memcpy(long_name_expected + expected_length, long_name,
	    sizeof(long_name));
	expected_length += sizeof(long_name);
	memcpy(long_name_expected + expected_length, "\r\n\r\n", 4U);
	expected_length += 4U;
	memcpy(long_name_expected + expected_length, long_confirmation,
	    sizeof(long_confirmation));
	expected_length += sizeof(long_confirmation);
	memcpy(long_name_expected + expected_length, "Y\r\n", 3U);
	expected_length += 3U;
	CHECK(expected_length == sizeof(long_name_expected));

	current = state(true);
	current.foreground = 6.0f;
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 6;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, name_prompt,
	    sizeof(name_prompt) - 1U, &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo(long_name, sizeof(long_name), long_name,
	    sizeof(long_name), &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_character(long_confirmation,
	    sizeof(long_confirmation), &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo((const uint8_t *)"Y", 1U,
	    (const uint8_t *)"Y", 1U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(capture.remote_length == sizeof(long_name_expected)
	    && memcmp(capture.remote, long_name_expected,
	    sizeof(long_name_expected)) == 0);

	long_answer[0] = 'Y';
	memset(long_answer + 1U, 'a', sizeof(long_answer) - 1U);
	expected_length = 0U;
	memcpy(long_answer_expected + expected_length, "\r\n", 2U);
	expected_length += 2U;
	memcpy(long_answer_expected + expected_length, name_prompt,
	    sizeof(name_prompt) - 1U);
	expected_length += sizeof(name_prompt) - 1U;
	memcpy(long_answer_expected + expected_length, "Nova\r\n\r\n", 8U);
	expected_length += 8U;
	memcpy(long_answer_expected + expected_length, confirmation,
	    sizeof(confirmation) - 1U);
	expected_length += sizeof(confirmation) - 1U;
	memcpy(long_answer_expected + expected_length, long_answer,
	    sizeof(long_answer));
	expected_length += sizeof(long_answer);
	memcpy(long_answer_expected + expected_length, "\r\n", 2U);
	expected_length += 2U;
	CHECK(expected_length == sizeof(long_answer_expected));

	current = state(true);
	current.foreground = 6.0f;
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 6;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, name_prompt,
	    sizeof(name_prompt) - 1U, &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo((const uint8_t *)"Nova", 4U,
	    (const uint8_t *)"Nova", 4U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_character(confirmation, sizeof(confirmation) - 1U,
	    &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo(long_answer, sizeof(long_answer),
	    long_answer, sizeof(long_answer), &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(capture.remote_length == sizeof(long_answer_expected)
	    && memcmp(capture.remote, long_answer_expected,
	    sizeof(long_answer_expected)) == 0);

	current = state(true);
	current.foreground = 6.0f;
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 6;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.bold = 1.0f;
	current.blink = 1.0f;
	pager_fixture_b05d(&pager, &current, protected,
	    sizeof(protected) - 1U, &capture);
	CHECK(sizeof(protected_expected) - 1U == 48U);
	CHECK(capture.remote_length == sizeof(protected_expected) - 1U
	    && memcmp(capture.remote, protected_expected,
	    sizeof(protected_expected) - 1U) == 0);

	current = state(true);
	current.foreground = 6.0f;
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 6;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, name_prompt,
	    sizeof(name_prompt) - 1U, &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo((const uint8_t *)"the WANDERER", 12,
	    (const uint8_t *)"the WANDERER", 12, &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.bold = 1.0f;
	current.blink = 1.0f;
	pager_fixture_b05d(&pager, &current, reserved,
	    sizeof(reserved) - 1U, &capture);
	CHECK(sizeof(reserved_expected) - 1U == 92U);
	CHECK(capture.remote_length == sizeof(reserved_expected) - 1U
	    && memcmp(capture.remote, reserved_expected,
	    sizeof(reserved_expected) - 1U) == 0);
}

static void
normal_exit_tail_fixture(bool ansi, bool evaluation,
    struct pager_capture *capture, struct yt_present_state *current,
    struct yt_pager_state *pager)
{
	static const uint8_t generating[] = "Generating ScoreBoard";
	static const uint8_t notice[] = "Cntl-X to Stop";
	static const uint8_t reminder[] =
	    "PLEASE HELP YOUR SYSOP REGISTER THIS GAME.";
	static const uint8_t returning[] = "Returning to Example BBS...";
	struct yt_present_result result;
	int index;

	*current = state(ansi);
	current->sound.user_sound = 0.0f;
	if (ansi)
		CHECK(yt_present_color(current, &result) == YT_PRESENT_OK);
	memset(pager, 0, sizeof(*pager));
	pager->foreground = 2;
	memset(capture, 0, sizeof(*capture));
	current->foreground = 1.0f;
	pager->foreground = 1;
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager->newline_flag = 1.0f;
	pager_fixture_b05d(pager, current, generating,
	    sizeof(generating) - 1U, capture);
	for (index = 0; index < 4; ++index) {
		CHECK(yt_present_character((const uint8_t *)".", 1,
		    current, &result) == YT_PRESENT_OK);
		pager_capture_result(capture, &result);
	}
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);

	pager->nonstop = 1.0f;
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager_fixture_b05d(pager, current, notice, sizeof(notice) - 1U,
	    capture);
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager->line_count = 0.0f;
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);

	if (evaluation) {
		CHECK(yt_present_attention(reminder, sizeof(reminder) - 1U,
		    current, &result) == YT_PRESENT_OK);
		pager_capture_result(capture, &result);
		CHECK(yt_present_line(NULL, 0, current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(capture, &result);
	}
	pager_fixture_b05d(pager, current, returning,
	    sizeof(returning) - 1U, capture);
}

static void
test_normal_exit_tail_presentation(void)
{
	static const uint8_t registered_ansi[] =
	    "\x1b[0;31;40m\r\nGenerating ScoreBoard....\r\n"
	    "\r\nCntl-X to Stop\n\r\r\n\r\n"
	    "Returning to Example BBS...\n\r";
	static const uint8_t registered_plain[] =
	    "\r\nGenerating ScoreBoard....\r\n"
	    "\r\nCntl-X to Stop\n\r\r\n\r\n"
	    "Returning to Example BBS...\n\r";
	static const uint8_t evaluation_ansi[] =
	    "\x1b[0;31;40m\r\nGenerating ScoreBoard....\r\n"
	    "\r\nCntl-X to Stop\n\r\r\n\r\n"
	    "\x1b[0;33;41;5;1m"
	    "PLEASE HELP YOUR SYSOP REGISTER THIS GAME."
	    "\x1b[0;33;40m\r\n\r\n"
	    "Returning to Example BBS...\n\r";
	static const uint8_t evaluation_plain[] =
	    "\r\nGenerating ScoreBoard....\r\n"
	    "\r\nCntl-X to Stop\n\r\r\n\r\n"
	    "PLEASE HELP YOUR SYSOP REGISTER THIS GAME.\r\n\r\n"
	    "Returning to Example BBS...\n\r";
	struct yt_present_state current;
	struct yt_pager_state pager;
	struct pager_capture capture;

	normal_exit_tail_fixture(true, false, &capture, &current, &pager);
	CHECK(capture.remote_length == sizeof(registered_ansi) - 1U
	    && memcmp(capture.remote, registered_ansi,
	    sizeof(registered_ansi) - 1U) == 0);
	normal_exit_tail_fixture(false, false, &capture, &current, &pager);
	CHECK(capture.remote_length == sizeof(registered_plain) - 1U
	    && memcmp(capture.remote, registered_plain,
	    sizeof(registered_plain) - 1U) == 0);
	normal_exit_tail_fixture(true, true, &capture, &current, &pager);
	CHECK(capture.remote_length == sizeof(evaluation_ansi) - 1U
	    && memcmp(capture.remote, evaluation_ansi,
	    sizeof(evaluation_ansi) - 1U) == 0);
	normal_exit_tail_fixture(false, true, &capture, &current, &pager);
	CHECK(capture.remote_length == sizeof(evaluation_plain) - 1U
	    && memcmp(capture.remote, evaluation_plain,
	    sizeof(evaluation_plain) - 1U) == 0);
	CHECK(pager.nonstop == 1.0f && pager.line_count == 1.0f);
}

static void
computer_deactivation_cycle_fixture(bool ansi,
    struct pager_capture *capture, struct yt_present_state *current,
    struct yt_pager_state *pager)
{
	static const uint8_t off[] = "<Computer deactivated>";
	static const uint8_t sector[] = "Sector: 733";
	static const uint8_t warps[] = "Warps lead to: 2, 9";
	static const uint8_t main_prompt[] =
	    "Time: 14:59  Main Command (?=Help)? ";
	struct yt_present_result result;
	char accumulator[80] = "";

	computer_return_prompt_fixture(ansi, 0.0f, capture, current, pager);
	yt_pager_editor_enter(pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo((const uint8_t *)"1", 1,
	    (const uint8_t *)"1", 1, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager->newline_flag = 0.0f;
	pager_fixture_b05d(pager, current, off, sizeof(off) - 1U, capture);

	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	CHECK(yt_present_line(sector, sizeof(sector) - 1U, current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	CHECK(yt_present_line(warps, sizeof(warps) - 1U, current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(capture, &result);

	pager->line_count = 0.0f;
	current->foreground = 2.0f;
	pager->foreground = 2;
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager->newline_flag = 1.0f;
	pager_fixture_b05d(pager, current, main_prompt,
	    sizeof(main_prompt) - 1U, capture);
	yt_pager_editor_enter(pager, accumulator, sizeof(accumulator));
}

static void
test_computer_deactivation_presentation(void)
{
	static const uint8_t ansi[] =
	    "\r\n\x1b[0;31;40mTime: 14:59  Computer command (?=help)? 1\r\n"
	    "\r\n<Computer deactivated>\n\r"
	    "\r\nSector: 733\r\nWarps lead to: 2, 9\r\n"
	    "\x1b[0;32;40m\r\nTime: 14:59  Main Command (?=Help)? ";
	static const uint8_t plain[] =
	    "\r\nTime: 14:59  Computer command (?=help)? 1\r\n"
	    "\r\n<Computer deactivated>\n\r"
	    "\r\nSector: 733\r\nWarps lead to: 2, 9\r\n"
	    "\r\nTime: 14:59  Main Command (?=Help)? ";
	struct yt_present_state current;
	struct yt_pager_state pager;
	struct pager_capture capture;

	computer_deactivation_cycle_fixture(true, &capture, &current, &pager);
	CHECK(sizeof(ansi) - 1U == 165U);
	CHECK(capture.remote_length == sizeof(ansi) - 1U
	    && memcmp(capture.remote, ansi, sizeof(ansi) - 1U) == 0);
	CHECK(pager.line_count == 0.0f);

	computer_deactivation_cycle_fixture(false, &capture, &current, &pager);
	CHECK(sizeof(plain) - 1U == 145U);
	CHECK(capture.remote_length == sizeof(plain) - 1U
	    && memcmp(capture.remote, plain, sizeof(plain) - 1U) == 0);
	CHECK(pager.line_count == 0.0f);
}

static void
computer_sensor_all_zero_cycle_fixture(bool ansi,
    struct pager_capture *capture, struct yt_present_state *current,
    struct yt_pager_state *pager)
{
	static const uint8_t heading[] = "[ Sensors Activated ]";
	static const uint8_t ending[] = "[ End Sensor Scan ]";
	static const uint8_t prompt[] =
	    "Time: 14:59  Computer command (?=help)? ";
	struct yt_present_result result;
	char accumulator[80] = "";

	computer_return_prompt_fixture(ansi, 0.0f, capture, current, pager);
	yt_pager_editor_enter(pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo((const uint8_t *)"S", 1,
	    (const uint8_t *)"S", 1, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);

	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	current->foreground = 7.0f;
	pager->foreground = 7;
	CHECK(yt_present_bold_line(heading, sizeof(heading) - 1U,
	    current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	CHECK(yt_present_sound(4.0f, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	current->foreground = 1.0f;
	pager->foreground = 1;
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	current->foreground = 7.0f;
	pager->foreground = 7;
	CHECK(yt_present_bold_line(ending, sizeof(ending) - 1U,
	    current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);

	current->foreground = 1.0f;
	pager->foreground = 1;
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager->newline_flag = 1.0f;
	pager_fixture_b05d(pager, current, prompt, sizeof(prompt) - 1U,
	    capture);
}

static void
test_computer_sensor_all_zero_cycle_presentation(void)
{
	static const uint8_t ansi[] =
	    "\r\n\x1b[0;31;40mTime: 14:59  Computer command (?=help)? S\r\n"
	    "\r\n\x1b[0;37;40;1m[ Sensors Activated ]\r\n"
	    "\x1b[MBT128O5L48P64CP64C\x0e"
	    "\x1b[0;31;40m\r\n"
	    "\x1b[0;37;40;1m[ End Sensor Scan ]\r\n"
	    "\x1b[0;31;40m\r\n"
	    "Time: 14:59  Computer command (?=help)? ";
	static const uint8_t plain[] =
	    "\r\nTime: 14:59  Computer command (?=help)? S\r\n"
	    "\r\n[ Sensors Activated ]\r\n"
	    "\r\n[ End Sensor Scan ]\r\n"
	    "\r\nTime: 14:59  Computer command (?=help)? ";
	struct yt_present_state current;
	struct yt_pager_state pager;
	struct pager_capture capture;

	computer_sensor_all_zero_cycle_fixture(true, &capture, &current,
	    &pager);
	CHECK(sizeof(ansi) - 1U == 211U);
	CHECK(capture.remote_length == sizeof(ansi) - 1U
	    && memcmp(capture.remote, ansi, sizeof(ansi) - 1U) == 0);
	CHECK(pager.line_count == 1.0f && pager.newline_flag == 0.0f);

	computer_sensor_all_zero_cycle_fixture(false, &capture, &current,
	    &pager);
	CHECK(sizeof(plain) - 1U == 135U);
	CHECK(capture.remote_length == sizeof(plain) - 1U
	    && memcmp(capture.remote, plain, sizeof(plain) - 1U) == 0);
	CHECK(pager.line_count == 1.0f && pager.newline_flag == 0.0f);
}

static void
computer_profit_cycle_fixture(bool ansi, bool all,
    struct pager_capture *capture, struct yt_present_state *current,
    struct yt_pager_state *pager)
{
	static const uint8_t title[] =
	    "Profits of a two way trade to ports in adjacent sectors.";
	static const uint8_t row_one[] =
	    "   2,   4 Equ -> Ore @ Profit of 46 ";
	static const uint8_t row_two[] =
	    "   2,   3 Equ -> Org @ Profit of 54 ";
	static const uint8_t row_three[] =
	    "   3,   4 Org -> Ore @ Profit of 39 ";
	static const uint8_t separator[] = {' ', 0xba, ' '};
	static const uint8_t ending[] = " *-[ End of List ]-*";
	static const uint8_t prompt[] =
	    "Time: 14:59  Computer command (?=help)? ";
	static const uint8_t command_all[] = "16";
	static const uint8_t command_adjacent[] = "17";
	const uint8_t *command = all ? command_all : command_adjacent;
	struct yt_present_result result;
	char accumulator[80] = "";

	CHECK(sizeof(row_one) - 1U == 36U);
	CHECK(sizeof(row_two) - 1U == 36U);
	CHECK(sizeof(row_three) - 1U == 36U);
	computer_return_prompt_fixture(ansi, 0.0f, capture, current, pager);
	yt_pager_editor_enter(pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo(command, 2, command, 2, current,
	    &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);

	if (!all) {
		current->foreground = 7.0f;
		pager->foreground = 7;
		CHECK(yt_present_line(NULL, 0, current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(capture, &result);
		CHECK(yt_present_bold_line(title, sizeof(title) - 1U,
		    current, &result) == YT_PRESENT_OK);
		pager_capture_result(capture, &result);
		CHECK(yt_present_line(NULL, 0, current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(capture, &result);

		current->foreground = 2.0f;
		pager->foreground = 2;
		CHECK(yt_present_bold_line(row_one, sizeof(row_one) - 1U,
		    current, &result) == YT_PRESENT_OK);
		pager_capture_result(capture, &result);
		current->foreground = 3.0f;
		pager->foreground = 3;
		CHECK(yt_present_bold_line(row_two, sizeof(row_two) - 1U,
		    current, &result) == YT_PRESENT_OK);
		pager_capture_result(capture, &result);
	}
	else {
		CHECK(yt_present_line(NULL, 0, current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(capture, &result);

		current->foreground = 2.0f;
		pager->foreground = 2;
		CHECK(yt_present_bold_character(row_one,
		    sizeof(row_one) - 1U, current, &result) == YT_PRESENT_OK);
		pager_capture_result(capture, &result);
		current->foreground = 6.0f;
		pager->foreground = 6;
		CHECK(yt_present_bold_character(separator, sizeof(separator),
		    current, &result) == YT_PRESENT_OK);
		pager_capture_result(capture, &result);

		current->foreground = 3.0f;
		pager->foreground = 3;
		CHECK(yt_present_bold_character(row_two,
		    sizeof(row_two) - 1U, current, &result) == YT_PRESENT_OK);
		pager_capture_result(capture, &result);
		current->foreground = 6.0f;
		pager->foreground = 6;
		CHECK(yt_present_line(NULL, 0, current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(capture, &result);

		current->foreground = 1.0f;
		pager->foreground = 1;
		CHECK(yt_present_bold_character(row_three,
		    sizeof(row_three) - 1U, current, &result) == YT_PRESENT_OK);
		pager_capture_result(capture, &result);
		current->foreground = 6.0f;
		pager->foreground = 6;
		CHECK(yt_present_bold_character(separator, sizeof(separator),
		    current, &result) == YT_PRESENT_OK);
		pager_capture_result(capture, &result);
		current->foreground = 7.0f;
		pager->foreground = 7;
		CHECK(yt_present_bold_line(ending, sizeof(ending) - 1U,
		    current, &result) == YT_PRESENT_OK);
		pager_capture_result(capture, &result);
	}

	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	current->foreground = 1.0f;
	pager->foreground = 1;
	pager->newline_flag = 1.0f;
	pager_fixture_b05d(pager, current, prompt, sizeof(prompt) - 1U,
	    capture);
}

static void
test_computer_profit_cycle_presentation(void)
{
	static const uint8_t adjacent_plain[] =
	    "\r\nTime: 14:59  Computer command (?=help)? 17\r\n"
	    "\r\nProfits of a two way trade to ports in adjacent sectors.\r\n"
	    "\r\n   2,   4 Equ -> Ore @ Profit of 46 \r\n"
	    "   2,   3 Equ -> Org @ Profit of 54 \r\n"
	    "\r\nTime: 14:59  Computer command (?=help)? ";
	static const uint8_t adjacent_ansi[] =
	    "\r\n\x1b[0;31;40mTime: 14:59  Computer command (?=help)? 17\r\n"
	    "\x1b[0;37;40m\r\n"
	    "\x1b[0;37;40;1mProfits of a two way trade to ports in adjacent sectors.\r\n"
	    "\x1b[0;37;40m\r\n"
	    "\x1b[0;32;40;1m   2,   4 Equ -> Ore @ Profit of 46 \r\n"
	    "\x1b[0;33;40;1m   2,   3 Equ -> Org @ Profit of 54 \r\n"
	    "\x1b[0;33;40m\r\n"
	    "\x1b[0;31;40mTime: 14:59  Computer command (?=help)? ";
	static const uint8_t all_plain[] =
	    "\r\nTime: 14:59  Computer command (?=help)? 16\r\n"
	    "\r\n   2,   4 Equ -> Ore @ Profit of 46  \xba "
	    "   2,   3 Equ -> Org @ Profit of 54 \r\n"
	    "   3,   4 Org -> Ore @ Profit of 39  \xba "
	    " *-[ End of List ]-*\r\n"
	    "\r\nTime: 14:59  Computer command (?=help)? ";
	static const uint8_t all_ansi[] =
	    "\r\n\x1b[0;31;40mTime: 14:59  Computer command (?=help)? 16\r\n"
	    "\r\n\x1b[0;32;40;1m   2,   4 Equ -> Ore @ Profit of 46 "
	    "\x1b[0;36;40;1m \xba "
	    "\x1b[0;33;40;1m   2,   3 Equ -> Org @ Profit of 54 "
	    "\x1b[0;36;40m\r\n"
	    "\x1b[0;31;40;1m   3,   4 Org -> Ore @ Profit of 39 "
	    "\x1b[0;36;40;1m \xba "
	    "\x1b[0;37;40;1m *-[ End of List ]-*\r\n"
	    "\x1b[0;37;40m\r\n"
	    "\x1b[0;31;40mTime: 14:59  Computer command (?=help)? ";
	struct yt_present_state current;
	struct yt_pager_state pager;
	struct pager_capture capture;

	computer_profit_cycle_fixture(false, false, &capture, &current,
	    &pager);
	CHECK(sizeof(adjacent_plain) - 1U == 226U);
	CHECK(capture.remote_length == sizeof(adjacent_plain) - 1U
	    && memcmp(capture.remote, adjacent_plain,
	    sizeof(adjacent_plain) - 1U) == 0);
	CHECK(pager.line_count == 1.0f);

	computer_profit_cycle_fixture(true, false, &capture, &current,
	    &pager);
	CHECK(sizeof(adjacent_ansi) - 1U == 312U);
	CHECK(capture.remote_length == sizeof(adjacent_ansi) - 1U
	    && memcmp(capture.remote, adjacent_ansi,
	    sizeof(adjacent_ansi) - 1U) == 0);

	computer_profit_cycle_fixture(false, true, &capture, &current,
	    &pager);
	CHECK(sizeof(all_plain) - 1U == 228U);
	CHECK(capture.remote_length == sizeof(all_plain) - 1U
	    && memcmp(capture.remote, all_plain,
	    sizeof(all_plain) - 1U) == 0);

	computer_profit_cycle_fixture(true, true, &capture, &current,
	    &pager);
	CHECK(sizeof(all_ansi) - 1U == 340U);
	CHECK(capture.remote_length == sizeof(all_ansi) - 1U
	    && memcmp(capture.remote, all_ansi,
	    sizeof(all_ansi) - 1U) == 0);
	CHECK(pager.line_count == 1.0f && pager.newline_flag == 0.0f);
}

static void
test_computer_front_presentation(void)
{
	static const uint8_t activated[] = "<Computer activated>";
	static const uint8_t prompt[] =
	    "Time: 14:59  Computer command (?=help)? ";
	static const uint8_t heading[] = " Computer commands:";
	static const char *const left[8] = {
		" 1) Exit Computer", " 3) Autopilot",
		" 5) Send Radio Message",
		" 7) Set autopilot Sectors to Avoid",
		" 9) Planet Report", "11) Fighter Finder (Yours)",
		"13) Planet Finder (Yours)", "15) Show Active Spies"
	};
	static const char *const right[8] = {
		" 2) Port Report", " 4) Rank Teams & Players",
		" 6) Radio Message Log", " 8) Galactic Newspaper",
		"10) Path Finder", "12) Port(s) Treasury Report",
		"14) Find Nearest Ports", "16) Find Port Pairs"
	};
	static const uint8_t final[] =
	    "17) Check Profits of Adjacent Ports";
	static const uint8_t expected[] =
	    "\r\n<Computer activated>\n\r"
	    "\r\nTime: 14:59  Computer command (?=help)? ?\r\n"
	    "\r\n Computer commands:\n\r\r\n"
	    " 1) Exit Computer                        2) Port Report\n\r"
	    " 3) Autopilot                            4) Rank Teams & Players\n\r"
	    " 5) Send Radio Message                   6) Radio Message Log\n\r"
	    " 7) Set autopilot Sectors to Avoid       8) Galactic Newspaper\n\r"
	    " 9) Planet Report                       10) Path Finder\n\r"
	    "11) Fighter Finder (Yours)              12) Port(s) Treasury Report\n\r"
	    "13) Planet Finder (Yours)               14) Find Nearest Ports\n\r"
	    "15) Show Active Spies                   16) Find Port Pairs\n\r"
	    "17) Check Profits of Adjacent Ports\n\r"
	    "\r\nTime: 14:59  Computer command (?=help)? ";
	struct yt_present_state current = state(false);
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	char accumulator[80] = "";
	size_t index;

	current.foreground = 1.0f;
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 1;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current, activated,
	    sizeof(activated) - 1U, &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, prompt, sizeof(prompt) - 1U,
	    &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo((const uint8_t *)"?", 1,
	    (const uint8_t *)"?", 1, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.line_count = 0.0f;
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current, heading, sizeof(heading) - 1U,
	    &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	for (index = 0; index < 8U; ++index) {
		uint8_t mutable[256];
		size_t length = strlen(left[index]);

		memcpy(mutable, left[index], length);
		CHECK(yt_present_fixed_width(mutable, &length, sizeof(mutable),
		    40.0f, &current, &result) == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		pager_fixture_b05d(&pager, &current,
		    (const uint8_t *)right[index], strlen(right[index]), &capture);
	}
	pager_fixture_b05d(&pager, &current, final, sizeof(final) - 1U,
	    &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, prompt, sizeof(prompt) - 1U,
	    &capture);
	CHECK(sizeof(expected) - 1U == 674U);
	CHECK(capture.remote_length == sizeof(expected) - 1U
	    && memcmp(capture.remote, expected, sizeof(expected) - 1U) == 0);
	CHECK(pager.line_count == 11.0f && pager.newline_flag == 0.0f);
}

enum computer_avoid_fixture_stop {
	COMPUTER_AVOID_FIXTURE_COMPLETE,
	COMPUTER_AVOID_FIXTURE_SLOT_EDITOR,
	COMPUTER_AVOID_FIXTURE_SECTOR_EDITOR,
};

struct computer_avoid_fixture_cut {
	size_t ordinal;
	size_t target;
	size_t fail_carrier_at;
};

static bool
computer_avoid_fixture_b05d(struct yt_pager_state *pager,
    struct yt_present_state *current, const uint8_t *text, size_t length,
    struct pager_capture *capture, struct computer_avoid_fixture_cut *control)
{
	if (control == NULL || control->ordinal != control->target) {
		pager_fixture_b05d(pager, current, text, length, capture);
		if (control != NULL)
			control->ordinal++;
		return true;
	}
	else {
		struct commodity_trade_join join;
		struct commodity_b05d_cut cut;
		bool result;

		memset(&join, 0, sizeof(join));
		join.current = *current;
		join.pager = *pager;
		join.capture = *capture;
		commodity_b05d_cut_init(&cut, &join,
		    control->fail_carrier_at);
		result = yt_paged_row_run(&join.pager, &join.current,
		    &cut.key_state, text, length, &commodity_b05d_cut_ops, &cut);
		*current = join.current;
		*pager = join.pager;
		*capture = join.capture;
		control->ordinal++;
		return result;
	}
}

static void
computer_avoid_accepted_cycle_fixture(bool ansi, float mode,
    float old_value, const char *sector_response,
    enum computer_avoid_fixture_stop stop,
    struct computer_avoid_fixture_cut *cut,
    struct pager_capture *capture, struct yt_present_state *current,
    struct yt_pager_state *pager)
{
	static const uint8_t heading_one[] =
	    "You may set the autopilot to avoid up to 30 sectors";
	static const uint8_t heading_two[] = "Current sectors to avoid are:";
	static const uint8_t slot_prompt[] =
	    "Enter the number of the slot to change [1 - 30]: ";
	static const uint8_t sector_prompt[] =
	    "Enter the sector you wish to avoid [1 - 2004] (0 to clear): ";
	static const uint8_t computer_prompt[] =
	    "Time: 14:59  Computer command (?=help)? ";
	struct yt_present_result result;
	enum yt_computer_avoid_selection_route route;
	char accumulator[80] = "";
	float new_value;
	bool available;
	bool locked;
	int row;

	CHECK(yt_computer_avoid_select_sector(sector_response, 2004.0f,
	    &new_value, &route, NULL));
	CHECK(route == YT_COMPUTER_AVOID_SELECTION_ACCEPTED);
	yt_computer_avoid_transition(old_value, new_value, &locked, &available);

	computer_return_prompt_fixture(ansi, mode, capture, current, pager);
	yt_pager_editor_enter(pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo((const uint8_t *)"7", 1U,
	    (const uint8_t *)"7", 1U, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	CHECK(yt_present_line(NULL, 0U, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);

	CHECK(yt_present_line(NULL, 0U, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	if (!computer_avoid_fixture_b05d(pager, current, heading_one,
	    sizeof(heading_one) - 1U, capture, cut))
		return;
	CHECK(yt_present_line(NULL, 0U, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	if (!computer_avoid_fixture_b05d(pager, current, heading_two,
	    sizeof(heading_two) - 1U, capture, cut))
		return;
	CHECK(yt_present_line(NULL, 0U, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	for (row = 1; row <= 10; ++row) {
		char first[96];
		char middle[96];
		char last[96];
		uint8_t mutable[256];
		size_t length;

		if (row == 1 && old_value == 5.0f)
			(void)snprintf(first, sizeof(first), "[  1 ]  -=>  5");
		else
			(void)snprintf(first, sizeof(first), "[ %2d ]  -=>  0",
			    row);
		(void)snprintf(middle, sizeof(middle), "[%3d ]  -=>  0",
		    row + 10);
		(void)snprintf(last, sizeof(last), "[%3d ]  -=>  0",
		    row + 20);
		length = strlen(first);
		memcpy(mutable, first, length);
		CHECK(yt_present_fixed_width(mutable, &length, sizeof(mutable),
		    20.0f, current, &result) == YT_PRESENT_OK);
		pager_capture_result(capture, &result);
		length = strlen(middle);
		memcpy(mutable, middle, length);
		CHECK(yt_present_fixed_width(mutable, &length, sizeof(mutable),
		    20.0f, current, &result) == YT_PRESENT_OK);
		pager_capture_result(capture, &result);
		if (!computer_avoid_fixture_b05d(pager, current,
		    (const uint8_t *)last, strlen(last), capture, cut))
			return;
	}

	CHECK(yt_present_line(NULL, 0U, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager->newline_flag = 1.0f;
	if (!computer_avoid_fixture_b05d(pager, current, slot_prompt,
	    sizeof(slot_prompt) - 1U, capture, cut))
		return;
	yt_pager_editor_enter(pager, accumulator, sizeof(accumulator));
	if (stop == COMPUTER_AVOID_FIXTURE_SLOT_EDITOR)
		return;
	CHECK(yt_present_editor_echo((const uint8_t *)"1", 1U,
	    (const uint8_t *)"1", 1U, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	CHECK(yt_present_line(NULL, 0U, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);

	CHECK(yt_present_line(NULL, 0U, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager->newline_flag = 1.0f;
	if (!computer_avoid_fixture_b05d(pager, current, sector_prompt,
	    sizeof(sector_prompt) - 1U, capture, cut))
		return;
	yt_pager_editor_enter(pager, accumulator, sizeof(accumulator));
	if (stop == COMPUTER_AVOID_FIXTURE_SECTOR_EDITOR)
		return;
	CHECK(yt_present_editor_echo((const uint8_t *)sector_response,
	    strlen(sector_response), (const uint8_t *)sector_response,
	    strlen(sector_response), current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	CHECK(yt_present_line(NULL, 0U, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);

	current->foreground = 2.0f;
	pager->foreground = 2;
	if (locked) {
		char number[64];
		char status[128];

		CHECK(qb_str_single(number, sizeof(number), new_value) >= 0);
		(void)snprintf(status, sizeof(status),
		    "Sector%s now locked out.", number);
		CHECK(yt_present_line(NULL, 0U, current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(capture, &result);
		if (!computer_avoid_fixture_b05d(pager, current,
		    (const uint8_t *)status, strlen(status), capture, cut))
			return;
	}
	if (available) {
		char number[64];
		char status[128];

		CHECK(qb_str_single(number, sizeof(number), old_value) >= 0);
		(void)snprintf(status, sizeof(status),
		    "Sector%s now available.", number);
		CHECK(yt_present_line(NULL, 0U, current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(capture, &result);
		if (!computer_avoid_fixture_b05d(pager, current,
		    (const uint8_t *)status, strlen(status), capture, cut))
			return;
	}
	current->foreground = 1.0f;
	pager->foreground = 1;
	CHECK(yt_present_line(NULL, 0U, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager->newline_flag = 1.0f;
	(void)computer_avoid_fixture_b05d(pager, current, computer_prompt,
	    sizeof(computer_prompt) - 1U, capture, cut);
}

static void
test_computer_avoid_presentation(void)
{
	static const struct {
		bool ansi;
		const char *response;
		size_t length;
		uint64_t fnv;
		float line_count;
	} transition_cases[] = {
		{false, "", 882U, UINT64_C(0x2688923ae495571a), 2.0f},
		{true, "", 912U, UINT64_C(0x3d259754694cb848), 2.0f},
		{false, "5", 884U, UINT64_C(0x5ae05d03730e91fc), 2.0f},
		{true, "5", 914U, UINT64_C(0xf2d3c94cf5f44b3a), 2.0f},
		{false, "7.5", 915U, UINT64_C(0x379b4eadc2c0e0ec), 3.0f},
		{true, "7.5", 945U, UINT64_C(0xb3c872e5117bb494), 3.0f},
	};
	static const uint8_t heading_one[] =
	    "You may set the autopilot to avoid up to 30 sectors";
	static const uint8_t heading_two[] = "Current sectors to avoid are:";
	static const uint8_t slot_prompt[] =
	    "Enter the number of the slot to change [1 - 30]: ";
	static const uint8_t computer_prompt[] =
	    "Time:10:00  Computer command (?=help)? ";
	static const uint8_t expected[] =
	    "\r\nYou may set the autopilot to avoid up to 30 sectors\n\r"
	    "\r\nCurrent sectors to avoid are:\n\r\r\n"
	    "[  1 ]  -=>  0      [ 11 ]  -=>  0      [ 21 ]  -=>  0\n\r"
	    "[  2 ]  -=>  0      [ 12 ]  -=>  0      [ 22 ]  -=>  0\n\r"
	    "[  3 ]  -=>  0      [ 13 ]  -=>  0      [ 23 ]  -=>  0\n\r"
	    "[  4 ]  -=>  0      [ 14 ]  -=>  0      [ 24 ]  -=>  0\n\r"
	    "[  5 ]  -=>  0      [ 15 ]  -=>  0      [ 25 ]  -=>  0\n\r"
	    "[  6 ]  -=>  0      [ 16 ]  -=>  0      [ 26 ]  -=>  0\n\r"
	    "[  7 ]  -=>  0      [ 17 ]  -=>  0      [ 27 ]  -=>  0\n\r"
	    "[  8 ]  -=>  0      [ 18 ]  -=>  0      [ 28 ]  -=>  0\n\r"
	    "[  9 ]  -=>  0      [ 19 ]  -=>  0      [ 29 ]  -=>  0\n\r"
	    "[ 10 ]  -=>  0      [ 20 ]  -=>  0      [ 30 ]  -=>  0\n\r"
	    "\r\nEnter the number of the slot to change [1 - 30]: \r\n"
	    "\r\nTime:10:00  Computer command (?=help)? ";
	static const uint8_t accepted_plain[] =
	    "\r\nTime: 14:59  Computer command (?=help)? 7\r\n"
	    "\r\nYou may set the autopilot to avoid up to 30 sectors\n\r"
	    "\r\nCurrent sectors to avoid are:\n\r\r\n"
	    "[  1 ]  -=>  0      [ 11 ]  -=>  0      [ 21 ]  -=>  0\n\r"
	    "[  2 ]  -=>  0      [ 12 ]  -=>  0      [ 22 ]  -=>  0\n\r"
	    "[  3 ]  -=>  0      [ 13 ]  -=>  0      [ 23 ]  -=>  0\n\r"
	    "[  4 ]  -=>  0      [ 14 ]  -=>  0      [ 24 ]  -=>  0\n\r"
	    "[  5 ]  -=>  0      [ 15 ]  -=>  0      [ 25 ]  -=>  0\n\r"
	    "[  6 ]  -=>  0      [ 16 ]  -=>  0      [ 26 ]  -=>  0\n\r"
	    "[  7 ]  -=>  0      [ 17 ]  -=>  0      [ 27 ]  -=>  0\n\r"
	    "[  8 ]  -=>  0      [ 18 ]  -=>  0      [ 28 ]  -=>  0\n\r"
	    "[  9 ]  -=>  0      [ 19 ]  -=>  0      [ 29 ]  -=>  0\n\r"
	    "[ 10 ]  -=>  0      [ 20 ]  -=>  0      [ 30 ]  -=>  0\n\r"
	    "\r\nEnter the number of the slot to change [1 - 30]: 1\r\n"
	    "\r\nEnter the sector you wish to avoid [1 - 2004] (0 to clear): "
	    "5\r\n\r\nSector 5 now locked out.\n\r"
	    "\r\nTime: 14:59  Computer command (?=help)? ";
	static const uint8_t accepted_ansi[] =
	    "\r\n\x1b[0;31;40mTime: 14:59  Computer command (?=help)? 7\r\n"
	    "\r\nYou may set the autopilot to avoid up to 30 sectors\n\r"
	    "\r\nCurrent sectors to avoid are:\n\r\r\n"
	    "[  1 ]  -=>  0      [ 11 ]  -=>  0      [ 21 ]  -=>  0\n\r"
	    "[  2 ]  -=>  0      [ 12 ]  -=>  0      [ 22 ]  -=>  0\n\r"
	    "[  3 ]  -=>  0      [ 13 ]  -=>  0      [ 23 ]  -=>  0\n\r"
	    "[  4 ]  -=>  0      [ 14 ]  -=>  0      [ 24 ]  -=>  0\n\r"
	    "[  5 ]  -=>  0      [ 15 ]  -=>  0      [ 25 ]  -=>  0\n\r"
	    "[  6 ]  -=>  0      [ 16 ]  -=>  0      [ 26 ]  -=>  0\n\r"
	    "[  7 ]  -=>  0      [ 17 ]  -=>  0      [ 27 ]  -=>  0\n\r"
	    "[  8 ]  -=>  0      [ 18 ]  -=>  0      [ 28 ]  -=>  0\n\r"
	    "[  9 ]  -=>  0      [ 19 ]  -=>  0      [ 29 ]  -=>  0\n\r"
	    "[ 10 ]  -=>  0      [ 20 ]  -=>  0      [ 30 ]  -=>  0\n\r"
	    "\r\nEnter the number of the slot to change [1 - 30]: 1\r\n"
	    "\r\nEnter the sector you wish to avoid [1 - 2004] (0 to clear): "
	    "5\r\n\x1b[0;32;40m\r\nSector 5 now locked out.\n\r"
	    "\x1b[0;31;40m\r\nTime: 14:59  Computer command (?=help)? ";
	static const uint8_t accepted_mode_two[] =
	    "\r\n\r\n\r\n\r\n\r\n"
	    "[  1 ]  -=>  0      [ 11 ]  -=>  0      "
	    "[  2 ]  -=>  0      [ 12 ]  -=>  0      "
	    "[  3 ]  -=>  0      [ 13 ]  -=>  0      "
	    "[  4 ]  -=>  0      [ 14 ]  -=>  0      "
	    "[  5 ]  -=>  0      [ 15 ]  -=>  0      "
	    "[  6 ]  -=>  0      [ 16 ]  -=>  0      "
	    "[  7 ]  -=>  0      [ 17 ]  -=>  0      "
	    "[  8 ]  -=>  0      [ 18 ]  -=>  0      "
	    "[  9 ]  -=>  0      [ 19 ]  -=>  0      "
	    "[ 10 ]  -=>  0      [ 20 ]  -=>  0      "
	    "\r\n\r\n\r\n\r\n\r\n\r\n";
	struct yt_present_state current = state(false);
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	char accumulator[80] = "";
	int row;
	size_t transition;

	current.foreground = 1.0f;
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 1;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current, heading_one,
	    sizeof(heading_one) - 1U, &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current, heading_two,
	    sizeof(heading_two) - 1U, &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	for (row = 1; row <= 10; ++row) {
		char first[96];
		char middle[96];
		char last[96];
		uint8_t mutable[256];
		size_t length;

		(void)snprintf(first, sizeof(first), "[ %2d ]  -=>  0", row);
		(void)snprintf(middle, sizeof(middle), "[%3d ]  -=>  0",
		    row + 10);
		(void)snprintf(last, sizeof(last), "[%3d ]  -=>  0",
		    row + 20);
		length = strlen(first);
		memcpy(mutable, first, length);
		CHECK(yt_present_fixed_width(mutable, &length, sizeof(mutable),
		    20.0f, &current, &result) == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		length = strlen(middle);
		memcpy(mutable, middle, length);
		CHECK(yt_present_fixed_width(mutable, &length, sizeof(mutable),
		    20.0f, &current, &result) == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		pager_fixture_b05d(&pager, &current, (const uint8_t *)last,
		    strlen(last), &capture);
	}
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, slot_prompt,
	    sizeof(slot_prompt) - 1U, &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, computer_prompt,
	    sizeof(computer_prompt) - 1U, &capture);
	CHECK(sizeof(expected) - 1U == 744U);
	CHECK(capture.remote_length == sizeof(expected) - 1U
	    && memcmp(capture.remote, expected, sizeof(expected) - 1U) == 0);
	CHECK(pager.line_count == 1.0f && pager.newline_flag == 0.0f);

	computer_avoid_accepted_cycle_fixture(false, 0.0f, 0.0f, "5",
	    COMPUTER_AVOID_FIXTURE_COMPLETE, NULL, &capture, &current, &pager);
	CHECK(sizeof(accepted_plain) - 1U == 884U);
	CHECK(capture.remote_length == sizeof(accepted_plain) - 1U
	    && memcmp(capture.remote, accepted_plain,
	    sizeof(accepted_plain) - 1U) == 0);
	CHECK(pager.line_count == 2.0f && pager.newline_flag == 0.0f);
	computer_avoid_accepted_cycle_fixture(true, 0.0f, 0.0f, "5",
	    COMPUTER_AVOID_FIXTURE_COMPLETE, NULL, &capture, &current, &pager);
	CHECK(sizeof(accepted_ansi) - 1U == 914U);
	CHECK(capture.remote_length == sizeof(accepted_ansi) - 1U
	    && memcmp(capture.remote, accepted_ansi,
	    sizeof(accepted_ansi) - 1U) == 0);
	CHECK(pager.line_count == 2.0f && pager.newline_flag == 0.0f);
	CHECK(capture.local_event_count == 129U
	    && capture.local_color_count == 65U
	    && capture.local_line_count == 24U
	    && capture.local_fragment_count == 40U
	    && capture.local_byte_count == 836U
	    && capture.local_fnv == UINT64_C(0x95f5f48462d30f5c));
	computer_avoid_accepted_cycle_fixture(true, 1.0f, 0.0f, "5",
	    COMPUTER_AVOID_FIXTURE_COMPLETE, NULL, &capture, &current, &pager);
	CHECK(capture.remote_length == 0U && pager.line_count == 2.0f
	    && pager.newline_flag == 0.0f
	    && capture.last_local_foreground == 7
	    && capture.last_local_background == 0);
	computer_avoid_accepted_cycle_fixture(true, 2.0f, 0.0f, "5",
	    COMPUTER_AVOID_FIXTURE_COMPLETE, NULL, &capture, &current, &pager);
	CHECK(sizeof(accepted_mode_two) - 1U == 422U);
	CHECK(capture.remote_length == sizeof(accepted_mode_two) - 1U
	    && memcmp(capture.remote, accepted_mode_two,
	    sizeof(accepted_mode_two) - 1U) == 0);
	CHECK(pager.line_count == 2.0f && pager.newline_flag == 0.0f
	    && capture.last_local_foreground == 7
	    && capture.last_local_background == 0);
	for (transition = 0U; transition < YT_ARRAY_LEN(transition_cases);
	    ++transition) {
		computer_avoid_accepted_cycle_fixture(
		    transition_cases[transition].ansi, 0.0f, 5.0f,
		    transition_cases[transition].response,
		    COMPUTER_AVOID_FIXTURE_COMPLETE, NULL, &capture, &current,
		    &pager);
		CHECK(capture.remote_length == transition_cases[transition].length
		    && viewer_bytes_fnv1a64(capture.remote,
		    capture.remote_length) == transition_cases[transition].fnv);
		CHECK(pager.line_count == transition_cases[transition].line_count
		    && pager.newline_flag == 0.0f
		    && capture.last_local_foreground == 7
		    && capture.last_local_background == 0);
	}
}

static void
test_computer_port_report_presentation(void)
{
	static const uint8_t prompt[] = "Enter sector number port is in -=> ";
	static const uint8_t unavailable[] = "No information available.";
	static const uint8_t expected[] =
	    "\r\nEnter sector number port is in -=> 3\r\n"
	    "\r\nNo information available.\n\r";
	static const uint8_t dependency_prefix[] =
	    "\r\nEnter sector number port is in -=> 2\r\n";
	struct yt_present_state current = state(false);
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	char accumulator[80] = "";

	current.foreground = 1.0f;
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 1;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, prompt, sizeof(prompt) - 1U,
	    &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo((const uint8_t *)"3", 1,
	    (const uint8_t *)"3", 1, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current, unavailable,
	    sizeof(unavailable) - 1U, &capture);
	CHECK(capture.remote_length == sizeof(expected) - 1U
	    && memcmp(capture.remote, expected, sizeof(expected) - 1U) == 0);
	CHECK(pager.line_count == 1.0f);

	/* Shared prefix for sector/current-friend/candidate-friend GET cuts. */
	current = state(false);
	current.foreground = 1.0f;
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 1;
	memset(&capture, 0, sizeof(capture));
	memset(accumulator, 0, sizeof(accumulator));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, prompt, sizeof(prompt) - 1U,
	    &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo((const uint8_t *)"2", 1U,
	    (const uint8_t *)"2", 1U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(capture.remote_length == sizeof(dependency_prefix) - 1U
	    && memcmp(capture.remote, dependency_prefix,
	    sizeof(dependency_prefix) - 1U) == 0
	    && pager.line_count == 0.0f);
	CHECK(sizeof(dependency_prefix) - 1U == 40U);
}

static void
test_computer_port_report_short_cycles_presentation(void)
{
	static const uint8_t sector_prompt[] =
	    "Enter sector number port is in -=> ";
	static const uint8_t unavailable[] = "No information available.";
	static const uint8_t computer_prompt[] =
	    "Time:15:00  Computer command (?=help)? ";
	static const uint8_t no_port[] =
	    "\r\nEnter sector number port is in -=> 3\r\n"
	    "\r\nNo information available.\n\r"
	    "\r\nTime:15:00  Computer command (?=help)? ";
	static const uint8_t blank[] =
	    "\r\nEnter sector number port is in -=> \r\n"
	    "\r\nTime:15:00  Computer command (?=help)? ";
	static const struct {
		bool unavailable;
		float mode;
		const uint8_t *expected;
		size_t expected_length;
	} cases[] = {
		{true, 0.0f, no_port, sizeof(no_port) - 1U},
		{false, 0.0f, blank, sizeof(blank) - 1U},
		{true, 1.0f, NULL, 0U},
		{false, 1.0f, NULL, 0U},
		{true, 2.0f, (const uint8_t *)"\r\n\r\n\r\n\r\n", 8U},
		{false, 2.0f, (const uint8_t *)"\r\n\r\n\r\n", 6U},
	};
	size_t pass;

	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		struct yt_present_state current = state(true);
		struct yt_present_result result;
		struct yt_pager_state pager;
		struct pager_capture capture;
		char accumulator[80];

		current.sound.mode = cases[pass].mode;
		current.foreground = 1.0f;
		current.cached_foreground = 1.0f;
		memset(&pager, 0, sizeof(pager));
		pager.foreground = 1;
		memset(&capture, 0, sizeof(capture));
		memset(accumulator, 0, sizeof(accumulator));
		CHECK(yt_present_line(NULL, 0, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		pager.newline_flag = 1.0f;
		pager_fixture_b05d(&pager, &current, sector_prompt,
		    sizeof(sector_prompt) - 1U, &capture);
		yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
		if (cases[pass].unavailable) {
			CHECK(yt_present_editor_echo((const uint8_t *)"3", 1U,
			    (const uint8_t *)"3", 1U, &current, &result)
			    == YT_PRESENT_OK);
			pager_capture_result(&capture, &result);
		}
		CHECK(yt_present_line(NULL, 0, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		if (cases[pass].unavailable) {
			CHECK(yt_present_line(NULL, 0, &current, &result)
			    == YT_PRESENT_OK);
			pager_capture_result(&capture, &result);
			pager_fixture_b05d(&pager, &current, unavailable,
			    sizeof(unavailable) - 1U, &capture);
		}
		CHECK(yt_present_line(NULL, 0, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		pager.foreground = 1;
		current.foreground = 1.0f;
		pager.newline_flag = 1.0f;
		pager_fixture_b05d(&pager, &current, computer_prompt,
		    sizeof(computer_prompt) - 1U, &capture);
		yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
		CHECK(capture.remote_length == cases[pass].expected_length
		    && (cases[pass].expected_length == 0U
		    || memcmp(capture.remote, cases[pass].expected,
		    cases[pass].expected_length) == 0)
		    && pager.line_count == 0.0f
		    && pager.newline_flag == 0.0f
		    && accumulator[0] == '\0');
	}
	CHECK(sizeof(no_port) - 1U == 110U && sizeof(blank) - 1U == 80U);
}

static void
test_computer_port_report_wrapper_b05d_cuts(void)
{
	static const uint8_t prompt[] =
	    "Enter sector number port is in -=> ";
	static const uint8_t unavailable[] = "No information available.";
	static const uint8_t earth_title[] =
	    "Commerce report for Earth: 07-25-2026 12:34:56";
	static const uint8_t prompt_before[] = "\r\n";
	static const uint8_t prompt_after[] =
	    "\r\nEnter sector number port is in -=> ";
	static const uint8_t unavailable_before[] =
	    "\r\nEnter sector number port is in -=> 3\r\n\r\n";
	static const uint8_t unavailable_after[] =
	    "\r\nEnter sector number port is in -=> 3\r\n\r\n"
	    "No information available.";
	static const uint8_t earth_title_before[] =
	    "\r\nEnter sector number port is in -=> 1\r\n\r\n";
	static const uint8_t earth_title_after[] =
	    "\r\nEnter sector number port is in -=> 1\r\n\r\n"
	    "Commerce report for Earth: 07-25-2026 12:34:56";
	struct commodity_trade_join join;
	struct commodity_b05d_cut cut;

	commodity_trade_join_init(&join);
	join.current = state(false);
	commodity_trade_join_line(&join);
	join.pager.newline_flag = 1.0f;
	commodity_b05d_cut_init(&cut, &join, 0U);
	CHECK(!yt_paged_row_run(&join.pager, &join.current, &cut.key_state,
	    prompt, sizeof(prompt) - 1U, &commodity_b05d_cut_ops, &cut));
	commodity_trade_join_check(&join, prompt_before,
	    sizeof(prompt_before) - 1U);
	CHECK(cut.carrier_calls == 1U && cut.sample_calls == 0U
	    && cut.present_calls == 0U && cut.finish_calls == 0U
	    && join.pager.line_count == 0.0f);

	commodity_trade_join_init(&join);
	join.current = state(false);
	commodity_trade_join_line(&join);
	join.pager.newline_flag = 1.0f;
	commodity_b05d_cut_init(&cut, &join, 1U);
	CHECK(!yt_paged_row_run(&join.pager, &join.current, &cut.key_state,
	    prompt, sizeof(prompt) - 1U, &commodity_b05d_cut_ops, &cut));
	commodity_trade_join_check(&join, prompt_after,
	    sizeof(prompt_after) - 1U);
	CHECK(cut.carrier_calls == 2U && cut.sample_calls == 1U
	    && cut.present_calls == 1U && cut.finish_calls == 0U
	    && join.pager.line_count == 0.0f);

	commodity_trade_join_init(&join);
	join.current = state(false);
	commodity_trade_join_line(&join);
	commodity_trade_join_b05d(&join, prompt, sizeof(prompt) - 1U, true);
	commodity_trade_join_input(&join, (const uint8_t *)"3", 1U);
	commodity_trade_join_line(&join);
	commodity_b05d_cut_init(&cut, &join, 0U);
	CHECK(!yt_paged_row_run(&join.pager, &join.current, &cut.key_state,
	    unavailable, sizeof(unavailable) - 1U,
	    &commodity_b05d_cut_ops, &cut));
	commodity_trade_join_check(&join, unavailable_before,
	    sizeof(unavailable_before) - 1U);
	CHECK(cut.carrier_calls == 1U && cut.sample_calls == 0U
	    && cut.present_calls == 0U && cut.finish_calls == 0U
	    && join.pager.line_count == 0.0f);

	commodity_trade_join_init(&join);
	join.current = state(false);
	commodity_trade_join_line(&join);
	commodity_trade_join_b05d(&join, prompt, sizeof(prompt) - 1U, true);
	commodity_trade_join_input(&join, (const uint8_t *)"3", 1U);
	commodity_trade_join_line(&join);
	commodity_b05d_cut_init(&cut, &join, 1U);
	CHECK(!yt_paged_row_run(&join.pager, &join.current, &cut.key_state,
	    unavailable, sizeof(unavailable) - 1U,
	    &commodity_b05d_cut_ops, &cut));
	commodity_trade_join_check(&join, unavailable_after,
	    sizeof(unavailable_after) - 1U);
	CHECK(cut.carrier_calls == 2U && cut.sample_calls == 1U
	    && cut.present_calls == 1U && cut.finish_calls == 0U
	    && join.pager.line_count == 0.0f);

	commodity_trade_join_init(&join);
	join.current = state(false);
	commodity_trade_join_line(&join);
	commodity_trade_join_b05d(&join, prompt, sizeof(prompt) - 1U, true);
	commodity_trade_join_input(&join, (const uint8_t *)"1", 1U);
	join.pager.line_count = 0.0f;
	join.current.foreground = 3.0f;
	join.pager.foreground = 3;
	commodity_trade_join_line(&join);
	commodity_b05d_cut_init(&cut, &join, 0U);
	CHECK(!yt_paged_row_run(&join.pager, &join.current, &cut.key_state,
	    earth_title, sizeof(earth_title) - 1U,
	    &commodity_b05d_cut_ops, &cut));
	commodity_trade_join_check(&join, earth_title_before,
	    sizeof(earth_title_before) - 1U);
	CHECK(cut.carrier_calls == 1U && cut.sample_calls == 0U
	    && cut.present_calls == 0U && cut.finish_calls == 0U
	    && join.pager.line_count == 0.0f);

	commodity_trade_join_init(&join);
	join.current = state(false);
	commodity_trade_join_line(&join);
	commodity_trade_join_b05d(&join, prompt, sizeof(prompt) - 1U, true);
	commodity_trade_join_input(&join, (const uint8_t *)"1", 1U);
	join.pager.line_count = 0.0f;
	join.current.foreground = 3.0f;
	join.pager.foreground = 3;
	commodity_trade_join_line(&join);
	commodity_b05d_cut_init(&cut, &join, 1U);
	CHECK(!yt_paged_row_run(&join.pager, &join.current, &cut.key_state,
	    earth_title, sizeof(earth_title) - 1U,
	    &commodity_b05d_cut_ops, &cut));
	commodity_trade_join_check(&join, earth_title_after,
	    sizeof(earth_title_after) - 1U);
	CHECK(cut.carrier_calls == 2U && cut.sample_calls == 1U
	    && cut.present_calls == 1U && cut.finish_calls == 0U
	    && join.pager.line_count == 0.0f);
	CHECK(sizeof(prompt_before) - 1U == 2U
	    && sizeof(prompt_after) - 1U == 37U
	    && sizeof(unavailable_before) - 1U == 42U
	    && sizeof(unavailable_after) - 1U == 67U
	    && sizeof(earth_title_before) - 1U == 42U
	    && sizeof(earth_title_after) - 1U == 88U);
}

struct computer_port_terminal_join {
	struct yt_present_state *current;
	struct yt_pager_state *pager;
	struct pager_capture *capture;
	bool *running;
	bool *terminated;
	int notice_carrier_failure;
	bool closed;
};

static bool
computer_port_terminal_carrier_end(struct computer_port_terminal_join *join)
{
	join->closed = true;
	*join->running = false;
	*join->terminated = true;
	return false;
}

static bool
computer_port_terminal_notice(void *context, const uint8_t *notice,
    size_t length)
{
	struct computer_port_terminal_join *join = context;
	struct yt_present_result result;

	if (yt_present_line(NULL, 0U, join->current, &result)
	    != YT_PRESENT_OK)
		return false;
	pager_capture_result(join->capture, &result);
	if (join->notice_carrier_failure == 1)
		return computer_port_terminal_carrier_end(join);
	if (join->notice_carrier_failure == 2) {
		if (yt_present_paged_text(notice, length, join->current, &result)
		    != YT_PRESENT_OK)
			return false;
		pager_capture_result(join->capture, &result);
		return computer_port_terminal_carrier_end(join);
	}
	pager_fixture_b05d(join->pager, join->current, notice, length,
	    join->capture);
	return true;
}

static bool
computer_port_terminal_close(void *context)
{
	struct computer_port_terminal_join *join = context;

	join->closed = true;
	return true;
}

static void
test_computer_port_report_terminal_presentation(void)
{
	static const uint8_t prompt[] =
	    "Enter sector number port is in -=> ";
	static const uint8_t inactivity[] =
	    "\r\nEnter sector number port is in -=> "
	    "\r\n\aUSER FELL ASLEEP!\n\r";
	static const uint8_t session_limit[] =
	    "\r\nEnter sector number port is in -=> "
	    "\r\n\a\a\aTIME LIMIT EXCEEDED!\a\a\a\n\r";
	static const uint8_t inactivity_carrier[] =
	    "\r\nEnter sector number port is in -=> \r\n";
	static const uint8_t direct_carrier[] =
	    "\r\nEnter sector number port is in -=> ";
	static const uint8_t session_carrier[] =
	    "\r\nEnter sector number port is in -=> "
	    "\r\n\a\a\aTIME LIMIT EXCEEDED!\a\a\a";
	static const struct {
		enum yt_ab36_terminal_kind kind;
		const uint8_t *expected;
		size_t expected_length;
	} cases[] = {
		{YT_AB36_TERMINAL_INACTIVITY, inactivity,
		    sizeof(inactivity) - 1U},
		{YT_AB36_TERMINAL_SESSION_LIMIT, session_limit,
		    sizeof(session_limit) - 1U},
	};
	size_t pass;

	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		struct yt_present_state current = state(true);
		struct yt_present_result result;
		struct yt_pager_state pager;
		struct pager_capture capture;
		struct computer_port_terminal_join join;
		char accumulator[80];
		bool running = true;
		bool terminated = false;

		current.foreground = 1.0f;
		current.cached_foreground = 1.0f;
		memset(&pager, 0, sizeof(pager));
		pager.foreground = 1;
		memset(&capture, 0, sizeof(capture));
		memset(accumulator, 0, sizeof(accumulator));
		CHECK(yt_present_line(NULL, 0U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		pager.newline_flag = 1.0f;
		pager_fixture_b05d(&pager, &current, prompt,
		    sizeof(prompt) - 1U, &capture);
		yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
		join.current = &current;
		join.pager = &pager;
		join.capture = &capture;
		join.running = &running;
		join.terminated = &terminated;
		join.notice_carrier_failure = 0;
		join.closed = false;
		CHECK(yt_input_ab36_terminal_run(cases[pass].kind, &running,
		    &terminated, computer_port_terminal_notice,
		    computer_port_terminal_close, &join));
		CHECK(capture.remote_length == cases[pass].expected_length
		    && memcmp(capture.remote, cases[pass].expected,
		    cases[pass].expected_length) == 0
		    && !running && terminated && join.closed
		    && pager.line_count == 1.0f
		    && pager.newline_flag == 0.0f
		    && accumulator[0] == '\0');
	}
	{
		static const struct {
			enum yt_ab36_terminal_kind kind;
			int failure;
			const uint8_t *expected;
			size_t expected_length;
		} carrier_cases[] = {
			{YT_AB36_TERMINAL_INACTIVITY, 1,
			    inactivity_carrier, sizeof(inactivity_carrier) - 1U},
			{YT_AB36_TERMINAL_SESSION_LIMIT, 2,
			    session_carrier, sizeof(session_carrier) - 1U},
		};
		size_t failure;

		for (failure = 0U; failure < YT_ARRAY_LEN(carrier_cases);
		    ++failure) {
			struct yt_present_state current = state(true);
			struct yt_present_result result;
			struct yt_pager_state pager;
			struct pager_capture capture;
			struct computer_port_terminal_join join;
			char accumulator[80];
			bool running = true;
			bool terminated = false;

			current.foreground = 1.0f;
			current.cached_foreground = 1.0f;
			memset(&pager, 0, sizeof(pager));
			pager.foreground = 1;
			memset(&capture, 0, sizeof(capture));
			memset(accumulator, 0, sizeof(accumulator));
			CHECK(yt_present_line(NULL, 0U, &current, &result)
			    == YT_PRESENT_OK);
			pager_capture_result(&capture, &result);
			pager.newline_flag = 1.0f;
			pager_fixture_b05d(&pager, &current, prompt,
			    sizeof(prompt) - 1U, &capture);
			yt_pager_editor_enter(&pager, accumulator,
			    sizeof(accumulator));
			join.current = &current;
			join.pager = &pager;
			join.capture = &capture;
			join.running = &running;
			join.terminated = &terminated;
			join.notice_carrier_failure =
			    carrier_cases[failure].failure;
			join.closed = false;
			CHECK(!yt_input_ab36_terminal_run(
			    carrier_cases[failure].kind,
			    &running, &terminated, computer_port_terminal_notice,
			    computer_port_terminal_close, &join));
			CHECK(capture.remote_length
			    == carrier_cases[failure].expected_length
			    && memcmp(capture.remote,
			    carrier_cases[failure].expected,
			    carrier_cases[failure].expected_length) == 0
			    && !running && terminated && join.closed
			    && pager.line_count == 0.0f);
		}
	}
	{
		struct yt_present_state current = state(true);
		struct yt_present_result result;
		struct yt_pager_state pager;
		struct pager_capture capture;
		struct computer_port_terminal_join join;
		char accumulator[80];
		bool running = true;
		bool terminated = false;

		current.foreground = 1.0f;
		current.cached_foreground = 1.0f;
		memset(&pager, 0, sizeof(pager));
		pager.foreground = 1;
		memset(&capture, 0, sizeof(capture));
		memset(accumulator, 0, sizeof(accumulator));
		CHECK(yt_present_line(NULL, 0U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		pager.newline_flag = 1.0f;
		pager_fixture_b05d(&pager, &current, prompt,
		    sizeof(prompt) - 1U, &capture);
		yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
		join.current = &current;
		join.pager = &pager;
		join.capture = &capture;
		join.running = &running;
		join.terminated = &terminated;
		join.notice_carrier_failure = 0;
		join.closed = false;
		CHECK(!computer_port_terminal_carrier_end(&join));
		CHECK(capture.remote_length == sizeof(direct_carrier) - 1U
		    && memcmp(capture.remote, direct_carrier,
		    sizeof(direct_carrier) - 1U) == 0
		    && !running && terminated && join.closed);
	}
	CHECK(sizeof(inactivity) - 1U == 59U
	    && sizeof(session_limit) - 1U == 67U
	    && sizeof(inactivity_carrier) - 1U == 39U
	    && sizeof(direct_carrier) - 1U == 37U
	    && sizeof(session_carrier) - 1U == 65U);
}

static void
test_computer_avoid_terminal_presentation(void)
{
	static const struct {
		enum computer_avoid_fixture_stop stop;
		bool ansi;
		size_t prefix_length;
		uint64_t prefix_fnv;
		size_t inactivity_length;
		uint64_t inactivity_fnv;
		size_t session_length;
		uint64_t session_fnv;
	} cases[] = {
		{COMPUTER_AVOID_FIXTURE_SLOT_EDITOR, false,
		    746U, UINT64_C(0x668590d6d691f5e3),
		    768U, UINT64_C(0x875a8566c9e3b46f),
		    776U, UINT64_C(0x733cdcdc12c30bc1)},
		{COMPUTER_AVOID_FIXTURE_SLOT_EDITOR, true,
		    756U, UINT64_C(0x0d976d4ba97f19b4),
		    778U, UINT64_C(0x6234d311f50b5ed4),
		    786U, UINT64_C(0xb897f95a379da376)},
		{COMPUTER_AVOID_FIXTURE_SECTOR_EDITOR, false,
		    811U, UINT64_C(0xdb26edf5a552323e),
		    833U, UINT64_C(0xd336f712950de70a),
		    841U, UINT64_C(0x691c295df1aa1f1c)},
		{COMPUTER_AVOID_FIXTURE_SECTOR_EDITOR, true,
		    821U, UINT64_C(0xa2746da79a150673),
		    843U, UINT64_C(0x1ae761c0cbbe315f),
		    851U, UINT64_C(0xb451bf573d5e11b1)},
	};
	static const struct {
		enum yt_ab36_terminal_kind kind;
		bool inactivity;
	} terminals[] = {
		{YT_AB36_TERMINAL_INACTIVITY, true},
		{YT_AB36_TERMINAL_SESSION_LIMIT, false},
	};
	size_t pass;

	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		struct yt_present_state current;
		struct yt_pager_state pager;
		struct pager_capture capture;
		struct computer_port_terminal_join join;
		bool running = true;
		bool terminated = false;
		size_t terminal;

		computer_avoid_accepted_cycle_fixture(cases[pass].ansi, 0.0f,
		    0.0f, "5", cases[pass].stop, NULL, &capture, &current,
		    &pager);
		CHECK(capture.remote_length == cases[pass].prefix_length
		    && viewer_bytes_fnv1a64(capture.remote,
		    capture.remote_length) == cases[pass].prefix_fnv
		    && pager.line_count == 0.0f && pager.newline_flag == 0.0f);
		join.current = &current;
		join.pager = &pager;
		join.capture = &capture;
		join.running = &running;
		join.terminated = &terminated;
		join.notice_carrier_failure = 0;
		join.closed = false;
		CHECK(!computer_port_terminal_carrier_end(&join));
		CHECK(!running && terminated && join.closed
		    && capture.remote_length == cases[pass].prefix_length);

		for (terminal = 0U; terminal < YT_ARRAY_LEN(terminals);
		    ++terminal) {
			size_t expected_length = terminals[terminal].inactivity
			    ? cases[pass].inactivity_length
			    : cases[pass].session_length;
			uint64_t expected_fnv = terminals[terminal].inactivity
			    ? cases[pass].inactivity_fnv
			    : cases[pass].session_fnv;

			computer_avoid_accepted_cycle_fixture(cases[pass].ansi,
			    0.0f, 0.0f, "5", cases[pass].stop, NULL, &capture,
			    &current, &pager);
			running = true;
			terminated = false;
			join.current = &current;
			join.pager = &pager;
			join.capture = &capture;
			join.running = &running;
			join.terminated = &terminated;
			join.notice_carrier_failure = 0;
			join.closed = false;
			CHECK(yt_input_ab36_terminal_run(
			    terminals[terminal].kind, &running, &terminated,
			    computer_port_terminal_notice,
			    computer_port_terminal_close, &join));
			CHECK(capture.remote_length == expected_length
			    && viewer_bytes_fnv1a64(capture.remote,
			    capture.remote_length) == expected_fnv
			    && !running && terminated && join.closed
			    && pager.line_count == 1.0f
			    && pager.newline_flag == 0.0f);
		}
	}
}

static void
test_computer_avoid_b05d_cuts(void)
{
	static const struct {
		size_t before_length;
		uint64_t before_fnv;
		size_t after_length;
		uint64_t after_fnv;
	} cuts[] = {
		{57U, UINT64_C(0x3cb36c15963fd333),
		    108U, UINT64_C(0xc38f75e6a1aa9115)},
		{112U, UINT64_C(0x31923d6309f79b27),
		    141U, UINT64_C(0x96d169eeb43ea95f)},
		{185U, UINT64_C(0x994316daf4249237),
		    199U, UINT64_C(0xaaafa81d3cc3c046)},
		{241U, UINT64_C(0xfde51a6b989fef76),
		    255U, UINT64_C(0xff834fedac2525aa)},
		{297U, UINT64_C(0x220538d26dfb3da6),
		    311U, UINT64_C(0xd51e7f4868a9262d)},
		{353U, UINT64_C(0x07b2028637884659),
		    367U, UINT64_C(0x9ce9fbcec8d817ab)},
		{409U, UINT64_C(0x89aeb2d69d832d17),
		    423U, UINT64_C(0xb778b2bf4f829072)},
		{465U, UINT64_C(0x4e62db44754dec3a),
		    479U, UINT64_C(0x341074a10419d28a)},
		{521U, UINT64_C(0x4034ec77d88e1fee),
		    535U, UINT64_C(0x10a1e0a3b53a78d9)},
		{577U, UINT64_C(0x249881a78ecfec4d),
		    591U, UINT64_C(0x0ee764f490253fdb)},
		{633U, UINT64_C(0xa59efaaab18276b7),
		    647U, UINT64_C(0xd712c9f2f036850e)},
		{689U, UINT64_C(0x4953d1b55d4dcf50),
		    703U, UINT64_C(0xfce1f5e4088a76ef)},
		{707U, UINT64_C(0xe1f16131d16d15d1),
		    756U, UINT64_C(0xfc4f56102d9cc6ad)},
		{761U, UINT64_C(0x78dd6b41821dd440),
		    821U, UINT64_C(0x39228cc21c21a344)},
		{836U, UINT64_C(0x4a5f4fcc31cc242b),
		    860U, UINT64_C(0x85083d0072774708)},
		{864U, UINT64_C(0xa6db08ece5c67d3a),
		    887U, UINT64_C(0x2a96b3b4a2c76256)},
		{901U, UINT64_C(0x313724eadc584cf7),
		    941U, UINT64_C(0xb3e2d1b2892dd7f4)},
	};
	size_t ordinal;

	for (ordinal = 0U; ordinal < YT_ARRAY_LEN(cuts); ++ordinal) {
		size_t side;

		for (side = 0U; side < 2U; ++side) {
			struct computer_avoid_fixture_cut cut = {
				.ordinal = 0U,
				.target = ordinal,
				.fail_carrier_at = side,
			};
			struct yt_present_state current;
			struct yt_pager_state pager;
			struct pager_capture capture;
			size_t expected_length = side == 0U
			    ? cuts[ordinal].before_length
			    : cuts[ordinal].after_length;
			uint64_t expected_fnv = side == 0U
			    ? cuts[ordinal].before_fnv : cuts[ordinal].after_fnv;

			computer_avoid_accepted_cycle_fixture(true, 0.0f, 5.0f,
			    "7", COMPUTER_AVOID_FIXTURE_COMPLETE, &cut, &capture,
			    &current, &pager);
			CHECK(cut.ordinal == ordinal + 1U
			    && capture.remote_length == expected_length
			    && viewer_bytes_fnv1a64(capture.remote,
			    capture.remote_length) == expected_fnv);
		}
	}
}

static void
test_computer_port_report_ab36_state_joins(void)
{
	struct yt_present_state current = state(true);
	struct yt_pager_state pager;
	struct yt_b05d_key_state key_state;
	struct yt_input_value selected;
	struct yt_input_value sampled;
	char accumulator[32];
	char response[32];
	char queue[32];
	size_t queue_position;
	size_t queue_length;

	/* Initial command-2 selector: inherit the computer editor's queue. */
	memset(&pager, 0, sizeof(pager));
	pager.line_count = 17.0f;
	pager.nonstop = 1.0f;
	pager.newline_flag = 0.0f;
	memcpy(pager.key, "Q", 2U);
	memcpy(accumulator, "stale", 6U);
	memcpy(queue, "2\r", 3U);
	queue_position = 0U;
	queue_length = 2U;
	current.foreground = 1.0f;
	current.bold = 0.0f;
	current.blink = 0.0f;
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(pager.line_count == 0.0f && pager.nonstop == 0.0f
	    && pager.newline_flag == 0.0f && pager.key[0] == '\0'
	    && accumulator[0] == '\0' && queue_position == 0U
	    && queue_length == 2U && memcmp(queue, "2\r", 2U) == 0
	    && current.foreground == 1.0f && current.bold == 0.0f
	    && current.blink == 0.0f);
	CHECK(yt_input_ab36_queue_pop(queue, sizeof(queue), &queue_position,
	    &queue_length, &selected)
	    && selected.length == 1U && selected.bytes[0] == '2'
	    && queue_position == 1U && queue_length == 2U);
	CHECK(yt_input_ab36_queue_pop(queue, sizeof(queue), &queue_position,
	    &queue_length, &selected)
	    && selected.length == 1U && selected.bytes[0] == '\r'
	    && queue_position == 0U && queue_length == 0U);

	/* Invalid selection: 02DB discards its semicolon typeahead. */
	memcpy(response, "2005;99", 8U);
	queue[0] = '\0';
	queue_position = 0U;
	queue_length = 0U;
	CHECK(yt_input_split_semicolon(response, queue, sizeof(queue),
	    &queue_position, &queue_length)
	    && strcmp(response, "2005") == 0
	    && queue_position == 0U && queue_length == 3U
	    && memcmp(queue, "99\r", 3U) == 0);
	current.bold = 0.0f;
	current.blink = 0.0f;
	CHECK(yt_input_queue_clear(queue, sizeof(queue), &queue_position,
	    &queue_length));
	pager.line_count = 2.0f;
	pager.nonstop = 1.0f;
	pager.newline_flag = 0.0f;
	memcpy(pager.key, "E", 2U);
	memcpy(accumulator, "2005", 5U);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(pager.line_count == 0.0f && pager.nonstop == 0.0f
	    && pager.newline_flag == 0.0f && pager.key[0] == '\0'
	    && accumulator[0] == '\0' && queue[0] == '\0'
	    && queue_position == 0U && queue_length == 0U
	    && current.bold == 0.0f && current.blink == 0.0f);
	CHECK(yt_input_ab36_queue_pop(queue, sizeof(queue), &queue_position,
	    &queue_length, &selected) && selected.length == 0U);

	/* Fresh 8639 prompt: retain typeahead collected by report B05D calls. */
	memset(&key_state, 0, sizeof(key_state));
	key_state.accumulator = accumulator;
	key_state.accumulator_capacity = sizeof(accumulator);
	key_state.queue = queue;
	key_state.queue_capacity = sizeof(queue);
	key_state.queue_position = &queue_position;
	key_state.queue_length = &queue_length;
	key_state.pager_key = pager.key;
	key_state.pager_key_capacity = sizeof(pager.key);
	memset(&sampled, 0, sizeof(sampled));
	sampled.length = 1U;
	sampled.bytes[0] = '7';
	CHECK(yt_b05d_process_key(&sampled, &key_state));
	sampled.bytes[0] = '\r';
	CHECK(yt_b05d_process_key(&sampled, &key_state)
	    && queue_position == 0U && queue_length == 2U
	    && memcmp(queue, "7\r", 2U) == 0);
	current.foreground = 1.0f;
	pager.foreground = 1;
	pager.line_count = 16.0f;
	pager.nonstop = 1.0f;
	pager.newline_flag = 0.0f;
	memcpy(pager.key, "NS", 3U);
	memcpy(accumulator, "report", 7U);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(pager.line_count == 0.0f && pager.nonstop == 0.0f
	    && pager.newline_flag == 0.0f && pager.key[0] == '\0'
	    && pager.foreground == 1 && accumulator[0] == '\0'
	    && queue_position == 0U && queue_length == 2U
	    && memcmp(queue, "7\r", 2U) == 0
	    && current.foreground == 1.0f
	    && current.bold == 0.0f && current.blink == 0.0f);
	CHECK(yt_input_ab36_queue_pop(queue, sizeof(queue), &queue_position,
	    &queue_length, &selected)
	    && selected.length == 1U && selected.bytes[0] == '7'
	    && queue_position == 1U && queue_length == 2U);
	CHECK(yt_input_ab36_queue_pop(queue, sizeof(queue), &queue_position,
	    &queue_length, &selected)
	    && selected.length == 1U && selected.bytes[0] == '\r'
	    && queue_position == 0U && queue_length == 0U);
}

static void
test_computer_port_report_earth_cycle_presentation(void)
{
	static const uint8_t plain[] =
	    "\r\nEnter sector number port is in -=> 1\r\n"
	    "\r\nCommerce report for Earth: 07-25-2026 12:34:56\n\r"
	    "\r\n----------------------*--------*------------\n\r"
	    "         ITEM         *  COST  * CAN AFFORD\n\r"
	    "----------------------*--------*------------\n\r"
	    "[1] Cloak Energy      * 1000   * 12\n\r"
	    "[2] Cargo Holds       * 250    * 49\n\r"
	    "[3] Fighters          * 50     * 246\n\r"
	    "[4] Play Lottery      * 5      * 2469\n\r"
	    "[5] Danger Scanner    * 500000 * 0\n\r"
	    "[6] Anti-Cloak Device * 1E+09  * 0\n\r"
	    "[7] Ground Forces     * 200    * 61\n\r"
	    "[8] Shield Power      * 50     * 246\n\r"
	    "[9] Hire Spies (Each) * 1E+09  * 0\n\r"
	    "----------------------*--------*------------\n\r"
	    "\r\nTime:15:00  Computer command (?=help)? ";
	static const uint8_t ansi[] =
	    "\r\nEnter sector number port is in -=> 1\r\n"
	    "\x1b[0;33;40m\r\n"
	    "Commerce report for Earth: 07-25-2026 12:34:56\n\r"
	    "\r\n----------------------*--------*------------\n\r"
	    "         ITEM         *  COST  * CAN AFFORD\n\r"
	    "----------------------*--------*------------\n\r"
	    "[1] Cloak Energy      * 1000   * 12\n\r"
	    "[2] Cargo Holds       * 250    * 49\n\r"
	    "[3] Fighters          * 50     * 246\n\r"
	    "[4] Play Lottery      * 5      * 2469\n\r"
	    "[5] Danger Scanner    * 500000 * 0\n\r"
	    "[6] Anti-Cloak Device * 1E+09  * 0\n\r"
	    "[7] Ground Forces     * 200    * 61\n\r"
	    "[8] Shield Power      * 50     * 246\n\r"
	    "[9] Hire Spies (Each) * 1E+09  * 0\n\r"
	    "----------------------*--------*------------\n\r"
	    "\r\n\x1b[0;31;40m"
	    "Time:15:00  Computer command (?=help)? ";
	static const uint8_t mode_two[] =
	    "\r\n\r\n\r\n\r\n"
	    "[1] Cloak Energy      * 1000   "
	    "[2] Cargo Holds       * 250    "
	    "[3] Fighters          * 50     "
	    "[4] Play Lottery      * 5      "
	    "[5] Danger Scanner    * 500000 "
	    "[6] Anti-Cloak Device * 1E+09  "
	    "[7] Ground Forces     * 200    "
	    "[8] Shield Power      * 50     "
	    "[9] Hire Spies (Each) * 1E+09  \r\n";
	static const uint8_t prompt[] =
	    "Enter sector number port is in -=> ";
	static const uint8_t computer_prompt[] =
	    "Time:15:00  Computer command (?=help)? ";
	static const struct {
		bool ansi;
		float mode;
		const uint8_t *expected;
		size_t expected_length;
	} cases[] = {
		{false, 0.0f, plain, sizeof(plain) - 1U},
		{true, 0.0f, ansi, sizeof(ansi) - 1U},
		{true, 1.0f, NULL, 0U},
		{true, 2.0f, mode_two, sizeof(mode_two) - 1U},
	};
	struct yt_present_state current;
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	char accumulator[80];
	size_t pass;

	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		current = state(cases[pass].ansi);
		current.sound.mode = cases[pass].mode;
		current.foreground = 1.0f;
		current.cached_foreground = cases[pass].ansi ? 1.0f : 0.0f;
		memset(&pager, 0, sizeof(pager));
		pager.foreground = 1;
		memset(&capture, 0, sizeof(capture));
		memset(accumulator, 0, sizeof(accumulator));
		CHECK(yt_present_line(NULL, 0, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		pager.newline_flag = 1.0f;
		pager_fixture_b05d(&pager, &current, prompt,
		    sizeof(prompt) - 1U, &capture);
		yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
		CHECK(yt_present_editor_echo((const uint8_t *)"1", 1U,
		    (const uint8_t *)"1", 1U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		CHECK(yt_present_line(NULL, 0, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		earth_report_fixture(&capture, &pager, &current);
		CHECK(yt_present_line(NULL, 0, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		current.foreground = 1.0f;
		pager.foreground = 1;
		pager.newline_flag = 1.0f;
		pager_fixture_b05d(&pager, &current, computer_prompt,
		    sizeof(computer_prompt) - 1U, &capture);
		yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
		CHECK(capture.remote_length == cases[pass].expected_length);
		CHECK(cases[pass].expected_length == 0U
		    || capture.remote_length != cases[pass].expected_length
		    || memcmp(capture.remote, cases[pass].expected,
		    cases[pass].expected_length) == 0);
		CHECK(current.foreground == 1.0f
		    && current.background == 0.0f
		    && current.cached_foreground
		    == (cases[pass].ansi ? 1.0f : 0.0f)
		    && pager.foreground == 1
		    && pager.line_count == 0.0f
		    && pager.newline_flag == 0.0f
		    && accumulator[0] == '\0');
	}
	CHECK(sizeof(plain) - 1U == 650U && sizeof(ansi) - 1U == 670U
	    && sizeof(mode_two) - 1U == 289U);
}

static void
test_computer_port_report_earth_failure_presentation(void)
{
	static const uint8_t earth_port_get[] =
	    "\r\nEnter sector number port is in -=> 1\r\n";
	static const uint8_t owner_player_get[] =
	    "\r\nEnter sector number port is in -=> 1\r\n"
	    "\x1b[0;33;40m\r\n"
	    "Commerce report for Earth: 07-25-2026 12:34:56\n\r";
	static const uint8_t current_player_get[] =
	    "\r\nEnter sector number port is in -=> 1\r\n"
	    "\x1b[0;33;40m\r\n"
	    "Commerce report for Earth: 07-25-2026 12:34:56\n\r"
	    "\r\n";
	static const uint8_t prompt[] =
	    "Enter sector number port is in -=> ";
	static const uint8_t title[] =
	    "Commerce report for Earth: 07-25-2026 12:34:56";
	enum earth_failure_stage {
		EARTH_FAILURE_PORT_GET,
		EARTH_FAILURE_OWNER_GET,
		EARTH_FAILURE_CURRENT_GET,
	};
	static const struct {
		enum earth_failure_stage stage;
		const uint8_t *expected;
		size_t expected_length;
		float expected_line_count;
	} cases[] = {
		{EARTH_FAILURE_PORT_GET, earth_port_get,
		    sizeof(earth_port_get) - 1U, 0.0f},
		{EARTH_FAILURE_OWNER_GET, owner_player_get,
		    sizeof(owner_player_get) - 1U, 1.0f},
		{EARTH_FAILURE_CURRENT_GET, current_player_get,
		    sizeof(current_player_get) - 1U, 1.0f},
	};
	struct yt_present_state current;
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	char accumulator[80];
	size_t pass;

	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		current = state(true);
		current.foreground = 1.0f;
		current.cached_foreground = 1.0f;
		memset(&pager, 0, sizeof(pager));
		pager.foreground = 1;
		memset(&capture, 0, sizeof(capture));
		memset(accumulator, 0, sizeof(accumulator));
		CHECK(yt_present_line(NULL, 0U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		pager.newline_flag = 1.0f;
		pager_fixture_b05d(&pager, &current, prompt,
		    sizeof(prompt) - 1U, &capture);
		yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
		CHECK(yt_present_editor_echo((const uint8_t *)"1", 1U,
		    (const uint8_t *)"1", 1U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		CHECK(yt_present_line(NULL, 0U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		if (cases[pass].stage != EARTH_FAILURE_PORT_GET) {
			pager.line_count = 0.0f;
			current.foreground = 3.0f;
			pager.foreground = 3;
			CHECK(yt_present_line(NULL, 0U, &current, &result)
			    == YT_PRESENT_OK);
			pager_capture_result(&capture, &result);
			pager_fixture_b05d(&pager, &current, title,
			    sizeof(title) - 1U, &capture);
		}
		if (cases[pass].stage == EARTH_FAILURE_CURRENT_GET) {
			CHECK(yt_present_line(NULL, 0U, &current, &result)
			    == YT_PRESENT_OK);
			pager_capture_result(&capture, &result);
		}
		CHECK(capture.remote_length == cases[pass].expected_length
		    && memcmp(capture.remote, cases[pass].expected,
		    cases[pass].expected_length) == 0
		    && pager.line_count == cases[pass].expected_line_count);
	}
	CHECK(sizeof(earth_port_get) - 1U == 40U
	    && sizeof(owner_player_get) - 1U == 100U
	    && sizeof(current_player_get) - 1U == 102U);
}

static void
test_computer_port_report_ordinary_failure_presentation(void)
{
	static const uint8_t pre_report[] =
	    "\r\nEnter sector number port is in -=> 2\r\n";
	static const uint8_t after_owner[] =
	    "\r\nEnter sector number port is in -=> 2\r\n"
	    "\r\nThis port is owned by: YOU, Credits: 1234.5\r\n";
	static const uint8_t prompt[] =
	    "Enter sector number port is in -=> ";
	static const uint8_t owner[] =
	    "This port is owned by: YOU, Credits: 1234.5";
	enum ordinary_failure_stage {
		ORDINARY_FAILURE_UPDATE_SECTOR_GET,
		ORDINARY_FAILURE_UPDATE_PORT_GET,
		ORDINARY_FAILURE_UPDATE_ARITHMETIC,
		ORDINARY_FAILURE_UPDATE_PORT_PUT,
		ORDINARY_FAILURE_OWNER_GET,
		ORDINARY_FAILURE_CURRENT_GET,
		ORDINARY_FAILURE_FINAL_PORT_GET,
	};
	static const struct {
		enum ordinary_failure_stage stage;
		const uint8_t *expected;
		size_t expected_length;
	} cases[] = {
		{ORDINARY_FAILURE_UPDATE_SECTOR_GET, pre_report,
		    sizeof(pre_report) - 1U},
		{ORDINARY_FAILURE_UPDATE_PORT_GET, pre_report,
		    sizeof(pre_report) - 1U},
		{ORDINARY_FAILURE_UPDATE_ARITHMETIC, pre_report,
		    sizeof(pre_report) - 1U},
		{ORDINARY_FAILURE_UPDATE_PORT_PUT, pre_report,
		    sizeof(pre_report) - 1U},
		{ORDINARY_FAILURE_OWNER_GET, pre_report,
		    sizeof(pre_report) - 1U},
		{ORDINARY_FAILURE_CURRENT_GET, after_owner,
		    sizeof(after_owner) - 1U},
		{ORDINARY_FAILURE_FINAL_PORT_GET, after_owner,
		    sizeof(after_owner) - 1U},
	};
	size_t pass;

	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		struct yt_present_state current = state(false);
		struct yt_present_result result;
		struct yt_pager_state pager;
		struct pager_capture capture;
		char accumulator[80] = "";

		current.foreground = 1.0f;
		memset(&pager, 0, sizeof(pager));
		pager.foreground = 1;
		memset(&capture, 0, sizeof(capture));
		CHECK(yt_present_line(NULL, 0U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		pager.newline_flag = 1.0f;
		pager_fixture_b05d(&pager, &current, prompt,
		    sizeof(prompt) - 1U, &capture);
		yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
		CHECK(yt_present_editor_echo((const uint8_t *)"2", 1U,
		    (const uint8_t *)"2", 1U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		CHECK(yt_present_line(NULL, 0U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);

		if (cases[pass].stage >= ORDINARY_FAILURE_CURRENT_GET) {
			pager.line_count = 0.0f;
			CHECK(yt_present_line(NULL, 0U, &current, &result)
			    == YT_PRESENT_OK);
			pager_capture_result(&capture, &result);
			CHECK(yt_present_line(owner, sizeof(owner) - 1U,
			    &current, &result) == YT_PRESENT_OK);
			pager_capture_result(&capture, &result);
		}
		CHECK(capture.remote_length == cases[pass].expected_length
		    && memcmp(capture.remote, cases[pass].expected,
		    cases[pass].expected_length) == 0
		    && pager.line_count == 0.0f);
	}
	CHECK(sizeof(pre_report) - 1U == 40U
	    && sizeof(after_owner) - 1U == 87U);
}

static void
test_computer_port_report_ordinary_cycle_presentation(void)
{
	static const uint8_t plain[] =
	    "\r\nEnter sector number port is in -=> 2\r\n"
	    "\r\nThis port is owned by: YOU, Credits: 1234.5\r\n"
	    "\r\nCommerce report for Argus: 07-25-2026 12:34:56\n\r"
	    "\r\n Items         Status      # units    in holds   Cost\n\r"
	    "=======       =========   =========   ========   ====\n\r"
	    "Ore..........  Buying          100        5.5 32    \r\n"
	    "Organics.....  Selling         200          6 8    \r\n"
	    "Equipment....  Buying          300       7.25 66    \r\n"
	    "\r\nTime:15:00  Computer command (?=help)? ";
	static const uint8_t ansi[] =
	    "\r\nEnter sector number port is in -=> 2\r\n"
	    "\r\nThis port is owned by: YOU, Credits: 1234.5\r\n"
	    "\r\nCommerce report for Argus: 07-25-2026 12:34:56\n\r"
	    "\r\n Items         Status      # units    in holds   Cost\n\r"
	    "\x1b[0;31;40;1m"
	    "=======       =========   =========   ========   ====\n\r"
	    "\x1b[0;33;40m"
	    "Ore..........  Buying          100        5.5 32    \r\n"
	    "\x1b[0;32;40m"
	    "Organics.....  Selling         200          6 8    \r\n"
	    "\x1b[0;33;40m"
	    "Equipment....  Buying          300       7.25 66    \r\n"
	    "\r\n\x1b[0;31;40m"
	    "Time:15:00  Computer command (?=help)? ";
	static const uint8_t mode_two[] =
	    "\r\n\r\n"
	    "\r\nThis port is owned by: YOU, Credits: 1234.5\r\n"
	    "\r\n\r\n"
	    "Ore..........  Buying          100        5.5 32    \r\n"
	    "Organics.....  Selling         200          6 8    \r\n"
	    "Equipment....  Buying          300       7.25 66    \r\n"
	    "\r\n";
	static const uint8_t sector_prompt[] =
	    "Enter sector number port is in -=> ";
	static const uint8_t owner[] =
	    "This port is owned by: YOU, Credits: 1234.5";
	static const uint8_t title[] =
	    "Commerce report for Argus: 07-25-2026 12:34:56";
	static const uint8_t header[] =
	    " Items         Status      # units    in holds   Cost";
	static const uint8_t rule[] =
	    "=======       =========   =========   ========   ====";
	static const uint8_t item_prefix[3][23] = {
		"Ore..........  Buying ",
		"Organics.....  Selling",
		"Equipment....  Buying ",
	};
	static const uint8_t capacity[3][13] = {
		"         100", "         200", "         300",
	};
	static const uint8_t hold[3][12] = {
		"        5.5", "          6", "       7.25",
	};
	static const uint8_t price[3][8] = {
		" 32    ", " 8    ", " 66    ",
	};
	static const uint8_t computer_prompt[] =
	    "Time:15:00  Computer command (?=help)? ";
	static const struct {
		bool ansi;
		float mode;
		const uint8_t *expected;
		size_t expected_length;
	} cases[] = {
		{false, 0.0f, plain, sizeof(plain) - 1U},
		{true, 0.0f, ansi, sizeof(ansi) - 1U},
		{true, 1.0f, NULL, 0U},
		{true, 2.0f, mode_two, sizeof(mode_two) - 1U},
	};
	struct yt_present_state current;
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	char accumulator[80];
	size_t index;
	size_t pass;

	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		current = state(cases[pass].ansi);
		current.sound.mode = cases[pass].mode;
		current.foreground = 1.0f;
		current.cached_foreground = cases[pass].ansi ? 1.0f : 0.0f;
		memset(&pager, 0, sizeof(pager));
		pager.foreground = 1;
		memset(&capture, 0, sizeof(capture));
		memset(accumulator, 0, sizeof(accumulator));
		CHECK(yt_present_line(NULL, 0, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		pager.newline_flag = 1.0f;
		pager_fixture_b05d(&pager, &current, sector_prompt,
		    sizeof(sector_prompt) - 1U, &capture);
		yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
		CHECK(yt_present_editor_echo((const uint8_t *)"2", 1U,
		    (const uint8_t *)"2", 1U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		CHECK(yt_present_line(NULL, 0, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);

		pager.line_count = 0.0f;
		CHECK(yt_present_line(NULL, 0, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		pager_capture_line(&capture, &current, owner, sizeof(owner) - 1U);
		CHECK(yt_present_line(NULL, 0, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		pager_fixture_b05d(&pager, &current, title,
		    sizeof(title) - 1U, &capture);
		CHECK(yt_present_line(NULL, 0, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		pager_fixture_b05d(&pager, &current, header,
		    sizeof(header) - 1U, &capture);
		current.bold = 1.0f;
		pager_fixture_b05d(&pager, &current, rule,
		    sizeof(rule) - 1U, &capture);
		for (index = 0U; index < 3U; ++index) {
			current.foreground = index == 1U ? 2.0f : 3.0f;
			pager.foreground = index == 1U ? 2 : 3;
			CHECK(yt_present_character(item_prefix[index], 22U,
			    &current, &result) == YT_PRESENT_OK);
			pager_capture_result(&capture, &result);
			CHECK(yt_present_character(capacity[index], 12U,
			    &current, &result) == YT_PRESENT_OK);
			pager_capture_result(&capture, &result);
			CHECK(yt_present_character(hold[index], 11U,
			    &current, &result) == YT_PRESENT_OK);
			pager_capture_result(&capture, &result);
			pager_capture_line(&capture, &current, price[index],
			    strlen((const char *)price[index]));
		}
		current.foreground = 3.0f;
		pager.foreground = 3;
		CHECK(yt_present_line(NULL, 0, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		current.foreground = 1.0f;
		pager.foreground = 1;
		pager.newline_flag = 1.0f;
		pager_fixture_b05d(&pager, &current, computer_prompt,
		    sizeof(computer_prompt) - 1U, &capture);
		yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
		CHECK(capture.remote_length == cases[pass].expected_length);
		CHECK(cases[pass].expected_length == 0U
		    || capture.remote_length != cases[pass].expected_length
		    || memcmp(capture.remote, cases[pass].expected,
		    cases[pass].expected_length) == 0);
		CHECK(current.foreground == 1.0f
		    && current.background == 0.0f
		    && current.cached_foreground
		    == (cases[pass].ansi ? 1.0f : 0.0f)
		    && pager.foreground == 1
		    && pager.line_count == 0.0f
		    && pager.newline_flag == 0.0f
		    && accumulator[0] == '\0');
	}
	CHECK(sizeof(plain) - 1U == 451U && sizeof(ansi) - 1U == 503U
	    && sizeof(mode_two) - 1U == 218U);
}

static void
test_computer_port_report_low_time_cycle_presentation(void)
{
	static const uint8_t plain[] =
	    "\r\n\r\n\aTime Left:5.9:00\r\n\r\n"
	    "Enter sector number port is in -=> 2\r\n"
	    "\r\nThis port is owned by: YOU, Credits: 1234.5\r\n"
	    "\r\nCommerce report for Argus: 07-25-2026 12:34:56\n\r"
	    "\r\n Items         Status      # units    in holds   Cost\n\r"
	    "=======       =========   =========   ========   ====\n\r"
	    "Ore..........  Buying          100        5.5 32    \r\n"
	    "Organics.....  Selling         200          6 8    \r\n"
	    "Equipment....  Buying          300       7.25 66    \r\n"
	    "\r\n\r\n\aTime Left:5.9:00\r\n\r\n"
	    "Time:5.9:00Computer command (?=help)? ";
	static const uint8_t ansi[] =
	    "\r\n\r\n\a\x1b[0;35;40;5;1mTime Left:5.9:00\r\n"
	    "\x1b[0;35;40m\r\n"
	    "Enter sector number port is in -=> 2\r\n"
	    "\r\nThis port is owned by: YOU, Credits: 1234.5\r\n"
	    "\r\nCommerce report for Argus: 07-25-2026 12:34:56\n\r"
	    "\r\n Items         Status      # units    in holds   Cost\n\r"
	    "\x1b[0;35;40;1m"
	    "=======       =========   =========   ========   ====\n\r"
	    "\x1b[0;33;40m"
	    "Ore..........  Buying          100        5.5 32    \r\n"
	    "\x1b[0;32;40m"
	    "Organics.....  Selling         200          6 8    \r\n"
	    "\x1b[0;33;40m"
	    "Equipment....  Buying          300       7.25 66    \r\n"
	    "\r\n\x1b[0;31;40m"
	    "\r\n\a\x1b[0;35;40;5;1mTime Left:5.9:00\r\n"
	    "\x1b[0;35;40m\r\n"
	    "Time:5.9:00Computer command (?=help)? ";
	static const uint8_t sector_prompt[] =
	    "Enter sector number port is in -=> ";
	static const uint8_t owner[] =
	    "This port is owned by: YOU, Credits: 1234.5";
	static const uint8_t title[] =
	    "Commerce report for Argus: 07-25-2026 12:34:56";
	static const uint8_t header[] =
	    " Items         Status      # units    in holds   Cost";
	static const uint8_t rule[] =
	    "=======       =========   =========   ========   ====";
	static const uint8_t item_prefix[3][23] = {
		"Ore..........  Buying ",
		"Organics.....  Selling",
		"Equipment....  Buying ",
	};
	static const uint8_t capacity[3][13] = {
		"         100", "         200", "         300",
	};
	static const uint8_t hold[3][12] = {
		"        5.5", "          6", "       7.25",
	};
	static const uint8_t price[3][8] = {
		" 32    ", " 8    ", " 66    ",
	};
	static const uint8_t computer_prompt[] =
	    "Time:5.9:00Computer command (?=help)? ";
	static const uint8_t time_text[] = "5.9:00";
	static const struct {
		bool ansi;
		const uint8_t *expected;
		size_t expected_length;
	} cases[] = {
		{false, plain, sizeof(plain) - 1U},
		{true, ansi, sizeof(ansi) - 1U},
	};
	struct yt_present_state current;
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	char accumulator[80];
	float remembered;
	bool warned;
	size_t index;
	size_t pass;

	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		current = state(cases[pass].ansi);
		current.foreground = 1.0f;
		current.cached_foreground = cases[pass].ansi ? 1.0f : 0.0f;
		memset(&pager, 0, sizeof(pager));
		pager.foreground = 1;
		memset(&capture, 0, sizeof(capture));
		memset(accumulator, 0, sizeof(accumulator));
		remembered = 6.0f;
		CHECK(yt_present_line(NULL, 0U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		current.foreground = 1.0f;
		pager.foreground = 1;
		CHECK(yt_present_low_time(time_text, sizeof(time_text) - 1U,
		    &remembered, &current, &result, &warned) == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		CHECK(warned && remembered == (float)5.9);
		pager.newline_flag = 1.0f;
		pager_fixture_b05d(&pager, &current, sector_prompt,
		    sizeof(sector_prompt) - 1U, &capture);
		yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
		CHECK(yt_present_editor_echo((const uint8_t *)"2", 1U,
		    (const uint8_t *)"2", 1U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		CHECK(yt_present_line(NULL, 0U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		pager.line_count = 0.0f;
		CHECK(yt_present_line(NULL, 0U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		pager_capture_line(&capture, &current, owner,
		    sizeof(owner) - 1U);
		CHECK(yt_present_line(NULL, 0U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		pager_fixture_b05d(&pager, &current, title,
		    sizeof(title) - 1U, &capture);
		CHECK(yt_present_line(NULL, 0U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		pager_fixture_b05d(&pager, &current, header,
		    sizeof(header) - 1U, &capture);
		current.bold = 1.0f;
		pager_fixture_b05d(&pager, &current, rule,
		    sizeof(rule) - 1U, &capture);
		for (index = 0U; index < 3U; ++index) {
			current.foreground = index == 1U ? 2.0f : 3.0f;
			pager.foreground = index == 1U ? 2 : 3;
			CHECK(yt_present_character(item_prefix[index], 22U,
			    &current, &result) == YT_PRESENT_OK);
			pager_capture_result(&capture, &result);
			CHECK(yt_present_character(capacity[index], 12U,
			    &current, &result) == YT_PRESENT_OK);
			pager_capture_result(&capture, &result);
			CHECK(yt_present_character(hold[index], 11U,
			    &current, &result) == YT_PRESENT_OK);
			pager_capture_result(&capture, &result);
			pager_capture_line(&capture, &current, price[index],
			    strlen((const char *)price[index]));
		}
		current.foreground = 3.0f;
		pager.foreground = 3;
		CHECK(yt_present_line(NULL, 0U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		current.foreground = 1.0f;
		pager.foreground = 1;
		CHECK(yt_present_low_time(time_text, sizeof(time_text) - 1U,
		    &remembered, &current, &result, &warned) == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		CHECK(warned && remembered == (float)5.9);
		pager.newline_flag = 1.0f;
		pager_fixture_b05d(&pager, &current, computer_prompt,
		    sizeof(computer_prompt) - 1U, &capture);
		yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
		CHECK(capture.remote_length == cases[pass].expected_length
		    && memcmp(capture.remote, cases[pass].expected,
		    cases[pass].expected_length) == 0
		    && current.foreground == 5.0f
		    && pager.foreground == 1
		    && pager.line_count == 0.0f
		    && pager.newline_flag == 0.0f
		    && accumulator[0] == '\0');
	}
	CHECK(sizeof(plain) - 1U == 496U && sizeof(ansi) - 1U == 596U);
}

static void
computer_port_report_render_ordinary_fixture(
    struct yt_present_state *current, struct yt_pager_state *pager,
    struct pager_capture *capture)
{
	static const uint8_t owner[] =
	    "This port is owned by: YOU, Credits: 1234.5";
	static const uint8_t title[] =
	    "Commerce report for Argus: 07-25-2026 12:34:56";
	static const uint8_t header[] =
	    " Items         Status      # units    in holds   Cost";
	static const uint8_t rule[] =
	    "=======       =========   =========   ========   ====";
	static const uint8_t item_prefix[3][23] = {
		"Ore..........  Buying ",
		"Organics.....  Selling",
		"Equipment....  Buying ",
	};
	static const uint8_t capacity[3][13] = {
		"         100", "         200", "         300",
	};
	static const uint8_t hold[3][12] = {
		"        5.5", "          6", "       7.25",
	};
	static const uint8_t price[3][8] = {
		" 32    ", " 8    ", " 66    ",
	};
	struct yt_present_result result;
	size_t index;

	pager->line_count = 0.0f;
	CHECK(yt_present_line(NULL, 0U, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager_capture_line(capture, current, owner, sizeof(owner) - 1U);
	CHECK(yt_present_line(NULL, 0U, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager_fixture_b05d(pager, current, title, sizeof(title) - 1U, capture);
	CHECK(yt_present_line(NULL, 0U, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager_fixture_b05d(pager, current, header, sizeof(header) - 1U,
	    capture);
	current->bold = 1.0f;
	pager_fixture_b05d(pager, current, rule, sizeof(rule) - 1U, capture);
	for (index = 0U; index < 3U; ++index) {
		current->foreground = index == 1U ? 2.0f : 3.0f;
		pager->foreground = index == 1U ? 2 : 3;
		CHECK(yt_present_character(item_prefix[index], 22U, current,
		    &result) == YT_PRESENT_OK);
		pager_capture_result(capture, &result);
		CHECK(yt_present_character(capacity[index], 12U, current,
		    &result) == YT_PRESENT_OK);
		pager_capture_result(capture, &result);
		CHECK(yt_present_character(hold[index], 11U, current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(capture, &result);
		pager_capture_line(capture, current, price[index],
		    strlen((const char *)price[index]));
	}
	current->foreground = 3.0f;
	pager->foreground = 3;
}

static void
test_computer_port_report_low_time_retry_cycle_presentation(void)
{
	static const uint8_t plain[] =
	    "\r\n\r\n\aTime Left:5.9:00\r\n\r\n"
	    "Enter sector number port is in -=> 2005\r\n"
	    "\r\nInvalid sector number! Range is 1 - 2004\n\r"
	    "\r\n\r\n\aTime Left:5.9:00\r\n\r\n"
	    "Enter sector number port is in -=> 2\r\n"
	    "\r\nThis port is owned by: YOU, Credits: 1234.5\r\n"
	    "\r\nCommerce report for Argus: 07-25-2026 12:34:56\n\r"
	    "\r\n Items         Status      # units    in holds   Cost\n\r"
	    "=======       =========   =========   ========   ====\n\r"
	    "Ore..........  Buying          100        5.5 32    \r\n"
	    "Organics.....  Selling         200          6 8    \r\n"
	    "Equipment....  Buying          300       7.25 66    \r\n"
	    "\r\n\r\n\aTime Left:5.9:00\r\n\r\n"
	    "Time:5.9:00Computer command (?=help)? ";
	static const uint8_t ansi[] =
	    "\r\n\r\n\a\x1b[0;35;40;5;1mTime Left:5.9:00\r\n"
	    "\x1b[0;35;40m\r\n"
	    "Enter sector number port is in -=> 2005\r\n"
	    "\r\n\x1b[0;35;40;5;1m"
	    "Invalid sector number! Range is 1 - 2004\n\r"
	    "\x1b[0;35;40m\r\n\r\n\a"
	    "\x1b[0;35;40;5;1mTime Left:5.9:00\r\n"
	    "\x1b[0;35;40m\r\n"
	    "Enter sector number port is in -=> 2\r\n"
	    "\r\nThis port is owned by: YOU, Credits: 1234.5\r\n"
	    "\r\nCommerce report for Argus: 07-25-2026 12:34:56\n\r"
	    "\r\n Items         Status      # units    in holds   Cost\n\r"
	    "\x1b[0;35;40;1m"
	    "=======       =========   =========   ========   ====\n\r"
	    "\x1b[0;33;40m"
	    "Ore..........  Buying          100        5.5 32    \r\n"
	    "\x1b[0;32;40m"
	    "Organics.....  Selling         200          6 8    \r\n"
	    "\x1b[0;33;40m"
	    "Equipment....  Buying          300       7.25 66    \r\n"
	    "\r\n\x1b[0;31;40m"
	    "\r\n\a\x1b[0;35;40;5;1mTime Left:5.9:00\r\n"
	    "\x1b[0;35;40m\r\n"
	    "Time:5.9:00Computer command (?=help)? ";
	static const uint8_t prompt[] =
	    "Enter sector number port is in -=> ";
	static const uint8_t invalid[] =
	    "Invalid sector number! Range is 1 - 2004";
	static const uint8_t computer_prompt[] =
	    "Time:5.9:00Computer command (?=help)? ";
	static const uint8_t time_text[] = "5.9:00";
	static const struct {
		bool ansi;
		const uint8_t *expected;
		size_t expected_length;
	} cases[] = {
		{false, plain, sizeof(plain) - 1U},
		{true, ansi, sizeof(ansi) - 1U},
	};
	struct yt_present_state current;
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	char accumulator[80];
	float remembered;
	bool warned;
	size_t pass;

	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		current = state(cases[pass].ansi);
		current.foreground = 1.0f;
		current.cached_foreground = cases[pass].ansi ? 1.0f : 0.0f;
		memset(&pager, 0, sizeof(pager));
		pager.foreground = 1;
		memset(&capture, 0, sizeof(capture));
		memset(accumulator, 0, sizeof(accumulator));
		remembered = 6.0f;

		CHECK(yt_present_line(NULL, 0U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		CHECK(yt_present_low_time(time_text, sizeof(time_text) - 1U,
		    &remembered, &current, &result, &warned) == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		CHECK(warned);
		pager.newline_flag = 1.0f;
		pager_fixture_b05d(&pager, &current, prompt,
		    sizeof(prompt) - 1U, &capture);
		yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
		CHECK(yt_present_editor_echo((const uint8_t *)"2005", 4U,
		    (const uint8_t *)"2005", 4U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		CHECK(yt_present_line(NULL, 0U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		CHECK(yt_present_line(NULL, 0U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		current.bold = 1.0f;
		current.blink = 1.0f;
		pager_fixture_b05d(&pager, &current, invalid,
		    sizeof(invalid) - 1U, &capture);

		CHECK(yt_present_line(NULL, 0U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		CHECK(yt_present_low_time(time_text, sizeof(time_text) - 1U,
		    &remembered, &current, &result, &warned) == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		CHECK(warned);
		pager.newline_flag = 1.0f;
		pager_fixture_b05d(&pager, &current, prompt,
		    sizeof(prompt) - 1U, &capture);
		yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
		CHECK(yt_present_editor_echo((const uint8_t *)"2", 1U,
		    (const uint8_t *)"2", 1U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		CHECK(yt_present_line(NULL, 0U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		computer_port_report_render_ordinary_fixture(&current, &pager,
		    &capture);

		CHECK(yt_present_line(NULL, 0U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		current.foreground = 1.0f;
		pager.foreground = 1;
		CHECK(yt_present_low_time(time_text, sizeof(time_text) - 1U,
		    &remembered, &current, &result, &warned) == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		CHECK(warned && remembered == (float)5.9);
		pager.newline_flag = 1.0f;
		pager_fixture_b05d(&pager, &current, computer_prompt,
		    sizeof(computer_prompt) - 1U, &capture);
		yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
		CHECK(capture.remote_length == cases[pass].expected_length
		    && memcmp(capture.remote, cases[pass].expected,
		    cases[pass].expected_length) == 0
		    && current.foreground == 5.0f
		    && pager.foreground == 1
		    && pager.line_count == 0.0f
		    && pager.newline_flag == 0.0f
		    && accumulator[0] == '\0');
	}
	CHECK(sizeof(plain) - 1U == 606U && sizeof(ansi) - 1U == 754U);
}

static void
test_computer_planet_report_front_presentation(void)
{
	static const uint8_t prompt[] =
	    "What sector number is the planet in? ";
	static const uint8_t range[] =
	    "Valid sector numbers are from 1 to 2004.";
	static const uint8_t limited[] =
	    "Planet: New Terra -*- Ground Forces: 40";
	static const uint8_t no_turns[] =
	    "Sorry but you have no turns left.";
	static const uint8_t no_turns_expected[] =
	    "\r\n\x1b[0;31;40;5;1mSorry but you have no turns left.\n\r";
	static const uint8_t expected[] =
	    "What sector number is the planet in? 2005\r\n"
	    "\r\n"
	    "\x1b[0;31;40;5;1m"
	    "Valid sector numbers are from 1 to 2004.\n\r"
	    "\x1b[0;31;40m"
	    "What sector number is the planet in? 7\r\n"
	    "\r\nPlanet: New Terra -*- Ground Forces: 40\n\r";
	struct yt_present_state current = state(true);
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	char accumulator[80] = "";

	current.foreground = 1.0f;
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 1;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.bold = 1.0f;
	current.blink = 1.0f;
	pager_fixture_b05d(&pager, &current, no_turns,
	    sizeof(no_turns) - 1U, &capture);
	CHECK(capture.remote_length == sizeof(no_turns_expected) - 1U
	    && memcmp(capture.remote, no_turns_expected,
	    sizeof(no_turns_expected) - 1U) == 0);

	current = state(true);
	current.foreground = 1.0f;
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 1;
	memset(&capture, 0, sizeof(capture));
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, prompt, sizeof(prompt) - 1U,
	    &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo((const uint8_t *)"2005", 4,
	    (const uint8_t *)"2005", 4, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.bold = 1.0f;
	current.blink = 1.0f;
	pager_fixture_b05d(&pager, &current, range, sizeof(range) - 1U,
	    &capture);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, prompt, sizeof(prompt) - 1U,
	    &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo((const uint8_t *)"7", 1,
	    (const uint8_t *)"7", 1, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current, limited, sizeof(limited) - 1U,
	    &capture);
	CHECK(capture.remote_length == sizeof(expected) - 1U
	    && memcmp(capture.remote, expected, sizeof(expected) - 1U) == 0);
	CHECK(pager.line_count == 1.0f && pager.newline_flag == 0.0f);
}

static void
test_computer_planet_inventory_presentation(void)
{
	static const uint8_t prompt[] =
	    "What sector number is the planet in? ";
	static const uint8_t title[] = "Planet: New Terra";
	static const uint8_t header[] =
	    " Item           Production     Amount    In Holds";
	static const uint8_t rule[] =
	    "=============  ============   ========  ==========";
	static const char *labels[9] = {
		"Ore..........", "Organics.....", "Equipment....",
		"Fighters.....", "Missiles.....", "Mines........",
		"Credits......", "Forces.......", "Plasma bolts."
	};
	static const char *production[9] = {
		" 11", " 22", " 33", " 68", " 0", " 0", " 9", " 2", " 0"
	};
	static const char *amount[9] = {
		" 101", " 202", " 303", " 404", " 5", " 6", " 1000", " 250", " 9"
	};
	static const char *held[9] = {
		" 10", " 20", " 5", " 7", " 2", " 3", " 12345", " 8", " 4"
	};
	static const uint8_t expected[] =
	    "What sector number is the planet in? 7\r\n"
	    "\r\nPlanet: New Terra\n\r"
	    "\r\n Item           Production     Amount    In Holds\n\r"
	    "=============  ============   ========  ==========\n\r"
	    "Ore..........           11        101          10\r\n"
	    "Organics.....           22        202          20\r\n"
	    "Equipment....           33        303           5\r\n"
	    "Fighters.....           68        404           7\r\n"
	    "Missiles.....            0          5           2\r\n"
	    "Mines........            0          6           3\r\n"
	    "Credits......            9       1000       12345\r\n"
	    "Forces.......            2        250           8\r\n"
	    "Plasma bolts.            0          9           4\r\n";
	struct yt_present_state current = state(false);
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	char accumulator[80] = "";
	int index;

	current.foreground = 1.0f;
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 1;
	memset(&capture, 0, sizeof(capture));
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, prompt, sizeof(prompt) - 1U,
	    &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo((const uint8_t *)"7", 1,
	    (const uint8_t *)"7", 1, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current, title, sizeof(title) - 1U,
	    &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current, header, sizeof(header) - 1U,
	    &capture);
	pager_fixture_b05d(&pager, &current, rule, sizeof(rule) - 1U,
	    &capture);
	for (index = 0; index < 9; ++index) {
		CHECK(yt_present_character((const uint8_t *)labels[index],
		    strlen(labels[index]), &current, &result) == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		CHECK(yt_present_right_aligned((const uint8_t *)production[index],
		    strlen(production[index]), 13.0f, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		CHECK(yt_present_right_aligned((const uint8_t *)amount[index],
		    strlen(amount[index]), 11.0f, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		CHECK(yt_present_right_aligned((const uint8_t *)held[index],
		    strlen(held[index]), 12.0f, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
	}
	CHECK(sizeof(expected) - 1U == 625U);
	CHECK(capture.remote_length == sizeof(expected) - 1U
	    && memcmp(capture.remote, expected, sizeof(expected) - 1U) == 0);
	CHECK(pager.line_count == 3.0f && pager.newline_flag == 0.0f);
}

static void
owned_planets_fixture_error(struct yt_error *error, const char *operation)
{
	if (error == NULL)
		return;
	error->status = YT_IO_ERROR;
	(void)snprintf(error->operation, sizeof(error->operation), "%s",
	    operation);
}

struct owned_planets_fixture {
	float links[5];
	uint32_t physical[4];
	struct yt_planet planets[4];
	size_t planet_count;
	unsigned events[32];
	size_t event_count;
	size_t calls;
	size_t fail_at;
	int colors[4];
	size_t color_count;
	float blink;
	size_t blink_calls;
	uint8_t rows[6][96];
	size_t row_lengths[6];
	bool row_bold[6];
	size_t row_count;
};

static bool
owned_planets_fixture_step(struct owned_planets_fixture *fixture,
    unsigned event, struct yt_error *error, const char *operation)
{
	CHECK(fixture->event_count < YT_ARRAY_LEN(fixture->events));
	if (fixture->event_count < YT_ARRAY_LEN(fixture->events))
		fixture->events[fixture->event_count++] = event;
	++fixture->calls;
	if (fixture->fail_at == fixture->calls) {
		owned_planets_fixture_error(error, operation);
		return false;
	}
	return true;
}

static bool
owned_planets_fixture_sector(void *context, int logical_sector,
    struct yt_sector *sector, struct yt_error *error)
{
	struct owned_planets_fixture *fixture = context;

	if (!owned_planets_fixture_step(fixture,
	    100U + (unsigned)logical_sector, error, "fixture sector"))
		return false;
	if (logical_sector < 1
	    || (size_t)logical_sector > YT_ARRAY_LEN(fixture->links)) {
		owned_planets_fixture_error(error, "fixture sector range");
		return false;
	}
	memset(sector, 0, sizeof(*sector));
	sector->planet = fixture->links[(size_t)logical_sector - 1U];
	return true;
}

static bool
owned_planets_fixture_planet(void *context, uint32_t physical_record,
    struct yt_planet *planet, struct yt_error *error)
{
	struct owned_planets_fixture *fixture = context;
	size_t index;

	if (!owned_planets_fixture_step(fixture,
	    1000U + (physical_record & 0x00ffffffU), error, "fixture planet"))
		return false;
	for (index = 0U; index < fixture->planet_count; ++index) {
		if (fixture->physical[index] == physical_record) {
			*planet = fixture->planets[index];
			return true;
		}
	}
	owned_planets_fixture_error(error, "fixture missing planet");
	return false;
}

static bool
owned_planets_fixture_present(void *context, const uint8_t *text,
    size_t length, bool bold, const char *operation,
    struct yt_error *error)
{
	struct owned_planets_fixture *fixture = context;
	unsigned event = text == NULL ? 10U
	    : length == strlen("Scanning...")
	    && memcmp(text, "Scanning...", length) == 0 ? 11U
	    : length == strlen("None found!")
	    && memcmp(text, "None found!", length) == 0 ? 13U : 12U;

	if (!owned_planets_fixture_step(fixture, event, error, operation))
		return false;
	CHECK(fixture->row_count < YT_ARRAY_LEN(fixture->rows)
	    && length <= sizeof(fixture->rows[0]));
	if (fixture->row_count < YT_ARRAY_LEN(fixture->rows)
	    && length <= sizeof(fixture->rows[0])) {
		if (length != 0U)
			memcpy(fixture->rows[fixture->row_count], text, length);
		fixture->row_lengths[fixture->row_count] = length;
		fixture->row_bold[fixture->row_count] = bold;
		++fixture->row_count;
	}
	return true;
}

static void
owned_planets_fixture_color(void *context, int foreground)
{
	struct owned_planets_fixture *fixture = context;

	CHECK(fixture->color_count < YT_ARRAY_LEN(fixture->colors));
	if (fixture->color_count < YT_ARRAY_LEN(fixture->colors))
		fixture->colors[fixture->color_count++] = foreground;
}

static void
owned_planets_fixture_blink(void *context, float blink)
{
	struct owned_planets_fixture *fixture = context;

	fixture->blink = blink;
	++fixture->blink_calls;
}

static void
owned_planets_fixture_init(struct owned_planets_fixture *fixture)
{
	memset(fixture, 0, sizeof(*fixture));
	fixture->links[1] = 1.0f;
	fixture->links[3] = 2.0f;
	fixture->links[4] = 1.0f;
	fixture->physical[0] = 3056U;
	fixture->physical[1] = 3057U;
	fixture->planet_count = 2U;
	yt_record_blank(&fixture->planets[0].record);
	yt_record_set_text(&fixture->planets[0].record,
	    (const uint8_t *)"Home", 4U);
	fixture->planets[0].owner = 2.0f;
	yt_record_blank(&fixture->planets[1].record);
	yt_record_set_text(&fixture->planets[1].record,
	    (const uint8_t *)"Other", 5U);
	fixture->planets[1].owner = 9.0f;
}

static void
test_owned_planets_transaction(void)
{
	static const struct yt_owned_planets_ops ops = {
		owned_planets_fixture_sector,
		owned_planets_fixture_planet,
		owned_planets_fixture_present,
		owned_planets_fixture_color,
		owned_planets_fixture_blink,
	};
	static const uint8_t prefix[] = "Planet: ";
	static const uint8_t infix[] = " Sector:";
	struct owned_planets_fixture success;
	struct owned_planets_fixture fixture;
	struct yt_owned_planets_state state;
	struct yt_error error;
	uint8_t expected[96];
	size_t expected_length;
	size_t fail_at;

	owned_planets_fixture_init(&success);
	memset(&state, 0, sizeof(state));
	state.maximum_sector = 5;
	state.planet_record_base = 3055.0f;
	state.current_player = 2.0f;
	yt_error_clear(&error);
	CHECK(yt_owned_planets_run(&state, &ops, &success, &error));
	CHECK(state.found && state.current_sector == 5
	    && state.current_link == 1.0f
	    && state.current_record_expression == 3056.0f
	    && state.current_planet_record == 3056U
	    && state.foreground == 3.0f && state.blink == 0.0f);
	CHECK(success.calls == 13U && success.event_count == 13U
	    && success.color_count == 2U && success.colors[0] == 2
	    && success.colors[1] == 3 && success.blink_calls == 0U
	    && success.row_count == 5U);
	expected_length = 0U;
	memcpy(expected + expected_length, prefix, sizeof(prefix) - 1U);
	expected_length += sizeof(prefix) - 1U;
	memcpy(expected + expected_length, success.planets[0].record.bytes,
	    YT_TEXT_FIELD_SIZE);
	expected_length += YT_TEXT_FIELD_SIZE;
	memcpy(expected + expected_length, infix, sizeof(infix) - 1U);
	expected_length += sizeof(infix) - 1U;
	memcpy(expected + expected_length, " 2", 2U);
	expected_length += 2U;
	CHECK(success.row_bold[3]
	    && success.row_lengths[3] == expected_length
	    && memcmp(success.rows[3], expected, expected_length) == 0);
	expected[expected_length - 1U] = '5';
	CHECK(success.row_bold[4]
	    && success.row_lengths[4] == expected_length
	    && memcmp(success.rows[4], expected, expected_length) == 0);

	/* Every fallible dependency retains precisely the successful prefix. */
	for (fail_at = 1U; fail_at <= success.calls; ++fail_at) {
		owned_planets_fixture_init(&fixture);
		fixture.fail_at = fail_at;
		memset(&state, 0, sizeof(state));
		state.maximum_sector = 5;
		state.planet_record_base = 3055.0f;
		state.current_player = 2.0f;
		yt_error_clear(&error);
		CHECK(!yt_owned_planets_run(&state, &ops, &fixture, &error));
		CHECK(error.status == YT_IO_ERROR && fixture.calls == fail_at
		    && fixture.event_count == fail_at
		    && memcmp(fixture.events, success.events,
		    fail_at * sizeof(fixture.events[0])) == 0);
	}

	/* No match sets blink only after the complete scan. */
	memset(&fixture, 0, sizeof(fixture));
	memset(&state, 0, sizeof(state));
	state.maximum_sector = 1;
	state.planet_record_base = 3055.0f;
	state.current_player = 2.0f;
	CHECK(yt_owned_planets_run(&state, &ops, &fixture, &error));
	CHECK(!state.found && state.blink == 1.0f
	    && fixture.blink_calls == 1U && fixture.blink == 1.0f
	    && fixture.row_count == 4U && fixture.row_bold[3]
	    && fixture.row_lengths[3] == strlen("None found!")
	    && memcmp(fixture.rows[3], "None found!",
	    fixture.row_lengths[3]) == 0);

	/* Raw SINGLE link addition feeds BRUN's 24-bit record conversion. */
	memset(&fixture, 0, sizeof(fixture));
	fixture.links[0] = 1.6f;
	fixture.physical[0] = 3056U;
	fixture.planet_count = 1U;
	yt_record_blank(&fixture.planets[0].record);
	fixture.planets[0].owner = 9.0f;
	memset(&state, 0, sizeof(state));
	state.maximum_sector = 1;
	state.planet_record_base = 3055.0f;
	state.current_player = 2.0f;
	CHECK(yt_owned_planets_run(&state, &ops, &fixture, &error));
	CHECK(state.current_record_expression == 3056.60009765625f
	    && state.current_planet_record == 3056U);

	memset(&fixture, 0, sizeof(fixture));
	fixture.links[0] = -3056.5f;
	fixture.physical[0] = 0x00fffffeU;
	fixture.planet_count = 1U;
	yt_record_blank(&fixture.planets[0].record);
	fixture.planets[0].owner = 9.0f;
	memset(&state, 0, sizeof(state));
	state.maximum_sector = 1;
	state.planet_record_base = 3055.0f;
	state.current_player = 2.0f;
	CHECK(yt_owned_planets_run(&state, &ops, &fixture, &error));
	CHECK(state.current_record_expression == -1.5f
	    && state.current_planet_record == 0x00fffffeU);

	memset(&fixture, 0, sizeof(fixture));
	fixture.links[0] = -3055.0f;
	memset(&state, 0, sizeof(state));
	state.maximum_sector = 1;
	state.planet_record_base = 3055.0f;
	state.current_player = 2.0f;
	yt_error_clear(&error);
	CHECK(!yt_owned_planets_run(&state, &ops, &fixture, &error));
	CHECK(error.status == YT_RANGE && state.current_planet_record == 0U
	    && fixture.calls == 4U && fixture.event_count == 4U);
}

static void
test_computer_finders_presentation(void)
{
	static const uint8_t computer_prompt[] =
	    "Time: 14:59  Computer command (?=help)? ";
	static const uint8_t searching[] = "Searching;";
	static const uint8_t amount[] = "Amount";
	static const uint8_t rule[] = "--------*--------";
	static const uint8_t fighter_expected[] =
	    "\r\nTime: 14:59  Computer command (?=help)? 11\r\n"
	    "\r\nSearching;\r\n\r\n"
	    " Sector   Amount\n\r"
	    "--------*--------\n\r"
	    " 3        20\n\r"
	    " 9        4\n\r"
	    "\r\n"
	    "\r\nTime: 14:59  Computer command (?=help)? ";
	struct yt_present_state current;
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	char accumulator[80] = "";
	uint8_t mutable[64];
	size_t length;
	int index;

	current = state(false);
	current.foreground = 1.0f;
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 1;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, computer_prompt,
	    sizeof(computer_prompt) - 1U, &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo((const uint8_t *)"11", 2,
	    (const uint8_t *)"11", 2, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, searching,
	    sizeof(searching) - 1U, &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	memcpy(mutable, " Sector", strlen(" Sector"));
	length = strlen(" Sector");
	CHECK(yt_present_fixed_width(mutable, &length, sizeof(mutable), 10.0f,
	    &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current, amount, sizeof(amount) - 1U,
	    &capture);
	pager_fixture_b05d(&pager, &current, rule, sizeof(rule) - 1U,
	    &capture);
	for (index = 0; index < 2; ++index) {
		static const char *sectors[2] = {" 3", " 9"};
		static const char *fighters[2] = {" 20", " 4"};

		length = strlen(sectors[index]);
		memcpy(mutable, sectors[index], length);
		CHECK(yt_present_fixed_width(mutable, &length, sizeof(mutable),
		    9.0f, &current, &result) == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		pager_fixture_b05d(&pager, &current,
		    (const uint8_t *)fighters[index], strlen(fighters[index]),
		    &capture);
	}
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, computer_prompt,
	    sizeof(computer_prompt) - 1U, &capture);
	CHECK(sizeof(fighter_expected) - 1U == 170U);
	CHECK(capture.remote_length == sizeof(fighter_expected) - 1U
	    && memcmp(capture.remote, fighter_expected,
	    sizeof(fighter_expected) - 1U) == 0);

	{
		static const uint8_t scanning[] = "Scanning...";
		static const uint8_t prefix[] = "Planet: ";
		static const uint8_t infix[] = " Sector:";
		uint8_t name[41];
		uint8_t row[80];
		uint8_t expected[227];
		size_t expected_length = 0;

		current = state(false);
		current.foreground = 1.0f;
		memset(&pager, 0, sizeof(pager));
		pager.foreground = 1;
		memset(&capture, 0, sizeof(capture));
		CHECK(yt_present_line(NULL, 0, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		pager.newline_flag = 1.0f;
		pager_fixture_b05d(&pager, &current, computer_prompt,
		    sizeof(computer_prompt) - 1U, &capture);
		yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
		CHECK(yt_present_editor_echo((const uint8_t *)"13", 2,
		    (const uint8_t *)"13", 2, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		CHECK(yt_present_line(NULL, 0, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		current.foreground = 2.0f;
		CHECK(yt_present_line(NULL, 0, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		CHECK(yt_present_line(scanning, sizeof(scanning) - 1U,
		    &current, &result) == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		CHECK(yt_present_line(NULL, 0, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		current.foreground = 3.0f;
		memset(name, ' ', sizeof(name));
		memcpy(name, "Home", 4);
		for (index = 0; index < 2; ++index) {
			length = 0;
			memcpy(row + length, prefix, sizeof(prefix) - 1U);
			length += sizeof(prefix) - 1U;
			memcpy(row + length, name, sizeof(name));
			length += sizeof(name);
			memcpy(row + length, infix, sizeof(infix) - 1U);
			length += sizeof(infix) - 1U;
			row[length++] = ' ';
			row[length++] = (uint8_t)(index == 0 ? '2' : '5');
			CHECK(yt_present_bold_line(row, length, &current, &result)
			    == YT_PRESENT_OK);
			pager_capture_result(&capture, &result);
		}
		CHECK(yt_present_line(NULL, 0, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		pager.newline_flag = 1.0f;
		pager_fixture_b05d(&pager, &current, computer_prompt,
		    sizeof(computer_prompt) - 1U, &capture);
		memcpy(expected + expected_length,
		    "\r\nTime: 14:59  Computer command (?=help)? 13\r\n"
		    "\r\nScanning...\r\n\r\n", 63);
		expected_length += 63U;
		for (index = 0; index < 2; ++index) {
			length = 0;
			memcpy(row + length, prefix, sizeof(prefix) - 1U);
			length += sizeof(prefix) - 1U;
			memcpy(row + length, name, sizeof(name));
			length += sizeof(name);
			memcpy(row + length, infix, sizeof(infix) - 1U);
			length += sizeof(infix) - 1U;
			row[length++] = ' ';
			row[length++] = (uint8_t)(index == 0 ? '2' : '5');
			memcpy(expected + expected_length, row, length);
			expected_length += length;
			memcpy(expected + expected_length, "\r\n", 2);
			expected_length += 2U;
		}
		memcpy(expected + expected_length,
		    "\r\nTime: 14:59  Computer command (?=help)? ", 42);
		expected_length += 42U;
		CHECK(expected_length == sizeof(expected));
		CHECK(capture.remote_length == sizeof(expected)
		    && memcmp(capture.remote, expected, sizeof(expected)) == 0);
		CHECK(pager.line_count == 1.0f);
	}
}

static void
treasury_cycle_fixture(bool collecting, const uint8_t *command,
    size_t command_length, const uint8_t *prompt, size_t prompt_length,
    bool scan_sector, float foreground, struct pager_capture *capture,
    struct yt_present_state *current, struct yt_pager_state *pager)
{
	static const uint8_t collect_prefix[] =
	    "Sending out armored cargo ships to";
	static const uint8_t report_prefix[] =
	    "Checking galactic bank statement for";
	static const uint8_t suffix[] = " ports with credits...";
	static const uint8_t total[] = " Total: 10";
	static const uint8_t summaries[][24] = {
	    "Total ports...: 2", "With credits..: 1",
	    "Barren ports..: 1", "Total credits.: 10"
	};
	static const uint8_t collected[] =
	    "You collected a total of 10 credits.";
	static const uint8_t reported[] =
	    "You have 10 credits in your port accounts.";
	struct yt_present_result result;
	uint8_t mutable[64];
	size_t length;
	size_t index;
	char accumulator[80] = "";

	*current = state(false);
	current->foreground = foreground;
	memset(pager, 0, sizeof(*pager));
	pager->foreground = (int)foreground;
	memset(capture, 0, sizeof(*capture));
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager->newline_flag = 1.0f;
	pager_fixture_b05d(pager, current, prompt, prompt_length, capture);
	yt_pager_editor_enter(pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo(command, command_length, command,
	    command_length, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);

	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	CHECK(yt_present_character(collecting ? collect_prefix : report_prefix,
	    collecting ? sizeof(collect_prefix) - 1U
	    : sizeof(report_prefix) - 1U, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	CHECK(yt_present_line(suffix, sizeof(suffix) - 1U, current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);

	memcpy(mutable, "Sector: 5", strlen("Sector: 5"));
	length = strlen("Sector: 5");
	CHECK(yt_present_fixed_width(mutable, &length, sizeof(mutable), 14.0f,
	    current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	memcpy(mutable, "Alpha", strlen("Alpha"));
	length = strlen("Alpha");
	CHECK(yt_present_fixed_width(mutable, &length, sizeof(mutable), 25.0f,
	    current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	memcpy(mutable, " Credits: 10", strlen(" Credits: 10"));
	length = strlen(" Credits: 10");
	CHECK(yt_present_fixed_width(mutable, &length, sizeof(mutable), 20.0f,
	    current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	CHECK(yt_present_line(total, sizeof(total) - 1U, current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	for (index = 0; index < sizeof(summaries) / sizeof(summaries[0]);
	    ++index) {
		CHECK(yt_present_line(summaries[index],
		    strlen((const char *)summaries[index]), current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(capture, &result);
	}
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	CHECK(yt_present_line(collecting ? collected : reported,
	    collecting ? sizeof(collected) - 1U : sizeof(reported) - 1U,
	    current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);

	if (scan_sector) {
		CHECK(yt_present_line(NULL, 0, current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(capture, &result);
		CHECK(yt_present_line((const uint8_t *)"Sector: 7", 9U,
		    current, &result) == YT_PRESENT_OK);
		pager_capture_result(capture, &result);
		CHECK(yt_present_line((const uint8_t *)"Warps lead to: 9", 16U,
		    current, &result) == YT_PRESENT_OK);
		pager_capture_result(capture, &result);
	}
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager->newline_flag = 1.0f;
	pager_fixture_b05d(pager, current, prompt, prompt_length, capture);
}

static void
test_computer_treasury_presentation(void)
{
	static const uint8_t computer_prompt[] =
	    "Time: 14:59  Computer command (?=help)? ";
	static const uint8_t main_prompt[] =
	    "Time: 14:59  Main Command (?=Help)? ";
	static const uint8_t report_expected[] =
	    "\r\nTime: 14:59  Computer command (?=help)? 12\r\n"
	    "\r\nChecking galactic bank statement for ports with credits...\r\n"
	    "\r\nSector: 5     Alpha                     Credits: 10"
	    "         Total: 10\r\n"
	    "\r\nTotal ports...: 2\r\nWith credits..: 1\r\n"
	    "Barren ports..: 1\r\nTotal credits.: 10\r\n"
	    "\r\nYou have 10 credits in your port accounts.\r\n"
	    "\r\nTime: 14:59  Computer command (?=help)? ";
	static const uint8_t collect_expected[] =
	    "\r\nTime: 14:59  Computer command (?=help)? !\r\n"
	    "\r\nSending out armored cargo ships to ports with credits...\r\n"
	    "\r\nSector: 5     Alpha                     Credits: 10"
	    "         Total: 10\r\n"
	    "\r\nTotal ports...: 2\r\nWith credits..: 1\r\n"
	    "Barren ports..: 1\r\nTotal credits.: 10\r\n"
	    "\r\nYou collected a total of 10 credits.\r\n"
	    "\r\nTime: 14:59  Computer command (?=help)? ";
	static const uint8_t main_expected[] =
	    "\r\nTime: 14:59  Main Command (?=Help)? $\r\n"
	    "\r\nSending out armored cargo ships to ports with credits...\r\n"
	    "\r\nSector: 5     Alpha                     Credits: 10"
	    "         Total: 10\r\n"
	    "\r\nTotal ports...: 2\r\nWith credits..: 1\r\n"
	    "Barren ports..: 1\r\nTotal credits.: 10\r\n"
	    "\r\nYou collected a total of 10 credits.\r\n"
	    "\r\nSector: 7\r\nWarps lead to: 9\r\n"
	    "\r\nTime: 14:59  Main Command (?=Help)? ";
	static const uint8_t no_ports[] = "You don't OWN any ports!!!";
	static const uint8_t no_ports_expected[] =
	    "\r\n\x1b[0;31;40;5;1mYou don't OWN any ports!!!\r\n";
	struct yt_present_state current;
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;

	treasury_cycle_fixture(false, (const uint8_t *)"12", 2U,
	    computer_prompt, sizeof(computer_prompt) - 1U, false, 1.0f,
	    &capture, &current, &pager);
	CHECK(sizeof(report_expected) - 1U == 348U);
	CHECK(capture.remote_length == sizeof(report_expected) - 1U
	    && memcmp(capture.remote, report_expected,
	    sizeof(report_expected) - 1U) == 0);
	CHECK(pager.line_count == 1.0f && pager.newline_flag == 0.0f);

	treasury_cycle_fixture(true, (const uint8_t *)"!", 1U,
	    computer_prompt, sizeof(computer_prompt) - 1U, false, 1.0f,
	    &capture, &current, &pager);
	CHECK(sizeof(collect_expected) - 1U == 339U);
	CHECK(capture.remote_length == sizeof(collect_expected) - 1U
	    && memcmp(capture.remote, collect_expected,
	    sizeof(collect_expected) - 1U) == 0);
	CHECK(pager.line_count == 1.0f && pager.newline_flag == 0.0f);

	treasury_cycle_fixture(true, (const uint8_t *)"$", 1U,
	    main_prompt, sizeof(main_prompt) - 1U, true, 2.0f,
	    &capture, &current, &pager);
	CHECK(sizeof(main_expected) - 1U == 362U);
	CHECK(capture.remote_length == sizeof(main_expected) - 1U
	    && memcmp(capture.remote, main_expected,
	    sizeof(main_expected) - 1U) == 0);
	CHECK(pager.line_count == 1.0f && pager.newline_flag == 0.0f);

	current = state(true);
	current.foreground = 1.0f;
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	memset(&capture, 0, sizeof(capture));
	pager_capture_result(&capture, &result);
	current.blink = 1.0f;
	CHECK(yt_present_bold_line(no_ports, sizeof(no_ports) - 1U,
	    &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(capture.remote_length == sizeof(no_ports_expected) - 1U
	    && memcmp(capture.remote, no_ports_expected,
	    sizeof(no_ports_expected) - 1U) == 0);
}

static void
fighters_cycle_fixture(bool ansi, struct pager_capture *capture,
    struct yt_present_state *current, struct yt_pager_state *pager)
{
	static const uint8_t main_prompt[] =
	    "Time: 14:59  Main Command (?=Help)? ";
	static const uint8_t title[] = "<Drop/Take Fighters>";
	static const uint8_t available[] =
	    "You have 18 fighters available.";
	static const uint8_t desired[] =
	    "Defend this sector with how many? ";
	static const uint8_t success[] =
	    "Done.  You have 6 fighters left.";
	struct yt_present_result result;
	char accumulator[80] = "";

	*current = state(ansi);
	current->foreground = 2.0f;
	memset(pager, 0, sizeof(*pager));
	pager->foreground = 2;
	memset(capture, 0, sizeof(*capture));
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager->newline_flag = 1.0f;
	pager_fixture_b05d(pager, current, main_prompt,
	    sizeof(main_prompt) - 1U, capture);
	yt_pager_editor_enter(pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo((const uint8_t *)"fTrailing", 9,
	    (const uint8_t *)"fTrailing", 9, current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);

	pager->newline_flag = 0.0f;
	pager_fixture_b05d(pager, current, title, sizeof(title) - 1U, capture);
	pager->newline_flag = 0.0f;
	pager_fixture_b05d(pager, current, available,
	    sizeof(available) - 1U, capture);
	pager->newline_flag = 1.0f;
	pager_fixture_b05d(pager, current, desired, sizeof(desired) - 1U,
	    capture);
	yt_pager_editor_enter(pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo((const uint8_t *)"12", 2,
	    (const uint8_t *)"12", 2, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager->newline_flag = 0.0f;
	pager_fixture_b05d(pager, current, success, sizeof(success) - 1U,
	    capture);
	CHECK(yt_present_sound(4.0f, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);

	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager->newline_flag = 1.0f;
	pager_fixture_b05d(pager, current, main_prompt,
	    sizeof(main_prompt) - 1U, capture);
}

static void
test_main_fighters_presentation(void)
{
	static const uint8_t plain[] =
	    "\r\nTime: 14:59  Main Command (?=Help)? fTrailing\r\n"
	    "<Drop/Take Fighters>\n\r"
	    "You have 18 fighters available.\n\r"
	    "Defend this sector with how many? 12\r\n"
	    "Done.  You have 6 fighters left.\n\r"
	    "\r\nTime: 14:59  Main Command (?=Help)? ";
	static const uint8_t ansi[] =
	    "\x1b[0;32;40m\r\nTime: 14:59  Main Command (?=Help)? "
	    "fTrailing\r\n"
	    "<Drop/Take Fighters>\n\r"
	    "You have 18 fighters available.\n\r"
	    "Defend this sector with how many? 12\r\n"
	    "Done.  You have 6 fighters left.\n\r"
	    "\x1b[MBT128O5L48P64CP64C\x0e"
	    "\r\nTime: 14:59  Main Command (?=Help)? ";
	struct yt_present_state current;
	struct yt_pager_state pager;
	struct pager_capture capture;

	fighters_cycle_fixture(false, &capture, &current, &pager);
	CHECK(sizeof(plain) - 1U == 214U);
	CHECK(capture.remote_length == sizeof(plain) - 1U
	    && memcmp(capture.remote, plain, sizeof(plain) - 1U) == 0);

	fighters_cycle_fixture(true, &capture, &current, &pager);
	CHECK(sizeof(ansi) - 1U == 246U);
	CHECK(capture.remote_length == sizeof(ansi) - 1U
	    && memcmp(capture.remote, ansi, sizeof(ansi) - 1U) == 0);
}

static void
genesis_body_fixture(bool ansi, struct pager_capture *capture)
{
	static const uint8_t prophecy_one[] =
	    "It has been written that one day a Trader Baron will rise up";
	static const uint8_t prophecy_two[] =
	    "and wipe the universe clean of the evil that infests it.";
	static const uint8_t prompt[] =
	    "Are you that Trader Captain Byte [y/N]";
	static const uint8_t success_one[] =
	    "...and so it was written, that one day a trader baron would emerge who";
	static const uint8_t success_two[] =
	    "would wipe away the all of the evil in the universe.....";
	struct yt_present_state current = state(ansi);
	struct yt_present_result result;
	struct yt_pager_state pager;
	char accumulator[80] = "";

	memset(&pager, 0, sizeof(pager));
	if (ansi)
		current.cached_foreground = 2.0f;
	pager.foreground = 2;
	memset(capture, 0, sizeof(*capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager.newline_flag = 0.0f;
	pager_fixture_b05d(&pager, &current, prophecy_one,
	    sizeof(prophecy_one) - 1U, capture);
	pager.newline_flag = 0.0f;
	pager_fixture_b05d(&pager, &current, prophecy_two,
	    sizeof(prophecy_two) - 1U, capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	CHECK(yt_present_character(prompt, sizeof(prompt) - 1U,
	    &current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo((const uint8_t *)"Y", 1U,
	    (const uint8_t *)"Y", 1U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	current.bold = 1.0f;
	pager.newline_flag = 0.0f;
	pager_fixture_b05d(&pager, &current, success_one,
	    sizeof(success_one) - 1U, capture);
	current.bold = 1.0f;
	pager.newline_flag = 0.0f;
	pager_fixture_b05d(&pager, &current, success_two,
	    sizeof(success_two) - 1U, capture);
}

static void
test_main_genesis_presentation(void)
{
	static const uint8_t plain[] =
	    "\r\nIt has been written that one day a Trader Baron will rise up\n\r"
	    "and wipe the universe clean of the evil that infests it.\n\r"
	    "\r\nAre you that Trader Captain Byte [y/N]Y\r\n\r\n"
	    "...and so it was written, that one day a trader baron would emerge who\n\r"
	    "would wipe away the all of the evil in the universe.....\n\r";
	static const uint8_t ansi[] =
	    "\r\nIt has been written that one day a Trader Baron will rise up\n\r"
	    "and wipe the universe clean of the evil that infests it.\n\r"
	    "\r\nAre you that Trader Captain Byte [y/N]Y\r\n\r\n"
	    "\x1b[0;32;40;1m"
	    "...and so it was written, that one day a trader baron would emerge who\n\r"
	    "\x1b[0;32;40;1m"
	    "would wipe away the all of the evil in the universe.....\n\r";
	struct pager_capture capture;

	genesis_body_fixture(false, &capture);
	CHECK(sizeof(plain) - 1U == 297U);
	CHECK(capture.remote_length == sizeof(plain) - 1U
	    && memcmp(capture.remote, plain, sizeof(plain) - 1U) == 0);
	genesis_body_fixture(true, &capture);
	CHECK(sizeof(ansi) - 1U == 321U);
	CHECK(capture.remote_length == sizeof(ansi) - 1U
	    && memcmp(capture.remote, ansi, sizeof(ansi) - 1U) == 0);
}

static void
test_planet_garrison_presentation(void)
{
	static const uint8_t expected[] =
	    "\r\nDrop how many ground force units on the planet? 18 Available ->"
	    "12\r\n\r\nGround force strength now at 12 units!\n\r";
	struct yt_present_state current = state(false);
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	uint8_t prompt[128];
	uint8_t success[128];
	size_t prompt_length;
	size_t success_length;
	char accumulator[80] = "";

	current.foreground = 6.0f;
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 6;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_planet_garrison_prompt(8.0f, 10.0f, prompt,
	    sizeof(prompt), &prompt_length));
	CHECK(yt_planet_garrison_success_row(12.0f, success,
	    sizeof(success), &success_length));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, prompt, prompt_length, &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo((const uint8_t *)"12", 2U,
	    (const uint8_t *)"12", 2U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.bold = 1.0f;
	current.blink = 1.0f;
	pager.newline_flag = 0.0f;
	pager_fixture_b05d(&pager, &current, success, success_length, &capture);
	CHECK(sizeof(expected) - 1U == 111U);
	CHECK(capture.remote_length == sizeof(expected) - 1U
	    && memcmp(capture.remote, expected, sizeof(expected) - 1U) == 0);
}

static void
test_planet_landing_presentation(void)
{
	static const uint8_t title[] = "<Land/Create planet>";
	static const uint8_t landing[] = "Landing...";
	static const uint8_t permission[] = "Permission to land is ";
	static const uint8_t denied[] = "DENIED!";
	static const uint8_t confirmation[] =
	    "Do you wish to try to force a landing? [y/N] ";
	static const uint8_t expected[] =
	    "\r\n<Land/Create planet>\n\r"
	    "\r\nLanding...\n\r"
	    "\r\n"
	    "This is space traffic control at planet LOCKED\r\n"
	    "Permission to land is DENIED!\r\n"
	    "\r\n"
	    "Sensors report ground forces of 10 units. You have 5.\n\r"
	    "Do you wish to try to force a landing? [y/N] N\r\n";
	struct yt_present_state current = state(false);
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	uint8_t traffic[128];
	uint8_t sensor[128];
	size_t traffic_length;
	size_t sensor_length;
	char accumulator[80] = "";

	memset(&pager, 0, sizeof(pager));
	pager.foreground = 2;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_planet_landing_traffic_row((const uint8_t *)"LOCKED", 6U,
	    traffic, sizeof(traffic), &traffic_length));
	CHECK(yt_planet_landing_sensor_row(10.0f, 5.0f, sensor,
	    sizeof(sensor), &sensor_length));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 0.0f;
	pager_fixture_b05d(&pager, &current, title, sizeof(title) - 1U,
	    &capture);
	current.foreground = 6.0f;
	pager.foreground = 6;
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 0.0f;
	pager_fixture_b05d(&pager, &current, landing, sizeof(landing) - 1U,
	    &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(traffic, traffic_length, &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_character(permission, sizeof(permission) - 1U,
	    &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.blink = 1.0f;
	current.foreground = 3.0f;
	pager.foreground = 3;
	CHECK(yt_present_bold_line(denied, sizeof(denied) - 1U, &current,
	    &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.foreground = 6.0f;
	pager.foreground = 6;
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 0.0f;
	pager_fixture_b05d(&pager, &current, sensor, sensor_length, &capture);
	CHECK(yt_present_character(confirmation, sizeof(confirmation) - 1U,
	    &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo((const uint8_t *)"N", 1U,
	    (const uint8_t *)"N", 1U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(capture.remote_length == sizeof(expected) - 1U
	    && memcmp(capture.remote, expected, sizeof(expected) - 1U) == 0);
	CHECK(pager.line_count == 0.0f);
	CHECK(current.foreground == 6.0f
	    && current.bold == 1.0f && current.blink == 1.0f);
}

static void
test_planet_assault_presentation(void)
{
	static const uint8_t engaging[] = "Forces engaging!";
	static const uint8_t defenses[] = "Planetary defenses destroyed!";
	static const uint8_t captured[] = "You've captured the planet!";
	static const uint8_t victory[] =
	    "\r\n"
	    "Forces engaging!\r\n"
	    "\r\n"
	    "Ground forces remaining: 0!\r\n"
	    "\r\n"
	    "Planetary defenses destroyed!\r\n"
	    "\x07"
	    "\r\n"
	    "You've captured the planet!\r\n"
	    "\x07";
	static const uint8_t failure[] =
	    "\r\n"
	    "Forces engaging!\r\n"
	    "\r\n"
	    "Your forces remaining  : 0!\r\n"
	    "\r\n"
	    "Attack Failed! Ground Forces remaining: 5!\r\n";
	struct yt_present_state current;
	struct yt_present_result result;
	struct pager_capture capture;
	uint8_t row[128];
	size_t row_length;

	current = state(false);
	current.foreground = 6.0f;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.blink = 1.0f;
	CHECK(yt_present_bold_line(engaging, sizeof(engaging) - 1U, &current,
	    &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_sound(2.0f, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.foreground = 4.0f;
	CHECK(yt_planet_assault_status_row(false, 0.0f, row, sizeof(row),
	    &row_length));
	CHECK(yt_present_line(row, row_length, &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_sound(2.0f, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.foreground = 6.0f;
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_bold_line(defenses, sizeof(defenses) - 1U, &current,
	    &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_sound(1.0f, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.blink = 1.0f;
	CHECK(yt_present_bold_line(captured, sizeof(captured) - 1U, &current,
	    &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_sound(1.0f, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(capture.remote_length == sizeof(victory) - 1U
	    && memcmp(capture.remote, victory, sizeof(victory) - 1U) == 0);
	CHECK(sizeof(victory) - 1U == 117U);

	current = state(false);
	current.foreground = 6.0f;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.blink = 1.0f;
	CHECK(yt_present_bold_line(engaging, sizeof(engaging) - 1U, &current,
	    &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_sound(2.0f, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.foreground = 3.0f;
	CHECK(yt_planet_assault_status_row(true, 0.0f, row, sizeof(row),
	    &row_length));
	CHECK(yt_present_line(row, row_length, &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.foreground = 6.0f;
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.blink = 1.0f;
	CHECK(yt_planet_assault_failure_row(5.0f, false, row, sizeof(row),
	    &row_length));
	CHECK(yt_present_bold_line(row, row_length, &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(capture.remote_length == sizeof(failure) - 1U
	    && memcmp(capture.remote, failure, sizeof(failure) - 1U) == 0);
	CHECK(sizeof(failure) - 1U == 97U);
}

static void
test_planet_creation_presentation(void)
{
	static const uint8_t no_planet[] =
	    "There is no planet in this sector.";
	static const uint8_t price[] = "Planets cost 25000 credits.";
	static const uint8_t buy[] =
	    "Do you wish to buy a planet(Y/N) [N]? ";
	static const uint8_t name_prompt[] =
	    "What do you want to name this planet? -=>";
	static const uint8_t confirmation[] =
	    "\"Nova\" Is this OK? (Y/n) [Y] ?";
	static const uint8_t advice[] =
	    "To increase productivity on your new planet, spend credits [$] on it.";
	static const uint8_t expected[] =
	    "\r\nThere is no planet in this sector.\n\r"
	    "Planets cost 25000 credits.\n\r"
	    "You have 30000 credits.\n\r"
	    "Do you wish to buy a planet(Y/N) [N]? Y\r\n"
	    "\r\nWhat do you want to name this planet? -=>Nova\r\n"
	    "\r\n\"Nova\" Is this OK? (Y/n) [Y] ?Y\r\n"
	    "\r\nPlanet \"Nova\" created with Genesis Device!\n\r"
	    "\r\nTo increase productivity on your new planet, spend "
	    "credits [$] on it.\n\r";
	struct yt_present_state current = state(false);
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	uint8_t credit[128];
	uint8_t created[128];
	size_t credit_length;
	size_t created_length;
	char accumulator[80] = "";

	current.foreground = 6.0f;
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 6;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_planet_creation_credit_row(30000.0, credit, sizeof(credit),
	    &credit_length));
	CHECK(yt_planet_creation_success_row((const uint8_t *)"Nova", 4U,
	    created, sizeof(created), &created_length));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 0.0f;
	pager_fixture_b05d(&pager, &current, no_planet,
	    sizeof(no_planet) - 1U, &capture);
	pager.newline_flag = 0.0f;
	pager_fixture_b05d(&pager, &current, price, sizeof(price) - 1U,
	    &capture);
	pager.newline_flag = 0.0f;
	pager_fixture_b05d(&pager, &current, credit, credit_length, &capture);
	CHECK(yt_present_character(buy, sizeof(buy) - 1U, &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo((const uint8_t *)"Y", 1U,
	    (const uint8_t *)"Y", 1U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, name_prompt,
	    sizeof(name_prompt) - 1U, &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo((const uint8_t *)"Nova", 4U,
	    (const uint8_t *)"Nova", 4U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_character(confirmation, sizeof(confirmation) - 1U,
	    &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo((const uint8_t *)"Y", 1U,
	    (const uint8_t *)"Y", 1U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 0.0f;
	pager_fixture_b05d(&pager, &current, created, created_length, &capture);
	CHECK(yt_present_sound(4.0f, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 0.0f;
	pager_fixture_b05d(&pager, &current, advice, sizeof(advice) - 1U,
	    &capture);
	CHECK(capture.remote_length == sizeof(expected) - 1U
	    && memcmp(capture.remote, expected, sizeof(expected) - 1U) == 0);
	CHECK(sizeof(expected) - 1U == 336U);
	CHECK(pager.line_count == 2.0f);
}

static void
test_planet_move_presentation(void)
{
	static const uint8_t cost[] =
	    "Moving planets costs 10 turns per sector.";
	static const uint8_t destination[] = "Move planet to what sector? ";
	static const uint8_t working[] = "Working. ";
	static const uint8_t confirmation[] = "Move the planet? (Y/[N])";
	static const uint8_t engaged[] = "Planet thrusters engaged.";
	static const uint8_t moving[] = "Moving to sector:";
	static const uint8_t expected[] =
	    "\r\nMoving planets costs 10 turns per sector.\n\r"
	    "Move planet to what sector? 2\r\n"
	    "\r\nWorking. The shortest path from sector 1 to sector 2 is:\n\r"
	    "\r\n 1 2\r\n"
	    "\r\nDistance is 1 and will take 10 turns.\n\r"
	    "You have 100 turns left.\n\r"
	    "Move the planet? (Y/[N])Y\r\n"
	    "\r\nPlanet thrusters engaged.\n\r"
	    "Moving to sector: 2\r\n"
	    "\r\nGaia moved! (Xannoron Movers, we move anyTHING, anyWHERE!)\r\n";
	struct yt_present_state current = state(false);
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	uint8_t row[256];
	size_t row_length;
	char accumulator[80] = "";
	char number[64];

	current.foreground = 6.0f;
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 6;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current, cost, sizeof(cost) - 1U, &capture);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, destination,
	    sizeof(destination) - 1U, &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo((const uint8_t *)"2", 1U,
	    (const uint8_t *)"2", 1U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, working,
	    sizeof(working) - 1U, &capture);
	CHECK(yt_planet_move_path_heading(1.0f, 2.0f, row,
	    sizeof(row), &row_length));
	pager.newline_flag = 0.0f;
	pager_fixture_b05d(&pager, &current, row, row_length, &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(qb_str_single(number, sizeof(number), 1.0f) > 0);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, (const uint8_t *)number,
	    strlen(number), &capture);
	CHECK(qb_str_single(number, sizeof(number), 2.0f) > 0);
	pager.line_count = 0.0f;
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, (const uint8_t *)number,
	    strlen(number), &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_planet_move_summary(10.0f, row, sizeof(row), &row_length));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 0.0f;
	pager_fixture_b05d(&pager, &current, row, row_length, &capture);
	CHECK(yt_planet_move_turns_row(100.0f, row, sizeof(row), &row_length));
	pager_fixture_b05d(&pager, &current, row, row_length, &capture);
	CHECK(yt_present_character(confirmation, sizeof(confirmation) - 1U,
	    &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo((const uint8_t *)"Y", 1U,
	    (const uint8_t *)"Y", 1U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.bold = 1.0f;
	current.blink = 1.0f;
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current, engaged,
	    sizeof(engaged) - 1U, &capture);
	pager.line_count = 0.0f;
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, moving,
	    sizeof(moving) - 1U, &capture);
	CHECK(qb_str_single(number, sizeof(number), 2.0f) > 0);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, (const uint8_t *)number,
	    strlen(number), &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_planet_move_success_row((const uint8_t *)"Gaia", 4U,
	    row, sizeof(row), &row_length));
	CHECK(yt_present_line(row, row_length, &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(capture.remote_length == sizeof(expected) - 1U
	    && memcmp(capture.remote, expected, sizeof(expected) - 1U) == 0);
	CHECK(sizeof(expected) - 1U == 350U);
}

static void
test_sector_mine_presentation(void)
{
	static const uint8_t warning[] = "** Sector is Mined!! **";
	static const uint8_t shields_destroyed[] = "Shields disintegrated!";
	static const uint8_t scanner_destroyed[] =
	    "Danger scanner destroyed!";
	static const uint8_t expected[] =
	    "\r\n"
	    "** Sector is Mined!! **\r\n"
	    "\x07"
	    "There are 3 mines here! 1 EXPLODE!\r\n"
	    "Shields down to 1 units!\r\n"
	    "Danger scanner destroyed!\r\n"
	    "There are 2 mines here! 1 EXPLODE!\r\n"
	    "Shields disintegrated!\r\n"
	    "There are 1 mines here! 1 EXPLODE!\r\n"
	    "Lost 1 fighters!\r\n"
	    "Lost 50% cloak!\r\n"
	    "Lost 1 Missiles!\r\n"
	    "Lost 1 mines!\r\n"
	    "Lost 1 holds of ore!\r\n"
	    "Lost 1 holds of organics!\r\n"
	    "Lost 1 holds of equipment!\r\n"
	    "Lost 1 empty holds!\r\n";
	static const struct {
		enum yt_sector_mine_loss_kind kind;
		float loss;
	} losses[] = {
		{YT_SECTOR_MINE_LOSS_FIGHTERS, 1.0f},
		{YT_SECTOR_MINE_LOSS_CLOAK, 50.0f},
		{YT_SECTOR_MINE_LOSS_MISSILES, 1.0f},
		{YT_SECTOR_MINE_LOSS_MINES, 1.0f},
		{YT_SECTOR_MINE_LOSS_ORE, 1.0f},
		{YT_SECTOR_MINE_LOSS_ORGANICS, 1.0f},
		{YT_SECTOR_MINE_LOSS_EQUIPMENT, 1.0f},
		{YT_SECTOR_MINE_LOSS_EMPTY_HOLDS, 1.0f},
	};
	struct yt_present_state current = state(false);
	struct yt_present_result result;
	struct pager_capture capture;
	uint8_t row[256];
	size_t row_length;
	size_t index;

	current.foreground = 6.0f;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.blink = 1.0f;
	CHECK(yt_present_line(warning, sizeof(warning) - 1U, &current,
	    &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_sound(5.0f, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);

	current.foreground = 3.0f;
	current.background = 0.0f;
	current.blink = 0.0f;
	CHECK(yt_sector_mine_explosion_row(3.0f, 1.0f, row,
	    sizeof(row), &row_length));
	CHECK(yt_present_bold_character(row, row_length, &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.background = 1.0f;
	CHECK(yt_present_line(NULL, 0U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_sector_mine_shields_row(1.0f, row, sizeof(row), &row_length));
	CHECK(yt_present_bold_line(row, row_length, &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.foreground = 7.0f;
	current.blink = 1.0f;
	CHECK(yt_present_bold_line(scanner_destroyed,
	    sizeof(scanner_destroyed) - 1U, &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.foreground = 6.0f;
	CHECK(yt_present_sound(2.0f, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);

	current.foreground = 3.0f;
	current.background = 0.0f;
	current.blink = 0.0f;
	CHECK(yt_sector_mine_explosion_row(2.0f, 1.0f, row,
	    sizeof(row), &row_length));
	CHECK(yt_present_bold_character(row, row_length, &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.background = 1.0f;
	CHECK(yt_present_line(NULL, 0U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.foreground = 7.0f;
	current.blink = 1.0f;
	CHECK(yt_present_bold_line(shields_destroyed,
	    sizeof(shields_destroyed) - 1U, &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.foreground = 6.0f;
	CHECK(yt_present_sound(2.0f, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);

	current.foreground = 3.0f;
	current.background = 0.0f;
	current.blink = 0.0f;
	CHECK(yt_sector_mine_explosion_row(1.0f, 1.0f, row,
	    sizeof(row), &row_length));
	CHECK(yt_present_bold_character(row, row_length, &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.background = 1.0f;
	CHECK(yt_present_line(NULL, 0U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	for (index = 0U; index < YT_ARRAY_LEN(losses); ++index) {
		CHECK(yt_sector_mine_loss_row(losses[index].kind,
		    losses[index].loss, row, sizeof(row), &row_length));
		CHECK(yt_present_line(row, row_length, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
	}
	CHECK(yt_present_sound(2.0f, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(capture.remote_length == sizeof(expected) - 1U
	    && memcmp(capture.remote, expected, sizeof(expected) - 1U) == 0);
	CHECK(sizeof(expected) - 1U == 379U);
	CHECK(current.foreground == 3.0f && current.background == 1.0f);
}

static void
test_direct_fighter_kill_warning_presentation(void)
{
	static const uint8_t expected[] =
	    "\r\n  -  VICTIM had sector mines! They EXPLODED!\n\r";
	struct yt_present_state current = state(false);
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	uint8_t row[128];
	size_t row_length;

	current.foreground = 6.0f;
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 6;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_direct_fighter_mine_warning((const uint8_t *)"VICTIM", 6U,
	    row, sizeof(row), &row_length));
	CHECK(yt_present_line(NULL, 0U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.bold = 1.0f;
	current.blink = 1.0f;
	pager.newline_flag = 0.0f;
	pager_fixture_b05d(&pager, &current, row, row_length, &capture);
	CHECK(capture.remote_length == sizeof(expected) - 1U
	    && memcmp(capture.remote, expected, sizeof(expected) - 1U) == 0);
	CHECK(sizeof(expected) - 1U == 48U);
	CHECK(pager.line_count == 1.0f);
}

static bool
direct_fighter_kill_composition_run(bool ansi, struct pager_capture *capture,
    struct yt_present_state *current, struct yt_pager_state *pager,
    size_t ends[5])
{
	static const uint8_t salvage_heading[] =
	    "You destroyed the ship and salvaged the following:";
	static const uint8_t mined[] = "** Sector is Mined!! **";
	struct yt_present_result result;
	uint8_t row[160];
	size_t row_length;
	size_t batch;

	if (capture == NULL || current == NULL || pager == NULL || ends == NULL)
		return false;
	*current = state(ansi);
	current->foreground = 6.0f;
	current->color_initialized = ansi ? 1.0f : 0.0f;
	current->cached_foreground = ansi ? 6.0f : 0.0f;
	memset(pager, 0, sizeof(*pager));
	pager->foreground = 6;
	memset(capture, 0, sizeof(*capture));

	if (yt_present_sound(3.0f, current, &result) != YT_PRESENT_OK)
		return false;
	pager_capture_result(capture, &result);
	ends[0] = capture->remote_length;

	if (!yt_death_title_row((const uint8_t *)"VICTIM", 6U, 1.0f,
	    row, sizeof(row), &row_length))
		return false;
	pager_capture_line(capture, current, row, row_length);
	ends[1] = capture->remote_length;

	pager_capture_line(capture, current, NULL, 0U);
	if (yt_present_bold_line(salvage_heading,
	    sizeof(salvage_heading) - 1U, current, &result) != YT_PRESENT_OK)
		return false;
	pager_capture_result(capture, &result);
	pager_capture_line(capture, current, NULL, 0U);
	if (!yt_salvage_simple_row(YT_SALVAGE_MINES, 1.0f, row,
	    sizeof(row), &row_length))
		return false;
	pager_capture_line(capture, current, row, row_length);
	ends[2] = capture->remote_length;

	if (!yt_direct_fighter_mine_warning((const uint8_t *)"VICTIM", 6U,
	    row, sizeof(row), &row_length))
		return false;
	pager_capture_line(capture, current, NULL, 0U);
	current->bold = 1.0f;
	current->blink = 1.0f;
	pager->newline_flag = 0.0f;
	pager_fixture_b05d(pager, current, row, row_length, capture);
	ends[3] = capture->remote_length;

	pager_capture_line(capture, current, NULL, 0U);
	current->blink = 1.0f;
	pager_capture_line(capture, current, mined, sizeof(mined) - 1U);
	if (yt_present_sound(5.0f, current, &result) != YT_PRESENT_OK)
		return false;
	pager_capture_result(capture, &result);
	for (batch = 0U; batch < 3U; ++batch) {
		current->foreground = 3.0f;
		current->background = 0.0f;
		current->blink = 0.0f;
		if (!yt_sector_mine_explosion_row(3.0f - (float)batch,
		    1.0f, row, sizeof(row), &row_length)
		    || yt_present_bold_character(row, row_length, current,
		    &result) != YT_PRESENT_OK)
			return false;
		pager_capture_result(capture, &result);
		current->background = 1.0f;
		pager_capture_line(capture, current, NULL, 0U);
		if (!yt_sector_mine_shields_row(10000.0f, row, sizeof(row),
		    &row_length)
		    || yt_present_bold_line(row, row_length, current, &result)
		    != YT_PRESENT_OK)
			return false;
		pager_capture_result(capture, &result);
		if (yt_present_sound(2.0f, current, &result) != YT_PRESENT_OK)
			return false;
		pager_capture_result(capture, &result);
	}
	ends[4] = capture->remote_length;
	return true;
}

static void
test_direct_fighter_kill_composition_presentation(void)
{
	static const uint8_t plain[] =
	    "\x07The titles to 1 ports of VICTIM's are now yours!\r\n"
	    "\r\nYou destroyed the ship and salvaged the following:\r\n"
	    "\r\n  -  Sector Mines: 1\r\n"
	    "\r\n  -  VICTIM had sector mines! They EXPLODED!\n\r"
	    "\r\n** Sector is Mined!! **\r\n\x07"
	    "There are 3 mines here! 1 EXPLODE!\r\n"
	    "Shields down to 10000 units!\r\n"
	    "There are 2 mines here! 1 EXPLODE!\r\n"
	    "Shields down to 10000 units!\r\n"
	    "There are 1 mines here! 1 EXPLODE!\r\n"
	    "Shields down to 10000 units!\r\n";
	static const uint8_t ansi[] =
	    "\x1b[MBO2L2P32CL3CL8CP32L3CP6E-L8DL3DL8CL3CO1L8BO2L1C\x0e"
	    "The titles to 1 ports of VICTIM's are now yours!\r\n"
	    "\r\n\x1b[0;36;40;1m"
	    "You destroyed the ship and salvaged the following:\r\n"
	    "\x1b[0;36;40m\r\n  -  Sector Mines: 1\r\n"
	    "\r\n\x1b[0;36;40;5;1m"
	    "  -  VICTIM had sector mines! They EXPLODED!\n\r"
	    "\x1b[0;36;40m\r\n\x1b[0;36;40;5m"
	    "** Sector is Mined!! **\r\n"
	    "\x1b[MBO1L64P8CdGCdGCdGCDGCGD\x0e"
	    "\x1b[0;33;40;1mThere are 3 mines here! 1 EXPLODE!"
	    "\x1b[0;33;41m\r\n"
	    "\x1b[0;33;41;1mShields down to 10000 units!\r\n"
	    "\x1b[MBO1L64P32CEDFEGFAGBAO5BAGFEDC\x0e"
	    "\x1b[0;33;40;1mThere are 2 mines here! 1 EXPLODE!"
	    "\x1b[0;33;41m\r\n"
	    "\x1b[0;33;41;1mShields down to 10000 units!\r\n"
	    "\x1b[MBO1L64P32CEDFEGFAGBAO5BAGFEDC\x0e"
	    "\x1b[0;33;40;1mThere are 1 mines here! 1 EXPLODE!"
	    "\x1b[0;33;41m\r\n"
	    "\x1b[0;33;41;1mShields down to 10000 units!\r\n"
	    "\x1b[MBO1L64P32CEDFEGFAGBAO5BAGFEDC\x0e";
	static const struct {
		bool ansi;
		const uint8_t *expected;
		size_t expected_length;
		size_t ends[5];
	} cases[] = {
		{false, plain, sizeof(plain) - 1U,
		    {1U, 51U, 129U, 177U, 403U}},
		{true, ansi, sizeof(ansi) - 1U,
		    {51U, 101U, 201U, 263U, 738U}},
	};
	struct yt_present_state current;
	struct yt_pager_state pager;
	struct pager_capture capture;
	size_t ends[5];
	size_t pass;

	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		CHECK(direct_fighter_kill_composition_run(cases[pass].ansi,
		    &capture, &current, &pager, ends));
		CHECK(memcmp(ends, cases[pass].ends, sizeof(ends)) == 0);
		CHECK(capture.remote_length == cases[pass].expected_length);
		CHECK(capture.remote_length != cases[pass].expected_length
		    || memcmp(capture.remote, cases[pass].expected,
		    cases[pass].expected_length) == 0);
		CHECK(current.foreground == 3.0f
		    && current.background == 1.0f
		    && current.bold == (cases[pass].ansi ? 0.0f : 1.0f)
		    && current.blink == 0.0f
		    && current.cached_foreground == 0.0f
		    && current.cached_background == 0.0f
		    && pager.foreground == 6 && pager.line_count == 1.0f
		    && pager.nonstop == 0.0f && pager.newline_flag == 0.0f);
	}
	CHECK(sizeof(plain) - 1U == 403U && sizeof(ansi) - 1U == 738U);
}

static struct pager_capture
black_hole_fixture(bool ansi, bool meltdown)
{
	static const uint8_t black_hole[] = "A *-BLACK HOLE-* grabs you!";
	static const uint8_t title[] = " * EMERGENCY WARP ENGAGED! * ";
	static const uint8_t wormhole[] =
	    "You enter a wormhole as your engines build up to emergency power!";
	static const uint8_t temperature[] = "     * Engine Temperature *";
	static const uint8_t scale[] = "[ Normal ][ Danger ][ Overheat ]";
	static const uint8_t ruler[] = "================================";
	static const uint8_t relief[] =
	    "You sigh in relief as you look at your scanner and find yourself in";
	static const uint8_t meltdown_row[] = "MELT DOWN!";
	static const uint8_t engines_disabled[] = "Your engines are disabled!";
	static const uint8_t repair[] =
	    "It will take a solar day to repair them.";
	struct yt_present_state current = state(ansi);
	struct yt_present_result result;
	struct pager_capture capture;
	uint8_t row[128];
	size_t row_length;

	current.foreground = 1.0f;
	current.color_initialized = ansi ? 1.0f : 0.0f;
	current.cached_foreground = ansi ? 1.0f : 0.0f;
	current.cached_background = 0.0f;
	current.sound.user_sound = 0.0f;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_attention(black_hole, sizeof(black_hole) - 1U,
	    &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_attention(title, sizeof(title) - 1U,
	    &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_bold_line(wormhole, sizeof(wormhole) - 1U,
	    &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.foreground = 6.0f;
	CHECK(yt_present_bold_line(temperature, sizeof(temperature) - 1U,
	    &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_bold_line(scale, sizeof(scale) - 1U,
	    &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.foreground = 2.0f;
	CHECK(yt_present_bold_line(ruler, sizeof(ruler) - 1U,
	    &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.foreground = 6.0f;
	CHECK(yt_present_bold_character((const uint8_t *)"[", 1U,
	    &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	for (size_t tick = 1U; tick <= (meltdown ? 31U : 1U); ++tick) {
		current.foreground = tick < 10U ? 2.0f
		    : tick < 20U ? 3.0f : 1.0f;
		if (tick >= 20U)
			current.blink = 1.0f;
		CHECK(yt_present_bold_character((const uint8_t *)"*", 1U,
		    &current, &result) == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
	}
	current.foreground = 2.0f;
	CHECK(yt_present_line(NULL, 0U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	if (meltdown) {
		CHECK(yt_present_attention(meltdown_row,
		    sizeof(meltdown_row) - 1U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		current.foreground = 1.0f;
		CHECK(yt_present_line(NULL, 0U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		CHECK(yt_present_bold_line(engines_disabled,
		    sizeof(engines_disabled) - 1U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		CHECK(yt_present_line(NULL, 0U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		CHECK(yt_present_bold_line(repair, sizeof(repair) - 1U,
		    &current, &result) == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		for (size_t cue = 0U; cue < 5U; ++cue) {
			CHECK(yt_present_sound(5.0f, &current, &result)
			    == YT_PRESENT_OK);
			pager_capture_result(&capture, &result);
		}
		CHECK(yt_present_line(NULL, 0U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		CHECK(yt_emergency_warp_stranded_row(1003.0f, row,
		    sizeof(row), &row_length));
	}
	else {
		CHECK(yt_present_sound(1.0f, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		CHECK(yt_present_line(relief, sizeof(relief) - 1U, &current,
		    &result) == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		CHECK(yt_emergency_warp_result_row(1003.0f, 3.0f, row,
		    sizeof(row), &row_length));
	}
	CHECK(yt_present_line(row, row_length, &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	return capture;
}

static uint64_t
presentation_fnv1a64(const uint8_t *data, size_t length)
{
	uint64_t value = UINT64_C(14695981039346656037);

	for (size_t index = 0U; index < length; ++index) {
		value ^= data[index];
		value *= UINT64_C(1099511628211);
	}
	return value;
}

struct info_panel_presentation_context {
	struct yt_present_state current;
	struct pager_capture capture;
	struct yt_player player;
};

static bool
info_panel_presentation_refresh(void *context, uint8_t *text,
    size_t capacity, size_t *length, struct yt_error *error)
{
	static const uint8_t value[] = " 15:09  ";

	(void)context;
	(void)error;
	if (length == NULL || capacity < sizeof(value) - 1U)
		return false;
	memcpy(text, value, sizeof(value) - 1U);
	*length = sizeof(value) - 1U;
	return true;
}

static bool
info_panel_presentation_team(void *context, struct yt_error *error)
{
	static const uint8_t none[] = "Team  : None";
	struct info_panel_presentation_context *fixture = context;
	struct yt_present_result result;

	(void)error;
	if (yt_present_line(none, sizeof(none) - 1U, &fixture->current,
	    &result) != YT_PRESENT_OK)
		return false;
	pager_capture_result(&fixture->capture, &result);
	if (yt_present_line(NULL, 0U, &fixture->current,
	    &result) != YT_PRESENT_OK)
		return false;
	pager_capture_result(&fixture->capture, &result);
	return true;
}

static bool
info_panel_presentation_read(void *context, struct yt_player *player,
    struct yt_error *error)
{
	struct info_panel_presentation_context *fixture = context;

	(void)error;
	*player = fixture->player;
	return true;
}

static bool
info_panel_presentation_present(void *context, const uint8_t *text,
    size_t length, enum yt_info_panel_output_kind kind, float width,
    struct yt_info_panel_state *state, struct yt_error *error)
{
	struct info_panel_presentation_context *fixture = context;
	struct yt_present_result result;
	enum yt_present_status status;
	uint8_t mutable[256];
	size_t mutable_length = length;

	(void)error;
	fixture->current.foreground = state->foreground;
	fixture->current.background = state->background;
	fixture->current.bold = state->bold;
	if (kind == YT_INFO_PANEL_LINE)
		status = yt_present_line(text, length, &fixture->current, &result);
	else if (kind == YT_INFO_PANEL_FIXED && length <= sizeof(mutable)
	    && (text != NULL || length == 0U)) {
		if (length != 0U)
			memcpy(mutable, text, length);
		status = yt_present_fixed_width(mutable, &mutable_length,
		    sizeof(mutable), width, &fixture->current, &result);
	}
	else
		return false;
	state->foreground = fixture->current.foreground;
	state->background = fixture->current.background;
	state->bold = fixture->current.bold;
	if (status != YT_PRESENT_OK)
		return false;
	pager_capture_result(&fixture->capture, &result);
	return true;
}

static struct info_panel_presentation_context
info_panel_presentation_fixture(bool ansi)
{
	static const struct yt_info_panel_ops ops = {
		info_panel_presentation_refresh,
		info_panel_presentation_team,
		info_panel_presentation_read,
		info_panel_presentation_present,
	};
	static const uint8_t name[] = "Pilot";
	struct info_panel_presentation_context fixture;
	struct yt_info_panel_state panel;

	memset(&fixture, 0, sizeof(fixture));
	memset(&panel, 0, sizeof(panel));
	fixture.current = state(ansi);
	fixture.current.foreground = 6.0f;
	fixture.player.credits = 12345.0f;
	fixture.player.sector = 733.0f;
	fixture.player.turns = 42.0f;
	fixture.player.holds = 20.0f;
	fixture.player.fighters = 1000.0f;
	fixture.player.ore = 3.0f;
	fixture.player.mines = 4.0f;
	fixture.player.organics = 5.0f;
	fixture.player.missiles = 6.0f;
	fixture.player.equipment = 7.0f;
	fixture.player.danger_scanner = 1.0f;
	fixture.player.ports_owned = 8.0f;
	fixture.player.shields = 90.0f;
	fixture.player.cloak = 0.75f;
	fixture.player.ground_forces = 9.0f;
	fixture.player.plasma = 10.0f;
	panel.cached_name = name;
	panel.cached_name_length = sizeof(name) - 1U;
	panel.foreground = fixture.current.foreground;
	panel.background = fixture.current.background;
	panel.bold = fixture.current.bold;
	CHECK(yt_info_panel_run(&panel, &ops, &fixture, NULL));
	fixture.current.foreground = panel.foreground;
	fixture.current.background = panel.background;
	fixture.current.bold = panel.bold;
	return fixture;
}

static void
test_info_panel_presentation(void)
{
	struct info_panel_presentation_context plain =
	    info_panel_presentation_fixture(false);
	struct info_panel_presentation_context ansi =
	    info_panel_presentation_fixture(true);

	CHECK(plain.capture.remote_length == 602U
	    && presentation_fnv1a64(plain.capture.remote,
	    plain.capture.remote_length) == UINT64_C(0x9b1a7fd0d0c4f1fd));
	CHECK(ansi.capture.remote_length == 678U
	    && presentation_fnv1a64(ansi.capture.remote,
	    ansi.capture.remote_length) == UINT64_C(0xb8f87d3dc030ed22));
	CHECK(plain.current.foreground == 6.0f
	    && plain.current.background == 0.0f
	    && plain.current.bold == 1.0f);
	CHECK(ansi.current.foreground == 6.0f
	    && ansi.current.background == 0.0f
	    && ansi.current.bold == 0.0f
	    && ansi.current.cached_foreground == 2.0f
	    && ansi.current.cached_background == 0.0f);
}

enum normal_exit_info_effect {
	NORMAL_EXIT_INFO_READ_CURRENT = 1,
	NORMAL_EXIT_INFO_LOAD_TEAM,
	NORMAL_EXIT_INFO_TEAM_ROW,
	NORMAL_EXIT_INFO_READ_CAPTAIN,
	NORMAL_EXIT_INFO_READ_OVERLAY,
	NORMAL_EXIT_INFO_WRITE_OVERLAY,
	NORMAL_EXIT_INFO_READ_FINAL,
};

enum normal_exit_info_team_fixture_route {
	NORMAL_EXIT_INFO_TEAM_NONE,
	NORMAL_EXIT_INFO_TEAM_SELF,
	NORMAL_EXIT_INFO_TEAM_OTHER,
	NORMAL_EXIT_INFO_TEAM_PROMOTION,
};

struct normal_exit_info_observation {
	struct yt_present_result refresh;
	struct yt_present_time_state time;
	struct yt_info_team_state team;
	struct yt_sector written_overlay;
	enum normal_exit_info_effect effects[16];
	float player_records[3];
	size_t effect_count;
	size_t player_read_count;
	size_t timer_used;
	bool time_updated;
	bool overlay_written;
};

struct normal_exit_info_context {
	struct physical_viewer_join *viewer;
	struct yt_player player;
	const uint8_t *time_text;
	size_t time_length;
	struct yt_present_time_state time;
	struct yt_player team_current;
	struct yt_player team_captain;
	struct yt_team team;
	struct yt_sector overlay;
	struct normal_exit_info_observation *observation;
	bool refresh_due;
	enum normal_exit_info_team_fixture_route team_route;
};

static bool
normal_exit_info_effect(struct normal_exit_info_context *fixture,
    enum normal_exit_info_effect effect)
{
	struct normal_exit_info_observation *observation = fixture->observation;

	if (observation == NULL
	    || observation->effect_count >= YT_ARRAY_LEN(observation->effects))
		return false;
	observation->effects[observation->effect_count++] = effect;
	return true;
}

static bool
normal_exit_info_refresh(void *context, uint8_t *text, size_t capacity,
    size_t *length, struct yt_error *error)
{
	struct normal_exit_info_context *fixture = context;
	static const float reads[] = {100.0f, 100.0f, 100.0f, 100.0f};
	struct yt_present_result result;
	size_t used;
	bool updated;

	(void)error;
	if (fixture->refresh_due) {
		if (length == NULL || fixture->observation == NULL
		    || yt_present_refresh_time(&fixture->time, reads,
		    YT_ARRAY_LEN(reads), &used, 1, 1,
		    &fixture->viewer->join.presentation, &result, &updated)
		    != YT_PRESENT_OK || fixture->time.text_length > capacity)
			return false;
		fixture->observation->refresh = result;
		fixture->observation->time = fixture->time;
		fixture->observation->timer_used = used;
		fixture->observation->time_updated = updated;
		viewer_pager_capture_result(&fixture->viewer->join, &result);
		if (fixture->time.text_length != 0U)
			memcpy(text, fixture->time.text,
			    fixture->time.text_length);
		*length = fixture->time.text_length;
		return true;
	}
	if (length == NULL || fixture->time_text == NULL
	    || capacity < fixture->time_length)
		return false;
	memcpy(text, fixture->time_text, fixture->time_length);
	*length = fixture->time_length;
	return true;
}

static bool
normal_exit_info_team_read(void *context, float record,
    struct yt_player *player, struct yt_error *error)
{
	struct normal_exit_info_context *fixture = context;
	struct normal_exit_info_observation *observation = fixture->observation;
	size_t position;

	(void)error;
	if (observation == NULL || player == NULL)
		return false;
	position = observation->player_read_count;
	if (position >= YT_ARRAY_LEN(observation->player_records))
		return false;
	observation->player_records[position] = record;
	++observation->player_read_count;
	if (position == 0U) {
		if (!normal_exit_info_effect(fixture,
		    NORMAL_EXIT_INFO_READ_CURRENT))
			return false;
		*player = fixture->team_current;
	}
	else {
		if (!normal_exit_info_effect(fixture,
		    NORMAL_EXIT_INFO_READ_CAPTAIN))
			return false;
		*player = fixture->team_captain;
	}
	return true;
}

static void
normal_exit_info_team_store_id(void *context, const uint8_t raw[4])
{
	(void)context;
	(void)raw;
}

static void
normal_exit_info_team_store_captain(void *context, const uint8_t raw[4])
{
	(void)context;
	(void)raw;
}

static bool
normal_exit_info_team_load(void *context, float team_id,
    float current_record, float *captain_flag, struct yt_team *team,
    struct yt_error *error)
{
	struct normal_exit_info_context *fixture = context;

	(void)error;
	if (team_id != 7.0f || current_record != 2.0f
	    || captain_flag == NULL || team == NULL
	    || !normal_exit_info_effect(fixture,
	    NORMAL_EXIT_INFO_LOAD_TEAM))
		return false;
	*captain_flag = fixture->team_route == NORMAL_EXIT_INFO_TEAM_SELF
	    ? -1.0f : 0.0f;
	*team = fixture->team;
	return true;
}

static bool
normal_exit_info_team_read_overlay(void *context, float team_id,
    struct yt_sector *overlay, struct yt_error *error)
{
	struct normal_exit_info_context *fixture = context;

	(void)error;
	if (team_id != 7.0f || overlay == NULL
	    || !normal_exit_info_effect(fixture,
	    NORMAL_EXIT_INFO_READ_OVERLAY))
		return false;
	*overlay = fixture->overlay;
	return true;
}

static bool
normal_exit_info_team_write_overlay(void *context, float team_id,
    const struct yt_sector *overlay, struct yt_error *error)
{
	struct normal_exit_info_context *fixture = context;

	(void)error;
	if (fixture->observation == NULL || team_id != 7.0f
	    || overlay == NULL || !normal_exit_info_effect(fixture,
	    NORMAL_EXIT_INFO_WRITE_OVERLAY))
		return false;
	fixture->observation->written_overlay = *overlay;
	fixture->observation->overlay_written = true;
	return true;
}

static bool
normal_exit_info_team_present(void *context, const uint8_t *text,
    size_t length, struct yt_error *error)
{
	struct normal_exit_info_context *fixture = context;
	struct viewer_pager_join *join = &fixture->viewer->join;
	struct yt_present_result result;

	(void)error;
	if (!normal_exit_info_effect(fixture, NORMAL_EXIT_INFO_TEAM_ROW)
	    || yt_present_line(text, length, &join->presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	return true;
}

static bool
normal_exit_info_team(void *context, struct yt_error *error)
{
	static const struct yt_info_team_ops promotion_ops = {
		normal_exit_info_team_read,
		normal_exit_info_team_store_id,
		normal_exit_info_team_store_captain,
		normal_exit_info_team_load,
		normal_exit_info_team_read_overlay,
		normal_exit_info_team_write_overlay,
		normal_exit_info_team_present,
	};
	static const uint8_t none[] = "Team  : None";
	struct normal_exit_info_context *fixture = context;
	struct viewer_pager_join *join = &fixture->viewer->join;
	struct yt_present_result result;

	if (fixture->team_route != NORMAL_EXIT_INFO_TEAM_NONE) {
		if (fixture->observation == NULL)
			return false;
		memset(&fixture->observation->team, 0,
		    sizeof(fixture->observation->team));
		fixture->observation->team.current_record = 2.0f;
		(void)qb_mbf32_encode(2.0f,
		    fixture->observation->team.current_record_raw);
		fixture->observation->team.sector_offset = 52.0f;
		return yt_info_team_resolver_run(&fixture->observation->team,
		    &promotion_ops, fixture, error);
	}
	(void)error;
	if (yt_present_line(none, sizeof(none) - 1U, &join->presentation,
	    &result) != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (yt_present_line(NULL, 0U, &join->presentation,
	    &result) != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	return true;
}

static bool
normal_exit_info_read(void *context, struct yt_player *player,
    struct yt_error *error)
{
	struct normal_exit_info_context *fixture = context;

	(void)error;
	if (fixture->observation != NULL
	    && !normal_exit_info_effect(fixture,
	    NORMAL_EXIT_INFO_READ_FINAL))
		return false;
	*player = fixture->player;
	return true;
}

static bool
normal_exit_info_present(void *context, const uint8_t *text, size_t length,
    enum yt_info_panel_output_kind kind, float width,
    struct yt_info_panel_state *state, struct yt_error *error)
{
	struct normal_exit_info_context *fixture = context;
	struct viewer_pager_join *join = &fixture->viewer->join;
	struct yt_present_result result;
	enum yt_present_status status;
	uint8_t mutable[256];
	size_t mutable_length = length;

	(void)error;
	join->presentation.foreground = state->foreground;
	join->presentation.background = state->background;
	join->presentation.bold = state->bold;
	join->pager.foreground = (int)state->foreground;
	if (kind == YT_INFO_PANEL_LINE)
		status = yt_present_line(text, length, &join->presentation, &result);
	else if (kind == YT_INFO_PANEL_FIXED && length <= sizeof(mutable)
	    && (text != NULL || length == 0U)) {
		if (length != 0U)
			memcpy(mutable, text, length);
		status = yt_present_fixed_width(mutable, &mutable_length,
		    sizeof(mutable), width, &join->presentation, &result);
	}
	else
		return false;
	state->foreground = join->presentation.foreground;
	state->background = join->presentation.background;
	state->bold = join->presentation.bold;
	if (status != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	return true;
}

struct normal_exit_info_values {
	struct yt_player player;
	const uint8_t *cached_name;
	size_t cached_name_length;
	struct normal_exit_info_observation *observation;
	bool refresh_due;
	enum normal_exit_info_team_fixture_route team_route;
};

static struct normal_exit_info_values
normal_exit_info_values_fixture(void)
{
	static const uint8_t name[] = "Pilot";
	struct normal_exit_info_values values;

	memset(&values, 0, sizeof(values));
	values.player.credits = 12345.0f;
	values.player.sector = 733.0f;
	values.player.turns = 42.0f;
	values.player.holds = 20.0f;
	values.player.fighters = 1000.0f;
	values.player.ore = 3.0f;
	values.player.mines = 4.0f;
	values.player.organics = 5.0f;
	values.player.missiles = 6.0f;
	values.player.equipment = 7.0f;
	values.player.danger_scanner = 1.0f;
	values.player.ports_owned = 8.0f;
	values.player.shields = 90.0f;
	values.player.cloak = 0.75f;
	values.player.ground_forces = 9.0f;
	values.player.plasma = 10.0f;
	values.cached_name = name;
	values.cached_name_length = sizeof(name) - 1U;
	return values;
}

static bool
normal_exit_info_run(struct physical_viewer_join *viewer,
    const uint8_t *time_text, size_t time_length,
    const struct normal_exit_info_values *values)
{
	static const struct yt_info_panel_ops ops = {
		normal_exit_info_refresh,
		normal_exit_info_team,
		normal_exit_info_read,
		normal_exit_info_present,
	};
	static const uint8_t name[] = "Pilot";
	struct normal_exit_info_context fixture;
	struct yt_info_panel_state panel;
	bool ok;

	memset(&fixture, 0, sizeof(fixture));
	memset(&panel, 0, sizeof(panel));
	fixture.viewer = viewer;
	fixture.time_text = time_text;
	fixture.time_length = time_length;
	if (values == NULL) {
		fixture.player.credits = 12345.0f;
		fixture.player.sector = 733.0f;
		fixture.player.turns = 42.0f;
		fixture.player.holds = 20.0f;
		fixture.player.fighters = 1000.0f;
		fixture.player.ore = 3.0f;
		fixture.player.mines = 4.0f;
		fixture.player.organics = 5.0f;
		fixture.player.missiles = 6.0f;
		fixture.player.equipment = 7.0f;
		fixture.player.danger_scanner = 1.0f;
		fixture.player.ports_owned = 8.0f;
		fixture.player.shields = 90.0f;
		fixture.player.cloak = 0.75f;
		fixture.player.ground_forces = 9.0f;
		fixture.player.plasma = 10.0f;
		panel.cached_name = name;
		panel.cached_name_length = sizeof(name) - 1U;
	}
	else {
		fixture.player = values->player;
		fixture.observation = values->observation;
		fixture.refresh_due = values->refresh_due;
		fixture.team_route = values->team_route;
		panel.cached_name = values->cached_name;
		panel.cached_name_length = values->cached_name_length;
	}
	if (fixture.refresh_due) {
		fixture.time.deadline = 1000.0f;
		fixture.time.next_refresh = 50.0f;
		if (time_text == NULL || time_length > sizeof(fixture.time.text))
			return false;
		if (time_length != 0U)
			memcpy(fixture.time.text, time_text, time_length);
		fixture.time.text_length = time_length;
	}
	if (fixture.team_route != NORMAL_EXIT_INFO_TEAM_NONE) {
		static const uint8_t team_name[] = "Raiders";
		static const uint8_t valid_captain_name[] = "LongCaptainName";
		static const uint8_t stale_captain_name[] = "Wrong Team";

		fixture.team_current.team = 7.0f;
		(void)yt_record_set_number(&fixture.team_current.record, YT_F89,
		    7.0f);
		fixture.team.id = 7;
		memcpy(fixture.team.name, team_name, sizeof(team_name) - 1U);
		fixture.team.name_length = sizeof(team_name) - 1U;
		fixture.team.captain = fixture.team_route
		    == NORMAL_EXIT_INFO_TEAM_SELF ? 2.0f : 3.0f;
		if (fixture.team_route == NORMAL_EXIT_INFO_TEAM_OTHER) {
			memcpy(fixture.team_captain.name, valid_captain_name,
			    sizeof(valid_captain_name) - 1U);
			fixture.team_captain.name_length = 4.0f;
			fixture.team_captain.team = 7.0f;
		}
		else if (fixture.team_route
		    == NORMAL_EXIT_INFO_TEAM_PROMOTION) {
			memcpy(fixture.team_captain.name, stale_captain_name,
			    sizeof(stale_captain_name) - 1U);
			fixture.team_captain.name_length =
			    (float)(sizeof(stale_captain_name) - 1U);
			fixture.team_captain.team = 8.0f;
		}
		memset(fixture.overlay.record.bytes, 0xa5,
		    sizeof(fixture.overlay.record.bytes));
		(void)yt_record_set_number(&fixture.team.overlay.record, YT_F77,
		    fixture.team.captain);
	}
	panel.foreground = viewer->join.presentation.foreground;
	panel.background = viewer->join.presentation.background;
	panel.bold = viewer->join.presentation.bold;
	ok = yt_info_panel_run(&panel, &ops, &fixture, NULL);
	viewer->join.presentation.foreground = panel.foreground;
	viewer->join.presentation.background = panel.background;
	viewer->join.presentation.bold = panel.bold;
	viewer->join.pager.foreground = (int)panel.foreground;
	return ok;
}

static bool
normal_exit_line(struct viewer_pager_join *join, const uint8_t *text,
    size_t length)
{
	struct yt_present_result result;

	if (yt_present_line(text, length, &join->presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	return true;
}

static bool
normal_exit_b05d(struct viewer_pager_join *join, const uint8_t *text,
    size_t length, float newline_flag)
{
	join->pager.newline_flag = newline_flag;
	return yt_paged_row_run(&join->pager, &join->presentation,
	    &join->key_state, text, length, &viewer_pager_ops, join);
}

static bool
computer_newspaper_full_cycle_run(struct physical_viewer_join *viewer,
    struct yt_file_viewer_stream_state *stream, bool ansi, uint8_t choice)
{
	static const uint8_t prompt[] =
	    "Time: 14:59  Computer command (?=help)? ";
	static const uint8_t command[] = "8";
	struct viewer_pager_join *join = &viewer->join;
	struct yt_present_result result;

	join->presentation = state(ansi);
	join->presentation.foreground = 6.0f;
	join->presentation.cached_foreground = ansi ? 6.0f : 0.0f;
	join->pager.foreground = 6;
	join->pager.line_count = 0.0f;
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	join->presentation.foreground = 1.0f;
	join->pager.foreground = 1;
	if (!normal_exit_b05d(join, prompt, sizeof(prompt) - 1U, 1.0f))
		return false;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	memcpy(join->accumulator, command, sizeof(command));
	if (yt_present_editor_echo(command, sizeof(command) - 1U,
	    command, sizeof(command) - 1U, &join->presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U)
	    || !newspaper_viewer_run(viewer, stream, choice)
	    || !normal_exit_line(join, NULL, 0U))
		return false;
	join->presentation.foreground = 1.0f;
	join->pager.foreground = 1;
	return normal_exit_b05d(join, prompt, sizeof(prompt) - 1U, 1.0f);
}

static void
test_computer_newspaper_full_cycle_presentation(void)
{
	static const uint8_t prompt[] =
	    "Time: 14:59  Computer command (?=help)? ";
	static const struct {
		bool ansi;
		uint8_t choice;
		const uint8_t *fixture;
		size_t fixture_length;
		const char *path;
		size_t lines;
		size_t remote_length;
		uint64_t remote_fnv;
		float final_bold;
	} cases[] = {
		{true, 'T', retained_current_news,
		    sizeof(retained_current_news) - 1U, "ytnews.dat", 9U,
		    732U, UINT64_C(0x7dec74d285edb57f), 0.0f},
		{false, 'T', retained_current_news,
		    sizeof(retained_current_news) - 1U, "ytnews.dat", 9U,
		    610U, UINT64_C(0x93efb9289cdb699d), 1.0f},
		{true, 'Y', retained_yesterday_news,
		    sizeof(retained_yesterday_news) - 1U, "YTYNEWS.DAT", 7U,
		    515U, UINT64_C(0x19b0435fd8e224af), 0.0f},
		{false, 'Y', retained_yesterday_news,
		    sizeof(retained_yesterday_news) - 1U, "YTYNEWS.DAT", 7U,
		    485U, UINT64_C(0x45869dac22db0a0f), 0.0f},
	};
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	uint8_t remote[800];
	size_t pass;

	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		memset(&viewer, 0, sizeof(viewer));
		fixture_viewer_initialize(&viewer, &stream,
		    cases[pass].fixture, cases[pass].fixture_length,
		    cases[pass].path, cases[pass].ansi, remote, sizeof(remote));
		CHECK(computer_newspaper_full_cycle_run(&viewer, &stream,
		    cases[pass].ansi, cases[pass].choice));
		CHECK(viewer.join.remote_length == cases[pass].remote_length
		    && viewer_bytes_fnv1a64(remote, viewer.join.remote_length)
		    == cases[pass].remote_fnv
		    && viewer.join.presentation.foreground == 1.0f
		    && viewer.join.presentation.background == 0.0f
		    && viewer.join.presentation.bold == cases[pass].final_bold
		    && viewer.join.presentation.blink == 0.0f
		    && viewer.join.presentation.cached_foreground
		    == (cases[pass].ansi ? 1.0f : 0.0f)
		    && viewer.join.pager.foreground == 1
		    && viewer.join.pager.line_count == 1.0f
		    && viewer.join.pager.nonstop == 0.0f
		    && viewer.join.pager.key[0] == '\0'
		    && viewer.join.accumulator[0] == (char)cases[pass].choice
		    && viewer.join.accumulator[1] == '\0'
		    && viewer.join.queue_length == 0U
		    && viewer.join.source_length == sizeof(prompt) - 1U
		    && memcmp(viewer.join.source, prompt,
		    sizeof(prompt) - 1U) == 0
		    && viewer.join.local_fragment_length
		    == sizeof(prompt) - 1U
		    && memcmp(viewer.join.local_fragment, prompt,
		    sizeof(prompt) - 1U) == 0);
		CHECK(viewer.join.position == cases[pass].lines
		    && stream.eof_checks == cases[pass].lines + 1U
		    && stream.key_checks == cases[pass].lines + 1U
		    && stream.read_count == cases[pass].lines
		    && stream.line_count == cases[pass].lines
		    && !stream.file_open && !viewer.join.file_open
		    && viewer.input.file == NULL && viewer.close_calls == 2U
		    && viewer.open_calls == 1U);
		yt_text_input_destroy(&viewer.input);
	}
}

static bool
computer_newspaper_low_time_cycle_run(struct physical_viewer_join *viewer,
    struct yt_file_viewer_stream_state *stream, bool ansi,
    bool invalid_first, size_t *warning_count)
{
	static const uint8_t selector_prompt[] =
	    "Do you want to read [T]oday's or [Y]esterday's news? [T/Y] -=> ";
	static const uint8_t computer_prompt[] =
	    "Time:5.9:00Computer command (?=help)? ";
	static const uint8_t time_text[] = "5.9:00";
	static const uint8_t invalid[] = "X";
	static const uint8_t accepted[] = "T";
	struct viewer_pager_join *join = &viewer->join;
	struct yt_present_result result;
	float remembered = 6.0f;
	size_t attempt;
	size_t attempts = invalid_first ? 2U : 1U;

	if (warning_count == NULL)
		return false;
	*warning_count = 0U;
	join->presentation = state(ansi);
	join->presentation.foreground = 1.0f;
	join->presentation.cached_foreground = ansi ? 1.0f : 0.0f;
	join->pager.foreground = 1;
	join->pager.line_count = 0.0f;
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	for (attempt = 0U; attempt < attempts; ++attempt) {
		const uint8_t *response = invalid_first && attempt == 0U
		    ? invalid : accepted;
		bool warned;

		if (yt_present_low_time(time_text, sizeof(time_text) - 1U,
		    &remembered, &join->presentation, &result, &warned)
		    != YT_PRESENT_OK)
			return false;
		viewer_pager_capture_result(join, &result);
		if (!warned)
			return false;
		++*warning_count;
		if (!normal_exit_b05d(join, selector_prompt,
		    sizeof(selector_prompt) - 1U, 1.0f))
			return false;
		yt_pager_editor_enter(&join->pager, join->accumulator,
		    sizeof(join->accumulator));
		join->accumulator[0] = (char)response[0];
		join->accumulator[1] = '\0';
		if (yt_present_editor_echo(response, 1U, response, 1U,
		    &join->presentation, &result) != YT_PRESENT_OK)
			return false;
		viewer_pager_capture_result(join, &result);
		if (!normal_exit_line(join, NULL, 0U))
			return false;
	}
	stream->play.saved_foreground = join->presentation.foreground;
	stream->play.saved_pager_foreground = join->pager.foreground;
	if (!physical_viewer_run(viewer, stream, NULL)
	    || !normal_exit_line(join, NULL, 0U))
		return false;
	join->presentation.foreground = 1.0f;
	join->pager.foreground = 1;
	{
		bool warned;

		if (yt_present_low_time(time_text, sizeof(time_text) - 1U,
		    &remembered, &join->presentation, &result, &warned)
		    != YT_PRESENT_OK)
			return false;
		viewer_pager_capture_result(join, &result);
		if (!warned)
			return false;
		++*warning_count;
	}
	return normal_exit_b05d(join, computer_prompt,
	    sizeof(computer_prompt) - 1U, 1.0f);
}

static void
test_computer_newspaper_low_time_cycles(void)
{
	static const uint8_t final_prompt[] =
	    "Time:5.9:00Computer command (?=help)? ";
	static const struct {
		bool ansi;
		bool invalid_first;
		size_t remote_length;
		uint64_t remote_fnv;
		size_t warnings;
	} cases[] = {
		{true, false, 779U, UINT64_C(0x4f0cb6d503683993), 2U},
		{true, true, 892U, UINT64_C(0x28902205334d357b), 3U},
		{false, false, 609U, UINT64_C(0x00b4ea7e7026e215), 2U},
		{false, true, 698U, UINT64_C(0xdeb55704ed9ce68d), 3U},
	};
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	uint8_t remote[1000];
	size_t pass;

	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		size_t warnings;

		memset(&viewer, 0, sizeof(viewer));
		fixture_viewer_initialize(&viewer, &stream,
		    retained_current_news, sizeof(retained_current_news) - 1U,
		    "ytnews.dat", cases[pass].ansi, remote, sizeof(remote));
		CHECK(computer_newspaper_low_time_cycle_run(&viewer, &stream,
		    cases[pass].ansi, cases[pass].invalid_first, &warnings));
		CHECK(viewer.join.remote_length == cases[pass].remote_length
		    && viewer_bytes_fnv1a64(remote, viewer.join.remote_length)
		    == cases[pass].remote_fnv
		    && warnings == cases[pass].warnings
		    && viewer.join.pager.line_count == 1.0f
		    && viewer.join.pager.foreground == 1
		    && viewer.join.presentation.foreground == 5.0f
		    && viewer.join.source_length == sizeof(final_prompt) - 1U
		    && memcmp(viewer.join.source, final_prompt,
		    sizeof(final_prompt) - 1U) == 0
		    && strcmp(viewer.join.accumulator, "T") == 0);
		yt_text_input_destroy(&viewer.input);
	}
}

static bool
computer_newspaper_command_queue_cycle_run(
    struct physical_viewer_join *viewer,
    struct yt_file_viewer_stream_state *stream, bool ansi)
{
	static const uint8_t prompt[] =
	    "Time: 14:59  Computer command (?=help)? ";
	static const uint8_t typed_command[] = "8;t";
	uint8_t selector[4];
	size_t selector_length = 0U;
	struct viewer_pager_join *join = &viewer->join;
	struct yt_present_result result;
	struct yt_input_value selected;

	join->presentation = state(ansi);
	join->presentation.foreground = 6.0f;
	join->presentation.cached_foreground = ansi ? 6.0f : 0.0f;
	join->pager.foreground = 6;
	join->pager.line_count = 0.0f;
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	join->presentation.foreground = 1.0f;
	join->pager.foreground = 1;
	if (!normal_exit_b05d(join, prompt, sizeof(prompt) - 1U, 1.0f))
		return false;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	memcpy(join->accumulator, typed_command, sizeof(typed_command));
	if (!yt_input_split_semicolon(join->accumulator, join->queue,
	    sizeof(join->queue), &join->queue_position, &join->queue_length)
	    || strcmp(join->accumulator, "8") != 0
	    || join->queue_length != 2U
	    || memcmp(join->queue, "t\r", 2U) != 0
	    || yt_present_editor_echo(typed_command,
	    sizeof(typed_command) - 1U, typed_command,
	    sizeof(typed_command) - 1U, &join->presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	for (;;) {
		if (!yt_input_ab36_queue_pop(join->queue, sizeof(join->queue),
		    &join->queue_position, &join->queue_length, &selected)
		    || selected.length != 1U)
			return false;
		if (selected.bytes[0] == '\r')
			break;
		if (selector_length >= sizeof(selector))
			return false;
		selector[selector_length++] = selected.bytes[0];
	}
	if (!newspaper_viewer_typed_run(viewer, stream, selector,
	    selector_length)
	    || !normal_exit_line(join, NULL, 0U))
		return false;
	join->presentation.foreground = 1.0f;
	join->pager.foreground = 1;
	return normal_exit_b05d(join, prompt, sizeof(prompt) - 1U, 1.0f);
}

static bool
computer_newspaper_selector_queue_cycle_run(
    struct physical_viewer_join *viewer,
    struct yt_file_viewer_stream_state *stream, const uint8_t *typed,
    size_t typed_length)
{
	static const uint8_t prompt[] =
	    "Time:15:00  Computer command (?=help)? ";
	struct viewer_pager_join *join = &viewer->join;

	join->response_from_queue = true;
	if (!newspaper_viewer_typed_run(viewer, stream, typed, typed_length)
	    || !normal_exit_line(join, NULL, 0U))
		return false;
	join->presentation.foreground = 1.0f;
	join->pager.foreground = 1;
	return normal_exit_b05d(join, prompt, sizeof(prompt) - 1U, 1.0f);
}

static size_t
computer_newspaper_long_fixture(uint8_t *output, size_t capacity,
    size_t rows)
{
	size_t length = 0U;
	size_t row;

	for (row = 1U; row <= rows; ++row) {
		int written;

		if (length >= capacity)
			return 0U;
		written = snprintf((char *)output + length, capacity - length,
		    "L%02zu\r\n", row);
		if (written != 5 || (size_t)written >= capacity - length)
			return 0U;
		length += (size_t)written;
	}
	if (length >= capacity)
		return 0U;
	output[length++] = 0x1aU;
	return length;
}

static void
test_computer_newspaper_queue_cycles(void)
{
	static const uint8_t command_prompt[] =
	    "Time: 14:59  Computer command (?=help)? ";
	static const uint8_t return_prompt[] =
	    "Time:15:00  Computer command (?=help)? ";
	static const uint8_t short_typed[] = "T;Q";
	static const struct {
		const uint8_t *typed;
		size_t typed_length;
		size_t rows;
		size_t remote_length;
		uint64_t remote_fnv;
		const char *accumulator;
		const char *pager_key;
		float nonstop;
	} page_cases[] = {
		{(const uint8_t *)"T;E", 3U, 24U, 344U,
		    UINT64_C(0x71c518ba3203207c), "E", "Q", 0.0f},
		{(const uint8_t *)"T;NS", 4U, 47U, 492U,
		    UINT64_C(0x8ccd2df3b2861dc2), "NS", "NS", 1.0f},
	};
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	uint8_t remote[1000];
	uint8_t long_news[512];
	size_t long_length;
	size_t pass;

	memset(&viewer, 0, sizeof(viewer));
	fixture_viewer_initialize(&viewer, &stream, retained_current_news,
	    sizeof(retained_current_news) - 1U, "ytnews.dat", true,
	    remote, sizeof(remote));
	CHECK(computer_newspaper_command_queue_cycle_run(&viewer, &stream,
	    true));
	CHECK(viewer.join.remote_length == 734U
	    && viewer_bytes_fnv1a64(remote, viewer.join.remote_length)
	    == UINT64_C(0x31e91a8bdda8a516)
	    && strcmp(viewer.join.accumulator, "t") == 0
	    && viewer.join.queue_position == 0U
	    && viewer.join.queue_length == 0U
	    && viewer.join.source_length == sizeof(command_prompt) - 1U
	    && memcmp(viewer.join.source, command_prompt,
	    sizeof(command_prompt) - 1U) == 0);
	yt_text_input_destroy(&viewer.input);

	memset(&viewer, 0, sizeof(viewer));
	fixture_viewer_initialize(&viewer, &stream, retained_current_news,
	    sizeof(retained_current_news) - 1U, "ytnews.dat", true,
	    remote, sizeof(remote));
	CHECK(computer_newspaper_selector_queue_cycle_run(&viewer, &stream,
	    short_typed, sizeof(short_typed) - 1U));
	CHECK(viewer.join.remote_length == 678U
	    && viewer_bytes_fnv1a64(remote, viewer.join.remote_length)
	    == UINT64_C(0xa0c1baaf085eb1f8)
	    && strcmp(viewer.join.accumulator, "T") == 0
	    && viewer.join.queue_position == 0U
	    && viewer.join.queue_length == 2U
	    && memcmp(viewer.join.queue, "Q\r", 2U) == 0
	    && viewer.join.source_length == sizeof(return_prompt) - 1U
	    && memcmp(viewer.join.source, return_prompt,
	    sizeof(return_prompt) - 1U) == 0);
	yt_text_input_destroy(&viewer.input);

	for (pass = 0U; pass < YT_ARRAY_LEN(page_cases); ++pass) {
		long_length = computer_newspaper_long_fixture(long_news,
		    sizeof(long_news), page_cases[pass].rows);
		CHECK(long_length != 0U);
		memset(&viewer, 0, sizeof(viewer));
		fixture_viewer_initialize(&viewer, &stream, long_news,
		    long_length, "ytnews.dat", true, remote, sizeof(remote));
		CHECK(computer_newspaper_selector_queue_cycle_run(&viewer,
		    &stream, page_cases[pass].typed,
		    page_cases[pass].typed_length));
		CHECK(viewer.join.remote_length == page_cases[pass].remote_length
		    && viewer_bytes_fnv1a64(remote, viewer.join.remote_length)
		    == page_cases[pass].remote_fnv
		    && strcmp(viewer.join.accumulator,
		    page_cases[pass].accumulator) == 0
		    && strcmp(viewer.join.pager.key,
		    page_cases[pass].pager_key) == 0
		    && viewer.join.pager.nonstop == page_cases[pass].nonstop
		    && viewer.join.queue_position == 0U
		    && viewer.join.queue_length == 0U
		    && viewer.join.pager.line_count == 1.0f);
		yt_text_input_destroy(&viewer.input);
	}
}

struct computer_newspaper_missing_join {
	struct physical_viewer_join *viewer;
	uint8_t appended[96];
	size_t appended_length;
	size_t append_calls;
	bool fail_append;
	bool persisted;
};

static bool
computer_newspaper_missing_present(void *context, const uint8_t *text,
    size_t length, bool paged, struct yt_error *error)
{
	struct computer_newspaper_missing_join *missing = context;
	struct viewer_pager_join *join = &missing->viewer->join;

	(void)error;
	return paged && normal_exit_line(join, NULL, 0U)
	    && normal_exit_b05d(join, text, length, 0.0f);
}

static bool
computer_newspaper_missing_append(void *context, const uint8_t *text,
    size_t length, struct yt_error *error)
{
	struct computer_newspaper_missing_join *missing = context;

	(void)error;
	++missing->append_calls;
	if (length > sizeof(missing->appended))
		return false;
	if (length != 0U)
		memcpy(missing->appended, text, length);
	missing->appended_length = length;
	if (missing->fail_append)
		return false;
	missing->persisted = true;
	return true;
}

static bool
computer_newspaper_missing_cycle_run(struct physical_viewer_join *viewer,
    struct yt_file_viewer_stream_state *stream, uint8_t choice,
    bool fail_append, struct computer_newspaper_missing_join *missing,
    size_t *body_end)
{
	static const uint8_t computer_prompt[] =
	    "Time:15:00  Computer command (?=help)? ";
	const char *path = choice == 'T' ? "ytnews.dat" : "YTYNEWS.DAT";
	struct viewer_pager_join *join = &viewer->join;
	struct yt_main_error_result handler;
	struct yt_present_result result;
	bool recovered;

	if (missing == NULL || body_end == NULL)
		return false;
	memset(missing, 0, sizeof(*missing));
	missing->viewer = viewer;
	missing->fail_append = fail_append;
	if (newspaper_viewer_typed_run(viewer, stream, &choice, 1U)
	    || viewer->open_calls != 1U || viewer->close_calls != 1U
	    || viewer->input.file != NULL)
		return false;
	if (!yt_main_error_compose(53, 40000, (const uint8_t *)path,
	    strlen(path), NULL, 0U, NULL, 0U, &handler)
	    || handler.route != YT_MAIN_ERROR_MISSING_FILE
	    || yt_present_forced_local_line(handler.debug,
	    handler.debug_length, &result) != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	recovered = yt_file_viewer_missing((const uint8_t *)path,
	    strlen(path), computer_newspaper_missing_present,
	    computer_newspaper_missing_append, missing, NULL);
	if (recovered == fail_append || missing->append_calls != 1U)
		return false;
	*body_end = join->remote_length;
	if (fail_append)
		return true;
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	join->presentation.foreground = 1.0f;
	join->pager.foreground = 1;
	return normal_exit_b05d(join, computer_prompt,
	    sizeof(computer_prompt) - 1U, 1.0f);
}

static void
test_computer_newspaper_missing_recovery_cycles(void)
{
	static const uint8_t debug[] =
	    "YT DEBUG Error Trap Entry ERL=  40000   ERR=  53 ";
	static const uint8_t today_row[] =
	    "*** GAME FILE [ytnews.dat] NOT FOUND! ***";
	static const uint8_t yesterday_row[] =
	    "*** GAME FILE [YTYNEWS.DAT] NOT FOUND! ***";
	static const uint8_t return_prompt[] =
	    "Time:15:00  Computer command (?=help)? ";
	static const struct {
		uint8_t choice;
		const char *path;
		const uint8_t *row;
		size_t row_length;
		size_t body_length;
		uint64_t body_fnv;
		size_t cycle_length;
		uint64_t cycle_fnv;
	} cases[] = {
		{'T', "ytnews.dat", today_row, sizeof(today_row) - 1U,
		    133U, UINT64_C(0xf1bafa4d6fa6dc81),
		    174U, UINT64_C(0xfa4827066307bbfa)},
		{'Y', "YTYNEWS.DAT", yesterday_row,
		    sizeof(yesterday_row) - 1U,
		    134U, UINT64_C(0xdec6c0c37ee7543b),
		    175U, UINT64_C(0xfcca9072487a3514)},
	};
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	struct computer_newspaper_missing_join missing;
	uint8_t remote[256];
	size_t body_end;
	size_t pass;

	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		size_t row;
		bool found_debug = false;

		memset(&viewer, 0, sizeof(viewer));
		fixture_viewer_initialize(&viewer, &stream, NULL, 0U,
		    cases[pass].path, true, remote, sizeof(remote));
		viewer.force_missing = true;
		CHECK(computer_newspaper_missing_cycle_run(&viewer, &stream,
		    cases[pass].choice, false, &missing, &body_end));
		for (row = 0U; row < viewer.join.local_row_count; ++row) {
			if (viewer.join.local_lengths[row]
			    == sizeof(debug) - 1U
			    && memcmp(viewer.join.local_rows[row],
			    debug, sizeof(debug) - 1U) == 0)
				found_debug = true;
		}
		CHECK(body_end == cases[pass].body_length
		    && viewer_bytes_fnv1a64(remote, body_end)
		    == cases[pass].body_fnv
		    && viewer.join.remote_length == cases[pass].cycle_length
		    && viewer_bytes_fnv1a64(remote, viewer.join.remote_length)
		    == cases[pass].cycle_fnv
		    && missing.persisted
		    && missing.appended_length == cases[pass].row_length
		    && memcmp(missing.appended, cases[pass].row,
		    cases[pass].row_length) == 0
		    && found_debug
		    && viewer.join.pager.line_count == 2.0f
		    && viewer.join.source_length == sizeof(return_prompt) - 1U
		    && memcmp(viewer.join.source, return_prompt,
		    sizeof(return_prompt) - 1U) == 0);
		yt_text_input_destroy(&viewer.input);
	}

	memset(&viewer, 0, sizeof(viewer));
	fixture_viewer_initialize(&viewer, &stream, NULL, 0U,
	    "YTYNEWS.DAT", true, remote, sizeof(remote));
	viewer.force_missing = true;
	CHECK(computer_newspaper_missing_cycle_run(&viewer, &stream, 'Y',
	    true, &missing, &body_end));
	CHECK(body_end == 134U && viewer.join.remote_length == 134U
	    && viewer_bytes_fnv1a64(remote, viewer.join.remote_length)
	    == UINT64_C(0xdec6c0c37ee7543b)
	    && !missing.persisted && missing.append_calls == 1U
	    && missing.appended_length == sizeof(yesterday_row) - 1U
	    && memcmp(missing.appended, yesterday_row,
	    sizeof(yesterday_row) - 1U) == 0
	    && viewer.join.pager.line_count == 1.0f
	    && viewer.join.source_length == sizeof(yesterday_row) - 1U
	    && memcmp(viewer.join.source, yesterday_row,
	    sizeof(yesterday_row) - 1U) == 0);
	yt_text_input_destroy(&viewer.input);
}

static void
test_computer_newspaper_carrier_prefixes(void)
{
	static const struct {
		bool ansi;
		size_t fail_at;
		size_t remote_length;
		uint64_t remote_fnv;
	} cases[] = {
		{true, 1U, 2U, UINT64_C(0x083cb407b4f40f36)},
		{true, 4U, 65U, UINT64_C(0x57e46fef5a7815db)},
		{true, 14U, 162U, UINT64_C(0xb9b614179171fb96)},
		{false, 14U, 152U, UINT64_C(0x029ee90b87a0e090)},
	};
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	uint8_t remote[200];
	size_t pass;

	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		memset(&viewer, 0, sizeof(viewer));
		fixture_viewer_initialize(&viewer, &stream,
		    retained_current_news, sizeof(retained_current_news) - 1U,
		    "ytnews.dat", cases[pass].ansi, remote, sizeof(remote));
		viewer.join.fail_at = cases[pass].fail_at;
		CHECK(!newspaper_viewer_run(&viewer, &stream, 'T')
		    && viewer.join.remote_length == cases[pass].remote_length
		    && viewer_bytes_fnv1a64(remote, viewer.join.remote_length)
		    == cases[pass].remote_fnv
		    && viewer.join.pager.line_count == 0.0f);
		yt_text_input_destroy(&viewer.input);
	}
}

static void
test_computer_newspaper_terminal_presentation(void)
{
	static const uint8_t prompt[] =
	    "Do you want to read [T]oday's or [Y]esterday's news? [T/Y] -=> ";
	static const uint8_t inactivity[] =
	    "\r\nDo you want to read [T]oday's or [Y]esterday's news? [T/Y] -=> "
	    "\r\n\aUSER FELL ASLEEP!\n\r";
	static const uint8_t inactivity_carrier[] =
	    "\r\nDo you want to read [T]oday's or [Y]esterday's news? [T/Y] -=> "
	    "\r\n";
	static const uint8_t session_limit[] =
	    "\r\nDo you want to read [T]oday's or [Y]esterday's news? [T/Y] -=> "
	    "\r\n\a\a\aTIME LIMIT EXCEEDED!\a\a\a\n\r";
	static const uint8_t session_carrier[] =
	    "\r\nDo you want to read [T]oday's or [Y]esterday's news? [T/Y] -=> "
	    "\r\n\a\a\aTIME LIMIT EXCEEDED!\a\a\a";
	static const uint8_t direct_carrier[] =
	    "\r\nDo you want to read [T]oday's or [Y]esterday's news? [T/Y] -=> ";
	static const struct {
		enum yt_ab36_terminal_kind kind;
		int carrier_failure;
		const uint8_t *expected;
		size_t expected_length;
		bool succeeds;
	} cases[] = {
		{YT_AB36_TERMINAL_INACTIVITY, 0, inactivity,
		    sizeof(inactivity) - 1U, true},
		{YT_AB36_TERMINAL_INACTIVITY, 1, inactivity_carrier,
		    sizeof(inactivity_carrier) - 1U, false},
		{YT_AB36_TERMINAL_SESSION_LIMIT, 0, session_limit,
		    sizeof(session_limit) - 1U, true},
		{YT_AB36_TERMINAL_SESSION_LIMIT, 2, session_carrier,
		    sizeof(session_carrier) - 1U, false},
	};
	size_t pass;

	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		struct yt_present_state current = state(true);
		struct yt_present_result result;
		struct yt_pager_state pager;
		struct pager_capture capture;
		struct computer_port_terminal_join join;
		char accumulator[80];
		bool running = true;
		bool terminated = false;
		bool result_ok;

		current.foreground = 1.0f;
		current.cached_foreground = 1.0f;
		memset(&pager, 0, sizeof(pager));
		pager.foreground = 1;
		memset(&capture, 0, sizeof(capture));
		memset(accumulator, 0, sizeof(accumulator));
		CHECK(yt_present_line(NULL, 0U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		pager.newline_flag = 1.0f;
		pager_fixture_b05d(&pager, &current, prompt,
		    sizeof(prompt) - 1U, &capture);
		yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
		join.current = &current;
		join.pager = &pager;
		join.capture = &capture;
		join.running = &running;
		join.terminated = &terminated;
		join.notice_carrier_failure = cases[pass].carrier_failure;
		join.closed = false;
		result_ok = yt_input_ab36_terminal_run(cases[pass].kind,
		    &running, &terminated, computer_port_terminal_notice,
		    computer_port_terminal_close, &join);
		CHECK(result_ok == cases[pass].succeeds
		    && capture.remote_length == cases[pass].expected_length
		    && memcmp(capture.remote, cases[pass].expected,
		    cases[pass].expected_length) == 0
		    && !running && terminated && join.closed
		    && pager.line_count == (cases[pass].succeeds ? 1.0f : 0.0f)
		    && pager.newline_flag == 0.0f && accumulator[0] == '\0');
	}
	{
		struct yt_present_state current = state(true);
		struct yt_present_result result;
		struct yt_pager_state pager;
		struct pager_capture capture;
		char accumulator[80];

		current.foreground = 1.0f;
		current.cached_foreground = 1.0f;
		memset(&pager, 0, sizeof(pager));
		pager.foreground = 1;
		memset(&capture, 0, sizeof(capture));
		memset(accumulator, 0, sizeof(accumulator));
		CHECK(yt_present_line(NULL, 0U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		pager.newline_flag = 1.0f;
		pager_fixture_b05d(&pager, &current, prompt,
		    sizeof(prompt) - 1U, &capture);
		yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
		CHECK(capture.remote_length == sizeof(direct_carrier) - 1U
		    && memcmp(capture.remote, direct_carrier,
		    sizeof(direct_carrier) - 1U) == 0
		    && pager.line_count == 0.0f
		    && pager.newline_flag == 0.0f);
	}
}

static bool
computer_scoreboard_full_cycle_run(struct physical_viewer_join *viewer,
    struct yt_file_viewer_stream_state *stream, bool ansi, bool updated)
{
	static const uint8_t prompt[] =
	    "Time: 14:59  Computer command (?=help)? ";
	static const uint8_t command[] = "4";
	static const uint8_t old_selector[] = "O";
	const uint8_t *selector = updated ? NULL : old_selector;
	size_t selector_length = updated ? 0U : sizeof(old_selector) - 1U;
	struct viewer_pager_join *join = &viewer->join;
	struct yt_present_result result;

	join->presentation = state(ansi);
	join->presentation.foreground = 6.0f;
	join->presentation.cached_foreground = ansi ? 6.0f : 0.0f;
	join->pager.foreground = 6;
	join->pager.line_count = 0.0f;
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	join->presentation.foreground = 1.0f;
	join->pager.foreground = 1;
	if (!normal_exit_b05d(join, prompt, sizeof(prompt) - 1U, 1.0f))
		return false;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	memcpy(join->accumulator, command, sizeof(command));
	if (yt_present_editor_echo(command, sizeof(command) - 1U,
	    command, sizeof(command) - 1U, &join->presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U)
	    || !scoreboard_viewer_run(viewer, stream, selector,
	    selector_length, updated))
		return false;
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	join->presentation.foreground = 1.0f;
	join->pager.foreground = 1;
	return normal_exit_b05d(join, prompt, sizeof(prompt) - 1U, 1.0f);
}

static void
test_computer_scoreboard_full_cycle_presentation(void)
{
	static const uint8_t prompt[] =
	    "Time: 14:59  Computer command (?=help)? ";
	static const struct {
		bool ansi;
		bool updated;
		size_t remote_length;
		uint64_t remote_fnv;
		float bold;
	} cases[] = {
		{true, false, 817U, UINT64_C(0x6bcba3c76cdb887d), 0.0f},
		{false, false, 787U, UINT64_C(0x3081c7913d682905), 0.0f},
		{true, true, 850U, UINT64_C(0x264fdc7e79d2bd67), 0.0f},
		{false, true, 820U, UINT64_C(0xc7fe339c3f4d74f5), 0.0f},
	};
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	uint8_t remote[900];
	size_t pass;

	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		memset(&viewer, 0, sizeof(viewer));
		fixture_viewer_initialize(&viewer, &stream,
		    retained_scoreboard, sizeof(retained_scoreboard) - 1U,
		    "YTSCORE.ASC", cases[pass].ansi, remote, sizeof(remote));
		CHECK(computer_scoreboard_full_cycle_run(&viewer, &stream,
		    cases[pass].ansi, cases[pass].updated));
		CHECK(viewer.join.remote_length == cases[pass].remote_length
		    && viewer_bytes_fnv1a64(remote, viewer.join.remote_length)
		    == cases[pass].remote_fnv
		    && viewer.join.presentation.foreground == 1.0f
		    && viewer.join.presentation.background == 0.0f
		    && viewer.join.presentation.bold == cases[pass].bold
		    && viewer.join.presentation.blink == 0.0f
		    && viewer.join.presentation.cached_foreground
		    == (cases[pass].ansi ? 1.0f : 0.0f)
		    && viewer.join.pager.foreground == 1
		    && viewer.join.pager.line_count == 1.0f
		    && viewer.join.pager.nonstop == 0.0f
		    && viewer.join.pager.key[0] == '\0'
		    && strcmp(viewer.join.accumulator,
		    cases[pass].updated ? "" : "O") == 0
		    && viewer.join.queue_position == 0U
		    && viewer.join.queue_length == 0U
		    && viewer.join.source_length == sizeof(prompt) - 1U
		    && memcmp(viewer.join.source, prompt,
		    sizeof(prompt) - 1U) == 0
		    && viewer.join.local_fragment_length
		    == sizeof(prompt) - 1U
		    && memcmp(viewer.join.local_fragment, prompt,
		    sizeof(prompt) - 1U) == 0);
		CHECK(viewer.join.position == 19U
		    && stream.eof_checks == 20U && stream.key_checks == 20U
		    && stream.read_count == 19U && stream.line_count == 19U
		    && !stream.file_open && !viewer.join.file_open
		    && viewer.input.file == NULL && viewer.close_calls == 2U
		    && viewer.open_calls == 1U);
		yt_text_input_destroy(&viewer.input);
	}
}

struct normal_exit_body_observation {
	size_t info_end;
	size_t post_info_end;
	size_t generating_end;
	size_t progress_end;
	size_t post_generator_end;
	size_t viewer_end;
	bool waited;
};

static bool
normal_exit_body_run_info(struct physical_viewer_join *viewer,
    struct yt_file_viewer_stream_state *stream, bool evaluation,
    const uint8_t *time_text, size_t time_length, float remembered,
    const struct normal_exit_info_values *info,
    struct normal_exit_body_observation *observation)
{
	static const uint8_t generating[] = "Generating ScoreBoard";
	static const uint8_t reminder[] =
	    "PLEASE HELP YOUR SYSOP REGISTER THIS GAME.";
	static const uint8_t returning[] = "Returning to Example BBS...";
	struct yt_present_result result;
	struct yt_timed_wait_state wait;
	bool warned;
	size_t index;

	if (observation == NULL || time_text == NULL)
		return false;
	memset(observation, 0, sizeof(*observation));
	viewer->join.presentation.foreground = 1.0f;
	viewer->join.presentation.sound.user_sound = 0.0f;
	viewer->join.presentation.sound.local_sound = 0.0f;
	viewer->join.pager.foreground = 1;
	if (!normal_exit_info_run(viewer, time_text, time_length, info))
		return false;
	observation->info_end = viewer->join.remote_length;
	if (!normal_exit_line(&viewer->join, NULL, 0U))
		return false;
	observation->post_info_end = viewer->join.remote_length;
	if (yt_present_low_time(time_text, time_length, &remembered,
	    &viewer->join.presentation, &result, &warned) != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(&viewer->join, &result);
	if (warned)
		return false;
	if (!normal_exit_b05d(&viewer->join, generating,
	    sizeof(generating) - 1U, 1.0f))
		return false;
	observation->generating_end = viewer->join.remote_length;
	for (index = 0U; index < 4U; ++index) {
		if (yt_present_character((const uint8_t *)".", 1U,
		    &viewer->join.presentation, &result) != YT_PRESENT_OK)
			return false;
		viewer_pager_capture_result(&viewer->join, &result);
	}
	observation->progress_end = viewer->join.remote_length;
	if (!normal_exit_line(&viewer->join, NULL, 0U))
		return false;
	observation->post_generator_end = viewer->join.remote_length;
	viewer->join.pager.nonstop = 1.0f;
	if (!physical_viewer_run(viewer, stream, NULL))
		return false;
	observation->viewer_end = viewer->join.remote_length;
	if (evaluation) {
		memset(&wait, 0, sizeof(wait));
		if (yt_present_attention(reminder, sizeof(reminder) - 1U,
		    &viewer->join.presentation, &result) != YT_PRESENT_OK)
			return false;
		viewer_pager_capture_result(&viewer->join, &result);
		if (!yt_timed_wait_begin(&wait, 10.0f, 0.0f)
		    || yt_timed_wait_timer(&wait, 0.0f)
		    != YT_TIMED_WAIT_CONTINUE
		    || yt_timed_wait_timer(&wait, 10.0f) != YT_TIMED_WAIT_TIMER
		    || wait.duration_cell != 10.0f || wait.timer_reads != 3U
		    || !normal_exit_line(&viewer->join, NULL, 0U))
			return false;
		observation->waited = true;
	}
	return normal_exit_b05d(&viewer->join, returning,
	    sizeof(returning) - 1U, 0.0f);
}

static bool
normal_exit_body_run(struct physical_viewer_join *viewer,
    struct yt_file_viewer_stream_state *stream, bool evaluation,
    const uint8_t *time_text, size_t time_length, float remembered,
    struct normal_exit_body_observation *observation)
{
	return normal_exit_body_run_info(viewer, stream, evaluation, time_text,
	    time_length, remembered, NULL, observation);
}

static void
test_full_normal_exit_presentation(void)
{
	static const uint8_t returning[] = "Returning to Example BBS...";
	static const uint8_t time_text[] = " 15:00  ";
	static const struct {
		bool ansi;
		bool evaluation;
		size_t info_end;
		size_t post_info_end;
		size_t generating_end;
		size_t progress_end;
		size_t post_generator_end;
		size_t viewer_end;
		size_t remote_length;
		uint64_t remote_fnv;
		size_t local_rows;
		uint64_t local_fnv;
		size_t colors;
		uint64_t color_fnv;
		size_t color_2;
		size_t color_4;
		size_t color_6;
		size_t color_7;
		size_t color_15_1;
		size_t color_30_4;
		float final_foreground;
		float final_bold;
		float final_blink;
	} cases[] = {
		{true, false, 678U, 690U, 711U, 715U, 717U, 1361U,
		    1390U, UINT64_C(0xd040452f34f501f1), 43U,
		    UINT64_C(0xe295e3155a115e93), 90U,
		    UINT64_C(0x20fdcf946f026a07), 53U, 12U, 0U, 22U,
		    3U, 0U, 1.0f, 0.0f, 0.0f},
		{true, true, 678U, 690U, 711U, 715U, 717U, 1361U,
		    1460U, UINT64_C(0x9127422a4aeb8699), 45U,
		    UINT64_C(0x4be101adfc6edb2e), 93U,
		    UINT64_C(0xc25b28ebb02d307f), 53U, 11U, 3U, 22U,
		    3U, 1U, 3.0f, 0.0f, 0.0f},
		{false, false, 602U, 604U, 625U, 629U, 631U, 1255U,
		    1284U, UINT64_C(0xa3047bee853ba1a2), 43U,
		    UINT64_C(0xe295e3155a115e93), 22U,
		    UINT64_C(0xbb8c8de9cb516c2d), 0U, 0U, 0U, 22U,
		    0U, 0U, 1.0f, 1.0f, 0.0f},
		{false, true, 602U, 604U, 625U, 629U, 631U, 1255U,
		    1330U, UINT64_C(0xe54c542459e21b0d), 45U,
		    UINT64_C(0x4be101adfc6edb2e), 22U,
		    UINT64_C(0xbb8c8de9cb516c2d), 0U, 0U, 0U, 22U,
		    0U, 0U, 3.0f, 1.0f, 1.0f},
	};
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	struct normal_exit_body_observation observation;
	uint8_t remote[1600];
	size_t pass;

	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		memset(&viewer, 0, sizeof(viewer));
		fixture_viewer_initialize(&viewer, &stream,
		    retained_scoreboard, sizeof(retained_scoreboard) - 1U,
		    "YTSCORE.ASC", cases[pass].ansi, remote, sizeof(remote));
		viewer.join.pager.line_count = 1.0f;
		CHECK(normal_exit_body_run(&viewer, &stream,
		    cases[pass].evaluation, time_text, sizeof(time_text) - 1U,
		    15.0f, &observation));
		CHECK(observation.info_end == cases[pass].info_end
		    && observation.post_info_end == cases[pass].post_info_end
		    && observation.generating_end == cases[pass].generating_end
		    && observation.progress_end == cases[pass].progress_end
		    && observation.post_generator_end
		    == cases[pass].post_generator_end
		    && observation.viewer_end == cases[pass].viewer_end
		    && observation.waited == cases[pass].evaluation);
		CHECK(viewer.join.remote_length == cases[pass].remote_length
		    && viewer_bytes_fnv1a64(remote, viewer.join.remote_length)
		    == cases[pass].remote_fnv);
		CHECK(viewer.join.local_row_count == cases[pass].local_rows
		    && viewer_rows_fnv1a64(&viewer.join)
		    == cases[pass].local_fnv
		    && viewer.join.local_fragment_length == 0U
		    && viewer.join.local_color_count == cases[pass].colors
		    && viewer_colors_fnv1a64(&viewer.join)
		    == cases[pass].color_fnv
		    && viewer_local_color_count(&viewer.join, 2, 0)
		    == cases[pass].color_2
		    && viewer_local_color_count(&viewer.join, 4, 0)
		    == cases[pass].color_4
		    && viewer_local_color_count(&viewer.join, 6, 0)
		    == cases[pass].color_6
		    && viewer_local_color_count(&viewer.join, 7, 0)
		    == cases[pass].color_7
		    && viewer_local_color_count(&viewer.join, 15, 1)
		    == cases[pass].color_15_1
		    && viewer_local_color_count(&viewer.join, 30, 4)
		    == cases[pass].color_30_4);
		CHECK(viewer.join.presentation.foreground
		    == cases[pass].final_foreground
		    && viewer.join.presentation.background == 0.0f
		    && viewer.join.presentation.bold == cases[pass].final_bold
		    && viewer.join.presentation.blink == cases[pass].final_blink
		    && viewer.join.pager.foreground == 1
		    && viewer.join.pager.line_count == 1.0f
		    && viewer.join.pager.nonstop == 1.0f
		    && viewer.join.pager.key[0] == '\0');
		CHECK(viewer.join.position == 19U
		    && stream.eof_checks == 20U && stream.key_checks == 20U
		    && stream.read_count == 19U && stream.line_count == 19U
		    && viewer.join.sample_calls == 22U
		    && viewer.join.response_calls == 0U
		    && viewer.join.direct_calls == 2U
		    && viewer.join.event_count == 110U
		    && !stream.file_open && !viewer.join.file_open
		    && viewer.input.file == NULL && viewer.close_calls == 2U
		    && viewer.open_calls == 1U
		    && viewer.join.source_length == sizeof(returning) - 1U
		    && memcmp(viewer.join.source, returning,
		    sizeof(returning) - 1U) == 0
		    && viewer.join.accumulator[0] == '\0'
		    && viewer.join.queue_length == 0U);
		yt_text_input_destroy(&viewer.input);
	}
}

static bool
direct_fighter_fatal_cycle_run(struct physical_viewer_join *viewer,
    struct yt_file_viewer_stream_state *stream, bool ansi, size_t ends[3])
{
	static const uint8_t victim[] = "VICTIM";
	static const uint8_t current_name[] = "CURRENT";
	static const uint8_t salvage_heading[] =
	    "You destroyed the ship and salvaged the following:";
	static const uint8_t mined[] = "** Sector is Mined!! **";
	static const uint8_t fatal[] = "Your ship has been destroyed!";
	static const uint8_t time_text[] = " 15:00  ";
	struct viewer_pager_join *join;
	struct normal_exit_info_values info;
	struct normal_exit_body_observation observation;
	struct yt_present_result result;
	uint8_t row[160];
	size_t row_length;

	if (viewer == NULL || stream == NULL || ends == NULL)
		return false;
	join = &viewer->join;
	join->presentation = state(ansi);
	join->presentation.foreground = 6.0f;
	join->pager.foreground = 6;
	join->pager.line_count = 0.0f;
	if (ansi) {
		if (yt_present_color(&join->presentation, &result)
		    != YT_PRESENT_OK)
			return false;
		/* The fixture enters after this inherited color is established. */
	}

	if (yt_present_sound(3.0f, &join->presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!yt_death_title_row(victim, sizeof(victim) - 1U, 1.0f,
	    row, sizeof(row), &row_length)
	    || !normal_exit_line(join, row, row_length)
	    || !normal_exit_line(join, NULL, 0U)
	    || yt_present_bold_line(salvage_heading,
	    sizeof(salvage_heading) - 1U, &join->presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U)
	    || !yt_salvage_simple_row(YT_SALVAGE_MINES, 1.0f, row,
	    sizeof(row), &row_length)
	    || !normal_exit_line(join, row, row_length)
	    || !yt_direct_fighter_mine_warning(victim,
	    sizeof(victim) - 1U, row, sizeof(row), &row_length)
	    || !normal_exit_line(join, NULL, 0U))
		return false;
	join->presentation.bold = 1.0f;
	join->presentation.blink = 1.0f;
	join->accumulator[0] = '\0';
	join->queue[0] = '\0';
	join->queue_position = 0U;
	join->queue_length = 0U;
	if (!normal_exit_b05d(join, row, row_length, 0.0f)
	    || !normal_exit_line(join, NULL, 0U))
		return false;
	join->presentation.blink = 1.0f;
	if (!normal_exit_line(join, mined, sizeof(mined) - 1U)
	    || yt_present_sound(5.0f, &join->presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	join->presentation.foreground = 3.0f;
	join->presentation.background = 0.0f;
	join->presentation.blink = 0.0f;
	if (!yt_sector_mine_explosion_row(3.0f, 1.0f, row,
	    sizeof(row), &row_length)
	    || yt_present_bold_character(row, row_length,
	    &join->presentation, &result) != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	join->presentation.background = 1.0f;
	if (!normal_exit_line(join, NULL, 0U)
	    || !yt_sector_mine_loss_row(YT_SECTOR_MINE_LOSS_MINES, 1.0f,
	    row, sizeof(row), &row_length)
	    || !normal_exit_line(join, row, row_length)
	    || !yt_sector_mine_loss_row(YT_SECTOR_MINE_LOSS_EMPTY_HOLDS,
	    1.0f, row, sizeof(row), &row_length)
	    || !normal_exit_line(join, row, row_length)
	    || yt_present_sound(2.0f, &join->presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	ends[0] = join->remote_length;

	join->presentation.foreground = 3.0f;
	join->pager.foreground = 3;
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	join->presentation.bold = 1.0f;
	join->presentation.blink = 1.0f;
	join->accumulator[0] = '\0';
	join->queue[0] = '\0';
	join->queue_position = 0U;
	join->queue_length = 0U;
	if (!normal_exit_b05d(join, fatal, sizeof(fatal) - 1U, 0.0f)
	    || yt_present_sound(3.0f, &join->presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	ends[1] = join->remote_length;

	memset(&info, 0, sizeof(info));
	info.cached_name = current_name;
	info.cached_name_length = sizeof(current_name) - 1U;
	if (!normal_exit_body_run_info(viewer, stream, false, time_text,
	    sizeof(time_text) - 1U, 15.0f, &info, &observation))
		return false;
	ends[2] = join->remote_length;
	return true;
}

static void
test_direct_fighter_fatal_cycle_presentation(void)
{
	static const struct {
		bool ansi;
		size_t ends[3];
		uint64_t remote_fnv;
	} cases[] = {
		{false, {277U, 311U, 1597U},
		    UINT64_C(0x8b6f898ced6524fb)},
		{true, {466U, 564U, 1900U},
		    UINT64_C(0x05c22c7e9ba14f68)},
	};
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	uint8_t remote[2000];
	size_t ends[3];
	size_t pass;

	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		memset(&viewer, 0, sizeof(viewer));
		fixture_viewer_initialize(&viewer, &stream,
		    retained_scoreboard, sizeof(retained_scoreboard) - 1U,
		    "YTSCORE.ASC", cases[pass].ansi, remote, sizeof(remote));
		CHECK(direct_fighter_fatal_cycle_run(&viewer, &stream,
		    cases[pass].ansi, ends));
		CHECK(memcmp(ends, cases[pass].ends, sizeof(ends)) == 0
		    && viewer.join.remote_length == cases[pass].ends[2]
		    && viewer_bytes_fnv1a64(remote, viewer.join.remote_length)
		    == cases[pass].remote_fnv
		    && viewer.join.presentation.foreground == 1.0f
		    && viewer.join.presentation.background == 0.0f
		    && viewer.join.presentation.bold
		    == (cases[pass].ansi ? 0.0f : 1.0f)
		    && viewer.join.presentation.blink
		    == (cases[pass].ansi ? 0.0f : 1.0f)
		    && viewer.join.presentation.cached_foreground
		    == (cases[pass].ansi ? 1.0f : 0.0f)
		    && viewer.join.pager.foreground == 1
		    && viewer.join.pager.line_count == 1.0f
		    && viewer.join.pager.nonstop == 1.0f
		    && viewer.join.position == 19U
		    && stream.eof_checks == 20U && stream.key_checks == 20U
		    && stream.read_count == 19U && stream.line_count == 19U
		    && !stream.file_open && !viewer.join.file_open
		    && viewer.input.file == NULL && viewer.close_calls == 2U
		    && viewer.open_calls == 1U
		    && viewer.join.queue_length == 0U);
		yt_text_input_destroy(&viewer.input);
	}
}

static bool
computer_info_cycle_run(struct physical_viewer_join *viewer, bool ansi,
    bool typeahead, size_t *front_end, size_t *info_end)
{
	static const uint8_t prompt[] =
	    "Time: 14:59  Computer command (?=help)? ";
	static const uint8_t time_text[] = " 14:59  ";
	static const uint8_t command[] = "I";
	static const uint8_t queued_command[] = "I;NEXT";
	const uint8_t *typed = typeahead ? queued_command : command;
	size_t typed_length = typeahead
	    ? sizeof(queued_command) - 1U : sizeof(command) - 1U;
	struct viewer_pager_join *join = &viewer->join;
	struct yt_present_result result;

	if (front_end == NULL || info_end == NULL)
		return false;
	join->presentation = state(ansi);
	join->presentation.foreground = 6.0f;
	join->pager.foreground = 6;
	join->pager.line_count = 0.0f;
	if (ansi) {
		if (yt_present_color(&join->presentation, &result)
		    != YT_PRESENT_OK)
			return false;
	}
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	join->presentation.foreground = 1.0f;
	join->pager.foreground = 1;
	if (!normal_exit_b05d(join, prompt, sizeof(prompt) - 1U, 1.0f))
		return false;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	memcpy(join->accumulator, typed, typed_length + 1U);
	if (typeahead
	    && (!yt_input_split_semicolon(join->accumulator, join->queue,
	    sizeof(join->queue), &join->queue_position, &join->queue_length)
	    || strcmp(join->accumulator, "I") != 0
	    || join->queue_position != 0U || join->queue_length != 5U
	    || memcmp(join->queue, "NEXT\r", 5U) != 0))
		return false;
	if (yt_present_editor_echo(typed, typed_length, typed, typed_length,
	    &join->presentation, &result) != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	*front_end = join->remote_length;
	if (!normal_exit_info_run(viewer, time_text, sizeof(time_text) - 1U,
	    NULL))
		return false;
	*info_end = join->remote_length;
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	join->presentation.foreground = 1.0f;
	join->pager.foreground = 1;
	return normal_exit_b05d(join, prompt, sizeof(prompt) - 1U, 1.0f);
}

static void
test_computer_info_cycle_presentation(void)
{
	static const uint8_t prompt[] =
	    "Time: 14:59  Computer command (?=help)? ";
	static const struct {
		bool ansi;
		bool typeahead;
		size_t front_end;
		size_t info_end;
		size_t remote_length;
		uint64_t remote_fnv;
		uint64_t local_fnv;
		size_t colors;
		uint64_t color_fnv;
		float final_bold;
	} cases[] = {
		{true, false, 55U, 733U, 785U,
		    UINT64_C(0x8650562b7840844e),
		    UINT64_C(0x8c7faf96b291d077), 44U,
		    UINT64_C(0xa24fad23efd389fe), 0.0f},
		{false, false, 45U, 647U, 689U,
		    UINT64_C(0x673801e6321068fb),
		    UINT64_C(0x8c7faf96b291d077), 2U,
		    UINT64_C(0x6d3fa4669b3587bd), 1.0f},
		{true, true, 60U, 738U, 790U,
		    UINT64_C(0x762e60ceeecd1b2c),
		    UINT64_C(0x72b2f9dcd2b14114), 44U,
		    UINT64_C(0xa24fad23efd389fe), 0.0f},
		{false, true, 50U, 652U, 694U,
		    UINT64_C(0x397fd08e06b70e37),
		    UINT64_C(0x72b2f9dcd2b14114), 2U,
		    UINT64_C(0x6d3fa4669b3587bd), 1.0f},
	};
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	uint8_t remote[900];
	size_t front_end;
	size_t info_end;
	size_t pass;

	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		memset(&viewer, 0, sizeof(viewer));
		fixture_viewer_initialize(&viewer, &stream,
		    retained_scoreboard, sizeof(retained_scoreboard) - 1U,
		    "YTSCORE.ASC", cases[pass].ansi, remote, sizeof(remote));
		CHECK(computer_info_cycle_run(&viewer, cases[pass].ansi,
		    cases[pass].typeahead, &front_end, &info_end));
		CHECK(front_end == cases[pass].front_end
		    && info_end == cases[pass].info_end
		    && viewer.join.remote_length == cases[pass].remote_length
		    && viewer_bytes_fnv1a64(remote, viewer.join.remote_length)
		    == cases[pass].remote_fnv
		    && viewer.join.local_row_count == 20U
		    && viewer_rows_fnv1a64(&viewer.join)
		    == cases[pass].local_fnv
		    && viewer.join.local_fragment_length == sizeof(prompt) - 1U
		    && memcmp(viewer.join.local_fragment, prompt,
		    sizeof(prompt) - 1U) == 0
		    && viewer.join.local_color_count == cases[pass].colors
		    && viewer_colors_fnv1a64(&viewer.join)
		    == cases[pass].color_fnv
		    && viewer.join.presentation.foreground == 1.0f
		    && viewer.join.presentation.background == 0.0f
		    && viewer.join.presentation.bold == cases[pass].final_bold
		    && viewer.join.presentation.blink == 0.0f
		    && viewer.join.presentation.cached_foreground
		    == (cases[pass].ansi ? 1.0f : 0.0f)
		    && viewer.join.pager.foreground == 1
		    && viewer.join.pager.line_count == 1.0f
		    && viewer.join.pager.nonstop == 0.0f
		    && viewer.join.pager.key[0] == '\0'
		    && strcmp(viewer.join.accumulator, "I") == 0
		    && viewer.join.queue_position == 0U
		    && viewer.join.queue_length
		    == (cases[pass].typeahead ? 5U : 0U)
		    && (!cases[pass].typeahead
		    || memcmp(viewer.join.queue, "NEXT\r", 5U) == 0)
		    && viewer.join.source_length == sizeof(prompt) - 1U
		    && memcmp(viewer.join.source, prompt,
		    sizeof(prompt) - 1U) == 0);
		CHECK(viewer.join.sample_calls == 2U
		    && viewer.join.response_calls == 0U
		    && viewer.join.direct_calls == 0U
		    && viewer.join.event_count == 10U
		    && viewer.join.position == 0U
		    && stream.eof_checks == 0U && stream.key_checks == 0U
		    && stream.read_count == 0U && stream.line_count == 0U
		    && !stream.file_open && !viewer.join.file_open
		    && viewer.input.file == NULL && viewer.close_calls == 0U
		    && viewer.open_calls == 0U);
		yt_text_input_destroy(&viewer.input);
	}
}

static bool
planet_info_cycle_run(struct physical_viewer_join *viewer, bool ansi,
    bool typeahead, const struct normal_exit_info_values *info,
    size_t *prompt_end, size_t *editor_end, size_t *info_end)
{
	static const uint8_t free_holds[] =
	    "You have 5 free cargo holds.";
	static const uint8_t prompt[] =
	    "Time: 14:59  Planet command (?=help) [A]? ";
	static const uint8_t time_text[] = " 14:59  ";
	static const uint8_t command[] = "I";
	static const uint8_t queued_command[] = "I;NEXT";
	const uint8_t *typed = typeahead ? queued_command : command;
	size_t typed_length = typeahead
	    ? sizeof(queued_command) - 1U : sizeof(command) - 1U;
	struct viewer_pager_join *join = &viewer->join;
	struct yt_present_result result;

	if (prompt_end == NULL || editor_end == NULL || info_end == NULL)
		return false;
	join->presentation = state(ansi);
	join->presentation.foreground = 6.0f;
	join->presentation.cached_foreground = ansi ? 6.0f : 0.0f;
	join->pager.foreground = 6;
	join->pager.line_count = 0.0f;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, free_holds,
	    sizeof(free_holds) - 1U, 0.0f)
	    || !normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, prompt, sizeof(prompt) - 1U, 1.0f))
		return false;
	*prompt_end = join->remote_length;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	memcpy(join->accumulator, typed, typed_length + 1U);
	if (typeahead
	    && (!yt_input_split_semicolon(join->accumulator, join->queue,
	    sizeof(join->queue), &join->queue_position, &join->queue_length)
	    || strcmp(join->accumulator, "I") != 0
	    || join->queue_position != 0U || join->queue_length != 5U
	    || memcmp(join->queue, "NEXT\r", 5U) != 0))
		return false;
	if (yt_present_editor_echo(typed, typed_length, typed, typed_length,
	    &join->presentation, &result) != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	*editor_end = join->remote_length;
	if (!normal_exit_info_run(viewer, time_text, sizeof(time_text) - 1U,
	    info))
		return false;
	*info_end = join->remote_length;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, free_holds,
	    sizeof(free_holds) - 1U, 0.0f)
	    || !normal_exit_line(join, NULL, 0U))
		return false;
	join->presentation.foreground = 6.0f;
	join->pager.foreground = 6;
	return normal_exit_b05d(join, prompt, sizeof(prompt) - 1U, 1.0f);
}

static void
test_planet_info_cycle_presentation(void)
{
	static const uint8_t prompt[] =
	    "Time: 14:59  Planet command (?=help) [A]? ";
	static const struct {
		bool ansi;
		bool typeahead;
		size_t editor_end;
		size_t info_end;
		size_t remote_length;
		uint64_t remote_fnv;
		uint64_t local_fnv;
		size_t colors;
		uint64_t color_fnv;
		float final_bold;
	} cases[] = {
		{true, false, 79U, 757U, 843U,
		    UINT64_C(0xf9979621eb64ff26),
		    UINT64_C(0xa6faad25a1d0a32f), 50U,
		    UINT64_C(0xb9e6e80a37c666a6), 0.0f},
		{false, false, 79U, 681U, 757U,
		    UINT64_C(0x24db98d57ab837d7),
		    UINT64_C(0xa6faad25a1d0a32f), 4U,
		    UINT64_C(0x01b4fd96ce8921d5), 1.0f},
		{true, true, 84U, 762U, 848U,
		    UINT64_C(0x587e2ca5225bdc76),
		    UINT64_C(0xab6ae1d072bf9950), 50U,
		    UINT64_C(0xb9e6e80a37c666a6), 0.0f},
		{false, true, 84U, 686U, 762U,
		    UINT64_C(0xb505adf4f01c2aa7),
		    UINT64_C(0xab6ae1d072bf9950), 4U,
		    UINT64_C(0x01b4fd96ce8921d5), 1.0f},
	};
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	uint8_t remote[900];
	size_t prompt_end;
	size_t editor_end;
	size_t info_end;
	size_t pass;

	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		memset(&viewer, 0, sizeof(viewer));
		fixture_viewer_initialize(&viewer, &stream,
		    retained_scoreboard, sizeof(retained_scoreboard) - 1U,
		    "YTSCORE.ASC", cases[pass].ansi, remote, sizeof(remote));
		CHECK(planet_info_cycle_run(&viewer, cases[pass].ansi,
		    cases[pass].typeahead, NULL, &prompt_end, &editor_end,
		    &info_end));
		CHECK(prompt_end == 76U && editor_end == cases[pass].editor_end
		    && info_end == cases[pass].info_end
		    && viewer.join.remote_length == cases[pass].remote_length
		    && viewer_bytes_fnv1a64(remote, viewer.join.remote_length)
		    == cases[pass].remote_fnv
		    && viewer.join.local_row_count == 24U
		    && viewer_rows_fnv1a64(&viewer.join)
		    == cases[pass].local_fnv
		    && viewer.join.local_fragment_length == sizeof(prompt) - 1U
		    && memcmp(viewer.join.local_fragment, prompt,
		    sizeof(prompt) - 1U) == 0
		    && viewer.join.local_color_count == cases[pass].colors
		    && viewer_colors_fnv1a64(&viewer.join)
		    == cases[pass].color_fnv
		    && viewer.join.presentation.foreground == 6.0f
		    && viewer.join.presentation.background == 0.0f
		    && viewer.join.presentation.bold == cases[pass].final_bold
		    && viewer.join.presentation.blink == 0.0f
		    && viewer.join.presentation.cached_foreground
		    == (cases[pass].ansi ? 6.0f : 0.0f)
		    && viewer.join.pager.foreground == 6
		    && viewer.join.pager.line_count == 2.0f
		    && viewer.join.pager.nonstop == 0.0f
		    && viewer.join.pager.key[0] == '\0'
		    && strcmp(viewer.join.accumulator, "I") == 0
		    && viewer.join.queue_position == 0U
		    && viewer.join.queue_length
		    == (cases[pass].typeahead ? 5U : 0U)
		    && (!cases[pass].typeahead
		    || memcmp(viewer.join.queue, "NEXT\r", 5U) == 0)
		    && viewer.join.source_length == sizeof(prompt) - 1U
		    && memcmp(viewer.join.source, prompt,
		    sizeof(prompt) - 1U) == 0);
		CHECK(viewer.join.sample_calls == 4U
		    && viewer.join.response_calls == 0U
		    && viewer.join.direct_calls == 0U
		    && viewer.join.event_count == 20U
		    && viewer.join.position == 0U
		    && stream.eof_checks == 0U && stream.key_checks == 0U
		    && stream.read_count == 0U && stream.line_count == 0U
		    && !stream.file_open && !viewer.join.file_open
		    && viewer.input.file == NULL && viewer.close_calls == 0U
		    && viewer.open_calls == 0U);
		yt_text_input_destroy(&viewer.input);
	}
}

static void
test_planet_info_promotion_refresh_cycle_presentation(void)
{
	static const enum normal_exit_info_effect expected_effects[] = {
		NORMAL_EXIT_INFO_READ_CURRENT,
		NORMAL_EXIT_INFO_LOAD_TEAM,
		NORMAL_EXIT_INFO_TEAM_ROW,
		NORMAL_EXIT_INFO_TEAM_ROW,
		NORMAL_EXIT_INFO_READ_CAPTAIN,
		NORMAL_EXIT_INFO_READ_OVERLAY,
		NORMAL_EXIT_INFO_WRITE_OVERLAY,
		NORMAL_EXIT_INFO_TEAM_ROW,
		NORMAL_EXIT_INFO_TEAM_ROW,
		NORMAL_EXIT_INFO_TEAM_ROW,
		NORMAL_EXIT_INFO_READ_FINAL,
	};
	static const struct {
		bool ansi;
		size_t info_end;
		size_t remote_length;
		uint64_t remote_fnv;
		size_t colors;
		uint64_t color_fnv;
		float final_bold;
	} cases[] = {
		{true, 887U, 973U, UINT64_C(0x0883189690943781),
		    55U, UINT64_C(0x8a10014347635983), 0.0f},
		{false, 811U, 887U, UINT64_C(0x19b54d3b1a6cc58c),
		    6U, UINT64_C(0xda1f2a2392998d66), 1.0f},
	};
	static const uint8_t prompt[] =
	    "Time: 14:59  Planet command (?=help) [A]? ";
	struct normal_exit_info_values info;
	struct normal_exit_info_observation observation;
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	struct yt_record expected_overlay;
	uint8_t remote[1100];
	float written_captain;
	size_t prompt_end;
	size_t editor_end;
	size_t info_end;
	size_t pass;

	info = normal_exit_info_values_fixture();
	info.refresh_due = true;
	info.team_route = NORMAL_EXIT_INFO_TEAM_PROMOTION;
	memset(expected_overlay.bytes, 0xa5, sizeof(expected_overlay.bytes));
	CHECK(yt_record_set_number(&expected_overlay, YT_F77, 2.0f));
	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		memset(&viewer, 0, sizeof(viewer));
		memset(&observation, 0, sizeof(observation));
		info.observation = &observation;
		fixture_viewer_initialize(&viewer, &stream,
		    retained_scoreboard, sizeof(retained_scoreboard) - 1U,
		    "YTSCORE.ASC", cases[pass].ansi, remote, sizeof(remote));
		CHECK(planet_info_cycle_run(&viewer, cases[pass].ansi, false,
		    &info, &prompt_end, &editor_end, &info_end));
		CHECK(prompt_end == 76U && editor_end == 79U
		    && info_end == cases[pass].info_end
		    && viewer.join.remote_length == cases[pass].remote_length
		    && viewer_bytes_fnv1a64(remote, viewer.join.remote_length)
		    == cases[pass].remote_fnv);
		CHECK(observation.timer_used == 4U && observation.time_updated
		    && observation.time.deadline == 1000.0f
		    && observation.time.next_refresh == 101.0f
		    && observation.time.saved_row == 1
		    && observation.time.saved_column == 1
		    && observation.time.text_length == 8U
		    && memcmp(observation.time.text, " 14:59  ", 8U) == 0
		    && observation.refresh.remote_length == 0U
		    && observation.refresh.event_count == 5U);
		CHECK(observation.refresh.events[0].operation
		    == YT_PRESENT_LOCAL_LOCATE
		    && observation.refresh.events[0].row == 25
		    && observation.refresh.events[0].column == 71
		    && observation.refresh.events[1].operation
		    == YT_PRESENT_LOCAL_COLOR
		    && observation.refresh.events[1].foreground == 11
		    && observation.refresh.events[1].background == 1
		    && observation.refresh.events[2].operation
		    == YT_PRESENT_LOCAL_SEMI
		    && observation.refresh.events[2].length == 8U
		    && memcmp(observation.refresh.events[2].data,
		    " 14:59  ", 8U) == 0
		    && observation.refresh.events[3].operation
		    == YT_PRESENT_LOCAL_LOCATE
		    && observation.refresh.events[3].row == 1
		    && observation.refresh.events[3].column == 1
		    && observation.refresh.events[3].cursor_visible == 1
		    && observation.refresh.events[3].cursor_start == 1
		    && observation.refresh.events[3].cursor_stop == 16
		    && observation.refresh.events[4].operation
		    == YT_PRESENT_LOCAL_COLOR
		    && observation.refresh.events[4].foreground == 7
		    && observation.refresh.events[4].background == 0);
		CHECK(observation.effect_count == YT_ARRAY_LEN(expected_effects)
		    && memcmp(observation.effects, expected_effects,
		    sizeof(expected_effects)) == 0
		    && observation.player_read_count == 2U
		    && observation.player_records[0] == 2.0f
		    && observation.player_records[1] == 3.0f
		    && observation.team.route == YT_INFO_TEAM_PROMOTED
		    && observation.team.team_id == 7.0f
		    && observation.team.captain_record == 2.0f
		    && observation.team.team.captain == 2.0f
		    && observation.team.captain_flag == 1.0f
		    && observation.team.current_is_captain
		    && observation.overlay_written);
		written_captain = yt_record_get_number(
		    &observation.written_overlay.record, YT_F77);
		CHECK(written_captain == 2.0f
		    && memcmp(observation.written_overlay.record.bytes,
		    expected_overlay.bytes, sizeof(expected_overlay.bytes)) == 0
		    && viewer.join.local_row_count == 27U
		    && viewer_rows_fnv1a64(&viewer.join)
		    == UINT64_C(0x4f8e177d59bb4b6c)
		    && viewer.join.local_fragment_length == sizeof(prompt) - 1U
		    && memcmp(viewer.join.local_fragment, prompt,
		    sizeof(prompt) - 1U) == 0
		    && viewer.join.local_color_count == cases[pass].colors
		    && viewer_colors_fnv1a64(&viewer.join)
		    == cases[pass].color_fnv
		    && viewer.join.presentation.foreground == 6.0f
		    && viewer.join.presentation.background == 0.0f
		    && viewer.join.presentation.bold == cases[pass].final_bold
		    && viewer.join.presentation.blink == 0.0f
		    && viewer.join.presentation.cached_foreground
		    == (cases[pass].ansi ? 6.0f : 0.0f)
		    && viewer.join.pager.foreground == 6
		    && viewer.join.pager.line_count == 2.0f
		    && viewer.join.queue_length == 0U
		    && viewer.join.sample_calls == 4U
		    && viewer.join.event_count == 20U
		    && viewer.join.position == 0U
		    && stream.eof_checks == 0U && stream.key_checks == 0U
		    && stream.read_count == 0U && stream.line_count == 0U
		    && !stream.file_open && !viewer.join.file_open
		    && viewer.input.file == NULL && viewer.close_calls == 0U
		    && viewer.open_calls == 0U);
		yt_text_input_destroy(&viewer.input);
	}
}

static void
test_planet_info_captain_route_cycles_presentation(void)
{
	static const enum normal_exit_info_effect self_effects[] = {
		NORMAL_EXIT_INFO_READ_CURRENT,
		NORMAL_EXIT_INFO_LOAD_TEAM,
		NORMAL_EXIT_INFO_TEAM_ROW,
		NORMAL_EXIT_INFO_TEAM_ROW,
		NORMAL_EXIT_INFO_TEAM_ROW,
		NORMAL_EXIT_INFO_TEAM_ROW,
		NORMAL_EXIT_INFO_READ_FINAL,
	};
	static const enum normal_exit_info_effect other_effects[] = {
		NORMAL_EXIT_INFO_READ_CURRENT,
		NORMAL_EXIT_INFO_LOAD_TEAM,
		NORMAL_EXIT_INFO_TEAM_ROW,
		NORMAL_EXIT_INFO_TEAM_ROW,
		NORMAL_EXIT_INFO_READ_CAPTAIN,
		NORMAL_EXIT_INFO_READ_CAPTAIN,
		NORMAL_EXIT_INFO_TEAM_ROW,
		NORMAL_EXIT_INFO_TEAM_ROW,
		NORMAL_EXIT_INFO_READ_FINAL,
	};
	static const struct {
		enum normal_exit_info_team_fixture_route route;
		bool ansi;
		size_t info_end;
		size_t remote_length;
		uint64_t remote_fnv;
		const enum normal_exit_info_effect *effects;
		size_t effect_count;
		size_t player_reads;
		uint64_t row_fnv;
		size_t colors;
		uint64_t color_fnv;
	} cases[] = {
		{NORMAL_EXIT_INFO_TEAM_SELF, true, 797U, 883U,
		    UINT64_C(0x24c6596c05f63213), self_effects,
		    YT_ARRAY_LEN(self_effects), 1U,
		    UINT64_C(0x2d651dc232f7957e), 52U,
		    UINT64_C(0x16871e44ce846c36)},
		{NORMAL_EXIT_INFO_TEAM_SELF, false, 721U, 797U,
		    UINT64_C(0x08eb41ede6b5f3b1e), self_effects,
		    YT_ARRAY_LEN(self_effects), 1U,
		    UINT64_C(0x2d651dc232f7957e), 4U,
		    UINT64_C(0x01b4fd96ce8921d5)},
		{NORMAL_EXIT_INFO_TEAM_OTHER, true, 794U, 880U,
		    UINT64_C(0x31e902e583211ace), other_effects,
		    YT_ARRAY_LEN(other_effects), 3U,
		    UINT64_C(0x40a7e9608ebb4ab4), 52U,
		    UINT64_C(0x16871e44ce846c36)},
		{NORMAL_EXIT_INFO_TEAM_OTHER, false, 718U, 794U,
		    UINT64_C(0xc02acb964bb54383), other_effects,
		    YT_ARRAY_LEN(other_effects), 3U,
		    UINT64_C(0x40a7e9608ebb4ab4), 4U,
		    UINT64_C(0x01b4fd96ce8921d5)},
	};
	static const uint8_t prompt[] =
	    "Time: 14:59  Planet command (?=help) [A]? ";
	struct normal_exit_info_values info;
	struct normal_exit_info_observation observation;
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	uint8_t remote[1000];
	size_t prompt_end;
	size_t editor_end;
	size_t info_end;
	size_t pass;

	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		memset(&viewer, 0, sizeof(viewer));
		memset(&observation, 0, sizeof(observation));
		info = normal_exit_info_values_fixture();
		info.observation = &observation;
		info.team_route = cases[pass].route;
		fixture_viewer_initialize(&viewer, &stream,
		    retained_scoreboard, sizeof(retained_scoreboard) - 1U,
		    "YTSCORE.ASC", cases[pass].ansi, remote, sizeof(remote));
		CHECK(planet_info_cycle_run(&viewer, cases[pass].ansi, false,
		    &info, &prompt_end, &editor_end, &info_end));
		CHECK(prompt_end == 76U && editor_end == 79U
		    && info_end == cases[pass].info_end
		    && viewer.join.remote_length == cases[pass].remote_length
		    && viewer_bytes_fnv1a64(remote, viewer.join.remote_length)
		    == cases[pass].remote_fnv
		    && observation.timer_used == 0U
		    && !observation.time_updated
		    && observation.refresh.event_count == 0U
		    && observation.effect_count == cases[pass].effect_count
		    && memcmp(observation.effects, cases[pass].effects,
		    cases[pass].effect_count * sizeof(cases[pass].effects[0])) == 0
		    && observation.player_read_count == cases[pass].player_reads
		    && observation.player_records[0] == 2.0f
		    && observation.team.team_id == 7.0f
		    && !observation.overlay_written);
		if (cases[pass].route == NORMAL_EXIT_INFO_TEAM_SELF) {
			CHECK(observation.team.route == YT_INFO_TEAM_SELF_CAPTAIN
			    && observation.team.current_is_captain
			    && observation.team.team.captain == 2.0f);
		}
		else {
			CHECK(observation.player_records[1] == 3.0f
			    && observation.player_records[2] == 3.0f
			    && observation.team.route
			    == YT_INFO_TEAM_OTHER_CAPTAIN
			    && !observation.team.current_is_captain
			    && observation.team.captain_record == 3.0f
			    && observation.team.captain_name_length == 4U
			    && memcmp(observation.team.captain_name,
			    "Long", 4U) == 0);
		}
		CHECK(viewer.join.local_row_count == 26U
		    && viewer_rows_fnv1a64(&viewer.join) == cases[pass].row_fnv
		    && viewer.join.local_fragment_length == sizeof(prompt) - 1U
		    && memcmp(viewer.join.local_fragment, prompt,
		    sizeof(prompt) - 1U) == 0
		    && viewer.join.local_color_count == cases[pass].colors
		    && viewer_colors_fnv1a64(&viewer.join)
		    == cases[pass].color_fnv
		    && viewer.join.presentation.foreground == 6.0f
		    && viewer.join.presentation.background == 0.0f
		    && viewer.join.presentation.bold
		    == (cases[pass].ansi ? 0.0f : 1.0f)
		    && viewer.join.presentation.blink == 0.0f
		    && viewer.join.presentation.cached_foreground
		    == (cases[pass].ansi ? 6.0f : 0.0f)
		    && viewer.join.pager.foreground == 6
		    && viewer.join.pager.line_count == 2.0f
		    && viewer.join.queue_length == 0U
		    && viewer.join.sample_calls == 4U
		    && viewer.join.event_count == 20U
		    && stream.eof_checks == 0U && stream.key_checks == 0U
		    && stream.read_count == 0U && stream.line_count == 0U
		    && !stream.file_open && !viewer.join.file_open
		    && viewer.input.file == NULL && viewer.close_calls == 0U
		    && viewer.open_calls == 0U
		    && strcmp(viewer.join.accumulator, "I") == 0
		    && viewer.join.source_length == sizeof(prompt) - 1U
		    && memcmp(viewer.join.source, prompt,
		    sizeof(prompt) - 1U) == 0);
		yt_text_input_destroy(&viewer.input);
	}
}

enum sensor_join_output_kind {
	SENSOR_JOIN_LINE,
	SENSOR_JOIN_BOLD_LINE,
	SENSOR_JOIN_RAW,
	SENSOR_JOIN_BOLD_RAW,
	SENSOR_JOIN_ATTENTION,
};

struct sensor_join_output {
	enum sensor_join_output_kind kind;
	const char *text;
};

static bool
sensor_join_present(struct viewer_pager_join *join,
    const struct sensor_join_output *output)
{
	const uint8_t *text = (const uint8_t *)output->text;
	size_t length = output->text == NULL ? 0U : strlen(output->text);
	struct yt_present_result result;
	enum yt_present_status status;

	switch (output->kind) {
	case SENSOR_JOIN_LINE:
		status = yt_present_line(text, length, &join->presentation,
		    &result);
		break;
	case SENSOR_JOIN_BOLD_LINE:
		status = yt_present_bold_line(text, length,
		    &join->presentation, &result);
		break;
	case SENSOR_JOIN_RAW:
		status = yt_present_character(text, length, &join->presentation,
		    &result);
		break;
	case SENSOR_JOIN_BOLD_RAW:
		status = yt_present_bold_character(text, length,
		    &join->presentation, &result);
		break;
	case SENSOR_JOIN_ATTENTION:
		status = yt_present_attention(text, length, &join->presentation,
		    &result);
		break;
	default:
		return false;
	}
	if (status != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	return true;
}

static bool
sensor_join_sound(struct viewer_pager_join *join)
{
	struct yt_present_result result;

	if (yt_present_sound(4.0f, &join->presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	return true;
}

static bool
planet_sensor_nonzero_cycle_run(struct physical_viewer_join *viewer,
    bool ansi, size_t *prompt_end, size_t *editor_end, size_t *sensor_end)
{
	static const uint8_t free_holds[] =
	    "You have 65 free cargo holds.";
	static const uint8_t prompt[] =
	    "Time: 14:59  Planet command (?=help) [A]? ";
	static const uint8_t command[] = "S";
	static const struct sensor_join_output output[] = {
		{SENSOR_JOIN_LINE, NULL},
		{SENSOR_JOIN_BOLD_LINE, "[ Sensors Activated ]"},
		{SENSOR_JOIN_LINE, NULL},
		{SENSOR_JOIN_LINE, "Sector: 9"},
		{SENSOR_JOIN_LINE, "Port: Relay, Selling: Equ"},
		{SENSOR_JOIN_BOLD_LINE, "Planet: Outpost * Forces: 5"},
		{SENSOR_JOIN_BOLD_LINE,
		    "You detect the shimmering of a cloaking device!"},
		{SENSOR_JOIN_BOLD_LINE, "Other Ships: "},
		{SENSOR_JOIN_LINE,
		    "    Cloaked - Fighters: 8 - Shields: 2"},
		{SENSOR_JOIN_RAW, "Warps lead to:"},
		{SENSOR_JOIN_RAW, " 9"},
		{SENSOR_JOIN_RAW, ", 42"},
		{SENSOR_JOIN_RAW, ", 7"},
		{SENSOR_JOIN_RAW, ", 9"},
		{SENSOR_JOIN_LINE, NULL},
		{SENSOR_JOIN_LINE, NULL},
		{SENSOR_JOIN_LINE, "Sector: 9"},
		{SENSOR_JOIN_LINE, "Port: Relay, Selling: Equ"},
		{SENSOR_JOIN_BOLD_LINE, "Planet: Outpost * Forces: 5"},
		{SENSOR_JOIN_BOLD_LINE, "Other Ships: "},
		{SENSOR_JOIN_LINE,
		    "    Cloaked - Fighters: 8 - Shields: 2"},
		{SENSOR_JOIN_RAW, "Warps lead to:"},
		{SENSOR_JOIN_RAW, " 9"},
		{SENSOR_JOIN_RAW, ", 42"},
		{SENSOR_JOIN_RAW, ", 7"},
		{SENSOR_JOIN_RAW, ", 9"},
		{SENSOR_JOIN_LINE, NULL},
		{SENSOR_JOIN_LINE, NULL},
		{SENSOR_JOIN_LINE, "Sector: 42"},
		{SENSOR_JOIN_BOLD_LINE, "Other Ships: "},
		{SENSOR_JOIN_LINE,
		    "    Neighbor - Team: 3 - Fighters: 5 - Shields: 6"},
		{SENSOR_JOIN_RAW, "Warps lead to:"},
		{SENSOR_JOIN_RAW, " 42"},
		{SENSOR_JOIN_RAW, ", 9"},
		{SENSOR_JOIN_LINE, NULL},
		{SENSOR_JOIN_LINE, NULL},
		{SENSOR_JOIN_BOLD_LINE, "[ Pause ]"},
		{SENSOR_JOIN_LINE, NULL},
		{SENSOR_JOIN_LINE, "Sector: 7"},
		{SENSOR_JOIN_ATTENTION,
		    "** Space-time disruption detected! **"},
		{SENSOR_JOIN_ATTENTION,
		    "** WARNING! SECTOR HAS 2 MINES! **"},
		{SENSOR_JOIN_BOLD_RAW, "Fighters in sector:"},
		{SENSOR_JOIN_LINE, " 12 (Belong to The Xannor)"},
		{SENSOR_JOIN_RAW, "Warps lead to:"},
		{SENSOR_JOIN_RAW, " 9"},
		{SENSOR_JOIN_RAW, ", 42"},
		{SENSOR_JOIN_LINE, NULL},
		{SENSOR_JOIN_LINE, NULL},
		{SENSOR_JOIN_BOLD_LINE, "[ End Sensor Scan ]"},
	};
	struct viewer_pager_join *join = &viewer->join;
	struct yt_present_result result;
	struct yt_timed_wait_state wait;
	size_t index;

	if (prompt_end == NULL || editor_end == NULL || sensor_end == NULL)
		return false;
	memset(&wait, 0, sizeof(wait));
	join->presentation = state(ansi);
	join->presentation.foreground = 6.0f;
	join->pager.foreground = 6;
	join->pager.line_count = 0.0f;
	if (ansi) {
		if (yt_present_color(&join->presentation, &result)
		    != YT_PRESENT_OK)
			return false;
	}
	else {
		join->presentation.sound.user_sound = 0.0f;
		join->presentation.sound.local_sound = 0.0f;
	}
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, free_holds,
	    sizeof(free_holds) - 1U, 0.0f)
	    || !normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, prompt, sizeof(prompt) - 1U, 1.0f))
		return false;
	*prompt_end = join->remote_length;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	memcpy(join->accumulator, command, sizeof(command));
	if (yt_present_editor_echo(command, sizeof(command) - 1U,
	    command, sizeof(command) - 1U, &join->presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	*editor_end = join->remote_length;
	for (index = 0U; index < YT_ARRAY_LEN(output); ++index) {
		if (index == 1U || index == 36U || index == 48U) {
			join->presentation.foreground = 7.0f;
			join->pager.foreground = 7;
		}
		else if (index == 2U || index == 37U) {
			join->presentation.foreground = 1.0f;
			join->pager.foreground = 1;
		}
		else if (index == 5U || index == 18U) {
			join->presentation.foreground = 3.0f;
			join->pager.foreground = 3;
		}
		if (!sensor_join_present(join, &output[index]))
			return false;
		if (index == 1U || index == 6U) {
			if (!sensor_join_sound(join))
				return false;
		}
		if (index == 5U || index == 18U) {
			join->presentation.foreground = 1.0f;
			join->pager.foreground = 1;
		}
		if (index == 36U) {
			if (!yt_timed_wait_begin(&wait, 15.0f, 0.0f)
			    || yt_timed_wait_timer(&wait, 0.0f)
			    != YT_TIMED_WAIT_CONTINUE
			    || yt_timed_wait_timer(&wait, 15.0f)
			    != YT_TIMED_WAIT_TIMER
			    || wait.duration_cell != 15.0f
			    || wait.timer_reads != 3U)
				return false;
		}
		if (index == 40U) {
			size_t sound;

			for (sound = 0U; sound < 3U; ++sound)
				if (!sensor_join_sound(join))
					return false;
		}
	}
	*sensor_end = join->remote_length;
	join->presentation.foreground = 6.0f;
	join->pager.foreground = 6;
	join->pager.line_count = 0.0f;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, free_holds,
	    sizeof(free_holds) - 1U, 0.0f)
	    || !normal_exit_line(join, NULL, 0U))
		return false;
	join->presentation.foreground = 6.0f;
	join->pager.foreground = 6;
	return normal_exit_b05d(join, prompt, sizeof(prompt) - 1U, 1.0f);
}

static void
test_planet_sensor_nonzero_cycle_presentation(void)
{
	static const uint8_t prompt[] =
	    "Time: 14:59  Planet command (?=help) [A]? ";
	static const struct {
		bool ansi;
		size_t sensor_end;
		size_t remote_length;
		uint64_t remote_fnv;
		float final_bold;
		float final_blink;
		size_t local_colors;
		uint64_t local_color_fnv;
	} cases[] = {
		{true, 1199U, 1286U, UINT64_C(0x25e89cef154445d8),
		    0.0f, 0.0f, 64U, UINT64_C(0x968cacea8669d75c)},
		{false, 753U, 830U, UINT64_C(0x3fc5a736f8f4ebf8),
		    1.0f, 1.0f, 4U, UINT64_C(0x01b4fd96ce8921d5)},
	};
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	uint8_t remote[1400];
	size_t prompt_end;
	size_t editor_end;
	size_t sensor_end;
	size_t pass;

	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		memset(&viewer, 0, sizeof(viewer));
		fixture_viewer_initialize(&viewer, &stream,
		    retained_scoreboard, sizeof(retained_scoreboard) - 1U,
		    "YTSCORE.ASC", cases[pass].ansi, remote, sizeof(remote));
		CHECK(planet_sensor_nonzero_cycle_run(&viewer, cases[pass].ansi,
		    &prompt_end, &editor_end, &sensor_end));
		CHECK(prompt_end == 77U && editor_end == 80U
		    && sensor_end == cases[pass].sensor_end
		    && viewer.join.remote_length == cases[pass].remote_length
		    && viewer_bytes_fnv1a64(remote, viewer.join.remote_length)
		    == cases[pass].remote_fnv
		    && viewer.join.presentation.foreground == 6.0f
		    && viewer.join.presentation.background == 0.0f
		    && viewer.join.presentation.bold == cases[pass].final_bold
		    && viewer.join.presentation.blink == cases[pass].final_blink
		    && viewer.join.presentation.cached_foreground
		    == (cases[pass].ansi ? 6.0f : 0.0f)
		    && viewer.join.pager.foreground == 6
		    && viewer.join.pager.line_count == 2.0f
		    && viewer.join.pager.nonstop == 0.0f
		    && viewer.join.local_row_count == 39U
		    && viewer_rows_fnv1a64(&viewer.join)
		    == UINT64_C(0x6a7469c3f7491002)
		    && viewer.join.local_fragment_length == sizeof(prompt) - 1U
		    && memcmp(viewer.join.local_fragment, prompt,
		    sizeof(prompt) - 1U) == 0
		    && viewer.join.local_color_count == cases[pass].local_colors
		    && viewer_colors_fnv1a64(&viewer.join)
		    == cases[pass].local_color_fnv
		    && strcmp(viewer.join.accumulator, "S") == 0
		    && viewer.join.queue_length == 0U
		    && viewer.join.sample_calls == 4U
		    && viewer.join.event_count == 20U
		    && stream.eof_checks == 0U && stream.key_checks == 0U
		    && stream.read_count == 0U && stream.line_count == 0U
		    && !stream.file_open && !viewer.join.file_open
		    && viewer.input.file == NULL && viewer.close_calls == 0U
		    && viewer.open_calls == 0U);
		yt_text_input_destroy(&viewer.input);
	}
}

static bool
planet_garrison_positive_cycle_run(struct physical_viewer_join *viewer,
    bool ansi, size_t *prompt_end, size_t *editor_end, size_t *body_end)
{
	static const uint8_t free_holds[] =
	    "You have 65 free cargo holds.";
	static const uint8_t planet_prompt[] =
	    "Time: 14:59  Planet command (?=help) [A]? ";
	static const uint8_t command[] = "F";
	static const uint8_t response[] = "12";
	struct viewer_pager_join *join = &viewer->join;
	struct yt_present_result result;
	uint8_t prompt[128];
	uint8_t success[128];
	size_t prompt_length;
	size_t success_length;
	float remaining;

	if (prompt_end == NULL || editor_end == NULL || body_end == NULL
	    || !yt_planet_garrison_prompt(8.0f, 10.0f, prompt,
	    sizeof(prompt), &prompt_length)
	    || !yt_planet_garrison_success_row(12.0f, success,
	    sizeof(success), &success_length))
		return false;
	remaining = yt_planet_garrison_after(8.0f, 12.0f, 10.0f);
	if (remaining != 6.0f)
		return false;
	join->presentation = state(ansi);
	join->presentation.foreground = 6.0f;
	join->presentation.cached_foreground = ansi ? 6.0f : 0.0f;
	join->pager.foreground = 6;
	join->pager.line_count = 0.0f;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, free_holds,
	    sizeof(free_holds) - 1U, 0.0f)
	    || !normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, planet_prompt,
	    sizeof(planet_prompt) - 1U, 1.0f))
		return false;
	*prompt_end = join->remote_length;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	memcpy(join->accumulator, command, sizeof(command));
	if (yt_present_editor_echo(command, sizeof(command) - 1U,
	    command, sizeof(command) - 1U, &join->presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	*editor_end = join->remote_length;
	join->presentation.foreground = 6.0f;
	join->pager.foreground = 6;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, prompt, prompt_length, 1.0f))
		return false;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	memcpy(join->accumulator, response, sizeof(response));
	if (yt_present_editor_echo(response, sizeof(response) - 1U,
	    response, sizeof(response) - 1U, &join->presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_line(join, NULL, 0U))
		return false;
	join->presentation.bold = 1.0f;
	join->presentation.blink = 1.0f;
	if (!normal_exit_b05d(join, success, success_length, 0.0f)
	    || yt_present_sound(4.0f, &join->presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	*body_end = join->remote_length;
	join->pager.line_count = 0.0f;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, free_holds,
	    sizeof(free_holds) - 1U, 0.0f)
	    || !normal_exit_line(join, NULL, 0U))
		return false;
	join->presentation.foreground = 6.0f;
	join->pager.foreground = 6;
	return normal_exit_b05d(join, planet_prompt,
	    sizeof(planet_prompt) - 1U, 1.0f);
}

static void
test_planet_garrison_positive_cycle_presentation(void)
{
	static const struct {
		bool ansi;
		size_t body_end;
		size_t remote_length;
		uint64_t remote_fnv;
		size_t local_colors;
		uint64_t local_color_fnv;
		float final_bold;
		float final_blink;
	} cases[] = {
		{true, 227U, 314U, UINT64_C(0xc81ad6a8c658de6d),
		    20U, UINT64_C(0xc1d9c09899e7628d), 0.0f, 0.0f},
		{false, 191U, 268U, UINT64_C(0x2556827aa24cfdc3),
		    6U, UINT64_C(0x17798e683d05096d), 1.0f, 1.0f},
	};
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	uint8_t remote[400];
	size_t prompt_end;
	size_t editor_end;
	size_t body_end;
	size_t pass;

	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		memset(&viewer, 0, sizeof(viewer));
		fixture_viewer_initialize(&viewer, &stream,
		    retained_scoreboard, sizeof(retained_scoreboard) - 1U,
		    "YTSCORE.ASC", cases[pass].ansi, remote, sizeof(remote));
		CHECK(planet_garrison_positive_cycle_run(&viewer,
		    cases[pass].ansi, &prompt_end, &editor_end, &body_end));
		CHECK(prompt_end == 77U && editor_end == 80U
		    && body_end == cases[pass].body_end
		    && viewer.join.remote_length == cases[pass].remote_length
		    && viewer_bytes_fnv1a64(remote, viewer.join.remote_length)
		    == cases[pass].remote_fnv
		    && viewer.join.local_row_count == 11U
		    && viewer_rows_fnv1a64(&viewer.join)
		    == UINT64_C(0x7729be9f3d4ebc27)
		    && viewer.join.local_fragment_length == 42U
		    && memcmp(viewer.join.local_fragment,
		    "Time: 14:59  Planet command (?=help) [A]? ", 42U) == 0
		    && viewer.join.local_color_count == cases[pass].local_colors
		    && viewer_colors_fnv1a64(&viewer.join)
		    == cases[pass].local_color_fnv
		    && viewer.join.presentation.foreground == 6.0f
		    && viewer.join.presentation.background == 0.0f
		    && viewer.join.presentation.bold == cases[pass].final_bold
		    && viewer.join.presentation.blink == cases[pass].final_blink
		    && viewer.join.presentation.cached_foreground
		    == (cases[pass].ansi ? 6.0f : 0.0f)
		    && viewer.join.pager.foreground == 6
		    && viewer.join.pager.line_count == 2.0f
		    && viewer.join.pager.nonstop == 0.0f
		    && strcmp(viewer.join.accumulator, "12") == 0
		    && viewer.join.queue_length == 0U
		    && viewer.join.sample_calls == 6U
		    && viewer.join.event_count == 30U
		    && stream.eof_checks == 0U && stream.key_checks == 0U
		    && stream.read_count == 0U && stream.line_count == 0U
		    && !stream.file_open && !viewer.join.file_open
		    && viewer.input.file == NULL && viewer.close_calls == 0U
		    && viewer.open_calls == 0U);
		yt_text_input_destroy(&viewer.input);
	}
}

static bool
planet_bank_cancel_cycle_run(struct physical_viewer_join *viewer, bool ansi,
    size_t *prompt_end, size_t *editor_end, size_t *body_end)
{
	static const uint8_t free_holds[] =
	    "You have 65 free cargo holds.";
	static const uint8_t planet_prompt[] =
	    "Time: 14:59  Planet command (?=help) [A]? ";
	static const uint8_t command[] = "b";
	static const uint8_t title[] =
	    "Welcome to the intergalactic bank of New Terra!";
	static const uint8_t bank_prompt[] =
	    "How many credits do you want in the account? 13345 Available ->";
	struct viewer_pager_join *join = &viewer->join;
	struct yt_present_result result;

	if (prompt_end == NULL || editor_end == NULL || body_end == NULL
	    || yt_planet_bank_available(12345.0f, 1000.0f) != 13345.0)
		return false;
	join->presentation = state(ansi);
	join->presentation.foreground = 6.0f;
	join->presentation.cached_foreground = ansi ? 6.0f : 0.0f;
	join->pager.foreground = 6;
	join->pager.line_count = 0.0f;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, free_holds,
	    sizeof(free_holds) - 1U, 0.0f)
	    || !normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, planet_prompt,
	    sizeof(planet_prompt) - 1U, 1.0f))
		return false;
	*prompt_end = join->remote_length;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	memcpy(join->accumulator, command, sizeof(command));
	if (yt_present_editor_echo(command, sizeof(command) - 1U,
	    command, sizeof(command) - 1U, &join->presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	*editor_end = join->remote_length;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, title, sizeof(title) - 1U, 0.0f)
	    || !normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, bank_prompt,
	    sizeof(bank_prompt) - 1U, 1.0f))
		return false;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	if (yt_present_editor_echo(NULL, 0U, NULL, 0U,
	    &join->presentation, &result) != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	*body_end = join->remote_length;
	join->pager.line_count = 0.0f;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, free_holds,
	    sizeof(free_holds) - 1U, 0.0f)
	    || !normal_exit_line(join, NULL, 0U))
		return false;
	join->presentation.foreground = 6.0f;
	join->pager.foreground = 6;
	return normal_exit_b05d(join, planet_prompt,
	    sizeof(planet_prompt) - 1U, 1.0f);
}

static void
test_planet_bank_cancel_cycle_presentation(void)
{
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	uint8_t remote[350];
	size_t prompt_end;
	size_t editor_end;
	size_t body_end;
	int pass;

	for (pass = 0; pass < 2; ++pass) {
		bool ansi = pass == 0;

		memset(&viewer, 0, sizeof(viewer));
		fixture_viewer_initialize(&viewer, &stream,
		    retained_scoreboard, sizeof(retained_scoreboard) - 1U,
		    "YTSCORE.ASC", ansi, remote, sizeof(remote));
		CHECK(planet_bank_cancel_cycle_run(&viewer, ansi,
		    &prompt_end, &editor_end, &body_end));
		CHECK(prompt_end == 77U && editor_end == 80U
		    && body_end == 198U && viewer.join.remote_length == 275U
		    && viewer_bytes_fnv1a64(remote, viewer.join.remote_length)
		    == UINT64_C(0xfd4fa7f4a8054f73)
		    && viewer.join.local_row_count == 11U
		    && viewer_rows_fnv1a64(&viewer.join)
		    == UINT64_C(0xbf15c6a7933ec78c)
		    && viewer.join.local_fragment_length == 42U
		    && memcmp(viewer.join.local_fragment,
		    "Time: 14:59  Planet command (?=help) [A]? ", 42U) == 0
		    && viewer.join.local_color_count == (ansi ? 20U : 6U)
		    && viewer_colors_fnv1a64(&viewer.join)
		    == (ansi ? UINT64_C(0x6ffcc2ba1c2c6835)
		    : UINT64_C(0x17798e683d05096d))
		    && viewer.join.presentation.foreground == 6.0f
		    && viewer.join.presentation.background == 0.0f
		    && viewer.join.presentation.bold == 0.0f
		    && viewer.join.presentation.blink == 0.0f
		    && viewer.join.presentation.cached_foreground
		    == (ansi ? 6.0f : 0.0f)
		    && viewer.join.pager.foreground == 6
		    && viewer.join.pager.line_count == 2.0f
		    && viewer.join.pager.nonstop == 0.0f
		    && viewer.join.accumulator[0] == '\0'
		    && viewer.join.queue_length == 0U
		    && viewer.join.sample_calls == 6U
		    && viewer.join.event_count == 30U
		    && stream.eof_checks == 0U && stream.key_checks == 0U
		    && stream.read_count == 0U && stream.line_count == 0U
		    && !stream.file_open && !viewer.join.file_open
		    && viewer.input.file == NULL && viewer.close_calls == 0U
		    && viewer.open_calls == 0U);
		yt_text_input_destroy(&viewer.input);
	}
}

static bool
planet_productivity_blank_cycle_run(struct physical_viewer_join *viewer,
    bool ansi, size_t *prompt_end, size_t *editor_end, size_t *body_end)
{
	static const uint8_t free_holds[] =
	    "You have 65 free cargo holds.";
	static const uint8_t planet_prompt[] =
	    "Time: 14:59  Planet command (?=help) [A]? ";
	static const uint8_t command[] = "$";
	static const uint8_t explanation[] =
	    "Productivity is increased by 1 Unit of EQU, ORG  & ORE "
	    "for each 250 credits.";
	static const uint8_t credits[] = "You have 12345 Credits.";
	static const uint8_t productivity_prompt[] =
	    "Spend how much to raise productivity? -+> ";
	struct viewer_pager_join *join = &viewer->join;
	struct yt_present_result result;

	if (prompt_end == NULL || editor_end == NULL || body_end == NULL)
		return false;
	join->presentation = state(ansi);
	join->presentation.foreground = 6.0f;
	join->presentation.cached_foreground = ansi ? 6.0f : 0.0f;
	join->pager.foreground = 6;
	join->pager.line_count = 0.0f;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, free_holds,
	    sizeof(free_holds) - 1U, 0.0f)
	    || !normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, planet_prompt,
	    sizeof(planet_prompt) - 1U, 1.0f))
		return false;
	*prompt_end = join->remote_length;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	memcpy(join->accumulator, command, sizeof(command));
	if (yt_present_editor_echo(command, sizeof(command) - 1U,
	    command, sizeof(command) - 1U, &join->presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	*editor_end = join->remote_length;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, explanation,
	    sizeof(explanation) - 1U, 0.0f)
	    || !normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, credits, sizeof(credits) - 1U, 0.0f)
	    || !normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, productivity_prompt,
	    sizeof(productivity_prompt) - 1U, 1.0f))
		return false;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	if (yt_present_editor_echo(NULL, 0U, NULL, 0U,
	    &join->presentation, &result) != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	*body_end = join->remote_length;
	join->pager.line_count = 0.0f;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, free_holds,
	    sizeof(free_holds) - 1U, 0.0f)
	    || !normal_exit_line(join, NULL, 0U))
		return false;
	join->presentation.foreground = 6.0f;
	join->pager.foreground = 6;
	return normal_exit_b05d(join, planet_prompt,
	    sizeof(planet_prompt) - 1U, 1.0f);
}

static void
test_planet_productivity_blank_cycle_presentation(void)
{
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	uint8_t remote[350];
	size_t prompt_end;
	size_t editor_end;
	size_t body_end;
	int pass;

	for (pass = 0; pass < 2; ++pass) {
		bool ansi = pass == 0;

		memset(&viewer, 0, sizeof(viewer));
		fixture_viewer_initialize(&viewer, &stream,
		    retained_scoreboard, sizeof(retained_scoreboard) - 1U,
		    "YTSCORE.ASC", ansi, remote, sizeof(remote));
		CHECK(planet_productivity_blank_cycle_run(&viewer, ansi,
		    &prompt_end, &editor_end, &body_end));
		CHECK(prompt_end == 77U && editor_end == 80U
		    && body_end == 233U && viewer.join.remote_length == 310U
		    && viewer_bytes_fnv1a64(remote, viewer.join.remote_length)
		    == UINT64_C(0x58fe6304e4711cba)
		    && viewer.join.local_row_count == 13U
		    && viewer_rows_fnv1a64(&viewer.join)
		    == UINT64_C(0xd68fd66502f62856)
		    && viewer.join.local_fragment_length == 42U
		    && memcmp(viewer.join.local_fragment,
		    "Time: 14:59  Planet command (?=help) [A]? ", 42U) == 0
		    && viewer.join.local_color_count == (ansi ? 23U : 7U)
		    && viewer_colors_fnv1a64(&viewer.join)
		    == (ansi ? UINT64_C(0x85fb302577784e5a)
		    : UINT64_C(0xc68cf2d74ffb7ffa))
		    && viewer.join.presentation.foreground == 6.0f
		    && viewer.join.presentation.background == 0.0f
		    && viewer.join.presentation.bold == 0.0f
		    && viewer.join.presentation.blink == 0.0f
		    && viewer.join.presentation.cached_foreground
		    == (ansi ? 6.0f : 0.0f)
		    && viewer.join.pager.foreground == 6
		    && viewer.join.pager.line_count == 2.0f
		    && viewer.join.pager.nonstop == 0.0f
		    && viewer.join.accumulator[0] == '\0'
		    && viewer.join.queue_length == 0U
		    && viewer.join.sample_calls == 7U
		    && viewer.join.event_count == 35U
		    && stream.eof_checks == 0U && stream.key_checks == 0U
		    && stream.read_count == 0U && stream.line_count == 0U
		    && !stream.file_open && !viewer.join.file_open
		    && viewer.input.file == NULL && viewer.close_calls == 0U
		    && viewer.open_calls == 0U);
		yt_text_input_destroy(&viewer.input);
	}
}

enum planet_transfer_cycle_outcome {
	PLANET_TRANSFER_CANCEL_CYCLE,
	PLANET_TRANSFER_NO_CARGO_CYCLE,
	PLANET_TRANSFER_CARGO_CYCLE,
	PLANET_TRANSFER_FIGHTER_CYCLE,
	PLANET_TRANSFER_PLASMA_CYCLE,
	PLANET_TRANSFER_MISSILE_CYCLE,
	PLANET_TRANSFER_MINE_CYCLE,
	PLANET_TRANSFER_INVALID_CYCLE,
	PLANET_TRANSFER_SUBSTRING_CYCLE,
	PLANET_TRANSFER_FIGHTER_BLANK_CYCLE,
	PLANET_TRANSFER_FIGHTER_E_CYCLE,
	PLANET_TRANSFER_FIGHTER_NEGATIVE_CYCLE,
	PLANET_TRANSFER_FIGHTER_HIGH_CYCLE,
};

static bool
planet_transfer_cycle_run(struct physical_viewer_join *viewer, bool ansi,
    enum planet_transfer_cycle_outcome outcome, size_t *prompt_end,
    size_t *editor_end, size_t *body_end)
{
	static const uint8_t free_holds_65[] =
	    "You have 65 free cargo holds.";
	static const uint8_t free_holds_100[] =
	    "You have 100 free cargo holds.";
	static const uint8_t planet_prompt[] =
	    "Time: 14:59  Planet command (?=help) [A]? ";
	static const uint8_t command[] = "t";
	static const uint8_t title[] = "<Transfer items to planet>";
	static const uint8_t question[] = "Transfer which item?";
	static const uint8_t plasma_row[] = "[B] Plasma Bolts";
	static const uint8_t cargo_row[] = "[C] Cargo";
	static const uint8_t fighter_row[] = "[F] Fighters";
	static const uint8_t missile_row[] = "[S] Missiles";
	static const uint8_t mine_row[] = "[M] Mines";
	static const uint8_t selector_prompt[] = "-=>";
	static const uint8_t empty_selector[] = "";
	static const uint8_t cargo_selector[] = "C";
	static const uint8_t fighter_selector[] = "F";
	static const uint8_t plasma_selector[] = "B";
	static const uint8_t missile_selector[] = "S";
	static const uint8_t mine_selector[] = "M";
	static const uint8_t invalid_selector[] = "X";
	static const uint8_t substring_selector[] = "SF";
	static const uint8_t fighter_prompt[] =
	    "You have 7 fighters. Transfer how many -=>";
	static const uint8_t fighter_amount[] = "3";
	static const uint8_t fighter_e_amount[] = "1e2";
	static const uint8_t fighter_negative_amount[] = "-1";
	static const uint8_t fighter_high_amount[] = "8";
	static const uint8_t no_cargo_message[] =
	    "You don't have any cargo!";
	static const uint8_t cargo_message[] = "Cargo transferred!!";
	static const uint8_t fighter_message[] = "Fighters Transferred!";
	static const uint8_t plasma_message[] = "Plasma Bolts Transferred!";
	static const uint8_t missile_message[] = "Missiles Transferred!";
	static const uint8_t mine_message[] = "Mines Transferred!";
	static const double empty_held[3] = {0.0, 0.0, 0.0};
	static const double nonempty_held[3] = {10.0, 20.0, 5.0};
	bool no_cargo = outcome == PLANET_TRANSFER_NO_CARGO_CYCLE;
	bool cargo = outcome == PLANET_TRANSFER_CARGO_CYCLE;
	bool fighter_accepted = outcome == PLANET_TRANSFER_FIGHTER_CYCLE;
	bool fighter_blank = outcome == PLANET_TRANSFER_FIGHTER_BLANK_CYCLE;
	bool fighter_e = outcome == PLANET_TRANSFER_FIGHTER_E_CYCLE;
	bool fighter_negative =
	    outcome == PLANET_TRANSFER_FIGHTER_NEGATIVE_CYCLE;
	bool fighter_high = outcome == PLANET_TRANSFER_FIGHTER_HIGH_CYCLE;
	bool fighter = fighter_accepted || fighter_blank || fighter_e
	    || fighter_negative || fighter_high;
	bool plasma = outcome == PLANET_TRANSFER_PLASMA_CYCLE;
	bool missile = outcome == PLANET_TRANSFER_MISSILE_CYCLE;
	bool mine = outcome == PLANET_TRANSFER_MINE_CYCLE;
	bool direct = plasma || missile || mine;
	bool invalid = outcome == PLANET_TRANSFER_INVALID_CYCLE;
	bool substring = outcome == PLANET_TRANSFER_SUBSTRING_CYCLE;
	const uint8_t *free_holds = no_cargo
	    ? free_holds_100 : free_holds_65;
	size_t free_holds_length = no_cargo
	    ? sizeof(free_holds_100) - 1U : sizeof(free_holds_65) - 1U;
	const uint8_t *selector = empty_selector;
	size_t selector_length = 0U;
	const uint8_t *direct_message = plasma ? plasma_message
	    : missile ? missile_message : mine_message;
	size_t direct_message_length = plasma ? sizeof(plasma_message) - 1U
	    : missile ? sizeof(missile_message) - 1U
	    : sizeof(mine_message) - 1U;
	const uint8_t *amount = fighter_accepted ? fighter_amount
	    : fighter_e ? fighter_e_amount
	    : fighter_negative ? fighter_negative_amount
	    : fighter_high ? fighter_high_amount : empty_selector;
	size_t amount_length = fighter_accepted ? sizeof(fighter_amount) - 1U
	    : fighter_e ? sizeof(fighter_e_amount) - 1U
	    : fighter_negative ? sizeof(fighter_negative_amount) - 1U
	    : fighter_high ? sizeof(fighter_high_amount) - 1U : 0U;
	struct viewer_pager_join *join = &viewer->join;
	struct yt_present_result result;

	if (no_cargo || cargo) {
		selector = cargo_selector;
		selector_length = sizeof(cargo_selector) - 1U;
	} else if (fighter) {
		selector = fighter_selector;
		selector_length = sizeof(fighter_selector) - 1U;
	} else if (plasma) {
		selector = plasma_selector;
		selector_length = sizeof(plasma_selector) - 1U;
	} else if (missile) {
		selector = missile_selector;
		selector_length = sizeof(missile_selector) - 1U;
	} else if (mine) {
		selector = mine_selector;
		selector_length = sizeof(mine_selector) - 1U;
	} else if (invalid) {
		selector = invalid_selector;
		selector_length = sizeof(invalid_selector) - 1U;
	} else if (substring) {
		selector = substring_selector;
		selector_length = sizeof(substring_selector) - 1U;
	}
	if (prompt_end == NULL || editor_end == NULL || body_end == NULL)
		return false;
	if (no_cargo && !yt_planet_transfer_cargo_empty(empty_held))
		return false;
	if (cargo && yt_planet_transfer_cargo_empty(nonempty_held))
		return false;
	if (fighter_accepted
	    && yt_planet_transfer_fighter_rejected(3.0f, 7.0f))
		return false;
	if (fighter_negative
	    && !yt_planet_transfer_fighter_rejected(-1.0f, 7.0f))
		return false;
	if (fighter_high
	    && !yt_planet_transfer_fighter_rejected(8.0f, 7.0f))
		return false;
	if (invalid && yt_planet_transfer_selector_position("X") != 0)
		return false;
	if (substring && yt_planet_transfer_selector_position("SF") != 2)
		return false;
	join->presentation = state(ansi);
	join->presentation.foreground = 6.0f;
	join->presentation.cached_foreground = ansi ? 6.0f : 0.0f;
	join->pager.foreground = 6;
	join->pager.line_count = 0.0f;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, free_holds,
	    free_holds_length, 0.0f)
	    || !normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, planet_prompt,
	    sizeof(planet_prompt) - 1U, 1.0f))
		return false;
	*prompt_end = join->remote_length;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	memcpy(join->accumulator, command, sizeof(command));
	if (yt_present_editor_echo(command, sizeof(command) - 1U,
	    command, sizeof(command) - 1U, &join->presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	*editor_end = join->remote_length;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, title, sizeof(title) - 1U, 0.0f)
	    || !normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, question, sizeof(question) - 1U, 0.0f)
	    || !normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, plasma_row,
	    sizeof(plasma_row) - 1U, 0.0f)
	    || !normal_exit_b05d(join, cargo_row,
	    sizeof(cargo_row) - 1U, 0.0f)
	    || !normal_exit_b05d(join, fighter_row,
	    sizeof(fighter_row) - 1U, 0.0f)
	    || !normal_exit_b05d(join, missile_row,
	    sizeof(missile_row) - 1U, 0.0f)
	    || !normal_exit_b05d(join, mine_row,
	    sizeof(mine_row) - 1U, 0.0f)
	    || !normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, selector_prompt,
	    sizeof(selector_prompt) - 1U, 1.0f))
		return false;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	memcpy(join->accumulator, selector, selector_length);
	join->accumulator[selector_length] = '\0';
	if (yt_present_editor_echo(selector, selector_length,
	    selector, selector_length,
	    &join->presentation, &result) != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	if (no_cargo) {
		if (!normal_exit_line(join, NULL, 0U))
			return false;
		join->queue_position = 0U;
		join->queue_length = 0U;
		join->queue[0] = '\0';
		join->presentation.bold = 1.0f;
		join->presentation.blink = 1.0f;
		if (!normal_exit_b05d(join, no_cargo_message,
		    sizeof(no_cargo_message) - 1U, 0.0f))
			return false;
	} else if (cargo) {
		if (!normal_exit_line(join, NULL, 0U)
		    || !normal_exit_b05d(join, cargo_message,
		    sizeof(cargo_message) - 1U, 0.0f))
			return false;
	} else if (fighter) {
		if (!normal_exit_line(join, NULL, 0U)
		    || !normal_exit_b05d(join, fighter_prompt,
		    sizeof(fighter_prompt) - 1U, 1.0f))
			return false;
		yt_pager_editor_enter(&join->pager, join->accumulator,
		    sizeof(join->accumulator));
		memcpy(join->accumulator, amount, amount_length);
		join->accumulator[amount_length] = '\0';
		if (yt_present_editor_echo(amount, amount_length,
		    amount, amount_length,
		    &join->presentation, &result) != YT_PRESENT_OK)
			return false;
		viewer_pager_capture_result(join, &result);
		if (!normal_exit_line(join, NULL, 0U))
			return false;
		if (fighter_accepted) {
			if (!normal_exit_line(join, NULL, 0U))
				return false;
			join->presentation.blink = 1.0f;
			if (!normal_exit_b05d(join, fighter_message,
			    sizeof(fighter_message) - 1U, 0.0f))
				return false;
		}
	} else if (direct) {
		if (!normal_exit_line(join, NULL, 0U))
			return false;
		join->presentation.blink = 1.0f;
		if (!normal_exit_b05d(join, direct_message,
		    direct_message_length, 0.0f))
			return false;
	}
	*body_end = join->remote_length;
	join->pager.line_count = 0.0f;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, free_holds,
	    free_holds_length, 0.0f)
	    || !normal_exit_line(join, NULL, 0U))
		return false;
	join->presentation.foreground = 6.0f;
	join->pager.foreground = 6;
	return normal_exit_b05d(join, planet_prompt,
	    sizeof(planet_prompt) - 1U, 1.0f);
}

static void
test_planet_transfer_cancel_cycle_presentation(void)
{
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	uint8_t remote[350];
	size_t prompt_end;
	size_t editor_end;
	size_t body_end;
	int pass;

	for (pass = 0; pass < 2; ++pass) {
		bool ansi = pass == 0;

		memset(&viewer, 0, sizeof(viewer));
		fixture_viewer_initialize(&viewer, &stream,
		    retained_scoreboard, sizeof(retained_scoreboard) - 1U,
		    "YTSCORE.ASC", ansi, remote, sizeof(remote));
		CHECK(planet_transfer_cycle_run(&viewer, ansi,
		    PLANET_TRANSFER_CANCEL_CYCLE,
		    &prompt_end, &editor_end, &body_end));
		CHECK(prompt_end == 77U && editor_end == 80U
		    && body_end == 211U && viewer.join.remote_length == 288U
		    && viewer_bytes_fnv1a64(remote, viewer.join.remote_length)
		    == UINT64_C(0x7db1274394dcd5e8)
		    && viewer.join.local_row_count == 19U
		    && viewer_rows_fnv1a64(&viewer.join)
		    == UINT64_C(0x1166e21a56f29b86)
		    && viewer.join.local_fragment_length == 42U
		    && memcmp(viewer.join.local_fragment,
		    "Time: 14:59  Planet command (?=help) [A]? ", 42U) == 0
		    && viewer.join.local_color_count == (ansi ? 34U : 12U)
		    && viewer_colors_fnv1a64(&viewer.join)
		    == (ansi ? UINT64_C(0x9c0cee38eed56cdd)
		    : UINT64_C(0x5218ab7752360135))
		    && viewer.join.presentation.foreground == 6.0f
		    && viewer.join.presentation.background == 0.0f
		    && viewer.join.presentation.bold == 0.0f
		    && viewer.join.presentation.blink == 0.0f
		    && viewer.join.presentation.cached_foreground
		    == (ansi ? 6.0f : 0.0f)
		    && viewer.join.pager.foreground == 6
		    && viewer.join.pager.line_count == 2.0f
		    && viewer.join.pager.nonstop == 0.0f
		    && viewer.join.accumulator[0] == '\0'
		    && viewer.join.queue_length == 0U
		    && viewer.join.sample_calls == 12U
		    && viewer.join.event_count == 60U
		    && stream.eof_checks == 0U && stream.key_checks == 0U
		    && stream.read_count == 0U && stream.line_count == 0U
		    && !stream.file_open && !viewer.join.file_open
		    && viewer.input.file == NULL && viewer.close_calls == 0U
		    && viewer.open_calls == 0U);
		yt_text_input_destroy(&viewer.input);
	}
}

static void
test_planet_transfer_no_cargo_cycle_presentation(void)
{
	static const uint8_t ansi[] =
	    "\r\nYou have 100 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? t\r\n"
	    "\r\n<Transfer items to planet>\n\r"
	    "\r\nTransfer which item?\n\r"
	    "\r\n[B] Plasma Bolts\n\r[C] Cargo\n\r[F] Fighters\n\r"
	    "[S] Missiles\n\r[M] Mines\n\r\r\n-=>C\r\n"
	    "\r\n\x1b[0;36;40;5;1mYou don't have any cargo!\n\r"
	    "\x1b[0;36;40m\r\nYou have 100 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? ";
	static const uint8_t plain[] =
	    "\r\nYou have 100 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? t\r\n"
	    "\r\n<Transfer items to planet>\n\r"
	    "\r\nTransfer which item?\n\r"
	    "\r\n[B] Plasma Bolts\n\r[C] Cargo\n\r[F] Fighters\n\r"
	    "[S] Missiles\n\r[M] Mines\n\r\r\n-=>C\r\n"
	    "\r\nYou don't have any cargo!\n\r"
	    "\r\nYou have 100 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? ";
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	uint8_t remote[380];
	size_t prompt_end;
	size_t editor_end;
	size_t body_end;
	int pass;

	for (pass = 0; pass < 2; ++pass) {
		bool ansi_mode = pass == 0;
		const uint8_t *expected = ansi_mode ? ansi : plain;
		size_t expected_length = ansi_mode
		    ? sizeof(ansi) - 1U : sizeof(plain) - 1U;

		memset(&viewer, 0, sizeof(viewer));
		fixture_viewer_initialize(&viewer, &stream,
		    retained_scoreboard, sizeof(retained_scoreboard) - 1U,
		    "YTSCORE.ASC", ansi_mode, remote, sizeof(remote));
		CHECK(planet_transfer_cycle_run(&viewer, ansi_mode,
		    PLANET_TRANSFER_NO_CARGO_CYCLE,
		    &prompt_end, &editor_end, &body_end));
		CHECK(prompt_end == 78U && editor_end == 81U
		    && body_end == (ansi_mode ? 256U : 242U)
		    && viewer.join.remote_length == expected_length
		    && expected_length == (ansi_mode ? 344U : 320U)
		    && memcmp(remote, expected, expected_length) == 0
		    && viewer_bytes_fnv1a64(remote, expected_length)
		    == (ansi_mode ? UINT64_C(0x9d9316c0e9ce3139)
		    : UINT64_C(0x184256ec0ff9bf51))
		    && viewer.join.local_row_count == 21U
		    && viewer_rows_fnv1a64(&viewer.join)
		    == UINT64_C(0xe6003c3da68e1325)
		    && viewer.join.local_fragment_length == 42U
		    && memcmp(viewer.join.local_fragment,
		    "Time: 14:59  Planet command (?=help) [A]? ", 42U) == 0
		    && viewer.join.local_color_count == (ansi_mode ? 37U : 13U)
		    && viewer_colors_fnv1a64(&viewer.join)
		    == (ansi_mode ? UINT64_C(0x15eca830b672d17a)
		    : UINT64_C(0xe4fcd46e10198702))
		    && viewer.join.presentation.foreground == 6.0f
		    && viewer.join.presentation.background == 0.0f
		    && viewer.join.presentation.bold == (ansi_mode ? 0.0f : 1.0f)
		    && viewer.join.presentation.blink == (ansi_mode ? 0.0f : 1.0f)
		    && viewer.join.presentation.cached_foreground
		    == (ansi_mode ? 6.0f : 0.0f)
		    && viewer.join.pager.foreground == 6
		    && viewer.join.pager.line_count == 2.0f
		    && viewer.join.pager.nonstop == 0.0f
		    && strcmp(viewer.join.accumulator, "C") == 0
		    && viewer.join.queue_position == 0U
		    && viewer.join.queue_length == 0U
		    && viewer.join.sample_calls == 13U
		    && viewer.join.event_count == 65U
		    && stream.eof_checks == 0U && stream.key_checks == 0U
		    && stream.read_count == 0U && stream.line_count == 0U
		    && !stream.file_open && !viewer.join.file_open
		    && viewer.input.file == NULL && viewer.close_calls == 0U
		    && viewer.open_calls == 0U);
		yt_text_input_destroy(&viewer.input);
	}
}

static void
test_planet_transfer_cargo_cycle_presentation(void)
{
	static const uint8_t expected[] =
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? t\r\n"
	    "\r\n<Transfer items to planet>\n\r"
	    "\r\nTransfer which item?\n\r"
	    "\r\n[B] Plasma Bolts\n\r[C] Cargo\n\r[F] Fighters\n\r"
	    "[S] Missiles\n\r[M] Mines\n\r\r\n-=>C\r\n"
	    "\r\nCargo transferred!!\n\r"
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? ";
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	uint8_t remote[350];
	size_t prompt_end;
	size_t editor_end;
	size_t body_end;
	int pass;

	for (pass = 0; pass < 2; ++pass) {
		bool ansi = pass == 0;

		memset(&viewer, 0, sizeof(viewer));
		fixture_viewer_initialize(&viewer, &stream,
		    retained_scoreboard, sizeof(retained_scoreboard) - 1U,
		    "YTSCORE.ASC", ansi, remote, sizeof(remote));
		CHECK(planet_transfer_cycle_run(&viewer, ansi,
		    PLANET_TRANSFER_CARGO_CYCLE,
		    &prompt_end, &editor_end, &body_end));
		CHECK(prompt_end == 77U && editor_end == 80U
		    && body_end == 235U && viewer.join.remote_length == 312U
		    && sizeof(expected) - 1U == 312U
		    && memcmp(remote, expected, sizeof(expected) - 1U) == 0
		    && viewer_bytes_fnv1a64(remote, viewer.join.remote_length)
		    == UINT64_C(0x753cb2447151ea4b)
		    && viewer.join.local_row_count == 21U
		    && viewer_rows_fnv1a64(&viewer.join)
		    == UINT64_C(0x6a1922c2618fe533)
		    && viewer.join.local_fragment_length == 42U
		    && memcmp(viewer.join.local_fragment,
		    "Time: 14:59  Planet command (?=help) [A]? ", 42U) == 0
		    && viewer.join.local_color_count == (ansi ? 37U : 13U)
		    && viewer_colors_fnv1a64(&viewer.join)
		    == (ansi ? UINT64_C(0xae1a9d6eec788b62)
		    : UINT64_C(0xe4fcd46e10198702))
		    && viewer.join.presentation.foreground == 6.0f
		    && viewer.join.presentation.background == 0.0f
		    && viewer.join.presentation.bold == 0.0f
		    && viewer.join.presentation.blink == 0.0f
		    && viewer.join.presentation.cached_foreground
		    == (ansi ? 6.0f : 0.0f)
		    && viewer.join.pager.foreground == 6
		    && viewer.join.pager.line_count == 2.0f
		    && viewer.join.pager.nonstop == 0.0f
		    && strcmp(viewer.join.accumulator, "C") == 0
		    && viewer.join.queue_length == 0U
		    && viewer.join.sample_calls == 13U
		    && viewer.join.event_count == 65U
		    && stream.eof_checks == 0U && stream.key_checks == 0U
		    && stream.read_count == 0U && stream.line_count == 0U
		    && !stream.file_open && !viewer.join.file_open
		    && viewer.input.file == NULL && viewer.close_calls == 0U
		    && viewer.open_calls == 0U);
		yt_text_input_destroy(&viewer.input);
	}
}

static void
test_planet_transfer_fighter_cycle_presentation(void)
{
	static const uint8_t ansi[] =
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? t\r\n"
	    "\r\n<Transfer items to planet>\n\r"
	    "\r\nTransfer which item?\n\r"
	    "\r\n[B] Plasma Bolts\n\r[C] Cargo\n\r[F] Fighters\n\r"
	    "[S] Missiles\n\r[M] Mines\n\r\r\n-=>F\r\n"
	    "\r\nYou have 7 fighters. Transfer how many -=>3\r\n"
	    "\r\n\x1b[0;36;40;5mFighters Transferred!\n\r"
	    "\x1b[0;36;40m\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? ";
	static const uint8_t plain[] =
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? t\r\n"
	    "\r\n<Transfer items to planet>\n\r"
	    "\r\nTransfer which item?\n\r"
	    "\r\n[B] Plasma Bolts\n\r[C] Cargo\n\r[F] Fighters\n\r"
	    "[S] Missiles\n\r[M] Mines\n\r\r\n-=>F\r\n"
	    "\r\nYou have 7 fighters. Transfer how many -=>3\r\n"
	    "\r\nFighters Transferred!\n\r"
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? ";
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	uint8_t remote[420];
	size_t prompt_end;
	size_t editor_end;
	size_t body_end;
	int pass;

	for (pass = 0; pass < 2; ++pass) {
		bool ansi_mode = pass == 0;
		const uint8_t *expected = ansi_mode ? ansi : plain;
		size_t expected_length = ansi_mode
		    ? sizeof(ansi) - 1U : sizeof(plain) - 1U;

		memset(&viewer, 0, sizeof(viewer));
		fixture_viewer_initialize(&viewer, &stream,
		    retained_scoreboard, sizeof(retained_scoreboard) - 1U,
		    "YTSCORE.ASC", ansi_mode, remote, sizeof(remote));
		CHECK(planet_transfer_cycle_run(&viewer, ansi_mode,
		    PLANET_TRANSFER_FIGHTER_CYCLE,
		    &prompt_end, &editor_end, &body_end));
		CHECK(prompt_end == 77U && editor_end == 80U
		    && body_end == (ansi_mode ? 296U : 284U)
		    && viewer.join.remote_length == expected_length
		    && expected_length == (ansi_mode ? 383U : 361U)
		    && memcmp(remote, expected, expected_length) == 0
		    && viewer_bytes_fnv1a64(remote, expected_length)
		    == (ansi_mode ? UINT64_C(0xbeb200831461e894)
		    : UINT64_C(0x681ac35eebfaa1b8))
		    && viewer.join.local_row_count == 23U
		    && viewer_rows_fnv1a64(&viewer.join)
		    == UINT64_C(0x28fae3200bbca559)
		    && viewer.join.local_fragment_length == 42U
		    && memcmp(viewer.join.local_fragment,
		    "Time: 14:59  Planet command (?=help) [A]? ", 42U) == 0
		    && viewer.join.local_color_count
		    == (ansi_mode ? 41U : 14U)
		    && viewer_colors_fnv1a64(&viewer.join)
		    == (ansi_mode ? UINT64_C(0xef4d90be9bc313e6)
		    : UINT64_C(0x4692bc1a44da0ecd))
		    && viewer.join.presentation.foreground == 6.0f
		    && viewer.join.presentation.background == 0.0f
		    && viewer.join.presentation.bold == 0.0f
		    && viewer.join.presentation.blink
		    == (ansi_mode ? 0.0f : 1.0f)
		    && viewer.join.presentation.cached_foreground
		    == (ansi_mode ? 6.0f : 0.0f)
		    && viewer.join.pager.foreground == 6
		    && viewer.join.pager.line_count == 2.0f
		    && viewer.join.pager.nonstop == 0.0f
		    && strcmp(viewer.join.accumulator, "3") == 0
		    && viewer.join.queue_length == 0U
		    && viewer.join.sample_calls == 14U
		    && viewer.join.event_count == 70U
		    && stream.eof_checks == 0U && stream.key_checks == 0U
		    && stream.read_count == 0U && stream.line_count == 0U
		    && !stream.file_open && !viewer.join.file_open
		    && viewer.input.file == NULL && viewer.close_calls == 0U
		    && viewer.open_calls == 0U);
		yt_text_input_destroy(&viewer.input);
	}
}

static void
test_planet_transfer_direct_cycles_presentation(void)
{
	static const uint8_t plasma_ansi[] =
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? t\r\n"
	    "\r\n<Transfer items to planet>\n\r"
	    "\r\nTransfer which item?\n\r"
	    "\r\n[B] Plasma Bolts\n\r[C] Cargo\n\r[F] Fighters\n\r"
	    "[S] Missiles\n\r[M] Mines\n\r\r\n-=>B\r\n"
	    "\r\n\x1b[0;36;40;5mPlasma Bolts Transferred!\n\r"
	    "\x1b[0;36;40m\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? ";
	static const uint8_t plasma_plain[] =
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? t\r\n"
	    "\r\n<Transfer items to planet>\n\r"
	    "\r\nTransfer which item?\n\r"
	    "\r\n[B] Plasma Bolts\n\r[C] Cargo\n\r[F] Fighters\n\r"
	    "[S] Missiles\n\r[M] Mines\n\r\r\n-=>B\r\n"
	    "\r\nPlasma Bolts Transferred!\n\r"
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? ";
	static const uint8_t missile_ansi[] =
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? t\r\n"
	    "\r\n<Transfer items to planet>\n\r"
	    "\r\nTransfer which item?\n\r"
	    "\r\n[B] Plasma Bolts\n\r[C] Cargo\n\r[F] Fighters\n\r"
	    "[S] Missiles\n\r[M] Mines\n\r\r\n-=>S\r\n"
	    "\r\n\x1b[0;36;40;5mMissiles Transferred!\n\r"
	    "\x1b[0;36;40m\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? ";
	static const uint8_t missile_plain[] =
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? t\r\n"
	    "\r\n<Transfer items to planet>\n\r"
	    "\r\nTransfer which item?\n\r"
	    "\r\n[B] Plasma Bolts\n\r[C] Cargo\n\r[F] Fighters\n\r"
	    "[S] Missiles\n\r[M] Mines\n\r\r\n-=>S\r\n"
	    "\r\nMissiles Transferred!\n\r"
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? ";
	static const uint8_t mine_ansi[] =
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? t\r\n"
	    "\r\n<Transfer items to planet>\n\r"
	    "\r\nTransfer which item?\n\r"
	    "\r\n[B] Plasma Bolts\n\r[C] Cargo\n\r[F] Fighters\n\r"
	    "[S] Missiles\n\r[M] Mines\n\r\r\n-=>M\r\n"
	    "\r\n\x1b[0;36;40;5mMines Transferred!\n\r"
	    "\x1b[0;36;40m\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? ";
	static const uint8_t mine_plain[] =
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? t\r\n"
	    "\r\n<Transfer items to planet>\n\r"
	    "\r\nTransfer which item?\n\r"
	    "\r\n[B] Plasma Bolts\n\r[C] Cargo\n\r[F] Fighters\n\r"
	    "[S] Missiles\n\r[M] Mines\n\r\r\n-=>M\r\n"
	    "\r\nMines Transferred!\n\r"
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? ";
	static const struct {
		enum planet_transfer_cycle_outcome outcome;
		const uint8_t *expected[2];
		size_t expected_length[2];
		size_t body_end[2];
		uint64_t remote_fnv[2];
		uint64_t row_fnv;
		const char *accumulator;
	} cases[] = {
		{PLANET_TRANSFER_PLASMA_CYCLE,
		    {plasma_ansi, plasma_plain},
		    {sizeof(plasma_ansi) - 1U, sizeof(plasma_plain) - 1U},
		    {253U, 241U},
		    {UINT64_C(0x2903bc60fa338a81),
		    UINT64_C(0xba6778c2aff95645)},
		    UINT64_C(0x24ed977995771d9b), "B"},
		{PLANET_TRANSFER_MISSILE_CYCLE,
		    {missile_ansi, missile_plain},
		    {sizeof(missile_ansi) - 1U, sizeof(missile_plain) - 1U},
		    {249U, 237U},
		    {UINT64_C(0x015fa913e629b4fd),
		    UINT64_C(0xc7d8da2104193399)},
		    UINT64_C(0x8cf2c0d00029ddc7), "S"},
		{PLANET_TRANSFER_MINE_CYCLE,
		    {mine_ansi, mine_plain},
		    {sizeof(mine_ansi) - 1U, sizeof(mine_plain) - 1U},
		    {246U, 234U},
		    {UINT64_C(0x5855b9fdcb230cca),
		    UINT64_C(0x46090502f23b09ca)},
		    UINT64_C(0x7d9a8a5b05756741), "M"},
	};
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	uint8_t remote[380];
	size_t prompt_end;
	size_t editor_end;
	size_t body_end;
	size_t index;
	int pass;

	CHECK(sizeof(plasma_ansi) - 1U == 340U
	    && sizeof(plasma_plain) - 1U == 318U
	    && sizeof(missile_ansi) - 1U == 336U
	    && sizeof(missile_plain) - 1U == 314U
	    && sizeof(mine_ansi) - 1U == 333U
	    && sizeof(mine_plain) - 1U == 311U);
	for (index = 0U; index < YT_ARRAY_LEN(cases); ++index) {
		for (pass = 0; pass < 2; ++pass) {
			bool ansi_mode = pass == 0;

			memset(&viewer, 0, sizeof(viewer));
			fixture_viewer_initialize(&viewer, &stream,
			    retained_scoreboard,
			    sizeof(retained_scoreboard) - 1U,
			    "YTSCORE.ASC", ansi_mode, remote, sizeof(remote));
			CHECK(planet_transfer_cycle_run(&viewer, ansi_mode,
			    cases[index].outcome,
			    &prompt_end, &editor_end, &body_end));
			CHECK(prompt_end == 77U && editor_end == 80U
			    && body_end == cases[index].body_end[pass]
			    && viewer.join.remote_length
			    == cases[index].expected_length[pass]
			    && memcmp(remote, cases[index].expected[pass],
			    cases[index].expected_length[pass]) == 0
			    && viewer_bytes_fnv1a64(remote,
			    viewer.join.remote_length)
			    == cases[index].remote_fnv[pass]
			    && viewer.join.local_row_count == 21U
			    && viewer_rows_fnv1a64(&viewer.join)
			    == cases[index].row_fnv
			    && viewer.join.local_fragment_length == 42U
			    && memcmp(viewer.join.local_fragment,
			    "Time: 14:59  Planet command (?=help) [A]? ", 42U)
			    == 0
			    && viewer.join.local_color_count
			    == (ansi_mode ? 37U : 13U)
			    && viewer_colors_fnv1a64(&viewer.join)
			    == (ansi_mode ? UINT64_C(0x9dfbf9f01dca0f72)
			    : UINT64_C(0xe4fcd46e10198702))
			    && viewer.join.presentation.foreground == 6.0f
			    && viewer.join.presentation.background == 0.0f
			    && viewer.join.presentation.bold == 0.0f
			    && viewer.join.presentation.blink
			    == (ansi_mode ? 0.0f : 1.0f)
			    && viewer.join.presentation.cached_foreground
			    == (ansi_mode ? 6.0f : 0.0f)
			    && viewer.join.pager.foreground == 6
			    && viewer.join.pager.line_count == 2.0f
			    && viewer.join.pager.nonstop == 0.0f
			    && strcmp(viewer.join.accumulator,
			    cases[index].accumulator) == 0
			    && viewer.join.queue_length == 0U
			    && viewer.join.sample_calls == 13U
			    && viewer.join.event_count == 65U
			    && stream.eof_checks == 0U && stream.key_checks == 0U
			    && stream.read_count == 0U && stream.line_count == 0U
			    && !stream.file_open && !viewer.join.file_open
			    && viewer.input.file == NULL && viewer.close_calls == 0U
			    && viewer.open_calls == 0U);
			yt_text_input_destroy(&viewer.input);
		}
	}
}

static void
test_planet_transfer_remaining_cycles_presentation(void)
{
#define TRANSFER_REMAINING_PREFIX \
	"\r\nYou have 65 free cargo holds.\n\r" \
	"\r\nTime: 14:59  Planet command (?=help) [A]? t\r\n" \
	"\r\n<Transfer items to planet>\n\r" \
	"\r\nTransfer which item?\n\r" \
	"\r\n[B] Plasma Bolts\n\r[C] Cargo\n\r[F] Fighters\n\r" \
	"[S] Missiles\n\r[M] Mines\n\r\r\n-=>"
#define TRANSFER_REMAINING_REDRAW \
	"\r\nYou have 65 free cargo holds.\n\r" \
	"\r\nTime: 14:59  Planet command (?=help) [A]? "
	static const uint8_t invalid[] =
	    TRANSFER_REMAINING_PREFIX "X\r\n" TRANSFER_REMAINING_REDRAW;
	static const uint8_t substring[] =
	    TRANSFER_REMAINING_PREFIX "SF\r\n" TRANSFER_REMAINING_REDRAW;
	static const uint8_t fighter_blank[] =
	    TRANSFER_REMAINING_PREFIX "F\r\n"
	    "\r\nYou have 7 fighters. Transfer how many -=>\r\n"
	    TRANSFER_REMAINING_REDRAW;
	static const uint8_t fighter_e[] =
	    TRANSFER_REMAINING_PREFIX "F\r\n"
	    "\r\nYou have 7 fighters. Transfer how many -=>1e2\r\n"
	    TRANSFER_REMAINING_REDRAW;
	static const uint8_t fighter_negative[] =
	    TRANSFER_REMAINING_PREFIX "F\r\n"
	    "\r\nYou have 7 fighters. Transfer how many -=>-1\r\n"
	    TRANSFER_REMAINING_REDRAW;
	static const uint8_t fighter_high[] =
	    TRANSFER_REMAINING_PREFIX "F\r\n"
	    "\r\nYou have 7 fighters. Transfer how many -=>8\r\n"
	    TRANSFER_REMAINING_REDRAW;
	static const struct {
		enum planet_transfer_cycle_outcome outcome;
		const uint8_t *expected;
		size_t expected_length;
		size_t body_end;
		uint64_t remote_fnv;
		uint64_t row_fnv;
		const char *accumulator;
	} cases[] = {
		{PLANET_TRANSFER_INVALID_CYCLE, invalid, sizeof(invalid) - 1U,
		    212U, UINT64_C(0x4cf74b255676ee7e),
		    UINT64_C(0x22bd76d4852df485), "X"},
		{PLANET_TRANSFER_SUBSTRING_CYCLE, substring,
		    sizeof(substring) - 1U, 213U,
		    UINT64_C(0x178bf7b8c3921bef),
		    UINT64_C(0x5808eab225fa70cf), "SF"},
		{PLANET_TRANSFER_FIGHTER_BLANK_CYCLE, fighter_blank,
		    sizeof(fighter_blank) - 1U, 258U,
		    UINT64_C(0x151bc2e9c7445b12),
		    UINT64_C(0x5713343941d2d2ab), ""},
		{PLANET_TRANSFER_FIGHTER_E_CYCLE, fighter_e,
		    sizeof(fighter_e) - 1U, 261U,
		    UINT64_C(0xf3c1fd8ebb16a2e8),
		    UINT64_C(0xaf81d5e354334abc), "1e2"},
		{PLANET_TRANSFER_FIGHTER_NEGATIVE_CYCLE, fighter_negative,
		    sizeof(fighter_negative) - 1U, 260U,
		    UINT64_C(0x661bb15b5bc8e060),
		    UINT64_C(0xfbc7639aa390e4cf), "-1"},
		{PLANET_TRANSFER_FIGHTER_HIGH_CYCLE, fighter_high,
		    sizeof(fighter_high) - 1U, 259U,
		    UINT64_C(0xe88da78c1e0b05cc),
		    UINT64_C(0x253c9a8a65b8802e), "8"},
	};
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	uint8_t remote[380];
	size_t prompt_end;
	size_t editor_end;
	size_t body_end;
	size_t index;
	int pass;

	CHECK(sizeof(invalid) - 1U == 289U
	    && sizeof(substring) - 1U == 290U
	    && sizeof(fighter_blank) - 1U == 335U
	    && sizeof(fighter_e) - 1U == 338U
	    && sizeof(fighter_negative) - 1U == 337U
	    && sizeof(fighter_high) - 1U == 336U);
	for (index = 0U; index < YT_ARRAY_LEN(cases); ++index) {
		for (pass = 0; pass < 2; ++pass) {
			bool ansi = pass == 0;

			memset(&viewer, 0, sizeof(viewer));
			fixture_viewer_initialize(&viewer, &stream,
			    retained_scoreboard,
			    sizeof(retained_scoreboard) - 1U,
			    "YTSCORE.ASC", ansi, remote, sizeof(remote));
			CHECK(planet_transfer_cycle_run(&viewer, ansi,
			    cases[index].outcome,
			    &prompt_end, &editor_end, &body_end));
			CHECK(prompt_end == 77U && editor_end == 80U
			    && body_end == cases[index].body_end
			    && viewer.join.remote_length
			    == cases[index].expected_length
			    && memcmp(remote, cases[index].expected,
			    cases[index].expected_length) == 0
			    && viewer_bytes_fnv1a64(remote,
			    viewer.join.remote_length) == cases[index].remote_fnv
			    && viewer.join.local_row_count
			    == (index < 2U ? 19U : 21U)
			    && viewer_rows_fnv1a64(&viewer.join)
			    == cases[index].row_fnv
			    && viewer.join.local_fragment_length == 42U
			    && memcmp(viewer.join.local_fragment,
			    "Time: 14:59  Planet command (?=help) [A]? ", 42U)
			    == 0
			    && viewer.join.local_color_count
			    == (index < 2U ? (ansi ? 34U : 12U)
			    : (ansi ? 38U : 13U))
			    && viewer_colors_fnv1a64(&viewer.join)
			    == (index < 2U
			    ? (ansi ? UINT64_C(0x9c0cee38eed56cdd)
			    : UINT64_C(0x5218ab7752360135))
			    : (ansi ? UINT64_C(0xf999c19b50ef5a89)
			    : UINT64_C(0xe4fcd46e10198702)))
			    && viewer.join.presentation.foreground == 6.0f
			    && viewer.join.presentation.background == 0.0f
			    && viewer.join.presentation.bold == 0.0f
			    && viewer.join.presentation.blink == 0.0f
			    && viewer.join.presentation.cached_foreground
			    == (ansi ? 6.0f : 0.0f)
			    && viewer.join.pager.foreground == 6
			    && viewer.join.pager.line_count == 2.0f
			    && viewer.join.pager.nonstop == 0.0f
			    && strcmp(viewer.join.accumulator,
			    cases[index].accumulator) == 0
			    && viewer.join.queue_length == 0U
			    && viewer.join.sample_calls
			    == (index < 2U ? 12U : 13U)
			    && viewer.join.event_count
			    == (index < 2U ? 60U : 65U)
			    && stream.eof_checks == 0U && stream.key_checks == 0U
			    && stream.read_count == 0U && stream.line_count == 0U
			    && !stream.file_open && !viewer.join.file_open
			    && viewer.input.file == NULL && viewer.close_calls == 0U
			    && viewer.open_calls == 0U);
			yt_text_input_destroy(&viewer.input);
		}
	}
#undef TRANSFER_REMAINING_PREFIX
#undef TRANSFER_REMAINING_REDRAW
}

static bool
planet_rename_protected_cycle_run(struct physical_viewer_join *viewer,
    bool ansi, size_t *prompt_end, size_t *editor_end, size_t *body_end)
{
	static const uint8_t free_holds[] =
	    "You have 65 free cargo holds.";
	static const uint8_t planet_prompt[] =
	    "Time: 14:59  Planet command (?=help) [A]? ";
	static const uint8_t command[] = "n";
	static const uint8_t protected[] = "You can't re-name this planet!";
	struct viewer_pager_join *join = &viewer->join;
	struct yt_present_result result;

	if (prompt_end == NULL || editor_end == NULL || body_end == NULL
	    || !yt_planet_rename_protected(101.5f, 100.5f, 400.25f))
		return false;
	join->presentation = state(ansi);
	join->presentation.foreground = 6.0f;
	join->presentation.cached_foreground = ansi ? 6.0f : 0.0f;
	join->pager.foreground = 6;
	join->pager.line_count = 0.0f;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, free_holds,
	    sizeof(free_holds) - 1U, 0.0f)
	    || !normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, planet_prompt,
	    sizeof(planet_prompt) - 1U, 1.0f))
		return false;
	*prompt_end = join->remote_length;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	memcpy(join->accumulator, command, sizeof(command));
	if (yt_present_editor_echo(command, sizeof(command) - 1U,
	    command, sizeof(command) - 1U, &join->presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	*editor_end = join->remote_length;
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	join->presentation.bold = 1.0f;
	join->presentation.blink = 1.0f;
	if (!normal_exit_b05d(join, protected, sizeof(protected) - 1U, 0.0f))
		return false;
	*body_end = join->remote_length;
	join->pager.line_count = 0.0f;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, free_holds,
	    sizeof(free_holds) - 1U, 0.0f)
	    || !normal_exit_line(join, NULL, 0U))
		return false;
	join->presentation.foreground = 6.0f;
	join->pager.foreground = 6;
	return normal_exit_b05d(join, planet_prompt,
	    sizeof(planet_prompt) - 1U, 1.0f);
}

static void
test_planet_rename_protected_cycle_presentation(void)
{
	static const struct {
		bool ansi;
		size_t body_end;
		size_t remote_length;
		uint64_t remote_fnv;
	} cases[] = {
		{true, 128U, 215U, UINT64_C(0x11c71a9d41ed31b4)},
		{false, 114U, 191U, UINT64_C(0x8d6f837de0241530)},
	};
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	uint8_t remote[250];
	size_t prompt_end;
	size_t editor_end;
	size_t body_end;
	size_t pass;

	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		memset(&viewer, 0, sizeof(viewer));
		fixture_viewer_initialize(&viewer, &stream,
		    retained_scoreboard, sizeof(retained_scoreboard) - 1U,
		    "YTSCORE.ASC", cases[pass].ansi, remote, sizeof(remote));
		CHECK(planet_rename_protected_cycle_run(&viewer,
		    cases[pass].ansi, &prompt_end, &editor_end, &body_end));
		CHECK(prompt_end == 77U && editor_end == 80U
		    && body_end == cases[pass].body_end
		    && viewer.join.remote_length == cases[pass].remote_length
		    && viewer_bytes_fnv1a64(remote, viewer.join.remote_length)
		    == cases[pass].remote_fnv
		    && viewer.join.local_row_count == 9U
		    && viewer_rows_fnv1a64(&viewer.join)
		    == UINT64_C(0xe7cdb4566a9fbf2f)
		    && viewer.join.local_fragment_length == 42U
		    && memcmp(viewer.join.local_fragment,
		    "Time: 14:59  Planet command (?=help) [A]? ", 42U) == 0
		    && viewer.join.local_color_count
		    == (cases[pass].ansi ? 16U : 5U)
		    && viewer_colors_fnv1a64(&viewer.join)
		    == (cases[pass].ansi ? UINT64_C(0xb96bd9b68b68a999)
		    : UINT64_C(0xc6f69f5cf097a0a2))
		    && viewer.join.presentation.foreground == 6.0f
		    && viewer.join.presentation.background == 0.0f
		    && viewer.join.presentation.bold
		    == (cases[pass].ansi ? 0.0f : 1.0f)
		    && viewer.join.presentation.blink
		    == (cases[pass].ansi ? 0.0f : 1.0f)
		    && viewer.join.presentation.cached_foreground
		    == (cases[pass].ansi ? 6.0f : 0.0f)
		    && viewer.join.pager.foreground == 6
		    && viewer.join.pager.line_count == 2.0f
		    && viewer.join.pager.nonstop == 0.0f
		    && strcmp(viewer.join.accumulator, "n") == 0
		    && viewer.join.queue_length == 0U
		    && viewer.join.sample_calls == 5U
		    && viewer.join.event_count == 25U
		    && stream.eof_checks == 0U && stream.key_checks == 0U
		    && stream.read_count == 0U && stream.line_count == 0U
		    && !stream.file_open && !viewer.join.file_open
		    && viewer.input.file == NULL && viewer.close_calls == 0U
		    && viewer.open_calls == 0U);
		yt_text_input_destroy(&viewer.input);
	}
}

static bool
planet_take_one_accepted_cycle_run(struct physical_viewer_join *viewer,
    bool ansi, int item, const uint8_t *amount, size_t amount_length,
    size_t *prompt_end, size_t *editor_end, size_t *body_end)
{
	static const uint8_t free_holds[] =
	    "You have 65 free cargo holds.";
	static const uint8_t planet_prompt[] =
	    "Time: 14:59  Planet command (?=help) [A]? ";
	static const uint8_t maximum_65[] = "How much [ 65 ]? ";
	static const uint8_t maximum_404[] = "How much [ 404 ]? ";
	static const uint8_t maximum_5[] = "How much [ 5 ]? ";
	static const uint8_t maximum_6[] = "How much [ 6 ]? ";
	static const uint8_t maximum_9[] = "How much [ 9 ]? ";
	const uint8_t *amount_prompt;
	size_t amount_prompt_length;
	uint8_t command[2];
	const char *title = yt_planet_take_one_title(item);
	struct viewer_pager_join *join = &viewer->join;
	struct yt_present_result result;

	if (amount == NULL || amount_length >= sizeof(join->accumulator)
	    || prompt_end == NULL || editor_end == NULL || body_end == NULL
	    || title == NULL)
		return false;
	switch (item) {
	case 1:
	case 2:
	case 3:
		amount_prompt = maximum_65;
		amount_prompt_length = sizeof(maximum_65) - 1U;
		break;
	case 4:
		amount_prompt = maximum_404;
		amount_prompt_length = sizeof(maximum_404) - 1U;
		break;
	case 5:
		amount_prompt = maximum_5;
		amount_prompt_length = sizeof(maximum_5) - 1U;
		break;
	case 6:
		amount_prompt = maximum_6;
		amount_prompt_length = sizeof(maximum_6) - 1U;
		break;
	case 9:
		amount_prompt = maximum_9;
		amount_prompt_length = sizeof(maximum_9) - 1U;
		break;
	default:
		return false;
	}
	command[0] = (uint8_t)('0' + item);
	command[1] = '\0';
	join->presentation = state(ansi);
	join->presentation.foreground = 6.0f;
	join->presentation.cached_foreground = ansi ? 6.0f : 0.0f;
	join->pager.foreground = 6;
	join->pager.line_count = 0.0f;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, free_holds,
	    sizeof(free_holds) - 1U, 0.0f)
	    || !normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, planet_prompt,
	    sizeof(planet_prompt) - 1U, 1.0f))
		return false;
	*prompt_end = join->remote_length;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	memcpy(join->accumulator, command, sizeof(command));
	if (yt_present_editor_echo(command, 1U, command, 1U,
	    &join->presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	*editor_end = join->remote_length;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, (const uint8_t *)title,
	    strlen(title), 0.0f)
	    || !normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, amount_prompt,
	    amount_prompt_length, 1.0f))
		return false;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	memcpy(join->accumulator, amount, amount_length);
	join->accumulator[amount_length] = '\0';
	if (yt_present_editor_echo(amount, amount_length,
	    amount, amount_length, &join->presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	*body_end = join->remote_length;
	join->pager.line_count = 0.0f;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, free_holds,
	    sizeof(free_holds) - 1U, 0.0f)
	    || !normal_exit_line(join, NULL, 0U))
		return false;
	join->presentation.foreground = 6.0f;
	join->pager.foreground = 6;
	return normal_exit_b05d(join, planet_prompt,
	    sizeof(planet_prompt) - 1U, 1.0f);
}

static void
test_planet_take_one_accepted_cycle_presentation(void)
{
	static const uint8_t amount[] = "1";
	static const struct {
		int item;
		size_t body_end;
		size_t remote_length;
		uint64_t remote_fnv;
		uint64_t row_fnv;
	} cases[] = {
		{1, 116U, 193U, UINT64_C(0xdb6145e26d95ab96),
		    UINT64_C(0x6132704c827a456b)},
		{2, 121U, 198U, UINT64_C(0x472cf90269f8614c),
		    UINT64_C(0xcab94f57a137c8fe)},
		{3, 122U, 199U, UINT64_C(0xbbb904922a08660f),
		    UINT64_C(0x0a7d16eb8c0256dc)},
		{4, 122U, 199U, UINT64_C(0x75846f146912b3e1),
		    UINT64_C(0x24bf8c90281fe66a)},
		{5, 120U, 197U, UINT64_C(0xebd8edd337944176),
		    UINT64_C(0xd408f1a7e23e30b3)},
		{6, 117U, 194U, UINT64_C(0xf470ca250563f37b),
		    UINT64_C(0xdbc1cc58885e68e1)},
		{9, 124U, 201U, UINT64_C(0xac175d85c7178aae),
		    UINT64_C(0x15d28c1540df4537)},
	};
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	uint8_t remote[250];
	size_t prompt_end;
	size_t editor_end;
	size_t body_end;
	size_t index;

	for (index = 0U; index < YT_ARRAY_LEN(cases); ++index) {
		int pass;

		for (pass = 0; pass < 2; ++pass) {
			bool ansi = pass == 0;

			memset(&viewer, 0, sizeof(viewer));
			fixture_viewer_initialize(&viewer, &stream,
			    retained_scoreboard,
			    sizeof(retained_scoreboard) - 1U,
			    "YTSCORE.ASC", ansi, remote, sizeof(remote));
			CHECK(planet_take_one_accepted_cycle_run(&viewer, ansi,
			    cases[index].item, amount, sizeof(amount) - 1U,
			    &prompt_end, &editor_end, &body_end));
			CHECK(prompt_end == 77U && editor_end == 80U
			    && body_end == cases[index].body_end
			    && viewer.join.remote_length
			    == cases[index].remote_length
			    && viewer_bytes_fnv1a64(remote,
			    viewer.join.remote_length) == cases[index].remote_fnv
			    && viewer.join.local_row_count == 11U
			    && viewer_rows_fnv1a64(&viewer.join)
			    == cases[index].row_fnv
			    && viewer.join.local_fragment_length == 42U
			    && memcmp(viewer.join.local_fragment,
			    "Time: 14:59  Planet command (?=help) [A]? ", 42U)
			    == 0
			    && viewer.join.local_color_count
			    == (ansi ? 20U : 6U)
			    && viewer_colors_fnv1a64(&viewer.join)
			    == (ansi ? UINT64_C(0x6ffcc2ba1c2c6835)
			    : UINT64_C(0x17798e683d05096d))
			    && viewer.join.presentation.foreground == 6.0f
			    && viewer.join.presentation.background == 0.0f
			    && viewer.join.presentation.bold == 0.0f
			    && viewer.join.presentation.blink == 0.0f
			    && viewer.join.presentation.cached_foreground
			    == (ansi ? 6.0f : 0.0f)
			    && viewer.join.pager.foreground == 6
			    && viewer.join.pager.line_count == 2.0f
			    && viewer.join.pager.nonstop == 0.0f
			    && strcmp(viewer.join.accumulator, "1") == 0
			    && viewer.join.queue_length == 0U
			    && viewer.join.sample_calls == 6U
			    && viewer.join.event_count == 30U
			    && stream.eof_checks == 0U && stream.key_checks == 0U
			    && stream.read_count == 0U && stream.line_count == 0U
			    && !stream.file_open && !viewer.join.file_open
			    && viewer.input.file == NULL
			    && viewer.close_calls == 0U
			    && viewer.open_calls == 0U);
			yt_text_input_destroy(&viewer.input);
		}
	}
}

static void
test_planet_take_one_blank_default_cycle_presentation(void)
{
	static const uint8_t amount[] = "";
	static const uint8_t expected[] =
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? 1\r\n"
	    "\r\n<Take Ore>\n\r\r\nHow much [ 65 ]? \r\n"
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? ";
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	uint8_t remote[250];
	size_t prompt_end;
	size_t editor_end;
	size_t body_end;
	int pass;

	for (pass = 0; pass < 2; ++pass) {
		bool ansi = pass == 0;

		memset(&viewer, 0, sizeof(viewer));
		fixture_viewer_initialize(&viewer, &stream,
		    retained_scoreboard, sizeof(retained_scoreboard) - 1U,
		    "YTSCORE.ASC", ansi, remote, sizeof(remote));
		CHECK(planet_take_one_accepted_cycle_run(&viewer, ansi, 1,
		    amount, 0U, &prompt_end, &editor_end, &body_end));
		CHECK(prompt_end == 77U && editor_end == 80U
		    && body_end == 115U && viewer.join.remote_length == 192U
		    && sizeof(expected) - 1U == 192U
		    && memcmp(remote, expected, sizeof(expected) - 1U) == 0
		    && viewer_bytes_fnv1a64(remote, viewer.join.remote_length)
		    == UINT64_C(0x50c2efb77d872847)
		    && viewer.join.local_row_count == 11U
		    && viewer_rows_fnv1a64(&viewer.join)
		    == UINT64_C(0xf8350788e2b5d6f3)
		    && viewer.join.local_fragment_length == 42U
		    && memcmp(viewer.join.local_fragment,
		    "Time: 14:59  Planet command (?=help) [A]? ", 42U) == 0
		    && viewer.join.local_color_count == (ansi ? 20U : 6U)
		    && viewer_colors_fnv1a64(&viewer.join)
		    == (ansi ? UINT64_C(0x6ffcc2ba1c2c6835)
		    : UINT64_C(0x17798e683d05096d))
		    && viewer.join.presentation.foreground == 6.0f
		    && viewer.join.presentation.background == 0.0f
		    && viewer.join.presentation.bold == 0.0f
		    && viewer.join.presentation.blink == 0.0f
		    && viewer.join.presentation.cached_foreground
		    == (ansi ? 6.0f : 0.0f)
		    && viewer.join.pager.foreground == 6
		    && viewer.join.pager.line_count == 2.0f
		    && viewer.join.pager.nonstop == 0.0f
		    && viewer.join.accumulator[0] == '\0'
		    && viewer.join.queue_length == 0U
		    && viewer.join.sample_calls == 6U
		    && viewer.join.event_count == 30U
		    && stream.eof_checks == 0U && stream.key_checks == 0U
		    && stream.read_count == 0U && stream.line_count == 0U
		    && !stream.file_open && !viewer.join.file_open
		    && viewer.input.file == NULL && viewer.close_calls == 0U
		    && viewer.open_calls == 0U);
		yt_text_input_destroy(&viewer.input);
	}
}

static void
test_planet_take_one_e_default_cycle_presentation(void)
{
	static const uint8_t amount[] = "1e3";
	static const uint8_t expected[] =
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? 1\r\n"
	    "\r\n<Take Ore>\n\r\r\nHow much [ 65 ]? 1e3\r\n"
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? ";
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	uint8_t remote[250];
	size_t prompt_end;
	size_t editor_end;
	size_t body_end;
	int pass;

	for (pass = 0; pass < 2; ++pass) {
		bool ansi = pass == 0;

		memset(&viewer, 0, sizeof(viewer));
		fixture_viewer_initialize(&viewer, &stream,
		    retained_scoreboard, sizeof(retained_scoreboard) - 1U,
		    "YTSCORE.ASC", ansi, remote, sizeof(remote));
		CHECK(planet_take_one_accepted_cycle_run(&viewer, ansi, 1,
		    amount, sizeof(amount) - 1U, &prompt_end, &editor_end,
		    &body_end));
		CHECK(prompt_end == 77U && editor_end == 80U
		    && body_end == 118U && viewer.join.remote_length == 195U
		    && sizeof(expected) - 1U == 195U
		    && memcmp(remote, expected, sizeof(expected) - 1U) == 0
		    && viewer_bytes_fnv1a64(remote, viewer.join.remote_length)
		    == UINT64_C(0xb52c1817e290744a)
		    && viewer.join.local_row_count == 11U
		    && viewer_rows_fnv1a64(&viewer.join)
		    == UINT64_C(0x73d2976c8d134b09)
		    && viewer.join.local_fragment_length == 42U
		    && memcmp(viewer.join.local_fragment,
		    "Time: 14:59  Planet command (?=help) [A]? ", 42U) == 0
		    && viewer.join.local_color_count == (ansi ? 20U : 6U)
		    && viewer_colors_fnv1a64(&viewer.join)
		    == (ansi ? UINT64_C(0x6ffcc2ba1c2c6835)
		    : UINT64_C(0x17798e683d05096d))
		    && viewer.join.presentation.foreground == 6.0f
		    && viewer.join.presentation.background == 0.0f
		    && viewer.join.presentation.bold == 0.0f
		    && viewer.join.presentation.blink == 0.0f
		    && viewer.join.presentation.cached_foreground
		    == (ansi ? 6.0f : 0.0f)
		    && viewer.join.pager.foreground == 6
		    && viewer.join.pager.line_count == 2.0f
		    && viewer.join.pager.nonstop == 0.0f
		    && strcmp(viewer.join.accumulator, "1e3") == 0
		    && viewer.join.queue_length == 0U
		    && viewer.join.sample_calls == 6U
		    && viewer.join.event_count == 30U
		    && stream.eof_checks == 0U && stream.key_checks == 0U
		    && stream.read_count == 0U && stream.line_count == 0U
		    && !stream.file_open && !viewer.join.file_open
		    && viewer.input.file == NULL && viewer.close_calls == 0U
		    && viewer.open_calls == 0U);
		yt_text_input_destroy(&viewer.input);
	}
}

static void
test_planet_take_one_zero_cycle_presentation(void)
{
	static const uint8_t amount[] = "0";
	static const uint8_t expected[] =
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? 1\r\n"
	    "\r\n<Take Ore>\n\r\r\nHow much [ 65 ]? 0\r\n"
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? ";
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	uint8_t remote[250];
	size_t prompt_end;
	size_t editor_end;
	size_t body_end;
	int pass;

	for (pass = 0; pass < 2; ++pass) {
		bool ansi = pass == 0;

		memset(&viewer, 0, sizeof(viewer));
		fixture_viewer_initialize(&viewer, &stream,
		    retained_scoreboard, sizeof(retained_scoreboard) - 1U,
		    "YTSCORE.ASC", ansi, remote, sizeof(remote));
		CHECK(planet_take_one_accepted_cycle_run(&viewer, ansi, 1,
		    amount, sizeof(amount) - 1U, &prompt_end, &editor_end,
		    &body_end));
		CHECK(prompt_end == 77U && editor_end == 80U
		    && body_end == 116U && viewer.join.remote_length == 193U
		    && sizeof(expected) - 1U == 193U
		    && memcmp(remote, expected, sizeof(expected) - 1U) == 0
		    && viewer_bytes_fnv1a64(remote, viewer.join.remote_length)
		    == UINT64_C(0xc30747beba0086e3)
		    && viewer.join.local_row_count == 11U
		    && viewer_rows_fnv1a64(&viewer.join)
		    == UINT64_C(0xfeba6500734caece)
		    && viewer.join.local_fragment_length == 42U
		    && memcmp(viewer.join.local_fragment,
		    "Time: 14:59  Planet command (?=help) [A]? ", 42U) == 0
		    && viewer.join.local_color_count == (ansi ? 20U : 6U)
		    && viewer_colors_fnv1a64(&viewer.join)
		    == (ansi ? UINT64_C(0x6ffcc2ba1c2c6835)
		    : UINT64_C(0x17798e683d05096d))
		    && viewer.join.presentation.foreground == 6.0f
		    && viewer.join.presentation.background == 0.0f
		    && viewer.join.presentation.bold == 0.0f
		    && viewer.join.presentation.blink == 0.0f
		    && viewer.join.presentation.cached_foreground
		    == (ansi ? 6.0f : 0.0f)
		    && viewer.join.pager.foreground == 6
		    && viewer.join.pager.line_count == 2.0f
		    && viewer.join.pager.nonstop == 0.0f
		    && strcmp(viewer.join.accumulator, "0") == 0
		    && viewer.join.queue_length == 0U
		    && viewer.join.sample_calls == 6U
		    && viewer.join.event_count == 30U
		    && stream.eof_checks == 0U && stream.key_checks == 0U
		    && stream.read_count == 0U && stream.line_count == 0U
		    && !stream.file_open && !viewer.join.file_open
		    && viewer.input.file == NULL && viewer.close_calls == 0U
		    && viewer.open_calls == 0U);
		yt_text_input_destroy(&viewer.input);
	}
}

static bool
planet_take_one_error_cycle_run(struct physical_viewer_join *viewer,
    bool ansi, const uint8_t *amount, size_t amount_length,
    const uint8_t *message, size_t message_length, size_t *prompt_end,
    size_t *editor_end, size_t *body_end)
{
	static const uint8_t free_holds[] =
	    "You have 65 free cargo holds.";
	static const uint8_t planet_prompt[] =
	    "Time: 14:59  Planet command (?=help) [A]? ";
	static const uint8_t command[] = "1";
	static const uint8_t amount_prompt[] = "How much [ 65 ]? ";
	const char *title = yt_planet_take_one_title(1);
	struct viewer_pager_join *join = &viewer->join;
	struct yt_present_result result;

	if (amount == NULL || amount_length >= sizeof(join->accumulator)
	    || message == NULL || prompt_end == NULL || editor_end == NULL
	    || body_end == NULL
	    || title == NULL || strcmp(title, "<Take Ore>") != 0)
		return false;
	join->presentation = state(ansi);
	join->presentation.foreground = 6.0f;
	join->presentation.cached_foreground = ansi ? 6.0f : 0.0f;
	join->pager.foreground = 6;
	join->pager.line_count = 0.0f;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, free_holds,
	    sizeof(free_holds) - 1U, 0.0f)
	    || !normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, planet_prompt,
	    sizeof(planet_prompt) - 1U, 1.0f))
		return false;
	*prompt_end = join->remote_length;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	memcpy(join->accumulator, command, sizeof(command));
	if (yt_present_editor_echo(command, sizeof(command) - 1U,
	    command, sizeof(command) - 1U, &join->presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	*editor_end = join->remote_length;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, (const uint8_t *)title,
	    strlen(title), 0.0f)
	    || !normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, amount_prompt,
	    sizeof(amount_prompt) - 1U, 1.0f))
		return false;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	memcpy(join->accumulator, amount, amount_length);
	join->accumulator[amount_length] = '\0';
	if (yt_present_editor_echo(amount, amount_length,
	    amount, amount_length, &join->presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_line(join, NULL, 0U))
		return false;
	/* YT:02DB clears pending input before the styled error row. */
	join->queue_position = 0U;
	join->queue_length = 0U;
	join->queue[0] = '\0';
	join->presentation.bold = 1.0f;
	join->presentation.blink = 1.0f;
	if (!normal_exit_b05d(join, message, message_length, 0.0f))
		return false;
	*body_end = join->remote_length;
	join->pager.line_count = 0.0f;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, free_holds,
	    sizeof(free_holds) - 1U, 0.0f)
	    || !normal_exit_line(join, NULL, 0U))
		return false;
	join->presentation.foreground = 6.0f;
	join->pager.foreground = 6;
	return normal_exit_b05d(join, planet_prompt,
	    sizeof(planet_prompt) - 1U, 1.0f);
}

static void
test_planet_take_one_stock_error_cycle_presentation(void)
{
	static const uint8_t amount[] = "102";
	static const uint8_t message[] = "They don't have that many.";
	static const uint8_t ansi[] =
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? 1\r\n"
	    "\r\n<Take Ore>\n\r\r\nHow much [ 65 ]? 102\r\n"
	    "\r\n\x1b[0;36;40;5;1mThey don't have that many.\n\r"
	    "\x1b[0;36;40m\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? ";
	static const uint8_t plain[] =
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? 1\r\n"
	    "\r\n<Take Ore>\n\r\r\nHow much [ 65 ]? 102\r\n"
	    "\r\nThey don't have that many.\n\r"
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? ";
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	uint8_t remote[280];
	size_t prompt_end;
	size_t editor_end;
	size_t body_end;
	int pass;

	for (pass = 0; pass < 2; ++pass) {
		bool ansi_mode = pass == 0;
		const uint8_t *expected = ansi_mode ? ansi : plain;
		size_t expected_length = ansi_mode
		    ? sizeof(ansi) - 1U : sizeof(plain) - 1U;

		memset(&viewer, 0, sizeof(viewer));
		fixture_viewer_initialize(&viewer, &stream,
		    retained_scoreboard, sizeof(retained_scoreboard) - 1U,
		    "YTSCORE.ASC", ansi_mode, remote, sizeof(remote));
		CHECK(planet_take_one_error_cycle_run(&viewer, ansi_mode,
		    amount, sizeof(amount) - 1U, message, sizeof(message) - 1U,
		    &prompt_end, &editor_end, &body_end));
		CHECK(prompt_end == 77U && editor_end == 80U
		    && body_end == (ansi_mode ? 162U : 148U)
		    && viewer.join.remote_length == expected_length
		    && expected_length == (ansi_mode ? 249U : 225U)
		    && memcmp(remote, expected, expected_length) == 0
		    && viewer_bytes_fnv1a64(remote, expected_length)
		    == (ansi_mode ? UINT64_C(0x9e5237b421267f0e)
		    : UINT64_C(0xa2428df53ea81f52))
		    && viewer.join.local_row_count == 13U
		    && viewer_rows_fnv1a64(&viewer.join)
		    == UINT64_C(0x92968389684fd139)
		    && viewer.join.local_fragment_length == 42U
		    && memcmp(viewer.join.local_fragment,
		    "Time: 14:59  Planet command (?=help) [A]? ", 42U) == 0
		    && viewer.join.local_color_count == (ansi_mode ? 23U : 7U)
		    && viewer_colors_fnv1a64(&viewer.join)
		    == (ansi_mode ? UINT64_C(0x545b7944971a4be2)
		    : UINT64_C(0xc68cf2d74ffb7ffa))
		    && viewer.join.presentation.foreground == 6.0f
		    && viewer.join.presentation.background == 0.0f
		    && viewer.join.presentation.bold == (ansi_mode ? 0.0f : 1.0f)
		    && viewer.join.presentation.blink == (ansi_mode ? 0.0f : 1.0f)
		    && viewer.join.presentation.cached_foreground
		    == (ansi_mode ? 6.0f : 0.0f)
		    && viewer.join.pager.foreground == 6
		    && viewer.join.pager.line_count == 2.0f
		    && viewer.join.pager.nonstop == 0.0f
		    && strcmp(viewer.join.accumulator, "102") == 0
		    && viewer.join.queue_position == 0U
		    && viewer.join.queue_length == 0U
		    && viewer.join.sample_calls == 7U
		    && viewer.join.event_count == 35U
		    && stream.eof_checks == 0U && stream.key_checks == 0U
		    && stream.read_count == 0U && stream.line_count == 0U
		    && !stream.file_open && !viewer.join.file_open
		    && viewer.input.file == NULL && viewer.close_calls == 0U
		    && viewer.open_calls == 0U);
		yt_text_input_destroy(&viewer.input);
	}
}

static void
test_planet_take_one_capacity_error_cycle_presentation(void)
{
	static const uint8_t amount[] = "66";
	static const uint8_t message[] = "You can't take that much!";
	static const uint8_t ansi[] =
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? 1\r\n"
	    "\r\n<Take Ore>\n\r\r\nHow much [ 65 ]? 66\r\n"
	    "\r\n\x1b[0;36;40;5;1mYou can't take that much!\n\r"
	    "\x1b[0;36;40m\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? ";
	static const uint8_t plain[] =
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? 1\r\n"
	    "\r\n<Take Ore>\n\r\r\nHow much [ 65 ]? 66\r\n"
	    "\r\nYou can't take that much!\n\r"
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? ";
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	uint8_t remote[280];
	size_t prompt_end;
	size_t editor_end;
	size_t body_end;
	int pass;

	for (pass = 0; pass < 2; ++pass) {
		bool ansi_mode = pass == 0;
		const uint8_t *expected = ansi_mode ? ansi : plain;
		size_t expected_length = ansi_mode
		    ? sizeof(ansi) - 1U : sizeof(plain) - 1U;

		memset(&viewer, 0, sizeof(viewer));
		fixture_viewer_initialize(&viewer, &stream,
		    retained_scoreboard, sizeof(retained_scoreboard) - 1U,
		    "YTSCORE.ASC", ansi_mode, remote, sizeof(remote));
		CHECK(planet_take_one_error_cycle_run(&viewer, ansi_mode,
		    amount, sizeof(amount) - 1U, message, sizeof(message) - 1U,
		    &prompt_end, &editor_end, &body_end));
		CHECK(prompt_end == 77U && editor_end == 80U
		    && body_end == (ansi_mode ? 160U : 146U)
		    && viewer.join.remote_length == expected_length
		    && expected_length == (ansi_mode ? 247U : 223U)
		    && memcmp(remote, expected, expected_length) == 0
		    && viewer_bytes_fnv1a64(remote, expected_length)
		    == (ansi_mode ? UINT64_C(0x502635455717481f)
		    : UINT64_C(0xe3e2fe6bbbe431df))
		    && viewer.join.local_row_count == 13U
		    && viewer_rows_fnv1a64(&viewer.join)
		    == UINT64_C(0xc1a18b60eab17766)
		    && viewer.join.local_fragment_length == 42U
		    && memcmp(viewer.join.local_fragment,
		    "Time: 14:59  Planet command (?=help) [A]? ", 42U) == 0
		    && viewer.join.local_color_count == (ansi_mode ? 23U : 7U)
		    && viewer_colors_fnv1a64(&viewer.join)
		    == (ansi_mode ? UINT64_C(0x545b7944971a4be2)
		    : UINT64_C(0xc68cf2d74ffb7ffa))
		    && viewer.join.presentation.foreground == 6.0f
		    && viewer.join.presentation.background == 0.0f
		    && viewer.join.presentation.bold == (ansi_mode ? 0.0f : 1.0f)
		    && viewer.join.presentation.blink == (ansi_mode ? 0.0f : 1.0f)
		    && viewer.join.presentation.cached_foreground
		    == (ansi_mode ? 6.0f : 0.0f)
		    && viewer.join.pager.foreground == 6
		    && viewer.join.pager.line_count == 2.0f
		    && viewer.join.pager.nonstop == 0.0f
		    && strcmp(viewer.join.accumulator, "66") == 0
		    && viewer.join.queue_position == 0U
		    && viewer.join.queue_length == 0U
		    && viewer.join.sample_calls == 7U
		    && viewer.join.event_count == 35U
		    && stream.eof_checks == 0U && stream.key_checks == 0U
		    && stream.read_count == 0U && stream.line_count == 0U
		    && !stream.file_open && !viewer.join.file_open
		    && viewer.input.file == NULL && viewer.close_calls == 0U
		    && viewer.open_calls == 0U);
		yt_text_input_destroy(&viewer.input);
	}
}

static void
test_planet_take_one_negative_error_cycle_presentation(void)
{
	static const uint8_t amount[] = "-1";
	static const uint8_t message[] = "They don't have that many.";
	static const uint8_t ansi[] =
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? 1\r\n"
	    "\r\n<Take Ore>\n\r\r\nHow much [ 65 ]? -1\r\n"
	    "\r\n\x1b[0;36;40;5;1mThey don't have that many.\n\r"
	    "\x1b[0;36;40m\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? ";
	static const uint8_t plain[] =
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? 1\r\n"
	    "\r\n<Take Ore>\n\r\r\nHow much [ 65 ]? -1\r\n"
	    "\r\nThey don't have that many.\n\r"
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? ";
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	uint8_t remote[280];
	size_t prompt_end;
	size_t editor_end;
	size_t body_end;
	int pass;

	for (pass = 0; pass < 2; ++pass) {
		bool ansi_mode = pass == 0;
		const uint8_t *expected = ansi_mode ? ansi : plain;
		size_t expected_length = ansi_mode
		    ? sizeof(ansi) - 1U : sizeof(plain) - 1U;

		memset(&viewer, 0, sizeof(viewer));
		fixture_viewer_initialize(&viewer, &stream,
		    retained_scoreboard, sizeof(retained_scoreboard) - 1U,
		    "YTSCORE.ASC", ansi_mode, remote, sizeof(remote));
		CHECK(planet_take_one_error_cycle_run(&viewer, ansi_mode,
		    amount, sizeof(amount) - 1U, message, sizeof(message) - 1U,
		    &prompt_end, &editor_end, &body_end));
		CHECK(prompt_end == 77U && editor_end == 80U
		    && body_end == (ansi_mode ? 161U : 147U)
		    && viewer.join.remote_length == expected_length
		    && expected_length == (ansi_mode ? 248U : 224U)
		    && memcmp(remote, expected, expected_length) == 0
		    && viewer_bytes_fnv1a64(remote, expected_length)
		    == (ansi_mode ? UINT64_C(0x317b7dc7b83fbf27)
		    : UINT64_C(0x6ade58e6b4363e3b))
		    && viewer.join.local_row_count == 13U
		    && viewer_rows_fnv1a64(&viewer.join)
		    == UINT64_C(0x42d9ac5ff8f00851)
		    && viewer.join.local_fragment_length == 42U
		    && memcmp(viewer.join.local_fragment,
		    "Time: 14:59  Planet command (?=help) [A]? ", 42U) == 0
		    && viewer.join.local_color_count == (ansi_mode ? 23U : 7U)
		    && viewer_colors_fnv1a64(&viewer.join)
		    == (ansi_mode ? UINT64_C(0x545b7944971a4be2)
		    : UINT64_C(0xc68cf2d74ffb7ffa))
		    && viewer.join.presentation.foreground == 6.0f
		    && viewer.join.presentation.background == 0.0f
		    && viewer.join.presentation.bold == (ansi_mode ? 0.0f : 1.0f)
		    && viewer.join.presentation.blink == (ansi_mode ? 0.0f : 1.0f)
		    && viewer.join.presentation.cached_foreground
		    == (ansi_mode ? 6.0f : 0.0f)
		    && viewer.join.pager.foreground == 6
		    && viewer.join.pager.line_count == 2.0f
		    && viewer.join.pager.nonstop == 0.0f
		    && strcmp(viewer.join.accumulator, "-1") == 0
		    && viewer.join.queue_position == 0U
		    && viewer.join.queue_length == 0U
		    && viewer.join.sample_calls == 7U
		    && viewer.join.event_count == 35U
		    && stream.eof_checks == 0U && stream.key_checks == 0U
		    && stream.read_count == 0U && stream.line_count == 0U
		    && !stream.file_open && !viewer.join.file_open
		    && viewer.input.file == NULL && viewer.close_calls == 0U
		    && viewer.open_calls == 0U);
		yt_text_input_destroy(&viewer.input);
	}
}

static bool
planet_leave_cycle_run(struct physical_viewer_join *viewer, bool ansi,
    size_t *prompt_end, size_t *editor_end, size_t *reentry_end)
{
	static const uint8_t free_holds[] =
	    "You have 65 free cargo holds.";
	static const uint8_t planet_prompt[] =
	    "Time: 14:59  Planet command (?=help) [A]? ";
	static const uint8_t command[] = "L";
	static const uint8_t sector[] = "Sector: 733";
	static const uint8_t warps[] = "Warps lead to: 2, 9";
	static const uint8_t main_prompt[] =
	    "Time: 14:59  Main Command (?=Help)? ";
	struct viewer_pager_join *join = &viewer->join;
	struct yt_present_result result;

	if (prompt_end == NULL || editor_end == NULL || reentry_end == NULL)
		return false;
	join->presentation = state(ansi);
	join->presentation.foreground = 6.0f;
	join->presentation.cached_foreground = ansi ? 6.0f : 0.0f;
	join->pager.foreground = 6;
	join->pager.line_count = 0.0f;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, free_holds,
	    sizeof(free_holds) - 1U, 0.0f)
	    || !normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, planet_prompt,
	    sizeof(planet_prompt) - 1U, 1.0f))
		return false;
	*prompt_end = join->remote_length;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	memcpy(join->accumulator, command, sizeof(command));
	if (yt_present_editor_echo(command, sizeof(command) - 1U,
	    command, sizeof(command) - 1U, &join->presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	*editor_end = join->remote_length;

	/* Position thirteen transfers directly into one mode-zero scan. */
	join->presentation.foreground = 1.0f;
	join->pager.foreground = 1;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_line(join, sector, sizeof(sector) - 1U)
	    || !normal_exit_line(join, warps, sizeof(warps) - 1U))
		return false;
	join->pager.line_count = 0.0f;
	join->presentation.foreground = 2.0f;
	join->pager.foreground = 2;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, main_prompt,
	    sizeof(main_prompt) - 1U, 1.0f))
		return false;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	*reentry_end = join->remote_length;
	return true;
}

static void
test_planet_leave_cycle_presentation(void)
{
	static const uint8_t ansi[] =
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? L\r\n"
	    "\x1b[0;31;40m\r\nSector: 733\r\nWarps lead to: 2, 9\r\n"
	    "\x1b[0;32;40m\r\nTime: 14:59  Main Command (?=Help)? ";
	static const uint8_t plain[] =
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? L\r\n"
	    "\r\nSector: 733\r\nWarps lead to: 2, 9\r\n"
	    "\r\nTime: 14:59  Main Command (?=Help)? ";
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	uint8_t remote[220];
	size_t prompt_end;
	size_t editor_end;
	size_t reentry_end;
	int pass;

	for (pass = 0; pass < 2; ++pass) {
		bool ansi_mode = pass == 0;
		const uint8_t *expected = ansi_mode ? ansi : plain;
		size_t expected_length = ansi_mode
		    ? sizeof(ansi) - 1U : sizeof(plain) - 1U;

		memset(&viewer, 0, sizeof(viewer));
		fixture_viewer_initialize(&viewer, &stream,
		    retained_scoreboard, sizeof(retained_scoreboard) - 1U,
		    "YTSCORE.ASC", ansi_mode, remote, sizeof(remote));
		CHECK(planet_leave_cycle_run(&viewer, ansi_mode, &prompt_end,
		    &editor_end, &reentry_end));
		CHECK(prompt_end == 77U && editor_end == 80U
		    && reentry_end == expected_length
		    && viewer.join.remote_length == expected_length
		    && memcmp(remote, expected, expected_length) == 0
		    && viewer.join.local_row_count == 8U
		    && viewer_rows_fnv1a64(&viewer.join)
		    == UINT64_C(0xa99e3456f96be29c)
		    && viewer.join.local_color_count
		    == (ansi_mode ? 13U : 3U)
		    && viewer_colors_fnv1a64(&viewer.join)
		    == (ansi_mode ? UINT64_C(0xe689d5c366e01845)
		    : UINT64_C(0x2207a27a6260aaca))
		    && viewer.join.presentation.foreground == 2.0f
		    && viewer.join.presentation.background == 0.0f
		    && viewer.join.presentation.bold == 0.0f
		    && viewer.join.presentation.blink == 0.0f
		    && viewer.join.presentation.cached_foreground
		    == (ansi_mode ? 2.0f : 0.0f)
		    && viewer.join.pager.foreground == 2
		    && viewer.join.pager.line_count == 0.0f
		    && viewer.join.pager.nonstop == 0.0f
		    && viewer.join.local_fragment_length == 36U
		    && memcmp(viewer.join.local_fragment,
		    "Time: 14:59  Main Command (?=Help)? ", 36U) == 0
		    && viewer.join.accumulator[0] == '\0'
		    && viewer.join.queue_length == 0U
		    && viewer.join.sample_calls == 3U
		    && viewer.join.event_count == 15U
		    && stream.eof_checks == 0U && stream.key_checks == 0U
		    && stream.read_count == 0U && stream.line_count == 0U
		    && !stream.file_open && !viewer.join.file_open
		    && viewer.input.file == NULL && viewer.close_calls == 0U
		    && viewer.open_calls == 0U);
		yt_text_input_destroy(&viewer.input);
	}
	CHECK(sizeof(ansi) - 1U == 174U && sizeof(plain) - 1U == 154U);
}

static bool
planet_thrusters_accepted_cycle_run(struct physical_viewer_join *viewer,
    bool ansi, size_t ends[7])
{
	static const uint8_t free_holds[] =
	    "You have 65 free cargo holds.";
	static const uint8_t planet_prompt[] =
	    "Time: 14:59  Planet command (?=help) [A]? ";
	static const uint8_t command[] = "!";
	static const uint8_t cost[] =
	    "Moving planets costs 10 turns per sector.";
	static const uint8_t sector_one[] = "Sector: 1";
	static const uint8_t sector_two[] = "Sector: 2";
	static const uint8_t planet[] = "Planet: Gaia * Forces: 0";
	static const uint8_t warps[] = "Warps lead to:";
	static const uint8_t warp_two[] = " 2";
	static const uint8_t destination[] = "Move planet to what sector? ";
	static const uint8_t destination_response[] = "2";
	static const uint8_t working[] = "Working. ";
	static const uint8_t confirmation[] = "Move the planet? (Y/[N])";
	static const uint8_t confirmation_response[] = "Y";
	static const uint8_t engaged[] = "Planet thrusters engaged.";
	static const uint8_t moving[] = "Moving to sector:";
	static const uint8_t main_prompt[] =
	    "Time: 14:59  Main Command (?=Help)? ";
	struct viewer_pager_join *join = &viewer->join;
	struct yt_present_result result;
	uint8_t row[256];
	size_t row_length;
	char number[64];
	int number_length;

	if (ends == NULL)
		return false;
	join->presentation = state(ansi);
	join->presentation.foreground = 6.0f;
	join->presentation.cached_foreground = ansi ? 6.0f : 0.0f;
	join->pager.foreground = 6;
	join->pager.line_count = 0.0f;
	if (!ansi) {
		join->presentation.sound.user_sound = 0.0f;
		join->presentation.sound.local_sound = 0.0f;
	}
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, free_holds,
	    sizeof(free_holds) - 1U, 0.0f)
	    || !normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, planet_prompt,
	    sizeof(planet_prompt) - 1U, 1.0f))
		return false;
	ends[0] = join->remote_length;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	memcpy(join->accumulator, command, sizeof(command));
	if (yt_present_editor_echo(command, sizeof(command) - 1U,
	    command, sizeof(command) - 1U, &join->presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	ends[1] = join->remote_length;

	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, cost, sizeof(cost) - 1U, 0.0f))
		return false;
	ends[2] = join->remote_length;
	join->presentation.foreground = 1.0f;
	join->pager.foreground = 1;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_line(join, sector_one, sizeof(sector_one) - 1U))
		return false;
	join->presentation.foreground = 3.0f;
	join->pager.foreground = 3;
	if (!sensor_join_present(join, &(const struct sensor_join_output){
	    SENSOR_JOIN_BOLD_LINE, (const char *)planet}))
		return false;
	join->presentation.foreground = 1.0f;
	join->pager.foreground = 1;
	if (!sensor_join_present(join, &(const struct sensor_join_output){
	    SENSOR_JOIN_RAW, (const char *)warps})
	    || !sensor_join_present(join, &(const struct sensor_join_output){
	    SENSOR_JOIN_RAW, (const char *)warp_two})
	    || !normal_exit_line(join, NULL, 0U))
		return false;
	ends[3] = join->remote_length;

	join->presentation.foreground = 6.0f;
	join->pager.foreground = 6;
	if (!normal_exit_b05d(join, destination,
	    sizeof(destination) - 1U, 1.0f))
		return false;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	memcpy(join->accumulator, destination_response,
	    sizeof(destination_response));
	if (yt_present_editor_echo(destination_response,
	    sizeof(destination_response) - 1U, destination_response,
	    sizeof(destination_response) - 1U, &join->presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, working, sizeof(working) - 1U, 1.0f)
	    || !yt_planet_move_path_heading(1.0f, 2.0f, row,
	    sizeof(row), &row_length)
	    || !normal_exit_b05d(join, row, row_length, 0.0f)
	    || !normal_exit_line(join, NULL, 0U))
		return false;
	number_length = qb_str_single(number, sizeof(number), 1.0f);
	if (number_length < 0 || !normal_exit_b05d(join,
	    (const uint8_t *)number, (size_t)number_length, 1.0f))
		return false;
	number_length = qb_str_single(number, sizeof(number), 2.0f);
	join->pager.line_count = 0.0f;
	if (number_length < 0 || !normal_exit_b05d(join,
	    (const uint8_t *)number, (size_t)number_length, 1.0f)
	    || !normal_exit_line(join, NULL, 0U)
	    || !normal_exit_line(join, NULL, 0U)
	    || !yt_planet_move_summary(10.0f, row, sizeof(row), &row_length)
	    || !normal_exit_b05d(join, row, row_length, 0.0f)
	    || !yt_planet_move_turns_row(100.0f, row, sizeof(row), &row_length)
	    || !normal_exit_b05d(join, row, row_length, 0.0f)
	    || yt_present_character(confirmation, sizeof(confirmation) - 1U,
	    &join->presentation, &result) != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	memcpy(join->accumulator, confirmation_response,
	    sizeof(confirmation_response));
	if (yt_present_editor_echo(confirmation_response,
	    sizeof(confirmation_response) - 1U, confirmation_response,
	    sizeof(confirmation_response) - 1U, &join->presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	join->presentation.bold = 1.0f;
	join->presentation.blink = 1.0f;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, engaged, sizeof(engaged) - 1U, 0.0f))
		return false;
	join->pager.line_count = 0.0f;
	if (!normal_exit_b05d(join, moving, sizeof(moving) - 1U, 1.0f))
		return false;
	ends[4] = join->remote_length;
	number_length = qb_str_single(number, sizeof(number), 2.0f);
	if (number_length < 0 || !normal_exit_b05d(join,
	    (const uint8_t *)number, (size_t)number_length, 1.0f))
		return false;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_line(join, NULL, 0U)
	    || !yt_planet_move_success_row((const uint8_t *)"Gaia", 4U,
	    row, sizeof(row), &row_length)
	    || !normal_exit_line(join, row, row_length)
	    || !sensor_join_sound(join))
		return false;
	ends[5] = join->remote_length;

	join->presentation.foreground = 1.0f;
	join->pager.foreground = 1;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_line(join, sector_two, sizeof(sector_two) - 1U))
		return false;
	join->presentation.foreground = 3.0f;
	join->pager.foreground = 3;
	if (!sensor_join_present(join, &(const struct sensor_join_output){
	    SENSOR_JOIN_BOLD_LINE, (const char *)planet}))
		return false;
	join->presentation.foreground = 1.0f;
	join->pager.foreground = 1;
	if (!sensor_join_present(join, &(const struct sensor_join_output){
	    SENSOR_JOIN_RAW, (const char *)warps})
	    || !normal_exit_line(join, NULL, 0U))
		return false;
	join->pager.line_count = 0.0f;
	join->presentation.foreground = 2.0f;
	join->pager.foreground = 2;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, main_prompt,
	    sizeof(main_prompt) - 1U, 1.0f))
		return false;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	ends[6] = join->remote_length;
	return true;
}

static void
test_planet_thrusters_accepted_cycle_presentation(void)
{
	static const uint8_t plain[] =
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? !\r\n"
	    "\r\nMoving planets costs 10 turns per sector.\n\r"
	    "\r\nSector: 1\r\nPlanet: Gaia * Forces: 0\r\n"
	    "Warps lead to: 2\r\nMove planet to what sector? 2\r\n"
	    "\r\nWorking. The shortest path from sector 1 to sector 2 is:\n\r"
	    "\r\n 1 2\r\n\r\nDistance is 1 and will take 10 turns.\n\r"
	    "You have 100 turns left.\n\rMove the planet? (Y/[N])Y\r\n"
	    "\r\nPlanet thrusters engaged.\n\rMoving to sector: 2\r\n"
	    "\r\nGaia moved! (Xannoron Movers, we move anyTHING, anyWHERE!)\r\n"
	    "\r\nSector: 2\r\nPlanet: Gaia * Forces: 0\r\n"
	    "Warps lead to:\r\n\r\nTime: 14:59  Main Command (?=Help)? ";
	static const uint8_t ansi[] =
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? !\r\n"
	    "\r\nMoving planets costs 10 turns per sector.\n\r"
	    "\x1b[0;31;40m\r\nSector: 1\r\n"
	    "\x1b[0;33;40;1mPlanet: Gaia * Forces: 0\r\n"
	    "\x1b[0;31;40mWarps lead to: 2\r\n"
	    "\x1b[0;36;40mMove planet to what sector? 2\r\n"
	    "\r\nWorking. The shortest path from sector 1 to sector 2 is:\n\r"
	    "\r\n 1 2\r\n\r\nDistance is 1 and will take 10 turns.\n\r"
	    "You have 100 turns left.\n\rMove the planet? (Y/[N])Y\r\n"
	    "\x1b[0;36;40;5;1m\r\n"
	    "\x1b[0;36;40mPlanet thrusters engaged.\n\r"
	    "Moving to sector: 2\r\n\r\n"
	    "Gaia moved! (Xannoron Movers, we move anyTHING, anyWHERE!)\r\n"
	    "\x1b[MBT128O5L48P64CP64C\x0e"
	    "\x1b[0;31;40m\r\nSector: 2\r\n"
	    "\x1b[0;33;40;1mPlanet: Gaia * Forces: 0\r\n"
	    "\x1b[0;31;40mWarps lead to:\r\n"
	    "\x1b[0;32;40m\r\nTime: 14:59  Main Command (?=Help)? ";
	static const struct {
		bool ansi;
		const uint8_t *expected;
		size_t expected_length;
		size_t ends[7];
	} cases[] = {
		{false, plain, sizeof(plain) - 1U,
		    {77U, 80U, 125U, 182U, 421U, 487U, 580U}},
		{true, ansi, sizeof(ansi) - 1U,
		    {77U, 80U, 125U, 214U, 487U, 575U, 710U}},
	};
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	uint8_t remote[760];
	size_t ends[7];
	size_t pass;

	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		memset(&viewer, 0, sizeof(viewer));
		fixture_viewer_initialize(&viewer, &stream,
		    retained_scoreboard, sizeof(retained_scoreboard) - 1U,
		    "YTSCORE.ASC", cases[pass].ansi, remote, sizeof(remote));
		CHECK(planet_thrusters_accepted_cycle_run(&viewer,
		    cases[pass].ansi, ends));
		CHECK(memcmp(ends, cases[pass].ends, sizeof(ends)) == 0
		    && viewer.join.remote_length == cases[pass].expected_length
		    && memcmp(remote, cases[pass].expected,
		    cases[pass].expected_length) == 0
		    && viewer.join.presentation.foreground == 2.0f
		    && viewer.join.presentation.background == 0.0f
		    && viewer.join.presentation.bold
		    == (cases[pass].ansi ? 0.0f : 1.0f)
		    && viewer.join.presentation.blink
		    == (cases[pass].ansi ? 0.0f : 1.0f)
		    && viewer.join.presentation.cached_foreground
		    == (cases[pass].ansi ? 2.0f : 0.0f)
		    && viewer.join.pager.foreground == 2
		    && viewer.join.pager.line_count == 0.0f
		    && viewer.join.pager.nonstop == 0.0f
		    && viewer.join.local_row_count == 29U
		    && viewer_rows_fnv1a64(&viewer.join)
		    == UINT64_C(0x17e3257096542482)
		    && viewer.join.local_color_count
		    == (cases[pass].ansi ? 55U : 14U)
		    && viewer_colors_fnv1a64(&viewer.join)
		    == (cases[pass].ansi
		    ? UINT64_C(0xa58d99221dc72c11)
		    : UINT64_C(0x4692bc1a44da0ecd))
		    && viewer.join.local_fragment_length == 36U
		    && memcmp(viewer.join.local_fragment,
		    "Time: 14:59  Main Command (?=Help)? ", 36U) == 0
		    && viewer.join.accumulator[0] == '\0'
		    && viewer.join.queue_length == 0U
		    && viewer.join.sample_calls == 14U
		    && viewer.join.event_count == 70U
		    && stream.eof_checks == 0U && stream.key_checks == 0U
		    && stream.read_count == 0U && stream.line_count == 0U
		    && !stream.file_open && !viewer.join.file_open
		    && viewer.input.file == NULL && viewer.close_calls == 0U
		    && viewer.open_calls == 0U);
		yt_text_input_destroy(&viewer.input);
	}
	CHECK(sizeof(plain) - 1U == 580U && sizeof(ansi) - 1U == 710U);
}

static bool
main_movement_accepted_cycle_run(struct physical_viewer_join *viewer,
    bool ansi, size_t ends[3])
{
	static const uint8_t main_prompt[] =
	    "Time: 14:59  Main Command (?=Help)? ";
	static const uint8_t command[] = "M";
	static const uint8_t destination_prompt[] = "Move to which sector? ";
	static const uint8_t destination[] = "42";
	static const uint8_t finalizer_row[] =
	    "One Turn Deducted, 59 left.";
	static const uint8_t sector[] = "Sector: 42";
	static const uint8_t warps[] = "Warps lead to:";
	static const uint8_t warp_one[] = " 12";
	static const uint8_t warp_two[] = ", 99";
	static const float warp_values[6] = {
		7.0f, 42.0f, 0.0f, 0.0f, 12.5f, 0.0f
	};
	struct viewer_pager_join *join = &viewer->join;
	struct yt_present_result result;
	uint8_t row[128];
	size_t row_length;

	if (ends == NULL)
		return false;
	join->presentation = state(ansi);
	join->presentation.foreground = 6.0f;
	join->presentation.cached_foreground = ansi ? 6.0f : 0.0f;
	join->pager.foreground = 6;
	join->pager.line_count = 8.0f;

	join->presentation.foreground = 2.0f;
	join->pager.foreground = 2;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, main_prompt,
	    sizeof(main_prompt) - 1U, 1.0f))
		return false;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	memcpy(join->accumulator, command, sizeof(command));
	if (yt_present_editor_echo(command, sizeof(command) - 1U,
	    command, sizeof(command) - 1U, &join->presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	ends[0] = join->remote_length;

	if (!yt_movement_warp_row(warp_values, row, sizeof(row), &row_length)
	    || !normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, row, row_length, 0.0f)
	    || !normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, destination_prompt,
	    sizeof(destination_prompt) - 1U, 1.0f))
		return false;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	memcpy(join->accumulator, destination, sizeof(destination));
	if (yt_present_editor_echo(destination, sizeof(destination) - 1U,
	    destination, sizeof(destination) - 1U,
	    &join->presentation, &result) != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, finalizer_row,
	    sizeof(finalizer_row) - 1U, 0.0f))
		return false;
	ends[1] = join->remote_length;

	join->presentation.foreground = 1.0f;
	join->pager.foreground = 1;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_line(join, sector, sizeof(sector) - 1U)
	    || !sensor_join_present(join, &(const struct sensor_join_output){
	    SENSOR_JOIN_RAW, (const char *)warps})
	    || !sensor_join_present(join, &(const struct sensor_join_output){
	    SENSOR_JOIN_RAW, (const char *)warp_one})
	    || !sensor_join_present(join, &(const struct sensor_join_output){
	    SENSOR_JOIN_RAW, (const char *)warp_two})
	    || !normal_exit_line(join, NULL, 0U))
		return false;
	join->pager.line_count = 0.0f;
	join->presentation.foreground = 2.0f;
	join->pager.foreground = 2;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, main_prompt,
	    sizeof(main_prompt) - 1U, 1.0f))
		return false;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	ends[2] = join->remote_length;
	return true;
}

static void
test_main_movement_accepted_cycle_presentation(void)
{
	static const uint8_t plain[] =
	    "\r\nTime: 14:59  Main Command (?=Help)? M\r\n"
	    "\r\nWarps lead to, 7, 42, 12.5\n\r"
	    "\r\nMove to which sector? 42\r\n"
	    "\r\nOne Turn Deducted, 59 left.\n\r"
	    "\r\nSector: 42\r\nWarps lead to: 12, 99\r\n"
	    "\r\nTime: 14:59  Main Command (?=Help)? ";
	static const uint8_t ansi[] =
	    "\x1b[0;32;40m\r\nTime: 14:59  Main Command (?=Help)? M\r\n"
	    "\r\nWarps lead to, 7, 42, 12.5\n\r"
	    "\r\nMove to which sector? 42\r\n"
	    "\r\nOne Turn Deducted, 59 left.\n\r"
	    "\x1b[0;31;40m\r\nSector: 42\r\nWarps lead to: 12, 99\r\n"
	    "\x1b[0;32;40m\r\nTime: 14:59  Main Command (?=Help)? ";
	static const struct {
		bool ansi;
		const uint8_t *expected;
		size_t expected_length;
		size_t ends[3];
	} cases[] = {
		{false, plain, sizeof(plain) - 1U, {41U, 130U, 205U}},
		{true, ansi, sizeof(ansi) - 1U, {51U, 140U, 235U}},
	};
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	uint8_t remote[260];
	size_t ends[3];
	size_t pass;

	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		memset(&viewer, 0, sizeof(viewer));
		fixture_viewer_initialize(&viewer, &stream,
		    retained_scoreboard, sizeof(retained_scoreboard) - 1U,
		    "YTSCORE.ASC", cases[pass].ansi, remote, sizeof(remote));
		CHECK(main_movement_accepted_cycle_run(&viewer,
		    cases[pass].ansi, ends));
		CHECK(memcmp(ends, cases[pass].ends, sizeof(ends)) == 0
		    && viewer.join.remote_length == cases[pass].expected_length
		    && memcmp(remote, cases[pass].expected,
		    cases[pass].expected_length) == 0
		    && viewer.join.presentation.foreground == 2.0f
		    && viewer.join.presentation.background == 0.0f
		    && viewer.join.presentation.bold == 0.0f
		    && viewer.join.presentation.blink == 0.0f
		    && viewer.join.presentation.cached_foreground
		    == (cases[pass].ansi ? 2.0f : 0.0f)
		    && viewer.join.pager.foreground == 2
		    && viewer.join.pager.line_count == 0.0f
		    && viewer.join.pager.nonstop == 0.0f
		    && viewer.join.local_fragment_length == 36U
		    && memcmp(viewer.join.local_fragment,
		    "Time: 14:59  Main Command (?=Help)? ", 36U) == 0
		    && viewer.join.accumulator[0] == '\0'
		    && viewer.join.queue_length == 0U
		    && stream.eof_checks == 0U && stream.key_checks == 0U
		    && stream.read_count == 0U && stream.line_count == 0U
		    && !stream.file_open && !viewer.join.file_open
		    && viewer.input.file == NULL && viewer.close_calls == 0U
		    && viewer.open_calls == 0U);
		yt_text_input_destroy(&viewer.input);
	}
	CHECK(sizeof(plain) - 1U == 205U && sizeof(ansi) - 1U == 235U);
}

static bool
main_attack_survivor_cycle_run(struct physical_viewer_join *viewer,
    bool ansi, size_t ends[4])
{
	static const uint8_t main_prompt[] =
	    "Time: 14:59  Main Command (?=Help)? ";
	static const uint8_t command[] = "A";
	static const uint8_t title[] = "<Attack>";
	static const uint8_t candidate_name[] = "VICTIM";
	static const uint8_t candidate_answer[] = "Y";
	static const uint8_t commitment[] = "2";
	static const uint8_t eliminated[] =
	    "Fighters eliminated! Attacking the ship!";
	struct viewer_pager_join *join = &viewer->join;
	struct yt_present_result result;
	uint8_t prompt[160];
	uint8_t attacker_row[160];
	uint8_t defender_row[160];
	uint8_t fighter_row[128];
	uint8_t shield_row[128];
	size_t prompt_length;
	size_t attacker_length;
	size_t defender_length;
	size_t fighter_length;
	size_t shield_length;

	if (ends == NULL)
		return false;
	join->presentation = state(ansi);
	join->presentation.foreground = 6.0f;
	join->presentation.color_initialized = 1.0f;
	join->presentation.cached_foreground = 6.0f;
	join->pager.foreground = 6;
	join->pager.line_count = 8.0f;

	join->presentation.foreground = 2.0f;
	join->pager.foreground = 2;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, main_prompt,
	    sizeof(main_prompt) - 1U, 1.0f))
		return false;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	memcpy(join->accumulator, command, sizeof(command));
	if (yt_present_editor_echo(command, sizeof(command) - 1U,
	    command, sizeof(command) - 1U, &join->presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	ends[0] = join->remote_length;

	if (!normal_exit_b05d(join, title, sizeof(title) - 1U, 0.0f)
	    || !yt_direct_attack_candidate_prompt(candidate_name,
	    sizeof(candidate_name) - 1U, prompt, sizeof(prompt),
	    &prompt_length)
	    || yt_present_character(prompt, prompt_length,
	    &join->presentation, &result) != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	memcpy(join->accumulator, candidate_answer, sizeof(candidate_answer));
	if (yt_present_editor_echo(candidate_answer,
	    sizeof(candidate_answer) - 1U, candidate_answer,
	    sizeof(candidate_answer) - 1U, &join->presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U)
	    || !yt_direct_attack_commitment_prompt(3.0, prompt,
	    sizeof(prompt), &prompt_length)
	    || !normal_exit_b05d(join, prompt, prompt_length, 1.0f))
		return false;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	memcpy(join->accumulator, commitment, sizeof(commitment));
	if (yt_present_editor_echo(commitment, sizeof(commitment) - 1U,
	    commitment, sizeof(commitment) - 1U,
	    &join->presentation, &result) != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U)
	    || yt_present_sound(2.0f, &join->presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!yt_direct_attack_result_rows(0.0, 1.0, 1.0, 0.0,
	    attacker_row, sizeof(attacker_row), &attacker_length,
	    defender_row, sizeof(defender_row), &defender_length)
	    || !normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, attacker_row, attacker_length, 0.0f)
	    || !normal_exit_b05d(join, defender_row, defender_length, 0.0f)
	    || !normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, eliminated,
	    sizeof(eliminated) - 1U, 0.0f)
	    || !yt_fighter_shield_spill_rows(0.0, 2.0f,
	    fighter_row, sizeof(fighter_row), &fighter_length,
	    shield_row, sizeof(shield_row), &shield_length)
	    || !normal_exit_line(join, fighter_row, fighter_length)
	    || !normal_exit_line(join, shield_row, shield_length))
		return false;
	ends[1] = join->remote_length;

	/* Positive target shields make the fighter-kill tail output-free. */
	ends[2] = join->remote_length;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, main_prompt,
	    sizeof(main_prompt) - 1U, 1.0f))
		return false;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	ends[3] = join->remote_length;
	return true;
}

static void
test_main_attack_survivor_cycle_presentation(void)
{
	static const uint8_t plain[] =
	    "\r\nTime: 14:59  Main Command (?=Help)? A\r\n"
	    "<Attack>\n\r"
	    "Attack VICTIM (Y/N)[Y]? Y\r\n"
	    "You have 3. Use how many fighters? [0] 2\r\n"
	    "\r\nYou lost 0 fighter(s), 1 remain.\n\r"
	    "You destroyed 1 enemy fighters, 0 remain.\n\r"
	    "\r\nFighters eliminated! Attacking the ship!\n\r"
	    "Fighters remaining: 0\r\n"
	    "Shields reduced to: 2\r\n"
	    "\r\nTime: 14:59  Main Command (?=Help)? ";
	static const uint8_t ansi[] =
	    "\x1b[0;32;40m\r\nTime: 14:59  Main Command (?=Help)? A\r\n"
	    "<Attack>\n\r"
	    "Attack VICTIM (Y/N)[Y]? Y\r\n"
	    "You have 3. Use how many fighters? [0] 2\r\n"
	    "\x1b[MBO1L64P32CEDFEGFAGBAO5BAGFEDC\x0e"
	    "\r\nYou lost 0 fighter(s), 1 remain.\n\r"
	    "You destroyed 1 enemy fighters, 0 remain.\n\r"
	    "\r\nFighters eliminated! Attacking the ship!\n\r"
	    "Fighters remaining: 0\r\n"
	    "Shields reduced to: 2\r\n"
	    "\r\nTime: 14:59  Main Command (?=Help)? ";
	static const struct {
		bool ansi;
		const uint8_t *expected;
		size_t expected_length;
		size_t ends[4];
	} cases[] = {
		{false, plain, sizeof(plain) - 1U,
		    {41U, 289U, 289U, 327U}},
		{true, ansi, sizeof(ansi) - 1U,
		    {51U, 332U, 332U, 370U}},
	};
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	uint8_t remote[400];
	size_t ends[4];
	size_t pass;

	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		memset(&viewer, 0, sizeof(viewer));
		fixture_viewer_initialize(&viewer, &stream,
		    retained_scoreboard, sizeof(retained_scoreboard) - 1U,
		    "YTSCORE.ASC", cases[pass].ansi, remote, sizeof(remote));
		CHECK(main_attack_survivor_cycle_run(&viewer,
		    cases[pass].ansi, ends));
		CHECK(memcmp(ends, cases[pass].ends, sizeof(ends)) == 0);
		CHECK(viewer.join.remote_length == cases[pass].expected_length);
		CHECK(viewer.join.remote_length != cases[pass].expected_length
		    || memcmp(remote, cases[pass].expected,
		    cases[pass].expected_length) == 0);
		CHECK(viewer.join.presentation.foreground == 2.0f
		    && viewer.join.presentation.background == 0.0f
		    && viewer.join.presentation.bold == 0.0f
		    && viewer.join.presentation.blink == 0.0f
		    && viewer.join.presentation.cached_foreground
		    == (cases[pass].ansi ? 2.0f : 6.0f));
		CHECK(viewer.join.pager.foreground == 2
		    && viewer.join.pager.line_count == 0.0f
		    && viewer.join.pager.nonstop == 0.0f);
		CHECK(viewer.join.local_fragment_length == 36U
		    && memcmp(viewer.join.local_fragment,
		    "Time: 14:59  Main Command (?=Help)? ", 36U) == 0
		    && viewer.join.accumulator[0] == '\0'
		    && viewer.join.queue_length == 0U);
		CHECK(stream.eof_checks == 0U && stream.key_checks == 0U
		    && stream.read_count == 0U && stream.line_count == 0U
		    && !stream.file_open && !viewer.join.file_open
		    && viewer.input.file == NULL && viewer.close_calls == 0U
		    && viewer.open_calls == 0U);
		yt_text_input_destroy(&viewer.input);
	}
	CHECK(sizeof(plain) - 1U == 327U && sizeof(ansi) - 1U == 370U);
}

static bool
main_attack_black_hole_cycle_run(bool ansi, struct pager_capture *capture,
    struct yt_present_state *current, struct yt_pager_state *pager,
    size_t ends[3])
{
	static const uint8_t main_prompt[] =
	    "Time: 14:59  Main Command (?=Help)? ";
	static const uint8_t command[] = "A";
	static const uint8_t title[] = "<Attack>";
	static const uint8_t none[] = "There's no one here!";
	static const uint8_t first_sector[] = "Sector: 733";
	static const uint8_t first_warps[] = "Warps lead to: 2, 9";
	static const uint8_t black_hole[] = "A *-BLACK HOLE-* grabs you!";
	static const uint8_t warp_title[] = " * EMERGENCY WARP ENGAGED! * ";
	static const uint8_t wormhole[] =
	    "You enter a wormhole as your engines build up to emergency power!";
	static const uint8_t temperature[] = "     * Engine Temperature *";
	static const uint8_t scale[] = "[ Normal ][ Danger ][ Overheat ]";
	static const uint8_t ruler[] = "================================";
	static const uint8_t relief[] =
	    "You sigh in relief as you look at your scanner and find yourself in";
	static const uint8_t second_sector[] = "Sector: 1003";
	static const uint8_t second_warps[] = "Warps lead to: 1, 42";
	struct yt_present_result result;
	char accumulator[80] = "";
	uint8_t row[128];
	size_t row_length;

	if (capture == NULL || current == NULL || pager == NULL || ends == NULL)
		return false;
	*current = state(ansi);
	current->foreground = 6.0f;
	current->color_initialized = 1.0f;
	current->cached_foreground = 6.0f;
	current->sound.user_sound = 0.0f;
	memset(pager, 0, sizeof(*pager));
	pager->foreground = 6;
	pager->line_count = 8.0f;
	memset(capture, 0, sizeof(*capture));

	current->foreground = 2.0f;
	pager->foreground = 2;
	pager_capture_line(capture, current, NULL, 0U);
	pager->newline_flag = 1.0f;
	pager_fixture_b05d(pager, current, main_prompt,
	    sizeof(main_prompt) - 1U, capture);
	yt_pager_editor_enter(pager, accumulator, sizeof(accumulator));
	if (yt_present_editor_echo(command, sizeof(command) - 1U,
	    command, sizeof(command) - 1U, current, &result) != YT_PRESENT_OK)
		return false;
	pager_capture_result(capture, &result);
	pager_capture_line(capture, current, NULL, 0U);
	pager_fixture_b05d(pager, current, title, sizeof(title) - 1U, capture);
	pager_capture_line(capture, current, NULL, 0U);
	current->bold = 1.0f;
	current->blink = 1.0f;
	pager_fixture_b05d(pager, current, none, sizeof(none) - 1U, capture);

	current->foreground = 1.0f;
	pager->foreground = 1;
	pager_capture_line(capture, current, NULL, 0U);
	pager_capture_line(capture, current, first_sector,
	    sizeof(first_sector) - 1U);
	pager_capture_line(capture, current, first_warps,
	    sizeof(first_warps) - 1U);
	ends[0] = capture->remote_length;

	/* The admitted black-hole child selects foreground two before its blank. */
	current->foreground = 2.0f;
	pager_capture_line(capture, current, NULL, 0U);
	if (yt_present_attention(black_hole, sizeof(black_hole) - 1U,
	    current, &result) != YT_PRESENT_OK)
		return false;
	pager_capture_result(capture, &result);
	pager_capture_line(capture, current, NULL, 0U);
	if (yt_present_attention(warp_title, sizeof(warp_title) - 1U,
	    current, &result) != YT_PRESENT_OK)
		return false;
	pager_capture_result(capture, &result);
	pager_capture_line(capture, current, NULL, 0U);
	if (yt_present_bold_line(wormhole, sizeof(wormhole) - 1U,
	    current, &result) != YT_PRESENT_OK)
		return false;
	pager_capture_result(capture, &result);
	pager_capture_line(capture, current, NULL, 0U);
	current->foreground = 6.0f;
	if (yt_present_bold_line(temperature, sizeof(temperature) - 1U,
	    current, &result) != YT_PRESENT_OK)
		return false;
	pager_capture_result(capture, &result);
	if (yt_present_bold_line(scale, sizeof(scale) - 1U,
	    current, &result) != YT_PRESENT_OK)
		return false;
	pager_capture_result(capture, &result);
	current->foreground = 2.0f;
	if (yt_present_bold_line(ruler, sizeof(ruler) - 1U,
	    current, &result) != YT_PRESENT_OK)
		return false;
	pager_capture_result(capture, &result);
	current->foreground = 6.0f;
	if (yt_present_bold_character((const uint8_t *)"[", 1U,
	    current, &result) != YT_PRESENT_OK)
		return false;
	pager_capture_result(capture, &result);
	current->foreground = 2.0f;
	if (yt_present_bold_character((const uint8_t *)"*", 1U,
	    current, &result) != YT_PRESENT_OK)
		return false;
	pager_capture_result(capture, &result);
	pager_capture_line(capture, current, NULL, 0U);
	pager_capture_line(capture, current, NULL, 0U);
	if (yt_present_sound(1.0f, current, &result) != YT_PRESENT_OK)
		return false;
	pager_capture_result(capture, &result);
	pager_capture_line(capture, current, relief, sizeof(relief) - 1U);
	if (!yt_emergency_warp_result_row(1003.0f, 3.0f, row,
	    sizeof(row), &row_length))
		return false;
	pager_capture_line(capture, current, row, row_length);
	ends[1] = capture->remote_length;

	current->foreground = 1.0f;
	pager->foreground = 1;
	pager_capture_line(capture, current, NULL, 0U);
	pager_capture_line(capture, current, second_sector,
	    sizeof(second_sector) - 1U);
	pager_capture_line(capture, current, second_warps,
	    sizeof(second_warps) - 1U);
	pager->line_count = 0.0f;
	current->foreground = 2.0f;
	pager->foreground = 2;
	pager_capture_line(capture, current, NULL, 0U);
	pager->newline_flag = 1.0f;
	pager_fixture_b05d(pager, current, main_prompt,
	    sizeof(main_prompt) - 1U, capture);
	yt_pager_editor_enter(pager, accumulator, sizeof(accumulator));
	ends[2] = capture->remote_length;
	return true;
}

static void
test_main_attack_black_hole_cycle_presentation(void)
{
	static const uint8_t plain[] =
	    "\r\nTime: 14:59  Main Command (?=Help)? A\r\n"
	    "<Attack>\n\r\r\nThere's no one here!\n\r"
	    "\r\nSector: 733\r\nWarps lead to: 2, 9\r\n"
	    "\r\nA *-BLACK HOLE-* grabs you!\r\n"
	    "\r\n * EMERGENCY WARP ENGAGED! * \r\n"
	    "\r\nYou enter a wormhole as your engines build up to emergency power!\r\n"
	    "\r\n     * Engine Temperature *\r\n"
	    "[ Normal ][ Danger ][ Overheat ]\r\n"
	    "================================\r\n[*\r\n\r\n"
	    "You sigh in relief as you look at your scanner and find yourself in\r\n"
	    "sector 1003. However, it takes you 3 turns to recharge your engines!\r\n"
	    "\r\nSector: 1003\r\nWarps lead to: 1, 42\r\n"
	    "\r\nTime: 14:59  Main Command (?=Help)? ";
	static const uint8_t ansi[] =
	    "\x1b[0;32;40m\r\nTime: 14:59  Main Command (?=Help)? A\r\n"
	    "<Attack>\n\r\r\n\x1b[0;32;40;5;1mThere's no one here!\n\r"
	    "\x1b[0;31;40m\r\nSector: 733\r\nWarps lead to: 2, 9\r\n"
	    "\x1b[0;32;40m\r\n"
	    "\x1b[0;33;41;5;1mA *-BLACK HOLE-* grabs you!"
	    "\x1b[0;33;40m\r\n\r\n"
	    "\x1b[0;33;41;5;1m * EMERGENCY WARP ENGAGED! * "
	    "\x1b[0;33;40m\r\n\r\n"
	    "\x1b[0;33;40;1mYou enter a wormhole as your engines build up to emergency power!\r\n"
	    "\x1b[0;33;40m\r\n"
	    "\x1b[0;36;40;1m     * Engine Temperature *\r\n"
	    "\x1b[0;36;40;1m[ Normal ][ Danger ][ Overheat ]\r\n"
	    "\x1b[0;32;40;1m================================\r\n"
	    "\x1b[0;36;40;1m[\x1b[0;32;40;1m*"
	    "\x1b[0;32;40m\r\n\r\n"
	    "You sigh in relief as you look at your scanner and find yourself in\r\n"
	    "sector 1003. However, it takes you 3 turns to recharge your engines!\r\n"
	    "\x1b[0;31;40m\r\nSector: 1003\r\nWarps lead to: 1, 42\r\n"
	    "\x1b[0;32;40m\r\nTime: 14:59  Main Command (?=Help)? ";
	static const struct {
		bool ansi;
		const uint8_t *expected;
		size_t expected_length;
		size_t ends[3];
	} cases[] = {
		{false, plain, sizeof(plain) - 1U, {111U, 488U, 564U}},
		{true, ansi, sizeof(ansi) - 1U, {145U, 672U, 768U}},
	};
	struct yt_present_state current;
	struct yt_pager_state pager;
	struct pager_capture capture;
	size_t ends[3];
	size_t pass;

	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		CHECK(main_attack_black_hole_cycle_run(cases[pass].ansi,
		    &capture, &current, &pager, ends));
		CHECK(memcmp(ends, cases[pass].ends, sizeof(ends)) == 0);
		CHECK(capture.remote_length == cases[pass].expected_length);
		CHECK(capture.remote_length != cases[pass].expected_length
		    || memcmp(capture.remote, cases[pass].expected,
		    cases[pass].expected_length) == 0);
		CHECK(current.foreground == 2.0f
		    && current.background == 0.0f
		    && current.bold == (cases[pass].ansi ? 0.0f : 1.0f)
		    && current.blink == (cases[pass].ansi ? 0.0f : 1.0f)
		    && current.cached_foreground
		    == (cases[pass].ansi ? 2.0f : 6.0f)
		    && pager.foreground == 2 && pager.line_count == 0.0f
		    && pager.nonstop == 0.0f);
	}
	CHECK(sizeof(plain) - 1U == 564U && sizeof(ansi) - 1U == 768U);
}

static bool
mine_emergency_warp_present(struct pager_capture *capture,
    struct yt_present_state *current)
{
	static const uint8_t title[] = " * EMERGENCY WARP ENGAGED! * ";
	static const uint8_t wormhole[] =
	    "You enter a wormhole as your engines build up to emergency power!";
	static const uint8_t temperature[] = "     * Engine Temperature *";
	static const uint8_t scale[] = "[ Normal ][ Danger ][ Overheat ]";
	static const uint8_t ruler[] = "================================";
	static const uint8_t relief[] =
	    "You sigh in relief as you look at your scanner and find yourself in";
	struct yt_present_result result;
	uint8_t row[128];
	size_t row_length;

	pager_capture_line(capture, current, NULL, 0U);
	if (yt_present_attention(title, sizeof(title) - 1U, current, &result)
	    != YT_PRESENT_OK)
		return false;
	pager_capture_result(capture, &result);
	pager_capture_line(capture, current, NULL, 0U);
	if (yt_present_bold_line(wormhole, sizeof(wormhole) - 1U,
	    current, &result) != YT_PRESENT_OK)
		return false;
	pager_capture_result(capture, &result);
	pager_capture_line(capture, current, NULL, 0U);
	current->foreground = 6.0f;
	if (yt_present_bold_line(temperature, sizeof(temperature) - 1U,
	    current, &result) != YT_PRESENT_OK)
		return false;
	pager_capture_result(capture, &result);
	if (yt_present_bold_line(scale, sizeof(scale) - 1U,
	    current, &result) != YT_PRESENT_OK)
		return false;
	pager_capture_result(capture, &result);
	current->foreground = 2.0f;
	if (yt_present_bold_line(ruler, sizeof(ruler) - 1U,
	    current, &result) != YT_PRESENT_OK)
		return false;
	pager_capture_result(capture, &result);
	current->foreground = 6.0f;
	if (yt_present_bold_character((const uint8_t *)"[", 1U,
	    current, &result) != YT_PRESENT_OK)
		return false;
	pager_capture_result(capture, &result);
	current->foreground = 2.0f;
	if (yt_present_bold_character((const uint8_t *)"*", 1U,
	    current, &result) != YT_PRESENT_OK)
		return false;
	pager_capture_result(capture, &result);
	pager_capture_line(capture, current, NULL, 0U);
	pager_capture_line(capture, current, NULL, 0U);
	if (yt_present_sound(1.0f, current, &result) != YT_PRESENT_OK)
		return false;
	pager_capture_result(capture, &result);
	pager_capture_line(capture, current, relief, sizeof(relief) - 1U);
	if (!yt_emergency_warp_result_row(1003.0f, 3.0f, row,
	    sizeof(row), &row_length))
		return false;
	pager_capture_line(capture, current, row, row_length);
	return true;
}

static bool
main_attack_mine_cycle_run(bool ansi, bool emergency_warp,
    struct pager_capture *capture,
    struct yt_present_state *current, struct yt_pager_state *pager,
    size_t ends[4])
{
	static const uint8_t main_prompt[] =
	    "Time: 14:59  Main Command (?=Help)? ";
	static const uint8_t command[] = "A";
	static const uint8_t title[] = "<Attack>";
	static const uint8_t none[] = "There's no one here!";
	static const uint8_t sector[] = "Sector: 42";
	static const uint8_t mine_warning[] =
	    "** WARNING! SECTOR HAS 1 MINES! **";
	static const uint8_t warps[] = "Warps lead to: 2, 9";
	static const uint8_t warp_sector[] = "Sector: 1003";
	static const uint8_t warp_warps[] = "Warps lead to: 1, 42";
	static const uint8_t mined[] = "** Sector is Mined!! **";
	struct yt_present_result result;
	char accumulator[80] = "";
	uint8_t row[128];
	size_t row_length;

	if (capture == NULL || current == NULL || pager == NULL || ends == NULL)
		return false;
	*current = state(ansi);
	current->foreground = 6.0f;
	current->color_initialized = 1.0f;
	current->cached_foreground = 6.0f;
	current->sound.user_sound = 0.0f;
	memset(pager, 0, sizeof(*pager));
	pager->foreground = 6;
	pager->line_count = 8.0f;
	memset(capture, 0, sizeof(*capture));

	current->foreground = 2.0f;
	pager->foreground = 2;
	pager_capture_line(capture, current, NULL, 0U);
	pager->newline_flag = 1.0f;
	pager_fixture_b05d(pager, current, main_prompt,
	    sizeof(main_prompt) - 1U, capture);
	yt_pager_editor_enter(pager, accumulator, sizeof(accumulator));
	if (yt_present_editor_echo(command, sizeof(command) - 1U,
	    command, sizeof(command) - 1U, current, &result) != YT_PRESENT_OK)
		return false;
	pager_capture_result(capture, &result);
	pager_capture_line(capture, current, NULL, 0U);
	pager_fixture_b05d(pager, current, title, sizeof(title) - 1U, capture);
	pager_capture_line(capture, current, NULL, 0U);
	current->bold = 1.0f;
	current->blink = 1.0f;
	pager_fixture_b05d(pager, current, none, sizeof(none) - 1U, capture);

	current->foreground = 1.0f;
	pager->foreground = 1;
	pager_capture_line(capture, current, NULL, 0U);
	pager_capture_line(capture, current, sector, sizeof(sector) - 1U);
	if (yt_present_attention(mine_warning, sizeof(mine_warning) - 1U,
	    current, &result) != YT_PRESENT_OK)
		return false;
	pager_capture_result(capture, &result);
	pager_capture_line(capture, current, warps, sizeof(warps) - 1U);
	ends[0] = capture->remote_length;

	pager_capture_line(capture, current, NULL, 0U);
	current->blink = 1.0f;
	pager_capture_line(capture, current, mined, sizeof(mined) - 1U);
	if (yt_present_sound(5.0f, current, &result) != YT_PRESENT_OK)
		return false;
	pager_capture_result(capture, &result);
	current->foreground = 3.0f;
	current->background = 0.0f;
	current->blink = 0.0f;
	if (!yt_sector_mine_explosion_row(1.0f, 1.0f, row,
	    sizeof(row), &row_length)
	    || yt_present_bold_character(row, row_length, current, &result)
	    != YT_PRESENT_OK)
		return false;
	pager_capture_result(capture, &result);
	current->background = 1.0f;
	pager_capture_line(capture, current, NULL, 0U);
	if (!yt_sector_mine_shields_row(10.0f, row, sizeof(row), &row_length)
	    || yt_present_bold_line(row, row_length, current, &result)
	    != YT_PRESENT_OK)
		return false;
	pager_capture_result(capture, &result);
	if (yt_present_sound(2.0f, current, &result) != YT_PRESENT_OK)
		return false;
	pager_capture_result(capture, &result);
	ends[1] = capture->remote_length;

	if (emergency_warp) {
		if (!mine_emergency_warp_present(capture, current))
			return false;
	}
	ends[2] = capture->remote_length;

	if (emergency_warp) {
		current->foreground = 1.0f;
		pager->foreground = 1;
	}
	else {
		if (ansi)
			current->background = 0.0f;
		pager->foreground = 3;
	}
	pager_capture_line(capture, current, NULL, 0U);
	pager_capture_line(capture, current,
	    emergency_warp ? warp_sector : sector,
	    emergency_warp ? sizeof(warp_sector) - 1U : sizeof(sector) - 1U);
	pager_capture_line(capture, current,
	    emergency_warp ? warp_warps : warps,
	    emergency_warp ? sizeof(warp_warps) - 1U : sizeof(warps) - 1U);
	pager->line_count = 0.0f;
	current->foreground = 2.0f;
	pager->foreground = 2;
	pager_capture_line(capture, current, NULL, 0U);
	pager->newline_flag = 1.0f;
	pager_fixture_b05d(pager, current, main_prompt,
	    sizeof(main_prompt) - 1U, capture);
	yt_pager_editor_enter(pager, accumulator, sizeof(accumulator));
	ends[3] = capture->remote_length;
	return true;
}

static void
test_main_attack_mine_cycle_presentation(void)
{
	static const uint8_t plain[] =
	    "\r\nTime: 14:59  Main Command (?=Help)? A\r\n"
	    "<Attack>\n\r\r\nThere's no one here!\n\r"
	    "\r\nSector: 42\r\n"
	    "** WARNING! SECTOR HAS 1 MINES! **\r\n"
	    "Warps lead to: 2, 9\r\n"
	    "\r\n** Sector is Mined!! **\r\n"
	    "There are 1 mines here! 1 EXPLODE!\r\n"
	    "Shields down to 10 units!\r\n"
	    "\r\nSector: 42\r\nWarps lead to: 2, 9\r\n"
	    "\r\nTime: 14:59  Main Command (?=Help)? ";
	static const uint8_t ansi[] =
	    "\x1b[0;32;40m\r\nTime: 14:59  Main Command (?=Help)? A\r\n"
	    "<Attack>\n\r\r\n\x1b[0;32;40;5;1mThere's no one here!\n\r"
	    "\x1b[0;31;40m\r\nSector: 42\r\n"
	    "\x1b[0;33;41;5;1m** WARNING! SECTOR HAS 1 MINES! **"
	    "\x1b[0;33;40m\r\nWarps lead to: 2, 9\r\n"
	    "\r\n\x1b[0;33;40;5m** Sector is Mined!! **\r\n"
	    "\x1b[0;33;40;1mThere are 1 mines here! 1 EXPLODE!"
	    "\x1b[0;33;41m\r\n"
	    "\x1b[0;33;41;1mShields down to 10 units!\r\n"
	    "\x1b[0;33;40m\r\nSector: 42\r\nWarps lead to: 2, 9\r\n"
	    "\x1b[0;32;40m\r\nTime: 14:59  Main Command (?=Help)? ";
	static const struct {
		bool ansi;
		const uint8_t *expected;
		size_t expected_length;
		size_t ends[4];
	} cases[] = {
		{false, plain, sizeof(plain) - 1U,
		    {146U, 236U, 236U, 309U}},
		{true, ansi, sizeof(ansi) - 1U,
		    {204U, 340U, 340U, 433U}},
	};
	struct yt_present_state current;
	struct yt_pager_state pager;
	struct pager_capture capture;
	size_t ends[4];
	size_t pass;

	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		CHECK(main_attack_mine_cycle_run(cases[pass].ansi, false,
		    &capture, &current, &pager, ends));
		CHECK(memcmp(ends, cases[pass].ends, sizeof(ends)) == 0);
		CHECK(capture.remote_length == cases[pass].expected_length);
		CHECK(capture.remote_length != cases[pass].expected_length
		    || memcmp(capture.remote, cases[pass].expected,
		    cases[pass].expected_length) == 0);
		CHECK(current.foreground == 2.0f
		    && current.background == (cases[pass].ansi ? 0.0f : 1.0f)
		    && current.bold == (cases[pass].ansi ? 0.0f : 1.0f)
		    && current.blink == 0.0f
		    && current.cached_foreground
		    == (cases[pass].ansi ? 2.0f : 6.0f)
		    && pager.foreground == 2 && pager.line_count == 0.0f
		    && pager.nonstop == 0.0f);
	}
	CHECK(sizeof(plain) - 1U == 309U && sizeof(ansi) - 1U == 433U);
}

static void
test_main_attack_mine_warp_cycle_presentation(void)
{
	static const uint8_t plain[] =
	    "\r\nTime: 14:59  Main Command (?=Help)? A\r\n"
	    "<Attack>\n\r\r\nThere's no one here!\n\r"
	    "\r\nSector: 42\r\n"
	    "** WARNING! SECTOR HAS 1 MINES! **\r\n"
	    "Warps lead to: 2, 9\r\n"
	    "\r\n** Sector is Mined!! **\r\n"
	    "There are 1 mines here! 1 EXPLODE!\r\n"
	    "Shields down to 10 units!\r\n"
	    "\r\n * EMERGENCY WARP ENGAGED! * \r\n"
	    "\r\nYou enter a wormhole as your engines build up to emergency power!\r\n"
	    "\r\n     * Engine Temperature *\r\n"
	    "[ Normal ][ Danger ][ Overheat ]\r\n"
	    "================================\r\n[*\r\n\r\n"
	    "You sigh in relief as you look at your scanner and find yourself in\r\n"
	    "sector 1003. However, it takes you 3 turns to recharge your engines!\r\n"
	    "\r\nSector: 1003\r\nWarps lead to: 1, 42\r\n"
	    "\r\nTime: 14:59  Main Command (?=Help)? ";
	static const uint8_t ansi[] =
	    "\x1b[0;32;40m\r\nTime: 14:59  Main Command (?=Help)? A\r\n"
	    "<Attack>\n\r\r\n\x1b[0;32;40;5;1mThere's no one here!\n\r"
	    "\x1b[0;31;40m\r\nSector: 42\r\n"
	    "\x1b[0;33;41;5;1m** WARNING! SECTOR HAS 1 MINES! **"
	    "\x1b[0;33;40m\r\nWarps lead to: 2, 9\r\n"
	    "\r\n\x1b[0;33;40;5m** Sector is Mined!! **\r\n"
	    "\x1b[0;33;40;1mThere are 1 mines here! 1 EXPLODE!"
	    "\x1b[0;33;41m\r\n"
	    "\x1b[0;33;41;1mShields down to 10 units!\r\n"
	    "\x1b[0;33;41m\r\n"
	    "\x1b[0;33;41;5;1m * EMERGENCY WARP ENGAGED! * "
	    "\x1b[0;33;40m\r\n\r\n"
	    "\x1b[0;33;40;1mYou enter a wormhole as your engines build up to emergency power!\r\n"
	    "\x1b[0;33;40m\r\n"
	    "\x1b[0;36;40;1m     * Engine Temperature *\r\n"
	    "\x1b[0;36;40;1m[ Normal ][ Danger ][ Overheat ]\r\n"
	    "\x1b[0;32;40;1m================================\r\n"
	    "\x1b[0;36;40;1m[\x1b[0;32;40;1m*"
	    "\x1b[0;32;40m\r\n\r\n"
	    "You sigh in relief as you look at your scanner and find yourself in\r\n"
	    "sector 1003. However, it takes you 3 turns to recharge your engines!\r\n"
	    "\x1b[0;31;40m\r\nSector: 1003\r\nWarps lead to: 1, 42\r\n"
	    "\x1b[0;32;40m\r\nTime: 14:59  Main Command (?=Help)? ";
	static const struct {
		bool ansi;
		const uint8_t *expected;
		size_t expected_length;
		size_t ends[4];
	} cases[] = {
		{false, plain, sizeof(plain) - 1U,
		    {146U, 236U, 582U, 658U}},
		{true, ansi, sizeof(ansi) - 1U,
		    {204U, 340U, 812U, 908U}},
	};
	struct yt_present_state current;
	struct yt_pager_state pager;
	struct pager_capture capture;
	size_t ends[4];
	size_t pass;

	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		CHECK(main_attack_mine_cycle_run(cases[pass].ansi, true,
		    &capture, &current, &pager, ends));
		CHECK(memcmp(ends, cases[pass].ends, sizeof(ends)) == 0);
		CHECK(capture.remote_length == cases[pass].expected_length);
		CHECK(capture.remote_length != cases[pass].expected_length
		    || memcmp(capture.remote, cases[pass].expected,
		    cases[pass].expected_length) == 0);
		CHECK(current.foreground == 2.0f
		    && current.background == 0.0f
		    && current.bold == (cases[pass].ansi ? 0.0f : 1.0f)
		    && current.blink == (cases[pass].ansi ? 0.0f : 1.0f)
		    && current.cached_foreground
		    == (cases[pass].ansi ? 2.0f : 6.0f)
		    && pager.foreground == 2 && pager.line_count == 0.0f
		    && pager.nonstop == 0.0f);
	}
	CHECK(sizeof(plain) - 1U == 658U && sizeof(ansi) - 1U == 908U);
}

static bool
planet_movement_accepted_cycle_run(struct physical_viewer_join *viewer,
    bool ansi, size_t ends[4])
{
	static const uint8_t free_holds[] =
	    "You have 65 free cargo holds.";
	static const uint8_t planet_prompt[] =
	    "Time: 14:59  Planet command (?=help) [A]? ";
	static const uint8_t command[] = "M";
	static const uint8_t destination_prompt[] = "Move to which sector? ";
	static const uint8_t destination[] = "42";
	static const uint8_t finalizer_row[] =
	    "One Turn Deducted, 59 left.";
	static const uint8_t sector[] = "Sector: 42";
	static const uint8_t warps[] = "Warps lead to:";
	static const uint8_t warp_one[] = " 12";
	static const uint8_t warp_two[] = ", 99";
	static const uint8_t main_prompt[] =
	    "Time: 14:59  Main Command (?=Help)? ";
	static const float warp_values[6] = {
		7.0f, 42.0f, 0.0f, 0.0f, 12.5f, 0.0f
	};
	struct viewer_pager_join *join = &viewer->join;
	struct yt_present_result result;
	uint8_t row[128];
	size_t row_length;

	if (ends == NULL)
		return false;
	join->presentation = state(ansi);
	join->presentation.foreground = 6.0f;
	join->presentation.cached_foreground = ansi ? 6.0f : 0.0f;
	join->pager.foreground = 6;
	join->pager.line_count = 0.0f;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, free_holds,
	    sizeof(free_holds) - 1U, 0.0f)
	    || !normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, planet_prompt,
	    sizeof(planet_prompt) - 1U, 1.0f))
		return false;
	ends[0] = join->remote_length;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	memcpy(join->accumulator, command, sizeof(command));
	if (yt_present_editor_echo(command, sizeof(command) - 1U,
	    command, sizeof(command) - 1U, &join->presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	ends[1] = join->remote_length;

	if (!yt_movement_warp_row(warp_values, row, sizeof(row), &row_length)
	    || !normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, row, row_length, 0.0f)
	    || !normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, destination_prompt,
	    sizeof(destination_prompt) - 1U, 1.0f))
		return false;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	memcpy(join->accumulator, destination, sizeof(destination));
	if (yt_present_editor_echo(destination, sizeof(destination) - 1U,
	    destination, sizeof(destination) - 1U,
	    &join->presentation, &result) != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, finalizer_row,
	    sizeof(finalizer_row) - 1U, 0.0f))
		return false;
	ends[2] = join->remote_length;

	join->presentation.foreground = 1.0f;
	join->pager.foreground = 1;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_line(join, sector, sizeof(sector) - 1U)
	    || !sensor_join_present(join, &(const struct sensor_join_output){
	    SENSOR_JOIN_RAW, (const char *)warps})
	    || !sensor_join_present(join, &(const struct sensor_join_output){
	    SENSOR_JOIN_RAW, (const char *)warp_one})
	    || !sensor_join_present(join, &(const struct sensor_join_output){
	    SENSOR_JOIN_RAW, (const char *)warp_two})
	    || !normal_exit_line(join, NULL, 0U))
		return false;
	join->pager.line_count = 0.0f;
	join->presentation.foreground = 2.0f;
	join->pager.foreground = 2;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, main_prompt,
	    sizeof(main_prompt) - 1U, 1.0f))
		return false;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	ends[3] = join->remote_length;
	return true;
}

static void
test_planet_movement_accepted_cycle_presentation(void)
{
	static const uint8_t plain[] =
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? M\r\n"
	    "\r\nWarps lead to, 7, 42, 12.5\n\r"
	    "\r\nMove to which sector? 42\r\n"
	    "\r\nOne Turn Deducted, 59 left.\n\r"
	    "\r\nSector: 42\r\nWarps lead to: 12, 99\r\n"
	    "\r\nTime: 14:59  Main Command (?=Help)? ";
	static const uint8_t ansi[] =
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? M\r\n"
	    "\r\nWarps lead to, 7, 42, 12.5\n\r"
	    "\r\nMove to which sector? 42\r\n"
	    "\r\nOne Turn Deducted, 59 left.\n\r"
	    "\x1b[0;31;40m\r\nSector: 42\r\nWarps lead to: 12, 99\r\n"
	    "\x1b[0;32;40m\r\nTime: 14:59  Main Command (?=Help)? ";
	static const struct {
		bool ansi;
		const uint8_t *expected;
		size_t expected_length;
		size_t ends[4];
	} cases[] = {
		{false, plain, sizeof(plain) - 1U, {77U, 80U, 169U, 244U}},
		{true, ansi, sizeof(ansi) - 1U, {77U, 80U, 169U, 264U}},
	};
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	uint8_t remote[300];
	size_t ends[4];
	size_t pass;

	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		memset(&viewer, 0, sizeof(viewer));
		fixture_viewer_initialize(&viewer, &stream,
		    retained_scoreboard, sizeof(retained_scoreboard) - 1U,
		    "YTSCORE.ASC", cases[pass].ansi, remote, sizeof(remote));
		CHECK(planet_movement_accepted_cycle_run(&viewer,
		    cases[pass].ansi, ends));
		CHECK(memcmp(ends, cases[pass].ends, sizeof(ends)) == 0
		    && viewer.join.remote_length == cases[pass].expected_length
		    && memcmp(remote, cases[pass].expected,
		    cases[pass].expected_length) == 0
		    && viewer.join.presentation.foreground == 2.0f
		    && viewer.join.presentation.background == 0.0f
		    && viewer.join.presentation.bold == 0.0f
		    && viewer.join.presentation.blink == 0.0f
		    && viewer.join.presentation.cached_foreground
		    == (cases[pass].ansi ? 2.0f : 0.0f)
		    && viewer.join.pager.foreground == 2
		    && viewer.join.pager.line_count == 0.0f
		    && viewer.join.pager.nonstop == 0.0f
		    && viewer.join.local_row_count == 14U
		    && viewer_rows_fnv1a64(&viewer.join)
		    == UINT64_C(0x738bf22fcb00cbf5)
		    && viewer.join.local_color_count
		    == (cases[pass].ansi ? 26U : 6U)
		    && viewer_colors_fnv1a64(&viewer.join)
		    == (cases[pass].ansi
		    ? UINT64_C(0x4adccaf52f3be1cd)
		    : UINT64_C(0x17798e683d05096d))
		    && viewer.join.local_fragment_length == 36U
		    && memcmp(viewer.join.local_fragment,
		    "Time: 14:59  Main Command (?=Help)? ", 36U) == 0
		    && viewer.join.accumulator[0] == '\0'
		    && viewer.join.queue_length == 0U
		    && viewer.join.sample_calls == 6U
		    && viewer.join.event_count == 30U
		    && stream.eof_checks == 0U && stream.key_checks == 0U
		    && stream.read_count == 0U && stream.line_count == 0U
		    && !stream.file_open && !viewer.join.file_open
		    && viewer.input.file == NULL && viewer.close_calls == 0U
		    && viewer.open_calls == 0U);
		yt_text_input_destroy(&viewer.input);
	}
	CHECK(sizeof(plain) - 1U == 244U && sizeof(ansi) - 1U == 264U);
}

static bool
planet_port_no_port_cycle_run(struct physical_viewer_join *viewer,
    bool ansi, size_t ends[4])
{
	static const uint8_t free_holds[] =
	    "You have 65 free cargo holds.";
	static const uint8_t planet_prompt[] =
	    "Time: 14:59  Planet command (?=help) [A]? ";
	static const uint8_t command[] = "P";
	static const uint8_t heading[] = "<Port>";
	static const uint8_t no_port[] = "No port here!";
	static const uint8_t sector[] = "Sector: 733";
	static const uint8_t warps[] = "Warps lead to:";
	static const uint8_t warp_one[] = " 2";
	static const uint8_t warp_two[] = ", 9";
	static const uint8_t main_prompt[] =
	    "Time: 14:59  Main Command (?=Help)? ";
	struct viewer_pager_join *join = &viewer->join;
	struct yt_present_result result;

	if (ends == NULL)
		return false;
	join->presentation = state(ansi);
	join->presentation.foreground = 6.0f;
	join->presentation.cached_foreground = ansi ? 6.0f : 0.0f;
	join->pager.foreground = 6;
	join->pager.line_count = 0.0f;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, free_holds,
	    sizeof(free_holds) - 1U, 0.0f)
	    || !normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, planet_prompt,
	    sizeof(planet_prompt) - 1U, 1.0f))
		return false;
	ends[0] = join->remote_length;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	memcpy(join->accumulator, command, sizeof(command));
	if (yt_present_editor_echo(command, sizeof(command) - 1U,
	    command, sizeof(command) - 1U, &join->presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	ends[1] = join->remote_length;

	if (!normal_exit_b05d(join, heading, sizeof(heading) - 1U, 0.0f))
		return false;
	join->presentation.foreground = 3.0f;
	join->pager.foreground = 3;
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	join->presentation.bold = 1.0f;
	join->presentation.blink = 1.0f;
	join->queue[0] = '\0';
	join->queue_position = 0U;
	join->queue_length = 0U;
	if (!normal_exit_b05d(join, no_port, sizeof(no_port) - 1U, 0.0f))
		return false;
	ends[2] = join->remote_length;

	join->presentation.foreground = 1.0f;
	join->pager.foreground = 1;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_line(join, sector, sizeof(sector) - 1U)
	    || !sensor_join_present(join, &(const struct sensor_join_output){
	    SENSOR_JOIN_RAW, (const char *)warps})
	    || !sensor_join_present(join, &(const struct sensor_join_output){
	    SENSOR_JOIN_RAW, (const char *)warp_one})
	    || !sensor_join_present(join, &(const struct sensor_join_output){
	    SENSOR_JOIN_RAW, (const char *)warp_two})
	    || !normal_exit_line(join, NULL, 0U))
		return false;
	join->pager.line_count = 0.0f;
	join->presentation.foreground = 2.0f;
	join->pager.foreground = 2;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, main_prompt,
	    sizeof(main_prompt) - 1U, 1.0f))
		return false;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	ends[3] = join->remote_length;
	return true;
}

static void
test_planet_port_no_port_cycle_presentation(void)
{
	static const uint8_t plain[] =
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? P\r\n"
	    "<Port>\n\r\r\nNo port here!\n\r"
	    "\r\nSector: 733\r\nWarps lead to: 2, 9\r\n"
	    "\r\nTime: 14:59  Main Command (?=Help)? ";
	static const uint8_t ansi[] =
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? P\r\n"
	    "<Port>\n\r"
	    "\x1b[0;33;40m\r\n"
	    "\x1b[0;33;40;5;1mNo port here!\n\r"
	    "\x1b[0;31;40m\r\nSector: 733\r\nWarps lead to: 2, 9\r\n"
	    "\x1b[0;32;40m\r\nTime: 14:59  Main Command (?=Help)? ";
	static const struct {
		bool ansi;
		const uint8_t *expected;
		size_t expected_length;
		size_t ends[4];
	} cases[] = {
		{false, plain, sizeof(plain) - 1U, {77U, 80U, 105U, 179U}},
		{true, ansi, sizeof(ansi) - 1U, {77U, 80U, 129U, 223U}},
	};
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	uint8_t remote[260];
	size_t ends[4];
	size_t pass;

	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		memset(&viewer, 0, sizeof(viewer));
		fixture_viewer_initialize(&viewer, &stream,
		    retained_scoreboard, sizeof(retained_scoreboard) - 1U,
		    "YTSCORE.ASC", cases[pass].ansi, remote, sizeof(remote));
		CHECK(planet_port_no_port_cycle_run(&viewer,
		    cases[pass].ansi, ends));
		CHECK(memcmp(ends, cases[pass].ends, sizeof(ends)) == 0
		    && viewer.join.remote_length == cases[pass].expected_length
		    && memcmp(remote, cases[pass].expected,
		    cases[pass].expected_length) == 0
		    && viewer.join.presentation.foreground == 2.0f
		    && viewer.join.presentation.background == 0.0f
		    && viewer.join.presentation.bold
		    == (cases[pass].ansi ? 0.0f : 1.0f)
		    && viewer.join.presentation.blink
		    == (cases[pass].ansi ? 0.0f : 1.0f)
		    && viewer.join.presentation.cached_foreground
		    == (cases[pass].ansi ? 2.0f : 0.0f)
		    && viewer.join.pager.foreground == 2
		    && viewer.join.pager.line_count == 0.0f
		    && viewer.join.pager.nonstop == 0.0f
		    && viewer.join.local_row_count == 11U
		    && viewer_rows_fnv1a64(&viewer.join)
		    == UINT64_C(0x5a0b950ca34d8443)
		    && viewer.join.local_color_count
		    == (cases[pass].ansi ? 21U : 5U)
		    && viewer_colors_fnv1a64(&viewer.join)
		    == (cases[pass].ansi
		    ? UINT64_C(0x2f1f7b742d1adda2)
		    : UINT64_C(0xc6f69f5cf097a0a2))
		    && viewer.join.local_fragment_length == 36U
		    && memcmp(viewer.join.local_fragment,
		    "Time: 14:59  Main Command (?=Help)? ", 36U) == 0
		    && viewer.join.accumulator[0] == '\0'
		    && viewer.join.queue_length == 0U
		    && viewer.join.sample_calls == 5U
		    && viewer.join.event_count == 25U
		    && stream.eof_checks == 0U && stream.key_checks == 0U
		    && stream.read_count == 0U && stream.line_count == 0U
		    && !stream.file_open && !viewer.join.file_open
		    && viewer.input.file == NULL && viewer.close_calls == 0U
		    && viewer.open_calls == 0U);
		yt_text_input_destroy(&viewer.input);
	}
	CHECK(sizeof(plain) - 1U == 179U && sizeof(ansi) - 1U == 223U);
}

static bool
planet_port_refusal_cycle_run(struct physical_viewer_join *viewer,
    bool ansi)
{
	static const uint8_t free_holds[] =
	    "You have 65 free cargo holds.";
	static const uint8_t planet_prompt[] =
	    "Time: 14:59  Planet command (?=help) [A]? ";
	static const uint8_t command[] = "P";
	static const uint8_t port_label[] = "<Port>";
	static const uint8_t docking[] = "Docking, ";
	static const uint8_t turn[] = "One Turn Deducted, 59 left.";
	static const uint8_t owner[] =
	    "This port is owned by: YOU, Credits: 1234.5";
	static const uint8_t title[] =
	    "Commerce report for Argus: 01-01-1991 00:00:00";
	static const uint8_t header[] =
	    " Items         Status      # units    in holds   Cost";
	static const uint8_t rule[] =
	    "=======       =========   =========   ========   ====";
	static const uint8_t item_prefix[3][24] = {
		"Ore..........  Selling",
		"Organics.....  Selling",
		"Equipment....  Selling",
	};
	static const uint8_t capacity[3][13] = {
		"         100", "         200", "         300",
	};
	static const uint8_t hold[3][12] = {
		"         10", "         20", "          5",
	};
	static const uint8_t price[3][8] = {
		" 20    ", " 30    ", " 40    ",
	};
	static const uint8_t refusal[] =
	    "We don't want your goods and you can't buy ours Pat!";
	static const uint8_t status[] =
	    "You have 12345 credits and 65 empty cargo holds.";
	static const uint8_t sector[] = "Sector: 733";
	static const uint8_t warps[] = "Warps lead to:";
	static const uint8_t warp_one[] = " 2";
	static const uint8_t warp_two[] = ", 9";
	static const uint8_t main_prompt[] =
	    "Time:10:00  Main Command (?=Help)? ";
	struct viewer_pager_join *join = &viewer->join;
	struct yt_present_result result;
	size_t index;

	join->presentation = state(ansi);
	join->presentation.foreground = 6.0f;
	join->presentation.cached_foreground = ansi ? 6.0f : 0.0f;
	join->pager.foreground = 6;
	join->pager.line_count = 0.0f;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, free_holds,
	    sizeof(free_holds) - 1U, 0.0f)
	    || !normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, planet_prompt,
	    sizeof(planet_prompt) - 1U, 1.0f))
		return false;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	memcpy(join->accumulator, command, sizeof(command));
	if (yt_present_editor_echo(command, sizeof(command) - 1U,
	    command, sizeof(command) - 1U, &join->presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, port_label,
	    sizeof(port_label) - 1U, 0.0f))
		return false;
	join->presentation.foreground = 3.0f;
	join->pager.foreground = 3;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, docking,
	    sizeof(docking) - 1U, 1.0f)
	    || !normal_exit_b05d(join, turn, sizeof(turn) - 1U, 0.0f))
		return false;

	join->pager.line_count = 0.0f;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_line(join, owner, sizeof(owner) - 1U)
	    || !normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, title, sizeof(title) - 1U, 0.0f)
	    || !normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, header, sizeof(header) - 1U, 0.0f))
		return false;
	join->presentation.bold = 1.0f;
	if (!normal_exit_b05d(join, rule, sizeof(rule) - 1U, 0.0f))
		return false;
	for (index = 0U; index < 3U; ++index) {
		join->presentation.foreground = 2.0f;
		join->pager.foreground = 2;
		if (yt_present_character(item_prefix[index],
		    strlen((const char *)item_prefix[index]),
		    &join->presentation, &result) != YT_PRESENT_OK)
			return false;
		viewer_pager_capture_result(join, &result);
		if (yt_present_character(capacity[index], 12U,
		    &join->presentation, &result) != YT_PRESENT_OK)
			return false;
		viewer_pager_capture_result(join, &result);
		if (yt_present_character(hold[index], 11U,
		    &join->presentation, &result) != YT_PRESENT_OK)
			return false;
		viewer_pager_capture_result(join, &result);
		if (!normal_exit_line(join, price[index], 7U))
			return false;
	}

	join->presentation.foreground = 6.0f;
	join->pager.foreground = 6;
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	join->presentation.bold = 1.0f;
	join->presentation.blink = 1.0f;
	join->queue[0] = '\0';
	join->queue_position = 0U;
	join->queue_length = 0U;
	if (!normal_exit_b05d(join, refusal, sizeof(refusal) - 1U, 0.0f)
	    || !normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, status, sizeof(status) - 1U, 0.0f))
		return false;

	join->presentation.foreground = 1.0f;
	join->pager.foreground = 1;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_line(join, sector, sizeof(sector) - 1U)
	    || !sensor_join_present(join, &(const struct sensor_join_output){
	    SENSOR_JOIN_RAW, (const char *)warps})
	    || !sensor_join_present(join, &(const struct sensor_join_output){
	    SENSOR_JOIN_RAW, (const char *)warp_one})
	    || !sensor_join_present(join, &(const struct sensor_join_output){
	    SENSOR_JOIN_RAW, (const char *)warp_two})
	    || !normal_exit_line(join, NULL, 0U))
		return false;
	join->pager.line_count = 0.0f;
	join->presentation.foreground = 2.0f;
	join->pager.foreground = 2;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, main_prompt,
	    sizeof(main_prompt) - 1U, 1.0f))
		return false;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	return true;
}

static void
test_planet_port_refusal_cycle_presentation(void)
{
	static const uint8_t plain[] =
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? P\r\n"
	    "<Port>\n\r\r\nDocking, One Turn Deducted, 59 left.\n\r"
	    "\r\nThis port is owned by: YOU, Credits: 1234.5\r\n"
	    "\r\nCommerce report for Argus: 01-01-1991 00:00:00\n\r"
	    "\r\n Items         Status      # units    in holds   Cost\n\r"
	    "=======       =========   =========   ========   ====\n\r"
	    "Ore..........  Selling         100         10 20    \r\n"
	    "Organics.....  Selling         200         20 30    \r\n"
	    "Equipment....  Selling         300          5 40    \r\n"
	    "\r\nWe don't want your goods and you can't buy ours Pat!\n\r"
	    "\r\nYou have 12345 credits and 65 empty cargo holds.\n\r"
	    "\r\nSector: 733\r\nWarps lead to: 2, 9\r\n"
	    "\r\nTime:10:00  Main Command (?=Help)? ";
	static const uint8_t ansi[] =
	    "\r\nYou have 65 free cargo holds.\n\r"
	    "\r\nTime: 14:59  Planet command (?=help) [A]? P\r\n"
	    "<Port>\n\r\x1b[0;33;40m\r\n"
	    "Docking, One Turn Deducted, 59 left.\n\r"
	    "\r\nThis port is owned by: YOU, Credits: 1234.5\r\n"
	    "\r\nCommerce report for Argus: 01-01-1991 00:00:00\n\r"
	    "\r\n Items         Status      # units    in holds   Cost\n\r"
	    "\x1b[0;33;40;1m"
	    "=======       =========   =========   ========   ====\n\r"
	    "\x1b[0;32;40m"
	    "Ore..........  Selling         100         10 20    \r\n"
	    "Organics.....  Selling         200         20 30    \r\n"
	    "Equipment....  Selling         300          5 40    \r\n"
	    "\x1b[0;36;40m\r\n\x1b[0;36;40;5;1m"
	    "We don't want your goods and you can't buy ours Pat!\n\r"
	    "\x1b[0;36;40m\r\n"
	    "You have 12345 credits and 65 empty cargo holds.\n\r"
	    "\x1b[0;31;40m\r\nSector: 733\r\nWarps lead to: 2, 9\r\n"
	    "\x1b[0;32;40m\r\nTime:10:00  Main Command (?=Help)? ";
	static const struct {
		bool ansi;
		const uint8_t *expected;
		size_t expected_length;
	} cases[] = {
		{false, plain, sizeof(plain) - 1U},
		{true, ansi, sizeof(ansi) - 1U},
	};
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	uint8_t remote[800];
	size_t pass;

	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		memset(&viewer, 0, sizeof(viewer));
		fixture_viewer_initialize(&viewer, &stream,
		    retained_scoreboard, sizeof(retained_scoreboard) - 1U,
		    "YTSCORE.ASC", cases[pass].ansi, remote, sizeof(remote));
		CHECK(planet_port_refusal_cycle_run(&viewer, cases[pass].ansi));
		CHECK(viewer.join.remote_length == cases[pass].expected_length
		    && memcmp(remote, cases[pass].expected,
		    cases[pass].expected_length) == 0
		    && viewer.join.presentation.foreground == 2.0f
		    && viewer.join.presentation.background == 0.0f
		    && viewer.join.presentation.bold
		    == (cases[pass].ansi ? 0.0f : 1.0f)
		    && viewer.join.presentation.blink
		    == (cases[pass].ansi ? 0.0f : 1.0f)
		    && viewer.join.presentation.cached_foreground
		    == (cases[pass].ansi ? 2.0f : 0.0f)
		    && viewer.join.pager.foreground == 2
		    && viewer.join.pager.line_count == 0.0f
		    && viewer.join.pager.nonstop == 0.0f
		    && viewer.join.accumulator[0] == '\0'
		    && viewer.join.queue_length == 0U
		    && !stream.file_open && !viewer.join.file_open
		    && viewer.input.file == NULL && viewer.close_calls == 0U
		    && viewer.open_calls == 0U);
		yt_text_input_destroy(&viewer.input);
	}
	CHECK(sizeof(plain) - 1U == 680U && sizeof(ansi) - 1U == 766U);
}

static bool
docking_earth_fixed(struct viewer_pager_join *join, const char *text,
    float width)
{
	struct yt_present_result result;
	uint8_t field[80];
	size_t length = strlen(text);

	if (length > sizeof(field))
		return false;
	memcpy(field, text, length);
	if (yt_present_fixed_width(field, &length, sizeof(field), width,
	    &join->presentation, &result) != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	return true;
}

static bool
docking_earth_row(struct viewer_pager_join *join, const char *label,
    const char *cost, const char *affordable)
{
	return docking_earth_fixed(join, label, 22.0f)
	    && docking_earth_fixed(join, cost, 9.0f)
	    && normal_exit_b05d(join, (const uint8_t *)affordable,
	    strlen(affordable), 0.0f);
}

struct docking_earth_terminal_context {
	struct viewer_pager_join *join;
	bool closed;
};

static bool
docking_earth_terminal_notice(void *context, const uint8_t *notice,
    size_t length)
{
	struct docking_earth_terminal_context *terminal = context;

	return normal_exit_line(terminal->join, NULL, 0U)
	    && normal_exit_b05d(terminal->join, notice, length, 0.0f);
}

static bool
docking_earth_terminal_close(void *context)
{
	struct docking_earth_terminal_context *terminal = context;

	terminal->closed = true;
	return true;
}

static bool
docking_earth_leave_cycle_run(struct physical_viewer_join *viewer,
    bool ansi, enum yt_ab36_terminal_kind terminal_kind, size_t ends[3],
    bool *running, bool *terminated, bool *closed)
{
	static const char *const label[9] = {
		"[1] Cloak Energy", "[2] Cargo Holds", "[3] Fighters",
		"[4] Play Lottery", "[5] Danger Scanner",
		"[6] Anti-Cloak Device", "[7] Ground Forces",
		"[8] Shield Power", "[9] Hire Spies (Each)"
	};
	static const char *const cost[9] = {
		"* 1000 ", "* 250 ", "* 50 ", "* 5", "* 500000 ",
		"* 1E+09 ", "* 200 ", "* 50 ", "* 1E+09 "
	};
	static const char *const affordable[9] = {
		"* 12", "* 49", "* 246", "* 2469", "* 0", "* 0",
		"* 61", "* 246", "* 0"
	};
	static const uint8_t port_label[] = "<Port>";
	static const uint8_t docking[] = "Docking, ";
	static const uint8_t turn[] = "One Turn Deducted, 59 left.";
	static const uint8_t title[] =
	    "Commerce report for Earth: 07-25-2026 12:34:56";
	static const uint8_t separator[] =
	    "----------------------*--------*------------";
	static const uint8_t header[] =
	    "         ITEM         *  COST  * CAN AFFORD";
	static const uint8_t menu[] =
	    "[I] Ship Info -=*=- [0] Leave Port";
	static const uint8_t prompt[] =
	    "Credits: 12345 -=*=- Buy Which Item? -=>";
	static const uint8_t response[] = "0";
	static const uint8_t sector[] = "Sector: 733";
	static const uint8_t warps[] = "Warps lead to:";
	static const uint8_t warp_one[] = " 2";
	static const uint8_t warp_two[] = ", 9";
	static const uint8_t main_prompt[] =
	    "Time:10:00  Main Command (?=Help)? ";
	struct viewer_pager_join *join = &viewer->join;
	struct docking_earth_terminal_context terminal = {join, false};
	struct yt_present_result result;
	size_t index;

	if (ends == NULL)
		return false;
	join->presentation = state(ansi);
	join->presentation.foreground = 6.0f;
	join->pager.foreground = 6;
	join->pager.line_count = 0.0f;
	if (!normal_exit_b05d(join, port_label,
	    sizeof(port_label) - 1U, 0.0f))
		return false;
	join->presentation.foreground = 3.0f;
	join->pager.foreground = 3;
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	if (!normal_exit_b05d(join, docking, sizeof(docking) - 1U, 1.0f)
	    || !normal_exit_b05d(join, turn, sizeof(turn) - 1U, 0.0f))
		return false;
	ends[0] = join->remote_length;

	/* YT:6CB3 resets the shared pager before its independent Earth GET. */
	join->pager.line_count = 0.0f;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, title, sizeof(title) - 1U, 0.0f)
	    || !normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, separator,
	    sizeof(separator) - 1U, 0.0f)
	    || !normal_exit_b05d(join, header, sizeof(header) - 1U, 0.0f)
	    || !normal_exit_b05d(join, separator,
	    sizeof(separator) - 1U, 0.0f))
		return false;
	for (index = 0U; index < 9U; ++index) {
		if (!docking_earth_row(join, label[index], cost[index],
		    affordable[index]))
			return false;
	}
	if (!normal_exit_b05d(join, separator, sizeof(separator) - 1U, 0.0f)
	    || !normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, menu, sizeof(menu) - 1U, 0.0f)
	    || !normal_exit_line(join, NULL, 0U))
		return false;
	if (!normal_exit_b05d(join, prompt, sizeof(prompt) - 1U, 1.0f))
		return false;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	if (terminal_kind == YT_AB36_TERMINAL_INACTIVITY
	    || terminal_kind == YT_AB36_TERMINAL_SESSION_LIMIT) {
		if (running == NULL || terminated == NULL || closed == NULL)
			return false;
		*running = true;
		*terminated = false;
		*closed = false;
		if (!yt_input_ab36_terminal_run(terminal_kind, running,
		    terminated, docking_earth_terminal_notice,
		    docking_earth_terminal_close, &terminal))
			return false;
		*closed = terminal.closed;
		ends[1] = join->remote_length;
		ends[2] = join->remote_length;
		return true;
	}
	memcpy(join->accumulator, response, sizeof(response));
	if (yt_present_editor_echo(response, sizeof(response) - 1U,
	    response, sizeof(response) - 1U, &join->presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	ends[1] = join->remote_length;

	join->presentation.foreground = 1.0f;
	join->pager.foreground = 1;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_line(join, sector, sizeof(sector) - 1U)
	    || !sensor_join_present(join, &(const struct sensor_join_output){
	    SENSOR_JOIN_RAW, (const char *)warps})
	    || !sensor_join_present(join, &(const struct sensor_join_output){
	    SENSOR_JOIN_RAW, (const char *)warp_one})
	    || !sensor_join_present(join, &(const struct sensor_join_output){
	    SENSOR_JOIN_RAW, (const char *)warp_two})
	    || !normal_exit_line(join, NULL, 0U))
		return false;
	join->pager.line_count = 0.0f;
	join->presentation.foreground = 2.0f;
	join->pager.foreground = 2;
	if (!normal_exit_line(join, NULL, 0U)
	    || !normal_exit_b05d(join, main_prompt,
	    sizeof(main_prompt) - 1U, 1.0f))
		return false;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	ends[2] = join->remote_length;
	return true;
}

static void
test_docking_earth_leave_cycle_presentation(void)
{
	static const uint8_t plain[] =
	    "<Port>\n\r\r\nDocking, One Turn Deducted, 59 left.\n\r"
	    "\r\nCommerce report for Earth: 07-25-2026 12:34:56\n\r"
	    "\r\n----------------------*--------*------------\n\r"
	    "         ITEM         *  COST  * CAN AFFORD\n\r"
	    "----------------------*--------*------------\n\r"
	    "[1] Cloak Energy      * 1000   * 12\n\r"
	    "[2] Cargo Holds       * 250    * 49\n\r"
	    "[3] Fighters          * 50     * 246\n\r"
	    "[4] Play Lottery      * 5      * 2469\n\r"
	    "[5] Danger Scanner    * 500000 * 0\n\r"
	    "[6] Anti-Cloak Device * 1E+09  * 0\n\r"
	    "[7] Ground Forces     * 200    * 61\n\r"
	    "[8] Shield Power      * 50     * 246\n\r"
	    "[9] Hire Spies (Each) * 1E+09  * 0\n\r"
	    "----------------------*--------*------------\n\r"
	    "\r\n[I] Ship Info -=*=- [0] Leave Port\n\r"
	    "\r\nCredits: 12345 -=*=- Buy Which Item? -=>0\r\n"
	    "\r\nSector: 733\r\nWarps lead to: 2, 9\r\n"
	    "\r\nTime:10:00  Main Command (?=Help)? ";
	static const uint8_t ansi[] =
	    "\x1b[0;36;40m<Port>\n\r\x1b[0;33;40m\r\n"
	    "Docking, One Turn Deducted, 59 left.\n\r"
	    "\r\nCommerce report for Earth: 07-25-2026 12:34:56\n\r"
	    "\r\n----------------------*--------*------------\n\r"
	    "         ITEM         *  COST  * CAN AFFORD\n\r"
	    "----------------------*--------*------------\n\r"
	    "[1] Cloak Energy      * 1000   * 12\n\r"
	    "[2] Cargo Holds       * 250    * 49\n\r"
	    "[3] Fighters          * 50     * 246\n\r"
	    "[4] Play Lottery      * 5      * 2469\n\r"
	    "[5] Danger Scanner    * 500000 * 0\n\r"
	    "[6] Anti-Cloak Device * 1E+09  * 0\n\r"
	    "[7] Ground Forces     * 200    * 61\n\r"
	    "[8] Shield Power      * 50     * 246\n\r"
	    "[9] Hire Spies (Each) * 1E+09  * 0\n\r"
	    "----------------------*--------*------------\n\r"
	    "\r\n[I] Ship Info -=*=- [0] Leave Port\n\r"
	    "\r\nCredits: 12345 -=*=- Buy Which Item? -=>0\r\n"
	    "\x1b[0;31;40m\r\nSector: 733\r\nWarps lead to: 2, 9\r\n"
	    "\x1b[0;32;40m\r\nTime:10:00  Main Command (?=Help)? ";
	static const struct {
		bool ansi;
		const uint8_t *expected;
		size_t expected_length;
		size_t ends[3];
	} cases[] = {
		{false, plain, sizeof(plain) - 1U, {48U, 700U, 773U}},
		{true, ansi, sizeof(ansi) - 1U, {68U, 720U, 813U}},
	};
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	uint8_t remote[850];
	size_t ends[3];
	size_t pass;

	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		memset(&viewer, 0, sizeof(viewer));
		fixture_viewer_initialize(&viewer, &stream,
		    retained_scoreboard, sizeof(retained_scoreboard) - 1U,
		    "YTSCORE.ASC", cases[pass].ansi, remote, sizeof(remote));
		CHECK(docking_earth_leave_cycle_run(&viewer,
		    cases[pass].ansi, (enum yt_ab36_terminal_kind)-1,
		    ends, NULL, NULL, NULL));
		CHECK(memcmp(ends, cases[pass].ends, sizeof(ends)) == 0);
		CHECK(viewer.join.remote_length == cases[pass].expected_length);
		CHECK(viewer.join.remote_length != cases[pass].expected_length
		    || memcmp(remote, cases[pass].expected,
		    cases[pass].expected_length) == 0);
		CHECK(viewer.join.presentation.foreground == 2.0f
		    && viewer.join.presentation.background == 0.0f
		    && viewer.join.presentation.bold == 0.0f
		    && viewer.join.presentation.blink == 0.0f
		    && viewer.join.presentation.cached_foreground
		    == (cases[pass].ansi ? 2.0f : 0.0f)
		    && viewer.join.pager.foreground == 2
		    && viewer.join.pager.line_count == 0.0f
		    && viewer.join.pager.nonstop == 0.0f
		    && viewer.join.local_fragment_length == 35U
		    && memcmp(viewer.join.local_fragment,
		    "Time:10:00  Main Command (?=Help)? ", 35U) == 0
		    && viewer.join.accumulator[0] == '\0'
		    && viewer.join.queue_length == 0U);
		CHECK(viewer.join.sample_calls == 20U
		    && stream.eof_checks == 0U && stream.key_checks == 0U
		    && stream.read_count == 0U && stream.line_count == 0U
		    && !stream.file_open && !viewer.join.file_open
		    && viewer.input.file == NULL && viewer.close_calls == 0U
		    && viewer.open_calls == 0U);
		yt_text_input_destroy(&viewer.input);
	}
	{
		static const uint8_t inactivity[] =
		    "\r\n\aUSER FELL ASLEEP!\n\r";
		static const uint8_t session_limit[] =
		    "\r\n\a\a\aTIME LIMIT EXCEEDED!\a\a\a\n\r";
		static const struct {
			enum yt_ab36_terminal_kind kind;
			const uint8_t *suffix;
			size_t suffix_length;
		} terminals[] = {
			{YT_AB36_TERMINAL_INACTIVITY, inactivity,
			    sizeof(inactivity) - 1U},
			{YT_AB36_TERMINAL_SESSION_LIMIT, session_limit,
			    sizeof(session_limit) - 1U},
		};
		size_t terminal_index;

		for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
			for (terminal_index = 0U;
			    terminal_index < YT_ARRAY_LEN(terminals);
			    ++terminal_index) {
				size_t prompt_end = cases[pass].ansi ? 717U : 697U;
				bool running;
				bool terminated;
				bool closed;

				memset(&viewer, 0, sizeof(viewer));
				fixture_viewer_initialize(&viewer, &stream,
				    retained_scoreboard,
				    sizeof(retained_scoreboard) - 1U,
				    "YTSCORE.ASC", cases[pass].ansi, remote,
				    sizeof(remote));
				CHECK(docking_earth_leave_cycle_run(&viewer,
				    cases[pass].ansi,
				    terminals[terminal_index].kind, ends,
				    &running, &terminated, &closed));
				CHECK(ends[0] == cases[pass].ends[0]
				    && ends[1] == prompt_end
				    + terminals[terminal_index].suffix_length
				    && ends[2] == ends[1]
				    && viewer.join.remote_length == ends[1]
				    && memcmp(remote, cases[pass].expected,
				    prompt_end) == 0
				    && memcmp(remote + prompt_end,
				    terminals[terminal_index].suffix,
				    terminals[terminal_index].suffix_length) == 0
				    && !running && terminated && closed
				    && viewer.join.pager.line_count == 1.0f
				    && viewer.join.pager.newline_flag == 0.0f
				    && viewer.join.sample_calls == 20U
				    && !stream.file_open && !viewer.join.file_open
				    && viewer.input.file == NULL);
				yt_text_input_destroy(&viewer.input);
			}
		}
	}
	CHECK(sizeof(plain) - 1U == 773U && sizeof(ansi) - 1U == 813U);
}

static bool
computer_quit_accept_prefix(struct physical_viewer_join *viewer, bool ansi)
{
	static const uint8_t prompt[] =
	    "Time: 14:59  Computer command (?=help)? ";
	static const uint8_t heading[] = "<Quit>";
	static const uint8_t confirmation[] = "Are you sure (Y/N)? ";
	static const uint8_t q[] = "Q";
	static const uint8_t y[] = "Y";
	struct viewer_pager_join *join = &viewer->join;
	struct yt_present_result result;

	join->presentation = state(ansi);
	join->presentation.foreground = 6.0f;
	join->pager.foreground = 6;
	join->pager.line_count = 0.0f;
	if (ansi) {
		if (yt_present_color(&join->presentation, &result)
		    != YT_PRESENT_OK)
			return false;
	}
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	join->presentation.foreground = 1.0f;
	join->pager.foreground = 1;
	if (!normal_exit_b05d(join, prompt, sizeof(prompt) - 1U, 1.0f))
		return false;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	memcpy(join->accumulator, q, sizeof(q));
	if (yt_present_editor_echo(q, sizeof(q) - 1U, q, sizeof(q) - 1U,
	    &join->presentation, &result) != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	join->presentation.foreground = 7.0f;
	join->pager.foreground = 7;
	if (!normal_exit_b05d(join, heading, sizeof(heading) - 1U, 0.0f))
		return false;
	if (yt_present_character(confirmation, sizeof(confirmation) - 1U,
	    &join->presentation, &result) != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	memcpy(join->accumulator, y, sizeof(y));
	if (yt_present_editor_echo(y, sizeof(y) - 1U, y, sizeof(y) - 1U,
	    &join->presentation, &result) != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	memcpy(join->source, y, sizeof(y));
	join->source_length = sizeof(y) - 1U;
	return true;
}

static void
test_computer_quit_accept_presentation(void)
{
	static const uint8_t returning[] = "Returning to Example BBS...";
	static const uint8_t time_text[] = " 14:59  ";
	static const struct {
		bool ansi;
		bool evaluation;
		size_t prefix_length;
		uint64_t prefix_fnv;
		size_t remote_length;
		uint64_t remote_fnv;
		size_t local_rows;
		uint64_t local_fnv;
		size_t colors;
		uint64_t color_fnv;
		size_t color_2;
		size_t color_3;
		size_t color_4;
		size_t color_6;
		size_t color_7;
		size_t color_15_1;
		size_t color_30_4;
		float final_foreground;
		float final_bold;
		float final_blink;
	} cases[] = {
		{true, false, 96U, UINT64_C(0x45eb0112a4c890ab),
		    1486U, UINT64_C(0x2ba3dd656dba1e24), 47U,
		    UINT64_C(0xd00cf13ed2b3256c), 98U,
		    UINT64_C(0xed4be8e9f309720b), 53U, 1U, 14U, 0U,
		    27U, 3U, 0U, 1.0f, 0.0f, 0.0f},
		{true, true, 96U, UINT64_C(0x45eb0112a4c890ab),
		    1556U, UINT64_C(0x29b38d17beab85dc), 49U,
		    UINT64_C(0x4d6205171ec1dc71), 101U,
		    UINT64_C(0x729ff8d62687d8f3), 53U, 1U, 13U, 3U,
		    27U, 3U, 1U, 3.0f, 0.0f, 0.0f},
		{false, false, 76U, UINT64_C(0x6500496cfbaef11f),
		    1360U, UINT64_C(0x3715fad633cdc227), 47U,
		    UINT64_C(0xd00cf13ed2b3256c), 24U,
		    UINT64_C(0x24ce503d7547c545), 0U, 0U, 0U, 0U,
		    24U, 0U, 0U, 1.0f, 1.0f, 0.0f},
		{false, true, 76U, UINT64_C(0x6500496cfbaef11f),
		    1406U, UINT64_C(0xf303a068c40b4d04), 49U,
		    UINT64_C(0x4d6205171ec1dc71), 24U,
		    UINT64_C(0x24ce503d7547c545), 0U, 0U, 0U, 0U,
		    24U, 0U, 0U, 3.0f, 1.0f, 1.0f},
	};
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	struct normal_exit_body_observation observation;
	uint8_t remote[1700];
	size_t pass;

	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		memset(&viewer, 0, sizeof(viewer));
		fixture_viewer_initialize(&viewer, &stream,
		    retained_scoreboard, sizeof(retained_scoreboard) - 1U,
		    "YTSCORE.ASC", cases[pass].ansi, remote, sizeof(remote));
		CHECK(computer_quit_accept_prefix(&viewer, cases[pass].ansi));
		CHECK(viewer.join.remote_length == cases[pass].prefix_length
		    && viewer_bytes_fnv1a64(remote, viewer.join.remote_length)
		    == cases[pass].prefix_fnv
		    && viewer.join.presentation.foreground == 7.0f
		    && viewer.join.presentation.background == 0.0f
		    && viewer.join.presentation.bold == 0.0f
		    && viewer.join.presentation.blink == 0.0f
		    && viewer.join.presentation.cached_foreground
		    == (cases[pass].ansi ? 7.0f : 0.0f)
		    && viewer.join.pager.foreground == 7
		    && viewer.join.pager.line_count == 0.0f
		    && viewer.join.pager.nonstop == 0.0f
		    && strcmp(viewer.join.accumulator, "Y") == 0
		    && viewer.join.source_length == 1U
		    && viewer.join.source[0] == 'Y');
		CHECK(normal_exit_body_run(&viewer, &stream,
		    cases[pass].evaluation, time_text, sizeof(time_text) - 1U,
		    14.0f, &observation));
		CHECK(observation.info_end == cases[pass].prefix_length
		    + (cases[pass].ansi ? 678U : 602U)
		    && observation.post_info_end == cases[pass].prefix_length
		    + (cases[pass].ansi ? 690U : 604U)
		    && observation.generating_end == cases[pass].prefix_length
		    + (cases[pass].ansi ? 711U : 625U)
		    && observation.progress_end == cases[pass].prefix_length
		    + (cases[pass].ansi ? 715U : 629U)
		    && observation.post_generator_end == cases[pass].prefix_length
		    + (cases[pass].ansi ? 717U : 631U)
		    && observation.viewer_end == cases[pass].prefix_length
		    + (cases[pass].ansi ? 1361U : 1255U)
		    && observation.waited == cases[pass].evaluation);
		CHECK(viewer.join.remote_length == cases[pass].remote_length
		    && viewer_bytes_fnv1a64(remote, viewer.join.remote_length)
		    == cases[pass].remote_fnv);
		CHECK(viewer.join.local_row_count == cases[pass].local_rows
		    && viewer_rows_fnv1a64(&viewer.join)
		    == cases[pass].local_fnv
		    && viewer.join.local_fragment_length == 0U
		    && viewer.join.local_color_count == cases[pass].colors
		    && viewer_colors_fnv1a64(&viewer.join)
		    == cases[pass].color_fnv
		    && viewer_local_color_count(&viewer.join, 2, 0)
		    == cases[pass].color_2
		    && viewer_local_color_count(&viewer.join, 3, 0)
		    == cases[pass].color_3
		    && viewer_local_color_count(&viewer.join, 4, 0)
		    == cases[pass].color_4
		    && viewer_local_color_count(&viewer.join, 6, 0)
		    == cases[pass].color_6
		    && viewer_local_color_count(&viewer.join, 7, 0)
		    == cases[pass].color_7
		    && viewer_local_color_count(&viewer.join, 15, 1)
		    == cases[pass].color_15_1
		    && viewer_local_color_count(&viewer.join, 30, 4)
		    == cases[pass].color_30_4);
		CHECK(viewer.join.presentation.foreground
		    == cases[pass].final_foreground
		    && viewer.join.presentation.background == 0.0f
		    && viewer.join.presentation.bold == cases[pass].final_bold
		    && viewer.join.presentation.blink == cases[pass].final_blink
		    && viewer.join.pager.foreground == 1
		    && viewer.join.pager.line_count == 1.0f
		    && viewer.join.pager.nonstop == 1.0f
		    && viewer.join.pager.key[0] == '\0');
		CHECK(viewer.join.position == 19U
		    && stream.eof_checks == 20U && stream.key_checks == 20U
		    && stream.read_count == 19U && stream.line_count == 19U
		    && viewer.join.sample_calls == 24U
		    && viewer.join.response_calls == 0U
		    && viewer.join.direct_calls == 2U
		    && viewer.join.event_count == 120U
		    && !stream.file_open && !viewer.join.file_open
		    && viewer.input.file == NULL && viewer.close_calls == 2U
		    && viewer.open_calls == 1U
		    && viewer.join.source_length == sizeof(returning) - 1U
		    && memcmp(viewer.join.source, returning,
		    sizeof(returning) - 1U) == 0
		    && strcmp(viewer.join.accumulator, "Y") == 0
		    && viewer.join.queue_length == 0U);
		yt_text_input_destroy(&viewer.input);
	}
}

static bool
planet_quit_accept_prefix(struct physical_viewer_join *viewer, bool ansi)
{
	static const uint8_t free_holds[] =
	    "You have 5 free cargo holds.";
	static const uint8_t prompt[] =
	    "Time: 14:59  Planet command (?=help) [A]? ";
	static const uint8_t heading[] = "<Quit>";
	static const uint8_t confirmation[] = "Are you sure (Y/N)? ";
	static const uint8_t q[] = "Q";
	static const uint8_t y[] = "Y";
	struct viewer_pager_join *join = &viewer->join;
	struct yt_present_result result;

	join->presentation = state(ansi);
	join->presentation.foreground = 6.0f;
	join->presentation.cached_foreground = ansi ? 6.0f : 0.0f;
	join->pager.foreground = 6;
	join->pager.line_count = 0.0f;
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	if (!normal_exit_b05d(join, free_holds, sizeof(free_holds) - 1U,
	    0.0f))
		return false;
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	if (!normal_exit_b05d(join, prompt, sizeof(prompt) - 1U, 1.0f))
		return false;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	memcpy(join->accumulator, q, sizeof(q));
	if (yt_present_editor_echo(q, sizeof(q) - 1U, q, sizeof(q) - 1U,
	    &join->presentation, &result) != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	join->presentation.foreground = 7.0f;
	join->pager.foreground = 7;
	if (!normal_exit_b05d(join, heading, sizeof(heading) - 1U, 0.0f))
		return false;
	if (yt_present_character(confirmation, sizeof(confirmation) - 1U,
	    &join->presentation, &result) != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	memcpy(join->accumulator, y, sizeof(y));
	if (yt_present_editor_echo(y, sizeof(y) - 1U, y, sizeof(y) - 1U,
	    &join->presentation, &result) != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	memcpy(join->source, y, sizeof(y));
	join->source_length = sizeof(y) - 1U;
	return true;
}

static void
test_planet_quit_accept_presentation(void)
{
	static const uint8_t returning[] = "Returning to Example BBS...";
	static const uint8_t time_text[] = " 14:59  ";
	static const struct {
		bool ansi;
		bool evaluation;
		size_t prefix_length;
		uint64_t prefix_fnv;
		size_t remote_length;
		uint64_t remote_fnv;
		size_t local_rows;
		uint64_t local_fnv;
		size_t colors;
		uint64_t color_fnv;
		size_t color_2;
		size_t color_3;
		size_t color_4;
		size_t color_6;
		size_t color_7;
		size_t color_15_1;
		size_t color_30_4;
		float final_foreground;
		float final_bold;
		float final_blink;
	} cases[] = {
		{true, false, 120U, UINT64_C(0x6403d4c264cf2586),
		    1510U, UINT64_C(0xb836c172eec0539d), 49U,
		    UINT64_C(0xddc486733d372e72), 101U,
		    UINT64_C(0x616fae30a6e0a184), 53U, 5U, 12U, 0U,
		    28U, 3U, 0U, 1.0f, 0.0f, 0.0f},
		{true, true, 120U, UINT64_C(0x6403d4c264cf2586),
		    1580U, UINT64_C(0x35260f1075897b45), 51U,
		    UINT64_C(0x330dbb407193215f), 104U,
		    UINT64_C(0x810431739b2dcc7c), 53U, 5U, 11U, 3U,
		    28U, 3U, 1U, 3.0f, 0.0f, 0.0f},
		{false, false, 110U, UINT64_C(0x5da92b4ec3869781),
		    1394U, UINT64_C(0xbc1ee8a9a18a2e05), 49U,
		    UINT64_C(0xddc486733d372e72), 25U,
		    UINT64_C(0x5f0a8f65f6ec1d92), 0U, 0U, 0U, 0U,
		    25U, 0U, 0U, 1.0f, 1.0f, 0.0f},
		{false, true, 110U, UINT64_C(0x5da92b4ec3869781),
		    1440U, UINT64_C(0x4a874e002aa30e36), 51U,
		    UINT64_C(0x330dbb407193215f), 25U,
		    UINT64_C(0x5f0a8f65f6ec1d92), 0U, 0U, 0U, 0U,
		    25U, 0U, 0U, 3.0f, 1.0f, 1.0f},
	};
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	struct normal_exit_body_observation observation;
	uint8_t remote[1700];
	size_t pass;

	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		memset(&viewer, 0, sizeof(viewer));
		fixture_viewer_initialize(&viewer, &stream,
		    retained_scoreboard, sizeof(retained_scoreboard) - 1U,
		    "YTSCORE.ASC", cases[pass].ansi, remote, sizeof(remote));
		CHECK(planet_quit_accept_prefix(&viewer, cases[pass].ansi));
		CHECK(viewer.join.remote_length == cases[pass].prefix_length
		    && viewer_bytes_fnv1a64(remote, viewer.join.remote_length)
		    == cases[pass].prefix_fnv
		    && viewer.join.presentation.foreground == 7.0f
		    && viewer.join.presentation.background == 0.0f
		    && viewer.join.presentation.bold == 0.0f
		    && viewer.join.presentation.blink == 0.0f
		    && viewer.join.presentation.cached_foreground
		    == (cases[pass].ansi ? 7.0f : 0.0f)
		    && viewer.join.pager.foreground == 7
		    && viewer.join.pager.line_count == 0.0f
		    && viewer.join.pager.nonstop == 0.0f
		    && strcmp(viewer.join.accumulator, "Y") == 0
		    && viewer.join.source_length == 1U
		    && viewer.join.source[0] == 'Y');
		CHECK(normal_exit_body_run(&viewer, &stream,
		    cases[pass].evaluation, time_text, sizeof(time_text) - 1U,
		    14.0f, &observation));
		CHECK(observation.info_end == cases[pass].prefix_length
		    + (cases[pass].ansi ? 678U : 602U)
		    && observation.post_info_end == cases[pass].prefix_length
		    + (cases[pass].ansi ? 690U : 604U)
		    && observation.generating_end == cases[pass].prefix_length
		    + (cases[pass].ansi ? 711U : 625U)
		    && observation.progress_end == cases[pass].prefix_length
		    + (cases[pass].ansi ? 715U : 629U)
		    && observation.post_generator_end == cases[pass].prefix_length
		    + (cases[pass].ansi ? 717U : 631U)
		    && observation.viewer_end == cases[pass].prefix_length
		    + (cases[pass].ansi ? 1361U : 1255U)
		    && observation.waited == cases[pass].evaluation);
		CHECK(viewer.join.remote_length == cases[pass].remote_length
		    && viewer_bytes_fnv1a64(remote, viewer.join.remote_length)
		    == cases[pass].remote_fnv);
		CHECK(viewer.join.local_row_count == cases[pass].local_rows
		    && viewer_rows_fnv1a64(&viewer.join)
		    == cases[pass].local_fnv
		    && viewer.join.local_fragment_length == 0U
		    && viewer.join.local_color_count == cases[pass].colors
		    && viewer_colors_fnv1a64(&viewer.join)
		    == cases[pass].color_fnv
		    && viewer_local_color_count(&viewer.join, 2, 0)
		    == cases[pass].color_2
		    && viewer_local_color_count(&viewer.join, 3, 0)
		    == cases[pass].color_3
		    && viewer_local_color_count(&viewer.join, 4, 0)
		    == cases[pass].color_4
		    && viewer_local_color_count(&viewer.join, 6, 0)
		    == cases[pass].color_6
		    && viewer_local_color_count(&viewer.join, 7, 0)
		    == cases[pass].color_7
		    && viewer_local_color_count(&viewer.join, 15, 1)
		    == cases[pass].color_15_1
		    && viewer_local_color_count(&viewer.join, 30, 4)
		    == cases[pass].color_30_4);
		CHECK(viewer.join.presentation.foreground
		    == cases[pass].final_foreground
		    && viewer.join.presentation.background == 0.0f
		    && viewer.join.presentation.bold == cases[pass].final_bold
		    && viewer.join.presentation.blink == cases[pass].final_blink
		    && viewer.join.pager.foreground == 1
		    && viewer.join.pager.line_count == 1.0f
		    && viewer.join.pager.nonstop == 1.0f
		    && viewer.join.pager.key[0] == '\0');
		CHECK(viewer.join.position == 19U
		    && stream.eof_checks == 20U && stream.key_checks == 20U
		    && stream.read_count == 19U && stream.line_count == 19U
		    && viewer.join.sample_calls == 25U
		    && viewer.join.response_calls == 0U
		    && viewer.join.direct_calls == 2U
		    && viewer.join.event_count == 125U
		    && !stream.file_open && !viewer.join.file_open
		    && viewer.input.file == NULL && viewer.close_calls == 2U
		    && viewer.open_calls == 1U
		    && viewer.join.source_length == sizeof(returning) - 1U
		    && memcmp(viewer.join.source, returning,
		    sizeof(returning) - 1U) == 0
		    && strcmp(viewer.join.accumulator, "Y") == 0
		    && viewer.join.queue_length == 0U);
		yt_text_input_destroy(&viewer.input);
	}
}

static bool
hostile_quit_accept_prefix(struct physical_viewer_join *viewer, bool ansi)
{
	static const uint8_t fighter_row[] = "Fighters: 1000 / 1250";
	static const uint8_t prompt[] =
	    "Option? (A,B,D,I,Q,S,T,W,?=Help):? ";
	static const uint8_t heading[] = "<Quit>";
	static const uint8_t confirmation[] = "Are you sure (Y/N)? ";
	static const uint8_t q[] = "Q";
	static const uint8_t y[] = "Y";
	struct viewer_pager_join *join = &viewer->join;
	struct yt_present_result result;

	join->presentation = state(ansi);
	join->presentation.foreground = 6.0f;
	join->presentation.cached_foreground = ansi ? 6.0f : 0.0f;
	join->pager.foreground = 6;
	join->pager.line_count = 1.0f;
	join->presentation.foreground = 3.0f;
	join->pager.foreground = 3;
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	if (!normal_exit_b05d(join, fighter_row, sizeof(fighter_row) - 1U,
	    0.0f))
		return false;
	if (!normal_exit_b05d(join, prompt, sizeof(prompt) - 1U, 1.0f))
		return false;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	memcpy(join->accumulator, q, sizeof(q));
	if (yt_present_editor_echo(q, sizeof(q) - 1U, q, sizeof(q) - 1U,
	    &join->presentation, &result) != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	join->presentation.foreground = 7.0f;
	join->pager.foreground = 7;
	if (!normal_exit_b05d(join, heading, sizeof(heading) - 1U, 0.0f))
		return false;
	if (yt_present_character(confirmation, sizeof(confirmation) - 1U,
	    &join->presentation, &result) != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	memcpy(join->accumulator, y, sizeof(y));
	if (yt_present_editor_echo(y, sizeof(y) - 1U, y, sizeof(y) - 1U,
	    &join->presentation, &result) != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	memcpy(join->source, y, sizeof(y));
	join->source_length = sizeof(y) - 1U;
	return true;
}

static void
test_hostile_quit_accept_presentation(void)
{
	static const uint8_t cached_name[] = "CACHED CURRENT";
	static const uint8_t returning[] = "Returning to Example BBS...";
	static const uint8_t time_text[] = " 14:59  ";
	static const struct {
		bool ansi;
		bool evaluation;
		size_t prefix_length;
		uint64_t prefix_fnv;
		size_t remote_length;
		uint64_t remote_fnv;
		size_t local_rows;
		uint64_t local_fnv;
		size_t colors;
		uint64_t color_fnv;
		size_t color_2;
		size_t color_4;
		size_t color_6;
		size_t color_7;
		size_t color_30_4;
		float final_foreground;
		float final_bold;
		float final_blink;
	} cases[] = {
		{true, false, 114U, UINT64_C(0xabfdb921754093b5),
		    1447U, UINT64_C(0x978df57234194f37), 48U,
		    UINT64_C(0x80e907a579f46e7c), 100U,
		    UINT64_C(0xaf7c144dcbc2e68d), 56U, 12U, 4U, 28U,
		    0U, 1.0f, 0.0f, 0.0f},
		{true, true, 114U, UINT64_C(0xabfdb921754093b5),
		    1517U, UINT64_C(0x8231003add4e6f83), 50U,
		    UINT64_C(0x981ef2b6217f8841), 103U,
		    UINT64_C(0x9384d8dbda922efd), 56U, 11U, 7U, 28U,
		    1U, 3.0f, 0.0f, 0.0f},
		{false, false, 94U, UINT64_C(0x6af9b06762ddda83),
		    1387U, UINT64_C(0x06a37583f874ea33), 48U,
		    UINT64_C(0x80e907a579f46e7c), 25U,
		    UINT64_C(0x5f0a8f65f6ec1d92), 0U, 0U, 0U, 25U,
		    0U, 1.0f, 0.0f, 0.0f},
		{false, true, 94U, UINT64_C(0x6af9b06762ddda83),
		    1433U, UINT64_C(0x52b3b64531691d58), 50U,
		    UINT64_C(0x981ef2b6217f8841), 25U,
		    UINT64_C(0x5f0a8f65f6ec1d92), 0U, 0U, 0U, 25U,
		    0U, 3.0f, 0.0f, 1.0f},
	};
	struct normal_exit_info_values info;
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	struct normal_exit_body_observation observation;
	uint8_t remote[1700];
	size_t pass;

	memset(&info, 0, sizeof(info));
	info.player.credits = 1005.0f;
	info.player.sector = 733.0f;
	info.player.turns = 17.0f;
	info.player.holds = 10.0f;
	info.player.fighters = 1000.0f;
	info.player.ore = 0.0f;
	info.player.mines = 0.0f;
	info.player.organics = 0.0f;
	info.player.missiles = 5.0f;
	info.player.equipment = 0.0f;
	info.player.danger_scanner = 1.0f;
	info.player.ports_owned = 0.0f;
	info.player.shields = 100.0f;
	info.player.cloak = 0.5f;
	info.player.ground_forces = 0.0f;
	info.player.plasma = 4.0f;
	info.cached_name = cached_name;
	info.cached_name_length = sizeof(cached_name) - 1U;
	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		memset(&viewer, 0, sizeof(viewer));
		fixture_viewer_initialize(&viewer, &stream,
		    retained_scoreboard, sizeof(retained_scoreboard) - 1U,
		    "YTSCORE.ASC", cases[pass].ansi, remote, sizeof(remote));
		CHECK(hostile_quit_accept_prefix(&viewer, cases[pass].ansi));
		CHECK(viewer.join.remote_length == cases[pass].prefix_length
		    && viewer_bytes_fnv1a64(remote, viewer.join.remote_length)
		    == cases[pass].prefix_fnv
		    && viewer.join.presentation.foreground == 7.0f
		    && viewer.join.presentation.background == 0.0f
		    && viewer.join.presentation.bold == 0.0f
		    && viewer.join.presentation.blink == 0.0f
		    && viewer.join.presentation.cached_foreground
		    == (cases[pass].ansi ? 7.0f : 0.0f)
		    && viewer.join.pager.foreground == 7
		    && viewer.join.pager.line_count == 0.0f
		    && viewer.join.pager.nonstop == 0.0f
		    && strcmp(viewer.join.accumulator, "Y") == 0
		    && viewer.join.source_length == 1U
		    && viewer.join.source[0] == 'Y');
		CHECK(normal_exit_body_run_info(&viewer, &stream,
		    cases[pass].evaluation, time_text, sizeof(time_text) - 1U,
		    14.0f, &info, &observation));
		CHECK(observation.info_end == cases[pass].prefix_length
		    + (cases[pass].ansi ? 621U : 611U)
		    && observation.post_info_end == cases[pass].prefix_length
		    + (cases[pass].ansi ? 633U : 613U)
		    && observation.generating_end == cases[pass].prefix_length
		    + (cases[pass].ansi ? 654U : 634U)
		    && observation.progress_end == cases[pass].prefix_length
		    + (cases[pass].ansi ? 658U : 638U)
		    && observation.post_generator_end == cases[pass].prefix_length
		    + (cases[pass].ansi ? 660U : 640U)
		    && observation.viewer_end == cases[pass].prefix_length
		    + (cases[pass].ansi ? 1304U : 1264U)
		    && observation.waited == cases[pass].evaluation);
		CHECK(viewer.join.remote_length == cases[pass].remote_length
		    && viewer_bytes_fnv1a64(remote, viewer.join.remote_length)
		    == cases[pass].remote_fnv);
		CHECK(viewer.join.local_row_count == cases[pass].local_rows
		    && viewer_rows_fnv1a64(&viewer.join)
		    == cases[pass].local_fnv
		    && viewer.join.local_fragment_length == 0U
		    && viewer.join.local_color_count == cases[pass].colors
		    && viewer_colors_fnv1a64(&viewer.join)
		    == cases[pass].color_fnv
		    && viewer_local_color_count(&viewer.join, 2, 0)
		    == cases[pass].color_2
		    && viewer_local_color_count(&viewer.join, 4, 0)
		    == cases[pass].color_4
		    && viewer_local_color_count(&viewer.join, 6, 0)
		    == cases[pass].color_6
		    && viewer_local_color_count(&viewer.join, 7, 0)
		    == cases[pass].color_7
		    && viewer_local_color_count(&viewer.join, 30, 4)
		    == cases[pass].color_30_4);
		CHECK(viewer.join.presentation.foreground
		    == cases[pass].final_foreground
		    && viewer.join.presentation.background == 0.0f
		    && viewer.join.presentation.bold == cases[pass].final_bold
		    && viewer.join.presentation.blink == cases[pass].final_blink
		    && viewer.join.pager.foreground == 1
		    && viewer.join.pager.line_count == 1.0f
		    && viewer.join.pager.nonstop == 1.0f
		    && viewer.join.pager.key[0] == '\0');
		CHECK(viewer.join.position == 19U
		    && stream.eof_checks == 20U && stream.key_checks == 20U
		    && stream.read_count == 19U && stream.line_count == 19U
		    && viewer.join.sample_calls == 25U
		    && viewer.join.response_calls == 0U
		    && viewer.join.direct_calls == 2U
		    && viewer.join.event_count == 125U
		    && !stream.file_open && !viewer.join.file_open
		    && viewer.input.file == NULL && viewer.close_calls == 2U
		    && viewer.open_calls == 1U
		    && viewer.join.source_length == sizeof(returning) - 1U
		    && memcmp(viewer.join.source, returning,
		    sizeof(returning) - 1U) == 0
		    && strcmp(viewer.join.accumulator, "Y") == 0
		    && viewer.join.queue_length == 0U);
		yt_text_input_destroy(&viewer.input);
	}
}

static bool
main_quit_accept_handoff_prefix(struct physical_viewer_join *viewer,
    bool ansi)
{
	static const uint8_t prompt[] =
	    "Time: 14:59  Main Command (?=Help)? ";
	static const uint8_t heading[] = "<Quit>";
	static const uint8_t confirmation[] = "Are you sure (Y/N)? ";
	static const uint8_t q[] = "Q";
	static const uint8_t y[] = "Y";
	struct viewer_pager_join *join = &viewer->join;
	struct yt_present_result result;

	join->presentation = state(ansi);
	join->presentation.foreground = 6.0f;
	join->presentation.cached_foreground = ansi ? 6.0f : 0.0f;
	join->pager.foreground = 6;
	join->pager.line_count = 0.0f;
	join->presentation.foreground = 2.0f;
	join->pager.foreground = 2;
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	if (!normal_exit_b05d(join, prompt, sizeof(prompt) - 1U, 1.0f))
		return false;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	memcpy(join->accumulator, q, sizeof(q));
	if (yt_present_editor_echo(q, sizeof(q) - 1U, q, sizeof(q) - 1U,
	    &join->presentation, &result) != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	join->presentation.foreground = 7.0f;
	join->pager.foreground = 7;
	if (!normal_exit_b05d(join, heading, sizeof(heading) - 1U, 0.0f))
		return false;
	if (yt_present_character(confirmation, sizeof(confirmation) - 1U,
	    &join->presentation, &result) != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	memcpy(join->accumulator, y, sizeof(y));
	if (yt_present_editor_echo(y, sizeof(y) - 1U, y, sizeof(y) - 1U,
	    &join->presentation, &result) != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	memcpy(join->source, y, sizeof(y));
	join->source_length = sizeof(y) - 1U;
	return true;
}

static void
test_main_quit_accept_handoff_presentation(void)
{
	static const struct {
		bool ansi;
		size_t remote_length;
		uint64_t remote_fnv;
		size_t colors;
		uint64_t color_fnv;
		size_t color_2;
		size_t color_7;
	} cases[] = {
		{true, 92U, UINT64_C(0x475c02557a9c6bd2), 8U,
		    UINT64_C(0x42a61ad6e51524c0), 3U, 5U},
		{false, 72U, UINT64_C(0x52b6bdb6ae1d3ceb), 2U,
		    UINT64_C(0x6d3fa4669b3587bd), 0U, 2U},
	};
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	uint8_t remote[128];
	size_t pass;

	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		memset(&viewer, 0, sizeof(viewer));
		fixture_viewer_initialize(&viewer, &stream,
		    retained_scoreboard, sizeof(retained_scoreboard) - 1U,
		    "YTSCORE.ASC", cases[pass].ansi, remote, sizeof(remote));
		CHECK(main_quit_accept_handoff_prefix(&viewer,
		    cases[pass].ansi));
		CHECK(viewer.join.remote_length == cases[pass].remote_length
		    && viewer_bytes_fnv1a64(remote, viewer.join.remote_length)
		    == cases[pass].remote_fnv
		    && viewer.join.local_row_count == 4U
		    && viewer_rows_fnv1a64(&viewer.join)
		    == UINT64_C(0x7bb9d88388d0695d)
		    && viewer.join.local_fragment_length == 0U
		    && viewer.join.local_color_count == cases[pass].colors
		    && viewer_colors_fnv1a64(&viewer.join)
		    == cases[pass].color_fnv
		    && viewer_local_color_count(&viewer.join, 2, 0)
		    == cases[pass].color_2
		    && viewer_local_color_count(&viewer.join, 7, 0)
		    == cases[pass].color_7);
		CHECK(viewer.join.presentation.foreground == 7.0f
		    && viewer.join.presentation.background == 0.0f
		    && viewer.join.presentation.bold == 0.0f
		    && viewer.join.presentation.blink == 0.0f
		    && viewer.join.presentation.cached_foreground
		    == (cases[pass].ansi ? 7.0f : 0.0f)
		    && viewer.join.pager.foreground == 7
		    && viewer.join.pager.line_count == 0.0f
		    && viewer.join.pager.nonstop == 0.0f
		    && viewer.join.pager.key[0] == '\0'
		    && strcmp(viewer.join.accumulator, "Y") == 0
		    && viewer.join.source_length == 1U
		    && viewer.join.source[0] == 'Y'
		    && viewer.join.queue_length == 0U);
		CHECK(viewer.join.sample_calls == 2U
		    && viewer.join.response_calls == 0U
		    && viewer.join.direct_calls == 0U
		    && viewer.join.event_count == 10U
		    && viewer.join.position == 0U
		    && stream.eof_checks == 0U && stream.key_checks == 0U
		    && stream.read_count == 0U && stream.line_count == 0U
		    && !stream.file_open && !viewer.join.file_open
		    && viewer.input.file == NULL && viewer.close_calls == 0U
		    && viewer.open_calls == 0U);
		yt_text_input_destroy(&viewer.input);
	}
}

static bool
quit_invalid_retry_typeahead_prefix(struct physical_viewer_join *viewer,
    bool ansi)
{
	static const uint8_t prompt[] =
	    "Time: 14:59  Computer command (?=help)? ";
	static const uint8_t heading[] = "<Quit>";
	static const uint8_t confirmation[] = "Are you sure (Y/N)? ";
	static const uint8_t command[] = "Q;X;Y";
	static const uint8_t x[] = "X";
	static const uint8_t y[] = "Y";
	struct viewer_pager_join *join = &viewer->join;
	struct yt_present_result result;
	struct yt_input_value selected;
	enum yt_yes_no_answer answer;
	char output[80];

	join->presentation = state(ansi);
	join->presentation.foreground = 6.0f;
	join->pager.foreground = 6;
	join->pager.line_count = 0.0f;
	if (ansi) {
		if (yt_present_color(&join->presentation, &result)
		    != YT_PRESENT_OK)
			return false;
	}
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	join->presentation.foreground = 1.0f;
	join->pager.foreground = 1;
	if (!normal_exit_b05d(join, prompt, sizeof(prompt) - 1U, 1.0f))
		return false;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	memcpy(join->accumulator, command, sizeof(command));
	if (!yt_input_split_semicolon(join->accumulator, join->queue,
	    sizeof(join->queue), &join->queue_position, &join->queue_length)
	    || strcmp(join->accumulator, "Q") != 0
	    || join->queue_position != 0U || join->queue_length != 4U
	    || memcmp(join->queue, "X\rY\r", 4U) != 0)
		return false;
	if (yt_present_editor_echo(command, sizeof(command) - 1U, command,
	    sizeof(command) - 1U, &join->presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	join->presentation.foreground = 7.0f;
	join->pager.foreground = 7;
	if (!normal_exit_b05d(join, heading, sizeof(heading) - 1U, 0.0f))
		return false;
	if (yt_present_character(confirmation, sizeof(confirmation) - 1U,
	    &join->presentation, &result) != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	if (!yt_input_ab36_queue_pop(join->queue, sizeof(join->queue),
	    &join->queue_position, &join->queue_length, &selected)
	    || selected.length != 1U || selected.bytes[0] != 'X')
		return false;
	memcpy(join->accumulator, x, sizeof(x));
	if (!yt_input_ab36_queue_pop(join->queue, sizeof(join->queue),
	    &join->queue_position, &join->queue_length, &selected)
	    || selected.length != 1U || selected.bytes[0] != '\r')
		return false;
	if (yt_present_editor_echo(x, sizeof(x) - 1U, x, sizeof(x) - 1U,
	    &join->presentation, &result) != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U)
	    || !yt_input_yes_no_candidate(join->accumulator, output,
	    sizeof(output), &answer) || answer != YT_YES_NO_INVALID)
		return false;
	join->presentation.bold = 1.0f;
	join->queue[0] = '\0';
	join->queue_position = 0U;
	join->queue_length = 0U;
	if (yt_present_character(confirmation, sizeof(confirmation) - 1U,
	    &join->presentation, &result) != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	memcpy(join->accumulator, y, sizeof(y));
	if (yt_present_editor_echo(y, sizeof(y) - 1U, y, sizeof(y) - 1U,
	    &join->presentation, &result) != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U)
	    || !yt_input_yes_no_candidate(join->accumulator, output,
	    sizeof(output), &answer) || answer != YT_YES_NO_YES
	    || strcmp(output, "Y") != 0)
		return false;
	memcpy(join->source, output, 2U);
	join->source_length = 1U;
	return true;
}

static void
test_quit_invalid_retry_typeahead_presentation(void)
{
	static const struct {
		bool ansi;
		size_t remote_length;
		uint64_t remote_fnv;
		size_t colors;
		uint64_t color_fnv;
		size_t color_3;
		size_t color_4;
		size_t color_7;
		size_t color_15;
		float final_bold;
	} cases[] = {
		{true, 145U, UINT64_C(0x131a4d06cc32e6ed), 10U,
		    UINT64_C(0x32a2c61f8d8dbee1), 1U, 2U, 6U, 1U, 0.0f},
		{false, 103U, UINT64_C(0xb9d09d4f6af5b9df), 2U,
		    UINT64_C(0x6d3fa4669b3587bd), 0U, 0U, 2U, 0U, 1.0f},
	};
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	uint8_t remote[192];
	size_t pass;

	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		memset(&viewer, 0, sizeof(viewer));
		fixture_viewer_initialize(&viewer, &stream,
		    retained_scoreboard, sizeof(retained_scoreboard) - 1U,
		    "YTSCORE.ASC", cases[pass].ansi, remote, sizeof(remote));
		CHECK(quit_invalid_retry_typeahead_prefix(&viewer,
		    cases[pass].ansi));
		CHECK(viewer.join.remote_length == cases[pass].remote_length
		    && viewer_bytes_fnv1a64(remote, viewer.join.remote_length)
		    == cases[pass].remote_fnv
		    && viewer.join.local_row_count == 5U
		    && viewer_rows_fnv1a64(&viewer.join)
		    == UINT64_C(0xecf4cb637fd673b9)
		    && viewer.join.local_fragment_length == 0U
		    && viewer.join.local_color_count == cases[pass].colors
		    && viewer_colors_fnv1a64(&viewer.join)
		    == cases[pass].color_fnv
		    && viewer_local_color_count(&viewer.join, 3, 0)
		    == cases[pass].color_3
		    && viewer_local_color_count(&viewer.join, 4, 0)
		    == cases[pass].color_4
		    && viewer_local_color_count(&viewer.join, 7, 0)
		    == cases[pass].color_7
		    && viewer_local_color_count(&viewer.join, 15, 0)
		    == cases[pass].color_15);
		CHECK(viewer.join.presentation.foreground == 7.0f
		    && viewer.join.presentation.background == 0.0f
		    && viewer.join.presentation.bold == cases[pass].final_bold
		    && viewer.join.presentation.blink == 0.0f
		    && viewer.join.presentation.cached_foreground
		    == (cases[pass].ansi ? 7.0f : 0.0f)
		    && viewer.join.pager.foreground == 7
		    && viewer.join.pager.line_count == 0.0f
		    && viewer.join.pager.nonstop == 0.0f
		    && viewer.join.pager.key[0] == '\0'
		    && strcmp(viewer.join.accumulator, "Y") == 0
		    && viewer.join.source_length == 1U
		    && viewer.join.source[0] == 'Y'
		    && viewer.join.queue_position == 0U
		    && viewer.join.queue_length == 0U
		    && viewer.join.queue[0] == '\0');
		CHECK(viewer.join.sample_calls == 2U
		    && viewer.join.response_calls == 0U
		    && viewer.join.direct_calls == 0U
		    && viewer.join.event_count == 10U
		    && viewer.join.position == 0U
		    && stream.eof_checks == 0U && stream.key_checks == 0U
		    && stream.read_count == 0U && stream.line_count == 0U
		    && !stream.file_open && !viewer.join.file_open
		    && viewer.input.file == NULL && viewer.close_calls == 0U
		    && viewer.open_calls == 0U);
		yt_text_input_destroy(&viewer.input);
	}
}

static bool
computer_quit_valid_typeahead_prefix(struct physical_viewer_join *viewer,
    bool ansi, const uint8_t *command, uint8_t typed_answer, bool confirmed)
{
	static const uint8_t prompt[] =
	    "Time: 14:59  Computer command (?=help)? ";
	static const uint8_t heading[] = "<Quit>";
	static const uint8_t confirmation[] = "Are you sure (Y/N)? ";
	static const uint8_t retained[] = "NEXT\r";
	const uint8_t answer_byte = confirmed ? 'Y' : 'N';
	struct viewer_pager_join *join = &viewer->join;
	struct yt_present_result result;
	struct yt_input_value selected;
	enum yt_yes_no_answer answer;
	char output[80];
	size_t command_length;

	if (command == NULL)
		return false;
	command_length = strlen((const char *)command);

	join->presentation = state(ansi);
	join->presentation.foreground = 6.0f;
	join->pager.foreground = 6;
	join->pager.line_count = 0.0f;
	if (ansi) {
		if (yt_present_color(&join->presentation, &result)
		    != YT_PRESENT_OK)
			return false;
	}
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	join->presentation.foreground = 1.0f;
	join->pager.foreground = 1;
	if (!normal_exit_b05d(join, prompt, sizeof(prompt) - 1U, 1.0f))
		return false;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	if (command_length >= sizeof(join->accumulator))
		return false;
	memcpy(join->accumulator, command, command_length + 1U);
	if (!yt_input_split_semicolon(join->accumulator, join->queue,
	    sizeof(join->queue), &join->queue_position, &join->queue_length)
	    || strcmp(join->accumulator, "Q") != 0
	    || join->queue_position != 0U
	    || join->queue_length != (typed_answer == 0U ? 6U : 7U)
	    || join->queue[0] != (char)(typed_answer == 0U
	    ? '\r' : typed_answer)
	    || memcmp(join->queue + 1U,
	    typed_answer == 0U ? "NEXT\r" : "\rNEXT\r",
	    typed_answer == 0U ? 5U : 6U) != 0)
		return false;
	if (yt_present_editor_echo(command, command_length, command,
	    command_length, &join->presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	join->presentation.foreground = 7.0f;
	join->pager.foreground = 7;
	if (!normal_exit_b05d(join, heading, sizeof(heading) - 1U, 0.0f))
		return false;
	if (yt_present_character(confirmation, sizeof(confirmation) - 1U,
	    &join->presentation, &result) != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	if (!yt_input_ab36_queue_pop(join->queue, sizeof(join->queue),
	    &join->queue_position, &join->queue_length, &selected)
	    || selected.length != 1U
	    || selected.bytes[0] != (typed_answer == 0U ? '\r' : typed_answer)
	    || selected.remote)
		return false;
	if (typed_answer != 0U) {
		join->accumulator[0] = (char)selected.bytes[0];
		join->accumulator[1] = '\0';
		if (!yt_input_ab36_queue_pop(join->queue, sizeof(join->queue),
		    &join->queue_position, &join->queue_length, &selected)
		    || selected.length != 1U || selected.bytes[0] != '\r'
		    || selected.remote)
			return false;
		if (yt_present_editor_echo(&typed_answer, 1U, &typed_answer, 1U,
		    &join->presentation, &result) != YT_PRESENT_OK)
			return false;
		viewer_pager_capture_result(join, &result);
	}
	if (!normal_exit_line(join, NULL, 0U)
	    || !yt_input_yes_no_candidate(join->accumulator, output,
	    sizeof(output), &answer)
	    || answer != (confirmed ? YT_YES_NO_YES
	    : (typed_answer == 0U ? YT_YES_NO_EMPTY : YT_YES_NO_NO))
	    || output[0] != (char)(typed_answer == 0U ? 0U : answer_byte)
	    || output[1] != '\0'
	    || join->queue_position != (typed_answer == 0U ? 1U : 2U)
	    || join->queue_length != (typed_answer == 0U ? 6U : 7U)
	    || memcmp(join->queue + join->queue_position, retained,
	    sizeof(retained) - 1U) != 0)
		return false;
	memcpy(join->source, output, 2U);
	join->source_length = typed_answer == 0U ? 0U : 1U;
	if (confirmed)
		return true;
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	join->presentation.foreground = 1.0f;
	join->pager.foreground = 1;
	return normal_exit_b05d(join, prompt, sizeof(prompt) - 1U, 1.0f);
}

static void
test_quit_valid_typeahead_presentation(void)
{
	static const uint8_t prompt[] =
	    "Time: 14:59  Computer command (?=help)? ";
	static const uint8_t upper_cancel[] = "Q;N;NEXT";
	static const uint8_t lower_cancel[] = "Q;n;NEXT";
	static const uint8_t blank_cancel[] = "Q;;NEXT";
	static const uint8_t upper_confirm[] = "Q;Y;NEXT";
	static const uint8_t lower_confirm[] = "Q;y;NEXT";
	static const struct {
		bool ansi;
		bool confirmed;
		const uint8_t *command;
		uint8_t typed_answer;
		const char *accumulator;
		size_t queue_position;
		size_t queue_length;
		size_t remote_length;
		uint64_t remote_fnv;
		size_t local_rows;
		uint64_t local_fnv;
		size_t colors;
		uint64_t color_fnv;
	} cases[] = {
		{true, false, upper_cancel, 'N', "N", 2U, 7U, 155U,
		    UINT64_C(0x38fbbcf4306f7d9a),
		    5U, UINT64_C(0xde94eff5c6423566), 11U,
		    UINT64_C(0x7001331ec067494d)},
		{false, false, upper_cancel, 'N', "N", 2U, 7U, 125U,
		    UINT64_C(0x3367e9c74106cdab),
		    5U, UINT64_C(0xde94eff5c6423566), 3U,
		    UINT64_C(0x2207a27a6260aaca)},
		{true, true, upper_confirm, 'Y', "Y", 2U, 7U, 103U,
		    UINT64_C(0x3e303d2c6ed01e21),
		    4U, UINT64_C(0xf04bbc982e5de59a), 8U,
		    UINT64_C(0xba58856d547852c1)},
		{false, true, upper_confirm, 'Y', "Y", 2U, 7U, 83U,
		    UINT64_C(0x04535d9c89803da3),
		    4U, UINT64_C(0xf04bbc982e5de59a), 2U,
		    UINT64_C(0x6d3fa4669b3587bd)},
		{true, false, lower_cancel, 'n', "n", 2U, 7U, 155U,
		    UINT64_C(0xe23d28b38f2ff81a), 5U,
		    UINT64_C(0xc948f84b83d84fe6), 11U,
		    UINT64_C(0x7001331ec067494d)},
		{false, false, lower_cancel, 'n', "n", 2U, 7U, 125U,
		    UINT64_C(0xdde08618f64fa7eb), 5U,
		    UINT64_C(0xc948f84b83d84fe6), 3U,
		    UINT64_C(0x2207a27a6260aaca)},
		{true, true, lower_confirm, 'y', "y", 2U, 7U, 103U,
		    UINT64_C(0xf503ce140fd6afa1), 4U,
		    UINT64_C(0x37f0ec1c9e67651a), 8U,
		    UINT64_C(0xba58856d547852c1)},
		{false, true, lower_confirm, 'y', "y", 2U, 7U, 83U,
		    UINT64_C(0x28fcfb8c7b6d0663), 4U,
		    UINT64_C(0x37f0ec1c9e67651a), 2U,
		    UINT64_C(0x6d3fa4669b3587bd)},
		{true, false, blank_cancel, 0U, "", 1U, 6U, 153U,
		    UINT64_C(0xf9f1b1c4d8691caa), 5U,
		    UINT64_C(0x5ec2ad45f9c3dc6e), 11U,
		    UINT64_C(0x7001331ec067494d)},
		{false, false, blank_cancel, 0U, "", 1U, 6U, 123U,
		    UINT64_C(0xc9da5ebd59a6af05), 5U,
		    UINT64_C(0x5ec2ad45f9c3dc6e), 3U,
		    UINT64_C(0x2207a27a6260aaca)},
	};
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	uint8_t remote[192];
	size_t pass;

	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		memset(&viewer, 0, sizeof(viewer));
		fixture_viewer_initialize(&viewer, &stream,
		    retained_scoreboard, sizeof(retained_scoreboard) - 1U,
		    "YTSCORE.ASC", cases[pass].ansi, remote, sizeof(remote));
		CHECK(computer_quit_valid_typeahead_prefix(&viewer,
		    cases[pass].ansi, cases[pass].command,
		    cases[pass].typed_answer, cases[pass].confirmed));
		CHECK(viewer.join.remote_length == cases[pass].remote_length
		    && viewer_bytes_fnv1a64(remote, viewer.join.remote_length)
		    == cases[pass].remote_fnv
		    && viewer.join.local_row_count == cases[pass].local_rows
		    && viewer_rows_fnv1a64(&viewer.join)
		    == cases[pass].local_fnv
		    && viewer.join.local_fragment_length
		    == (cases[pass].confirmed ? 0U : sizeof(prompt) - 1U)
		    && (cases[pass].confirmed
		    || memcmp(viewer.join.local_fragment, prompt,
		    viewer.join.local_fragment_length) == 0)
		    && viewer.join.local_color_count == cases[pass].colors
		    && viewer_colors_fnv1a64(&viewer.join)
		    == cases[pass].color_fnv
		    && strcmp(viewer.join.accumulator,
		    cases[pass].accumulator) == 0
		    && viewer.join.queue_position == cases[pass].queue_position
		    && viewer.join.queue_length == cases[pass].queue_length
		    && memcmp(viewer.join.queue + viewer.join.queue_position,
		    "NEXT\r", 5U) == 0
		    && viewer.join.pager.line_count
		    == (cases[pass].confirmed ? 0.0f : 1.0f)
		    && viewer.join.pager.foreground
		    == (cases[pass].confirmed ? 7 : 1)
		    && viewer.join.presentation.foreground
		    == (cases[pass].confirmed ? 7.0f : 1.0f)
		    && viewer.join.presentation.cached_foreground
		    == (cases[pass].ansi
		    ? (cases[pass].confirmed ? 7.0f : 1.0f) : 0.0f)
		    && viewer.join.presentation.bold == 0.0f
		    && viewer.join.presentation.blink == 0.0f
		    && viewer.join.pager.nonstop == 0.0f
		    && viewer.join.pager.key[0] == '\0');
		CHECK(viewer.join.sample_calls
		    == (cases[pass].confirmed ? 2U : 3U)
		    && viewer.join.response_calls == 0U
		    && viewer.join.direct_calls == 0U
		    && viewer.join.event_count
		    == (cases[pass].confirmed ? 10U : 15U)
		    && viewer.join.position == 0U
		    && stream.eof_checks == 0U && stream.key_checks == 0U
		    && stream.read_count == 0U && stream.line_count == 0U
		    && !stream.file_open && !viewer.join.file_open
		    && viewer.input.file == NULL && viewer.close_calls == 0U
		    && viewer.open_calls == 0U);
		CHECK(viewer.join.source_length
		    == (cases[pass].confirmed ? 1U : sizeof(prompt) - 1U)
		    && memcmp(viewer.join.source,
		    cases[pass].confirmed ? (const uint8_t *)"Y" : prompt,
		    viewer.join.source_length) == 0);
		yt_text_input_destroy(&viewer.input);
	}
}

static bool
computer_quit_heading_sample_blank_prefix(
    struct physical_viewer_join *viewer, bool ansi, bool remote_source)
{
	static const uint8_t prompt[] =
	    "Time: 14:59  Computer command (?=help)? ";
	static const uint8_t heading[] = "<Quit>";
	static const uint8_t confirmation[] = "Are you sure (Y/N)? ";
	static const uint8_t command[] = "Q";
	struct viewer_pager_join *join = &viewer->join;
	struct yt_input_splitter splitter;
	struct yt_input_value incoming;
	struct yt_input_value selected;
	struct yt_present_result result;
	enum yt_yes_no_answer answer;
	char output[80];

	memset(&incoming, 0, sizeof(incoming));
	incoming.bytes[0] = '\r';
	incoming.length = 1U;
	yt_input_splitter_init(&splitter);
	if (!yt_input_splitter_push(&splitter, remote_source, &incoming))
		return false;
	selected = yt_input_splitter_select_source(&splitter, remote_source);
	if (selected.length != 1U || selected.bytes[0] != '\r'
	    || selected.remote != remote_source)
		return false;
	join->injected_sample = selected;
	join->injected_sample_call = 2U;

	join->presentation = state(ansi);
	join->presentation.foreground = 6.0f;
	join->pager.foreground = 6;
	join->pager.line_count = 0.0f;
	if (ansi) {
		if (yt_present_color(&join->presentation, &result)
		    != YT_PRESENT_OK)
			return false;
	}
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	join->presentation.foreground = 1.0f;
	join->pager.foreground = 1;
	if (!normal_exit_b05d(join, prompt, sizeof(prompt) - 1U, 1.0f))
		return false;
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	memcpy(join->accumulator, command, sizeof(command));
	if (yt_present_editor_echo(command, sizeof(command) - 1U, command,
	    sizeof(command) - 1U, &join->presentation, &result)
	    != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	join->presentation.foreground = 7.0f;
	join->pager.foreground = 7;
	if (!normal_exit_b05d(join, heading, sizeof(heading) - 1U, 0.0f)
	    || join->injected_sample_hits != 1U
	    || join->queue_position != 0U || join->queue_length != 1U
	    || join->queue[0] != '\r')
		return false;
	if (yt_present_character(confirmation, sizeof(confirmation) - 1U,
	    &join->presentation, &result) != YT_PRESENT_OK)
		return false;
	viewer_pager_capture_result(join, &result);
	yt_pager_editor_enter(&join->pager, join->accumulator,
	    sizeof(join->accumulator));
	if (!yt_input_ab36_queue_pop(join->queue, sizeof(join->queue),
	    &join->queue_position, &join->queue_length, &selected)
	    || selected.length != 1U || selected.bytes[0] != '\r'
	    || selected.remote || join->queue_position != 0U
	    || join->queue_length != 0U || join->queue[0] != '\0')
		return false;
	if (!normal_exit_line(join, NULL, 0U)
	    || !yt_input_yes_no_candidate(join->accumulator, output,
	    sizeof(output), &answer) || answer != YT_YES_NO_EMPTY
	    || output[0] != '\0')
		return false;
	join->source[0] = '\0';
	join->source_length = 0U;
	if (!normal_exit_line(join, NULL, 0U))
		return false;
	join->presentation.foreground = 1.0f;
	join->pager.foreground = 1;
	return normal_exit_b05d(join, prompt, sizeof(prompt) - 1U, 1.0f);
}

static void
test_quit_heading_sample_typeahead_presentation(void)
{
	static const uint8_t prompt[] =
	    "Time: 14:59  Computer command (?=help)? ";
	static const struct {
		bool ansi;
		bool remote_source;
		size_t remote_length;
		uint64_t remote_fnv;
		size_t colors;
		uint64_t color_fnv;
	} cases[] = {
		{true, false, 147U, UINT64_C(0x006372f5a873a37d),
		    11U, UINT64_C(0x7001331ec067494d)},
		{true, true, 147U, UINT64_C(0x006372f5a873a37d),
		    11U, UINT64_C(0x7001331ec067494d)},
		{false, false, 117U, UINT64_C(0x696d9dc089124cd2),
		    3U, UINT64_C(0x2207a27a6260aaca)},
		{false, true, 117U, UINT64_C(0x696d9dc089124cd2),
		    3U, UINT64_C(0x2207a27a6260aaca)},
	};
	struct physical_viewer_join viewer;
	struct yt_file_viewer_stream_state stream;
	uint8_t remote[192];
	size_t pass;

	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		memset(&viewer, 0, sizeof(viewer));
		fixture_viewer_initialize(&viewer, &stream,
		    retained_scoreboard, sizeof(retained_scoreboard) - 1U,
		    "YTSCORE.ASC", cases[pass].ansi, remote, sizeof(remote));
		CHECK(computer_quit_heading_sample_blank_prefix(&viewer,
		    cases[pass].ansi, cases[pass].remote_source));
		CHECK(viewer.join.remote_length == cases[pass].remote_length
		    && viewer_bytes_fnv1a64(remote, viewer.join.remote_length)
		    == cases[pass].remote_fnv
		    && viewer.join.local_row_count == 5U
		    && viewer_rows_fnv1a64(&viewer.join)
		    == UINT64_C(0x2b32ca4015639477)
		    && viewer.join.local_fragment_length == sizeof(prompt) - 1U
		    && memcmp(viewer.join.local_fragment, prompt,
		    sizeof(prompt) - 1U) == 0
		    && viewer.join.local_color_count == cases[pass].colors
		    && viewer_colors_fnv1a64(&viewer.join)
		    == cases[pass].color_fnv
		    && viewer.join.injected_sample_hits == 1U
		    && viewer.join.injected_sample.remote
		    == cases[pass].remote_source
		    && viewer.join.queue_position == 0U
		    && viewer.join.queue_length == 0U
		    && viewer.join.queue[0] == '\0'
		    && viewer.join.accumulator[0] == '\0'
		    && viewer.join.presentation.foreground == 1.0f
		    && viewer.join.presentation.cached_foreground
		    == (cases[pass].ansi ? 1.0f : 0.0f)
		    && viewer.join.presentation.bold == 0.0f
		    && viewer.join.presentation.blink == 0.0f
		    && viewer.join.pager.foreground == 1
		    && viewer.join.pager.line_count == 1.0f
		    && viewer.join.pager.nonstop == 0.0f
		    && viewer.join.pager.key[0] == '\0'
		    && viewer.join.source_length == sizeof(prompt) - 1U
		    && memcmp(viewer.join.source, prompt,
		    sizeof(prompt) - 1U) == 0);
		CHECK(viewer.join.sample_calls == 3U
		    && viewer.join.response_calls == 0U
		    && viewer.join.direct_calls == 0U
		    && viewer.join.event_count == 15U
		    && viewer.join.position == 0U
		    && stream.eof_checks == 0U && stream.key_checks == 0U
		    && stream.read_count == 0U && stream.line_count == 0U
		    && !stream.file_open && !viewer.join.file_open
		    && viewer.input.file == NULL && viewer.close_calls == 0U
		    && viewer.open_calls == 0U);
		yt_text_input_destroy(&viewer.input);
	}
}

struct spy_sweep_presentation_context {
	struct yt_present_state current;
	struct pager_capture capture;
	struct yt_sector sector;
	struct yt_planet planet;
	struct yt_player players[5];
	struct yt_sector team;
	float draws[2];
	size_t draw_position;
};

static bool
spy_sweep_presentation_sector(void *context, int logical,
    struct yt_sector *sector, struct yt_error *error)
{
	struct spy_sweep_presentation_context *fixture = context;

	(void)logical;
	(void)error;
	*sector = fixture->sector;
	return true;
}

static bool
spy_sweep_presentation_update(void *context, float link,
    struct yt_error *error)
{
	(void)context;
	(void)link;
	(void)error;
	return true;
}

static bool
spy_sweep_presentation_planet(void *context, float link,
    struct yt_planet *planet, struct yt_error *error)
{
	struct spy_sweep_presentation_context *fixture = context;

	(void)link;
	(void)error;
	*planet = fixture->planet;
	return true;
}

static bool
spy_sweep_presentation_player(void *context, float record,
    struct yt_player *player, struct yt_error *error)
{
	struct spy_sweep_presentation_context *fixture = context;
	int index = (int)record;

	(void)error;
	if (index < 0 || (size_t)index >= YT_ARRAY_LEN(fixture->players))
		return false;
	*player = fixture->players[index];
	return true;
}

static bool
spy_sweep_presentation_team(void *context, float team,
    struct yt_sector *overlay, struct yt_error *error)
{
	struct spy_sweep_presentation_context *fixture = context;

	(void)team;
	(void)error;
	*overlay = fixture->team;
	return true;
}

static bool
spy_sweep_presentation_random(void *context, float *value,
    struct yt_error *error)
{
	struct spy_sweep_presentation_context *fixture = context;

	(void)error;
	if (fixture->draw_position >= YT_ARRAY_LEN(fixture->draws))
		return false;
	*value = fixture->draws[fixture->draw_position++];
	return true;
}

static void
spy_sweep_presentation_import(struct spy_sweep_presentation_context *fixture,
    const struct yt_spy_sweep_state *state)
{
	fixture->current.foreground = state->foreground;
	fixture->current.background = state->background;
	fixture->current.bold = state->bold;
	fixture->current.blink = state->blink;
}

static void
spy_sweep_presentation_export(struct yt_spy_sweep_state *state,
    const struct spy_sweep_presentation_context *fixture)
{
	state->foreground = fixture->current.foreground;
	state->background = fixture->current.background;
	state->bold = fixture->current.bold;
	state->blink = fixture->current.blink;
}

static bool
spy_sweep_presentation_sound(void *context, float selector,
    struct yt_error *error)
{
	struct spy_sweep_presentation_context *fixture = context;
	struct yt_present_result result;

	(void)error;
	if (yt_present_sound(selector, &fixture->current, &result)
	    != YT_PRESENT_OK)
		return false;
	pager_capture_result(&fixture->capture, &result);
	return true;
}

static bool
spy_sweep_presentation_present(void *context, const uint8_t *text,
    size_t length, enum yt_spy_output_kind kind,
    struct yt_spy_sweep_state *state, struct yt_error *error)
{
	struct spy_sweep_presentation_context *fixture = context;
	struct yt_present_result result;
	enum yt_present_status status;

	(void)error;
	spy_sweep_presentation_import(fixture, state);
	switch (kind) {
	case YT_SPY_LINE:
		status = yt_present_line(text, length, &fixture->current, &result);
		break;
	case YT_SPY_BOLD_LINE:
		status = yt_present_bold_line(text, length, &fixture->current,
		    &result);
		break;
	case YT_SPY_BOLD_RAW:
		status = yt_present_bold_character(text, length, &fixture->current,
		    &result);
		break;
	case YT_SPY_ATTENTION:
		status = yt_present_attention(text, length, &fixture->current,
		    &result);
		break;
	default:
		return false;
	}
	spy_sweep_presentation_export(state, fixture);
	if (status != YT_PRESENT_OK)
		return false;
	pager_capture_result(&fixture->capture, &result);
	return true;
}

static bool
spy_sweep_presentation_pause(void *context,
    struct yt_spy_sweep_state *state, struct yt_error *error)
{
	struct spy_sweep_presentation_context *fixture = context;
	struct yt_present_result result;
	float saved;

	(void)error;
	spy_sweep_presentation_import(fixture, state);
	if (yt_present_press_prompt(&fixture->current, &result, &saved)
	    != YT_PRESENT_OK)
		return false;
	pager_capture_result(&fixture->capture, &result);
	if (yt_present_press_cleanup(saved, &fixture->current, &result)
	    != YT_PRESENT_OK)
		return false;
	pager_capture_result(&fixture->capture, &result);
	spy_sweep_presentation_export(state, fixture);
	return true;
}

static struct spy_sweep_presentation_context
spy_sweep_presentation_fixture(bool ansi)
{
	static const struct yt_spy_sweep_ops ops = {
		spy_sweep_presentation_sector,
		spy_sweep_presentation_update,
		spy_sweep_presentation_planet,
		spy_sweep_presentation_player,
		spy_sweep_presentation_team,
		spy_sweep_presentation_random,
		spy_sweep_presentation_sound,
		spy_sweep_presentation_present,
		spy_sweep_presentation_pause,
		NULL,
		NULL,
		NULL,
	};
	struct spy_sweep_presentation_context fixture;
	struct yt_spy_sweep_state sweep;
	int sectors[3] = {100, 0, 0};
	int markers[3] = {0, 0, 0};
	float sector_cache[52] = {0};
	float cloak_cache[52] = {0};

	memset(&fixture, 0, sizeof(fixture));
	memset(&sweep, 0, sizeof(sweep));
	fixture.current = state(ansi);
	fixture.current.foreground = 5.0f;
	fixture.current.sound.user_sound = 0.0f;
	fixture.current.sound.local_sound = 0.0f;
	fixture.sector.mines = 5.0f;
	fixture.sector.planet = 1.0f;
	fixture.sector.fighters = 8.0f;
	fixture.sector.fighter_owner = 4.0f;
	fixture.sector.warps[1] = 200.0f;
	memcpy(fixture.planet.record.bytes, "Gaia", 4U);
	fixture.planet.name_length = 4.0f;
	fixture.planet.ground_forces = 9.0f;
	memcpy(fixture.players[3].record.bytes, "Ada", 3U);
	fixture.players[3].name_length = 3.0f;
	fixture.players[3].team = 7.0f;
	fixture.players[3].fighters = 12.0f;
	fixture.players[3].shields = 34.0f;
	memcpy(fixture.players[4].record.bytes, "Grace", 5U);
	fixture.players[4].name_length = 5.0f;
	fixture.players[4].team = 2.0f;
	memcpy(fixture.team.record.bytes, "Union", 5U);
	CHECK(yt_record_set_number(&fixture.team.record, YT_F73, 5.0f));
	fixture.draws[0] = 0.75f;
	fixture.draws[1] = 0.2f;
	sector_cache[3] = 100.0f;
	cloak_cache[3] = 0.5f;
	sweep.active_spies = 1.0f;
	sweep.spy_sectors = sectors;
	sweep.last_reported_sectors = markers;
	sweep.spy_capacity = 3U;
	sweep.current_player_record = 2;
	sweep.last_player_record = 51.0f;
	sweep.sector_cache = sector_cache;
	sweep.cloak_cache = cloak_cache;
	sweep.cache_count = 52U;
	sweep.foreground = 5.0f;
	CHECK(yt_spy_sweep_run(&sweep, &ops, &fixture, NULL));
	fixture.current.foreground = sweep.foreground;
	fixture.current.background = sweep.background;
	fixture.current.bold = sweep.bold;
	fixture.current.blink = sweep.blink;
	return fixture;
}

static void
test_spy_sweep_presentation(void)
{
	struct spy_sweep_presentation_context plain =
	    spy_sweep_presentation_fixture(false);
	struct spy_sweep_presentation_context ansi =
	    spy_sweep_presentation_fixture(true);

	CHECK(plain.capture.remote_length == 363U
	    && presentation_fnv1a64(plain.capture.remote,
	    plain.capture.remote_length) == UINT64_C(0x94e7f43d9a999917));
	CHECK(ansi.capture.remote_length == 553U
	    && presentation_fnv1a64(ansi.capture.remote,
	    ansi.capture.remote_length) == UINT64_C(0x2ffc3b954d679489));
	CHECK(plain.current.foreground == 0.0f
	    && ansi.current.foreground == 0.0f
	    && ansi.current.background == 0.0f
	    && ansi.current.bold == 0.0f && ansi.current.blink == 0.0f);
}

static void
test_black_hole_presentation(void)
{
	static const uint8_t plain[] =
	    "\r\nA *-BLACK HOLE-* grabs you!\r\n"
	    "\r\n * EMERGENCY WARP ENGAGED! * \r\n"
	    "\r\nYou enter a wormhole as your engines build up to emergency power!\r\n"
	    "\r\n     * Engine Temperature *\r\n"
	    "[ Normal ][ Danger ][ Overheat ]\r\n"
	    "================================\r\n"
	    "[*\r\n\r\n"
	    "You sigh in relief as you look at your scanner and find yourself in\r\n"
	    "sector 1003. However, it takes you 3 turns to recharge your engines!\r\n";
	static const uint8_t ansi[] =
	    "\r\n"
	    "\x1b[0;33;41;5;1mA *-BLACK HOLE-* grabs you!"
	    "\x1b[0;33;40m\r\n"
	    "\r\n"
	    "\x1b[0;33;41;5;1m * EMERGENCY WARP ENGAGED! * "
	    "\x1b[0;33;40m\r\n"
	    "\r\n"
	    "\x1b[0;33;40;1mYou enter a wormhole as your engines build up to emergency power!\r\n"
	    "\x1b[0;33;40m\r\n"
	    "\x1b[0;36;40;1m     * Engine Temperature *\r\n"
	    "\x1b[0;36;40;1m[ Normal ][ Danger ][ Overheat ]\r\n"
	    "\x1b[0;32;40;1m================================\r\n"
	    "\x1b[0;36;40;1m[\x1b[0;32;40;1m*"
	    "\x1b[0;32;40m\r\n"
	    "\r\n"
	    "You sigh in relief as you look at your scanner and find yourself in\r\n"
	    "sector 1003. However, it takes you 3 turns to recharge your engines!\r\n";
	struct pager_capture capture;

	capture = black_hole_fixture(false, false);
	CHECK(capture.remote_length == sizeof(plain) - 1U
	    && memcmp(capture.remote, plain, sizeof(plain) - 1U) == 0);
	CHECK(capture.remote_length == 377U);
	capture = black_hole_fixture(true, false);
	CHECK(capture.remote_length == sizeof(ansi) - 1U
	    && memcmp(capture.remote, ansi, sizeof(ansi) - 1U) == 0);
	CHECK(capture.remote_length == 517U);
	capture = black_hole_fixture(false, true);
	CHECK(capture.remote_length == 390U
	    && presentation_fnv1a64(capture.remote, capture.remote_length)
	    == UINT64_C(0x8094673d4189baa8));
	capture = black_hole_fixture(true, true);
	CHECK(capture.remote_length == 992U
	    && presentation_fnv1a64(capture.remote, capture.remote_length)
	    == UINT64_C(0xe5d4a68593eb10a8));
}

static void
test_movement_presentation(void)
{
	static const uint8_t destination_prompt[] = "Move to which sector? ";
	static const uint8_t finalizer_row[] = "One Turn Deducted, 59 left.";
	static const uint8_t expected[] =
	    "\r\n"
	    "Warps lead to, 7, 42, 12.5\n\r"
	    "\r\n"
	    "Move to which sector? 42\r\n"
	    "\r\n"
	    "One Turn Deducted, 59 left.\n\r";
	const float warps[6] = {7.0f, 42.0f, 0.0f, 0.0f, 12.5f, 0.0f};
	struct yt_present_state current = state(false);
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	char accumulator[80] = "M";
	uint8_t row[128];
	size_t row_length;

	current.foreground = 2.0f;
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 2;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_movement_warp_row(warps, row, sizeof(row), &row_length));
	CHECK(yt_present_line(NULL, 0U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current, row, row_length, &capture);
	CHECK(yt_present_line(NULL, 0U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, destination_prompt,
	    sizeof(destination_prompt) - 1U, &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo((const uint8_t *)"42", 2U,
	    (const uint8_t *)"42", 2U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current, finalizer_row,
	    sizeof(finalizer_row) - 1U, &capture);
	CHECK(capture.remote_length == sizeof(expected) - 1U
	    && memcmp(capture.remote, expected, sizeof(expected) - 1U) == 0);
	CHECK(capture.remote_length == 89U && pager.line_count == 1.0f);
}

static void
test_direct_attack_presentation(void)
{
	static const uint8_t title[] = "<Attack>";
	static const uint8_t eliminated[] =
	    "Fighters eliminated! Attacking the ship!";
	static const uint8_t expected[] =
	    "<Attack>\n\r"
	    "Attack VICTIM (Y/N)[Y]? Y\r\n"
	    "You have 5. Use how many fighters? [0] 3\r\n"
	    "\r\nYou lost 0 fighter(s), 2 remain.\n\r"
	    "You destroyed 2 enemy fighters, 0 remain.\n\r"
	    "\r\nFighters eliminated! Attacking the ship!\n\r";
	struct yt_present_state current = state(false);
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	char accumulator[80] = "";
	uint8_t prompt[160];
	uint8_t attacker_row[160];
	uint8_t defender_row[160];
	size_t prompt_length;
	size_t attacker_length;
	size_t defender_length;

	memset(&pager, 0, sizeof(pager));
	pager.foreground = 2;
	memset(&capture, 0, sizeof(capture));
	pager_fixture_b05d(&pager, &current, title, sizeof(title) - 1U,
	    &capture);
	CHECK(yt_direct_attack_candidate_prompt((const uint8_t *)"VICTIM",
	    6U, prompt, sizeof(prompt), &prompt_length));
	CHECK(yt_present_character(prompt, prompt_length, &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo((const uint8_t *)"Y", 1U,
	    (const uint8_t *)"Y", 1U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_capture_line(&capture, &current, NULL, 0U);
	CHECK(yt_direct_attack_commitment_prompt(5.0, prompt,
	    sizeof(prompt), &prompt_length));
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, prompt, prompt_length, &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo((const uint8_t *)"3", 1U,
	    (const uint8_t *)"3", 1U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_capture_line(&capture, &current, NULL, 0U);
	CHECK(yt_direct_attack_result_rows(0.0, 2.0, 2.0, 0.0,
	    attacker_row, sizeof(attacker_row), &attacker_length,
	    defender_row, sizeof(defender_row), &defender_length));
	pager_capture_line(&capture, &current, NULL, 0U);
	pager_fixture_b05d(&pager, &current, attacker_row, attacker_length,
	    &capture);
	pager_fixture_b05d(&pager, &current, defender_row, defender_length,
	    &capture);
	pager_capture_line(&capture, &current, NULL, 0U);
	pager_fixture_b05d(&pager, &current, eliminated,
	    sizeof(eliminated) - 1U, &capture);
	CHECK(capture.remote_length == sizeof(expected) - 1U
	    && memcmp(capture.remote, expected, sizeof(expected) - 1U) == 0);
}

static void
spy_cycle_fixture(bool ansi, int active_count, struct pager_capture *capture,
    struct yt_present_state *current, struct yt_pager_state *pager)
{
	static const uint8_t prompt[] =
	    "Time: 14:59  Computer command (?=help)? ";
	static const uint8_t rows[2][39] = {
	    "Spy # 1 will hunt in sector 42.",
	    "Spy # 2 will hunt in sector-7."
	};
	static const uint8_t none[] = "You do not have any spies!";
	struct yt_present_result result;
	char accumulator[80] = "";
	int index;

	*current = state(ansi);
	current->foreground = 6.0f;
	if (ansi)
		CHECK(yt_present_color(current, &result) == YT_PRESENT_OK);
	memset(pager, 0, sizeof(*pager));
	pager->foreground = 6;
	memset(capture, 0, sizeof(*capture));
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	current->foreground = 1.0f;
	pager->foreground = 1;
	pager->newline_flag = 1.0f;
	pager_fixture_b05d(pager, current, prompt, sizeof(prompt) - 1U,
	    capture);
	yt_pager_editor_enter(pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo((const uint8_t *)"15", 2,
	    (const uint8_t *)"15", 2, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);

	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	if (active_count == 0) {
		current->bold = 1.0f;
		current->blink = 1.0f;
		pager->newline_flag = 0.0f;
		pager_fixture_b05d(pager, current, none, sizeof(none) - 1U,
		    capture);
	}
	else {
		for (index = 0; index < active_count; ++index) {
			current->bold = 1.0f;
			pager->newline_flag = 0.0f;
			pager_fixture_b05d(pager, current, rows[index],
			    strlen((const char *)rows[index]), capture);
		}
	}

	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	current->foreground = 1.0f;
	pager->foreground = 1;
	pager->newline_flag = 1.0f;
	pager_fixture_b05d(pager, current, prompt, sizeof(prompt) - 1U,
	    capture);
}

static void
test_computer_spy_presentation(void)
{
	static const uint8_t active_ansi[] =
	    "\r\n\x1b[0;31;40mTime: 14:59  Computer command (?=help)? 15\r\n"
	    "\r\n\x1b[0;31;40;1mSpy # 1 will hunt in sector 42.\n\r"
	    "\x1b[0;31;40;1mSpy # 2 will hunt in sector-7.\n\r"
	    "\x1b[0;31;40m\r\nTime: 14:59  Computer command (?=help)? ";
	static const uint8_t active_plain[] =
	    "\r\nTime: 14:59  Computer command (?=help)? 15\r\n"
	    "\r\nSpy # 1 will hunt in sector 42.\n\r"
	    "Spy # 2 will hunt in sector-7.\n\r"
	    "\r\nTime: 14:59  Computer command (?=help)? ";
	static const uint8_t zero_ansi[] =
	    "\r\n\x1b[0;31;40mTime: 14:59  Computer command (?=help)? 15\r\n"
	    "\r\n\x1b[0;31;40;5;1mYou do not have any spies!\n\r"
	    "\x1b[0;31;40m\r\nTime: 14:59  Computer command (?=help)? ";
	struct yt_present_state current;
	struct yt_pager_state pager;
	struct pager_capture capture;

	spy_cycle_fixture(true, 2, &capture, &current, &pager);
	CHECK(sizeof(active_ansi) - 1U == 199U);
	CHECK(capture.remote_length == sizeof(active_ansi) - 1U
	    && memcmp(capture.remote, active_ansi,
	    sizeof(active_ansi) - 1U) == 0);
	CHECK(pager.line_count == 3.0f && pager.newline_flag == 0.0f);

	spy_cycle_fixture(false, 2, &capture, &current, &pager);
	CHECK(sizeof(active_plain) - 1U == 155U);
	CHECK(capture.remote_length == sizeof(active_plain) - 1U
	    && memcmp(capture.remote, active_plain,
	    sizeof(active_plain) - 1U) == 0);
	CHECK(pager.line_count == 3.0f && current.bold == 1.0f);

	spy_cycle_fixture(true, 0, &capture, &current, &pager);
	CHECK(sizeof(zero_ansi) - 1U == 152U);
	CHECK(capture.remote_length == sizeof(zero_ansi) - 1U
	    && memcmp(capture.remote, zero_ansi,
	    sizeof(zero_ansi) - 1U) == 0);
	CHECK(pager.line_count == 2.0f && current.bold == 0.0f
	    && current.blink == 0.0f);
}

static void
test_computer_path_presentation(void)
{
	static const uint8_t start_prompt[] =
	    "Enter start for path search? ";
	static const uint8_t destination_prompt[] =
	    "What sector do you want to go to? ";
	static const uint8_t working[] = "Working. ";
	static const uint8_t heading[] =
	    "The shortest path from sector 1 to sector 2 is:";
	static const uint8_t one[] = " 1";
	static const uint8_t two[] = " 2";
	static const uint8_t course[] = "Course will take 1 turns.";
	static const uint8_t expected[] =
	    "\r\nEnter start for path search? 1\r\n"
	    "\r\nWhat sector do you want to go to? 2\r\n"
	    "\r\nWorking. The shortest path from sector 1 to sector 2 is:\n\r"
	    "\r\n 1 2\r\n"
	    "\r\nCourse will take 1 turns.\n\r";
	struct yt_present_state current = state(false);
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	char accumulator[80] = "";

	current.foreground = 1.0f;
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 1;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, start_prompt,
	    sizeof(start_prompt) - 1U, &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo((const uint8_t *)"1", 1,
	    (const uint8_t *)"1", 1, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, destination_prompt,
	    sizeof(destination_prompt) - 1U, &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo((const uint8_t *)"2", 1,
	    (const uint8_t *)"2", 1, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, working, sizeof(working) - 1U,
	    &capture);
	pager_fixture_b05d(&pager, &current, heading, sizeof(heading) - 1U,
	    &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, one, sizeof(one) - 1U,
	    &capture);
	pager.line_count = 0.0f;
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, two, sizeof(two) - 1U,
	    &capture);
	pager.line_count = 0.0f;
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current, course, sizeof(course) - 1U,
	    &capture);
	CHECK(sizeof(expected) - 1U == 170U);
	CHECK(capture.remote_length == sizeof(expected) - 1U
	    && memcmp(capture.remote, expected, sizeof(expected) - 1U) == 0);
	CHECK(pager.line_count == 1.0f && pager.newline_flag == 0.0f);
}

static void
test_computer_path_start_terminal_presentation(void)
{
	static const uint8_t prompt[] = "Enter start for path search? ";
	static const uint8_t inactivity[] =
	    "\r\nEnter start for path search? "
	    "\r\n\aUSER FELL ASLEEP!\n\r";
	static const uint8_t inactivity_carrier[] =
	    "\r\nEnter start for path search? \r\n";
	static const uint8_t direct_carrier[] =
	    "\r\nEnter start for path search? ";
	static const uint8_t session_limit[] =
	    "\r\nEnter start for path search? "
	    "\r\n\a\a\aTIME LIMIT EXCEEDED!\a\a\a\n\r";
	static const uint8_t session_carrier[] =
	    "\r\nEnter start for path search? "
	    "\r\n\a\a\aTIME LIMIT EXCEEDED!\a\a\a";
	static const struct {
		enum yt_ab36_terminal_kind kind;
		int carrier_failure;
		const uint8_t *expected;
		size_t expected_length;
		bool succeeds;
	} cases[] = {
		{YT_AB36_TERMINAL_INACTIVITY, 0, inactivity,
		    sizeof(inactivity) - 1U, true},
		{YT_AB36_TERMINAL_INACTIVITY, 1, inactivity_carrier,
		    sizeof(inactivity_carrier) - 1U, false},
		{YT_AB36_TERMINAL_SESSION_LIMIT, 0, session_limit,
		    sizeof(session_limit) - 1U, true},
		{YT_AB36_TERMINAL_SESSION_LIMIT, 2, session_carrier,
		    sizeof(session_carrier) - 1U, false},
	};
	size_t pass;

	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		struct yt_present_state current = state(true);
		struct yt_present_result result;
		struct yt_pager_state pager;
		struct pager_capture capture;
		struct computer_port_terminal_join join;
		char accumulator[80];
		bool running = true;
		bool terminated = false;
		bool result_ok;

		current.foreground = 1.0f;
		current.cached_foreground = 1.0f;
		memset(&pager, 0, sizeof(pager));
		pager.foreground = 1;
		memset(&capture, 0, sizeof(capture));
		memset(accumulator, 0, sizeof(accumulator));
		CHECK(yt_present_line(NULL, 0U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		pager.newline_flag = 1.0f;
		pager_fixture_b05d(&pager, &current, prompt,
		    sizeof(prompt) - 1U, &capture);
		yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
		join.current = &current;
		join.pager = &pager;
		join.capture = &capture;
		join.running = &running;
		join.terminated = &terminated;
		join.notice_carrier_failure = cases[pass].carrier_failure;
		join.closed = false;
		result_ok = yt_input_ab36_terminal_run(cases[pass].kind,
		    &running, &terminated, computer_port_terminal_notice,
		    computer_port_terminal_close, &join);
		CHECK(result_ok == cases[pass].succeeds
		    && capture.remote_length == cases[pass].expected_length
		    && memcmp(capture.remote, cases[pass].expected,
		    cases[pass].expected_length) == 0
		    && !running && terminated && join.closed
		    && pager.line_count == (cases[pass].succeeds ? 1.0f : 0.0f)
		    && pager.newline_flag == 0.0f && accumulator[0] == '\0');
	}
	{
		struct yt_present_state current = state(true);
		struct yt_present_result result;
		struct yt_pager_state pager;
		struct pager_capture capture;
		char accumulator[80];

		current.foreground = 1.0f;
		current.cached_foreground = 1.0f;
		memset(&pager, 0, sizeof(pager));
		pager.foreground = 1;
		memset(&capture, 0, sizeof(capture));
		memset(accumulator, 0, sizeof(accumulator));
		CHECK(yt_present_line(NULL, 0U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		pager.newline_flag = 1.0f;
		pager_fixture_b05d(&pager, &current, prompt,
		    sizeof(prompt) - 1U, &capture);
		yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
		CHECK(capture.remote_length == sizeof(direct_carrier) - 1U
		    && memcmp(capture.remote, direct_carrier,
		    sizeof(direct_carrier) - 1U) == 0
		    && pager.line_count == 0.0f
		    && pager.newline_flag == 0.0f);
	}
}

static void
test_computer_autopilot_destination_terminal_presentation(void)
{
	static const uint8_t prompt[] =
	    "What sector do you want to go to? ";
	static const uint8_t prefix[] =
	    "\r\nWhat sector do you want to go to? ";
	static const uint8_t inactivity[] =
	    "\r\n\aUSER FELL ASLEEP!\n\r";
	static const uint8_t notice_before[] = "\r\n";
	static const uint8_t session_limit[] =
	    "\r\n\a\a\aTIME LIMIT EXCEEDED!\a\a\a\n\r";
	static const uint8_t session_after[] =
	    "\r\n\a\a\aTIME LIMIT EXCEEDED!\a\a\a";
	static const struct {
		enum yt_ab36_terminal_kind kind;
		int carrier_failure;
		const uint8_t *suffix;
		size_t suffix_length;
		bool succeeds;
	} cases[] = {
		{YT_AB36_TERMINAL_INACTIVITY, 0, inactivity,
		    sizeof(inactivity) - 1U, true},
		{YT_AB36_TERMINAL_INACTIVITY, 1, notice_before,
		    sizeof(notice_before) - 1U, false},
		{YT_AB36_TERMINAL_SESSION_LIMIT, 0, session_limit,
		    sizeof(session_limit) - 1U, true},
		{YT_AB36_TERMINAL_SESSION_LIMIT, 2, session_after,
		    sizeof(session_after) - 1U, false},
	};
	size_t pass;

	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		struct yt_present_state current = state(true);
		struct yt_present_result result;
		struct yt_pager_state pager;
		struct pager_capture capture;
		struct computer_port_terminal_join join;
		char accumulator[80];
		bool running = true;
		bool terminated = false;
		bool result_ok;

		current.foreground = 1.0f;
		current.cached_foreground = 1.0f;
		memset(&pager, 0, sizeof(pager));
		pager.foreground = 1;
		memset(&capture, 0, sizeof(capture));
		memset(accumulator, 0, sizeof(accumulator));
		CHECK(yt_present_line(NULL, 0U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		pager.newline_flag = 1.0f;
		pager_fixture_b05d(&pager, &current, prompt,
		    sizeof(prompt) - 1U, &capture);
		yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
		CHECK(capture.remote_length == sizeof(prefix) - 1U
		    && memcmp(capture.remote, prefix, sizeof(prefix) - 1U) == 0);
		join.current = &current;
		join.pager = &pager;
		join.capture = &capture;
		join.running = &running;
		join.terminated = &terminated;
		join.notice_carrier_failure = cases[pass].carrier_failure;
		join.closed = false;
		result_ok = yt_input_ab36_terminal_run(cases[pass].kind,
		    &running, &terminated, computer_port_terminal_notice,
		    computer_port_terminal_close, &join);
		CHECK(result_ok == cases[pass].succeeds
		    && capture.remote_length == sizeof(prefix) - 1U
		    + cases[pass].suffix_length
		    && memcmp(capture.remote + sizeof(prefix) - 1U,
		    cases[pass].suffix, cases[pass].suffix_length) == 0
		    && !running && terminated && join.closed
		    && pager.line_count == (cases[pass].succeeds ? 1.0f : 0.0f)
		    && pager.newline_flag == 0.0f && accumulator[0] == '\0');
	}
	{
		struct yt_present_state current = state(true);
		struct yt_present_result result;
		struct yt_pager_state pager;
		struct pager_capture capture;
		char accumulator[80];

		current.foreground = 1.0f;
		current.cached_foreground = 1.0f;
		memset(&pager, 0, sizeof(pager));
		pager.foreground = 1;
		memset(&capture, 0, sizeof(capture));
		memset(accumulator, 0, sizeof(accumulator));
		CHECK(yt_present_line(NULL, 0U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		pager.newline_flag = 1.0f;
		pager_fixture_b05d(&pager, &current, prompt,
		    sizeof(prompt) - 1U, &capture);
		yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
		CHECK(capture.remote_length == sizeof(prefix) - 1U
		    && memcmp(capture.remote, prefix, sizeof(prefix) - 1U) == 0
		    && pager.line_count == 0.0f
		    && pager.newline_flag == 0.0f);
	}
}

static void
test_computer_path_destination_terminal_presentation(void)
{
	static const uint8_t start_prompt[] =
	    "Enter start for path search? ";
	static const uint8_t destination_prompt[] =
	    "What sector do you want to go to? ";
	static const uint8_t prefix[] =
	    "\r\nEnter start for path search? 1\r\n"
	    "\r\nWhat sector do you want to go to? ";
	static const uint8_t inactivity[] =
	    "\r\n\aUSER FELL ASLEEP!\n\r";
	static const uint8_t notice_before[] = "\r\n";
	static const uint8_t session_limit[] =
	    "\r\n\a\a\aTIME LIMIT EXCEEDED!\a\a\a\n\r";
	static const uint8_t session_after[] =
	    "\r\n\a\a\aTIME LIMIT EXCEEDED!\a\a\a";
	static const struct {
		enum yt_ab36_terminal_kind kind;
		int carrier_failure;
		const uint8_t *suffix;
		size_t suffix_length;
		bool succeeds;
	} cases[] = {
		{YT_AB36_TERMINAL_INACTIVITY, 0, inactivity,
		    sizeof(inactivity) - 1U, true},
		{YT_AB36_TERMINAL_INACTIVITY, 1, notice_before,
		    sizeof(notice_before) - 1U, false},
		{YT_AB36_TERMINAL_SESSION_LIMIT, 0, session_limit,
		    sizeof(session_limit) - 1U, true},
		{YT_AB36_TERMINAL_SESSION_LIMIT, 2, session_after,
		    sizeof(session_after) - 1U, false},
	};
	size_t pass;

	for (pass = 0U; pass <= YT_ARRAY_LEN(cases); ++pass) {
		struct yt_present_state current = state(true);
		struct yt_present_result result;
		struct yt_pager_state pager;
		struct pager_capture capture;
		struct computer_port_terminal_join join;
		char accumulator[80];
		bool running = true;
		bool terminated = false;
		bool result_ok;

		current.foreground = 1.0f;
		current.cached_foreground = 1.0f;
		memset(&pager, 0, sizeof(pager));
		pager.foreground = 1;
		memset(&capture, 0, sizeof(capture));
		memset(accumulator, 0, sizeof(accumulator));
		CHECK(yt_present_line(NULL, 0U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		pager.newline_flag = 1.0f;
		pager_fixture_b05d(&pager, &current, start_prompt,
		    sizeof(start_prompt) - 1U, &capture);
		yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
		CHECK(yt_present_editor_echo((const uint8_t *)"1", 1U,
		    (const uint8_t *)"1", 1U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		CHECK(yt_present_line(NULL, 0U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		CHECK(yt_present_line(NULL, 0U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		pager.newline_flag = 1.0f;
		pager_fixture_b05d(&pager, &current, destination_prompt,
		    sizeof(destination_prompt) - 1U, &capture);
		yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
		CHECK(capture.remote_length == sizeof(prefix) - 1U
		    && memcmp(capture.remote, prefix, sizeof(prefix) - 1U) == 0
		    && pager.line_count == 0.0f
		    && pager.newline_flag == 0.0f);
		if (pass == YT_ARRAY_LEN(cases))
			continue;
		join.current = &current;
		join.pager = &pager;
		join.capture = &capture;
		join.running = &running;
		join.terminated = &terminated;
		join.notice_carrier_failure = cases[pass].carrier_failure;
		join.closed = false;
		result_ok = yt_input_ab36_terminal_run(cases[pass].kind,
		    &running, &terminated, computer_port_terminal_notice,
		    computer_port_terminal_close, &join);
		CHECK(result_ok == cases[pass].succeeds
		    && capture.remote_length == sizeof(prefix) - 1U
		    + cases[pass].suffix_length
		    && memcmp(capture.remote + sizeof(prefix) - 1U,
		    cases[pass].suffix, cases[pass].suffix_length) == 0
		    && !running && terminated && join.closed
		    && pager.line_count == (cases[pass].succeeds ? 1.0f : 0.0f)
		    && pager.newline_flag == 0.0f && accumulator[0] == '\0');
	}
}

static void
test_computer_autopilot_presentation(void)
{
	static const uint8_t destination_prompt[] =
	    "What sector do you want to go to? ";
	static const uint8_t working[] = "Working. ";
	static const uint8_t heading[] =
	    "The shortest path from sector 1 to sector 2 is:";
	static const uint8_t one[] = " 1";
	static const uint8_t two[] = " 2";
	static const uint8_t course[] = "Course will take 1 turns.";
	static const uint8_t turns[] = "You have 1 turns left.";
	static const uint8_t confirmation[] =
	    "Enter course into autopilot? (Y/[N])";
	static const uint8_t engaged[] = "Autopilot Engaged.";
	static const uint8_t stop[] = "Ctrl-X to Stop";
	static const uint8_t expected[] =
	    "\r\nWhat sector do you want to go to? 2\r\n"
	    "\r\nWorking. The shortest path from sector 1 to sector 2 is:\n\r"
	    "\r\n 1 2\r\n"
	    "\r\nCourse will take 1 turns.\n\r"
	    "You have 1 turns left.\n\r"
	    "Enter course into autopilot? (Y/[N])Y\r\n"
	    "\r\nAutopilot Engaged.\n\r"
	    "\r\nCtrl-X to Stop\n\r";
	struct yt_present_state current = state(false);
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	char accumulator[80] = "";

	current.foreground = 1.0f;
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 1;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, destination_prompt,
	    sizeof(destination_prompt) - 1U, &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo((const uint8_t *)"2", 1,
	    (const uint8_t *)"2", 1, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, working, sizeof(working) - 1U,
	    &capture);
	pager_fixture_b05d(&pager, &current, heading, sizeof(heading) - 1U,
	    &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, one, sizeof(one) - 1U,
	    &capture);
	pager.line_count = 0.0f;
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, two, sizeof(two) - 1U,
	    &capture);
	pager.line_count = 0.0f;
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current, course, sizeof(course) - 1U,
	    &capture);
	pager_fixture_b05d(&pager, &current, turns, sizeof(turns) - 1U,
	    &capture);
	CHECK(yt_present_character(confirmation, sizeof(confirmation) - 1U,
	    &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo((const uint8_t *)"Y", 1,
	    (const uint8_t *)"Y", 1, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current, engaged, sizeof(engaged) - 1U,
	    &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current, stop, sizeof(stop) - 1U,
	    &capture);
	CHECK(sizeof(expected) - 1U == 239U);
	CHECK(capture.remote_length == sizeof(expected) - 1U
	    && memcmp(capture.remote, expected, sizeof(expected) - 1U) == 0);
	CHECK(pager.line_count == 2.0f && pager.newline_flag == 0.0f);
}

static void
computer_autopilot_one_hop_prefix(bool ansi,
    struct pager_capture *capture, struct yt_present_state *current,
    struct yt_pager_state *pager, char *accumulator,
    size_t accumulator_capacity)
{
	static const uint8_t destination_prompt[] =
	    "What sector do you want to go to? ";
	static const uint8_t working[] = "Working. ";
	static const uint8_t heading[] =
	    "The shortest path from sector 1 to sector 2 is:";
	static const uint8_t one[] = " 1";
	static const uint8_t two[] = " 2";
	static const uint8_t course[] = "Course will take 1 turns.";
	struct yt_present_result result;

	*current = state(ansi);
	current->foreground = 1.0f;
	if (ansi)
		current->cached_foreground = 1.0f;
	memset(pager, 0, sizeof(*pager));
	pager->foreground = 1;
	memset(capture, 0, sizeof(*capture));
	memset(accumulator, 0, accumulator_capacity);
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager->newline_flag = 1.0f;
	pager_fixture_b05d(pager, current, destination_prompt,
	    sizeof(destination_prompt) - 1U, capture);
	yt_pager_editor_enter(pager, accumulator, accumulator_capacity);
	CHECK(yt_present_editor_echo((const uint8_t *)"2", 1U,
	    (const uint8_t *)"2", 1U, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager->newline_flag = 1.0f;
	pager_fixture_b05d(pager, current, working, sizeof(working) - 1U,
	    capture);
	pager_fixture_b05d(pager, current, heading, sizeof(heading) - 1U,
	    capture);
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager->newline_flag = 1.0f;
	pager_fixture_b05d(pager, current, one, sizeof(one) - 1U, capture);
	pager->line_count = 0.0f;
	pager->newline_flag = 1.0f;
	pager_fixture_b05d(pager, current, two, sizeof(two) - 1U, capture);
	pager->line_count = 0.0f;
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager_fixture_b05d(pager, current, course, sizeof(course) - 1U,
	    capture);
}

static void
test_computer_autopilot_alternate_presentation(void)
{
	static const uint8_t turns[] = "You have 1 turns left.";
	static const uint8_t confirmation[] =
	    "Enter course into autopilot? (Y/[N])";
	static const uint8_t insufficient[] =
	    "Not enough turns left to autopilot this course!";
	static const uint8_t explicit_n[] =
	    "\r\nWhat sector do you want to go to? 2\r\n"
	    "\r\nWorking. The shortest path from sector 1 to sector 2 is:\n\r"
	    "\r\n 1 2\r\n"
	    "\r\nCourse will take 1 turns.\n\r"
	    "You have 1 turns left.\n\r"
	    "Enter course into autopilot? (Y/[N])N\r\n";
	static const uint8_t blank[] =
	    "\r\nWhat sector do you want to go to? 2\r\n"
	    "\r\nWorking. The shortest path from sector 1 to sector 2 is:\n\r"
	    "\r\n 1 2\r\n"
	    "\r\nCourse will take 1 turns.\n\r"
	    "You have 1 turns left.\n\r"
	    "Enter course into autopilot? (Y/[N])\r\n";
	static const uint8_t insufficient_ansi[] =
	    "\r\nWhat sector do you want to go to? 2\r\n"
	    "\r\nWorking. The shortest path from sector 1 to sector 2 is:\n\r"
	    "\r\n 1 2\r\n"
	    "\r\nCourse will take 1 turns.\n\r"
	    "\r\n\x1b[0;31;40;5;1m"
	    "Not enough turns left to autopilot this course!\n\r";
	struct yt_present_state current;
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	char accumulator[80];

	computer_autopilot_one_hop_prefix(false, &capture, &current, &pager,
	    accumulator, sizeof(accumulator));
	pager_fixture_b05d(&pager, &current, turns, sizeof(turns) - 1U,
	    &capture);
	CHECK(yt_present_character(confirmation, sizeof(confirmation) - 1U,
	    &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo((const uint8_t *)"N", 1U,
	    (const uint8_t *)"N", 1U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(sizeof(explicit_n) - 1U == 199U
	    && capture.remote_length == sizeof(explicit_n) - 1U
	    && memcmp(capture.remote, explicit_n, sizeof(explicit_n) - 1U) == 0
	    && pager.line_count == 0.0f && pager.newline_flag == 0.0f);

	computer_autopilot_one_hop_prefix(false, &capture, &current, &pager,
	    accumulator, sizeof(accumulator));
	pager_fixture_b05d(&pager, &current, turns, sizeof(turns) - 1U,
	    &capture);
	CHECK(yt_present_character(confirmation, sizeof(confirmation) - 1U,
	    &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(sizeof(blank) - 1U == 198U
	    && capture.remote_length == sizeof(blank) - 1U
	    && memcmp(capture.remote, blank, sizeof(blank) - 1U) == 0
	    && pager.line_count == 0.0f && pager.newline_flag == 0.0f);

	computer_autopilot_one_hop_prefix(true, &capture, &current, &pager,
	    accumulator, sizeof(accumulator));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.bold = 1.0f;
	current.blink = 1.0f;
	pager.newline_flag = 0.0f;
	pager_fixture_b05d(&pager, &current, insufficient,
	    sizeof(insufficient) - 1U, &capture);
	CHECK(sizeof(insufficient_ansi) - 1U == 201U
	    && capture.remote_length == sizeof(insufficient_ansi) - 1U
	    && memcmp(capture.remote, insufficient_ansi,
	    sizeof(insufficient_ansi) - 1U) == 0
	    && pager.line_count == 2.0f && pager.newline_flag == 0.0f
	    && current.bold == 0.0f && current.blink == 0.0f);
}

static void
test_computer_autopilot_confirmation_terminal_presentation(void)
{
	static const uint8_t turns[] = "You have 1 turns left.";
	static const uint8_t confirmation[] =
	    "Enter course into autopilot? (Y/[N])";
	static const uint8_t inactivity[] =
	    "\r\n\aUSER FELL ASLEEP!\n\r";
	static const uint8_t notice_before[] = "\r\n";
	static const uint8_t session_limit[] =
	    "\r\n\a\a\aTIME LIMIT EXCEEDED!\a\a\a\n\r";
	static const uint8_t session_after[] =
	    "\r\n\a\a\aTIME LIMIT EXCEEDED!\a\a\a";
	static const struct {
		enum yt_ab36_terminal_kind kind;
		int carrier_failure;
		const uint8_t *suffix;
		size_t suffix_length;
		bool succeeds;
	} cases[] = {
		{YT_AB36_TERMINAL_INACTIVITY, 0, inactivity,
		    sizeof(inactivity) - 1U, true},
		{YT_AB36_TERMINAL_INACTIVITY, 1, notice_before,
		    sizeof(notice_before) - 1U, false},
		{YT_AB36_TERMINAL_SESSION_LIMIT, 0, session_limit,
		    sizeof(session_limit) - 1U, true},
		{YT_AB36_TERMINAL_SESSION_LIMIT, 2, session_after,
		    sizeof(session_after) - 1U, false},
	};
	size_t pass;

	for (pass = 0U; pass < YT_ARRAY_LEN(cases); ++pass) {
		struct yt_present_state current;
		struct yt_present_result result;
		struct yt_pager_state pager;
		struct pager_capture capture;
		struct computer_port_terminal_join join;
		char accumulator[80];
		bool running = true;
		bool terminated = false;
		bool result_ok;
		size_t prefix_length;

		computer_autopilot_one_hop_prefix(true, &capture, &current,
		    &pager, accumulator, sizeof(accumulator));
		pager_fixture_b05d(&pager, &current, turns,
		    sizeof(turns) - 1U, &capture);
		CHECK(yt_present_character(confirmation,
		    sizeof(confirmation) - 1U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
		prefix_length = capture.remote_length;
		join.current = &current;
		join.pager = &pager;
		join.capture = &capture;
		join.running = &running;
		join.terminated = &terminated;
		join.notice_carrier_failure = cases[pass].carrier_failure;
		join.closed = false;
		result_ok = yt_input_ab36_terminal_run(cases[pass].kind,
		    &running, &terminated, computer_port_terminal_notice,
		    computer_port_terminal_close, &join);
		CHECK(prefix_length == 196U
		    && result_ok == cases[pass].succeeds
		    && capture.remote_length == prefix_length
		    + cases[pass].suffix_length
		    && memcmp(capture.remote + prefix_length, cases[pass].suffix,
		    cases[pass].suffix_length) == 0
		    && !running && terminated && join.closed
		    && pager.line_count == (cases[pass].succeeds ? 1.0f : 0.0f)
		    && pager.newline_flag == 0.0f && accumulator[0] == '\0');
	}
	{
		struct yt_present_state current;
		struct yt_present_result result;
		struct yt_pager_state pager;
		struct pager_capture capture;
		char accumulator[80];

		computer_autopilot_one_hop_prefix(true, &capture, &current,
		    &pager, accumulator, sizeof(accumulator));
		pager_fixture_b05d(&pager, &current, turns,
		    sizeof(turns) - 1U, &capture);
		CHECK(yt_present_character(confirmation,
		    sizeof(confirmation) - 1U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
		CHECK(capture.remote_length == 196U
		    && pager.line_count == 0.0f
		    && pager.newline_flag == 0.0f);
	}
}

static void
test_computer_navigation_direct_b05d_carrier_prefixes(void)
{
	static const uint8_t start_prompt[] =
	    "Enter start for path search? ";
	static const uint8_t start_after[] =
	    "\r\nEnter start for path search? ";
	static const uint8_t turns[] = "You have 1 turns left.";
	static const uint8_t confirmation[] =
	    "Enter course into autopilot? (Y/[N])";
	static const uint8_t engaged[] = "Autopilot Engaged.";
	struct yt_present_state current;
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	char accumulator[80];

	current = state(true);
	current.foreground = 1.0f;
	current.cached_foreground = 1.0f;
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 1;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(capture.remote_length == 2U
	    && memcmp(capture.remote, "\r\n", 2U) == 0
	    && pager.line_count == 0.0f);

	CHECK(yt_present_paged_text(start_prompt, sizeof(start_prompt) - 1U,
	    &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(capture.remote_length == sizeof(start_after) - 1U
	    && memcmp(capture.remote, start_after,
	    sizeof(start_after) - 1U) == 0
	    && pager.line_count == 0.0f);

	computer_autopilot_one_hop_prefix(true, &capture, &current, &pager,
	    accumulator, sizeof(accumulator));
	pager_fixture_b05d(&pager, &current, turns, sizeof(turns) - 1U,
	    &capture);
	CHECK(yt_present_character(confirmation, sizeof(confirmation) - 1U,
	    &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo((const uint8_t *)"Y", 1U,
	    (const uint8_t *)"Y", 1U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(capture.remote_length == 199U);
	CHECK(yt_present_line(NULL, 0U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(capture.remote_length == 201U
	    && memcmp(capture.remote + 196U,
	    "Y\r\n\r\n", 5U) == 0 && pager.line_count == 0.0f);

	CHECK(yt_present_paged_text(engaged, sizeof(engaged) - 1U,
	    &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(capture.remote_length == 219U
	    && memcmp(capture.remote + 201U, engaged,
	    sizeof(engaged) - 1U) == 0
	    && pager.line_count == 0.0f);
}

static void
test_computer_scoreboard_presentation(void)
{
	static const uint8_t prompt[] =
	    "Enter 'O' to see OLD scoreboard or press [ENTER] for UPDATED one. -=>";
	static const uint8_t heading[] = "P l a y e r  R a n k i n g s";
	static const uint8_t notice[] = "Cntl-X to Stop";
	static const uint8_t expected[] =
	    "\r\n"
	    "Enter 'O' to see OLD scoreboard or press [ENTER] for UPDATED one. -=>"
	    "\r\n\r\n"
	    "P l a y e r  R a n k i n g s....\r\n"
	    "\r\nCntl-X to Stop\n\r"
	    "\r\n\r\n";
	struct yt_present_state current = state(false);
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	char accumulator[80] = "";
	int index;

	current.foreground = 1.0f;
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 1;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, prompt, sizeof(prompt) - 1U,
	    &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.line_count = 0.0f;
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, heading, sizeof(heading) - 1U,
	    &capture);
	for (index = 0; index < 4; ++index) {
		CHECK(yt_present_character((const uint8_t *)".", 1,
		    &current, &result) == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
	}
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current, notice, sizeof(notice) - 1U,
	    &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.line_count = 0.0f;
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(sizeof(expected) - 1U == 131U);
	CHECK(capture.remote_length == sizeof(expected) - 1U
	    && memcmp(capture.remote, expected, sizeof(expected) - 1U) == 0);
	CHECK(pager.line_count == 0.0f && pager.newline_flag == 0.0f);
}

static void
test_radio_target_blank_presentation(void)
{
	static const uint8_t warming[] = "Warming up sub-space radio.";
	static const uint8_t prompt[] =
	    "Send a message to who? (search string) or 'ALL' or 'TEAM'? ";
	static const uint8_t expected[] =
	    "\r\nWarming up sub-space radio.\n\r"
	    "\r\n"
	    "Send a message to who? (search string) or 'ALL' or 'TEAM'? \r\n";
	struct yt_present_state current = state(false);
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	char accumulator[80] = "";

	current.foreground = 1.0f;
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 1;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current, warming, sizeof(warming) - 1U,
	    &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, prompt, sizeof(prompt) - 1U,
	    &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(sizeof(expected) - 1U == 94U);
	CHECK(capture.remote_length == sizeof(expected) - 1U
	    && memcmp(capture.remote, expected, sizeof(expected) - 1U) == 0);
	CHECK(pager.line_count == 0.0f && pager.newline_flag == 0.0f);
}

static void
test_computer_radio_composer_cycle_presentation(void)
{
	static const uint8_t computer_prompt[] =
	    "Time: 14:59  Computer command (?=help)? ";
	static const uint8_t warming[] = "Warming up sub-space radio.";
	static const uint8_t target_prompt[] =
	    "Send a message to who? (search string) or 'ALL' or 'TEAM'? ";
	static const uint8_t expected[] =
	    "\r\n"
	    "Time: 14:59  Computer command (?=help)? 5\r\n"
	    "\r\nWarming up sub-space radio.\n\r"
	    "\r\n"
	    "Send a message to who? (search string) or 'ALL' or 'TEAM'? \r\n"
	    "\r\n"
	    "Time: 14:59  Computer command (?=help)? ";
	struct yt_present_state current = state(false);
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	char accumulator[80] = "";

	current.foreground = 1.0f;
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 1;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, computer_prompt,
	    sizeof(computer_prompt) - 1U, &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo((const uint8_t *)"5", 1,
	    (const uint8_t *)"5", 1, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current, warming, sizeof(warming) - 1U,
	    &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, target_prompt,
	    sizeof(target_prompt) - 1U, &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, computer_prompt,
	    sizeof(computer_prompt) - 1U, &capture);
	CHECK(sizeof(expected) - 1U == 181U);
	CHECK(capture.remote_length == sizeof(expected) - 1U
	    && memcmp(capture.remote, expected, sizeof(expected) - 1U) == 0);
	CHECK(pager.line_count == 1.0f && pager.newline_flag == 0.0f);
}

static void
test_radio_body_presentation(void)
{
	static const uint8_t first_prompt[] = " 1:";
	static const uint8_t second_prompt[] = " 2:";
	static const uint8_t menu[] =
	    "[L] List [S] Send [A] Abort [C] Continue [E] Edit -=> ";
	static const uint8_t success[] = "Transmission successful!";
	static const uint8_t empty_expected[] = " 1:\r\n\r\n";
	static const uint8_t send_expected[] =
	    " 1:Hi\r\n"
	    " 2:\r\n"
	    "\r\n"
	    "[L] List [S] Send [A] Abort [C] Continue [E] Edit -=> S\r\n"
	    "\r\n"
	    "Transmission successful!\n\r";
	struct yt_present_state current;
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	char accumulator[80] = "";

	current = state(false);
	current.foreground = 1.0f;
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 1;
	memset(&capture, 0, sizeof(capture));
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, first_prompt,
	    sizeof(first_prompt) - 1U, &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(sizeof(empty_expected) - 1U == 7U);
	CHECK(capture.remote_length == sizeof(empty_expected) - 1U
	    && memcmp(capture.remote, empty_expected,
	    sizeof(empty_expected) - 1U) == 0);

	current = state(false);
	current.foreground = 1.0f;
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 1;
	memset(&capture, 0, sizeof(capture));
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, first_prompt,
	    sizeof(first_prompt) - 1U, &capture);
	CHECK(yt_present_character((const uint8_t *)"H", 1,
	    &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_character((const uint8_t *)"i", 1,
	    &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.line_count = 0.0f;
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, second_prompt,
	    sizeof(second_prompt) - 1U, &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, menu, sizeof(menu) - 1U,
	    &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo((const uint8_t *)"S", 1,
	    (const uint8_t *)"S", 1, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.bold = 1.0f;
	current.blink = 1.0f;
	pager_fixture_b05d(&pager, &current, success, sizeof(success) - 1U,
	    &capture);
	CHECK(sizeof(send_expected) - 1U == 99U);
	CHECK(capture.remote_length == sizeof(send_expected) - 1U
	    && memcmp(capture.remote, send_expected,
	    sizeof(send_expected) - 1U) == 0);
	CHECK(pager.line_count == 1.0f && pager.newline_flag == 0.0f);
}

static void
test_radio_body_cleanup_presentation(void)
{
	static const uint8_t backspace[] = {'\b', ' ', '\b'};
	static const uint8_t wrap[] = {'\b', ' ', '\r'};
	struct yt_present_state current = state(false);
	struct yt_present_result result;

	current.sound.snoop = 0.0f;
	current.sound.mode = 0.0f;
	CHECK(yt_present_radio_backspace(1, 0U, &current, &result)
	    == YT_PRESENT_OK);
	CHECK(result.remote_length == sizeof(backspace)
	    && memcmp(result.remote, backspace, sizeof(backspace)) == 0);
	CHECK(result.event_count == 4U
	    && result.events[0].operation == YT_PRESENT_REMOTE_SEMI
	    && result.events[1].operation == YT_PRESENT_LOCAL_LOCATE
	    && result.events[1].row == -1 && result.events[1].column == 4
	    && result.events[2].operation == YT_PRESENT_LOCAL_SEMI
	    && result.events[2].length == 1U
	    && result.events[2].data[0] == ' '
	    && result.events[3].operation == YT_PRESENT_LOCAL_LOCATE
	    && result.events[3].row == -1 && result.events[3].column == 4);

	current.sound.mode = 1.0f;
	CHECK(yt_present_radio_backspace(20, 5U, &current, &result)
	    == YT_PRESENT_OK);
	CHECK(result.remote_length == 0U && result.event_count == 3U
	    && result.events[0].operation == YT_PRESENT_LOCAL_LOCATE
	    && result.events[0].column == 10
	    && result.events[1].operation == YT_PRESENT_LOCAL_SEMI
	    && result.events[2].operation == YT_PRESENT_LOCAL_LOCATE
	    && result.events[2].column == 10);
	current.sound.mode = 2.0f;
	CHECK(yt_present_radio_backspace(1, 0U, &current, &result)
	    == YT_PRESENT_OK && result.remote_length == 0U);

	current.sound.mode = 0.0f;
	CHECK(yt_present_radio_wrap_cleanup(1, 74U, &current, &result)
	    == YT_PRESENT_OK);
	CHECK(result.remote_length == sizeof(wrap)
	    && memcmp(result.remote, wrap, sizeof(wrap)) == 0
	    && result.event_count == 3U
	    && result.events[0].operation == YT_PRESENT_LOCAL_LOCATE
	    && result.events[0].row == -1 && result.events[0].column == 77
	    && result.events[1].operation == YT_PRESENT_LOCAL_SEMI
	    && result.events[1].length == 1U
	    && result.events[1].data[0] == ' '
	    && result.events[2].operation == YT_PRESENT_REMOTE_SEMI);
	current.sound.mode = 1.0f;
	CHECK(yt_present_radio_wrap_cleanup(1, 71U, &current, &result)
	    == YT_PRESENT_OK);
	CHECK(result.remote_length == 0U && result.event_count == 2U
	    && result.events[0].operation == YT_PRESENT_LOCAL_LOCATE
	    && result.events[0].column == 74
	    && result.events[1].operation == YT_PRESENT_LOCAL_SEMI
	    && result.events[1].length == 4U
	    && memcmp(result.events[1].data, "    ", 4U) == 0);
	current.sound.mode = 2.0f;
	CHECK(yt_present_radio_wrap_cleanup(1, 75U, &current, &result)
	    == YT_PRESENT_OK && result.remote_length == 1U
	    && result.remote[0] == '\r' && result.event_count == 3U
	    && result.events[1].operation == YT_PRESENT_LOCAL_SEMI
	    && result.events[1].length == 0U);

	CHECK(yt_present_radio_backspace(0, 0U, &current, &result)
	    == YT_PRESENT_RANGE);
	CHECK(yt_present_radio_backspace(1, 75U, &current, &result)
	    == YT_PRESENT_RANGE);
	CHECK(yt_present_radio_wrap_cleanup(1, 0U, &current, &result)
	    == YT_PRESENT_RANGE);
	CHECK(yt_present_radio_wrap_cleanup(22, 74U, &current, &result)
	    == YT_PRESENT_RANGE);
}

static void
computer_radio_log_empty_cycle_fixture(bool ansi, float mode,
    const uint8_t *typed, size_t typed_length,
    struct yt_present_state *current, struct yt_pager_state *pager,
    struct pager_capture *capture, char *accumulator,
    size_t accumulator_capacity, char *queue, size_t queue_capacity,
    size_t *queue_position, size_t *queue_length)
{
	static const uint8_t prompt[] =
	    "Time: 14:59  Computer command (?=help)? ";
	static const uint8_t heading[] =
	    "Log of messages sent/recieved.";
	static const uint8_t none[] = "None Found.";
	struct yt_present_result result;

	CHECK(typed != NULL && typed_length < accumulator_capacity);
	*current = state(ansi);
	current->sound.mode = mode;
	current->foreground = 6.0f;
	current->cached_foreground = ansi ? 6.0f : 0.0f;
	memset(pager, 0, sizeof(*pager));
	pager->foreground = 6;
	memset(capture, 0, sizeof(*capture));
	memset(accumulator, 0, accumulator_capacity);
	memset(queue, 0, queue_capacity);
	*queue_position = 0U;
	*queue_length = 0U;
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	current->foreground = 1.0f;
	pager->foreground = 1;
	pager->newline_flag = 1.0f;
	pager_fixture_b05d(pager, current, prompt, sizeof(prompt) - 1U,
	    capture);
	yt_pager_editor_enter(pager, accumulator, accumulator_capacity);
	memcpy(accumulator, typed, typed_length);
	accumulator[typed_length] = '\0';
	CHECK(yt_present_editor_echo(typed, typed_length, typed, typed_length,
	    current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	CHECK(yt_input_split_semicolon(accumulator, queue, queue_capacity,
	    queue_position, queue_length));
	CHECK(strcmp(accumulator, "6") == 0);
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	CHECK(yt_present_line(heading, sizeof(heading) - 1U,
	    current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	CHECK(yt_present_line(none, sizeof(none) - 1U,
	    current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(capture, &result);
	pager->newline_flag = 1.0f;
	pager_fixture_b05d(pager, current, prompt, sizeof(prompt) - 1U,
	    capture);
}

static void
test_computer_radio_log_presentation(void)
{
	static const uint8_t prompt[] =
	    "Time: 14:59  Computer command (?=help)? ";
	static const uint8_t heading[] =
	    "Log of messages sent/recieved.";
	static const uint8_t expected[] =
	    "\r\n"
	    "Time: 14:59  Computer command (?=help)? 6\r\n"
	    "\r\n"
	    "Log of messages sent/recieved.\r\n"
	    "None Found.\r\n"
	    "\r\n"
	    "Time: 14:59  Computer command (?=help)? ";
	static const uint8_t ansi_expected[] =
	    "\r\n"
	    "\x1b[0;31;40mTime: 14:59  Computer command (?=help)? 6\r\n"
	    "\r\n"
	    "Log of messages sent/recieved.\r\n"
	    "None Found.\r\n"
	    "\r\n"
	    "Time: 14:59  Computer command (?=help)? ";
	static const uint8_t corrupt_expected[] =
	    "\r\n\r\n\r\n"
	    "Log of messages sent/recieved.\r\n"
	    "None Found.\r\n\r\n";
	static const uint8_t message_prefix[] =
	    "\r\n"
	    "Time: 14:59  Computer command (?=help)? 6\r\n"
	    "\r\n"
	    "Log of messages sent/recieved.\r\n"
	    "\r\n"
	    "Message to: Bob * From: Ada\r\n"
	    "SENT";
	static const uint8_t message_suffix[] =
	    "\r\n"
	    "\r\n"
	    "Time: 14:59  Computer command (?=help)? ";
	static const uint8_t message_header[] =
	    "Message to: Bob * From: Ada";
	struct yt_present_state current = state(false);
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	uint8_t body[74];
	uint8_t message_expected[228];
	char queue[16];
	char accumulator[80] = "";
	size_t offset;
	size_t queue_position;
	size_t queue_length;

	computer_radio_log_empty_cycle_fixture(false, 0.0f,
	    (const uint8_t *)"6", 1U, &current, &pager, &capture,
	    accumulator, sizeof(accumulator), queue, sizeof(queue),
	    &queue_position, &queue_length);
	CHECK(sizeof(expected) - 1U == 134U);
	CHECK(capture.remote_length == sizeof(expected) - 1U
	    && memcmp(capture.remote, expected, sizeof(expected) - 1U) == 0);
	CHECK(pager.line_count == 1.0f && pager.newline_flag == 0.0f
	    && queue_position == 0U && queue_length == 0U);

	computer_radio_log_empty_cycle_fixture(true, 0.0f,
	    (const uint8_t *)"6", 1U, &current, &pager, &capture,
	    accumulator, sizeof(accumulator), queue, sizeof(queue),
	    &queue_position, &queue_length);
	CHECK(sizeof(ansi_expected) - 1U == 144U);
	CHECK(capture.remote_length == sizeof(ansi_expected) - 1U
	    && memcmp(capture.remote, ansi_expected,
	    sizeof(ansi_expected) - 1U) == 0);

	computer_radio_log_empty_cycle_fixture(false, 1.0f,
	    (const uint8_t *)"6", 1U, &current, &pager, &capture,
	    accumulator, sizeof(accumulator), queue, sizeof(queue),
	    &queue_position, &queue_length);
	CHECK(capture.remote_length == 0U && capture.local_event_count != 0U);

	computer_radio_log_empty_cycle_fixture(false, 2.0f,
	    (const uint8_t *)"6", 1U, &current, &pager, &capture,
	    accumulator, sizeof(accumulator), queue, sizeof(queue),
	    &queue_position, &queue_length);
	CHECK(capture.remote_length == sizeof(corrupt_expected) - 1U
	    && memcmp(capture.remote, corrupt_expected,
	    sizeof(corrupt_expected) - 1U) == 0);

	computer_radio_log_empty_cycle_fixture(false, 0.0f,
	    (const uint8_t *)"6;Q", 3U, &current, &pager, &capture,
	    accumulator, sizeof(accumulator), queue, sizeof(queue),
	    &queue_position, &queue_length);
	CHECK(queue_position == 0U && queue_length == 2U
	    && memcmp(queue, "Q\r", 2U) == 0
	    && strcmp(accumulator, "6") == 0);

	offset = 0U;
	memcpy(message_expected + offset, message_prefix,
	    sizeof(message_prefix) - 1U);
	offset += sizeof(message_prefix) - 1U;
	memset(message_expected + offset, ' ', 70U);
	offset += 70U;
	memcpy(message_expected + offset, message_suffix,
	    sizeof(message_suffix) - 1U);
	offset += sizeof(message_suffix) - 1U;
	CHECK(offset == sizeof(message_expected));
	memset(body, ' ', sizeof(body));
	memcpy(body, "SENT", 4U);
	current = state(false);
	current.foreground = 1.0f;
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 1;
	memset(&capture, 0, sizeof(capture));
	accumulator[0] = '\0';
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, prompt, sizeof(prompt) - 1U,
	    &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo((const uint8_t *)"6", 1,
	    (const uint8_t *)"6", 1, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(heading, sizeof(heading) - 1U,
	    &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(message_header, sizeof(message_header) - 1U,
	    &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(body, sizeof(body), &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, prompt, sizeof(prompt) - 1U,
	    &capture);
	CHECK(capture.remote_length == sizeof(message_expected)
	    && memcmp(capture.remote, message_expected,
	    sizeof(message_expected)) == 0);
	CHECK(pager.line_count == 1.0f && pager.newline_flag == 0.0f);
}

static void
test_computer_newspaper_presentation(void)
{
	static const uint8_t prompt[] =
	    "Do you want to read [T]oday's or [Y]esterday's news? [T/Y] -=> ";
	static const uint8_t notice[] = "Cntl-X to Stop";
	static const uint8_t expected[] =
	    "\r\n"
	    "Do you want to read [T]oday's or [Y]esterday's news? [T/Y] -=> T\r\n"
	    "\r\nCntl-X to Stop\n\r"
	    "\r\n\r\n";
	struct yt_present_state current = state(false);
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	char accumulator[80] = "";

	current.foreground = 1.0f;
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 1;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, prompt, sizeof(prompt) - 1U,
	    &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo((const uint8_t *)"T", 1,
	    (const uint8_t *)"T", 1, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current, notice, sizeof(notice) - 1U,
	    &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.line_count = 0.0f;
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(sizeof(expected) - 1U == 90U);
	CHECK(capture.remote_length == sizeof(expected) - 1U
	    && memcmp(capture.remote, expected, sizeof(expected) - 1U) == 0);
	CHECK(pager.line_count == 0.0f && pager.newline_flag == 0.0f);
}

static void
test_hostile_attack_admission_presentation(void)
{
	static const uint8_t heading[] = "<Attack>";
	static const uint8_t prompt[] = "Attack with how many fighters? ";
	static const uint8_t none[] = "You don't have any fighters!";
	static const uint8_t too_many[] = "You only have 12!";
	static const uint8_t no_fighters_expected[] =
	    "<Attack>\n\r\r\n"
	    "\x1b[0;33;40;5;1mYou don't have any fighters!\n\r";
	static const uint8_t too_many_expected[] =
	    "<Attack>\n\rAttack with how many fighters? 13\r\n\r\n"
	    "\x1b[0;33;40;5;1mYou only have 12!\n\r";
	static const uint8_t less_than_one_expected[] =
	    "<Attack>\n\rAttack with how many fighters? 0\r\n";
	static const uint8_t defenders_remain_expected[] =
	    "<Attack>\n\rAttack with how many fighters? 1\r\n"
	    "\x1b[MBO1L64P32CEDFEGFAGBAO5BAGFEDC\x0e"
	    "\r\n You lost 1 fighter(s)\n\r"
	    " You destroyed 0 enemy fighters.\n\r\r\n";
	static const uint8_t loss_row[] = " You lost 1 fighter(s)";
	static const uint8_t destroyed_row[] =
	    " You destroyed 0 enemy fighters.";
	struct yt_present_state current;
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	char accumulator[80] = "A";
	static const uint8_t thirteen[] = "13";
	static const uint8_t zero[] = "0";
	static const uint8_t one[] = "1";

	current = state(true);
	current.foreground = 3.0f;
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	memset(&pager, 0, sizeof(pager));
	memset(&capture, 0, sizeof(capture));
	pager.foreground = 3;
	pager_fixture_b05d(&pager, &current, heading, sizeof(heading) - 1U,
	    &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.bold = 1.0f;
	current.blink = 1.0f;
	pager_fixture_b05d(&pager, &current, none, sizeof(none) - 1U,
	    &capture);
	CHECK(capture.remote_length == sizeof(no_fighters_expected) - 1U
	    && memcmp(capture.remote, no_fighters_expected,
	    sizeof(no_fighters_expected) - 1U) == 0);
	CHECK(capture.remote_length == 56U && pager.line_count == 2.0f);

	current = state(true);
	current.foreground = 3.0f;
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	memset(&pager, 0, sizeof(pager));
	memset(&capture, 0, sizeof(capture));
	pager.foreground = 3;
	pager_fixture_b05d(&pager, &current, heading, sizeof(heading) - 1U,
	    &capture);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, prompt, sizeof(prompt) - 1U,
	    &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo(thirteen, sizeof(thirteen) - 1U,
	    thirteen, sizeof(thirteen) - 1U, &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.bold = 1.0f;
	current.blink = 1.0f;
	pager_fixture_b05d(&pager, &current, too_many,
	    sizeof(too_many) - 1U, &capture);
	CHECK(capture.remote_length == sizeof(too_many_expected) - 1U
	    && memcmp(capture.remote, too_many_expected,
	    sizeof(too_many_expected) - 1U) == 0);
	CHECK(capture.remote_length == 80U && pager.line_count == 1.0f);

	current = state(true);
	current.foreground = 3.0f;
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	memset(&pager, 0, sizeof(pager));
	memset(&capture, 0, sizeof(capture));
	pager.foreground = 3;
	pager_fixture_b05d(&pager, &current, heading, sizeof(heading) - 1U,
	    &capture);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, prompt, sizeof(prompt) - 1U,
	    &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo(zero, sizeof(zero) - 1U,
	    zero, sizeof(zero) - 1U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(capture.remote_length == sizeof(less_than_one_expected) - 1U
	    && memcmp(capture.remote, less_than_one_expected,
	    sizeof(less_than_one_expected) - 1U) == 0);
	CHECK(capture.remote_length == 44U && pager.line_count == 0.0f);

	current = state(true);
	current.foreground = 3.0f;
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	memset(&pager, 0, sizeof(pager));
	memset(&capture, 0, sizeof(capture));
	pager.foreground = 3;
	pager_fixture_b05d(&pager, &current, heading, sizeof(heading) - 1U,
	    &capture);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, prompt, sizeof(prompt) - 1U,
	    &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo(one, sizeof(one) - 1U,
	    one, sizeof(one) - 1U, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_sound(2.0f, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current, loss_row,
	    sizeof(loss_row) - 1U, &capture);
	pager_fixture_b05d(&pager, &current, destroyed_row,
	    sizeof(destroyed_row) - 1U, &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(capture.remote_length == sizeof(defenders_remain_expected) - 1U
	    && memcmp(capture.remote, defenders_remain_expected,
	    sizeof(defenders_remain_expected) - 1U) == 0);
	CHECK(capture.remote_length == 139U && pager.line_count == 2.0f);
}

static void
test_deployed_fighter_surrender_presentation(void)
{
	static const uint8_t heading[] = "<Attack>";
	static const uint8_t amount_prompt[] =
	    "Attack with how many fighters? ";
	static const uint8_t radio[] = "RADIO MESSAGE COMING IN!";
	static const uint8_t captain[] =
	    "This is the captain of the fighter group in sector 7";
	static const uint8_t wish[] = "WE WISH TO SURRENDER!!!";
	static const uint8_t answer_prompt[] =
	    "Will you accept our surrender? [Y]/N -=>";
	static const uint8_t joined[] = " We join your forces!";
	static const uint8_t surrendered[] = " 1 fighters surrendered!";
	static const uint8_t lost[] = " You lost 0 fighter(s)";
	static const uint8_t destroyed[] =
	    " You destroyed 0 enemy fighters.";
	static const uint8_t defeated[] =
	    "You defeated all the fighters and have 21 left.";
	static const uint8_t plain[] =
	    "<Attack>\n\rAttack with how many fighters? 20\r\n"
	    "\r\nRADIO MESSAGE COMING IN!\n\r"
	    "\r\nThis is the captain of the fighter group in sector 7\n\r"
	    "\r\nWE WISH TO SURRENDER!!!\n\r"
	    "\r\nWill you accept our surrender? [Y]/N -=>Y\r\n"
	    "\r\n We join your forces!\n\r\x07"
	    " 1 fighters surrendered!\n\r"
	    "\r\n You lost 0 fighter(s)\n\r"
	    " You destroyed 0 enemy fighters.\n\r"
	    "\r\nYou defeated all the fighters and have 21 left.\n\r";
	static const uint8_t ansi[] =
	    "<Attack>\n\rAttack with how many fighters? 20\r\n"
	    "\x1b[MBO1L64P32CEDFEGFAGBAO5BAGFEDC\x0e"
	    "\r\nRADIO MESSAGE COMING IN!\n\r"
	    "\x1b[MBT128O5L48P64CP64C\x0e"
	    "\r\nThis is the captain of the fighter group in sector 7\n\r"
	    "\r\n\x1b[0;36;40;5;1mWE WISH TO SURRENDER!!!\n\r"
	    "\x1b[0;36;40m\r\n"
	    "Will you accept our surrender? [Y]/N -=>Y\r\n"
	    "\r\n We join your forces!\n\r"
	    "\x1b[MBO4L32P32CP64CP64CP64L16EP64L32CP64L12E\x0e"
	    " 1 fighters surrendered!\n\r"
	    "\r\n You lost 0 fighter(s)\n\r"
	    " You destroyed 0 enemy fighters.\n\r"
	    "\r\nYou defeated all the fighters and have 21 left.\n\r";
	static const uint8_t twenty[] = "20";
	static const uint8_t yes[] = "Y";
	struct yt_present_state current;
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	char accumulator[80];
	const uint8_t *expected;
	size_t expected_length;
	bool use_ansi;
	int pass;

	for (pass = 0; pass < 2; ++pass) {
		use_ansi = pass != 0;
		current = state(use_ansi);
		current.foreground = 6.0f;
		current.color_initialized = 1.0f;
		current.cached_foreground = 6.0f;
		current.cached_background = 0.0f;
		memset(&pager, 0, sizeof(pager));
		pager.foreground = 6;
		memset(&capture, 0, sizeof(capture));
		memset(accumulator, 0, sizeof(accumulator));

		pager_fixture_b05d(&pager, &current, heading,
		    sizeof(heading) - 1U, &capture);
		pager.newline_flag = 1.0f;
		pager_fixture_b05d(&pager, &current, amount_prompt,
		    sizeof(amount_prompt) - 1U, &capture);
		yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
		CHECK(yt_present_editor_echo(twenty, sizeof(twenty) - 1U,
		    twenty, sizeof(twenty) - 1U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		pager_capture_line(&capture, &current, NULL, 0U);
		CHECK(yt_present_sound(2.0f, &current, &result) == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);

		pager_capture_line(&capture, &current, NULL, 0U);
		pager_fixture_b05d(&pager, &current, radio,
		    sizeof(radio) - 1U, &capture);
		CHECK(yt_present_sound(4.0f, &current, &result) == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		pager_capture_line(&capture, &current, NULL, 0U);
		pager_fixture_b05d(&pager, &current, captain,
		    sizeof(captain) - 1U, &capture);

		pager_capture_line(&capture, &current, NULL, 0U);
		current.bold = 1.0f;
		current.blink = 1.0f;
		pager_fixture_b05d(&pager, &current, wish,
		    sizeof(wish) - 1U, &capture);
		pager_capture_line(&capture, &current, NULL, 0U);
		CHECK(yt_present_character(answer_prompt,
		    sizeof(answer_prompt) - 1U, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
		CHECK(yt_present_editor_echo(yes, sizeof(yes) - 1U,
		    yes, sizeof(yes) - 1U, &current, &result) == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		pager_capture_line(&capture, &current, NULL, 0U);

		pager_capture_line(&capture, &current, NULL, 0U);
		pager_fixture_b05d(&pager, &current, joined,
		    sizeof(joined) - 1U, &capture);
		CHECK(yt_present_sound(1.0f, &current, &result) == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		pager_fixture_b05d(&pager, &current, surrendered,
		    sizeof(surrendered) - 1U, &capture);
		pager_capture_line(&capture, &current, NULL, 0U);
		pager_fixture_b05d(&pager, &current, lost,
		    sizeof(lost) - 1U, &capture);
		pager_fixture_b05d(&pager, &current, destroyed,
		    sizeof(destroyed) - 1U, &capture);
		pager_capture_line(&capture, &current, NULL, 0U);
		pager_fixture_b05d(&pager, &current, defeated,
		    sizeof(defeated) - 1U, &capture);

		expected = use_ansi ? ansi : plain;
		expected_length = use_ansi ? sizeof(ansi) - 1U
		    : sizeof(plain) - 1U;
		CHECK(capture.remote_length == expected_length
		    && memcmp(capture.remote, expected, expected_length) == 0);
		CHECK(pager.line_count == 5.0f
		    && current.foreground == 6.0f
		    && capture.last_local_foreground == 7
		    && capture.last_local_background == 0);
	}
}

static void
test_deployed_fighter_faction_presentation(void)
{
	static const uint8_t xannor[] =
	    "Whee fyte to the deeth hoo-man slyme!";
	static const uint8_t mercenary[] =
	    "We'll DIE before joining with a slyme like you Sysop!";
	static const uint8_t xannor_plain[] =
	    "Whee fyte to the deeth hoo-man slyme!\n\r\x07";
	static const uint8_t mercenary_plain[] =
	    "We'll DIE before joining with a slyme like you Sysop!\n\r\x07";
	static const uint8_t xannor_ansi[] =
	    "Whee fyte to the deeth hoo-man slyme!\n\r"
	    "\x1b[MBO1L64P8CdGCdGCdGCDGCGD\x0e";
	static const uint8_t mercenary_ansi[] =
	    "We'll DIE before joining with a slyme like you Sysop!\n\r"
	    "\x1b[MBO1L64P8CdGCdGCdGCDGCGD\x0e";
	static const uint8_t reward_plain[] =
	    "Collect 2 turns bonus for destroying 512000 Xannor!!\n\r";
	static const uint8_t reward_ansi[] =
	    "\x1b[0;36;40;1m"
	    "Collect 2 turns bonus for destroying 512000 Xannor!!\n\r";
	static const uint8_t name[] = "Ada";
	uint8_t reward[128];
	uint8_t news[128];
	size_t reward_length;
	size_t news_length;
	const uint8_t *rows[2] = {xannor, mercenary};
	const size_t row_lengths[2] = {
		sizeof(xannor) - 1U, sizeof(mercenary) - 1U
	};
	struct yt_present_state current;
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	const uint8_t *expected;
	size_t expected_length;
	int faction;
	int pass;

	CHECK(yt_xannor_attack_reward_rows(name, sizeof(name) - 1U,
	    2.0f, 512000.0, reward, sizeof(reward), &reward_length,
	    news, sizeof(news), &news_length));
	for (pass = 0; pass < 2; ++pass) {
		for (faction = 0; faction < 2; ++faction) {
			current = state(pass != 0);
			current.foreground = 6.0f;
			current.color_initialized = 1.0f;
			current.cached_foreground = 6.0f;
			current.cached_background = 0.0f;
			memset(&pager, 0, sizeof(pager));
			pager.foreground = 6;
			memset(&capture, 0, sizeof(capture));
			pager_fixture_b05d(&pager, &current, rows[faction],
			    row_lengths[faction], &capture);
			CHECK(yt_present_sound(5.0f, &current, &result)
			    == YT_PRESENT_OK);
			pager_capture_result(&capture, &result);
			if (pass == 0) {
				expected = faction == 0 ? xannor_plain
				    : mercenary_plain;
				expected_length = faction == 0
				    ? sizeof(xannor_plain) - 1U
				    : sizeof(mercenary_plain) - 1U;
			}
			else {
				expected = faction == 0 ? xannor_ansi
				    : mercenary_ansi;
				expected_length = faction == 0
				    ? sizeof(xannor_ansi) - 1U
				    : sizeof(mercenary_ansi) - 1U;
			}
			CHECK(capture.remote_length == expected_length
			    && memcmp(capture.remote, expected,
			    expected_length) == 0);
		}

		current = state(pass != 0);
		current.foreground = 6.0f;
		current.color_initialized = 1.0f;
		current.cached_foreground = 6.0f;
		current.cached_background = 0.0f;
		current.bold = 1.0f;
		memset(&pager, 0, sizeof(pager));
		pager.foreground = 6;
		memset(&capture, 0, sizeof(capture));
		pager_fixture_b05d(&pager, &current, reward, reward_length,
		    &capture);
		expected = pass == 0 ? reward_plain : reward_ansi;
		expected_length = pass == 0 ? sizeof(reward_plain) - 1U
		    : sizeof(reward_ansi) - 1U;
		CHECK(capture.remote_length == expected_length
		    && memcmp(capture.remote, expected, expected_length) == 0);
	}
}

static void
test_shield_spill_presentation(void)
{
	static const uint8_t fighters[] = "Fighters remaining: 0";
	static const uint8_t shields[] = "Shields reduced to: 1";
	static const uint8_t expected[] =
	    "\x1b[0;36;40;1mFighters remaining: 0\r\n"
	    "\x1b[0;36;40mShields reduced to: 1\r\n";
	struct yt_present_state current = state(true);
	struct yt_present_result result;
	struct pager_capture capture;
	float line_count = 21.0f;

	memset(&capture, 0, sizeof(capture));
	current.foreground = 6.0f;
	current.bold = 1.0f;
	CHECK(yt_present_line(fighters, sizeof(fighters) - 1U,
	    &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(shields, sizeof(shields) - 1U,
	    &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(capture.remote_length == sizeof(expected) - 1U
	    && memcmp(capture.remote, expected, sizeof(expected) - 1U) == 0);
	CHECK(line_count == 21.0f && current.bold == 0.0f
	    && capture.last_local_foreground == 3
	    && capture.last_local_background == 0);
}

static struct pager_capture
hostile_bribe_offer_fixture(bool ansi, const uint8_t *response,
    size_t response_length, bool accepted, struct yt_present_state *current,
    struct yt_pager_state *pager)
{
	static const uint8_t introduction[] =
	    "We MAY join up if you pay us enough Ada!";
	static const uint8_t prompt[] =
	    "You have 100 credits. How much do you offer? -+>";
	static const uint8_t agreement[] =
	    "Good Deal! We join up with you!";
	struct yt_present_result result;
	struct pager_capture capture;
	char accumulator[80] = "B";

	*current = state(ansi);
	current->foreground = 3.0f;
	if (ansi)
		CHECK(yt_present_color(current, &result) == YT_PRESENT_OK);
	memset(pager, 0, sizeof(*pager));
	pager->foreground = 3;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(pager, current, introduction,
	    sizeof(introduction) - 1U, &capture);
	pager->newline_flag = 1.0f;
	pager_fixture_b05d(pager, current, prompt, sizeof(prompt) - 1U,
	    &capture);
	yt_pager_editor_enter(pager, accumulator, sizeof(accumulator));
	CHECK(accumulator[0] == '\0');
	CHECK(yt_present_editor_echo(response, response_length,
	    response, response_length, current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	if (accepted) {
		CHECK(yt_present_line(NULL, 0, current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		current->bold = 1.0f;
		current->blink = 1.0f;
		pager_fixture_b05d(pager, current, agreement,
		    sizeof(agreement) - 1U, &capture);
		CHECK(yt_present_sound(1.0f, current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
	}
	return capture;
}

static struct pager_capture
hostile_bribe_refusal_fixture(bool ansi, const uint8_t *row,
    size_t row_length, struct yt_present_state *current,
    struct yt_pager_state *pager)
{
	struct yt_present_result result;
	struct pager_capture capture;

	*current = state(ansi);
	current->foreground = 3.0f;
	if (ansi)
		CHECK(yt_present_color(current, &result) == YT_PRESENT_OK);
	memset(pager, 0, sizeof(*pager));
	pager->foreground = 3;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current->bold = 1.0f;
	current->blink = 1.0f;
	pager_fixture_b05d(pager, current, row, row_length, &capture);
	return capture;
}

static void
test_hostile_bribe_presentation(void)
{
	static const uint8_t accepted_plain[] =
	    "\r\nWe MAY join up if you pay us enough Ada!\n\r"
	    "You have 100 credits. How much do you offer? -+>30\r\n"
	    "\r\nGood Deal! We join up with you!\n\r\x07";
	static const uint8_t accepted_ansi[] =
	    "\r\nWe MAY join up if you pay us enough Ada!\n\r"
	    "You have 100 credits. How much do you offer? -+>30\r\n"
	    "\r\n\x1b[0;33;40;5;1mGood Deal! We join up with you!\n\r"
	    "\x1b[MBO4L32P32CP64CP64CP64L16EP64L32CP64L12E\x0e";
	static const uint8_t empty_offer[] =
	    "\r\nWe MAY join up if you pay us enough Ada!\n\r"
	    "You have 100 credits. How much do you offer? -+>\r\n";
	static const uint8_t planet[] =
	    "Scram Ada, This planet is OURS!";
	static const uint8_t planet_plain[] =
	    "\r\nScram Ada, This planet is OURS!\n\r";
	static const uint8_t planet_ansi[] =
	    "\r\n\x1b[0;33;40;5;1mScram Ada, This planet is OURS!\n\r";
	static const uint8_t ordinary[] =
	    "We don't accept no Bribes Ada!";
	static const uint8_t ordinary_plain[] =
	    "\r\nWe don't accept no Bribes Ada!\n\r";
	static const uint8_t ordinary_ansi[] =
	    "\r\n\x1b[0;33;40;5;1mWe don't accept no Bribes Ada!\n\r";
	static const uint8_t life[] =
	    "We just want your miserable life Ada!";
	static const uint8_t life_plain[] =
	    "\r\nWe just want your miserable life Ada!\n\r";
	static const uint8_t life_ansi[] =
	    "\r\n\x1b[0;33;40;5;1mWe just want your miserable life Ada!\n\r";
	static const uint8_t insult[] =
	    "You insult us Ada! Prepare to DIE!";
	static const uint8_t rejected_plain[] =
	    "\r\nWe MAY join up if you pay us enough Ada!\n\r"
	    "You have 100 credits. How much do you offer? -+>10\r\n"
	    "\r\nYou insult us Ada! Prepare to DIE!\n\r";
	static const uint8_t rejected_ansi[] =
	    "\r\nWe MAY join up if you pay us enough Ada!\n\r"
	    "You have 100 credits. How much do you offer? -+>10\r\n"
	    "\r\n\x1b[0;33;40;5;1mYou insult us Ada! Prepare to DIE!\n\r";
	static const uint8_t offer[] = "30";
	static const uint8_t rejected_offer[] = "10";
	struct yt_present_state current;
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;

	capture = hostile_bribe_offer_fixture(false, offer,
	    sizeof(offer) - 1U, true, &current, &pager);
	CHECK(capture.remote_length == sizeof(accepted_plain) - 1U
	    && memcmp(capture.remote, accepted_plain,
	    sizeof(accepted_plain) - 1U) == 0);
	CHECK(capture.remote_length == 132U && pager.line_count == 1.0f
	    && current.foreground == 3.0f && current.bold == 1.0f
	    && current.blink == 1.0f);

	capture = hostile_bribe_offer_fixture(true, offer,
	    sizeof(offer) - 1U, true, &current, &pager);
	CHECK(capture.remote_length == sizeof(accepted_ansi) - 1U
	    && memcmp(capture.remote, accepted_ansi,
	    sizeof(accepted_ansi) - 1U) == 0);
	CHECK(capture.remote_length == 188U);
	CHECK(pager.line_count == 1.0f);
	CHECK(current.foreground == 3.0f && current.background == 0.0f);
	CHECK(current.bold == 0.0f && current.blink == 0.0f);
	CHECK(capture.last_local_foreground == 7
	    && capture.last_local_background == 0);

	capture = hostile_bribe_offer_fixture(false, NULL, 0, false,
	    &current, &pager);
	CHECK(capture.remote_length == sizeof(empty_offer) - 1U
	    && memcmp(capture.remote, empty_offer,
	    sizeof(empty_offer) - 1U) == 0);
	CHECK(capture.remote_length == 94U && pager.line_count == 0.0f);
	capture = hostile_bribe_offer_fixture(true, NULL, 0, false,
	    &current, &pager);
	CHECK(capture.remote_length == sizeof(empty_offer) - 1U
	    && memcmp(capture.remote, empty_offer,
	    sizeof(empty_offer) - 1U) == 0);

	capture = hostile_bribe_refusal_fixture(false, planet,
	    sizeof(planet) - 1U, &current, &pager);
	CHECK(capture.remote_length == sizeof(planet_plain) - 1U
	    && memcmp(capture.remote, planet_plain,
	    sizeof(planet_plain) - 1U) == 0);
	CHECK(capture.remote_length == 35U && pager.line_count == 1.0f);
	capture = hostile_bribe_refusal_fixture(true, planet,
	    sizeof(planet) - 1U, &current, &pager);
	CHECK(capture.remote_length == sizeof(planet_ansi) - 1U
	    && memcmp(capture.remote, planet_ansi,
	    sizeof(planet_ansi) - 1U) == 0);
	CHECK(capture.remote_length == 49U && pager.line_count == 1.0f);

	capture = hostile_bribe_refusal_fixture(false, ordinary,
	    sizeof(ordinary) - 1U, &current, &pager);
	CHECK(capture.remote_length == sizeof(ordinary_plain) - 1U
	    && memcmp(capture.remote, ordinary_plain,
	    sizeof(ordinary_plain) - 1U) == 0);
	CHECK(capture.remote_length == 34U && pager.line_count == 1.0f);
	capture = hostile_bribe_refusal_fixture(true, ordinary,
	    sizeof(ordinary) - 1U, &current, &pager);
	CHECK(capture.remote_length == sizeof(ordinary_ansi) - 1U
	    && memcmp(capture.remote, ordinary_ansi,
	    sizeof(ordinary_ansi) - 1U) == 0);
	CHECK(capture.remote_length == 48U && pager.line_count == 1.0f);

	capture = hostile_bribe_refusal_fixture(false, life,
	    sizeof(life) - 1U, &current, &pager);
	CHECK(capture.remote_length == sizeof(life_plain) - 1U
	    && memcmp(capture.remote, life_plain,
	    sizeof(life_plain) - 1U) == 0);
	CHECK(capture.remote_length == 41U && pager.line_count == 1.0f);
	capture = hostile_bribe_refusal_fixture(true, life,
	    sizeof(life) - 1U, &current, &pager);
	CHECK(capture.remote_length == sizeof(life_ansi) - 1U
	    && memcmp(capture.remote, life_ansi,
	    sizeof(life_ansi) - 1U) == 0);
	CHECK(capture.remote_length == 55U && pager.line_count == 1.0f);

	capture = hostile_bribe_offer_fixture(false, rejected_offer,
	    sizeof(rejected_offer) - 1U, false, &current, &pager);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.bold = 1.0f;
	current.blink = 1.0f;
	pager_fixture_b05d(&pager, &current, insult, sizeof(insult) - 1U,
	    &capture);
	CHECK(capture.remote_length == sizeof(rejected_plain) - 1U
	    && memcmp(capture.remote, rejected_plain,
	    sizeof(rejected_plain) - 1U) == 0);
	CHECK(capture.remote_length == 134U && pager.line_count == 1.0f);

	capture = hostile_bribe_offer_fixture(true, rejected_offer,
	    sizeof(rejected_offer) - 1U, false, &current, &pager);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.bold = 1.0f;
	current.blink = 1.0f;
	pager_fixture_b05d(&pager, &current, insult, sizeof(insult) - 1U,
	    &capture);
	CHECK(capture.remote_length == sizeof(rejected_ansi) - 1U
	    && memcmp(capture.remote, rejected_ansi,
	    sizeof(rejected_ansi) - 1U) == 0);
	CHECK(capture.remote_length == 148U && pager.line_count == 1.0f);
}

static void
test_hostile_sector_mine_presentation(void)
{
	static const uint8_t prompt[] =
	    "You have 5 mines. Drop how many? [0] -=>";
	static const uint8_t success[] = "Sector 733 is now mined!";
	static const uint8_t accepted_plain[] =
	    "\r\nYou have 5 mines. Drop how many? [0] -=>2\r\n"
	    "\r\nSector 733 is now mined!\n\r";
	static const uint8_t accepted_ansi[] =
	    "\r\nYou have 5 mines. Drop how many? [0] -=>2\r\n"
	    "\x1b[0;36;40m\r\n"
	    "\x1b[0;36;40;5;1mSector 733 is now mined!\n\r"
	    "\x1b[MBT128O5L48P64CP64C\x0e";
	static const uint8_t cancelled[] =
	    "\r\nYou have 5 mines. Drop how many? [0] -=>\r\n";
	static const uint8_t none[] = "You don't HAVE any!";
	static const uint8_t none_plain[] = "\r\nYou don't HAVE any!\n\r";
	static const uint8_t none_ansi[] =
	    "\r\n\x1b[0;33;40;5;1mYou don't HAVE any!\n\r";
	static const uint8_t union_row[] =
	    "The Union doesnt like the home 7 sectors mined!";
	static const uint8_t union_plain[] =
	    "\r\nThe Union doesnt like the home 7 sectors mined!\n\r";
	static const uint8_t union_ansi[] =
	    "\r\n\x1b[0;33;40;5;1m"
	    "The Union doesnt like the home 7 sectors mined!\n\r";
	static const uint8_t two[] = "2";
	struct yt_present_state current;
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	char accumulator[80] = "D";
	bool ansi;
	int pass;

	for (pass = 0; pass < 2; ++pass) {
		ansi = pass != 0;
		current = state(ansi);
		current.foreground = 3.0f;
		if (ansi)
			CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
		memset(&pager, 0, sizeof(pager));
		pager.foreground = 3;
		memset(&capture, 0, sizeof(capture));
		CHECK(yt_present_line(NULL, 0, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		pager.newline_flag = 1.0f;
		pager_fixture_b05d(&pager, &current, prompt,
		    sizeof(prompt) - 1U, &capture);
		yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
		CHECK(yt_present_editor_echo(two, sizeof(two) - 1U,
		    two, sizeof(two) - 1U, &current, &result) == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		CHECK(yt_present_line(NULL, 0, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		current.foreground = 6.0f;
		pager.foreground = 6;
		CHECK(yt_present_line(NULL, 0, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		current.bold = 1.0f;
		current.blink = 1.0f;
		pager_fixture_b05d(&pager, &current, success,
		    sizeof(success) - 1U, &capture);
		CHECK(yt_present_sound(4.0f, &current, &result)
		    == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
		if (ansi) {
			CHECK(capture.remote_length == sizeof(accepted_ansi) - 1U
			    && memcmp(capture.remote, accepted_ansi,
			    sizeof(accepted_ansi) - 1U) == 0);
			CHECK(capture.remote_length == 119U);
		}
		else {
			CHECK(capture.remote_length == sizeof(accepted_plain) - 1U
			    && memcmp(capture.remote, accepted_plain,
			    sizeof(accepted_plain) - 1U) == 0);
			CHECK(capture.remote_length == 73U);
		}
		CHECK(pager.line_count == 1.0f
		    && current.foreground == 6.0f);
		if (ansi)
			CHECK(current.bold == 0.0f && current.blink == 0.0f);
		else
			CHECK(current.bold == 1.0f && current.blink == 1.0f);
	}

	current = state(true);
	current.foreground = 3.0f;
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 3;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, prompt, sizeof(prompt) - 1U,
	    &capture);
	yt_pager_editor_enter(&pager, accumulator, sizeof(accumulator));
	CHECK(yt_present_editor_echo(NULL, 0, NULL, 0, &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(capture.remote_length == sizeof(cancelled) - 1U
	    && memcmp(capture.remote, cancelled, sizeof(cancelled) - 1U) == 0);
	CHECK(capture.remote_length == 44U && pager.line_count == 0.0f);

	capture = hostile_bribe_refusal_fixture(false, none,
	    sizeof(none) - 1U, &current, &pager);
	CHECK(capture.remote_length == sizeof(none_plain) - 1U
	    && memcmp(capture.remote, none_plain,
	    sizeof(none_plain) - 1U) == 0);
	capture = hostile_bribe_refusal_fixture(true, none,
	    sizeof(none) - 1U, &current, &pager);
	CHECK(capture.remote_length == sizeof(none_ansi) - 1U
	    && memcmp(capture.remote, none_ansi,
	    sizeof(none_ansi) - 1U) == 0);

	capture = hostile_bribe_refusal_fixture(false, union_row,
	    sizeof(union_row) - 1U, &current, &pager);
	CHECK(capture.remote_length == sizeof(union_plain) - 1U
	    && memcmp(capture.remote, union_plain,
	    sizeof(union_plain) - 1U) == 0);
	capture = hostile_bribe_refusal_fixture(true, union_row,
	    sizeof(union_row) - 1U, &current, &pager);
	CHECK(capture.remote_length == sizeof(union_ansi) - 1U
	    && memcmp(capture.remote, union_ansi,
	    sizeof(union_ansi) - 1U) == 0);
}

static void
test_startup_pre_admission_presentation(void)
{
	static const uint8_t expected[] =
	    "\x1b[0;35;40m\r\nInitializing...\n\r\r\nWelcome John!\n\r"
	    "Searching my records for your name.\n\r";
	static const uint8_t lockout_ansi[] =
	    "\x1b[0;35;40m\r\nInitializing...\n\r\r\n"
	    "\x1b[0;35;40;1m\aYOUR ACCESS TO THIS GAME HAS BEEN REVOKED!\a"
	    "\r\n\x1b[0;35;40;1m"
	    "Please contact your sysop Jane Sysop.\r\n";
	static const uint8_t lockout_plain[] =
	    "\r\nInitializing...\n\r\r\n"
	    "\aYOUR ACCESS TO THIS GAME HAS BEEN REVOKED!\a\r\n"
	    "Please contact your sysop Jane Sysop.\r\n";
	static const uint8_t revoked[] =
	    "\aYOUR ACCESS TO THIS GAME HAS BEEN REVOKED!\a";
	static const uint8_t contact[] =
	    "Please contact your sysop Jane Sysop.";
	struct yt_present_state current = state(true);
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;

	current.foreground = 6.0f;
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	current.foreground = 5.0f;
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 5;
	pager.nonstop = 1.0f;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current,
	    (const uint8_t *)"Initializing...", strlen("Initializing..."),
	    &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current,
	    (const uint8_t *)"Welcome John!", strlen("Welcome John!"),
	    &capture);
	pager_fixture_b05d(&pager, &current,
	    (const uint8_t *)"Searching my records for your name.",
	    strlen("Searching my records for your name."), &capture);
	CHECK(capture.remote_length == sizeof(expected) - 1U
	    && memcmp(capture.remote, expected, sizeof(expected) - 1U) == 0);
	CHECK(pager.line_count == 3.0f && pager.nonstop == 1.0f
	    && pager.newline_flag == 0.0f && pager.foreground == 5);
	CHECK(current.foreground == 5.0f);

	current = state(true);
	current.foreground = 6.0f;
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	current.foreground = 5.0f;
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 5;
	pager.nonstop = 1.0f;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current,
	    (const uint8_t *)"Initializing...", strlen("Initializing..."),
	    &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_bold_line(revoked, sizeof(revoked) - 1U, &current,
	    &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_bold_line(contact, sizeof(contact) - 1U, &current,
	    &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(sizeof(lockout_ansi) - 1U == 140U
	    && capture.remote_length == sizeof(lockout_ansi) - 1U
	    && memcmp(capture.remote, lockout_ansi,
	    sizeof(lockout_ansi) - 1U) == 0);
	CHECK(pager.line_count == 1.0f && pager.nonstop == 1.0f);

	current = state(false);
	current.foreground = 5.0f;
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 5;
	pager.nonstop = 1.0f;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current,
	    (const uint8_t *)"Initializing...", strlen("Initializing..."),
	    &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_bold_line(revoked, sizeof(revoked) - 1U, &current,
	    &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_bold_line(contact, sizeof(contact) - 1U, &current,
	    &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(sizeof(lockout_plain) - 1U == 106U
	    && capture.remote_length == sizeof(lockout_plain) - 1U
	    && memcmp(capture.remote, lockout_plain,
	    sizeof(lockout_plain) - 1U) == 0);
}

static void
test_startup_status_row(void)
{
	static const uint8_t real_name[] = "John Doe";
	static const uint8_t expression[] = " | John Doe | ";
	static const uint8_t login_expression[] = " | John Doe | Pilot";
	uint8_t long_name[80];
	struct yt_present_state current = state(false);
	struct yt_present_result result;
	size_t index;

	current.sound.snoop = 0.0f;
	CHECK(yt_present_status_row(real_name, sizeof(real_name) - 1U,
	    (const uint8_t *)"", 0, &current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == 0 && result.event_count == 0);

	current.sound.snoop = -1.0f;
	CHECK(yt_present_status_row(real_name, sizeof(real_name) - 1U,
	    (const uint8_t *)"", 0, &current, &result) == YT_PRESENT_OK);
	CHECK(result.remote_length == 0 && result.event_count == 10);
	CHECK(result.events[0].operation == YT_PRESENT_LOCAL_LOCATE
	    && result.events[0].row == 25 && result.events[0].column == 1);
	CHECK(result.events[1].operation == YT_PRESENT_LOCAL_COLOR
	    && result.events[1].foreground == 11
	    && result.events[1].background == 1);
	CHECK(result.events[2].operation == YT_PRESENT_LOCAL_SEMI
	    && result.events[2].length == 79);
	for (index = 0; index < result.events[2].length; ++index)
		CHECK(result.events[2].data[index] == ' ');
	CHECK(result.events[3].operation == YT_PRESENT_LOCAL_LOCATE
	    && result.events[3].row == 25 && result.events[3].column == 1);
	CHECK(result.events[4].operation == YT_PRESENT_LOCAL_COLOR
	    && result.events[4].foreground == 14
	    && result.events[4].background == 3);
	CHECK(result.events[5].operation == YT_PRESENT_LOCAL_SEMI
	    && result.events[5].length == 15
	    && memcmp(result.events[5].data, " Yankee Trader ", 15) == 0);
	CHECK(result.events[6].operation == YT_PRESENT_LOCAL_COLOR
	    && result.events[6].foreground == 11
	    && result.events[6].background == 1);
	CHECK(result.events[7].operation == YT_PRESENT_LOCAL_SEMI
	    && result.events[7].length == 1
	    && result.events[7].data[0] == ' ');
	CHECK(result.events[8].operation == YT_PRESENT_LOCAL_SEMI
	    && result.events[8].length == sizeof(expression) - 1U
	    && memcmp(result.events[8].data, expression,
	    sizeof(expression) - 1U) == 0);
	CHECK(result.events[9].operation == YT_PRESENT_LOCAL_COLOR
	    && result.events[9].foreground == 7
	    && result.events[9].background == 0);
	CHECK(yt_present_status_row(real_name, sizeof(real_name) - 1U,
	    (const uint8_t *)"Pilot", 5, &current, &result)
	    == YT_PRESENT_OK);
	CHECK(result.remote_length == 0 && result.event_count == 10
	    && result.events[8].length == sizeof(login_expression) - 1U
	    && memcmp(result.events[8].data, login_expression,
	    sizeof(login_expression) - 1U) == 0);

	memset(long_name, 'R', sizeof(long_name));
	CHECK(yt_present_status_row(long_name, sizeof(long_name),
	    (const uint8_t *)"ignored", 7, &current, &result)
	    == YT_PRESENT_OK);
	CHECK(result.events[8].length == 63
	    && memcmp(result.events[8].data, " | ", 3) == 0);
	for (index = 3; index < result.events[8].length; ++index)
		CHECK(result.events[8].data[index] == 'R');
}

static void
test_new_alias_success_presentation(void)
{
	static const uint8_t expected[] =
	    "\x1b[0;32;40m\r\nYou are a new player.\n\r\r\n"
	    "Enter the FULL alias you wish to use in the game.\n\r\r\n"
	    "Press [ENTER] to use your real name.\n\r-+> Star Lord\r\n"
	    "\x1b[0;33;40m\r\n\x1b[0;33;40;1m"
	    "John Doe a.k.a. Star Lord\n\r\x1b[0;33;40m\r\n"
	    "\x1b[0;36;40mIs this OK (Y/[N])? Y\r\n\r\n"
	    "\x1b[0;36;40;5;1mYour Alias has been recorded. Have fun!"
	    "\n\r\x1b[0;36;40m\r\n";
	static const char alias[] = "Star Lord";
	struct yt_present_state current = state(true);
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct pager_capture capture;
	size_t index;

	current.foreground = 5.0f;
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 5;
	pager.line_count = 3.0f;
	pager.nonstop = 1.0f;
	memset(&capture, 0, sizeof(capture));
	current.foreground = 2.0f;
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current,
	    (const uint8_t *)"You are a new player.",
	    strlen("You are a new player."), &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current,
	    (const uint8_t *)
	    "Enter the FULL alias you wish to use in the game.",
	    strlen("Enter the FULL alias you wish to use in the game."),
	    &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current,
	    (const uint8_t *)"Press [ENTER] to use your real name.",
	    strlen("Press [ENTER] to use your real name."), &capture);
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current, (const uint8_t *)"-+> ", 4,
	    &capture);
	pager.line_count = 0.0f;
	pager.nonstop = 0.0f;
	for (index = 0; index < sizeof(alias) - 1U; ++index) {
		uint8_t byte = (uint8_t)alias[index];

		CHECK(yt_present_editor_echo(&byte, 1, &byte, 1, &current,
		    &result) == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
	}
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);

	current.foreground = 3.0f;
	pager.foreground = 3;
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.bold = 1.0f;
	pager_fixture_b05d(&pager, &current,
	    (const uint8_t *)"John Doe a.k.a. Star Lord",
	    strlen("John Doe a.k.a. Star Lord"), &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.foreground = 6.0f;
	pager.foreground = 6;
	pager.newline_flag = 1.0f;
	pager_fixture_b05d(&pager, &current,
	    (const uint8_t *)"Is this OK (Y/[N])? ",
	    strlen("Is this OK (Y/[N])? "), &capture);
	pager.line_count = 0.0f;
	pager.nonstop = 0.0f;
	{
		uint8_t byte = 'Y';

		CHECK(yt_present_editor_echo(&byte, 1, &byte, 1, &current,
		    &result) == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
	}
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.bold = 1.0f;
	current.blink = 1.0f;
	pager_fixture_b05d(&pager, &current,
	    (const uint8_t *)"Your Alias has been recorded. Have fun!",
	    strlen("Your Alias has been recorded. Have fun!"), &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);

	CHECK(capture.remote_length == sizeof(expected) - 1U
	    && memcmp(capture.remote, expected, sizeof(expected) - 1U) == 0);
	CHECK(pager.line_count == 1.0f && pager.nonstop == 0.0f
	    && pager.newline_flag == 0.0f && pager.foreground == 6);
	CHECK(current.foreground == 6.0f && current.bold == 0.0f
	    && current.blink == 0.0f);
}

static void
test_new_player_admission_presentation(void)
{
	static const uint8_t vacant_expected[] =
	    "\r\nEntering a new player...\n\r\r\n"
	    "Notice: If your ship is dead and you have not played for 14\n\r"
	    "days, it will be deleted to make room for someone else.\n\r"
	    "\r\n\r\nYour ship has been built.\r\n"
	    "Do you want instructions (Y/N) [N]? \r\n";
	static const uint8_t full_expected[] =
	    "\r\nEntering a new player...\n\r\r\n"
	    "\x1b[0;35;40;5;1m"
	    "I'm sorry but the game is full. Try again tomorrow.\n\r";
	static const uint8_t invalid_then_no_expected[] =
	    "\r\nEntering a new player...\n\r\r\n"
	    "Notice: If your ship is dead and you have not played for 14\n\r"
	    "days, it will be deleted to make room for someone else.\n\r"
	    "\r\n\r\nYour ship has been built.\r\n"
	    "Do you want instructions (Y/N) [N]? X\r\n"
	    "\x1b[0;35;40;1m"
	    "Do you want instructions (Y/N) [N]? N"
	    "\x1b[0;35;40m\r\n";
	struct yt_present_state current;
	struct yt_present_state prompt_current;
	struct yt_present_result result;
	struct yt_pager_state pager;
	struct yt_pager_state prompt_pager;
	struct pager_capture capture;
	struct pager_capture prompt_capture;

	current = state(true);
	current.foreground = 5.0f;
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 5;
	pager.line_count = 3.0f;
	pager.nonstop = 1.0f;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current,
	    (const uint8_t *)"Entering a new player...",
	    strlen("Entering a new player..."), &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current,
	    (const uint8_t *)
	    "Notice: If your ship is dead and you have not played for 14",
	    strlen("Notice: If your ship is dead and you have not played for 14"),
	    &capture);
	pager_fixture_b05d(&pager, &current,
	    (const uint8_t *)
	    "days, it will be deleted to make room for someone else.",
	    strlen("days, it will be deleted to make room for someone else."),
	    &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line((const uint8_t *)"Your ship has been built.",
	    strlen("Your ship has been built."), &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_character(
	    (const uint8_t *)"Do you want instructions (Y/N) [N]? ",
	    strlen("Do you want instructions (Y/N) [N]? "), &current,
	    &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	prompt_current = current;
	prompt_pager = pager;
	prompt_capture = capture;
	pager.line_count = 0.0f;
	pager.nonstop = 0.0f;
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(capture.remote_length == sizeof(vacant_expected) - 1U
	    && memcmp(capture.remote, vacant_expected,
	    sizeof(vacant_expected) - 1U) == 0);
	CHECK(pager.line_count == 0.0f && pager.nonstop == 0.0f
	    && pager.foreground == 5);

	current = prompt_current;
	pager = prompt_pager;
	capture = prompt_capture;
	pager.line_count = 0.0f;
	pager.nonstop = 0.0f;
	{
		uint8_t key = 'X';

		CHECK(yt_present_editor_echo(&key, 1, &key, 1, &current,
		    &result) == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
	}
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.bold = 1.0f;
	CHECK(yt_present_character(
	    (const uint8_t *)"Do you want instructions (Y/N) [N]? ",
	    strlen("Do you want instructions (Y/N) [N]? "), &current,
	    &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	{
		uint8_t key = 'N';

		CHECK(yt_present_editor_echo(&key, 1, &key, 1, &current,
		    &result) == YT_PRESENT_OK);
		pager_capture_result(&capture, &result);
	}
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(sizeof(invalid_then_no_expected) - 1U == 279U);
	CHECK(capture.remote_length == sizeof(invalid_then_no_expected) - 1U
	    && memcmp(capture.remote, invalid_then_no_expected,
	    sizeof(invalid_then_no_expected) - 1U) == 0);

	current = state(true);
	current.foreground = 5.0f;
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	memset(&pager, 0, sizeof(pager));
	pager.foreground = 5;
	pager.line_count = 3.0f;
	pager.nonstop = 1.0f;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	pager_fixture_b05d(&pager, &current,
	    (const uint8_t *)"Entering a new player...",
	    strlen("Entering a new player..."), &capture);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.bold = 1.0f;
	current.blink = 1.0f;
	pager_fixture_b05d(&pager, &current,
	    (const uint8_t *)
	    "I'm sorry but the game is full. Try again tomorrow.",
	    strlen("I'm sorry but the game is full. Try again tomorrow."),
	    &capture);
	CHECK(capture.remote_length == sizeof(full_expected) - 1U
	    && memcmp(capture.remote, full_expected,
	    sizeof(full_expected) - 1U) == 0);
}

static void
test_returning_player_presentation(void)
{
	static const uint8_t same_day_alive[] =
	    "\x1b[0;32;40m\r\nYou have been on today.\r\n";
	static const uint8_t xannor_rebuild[] =
	    "\x1b[0;32;40m\r\n\r\n"
	    "\x1b[0;32;40;5;1mYou have been killed by The Xannor!\r\n"
	    "\x1b[0;32;40m\r\nYour ship has been built.\r\n";
	static const uint8_t self_denial[] =
	    "\x1b[0;32;40m\r\nYou have been on today.\r\n\r\n"
	    "You managed to kill yourself on your last time on.\r\n\r\n"
	    "\x1b[0;37;40;5;1m"
	    "You will be allowed to play again tomorrow!\r\n";
	static const uint8_t empty_killer[] =
	    "\x1b[0;32;40;5;1m destroyed your ship!\r\n";
	struct yt_present_state current;
	struct yt_present_result result;
	struct pager_capture capture;

	current = state(true);
	current.foreground = 5.0f;
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	memset(&capture, 0, sizeof(capture));
	current.foreground = 2.0f;
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line((const uint8_t *)"You have been on today.",
	    strlen("You have been on today."), &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(capture.remote_length == sizeof(same_day_alive) - 1U
	    && memcmp(capture.remote, same_day_alive,
	    sizeof(same_day_alive) - 1U) == 0);

	current = state(true);
	current.foreground = 5.0f;
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	memset(&capture, 0, sizeof(capture));
	current.foreground = 2.0f;
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.blink = 1.0f;
	CHECK(yt_present_bold_line(
	    (const uint8_t *)"You have been killed by The Xannor!",
	    strlen("You have been killed by The Xannor!"), &current,
	    &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line((const uint8_t *)"Your ship has been built.",
	    strlen("Your ship has been built."), &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(capture.remote_length == sizeof(xannor_rebuild) - 1U
	    && memcmp(capture.remote, xannor_rebuild,
	    sizeof(xannor_rebuild) - 1U) == 0);

	current = state(true);
	current.foreground = 2.0f;
	current.blink = 1.0f;
	memset(&capture, 0, sizeof(capture));
	CHECK(yt_present_bold_line(
	    (const uint8_t *)" destroyed your ship!",
	    strlen(" destroyed your ship!"), &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(capture.remote_length == sizeof(empty_killer) - 1U
	    && memcmp(capture.remote, empty_killer,
	    sizeof(empty_killer) - 1U) == 0);

	current = state(true);
	current.foreground = 5.0f;
	CHECK(yt_present_color(&current, &result) == YT_PRESENT_OK);
	memset(&capture, 0, sizeof(capture));
	current.foreground = 2.0f;
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line((const uint8_t *)"You have been on today.",
	    strlen("You have been on today."), &current, &result)
	    == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(
	    (const uint8_t *)
	    "You managed to kill yourself on your last time on.",
	    strlen("You managed to kill yourself on your last time on."),
	    &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(yt_present_line(NULL, 0, &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	current.foreground = 7.0f;
	current.blink = 1.0f;
	CHECK(yt_present_bold_line(
	    (const uint8_t *)"You will be allowed to play again tomorrow!",
	    strlen("You will be allowed to play again tomorrow!"),
	    &current, &result) == YT_PRESENT_OK);
	pager_capture_result(&capture, &result);
	CHECK(capture.remote_length == sizeof(self_denial) - 1U
	    && memcmp(capture.remote, self_denial,
	    sizeof(self_denial) - 1U) == 0);
}

int
main(void)
{
	test_xannor_file_playback();
	test_color();
	test_color_process_cache();
	test_color_process_table();
	test_direct_output();
	test_paged_output();
	test_editor_echo();
	test_projectile_parent_presentation();
	test_projectile_ordinary_cycle_presentation();
	test_counterlaunch_presentation();
	test_xannor_retaliation_presentation();
	test_projectile_refusal_presentation();
	test_projectile_early_terminal_presentation();
	test_pager_transactions();
	test_pager_gates();
	test_pager_raw_process_cells();
	test_file_viewer_pager_join();
	test_startup_ascii_physical_join();
	test_instruction_physical_viewer_join();
	test_newspaper_physical_viewer_join();
	test_newspaper_endpoint_modes();
	test_newspaper_pagination_and_ctrl_x();
	test_computer_newspaper_low_time_cycles();
	test_computer_newspaper_queue_cycles();
	test_computer_newspaper_missing_recovery_cycles();
	test_scoreboard_physical_viewer_join();
	test_scoreboard_endpoint_modes();
	test_normal_exit_scoreboard_viewer_join();
	test_sector_private_pager();
	test_sector_scanner_rows();
	test_radio_private_pager();
	test_radio_reader_presentation();
	test_editor_aux_notices();
	test_editor_terminal_notices();
	test_common_fatal_notice();
	test_formatting_wrappers();
	test_attention();
	test_sound_toggle();
	test_sysop_time();
	test_sysop_chat_header();
	test_basic_fault_registry();
	test_basic_fault_projection();
	test_main_error_model();
	test_shared_error_model();
	test_serial_startup_output();
	test_time_helpers();
	test_opening_streamer();
	test_press_any_key_presentation();
	test_post_login_press_presentation();
	test_gameplay_reentry_hostile_warning();
	test_hostile_menu_presentation();
	test_hostile_quit_presentation();
	test_direct_emergency_warp_presentation();
	test_team_front_presentation();
	test_team_create_presentation();
	test_team_join_presentation();
	test_team_quit_presentation();
	test_team_resource_presentation();
	test_team_banish_presentation();
	test_port_docking_controller_presentation();
	test_action_finalizer_presentation();
	test_commodity_trade_presentation();
	test_commodity_trade_branch_presentation();
	test_commodity_trade_adapter_cuts();
	test_commodity_trade_recursive_pager();
	test_port_report_b05d_adapter_cuts();
	test_computer_return_prompt_presentation();
	test_computer_quit_cancel_presentation();
	test_main_quit_cancel_presentation();
	test_main_shell_front_presentation();
	test_main_shell_branch_presentation();
	test_planet_quit_cancel_presentation();
	test_planet_menu_front_presentation();
	test_planet_take_all_default_cycle_presentation();
	test_planet_take_one_presentation();
	test_planet_transfer_presentation();
	test_planet_bank_presentation();
	test_planet_productivity_presentation();
	test_clearance_presentation();
	test_earth_report_presentation();
	test_earth_purchase_presentation();
	test_earth_anti_cloak_presentation();
	test_earth_spy_purchase_presentation();
	test_earth_lottery_loss_presentation();
	test_planet_computer_entry_cycle_presentation();
	test_planet_display_cycle_presentation();
	test_planet_sensor_all_zero_cycle_presentation();
	test_planet_rename_presentation();
	test_normal_exit_tail_presentation();
	test_computer_deactivation_presentation();
	test_computer_sensor_all_zero_cycle_presentation();
	test_computer_profit_cycle_presentation();
	test_computer_front_presentation();
	test_computer_avoid_presentation();
	test_computer_port_report_presentation();
	test_computer_port_report_short_cycles_presentation();
	test_computer_port_report_wrapper_b05d_cuts();
	test_computer_port_report_terminal_presentation();
	test_computer_avoid_terminal_presentation();
	test_computer_avoid_b05d_cuts();
	test_computer_port_report_ab36_state_joins();
	test_computer_port_report_earth_cycle_presentation();
	test_computer_port_report_earth_failure_presentation();
	test_computer_port_report_ordinary_failure_presentation();
	test_computer_port_report_ordinary_cycle_presentation();
	test_computer_port_report_low_time_cycle_presentation();
	test_computer_port_report_low_time_retry_cycle_presentation();
	test_computer_planet_report_front_presentation();
	test_computer_planet_inventory_presentation();
	test_owned_planets_transaction();
	test_computer_finders_presentation();
	test_computer_treasury_presentation();
	test_main_fighters_presentation();
	test_main_genesis_presentation();
	test_planet_garrison_presentation();
	test_planet_landing_presentation();
	test_planet_assault_presentation();
	test_planet_creation_presentation();
	test_planet_move_presentation();
	test_sector_mine_presentation();
	test_direct_fighter_kill_warning_presentation();
	test_direct_fighter_kill_composition_presentation();
	test_info_panel_presentation();
	test_full_normal_exit_presentation();
	test_direct_fighter_fatal_cycle_presentation();
	test_computer_info_cycle_presentation();
	test_planet_info_cycle_presentation();
	test_planet_info_promotion_refresh_cycle_presentation();
	test_planet_info_captain_route_cycles_presentation();
	test_planet_sensor_nonzero_cycle_presentation();
	test_planet_garrison_positive_cycle_presentation();
	test_planet_bank_cancel_cycle_presentation();
	test_planet_productivity_blank_cycle_presentation();
	test_planet_transfer_cancel_cycle_presentation();
	test_planet_transfer_no_cargo_cycle_presentation();
	test_planet_transfer_cargo_cycle_presentation();
	test_planet_transfer_fighter_cycle_presentation();
	test_planet_transfer_direct_cycles_presentation();
	test_planet_transfer_remaining_cycles_presentation();
	test_planet_rename_protected_cycle_presentation();
	test_planet_take_one_accepted_cycle_presentation();
	test_planet_take_one_blank_default_cycle_presentation();
	test_planet_take_one_e_default_cycle_presentation();
	test_planet_take_one_zero_cycle_presentation();
	test_planet_take_one_stock_error_cycle_presentation();
	test_planet_take_one_capacity_error_cycle_presentation();
	test_planet_take_one_negative_error_cycle_presentation();
	test_planet_leave_cycle_presentation();
	test_planet_thrusters_accepted_cycle_presentation();
	test_main_movement_accepted_cycle_presentation();
	test_main_attack_survivor_cycle_presentation();
	test_main_attack_black_hole_cycle_presentation();
	test_main_attack_mine_cycle_presentation();
	test_main_attack_mine_warp_cycle_presentation();
	test_planet_movement_accepted_cycle_presentation();
	test_planet_port_no_port_cycle_presentation();
	test_planet_port_refusal_cycle_presentation();
	test_docking_earth_leave_cycle_presentation();
	test_computer_quit_accept_presentation();
	test_planet_quit_accept_presentation();
	test_hostile_quit_accept_presentation();
	test_main_quit_accept_handoff_presentation();
	test_quit_invalid_retry_typeahead_presentation();
	test_quit_valid_typeahead_presentation();
	test_quit_heading_sample_typeahead_presentation();
	test_spy_sweep_presentation();
	test_black_hole_presentation();
	test_movement_presentation();
	test_direct_attack_presentation();
	test_computer_spy_presentation();
	test_computer_path_presentation();
	test_computer_path_start_terminal_presentation();
	test_computer_autopilot_destination_terminal_presentation();
	test_computer_path_destination_terminal_presentation();
	test_computer_autopilot_presentation();
	test_computer_autopilot_alternate_presentation();
	test_computer_autopilot_confirmation_terminal_presentation();
	test_computer_navigation_direct_b05d_carrier_prefixes();
	test_computer_scoreboard_presentation();
	test_computer_scoreboard_full_cycle_presentation();
	test_computer_newspaper_full_cycle_presentation();
	test_computer_newspaper_carrier_prefixes();
	test_computer_newspaper_terminal_presentation();
	test_radio_target_blank_presentation();
	test_computer_radio_composer_cycle_presentation();
	test_radio_body_presentation();
	test_radio_body_cleanup_presentation();
	test_computer_radio_log_presentation();
	test_computer_newspaper_presentation();
	test_hostile_attack_admission_presentation();
	test_deployed_fighter_surrender_presentation();
	test_deployed_fighter_faction_presentation();
	test_shield_spill_presentation();
	test_hostile_bribe_presentation();
	test_hostile_sector_mine_presentation();
	test_startup_pre_admission_presentation();
	test_startup_status_row();
	test_new_alias_success_presentation();
	test_new_player_admission_presentation();
	test_returning_player_presentation();
	if (failures != 0) {
		fprintf(stderr, "%u test(s) failed\n", failures);
		return 1;
	}
	puts("test_presentation: ok");
	return 0;
}
