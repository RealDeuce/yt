#ifndef YT_PRESENTATION_H
#define YT_PRESENTATION_H

#include "yt_sound.h"

#define YT_PRESENT_REMOTE_SIZE 1024U
#define YT_PRESENT_EVENTS 16U
#define YT_PRESENT_EVENT_DATA 1024U

enum yt_present_status {
	YT_PRESENT_OK,
	YT_PRESENT_RANGE,
	YT_PRESENT_OVERFLOW,
	YT_PRESENT_CAPACITY,
	YT_PRESENT_SOUND_ERROR,
	YT_PRESENT_TIMER_EXHAUSTED
};

enum yt_present_operation {
	YT_PRESENT_REMOTE_LINE,
	YT_PRESENT_REMOTE_SEMI,
	YT_PRESENT_LOCAL_COLOR,
	YT_PRESENT_LOCAL_LINE,
	YT_PRESENT_LOCAL_SEMI,
	YT_PRESENT_LOCAL_PLAY,
	YT_PRESENT_LOCAL_LOCATE,
	YT_PRESENT_LOCAL_BEEP,
	YT_PRESENT_LOCAL_CLEAR
};

struct yt_present_event {
	enum yt_present_operation operation;
	int foreground;
	int background;
	int row;
	int column;
	uint8_t data[YT_PRESENT_EVENT_DATA];
	size_t length;
};

struct yt_present_time_state {
	uint8_t text[64];
	size_t text_length;
	float remaining_minutes;
	uint8_t seconds_text[16];
	size_t seconds_length;
};

struct yt_present_state {
	struct yt_sound_state sound;
	uint8_t conversion_mode;
	int foreground;
	int background;
	bool bold;
	bool blink;
	int cached_foreground;
	int cached_background;
};

struct yt_present_result {
	uint8_t remote[YT_PRESENT_REMOTE_SIZE];
	size_t remote_length;
	struct yt_present_event events[YT_PRESENT_EVENTS];
	size_t event_count;
};

enum yt_present_status yt_present_line(const uint8_t *text, size_t length,
    struct yt_present_state *state, struct yt_present_result *result);
enum yt_present_status yt_present_character(const uint8_t *text,
    size_t length, struct yt_present_state *state,
    struct yt_present_result *result);
enum yt_present_status yt_present_bold_line(const uint8_t *text,
    size_t length, struct yt_present_state *state,
    struct yt_present_result *result);
enum yt_present_status yt_present_bold_character(const uint8_t *text,
    size_t length, struct yt_present_state *state,
    struct yt_present_result *result);
enum yt_present_status yt_present_paged_text(const uint8_t *text,
    size_t length, struct yt_present_state *state,
    struct yt_present_result *result);
enum yt_present_status yt_present_paged_finish(bool suppress_newline,
    struct yt_present_state *state, struct yt_present_result *result);
enum yt_present_status yt_present_editor_echo(const uint8_t *local,
    size_t local_length, const uint8_t *remote, size_t remote_length,
    struct yt_present_state *state, struct yt_present_result *result);
enum yt_present_status yt_present_local_line(const uint8_t *text,
    size_t length, struct yt_present_state *state,
    struct yt_present_result *result);
enum yt_present_status yt_present_forced_local_line(const uint8_t *text,
    size_t length, struct yt_present_result *result);
enum yt_present_status yt_present_local_beep(
    struct yt_present_result *result);
enum yt_present_status yt_present_right_aligned(const uint8_t *text,
    size_t length, float width, struct yt_present_state *state,
    struct yt_present_result *result);
enum yt_present_status yt_present_fixed_width(uint8_t *text, size_t *length,
    size_t capacity, float width, struct yt_present_state *state,
    struct yt_present_result *result);
enum yt_present_status yt_present_centered_line(const uint8_t *text,
    size_t length, struct yt_present_state *state,
    struct yt_present_result *result);
enum yt_present_status yt_present_attention(const uint8_t *text,
    size_t length, struct yt_present_state *state,
    struct yt_present_result *result);
enum yt_present_status yt_present_sound_toggle(struct yt_present_state *state,
    struct yt_present_result *result);
enum yt_present_status yt_present_sound(enum yt_sound_cue cue,
    struct yt_present_state *state, struct yt_present_result *result);
enum yt_present_status yt_present_press_prompt(
    struct yt_present_state *state, struct yt_present_result *result,
    int *saved_foreground);
enum yt_present_status yt_present_press_cleanup(int saved_foreground,
    struct yt_present_state *state, struct yt_present_result *result);
enum yt_present_status yt_present_lottery_rewind(int row, int column,
    struct yt_present_state *state, struct yt_present_result *result);
enum yt_present_status yt_present_radio_backspace(int line_number,
    size_t shortened_length, struct yt_present_state *state,
    struct yt_present_result *result);
enum yt_present_status yt_present_radio_wrap_cleanup(int line_number,
    size_t wrap_marker, struct yt_present_state *state,
    struct yt_present_result *result);
enum yt_present_status yt_present_format_remaining_seconds(
    struct yt_present_time_state *time, float remaining_seconds);
enum yt_present_status yt_present_low_time(const uint8_t *text, size_t length,
    float *remembered, struct yt_present_state *state,
    struct yt_present_result *result, bool *warned);
uint8_t yt_present_pc_attribute(int foreground, int background);

#endif
