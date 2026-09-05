#include "yt_pager.h"

#include "qb.h"

#include <stdio.h>
#include <string.h>

static float
single_add(float left, float right)
{
	volatile float result = left + right;

	return result;
}

static bool
pager_process_bound(const struct yt_pager_state *pager)
{
	return pager->line_count_cell != NULL && pager->nonstop_cell != NULL
	    && pager->newline_flag_cell != NULL && pager->foreground_cell != NULL
	    && pager->saved_foreground_cell != NULL;
}

static void
pager_store_raw(float *value, uint8_t *cell, const uint8_t raw[4])
{
	if (cell != NULL)
		memcpy(cell, raw, 4U);
	*value = qb_mbf32_decode(raw);
}

static void
pager_store_single(float *destination, uint8_t *cell, float value)
{
	uint8_t raw[4];

	if (qb_mbf32_encode(value, raw) == QB_MBF_OK)
		pager_store_raw(destination, cell, raw);
}

void
yt_pager_bind_process_cells(struct yt_pager_state *pager,
    uint8_t line_count[4], uint8_t nonstop[4], uint8_t newline_flag[4],
    uint8_t foreground[4], uint8_t saved_foreground[4],
    uint8_t uppercase_numeric_temp[4], uint8_t uppercase_length[4],
    uint8_t uppercase_index[4])
{
	if (pager == NULL)
		return;
	pager->line_count_cell = line_count;
	pager->nonstop_cell = nonstop;
	pager->newline_flag_cell = newline_flag;
	pager->foreground_cell = foreground;
	pager->saved_foreground_cell = saved_foreground;
	pager->uppercase_numeric_temp_cell = uppercase_numeric_temp;
	pager->uppercase_length_cell = uppercase_length;
	pager->uppercase_index_cell = uppercase_index;
	yt_pager_sync_process(pager);
}

static void
pager_upper_store(void *context, enum qb_compat_upper_store_kind kind,
    float value)
{
	struct yt_pager_state *pager = context;
	uint8_t *cell;
	uint8_t raw[4];

	switch (kind) {
	case QB_COMPAT_UPPER_STORE_NUMERIC_TEMP:
		cell = pager->uppercase_numeric_temp_cell;
		break;
	case QB_COMPAT_UPPER_STORE_LENGTH:
		cell = pager->uppercase_length_cell;
		break;
	case QB_COMPAT_UPPER_STORE_INDEX:
		cell = pager->uppercase_index_cell;
		break;
	default:
		return;
	}
	if (cell != NULL && qb_mbf32_encode(value, raw) == QB_MBF_OK)
		memcpy(cell, raw, sizeof(raw));
}

void
yt_pager_sync_process(struct yt_pager_state *pager)
{
	if (pager == NULL || !pager_process_bound(pager))
		return;
	pager->line_count = qb_mbf32_decode(pager->line_count_cell);
	pager->nonstop = qb_mbf32_decode(pager->nonstop_cell);
	pager->newline_flag = qb_mbf32_decode(pager->newline_flag_cell);
	pager->foreground = (int)qb_mbf32_decode(pager->foreground_cell);
}

void
yt_pager_set_line_count_raw(struct yt_pager_state *pager,
    const uint8_t raw[4])
{
	if (pager != NULL && raw != NULL)
		pager_store_raw(&pager->line_count, pager->line_count_cell, raw);
}

void
yt_pager_set_line_count(struct yt_pager_state *pager, float value)
{
	if (pager != NULL)
		pager_store_single(&pager->line_count, pager->line_count_cell, value);
}

void
yt_pager_set_nonstop_raw(struct yt_pager_state *pager,
    const uint8_t raw[4])
{
	if (pager != NULL && raw != NULL)
		pager_store_raw(&pager->nonstop, pager->nonstop_cell, raw);
}

void
yt_pager_set_nonstop(struct yt_pager_state *pager, float value)
{
	if (pager != NULL)
		pager_store_single(&pager->nonstop, pager->nonstop_cell, value);
}

void
yt_pager_set_newline_raw(struct yt_pager_state *pager,
    const uint8_t raw[4])
{
	if (pager != NULL && raw != NULL)
		pager_store_raw(&pager->newline_flag, pager->newline_flag_cell, raw);
}

void
yt_pager_set_newline(struct yt_pager_state *pager, float value)
{
	if (pager != NULL)
		pager_store_single(&pager->newline_flag, pager->newline_flag_cell,
		    value);
}

bool
yt_pager_advance(struct yt_pager_state *pager,
    struct yt_present_state *presentation, int *saved_foreground)
{
	static const uint8_t dirty_line_zero[4] = {
		0x00U, 0x00U, 0x38U, 0x00U,
	};
	static const uint8_t foreground_three[4] = {
		0x00U, 0x00U, 0x40U, 0x82U,
	};
	static const uint8_t one[4] = {0x00U, 0x00U, 0x00U, 0x81U};

	yt_pager_sync_process(pager);
	yt_pager_set_line_count(pager,
	    single_add(pager->line_count, 1.0f));
	if (pager->nonstop != 0.0f || pager->line_count < 23.0f
	    || pager->newline_flag != 0.0f)
		return false;
	*saved_foreground = pager->foreground;
	if (pager_process_bound(pager)) {
		memcpy(pager->saved_foreground_cell, pager->foreground_cell, 4U);
		yt_pager_set_line_count_raw(pager, dirty_line_zero);
		memcpy(pager->foreground_cell, foreground_three, 4U);
	}
	else
		pager->line_count = 0.0f;
	pager->foreground = 3;
	presentation->foreground = 3.0f;
	presentation->bold = 1.0f;
	if (pager_process_bound(pager))
		yt_pager_set_newline_raw(pager, one);
	else
		pager->newline_flag = 1.0f;
	return true;
}

