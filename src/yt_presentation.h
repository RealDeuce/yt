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
	YT_PRESENT_LOCAL_BEEP
};

struct yt_present_event {
	enum yt_present_operation operation;
	int foreground;
	int background;
	int row;
	int column;
	int cursor_visible;
	int cursor_start;
	int cursor_stop;
	uint8_t data[YT_PRESENT_EVENT_DATA];
	size_t length;
};

struct yt_present_sink {
	void *context;
	void (*remote)(void *context, const uint8_t *data, size_t length,
	    bool line);
	void (*local_color)(void *context, int foreground, int background);
	void (*local_text)(void *context, const uint8_t *data, size_t length,
	    bool line);
	void (*local_locate)(void *context, int row, int column,
	    int cursor_visible, int cursor_start, int cursor_stop);
	void (*local_beep)(void *context);
};

struct yt_present_time_state {
	float deadline;
	float next_refresh;
	uint8_t text[64];
	size_t text_length;
	int saved_row;
	int saved_column;
	float remaining_minutes;
	uint8_t seconds_text[16];
	size_t seconds_length;
};

struct yt_present_state {
	struct yt_sound_state sound;
	float foreground;
	float background;
	float bold;
	float blink;
	float color_initialized;
	float color_memory[8];
	float cached_foreground;
	float cached_background;
};

struct yt_present_result {
	uint8_t remote[YT_PRESENT_REMOTE_SIZE];
	size_t remote_length;
	struct yt_present_event events[YT_PRESENT_EVENTS];
	size_t event_count;
};

enum yt_present_status yt_present_color(struct yt_present_state *state,
    struct yt_present_result *result);
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
enum yt_present_status yt_present_sound(float selector,
    struct yt_present_state *state, struct yt_present_result *result);
enum yt_present_status yt_present_press_prompt(
    struct yt_present_state *state, struct yt_present_result *result,
    float *saved_foreground);
enum yt_present_status yt_present_press_cleanup(float saved_foreground,
    struct yt_present_state *state, struct yt_present_result *result);
enum yt_present_status yt_present_refresh_time(
    struct yt_present_time_state *time, const float *timer_reads,
    size_t timer_count, size_t *timer_used, int cursor_row, int cursor_column,
    struct yt_present_state *state, struct yt_present_result *result,
    bool *updated);
enum yt_present_status yt_present_low_time(const uint8_t *text, size_t length,
    float *remembered, struct yt_present_state *state,
    struct yt_present_result *result, bool *warned);
void yt_present_replay(const struct yt_present_result *result,
    const struct yt_present_sink *sink);
uint8_t yt_present_pc_attribute(int foreground, int background);

#endif
