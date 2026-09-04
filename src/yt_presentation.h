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
	void (*local_clear)(void *context);
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
	uint8_t *background_process;
	float bold;
	uint8_t *bold_process;
	float blink;
	uint8_t *blink_process;
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

void yt_present_bind_background_process(struct yt_present_state *state,
    uint8_t background[4]);
float yt_present_background(const struct yt_present_state *state);
void yt_present_set_background(struct yt_present_state *state, float value);
void yt_present_bind_bold_process(struct yt_present_state *state,
    uint8_t bold[4]);
float yt_present_bold(const struct yt_present_state *state);
void yt_present_set_bold(struct yt_present_state *state, float value);
void yt_present_bind_blink_process(struct yt_present_state *state,
    uint8_t blink[4]);
float yt_present_blink(const struct yt_present_state *state);
void yt_present_set_blink(struct yt_present_state *state, float value);

typedef enum yt_present_status (*yt_present_sysop_replay_fn)(void *context);

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
enum yt_present_status yt_present_sound_toggle_process(
    const uint8_t mode[4], uint8_t user_sound[4], uint8_t local_sound[4],
    struct yt_present_state *state, struct yt_present_result *result);
enum yt_present_status yt_present_sysop_sound_toggle(
    struct yt_present_state *state, struct yt_present_result *result);
enum yt_present_status yt_present_sysop_sound_toggle_process(
    const uint8_t mode[4], uint8_t local_sound[4], uint8_t user_sound[4],
    struct yt_present_state *state, struct yt_present_result *result);
enum yt_present_status yt_present_sysop_snoop_toggle(
    const uint8_t *real_name, size_t real_name_length,
    const uint8_t *alias, size_t alias_length,
    struct yt_present_state *state, struct yt_present_result *result);
enum yt_present_status yt_present_sysop_snoop_toggle_process(
    const uint8_t *real_name, size_t real_name_length,
    const uint8_t *alias, size_t alias_length, const uint8_t mode[4],
    uint8_t snoop[4], struct yt_present_state *state,
    struct yt_present_result *result);
enum yt_present_status yt_present_sysop_time_prompt(float deadline,
    float timer, struct yt_present_result *result);
enum yt_present_status yt_present_sysop_time_replace(
    const uint8_t *entered, size_t entered_length, float commit_timer,
    float *deadline, float *minutes, bool *changed);
enum yt_present_status yt_present_sysop_time_replace_process(
    const uint8_t *entered, size_t entered_length, float commit_timer,
    uint8_t deadline[4], float *minutes, bool *changed);
enum yt_present_status yt_present_sysop_time_handler(float prompt_timer,
    const uint8_t *entered, size_t entered_length, float commit_timer,
    float *deadline, float *minutes, bool *changed,
    struct yt_present_result *prompt,
    yt_present_sysop_replay_fn replay, void *replay_context);
enum yt_present_status yt_present_sysop_time_handler_process(
    float prompt_timer, const uint8_t *entered, size_t entered_length,
    float commit_timer, uint8_t deadline[4], float *minutes, bool *changed,
    struct yt_present_result *prompt, yt_present_sysop_replay_fn replay,
    void *replay_context);
enum yt_present_status yt_present_sysop_chat_header(
    const uint8_t *sysop, size_t sysop_length, int local_background,
    struct yt_present_result *result);
enum yt_present_status yt_present_serial_startup_missing_command(
    struct yt_present_result *result);
enum yt_present_status yt_present_serial_startup_status(int port,
    float detected_baud, struct yt_present_result *result);
enum yt_present_status yt_present_carrier_drop(
    struct yt_present_state *state, struct yt_present_result *result);
enum yt_present_status yt_present_sound(float selector,
    struct yt_present_state *state, struct yt_present_result *result);
enum yt_present_status yt_present_press_prompt(
    struct yt_present_state *state, struct yt_present_result *result,
    float *saved_foreground);
enum yt_present_status yt_present_press_cleanup(float saved_foreground,
    struct yt_present_state *state, struct yt_present_result *result);
enum yt_present_status yt_present_lottery_rewind(int row, int column,
    struct yt_present_state *state, struct yt_present_result *result);
enum yt_present_status yt_present_radio_backspace(int line_number,
    size_t shortened_length, struct yt_present_state *state,
    struct yt_present_result *result);
enum yt_present_status yt_present_radio_wrap_cleanup(int line_number,
    size_t wrap_marker, struct yt_present_state *state,
    struct yt_present_result *result);
enum yt_present_status yt_present_refresh_time(
    struct yt_present_time_state *time, const float *timer_reads,
    size_t timer_count, size_t *timer_used, int cursor_row, int cursor_column,
    struct yt_present_state *state, struct yt_present_result *result,
    bool *updated);
enum yt_present_status yt_present_refresh_time_process(
    struct yt_present_time_state *time, uint8_t deadline[4],
    uint8_t next_refresh[4], const float *timer_reads, size_t timer_count,
    size_t *timer_used, int cursor_row, int cursor_column,
    struct yt_present_state *state, struct yt_present_result *result,
    bool *updated);
enum yt_present_status yt_present_low_time(const uint8_t *text, size_t length,
    float *remembered, struct yt_present_state *state,
    struct yt_present_result *result, bool *warned);
enum yt_present_status yt_present_low_time_process(const uint8_t *text,
    size_t length, uint8_t remembered[4], struct yt_present_state *state,
    struct yt_present_result *result, bool *warned);
enum yt_present_status yt_present_status_row(const uint8_t *real_name,
    size_t real_name_length, const uint8_t *alias, size_t alias_length,
    struct yt_present_state *state, struct yt_present_result *result);
enum yt_present_status yt_present_opening_row(const uint8_t *text,
    size_t length, float mode, float snoop,
    struct yt_present_result *result);
enum yt_present_status yt_present_opening_cleanup(float mode, float snoop,
    struct yt_present_result *result);
void yt_present_replay(const struct yt_present_result *result,
    const struct yt_present_sink *sink);
uint8_t yt_present_pc_attribute(int foreground, int background);

#endif