void
yt_pager_editor_enter(struct yt_pager_state *pager, char *accumulator,
    size_t accumulator_capacity)
{
	static const uint8_t dirty_zero[4] = {
		0x00U, 0x00U, 0x0cU, 0x00U,
	};

	if (pager_process_bound(pager))
		yt_pager_set_nonstop_raw(pager, dirty_zero);
	else
		pager->nonstop = 0.0f;
	if (accumulator_capacity != 0)
		accumulator[0] = '\0';
	if (pager_process_bound(pager))
		yt_pager_set_line_count_raw(pager, dirty_zero);
	else
		pager->line_count = 0.0f;
	pager->key[0] = '\0';
}

bool
yt_pager_accept_response(struct yt_pager_state *pager, char *response,
    size_t response_capacity)
{
	(void)response_capacity;
	qb_compat_upper_n_observed((uint8_t *)response, strlen(response),
	    pager->uppercase_numeric_temp_cell != NULL
	    && pager->uppercase_length_cell != NULL
	    && pager->uppercase_index_cell != NULL ? pager_upper_store : NULL,
	    pager);
	snprintf(pager->key, sizeof(pager->key), "%s", response);
	if (strcmp(response, "NS") != 0)
		return false;
	yt_pager_set_nonstop(pager, 1.0f);
	return true;
}

void
yt_pager_complete(struct yt_pager_state *pager,
    struct yt_present_state *presentation, int saved_foreground)
{
	if (strstr(pager->key, "E") != NULL) {
		pager->key[0] = 'Q';
		pager->key[1] = '\0';
	}
	if (pager_process_bound(pager)) {
		memcpy(pager->foreground_cell, pager->saved_foreground_cell, 4U);
		pager->foreground = (int)qb_mbf32_decode(pager->foreground_cell);
		presentation->foreground = qb_mbf32_decode(
		    pager->foreground_cell);
	}
	else {
		pager->foreground = saved_foreground;
		presentation->foreground = (float)saved_foreground;
	}
}

static bool
paged_row_run(struct yt_pager_state *pager,
    struct yt_present_state *presentation,
    struct yt_b05d_key_state *key_state, const uint8_t *text,
    size_t length, const struct yt_paged_row_ops *ops, void *context)
{
	static const uint8_t prompt[] =
	    "[ENTER] for more, [E] to end, or [NS] for Non-stop ";
	static const uint8_t notice[] = "Ctrl-X to Stop";
	struct yt_input_value sampled;
	char response[80];
	int saved_foreground;
	bool emit_notice;

	if (!ops->carrier(context)
	    || !ops->sample(context, &sampled)
	    || !yt_b05d_process_key(&sampled, key_state)
	    || !ops->present(context, text, length)
	    || !ops->carrier(context)
	    || !ops->finish(context, pager->newline_flag != 0.0f))
		return false;
	if (yt_pager_advance(pager, presentation, &saved_foreground)) {
		if (!paged_row_run(pager, presentation, key_state, prompt,
		    sizeof(prompt) - 1U, ops, context)
		    || !ops->response(context, response, sizeof(response)))
			return false;
		emit_notice = yt_pager_accept_response(pager, response,
		    sizeof(response));
		if (emit_notice && !paged_row_run(pager, presentation, key_state,
		    notice, sizeof(notice) - 1U, ops, context))
			return false;
		yt_pager_complete(pager, presentation, saved_foreground);
	}
	if (pager_process_bound(pager)) {
		static const uint8_t dirty_zero[4] = {
			0x00U, 0x00U, 0x0cU, 0x00U,
		};

		yt_pager_set_newline_raw(pager, dirty_zero);
	}
	else
		pager->newline_flag = 0.0f;
	return true;
}

bool
yt_paged_row_run(struct yt_pager_state *pager,
    struct yt_present_state *presentation,
    struct yt_b05d_key_state *key_state, const uint8_t *text,
    size_t length, const struct yt_paged_row_ops *ops, void *context)
{
	if (pager == NULL || presentation == NULL || key_state == NULL
	    || (text == NULL && length != 0U) || ops == NULL
	    || ops->carrier == NULL || ops->sample == NULL
	    || ops->present == NULL || ops->finish == NULL
	    || ops->response == NULL)
		return false;
	return paged_row_run(pager, presentation, key_state, text, length,
	    ops, context);
}

void
yt_sector_pager_begin(struct yt_sector_pager_state *pager)
{
	pager->line_count = 3.0f;
}

void
yt_sector_pager_add(struct yt_sector_pager_state *pager, float lines)
{
	pager->line_count = single_add(pager->line_count, lines);
}

bool
yt_sector_pager_finish_sector(struct yt_sector_pager_state *pager)
{
	if (pager->line_count <= 15.0f)
		return false;
	pager->line_count = 0.0f;
	return true;
}

void
yt_radio_pager_begin(struct yt_radio_pager_state *pager)
{
	pager->line_count = 0.0f;
}

void
yt_radio_pager_add_pair(struct yt_radio_pager_state *pager)
{
	pager->line_count = single_add(pager->line_count, 2.0f);
}

bool
yt_radio_pager_add_body(struct yt_radio_pager_state *pager)
{
	pager->line_count = single_add(pager->line_count, 1.0f);
	if (pager->line_count <= 22.0f)
		return false;
	pager->line_count = 0.0f;
	return true;
}
