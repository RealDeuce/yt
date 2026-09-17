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
	value.ansi = ansi;
	value.user_sound = true;
	value.local_output = true;
	value.local_sound = true;
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

		CHECK(yt_sound_dispatch((enum yt_sound_cue)selector,
		    &current, &result)
		    == YT_SOUND_OK);
		CHECK(result.remote_length == length + 3U);
		CHECK(result.remote[0] == 0x1b && result.remote[1] == '[');
		CHECK(memcmp(result.remote + 2, expected[selector], length) == 0);
		CHECK(result.remote[length + 2U] == 0x0e);
		CHECK(result.play_length == length);
		CHECK(memcmp(result.play, expected[selector], length) == 0);
	}
}

static void
test_branches_and_gates(void)
{
	struct yt_sound_state current = state(false);
	struct yt_sound_result result;

	CHECK(yt_sound_dispatch(YT_SOUND_CUE_ATTACK, &current, &result) == YT_SOUND_OK);
	CHECK(result.remote_length == 0 && result.play_length == 0);
	CHECK(yt_sound_dispatch(YT_SOUND_CUE_ACTION, &current, &result) == YT_SOUND_OK);
	CHECK(result.remote_length == 0 && result.play_length == 0);
	CHECK(yt_sound_dispatch(YT_SOUND_CUE_DANGER, &current, &result) == YT_SOUND_OK);
	CHECK(result.remote_length == 1 && result.remote[0] == 0x07);
	CHECK(result.play_length == 11);
	CHECK(memcmp(result.play, "T255L63o4be", 11) == 0);

	current = state(false);
	current.local_mode = true;
	CHECK(yt_sound_dispatch(YT_SOUND_CUE_DANGER, &current, &result) == YT_SOUND_OK);
	CHECK(result.remote_length == 0 && result.play_length == 11);
	current.local_output = false;
	current.local_sound = true;
	current.local_mode = false;
	CHECK(yt_sound_dispatch(YT_SOUND_CUE_DANGER, &current, &result) == YT_SOUND_OK);
	CHECK(result.remote_length == 1 && result.play_length == 0);
}

static void
test_invalid_cue(void)
{
	struct yt_sound_state current = state(true);
	struct yt_sound_result result;

	CHECK(yt_sound_dispatch((enum yt_sound_cue)0, &current, &result)
	    == YT_SOUND_INVALID_CUE);
	CHECK(result.remote_length == 0U && result.play_length == 0U);
}

static void
test_toggle(void)
{
	struct yt_sound_state current = state(true);
	struct yt_sound_result result;

	CHECK(yt_sound_toggle(&current, &result) == YT_SOUND_OK);
	CHECK(!current.user_sound && current.local_sound);
	CHECK(result.line_length == 9U
	    && memcmp(result.line, "Sound OFF", 9U) == 0);
	CHECK(result.remote_length == 0U && result.play_length == 0U);
	CHECK(yt_sound_toggle(&current, &result) == YT_SOUND_OK);
	CHECK(current.user_sound && current.local_sound);
	CHECK(result.line_length == 8U
	    && memcmp(result.line, "Sound ON", 8U) == 0);
	CHECK(result.remote_length == 43U && result.play_length == 40U);

	current = state(false);
	current.local_mode = true;
	CHECK(yt_sound_toggle(&current, &result) == YT_SOUND_OK);
	CHECK(!current.user_sound && !current.local_sound);
	CHECK(result.remote_length == 0U && result.play_length == 0U);
	CHECK(yt_sound_toggle(&current, &result) == YT_SOUND_OK);
	CHECK(current.user_sound && current.local_sound);
	CHECK(result.remote_length == 0U && result.play_length == 11U);
}

int
main(void)
{
	test_known_cues();
	test_branches_and_gates();
	test_invalid_cue();
	test_toggle();
	if (failures != 0) {
		fprintf(stderr, "%u test(s) failed\n", failures);
		return 1;
	}
	puts("test_sound: ok");
	return 0;
}
