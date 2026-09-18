#include "info_panel_model.h"

#include "qb.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static float
single_mul(float left, float right)
{
	return left * right;
}

static bool
append_bytes(uint8_t *row, size_t capacity, size_t *length,
    const void *text, size_t text_length)
{
	if (row == NULL || length == NULL || *length > capacity
	    || text_length > capacity - *length
	    || (text == NULL && text_length != 0U))
		return false;
	if (text_length != 0U)
		memcpy(row + *length, text, text_length);
	*length += text_length;
	return true;
}

static bool
present(struct yt_info_panel_state *state,
    const struct yt_info_panel_ops *ops, void *context,
    const uint8_t *text, size_t length, enum yt_info_panel_output_kind kind,
    uint8_t width, struct yt_error *error)
{
	return ops->present(context, text, length, kind, width, state, error);
}

static bool
cell(uint8_t *value, size_t capacity, size_t *length,
    const char *label, const char *text)
{
	static const uint8_t bar = 0xba;

	*length = 0U;
	return append_bytes(value, capacity, length, &bar, 1U)
	    && append_bytes(value, capacity, length, label, strlen(label))
	    && append_bytes(value, capacity, length, text, strlen(text));
}

static bool
ordinary(struct yt_info_panel_state *state,
    const struct yt_info_panel_ops *ops, void *context,
    const char *left_label, const char *left_value,
    const char *right_label, const char *right_value,
    struct yt_error *error)
{
	static const uint8_t bar = 0xba;
	uint8_t left[160];
	uint8_t right[160];
	size_t left_length;
	size_t right_length;

	return cell(left, sizeof(left), &left_length, left_label, left_value)
	    && cell(right, sizeof(right), &right_length, right_label,
	    right_value)
	    && present(state, ops, context, left, left_length,
	    YT_INFO_PANEL_FIXED, 26U, error)
	    && present(state, ops, context, right, right_length,
	    YT_INFO_PANEL_FIXED, 23U, error)
	    && present(state, ops, context, &bar, 1U,
	    YT_INFO_PANEL_LINE, 0U, error);
}

static bool
commodity(struct yt_info_panel_state *state,
    const struct yt_info_panel_ops *ops, void *context,
    const char *left_label, const char *left_value,
    const char *right_label, float right_value, struct yt_error *error)
{
	static const uint8_t bar = 0xba;
	uint8_t left[160];
	uint8_t right[160];
	char number[64];
	size_t left_length;
	size_t right_length;
	int number_length;

	number_length = qb_str_single(number, sizeof(number), right_value);
	if (number_length < 0
	    || !cell(left, sizeof(left), &left_length, left_label, left_value)
	    || !cell(right, sizeof(right), &right_length, right_label, "")
	    || !present(state, ops, context, left, left_length,
	    YT_INFO_PANEL_FIXED, 26U, error)
	    || !present(state, ops, context, right, right_length,
	    YT_INFO_PANEL_FIXED, 17U, error))
		return false;
	if (right_value != 0.0f) {
		state->bold = true;
		state->foreground = 7.0f;
		state->background = 4.0f;
	}
	if (!present(state, ops, context, (const uint8_t *)number,
	    (size_t)number_length, YT_INFO_PANEL_FIXED, 6U, error))
		return false;
	state->foreground = 2.0f;
	state->background = 0.0f;
	return present(state, ops, context, &bar, 1U,
	    YT_INFO_PANEL_LINE, 0U, error);
}

