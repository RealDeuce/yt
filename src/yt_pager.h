#ifndef YT_PAGER_H
#define YT_PAGER_H

#include "yt_input_model.h"
#include "yt_presentation.h"

#define YT_PAGER_KEY_SIZE 80U

struct yt_pager_state {
	float line_count;
	float nonstop;
	float newline_flag;
	char key[YT_PAGER_KEY_SIZE];
	int foreground;
	uint8_t *line_count_cell;
	uint8_t *nonstop_cell;
	uint8_t *newline_flag_cell;
	uint8_t *foreground_cell;
	uint8_t *saved_foreground_cell;
	uint8_t *uppercase_numeric_temp_cell;
	uint8_t *uppercase_length_cell;
	uint8_t *uppercase_index_cell;
};

struct yt_sector_pager_state {
	float line_count;
};

struct yt_radio_pager_state {
	float line_count;
};

bool yt_pager_advance(struct yt_pager_state *pager,
    struct yt_present_state *presentation, int *saved_foreground);
void yt_pager_bind_process_cells(struct yt_pager_state *pager,
	uint8_t line_count[4], uint8_t nonstop[4], uint8_t newline_flag[4],
	uint8_t foreground[4], uint8_t saved_foreground[4],
	uint8_t uppercase_numeric_temp[4], uint8_t uppercase_length[4],
	uint8_t uppercase_index[4]);
void yt_pager_sync_process(struct yt_pager_state *pager);
void yt_pager_set_line_count_raw(struct yt_pager_state *pager,
	const uint8_t raw[4]);
void yt_pager_set_line_count(struct yt_pager_state *pager, float value);
void yt_pager_set_nonstop_raw(struct yt_pager_state *pager,
	const uint8_t raw[4]);
void yt_pager_set_nonstop(struct yt_pager_state *pager, float value);
void yt_pager_set_newline_raw(struct yt_pager_state *pager,
	const uint8_t raw[4]);
void yt_pager_set_newline(struct yt_pager_state *pager, float value);
void yt_pager_editor_enter(struct yt_pager_state *pager,
    char *accumulator, size_t accumulator_capacity);
bool yt_pager_accept_response(struct yt_pager_state *pager, char *response,
    size_t response_capacity);
void yt_pager_complete(struct yt_pager_state *pager,
    struct yt_present_state *presentation, int saved_foreground);

typedef bool (*yt_paged_row_carrier_fn)(void *context);
typedef bool (*yt_paged_row_sample_fn)(void *context,
	struct yt_input_value *sampled);
typedef bool (*yt_paged_row_present_fn)(void *context,
	const uint8_t *text, size_t length);
typedef bool (*yt_paged_row_finish_fn)(void *context, bool newline_flag);
typedef bool (*yt_paged_row_response_fn)(void *context, char *response,
	size_t capacity);

struct yt_paged_row_ops {
	yt_paged_row_carrier_fn carrier;
	yt_paged_row_sample_fn sample;
	yt_paged_row_present_fn present;
	yt_paged_row_finish_fn finish;
	yt_paged_row_response_fn response;
};

bool yt_paged_row_run(struct yt_pager_state *pager,
	struct yt_present_state *presentation,
	struct yt_b05d_key_state *key_state, const uint8_t *text,
	size_t length, const struct yt_paged_row_ops *ops, void *context);

void yt_sector_pager_begin(struct yt_sector_pager_state *pager);
void yt_sector_pager_add(struct yt_sector_pager_state *pager, float lines);
bool yt_sector_pager_finish_sector(struct yt_sector_pager_state *pager);
void yt_radio_pager_begin(struct yt_radio_pager_state *pager);
void yt_radio_pager_add_pair(struct yt_radio_pager_state *pager);
bool yt_radio_pager_add_body(struct yt_radio_pager_state *pager);

#endif
