#ifndef YT_SOUND_H
#define YT_SOUND_H

#include "yt_common.h"

#define YT_SOUND_CUE_SIZE 80U
#define YT_SOUND_REMOTE_SIZE 84U

enum yt_sound_cue {
	YT_SOUND_CUE_REWARD = 1,
	YT_SOUND_CUE_ATTACK,
	YT_SOUND_CUE_DESTRUCTION,
	YT_SOUND_CUE_ACTION,
	YT_SOUND_CUE_DAMAGE,
	YT_SOUND_CUE_ATTENTION,
	YT_SOUND_CUE_LAUNCH,
	YT_SOUND_CUE_DANGER,
	YT_SOUND_CUE_SPY,
	YT_SOUND_CUE_COUNT
};

enum yt_sound_status {
	YT_SOUND_OK,
	YT_SOUND_INVALID_CUE,
	YT_SOUND_USER_OVERFLOW,
	YT_SOUND_SNOOP_OVERFLOW,
	YT_SOUND_LOCAL_OVERFLOW
};

struct yt_sound_state {
	uint8_t conversion_mode;
	float ansi;
	float mode;
	float user_sound;
	float snoop;
	float local_sound;
};

struct yt_sound_result {
	uint8_t line[10];
	size_t line_length;
	uint8_t remote[YT_SOUND_REMOTE_SIZE];
	size_t remote_length;
	uint8_t play[YT_SOUND_CUE_SIZE];
	size_t play_length;
};

enum yt_sound_status yt_sound_dispatch(enum yt_sound_cue cue,
    struct yt_sound_state *state, struct yt_sound_result *result);
enum yt_sound_status yt_sound_toggle(struct yt_sound_state *state,
    struct yt_sound_result *result);
#endif
