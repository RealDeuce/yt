#include "yt_game.h"

#include "qb.h"

#include <string.h>

bool
yt_projectile_sector_mine_hit_row(double mines, uint16_t sector,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t hit_prefix[] = "The missiles hit";
	static const uint8_t hit_middle[] = " SECTOR MINES in sector";
	char mine_text[64];
	char sector_text[64];
	int mine_length;
	int sector_length;
	size_t row_length;

	if (row == NULL || length == NULL)
		return false;
	sector_length = qb_str_single(sector_text, sizeof(sector_text),
	    (float)sector);
	mine_length = qb_str_double(mine_text, sizeof(mine_text), mines);
	if (sector_length < 0 || mine_length < 0
	    || sizeof(hit_prefix) - 1U + (size_t)mine_length
	    + sizeof(hit_middle) - 1U + (size_t)sector_length + 1U
	    > capacity)
		return false;
	memcpy(row, hit_prefix, sizeof(hit_prefix) - 1U);
	memcpy(row + sizeof(hit_prefix) - 1U, mine_text,
	    (size_t)mine_length);
	row_length = sizeof(hit_prefix) - 1U + (size_t)mine_length;
	memcpy(row + row_length, hit_middle, sizeof(hit_middle) - 1U);
	row_length += sizeof(hit_middle) - 1U;
	memcpy(row + row_length, sector_text, (size_t)sector_length);
	row_length += (size_t)sector_length;
	row[row_length++] = '!';
	*length = row_length;
	return true;
}

bool
yt_projectile_sector_mine_news_row(const uint8_t *shooter,
    size_t shooter_length, uint16_t sector, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t middle[] =
	    "'s Missiles hit sector mines in sector";
	char sector_text[64];
	int sector_length;
	size_t row_length = 0U;

	if (row == NULL || length == NULL
	    || (shooter == NULL && shooter_length != 0U))
		return false;
	sector_length = qb_str_single(sector_text, sizeof(sector_text),
	    (float)sector);
	if (sector_length < 0 || shooter_length + sizeof(middle) - 1U
	    + (size_t)sector_length + 1U > capacity)
		return false;
	if (shooter_length != 0U) {
		memcpy(row, shooter, shooter_length);
		row_length = shooter_length;
	}
	memcpy(row + row_length, middle, sizeof(middle) - 1U);
	row_length += sizeof(middle) - 1U;
	memcpy(row + row_length, sector_text, (size_t)sector_length);
	row_length += (size_t)sector_length;
	row[row_length++] = '!';
	*length = row_length;
	return true;
}

bool
yt_projectile_sector_mine_destroyed_row(float destroyed,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "The missile";
	static const uint8_t middle[] = " destroyed";
	static const uint8_t mine[] = " mine";
	char number[64];
	int number_length;
	size_t plural = destroyed > 1.0f ? 1U : 0U;
	size_t row_length;

	if (row == NULL || length == NULL)
		return false;
	number_length = qb_str_single(number, sizeof(number), destroyed);
	if (number_length < 0 || sizeof(prefix) - 1U + plural
	    + sizeof(middle) - 1U + (size_t)number_length
	    + sizeof(mine) - 1U + plural + 1U > capacity)
		return false;
	memcpy(row, prefix, sizeof(prefix) - 1U);
	row_length = sizeof(prefix) - 1U;
	if (plural != 0U)
		row[row_length++] = 's';
	memcpy(row + row_length, middle, sizeof(middle) - 1U);
	row_length += sizeof(middle) - 1U;
	memcpy(row + row_length, number, (size_t)number_length);
	row_length += (size_t)number_length;
	memcpy(row + row_length, mine, sizeof(mine) - 1U);
	row_length += sizeof(mine) - 1U;
	if (plural != 0U)
		row[row_length++] = 's';
	row[row_length++] = '!';
	*length = row_length;
	return true;
}

bool
yt_projectile_survivor_overlay(struct yt_player *player, float shields,
    double fighters, int scanner, bool scanner_disabled)
{
	static const uint8_t scanner_zero[4] = {
		0x00, 0x00, 0x48, 0x00
	};

	if (player == NULL)
		return false;
	player->shields = shields;
	player->fighters = (float)fighters;
	player->danger_scanner = scanner_disabled ? 0 : scanner;
	if (!yt_record_set_number(&player->record, YT_F53, shields))
		return false;
	if (!yt_record_set_number(&player->record, YT_F61,
	    player->fighters))
		return false;
	if (scanner_disabled)
		return yt_record_set_raw_number(&player->record, YT_F93,
		    scanner_zero);
	return yt_record_set_number(&player->record, YT_F93, (float)scanner);
}

bool
yt_projectile_victim_mines_overlay(struct yt_player *player,
    float *saved_mines)
{
	if (player == NULL || saved_mines == NULL)
		return false;
	*saved_mines = player->mines;
	player->mines = 0.0f;
	return yt_record_set_number(&player->record, YT_F129, 0.0f);
}

bool
yt_projectile_sector_mines_overlay(struct yt_sector *sector,
    float carried_mines)
{
	if (sector == NULL)
		return false;
	sector->mines = (sector->mines + carried_mines);
	return yt_record_set_number(&sector->record, YT_F129, sector->mines);
}
