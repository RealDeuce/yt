#include "yt_game.h"

#include "qb.h"

#include <math.h>
#include <string.h>

bool
yt_planet_garrison_prompt(float player_forces, float planet_forces,
    uint8_t *prompt, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] =
	    "Drop how many ground force units on the planet?";
	static const uint8_t suffix[] = " Available ->";
	volatile float available = player_forces + planet_forces;
	char number[64];
	int number_length;
	size_t needed;

	if (length == NULL)
		return false;
	*length = 0U;
	number_length = qb_str_single(number, sizeof(number), available);
	if (number_length < 0)
		return false;
	needed = sizeof(prefix) - 1U + (size_t)number_length
	    + sizeof(suffix) - 1U;
	if (needed > capacity || (needed != 0U && prompt == NULL))
		return false;
	memcpy(prompt, prefix, sizeof(prefix) - 1U);
	memcpy(prompt + sizeof(prefix) - 1U, number, (size_t)number_length);
	memcpy(prompt + sizeof(prefix) - 1U + (size_t)number_length,
	    suffix, sizeof(suffix) - 1U);
	*length = needed;
	return true;
}

float
yt_planet_garrison_after(float player_forces, float desired,
    float planet_forces)
{
	volatile float subtracted = player_forces - desired;
	volatile float result = subtracted + planet_forces;

	return result;
}

void
yt_planet_garrison_overlay(struct yt_planet *planet, float desired,
    int player_record)
{
	static const uint8_t dirty_zero[4] = {0x00, 0x00, 0x40, 0x00};

	if (planet == NULL)
		return;
	planet->ground_forces = desired;
	(void)yt_record_set_number(&planet->record, YT_F77, desired);
	(void)yt_record_set_raw_number(&planet->record, YT_F73, dirty_zero);
	planet->owner = 0.0f;
	if (desired >= 1.0f && player_record != 0) {
		planet->owner = (float)player_record;
		(void)yt_record_set_number(&planet->record, YT_F73,
		    planet->owner);
	}
}

void
yt_planet_garrison_player_overlay(struct yt_player *player, float remaining)
{
	volatile float integral = floorf(remaining);

	if (player == NULL)
		return;
	(void)yt_record_set_number(&player->record, YT_F121, integral);
}

bool
yt_planet_garrison_success_row(float desired, uint8_t *row,
    size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "Ground force strength now at";
	static const uint8_t suffix[] = " units!";
	char number[64];
	int number_length;
	size_t needed;

	if (length == NULL)
		return false;
	*length = 0U;
	number_length = qb_str_single(number, sizeof(number), desired);
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
