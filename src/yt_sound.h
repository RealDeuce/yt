#ifndef YT_SOUND_H
#define YT_SOUND_H

#include "yt_common.h"

#define YT_SOUND_SCRATCH_SIZE 80U
#define YT_SOUND_REMOTE_SIZE 84U

enum yt_sound_status {
	YT_SOUND_OK,
	YT_SOUND_INVALID_STATE,
	YT_SOUND_USER_OVERFLOW,
	YT_SOUND_SNOOP_OVERFLOW,
	YT_SOUND_LOCAL_OVERFLOW
};

struct yt_sound_state {
	uint8_t conversion_mode;
	float ansi;
	const uint8_t *ansi_process;
	float mode;
	const uint8_t *mode_process;
	float user_sound;
	uint8_t *user_sound_process;
	float snoop;
	uint8_t *snoop_process;
	float local_sound;
	uint8_t *local_sound_process;
	uint8_t *toggle_selector_process;
	uint8_t scratch[YT_SOUND_SCRATCH_SIZE];
	size_t scratch_length;
};

void yt_sound_bind_ansi_process(struct yt_sound_state *state,
    const uint8_t ansi[4]);
float yt_sound_ansi(const struct yt_sound_state *state);
void yt_sound_bind_endpoint_process(struct yt_sound_state *state,
    const uint8_t mode[4], uint8_t user_sound[4], uint8_t local_sound[4]);
float yt_sound_mode(const struct yt_sound_state *state);
float yt_sound_user_sound(const struct yt_sound_state *state);
float yt_sound_local_sound(const struct yt_sound_state *state);
void yt_sound_bind_toggle_selector_process(struct yt_sound_state *state,
    uint8_t selector[4]);
void yt_sound_bind_snoop_process(struct yt_sound_state *state,
    uint8_t snoop[4]);
float yt_sound_snoop(const struct yt_sound_state *state);
void yt_sound_set_snoop(struct yt_sound_state *state, float value);

struct yt_sound_result {
	uint8_t line[10];
	size_t line_length;
	uint8_t remote[YT_SOUND_REMOTE_SIZE];
	size_t remote_length;
	uint8_t play[YT_SOUND_SCRATCH_SIZE];
	size_t play_length;
};

enum yt_sound_status yt_sound_dispatch(float selector,
    struct yt_sound_state *state, struct yt_sound_result *result);
enum yt_sound_status yt_sound_toggle(struct yt_sound_state *state,
    struct yt_sound_result *result);
enum yt_sound_status yt_sound_toggle_process(struct yt_sound_state *state,
    const uint8_t mode[4], uint8_t user_sound[4], uint8_t local_sound[4],
    struct yt_sound_result *result);
enum yt_sound_status yt_sound_sysop_toggle(struct yt_sound_state *state,
    bool *enabled);
enum yt_sound_status yt_sound_sysop_toggle_process(
    struct yt_sound_state *state, const uint8_t mode[4],
    uint8_t local_sound[4], uint8_t user_sound[4], bool *enabled);
enum yt_sound_status yt_sound_sysop_snoop_toggle(
    struct yt_sound_state *state, bool *returned_early, bool *enabled);

#endif
