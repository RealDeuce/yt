#ifndef YT_FRAMEBUFFER_H
#define YT_FRAMEBUFFER_H

#include "yt_presentation.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define YT_FRAMEBUFFER_WIDTH 80U
#define YT_FRAMEBUFFER_HEIGHT 25U
#define YT_FRAMEBUFFER_CELLS \
	(YT_FRAMEBUFFER_WIDTH * YT_FRAMEBUFFER_HEIGHT)
#define YT_FRAMEBUFFER_PROFILE \
	"dosbox-0.74-3/text-80x25/page-0/cp437"
#define YT_FRAMEBUFFER_STATE_BYTES 4092U

enum yt_framebuffer_status {
	YT_FRAMEBUFFER_OK,
	YT_FRAMEBUFFER_INVALID_ARGUMENT,
	YT_FRAMEBUFFER_INVALID_STATE,
	YT_FRAMEBUFFER_UNSUPPORTED_CONTROL,
	YT_FRAMEBUFFER_ANSI_OVERFLOW,
	YT_FRAMEBUFFER_CAPACITY
};

enum yt_framebuffer_operation {
	YT_FRAMEBUFFER_COLOR,
	YT_FRAMEBUFFER_CLS,
	YT_FRAMEBUFFER_LOCATE,
	YT_FRAMEBUFFER_CURSOR_HIDE,
	YT_FRAMEBUFFER_FUNCTION_BAR_SET,
	YT_FRAMEBUFFER_END_CLEANUP,
	YT_FRAMEBUFFER_PRINT_RAW,
	YT_FRAMEBUFFER_PRINT_NL,
	YT_FRAMEBUFFER_CON_DEVICE_STREAM,
	YT_FRAMEBUFFER_PROCESS_STARTUP
};

struct yt_framebuffer_state {
	uint8_t characters[YT_FRAMEBUFFER_CELLS];
	uint8_t attributes[YT_FRAMEBUFFER_CELLS];
	uint16_t bios_row;
	uint16_t bios_column;
	uint16_t qb_row;
	uint16_t qb_column;
	uint16_t brun_bios_cache_row;
	uint16_t brun_bios_cache_column;
	uint8_t qb_attribute;
	uint16_t cursor_shape;
	bool cursor_visible;
	uint8_t ansi_attribute;
	bool ansi_enabled;
	uint16_t ansi_saved_row;
	uint16_t ansi_saved_column;
	uint8_t ansi_lastwrite;
	bool ansi_escape_pending;
	bool ansi_csi_pending;
	uint8_t ansi_argument_index;
	uint8_t ansi_arguments[10];
	uint32_t qb_scrolls;
	uint32_t con_scrolls;
	bool function_bar;
	bool process_entry_cursor_shape_present;
	uint16_t process_entry_cursor_shape;
};

struct yt_framebuffer_event {
	enum yt_framebuffer_operation operation;
	const uint8_t *data;
	size_t length;
	bool has_foreground;
	uint8_t foreground;
	bool has_background;
	uint8_t background;
	bool has_row;
	uint16_t row;
	bool has_column;
	uint16_t column;
	bool has_cursor;
	bool cursor_visible;
	uint8_t cursor_start;
	uint8_t cursor_stop;
	bool has_function_bar;
	bool function_bar;
};

void yt_framebuffer_init(struct yt_framebuffer_state *state,
    bool process_entry_cursor_shape_present,
    uint16_t process_entry_cursor_shape);
bool yt_framebuffer_validate(const struct yt_framebuffer_state *state);
enum yt_framebuffer_status yt_framebuffer_apply(
    struct yt_framebuffer_state *state,
    const struct yt_framebuffer_event *event);
enum yt_framebuffer_status yt_framebuffer_apply_all(
    struct yt_framebuffer_state *state,
    const struct yt_framebuffer_event *events, size_t count);
enum yt_framebuffer_status yt_framebuffer_apply_present_event(
    struct yt_framebuffer_state *state,
    const struct yt_present_event *event);
enum yt_framebuffer_status yt_framebuffer_apply_present_result(
    struct yt_framebuffer_state *state,
    const struct yt_present_result *result);
enum yt_framebuffer_status yt_framebuffer_apply_con_observation(
    struct yt_framebuffer_state *state, const uint8_t *requested,
    size_t requested_length, const uint8_t *accepted,
    size_t accepted_length, bool completed, uint8_t error);
enum yt_framebuffer_status yt_framebuffer_serialize(
    const struct yt_framebuffer_state *state, uint8_t *output,
    size_t capacity, size_t *written);
enum yt_framebuffer_status yt_framebuffer_deserialize(
    struct yt_framebuffer_state *state, const uint8_t *input,
    size_t length);

#endif

