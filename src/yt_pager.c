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

bool
yt_pager_advance(struct yt_pager_state *pager,
    struct yt_present_state *presentation, int *saved_foreground)
{
	pager->line_count = single_add(pager->line_count, 1.0f);
	if (pager->nonstop != 0.0f || pager->line_count < 23.0f
	    || pager->newline_flag != 0.0f)
		return false;
	*saved_foreground = pager->foreground;
	pager->line_count = 0.0f;
	pager->foreground = 3;
	presentation->foreground = 3.0f;
	presentation->bold = 1.0f;
	pager->newline_flag = 1.0f;
	return true;
}

void
yt_pager_editor_enter(struct yt_pager_state *pager, char *accumulator,
    size_t accumulator_capacity)
{
	pager->nonstop = 0.0f;
	if (accumulator_capacity != 0)
		accumulator[0] = '\0';
	pager->line_count = 0.0f;
	pager->key[0] = '\0';
}

bool
yt_pager_accept_response(struct yt_pager_state *pager, char *response,
    size_t response_capacity)
{
	(void)response_capacity;
	qb_compat_upper(response);
	snprintf(pager->key, sizeof(pager->key), "%s", response);
	if (strcmp(response, "NS") != 0)
		return false;
	pager->nonstop = 1.0f;
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
	pager->foreground = saved_foreground;
	presentation->foreground = (float)saved_foreground;
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
