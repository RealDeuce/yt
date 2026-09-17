#include "yt_game.h"
#include "yt_game_internal.h"

#include "qb.h"
#include "yt_main_error.h"

#include <limits.h>
#include <math.h>
#include <string.h>

float
yt_sector_mine_batch(float mines_before)
{
	volatile float quotient;

	if (!(mines_before > 19.0f))
		return 1.0f;
	quotient = mines_before / 10.0f;
	return floorf(quotient);
}

float
yt_sector_mine_shield_result(float shields, float batch, float draw)
{
	volatile float product = draw * 1001.0f;
	volatile float quantum = floorf(product);
	volatile float loss = quantum * batch;
	volatile float result = shields - loss;

	return result < 1.0f ? 0.0f : result;
}

float
yt_sector_mine_cloak_loss(float cloak, float batch, float draw)
{
	volatile float first = draw * batch;
	volatile float scaled = first * 100.0f;
	volatile float integral = floorf(scaled);
	volatile float loss = integral / 100.0f;

	return loss > cloak ? cloak : loss;
}

float
yt_sector_mine_missile_loss(float missiles, float batch, float draw)
{
	volatile float range = batch * missiles;
	volatile float product = draw * range;
	volatile float loss = floorf(product) + 1.0f;

	return loss > missiles ? missiles : loss;
}

float
yt_sector_mine_empty_holds(const struct yt_player *player)
{
	volatile float empty;

	if (player == NULL)
		return 0.0f;
	empty = player->holds - player->equipment;
	empty = empty - player->organics;
	empty = empty - player->ore;
	return empty;
}

void
yt_sector_mine_sector_overlay(struct yt_sector *sector, float mines_after)
{
	if (sector == NULL)
		return;
	sector->mines = mines_after;
	(void)yt_record_set_number(&sector->record, YT_F129, mines_after);
}

void
yt_sector_mine_player_overlay(struct yt_player *fresh,
    const struct yt_player *working, unsigned fields)
{
	static const uint8_t scanner_zero[4] = {0x00, 0x00, 0x48, 0x00};

	if (fresh == NULL || working == NULL)
		return;
#define MINE_OVERLAY(flag, member, offset) do { \
	if ((fields & (flag)) != 0U) { \
		fresh->member = working->member; \
		(void)yt_record_set_number(&fresh->record, (offset), \
		    working->member); \
	} \
} while (0)
	MINE_OVERLAY(YT_SECTOR_MINE_DAMAGE_SHIELDS, shields, YT_F53);
	MINE_OVERLAY(YT_SECTOR_MINE_DAMAGE_FIGHTERS, fighters, YT_F61);
	MINE_OVERLAY(YT_SECTOR_MINE_DAMAGE_HOLDS, holds, YT_F65);
	MINE_OVERLAY(YT_SECTOR_MINE_DAMAGE_ORE, ore, YT_F69);
	MINE_OVERLAY(YT_SECTOR_MINE_DAMAGE_ORGANICS, organics, YT_F73);
	MINE_OVERLAY(YT_SECTOR_MINE_DAMAGE_EQUIPMENT, equipment, YT_F77);
	if ((fields & YT_SECTOR_MINE_DAMAGE_SCANNER) != 0U) {
		fresh->danger_scanner = working->danger_scanner;
		if (working->danger_scanner == 0)
			(void)yt_record_set_raw_number(&fresh->record, YT_F93,
			    scanner_zero);
		else
			(void)yt_record_set_number(&fresh->record, YT_F93,
			    (float)working->danger_scanner);
	}
	MINE_OVERLAY(YT_SECTOR_MINE_DAMAGE_MISSILES, missiles, YT_F97);
	MINE_OVERLAY(YT_SECTOR_MINE_DAMAGE_CLOAK, cloak, YT_F125);
	MINE_OVERLAY(YT_SECTOR_MINE_DAMAGE_CARRIED_MINES, mines, YT_F129);
#undef MINE_OVERLAY
}

