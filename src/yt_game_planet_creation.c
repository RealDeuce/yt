#include "yt_game.h"

#include "qb.h"

#include <string.h>

bool
yt_planet_creation_credit_row(double credits, uint8_t *row,
    size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "You have";
	static const uint8_t suffix[] = " credits.";
	char number[64];
	int number_length;
	size_t needed;

	if (length == NULL)
		return false;
	*length = 0U;
	number_length = qb_str_double(number, sizeof(number), credits);
	if (number_length < 0)
		return false;
	needed = sizeof(prefix) - 1U + (size_t)number_length
	    + sizeof(suffix) - 1U;
	if (needed > capacity || (needed != 0U && row == NULL))
		return false;
	memcpy(row, prefix, sizeof(prefix) - 1U);
	memcpy(row + sizeof(prefix) - 1U, number, (size_t)number_length);
	memcpy(row + sizeof(prefix) - 1U + (size_t)number_length,
	    suffix, sizeof(suffix) - 1U);
	*length = needed;
	return true;
}

void
yt_planet_creation_overlay(struct yt_planet *planet,
    int current_player_record)
{
	static const uint8_t dirty_zero[4] = {0x00, 0x00, 0x20, 0x00};
	size_t index;

	if (planet == NULL)
		return;
	for (index = 0U; index < 3U; ++index) {
		planet->production[index] = 1.0f;
		planet->stock[index] = 10.0f;
		(void)yt_record_set_number(&planet->record,
		    YT_F45 + index * 4U, 1.0f);
		(void)yt_record_set_number(&planet->record,
		    YT_F57 + index * 4U, 10.0f);
	}
	planet->mines = 0.0f;
	planet->missiles = 0.0f;
	planet->owner = current_player_record;
	planet->ground_forces = 1.0f;
	planet->plasma = 0.0f;
	planet->bank = 0.0f;
	planet->fighters = 30.0f;
	(void)yt_record_set_raw_number(&planet->record, YT_F125, dirty_zero);
	(void)yt_record_set_raw_number(&planet->record, YT_F69, dirty_zero);
	(void)yt_record_set_number(&planet->record, YT_F73,
	    (float)planet->owner);
	(void)yt_record_set_number(&planet->record, YT_F77, 1.0f);
	(void)yt_record_set_number(&planet->record, YT_F113, 0.0f);
	(void)yt_record_set_number(&planet->record, YT_F117, 0.0f);
	(void)yt_record_set_number(&planet->record, YT_F129, 30.0f);
}

void
yt_planet_creation_timestamp_overlay(struct yt_planet *planet,
    float day, float minute)
{
	if (planet == NULL)
		return;
	planet->last_day = day;
	planet->last_minute = minute;
	(void)yt_record_set_number(&planet->record, YT_F41, day);
	(void)yt_record_set_number(&planet->record, YT_F89, minute);
}

bool
yt_planet_creation_news(const uint8_t *trader_name,
    size_t trader_name_length, const uint8_t *planet_name,
    size_t planet_name_length, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t prefix[] = "  -  ";
	static const uint8_t middle[] = " made a planet: ";
	size_t needed;
	size_t cursor = 0U;

	if (length == NULL || (trader_name == NULL && trader_name_length != 0U)
	    || (planet_name == NULL && planet_name_length != 0U))
		return false;
	*length = 0U;
	needed = sizeof(prefix) - 1U + trader_name_length
	    + sizeof(middle) - 1U + planet_name_length;
	if (needed > capacity || (needed != 0U && row == NULL))
		return false;
	memcpy(row + cursor, prefix, sizeof(prefix) - 1U);
	cursor += sizeof(prefix) - 1U;
	if (trader_name_length != 0U) {
		memcpy(row + cursor, trader_name, trader_name_length);
		cursor += trader_name_length;
	}
	memcpy(row + cursor, middle, sizeof(middle) - 1U);
	cursor += sizeof(middle) - 1U;
	if (planet_name_length != 0U)
		memcpy(row + cursor, planet_name, planet_name_length);
	*length = needed;
	return true;
}

bool
yt_planet_creation_success_row(const uint8_t *planet_name,
    size_t planet_name_length, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t prefix[] = "Planet \"";
	static const uint8_t suffix[] =
	    "\" created with Genesis Device!";
	size_t needed;

	if (length == NULL || (planet_name == NULL && planet_name_length != 0U))
		return false;
	*length = 0U;
	needed = sizeof(prefix) - 1U + planet_name_length
	    + sizeof(suffix) - 1U;
	if (needed > capacity || (needed != 0U && row == NULL))
		return false;
	memcpy(row, prefix, sizeof(prefix) - 1U);
	if (planet_name_length != 0U)
		memcpy(row + sizeof(prefix) - 1U, planet_name,
		    planet_name_length);
	memcpy(row + sizeof(prefix) - 1U + planet_name_length, suffix,
	    sizeof(suffix) - 1U);
	*length = needed;
	return true;
}
