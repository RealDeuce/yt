#include "yt_sound.h"
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
test_ansi_process_cell(void)
{
	static const uint8_t dirty_zero[] = {0x5a, 0xa5, 0x80, 0x00};
	static const uint8_t raw_one[] = {0x00, 0x00, 0x00, 0x81};
	struct yt_sound_state current = state(false);
	struct yt_sound_result result;
	uint8_t ansi[4];

	memcpy(ansi, dirty_zero, sizeof(ansi));
	memcpy(current.scratch, "stale", 5U);
	current.scratch_length = 5U;
	yt_sound_bind_ansi_process(&current, ansi);
	CHECK(yt_sound_ansi(&current) == 0.0f);
	CHECK(yt_sound_dispatch(2.0f, &current, &result) == YT_SOUND_OK);
	CHECK(result.remote_length == 0U && result.play_length == 0U
	    && current.scratch_length == 5U
	    && memcmp(ansi, dirty_zero, sizeof(ansi)) == 0);

	memcpy(ansi, raw_one, sizeof(ansi));
	CHECK(yt_sound_ansi(&current) == 1.0f);
	CHECK(yt_sound_dispatch(2.0f, &current, &result) == YT_SOUND_OK);
	CHECK(result.remote_length == 33U && result.remote[0] == 0x1b
	    && result.remote[1] == '[' && result.remote[32] == 0x0e
	    && result.play_length == 30U && current.scratch_length == 0U
	    && memcmp(ansi, raw_one, sizeof(ansi)) == 0);
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
	{
		static const uint8_t dirty_zero[4] = {
			0xffU, 0xffU, 0x00U, 0x00U,
		};
		struct yt_sound_state current = state(true);
		struct yt_sound_result result;
		uint8_t mode[4];
		uint8_t user[4];
		uint8_t local[4];
		uint8_t before[4];

		CHECK(qb_mbf32_encode(0.0f, mode) == QB_MBF_OK
		    && qb_mbf32_encode(-1.0f, user) == QB_MBF_OK
		    && qb_mbf32_encode(77.0f, local) == QB_MBF_OK);
		memcpy(before, local, sizeof(before));
		CHECK(yt_sound_toggle_process(&current, mode, user, local,
		    &result) == YT_SOUND_OK);
		CHECK(memcmp(user, dirty_zero, sizeof(dirty_zero)) == 0
		    && memcmp(local, before, sizeof(before)) == 0
		    && current.user_sound == 0.0f
		    && current.local_sound == 77.0f
		    && result.line_length == sizeof("Sound OFF") - 1U);

		CHECK(qb_mbf32_encode(1.0f, mode) == QB_MBF_OK
		    && qb_mbf32_encode(0.0f, user) == QB_MBF_OK
		    && qb_mbf32_encode(77.0f, local) == QB_MBF_OK);
		CHECK(yt_sound_toggle_process(&current, mode, user, local,
		    &result) == YT_SOUND_OK);
		CHECK(memcmp(user, "\x00\x00\x80\x81", 4U) == 0
		    && memcmp(local, user, 4U) == 0
		    && current.user_sound == -1.0f
		    && current.local_sound == -1.0f);

		CHECK(qb_mbf32_encode(0.0f, mode) == QB_MBF_OK
		    && qb_mbf32_encode(0.0f, user) == QB_MBF_OK
		    && qb_mbf32_encode(40000.0f, local) == QB_MBF_OK);
		memcpy(before, local, sizeof(before));
		CHECK(yt_sound_toggle_process(&current, mode, user, local,
		    &result) == YT_SOUND_LOCAL_OVERFLOW);
		CHECK(memcmp(user, "\x00\x00\x80\x81", 4U) == 0
		    && memcmp(local, before, sizeof(before)) == 0
		    && result.remote_length != 0U);

		CHECK(qb_mbf32_encode(40000.0f, user) == QB_MBF_OK
		    && qb_mbf32_encode(77.0f, local) == QB_MBF_OK);
		memcpy(before, user, sizeof(before));
		CHECK(yt_sound_toggle_process(&current, mode, user, local,
		    &result) == YT_SOUND_USER_OVERFLOW);
		CHECK(memcmp(user, before, sizeof(before)) == 0
		    && result.line_length == 0U && result.remote_length == 0U);
		CHECK(yt_sound_toggle_process(NULL, mode, user, local, &result)
		    == YT_SOUND_INVALID_STATE);
	}
}