bool
yt_sector_mine_explosion_row(float mines_before, float batch,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t first[] = "There are";
	static const uint8_t middle[] = " mines here!";
	static const uint8_t suffix[] = " EXPLODE!";
	char before_text[64];
	char batch_text[64];
	int before_length = qb_str_single(before_text, sizeof(before_text),
	    mines_before);
	int batch_length = qb_str_single(batch_text, sizeof(batch_text), batch);

	if (before_length < 0 || batch_length < 0)
		return false;
	return yt_game_join_parts(first, sizeof(first) - 1U,
	    (const uint8_t *)before_text, (size_t)before_length,
	    middle, sizeof(middle) - 1U,
	    (const uint8_t *)batch_text, (size_t)batch_length,
	    suffix, sizeof(suffix) - 1U, row, capacity, length);
}

bool
yt_sector_mine_shields_row(float shields, uint8_t *row,
    size_t capacity, size_t *length)
{
	static const uint8_t first[] = "Shields down to";
	static const uint8_t suffix[] = " units!";
	char number[64];
	int number_length = qb_str_single(number, sizeof(number), shields);

	if (number_length < 0)
		return false;
	return yt_game_join_parts(first, sizeof(first) - 1U,
	    (const uint8_t *)number, (size_t)number_length,
	    suffix, sizeof(suffix) - 1U, NULL, 0U, NULL, 0U,
	    row, capacity, length);
}

bool
yt_sector_mine_loss_row(enum yt_sector_mine_loss_kind kind, float loss,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t first[] = "Lost";
	static const char *const suffixes[] = {
		" fighters!", "% cloak!", " Missiles!", " mines!",
		" holds of ore!", " holds of organics!",
		" holds of equipment!", " empty holds!",
	};
	char number[64];
	int number_length;
	const char *suffix;

	if (kind < YT_SECTOR_MINE_LOSS_FIGHTERS
	    || kind > YT_SECTOR_MINE_LOSS_EMPTY_HOLDS)
		return false;
	suffix = suffixes[kind];
	number_length = qb_str_single(number, sizeof(number), loss);
	if (number_length < 0)
		return false;
	return yt_game_join_parts(first, sizeof(first) - 1U,
	    (const uint8_t *)number, (size_t)number_length,
	    (const uint8_t *)suffix, strlen(suffix), NULL, 0U, NULL, 0U,
	    row, capacity, length);
}

bool
yt_sector_mine_entry_news(const uint8_t *player_name,
    size_t player_name_length, float sector, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t middle[] = " hit sector mines in sector";
	static const uint8_t suffix[] = "!";
	char number[64];
	int number_length = qb_str_single(number, sizeof(number), sector);

	if (number_length < 0)
		return false;
	return yt_game_join_parts(player_name, player_name_length,
	    middle, sizeof(middle) - 1U,
	    (const uint8_t *)number, (size_t)number_length,
	    suffix, sizeof(suffix) - 1U, NULL, 0U,
	    row, capacity, length);
}

bool
yt_sector_mine_final_news(float shields, uint8_t *row,
    size_t capacity, size_t *length)
{
	static const uint8_t first[] = "Shields reduced to";
	static const uint8_t suffix[] = " units!";
	char number[64];
	int number_length = qb_str_single(number, sizeof(number), shields);

	if (number_length < 0)
		return false;
	return yt_game_join_parts(first, sizeof(first) - 1U,
	    (const uint8_t *)number, (size_t)number_length,
	    suffix, sizeof(suffix) - 1U, NULL, 0U, NULL, 0U,
	    row, capacity, length);
}

bool
yt_direct_fighter_mine_warning(const uint8_t *victim_name,
    size_t victim_name_length, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t prefix[] = "  -  ";
	static const uint8_t suffix[] =
	    " had sector mines! They EXPLODED!";

	return yt_game_join_parts(prefix, sizeof(prefix) - 1U,
	    victim_name, victim_name_length, suffix, sizeof(suffix) - 1U,
	    NULL, 0U, NULL, 0U, row, capacity, length);
}

float
yt_emergency_warp_duration(float first, float second)
{
	volatile float first_part = first * 70.0f;
	volatile float second_part = second * 70.0f;
	volatile float result = first_part + second_part;

	return result;
}

