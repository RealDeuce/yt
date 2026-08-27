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