static void
test_sysop_toggle(void)
{
	struct yt_sound_state current = state(true);
	uint8_t mode[4];
	uint8_t local[4];
	uint8_t user[4];
	uint8_t before[4];
	bool enabled = false;

	current.mode = 0.0f;
	current.local_sound = 0.0f;
	current.user_sound = -1.0f;
	CHECK(yt_sound_sysop_toggle(&current, &enabled) == YT_SOUND_OK);
	CHECK(enabled && current.local_sound == -1.0f
	    && current.user_sound == -1.0f);
	current.mode = 1.0f;
	current.user_sound = 77.0f;
	CHECK(yt_sound_sysop_toggle(&current, &enabled) == YT_SOUND_OK);
	CHECK(!enabled && current.local_sound == 0.0f
	    && current.user_sound == 0.0f);
	current.mode = 2.0f;
	current.local_sound = 1.0f;
	current.user_sound = 77.0f;
	CHECK(yt_sound_sysop_toggle(&current, &enabled) == YT_SOUND_OK);
	CHECK(enabled && current.local_sound == -2.0f
	    && current.user_sound == -2.0f);
	current.local_sound = 40000.0f;
	current.user_sound = 77.0f;
	CHECK(yt_sound_sysop_toggle(&current, &enabled)
	    == YT_SOUND_LOCAL_OVERFLOW);
	CHECK(current.local_sound == 40000.0f && current.user_sound == 77.0f);

	current = state(true);
	CHECK(qb_mbf32_encode(0.0f, mode) == QB_MBF_OK
	    && qb_mbf32_encode(-1.0f, local) == QB_MBF_OK
	    && qb_mbf32_encode(77.0f, user) == QB_MBF_OK);
	memcpy(before, user, sizeof(before));
	CHECK(yt_sound_sysop_toggle_process(&current, mode, local, user,
	    &enabled) == YT_SOUND_OK);
	CHECK(!enabled && memcmp(local, "\xff\xff\x00\x00", 4U) == 0
	    && memcmp(user, before, sizeof(before)) == 0);

	CHECK(qb_mbf32_encode(2.0f, mode) == QB_MBF_OK
	    && qb_mbf32_encode(0.0f, local) == QB_MBF_OK
	    && qb_mbf32_encode(77.0f, user) == QB_MBF_OK);
	CHECK(yt_sound_sysop_toggle_process(&current, mode, local, user,
	    &enabled) == YT_SOUND_OK);
	CHECK(enabled && memcmp(local, "\x00\x00\x80\x81", 4U) == 0
	    && memcmp(user, local, 4U) == 0);

	CHECK(qb_mbf32_encode(40000.0f, local) == QB_MBF_OK
	    && qb_mbf32_encode(77.0f, user) == QB_MBF_OK);
	memcpy(before, local, sizeof(before));
	CHECK(yt_sound_sysop_toggle_process(&current, mode, local, user,
	    &enabled) == YT_SOUND_LOCAL_OVERFLOW);
	CHECK(memcmp(local, before, sizeof(before)) == 0
	    && qb_mbf32_decode(user) == 77.0f);
}

static void
test_sysop_snoop_toggle(void)
{
	struct yt_sound_state current = state(true);
	bool returned_early = false;
	bool enabled = false;

	current.mode = 1.0f;
	current.snoop = 40000.0f;
	CHECK(yt_sound_sysop_snoop_toggle(&current, &returned_early, &enabled)
	    == YT_SOUND_OK);
	CHECK(returned_early && current.snoop == 40000.0f);
	current.mode = 0.0f;
	current.snoop = 0.0f;
	CHECK(yt_sound_sysop_snoop_toggle(&current, &returned_early, &enabled)
	    == YT_SOUND_OK);
	CHECK(!returned_early && enabled && current.snoop == -1.0f);
	current.mode = 2.0f;
	current.snoop = 1.0f;
	CHECK(yt_sound_sysop_snoop_toggle(&current, &returned_early, &enabled)
	    == YT_SOUND_OK);
	CHECK(!returned_early && enabled && current.snoop == -2.0f);
	current.snoop = 40000.0f;
	CHECK(yt_sound_sysop_snoop_toggle(&current, &returned_early, &enabled)
	    == YT_SOUND_SNOOP_OVERFLOW);
	CHECK(current.snoop == 40000.0f);
}

int
main(void)
{
	test_known_cues();
	test_branches_and_gates();
	test_stale_and_failures();
	test_ansi_process_cell();
	test_conversion_mode();
	test_toggle();
	test_sysop_toggle();
	test_sysop_snoop_toggle();
	if (failures != 0) {
		fprintf(stderr, "%u test(s) failed\n", failures);
		return 1;
	}
	puts("test_sound: ok");
	return 0;
}