int
yt_emergency_warp_destination(float draw, int sector_count)
{
	volatile float product = draw * (float)sector_count;
	volatile float integral = floorf(product);
	volatile float result = integral + 1.0f;

	return (int)result;
}

float
yt_emergency_warp_cost(float heat, float draw, float turns, bool meltdown)
{
	volatile float heat_cost = heat * 4.0f;
	volatile float jitter_product = draw * 4.0f;
	volatile float jitter = floorf(jitter_product);
	volatile float result = heat_cost + jitter;

	if (result > turns)
		result = turns;
	if (meltdown)
		result = turns;
	return result;
}

void
yt_emergency_warp_player_overlay(struct yt_player *player,
    int destination, float cost)
{
	volatile float remaining;

	if (player == NULL)
		return;
	remaining = player->turns - cost;
	player->sector = destination;
	player->turns = remaining;
	(void)yt_record_set_number(&player->record, YT_F57,
	    (float)destination);
	(void)yt_record_set_number(&player->record, YT_F49, remaining);
}

bool
yt_emergency_warp_result_row(int destination, float cost,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t first[] = "sector";
	static const uint8_t middle[] =
	    ". However, it takes you";
	static const uint8_t suffix[] =
	    " turns to recharge your engines!";
	char destination_text[64];
	char cost_text[64];
	int destination_length = qb_str_single(destination_text,
	    sizeof(destination_text), (float)destination);
	int cost_length = qb_str_single(cost_text, sizeof(cost_text), cost);

	if (destination_length < 0 || cost_length < 0)
		return false;
	return yt_game_join_parts(first, sizeof(first) - 1U,
	    (const uint8_t *)destination_text, (size_t)destination_length,
	    middle, sizeof(middle) - 1U,
	    (const uint8_t *)cost_text, (size_t)cost_length,
	    suffix, sizeof(suffix) - 1U, row, capacity, length);
}

bool
yt_emergency_warp_stranded_row(int destination, uint8_t *row,
    size_t capacity, size_t *length)
{
	static const uint8_t first[] = "You are stranded in sector";
	static const uint8_t suffix[] = ".";
	char destination_text[64];
	int destination_length = qb_str_single(destination_text,
	    sizeof(destination_text), (float)destination);

	if (destination_length < 0)
		return false;
	return yt_game_join_parts(first, sizeof(first) - 1U,
	    (const uint8_t *)destination_text, (size_t)destination_length,
	    suffix, sizeof(suffix) - 1U, NULL, 0U, NULL, 0U,
	    row, capacity, length);
}

bool
yt_movement_warp_row(const int warps[6], uint8_t *row,
    size_t capacity, size_t *length)
{
	static const uint8_t heading[] = "Warps lead to";
	size_t used = sizeof(heading) - 1U;
	size_t slot;

	if (warps == NULL || row == NULL || length == NULL
	    || used > capacity)
		return false;
	memcpy(row, heading, used);
	for (slot = 0U; slot < 6U; ++slot) {
		char number[64];
		int number_length;

		if (warps[slot] == 0)
			continue;
		number_length = qb_str_single(number, sizeof(number),
		    (float)warps[slot]);
		if (number_length < 0 || used + 1U + (size_t)number_length
		    > capacity)
			return false;
		row[used++] = ',';
		memcpy(row + used, number, (size_t)number_length);
		used += (size_t)number_length;
	}
	*length = used;
	return true;
}

bool
yt_movement_confirmation_prompt(float target, uint8_t *row,
    size_t capacity, size_t *length)
{
	static const uint8_t first[] = "Move into sector";
	static const uint8_t suffix[] = "? [y/N] ";
	char number[64];
	int number_length = qb_str_single(number, sizeof(number), target);

	if (number_length < 0)
		return false;
	return yt_game_join_parts(first, sizeof(first) - 1U,
	    (const uint8_t *)number, (size_t)number_length,
	    suffix, sizeof(suffix) - 1U, NULL, 0U, NULL, 0U,
	    row, capacity, length);
}

void
yt_movement_player_overlay(struct yt_player *player, int target)
{
	if (player == NULL)
		return;
	player->sector = target;
	(void)yt_record_set_number(&player->record, YT_F57, (float)target);
}
