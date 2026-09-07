#ifndef YT_BRUN_FATAL_H
#define YT_BRUN_FATAL_H

#include "yt_common.h"

#define YT_BRUN_FATAL_TEXT 256U
#define YT_BRUN_FATAL_DRAIN_WORDS 16U
#define YT_BRUN_FATAL_DESCRIPTION 64U

enum yt_brun_internal_fatal_entry {
	YT_BRUN_INTERNAL_FATAL_GC = 0x0AC9,
	YT_BRUN_INTERNAL_FATAL_OWNER = 0x0ACC,
};

struct yt_brun_internal_fatal_state {
	enum yt_brun_internal_fatal_entry entry;
	char module[9];
	uint16_t module_segment;
	uint16_t saved_ip;
	int32_t source_line;
	uint8_t diagnostic[YT_BRUN_FATAL_TEXT];
	size_t diagnostic_length;
	uint8_t prompt[YT_BRUN_FATAL_TEXT];
	size_t prompt_length;
	uint8_t local_bytes[YT_BRUN_FATAL_TEXT];
	size_t local_length;
	uint16_t drained_words[YT_BRUN_FATAL_DRAIN_WORDS];
	size_t drained_word_count;
	uint16_t process_entry_cursor_shape;
	bool has_source_line;
	bool redirected_stdin;
	bool function_bar_before;
	bool function_bar_after;
	bool cursor_shape_known;
	bool close_all_completed;
	bool input_drained;
	bool terminal_restored;
	bool ended;
	unsigned exit_status;
};

typedef void (*yt_brun_fatal_local_fn)(void *context,
	const uint8_t *data, size_t length);
typedef void (*yt_brun_fatal_close_all_fn)(void *context);
typedef size_t (*yt_brun_fatal_drain_fn)(void *context,
	uint16_t *words, size_t capacity);
typedef void (*yt_brun_fatal_function_bar_fn)(void *context);
typedef void (*yt_brun_fatal_restore_fn)(void *context,
	bool cursor_shape_known, uint16_t cursor_shape);
typedef void (*yt_brun_fatal_end_fn)(void *context, unsigned status);

struct yt_brun_internal_fatal_ops {
	yt_brun_fatal_local_fn local;
	yt_brun_fatal_close_all_fn close_all;
	yt_brun_fatal_drain_fn drain;
	yt_brun_fatal_function_bar_fn clear_function_bar;
	yt_brun_fatal_restore_fn restore_terminal;
	yt_brun_fatal_end_fn end;
};

struct yt_brun_runtime_fatal_state {
	uint8_t error_number;
	uint8_t error_description[YT_BRUN_FATAL_DESCRIPTION];
	size_t error_description_length;
	/* Common diagnostic, input, cleanup and terminal carrier. */
	struct yt_brun_internal_fatal_state terminal;
};

/* Selects the canonical BRUN description for every byte in 00h..FFh. */
bool yt_brun_runtime_error_description(uint8_t error_number,
	const uint8_t **description, size_t *description_length);

/*
 * Runs the shared BRUN 0AC9/0ACC terminal in its documented order.  The
 * caller supplies the relocated module segment and the physical cleanup
 * adapters; no BASIC ERR/ERL handler is entered.
 */
bool yt_brun_internal_fatal_run(enum yt_brun_internal_fatal_entry entry,
	const char module[8], bool has_source_line, int32_t source_line,
	uint16_t module_segment, uint16_t saved_ip, bool redirected_stdin,
	bool function_bar, bool cursor_shape_known,
	uint16_t process_entry_cursor_shape,
	const struct yt_brun_internal_fatal_ops *ops, void *context,
	struct yt_brun_internal_fatal_state *state);

/*
 * Runs BRUN's shared 0AC4 no-handler/active-handler fatal terminal with an
 * already selected exact CP437 description.
 */
bool yt_brun_runtime_fatal_run(uint8_t error_number,
	const uint8_t *error_description, size_t error_description_length,
	const char module[8], bool has_source_line, int32_t source_line,
	uint16_t module_segment, uint16_t saved_ip, bool redirected_stdin,
	bool function_bar, bool cursor_shape_known,
	uint16_t process_entry_cursor_shape,
	const struct yt_brun_internal_fatal_ops *ops, void *context,
	struct yt_brun_runtime_fatal_state *state);

/* Selects the canonical description and runs the same shared terminal. */
bool yt_brun_runtime_error_fatal_run(uint8_t error_number,
	const char module[8], bool has_source_line, int32_t source_line,
	uint16_t module_segment, uint16_t saved_ip, bool redirected_stdin,
	bool function_bar, bool cursor_shape_known,
	uint16_t process_entry_cursor_shape,
	const struct yt_brun_internal_fatal_ops *ops, void *context,
	struct yt_brun_runtime_fatal_state *state);

#endif
