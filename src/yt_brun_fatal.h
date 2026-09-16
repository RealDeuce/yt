#ifndef YT_BRUN_FATAL_H
#define YT_BRUN_FATAL_H

#include "yt_common.h"

#define YT_BRUN_FATAL_TEXT 256U

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
	bool has_source_line;
};

/* Selects the canonical BRUN description for every byte in 00h..FFh. */
bool yt_brun_runtime_error_description(uint8_t error_number,
	const uint8_t **description, size_t *description_length);

/*
 * Composes the diagnostic and prompt for the shared BRUN 0AC9/0ACC terminal.
 * The owning executable performs its own concrete output and cleanup.
 */
bool yt_brun_internal_fatal_compose(enum yt_brun_internal_fatal_entry entry,
	const char module[8], bool has_source_line, int32_t source_line,
	uint16_t module_segment, uint16_t saved_ip,
	struct yt_brun_internal_fatal_state *state);

#endif
