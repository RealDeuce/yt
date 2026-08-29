#ifndef YT_PORTNAME_H
#define YT_PORTNAME_H

#include "yt_file.h"
#include "yt_random.h"

enum yt_portname_confirmation {
	YT_PORTNAME_CONFIRM_BLANK,
	YT_PORTNAME_CONFIRM_ACCEPT,
	YT_PORTNAME_CONFIRM_REJECT
};

enum yt_portname_output_kind {
	YT_PORTNAME_OUTPUT_INTRO,
	YT_PORTNAME_OUTPUT_MISSING_DATA,
	YT_PORTNAME_OUTPUT_ABORT,
	YT_PORTNAME_OUTPUT_RENAMING,
	YT_PORTNAME_OUTPUT_PROGRESS,
	YT_PORTNAME_OUTPUT_COMPLETE
};

struct yt_portname_output {
	uint8_t bytes[384];
	size_t length;
};

struct yt_portname_result {
	float loop_bound;
	float final_logical_port;
	int iterations;
	uint64_t draws_consumed;
	bool play_event;
};

enum yt_portname_parse_result {
	YT_PORTNAME_PARSE_VALID,
	YT_PORTNAME_PARSE_REDO,
	YT_PORTNAME_PARSE_RANGE
};

typedef bool (*yt_portname_output_fn)(void *context, const uint8_t *data,
    size_t length, struct yt_error *error);

enum yt_portname_confirmation yt_portname_confirm(const uint8_t *response,
    size_t length);
enum yt_portname_parse_result yt_portname_parse_confirmation(
    const uint8_t *input, size_t input_length, uint8_t *value,
    size_t value_capacity, size_t *value_length);
bool yt_portname_compose_output(enum yt_portname_output_kind kind,
    float logical_port, const uint8_t *name, size_t name_length,
    struct yt_portname_output *output);
uint32_t yt_portname_record_number(float port_offset, float logical_port);
bool yt_portname_overlay_record(struct yt_record *record, const uint8_t *name,
    size_t name_length, struct yt_error *error);
bool yt_portname_rename(struct yt_database *database, float port_offset,
    float planet_offset, struct yt_random *random,
    yt_portname_output_fn output, void *output_context,
    struct yt_portname_result *result, struct yt_error *error);

#endif
