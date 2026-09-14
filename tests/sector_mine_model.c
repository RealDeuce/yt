#include "sector_mine_model.h"

#include "qb.h"

#include <stdio.h>
#include <string.h>

static float
test_single_mul(float left, float right)
{
	volatile float result = left * right;

	return result;
}

static float
test_single_sub(float left, float right)
{
	volatile float result = left - right;

	return result;
}

static void
test_sector_mine_apply_style(struct test_sector_mine_state *state,
    const struct test_sector_mine_ops *ops, void *context)
{
	if (ops->style != NULL)
		ops->style(context, state->foreground, state->background,
		    state->blink, state->pager_foreground);
}

static bool
test_sector_mine_stock_loss(const struct test_sector_mine_ops *ops, void *context,
    float batch, float *stock, float *loss, struct yt_error *error)
{
	float sampled;

	if (!ops->shrink(context, test_single_mul(batch, *stock),
	    &sampled, error))
		return false;
	if (sampled > *stock)
		sampled = *stock;
	*stock = test_single_sub(*stock, sampled);
	*loss = sampled;
	return true;
}

bool
test_sector_mine_run(struct test_sector_mine_state *state,
    const struct test_sector_mine_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t warning[] = "** Sector is Mined!! **";
	static const uint8_t shields_destroyed[] = "Shields disintegrated!";
	static const uint8_t scanner_destroyed[] =
	    "Danger scanner destroyed!";
	uint8_t row[300];
	size_t row_length;
	bool overflow;
	int32_t converted;
	int current;

	if (state == NULL || state->destroyed == NULL || ops == NULL
	    || ops->read_current == NULL || ops->read_player == NULL
	    || ops->write_player == NULL || ops->read_sector == NULL
	    || ops->write_sector == NULL || ops->present == NULL
	    || ops->sound == NULL || ops->news == NULL || ops->random == NULL
	    || ops->shrink == NULL || ops->emergency_warp == NULL)
		return false;
	state->terminal = false;
	state->complete = false;
	state->batches = 0U;
	converted = qb_cint_mode((double)state->current_sector,
	    state->conversion_mode, &overflow);
	if (overflow) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "sector mine sector record CINT");
		}
		return false;
	}
	current = (int)converted;
	if (!ops->present(context, NULL, 0U, TEST_SECTOR_MINE_OUTPUT_LINE,
	    error))
		return false;
	state->blink = 1.0f;
	test_sector_mine_apply_style(state, ops, context);
	if (!ops->present(context, warning, sizeof(warning) - 1U,
	    TEST_SECTOR_MINE_OUTPUT_LINE, error)
	    || !ops->sound(context, 5.0f, error)
	    || !ops->read_current(context, &state->player, error))
		return false;
	converted = qb_cint_mode((double)state->player.name_length,
	    state->conversion_mode, &overflow);
	if (overflow || converted < 0) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "sector mine player name length");
		}
		return false;
	}
	if ((size_t)converted > YT_TEXT_FIELD_SIZE)
		converted = YT_TEXT_FIELD_SIZE;
	if (!yt_sector_mine_entry_news(state->player.record.bytes,
	    (size_t)converted, state->current_sector, row, sizeof(row),
	    &row_length)
	    || !ops->news(context, row, row_length, error))
		return false;

	for (;;) {
		struct yt_player working;
		struct yt_player persisted;
		float saved_foreground;
		float draw;
		float loss;
		float empty;

		state->touched = 0U;
		if (!ops->read_sector(context, current, &state->sector, error))
			return false;
		state->mines_before = state->sector.mines;
		state->batch = yt_sector_mine_batch(state->mines_before);
		yt_sector_mine_sector_overlay(&state->sector,
		    test_single_sub(state->mines_before, state->batch));
		if (!ops->write_sector(context, current, &state->sector, error))
			return false;
		++state->batches;
		saved_foreground = state->foreground;
		state->foreground = 3.0f;
		state->background = 0.0f;
		state->blink = 0.0f;
		state->pager_foreground = 3;
		test_sector_mine_apply_style(state, ops, context);
		if (!yt_sector_mine_explosion_row(state->mines_before,
		    state->batch, row, sizeof(row), &row_length)
		    || !ops->present(context, row, row_length,
		    TEST_SECTOR_MINE_OUTPUT_BOLD_RAW, error))
			return false;
		state->background = 1.0f;
		test_sector_mine_apply_style(state, ops, context);
		if (!ops->present(context, NULL, 0U,
		    TEST_SECTOR_MINE_OUTPUT_LINE, error)
		    || !ops->read_current(context, &state->player, error))
			return false;
		working = state->player;
		if (working.shields > 0.0f) {
			if (!ops->random(context, &draw, error))
				return false;
			working.shields = yt_sector_mine_shield_result(
			    working.shields, state->batch, draw);
			state->touched |= YT_SECTOR_MINE_DAMAGE_SHIELDS;
			if (working.shields == 0.0f) {
				state->foreground = 7.0f;
				state->blink = 1.0f;
				state->pager_foreground = 7;
				test_sector_mine_apply_style(state, ops, context);
				if (!ops->present(context, shields_destroyed,
				    sizeof(shields_destroyed) - 1U,
				    TEST_SECTOR_MINE_OUTPUT_BOLD_LINE, error))
					return false;
				state->foreground = saved_foreground;
				state->pager_foreground = (int)saved_foreground;
				test_sector_mine_apply_style(state, ops, context);
			} else {
				if (!yt_sector_mine_shields_row(working.shields,
				    row, sizeof(row), &row_length)
				    || !ops->present(context, row, row_length,
				    TEST_SECTOR_MINE_OUTPUT_BOLD_LINE, error)
				    || !ops->random(context, &draw, error))
					return false;
				if (working.danger_scanner != 0.0f
				    && draw > 0.949999988079071f) {
					working.danger_scanner = 0.0f;
					state->touched |=
					    YT_SECTOR_MINE_DAMAGE_SCANNER;
					state->foreground = 7.0f;
					state->blink = 1.0f;
					state->pager_foreground = 7;
					test_sector_mine_apply_style(state, ops, context);
					if (!ops->present(context, scanner_destroyed,
					    sizeof(scanner_destroyed) - 1U,
					    TEST_SECTOR_MINE_OUTPUT_BOLD_LINE, error))
						return false;
					state->foreground = saved_foreground;
					state->pager_foreground =
					    (int)saved_foreground;
					test_sector_mine_apply_style(state, ops, context);
				}
			}
		} else {
#define MINE_LOSS_ROW(kind, operation) do { \
	if (!yt_sector_mine_loss_row((kind), loss, row, sizeof(row), \
	    &row_length) || !ops->present(context, row, row_length, \
	    TEST_SECTOR_MINE_OUTPUT_LINE, error)) \
		return false; \
} while (0)
			if (working.fighters != 0.0f) {
				if (!ops->shrink(context,
				    test_single_mul(40000.0f, state->batch),
				    &loss, error))
					return false;
				if (loss > working.fighters)
					loss = working.fighters;
				working.fighters = test_single_sub(
				    working.fighters, loss);
				state->touched |= YT_SECTOR_MINE_DAMAGE_FIGHTERS;
				MINE_LOSS_ROW(YT_SECTOR_MINE_LOSS_FIGHTERS,
				    "sector mine fighter loss");
			}
			if (working.cloak != 0.0f) {
				if (!ops->random(context, &draw, error))
					return false;
				loss = yt_sector_mine_cloak_loss(working.cloak,
				    state->batch, draw);
				working.cloak = test_single_sub(
				    working.cloak, loss);
				state->touched |= YT_SECTOR_MINE_DAMAGE_CLOAK;
				MINE_LOSS_ROW(YT_SECTOR_MINE_LOSS_CLOAK,
				    "sector mine cloak loss");
			}
			if (working.missiles != 0.0f) {
				if (!ops->random(context, &draw, error))
					return false;
				loss = yt_sector_mine_missile_loss(working.missiles,
				    state->batch, draw);
				working.missiles = test_single_sub(
				    working.missiles, loss);
				state->touched |= YT_SECTOR_MINE_DAMAGE_MISSILES;
				MINE_LOSS_ROW(YT_SECTOR_MINE_LOSS_MISSILES,
				    "sector mine missile loss");
			}
			if (working.danger_scanner != 0.0f) {
				working.danger_scanner = 0.0f;
				state->touched |= YT_SECTOR_MINE_DAMAGE_SCANNER;
				state->foreground = 7.0f;
				state->blink = 1.0f;
				state->pager_foreground = 7;
				test_sector_mine_apply_style(state, ops, context);
				if (!ops->present(context, scanner_destroyed,
				    sizeof(scanner_destroyed) - 1U,
				    TEST_SECTOR_MINE_OUTPUT_BOLD_LINE, error))
					return false;
				state->foreground = saved_foreground;
				state->pager_foreground = (int)saved_foreground;
				test_sector_mine_apply_style(state, ops, context);
			}
#define MINE_STOCK(member, flag, kind) do { \
	if (working.member != 0.0f) { \
		if (!test_sector_mine_stock_loss(ops, context, state->batch, \
		    &working.member, &loss, error)) \
			return false; \
		state->touched |= (flag); \
		MINE_LOSS_ROW((kind), "sector mine stock loss"); \
	} \
} while (0)
			MINE_STOCK(mines, YT_SECTOR_MINE_DAMAGE_CARRIED_MINES,
			    YT_SECTOR_MINE_LOSS_MINES);
			MINE_STOCK(ore, YT_SECTOR_MINE_DAMAGE_ORE,
			    YT_SECTOR_MINE_LOSS_ORE);
			MINE_STOCK(organics, YT_SECTOR_MINE_DAMAGE_ORGANICS,
			    YT_SECTOR_MINE_LOSS_ORGANICS);
			MINE_STOCK(equipment, YT_SECTOR_MINE_DAMAGE_EQUIPMENT,
			    YT_SECTOR_MINE_LOSS_EQUIPMENT);
