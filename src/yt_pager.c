#include "yt_pager.h"

#include "qb.h"

#include <stdio.h>
#include <string.h>

bool
yt_pager_advance(struct yt_pager_state *pager,
    struct yt_present_state *presentation, int *saved_foreground)
{
	++pager->line_count;
	if (pager->nonstop || pager->line_count < 23
	    || pager->newline_flag)
		return false;
	*saved_foreground = pager->foreground;
	pager->line_count = 0;
	pager->foreground = 3;
	presentation->foreground = 3;
	presentation->bold = true;
	pager->newline_flag = true;
	return true;
}

void
yt_pager_editor_enter(struct yt_pager_state *pager, char *accumulator,
    size_t accumulator_capacity)
{
	pager->nonstop = false;
	if (accumulator_capacity != 0)
		accumulator[0] = '\0';
	pager->line_count = 0;
	pager->key[0] = '\0';
}

bool
yt_pager_accept_response(struct yt_pager_state *pager, char *response,
    size_t response_capacity)
{
	(void)response_capacity;
	qb_compat_upper_n((uint8_t *)response, strlen(response));
	snprintf(pager->key, sizeof(pager->key), "%s", response);
	if (strcmp(response, "NS") != 0)
		return false;
	pager->nonstop = true;
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
	presentation->foreground = saved_foreground;
}

bool
yt_pager_apply_key(const struct yt_input_value *value,
    struct yt_pager_key_state *state)
{
	size_t position = *state->queue_position;
	size_t length = *state->queue_length;
	size_t queued = length - position;
	uint8_t key;

	if (value->length != 1)
		return true;
	key = value->bytes[0];
	if (key == 0x18) {
		state->accumulator[0] = '\0';
		state->queue[0] = '\0';
		*state->queue_position = 0;
		*state->queue_length = 0;
		if (state->pager_key_capacity < 2U)
			return false;
		state->pager_key[0] = 'Q';
		state->pager_key[1] = '\0';
		return true;
	}
	if (key == 0x12 && queued != 0)
		return true;
	if (key >= 0x7f || (key < 0x20 && key != '\r'))
		return true;
	if (queued + 1U >= state->queue_capacity)
		return false;
	if (queued != 0 && position != 0)
		memmove(state->queue, state->queue + position, queued);
	state->queue[queued] = (char)key;
	state->queue[queued + 1U] = '\0';
	*state->queue_position = 0;
	*state->queue_length = queued + 1U;
	return true;
}

void
yt_sector_pager_begin(struct yt_sector_pager_state *pager)
{
	pager->line_count = 3;
}

void
yt_sector_pager_add(struct yt_sector_pager_state *pager, int lines)
{
	pager->line_count += lines;
}

bool
yt_sector_pager_finish_sector(struct yt_sector_pager_state *pager)
{
	if (pager->line_count <= 15)
		return false;
	pager->line_count = 0;
	return true;
}

void
yt_radio_pager_begin(struct yt_radio_pager_state *pager)
{
	pager->line_count = 0;
}

void
yt_radio_pager_add_pair(struct yt_radio_pager_state *pager)
{
	pager->line_count += 2;
}

bool
yt_radio_pager_add_body(struct yt_radio_pager_state *pager)
{
	++pager->line_count;
	if (pager->line_count <= 22)
		return false;
	pager->line_count = 0;
	return true;
}