bool
yt_info_panel_run(struct yt_info_panel_state *state,
    const struct yt_info_panel_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t top[50] = {
		0xc9, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
		0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
		0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcb, 0xcd, 0xcd, 0xcd,
		0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
		0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xbb
	};
	static const uint8_t bottom[50] = {
		0xc8, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
		0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
		0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xca, 0xcd, 0xcd, 0xcd,
		0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
		0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xbc
	};
	static const uint8_t title[] = "[ Info ]";
	uint8_t row[256];
	char left[64];
	char right[64];
	size_t row_length;
	int saved_foreground;
	uint8_t cloak_percent;
	int length;

	if (state == NULL || ops == NULL || ops->refresh_time == NULL
	    || ops->team == NULL || ops->read_player == NULL
	    || ops->present == NULL
	    || (state->cached_name == NULL && state->cached_name_length != 0U))
		return false;
	if (!ops->refresh_time(context, state->time_text,
	    sizeof(state->time_text), &state->time_text_length, error)
	    || state->time_text_length > sizeof(state->time_text))
		return false;
	saved_foreground = state->foreground;
	state->foreground = 2.0f;
	if (!present(state, ops, context, NULL, 0U,
	    YT_INFO_PANEL_LINE, 0U, error)
	    || !present(state, ops, context, NULL, 0U,
	    YT_INFO_PANEL_FIXED, 20U, error)
	    || !present(state, ops, context, title, sizeof(title) - 1U,
	    YT_INFO_PANEL_LINE, 0U, error)
	    || !present(state, ops, context, NULL, 0U,
	    YT_INFO_PANEL_LINE, 0U, error))
		return false;
	row_length = 0U;
	if (!append_bytes(row, sizeof(row), &row_length, "Name  : ", 8U)
	    || !append_bytes(row, sizeof(row), &row_length,
	    state->cached_name, state->cached_name_length)
	    || !present(state, ops, context, row, row_length,
	    YT_INFO_PANEL_LINE, 0U, error))
		return false;
	row_length = 0U;
	if (!append_bytes(row, sizeof(row), &row_length, "Time  :", 7U)
	    || !append_bytes(row, sizeof(row), &row_length,
	    state->time_text, state->time_text_length)
	    || !present(state, ops, context, row, row_length,
	    YT_INFO_PANEL_LINE, 0U, error)
	    || !ops->team(context, error)
	    || !ops->read_player(context, &state->player, error)
	    || !present(state, ops, context, top, sizeof(top),
	    YT_INFO_PANEL_LINE, 0U, error))
		return false;
	length = qb_str_double(left, sizeof(left), (double)state->player.credits);
	if (length < 0)
		return false;
	length = qb_str_single(right, sizeof(right),
	    (float)state->player.sector);
	if (length < 0 || !ordinary(state, ops, context,
	    " Credits.. :", left, " Sector....... :", right, error))
		return false;
	if (qb_str_single(left, sizeof(left), state->player.turns) < 0
	    || qb_str_single(right, sizeof(right), state->player.holds) < 0
	    || !ordinary(state, ops, context, " Turns.... :", left,
	    " Holds........ :", right, error))
		return false;
	if (qb_str_double(left, sizeof(left),
	    (double)state->player.fighters) < 0
	    || !commodity(state, ops, context, " Fighters. :", left,
	    " Ore.......... :", state->player.ore, error))
		return false;
	if (qb_str_single(left, sizeof(left), state->player.mines) < 0
	    || !commodity(state, ops, context, " Mines.... :", left,
	    " Organics..... :", state->player.organics, error))
		return false;
	if (qb_str_single(left, sizeof(left), state->player.missiles) < 0
	    || !commodity(state, ops, context, " Missiles. :", left,
	    " Equipment.... :", state->player.equipment, error))
		return false;
	(void)snprintf(left, sizeof(left), "%s",
	    state->player.danger_scanner == 0 ? " NONE" : " Installed");
	if (qb_str_single(right, sizeof(right),
	    (float)state->player.ports_owned) < 0
	    || !ordinary(state, ops, context, " Scanner.. :", left,
	    " Ports Owned.. :", right, error))
		return false;
	if (qb_str_double(left, sizeof(left),
	    (double)state->player.shields) < 0)
		return false;
	if (state->anti_cloak != 0.0f)
		(void)snprintf(right, sizeof(right), "%s", " FAIL");
	else {
		cloak_percent = (uint8_t)floorf(single_mul(state->player.cloak,
		    100.0f));
		if (qb_str_single(right, sizeof(right), (float)cloak_percent) < 0
		    || strlen(right) + 1U >= sizeof(right))
			return false;
		strcat(right, "%");
	}
	if (!ordinary(state, ops, context, " Shields.. :", left,
	    " Cloak Energy. :", right, error)
	    || qb_str_single(left, sizeof(left),
	    state->player.ground_forces) < 0
	    || qb_str_single(right, sizeof(right), state->player.plasma) < 0
	    || !ordinary(state, ops, context, " Forces... :", left,
	    " Plasma Bolts. :", right, error)
	    || !present(state, ops, context, bottom, sizeof(bottom),
	    YT_INFO_PANEL_LINE, 0U, error))
		return false;
	state->foreground = saved_foreground;
	return true;
}