#undef MINE_STOCK
			empty = yt_sector_mine_empty_holds(&working);
			if (empty > 0.0f) {
				if (!ops->shrink(context, empty, &loss, error))
					return false;
				loss = test_single_mul(loss, state->batch);
				if (loss > empty)
					loss = empty;
			working.holds = test_single_sub(working.holds,
			    loss);
			if (working.holds < 1.0f) {
				working.holds = 0.0f;
				*state->destroyed = true;
			}
				state->touched |= YT_SECTOR_MINE_DAMAGE_HOLDS;
				MINE_LOSS_ROW(YT_SECTOR_MINE_LOSS_EMPTY_HOLDS,
				    "sector mine empty hold loss");
			}
#undef MINE_LOSS_ROW
		}
		if (!ops->read_player(context, state->current_player_record,
		    &persisted, error))
			return false;
		yt_sector_mine_player_overlay(&persisted, &working,
		    state->touched);
		if (!ops->write_player(context, state->current_player_record,
		    &persisted, error))
			return false;
		working.record = persisted.record;
		state->player = working;
		if (ops->set_current != NULL)
			ops->set_current(context, &working);
		if (!ops->sound(context, 2.0f, error)
		    || !ops->random(context, &draw, error))
			return false;
		if (draw > 0.800000011920929f && working.holds < 10.0f) {
			if (!ops->emergency_warp(context, error))
				return false;
			state->terminal = true;
			state->complete = true;
			return true;
		}
		if (state->sector.mines > 0.0f && !*state->destroyed)
			continue;
		break;
	}
	if (!yt_sector_mine_final_news(state->player.shields, row,
	    sizeof(row), &row_length)
	    || !ops->news(context, row, row_length, error)
	    || !ops->read_sector(context, current, &state->sector, error))
		return false;
	state->complete = true;
	return true;
}

