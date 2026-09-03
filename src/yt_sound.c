#include "yt_sound.h"

#include "qb.h"

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

static bool
sound_integer_raw(int32_t value, uint8_t raw[4])
{
	static const uint8_t dirty_zero[4] = {
		0xffU, 0xffU, 0x00U, 0x00U,
	};

	if (value == 0) {
		memcpy(raw, dirty_zero, sizeof(dirty_zero));
		return true;
	}
	return qb_mbf32_encode((float)value, raw) == QB_MBF_OK;
}

static enum yt_sound_status
endpoint_gates(const struct yt_sound_state *state,
    const uint8_t *cue, size_t cue_length, bool ansi,
    struct yt_sound_result *result)
{
	bool overflow;
	int32_t converted;
	int32_t snoop;
	int32_t local;

	if (state->mode == 0.0f) {
		converted = qb_cint_mode(state->user_sound,
		    state->conversion_mode, &overflow);
		if (overflow)
			return YT_SOUND_USER_OVERFLOW;
		if (converted != 0) {
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
	snoop = qb_cint_mode(state->snoop, state->conversion_mode, &overflow);
	if (overflow)
		return YT_SOUND_SNOOP_OVERFLOW;
	local = qb_cint_mode(state->local_sound,
	    state->conversion_mode, &overflow);
	if (overflow)
		return YT_SOUND_LOCAL_OVERFLOW;
	if ((snoop & local) != 0) {
		memcpy(result->play, cue, cue_length);
		result->play_length = cue_length;
	}
	return YT_SOUND_OK;
}

enum yt_sound_status
yt_sound_dispatch(float selector, struct yt_sound_state *state,
    struct yt_sound_result *result)
{
	const uint8_t *selected;
	size_t selected_length;
	enum yt_sound_status status;
	int index;

	memset(result, 0, sizeof(*result));
	if (selector == 0.0f)
		return YT_SOUND_OK;
	if (state->ansi == 0.0f) {
		if (selector == 2.0f || selector == 4.0f)
			return YT_SOUND_OK;
		return endpoint_gates(state, ascii_cue, sizeof(ascii_cue) - 1U,
		    false, result);
	}
	if (state->scratch_length > sizeof(state->scratch))
		return YT_SOUND_INVALID_STATE;
	if (selector >= 1.0f && selector <= 9.0f) {
		index = (int)selector;
	}
	else
		index = 0;
	if (index != 0 && selector == (float)index) {
		selected = cues[index].data;
		selected_length = cues[index].length;
		memcpy(state->scratch, selected, selected_length);
		state->scratch_length = selected_length;
	}
	else {
		selected = state->scratch;
		selected_length = state->scratch_length;
	}
	status = endpoint_gates(state, selected, selected_length, true, result);
	if (status != YT_SOUND_OK)
		return status;
	state->scratch_length = 0;
	return YT_SOUND_OK;
}

enum yt_sound_status
yt_sound_toggle(struct yt_sound_state *state, struct yt_sound_result *result)
{
	static const uint8_t on[] = "Sound ON";
	static const uint8_t off[] = "Sound OFF";
	bool overflow;
	int32_t prior;
	float toggled;
	enum yt_sound_status status;

	memset(result, 0, sizeof(*result));
	prior = qb_cint_mode(state->user_sound, state->conversion_mode,
	    &overflow);
	if (overflow)
		return YT_SOUND_USER_OVERFLOW;
	toggled = (float)(~prior);
	state->user_sound = toggled;
	if (state->mode != 0.0f)
		state->local_sound = toggled;
	if (toggled != 0.0f) {
		status = yt_sound_dispatch(1.0f, state, result);
		memcpy(result->line, on, sizeof(on) - 1U);
		result->line_length = sizeof(on) - 1U;
		return status;
	}
	memcpy(result->line, off, sizeof(off) - 1U);
	result->line_length = sizeof(off) - 1U;
	return YT_SOUND_OK;
}

enum yt_sound_status
yt_sound_toggle_process(struct yt_sound_state *state, const uint8_t mode[4],
    uint8_t user_sound[4], uint8_t local_sound[4],
    struct yt_sound_result *result)
{
	uint8_t toggled_raw[4];
	bool overflow;
	int32_t prior;
	int32_t toggled;
	enum yt_sound_status status;

	if (result != NULL)
		memset(result, 0, sizeof(*result));
	if (state == NULL || mode == NULL || user_sound == NULL
	    || local_sound == NULL || result == NULL)
		return YT_SOUND_INVALID_STATE;
	state->mode = qb_mbf32_decode(mode);
	state->user_sound = qb_mbf32_decode(user_sound);
	state->local_sound = qb_mbf32_decode(local_sound);
	prior = qb_cint_mode(state->user_sound, state->conversion_mode,
	    &overflow);
	if (overflow) {
		return YT_SOUND_USER_OVERFLOW;
	}
	toggled = ~prior;
	status = yt_sound_toggle(state, result);
	if (!sound_integer_raw(toggled, toggled_raw))
		return YT_SOUND_INVALID_STATE;
	memcpy(user_sound, toggled_raw, sizeof(toggled_raw));
	if (state->mode != 0.0f)
		memcpy(local_sound, toggled_raw, sizeof(toggled_raw));
	state->user_sound = qb_mbf32_decode(user_sound);
	state->local_sound = qb_mbf32_decode(local_sound);
	return status;
}

enum yt_sound_status
yt_sound_sysop_toggle_process(struct yt_sound_state *state,
    const uint8_t mode[4], uint8_t local_sound[4], uint8_t user_sound[4],
    bool *enabled)
{
	uint8_t toggled_raw[4];
	bool overflow;
	int32_t prior;
	int32_t toggled;

	if (state == NULL || mode == NULL || local_sound == NULL
	    || user_sound == NULL)
		return YT_SOUND_INVALID_STATE;
	state->mode = qb_mbf32_decode(mode);
	state->local_sound = qb_mbf32_decode(local_sound);
	state->user_sound = qb_mbf32_decode(user_sound);
	prior = qb_cint_mode(state->local_sound, state->conversion_mode,
	    &overflow);
	if (overflow)
		return YT_SOUND_LOCAL_OVERFLOW;
	toggled = ~prior;
	if (!sound_integer_raw(toggled, toggled_raw))
		return YT_SOUND_INVALID_STATE;
	memcpy(local_sound, toggled_raw, sizeof(toggled_raw));
	if (state->mode != 0.0f)
		memcpy(user_sound, toggled_raw, sizeof(toggled_raw));
	state->local_sound = qb_mbf32_decode(local_sound);
	state->user_sound = qb_mbf32_decode(user_sound);
	if (enabled != NULL)
		*enabled = toggled != 0;
	return YT_SOUND_OK;
}

enum yt_sound_status
yt_sound_sysop_toggle(struct yt_sound_state *state, bool *enabled)
{
	bool overflow;
	int32_t prior;
	float toggled;

	prior = qb_cint_mode(state->local_sound, state->conversion_mode,
	    &overflow);
	if (overflow)
		return YT_SOUND_LOCAL_OVERFLOW;
	toggled = (float)(~prior);
	state->local_sound = toggled;
	if (state->mode != 0.0f)
		state->user_sound = toggled;
	if (enabled != NULL)
		*enabled = toggled != 0.0f;
	return YT_SOUND_OK;
}

enum yt_sound_status
yt_sound_sysop_snoop_toggle(struct yt_sound_state *state,
    bool *returned_early, bool *enabled)
{
	bool overflow;
	int32_t prior;
	float toggled;

	if (returned_early != NULL)
		*returned_early = state->mode == 1.0f;
	if (state->mode == 1.0f)
		return YT_SOUND_OK;
	prior = qb_cint_mode(state->snoop, state->conversion_mode, &overflow);
	if (overflow)
		return YT_SOUND_SNOOP_OVERFLOW;
	toggled = (float)(~prior);
	state->snoop = toggled;
	if (enabled != NULL)
		*enabled = toggled != 0.0f;
	return YT_SOUND_OK;
}
