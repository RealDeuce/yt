#include "yt_sound.h"
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

static struct yt_sound_state
state(bool ansi)
{
	struct yt_sound_state value;

	memset(&value, 0, sizeof(value));
	value.ansi = ansi ? 1.0f : 0.0f;
	value.user_sound = -1.0f;
	value.snoop = -1.0f;
	value.local_sound = -1.0f;
	return value;
}

static void
test_known_cues(void)
{
	static const char *const expected[] = {
		"",
		"MBO4L32P32CP64CP64CP64L16EP64L32CP64L12E",
		"MBO1L64P32CEDFEGFAGBAO5BAGFEDC",
		"MBO2L2P32CL3CL8CP32L3CP6E-L8DL3DL8CL3CO1L8BO2L1C",
		"MBT128O5L48P64CP64C",
		"MBO1L64P8CdGCdGCdGCDGCGD",
		"MBO2T200L64FBEAP8FBEAP8FBEAP4FBEAP8FBEAP8FBEAP4T128",
		"MBO6L64T128GADO3BAGFEDCO2BAGFEDCO1BAGFEDCP16",
		"MBO4T128L64CGCGCGP16CGCGCGP16CGCGCGP16",
		"MBT64L64O3CEFADFGL16B"
	};
	int selector;

	for (selector = 1; selector <= 9; ++selector) {
		struct yt_sound_state current = state(true);
		struct yt_sound_result result;
		size_t length = strlen(expected[selector]);

		CHECK(yt_sound_dispatch((float)selector, &current, &result)
		    == YT_SOUND_OK);
		CHECK(result.remote_length == length + 3U);
		CHECK(result.remote[0] == 0x1b && result.remote[1] == '[');
		CHECK(memcmp(result.remote + 2, expected[selector], length) == 0);
		CHECK(result.remote[length + 2U] == 0x0e);
		CHECK(result.play_length == length);
		CHECK(memcmp(result.play, expected[selector], length) == 0);
		CHECK(current.scratch_length == 0);
	}
}

static void
test_branches_and_gates(void)
{
	struct yt_sound_state current = state(false);
	struct yt_sound_result result;

	memcpy(current.scratch, "stale", 5);
	current.scratch_length = 5;
	CHECK(yt_sound_dispatch(0.0f, &current, &result) == YT_SOUND_OK);
	CHECK(result.remote_length == 0U && result.play_length == 0U
	    && current.scratch_length == 5U);
	CHECK(yt_sound_dispatch(2.0f, &current, &result) == YT_SOUND_OK);
	CHECK(result.remote_length == 0 && result.play_length == 0);
	CHECK(current.scratch_length == 5);
	CHECK(yt_sound_dispatch(4.0f, &current, &result) == YT_SOUND_OK);
	CHECK(result.remote_length == 0 && result.play_length == 0);
	CHECK(current.scratch_length == 5);
	CHECK(yt_sound_dispatch(2.5f, &current, &result) == YT_SOUND_OK);
	CHECK(result.remote_length == 1U && result.remote[0] == 0x07
	    && result.play_length == 11U && current.scratch_length == 5U);
	CHECK(yt_sound_dispatch(8.0f, &current, &result) == YT_SOUND_OK);
	CHECK(result.remote_length == 1 && result.remote[0] == 0x07);
	CHECK(result.play_length == 11);
	CHECK(memcmp(result.play, "T255L63o4be", 11) == 0);
	CHECK(current.scratch_length == 5);

	current = state(false);
	current.mode = 1.0f;
	CHECK(yt_sound_dispatch(8.0f, &current, &result) == YT_SOUND_OK);
	CHECK(result.remote_length == 0 && result.play_length == 11);
	current.snoop = 2.0f;
	current.local_sound = 1.0f;
	current.mode = 0.0f;
	CHECK(yt_sound_dispatch(8.0f, &current, &result) == YT_SOUND_OK);
	CHECK(result.remote_length == 1 && result.play_length == 0);
}

