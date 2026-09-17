#include "yt_sound.h"

#include <string.h>

struct cue {
	const uint8_t *data;
	size_t length;
};

#define CUE(text) {(const uint8_t *)(text), sizeof(text) - 1U}

static const struct cue cues[] = {
	CUE(""),
	CUE("MBO4L32P32CP64CP64CP64L16EP64L32CP64L12E"),
	CUE("MBO1L64P32CEDFEGFAGBAO5BAGFEDC"),
	CUE("MBO2L2P32CL3CL8CP32L3CP6E-L8DL3DL8CL3CO1L8BO2L1C"),
	CUE("MBT128O5L48P64CP64C"),
	CUE("MBO1L64P8CdGCdGCdGCDGCGD"),
	CUE("MBO2T200L64FBEAP8FBEAP8FBEAP4FBEAP8FBEAP8FBEAP4T128"),
	CUE("MBO6L64T128GADO3BAGFEDCO2BAGFEDCO1BAGFEDCP16"),
	CUE("MBO4T128L64CGCGCGP16CGCGCGP16CGCGCGP16"),
	CUE("MBT64L64O3CEFADFGL16B")
};
static const uint8_t ascii_cue[] = "T255L63o4be";

static enum yt_sound_status
endpoint_gates(const struct yt_sound_state *state,
    const uint8_t *cue, size_t cue_length, bool ansi,
    struct yt_sound_result *result)
{
	if (!state->local_mode) {
		if (state->user_sound) {
			if (ansi) {
				result->remote[0] = 0x1b;
				result->remote[1] = '[';
				memcpy(result->remote + 2, cue, cue_length);
				result->remote[cue_length + 2U] = 0x0e;
				result->remote_length = cue_length + 3U;
			}
			else {
				result->remote[0] = 0x07;
				result->remote_length = 1;
			}
		}
	}
	if (state->local_output && state->local_sound) {
		memcpy(result->play, cue, cue_length);
		result->play_length = cue_length;
	}
	return YT_SOUND_OK;
}

enum yt_sound_status
yt_sound_dispatch(enum yt_sound_cue cue, struct yt_sound_state *state,
    struct yt_sound_result *result)
{
	const uint8_t *selected;
	size_t selected_length;

	memset(result, 0, sizeof(*result));
	if (cue < YT_SOUND_CUE_REWARD || cue >= YT_SOUND_CUE_COUNT)
		return YT_SOUND_INVALID_CUE;
	if (!state->ansi) {
		if (cue == YT_SOUND_CUE_ATTACK || cue == YT_SOUND_CUE_ACTION)
			return YT_SOUND_OK;
		return endpoint_gates(state, ascii_cue, sizeof(ascii_cue) - 1U,
		    false, result);
	}
	selected = cues[cue].data;
	selected_length = cues[cue].length;
	return endpoint_gates(state, selected, selected_length, true, result);
}

enum yt_sound_status
yt_sound_toggle(struct yt_sound_state *state, struct yt_sound_result *result)
{
	static const uint8_t on[] = "Sound ON";
	static const uint8_t off[] = "Sound OFF";
	enum yt_sound_status status;

	memset(result, 0, sizeof(*result));
	state->user_sound = !state->user_sound;
	if (state->local_mode)
		state->local_sound = state->user_sound;
	if (state->user_sound) {
		status = yt_sound_dispatch(YT_SOUND_CUE_REWARD, state, result);
		memcpy(result->line, on, sizeof(on) - 1U);
		result->line_length = sizeof(on) - 1U;
		return status;
	}
	memcpy(result->line, off, sizeof(off) - 1U);
	result->line_length = sizeof(off) - 1U;
	return YT_SOUND_OK;
}