static void
test_stale_and_failures(void)
{
	struct yt_sound_state current = state(true);
	struct yt_sound_result result;

	memcpy(current.scratch, "stale", 5);
	current.scratch_length = 5;
	CHECK(yt_sound_dispatch(10.0f, &current, &result) == YT_SOUND_OK);
	CHECK(memcmp(result.remote, "\x1b[stale\x0e", 8) == 0);
	CHECK(memcmp(result.play, "stale", 5) == 0);
	CHECK(current.scratch_length == 0);
	current = state(true);
	current.mode = 2.0f;
	current.snoop = 0.0f;
	memcpy(current.scratch, "stale", 5U);
	current.scratch_length = 5U;
	CHECK(yt_sound_dispatch(1.5f, &current, &result) == YT_SOUND_OK);
	CHECK(result.remote_length == 0U && result.play_length == 0U
	    && current.scratch_length == 0U);

	current = state(true);
	current.user_sound = 40000.0f;
	CHECK(yt_sound_dispatch(1.0f, &current, &result)
	    == YT_SOUND_USER_OVERFLOW);
	CHECK(current.scratch_length > 0 && result.remote_length == 0);
	current = state(true);
	current.snoop = 40000.0f;
	CHECK(yt_sound_dispatch(1.0f, &current, &result)
	    == YT_SOUND_SNOOP_OVERFLOW);
	CHECK(result.remote_length > 0 && current.scratch_length > 0);
	current = state(true);
	current.local_sound = 40000.0f;
	CHECK(yt_sound_dispatch(1.0f, &current, &result)
	    == YT_SOUND_LOCAL_OVERFLOW);
	CHECK(result.remote_length > 0 && current.scratch_length > 0);
	current = state(false);
	current.mode = 1.0f;
	current.user_sound = 40000.0f;
	CHECK(yt_sound_dispatch(1.0f, &current, &result) == YT_SOUND_OK);
	CHECK(result.remote_length == 0);

	current = state(true);
	current.scratch_length = sizeof(current.scratch) + 1U;
	CHECK(yt_sound_dispatch(10.0f, &current, &result)
	    == YT_SOUND_INVALID_STATE);
	CHECK(result.remote_length == 0 && result.play_length == 0);
	current = state(true);
	CHECK(yt_sound_dispatch(1.0e30f, &current, &result) == YT_SOUND_OK);
	CHECK(result.remote_length == 3 && result.play_length == 0);
}

static void
test_conversion_mode(void)
{
	struct yt_sound_state current = state(false);
	struct yt_sound_result result;

	current.user_sound = 0.0f;
	current.snoop = -1.4999f;
	current.local_sound = 1.0f;
	CHECK(yt_sound_dispatch(1.0f, &current, &result) == YT_SOUND_OK);
	CHECK(result.play_length == 11);
	current.conversion_mode = 4;
	CHECK(yt_sound_dispatch(1.0f, &current, &result) == YT_SOUND_OK);
	CHECK(result.play_length == 0);
}

static void
test_toggle(void)
{
	static const struct {
		float prior;
		float toggled;
		const char *line;
	} nearest[] = {
		{0.0f, -1.0f, "Sound ON"},
		{-0.5f, 0.0f, "Sound OFF"},
		{-1.0f, 0.0f, "Sound OFF"},
		{1.0f, -2.0f, "Sound ON"},
		{-2.0f, 1.0f, "Sound ON"},
		{2.5f, -4.0f, "Sound ON"},
		{-2.5f, 2.0f, "Sound ON"},
		{1.5f, -3.0f, "Sound ON"},
		{-1.5f, 1.0f, "Sound ON"},
		{-0.5001f, 0.0f, "Sound OFF"},
		{-1.4999f, 0.0f, "Sound OFF"},
		{32767.0f, -32768.0f, "Sound ON"},
		{-32768.0f, 32767.0f, "Sound ON"}
	};
	size_t index;

	for (index = 0; index < sizeof(nearest) / sizeof(nearest[0]); ++index) {
		struct yt_sound_state current = state(true);
		struct yt_sound_result result;
		size_t line_length = strlen(nearest[index].line);

		current.user_sound = nearest[index].prior;
		CHECK(yt_sound_toggle(&current, &result) == YT_SOUND_OK);
		CHECK(current.user_sound == nearest[index].toggled);
		CHECK(current.local_sound == -1.0f);
		CHECK(result.line_length == line_length);
		CHECK(memcmp(result.line, nearest[index].line,
		    line_length) == 0);
		CHECK(result.remote_length ==
		    (nearest[index].toggled != 0.0f ? 43U : 0U));
	}
	{
		struct yt_sound_state current = state(false);
		struct yt_sound_result result;

		current.conversion_mode = 4;
		current.user_sound = 2.5f;
		CHECK(yt_sound_toggle(&current, &result) == YT_SOUND_OK);
		CHECK(current.user_sound == -3.0f);
		current.mode = 1.0f;
		current.user_sound = 0.0f;
		current.local_sound = 77.0f;
		CHECK(yt_sound_toggle(&current, &result) == YT_SOUND_OK);
		CHECK(current.user_sound == -1.0f);
		CHECK(current.local_sound == -1.0f);
		CHECK(result.remote_length == 0 && result.play_length == 11);
		current.user_sound = -1.0f;
		memcpy(current.scratch, "stale", 5);
		current.scratch_length = 5;
		CHECK(yt_sound_toggle(&current, &result) == YT_SOUND_OK);
		CHECK(current.user_sound == 0.0f);
		CHECK(current.local_sound == 0.0f);
		CHECK(current.scratch_length == 5);
		CHECK(result.remote_length == 0 && result.play_length == 0);
		current.user_sound = 40000.0f;
		current.local_sound = 77.0f;
		CHECK(yt_sound_toggle(&current, &result)
		    == YT_SOUND_USER_OVERFLOW);
		CHECK(current.user_sound == 40000.0f);
		CHECK(current.local_sound == 77.0f);
		CHECK(result.line_length == 0);
	}
}

int
main(void)
{
	test_known_cues();
	test_branches_and_gates();
	test_stale_and_failures();
	test_conversion_mode();
	test_toggle();
	if (failures != 0) {
		fprintf(stderr, "%u test(s) failed\n", failures);
		return 1;
	}
	puts("test_sound: ok");
	return 0;
}
